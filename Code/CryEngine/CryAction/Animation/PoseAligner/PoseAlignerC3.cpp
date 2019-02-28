// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryExtension/CryCreateClassInstance.h>
#include <CryExtension/ClassWeaver.h>
#include <CryAnimation/ICryAnimation.h>

#include "PoseAligner.h"

#define UNKNOWN_GROUND_HEIGHT -1E10f

// Pinger specific Raycaster. Finds and updates the foot position for the pinger.

class CContactRaycastPinger : public PoseAligner::CContactReporter
{
public:
	CContactRaycastPinger(IVehicle& pinger, float length)
		: m_pinger(pinger)
		, m_length(length)
	{}

	// CContactReporter
public:
	virtual bool Update(Vec3& position, Vec3& normal);

private:
	IVehicle& m_pinger;
	float     m_length;
};

typedef _smart_ptr<CContactRaycastPinger> CContactRaycastPingerPtr;

bool CContactRaycastPinger::Update(Vec3& position, Vec3& normal)
{
	Vec3 positionPrevious = position;
	Vec3 positionPreviousGlobal = positionPrevious;

	Vec3 rayPosition = positionPreviousGlobal;
	rayPosition.z += m_length;

	IPhysicalEntity* pSkipEntities[10];
	int nSkip = m_pinger.GetSkipEntities(pSkipEntities, 10);

	static const int totalHits = 6;
	ray_hit hits[totalHits];
	IPhysicalWorld::SRWIParams rp;

	rp.Init(rayPosition, Vec3(0.0f, 0.0f, -m_length * 2.0f), ent_static | ent_terrain, rwi_pierceability0,
	        SCollisionClass(0, collision_class_living | collision_class_articulated),
	        &hits[0], totalHits, pSkipEntities, nSkip);

	int hitCount = gEnv->pPhysicalWorld->RayWorldIntersection(rp);

	// Find best hit. This is the highest contact, that is a viable place to put the foot.
	//	eg. Not on poles, other Pinger's legs, or other unwanted places.

	ray_hit* pHit = NULL;
	for (int i = 0; i < totalHits; i++)
	{
		ray_hit& hit = hits[i];
		if (!hit.pCollider)
			continue;
		if (!hit.bTerrain)
		{
			// Ignore small and fast things.
			pe_params_flags pgetflags;
			pe_status_pos sgetpos;
			if (hit.pCollider->GetStatus(&sgetpos) && hit.pCollider->GetParams(&pgetflags))
				if (sgetpos.iSimClass == 2 && (pgetflags.flags & ref_small_and_fast) != 0)
					continue;

			pe_params_part pgetpart;
			pgetpart.ipart = hit.ipart;

			phys_geometry* geometry = NULL;
			if (hit.pCollider->GetParams(&pgetpart))
			{
				if (pgetpart.flagsOR & geom_no_coll_response)
					continue;

				geometry = pgetpart.pPhysGeomProxy ? pgetpart.pPhysGeomProxy : pgetpart.pPhysGeom;
			}

			Vec3 sz;
			if (geometry && geometry->pGeom)
			{
				primitives::box tempBox;
				geometry->pGeom->GetBBox(&tempBox);
				sz = tempBox.size;
			}
			else
			{
				pe_params_bbox pgetbox;
				hit.pCollider->GetParams(&pgetbox);
				sz = (pgetbox.BBox[1] - pgetbox.BBox[0]);
			}

			// Ignore small objects. (Matches similar check in livingentity physics code)
			const float maxarea = max(max(sz.x * sz.y, sz.x * sz.z), sz.y * sz.z) * sqr(pgetpart.scale) * 4;
			const float maxdim = max(max(sz.x, sz.y), sz.z) * pgetpart.scale * 2;
			if (maxarea < sqr(sz.x) * g_PI * 0.25f && maxdim < sz.z * 1.4f)
				continue;
		}
		pHit = ((!pHit) || hit.dist < pHit->dist) ? &hit : pHit;
	}

	// Apply the chosen hit results.
	if (pHit)
	{
		position.z = pHit->pt.z;
		normal = pHit->n * (float)__fsel(pHit->n.z, 1.f, -1.f);
		normal.NormalizeSafe(Vec3(0.0f, 0.0f, 1.0f));
	}

#ifndef _RELEASE
	if (PoseAligner::CVars::GetInstance().m_debugDraw)
	{
		SAuxGeomRenderFlags flags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		flags.SetDepthTestFlag(e_DepthTestOff);
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(flags);

		if (pHit)
		{
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(pHit->pt, 0.05f, ColorB(0xff, 0xff, 0x00, 0xff));
		}
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(positionPreviousGlobal, 0.05f, ColorB(0xff, 0x00, 0xff, 0xff));

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
		  rayPosition, ColorB(0xff, 0xff, 0xff, 0xff),
		  rayPosition + Vec3(0.0f, 0.0f, -m_length * 2.0f), ColorB(0xff, 0xff, 0xff, 0xff));
	}
#endif //_RELEASE

	return pHit != NULL;
}

// TEMP
// This code does the initialization of the PoseAligner. Currently all values
// are hard-coded here, however the plan is to have all of these be read from
// auxiliary parameters in the CHRPARAMS or as part of the entity properties.

ILINE bool InitializePoseAlignerBipedHuman(PoseAligner::CPose& pose, IEntity& entity, ICharacterInstance& character)
{
	IDefaultSkeleton& rIDefaultSkeleton = character.GetIDefaultSkeleton();
	int jointIndexRoot = rIDefaultSkeleton.GetJointIDByName("Bip01 Pelvis");
	if (jointIndexRoot < 0)
		return false;
	int jointIndexFrontLeft = rIDefaultSkeleton.GetJointIDByName("Bip01 planeTargetLeft");
	if (jointIndexFrontLeft < 0)
		return false;
	int jointIndexFrontRight = rIDefaultSkeleton.GetJointIDByName("Bip01 planeTargetRight");
	if (jointIndexFrontRight < 0)
		return false;

	int jointIndexLeftBlend = rIDefaultSkeleton.GetJointIDByName("Bip01 planeWeightLeft");
	int jointIndexRightBlend = rIDefaultSkeleton.GetJointIDByName("Bip01 planeWeightRight");

	if (!pose.Initialize(entity, &character, jointIndexRoot))
		return false;

	pose.SetRootOffsetMinMax(-0.4f, 0.0f);

	PoseAligner::SChainDesc chainDesc;
	chainDesc.name = "Left";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("LftLeg01");
	chainDesc.contactJointIndex = jointIndexFrontLeft;
	chainDesc.targetBlendJointIndex = jointIndexLeftBlend;
	chainDesc.bBlendProcedural = true;
	chainDesc.bTargetSmoothing = true;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, 0.0f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.6f);
	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "Right";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("RgtLeg01");
	chainDesc.contactJointIndex = jointIndexFrontRight;
	chainDesc.targetBlendJointIndex = jointIndexRightBlend;
	chainDesc.bBlendProcedural = true;
	chainDesc.bTargetSmoothing = true;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, 0.0f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.6f);
	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	return pose.GetChainCount() != 0;
}

ILINE bool InitializePoseAlignerBipedAlien(PoseAligner::CPose& pose, IEntity& entity, ICharacterInstance& character)
{
	IDefaultSkeleton& rIDefaultSkeleton = character.GetIDefaultSkeleton();
	// Stalker, Grunt
	int jointIndexRoot = rIDefaultSkeleton.GetJointIDByName("Pelvis");
	if (jointIndexRoot < 0)
		return false;
	int jointIndexFrontLeft = rIDefaultSkeleton.GetJointIDByName("L Heel");
	if (jointIndexFrontLeft < 0)
		return false;
	int jointIndexFrontRight = rIDefaultSkeleton.GetJointIDByName("R Heel");
	if (jointIndexFrontRight < 0)
		return false;

	if (!pose.Initialize(entity, &character, jointIndexRoot))
		return false;

	pose.SetRootOffsetMinMax(-0.5f, 0.0f);

	PoseAligner::SChainDesc chainDesc;
	chainDesc.name = "Left";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("LftLeg01");
	chainDesc.contactJointIndex = jointIndexFrontLeft;
	chainDesc.bBlendProcedural = true;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -0.1f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, +0.5f);
	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "Right";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("RgtLeg01");
	chainDesc.contactJointIndex = jointIndexFrontRight;
	chainDesc.bBlendProcedural = true;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -0.1f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, +0.5f);
	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	return pose.GetChainCount() != 0;
}

ILINE bool InitializePoseAlignerPinger(PoseAligner::CPose& pose, IEntity& entity, ICharacterInstance& character)
{
	IDefaultSkeleton& rIDefaultSkeleton = character.GetIDefaultSkeleton();

	const bool bIsMP = gEnv->bMultiplayer;

	IVehicle* pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(entity.GetId());
	if (bIsMP && !pVehicle)
		return false;

#define CREATEPINGERCONTACTREPORTER                                                                          \
  if (bIsMP)                                                                                                 \
  {                                                                                                          \
    if (_smart_ptr<CContactRaycastPinger> pContactRaycast = new CContactRaycastPinger(*pVehicle, 3.f))       \
    {                                                                                                        \
      chainDesc.pContactReporter = pContactRaycast;                                                          \
    }                                                                                                        \
  }                                                                                                          \
  else                                                                                                       \
  {                                                                                                          \
    if (_smart_ptr<PoseAligner::CContactRaycast> pContactRaycast = new PoseAligner::CContactRaycast(entity)) \
    {                                                                                                        \
      pContactRaycast->SetLength(4.f);                                                                       \
      chainDesc.pContactReporter = pContactRaycast;                                                          \
    }                                                                                                        \
  }

	int jointIndexRoot = rIDefaultSkeleton.GetJointIDByName("Pelvis");
	if (jointIndexRoot < 0)
		return false;
	int jointIndexFrontLeft = rIDefaultSkeleton.GetJointIDByName("planeAlignerLeft");
	if (jointIndexFrontLeft < 0)
		return false;
	int jointIndexFrontLeftBlend = rIDefaultSkeleton.GetJointIDByName("planeLockWeightLeft");
	if (jointIndexFrontLeftBlend < 0)
		return false;
	int jointIndexFrontRight = rIDefaultSkeleton.GetJointIDByName("planeAlignerRight");
	if (jointIndexFrontRight < 0)
		return false;
	int jointIndexFrontRightBlend = rIDefaultSkeleton.GetJointIDByName("planeLockWeightRight");
	if (jointIndexFrontRightBlend < 0)
		return false;
	int jointIndexFrontCenter = rIDefaultSkeleton.GetJointIDByName("planeAlignerCenter");
	if (jointIndexFrontCenter < 0)
		return false;
	int jointIndexFrontCenterBlend = rIDefaultSkeleton.GetJointIDByName("planeLockWeightCenter");
	if (jointIndexFrontCenterBlend < 0)
		return false;

	if (!pose.Initialize(entity, &character, jointIndexRoot))
		return false;

	PoseAligner::SChainDesc chainDesc;
	chainDesc.name = "Left";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("LftLeg01");
	chainDesc.contactJointIndex = jointIndexFrontLeft;
	chainDesc.targetBlendJointIndex = jointIndexFrontLeftBlend;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, bIsMP ? -1.8f : -1.8f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, bIsMP ? +0.75f : +1.f);
	chainDesc.bForceNoIntersection = true;

	float rootMax = -chainDesc.offsetMin.z;
	float rootMin = -chainDesc.offsetMax.z;

	CREATEPINGERCONTACTREPORTER;

	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "Right";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("RgtLeg01");
	chainDesc.contactJointIndex = jointIndexFrontRight;
	chainDesc.targetBlendJointIndex = jointIndexFrontRightBlend;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, bIsMP ? -1.8f : -1.8f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, bIsMP ? +0.75f : +1.f);
	chainDesc.bForceNoIntersection = true;

	rootMax = min(rootMax, -chainDesc.offsetMin.z);
	rootMin = max(rootMin, -chainDesc.offsetMax.z);

	CREATEPINGERCONTACTREPORTER;

	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "Center";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("CtrLeg01");
	chainDesc.contactJointIndex = jointIndexFrontCenter;
	chainDesc.targetBlendJointIndex = jointIndexFrontCenterBlend;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, bIsMP ? -2.f : -2.f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, bIsMP ? +1.3f : +1.6f);
	chainDesc.bForceNoIntersection = true;

	rootMax = min(rootMax, -chainDesc.offsetMin.z);
	rootMin = max(rootMin, -chainDesc.offsetMax.z);

	CREATEPINGERCONTACTREPORTER;

	pose.SetRootOffsetMinMax(rootMin, rootMax);
	pose.SetRootOffsetAverage(!bIsMP);

	if (!pose.CreateChain(chainDesc))
		return false;

	return pose.GetChainCount() != 0;
}

ILINE bool InitializePoseAlignerScorcher(PoseAligner::CPose& pose, IEntity& entity, ICharacterInstance& character)
{
	IDefaultSkeleton& rIDefaultSkeleton = character.GetIDefaultSkeleton();
	int jointIndexRoot = rIDefaultSkeleton.GetJointIDByName("hips_joint");
	if (jointIndexRoot < 0)
		return false;
	int jointIndexBackLeft = rIDefaultSkeleton.GetJointIDByName("l_bwd_leg_end_joint");
	if (jointIndexBackLeft < 0)
		return false;
	int jointIndexBackLeftBlend = rIDefaultSkeleton.GetJointIDByName("planeLockWeightBackLeft");
	if (jointIndexBackLeftBlend < 0)
		return false;
	int jointIndexBackRight = rIDefaultSkeleton.GetJointIDByName("r_bwd_leg_end_joint");
	if (jointIndexBackRight < 0)
		return false;
	int jointIndexBackRightBlend = rIDefaultSkeleton.GetJointIDByName("planeLockWeightBackRight");
	if (jointIndexBackRightBlend < 0)
		return false;
	int jointIndexFrontLeft = rIDefaultSkeleton.GetJointIDByName("l_fwd_leg_end_joint");
	if (jointIndexFrontLeft < 0)
		return false;
	int jointIndexFrontLeftBlend = rIDefaultSkeleton.GetJointIDByName("planeLockWeightFrontLeft");
	if (jointIndexFrontLeftBlend < 0)
		return false;
	int jointIndexFrontRight = rIDefaultSkeleton.GetJointIDByName("r_fwd_leg_end_joint");
	if (jointIndexFrontRight < 0)
		return false;
	int jointIndexFrontRightBlend = rIDefaultSkeleton.GetJointIDByName("planeLockWeightFrontRight");
	if (jointIndexFrontRightBlend < 0)
		return false;

	if (!pose.Initialize(entity, &character, jointIndexRoot))
		return false;

	pose.SetRootOffsetMinMax(-1.0f, 1.0f);

	PoseAligner::SChainDesc chainDesc;
	chainDesc.name = "Left";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("LftLeg01");
	chainDesc.contactJointIndex = jointIndexBackLeft;
	chainDesc.targetBlendJointIndex = jointIndexBackLeftBlend;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -1.0f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.15f);
	chainDesc.bTargetSmoothing = true;
	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "Right";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("RgtLeg01");
	chainDesc.contactJointIndex = jointIndexBackRight;
	chainDesc.targetBlendJointIndex = jointIndexBackRightBlend;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -1.0f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.15f);
	chainDesc.bTargetSmoothing = true;
	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "FrontLeft";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("LftArm01");
	chainDesc.contactJointIndex = jointIndexFrontLeft;
	chainDesc.targetBlendJointIndex = jointIndexFrontLeftBlend;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -1.0f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.15f);
	chainDesc.bTargetSmoothing = true;
	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "FrontRight";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("RgtArm01");
	chainDesc.contactJointIndex = jointIndexFrontRight;
	chainDesc.targetBlendJointIndex = jointIndexFrontRightBlend;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -1.0f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.15f);
	chainDesc.bTargetSmoothing = true;
	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	return pose.GetChainCount() != 0;
}

ILINE bool InitializePoseAlignerDeer(PoseAligner::CPose& pose, IEntity& entity, ICharacterInstance& character)
{
	IDefaultSkeleton& rIDefaultSkeleton = character.GetIDefaultSkeleton();
	int jointIndexRoot = rIDefaultSkeleton.GetJointIDByName("Def_COG_jnt");
	if (jointIndexRoot < 0)
		return false;
	int jointIndexBackLeft = rIDefaultSkeleton.GetJointIDByName("Def_L_ToeEnd_jnt");
	if (jointIndexBackLeft < 0)
		return false;
	int jointIndexBackRight = rIDefaultSkeleton.GetJointIDByName("Def_R_ToeEnd_jnt");
	if (jointIndexBackRight < 0)
		return false;
	int jointIndexFrontLeft = rIDefaultSkeleton.GetJointIDByName("Def_L_WristEnd_jnt");
	if (jointIndexFrontLeft < 0)
		return false;
	int jointIndexFrontRight = rIDefaultSkeleton.GetJointIDByName("Def_R_WristEnd_jnt");
	if (jointIndexFrontRight < 0)
		return false;

	if (!pose.Initialize(entity, &character, jointIndexRoot))
		return false;

	pose.SetRootOffsetMinMax(-0.1f, 0.04f);

	PoseAligner::SChainDesc chainDesc;
	chainDesc.name = "Left";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("LftLeg01");
	chainDesc.contactJointIndex = jointIndexBackLeft;
	chainDesc.bBlendProcedural = true;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -0.05f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.4f);

	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "Right";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("RgtLeg01");
	chainDesc.contactJointIndex = jointIndexBackRight;
	chainDesc.bBlendProcedural = true;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -0.05f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.4f);

	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "FrontLeft";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("LftArm01");
	chainDesc.contactJointIndex = jointIndexFrontLeft;
	chainDesc.bBlendProcedural = true;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -0.05f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.02f);

	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	chainDesc.name = "FrontRight";
	chainDesc.eType = IAnimationPoseAlignerChain::eType_Limb;
	chainDesc.solver = CCrc32::ComputeLowercase("RgtArm01");
	chainDesc.contactJointIndex = jointIndexFrontRight;
	chainDesc.bBlendProcedural = true;
	chainDesc.offsetMin = Vec3(0.0f, 0.0f, -0.05f);
	chainDesc.offsetMax = Vec3(0.0f, 0.0f, 0.02f);

	if (PoseAligner::CContactRaycastPtr pContactRaycast = new PoseAligner::CContactRaycast(entity))
	{
		pContactRaycast->SetLength(1.0f);
		chainDesc.pContactReporter = pContactRaycast;
	}
	if (!pose.CreateChain(chainDesc))
		return false;

	return pose.GetChainCount() != 0;
}

ILINE bool InitializePoseAligner(PoseAligner::CPose& pose, IEntity& entity, ICharacterInstance& character)
{
	// TEMP: Make sure we do string comparison only once!
	if (pose.m_bInitialized)
		return pose.m_pEntity != NULL;

	if (!InitializePoseAlignerBipedHuman(pose, entity, character))
		if (!InitializePoseAlignerBipedAlien(pose, entity, character))
			if (!InitializePoseAlignerPinger(pose, entity, character))
				if (!InitializePoseAlignerScorcher(pose, entity, character))
					if (!InitializePoseAlignerDeer(pose, entity, character))
					{
						pose.m_bInitialized = true;
						return false;
					}

	pose.m_bInitialized = true;
	return pose.GetChainCount() != 0;
}

bool CPoseAlignerC3::Initialize(IEntity& entity, ICharacterInstance* pCharacter)
{
	return InitializePoseAligner(*this, entity, *pCharacter);
}

CRYREGISTER_CLASS(CPoseAlignerC3)
