#include "HousekeepingService.hpp"
#include "ServicePool.hpp"

void parseHousekeepingStructureFromUint8Array(etl::span<uint8_t> array_input, HousekeepingStructure &structure_output) {
	uint8_t _read_index = 0;

	structure_output.structureId = static_cast<uint16_t>((array_input[_read_index++]<<8) | array_input[_read_index++]);
	structure_output.collectionInterval =	static_cast<uint32_t>((array_input[_read_index++]<<24)) |
											static_cast<uint32_t>((array_input[_read_index++]<<16)) |
											static_cast<uint32_t>((array_input[_read_index++]<<8)) |
											static_cast<uint32_t>((array_input[_read_index++]));
	uint16_t _isPeriodic = static_cast<uint16_t>((array_input[_read_index++]<<8) | array_input[_read_index++]);
	if (_isPeriodic == 1) {
		structure_output.periodicGenerationActionStatus = true;
	}else {
		structure_output.periodicGenerationActionStatus = false;
	}

	uint16_t _parameters_count = static_cast<uint16_t>((array_input[_read_index++]<<8)) | array_input[_read_index++];

	for (int i=0;i<_parameters_count;i++) {
		uint16_t parameter = static_cast<uint16_t>((array_input[_read_index++]<<8)) | array_input[_read_index++];
		structure_output.simplyCommutatedParameterIds.push_back(parameter);
	}

}

void parseUint8ArrayFromHousekeepingStructure(HousekeepingStructure structure_input, etl::span<uint8_t> &array_output) {
	uint8_t _write_index = 0;

	// structureId (uint16_t)
	array_output[_write_index++] = static_cast<uint8_t>(structure_input.structureId >> 8);
	array_output[_write_index++] = static_cast<uint8_t>(structure_input.structureId & 0xFF);

	// collectionInterval (uint32_t)
	array_output[_write_index++] = static_cast<uint8_t>(structure_input.collectionInterval >> 24);
	array_output[_write_index++] = static_cast<uint8_t>((structure_input.collectionInterval >> 16) & 0xFF);
	array_output[_write_index++] = static_cast<uint8_t>((structure_input.collectionInterval >> 8) & 0xFF);
	array_output[_write_index++] = static_cast<uint8_t>(structure_input.collectionInterval & 0xFF);

	// periodicGenerationActionStatus (1 byte)
	array_output[_write_index++] = 0;
	array_output[_write_index++] = structure_input.periodicGenerationActionStatus ? 1 : 0;

	// Parameter count (uint16_t)
	uint16_t _parameters_count = static_cast<uint16_t>(structure_input.simplyCommutatedParameterIds.size());
	array_output[_write_index++] = static_cast<uint8_t>(_parameters_count >> 8);
	array_output[_write_index++] = static_cast<uint8_t>(_parameters_count & 0xFF);

	// Parameter values (each uint16_t)
	for (uint16_t param : structure_input.simplyCommutatedParameterIds) {
		array_output[_write_index++] = static_cast<uint8_t>(param >> 8);
		array_output[_write_index++] = static_cast<uint8_t>(param & 0xFF);
	}
}

void HousekeepingService::readHousekeepingStruct(uint8_t struct_offset, HousekeepingStructure& structure) {
	etl::array<uint8_t, MemoryFilesystem::MRAM_DATA_BLOCK_SIZE-1> _read_arr = {0};
	etl::span<uint8_t> _read_span(_read_arr);
	uint16_t _read_count = 0;
	auto status = MemoryManager::readFromFile(MemoryFilesystem::HOUSEKEEPING_STRUCTS_FILENAME, _read_span, struct_offset, struct_offset+1, _read_count);
	if (status!=Memory_Errno::NONE && status!=Memory_Errno::REACHED_EOF) {
		LOG_ERROR<<"[HOUSEKEEPING_STRUCT] Error recovering housekeeping struct "<<struct_offset<<" returning to default";
		uint8_t _read_index = 0;
		for (int i=0;i<25;i++) {
			uint16_t _read_value = default_housekeeping_structures[(struct_offset*25)+i];
			_read_arr[_read_index++] = _read_value >> 8;
			_read_arr[_read_index++] = _read_value & 0xFF;
		}
	}
	// LOG_INFO<<"[HOUSEKEEPING_STRUCT] Recovered housekeeping struct "<<struct_offset;
	etl::span<uint8_t> _parse_span(_read_arr);
	parseHousekeepingStructureFromUint8Array(_parse_span, structure);
}

void HousekeepingService::updateHouseKeepingStruct(uint8_t struct_offset, HousekeepingStructure structure) {
	etl::array<uint8_t, MemoryFilesystem::MRAM_DATA_BLOCK_SIZE-1> _write_arr = {0};
	etl::span<uint8_t> _parse_span(_write_arr);
	parseUint8ArrayFromHousekeepingStructure(structure, _parse_span);
	etl::span<const uint8_t> _write_span(_write_arr);
	auto status = MemoryManager::writeToMramFileAtOffset(MemoryFilesystem::HOUSEKEEPING_STRUCTS_FILENAME, _write_span, struct_offset);
	if (status!=Memory_Errno::NONE) {
		LOG_ERROR<<"[HOUSEKEEPING_STRUCT] Error saving housekeeping struct "<<struct_offset;
		return;
	}
	LOG_INFO<<"[HOUSEKEEPING_STRUCT] Updated housekeeping struct "<<struct_offset;
}

int HousekeepingService::getHousekeepingStructureById(uint16_t structure_id, HousekeepingStructure& structure) {
	for (int i=0;i<ECSSMaxHousekeepingStructures;i++) {
		readHousekeepingStruct(i, structure);
		if (structure.structureId == structure_id) {
			return i;
		}
	}
	return -1;
}

void HousekeepingService::createHousekeepingReportStructure(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::CreateHousekeepingReportStructure)) {
		return;
	}

	const ParameterReportStructureId idToCreate = request.read<ParameterReportStructureId>();
	if (hasAlreadyExistingStructError(idToCreate, request)) {
		return;
	}
	if (hasExceededMaxNumOfHousekeepingStructsError(request)) {
		return;
	}
	HousekeepingStructure newStructure;
	newStructure.structureId = idToCreate;
	newStructure.collectionInterval = request.read<CollectionInterval>();
	newStructure.periodicGenerationActionStatus = false;

	uint16_t const numOfSimplyCommutatedParams = request.readUint16();

	for (uint16_t i = 0; i < numOfSimplyCommutatedParams; i++) {
		const ParameterId newParamId = request.read<ParameterId>();
		if (hasAlreadyExistingParameterError(newStructure, newParamId, request)) {
			continue;
		}
		newStructure.simplyCommutatedParameterIds.push_back(newParamId);
	}
	updateHouseKeepingStruct((idToCreate%ECSSMaxHousekeepingStructures),newStructure);
}

void HousekeepingService::deleteHousekeepingReportStructure(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DeleteHousekeepingReportStructure)) {
		return;
	}
	uint8_t const numOfStructuresToDelete = request.readUint8();
	for (uint8_t i = 0; i < numOfStructuresToDelete; i++) {
		ParameterReportStructureId const structureId = request.read<ParameterReportStructureId>();
		if (hasNonExistingStructExecutionError(structureId, request)) {
			continue;
		}

		if (hasRequestedDeletionOfEnabledHousekeepingError(structureId, request)) {
			continue;
		}
		HousekeepingStructure structure{};
		int offset = getHousekeepingStructureById(structureId, structure);
		if (offset != -1) {
			HousekeepingStructure clear_structure{};
			updateHouseKeepingStruct(offset, clear_structure);
		}
	}
}

void HousekeepingService::enablePeriodicHousekeepingParametersReport(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::EnablePeriodicHousekeepingParametersReport)) {
		return;
	}

	uint8_t const numOfStructIds = request.readUint8();
	for (uint8_t i = 0; i < numOfStructIds; i++) {
		const ParameterReportStructureId structIdToEnable = request.read<ParameterReportStructureId>();
		if (hasNonExistingStructError(structIdToEnable, request)) {
			continue;
		}
		setPeriodicGenerationActionStatus(structIdToEnable, true);
	}
}

void HousekeepingService::disablePeriodicHousekeepingParametersReport(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::DisablePeriodicHousekeepingParametersReport)) {
		return;
	}

	uint8_t const numOfStructIds = request.readUint8();
	for (uint8_t i = 0; i < numOfStructIds; i++) {
		const ParameterReportStructureId structIdToDisable = request.read<ParameterReportStructureId>();
		if (hasNonExistingStructError(structIdToDisable, request)) {
			continue;
		}
		setPeriodicGenerationActionStatus(structIdToDisable, false);
	}
}

void HousekeepingService::reportHousekeepingStructures(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::ReportHousekeepingStructures)) {
		return;
	}

	uint8_t const numOfStructsToReport = request.readUint8();
	for (uint8_t i = 0; i < numOfStructsToReport; i++) {
		const ParameterReportStructureId structureId = request.read<ParameterReportStructureId>();
		if (hasNonExistingStructExecutionError(structureId, request)) {
			continue;
		}

		housekeepingStructureReport(structureId);
	}
}

void HousekeepingService::housekeepingStructureReport(ParameterReportStructureId structIdToReport) {
	HousekeepingStructure housekeepingStructure = {};
	int offset = getHousekeepingStructureById(structIdToReport, housekeepingStructure);
	if (offset == -1) {
		return;
	}

	if (hasNonExistingStructInternalError(structIdToReport)) {
		return;
	}
	Message structReport = createTM(MessageType::HousekeepingStructuresReport);
	structReport.append<ParameterReportStructureId>(structIdToReport);

	structReport.appendBoolean(housekeepingStructure.periodicGenerationActionStatus);
	structReport.append<CollectionInterval>(housekeepingStructure.collectionInterval);
	structReport.appendUint16(housekeepingStructure.simplyCommutatedParameterIds.size());

	for (auto parameterId: housekeepingStructure.simplyCommutatedParameterIds) {
		structReport.append<ParameterId>(parameterId);
	}
	storeMessage(structReport, structReport.data_size_message_);
}

void HousekeepingService::housekeepingParametersReport(ParameterReportStructureId structureId) {
	if (hasNonExistingStructInternalError(structureId)) {
		return;
	}

	HousekeepingStructure housekeepingStructure = {};
	int offset = getHousekeepingStructureById(structureId, housekeepingStructure);

	Message housekeepingReport = createTM(MessageType::HousekeepingParametersReport);

	housekeepingReport.append<ParameterReportStructureId>(structureId);
	for (auto id: housekeepingStructure.simplyCommutatedParameterIds) {
		Services.parameterManagement.appendParameterToMessage(housekeepingReport, id);
	}
	storeMessage(housekeepingReport, housekeepingReport.data_size_message_);
}

void HousekeepingService::generateOneShotHousekeepingReport(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::GenerateOneShotHousekeepingReport)) {
		return;
	}

	uint8_t const numOfStructsToReport = request.readUint8();
	for (uint8_t i = 0; i < numOfStructsToReport; i++) {
		const ParameterReportStructureId structureId = request.read<ParameterReportStructureId>();
		if (hasNonExistingStructExecutionError(structureId, request)) {
			continue;
		}

		housekeepingParametersReport(structureId);
	}
}

void HousekeepingService::appendParametersToHousekeepingStructure(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::AppendParametersToHousekeepingStructure)) {
		return;
	}

	const ParameterReportStructureId targetStructId = request.read<ParameterReportStructureId>();
	if (hasNonExistingStructExecutionError(targetStructId, request)) {
		return;
	}
	HousekeepingStructure housekeepingStructure = {};
	int offset = getHousekeepingStructureById(targetStructId, housekeepingStructure);
	if (hasRequestedAppendToEnabledHousekeepingError(housekeepingStructure, request)) {
		return;
	}
	uint16_t const numOfSimplyCommutatedParameters = request.readUint16();

	for (uint16_t i = 0; i < numOfSimplyCommutatedParameters; i++) {
		if (hasExceededMaxNumOfSimplyCommutatedParamsError(housekeepingStructure, request)) {
			return;
		}
		const ParameterId newParamId = request.read<ParameterId>();
		if (!Services.parameterManagement.parameterExists(newParamId)) {
			ErrorHandler::reportError(request, ErrorHandler::ExecutionStartErrorType::GetNonExistingParameter);
			continue;
		}
		if (hasAlreadyExistingParameterError(housekeepingStructure, newParamId, request)) {
			continue;
		}
		housekeepingStructure.simplyCommutatedParameterIds.push_back(newParamId);
	}
}

void HousekeepingService::modifyCollectionIntervalOfStructures(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::ModifyCollectionIntervalOfStructures)) {
		return;
	}

	uint8_t const numOfTargetStructs = request.readUint8();
	for (uint8_t i = 0; i < numOfTargetStructs; i++) {
		const ParameterReportStructureId targetStructId = request.read<ParameterReportStructureId>();
		const CollectionInterval newCollectionInterval = request.read<CollectionInterval>();
		if (hasNonExistingStructExecutionError(targetStructId, request)) {
			continue;
		}
		setCollectionInterval(targetStructId, newCollectionInterval);
	}
}

void HousekeepingService::reportHousekeepingPeriodicProperties(Message& request) {
	if (!request.assertTC(ServiceType, MessageType::ReportHousekeepingPeriodicProperties)) {
		return;
	}

	uint8_t numOfValidIds = 0;
	uint8_t const numOfStructIds = request.readUint8();
	for (uint8_t i = 0; i < numOfStructIds; i++) {
		const ParameterReportStructureId structIdToReport = request.read<ParameterReportStructureId>();
		if (structExists(structIdToReport)) {
			numOfValidIds++;
		}
	}
	Message periodicPropertiesReport = createTM(MessageType::HousekeepingPeriodicPropertiesReport);
	periodicPropertiesReport.appendUint8(numOfValidIds);
	request.resetRead();
	request.readUint8();

	for (uint8_t i = 0; i < numOfStructIds; i++) {
		const ParameterReportStructureId structIdToReport = request.read<ParameterReportStructureId>();
		if (hasNonExistingStructExecutionError(structIdToReport, request)) {
			continue;
		}
		appendPeriodicPropertiesToMessage(periodicPropertiesReport, structIdToReport);
	}
	storeMessage(periodicPropertiesReport, periodicPropertiesReport.data_size_message_);
}

void HousekeepingService::appendPeriodicPropertiesToMessage(Message& report, ParameterReportStructureId structureId) {
	report.append<ParameterReportStructureId>(structureId);
	report.appendBoolean(getPeriodicGenerationActionStatus(structureId));
	report.append<CollectionInterval>(getCollectionInterval(structureId));
}

void HousekeepingService::execute(Message& message) {
	switch (message.messageType) {
		case CreateHousekeepingReportStructure:
			createHousekeepingReportStructure(message);
			break;
		case DeleteHousekeepingReportStructure:
			deleteHousekeepingReportStructure(message);
			break;
		case EnablePeriodicHousekeepingParametersReport:
			enablePeriodicHousekeepingParametersReport(message);
			break;
		case DisablePeriodicHousekeepingParametersReport:
			disablePeriodicHousekeepingParametersReport(message);
			break;
		case ReportHousekeepingStructures:
			reportHousekeepingStructures(message);
			break;
		case GenerateOneShotHousekeepingReport:
			generateOneShotHousekeepingReport(message);
			break;
		case AppendParametersToHousekeepingStructure:
			appendParametersToHousekeepingStructure(message);
			break;
		case ModifyCollectionIntervalOfStructures:
			modifyCollectionIntervalOfStructures(message);
			break;
		case ReportHousekeepingPeriodicProperties:
			reportHousekeepingPeriodicProperties(message);
			break;
		default:
			ErrorHandler::reportError(message, ErrorHandler::OtherMessageType);
			break;
	}
}

bool HousekeepingService::existsInVector(const etl::vector<uint16_t, ECSSMaxSimplyCommutatedParameters>& ids,
                                         ParameterId parameterId) {
	return std::find(std::begin(ids), std::end(ids), parameterId) != std::end(ids);
}

UTCTimestamp HousekeepingService::reportPendingStructures(UTCTimestamp currentTime, UTCTimestamp previousTime, UTCTimestamp expectedDelay) {
	UTCTimestamp nextCollection{9999, 12, 31, 23, 59, 59}; // Max timestamp

	for (int i=0;i<ECSSMaxHousekeepingStructures;i++) {
		HousekeepingStructure housekeepingStructure = {};
		readHousekeepingStruct(i, housekeepingStructure);
		if (!housekeepingStructure.periodicGenerationActionStatus) {
			continue;
		}
		if (housekeepingStructure.collectionInterval == 0) {
			housekeepingParametersReport(housekeepingStructure.structureId);
			nextCollection = currentTime;;
			continue;
		}
		const uint64_t currentSeconds = currentTime.toEpochSeconds();
		const uint64_t previousSeconds = previousTime.toEpochSeconds();
		const uint64_t delaySeconds = expectedDelay.toEpochSeconds();
		const uint64_t interval = housekeepingStructure.collectionInterval;

		if (currentSeconds != 0 && (currentSeconds % interval == 0 ||
									(previousSeconds + delaySeconds) % interval == 0)) {
			housekeepingParametersReport(housekeepingStructure.structureId);
									}

		uint64_t secondsUntilNextCollection = interval - (currentSeconds % interval);
		const UTCTimestamp structureTimeToCollection = currentTime + std::chrono::seconds(secondsUntilNextCollection);
		if (nextCollection > structureTimeToCollection) {
			nextCollection = structureTimeToCollection;
		}
	}

	return nextCollection;
}

bool HousekeepingService::hasNonExistingStructExecutionError(ParameterReportStructureId id, const Message& request) {
	if (!structExists(id)) {
		ErrorHandler::reportError(request, ErrorHandler::ExecutionStartErrorType::RequestedNonExistingStructure);
		return true;
	}
	return false;
}

bool HousekeepingService::hasNonExistingStructError(ParameterReportStructureId id, const Message& request) {
	if (!structExists(id)) {
		ErrorHandler::reportError(request, ErrorHandler::RequestedNonExistingStructure);
		return true;
	}
	return false;
}

bool HousekeepingService::hasNonExistingStructInternalError(ParameterReportStructureId id) {
	if (!structExists(id)) {
		ErrorHandler::reportInternalError(ErrorHandler::InternalErrorType::NonExistentHousekeeping);
		return true;
	}
	return false;
}

bool HousekeepingService::hasAlreadyExistingParameterError(const HousekeepingStructure& housekeepingStruct, ParameterReportStructureId id, const Message& request) {
	if (existsInVector(housekeepingStruct.simplyCommutatedParameterIds, id)) {
		ErrorHandler::reportError(request, ErrorHandler::ExecutionStartErrorType::AlreadyExistingParameter);
		return true;
	}
	return false;
}

bool HousekeepingService::hasAlreadyExistingStructError(ParameterReportStructureId id, const Message& request) {
	if (structExists(id)) {
		ErrorHandler::reportError(request, ErrorHandler::ExecutionStartErrorType::RequestedAlreadyExistingStructure);
		return true;
	}
	return false;
}

bool HousekeepingService::hasExceededMaxNumOfHousekeepingStructsError(const Message& request) {
	// if (housekeepingStructures.size() >= ECSSMaxHousekeepingStructures) {
	// 	ErrorHandler::reportError(request, ErrorHandler::ExecutionStartErrorType::ExceededMaxNumberOfHousekeepingStructures);
	// 	return true;
	// }
	return false;
}

bool HousekeepingService::hasRequestedAppendToEnabledHousekeepingError(const HousekeepingStructure& housekeepingStruct, const Message& request) {
	if (housekeepingStruct.periodicGenerationActionStatus) {
		ErrorHandler::reportError(request, ErrorHandler::ExecutionStartErrorType::RequestedAppendToEnabledHousekeeping);
		return true;
	}
	return false;
}

bool HousekeepingService::hasRequestedDeletionOfEnabledHousekeepingError(ParameterReportStructureId id, const Message& request) {
	if (getPeriodicGenerationActionStatus(id)) {
		ErrorHandler::reportError(request, ErrorHandler::ExecutionStartErrorType::RequestedDeletionOfEnabledHousekeeping);
		return true;
	}
	return false;
}

bool HousekeepingService::hasExceededMaxNumOfSimplyCommutatedParamsError(const HousekeepingStructure& housekeepingStruct, const Message& request) {
	if (housekeepingStruct.simplyCommutatedParameterIds.size() >= ECSSMaxSimplyCommutatedParameters) {
		ErrorHandler::reportError(request, ErrorHandler::ExecutionStartErrorType::ExceededMaxNumberOfSimplyCommutatedParameters);
		return true;
	}
	return false;
}