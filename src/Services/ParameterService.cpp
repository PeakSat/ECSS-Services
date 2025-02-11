#include "ECSS_Configuration.hpp"
#ifdef SERVICE_PARAMETER

#include "ParameterService.hpp"


void ParameterService::reportParameters(Message& paramIds) {

	if (!paramIds.assertTC(ServiceType, ReportParameterValues)) {
		return;
	}
	Message parameterReport = createTM(ParameterValuesReport);

	uint16_t numOfIds = paramIds.readUint16();
	uint16_t numberOfValidIds = 0;
	for (uint16_t i = 0; i < numOfIds; i++) {
		if (parameterExists(paramIds.read<ParameterId>())) {
			numberOfValidIds++;
		}
	}
	parameterReport.appendUint16(numberOfValidIds);
	paramIds.resetRead();

	numOfIds = paramIds.readUint16();
	for (uint16_t i = 0; i < numOfIds; i++) {
		const ParameterId currId = paramIds.read<ParameterId>();
		parameterReport.append<ParameterId>(currId);
		appendParameterToMessage(parameterReport, currId);
	}

	storeMessage(parameterReport);
}

void ParameterService::setParameters(Message& newParamValues){
	if (!newParamValues.assertTC(ServiceType, MessageType::SetParameterValues)) {
		return;
	}

	uint16_t const numOfIds = newParamValues.readUint16();

	for (uint16_t i = 0; i < numOfIds; i++) {
		const ParameterId currId = newParamValues.read<ParameterId>();
		updateParameterFromMessage(newParamValues, currId);
	}
}

void ParameterService::appendParameterToMessage(Message& message, ParameterId parameter){
	PARAMETER_TYPE type = getParameterType(parameter);

	switch (type) {
		case PARAMETER_TYPE::UINT8: {
			uint8_t temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::INT8: {
			int8_t temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::UINT16: {
			uint16_t temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::INT16: {
			int16_t temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::UINT32: {
			uint32_t temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::INT32: {
			int32_t temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::FLOAT: {
			float temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::UINT64: {
			uint64_t temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::INT64: {
			int64_t temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::DOUBLE: {
			double temp = 0;
			memManTask->getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		default:
			// Handle unknown types safely
			break;
	}
}

void ParameterService::updateParameterFromMessage(Message& message, ParameterId parameter){
	PARAMETER_TYPE type = getParameterType(parameter);

	switch (type) {
		case PARAMETER_TYPE::UINT8: {
			uint8_t temp = message.read<uint8_t>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::INT8: {
			int8_t temp = message.read<int8_t>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::UINT16: {
			uint16_t temp = message.read<uint16_t>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::INT16: {
			int16_t temp = message.read<int16_t>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::UINT32: {
			uint32_t temp = message.read<uint32_t>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::INT32: {
			int32_t temp = message.read<int32_t>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::FLOAT: {
			float temp = message.read<float>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::UINT64: {
			uint64_t temp = message.read<uint64_t>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::INT64: {
			int64_t temp = message.read<int64_t>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::DOUBLE: {
			double temp = message.read<double>();
			memManTask->setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		default:
			// Handle unknown types safely
			break;
	}
}

void ParameterService::execute(Message& message) {
	switch (message.messageType) {
		case ReportParameterValues:
			reportParameters(message);
			break;
		case SetParameterValues:
			setParameters(message);
			break;
		default:
			ErrorHandler::reportInternalError(ErrorHandler::OtherMessageType);
	}
}

#endif
