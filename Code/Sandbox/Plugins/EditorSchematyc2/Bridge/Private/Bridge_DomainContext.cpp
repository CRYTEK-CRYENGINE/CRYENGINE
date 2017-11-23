// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Bridge_DomainContext.h"

namespace Bridge {

//////////////////////////////////////////////////////////////////////////
CDomainContext::CDomainContext(Schematyc2::IDomainContextPtr context, const SDomainContextScope& scope)
	:   m_delegate(context)
	,   m_scope(scope)
{
	CRY_ASSERT(m_delegate);
}

//////////////////////////////////////////////////////////////////////////
const SDomainContextScope& CDomainContext::GetScope() const
{
	return m_scope;
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IFoundationConstPtr CDomainContext::GetEnvFoundation() const
{
	return Delegate()->GetEnvFoundation();
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptClass* CDomainContext::GetScriptClass() const
{
	return Delegate()->GetScriptClass();
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitEnvFunctions(const Schematyc2::EnvFunctionVisitor& visitor) const
{
	Delegate()->VisitEnvFunctions(visitor);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitEnvGlobalFunctions(const Schematyc2::EnvGlobalFunctionVisitor& visitor) const
{
	Delegate()->VisitEnvGlobalFunctions(visitor);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitEnvAbstractInterfaces(const Schematyc2::EnvAbstractInterfaceVisitor& visitor) const
{
	Delegate()->VisitEnvAbstractInterfaces(visitor);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitEnvComponentFactories(const Schematyc2::EnvComponentFactoryVisitor& visitor) const
{
	Delegate()->VisitEnvComponentFactories(visitor);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitEnvComponentMemberFunctions(const Schematyc2::EnvComponentMemberFunctionVisitor& visitor) const
{
	Delegate()->VisitEnvComponentMemberFunctions(visitor);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitEnvActionMemberFunctions(const Schematyc2::EnvActionMemberFunctionVisitor& visitor) const
{
	Delegate()->VisitEnvActionMemberFunctions(visitor);
}

void CDomainContext::VisitScriptEnumerations(const Schematyc2::ScriptEnumerationConstVisitor& visitor, Schematyc2::EDomainScope scope) const
{
	Delegate()->VisitScriptEnumerations(visitor, scope);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitScriptStates(const Schematyc2::ScriptStateConstVisitor& visitor, Schematyc2::EDomainScope scope) const
{
	Delegate()->VisitScriptStates(visitor, scope);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitScriptStateMachines(const Schematyc2::ScriptStateMachineConstVisitor& visitor, Schematyc2::EDomainScope scope) const
{
	Delegate()->VisitScriptStateMachines(visitor, scope);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitScriptVariables(const Schematyc2::ScriptVariableConstVisitor& visitor, Schematyc2::EDomainScope scope) const
{
	Delegate()->VisitScriptVariables(visitor, scope);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitScriptProperties(const Schematyc2::ScriptPropertyConstVisitor& visitor, Schematyc2::EDomainScope scope) const
{
	Delegate()->VisitScriptProperties(visitor, scope);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitScriptComponentInstances(const Schematyc2::ScriptComponentInstanceConstVisitor& visitor, Schematyc2::EDomainScope scope) const
{
	Delegate()->VisitScriptComponentInstances(visitor, scope);
}

//////////////////////////////////////////////////////////////////////////
void CDomainContext::VisitScriptActionInstances(const Schematyc2::ScriptActionInstanceConstVisitor& visitor, Schematyc2::EDomainScope scope) const
{
	Delegate()->VisitScriptActionInstances(visitor, scope);
}

void CDomainContext::VisitDocGraphs(const Schematyc2::DocGraphConstVisitor& visitor, Schematyc2::EDomainScope scope) const
{
	Delegate()->VisitDocGraphs(visitor, scope);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptStateMachine* CDomainContext::GetScriptStateMachine(const Schematyc2::SGUID& guid) const
{
	return Delegate()->GetScriptStateMachine(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptComponentInstance* CDomainContext::GetScriptComponentInstance(const Schematyc2::SGUID& guid) const
{
	return Delegate()->GetScriptComponentInstance(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IScriptActionInstance* CDomainContext::GetScriptActionInstance(const Schematyc2::SGUID& guid) const
{
	return Delegate()->GetScriptActionInstance(guid);
}

//////////////////////////////////////////////////////////////////////////
const Schematyc2::IDocGraph* CDomainContext::GetDocGraph(const Schematyc2::SGUID& guid) const
{
	return Delegate()->GetDocGraph(guid);
}

//////////////////////////////////////////////////////////////////////////
bool CDomainContext::QualifyName(const Schematyc2::IGlobalFunction& envGlobalFunction, stack_string& output) const
{
	return Delegate()->QualifyName(envGlobalFunction, output);
}

//////////////////////////////////////////////////////////////////////////
bool CDomainContext::QualifyName(const Schematyc2::IScriptComponentInstance& scriptComponentInstance, const Schematyc2::IEnvFunctionDescriptor& envFunctionDescriptor, Schematyc2::EDomainQualifier qualifier, stack_string& output) const
{
	return Delegate()->QualifyName(scriptComponentInstance, envFunctionDescriptor, qualifier, output);
}

//////////////////////////////////////////////////////////////////////////
bool CDomainContext::QualifyName(const Schematyc2::IAbstractInterface& envAbstractInterface, stack_string& output) const
{
	return Delegate()->QualifyName(envAbstractInterface, output);
}

//////////////////////////////////////////////////////////////////////////
bool CDomainContext::QualifyName(const Schematyc2::IAbstractInterfaceFunction& envAbstractInterfaceFunction, stack_string& output) const
{
	return Delegate()->QualifyName(envAbstractInterfaceFunction, output);
}

//////////////////////////////////////////////////////////////////////////
bool CDomainContext::QualifyName(const Schematyc2::IComponentFactory& envComponentFactory, stack_string& output) const
{
	return Delegate()->QualifyName(envComponentFactory, output);
}

//////////////////////////////////////////////////////////////////////////
bool CDomainContext::QualifyName(const Schematyc2::IScriptComponentInstance& scriptComponentInstance, const Schematyc2::IComponentMemberFunction& envComponentMemberFunction, Schematyc2::EDomainQualifier qualifier, stack_string& output) const
{
	return Delegate()->QualifyName(scriptComponentInstance, envComponentMemberFunction, qualifier, output);
}

//////////////////////////////////////////////////////////////////////////
bool CDomainContext::QualifyName(const Schematyc2::IScriptActionInstance& scriptActionInstance, const Schematyc2::IActionMemberFunction& envActionMemberFunction, Schematyc2::EDomainQualifier qualifier, stack_string& output) const
{
	return Delegate()->QualifyName(scriptActionInstance, envActionMemberFunction, qualifier, output);
}

//////////////////////////////////////////////////////////////////////////
bool CDomainContext::QualifyName(const Schematyc2::IScriptElement& scriptElement, Schematyc2::EDomainQualifier qualifier, stack_string& output) const
{
	return Delegate()->QualifyName(scriptElement, qualifier, output);
}

}