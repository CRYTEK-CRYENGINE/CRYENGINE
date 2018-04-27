// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Do we need to replace stack_string with CharArrayView?

#pragma once

#include "CrySchematyc2/IDomainContext.h"

namespace Bridge {

struct IScriptFile;

struct SDomainContextScope
{
	inline SDomainContextScope(const IScriptFile* _pScriptFile = nullptr, const Schematyc2::SGUID& _guid = Schematyc2::SGUID())
		: pScriptFile(_pScriptFile)
		, guid(_guid)
	{}

	const IScriptFile* pScriptFile;
	Schematyc2::SGUID  guid;
};

struct IDomainContext
{
	virtual ~IDomainContext() {}

	virtual const SDomainContextScope& GetScope() const = 0;
	virtual Schematyc2::IFoundationConstPtr GetEnvFoundation() const = 0;
	virtual const Schematyc2::IScriptClass* GetScriptClass() const = 0;

	// #SchematycTODO : Do we need concept of all or usable for env elements?

	virtual void VisitEnvFunctions(const Schematyc2::EnvFunctionVisitor& visitor) const = 0;
	virtual void VisitEnvGlobalFunctions(const Schematyc2::EnvGlobalFunctionVisitor& visitor) const = 0;
	virtual void VisitEnvAbstractInterfaces(const Schematyc2::EnvAbstractInterfaceVisitor& visitor) const = 0;
	virtual void VisitEnvComponentFactories(const Schematyc2::EnvComponentFactoryVisitor& visitor) const = 0;
	virtual void VisitEnvComponentMemberFunctions(const Schematyc2::EnvComponentMemberFunctionVisitor& visitor) const = 0;
	virtual void VisitEnvActionMemberFunctions(const Schematyc2::EnvActionMemberFunctionVisitor& visitor) const = 0;
		
	virtual void VisitScriptEnumerations(const Schematyc2::ScriptEnumerationConstVisitor& visitor, Schematyc2::EDomainScope scope) const = 0;
	virtual void VisitScriptStates(const Schematyc2::ScriptStateConstVisitor& visitor, Schematyc2::EDomainScope scope) const = 0;
	virtual void VisitScriptStateMachines(const Schematyc2::ScriptStateMachineConstVisitor& visitor, Schematyc2::EDomainScope scope) const = 0;
	virtual void VisitScriptVariables(const Schematyc2::ScriptVariableConstVisitor& visitor, Schematyc2::EDomainScope scope) const = 0;
	virtual void VisitScriptProperties(const Schematyc2::ScriptPropertyConstVisitor& visitor, Schematyc2::EDomainScope scope) const = 0;
	virtual void VisitScriptComponentInstances(const Schematyc2::ScriptComponentInstanceConstVisitor& visitor, Schematyc2::EDomainScope scope) const = 0;
	virtual void VisitScriptActionInstances(const Schematyc2::ScriptActionInstanceConstVisitor& visitor, Schematyc2::EDomainScope scope) const = 0;
	virtual void VisitDocGraphs(const Schematyc2::DocGraphConstVisitor& visitor, Schematyc2::EDomainScope scope) const = 0;
		
	virtual const Schematyc2::IScriptStateMachine* GetScriptStateMachine(const Schematyc2::SGUID& guid) const = 0;
	virtual const Schematyc2::IScriptComponentInstance* GetScriptComponentInstance(const Schematyc2::SGUID& guid) const = 0;
	virtual const Schematyc2::IScriptActionInstance* GetScriptActionInstance(const Schematyc2::SGUID& guid) const = 0;
	virtual const Schematyc2::IDocGraph* GetDocGraph(const Schematyc2::SGUID& guid) const = 0;

	virtual bool QualifyName(const Schematyc2::IGlobalFunction& envGlobalFunction, stack_string& output) const = 0;
	virtual bool QualifyName(const Schematyc2::IScriptComponentInstance& scriptComponentInstance, const Schematyc2::IEnvFunctionDescriptor& envFunctionDescriptor, Schematyc2::EDomainQualifier qualifier, stack_string& output) const = 0;
	virtual bool QualifyName(const Schematyc2::IAbstractInterface& envAbstractInterface, stack_string& output) const = 0;
	virtual bool QualifyName(const Schematyc2::IAbstractInterfaceFunction& envAbstractInterfaceFunction, stack_string& output) const = 0;
	virtual bool QualifyName(const Schematyc2::IComponentFactory& envComponentFactory, stack_string& output) const = 0;
	virtual bool QualifyName(const Schematyc2::IScriptComponentInstance& scriptComponentInstance, const Schematyc2::IComponentMemberFunction& envComponentMemberFunction, Schematyc2::EDomainQualifier qualifier, stack_string& output) const = 0;
	virtual bool QualifyName(const Schematyc2::IScriptActionInstance& scriptActionInstance, const Schematyc2::IActionMemberFunction& envActionMemberFunction, Schematyc2::EDomainQualifier qualifier, stack_string& output) const = 0;
	virtual bool QualifyName(const Schematyc2::IScriptElement& scriptElement, Schematyc2::EDomainQualifier qualifier, stack_string& output) const = 0;
};

DECLARE_SHARED_POINTERS(IDomainContext)

}
