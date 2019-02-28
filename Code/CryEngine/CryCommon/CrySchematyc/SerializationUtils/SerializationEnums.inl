// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This header should be included once (and only once) per module.

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Services/ITimerSystem.h"
#include "CrySchematyc/Script/IScriptElement.h"
#include "CrySchematyc/Script/Elements/IScriptSignalReceiver.h"
#include "CrySchematyc/Utils/UniqueId.h"

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, EDomain, "CrySchematyc Domain")
SERIALIZATION_ENUM(Schematyc::EDomain::Env, "Env", "Environment")
SERIALIZATION_ENUM(Schematyc::EDomain::Script, "Script", "Script")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, EOverridePolicy, "CrySchematyc Override Policy")
SERIALIZATION_ENUM(Schematyc::EOverridePolicy::Default, "Default", "Default")
SERIALIZATION_ENUM(Schematyc::EOverridePolicy::Override, "Override", "Override")
SERIALIZATION_ENUM(Schematyc::EOverridePolicy::Final, "Final", "Final")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, ETimerUnits, "CrySchematyc Timer Units")
SERIALIZATION_ENUM(Schematyc::ETimerUnits::Empty, "Empty", "Empty")
SERIALIZATION_ENUM(Schematyc::ETimerUnits::Frames, "Frames", "Frames")
SERIALIZATION_ENUM(Schematyc::ETimerUnits::Seconds, "Seconds", "Seconds")
SERIALIZATION_ENUM(Schematyc::ETimerUnits::Random, "Random", "Random")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, ETimerFlags, "CrySchematyc Timer Flags")
SERIALIZATION_ENUM(Schematyc::ETimerFlags::AutoStart, "AutoStart", "Auto Start")
SERIALIZATION_ENUM(Schematyc::ETimerFlags::Repeat, "Repeat", "Repeat")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED2(Schematyc, CUniqueId, EType, "CrySchematyc Unique Port Id Type")
#if SCHEMATYC_PATCH_UNIQUE_IDS
SERIALIZATION_ENUM(Schematyc::CUniqueId::EType::UInt32, "UniqueId", "UniqueId")
#endif
SERIALIZATION_ENUM(Schematyc::CUniqueId::EType::Idx, "Idx", "Idx")
SERIALIZATION_ENUM(Schematyc::CUniqueId::EType::UInt32, "UInt32", "UInt32")
SERIALIZATION_ENUM(Schematyc::CUniqueId::EType::StringHash, "StringHash", "StringHash")
SERIALIZATION_ENUM(Schematyc::CUniqueId::EType::GUID, "GUID", "GUID")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, EScriptElementType, "CrySchematyc Script Element Type")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Root, "Root", "Root")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Module, "Module", "Module")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Enum, "Enum", "Enumeration")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Struct, "Struct", "Structure")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Signal, "Signal", "Signal")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Constructor, "Constructor", "Constructor")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Function, "Function", "Function")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Interface, "Interface", "Interface")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::InterfaceFunction, "InterfaceFunction", "Interface Function")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::InterfaceTask, "InterfaceTask", "Interface Task")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Class, "Class", "Class")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Base, "Base", "Base")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::StateMachine, "StateMachine", "State Machine")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::State, "State", "State")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Variable, "Variable", "Variable")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::Timer, "Timer", "Timer")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::SignalReceiver, "SignalReceiver", "Signal Receiver")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::InterfaceImpl, "InterfaceImpl", "Interface Implementation")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::ComponentInstance, "ComponentInstance", "Component Instance")
SERIALIZATION_ENUM(Schematyc::EScriptElementType::ActionInstance, "ActionInstance", "Action Instance")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, EScriptElementAccessor, "CrySchematyc Script Element Accessor")
SERIALIZATION_ENUM(Schematyc::EScriptElementAccessor::Public, "Public", "Public")
SERIALIZATION_ENUM(Schematyc::EScriptElementAccessor::Protected, "Protected", "Protected")
SERIALIZATION_ENUM(Schematyc::EScriptElementAccessor::Private, "Private", "Private")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, EScriptSignalReceiverType, "CrySchematyc Script Signal Receiver Type")
SERIALIZATION_ENUM(Schematyc::EScriptSignalReceiverType::EnvSignal, "EnvSignal", "Environment Signal")
SERIALIZATION_ENUM(Schematyc::EScriptSignalReceiverType::ScriptSignal, "ScriptSignal", "Script Signal")
SERIALIZATION_ENUM(Schematyc::EScriptSignalReceiverType::ScriptTimer, "ScriptTimer", "Script Timer")
SERIALIZATION_ENUM(Schematyc::EScriptSignalReceiverType::Universal, "Universal", "Universal")
SERIALIZATION_ENUM_END()
