#include "ECSS_Configuration.hpp"
#ifdef SERVICE_FUNCTION
#include "FunctionManagementService.hpp"

void FunctionManagementService::execute(Message& message) {
	switch (message.messageType) {
		case PerformFunction:
			call(message); // TC[8,1]
			break;
		default:
			ErrorHandler::reportInternalError(ErrorHandler::OtherMessageType);
			break;
	}
}


#endif
