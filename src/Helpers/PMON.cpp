#include "PMON.hpp"
#include "ServicePool.hpp"

PMON::PMON(ParameterId monitoredParameterId, PMONRepetitionNumber repetitionNumber, CheckType checkType)
    : monitoredParameterId(monitoredParameterId),
      repetitionNumber(repetitionNumber),
      checkType(checkType){
	if (!Services.parameterManagement.parameterExists(monitoredParameterId)) {
		ErrorHandler::reportInternalError(ErrorHandler::InvalidParameterId);
	}
}