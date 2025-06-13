#include "ECSS_Configuration.hpp"
#ifdef SERVICE_LARGEPACKET

#include <LargePacketTransferService.hpp>
#include <etl/String.hpp>

#include "ErrorMaps.hpp"
#include "MemoryManager.hpp"
#include "Message.hpp"
#include "PMON_Handlers.hpp"
#include "ServicePool.hpp"

void LargePacketTransferService::firstDownlinkPartReport(LargeMessageTransactionId largeMessageTransactionIdentifier,
                                                         PartSequenceNum partSequenceNumber,
                                                         const String<ECSSMaxFixedOctetStringSize>& string) {
	Message report = createTM(LargePacketTransferService::MessageType::FirstDownlinkPartReport);
	report.append<LargeMessageTransactionId>(largeMessageTransactionIdentifier); // large message transaction identifier
	report.append<PartSequenceNum>(partSequenceNumber);                          // part sequence number
	report.appendOctetString(string);                                            // fixed octet-string
	storeMessage(report, report.data_size_message_);
}

void LargePacketTransferService::intermediateDownlinkPartReport(
    LargeMessageTransactionId largeMessageTransactionIdentifier, PartSequenceNum partSequenceNumber,
    const String<ECSSMaxFixedOctetStringSize>& string) {

	Message report = createTM(LargePacketTransferService::MessageType::IntermediateDownlinkPartReport);
	report.append<LargeMessageTransactionId>(largeMessageTransactionIdentifier); // large message transaction identifier
	report.append<PartSequenceNum>(partSequenceNumber);                          // part sequence number
	report.appendOctetString(string);                                            // fixed octet-string
	storeMessage(report, report.data_size_message_);
}

void LargePacketTransferService::lastDownlinkPartReport(LargeMessageTransactionId largeMessageTransactionIdentifier,
                                                        PartSequenceNum partSequenceNumber,
                                                        const String<ECSSMaxFixedOctetStringSize>& string) {
	Message report = createTM(LargePacketTransferService::MessageType::LastDownlinkPartReport);
	report.append<LargeMessageTransactionId>(largeMessageTransactionIdentifier); // large message transaction identifier
	report.append<PartSequenceNum>(partSequenceNumber);                          // part sequence number
	report.appendOctetString(string);                                            // fixed octet-string
	storeMessage(report, report.data_size_message_);
}


void LargePacketTransferService::firstUplinkPart(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::FirstUplinkPartReport)) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	LargeMessageTransactionId largeMessageTransactionIdentifier = message.read<LargeMessageTransactionId>();
	if (!isValidUpLinkIdentifier(static_cast<UplinkLargeMessageTransactionIdentifiers>(largeMessageTransactionIdentifier))) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	if (auto resMem = MemoryManager::setParameter(PeakSatParameters::OBDH_LARGE_MESSAGE_TRANSACTION_IDENTIFIER_ID, static_cast<void*>(&largeMessageTransactionIdentifier));
	    not resMem.has_value()) {
		//abort
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resMem.error()));
		return;
	}
	PartSequenceNum partSequenceNumber = message.read<PartSequenceNum>();
	if (partSequenceNumber != 0U) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	const auto dataString = message.read<String<ECSSMaxFixedOctetStringSize>>();
	etl::string<MemoryFilesystem::MAX_FILENAME> filename_sized;
	filename_sized.assign(dataString.begin(),
	                      dataString.begin() + std::min(static_cast<size_t>(MemoryFilesystem::MAX_FILENAME),
	                                                    dataString.size()));
	auto size = static_cast<uint32_t>(dataString[10]) | // Least significant byte
	            (static_cast<uint32_t>(dataString[11]) << 8) |
	            (static_cast<uint32_t>(dataString[12]) << 16) |
	            (static_cast<uint32_t>(dataString[13]) << 24); // Most significant byte

	PMON_Handlers::raiseMRAMErrorEvent(MemoryManager::setParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_UPLINK_SIZE_ID, &size));

	if (const auto validateFile = MemoryManagerHelpers::getFileTransferIdFromFilename(filename_sized.data()); validateFile != largeMessageTransactionIdentifier) {
		uint32_t reset = 0U;
		PMON_Handlers::raiseMRAMErrorEvent(MemoryManager::setParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID, static_cast<void*>(&reset)));
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	if (filename_sized.size() >= MemoryFilesystem::MAX_FILENAME) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	if (auto resStoreCount = MemoryManager::setParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID, &partSequenceNumber); not resStoreCount.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resStoreCount.error()));
	}
	localFilename.clear();
	localFilename.assign(filename_sized.begin(), filename_sized.end());
	// todo add count to secquence
	// only use write to MRAM with offset

	//send ack
	Services.requestVerification.successAcceptanceVerification(message);
	//todo start timer
}


void LargePacketTransferService::intermediateUplinkPart(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::IntermediateUplinkPartReport)) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	LargeMessageTransactionId largeMessageTransactionIdentifier = message.read<LargeMessageTransactionId>();
	if (!isValidUpLinkIdentifier(static_cast<UplinkLargeMessageTransactionIdentifiers>(largeMessageTransactionIdentifier))) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	uint16_t storedId = 0xFFFF;
	if (auto resStoredId = MemoryManager::getParameter(PeakSatParameters::OBDH_LARGE_MESSAGE_TRANSACTION_IDENTIFIER_ID, &storedId);
	    not resStoredId.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resStoredId.error()));
		return;
	}
	if (storedId != largeMessageTransactionIdentifier) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	uint16_t sequenceNumber = message.read<PartSequenceNum>();
	const auto dataString = message.read<String<ECSSMaxFixedOctetStringSize>>();
	etl::span<const uint8_t> DataSpan(dataString.begin(), dataString.end());

	uint32_t storedSequenceNum = 0U;
	if (auto resCount = MemoryManager::getParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &storedSequenceNum); not resCount.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resCount.error()));
		return;
	}

	uint32_t storedCount = 0U;
	if (auto resCount = MemoryManager::getParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID, &storedCount); not resCount.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resCount.error()));
		return;
	}
	if (storedSequenceNum + 1 != sequenceNumber) {
		//abort
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	const auto resMramWriteFile = MemoryManager::writeToMramFileAtOffset(localFilename.data(),
	                                                                     DataSpan,
	                                                                     ECSSMaxFixedOctetStringSize / (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE - 1) * (storedCount + sequenceNumber));
	if (resMramWriteFile != Memory_Errno::NONE) {
		//abort

		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resMramWriteFile));
		return;
	}
	if (auto resCount = MemoryManager::setParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &sequenceNumber); not resCount.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resCount.error()));
		return;
	}

	Services.requestVerification.successCompletionExecutionVerification(message);
}


void LargePacketTransferService::lastUplinkPart(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::LastUplinkPartReport)) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	LargeMessageTransactionId largeMessageTransactionIdentifier = message.read<LargeMessageTransactionId>();
	if (!isValidUpLinkIdentifier(static_cast<UplinkLargeMessageTransactionIdentifiers>(largeMessageTransactionIdentifier))) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}
	uint16_t storedId = 0xFFFF;
	if (auto resStoredId = MemoryManager::getParameter(PeakSatParameters::OBDH_LARGE_MESSAGE_TRANSACTION_IDENTIFIER_ID, &storedId);
	    not resStoredId.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resStoredId.error()));
		return;
	}
	if (storedId != largeMessageTransactionIdentifier) {
		Services.requestVerification.failAcceptanceVerification(message, SpacecraftErrorCode::OBDH_ERROR_INVALID_ARGUMENT);
		return;
	}

	const uint16_t sequenceNumber = message.read<PartSequenceNum>();
	const auto dataString = message.read<String<ECSSMaxFixedOctetStringSize>>();

	uint32_t storedCount = 0U;
	if (auto resCount = MemoryManager::getParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_COUNT_ID, &storedCount); not resCount.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resCount.error()));
		return;
	}
	uint32_t storedSize = 0U;
	if (auto resSize = MemoryManager::getParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_UPLINK_SIZE_ID, &storedSize); not resSize.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resSize.error()));
		return;
	}
	if (storedSize != (MemoryFilesystem::MRAM_DATA_BLOCK_SIZE - 1) * (storedCount + sequenceNumber) * ECSSMaxFixedOctetStringSize) {
		// report back
	}
	uint16_t resetSequenceNumber = 0U;
	if (auto resResetSequenceNumber = MemoryManager::getParameter(PeakSatParameters::OBDH_LARGE_FILE_TRANFER_SEQUENCE_NUM_ID, &resetSequenceNumber); not resResetSequenceNumber.has_value()) {
		Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromMemoryError(resResetSequenceNumber.error()));
		return;
	}
	Services.requestVerification.successCompletionExecutionVerification(message);
}

void LargePacketTransferService::split(const Message& message, LargeMessageTransactionId largeMessageTransactionIdentifier) {
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

bool LargePacketTransferService::validateUplinkMessage(Message& message, LargePacketTransferService::MessageType expectedType,
							 LargeMessageTransactionId& transactionId) {
	if (!message.assertTC(message.serviceType, expectedType)) {
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

bool LargePacketTransferService::validateSequenceNumber(Message& message, uint16_t currentSequence) {
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
