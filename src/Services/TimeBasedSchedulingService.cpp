#include "ECSS_Configuration.hpp"
#ifdef SERVICE_TIMESCHEDULING

#include "MemoryManager.hpp"
#include "TCHandlingTask.hpp"
#include "TimeBasedSchedulingService.hpp"

#include "ErrorMaps.hpp"
#include "TimeBasedSchedulingTask.hpp"


SpacecraftErrorCode TimeBasedSchedulingService::readActivityEntries(ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities]) {
	etl::array<uint8_t, activitiesEntriesArraySize> _activityEntriesBuffer = {0};
	etl::span<uint8_t> _bufferSpan(_activityEntriesBuffer);
	uint16_t read_count = 0;
	uint32_t _start_activity_block = 0;
	uint32_t _end_activity_block = _start_activity_block + MRAM_BLOCKS_OFFSET_ACTIVITIES_LIST;
	const auto status = MemoryManager::readFromFile(MemoryFilesystem::SCHED_TC_FILENAME, _bufferSpan, 0, 2, read_count);

	if (status != Memory_Errno::NONE) {
		return getSpacecraftErrorCodeFromMemoryError(status);
	}
	if (read_count!=activitiesEntriesArraySize) {
		return getSpacecraftErrorCodeFromMemoryError(Memory_Errno::BAD_DATA);
	}

	uint8_t _activityEntriesIndex = 0;
	for (int i = 0; i < ECSSMaxNumberOfTimeSchedActivities; ++i) {
		ActivityEntry& entry = entries[i];

		entry.id = _activityEntriesBuffer[_activityEntriesIndex++];
		entry.timestamp.year = static_cast<uint16_t>(_activityEntriesBuffer[_activityEntriesIndex++] << 8);
		entry.timestamp.year |= static_cast<uint16_t>(_activityEntriesBuffer[_activityEntriesIndex++]);
		entry.timestamp.month = _activityEntriesBuffer[_activityEntriesIndex++];
		entry.timestamp.day = _activityEntriesBuffer[_activityEntriesIndex++];
		entry.timestamp.hour = _activityEntriesBuffer[_activityEntriesIndex++];
		entry.timestamp.minute = _activityEntriesBuffer[_activityEntriesIndex++];
		entry.timestamp.second = _activityEntriesBuffer[_activityEntriesIndex++];
		entry.state = static_cast<Activity_State>(_activityEntriesBuffer[_activityEntriesIndex++]);
	}

	return SpacecraftErrorCode::GENERIC_ERROR_NONE;
}

SpacecraftErrorCode TimeBasedSchedulingService::storeActivityEntries(ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities]) {
	etl::array<uint8_t, activitiesEntriesArraySize> _activityEntriesBuffer = {0};
	uint8_t _activityEntriesIndex = 0;
	for (int i=0;i<ECSSMaxNumberOfTimeSchedActivities;i++) {
		_activityEntriesBuffer[_activityEntriesIndex++] = entries[i].id;
		_activityEntriesBuffer[_activityEntriesIndex++] = static_cast<uint8_t>(entries[i].timestamp.year >> 8);	// MSB
		_activityEntriesBuffer[_activityEntriesIndex++] = static_cast<uint8_t>(entries[i].timestamp.year & 0xFF);	// LSB
		_activityEntriesBuffer[_activityEntriesIndex++] = entries[i].timestamp.month;
		_activityEntriesBuffer[_activityEntriesIndex++] = entries[i].timestamp.day;
		_activityEntriesBuffer[_activityEntriesIndex++] = entries[i].timestamp.hour;
		_activityEntriesBuffer[_activityEntriesIndex++] = entries[i].timestamp.minute;
		_activityEntriesBuffer[_activityEntriesIndex++] = entries[i].timestamp.second;
		_activityEntriesBuffer[_activityEntriesIndex++] = static_cast<uint8_t>(entries[i].state);
	}
	etl::span<const uint8_t> _bufferSpan(_activityEntriesBuffer);
	const auto status = MemoryManager::writeToMramFileAtOffset(MemoryFilesystem::SCHED_TC_FILENAME, _bufferSpan, 0);
	if (status != Memory_Errno::NONE) {
		return getSpacecraftErrorCodeFromMemoryError(status);
	}
	return SpacecraftErrorCode::GENERIC_ERROR_NONE;
}

SpacecraftErrorCode TimeBasedSchedulingService::storeScheduledActivity(ScheduledActivity activity, const uint8_t id) {
	// Serialize entry to uint8_t buffer, to store it in memory
	etl::array<uint8_t, MAX_ENTRY_SIZE> entryBuffer = {0};
	uint16_t entryIndex = 0;

	// Append Request to buffer
	auto requestString = MessageParser::compose(activity.request, activity.request.data_size_message_ + ECSSSecondaryTCHeaderSize);
	memcpy(entryBuffer.data(), requestString.value().data(), requestString.value().size());
	entryIndex = CCSDSMaxMessageSize; // Move iterator to end of available message space, to help with parsing

	// Append requestID to buffer
	entryBuffer[entryIndex++] = static_cast<uint8_t>(activity.requestID.applicationID >> 8);	// MSB
	entryBuffer[entryIndex++] = static_cast<uint8_t>(activity.requestID.applicationID & 0xFF); // LSB

	entryBuffer[entryIndex++] = static_cast<uint8_t>(activity.requestID.sequenceCount >> 8);	// MSB
	entryBuffer[entryIndex++] = static_cast<uint8_t>(activity.requestID.sequenceCount & 0xFF); // LSB

	entryBuffer[entryIndex++] = static_cast<uint8_t>(activity.requestID.sourceID >> 8);	// MSB
	entryBuffer[entryIndex++] = static_cast<uint8_t>(activity.requestID.sourceID & 0xFF);	// LSB

	// Append requestReleaseTime to buffer
	entryBuffer[entryIndex++] = static_cast<uint8_t>(activity.requestReleaseTime.year >> 8);	// MSB
	entryBuffer[entryIndex++] = static_cast<uint8_t>(activity.requestReleaseTime.year & 0xFF);	// LSB

	entryBuffer[entryIndex++] = activity.requestReleaseTime.month;
	entryBuffer[entryIndex++] = activity.requestReleaseTime.day;
	entryBuffer[entryIndex++] = activity.requestReleaseTime.hour;
	entryBuffer[entryIndex++] = activity.requestReleaseTime.minute;
	entryBuffer[entryIndex++] = activity.requestReleaseTime.second;

	etl::span<const uint8_t> entryBufferSpan(entryBuffer);

	uint32_t _block_offset = MRAM_BLOCKS_OFFSET_ACTIVITIES_LIST + (id * MRAM_BLOCKS_PER_ACTIVITY);

	auto status = MemoryManager::writeToMramFileAtOffset(MemoryFilesystem::SCHED_TC_FILENAME, entryBufferSpan, _block_offset);
	if (status!=Memory_Errno::NONE) {
		LOG_ERROR<<"[TC_SCHEDULING] Error storing scheduled activity";
		return getSpacecraftErrorCodeFromMemoryError(status);
	}
	LOG_INFO<<"[TC_SCHEDULING] Stored scheduled activity with id: "<<id;
	return SpacecraftErrorCode::GENERIC_ERROR_NONE;
}

SpacecraftErrorCode TimeBasedSchedulingService::recoverScheduledActivity(ScheduledActivity& activity, const uint8_t id) {
	// Serial buffer, to read entry from memory
	etl::array<uint8_t, MAX_ENTRY_SIZE> entryBuffer = {0};
	etl::span<uint8_t> entryBufferSpan(entryBuffer);
	uint16_t _read_count = 0;

	uint32_t _start_mram_block = MRAM_BLOCKS_OFFSET_ACTIVITIES_LIST + (id * MRAM_BLOCKS_PER_ACTIVITY);
	uint32_t _end_mram_block =  _start_mram_block + MRAM_BLOCKS_PER_ACTIVITY;

	auto status = MemoryManager::readFromFile(MemoryFilesystem::SCHED_TC_FILENAME, entryBufferSpan, _start_mram_block, _end_mram_block, _read_count);

	if (status != Memory_Errno::NONE || _read_count!=MAX_ENTRY_SIZE) {
		LOG_ERROR<<"[TC_SCHEDULING] Unable to recover scheduled activity";
		return SpacecraftErrorCode::OBDH_ERROR_CORRUPTED_TC_SCHEDULE_FILE;
	}

	const uint16_t tcSize = static_cast<uint16_t>(entryBuffer.at(4U) << 8U) | static_cast<uint16_t>(entryBuffer.at(5U));
	// +1 Comes from CCSDS protocol (Inside compose is -1), kati malakies
	auto parseError = MessageParser::parse(entryBuffer.data(), tcSize + CCSDSPrimaryHeaderSize + 1, activity.request, false, true); // TODO Ask about true, false flags
	if (parseError!=SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		LOG_ERROR<<"[TC_SCHEDULING] Error parsing message";
		return parseError;
	}
	uint16_t entryIndex = CCSDSMaxMessageSize;
	activity.requestID.applicationID = (static_cast<uint16_t>(entryBuffer[entryIndex++])<<8 | entryBuffer[entryIndex++]);
	activity.requestID.sequenceCount = (static_cast<uint16_t>(entryBuffer[entryIndex++])<<8 | entryBuffer[entryIndex++]);
	activity.requestID.sourceID = (static_cast<uint16_t>(entryBuffer[entryIndex++])<<8 | entryBuffer[entryIndex++]);
	activity.requestReleaseTime.year = (static_cast<uint16_t>(entryBuffer[entryIndex++])<<8 | entryBuffer[entryIndex++]);
	activity.requestReleaseTime.month = static_cast<uint16_t>(entryBuffer[entryIndex++]);
	activity.requestReleaseTime.day = static_cast<uint16_t>(entryBuffer[entryIndex++]);
	activity.requestReleaseTime.hour = static_cast<uint16_t>(entryBuffer[entryIndex++]);
	activity.requestReleaseTime.minute = static_cast<uint16_t>(entryBuffer[entryIndex++]);
	activity.requestReleaseTime.second = static_cast<uint16_t>(entryBuffer[entryIndex++]);

	LOG_INFO<<"[TC_SCHEDULING] Recovered scheduled activity with id: "<<id;
	return SpacecraftErrorCode::GENERIC_ERROR_NONE;
}

TimeBasedSchedulingService::TimeBasedSchedulingService() {
	serviceType = TimeBasedSchedulingService::ServiceType;
}

UTCTimestamp TimeBasedSchedulingService::getNextScheduledActivityTimestamp(UTCTimestamp currentTime) {
	ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities];
	if (readActivityEntries(entries) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return {9999, 12, 31, 23, 59, 59};
	}
	sortActivityEntries(entries);
	auto storeStatus = storeActivityEntries(entries);
	if (storeStatus != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return {9999, 12, 31, 23, 59, 59};
	}
	if (entries[0].state != Activity_State::invalid) {
		if (currentTime > entries[0].timestamp) {
			if (isExecutionTimeWithinMargin(currentTime, entries[0].timestamp)) {
				return currentTime;
			}
		}
		if (currentTime < entries[0].timestamp) {
			LOG_INFO<<"[TC_SCHEDULING] Maybe a time shift happened?";
			uint8_t _stored_tc_to_check = 0;
			while ((not isExecutionTimeWithinMargin(currentTime, entries[_stored_tc_to_check].timestamp))  && (entries[_stored_tc_to_check].state == Activity_State::waiting)) {
				// Expired TC
				LOG_DEBUG<<"[TC_SCHEDULING] Found expired TC, invalidating";
				entries[_stored_tc_to_check].state = Activity_State::invalid;
				_stored_tc_to_check++;
				if (_stored_tc_to_check == ECSSMaxNumberOfTimeSchedActivities) {
					return {9999, 12, 31, 23, 59, 59};
				}
			}
			if (entries[_stored_tc_to_check].state == Activity_State::waiting) {
				return entries[_stored_tc_to_check].timestamp;
			}
			return {9999, 12, 31, 23, 59, 59};
		}

		return entries[0].timestamp;
	}
	return {9999, 12, 31, 23, 59, 59};
}

bool TimeBasedSchedulingService::hasActivityExpired(UTCTimestamp currentTime, UTCTimestamp executionTime) const {
	if (currentTime > executionTime) {
		if ( std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - executionTime).count() >= _tc_execution_margin_ms ) {
			return true;
		}
	}
	return false;
}

bool TimeBasedSchedulingService::isExecutionTimeWithinMargin(UTCTimestamp currentTime, UTCTimestamp executionTime) const {
	if (currentTime < executionTime) {
		if ( std::chrono::duration_cast<std::chrono::milliseconds>(executionTime - currentTime).count() <= _tc_execution_margin_ms ) {
			return true;
		}
	}else {
		if ( std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - executionTime).count() <= _tc_execution_margin_ms ) {
			return true;
		}
	}
	return false;
}

void TimeBasedSchedulingService::executeScheduledActivity(UTCTimestamp currentTime) {
	ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities];
	if (readActivityEntries(entries) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return;
	}

	uint8_t _stored_tc_to_check = 0;
	while ((hasActivityExpired(currentTime, entries[_stored_tc_to_check].timestamp))  && (entries[_stored_tc_to_check].state == Activity_State::waiting)) {
		// Expired TC
		LOG_DEBUG<<"[TC_SCHEDULING] Found expired TC, invalidating";
		entries[_stored_tc_to_check].state = Activity_State::invalid;
		_stored_tc_to_check++;
		if (_stored_tc_to_check == ECSSMaxNumberOfTimeSchedActivities) {
			_stored_tc_to_check=0;
			break;
		}
	}

	while (isExecutionTimeWithinMargin(currentTime, entries[_stored_tc_to_check].timestamp) && (entries[_stored_tc_to_check].state == Activity_State::waiting) ) {
		ScheduledActivity _activity;
		auto recoverStatus = recoverScheduledActivity(_activity, entries[_stored_tc_to_check].id);
		if (recoverStatus != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
			return;
		}
		if (_activity.requestID.applicationID == ApplicationId) {
			auto status = tcHandlingTask->addToQueue(_activity.request, 20);
			if (status == true) {
				xTaskNotify(tcHandlingTask->taskHandle, TASK_BIT_TC_HANDLING, eSetBits);
				LOG_DEBUG<<"[TC_SCHEDULING] Added activity to TC Handling queue";
			}else {
				LOG_ERROR<<"[TC_SCHEDULING] Failed to add activity to TC Handling queue";
			}
		}
		entries[_stored_tc_to_check].state = Activity_State::invalid;
		_stored_tc_to_check++;
	}
	sortActivityEntries(entries);
	storeActivityEntries(entries);
}

void TimeBasedSchedulingService::enableScheduleExecution(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::EnableTimeBasedScheduleExecutionFunction)) {
		return;
	}
	executionFunctionStatus = true;
	uint8_t _active_tc_schedule = 1;
	MemoryManager::setParameter(PeakSatParameters::OBDH_TC_SCHEDULE_ACTIVE_ID, static_cast<void*>(&_active_tc_schedule));

	Services.requestVerification.successCompletionExecutionVerification(request);
}

void TimeBasedSchedulingService::disableScheduleExecution(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DisableTimeBasedScheduleExecutionFunction)) {
		return;
	}
	executionFunctionStatus = false;
	uint8_t _active_tc_schedule = 0;
	MemoryManager::setParameter(PeakSatParameters::OBDH_TC_SCHEDULE_ACTIVE_ID, static_cast<void*>(&_active_tc_schedule));

	Services.requestVerification.successCompletionExecutionVerification(request);
}

void TimeBasedSchedulingService::resetSchedule(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::ResetTimeBasedSchedule)) {
		return;
	}
	uint8_t _valid_tc_schedule = 0;

	ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities] = {0};
	for (int i=0; i<ECSSMaxNumberOfTimeSchedActivities;i++) {
		entries[i].id = i;
		entries[i].state = Activity_State::invalid;
	}
	const auto deleteStatus = storeActivityEntries(entries);

	if (deleteStatus != GENERIC_ERROR_NONE) {
		LOG_ERROR<<"[TC_SCHEDULING] Error reseting schedule <SEC>"<<static_cast<uint16_t>(deleteStatus);
		Services.requestVerification.failCompletionExecutionVerification(request, static_cast<SpacecraftErrorCode>(deleteStatus));
		return; // Exit execution
	}

	Services.requestVerification.successCompletionExecutionVerification(request);

	_valid_tc_schedule = 1;
	uint8_t _is_memory_health_check_programmed = 0;
	MemoryManager::setParameter(PeakSatParameters::OBDH_VALID_TC_SCHEDULE_LIST_ID, static_cast<void*>(&_valid_tc_schedule));
	MemoryManager::setParameter(PeakSatParameters::OBDH_MEMORY_HEALTH_CHECKS_IS_SET_ID, static_cast<void*>(&_is_memory_health_check_programmed));
	LOG_DEBUG<<"[TC_SCHEDULING] Schedule reset";
	notifyNewActivityAddition();
}

void TimeBasedSchedulingService::insertActivities(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::InsertActivities)) {
		return;
	}

	ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities];
	if (readActivityEntries(entries) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return;
	}

	uint16_t iterationCount = request.readUint16();
	while (iterationCount != 0) {
		const UTCTimestamp currentTime(TimeGetter::getCurrentTimeUTC());
		const UTCTimestamp releaseTime(request.readUTCTimestamp());

		if ((releaseTime < (currentTime + ECSSTimeMarginForActivation))) {
			LOG_ERROR<<"[TC_SCHEDULING] Rejected scheduled TC due to short release time";
			ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
			// request.skipBytes(ECSSTCRequestStringSize);
		} else {
			uint8_t _available_id = 255;
			for (int i=0; i<ECSSMaxNumberOfTimeSchedActivities; i++){
				if (entries[i].state != Activity_State::waiting) {
					_available_id = entries[i].id;
					entries[i].state = Activity_State::waiting;
					entries[i].timestamp = releaseTime;
					break;
				}
			}
			if (_available_id == 255) {
				LOG_ERROR<<"[TC_SCHEDULING] Rejected scheduled TC, list full";
				ErrorHandler::reportError(request, ErrorHandler::InstructionExecutionStartError);
				return;
			}
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

			auto storeStatus = storeScheduledActivity(newActivity, _available_id);
			if (storeStatus != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
				// Reason for failure is printed inside the function
				return;
			}
		}
		iterationCount-=1;
	}
	sortActivityEntries(entries);
	auto status = storeActivityEntries(entries);

	if (status != GENERIC_ERROR_NONE) {
		Services.requestVerification.failCompletionExecutionVerification(request, static_cast<SpacecraftErrorCode>(status));
		return; // Exit execution
	}
	Services.requestVerification.successCompletionExecutionVerification(request);

	notifyNewActivityAddition();
}

void TimeBasedSchedulingService::timeShiftAllActivities(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::TimeShiftALlScheduledActivities)) {
		return;
	}

	ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities];
	if (readActivityEntries(entries) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return;
	}

	const UTCTimestamp currentTime(TimeGetter::getCurrentTimeUTC());

	const Time::RelativeTime relativeOffset = request.readRelativeTime();
	if ((entries[0].timestamp + std::chrono::seconds(relativeOffset)) < (currentTime + ECSSTimeMarginForActivation)) {
		LOG_ERROR<<"[TC_SCHEDULING] Time shift failed, new release time out of bounds";
		ErrorHandler::reportError(request, ErrorHandler::SubServiceExecutionStartError);
		return;
	}
	for (int i=0; i< ECSSMaxNumberOfTimeSchedActivities; i++) {
		if (entries[i].state == Activity_State::waiting) {
			entries[i].timestamp += std::chrono::seconds(relativeOffset);
		}
	}
	auto status = storeActivityEntries(entries);

	if (status != GENERIC_ERROR_NONE) {
		Services.requestVerification.failCompletionExecutionVerification(request, static_cast<SpacecraftErrorCode>(status));
	}
	Services.requestVerification.successCompletionExecutionVerification(request);
}

void TimeBasedSchedulingService::detailReportAllActivities(const Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DetailReportAllScheduledActivities)) {
		return;
	}
	timeBasedScheduleDetailReport();
}

void TimeBasedSchedulingService::timeBasedScheduleDetailReport() {
	// Message report = createTM(TimeBasedSchedulingService::MessageType::TimeBasedScheduleReportById);
	// report.appendUint16(static_cast<uint16_t>(listOfActivities.size()));
	//
	// for (auto& activity: listOfActivities) {
	// 	report.appendUTCTimestamp(activity.requestReleaseTime); // todo (#267): Replace with the time parser
	// 	auto result = MessageParser::composeECSS(activity.request, activity.request.total_size_ecss_);
	// 	if (result.has_value()) {
	// 		report.appendString(result.value());
	// 	}
	// 	// Note: If composition fails, the activity is skipped in the report
	// }
	// storeMessage(report, report.data_size_message_);
}

void TimeBasedSchedulingService::timeBasedScheduleSummaryReport() {
	// Message report = createTM(TimeBasedSchedulingService::MessageType::TimeBasedScheduledSummaryReport);
	//
	// report.appendUint16(static_cast<uint16_t>(listOfActivities.size()));
	// for (const auto& match: listOfActivities) {
	// 	report.appendUTCTimestamp(match.requestReleaseTime);
	// 	report.append<SourceId>(match.requestID.sourceID);
	// 	report.append<ApplicationProcessId>(match.requestID.applicationID);
	// 	report.append<SequenceCount>(match.requestID.sequenceCount);
	// }
	// storeMessage(report, report.data_size_message_);
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

void TimeBasedSchedulingService::initEsotericVariables() {
	uint8_t _valid_schedule_list = 0;
	uint8_t _active_tc_schedule = 0;
	uint8_t _tc_execution_margin = 0;
	MemoryManager::getParameter(PeakSatParameters::OBDH_VALID_TC_SCHEDULE_LIST_ID, static_cast<void*>(&_valid_schedule_list));
	MemoryManager::getParameter(PeakSatParameters::OBDH_TC_SCHEDULE_ACTIVE_ID, static_cast<void*>(&_active_tc_schedule));
	MemoryManager::getParameter(PeakSatParameters::OBDH_SCHEDULED_TC_EXECUTION_MARGIN_ID, static_cast<void*>(&_tc_execution_margin));

	ActivityEntry entries[ECSSMaxNumberOfTimeSchedActivities] = {0};
	if (_valid_schedule_list == 0) {
		// Invalid schedule list, initialise array in MRAM with the correct IDs
		for (int i=0; i<ECSSMaxNumberOfTimeSchedActivities;i++) {
			entries[i].id = i;
			entries[i].state = Activity_State::invalid;
		}
		const auto status = storeActivityEntries(entries);
		if (status!=SpacecraftErrorCode::GENERIC_ERROR_NONE) {
			LOG_ERROR<<"[TC_SCHEDULING] Error initialising schedule <SEC>"<<static_cast<uint16_t>(status);
		}
		_valid_schedule_list = 1;
		MemoryManager::setParameter(PeakSatParameters::OBDH_VALID_TC_SCHEDULE_LIST_ID, static_cast<void*>(&_valid_schedule_list));
	}


	if (readActivityEntries(entries) != SpacecraftErrorCode::GENERIC_ERROR_NONE) {
		// Reason for failure is printed inside the function
		return;
	}

	uint8_t _stored_tc_to_check = 0;
	auto currentTime = TimeGetter::getCurrentTimeUTC();
	while ((hasActivityExpired(currentTime, entries[_stored_tc_to_check].timestamp))  && (entries[_stored_tc_to_check].state == Activity_State::waiting)) {
		// Expired TC
		LOG_DEBUG<<"[TC_SCHEDULING] Found expired TC, invalidating";
		entries[_stored_tc_to_check].state = Activity_State::invalid;
		_stored_tc_to_check++;
		if (_stored_tc_to_check == ECSSMaxNumberOfTimeSchedActivities) {
			_stored_tc_to_check=0;
			break;
		}
	}

	if (_active_tc_schedule != 0) {
		executionFunctionStatus = true;
	}else {
		executionFunctionStatus = false;
	}

	_tc_execution_margin_ms = _tc_execution_margin * 1000;

}


#endif
