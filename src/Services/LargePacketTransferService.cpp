#include "ECSS_Configuration.hpp"
#ifdef SERVICE_LARGEPACKET

#include <LargePacketTransferService.hpp>
#include <etl/String.hpp>

#include "ErrorMaps.hpp"
#include "HelperFunctions.hpp"
#include "Message.hpp"
#include "PMON_Handlers.hpp"

#include "ServicePool.hpp"
#include "internal_flash_driver.hpp"

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

void LargePacketTransferService::uplinkAbortionReport(LargeMessageTransactionId largeMessageTransactionIdentifier, const SpacecraftErrorCode abortionReason) const {

	Message report = createTM(LargePacketTransferService::MessageType::UplinkAborted);

	// Append large message transaction identifier
	report.append<LargeMessageTransactionId>(largeMessageTransactionIdentifier);

	// Append failure reason using SpacecraftErrorCode
	report.append<uint16_t>(static_cast<uint16_t>(abortionReason));

	storeMessage(report, report.data_size_message_);
}


void LargePacketTransferService::firstUplinkPart(Message& message) {
	LargeMessageTransactionId largeMessageTransactionIdentifier = message.read<LargeMessageTransactionId>();

	if (!validateUplinkMessage(message, MessageType::FirstUplinkPartReport, largeMessageTransactionIdentifier)) {
		return;
	}

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_MESSAGE_TRANSACTION_IDENTIFIER_ID,
	                        static_cast<void*>(&largeMessageTransactionIdentifier))) {
		return;
	}

	PartSequenceNum partSequenceNumber = message.read<PartSequenceNum>();

	if (not setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID,
	                           static_cast<void*>(&partSequenceNumber))) {
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
	const auto validateFile = static_cast<uint16_t>(MemoryManagerHelpers::getFileTransferIdFromFilename(filename_sized.data()));
	if (validateFile != largeMessageTransactionIdentifier) {
		resetTransferParameters();
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	etl::copy_n(filename_sized.begin(), filename_sized.size(), localFilename.begin());

	// TODO: start timer
	LOG_DEBUG << "[LTF] sequence number: " << partSequenceNumber;

	Services.requestVerification.successAcceptanceVerification(message);
}


void LargePacketTransferService::intermediateUplinkPart(Message& message) {
	if (isFlashFile()) {
		handleFlashLastPart(message);
		return;
	}
	LargeMessageTransactionId largeMessageTransactionIdentifier = message.read<LargeMessageTransactionId>();

	if (!validateUplinkMessage(message, MessageType::IntermediateUplinkPartReport, largeMessageTransactionIdentifier)) {
		// Services.requestVerification.failProgressExecutionVerification(message, OBDH_ERROR_INVALID_ARGUMENT, sequenceNumber);
		return;
	}

	if (!validateStoredTransactionId(message, largeMessageTransactionIdentifier)) {
		return;
	}

	uint16_t sequenceNumber = message.read<PartSequenceNum>();
	LOG_DEBUG << "[LTF] sequence number got: " << sequenceNumber;
	if (!validateSequenceNumber(message, sequenceNumber)) { // will store sequence number at the correct offset even if out of order
		                                                    // Services.requestVerification.failProgressExecutionVerification(message, OBDH_ERROR_INVALID_ARGUMENT, sequenceNumber);
		                                                    // return;
	}

	// Validate remaining data
	if (message.readPosition + ECSSMaxFixedOctetStringSize != message.data_size_ecss_) { // Intermediate parts must have EXACTLY 127 bytes
		Services.requestVerification.failProgressExecutionVerification(message, OBDH_ERROR_INVALID_ARGUMENT, sequenceNumber);
		return;
	}

	// safely create the span
	etl::span<const uint8_t> DataSpan(message.data.begin() + message.readPosition, ECSSMaxFixedOctetStringSize);

	const uint32_t offset = (ECSSMaxFixedOctetStringSize / (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE - 1U)) *
	                        (static_cast<uint32_t>(sequenceNumber));

	const auto resMramWriteFile = MemoryManager::writeToMramFileAtOffset(
	    localFilename.data(), DataSpan, offset);

	if (resMramWriteFile != Memory_Errno::NONE) {
		Services.requestVerification.failProgressExecutionVerification(message, getSpacecraftErrorCodeFromMemoryError(resMramWriteFile), sequenceNumber);
		return;
	}

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &sequenceNumber)) {
		Services.requestVerification.failProgressExecutionVerification(message, OBDH_ERROR_MEMORY_UNKNOWN, sequenceNumber);
		return;
	}

	LOG_DEBUG << "[LTF] sequence number success: " << sequenceNumber;

	Services.requestVerification.successProgressExecutionVerification(message, sequenceNumber);
}


void LargePacketTransferService::lastUplinkPart(Message& message) {
	if (isFlashFile()) {
		handleFlashLastPart(message);
		return;
	}
	LargeMessageTransactionId largeMessageTransactionIdentifier = message.read<LargeMessageTransactionId>();


	if (!validateUplinkMessage(message, MessageType::LastUplinkPartReport, largeMessageTransactionIdentifier)) {
		return;
	}

	if (!validateStoredTransactionId(message, largeMessageTransactionIdentifier)) {
		return;
	}
	uint16_t sequenceNumber = message.read<PartSequenceNum>();
	LOG_DEBUG << "[LTF] sequence number got: " << sequenceNumber;
	if (!validateSequenceNumber(message, sequenceNumber)) {
		return;
	}

	// Validate remaining data
	if (message.readPosition + ECSSMaxFixedOctetStringSize < message.data_size_ecss_) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	// safely create the span
	etl::span<const uint8_t> DataSpan(message.data.begin() + message.readPosition, message.data_size_ecss_ - message.readPosition);

	const uint32_t offset = (ECSSMaxFixedOctetStringSize / (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE - 1U)) *
	                        (static_cast<uint32_t>(sequenceNumber));

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

	const uint32_t calculatedSize = ECSSMaxFixedOctetStringSize * (static_cast<uint32_t>(sequenceNumber));
	if (storedSize != calculatedSize) {
		// report back - implementation specific
	}

	LOG_DEBUG << "[LTF] sequence number: " << sequenceNumber;

	Services.requestVerification.successCompletionExecutionVerification(message);
}

bool LargePacketTransferService::validateUplinkMessage(Message& message, const LargePacketTransferService::MessageType expectedType,
                                                       LargeMessageTransactionId& transactionId) {
	if (!message.assertTC(ServiceType, expectedType)) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return false;
	}

	if (!isValidUpLinkIdentifier(static_cast<UplinkLargeMessageTransactionIdentifiers>(transactionId))) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return false;
	}

	return true;
}
bool LargePacketTransferService::validateStoredTransactionId(const Message& message, const LargeMessageTransactionId expectedId) {
	LargeMessageTransactionId storedId = 0xFF;
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

bool LargePacketTransferService::validateSequenceNumber(Message& message, uint16_t currentSequence) {
	uint32_t storedSequenceNum = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID,
	                        &storedSequenceNum)) {
		return false;
	}
	if (currentSequence == 0 && storedSequenceNum == 0) {
		// First data packet is allowed to be 0
		return true;
	}
	if (storedSequenceNum + 1 != currentSequence) {
		uint16_t discontinuity_counter = 0;
		MemoryManager::getParameter(PeakSatParameters::OBDH_LFT_DISCONTINUITY_COUNTER_ID, &discontinuity_counter);
		discontinuity_counter++;
		MemoryManager::setParameter(PeakSatParameters::OBDH_LFT_DISCONTINUITY_COUNTER_ID, &discontinuity_counter);

		MemoryManager::setParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &currentSequence);

		// Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
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

void LargePacketTransferService::handleFlashIntermediateParts(Message& message) {
	if (!message.assertTC(ServiceType, IntermediateUplinkPartReport)) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	LargeMessageTransactionId largeMessageTransactionIdentifier = message.read<LargeMessageTransactionId>();
	if (largeMessageTransactionIdentifier != static_cast<UplinkLargeMessageTransactionIdentifiers_t>(UplinkLargeMessageTransactionIdentifiers::ObcFirmware)) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	uint16_t sequenceNumber = message.read<PartSequenceNum>();
	LOG_DEBUG << "[LTF] sequence number got: " << sequenceNumber;
	uint32_t storedSequenceNum = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID,
	                        &storedSequenceNum)) {
		return;
	}
	if (sequenceNumber == 0 && storedSequenceNum == 0) {
		// First data packet is allowed to be 0
	} else if (storedSequenceNum + 1 != sequenceNumber) {
		Services.requestVerification.failProgressExecutionVerification(message, OBDH_ERROR_INVALID_ARGUMENT, sequenceNumber);
		return;
	}

	const uint16_t partIndex = sequenceNumber % PARTS_PER_FLASH_PAGE;
	;

	if (partIndex > (PARTS_PER_FLASH_PAGE - 1)) {
		return; // expecting 4 parts per page
	}

	if (partIndex == 0) {
		localFlashPageBytes.fill(0xFFFFFFFF); // clear buffer on new batch
	}

	// Validate remaining data
	if (message.readPosition + I_FLASH_PART_EXPECTED_BYTES != message.data_size_ecss_) { // Intermediate parts must have EXACTLY 128 bytes
		Services.requestVerification.failProgressExecutionVerification(message, OBDH_ERROR_INVALID_ARGUMENT, sequenceNumber);
		return;
	}

	// Create span for the incoming 128 bytes
	const etl::span<const uint8_t> incomingData(message.data.data() + message.readPosition, I_FLASH_PART_EXPECTED_BYTES);

	// Calculate starting position in uint32_t array
	const uint32_t baseOffset = partIndex * UINT32_PER_PART;
	;

	for (uint32_t i = 0; i < UINT32_PER_PART; i++) {
		const size_t byteIndex = i * BYTES_PER_UINT32;

		// Pack 4 bytes into big-endian uint32_t
		localFlashPageBytes[baseOffset + i] =
		    (static_cast<uint32_t>(incomingData[byteIndex + 0]) << (3 * BITS_PER_BYTE)) |
		    (static_cast<uint32_t>(incomingData[byteIndex + 1]) << (2 * BITS_PER_BYTE)) |
		    (static_cast<uint32_t>(incomingData[byteIndex + 2]) << (1 * BITS_PER_BYTE)) |
		    (static_cast<uint32_t>(incomingData[byteIndex + 3]) << (0 * BITS_PER_BYTE));
	}

	// Write to flash after receiving the last part of the page
	if (partIndex == (PARTS_PER_FLASH_PAGE - 1)) {
		const uint16_t logicalPage = sequenceNumber / PARTS_PER_FLASH_PAGE;
		const uint16_t absolutePage = FLASH_IOP_FLAGS_PAGE + logicalPage;

		const auto res = flash_write_page(absolutePage,
										   localFlashPageBytes.data(),
										   I_FLASH_PAGE_SIZE_U32);
		if (res != Memory_Errno::NONE) {
			Services.requestVerification.failProgressExecutionVerification(
				message, getSpacecraftErrorCodeFromMemoryError(res), sequenceNumber);
			return;
		}
	}

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &sequenceNumber)) {
		Services.requestVerification.failProgressExecutionVerification(message, OBDH_ERROR_MEMORY_UNKNOWN, sequenceNumber);
		return;
	}

	LOG_DEBUG << "[LTF] sequence number success: " << sequenceNumber;

	Services.requestVerification.successProgressExecutionVerification(message, sequenceNumber);
}

void LargePacketTransferService::handleFlashLastPart(Message& message) {
	if (!message.assertTC(ServiceType, LastUplinkPartReport)) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	LargeMessageTransactionId largeMessageTransactionIdentifier = message.read<LargeMessageTransactionId>();
	if (largeMessageTransactionIdentifier != static_cast<UplinkLargeMessageTransactionIdentifiers_t>(UplinkLargeMessageTransactionIdentifiers::ObcFirmware)) {
		Services.requestVerification.failAcceptanceVerification(
		    message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	uint16_t sequenceNumber = message.read<PartSequenceNum>();
	LOG_DEBUG << "[LTF] Last part sequence number: " << sequenceNumber;

	uint32_t storedSequenceNum = 0U;
	if (!getMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID,
	                        &storedSequenceNum)) {
		return;
	}

	if (storedSequenceNum + 1 != sequenceNumber) {
		Services.requestVerification.failCompletionExecutionVerification(message, OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	const uint8_t partIndex = sequenceNumber % PARTS_PER_FLASH_PAGE;
	const uint16_t remainingBytes = message.data_size_ecss_ - message.readPosition;

	// Validate we have some data
	if (remainingBytes == 0 || remainingBytes > I_FLASH_PART_EXPECTED_BYTES) {
		Services.requestVerification.failCompletionExecutionVerification(message, OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	// Create span for the incoming data
	const etl::span<const uint8_t> incomingData(message.data.data() + message.readPosition, remainingBytes);

	// Calculate starting position in uint32_t array
	const uint32_t baseOffset = partIndex * UINT32_PER_PART;

	// Calculate how many complete uint32_t we can pack
	const uint32_t completeUint32Count = remainingBytes / BYTES_PER_UINT32;

	// Pack complete uint32_t values
	for (uint32_t i = 0; i < completeUint32Count; i++) {
		const size_t byteIndex = i * BYTES_PER_UINT32;

		localFlashPageBytes[baseOffset + i] =
		    (static_cast<uint32_t>(incomingData[byteIndex + 0]) << (3 * BITS_PER_BYTE)) |
		    (static_cast<uint32_t>(incomingData[byteIndex + 1]) << (2 * BITS_PER_BYTE)) |
		    (static_cast<uint32_t>(incomingData[byteIndex + 2]) << (1 * BITS_PER_BYTE)) |
		    (static_cast<uint32_t>(incomingData[byteIndex + 3]) << (0 * BITS_PER_BYTE));
	}

	// Handle remaining bytes (if not multiple of 4)
	const uint32_t remainingPartialBytes = remainingBytes % BYTES_PER_UINT32;
	if (remainingPartialBytes > 0) {
		uint32_t partialValue = 0xFFFFFFFF;
		const size_t baseByteIndex = completeUint32Count * BYTES_PER_UINT32;

		for (uint32_t b = 0; b < remainingPartialBytes; b++) {
			const uint8_t shiftAmount = (3 - b) * BITS_PER_BYTE;
			partialValue &= ~(0xFFU << shiftAmount);
			partialValue |= (static_cast<uint32_t>(incomingData[baseByteIndex + b]) << shiftAmount);
		}

		localFlashPageBytes[baseOffset + completeUint32Count] = partialValue;
	}

	// Always write the flash page for last part
	const uint16_t flashPageNumber = sequenceNumber / PARTS_PER_FLASH_PAGE;
	const uint16_t logicalPage = sequenceNumber / PARTS_PER_FLASH_PAGE;
	const uint16_t absolutePage = FLASH_IOP_FLAGS_PAGE + logicalPage;

	const auto res = flash_write_page(absolutePage,
									   localFlashPageBytes.data(),
									   I_FLASH_PAGE_SIZE_U32);

	if (res != Memory_Errno::NONE) {
		Services.requestVerification.failProgressExecutionVerification(message, getSpacecraftErrorCodeFromMemoryError(res), sequenceNumber);
		return;
	}
	LOG_DEBUG << "[LTF] Written final flash page: " << flashPageNumber;

	if (!setMemoryParameter(message, PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &sequenceNumber)) {
		Services.requestVerification.failCompletionExecutionVerification(message, OBDH_ERROR_MEMORY_UNKNOWN);
		return;
	}

	Services.requestVerification.successCompletionExecutionVerification(message);
}

#endif
