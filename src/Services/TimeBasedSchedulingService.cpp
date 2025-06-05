#include "ECSS_Configuration.hpp"
#ifdef SERVICE_TIMESCHEDULING

#include "TimeBasedSchedulingService.hpp"
#include "MemoryManager.hpp"

constexpr uint16_t MAX_ENTRY_SIZE = CCSDSMaxMessageSize //	Max Message size
									+ 6					//  RequestID size
									+ 7;				//  UTC Timestamp size

void TimeBasedSchedulingService::storeScheduleTCList(const etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities>& activityList) {
	if (scheduledActivities.empty()) {
		// Empty list
		LOG_DEBUG<<"[SCHED TC] Attempted to store empty list";
		return;
	}


	for (auto& entry : scheduledActivities) {
		// Serialize entry to uint8_t buffer, to store it in memory
		etl::array<uint8_t, MAX_ENTRY_SIZE> entryBuffer = {0};
		uint16_t entryIndex = 0;

		// Append Request to buffer
		auto requestString = MessageParser::compose(entry.request, entry.request.data_size_message_);
		memcpy(entryBuffer.data(), requestString.value().data(), requestString.value().size());
		entryIndex = CCSDSMaxMessageSize; // Move iterator to end of available message space, to help with parsing

		// Append requestID to buffer
		entryBuffer[entryIndex++] = static_cast<uint8_t>(entry.requestID.applicationID >> 8);	// MSB
		entryBuffer[entryIndex++] = static_cast<uint8_t>(entry.requestID.applicationID & 0xFF); // LSB

		entryBuffer[entryIndex++] = static_cast<uint8_t>(entry.requestID.sequenceCount >> 8);	// MSB
		entryBuffer[entryIndex++] = static_cast<uint8_t>(entry.requestID.sequenceCount & 0xFF); // LSB

		entryBuffer[entryIndex++] = static_cast<uint8_t>(entry.requestID.sourceID >> 8);	// MSB
		entryBuffer[entryIndex++] = static_cast<uint8_t>(entry.requestID.sourceID & 0xFF);	// LSB

		// Append requestReleaseTime to buffer
		entryBuffer[entryIndex++] = static_cast<uint8_t>(entry.requestReleaseTime.year >> 8);	// MSB
		entryBuffer[entryIndex++] = static_cast<uint8_t>(entry.requestReleaseTime.year & 0xFF);	// LSB

		entryBuffer[entryIndex++] = entry.requestReleaseTime.month;
		entryBuffer[entryIndex++] = entry.requestReleaseTime.day;
		entryBuffer[entryIndex++] = entry.requestReleaseTime.hour;
		entryBuffer[entryIndex++] = entry.requestReleaseTime.minute;
		entryBuffer[entryIndex++] = entry.requestReleaseTime.second;
	}


}

void TimeBasedSchedulingService::recoverScheduleTCList(etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities>& activityList) {

}


TimeBasedSchedulingService::TimeBasedSchedulingService() {
	serviceType = TimeBasedSchedulingService::ServiceType;
}

UTCTimestamp TimeBasedSchedulingService::executeScheduledActivity(UTCTimestamp currentTime) {
	if (currentTime >= scheduledActivities.front().requestReleaseTime && !scheduledActivities.empty()) {
		if (scheduledActivities.front().requestID.applicationID == ApplicationId) {
			MessageParser::execute(scheduledActivities.front().request);
		}
		scheduledActivities.pop_front();
	}

	if (!scheduledActivities.empty()) {
		return scheduledActivities.front().requestReleaseTime;
	}
	return {9999, 12, 31, 23, 59, 59};
}

void TimeBasedSchedulingService::enableScheduleExecution(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::EnableTimeBasedScheduleExecutionFunction)) {
		return;
	}
	executionFunctionStatus = true;
}

void TimeBasedSchedulingService::disableScheduleExecution(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DisableTimeBasedScheduleExecutionFunction)) {
		return;
	}
	executionFunctionStatus = false;
}

void TimeBasedSchedulingService::resetSchedule(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::ResetTimeBasedSchedule)) {
		return;
	}
	executionFunctionStatus = false;
	scheduledActivities.clear();
	// todo (#264): Add resetting for sub-schedules and groups, if defined
}


void TimeBasedSchedulingService::insertActivities(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::InsertActivities)) {
		return;
	}

	// todo (#228): Get the sub-schedule ID if they are implemented
	uint16_t iterationCount = request.readUint16();
	while (iterationCount-- != 0) {
		// todo (#229): Get the group ID first, if groups are used
		const UTCTimestamp currentTime(TimeGetter::getCurrentTimeUTC());

		const UTCTimestamp releaseTime(request.readUTCTimestamp());
		if ((scheduledActivities.available() == 0) || (releaseTime < (currentTime + ECSSTimeMarginForActivation))) {
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
			request.skipBytes(ECSSTCRequestStringSize);
		} else {
			etl::array<uint8_t, ECSSTCRequestStringSize> requestData = {0};
			request.readString(requestData.data(), ECSSTCRequestStringSize);
			Message receivedTCPacket;
			const auto res = MessageParser::parseECSSTC(requestData.data(), receivedTCPacket);
			ScheduledActivity newActivity;

			newActivity.request = receivedTCPacket;
			newActivity.requestReleaseTime = releaseTime;

			newActivity.requestID.sourceID = request.source_ID_;
			newActivity.requestID.applicationID = request.application_ID_;
			newActivity.requestID.sequenceCount = request.packet_sequence_count_;

			scheduledActivities.push_back(newActivity);
		}
	}
	sortActivitiesReleaseTime(scheduledActivities);
	notifyNewActivityAddition();
}

void TimeBasedSchedulingService::timeShiftAllActivities(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::TimeShiftALlScheduledActivities)) {
		return;
	}

	const UTCTimestamp currentTime(TimeGetter::getCurrentTimeUTC());

	const auto releaseTimes =
	    etl::minmax_element(scheduledActivities.begin(), scheduledActivities.end(),
	                        [](ScheduledActivity const& leftSide, ScheduledActivity const& rightSide) {
		                        return leftSide.requestReleaseTime < rightSide.requestReleaseTime;
	                        });
	// todo (#267): Define what the time format is going to be
	const Time::RelativeTime relativeOffset = request.readRelativeTime();
	if ((releaseTimes.first->requestReleaseTime + std::chrono::seconds(relativeOffset)) < (currentTime + ECSSTimeMarginForActivation)) {
		ErrorHandler::reportError(request, ErrorHandler::SubServiceExecutionStartError);
		return;
	}
	for (auto& activity: scheduledActivities) {
		activity.requestReleaseTime += std::chrono::seconds(relativeOffset);
	}
}

void TimeBasedSchedulingService::timeShiftActivitiesByID(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::TimeShiftActivitiesById)) {
		return;
	}

	const UTCTimestamp currentTime(TimeGetter::getCurrentTimeUTC());

	auto relativeOffset = std::chrono::seconds(request.readRelativeTime());
	uint16_t iterationCount = request.readUint16();
	while (iterationCount-- != 0) {
		RequestID receivedRequestID;
		receivedRequestID.sourceID = request.read<SourceId>();
		receivedRequestID.applicationID = request.read<ApplicationProcessId>();
		receivedRequestID.sequenceCount = request.read<SequenceCount>();

		auto requestIDMatch = etl::find_if_not(scheduledActivities.begin(), scheduledActivities.end(),
		                                       [&receivedRequestID](ScheduledActivity const& currentElement) {
			                                       return receivedRequestID != currentElement.requestID;
		                                       });

		if (requestIDMatch != scheduledActivities.end()) {
			if ((requestIDMatch->requestReleaseTime + relativeOffset) <
			    (currentTime + ECSSTimeMarginForActivation)) {
				ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
			} else {
				requestIDMatch->requestReleaseTime += relativeOffset;
			}
		} else {
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
		}
	}
	sortActivitiesReleaseTime(scheduledActivities);
}

void TimeBasedSchedulingService::deleteActivitiesByID(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DeleteActivitiesById)) {
		return;
	}

	uint16_t iterationCount = request.readUint16();
	while (iterationCount-- != 0) {
		RequestID receivedRequestID;
		receivedRequestID.sourceID = request.read<SourceId>();
		receivedRequestID.applicationID = request.read<ApplicationProcessId>();
		receivedRequestID.sequenceCount = request.read<SequenceCount>();

		const auto requestIDMatch = etl::find_if_not(scheduledActivities.begin(), scheduledActivities.end(),
		                                             [&receivedRequestID](ScheduledActivity const& currentElement) {
			                                             return receivedRequestID != currentElement.requestID;
		                                             });

		if (requestIDMatch != scheduledActivities.end()) {
			scheduledActivities.erase(requestIDMatch);
		} else {
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
		}
	}
}

void TimeBasedSchedulingService::detailReportAllActivities(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DetailReportAllScheduledActivities)) {
		return;
	}

	timeBasedScheduleDetailReport(scheduledActivities);
}

void TimeBasedSchedulingService::timeBasedScheduleDetailReport(etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities>& listOfActivities) {
	// todo (#228): (#229) append sub-schedule and group ID if they are defined
	Message report = createTM(TimeBasedSchedulingService::MessageType::TimeBasedScheduleReportById);
	report.appendUint16(static_cast<uint16_t>(listOfActivities.size()));

	for (auto& activity: listOfActivities) {
		report.appendUTCTimestamp(activity.requestReleaseTime); // todo (#267): Replace with the time parser
		auto result = MessageParser::composeECSS(activity.request, activity.request.total_size_ecss_);
		if (result.has_value()) {
			report.appendString(result.value());
		}
		// Note: If composition fails, the activity is skipped in the report
	}
	storeMessage(report, report.data_size_message_);
}

void TimeBasedSchedulingService::detailReportActivitiesByID(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DetailReportActivitiesById)) {
		return;
	}

	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> matchedActivities;

	uint16_t iterationCount = request.readUint16();
	while (iterationCount-- != 0) {
		RequestID receivedRequestID;
		receivedRequestID.sourceID = request.read<SourceId>();
		receivedRequestID.applicationID = request.read<ApplicationProcessId>();
		receivedRequestID.sequenceCount = request.read<SequenceCount>();

		const auto requestIDMatch = etl::find_if_not(scheduledActivities.begin(), scheduledActivities.end(),
		                                             [&receivedRequestID](ScheduledActivity const& currentElement) {
			                                             return receivedRequestID != currentElement.requestID;
		                                             });

		if (requestIDMatch != scheduledActivities.end()) {
			matchedActivities.push_back(*requestIDMatch);
		} else {
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
		}
	}

	sortActivitiesReleaseTime(matchedActivities);

	timeBasedScheduleDetailReport(matchedActivities);
}

void TimeBasedSchedulingService::summaryReportActivitiesByID(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::ActivitiesSummaryReportById)) {
		return;
	}

	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> matchedActivities;

	uint16_t iterationCount = request.readUint16();
	while (iterationCount-- != 0) {
		RequestID receivedRequestID;
		receivedRequestID.sourceID = request.read<SourceId>();
		receivedRequestID.applicationID = request.read<ApplicationProcessId>();
		receivedRequestID.sequenceCount = request.read<SequenceCount>();

		auto requestIDMatch = etl::find_if_not(scheduledActivities.begin(), scheduledActivities.end(),
		                                       [&receivedRequestID](ScheduledActivity const& currentElement) {
			                                       return receivedRequestID != currentElement.requestID;
		                                       });

		if (requestIDMatch != scheduledActivities.end()) {
			matchedActivities.push_back(*requestIDMatch);
		} else {
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
		}
	}
	sortActivitiesReleaseTime(matchedActivities);

	timeBasedScheduleSummaryReport(matchedActivities);
}

void TimeBasedSchedulingService::timeBasedScheduleSummaryReport(const etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities>& listOfActivities) {
	Message report = createTM(TimeBasedSchedulingService::MessageType::TimeBasedScheduledSummaryReport);

	// todo (#228): append sub-schedule and group ID if they are defined
	report.appendUint16(static_cast<uint16_t>(listOfActivities.size()));
	for (const auto& match: listOfActivities) {
		// todo (#229): append sub-schedule and group ID if they are defined
		report.appendUTCTimestamp(match.requestReleaseTime);
		report.append<SourceId>(match.requestID.sourceID);
		report.append<ApplicationProcessId>(match.requestID.applicationID);
		report.append<SequenceCount>(match.requestID.sequenceCount);
	}
	storeMessage(report, report.data_size_message_);
}

void TimeBasedSchedulingService::execute(Message& message) {
	switch (message.messageType) {
		case EnableTimeBasedScheduleExecutionFunction:
			enableScheduleExecution(message);
			break;
		case DisableTimeBasedScheduleExecutionFunction:
			disableScheduleExecution(message);
			break;
		case ResetTimeBasedSchedule:
			resetSchedule(message);
			break;
		case InsertActivities:
			// insertActivities(message);
			break;
		case DeleteActivitiesById:
			deleteActivitiesByID(message);
			break;
		case TimeShiftActivitiesById:
			timeShiftActivitiesByID(message);
			break;
		case DetailReportActivitiesById:
			detailReportActivitiesByID(message);
			break;
		case ActivitiesSummaryReportById:
			summaryReportActivitiesByID(message);
			break;
		case TimeShiftALlScheduledActivities:
			timeShiftAllActivities(message);
			break;
		case DetailReportAllScheduledActivities:
			detailReportAllActivities(message);
			break;
		default:
			ErrorHandler::reportError(message, ErrorHandler::OtherMessageType);
			break;
	}
}

#endif
