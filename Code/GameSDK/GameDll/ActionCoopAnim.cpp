// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Implements Coop Anim.

-------------------------------------------------------------------------
History:
- 21:11:2011: Shamelessly stolen from TomBs StealthKill Action.

*************************************************************************/
#include "StdAfx.h"

#include "ActionCoopAnim.h"

#include "Player.h"
#include "RecordingSystem.h"
#include "PlayerStateEvents.h"

CActionCoopAnimation::CActionCoopAnimation(const SActionCoopParams& params)
	: TPlayerAction(PP_PlayerActionUrgent, params.m_fragID, params.m_tagState),
	m_player(params.m_player),
	m_target(params.m_target),
	m_targetEntityID(params.m_target.GetEntityId()),
	m_targetTagID(params.m_targetTagID),
	m_piOptionalTargetDatabase(params.m_piOptionalTargetDatabase)
{
	if(gEnv->bMultiplayer)
	{
		gEnv->pEntitySystem->AddEntityEventListener(m_targetEntityID, ENTITY_EVENT_DONE, this);
	}
}

CActionCoopAnimation::~CActionCoopAnimation()
{
	if(m_targetEntityID)
	{
		gEnv->pEntitySystem->RemoveEntityEventListener(m_targetEntityID, ENTITY_EVENT_DONE, this);
	}
}

void CActionCoopAnimation::Install()
{
	TPlayerAction::Install();

	IActionController* pTargetActionController = m_target.GetAnimatedCharacter()->GetActionController();
	if(pTargetActionController)
	{
		m_player.GetAnimatedCharacter()->GetActionController()->SetSlaveController(*pTargetActionController, PlayerMannequin.contextIDs.SlaveChar, true, m_piOptionalTargetDatabase);
		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnMannequinSetSlaveController(m_rootScope->GetEntityId(), m_target.GetEntityId(), PlayerMannequin.contextIDs.SlaveChar, true, m_piOptionalTargetDatabase);
		}
	}
	else
	{
		IEntity* pTargetEnt = m_target.GetEntity();
		if (pTargetEnt)
		{
			IEntity& targetEnt = *pTargetEnt;

			m_player.GetAnimatedCharacter()->GetActionController()->SetScopeContext(PlayerMannequin.contextIDs.SlaveChar, targetEnt, targetEnt.GetCharacter(0), m_piOptionalTargetDatabase);
		}
	}

	m_player.GetAnimatedCharacter()->GetActionController()->GetContext().state.Set(m_targetTagID, true);
}

void CActionCoopAnimation::Enter()
{
	TPlayerAction::Enter();

	QuatT targetPos(m_rootScope->GetEntity().GetPos(), m_rootScope->GetEntity().GetRotation());

	SetParam("TargetPos", targetPos);
	CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
	if (pRecordingSystem)
	{
		pRecordingSystem->OnMannequinSetParam(m_rootScope->GetEntityId(), "TargetPos", targetPos);
	}

	IAnimatedCharacter* pAnimChar				= m_player.GetAnimatedCharacter();
	IAnimatedCharacter* pAnimCharTarget = m_target.GetAnimatedCharacter();

	if( pAnimChar )
	{
		pAnimChar->SetMovementControlMethods(eMCM_Animation, eMCM_Animation);
		pAnimChar->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CActionStealthKill::Enter()");
	}
	if (pAnimCharTarget)
	{
		pAnimCharTarget->SetMovementControlMethods(eMCM_Animation, eMCM_Animation);
		pAnimCharTarget->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CActionStealthKill::Enter()");
	}

	PlayerCameraAnimationSettings cameraAnimationSettings;
	cameraAnimationSettings.positionFactor = 1.0f;
	cameraAnimationSettings.rotationFactor = 1.0f;
	cameraAnimationSettings.stableBlendOff = true;
	m_player.PartialAnimationControlled( true, cameraAnimationSettings );
	m_player.GetActorStats()->animationControlledID = m_target.GetEntityId();
	m_player.HolsterItem(true);

	// Update visibility to change render mode of 1st person character
	m_player.RefreshVisibilityState();

	// Mannequin can't set the tag's correctly on the exit, so we have to do it immediately after we started instead :)
	// the tags are set, but Exit() is called too late for our purposes, it is needed during the resolve of the next action
	m_rootScope->GetActionController().GetContext().state.Set(m_targetTagID, false);
}

void CActionCoopAnimation::Exit()
{
	SendStateEventCoopAnim();

	TPlayerAction::Exit();

	IAnimatedCharacter* pAnimChar = m_player.GetAnimatedCharacter();
	IAnimatedCharacter* pAnimCharTarget = m_targetEntityID ? m_target.GetAnimatedCharacter() : NULL;

	if( pAnimChar )
	{
		pAnimChar->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
		pAnimChar->ForceRefreshPhysicalColliderMode();
		pAnimChar->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CActionStealthKill::Exit()");
	}
	if (pAnimCharTarget)
	{
		pAnimCharTarget->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
		pAnimCharTarget->ForceRefreshPhysicalColliderMode();
		pAnimCharTarget->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CActionStealthKill::Exit()");
	}

	RemoveTargetFromSlaveContext();

	m_player.PartialAnimationControlled( false, PlayerCameraAnimationSettings());
	m_player.GetActorStats()->animationControlledID = 0;
	m_player.HolsterItem(false);

	// Update visibility to change render mode of 1st person character
	m_player.RefreshVisibilityState();
}

void CActionCoopAnimation::RemoveTargetFromSlaveContext()
{
	IAnimatedCharacter* pTargetAnimChar = m_targetEntityID ? m_target.GetAnimatedCharacter() : NULL;
	IActionController* pTargetActionController = pTargetAnimChar ? pTargetAnimChar->GetActionController() : NULL;

	if(pTargetActionController)
	{
		IAnimatedCharacter* pAnimChar = m_player.GetAnimatedCharacter();
		IActionController* pActionController = pAnimChar->GetActionController();
		pActionController->SetSlaveController(*pTargetActionController, PlayerMannequin.contextIDs.SlaveChar, false, m_piOptionalTargetDatabase);

		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnMannequinSetSlaveController(m_rootScope->GetEntityId(), pTargetActionController->GetEntityId(), PlayerMannequin.contextIDs.SlaveChar, false, m_piOptionalTargetDatabase);
		}
	}
	else
	{
		m_rootScope->GetActionController().ClearScopeContext(PlayerMannequin.contextIDs.SlaveChar);
	}
}

void CActionCoopAnimation::SendStateEventCoopAnim()
{
	m_player.StateMachineHandleEventMovement( SStateEventCoopAnim(m_targetEntityID) );
}

void CActionCoopAnimation::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	if(!pEntity)
		return;

	const EntityId entityId = pEntity->GetId();
	if(entityId == m_targetEntityID)
	{
		// Cleaning the slave context currently /immediately/ flush scopes using
		// it, which means WE will get flushed too. In that case we will get a
		// Release() call. By adding a reference to ourselves we prevent ourselves
		// from being deleted in flight.
		AddRef();

		if (m_eStatus == Pending)
		{
			// In the unlikely event we didn't even start yet, make sure to send the
			// event that tells the rest of the code we finished
			SendStateEventCoopAnim();

			ForceFinish();
		}
		else if (m_eStatus == Installed)
		{
			// Note: Cleaning the slave context will cause Exit() to be called on ourselves
			RemoveTargetFromSlaveContext();
		}

		m_targetEntityID = 0;

		// Release ourselves after we cleaned up (this might cause ourselves to be deleted)
		Release();
	}
}
