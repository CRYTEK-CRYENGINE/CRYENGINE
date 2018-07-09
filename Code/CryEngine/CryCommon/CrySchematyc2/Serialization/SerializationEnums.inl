// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This header should be included once (and only once) per module.

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/Script/IScriptElement.h"
#include "CrySchematyc2/Script/IScriptGraph.h"

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EDomain, "Schematyc Domain")
	SERIALIZATION_ENUM(Schematyc2::EDomain::Env, "env", "Environment")
	SERIALIZATION_ENUM(Schematyc2::EDomain::Script, "doc", "Script") // #SchematycTODO : Update name and patch files!
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EAccessor, "Schematyc Accessor")
	SERIALIZATION_ENUM(Schematyc2::EAccessor::Public, "Public", "Public")
	SERIALIZATION_ENUM(Schematyc2::EAccessor::Protected, "Protected", "Protected")
	SERIALIZATION_ENUM(Schematyc2::EAccessor::Private, "Private", "Private")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EOverridePolicy, "Schematyc Override Policy")
	SERIALIZATION_ENUM(Schematyc2::EOverridePolicy::Default, "Default", "Default")
	SERIALIZATION_ENUM(Schematyc2::EOverridePolicy::Override, "Override", "Override")
	SERIALIZATION_ENUM(Schematyc2::EOverridePolicy::Finalize, "Finalize", "Finalize")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EScriptElementType, "Schematyc Script Element Type")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::None, "", "None") // Workaround to avoid triggering assert error when serializing empty value.
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Root, "Root", "Root")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Module, "Module", "Module")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Include, "Include", "Include")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Group, "Group", "Group")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Enumeration, "Enumeration", "Enumeration")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Structure, "Structure", "Structure")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Signal, "Signal", "Signal")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Function, "Function", "Function")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::AbstractInterface, "AbstractInterface", "AbstractInterface")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::AbstractInterfaceFunction, "AbstractInterfaceFunction", "AbstractInterfaceFunction")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::AbstractInterfaceTask, "AbstractInterfaceTask", "AbstractInterfaceTask")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Class, "Class", "Class")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::ClassBase, "ClassBase", "ClassBase")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::StateMachine, "StateMachine", "StateMachine")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::State, "State", "State")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Variable, "Variable", "Variable")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Property, "Property", "Property")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Container, "Container", "Container")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Timer, "Timer", "Timer")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::AbstractInterfaceImplementation, "AbstractInterfaceImplementation", "AbstractInterfaceImplementation")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::ComponentInstance, "ComponentInstance", "ComponentInstance")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::ActionInstance, "ActionInstance", "ActionInstance")
	SERIALIZATION_ENUM(Schematyc2::EScriptElementType::Graph, "Graph", "Graph")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EScriptGraphType, "Schematyc Script Graph Type")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::Unknown, "unknown", "Unknown")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::AbstractInterfaceFunction, "abstract_interface_function", "AbstractInterfaceFunction")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::Function, "function", "Function")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::Condition, "condition", "Condition")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::Constructor, "constructor", "Constructor")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::Destructor, "destructor", "Destructor")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::SignalReceiver, "signal_receiver", "SignalReceiver")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::Transition, "transition", "Transition")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphType::Property, "Property", "Property")
SERIALIZATION_ENUM_END()
