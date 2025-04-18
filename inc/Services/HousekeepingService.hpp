#ifndef ECSS_SERVICES_HOUSEKEEPINGSERVICE_HPP
#define ECSS_SERVICES_HOUSEKEEPINGSERVICE_HPP

#include <optional>
#include "ECSS_Definitions.hpp"
#include "ErrorHandler.hpp"
#include "HousekeepingStructure.hpp"
#include "Service.hpp"
#include "etl/map.h"

/**
 * Implementation of the ST[03] Housekeeping Reporting Service. The job of the Housekeeping Service is to store
 * parameters in the housekeeping structures so that it can generate housekeeping reports periodically.
 *
 * @ingroup Services
 * @author Petridis Konstantinos <petridkon@gmail.com>
 */
class HousekeepingService : Service {
private:
	/**
	 * Appends the periodic properties of a housekeeping structure to a message.
	 *
	 * @note The structureId is checked before being passed in this function, so there is a convention that the ID is
	 * valid. If this function needs to be called from another point of the code, the case of an invalid ID passed as
	 * argument will lead in undefined behavior.
	 */
	void appendPeriodicPropertiesToMessage(Message& report, ParameterReportStructureId structureId);

	/**
	 * Returns true if the given parameter ID exists in the parameters contained in the housekeeping structure.
	 */
	static bool existsInVector(const etl::vector<uint16_t, ECSSMaxSimplyCommutatedParameters>& ids,
	                           ParameterId parameterId);

	/**
     * Initializes Housekeeping Structures with the Parameters found in the obc-software.
     * The function definition is also found in the obc-software repo.
     */
	void initializeHousekeepingStructures();

public:
	inline static constexpr ServiceTypeNum ServiceType = 3;

	/**
	 * Map containing the housekeeping structures. Map[i] contains the housekeeping structure with ID = i.
	 */
	etl::map<ParameterReportStructureId, HousekeepingStructure, ECSSMaxHousekeepingStructures> housekeepingStructures;

	enum MessageType : uint8_t {
		CreateHousekeepingReportStructure = 1,
		DeleteHousekeepingReportStructure = 3,
		EnablePeriodicHousekeepingParametersReport = 5,
		DisablePeriodicHousekeepingParametersReport = 6,
		ReportHousekeepingStructures = 9,
		HousekeepingStructuresReport = 10,
		HousekeepingParametersReport = 25,
		GenerateOneShotHousekeepingReport = 27,
		AppendParametersToHousekeepingStructure = 29,
		ModifyCollectionIntervalOfStructures = 31,
		ReportHousekeepingPeriodicProperties = 33,
		HousekeepingPeriodicPropertiesReport = 35,
	};

	HousekeepingService() {
		serviceType = ServiceType;
		initializeHousekeepingStructures();
	};

	/**
	 * Returns the periodic generation action status of a Housekeeping structure.
	 * @param id Housekeeping structure ID
	 * @return boolean True if periodic generation of housekeeping reports is enabled, false otherwise
	 */
	inline bool getPeriodicGenerationActionStatus(ParameterReportStructureId id) {
		const HousekeepingStructure newStructure{};
		if (hasNonExistingStructInternalError(id)) {
			return newStructure.periodicGenerationActionStatus;
		}
		return housekeepingStructures.at(id).periodicGenerationActionStatus;
	}

	/**
	 * Returns a reference to the structure at position of "id" in the map.
	 * @param id Housekeeping structure ID
	 * @return optional<std::reference_wrapper<HousekeepingStructure>> Reference to Housekeeping Structure
	 */
	inline std::optional<std::reference_wrapper<HousekeepingStructure>> getStruct(ParameterReportStructureId id) {
		if (hasNonExistingStructInternalError(id)) {
			return {};
		}
		return housekeepingStructures.at(id);
	}

	/**
	 * Returns the collection interval (how often data is collected) of a Housekeeping structure.
	 * @param id Housekeeping structure ID
	 * @return uint32_t Integer multiples of the minimum sampling interval
	 */
	inline CollectionInterval getCollectionInterval(ParameterReportStructureId id) {
		if (hasNonExistingStructInternalError(id)) {
			const HousekeepingStructure newStructure{};
			return newStructure.collectionInterval;
		}
		return housekeepingStructures.at(id).collectionInterval;
	}

	/**
	 * Sets the periodic generation action status of a Housekeeping structure.
	 * @param id Housekeeping structure ID
	 * @param status Periodic generation status of housekeeping reports
	 */
	inline void setPeriodicGenerationActionStatus(ParameterReportStructureId id, bool status) {
		if (hasNonExistingStructInternalError(id)) {
			return;
		}
		housekeepingStructures.at(id).periodicGenerationActionStatus = status;
	}

	/**
	 * Sets the collection interval of a Housekeeping structure.
	 * @param id Housekeeping structure ID
	 * @param interval Integer multiples of the minimum sampling interval
	 */
	inline void setCollectionInterval(ParameterReportStructureId id, CollectionInterval interval) {
		if (hasNonExistingStructInternalError(id)) {
			return;
		}
		housekeepingStructures.at(id).collectionInterval = interval;
	}

	/**
	 * Checks if the structure exists in the map.
	 * @param id Housekeeping structure ID
	 * @return boolean True if the structure exists, false otherwise
	 */
	inline bool structExists(ParameterReportStructureId id) {
		return (housekeepingStructures.find(id) != housekeepingStructures.end());
	}

	/**
	 * Checks if the structure doesn't exist in the map and then accordingly reports execution start error.
	 * @param id Housekeeping structure ID
	 * @param request Telemetry (TM) or telecommand (TC) message
	 * @return boolean True if the structure doesn't exist, false otherwise
	 */
	bool hasNonExistingStructExecutionError(ParameterReportStructureId id, const Message& request);

	/**
	 * Checks if the structure doesn't exist in the map and then accordingly reports error.
	 * @param id Housekeeping structure ID
	 * @param request Telemetry (TM) or telecommand (TC) message
	 * @return boolean True if the structure doesn't exist, false otherwise
	 */
	bool hasNonExistingStructError(ParameterReportStructureId id, const Message& request);

	/**
	 * Checks if the structure doesn't exist in the map and then accordingly reports internal error.
	 * @param id Housekeeping structure ID
	 * @return boolean True if the structure doesn't exist, false otherwise
	 */
	bool hasNonExistingStructInternalError(ParameterReportStructureId id);

	/**
	 * Checks if the parameter exists in the vector and if it does it reports an error.
	 * @param id Parameter ID
	 * @param housekeepingStruct Housekkeping Structure
	 * @param request Telemetry (TM) or telecommand (TC) message
	 * @return boolean True if the parameter exists, false otherwise
	 */
	static bool hasAlreadyExistingParameterError(const HousekeepingStructure& housekeepingStruct, ParameterReportStructureId id, const Message& request);

	/**
	 * Checks if the struct requested exists and if it exists reports execution error.
	 * @param id Housekeeping structure ID
	 * @param request Telemetry (TM) or telecommand (TC) message
	 * @return boolean True if the structure exists, false otherwise
	 */
	bool hasAlreadyExistingStructError(ParameterReportStructureId id, const Message& request);

	/**
	 * Reports execution error if the max number of housekeeping structures is exceeded.
	 * @param request Telemetry (TM) or telecommand (TC) message
	 * @return boolean True if max number of housekeeping structures is exceeded, false otherwise
	 */
	bool hasExceededMaxNumOfHousekeepingStructsError(const Message& request);

	/**
	 * Reports execution error if it's attempted to append a new parameter id to a housekeeping structure, but the periodic generation status is enabled.
	 * @param housekeepingStruct Housekkeping Structure
	 * @param request Telemetry (TM) or telecommand (TC) message
	 * @return boolean True if periodic generation status is enabled, false otherwise
	 */
	static bool hasRequestedAppendToEnabledHousekeepingError(const HousekeepingStructure& housekeepingStruct, const Message& request);

	/**
	 * Reports execution error if it's attempted to delete structure which has the periodic reporting status enabled.
	 * @param id Housekeeping structure ID
	 * @param request Telemetry (TM) or telecommand (TC) message
	 * @return boolean True if periodic reporting status is enabled, false otherwise
	 */
	bool hasRequestedDeletionOfEnabledHousekeepingError(ParameterReportStructureId id, const Message& request);

	/**
	 * Reports execution error if the max number of simply commutated parameters is exceeded.
	 * @param housekeepingStruct Housekkeping Structure
	 * @param request Telemetry (TM) or telecommand (TC) message
	 * @return boolean True if max number of simply commutated parameters is exceeded, false otherwise
	 */
	static bool hasExceededMaxNumOfSimplyCommutatedParamsError(const HousekeepingStructure& housekeepingStruct, const Message& request);

	/**
	 * Implementation of TC[3,1]. Request to create a housekeeping parameters report structure.
	 */
	void createHousekeepingReportStructure(Message& request);

	/**
	 * Implementation of TC[3,3]. Request to delete a housekeeping parameters report structure.
	 */
	void deleteHousekeepingReportStructure(Message& request);

	/**
	 * Implementation of TC[3,5]. Request to enable the periodic housekeeping parameters reporting for a specific
	 * housekeeping structure.
	 */
	void enablePeriodicHousekeepingParametersReport(Message& request);

	/**
	 * Implementation of TC[3,6]. Request to disable the periodic housekeeping parameters reporting for a specific
	 * housekeeping structure.
	 */
	void disablePeriodicHousekeepingParametersReport(Message& request);

	/**
	 * This function gets a message type TC[3,9] 'report housekeeping structures'.
	 */
	void reportHousekeepingStructures(Message& request);

	/**
	 * This function takes a structure ID as argument and constructs/stores a TM[3,10] housekeeping structure report.
	 */
	void housekeepingStructureReport(ParameterReportStructureId structIdToReport);

	/**
	 * This function gets a housekeeping structure ID and stores a TM[3,25] 'housekeeping
	 * parameter report' message.
	 */
	void housekeepingParametersReport(ParameterReportStructureId structureId);

	/**
	 * This function takes as argument a message type TC[3,27] 'generate one shot housekeeping report' and stores
	 * TM[3,25] report messages.
	 */
	void generateOneShotHousekeepingReport(Message& request);

	/**
	 * This function receives a message type TC[3,29] 'append new parameters to an already existing housekeeping
	 * structure'
	 *
	 * @note As per 6.3.3.8.d.4, in case of an invalid parameter, the whole message shall be rejected. However, a
	 * convention was made, saying that it would be more practical to just skip the invalid parameter and continue
	 * processing the rest of the message.
	 */
	void appendParametersToHousekeepingStructure(Message& request);

	/**
	 * This function receives a message type TC[3,31] 'modify the collection interval of specified structures'.
	 */
	void modifyCollectionIntervalOfStructures(Message& request);

	/**
	 * This function takes as argument a message type TC[3,33] 'report housekeeping periodic properties' and
	 * responds with a TM[3,35] 'housekeeping periodic properties report'.
	 */
	void reportHousekeepingPeriodicProperties(Message& request);

	/**
	 * This function calculates the time needed to pass until the next periodic report for each housekeeping 
	 * structure. The function also calls the housekeeping reporting functions as needed.
	 * 
	 * @note Three arguments are needed for resiliency in case the function doesn't execute at the exact time that is expected
	 * 
	 * @param currentTime The current system time, in milliseconds.
	 * @param previousTime The system time of the previous call of the function.
	 * @param expectedDelay The output of this function after its last execution.
	 * @return uint32_t The minimum amount of time until the next periodic housekeeping report, in milliseconds.
	 */
	UTCTimestamp reportPendingStructures(UTCTimestamp currentTime, UTCTimestamp previousTime, UTCTimestamp expectedDelay);

	/**
	 * It is responsible to call the suitable function that executes a TC packet. The source of that packet
	 * is the ground station.
	 *
	 * @note This function is called from the main execute() that is defined in the file MessageParser.hpp
	 * @param message Contains the necessary parameters to call the suitable subservice
	 */
	void execute(Message& message);
};

#endif
