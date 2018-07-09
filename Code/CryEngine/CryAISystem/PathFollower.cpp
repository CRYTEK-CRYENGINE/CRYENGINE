// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   PathFollower.cpp
   Version:     v1.00
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 4 May 2009  : Evgeny Adamenkov: Replaced IRenderer with CDebugDrawContext

 *********************************************************************/

#include "StdAfx.h"

#include "PathFollower.h"
#include "CAISystem.h"
#include "AILog.h"
#include "NavPath.h"
#include "DebugDrawContext.h"

//===================================================================
// Serialize
//===================================================================
void PathFollowerParams::Serialize(TSerialize ser)
{
	ser.Value("normalSpeed", normalSpeed);
	ser.Value("pathRadius", pathRadius);
	ser.Value("pathLookAheadDist", pathLookAheadDist);
	ser.Value("maxAccel", maxAccel);
	ser.Value("maxDecel", maxDecel);
	ser.Value("minSpeed", minSpeed);
	ser.Value("maxSpeed", maxSpeed);
	ser.Value("endAccuracy", endAccuracy);
	ser.Value("endDistance", endDistance);
	ser.Value("stopAtEnd", stopAtEnd);
	ser.Value("use2D", use2D);
	ser.Value("snapEndPointToGround", snapEndPointToGround);
	ser.Value("isVehicle", isVehicle);
}

//===================================================================
// CPathFollower
//===================================================================
CPathFollower::CPathFollower(const PathFollowerParams& params)
	: m_params(params),
	m_pathVersion(-2),
	m_curLASegmentIndex(0),
	m_curLAPos(ZERO),
	m_lastOutputSpeed(0.0f),
	m_navPath(0),
	m_CurPos(ZERO),
	m_CurIndex(0)
{
}

//===================================================================
// CPathFollower
//===================================================================
CPathFollower::~CPathFollower()
{
}

//===================================================================
// DistancePointPoint
//===================================================================
float CPathFollower::DistancePointPoint(const Vec3 pt1, const Vec3 pt2) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	return m_params.use2D ? Distance::Point_Point2D(pt1, pt2) : Distance::Point_Point(pt1, pt2);
}

//===================================================================
// DistancePointPointSq
//===================================================================
float CPathFollower::DistancePointPointSq(const Vec3 pt1, const Vec3 pt2) const
{
	return m_params.use2D ? Distance::Point_Point2DSq(pt1, pt2) : Distance::Point_PointSq(pt1, pt2);
}

void CPathFollower::Reset()
{
	m_pathVersion = -2;
	m_curLASegmentIndex = 0;
	m_curLAPos = ZERO;
	m_lastOutputSpeed = 0.0f;

	m_pathControlPoints.resize(0);
}

//===================================================================
// AttachToPath
//===================================================================
void CPathFollower::AttachToPath(INavPath* pNavPath)
{
	m_navPath = pNavPath;
}

//===================================================================
// GetPathPtAhead
//===================================================================
static Vec3 GetPathPointAhead(const TPathPoints& pathPts, TPathPoints::const_iterator it, float dist)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	AIAssert(!pathPts.empty());
	if (dist >= 0.0f)
	{
		float curDist = 0.0f;
		Vec3 oldPos = it->vPos;
		while (++it != pathPts.end())
		{
			Vec3 pos = it->vPos;
			float segDist = Distance::Point_Point(oldPos, pos);

			float newDist = segDist + curDist;
			if (newDist > dist && segDist > 0.0f)
			{
				float frac = (dist - curDist) / segDist;
				Vec3 pt = frac * pos + (1.0f - frac) * oldPos;
				return pt;
			}

			curDist = newDist;
			oldPos = pos;
		}
		return pathPts.back().vPos;
	}
	else
	{
		TPathPoints pathPtsRev(pathPts);
		std::reverse(pathPtsRev.begin(), pathPtsRev.end());
		int n = (int)std::distance(pathPts.begin(), it);
		TPathPoints::iterator itRev = pathPtsRev.begin();
		std::advance(itRev, (int)(pathPtsRev.size() - (n + 1)));
		return ::GetPathPointAhead(pathPtsRev, itRev, -dist);
	}
}

//===================================================================
// ProcessPath
//===================================================================
void CPathFollower::ProcessPath()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IAISystem::tNavCapMask swingOutTypes = IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_VOLUME;

	AIAssert(m_navPath);
	m_pathVersion = m_navPath->GetVersion();

	m_pathControlPoints.clear();

	// TODO(marcio): fix
	CNavPath* pNavPath = static_cast<CNavPath*>(m_navPath);
	const TPathPoints& pathPts = pNavPath->GetPath();

	if (pathPts.size() < 2)
		return;

	const TPathPoints::const_iterator itEnd = pathPts.end();

	float totalLength = 0.0f;
	Vec3 prevPos = pathPts.front().vPos;
	for (TPathPoints::const_iterator it = pathPts.begin(); it != itEnd; ++it)
	{
		Vec3 delta = it->vPos - prevPos;
		totalLength += m_params.use2D ? delta.GetLength2D() : delta.GetLength();
		prevPos = it->vPos;
	}

	static bool useSwingout = true;

	float distFromStart = 0.0f;
	float distFromEnd = totalLength;

	static float startSwingoutDist = 1.0f;
	static float endSwingoutDist = 1.0f;
	// amount of swingout for 90 deg corner
	static float swingoutAmount = 1.0f;

	// add extra points this far in front of/behind each original point
	// only swingout if path seg dot product < this
	static float criticalDot = 0.9f;

	Vec3 oldPos = pathPts.front().vPos;
	for (TPathPoints::const_iterator it = pathPts.begin(); it != itEnd; ++it)
	{
		static float pathCurveDist = 2.0f;
		const PathPointDescriptor& ppd = *it;

		float prevSegLen = m_params.use2D ? Distance::Point_Point2D(ppd.vPos, oldPos) : Distance::Point_Point(ppd.vPos, oldPos);
		distFromStart += prevSegLen;
		distFromEnd -= prevSegLen;

		if (useSwingout && distFromStart > startSwingoutDist && distFromEnd > endSwingoutDist && (ppd.navType & swingOutTypes))
		{
			// current pt
			{
				Vec3 prevPos2 = ::GetPathPointAhead(pathPts, it, -pathCurveDist);
				Vec3 nextPos = ::GetPathPointAhead(pathPts, it, pathCurveDist);

				Vec3 swingOutDir = -(nextPos + prevPos2 - 2.0f * ppd.vPos) / square(pathCurveDist);
				float diff2Len = swingOutDir.NormalizeSafe();

				static float swingoutScale = 1.0f;
				float swingout = diff2Len * swingoutScale;
				Limit(swingout, 0.0f, 1.0f);

				Vec3 prevDir = ppd.vPos - oldPos;
				if (m_params.use2D) prevDir.z = 0.0f;
				prevDir.NormalizeSafe();

				TPathPoints::const_iterator itNext = it;
				++itNext;
				Vec3 nextPt = (itNext == itEnd) ? ppd.vPos : itNext->vPos;
				Vec3 nextDir = nextPt - ppd.vPos;
				if (m_params.use2D) nextDir.z = 0.0f;
				nextDir.NormalizeSafe();

				Vec3 prevOutDir(prevDir.y, -prevDir.x, 0.0f);
				Vec3 nextOutDir(nextDir.y, -nextDir.x, 0.0f);

				if (prevOutDir.Dot(swingOutDir) < 0.0f)
					prevOutDir *= -1.0f;
				if (nextOutDir.Dot(swingOutDir) < 0.0f)
					nextOutDir *= -1.0f;

				if (prevDir.Dot(nextDir) > criticalDot)
				{
					m_pathControlPoints.push_back(SPathControlPoint(ppd.navType, ppd.vPos, (prevOutDir + nextOutDir).GetNormalizedSafe(), swingout * swingoutAmount));
					m_pathControlPoints.back().customId = ppd.navTypeCustomId;
				}
				else
				{
					m_pathControlPoints.push_back(SPathControlPoint(ppd.navType, ppd.vPos, prevOutDir, swingout * swingoutAmount));
					m_pathControlPoints.back().customId = ppd.navTypeCustomId;
					m_pathControlPoints.push_back(SPathControlPoint(ppd.navType, ppd.vPos, nextOutDir, swingout * swingoutAmount));
					m_pathControlPoints.back().customId = ppd.navTypeCustomId;
				}
			}
		}
		else
		{
			m_pathControlPoints.push_back(SPathControlPoint(ppd.navType, ppd.vPos, ZERO, 0.0f));
			m_pathControlPoints.back().customId = ppd.navTypeCustomId;
		}
		oldPos = ppd.vPos;
	}

	// Now cut the path short if we should stop before the end
	if (m_params.endDistance > 0.0f)
	{
		float cutDist = 0.0f; // total amount we have cut
		for (int iPt = ((int) m_pathControlPoints.size()) - 2; iPt >= 0; --iPt)
		{
			int iNext = iPt + 1;
			Vec3 pt = GetPathControlPoint(iPt);
			Vec3 next = GetPathControlPoint(iNext);
			float dist = DistancePointPoint(pt, next);
			if (m_pathControlPoints.size() > 2 && (dist < 0.01f || cutDist + dist < m_params.endDistance))
			{
				m_pathControlPoints.pop_back();
				cutDist += dist;
			}
			else
			{
				float frac = clamp_tpl(1.0f - (m_params.endDistance - cutDist) / dist, 0.001f, 1.0f);
				m_pathControlPoints.back().pos = pt + (next - pt) * frac;
				break;
			}
		}
	}

	AIAssert(m_pathControlPoints.size() >= 2);

	Lineseg lastSeg;
	lastSeg.end = m_pathControlPoints.back().pos;
	lastSeg.start = m_pathControlPoints.front().pos;
	for (int iPt = ((int) m_pathControlPoints.size()) - 1; iPt >= 0; --iPt)
	{
		Vec3 pt = m_pathControlPoints[iPt].pos;
		float dist = DistancePointPoint(pt, lastSeg.end);
		if (dist > 0.01f)
		{
			lastSeg.start = pt;
			break;
		}
	}
	if (m_params.use2D)
		lastSeg.end.z = lastSeg.start.z;

	// replicate the last point, but advance it in the last segment direction
	m_pathControlPoints.push_back(m_pathControlPoints.back());
	Vec3 dir = (lastSeg.end - lastSeg.start).GetNormalizedSafe(Vec3Constants<float>::fVec3_OneX);
	m_pathControlPoints.back().offsetDir = dir;
	m_pathControlPoints.back().offsetAmount = m_params.stopAtEnd ? 0.01f : 10.0f;

	m_curLASegmentIndex = 0;
	m_curLAPos = GetPathControlPoint(0);
}

//===================================================================
// StartFollowing
//===================================================================
void CPathFollower::StartFollowing(const Vec3& curPos, const Vec3& curVel)
{
	float bestPathDistSq = std::numeric_limits<float>::max();
	int nPts = m_pathControlPoints.size();
	for (int i = 0; i < nPts - 1; ++i)
	{
		float t;
		Lineseg lineseg(GetPathControlPoint(i), GetPathControlPoint(i + 1));
		float distSq = Distance::Point_Lineseg2DSq(curPos, lineseg, t);
		if (distSq < bestPathDistSq)
		{
			bestPathDistSq = distSq;
			m_curLAPos = lineseg.GetPoint(t);
			m_curLASegmentIndex = i;
		}
	}

	Vec3 newLAPos;
	int newLASegIndex;
	// the large time passed just means that the initial lookahead should not be limited how
	// far it goes - except by the LA distance.
	GetNewLookAheadPos(newLAPos, newLASegIndex, m_curLAPos, m_curLASegmentIndex, curPos, curVel, 100.0f);
	m_curLAPos = newLAPos;
	m_curLASegmentIndex = newLASegIndex;

	// only start with curSpeed if we will move exactly aligned with the current velocity
	Vec3 velDir = curVel;
	Vec3 moveDir = (m_curLAPos - curPos);
	if (m_params.use2D)
	{
		velDir.z = 0.0f;
		moveDir.z = 0.0f;
	}
	velDir.NormalizeSafe();
	moveDir.NormalizeSafe();
	float dot = velDir.Dot(moveDir);
	Limit(dot, 0.0f, 1.0f);
	float curSpeed = m_params.use2D ? curVel.GetLength2D() : curVel.GetLength();
	m_lastOutputSpeed = curSpeed * dot;
}

//===================================================================
// GetNewLookAheadPos
// push the lookahead as far as possible without taking it beyond the lookahead
// distance from curPos
//===================================================================
void CPathFollower::GetNewLookAheadPos(Vec3& newLAPos, int& newLASegIndex, const Vec3& curLAPos, int curLASegIndex,
                                       const Vec3& curPos, const Vec3& curVel, float dt) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	newLASegIndex = curLASegIndex;
	newLAPos = curLAPos;

	int nPts = m_pathControlPoints.size();
	if (nPts < 2)
		return;

	// Danny todo ultimately do a binary search, or calculate this exactly. For now just probe ahead -
	// extremely slow but easy to test with and easy to optimise later
	float probeDist = m_params.pathLookAheadDist * 0.01f;

	// limit the total amount we probe ahead
	static float maxDistScale = 3.0f;
	float maxProbeDist = maxDistScale * dt * m_params.maxSpeed;
	float totalProbeDist = 0.0f; // how far probe moved in total

	float curDist;

	Vec3 prevPt, nextPt;
	int nextPtIndex;
	float segLen, distToNext;

	// need to execute this when newLASegIndex changes
#define CALC_SEGMENT                           \
  prevPt = GetPathControlPoint(newLASegIndex); \
  nextPtIndex = newLASegIndex + 1;             \
  if (nextPtIndex == nPts)                     \
  {                                            \
    newLAPos = prevPt;                         \
    return;                                    \
  }                                            \
  nextPt = GetPathControlPoint(nextPtIndex);   \
  segLen = DistancePointPoint(prevPt, nextPt); \
  distToNext = DistancePointPoint(newLAPos, nextPt)

	CALC_SEGMENT;

	while ((curDist = DistancePointPoint(newLAPos, curPos)) < m_params.pathLookAheadDist)
	{
		// rather than calculating exactly how far along the next segment - just go to the start of it
		float thisProbeDist = max(probeDist, m_params.pathLookAheadDist - curDist);
		bool returnEarly = false;
		if (thisProbeDist + totalProbeDist > maxProbeDist)
		{
			thisProbeDist = maxProbeDist - totalProbeDist;
			returnEarly = true;
		}

		if (thisProbeDist > distToNext)
		{
			newLAPos = nextPt;
			++newLASegIndex;
			totalProbeDist += distToNext;
			if (newLASegIndex > nPts - 2)
			{
				newLASegIndex = nPts - 2;
				return;
			}
			CALC_SEGMENT;
		}
		else
		{
			float distFromNext = distToNext - thisProbeDist;
			float frac = (distFromNext / segLen);
			newLAPos = frac * prevPt + (1.0f - frac) * nextPt;
			totalProbeDist += thisProbeDist;
			if (returnEarly)
				return;
			distToNext -= thisProbeDist;
		}
	}
}

//===================================================================
// UseLookAheadPos
//===================================================================
void CPathFollower::UseLookAheadPos(Vec3& velocity, bool& reachedEnd, const Vec3 LAPos, const Vec3 curPos, const Vec3 curVel,
                                    float dt, int curLASegmentIndex, float& lastOutputSpeed) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	velocity = LAPos - curPos;
	if (m_params.use2D)
		velocity.z = 0.0f;
	float dist = velocity.NormalizeSafe();
	float speed = m_params.normalSpeed * dist / m_params.pathLookAheadDist;

	// speed control depending on curvature
	static bool doCurvatureSpeedControl = true;
	if (doCurvatureSpeedControl)
	{
		float actualPathDist;
		static float pathDist = 3.0f;
		Vec3 posAhead = GetPathPointAhead(pathDist, actualPathDist, curLASegmentIndex, LAPos);
		if (actualPathDist > 0.1f)
		{
			float distToPosAhead = DistancePointPoint(posAhead, LAPos);
			float ratio = distToPosAhead / actualPathDist;
			static float minSpeedScale = 0.5f;
			static float multiplier = 3.0f;
			float speedScale = 1 + (ratio - 1) * multiplier;
			Limit(speedScale, minSpeedScale, 1.0f);
			speed *= speedScale;
		}
	}

	// speed control based on slowing down to stop at the end of the path
	// at distance s = vt - 0.5 * a * t^2
	// and time is given by t = v / a
	// so s = v^2 / a - 0.5 * v^2 / a
	// so v = sqrt(2 * a * s )
	// only do it if it's worth it

	// Marcio: Only do this if we want to stop at the end
	if (m_params.stopAtEnd)
	{
		float cutoffDist = 0.5f * square(speed) / min(3.0f, m_params.maxDecel);
		float directDistToEnd = DistancePointPoint(curPos, GetPathControlPoint(m_pathControlPoints.size() - 1));
		if (directDistToEnd < cutoffDist)
		{
			float distToEnd = GetDistToEnd(&curPos, curLASegmentIndex, LAPos);
			float maxSpeed = sqrtf(2.0f * m_params.maxDecel * distToEnd);
			if (speed > maxSpeed)
				speed = maxSpeed;
		}
	}

	// Allow to slow down to full stop at the end of the path.
	/*	const float slowDownDist = 3.0f;
	   const float slowDownSpeed = 5.0f;
	   const float minSpeedEndOfPath = 0.5f + slowDownSpeed * (directDistToEnd / slowDownDist);
	   float minSpeed = min(m_params.minSpeed, minSpeedEndOfPath);*/

	float maxOutputSpeed = min(lastOutputSpeed + dt * m_params.maxAccel, m_params.maxSpeed);
	float minOutputSpeed = m_params.stopAtEnd ? 0.0f : max(lastOutputSpeed - dt * m_params.maxDecel, m_params.minSpeed);

	Limit(speed, minOutputSpeed, maxOutputSpeed);
	velocity *= speed;
	lastOutputSpeed = speed;

	// only finish when we go just beyond the start of the last segment
	if (curLASegmentIndex >= (int)m_pathControlPoints.size() - 2)
	{
		Vec3 segStart = GetPathControlPoint(m_pathControlPoints.size() - 2);
		Vec3 segEnd = GetPathControlPoint(m_pathControlPoints.size() - 1);

		float distSq = (curPos - segStart).GetLengthSquared2D();

		if (m_params.stopAtEnd)
			reachedEnd = distSq < 0.05 * 0.05f;
		else
			reachedEnd = distSq < 0.25f * 0.25f;
	}
	else
		reachedEnd = false;
}

uint32 CPathFollower::GetIndex(const Vec3& pos) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	float bestPathDistSq = std::numeric_limits<float>::max();
	int nPts = m_pathControlPoints.size();

	uint32 ret = 0;

	for (int i = 0; i < nPts - 1; ++i)
	{
		float t;
		Lineseg lineseg(GetPathControlPoint(i), GetPathControlPoint(i + 1));
		float distSq = Distance::Point_Lineseg2DSq(pos, lineseg, t);
		if (distSq < bestPathDistSq)
		{
			bestPathDistSq = distSq;
			ret = i;
		}
	}

	return ret;
}

//===================================================================
// Update
// Method is to advance the lookahead as far as possible without exceeding
// the lookahead distance. Then move towards it.
// For the moment do no speed control
//===================================================================
bool CPathFollower::Update(PathFollowResult& result, const Vec3& curPos, const Vec3& curVel, float dt)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	float curSpeed = m_params.use2D ? curVel.GetLength2D() : curVel.GetLength();

	if (m_pathVersion != m_navPath->GetVersion())
	{
		ProcessPath();
		StartFollowing(curPos, curVel);
	}

	// clamp the last speed to +/- speedClampTol of the actual speed. This is mainly to
	// catch the case where the entity is limited by animation. We can't use the physics
	// speed exactly because there's no guarantee it is what we asked for last time
	// and using it can stop us accelerating
	static float speedClampTol = 0.5f;
	if (m_lastOutputSpeed > curSpeed + speedClampTol)
		m_lastOutputSpeed = curSpeed + speedClampTol;

	result.velocityOut.zero();
	if (result.predictedStates)
		result.predictedStates->resize(0);
	result.reachedEnd = true;

	// an extra point got added during preprocessing, so a real path will always be more than
	// two points
	if (m_pathControlPoints.size() < 2)
		return false;

	Vec3 newLAPos;
	int newLASegIndex;
	GetNewLookAheadPos(newLAPos, newLASegIndex, m_curLAPos, m_curLASegmentIndex, curPos, curVel, dt);

	m_CurPos = curPos;
	m_CurIndex = GetIndex(curPos);

	m_curLAPos = newLAPos;
	m_curLASegmentIndex = newLASegIndex;

	UseLookAheadPos(result.velocityOut, result.reachedEnd, m_curLAPos, curPos, curVel, dt, m_curLASegmentIndex, m_lastOutputSpeed);

	if (result.reachedEnd)
	{
		result.velocityOut.zero();
		return true;
	}

	if (result.predictedStates)
	{
		result.predictedStates->resize(0);
		int nDesiredPredictions = (int)(result.desiredPredictionTime / result.predictionDeltaTime + 0.5f);

		// Danny todo having a very small value here improves the smoothness of the prediction, but
		// it's expensive too.
		static float idealDt = 0.05f;
		int stepsPerPrediction = (int) (result.predictionDeltaTime / idealDt);
		float actualDt = result.predictionDeltaTime / stepsPerPrediction;

		// copy the variables so we use/modify them for prediction
		Vec3 predCurLAPos = m_curLAPos;
		int predCurLASegmentIndex = m_curLASegmentIndex;
		Vec3 predCurPos = curPos;
		Vec3 predCurVel = curVel;
		float lastOutputSpeed = m_lastOutputSpeed;

		for (int iPrediction = 0; iPrediction != nDesiredPredictions; ++iPrediction)
		{
			for (int iStep = 0; iStep < stepsPerPrediction; ++iStep)
			{
				Vec3 newLAPos2;
				int newLASegIndex2;
				GetNewLookAheadPos(newLAPos2, newLASegIndex2, predCurLAPos, predCurLASegmentIndex, predCurPos, predCurVel, actualDt);

				predCurLAPos = newLAPos2;
				predCurLASegmentIndex = newLASegIndex2;

				Vec3 velocity;
				bool reachedEnd;
				UseLookAheadPos(velocity, reachedEnd, predCurLAPos, predCurPos, predCurVel, actualDt, predCurLASegmentIndex, lastOutputSpeed);

				if (reachedEnd && m_params.stopAtEnd)
				{
					predCurVel.zero();
					predCurPos = GetPathControlPoint(m_pathControlPoints.size() - 2);
				}
				else
				{
					// now assume physics would do what it's told
					predCurPos += predCurVel * actualDt; // predCurVel seems slightly better than using velocity, or even midpoint method
					predCurVel = velocity;
				}
			}

			result.predictedStates->push_back(PathFollowResult::SPredictedState(predCurPos, predCurVel));
		}
	}

#ifndef _RELEASE
	if (gAIEnv.CVars.DrawPathFollower == 1)
	{
		Draw();
	}
#endif

	// This path follower always *assumes* the follow target is always reachable.
	return true;
}

//===================================================================
// GetDistToEnd
//===================================================================
float CPathFollower::GetDistToEnd(const Vec3* pCurPos, int curLASegmentIndex, Vec3 curLAPos) const
{
	if (m_pathControlPoints.empty())
		return 0.0f;

	float dist = 0.0f;
	int nPts = m_pathControlPoints.size();
	for (int i = curLASegmentIndex + 1; i < nPts - 2; ++i)
		dist += DistancePointPoint(GetPathControlPoint(i), GetPathControlPoint(i + 1));

	dist += DistancePointPoint(curLAPos, GetPathControlPoint(min(curLASegmentIndex + 1, nPts - 1)));
	if (pCurPos)
		dist += DistancePointPoint(*pCurPos, curLAPos);
	return dist;
}

//===================================================================
// GetDistToEnd
//===================================================================
float CPathFollower::GetDistToEnd(const Vec3* pCurPos) const
{
	return GetDistToEnd(pCurPos, m_curLASegmentIndex, m_curLAPos);
}

//===================================================================
// Serialize
//===================================================================
void CPathFollower::Serialize(TSerialize ser)
{
	ser.Value("m_params", m_params);
	ser.Value("m_curLAPos", m_curLAPos);
	ser.Value("m_curLASegmentIndex", m_curLASegmentIndex);
	ser.Value("m_lastOutputSpeed", m_lastOutputSpeed);
}

//===================================================================
// Draw
//===================================================================
void CPathFollower::Draw(const Vec3& drawOffset) const
{
	bool useTerrain = false;

	if (!m_pathControlPoints.empty() && m_pathControlPoints.front().navType == IAISystem::NAV_TRIANGULAR)
		useTerrain = true;

	CDebugDrawContext dc;

	Sphere LASphere(m_curLAPos, 0.2f);
	LASphere.center.z = dc->GetDebugDrawZ(LASphere.center, useTerrain);
	dc->DrawSphere(LASphere.center, LASphere.radius, ColorB(255, 0, 0));

	int nPts = m_pathControlPoints.size();
	for (int i = 1; i < nPts; ++i)
	{
		dc->DrawLine(GetPathControlPoint(i - 1), ColorB(0, 0, 0), GetPathControlPoint(i), ColorB(0, 0, 0));
		dc->DrawSphere(GetPathControlPoint(i - 1), 0.05f, ColorB(0, 0, 0));
	}
}

//===================================================================
// GetDistToSmartObject
//===================================================================
float CPathFollower::GetDistToSmartObject() const
{
	return GetDistToNavType(IAISystem::NAV_SMARTOBJECT);
}

float CPathFollower::GetDistToNavType(IAISystem::ENavigationType navType) const
{
	uint32 nPts = m_pathControlPoints.size();

	if (m_CurIndex + 1 >= nPts)
		return std::numeric_limits<float>::max();

	float curDist = DistancePointPoint(m_CurPos, m_pathControlPoints[m_CurIndex].pos);
	float totalDist = DistancePointPoint(m_pathControlPoints[m_CurIndex].pos, m_pathControlPoints[m_CurIndex + 1].pos);
	float curFraction = (totalDist > 0.0f) ? (curDist / totalDist) : 0.0f;

	if ((m_pathControlPoints[m_CurIndex].navType == navType) &&
	    (m_pathControlPoints[m_CurIndex + 1].navType == navType) &&
	    (m_pathControlPoints[m_CurIndex].customId == m_pathControlPoints[m_CurIndex + 1].customId))
		return (curFraction < 0.5f) ? 0.0f : std::numeric_limits<float>::max();

	float dist = 0.0f;
	Vec3 lastPos = m_CurPos;
	for (uint32 i = m_CurIndex + 1; i < nPts; ++i)
	{
		dist += DistancePointPoint(lastPos, m_pathControlPoints[i].pos);
		if (i + 1 < nPts)
		{
			if ((m_pathControlPoints[i].navType == navType) &&
			    (m_pathControlPoints[i + 1].navType == navType) &&
			    (m_pathControlPoints[i].customId == m_pathControlPoints[i + 1].customId))
				return dist;
		}
		else
		{
			if (m_pathControlPoints[i].navType == navType)
				return dist;
		}
		lastPos = m_pathControlPoints[i].pos;
	}
	return std::numeric_limits<float>::max();
}

//===================================================================
// GetDistToCustomNav
//===================================================================
float CPathFollower::GetDistToCustomNav(const TPathControlPoints& controlPoints, uint32 curLASegmentIndex, const Vec3& curLAPos) const
{
	int nPts = controlPoints.size();

	float dist = std::numeric_limits<float>::max();

	if (curLASegmentIndex + 1 < (uint32)nPts)
	{

		dist = 0.0f;

		if (controlPoints[curLASegmentIndex].navType != IAISystem::NAV_CUSTOM_NAVIGATION ||
		    controlPoints[curLASegmentIndex + 1].navType != IAISystem::NAV_CUSTOM_NAVIGATION)
		{
			Vec3 lastPos = curLAPos;

			for (int i = curLASegmentIndex + 1; i < nPts; ++i)
			{
				dist += DistancePointPoint(lastPos, m_pathControlPoints[i].pos);
				if (i + 1 < nPts)
				{
					if (controlPoints[i].navType == IAISystem::NAV_CUSTOM_NAVIGATION &&
					    controlPoints[i + 1].navType == IAISystem::NAV_CUSTOM_NAVIGATION)
					{
						break;
					}
				}
				else
				{
					if (controlPoints[i].navType == IAISystem::NAV_CUSTOM_NAVIGATION)
					{
						break;
					}
				}

				lastPos = controlPoints[i].pos;
			}
		}
	}

	return dist;
}

//===================================================================
// GetPathPointAhead
//===================================================================
Vec3 CPathFollower::GetPathPointAhead(float dist, float& actualDist, int curLASegmentIndex, Vec3 curLAPos) const
{
	Vec3 pt = curLAPos;
	actualDist = 0.0f;
	const int nPts1 = m_pathControlPoints.size() - 1;
	while (curLASegmentIndex++ < nPts1)
	{
		Vec3 nextPathPt = GetPathControlPoint(curLASegmentIndex);
		float distToNext = DistancePointPoint(pt, nextPathPt);

		float newDist = actualDist + distToNext;
		if (newDist > dist && (newDist - actualDist) > 0.0f)
		{
			float frac = (dist - actualDist) / (newDist - actualDist);
			pt = frac * nextPathPt + (1.0f - frac) * pt;
			actualDist = dist;
			return pt;
		}
		actualDist = newDist;
		pt = nextPathPt;
	}
	return pt;
}

//===================================================================
// GetPathPointAhead
//===================================================================
Vec3 CPathFollower::GetPathPointAhead(float dist, float& actualDist) const
{
	return GetPathPointAhead(dist, actualDist, m_CurIndex, m_CurPos);
}

//===================================================================
// Advance
//===================================================================
void CPathFollower::Advance(float distance)
{
	float distLeft = distance;
	while (distLeft > 0.0f && m_curLASegmentIndex < (int) m_pathControlPoints.size() - 1)
	{
		Vec3 nextPt = GetPathControlPoint(m_curLASegmentIndex + 1);
		float distToNext = DistancePointPoint(m_curLAPos, nextPt);
		if (distToNext > distLeft && distToNext > 0.0f)
		{
			float frac = distLeft / distToNext;
			m_curLAPos = frac * nextPt + (1.0f - frac) * m_curLAPos;
			return;
		}
		distLeft -= distToNext;
		m_curLAPos = nextPt;
		++m_curLASegmentIndex;
	}
}

//===================================================================
// CheckWalkability
//===================================================================
bool CPathFollower::CheckWalkability(const Vec2* path, const size_t length) const
{
	CRY_ASSERT(false); // not implemented
	return false;
}

//===================================================================
// GetAllowCuttingCorners
//===================================================================
bool CPathFollower::GetAllowCuttingCorners() const
{
	return true;
}

//===================================================================
// SetAllowCuttingCorners
//===================================================================
void CPathFollower::SetAllowCuttingCorners(const bool allowCuttingCorners)
{
	CRY_ASSERT(false); // not implemented
}

//===================================================================
// IsRemainingPathAffectedByNavMeshChange
//===================================================================
bool CPathFollower::IsRemainingPathAffectedByNavMeshChange(const NavigationMeshID affectedMeshID, const MNM::TileID affectedTileID, bool bAnnotationChange, bool bDataChange) const
{
	CRY_ASSERT(false);  // not implemented
	return false;
}
