// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   CAISystemPhys.cpp
   $Id$
   Description:	should contaioin all the methods of CAISystem which have to deal with Physics

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Kirill Bulatsev


 *********************************************************************/

#include "StdAfx.h"
#include "CAISystem.h"
#include "Puppet.h"

//===================================================================
// GetWaterOcclusionValue
//===================================================================
float CAISystem::GetWaterOcclusionValue(const Vec3& targetPos) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	float fResult = 0.0f;

	if (gAIEnv.CVars.WaterOcclusionEnable > 0)
	{
		const float waterLevel = gEnv->p3DEngine->GetWaterLevel(&targetPos);
		const float inWaterDepth = waterLevel - targetPos.z;

		if (inWaterDepth > FLT_EPSILON)
		{
			IPhysicalEntity** areas;
			int areaCount = GetPhysicalEntitiesInBox(targetPos, targetPos, areas, ent_areas);

			for (int i = 0; i < areaCount; ++i)
			{
				pe_params_buoyancy pb;

				// if this is a water area (there can be only one!1)
				if (areas[i]->GetParams(&pb) && !is_unused(pb.waterPlane.origin))
				{
					pe_status_contains_point scp;
					scp.pt = targetPos;

					if (areas[i]->GetStatus(&scp))
					{
						const float areaWaterLevel = targetPos.z - pb.waterPlane.n.z *
						                             (pb.waterPlane.n * (targetPos - pb.waterPlane.origin));
						const float areaInWaterDepth = areaWaterLevel - targetPos.z;

						if (areaInWaterDepth > FLT_EPSILON)
						{
							IWaterVolumeRenderNode* waterRenderNode((IWaterVolumeRenderNode*)areas[i]
							                                        ->GetForeignData(PHYS_FOREIGN_ID_WATERVOLUME));

							if (waterRenderNode)
							{
								const float waterFogDensity = waterRenderNode->GetFogDensity();
								const float resValue = gAIEnv.CVars.WaterOcclusionScale * exp(-waterFogDensity * inWaterDepth);

								// make sure it's in 0-1 range
								fResult = 1.0f - min(1.0f, resValue);
							}
							else
							{
								// no render node if in the ocean
								// make sure it's in 0-1 range
								const float resValue = gAIEnv.CVars.WaterOcclusionScale * inWaterDepth * (1.0f / 6.0f);
								fResult = min(1.0f, resValue);
							}
						}
						break;
					}
				}
			}
		}
	}

	return fResult;
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::CheckPointsVisibility(const Vec3& from, const Vec3& to, float rayLength, IPhysicalEntity* pSkipEnt, IPhysicalEntity* pSkipEntAux)
{
	Vec3 dir = to - from;
	if (rayLength && dir.GetLengthSquared() > rayLength * rayLength)
		dir *= rayLength / dir.GetLength();

	IPhysicalEntity* skipList[2] =
	{
		pSkipEnt,
		pSkipEntAux,
	};

	uint skipListSize = 0;
	if (!skipList[0])
	{
		skipList[0] = skipList[1];
		skipList[1] = 0;
	}

	skipListSize += (skipList[0] != 0) ? 1 : 0;
	skipListSize += (skipList[1] != 0) ? 1 : 0;

	return !gAIEnv.pRayCaster->Cast(
	  RayCastRequest(
	    from, dir, COVER_OBJECT_TYPES,
	    AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
	    skipList, skipListSize));
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::CheckObjectsVisibility(const IAIObject* pObj1, const IAIObject* pObj2, float rayLength)
{
	Vec3 dir = pObj2->GetPos() - pObj1->GetPos();
	if (rayLength && dir.GetLengthSquared() > sqr(rayLength))
		dir *= rayLength / dir.GetLength();

	PhysSkipList skipList;
	if (const CAIActor* pActor = pObj1->CastToCAIActor())
		pActor->GetPhysicalSkipEntities(skipList);
	if (const CAIActor* pActor = pObj2->CastToCAIActor())
		pActor->GetPhysicalSkipEntities(skipList);

	const RayCastResult& result = gAIEnv.pRayCaster->Cast(
	  RayCastRequest(
	    pObj1->GetPos(), dir, COVER_OBJECT_TYPES,
	    AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER, &skipList[0], skipList.size()));

	// Allow small fudge in th test just in case the point is exactly on ground.
	return !result || result[0].dist > (dir.GetLength() - 0.1f);
}

//-----------------------------------------------------------------------------------------------------------
//
bool CAISystem::CheckVisibilityToBody(CAIActor* pObserver, CAIActor* pBody, float& closestDistSq, IPhysicalEntity* pSkipEnt)
{
	int newFlags = AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER;

	CRY_PROFILE_FUNCTION(PROFILE_AI);

	Vec3 bodyPos(pBody->GetPos());

	const Vec3& puppetPos = pObserver->GetPos();
	Vec3 posDiff = bodyPos - puppetPos;
	float distSq = posDiff.GetLengthSquared();
	if (distSq > closestDistSq)
		return false;

	if (IAIObject::eFOV_Outside == pObserver->IsObjectInFOV(pBody, pObserver->GetParameters().m_PerceptionParams.perceptionScale.visual * 0.75f))
		return false;

	//--------------- ACCURATE MEASURING
	float dist = sqrtf(distSq);

	PhysSkipList skipList;
	pObserver->GetPhysicalSkipEntities(skipList);
	pBody->GetPhysicalSkipEntities(skipList);
	if (pSkipEnt)
		stl::push_back_unique(skipList, pSkipEnt);

	const RayCastResult& result = gAIEnv.pRayCaster->Cast(RayCastRequest(puppetPos, posDiff, COVER_OBJECT_TYPES, newFlags,
	                                                                     &skipList[0], skipList.size()));

	bool isVisible = !result;

	// check if collider is the body itself
	if (!isVisible)
	{
		const ray_hit& hit = result[0];
		isVisible = hit.pCollider == pBody->GetProxy()->GetPhysics() || hit.pCollider == pBody->GetProxy()->GetPhysics(true);
	}

	if (!isVisible)
		return false;

	closestDistSq = distSq;

	return true;
}
