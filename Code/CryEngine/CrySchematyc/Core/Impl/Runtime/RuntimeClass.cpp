// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RuntimeClass.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/IEnvComponent.h>
#include <CrySchematyc/Utils/Any.h>
#include <CrySchematyc/Utils/Assert.h>

#include "ObjectProperties.h"

namespace Schematyc
{
namespace
{

typedef std::vector<uint32> Dependencies;

struct SComponentInstanceSortRef
{
	inline SComponentInstanceSortRef(uint32 _componentInstanceIdx)
		: componentInstanceIdx(_componentInstanceIdx)
		, priority(0)
	{}

	uint32       componentInstanceIdx;
	int32        priority;
	Dependencies dependencies;
};

typedef std::vector<SComponentInstanceSortRef> ComponentInstanceSortRefs;

struct SComponentInstanceSortPredicate
{
	inline bool operator()(const SComponentInstanceSortRef& lhs, const SComponentInstanceSortRef& rhs) const
	{
		return lhs.priority > rhs.priority;
	}
};

} // Anonymous

SRuntimeFunction::SRuntimeFunction()
	: graphIdx(InvalidIdx)
{}

SRuntimeFunction::SRuntimeFunction(uint32 _graphIdx, const SRuntimeActivationParams& _activationParams)
	: graphIdx(_graphIdx)
	, activationParams(_activationParams)
{}

SRuntimeClassFunction::SRuntimeClassFunction(const CryGUID& _guid)
	: guid(_guid)
{}

SRuntimeClassFunction::SRuntimeClassFunction(uint32 _graphIdx, const SRuntimeActivationParams& _activationParams)
	: SRuntimeFunction(_graphIdx, _activationParams)
{}

SRuntimeClassConstructor::SRuntimeClassConstructor(uint32 _graphIdx, const SRuntimeActivationParams& _activationParams)
	: SRuntimeFunction(_graphIdx, _activationParams)
{}

SRuntimeClassStateMachine::SRuntimeClassStateMachine(const CryGUID& _guid, const char* _szName)
	: guid(_guid)
	, name(_szName)
{}

SRuntimeClassStateTimer::SRuntimeClassStateTimer(const CryGUID& _guid, const char* _szName, const STimerParams& _params)
	: guid(_guid)
	, name(_szName)
	, params(_params)
{}

SRuntimeClassStateSignalReceiver::SRuntimeClassStateSignalReceiver(const CryGUID& _signalGUID, const CryGUID& _senderGUID, uint32 _graphIdx, const SRuntimeActivationParams& _activationParams)
	: SRuntimeFunction(_graphIdx, _activationParams)
	, signalGUID(_signalGUID)
	, senderGUID(_senderGUID)
{}

SRuntimeClassStateTransition::SRuntimeClassStateTransition(const CryGUID& _signalGUID, uint32 _graphIdx, const SRuntimeActivationParams& _activationParams)
	: SRuntimeFunction(_graphIdx, _activationParams)
	, signalGUID(_signalGUID)
{}

SRuntimeStateActionDesc::SRuntimeStateActionDesc(const CryGUID& _guid, const CryGUID& _typeGUID)
	: guid(_guid)
	, typeGUID(_typeGUID)
{}

SRuntimeClassState::SRuntimeClassState(const CryGUID& _guid, const char* _szName)
	: guid(_guid)
	, name(_szName)
{}

SRuntimeClassVariable::SRuntimeClassVariable(const CryGUID& _guid, const char* _szName, bool _bPublic, uint32 _pos)
	: guid(_guid)
	, name(_szName)
	, bPublic(_bPublic)
	, pos(_pos)
{}

SRuntimeClassTimer::SRuntimeClassTimer(const CryGUID& _guid, const char* _szName, const STimerParams& _params)
	: guid(_guid)
	, name(_szName)
	, params(_params)
{}

SRuntimeClassComponentInstance::SRuntimeClassComponentInstance(const CryGUID& _guid, const char* _szName, bool _bPublic, const CryGUID& _componentTypeGUID, const CTransformPtr& _transform, const CClassProperties& _properties, uint32 _parentIdx)
	: guid(_guid)
	, name(_szName)
	, bPublic(_bPublic)
	, componentTypeGUID(_componentTypeGUID)
	, transform(_transform)
	, properties(_properties)
	, parentIdx(_parentIdx)
{}

SRuntimeActionDesc::SRuntimeActionDesc(const CryGUID& _guid, const CryGUID& _typeGUID)
	: guid(_guid)
	, typeGUID(_typeGUID)
{}

SRuntimeClassSignalReceiver::SRuntimeClassSignalReceiver(const CryGUID& _signalGUID, const CryGUID& _senderGUID, uint32 _graphIdx, const SRuntimeActivationParams& _activationParams)
	: SRuntimeFunction(_graphIdx, _activationParams)
	, signalGUID(_signalGUID)
	, senderGUID(_senderGUID)
{}

CRuntimeClass::CRuntimeClass(time_t timeStamp, const CryGUID& guid, const char* szName, const CryGUID& envClassGUID, const CAnyConstPtr& pEnvClassProperties)
	: m_timeStamp(timeStamp)
	, m_guid(guid)
	, m_name(szName)
	, m_pDefaultProperties(new CObjectProperties())
	, m_envClassGUID(envClassGUID)
	, m_pEnvClassProperties(pEnvClassProperties ? CAnyValue::CloneShared(*pEnvClassProperties) : CAnyValuePtr())
{}

time_t CRuntimeClass::GetTimeStamp() const
{
	return m_timeStamp;
}

CryGUID CRuntimeClass::GetGUID() const
{
	return m_guid;
}

const char* CRuntimeClass::GetName() const
{
	return m_name.c_str();
}

const IObjectProperties& CRuntimeClass::GetDefaultProperties() const
{
	return *m_pDefaultProperties;
}

CryGUID CRuntimeClass::GetEnvClassGUID() const
{
	return m_envClassGUID;
}

CAnyConstPtr CRuntimeClass::GetEnvClassProperties() const
{
	return m_pEnvClassProperties.get();
}

const CScratchpad& CRuntimeClass::GetScratchpad() const
{
	return m_scratchpad;
}

uint32 CRuntimeClass::AddGraph(const CryGUID& guid, const char* szName)
{
	m_graphs.reserve(20);
	m_graphs.emplace_back(std::make_shared<CRuntimeGraph>(guid, szName));
	return m_graphs.size() - 1;
}

uint32 CRuntimeClass::FindGraph(const CryGUID& guid) const
{
	for (uint32 graphIdx = 0, graphCount = m_graphs.size(); graphIdx < graphCount; ++graphIdx)
	{
		if (m_graphs[graphIdx]->GetGUID() == guid)
		{
			return graphIdx;
		}
	}
	return InvalidIdx;
}

uint32 CRuntimeClass::GetGraphCount() const
{
	return m_graphs.size();
}

CRuntimeGraph* CRuntimeClass::GetGraph(uint32 graphIdx)
{
	return graphIdx < m_graphs.size() ? m_graphs[graphIdx].get() : nullptr;
}

const CRuntimeGraph* CRuntimeClass::GetGraph(uint32 graphIdx) const
{
	return graphIdx < m_graphs.size() ? m_graphs[graphIdx].get() : nullptr;
}

uint32 CRuntimeClass::AddFunction(uint32 graphIdx, const SRuntimeActivationParams& activationParams)
{
	m_functions.reserve(20);
	m_functions.emplace_back(graphIdx, activationParams);
	return m_functions.size() - 1;
}

uint32 CRuntimeClass::AddFunction(const CryGUID& guid, uint32 graphIdx, const SRuntimeActivationParams& activationParams)
{
	const uint32 functionIdx = FindOrReserveFunction(guid);
	SRuntimeClassFunction& function = m_functions[functionIdx];
	function.graphIdx = graphIdx;
	function.activationParams = activationParams;
	return functionIdx;
}

uint32 CRuntimeClass::FindOrReserveFunction(const CryGUID& guid)
{
	const uint32 functionCount = m_functions.size();
	for (uint32 functionIdx = 0; functionIdx < functionCount; ++functionIdx)
	{
		if (m_functions[functionIdx].guid == guid)
		{
			return functionIdx;
		}
	}
	m_functions.reserve(20);
	m_functions.emplace_back(guid);
	return functionCount;
}

const RuntimeClassFunctions& CRuntimeClass::GetFunctions() const
{
	return m_functions;
}

void CRuntimeClass::AddConstructor(uint32 graphIdx, const SRuntimeActivationParams& activationParams)
{
	m_constructors.reserve(5);
	m_constructors.emplace_back(graphIdx, activationParams);
}

const RuntimeClassConstructors& CRuntimeClass::GetConstructors() const
{
	return m_constructors;
}

uint32 CRuntimeClass::AddStateMachine(const CryGUID& guid, const char* szName)
{
	m_stateMachines.reserve(10);
	m_stateMachines.emplace_back(guid, szName);
	return m_stateMachines.size() - 1;
}

uint32 CRuntimeClass::FindStateMachine(const CryGUID& guid) const
{
	for (uint32 stateMachineIdx = 0, stateMachineCount = m_stateMachines.size(); stateMachineIdx < stateMachineCount; ++stateMachineIdx)
	{
		if (m_stateMachines[stateMachineIdx].guid == guid)
		{
			return stateMachineIdx;
		}
	}
	return InvalidIdx;
}

void CRuntimeClass::SetStateMachineBeginFunction(uint32 stateMachineIdx, uint32 graphIdx, const SRuntimeActivationParams& activationParams)
{
	SCHEMATYC_CORE_ASSERT(stateMachineIdx < m_stateMachines.size());
	if (stateMachineIdx < m_stateMachines.size())
	{
		m_stateMachines[stateMachineIdx].beginFunction = SRuntimeFunction(graphIdx, activationParams);
	}
}

const RuntimeClassStateMachines& CRuntimeClass::GetStateMachines() const
{
	return m_stateMachines;
}

uint32 CRuntimeClass::AddState(const CryGUID& guid, const char* szName)
{
	m_states.reserve(20);
	m_states.emplace_back(guid, szName);
	return m_states.size() - 1;
}

uint32 CRuntimeClass::FindState(const CryGUID& guid) const
{
	for (uint32 stateIdx = 0, stateCount = m_states.size(); stateIdx < stateCount; ++stateIdx)
	{
		if (m_states[stateIdx].guid == guid)
		{
			return stateIdx;
		}
	}
	return InvalidIdx;
}

void CRuntimeClass::AddStateTimer(uint32 stateIdx, const CryGUID& guid, const char* szName, const STimerParams& params)
{
	SCHEMATYC_CORE_ASSERT(stateIdx < m_states.size());
	if (stateIdx < m_states.size())
	{
		SRuntimeClassState& state = m_states[stateIdx];
		state.timers.reserve(10);
		state.timers.emplace_back(guid, szName, params);
	}
}

void CRuntimeClass::AddStateSignalReceiver(uint32 stateIdx, const CryGUID& signalGUID, const CryGUID& senderGUID, uint32 graphIdx, const SRuntimeActivationParams& activationParams)
{
	SCHEMATYC_CORE_ASSERT(stateIdx < m_states.size());
	if (stateIdx < m_states.size())
	{
		SRuntimeClassState& state = m_states[stateIdx];
		state.signalReceivers.reserve(10);
		state.signalReceivers.emplace_back(signalGUID, senderGUID, graphIdx, activationParams);
	}
}

uint32 CRuntimeClass::AddStateAction(uint32 stateIdx, const CryGUID& guid, const CryGUID& typeGUID)
{
	SCHEMATYC_CORE_ASSERT(stateIdx < m_states.size());
	if (stateIdx < m_states.size())
	{
		SRuntimeClassState& state = m_states[stateIdx];
		state.actions.reserve(10);
		state.actions.emplace_back(guid, typeGUID);
		return m_actions.size() - 1;
	}
	return InvalidIdx;
}

void CRuntimeClass::AddStateTransition(uint32 stateIdx, const CryGUID& signalGUID, uint32 graphIdx, const SRuntimeActivationParams& activationParams)
{
	SCHEMATYC_CORE_ASSERT(stateIdx < m_states.size());
	if (stateIdx < m_states.size())
	{
		SRuntimeClassState& state = m_states[stateIdx];
		state.transitions.reserve(10);
		state.transitions.emplace_back(signalGUID, graphIdx, activationParams);
	}
}

const RuntimeClassStates& CRuntimeClass::GetStates() const
{
	return m_states;
}

uint32 CRuntimeClass::AddVariable(const CryGUID& guid, const char* szName, bool bPublic, const CAnyConstRef& value)
{
	if (bPublic)
	{
		m_pDefaultProperties->AddVariable(guid, szName, value);
	}

	const uint32 pos = m_scratchpad.Add(value);
	m_variables.reserve(20);
	m_variables.emplace_back(guid, szName, bPublic, pos);
	return m_variables.size() - 1;
}

const RuntimeClassVariables& CRuntimeClass::GetVariables() const
{
	return m_variables;
}

uint32 CRuntimeClass::GetVariablePos(const CryGUID& guid) const
{
	for (const SRuntimeClassVariable& variable : m_variables)
	{
		if (variable.guid == guid)
		{
			return variable.pos;
		}
	}
	return InvalidIdx;
}

uint32 CRuntimeClass::AddTimer(const CryGUID& guid, const char* szName, const STimerParams& params)
{
	m_timers.reserve(20);
	m_timers.emplace_back(guid, szName, params);
	return m_timers.size() - 1;
}

const RuntimeClassTimers& CRuntimeClass::GetTimers() const
{
	return m_timers;
}

uint32 CRuntimeClass::AddComponentInstance(const CryGUID& guid, const char* szName, bool bPublic, const CryGUID& componentTypeGUID, const CTransformPtr& transform, const CClassProperties& properties, uint32 parentIdx)
{
	if (bPublic)
	{
		m_pDefaultProperties->AddComponent(guid, szName, properties);
	}

	m_componentInstances.reserve(10);
	m_componentInstances.emplace_back(guid, szName, bPublic, componentTypeGUID, transform, properties, parentIdx);
	return m_componentInstances.size() - 1;
}

uint32 CRuntimeClass::FindComponentInstance(const CryGUID& guid) const
{
	for (uint32 componentInstanceIdx = 0, componentInstanceCount = m_componentInstances.size(); componentInstanceIdx < componentInstanceCount; ++componentInstanceIdx)
	{
		if (m_componentInstances[componentInstanceIdx].guid == guid)
		{
			return componentInstanceIdx;
		}
	}
	return InvalidIdx;
}

const RuntimeClassComponentInstances& CRuntimeClass::GetComponentInstances() const
{
	return m_componentInstances;
}

uint32 CRuntimeClass::AddSignalReceiver(const CryGUID& signalGUID, const CryGUID& senderGUID, uint32 graphIdx, const SRuntimeActivationParams& activationParams)
{
	m_signalReceivers.reserve(20);
	m_signalReceivers.emplace_back(signalGUID, senderGUID, graphIdx, activationParams);
	return m_signalReceivers.size() - 1;
}

const RuntimeClassSignalReceivers& CRuntimeClass::GetSignalReceivers() const
{
	return m_signalReceivers;
}

uint32 CRuntimeClass::AddAction(const CryGUID& guid, const CryGUID& typeGUID)
{
	m_actions.emplace_back(guid, typeGUID);
	return m_actions.size() - 1;
}

const RuntimeActionDescs& CRuntimeClass::GetActions() const
{
	return m_actions;
}

uint32 CRuntimeClass::CountSignalReceviers(const CryGUID& signalGUID) const
{
	uint32 count = 0;
	for (const SRuntimeClassState& state : m_states)
	{
		for (const SRuntimeClassStateSignalReceiver& stateSgnalReceiver : state.signalReceivers)
		{
			if (stateSgnalReceiver.signalGUID == signalGUID)
			{
				++count;
			}
		}
	}
	for (const SRuntimeClassSignalReceiver& signalReceiver : m_signalReceivers)
	{
		if (signalReceiver.signalGUID == signalGUID)
		{
			++count;
		}
	}
	return count;
}

void CRuntimeClass::FinalizeComponentInstances()
{
	// #SchematycTODO : Make sure parent->child relationshipes are taken into account!!!

	IEnvRegistry& envRegistry = gEnv->pSchematyc->GetEnvRegistry();

	const uint32 componentInstanceCount = m_componentInstances.size();
	ComponentInstanceSortRefs componentInstanceSortRefs;
	componentInstanceSortRefs.reserve(componentInstanceCount);

	for (uint32 componentInstanceIdx = 0; componentInstanceIdx < componentInstanceCount; ++componentInstanceIdx)
	{
		componentInstanceSortRefs.push_back(SComponentInstanceSortRef(componentInstanceIdx));

		const SRuntimeClassComponentInstance& componentInstance = m_componentInstances[componentInstanceIdx];
		const IEnvComponent* pEnvComponent = envRegistry.GetComponent(componentInstance.componentTypeGUID);
		if (pEnvComponent)
		{
			const CEntityComponentClassDesc& componentDesc = pEnvComponent->GetDesc();

			SComponentInstanceSortRef& componentInstanceSortRef = componentInstanceSortRefs.back();
			const DynArray<SEntityComponentRequirements>& componentInteractions = componentDesc.GetComponentInteractions();

			componentInstanceSortRef.dependencies.reserve(componentInteractions.size());

			for (const SEntityComponentRequirements& componentInteraction : componentInteractions)
			{
				for (uint32 otherComponentInstanceIdx = 0; otherComponentInstanceIdx < componentInstanceCount; ++otherComponentInstanceIdx)
				{
					if ((componentInteraction.type == SEntityComponentRequirements::EType::SoftDependency || componentInteraction.type == SEntityComponentRequirements::EType::HardDependency) &&
						m_componentInstances[otherComponentInstanceIdx].componentTypeGUID == componentInteraction.guid)
					{
						componentInstanceSortRef.dependencies.push_back(otherComponentInstanceIdx);
						if (componentInteraction.type == SEntityComponentRequirements::EType::HardDependency)
							break;
					}
				}
			}
		}
		else
		{
			CStackString guid;
			GUID::ToString(guid, componentInstance.componentTypeGUID);
			SCHEMATYC_CORE_ERROR("Unable to retrieve environment component: guid = %s", guid.c_str());
		}
	}

	bool bUnresolvedDependencies;
	do
	{
		bUnresolvedDependencies = false;
		for (uint32 componentInstanceIdx = 0; componentInstanceIdx < componentInstanceCount; ++componentInstanceIdx)
		{
			SComponentInstanceSortRef& componentInstanceSortRef = componentInstanceSortRefs[componentInstanceIdx];
			for (uint32 componentDependencyIdx : componentInstanceSortRef.dependencies)
			{
				SComponentInstanceSortRef& otherComponentInstanceSortRef = componentInstanceSortRefs[componentDependencyIdx];
				if (otherComponentInstanceSortRef.priority <= componentInstanceSortRef.priority)
				{
					otherComponentInstanceSortRef.priority = componentInstanceSortRef.priority + 1;
					bUnresolvedDependencies = true;
				}
			}
		}
	}
	while (bUnresolvedDependencies);
	std::sort(componentInstanceSortRefs.begin(), componentInstanceSortRefs.end(), SComponentInstanceSortPredicate());

	RuntimeClassComponentInstances componentInstances;
	componentInstances.reserve(componentInstanceCount);
	for (uint32 componentInstanceIdx = 0; componentInstanceIdx < componentInstanceCount; ++componentInstanceIdx)
	{
		componentInstances.push_back(m_componentInstances[componentInstanceSortRefs[componentInstanceIdx].componentInstanceIdx]);
	}
	std::swap(componentInstances, m_componentInstances);
}

void CRuntimeClass::Finalize()
{
	m_scratchpad.ShrinkToFit();

	m_graphs.shrink_to_fit();
	m_functions.shrink_to_fit();
	m_constructors.shrink_to_fit();
	m_stateMachines.shrink_to_fit();
	m_states.shrink_to_fit();
	m_variables.shrink_to_fit();
	m_timers.shrink_to_fit();
	m_componentInstances.shrink_to_fit();
	m_actions.shrink_to_fit();
	m_signalReceivers.shrink_to_fit();

	for (SRuntimeClassState& state : m_states)
	{
		state.signalReceivers.shrink_to_fit();
		state.transitions.shrink_to_fit();
		state.actions.shrink_to_fit();
	}
}

} // Schematyc
