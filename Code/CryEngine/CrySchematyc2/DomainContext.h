// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/IDomainContext.h>

namespace Schematyc2
{
	class CDomainContext : public IDomainContext
	{
	public:

		CDomainContext(const SDomainContextScope& scope);

		// IDomainContext

		virtual const SDomainContextScope& GetScope() const override;
		virtual IFoundationConstPtr GetEnvFoundation() const override;
		virtual const IScriptClass* GetScriptClass() const override;

		virtual void VisitEnvFunctions(const EnvFunctionVisitor& visitor) const override;
		virtual void VisitEnvGlobalFunctions(const EnvGlobalFunctionVisitor& visitor) const override;
		virtual void VisitEnvAbstractInterfaces(const EnvAbstractInterfaceVisitor& visitor) const override;
		virtual void VisitEnvComponentFactories(const EnvComponentFactoryVisitor& visitor) const override;
		virtual void VisitEnvComponentMemberFunctions(const EnvComponentMemberFunctionVisitor& visitor) const override;
		virtual void VisitEnvActionMemberFunctions(const EnvActionMemberFunctionVisitor& visitor) const override;
		
		virtual void VisitScriptEnumerations(const ScriptEnumerationConstVisitor& visitor, EDomainScope scope) const override;
		virtual void VisitScriptStates(const ScriptStateConstVisitor& visitor, EDomainScope scope) const override;
		virtual void VisitScriptStateMachines(const ScriptStateMachineConstVisitor& visitor, EDomainScope scope) const override;
		virtual void VisitScriptVariables(const ScriptVariableConstVisitor& visitor, EDomainScope scope) const override;
		virtual void VisitScriptProperties(const ScriptPropertyConstVisitor& visitor, EDomainScope scope) const override;
		virtual void VisitScriptComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, EDomainScope scope) const override;
		virtual void VisitScriptActionInstances(const ScriptActionInstanceConstVisitor& visitor, EDomainScope scope) const override;
		virtual void VisitDocGraphs(const DocGraphConstVisitor& visitor, EDomainScope scope) const override;

		virtual const IScriptStateMachine* GetScriptStateMachine(const SGUID& guid) const override;
		virtual const IScriptComponentInstance* GetScriptComponentInstance(const SGUID& guid) const override;
		virtual const IScriptActionInstance* GetScriptActionInstance(const SGUID& guid) const override;
		virtual const IDocGraph* GetDocGraph(const SGUID& guid) const override;

		virtual bool QualifyName(const IGlobalFunction& envGlobalFunction, stack_string& output) const override;
		virtual bool QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IEnvFunctionDescriptor& envFunctionDescriptor, EDomainQualifier qualifier, stack_string& output) const override;
		virtual bool QualifyName(const IAbstractInterface& envAbstractInterface, stack_string& output) const override;
		virtual bool QualifyName(const IAbstractInterfaceFunction& envAbstractInterfaceFunction, stack_string& output) const override;
		virtual bool QualifyName(const IComponentFactory& envComponentFactory, stack_string& output) const override;
		virtual bool QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IComponentMemberFunction& envComponentMemberFunction, EDomainQualifier qualifier, stack_string& output) const override;
		virtual bool QualifyName(const IScriptActionInstance& scriptActionInstance, const IActionMemberFunction& envActionMemberFunction, EDomainQualifier qualifier, stack_string& output) const override;
		virtual bool QualifyName(const IScriptElement& scriptElement, EDomainQualifier qualifier, stack_string& output) const override;

		// ~IDomainContext

	private:

		SDomainContextScope m_scope;
	};
}
