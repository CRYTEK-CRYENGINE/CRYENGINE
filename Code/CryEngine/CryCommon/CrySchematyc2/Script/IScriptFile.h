// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/AggregateTypeId.h"
#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/IAny.h"
#include "CrySchematyc2/Script/IScriptElement.h"
#include "CrySchematyc2/Services/ITimerSystem.h"

namespace Schematyc2
{
	struct IDocGraph; // #SchematycTODO : Remove!
	struct IProperties;
	struct SScriptGraphParams;

	DECLARE_SHARED_POINTERS(IProperties)

	struct IScriptInclude : public IScriptElement
	{
		virtual ~IScriptInclude() {}

		virtual const char* GetFileName() const = 0;
		virtual SGUID GetRefGUID() const = 0;
	};

	struct IScriptGroup : public IScriptElement
	{
		virtual ~IScriptGroup() {}
	};

	struct IScriptEnumeration : public IScriptElement
	{
		virtual ~IScriptEnumeration() {}

		virtual CAggregateTypeId GetTypeId() const = 0;
		virtual size_t GetConstantCount() const = 0;
		virtual size_t FindConstant(const char* szConstant) const = 0;
		virtual const char* GetConstant(size_t constantIdx) const = 0;
	};

	struct IScriptStructure : public IScriptElement
	{
		virtual ~IScriptStructure() {}

		virtual CAggregateTypeId GetTypeId() const = 0;
		virtual size_t GetFieldCount() const = 0;
		virtual const char* GetFieldName(size_t fieldIdx) const = 0;
		virtual IAnyConstPtr GetFieldValue(size_t fieldIdx) const = 0;
	};

	struct IScriptSignal : public IScriptElement
	{
		virtual ~IScriptSignal() {}

		virtual size_t GetInputCount() const = 0;
		virtual const char* GetInputName(size_t inputIdx) const = 0;
		virtual IAnyConstPtr GetInputValue(size_t inputIdx) const = 0;
	};

	struct IScriptAbstractInterface : public IScriptElement
	{
		virtual ~IScriptAbstractInterface() {}

		virtual const char* GetAuthor() const = 0;
		virtual const char* GetDescription() const = 0;
	};

	struct IScriptAbstractInterfaceFunction : public IScriptElement
	{
		virtual ~IScriptAbstractInterfaceFunction() {}

		virtual const char* GetAuthor() const = 0;
		virtual const char* GetDescription() const = 0;
		virtual size_t GetInputCount() const = 0;
		virtual const char* GetInputName(size_t inputIdx) const = 0;
		virtual IAnyConstPtr GetInputValue(size_t inputIdx) const = 0;
		virtual size_t GetOutputCount() const = 0;
		virtual const char* GetOutputName(size_t outputIdx) const = 0;
		virtual IAnyConstPtr GetOutputValue(size_t outputIdx) const = 0;
	};

	struct IScriptAbstractInterfaceTask : public IScriptElement
	{
		virtual ~IScriptAbstractInterfaceTask() {}

		virtual const char* GetAuthor() const = 0;
		virtual const char* GetDescription() const = 0;
	};

	struct IScriptClass : public IScriptElement
	{
		virtual ~IScriptClass() {}

		virtual const char* GetAuthor() const = 0;
		virtual const char* GetDescription() const = 0;
		virtual SGUID GetFoundationGUID() const = 0;
		virtual IPropertiesConstPtr GetFoundationProperties() const = 0;
	};

	struct IScriptClassBase : public IScriptElement
	{
		virtual ~IScriptClassBase() {}

		virtual SGUID GetRefGUID() const = 0;
	};

	// #SchematycTODO : Move to BasicTypes.h, merge with ELibStateMachineLifetime and rename EStateMachineLifetime?
	// #SchematycTODO : Consider making tasks and state machines different element types!
	enum class EScriptStateMachineLifetime 
	{
		Persistent,
		Task
	};

	struct IScriptStateMachine : public IScriptElement
	{
		virtual ~IScriptStateMachine() {}

		virtual EScriptStateMachineLifetime GetLifetime() const = 0;
		virtual SGUID GetContextGUID() const = 0;
		virtual SGUID GetPartnerGUID() const = 0;
	};

	struct IScriptState : public IScriptElement
	{
		virtual ~IScriptState() {}

		virtual SGUID GetPartnerGUID() const = 0;
	};

	struct IScriptVariable : public IScriptElement
	{
		virtual ~IScriptVariable() {}

		virtual CAggregateTypeId GetTypeId() const = 0;
		virtual IAnyConstPtr GetValue() const = 0;
	};

	struct IScriptProperty : public IScriptElement
	{
		virtual ~IScriptProperty() {}

		virtual SGUID GetRefGUID() const = 0;
		virtual EOverridePolicy GetOverridePolicy() const = 0;
		virtual CAggregateTypeId GetTypeId() const = 0;
		virtual IAnyConstPtr GetValue() const = 0;
	};

	struct IScriptContainer : public IScriptElement
	{
		virtual ~IScriptContainer() {}

		virtual SGUID GetTypeGUID() const = 0;
	};

	struct IScriptTimer : public IScriptElement
	{
		virtual ~IScriptTimer() {}

		virtual STimerParams GetParams() const = 0;
	};

	struct IScriptAbstractInterfaceImplementation : public IScriptElement
	{
		virtual ~IScriptAbstractInterfaceImplementation() {}

		virtual EDomain GetDomain() const = 0;
		virtual SGUID GetRefGUID() const = 0;
	};

	enum class EScriptComponentInstanceFlags
	{
		None       = 0,
		Foundation = BIT(0) // Component is part of foundation and can't be removed.
	};

	DECLARE_ENUM_CLASS_FLAGS(EScriptComponentInstanceFlags)

	struct IScriptComponentInstance : public IScriptElement
	{
		virtual ~IScriptComponentInstance() {}

		virtual SGUID GetComponentGUID() const = 0;
		virtual EScriptComponentInstanceFlags GetFlags() const = 0;
		virtual IPropertiesConstPtr GetProperties() const = 0;
	};

	struct IScriptActionInstance : public IScriptElement
	{
		virtual ~IScriptActionInstance() {}

		virtual SGUID GetActionGUID() const = 0;
		virtual SGUID GetComponentInstanceGUID() const = 0;
		virtual IPropertiesConstPtr GetProperties() const = 0;
	};

	enum class EScriptFileFlags
	{
		None     = 0,
		OnDisk   = BIT(0), // File is on disk and not loaded from pak.
		Modified = BIT(1)  // #SchematycTODO : Need to think more about how we can mark individual elements as modified, but this flag at least allows us to report when save fails.
	};

	DECLARE_ENUM_CLASS_FLAGS(EScriptFileFlags)

	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptElement&)>                               ScriptElementVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptElement&)>                         ScriptElementConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptInclude&)>                               ScriptIncludeVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptInclude&)>                         ScriptIncludeConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptGroup&)>                                 ScriptGroupVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptGroup&)>                           ScriptGroupConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptEnumeration&)>                           ScriptEnumerationVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptEnumeration&)>                     ScriptEnumerationConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptStructure&)>                             ScriptStructureVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptStructure&)>                       ScriptStructureConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptSignal&)>                                ScriptSignalVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptSignal&)>                          ScriptSignalConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptAbstractInterface&)>                     ScriptAbstractInterfaceVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptAbstractInterface&)>               ScriptAbstractInterfaceConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptAbstractInterfaceFunction&)>             ScriptAbstractInterfaceFunctionVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptAbstractInterfaceFunction&)>       ScriptAbstractInterfaceFunctionConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptAbstractInterfaceTask&)>                 ScriptAbstractInterfaceTaskVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptAbstractInterfaceTask&)>           ScriptAbstractInterfaceTaskConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptClass&)>                                 ScriptClassVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptClass&)>                           ScriptClassConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptClassBase&)>                             ScriptClassBaseVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptClassBase&)>                       ScriptClassBaseConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptStateMachine&)>                          ScriptStateMachineVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptStateMachine&)>                    ScriptStateMachineConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptState&)>                                 ScriptStateVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptState&)>                           ScriptStateConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptVariable&)>                              ScriptVariableVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptVariable&)>                        ScriptVariableConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptProperty&)>                              ScriptPropertyVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptProperty&)>                        ScriptPropertyConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptContainer&)>                             ScriptContainerVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptContainer&)>                       ScriptContainerConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptTimer&)>                                 ScriptTimerVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptTimer&)>                           ScriptTimerConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptAbstractInterfaceImplementation&)>       ScriptAbstractInterfaceImplementationVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptAbstractInterfaceImplementation&)> ScriptAbstractInterfaceImplementationConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptComponentInstance&)>                     ScriptComponentInstanceVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptComponentInstance&)>               ScriptComponentInstanceConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IScriptActionInstance&)>                        ScriptActionInstanceVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptActionInstance&)>                  ScriptActionInstanceConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (IDocGraph&)>                                    DocGraphVisitor; // #SchematycTODO : Remove!
	typedef TemplateUtils::CDelegate<EVisitStatus (const IDocGraph&)>                              DocGraphConstVisitor; // #SchematycTODO : Remove!

	struct SScriptElementClipboardInfo // #SchematycTODO : Where should this live?
	{
		inline SScriptElementClipboardInfo(EScriptElementType _elementType = EScriptElementType::None)
			: elementType(_elementType)
		{}

		EScriptElementType elementType;
	};

	struct IScriptFile
	{
		virtual ~IScriptFile() {}

		virtual const char* GetFileName() const = 0;
		virtual SGUID GetGUID() const = 0;
		virtual void SetFlags(EScriptFileFlags flags) = 0;
		virtual EScriptFileFlags GetFlags() const = 0;
		virtual IScriptInclude* AddInclude(const SGUID& scopeGUID, const char* szFileName, const SGUID& refGUID) = 0;
		virtual IScriptInclude* GetInclude(const SGUID& guid) = 0;
		virtual const IScriptInclude* GetInclude(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitIncludes(const ScriptIncludeVisitor& visitor) = 0;
		virtual EVisitStatus VisitIncludes(const ScriptIncludeVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitIncludes(const ScriptIncludeConstVisitor& visitor) const = 0;
		virtual EVisitStatus VisitIncludes(const ScriptIncludeConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptGroup* AddGroup(const SGUID& scopeGUID, const char* szName) = 0;
		virtual IScriptGroup* GetGroup(const SGUID& guid) = 0;
		virtual const IScriptGroup* GetGroup(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitGroups(const ScriptGroupVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitGroups(const ScriptGroupConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptEnumeration* AddEnumeration(const SGUID& scopeGUID, const char* szName) = 0;
		virtual IScriptEnumeration* GetEnumeration(const SGUID& guid) = 0;
		virtual const IScriptEnumeration* GetEnumeration(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitEnumerations(const ScriptEnumerationVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitEnumerations(const ScriptEnumerationConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptStructure* AddStructure(const SGUID& scopeGUID, const char* szName) = 0;
		virtual IScriptStructure* GetStructure(const SGUID& guid) = 0;
		virtual const IScriptStructure* GetStructure(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitStructures(const ScriptStructureVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitStructures(const ScriptStructureConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptSignal* AddSignal(const SGUID& scopeGUID, const char* szName) = 0;
		virtual IScriptSignal* GetSignal(const SGUID& guid) = 0;
		virtual const IScriptSignal* GetSignal(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitSignals(const ScriptSignalVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitSignals(const ScriptSignalConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptAbstractInterface* AddAbstractInterface(const SGUID& scopeGUID, const char* szName) = 0;
		virtual IScriptAbstractInterface* GetAbstractInterface(const SGUID& guid) = 0;
		virtual const IScriptAbstractInterface* GetAbstractInterface(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitAbstractInterfaces(const ScriptAbstractInterfaceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitAbstractInterfaces(const ScriptAbstractInterfaceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptAbstractInterfaceFunction* AddAbstractInterfaceFunction(const SGUID& scopeGUID, const char* szName) = 0;
		virtual IScriptAbstractInterfaceFunction* GetAbstractInterfaceFunction(const SGUID& guid) = 0;
		virtual const IScriptAbstractInterfaceFunction* GetAbstractInterfaceFunction(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitAbstractInterfaceFunctions(const ScriptAbstractInterfaceFunctionVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitAbstractInterfaceFunctions(const ScriptAbstractInterfaceFunctionConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptAbstractInterfaceTask* AddAbstractInterfaceTask(const SGUID& scopeGUID, const char* szName) = 0;
		virtual IScriptAbstractInterfaceTask* GetAbstractInterfaceTask(const SGUID& guid) = 0;
		virtual const IScriptAbstractInterfaceTask* GetAbstractInterfaceTask(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitAbstractInterfaceTasks(const ScriptAbstractInterfaceTaskVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitAbstractInterfaceTasks(const ScriptAbstractInterfaceTaskConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptClass* AddClass(const SGUID& scopeGUID, const char* szName, const SGUID& foundationGUID) = 0;
		virtual IScriptClass* GetClass(const SGUID& guid) = 0;
		virtual const IScriptClass* GetClass(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitClasses(const ScriptClassVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitClasses(const ScriptClassConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptClassBase* AddClassBase(const SGUID& scopeGUID, const SGUID& refGUID) = 0;
		virtual IScriptClassBase* GetClassBase(const SGUID& guid) = 0;
		virtual const IScriptClassBase* GetClassBase(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitClassBases(const ScriptClassBaseVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitClassBases(const ScriptClassBaseConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptStateMachine* AddStateMachine(const SGUID& scopeGUID, const char* szName, EScriptStateMachineLifetime lifetime, const SGUID& contextGUID, const SGUID& partnerGUID) = 0;
		virtual IScriptStateMachine* GetStateMachine(const SGUID& guid) = 0;
		virtual const IScriptStateMachine* GetStateMachine(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitStateMachines(const ScriptStateMachineVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitStateMachines(const ScriptStateMachineConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptState* AddState(const SGUID& scopeGUID, const char* szName, const SGUID& partnerGUID) = 0;
		virtual IScriptState* GetState(const SGUID& guid) = 0;
		virtual const IScriptState* GetState(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitStates(const ScriptStateVisitor& visitor) = 0;
		virtual EVisitStatus VisitStates(const ScriptStateVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitStates(const ScriptStateConstVisitor& visitor) const = 0;
		virtual EVisitStatus VisitStates(const ScriptStateConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptVariable* AddVariable(const SGUID& scopeGUID, const char* szName, const CAggregateTypeId& typeId) = 0;
		virtual IScriptVariable* GetVariable(const SGUID& guid) = 0;
		virtual const IScriptVariable* GetVariable(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitVariables(const ScriptVariableVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitVariables(const ScriptVariableConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptProperty* AddProperty(const SGUID& scopeGUID, const char* szName, const SGUID& refGUID, const CAggregateTypeId& typeId) = 0;
		virtual IScriptProperty* GetProperty(const SGUID& guid) = 0;
		virtual const IScriptProperty* GetProperty(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitProperties(const ScriptPropertyVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitProperties(const ScriptPropertyConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptContainer* AddContainer(const SGUID& scopeGUID, const char* szName, const SGUID& typeGUID) = 0;
		virtual IScriptContainer* GetContainer(const SGUID& guid) = 0;
		virtual const IScriptContainer* GetContainer(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitContainers(const ScriptContainerVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitContainers(const ScriptContainerConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptTimer* AddTimer(const SGUID& scopeGUID, const char* szName) = 0;
		virtual IScriptTimer* GetTimer(const SGUID& guid) = 0;
		virtual const IScriptTimer* GetTimer(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitTimers(const ScriptTimerVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitTimers(const ScriptTimerConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptAbstractInterfaceImplementation* AddAbstractInterfaceImplementation(const SGUID& scopeGUID, EDomain domain, const SGUID& refGUID) = 0;
		virtual IScriptAbstractInterfaceImplementation* GetAbstractInterfaceImplementation(const SGUID& guid) = 0;
		virtual const IScriptAbstractInterfaceImplementation* GetAbstractInterfaceImplementation(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitAbstractInterfaceImplementations(const ScriptAbstractInterfaceImplementationVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitAbstractInterfaceImplementations(const ScriptAbstractInterfaceImplementationConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptComponentInstance* AddComponentInstance(const SGUID& scopeGUID, const char* szName, const SGUID& componentGUID, EScriptComponentInstanceFlags flags) = 0;
		virtual IScriptComponentInstance* GetComponentInstance(const SGUID& guid) = 0;
		virtual const IScriptComponentInstance* GetComponentInstance(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitComponentInstances(const ScriptComponentInstanceVisitor& visitor) = 0;
		virtual EVisitStatus VisitComponentInstances(const ScriptComponentInstanceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitComponentInstances(const ScriptComponentInstanceConstVisitor& visitor) const = 0;
		virtual EVisitStatus VisitComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IScriptActionInstance* AddActionInstance(const SGUID& scopeGUID, const char* szName, const SGUID& actionGUID, const SGUID& componentInstanceGUID) = 0;
		virtual IScriptActionInstance* GetActionInstance(const SGUID& guid) = 0;
		virtual const IScriptActionInstance* GetActionInstance(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitActionInstances(const ScriptActionInstanceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitActionInstances(const ScriptActionInstanceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual IDocGraph* AddGraph(const SScriptGraphParams& params) = 0;
		virtual IDocGraph* GetGraph(const SGUID& guid) = 0;
		virtual const IDocGraph* GetGraph(const SGUID& guid) const = 0;
		virtual EVisitStatus VisitGraphs(const DocGraphVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitGraphs(const DocGraphConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual void RemoveElement(const SGUID& guid, bool clearScope) = 0;
		virtual IScriptElement* GetElement(const SGUID& guid) = 0;
		virtual const IScriptElement* GetElement(const SGUID& guid) const = 0;
		virtual IScriptElement* GetElement(const SGUID& guid, EScriptElementType elementType) = 0;
		virtual const IScriptElement* GetElement(const SGUID& guid, EScriptElementType elementType) const = 0;
		virtual EVisitStatus VisitElements(const ScriptElementVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) = 0;
		virtual EVisitStatus VisitElements(const ScriptElementConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const = 0;
		virtual bool IsElementNameUnique(const SGUID& scopeGUID, const char* szName) const = 0; // #SchematycTODO : Move to separate utils class!
		virtual XmlNodeRef CopyElementsToXml(const SGUID& guid, bool bRecurseHierarchy) const = 0;
		virtual void PasteElementsFromXml(const SGUID& scopeGUID, const XmlNodeRef& xml) = 0;
		virtual void Load() = 0;
		virtual void Save() = 0;
		virtual void Refresh(const SScriptRefreshParams& params) = 0;
		virtual bool GetClipboardInfo(const XmlNodeRef& xml, SScriptElementClipboardInfo& clipboardInfo) const = 0; // #SchematycTODO : Move to separate utils class!
	};
}
