// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Script/Elements/IScriptSignalReceiver.h" // Wrap EScriptSignalReceiverType in a SScriptSignalReceiverParams structure so that we can forward declare?
#include "CrySchematyc/Script/Elements/IScriptStateMachine.h"   // Wrap EScriptStateMachineLifetime in a SScriptStateMachineParams structure so that we can forward declare?
#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/Signal.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IString;
struct IScriptActionInstance;
struct IScriptBase;
struct IScriptClass;
struct IScriptComponentInstance;
struct IScriptConstructor;
struct IScriptEnum;
struct IScriptFunction;
struct IScriptInterface;
struct IScriptInterfaceFunction;
struct IScriptInterfaceImpl;
struct IScriptInterfaceTask;
struct IScriptModule;
struct IScriptSignal;
struct IScriptState;
struct IScriptStateMachine;
struct IScriptStruct;
struct IScriptTimer;
struct IScriptVariable;
// Forward declare structures.
struct SElementId;

typedef std::function<EVisitStatus(IScript&)> ScriptVisitor;

enum class EScriptRegistryChangeType
{
	Invalid,
	ElementAdded,              // Sent after element has been added.
	ElementModified,           // Sent after element has been modified.
	ElementRemoved,            // Sent before element is removed.
	ElementSaved,              // Sent after element has been saved.
	ElementDependencyModified, // Sent after dependency of element has been modified.
	ElementDependencyRemoved   // Sent after dependency of element has been removed.
};

struct SScriptRegistryChange
{
	inline SScriptRegistryChange(EScriptRegistryChangeType _type, IScriptElement& _element)
		: type(_type)
		, element(_element)
	{}

	EScriptRegistryChangeType type;
	IScriptElement&           element;
};

typedef CSignal<void (const SScriptRegistryChange&)> ScriptRegistryChangeSignal;

struct IScriptRegistry
{
	virtual ~IScriptRegistry() {}

	// Compatibility interface.
	//////////////////////////////////////////////////

	virtual void ProcessEvent(const SScriptEvent& event) = 0;
	virtual bool Load() = 0;
	virtual void Save(bool bAlwaysSave = false) = 0;

	// New interface.
	//////////////////////////////////////////////////

	virtual bool                               IsValidScope(EScriptElementType elementType, IScriptElement* pScope) const = 0;
	virtual bool                               IsValidName(const char* szName, IScriptElement* pScope, const char*& szErrorMessage) const = 0;

	virtual IScriptModule*                     AddModule(const char* szName, const char* szFilePath) = 0;
	virtual IScriptEnum*                       AddEnum(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptStruct*                     AddStruct(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptSignal*                     AddSignal(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptConstructor*                AddConstructor(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptFunction*                   AddFunction(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptInterface*                  AddInterface(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptInterfaceFunction*          AddInterfaceFunction(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptInterfaceTask*              AddInterfaceTask(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptClass*                      AddClass(const char* szName, const SElementId& baseId, const char* szFilePath) = 0;
	virtual IScriptBase*                       AddBase(const SElementId& baseId, IScriptElement* pScope) = 0;
	virtual IScriptStateMachine*               AddStateMachine(const char* szName, EScriptStateMachineLifetime lifetime, const CryGUID& contextGUID, const CryGUID& partnerGUID, IScriptElement* pScope) = 0;
	virtual IScriptState*                      AddState(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptVariable*                   AddVariable(const char* szName, const SElementId& typeId, const CryGUID& baseGUID, IScriptElement* pScope) = 0;
	virtual IScriptTimer*                      AddTimer(const char* szName, IScriptElement* pScope) = 0;
	virtual IScriptSignalReceiver*             AddSignalReceiver(const char* szName, EScriptSignalReceiverType type, const CryGUID& signalGUID, IScriptElement* pScope) = 0;
	virtual IScriptInterfaceImpl*              AddInterfaceImpl(EDomain domain, const CryGUID& refGUID, IScriptElement* pScope) = 0;
	virtual IScriptComponentInstance*          AddComponentInstance(const char* szName, const CryGUID& typeGUID, IScriptElement* pScope) = 0;
	virtual IScriptActionInstance*             AddActionInstance(const char* szName, const CryGUID& actionGUID, const CryGUID& contextGUID, IScriptElement* pScope) = 0;

	virtual void                               RemoveElement(const CryGUID& guid) = 0;

	virtual IScriptElement&                    GetRootElement() = 0;
	virtual const IScriptElement&              GetRootElement() const = 0;
	virtual IScriptElement*                    GetElement(const CryGUID& guid) = 0;
	virtual const IScriptElement*              GetElement(const CryGUID& guid) const = 0;

	virtual bool                               CopyElementsToXml(XmlNodeRef& output, IScriptElement& scope) const = 0;
	virtual bool                               PasteElementsFromXml(const XmlNodeRef& input, IScriptElement* pScope) = 0;

	virtual bool                               SaveUndo(XmlNodeRef& output, IScriptElement& scope) const = 0;
	virtual IScriptElement*                    RestoreUndo(const XmlNodeRef& input, IScriptElement* pScope) = 0;

	virtual bool                               IsElementNameUnique(const char* szName, IScriptElement* pScope) const = 0;
	virtual void                               MakeElementNameUnique(IString& name, IScriptElement* pScope) const = 0;    // Make element name unique within specified scope.
	virtual void                               ElementModified(IScriptElement& element) = 0;                              // Notify registry that element has been modified.

	virtual ScriptRegistryChangeSignal::Slots& GetChangeSignalSlots() = 0;

	virtual IScript*                           GetScriptByGuid(const CryGUID& guid) const = 0;
	virtual IScript*                           GetScriptByFileName(const char* szFilePath) const = 0;

	virtual IScript*                           LoadScript(const char* szFilePath) = 0;
	virtual void                               SaveScript(IScript& script) = 0;

	virtual void                               OnScriptRenamed(IScript& script, const char* szFilePath) = 0;
};
} // Schematyc
