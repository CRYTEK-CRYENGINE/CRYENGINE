// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIHideObject.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 2:6:2005   16:26 : Created by Kirill Bulatsev
   - 2 Mar 2009			 : Evgeny Adamenkov: Replaced IRenderer with CDebugDrawContext

 *********************************************************************/

#include "StdAfx.h"
#include "CAISystem.h"
#include "AIHideObject.h"
#include "PipeUser.h"
#include <math.h>
#include "DebugDrawContext.h"
#include "HideSpot.h"

static const float LOW_COVER_OFFSET = 0.7f;
static const float HIGH_COVER_OFFSET = 1.7f;
static const int REFINE_SAMPLES = 4;
static const float STEP_SIZE = 0.75f;
static const float SAMPLE_DIST = 0.45f;
static const float SAMPLE_RADIUS = 0.15f;

static const float DEFAULT_AGENT_RADIUS = 0.4f;
static const IAISystem::tNavCapMask DEFAULT_AGENT_NAVMASK =
  IAISystem::NAV_TRIANGULAR |
  IAISystem::NAV_WAYPOINT_HUMAN;

//
//-------------------------------------------------------------------------------------------------------------
CAIHideObject::CAIHideObject()
	: m_bIsValid(false)
	, m_isUsingCover(false)
	, m_objectPos(ZERO)
	, m_objectDir(ZERO)
	, m_objectRadius(0.0f)
	, m_objectHeight(0.0f)
	, m_objectCollidable(false)
	, m_vLastHidePos(ZERO)
	, m_vLastHideDir(ZERO)
	, m_bIsSmartObject(false)
	, m_useCover(USECOVER_NONE)
	, m_dynCoverEntityId(0)
	, m_dynCoverEntityPos(ZERO)
	, m_dynCoverPosLocal(ZERO)
	, m_hideSpotType(SHideSpotInfo::eHST_INVALID)
	, m_coverPos(ZERO)
	, m_distToCover(0.0f)
	, m_pathOrig(ZERO)
	, m_pathDir(ZERO)
	, m_pathNorm(ZERO)
	, m_pathLimitLeft(0.0f)
	, m_pathLimitRight(0.0f)
	, m_tempCover(0.0f)
	, m_tempDepth(0.0f)
	, m_pathComplete(false)
	, m_highCoverValid(false)
	, m_lowCoverValid(false)
	, m_pathHurryUp(false)
	, m_lowLeftEdgeValid(false)
	, m_lowRightEdgeValid(false)
	, m_highLeftEdgeValid(false)
	, m_highRightEdgeValid(false)
	, m_lowCoverWidth(0.0f)
	, m_highCoverWidth(0.0f)
	, m_lowLeftEdge(0.0f)
	, m_lowRightEdge(0.0f)
	, m_highLeftEdge(0.0f)
	, m_highRightEdge(0.0f)
	, m_pathUpdateIter(0)
	, m_id(0)
{
}

//
//-------------------------------------------------------------------------------------------------------------
CAIHideObject::~CAIHideObject()
{

}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::Set(const SHideSpot* hs, const Vec3& hidePos, const Vec3& hideDir)
{
	CCCPOINT(AIHideObject_Set);

	Vec3 oldObjPos(m_objectPos);
	Vec3 oldObjDir(m_objectDir);
	float oldRadius(m_objectRadius);

	m_bIsValid = (hs != NULL);
	m_bIsSmartObject = m_bIsValid ? (hs->info.type == SHideSpotInfo::eHST_SMARTOBJECT) : false;

	m_dynCoverEntityId = 0;
	m_dynCoverEntityPos.Set(0, 0, 0);
	m_dynCoverPosLocal.Set(0, 0, 0);
	m_sAnchorName.clear();

	if (hs)
	{
		m_objectPos = hs->info.pos;
		m_objectDir = hs->info.dir;
		m_hideSpotType = hs->info.type;
		m_objectRadius = 0.0f;
		m_objectHeight = 0.0f;
		if (m_objectDir.IsZero())
		{
			m_objectDir = hideDir;
			if (m_objectDir.IsZero())
			{
				m_objectDir = m_objectPos - hidePos;               // We believe these are always different
				m_objectDir.NormalizeSafe();
			}
		}

		if (hs->pObstacle)
		{
			// (MATT) The m_ObjectPos line below wasn't used in Crysis gold - should check the impact {2008/08/15}
			// MTJ accurately reflect position of obstacle itself
			m_objectPos = hs->pObstacle->vPos;

			m_objectRadius = hs->pObstacle->fApproxRadius;
			if (m_objectRadius > 0.001f)
				m_objectCollidable = hs->pObstacle->IsCollidable() != 0;
			else
				m_objectCollidable = true;
			m_objectHeight = hs->pObstacle->GetApproxHeight();
		}
		if (hs->pAnchorObject)
		{
			if ((hs->pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT) ||
			    (hs->pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY))
			{
				m_objectCollidable = true;
				m_objectRadius = hs->pAnchorObject->GetRadius();
			}
			else
			{
				m_objectCollidable = false;
				m_objectRadius = 0.05f; // Omni directional.
			}

			m_sAnchorName = hs->pAnchorObject->GetName();
		}
		if (hs->info.type == SHideSpotInfo::eHST_SMARTOBJECT)
		{
			m_HideSmartObject = hs->SOQueryEvent;
			if (m_HideSmartObject.pRule->pObjectHelper)
				m_objectPos = m_HideSmartObject.pObject->GetHelperPos(m_HideSmartObject.pRule->pObjectHelper);
			else
				m_objectPos = m_HideSmartObject.pObject->GetPos();
			m_objectDir = m_HideSmartObject.pObject->GetOrientation(m_HideSmartObject.pRule->pObjectHelper);
			m_objectCollidable = true;
		}

		if (hs->info.type == SHideSpotInfo::eHST_DYNAMIC)
		{
			// Get the initial position of the entity which is associated with the dyn hidespot.
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(hs->entityId);
			if (pEnt)
			{
				// Store the original entity position.
				m_dynCoverEntityPos = pEnt->GetWorldPos();
				// Store the cover position in local entity space.
				m_dynCoverPosLocal = pEnt->GetWorldRotation().GetInverted() * (hs->info.pos - pEnt->GetWorldPos());
				// Store the entity id.
				m_dynCoverEntityId = hs->entityId;
			}
			else
			{
				// Could not find entity,
				m_dynCoverEntityId = 0;
				m_bIsValid = false;
			}
		}
		else
		{
			m_dynCoverEntityId = 0;
		}
	}

	if (m_bIsValid)
	{
		m_vLastHidePos = hidePos;
		m_vLastHideDir = hideDir;

		// Marcio: Avoid re-sampling if nothing changed!
		if (!oldObjPos.IsEquivalent(m_objectPos, 0.005f) || !oldObjDir.IsEquivalent(m_objectDir, 0.005f) || (fabs_tpl(oldRadius - m_objectRadius) > 0.005f))
		{
			m_useCover = USECOVER_NONE;
			m_pathUpdateIter = 0;
			m_pathComplete = false;
			m_pathHurryUp = false;
			m_isUsingCover = true;
			m_id++;
		}
	}

	if (!m_bIsValid && hidePos.GetLength() > 0.001f)
	{
		CryLog("Trying to set invalid hidespots!");
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::Update(CPipeUser* pOperand)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_bIsValid && !m_bIsSmartObject)
		UpdatePathExpand(pOperand);
}

bool ProjectedPointOnLine(float& u, const Vec3& lineOrig, const Vec3& lineDir, const Vec3& lineNorm, const Vec3& pt, const Vec3& target)
{
	Plane plane;
	plane.SetPlane(lineNorm, lineOrig);
	Ray r(target, pt - target);
	if (r.direction.IsZero(0.000001f))
		return false;
	Vec3 intr(pt);
	bool res = Intersect::Ray_Plane(r, plane, intr);
	u = lineDir.Dot(intr - lineOrig);
	return res;
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::GetCoverPoints(bool useLowCover, float peekOverLeft, float peekOverRight, const Vec3& targetPos, Vec3& hidePos,
                                   Vec3& peekPosLeft, Vec3& peekPosRight, bool& peekLeftClamped, bool& peekRightClamped, bool& coverCompromised) const
{
	// Calculate the extends of the shadow cast by the cover on the walkable line.
	// The returned hide point is in the middle of the umbra, and the peek points are at the extends.
	// The umbra is clipped to the walkable line and the clipping status is returned too.

	if (m_pathNorm.Dot(targetPos - m_pathOrig) < 0.0f)
	{
		hidePos = m_pathOrig;
		peekPosLeft = m_pathOrig;
		peekPosRight = m_pathOrig;
		peekLeftClamped = false;
		peekRightClamped = false;
		coverCompromised = true;

		return;
	}

	Plane plane;
	plane.SetPlane(m_pathNorm, m_pathOrig);

	// Initialize with the center point of the cover.
	float umbraLeft = FLT_MAX, umbraRight = -FLT_MAX;
	if (!m_pathComplete)
	{
		float u = 0.0f;
		if (ProjectedPointOnLine(u, m_pathOrig, m_pathDir, m_pathNorm, m_coverPos, targetPos))
			umbraLeft = umbraRight = u;
	}

	bool coverEmpty = false;

	// Calculate the umbra (shadow of the cover).
	std::deque<Vec3>::const_iterator begin, end;

	if (useLowCover)
	{
		begin = m_lowCoverPoints.begin();
		end = m_lowCoverPoints.end();
		coverEmpty = m_lowCoverPoints.empty();
	}
	else
	{
		begin = m_highCoverPoints.begin();
		end = m_highCoverPoints.end();
		coverEmpty = m_highCoverPoints.empty();
	}

	if (!coverEmpty)
	{
		int validPts = 0;

		for (std::deque<Vec3>::const_iterator it = begin; it != end; ++it)
		{
			float u = 0.0f;
			if (ProjectedPointOnLine(u, m_pathOrig, m_pathDir, m_pathNorm, (*it), targetPos))
			{
				umbraLeft = min(umbraLeft, u);
				umbraRight = max(umbraRight, u);
				validPts++;
			}
		}

		if (!validPts)
		{
			hidePos = m_pathOrig;
			peekPosLeft = m_pathOrig;
			peekPosRight = m_pathOrig;
			peekLeftClamped = false;
			peekRightClamped = false;
			coverCompromised = true;

			return;
		}
	}

	float mid = (umbraLeft + umbraRight) * 0.5f;

	// Allow to reach over the edge or stay almost visible.
	if (umbraLeft - peekOverLeft > mid)
		umbraLeft = mid;
	else
		umbraLeft -= peekOverLeft;

	if (umbraRight + peekOverRight < mid)
		umbraRight = mid;
	else
		umbraRight += peekOverRight;

	if (umbraLeft > umbraRight)
		umbraLeft = umbraRight = (umbraLeft + umbraRight) * 0.5f;

	// Clamp the umbra to the walkable line.
	if (umbraLeft < m_pathLimitLeft)
	{
		umbraLeft = m_pathLimitLeft;
		peekLeftClamped = true;
	}
	else if (umbraLeft > m_pathLimitRight)
	{
		umbraLeft = m_pathLimitRight;
		peekLeftClamped = true;
	}
	else
		peekLeftClamped = false;

	if (umbraRight > m_pathLimitRight)
	{
		umbraRight = m_pathLimitRight;
		peekRightClamped = true;
	}
	else if (umbraRight < m_pathLimitLeft)
	{
		umbraRight = m_pathLimitLeft;
		peekRightClamped = true;
	}
	else
		peekRightClamped = false;

	hidePos = m_pathOrig + m_pathDir * (umbraLeft + umbraRight) * 0.5f;
	peekPosLeft = m_pathOrig + m_pathDir * umbraLeft;
	peekPosRight = m_pathOrig + m_pathDir * umbraRight;

	if (coverEmpty)
		coverCompromised = m_pathComplete && fabsf((umbraLeft + umbraRight) * 0.5f) > 2.0f;
	else
		coverCompromised = m_pathComplete && (peekLeftClamped && peekRightClamped) && fabs(umbraRight - umbraLeft) < 0.001f;
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::GetCoverDistances(bool useLowCover, const Vec3& target, bool& coverCompromised, float& leftEdge, float& rightEdge, float& leftUmbra, float& rightUmbra) const
{
	Vec3 toTarget = (target - m_pathOrig).GetNormalized();

	if (m_pathNorm.Dot(toTarget) <= 0.2f) //~80º
	{
		leftUmbra = rightUmbra = 0.0f;
		leftEdge = rightEdge = 0.0f;
		coverCompromised = true;

		return;
	}

	bool coverEmpty = false;
	std::deque<Vec3>::const_iterator begin, end;

	leftUmbra = 0.0f;
	rightUmbra = 0.0f;

	if (useLowCover)
	{
		leftEdge = m_lowLeftEdge;
		rightEdge = m_lowRightEdge;

		begin = m_lowCoverPoints.begin();
		end = m_lowCoverPoints.end();
		coverEmpty = m_lowCoverPoints.empty();
	}
	else
	{
		leftEdge = m_highLeftEdge;
		rightEdge = m_highRightEdge;

		begin = m_highCoverPoints.begin();
		end = m_highCoverPoints.end();
		coverEmpty = m_highCoverPoints.empty();
	}

	if (!coverEmpty)
	{
		bool validFound = false;

		leftUmbra = FLT_MAX;
		rightUmbra = -FLT_MAX;

		for (std::deque<Vec3>::const_iterator it = begin; it != end; ++it)
		{
			float u = 0.0f;

			if (ProjectedPointOnLine(u, m_pathOrig, m_pathDir, m_pathNorm, *it, target))
			{
				leftUmbra = min(leftUmbra, u);
				rightUmbra = max(rightUmbra, u);

				validFound = true;
			}
		}

		if (!validFound)
		{
			leftUmbra = rightUmbra = 0.0f;
			leftEdge = rightEdge = 0.0f;
			coverCompromised = true;

			return;
		}
	}

	const float maxInsideDisplace = 0.75f;
	if ((leftUmbra > maxInsideDisplace) || (rightUmbra < -maxInsideDisplace))
		coverCompromised = true;

	if (leftUmbra < m_pathLimitLeft)
		leftUmbra = m_pathLimitLeft;
	if (leftUmbra > m_pathLimitRight)
		leftUmbra = m_pathLimitRight;

	if (rightUmbra > m_pathLimitRight)
		rightUmbra = m_pathLimitRight;
	if (rightUmbra < m_pathLimitLeft)
		rightUmbra = m_pathLimitLeft;

	if (!coverCompromised)
	{
		if (coverEmpty)
			coverCompromised = m_pathComplete;
		else
			coverCompromised = m_pathComplete && fabsf(rightUmbra - leftUmbra) < 0.5f;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
bool CAIHideObject::IsValid() const
{
	if (!m_bIsValid)
		return false;

	if (m_dynCoverEntityId)
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_dynCoverEntityId);
		if (!pEnt)
		{
			m_bIsValid = false;

			return false;
		}
		else
		{
			if (Distance::Point_PointSq(pEnt->GetWorldPos(), m_dynCoverEntityPos) > sqr(0.3f))
			{
				m_bIsValid = false;

				return false;
			}

			Vec3 curPos = pEnt->GetWorldPos() + pEnt->GetWorldRotation() * m_dynCoverPosLocal;

			if (Distance::Point_PointSq(m_objectPos, curPos) > sqr(0.3f))
			{
				m_bIsValid = false;

				return false;
			}
		}
	}

	return m_bIsValid;
}

//
//-------------------------------------------------------------------------------------------------------------
bool CAIHideObject::IsCompromised(const CPipeUser* pRequester, const Vec3& target) const
{
	if (!IsValid())
		return true;

	if (pRequester && pRequester->IsInCover() && !IsNearCover(pRequester))
		return true;

	if (!IsCoverPathComplete())
		return false;

	bool lowCompromised = !HasLowCover();
	bool highCompromised = !HasHighCover();

	float leftEdge;
	float rightEdge;
	float leftUmbra;
	float rightUmbra;

	if (!lowCompromised)
		GetCoverDistances(true, target, lowCompromised, leftEdge, rightEdge, leftUmbra, rightUmbra);

	if (!highCompromised)
		GetCoverDistances(false, target, highCompromised, leftEdge, rightEdge, leftUmbra, rightUmbra);

	return lowCompromised && highCompromised;
}

//
//-------------------------------------------------------------------------------------------------------------
bool CAIHideObject::IsNearCover(const CPipeUser* pRequester) const
{
	if (pRequester)
	{
		float safeRange = 0.25f;
		if (gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS)
			safeRange = 1.3f;

		float agentRadius = pRequester ? pRequester->GetParameters().m_fPassRadius : DEFAULT_AGENT_RADIUS;

		safeRange += agentRadius;

		float dist = GetDistanceToCoverPath(pRequester->GetPhysicsPos());
		if (dist > safeRange)
			return false;

		return true;
	}

	return false;
}

//
//-------------------------------------------------------------------------------------------------------------
float CAIHideObject::GetCoverWidth(bool useLowCover)
{
	if (!m_pathComplete)
	{
		// Over estimate a bit.
		if (useLowCover)
			return max(0.5f, m_lowCoverWidth);
		else
			return max(0.5f, m_highCoverWidth);
	}
	else
	{
		if (useLowCover)
			return m_lowCoverWidth;
		else
			return m_highCoverWidth;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
float CAIHideObject::GetDistanceToCoverPath(const Vec3& pt) const
{
	Lineseg coverPath(m_pathOrig + m_pathDir * m_pathLimitLeft, m_pathOrig + m_pathDir * m_pathLimitRight);
	float t;
	float distToLine = Distance::Point_Lineseg2D(pt, coverPath, t);
	Vec3 ptOnLine(coverPath.GetPoint(t));
	float heightDist = fabsf(ptOnLine.z - pt.z);
	return max(distToLine, heightDist);
}

float CAIHideObject::GetMaxCoverPathLen() const
{
	if (gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2)
	{
		if (m_objectRadius > 0.001f)
			return m_objectRadius * 2.0f;
	}

	return 12.0f;
}

Vec3 CAIHideObject::GetPointAlongCoverPath(float distance) const
{
	// TODO: Actually trace along the path points
	return m_pathOrig + m_pathDir * distance;
}

void CAIHideObject::GetCoverHeightAlongCoverPath(float distanceAlongPath, const Vec3& target, bool& hasLowCover, bool& hasHighCover) const
{
	/// low cover
	std::deque<Vec3>::const_iterator it = m_lowCoverPoints.begin();
	std::deque<Vec3>::const_iterator itEnd = m_lowCoverPoints.end();

	float lowDistanceLeft = -FLT_MAX;
	float lowDistanceRight = FLT_MAX;

	for (; it != itEnd; ++it)
	{
		float u = 0.0f;
		if (ProjectedPointOnLine(u, m_pathOrig, m_pathDir, m_pathNorm, *it, target))
		{
			if (u < distanceAlongPath)
				lowDistanceLeft = u;
			else if (u > distanceAlongPath)
			{
				lowDistanceRight = u;
				break;
			}
		}
	}

	bool lowRightEdge = (it == itEnd);

	if ((fabs_tpl(lowDistanceLeft - distanceAlongPath) <= 0.75f) &&
	    (lowRightEdge || (fabs_tpl(lowDistanceRight - distanceAlongPath) <= 0.75f)))
		hasLowCover = true;

	// high cover
	it = m_highCoverPoints.begin();
	itEnd = m_highCoverPoints.end();

	float highDistanceLeft = -FLT_MAX;
	float highDistanceRight = FLT_MAX;

	for (; it != itEnd; ++it)
	{
		float u = 0.0f;
		if (ProjectedPointOnLine(u, m_pathOrig, m_pathDir, m_pathNorm, *it, target))
		{
			if (u < distanceAlongPath)
				highDistanceLeft = u;
			else if (u > distanceAlongPath)
			{
				highDistanceRight = u;
				break;
			}
		}
	}

	bool highRightEdge = (it == itEnd);

	if ((fabs_tpl(highDistanceLeft - distanceAlongPath) < 0.75f) &&
	    (lowRightEdge || (fabs_tpl(highDistanceRight - distanceAlongPath) < 0.75f)))
		hasHighCover = true;
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::SetupPathExpand(CPipeUser* pOperand)
{
	Vec3 hidePos(GetObjectPos());
	Vec3 hideDir(GetObjectDir());
	float hideRadius(GetObjectRadius());
	float agentRadius = pOperand ? pOperand->GetParameters().m_fPassRadius : DEFAULT_AGENT_RADIUS;

	// Move the path origin on ground.
	Vec3 floorPos = GetLastHidePos();
	GetAISystem()->AdjustDirectionalCoverPosition(floorPos, hideDir, agentRadius, 0.7f);
	floorPos.z -= 0.7f;

	m_pathOrig = floorPos;
	m_distToCover = AGENT_COVER_CLEARANCE + agentRadius;

	m_pathNorm = m_vLastHideDir;
	m_coverPos = hidePos;

	m_pathNorm.z = 0;
	m_pathNorm.NormalizeSafe();
	m_pathDir.Set(m_pathNorm.y, -m_pathNorm.x, 0);

	Vec3 hitPos;
	float hitDist;

	Vec3 lowCoverOrig(m_pathOrig.x, m_pathOrig.y, m_pathOrig.z + LOW_COVER_OFFSET);
	Vec3 highCoverOrig(m_pathOrig.x, m_pathOrig.y, m_pathOrig.z + HIGH_COVER_OFFSET);

	m_lowCoverValid = IntersectSweptSphere(&hitPos, hitDist, Lineseg(lowCoverOrig, lowCoverOrig + m_pathNorm * (m_distToCover + 0.5f)), SAMPLE_RADIUS, AICE_ALL);
	m_highCoverValid = IntersectSweptSphere(&hitPos, hitDist, Lineseg(highCoverOrig, highCoverOrig + m_pathNorm * (m_distToCover + 0.5f)), SAMPLE_RADIUS, AICE_ALL);

	m_lowLeftEdgeValid = false;
	m_lowRightEdgeValid = false;
	m_highLeftEdgeValid = false;
	m_highRightEdgeValid = false;

	m_lowCoverPoints.clear();
	m_highCoverPoints.clear();

	m_lowLeftEdge = 0.0;
	m_lowRightEdge = 0.0;

	m_highLeftEdge = 0.0;
	m_highRightEdge = 0.0;

	m_pathLimitLeft = 0;
	m_pathLimitRight = 0;

	m_pathUpdateIter = 0;
	m_pathComplete = false;
}

//
//-------------------------------------------------------------------------------------------------------------
bool CAIHideObject::IsSegmentValid(CPipeUser* pOperand, const Vec3& posFrom, const Vec3& posTo)
{
	return false;
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::SampleCover(CPipeUser* pOperand, float& maxCover, float& maxDepth,
                                const Vec3& startPos, float maxWidth,
                                float sampleDist, float sampleRad, float sampleDepth,
                                std::deque<Vec3>& points, bool pushBack, bool& reachedEdge)
{
	//	const int REFINE_SAMPLES = 4;

	Vec3 hitPos;
	Vec3 checkDir = m_pathNorm * sampleDepth;
	maxCover = 0;
	maxDepth = 0;

	reachedEdge = false;

	// Linear rough samples

	int n = 1 + (int)floorf(fabs(maxWidth) / sampleDist);
	float deltaWidth = 1.0f / (float)n * maxWidth;

	for (int i = 0; i < n; i++)
	{
		float w = deltaWidth * (i + 1);
		Vec3 pos = startPos + m_pathDir * w;
		float d = 0;

		if (!IntersectSweptSphere(&hitPos, d, Lineseg(pos, pos + checkDir), sampleRad, AICE_ALL))
		{
			reachedEdge = true;
			break;
		}
		else
		{
			maxCover = w;
			if (pushBack)
				points.push_back(pos + m_pathNorm * d);
			else
				points.push_front(pos + m_pathNorm * d);
		}
		maxDepth = max(maxDepth, d);
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::SampleCoverRefine(CPipeUser* pOperand, float& maxCover, float& maxDepth,
                                      const Vec3& startPos, float maxWidth,
                                      float sampleDist, float sampleRad, float sampleDepth,
                                      std::deque<Vec3>& points, bool pushBack)
{
	Vec3 hitPos;
	int n = 1 + (int)floorf(fabs(maxWidth) / sampleDist);
	float deltaWidth = 1.0f / (float)n * maxWidth;
	Vec3 checkDir = m_pathNorm * sampleDepth;

	// Refine few iterations
	float t = 0.0f;
	float dt = 0.5f;
	for (int j = 0; j < REFINE_SAMPLES; j++)
	{
		Vec3 pos = startPos + m_pathDir * (maxCover + deltaWidth * t);

		float dist = 0;
		if (!IntersectSweptSphere(&hitPos, dist, Lineseg(pos, pos + checkDir), sampleRad, AICE_ALL))
			t -= dt;
		else
			t += dt;
		maxDepth = max(maxDepth, dist);
		dt *= 0.5f;
	}

	maxCover += deltaWidth * t;

	if (maxDepth < 0.01f)
		maxDepth = sampleDepth;

	if (pushBack)
		points.push_back(startPos + m_pathDir * maxCover + m_pathNorm * maxDepth);
	else
		points.push_front(startPos + m_pathDir * maxCover + m_pathNorm * maxDepth);
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::SampleLine(CPipeUser* pOperand, float& maxMove, float maxWidth, float sampleDist)
{
	// Linear rough samples
	Vec3 lastPos(m_pathOrig);
	int n = 1 + (int)floorf(fabs(maxWidth) / sampleDist);
	float deltaWidth = 1.0f / (float)n * maxWidth;

	for (int i = 0; i < n; i++)
	{
		const float w = deltaWidth * (i + 1);
		Vec3 pos = m_pathOrig + m_pathDir * w;
		if (!IsSegmentValid(pOperand, lastPos, pos))
			break;
		else
			maxMove = w;
		lastPos = pos;
	}
}

void CAIHideObject::SampleLineRefine(CPipeUser* pOperand, float& maxMove, float maxWidth, float sampleDist)
{
	// Refine few iterations
	Vec3 lastPos(m_pathOrig + m_pathDir * maxMove);
	int n = 1 + (int)floorf(fabs(maxWidth) / sampleDist);
	float deltaWidth = 1.0f / (float)n * maxWidth;

	float t = 0.0f;
	float dt = 0.5f;
	for (int j = 0; j < REFINE_SAMPLES; j++)
	{
		Vec3 pos = m_pathOrig + m_pathDir * (maxMove + deltaWidth * t);
		if (!IsSegmentValid(pOperand, lastPos, pos))
			t -= dt;
		else
			t += dt;
		dt *= 0.5f;
	}

	maxMove += deltaWidth * t;
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::UpdatePathExpand(CPipeUser* pOperand)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_pathUpdateIter == 0)
		SetupPathExpand(pOperand);

	if (m_pathComplete)
		return;

	const float LINE_LEN = GetMaxCoverPathLen();

	Vec3 lowCoverOrig(m_pathOrig.x, m_pathOrig.y, m_pathOrig.z + LOW_COVER_OFFSET);
	Vec3 highCoverOrig(m_pathOrig.x, m_pathOrig.y, m_pathOrig.z + HIGH_COVER_OFFSET);
	Vec3 hitPos;

	int maxIter = 1;
	int iter = 0;

	if (m_pathHurryUp)
		maxIter = 2;

	while (!m_pathComplete && iter < maxIter)
	{
		switch (m_pathUpdateIter)
		{
		case 0:
			SampleLine(pOperand, m_pathLimitLeft, -LINE_LEN / 2, STEP_SIZE);
			iter++;
			break;
		case 1:
			SampleLineRefine(pOperand, m_pathLimitLeft, -LINE_LEN / 2, STEP_SIZE);
			iter++;
			break;
		case 2:
			SampleLine(pOperand, m_pathLimitRight, LINE_LEN / 2, STEP_SIZE);
			iter++;
			break;
		case 3:
			SampleLineRefine(pOperand, m_pathLimitRight, LINE_LEN / 2, STEP_SIZE);
			iter++;
			break;
		case 4:
			if (m_lowCoverValid)
			{
				SampleCover(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, false, m_lowLeftEdgeValid);
				iter++;
			}
			break;
		case 5:
			if (m_lowCoverValid)
			{
				SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, false);
				iter++;
			}
			break;
		case 6:
			if (m_lowCoverValid)
			{
				SampleCover(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, true, m_lowRightEdgeValid);
				iter++;
			}
			break;
		case 7:
			if (m_lowCoverValid)
			{
				SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, true);
				iter++;
			}
			break;
		case 8:
			if (m_highCoverValid)
			{
				SampleCover(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, false, m_highLeftEdgeValid);
				iter++;
			}
			break;
		case 9:
			if (m_highCoverValid)
			{
				SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, false);
				iter++;
			}
			break;
		case 10:
			if (m_highCoverValid)
			{
				SampleCover(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, true, m_highRightEdgeValid);
				iter++;
			}
			break;
		case 11:
			if (m_highCoverValid)
			{
				SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, true);
				iter++;
			}
			break;
		default:
			m_pathComplete = true;
			break;
		}
		m_pathUpdateIter++;
	}

	// Update cover width.
	m_lowCoverWidth = 0.0f;
	m_highCoverWidth = 0.0f;

	float cmin, cmax;
	std::deque<Vec3>::iterator end;

	cmin = 0.0f;
	cmax = 0.0f;
	end = m_lowCoverPoints.end();
	for (std::deque<Vec3>::iterator it = m_lowCoverPoints.begin(); it != end; ++it)
	{
		float u = m_pathDir.Dot((*it) - m_pathOrig);
		cmin = min(cmin, u);
		cmax = max(cmax, u);
	}

	m_lowLeftEdge = cmin;
	m_lowRightEdge = cmax;

	// Adjust the cover to match the non-collidable cover width if applicable.
	if (m_pathComplete && !m_objectCollidable && m_objectRadius > 0.1f)
	{
		if (cmin > -m_objectRadius)
		{
			m_lowCoverPoints.push_front(lowCoverOrig + m_pathNorm * m_distToCover + m_pathDir * -m_objectRadius);
			m_lowLeftEdgeValid = true;
			cmin = -m_objectRadius;
		}
		m_lowLeftEdge = min(-m_objectRadius, cmin);

		if (cmax < m_objectRadius)
		{
			m_lowCoverPoints.push_back(lowCoverOrig + m_pathNorm * m_distToCover + m_pathDir * m_objectRadius);
			m_lowRightEdgeValid = true;
			cmax = m_objectRadius;
		}
		m_lowRightEdge = max(m_objectRadius, cmax);
	}

	m_lowCoverWidth = cmax - cmin;

	cmin = 0.0f;
	cmax = 0.0f;
	end = m_highCoverPoints.end();
	for (std::deque<Vec3>::iterator it = m_highCoverPoints.begin(); it != end; ++it)
	{
		float u = m_pathDir.Dot((*it) - m_pathOrig);
		cmin = min(cmin, u);
		cmax = max(cmax, u);
	}

	m_highLeftEdge = cmin;
	m_highRightEdge = cmax;

	// Adjust the cover to match the non-collidable cover width if applicable.
	if (m_pathComplete && !m_objectCollidable && m_objectRadius > 0.1f && m_objectHeight > 0.8f)
	{
		CCCPOINT(CAIHideObject_UpdatePathExpand_HighCover);
		if (cmin > -m_objectRadius)
		{
			m_highCoverPoints.push_front(highCoverOrig + m_pathNorm * m_distToCover + m_pathDir * -m_objectRadius);
			m_highLeftEdgeValid = true;
			cmin = -m_objectRadius;
		}
		m_highLeftEdge = min(-m_objectRadius, cmin);

		if (cmax < m_objectRadius)
		{
			m_highCoverPoints.push_back(highCoverOrig + m_pathNorm * m_distToCover + m_pathDir * m_objectRadius);
			m_highRightEdgeValid = true;
			cmax = m_objectRadius;
		}
		m_highRightEdge = max(m_objectRadius, cmax);
	}

	m_highCoverWidth = cmax - cmin;

	/*	if(m_lowCoverWidth < 0.01f && m_objectRadius > 0.01f)
	    m_lowCoverWidth = m_objectRadius * 2.0f;
	   if(m_highCoverWidth < 0.01f && m_objectRadius > 0.01f)
	    m_highCoverWidth = m_objectRadius * 2.0f;*/

	/*
	   SampleLine(pOperand, m_pathLimitLeft, -LINE_LEN/2, STEP_SIZE);
	   SampleLineRefine(pOperand, m_pathLimitLeft, -LINE_LEN/2, STEP_SIZE);
	   SampleLine(pOperand, m_pathLimitRight, LINE_LEN/2, STEP_SIZE);
	   SampleLineRefine(pOperand, m_pathLimitRight, LINE_LEN/2, STEP_SIZE);

	   if(m_lowCoverValid)
	   {
	    SampleCover(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, false);
	    SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, false);

	    SampleCover(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, true);
	    SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, true);
	   }

	   if(m_highCoverValid)
	   {
	    SampleCover(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, false);
	    SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, false);

	    SampleCover(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, true);
	    SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, true);
	   }*/

}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::DebugDraw()
{
	if (m_bIsSmartObject)
		return;

	ColorB white(255, 255, 255);
	ColorB red(255, 0, 0);
	ColorB redTrans(255, 0, 0, 128);
	ColorB green(0, 192, 0);
	ColorB greenTrans(0, 192, 0, 128);
	ColorB blue(0, 0, 255);

	CDebugDrawContext dc;

	dc->DrawLine(GetLastHidePos() + Vec3(0, 0, 0.5f), white, GetObjectPos() + Vec3(0, 0, 0.5f), white);
	dc->DrawLine(GetObjectPos() + Vec3(0, 0, -0.5f), white, GetObjectPos() + Vec3(0, 0, 2.5f), white);

	if (m_pathUpdateIter == 0)
		return;

	// Draw walkability
	dc->DrawLine(m_pathOrig + m_pathDir * m_pathLimitLeft, white,
	             m_pathOrig + m_pathDir * m_pathLimitRight, white);

	size_t maxPts = max(m_lowCoverPoints.size(), m_highCoverPoints.size());
	std::vector<Vec3> tris;
	tris.resize((maxPts + 1) * 12);

	// Draw low cover
	if (m_lowCoverPoints.size() > 1)
	{
		std::deque<Vec3>::iterator cur = m_lowCoverPoints.begin(), next = m_lowCoverPoints.begin();
		++next;
		size_t i = 0;
		while (next != m_lowCoverPoints.end())
		{
			const Vec3& left = (*cur);
			const Vec3& right = (*next);
			// Back
			tris[i + 0] = left + Vec3(0, 0, -0.5f);
			tris[i + 1] = right + Vec3(0, 0, 0.5f);
			tris[i + 2] = right + Vec3(0, 0, -0.5f);
			tris[i + 3] = left + Vec3(0, 0, -0.5f);
			tris[i + 4] = left + Vec3(0, 0, 0.5f);
			tris[i + 5] = right + Vec3(0, 0, 0.5f);
			i += 6;
			// Front
			tris[i + 0] = left + Vec3(0, 0, -0.5f);
			tris[i + 2] = right + Vec3(0, 0, 0.5f);
			tris[i + 1] = right + Vec3(0, 0, -0.5f);
			tris[i + 3] = left + Vec3(0, 0, -0.5f);
			tris[i + 5] = left + Vec3(0, 0, 0.5f);
			tris[i + 4] = right + Vec3(0, 0, 0.5f);
			i += 6;
			cur = next;
			++next;
		}
		AIAssert(i > 0);
		AIAssert(i <= tris.size());
		dc->DrawTriangles(&tris[0], i, greenTrans);

		// Draw edge markers
		if (m_lowLeftEdgeValid)
		{
			dc->DrawCone(m_lowCoverPoints.front() + Vec3(0, 0, 0.5f), Vec3(0, 0, -1), 0.07f, 1.2f, green);
		}

		if (m_lowRightEdgeValid)
		{
			dc->DrawCone(m_lowCoverPoints.front() + Vec3(0, 0, 0.5f), Vec3(0, 0, -1), 0.07f, 1.2f, green);
		}
	}

	// Draw high cover
	if (m_highCoverPoints.size() > 1)
	{
		std::deque<Vec3>::iterator cur = m_highCoverPoints.begin(), next = m_highCoverPoints.begin();
		++next;
		size_t i = 0;
		while (next != m_highCoverPoints.end())
		{
			const Vec3& left = (*cur);
			const Vec3& right = (*next);
			// Back
			tris[i + 0] = left + Vec3(0, 0, -0.5f);
			tris[i + 1] = right + Vec3(0, 0, 0.25f);
			tris[i + 2] = right + Vec3(0, 0, -0.5f);
			tris[i + 3] = left + Vec3(0, 0, -0.5f);
			tris[i + 4] = left + Vec3(0, 0, 0.25f);
			tris[i + 5] = right + Vec3(0, 0, 0.25f);
			i += 6;
			// Front
			tris[i + 0] = left + Vec3(0, 0, -0.5f);
			tris[i + 2] = right + Vec3(0, 0, 0.25f);
			tris[i + 1] = right + Vec3(0, 0, -0.5f);
			tris[i + 3] = left + Vec3(0, 0, -0.5f);
			tris[i + 5] = left + Vec3(0, 0, 0.25f);
			tris[i + 4] = right + Vec3(0, 0, 0.25f);
			i += 6;
			cur = next;
			++next;
		}
		AIAssert(i > 0);
		AIAssert(i <= tris.size());
		dc->DrawTriangles(&tris[0], i, greenTrans);

		// Draw edge markers
		if (m_highLeftEdgeValid)
		{
			dc->DrawCone(m_highCoverPoints.front() + Vec3(0, 0, 0.25f), Vec3(0, 0, -1), 0.07f, 0.75f, green);
		}

		if (m_highRightEdgeValid)
		{
			dc->DrawCone(m_highCoverPoints.back() + Vec3(0, 0, 0.25f), Vec3(0, 0, -1), 0.07f, 0.75f, green);
		}
	}

	if (m_pathComplete && (m_lowCoverPoints.size() < 2) && (m_highCoverPoints.size() < 2))
	{
		dc->DrawCylinder(m_objectPos + Vec3Constants<float>::fVec3_OneZ * HIGH_COVER_OFFSET * 0.25f,
		                 Vec3Constants<float>::fVec3_OneZ, DEFAULT_AGENT_RADIUS + AGENT_COVER_CLEARANCE, HIGH_COVER_OFFSET, redTrans, true);
	}
}

//
//----------------------------------------------------------------------------------------------------
bool CAIHideObject::HasLowCover() const
{
	return !m_lowCoverPoints.empty() && m_lowCoverPoints.size() > 1;
}

bool CAIHideObject::HasHighCover() const
{
	return !m_highCoverPoints.empty() && m_highCoverPoints.size() > 1;
}

bool CAIHideObject::IsLeftEdgeValid(bool useLowCover) const
{
	return useLowCover ? m_lowLeftEdgeValid : m_highLeftEdgeValid;
}

bool CAIHideObject::IsRightEdgeValid(bool useLowCover) const
{
	return useLowCover ? m_lowRightEdgeValid : m_highRightEdgeValid;
}

const char* CAIHideObject::GetAnchorName() const
{
	return m_sAnchorName.c_str();
}

//
//----------------------------------------------------------------------------------------------------
void CAIHideObject::Serialize(TSerialize ser)
{
	ser.BeginGroup("AIHideObject");
	{
		ser.Value("m_bIsValid", m_bIsValid);
		if (m_bIsValid)
		{
			ser.Value("m_isUsingCover", m_isUsingCover);
			ser.Value("m_objectPos", m_objectPos);
			ser.Value("m_objectDir", m_objectDir);
			ser.Value("m_objectRadius", m_objectRadius);
			ser.Value("m_objectCollidable", m_objectCollidable);
			ser.Value("m_vLastHidePos", m_vLastHidePos);
			ser.Value("m_vLastHideDir", m_vLastHideDir);
			ser.Value("m_bIsSmartObject", m_bIsSmartObject);
			m_HideSmartObject.Serialize(ser);
			ser.Value("m_useCover", m_useCover);
			ser.Value("m_coverPos", m_coverPos);
			ser.Value("m_distToCover", m_distToCover);
			ser.Value("m_pathOrig", m_pathOrig);
			ser.Value("m_pathDir", m_pathDir);
			ser.Value("m_pathNorm", m_pathNorm);
			ser.Value("m_pathLimitLeft", m_pathLimitLeft);
			ser.Value("m_pathLimitRight", m_pathLimitRight);
			ser.Value("m_tempCover", m_tempCover);
			ser.Value("m_tempDepth", m_tempDepth);
			ser.Value("m_pathComplete", m_pathComplete);
			ser.Value("m_highCoverValid", m_highCoverValid);
			ser.Value("m_lowCoverValid", m_lowCoverValid);
			ser.Value("m_pathHurryUp", m_pathHurryUp);
			ser.Value("m_lowLeftEdgeValid", m_lowLeftEdgeValid);
			ser.Value("m_lowRightEdgeValid", m_lowRightEdgeValid);
			ser.Value("m_highLeftEdgeValid", m_highLeftEdgeValid);
			ser.Value("m_highRightEdgeValid", m_highRightEdgeValid);
			ser.Value("m_lowCoverPoints", m_lowCoverPoints);
			ser.Value("m_highCoverPoints", m_highCoverPoints);
			ser.Value("m_lowCoverWidth", m_lowCoverWidth);
			ser.Value("m_highCoverWidth", m_highCoverWidth);
			ser.Value("m_pathUpdateIter", m_pathUpdateIter);
			ser.Value("m_id", m_id);
			ser.Value("m_dynCoverEntityId", m_dynCoverEntityId);
			ser.Value("m_dynCoverEntityPos", m_dynCoverEntityPos);
			ser.Value("m_dynCoverPosLocal", m_dynCoverPosLocal);
		}
		ser.EndGroup();
	}
}
