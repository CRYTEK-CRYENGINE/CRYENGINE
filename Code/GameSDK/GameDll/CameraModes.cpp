// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Camera mode implementations

-------------------------------------------------------------------------
History:
- 16:10:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "CameraModes.h"
#include "Player.h"
#include "GameCVars.h"
#include "Single.h"
#include "Effects/GameEffects/KillCamGameEffect.h"
#include "RecordingSystem.h"

#include "GameRulesModules/IGameRulesSpectatorModule.h"

#include "Utility/CryWatch.h"

#include <CryCore/TypeInfo_impl.h>

#include <IViewSystem.h>
#include <IVehicleSystem.h>

#include "PlayerVisTable.h"

#include "LocalPlayerComponent.h"
#include "VTOLVehicleManager/VTOLVehicleManager.h"

namespace
{


	CCameraPose FirstPersonCameraAnimation(const CPlayer& clientPlayer, const CCameraPose& firstPersonCamera, float cameraFactor)
	{
		const QuatT &lastCameraDelta = clientPlayer.GetLastSTAPCameraDelta();
		const CCameraPose animatedCameraOffset = CCameraPose(firstPersonCamera.GetRotation() * lastCameraDelta.t, lastCameraDelta.q);
		const CCameraPose cameraAnimation = CCameraPose::Compose(firstPersonCamera, CCameraPose::Scale(animatedCameraOffset, cameraFactor));

		return cameraAnimation;
	}



	CCameraPose MountedGunView(const CPlayer& clientPlayer, IItem* pCurrentItem, const CCameraPose& firstPersonCamera)
	{
		bool isMounted = pCurrentItem ? static_cast<CItem*>(pCurrentItem)->IsMounted() : false;

		if (isMounted)
		{
			const CWeapon* pCurrentWeapon = static_cast<CWeapon*>(pCurrentItem->GetIWeapon());
			const bool isWeaponZoomed = pCurrentWeapon ? pCurrentWeapon->IsZoomed() : false;
			const float weaponFPAimHoriz = clientPlayer.m_weaponFPAiming.GetHorizontalSweay();
			const float weaponFPAimVert = clientPlayer.m_weaponFPAiming.GetVerticalSweay();

			const float zoomFactor = 0.2f;
			const float horizontalDirection = isWeaponZoomed ? -1.0f : 1.0f;
			const float factor = isWeaponZoomed ? zoomFactor : 1.0f;
			const float rotationMultiplier = 0.05f * factor;
			const float positionmultiplier = 0.035f * factor;

			const Vec3 right = firstPersonCamera.GetRotation().GetColumn0();
			const Vec3 up = firstPersonCamera.GetRotation().GetColumn2();

			const Ang3 offsetAngles = Ang3(
				-weaponFPAimVert * rotationMultiplier,
				0.0f,
				-weaponFPAimHoriz * rotationMultiplier * horizontalDirection);
			const Vec3 offsetPosition =
				- weaponFPAimHoriz*right*positionmultiplier*horizontalDirection
				+ weaponFPAimVert*up*positionmultiplier;

			return CCameraPose(offsetPosition, Quat(offsetAngles));
		}
		else
		{
			return CCameraPose();
		}
	}


}



CDefaultCameraMode::CDefaultCameraMode()
	:	m_lastTpvDistance(g_pGameCVars->cl_tpvDist)
	, m_bobCycle(0.0f)
	,	m_bobOffset(ZERO)
	, m_minAngleTarget(std::numeric_limits<float>::infinity())
	, m_minAngleRate(0.0f)
	, m_minAngle(0.0f)
{
}

CDefaultCameraMode::~CDefaultCameraMode()
{

}

bool CDefaultCameraMode::UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime)
{
	viewParams.fov = g_pGame->GetFOV()* clientPlayer.GetActorParams().viewFoVScale * (gf_PI/180.0f);

	bool isThirdPerson = clientPlayer.IsThirdPerson();
	bool ret = false;

	IItem* pCurrentItem = clientPlayer.GetCurrentItem();

	const CCameraPose cameraPose =
		!isThirdPerson ?
			FirstPersonCameraPose(clientPlayer, pCurrentItem,  frameTime) : 
			ThirdPersonCameraPose(clientPlayer, viewParams);

	viewParams.position = cameraPose.GetPosition();
	viewParams.rotation = cameraPose.GetRotation();

#ifndef _RELEASE
	if (g_pGameCVars->pl_debug_view)
	{
		CryWatch("DefaultCamera Pos (%.2f, %.2f, %.2f)", viewParams.position.x, viewParams.position.y, viewParams.position.z);
		Vec3 fwd = viewParams.rotation.GetColumn1();
		CryWatch("DefaultCamera Rot (%.2f, %.2f, %.2f, %.2f) (%f, %f, %f)", viewParams.rotation.v.x, viewParams.rotation.v.y, viewParams.rotation.v.z, viewParams.rotation.w, fwd.x, fwd.y, fwd.z);
	}
#endif //_RELEASE

	if (!isThirdPerson)
	{
		// Prevent the camera from going any lower than the angle limit to make sure you
		// don't see the stretching between the waist and torso (e.g. while reloading)
		if (pCurrentItem)
		{
			CWeapon* pWeapon = (CWeapon*)pCurrentItem->GetIWeapon();
			if (pWeapon)
			{
				Vec3 forward = viewParams.rotation.GetColumn1();
				Vec3 down = Vec3(0, 0, -1);

				float dotProd = forward.dot(down);
				float angle = acos(dotProd);

				SPlayerRotationParams::EAimType aimType = clientPlayer.GetCurrentAimType();
				const SAimAccelerationParams &params = clientPlayer.GetPlayerRotationParams().GetVerticalAimParams(aimType, !clientPlayer.IsThirdPerson());

				// Smooth transition for up/down angle limits	
				if (m_minAngleTarget == std::numeric_limits<float>::infinity())
				{
					m_minAngle = m_minAngleTarget = params.angle_min;
					m_angleTransitionTimer = gEnv->pTimer->GetFrameStartTime();
					m_minAngleRate = 0;
				}
				if (params.angle_min != m_minAngleTarget)
				{
					m_minAngle = m_minAngleTarget;
					m_minAngleTarget = params.angle_min;
					m_angleTransitionTimer = gEnv->pTimer->GetFrameStartTime();
					m_minAngleRate = 0;
				}
				const float transitionTime = g_pGameCVars->cl_angleLimitTransitionTime;
				float timer = gEnv->pTimer->GetFrameStartTime().GetDifferenceInSeconds(m_angleTransitionTimer);
				float minAngle;
				if (timer < transitionTime && m_minAngle < m_minAngleTarget)
				{
					SmoothCD(m_minAngle, m_minAngleRate, timer, m_minAngleTarget, transitionTime);
					minAngle = m_minAngle;
				}
				else
				{
					minAngle = m_minAngleTarget;
				}
				minAngle = DEG2RAD(90 + minAngle);
				
				// Angle is relative to downwards direction, 0 degrees = straight down, 90 degrees = horizontal, 180 = straight up
				if (angle < minAngle)
				{
					Vec3 crossProd = forward.cross(down);
					float diff = angle - minAngle;
					Quat adjustment = Quat::CreateRotationAA(diff, crossProd.GetNormalized());
					viewParams.rotation = adjustment * viewParams.rotation;
				}
			}
		}

		if (!isThirdPerson)
		{
			ret = ApplyItemFPViewFilter(clientPlayer, pCurrentItem, viewParams);
		}

#ifndef _RELEASE
		if (g_pGameCVars->pl_debug_view)
		{
			Vec3 fwd = viewParams.rotation.GetColumn1();
			CryWatch("DefaultCamera(PostItem) Rot (%.2f, %.2f, %.2f, %.2f) (%f, %f, %f)", viewParams.rotation.v.x, viewParams.rotation.v.y, viewParams.rotation.v.z, viewParams.rotation.w, fwd.x, fwd.y, fwd.z);
		}
#endif //_RELEASE
	}

	return ret;
}

bool CDefaultCameraMode::ApplyItemFPViewFilter(const CPlayer& clientPlayer, IItem* pCurrentItem, SViewParams& viewParams)
{
	if (pCurrentItem)
	{
		return pCurrentItem->FilterView(viewParams);		
	}

	return false;
}

CCameraPose CDefaultCameraMode::FirstPersonCameraPose(const CPlayer& clientPlayer, IItem* pCurrentItem, float frameTime)
{
	bool bWorldSpace = true;

	// If rendering in camera space then remove world space position here
	IEntity* pEntity = clientPlayer.GetEntity();
	uint32 entityFlags = (pEntity) ? pEntity->GetSlotFlags(eIGS_FirstPerson) : 0;
	if(entityFlags & ENTITY_SLOT_RENDER_NEAREST)
	{
		bWorldSpace = false;
	}

	Vec3 firstPersonCameraPosePos = clientPlayer.GetFPCameraPosition(bWorldSpace);
	if(!bWorldSpace)
	{
		// Apply player rot onto pose for camera space rendering
		firstPersonCameraPosePos = clientPlayer.GetEntity()->GetWorldRotation() * firstPersonCameraPosePos;
	}

	const CCameraPose firstPersonCameraPose = CCameraPose(
		firstPersonCameraPosePos,
		clientPlayer.GetViewQuatFinal());

	return
		CCameraPose::Compose(
			firstPersonCameraPose,
			CCameraPose::Compose(
				ViewBobing(clientPlayer, pCurrentItem, frameTime),
				MountedGunView(clientPlayer, pCurrentItem, firstPersonCameraPose)));
}



CCameraPose CDefaultCameraMode::ThirdPersonCameraPose(const CPlayer& clientPlayer, const SViewParams& viewParams)
{
	const float cameraYaw = g_pGameCVars->cl_tpvYaw;
	const Quat cameraYawRotation =
		cameraYaw > 0.001f ? 
		Quat::CreateRotationXYZ(Ang3(0, 0, cameraYaw * gf_PI/180.0f)) :
		Quat(IDENTITY);
	Quat tpCameraRotation = clientPlayer.GetViewQuatFinal() * cameraYawRotation;

	const Vec3 forward = tpCameraRotation.GetColumn1();
	const Vec3 down = Vec3(0, 0, -1);
	const float dotProd = forward.dot(down);
	const float angle = acos(dotProd);

	SPlayerRotationParams::EAimType aimType = clientPlayer.GetCurrentAimType();
	const SAimAccelerationParams &params = clientPlayer.GetPlayerRotationParams().GetVerticalAimParams(aimType, !clientPlayer.IsThirdPerson());
	const float minAngle = DEG2RAD(90 + params.angle_min);

	// Angle is relative to downwards direction, 0 degrees = straight down, 90 degrees = horizontal, 180 = straight up
	if (angle < minAngle)
	{
		Vec3 crossProd = forward.cross(down);
		Quat adjustment = Quat::CreateRotationAA(angle - minAngle, crossProd.GetNormalized());
		tpCameraRotation = adjustment * tpCameraRotation;
	}

	float fSin, fCos;
	sincos_tpl(tpCameraRotation.GetRotZ(), &fSin, &fCos);
	const Vec3 cameraDistanceVector(
		g_pGameCVars->cl_tpvDistHorizontal * fCos + g_pGameCVars->cl_tpvDist * fSin,
		g_pGameCVars->cl_tpvDistHorizontal * fSin - g_pGameCVars->cl_tpvDist * fCos,
		g_pGameCVars->cl_tpvDistVertical - (g_pGameCVars->cl_tpvDist * tpCameraRotation.GetFwdZ())
		);

	const Matrix34& clientPlayerTM = clientPlayer.GetEntity()->GetWorldTM();
	const Vec3 stanceViewOffset = clientPlayer.GetBaseQuat() * clientPlayer.GetStanceViewOffset(clientPlayer.GetStance());
	Vec3 tpCameraPosition = clientPlayerTM.GetTranslation() + cameraDistanceVector + stanceViewOffset;
	const Vec3 tpCameraDirection = (Vec3Constants<float>::fVec3_OneY * tpCameraRotation.GetInverted()) * (g_pGameCVars->cl_tpvDist);

	const Vec3 tpCrosshairPosition = tpCameraPosition + tpCameraDirection;

	// Handle camera collisions
	if (g_pGameCVars->cl_tpvCameraCollision)
	{
		IPhysicalEntity *pSkipEnts[5];
		const int nSkipEnts = clientPlayer.GetPhysicalSkipEntities(pSkipEnts, CRY_ARRAY_COUNT(pSkipEnts));
		
		primitives::sphere spherePrim;
		spherePrim.center = tpCrosshairPosition;
		spherePrim.r = g_pGameCVars->cl_tpvCameraCollisionOffset;

		float fDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(spherePrim.type, &spherePrim, -tpCameraDirection,
			ent_all, nullptr, 0, geom_colltype0, nullptr, nullptr, 0, pSkipEnts, nSkipEnts);
		if (fDist <= 0.0f)
		{
			fDist = g_pGameCVars->cl_tpvDist;
		}
		fDist = max(g_pGameCVars->cl_tpvMinDist, fDist);
		
		Interpolate(m_lastTpvDistance, fDist, g_pGameCVars->cl_tpvInterpolationSpeed, viewParams.frameTime);
		tpCameraPosition = tpCrosshairPosition - (tpCameraDirection.GetNormalized() * m_lastTpvDistance);
	}

	return CCameraPose(tpCameraPosition, tpCameraRotation);
}



CCameraPose CDefaultCameraMode::ViewBobing(const CPlayer& clientPlayer, IItem* pCurrentItem, float frameTime)
{
	const float headBod = 0.1f;
	const float headBobLimit = 1.5f;

	CWeapon* pPlayerWeapon = pCurrentItem ? static_cast<CWeapon*>(pCurrentItem->GetIWeapon()) : NULL;
	if (pPlayerWeapon && pPlayerWeapon->IsZoomed())
		return CCameraPose();
	if (pPlayerWeapon)
	{
		const SAimLookParameters& lookPrams =
			pPlayerWeapon->GetSharedItemParams()->params.aimLookParams;
		bool sprinting = clientPlayer.IsSprinting();
		bool sprintCameraAnimation = lookPrams.sprintCameraAnimation;
		if (sprinting && sprintCameraAnimation)
			return CCameraPose();
	}

	EStance refStance = clientPlayer.GetStance();
	float standSpeed = clientPlayer.GetStanceMaxSpeed(refStance);
	float flatSpeed = clientPlayer.GetActorStats()->speedFlat;
	Vec3 playerVelocity = clientPlayer.GetActorPhysics().velocity;
	bool onGround = clientPlayer.IsOnGround();
	Quat viewQuatFinal = clientPlayer.GetViewQuatFinal();
	Quat baseQuat = clientPlayer.GetBaseQuat();
	bool sprinting = clientPlayer.IsSprinting();

	Vec3 bobOffset(0,0,0);

	Vec3 vSpeed(0,0,0);
	if (standSpeed > 0.001f)
		vSpeed = (playerVelocity / standSpeed);

	float vSpeedLen(vSpeed.len());
	if (vSpeedLen>1.5f)
		vSpeed = vSpeed / vSpeedLen * 1.5f;

	float speedMul = 0;
	if (standSpeed > 0.001f)
		speedMul = (flatSpeed / standSpeed * 1.1f);

	speedMul = min(1.5f,speedMul);

	float verticalSpeed = SATURATE(std::abs(vSpeed.z));

	if (onGround)
	{
		const float kSpeedToBobFactor = g_pGameCVars->cl_speedToBobFactor;
		const float bobWidth = (!sprinting ? g_pGameCVars->cl_bobWidth : g_pGameCVars->cl_bobWidth * g_pGameCVars->cl_bobSprintMultiplier) * (1.0f - verticalSpeed);
		const float bobHeight = min((!sprinting ? g_pGameCVars->cl_bobHeight : g_pGameCVars->cl_bobHeight * g_pGameCVars->cl_bobSprintMultiplier) * (verticalSpeed*g_pGameCVars->cl_bobVerticalMultiplier + 1.0f), g_pGameCVars->cl_bobMaxHeight);
		const float kStrafeHorzScale = g_pGameCVars->cl_strafeHorzScale;
		const float kBobWidth = bobWidth * speedMul;
		const float kBobHeight = bobHeight * speedMul;

		m_bobCycle += frameTime * kSpeedToBobFactor * standSpeed;

		if (speedMul < 0.1f)
			m_bobCycle = min(m_bobCycle + frameTime, 1.0f);

		if (m_bobCycle > 1.0f)
			m_bobCycle = m_bobCycle - 1.0f;

		Vec3 bobDir(sin_tpl(m_bobCycle*gf_PI*2.0f)*kBobWidth*speedMul,0,sin_tpl(m_bobCycle*gf_PI*4.0f)*kBobHeight*speedMul);

		if (speedMul > 0.01f)
		{
			float dot(viewQuatFinal.GetColumn0() * vSpeed);
			bobDir.x -= dot * kStrafeHorzScale;
		}

		bobOffset += bobDir;
		bobOffset -= baseQuat.GetColumn2() * 0.035f * speedMul;
	}
	else
	{
		m_bobCycle = 0;
	}

	float headBobScale = 1.0f;
	if (standSpeed * standSpeed > 0.001f)
	{
		headBobScale = flatSpeed / standSpeed;
	}
	headBobScale = min(1.0f, headBobScale);
	bobOffset = bobOffset * 2.0f * headBod * headBobScale;

	Interpolate(m_bobOffset, bobOffset, 3.95f, frameTime);

	float bobLenSq = m_bobOffset.GetLengthSquared();
	if (bobLenSq > headBobLimit*headBobLimit)
	{
		float bobLen = sqrt_tpl(bobLenSq);
		m_bobOffset *= headBobLimit/bobLen;
	}

	return CCameraPose(
		Vec3(ZERO),
		Quat(
			Ang3(
				m_bobOffset.z,
				m_bobOffset.x,
				m_bobOffset.x)));
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CSpectatorFollowCameraMode::CSpectatorFollowCameraMode()
: m_lastSpectatorTarget(0)
, m_lastFrameId(0)
, m_viewOffset(ZERO)
, m_entityPos(ZERO)
, m_entityExtra(ZERO)
, m_orbitX(0.f)
, m_orbitZ(0.f)
, m_postEffectsForSpecMode(0)
{
	m_disableDrawNearest = true;
}

CSpectatorFollowCameraMode::~CSpectatorFollowCameraMode()
{

}

bool CSpectatorFollowCameraMode::UpdateView( const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime )
{
	const SFollowCameraSettings& followCameraSettings = clientPlayer.GetCurrentFollowCameraSettings();

	const EntityId spectatorTargetId = clientPlayer.GetSpectatorTarget();

	bool crouching = false;
	float minFocusRange = 0.0f;
	CActor* pTargetActor = (CActor*)g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(spectatorTargetId);
	IEntity *pTargetEntity = NULL;
	IVehicle *pTargetVehicle = NULL;
	IItem *pTargetItem = NULL;
	viewParams.nearplane = 0.05f;
	CGameRules* pGameRules = g_pGame->GetGameRules();

	if (pTargetActor)
	{
		pTargetEntity = pTargetActor->GetEntity();
		pTargetVehicle = pTargetActor->GetLinkedVehicle();
		if(pTargetVehicle && pGameRules)
		{
			if(CVTOLVehicleManager* pVTOLManager = pGameRules->GetVTOLVehicleManager())
			{
				if(pVTOLManager->IsVTOL(pTargetVehicle->GetEntityId()))
				{
					pTargetVehicle = NULL;
				}
			}
		}

		pTargetItem = pTargetActor->GetCurrentItem();

		if (pTargetActor->IsPlayer())
		{
			CPlayer *pTargetPlayer = static_cast<CPlayer*>(pTargetActor);

			crouching = pTargetPlayer->GetStance() == STANCE_CROUCH;

			if (pTargetPlayer->IsInFreeFallDeath())
			{
				if (pGameRules)
				{
					IGameRulesSpectatorModule* specmod = pGameRules->GetSpectatorModule();
					if (specmod)
					{
						specmod->ChangeSpectatorModeBestAvailable(&clientPlayer, false);
					}
				}

				return false;
			}
		}
	}
	else if( IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(spectatorTargetId) )
	{
		pTargetVehicle = pVehicle;
		pTargetEntity = pVehicle->GetEntity();
	}
	else
	{
		pTargetEntity = gEnv->pEntitySystem->GetEntity(spectatorTargetId);
		if (!pTargetEntity)
		{
			if (pGameRules)
			{
				IGameRulesSpectatorModule* specmod = pGameRules->GetSpectatorModule();
				if (specmod)
				{
					specmod->ChangeSpectatorModeBestAvailable(&clientPlayer, false);
				}
			}
			return false;
		}
	}


	// Start building the camera information for the view.

	const Matrix34& targetWorldTM = pTargetEntity->GetWorldTM();

	Vec3 targetWorldPos = targetWorldTM.GetTranslation();
	Vec3 targetWorldExtra(ZERO);
	if(crouching)
	{
		targetWorldExtra.z -= 0.75f;
	}

	Ang3 worldAngles = Ang3::GetAnglesXYZ(targetWorldTM);

	IMovementController* pMC(NULL);
	if(pTargetActor && (followCameraSettings.m_flags & SFollowCameraSettings::eFCF_UseEyeDir) && (pMC = pTargetActor->GetMovementController()))
	{
		SMovementState s;
		pMC->GetMovementState(s);

		Quat eyeQuat = Quat::CreateRotationVDir(s.eyeDirection);
		worldAngles.z = Ang3(eyeQuat).z;
		//We only want to adjust the target offset based on the yaw component of the eye direction
		Ang3 eyeAng(eyeQuat);
		eyeAng.x = 0.f;
		targetWorldPos += Quat(eyeAng) *  followCameraSettings.m_targetOffset;
	}
	else
	{
		targetWorldPos += targetWorldTM.TransformVector(followCameraSettings.m_targetOffset);
	}



	// Camera Rotation.
	static const float kAdditionalRot = gf_PI;
	float rot = followCameraSettings.m_cameraYaw + kAdditionalRot;

	if(followCameraSettings.m_flags & SFollowCameraSettings::eFCF_AllowOrbitYaw)
	{
		m_orbitZ += DEG2RAD(g_pGameCVars->g_spectate_follow_orbitYawSpeedDegrees) * clientPlayer.GetSpectatorOrbitYawSpeed() * frameTime;
		rot += m_orbitZ;

		if(g_pGameCVars->g_spectate_follow_orbitAlsoRotateWithTarget)
		{
			rot -= worldAngles.z;
		}
	}
	else
	{
		m_orbitZ = 0.f;
		rot -= worldAngles.z;
	}

	if(followCameraSettings.m_flags & SFollowCameraSettings::eFCF_AllowOrbitPitch)
	{
		m_orbitX += DEG2RAD(g_pGameCVars->g_spectate_follow_orbitYawSpeedDegrees) * clientPlayer.GetSpectatorOrbitPitchSpeed() * frameTime;
		m_orbitX = clamp_tpl(m_orbitX, g_pGameCVars->g_spectate_follow_orbitMinPitchRadians, g_pGameCVars->g_spectate_follow_orbitMaxPitchRadians);
	}
	else
	{
		m_orbitX = 0.f;
	}

	// Optimum Distance.
	float distance = followCameraSettings.m_desiredDistance;
	if(pTargetVehicle)
	{
		AABB vehicleBox;
		pTargetVehicle->GetEntity()->GetLocalBounds(vehicleBox);
		const float vehicleDiameter = 2.0f * vehicleBox.GetRadius();
		distance = vehicleDiameter;
		minFocusRange = vehicleDiameter;
	}


	// Get target Bounds.
	AABB targetBounds;
	pTargetEntity->GetLocalBounds(targetBounds);

	// Camera offset.
	const float cameraHeightOffset = pTargetVehicle ? 2.0f : followCameraSettings.m_cameraHeightOffset;



	// World offsets are the intended camera world position relative to the look-at point (neg view direction).
	const Vec3 worldOffset( distance*cosf(m_orbitX)*sinf(rot), distance*cosf(rot)*cosf(m_orbitX), distance*sinf(m_orbitX)+cameraHeightOffset+targetBounds.max.z );

	Vec3 worldOffset_Norm( worldOffset );
	float worldOffset_Len = worldOffset_Norm.NormalizeSafe();

	const Matrix33 viewRotationMatrix = Matrix33::CreateRotationVDir(-worldOffset_Norm);
	const Matrix33 invViewRotationMatrix(viewRotationMatrix.GetInverted());

	// Apply the View Offset.
	targetWorldPos += viewRotationMatrix.TransformVector(followCameraSettings.m_viewOffset);
	

	// Do the physics checks.
	PhysSkipList skipList;
	if (pTargetItem)
	{
		CWeapon* pWeapon = (CWeapon*)pTargetItem->GetIWeapon();
		if (pWeapon)
			CSingle::GetSkipEntities(pWeapon, skipList);
	}
	else if (pTargetVehicle)
	{
		// vehicle drivers don't seem to have current items, so need to add the vehicle itself here
		IPhysicalEntity* pSkipEntities[10];
		int nSkip = 0;
		nSkip = pTargetVehicle->GetSkipEntities(pSkipEntities, 10);
		for (int i = 0; i < nSkip; ++i)
			stl::push_back_unique(skipList, pSkipEntities[i]);
	}

	// Let the actor have a say
	if(pTargetActor)
	{
		IPhysicalEntity* pSkipEntities[10];
		int nSkip = pTargetActor->GetPhysicalSkipEntities(pSkipEntities,10); 
		for(int i = 0; i < nSkip; ++i)
		{
			stl::push_back_unique(skipList, pSkipEntities[i]);
		}
	}

	primitives::sphere sphere;
	sphere.center = (targetWorldPos+targetWorldExtra);
	sphere.r = viewParams.nearplane+0.04f;

	geom_contact *pContact = 0;          
	const float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, worldOffset, ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid,
		&pContact, 0, (geom_colltype_player<<rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, !skipList.empty() ? &skipList[0] : NULL, skipList.size());

	// even when we have contact, keep the camera the same height above the target
	const float minHeightDiff = worldOffset.z;

	if(hitDist > 0.f && pContact)
	{
		if(followCameraSettings.m_flags & SFollowCameraSettings::eFCF_DisableHeightAdj)
		{
			float minimumDistFromTarget = 0.f;
			if(pTargetActor)
			{
				// Reverse cast to hit the player.
				if(ICharacterInstance* pCharacter = pTargetActor->GetEntity()->GetCharacter(0))
				{
					if(IPhysicalEntity* pBodyPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
					{
						ray_hit hit;
						sphere.center += worldOffset;
						if(gEnv->pPhysicalWorld->CollideEntityWithPrimitive(pBodyPhys, sphere.type, &sphere, -worldOffset, &hit))
						{
							if(hit.pCollider && hit.dist>0.f)
							{
								minimumDistFromTarget = (worldOffset_Len-(hit.dist-sphere.r));
							}
						}
					}
				}
			}

			worldOffset_Len = max(minimumDistFromTarget, hitDist-sphere.r);
		}
		else
		{
			worldOffset_Len = hitDist;
			const float newZLength = hitDist*worldOffset_Norm.z;

			if( newZLength < minHeightDiff )
			{
				// can't move the camera far enough away from the player in this direction. Try moving it directly up a bit
				int numHits = 0;

				// (move back just slightly to avoid colliding with the wall we've already found...)
				sphere.center += worldOffset_Norm * (worldOffset_Len-0.05f);

				const float newHitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, Vec3(0,0,minHeightDiff), ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid,
					&pContact, 0, (geom_colltype_player<<rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, !skipList.empty() ? &skipList[0] : NULL, skipList.size());

				float raiseDist = minHeightDiff - (worldOffset_Len*worldOffset_Norm.z) - sphere.r;
				if(newHitDist != 0)
				{
					raiseDist = std::min(minHeightDiff, newHitDist);
				}

				raiseDist = std::max(0.0f, raiseDist);

				targetWorldExtra.z += raiseDist*0.8f;
			}
		}
	}


	// Calc the local camera offset (camera space)
	const Vec3 localViewOffset( invViewRotationMatrix.TransformVector(worldOffset_Len*worldOffset_Norm) );


	const int thisFrameId = gEnv->pRenderer->GetFrameID();
	if( (thisFrameId-m_lastFrameId) > 5 || (m_lastSpectatorTarget!=spectatorTargetId) )
	{
		// Force catch-up on frame skip or change of target.
		m_viewOffset = localViewOffset;
		m_entityPos = targetWorldPos;
		m_entityExtra = targetWorldExtra;
		m_lastSpectatorTarget = spectatorTargetId;
	}
	else if( pTargetVehicle )
	{
		// Vehicles have locked everything except the view offset.
		Interpolate(m_viewOffset, localViewOffset, 5.0f, viewParams.frameTime);
		m_entityPos = targetWorldPos;
		m_entityExtra = targetWorldExtra;
		m_viewOffset = localViewOffset;
	}
	else
	{
		const float selChange = ((targetWorldPos+targetWorldExtra)-m_entityPos).GetLengthSquared()-100.0f;

		const float baseSpeed = (float)__fsel(selChange*followCameraSettings.m_cameraSpeed,followCameraSettings.m_cameraSpeed,10000.0f);

		const Vec3 offsetSpeeds( 
									(float)__fsel( followCameraSettings.m_offsetSpeeds.x-FLT_EPSILON, followCameraSettings.m_offsetSpeeds.x, baseSpeed),
									(float)__fsel( followCameraSettings.m_offsetSpeeds.y-FLT_EPSILON, followCameraSettings.m_offsetSpeeds.y, baseSpeed),
									(float)__fsel( followCameraSettings.m_offsetSpeeds.z-FLT_EPSILON, followCameraSettings.m_offsetSpeeds.z, baseSpeed));

		// Interpolate the target pos.
		Interpolate(m_entityPos, targetWorldPos, baseSpeed, viewParams.frameTime);

		// Interpolate the extra offset on the target, then reapply.
		Interpolate(m_entityExtra, targetWorldExtra, offsetSpeeds.z, viewParams.frameTime);

		// Interpolate the offset on each camera axis, then reapply.
		Interpolate(m_viewOffset.x, localViewOffset.x, offsetSpeeds.x, viewParams.frameTime);	//SIDE-SIDE
		Interpolate(m_viewOffset.y, localViewOffset.y, offsetSpeeds.y, viewParams.frameTime);	//IN-OUT
		Interpolate(m_viewOffset.z, localViewOffset.z, offsetSpeeds.z, viewParams.frameTime);	//UP-DOWN
	}

	// Update the frameId.
	m_lastFrameId = thisFrameId;

	// Save out the final results to viewParams.
	viewParams.position = m_entityPos + m_entityExtra + viewRotationMatrix.TransformVector(m_viewOffset);
	Matrix33 rotation = Matrix33::CreateRotationVDir(((m_entityPos+m_entityExtra) - viewParams.position).GetNormalizedSafe());
	viewParams.rotation = Quat(rotation);
	viewParams.fov = (float)__fsel( followCameraSettings.m_cameraFov, followCameraSettings.m_cameraFov, viewParams.fov );

	ApplyEffects( clientPlayer, viewParams, (viewParams.position-targetWorldPos).GetLength(), minFocusRange );

#undef PDBG
	return false;
}

void CSpectatorFollowCameraMode::Activate( const CPlayer & clientPlayer )
{
}

void CSpectatorFollowCameraMode::Deactivate( const CPlayer & clientPlayer )
{
	RevertEffects();
	m_postEffectsForSpecMode = 0;
}

void CSpectatorFollowCameraMode::ApplyEffects( const CPlayer& clientPlayer, const SViewParams& viewParams, float focalDist, float minFocalRange )
{
	const uint8 prevMode = m_postEffectsForSpecMode;
	const uint8 specMode = clientPlayer.GetSpectatorMode();

	if( prevMode != specMode )
	{
		RevertEffects();

		if( specMode == CActor::eASM_Killer )
		{
			// Activate 
			CRecordingSystem *pCrs = g_pGame->GetRecordingSystem();
			CRY_ASSERT(pCrs);
			if (pCrs)
			{
				pCrs->GetKillCamGameEffect().SetCurrentMode(CKillCamGameEffect::eKCEM_KillerCam);
				pCrs->GetKillCamGameEffect().SetActiveIfCurrentMode(CKillCamGameEffect::eKCEM_KillerCam,true);
			}

			// make the local player exit all areas.. will turn off any reverb effects that are setup around the local player
			// ready for the killer cam and kill cam cameras to have their correct audio setup
			IAreaManager* pAreaManager = gEnv->pEntitySystem->GetAreaManager();
			pAreaManager->ExitAllAreas(clientPlayer.GetEntity()->GetId());
		}
	}

	switch( specMode )
	{
	case CActor::eASM_Killer:
		{
			IRenderer* pRenderer = gEnv->pRenderer;
			CRY_ASSERT(pRenderer);

			// Apply Post Effects.
			const float focalRange = (float)__fsel( g_pGameCVars->g_killercam_dofFocusRange-minFocalRange, g_pGameCVars->g_killercam_dofFocusRange, minFocalRange );
			pRenderer->EF_SetPostEffectParam("Dof_User_Active", 1.f, true);
			pRenderer->EF_SetPostEffectParam("Dof_User_FocusDistance", focalDist, true);
			pRenderer->EF_SetPostEffectParam("Dof_User_FocusRange", focalRange, true);
			pRenderer->EF_SetPostEffectParam("Dof_User_BlurAmount", g_pGameCVars->g_killercam_dofBlurAmount, true);
		}
		break;
	}

	m_postEffectsForSpecMode = specMode;

}

void CSpectatorFollowCameraMode::RevertEffects() const
{
	switch( m_postEffectsForSpecMode )
	{
	case CActor::eASM_Killer:
		{
			IRenderer* pRenderer = gEnv->pRenderer;
			CRY_ASSERT(pRenderer);

			// Revert Post Effects.
			pRenderer->EF_SetPostEffectParam("Dof_User_Active", 0.f );

			// Use the KillCam Effect for the vignette.
			CRecordingSystem *pCrs = g_pGame->GetRecordingSystem();
			CRY_ASSERT(pCrs);
			if (pCrs)
			{
				pCrs->GetKillCamGameEffect().SetActiveIfCurrentMode(CKillCamGameEffect::eKCEM_KillerCam,false);
			}
		}
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CSpectatorFixedCameraMode::UpdateView( const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime )
{
	const SPlayerStats* pPlayerStats = static_cast<const SPlayerStats*>(clientPlayer.GetActorStats());

	if (pPlayerStats->spectatorInfo.mode == CActor::eASM_Fixed)
	{
		IEntity*  pLocationEntity = gEnv->pEntitySystem->GetEntity(clientPlayer.GetSpectatorFixedLocation());
		assert(pLocationEntity);
		PREFAST_ASSUME(pLocationEntity);
		const Vec3  locationPosition = (pLocationEntity ? pLocationEntity->GetWorldPos() : viewParams.position);

		if (!IsEquivalent(locationPosition, viewParams.position))  // ie. changed camera (probably)
		{
			viewParams.position = locationPosition;
			viewParams.rotation = pLocationEntity->GetWorldRotation();
		}
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CSpectatorFixedCameraMode::CSpectatorFixedCameraMode()
{
	m_disableDrawNearest = true;
}

bool CAnimationControlledCameraMode::UpdateView( const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime )
{
	const SPlayerStats* pStats = static_cast<const SPlayerStats*>(clientPlayer.GetActorStats());

	const Matrix34& clientPlayerTM = clientPlayer.GetEntity()->GetWorldTM();
	const QuatT localEyeLocation = clientPlayer.GetCameraTran();
	bool followCharacterHead = pStats->followCharacterHead.Value() != 0;

	// If drawNearest then use "camera space" instead of "world space" 
	IEntity* pPlayerEntity = clientPlayer.GetEntity();
	if(pPlayerEntity && (pPlayerEntity->GetSlotFlags(eIGS_FirstPerson) & ENTITY_SLOT_RENDER_NEAREST))
	{
		// Camera Space
		viewParams.position = pPlayerEntity->GetWorldRotation() * localEyeLocation.t;
	}
	else
	{
		// World Space
		viewParams.position = clientPlayerTM * localEyeLocation.t;
	}

	if (followCharacterHead)
	{
		const Quat& baseQuat = clientPlayer.GetBaseQuat();
		const Quat viewQuatDeviation = !clientPlayer.IsInteractiveActionDone() ? baseQuat.GetInverted() * clientPlayer.GetViewQuatFinal() : IDENTITY;
		viewParams.rotation = baseQuat * localEyeLocation.q * viewQuatDeviation; 
	}
	else
	{
		viewParams.rotation = clientPlayer.GetViewQuatFinal();
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CDeathCameraMode::CDeathCameraMode()
{
	Init(0,0, CGameRules::EHitType::Invalid);
}

CDeathCameraMode::~CDeathCameraMode()
{

}

void CDeathCameraMode::Activate(const CPlayer& clientPlayer)
{
	const SPlayerStats*  pPlayerStats = static_cast< const SPlayerStats* >( clientPlayer.GetActorStats() );

	const CPlayer*  subject = &clientPlayer;
	const EntityId  killerEid = pPlayerStats->recentKiller;

	Init(subject, killerEid, pPlayerStats->killedByDamageType);

	// Set the Turn angle.
	float const kTurnAngleMax = DEG2RAD(m_hitType==CGameRules::EHitType::VehicleDestruction?45.f:g_pGameCVars->g_tpdeathcam_maxTurn);
	m_turnAngle = cry_random(-kTurnAngleMax, kTurnAngleMax);

	// Set the timeout.
	IActor* pKillerActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_killerEid);
	const bool wasKilled = pKillerActor && pKillerActor->IsPlayer() && (m_killerEid != m_subjectEid);
	CGameRules* pGameRules = g_pGame->GetGameRules();
	IGameRulesSpectatorModule* pSpectatorModule = pGameRules->GetSpectatorModule();
	CRY_ASSERT_MESSAGE( pSpectatorModule, "Needs a spectator module to change the mode!");

	float deathCam = 0.f;
	float killerCam = 0.f;
	float killCam = 0.f;

	pSpectatorModule->GetPostDeathDisplayDurations(m_subjectEid, clientPlayer.GetTeamWhenKilled(), !wasKilled, false, deathCam, killerCam, killCam);

	m_gotoKillerCam = wasKilled && killerCam>0.f;
	m_timeOut = deathCam + ( (float)__fsel(-killCam,0.f,0.25f) * (float)__fsel(-deathCam,0.f,1.f) );
	m_invTimeMulti = __fres((float)__fsel(0.5f-m_timeOut,3.0f,m_timeOut-0.5f));
}

void CDeathCameraMode::Init(const CPlayer* subject, const EntityId killerEid, const int hitType)
{
	m_subjectEid = (subject ? subject->GetEntityId() : 0);
	m_killerEid = ((subject && killerEid) ? killerEid : 0);
	m_hitType = hitType;
	m_prevSubWorldTM0.SetIdentity();
	m_prevSubWorldTM1.SetIdentity();
	m_firstFocusPos = ZERO;
	m_prevFocusDir = ZERO;
	m_prevLookPos = ZERO;
	m_prevFocus2camClearDist = 0.f;
	m_prevBumpUp = 0.f;
	m_updateTime = 0.f;
	m_firstUpdate = true;
	m_turnAngle = 0.0f;
	m_timeOut = 10.0f;
	m_invTimeMulti = 1.0f;
	m_gotoKillerCam = false;
}

// [tlh] TODO need to make explosion deaths focus on the explosion pos...
// [tlh] TODO? if needed, implement g_tpdeathcam_camMinDistFromPlayerAfterCollisions so collision-corrected cameras don't get too close to player (and intersect)...
bool CDeathCameraMode::UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime)
{
	bool  ret = false;

	CRY_ASSERT(m_subjectEid);
	IEntity*  subEnt = gEnv->pEntitySystem->GetEntity(m_subjectEid);
	CRY_ASSERT(subEnt);
	CActor*  subAct = (CActor*) g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_subjectEid);
	CRY_ASSERT(subAct);
	IEntity*  kilEnt = gEnv->pEntitySystem->GetEntity(m_killerEid);

	if (subAct->IsDead())
	{
		// Drop out.
		if( m_updateTime > m_timeOut )
		{
			CGameRules *pGameRules = g_pGame->GetGameRules();
			IGameRulesSpectatorModule *pSpectatorModule = pGameRules->GetSpectatorModule();
			CRY_ASSERT_MESSAGE( pSpectatorModule, "Needs a spectator module to change the mode!");

			if( m_gotoKillerCam && (gEnv->pEntitySystem->GetEntity(m_killerEid) != NULL) )
			{
				pSpectatorModule->ChangeSpectatorMode(subAct,CActor::eASM_Killer,m_killerEid,true);
			}
			else
			{
				pSpectatorModule->ChangeSpectatorModeBestAvailable(subAct,true);
			}
			return false;
		}
		
		m_updateTime += frameTime;
	}

	const float  kEpsilon = g_pGameCVars->g_tpdeathcam_collisionEpsilon;
	const float  kCamRadius = g_pGameCVars->g_tpdeathcam_camCollisionRadius;

	if (m_firstUpdate)
	{
		m_prevSubWorldTM0 = m_prevSubWorldTM1 = subEnt->GetWorldTM();
	}

	Vec3  idealFocusPos;

	const QuatT&  boneQuat = subAct->GetBoneTransform(BONE_SPINE2);
	Matrix34  boneWorldMtx = (m_prevSubWorldTM0 * Matrix34(boneQuat));  // [tlh] this may seem like madness, but we need to use the entity's world transform matrix from *2* frames ago when transforming the bone into world space, otherwise there's a big 2-frame glitch at the point where the entity tranform's rotations are reset when ragdoll is activated
	idealFocusPos = boneWorldMtx.GetTranslation();

	idealFocusPos.z += g_pGameCVars->g_tpdeathcam_camHeightTweak;
	if(m_hitType==CGameRules::EHitType::VehicleDestruction)
	{
		idealFocusPos.z += 2.f;
	}

	Vec3  safeFocusPos = subEnt->GetWorldPos();
	safeFocusPos.z = idealFocusPos.z;

	Vec3  focusPos = idealFocusPos;

	PhysSkipList skipList;
	skipList.clear();

	// build up skip list for the intersection tests
	{
		stl::push_back_unique(skipList, subEnt->GetPhysics());

		if (IItem* pItem=clientPlayer.GetCurrentItem())
		{
			if (CWeapon* pWeapon=(CWeapon*)pItem->GetIWeapon())
			{
				CSingle::GetSkipEntities(pWeapon, skipList);
			}
		}
	}

	// do a test between the "safe" and "ideal" focus points to find the best pos that's not through a wall (in theory)
	{
		Vec3  sphereDir (idealFocusPos - safeFocusPos);
		if (!sphereDir.IsZero())
		{
			Vec3  sphereDirN = sphereDir.GetNormalized();
			primitives::sphere  sphere;
			sphere.r = kCamRadius;
			sphere.center = (safeFocusPos - (sphereDirN * (sphere.r + kEpsilon)));  // pull the start pos "back" a bit in case it's already intersecting with a wall (because in that case the collision will not happen)
			sphereDir = (idealFocusPos - sphere.center);  // calc again because start pos has changed
			geom_contact*  pContact = NULL;
			float  hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, sphereDir, (ent_static | ent_terrain | ent_rigid), &pContact, 0, (rwi_ignore_noncolliding | rwi_stop_at_pierceable), 0, 0, 0, !skipList.empty() ? &skipList[0] : NULL, skipList.size());
			if (hitDist > 0.f)
			{
				focusPos = (sphere.center + (sphereDirN * (hitDist - kEpsilon)));
			}
		}
	}


	enum
	{
		eLPT_Null = 0,
		eLPT_AtKiller,
		eLPT_InFront,
		eLPT_PrevFramePos,
	}
	lookPosType = eLPT_Null;

	// work out what type of look pos to use depending on how the kill happened
	{
		// [pb] Disabled for now, as it causes more jerkiness and barely gives you a view of your killer.
		//if (kilEnt && (kilEnt != subEnt))
		//{
		//	lookPosType = eLPT_AtKiller;
		//}
		//else
		{
			if (m_firstUpdate)
			{
				lookPosType = eLPT_InFront;
			}
			else
			{
				lookPosType = eLPT_PrevFramePos;

#ifndef _RELEASE
				if (g_pGameCVars->g_tpdeathcam_dbg_alwaysOn)
				{
					lookPosType = eLPT_InFront;
				}
#endif
			}
		}
	}

	CRY_ASSERT(lookPosType != eLPT_Null);

	Vec3  lookPos (FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3  camPos (FLT_MAX, FLT_MAX, FLT_MAX);

	// calculate the look pos and (desired) cam pos based on the type of look
	{
		Vec3  dir (0.f, 1.f, 0.f);

		switch (lookPosType)
		{
		case eLPT_AtKiller:
			{
				CRY_ASSERT(kilEnt);

				AABB  kilBounds;
				kilEnt->GetWorldBounds(kilBounds);

				lookPos = kilBounds.GetCenter();

				dir = (lookPos - focusPos);
				dir.z = 0.f;
				dir.NormalizeFast();

				break;
			}
		case eLPT_InFront:
			{
				dir = (subEnt->GetRotation() * dir);
				dir.z = 0.f;
				dir.NormalizeFast();

				lookPos = (focusPos + (g_pGameCVars->g_tpdeathcam_lookDistWhenNoKiller * dir));

				break;
			}
		case eLPT_PrevFramePos:
			{
				lookPos = m_prevLookPos;

				dir = (lookPos - focusPos);
				dir.z = 0.f;
				dir.NormalizeFast();

				break;
			}
		default:
			{
				CRY_ASSERT(0);
				break;
			}
		}

		CRY_ASSERT((lookPos.x < FLT_MAX) && (lookPos.y < FLT_MAX) && (lookPos.z < FLT_MAX));

		// Apply Turn.
		const float timePerc = (m_updateTime*m_invTimeMulti);
		const float smoothTimePerc = SmoothBlendValue(timePerc);
		const float additionalAng = m_turnAngle * smoothTimePerc;

		const float initialDist = g_pGameCVars->g_tpdeathcam_camDistFromPlayerStart;
		const float finalDist = (float)__fsel((g_pGameCVars->g_tpdeathcam_camDistFromPlayerEnd)-g_pGameCVars->g_tpdeathcam_camDistFromPlayerMin,g_pGameCVars->g_tpdeathcam_camDistFromPlayerEnd,g_pGameCVars->g_tpdeathcam_camDistFromPlayerMin);
		const float distance = LERP(initialDist,finalDist,smoothTimePerc)*(m_hitType==CGameRules::EHitType::VehicleDestruction?5.f:1.f);

		const Matrix33 rotMat(Matrix33::CreateRotationZ(additionalAng));
		dir = rotMat.TransformVector(dir);
		camPos = (focusPos - (distance * dir));

		if (!m_firstUpdate)
		{
			camPos.z = (float)__fsel(m_firstFocusPos.z-focusPos.z,m_firstFocusPos.z,camPos.z);
		}
	}

	Vec3  groundFocusPos = focusPos;

	// do small reverse test towards the focus pos (from direction of the cam pos) to find a pos sufficiently clear of the ground for the main focus-to-cam test
	{
		Vec3  testDir (groundFocusPos - camPos);

		if (!testDir.IsZero())
		{
			Vec3  testDirN = testDir.GetNormalized();

			primitives::sphere  sphere;
			sphere.r = kCamRadius;

			const float  testLen = g_pGameCVars->g_tpdeathcam_directionalFocusGroundTestLen;
			testDir = (testDirN * testLen);

			sphere.center = (groundFocusPos - testDir);

			// if needed, could possibly do filtering on testLen here so lens don't vary by too large an amount from frame-to-frame (see usage of g_tpdeathcam_testLenIncreaseRestriction later in this func)

			geom_contact*  pContact = NULL;
			float  hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, testDir, (ent_static | ent_terrain | ent_rigid), &pContact, 0, (rwi_ignore_noncolliding | rwi_stop_at_pierceable), 0, 0, 0, !skipList.empty() ? &skipList[0] : NULL, skipList.size());

			if (hitDist > 0.f)
			{
				groundFocusPos = (sphere.center + (testDirN * (hitDist - kEpsilon)));
			}
		}
	}

	float  bumpUp = 0.f;
	float  focus2camClearDist = 0.f;

	// do the main test from focus pos to cam pos, additionally calculating a height bump for the cam pos if there's a collision
	{
		primitives::sphere  sphere;
		sphere.center = groundFocusPos;
		sphere.r = kCamRadius;

		Vec3  testDir (camPos - sphere.center);
		float  testLen = testDir.GetLength();

		if (testLen != 0.f)
		{
			Vec3  testDirN = (testDir / testLen);

			if (!m_firstUpdate)
			{
				if ((testLen - m_prevFocus2camClearDist) > g_pGameCVars->g_tpdeathcam_testLenIncreaseRestriction)
				{
					// limiting the length of this test to the length of the previous frame's result plus a-little-bit-more - as an attempt to limit judderyness
					testLen = (m_prevFocus2camClearDist + g_pGameCVars->g_tpdeathcam_testLenIncreaseRestriction);
					camPos = (sphere.center + (testDirN * testLen));
					testDir = (camPos - sphere.center);
				}
			}

			geom_contact*  pContact = NULL;
			float  hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, testDir, (ent_static | ent_terrain | ent_rigid), &pContact, 0, (rwi_ignore_noncolliding | rwi_stop_at_pierceable), 0, 0, 0, !skipList.empty() ? &skipList[0] : NULL, skipList.size());

			if (hitDist > 0.f)
			{
				camPos = (sphere.center + (testDirN * (hitDist - kEpsilon)));

				// If the hitDist is too close to the player, bump up the cam.
				if( hitDist < g_pGameCVars->g_tpdeathcam_camDistFromPlayerMin )
				{
					float  dirFrac = (hitDist / testLen);
					bumpUp = (g_pGameCVars->g_tpdeathcam_maxBumpCamUpOnCollide * (1.f - dirFrac));
				}

				focus2camClearDist = hitDist;
			}
			else
			{
				focus2camClearDist = testLen;
			}
		}
	}

	// "bumpUp" can't be less than "m_prevBumpUp".
	bumpUp = (float)__fsel(bumpUp-m_prevBumpUp,bumpUp,m_prevBumpUp);

	camPos.z += bumpUp;

	Vec3  focusDir = (focusPos - camPos);

	if (m_firstUpdate)
	{
		SetInternalPositions( camPos, focusPos );
	}
	else
	{
		PreventVerticalGimbalIssues( focusPos, focusDir, camPos );
		UpdateInternalPositions( camPos, focusPos, g_pGameCVars->g_tpdeathcam_camSmoothSpeed, viewParams.frameTime );
	}

	ApplyResults(viewParams);
	
	if (m_firstUpdate)
	{
		m_firstFocusPos = focusPos;
	}

	m_prevSubWorldTM0 = m_prevSubWorldTM1;
	m_prevSubWorldTM1 = subEnt->GetWorldTM();
	m_prevFocusDir = focusDir;
	m_prevLookPos = lookPos;
	m_prevFocus2camClearDist = focus2camClearDist;
	m_prevBumpUp = bumpUp;
	m_firstUpdate = false;
	return ret;
}

void CDeathCameraMode::PreventVerticalGimbalIssues( Vec3& focusPos, Vec3& focusDir, const Vec3& camPos ) const
{
	// detect whether the camera direction is going to be close to vertical down, in which case fudge it to use previous values otherwise the camera angle calculations freak out
	const Vec3 focusDirN = focusDir.GetNormalized();
	const float fabsFocusDirNz = fabsf(focusDirN.z);
	if ((fabsFocusDirNz > fabsf(focusDirN.x)) && (fabsFocusDirNz > fabsf(focusDirN.y)) && (fabsFocusDirNz > g_pGameCVars->g_tpdeathcam_zVerticalLimit) )
	{
		focusDir = m_prevFocusDir;
		focusPos = (camPos + focusDir);
	}
}

void CDeathCameraMode::UpdateInternalPositions( const Vec3& eye, const Vec3& lookAt, const float lerpSpeed, const float frameTime )
{
	Interpolate( m_cameraEye, eye, lerpSpeed, frameTime );
	Interpolate( m_cameraLookAt, lookAt, lerpSpeed, frameTime );
}

void CDeathCameraMode::ApplyResults( SViewParams& viewParams )
{
	// Position
	viewParams.position = m_cameraEye;

	// Rotation
	const Vec3 dirN( (m_cameraLookAt-m_cameraEye).GetNormalizedSafe() );
	viewParams.rotation = Quat(Matrix33::CreateRotationVDir(dirN));	

	// FOV
	//viewParams.fov = g_pGameCVars->cl_fov * m_lastFOVScale * (gf_PI/180.0f);  // [tlh] TODO? this needed?
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CDeathCameraModeSinglePlayer::CDeathCameraModeSinglePlayer() : m_params(g_pGameCVars->g_deathCamSP)
{
	Init(NULL, 0);
}

CDeathCameraModeSinglePlayer::~CDeathCameraModeSinglePlayer()
{

}

void CDeathCameraModeSinglePlayer::Activate(const CPlayer& clientPlayer)
{
	const SPlayerStats*  pPlayerStats = static_cast< const SPlayerStats* >( clientPlayer.GetActorStats() );

	const CPlayer*  subject = &clientPlayer;
	const EntityId  killerEid = pPlayerStats->recentKiller;

	Init(subject, killerEid);

	gEnv->p3DEngine->SetPostEffectParam("Dof_Active", m_params.dof_enable ? 1.0f : 0.0f);
}

void CDeathCameraModeSinglePlayer::Deactivate(const CPlayer& clientPlayer)
{
	gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0.0f);
}

void CDeathCameraModeSinglePlayer::Init(const CPlayer* subject, const EntityId killerEid)
{
	m_subjectEid = (subject ? subject->GetEntityId() : 0);
	m_killerEid = ((subject && killerEid) ? killerEid : 0);

	m_fUpdateCounter = m_params.updateFrequency;
	m_fKillerHeightOffset = 0.0f;
	m_bIsKillerInFrustrum = false;

	m_fFocusRange = m_params.dofRangeNoKiller;
	m_fFocusDistance = 0.0f;

	// Cache the height
	IEntity* pKilEnt = gEnv->pEntitySystem->GetEntity(killerEid);
	if (pKilEnt)
	{
		AABB localBounds; 
		pKilEnt->GetLocalBounds(localBounds);
		m_fKillerHeightOffset = localBounds.GetCenter().z;
	}
}

bool CDeathCameraModeSinglePlayer::UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	m_fUpdateCounter -= frameTime;

	bool bRet = ParentMode::UpdateView(clientPlayer,viewParams, frameTime);

	float fDesiredDistance = 0.0f;
	float fDesiredFocusRange = m_params.dofRangeNoKiller;
	if (m_killerEid != m_subjectEid)
	{
		IEntity* pKilEnt = gEnv->pEntitySystem->GetEntity(m_killerEid);
		if (pKilEnt)
		{
			// Update cached values after m_fUpdateCounter seconds only
			const Vec3& vKillerPos = pKilEnt->GetWorldPos() + Vec3(0.f, 0.f, m_fKillerHeightOffset);
			if (m_fUpdateCounter <= 0.0f)
			{
				m_fUpdateCounter = m_params.updateFrequency;

				AABB localBounds; 
				pKilEnt->GetLocalBounds(localBounds);
				m_fKillerHeightOffset = localBounds.GetCenter().z;

				m_bIsKillerInFrustrum = GetISystem()->GetViewCamera().IsPointVisible(vKillerPos);
			}

			CPlayerVisTable::SVisibilityParams queryTargetParams(m_killerEid);
			queryTargetParams.heightOffset = m_fKillerHeightOffset;
			if (g_pGame->GetPlayerVisTable()->CanLocalPlayerSee(queryTargetParams, 10) && m_bIsKillerInFrustrum)
			{
				// Update DOF distance
				fDesiredDistance = (vKillerPos - viewParams.position).GetLength();
				fDesiredFocusRange = m_params.dofRange;

				// [*DavidR | 23/May/2010] Should we look at the killer too?
			}
		}
	}

	// Apply dof values
	if (m_params.dof_enable)
	{
		// Approximate distance
		float fDelta = fDesiredDistance - m_fFocusDistance;
		m_fFocusDistance += static_cast<float>(__fsel(fDelta, 1.0f, -1.0f) * min(fabs_tpl(fDelta), (m_params.dofDistanceSpeed * frameTime)));

		// Approximate range
		fDelta = fDesiredFocusRange - m_fFocusRange;
		m_fFocusRange += static_cast<float>(__fsel(fDelta, 1.0f, -1.0f) * min(fabs_tpl(fDelta), (m_params.dofRangeSpeed * frameTime)));


		gEnv->p3DEngine->SetPostEffectParam("Dof_FocusDistance", m_fFocusDistance);
		gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", m_fFocusRange);
	}

	// [*DavidR | 23/May/2010] ToDo: More dof-based effects here? (blurriness amount oscillation?)

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CVehicleCameraMode::UpdateView( const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime )
{
	if (IVehicle* pVehicle = clientPlayer.GetLinkedVehicle())
	{
		pVehicle->UpdateView(viewParams, clientPlayer.GetEntityId());

		bool isFirstPerson = !clientPlayer.IsThirdPerson();

		if (isFirstPerson)
		{
			const float cameraFactor = 0; 

			IEntity* pEntity = clientPlayer.GetEntity();
			uint32 entityFlags = (pEntity) ? pEntity->GetSlotFlags(eIGS_FirstPerson) : 0;

			bool worldSpace = !(entityFlags & ENTITY_SLOT_RENDER_NEAREST);
			
			IItem* pCurrentItem = clientPlayer.GetCurrentItem();
			CCameraPose firstPersonCameraPose = CCameraPose(
				clientPlayer.GetFPCameraPosition(true),
				viewParams.rotation);
			firstPersonCameraPose = CCameraPose::Compose(
				firstPersonCameraPose,
				MountedGunView(clientPlayer, pCurrentItem, firstPersonCameraPose));
			const CCameraPose vehicleCameraPose = CCameraPose(
				viewParams.position,
				viewParams.rotation);
			const CCameraPose baseCameraPose = CCameraPose::Lerp(
				vehicleCameraPose,
				firstPersonCameraPose,
				cameraFactor);

			const CCameraPose animatedFirstPersonCamera = 
				(cameraFactor > 0.0f) ? 
					FirstPersonCameraAnimation(clientPlayer, baseCameraPose, cameraFactor) :
					baseCameraPose;

			if (worldSpace)
			{
				viewParams.position = animatedFirstPersonCamera.GetPosition();
			}
			else
			{
				viewParams.position = animatedFirstPersonCamera.GetPosition() - clientPlayer.GetEntity()->GetWorldPos();
			}
			viewParams.rotation = animatedFirstPersonCamera.GetRotation();

			viewParams.fov = g_pGame->GetFOV() * clientPlayer.GetActorParams().viewFoVScale * (gf_PI/180.0f);

#ifndef _RELEASE
			if (g_pGameCVars->pl_debug_view)
			{
				CryWatch("VehicleCamera AnimFactor (%.2f)", cameraFactor);
				CryWatch("VehicleCameraFP Pos (%.2f, %.2f, %.2f)", firstPersonCameraPose.GetPosition().x, firstPersonCameraPose.GetPosition().y, firstPersonCameraPose.GetPosition().z);
				CryWatch("VehicleCameraFP Rot (%.2f, %.2f, %.2f, %.2f)", firstPersonCameraPose.GetRotation().v.x, firstPersonCameraPose.GetRotation().v.y, firstPersonCameraPose.GetRotation().v.z, firstPersonCameraPose.GetRotation().w);
				CryWatch("VehicleCameraVP Pos (%.2f, %.2f, %.2f)", vehicleCameraPose.GetPosition().x, vehicleCameraPose.GetPosition().y, vehicleCameraPose.GetPosition().z);
				CryWatch("VehicleCameraVP Rot (%.2f, %.2f, %.2f, %.2f)", vehicleCameraPose.GetRotation().v.x, vehicleCameraPose.GetRotation().v.y, vehicleCameraPose.GetRotation().v.z, vehicleCameraPose.GetRotation().w);
				CryWatch("VehicleCameraBase Pos (%.2f, %.2f, %.2f)", baseCameraPose.GetPosition().x, baseCameraPose.GetPosition().y, baseCameraPose.GetPosition().z);
				CryWatch("VehicleCameraBase Rot (%.2f, %.2f, %.2f, %.2f)", baseCameraPose.GetRotation().v.x, baseCameraPose.GetRotation().v.y, baseCameraPose.GetRotation().v.z, baseCameraPose.GetRotation().w);
				CryWatch("VehicleCamera Pos (%.2f, %.2f, %.2f)", viewParams.position.x, viewParams.position.y, viewParams.position.z);
				CryWatch("VehicleCamera Rot (%.2f, %.2f, %.2f, %.2f)", viewParams.rotation.v.x, viewParams.rotation.v.y, viewParams.rotation.v.z, viewParams.rotation.w);
			}
#endif //_RELEASE
		}
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CPartialAnimationControlledCameraMode::CPartialAnimationControlledCameraMode()
	: m_currentOrientationFactor(0.0f)
	, m_currentPositionFactor(0.0f)
	, m_lastQuatBeforeDeactivation(IDENTITY, ZERO)
{
	m_animationSettings.positionFactor = 1.0f;
	m_animationSettings.rotationFactor = 1.0f;
}

CPartialAnimationControlledCameraMode::~CPartialAnimationControlledCameraMode()
{

}

void CPartialAnimationControlledCameraMode::Activate(const CPlayer& clientPlayer)
{
	m_currentPositionFactor = 0.0f;
	m_currentOrientationFactor = 0.0f;
	m_animationSettings.applyResult = false;
}

bool CPartialAnimationControlledCameraMode::UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime)
{
	const Matrix34& clientPlayerTM = clientPlayer.GetEntity()->GetWorldTM();
	QuatT localReferenceEyeLocation = clientPlayer.GetCameraTran();
	const bool isThirdPerson = clientPlayer.IsThirdPerson();

	viewParams.blend = true;

	viewParams.rotation = clientPlayer.GetViewQuatFinal();

	if(isThirdPerson)
	{
		const Vec3 view3rdOffset =	(viewParams.rotation.GetColumn1() * -g_pGameCVars->cl_tpvDist) +  
																(clientPlayer.GetBaseQuat() * clientPlayer.GetStanceViewOffset(clientPlayer.GetStance()));
		viewParams.position = clientPlayerTM.GetTranslation() + view3rdOffset;
	}
	else
	{
		if (!IsBlendingOff())
		{
			const float blendOrientationFactor = GetBlendOrientationFactor();
			const float blendPositionFactor = GetBlendPositionFactor();

			m_currentPositionFactor = blendPositionFactor;
			m_currentOrientationFactor = blendOrientationFactor;
			m_lastQuatBeforeDeactivation = localReferenceEyeLocation;
		}
		else if(m_animationSettings.stableBlendOff)
		{
			//Use the last stored value to have a stable reference to blend off to the next camera
			localReferenceEyeLocation = m_lastQuatBeforeDeactivation; 
		}

		const Quat worldEyeOrientation = clientPlayer.GetEntity()->GetWorldRotation() * localReferenceEyeLocation.q;
		const Quat localChangeQuat = !viewParams.rotation * worldEyeOrientation;

		viewParams.localRotationLast = Quat::CreateSlerp(IDENTITY, localChangeQuat, m_currentOrientationFactor);
		viewParams.rotation = viewParams.rotation * viewParams.localRotationLast;
		viewParams.rotation.Normalize();

		// If drawNearest then use "camera space" instead of "world space" 
		bool bWorldSpace = true;
		Vec3 eyeLocation(ZERO);
		IEntity* pPlayerEntity = clientPlayer.GetEntity();
		if(pPlayerEntity && (pPlayerEntity->GetSlotFlags(eIGS_FirstPerson) & ENTITY_SLOT_RENDER_NEAREST))
		{
			// Camera Space
			bWorldSpace = false;
			eyeLocation = (pPlayerEntity->GetWorldRotation() * localReferenceEyeLocation.t);
		}
		else
		{
			// World Space
			eyeLocation = clientPlayerTM.TransformPoint(localReferenceEyeLocation.t);
		}

		Vec3 fpCamPos = clientPlayer.GetFPCameraPosition(bWorldSpace);
		if(!bWorldSpace)
		{
			fpCamPos = clientPlayer.GetEntity()->GetWorldRotation() * fpCamPos;			
		}
		const Vec3 desiredWorldCameraPos = LERP(fpCamPos,eyeLocation, m_currentPositionFactor);
		viewParams.position = desiredWorldCameraPos;

#ifndef _RELEASE
		if (g_pGameCVars->pl_debug_view)
		{
			Vec3 fwd = clientPlayer.GetViewQuatFinal().GetColumn1();
			Vec3 wldFwd = clientPlayer.GetEntity()->GetWorldRotation().GetColumn1();
			CryWatch("ViewQuatFinal Fwd (%f, %f, %f) Wrld fwd (%f, %f, %f)", fwd.x, fwd.y, fwd.z, wldFwd.x, wldFwd.y, wldFwd.z);
			CryWatch("PartialAnimControl Pos (%.2f, %.2f, %.2f) Blend(%.2f)", viewParams.position.x, viewParams.position.y, viewParams.position.z, m_currentPositionFactor);
			CryWatch("PartialAnimControl Rot (%.2f, %.2f, %.2f, %.2f) Blend(%.2f)", viewParams.rotation.v.x, viewParams.rotation.v.y, viewParams.rotation.v.z, viewParams.rotation.w, m_currentOrientationFactor);
			fwd = viewParams.rotation.GetColumn1();
			CryWatch("PartialAnimControl Fwd (%f, %f, %f)", fwd.x, fwd.y, fwd.z);
		}
#endif //!_RELEASE
	}

	return m_animationSettings.applyResult;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
