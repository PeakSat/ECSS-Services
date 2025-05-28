#include <FunctionManagementService.hpp>


#include "ECSS_Configuration.hpp"
#ifdef SERVICE_EVENTACTION

#include "EventActionService.hpp"
#include "Message.hpp"
#include "MessageParser.hpp"

EventActionService::EventActionDefinition::EventActionDefinition(ApplicationProcessId applicationID, EventDefinitionId eventDefinitionID, EventActionId actionID, const etl::array<uint8_t, ECSSFunctionMaxArgLength>& actionArgs)
    : applicationID(applicationID), eventDefinitionID(eventDefinitionID), actionID(actionID), actionArgs(actionArgs) {
}

void EventActionService::addEventActionDefinitions(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::AddEventAction)) {
		return;
	}
	uint8_t numberOfEventActionDefinitions = message.readUint8();
	while (numberOfEventActionDefinitions-- != 0) {
		const ApplicationProcessId applicationID = message.read<ApplicationProcessId>();
		EventDefinitionId eventDefinitionID = message.read<EventDefinitionId>();
		const EventActionId actionID = message.read<EventActionId>();
		bool canBeAdded = true; // NOLINT(misc-const-correctness)
		// Read the length first
		const uint8_t argsLength = message.readUint8();

		// Validate length doesn't exceed buffer size
		if (argsLength > ECSSFunctionMaxArgLength) {
			// Handle error case
			ErrorHandler::reportError(message, ErrorHandler::MessageTooLarge);
			return;
		}
		etl::array<uint8_t, ECSSFunctionMaxArgLength> actionArgs = {};
		message.readString(actionArgs.data(), argsLength);

		auto it = std::find_if(eventActionDefinitionMap.begin(), eventActionDefinitionMap.end(), [&](const auto& element) {
			if (element.first == eventDefinitionID) {
				if (element.second.enabled) {
					canBeAdded = false;
					ErrorHandler::reportError(message, ErrorHandler::EventActionEnabledError);
				} else {
					eventActionDefinitionMap.erase(eventDefinitionID);
				}
				return true;
			}
			return false;
		});
		if (canBeAdded) {
			if (eventActionDefinitionMap.size() == ECSSEventActionStructMapSize) {
				ErrorHandler::reportError(message, ErrorHandler::EventActionDefinitionsMapIsFull);
				continue;
			}
			const EventActionDefinition temporaryEventActionDefinition(applicationID, eventDefinitionID, actionID, actionArgs);
			eventActionDefinitionMap.insert(std::make_pair(eventDefinitionID, temporaryEventActionDefinition));
		}
	}
}

void EventActionService::deleteEventActionDefinitions(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::DeleteEventAction)) {
		return;
	}
	uint8_t numberOfEventActionDefinitions = message.readUint8();
	while (numberOfEventActionDefinitions-- != 0) {
		ApplicationProcessId applicationID = message.read<ApplicationProcessId>();
		EventDefinitionId eventDefinitionID = message.read<EventDefinitionId>();
		bool actionDefinitionExists = false; // NOLINT(misc-const-correctness)


		auto it = std::find_if(eventActionDefinitionMap.begin(), eventActionDefinitionMap.end(), [&](const auto& element) {
			if (element.first == eventDefinitionID) {
				actionDefinitionExists = true;
				if (element.second.applicationID != applicationID) {
					ErrorHandler::reportError(message, ErrorHandler::EventActionUnknownEventActionDefinitionError);
				} else if (element.second.enabled) {
					ErrorHandler::reportError(message, ErrorHandler::EventActionDeleteEnabledDefinitionError);
				} else {
					eventActionDefinitionMap.erase(eventActionDefinitionMap.find(eventDefinitionID));
				}
				return true;
			}
			return false;
		});
		if (not actionDefinitionExists) {
			ErrorHandler::reportError(message, ErrorHandler::EventActionUnknownEventActionDefinitionError);
		}
	}
}

void EventActionService::deleteAllEventActionDefinitions(const Message& message) {
	if (!message.assertTC(ServiceType, MessageType::DeleteAllEventAction)) {
		return;
	}
	setEventActionFunctionStatus(false);
	eventActionDefinitionMap.clear();
}

void EventActionService::enableEventActionDefinitions(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::EnableEventAction)) {
		return;
	}
	uint8_t numberOfEventActionDefinitions = message.readUint8();
	if (numberOfEventActionDefinitions != 0U) {
		while (numberOfEventActionDefinitions-- != 0) {
			ApplicationProcessId applicationID = message.read<ApplicationProcessId>();
			EventDefinitionId eventDefinitionID = message.read<EventDefinitionId>();
			bool actionDefinitionExists = false; // NOLINT(misc-const-correctness)

			auto it = std::find_if(eventActionDefinitionMap.begin(), eventActionDefinitionMap.end(), [&](auto& element) {
				if (element.first == eventDefinitionID) {
					actionDefinitionExists = true;
					if (element.second.applicationID != applicationID) {
						ErrorHandler::reportError(message, ErrorHandler::EventActionUnknownEventActionDefinitionError);
					} else {
						element.second.enabled = true;
					}
					return true;
				}
				return false;
			});
			if (not actionDefinitionExists) {
				ErrorHandler::reportError(message, ErrorHandler::EventActionUnknownEventActionDefinitionError);
			}
		}
	} else {
		for (auto& element: eventActionDefinitionMap) {
			element.second.enabled = true;
		}
	}
}

void EventActionService::disableEventActionDefinitions(Message& message) {
	if (!message.assertTC(ServiceType, MessageType::DisableEventAction)) {
		return;
	}
	uint8_t numberOfEventActionDefinitions = message.readUint8();
	if (numberOfEventActionDefinitions != 0U) {
		while (numberOfEventActionDefinitions-- != 0) {
			ApplicationProcessId applicationID = message.read<ApplicationProcessId>();
			EventDefinitionId eventDefinitionID = message.read<EventDefinitionId>();
			bool actionDefinitionExists = false; // NOLINT(misc-const-correctness)

			auto it = std::find_if(eventActionDefinitionMap.begin(), eventActionDefinitionMap.end(), [&](auto& element) {
				if (element.first == eventDefinitionID) {
					actionDefinitionExists = true;
					if (element.second.applicationID != applicationID) {
						ErrorHandler::reportError(message, ErrorHandler::EventActionUnknownEventActionDefinitionError);
					} else {
						element.second.enabled = false;
					}
					return true;
				}
				return false;
			});
			if (not actionDefinitionExists) {
				ErrorHandler::reportError(message, ErrorHandler::EventActionUnknownEventActionDefinitionError);
			}
		}
	} else {
		for (auto& element: eventActionDefinitionMap) {
			element.second.enabled = false;
		}
	}
}

void EventActionService::requestEventActionDefinitionStatus(const Message& message) {
	if (!message.assertTC(ServiceType, MessageType::ReportStatusOfEachEventAction)) {
		return;
	}
	eventActionStatusReport();
}

void EventActionService::eventActionStatusReport() {
	Message report = createTM(EventActionStatusReport);
	const uint16_t count = eventActionDefinitionMap.size(); // NOLINT(cppcoreguidelines-init-variables)
	report.appendUint16(count);
	for (const auto& element: eventActionDefinitionMap) {
		report.append<ApplicationProcessId>(element.second.applicationID);
		report.append<EventDefinitionId>(element.second.eventDefinitionID);
		report.appendBoolean(element.second.enabled);
	}
	storeMessage(report, report.data_size_message_);
}

void EventActionService::enableEventActionFunction(const Message& message) {
	if (!message.assertTC(ServiceType, MessageType::EnableEventActionFunction)) {
		return;
	}
	setEventActionFunctionStatus(true);
}

void EventActionService::disableEventActionFunction(const Message& message) {
	if (!message.assertTC(ServiceType, MessageType::DisableEventActionFunction)) {
		return;
	}
	setEventActionFunctionStatus(false);
}

void EventActionService::executeAction(EventDefinitionId eventDefinitionID) { // NOLINT (readability-make-member-function-const)
	if (eventActionFunctionStatus) {
		auto range = eventActionDefinitionMap.equal_range(eventDefinitionID);
		for (auto& element = range.first; element != range.second; ++element) {
			if (element->second.enabled) {
				FunctionManagementService::call(element->second.actionID, element->second.actionArgs);
			}
		}
	}
}

void EventActionService::execute(Message& message) {
	switch (message.messageType) {
		case AddEventAction:
			addEventActionDefinitions(message);
			break;
		case DeleteEventAction:
			deleteEventActionDefinitions(message);
			break;
		case DeleteAllEventAction:
			deleteAllEventActionDefinitions(message);
			break;
		case EnableEventAction:
			enableEventActionDefinitions(message);
			break;
		case DisableEventAction:
			disableEventActionDefinitions(message);
			break;
		case ReportStatusOfEachEventAction:
			requestEventActionDefinitionStatus(message);
			break;
		case EnableEventActionFunction:
			enableEventActionFunction(message);
			break;
		case DisableEventActionFunction:
			disableEventActionFunction(message);
			break;
		default:
			ErrorHandler::reportError(message, ErrorHandler::OtherMessageType);
	}
}

#endif
