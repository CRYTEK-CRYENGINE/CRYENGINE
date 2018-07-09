// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Maybe we don't need to mark/store persistent timers, actions etc. If they can be turned on/off we can just default to on and turn off if they belong to a state.
// #SchematycTODO : Simplify structure and compilation by giving lib classes a default action?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_ArrayView.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>

#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/IAny.h"
#include "CrySchematyc2/ISignal.h"
#include "CrySchematyc2/Deprecated/Variant.h"
#include "CrySchematyc2/Env/IEnvRegistry.h"
#include "CrySchematyc2/Runtime/IRuntime.h"
#include "CrySchematyc2/Services/ITimerSystem.h"

namespace Schematyc2
{
class CRuntimeFunction;
struct ILib;
struct ILibClassProperties;
struct SVMOp;

DECLARE_SHARED_POINTERS(ILibClassProperties)

typedef TemplateUtils::CArrayView<const IGlobalFunctionConstPtr>          GlobalFunctionConstTable;
typedef TemplateUtils::CArrayView<const IComponentMemberFunctionConstPtr> ComponentMemberFunctionConstTable;
typedef TemplateUtils::CArrayView<const IActionMemberFunctionConstPtr>    ActionMemberFunctionConstTable;
typedef TemplateUtils::CDelegate<void (const char*)>                      StringStreamOutCallback;

struct ILibAbstractInterfaceFunction
{
	virtual ~ILibAbstractInterfaceFunction() {}

	virtual SGUID              GetGUID() const = 0;
	virtual const char*        GetName() const = 0;
	virtual TVariantConstArray GetVariantInputs() const = 0;
	virtual TVariantConstArray GetVariantOutputs() const = 0;
};

DECLARE_SHARED_POINTERS(ILibAbstractInterfaceFunction)

struct ILibAbstractInterface
{
	virtual ~ILibAbstractInterface() {}

	virtual SGUID       GetGUID() const = 0;
	virtual const char* GetName() const = 0;
};

DECLARE_SHARED_POINTERS(ILibAbstractInterface)

enum class ELibStateMachineLifetime
{
	Unknown = 0,
	Persistent,
	Task
};

struct ILibStateMachine
{
	virtual ~ILibStateMachine() {}

	virtual SGUID                    GetGUID() const = 0;
	virtual const char*              GetName() const = 0;
	virtual ELibStateMachineLifetime GetLifetime() const = 0;
	virtual size_t                   GetPartner() const = 0;
	virtual size_t                   GetListenerCount() const = 0;
	virtual size_t                   GetListener(size_t listenerIdx) const = 0;
	virtual size_t                   GetVariableCount() const = 0;
	virtual size_t                   GetVariable(const size_t variableIdx) const = 0;
	virtual size_t                   GetContainerCount() const = 0;
	virtual size_t                   GetContainer(const size_t containerIdx) const = 0;
	virtual void                     SetBeginFunction(const LibFunctionId& functionId) = 0;
	virtual LibFunctionId            GetBeginFunction() const = 0;
};

struct ILibState
{
	virtual ~ILibState() {}

	virtual SGUID       GetGUID() const = 0;
	virtual const char* GetName() const = 0;
	virtual size_t      GetParent() const = 0;
	virtual size_t      GetPartner() const = 0;
	virtual size_t      GetStateMachine() const = 0;
	virtual size_t      GetVariableCount() const = 0;
	virtual size_t      GetVariable(size_t variableIdx) const = 0;
	virtual size_t      GetContainerCount() const = 0;
	virtual size_t      GetContainer(size_t containerIdx) const = 0;
	virtual size_t      GetTimerCount() const = 0;
	virtual size_t      GetTimer(size_t timerIdx) const = 0;
	virtual size_t      GetActionInstanceCount() const = 0;
	virtual size_t      GetActionInstance(size_t actionInstanceIdx) const = 0;
	virtual size_t      GetConstructorCount() const = 0;
	virtual size_t      GetConstructor(size_t constructorIdx) const = 0;
	virtual size_t      GetDestructorCount() const = 0;
	virtual size_t      GetDestructor(size_t destructorIdx) const = 0;
	virtual size_t      GetSignalReceiverCount() const = 0;
	virtual size_t      GetSignalReceiver(size_t signalReceiverIdx) const = 0;
	virtual size_t      GetTransitionCount() const = 0;
	virtual size_t      GetTransition(size_t transitionIdx) const = 0;
};

enum class ELibVariableFlags   // #SchematycTODO : Do these really need to be flags?
{
	None                 = 0,
	ClassProperty        = BIT(0),
	StateMachineProperty = BIT(1),
	StateProperty        = BIT(2),
};

DECLARE_ENUM_CLASS_FLAGS(ELibVariableFlags)

struct ILibVariable
{
	virtual ~ILibVariable() {}

	virtual SGUID             GetGUID() const = 0;
	virtual const char*       GetName() const = 0;
	virtual EOverridePolicy   GetOverridePolicy() const = 0;
	virtual IAnyConstPtr      GetValue() const = 0;
	virtual size_t            GetVariantPos() const = 0;
	virtual size_t            GetVariantCount() const = 0;
	virtual ELibVariableFlags GetFlags() const = 0;
};

struct ILibContainer
{
	virtual ~ILibContainer() {}

	virtual SGUID       GetGUID() const = 0;
	virtual const char* GetName() const = 0;
	virtual SGUID       GetTypeGUID() const = 0;
};

struct ILibTimer
{
	virtual ~ILibTimer() {}

	virtual SGUID               GetGUID() const = 0;
	virtual const char*         GetName() const = 0;
	virtual const STimerParams& GetParams() const = 0;
};

struct ILibAbstractInterfaceImplementation
{
	virtual ~ILibAbstractInterfaceImplementation() {}

	virtual SGUID         GetGUID() const = 0;
	virtual const char*   GetName() const = 0;
	virtual SGUID         GetInterfaceGUID() const = 0;
	virtual size_t        GetFunctionCount() const = 0;
	virtual SGUID         GetFunctionGUID(size_t functionIdx) const = 0;
	virtual LibFunctionId GetFunctionId(size_t functionIdx) const = 0;
};

struct ILibComponentInstance
{
	virtual ~ILibComponentInstance() {}

	virtual SGUID               GetGUID() const = 0;
	virtual const char*         GetName() const = 0;
	virtual SGUID               GetComponentGUID() const = 0;
	virtual IPropertiesConstPtr GetProperties() const = 0;
	virtual uint32              GetPropertyFunctionIdx() const = 0;
	virtual uint32              GetParentIdx() const = 0;
};

struct ILibActionInstance
{
	virtual ~ILibActionInstance() {}

	virtual SGUID               GetGUID() const = 0;
	virtual const char*         GetName() const = 0;
	virtual SGUID               GetActionGUID() const = 0;
	virtual size_t              GetComponentInstance() const = 0;
	virtual IPropertiesConstPtr GetProperties() const = 0;
};

struct ILibConstructor
{
	virtual ~ILibConstructor() {}

	virtual SGUID         GetGUID() const = 0;
	virtual LibFunctionId GetFunctionId() const = 0;
};

struct ILibDestructor
{
	virtual ~ILibDestructor() {}

	virtual SGUID         GetGUID() const = 0;
	virtual LibFunctionId GetFunctionId() const = 0;
};

struct ILibSignalReceiver
{
	virtual ~ILibSignalReceiver() {}

	virtual SGUID         GetGUID() const = 0;
	virtual SGUID         GetContextGUID() const = 0;
	virtual LibFunctionId GetFunctionId() const = 0;
};

enum class ELibTransitionResult
{
	Continue,
	ChangeState,
	EndSuccess,
	EndFailure
};

struct ILibTransition
{
	virtual ~ILibTransition() {}

	virtual SGUID         GetGUID() const = 0;
	virtual SGUID         GetContextGUID() const = 0;
	virtual LibFunctionId GetFunctionId() const = 0;
};

struct ILibFunction
{
	virtual ~ILibFunction() {}

	virtual SGUID                             GetGUID() const = 0;
	virtual SGUID                             GetClassGUID() const = 0;
	virtual void                              SetName(const char* szName) = 0;
	virtual const char*                       GetName() const = 0;
	virtual void                              SetScope(const char* szScope) = 0;
	virtual const char*                       GetScope() const = 0;
	virtual void                              SetFileName(const char* szFileName) = 0;
	virtual const char*                       GetFileName() const = 0;
	virtual void                              SetAuthor(const char* szAuthor) = 0;
	virtual const char*                       GetAuthor() const = 0;
	virtual void                              SetDescription(const char* szDescription) = 0;
	virtual const char*                       GetDescription() const = 0;
	virtual void                              AddInput(const char* szName, const char* szDescription, const IAny& value) = 0;
	virtual size_t                            GetInputCount() const = 0;
	virtual const char*                       GetInputName(size_t inputIdx) const = 0;
	virtual const char*                       GetInputTextDesc(size_t inputIdx) const = 0;
	virtual TVariantConstArray                GetVariantInputs() const = 0;
	virtual void                              AddOutput(const char* szName, const char* szDescription, const IAny& value) = 0;
	virtual size_t                            GetOutputCount() const = 0;
	virtual const char*                       GetOutputName(size_t outputIdx) const = 0;
	virtual const char*                       GetOutputTextDesc(size_t outputIdx) const = 0;
	virtual TVariantConstArray                GetVariantOutputs() const = 0;
	virtual TVariantConstArray                GetVariantConsts() const = 0;
	virtual GlobalFunctionConstTable          GetGlobalFunctionTable() const = 0;
	virtual ComponentMemberFunctionConstTable GetComponentMemberFunctionTable() const = 0;
	virtual ActionMemberFunctionConstTable    GetActionMemberFunctionTable() const = 0;
	virtual size_t                            GetSize() const = 0;
	virtual const SVMOp*                      GetOp(size_t pos) const = 0;
};

// The following type exists only for backwards compatibility in Hunt and can be removed once all levels have been re-exported.
typedef TemplateUtils::CDelegate<void (const SGUID& guid, const char* szLabel, const IAnyConstPtr& pValue)> LibClassPropertyVisitor;

struct ILibClassProperties
{
	virtual ~ILibClassProperties() {}

	virtual ILibClassPropertiesPtr Clone() const = 0;
	virtual void                   Serialize(Serialization::IArchive& archive) = 0;
	virtual IAnyConstPtr           GetProperty(const SGUID& guid) const = 0;
	// The following functions exist only for backwards compatibility in Hunt and can be removed once all levels have been re-exported.
	virtual void                   VisitProperties(const LibClassPropertyVisitor& visitor) const = 0;
	virtual void                   OverrideProperty(const SGUID& guid, const IAny& value) = 0;
};

struct ILibClass
{
	virtual ~ILibClass() {}

	virtual const ILib&                                GetLib() const = 0;
	virtual SGUID                                      GetGUID() const = 0;
	virtual const char*                                GetName() const = 0;
	virtual ILibClassPropertiesPtr                     CreateProperties() const = 0;
	virtual SGUID                                      GetFoundationGUID() const = 0;
	virtual IPropertiesConstPtr                        GetFoundationProperties() const = 0; // #SchematycTODO : Return raw pointer?
	virtual size_t                                     GetStateMachineCount() const = 0;
	virtual const ILibStateMachine*                    GetStateMachine(size_t stateMachineIdx) const = 0;
	virtual size_t                                     GetStateCount() const = 0;
	virtual const ILibState*                           GetState(size_t stateIdx) const = 0;
	virtual size_t                                     GetVariableCount() const = 0;
	virtual const ILibVariable*                        GetVariable(size_t variableIdx) const = 0;
	virtual TVariantConstArray                         GetVariants() const = 0; // #SchematycTODO : GetVariableVariants()?
	virtual size_t                                     GetContainerCount() const = 0;
	virtual const ILibContainer*                       GetContainer(size_t containerIdx) const = 0;
	virtual size_t                                     GetTimerCount() const = 0;
	virtual const ILibTimer*                           GetTimer(size_t timerIdx) const = 0;
	virtual size_t                                     GetPersistentTimerCount() const = 0;
	virtual size_t                                     GetPersistentTimer(size_t persistentTimerIdx) const = 0;
	virtual size_t                                     GetAbstractInterfaceImplementationCount() const = 0;
	virtual const ILibAbstractInterfaceImplementation* GetAbstractInterfaceImplementation(size_t abstractInterfaceImplementationIdx) const = 0;
	virtual size_t                                     GetComponentInstanceCount() const = 0;
	virtual const ILibComponentInstance*               GetComponentInstance(size_t componentInstanceIdx) const = 0;
	virtual size_t                                     GetActionInstanceCount() const = 0;
	virtual const ILibActionInstance*                  GetActionInstance(size_t actionInstanceIdx) const = 0;
	virtual size_t                                     GetPersistentActionInstanceCount() const = 0;
	virtual size_t                                     GetPersistentActionInstance(size_t persistentActionInstanceIdx) const = 0;
	virtual size_t                                     GetConstructorCount() const = 0;
	virtual const ILibConstructor*                     GetConstructor(size_t constructorIdx) const = 0;
	virtual size_t                                     GetPersistentConstructorCount() const = 0;
	virtual size_t                                     GetPersistentConstructor(size_t persistentConstructorIdx) const = 0;
	virtual size_t                                     GetDestructorCount() const = 0;
	virtual const ILibDestructor*                      GetDestructor(size_t destructorIdx) const = 0;
	virtual size_t                                     GetPersistentDestructorCount() const = 0;
	virtual size_t                                     GetPersistentDestructor(size_t persistentDestructorIdx) const = 0;
	virtual size_t                                     GetSignalReceiverCount() const = 0;
	virtual const ILibSignalReceiver*                  GetSignalReceiver(size_t signalReceiverIdx) const = 0;
	virtual size_t                                     GetPersistentSignalReceiverCount() const = 0;
	virtual size_t                                     GetPersistentSignalReceiver(size_t persistentSignalReceiverIdx) const = 0;
	virtual size_t                                     GetTransitionCount() const = 0;
	virtual const ILibTransition*                      GetTransition(size_t transitionIdx) const = 0;
	virtual LibFunctionId                              GetFunctionId(const SGUID& guid) const = 0;
	virtual const ILibFunction*                        GetFunction(const LibFunctionId& functionId) const = 0;
	virtual const CRuntimeFunction*                    GetFunction_New(uint32 functionIdx) const = 0;
	virtual void         PreviewGraphFunctions(const SGUID& graphGUID, const StringStreamOutCallback& stringStreamOutCallback) const = 0;
	virtual void         AddPrecacheResource(IAnyConstPtr resource) = 0;
	virtual size_t       GetPrecacheResourceCount() const = 0;
	virtual IAnyConstPtr GetPrecacheResource(size_t ressourceIdx) const = 0;
};

DECLARE_SHARED_POINTERS(ILibClass)

typedef TemplateUtils::CDelegate<EVisitStatus(const ISignalConstPtr&)>                       LibSignalVisitor;
typedef TemplateUtils::CDelegate<EVisitStatus(const ILibAbstractInterfaceConstPtr&)>         LibAbstractInterfaceVisitor;
typedef TemplateUtils::CDelegate<EVisitStatus(const ILibAbstractInterfaceFunctionConstPtr&)> LibAbstractInterfaceFunctionVisitor;
typedef TemplateUtils::CDelegate<EVisitStatus(const ILibClassConstPtr&)>                     LibClassVisitor;

struct ILib
{
	virtual ~ILib() {}

	virtual const char*                           GetName() const = 0;
	virtual ISignalConstPtr                       GetSignal(const SGUID& guid) const = 0;
	virtual void                                  VisitSignals(const LibSignalVisitor& visitor) = 0;
	virtual ILibAbstractInterfaceConstPtr         GetAbstractInterface(const SGUID& guid) const = 0;
	virtual void                                  VisitAbstractInterfaces(const LibAbstractInterfaceVisitor& visitor) = 0;
	virtual ILibAbstractInterfaceFunctionConstPtr GetAbstractInterfaceFunction(const SGUID& guid) const = 0;
	virtual void                                  VisitAbstractInterfaceFunctions(const LibAbstractInterfaceFunctionVisitor& visitor) = 0;
	virtual ILibClassConstPtr                     GetClass(const SGUID& guid) const = 0;
	virtual void                                  VisitClasses(const LibClassVisitor& visitor) const = 0;
	virtual void                                  PreviewGraphFunctions(const SGUID& graphGUID, const StringStreamOutCallback& stringStreamOutCallback) const = 0;
};

DECLARE_SHARED_POINTERS(ILib)
}
