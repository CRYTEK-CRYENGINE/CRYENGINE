// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Bridge_IScriptFile.h"

namespace Bridge {

class CScriptFile : public IScriptFile
{
public:
	CScriptFile(Schematyc2::IScriptFile* file);

	// IScriptFile
	virtual Schematyc2::IScriptFile* GetDelegate() const override;

	virtual const char* GetFileName() const override;
	virtual Schematyc2::SGUID GetGUID() const override;

	virtual void SetFlags(Schematyc2::EScriptFileFlags flags) override;
	virtual Schematyc2::EScriptFileFlags GetFlags() const override;

	virtual Schematyc2::IScriptInclude* AddInclude(const Schematyc2::SGUID& scopeGUID, const char* szFileName, const Schematyc2::SGUID& refGUID) override;
	virtual Schematyc2::IScriptInclude* GetInclude(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptInclude* GetInclude(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitIncludes(const Schematyc2::ScriptIncludeVisitor& visitor) override;
	virtual Schematyc2::EVisitStatus VisitIncludes(const Schematyc2::ScriptIncludeVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitIncludes(const Schematyc2::ScriptIncludeConstVisitor& visitor) const override;
	virtual Schematyc2::EVisitStatus VisitIncludes(const Schematyc2::ScriptIncludeConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptGroup* AddGroup(const Schematyc2::SGUID& scopeGUID, const char* szName) override;
	virtual Schematyc2::IScriptGroup* GetGroup(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptGroup* GetGroup(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitGroups(const Schematyc2::ScriptGroupVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitGroups(const Schematyc2::ScriptGroupConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptEnumeration* AddEnumeration(const Schematyc2::SGUID& scopeGUID, const char* szName) override;
	virtual Schematyc2::IScriptEnumeration* GetEnumeration(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptEnumeration* GetEnumeration(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitEnumerations(const Schematyc2::ScriptEnumerationVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitEnumerations(const Schematyc2::ScriptEnumerationConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptStructure* AddStructure(const Schematyc2::SGUID& scopeGUID, const char* szName) override;
	virtual Schematyc2::IScriptStructure* GetStructure(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptStructure* GetStructure(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitStructures(const Schematyc2::ScriptStructureVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitStructures(const Schematyc2::ScriptStructureConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptSignal* AddSignal(const Schematyc2::SGUID& scopeGUID, const char* szName) override;
	virtual Schematyc2::IScriptSignal* GetSignal(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptSignal* GetSignal(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitSignals(const Schematyc2::ScriptSignalVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitSignals(const Schematyc2::ScriptSignalConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptAbstractInterface* AddAbstractInterface(const Schematyc2::SGUID& scopeGUID, const char* szName) override;
	virtual Schematyc2::IScriptAbstractInterface* GetAbstractInterface(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptAbstractInterface* GetAbstractInterface(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitAbstractInterfaces(const Schematyc2::ScriptAbstractInterfaceVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitAbstractInterfaces(const Schematyc2::ScriptAbstractInterfaceConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptAbstractInterfaceFunction* AddAbstractInterfaceFunction(const Schematyc2::SGUID& scopeGUID, const char* szName) override;
	virtual Schematyc2::IScriptAbstractInterfaceFunction* GetAbstractInterfaceFunction(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptAbstractInterfaceFunction* GetAbstractInterfaceFunction(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitAbstractInterfaceFunctions(const Schematyc2::ScriptAbstractInterfaceFunctionVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitAbstractInterfaceFunctions(const Schematyc2::ScriptAbstractInterfaceFunctionConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptAbstractInterfaceTask* AddAbstractInterfaceTask(const Schematyc2::SGUID& scopeGUID, const char* szName) override;
	virtual Schematyc2::IScriptAbstractInterfaceTask* GetAbstractInterfaceTask(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptAbstractInterfaceTask* GetAbstractInterfaceTask(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitAbstractInterfaceTasks(const Schematyc2::ScriptAbstractInterfaceTaskVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitAbstractInterfaceTasks(const Schematyc2::ScriptAbstractInterfaceTaskConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptClass* AddClass(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& foundationGUID) override;
	virtual Schematyc2::IScriptClass* GetClass(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptClass* GetClass(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitClasses(const Schematyc2::ScriptClassVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitClasses(const Schematyc2::ScriptClassConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptClassBase* AddClassBase(const Schematyc2::SGUID& scopeGUID, const Schematyc2::SGUID& refGUID) override;
	virtual Schematyc2::IScriptClassBase* GetClassBase(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptClassBase* GetClassBase(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitClassBases(const Schematyc2::ScriptClassBaseVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitClassBases(const Schematyc2::ScriptClassBaseConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptStateMachine* AddStateMachine(const Schematyc2::SGUID& scopeGUID, const char* szName, Schematyc2::EScriptStateMachineLifetime lifetime, const Schematyc2::SGUID& contextGUID, const Schematyc2::SGUID& partnerGUID) override;
	virtual Schematyc2::IScriptStateMachine* GetStateMachine(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptStateMachine* GetStateMachine(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitStateMachines(const Schematyc2::ScriptStateMachineVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitStateMachines(const Schematyc2::ScriptStateMachineConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptState* AddState(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& partnerGUID) override;
	virtual Schematyc2::IScriptState* GetState(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptState* GetState(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitStates(const Schematyc2::ScriptStateVisitor& visitor) override;
	virtual Schematyc2::EVisitStatus VisitStates(const Schematyc2::ScriptStateVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitStates(const Schematyc2::ScriptStateConstVisitor& visitor) const override;
	virtual Schematyc2::EVisitStatus VisitStates(const Schematyc2::ScriptStateConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptVariable* AddVariable(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::CAggregateTypeId& typeId) override;
	virtual Schematyc2::IScriptVariable* GetVariable(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptVariable* GetVariable(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitVariables(const Schematyc2::ScriptVariableVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitVariables(const Schematyc2::ScriptVariableConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptProperty* AddProperty(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& refGUID, const Schematyc2::CAggregateTypeId& typeId) override;
	virtual Schematyc2::IScriptProperty* GetProperty(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptProperty* GetProperty(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitProperties(const Schematyc2::ScriptPropertyVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitProperties(const Schematyc2::ScriptPropertyConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptContainer* AddContainer(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& typeGUID) override;
	virtual Schematyc2::IScriptContainer* GetContainer(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptContainer* GetContainer(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitContainers(const Schematyc2::ScriptContainerVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitContainers(const Schematyc2::ScriptContainerConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptTimer* AddTimer(const Schematyc2::SGUID& scopeGUID, const char* szName) override;
	virtual Schematyc2::IScriptTimer* GetTimer(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptTimer* GetTimer(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitTimers(const Schematyc2::ScriptTimerVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitTimers(const Schematyc2::ScriptTimerConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptAbstractInterfaceImplementation* AddAbstractInterfaceImplementation(const Schematyc2::SGUID& scopeGUID, Schematyc2::EDomain domain, const Schematyc2::SGUID& refGUID) override;
	virtual Schematyc2::IScriptAbstractInterfaceImplementation* GetAbstractInterfaceImplementation(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptAbstractInterfaceImplementation* GetAbstractInterfaceImplementation(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitAbstractInterfaceImplementations(const Schematyc2::ScriptAbstractInterfaceImplementationVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitAbstractInterfaceImplementations(const Schematyc2::ScriptAbstractInterfaceImplementationConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptComponentInstance* AddComponentInstance(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& componentGUID, Schematyc2::EScriptComponentInstanceFlags flags) override;
	virtual Schematyc2::IScriptComponentInstance* GetComponentInstance(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptComponentInstance* GetComponentInstance(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitComponentInstances(const Schematyc2::ScriptComponentInstanceVisitor& visitor) override;
	virtual Schematyc2::EVisitStatus VisitComponentInstances(const Schematyc2::ScriptComponentInstanceVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitComponentInstances(const Schematyc2::ScriptComponentInstanceConstVisitor& visitor) const override;
	virtual Schematyc2::EVisitStatus VisitComponentInstances(const Schematyc2::ScriptComponentInstanceConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IScriptActionInstance* AddActionInstance(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& actionGUID, const Schematyc2::SGUID& componentInstanceGUID) override;
	virtual Schematyc2::IScriptActionInstance* GetActionInstance(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptActionInstance* GetActionInstance(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitActionInstances(const Schematyc2::ScriptActionInstanceVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitActionInstances(const Schematyc2::ScriptActionInstanceConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual Schematyc2::IDocGraph* AddGraph(const Schematyc2::SScriptGraphParams& params) override;
	virtual Schematyc2::IDocGraph* GetGraph(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IDocGraph* GetGraph(const Schematyc2::SGUID& guid) const override;

	virtual Schematyc2::EVisitStatus VisitGraphs(const Schematyc2::DocGraphVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitGraphs(const Schematyc2::DocGraphConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual void RemoveElement(const Schematyc2::SGUID& guid, bool clearScope) override;
	virtual Schematyc2::IScriptElement* GetElement(const Schematyc2::SGUID& guid) override;
	virtual const Schematyc2::IScriptElement* GetElement(const Schematyc2::SGUID& guid) const override;
	virtual Schematyc2::IScriptElement* GetElement(const Schematyc2::SGUID& guid, Schematyc2::EScriptElementType elementType) override;
	virtual const Schematyc2::IScriptElement* GetElement(const Schematyc2::SGUID& guid, Schematyc2::EScriptElementType elementType) const override;

	virtual Schematyc2::EVisitStatus VisitElements(const Schematyc2::ScriptElementVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) override;
	virtual Schematyc2::EVisitStatus VisitElements(const Schematyc2::ScriptElementConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const override;

	virtual bool IsElementNameUnique(const Schematyc2::SGUID& scopeGUID, const char* szName) const override; // #SchematycTODO : Move to separate utils class!
	virtual XmlNodeRef CopyElementsToXml(const Schematyc2::SGUID& guid, bool bRecurseHierarchy) const override;
	virtual void PasteElementsFromXml(const Schematyc2::SGUID& scopeGUID, const XmlNodeRef& xml) override;

	virtual void Load() override;
	virtual void Save() override;
	virtual void Refresh(const Schematyc2::SScriptRefreshParams& params) override;
	virtual bool GetClipboardInfo(const XmlNodeRef& xml, Schematyc2::SScriptElementClipboardInfo& clipboardInfo) const override; // #SchematycTODO : Move to separate utils class!
	// ~IScriptFile

private:
	Schematyc2::IScriptFile* m_delegate;

};

DECLARE_SHARED_POINTERS(CScriptFile)

}
