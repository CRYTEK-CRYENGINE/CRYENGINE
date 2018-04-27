// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// IScripView is a utility interface intended to define the rules of what is visible/available to a given scope within a script
// e.g. when advertising nodes in a graph we use the script view to determine which types, functions and signals are available to the owner of that graph.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/EnumFlags.h"
#include "CrySchematyc/Utils/GUID.h"
#include "CrySchematyc/Utils/IString.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IEnvAction;
struct IEnvClass;
struct IEnvComponent;
struct IEnvDataType;
struct IEnvElement;
struct IEnvFunction;
struct IEnvInterface;
struct IEnvInterfaceFunction;
struct IEnvSignal;
struct IScriptActionInstance;
struct IScriptClass;
struct IScriptComponentInstance;
struct IScriptElement;
struct IScriptEnum;
struct IScriptFunction;
struct IScriptModule;
struct IScriptSignal;
struct IScriptState;
struct IScriptStateMachine;
struct IScriptStruct;
struct IScriptTimer;
struct IScriptVariable;

typedef std::function<EVisitStatus(const IEnvDataType&)>             EnvDataTypeConstVisitor;
typedef std::function<EVisitStatus(const IEnvSignal&)>               EnvSignalConstVisitor;
typedef std::function<EVisitStatus(const IEnvFunction&)>             EnvFunctionConstVisitor;
typedef std::function<EVisitStatus(const IEnvClass&)>                EnvClassConstVisitor;
typedef std::function<EVisitStatus(const IEnvInterface&)>            EnvInterfaceConstVisitor;
typedef std::function<EVisitStatus(const IEnvAction&)>               EnvActionConstVisitor;
typedef std::function<EVisitStatus(const IEnvComponent&)>            EnvComponentConstVisitor;

typedef std::function<EVisitStatus(const IScriptModule&)>            ScriptModuleConstVisitor;
typedef std::function<void (const IScriptEnum&)>                     ScriptEnumConstVisitor;
typedef std::function<void (const IScriptStruct&)>                   ScriptStructConstVisitor;
typedef std::function<void (const IScriptSignal&)>                   ScriptSignalConstVisitor;
typedef std::function<EVisitStatus(const IScriptFunction&)>          ScriptFunctionConstVisitor;
typedef std::function<EVisitStatus(const IScriptClass&)>             ScriptClassConstVisitor;
typedef std::function<void (const IScriptState&)>                    ScriptStateConstVisitor;
typedef std::function<EVisitStatus(const IScriptStateMachine&)>      ScriptStateMachineConstVisitor;
typedef std::function<EVisitStatus(const IScriptVariable&)>          ScriptVariableConstVisitor;
typedef std::function<void (const IScriptTimer&)>                    ScriptTimerConstVisitor;
typedef std::function<EVisitStatus(const IScriptComponentInstance&)> ScriptComponentInstanceConstVisitor;
typedef std::function<EVisitStatus(const IScriptActionInstance&)>    ScriptActionInstanceConstVisitor;

typedef std::function<EVisitStatus(const IScriptFunction&)>          ScriptModuleFunctionsConstVisitor;
typedef std::function<EVisitStatus(const IScriptVariable&)>          ScriptModuleVariablesConstVisitor;
typedef std::function<EVisitStatus(const IScriptSignal&)>            ScriptModuleSignalsConstVisitor;

enum class EDomainScope
{
	// #SchematycTODO : Remove 'All'? It's only used internally and kinda defeats the whole purpose of the script view class.

	Local,     // Everything local to scope.
	Derived,   // Everything local to scope and everything with public or protected accessor in derived scopes.
	All        // Everything local to scope and everything in derived scopes.
};

enum class EDomainQualifier
{
	Local,
	Global
};

struct IScriptView
{
	virtual ~IScriptView() {}

	virtual const CryGUID&                  GetScopeGUID() const = 0; // #SchematycTODO : Scope using element instead of GUID?
	virtual const IEnvClass*                GetEnvClass() const = 0;
	virtual const IScriptClass*             GetScriptClass() const = 0;

	virtual void                VisitEnvDataTypes(const EnvDataTypeConstVisitor& visitor) const = 0;               // #SchematycTODO : Move to IEnvContext/View?
	virtual void                VisitEnvSignals(const EnvSignalConstVisitor& visitor) const = 0;                   // #SchematycTODO : Move to IEnvContext/View?
	virtual void                VisitEnvFunctions(const EnvFunctionConstVisitor& visitor) const = 0;               // #SchematycTODO : Move to IEnvContext/View?
	virtual void                VisitEnvClasses(const EnvClassConstVisitor& visitor) const = 0;                    // #SchematycTODO : Move to IEnvContext/View?
	virtual void                VisitEnvInterfaces(const EnvInterfaceConstVisitor& visitor) const = 0;             // #SchematycTODO : Move to IEnvContext/View?
	virtual void                VisitEnvComponents(const EnvComponentConstVisitor& visitor) const = 0;             // #SchematycTODO : Move to IEnvContext/View?
	virtual void                VisitEnvActions(const EnvActionConstVisitor& visitor) const = 0;                   // #SchematycTODO : Move to IEnvContext/View?

	virtual void                VisitScriptModules(const ScriptModuleConstVisitor& visitor) const = 0;             // #SchematycTODO : Remove!!!
	virtual void                VisitScriptFunctions(const ScriptFunctionConstVisitor& visitor) const = 0;
	virtual void                VisitScriptClasses(const ScriptClassConstVisitor& visitor, EDomainScope scope) const = 0;
	//virtual void                            VisitScriptStateMachines(const ScriptStateMachineConstVisitor& visitor, EDomainScope scope) const = 0;
	virtual void                VisitScriptVariables(const ScriptVariableConstVisitor& visitor, EDomainScope scope) const = 0;
	//virtual void                            VisitInterfaceImpls(const ScriptTimerConstVisitor& visitor, EDomainScope scope) const = 0;
	virtual void                VisitScriptComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, EDomainScope scope) const = 0;
	virtual void                VisitScriptActionInstances(const ScriptActionInstanceConstVisitor& visitor, EDomainScope scope) const = 0;

	// #SchematycTODO : Can't we just access the script registry directly instead of calling VisitEnclosed? Or should we remove visit functionality from script registry? IScriptRegistry::CreateView?
	virtual void                            VisitEnclosedEnums(const ScriptEnumConstVisitor& visitor) const = 0;
	virtual void                            VisitAccesibleEnums(const ScriptEnumConstVisitor& visitor) const = 0;
	virtual void                            VisitEnclosedStructs(const ScriptStructConstVisitor& visitor) const = 0;
	virtual void                            VisitAccesibleStructs(const ScriptStructConstVisitor& visitor) const = 0;
	virtual void                            VisitEnclosedSignals(const ScriptSignalConstVisitor& visitor) const = 0;
	virtual void                            VisitAccesibleSignals(const ScriptSignalConstVisitor& visitor) const = 0;
	virtual void                            VisitEnclosedStates(const ScriptStateConstVisitor& visitor) const = 0;
	virtual void                            VisitAccesibleStates(const ScriptStateConstVisitor& visitor) const = 0;
	virtual void                            VisitEnclosedTimers(const ScriptTimerConstVisitor& visitor) const = 0;
	virtual void                            VisitAccesibleTimers(const ScriptTimerConstVisitor& visitor) const = 0;

	virtual const IScriptStateMachine*      GetScriptStateMachine(const CryGUID& guid) const = 0;      // #SchematycTODO : Can't we just access the script registry directly?
	virtual const IScriptComponentInstance* GetScriptComponentInstance(const CryGUID& guid) const = 0; // #SchematycTODO : Can't we just access the script registry directly?
	virtual const IScriptActionInstance*    GetScriptActionInstance(const CryGUID& guid) const = 0;    // #SchematycTODO : Can't we just access the script registry directly?
	virtual const IScriptElement*           GetScriptElement(const CryGUID& guid) const = 0;           // #SchematycTODO : Can't we just access the script registry directly?

	virtual bool                            QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IEnvFunction& envFunction, EDomainQualifier qualifier, IString& output) const = 0; // #SchematycTODO : Do we really need to pass a script component instance here? Shouldn't this be inherent from scope?
	virtual bool                            QualifyName(const IEnvClass& envClass, IString& output) const = 0;                                                                                            // #SchematycTODO : Does this really belong here?
	virtual bool                            QualifyName(const IEnvInterface& envInterface, IString& output) const = 0;                                                                                    // #SchematycTODO : Does this really belong here?
	virtual bool                            QualifyName(const IEnvInterfaceFunction& envInterfaceFunction, IString& output) const = 0;                                                                    // #SchematycTODO : Does this really belong here?
	virtual bool                            QualifyName(const IEnvComponent& envComponent, IString& output) const = 0;                                                                                    // #SchematycTODO : Does this really belong here?
	virtual void                            QualifyName(const IEnvElement& envElement, IString& output) const = 0;                                                                                        // #SchematycTODO : Does this really belong here?
	virtual bool                            QualifyName(const IScriptElement& scriptElement, EDomainQualifier qualifier, IString& output) const = 0;                                                      // #SchematycTODO : Does this really belong here?

	//virtual bool                            QualifyName(const IScriptElement& scriptElement, EScriptElementAccessor accessor, IString& output) const = 0;
};

DECLARE_SHARED_POINTERS(IScriptView)
} // Schematyc
