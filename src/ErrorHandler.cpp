#include <ErrorHandler.hpp>
#include <ServicePool.hpp>
#include "ECSS_Configuration.hpp"

#include "ErrorMaps.hpp"
#include "PMON_Handlers.hpp"
#include "RequestVerificationService.hpp"

template <>
void ErrorHandler::reportError(const Message& message, AcceptanceErrorType errorCode) {
#ifdef SERVICE_REQUESTVERIFICATION
	Services.requestVerification.failAcceptanceVerification(message, getSpacecraftErrorCodeFromECSSError(errorCode));
#endif

	logError(message, errorCode);
}

template <>
void ErrorHandler::reportError(const Message& message, ExecutionStartErrorType errorCode) {
#ifdef SERVICE_REQUESTVERIFICATION
	Services.requestVerification.failStartExecutionVerification(message, getSpacecraftErrorCodeFromECSSError(errorCode));
#endif

	logError(message, errorCode);
}

void ErrorHandler::reportProgressError(const Message& message, ExecutionProgressErrorType errorCode, StepId stepID) {
#ifdef SERVICE_REQUESTVERIFICATION
	Services.requestVerification.failProgressExecutionVerification(message, getSpacecraftErrorCodeFromECSSError(errorCode), stepID);
#endif

	logError(message, errorCode);
}

template <>
void ErrorHandler::reportError(const Message& message, ExecutionCompletionErrorType errorCode) {
#ifdef SERVICE_REQUESTVERIFICATION
	Services.requestVerification.failCompletionExecutionVerification(message, getSpacecraftErrorCodeFromECSSError(errorCode));
#endif

	logError(message, errorCode);
}

template <>
void ErrorHandler::reportError(const Message& message, RoutingErrorType errorCode) {
#ifdef SERVICE_REQUESTVERIFICATION
	Services.requestVerification.failRoutingVerification(message, getSpacecraftErrorCodeFromECSSError(errorCode));
#endif

	logError(message, errorCode);
}

template <>
void ErrorHandler::reportError(const Message& message, InternalErrorType errorCode) {
#ifdef SERVICE_REQUESTVERIFICATION
	Services.requestVerification.failStartExecutionVerification(message, getSpacecraftErrorCodeFromECSSError(errorCode));
#endif

	logError(message, errorCode);
}

void ErrorHandler::reportInternalError(ErrorHandler::InternalErrorType errorCode) {
	String<ECSSEventDataAuxiliaryMaxSize> eventMessage("");
	eventMessage.append(static_cast<uint8_t>(errorCode), sizeof(uint8_t));
	PMON_Handlers::raiseEvent(EventReportService::FailedStartOfExecution, EventType::InternalErrorType, EventReportService::LowSeverityAnomalyReport, eventMessage);
	logError(errorCode);
}
