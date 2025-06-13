#include "ECSS_Configuration.hpp"
#ifdef SERVICE_LARGEPACKET

#include <LargePacketTransferService.hpp>
#include <etl/String.hpp>

#include "ErrorMaps.hpp"
#include "HelperFunctions.hpp"
#include "Message.hpp"
#include "PMON_Handlers.hpp"

#include "ServicePool.hpp"

static_assert(ECSSMaxFixedOctetStringSize%(MemoryFilesystem::MRAM_DATA_BLOCK_SIZE-1)==0, "ECSSMaxFixedOctetStringSize must be a multiple of MRAM_DATA_BLOCK_SIZE");

template <typename T>
 bool LargePacketTransferService::getMemoryParameter(Message& message, const ParameterId paramId, T* value) {
	auto result = MemoryManager::getParameter(paramId, value);
	if (!result.has_value()) {
		Services.requestVerification.failAcceptanceVerification(
			message, getSpacecraftErrorCodeFromMemoryError(result.error()));
		return false;
	}
	return true;
}

template <typename T>
 bool LargePacketTransferService::setMemoryParameter(Message& message, const ParameterId paramId, T* value) {
	auto result = MemoryManager::setParameter(paramId, value);
	if (!result.has_value()) {
		Services.requestVerification.failAcceptanceVerification(
			message, getSpacecraftErrorCodeFromMemoryError(result.error()));
		return false;
	}
	return true;
}


void LargePacketTransferService::firstDownlinkPartReport(const LargeMessageTransactionId largeMessageTransactionIdentifier,
                                                         const PartSequenceNum partSequenceNumber,
                                                         const String<ECSSMaxFixedOctetStringSize>& string) const {
	Message report = createTM(LargePacketTransferService::MessageType::FirstDownlinkPartReport);
	report.append<LargeMessageTransactionId>(largeMessageTransactionIdentifier); // large message transaction identifier
	report.append<PartSequenceNum>(partSequenceNumber);                          // part sequence number
	report.appendOctetString(string);                                            // fixed octet-string
	storeMessage(report, report.data_size_message_);
}

void LargePacketTransferService::intermediateDownlinkPartReport(
    const LargeMessageTransactionId largeMessageTransactionIdentifier, const PartSequenceNum partSequenceNumber,
    const String<ECSSMaxFixedOctetStringSize>& string) const {

	Message report = createTM(LargePacketTransferService::MessageType::IntermediateDownlinkPartReport);
	report.append<LargeMessageTransactionId>(largeMessageTransactionIdentifier); // large message transaction identifier
	report.append<PartSequenceNum>(partSequenceNumber);                          // part sequence number
	report.appendOctetString(string);                                            // fixed octet-string
	storeMessage(report, report.data_size_message_);
}

void LargePacketTransferService::lastDownlinkPartReport(LargeMessageTransactionId largeMessageTransactionIdentifier,
                                                        PartSequenceNum partSequenceNumber,
                                                        const String<ECSSMaxFixedOctetStringSize>& string) const {
	Message report = createTM(LargePacketTransferService::MessageType::LastDownlinkPartReport);
	report.append<LargeMessageTransactionId>(largeMessageTransactionIdentifier); // large message transaction identifier
	report.append<PartSequenceNum>(partSequenceNumber);                          // part sequence number
	report.appendOctetString(string);                                            // fixed octet-string
	storeMessage(report, report.data_size_message_);
}


void LargePacketTransferService::firstUplinkPart(Message& message) {
	LargeMessageTransactionId largeMessageTransactionIdentifier = 0U;

	if (!validateUplinkMessage(message, MessageType::FirstUplinkPartReport, largeMessageTransactionIdentifier)) {
		return;
	}

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_MESSAGE_TRANSACTION_IDENTIFIER_ID,
	                                         static_cast<void*>(&largeMessageTransactionIdentifier))) {
		return;
	}

	PartSequenceNum partSequenceNumber = message.read<PartSequenceNum>();
	if (partSequenceNumber != 0U) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	etl::array<uint8_t, ECSSMaxFixedOctetStringSize> dataArray{};
	message.readOctetString(dataArray.data());

	// Extract filename safely
	etl::string<MemoryFilesystem::MAX_FILENAME> filename_sized{};
	const size_t copySize = std::min(static_cast<size_t>(MemoryFilesystem::MAX_FILENAME),
									dataArray.size());
	filename_sized.assign(dataArray.begin(), dataArray.begin() + copySize);

	// Extract size with bounds checking
	constexpr size_t SIZE_OFFSET = 10U;
	constexpr size_t REQUIRED_SIZE = 14U; // Need at least 14 bytes

	if (dataArray.size() < REQUIRED_SIZE) {
		Services.requestVerification.failAcceptanceVerification(
			message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	auto size = static_cast<uint32_t>(dataArray[10]) |
				(static_cast<uint32_t>(dataArray[11]) << 8) |
				(static_cast<uint32_t>(dataArray[12]) << 16) |
				(static_cast<uint32_t>(dataArray[13]) << 24);

	PMON_Handlers::raiseMRAMErrorEvent(
	    MemoryManager::setParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_UPLINK_SIZE_ID, &size));

	if (auto validateFile = MemoryManagerHelpers::getFileTransferIdFromFilename(filename_sized.data()); validateFile != largeMessageTransactionIdentifier) {
		resetTransferParameters();
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	if (filename_sized.size() >= MemoryFilesystem::MAX_FILENAME) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID, &partSequenceNumber)) {
		return;
	}

	localFilename.clear();
	localFilename.assign(filename_sized.begin(), filename_sized.end());
	// todo start timer
	Services.requestVerification.successAcceptanceVerification(message);
}


void LargePacketTransferService::intermediateUplinkPart(Message& message) {
	LargeMessageTransactionId largeMessageTransactionIdentifier = 0U;

	if (!validateUplinkMessage(message, MessageType::IntermediateUplinkPartReport, largeMessageTransactionIdentifier)) {
		return;
	}

	if (!validateStoredTransactionId(message, largeMessageTransactionIdentifier)) {
		return;
	}

	uint16_t sequenceNumber = message.read<PartSequenceNum>();
	etl::array<uint8_t, ECSSMaxFixedOctetStringSize> dataArray{};
	message.readOctetString(dataArray.data());
	etl::span<const uint8_t> DataSpan(dataArray);

	if (!validateSequenceNumber(message, sequenceNumber)) {
		return;
	}

	uint32_t storedCount = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID, &storedCount)) {
		return;
	}

	const uint32_t offset = (ECSSMaxFixedOctetStringSize / (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE - 1U)) *
	                        (storedCount + static_cast<uint32_t>(sequenceNumber));

	const auto resMramWriteFile = MemoryManager::writeToMramFileAtOffset(
	    localFilename.data(), DataSpan, offset);

	if (resMramWriteFile != Memory_Errno::NONE) {
		Services.requestVerification.failAcceptanceVerification(
		    message, getSpacecraftErrorCodeFromMemoryError(resMramWriteFile));
		return;
	}

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &sequenceNumber)) {
		return;
	}

	Services.requestVerification.successCompletionExecutionVerification(message);
}


void LargePacketTransferService::lastUplinkPart(Message& message) {
	LargeMessageTransactionId largeMessageTransactionIdentifier = 0U;

	if (!validateUplinkMessage(message, MessageType::LastUplinkPartReport, largeMessageTransactionIdentifier)) {
		return;
	}

	if (!validateStoredTransactionId(message, largeMessageTransactionIdentifier)) {
		return;
	}

	const uint16_t sequenceNumber = message.read<PartSequenceNum>();
	etl::array<uint8_t, ECSSMaxFixedOctetStringSize> dataArray{};
	message.readOctetString(dataArray.data());
	etl::span<const uint8_t> DataSpan(dataArray);

	if (!validateSequenceNumber(message, sequenceNumber)) {
		return;
	}

	uint32_t storedCount = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID, &storedCount)) {
		return;
	}

	const uint32_t offset = (ECSSMaxFixedOctetStringSize / (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE - 1U)) *
							(storedCount + static_cast<uint32_t>(sequenceNumber));

	const auto resMramWriteFile = MemoryManager::writeToMramFileAtOffset(
		localFilename.data(), DataSpan, offset);

	if (resMramWriteFile != Memory_Errno::NONE) {
		Services.requestVerification.failAcceptanceVerification(
			message, getSpacecraftErrorCodeFromMemoryError(resMramWriteFile));
		return;
	}

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &sequenceNumber)) {
		return;
	}

	uint32_t storedSize = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_UPLINK_SIZE_ID, &storedSize)) {
		return;
	}

	const uint32_t calculatedSize = (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE - 1U) *
	                                (storedCount + static_cast<uint32_t>(sequenceNumber)) *
	                                ECSSMaxFixedOctetStringSize;

	if (storedSize != calculatedSize) {
		// report back - implementation specific
	}

	uint16_t resetSequenceNumber = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &resetSequenceNumber)) {
		return;
	}

	Services.requestVerification.successCompletionExecutionVerification(message);
}



void LargePacketTransferService::split(const Message& message, const LargeMessageTransactionId largeMessageTransactionIdentifier) const {
	uint16_t const size = message.data_size_message_;
	uint16_t positionCounter = 0;
	uint16_t const parts = (size / ECSSMaxFixedOctetStringSize) + 1;
	String<ECSSMaxFixedOctetStringSize> stringPart("");
	etl::array<uint8_t, ECSSMaxFixedOctetStringSize> dataPart = {};

	for (uint16_t i = 0; i < ECSSMaxFixedOctetStringSize; i++) {
		dataPart[i] = message.data[positionCounter];
		positionCounter++;
	}
	stringPart = dataPart.data();
	firstDownlinkPartReport(largeMessageTransactionIdentifier, 0, stringPart);

	for (uint16_t part = 1; part < (parts - 1U); part++) {
		for (uint16_t i = 0; i < ECSSMaxFixedOctetStringSize; i++) {
			dataPart[i] = message.data[positionCounter];
			positionCounter++;
		}
		stringPart = dataPart.data();
		intermediateDownlinkPartReport(largeMessageTransactionIdentifier, part, stringPart);
	}

	for (uint16_t i = 0; i < ECSSMaxFixedOctetStringSize; i++) {
		if (message.data_size_message_ == positionCounter) {
			dataPart[i] = 0; // To prevent from filling the rest of the String with garbage info
		}
		dataPart[i] = message.data[positionCounter];
		positionCounter++;
	}
	stringPart = dataPart.data();
	lastDownlinkPartReport(largeMessageTransactionIdentifier, (parts - 1U), stringPart);
}

bool LargePacketTransferService::validateUplinkMessage(Message& message, const LargePacketTransferService::MessageType expectedType,
                                                       LargeMessageTransactionId& transactionId) {
	if (!message.assertTC(ServiceType, expectedType)) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return false;
	}

	transactionId = message.read<LargeMessageTransactionId>();
	if (!isValidUpLinkIdentifier(static_cast<UplinkLargeMessageTransactionIdentifiers>(transactionId))) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return false;
	}

	return true;
}
bool LargePacketTransferService::validateStoredTransactionId(const Message& message, const LargeMessageTransactionId expectedId) {
	uint16_t storedId = 0xFFFF;
	auto resStoredId = MemoryManager::getParameter(
	    PeakSatParameters::OBDH_LARGE_MESSAGE_TRANSACTION_IDENTIFIER_ID, &storedId);

	if (!resStoredId.has_value()) {
		Services.requestVerification.failAcceptanceVerification(
		    message, getSpacecraftErrorCodeFromMemoryError(resStoredId.error()));
		return false;
	}

	if (storedId != expectedId) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return false;
	}

	return true;
}

bool LargePacketTransferService::validateSequenceNumber(Message& message, const uint16_t currentSequence) {
	uint32_t storedSequenceNum = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID,
	                                         &storedSequenceNum)) {
		return false;
	}

	if (storedSequenceNum + 1 != currentSequence) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return false;
	}

	return true;
}

void LargePacketTransferService::resetTransferParameters() {
	uint32_t reset = 0U;
	PMON_Handlers::raiseMRAMErrorEvent(
	    MemoryManager::setParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID,
	                                static_cast<void*>(&reset)));
}

bool LargePacketTransferService::isValidUpLinkIdentifier(const UplinkLargeMessageTransactionIdentifiers id) {
	bool result = false;

	switch (id) {
		case UplinkLargeMessageTransactionIdentifiers::AtlasMcuFirmware:
			result = true;
			break;
		case UplinkLargeMessageTransactionIdentifiers::AtlasBitStream:
			result = true;
			break;
		case UplinkLargeMessageTransactionIdentifiers::AtlasSoftCpuFirmware:
			result = true;
			break;
		case UplinkLargeMessageTransactionIdentifiers::ScheduledTC:
			result = true;
			break;
		case UplinkLargeMessageTransactionIdentifiers::ObcFirmware:
			result = true;
			break;
		default:
			result = false;
			break;
	}

	return result;
}

void LargePacketTransferService::execute(Message& message) {
	switch (message.messageType) {
		case FirstUplinkPartReport:
			firstUplinkPart(message);
			break;
		case IntermediateUplinkPartReport:
			intermediateUplinkPart(message);
			break;
		case LastUplinkPartReport:
			lastUplinkPart(message);
			break;
		default:
			Services.requestVerification.failAcceptanceVerification(message, GENERIC_ERROR_CAN_INVALID_MESSAGE_ID);
			break;
	}
}

#endif
