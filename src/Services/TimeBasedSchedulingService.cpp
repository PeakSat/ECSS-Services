#include "ECSS_Configuration.hpp"
#ifdef SERVICE_TIMESCHEDULING

#include "MemoryManager.hpp"
#include "TCHandlingTask.hpp"
#include "TimeBasedSchedulingService.hpp"

#include "ErrorMaps.hpp"

constexpr uint16_t MAX_ENTRY_SIZE = CCSDSMaxMessageSize //	Max Message size
									+ 6					//  RequestID size
									+ 7;				//  UTC Timestamp size

SpacecraftErrorCode TimeBasedSchedulingService::storeScheduleTCList(etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities>& activityList) {
	if (activityList.empty()) {
		// Empty list
		LOG_ERROR<<"[TC_SCHEDULING] Attempted to store empty list";
		return SpacecraftErrorCode::OBDH_ERROR_EMPTY_TC_SCHEDULE_LIST;
	}

	auto deleteStatus = MemoryManager::deleteFile(MemoryFilesystem::SCHED_TC_FILENAME);
	if (deleteStatus!=Memory_Errno::NONE && deleteStatus!=Memory_Errno::FILE_DOES_NOT_EXIST ) {
		LOG_ERROR<<"[TC_SCHEDULING] Error erasing old file";
		return getSpacecraftErrorCodeFromMemoryError(deleteStatus);
	}

	uint16_t _activities_count = 0;
	for (auto& entry : activityList) {
		// Serialize entry to uint8_t buffer, to store it in memory
		etl::array<uint8_t, MAX_ENTRY_SIZE> entryBuffer = {0};
		uint16_t entryIndex = 0;

		// Append Request to buffer
		auto requestString = MessageParser::compose(entry.request, entry.request.data_size_message_ + ECSSSecondaryTCHeaderSize);
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

		etl::span<const uint8_t> entryBufferSpan(entryBuffer);
		auto status = MemoryManager::writeToFile(MemoryFilesystem::SCHED_TC_FILENAME, entryBufferSpan);
		if (status!=Memory_Errno::NONE) {
			LOG_ERROR<<"[TC_SCHEDULING] Error writing scheduled tc file";
			return getSpacecraftErrorCodeFromMemoryError(deleteStatus);
		}
		_activities_count++;
	}
	LOG_INFO<<"[TC_SCHEDULING] Stored "<<_activities_count<<" scheduled activities";
	return SpacecraftErrorCode::GENERIC_ERROR_NONE;
}

SpacecraftErrorCode TimeBasedSchedulingService::recoverScheduleTCList(etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities>& activityList) {
	uint32_t _scheduledTCfileSize = 0;
	auto status_file = MemoryManager::getFileSize(MemoryFilesystem::SCHED_TC_FILENAME, _scheduledTCfileSize);
	if (status_file!=Memory_Errno::NONE) {
		LOG_ERROR<<"[TC_SCHEDULING] Error writing scheduled tc file";
		return getSpacecraftErrorCodeFromMemoryError(status_file);
	}
	if (_scheduledTCfileSize==0) {
		LOG_INFO<<"[TC_SCHEDULING] Empty scheduled tc file";
		return SpacecraftErrorCode::GENERIC_ERROR_NONE;
	}

	uint32_t iterations = _scheduledTCfileSize/MAX_ENTRY_SIZE;
	if ((_scheduledTCfileSize%MAX_ENTRY_SIZE)!=0) {
		LOG_ERROR<<"[TC_SCHEDULING] Error file size";
		return SpacecraftErrorCode::OBDH_ERROR_CORRUPTED_TC_SCHEDULE_FILE;
	}
	for (int i=0;i<iterations;i++) {
		// Serial buffer, to read entry from memory
		etl::array<uint8_t, MAX_ENTRY_SIZE> entryBuffer = {0};
		etl::span<uint8_t> entryBufferSpan(entryBuffer);
		uint16_t entryIndex = 0;
		uint16_t _read_count = 0;
		return getSpacecraftErrorCodeFromMemoryError(Memory_Errno::UNKNOWN_ERROR);
		// auto readStatus = MemoryManager::readFromFile(MemoryFilesystem::SCHED_TC_FILENAME, entryBufferSpan, i*MAX_ENTRY_SIZE, _read_count);
		// if (readStatus!=Memory_Errno::NONE) {
		// 	LOG_ERROR<<"[TC_SCHEDULING] Error reading from file";
		// 	return getSpacecraftErrorCodeFromMemoryError(readStatus);
		// }
		if (_read_count!=MAX_ENTRY_SIZE) {
			LOG_ERROR<<"[TC_SCHEDULING] Error read unexpected size from file";
			return SpacecraftErrorCode::OBDH_ERROR_CORRUPTED_TC_SCHEDULE_FILE;
		}

		ScheduledActivity entry = {};
		const uint16_t tcSize = static_cast<uint16_t>(entryBuffer.at(4U) << 8U) | static_cast<uint16_t>(entryBuffer.at(5U));
		// +1 Comes from CCSDS protocol (Inside compose is -1), kati malakies
		auto parseError = MessageParser::parse(entryBuffer.data(), tcSize + CCSDSPrimaryHeaderSize + 1, entry.request, false, true); // TODO Ask about true, false flags
		if (parseError!=SpacecraftErrorCode::GENERIC_ERROR_NONE) {
			LOG_ERROR<<"[TC_SCHEDULING] Error parsing message";
			return parseError;
		}
		entryIndex = CCSDSMaxMessageSize;
		entry.requestID.applicationID = (static_cast<uint16_t>(entryBuffer[entryIndex++])<<8 | entryBuffer[entryIndex++]);
		entry.requestID.sequenceCount = (static_cast<uint16_t>(entryBuffer[entryIndex++])<<8 | entryBuffer[entryIndex++]);
		entry.requestID.sourceID = (static_cast<uint16_t>(entryBuffer[entryIndex++])<<8 | entryBuffer[entryIndex++]);
		entry.requestReleaseTime.year = (static_cast<uint16_t>(entryBuffer[entryIndex++])<<8 | entryBuffer[entryIndex++]);
		entry.requestReleaseTime.month = static_cast<uint16_t>(entryBuffer[entryIndex++]);
		entry.requestReleaseTime.day = static_cast<uint16_t>(entryBuffer[entryIndex++]);
		entry.requestReleaseTime.hour = static_cast<uint16_t>(entryBuffer[entryIndex++]);
		entry.requestReleaseTime.minute = static_cast<uint16_t>(entryBuffer[entryIndex++]);
		entry.requestReleaseTime.second = static_cast<uint16_t>(entryBuffer[entryIndex++]);

		if ((activityList.available() == 0)) {
			LOG_ERROR<<"[TC_SCHEDULING] Error Activity list full";
			return SpacecraftErrorCode::OBDH_ERROR_OVERFLOW_TC_SCHEDULE_LIST;
		}
		activityList.push_back(entry);
	}

	// Delete file after reading
	auto deleteStatus = MemoryManager::deleteFile(MemoryFilesystem::SCHED_TC_FILENAME);
	if (deleteStatus!=Memory_Errno::NONE) {
		LOG_ERROR<<"[TC_SCHEDULING] Error erasing old file";
		return getSpacecraftErrorCodeFromMemoryError(deleteStatus);
	}

	LOG_INFO<<"[TC_SCHEDULING] Recovered "<<iterations<<" scheduled activities";
	return SpacecraftErrorCode::GENERIC_ERROR_NONE;
}

TimeBasedSchedulingService::TimeBasedSchedulingService() {
	serviceType = TimeBasedSchedulingService::ServiceType;
}

UTCTimestamp TimeBasedSchedulingService::executeScheduledActivity(UTCTimestamp currentTime) {
	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> scheduledActivities;
	auto recoverStatus = recoverScheduleTCList(scheduledActivities);
	if (recoverStatus != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return {9999, 12, 31, 23, 59, 59};
	}

	if (currentTime >= scheduledActivities.front().requestReleaseTime && !scheduledActivities.empty()) {
		if (scheduledActivities.front().requestID.applicationID == ApplicationId) {
			auto status = tcHandlingTask->addToQueue(scheduledActivities.front().request, 10);
			if (status == true) {
				xTaskNotify(TCHandlingTask::tcHandlingTaskHandle, TASK_BIT_TC_HANDLING, eSetBits);
			}else {
				LOG_ERROR<<"[TC_SCHEDULING] Failed to add activity to TC Handling queue";
			}
		}
		scheduledActivities.pop_front();
	}

	if (!scheduledActivities.empty()) {
		auto storeStatus = storeScheduleTCList(scheduledActivities);
		if (storeStatus != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
			// Reason for failure is printed inside the function
			return {9999, 12, 31, 23, 59, 59};
		}
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
	auto deleteStatus = MemoryManager::deleteFile(MemoryFilesystem::SCHED_TC_FILENAME);
	if (deleteStatus!=Memory_Errno::NONE) {
		ErrorHandler::reportError(request, deleteStatus);
	}
}


void TimeBasedSchedulingService::insertActivities(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::InsertActivities)) {
		return;
	}

	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> scheduledActivities;
	if (recoverScheduleTCList(scheduledActivities) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return;
	}

	uint16_t iterationCount = request.readUint16();
	while (iterationCount-- != 0) {
		const UTCTimestamp currentTime(TimeGetter::getCurrentTimeUTC());
		const UTCTimestamp releaseTime(request.readUTCTimestamp());

		if ((scheduledActivities.available() == 0) || (releaseTime < (currentTime + ECSSTimeMarginForActivation))) {
			LOG_ERROR<<"[TC_SCHEDULING] Rejected scheduled TC";
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
			// request.skipBytes(ECSSTCRequestStringSize);
		} else {
			etl::array<uint8_t, ECSSTCRequestStringSize> requestData = {0};
			request.readString(requestData.data(), ECSSTCRequestStringSize);
			Message receivedTCPacket;
			receivedTCPacket.total_size_ecss_ = request.data_size_ecss_ + ECSSSecondaryTCHeaderSize;
			receivedTCPacket.packet_type_ = Message::TC;
			const auto res = MessageParser::parseECSSTC(requestData.data(), receivedTCPacket);
			if (res != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
				LOG_ERROR<<"[TC_SCHEDULING] Error parsing TC <SEC>: "<<res;
				continue;
			}
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
	if (storeScheduleTCList(scheduledActivities) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return;
	}
	notifyNewActivityAddition();
}

void TimeBasedSchedulingService::timeShiftAllActivities(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::TimeShiftALlScheduledActivities)) {
		return;
	}
	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> scheduledActivities;
	if (recoverScheduleTCList(scheduledActivities) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return;
	}

	const UTCTimestamp currentTime(TimeGetter::getCurrentTimeUTC());

	const auto releaseTimes =
	    etl::minmax_element(scheduledActivities.begin(), scheduledActivities.end(),
	                        [](ScheduledActivity const& leftSide, ScheduledActivity const& rightSide) {
		                        return leftSide.requestReleaseTime < rightSide.requestReleaseTime;
	                        });

	const Time::RelativeTime relativeOffset = request.readRelativeTime();
	if ((releaseTimes.first->requestReleaseTime + std::chrono::seconds(relativeOffset)) < (currentTime + ECSSTimeMarginForActivation)) {
		LOG_ERROR<<"[TC_SCHEDULING] Time shift failed, new release time out of bounds";
		ErrorHandler::reportError(request, ErrorHandler::SubServiceExecutionStartError);
		return;
	}
	for (auto& activity: scheduledActivities) {
		activity.requestReleaseTime += std::chrono::seconds(relativeOffset);
	}
	storeScheduleTCList(scheduledActivities);
}

void TimeBasedSchedulingService::timeShiftActivitiesByID(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::TimeShiftActivitiesById)) {
		return;
	}
	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> scheduledActivities;
	if (recoverScheduleTCList(scheduledActivities) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
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
				LOG_ERROR<<"[TC_SCHEDULING] Time shift failed, new release time out of bounds";
				ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
			} else {
				requestIDMatch->requestReleaseTime += relativeOffset;
			}
		} else {
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
		}
	}
	sortActivitiesReleaseTime(scheduledActivities);
	storeScheduleTCList(scheduledActivities);
}

void TimeBasedSchedulingService::deleteActivitiesByID(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DeleteActivitiesById)) {
		return;
	}
	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> scheduledActivities;

	if (recoverScheduleTCList(scheduledActivities) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
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
			LOG_ERROR<<"[TC_SCHEDULING] Failed to delete activity";
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
		}
	}
	storeScheduleTCList(scheduledActivities);
}

void TimeBasedSchedulingService::detailReportAllActivities(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DetailReportAllScheduledActivities)) {
		return;
	}
	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> scheduledActivities;

	if (recoverScheduleTCList(scheduledActivities) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return;
	}

	timeBasedScheduleDetailReport(scheduledActivities);

	storeScheduleTCList(scheduledActivities);
}

void TimeBasedSchedulingService::timeBasedScheduleDetailReport(etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities>& listOfActivities) {
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
	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> scheduledActivities;
	if (recoverScheduleTCList(scheduledActivities) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
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
			LOG_ERROR<<"[TC_SCHEDULING] Failed to generate activity detailed report";
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
		}
	}

	sortActivitiesReleaseTime(matchedActivities);
	timeBasedScheduleDetailReport(matchedActivities);
	storeScheduleTCList(scheduledActivities);
}

void TimeBasedSchedulingService::summaryReportActivitiesByID(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::ActivitiesSummaryReportById)) {
		return;
	}

	etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities> scheduledActivities;
	if (recoverScheduleTCList(scheduledActivities) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
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
			LOG_ERROR<<"[TC_SCHEDULING] Failed to generate activity summary report";
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
		}
	}
	sortActivitiesReleaseTime(matchedActivities);
	timeBasedScheduleSummaryReport(matchedActivities);
	storeScheduleTCList(scheduledActivities);
}

void TimeBasedSchedulingService::timeBasedScheduleSummaryReport(const etl::list<ScheduledActivity, ECSSMaxNumberOfTimeSchedActivities>& listOfActivities) {
	Message report = createTM(TimeBasedSchedulingService::MessageType::TimeBasedScheduledSummaryReport);

	report.appendUint16(static_cast<uint16_t>(listOfActivities.size()));
	for (const auto& match: listOfActivities) {
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
			insertActivities(message);
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
