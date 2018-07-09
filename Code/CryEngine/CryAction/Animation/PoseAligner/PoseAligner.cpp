// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryExtension/CryCreateClassInstance.h>
#include <CryExtension/ClassWeaver.h>
#include <CryAnimation/ICryAnimation.h>

#include "PoseAligner.h"

#define UNKNOWN_GROUND_HEIGHT -1E10f

namespace {

bool GetGroundFromEntity(const IEntity& entity, float& height, Vec3& normal)
{

	height = entity.GetWorldPos().z;
	normal = Vec3(0.0f, 0.0f, 1.0f);

	IPhysicalEntity* pPhysEntity = entity.GetPhysics();
	bool bHeightValid = false;
	if (pPhysEntity && pPhysEntity->GetType() == PE_LIVING)
	{
		pe_status_living status;
		pPhysEntity->GetStatus(&status);
		height = status.groundHeight;
		normal = status.groundSlope;
		bHeightValid = (height != UNKNOWN_GROUND_HEIGHT) && (height != -1e-10f);
	}

	return bHeightValid;
}

Vec3 ComputeAnimationMovementPlaneNormal(const ISkeletonAnim& skeletonAnim, const Vec3& defaultNormal)
{
	const QuatT movement = skeletonAnim.GetRelMovement();
	if (movement.t.GetLengthSquared() > 0.0001f)
	{
		Vec3 movementDirection = movement.t.GetNormalized();
		if (::fabs(movementDirection.z) > 0.0001f)
		{
			Vec3 v(movementDirection.x, movementDirection.y, 0.0f);
			v.Normalize();

			Vec3 normal = movementDirection.Cross(v).GetNormalized().Cross(movementDirection);
			if (normal.z < 0.0f)
				normal = -normal;
			return normal;
		}
	}
	return defaultNormal;
}

class CProceduralBlend
{
public:
	CProceduralBlend() :
		m_elevation(0.3f),
		m_elevationScale(1.0f / 0.3f),
		m_rangeStart(0.0f),
		m_rangeEnd(0.2f)
	{
	}

public:
	float ComputeBlendWeight(float value) const
	{
		float w = (value - m_rangeStart) / (m_rangeEnd - m_rangeStart) * m_elevationScale;
		return clamp_tpl(1.0f - w, 0.0f, 1.0f);
	}

public:
	float m_elevation;
	float m_elevationScale;
	float m_rangeStart;
	float m_rangeEnd;
};

} // namespace

namespace PoseAligner {

CVars::CVars()
{
	REGISTER_CVAR2("a_poseAlignerEnable", &m_enable, 1, VF_NULL, "Enable PoseAligner.");
	REGISTER_CVAR2("a_poseAlignerDebugDraw", &m_debugDraw, 0, VF_NULL, "Enable PoseAligner debug drawing.");
	REGISTER_CVAR2("a_poseAlignerForceTargetSmoothing", &m_forceTargetSmoothing, 0, VF_NULL, "PoseAligner forces smoothing of target position and normal.");
	REGISTER_CVAR2("a_poseAlignerForceNoRootOffset", &m_forceNoRootOffset, 0, VF_NULL, "PoseAligner forces no root offset.");
	REGISTER_CVAR2("a_poseAlignerForceNoIntersections", &m_forceNoIntersections, 0, VF_NULL, "PoseAligner forces no intersections, might make animation more 'poppy'.");
	REGISTER_CVAR2("a_poseAlignerForceLock", &m_forceLock, 0, VF_NULL, "PoseAligner force lock.");
	REGISTER_CVAR2("a_poseAlignerForceWeightOne", &m_forceWeightOne, 0, VF_NULL, "PoseAligner forces targeting weight to always be one.");

}

/*
   CContactRaycast
 */

CContactRaycast::CContactRaycast(IEntity& entity) :
	m_pEntity(&entity),
	m_length(3.0f)
{
}

//

bool CContactRaycast::Update(Vec3& position, Vec3& normal)
{
	QuatT location(m_pEntity->GetWorldRotation(), m_pEntity->GetWorldPos());

	Vec3 positionPrevious = position;
	Vec3 positionPreviousGlobal = positionPrevious;

	Vec3 rayPosition = positionPreviousGlobal;
	rayPosition.z += m_length;

	IPhysicalEntity* pEntityPhysics = m_pEntity->GetPhysics();
	ray_hit hit;
	int hitCount = 0;
	const int MAX_SKIP_ENT = 4;
	IPhysicalEntity* pSkipEntities[MAX_SKIP_ENT];

	// Always include self
	pSkipEntities[0] = pEntityPhysics;

	if (IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_pEntity ? m_pEntity->GetId() : -1))
	{
		// Always include self + any other optional entities actor would like to ignore
		int nSkip = pActor->GetPhysicalSkipEntities(pEntityPhysics ? &pSkipEntities[1] : pSkipEntities, pEntityPhysics ? MAX_SKIP_ENT - 1 : MAX_SKIP_ENT);
		if (pEntityPhysics)
		{
			++nSkip;
		}

		hitCount = gEnv->pPhysicalWorld->RayWorldIntersection(
		  rayPosition, Vec3(0.0f, 0.0f, -m_length * 2.0f),
		  ent_rigid | ent_sleeping_rigid | ent_static | ent_terrain,
		  rwi_stop_at_pierceable,
		  &hit, 1, pSkipEntities, nSkip);
	}
	else
	{
		hitCount = gEnv->pPhysicalWorld->RayWorldIntersection(
		  rayPosition, Vec3(0.0f, 0.0f, -m_length * 2.0f),
		  ent_rigid | ent_sleeping_rigid | ent_static | ent_terrain,
		  rwi_stop_at_pierceable,
		  &hit, 1, &pEntityPhysics, pEntityPhysics ? 1 : 0);
	}

	if (hitCount > 0)
	{
		position.z = hit.pt.z;
		normal = Vec3(hit.n.x, hit.n.y, fabsf(hit.n.z));
		normal.NormalizeSafe(Vec3(0.0f, 0.0f, 1.0f));
	}

	//

#ifndef _RELEASE
	if (CVars::GetInstance().m_debugDraw)
	{
		SAuxGeomRenderFlags flags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		flags.SetDepthTestFlag(e_DepthTestOff);
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(flags);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(hit.pt, 0.05f, ColorB(0xff, 0xff, 0x00, 0xff));
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(positionPreviousGlobal, 0.05f, ColorB(0xff, 0x00, 0xff, 0xff));

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
		  rayPosition, ColorB(0xff, 0xff, 0xff, 0xff),
		  rayPosition + Vec3(0.0f, 0.0f, -m_length * 2.0f), ColorB(0xff, 0xff, 0xff, 0xff));

		//		Plane plane = Plane::CreatePlane(
		//			location.q * (normal * location.q),
		//			location * (location.GetInverted() * position));
		//		Ray ray(rayPosition, Vec3(0.0f, 0.0f, -m_length * 2.0f));
		//		Vec3 p;
		//		if (Intersect::Ray_Plane(ray, plane, p))
		//		{
		////			gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(
		////				p, normal, 0.1f, 0.5f, ColorB(0xff, 0xff, 0x00, 0xff), true);
		//		}
	}
#endif //_RELEASE

	return hitCount > 0;
}

/*
   CChain
 */

CChainPtr CChain::Create(const SChainDesc& desc)
{
	if (!desc.IsValid())
		return CChainPtr();

	CChainPtr chain = new CChain();
	chain->m_desc = desc;
	return chain;
}

CChain::CChain()
{
	m_targetPositionAnimation = QuatT(IDENTITY);
	Reset();
}

//

void CChain::Reset()
{
	m_targetPosition = Vec3(-99999.0f, -99999.0f, -99999.0f);
	m_targetPositionFiltered = Vec3(-99999.0f, -99999.0f, -99999.0f);
	m_targetPositionFilteredSmoothRate = Vec3(ZERO);

	m_targetNormal = Vec3(0.0f, 0.0f, 1.0f);
	m_targetNormalFiltered = Vec3(0.0f, 0.0f, 1.0f);
	m_targetNormalFilteredSmoothRate = Vec3(ZERO);

	m_targetBlendWeight = 0.0f;
	m_targetBlendWeightAnimated = 0.0f;
	m_targetBlendWeightFiltered = 0.0f;
	m_targetBlendWeightFilteredSmoothRate = 0.0f;

	m_targetDelta = Vec3(ZERO);

	m_bTargetIntersecting = false;

	m_bTargetLock = false;

	m_bTargetForceWeightOne = false;

	m_animationSlopeNormal = Vec3(0.0f, 0.0f, 1.0f);
	m_animationSlopeNormalFiltered = m_animationSlopeNormal;
	m_animationSlopeNormalFilteredSmoothRate = Vec3(ZERO);
}

void CChain::UpdateFromAnimations(ICharacterInstance& character, const QuatT& location, const float time)
{
	m_animationSlopeNormal = ComputeAnimationMovementPlaneNormal(*character.GetISkeletonAnim(), Vec3(0.0f, 0.0f, 1.0f));
	SmoothCD(m_animationSlopeNormalFiltered, m_animationSlopeNormalFilteredSmoothRate, time, m_animationSlopeNormal, 0.01f);
	m_animationSlopeNormalFiltered.NormalizeSafe(Vec3(0.0f, 0.0f, 1.0f));
}

void CChain::FindContact(const QuatT& location)
{
	Vec3 targetPositionPrevious = m_targetPosition;

	m_targetPosition = location * m_targetPositionAnimation.t;
	m_targetNormal = Vec3(0.0f, 0.0f, 1.0f);

	float projectedDifference = (Vec2(m_targetPosition) - Vec2(targetPositionPrevious)).GetLength();
	if (projectedDifference < 0.05f)
	{
		m_targetPosition.x = targetPositionPrevious.x;
		m_targetPosition.y = targetPositionPrevious.y;
	}

	if (!m_desc.pContactReporter)
	{
		m_targetBlendWeight = 0.0f;
		return;
	}

	Vec3 targetPositionNew = m_targetPosition;
	Vec3 targetNormalNew(0.0f, 0.0f, 1.0f);
	bool bContact = m_desc.pContactReporter->Update(targetPositionNew, targetNormalNew);

	if (bContact)
	{
		targetPositionNew += m_animationSlopeNormalFiltered * m_animationSlopeNormalFiltered.Dot(m_targetPositionAnimation.t);

		m_bTargetIntersecting = m_targetPosition.z < targetPositionNew.z;
		if (m_bTargetIntersecting)
		{
			m_targetBlendWeight = 1.0f;
		}
		else if (m_desc.bBlendProcedural)
		{
			CProceduralBlend proceduralBlend;
			m_targetBlendWeight = proceduralBlend.ComputeBlendWeight(m_targetPositionAnimation.t.z);
		}

		m_targetDelta.z = targetPositionNew.z - m_targetPosition.z;
	}
	else
	{
		m_targetBlendWeight = 0.0f;
		m_bTargetIntersecting = false;
		targetPositionNew.z = location.t.z;
		m_targetDelta.z = 0.0f;
	}

#if 1
	// TODO: Better code to handle custom slope angle limits!
	Vec3 proj = Vec3(targetNormalNew.x, targetNormalNew.y, 0);
	if (proj.GetLength() > 0.1f)
	{
		f32 slopeAngleMax = DEG2RAD(30.0f);
		proj.Normalize();
		f32 slopeAngle = acos_tpl(targetNormalNew.z);
		Vec3 cross = (Vec3(0.0f, 0.0f, 1.0f) % targetNormalNew).GetNormalized();
		if (slopeAngle > slopeAngleMax)
			targetNormalNew = Quat::CreateRotationAA(slopeAngleMax, cross) * Vec3(0.0f, 0.0f, 1.0f);
	}
#endif

	m_targetPosition = targetPositionNew;
	m_targetNormal = targetNormalNew;
}

void CChain::FilterTargetLocation(const float time)
{
	const float smoothTime = m_targetPosition.z > m_targetPositionFiltered.z ? 0.08f : 0.1f;

	SmoothCD(m_targetNormalFiltered, m_targetNormalFilteredSmoothRate, time, m_targetNormal, smoothTime);
	m_targetNormal.NormalizeSafe(Vec3(0.0f, 0.0f, 1.0f));

	if (!m_desc.bTargetSmoothing && !CVars::GetInstance().m_forceTargetSmoothing)
	{
		m_targetPositionFiltered = m_targetPosition;
		m_targetPositionFilteredSmoothRate = ZERO;

		return;
	}

	SmoothCD(m_targetPositionFiltered, m_targetPositionFilteredSmoothRate, time, m_targetPosition, smoothTime);
}

float CChain::ComputeTargetBlendValue(ISkeletonPose& skeletonPose, const float time, const float weight)
{
	m_targetBlendWeightAnimated = -1.0f;
	if (m_desc.targetBlendJointIndex > -1)
	{
		const QuatT& transformation = skeletonPose.GetRelJointByID(m_desc.targetBlendJointIndex);
		m_targetBlendWeightAnimated = transformation.t.x;
	}

	// Force the target to always be in a non-intersected state with the traced
	// environment. Might result in animation popping.
	const bool bTargetForceWeightOne = m_bTargetForceWeightOne || CVars::GetInstance().m_forceWeightOne;
	if ((m_bTargetIntersecting && (m_desc.bForceNoIntersection || CVars::GetInstance().m_forceNoIntersections)) ||
	    bTargetForceWeightOne)
	{
		m_targetBlendWeight = 1.0f;
		m_targetBlendWeightFiltered = m_targetBlendWeight;
		m_targetBlendWeightFilteredSmoothRate = 0.0f;
		return m_targetBlendWeight;
	}

	if (m_targetBlendWeightAnimated > -0.5f)
	{
		m_targetBlendWeight = clamp_tpl(m_targetBlendWeightAnimated, 0.0f, 1.0f);
		m_targetBlendWeightFiltered = m_targetBlendWeight;
		m_targetBlendWeightFilteredSmoothRate = 0.0f;
	}

	SmoothCD(m_targetBlendWeightFiltered, m_targetBlendWeightFilteredSmoothRate, time, m_targetBlendWeight, 0.1f);
	m_targetBlendWeightFiltered *= weight;
	return m_targetBlendWeightFiltered;
}

bool CChain::ComputeRootOffsetExtents(float& offsetMin, float& offsetMax)
{
	offsetMin = m_targetDelta.z - m_desc.offsetMin.z;
	if (offsetMin < -m_desc.offsetMax.z)
		offsetMin = -m_desc.offsetMax.z;

	offsetMax = m_targetDelta.z - m_desc.offsetMax.z;
	if (offsetMax > -m_desc.offsetMin.z)
		offsetMax = -m_desc.offsetMin.z;

	return true;
}

bool CChain::SetupStoreOperators(IAnimationOperatorQueue& operatorQueue)
{
	operatorQueue.PushComputeAbsolute();
	operatorQueue.PushStoreAbsolute(m_desc.contactJointIndex, m_targetPositionAnimation);
	return true;
}

bool CChain::SetupTargetPoseModifiers(const QuatT& location, const Vec3& limitOffset, ISkeletonAnim& skeletonAnim)
{
	bool bTargetLock = m_bTargetLock || CVars::GetInstance().m_forceLock;
	if (!(m_targetBlendWeightFiltered > 0.0f) && !bTargetLock)
		return true;

	if (!m_pPoseAlignerChain)
		CryCreateClassInstanceForInterface<IAnimationPoseAlignerChain>(cryiidof<IAnimationPoseAlignerChain>(), m_pPoseAlignerChain);
	if (!m_pPoseAlignerChain)
		return false;

	const Vec3& planeNormal = (m_targetNormalFiltered * location.q).GetNormalized();
	Plane plane = Plane::CreatePlane(planeNormal, location.GetInverted() * m_targetPositionFiltered);

	m_pPoseAlignerChain->Initialize(m_desc.solver, m_desc.contactJointIndex);
	IAnimationPoseAlignerChain::STarget target;
	target.plane = plane;
	target.distance = 6.0f;
	target.offsetMin = m_desc.offsetMin.z + limitOffset.z;
	target.offsetMax = m_desc.offsetMax.z + limitOffset.z;
	target.targetWeight = bTargetLock ? 1.0f : m_targetBlendWeightFiltered;
	target.alignWeight = target.targetWeight;

	m_pPoseAlignerChain->SetTarget(target);
	m_pPoseAlignerChain->SetTargetLock(bTargetLock ?
	                                   IAnimationPoseAlignerChain::eLockMode_Apply : IAnimationPoseAlignerChain::eLockMode_Store);
	skeletonAnim.PushPoseModifier(0, m_pPoseAlignerChain);
	return true;
}

void CChain::DrawDebug(const QuatT& location)
{
	float offsetMin, offsetMax;
	ComputeRootOffsetExtents(offsetMin, offsetMax);
	offsetMin = min(offsetMin, 0.0f);
	offsetMax = max(offsetMax, 0.0f);
	IRenderAuxText::DrawLabelF(m_targetPosition, 1.5f,
	                           "w %.2f a: %.2f\n"
	                           "min %.2f\n"
	                           "max %.2f\n",
	                           m_targetBlendWeightFiltered, m_targetBlendWeightAnimated,
	                           offsetMin,
	                           offsetMax);

	gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(
	  m_targetPosition, location.q * m_animationSlopeNormalFiltered, 0.05f, 1.0f, ColorB(0xff, 0xff, 0xff, 0xff));
}

/*
   CPose
 */

CPose::CPose()
{
	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_operatorQueue);

	Clear();
}

CPose::~CPose()
{
}

//

void CPose::Reset()
{
	m_rootOffsetSmoothed = 0.0f;
	m_rootOffsetSmoothedRate = 0.0f;

	uint chainCount = uint(m_chains.size());
	for (uint i = 0; i < chainCount; ++i)
		m_chains[i]->Reset();

	m_blendWeight = 1.0f;
}

void CPose::Clear()
{
	RemoveAllChains();

	m_pEntity = NULL;
	m_pSkeletonAnim = NULL;
	m_pSkeletonPose = NULL;

	m_rootJointIndex = -1;

	m_bRootOffset = true;
	m_bRootOffsetAverage = false;
	m_rootOffsetDirection = Vec3(0.0f, 0.0f, 1.0f);
	m_rootOffsetAdditional = ZERO;
	m_rootOffsetMin = 0.0f;
	m_rootOffsetMax = 0.0f;

	Reset();

	// TEMP
	m_bInitialized = false;
}

bool CPose::Initialize(IEntity& entity, ICharacterInstance* pCharacter, int rootJointIndex)
{
	Clear();

	if (!m_operatorQueue)
		return false;
	if (rootJointIndex < 0)
		return false;

	m_pEntity = &entity;
	m_pSkeletonAnim = pCharacter->GetISkeletonAnim();
	m_pSkeletonPose = pCharacter->GetISkeletonPose();

	m_rootJointIndex = rootJointIndex;

	return true;
}

CChainPtr CPose::CreateChain(const SChainDesc& desc)
{
	SChainDesc chainDesc = desc;
	if (!chainDesc.pContactReporter)
		chainDesc.pContactReporter = new CContactRaycast(*m_pEntity);

	CChainPtr chain = CChain::Create(chainDesc);
	if (chain)
		m_chains.push_back(chain);
	return chain;
}

void CPose::RemoveAllChains()
{
	m_chains.clear();
}

//

void CPose::Update(ICharacterInstance* pCharacter, const QuatT& location, const float time)
{
	CRY_ASSERT(m_pEntity);

	if (!CVars::GetInstance().m_enable)
		return;

	m_pSkeletonAnim = pCharacter->GetISkeletonAnim();
	m_pSkeletonPose = pCharacter->GetISkeletonPose();
	if (m_pSkeletonAnim == nullptr || m_pSkeletonPose == nullptr)
		return;
	
	if (m_pSkeletonAnim->GetNumAnimsInFIFO(0) == 0)
		return;

	uint chainCount = uint(m_chains.size());
	if (!chainCount)
		return;

	m_blendWeight = clamp_tpl(m_blendWeight, 0.0f, 1.0f);

	float groundHeight = 0.0f;
	Vec3 groundNormal(0.0f, 0.0f, 1.0f);
	bool bGroundHeightValid = GetGroundFromEntity(*m_pEntity, groundHeight, groundNormal);

	//

	float chainOffsetMin = 0.0f;
	float chainOffsetMax = 0.0f;
	for (uint i = 0; i < chainCount; ++i)
	{
		CChain& chain = *m_chains[i];

		chain.UpdateFromAnimations(*pCharacter, location, time);
		chain.FindContact(location);
		chain.FilterTargetLocation(time);
		chain.ComputeTargetBlendValue(*m_pSkeletonPose, time, m_blendWeight);

		float offsetMin, offsetMax;
		chain.ComputeRootOffsetExtents(offsetMin, offsetMax);
		chainOffsetMin = min(chainOffsetMin, offsetMin);
		chainOffsetMax = max(chainOffsetMax, offsetMax);

		if (CVars::GetInstance().m_debugDraw)
			chain.DrawDebug(location);
	}

	float rootOffset = 0.0f;
	if (m_bRootOffset && !CVars::GetInstance().m_forceNoRootOffset)
	{
		rootOffset = chainOffsetMin;
		if (m_bRootOffsetAverage)
		{
			rootOffset = (chainOffsetMin + chainOffsetMax) * 0.5f;
		}
		else if (abs(chainOffsetMax) > abs(chainOffsetMin))
		{
			rootOffset = chainOffsetMax;
		}
		rootOffset = clamp_tpl(rootOffset, m_rootOffsetMin, m_rootOffsetMax);
	}

	//

	static const float SMOOTH_RATE = 0.10f;
	SmoothCD(m_rootOffsetSmoothed, m_rootOffsetSmoothedRate, time, rootOffset, SMOOTH_RATE);
	m_rootOffsetSmoothed *= m_blendWeight;

	//

	SetupPoseModifiers(location);
	DrawDebug(location, groundNormal);
}

void CPose::SetupPoseModifiers(const QuatT& location)
{
	uint chainCount = uint(m_chains.size());
	if (!chainCount)
		return;

	for (uint i = 0; i < chainCount; ++i)
		m_chains[i]->SetupStoreOperators(*m_operatorQueue.get());

	QuatT chainsLocation = location;
	float rootOffset = m_rootOffsetSmoothed + m_rootOffsetAdditional.z;

	if (rootOffset != 0.0f)
	{
		m_operatorQueue->PushPosition(
		  m_rootJointIndex, IAnimationOperatorQueue::eOp_Additive,
		  m_rootOffsetDirection * rootOffset);
		m_operatorQueue->PushComputeAbsolute();
	}

	m_pSkeletonAnim->PushPoseModifier(0, m_operatorQueue);

	for (uint i = 0; i < chainCount; ++i)
		m_chains[i]->SetupTargetPoseModifiers(chainsLocation, m_rootOffsetDirection * rootOffset, *m_pSkeletonAnim);
}

void CPose::DrawDebug(const QuatT& location, const Vec3& groundNormal)
{
	if (!CVars::GetInstance().m_debugDraw)
		return;

	Vec3 groundPosition = location.t;
	groundPosition += m_rootOffsetDirection * m_rootOffsetSmoothed;
	SAuxGeomRenderFlags flags;
	flags.SetAlphaBlendMode(e_AlphaBlended);
	flags.SetCullMode(e_CullModeNone);
	flags.SetDepthTestFlag(e_DepthTestOn);
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(flags);
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawCylinder(
	  groundPosition, groundNormal, 1.0f, 0.005f, ColorB(0x00, 0x00, 0x00, 0x40), true);

	IRenderAuxText::DrawLabelF(location * m_pSkeletonPose->GetAbsJointByID(m_rootJointIndex).t, 1.5f,
	                           "h %.2f\n"
	                           "w %.2f\n",
	                           m_rootOffsetSmoothed,
	                           m_blendWeight);
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(location.t, 0.125f, ColorB(uint8((1.0f - m_blendWeight) * 255.0f), 0xff, 0xff, 0xff), true);
}

} // namespace PoseAligner
