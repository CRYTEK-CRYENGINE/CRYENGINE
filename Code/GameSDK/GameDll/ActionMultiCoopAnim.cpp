// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Implements Multiple Actor Coop Anims.

*************************************************************************/
#include "StdAfx.h"

#include "ActionMultiCoopAnim.h"

#include "Player.h"
#include "RecordingSystem.h"
#include "PlayerStateEvents.h"

//#define MCALOG

#ifdef MCALOG
#define MCALog(...)					CryLog("[MCA] "__VA_ARGS__)
#define MCALogAlways(...)		CryLogAlways("[MCA] "__VA_ARGS__)
#else
#define MCALog(...)					((void)0)
#define MCALogAlways(...)		((void)0)
#endif

CActionMultiCoopAnimation::CActionMultiCoopAnimation( const QuatT& animSceneRoot, FragmentID fragId, TagState tagState, CPlayer& master, const ParamFlags masterParams )
	: TBaseAction( PP_PlayerActionUrgent, fragId, tagState )
	, m_animSceneRoot(animSceneRoot)
	, m_updatedSceneRoot(animSceneRoot)
	, m_master(master,0,0,NULL,masterParams)
	, m_dirtyUpdatedSceneRoot(0)
{
}

CActionMultiCoopAnimation::~CActionMultiCoopAnimation()
{
}

void CActionMultiCoopAnimation::AddSlave( SParticipant* pSlave )
{
	CRY_ASSERT_MESSAGE(GetStatus()==None,"Slaves must be added before queueing the action!");
	m_slaves.push_back(pSlave);
	pSlave->OnAddedAsSlave();
}

void CActionMultiCoopAnimation::UpdateSceneRoot( const QuatT& sceneRoot )
{
	const QuatT currSceneRoot(m_updatedSceneRoot);
	if(currSceneRoot.t!=sceneRoot.t || currSceneRoot.q.v != sceneRoot.q.v || currSceneRoot.q.w != sceneRoot.q.w)
	{
		m_updatedSceneRoot = sceneRoot;
		m_dirtyUpdatedSceneRoot=1;
	}
}

void CActionMultiCoopAnimation::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	if(event.event==ENTITY_EVENT_DONE)
	{
		const EntityId entityId = pEntity->GetId();
		if(m_master.m_entityId==entityId)
		{
			m_master.m_entityValid = 0;
		}
		else
		{
			TSlaveList::const_iterator end = m_slaves.end();
			for(TSlaveList::iterator i = m_slaves.begin(); i!=end; ++i)
			{
				SParticipant& slave = (**i);
				if(slave.m_entityId==entityId)
				{
					slave.m_entityValid = 0;
					break;
				}
			}
		}
		ForceFinish();
	}
}

IAction::EStatus CActionMultiCoopAnimation::Update( float timePassed )
{
	MCALogAlways("Update()");

	if(m_dirtyUpdatedSceneRoot)
	{
		const QuatT delta(!m_animSceneRoot.q*m_updatedSceneRoot.q,m_updatedSceneRoot.t-m_animSceneRoot.t);
		m_master.ApplyDeltaTransform(delta);
		if (m_slaves.empty()==false)
		{
			TSlaveList::const_iterator end = m_slaves.end();
			for(TSlaveList::iterator i = m_slaves.begin(); i!=end; ++i)
			{
				(*i)->ApplyDeltaTransform(delta);
			}
		}
		m_animSceneRoot = m_updatedSceneRoot;
		m_dirtyUpdatedSceneRoot = 0;
	}
	return TBaseAction::Update(timePassed);
}

void CActionMultiCoopAnimation::Install()
{
	MCALogAlways("Install()");

	TBaseAction::Install();

	m_master.Install();
	CRY_ASSERT_MESSAGE(m_master.GetActionController(), "Master must have ActionController.");

	if (m_slaves.empty()==false)
	{
		TSlaveList::const_iterator end = m_slaves.end();
		for(TSlaveList::iterator i = m_slaves.begin(); i!=end; ++i)
		{
			SParticipant& slave = (**i);
			slave.Install();
			m_master.Enslave(slave);
		}
	}
}

void CActionMultiCoopAnimation::Enter()
{
	MCALogAlways("Enter()");

	SetParam("TargetPos", m_animSceneRoot);
	
	CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
	if (pRecordingSystem)
	{
		pRecordingSystem->OnMannequinSetParam(m_rootScope->GetEntityId(), "TargetPos", m_animSceneRoot);
	}

	m_master.Enter();
	RegisterForEntityEvents(m_master.m_entityId);
	if (m_slaves.empty()==false)
	{
		TSlaveList::const_iterator end = m_slaves.end();
		for(TSlaveList::iterator i = m_slaves.begin(); i!=end; ++i)
		{
			SParticipant& slave = *(*i);
			slave.Enter();
			RegisterForEntityEvents(slave.m_entityId);
		}
	}

	TBaseAction::Enter();
}

void CActionMultiCoopAnimation::Fail(EActionFailure failure)
{
	MCALogAlways("Fail()");

	if (m_eStatus == Installed)
	{
		Exit();
	}

	TBaseAction::Fail(failure);
}

void CActionMultiCoopAnimation::Exit()
{
	MCALogAlways("Exit()");

	TBaseAction::Exit();

	m_master.GetAndValidateEntity();
	if (m_slaves.empty()==false)
	{
		TSlaveList::const_iterator end = m_slaves.end();
		for(TSlaveList::const_iterator i = m_slaves.begin(); i!=end; ++i)
		{
			SParticipant& slave = *(*i);
			slave.Exit();
			UnregisterForEntityEvents(slave.m_entityId);
		}
	}
	m_master.Exit();
	UnregisterForEntityEvents(m_master.m_entityId);
}

void CActionMultiCoopAnimation::RegisterForEntityEvents( const EntityId entityId )
{
	if(IEntitySystem* pEntitySystem = gEnv->pEntitySystem)
	{
		pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
	}
}

void CActionMultiCoopAnimation::UnregisterForEntityEvents( const EntityId entityId )
{
	if(IEntitySystem* pEntitySystem = gEnv->pEntitySystem)
	{
		pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
	}
}

//////////////////////////////////////////////////////////////////////////

CActionMultiCoopAnimation::SParticipant::SParticipant( IAnimatedCharacter& animChar, ICharacterInstance* pCharInst, TagID tagId, uint32 contextId, const IAnimationDatabase* pAnimDb, const ParamFlags params )
	: m_entityId(animChar.GetEntityId())
	, m_animChar(animChar)
	, m_pCharInst(pCharInst)
	, m_pAnimDb(pAnimDb)
	, m_pActionController(NULL)
	, m_pMaster(NULL)
	, m_tagId(tagId)
	, m_contextId(contextId)
	, m_paramFlags(params)
	, m_entityValid(1)
{
}

void CActionMultiCoopAnimation::SParticipant::Install()
{
	m_pActionController = m_animChar.GetActionController();
}

void CActionMultiCoopAnimation::SParticipant::Enslave(SParticipant& slave)
{
	if(slave.m_pActionController)
	{
		// Mannequin Slave
		MCALogAlways("Enslave: Master[%d] Slave[%d] SetSlaveController(true)", m_entityId, slave.m_entityId);
		m_pActionController->SetSlaveController(*slave.m_pActionController, slave.m_contextId, true, slave.m_pAnimDb);

		if(CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem())
		{
			pRecordingSystem->OnMannequinSetSlaveController(m_entityId, slave.m_entityId, slave.m_contextId, true, slave.m_pAnimDb);
		}
	}
	else
	{
		// Non-Mannequin Slave
		if(IEntity* pSlaveEntity = slave.GetAndValidateEntity())
		{
			if(IAnimationGraphState* pAGState = slave.m_animChar.GetAnimationGraphState())
			{
				pAGState->Pause(true, eAGP_PlayAnimationNode);
			}
			MCALogAlways("Enslave: Master[%d] Slave[%d] NonMann - SetScopeContext()", m_entityId, slave.m_entityId);
			m_pActionController->SetScopeContext(slave.m_contextId, *pSlaveEntity, slave.m_pCharInst, slave.m_pAnimDb);
		}
	}

	m_pActionController->GetContext().state.Set(slave.m_tagId, true);

	slave.m_pMaster = this;
}

void CActionMultiCoopAnimation::SParticipant::Enter()
{
	if(m_pMaster)
	{
		m_pMaster->OnSlaveEnter(*this);
	}

	if(IEntity* pEntity = GetAndValidateEntity())
	{
		if(HasFlag(ePF_AnimCtrldMovement))
		{
			m_animChar.SetMovementControlMethods(eMCM_Animation, eMCM_Animation);
		}
		if(HasFlag(ePF_DisablePhysics))
		{
			m_animChar.ForceRefreshPhysicalColliderMode();
			m_animChar.RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CActionMultiCoopAnimation::SParticipant::Enter()");
		}
		if(HasFlag(ePF_RenderNearest))
		{
			SetRenderNearest(*pEntity,true);
		}
	}
}

void CActionMultiCoopAnimation::SParticipant::Exit()
{
	if(IEntity* pEntity = GetAndValidateEntity())
	{
		if(HasFlag(ePF_AnimCtrldMovement))
		{
			m_animChar.SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
		}
		if(HasFlag(ePF_DisablePhysics))
		{
			m_animChar.ForceRefreshPhysicalColliderMode();
			m_animChar.RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CActionMultiCoopAnimation::SParticipant::Exit()");
		}
		if(HasFlag(ePF_RenderNearest))
		{
			SetRenderNearest(*pEntity,false);
		}
	}

	if(m_pMaster)
	{
		m_pMaster->OnSlaveExit(*this);
	}

	m_pMaster = NULL;
}

void CActionMultiCoopAnimation::SParticipant::OnSlaveEnter( SParticipant& slave )
{
}

void CActionMultiCoopAnimation::SParticipant::OnSlaveExit( SParticipant& slave )
{
	if( slave.m_pActionController )
	{
		if( slave.m_entityValid )
		{
			MCALogAlways("SlaveExit: Master[%d] Slave[%d] Slave Valid - SetSlaveController(false)", m_entityId, slave.m_entityId);
			m_pActionController->SetSlaveController(*slave.m_pActionController, slave.m_contextId, false, slave.m_pAnimDb);
		}
		else
		{
			MCALogAlways("SlaveExit: Master[%d] Slave[%d] Slave Invalid - ClearScopeContext()", m_entityId, slave.m_entityId);
			m_pActionController->ClearScopeContext(slave.m_contextId);
		}

		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnMannequinSetSlaveController(m_entityId, slave.m_entityId, slave.m_contextId, false, slave.m_pAnimDb);
		}
	}
	else
	{
		MCALogAlways("SlaveExit: Master[%d] Slave[%d] Slave NoActionController - ClearScopeContext()", m_entityId, slave.m_entityId);
		m_pActionController->ClearScopeContext(slave.m_contextId);

		if(slave.m_entityValid)
		{
			if(IAnimationGraphState* pAGState = slave.m_animChar.GetAnimationGraphState())
			{
				pAGState->Pause(false, eAGP_PlayAnimationNode);
			}
		}
	}

	m_pActionController->GetContext().state.Set(slave.m_tagId, false);

	slave.OnRemovedAsSlave();
}

void CActionMultiCoopAnimation::SParticipant::ApplyDeltaTransform( const QuatT& delta )
{
	if(HasFlag(ePF_ApplyDeltas) && GetAndValidateEntity())
	{
		m_animChar.ForceMovement(delta);
	}
}

void CActionMultiCoopAnimation::SParticipant::SetRenderNearest( IEntity& entity, const bool bSet )
{
	bool bRendering = false;
	{
		if(IRenderNode* pRenderNode = entity.GetRenderNode())
		{
			const int nslots = entity.GetSlotCount();
			uint32 orFlags = 0;
			uint32 andFlags = ~0;
			if(bSet)
				orFlags = ENTITY_SLOT_RENDER_NEAREST;
			else
				andFlags = (~ENTITY_SLOT_RENDER_NEAREST);

			for(int i=0;i<nslots;i++)
			{
				const uint32 slotFlags = entity.GetSlotFlags(i);
				if(slotFlags&ENTITY_SLOT_RENDER)
				{
					entity.SetSlotFlags(i, (slotFlags|orFlags)&andFlags);
					bRendering = true;
				}
			}
			if(bRendering)
			{
				pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION, bSet);
			}
		}
	}
}


IEntity* CActionMultiCoopAnimation::SParticipant::GetAndValidateEntity()
{
	if(m_entityValid)
	{
		if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
		{
			if(!pEntity->IsGarbage())
			{
				return pEntity;
			}
		}
		MCALogAlways("Entity invalidated: %d", m_entityId);
	}
	m_entityValid = 0;
	return NULL;
}

void CActionMultiCoopAnimation::SParticipant::OnAddedAsSlave()
{
	// ICharInstances are liable to be deleted while Slaves without notifying anything.
	// This will make sure the char inst is still valid until we are finished with it, even if it is no longer drawn or has an entity.
	if(m_pCharInst)
	{
		m_pCharInst->AddRef();
	}
}

void CActionMultiCoopAnimation::SParticipant::OnRemovedAsSlave()
{
	if(m_pCharInst)
	{
		m_pCharInst->Release();
	}
}

//////////////////////////////////////////////////////////////////////////

CActionMultiCoopAnimation::SPlayerParticipant::SPlayerParticipant( CPlayer &player, TagID tagId, uint32 contextId, const IAnimationDatabase* pAnimDb, const ParamFlags params ) 
	: SParticipant(*player.GetAnimatedCharacter(), player.GetEntity()->GetCharacter(0), tagId, contextId, pAnimDb, params)
	, m_player(player)
{
}

void CActionMultiCoopAnimation::SPlayerParticipant::Enter()
{
	SParticipant::Enter();

	if(HasFlag(ePF_AnimControlledCam))
	{
		PlayerCameraAnimationSettings cameraAnimationSettings;
		cameraAnimationSettings.positionFactor = 1.0f;
		cameraAnimationSettings.rotationFactor = 1.0f;
		cameraAnimationSettings.applyResult = true;
		m_player.PartialAnimationControlled( true, cameraAnimationSettings );
	}
	if(HasFlag(ePF_HolsterItem))
	{
		m_player.HolsterItem(true);
	}
}

void CActionMultiCoopAnimation::SPlayerParticipant::Exit()
{
	SParticipant::Exit();

	if(m_entityValid)
	{
		if(HasFlag(ePF_AnimControlledCam))
		{
			PlayerCameraAnimationSettings cameraAnimationSettings;
			cameraAnimationSettings.positionFactor = 0.0f;
			cameraAnimationSettings.rotationFactor = 0.0f;
			m_player.PartialAnimationControlled( false, PlayerCameraAnimationSettings() );
		}
		if(HasFlag(ePF_UnholsterItemOnExit))
		{
			m_player.HolsterItem(false);
		}
	}
}

void CActionMultiCoopAnimation::SPlayerParticipant::OnSlaveEnter( SParticipant& slave )
{
	SParticipant::OnSlaveEnter(slave);

	m_player.GetActorStats()->animationControlledID = slave.GetAnimatedCharacter().GetEntityId();
}

void CActionMultiCoopAnimation::SPlayerParticipant::OnSlaveExit( SParticipant& slave )
{
	SParticipant::OnSlaveExit(slave);

	if(m_entityValid)
	{
		m_player.GetActorStats()->animationControlledID = 0;
	}
	//player.StateMachineHandleEventMovement( SStateEventMultiCoopAnim(slave.m_actor.GetEntityId()) );
}

void CActionMultiCoopAnimation::SPlayerParticipant::OnAddedAsSlave()
{
	SParticipant::OnAddedAsSlave();
	m_player.GetActorStats()->isAnimatedSlave++;
}

void CActionMultiCoopAnimation::SPlayerParticipant::OnRemovedAsSlave()
{
	SParticipant::OnRemovedAsSlave();
	if(m_entityValid)
	{
		m_player.GetActorStats()->isAnimatedSlave--;
	}
}

