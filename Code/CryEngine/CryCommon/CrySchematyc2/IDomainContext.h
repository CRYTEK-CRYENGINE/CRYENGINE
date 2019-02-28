// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Do we need to replace stack_string with CharArrayView?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>

#include "CrySchematyc2/GUID.h"

namespace Schematyc2
{
	struct IAbstractInterface;
	struct IAbstractInterfaceFunction;
	struct IActionMemberFunction;
	struct IComponentFactory;
	struct IComponentMemberFunction;
	struct IDocGraph;
	struct IEnvFunctionDescriptor;
	struct IFoundation;
	struct IGlobalFunction;
	struct IScriptActionInstance;
	struct IScriptClass;
	struct IScriptComponentInstance;
	struct IScriptElement;
	struct IScriptEnumeration;
	struct IScriptFile;
	struct IScriptProperty;
	struct IScriptState;
	struct IScriptStateMachine;
	struct IScriptVariable;

	DECLARE_SHARED_POINTERS(IAbstractInterface)
	DECLARE_SHARED_POINTERS(IAbstractInterfaceFunction)
	DECLARE_SHARED_POINTERS(IActionMemberFunction)
	DECLARE_SHARED_POINTERS(IComponentFactory)
	DECLARE_SHARED_POINTERS(IComponentMemberFunction)
	DECLARE_SHARED_POINTERS(IFoundation)
	DECLARE_SHARED_POINTERS(IGlobalFunction)

	typedef TemplateUtils::CDelegate<EVisitStatus (const IEnvFunctionDescriptor&)>           EnvFunctionVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IGlobalFunctionConstPtr&)>          EnvGlobalFunctionVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IEnvFunctionDescriptor&)>           EnvMemberFunctionVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IAbstractInterfaceConstPtr&)>       EnvAbstractInterfaceVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IComponentFactoryConstPtr&)>        EnvComponentFactoryVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IComponentMemberFunctionConstPtr&)> EnvComponentMemberFunctionVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IActionMemberFunctionConstPtr&)>    EnvActionMemberFunctionVisitor;

	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptEnumeration&)>       ScriptEnumerationConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptClass&)>             ScriptClassConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptState&)>             ScriptStateConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptStateMachine&)>      ScriptStateMachineConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptVariable&)>          ScriptVariableConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptProperty&)>          ScriptPropertyConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptComponentInstance&)> ScriptComponentInstanceConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IScriptActionInstance&)>    ScriptActionInstanceConstVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IDocGraph&)>                DocGraphConstVisitor;

	struct SDomainContextScope
	{
		inline SDomainContextScope(const IScriptFile* _pScriptFile = nullptr, const SGUID& _guid = SGUID())
			: pScriptFile(_pScriptFile)
			, guid(_guid)
		{}

		const IScriptFile* pScriptFile;
		SGUID              guid;
	};

	enum class EDomainScope
	{
		Local,   // Everything in local class.
		Derived, // Everything in local class and everything with public or protected accessor in base classes.
		All      // Everything in local class and everything in base classes.
	};

	enum class EDomainQualifier
	{
		Local,
		Global
	};

	struct IDomainContext
	{
		virtual ~IDomainContext() {}

		virtual const SDomainContextScope& GetScope() const = 0;
		virtual IFoundationConstPtr GetEnvFoundation() const = 0;
		virtual const IScriptClass* GetScriptClass() const = 0;

		// #SchematycTODO : Do we need concept of all or usable for env elements?

		virtual void VisitEnvFunctions(const EnvFunctionVisitor& visitor) const = 0;
		virtual void VisitEnvGlobalFunctions(const EnvGlobalFunctionVisitor& visitor) const = 0;
		virtual void VisitEnvAbstractInterfaces(const EnvAbstractInterfaceVisitor& visitor) const = 0;
		virtual void VisitEnvComponentFactories(const EnvComponentFactoryVisitor& visitor) const = 0;
		virtual void VisitEnvComponentMemberFunctions(const EnvComponentMemberFunctionVisitor& visitor) const = 0;
		virtual void VisitEnvActionMemberFunctions(const EnvActionMemberFunctionVisitor& visitor) const = 0;
		
		virtual void VisitScriptEnumerations(const ScriptEnumerationConstVisitor& visitor, EDomainScope scope) const = 0;
		virtual void VisitScriptStates(const ScriptStateConstVisitor& visitor, EDomainScope scope) const = 0;
		virtual void VisitScriptStateMachines(const ScriptStateMachineConstVisitor& visitor, EDomainScope scope) const = 0;
		virtual void VisitScriptVariables(const ScriptVariableConstVisitor& visitor, EDomainScope scope) const = 0;
		virtual void VisitScriptProperties(const ScriptPropertyConstVisitor& visitor, EDomainScope scope) const = 0;
		virtual void VisitScriptComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, EDomainScope scope) const = 0;
		virtual void VisitScriptActionInstances(const ScriptActionInstanceConstVisitor& visitor, EDomainScope scope) const = 0;
		virtual void VisitDocGraphs(const DocGraphConstVisitor& visitor, EDomainScope scope) const = 0;
		
		virtual const IScriptStateMachine* GetScriptStateMachine(const SGUID& guid) const = 0;
		virtual const IScriptComponentInstance* GetScriptComponentInstance(const SGUID& guid) const = 0;
		virtual const IScriptActionInstance* GetScriptActionInstance(const SGUID& guid) const = 0;
		virtual const IDocGraph* GetDocGraph(const SGUID& guid) const = 0;

		virtual bool QualifyName(const IGlobalFunction& envGlobalFunction, stack_string& output) const = 0;
		virtual bool QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IEnvFunctionDescriptor& envFunctionDescriptor, EDomainQualifier qualifier, stack_string& output) const = 0;
		virtual bool QualifyName(const IAbstractInterface& envAbstractInterface, stack_string& output) const = 0;
		virtual bool QualifyName(const IAbstractInterfaceFunction& envAbstractInterfaceFunction, stack_string& output) const = 0;
		virtual bool QualifyName(const IComponentFactory& envComponentFactory, stack_string& output) const = 0;
		virtual bool QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IComponentMemberFunction& envComponentMemberFunction, EDomainQualifier qualifier, stack_string& output) const = 0;
		virtual bool QualifyName(const IScriptActionInstance& scriptActionInstance, const IActionMemberFunction& envActionMemberFunction, EDomainQualifier qualifier, stack_string& output) const = 0;
		virtual bool QualifyName(const IScriptElement& scriptElement, EDomainQualifier qualifier, stack_string& output) const = 0;
	};

	DECLARE_SHARED_POINTERS(IDomainContext)
}
