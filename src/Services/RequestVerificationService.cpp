#include "ECSS_Configuration.hpp"
#ifdef SERVICE_REQUESTVERIFICATION

#include "RequestVerificationService.hpp"


void RequestVerificationService::assembleReportMessage(const Message& request, Message& report) {

	report.appendEnumerated(CCSDSPacketVersionBits, CCSDSPacketVersion);
	report.appendEnumerated(PacketTypeBits, request.packet_type_);
	report.appendBits(SecondaryHeaderFlagBits, SecondaryHeaderFlag);
	report.appendEnumerated(ApplicationIdBits, request.application_ID_);
	report.appendEnumerated(ECSSSequenceFlagsBits, ECSSSequenceFlags);
	report.appendBits(PacketSequenceCountBits, request.packet_sequence_count_);
	report.append<uint16_t>(request.function_id_);
}

void RequestVerificationService::successAcceptanceVerification(const Message& request) {
	// TM[1,1] successful acceptance verification report

	Message report = createTM(RequestVerificationService::MessageType::SuccessfulAcceptanceReport);

	assembleReportMessage(request, report);
	storeMessage(report, report.data_size_message_);
}

void RequestVerificationService::failAcceptanceVerification(const Message& request,
                                                            SpacecraftErrorCode errorCode) {
	// TM[1,2] failed acceptance verification report

	Message report = createTM(RequestVerificationService::MessageType::FailedAcceptanceReport);

	assembleReportMessage(request, report);
	report.append<ECSSErrorCode>(errorCode); // error code

	storeMessage(report, report.data_size_message_);
}

void RequestVerificationService::successStartExecutionVerification(const Message& request) {
	// TM[1,3] successful start of execution verification report

	Message report = createTM(RequestVerificationService::MessageType::SuccessfulStartOfExecution);

	assembleReportMessage(request, report);

	storeMessage(report, report.data_size_message_);
}

void RequestVerificationService::failStartExecutionVerification(const Message& request,
                                                                SpacecraftErrorCode errorCode) {
	// TM[1,4] failed start of execution verification report

	Message report = createTM(RequestVerificationService::MessageType::FailedStartOfExecution);

	assembleReportMessage(request, report);

	report.append<ECSSErrorCode>(errorCode); // error code

	storeMessage(report, report.data_size_message_);
}

void RequestVerificationService::successProgressExecutionVerification(const Message& request, StepId stepID) {
	// TM[1,5] successful progress of execution verification report

	Message report = createTM(RequestVerificationService::MessageType::SuccessfulProgressOfExecution);

	assembleReportMessage(request, report);
	report.append<StepId>(stepID); // step ID

	storeMessage(report, report.data_size_message_);
}

void RequestVerificationService::failProgressExecutionVerification(const Message& request,
                                                                   SpacecraftErrorCode errorCode,
                                                                   StepId stepID) {
	// TM[1,6] failed progress of execution verification report

	Message report = createTM(RequestVerificationService::MessageType::FailedProgressOfExecution);

	assembleReportMessage(request, report);
	report.append<StepId>(stepID);           // step ID
	report.append<ECSSErrorCode>(errorCode); // error code

	storeMessage(report, report.data_size_message_);
}

void RequestVerificationService::successCompletionExecutionVerification(const Message& request) {
	// TM[1,7] successful completion of execution verification report

	Message report = createTM(RequestVerificationService::MessageType::SuccessfulCompletionOfExecution);

	assembleReportMessage(request, report);

	storeMessage(report, report.data_size_message_);
}

void RequestVerificationService::failCompletionExecutionVerification(
    const Message& request, SpacecraftErrorCode errorCode) {
	// TM[1,8] failed completion of execution verification report

	Message report = createTM(RequestVerificationService::MessageType::FailedCompletionOfExecution);

	assembleReportMessage(request, report);
	report.append<ECSSErrorCode>(errorCode); // error code

	storeMessage(report, report.data_size_message_);
}


void RequestVerificationService::failRoutingVerification(const Message& request,
                                                         SpacecraftErrorCode errorCode) {
	// TM[1,10] failed routing verification report

	Message report = createTM(RequestVerificationService::MessageType::FailedRoutingReport);

	assembleReportMessage(request, report);
	report.append<ECSSErrorCode>(errorCode); // error code

	storeMessage(report, report.data_size_message_);
}

#endif
