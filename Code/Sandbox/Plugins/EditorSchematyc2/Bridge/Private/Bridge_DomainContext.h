// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Bridge_IDomainContext.h"

namespace Schematyc2 {

struct IDomainContext;

}

namespace Bridge {

class CDomainContext : public IDomainContext
{
public:

	CDomainContext(Schematyc2::IDomainContextPtr context, const SDomainContextScope& scope);

public:
	inline Schematyc2::IDomainContextPtr Delegate() const { return m_delegate; }

	// IDomainContext

	virtual const SDomainContextScope& GetScope() const override;
	virtual Schematyc2::IFoundationConstPtr GetEnvFoundation() const override;
	virtual const Schematyc2::IScriptClass* GetScriptClass() const override;

	virtual void VisitEnvFunctions(const Schematyc2::EnvFunctionVisitor& visitor) const override;
	virtual void VisitEnvGlobalFunctions(const Schematyc2::EnvGlobalFunctionVisitor& visitor) const override;
	virtual void VisitEnvAbstractInterfaces(const Schematyc2::EnvAbstractInterfaceVisitor& visitor) const override;
	virtual void VisitEnvComponentFactories(const Schematyc2::EnvComponentFactoryVisitor& visitor) const override;
	virtual void VisitEnvComponentMemberFunctions(const Schematyc2::EnvComponentMemberFunctionVisitor& visitor) const override;
	virtual void VisitEnvActionMemberFunctions(const Schematyc2::EnvActionMemberFunctionVisitor& visitor) const override;

	virtual void VisitScriptEnumerations(const Schematyc2::ScriptEnumerationConstVisitor& visitor, Schematyc2::EDomainScope scope) const override;
	virtual void VisitScriptStates(const Schematyc2::ScriptStateConstVisitor& visitor, Schematyc2::EDomainScope scope) const override;
	virtual void VisitScriptStateMachines(const Schematyc2::ScriptStateMachineConstVisitor& visitor, Schematyc2::EDomainScope scope) const override;
	virtual void VisitScriptVariables(const Schematyc2::ScriptVariableConstVisitor& visitor, Schematyc2::EDomainScope scope) const override;
	virtual void VisitScriptProperties(const Schematyc2::ScriptPropertyConstVisitor& visitor, Schematyc2::EDomainScope scope) const override;
	virtual void VisitScriptComponentInstances(const Schematyc2::ScriptComponentInstanceConstVisitor& visitor, Schematyc2::EDomainScope scope) const override;
	virtual void VisitScriptActionInstances(const Schematyc2::ScriptActionInstanceConstVisitor& visitor, Schematyc2::EDomainScope scope) const override;
	virtual void VisitDocGraphs(const Schematyc2::DocGraphConstVisitor& visitor, Schematyc2::EDomainScope scope) const override;

	virtual const Schematyc2::IScriptStateMachine* GetScriptStateMachine(const Schematyc2::SGUID& guid) const override;
	virtual const Schematyc2::IScriptComponentInstance* GetScriptComponentInstance(const Schematyc2::SGUID& guid) const override;
	virtual const Schematyc2::IScriptActionInstance* GetScriptActionInstance(const Schematyc2::SGUID& guid) const override;
	virtual const Schematyc2::IDocGraph* GetDocGraph(const Schematyc2::SGUID& guid) const override;

	virtual bool QualifyName(const Schematyc2::IGlobalFunction& envGlobalFunction, stack_string& output) const override;
	virtual bool QualifyName(const Schematyc2::IScriptComponentInstance& scriptComponentInstance, const Schematyc2::IEnvFunctionDescriptor& envFunctionDescriptor, Schematyc2::EDomainQualifier qualifier, stack_string& output) const override;
	virtual bool QualifyName(const Schematyc2::IAbstractInterface& envAbstractInterface, stack_string& output) const override;
	virtual bool QualifyName(const Schematyc2::IAbstractInterfaceFunction& envAbstractInterfaceFunction, stack_string& output) const override;
	virtual bool QualifyName(const Schematyc2::IComponentFactory& envComponentFactory, stack_string& output) const override;
	virtual bool QualifyName(const Schematyc2::IScriptComponentInstance& scriptComponentInstance, const Schematyc2::IComponentMemberFunction& envComponentMemberFunction, Schematyc2::EDomainQualifier qualifier, stack_string& output) const override;
	virtual bool QualifyName(const Schematyc2::IScriptActionInstance& scriptActionInstance, const Schematyc2::IActionMemberFunction& envActionMemberFunction, Schematyc2::EDomainQualifier qualifier, stack_string& output) const override;
	virtual bool QualifyName(const Schematyc2::IScriptElement& scriptElement, Schematyc2::EDomainQualifier qualifier, stack_string& output) const override;

	// ~IDomainContext       

private:
	SDomainContextScope           m_scope;
	Schematyc2::IDomainContextPtr m_delegate;
};

}