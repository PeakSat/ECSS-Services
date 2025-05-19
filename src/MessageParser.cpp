#include "MessageParser.hpp"
#include <ServicePool.hpp>
#include "ErrorHandler.hpp"
#include "CRCHelper.hpp"
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
		default:
			ErrorHandler::reportInternalError(ErrorHandler::OtherMessageType);
	}
}

SpacecraftErrorCode MessageParser::parse(const uint8_t* data, uint32_t length, Message& message, bool error_reporting_active, bool parse_ccsds) {
    if (parse_ccsds == false)
        return TTC_ERROR_MESSAGE_PARSER_PARSE_WRONG_USAGE;
    uint16_t const packetHeaderIdentification = (data[0] << 8) | data[1];
    uint16_t const packetSequenceControl = (data[2] << 8) | data[3];
    uint16_t const packetCCSDSDataLength = (data[4] << 8) | data[5];

    uint8_t const versionNumber = data[0] >> 5;
    Message::PacketType const packet_type = ((data[0] & 0x10) == 0) ? Message::TM : Message::TC;
    bool const secondaryHeaderFlag = (data[0] & 0x08U) != 0U;
    ApplicationProcessId const APID = packetHeaderIdentification & static_cast<ApplicationProcessId>(0x07ff);
    auto sequenceFlags = static_cast<uint8_t>(packetSequenceControl >> 14);
    SequenceCount const packetSequenceCount = packetSequenceControl & (~0xc000U); // keep last 14 bits

    message = Message(0, 0, packet_type, APID);

    if (packet_type == Message::TM && length < ECSSSecondaryTMHeaderSize) {
        return TTC_ERROR_MESSAGE_PARSER_TM_SIZE_LESS_THAN_EXPECTED;
    }
    if (packet_type == Message::TC && length < ECSSSecondaryTCHeaderSize) {
        return TTC_ERROR_MESSAGE_PARSER_TC_SIZE_LESS_THAN_EXPECTED;
    }

    if (error_reporting_active) {
        if (versionNumber != 0U)
            return TTC_ERROR_PUS_VERSION_WRONG;
        if (!secondaryHeaderFlag)
            return TTC_ERROR_MESSAGE_PARSER_PARSE_SECONDARY_HEADER;
        if (sequenceFlags != 0x3U)
            return TTC_ERROR_MESSAGE_PARSER_PARSE_SEQUENCE_FLAGS;
    }

    message.packet_sequence_count_ = packetSequenceCount;

    if (packetCCSDSDataLength > ECSSMaxMessageSize)
        return TTC_ERROR_MESSAGE_PARSER_TC_SIZE_LARGER_THAN_EXPECTED;
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
    //
    message.data_size_message_ = message.total_size_ecss_;
    //
    etl::array<uint8_t, ECSSMaxMessageSize> ecss_data{};
    memcpy(ecss_data.data(), &data[CCSDSPrimaryHeaderSize], message.total_size_ecss_);
    //
    if (packet_type == Message::TC) {
        return parseECSSTCHeader(ecss_data.data(), message);
    }
    return parseECSSTMHeader(ecss_data.data(), message.total_size_ecss_, message);
}



SpacecraftErrorCode MessageParser::parseECSSTCHeader(const uint8_t* data, Message& message) {
    // sanity check
    if (message.total_size_ecss_ > ECSSMaxMessageSize)
        return TTC_ERROR_MESSAGE_PARSER_TC_SIZE_LARGER_THAN_EXPECTED;

    if (message.total_size_ecss_ < ECSSSecondaryTCHeaderSize) {
        return TTC_ERROR_MESSAGE_PARSER_TC_SIZE_LESS_THAN_EXPECTED;
    }
	// Individual fields of the TC header
	uint8_t const pusVersion = data[0] >> 4;
	ServiceTypeNum const serviceType = data[1];
	MessageTypeNum const messageType = data[2];
	SourceId const sourceId = (data[3] << 8) + data[4];

    if (pusVersion != 2U)
        return TTC_ERROR_PUS_VERSION_WRONG;

	// Remove the length of the header
	message.total_size_ecss_ = message.total_size_ccsds_ - ECSSSecondaryTCHeaderSize;

	// Copy the data to the message
	message.serviceType = serviceType;
	message.messageType = messageType;
	message.source_ID_ = sourceId;
	std::copy(data + ECSSSecondaryTCHeaderSize, data + message.total_size_ecss_, message.data.begin());

    return GENERIC_ERROR_NONE;
}


SpacecraftErrorCode MessageParser::parseECSSTC(String<ECSSTCRequestStringSize> data, Message& message) {
    const auto* dataInt = reinterpret_cast<const uint8_t*>(data.data());
    return parseECSSTCHeader(dataInt, message);
}

SpacecraftErrorCode MessageParser::parseECSSTC(const uint8_t* data, Message& message) {
    return parseECSSTCHeader(data, message);
}

etl::pair<SpacecraftErrorCode, String<CCSDSMaxMessageSize>> MessageParser::composeECSS(const Message& message, uint16_t ecss_total_size) {
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
	}

	String<CCSDSMaxMessageSize> outData(header.data(), ((message.packet_type_ == Message::TM) ? ECSSSecondaryTMHeaderSize : ECSSSecondaryTCHeaderSize));
    if (message.packet_type_ == Message::TC) {
        outData.append(message.data.begin(), ecss_total_size - ECSSSecondaryTCHeaderSize);
    }
    else if (message.packet_type_ == Message::TM) {
        outData.append(message.data.begin(), ecss_total_size - ECSSSecondaryTMHeaderSize);
    }

	// Make sure to reach the requested size
	if (ecss_total_size != 0) {
	    const auto currentSize = outData.size();

	    // if (currentSize != (size + ECSSSecondaryTMHeaderSize)) {
	        // Error: Message exceeds maximum allowed size
	        // return etl::make_pair(getSpacecraftErrorCodeFromECSSError(ErrorHandler::UnacceptablePacket), String<CCSDSMaxMessageSize>("Message too large"));
	    // }
        if (currentSize > CCSDSMaxMessageSize) {
            return etl::make_pair(TTC_ERROR_MESSAGE_PARSER_COMPOSE_ECSS_DATA_SIZE_LARGER_THAN_EXPECTED, String<CCSDSMaxMessageSize>("Message too large"));
        }
	    if (currentSize < ecss_total_size) {
	        // Pad with zeros to reach the requested size
	        outData.append(ecss_total_size - currentSize, 0);
	    }
    }
	return etl::make_pair(GENERIC_ERROR_NONE, outData);
}

 etl::pair<SpacecraftErrorCode, String<CCSDSMaxMessageSize>> MessageParser::compose(const Message& message, uint16_t total_eccs_size) {

    if (total_eccs_size > CCSDSMaxMessageSize - CCSDSPrimaryHeaderSize) {
        return etl::make_pair(TTC_ERROR_MESSAGE_PARSER_COMPOSE_DATA_SIZE_LARGER_THAN_EXPECTED, String<CCSDSMaxMessageSize>("Message too large"));
    }

	// First, compose the ECSS part
    // here the size must be totalECSSSize
    auto result = composeECSS(message, total_eccs_size);

    auto spacecraft_error_code = result.first;
    if (spacecraft_error_code != GENERIC_ERROR_NONE) {
        return etl::make_pair(spacecraft_error_code, String<CCSDSMaxMessageSize>("error"));
    }
    auto data = result.second;

	// Parts of the header
	ApplicationProcessId packetId = message.application_ID_;
	packetId |= (1U << 11U);                                              // Secondary header flag
	packetId |= (message.packet_type_ == Message::TC) ? (1U << 12U)
    : (0U); // Ignore-MISRA
	SequenceCount const packetSequenceControl = message.packet_sequence_count_ | (3U << 14U);
	uint16_t packetCCSDSDataLength = data.size() - 1;

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
		etl::array<uint8_t, CRCField> crcMessage = {static_cast<uint8_t>(crcField >> 8U), static_cast<uint8_t>
		                                            (crcField &  0xFF)};
		String<CCSDSMaxMessageSize> crcString(crcMessage.data(), 2);
		ccsdsMessage.append(crcString);
	}

	return etl::make_pair(GENERIC_ERROR_NONE, ccsdsMessage);
}

SpacecraftErrorCode MessageParser::parseECSSTMHeader(const uint8_t* data, uint16_t length, Message& message) {
    // sanity check
    if (length > ECSSMaxMessageSize) {
        return TTC_ERROR_MESSAGE_PARSER_TM_SIZE_LARGER_THAN_EXPECTED;
    }
    //
    if (length < ECSSSecondaryTMHeaderSize) {
        return TTC_ERROR_MESSAGE_PARSER_TM_SIZE_LESS_THAN_EXPECTED;
    }
	// Individual fields of the TM header
	uint8_t const pusVersion = data[0] >> 4;
    if (pusVersion != 2U)
        return TTC_ERROR_PUS_VERSION_WRONG;

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
