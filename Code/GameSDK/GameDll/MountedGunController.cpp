// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Controls player movement when using a mounted gun

-------------------------------------------------------------------------
History:
- 2:10:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "MountedGunController.h"

#include "Player.h"
#include "Weapon.h"
#include "Battlechatter.h"
#include "GameRules.h"
#include "RecordingSystem.h"

#include "PlayerAnimation.h"
#include "IVehicleSystem.h"

#define REPLAY_PLAYER_ANIMATION_LAYER_BASE			 0
#define REPLAY_PLAYER_ANIMATION_LAYER_AIM_UP		 3
#define REPLAY_PLAYER_ANIMATION_LAYER_AIM_DOWN	 4

#define MAN_MOUNTEDGUN_FRAGMENTS( f ) \
	f( MotionMounted )

#define MAN_MOUNTEDGUN_TAGS( f )

MANNEQUIN_USER_PARAMS( SMannequinMountedGunParams, MAN_MOUNTEDGUN_FRAGMENTS, MAN_MOUNTEDGUN_TAGS, MANNEQUIN_USER_PARAMS__EMPTY_LIST, MANNEQUIN_USER_PARAMS__EMPTY_LIST, MANNEQUIN_USER_PARAMS__EMPTY_LIST, MANNEQUIN_USER_PARAMS__EMPTY_LIST );


#define MountedGunCrcList( f ) \
	f( aimUpParam ) \
	f( aimDownParam ) \
	f( aimMovementParam )

MANNEQUIN_AUTO_CRC( SMountedGunCRCs, MountedGunCrcList );
SMountedGunCRCs MountedGunCRCs;


void CMountedGunController::Update(EntityId mountedGunID, float frameTime)
{
	CRY_ASSERT_MESSAGE(m_pControlledPlayer, "Controlled player not initialized");

	// Animation state needs to be updated when the player switched between first and third person view
	if (m_PreviousThirdPersonState != m_pControlledPlayer->IsThirdPerson())
	{
		m_PreviousThirdPersonState = m_pControlledPlayer->IsThirdPerson();

		OnLeave();
		OnEnter(mountedGunID);
	}

	CItem* pMountedGun = static_cast<CItem*>(gEnv->pGameFramework->GetIItemSystem()->GetItem(mountedGunID));

	bool canUpdateMountedGun = (pMountedGun != NULL) && (pMountedGun->GetStats().mounted);

	if (canUpdateMountedGun)
	{
		IMovementController * pMovementController = m_pControlledPlayer->GetMovementController();
		assert(pMovementController);

		SMovementState info;
		pMovementController->GetMovementState(info);

		IEntity* pMountedGunEntity = pMountedGun->GetEntity();
		const Matrix34& lastMountedGunWorldTM = pMountedGunEntity->GetWorldTM();

		Vec3 desiredAimDirection = info.aimDirection.GetNormalized();

		// AI can switch directions too fast, prevent snapping
		if(!m_pControlledPlayer->IsPlayer())
		{
			const Vec3 currentDir = lastMountedGunWorldTM.GetColumn1();
			const float dot = currentDir.Dot(desiredAimDirection);
			const float reqAngle = acos_tpl(dot);
			const float maxRotSpeed = 2.0f;
			const float maxAngle = frameTime * maxRotSpeed;
			if(fabs(reqAngle) > maxAngle)
			{
				const Vec3 axis = currentDir.Cross(desiredAimDirection);
				if(axis.GetLengthSquared() > 0.001f) // current dir and new dir are enough different
				{
					desiredAimDirection = currentDir.GetRotated(axis.GetNormalized(),sgn(reqAngle)*maxAngle);
				}
			}
		}				
		
		bool isUserClient = m_pControlledPlayer->IsClient();
		
		IEntity* pMountedGunParentEntity = pMountedGunEntity->GetParent();
		IVehicle *pVehicle = NULL;
		if(pMountedGunParentEntity && m_pControlledPlayer)
			pVehicle = m_pControlledPlayer->GetLinkedVehicle();

		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();

		//For client update always, for others only when there is notable change
		if (!pVehicle && (isUserClient || (!desiredAimDirection.IsEquivalent(lastMountedGunWorldTM.GetColumn1(), 0.003f)))) 
		{
			Quat rotation = Quat::CreateRotationVDir(desiredAimDirection, 0.0f);
			pMountedGunEntity->SetRotation(rotation);

			if (isUserClient && pRecordingSystem)
			{
				// Only record the gun position if you're using the gun.
				pRecordingSystem->OnMountedGunRotate(pMountedGunEntity, rotation);
			}
		}

		const Vec3 vInitialAimDirection = GetMountDirection(pMountedGun, pMountedGunParentEntity);
		assert( vInitialAimDirection.IsUnit() );

		//Adjust gunner position and animations
		UpdateGunnerLocation(pMountedGun, pMountedGunParentEntity, vInitialAimDirection);

		const float aimrad = Ang3::CreateRadZ(Vec2(vInitialAimDirection),Vec2(-desiredAimDirection));
		const float pitchLimit = sin_tpl(DEG2RAD(30.0f));
		const float animHeight = fabs_tpl(clamp_tpl(desiredAimDirection.z * (float)__fres(pitchLimit), -1.0f, 1.0f));

		const float aimUp = (float)__fsel(-desiredAimDirection.z, 0.0f, animHeight); 
		const float aimDown = (float)__fsel(desiredAimDirection.z, 0.0f, animHeight);

		if (pRecordingSystem)
		{
			pRecordingSystem->OnMountedGunUpdate(m_pControlledPlayer, aimrad, aimUp, aimDown);
		}

		if(!m_pControlledPlayer->IsThirdPerson())
		{
			UpdateFirstPersonAnimations(pMountedGun, desiredAimDirection);
		}

		if(m_pMovementAction)
		{
			const float aimUpParam = aimUp;
			const float aimDownParam = aimDown;
			const float aimMovementParam = CalculateAnimationTime(aimrad);

			m_pMovementAction->SetParam(MountedGunCRCs.aimUpParam, aimUpParam);
			m_pMovementAction->SetParam(MountedGunCRCs.aimDownParam, aimDownParam);
			m_pMovementAction->SetParam(MountedGunCRCs.aimMovementParam, aimMovementParam);
		}

		UpdateIKMounted(pMountedGun);
	}
}


void CMountedGunController::OnEnter(EntityId mountedGunId)
{
	m_PreviousThirdPersonState = m_pControlledPlayer->IsThirdPerson();

	CRY_ASSERT_MESSAGE(m_pControlledPlayer, "Controlled player not initialized");

	ICharacterInstance* pCharacter = m_pControlledPlayer->IsThirdPerson() ? m_pControlledPlayer->GetEntity()->GetCharacter(0) : m_pControlledPlayer->GetShadowCharacter();
	if (pCharacter)
	{
		CItem* pWeapon = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(mountedGunId));
		assert(pWeapon);

		IActionController* pActionController = m_pControlledPlayer->GetAnimatedCharacter()->GetActionController();
		if (pActionController && m_pMannequinParams)
		{
			SAFE_RELEASE(m_pMovementAction);

			m_pMovementAction = new TPlayerAction(PP_PlayerAction, m_pMannequinParams->fragmentIDs.MotionMounted);
			m_pMovementAction->AddRef();
			pActionController->Queue(*m_pMovementAction);
		}

		//////////////////////////////////////////////////////////////////////////
		// NOTE: This should go through mannequin
		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			const SParams& pWeaponParams = pWeapon->GetParams();

			IAnimationSet* pAnimationSet = pCharacter->GetIAnimationSet();
			const int upAnimId = pAnimationSet->GetAnimIDByName(pWeaponParams.mountedTPAimAnims[MountedTPAimAnim::Up].c_str());
			const int downAnimId = pAnimationSet->GetAnimIDByName(pWeaponParams.mountedTPAimAnims[MountedTPAimAnim::Down].c_str());

			pRecordingSystem->OnMountedGunEnter(m_pControlledPlayer, upAnimId, downAnimId);
		}

		//////////////////////////////////////////////////////////////////////////

		IAnimatedCharacter* pAnimatedCharacter = m_pControlledPlayer->GetAnimatedCharacter();
		CRY_ASSERT(pAnimatedCharacter);

		pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CMountedGunController::OnEnter");
		pAnimatedCharacter->SetNoMovementOverride(true);

		if (gEnv->bMultiplayer)
			pAnimatedCharacter->EnableRigidCollider(0.5f);
		else if (m_pControlledPlayer->IsPlayer())
			pAnimatedCharacter->EnableRigidCollider(1.5f);

		pAnimatedCharacter->GetGroundAlignmentParams().SetFlag(eGA_AllowWithNoCollision, true);

		BATTLECHATTER(BC_WatchMyBack, m_pControlledPlayer->GetEntityId());
	}
}


void CMountedGunController::OnLeave( )
{
	CRY_ASSERT_MESSAGE(m_pControlledPlayer, "Controlled player not initialized");

	ICharacterInstance* pCharacter = m_pControlledPlayer->IsThirdPerson() ? m_pControlledPlayer->GetEntity()->GetCharacter(0) : m_pControlledPlayer->GetShadowCharacter();
	if (pCharacter)
	{
		if(m_pMovementAction)
		{
			m_pMovementAction->ForceFinish();
			SAFE_RELEASE(m_pMovementAction);
		}

		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnMountedGunLeave(m_pControlledPlayer);
		}

		IAnimatedCharacter* pAnimatedCharacter = m_pControlledPlayer->GetAnimatedCharacter();
		CRY_ASSERT(pAnimatedCharacter);

		pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
		pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CMountedGunController::OnLeave");
		pAnimatedCharacter->SetNoMovementOverride(false);

		if (gEnv->bMultiplayer || m_pControlledPlayer->IsPlayer())
			pAnimatedCharacter->DisableRigidCollider();

		pAnimatedCharacter->GetGroundAlignmentParams().SetFlag(eGA_AllowWithNoCollision, true);

		m_pControlledPlayer->GetActorParams().mountedWeaponCameraTarget.zero();
	}
}

void CMountedGunController::UpdateGunnerLocation( CItem* pMountedGun, IEntity* pParent, const Vec3& bodyDirection )
{
	const SMountParams* pMountParams = pMountedGun->GetMountedParams();

	if (pMountParams)
	{
		f32 bodyDist = pMountParams->body_distance;
		f32 groundDist = pMountParams->ground_distance;

		Matrix34 gunLocalTM  = pMountedGun->GetEntity()->GetLocalTM();

		Matrix34	gunLocalTMXY(IDENTITY);
		Matrix34 characterTM(IDENTITY);
		Matrix34 newGunnerTM;
		Vec3 playerOffset(0.0f, -bodyDist, -groundDist);
		characterTM.SetTranslation(playerOffset);

		IEntity* pControlledPlayerEntity = m_pControlledPlayer->GetEntity();
		IVehicle *pVehicle = NULL;

		if (gEnv->bMultiplayer && m_pControlledPlayer->IsClient() && m_pControlledPlayer->GetLinkedVehicle())
		{
			newGunnerTM = gunLocalTM;
			newGunnerTM.SetTranslation(gunLocalTM.GetTranslation() + Quat(gunLocalTM) * playerOffset);
		}
		else
		{
			float rotZ = pMountedGun->GetEntity()->GetRotation().GetRotZ();
			gunLocalTMXY.SetRotationZ(rotZ);
			gunLocalTMXY.SetTranslation(gunLocalTM.GetTranslation());
			newGunnerTM = gunLocalTMXY*characterTM;
		}

		pControlledPlayerEntity->SetLocalTM(newGunnerTM, ENTITY_XFORM_USER);

//		CryWatch("Mount wrot: plr: %f vehicle: %f wpn: %f", pControlledPlayerEntity->GetWorldRotation().GetRotZ(), pParent ? pParent->GetWorldRotation().GetRotZ() : -99.0f, pMountedGun->GetEntity()->GetWorldRotation().GetRotZ());
//		CryWatch("Mount lrot: plr: %f vehicle: %f wpn: %f", pControlledPlayerEntity->GetRotation().GetRotZ(), pParent ? pParent->GetRotation().GetRotZ() : -99.0f, pMountedGun->GetEntity()->GetRotation().GetRotZ());
	}		
}

float CMountedGunController::CalculateAnimationTime(float aimRad)
{
	const float ANIM_ANGLE_RANGE = gf_PI*0.25f;

	float time = fmod_tpl(aimRad / ANIM_ANGLE_RANGE, 1.0f);
	time = (float)__fsel(time, time, 1.0f + time);

	return time;
}

//////////////////////////////////////////////////////////////////////////
/// Recording System Replay - This should work through mannequin
void CMountedGunController::ReplayStartThirdPersonAnimations( ICharacterInstance* pCharacter, int upAnimId, int downAnimId )
{
	assert(pCharacter);
	ISkeletonAnim *pSkeletonAnim = pCharacter->GetISkeletonAnim();
	assert(pSkeletonAnim);

	CryCharAnimationParams animParams;
	animParams.m_nFlags = CA_LOOP_ANIMATION;
	animParams.m_fTransTime = 0.1f;

	animParams.m_nLayerID = REPLAY_PLAYER_ANIMATION_LAYER_AIM_UP;
	animParams.m_fPlaybackWeight = 0.0f;

	pSkeletonAnim->StartAnimationById(upAnimId, animParams);

	animParams.m_nLayerID = REPLAY_PLAYER_ANIMATION_LAYER_AIM_DOWN;
	animParams.m_fPlaybackWeight = 0.0f;
	pSkeletonAnim->StartAnimationById(downAnimId, animParams);
}

void CMountedGunController::ReplayStopThirdPersonAnimations( ICharacterInstance* pCharacter )
{
	assert(pCharacter);
	ISkeletonAnim *pSkeletonAnim = pCharacter->GetISkeletonAnim();
	assert(pSkeletonAnim);

	pSkeletonAnim->StopAnimationInLayer(REPLAY_PLAYER_ANIMATION_LAYER_AIM_UP, 0.1f);
	pSkeletonAnim->StopAnimationInLayer(REPLAY_PLAYER_ANIMATION_LAYER_AIM_DOWN, 0.1f);
}


void CMountedGunController::ReplayUpdateThirdPersonAnimations( ICharacterInstance* pCharacter, float aimRad, float aimUp, float aimDown, bool firstPerson /*= false*/)
{
	if (pCharacter)
	{
		ISkeletonAnim *pSkeletonAnim = pCharacter->GetISkeletonAnim();
		assert(pSkeletonAnim);

		//Update manually animation time, to match current weapon orientation
		uint32 numAnimsLayer = pSkeletonAnim->GetNumAnimsInFIFO(REPLAY_PLAYER_ANIMATION_LAYER_BASE);
		for(uint32 i=0; i<numAnimsLayer; i++)
		{
			CAnimation &animation = pSkeletonAnim->GetAnimFromFIFO(REPLAY_PLAYER_ANIMATION_LAYER_BASE, i);
			if (animation.HasStaticFlag(CA_MANUAL_UPDATE))
			{
				float time = CalculateAnimationTime(aimRad);
				pSkeletonAnim->SetAnimationNormalizedTime(&animation, time);
				animation.ClearStaticFlag( CA_DISABLE_MULTILAYER );
			}
		}

		if (firstPerson)
			return;

		//Update additive weights for aiming up/down
		pSkeletonAnim->SetLayerBlendWeight(REPLAY_PLAYER_ANIMATION_LAYER_AIM_UP, aimUp);
		pSkeletonAnim->SetLayerBlendWeight(REPLAY_PLAYER_ANIMATION_LAYER_AIM_DOWN, aimDown);
	}
}

//////////////////////////////////////////////////////////////////////////

void CMountedGunController::UpdateFirstPersonAnimations( CItem* pMountedGun, const Vec3 &aimDirection )
{
	if (const CWeapon* pMountedWeapon = static_cast<CWeapon*>(pMountedGun->GetIWeapon()))
	{
		const IZoomMode* pCurrentZoomMode = pMountedWeapon->GetZoomMode(pMountedWeapon->GetCurrentZoomMode());
		const SMountParams* pMountParams = pMountedWeapon->GetMountedParams();

		float ironSightWeigth = 0.0f;
		
		if (pCurrentZoomMode && (pCurrentZoomMode->IsZoomed() || pCurrentZoomMode->IsZoomingInOrOut()))
		{
			ironSightWeigth = 1.0f;
			if (pCurrentZoomMode->IsZoomingInOrOut())
			{
				ironSightWeigth = pCurrentZoomMode->GetZoomTransition();
			}
		}
		
		IEntity* pMountedWeaponEntity = pMountedWeapon->GetEntity();
		const Matrix34& weaponTM = pMountedWeaponEntity->GetWorldTM();
		const Matrix34& playerTMInv = m_pControlledPlayer->GetEntity()->GetWorldTM().GetInverted();

		const Vec3 localTargetPoint = pMountParams ? LERP(pMountParams->fpBody_offset, pMountParams->fpBody_offset_ironsight, ironSightWeigth) : Vec3(0.f,0.f,0.f);
		const Vec3 desiredCameraPos = weaponTM.TransformPoint(localTargetPoint);
		const Vec3 modelSpacePoint = playerTMInv.TransformPoint(desiredCameraPos);

		m_pControlledPlayer->GetActorParams().mountedWeaponCameraTarget = modelSpacePoint;

		// Set weapon entity local TM so it can be rendered in Camera Space in the nearest pass
		const int slotIndex = 0;
		const uint32 mountedWeaponSlotFlags = pMountedWeaponEntity->GetSlotFlags(slotIndex);
		if(mountedWeaponSlotFlags & ENTITY_SLOT_RENDER_NEAREST)
		{
			const Vec3 cameraSpacePos = -localTargetPoint;
			pMountedWeapon->GetEntity()->SetSlotCameraSpacePos(slotIndex,cameraSpacePos);
		}
	}
}


void CMountedGunController::UpdateIKMounted( CItem* pMountedGun )
{
	const SMountParams* pMountParams = pMountedGun->GetMountedParams();

	bool hasIKHelpers = (pMountParams != NULL) && (!pMountParams->left_hand_helper.empty()) && (!pMountParams->right_hand_helper.empty());
	if (hasIKHelpers)
	{
		bool isThirdPerson = m_pControlledPlayer->IsThirdPerson();

		const Vec3 leftHandPos = pMountedGun->GetSlotHelperPos(isThirdPerson ? eIGS_ThirdPerson : eIGS_FirstPerson, pMountParams->left_hand_helper.c_str(), true);
		const Vec3 rightHandPos = pMountedGun->GetSlotHelperPos(isThirdPerson ? eIGS_ThirdPerson: eIGS_FirstPerson, pMountParams->right_hand_helper.c_str(), true);

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(leftHandPos, 0.075f, ColorB(255, 255, 255, 255));
		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(rightHandPos, 0.075f, ColorB(128, 128, 128, 255));

		if (isThirdPerson)
		{
			m_pControlledPlayer->SetIKPos("leftArm",		leftHandPos, 1);
			m_pControlledPlayer->SetIKPos("rightArm",	rightHandPos, 1);
		}
		else
		{
			m_pControlledPlayer->SetIKPos("leftArmShadow",		leftHandPos, 1);
			m_pControlledPlayer->SetIKPos("rightArmShadow",	rightHandPos, 1);
		}
	}
}


Vec3 CMountedGunController::GetMountDirection( CItem* pMountedGun, IEntity* pParent ) const
{
	if (pParent == NULL)
	{
		return pMountedGun->GetStats().mount_dir;
	}
	else
	{
		return pParent->GetWorldTM().TransformVector(pMountedGun->GetStats().mount_dir);
	}
}

Vec3 CMountedGunController::GetMountedGunPosition( CItem* pMountedGun, IEntity* pParent ) const
{
	if (pParent == NULL)
	{
		return pMountedGun->GetEntity()->GetWorldPos();
	}
	else
	{
		return pParent->GetWorldTM().TransformPoint(pMountedGun->GetEntity()->GetLocalTM().GetTranslation());
	}
}

void CMountedGunController::InitMannequinParams()
{
	CRY_ASSERT(g_pGame);
	IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
	CRY_ASSERT(pGameFramework);
	CMannequinUserParamsManager& mannequinUserParams = pGameFramework->GetMannequinInterface().GetMannequinUserParamsManager();

	CRY_ASSERT(m_pControlledPlayer);
	IAnimatedCharacter* pAnimatedCharacter = m_pControlledPlayer->GetAnimatedCharacter();
	CRY_ASSERT(pAnimatedCharacter);
	IActionController* pActionController = pAnimatedCharacter->GetActionController();
	m_pMannequinParams = mannequinUserParams.FindOrCreateParams<SMannequinMountedGunParams>(pActionController);

	MountedGunCRCs.Init();
}
