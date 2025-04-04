#include <FunctionManagementWrappers.hpp>


#include "ECSS_Configuration.hpp"
#ifdef SERVICE_FUNCTION
#include "FunctionManagementService.hpp"

void FunctionManagementService::execute(Message& message) {
    const uint16_t functionIDraw = (message.data[0] << 8) | message.data[1];
	etl::array<uint8_t, ECSSFunctionMaxArgLength> functionArgs{};
	if (message.dataSize - 2 > ECSSFunctionMaxArgLength) {
		ErrorHandler::reportInternalError(ErrorHandler::MessageTooLarge);
		return;
	}
	etl::copy(message.data.begin() + 2, message.data.begin() + message.dataSize, functionArgs.begin());

	switch (message.messageType) {
		case PerformFunction:
			call(functionIDraw, functionArgs); // TC[8,1]
			break;
		default:
			ErrorHandler::reportInternalError(ErrorHandler::OtherMessageType);
			break;
	}
}


#endif
