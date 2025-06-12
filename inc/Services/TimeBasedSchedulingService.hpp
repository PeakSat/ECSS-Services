#ifndef ECSS_SERVICES_TIMEBASEDSCHEDULINGSERVICE_HPP
#define ECSS_SERVICES_TIMEBASEDSCHEDULINGSERVICE_HPP

#include "CRCHelper.hpp"
#include "ErrorHandler.hpp"
#include "MessageParser.hpp"
#include "Service.hpp"
#include "etl/list.h"

// Include platform specific files
#include "TimeGetter.hpp"


/**
 * @def GROUPS_ENABLED
 * @brief Indicates whether scheduling groups are enabled
 */
#define GROUPS_ENABLED 0 // NOLINT(cppcoreguidelines-macro-usage)

/**
 * @def SUB_SCHEDULES_ENABLED
 * @brief Indicates whether sub-schedules are supported
 *
 * @details Sub-schedules are currently not implemented so this has no effect
 */
#define SUB_SCHEDULES_ENABLED 0 // NOLINT(cppcoreguidelines-macro-usage)

/**
 * @brief Namespace to access private members during test
 *
 * @details Define a namespace for the access of the private members to avoid conflicts
 */
namespace unit_test {
	struct Tester;
} // namespace unit_test

/**
 * @brief An implementation of the ECSS standard ST[11] service
 *
 * @details This service is taking care of the timed release of a received TC packet from the
 * ground.
 * @todo (#227) Define whether the parsed absolute release time is saved in the scheduled activity as an
 * uint32_t or in the time format specified by the time management service.
 *
 * @ingroup Services
 */
class TimeBasedSchedulingService : public Service {
private:
	/**
	 * @brief Indicator of the schedule execution
	 * True indicates "enabled" and False "disabled" state
	 * @details The schedule execution indicator will be updated by the process that is running
	 * the time scheduling service.
	 */
	bool executionFunctionStatus = false;

	static constexpr int16_t MAX_ENTRY_SIZE = CCSDSMaxMessageSize //	Max Message size
									+ 6					//  RequestID size
									+ 7					//  UTC Timestamp size
									+ 87;				//  Padding to match exactly 9 MRAM blocks

	static constexpr uint16_t MRAM_BLOCKS_PER_ACTIVITY = MAX_ENTRY_SIZE / (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE-1);
	static_assert(MRAM_BLOCKS_PER_ACTIVITY%(MemoryFilesystem::MRAM_DATA_BLOCK_SIZE-1) != 0, "Should be a multiple of 127" );

	enum class Activity_State: uint8_t {
		invalid = 0,
		waiting = 1,
	};

	typedef struct {
		uint8_t id = 0;  // Id in the MRAM storage area
		UTCTimestamp timestamp;
		Activity_State state = Activity_State::invalid;
	}ActivityEntry;

	static constexpr uint32_t activitiesEntriesArraySize = 9 * ECSSMaxNumberOfTimeSchedActivities;
	static constexpr uint32_t MRAM_BLOCKS_OFFSET_ACTIVITIES_LIST = 2;

	/**
	 * @brief Request identifier of the received packet
	 *
	 * @details The request identifier consists of the application process ID, the packet
	 * sequence count and the source ID, all defined in the ECSS standard.
	 */
	struct RequestID {
		ApplicationProcessId applicationID = 0; ///< Application process ID
		SequenceCount sequenceCount = 0;        ///< Packet sequence count
		SourceId sourceID = 0;                  ///< Packet source ID

		bool operator!=(const RequestID& rightSide) const {
			return (sequenceCount != rightSide.sequenceCount)
					or (applicationID != rightSide.applicationID)
					or (sourceID != rightSide.sourceID);
		}
	};

	/**
	 * @brief Instances of activities to run in the schedule
	 *
	 * @details All scheduled activities must contain the request they exist for, their release
	 * time and the corresponding request identifier.
	 *
	 * @todo (#228) If we decide to use sub-schedules, the ID of that has to be defined
	 * @todo (#229) If groups are used, then the group ID has to be defined here
	 */
	struct ScheduledActivity {
		Message request;                   ///< Hold the received command request
		RequestID requestID;               ///< Request ID, characteristic of the definition
		UTCTimestamp requestReleaseTime{}; ///< Keep the command release time
	};


	static bool isEarlier(const UTCTimestamp& a, const UTCTimestamp& b) {
		if (a.year != b.year) return a.year < b.year;
		if (a.month != b.month) return a.month < b.month;
		if (a.day != b.day) return a.day < b.day;
		if (a.hour != b.hour) return a.hour < b.hour;
		if (a.minute != b.minute) return a.minute < b.minute;
		return a.second < b.second;
	}

	static bool isValidScheduled(const ActivityEntry& entry) {
		return entry.state == Activity_State::waiting;
	}

	/**
	 * @brief Sort the activities by their release time
	 *
	 * @details The ECSS standard requires that the activities are sorted in the TM message
	 * response. Also it is better to have the activities sorted.
	 */
	static void sortActivityEntries(ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities]) {
		// Step 1: Partition valid (waiting) vs others
		int validCount = 0;

		for (int i = 0; i < ECSSMaxNumberOfTimeSchedActivities; ++i) {
			if (isValidScheduled(entries[i])) {
				if (i != validCount) {
					std::swap(entries[i], entries[validCount]);
				}
				++validCount;
			}
		}

		// Step 2: Sort only the valid part by timestamp
		for (int i = 0; i < validCount - 1; ++i) {
			for (int j = i + 1; j < validCount; ++j) {
				if (isEarlier(entries[j].timestamp, entries[i].timestamp)) {
					std::swap(entries[i], entries[j]);
				}
			}
		}
	}

	/**
	 * @brief Define a friend in order to be able to access private members during testing
	 *
	 * @details The private members defined in this class, must not in any way be public to avoid
	 * misuse. During testing, access to private members for verification is required, so an
	 * access friend structure is defined here.
	 */
	friend struct ::unit_test::Tester;

	/**
     * Notifies the timeBasedSchedulingTask after the insertion of activities to scheduleActivity list.
     */
	void notifyNewActivityAddition();

	void initEsotericVariables();

public:
	inline static constexpr ServiceTypeNum ServiceType = 11;

	enum MessageType : uint8_t {
		EnableTimeBasedScheduleExecutionFunction = 1,
		DisableTimeBasedScheduleExecutionFunction = 2,
		ResetTimeBasedSchedule = 3,
		InsertActivities = 4,
		DeleteActivitiesById = 5,
		TimeShiftActivitiesById = 7,
		DetailReportActivitiesById = 9,
		TimeBasedScheduleReportById = 10,
		ActivitiesSummaryReportById = 12,
		TimeBasedScheduledSummaryReport = 13,
		TimeShiftALlScheduledActivities = 15,
		DetailReportAllScheduledActivities = 16,
	};

	/**
	 * @brief Class constructor
	 * @details Initializes the serviceType
	 */
	TimeBasedSchedulingService();

	/**
	 * This function executes the next activity and removes it from the list.
	 * @return the requestReleaseTime of next activity to be executed after this time
	 */
	UTCTimestamp executeScheduledActivity(UTCTimestamp currentTime);

	/**
	 * @brief TC[11,1] enable the time-based schedule execution function
	 *
	 * @details Enables the time-based command execution scheduling
	 * @param request Provide the received message as a parameter
	 */
	void enableScheduleExecution(const Message& request);

	/**
	 * @brief TC[11,2] disable the time-based schedule execution function
	 *
	 * @details Disables the time-based command execution scheduling
	 * @param request Provide the received message as a parameter
	 */
	void disableScheduleExecution(const Message& request);

	/**
	 * @brief TC[11,3] reset the time-based schedule
	 *
	 * @details Resets the time-based command execution schedule, by clearing all scheduled
	 * activities.
	 * @param request Provide the received message as a parameter
	 */
	void resetSchedule(const Message& request);

	/**
	 * @brief TC[11,4] insert activities into the time based schedule
	 *
	 * @details Add activities into the schedule for future execution. The activities are inserted
	 * by ascending order of their release time. This done to avoid confusion during the
	 * execution of the schedule and also to make things easier whenever a release time sorted
	 * report is requested by he corresponding service.
	 * @param request Provide the received message as a parameter
	 * @todo (#230) Definition of the time format is required
	 * @throws ExecutionStartError If there is request to be inserted and the maximum
	 * number of activities in the current schedule has been reached, then an @ref
	 * ErrorHandler::ExecutionStartErrorType is being issued.  Also if the release time of the
	 * request is less than a set time margin, defined in @ref ECSS_TIME_MARGIN_FOR_ACTIVATION,
	 * from the current time a @ref ErrorHandler::ExecutionStartErrorType is also issued.
	 */
	void insertActivities(Message& request);

	/**
	 * @brief TC[11,15] time-shift all scheduled activities
	 *
	 * @details All scheduled activities are shifted per user request. The relative time offset
	 * received and tested against the current time.
	 * @param request Provide the received message as a parameter
	 * @todo (#231) Definition of the time format is required for the relative time format
	 * @throws ExecutionStartError If the release time of the request is less than a
	 * set time margin, defined in @ref ECSS_TIME_MARGIN_FOR_ACTIVATION, from the current time an
	 * @ref ErrorHandler::ExecutionStartErrorType report is issued for that instruction.
	 */
	void timeShiftAllActivities(Message& request);

	/**
	 * @brief TC[11,16] detail-report all activities
	 *
	 * @details Send a detailed report about the status of all the activities
	 * on the current schedule. Generates a TM[11,10] response.
	 * @param request Provide the received message as a parameter
	 * @todo (#232) Replace the time parsing with the time parser
	 */
	void detailReportAllActivities(const Message& request);

	/**
	 * @brief TM[11,10] time-based schedule detail report
	 *
	 * @details Send a detailed report about the status of the activities listed
	 * on the provided list. Generates a TM[11,10] response.
	 * @param listOfActivities Provide the list of activities that need to be reported on
	 */
	void timeBasedScheduleDetailReport();

	/**
	 * @brief TM[11,13] time-based schedule summary report
	 *
	 * @details Send a summary report about the status of the activities listed
	 * on the provided list. Generates a TM[11,13] response.
	 * @param listOfActivities Provide the list of activities that need to be reported on
	 */
	void timeBasedScheduleSummaryReport();

	/**
	 * It is responsible to call the suitable function that executes a telecommand packet. The source of that packet
	 * is the ground station.
	 *
	 * @note This function is called from the main execute() that is defined in the file MessageParser.hpp
	 * @param message Contains the necessary parameters to call the suitable subservice
	 */
	void execute(Message& message);

	static SpacecraftErrorCode readActivityEntries(ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities]);

	static SpacecraftErrorCode storeActivityEntries(ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities]);

	static SpacecraftErrorCode storeScheduledActivity(ScheduledActivity activity, const uint8_t id);

	static SpacecraftErrorCode recoverScheduledActivity(ScheduledActivity& activity, const uint8_t id);

};

#endif // ECSS_SERVICES_TIMEBASEDSCHEDULINGSERVICE_HPP
