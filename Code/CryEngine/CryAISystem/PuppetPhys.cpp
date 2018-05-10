// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PuppetPhys.cpp
   $Id$
   Description:	should contaioin all the methods of CPuppet which have to deal with Physics

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Kirill Bulatsev
   - 4 May 2009  : Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/

#include "StdAfx.h"
#include "Puppet.h"
#include "AILog.h"
#include "GoalOp.h"
#include "AIPlayer.h"
#include "Leader.h"
#include "CAISystem.h"
#include "AICollision.h"
#include "SmartObjects.h"
#include "PathFollower.h"
#include "AIVehicle.h"

#include <CrySystem/IConsole.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ILog.h>
#include <CrySystem/ITimer.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryNetwork/ISerialize.h>

//#pragma optimize("", off)
//#pragma inline_depth(0)

std::vector<std::pair<float, size_t>> CPuppet::s_weights;

// Helper structure to sort indices pointing to an array of weights.
struct SDamageLUTSorter
{
	SDamageLUTSorter(const std::vector<float>& values) : m_values(values) {}
	bool operator()(int lhs, int rhs) const { return m_values[lhs] < m_values[rhs]; }
	const std::vector<float>& m_values;
};

//====================================================================
// ActorObstructingAim
//====================================================================
bool CPuppet::ActorObstructingAim(const CAIActor* pActor, const Vec3& firePos, const Vec3& dir, const Ray& fireRay) const
{
	IF_UNLIKELY (!pActor)
		return false;

	const Vec3 normalizedFireDirectionXY = Vec3(dir.x, dir.y, 0.0f).GetNormalizedSafe();
	Vec3 directionFromFirePositionToOtherAgentPosition = pActor->GetPhysicsPos() - firePos;
	directionFromFirePositionToOtherAgentPosition.z = 0.0f;
	directionFromFirePositionToOtherAgentPosition.NormalizeSafe();

	static float threshold = cosf(DEG2RAD(35.0f));
	if (normalizedFireDirectionXY.Dot(directionFromFirePositionToOtherAgentPosition) > threshold)
	{
		// Check if ray intersects with other agent's bounding box
		if (IPhysicalEntity* actorPhysics = pActor->GetPhysics())
		{
			pe_status_pos ppos;

			if (actorPhysics->GetStatus(&ppos))
			{
				Vec3 point;
				AABB box(ppos.pos + ppos.BBox[0], ppos.pos + ppos.BBox[1]);
				if (Intersect::Ray_AABB(fireRay, box, point))
				{
					return true;
				}
			}
		}
	}

	return false;
}

//====================================================================
// CanAimWithoutObstruction
//====================================================================
bool CPuppet::CanAimWithoutObstruction(const Vec3& vTargetPos)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_bDryUpdate)
		return m_lastAimObstructionResult;

	const float checkDistance = GetPos().GetDistance(vTargetPos) * 0.5f;
	const float softCheckDistance = 0.5f;
	const bool bResult = CheckLineOfFire(vTargetPos, checkDistance, softCheckDistance);

	m_lastAimObstructionResult = bResult;
	return bResult;
}

void CPuppet::LineOfFireRayComplete(const QueuedRayID& rayID, const RayCastResult& result)
{
	m_lineOfFireState.asyncState = AsyncReady;
	m_lineOfFireState.rayID = 0;

	m_lineOfFireState.result = true;
	if (result)
	{
		m_lineOfFireState.result = false;

		pe_status_pos stat;
		if (result[0].pCollider && result[0].pCollider->GetStatus(&stat))
		{
			if ((stat.flagsOR & geom_colltype_obstruct))
				m_lineOfFireState.result = !(result[0].dist < m_lineOfFireState.softDistance);
		}
	}
}

//====================================================================
// CheckLineOfFire
//====================================================================
bool CPuppet::CheckLineOfFire(const Vec3& vTargetPos, float fDistance, float fSoftDistance, EStance stance /*= STANCE_NULL*/) const
{
	CRY_ASSERT(vTargetPos.IsValid());

	bool bResult = true;

	// Early outs
	IAIActorProxy* pProxy = GetProxy();
	assert(pProxy);//shut up SCA
	if ((pProxy && pProxy->GetLinkedVehicleEntityId()) || GetSubType() == CAIObject::STP_2D_FLY)
		return true;

	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
	lookUp.Prepare(ActorLookUp::Position);

	SAIBodyInfo bodyInfo;
	const Vec3& firePos = (stance > STANCE_NULL && pProxy->QueryBodyInfo(SAIBodyInfoQuery(stance, 0.0f, 0.0f, true), bodyInfo) ? bodyInfo.vFirePos : GetFirePos());
	Vec3 dir = vTargetPos - firePos;

	Ray fireRay(firePos, dir);
	Vec3 pos = GetPos();

	size_t activeActorCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeActorCount; ++actorIndex)
	{
		CAIActor* pAIActor = lookUp.GetActor<CAIActor>(actorIndex);
		if (pAIActor == this)
			continue;

		if (Distance::Point_PointSq(pos, lookUp.GetPosition(actorIndex)) > sqr(10.0f))
			continue;

		if (!IsHostile(pAIActor) && ActorObstructingAim(pAIActor, firePos, dir, fireRay))
		{
			bResult = false;
			break;
		}
	}

	if (bResult)
	{
		//check the player (friends only)
		CAIActor* pPlayer(CastToCAIActorSafe(GetAISystem()->GetPlayer()));
		if (pPlayer)
		{
			if (!IsHostile(pPlayer) &&
			    Distance::Point_PointSq(GetPos(), pPlayer->GetPos()) < sqr(5.0f) &&
			    ActorObstructingAim(pPlayer, firePos, dir, fireRay))
			{
				bResult = false;
			}
		}
	}

	if (bResult && m_fireMode != FIREMODE_KILL && fDistance > FLT_EPSILON) // when in KILL mode - just shoot no matter what
	{
		dir.Normalize();
		dir *= fDistance;

		m_lineOfFireState.softDistance = fSoftDistance;

		if (m_lineOfFireState.asyncState == AsyncReady)
		{
			m_lineOfFireState.asyncState = AsyncInProgress;

			PhysSkipList skipList;
			GetPhysicalSkipEntities(skipList);

			// Add the physical body of the target to the skip list.
			if (IAIObject* pTarget = GetAttentionTarget())
			{
				// If the target is a memory or a sound, then use the physics
				// from the association (the real entity running around.)
				if (IAIObject* pAssociation = GetAttentionTargetAssociation())
					pTarget = pAssociation;

				if (IEntity* pEntity = pTarget->GetEntity())
				{
					if (IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics())
						stl::push_back_unique(skipList, pPhysicalEntity);
				}
			}

			m_lineOfFireState.rayID = gAIEnv.pRayCaster->Queue(RayCastRequest::HighestPriority,
			                                                   RayCastRequest(
			                                                     firePos, dir, COVER_OBJECT_TYPES,
			                                                     AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
			                                                     &skipList[0], skipList.size()),
			                                                   functor(const_cast<CPuppet&>(*this), &CPuppet::LineOfFireRayComplete));
		}

		return m_lineOfFireState.result;
	}

	return bResult;
}

//====================================================================
// ClampPointInsideCone
//====================================================================
static void ClampPointInsideCone(const Vec3& conePos, const Vec3& coneDir, float coneAngle, Vec3& pos)
{
	Vec3 dirToPoint = pos - conePos;
	float distOnLine = coneDir.Dot(dirToPoint);
	if (distOnLine < 0.0f)
	{
		pos = conePos;
		AILogComment("ClampPointInsideCone(): Point clamped distOnLine = %f", distOnLine);
		return;
	}

	Vec3 pointOnLine = coneDir * distOnLine;

	const float dmax = tanf(coneAngle) * distOnLine + 0.75f;

	Vec3 diff = dirToPoint - pointOnLine;
	float d = diff.NormalizeSafe();

	if (d > dmax)
	{
		pos = conePos + pointOnLine + diff * dmax;
		AILogComment("ClampPointInsideCone(): Point clamped d = %f, dmax = %f", d, dmax);
	}
}

ILINE Vec3 JitterVector(Vec3 v, Vec3 amount)
{
	return Vec3(
	  v.x + cry_random(-amount.x, amount.x),
	  v.y + cry_random(-amount.y, amount.y),
	  v.z + cry_random(-amount.z, amount.z));
}

//====================================================================
// CheckAndAdjustFireTarget
//====================================================================
bool CPuppet::AdjustFireTarget(CAIObject* targetObject, const Vec3& target, bool hit, float missExtraOffset,
                               float clampAngle, Vec3* posOut)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	Vec3 out(target);

	if (hit)
	{
		if (!CalculateHitPointOnTarget(targetObject, target, clampAngle, &out))
		{
			out = JitterVector(target, Vec3(0.05f));
			AdjustWithPrediction(targetObject, out);
		}

		m_targetLastMissPoint = out;

		if (IsFireTargetValid(out, targetObject))
		{
			*posOut = out;

			return true;
		}

		m_lastHitShotsCount = ~0l;
	}
	else
	{
		size_t shotsCount = m_pFireCmdHandler ? (size_t)m_pFireCmdHandler->GetShotCount() : 0;
		if (m_lastMissShotsCount == shotsCount && STP_HELICRYSIS2 != GetSubType())
			out = m_targetLastMissPoint;
		else
		{
			bool found = false;

			if (targetObject && gAIEnv.CVars.EnableCoolMisses && cry_random(0.0f, 1.0f) < gAIEnv.CVars.CoolMissesProbability)
			{
				if (CAIPlayer* player = targetObject->CastToCAIPlayer())
				{
					Vec3 fireLocation = GetFirePos();
					Vec3 dir = target - fireLocation;

					float distance = dir.NormalizeSafe();

					if (distance >= gAIEnv.CVars.CoolMissesMinMissDistance)
						found = player->GetMissLocation(fireLocation, dir, clampAngle, out);
				}
			}

			found = found || CalculateMissPointOutsideTargetSilhouette(targetObject, target, missExtraOffset, &out);

			if (!found)
				out = JitterVector(target, Vec3(0.15f));
		}

		m_targetLastMissPoint = out;

		if (IsFireTargetValid(out, targetObject))
		{
			m_lastMissShotsCount = shotsCount;
			m_targetEscapeLastMiss = clamp_tpl(m_targetEscapeLastMiss - 0.1f, 0.0f, 1.0f);
			*posOut = out;

			return true;
		}

		m_lastMissShotsCount = ~0l;
		m_targetEscapeLastMiss = clamp_tpl(m_targetEscapeLastMiss + 0.1f, 0.0f, 1.0f);
	}

	*posOut = out;

	return false;
}

//====================================================================
// DeltaAngle
// returns delta angle from a to b in -PI..PI
//====================================================================
inline float DeltaAngle(float a, float b)
{
	float d = b - a;
	d = fmodf(d, gf_PI2);
	if (d < -gf_PI) d += gf_PI2;
	if (d > gf_PI) d -= gf_PI2;
	return d;
};

bool CPuppet::CalculateMissPointOutsideTargetSilhouette(CAIObject* targetObject, const Vec3& target, float missExtraOffset,
                                                        Vec3* posOut)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_targetSilhouette.valid)
	{
		const Vec3 silhouettePlanePoint = m_targetSilhouette.IntersectSilhouettePlane(GetFirePos(), target);
		const Vec3 silhouettePoint = m_targetSilhouette.ProjectVectorOnSilhouette(silhouettePlanePoint - m_targetSilhouette.center);

		if (Overlap::Point_Polygon2D(silhouettePoint, m_targetSilhouette.points))
		{
			const Vec3 bias = m_targetSilhouette.ProjectVectorOnSilhouette(m_targetBiasDirection)
			                  .GetNormalizedSafe(Vec3(0.0f, 0.0f, 0.0f));
			const float spread = DEG2RAD(60.0f - 50.0f * m_targetFocus);
			const float angleLimit = DEG2RAD(50.0f);

			float angle = (1.5f * atan2_tpl(bias.y, bias.x)) + cry_random(-1.0f, 1.0f) * spread * 0.5f;

			if (!m_targetLastMissPoint.IsZero())
			{
				const Vec3 lastMiss = m_targetSilhouette.ProjectVectorOnSilhouette(m_targetLastMissPoint - m_targetSilhouette.center);

				float lastMissAngle = atan2_tpl(lastMiss.y, lastMiss.x);
				float deltaAngle = DeltaAngle(lastMissAngle, angle);

				Limit(deltaAngle, -angleLimit, angleLimit);
				angle = lastMissAngle + deltaAngle;
			}

			Vec3 pt;
			Vec3 dir(cos_tpl(angle), sin_tpl(angle), 0.0f);

			if (Intersect::Lineseg_Polygon2D(Lineseg(ZERO, dir * 100.0f), m_targetSilhouette.points, pt))
			{
				const Vec3 u = m_targetSilhouette.baseMtx.GetColumn0();
				const Vec3 v = m_targetSilhouette.baseMtx.GetColumn2();

				Vec3 missTarget = m_targetSilhouette.center +
				                  u * (pt.x + dir.x * missExtraOffset) + v * (pt.y + dir.y * missExtraOffset);

				if (missTarget.z >= target.z + 0.15f)
					missTarget.z -= (missTarget.z - target.z);

				*posOut = missTarget;

				return true;
			}
		}

		*posOut = target;

		return true;
	}

	return false;
}

//====================================================================
// GetHitPointOnTarget
//====================================================================
bool CPuppet::CalculateHitPointOnTarget(CAIObject* targetObject, const Vec3& target, float clampAngle, Vec3* posOut)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!targetObject->IsAgent())
		return false;

	CAIActor* targetActor = targetObject->CastToCAIActor();
	if (!targetActor)
		return false;

	if (!targetActor->GetDamageParts())
		return false;

	const DamagePartVector& parts = *targetActor->GetDamageParts();
	size_t partCount = parts.size();

	if (!partCount)
		return false;

	if (m_fireMode == FIREMODE_KILL)
	{
		float maxMult = 0.0f;

		for (size_t i = 0; i < partCount; ++i)
		{
			if (parts[i].damageMult > maxMult)
			{
				*posOut = parts[i].pos;
				maxMult = parts[i].damageMult;
			}
		}

		return maxMult > 0.0f;
	}

	const Vec3 FireLocation = GetFirePos();
	Vec3 pos;

	size_t shotsCount = m_pFireCmdHandler ? (size_t)m_pFireCmdHandler->GetShotCount() : 0;
	if ((m_lastTargetPart < partCount) && (m_lastHitShotsCount == shotsCount))
	{
		pos = parts[m_lastTargetPart].pos;
	}
	else
	{
		m_lastHitShotsCount = shotsCount;

		s_weights.clear();
		s_weights.resize(partCount);

		const Lineseg LineOfFire(FireLocation, target);

		float t;
		for (size_t i = 0; i < partCount; ++i)
			s_weights[i] = std::make_pair(Distance::Point_LinesegSq(parts[i].pos, LineOfFire, t), i);

		std::sort(s_weights.begin(), s_weights.end());

		size_t considerCount = std::min<size_t>(8, partCount);
		assert(considerCount > 0);

		size_t targetPart = s_weights[cry_random((size_t)0, considerCount - 1)].second;
		m_lastTargetPart = targetPart;
		pos = parts[targetPart].pos;
	}

	const float jitterAmount = 0.075f;
	pos = JitterVector(pos, Vec3(jitterAmount));

	// Luciano: add prediction based on bullet and target speeds
	AdjustWithPrediction(targetActor, pos);

	Vec3 dir = m_State.vAimTargetPos - FireLocation;
	dir.Normalize();

	const SAIBodyInfo& bodyInfo = GetBodyInfo();
	if (IEntity* pLinkedVehicleEntity = bodyInfo.GetLinkedVehicleEntity())
	{
		if (pLinkedVehicleEntity->HasAI())
		{
			if (dir.Dot(bodyInfo.vFireDir) < cos_tpl(clampAngle))
				return false;
		}
	}

	*posOut = pos;

	return true;
}

//====================================================================
// AdjustWithPrediction
//====================================================================
void CPuppet::AdjustWithPrediction(CAIObject* pTarget, Vec3& posOut)
{
	if (!pTarget || (GetSubType() == CAIObject::STP_2D_FLY))
		return;

	float sp = m_CurrentWeaponDescriptor.fSpeed;//bullet speed
	if (sp > 0.0f)
	{
		if (pTarget->GetPhysics())
		{
			pe_status_dynamics dyn;
			pTarget->GetPhysics()->GetStatus(&dyn);

			Vec3 Q(GetFirePos());
			Vec3 X0(posOut - Q);
			Vec3 V(dyn.v);//target velocity
			// solve a 2nd degree equation in t = time at which bullet and target will collide given their velocities
			float x0v = X0.Dot(V);
			float v02 = V.GetLengthSquared();
			float w02 = sp * sp;
			float x02 = X0.GetLengthSquared();
			float b = x0v;
			float sq = x0v * x0v - x02 * (v02 - w02);
			if (sq < 0)// bullet can't reach the target
				return;

			sq = sqrt(sq);
			float d = (v02 - w02);
			float t = (-b + sq) / d;
			float t1 = (-b - sq) / d;
			if (t < 0 && t1 > 0 || t1 > 0 && t1 < t)
				t = t1;
			if (t < 0)
				return;
			Vec3 W(X0 / t + V);//bullet velocity

			posOut = Q + W * t;
			//GetAISystem()->AddDebugLine(Q,posOut,255,255,255,3);
		}
	}
}

bool CPuppet::IsFireTargetValid(const Vec3& targetPos, const CAIObject* pTargetObject)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Accept the point if:
	// 1) Shooting in the direction hits something relatively far away
	// 2) There are no friendly units between the shooter and the target

	const Vec3& firePos = GetFirePos();
	Vec3 fireDir = targetPos - firePos;

	if (m_fireMode != FIREMODE_BURST_DRAWFIRE)
	{
		const float fireDirLen = fireDir.NormalizeSafe();

		const float minCheckDist = 1.0f;
		const float maxCheckDist = 15.0f;
		const float requiredTravelDistPercent = 0.25f;
		const float requiredDist = clamp_tpl(fireDirLen * requiredTravelDistPercent, minCheckDist, maxCheckDist);

		if (m_validTargetState.asyncState == AsyncReady)
			QueueFireTargetValidRay(pTargetObject, firePos, fireDir * requiredDist);

		if (m_validTargetState.latestHitDist < requiredDist)
			return false;
	}

	return !CheckFriendsInLineOfFire(fireDir, false);
}

void CPuppet::QueueFireTargetValidRay(const CAIObject* targetObj, const Vec3& firePos, const Vec3& fireDir)
{
	m_validTargetState.asyncState = AsyncInProgress;

	PhysSkipList skipList;
	GetPhysicalSkipEntities(skipList);

	if (targetObj)
	{
		if (targetObj->IsAgent())
		{
			if (const CAIActor* targetActor = targetObj->CastToCAIActor())
				targetActor->GetPhysicalSkipEntities(skipList);
		}
		else if (IPhysicalEntity* physicalEnt = targetObj->GetPhysics())
		{
			stl::push_back_unique(skipList, physicalEnt);
		}
	}

	m_validTargetState.rayID = gAIEnv.pRayCaster->Queue(
	  RayCastRequest::HighestPriority,
	  RayCastRequest(
	    firePos, fireDir, COVER_OBJECT_TYPES,
	    AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
	    &skipList[0], skipList.size()),
	  functor(*this, &CPuppet::FireTargetValidRayComplete));
}

void CPuppet::FireTargetValidRayComplete(const QueuedRayID& rayID, const RayCastResult& result)
{
	if (m_validTargetState.rayID == rayID)
	{
		m_validTargetState.rayID = 0;
		m_validTargetState.asyncState = AsyncReady;

		m_validTargetState.latestHitDist = result ? result[0].dist : FLT_MAX;
	}
}

//====================================================================
// ChooseMissPoint
//====================================================================
Vec3 CPuppet::ChooseMissPoint_Deprecated(const Vec3& vTargetPos) const
{
	CCCPOINT(CPuppet_ChooseMissPoint_Deprecated);

	int trysLimit(5); // how many times try to get good point to shoot

	Vec3 dir(vTargetPos - GetFirePos());
	float distToTarget = dir.len();
	if (distToTarget > 0.00001f)
		dir /= distToTarget;

	Matrix33 mat(Matrix33::CreateRotationXYZ(Ang3::GetAnglesXYZ(Quat::CreateRotationVDir(dir))));
	Vec3 right(mat.GetColumn(0));
	//          Vec3 up(b3DTarget ? mat.GetColumn(2).normalize(): ZERO);
	Vec3 up(mat.GetColumn(2));
	float spreadHoriz(0), spreadVert(0);

	while (--trysLimit >= 0)
	{
		//miss randomly on left/right
		spreadHoriz = cry_random(0.9f, 1.2f) * (cry_random(0, 99) < 50 ? -1.0f : 1.0f);
		//miss randomly on up/dowm - only in 3D
		//		spreadVert = cry_random(1.2f, 1.5f) * (cry_random(0, 99)<50 ? -1.0f : 1.0f);
		//miss randomly on up/dowm
		spreadVert = cry_random(0.0f, 1.5f) * (cry_random(0, 99) < 50 ? -1.0f : .35f);
		Vec3 candidateShootPos(vTargetPos + spreadHoriz * right + spreadVert * up);
		if (m_pFireCmdHandler->ValidateFireDirection(candidateShootPos - GetFirePos(), false))
			return candidateShootPos;
	}
	// can't find any good point to miss
	return ZERO;
}

//
//----------------------------------------------------------------------------------------------------
bool CPuppet::CheckAndGetFireTarget_Deprecated(IAIObject* pTarget, bool lowDamage, Vec3& vTargetPos, Vec3& vTargetDir) const
{
	//		CAIObject* pTargetAI = static_cast<CAIObject*>(pTarget);

	if (!m_pFireCmdHandler)
		return false;

	CAIActor* pTargetActor = pTarget->CastToCAIActor();
	DamagePartVector* pDamageParts = pTargetActor ? pTargetActor->GetDamageParts() : NULL;
	if (pDamageParts)
	{
		DamagePartVector& parts = *(pDamageParts);
		std::vector<float> weights;
		std::vector<int> partLut;

		// Check if the parts have multipliers set up.
		float accMult = 0.0f;
		int n = (int)parts.size();
		for (int i = 0; i < n; ++i)
			accMult += parts[i].damageMult;

		weights.resize(n);

		if (accMult > 0.001f)
		{
			// Sort based on damage multiplier.
			// Add slight random scaling to randomize selection of objects of same multiplier.
			for (int i = 0; i < n; ++i)
			{
				weights[i] = parts[i].damageMult * cry_random(1.0f, 1.01f);
				if (lowDamage && parts[i].damageMult > 0.95f) continue;
				partLut.push_back(i);
			}
		}
		else
		{
			// Sort based on part volume.
			// Add slight random scaling to randomize selection of objects of same multiplier.
			for (int i = 0; i < n; ++i)
			{
				// Sort from smallest volume to largest, hence negative sign.
				weights[i] = -parts[i].volume * cry_random(1.0f, 1.01f);
				partLut.push_back(i);
			}
		}

		std::sort(partLut.begin(), partLut.end(), SDamageLUTSorter(weights));

		for (std::vector<int>::iterator it = partLut.begin(); it != partLut.end(); ++it)
		{
			vTargetPos = parts[*it].pos;
			vTargetDir = vTargetPos - GetFirePos();
			if (m_pFireCmdHandler->ValidateFireDirection(vTargetDir, true))
				return true;
		}
	}
	else
	{
		// Inanimate target, use the default position.
		vTargetPos = pTarget->GetPos();
		vTargetDir = vTargetPos - GetFirePos();

		// The head is accessible.
		if (m_pFireCmdHandler->ValidateFireDirection(vTargetDir, false))
			return true;
	}

	// TODO: Should probably use one of those missed points instead, since they cannot reach the target anyway.
	vTargetPos = ChooseMissPoint_Deprecated(vTargetPos);

	return true;
}
