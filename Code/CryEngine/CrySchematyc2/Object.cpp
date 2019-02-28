// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Object.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <CryRenderer/IRenderer.h>

#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/ILibRegistry.h>
#include <CrySchematyc2/IObjectManager.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Deprecated/Stack.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Runtime/IRuntime.h>
#include <CrySchematyc2/Runtime/RuntimeParams.h>

#include "CVars.h"

#ifdef _RELEASE
#define SCHEMATYC2_OBJECT_VM_DEBUG 0
#else
#define SCHEMATYC2_OBJECT_VM_DEBUG 1
#endif

namespace Schematyc2
{
	typedef std::vector<size_t> TSizeTVector;

	namespace
	{
		class CFunctionStack // #SchematycTODO : Replace with standard CStack class!?!
		{
		public:

			inline void Push(const CVariant& value)
			{
				m_data.push_back(value);
			}

			inline void Pop()
			{
				m_data.pop_back();
			}

			inline CVariant& Peek()
			{
				CRY_ASSERT(!m_data.empty());
				return m_data.back();
			}

			inline const CVariant& Peek() const
			{
				CRY_ASSERT(!m_data.empty());
				return m_data.back();
			}

#if SCHEMATYC2_OBJECT_VM_DEBUG
			inline bool ValidateCopy(size_t srcPos, size_t dstPos)
			{
				return ((srcPos == INVALID_INDEX) || (srcPos < m_data.size())) && ((dstPos == INVALID_INDEX) || (dstPos < m_data.size()));
			}
#endif

			inline void Copy(size_t srcPos, size_t dstPos)
			{
				if(srcPos == INVALID_INDEX)
				{
					srcPos = m_data.size() - 1;
				}
				if(dstPos == INVALID_INDEX)
				{
					m_data.push_back(m_data[srcPos]);
				}
				else
				{
					m_data[dstPos] = m_data[srcPos];
				}
			}

			inline size_t GetSize() const
			{
				return m_data.size();
			}

			inline void Reserve(size_t size)
			{
				m_data.reserve(size);
			}

			inline void Resize(size_t size)
			{
				m_data.resize(size);
			}

			inline CVariant& operator [] (size_t pos)
			{
				CRY_ASSERT(pos < m_data.size());
				return m_data[pos];
			}

			inline const CVariant& operator [] (size_t pos) const
			{
				CRY_ASSERT(pos < m_data.size());
				return m_data[pos];
			}

		private:

			TVariantVector	m_data;
		};

		inline void CryLogVariants(const TVariantConstArray& variants)
		{
			char	stringBuffer[256] = "";
			for(size_t iVariant = 0, variantCount = variants.size(); iVariant < variantCount; ++ iVariant)
			{
				StringUtils::VariantToString(variants[iVariant], stringBuffer);
				CryLogAlways(stringBuffer);
			}
		}
	}

	namespace ObjectUtils
	{
		bool ShouldStartAction(EActionFlags flags, bool bHaveNetworkAuthority)
		{
			const bool bIsServer = gEnv->bServer;
			const bool bIsClient = gEnv->IsClient();

			if(!bIsServer && ((flags & EActionFlags::ServerOnly) != 0))
			{
				return false;
			}
			if(!bIsClient && ((flags & EActionFlags::ClientOnly) != 0))
			{
				return false;
			}

			if(bHaveNetworkAuthority)
			{
				return true;
			}
			else if(bIsServer && ((flags & EActionFlags::NetworkReplicateServer) != 0))
			{
				return true;
			}
			else if(bIsClient && ((flags & EActionFlags::NetworkReplicateClients) != 0))
			{
				return true;
			}

			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CObject::CObject(const ObjectId& objectId, const SObjectParams& params)
		: m_objectId(objectId)
		, m_pLibClass(params.pLibClass)
		, m_pNetworkObject(params.pNetworkObject)
		, m_entityId(params.entityId)
		, m_serverAspect(params.serverAspect)
		, m_clientAspect(params.clientAspect)
		, m_flags(params.flags)
		, m_simulationMode(ESimulationMode::NotSet)
	{
		// Register network serializer?
		if(m_pNetworkObject)
		{
			const NetworkSerializeCallbackConnection networkSerializeCallbackConnection(NetworkSerializeCallback::FromMemberFunction<CObject, &CObject::NetworkSerialize>(*this), TemplateUtils::CScopedConnection(m_connectionScope));
			m_pNetworkObject->RegisterNetworkSerializer(networkSerializeCallbackConnection, m_serverAspect | m_clientAspect);
		}
		// Create state machines.
		m_stateMachines.resize(m_pLibClass->GetStateMachineCount());
		// Create and initialize variants.
		TVariantConstArray libVariants = m_pLibClass->GetVariants();
		const size_t       variantCount = libVariants.size();
		m_variants.reserve(variantCount);
		for(size_t variantIdx = 0; variantIdx < variantCount; ++ variantIdx)
		{
			m_variants.push_back(libVariants[variantIdx]);
		}
		// Initialize properties.
		if(params.pProperties)
		{
			InitProperties(*params.pProperties);
		}
		// Create and initialize containers.
		const size_t	containerCount = m_pLibClass->GetContainerCount();
		m_containers.resize(containerCount);
		// Create timers.
		ITimerSystem&	timerSystem = gEnv->pSchematyc2->GetTimerSystem();
		const size_t	timerCount = m_pLibClass->GetTimerCount();
		m_timers.reserve(timerCount);
		for(size_t iTimer = 0; iTimer < timerCount; ++ iTimer)
		{
			const ILibTimer&	libTimer = *m_pLibClass->GetTimer(iTimer);
			m_timers.push_back(STimer(this, libTimer.GetGUID()));
			STimer&				timer = m_timers.back();
			STimerParams	timerParams = libTimer.GetParams();
			timerParams.flags &= ~ETimerFlags::AutoStart;	// We're hijacking this flag and therefore don't want to pass it on to the timer system.
			timer.timerId = timerSystem.CreateTimer(timerParams, TimerCallback::FromMemberFunction<STimer, &STimer::OnTimer>(timer));
		}
		// Create and initialize components.
		CreateAndInitComponents(params.pNetworkSpawnParams);
		// Create and initialize actions.
		CreateAndInitActions();

		// Initialize network serialization helper
		// TODO pavloi 2017.11.03: no need to initialize mapper for each object, it's enough to do it once per libClass
		// Discuss with Dominique, if how to fit it better there
		if (m_pNetworkObject && (m_flags & EObjectFlags::NetworkReplicateActions) != 0)
		{
			m_stateNetIdxMapper.Init(m_pLibClass.get(), m_actionInstances);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CObject::~CObject()
	{
		// Call persistent destructors if we already ran constructors.
		if(m_simulationMode == ESimulationMode::Game)
		{
			for(size_t iPersistentDestructor = 0, persistentDestructorCount = m_pLibClass->GetPersistentDestructorCount(); iPersistentDestructor < persistentDestructorCount; ++ iPersistentDestructor)
			{
				const ILibDestructor&	libDestructor = *m_pLibClass->GetDestructor(m_pLibClass->GetPersistentDestructor(iPersistentDestructor));
				ProcessFunction(libDestructor.GetFunctionId(), TVariantConstArray(), TVariantArray());
			}
		}
		// Shutdown actions.
		for(TActionInstanceVector::reverse_iterator iActionInstance = m_actionInstances.rbegin(), iEndActionInstance = m_actionInstances.rend(); iActionInstance != iEndActionInstance; ++ iActionInstance)
		{
			if(iActionInstance->active)
			{
				iActionInstance->pAction->Stop();
			}
			iActionInstance->pAction->Shutdown();
		}
		// Shutdown components.
		for(ComponentInstances::reverse_iterator itComponentInstance = m_componentInstances.rbegin(), itEndComponentInstance = m_componentInstances.rend(); itComponentInstance != itEndComponentInstance; ++ itComponentInstance)
		{
			itComponentInstance->pComponent->Shutdown();
		}
		// Destroy timers.
		ITimerSystem&	timerSystem = gEnv->pSchematyc2->GetTimerSystem();
		for(TTimerVector::const_iterator iTimer = m_timers.begin(), iEndTimer = m_timers.end(); iTimer != iEndTimer; ++ iTimer)
		{
			timerSystem.DestroyTimer(iTimer->timerId);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::Run(ESimulationMode simulationMode)
	{
		m_simulationMode = simulationMode;
		// Run components.
		for(SComponentInstance& componentInstance : m_componentInstances)
		{
			componentInstance.pComponent->Run(m_simulationMode);
		}
		if(m_simulationMode == ESimulationMode::Game || (m_simulationMode == ESimulationMode::Editing && ((m_flags & EObjectFlags::UpdateInEditor) != 0)))
		{
			const bool bHaveNetworkAuthority = HaveNetworkAuthority();
			// Start persistent actions.
			if(bHaveNetworkAuthority || ((m_flags & EObjectFlags::NetworkReplicateActions) != 0))
			{
				for(size_t persistentActionInstanceIdx = 0, persistentActionInstanceCount = m_pLibClass->GetPersistentActionInstanceCount(); persistentActionInstanceIdx < persistentActionInstanceCount; ++ persistentActionInstanceIdx)
				{
					SActionInstance& actionInstance = m_actionInstances[m_pLibClass->GetPersistentActionInstance(persistentActionInstanceIdx)];
					if(ObjectUtils::ShouldStartAction(actionInstance.flags, bHaveNetworkAuthority))
					{
						actionInstance.pAction->Start();
						actionInstance.active = true;
					}
				}
			}
			if(bHaveNetworkAuthority)
			{
				// Start persistent timers.
				ITimerSystem& timerSystem = gEnv->pSchematyc2->GetTimerSystem();
				for(size_t persistentTimerIdx = 0, persistentTimerCount = m_pLibClass->GetPersistentTimerCount(); persistentTimerIdx < persistentTimerCount; ++ persistentTimerIdx)
				{
					const size_t     timerIdx = m_pLibClass->GetPersistentTimer(persistentTimerIdx);
					const ILibTimer& libTimer = *m_pLibClass->GetTimer(timerIdx); 
					if((libTimer.GetParams().flags & ETimerFlags::AutoStart) != 0)
					{
						timerSystem.StartTimer(m_timers[timerIdx].timerId);
					}
				}
				TVariantConstArray inputs;
				TVariantArray      outputs;
				// Call persistent constructors.
				for(size_t persistentConstructorIdx = 0, persistentConstructorCount = m_pLibClass->GetPersistentConstructorCount(); persistentConstructorIdx < persistentConstructorCount; ++ persistentConstructorIdx)
				{
					const ILibConstructor& libConstructor = *m_pLibClass->GetConstructor(m_pLibClass->GetPersistentConstructor(persistentConstructorIdx));
					ProcessFunction(libConstructor.GetFunctionId(), inputs, outputs);
				}
				// Call begin function for each persistent state machine.
				for(size_t stateMachineIdx = 0, stateMachineCount = m_pLibClass->GetStateMachineCount(); stateMachineIdx < stateMachineCount; ++ stateMachineIdx)
				{
					const ILibStateMachine& libStateMachine = *m_pLibClass->GetStateMachine(stateMachineIdx);
					if(libStateMachine.GetLifetime() == ELibStateMachineLifetime::Persistent)
					{
						CVariant beginOutputs[2];
						ProcessFunction(libStateMachine.GetBeginFunction(), TVariantConstArray(), beginOutputs);
						if(static_cast<ELibTransitionResult>(beginOutputs[0].AsInt32()) == ELibTransitionResult::ChangeState)
						{
							ChangeState(stateMachineIdx, beginOutputs[1].AsInt32(), SEvent(), TVariantConstArray());
						}
					}
				}
			}
			else
			{
				// Set initial states.
				if((m_flags & EObjectFlags::NetworkReplicateActions) != 0)
				{
					for(size_t stateMachineIdx = 0, stateMachineCount = m_stateMachines.size(); stateMachineIdx < stateMachineCount; ++ stateMachineIdx)
					{
						SStateMachine& stateMachine = m_stateMachines[stateMachineIdx];
						if((stateMachine.iCurrentState != stateMachine.iRequestedState))
						{
							ChangeState(stateMachineIdx, stateMachine.iRequestedState, SEvent(), TVariantConstArray());
						}
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::ConnectSignalObserver(const SGUID& signalGUID, const SignalCallback& signalCallback, TemplateUtils::CConnectionScope& connectionScope)
	{
		m_signalObservers.Add(TSignalObserver(signalGUID, signalCallback), connectionScope);
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::DisconnectSignalObserver(const SGUID& signalGUID, const SignalCallback& signalCallback, TemplateUtils::CConnectionScope& connectionScope)
	{
		m_signalObservers.Remove(TSignalObserver(signalGUID, signalCallback), connectionScope);
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::ProcessSignal(const SGUID& signalGUID, const TVariantConstArray& inputs)
	{
		if((m_simulationMode == ESimulationMode::Game) && HaveNetworkAuthority())
		{
			CRY_PROFILE_FUNCTION(PROFILE_GAME);

			m_signalHistory.PushBack(signalGUID);
			SSignalObserverConnectionProcessor signalObserverConnectionProcessor(signalGUID, inputs);
			m_signalObservers.Process(signalObserverConnectionProcessor);
			// Iterate through hierarchy of each state machine from current state to root to process signal receivers and evaluate transitions.
			SEvent        event(SEvent::SIGNAL, signalGUID);
			TVariantArray outputs;
			for(size_t iStateMachine = 0, stateMachineCount = m_stateMachines.size(); iStateMachine < stateMachineCount; ++ iStateMachine)
			{
				SStateMachine& stateMachine = m_stateMachines[iStateMachine];
				const size_t   initStateIdx = stateMachine.iCurrentState;
				// Process signal receivers.
				for(size_t stateIdx = initStateIdx; stateIdx != INVALID_INDEX; stateIdx = m_pLibClass->GetState(stateIdx)->GetParent())
				{
					const ILibState& libState = *m_pLibClass->GetState(stateIdx);
					for(size_t signalReceiverIdx = 0, signalReceiverCount = libState.GetSignalReceiverCount(); signalReceiverIdx < signalReceiverCount; ++ signalReceiverIdx)
					{
						const ILibSignalReceiver& libSignalReceiver = *m_pLibClass->GetSignalReceiver(libState.GetSignalReceiver(signalReceiverIdx));
						if(libSignalReceiver.GetContextGUID() == signalGUID)
						{
							if(stateMachine.iCurrentState == initStateIdx)
							{
								ProcessFunction(libSignalReceiver.GetFunctionId(), inputs, outputs);
							}
						}
					}

				}
				// Evaluate transitions if we haven't already changed state as a result of incoming signal
				if(stateMachine.iCurrentState == initStateIdx)
				{
					for(size_t stateIdx = stateMachine.iCurrentState; stateIdx != INVALID_INDEX; stateIdx = m_pLibClass->GetState(stateIdx)->GetParent())
					{
						if(EvaluateTransitions(iStateMachine, stateIdx, event, inputs))
						{
							break;
						}
					}
				}
			}
			// Process persistent signal receivers.
			for(size_t iPersistentSignalReceiver = 0, persistentSignalReceiverCount = m_pLibClass->GetPersistentSignalReceiverCount(); iPersistentSignalReceiver < persistentSignalReceiverCount; ++ iPersistentSignalReceiver)
			{
				const ILibSignalReceiver&	libSignalReceiver = *m_pLibClass->GetSignalReceiver(m_pLibClass->GetPersistentSignalReceiver(iPersistentSignalReceiver));
				if(libSignalReceiver.GetContextGUID() == signalGUID)
				{
					ProcessFunction(libSignalReceiver.GetFunctionId(), inputs, outputs);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CObject::CallAbstractInterfaceFunction(const SGUID& interfaceGUID, const SGUID& functionGUID, const TVariantConstArray& inputs, const TVariantArray& outputs)
	{
		if((m_simulationMode == ESimulationMode::Game) && HaveNetworkAuthority())
		{
			if(const ILibAbstractInterfaceImplementation* pAbstractInterfaceImplementation = m_pLibClass->GetAbstractInterfaceImplementation(LibUtils::FindAbstractInterfaceImplementation(*m_pLibClass, interfaceGUID)))
			{
				const LibFunctionId	functionId = pAbstractInterfaceImplementation->GetFunctionId(LibUtils::FindAbstractInterfaceFunction(*pAbstractInterfaceImplementation, functionGUID));
				if(functionId != LibFunctionId::s_invalid)
				{
					ProcessFunction(functionId, inputs, outputs);
					return true;
				}
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CObject::RunTask(const SGUID& stateMachineGUID, CStack& stack, const TaskCallbackConnection& callbackConnection)
	{
		if((m_simulationMode == ESimulationMode::Game) && HaveNetworkAuthority())
		{
			const size_t	iStateMachine = LibUtils::FindStateMachineByGUID(*m_pLibClass, stateMachineGUID);
			CRY_ASSERT(iStateMachine != INVALID_INDEX);
			if(iStateMachine != INVALID_INDEX)
			{
				const ILibStateMachine&	libStateMachine = *m_pLibClass->GetStateMachine(iStateMachine);
				if((libStateMachine.GetLifetime() == ELibStateMachineLifetime::Task) && (m_stateMachines[iStateMachine].iCurrentState == INVALID_INDEX))
				{
					// Store and task properties.
					m_stateMachines[iStateMachine].taskCallbackConnection = callbackConnection;
					InitStateMachineVariables(iStateMachine, stack);
					// Call begin function on state machine.
					CVariant	outputs[2];
					ProcessFunction(libStateMachine.GetBeginFunction(), TVariantConstArray(), outputs);
					if(static_cast<ELibTransitionResult>(outputs[0].AsInt32()) == ELibTransitionResult::ChangeState)
					{
						ChangeState(iStateMachine, outputs[1].AsInt32(), SEvent(), TVariantConstArray());
					}
					return true;
				}
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::CancelTask(const SGUID& stateMachineGUID)
	{
		if((m_simulationMode == ESimulationMode::Game) && HaveNetworkAuthority())
		{
			const size_t	iStateMachine = LibUtils::FindStateMachineByGUID(*m_pLibClass, stateMachineGUID);
			CRY_ASSERT(iStateMachine != INVALID_INDEX);
			if(iStateMachine != INVALID_INDEX)
			{
				const ILibStateMachine&	libStateMachine = *m_pLibClass->GetStateMachine(iStateMachine);
				if((libStateMachine.GetLifetime() == ELibStateMachineLifetime::Task) && (m_stateMachines[iStateMachine].iCurrentState != INVALID_INDEX))
				{
					ChangeState(iStateMachine, INVALID_INDEX, SEvent(), TVariantConstArray());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IObjectPreviewPropertiesPtr CObject::GetPreviewProperties() const
	{
		return std::make_shared<SPreviewProperties>(*m_pLibClass, m_componentInstances);
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const IObjectPreviewProperties& previewProperties) const
	{
		const SPreviewProperties& previewPropertiesImpl = static_cast<const SPreviewProperties&>(previewProperties);
		for(uint32 componentIdx = 0, componentCount = m_componentInstances.size(); componentIdx < componentCount; ++ componentIdx)
		{
			m_componentInstances[componentIdx].pComponent->Preview(params, passInfo, *previewPropertiesImpl.componentPreviewProperties[componentIdx].pData);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	ObjectId CObject::GetObjectId() const
	{
		return m_objectId;
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibClass& CObject::GetLibClass() const
	{
		return *m_pLibClass;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::InitProperties(const ILibClassProperties& properties)
	{
		const uint32 variableCount = m_pLibClass->GetVariableCount();
		for(uint32 variableIdx = 0; variableIdx < variableCount; ++ variableIdx)
		{
			const ILibVariable& libVariable = *m_pLibClass->GetVariable(variableIdx);
			if((libVariable.GetFlags() & ELibVariableFlags::ClassProperty) != 0)
			{
				IAnyConstPtr pPropertyValue = properties.GetProperty(libVariable.GetGUID());
				if(pPropertyValue)
				{
					SetVariable(variableIdx, *pPropertyValue);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::CreateAndInitComponents(const IObjectNetworkSpawnParamsPtr& pNetworkSpawnParams)
	{
		IEnvRegistry& envRegistry = gEnv->pSchematyc2->GetEnvRegistry();
		const uint32  componentInstanceCount = m_pLibClass->GetComponentInstanceCount();
		m_componentInstances.reserve(componentInstanceCount);
		for(uint32 componentInstanceIdx = 0; componentInstanceIdx < componentInstanceCount; ++ componentInstanceIdx)
		{
			const ILibComponentInstance& libComponentInstance = *m_pLibClass->GetComponentInstance(componentInstanceIdx);
			IComponentFactoryConstPtr    pComponentFactory = envRegistry.GetComponentFactory(libComponentInstance.GetComponentGUID());
			SCHEMATYC2_SYSTEM_ASSERT(pComponentFactory);
			if(pComponentFactory)
			{
				IComponentPtr pComponent = pComponentFactory->CreateComponent();
				SCHEMATYC2_SYSTEM_ASSERT(pComponent);
				if(pComponent)
				{
					IPropertiesPtr pComponentProperties = libComponentInstance.GetProperties()->Clone(); // #SchematycTODO : Do we need to double check the result of GetProperties()?
					m_componentInstances.push_back(SComponentInstance(pComponentProperties, pComponent));
					// Run component property function.
					const uint32 componentPropertyFunctionIdx = libComponentInstance.GetPropertyFunctionIdx();
					if(componentPropertyFunctionIdx != s_invalidIdx)
					{
						const CRuntimeParams inputs;
						CRuntimeParams       outputs;
						ExecuteRuntimeFunction(*m_pLibClass->GetFunction_New(componentPropertyFunctionIdx), this, inputs, outputs);
					}
					// Initialize component.
					const uint32     parentIdx = libComponentInstance.GetParentIdx();
					SComponentParams componentParams(*this, *pComponentProperties);
					componentParams.pNetworkSpawnParams = pNetworkSpawnParams ? pNetworkSpawnParams->GetComponentSpawnParams(componentInstanceIdx) : INetworkSpawnParamsPtr();
					componentParams.pParent             = parentIdx != s_invalidIdx ? m_componentInstances[parentIdx].pComponent.get() : nullptr;
					pComponent->Init(componentParams); // #SchematycTODO : What do we do if init fails?
				}
			}
		}
	}

	///////////////////////////////////////////////////////////////////////
	void CObject::CreateAndInitActions()
	{
		IEnvRegistry& envRegistry = gEnv->pSchematyc2->GetEnvRegistry();
		const uint32  actionInstanceCount = m_pLibClass->GetActionInstanceCount();
		m_actionInstances.reserve(actionInstanceCount);
		for(uint32 actionInstanceIdx = 0; actionInstanceIdx < actionInstanceCount; ++ actionInstanceIdx)
		{
			const ILibActionInstance& libActionInstance = *m_pLibClass->GetActionInstance(actionInstanceIdx);
			IActionFactoryConstPtr    pActionFactory = envRegistry.GetActionFactory(libActionInstance.GetActionGUID());
			SCHEMATYC2_SYSTEM_ASSERT(pActionFactory);
			if(pActionFactory)
			{
				IActionPtr pAction = pActionFactory->CreateAction();
				SCHEMATYC2_SYSTEM_ASSERT(pAction);
				if(pAction)
				{
					m_actionInstances.push_back(SActionInstance(pAction, pActionFactory->GetFlags(), false));
					IComponent*  pComponent = nullptr;
					const size_t componentInstanceIdx = libActionInstance.GetComponentInstance();
					if(componentInstanceIdx != INVALID_INDEX)
					{
						pComponent = m_componentInstances[componentInstanceIdx].pComponent.get();
					}
					// Initialize action.
					// #SchematycTODO : What do we do if Init fails?
					pAction->Init(SActionParams(*this, pComponent, *libActionInstance.GetProperties()));	
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	INetworkObject& CObject::GetNetworkObject() const
	{
		CRY_ASSERT(m_pNetworkObject);
		return *m_pNetworkObject;
	}

	//////////////////////////////////////////////////////////////////////////
	INetworkObject* CObject::GetNetworkObjectPtr() const
	{
		return m_pNetworkObject;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::GetNetworkSpawnParams(IObjectNetworkSpawnParamsPtr& pSpawnParams) const
	{
		CRY_ASSERT(pSpawnParams);
		for(uint32 componentInstanceIdx = 0, componentInstanceCount = m_componentInstances.size(); componentInstanceIdx < componentInstanceCount; ++ componentInstanceIdx)
		{
			INetworkSpawnParamsPtr pComponentSpawnParams = m_componentInstances[componentInstanceIdx].pComponent->GetNetworkSpawnParams();
			if(pComponentSpawnParams)
			{
				pSpawnParams->AddComponentSpawnParams(componentInstanceIdx, pComponentSpawnParams);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	ExplicitEntityId CObject::GetEntityId() const
	{
		return m_entityId;
	}

	//////////////////////////////////////////////////////////////////////////
	IComponent* CObject::GetSingletonComponent(const SGUID& componentGUID) const
	{
		for(uint32 componentInstanceIdx = 0, componentInstanceCount = m_pLibClass->GetComponentInstanceCount(); componentInstanceIdx < componentInstanceCount; ++ componentInstanceIdx)
		{
			const ILibComponentInstance& libComponentInstance = *m_pLibClass->GetComponentInstance(componentInstanceIdx);
			if(libComponentInstance.GetComponentGUID() == componentGUID) // #SchematycTODO : Store guid in component instance?
			{
				return m_componentInstances[componentInstanceIdx].pComponent.get(); // #SchematycTODO : Double check component is singleton!!!
			}
		}
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CObject::GetState(size_t iStateMachine) const
	{
		return iStateMachine < m_stateMachines.size() ? m_stateMachines[iStateMachine].iCurrentState : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::SetVariable(size_t iVariable, const IAny& value)
	{
		CRY_ASSERT(iVariable < m_pLibClass->GetVariableCount());
		if(iVariable < m_pLibClass->GetVariableCount())
		{
			const ILibVariable&	libVariable = *m_pLibClass->GetVariable(iVariable);
			IAnyConstPtr				pVariableValue = libVariable.GetValue();
			CRY_ASSERT(pVariableValue != NULL);
			if(pVariableValue != NULL)
			{
				CRY_ASSERT(value.GetTypeInfo().GetTypeId() == pVariableValue->GetTypeInfo().GetTypeId());
				if(value.GetTypeInfo().GetTypeId() == pVariableValue->GetTypeInfo().GetTypeId())
				{
					CVariantArrayOutputArchive	archive(TVariantArray(&m_variants[libVariable.GetVariantPos()], libVariable.GetVariantCount()));
					archive(value, "", "");
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CVariant CObject::GetVariant(size_t iVariant) const
	{
		return iVariant < m_variants.size() ? m_variants[iVariant] : CVariant();
	}

	//////////////////////////////////////////////////////////////////////////
	const CVariantContainer* CObject::GetContainer(size_t iContainer) const
	{
		return iContainer < m_containers.size() ? &m_containers[iContainer] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CObject::GetProperty(uint32 propertyIdx) const
	{
		// N.B. This implementation is complete hack. At some point all variables and properties should be stored in a scratch-pad and we won't need to serialize from variants.
		SCHEMATYC2_SYSTEM_ASSERT(propertyIdx < m_pLibClass->GetVariableCount());
		if(propertyIdx < m_pLibClass->GetVariableCount())
		{
			const ILibVariable& libVariable = *m_pLibClass->GetVariable(propertyIdx);
			IAnyConstPtr        pLibVariableValue = libVariable.GetValue();
			if(pLibVariableValue)
			{
				IAnyPtr                   pPropertyValue = pLibVariableValue->Clone();
				CVariantArrayInputArchive archive(TVariantConstArray(&m_variants[libVariable.GetVariantPos()], libVariable.GetVariantCount()));
				archive(*pPropertyValue, "", "");
				return pPropertyValue;
			}
		}
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	TimerId CObject::GetTimerId(size_t iTimer) const
	{
		return iTimer < m_timers.size() ? m_timers[iTimer].timerId : s_invalidTimerId;
	}

	//////////////////////////////////////////////////////////////////////////
	IComponentPtr CObject::GetComponentInstance(size_t componentInstanceIdx)
	{
		return componentInstanceIdx < m_componentInstances.size() ? m_componentInstances[componentInstanceIdx].pComponent : IComponentPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	IPropertiesPtr CObject::GetComponentInstanceProperties(size_t componentInstanceIdx)
	{
		return componentInstanceIdx < m_componentInstances.size() ? m_componentInstances[componentInstanceIdx].pProperties : IPropertiesPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::VisitActiveTimers(const ObjectActiveTimerVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			ITimerSystem&	timerSystem = gEnv->pSchematyc2->GetTimerSystem();
			for(size_t iTimer = 0, timerCount = m_timers.size(); iTimer < timerCount; ++ iTimer)
			{
				if(timerSystem.IsTimerActive(m_timers[iTimer].timerId) == true)
				{
					if(visitor(iTimer) != EVisitStatus::Continue)
					{
						return;
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::VisitActiveActionInstances(const ObjectActiveActionInstanceVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(size_t iActionInstance = 0, actionInstanceCount = m_actionInstances.size(); iActionInstance < actionInstanceCount; ++ iActionInstance)
			{
				if(m_actionInstances[iActionInstance].active)
				{
					if(visitor(iActionInstance) != EVisitStatus::Continue)
					{
						return;
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	const ObjectSignalHistory& CObject::GetSignalHistory() const
	{
		return m_signalHistory;
	}

	//////////////////////////////////////////////////////////////////////////
	CObject::SStateMachine::SStateMachine()
		: iCurrentState(INVALID_INDEX)
		, iRequestedState(INVALID_INDEX)
	{}

	//////////////////////////////////////////////////////////////////////////
	CObject::STimer::STimer(CObject* _pObject, const SGUID& _guid)
		: pObject(_pObject)
		, guid(_guid)
		, timerId(s_invalidTimerId)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CObject::STimer::OnTimer()
	{
		CRY_ASSERT(pObject != NULL);
		if(pObject != NULL)
		{
			TVariantConstArray	inputs;
			pObject->ProcessSignal(guid, inputs);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CObject::SComponentInstance::SComponentInstance(const IPropertiesPtr& _pProperties, const IComponentPtr& _pComponent)
		: pProperties(_pProperties)
		, pComponent(_pComponent)
	{}

	//////////////////////////////////////////////////////////////////////////
	CObject::SActionInstance::SActionInstance(const IActionPtr& _pAction, EActionFlags _flags, bool _active)
		: pAction(_pAction)
		, flags(_flags)
		, active(_active)
	{}

	//////////////////////////////////////////////////////////////////////////
	CObject::SEvent::SEvent()
		: type(NONE)
	{}

	//////////////////////////////////////////////////////////////////////////
	CObject::SEvent::SEvent(EType _type, const SGUID& _guid)
		: type(_type)
		, guid(_guid)
	{}

	//////////////////////////////////////////////////////////////////////////
	int32 CObject::GetNetworkAspect() const
	{
		const bool bClientAuthority = m_pNetworkObject ? m_pNetworkObject->ClientAuthority() : false;
		return bClientAuthority ? m_clientAspect : m_serverAspect;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::MarkAspectDirtyForStateMachine(size_t iStateMachine)
	{
		if (m_pNetworkObject && HaveNetworkAuthority() && (m_flags & EObjectFlags::NetworkReplicateActions) != 0)
		{
			if (m_stateNetIdxMapper.GetStateCount(iStateMachine) != 0)
			{
				m_pNetworkObject->MarkAspectsDirty(GetNetworkAspect());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CObject::SPreviewProperties::SComponentPreviewProperties::SComponentPreviewProperties(const char* _componentName, const IComponentPreviewPropertiesPtr& _pData)
		: componentName(_componentName)
		, pData(_pData)
	{}

	//////////////////////////////////////////////////////////////////////////
	CObject::SPreviewProperties::SPreviewProperties(const ILibClass& libClass, const ComponentInstances& componentInstances)
	{
		const uint32 componentCount = componentInstances.size();
		componentPreviewProperties.reserve(componentCount);
		for(uint32 componentIdx = 0; componentIdx < componentCount; ++ componentIdx)
		{
			componentPreviewProperties.push_back(SComponentPreviewProperties(libClass.GetComponentInstance(componentIdx)->GetName(), componentInstances[componentIdx].pComponent->GetPreviewProperties()));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::SPreviewProperties::Serialize(Serialization::IArchive& archive)
	{
		for(SComponentPreviewProperties& _componentPreviewProperties : componentPreviewProperties)
		{
			if(_componentPreviewProperties.pData)
			{
				const char* szComponentName = _componentPreviewProperties.componentName.c_str();
				archive(*_componentPreviewProperties.pData, szComponentName, szComponentName);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CObject::HaveNetworkAuthority() const
	{
		return m_pNetworkObject ? m_pNetworkObject->LocalAuthority() : true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CObject::EvaluateTransitions(size_t iStateMachine, size_t iState, const SEvent& event, const TVariantConstArray& inputs)
	{
		bool	stateChanged = false;
		if(iState != INVALID_INDEX)
		{
			SStateMachine&		stateMachine = m_stateMachines[iStateMachine];
			const ILibState&	libState = *m_pLibClass->GetState(iState);
			for(size_t iTransition = 0, transitionCount = libState.GetTransitionCount(); iTransition < transitionCount; ++ iTransition)
			{
				const ILibTransition&	libTransition = *m_pLibClass->GetTransition(libState.GetTransition(iTransition));
				if(libTransition.GetContextGUID() == event.guid)
				{
					CVariant	transitionOutputs[2];
					ProcessFunction(libTransition.GetFunctionId(), inputs, transitionOutputs);
					switch(static_cast<ELibTransitionResult>(transitionOutputs[0].AsInt32()))
					{
					case ELibTransitionResult::ChangeState:
						{
							ChangeState(iStateMachine, transitionOutputs[1].AsInt32(), event, inputs);
							stateChanged = true;
							break;
						}
					case ELibTransitionResult::EndSuccess:
						{
							ChangeState(iStateMachine, INVALID_INDEX, event, inputs);
							if(stateMachine.taskCallbackConnection.second.IsConnected())
							{
								stateMachine.taskCallbackConnection.first(true);
								stateMachine.taskCallbackConnection.second.Disconnect();
							}
							stateChanged = true;
							break;
						}
					case ELibTransitionResult::EndFailure:
						{
							ChangeState(iStateMachine, INVALID_INDEX, event, inputs);
							if(stateMachine.taskCallbackConnection.second.IsConnected())
							{
								stateMachine.taskCallbackConnection.first(false);
								stateMachine.taskCallbackConnection.second.Disconnect();
							}
							stateChanged = true;
							break;
						}
					}
				}
			}
		}
		return stateChanged;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::ChangeState(size_t iStateMachine, size_t iNewState, const SEvent& event, const TVariantConstArray& inputs)
	{
		SStateMachine&	stateMachine = m_stateMachines[iStateMachine];
		// Find first common ancestor between current and new states.
		TSizeTVector	currentStateStack;
		for(size_t iState = stateMachine.iCurrentState; iState != INVALID_INDEX; iState = m_pLibClass->GetState(iState)->GetParent())
		{
			currentStateStack.push_back(iState);
		}
		std::reverse(currentStateStack.begin(), currentStateStack.end());
		TSizeTVector	newStateStack;
		for(size_t iState = iNewState; iState != INVALID_INDEX; iState = m_pLibClass->GetState(iState)->GetParent())
		{
			newStateStack.push_back(iState);
		}
		std::reverse(newStateStack.begin(), newStateStack.end());
		const int32	currentDepth = static_cast<int32>(currentStateStack.size());
		const int32	newDepth = static_cast<int32>(newStateStack.size());
		int32				firstCommonAncestor = -1;
		for(int32 depth = /*std::*/min(currentDepth, newDepth); (firstCommonAncestor + 1) < depth; ++ firstCommonAncestor)
		{
			if(currentStateStack[firstCommonAncestor + 1] != newStateStack[firstCommonAncestor + 1])
			{
				break;
			}
		}
		// Exit old states and enter new states.
		for(int32 pos = currentDepth - 1; pos > firstCommonAncestor; -- pos)
		{
			ExitState(iStateMachine, currentStateStack[pos], event, inputs);
		}
		stateMachine.iCurrentState = iNewState;
		for(int32 pos = firstCommonAncestor + 1; pos < newDepth; ++ pos)
		{
			EnterState(iStateMachine, newStateStack[pos], inputs);
		}
		// Mark network aspect dirty?
		MarkAspectDirtyForStateMachine(iStateMachine);
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::EnterState(size_t iStateMachine, size_t iState, const TVariantConstArray& inputs)
	{
		const bool				bHaveNetworkAuthority = HaveNetworkAuthority();
		const ILibState&	libState = *m_pLibClass->GetState(iState);
		bool							stateChanged = false;
		if(bHaveNetworkAuthority)
		{
			// Request partner state?
			const size_t	iPartnerState = libState.GetPartner();
			if(const ILibState* pPartnerLibState = m_pLibClass->GetState(iPartnerState))
			{
				const size_t		iPartnerStateMachine = m_pLibClass->GetStateMachine(iStateMachine)->GetPartner();
				SCHEMATYC2_SYSTEM_ASSERT_FATAL_MESSAGE(iPartnerStateMachine != INVALID_INDEX,
					"StateMachine %s in class %s is not partnered, but has partnered state %s",
					m_pLibClass->GetStateMachine(iStateMachine)->GetName(),
					m_pLibClass->GetName(),
					libState.GetName());
				SStateMachine&	partnerStateMachine = m_stateMachines[iPartnerStateMachine];
				for(size_t iRequestState = partnerStateMachine.iCurrentState; iRequestState != INVALID_INDEX; iRequestState = m_pLibClass->GetState(iRequestState)->GetParent())
				{
					EvaluateTransitions(iPartnerStateMachine, iRequestState, SEvent(SEvent::REQUEST_STATE, pPartnerLibState->GetGUID()), TVariantConstArray());
					if(partnerStateMachine.iCurrentState == iPartnerState)
					{
						break;
					}
				}
				if(partnerStateMachine.iCurrentState != iPartnerState)
				{
					stateChanged |= EvaluateTransitions(iStateMachine, iState, SEvent(SEvent::FAILED_STATE, libState.GetGUID()), TVariantConstArray());
				}
			}
		}
		// Make sure partner request didn't fail.
		if(!stateChanged)
		{
			// Start actions.
			for(size_t iStateActionInstance = 0, stateActionInstanceCount = libState.GetActionInstanceCount(); iStateActionInstance < stateActionInstanceCount; ++ iStateActionInstance)
			{
				SActionInstance&	actionInstance = m_actionInstances[libState.GetActionInstance(iStateActionInstance)];
				if(ObjectUtils::ShouldStartAction(actionInstance.flags, bHaveNetworkAuthority))
				{
					actionInstance.pAction->Start();
					actionInstance.active = true;
				}
			}
			if(bHaveNetworkAuthority)
			{
				// Start timers.
				ITimerSystem&	timerSystem = gEnv->pSchematyc2->GetTimerSystem();
				for(size_t iStateTimer = 0, stateTimerCount = libState.GetTimerCount(); iStateTimer < stateTimerCount; ++ iStateTimer)
				{
					const size_t			iTimer = libState.GetTimer(iStateTimer);
					const ILibTimer&	libTimer = *m_pLibClass->GetTimer(iTimer); 
					if((libTimer.GetParams().flags & ETimerFlags::AutoStart) != 0)
					{
						timerSystem.StartTimer(m_timers[iTimer].timerId);
					}
				}
				// Call constructors.
				TVariantArray	outputs;
				for(size_t iStateConstructor = 0, statConstructorCount = libState.GetConstructorCount(); iStateConstructor < statConstructorCount; ++ iStateConstructor)
				{
					ProcessFunction(m_pLibClass->GetConstructor(libState.GetConstructor(iStateConstructor))->GetFunctionId(), inputs, outputs);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::ExitState(size_t iStateMachine, size_t iState, const SEvent& event, const TVariantConstArray& inputs)
	{
		const bool              bHaveNetworkAuthority = HaveNetworkAuthority();
		const ILibStateMachine& libStateMachine = *m_pLibClass->GetStateMachine(iStateMachine);
		const ILibState&        libState = *m_pLibClass->GetState(iState);
		if (bHaveNetworkAuthority)
		{
			// Call destructors.
			for (size_t iStateDestructor = 0, statDestructorCount = libState.GetDestructorCount(); iStateDestructor < statDestructorCount; ++iStateDestructor)
			{
				ProcessFunction(m_pLibClass->GetDestructor(libState.GetDestructor(iStateDestructor))->GetFunctionId(), inputs, TVariantArray());
			}
			// Stop timers.
			ITimerSystem&	timerSystem = gEnv->pSchematyc2->GetTimerSystem();
			for (size_t iStateTimer = 0, stateTimerCount = libState.GetTimerCount(); iStateTimer < stateTimerCount; ++iStateTimer)
			{
				timerSystem.StopTimer(m_timers[libState.GetTimer(iStateTimer)].timerId);
			}
		}
		// Stop actions.
		for(size_t iStateActionInstance = 0, stateActionInstanceCount = libState.GetActionInstanceCount(); iStateActionInstance < stateActionInstanceCount; ++ iStateActionInstance)
		{
			SActionInstance&	actionInstance = m_actionInstances[libState.GetActionInstance(iStateActionInstance)];
			if(actionInstance.active)
			{
				actionInstance.pAction->Stop();
				actionInstance.active = false;
			}
		}
		// Reset variables.
		TVariantConstArray	libVariants = m_pLibClass->GetVariants();
		for(size_t iStateVariable = 0, stateVariableCount = libState.GetVariableCount(); iStateVariable < stateVariableCount; ++ iStateVariable)
		{
			const ILibVariable&	libVariable = *m_pLibClass->GetVariable(libState.GetVariable(iStateVariable));
			for(size_t iVariant = libVariable.GetVariantPos(), iEndVariant = iVariant + libVariable.GetVariantCount(); iVariant < iEndVariant; ++ iVariant)
			{
				m_variants[iVariant] = libVariants[iVariant];
			}
		}
		if (bHaveNetworkAuthority)
		{
			// Release partner state?
			const size_t	iPartnerStateMachine = libStateMachine.GetPartner();
			const size_t	iPartnerState = libState.GetPartner();
			if (const ILibState* pPartnerLibState = m_pLibClass->GetState(iPartnerState))
			{
				SStateMachine&	partnerStateMachine = m_stateMachines[iPartnerStateMachine];
				if (partnerStateMachine.iCurrentState == iPartnerState)
				{
					EvaluateTransitions(iPartnerStateMachine, partnerStateMachine.iCurrentState, SEvent(SEvent::RELEASE_STATE, pPartnerLibState->GetGUID()), TVariantConstArray());
				}
			}
			// Notify listeners?
			if (event.type != SEvent::RELEASE_STATE)
			{
				for (size_t iListener = 0, listenerCount = libStateMachine.GetListenerCount(); iListener < listenerCount; ++iListener)
				{
					const size_t	iListenerStateMachine = libStateMachine.GetListener(iListener);
					EvaluateTransitions(iListenerStateMachine, m_stateMachines[iListenerStateMachine].iCurrentState, SEvent(SEvent::EXITING_STATE, libState.GetGUID()), TVariantConstArray());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::StartTimer(const SGUID& guid)
	{
		// #SchematycTODO : Create and use a LibUtils::FindTimer() function?
		for(size_t iTimer = 0, timerCount = m_timers.size(); iTimer < timerCount; ++ iTimer)
		{
			const ILibTimer&	libTimer = *m_pLibClass->GetTimer(iTimer);
			if(libTimer.GetGUID() == guid)
			{
				gEnv->pSchematyc2->GetTimerSystem().StartTimer(m_timers[iTimer].timerId);
				return;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::StopTimer(const SGUID& guid)
	{
		// #SchematycTODO : Create and use a LibUtils::FindTimer() function?
		for(size_t iTimer = 0, timerCount = m_timers.size(); iTimer < timerCount; ++ iTimer)
		{
			const ILibTimer&	libTimer = *m_pLibClass->GetTimer(iTimer);
			if(libTimer.GetGUID() == guid)
			{
				gEnv->pSchematyc2->GetTimerSystem().StopTimer(m_timers[iTimer].timerId);
				return;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::ResetTimer(const SGUID& guid)
	{
		// #SchematycTODO : Create and use a LibUtils::FindTimer() function?
		for(size_t iTimer = 0, timerCount = m_timers.size(); iTimer < timerCount; ++ iTimer)
		{
			const ILibTimer&	libTimer = *m_pLibClass->GetTimer(iTimer);
			if(libTimer.GetGUID() == guid)
			{
				gEnv->pSchematyc2->GetTimerSystem().ResetTimer(m_timers[iTimer].timerId);
				return;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::InitStateMachineVariables(size_t iStateMachine, CStack& stack)
	{
		const ILibStateMachine&	libStateMachine = *m_pLibClass->GetStateMachine(iStateMachine);
		TVariantConstArray			libVariants = m_pLibClass->GetVariants();
		for(int32 iVariable = libStateMachine.GetVariableCount() - 1; iVariable >= 0; -- iVariable)
		{
			const ILibVariable&	libVariable = *m_pLibClass->GetVariable(libStateMachine.GetVariable(iVariable));
			for(int32 iFirstVariant = libVariable.GetVariantPos(), iVariant = iFirstVariant + libVariable.GetVariantCount() - 1; iVariant >= iFirstVariant; -- iVariant)
			{
				bool	useDefaultValue = true;
				if((libVariable.GetFlags() & ELibVariableFlags::StateMachineProperty) != 0)
				{
					SCHEMATYC2_SYSTEM_ASSERT(!stack.Empty());
					if(!stack.Empty())
					{
						const CVariant	variant = stack.Top();
						SCHEMATYC2_SYSTEM_ASSERT(variant.GetTypeId() == m_variants[iVariant].GetTypeId());
						if(variant.GetTypeId() == m_variants[iVariant].GetTypeId())
						{
							m_variants[iVariant]	= stack.Top();
							useDefaultValue				= false;
						}
						stack.Pop();
					}
				}
				if(useDefaultValue)
				{
					m_variants[iVariant] = libVariants[iVariant];
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::ProcessFunction(const LibFunctionId& functionId, const TVariantConstArray& inputs, const TVariantArray& outputs)
	{
		CRY_PROFILE_FUNCTION_ARG(PROFILE_GAME, m_pLibClass->GetName());
		const ILibFunction*	pFunction = m_pLibClass->GetFunction(functionId);
		CRY_ASSERT(pFunction);
		if(pFunction)
		{
			LOADING_TIME_PROFILE_SECTION_NAMED_ARGS("ProcessFunction", pFunction->GetName());
			static uint32 s_recursionDepth = 0;
			if(s_recursionDepth >= CVars::sc_MaxRecursionDepth)
			{
				SCHEMATYC2_SYSTEM_METAINFO_CRITICAL_ERROR(
					SCHEMATYC2_LOG_METAINFO(ECryLinkCommand::Show, SLogMetaItemGUID(pFunction->GetGUID())), 
					"Exceeded maximum recursion depth: class = %s, function = %s", 
					m_pLibClass->GetName(), 
					pFunction->GetName()
				);
				return;
			}
			++ s_recursionDepth;

			const int64	startTicks = CryGetTicks();
			// Verify inputs and outputs.
			TVariantConstArray	defaultInputs = pFunction->GetVariantInputs();
			const size_t				inputCount = defaultInputs.size();
			TVariantConstArray	defaultOutputs = pFunction->GetVariantOutputs();
			const size_t				outputCount = defaultOutputs.size();
			CRY_ASSERT((inputs.size() >= inputCount) && (outputs.size() >= outputCount));
			if((inputs.size() >= inputCount) && (outputs.size() >= outputCount))
			{
				// Initialize stack.
				CFunctionStack	stack;
				stack.Reserve(inputCount + outputCount);
				for(size_t iOutput = 0; iOutput < outputCount; ++ iOutput)
				{
					stack.Push(defaultOutputs[iOutput]);
				}
				for(size_t iInput = 0; iInput < inputCount; ++ iInput)
				{
					stack.Push(inputs[iInput]);
				}
				// Process operations.
				const IEnvRegistry&								envRegistry = gEnv->pSchematyc2->GetEnvRegistry();
				const ILibRegistry&								libRegistry = gEnv->pSchematyc2->GetLibRegistry();
				GlobalFunctionConstTable					globalFunctionTable = pFunction->GetGlobalFunctionTable();
				ComponentMemberFunctionConstTable	componentMemberFunctionTable = pFunction->GetComponentMemberFunctionTable();
				ActionMemberFunctionConstTable		actionMemberFunctionTable = pFunction->GetActionMemberFunctionTable();
				for(size_t pos = 0, size = pFunction->GetSize(); pos < size; )
				{
					const SVMOp*	pOp = pFunction->GetOp(pos);
					switch(pOp->opCode)
					{
					case SVMOp::PUSH:
						{
							const SVMPushOp*	pPushOp = static_cast<const SVMPushOp*>(pOp);
							stack.Push(pFunction->GetVariantConsts()[pPushOp->iConstValue]);
							pos += pOp->size;
							break;
						}
					case SVMOp::SET:
						{
							const SVMSetOp*	pSetOp = static_cast<const SVMSetOp*>(pOp);
							stack[pSetOp->pos] = pFunction->GetVariantConsts()[pSetOp->iConstValue];
							pos += pOp->size;
							break;
						}
					case SVMOp::COPY:
						{
							const SVMCopyOp*	pCopyOp = static_cast<const SVMCopyOp*>(pOp);
#if SCHEMATYC2_OBJECT_VM_DEBUG
							if(!stack.ValidateCopy(pCopyOp->srcPos, pCopyOp->dstPos))
							{
								SCHEMATYC2_SYSTEM_CRITICAL_ERROR("Invalid copy operation: class = %s, function = %s, pos = %d", m_pLibClass->GetName(), pFunction->GetName(), pos);
							}
#endif
							stack.Copy(pCopyOp->srcPos, pCopyOp->dstPos);
							pos += pOp->size;
							break;
						}
					case SVMOp::POP:
						{
							const SVMPopOp*	pPopOp = static_cast<const SVMPopOp*>(pOp);
							stack.Resize(stack.GetSize() - pPopOp->count);
							pos += pOp->size;
							break;
						}
					case SVMOp::COLLAPSE:
						{
							const SVMCollapseOp*	pCollapseOp = static_cast<const SVMCollapseOp*>(pOp);
							stack.Resize(pCollapseOp->pos);
							pos += pOp->size;
							break;
						}
					case SVMOp::LOAD:
						{
							const SVMLoadOp*	pLoadOp = static_cast<const SVMLoadOp*>(pOp);
							for(size_t variantPos = pLoadOp->pos, variantEnd = variantPos + pLoadOp->count; variantPos < variantEnd; ++ variantPos)
							{
								stack.Push(m_variants[variantPos]);
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::STORE:
						{
							const SVMStoreOp*	pStoreOp = static_cast<const SVMStoreOp*>(pOp);
							for(size_t variantPos = pStoreOp->pos, variantEnd = variantPos + pStoreOp->count, stackPos = stack.GetSize() - pStoreOp->count; variantPos < variantEnd; ++ variantPos, ++ stackPos)
							{
								m_variants[variantPos] = stack[stackPos];
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::CONTAINER_ADD:
						{
							const SVMContainerAddOp*	pContainerAddOp = static_cast<const SVMContainerAddOp*>(pOp);
							CVariantContainer&				container = m_containers[pContainerAddOp->iContainer];
							container.Add(TVariantConstArray(&stack[stack.GetSize() - pContainerAddOp->count], pContainerAddOp->count));
							pos += pOp->size;
							break;
						}
					case SVMOp::CONTAINER_REMOVE_BY_INDEX:
						{
							const SVMContainerRemoveByIndexOp*	pContainerRemoveByIndexOp = static_cast<const SVMContainerRemoveByIndexOp*>(pOp);
							CVariantContainer&									container = m_containers[pContainerRemoveByIndexOp->iContainer];
							const size_t												containerPos = stack.Peek().AsInt32();
							const bool													result = container.RemoveByIndex(containerPos);
							stack.Push(CVariant(result));
							pos += pOp->size;
							break;
						}
					case SVMOp::CONTAINER_REMOVE_BY_VALUE:
						{
							const SVMContainerRemoveByValueOp*	pContainerRemoveByValueOp = static_cast<const SVMContainerRemoveByValueOp*>(pOp);
							CVariantContainer&									container = m_containers[pContainerRemoveByValueOp->iContainer];
							const bool													result = container.RemoveByValue(TVariantConstArray(&stack[stack.GetSize() - pContainerRemoveByValueOp->count], pContainerRemoveByValueOp->count));
							stack.Push(CVariant(result));
							pos += pOp->size;
							break;
						}
					case SVMOp::CONTAINER_CLEAR:
						{
							const SVMContainerClearOp*	pContainerClearOp = static_cast<const SVMContainerClearOp*>(pOp);
							m_containers[pContainerClearOp->iContainer].Clear();
							pos += pOp->size;
							break;
						}
					case SVMOp::CONTAINER_SIZE:
						{
							const SVMContainerSizeOp*	pContainerSizeOp = static_cast<const SVMContainerSizeOp*>(pOp);
							stack.Push(CVariant(static_cast<int32>(m_containers[pContainerSizeOp->iContainer].Size() / pContainerSizeOp->divisor)));
							pos += pOp->size;
							break;
						}
					case SVMOp::CONTAINER_GET:
						{
							const SVMContainerGetOp*	pContainerGetOp = static_cast<const SVMContainerGetOp*>(pOp);
							CVariantContainer&				container = m_containers[pContainerGetOp->iContainer];
							const size_t							containerPos = stack.Peek().AsInt32() * pContainerGetOp->count;
							if((containerPos + pContainerGetOp->count) <= container.Size())
							{
								for(size_t offset = 0; offset < pContainerGetOp->count; ++ offset)
								{
									stack.Push(container[containerPos + offset]);
								}
								stack.Push(CVariant(bool(true)));
							}
							else
							{
								stack.Push(CVariant(bool(false)));
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::CONTAINER_SET:
						{
							const SVMContainerSetOp*	pContainerSetOp = static_cast<const SVMContainerSetOp*>(pOp);
							CVariantContainer&				container = m_containers[pContainerSetOp->iContainer];

							// stack: ..., index, value[count]
							const TVariantConstArray	valuesView(&stack[stack.GetSize() - pContainerSetOp->count], pContainerSetOp->count);
							const size_t							containerPos = stack[stack.GetSize() - pContainerSetOp->count - 1].AsInt32();

							if ((containerPos + pContainerSetOp->count) <= container.Size())
							{
								for (size_t offset = 0; offset < pContainerSetOp->count; ++offset)
								{
									container[containerPos + offset] = valuesView[offset];
								}
								stack.Push(CVariant(bool(true)));
							}
							else
							{
								stack.Push(CVariant(bool(false)));
							}
							pos += pOp->size;


							break;
						}
					case SVMOp::CONTAINER_FIND_BY_VALUE:
						{
							const SVMContainerFindByValueOp*	pContainerFindByValueOp = static_cast<const SVMContainerFindByValueOp*>(pOp);
							const CVariantContainer&					container = m_containers[pContainerFindByValueOp->iContainer];
							// #SchematycTODO: only first value is used
							const size_t											resultIndex = container.FindByValue(stack[stack.GetSize() - pContainerFindByValueOp->count]);
							const bool												result = (resultIndex != CVariantContainer::npos);

							stack.Push(CVariant(int32(resultIndex)));
							stack.Push(CVariant(bool(result)));

							pos += pOp->size;
							break;
						}
					case SVMOp::COMPARE:
						{
							const SVMCompareOp*	pCompareOp = static_cast<const SVMCompareOp*>(pOp);
							bool								result = true;
							for(size_t offset = 0; offset < pCompareOp->count; ++ offset)
							{
								if(stack[pCompareOp->lhsPos + offset] != stack[pCompareOp->rhsPos + offset])
								{
									result = false;
									break;
								}
							}
							stack.Push(CVariant(result));
							pos += pOp->size;
							break;
						}
					case SVMOp::INCREMENT_INT32:
						{
							// #SchematycTODO : Probably worth adding some error checking here.
							const SVMIncrementInt32Op*	pIncrementInt32Op = static_cast<const SVMIncrementInt32Op*>(pOp);
							++ stack[pIncrementInt32Op->pos].AsInt32();
							pos += pOp->size;
							break;
						}
					case SVMOp::LESS_THAN_INT32:
						{
							// #SchematycTODO : Probably worth adding some error checking here.
							const SVMLessThanInt32Op*	pLessThanInt32Op = static_cast<const SVMLessThanInt32Op*>(pOp);
							const bool								result = stack[pLessThanInt32Op->lhsPos].AsInt32() < stack[pLessThanInt32Op->rhsPos].AsInt32();
							stack.Push(result);
							pos += pOp->size;
							break;
						}
					case SVMOp::GREATER_THAN_INT32:
						{
							// #SchematycTODO : Probably worth adding some error checking here.
							const SVMGreaterThanInt32Op*	pGreaterThanInt32Op = static_cast<const SVMGreaterThanInt32Op*>(pOp);
							const bool										result = stack[pGreaterThanInt32Op->lhsPos].AsInt32() > stack[pGreaterThanInt32Op->rhsPos].AsInt32();
							stack.Push(result);
							pos += pOp->size;
							break;
						}
					case SVMOp::BRANCH:
						{
							const SVMBranchOp*	pBranchOp = static_cast<const SVMBranchOp*>(pOp);
							pos = pBranchOp->pos;
							break;
						}
					case SVMOp::BRANCH_IF_ZERO:
						{
							const SVMBranchIfZeroOp*	pBranchIfZeroOp = static_cast<const SVMBranchIfZeroOp*>(pOp);
							if(stack.Peek().AsBool() == false)
							{
								pos = pBranchIfZeroOp->pos;
							}
							else
							{
								pos += pOp->size;
							}
							break;
						}
					case SVMOp::BRANCH_IF_NOT_ZERO:
						{
							const SVMBranchIfNotZeroOp*	pBranchIfNotZeroOp = static_cast<const SVMBranchIfNotZeroOp*>(pOp);
							if(stack.Peek().AsBool() == true)
							{
								pos = pBranchIfNotZeroOp->pos;
							}
							else
							{
								pos += pOp->size;
							}
							break;
						}
					case SVMOp::GET_OBJECT:
						{
							stack.Push(m_objectId.GetValue());
							pos += pOp->size;
							break;
						}
					case SVMOp::START_TIMER:
						{
							const SVMStartTimerOp*	pStartTimerOp = static_cast<const SVMStartTimerOp*>(pOp);
							StartTimer(pStartTimerOp->guid);
							pos += pOp->size;
							break;
						}
					case SVMOp::STOP_TIMER:
						{
							const SVMStopTimerOp*	pStopTimerOp = static_cast<const SVMStopTimerOp*>(pOp);
							StopTimer(pStopTimerOp->guid);
							pos += pOp->size;
							break;
						}
					case SVMOp::RESET_TIMER:
						{
							const SVMResetTimerOp*	pResetTimerOp = static_cast<const SVMResetTimerOp*>(pOp);
							ResetTimer(pResetTimerOp->guid);
							pos += pOp->size;
							break;
						}
					case SVMOp::SEND_SIGNAL:
						{
							const SVMSendSignalOp*	pSendSignalOp = static_cast<const SVMSendSignalOp*>(pOp);
							ISignalConstPtr					pSignal = envRegistry.GetSignal(pSendSignalOp->guid);
							if(pSignal == NULL)
							{
								pSignal = libRegistry.GetSignal(pSendSignalOp->guid);
							}
							CRY_ASSERT(pSignal != NULL);
							if(pSignal != NULL)
							{
								const size_t	stackSize = stack.GetSize();
								const size_t	signalInputSize = pSignal->GetVariantInputs().size();
								CRY_ASSERT(stackSize >= (signalInputSize + 1));
								if(stackSize >= (signalInputSize + 1))
								{
									const ObjectId			objectId(stack[stackSize - signalInputSize - 1].AsUInt32());
									TVariantConstArray	signalInputs(signalInputSize ? &stack[stackSize - signalInputSize] : NULL, signalInputSize);
									if(objectId == m_objectId)
									{
										ProcessSignal(pSendSignalOp->guid, signalInputs);
									}
									else
									{
										gEnv->pSchematyc2->GetObjectManager().SendSignal(objectId, pSendSignalOp->guid, signalInputs);
									}
								}
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::BROADCAST_SIGNAL:
						{
							const SVMBroadcastSignalOp*	pBroadcastSignalOp = static_cast<const SVMBroadcastSignalOp*>(pOp);
							ISignalConstPtr							pSignal = envRegistry.GetSignal(pBroadcastSignalOp->guid);
							if(pSignal == NULL)
							{
								pSignal = libRegistry.GetSignal(pBroadcastSignalOp->guid);
							}
							CRY_ASSERT(pSignal != NULL);
							if(pSignal != NULL)
							{
								const size_t	stackSize = stack.GetSize();
								const size_t	signalInputSize = pSignal->GetVariantInputs().size();
								CRY_ASSERT(stackSize >= signalInputSize);
								if(stackSize >= signalInputSize)
								{
									TVariantConstArray	signalInputs(signalInputSize ? &stack[stackSize - signalInputSize] : NULL, signalInputSize);
									gEnv->pSchematyc2->GetObjectManager().BroadcastSignal(pBroadcastSignalOp->guid, signalInputs);
								}
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::CALL_GLOBAL_FUNCTION:
						{
							// #SchematycTODO : Can we use an additional op or op parameter to specify how much stack is required?
							const SVMCallGlobalFunctionOp*	pCallGlobalFunctionOp = static_cast<const SVMCallGlobalFunctionOp*>(pOp);
							IGlobalFunctionConstPtr					pGlobalFunction = globalFunctionTable[pCallGlobalFunctionOp->iGlobalFunction];
							size_t													stackSize = stack.GetSize();
							const size_t										functionInputSize = pGlobalFunction->GetVariantInputs().size();
							CRY_ASSERT(stackSize >= functionInputSize);
							if(stackSize >= functionInputSize)
							{
								const size_t	functionOutputSize = pGlobalFunction->GetVariantOutputs().size();
								stackSize += functionOutputSize;
								stack.Resize(stackSize);
								TVariantConstArray	functionInputs(functionInputSize ? &stack[stackSize - functionInputSize - functionOutputSize] : NULL, functionInputSize);
								TVariantArray				functionOutputs(functionOutputSize ? &stack[stackSize - functionOutputSize] : NULL, functionOutputSize);
								pGlobalFunction->Call(functionInputs, functionOutputs);
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::CALL_ENV_ABSTRACT_INTERFACE_FUNCTION:
						{
							const SVMCallEnvAbstractInterfaceFunctionOp*	pCallEnvAbstractInterfaceFunctionOp = static_cast<const SVMCallEnvAbstractInterfaceFunctionOp*>(pOp);
							IAbstractInterfaceFunctionConstPtr						pAbstractInterfaceFunction = gEnv->pSchematyc2->GetEnvRegistry().GetAbstractInterfaceFunction(pCallEnvAbstractInterfaceFunctionOp->functionGUID);
							CRY_ASSERT(pAbstractInterfaceFunction);
							if(pAbstractInterfaceFunction)
							{
								size_t				prevStackSize = stack.GetSize();
								const size_t	variantInputSize = pAbstractInterfaceFunction->GetVariantInputs().size();
								CRY_ASSERT(prevStackSize >= (variantInputSize + 1));
								if(prevStackSize >= (variantInputSize + 1))
								{
									const ObjectId						objectId(stack[prevStackSize - variantInputSize - 1].AsUInt32());
									const TVariantConstArray	variantOutputs = pAbstractInterfaceFunction->GetVariantOutputs();
									const size_t							variantOutputSize = pAbstractInterfaceFunction->GetVariantOutputs().size();
									for(size_t iVariantOutput = 0; iVariantOutput < variantOutputSize; ++ iVariantOutput)
									{
										stack.Push(variantOutputs[iVariantOutput]);
									}
									if(IObject* pObject = objectId == m_objectId ? this : gEnv->pSchematyc2->GetObjectManager().GetObjectById(objectId))
									{
										TVariantConstArray	functionInputs(variantInputSize ? &stack[prevStackSize - variantInputSize] : NULL, variantInputSize);
										TVariantArray				functionOutputs(variantOutputSize ? &stack[prevStackSize] : NULL, variantOutputSize);
										const bool					result = pObject->CallAbstractInterfaceFunction(pCallEnvAbstractInterfaceFunctionOp->abstractInterfaceGUID, pCallEnvAbstractInterfaceFunctionOp->functionGUID, functionInputs, functionOutputs);
										stack.Push(CVariant(result));
									}
									else
									{
										stack.Push(CVariant(bool(false)));
									}
								}
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::CALL_LIB_ABSTRACT_INTERFACE_FUNCTION:
						{
							const SVMCallLibAbstractInterfaceFunctionOp*	pCallLibAbstractInterfaceFunctionOp = static_cast<const SVMCallLibAbstractInterfaceFunctionOp*>(pOp);
							ILibAbstractInterfaceFunctionConstPtr					pAbstractInterfaceFunction = gEnv->pSchematyc2->GetLibRegistry().GetAbstractInterfaceFunction(pCallLibAbstractInterfaceFunctionOp->functionGUID);
							CRY_ASSERT(pAbstractInterfaceFunction);
							if(pAbstractInterfaceFunction)
							{
								size_t				prevStackSize = stack.GetSize();
								const size_t	variantInputSize = pAbstractInterfaceFunction->GetVariantInputs().size();
								CRY_ASSERT(prevStackSize >= (variantInputSize + 1));
								if(prevStackSize >= (variantInputSize + 1))
								{
									const ObjectId						objectId(stack[prevStackSize - variantInputSize - 1].AsUInt32());
									const TVariantConstArray	variantOutputs = pAbstractInterfaceFunction->GetVariantOutputs();
									const size_t							variantOutputSize = pAbstractInterfaceFunction->GetVariantOutputs().size();
									for(size_t iVariantOutput = 0; iVariantOutput < variantOutputSize; ++ iVariantOutput)
									{
										stack.Push(variantOutputs[iVariantOutput]);
									}
									if(IObject* pObject = objectId == m_objectId ? this : gEnv->pSchematyc2->GetObjectManager().GetObjectById(objectId))
									{
										TVariantConstArray	functionInputs(variantInputSize ? &stack[prevStackSize - variantInputSize] : NULL, variantInputSize);
										TVariantArray				functionOutputs(variantOutputSize ? &stack[prevStackSize] : NULL, variantOutputSize);
										const bool					result = pObject->CallAbstractInterfaceFunction(pCallLibAbstractInterfaceFunctionOp->abstractInterfaceGUID, pCallLibAbstractInterfaceFunctionOp->functionGUID, functionInputs, functionOutputs);
										stack.Push(CVariant(result));
									}
									else
									{
										stack.Push(CVariant(bool(false)));
									}
								}
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::CALL_COMPONENT_MEMBER_FUNCTION:
						{
							// #SchematycTODO : Can we use an additional op or op parameter to specify how much stack is required?
							const SVMCallComponentMemberFunctionOp*	pCallComponentMemberFunctionOp = static_cast<const SVMCallComponentMemberFunctionOp*>(pOp);
							IComponentMemberFunctionConstPtr				pComponentMemberFunction = componentMemberFunctionTable[pCallComponentMemberFunctionOp->iComponentMemberFunction];
							size_t																	stackSize = stack.GetSize();
							const size_t														functionInputSize = pComponentMemberFunction->GetVariantInputs().size();
							CRY_ASSERT(stackSize >= functionInputSize);
							if(stackSize > functionInputSize)
							{
								const uint32	iComponentInstance = stack[stackSize - functionInputSize - 1].AsUInt32();
								CRY_ASSERT(iComponentInstance < m_componentInstances.size());
								if(iComponentInstance < m_componentInstances.size())
								{
									const size_t	functionOutputSize = pComponentMemberFunction->GetVariantOutputs().size();
									stackSize += functionOutputSize;
									stack.Resize(stackSize);
									TVariantConstArray	functionInputs(functionInputSize ? &stack[stackSize - functionInputSize - functionOutputSize] : NULL, functionInputSize);
									TVariantArray				functionOutputs(functionOutputSize ? &stack[stackSize - functionOutputSize] : NULL, functionOutputSize);
									pComponentMemberFunction->Call(*m_componentInstances[iComponentInstance].pComponent, functionInputs, functionOutputs);
								}
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::CALL_ACTION_MEMBER_FUNCTION:
						{
							// #SchematycTODO : Can we use an additional op or op parameter to specify how much stack is required?
							const SVMCallActionMemberFunctionOp*	pCallActionMemberFunctionOp = static_cast<const SVMCallActionMemberFunctionOp*>(pOp);
							IActionMemberFunctionConstPtr					pActionMemberFunction = actionMemberFunctionTable[pCallActionMemberFunctionOp->iActionMemberFunction];
							size_t																stackSize = stack.GetSize();
							const size_t													functionInputSize = pActionMemberFunction->GetVariantInputs().size();
							CRY_ASSERT(stackSize >= functionInputSize);
							if(stackSize > functionInputSize)
							{
								const uint32	iActionInstance = stack[stackSize - functionInputSize - 1].AsUInt32();
								CRY_ASSERT(iActionInstance < m_actionInstances.size());	// #SchematycTODO : Double check action in an env action!!!
								if(iActionInstance < m_actionInstances.size())
								{
									const size_t	functionOutputSize = pActionMemberFunction->GetVariantOutputs().size();
									stackSize += functionOutputSize;
									stack.Resize(stackSize);
									TVariantConstArray	functionInputs(functionInputSize ? &stack[stackSize - functionInputSize - functionOutputSize] : NULL, functionInputSize);
									TVariantArray				functionOutputs(functionOutputSize ? &stack[stackSize - functionOutputSize] : NULL, functionOutputSize);
									pActionMemberFunction->Call(*m_actionInstances[iActionInstance].pAction, functionInputs, functionOutputs);
								}
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::CALL_LIB_FUNCTION:
						{
							const SVMCallLibFunctionOp*	pCallLibFunctionOp = static_cast<const SVMCallLibFunctionOp*>(pOp);
							const ILibFunction*					pLibFunction = m_pLibClass->GetFunction(pCallLibFunctionOp->functionId);
							CRY_ASSERT(pLibFunction != NULL);
							if(pLibFunction != NULL)
							{
								size_t				stackSize = stack.GetSize();
								const size_t	functionInputSize = pLibFunction->GetVariantInputs().size();
								CRY_ASSERT(stackSize >= functionInputSize);
								if(stackSize >= functionInputSize)
								{
									const size_t	functionOutputSize = pLibFunction->GetVariantOutputs().size();
									stackSize += functionOutputSize;
									stack.Resize(stackSize);
									TVariantConstArray	functionInputs(functionInputSize? &stack[stackSize - functionInputSize - functionOutputSize] : NULL, functionInputSize);
									TVariantArray				functionOutputs(functionOutputSize ? &stack[stackSize - functionOutputSize] : NULL, functionOutputSize);
									ProcessFunction(pCallLibFunctionOp->functionId, functionInputs, functionOutputs);
								}
							}
							pos += pOp->size;
							break;
						}
					case SVMOp::RETURN:
						{
							pos = size;
							break;
						}
					default:
						{
							CRY_ASSERT(false);
							pos += pOp->size;
							break;
						}
					}
				}
				// Copy outputs from stack.
				for(size_t iOutput = 0; iOutput < outputCount; ++ iOutput)
				{
					outputs[iOutput] = stack[iOutput];
				}
			}
			if(CVars::sc_FunctionTimeLimit)
			{
				const int64	endTicks = CryGetTicks();
				const float	time = gEnv->pTimer->TicksToSeconds(endTicks - startTicks);
				if(time > CVars::sc_FunctionTimeLimit)
				{
					SCHEMATYC2_SYSTEM_ERROR("Function took more than %f(s) to process: class = %s, function = %s, time = %f(s)", CVars::sc_FunctionTimeLimit, m_pLibClass->GetName(), pFunction->GetName(), time);
				}
			}
			-- s_recursionDepth;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::NetworkSerialize(TSerialize serialize, int32 aspects, uint8 profile, int flags)
	{
		if((m_flags & EObjectFlags::NetworkReplicateActions) != 0)
		{
			const bool isReading = serialize.IsReading();

			for(size_t stateMachineIdx = 0, stateMachineCount = m_stateMachines.size(); stateMachineIdx < stateMachineCount; ++ stateMachineIdx)
			{
				SStateMachine& stateMachine = m_stateMachines[stateMachineIdx];

				const CStateNetIdxMapper::StateNetIdx invalidStateIdx = CStateNetIdxMapper::INVALID_NET_IDX;
				const CStateNetIdxMapper::StateNetIdx statesCount = m_stateNetIdxMapper.GetStateCount(stateMachineIdx);
				if (statesCount == 0)
				{
					continue;
				}

				CStateNetIdxMapper::StateNetIdx serializedStateIdx = CStateNetIdxMapper::INVALID_NET_IDX;
				if (isReading == false)
				{
					serializedStateIdx = m_stateNetIdxMapper.Encode(stateMachine.iCurrentState);
				}

				serialize.EnumValue(m_pLibClass->GetStateMachine(stateMachineIdx)->GetName(), serializedStateIdx, invalidStateIdx, statesCount);

				if(isReading)
				{
					stateMachine.iRequestedState = m_stateNetIdxMapper.Decode(stateMachineIdx, serializedStateIdx);

					if((m_simulationMode == ESimulationMode::Game) && (stateMachine.iRequestedState != stateMachine.iCurrentState))
					{
						ChangeState(stateMachineIdx, stateMachine.iRequestedState, SEvent(), TVariantConstArray());
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObject::CStateNetIdxMapper::Init(const ILibClass* pLibClass, const TActionInstanceVector& actionInstances)
	{
		// TODO pavloi 2017.11.03: whole Encode/Decode might be much simpler and doesn't have to store index mapping,
		// if we could assure, that states are sorted by their state machines. 
		// and that each state machine stores its firstStateIndex and statesCount. Then:
		//  netStateIndex = (iState - firstStateIndex);
		//  iState = (stateNetIdx + firstStateIndex);
		// This way, mapper will have to store, if state machines are net-relevant. Or it may become a flag on ILibStateMachine.
		// Discuss with Dominique, if we can enforce ILibState sorting and store firstStateIndex and statesCount in ILibStateMachine.
		// It will require some additional changes in CCompiler::CompileClass for state sorting (or maybe we should enforce 
		// sorting on class saving and on loading we just validate?).

		const size_t stateMachineCount = pLibClass->GetStateMachineCount();
		const size_t stateCount = pLibClass->GetStateCount();

		std::vector<bool> stateMachineRelevant(stateMachineCount, false);

		TStateIndexVector stateMachineCounters(stateMachineCount, 0);
		TStateIndexVector stateIndexMapping(stateCount, INVALID_NET_IDX);
		TReverseStateIndexMap::container_type reverseStateIndexMapping;
		reverseStateIndexMapping.reserve(stateCount);

		for (size_t iState = 0; iState < stateCount; ++iState)
		{
			const ILibState* pState = pLibClass->GetState(iState);
			const size_t iStateMachine = pState->GetStateMachine();

			CRY_ASSERT(iStateMachine < stateMachineCount);

			const StateNetIdx idx = stateMachineCounters[iStateMachine]++;
			stateIndexMapping[iState] = idx;

			reverseStateIndexMapping.push_back(std::make_pair(std::make_pair(iStateMachine, idx), static_cast<int32>(iState)));

			if (stateMachineRelevant[iStateMachine] == false)
			{
				const size_t actionsCount = pState->GetActionInstanceCount();
				for (size_t stateActionIdx = 0; stateActionIdx < actionsCount; ++stateActionIdx)
				{
					const size_t iAction = pState->GetActionInstance(stateActionIdx);
					CRY_ASSERT(iAction < actionInstances.size());
					if ((actionInstances[iAction].flags & (EActionFlags::NetworkReplicateServer | EActionFlags::NetworkReplicateClients)) != 0)
					{
						stateMachineRelevant[iStateMachine] = true;
						break;
					}
				}
			}
		}

		for (size_t iStateMachine = 0; iStateMachine < stateMachineCount; ++iStateMachine)
		{
			if (stateMachineRelevant[iStateMachine] == false)
			{
				stateMachineCounters[iStateMachine] = 0;
			}
		}

		m_stateIndexMapping = std::move(stateIndexMapping);
		m_reverseStateIndexMapping.SwapElementsWithVector(reverseStateIndexMapping);
		m_stateMachineCounts = std::move(stateMachineCounters);
	}

	//////////////////////////////////////////////////////////////////////////
	int32 CObject::CStateNetIdxMapper::GetStateCount(size_t iStateMachine) const
	{
		return m_stateMachineCounts[iStateMachine];
	}

	//////////////////////////////////////////////////////////////////////////
	int32 CObject::CStateNetIdxMapper::Encode(size_t iState) const
	{
		if (iState == INVALID_INDEX)
		{
			return INVALID_NET_IDX;
		}

		CRY_ASSERT(iState < m_stateIndexMapping.size());

		return m_stateIndexMapping[iState];
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CObject::CStateNetIdxMapper::Decode(size_t iStateMachine, StateNetIdx netStateIdx) const
	{
		if (netStateIdx == INVALID_NET_IDX)
		{
			return INVALID_INDEX;
		}

		CRY_ASSERT(iStateMachine < m_stateMachineCounts.size());

		auto iter = m_reverseStateIndexMapping.find(std::make_pair(iStateMachine, netStateIdx));
		if (iter != m_reverseStateIndexMapping.end())
		{
			return iter->second;
		}
		return INVALID_INDEX;
	}
}
