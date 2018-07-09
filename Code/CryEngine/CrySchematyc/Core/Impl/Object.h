// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/IObject.h>
#include <CrySchematyc/Runtime/RuntimeGraph.h>
#include <CrySchematyc/Services/ITimerSystem.h>
#include <CrySchematyc/Utils/Transform.h>
#include <CrySchematyc/Env/Elements/IEnvComponent.h>

#include "Runtime/RuntimeClass.h"

namespace Schematyc
{

// Forward declare interfaces.

struct IObjectProperties;
// Forward declare structures.
struct SUpdateContext;
// Forward declare classes.
class CAction;
class CActionDesc;

class CRuntimeParamMap;
class CScratchpad;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(IObjectProperties)
DECLARE_SHARED_POINTERS(CAction)

DECLARE_SHARED_POINTERS(CRuntimeClass)

struct SQueuedObjectSignal
{
	inline SQueuedObjectSignal() {}

	inline SQueuedObjectSignal(const SQueuedObjectSignal& rhs)
		: signal(rhs.signal)
		, scratchpad(rhs.scratchpad)
	{}

	inline SQueuedObjectSignal(const SObjectSignal& rhs)
		: signal(rhs.typeGUID, rhs.senderGUID)
	{
		auto visitInput = [this](const CUniqueId& id, const CAnyConstRef& value)
		{
			const uint32 pos = scratchpad.Add(value);
			const CAnyPtr pValue = scratchpad.Get(pos);
			signal.params.BindInput(id, pValue);
		};
		rhs.params.VisitInputs(visitInput);

		auto visitOutput = [this](const CUniqueId& id, const CAnyConstRef& value)
		{
			const uint32 pos = scratchpad.Add(value);
			const CAnyPtr pValue = scratchpad.Get(pos);
			signal.params.BindOutput(id, pValue);
		};
		rhs.params.VisitOutputs(visitOutput);
	}

	SObjectSignal   signal;
	StackScratchpad scratchpad;
};

typedef std::deque<SQueuedObjectSignal> ObjectSignalQueue;

class CObject : public IObject
{
private:

	typedef std::vector<CRuntimeGraphInstance> Graphs;

	struct SStateMachine
	{
		SStateMachine();

		uint32 stateIdx;
	};

	typedef std::vector<SStateMachine> StateMachines;

	struct SComponent
	{
		SComponent(const SRuntimeClassComponentInstance& inst, const IEnvComponent* pEnvComponent)
			: classComponentInstance(inst)
			, classDesc(pEnvComponent->GetDesc())
			, pComponent(pEnvComponent->CreateFromPool())
		{};
		const SRuntimeClassComponentInstance& classComponentInstance;
		const CClassDesc&                     classDesc;
		std::shared_ptr<IEntityComponent>     pComponent;
	};
	typedef std::vector<SComponent> Components;

	struct SAction
	{
		SAction(const CActionDesc& _desc, const SRuntimeActionDesc& _runtimeDesc, const CActionPtr& _ptr);

		const CActionDesc& desc;
		SRuntimeActionDesc runtimeDesc;
		CActionPtr         ptr;
		bool               bRunning = false;
	};

	typedef std::vector<SAction> Actions;

	struct STimer
	{
		STimer(CObject* _pObject, const CryGUID& _guid, TimerFlags _flags);

		void Activate();

		CObject*   pObject;
		CryGUID    guid;
		TimerFlags flags;
		TimerId    id;
	};

	typedef std::vector<STimer> Timers;

	struct SState
	{
		Timers timers;
	};

	typedef std::vector<SState> States;

public:
	CObject(IEntity& entity, ObjectId id, void* pCustomData);
	~CObject();

	bool Init(CryGUID classGUID, const IObjectPropertiesPtr& pProperties);

	// IObject
	virtual ObjectId             GetId() const override;
	virtual const IRuntimeClass& GetClass() const override;
	virtual const char*          GetScriptFile() const override;
	virtual void*                GetCustomData() const override;
	virtual ESimulationMode      GetSimulationMode() const override;

	virtual bool                 SetSimulationMode(ESimulationMode simulationMode, EObjectSimulationUpdatePolicy updatePolicy) override;
	virtual void                 ProcessSignal(const SObjectSignal& signal) override;
	virtual void                 StopAction(CAction& action) override;

	virtual EVisitResult         VisitComponents(const ObjectComponentConstVisitor& visitor) const override;
	virtual void                 Dump(IObjectDump& dump, const ObjectDumpFlags& flags = EObjectDumpFlags::All) const override;

	virtual IEntity*             GetEntity() const final           { return m_pEntity; };

	virtual IObjectPropertiesPtr GetObjectProperties() const final { return m_pProperties; };
	// ~IObject

	CScratchpad&      GetScratchpad();

	bool              ExecuteFunction(uint32 functionIdx, CRuntimeParamMap& params);
	bool              StartAction(uint32 actionIdx, CRuntimeParamMap& params);

	IEntityComponent* GetComponent(uint32 componentId);

private:
	bool InitClass();

	void StopSimulation();
	void Update(const SUpdateContext& updateContext);

	void CreateGraphs();
	void ResetGraphs();

	bool ReadProperties();

	void ExecuteConstructors(ESimulationMode simulationMode);

	bool CreateStateMachines();
	void StartStateMachines(ESimulationMode simulationMode);
	void StopStateMachines();
	void DestroyStateMachines();

	bool CreateComponents();
	void ShutdownComponents();
	void DestroyComponents();

	bool CreateActions();
	bool InitActions();
	void ShutdownActions();
	void DestroyActions();

	bool CreateTimers();
	void StartTimers(ESimulationMode simulationMode);
	void StopTimers();
	void DestroyTimers();

	void RegisterForUpdate();

	void ExecuteSignalReceivers(const SObjectSignal& signal);

	void StartStateTimers(uint32 stateMachineIdx);
	void StopStateTimers(uint32 stateMachineIdx);
	void ExecuteStateSignalReceivers(uint32 stateMachineIdx, const SObjectSignal& signal);
	bool EvaluateStateTransitions(uint32 stateMachineIdx, const SObjectSignal& signal);
	void ChangeState(uint32 stateMachineIdx, uint32 stateIdx);

	void ExecuteFunction(const SRuntimeFunction& function, CRuntimeParamMap& params);
	void ExecuteFunction(uint32 graphIdx, CRuntimeParamMap& params, SRuntimeActivationParams activationParams);
	void ProcessSignalQueue();

private:
	ObjectId              m_id;

	CRuntimeClassConstPtr m_pClass;
	void*                 m_pCustomData;
	IObjectPropertiesPtr  m_pProperties;
	ESimulationMode       m_simulationMode;

	HeapScratchpad        m_scratchpad;
	Graphs                m_graphs;

	StateMachines         m_stateMachines;

	Actions               m_actions;
	Timers                m_timers;
	States                m_states;

	bool                  m_bQueueSignals;
	ObjectSignalQueue     m_signalQueue;

	CConnectionScope      m_connectionScope;

	// Components created by Schematyc object
	Components m_components;

	IEntity*   m_pEntity;
};

} // Schematyc
