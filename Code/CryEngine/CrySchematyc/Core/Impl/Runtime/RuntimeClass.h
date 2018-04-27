// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : How can we reduce duplication of functionality between persistent state and intermittent states?
// #SchematycTODO : Create lightweight runtime element id system to replace guids? We can always build an id to guid map (or vice-versa) if we need access to guids at runtime (e.g. for debugging).

#pragma once

#include "ObjectProperties.h"

#include <CrySchematyc/Runtime/IRuntimeClass.h>
#include <CrySchematyc/Runtime/RuntimeGraph.h>
#include <CrySchematyc/Services/ITimerSystem.h>
#include <CrySchematyc/Utils/ClassProperties.h>
#include <CrySchematyc/Utils/GUID.h>
#include <CrySchematyc/Utils/Scratchpad.h>
#include <CrySchematyc/Utils/Transform.h>

namespace Schematyc
{

// Forward declare classes.
class CAnyConstPtr;
class CAnyConstRef;
class CAnyValue;
class CObjectProperties;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(CAnyValue)
DECLARE_SHARED_POINTERS(CRuntimeGraph)

struct SRuntimeFunction
{
	SRuntimeFunction();
	SRuntimeFunction(uint32 _graphIdx, const SRuntimeActivationParams& _activationParams);

	uint32                   graphIdx;
	SRuntimeActivationParams activationParams;
};

struct SRuntimeClassFunction : public SRuntimeFunction
{
	SRuntimeClassFunction(const CryGUID& _guid);
	SRuntimeClassFunction(uint32 _graphIdx, const SRuntimeActivationParams& _activationParams);

	CryGUID guid;
};

typedef std::vector<SRuntimeClassFunction> RuntimeClassFunctions;

struct SRuntimeClassConstructor : public SRuntimeFunction
{
	SRuntimeClassConstructor(uint32 _graphIdx, const SRuntimeActivationParams& _activationParams);
};

typedef std::vector<SRuntimeClassConstructor> RuntimeClassConstructors;

struct SRuntimeClassStateMachine
{
	SRuntimeClassStateMachine(const CryGUID& _guid, const char* _szName);

	CryGUID            guid;
	string           name;
	SRuntimeFunction beginFunction;
};

typedef std::vector<SRuntimeClassStateMachine> RuntimeClassStateMachines;

struct SRuntimeClassStateTimer : public SRuntimeFunction
{
	SRuntimeClassStateTimer(const CryGUID& _guid, const char* _szName, const STimerParams& _params);

	CryGUID        guid;
	string       name;
	STimerParams params;
};

typedef std::vector<SRuntimeClassStateTimer> RuntimeClassStateTimers;

struct SRuntimeClassStateSignalReceiver : public SRuntimeFunction // #SchematycTODO : Rename signal receiver to signal mapping / connection?
{
	SRuntimeClassStateSignalReceiver(const CryGUID& _signalGUID, const CryGUID& _senderGUID, uint32 _graphIdx, const SRuntimeActivationParams& _activationParams);

	CryGUID signalGUID;
	CryGUID senderGUID;
};

typedef std::vector<SRuntimeClassStateSignalReceiver> RuntimeClassStateSignalReceivers;

struct SRuntimeClassStateTransition : public SRuntimeFunction
{
	SRuntimeClassStateTransition(const CryGUID& _signalGUID, uint32 _graphIdx, const SRuntimeActivationParams& _activationParams);

	CryGUID signalGUID;
};

typedef std::vector<SRuntimeClassStateTransition> RuntimeClassStateTransitions;

struct SRuntimeStateActionDesc
{
	SRuntimeStateActionDesc(const CryGUID& _guid, const CryGUID& _typeGUID);

	CryGUID guid;
	CryGUID typeGUID; // #SchematycTODO : Can we store a raw pointer to the env action rather than referencing by GUID?
};

typedef std::vector<SRuntimeStateActionDesc> RuntimeStateActionDescs;

struct SRuntimeClassState
{
	SRuntimeClassState(const CryGUID& _guid, const char* _szName);

	CryGUID                            guid;
	string                           name;
	RuntimeClassStateTimers          timers;
	RuntimeClassStateSignalReceivers signalReceivers;
	RuntimeClassStateTransitions     transitions;
	RuntimeStateActionDescs          actions;
};

typedef std::vector<SRuntimeClassState> RuntimeClassStates;

struct SRuntimeClassVariable
{
	SRuntimeClassVariable(const CryGUID& _guid, const char* _szName, bool _bPublic, uint32 _pos);

	CryGUID  guid;
	string name;
	bool   bPublic;
	uint32 pos;
};

typedef std::vector<SRuntimeClassVariable> RuntimeClassVariables;

struct SRuntimeClassTimer
{
	SRuntimeClassTimer(const CryGUID& _guid, const char* _szName, const STimerParams& _params);

	CryGUID        guid;
	string       name;
	STimerParams params;
};

typedef std::vector<SRuntimeClassTimer> RuntimeClassTimers;

struct SRuntimeClassComponentInstance
{
	SRuntimeClassComponentInstance(const CryGUID& _guid, const char* _szName, bool _bPublic, const CryGUID& _componentTypeGUID, const CTransformPtr& _transform, const CClassProperties& _properties, uint32 _parentIdx);

	CryGUID          guid;
	string           name;
	bool             bPublic;
	CryGUID          componentTypeGUID;    // #SchematycTODO : Can we store a raw pointer to the env component rather than referencing by GUID?
	CTransformPtr    transform;
	CClassProperties properties;
	uint32           parentIdx;
};

typedef std::vector<SRuntimeClassComponentInstance> RuntimeClassComponentInstances;

struct SRuntimeActionDesc
{
	SRuntimeActionDesc(const CryGUID& _guid, const CryGUID& _typeGUID);

	CryGUID guid;
	CryGUID typeGUID; // #SchematycTODO : Can we store a raw pointer to the env action rather than referencing by GUID?
};

typedef std::vector<SRuntimeActionDesc> RuntimeActionDescs;

struct SRuntimeClassSignalReceiver : public SRuntimeFunction // #SchematycTODO : Rename signal receiver to signal mapping / connection?
{
	SRuntimeClassSignalReceiver(const CryGUID& _signalGUID, const CryGUID& _senderGUID, uint32 _graphIdx, const SRuntimeActivationParams& _activationParams);

	CryGUID signalGUID;
	CryGUID senderGUID;
};

typedef std::vector<SRuntimeClassSignalReceiver> RuntimeClassSignalReceivers;

class CRuntimeClass : public IRuntimeClass
{
private:

	typedef std::unique_ptr<CObjectProperties> PropertiesPtr;
	typedef std::vector<CRuntimeGraphPtr>      Graphs;

public:

	CRuntimeClass(time_t timeStamp, const CryGUID& guid, const char* szName, const CryGUID& envClassGUID, const CAnyConstPtr& pEnvClassProperties);

	// IRuntimeClass

	virtual time_t                   GetTimeStamp() const override;
	virtual CryGUID                  GetGUID() const override;
	virtual const char*              GetName() const override;

	virtual const IObjectProperties& GetDefaultProperties() const override;
	virtual CryGUID                  GetEnvClassGUID() const override;
	virtual CAnyConstPtr             GetEnvClassProperties() const override;
	virtual const CScratchpad&       GetScratchpad() const override;

	// ~IRuntimeClass

	uint32                                AddGraph(const CryGUID& guid, const char* szName);
	uint32                                FindGraph(const CryGUID& guid) const;
	uint32                                GetGraphCount() const;
	CRuntimeGraph*                        GetGraph(uint32 graphIdx);
	const CRuntimeGraph*                  GetGraph(uint32 graphIdx) const;

	uint32                                AddFunction(uint32 graphIdx, const SRuntimeActivationParams& activationParams);
	uint32                                AddFunction(const CryGUID& guid, uint32 graphIdx, const SRuntimeActivationParams& activationParams);
	uint32                                FindOrReserveFunction(const CryGUID& guid);
	const RuntimeClassFunctions&          GetFunctions() const;

	void                                  AddConstructor(uint32 graphIdx, const SRuntimeActivationParams& activationParams);
	const RuntimeClassConstructors&       GetConstructors() const;

	uint32                                AddStateMachine(const CryGUID& guid, const char* szName);
	uint32                                FindStateMachine(const CryGUID& guid) const;
	void                                  SetStateMachineBeginFunction(uint32 stateMachineIdx, uint32 graphIdx, const SRuntimeActivationParams& activationParams);
	const RuntimeClassStateMachines&      GetStateMachines() const;

	uint32                                AddState(const CryGUID& guid, const char* szName);
	uint32                                FindState(const CryGUID& guid) const;
	void                                  AddStateTimer(uint32 stateIdx, const CryGUID& guid, const char* szName, const STimerParams& params);
	void                                  AddStateSignalReceiver(uint32 stateIdx, const CryGUID& signalGUID, const CryGUID& senderGUID, uint32 graphIdx, const SRuntimeActivationParams& activationParams);
	uint32                                AddStateAction(uint32 stateIdx, const CryGUID& guid, const CryGUID& typeGUID);
	void                                  AddStateTransition(uint32 stateIdx, const CryGUID& signalGUID, uint32 graphIdx, const SRuntimeActivationParams& activationParams);
	const RuntimeClassStates&             GetStates() const;

	uint32                                AddVariable(const CryGUID& guid, const char* szName, bool bPublic, const CAnyConstRef& value);
	const RuntimeClassVariables&          GetVariables() const;
	uint32                                GetVariablePos(const CryGUID& guid) const;

	uint32                                AddTimer(const CryGUID& guid, const char* szName, const STimerParams& params);
	const RuntimeClassTimers&             GetTimers() const;

	uint32                                AddComponentInstance(const CryGUID& guid, const char* szName, bool bPublic, const CryGUID& componentTypeGUID, const CTransformPtr& transform, const CClassProperties& properties, uint32 parentIdx);
	uint32                                FindComponentInstance(const CryGUID& guid) const;
	const RuntimeClassComponentInstances& GetComponentInstances() const;

	uint32                                AddSignalReceiver(const CryGUID& signalGUID, const CryGUID& senderGUID, uint32 graphIdx, const SRuntimeActivationParams& activationParams);
	const RuntimeClassSignalReceivers&    GetSignalReceivers() const;

	uint32                                AddAction(const CryGUID& guid, const CryGUID& typeGUID);
	const RuntimeActionDescs&             GetActions() const;

	uint32                                CountSignalReceviers(const CryGUID& signalGUID) const;
	void                                  FinalizeComponentInstances();
	void                                  Finalize();

private:

	time_t                         m_timeStamp;
	CryGUID                          m_guid;
	string                         m_name;
	PropertiesPtr                  m_pDefaultProperties;

	CryGUID                          m_envClassGUID;
	CAnyValuePtr                   m_pEnvClassProperties;

	HeapScratchpad                 m_scratchpad;
	Graphs                         m_graphs;

	RuntimeClassFunctions          m_functions;
	RuntimeClassConstructors       m_constructors;
	RuntimeClassStateMachines      m_stateMachines;
	RuntimeClassStates             m_states;
	RuntimeClassVariables          m_variables;
	RuntimeClassTimers             m_timers;
	RuntimeClassComponentInstances m_componentInstances;
	RuntimeClassSignalReceivers    m_signalReceivers;
	RuntimeActionDescs             m_actions;
};

} // Schematyc
