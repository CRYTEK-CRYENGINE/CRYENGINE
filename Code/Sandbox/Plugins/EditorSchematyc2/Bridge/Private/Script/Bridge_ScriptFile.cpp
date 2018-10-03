// Copyrerhight 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Bridge_ScriptFile.h"

namespace Bridge {

//////////////////////////////////////////////////////////////////////////
CScriptFile::CScriptFile(Schematyc2::IScriptFile* file)
	:   m_delegate(file)
{
	CRY_ASSERT(m_delegate);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptFile* CScriptFile::GetDelegate() const
{
	return m_delegate;
}

//////////////////////////////////////////////////////////////////////////
const char* CScriptFile::GetFileName() const
{
	return m_delegate->GetFileName();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::SGUID CScriptFile::GetGUID() const
{
	return m_delegate->GetGUID();
}

//////////////////////////////////////////////////////////////////////////
void CScriptFile::SetFlags(Schematyc2::EScriptFileFlags flags)
{
	m_delegate->SetFlags(flags);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EScriptFileFlags CScriptFile::GetFlags() const
{
	return m_delegate->GetFlags();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptInclude* CScriptFile::AddInclude(const Schematyc2::SGUID& scopeGUID, const char* szFileName, const Schematyc2::SGUID& refGUID)
{
	return m_delegate->AddInclude(scopeGUID, szFileName, refGUID);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptInclude* CScriptFile::GetInclude(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetInclude(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptInclude* CScriptFile::GetInclude(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetInclude(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitIncludes(const Schematyc2::ScriptIncludeVisitor& visitor)
{
	return m_delegate->VisitIncludes(visitor);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitIncludes(const Schematyc2::ScriptIncludeVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitIncludes(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitIncludes(const Schematyc2::ScriptIncludeConstVisitor& visitor) const
{
	return m_delegate->VisitIncludes(visitor);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitIncludes(const Schematyc2::ScriptIncludeConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitIncludes(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptGroup* CScriptFile::AddGroup(const Schematyc2::SGUID& scopeGUID, const char* szName)
{
	return m_delegate->AddGroup(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptGroup* CScriptFile::GetGroup(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetGroup(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptGroup* CScriptFile::GetGroup(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetGroup(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitGroups(const Schematyc2::ScriptGroupVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitGroups(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitGroups(const Schematyc2::ScriptGroupConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitGroups(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptEnumeration* CScriptFile::AddEnumeration(const Schematyc2::SGUID& scopeGUID, const char* szName)
{
	return m_delegate->AddEnumeration(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptEnumeration* CScriptFile::GetEnumeration(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetEnumeration(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptEnumeration* CScriptFile::GetEnumeration(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetEnumeration(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitEnumerations(const Schematyc2::ScriptEnumerationVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitEnumerations(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitEnumerations(const Schematyc2::ScriptEnumerationConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitEnumerations(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptStructure* CScriptFile::AddStructure(const Schematyc2::SGUID& scopeGUID, const char* szName)
{
	return m_delegate->AddStructure(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptStructure* CScriptFile::GetStructure(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetStructure(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptStructure* CScriptFile::GetStructure(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetStructure(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitStructures(const Schematyc2::ScriptStructureVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitStructures(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitStructures(const Schematyc2::ScriptStructureConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitStructures(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptSignal* CScriptFile::AddSignal(const Schematyc2::SGUID& scopeGUID, const char* szName)
{
	return m_delegate->AddSignal(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptSignal* CScriptFile::GetSignal(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetSignal(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptSignal* CScriptFile::GetSignal(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetSignal(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitSignals(const Schematyc2::ScriptSignalVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitSignals(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitSignals(const Schematyc2::ScriptSignalConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitSignals(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptAbstractInterface* CScriptFile::AddAbstractInterface(const Schematyc2::SGUID& scopeGUID, const char* szName)
{
	return m_delegate->AddAbstractInterface(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptAbstractInterface* CScriptFile::GetAbstractInterface(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetAbstractInterface(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptAbstractInterface* CScriptFile::GetAbstractInterface(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetAbstractInterface(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitAbstractInterfaces(const Schematyc2::ScriptAbstractInterfaceVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitAbstractInterfaces(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitAbstractInterfaces(const Schematyc2::ScriptAbstractInterfaceConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitAbstractInterfaces(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptAbstractInterfaceFunction* CScriptFile::AddAbstractInterfaceFunction(const Schematyc2::SGUID& scopeGUID, const char* szName)
{
	return m_delegate->AddAbstractInterfaceFunction(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptAbstractInterfaceFunction* CScriptFile::GetAbstractInterfaceFunction(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetAbstractInterfaceFunction(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptAbstractInterfaceFunction* CScriptFile::GetAbstractInterfaceFunction(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetAbstractInterfaceFunction(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitAbstractInterfaceFunctions(const Schematyc2::ScriptAbstractInterfaceFunctionVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitAbstractInterfaceFunctions(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitAbstractInterfaceFunctions(const Schematyc2::ScriptAbstractInterfaceFunctionConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitAbstractInterfaceFunctions(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptAbstractInterfaceTask* CScriptFile::AddAbstractInterfaceTask(const Schematyc2::SGUID& scopeGUID, const char* szName)
{
	return m_delegate->AddAbstractInterfaceTask(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptAbstractInterfaceTask* CScriptFile::GetAbstractInterfaceTask(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetAbstractInterfaceTask(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptAbstractInterfaceTask* CScriptFile::GetAbstractInterfaceTask(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetAbstractInterfaceTask(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitAbstractInterfaceTasks(const Schematyc2::ScriptAbstractInterfaceTaskVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitAbstractInterfaceTasks(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitAbstractInterfaceTasks(const Schematyc2::ScriptAbstractInterfaceTaskConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitAbstractInterfaceTasks(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptClass* CScriptFile::AddClass(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& foundationGUID)
{
	return m_delegate->AddClass(scopeGUID, szName, foundationGUID);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptClass* CScriptFile::GetClass(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetClass(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptClass* CScriptFile::GetClass(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetClass(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitClasses(const Schematyc2::ScriptClassVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitClasses(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitClasses(const Schematyc2::ScriptClassConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitClasses(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptClassBase* CScriptFile::AddClassBase(const Schematyc2::SGUID& scopeGUID, const Schematyc2::SGUID& refGUID)
{
	return m_delegate->AddClassBase(scopeGUID, refGUID);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptClassBase* CScriptFile::GetClassBase(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetClassBase(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptClassBase* CScriptFile::GetClassBase(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetClassBase(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitClassBases(const Schematyc2::ScriptClassBaseVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitClassBases(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitClassBases(const Schematyc2::ScriptClassBaseConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitClassBases(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptStateMachine* CScriptFile::AddStateMachine(const Schematyc2::SGUID& scopeGUID, const char* szName, Schematyc2::EStateMachineLifetime lifetime, const Schematyc2::SGUID& contextGUID, const Schematyc2::SGUID& partnerGUID)
{
	return m_delegate->AddStateMachine(scopeGUID, szName, lifetime, contextGUID, partnerGUID);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptStateMachine* CScriptFile::GetStateMachine(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetStateMachine(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptStateMachine* CScriptFile::GetStateMachine(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetStateMachine(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitStateMachines(const Schematyc2::ScriptStateMachineVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitStateMachines(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitStateMachines(const Schematyc2::ScriptStateMachineConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitStateMachines(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptState* CScriptFile::AddState(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& partnerGUID)
{
	return m_delegate->AddState(scopeGUID, szName, partnerGUID);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptState* CScriptFile::GetState(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetState(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptState* CScriptFile::GetState(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetState(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitStates(const Schematyc2::ScriptStateVisitor& visitor)
{
	return m_delegate->VisitStates(visitor);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitStates(const Schematyc2::ScriptStateVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitStates(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitStates(const Schematyc2::ScriptStateConstVisitor& visitor) const
{
	return m_delegate->VisitStates(visitor);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitStates(const Schematyc2::ScriptStateConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitStates(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptVariable* CScriptFile::AddVariable(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::CAggregateTypeId& typeId)
{
	return m_delegate->AddVariable(scopeGUID, szName, typeId);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptVariable* CScriptFile::GetVariable(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetVariable(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptVariable* CScriptFile::GetVariable(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetVariable(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitVariables(const Schematyc2::ScriptVariableVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitVariables(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitVariables(const Schematyc2::ScriptVariableConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitVariables(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptProperty* CScriptFile::AddProperty(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& refGUID, const Schematyc2::CAggregateTypeId& typeId)
{
	return m_delegate->AddProperty(scopeGUID, szName, refGUID, typeId);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptProperty* CScriptFile::GetProperty(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetProperty(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptProperty* CScriptFile::GetProperty(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetProperty(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitProperties(const Schematyc2::ScriptPropertyVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitProperties(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitProperties(const Schematyc2::ScriptPropertyConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitProperties(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptContainer* CScriptFile::AddContainer(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& typeGUID)
{
	return m_delegate->AddContainer(scopeGUID, szName, typeGUID);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptContainer* CScriptFile::GetContainer(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetContainer(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptContainer* CScriptFile::GetContainer(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetContainer(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitContainers(const Schematyc2::ScriptContainerVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitContainers(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitContainers(const Schematyc2::ScriptContainerConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitContainers(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptTimer* CScriptFile::AddTimer(const Schematyc2::SGUID& scopeGUID, const char* szName)
{
	return m_delegate->AddTimer(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptTimer* CScriptFile::GetTimer(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetTimer(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptTimer* CScriptFile::GetTimer(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetTimer(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitTimers(const Schematyc2::ScriptTimerVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitTimers(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitTimers(const Schematyc2::ScriptTimerConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitTimers(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptAbstractInterfaceImplementation* CScriptFile::AddAbstractInterfaceImplementation(const Schematyc2::SGUID& scopeGUID, Schematyc2::EDomain domain, const Schematyc2::SGUID& refGUID)
{
	return m_delegate->AddAbstractInterfaceImplementation(scopeGUID, domain, refGUID);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptAbstractInterfaceImplementation* CScriptFile::GetAbstractInterfaceImplementation(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetAbstractInterfaceImplementation(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptAbstractInterfaceImplementation* CScriptFile::GetAbstractInterfaceImplementation(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetAbstractInterfaceImplementation(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitAbstractInterfaceImplementations(const Schematyc2::ScriptAbstractInterfaceImplementationVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitAbstractInterfaceImplementations(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitAbstractInterfaceImplementations(const Schematyc2::ScriptAbstractInterfaceImplementationConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitAbstractInterfaceImplementations(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptComponentInstance* CScriptFile::AddComponentInstance(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& componentGUID, Schematyc2::EScriptComponentInstanceFlags flags)
{
	return m_delegate->AddComponentInstance(scopeGUID, szName, componentGUID, flags);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptComponentInstance* CScriptFile::GetComponentInstance(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetComponentInstance(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptComponentInstance* CScriptFile::GetComponentInstance(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetComponentInstance(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitComponentInstances(const Schematyc2::ScriptComponentInstanceVisitor& visitor)
{
	return m_delegate->VisitComponentInstances(visitor);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitComponentInstances(const Schematyc2::ScriptComponentInstanceVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitComponentInstances(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitComponentInstances(const Schematyc2::ScriptComponentInstanceConstVisitor& visitor) const
{
	return m_delegate->VisitComponentInstances(visitor);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitComponentInstances(const Schematyc2::ScriptComponentInstanceConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitComponentInstances(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptActionInstance* CScriptFile::AddActionInstance(const Schematyc2::SGUID& scopeGUID, const char* szName, const Schematyc2::SGUID& actionGUID, const Schematyc2::SGUID& componentInstanceGUID)
{
	return m_delegate->AddActionInstance(scopeGUID, szName, actionGUID, componentInstanceGUID);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptActionInstance* CScriptFile::GetActionInstance(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetActionInstance(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptActionInstance* CScriptFile::GetActionInstance(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetActionInstance(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitActionInstances(const Schematyc2::ScriptActionInstanceVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitActionInstances(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitActionInstances(const Schematyc2::ScriptActionInstanceConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitActionInstances(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IDocGraph* CScriptFile::AddGraph(const Schematyc2::SScriptGraphParams& params)
{
	return m_delegate->AddGraph(params);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IDocGraph* CScriptFile::GetGraph(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetGraph(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IDocGraph* CScriptFile::GetGraph(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetGraph(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitGraphs(const Schematyc2::DocGraphVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitGraphs(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitGraphs(const Schematyc2::DocGraphConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitGraphs(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
void CScriptFile::RemoveElement(const Schematyc2::SGUID& guid, bool clearScope)
{
	return m_delegate->RemoveElement(guid, clearScope);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptElement* CScriptFile::GetElement(const Schematyc2::SGUID& guid)
{
	return m_delegate->GetElement(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptElement* CScriptFile::GetElement(const Schematyc2::SGUID& guid) const
{
	return m_delegate->GetElement(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptElement* CScriptFile::GetElement(const Schematyc2::SGUID& guid, Schematyc2::EScriptElementType elementType)
{
	return m_delegate->GetElement(guid, elementType);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptElement* CScriptFile::GetElement(const Schematyc2::SGUID& guid, Schematyc2::EScriptElementType elementType) const
{
	return m_delegate->GetElement(guid, elementType);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitElements(const Schematyc2::ScriptElementVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy)
{
	return m_delegate->VisitElements(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptFile::VisitElements(const Schematyc2::ScriptElementConstVisitor& visitor, const Schematyc2::SGUID& scopeGUID, bool bRecurseHierarchy) const
{
	return m_delegate->VisitElements(visitor, scopeGUID, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptFile::IsElementNameUnique(const Schematyc2::SGUID& scopeGUID, const char* szName) const
{
	return m_delegate->IsElementNameUnique(scopeGUID, szName);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CScriptFile::CopyElementsToXml(const Schematyc2::SGUID& guid, bool bRecurseHierarchy) const
{
	return m_delegate->CopyElementsToXml(guid, bRecurseHierarchy);
}

//////////////////////////////////////////////////////////////////////////
void CScriptFile::PasteElementsFromXml(const Schematyc2::SGUID& scopeGUID, const XmlNodeRef& xml)
{
	return m_delegate->PasteElementsFromXml(scopeGUID, xml);
}

//////////////////////////////////////////////////////////////////////////
void CScriptFile::Load()
{
	m_delegate->Load();
}

//////////////////////////////////////////////////////////////////////////
void CScriptFile::Save()
{
	m_delegate->Save();
}

//////////////////////////////////////////////////////////////////////////
void CScriptFile::Refresh(const Schematyc2::SScriptRefreshParams& params)
{
	m_delegate->Refresh(params);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptFile::GetClipboardInfo(const XmlNodeRef& xml, Schematyc2::SScriptElementClipboardInfo& clipboardInfo) const
{
	return m_delegate->GetClipboardInfo(xml, clipboardInfo);
}

}