// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

#include "FlowNodeAIAction.h"
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IGoalPipe.h>
#include <CryFlowGraph/IFlowBaseNode.h>

//#pragma optimize("", off)
//#pragma inline_depth(0)

#define DEF_CLONE(CLASS) IFlowNodePtr CLASS::Clone(SActivationInfo * pActInfo){ CLASS* p = new CLASS(pActInfo); p->m_bNeedsExec = m_bNeedsExec; p->m_bNeedsSink = m_bNeedsSink; return p; }

static const char* FORCE_UICONFIG = "enum_int:No=0,IgnoreAll=1";

#if !defined(RELEASE)
#define CRYACTION_AI_VERBOSITY
#endif

#if !defined(RELEASE)
	#define AILogEventID    gEnv->pAISystem->LogEvent
#else
	#define AILogEventID    (void)
#endif

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
template<bool TBlocking> void CFlowNode_AIBase<TBlocking >::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
{
	switch (event.event)
	{
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
			IVehicle* pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
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

	IAIActorProxy* pAIProxy = pAIActor->GetProxy();
	if (pAIProxy)
	{
		switch (method)
		{
		case eIgnoreAll:
			pAIProxy->SetForcedExecute(true);
			break;
		case eNoForce:
			pAIProxy->SetForcedExecute(false);
			break;
		default:
			assert(0);
			break;
		}
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

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------

REGISTER_FLOW_NODE("AI:Execute", CFlowNode_AIExecute)
REGISTER_FLOW_NODE("AI:Signal", CFlowNode_AISignal)
REGISTER_FLOW_NODE("AI:SetState", CFlowNode_AISetState)
REGISTER_FLOW_NODE("AI:LookAt", CFlowNode_AILookAt)
REGISTER_FLOW_NODE("AI:Stance", CFlowNode_AIStance)
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
REGISTER_FLOW_NODE("AI:CharacterSet", CFlowNode_AISetCharacter)
#endif
REGISTER_FLOW_NODE("AI:ObjectDrop", CFlowNode_AIDropObject)
REGISTER_FLOW_NODE("AI:ObjectUse", CFlowNode_AIUseObject)
REGISTER_FLOW_NODE("AI:AlertMe", CFlowNode_AISignalAlerted)

#undef DEF_CLONE
