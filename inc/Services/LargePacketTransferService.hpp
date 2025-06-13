#ifndef ECSS_SERVICES_LARGEPACKETTRANSFERSERVICE_HPP
#define ECSS_SERVICES_LARGEPACKETTRANSFERSERVICE_HPP

#include <etl/String.hpp>

#include "ErrorMaps.hpp"
#include "FilesystemDefinitions.hpp"
#include "Service.hpp"

/**
 * Implementation of the ST[13] large packet transfer service
 * The goal of this service is to help in splitting data packages that exceed the standard's
 * maximum data size
 *
 * Note: More information can be found in the standards' manual, in p. 526-528 and in p. 229-236
 *
 * @ingroup Services
 */

class LargePacketTransferService : public Service {
public:
	inline static constexpr ServiceTypeNum ServiceType = 13;

	static constexpr uint8_t UplinkMaximumLargePacketsSize = 400U; // todo revisit
	static constexpr uint8_t UplinkMaximumPartSize = ECSSMaxFixedOctetStringSize;
	static constexpr uint32_t UplinkReceptionTimeout = 300U; // seconds todo revisit

	static String<MemoryFilesystem::MAX_FILENAME> localFilename;

	enum MessageType : uint8_t {
		FirstDownlinkPartReport = 1,
		IntermediateDownlinkPartReport = 2,
		LastDownlinkPartReport = 3,
		FirstUplinkPartReport = 9,
		IntermediateUplinkPartReport = 10,
		LastUplinkPartReport = 11,
		UplinkAborted = 16,
	};

	enum class UplinkLargeMessageTransactionIdentifiers : uint16_t {
		AtlasMcuFirmware = 70U,
		AtlasSoftCpuFirmware = 80U,
		AtlasBitStream = 90U,
		ScheduledTC = 130U,
		ObcFirmware = 140U,
	};
	using UplinkLargeMessageTransactionIdentifiers_t = std::underlying_type<UplinkLargeMessageTransactionIdentifiers>;

	/**
	 * Default constructor since only functions will be used.
	 */
	LargePacketTransferService() {
		serviceType = ServiceType;
	}

	/**
	 * TM[13,1] Function that handles the first part of the download report
	 * @param largeMessageTransactionIdentifier The identifier of the large packet
	 * @param partSequenceNumber The identifier of the part of the large packet
	 * @param string The data contained in this part of the large packet
	 */
	void firstDownlinkPartReport(LargeMessageTransactionId largeMessageTransactionIdentifier, PartSequenceNum partSequenceNumber,
	                             const String<ECSSMaxFixedOctetStringSize>& string);

	/**
	 * TM[13,2] Function that handles the n-2 parts of tbe n-part download report
	 * @param largeMessageTransactionIdentifier The identifier of the large packet
	 * @param partSequenceNumber The identifier of the part of the large packet
	 * @param string The data contained in this part of the large packet
	 */
	void intermediateDownlinkPartReport(LargeMessageTransactionId largeMessageTransactionIdentifier, PartSequenceNum partSequenceNumber,
	                                    const String<ECSSMaxFixedOctetStringSize>& string);

	/**
	 * TM[13,3] Function that handles the last part of the download report
	 * @param largeMessageTransactionIdentifier The identifier of the large packet
	 * @param partSequenceNumber The identifier of the part of the large packet
	 * @param string The data contained in this part of the large packet
	 */
	void lastDownlinkPartReport(LargeMessageTransactionId largeMessageTransactionIdentifier, PartSequenceNum partSequenceNumber,
	                            const String<ECSSMaxFixedOctetStringSize>& string);

	// The three uplink functions should handle a TC request to "upload" data. Since there is not
	// a composeECSS function ready, I just return the given string.
	// @TODO (#220): Modify these functions properly
	/**
	 * TC[13,9] Function that handles the first part of the uplink request
	 * @param string This will change when these function will be modified
	 */
	static void firstUplinkPart(Message& message);

	/**
	 * TC[13,10] Function that handles the n-2 parts of the n-part uplink request
	 * @param string This will change when these function will be modified
	 */
	static void
	intermediateUplinkPart(Message& message);

	/**
	 * TC[13,11] Function that handles the last part of the uplink request
	 * @param string This will change when these function will be modified
	 */
	static void lastUplinkPart(Message& message);

	/**
	 * Function that splits large messages
	 * @param message that is exceeds the standards and has to be split down
	 * @param largeMessageTransactionIdentifier that is a value we assign to this splitting of the large message
	 */
	void split(const Message& message, LargeMessageTransactionId largeMessageTransactionIdentifier);

	static bool isValidUpLinkIdentifier(UplinkLargeMessageTransactionIdentifiers id);

	void execute(Message& message);

private:
	// ... existing private members ...

	/**
	 * Helper function to validate uplink message type and extract transaction ID
	 * @param message The message to validate
	 * @param expectedType The expected message type
	 * @param transactionId Output parameter for the transaction ID
	 * @return true if validation successful, false otherwise
	 */
	static bool validateUplinkMessage(Message& message, MessageType expectedType,
	                                  LargeMessageTransactionId& transactionId);

	/**
	 * Helper function to validate that stored transaction ID matches expected ID
	 * @param message The message context for error reporting
	 * @param expectedId The expected transaction ID
	 * @return true if validation successful, false otherwise
	 */
	static bool validateStoredTransactionId(const Message& message, LargeMessageTransactionId expectedId);

	/**
	 * Template helper function to get a parameter from memory with error handling
	 * @tparam T The type of parameter to retrieve
	 * @param message The message context for error reporting
	 * @param paramId The parameter ID to retrieve
	 * @param value Pointer to store the retrieved value
	 * @return true if successful, false otherwise
	 */
	template <typename T>
	bool getMemoryParameter(Message& message, const ParameterId paramId, T* value) {
		auto result = MemoryManager::getParameter(paramId, value);
		if (!result.has_value()) {
			Services.requestVerification.failAcceptanceVerification(
			    message, getSpacecraftErrorCodeFromMemoryError(result.error()));
			return false;
		}
		return true;
	}

	/**
	 * Template helper function to set a parameter in memory with error handling
	 * @tparam T The type of parameter to set
	 * @param message The message context for error reporting
	 * @param paramId The parameter ID to set
	 * @param value Pointer to the value to set
	 * @return true if successful, false otherwise
	 */
	template <typename T>
	bool setMemoryParameter(Message& message, const ParameterId paramId, T* value) {
		auto result = MemoryManager::setParameter(paramId, value);
		if (!result.has_value()) {
			Services.requestVerification.failAcceptanceVerification(
			    message, getSpacecraftErrorCodeFromMemoryError(result.error()));
			return false;
		}
		return true;
	}

	/**
	 * Helper function to validate sequence number continuity
	 * @param message The message context for error reporting
	 * @param currentSequence The current sequence number to validate
	 * @return true if sequence is valid, false otherwise
	 */
	bool validateSequenceNumber(Message& message, uint16_t currentSequence);

	/**
	 * Helper function to reset transfer parameters
	 * Resets the transfer count parameter to 0
	 */
	static void resetTransferParameters();
};

#endif // ECSS_SERVICES_LARGEPACKETTRANSFERSERVICE_HPP
