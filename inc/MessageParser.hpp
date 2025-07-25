#ifndef ECSS_SERVICES_MESSAGEPARSER_HPP
#define ECSS_SERVICES_MESSAGEPARSER_HPP
#include <etl/expected.h>


#include "ErrorDefinitions.hpp"
#include "Message.hpp"

/**
 * A generic class responsible for the execution and the parsing of the incoming telemetry and telecommand
 * packets
 *
 * This class is responsible for converting Packets and Messages to and from the internal representation used in this
 * project. The following hierarchy is used between the different layers on ecss-services:
 *
 * \code
 *                                                       -------------------
 *                                                       | User data field |
 *                                                       -------------------
 *                            ---------------------------                            Application Layer
 *                            | Packet secondary header |
 *                            |      (ECSS header)      |
 *                            ---------------------------                    --------------------------------
 *  -------------------------
 *  | Packet primary header |
 *  |     (CCSDS header)    |                                                          Network Layer
 *  -------------------------
 * \endcode
 *
 * The service data is encapsulated within the, **ECSS packet** which is encapsulated within the **CCSDS packet**.
 * The MessageParser class is responsible for adding and processing both the ECSS and CCSDS headers. The target it uses
 * for the internal representation of all received Telemetry (TM) and Telecommands (TC) is the \ref Message class.
 */

namespace CAN {
	class TPMessage;
}
class MessageParser {
public:
	/**
	 * This function takes as input TC packets and calls the proper services' functions that have been
	 * implemented to handle TC packets.
	 *
	 * @param message Contains the necessary parameters to call the suitable subservice
	 */
	static void execute(Message& message);

	/**
	 * Parse a message that contains the CCSDS and ECSS packet headers, as well as the data
	 *
	 * As defined in CCSDS 133.0-B-1
	 *
	 * @param data The data of the message (not null-terminated)
	 * @param length The size of the message
	 * @return A new object that represents the parsed message
	 */
    static SpacecraftErrorCode parse(const uint8_t* data, uint32_t length, Message& message, bool error_reporting_active, bool parse_ccsds);

	/**
	 * Parse data that contains the ECSS packet header, without the CCSDS space packet header
	 *
	 * Note: conversion of char* to unsigned char* should flow without any problems according to
	 * this great analysis:
	 * stackoverflow.com/questions/15078638/can-i-turn-unsigned-char-into-char-and-vice-versa
	 */

    static SpacecraftErrorCode parseECSSTC(const uint8_t* data, Message& message);

	/**
	 * @brief Overloaded version of \ref MessageParser::parseECSSTC(String<ECSS_TC_REQUEST_STRING_SIZE> data)
	 * @param data A uint8_t array of the TC packet data
	 * @return Parsed message
	 */
    static SpacecraftErrorCode parseECSSTC(String<ECSSTCRequestStringSize> data, Message& message);

	/**
	 * @brief Converts a TC or TM message to a message string, appending just the ECSS header
	 * @todo (#249) Add time reference, as soon as it is available and the format has been specified
	 * @param message The Message object to be parsed to a String
	 * @param size The wanted size of the message (including the headers). Messages larger than \p size display an
	 * error. Messages smaller than \p size are padded with zeros. When `size = 0`, there is no size limit.
	 * @return A String class containing the parsed Message
	 */
	static etl::expected<String<CCSDSMaxMessageSize>, SpacecraftErrorCode> composeECSS(Message& message,  uint16_t size);

	/**
	 * @brief Converts a TC or TM message to a packet string, appending the ECSS and then the CCSDS header
	 * @param message The Message object to be parsed to a String
	 * @return A String class containing the parsed Message
	 */
    static etl::expected<String<CCSDSMaxMessageSize>, SpacecraftErrorCode> compose( Message& message, uint16_t size);


    /**
 * Parse the ECSS Telecommand packet secondary header
 *
 * As specified in section 7.4.4.1 of the standard
 *
 * @param data The data of the header (not null-terminated)
 * @param length The size of the header
 * @param message The Message to modify based on the header
 */
    static SpacecraftErrorCode parseECSSTCHeader(const uint8_t* data, Message& message);

    /**
 * Parse the ECSS Telemetry packet secondary header
 *
 * As specified in section 7.4.3.1 of the standard
 *
 * @param data The data of the header (not null-terminated)
 * @param length The size of the header
 * @param message The Message to modify based on the header
 */
    static SpacecraftErrorCode parseECSSTMHeader(const uint8_t* data, uint16_t length, Message& message);


private:
	/**
	 * The number of bytes in the CRC field
	 */
	static constexpr CRCSize CRCField = 2U;

};

#endif // ECSS_SERVICES_MESSAGEPARSER_HPP
