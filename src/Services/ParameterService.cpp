#include "ECSS_Configuration.hpp"
#ifdef SERVICE_PARAMETER

#include "ParameterService.hpp"
#include "etl/bit.h"


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

	storeMessage(parameterReport, parameterReport.data_size_message_);
}

void ParameterService::setParameters(Message& newParamValues) {
	if (!newParamValues.assertTC(ServiceType, MessageType::SetParameterValues)) {
		return;
	}

	uint16_t const numOfIds = newParamValues.readUint16();

	for (uint16_t i = 0; i < numOfIds; i++) {
		const ParameterId currId = newParamValues.read<ParameterId>();
		updateParameterFromMessage(newParamValues, currId);
	}
}

void ParameterService::appendParameterToMessage(Message& message, ParameterId parameter) {
	PARAMETER_TYPE type = getParameterType(parameter);

	switch (type) {
		case PARAMETER_TYPE::UINT8: {
			uint8_t temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::INT8: {
			int8_t temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::UINT16: {
			uint16_t temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::INT16: {
			int16_t temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::UINT32: {
			uint32_t temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::INT32: {
			int32_t temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::FLOAT: {
			float temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::UINT64: {
			uint64_t temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::INT64: {
			int64_t temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		case PARAMETER_TYPE::DOUBLE: {
			double temp = 0;
			MemoryManager::getParameter(parameter, &temp);
			message.append(temp);
			break;
		}
		default:
			// Handle unknown types safely
			break;
	}
}

void ParameterService::updateParameterFromMessage(Message& message, ParameterId parameter) {
	PARAMETER_TYPE type = getParameterType(parameter);
	const uint64_t temp_max = message.read<uint64_t>();

	switch (type) {
		case PARAMETER_TYPE::UINT8: {
			uint8_t temp = static_cast<uint8_t>(temp_max);
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::INT8: {
			int8_t temp = static_cast<int8_t>(temp_max);
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::UINT16: {
			uint16_t temp = static_cast<uint16_t>(temp_max);
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::INT16: {
			int16_t temp = static_cast<int16_t>(temp_max);
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::UINT32: {
			uint32_t temp = static_cast<uint32_t>(temp_max);
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::INT32: {
			int32_t temp = static_cast<int32_t>(temp_max);
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::FLOAT: {
			float temp = 0;
			const uint32_t temp_raw = static_cast<uint32_t>(temp_max);
			memcpy(&temp, &temp_raw, sizeof(temp));
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::UINT64: {
			uint64_t temp = static_cast<uint64_t>(temp_max);
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::INT64: {
			int64_t temp = static_cast<int64_t>(temp_max);
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
			break;
		}
		case PARAMETER_TYPE::DOUBLE: {
			double temp = 0;
			memcpy(&temp, &temp_max, sizeof(temp));
			MemoryManager::setParameter(parameter, static_cast<void*>(&temp));
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
			ErrorHandler::reportError(message, ErrorHandler::OtherMessageType);
			break;
	}
}

#endif
