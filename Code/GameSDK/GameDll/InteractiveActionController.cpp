// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Controls player when playing an interactive animation

-------------------------------------------------------------------------
History:
- 19:12:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "InteractiveActionController.h"

#include "Item.h"
#include "Player.h"

#include "Environment/InteractiveObjectRegistry.h"

#include "EntityUtility/EntityScriptCalls.h"
#include "RecordingSystem.h"

#define INTERACTION_DEFAULT_CORRECTION_DELAY 0.4f

namespace 
{
	const float ACTION_INSTALL_FAIL_TIME = 1.0f;
}

#include "PlayerAnimation.h"
class CActionInteractive : public TPlayerAction
{
public:

	DEFINE_ACTION("InteractiveAction");

	typedef TAction<SAnimationContext> BaseClass;

	CActionInteractive(CPlayer &player, EntityId interactiveObjectId, FragmentID fragID, const TagState &fragTags, uint32 optionIdx, CInteractiveActionController &IAController, const QuatT &targetLocation, const TagID tagId)
		: TPlayerAction(PP_PlayerActionUrgent, fragID, fragTags)
		, m_player(player)		
		, m_interactiveObjectId(interactiveObjectId)
		, m_targetLocation(targetLocation)
		, m_tagId(tagId)
		, m_overrideTarget(false)
		, m_pIAController(&IAController)
	{
		m_optionIdx = optionIdx;
		m_target = (interactiveObjectId) ? gEnv->pEntitySystem->GetEntity(m_interactiveObjectId) : nullptr;
	}

	virtual void Install() override
	{
		TPlayerAction::Install();

		if (m_target)
		{
			ICharacterInstance *targetChar = m_target->GetCharacter(0);
			if (targetChar)
			{
				IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
				const IAnimationDatabase *animDB = mannequinSys.GetAnimationDatabaseManager().Load("Animations/Mannequin/ADB/interactiveObjects.adb");
				m_rootScope->GetActionController().SetScopeContext(PlayerMannequin.contextIDs.SlaveObject, *m_target, targetChar, animDB);

				CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
				if (pRecordingSystem)
				{
					pRecordingSystem->OnMannequinSetSlaveController(m_rootScope->GetEntityId(), m_interactiveObjectId, PlayerMannequin.contextIDs.SlaveChar, true, animDB);
				}
			}
		}

		if(m_pIAController)
		{
			m_pIAController->MannequinActionInstalled();
		}
	}

	virtual void Enter() override
	{
		TPlayerAction::Enter();

		if( m_target )
		{
			SetParam( "TargetPos", m_targetLocation );
		}

		IAnimatedCharacter* pAnimChar				= m_player.GetAnimatedCharacter();

		if( pAnimChar )
		{
			pAnimChar->SetMovementControlMethods(eMCM_Animation, eMCM_Animation);
			pAnimChar->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CActionInteractive::Enter()");
			pAnimChar->EnableRigidCollider(0.45f);
		}

		PlayerCameraAnimationSettings cameraAnimationSettings;
		cameraAnimationSettings.positionFactor = 1.0f;
		cameraAnimationSettings.rotationFactor = 1.0f;
		m_player.PartialAnimationControlled( true, cameraAnimationSettings );
		m_player.HolsterItem(true);
	}

	virtual void Exit() override
	{
		TPlayerAction::Exit();

		IAnimatedCharacter* pAnimChar = m_player.GetAnimatedCharacter();

		if( pAnimChar )
		{
			pAnimChar->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
			pAnimChar->ForceRefreshPhysicalColliderMode();
			pAnimChar->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CActionInteractive::Exit()");
			pAnimChar->DisableRigidCollider();
		}

		m_rootScope->GetActionController().ClearScopeContext(PlayerMannequin.contextIDs.SlaveObject, false);

		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem && m_target)
		{
			pRecordingSystem->OnMannequinSetSlaveController(m_rootScope->GetEntityId(), m_target->GetId(), PlayerMannequin.contextIDs.SlaveChar, false, NULL);
			pRecordingSystem->OnInteractiveObjectFinishedUse(m_interactiveObjectId, m_tagId, m_optionIdx);
		}

		m_player.PartialAnimationControlled( false, PlayerCameraAnimationSettings() );
		m_player.HolsterItem(false);

		if(m_pIAController)
		{
			m_pIAController->NotifyFinished(m_player);
		}
	}

	ILINE void ClearBackPointer() { m_pIAController = NULL; }

private:
	CPlayer	&m_player;
	IEntity *m_target;
	EntityId m_interactiveObjectId;
	const QuatT m_targetLocation;
	const TagID m_tagId;
	bool m_overrideTarget;

	CInteractiveActionController* m_pIAController;
};

//////////////////////////////////////////////////////////////////////////

CInteractiveActionController::~CInteractiveActionController()
{
	SafelyReleaseAction();
}

void CInteractiveActionController::OnEnter( CPlayer& player, EntityId interactiveObjectId, int interactionIndex, float actionSpeed /*= 1.0f*/ )
{
	m_interactiveObjectId = interactiveObjectId;
	m_state = eState_PositioningUser;
	m_runningTime	= 0.0f;

	// ensure the player data is in sync
	player.SetLastInteractionIndex(interactionIndex); 

	CalculateStartTargetLocation(player, interactionIndex);

	if (IActionController *pActionController = player.GetAnimatedCharacter()->GetActionController())
	{
		if (m_interactiveObjectId && PlayerMannequin.fragmentIDs.interact.IsValid())
		{
			const CInteractiveObjectRegistry::SMannequinParams interactionMannParams = g_pGame->GetInteractiveObjectsRegistry().GetInteractionTagForEntity(m_interactiveObjectId, interactionIndex);

			const CTagDefinition *pTagDef = pActionController->GetContext().controllerDef.GetFragmentTagDef(PlayerMannequin.fragmentIDs.interact);
			TagState fragmentTags(TAG_STATE_EMPTY);
			if (pTagDef && (interactionMannParams.interactionTagID != TAG_ID_INVALID))
			{
				pTagDef->Set(fragmentTags, interactionMannParams.interactionTagID, true);
				if(interactionMannParams.stateTagID != TAG_ID_INVALID)
				{
					pTagDef->Set(fragmentTags, interactionMannParams.stateTagID, true);
				}
			}
			m_state = eState_InstallPending;

			SafelyReleaseAction();
			m_pAction = new CActionInteractive(player, m_interactiveObjectId, PlayerMannequin.fragmentIDs.interact, fragmentTags, interactionIndex, *this, m_targetLocation, interactionMannParams.interactionTagID);
			m_pAction->AddRef();
			m_pAction->SetSpeedBias(actionSpeed);
			pActionController->Queue(*m_pAction);
		}
		else if ((m_interactionName.empty() == false) && PlayerMannequin.fragmentIDs.animationControlled.IsValid())
		{
			const CTagDefinition *pTagDef = pActionController->GetContext().controllerDef.GetFragmentTagDef(PlayerMannequin.fragmentIDs.animationControlled);
			if ( pTagDef != NULL )
			{
				const TagID desiredTag = pTagDef->Find( m_interactionName.c_str() );
				TagState fragmentTags(TAG_STATE_EMPTY);
				if (desiredTag != TAG_ID_INVALID)
				{
					pTagDef->Set( fragmentTags, desiredTag, true );
				}
				m_state = eState_InstallPending;

				SafelyReleaseAction();
				m_pAction = new CActionInteractive(player, 0, PlayerMannequin.fragmentIDs.animationControlled, fragmentTags, 0, *this, QuatT(IDENTITY), desiredTag);
				m_pAction->AddRef();
				m_pAction->SetSpeedBias(actionSpeed);
				pActionController->Queue(*m_pAction);
			}
		}

		if( m_state != eState_InstallPending )
		{
			Abort( player );
		}
	}
	else
	{
		Abort( player );
	}

	player.LockInteractor(interactiveObjectId, true);
}

void CInteractiveActionController::OnEnterByName( CPlayer& player, const char* interaction, float actionSpeed /*= 1.0f*/ )
{
	m_interactionName = interaction;

	OnEnter( player, 0, 0, actionSpeed );
}

void CInteractiveActionController::OnLeave( CPlayer& player )
{
	player.LockInteractor(m_interactiveObjectId, false);

	m_interactionName = "";
	m_interactiveObjectId = 0;
	m_state = eState_None;
	m_runningTime	= 0.0f;

	player.HolsterItem(false);
}

void CInteractiveActionController::Update( CPlayer& player, float frameTime,  const SActorFrameMovementParams& movement )
{
	CRY_ASSERT_MESSAGE(m_state != eState_None, "Non valid state!");

	m_runningTime += frameTime;

	// lock the player rotation during playback
	player.SetViewRotation(player.GetEntity()->GetWorldRotation());

	if( m_state == eState_InstallPending && m_runningTime > ACTION_INSTALL_FAIL_TIME )
	{
		NotifyFinished(player);
	}
}

void CInteractiveActionController::NotifyFinished(CPlayer& player)
{
	m_state = eState_Done;

	if (m_interactiveObjectId)
	{
		IEntity* pInteractiveObject = gEnv->pEntitySystem->GetEntity(m_interactiveObjectId);
		if (pInteractiveObject)
		{
			EntityScripts::CallScriptFunction(pInteractiveObject, pInteractiveObject->GetScriptTable(), "StopUse", ScriptHandle(player.GetEntityId()));
		}
	}

	player.EndInteractiveAction(m_interactiveObjectId);
}

void CInteractiveActionController::CalculateStartTargetLocation(CPlayer& player, int interactionIndex)
{
	m_targetLocation.t = player.GetEntity()->GetWorldPos();
	m_targetLocation.q = player.GetEntity()->GetWorldRotation();
}

void CInteractiveActionController::FullSerialize( CPlayer& player, TSerialize &ser )
{
	string serializationString = m_interactionName.c_str();
	ser.Value("InteractionName", serializationString);
	ser.Value("location", m_targetLocation.t);
	ser.Value("orientation", m_targetLocation.q);

	EntityId playerId = 0;
	playerId = player.GetEntityId();

	ser.Value("targetPlayerId", playerId);

	ser.Value("m_interactiveObjectId", m_interactiveObjectId);
	ser.Value("m_runningTime", m_runningTime);
	ser.EnumValue("eState", m_state, eState_None, eState_Done);

	if(ser.IsReading())
	{
		m_interactionName = serializationString.c_str();
	}
}

void CInteractiveActionController::Abort(CPlayer& player)
{
	CRY_ASSERT(IsInInteractiveAction());

	switch (m_state)
	{
		case eState_PositioningUser:
		{
			break;
		}
		case eState_PlayingAnimation:
		{
			if (m_interactiveObjectId)
			{
				IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(m_interactiveObjectId);
				ICharacterInstance* pObjectCharacter = pObjectEntity ? pObjectEntity->GetCharacter(0) : NULL;
				if (pObjectCharacter)
				{
					pObjectCharacter->GetISkeletonAnim()->StopAnimationInLayer(0, 0.01f);
				}
			}
			break;
		}
	}

	if (m_interactiveObjectId)
	{
		IEntity* pInteractiveObject = gEnv->pEntitySystem->GetEntity(m_interactiveObjectId);
		if (pInteractiveObject)
		{
			EntityScripts::CallScriptFunction(pInteractiveObject, pInteractiveObject->GetScriptTable(), "AbortUse", ScriptHandle(player.GetEntityId()));
		}
	}

	m_state = eState_Done;

	player.EndInteractiveAction(m_interactiveObjectId);
}

void CInteractiveActionController::SafelyReleaseAction()
{
	if(m_pAction)
	{
		m_pAction->ClearBackPointer();
	}
	SAFE_RELEASE(m_pAction);
}

