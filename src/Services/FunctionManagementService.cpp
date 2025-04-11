#include <ServicePool.hpp>


#include "ECSS_Configuration.hpp"
#include "FunctionManagementWrappers.hpp"
#ifdef SERVICE_FUNCTION
#include "FunctionManagementService.hpp"

void FunctionManagementService::execute(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::PerformFunction)) {
		return;
	}

	const uint16_t functionIDraw = (message.data[0] << 8) | message.data[1];
	etl::array<uint8_t, ECSSFunctionMaxArgLength> functionArgs{};

	if (message.dataSize - 2 > ECSSFunctionMaxArgLength) {
		Services.requestVerification.failAcceptanceVerification(message, ErrorHandler::AcceptanceErrorType::UnacceptableMessage);
		return;
	}
	etl::copy(message.data.begin() + 2, message.data.begin() + message.dataSize, functionArgs.begin());
	SpacecraftErrorCode status = GENERIC_ERROR_TODO;

	status = call(functionIDraw, functionArgs); // TC[8,1]

	if (status != GENERIC_ERROR_NONE) {
		Services.requestVerification.failCompletionExecutionVerification(message, static_cast<SpacecraftErrorCode>(status));
	} else {
		Services.requestVerification.successCompletionExecutionVerification(message);
	}
}


#endif
