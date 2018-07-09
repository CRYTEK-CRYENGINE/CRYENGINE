// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Helper class for melee collision detection

-------------------------------------------------------------------------
History:
- 10:03:2010   Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "MeleeCollisionHelper.h"

#include "Game.h"
#include "GameRules.h"
#include "AutoAimManager.h"
#include "Actor.h"
#include "ActorImpulseHandler.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include <CryAnimation/ICryAnimation.h>

void CMeleeCollisionHelper::OnRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result )
{
	CRY_ASSERT(rayID == m_queuedRayID);

	m_queuedRayID = 0;

	if (result.hitCount > 0)
	{
		const ray_hit &hit = result.hits[0];
		CCCPOINT(Melee_HitSomethingWithRayCast);
		OnSuccesfulHit(hit);
	}
	else
	{
		CCCPOINT(Melee_SwingAndAMiss);
		OnFailedHit();
	}
}

void CMeleeCollisionHelper::DoCollisionTest( const SCollisionTestParams& requestInfo )
{
	m_testParams = requestInfo;

	IEntity *pIgnoredEntity = gEnv->pEntitySystem->GetEntity(requestInfo.m_ignoredEntityIds[0]);
	IEntity *pIgnoredEntity2 = gEnv->pEntitySystem->GetEntity(requestInfo.m_ignoredEntityIds[1]);
	int ignoreCount = 0;
	IPhysicalEntity *pIgnoredEntityPhysics[2] = { NULL, NULL };
	if (pIgnoredEntity)
	{
		pIgnoredEntityPhysics[ignoreCount] = pIgnoredEntity->GetPhysics();
		ignoreCount += pIgnoredEntityPhysics[ignoreCount] ? 1 : 0;
	}
	if (pIgnoredEntity2)
	{
		pIgnoredEntityPhysics[ignoreCount] = pIgnoredEntity2->GetPhysics();
		ignoreCount += pIgnoredEntityPhysics[ignoreCount] ? 1 : 0;
	}

	assert(m_queuedRayID == 0);

	CancelPendingRay();

	m_queuedRayID = g_pGame->GetRayCaster().Queue(
		requestInfo.m_remote ? RayCastRequest::MediumPriority : RayCastRequest::HighPriority,
		RayCastRequest(requestInfo.m_pos, requestInfo.m_dir * requestInfo.m_distance,
		ent_all|ent_water,
		rwi_stop_at_pierceable|rwi_ignore_back_faces,
		pIgnoredEntityPhysics,
		ignoreCount),
		functor(*this, &CMeleeCollisionHelper::OnRayCastDataReceived));
}

EntityId CMeleeCollisionHelper::GetBestAutoAimTargetForUser( EntityId userId,  const Vec3& scanPosition, const Vec3& scanDirection, float range, float angle ) const
{
	CGameRules* pGameRules = g_pGame->GetGameRules();
	int playerTeam = pGameRules->GetTeam(userId);

	const TAutoaimTargets& aaTargets = g_pGame->GetAutoAimManager().GetAutoAimTargets();
	const int targetCount = aaTargets.size();

	EntityId nearestTargetId = 0;
	float nearestEnemyDst = range + 0.1f;

	for (int i = 0; i < targetCount; ++i)
	{
		const SAutoaimTarget& target = aaTargets[i];

		CRY_ASSERT(target.entityId != userId);

		//Skip friendly ai
		if (!gEnv->bMultiplayer && (target.HasFlagSet(eAATF_AIHostile) == false))
			continue;

		//Skip team mates
		if (gEnv->bMultiplayer && playerTeam != 0 && pGameRules->GetTeam(target.entityId) == playerTeam)
			continue;

		//If not an actor
		if(target.pActorWeak.expired())
			continue;

		//If not in a vehicle
		CActor* pActor = target.pActorWeak.lock().get();
		if(pActor->GetLinkedVehicle())
			continue;

		const Vec3 targetPos = target.primaryAimPosition;
		Vec3 targetDir = (targetPos - scanPosition);
		const float distance = targetDir.NormalizeSafe();

		// Reject if the enemy is not near enough or too far away
		if ((distance >= nearestEnemyDst) || (distance > range))
			continue;

		// Reject if not inside the view cone
		float alignment = scanDirection.Dot(targetDir);
		if (alignment < angle)
			continue;

		// We have a new candidate
		nearestEnemyDst = distance;
		nearestTargetId = target.entityId;

	}

	return nearestTargetId;
}

void CMeleeCollisionHelper::Impulse( IPhysicalEntity *pCollider, const Vec3 &position, const Vec3 &impulse, int partId, int ipart, int hitTypeID )
{
	assert(pCollider);

	IEntity * pEntity = (IEntity*) pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

	if (pEntity)
	{
		CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
		if(pActor)
		{
			//--- The push impulse is now disabled on MP, excepting for client melees
			const float fPushImpulseScale = !gEnv->bMultiplayer || (pActor->IsClient() && (hitTypeID == CGameRules::EHitType::Melee)) ? 1.0f : 0.0f;
			pActor->AddLocalHitImpulse(SHitImpulse(partId, -1, position, impulse, fPushImpulseScale));
		}
		else
		{
			pEntity->AddImpulse(partId, position, impulse, true, 1.0f, 1.0f);
		}
	}
	else
	{
		pe_action_impulse ai;
		ai.partid = partId;
		ai.point = position;
		ai.impulse = impulse;
		pCollider->Action(&ai);
	}
}

void CMeleeCollisionHelper::GenerateArtificialCollision( IEntity* pUser, IPhysicalEntity *pCollider, const Vec3 &position, const Vec3& normal, const Vec3 &speed, int partId, int ipart, int surfaceIdx, int iPrim )
{
	ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	int matPlayerColliderId = pSurfaceTypeManager->GetSurfaceTypeByName("mat_player_collider")->GetId();

	// create a physical collision to break trees
	pe_action_register_coll_event collision;

	collision.collMass = 0.005f; // this is actually ignored
	collision.partid[1] = partId;

	// collisions involving partId<-1 are to be ignored by game's damage calculations
	// usually created articially to make stuff break.
	collision.partid[0] = -2;
	collision.idmat[1] = surfaceIdx;
	collision.idmat[0] = matPlayerColliderId;
	collision.n = normal;
	collision.pt = position;
	collision.vSelf = speed;
	collision.v = Vec3(0,0,0);
	collision.pCollider = pCollider;
	collision.iPrim[0] = -1;
	collision.iPrim[1] = iPrim;

	ICharacterInstance *pUserCharacter = pUser ? pUser->GetCharacter(0) : NULL;
	ISkeletonPose* pSkeletonPose = pUserCharacter ? pUserCharacter->GetISkeletonPose() : NULL;
	IPhysicalEntity* pCharacterPhysics = pSkeletonPose ? pSkeletonPose->GetCharacterPhysics() : NULL;
	if (pCharacterPhysics)
	{
		pCharacterPhysics->Action(&collision);
	}
}

bool CMeleeCollisionHelper::PerformMeleeOnAutoTarget( EntityId targetId )
{
	const SAutoaimTarget* pAutoTarget = g_pGame->GetAutoAimManager().GetTargetInfo(targetId);
	IEntity* pAutoTargetEntity = gEnv->pEntitySystem->GetEntity(targetId);

	if(!pAutoTarget || !pAutoTargetEntity)
		return false;

	CActorPtr pActor = pAutoTarget->pActorWeak.lock();
	if(!pActor)
		return false;

	ICharacterInstance* pCharacter = pAutoTargetEntity->GetCharacter(0);
	ISkeletonPose* pSkeletonPose = pCharacter ? pCharacter->GetISkeletonPose() : NULL;
	IPhysicalEntity* pTargetPhysics =  pSkeletonPose ? pSkeletonPose->GetCharacterPhysics() : NULL;
	if (!pTargetPhysics)
		return false;

	int jointID = pActor->GetBoneID(pAutoTarget->physicsBoneId);

	pe_params_part partParams;
	partParams.partid = jointID;
	int iPart = -1;
	int surfaceIdx = 0;
	if (pTargetPhysics->GetParams(&partParams))
	{
		phys_geometry* pGeom = partParams.pPhysGeomProxy ? partParams.pPhysGeomProxy : partParams.pPhysGeom;
		int geomSurfaceIdx = pGeom ? pGeom->surface_idx : -1;
		iPart = partParams.ipart;
		if (geomSurfaceIdx >= 0 &&  geomSurfaceIdx < partParams.nMats)
		{
			surfaceIdx = partParams.pMatMapping[geomSurfaceIdx];   
		}
	}

	ray_hit hit;
	hit.pt = pAutoTarget->primaryAimPosition;
	hit.n = -m_testParams.m_dir;
	hit.pCollider = pTargetPhysics;
	hit.surface_idx = surfaceIdx;
	hit.partid = jointID;
	hit.ipart = iPart;

	CCCPOINT(Melee_SuccessfullyHitAutoTarget);
	OnSuccesfulHit(hit);

	return true;
}

void CMeleeCollisionHelper::CancelPendingRay()
{
	if (m_queuedRayID != 0)
	{
		g_pGame->GetRayCaster().Cancel(m_queuedRayID);
	}
	m_queuedRayID = 0;
}
