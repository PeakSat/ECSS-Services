#include "ECSS_Configuration.hpp"
#ifdef SERVICE_LARGEPACKET

#include <LargePacketTransferService.hpp>
#include <etl/String.hpp>

#include "ErrorMaps.hpp"
#include "HelperFunctions.hpp"
#include "Message.hpp"
#include "PMON_Handlers.hpp"

#include "ServicePool.hpp"

static_assert(ECSSMaxFixedOctetStringSize % (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE - 1) == 0, "ECSSMaxFixedOctetStringSize must be a multiple of MRAM_DATA_BLOCK_SIZE");

template <typename T>
bool LargePacketTransferService::getMemoryParameter(Message& message, ParameterId paramId, T* value) {
	auto result = MemoryManager::getParameter(paramId, static_cast<void*>(value));
	if (!result.has_value()) {
		Services.requestVerification.failAcceptanceVerification(
		    message, getSpacecraftErrorCodeFromMemoryError(result.error()));
		return false;
	}
	return true;
}

template <typename T>
bool LargePacketTransferService::setMemoryParameter(Message& message, ParameterId paramId, T* value) {
	auto result = MemoryManager::setParameter(paramId, static_cast<void*>(value));
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

	// Validate we have enough data remaining for the payload
	constexpr size_t REQUIRED_SIZE = 14U; // filename (10 bytes) + size (4 bytes)
	if (message.readPosition + REQUIRED_SIZE > message.data_size_ecss_) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	// Create a span for the payload data
	etl::span<const uint8_t> payloadSpan(message.data.begin() + message.readPosition, REQUIRED_SIZE);

	// Extract filename using ETL
	etl::array<char, MemoryFilesystem::MAX_FILENAME> filename_sized{};
	constexpr size_t FILENAME_SIZE = 10U;
	constexpr size_t copySize = etl::min(FILENAME_SIZE, filename_sized.size());

	etl::copy_n(message.data.begin() + message.readPosition, copySize, filename_sized.begin());
	message.readPosition += FILENAME_SIZE;

	// Extract size using ETL byte operations (big-endian)
	uint32_t size = (static_cast<uint32_t>(payloadSpan[FILENAME_SIZE]) << 24) |
	                (static_cast<uint32_t>(payloadSpan[FILENAME_SIZE + 1]) << 16) |
	                (static_cast<uint32_t>(payloadSpan[FILENAME_SIZE + 2]) << 8) |
	                (static_cast<uint32_t>(payloadSpan[FILENAME_SIZE + 3]));
	message.readPosition += 4;

	// Store the file size
	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_UPLINK_SIZE_ID, &size)) {
		return;
	}

	// Validate filename matches transaction ID
	auto validateFile = static_cast<uint16_t>(MemoryManagerHelpers::getFileTransferIdFromFilename(filename_sized.data()));
	if (validateFile != largeMessageTransactionIdentifier) {
		resetTransferParameters();
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	// Store part sequence number
	uint32_t reset = 0U;
	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID, &reset)) {
		return;
	}

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID,
	                        &reset)) {
		return;
	}

	etl::copy_n(filename_sized.begin(), filename_sized.size(), localFilename.begin());

	// TODO: start timer
	Services.requestVerification.successAcceptanceVerification(message);
}


void LargePacketTransferService::intermediateUplinkPart(Message& message) {
	LargeMessageTransactionId largeMessageTransactionIdentifier = 0U;

	if (!validateUplinkMessage(message, MessageType::IntermediateUplinkPartReport, largeMessageTransactionIdentifier)) {
		return;
	}
	uint16_t sequenceNumber = message.read<PartSequenceNum>() + 1;
	LOG_DEBUG << " ------> sequenceNumber: " << sequenceNumber;

	if (!validateStoredTransactionId(message, largeMessageTransactionIdentifier)) {
		return;
	}

	// Validate remaining data
	if (message.readPosition + ECSSMaxFixedOctetStringSize > message.data_size_ecss_) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	// safely create the span
	etl::span<const uint8_t> DataSpan(message.data.begin() + message.readPosition, ECSSMaxFixedOctetStringSize);
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


	Services.requestVerification.successProgressExecutionVerification(message, sequenceNumber);
}


void LargePacketTransferService::lastUplinkPart(Message& message) {
	LargeMessageTransactionId largeMessageTransactionIdentifier = 0U;

	if (!validateUplinkMessage(message, MessageType::LastUplinkPartReport, largeMessageTransactionIdentifier)) {
		return;
	}

	if (!validateStoredTransactionId(message, largeMessageTransactionIdentifier)) {
		return;
	}

	uint16_t sequenceNumber = message.read<PartSequenceNum>();
	// Validate remaining data
	// if (message.readPosition + ECSSMaxFixedOctetStringSize < message.data_size_ecss_) {
	// 	Services.requestVerification.failAcceptanceVerification(
	// 	    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
	// 	return;
	// }

	// safely create the span
	etl::span<const uint8_t> DataSpan(message.data.begin() + message.readPosition, message.data_size_ecss_ - message.readPosition);


	if (!validateSequenceNumber(message, sequenceNumber)) {
		// return;
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

	LOG_DEBUG<<"{{{{{ STORED SIZE: "<<storedSize;
	LOG_DEBUG<<"{{{{{ Calculated Size: "<<calculatedSize;

	uint16_t resetSequenceNumber = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &resetSequenceNumber)) {
		return;
	}

	Services.requestVerification.successCompletionExecutionVerification(message);
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
	LOG_DEBUG << "----> stored" << storedSequenceNum + 1;

	if (storedSequenceNum + 1 != currentSequence) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		// return false;
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
