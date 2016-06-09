// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   FlowNodeAIAction.cpp
   $Id$
   Description: place for AI action related flow graph nodes

   -------------------------------------------------------------------------
   History:
   - 15:6:2005   15:24 : Created by Kirill Bulatsev

 *********************************************************************/

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIActorProxy.h>
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "FlowBaseNode.h"
#include "FlowNodeAIAction.h"
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IGoalPipe.h>
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleSeat.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

#define DEF_CLONE(CLASS) IFlowNodePtr CLASS::Clone(SActivationInfo * pActInfo){ CLASS* p = new CLASS(pActInfo); p->m_bNeedsExec = m_bNeedsExec; p->m_bNeedsSink = m_bNeedsSink; return p; }

static const char* RUN_SPEED_UICONFIG = "enum_int:VerySlow=0,Walk=1,Run=2,Sprint=3";
static const char* STANCE_UICONFIG = "enum_int:Prone=0,Crouch=1,Combat=2,CombatAlerted=3,Relaxed=4,Stealth=5";
static const char* FORCE_UICONFIG = "enum_int:No=0,KeepPerception=1,IgnoreAll=2";
static const char* SECONDARY_UICONFIG = "enum_int:No=0,Any=1,Left=2,Right=3";

//////////////////////////////////////////////////////////////////////////
// base AI Flow node
//////////////////////////////////////////////////////////////////////////
//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> CFlowNode_AIBase<TBlocking>::CFlowNode_AIBase(SActivationInfo* pActInfo)
	: m_bExecuted(false)
	, m_bSynchronized(false)
	, m_bNeedsSink(false)
	, m_bNeedsExec(false)
	, m_bNeedsReset(true)
	, m_EntityId(0)
	, m_UnregisterEntityId(0)
	, m_GoalPipeId(0)
	, m_UnregisterGoalPipeId(0)
{
	//		m_entityId = (EntityId) pActInfo->m_pUserData;
	m_nodeID = pActInfo->myID;
	m_pGraph = pActInfo->pGraph;
}
//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> CFlowNode_AIBase<TBlocking>::~CFlowNode_AIBase()
{
	UnregisterEvents();
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::Serialize(SActivationInfo*, TSerialize ser)
{
	if (ser.IsReading())
		UnregisterEvents();

	ser.Value("GoalPipeId", m_GoalPipeId);
	ser.Value("UnregisterGoalPipeId", m_UnregisterGoalPipeId);
	ser.Value("EntityId", m_EntityId);
	ser.Value("UnregisterEntityId", m_UnregisterEntityId);
	ser.Value("bExecuted", m_bExecuted);
	ser.Value("bSynchronized", m_bSynchronized);
	ser.Value("bNeedsExec", m_bNeedsExec);
	ser.Value("bNeedsSink", m_bNeedsSink);
	ser.Value("bNeedsReset", m_bNeedsReset);

	if (ser.IsReading() && /*m_bBlocking &&*/ m_EntityId != 0)
	{
		RegisterEntityEvents(); // We'll register for AI events in PostSerialize, once AI is created.
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_ConnectInputPort)
	{
		if (pActInfo->connectPort == 0)
			m_bNeedsExec = true;
		else if (pActInfo->connectPort == 1)
			m_bNeedsSink = true;
	}
	else if (event == eFE_DisconnectInputPort)
	{
		if (pActInfo->connectPort == 0)
			m_bNeedsExec = false;
		else if (pActInfo->connectPort == 1)
			m_bNeedsSink = false;
	}
	else if (event == eFE_Initialize)
	{
		if (m_bNeedsReset)
		{
			// it may happen that updates were enabled in previous game session
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

			UnregisterEvents();
			m_EntityId = 0;
			m_GoalPipeId = 0;
			m_bExecuted = false;
			m_bSynchronized = false;
			//		m_bNeedsExec = pActInfo->pEntity == 0;
			//		m_bNeedsSink = false;
			m_bNeedsReset = false;
		}
		if (m_bNeedsExec && pActInfo->pEntity && IsPortActive(pActInfo, -1))
		{
			// entity is dynamically assigned during initialization
			m_bNeedsExec = false;
		}
	}
	else if (event == eFE_SetEntityId && pActInfo->pGraph->GetAIAction() != NULL)
	{
		m_bNeedsReset = true;
		if (m_bNeedsExec && !m_bExecuted && pActInfo->pEntity)
		{
			m_bNeedsReset = true;
			m_bExecuted = true;
			if (m_bSynchronized || !m_bNeedsSink)
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}
	}
	else if (event == eFE_SetEntityId && pActInfo->pGraph->GetAIAction() == NULL)
	{
		m_bNeedsReset = true;
		if (m_bNeedsExec && !m_bExecuted && pActInfo->pEntity)
		{
			m_bNeedsReset = true;
			m_bExecuted = true;
			if (m_bSynchronized /*|| !m_bNeedsSink*/)
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}
	}
	else if (event == eFE_Activate)
	{
		/*
		    if ( IsPortActive(pActInfo, -1) )
		    {
		      m_bNeedsReset = true;
		      if ( m_bNeedsExec && !m_bExecuted && pActInfo->pEntity )
		      {
		        m_bNeedsReset = true;
		        m_bExecuted = true;
		        if ( m_bSynchronized || !m_bNeedsSink )
		          pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
		      }
		    }
		 */
		if (IsPortActive(pActInfo, 0))
		{
			m_bNeedsReset = true;
			if (m_bNeedsSink && !m_bSynchronized)
			{
				m_bSynchronized = true;
				if (m_bExecuted || !m_bNeedsExec)
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				PreExecute(pActInfo);
			}
		}
		if (TBlocking && m_EntityId && m_GoalPipeId && IsPortActive(pActInfo, 1))
		{
			IEntity* pEntity = GetEntity(pActInfo);
			if (pEntity)
			{
				IAIObject* pAI = pEntity->GetAI();
				if (pAI)
				{
					IPipeUser* pPipeUser = pAI->CastToIPipeUser();
					if (pPipeUser)
						OnCancelPortActivated(pPipeUser, pActInfo);
				}
			}
		}
	}
	else if (event == eFE_Update)
	{
		m_bNeedsReset = true;
		IEntity* pEntity = GetEntity(pActInfo);
		if (pEntity)
		{
			// first check is ai object updated at least once
			IAIObject* pAI = pEntity->GetAI();
			if (!pAI || !pAI->IsAgent() || pAI->IsUpdatedOnce())
			{
				if (!OnUpdate(pActInfo))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					TryExecute(pAI, event, pActInfo);

					m_bExecuted = false;
					m_bSynchronized = false;
				}
			}
			else
				m_EntityId = 0;
		}
		else
		{
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

			m_bExecuted = false;
			m_bSynchronized = false;
		}
	}
	else if (event == eFE_Resume && m_EntityId)
	{
		OnResume(pActInfo);
		//	UnregisterEvents();
		//	pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
	}
}

template<bool TBlocking>
void CFlowNode_AIBase<TBlocking >::TryExecute(IAIObject* pAI, EFlowEvent event, SActivationInfo* pActInfo)
{
	// marcok: we use the CVar to control the check for alertness
	static ICVar* aiFlowNodeAlertnessCheck = gEnv->pConsole->GetCVar("ai_FlowNodeAlertnessCheck");

	// check is the node inside an entity flow graph and alerted. if so fail execution
	if (aiFlowNodeAlertnessCheck->GetIVal() && pAI && pAI->GetProxy() && pAI->GetProxy()->GetAlertnessState() && !pActInfo->pGraph->GetAIAction())
	{
		IEntity* graphOwner = gEnv->pEntitySystem->GetEntity(pActInfo->pGraph->GetGraphEntity(0));
		AILogEventID("<Flowgraph> ", "Canceling execution of an AI flowgraph node because the agent '%s' alertness is too high (%d). Owner:%s Node:%d",
		             pAI->GetName(), pAI->GetProxy()->GetAlertnessState(), graphOwner ? graphOwner->GetName() : "<unknown>", pActInfo->myID);

		Cancel();
	}
	else
		DoProcessEvent(event, pActInfo);
}

template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::OnCancelPortActivated(IPipeUser* pPipeUser, SActivationInfo* pActInfo)
{
	if (m_GoalPipeId != -1)
	{
		// We have a goal pipe for this flow node. Try to cancel it.
		if (pPipeUser->CancelSubPipe(m_GoalPipeId))
		{
			// We successfully canceled the sub pipe. This mean a deselect
			// event will be triggered and that will by itself call Cancel.
			return;
		}
		else
		{
			// We could not cancel the goal pipe. This might mean that
			// the pipe is not yet running, or it has already been canceled.
			// Fall through and call Cancel from here then.
		}
	}

	Cancel();
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::OnGoalPipeEvent(IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent)
{
	if (m_GoalPipeId != goalPipeId)
		return;

	switch (event)
	{
	case ePN_Deselected:
		Cancel();
		break;

	case ePN_Removed:
	case ePN_Finished:
		Finish();
		break;

	case ePN_Suspended:
		break;

	case ePN_Resumed:
	case ePN_RefPointMoved:
		OnResume();
		break;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::OnEntityEvent(IEntity* pEntity, SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_AI_DONE:
		if (m_pGraph->IsSuspended())
			return;
		Finish();
		break;

	case ENTITY_EVENT_RESET:
	case ENTITY_EVENT_DONE:
		Cancel();
		break;

	case ENTITY_EVENT_POST_SERIALIZE:
		// AI are created after flowgraph is serialized, so RegisterEvents() needs to happen
		// on post serialize to make sure we register as goalpipe listeners.
		RegisterAIEvents();
		break;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::Cancel()
{
	OnCancel();

	SFlowAddress done(m_nodeID, 0, true);
	SFlowAddress fail(m_nodeID, 2, true);
	m_pGraph->ActivatePort(done, m_EntityId);
	m_pGraph->ActivatePort(fail, m_EntityId);

	if (m_EntityId)
	{
		m_bExecuted = false;
		m_bSynchronized = false;

		m_UnregisterEntityId = m_EntityId;
		m_EntityId = 0;

		m_UnregisterGoalPipeId = m_GoalPipeId;
		m_GoalPipeId = 0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::Finish()
{
	OnFinish();

	SFlowAddress done(m_nodeID, 0, true);
	SFlowAddress succeed(m_nodeID, 1, true);
	m_pGraph->ActivatePort(done, m_EntityId);
	m_pGraph->ActivatePort(succeed, m_EntityId);

	if (m_EntityId)
	{
		m_bExecuted = false;
		m_bSynchronized = false;

		m_UnregisterEntityId = m_EntityId;
		m_EntityId = 0;

		m_UnregisterGoalPipeId = m_GoalPipeId;
		m_GoalPipeId = 0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::RegisterEvents()
{
	RegisterEntityEvents();
	RegisterAIEvents();
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::RegisterEntityEvents()
{
	if (m_EntityId)
	{
		IEntitySystem* pSystem = gEnv->pEntitySystem;
		pSystem->AddEntityEventListener(m_EntityId, ENTITY_EVENT_AI_DONE, this);
		pSystem->AddEntityEventListener(m_EntityId, ENTITY_EVENT_POST_SERIALIZE, this);
		//	pSystem->AddEntityEventListener( m_EntityId, ENTITY_EVENT_DONE, this );
		//	pSystem->AddEntityEventListener( m_EntityId, ENTITY_EVENT_RESET, this );
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::RegisterAIEvents()
{
	if (m_EntityId)
	{
		if (m_GoalPipeId > 0)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
			if (pEntity)
			{
				IAIObject* pAIObject = pEntity->GetAI();
				if (pAIObject)
				{
					IPipeUser* pPipeUser = pAIObject->CastToIPipeUser();
					if (pPipeUser)
					{
						pPipeUser->RegisterGoalPipeListener(this, m_GoalPipeId, "CFlowNode_AIBase::RegisterEvents");
					}
				}
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::UnregisterEvents()
{
	if (m_UnregisterEntityId && !m_EntityId)
		m_EntityId = m_UnregisterEntityId;

	if (m_EntityId)
	{
		IEntitySystem* pSystem = gEnv->pEntitySystem;
		if (pSystem)
		{
			pSystem->RemoveEntityEventListener(m_EntityId, ENTITY_EVENT_AI_DONE, this);
			pSystem->RemoveEntityEventListener(m_EntityId, ENTITY_EVENT_POST_SERIALIZE, this);
		}

		if (m_UnregisterGoalPipeId && !m_GoalPipeId)
			m_GoalPipeId = m_UnregisterGoalPipeId;
		if (pSystem && m_GoalPipeId > 0)
		{
			IEntity* pEntity = pSystem->GetEntity(m_EntityId);
			if (pEntity)
			{
				IAIObject* pAIObject = pEntity->GetAI();
				if (pAIObject)
				{
					IPipeUser* pPipeUser = pAIObject->CastToIPipeUser();
					if (pPipeUser)
					{
						pPipeUser->UnRegisterGoalPipeListener(this, m_GoalPipeId);
					}
				}
			}
		}
		if (m_UnregisterGoalPipeId && m_GoalPipeId == m_UnregisterGoalPipeId)
			m_UnregisterGoalPipeId = 0;
		m_GoalPipeId = 0;

		//	pSystem->RemoveEntityEventListener( m_EntityId, ENTITY_EVENT_DONE, this );
		//	pSystem->RemoveEntityEventListener( m_EntityId, ENTITY_EVENT_RESET, this );
	}

	if (m_UnregisterEntityId && m_EntityId == m_UnregisterEntityId)
		m_UnregisterEntityId = 0;
	m_EntityId = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> IEntity* CFlowNode_AIBase<TBlocking >::GetEntity(SActivationInfo* pActInfo)
{
	m_EntityId = 0;
	if (pActInfo->pEntity)
		m_EntityId = pActInfo->pEntity->GetId();
	return pActInfo->pEntity;
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> bool CFlowNode_AIBase<TBlocking >::Execute(SActivationInfo* pActInfo, const char* pSignalText, IAISignalExtraData* pData, int senderId)
{
	UnregisterEvents();

	IEntity* pEntity = GetEntity(pActInfo);
	if (!pEntity)
	{
		gEnv->pAISystem->FreeSignalExtraData(pData);

		return false;
	}

	IEntity* pSender = pEntity;
	if (senderId)
		pSender = gEnv->pEntitySystem->GetEntity(senderId);

	bool result = false;
	if (pEntity->HasAI())
		result = ExecuteOnAI(pActInfo, pSignalText, pData, pEntity, pSender);

	if (!result)
		result = ExecuteOnEntity(pActInfo, pSignalText, pData, pEntity, pSender);

	if (!result)
	{
		gEnv->pAISystem->FreeSignalExtraData(pData);
		Cancel();
	}

	return result;
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> bool CFlowNode_AIBase<TBlocking >::ExecuteOnAI(SActivationInfo* pActInfo, const char* pSignalText,
                                                                        IAISignalExtraData* pData, IEntity* pEntity, IEntity* pSender)
{
	IAIObject* pAI = pEntity->GetAI();
	CRY_ASSERT(pAI);
	if (!pAI)
		return false;

	switch (pAI->GetAIType())
	{
	case AIOBJECT_VEHICLE:
		{
			// activate vehicle AI, unless it's destroyed
			IVehicle* pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
			if (!pVehicle || pVehicle->IsDestroyed())
				return false;
			pAI->Event(AIEVENT_DRIVER_IN, NULL);  // enabling vehicle's full update to process signals, even if there is no driver
		}
		break;

	case AIOBJECT_ACTOR:
		break;

	case AIOBJECT_PLAYER:
		{
			// execute on player
			m_EntityId = pEntity->GetId();
			ActivateOutput(pActInfo, 0, m_EntityId);
			ActivateOutput(pActInfo, 1, m_EntityId);
			m_bExecuted = false;
			m_bSynchronized = false;
			m_EntityId = 0;
			m_GoalPipeId = -1;

			IAIActor* pAIActor = pAI->CastToIAIActor();
			if (pAIActor)
				pAIActor->SetSignal(10, pSignalText, pSender, pData);   // 10 means this signal must be sent (but sent[!], not set)
			else
				gEnv->pAISystem->FreeSignalExtraData(pData);

			// even if the same signal is already present in the queue
			return true;
		}
	default:
		// invalid AIObject type
		return false;
	}

	m_EntityId = pEntity->GetId();
	m_GoalPipeId = gEnv->pAISystem->AllocGoalPipeId();

	if (m_bBlocking)
	{
		RegisterEvents();

		IAIActor* pAIActor = pAI->CastToIAIActor();
		if (pAIActor)
		{
			if (!pData)
				pData = gEnv->pAISystem->CreateSignalExtraData();
			pData->iValue = m_GoalPipeId;
			pAIActor->SetSignal(10, pSignalText, pSender, pData);   // 10 means this signal must be sent (but sent[!], not set)
		}
		else
			gEnv->pAISystem->FreeSignalExtraData(pData);
	}
	else
	{
		RegisterEvents();
		IAIActor* pAIActor = pAI->CastToIAIActor();
		if (pAIActor)
		{
			IAISignalExtraData* pExtraData = gEnv->pAISystem->CreateSignalExtraData();
			CRY_ASSERT(pExtraData);
			pExtraData->iValue = m_GoalPipeId;
			pAIActor->SetSignal(10, pSignalText, pSender, pData);   // 10 means this signal must be sent (but sent[!], not set)
			pAIActor->SetSignal(10, "ACT_DUMMY", pEntity, pExtraData);
		}
		else
			gEnv->pAISystem->FreeSignalExtraData(pData);
	}

	return true;
}

//
//-------------------------------------------------------------------------------------------------------------
template<bool TBlocking> bool CFlowNode_AIBase<TBlocking >::ExecuteOnEntity(SActivationInfo* pActInfo, const char* pSignalText,
                                                                            IAISignalExtraData* pData, IEntity* pEntity, IEntity* pSender)
{
	// sorry, not implemented :(
	//	CRY_ASSERT( 0 );
	// dont assert: it's perfectly normal for this to happen at runtime for AISpawners
	return false;
}

// must sort-of match COPRunCmd::COPRunCmd
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::SetSpeed(IAIObject* pAI, int iSpeed)
{
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return;

	SOBJECTSTATE& state = pAIActor->GetState();

	// There is code/configuration elsewhere that means we won't even move at all if we select
	// a "pseudo-speed" value that is too low (but still non-zero). So we're forced to hard-code these
	// numbers in the hope that stuff elsewhere won't change because the interface makes us use a floating point number
	// ("pseudoSpeed") to represent discrete states than a finite set of values.
	if (iSpeed == -4)
		state.fMovementUrgency = -AISPEED_SPRINT;
	if (iSpeed == -3)
		state.fMovementUrgency = -AISPEED_RUN;
	if (iSpeed == -2)
		state.fMovementUrgency = -AISPEED_WALK;
	if (iSpeed == -1)
		state.fMovementUrgency = -AISPEED_SLOW;
	if (iSpeed == 0)
		state.fMovementUrgency = AISPEED_SLOW;
	else if (iSpeed == 1)
		state.fMovementUrgency = AISPEED_WALK;
	else if (iSpeed == 2)
		state.fMovementUrgency = AISPEED_RUN;
	else if (iSpeed >= 3)
		state.fMovementUrgency = AISPEED_SPRINT;
}

template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::SetStance(IAIObject* pAI, int stance)
{
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return;
	SOBJECTSTATE& state = pAIActor->GetState();

	state.forceWeaponAlertness = false;

	switch (stance)
	{
	case 0:
		stance = STANCE_PRONE;
		break;
	case 1:
		stance = STANCE_CROUCH;
		break;
	case 2:
		stance = STANCE_STAND;
		break;
	case 3:
		state.forceWeaponAlertness = true;
		stance = STANCE_ALERTED;
		break;
	case 4:
		stance = STANCE_RELAXED;
		break;
	case 5:
		stance = STANCE_STEALTH;
		break;
	}

	state.bodystate = (EStance)stance;

	if (pAI->HasFormation())
	{
		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->iValue = stance;
		gEnv->pAISystem->SendSignal(SIGNALFILTER_FORMATION_EXCEPT, 1, "OnChangeStance", pAI, pData);
	}
}

template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::SetAllowStrafing(IAIObject* pAI, bool bAllowStrafing)
{
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return;

	if (bAllowStrafing)
		pPipeUser->SetAllowedStrafeDistances(999999.0f, 999999.0f, true);
	else
		pPipeUser->SetAllowedStrafeDistances(0, 0, false);
}

template<bool TBlocking> void CFlowNode_AIForceableBase<TBlocking >::ProcessEvent(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo)
{
	if (event == IFlowNode::eFE_Initialize)
	{
		if (this->m_bNeedsReset)
		{
			IEntity* pEntity = this->GetEntity(pActInfo);
			if (!pEntity)
				return;

			SetForceMethod(pEntity->GetAI(), eNoForce);
		}
	}

	TBase::ProcessEvent(event, pActInfo);
}

template<bool TBlocking> void CFlowNode_AIForceableBase<TBlocking >::TryExecute(IAIObject* pAI, IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo)
{
	if (GetForceMethod(pActInfo) == eNoForce)
		TBase::TryExecute(pAI, event, pActInfo);  // Keep the old behavior
	else
		this->DoProcessEvent(event, pActInfo);
}

template<bool TBlocking> void CFlowNode_AIForceableBase<TBlocking >::SetForceMethod(IAIObject* pAI, EForceMethod method)
{
	if (!pAI)
		return;

	if (m_LastForceMethod == method)
		return;

	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return;

	if (m_LastForceMethod == eIgnoreAll)
		pAIActor->EnablePerception(true);

	IAIActorProxy* pAIProxy = pAIActor->GetProxy();

	switch (method)
	{
	case eIgnoreAll:
		pAIActor->ResetPerception();
		pAIActor->EnablePerception(false);
	case eKeepPerception:
		if (pAIProxy)
			pAIProxy->SetForcedExecute(true);
		break;
	case eNoForce:
		if (pAIProxy)
			pAIProxy->SetForcedExecute(false);
		break;
	default:
		assert(0);
		break;
	}

	m_LastForceMethod = method;
}

template<bool TBlocking> void CFlowNode_AIForceableBase<TBlocking >::OnCancel()
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(this->m_EntityId);
	if (!pEntity)
		return;

	SetForceMethod(pEntity->GetAI(), eNoForce);
}

template<bool TBlocking> void CFlowNode_AIForceableBase<TBlocking >::OnFinish()
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(this->m_EntityId);
	if (!pEntity)
		return;

	SetForceMethod(pEntity->GetAI(), eNoForce);
}

template<bool TBlocking> void CFlowNode_AIForceableBase<TBlocking >::Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)
{
	TBase::Serialize(pActInfo, ser);
	int lastForceMethod = eNoForce;

	if (ser.IsWriting())
	{
		lastForceMethod = (int) m_LastForceMethod;
	}
	ser.Value("m_LastForceMethod", lastForceMethod);
	if (ser.IsReading())
	{
		m_LastForceMethod = (EForceMethod)lastForceMethod;
	}
}

//---------------------------------------------------------------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////
// generic AI order
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerivedFromSignalBase>
void CFlowNode_AISignalBase<TDerivedFromSignalBase >::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink", _HELP("for synchronization only"), _HELP("Sync")),
		{ 0 }
	};
	config.sDescription = _HELP("Generic AI signal.");
	config.pInputPorts = in_config;
	config.pOutputPorts = 0;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerivedFromSignalBase>
IFlowNodePtr CFlowNode_AISignalBase<TDerivedFromSignalBase >::Clone(SActivationInfo* pActInfo)
{
	//		pActInfo->m_pUserData = (void*) m_entityId;
	TDerivedFromSignalBase* p = new TDerivedFromSignalBase(pActInfo);
	p->m_bNeedsExec = m_bNeedsExec;
	p->m_bNeedsSink = m_bNeedsSink;
	return p;
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerivedFromSignalBase>
void CFlowNode_AISignalBase<TDerivedFromSignalBase >::DoProcessEvent(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo)
{
	IEntity* pEntity = this->GetEntity(pActInfo);
	if (!pEntity)
		return;
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return;
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return;
	unsigned short int nType = pAI->GetAIType();
	if ((nType != AIOBJECT_ACTOR) && (nType != AIOBJECT_VEHICLE))
		return; // invalid AIObject type

	// not needed for now:
	//if ( pAI->GetType() == AIOBJECT_VEHICLE )
	//	pAI->Event( AIEVENT_ENABLE, NULL );

	IAISignalExtraData* pExtraData = GetExtraData(pActInfo);
	pAIActor->SetSignal(10, m_SignalText, pEntity, pExtraData);

	// allow using the node more than once
	this->m_bExecuted = false;
	this->m_bSynchronized = false;
	this->m_EntityId = 0;
}

template<class TDerivedFromSignalBase>
void CFlowNode_AISignalBase<TDerivedFromSignalBase >::TryExecute(IAIObject* pAI, EFlowEvent event, SActivationInfo* pActInfo)
{
	if (ShouldForce(pActInfo))
		DoProcessEvent(event, pActInfo);
	else
		TBase::TryExecute(pAI, event, pActInfo);
}

template<class TDerivedFromSignalBase>
bool CFlowNode_AISignalBase<TDerivedFromSignalBase >::ShouldForce(IFlowNode::SActivationInfo* pActInfo) const
{
	return false;
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerivedFromSignalBase>
void CFlowNode_AISignalBase<TDerivedFromSignalBase >::Cancel()
{
	// Note: This function is similar as the AIBase Cancel, but
	// does not set the (Done/Succeed/Cancel) outputs since AISignalBase does not have them.
	this->OnCancel();

	if (this->m_EntityId)
	{
		this->m_bExecuted = false;
		this->m_bSynchronized = false;

		this->m_UnregisterEntityId = this->m_EntityId;
		this->m_EntityId = 0;

		this->m_UnregisterGoalPipeId = this->m_GoalPipeId;
		this->m_GoalPipeId = 0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerivedFromSignalBase>
void CFlowNode_AISignalBase<TDerivedFromSignalBase >::Finish()
{
	// Note: This function is similar as the AIBase Finish, but
	// does not set the (Done/Succeed/Cancel) outputs since AISignalBase does not have them.
	this->OnFinish();

	if (this->m_EntityId)
	{
		this->m_bExecuted = false;
		this->m_bSynchronized = false;

		this->m_UnregisterEntityId = this->m_EntityId;
		this->m_EntityId = 0;

		this->m_UnregisterGoalPipeId = this->m_GoalPipeId;
		this->m_GoalPipeId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
// prototyping AI orders
//////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////
// generic AI order
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISignal::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",       _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<string>("signal",  _HELP("order to execute")),
		InputPortConfig<Vec3>("posValue",  _HELP("pos signal data")),
		InputPortConfig<Vec3>("posValue2", _HELP("pos2 signal data")),
		InputPortConfig<int>("iValue",     _HELP("int signal data")),
		InputPortConfig<float>("fValue",   _HELP("float signal data")),
		InputPortConfig<string>("sValue",  _HELP("string signal data")),
		InputPortConfig<EntityId>("id",    _HELP("entity id signal data")),
		InputPortConfig<bool>("Force",     false,                             _HELP("send the signal even if alerted")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Generic AI action.");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

bool CFlowNode_AISignal::ShouldForce(IFlowNode::SActivationInfo* pActInfo) const
{
	return GetPortBool(pActInfo, 8);
}

DEF_CLONE(CFlowNode_AISignal)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISignal::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->point = GetPortVec3(pActInfo, 2);
	pData->point2 = GetPortVec3(pActInfo, 3);
	pData->iValue = GetPortInt(pActInfo, 4);
	pData->fValue = GetPortFloat(pActInfo, 5);
	pData->SetObjectName(GetPortString(pActInfo, 6));
	pData->nID = GetPortEntityId(pActInfo, 7);

	Execute(pActInfo, GetPortString(pActInfo, 1), pData);
}

void CFlowNode_AISignal::OnCancel()
{
	SFlowAddress done(m_nodeID, 0, true);
	SFlowAddress fail(m_nodeID, 2, true);

	m_pGraph->ActivatePort(done, m_EntityId);
	m_pGraph->ActivatePort(fail, m_EntityId);
}

void CFlowNode_AISignal::OnFinish()
{
	SFlowAddress done(m_nodeID, 0, true);
	SFlowAddress success(m_nodeID, 1, true);

	m_pGraph->ActivatePort(done, m_EntityId);
	m_pGraph->ActivatePort(success, m_EntityId);
}

//////////////////////////////////////////////////////////////////////////
// Executes an Action
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExecute::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",               _HELP("for synchronization only"),                                            _HELP("Sync")),
		InputPortConfig_Void("cancel",             _HELP("cancels execution"),                                                   _HELP("Cancel")),
		InputPortConfig<EntityId>("objectId",      _HELP("Entity ID of the object on which the agent should execute AI Action"), _HELP("ObjectId")),
		InputPortConfig<string>("soaction_action", _HELP("AI action to be executed"),                                            _HELP("Action")),
		InputPortConfig<int>("maxAlertness",       2,                                                                            _HELP("maximum alertness which allows execution (0, 1 or 2)"),                                      _HELP("MaxAlertness"),_UICONFIG("enum_int:Idle=0,Alerted=1,Combat=2")),
		InputPortConfig<bool>("HighPriority",      true,                                                                         _HELP("action priority - use to force the action to be finished (except if alertness get higher)")),
		InputPortConfig<int>("Force",              0,                                                                            _HELP("ForceExecution method"),                                                                     _HELP("Force"),  _UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Executes an AI Action");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIExecute)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIExecute::EForceMethod CFlowNode_AIExecute::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 6));
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExecute::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->SetObjectName(GetPortString(pActInfo, 3));
	pData->nID = GetPortEntityId(pActInfo, 2);
	pData->fValue = (float)GetPortInt(pActInfo, 4);

	if (GetPortBool(pActInfo, 5))
		pData->fValue += 100.0f;

	Execute(pActInfo, "ACT_EXECUTE", pData, GetPortEntityId(pActInfo, 2));
}

//
// should call DoProcessEvent if owner is not too much alerted
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExecute::TryExecute(IAIObject* pAI, EFlowEvent event, SActivationInfo* pActInfo)
{
	// check is the node inside an entity flow graph and alerted. if so fail execution
	if (pAI && pAI->GetProxy() && pAI->GetProxy()->GetAlertnessState() > GetPortInt(pActInfo, 4))
		Cancel();
	else
		DoProcessEvent(event, pActInfo);
}

//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExecute::OnCancelPortActivated(IPipeUser* pPipeUser, SActivationInfo* pActInfo)
{
	if (m_EntityId)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
		if (pEntity && m_GoalPipeId > 0)
		{
			m_bCancel = true;
			gEnv->pAISystem->GetAIActionManager()->AbortAIAction(pEntity, m_GoalPipeId);
		}
	}
}

void CFlowNode_AIExecute::Cancel()
{
	m_bCancel = false;
	TBase::Cancel();
}

void CFlowNode_AIExecute::Finish()
{
	if (m_bCancel)
		Cancel();
	else
		TBase::Finish();
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
//////////////////////////////////////////////////////////////////////////
// Set Character
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISetCharacter::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",                      _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<string>("aicharacter_character"),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Changes AI Character");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AISetCharacter)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISetCharacter::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	if (pEntity)
	{
		bool success = false;
		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
		{
			IPipeUser* pPipeUser = pAI->CastToIPipeUser();
			if (pPipeUser)
				success = pPipeUser->SetCharacter(GetPortString(pActInfo, 1));
		}
		ActivateOutput(pActInfo, 0, m_EntityId);
		ActivateOutput(pActInfo, success ? 1 : 2, m_EntityId);
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
// Set State
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISetState::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",              _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<string>("sostate_State"),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Changes Smart Object State");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_ADVANCED);
}

DEF_CLONE(CFlowNode_AISetState)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISetState::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	gEnv->pAISystem->GetSmartObjectManager()->SetSmartObjectState(GetEntity(pActInfo), GetPortString(pActInfo, 1));
	ActivateOutput(pActInfo, 0, m_EntityId);
	ActivateOutput(pActInfo, 1, m_EntityId);
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}

//////////////////////////////////////////////////////////////////////////
// Modify States
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIModifyStates::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",                _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<string>("sostates_states"),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Adds/removes smart object states");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIModifyStates)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIModifyStates::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	gEnv->pAISystem->GetSmartObjectManager()->ModifySmartObjectStates(GetEntity(pActInfo), GetPortString(pActInfo, 1));
	ActivateOutput(pActInfo, 0, m_EntityId);
	ActivateOutput(pActInfo, 1, m_EntityId);
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}

//////////////////////////////////////////////////////////////////////////
// Check States
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AICheckStates::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Sync",                 _HELP("for synchronization only")),
		InputPortConfig<string>("sopattern_Pattern", _HELP("states pattern to check")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("True",  _HELP("outputs the EntityId of the owner entity if its current smart object states are matching with the pattern")),
		OutputPortConfig<EntityId>("False", _HELP("outputs the EntityId of the owner entity if its current smart object states are not matching with the pattern")),
		{ 0 }
	};
	config.sDescription = _HELP("checks smart object states of the owner if they match the specified pattern");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_ADVANCED);
}

DEF_CLONE(CFlowNode_AICheckStates)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AICheckStates::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (gEnv->pAISystem->GetSmartObjectManager()->CheckSmartObjectStates(GetEntity(pActInfo), GetPortString(pActInfo, 1)))
		ActivateOutput(pActInfo, 0, m_EntityId);
	else
		ActivateOutput(pActInfo, 1, m_EntityId);
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}

//////////////////////////////////////////////////////////////////////////
// Follow Path
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPath::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",             _HELP("for synchronization only"),    _HELP("sync")),
		InputPortConfig_Void("Cancel",           _HELP("cancels execution")),
		InputPortConfig<bool>("PathFindToStart", _HELP("Path-find to start of path")),
		InputPortConfig<bool>("Reverse",         _HELP("Reverse the path direction")),
		InputPortConfig<bool>("StartNearest",    false,                                _HELP("Starts the path at nearest point on path")),
		InputPortConfig<int>("Loops",            0,                                    _HELP("Number of times to loop around the path (-1 = forever)")),
		InputPortConfig<string>("PathName",      _HELP("Name of the path to follow")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("Done",    _HELP("action done")),
		OutputPortConfig<EntityId>("Succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("Fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("follow path action");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_OBSOLETE);
}

DEF_CLONE(CFlowNode_AIFollowPath)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPath::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	bool pathFindToStart = GetPortBool(pActInfo, 2);
	bool reverse = GetPortBool(pActInfo, 3);
	bool nearest = GetPortBool(pActInfo, 4);
	int loops = GetPortInt(pActInfo, 5);
	const string& pathName = GetPortString(pActInfo, 6);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		unsigned short int nType = pAI->GetAIType();
		if ((nType != AIOBJECT_ACTOR) && (nType != AIOBJECT_VEHICLE))
			return;

		pAI->CastToIAIActor()->SetPathToFollow(pathName.c_str());   //, pathFindToStart, reverse );

		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->point2.x = (float)loops;
		pData->fValue = 10.0f;
		pData->point.x = pathFindToStart ? 1.0f : 0.0f;
		pData->point.y = reverse ? 1.0f : 0.0f;
		pData->point.z = nearest ? 1.0f : 0.0f;
		Execute(pActInfo, "ACT_FOLLOWPATH", pData);
	}
}

//////////////////////////////////////////////////////////////////////////
// Follow Path Speed Stance
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPathSpeedStance::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",             _HELP("for synchronization only"),    _HELP("sync")),
		InputPortConfig_Void("cancel",           _HELP("cancels execution")),
		InputPortConfig<bool>("PathFindToStart", _HELP("Path-find to start of path")),
		InputPortConfig<bool>("Reverse",         _HELP("Reverse the path direction")),
		InputPortConfig<bool>("StartNearest",    false,                                _HELP("Starts the path at nearest point on path")),
		InputPortConfig<int>("Loops",            0,                                    _HELP("Number of times to loop around the path (-1 = forever)")),
		InputPortConfig<string>("path_name",     _HELP("Name of the path to follow"),  "Path Name"),
		InputPortConfig<int>("Run",              1,                                    _HELP("Speed"),                                                  _HELP("Run"),     _UICONFIG(RUN_SPEED_UICONFIG)),
		InputPortConfig<int>("Stance",           4,                                    _HELP("Stance"),                                                 _HELP("Stance"),  _UICONFIG(STANCE_UICONFIG)),
		InputPortConfig<int>("Force",            0,                                    _HELP("ForceExecution method"),                                  _HELP("Force"),   _UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Follow path speed stance action");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_OBSOLETE);
}

DEF_CLONE(CFlowNode_AIFollowPathSpeedStance)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPathSpeedStance::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	bool pathFindToStart = GetPortBool(pActInfo, 2);
	bool reverse = GetPortBool(pActInfo, 3);
	bool nearest = GetPortBool(pActInfo, 4);
	int loops = GetPortInt(pActInfo, 5);
	const string& pathName = GetPortString(pActInfo, 6);
	int iSpeed = GetPortInt(pActInfo, 7);
	int iStance = GetPortInt(pActInfo, 8);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (pEntity)
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			uint16 nType = pAI->GetAIType();
			if ((nType != AIOBJECT_ACTOR) && (nType != AIOBJECT_VEHICLE))
				return;

			SetForceMethod(pAI, GetForceMethod(pActInfo));

			SetStance(pAI, iStance);
			SetSpeed(pAI, iSpeed);

			pAI->CastToIAIActor()->SetPathToFollow(pathName.c_str());

			IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
			pData->point2.x = (float)loops;
			pData->point.x = pathFindToStart ? 1.0f : 0.0f;
			pData->point.y = reverse ? 1.0f : 0.0f;
			pData->point.z = nearest ? 1.0f : 0.0f;
			pData->fValue = (float)iSpeed;
			Execute(pActInfo, "ACT_FOLLOWPATH", pData);
		}
	}
}

CFlowNode_AIFollowPathSpeedStance::EForceMethod CFlowNode_AIFollowPathSpeedStance::GetForceMethod(SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 9));   // Replace this number with an enum value
}

//////////////////////////////////////////////////////////////////////////
// Vehicle Follow Path
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIVehicleFollowPath::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",             _HELP("for synchronization only"),    _HELP("sync")),
		InputPortConfig_Void("cancel",           _HELP("cancels execution")),
		InputPortConfig<bool>("PathFindToStart", _HELP("Path-find to start of path")),
		InputPortConfig<bool>("Reverse",         _HELP("Reverse the path direction")),
		InputPortConfig<bool>("StartNearest",    false,                                _HELP("Starts the path at nearest point on path")),
		InputPortConfig<int>("Loops",            0,                                    _HELP("Number of times to loop around the path (-1 = forever)")),
		InputPortConfig<string>("path_name",     _HELP("Name of the path to follow"),  "Path Name"),
		InputPortConfig<float>("Speed",          10,                                   _HELP("Speed"),                                                  _HELP("Speed (m/s)")),
		InputPortConfig<int>("Force",            0,                                    _HELP("ForceExecution method"),                                  _HELP("Force"), _UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Follow path speed stance action");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIVehicleFollowPath)

void CFlowNode_AIVehicleFollowPath::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	// Dynamically update the point to approach to.
	if (m_EntityId && event == eFE_Activate && IsPortActive(pActInfo, 7))
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
		if (pEntity)
		{
			if (IAIObject* pAI = pEntity->GetAI())
			{
				unsigned short int nType = pAI->GetAIType();
				if (nType != AIOBJECT_VEHICLE)
				{
					CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "AI:VehicleFollowPath node should only be used to control vehicle entities");
					return;
				}

				IAIActor* pAIActor = pAI->CastToIAIActor();
				if (pAIActor)
				{
					pAIActor->GetState().fDesiredSpeed = GetPortFloat(pActInfo, 7);
				}
			}
		}
	}
	TBase::ProcessEvent(event, pActInfo);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIVehicleFollowPath::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	bool pathFindToStart = GetPortBool(pActInfo, 2);
	bool reverse = GetPortBool(pActInfo, 3);
	bool nearest = GetPortBool(pActInfo, 4);
	int loops = GetPortInt(pActInfo, 5);
	const string& pathName = GetPortString(pActInfo, 6);
	float speed = GetPortFloat(pActInfo, 7);    // m/s, NOT the usual run/walk/sprint

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (pEntity)
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			unsigned short int nType = pAI->GetAIType();
			if (nType != AIOBJECT_VEHICLE)
			{
				CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "AI:VehicleFollowPath node should only be used to control vehicle entities");
				return;
			}

			SetForceMethod(pAI, GetForceMethod(pActInfo));

			pAI->CastToIAIActor()->SetPathToFollow(pathName.c_str());

			IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
			pData->point2.x = (float)loops;
			pData->fValue = (float)speed;
			pData->point.x = pathFindToStart ? 1.0f : 0.0f;
			pData->point.y = reverse ? 1.0f : 0.0f;
			pData->point.z = nearest ? 1.0f : 0.0f;
			pData->iValue2 = 1; // this means 'don't control speed'
			Execute(pActInfo, "ACT_FOLLOWPATH", pData);
		}
	}
}

CFlowNode_AIVehicleFollowPath::TBase::EForceMethod CFlowNode_AIVehicleFollowPath::GetForceMethod(SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 8));   // Replace this number with an enum value
}

//////////////////////////////////////////////////////////////////////////
// Vehicle Stick Path
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIVehicleStickPath::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sync",          _HELP("for synchronization only")),
		InputPortConfig_Void("cancel",        _HELP("cancels execution")),
		InputPortConfig<string>("path_name",  _HELP("Name of the path to stick to and follow"),                                                                                            "Path Name"),
		InputPortConfig<bool>("Continuous",   false,                                                                                                                                       _HELP("Specifies if the vehicle should continue to follow the path (sticking to the target) or stop once it reaches the end or the specified target")),
		InputPortConfig<bool>("CanReverse",   true,                                                                                                                                        _HELP("Specifies if the vehicle is allowed to drive in reverse to either follow its target or move along a looped path")),
		InputPortConfig<float>("Speed",       10.0f,                                                                                                                                       _HELP("Speed"),                                                                                                                                         _HELP("Max speed of the vehicle (m/s)")),
		InputPortConfig<float>("DistanceMin", 0.0f,                                                                                                                                        _HELP("Minimum stick distance to target"),                                                                                                              "Min Distance"),
		InputPortConfig<float>("DistanceMax", 0.0f,                                                                                                                                        _HELP("Maximum stick distance to target"),                                                                                                              "Max Distance"),
		InputPortConfig<EntityId>("Target",   _HELP("Id of the target to stick to when following the path. Non-continuous following means vehicle will stop once the target is reached.")),
		InputPortConfig<int>("Force",         0,                                                                                                                                           _HELP("ForceExecution method"),                                                                                                                         _HELP("Force"),  _UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		OutputPortConfig<EntityId>("close",   _HELP("close to destination")),
		{ 0 }
	};
	config.sDescription = _HELP("Follows the specified path to the end, sticking to the optional target, either continuously or as a one-off event. Use the vehicle with the node's Entity input.");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIVehicleStickPath)

CFlowNode_AIVehicleStickPath::~CFlowNode_AIVehicleStickPath()
{
	UnregisterFromScriptEvent();
}

void CFlowNode_AIVehicleStickPath::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	TBase::Serialize(pActInfo, ser);

	ser.Value("m_entityRegisteredToAsListener", m_entityRegisteredToAsListener);
	int val = m_outputToActivate;
	ser.Value("m_outputToActivate", val);
	m_outputToActivate = (Outputs)val;
	ser.Value("m_activateOutput", m_activateOutput);
}

void CFlowNode_AIVehicleStickPath::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		m_ID = pActInfo->myID;
		TBase::ProcessEvent(event, pActInfo);
		break;
	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, 0))
			{
				// Dynamically update the point to approach to.
				IEntity* pEntity = GetEntity(pActInfo);

				const string& pathName = GetPortString(pActInfo, 2);
				const bool bContinuous = GetPortBool(pActInfo, 3);
				const bool bCanReverse = GetPortBool(pActInfo, 4);
				const float speed = GetPortFloat(pActInfo, 5);    // m/s, NOT the usual run/walk/sprint
				const float distanceMin = GetPortFloat(pActInfo, 6);
				const float distanceMax = GetPortFloat(pActInfo, 7);
				const EntityId targetId = GetPortEntityId(pActInfo, 8);

				if (pEntity)
				{
					if (IAIObject* pAI = pEntity->GetAI())
					{
						SetForceMethod(pAI, GetForceMethod(pActInfo));

						IAIActor* pAIActor = pAI->CastToIAIActor();
						if (pAIActor)
						{
							pAIActor->SetPathToFollow(pathName.c_str());
							pAIActor->GetState().fDesiredSpeed = speed;
						}

						// ------------------------------------------------------------------
						// make sure old sticpath nodes get cancelled
						// ------------------------------------------------------------------
						SEntityEvent cancelEvent(ENTITY_EVENT_SCRIPT_EVENT);
						cancelEvent.nParam[0] = (INT_PTR)"PathCancelled";
						cancelEvent.nParam[1] = IEntityClass::EVT_INT;
						cancelEvent.nParam[2] = (INT_PTR)pActInfo->myID;
						pEntity->SendEvent(cancelEvent);
						// ------------------------------------------------------------------

						IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
						pData->fValue = speed;
						pData->iValue2 = (bContinuous ? 1 : 0);
						pData->point.x = distanceMin;
						pData->point.y = distanceMax;
						pData->point.z = (bCanReverse ? 1.0f : 0.0f);
						pData->nID.n = targetId;
						Execute(pActInfo, "ACT_VEHICLESTICKPATH", pData);

						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
						RegisterForScriptEvent();

						m_activateOutput = false;
					}
				}
			}
			else if (IsPortActive(pActInfo, 1))
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				UnregisterFromScriptEvent();
			}
			else if (IsPortActive(pActInfo, 5))
			{
				// Dynamically update the point to approach to.
				IEntity* pEntity = GetEntity(pActInfo);

				if (pEntity)
				{
					if (IAIObject* pAI = pEntity->GetAI())
					{
						IAIActor* pAIActor = pAI->CastToIAIActor();

						if (IPipeUser* pipeUser = pAI->CastToIPipeUser())
						{
							IGoalPipe* pipe = pipeUser->GetCurrentGoalPipe();

							if (pipe)
							{
								pipe->ParseParam("desiredSpeed", GetPortFloat(pActInfo, 5));
							}
						}

						if (pAIActor)
						{
							pAIActor->GetState().fDesiredSpeed = GetPortFloat(pActInfo, 5);
						}
					}
				}
			}
		}
		break;

	case eFE_Update:
		if (m_activateOutput)
		{
			switch (m_outputToActivate)
			{
			case eOut_Failed:
			case eOut_Succeeded:
				ActivateOutput(pActInfo, m_outputToActivate, m_EntityId);
				ActivateOutput(pActInfo, eOut_Done, m_EntityId);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				break;

			case eOut_Close:
				ActivateOutput(pActInfo, m_outputToActivate, m_EntityId);
				break;

			default:
				ActivateOutput(pActInfo, m_outputToActivate, m_EntityId);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				break;
			}

			m_activateOutput = false;
		}
		break;

	default:
		TBase::ProcessEvent(event, pActInfo);
		break;
	}
}

void CFlowNode_AIVehicleStickPath::RegisterForScriptEvent()
{
	UnregisterFromScriptEvent();
	m_entityRegisteredToAsListener = m_EntityId;
	gEnv->pEntitySystem->AddEntityEventListener(m_entityRegisteredToAsListener, ENTITY_EVENT_SCRIPT_EVENT, this);
}

void CFlowNode_AIVehicleStickPath::UnregisterFromScriptEvent()
{
	if (gEnv->pEntitySystem && m_entityRegisteredToAsListener)
	{
		gEnv->pEntitySystem->RemoveEntityEventListener(m_entityRegisteredToAsListener, ENTITY_EVENT_SCRIPT_EVENT, this);
		m_entityRegisteredToAsListener = 0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIVehicleStickPath::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{

}

void CFlowNode_AIVehicleStickPath::PreExecute(SActivationInfo* pActInfo)
{
}

CFlowNode_AIVehicleStickPath::EForceMethod CFlowNode_AIVehicleStickPath::GetForceMethod(SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 9));   // Replace this number with an enum value
}

void CFlowNode_AIVehicleStickPath::OnEntityEvent(IEntity* pEntity, SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_SCRIPT_EVENT)
	{
		const char* eventName = (const char*)event.nParam[0];
		if (!strcmp("GoToSucceeded", eventName))
		{
			m_activateOutput = true;
			m_outputToActivate = eOut_Succeeded;
		}
		else if (!strcmp("GoToFailed", eventName))
		{
			m_activateOutput = true;
			m_outputToActivate = eOut_Failed;
		}
		else if (!strcmp("GoToCloseToDest", eventName))
		{
			m_activateOutput = true;
			m_outputToActivate = eOut_Close;
		}
		else if (!strcmp("PathCancelled", eventName))
		{
			//only activate output if this event was not send by this instance
			if (((event.nParam[1] == IEntityClass::EVT_INT) && event.nParam[2] != m_ID) ||
			    ((event.nParam[1] == IEntityClass::EVT_BOOL) && event.nParam[2] > 0))
			{
				m_activateOutput = true;
				m_outputToActivate = eOut_Done;
			}
		}

		if (m_activateOutput && (m_outputToActivate != eOut_Close))
		{
			gEnv->pEntitySystem->RemoveEntityEventListener(m_EntityId, ENTITY_EVENT_SCRIPT_EVENT, this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Vehicle Chase Target
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIVehicleChaseTarget::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sync",          _HELP("for synchronization only")),
		InputPortConfig_Void("cancel",        _HELP("cancels execution")),
		InputPortConfig<string>("path_name",  _HELP("Name of the path to stick to and follow"),                                                                                            "Path Name"),
		InputPortConfig<float>("Speed",       10.0f,                                                                                                                                       _HELP("Speed"),                            _HELP("Max speed of the vehicle (m/s)")),
		InputPortConfig<float>("DistanceMin", 0.0f,                                                                                                                                        _HELP("Minimum chase distance to target"), "Min Distance"),
		InputPortConfig<float>("DistanceMax", 0.0f,                                                                                                                                        _HELP("Maximum chase distance to target"), "Max Distance"),
		InputPortConfig<EntityId>("Target",   _HELP("Id of the target to stick to when following the path. Non-continuous following means vehicle will stop once the target is reached.")),
		InputPortConfig<int>("Force",         0,                                                                                                                                           _HELP("ForceExecution method"),            _HELP("Force"),  _UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("fail", _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Follows/navigates on the specified path, trying to establish line of sight/fire with the specified target.");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIVehicleChaseTarget)

void CFlowNode_AIVehicleChaseTarget::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		m_ID = pActInfo->myID;
		TBase::ProcessEvent(event, pActInfo);
		break;
	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, 0))
			{
				// Dynamically update the point to approach to.
				IEntity* pEntity = GetEntity(pActInfo);

				const string& pathName = GetPortString(pActInfo, 2);
				const float speed = GetPortFloat(pActInfo, 3);    // m/s, NOT the usual run/walk/sprint
				const float distanceMin = GetPortFloat(pActInfo, 4);
				const float distanceMax = GetPortFloat(pActInfo, 5);
				const EntityId targetId = GetPortEntityId(pActInfo, 6);

				if (pEntity)
				{
					if (IAIObject* pAI = pEntity->GetAI())
					{
						SetForceMethod(pAI, GetForceMethod(pActInfo));

						IAIActor* pAIActor = pAI->CastToIAIActor();
						if (pAIActor)
						{
							pAIActor->SetPathToFollow(pathName.c_str());
							pAIActor->GetState().fDesiredSpeed = speed;
						}

						// ------------------------------------------------------------------
						// make sure old stickpath nodes get cancelled
						// ------------------------------------------------------------------
						SEntityEvent cancelEvent(ENTITY_EVENT_SCRIPT_EVENT);
						cancelEvent.nParam[0] = (INT_PTR)"PathCancelled";
						cancelEvent.nParam[1] = IEntityClass::EVT_INT;
						cancelEvent.nParam[2] = (INT_PTR)pActInfo->myID;
						pEntity->SendEvent(cancelEvent);
						// ------------------------------------------------------------------

						IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
						pData->fValue = speed;
						pData->point.x = distanceMin;
						pData->point.y = distanceMax;
						pData->point.z = 0.0f;
						pData->nID.n = targetId;
						Execute(pActInfo, "ACT_CHASETARGET", pData);

						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
						//gEnv->pEntitySystem->AddEntityEventListener(m_EntityId, ENTITY_EVENT_SCRIPT_EVENT, this);

						m_activateOutput = false;
					}
				}
			}
			else if (IsPortActive(pActInfo, 1))
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				//gEnv->pEntitySystem->RemoveEntityEventListener(m_EntityId, ENTITY_EVENT_SCRIPT_EVENT, this);
			}
			else if (IsPortActive(pActInfo, 5))
			{
				// Dynamically update the point to approach to.
				IEntity* pEntity = GetEntity(pActInfo);

				if (pEntity)
				{
					if (IAIObject* pAI = pEntity->GetAI())
					{
						IAIActor* pAIActor = pAI->CastToIAIActor();

						if (IPipeUser* pipeUser = pAI->CastToIPipeUser())
						{
							if (IGoalPipe* pipe = pipeUser->GetCurrentGoalPipe())
								pipe->ParseParam("desiredSpeed", GetPortFloat(pActInfo, 3));
						}

						if (pAIActor)
							pAIActor->GetState().fDesiredSpeed = GetPortFloat(pActInfo, 3);
					}
				}
			}
		}
		break;

	case eFE_Update:
		if (m_activateOutput)
		{
			ActivateOutput(pActInfo, m_outputToActivate, m_EntityId);
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			m_activateOutput = false;
		}
		break;

	default:
		TBase::ProcessEvent(event, pActInfo);
		break;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIVehicleChaseTarget::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
}

void CFlowNode_AIVehicleChaseTarget::PreExecute(SActivationInfo* pActInfo)
{
}

CFlowNode_AIVehicleChaseTarget::EForceMethod CFlowNode_AIVehicleChaseTarget::GetForceMethod(SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 7));   // Replace this number with an enum value
}

void CFlowNode_AIVehicleChaseTarget::OnEntityEvent(IEntity* pEntity, SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_SCRIPT_EVENT)
	{
		const char* eventName = (const char*)event.nParam[0];
		if (!strcmp("PathCancelled", eventName))
		{
			//only activate output if this event was not send by this instance
			if (((event.nParam[1] == IEntityClass::EVT_INT) && event.nParam[2] != m_ID) ||
			    ((event.nParam[1] == IEntityClass::EVT_BOOL) && event.nParam[2] > 0))
			{
				m_activateOutput = true;
			}
		}

		if (m_activateOutput)
		{
			gEnv->pEntitySystem->RemoveEntityEventListener(m_EntityId, ENTITY_EVENT_SCRIPT_EVENT, this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// GOTO
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGoto::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",           _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig_Void("cancel",         _HELP("cancels execution")),
		InputPortConfig<Vec3>("pos"),
		InputPortConfig<float>("distance",     0.0f,                              _HELP("How accurately to position at the path end"),  "EndAccuracy"),
		InputPortConfig<float>("StopDistance", 0.0f,                              _HELP("Stop this amount before the end of the path")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("goto action");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGoto::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	// Dynamically update the point to approach to.
	if (m_EntityId && event == eFE_Activate && IsPortActive(pActInfo, 2))
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
		if (pEntity)
		{
			m_vDestination = GetPortVec3(pActInfo, 2);
			IAIObject* pAI = pEntity->GetAI();
			if (pAI)
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if (pPipeUser)
				{
					// First check is the current goal pipe the one which belongs to this node
					if (pPipeUser->GetGoalPipeId() == m_GoalPipeId)
					{
						pPipeUser->SetRefPointPos(m_vDestination);
					}
				}
				else
				{
					if (IAIActor* pAIActor = pAI->CastToIAIActor())
					{
						pAIActor->GoTo(m_vDestination);
					}
				}
			}
		}
	}
	TBase::ProcessEvent(event, pActInfo);
}

DEF_CLONE(CFlowNode_AIGoto)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGoto::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->point = m_vDestination = GetPortVec3(pActInfo, 2);
	pData->fValue = GetPortFloat(pActInfo, 3);
	pData->point2.x = GetPortFloat(pActInfo, 4);
	Execute(pActInfo, "ACT_GOTO", pData);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGoto::OnResume(SActivationInfo* pActInfo)
{
	if (!m_GoalPipeId)
		return;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		unsigned short int nType = pAI->GetAIType();
		if ((nType != AIOBJECT_ACTOR) && (nType != AIOBJECT_VEHICLE))
			return;

		IPipeUser* pPipeUser = pAI->CastToIPipeUser();
		if (pPipeUser)
		{
			// First check is the current goal pipe the one which belongs to this node
			if (pPipeUser->GetGoalPipeId() == m_GoalPipeId)
			{
				// restore his refPoint position
				pPipeUser->SetRefPointPos(m_vDestination);
			}
		}
		else
		{
			if (IAIActor* pAIActor = pAI->CastToIAIActor())
			{
				pAIActor->GoTo(m_vDestination);
			}
		}
	}
}

void CFlowNode_AIGoto::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	TBase::Serialize(pActInfo, ser);
	ser.Value("m_vDestination", m_vDestination);
}

//////////////////////////////////////////////////////////////////////////
// GOTO - Also sets speed and stance
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGotoSpeedStance::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",           _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig_Void("cancel",         _HELP("cancels execution")),
		InputPortConfig<Vec3>("pos"),
		InputPortConfig<float>("distance",     0.0f,                              _HELP("How accurately to position at the path end"),  "EndAccuracy"),
		InputPortConfig<float>("StopDistance", 0.0f,                              _HELP("Stop this amount before the end of the path")),
		InputPortConfig<int>("Run",            1,                                 _HELP("Speed"),                                       _HELP("Run"),    _UICONFIG(RUN_SPEED_UICONFIG)),
		InputPortConfig<int>("Stance",         4,                                 _HELP("Stance"),                                      _HELP("Stance"), _UICONFIG(STANCE_UICONFIG)),
		InputPortConfig<int>("Force",          0,                                 _HELP("ForceExecution method"),                       _HELP("Force"),  _UICONFIG(FORCE_UICONFIG)),
		InputPortConfig<bool>("allowStrafing", false,                             _HELP("Allow strafing")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Goto speed stance action.");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------

CFlowNode_AIGotoSpeedStance::EForceMethod CFlowNode_AIGotoSpeedStance::GetForceMethod(SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 7));
}

void CFlowNode_AIGotoSpeedStance::OnResume(SActivationInfo* pActInfo)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		unsigned short int nType = pAI->GetAIType();
		if ((nType != AIOBJECT_ACTOR) && (nType != AIOBJECT_VEHICLE))
			return;

		IPipeUser* pPipeUser = pAI->CastToIPipeUser();
		if (pPipeUser)
		{
			// First check is the current goal pipe the one which belongs to this node
			if (pPipeUser->GetGoalPipeId() == m_GoalPipeId)
			{
				// restore his refPoint position
				pPipeUser->SetRefPointPos(m_vDestination);

				// restore his speed, stance and allowstrafing
				SetStance(pAI, m_iStance);
				SetSpeed(pAI, m_iSpeed);
				SetAllowStrafing(pAI, m_bAllowStrafing);
			}
		}
		else
		{
			if (IAIActor* pAIActor = pAI->CastToIAIActor())
			{
				pAIActor->GoTo(m_vDestination);
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGotoSpeedStance::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (m_EntityId && event == eFE_Activate && IsPortActive(pActInfo, 2))
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
		if (pEntity)
		{
			m_vDestination = GetPortVec3(pActInfo, 2);
			IAIObject* pAI = pEntity->GetAI();
			if (pAI)
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if (pPipeUser)
				{
					// First check is the current goal pipe the one which belongs to this node
					if (pPipeUser->GetGoalPipeId() == m_GoalPipeId)
					{
						pPipeUser->SetRefPointPos(m_vDestination);
					}
				}
				else
				{
					if (IAIActor* pAIActor = pAI->CastToIAIActor())
					{
						pAIActor->GoTo(m_vDestination);
					}
				}
			}
		}
	}
	TBase::ProcessEvent(event, pActInfo);
}

DEF_CLONE(CFlowNode_AIGotoSpeedStance)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGotoSpeedStance::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	m_iSpeed = GetPortInt(pActInfo, 5);
	m_iStance = GetPortInt(pActInfo, 6);
	m_bAllowStrafing = GetPortBool(pActInfo, 8);

	IAIObject* pAI = GetEntity(pActInfo)->GetAI();
	if (pAI)
	{
		SetStance(pAI, m_iStance);
		SetSpeed(pAI, m_iSpeed);
		SetAllowStrafing(pAI, m_bAllowStrafing);
	}

	SetForceMethod(pAI, GetForceMethod(pActInfo));

	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->point = m_vDestination = GetPortVec3(pActInfo, 2);
	pData->fValue = GetPortFloat(pActInfo, 3);
	pData->point2.x = GetPortFloat(pActInfo, 4);

	Execute(pActInfo, "ACT_GOTO", pData);
}

void CFlowNode_AIGotoSpeedStance::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	TBase::Serialize(pActInfo, ser);

	ser.Value("m_vDestination", m_vDestination);
	ser.Value("m_iStance", m_iStance);
	ser.Value("m_iSpeed", m_iSpeed);
	ser.Value("m_bAllowStrafing", m_bAllowStrafing);
}

//////////////////////////////////////////////////////////////////////////
// Look At
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AILookAt::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",          _HELP("for synchronization only"),          _HELP("Sync")),
		InputPortConfig_Void("cancel",        _HELP("cancels execution")),
		InputPortConfig<Vec3>("pos",          _HELP("point to look at"),                  _HELP("Point")),
		InputPortConfig<Vec3>("Direction",    _HELP("direction to look at")),
		InputPortConfig<EntityId>("objectId", _HELP("ID of the object to look at")),
		InputPortConfig<float>("Duration",    _HELP("how long to look at [in seconds]")),
		InputPortConfig<int>("Force",         0,                                          _HELP("ForceExecution method"),_HELP("Force"),_UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Sets AI to look at a point, entity or direction.");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AILookAt)

CFlowNode_AILookAt::EForceMethod CFlowNode_AILookAt::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 6));
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AILookAt::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	if (!pEntity)
		return;

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		Cancel();
		return;
	}

	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
	{
		Cancel();
		return;
	}

	Vec3 pos = GetPortVec3(pActInfo, 2);
	Vec3 dir = GetPortVec3(pActInfo, 3);
	EntityId objectEntityId = GetPortEntityId(pActInfo, 4);

	if (!pos.IsZero(0.001f) || !dir.IsZero(0.001f) || (objectEntityId != 0))
	{
		// activate look at
		m_startTime = 0.f;
		m_bExecuted = true;
		ClearLookTarget();
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);

		SetForceMethod(pAI, GetForceMethod(pActInfo));
	}
	else
	{
		// reset look at
		pAIActor->ResetLookAt();

		Finish();
	}
}

bool CFlowNode_AILookAt::OnUpdate(SActivationInfo* pActInfo)
{
	if (!m_bExecuted)
		return false;

	IEntity* pEntity = GetEntity(pActInfo);
	if (!pEntity)
		return false;

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		Cancel();
		return false;
	}

	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
	{
		Cancel();
		return false;
	}

	// read inputs
	Vec3 pos = GetPortVec3(pActInfo, 2);
	if (pos.IsZero(0.001f))
	{
		EntityId objectEntityId = GetPortEntityId(pActInfo, 4);
		if (objectEntityId != 0)
		{
			if (IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(objectEntityId))
			{
				if (IAIObject* aiObject = pObjectEntity->GetAI())
				{
					pos = aiObject->GetPos();
				}
				else
				{
					pos = pObjectEntity->GetPos();
				}
			}
		}
	}

	Vec3 dir = GetPortVec3(pActInfo, 3);
	float duration = GetPortFloat(pActInfo, 5);

	// this is to enable canceling
	m_GoalPipeId = -1;

	if (!pos.IsZero(0.001f))
	{
		// look at point
		if (!m_startTime.GetValue())
		{
			SetLookTarget(pAI, pos);

			if (pAIActor->SetLookAtPointPos(pos, true))
			{
				if (duration > 0)
				{
					m_startTime = gEnv->pTimer->GetFrameStartTime() + duration;
				}
				else
				{
					m_bExecuted = false;
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					Finish();
				}
			}
		}
		else
		{
			if (gEnv->pTimer->GetFrameStartTime() >= m_startTime)
			{
				m_bExecuted = false;
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				pAIActor->ResetLookAt();
				Finish();
			}
			else
			{
				SetLookTarget(pAI, pos);
				pAIActor->SetLookAtPointPos(pos, true);
			}
		}
	}
	else if (!dir.IsZero(0.001f))
	{
		// look at direction
		if (!m_startTime.GetValue())
		{
			if (pAIActor->SetLookAtDir(dir, true))
			{
				if (duration > 0)
				{
					m_startTime = gEnv->pTimer->GetFrameStartTime() + duration;
				}
				else
				{
					m_bExecuted = false;
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					Finish();
				}
			}
		}
		else
		{
			if (gEnv->pTimer->GetFrameStartTime() >= m_startTime)
			{
				m_bExecuted = false;
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				pAIActor->ResetLookAt();
				Finish();
			}
		}
	}
	else
	{
		// reset look at
		m_bExecuted = false;
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		pAIActor->ResetLookAt();
		Finish();
	}

	return true;
}

void CFlowNode_AILookAt::OnCancel()
{
	TBase::OnCancel();

	m_bExecuted = true;
	m_pGraph->SetRegularlyUpdated(m_nodeID, false);

	ClearLookTarget();

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
		{
			IAIActor* pAIActor = pAI->CastToIAIActor();
			if (pAIActor)
				pAIActor->ResetLookAt();
		}
	}
}

void CFlowNode_AILookAt::OnFinish()
{
	ClearLookTarget();
}

void CFlowNode_AILookAt::SetLookTarget(IAIObject* pAI, Vec3 pos)
{
	if (IPipeUser* pipeUser = pAI->CastToIPipeUser())
	{
		if (!m_lookTarget.get())
		{
			m_lookTarget = pipeUser->CreateLookTarget();
		}

		*m_lookTarget = pos;
	}
}

void CFlowNode_AILookAt::ClearLookTarget()
{
	m_lookTarget.reset();
}

//////////////////////////////////////////////////////////////////////////
// body stance controller
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIStance::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",                                                                                                _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<int>("stance",                                                                                              4,                                 _HELP("0-prone\n1-crouch\n2-combat\n3-combat alerted\n4-relaxed\n5-stealth"),
		                     _HELP("Stance"),                                                                                       _UICONFIG("enum_int:Prone=0,Crouch=1,Combat=2,CombatAlerted=3,Relaxed=4,Stealth=5")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Body stance controller.");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIStance)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIStance::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IAIObject* pAI = GetEntity(pActInfo)->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (!pAIActor)
	{
		ActivateOutput(pActInfo, 0, m_EntityId);
		ActivateOutput(pActInfo, 2, m_EntityId);
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
		return;
	}

	bool success = true;
	SOBJECTSTATE& state = pAIActor->GetState();

	int stance = GetPortInt(pActInfo, 1);
	state.forceWeaponAlertness = false;
	switch (stance)
	{
	case 0:
		stance = STANCE_PRONE;
		break;
	case 1:
		stance = STANCE_CROUCH;
		break;
	case 2:
		stance = STANCE_STAND;
		break;
	case 3:
		state.forceWeaponAlertness = true;
		stance = STANCE_STAND;
		break;
	case 4:
		stance = STANCE_RELAXED;
		break;
	case 5:
		stance = STANCE_STEALTH;
		break;
	default:
		success = false;
		break;
	}
	state.bodystate = stance;

	if (pAI->HasFormation())
	{
		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->iValue = stance;
		gEnv->pAISystem->SendSignal(SIGNALFILTER_FORMATION_EXCEPT, 1, "OnChangeStance", pAI, pData);
	}

	ActivateOutput(pActInfo, 0, m_EntityId);
	ActivateOutput(pActInfo, success ? 1 : 2, m_EntityId);
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}

//////////////////////////////////////////////////////////////////////////
// unload vehicle
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",   _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig_Void("Cancel", _HELP("cancels execution")),
		InputPortConfig<int>("Seat",   _HELP("seat(s) to be unloaded"),   NULL,          _UICONFIG("enum_int:All=0,AllExceptDriver=-1,AllExceptGunner=-2,Driver=1,Gunner=2,Seat03=3,Seat04=4,Seat05=5,Seat06=6,Seat07=7,Seat08=8,Seat09=9,Seat10=10,Seat11=11")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Unloads vehicle, ejecting specified passengers");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIUnload)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	CRY_ASSERT(0); // This function should not be called!
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	// Override processing of eFE_Update event
	if (event != eFE_Update)
	{
		TBase::ProcessEvent(event, pActInfo);
	}
	else
	{
		// regular updates are not needed anymore
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

		m_bNeedsReset = true;
		IEntity* pEntity = GetEntity(pActInfo);
		if (pEntity)
		{
			// first check is the owner a vehicle
			IAIObject* pAI = pEntity->GetAI();
			IVehicle* pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
			if (!pVehicle)
			{
				m_EntityId = 0;
				gEnv->pLog->LogError("The owner entity '%s' of Vehicle:Unload FG node with ID %d is not a vehicle! ", pEntity->GetName(), pActInfo->myID);
			}
			else
			{
				m_mapPassengers.clear();
				int seatsToUnload = GetPortInt(pActInfo, 2);
				int numSeats = pVehicle->GetSeatCount();
				switch (seatsToUnload)
				{
				case -2: // all except the gunner
					UnloadSeat(pVehicle, 1);
					for (int i = 3; i <= numSeats; ++i)
						UnloadSeat(pVehicle, i);
					break;
				case -1: // all except the driver
					for (int i = 2; i <= numSeats; ++i)
						UnloadSeat(pVehicle, i);
					break;
				case 0: // all
					for (int i = 1; i <= numSeats; ++i)
						UnloadSeat(pVehicle, i);
					break;
				default:
					if (seatsToUnload <= numSeats)
					{
						UnloadSeat(pVehicle, seatsToUnload);
					}
					else
					{
						m_EntityId = 0;
						gEnv->pLog->LogError("Invalid vehicle seat id on Vehicle:Unload FG node with ID %d! ", pActInfo->myID);
					}
				}
				m_numPassengers = m_mapPassengers.size();
				if (!m_numPassengers)
					Finish();
			}
		}
		else
		{
			m_bExecuted = false;
			m_bSynchronized = false;
		}
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::UnloadSeat(IVehicle* pVehicle, int seatId)
{
	if (IVehicleSeat* pSeat = pVehicle->GetSeatById(seatId))
		if (EntityId passengerId = pSeat->GetPassenger())
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(passengerId))
				if (IAIObject* pAI = pEntity->GetAI())
					if (IAIActor* pAIActor = pAI->CastToIAIActor())
						if (IPipeUser* pPipeUser = pAI->CastToIPipeUser())
						{
							int goalPipeId = gEnv->pAISystem->AllocGoalPipeId();
							pPipeUser->RegisterGoalPipeListener(this, goalPipeId, "CFlowNode_AIUnload::UnloadSeat");
							m_mapPassengers[goalPipeId] = passengerId;

							IAISignalExtraData* pExtraData = gEnv->pAISystem->CreateSignalExtraData();
							pExtraData->iValue = goalPipeId;
							pAIActor->SetSignal(10, "ACT_EXITVEHICLE", pEntity, pExtraData);
						}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::OnGoalPipeEvent(IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent)
{
	std::map<int, EntityId>::iterator it = m_mapPassengers.find(goalPipeId);
	if (it == m_mapPassengers.end())
		return;

	switch (event)
	{
	case ePN_Deselected:
	case ePN_Removed:
	case ePN_Finished:
		if (--m_numPassengers == 0)
			Finish();
		break;
	case ePN_Suspended:
	case ePN_Resumed:
	case ePN_RefPointMoved:
		break;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::UnregisterEvents()
{
	std::map<int, EntityId>::iterator it, itEnd = m_mapPassengers.end();
	for (it = m_mapPassengers.begin(); it != itEnd; ++it)
	{
		if (it->second && gEnv->pEntitySystem)
		{
			if (gEnv->pEntitySystem)
				if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(it->second))
					if (IAIObject* pAI = pEntity->GetAI())
						if (IPipeUser* pPipeUser = pAI->CastToIPipeUser())
							pPipeUser->UnRegisterGoalPipeListener(this, it->first);
		}
	}
	m_mapPassengers.clear();
	TBase::UnregisterEvents();
}

//////////////////////////////////////////////////////////////////////////
// speed controller
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISpeed::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink", _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<int>("run",  1,                                 _HELP("0=very slow\n1=normal (walk)\n2=fast (run)\n3=very fast (sprint) -ve should reverse")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("movement speed controller");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AISpeed)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISpeed::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	//	AISIGNAL_EXTRA_DATA data( GetPortInt(pActInfo, 1) );
	//	Execute( pActInfo, "ACT_SPEED", &data );

	// signal version replaced with immediate version to avoid making this action blocking
	IAIObject* pAI = GetEntity(pActInfo)->GetAI();
	if (!pAI)
	{
		ActivateOutput(pActInfo, 0, m_EntityId);
		ActivateOutput(pActInfo, 2, m_EntityId);
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
		return;
	}

	int iSpeed = GetPortInt(pActInfo, 1);
	SetSpeed(pAI, iSpeed);

	ActivateOutput(pActInfo, 0, m_EntityId);
	ActivateOutput(pActInfo, 1, m_EntityId);
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_DebugAISpeed::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",    _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<float>("speed", 0.4f,                              _HELP("0.0 - stop\n0.1 - very slow (AI lower limit)\n0.2 - slow\n0.4 - walk\n1.0 - run\n1.3 - sprint\n1.6 - AI upper limit")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("This node should be used only for debugging!\nSets the speed urgency (pseudospeed) in the same range as AI would request it from game.");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_DebugAISpeed)

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_DebugAISpeed::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IAIActor* pAIActor = CastToIAIActorSafe(GetEntity(pActInfo)->GetAI());
	if (!pAIActor)
	{
		ActivateOutput(pActInfo, 0, m_EntityId);
		ActivateOutput(pActInfo, 2, m_EntityId);
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
		return;
	}

	pAIActor->GetState().fMovementUrgency = GetPortFloat(pActInfo, 1);

	ActivateOutput(pActInfo, 0, m_EntityId);
	ActivateOutput(pActInfo, 1, m_EntityId);
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}

//////////////////////////////////////////////////////////////////////////
// animation controller
//////////////////////////////////////////////////////////////////////////

CFlowNode_AIAnim::~CFlowNode_AIAnim()
{
	OnCancel();
}

void CFlowNode_AIAnim::OnCancelPortActivated(IPipeUser* pPipeUser, SActivationInfo* pActInfo)
{
	if (m_iMethod == 2)   // is method == Action?
	{
		pPipeUser->RemoveSubPipe(m_GoalPipeId, true);
		OnCancel(); // only to reuse unregistering code
	}
	else
		pPipeUser->CancelSubPipe(m_GoalPipeId);
}

void CFlowNode_AIAnim::OnCancel()
{
	if (m_pAGState)
	{
		m_pAGState->RemoveListener(this);

		switch (m_iMethod)
		{
		case 0:
			if (!m_bStarted)
				m_pAGState->ClearForcedStates();
			break;
		case 1:
			if (!m_bStarted)
				m_pAGState->SetInput("Signal", "none");
			m_pAGState->Hurry();
			break;
		case 2:
			m_pAGState->SetInput("Action", "idle");
			break;
		}

		m_pAGState = NULL;
		m_queryID = 0;
		m_bStarted = false;
	}
}

void CFlowNode_AIAnim::DestroyedState(IAnimationGraphState*)
{
	m_pAGState = NULL;
	m_queryID = 0;
	m_bStarted = false;
}

void CFlowNode_AIAnim::OnEntityEvent(IEntity* pEntity, SEntityEvent& event)
{
	TBase::OnEntityEvent(pEntity, event);

	if (event.event == ENTITY_EVENT_POST_SERIALIZE)
	{
		if (IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_EntityId))
		{
			m_pAGState = pActor->GetAnimationGraphState();
			if (m_pAGState != NULL)
			{
				m_pAGState->AddListener("aianim", this);

				if (!m_bStarted)
				{
					IAnimationGraphState::InputID inputID = (IAnimationGraphState::InputID) -1;
					switch (m_iMethod)
					{
					case 1: // use "signal"
						inputID = m_pAGState->GetInputId("Signal");
						break;
					case 2: // use "action"
						inputID = m_pAGState->GetInputId("Action");
						break;
					}
					if (inputID != (IAnimationGraphState::InputID) -1)
					{
						char inputValue[256];
						m_pAGState->GetInput(inputID, inputValue);
						m_pAGState->SetInput(inputID, inputValue, &m_queryID);
					}
				}
				else
				{
					if (m_iMethod <= 1)
						m_pAGState->QueryLeaveState(&m_queryID);
					else
						m_pAGState->QueryChangeInput("Action", &m_queryID);
				}
			}
		}
	}
}

void CFlowNode_AIAnim::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	TBase::Serialize(pActInfo, ser);

	ser.Value("m_bStarted", m_bStarted);
	ser.Value("m_iMethod", m_iMethod);

	if (ser.IsReading() && m_pAGState != NULL)
	{
		m_pAGState->RemoveListener(this);
		m_pAGState = NULL;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIAnim::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",                _HELP("for synchronization only"),                 _HELP("Sync")),
		InputPortConfig_Void("cancel",              _HELP("cancels execution (or stops the action)")),
		InputPortConfig<string>("animstateEx_name", _HELP("name of animation to be played")),
		InputPortConfig<int>("signal",              1,                                                 _HELP("which method to use"),_HELP("Method"),_UICONFIG("enum_int:Signal=1,Action=2")),
		//InputPortConfig<int>( "count", 1, _HELP("OBSOLETE PORT!!! Don't use it! Will be removed soon...") ),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		OutputPortConfig<EntityId>("start",   _HELP("activated on animation start")),
		{ 0 }
	};
	config.sDescription = _HELP("Plays animation");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIAnim)
//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIAnim::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_EntityId);
	if (pActor)
	{
		m_pAGState = pActor->GetAnimationGraphState();
		if (!m_pAGState)
			return;
		m_pAGState->AddListener("aianim", this);

		m_iMethod = GetPortInt(pActInfo, 3);
		switch (m_iMethod)
		{
		case 0: // forced state
			m_pAGState->PushForcedState(GetPortString(pActInfo, 2), &m_queryID);
			break;
		case 1: // use "signal"
			m_pAGState->SetInput("Signal", GetPortString(pActInfo, 2), &m_queryID);
			break;
		case 2: // use "action"
			m_pAGState->SetInput("Action", GetPortString(pActInfo, 2), &m_queryID);
			break;
		}
	}
	else
	{
		CRY_ASSERT(0);
	}

	if (!m_bStarted)
		Execute(pActInfo, "ACT_ANIM");
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIAnim::QueryComplete(TAnimationGraphQueryID queryID, bool succeeded)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (!pEntity)
		return;

	if (queryID != m_queryID)
		return;

	IAIObject* pAI = pEntity->GetAI();

	if (succeeded || m_bStarted && m_iMethod == 2)
	{
		if (!m_bStarted)
		{
			m_bStarted = true;

			// if "signal" wait to leave current state
			// if "action" wait for input value change
			if (m_iMethod <= 1)
				m_pAGState->QueryLeaveState(&m_queryID);
			else
				m_pAGState->QueryChangeInput("Action", &m_queryID);

			// activate "Start" output port
			SFlowAddress start(m_nodeID, 3, true);
			m_pGraph->ActivatePort(start, m_EntityId);
		}
		else
		{
			if (pAI)
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if (pPipeUser)
					pPipeUser->RemoveSubPipe(m_GoalPipeId, true);
			}

			if (m_pAGState)
			{
				m_pAGState->RemoveListener(this);

				m_pAGState = NULL;
				m_queryID = 0;
				m_bStarted = false;
			}
		}
	}
	else if (pAI)
	{
		IPipeUser* pPipeUser = pAI->CastToIPipeUser();
		if (pPipeUser)
			pPipeUser->CancelSubPipe(m_GoalPipeId);
	}
}

//////////////////////////////////////////////////////////////////////////
// AIAnimEx
//////////////////////////////////////////////////////////////////////////

void CFlowNode_AIAnimEx::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Sync",                                                                                                 _HELP("for synchronization only")),
		InputPortConfig_Void("Cancel",                                                                                               _HELP("cancels execution")),
		InputPortConfig<bool>("OneShot",                                                                                             true,                                              _HELP("True if it's an one shot animation (signal)\nFalse if it's a looping animation (action)")),
		InputPortConfig<string>("animstateEx_Animation",                                                                             _HELP("name of animation to be played")),
		InputPortConfig<int>("Speed",                                                                                                -1,                                                _HELP("speed while approaching the point"),
		                     NULL,                                                                                                   _UICONFIG("enum_int:<ignore>=-1,Very Slow=0,Normal/Walk=1,Fast/Run=2,Very Fast/Sprint=3")),
		InputPortConfig<int>("Stance",                                                                                               -1,                                                _HELP("body stance while approaching the point"),
		                     NULL,                                                                                                   _UICONFIG("enum_int:<ignore>=-1,Prone=0,Crouch=1,Combat=2,CombatAlerted=3,Relaxed=4,Stealth=5")),
		InputPortConfig<Vec3>("Position",                                                                                            _HELP("where to play the animation")),
		InputPortConfig<Vec3>("StartRadius",                                                                                         Vec3(.1f,                                          .1f,                                                                 .1f),_HELP("tolerance from where to start the animation")),
		InputPortConfig<Vec3>("Direction",                                                                                           _HELP("forward dir. while playing the animation")),
		InputPortConfig<float>("DirTolerance",                                                                                       180.0f,                                            _HELP("direction tolerance")),
		InputPortConfig<float>("TargetRadius",                                                                                       .05f,                                              _HELP("tolerance where to end the animation")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		OutputPortConfig<EntityId>("start",   _HELP("activated on animation start")),
		{ 0 }
	};
	config.sDescription = _HELP("Plays an exactly positioned animation");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_ADVANCED);
}

DEF_CLONE(CFlowNode_AIAnimEx)

void CFlowNode_AIAnimEx::DoProcessEvent(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (!pEntity)
		return;

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI) return;

	if (pAI->GetAIType() != AIOBJECT_ACTOR)
		return;

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return;

	m_bOneShot = GetPortBool(pActInfo, 2);
	const string& sAnimation = GetPortString(pActInfo, 3);
	m_iSpeed = GetPortInt(pActInfo, 4);
	m_iStance = GetPortInt(pActInfo, 5);
	m_vPos = GetPortVec3(pActInfo, 6);
	if (m_vPos.IsZero())
		m_vPos = pAI->GetPos();
	Vec3 vStartRadius = GetPortVec3(pActInfo, 7);
	m_vDir = GetPortVec3(pActInfo, 8);
	if (m_vDir.IsZero())
		m_vDir = pAI->GetMoveDir();
	m_vDir.Normalize();
	float fDirTolerance = GetPortFloat(pActInfo, 9);
	float fTargetRadius = GetPortFloat(pActInfo, 10);

	// Check if animation is valid
	if (!gEnv->IsEditor() || IsValidAnimationInputValue(pEntity, sAnimation.c_str(), m_bOneShot))
	{
		// set the stance and speed
		if (m_iStance >= 0)
			SetStance(pAI, m_iStance);
		if (m_iSpeed >= 0)
			SetSpeed(pAI, m_iSpeed);

		// set the refPoint position and orientation
		pPipeUser->SetRefPointPos(m_vPos, m_vDir);

		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->iValue2 = m_bOneShot ? 1 : 0;
		pData->SetObjectName(sAnimation);
		pData->point = vStartRadius;
		pData->fValue = fDirTolerance;
		pData->point2.x = fTargetRadius;

		CRY_ASSERT(m_State == eS_Disabled);
		m_State = eS_Requested;

		Execute(pActInfo, "ACT_ANIMEX", pData);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "AI:AnimEx node has invalid animation input for entity \'%s\': %s", pEntity->GetName(), sAnimation.c_str());
		Cancel();
	}
}

void CFlowNode_AIAnimEx::OnResume(IFlowNode::SActivationInfo* pActInfo)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (!pEntity)
		return;

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI) return;

	if (pAI->GetAIType() != AIOBJECT_ACTOR)
		return;

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return;

	// First check is the current goal pipe the one which belongs to this node
	if (pPipeUser->GetGoalPipeId() == m_GoalPipeId)
	{
		// restore the refPoint position
		pPipeUser->SetRefPointPos(m_vPos, m_vDir);

		// restore the speed and stance
		if (m_iStance >= 0)
			SetStance(pAI, m_iStance);
		if (m_iSpeed >= 0)
			SetSpeed(pAI, m_iSpeed);
	}
}

void CFlowNode_AIAnimEx::OnCancel()
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(m_EntityId))
	{
		if (IAIObject* ai = entity->GetAI())
		{
			if (IAIActor* actor = ai->CastToIAIActor())
			{
				actor->SetSignal(AISIGNAL_DEFAULT, "AnimationCanceled");
			}
		}
	}
}

void CFlowNode_AIAnimEx::OnGoalPipeEvent(IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent)
{
	TBase::OnGoalPipeEvent(pPipeUser, event, goalPipeId, unregisterListenerAfterEvent);
	if (m_GoalPipeId == goalPipeId && event == ePN_AnimStarted)
	{
		// activate "Start" output port
		SFlowAddress start(m_nodeID, 3, true);
		m_pGraph->ActivatePort(start, m_EntityId);

		CRY_ASSERT(m_State == eS_Requested);
		m_State = eS_Started;
	}
}

bool CFlowNode_AIAnimEx::IsValidAnimationInputValue(IEntity* pEntity, const char* szAnimation, bool bIsOneShot) const
{
	return false;
}

void CFlowNode_AIAnimEx::OnCancelPortActivated(IPipeUser* pPipeUser, SActivationInfo* pActInfo)
{
	if (m_State != eS_Disabled)
	{
		// Cancel actortarget request
		// note this doesn't properly cancel the exactpositioning yet: AIHandler doesn't handle canceling and exactpositioning doesn't seem to support it
		pPipeUser->ClearActorTargetRequest();

		// Cancel pending or running animation (this should be handled by canceling exactpositioning imo)
		if (IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_EntityId))
		{
			IAnimationGraphState* pAGState = pActor->GetAnimationGraphState();

			if (m_bOneShot)
			{
				if (m_State == eS_Requested)
				{
					pAGState->SetInput("Signal", "none"); // cancel any pending signals in case they haven't been processed yet
				}
				pAGState->Hurry(); // Force graph out of any signaled, hurryable state
			}
			else
			{
				// Unfortunately this doesn't work as the exactpositioning code is not properly canceled when the actor
				// target request is removed, and it keeps forcing the Action to whatever it was
				//pAGState->SetInput("Action", "idle"); // cancel any pending or running actions
			}
		}
	}

	TBase::OnCancelPortActivated(pPipeUser, pActInfo);
}

void CFlowNode_AIAnimEx::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	TBase::Serialize(pActInfo, ser);

	ser.Value("m_bOneShot", m_bOneShot);
	ser.Value("m_State", *alias_cast<int*>(&m_State));
}

void CFlowNode_AIAnimEx::PreExecute(SActivationInfo* pActInfo)
{
	TBase::PreExecute(pActInfo);

	m_State = eS_Disabled;
}

//////////////////////////////////////////////////////////////////////////
// grab object
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGrabObject::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",          _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig_Void("cancel",        _HELP("cancels execution")),
		InputPortConfig<EntityId>("objectId", _HELP("object to be grabbed")),
		InputPortConfig<int>("Force",         0,                                 _HELP("ForceExecution method"),_HELP("Force"),_UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Grabs an object.");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIGrabObject)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIGrabObject::EForceMethod CFlowNode_AIGrabObject::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 3));
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGrabObject::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	Execute(pActInfo, "ACT_GRAB_OBJECT", NULL, GetPortEntityId(pActInfo, 2));
}

//////////////////////////////////////////////////////////////////////////
// drop object
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIDropObject::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",     _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig_Void("cancel",   _HELP("cancels execution")),
		InputPortConfig<Vec3>("impulse", _HELP("use for throwing")),
		InputPortConfig<int>("Force",    0,                                 _HELP("ForceExecution method"),_HELP("Force"),_UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Drops grabbed object.");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIDropObject)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIDropObject::EForceMethod CFlowNode_AIDropObject::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 3));
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIDropObject::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->point = GetPortVec3(pActInfo, 2);
	Execute(pActInfo, "ACT_DROP_OBJECT", pData);
}

//////////////////////////////////////////////////////////////////////////
// draws weapon
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponDraw::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",  _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<int>("Force", 0,                                 _HELP("ForceExecution method"),_HELP("Force"),_UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Force AI to draw holstered weapon.");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIWeaponDraw)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIWeaponDraw::EForceMethod CFlowNode_AIWeaponDraw::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 1));
}
//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponDraw::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	Execute(pActInfo, "ACT_WEAPONDRAW");
}

//////////////////////////////////////////////////////////////////////////
// holsters weapon
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponHolster::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",  _HELP("for synchronization only"), _HELP("Sync")),
		InputPortConfig<int>("Force", 0,                                 _HELP("ForceExecution method"),_HELP("Force"),_UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Force AI to holster current weapon.");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIWeaponHolster)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIWeaponHolster::EForceMethod CFlowNode_AIWeaponHolster::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 1));
}
//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponHolster::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	Execute(pActInfo, "ACT_WEAPONHOLSTER");
}

//////////////////////////////////////////////////////////////////////////
// Shoot at given point
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",          _HELP("for synchronization only"),                                                                                                                                     _HELP("Sync")),
		InputPortConfig_Void("cancel",        _HELP("cancels execution")),
		InputPortConfig<Vec3>("targetPos",    _HELP("Position to shoot or aim at (dynamically updated). If TargetId is specified it will override this parameter."),                                                 _HELP("TargetPos")),
		InputPortConfig<float>("time",        1.0f,                                                                                                                                                                  _HELP("How long to shoot"),    _HELP("Duration")),
		InputPortConfig<int>("shots",         -1,                                                                                                                                                                    _HELP("How many shots to do"), _HELP("ShotsCount")),
		InputPortConfig<bool>("AimOnly",      _HELP("Only aim at target position instead of shooting")),
		InputPortConfig<EntityId>("TargetId", _HELP("Entity to shoot or aim at (dynamically updated), uses the position the AI system aims at if the entity has AI object. If not set TargetPos is used instead.")),
		InputPortConfig<int>("Force",         0,                                                                                                                                                                     _HELP("ForceExecution method"),_HELP("Force"),                 _UICONFIG(FORCE_UICONFIG)),
		InputPortConfig<bool>("AutoReload",   _HELP("Automatically reload when the clip is empty")),
		InputPortConfig<int>("UseSecondary",  _HELP("Use the secondary weapon to fire"),                                                                                                                             _HELP("UseSecondary"),         _UICONFIG(SECONDARY_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		OutputPortConfig_Void("OnShoot",      _HELP("shot is done")),
		{ 0 }
	};
	config.sDescription = _HELP("Set AI to shoot at position or entity.");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIShootAt)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIShootAt::EForceMethod CFlowNode_AIShootAt::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 7));
}

void CFlowNode_AIShootAt::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	TBase::Serialize(pActInfo, ser);
	ser.Value("m_isShootingAtEntity", m_isShootingAtEntity);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	m_actInfo = *pActInfo;

	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();

	// Choose the shoot at position based on the targetPos or targetId.
	EntityId targetId = GetPortEntityId(pActInfo, 6);
	IEntity* pTargetEntity = 0;
	if (targetId)
		pTargetEntity = gEnv->pEntitySystem->GetEntity(targetId);
	if (pTargetEntity)
	{
		// Use 'targetId'
		if (IAIObject* aiObject = pTargetEntity->GetAI())
			pData->point = aiObject->GetPos();
		else
			pData->point = pTargetEntity->GetPos();
		m_isShootingAtEntity = true;
	}
	else
	{
		// Use 'targetPos'
		pData->point = GetPortVec3(pActInfo, 2);
		m_isShootingAtEntity = false;
	}
	pData->fValue = GetPortFloat(pActInfo, 3);  // Time
	pData->iValue2 = GetPortInt(pActInfo, 9);   // ? 1 : 0; // UseSecondary
	pData->nID = targetId;
	// if in relaxed stance now - change to combat before shooting; no animations for shooting in relaxed stance
	IAIActor* pAIActor = CastToIAIActorSafe(GetEntity(pActInfo)->GetAI());
	if (pAIActor && pAIActor->GetProxy())
	{
		SAIBodyInfo bodyInfo;
		pAIActor->GetProxy()->QueryBodyInfo(bodyInfo);
		if (bodyInfo.stance == STANCE_RELAXED)
		{
			pAIActor->GetState().bodystate = STANCE_STAND;
		}
	}
	{
		IEntity* pEntity = GetEntity(pActInfo);
		SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));
	}
	if (GetPortBool(pActInfo, 5))
	{
		Execute(pActInfo, "ACT_AIMAT", pData);
	}
	else
	{
		Execute(pActInfo, "ACT_SHOOTAT", pData);
		// prepare shoots counting
		m_ShotsNumber = GetPortInt(pActInfo, 4);
	}
	if (m_isShootingAtEntity)
	{
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
	}
}

//------------------------------------------------------------------------
void CFlowNode_AIShootAt::UnregisterWithWeapon()
{
	if (m_weaponId)
	{
		IItem* pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_weaponId);
		if (pItem && pItem->GetIWeapon())
			pItem->GetIWeapon()->RemoveEventListener(this);

		m_weaponId = 0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::OnGoalPipeEvent(IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent)
{
	TBase::OnGoalPipeEvent(pPipeUser, event, goalPipeId, unregisterListenerAfterEvent);
	if (m_GoalPipeId == goalPipeId && event == ePN_Inserted)
	{
		IEntity* pEntity = GetEntity(&m_actInfo);
		if (pEntity)
		{
			IAIObject* pAI = pEntity->GetAI();
			IAIActorProxy* pProxy = pAI ? pAI->GetProxy() : NULL;
			if (pProxy)
			{
				EntityId currentWeaponId = 0;
				if (IWeapon* pWeapon = pProxy->QueryCurrentWeapon(currentWeaponId))
				{
					pWeapon->AddEventListener(this, __FUNCTION__);
					m_weaponId = currentWeaponId;
				}
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate)
	{
		m_autoReload = GetPortBool(pActInfo, 8);

		if (m_EntityId && IsPortActive(pActInfo, 2))
			UpdateShootFromInputs(pActInfo);
	}
	else if (event == eFE_Initialize)
	{
		m_isShootingAtEntity = false;
	}

	TBase::ProcessEvent(event, pActInfo);
}

bool CFlowNode_AIShootAt::OnUpdate(SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	if (!pEntity)
		return false;

	if (m_isShootingAtEntity)
	{
		UpdateShootFromInputs(pActInfo);
		return true;
	}

	return false;
}

void CFlowNode_AIShootAt::UpdateShootFromInputs(SActivationInfo* pActInfo)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
	if (pEntity)
	{
		// Choose the shoot at position based on the targetPos or targetId.
		Vec3 target;
		EntityId targetId = GetPortEntityId(pActInfo, 6);
		IEntity* pTargetEntity = 0;

		if (targetId)
			pTargetEntity = gEnv->pEntitySystem->GetEntity(targetId);

		if (pTargetEntity)
		{
			// Use 'targetId'
			if (IAIObject* aiObject = pTargetEntity->GetAI())
				target = aiObject->GetPos();
			else
				target = pTargetEntity->GetPos();
		}
		else
		{
			// Use 'targetPos'
			target = GetPortVec3(pActInfo, 2);
		}

		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
		{
			IPipeUser* pPipeUser = pAI->CastToIPipeUser();
			if (pPipeUser)
			{
				Vec3 dir = target - pAI->GetPos();
				dir.NormalizeSafe();
				pAI->SetViewDir(dir);

				IAIObject* pAIObject = pPipeUser->GetSpecialAIObject("refpoint");
				if (pAIObject)
				{
					pAIObject->SetPos(target);
					if (pTargetEntity)
						pAIObject->SetEntityID(targetId);
				}

				pAIObject = pPipeUser->GetSpecialAIObject("lookat_target");
				if (pAIObject)
					pAIObject->SetPos(target);
			}
		}
	}
}

void CFlowNode_AIShootAt::OnCancel()
{
	UnregisterWithWeapon();

	TBase::OnCancel();
	OnStopAction();

	if (m_EntityId)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId);
		if (pEntity)
		{
			IAIObject* pAI = pEntity->GetAI();
			if (pAI)
			{
				IAIActor* pAIActor = pAI->CastToIAIActor();
				if (pAIActor)
				{
					pAIActor->ResetLookAt();
					IPipeUser* pPipeUser = pAI->CastToIPipeUser();
					if (pPipeUser)
					{
						pPipeUser->SetFireMode(FIREMODE_OFF);
						//if ( pPipeUser->GetFireMode() == FIREMODE_AIM )
						//	pPipeUser->SetFireMode( FIREMODE_BURST, false );
					}
				}
			}
		}
	}
}

void CFlowNode_AIShootAt::OnStopAction()
{
	m_pGraph->SetRegularlyUpdated(m_nodeID, false);
	m_isShootingAtEntity = false;
}

void CFlowNode_AIShootAt::OnFinish()
{
	TBase::OnFinish();
	OnStopAction();
}

void CFlowNode_AIShootAt::UnregisterEvents()
{
	UnregisterWithWeapon();

	if (m_UnregisterEntityId && !m_EntityId)
		m_EntityId = m_UnregisterEntityId;

	TBase::UnregisterEvents();
}

void CFlowNode_AIShootAt::OnOutOfAmmo(IWeapon* pWeapon, IEntityClass* pAmmoType)
{
	if (m_autoReload)
		pWeapon->Reload(true);
}

//
//----------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::OnDropped(IWeapon* pWeapon, EntityId actorId)
{
	UnregisterWithWeapon();

	if (!m_EntityId)
		return;

	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId))
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			IPipeUser* pPipeUser = pAI->CastToIPipeUser();
			if (pPipeUser)
				pPipeUser->CancelSubPipe(m_GoalPipeId);
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
                                  const Vec3& pos, const Vec3& dir, const Vec3& vel)
{
	if (!m_EntityId || !m_GoalPipeId)
		return;

	if (m_ShotsNumber > 0)
	{
		if ((--m_ShotsNumber) == 0)
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_EntityId))
			{
				if (IAIObject* pAI = pEntity->GetAI())
				{
					IPipeUser* pPipeUser = pAI->CastToIPipeUser();
					if (pPipeUser)
						pPipeUser->RemoveSubPipe(m_GoalPipeId, true);
				}
			}
		}
	}

	SFlowAddress done(m_nodeID, 3, true);
	m_pGraph->ActivatePort(done, true);
}

//////////////////////////////////////////////////////////////////////////
// Uses an object
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUseObject::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",          _HELP("for synchronization only"),                            _HELP("Sync")),
		InputPortConfig_Void("cancel",        _HELP("cancels execution")),
		InputPortConfig<EntityId>("objectId", "Entity ID of the object which should be used by the agent"),
		InputPortConfig<int>("Force",         0,                                                            _HELP("ForceExecution method"),_HELP("Force"),_UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Uses an object");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIUseObject)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIUseObject::EForceMethod CFlowNode_AIUseObject::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 3));
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUseObject::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	Execute(pActInfo, "ACT_USEOBJECT", NULL, GetPortEntityId(pActInfo, 2));
}

//////////////////////////////////////////////////////////////////////////
// select weapon
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponSelect::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("sink",      _HELP("for synchronization only"),      _HELP("Sync")),
		InputPortConfig_Void("cancel",    _HELP("cancels execution")),
		InputPortConfig<string>("weapon", _HELP("name of weapon to be selected"), 0,                             _UICONFIG("enum_global:weapon")),
		InputPortConfig<int>("Force",     0,                                      _HELP("ForceExecution method"),_HELP("Force"),      _UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};
	config.sDescription = _HELP("Selects specified weapon.");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIWeaponSelect)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIWeaponSelect::EForceMethod CFlowNode_AIWeaponSelect::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, 3));
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponSelect::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->SetObjectName(GetPortString(pActInfo, 2));
	Execute(pActInfo, "ACT_WEAPONSELECT", pData);
}

//////////////////////////////////////////////////////////////////////////
// Makes ai enter specified seat of specified vehicle
//////////////////////////////////////////////////////////////////////////
void CFlowNode_AIEnterVehicle::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Trigger",        _HELP("for synchronization only")),
		InputPortConfig_Void("Cancel",         _HELP("Cancels execution")),
		InputPortConfig<EntityId>("VehicleId", _HELP("Vehicle to be entered")),
		InputPortConfig<string>("SeatNumber",  _HELP("Seat to be entered"),                              _HELP("Seat"),                   _UICONFIG("dt=vehicleSeats, ref_entity=VehicleId")),
		InputPortConfig<bool>("Fast",          _HELP("Skip approaching and playing entering animation")),
		InputPortConfig<int>("Force",          eNoForce,                                                 _HELP("ForceExecution method"),  _HELP("Force"),                                     _UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("Done", _HELP("Activated when the action finished")),
		OutputPortConfig<EntityId>("Success", _HELP("Activated when Vehicle:Enter action succeeded")),
		OutputPortConfig<EntityId>("Fail",    _HELP("Activated when Vehicle:Enter action failed")),
		{ 0 }
	};

	config.sDescription = _HELP("Makes AI enter specified seat of specified vehicle");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIEnterVehicle)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIEnterVehicle::EForceMethod CFlowNode_AIEnterVehicle::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, eIn_ForceMethod));
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIEnterVehicle::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (!pActInfo->pEntity)
	{
		ActivateOutput(pActInfo, eOut_Success, false);
		return;
	}

	bool bFast = GetPortBool(pActInfo, eIn_Fast);
	bool bSuccess = false;

	// test that we can enter the vehicle
	EntityId vehicleId = GetPortEntityId(pActInfo, eIn_VehicleId);
	IVehicle* pVehicle = CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle(vehicleId);
	if (!pVehicle || pVehicle->IsCrewHostile(pActInfo->pEntity->GetId()))
	{
		CryLog("Actor %s failed to enter vehicle (IsCrewHostile returned true)", pActInfo->pEntity->GetName());
		ActivateOutput(pActInfo, eOut_Success, false);
		return;
	}

	TVehicleSeatId seatId = pVehicle->GetSeatId(GetPortString(pActInfo, eIn_Seat));

	if (!bFast)
	{
		ICVar* pTransitionsVar = gEnv->pConsole->GetCVar("v_transitionAnimations");
		if (pTransitionsVar && pTransitionsVar->GetIVal() == 0)
			bFast = true;
	}

	IEntity* pEntity = GetEntity(pActInfo);

	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	IActor* pActor = pEntity ? CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pEntity->GetId()) : 0;

	if (pActor && pActor->IsPlayer())
	{
		if (IVehicleSeat* pVehicleSeat = pVehicle->GetSeatById(seatId))
		{
			bSuccess = pVehicleSeat->Enter(pEntity->GetId(), true);
		}
	}
	else if (pEntity && pEntity->GetAI())
	{
		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();

		pData->fValue = (float)seatId;
		pData->nID = vehicleId;
		pData->iValue2 = bFast;

		bSuccess = Execute(pActInfo, "ACT_ENTERVEHICLE", pData);
	}
	else if (pActor)
	{
		IVehicleSeat* seat = pVehicle->GetSeatById(seatId);
		if (seat)
		{
			pActor->HolsterItem(true);
			pActor->MountedGunControllerEnabled(false);
			pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
			bSuccess = seat->Enter(pEntity->GetId(), true);
		}
	}

	// PS - Allow node to fire a second time.
	m_bNeedsExec = false;
	ActivateOutput(pActInfo, eOut_Success, bSuccess);
}

//////////////////////////////////////////////////////////////////////////
// Makes ai exit vehicle
//////////////////////////////////////////////////////////////////////////
void CFlowNode_AIExitVehicle::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Sync",   _HELP("for synchronization only")),
		InputPortConfig_Void("Cancel", _HELP("cancels execution")),
		InputPortConfig<int>("Force",  eNoForce,                          _HELP("ForceExecution method"),_HELP("Force"), _UICONFIG(FORCE_UICONFIG)),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>("done",    _HELP("action done")),
		OutputPortConfig<EntityId>("succeed", _HELP("action done successfully")),
		OutputPortConfig<EntityId>("fail",    _HELP("action failed")),
		{ 0 }
	};

	config.sDescription = _HELP("makes ai exit a vehicle");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

DEF_CLONE(CFlowNode_AIExitVehicle)

//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIExitVehicle::EForceMethod CFlowNode_AIExitVehicle::GetForceMethod(IFlowNode::SActivationInfo* pActInfo) const
{
	return static_cast<EForceMethod>(GetPortInt(pActInfo, eIn_Force));
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExitVehicle::DoProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IEntity* pEntity = GetEntity(pActInfo);
	SetForceMethod(pEntity ? pEntity->GetAI() : 0, GetForceMethod(pActInfo));

	Execute(pActInfo, "ACT_EXITVEHICLE");
}

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------
class CFlowNode_AILookAtPoint : public CFlowNode_AILookAt
{
public:
	CFlowNode_AILookAtPoint(SActivationInfo* pActInfo) : CFlowNode_AILookAt(pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		CFlowNode_AILookAt::GetConfiguration(config);
		config.SetCategory(EFLN_OBSOLETE);
	}
};

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------
class CFlowNode_AIEnterVehicle_Old : public CFlowNode_AIEnterVehicle
{
public:
	CFlowNode_AIEnterVehicle_Old(SActivationInfo* pActInfo) : CFlowNode_AIEnterVehicle(pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		CFlowNode_AIEnterVehicle::GetConfiguration(config);
		config.SetCategory(EFLN_OBSOLETE);
	}
};

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------
class CFlowNode_AIUnload_Old : public CFlowNode_AIUnload
{
public:
	CFlowNode_AIUnload_Old(SActivationInfo* pActInfo) : CFlowNode_AIUnload(pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		CFlowNode_AIUnload::GetConfiguration(config);
		config.SetCategory(EFLN_OBSOLETE);
	}
};

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------
CFlowNode_AIGoToEx::CFlowNode_AIGoToEx(IFlowNode::SActivationInfo* pActInfo)
	: m_entityID(0)
	, m_activateOutput(false)
	, m_isPrepared(false)
	, m_outputToActivate(eOut_Succeeded)
{
}

CFlowNode_AIGoToEx::~CFlowNode_AIGoToEx()
{
	if (m_entityID)
		gEnv->pEntitySystem->RemoveEntityEventListener(m_entityID, ENTITY_EVENT_SCRIPT_EVENT, this);
}

void CFlowNode_AIGoToEx::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Sync"),
		InputPortConfig_Void("Cancel"),
		InputPortConfig<Vec3>("Position"),
		InputPortConfig<float>("StopDistance",    0.0f),
		InputPortConfig<int>("Stance",            3,      _HELP("Note : The crouch stance is only available to the humans."),            NULL, _UICONFIG("enum_int:Relaxed=3,Alerted=6,Combat=0,Crouch=4")),
		InputPortConfig<int>("Speed",             0,      NULL,                                                                          NULL, _UICONFIG("enum_int:Walk=0,Run=1,Sprint=2")),
		InputPortConfig<bool>("Strafe",           false),
		InputPortConfig<bool>("AllowFire",        true),
		InputPortConfig<bool>("BreakOnTarget",    true,   _HELP("Interrupts the go to command if the agent has an attention target.")),
		InputPortConfig<bool>("BreakOnBulletRain",false,  _HELP("Interrupts the go to command if the agent is under enemy fire.")),
		InputPortConfig<bool>("Prepare",          false),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<int>("Done"),
		OutputPortConfig<int>("Succeeded"),
		OutputPortConfig<int>("Failed",     _HELP("Triggers whenever the command is interrupted or failed to get a path.")),
		OutputPortConfig<int>("Interrupted",_HELP("Triggers when the command is interrupted by any of the break conditions, the cancel input or another GoTo command.")),
		OutputPortConfig<int>("NoPath"),
		OutputPortConfig<int>("Close"),
		{ 0 }
	};

	config.sDescription = _HELP("");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_OBSOLETE);
}

void CFlowNode_AIGoToEx::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		{
			if (m_entityID)
			{
				gEnv->pEntitySystem->RemoveEntityEventListener(m_entityID, ENTITY_EVENT_SCRIPT_EVENT, this);
				m_entityID = 0;
				m_isPrepared = false;
			}
			m_activateOutput = false;
			break;
		}
	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, eIN_Prepare))
			{
				if (IEntity* entity = pActInfo->pEntity)
				{
					if (IScriptTable* entityScriptTable = entity->GetScriptTable())
					{
						HSCRIPTFUNCTION functionToCall = 0;
						if (entityScriptTable->GetValue("SetGoToPosition", functionToCall))
						{
							// Setup data
							SmartScriptTable goToData(gEnv->pScriptSystem);

							CScriptVector position(gEnv->pScriptSystem);
							position.Set(GetPortVec3(pActInfo, eIN_Position));
							goToData->SetValue("Position", position);
							Script::CallMethod(entityScriptTable, functionToCall, goToData);
							gEnv->pScriptSystem->ReleaseFunc(functionToCall);

							gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 0, "OnPrepareGoTo", entity->GetAI());

							//pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);

							m_isPrepared = true;
						}
					}
				}
			}

			if (IsPortActive(pActInfo, eIN_Go))
			{
				bool goToTriggered = false;

				if (IEntity* entity = pActInfo->pEntity)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					gEnv->pEntitySystem->RemoveEntityEventListener(m_entityID, ENTITY_EVENT_SCRIPT_EVENT, this);
					m_entityID = entity->GetId();

					if (!m_isPrepared)
					{
						// Send interrupt event to other goto flownode that could be running
						SEntityEvent entityEvent(ENTITY_EVENT_SCRIPT_EVENT);
						entityEvent.nParam[0] = (INT_PTR)"GoToInterrupted";
						entity->SendEvent(entityEvent);
					}

					bool breakOnTarget = GetPortBool(pActInfo, eIN_BreakOnTarget);
					if (breakOnTarget)
					{
						bool hasTarget = false;
						if (IAIObject* aiobject = entity->GetAI())
							if (IAIActor* aiactor = aiobject->CastToIAIActor())
								hasTarget = (aiactor->GetAttentionTarget() != NULL);

						if (hasTarget)
						{
							if (IScriptTable* entityScriptTable = entity->GetScriptTable())
							{
								SmartScriptTable currentBehavior;
								entityScriptTable->GetValue("Behavior", currentBehavior);

								HSCRIPTFUNCTION functionToCall = 0;
								if (currentBehavior && currentBehavior->GetValue("OnCancelGoTo", functionToCall))
								{
									Script::CallMethod(currentBehavior, functionToCall, entityScriptTable);
									gEnv->pScriptSystem->ReleaseFunc(functionToCall);
								}
							}

							TriggerOutput(pActInfo, eOut_Interrupted);
							return;
						}
					}

					m_activateOutput = false;

					if (IScriptTable* entityScriptTable = entity->GetScriptTable())
					{
						HSCRIPTFUNCTION functionToCall = 0;
						if (entityScriptTable->GetValue("SetGoToData", functionToCall))
						{
							// Setup data
							SmartScriptTable goToData(gEnv->pScriptSystem);

							CScriptVector position(gEnv->pScriptSystem);
							position.Set(GetPortVec3(pActInfo, eIN_Position));
							goToData->SetValue("Position", position);
							goToData->SetValue("StopDistance", GetPortFloat(pActInfo, eIN_StopDistance));
							goToData->SetValue("Stance", GetPortInt(pActInfo, eIN_Stance));
							switch (GetPortInt(pActInfo, eIN_Speed))
							{
							case 0:
								goToData->SetValue("Speed", AISPEED_WALK);
								break;
							case 1:
								goToData->SetValue("Speed", AISPEED_RUN);
								break;
							case 2:
								goToData->SetValue("Speed", AISPEED_SPRINT);
								break;
							}
							goToData->SetValue("Strafe", GetPortBool(pActInfo, eIN_Strafe));
							goToData->SetValue("AllowFire", GetPortBool(pActInfo, eIN_AllowFire));
							goToData->SetValue("BreakOnTarget", breakOnTarget);
							goToData->SetValue("BreakOnBulletRain", GetPortBool(pActInfo, eIN_BreakOnBulletRain));

							Script::CallMethod(entityScriptTable, functionToCall, goToData);
							gEnv->pScriptSystem->ReleaseFunc(functionToCall);

							if (!m_isPrepared)
							{
								gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 0, "OnExecuteGoTo", entity->GetAI());
							}

							m_isPrepared = false;
							pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
							gEnv->pEntitySystem->AddEntityEventListener(m_entityID, ENTITY_EVENT_SCRIPT_EVENT, this);

							goToTriggered = true;
						}
					}

					if (!goToTriggered)
						TriggerOutput(pActInfo, eOut_Failed);
				}
			}
			if (IsPortActive(pActInfo, eIN_Cancel))
			{
				if (IEntity* entity = pActInfo->pEntity)
				{
					if (IScriptTable* entityScriptTable = entity->GetScriptTable())
					{
						SmartScriptTable currentBehavior;
						entityScriptTable->GetValue("Behavior", currentBehavior);

						HSCRIPTFUNCTION functionToCall = 0;
						if (currentBehavior && currentBehavior->GetValue("OnCancelGoTo", functionToCall))
						{
							Script::CallMethod(currentBehavior, functionToCall, entityScriptTable);
							gEnv->pScriptSystem->ReleaseFunc(functionToCall);
							TriggerOutput(pActInfo, eOut_Interrupted);
						}
					}
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					gEnv->pEntitySystem->RemoveEntityEventListener(m_entityID, ENTITY_EVENT_SCRIPT_EVENT, this);
				}
			}
			break;
		}
	case eFE_Update:
		{
			if (m_activateOutput)
			{
				m_activateOutput = false;

				if (m_outputToActivate != eOut_Close)
				{
					TriggerOutput(pActInfo, m_outputToActivate);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				}
				else
				{
					TriggerOutput(pActInfo, m_outputToActivate, false);
				}
			}
		}
	}
}

void CFlowNode_AIGoToEx::OnEntityEvent(IEntity* pEntity, SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_SCRIPT_EVENT)
	{
		const char* eventName = (const char*)event.nParam[0];
		if (!strcmp("GoToSucceeded", eventName))
		{
			m_activateOutput = true;
			m_outputToActivate = eOut_Succeeded;
			gEnv->pEntitySystem->RemoveEntityEventListener(m_entityID, ENTITY_EVENT_SCRIPT_EVENT, this);
		}
		else if (!strcmp("GoToInterrupted", eventName))
		{
			m_activateOutput = true;
			m_outputToActivate = eOut_Interrupted;
			gEnv->pEntitySystem->RemoveEntityEventListener(m_entityID, ENTITY_EVENT_SCRIPT_EVENT, this);
		}
		else if (!strcmp("GoToFailed", eventName))
		{
			m_activateOutput = true;
			m_outputToActivate = eOut_NoPath;
			gEnv->pEntitySystem->RemoveEntityEventListener(m_entityID, ENTITY_EVENT_SCRIPT_EVENT, this);
		}
		else if (!strcmp("GoToCloseToDest", eventName))
		{
			m_activateOutput = true;
			m_outputToActivate = eOut_Close;
		}
	}
}

void CFlowNode_AIGoToEx::TriggerOutput(SActivationInfo* pActInfo, Outputs output, bool triggerDoneOutput /*= true*/)
{
	ActivateOutput(pActInfo, output, m_entityID);

	if (triggerDoneOutput)
		ActivateOutput(pActInfo, eOut_Done, m_entityID);

	// Triggering the failed output whenever the goto was unsuccessful, as requested by level design. /Mario
	if (output == eOut_Interrupted || output == eOut_NoPath)
		ActivateOutput(pActInfo, eOut_Failed, m_entityID);
}

//////////////////////////////////////////////////////////////////////////
// Get vehicle seat helper positions
//////////////////////////////////////////////////////////////////////////
void CFlowNode_GetSeatHelperVehicle::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Get",           _HELP("Trigger to get the values"),                    _HELP("Trigger")),
		InputPortConfig<string>("SeatNumber", _HELP("Seat that is connected to the 'enter' helper"), _HELP("Seat"),    _UICONFIG("dt=vehicleSeats, ref_entity=entityId")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<Vec3>("pos", _HELP("Position of the seat helper")),
		OutputPortConfig<Vec3>("dir", _HELP("Direction of the seat helper")),
		{ 0 }
	};

	config.sDescription = _HELP("Gets the position and orientation of a seat's 'enter' helper in world space");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_GetSeatHelperVehicle::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	CFlowVehicleBase::ProcessEvent(event, pActInfo); // handles eFE_SetEntityId

	if (event == eFE_Activate && IsPortActive(pActInfo, eIn_Get))
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(GetVehicle());
		if (pVehicle)
		{
			const string seatName = GetPortString(pActInfo, eIn_Seat);
			CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pVehicle->GetSeatById(pVehicle->GetSeatId(seatName)));
			if (pSeat)
			{
				Matrix34 tm;
				pSeat->GetEnterHelper()->GetWorldTM(tm);
				ActivateOutput(pActInfo, eOut_Pos, tm.GetTranslation());
				ActivateOutput(pActInfo, eOut_Dir, tm.GetColumn(1));
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------

REGISTER_FLOW_NODE("AI:AIExecute", CFlowNode_AIExecute)
REGISTER_FLOW_NODE("AI:AISignal", CFlowNode_AISignal)
REGISTER_FLOW_NODE("AI:AISetState", CFlowNode_AISetState)
REGISTER_FLOW_NODE("AI:AIModifyStates", CFlowNode_AIModifyStates)
REGISTER_FLOW_NODE("AI:AICheckStates", CFlowNode_AICheckStates)
REGISTER_FLOW_NODE("AI:AIGoto", CFlowNode_AIGoto)
REGISTER_FLOW_NODE("AI:AIGotoSpeedStance", CFlowNode_AIGotoSpeedStance)
REGISTER_FLOW_NODE("AI:AILookAtPoint", CFlowNode_AILookAtPoint)
REGISTER_FLOW_NODE("AI:AILookAt", CFlowNode_AILookAt)
REGISTER_FLOW_NODE("AI:AIStance", CFlowNode_AIStance)
REGISTER_FLOW_NODE("AI:AISpeed", CFlowNode_AISpeed)
REGISTER_FLOW_NODE("AI:DebugAISpeed", CFlowNode_DebugAISpeed)
REGISTER_FLOW_NODE("AI:AIAnim", CFlowNode_AIAnim)
REGISTER_FLOW_NODE("AI:AIAnimEx", CFlowNode_AIAnimEx)
REGISTER_FLOW_NODE("AI:AIFollowPath", CFlowNode_AIFollowPath)
REGISTER_FLOW_NODE("AI:AIFollowPathSpeedStance", CFlowNode_AIFollowPathSpeedStance)
REGISTER_FLOW_NODE("AI:AIUnload", CFlowNode_AIUnload_Old)
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
REGISTER_FLOW_NODE("AI:AISetCharacter", CFlowNode_AISetCharacter)
#endif
REGISTER_FLOW_NODE("AI:AIGrabObject", CFlowNode_AIGrabObject)
REGISTER_FLOW_NODE("AI:AIDropObject", CFlowNode_AIDropObject)
REGISTER_FLOW_NODE("AI:AIWeaponDraw", CFlowNode_AIWeaponDraw)
REGISTER_FLOW_NODE("AI:AIWeaponHolster", CFlowNode_AIWeaponHolster)
REGISTER_FLOW_NODE("AI:AIShootAt", CFlowNode_AIShootAt)
REGISTER_FLOW_NODE("AI:AIUseObject", CFlowNode_AIUseObject)
REGISTER_FLOW_NODE("AI:AIWeaponSelect", CFlowNode_AIWeaponSelect)
REGISTER_FLOW_NODE("AI:AIEnterVehicle", CFlowNode_AIEnterVehicle_Old)
REGISTER_FLOW_NODE("AI:AIAlertMe", CFlowNode_AISignalAlerted)
REGISTER_FLOW_NODE("Vehicle:Enter", CFlowNode_AIEnterVehicle)
REGISTER_FLOW_NODE("Vehicle:Unload", CFlowNode_AIUnload)
REGISTER_FLOW_NODE("Vehicle:Exit", CFlowNode_AIExitVehicle)
REGISTER_FLOW_NODE("Vehicle:FollowPath", CFlowNode_AIVehicleFollowPath)
REGISTER_FLOW_NODE("Vehicle:StickPath", CFlowNode_AIVehicleStickPath)
REGISTER_FLOW_NODE("Vehicle:ChaseTarget", CFlowNode_AIVehicleChaseTarget)
REGISTER_FLOW_NODE("Vehicle:GetSeatHelper", CFlowNode_GetSeatHelperVehicle)

REGISTER_FLOW_NODE("AI:GoToEx", CFlowNode_AIGoToEx)

#undef DEF_CLONE
