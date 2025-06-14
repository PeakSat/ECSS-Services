#ifndef ECSS_SERVICES_PARAMETERSERVICE_HPP
#define ECSS_SERVICES_PARAMETERSERVICE_HPP

#include <optional>
#include "ECSS_Definitions.hpp"
#include "ErrorHandler.hpp"
#include "Service.hpp"
#include "MemoryManager.hpp"
#include "etl/map.h"

/**
 * Implementation of the ST[20] parameter management service,
 * as defined in ECSS-E-ST-70-41C
 *
 * Parameter manager - ST[20]
 *
 * The purpose of this class is to handle functions regarding the access and modification
 * of the various parameters of the CubeSat.
 * The parameters to be managed are initialized and kept in \ref SystemParameters.
 *
 * @ingroup Services
 * @author Grigoris Pavlakis <grigpavl@ece.auth.gr>
 * @author Athanasios Theocharis <athatheoc@gmail.com>
 */
class ParameterService : public Service {
private:


public:
	inline static constexpr ServiceTypeNum ServiceType = 20;

	enum MessageType : uint8_t {
		ReportParameterValues = 1,
		ParameterValuesReport = 2,
		SetParameterValues = 3,
	};

	/**
	 * The Constructor initializes \var parameters
	 * by calling \fn initializeParametersArray
	 */
	ParameterService() {
		serviceType = ServiceType;
	}

	/**
	 * Checks if \var parameters contains a reference to a parameter with
	 * the given parameter ID as key
	 *
	 * @param parameterId the given ID
	 * @return True if there is a reference to a parameter with the given ID, False otherwise
	 */
	static bool parameterExists(ParameterId parameterId) {
		for (int i=0; i < PeakSatParameters::parametersArraySize;i++) {
			if (PeakSatParameters::allParameterIds[i] == parameterId) {
				return true;
			}
		}
		return false;
	}


	/**
	 * This function receives a TC[20, 1] packet and returns a TM[20, 2] packet
	 * containing the current configuration
	 * **for the parameters specified in the carried valid IDs**.
	 *
	 * @param paramId: a TC[20, 1] packet carrying the requested parameter IDs
	 * @return None (messages are stored using storeMessage())
	 */
	void reportParameters(Message& paramIds);

	/**
	 * This function receives a TC[20, 3] message and after checking whether its type is correct,
	 * iterates over all contained parameter IDs and replaces the settings for each valid parameter,
	 * while ignoring all invalid IDs.
	 *
	 * @param newParamValues: a valid TC[20, 3] message carrying parameter ID and replacement value
	 */
	void setParameters(Message& newParamValues);

	void appendParameterToMessage(Message& message, ParameterId parameter);

	void updateParameterFromMessage(Message& message, ParameterId parameter);

	/**
	 * It is responsible to call the suitable function that executes a telecommand packet. The source of that packet
	 * is the ground station.
	 *
	 * @note This function is called from the main execute() that is defined in the file MessageParser.hpp
	 * @param param Contains the necessary parameters to call the suitable subservice
	 */
	void execute(Message& message);
};

#endif // ECSS_SERVICES_PARAMETERSERVICE_HPP
