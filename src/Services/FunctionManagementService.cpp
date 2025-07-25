#include <ServicePool.hpp>


#include "ECSS_Configuration.hpp"
#include "FunctionManagementWrappers.hpp"
#ifdef SERVICE_FUNCTION
#include "FunctionManagementService.hpp"

void FunctionManagementService::execute(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::PerformFunction)) {
		return;
	}

	const uint16_t functionID = (message.data[0] << 8) | message.data[1];
	message.function_id_ = functionID;
	etl::array<uint8_t, ECSSFunctionMaxArgLength> functionArgs{};

	etl::copy_n(message.data.begin()+2, ECSSFunctionMaxArgLength, functionArgs.begin());
	SpacecraftErrorCode status = OBDH_ERROR_UNKNOWN_INTERNAL;

	status = call(functionID, functionArgs); // TC[8,1]

	if (status != GENERIC_ERROR_NONE) {
		Services.requestVerification.failCompletionExecutionVerification(message, static_cast<SpacecraftErrorCode>(status));
	} else {
		Services.requestVerification.successCompletionExecutionVerification(message);
	}
}


#endif
