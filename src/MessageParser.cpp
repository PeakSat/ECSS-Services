#include "MessageParser.hpp"
#include <ServicePool.hpp>
#include "CRCHelper.hpp"
#include "ErrorHandler.hpp"
#include "RequestVerificationService.hpp"
#include "macros.hpp"

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers) The reason is that we do a lot of bit shifts
static_assert(sizeof(ServiceTypeNum) == 1);

static_assert(sizeof(MessageTypeNum) == 1);

void MessageParser::execute(Message& message) { //cppcheck-suppress[constParameter,constParameterReference]
	switch (message.serviceType) {

#ifdef SERVICE_HOUSEKEEPING
		case HousekeepingService::ServiceType:
			Services.housekeeping.execute(message);
			break;
#endif

#ifdef SERVICE_PARAMETERSTATISTICS
		case ParameterStatisticsService::ServiceType:
			Services.parameterStatistics.execute(message);
			break;
#endif

#ifdef SERVICE_EVENTREPORT
		case EventReportService::ServiceType:
			Services.eventReport.execute(message);
			break;
#endif

#ifdef SERVICE_MEMORY
		case MemoryManagementService::ServiceType:
			Services.memoryManagement.execute(message);
			break;
#endif

#ifdef SERVICE_FUNCTION
		case FunctionManagementService::ServiceType:
			Services.functionManagement.execute(message);
			break;
#endif

#ifdef SERVICE_TIMESCHEDULING
		case TimeBasedSchedulingService::ServiceType:
			Services.timeBasedScheduling.execute(message);
			break;
#endif

#ifdef SERVICE_STORAGEANDRETRIEVAL
		case StorageAndRetrievalService::ServiceType:
			Services.storageAndRetrieval.execute(message);
#endif

#ifdef SERVICE_ONBOARDMONITORING
		case OnBoardMonitoringService::ServiceType:
			Services.onBoardMonitoringService.execute(message);
			break;
#endif

#ifdef SERVICE_TEST
		case TestService::ServiceType:
			Services.testService.execute(message);
			break;
#endif

#ifdef SERVICE_EVENTACTION
		case EventActionService::ServiceType:
			Services.eventAction.execute(message);
			break;
#endif

#ifdef SERVICE_PARAMETER
		case ParameterService::ServiceType:
			Services.parameterManagement.execute(message);
			break;
#endif

#ifdef SERVICE_REALTIMEFORWARDINGCONTROL
		case RealTimeForwardingControlService::ServiceType:
			Services.realTimeForwarding.execute(message);
			break;
#endif

#ifdef SERVICE_FILE_MANAGEMENT
		case FileManagementService::ServiceType:
			Services.fileManagement.execute(message); // ST[23]
			break;
#endif
		case LargePacketTransferService::ServiceType:
			Services.largePacketTransferService.execute(message);
			break;
		default:
			ErrorHandler::reportInternalError(ErrorHandler::OtherMessageType);
	}
}

SpacecraftErrorCode MessageParser::parse(const uint8_t* data, uint32_t length, Message& message, bool error_reporting_active, bool parse_ccsds) {
	if (data == nullptr || length < CCSDSPrimaryHeaderSize) {
		return OBDH_ERROR_MESSAGE_PARSER_PARSE_LENGTH_LESS_THAN_EXPECTED;
	}
	if (parse_ccsds == false) {
		return OBDH_ERROR_MESSAGE_PARSER_PARSE_WRONG_PUS_VERSION;
	}

	uint16_t const packetHeaderIdentification = (data[0] << 8) | data[1];
	uint16_t const packetSequenceControl = (data[2] << 8) | data[3];
	uint16_t const packetCCSDSDataLength = (data[4] << 8) | data[5];

	uint8_t const versionNumber = data[0] >> 5;
	Message::PacketType const packet_type = ((data[0] & 0x10) == 0) ? Message::TM : Message::TC;
	bool const secondaryHeaderFlag = (data[0] & 0x08U) != 0U;
	ApplicationProcessId const APID = packetHeaderIdentification & static_cast<ApplicationProcessId>(0x07ff);
	const auto sequenceFlags = static_cast<uint8_t>(packetSequenceControl >> 14);
	SequenceCount const packetSequenceCount = packetSequenceControl & (~0xc000U); // keep last 14 bits

	message = Message(0, 0, packet_type, APID);

	if ((packet_type == Message::TM) && (length < ECSSSecondaryTMHeaderSize)) {
		return OBDH_ERROR_MESSAGE_PARSER_TM_SIZE_LESS_THAN_EXPECTED;
	}
	if ((packet_type == Message::TC) && (length < ECSSSecondaryTCHeaderSize)) {
		return OBDH_ERROR_MESSAGE_PARSER_TC_SIZE_LESS_THAN_EXPECTED;
	}

	if (error_reporting_active) {
		if (versionNumber != 0U)
			return OBDH_ERROR_MESSAGE_PARSER_PARSE_WRONG_PUS_VERSION;
		if (!secondaryHeaderFlag)
			return OBDH_ERROR_MESSAGE_PARSER_PARSE_SECONDARY_HEADER;
		if (sequenceFlags != 0x3U)
			return OBDH_ERROR_MESSAGE_PARSER_PARSE_SEQUENCE_FLAGS;
	}

	message.packet_sequence_count_ = packetSequenceCount;

	if (packetCCSDSDataLength > ECSSMaxMessageSize)
		return OBDH_ERROR_MESSAGE_PARSER_TC_SIZE_LARGER_THAN_EXPECTED;
	//
	message.total_size_ccsds_ = length;
	message.total_size_ecss_ = message.total_size_ccsds_ - CCSDSPrimaryHeaderSize;
	//
	if (message.packet_type_ == Message::TM) {
		message.data_size_ecss_ = message.total_size_ecss_ - ECSSSecondaryTMHeaderSize;
	}
	if (message.packet_type_ == Message::TC) {
		message.data_size_ecss_ = message.total_size_ecss_ - ECSSSecondaryTCHeaderSize;
	}

	message.data_size_message_ = message.total_size_ecss_;

	etl::array<uint8_t, ECSSMaxMessageSize> ecss_data{};

	// Validate bounds before copying
	if (CCSDSPrimaryHeaderSize + message.total_size_ecss_ > length) {
		return OBDH_ERROR_MESSAGE_PARSER_PARSE_LENGTH_LESS_THAN_EXPECTED;
	}
	if (message.total_size_ecss_ > ECSSMaxMessageSize) {
		return OBDH_ERROR_MESSAGE_PARSER_DATA_TOO_LARGE;
	}
	etl::copy_n(&data[CCSDSPrimaryHeaderSize],
	            message.total_size_ecss_,
	            ecss_data.data());

	if (packet_type == Message::TC) {
		return parseECSSTCHeader(ecss_data.data(), message);
	}
	return parseECSSTMHeader(ecss_data.data(), message.total_size_ecss_, message);
}


SpacecraftErrorCode MessageParser::parseECSSTCHeader(const uint8_t* data, Message& message) {
	// sanity check
	if (message.total_size_ecss_ > ECSSMaxMessageSize)
		return OBDH_ERROR_MESSAGE_PARSER_TC_SIZE_LARGER_THAN_EXPECTED;

	if (message.total_size_ecss_ < ECSSSecondaryTCHeaderSize) {
		return OBDH_ERROR_MESSAGE_PARSER_TC_SIZE_LESS_THAN_EXPECTED;
	}
	// Individual fields of the TC header
	uint8_t const pusVersion = data[0] >> 4;
	ServiceTypeNum const serviceType = data[1];
	MessageTypeNum const messageType = data[2];
	SourceId const sourceId = (data[3] << 8) + data[4];

	if (pusVersion != 2U)
		return OBDH_ERROR_MESSAGE_PARSER_PARSE_WRONG_PUS_VERSION;

	// Remove the length of the header
	message.data_size_ecss_ = message.total_size_ecss_ - ECSSSecondaryTCHeaderSize;

	// Copy the data to the message
	message.serviceType = serviceType;
	message.messageType = messageType;
	message.source_ID_ = sourceId;
	etl::copy_n(data + ECSSSecondaryTCHeaderSize, message.data_size_ecss_, message.data.begin());

	return GENERIC_ERROR_NONE;
}


SpacecraftErrorCode MessageParser::parseECSSTC(String<ECSSTCRequestStringSize> data, Message& message) {
	const auto* dataInt = reinterpret_cast<const uint8_t*>(data.data());
	return parseECSSTCHeader(dataInt, message);
}

SpacecraftErrorCode MessageParser::parseECSSTC(const uint8_t* data, Message& message) {
	return parseECSSTCHeader(data, message);
}

etl::expected<String<CCSDSMaxMessageSize>, SpacecraftErrorCode> MessageParser::composeECSS(Message& message, uint16_t ecss_total_size) {
	// We will create an array with the maximum size.
	etl::array<uint8_t, ECSSSecondaryTMHeaderSize> header = {};

	if (message.packet_type_ == Message::TC) {
		header[0] = ECSSPUSVersion << 4U; // Assign the pusVersion = 2
		header[0] |= 0x00;                // Ack flags
		header[1] = message.serviceType;
		header[2] = message.messageType;
		header[3] = message.application_ID_ >> 8U;
		header[4] = message.application_ID_;
	} else {
		header[0] = ECSSPUSVersion << 4U; // Assign the pusVersion = 2
		header[0] |= 0x00;                // Spacecraft time reference status
		header[1] = message.serviceType;
		header[2] = message.messageType;
		header[3] = static_cast<uint8_t>(message.message_type_counter_ >> 8U);
		header[4] = static_cast<uint8_t>(message.message_type_counter_ & 0xffU);
		header[5] = message.application_ID_ >> 8U; // DestinationID
		header[6] = message.application_ID_;
		const uint64_t epochSeconds = TimeGetter::getCurrentTimeUTC().toEpochSeconds();

		// Format as 4-byte value for header (masking to 32 bits)
		const auto ticks = static_cast<uint32_t>(epochSeconds & 0xFFFFFFFFULL);
		header[7] = static_cast<uint8_t>((ticks >> 24) & 0xFFU);
		header[8] = static_cast<uint8_t>((ticks >> 16) & 0xFFU);
		header[9] = static_cast<uint8_t>((ticks >> 8) & 0xFFU);
		header[10] = static_cast<uint8_t>(ticks & 0xFFU);
		header[11] = 0;
		header[12] = 0;
		header[13] = 0;
		header[14] = 0;
	}

	String<CCSDSMaxMessageSize> outData(header.data(), ((message.packet_type_ == Message::TM) ? ECSSSecondaryTMHeaderSize : ECSSSecondaryTCHeaderSize));
	if (message.packet_type_ == Message::TC) {
		outData.append(message.data.begin(), ecss_total_size - ECSSSecondaryTCHeaderSize);
	} else if (message.packet_type_ == Message::TM) {
		outData.append(message.data.begin(), ecss_total_size - ECSSSecondaryTMHeaderSize);
	}

	// Make sure to reach the requested size
	if (ecss_total_size != 0) {
		const auto currentSize = outData.size();

		if (currentSize > CCSDSMaxMessageSize) {
			return etl::unexpected(OBDH_ERROR_MESSAGE_PARSER_COMPOSE_ECSS_DATA_SIZE_LARGER_THAN_EXPECTED);
		}
		if (currentSize < ecss_total_size) {
			// Pad with zeros to reach the requested size
			outData.append(ecss_total_size - currentSize, 0);
		}
	}
	return outData;
}

etl::expected<String<CCSDSMaxMessageSize>, SpacecraftErrorCode> MessageParser::compose(Message& message, uint16_t total_eccs_size) {

	if (total_eccs_size > CCSDSMaxMessageSize - CCSDSPrimaryHeaderSize) {
		return etl::unexpected(OBDH_ERROR_MESSAGE_PARSER_COMPOSE_DATA_SIZE_LARGER_THAN_EXPECTED);
	}
	message.total_size_ecss_ = total_eccs_size;
	if (message.packet_type_ == Message::TC) {
		message.data_size_ecss_ = total_eccs_size - ECSSSecondaryTCHeaderSize;
	}

	if (message.packet_type_ == Message::TM) {
		message.data_size_ecss_ = total_eccs_size - ECSSSecondaryTMHeaderSize;
	}
	message.total_size_ccsds_ = total_eccs_size + CCSDSPrimaryHeaderSize;

	// First, compose the ECSS part
	// here the size must be totalECSSSize
	auto result = composeECSS(message, total_eccs_size);

	if (!result.has_value()) {
		return etl::unexpected(result.error());
	}
	const auto& data = result.value();

	// Parts of the header
	ApplicationProcessId packetId = message.application_ID_;
	packetId |= (1U << 11U); // Secondary header flag
	packetId |= (message.packet_type_ == Message::TC) ? (1U << 12U)
	                                                  : (0U); // Ignore-MISRA
	SequenceCount const packetSequenceControl = message.packet_sequence_count_ | (3U << 14U);
	const uint16_t packetCCSDSDataLength = data.size() - 1;

	// Compile the header
	etl::array<uint8_t, CCSDSPrimaryHeaderSize> header = {};

	header[0] = packetId >> 8U;
	header[1] = packetId & 0xffU;
	header[2] = packetSequenceControl >> 8U;
	header[3] = packetSequenceControl & 0xffU;
	header[4] = packetCCSDSDataLength >> 8U;
	header[5] = packetCCSDSDataLength & 0xffU;

	// Compile the final message by appending the header
	String<CCSDSMaxMessageSize> ccsdsMessage(header.data(), CCSDSPrimaryHeaderSize);
	ccsdsMessage.append(data);
	// CRC
	if constexpr (CRCHelper::EnableCRC) {
		const CRCSize crcField = CRCHelper::calculateCRC(reinterpret_cast<uint8_t*>(ccsdsMessage.data()), CCSDSPrimaryHeaderSize + data.size());
		etl::array<uint8_t, CRCField> crcMessage = {static_cast<uint8_t>(crcField >> 8U), static_cast<uint8_t>(crcField & 0xFF)};
		String<CCSDSMaxMessageSize> crcString(crcMessage.data(), 2);
		ccsdsMessage.append(crcString);
	}

	return ccsdsMessage;
}

SpacecraftErrorCode MessageParser::parseECSSTMHeader(const uint8_t* data, uint16_t length, Message& message) {
	// sanity check
	if (length > ECSSMaxMessageSize) {
		return OBDH_ERROR_MESSAGE_PARSER_TM_SIZE_LARGER_THAN_EXPECTED;
	}
	//
	if (length < ECSSSecondaryTMHeaderSize) {
		return OBDH_ERROR_MESSAGE_PARSER_TM_SIZE_LESS_THAN_EXPECTED;
	}
	// Individual fields of the TM header
	uint8_t const pusVersion = data[0] >> 4;
	if (pusVersion != 2U)
		return OBDH_ERROR_MESSAGE_PARSER_PARSE_WRONG_PUS_VERSION;

	ServiceTypeNum const serviceType = data[1];
	MessageTypeNum const messageType = data[2];

	// Copy the data to the message
	message.serviceType = serviceType;
	message.messageType = messageType;
	std::copy(data + ECSSSecondaryTMHeaderSize, data + length, message.data.begin());
	message.data_size_ecss_ = length - ECSSSecondaryTMHeaderSize;

	return GENERIC_ERROR_NONE;
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
