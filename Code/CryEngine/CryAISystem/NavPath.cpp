// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   NavPath.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 13:1:2005   15:53 : Created by Kirill Bulatsev
   - 4 May 2009        : Evgeny Adamenkov: Replaced IRenderer with CDebugDrawContext

 *********************************************************************/

#include "StdAfx.h"
#include "NavPath.h"
#include "AILog.h"
#include "CAISystem.h"
#include <CryNetwork/ISerialize.h>
#include "AICollision.h"
#include "Puppet.h"

#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include "DebugDrawContext.h"

#include "Navigation/NavigationSystem/OffMeshNavigationManager.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "Navigation/MNM/OffGridLinks.h"

#include <numeric>
#include <algorithm>

void PathPointDescriptor::Serialize(TSerialize ser)
{
	ser.Value("vPos", vPos);
	ser.EnumValue("navType", navType, IAISystem::NAV_UNSET, IAISystem::NAV_MAX_VALUE);
	ser.EnumValue("navSOMethod", navSOMethod, nSOmNone, nSOmLast);
}

//====================================================================
// CNavPath
//====================================================================
CNavPath::CNavPath()
	: m_endDir(ZERO)
	, m_currentFrac(0.0f)
	, m_stuckTime(0.0f)
	, m_pathEndIsAsRequested(true)
	, m_fDiscardedPathLength(0)
{
}

//====================================================================
// ~CNavPath
//====================================================================
CNavPath::~CNavPath()
{
}

NavigationMeshID CNavPath::GetMeshID() const
{
	return m_params.meshID;
}

//====================================================================
// Advance
//====================================================================
bool CNavPath::Advance(PathPointDescriptor& nextPathPoint)
{
	if (m_pathPoints.size() <= 1)
		return false;
	++m_version.v;
	m_pathPoints.erase(m_pathPoints.begin());
	if (m_pathPoints.size() == 1)
	{
		m_currentFrac = 0.0f;
		return false;
	}
	nextPathPoint = *GetNextPathPoint();
	return true;
}

template<class F>
ILINE bool IsEquivalent2D(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, f32 epsilon = VEC_EPSILON)
{
	return  ((fabs_tpl(v0.x - v1.x) <= epsilon) && (fabs_tpl(v0.y - v1.y) <= epsilon));
}

//====================================================================
// push_front
//====================================================================
void CNavPath::PushFront(const PathPointDescriptor& newPathPoint, bool force)
{
	++m_version.v;
	if (!force && !m_pathPoints.empty())
	{
		IAISystem::ENavigationType newType = newPathPoint.navType;
		IAISystem::ENavigationType curType = m_pathPoints.front().navType;

		bool twoD = (curType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN)) != 0 ||
		            (newType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN)) != 0;

		// Don't allow null length segments!
		if (twoD ? IsEquivalent2D(newPathPoint.vPos, m_pathPoints.front().vPos) : IsEquivalent(newPathPoint.vPos, m_pathPoints.front().vPos))
		{
			if (newType != IAISystem::NAV_SMARTOBJECT && curType != IAISystem::NAV_SMARTOBJECT && newType != IAISystem::NAV_CUSTOM_NAVIGATION && curType != IAISystem::NAV_CUSTOM_NAVIGATION)
				return;
		}
	}
	m_pathPoints.insert(m_pathPoints.begin(), newPathPoint);
}

//====================================================================
// PushBack
//====================================================================
void CNavPath::PushBack(const PathPointDescriptor& newPathPoint, bool force)
{
	++m_version.v;
	// Don't allow null length segments!
	if (!force && !m_pathPoints.empty())
	{
		IAISystem::ENavigationType newType = newPathPoint.navType;
		IAISystem::ENavigationType curType = m_pathPoints.back().navType;
		bool twoD = (curType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN)) != 0 ||
		            (newType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN)) != 0;

		if (twoD ? IsEquivalent2D(newPathPoint.vPos, m_pathPoints.back().vPos) : IsEquivalent(newPathPoint.vPos, m_pathPoints.back().vPos))
		{
			if (newType != IAISystem::NAV_SMARTOBJECT && curType != IAISystem::NAV_SMARTOBJECT && newType != IAISystem::NAV_CUSTOM_NAVIGATION && curType != IAISystem::NAV_CUSTOM_NAVIGATION)
				return;
		}
	}
	m_pathPoints.push_back(newPathPoint);
}

//====================================================================
// GetLength
//====================================================================
float CNavPath::GetPathLength(bool b2D) const
{
	Vec3 vPos;
	if (!GetPosAlongPath(vPos))
		return 0.f;

	float length = 0.f;
	if (m_pathPoints.size() > 1)
	{
		TPathPoints::const_iterator li = m_pathPoints.begin();
		for (++li; li != m_pathPoints.end(); ++li)
		{
			const Vec3 vDelta = li->vPos - vPos;
			length += b2D ? vDelta.GetLength2D() : vDelta.GetLength();
			vPos = li->vPos;
		}
	}
	return length;
}

//====================================================================
// Draw
//====================================================================
void CNavPath::Draw(const Vec3& drawOffset) const
{
	bool useTerrain = false;

	CDebugDrawContext dc;

	if (!m_pathPoints.empty())
	{
		TPathPoints::const_iterator li, linext;
		li = m_pathPoints.begin();
		linext = li;
		++linext;
		while (linext != m_pathPoints.end())
		{
			Vec3 p0 = li->vPos;
			Vec3 p1 = linext->vPos;
			if (li->navType == IAISystem::NAV_TRIANGULAR)
				useTerrain = true;

			p0.z = dc->GetDebugDrawZ(p0, li->navType == IAISystem::NAV_TRIANGULAR);
			p1.z = dc->GetDebugDrawZ(p1, li->navType == IAISystem::NAV_TRIANGULAR);
			dc->DrawLine(p0, Col_SteelBlue, p1, Col_SteelBlue);

			ColorB color = Col_Grey;

			switch (li->navType)
			{
			case IAISystem::NAV_SMARTOBJECT:
				color = Col_SlateBlue;
				break;
			case IAISystem::NAV_TRIANGULAR:
				color = Col_Brown;
				break;
			case IAISystem::NAV_WAYPOINT_HUMAN:
				color = Col_Cyan;
				break;
			default:
				color = Col_Grey;
				break;
			}

			li = linext;
			++linext;
		}
	}

	if (!m_remainingPathPoints.empty())
	{
		TPathPoints::const_iterator li, linext;
		li = m_remainingPathPoints.begin();
		linext = li;
		++linext;
		while (linext != m_remainingPathPoints.end())
		{
			Vec3 p0 = li->vPos;
			Vec3 p1 = linext->vPos;
			if (li->navType == IAISystem::NAV_TRIANGULAR)
				useTerrain = true;

			p0.z = dc->GetDebugDrawZ(p0, li->navType == IAISystem::NAV_TRIANGULAR);
			p1.z = dc->GetDebugDrawZ(p1, li->navType == IAISystem::NAV_TRIANGULAR);
			dc->DrawLine(p0, Col_DarkOrchid, p1, Col_DarkOrchid);

			ColorB color = Col_Grey;

			switch (li->navType)
			{
			case IAISystem::NAV_SMARTOBJECT:
				color = Col_SlateBlue;
				break;
			case IAISystem::NAV_TRIANGULAR:
				color = Col_Brown;
				break;
			case IAISystem::NAV_WAYPOINT_HUMAN:
				color = Col_Cyan;
				break;
			default:
				color = Col_Grey;
				break;
			}
			dc->DrawSphere(p1, 0.125f, color);

			li = linext;
			++linext;
		}
	}

	std::list<SDebugLine>::const_iterator dli;
	for (dli = m_debugLines.begin(); dli != m_debugLines.end(); ++dli)
	{
		SDebugLine line = (*dli);
		line.P0.z = dc->GetDebugDrawZ(line.P0, useTerrain);
		line.P1.z = dc->GetDebugDrawZ(line.P1, useTerrain);
		dc->DrawLine(line.P0, line.col, line.P1, line.col);
	}
	std::list<SDebugSphere>::const_iterator dsi;
	for (dsi = m_debugSpheres.begin(); dsi != m_debugSpheres.end(); ++dsi)
	{
		SDebugSphere sphere = (*dsi);
		sphere.pos.z = dc->GetDebugDrawZ(sphere.pos, useTerrain);
		dc->DrawSphere(sphere.pos, sphere.r, sphere.col);
	}
}

//====================================================================
// empty
//====================================================================
bool CNavPath::Empty() const
{
	return m_pathPoints.size() < 2;
}

//====================================================================
// Clear
//====================================================================
void CNavPath::Clear(const char* dbgString)
{
	++m_version.v;
	if (gAIEnv.CVars.DebugPathFinding && !m_pathPoints.empty())
	{
		const PathPointDescriptor& ppd = m_pathPoints.back();
		AILogAlways("CNavPath::Clear old path end is (%5.2f, %5.2f, %5.2f) %s",
		            ppd.vPos.x, ppd.vPos.y, ppd.vPos.z, dbgString);
	}
	m_params.Clear();
	m_pathPoints.clear();
	m_debugLines.clear();
	m_debugSpheres.clear();
	m_currentFrac = 0.0f;
	m_stuckTime = 0.0f;
	m_pathEndIsAsRequested = true;
	m_fDiscardedPathLength = 0;
}

//====================================================================
// SetPreviousPoint
//====================================================================
void CNavPath::SetPreviousPoint(const PathPointDescriptor& previousPoint)
{
	if (m_pathPoints.empty())
		return;
	++m_version.v;
	m_pathPoints.front() = previousPoint;
}

//====================================================================
// DebugLine
//====================================================================
void CNavPath::DebugLine(const Vec3& P0, const Vec3& P1, const ColorF& col) const
{
	if (gAIEnv.CVars.DebugDraw == 0)
		return;
	SDebugLine line;
	line.P0 = P0;
	line.P1 = P1;
	line.col = col;
	m_debugLines.push_back(line);
}

void CNavPath::DebugSphere(const Vec3& pos, float r, const ColorF& col) const
{
	if (gAIEnv.CVars.DebugDraw == 0)
		return;
	SDebugSphere sphere;
	sphere.pos = pos;
	sphere.r = r;
	sphere.col = col;
	m_debugSpheres.push_back(sphere);
}

//====================================================================
// Serialize
//====================================================================
void CNavPath::Serialize(TSerialize ser)
{
	ser.BeginGroup("CNavPath");

	unsigned pathSize = m_pathPoints.size();
	ser.Value("pathSize", pathSize);

	if (ser.IsReading())
		m_pathPoints.resize(pathSize);

	if (pathSize > 0)
	{
		ser.Value("PathPoints", m_pathPoints);
	}

	ser.Value("m_endDir", m_endDir);
	m_debugLines.clear();
	m_debugSpheres.clear();
	ser.Value("m_currentFrac", m_currentFrac);
	ser.Value("m_stuckTime", m_stuckTime);
	ser.Value("m_pathEndIsAsRequested", m_pathEndIsAsRequested);
	m_params.Serialize(ser);

	ser.Value("m_fDiscardedPathLength", m_fDiscardedPathLength);
	ser.Value("m_version", m_version);

	ser.EndGroup();
}

//====================================================================
// UpdatePathPosition
// For each segment from the current one, if the agentPos lies beyond
// the end of the segment, use this segment and get the iterator position
// by projecting onto the segment and clamping.
// "beyond the end of the segment" is done by calculating two planes that
// divide up this segment from the next
//====================================================================
float CNavPath::UpdatePathPosition(Vec3 agentPos, float lookAhead, bool twoD, bool allowPathToFinish)
{
	float totalLen = GetPathLength(twoD);

	if (twoD)
		agentPos.z = 0.0f;

	while (m_pathPoints.size() >= 2)
	{
		TPathPoints::const_iterator pathIt = m_pathPoints.begin();
		TPathPoints::const_iterator pathItNext = pathIt;
		++pathItNext;

		// start and end of this segment
		Vec3 segStart = pathIt->vPos;
		Vec3 segEnd = pathItNext->vPos;

		float segLen = twoD ? (segStart - segEnd).GetLength2D() : (segStart - segEnd).GetLength();
		if (segLen > 0.1f)
		{
			// the end of the next segment (may be faked)
			Vec3 segEndNext;

			TPathPoints::const_iterator pathItNextNext = pathItNext;
			++pathItNextNext;
			if (pathItNextNext == m_pathPoints.end())
				segEndNext = segEnd + (segEnd - segStart);
			else
				segEndNext = pathItNextNext->vPos;

			if (twoD)
				segStart.z = segEnd.z = segEndNext.z = 0.0f;

			Vec3 segDir = (segEnd - segStart).GetNormalizedSafe();

			// don't advance until we're within the path width of the segment end
			Plane planeSeg = Plane::CreatePlane(segDir, segEnd);
			float planeSegDist = planeSeg.DistFromPlane(agentPos);
			if (planeSegDist < -0.6f * lookAhead)
			{
				// This segment is the one - calculate the fraction and then
				// clamp it - but don't go backwards!
				float newFrac = m_currentFrac;
				// all z values are already 0
				Distance::Point_LinesegSq(agentPos, Lineseg(segStart, segEnd), newFrac);
				if (newFrac > 1.0f)
					m_currentFrac = 1.0f;
				else if (newFrac > m_currentFrac)
					m_currentFrac = newFrac;
				return totalLen;
			}

			// past the divider between this and the next?
			Vec3 nextSegDir = (segEndNext - segEnd).GetNormalizedSafe(segDir);
			Vec3 avDir = (segDir + nextSegDir).GetNormalizedSafe(segDir);

			Plane planeSegDiv = Plane::CreatePlane(avDir, segEnd);
			float planeSegDivDist = planeSegDiv.DistFromPlane(agentPos);

			if (planeSegDivDist <= 0.0f)
			{
				// This segment is the one - calculate the fraction and then
				// clamp it - but don't go backwards!
				float newFrac = m_currentFrac;
				// all z values are already 0
				Distance::Point_LinesegSq(agentPos, Lineseg(segStart, segEnd), newFrac);
				if (newFrac > 1.0f)
					m_currentFrac = 1.0f;
				else if (newFrac > m_currentFrac)
					m_currentFrac = newFrac;

				return totalLen;
			}
		} // trivial segment length

		if (!allowPathToFinish && m_pathPoints.size() == 2)
		{
			m_currentFrac = 0.99f;
			return totalLen;
		}

		// moving onto the next segment
		PathPointDescriptor junk;
		Advance(junk);
		m_currentFrac = 0.0;
	}
	// got to the end
	m_currentFrac = 0.0f;
	return totalLen;
}

//====================================================================
// GetPosAlongPath
//====================================================================
bool CNavPath::GetPosAlongPath(Vec3& posOut, float dist, bool b2D, bool bExtrapolateBeyondEnd, IAISystem::ENavigationType* pNextPointType /*=NULL*/) const
{
	if (m_pathPoints.empty())
		return false;

	if (m_pathPoints.size() == 1)
	{
		const PathPointDescriptor& pointDesc = m_pathPoints.front();
		posOut = pointDesc.vPos;
		if (pNextPointType)
			*pNextPointType = pointDesc.navType;
		return true;
	}

	// We are at prevPt, a point of m_currentFrac completion of segment ["previous point", thisPt]
	TPathPoints::const_iterator it = ++m_pathPoints.begin();
	Vec3 thisPt = it->vPos;
	Vec3 prevPt = m_currentFrac * thisPt + (1.0f - m_currentFrac) * GetPrevPathPoint()->vPos;

	// Some clients want to know only the current position on path [6/3/2010 evgeny]
	if (dist == 0.f)
	{
		posOut = prevPt;
		if (pNextPointType)
			*pNextPointType = it->navType;
		return true;
	}

	const TPathPoints::const_iterator itEnd = m_pathPoints.end();

	// No initialization; the loop below will run at least once (we have >= 2 path points here).
	Vec3 vDelta;
	float fDeltaLen = 0.f;  // initializing to make the Static Analyzer happy
	assert(it != itEnd);

	float& fDistRemaining = dist;

	for (; it != itEnd; ++it)
	{
		thisPt = it->vPos;
		vDelta = thisPt - prevPt;
		fDeltaLen = b2D ? vDelta.GetLength2D() : vDelta.GetLength();
		if (fDistRemaining <= fDeltaLen)
		{
			float frac = fDeltaLen > 0.f ? fDistRemaining / fDeltaLen : 0.f;
			posOut = frac * thisPt + (1.f - frac) * prevPt;
			if (pNextPointType)
				*pNextPointType = it->navType;
			return true;
		}
		fDistRemaining -= fDeltaLen;
		prevPt = thisPt;
	}

	posOut = thisPt;

	if (bExtrapolateBeyondEnd)
	{
		float frac = fDeltaLen > 0.f ? fDistRemaining / fDeltaLen : 0.f;
		posOut += frac * vDelta;
	}

	if (pNextPointType)
		*pNextPointType = m_pathPoints.back().navType;

	return true;
}

//====================================================================
// GetPathProperties
//====================================================================
bool CNavPath::GetPathPropertiesAhead(const float distAhead, const bool twoD, Vec3& posOut, Vec3& dirOut,
                                      float* invRcOut, float& lowestPathDotOut, bool scaleOutputWithDist) const
{
	float dist = distAhead;
	if (invRcOut)
		*invRcOut = 0.0f;
	lowestPathDotOut = 1.0f;
	bool extrapolateBeyondEnd = false;

	if (m_pathPoints.size() < 2)
		return false;

	Vec3 thisPt = GetNextPathPoint()->vPos;
	Vec3 currentPos = m_currentFrac * thisPt + (1.0f - m_currentFrac) * GetPrevPathPoint()->vPos;
	Vec3 prevPt = currentPos;

	Vec3 firstSegDir = thisPt - currentPos;
	if (twoD)
		firstSegDir.z = 0.0f;
	firstSegDir.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
	dirOut = firstSegDir; // ensure it's not unset

	const TPathPoints::const_iterator itEnd = m_pathPoints.end();
	TPathPoints::const_iterator it;
	for (it = ++m_pathPoints.begin(); it != itEnd; ++it)
	{
		thisPt = it->vPos;
		Vec3 delta = thisPt - prevPt;
		float deltaLen = twoD ? delta.GetLength2D() : delta.GetLength();

		Vec3 currentSegDir = delta;
		if (twoD)
			currentSegDir.z = 0.0f;
		float segLen = currentSegDir.NormalizeSafe();
		if (segLen < 0.001f)
			continue;
		dirOut = currentSegDir;
		float pathDirDot = currentSegDir.Dot(firstSegDir);
		if (scaleOutputWithDist)
		{
			float frac = dist / distAhead;
			pathDirDot = frac * pathDirDot + (1.0f - frac) * 1.0f;
		}

		if (pathDirDot < lowestPathDotOut)
			lowestPathDotOut = pathDirDot;

		// calculate posOut here so it is correct when we exit the loop - will
		// adjust if necessary if we didn't need to extrapolate
		float frac = deltaLen > 0.0f ? dist / deltaLen : 1.0f;
		posOut = frac * thisPt + (1.0f - frac) * prevPt;

		if (dist <= deltaLen)
			break;
		dist -= deltaLen;
		prevPt = thisPt;
	}

	if (it == itEnd && !extrapolateBeyondEnd)
	{
		// fallen off the end
		posOut = thisPt;
	}

	if (invRcOut)
	{
		// estimate the radius of curvature by comparing the direct distance with the path distance, and assuming that
		// the path goes through a single point offset by h (in the middle) from the direct path
		float directDistance = twoD ? Distance::Point_Point2D(posOut, currentPos) : Distance::Point_Point(posOut, currentPos);

		if (directDistance < distAhead)
		{
			float d = 0.5f * directDistance;
			static float hScale = 0.5f; // approx scaling since the angled path isn't a circle
			float h = sqrt_tpl(square(0.5f * distAhead) - square(d));
			h *= hScale;
			float R = 0.5f * (square(h) + square(d)) / h;
			*invRcOut = 1.0f / R;
		}
		else
		{
			*invRcOut = 0.0f;
		}
	}
	return true;
}

//====================================================================
// DoesTargetSegPosExceedPathSeg
// returns true if targetSeg hits the inside of the infinite cylinder
// represented by pathSeg
//====================================================================
bool WouldTargetPosExceedPathSeg(const Vec3& targetPos, const Lineseg& pathSeg,
                                 float radius, bool twoD)
{
	if (twoD)
	{
		Vec3 segDir = (pathSeg.end - pathSeg.start);
		segDir.z = 0.0f;
		segDir.NormalizeSafe();
		Vec3 delta = (targetPos - pathSeg.start);
		delta.z = 0.0f;
		Vec3 perp = delta - segDir * delta.Dot(segDir);
		float distSq = perp.GetLengthSquared2D();
		return (distSq > square(radius));
	}
	else
	{
		Vec3 segDir = (pathSeg.end - pathSeg.start).GetNormalizedSafe();
		Vec3 delta = (targetPos - pathSeg.start);
		Vec3 perp = delta - segDir * delta.Dot(segDir);
		float distSq = perp.GetLengthSquared();
		return (distSq > square(radius));
	}
}

//====================================================================
// CalculateTargetPos
//====================================================================
Vec3 CNavPath::CalculateTargetPos(Vec3 agentPos, float lookAhead, float minLookAheadAlongPath, float pathRadius, bool twoD) const
{
	if (m_pathPoints.empty())
		return agentPos;
	if (m_pathPoints.size() == 1)
		return m_pathPoints.front().vPos;

	Vec3 pathPos = GetPrevPathPoint()->vPos * (1.0f - m_currentFrac) + GetNextPathPoint()->vPos * m_currentFrac;

	DebugLine(agentPos, pathPos, ColorF(0, 1, 0));
	DebugSphere(pathPos, 0.3f, ColorF(0, 1, 0));

	float distAgentToPathPos = twoD ? (pathPos - agentPos).GetLength2D() : (pathPos - agentPos).GetLength();
	Vec3 pathDelta = GetNextPathPoint()->vPos - pathPos;
	float distPathPosToCurrent = twoD ? pathDelta.GetLength2D() : pathDelta.GetLength();

	lookAhead -= distAgentToPathPos;

	if (lookAhead < minLookAheadAlongPath)
		lookAhead = minLookAheadAlongPath;

	if (lookAhead < distPathPosToCurrent)
	{
		Vec3 targetPos = pathPos + lookAhead * pathDelta / distPathPosToCurrent;
		return targetPos;
	}

	if (m_pathPoints.size() == 2)
	{
		lookAhead -= distPathPosToCurrent;

		Vec3 delta = GetNextPathPoint()->vPos - GetPrevPathPoint()->vPos;
		if (twoD)
			delta.z = 0.0f;
		float deltaLen = delta.NormalizeSafe();
		if (deltaLen < 0.01f)
		{
			return GetNextPathPoint()->vPos;
		}
		else
		{
			delta *= lookAhead;
			return GetNextPathPoint()->vPos + delta;
		}
	}
	// walk forward through the remaining path until we either reach lookAhead, or we (Danny todo) deviate from
	// the path

	// Binary search to find a suitable target position that doesn't intersect the path width.
	Vec3 targetPos;
	if (false == GetPosAlongPath(targetPos, lookAhead, twoD, true))
		return GetNextPathPoint()->vPos;

	Lineseg curSeg(GetPrevPathPoint()->vPos, GetNextPathPoint()->vPos);
	// if almost on the next seg use that one to allow the lookahead to progress
	if (const PathPointDescriptor* pNextNext = GetNextNextPathPoint())
	{
		Lineseg nextSeg(curSeg.end, pNextNext->vPos);
		float junk;
		float distToNextSegSq = twoD ? Distance::Point_Lineseg2DSq(agentPos, nextSeg, junk) : Distance::Point_LinesegSq(agentPos, nextSeg, junk);
		if (distToNextSegSq < square(pathRadius))
			curSeg = nextSeg;
	}

	if (WouldTargetPosExceedPathSeg(targetPos, curSeg, pathRadius, twoD))
	{
		lookAhead = minLookAheadAlongPath + 0.5f * (lookAhead - minLookAheadAlongPath);
		float delta = lookAhead * 0.5f;
		for (int i = 0; i < 8; ++i)
		{
			if (false == GetPosAlongPath(targetPos, lookAhead, twoD, true))
				return GetNextPathPoint()->vPos;
			if (WouldTargetPosExceedPathSeg(targetPos, curSeg, pathRadius, twoD))
				lookAhead -= delta;
			else
				lookAhead += delta;
			delta *= 0.5f;
		}
	}
	return targetPos;
}

//===================================================================
// GetPathDeviationDistance
//===================================================================
float CNavPath::GetPathDeviationDistance(Vec3& deviationOut, float criticalDeviation, bool twoD)
{
	deviationOut.zero();
	if (m_pathPoints.size() < 3)
		return 0.0f;

	TPathPoints::const_iterator it = m_pathPoints.begin();
	Lineseg thisSeg;
	thisSeg.start = (it++)->vPos;
	thisSeg.end = (it++)->vPos;

	float distOut = twoD ? (1.0f - m_currentFrac) * Distance::Point_Point2D(thisSeg.start, thisSeg.end) :
	                m_currentFrac* Distance::Point_Point(thisSeg.start, thisSeg.end);
	Vec3 lastPt = thisSeg.end;
	float lastDeviation = 0.0f;
	for (; it != m_pathPoints.end(); ++it)
	{
		Vec3 pt = it->vPos;
		float t;
		float deviation = twoD ? Distance::Point_Lineseg2D(pt, Lineseg(thisSeg.start, thisSeg.end), t) :
		                  Distance::Point_Lineseg(pt, Lineseg(thisSeg.start, thisSeg.end), t);
		float segLen = twoD ? Distance::Point_Point2D(lastPt, pt) : Distance::Point_Point(lastPt, pt);
		if (deviation > criticalDeviation)
		{
			float frac = 1.0f - (deviation - criticalDeviation) / (deviation - lastDeviation);
			distOut += frac * segLen;
			Vec3 exitPt = frac * pt + (1.0f - frac) * lastPt;

			Vec3 exitDelta = exitPt - thisSeg.start;
			Vec3 segDir = (thisSeg.end - thisSeg.start).GetNormalizedSafe();
			deviationOut = exitDelta - exitDelta.Dot(segDir) * segDir;
			if (twoD)
				deviationOut.z = 0.0f;
			return distOut;
		}
		distOut += segLen;
		lastPt = pt;
		lastDeviation = deviation;
	}

	return distOut;
}

//====================================================================
// Dump
//====================================================================
void CNavPath::Dump(const char* name) const
{
	AILogAlways("Path for %s", name);
	int i = 0;
	for (TPathPoints::const_iterator it = m_pathPoints.begin(); it != m_pathPoints.end(); ++it, ++i)
	{
		const PathPointDescriptor& ppd = *it;
		AILogAlways("pt %4d: %5.2f %5.2f %5.2f", i, ppd.vPos.x, ppd.vPos.y, ppd.vPos.z);
	}
}

//====================================================================
// Update
// Procedure is this:
// 1. Calculate a working position based on the input position, velocity
// and response time.
// 2. Calculate a new path position by advancing the old one.
// 3. Find a new target position by advancing from the iterator
// position
// 4. Adjust the target position to stop it taking the agent too far
// off the path
// 5. Calculate the path curvature and distance to destination
//====================================================================
bool CNavPath::UpdateAndSteerAlongPath(Vec3& dirOut, float& distToEndOut, float& distToPathOut, bool& isResolvingStickingOut,
                                       Vec3& pathDirOut, Vec3& pathAheadDirOut, Vec3& pathAheadPosOut,
                                       Vec3 currentPos, const Vec3& currentVel,
                                       float lookAhead, float pathRadius, float dt, bool resolveSticking, bool twoD)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	m_debugLines.clear();
	m_debugSpheres.clear();

	if (m_pathPoints.size() < 2)
		return false;

	/*
	   Vec3 deviation;
	   float maxDeviation = 0.5f * pathRadius;
	   float dist = GetPathDeviationDistance(deviation, maxDeviation, twoD);
	   float frac = dist < lookAhead ? dist / lookAhead : 1.0f - (dist - lookAhead)/5.0f;
	   if (frac > 0.0f)
	   currentPos += deviation * frac;
	 */

	// use a kind-of mid-point method
	const Vec3 workingPos = currentPos;// + currentVel * (0.5f * dt);

	Vec3 origPathPosition(currentPos);
	origPathPosition = GetPrevPathPoint()->vPos * (1.0f - m_currentFrac) + GetNextPathPoint()->vPos * m_currentFrac;

	// update the path position.
	distToEndOut = UpdatePathPosition(workingPos, lookAhead, twoD, true);
	distToPathOut = 0.0f;

	if (m_pathPoints.size() < 2)
		return false;

	float workingLookAhead = lookAhead;
	isResolvingStickingOut = false;
	if (resolveSticking && distToEndOut > lookAhead)
	{
		// prevent sticking if the path position isn't moving and the agent is some way from the path
		Vec3 newPathPosition = GetPrevPathPoint()->vPos * (1.0f - m_currentFrac) + GetNextPathPoint()->vPos * m_currentFrac;
		float dist = (newPathPosition - origPathPosition).GetLength();
		float expectedDist = twoD ? dt* currentVel.GetLength2D() : dt* currentVel.GetLength();

		distToPathOut = twoD ? (currentPos - newPathPosition).GetLength2D() : (currentPos - newPathPosition).GetLength();

		static float stickFrac = 0.1f;
		if (dist < stickFrac * expectedDist && distToPathOut > 0.5f * pathRadius)
			m_stuckTime += dt;
		else
			m_stuckTime -= min(m_stuckTime, dt);

		static float minStickTime = 0.2f;
		static float maxStickTime = 1.0f;
		static float stickTimeRate = 1.0f;
		if (m_stuckTime > minStickTime)
		{
			isResolvingStickingOut = true;
			workingLookAhead *= 1.0f / (1.0f + stickTimeRate * (m_stuckTime - minStickTime));
		}
		if (m_stuckTime > maxStickTime)
			m_stuckTime = maxStickTime;
	}
	else
	{
		Vec3 newPathPosition = GetPrevPathPoint()->vPos * (1.0f - m_currentFrac) + GetNextPathPoint()->vPos * m_currentFrac;
		distToPathOut = twoD ? (currentPos - newPathPosition).GetLength2D() : (currentPos - newPathPosition).GetLength();
	}

	distToEndOut += distToPathOut;

	// Calculate where the agent should head towards - makes sure this heading
	// doesn't deviate too much from the path even with a big look-ahead
	static float minLookAheadFrac = 0.1f;
	float minLookAheadAlongPath = minLookAheadFrac * lookAhead;
	pathAheadPosOut = CalculateTargetPos(workingPos, workingLookAhead, minLookAheadAlongPath, pathRadius, twoD);

	DebugLine(currentPos, pathAheadPosOut, ColorF(1, 0, 0));
	DebugSphere(pathAheadPosOut, 0.3f, ColorF(1, 0, 0));

	dirOut = (pathAheadPosOut - currentPos).GetNormalizedSafe(Vec3Constants<float>::fVec3_OneX);

	pathDirOut = (GetNextPathPoint()->vPos - GetPrevPathPoint()->vPos).GetNormalizedSafe();
	pathAheadDirOut = pathDirOut;

	// Sometimes even if the agent reached the steering point it wouldn't advance the path position
	// (especially for vehicles) so if the agent has already progressed up to the steer point then
	// gradually advance the current position.
	if ((currentPos - pathAheadPosOut).Dot(pathAheadPosOut - workingPos) > 0.0f)
	{
		float advanceDist = twoD ? (pathAheadPosOut - workingPos).GetLength2D() : (pathAheadPosOut - workingPos).GetLength();
		Vec3 pathNextPoint;
		if (GetPosAlongPath(pathNextPoint, advanceDist, true, false))
			UpdatePathPosition(pathNextPoint, 100.0f, true, false);
	}

	return true;
}

void CNavPath::PrepareNavigationalSmartObjectsForMNM(IEntity* pEntity)
{
	m_fDiscardedPathLength = 0.0f;

	// TODO: Make this work perhaps with an ISmartObjectUser interface?
	IAIObject* pAIObject = pEntity ? pEntity->GetAI() : NULL;
	CPipeUser* pPipeUser = pAIObject ? pAIObject->CastToCPipeUser() : NULL;

	if (!pPipeUser)
		return;

	for (TPathPoints::iterator it = m_pathPoints.begin(), itEnd = m_pathPoints.end(); it != itEnd; ++it)
	{
		if (it->navType == IAISystem::NAV_SMARTOBJECT)
		{
			//////////////////////////////////////////////////////////////////////////
			/// MNM Smart Objects glue code
			PathPointDescriptor& pathPointDescriptor = *it;
			++it;
			MNM::OffMeshLink* pOffMeshLink = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager()->GetOffMeshLink(pathPointDescriptor.offMeshLinkData.offMeshLinkID);
			OffMeshLink_SmartObject* pSOLink = pOffMeshLink ? pOffMeshLink->CastTo<OffMeshLink_SmartObject>() : NULL;
			if (pSOLink)
			{
				// Only cut at animation navSOs
				pathPointDescriptor.navSOMethod = static_cast<ENavSOMethod>
				                                  (gAIEnv.pSmartObjectManager->GetNavigationalSmartObjectActionTypeForMNM(
				                                     pPipeUser,
				                                     pSOLink->m_pSmartObject,
				                                     pSOLink->m_pSmartObjectClass,
				                                     pSOLink->m_pFromHelper,
				                                     pSOLink->m_pToHelper));

				const ENavSOMethod navSOMethod = pathPointDescriptor.navSOMethod;
				if (navSOMethod == nSOmStraight)
				{
					if (it != itEnd)
					{
						continue;
					}
					else
					{
						return;
					}
				}

				++m_version.v;
				if (it != itEnd)
				{
					// compute the length of the discarded path segment
					//m_fDiscardedPathLength = 0;
					Vec3 vPos = pathPointDescriptor.vPos;
					for (TPathPoints::const_iterator itNext = it; itNext != itEnd; ++itNext)
					{
						const Vec3& vNextPos = itNext->vPos;
						m_fDiscardedPathLength += Distance::Point_Point(vPos, vNextPos);
						vPos = vNextPos;
					}

					// Preserve the remaining path points for later
					m_remainingPathPoints.assign(it, itEnd);

					m_pathPoints.erase(it, itEnd);
					return;
				}
				else
				{
					m_pathPoints.pop_back();
					return;
				}
			}
			else  // if (pSONavData)
			{
				// Can be null only at the end of the path.
				AIAssert(0);
				return;
			}
		}
	}
}

void CNavPath::ResurrectRemainingPath()
{
	if (!m_remainingPathPoints.empty())
	{
		m_fDiscardedPathLength = 0.0f;
		m_pathPoints.swap(m_remainingPathPoints);
		m_remainingPathPoints.clear();
		m_version.v++;
	}
}

//===================================================================
// MovePathEndsOutOfObstacles
//===================================================================
void CNavPath::TrimPath(float trimLength, bool twoD)
{
	if (m_pathPoints.size() < 2)
		return;

	float initialLenght = GetPathLength(twoD);

	if (trimLength > 0.0f)
		trimLength = initialLenght - trimLength;
	else
		trimLength = -trimLength;

	if (trimLength > initialLenght)
		return;

	if (trimLength < 0.01f)
	{
		// Trim the whole path.
		++m_version.v;
		PathPointDescriptor pt = m_pathPoints.back();
		m_pathPoints.clear();
		m_pathPoints.push_back(pt);
		m_pathPoints.push_back(pt);
		return;
	}

	//	IAISystem::ENavigationType lastSafeNavType = IAISystem::NAV_UNSET;
	float runLength = 0.0;
	const TPathPoints::iterator itPathEnd = m_pathPoints.end();
	for (TPathPoints::iterator it = m_pathPoints.begin(); it != itPathEnd; ++it)
	{
		TPathPoints::iterator itNext = it;
		++itNext;
		if (itNext == itPathEnd)
			break;

		PathPointDescriptor& start = *it;
		PathPointDescriptor& end = *itNext;

		//		if (start.navType != IAISystem::NAV_SMARTOBJECT)
		//			lastSafeNavType = start.navType;

		float segmentLength;
		if (twoD)
			segmentLength = Distance::Point_Point2D(start.vPos, end.vPos);
		else
			segmentLength = Distance::Point_Point(start.vPos, end.vPos);

		if (trimLength >= runLength && trimLength < (runLength + segmentLength))
		{
			// If end point is navso, reset it.
			if (start.navType == IAISystem::NAV_SMARTOBJECT && end.navType == IAISystem::NAV_SMARTOBJECT)
			{
				// The cut would happen at a navso segment, allow to pass it.
				// The navso cutter will handle this case.
				return;
			}
			else
			{
				if (end.navType == IAISystem::NAV_SMARTOBJECT)
				{
					end.navType = start.navType;
					end.navSOMethod = nSOmNone;
				}

				float u = (trimLength - runLength) / segmentLength;
				end.vPos = start.vPos + u * (end.vPos - start.vPos);

				++itNext;
			}

			// Remove the end of the path.
			if (itNext != itPathEnd)
				m_pathPoints.erase(itNext, itPathEnd);

			++m_version.v;

			return;
		}

		runLength += segmentLength;
	}
}

Vec3 GetSafePositionInMesh(const NavigationMesh& mesh, const Vec3& testLocation, const float verticalRange, const float horizontalRange, const INavMeshQueryFilter* pFilter)
{
	Vec3 safePosition = testLocation;

	const MNM::vector3_t testLocationFixedPoint(MNM::real_t(testLocation.x), MNM::real_t(testLocation.y), MNM::real_t(testLocation.z));

	const MNM::real_t vRange(verticalRange);
	const MNM::real_t hRange(horizontalRange);

	if (!mesh.navMesh.GetTriangleAt(testLocationFixedPoint, vRange, vRange, pFilter))
	{
		MNM::vector3_t closestLocation;
		if (mesh.navMesh.GetClosestTriangle(testLocationFixedPoint, vRange, hRange, pFilter, nullptr, &closestLocation))
		{
			safePosition = closestLocation.GetVec3();
		}
	}

	return safePosition;
}

//===================================================================
// MovePathEndsOutOfObstacles
//===================================================================
void CNavPath::MovePathEndsOutOfObstacles(const CPathObstacles& obstacles, const INavMeshQueryFilter* pFilter)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_pathPoints.empty())
		return;

	++m_version.v;
	Vec3& pathStart = m_pathPoints.front().vPos;
	Vec3& pathEnd = m_pathPoints.back().vPos;
	const PathPointDescriptor& ppdStart = m_pathPoints.front();
	const PathPointDescriptor& ppdEnd = m_pathPoints.back();
	static float extra = 0.1f;

	//////////////////////////////////////////////////////////////////////////
	/// Note: When using MNM moving the points could get those out of the mesh

	const bool usingMNM = (GetMeshID() != NavigationMeshID(0));

	if (usingMNM)
	{
		const IAISystem::ENavigationType navTypeFilter = (IAISystem::ENavigationType)(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD | IAISystem::NAV_UNSET);

		const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(GetMeshID());
		const MNM::CNavMesh::SGridParams& gridParams = mesh.navMesh.GetGridParams();

		// Danny todo actually walk/back/forward moving all points out?
		if (ppdStart.navType & navTypeFilter)
		{
			const Vec3 newPosition = obstacles.GetPointOutsideObstacles(pathStart, extra);

			//Check if new position is safe
			if (!newPosition.IsEquivalent(pathStart))
			{
				const float displacement = (newPosition - pathStart).len();

				pathStart = GetSafePositionInMesh(mesh, newPosition - gridParams.origin, 2.0f, displacement / gridParams.voxelSize.x, pFilter);
			}
		}

		if (ppdEnd.navType & navTypeFilter)
		{
			Vec3 newPosition = obstacles.GetPointOutsideObstacles(pathEnd, extra);

			//Check if new position is safe
			if (!newPosition.IsEquivalent(pathEnd))
			{
				const float displacement = (newPosition - pathEnd).len();

				pathEnd = GetSafePositionInMesh(mesh, newPosition - gridParams.origin, 2.0f, displacement / gridParams.voxelSize.x, pFilter);
			}
		}
	}
	else
	{
		const IAISystem::ENavigationType navTypeFilter = (IAISystem::ENavigationType)(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD);

		// Danny todo actually walk/back/forward moving all points out?
		if (ppdStart.navType & navTypeFilter)
		{
			pathStart = obstacles.GetPointOutsideObstacles(pathStart, extra);
		}

		if (ppdEnd.navType & navTypeFilter)
		{
			pathEnd = obstacles.GetPointOutsideObstacles(pathEnd, extra);
		}
	}
}

//===================================================================
// CheckPath
// Currently just checks against forbidden edges, but should also check
// other things (depending on the navCapMask too). radius also needs to
// get used.
//===================================================================
bool CNavPath::CheckPath(const TPathPoints& pathList, float radius, const INavMeshQueryFilter* pFilter) const
{
	const bool usingMNM = (GetMeshID() != NavigationMeshID(0));

	if (usingMNM)
	{
		const size_t MaxWayTriangleCount = 512;
		MNM::TriangleID way[MaxWayTriangleCount] = { 0 };

		const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(GetMeshID());
		const MNM::CNavMesh::SGridParams& gridParams = mesh.navMesh.GetGridParams();
		const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(gridParams.origin.x), MNM::real_t(gridParams.origin.y), MNM::real_t(gridParams.origin.z));

		const MNM::real_t verticalRange(3.0f);

		MNM::vector3_t startLocation;
		MNM::vector3_t endLocation;

		MNM::TriangleID triangleStartID(0);
		MNM::TriangleID triangleEndID(0);

		for (TPathPoints::const_iterator it = pathList.begin(); it != pathList.end(); ++it)
		{
			TPathPoints::const_iterator itNext = it;
			if (++itNext != pathList.end())
			{
				const Vec3 from = it->vPos;
				const Vec3 to = itNext->vPos;

				if (triangleEndID)
				{
					startLocation = endLocation;
					endLocation.Set(MNM::real_t(to.x), MNM::real_t(to.y), MNM::real_t(to.z));

					triangleStartID = triangleEndID;
					triangleEndID = mesh.navMesh.GetTriangleAt(endLocation - origin, verticalRange, verticalRange, pFilter);
				}
				else
				{
					startLocation.Set(MNM::real_t(from.x), MNM::real_t(from.y), MNM::real_t(from.z));
					endLocation.Set(MNM::real_t(to.x), MNM::real_t(to.y), MNM::real_t(to.z));

					triangleStartID = mesh.navMesh.GetTriangleAt(startLocation - origin, verticalRange, verticalRange, pFilter);
					triangleEndID = mesh.navMesh.GetTriangleAt(endLocation - origin, verticalRange, verticalRange, pFilter);
				}

				if (!triangleStartID || !triangleEndID)
					return false;

				MNM::CNavMesh::RayCastRequest<512> raycastRequest;
				if (mesh.navMesh.RayCast(startLocation, triangleStartID, endLocation, triangleEndID, raycastRequest, pFilter) != MNM::CNavMesh::eRayCastResult_NoHit)
					return false;
			}
		}
	}
	return true;
}

//===================================================================
// AdjustPathAroundObstacleShape2D
//===================================================================
bool CNavPath::AdjustPathAroundObstacleShape2D(const SPathObstacleShape2D& obstacle, IAISystem::tNavCapMask navCapMask, const INavMeshQueryFilter* pFilter)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!(navCapMask & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD)))
		return true;

	if (m_pathPoints.empty() || obstacle.pts.empty())
		return true;

	const TPathPoints::iterator itPathEnd = m_pathPoints.end();
	// the path point before entering the shape
	TPathPoints::iterator itCutEntrance = itPathEnd;
	// the path point before exiting the shape
	TPathPoints::iterator itCutExit = itPathEnd;
	// the shape entrance/exit points
	Vec3 entrancePoint(ZERO);
	Vec3 exitPoint(ZERO);
	// counters to make sure we record the correct entrance/exit points
	float entranceCounter = std::numeric_limits<float>::max();
	float exitCounter = -std::numeric_limits<float>::max();
	// entrance/exit indices (and fractions) so that the entrance/exit points lie on the segment between
	// these indices and the index+1
	int entranceShapeIndex = -1;
	int exitShapeIndex = -1;
	float entranceShapeFrac = -1;
	float exitShapeFrac = -1;

	const unsigned pointAmount = obstacle.pts.size();
	const Vec3 polyCenter = std::accumulate(obstacle.pts.begin(), obstacle.pts.end(), Vec3(ZERO)) / (float)pointAmount;

	const float offset = 0.1f;

	const bool usingMNM = (GetMeshID() != NavigationMeshID(0));
	const IAISystem::ENavigationType startNavTypeFilter = usingMNM ? (IAISystem::ENavigationType)(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD | IAISystem::NAV_UNSET) : (IAISystem::ENavigationType)(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD);
	const IAISystem::ENavigationType endNavTypeFilter = (IAISystem::ENavigationType)(startNavTypeFilter | IAISystem::NAV_SMARTOBJECT);

	int segCounter = 0;
	for (TPathPoints::iterator it = m_pathPoints.begin(); it != itPathEnd; ++it, ++segCounter)
	{
		TPathPoints::iterator itNext = it;
		++itNext;
		if (itNext == itPathEnd)
			break;

		PathPointDescriptor& start = *it;
		PathPointDescriptor& end = *itNext;

		// Danny todo - problem if we start in tri, skip some indoor, then end in tri?
		if (!(start.navType & startNavTypeFilter) || !(end.navType & endNavTypeFilter))
			continue;

		Lineseg pathSegment(start.vPos, end.vPos);
		Vec3 intersectPos;

		// if the seg intersection is for the entrance then this gets set so we know
		// whether to over-ride it

		for (unsigned pointIndex = 0; pointIndex < pointAmount; ++pointIndex)
		{
			unsigned nextPointIndex = (pointIndex + 1) % pointAmount;
			Lineseg obstacleSegment(obstacle.pts[pointIndex], obstacle.pts[nextPointIndex]);

			float pathSegmentFraction, obstacleSegmentFraction;
			if (!Intersect::Lineseg_Lineseg2D(pathSegment, obstacleSegment, pathSegmentFraction, obstacleSegmentFraction))
				continue;

			const Vec3 intersectionPoint = pathSegment.GetPoint(pathSegmentFraction);

			const float zTolerance = 0.1f;
			const bool isObstacleOnTheSameLayerAsThePathSegment = (intersectionPoint.z > obstacle.minZ - zTolerance) && (intersectionPoint.z < obstacle.maxZ + zTolerance);
			if (!isObstacleOnTheSameLayerAsThePathSegment)
				continue;

			if ((segCounter + pathSegmentFraction) < entranceCounter)
			{
				entranceCounter = segCounter + pathSegmentFraction;
				itCutEntrance = it;
				entrancePoint = intersectionPoint;
				entranceShapeIndex = pointIndex;
				entranceShapeFrac = obstacleSegmentFraction;
				entrancePoint = entrancePoint + Vec2(entrancePoint - polyCenter).GetNormalized() * offset;
			}
			if ((segCounter + pathSegmentFraction) > exitCounter)
			{
				exitCounter = segCounter + pathSegmentFraction;
				itCutExit = it;
				exitPoint = intersectionPoint;
				exitShapeIndex = pointIndex;
				exitShapeFrac = obstacleSegmentFraction;
				exitPoint = exitPoint + Vec2(entrancePoint - polyCenter).GetNormalized() * offset;
			}
		}
	}

	if (itCutEntrance == itPathEnd || itCutExit == itPathEnd)
		return true;

	++m_version.v;

	AIAssert(entranceShapeIndex >= 0 && entranceShapeIndex < (int)pointAmount);
	AIAssert(exitShapeIndex >= 0 && exitShapeIndex < (int)pointAmount);

	TPathPoints::iterator itCutExitAfter = itCutExit;
	++itCutExitAfter;
	AIAssert(itCutExitAfter != itPathEnd);
	AIAssert(exitCounter >= entranceCounter);

	// First remove any intermediate points in the path list.
	for (TPathPoints::iterator it = itCutEntrance; it != itCutExitAfter; )
	{
		if (it != itCutEntrance)
			it = m_pathPoints.erase(it);
		else
			++it;
	}

	// now insert the extra points before itCutExitAfter
	IAISystem::ENavigationType navType = itCutEntrance->navType;

	float distFwd = 0.0f;
	float distBwd = 0.0f;
	TPathPoints pathFwd;
	TPathPoints pathBwd;

	pathFwd.push_back(PathPointDescriptor(navType, entrancePoint));
	pathBwd.push_back(PathPointDescriptor(navType, entrancePoint));

	// if we enter/exit on the same edge then we need to make sure that we don't cut
	// through a forbidden edge - so still calculate the two routes (but fake the distances
	// for convenience)
	if (entranceShapeIndex == exitShapeIndex)
	{
		bool shortestRouteIsFwd = entranceShapeFrac < exitShapeFrac;
		distFwd = shortestRouteIsFwd ? 0.0f : 1.0f;
		distBwd = shortestRouteIsFwd ? 1.0f : 0.0f;

		if (shortestRouteIsFwd)
		{
			bool doingFirst = true;
			for (int iPt = entranceShapeIndex; doingFirst || iPt != exitShapeIndex; )
			{
				doingFirst = false;
				Vec3 aheadPt = obstacle.pts[iPt];
				aheadPt.z = entrancePoint.z;
				aheadPt = aheadPt + Vec2(aheadPt - polyCenter).GetNormalized() * offset;
				pathBwd.push_back(PathPointDescriptor(navType, aheadPt));
				if (--iPt < 0)
					iPt = pointAmount - 1;
			}
		}
		else
		{
			bool doingFirst = true;
			for (int iPt = entranceShapeIndex; doingFirst || iPt != exitShapeIndex; )
			{
				doingFirst = false;
				int iAheadPt = (iPt + 1) % pointAmount;
				Vec3 aheadPt = obstacle.pts[iAheadPt];
				aheadPt.z = entrancePoint.z;
				aheadPt = aheadPt + Vec2(aheadPt - polyCenter).GetNormalized() * offset;
				pathFwd.push_back(PathPointDescriptor(navType, aheadPt));
				if (++iPt >= (int)pointAmount)
					iPt = 0;
			}
		}
	}
	else
	{
		// Generate paths around the object in both directions to (a) choose the shortest
		// and (b) be able to choose one that is less problematic
		distFwd = (obstacle.pts[exitShapeIndex] - exitPoint).GetLength2D();
		distBwd = (obstacle.pts[(exitShapeIndex + 1) % pointAmount] - exitPoint).GetLength2D();

		// fwd
		Vec3 lastPt = entrancePoint;
		for (int iPt = entranceShapeIndex; iPt != exitShapeIndex; )
		{
			int iAheadPt = (iPt + 1) % pointAmount;
			Vec3 aheadPt = obstacle.pts[iAheadPt];
			aheadPt.z = entrancePoint.z;
			aheadPt = aheadPt + Vec2(aheadPt - polyCenter).GetNormalized() * offset;
			pathFwd.push_back(PathPointDescriptor(navType, aheadPt));
			distFwd += (aheadPt - lastPt).GetLength2D();
			lastPt = aheadPt;
			if (++iPt >= (int)pointAmount)
				iPt = 0;
		}
		// bwd
		lastPt = entrancePoint;
		for (int iPt = entranceShapeIndex; iPt != exitShapeIndex; )
		{
			Vec3 aheadPt = obstacle.pts[iPt];
			aheadPt.z = entrancePoint.z;
			aheadPt = aheadPt + Vec2(aheadPt - polyCenter).GetNormalized() * offset;
			pathBwd.push_back(PathPointDescriptor(navType, aheadPt));
			distBwd += (aheadPt - lastPt).GetLength2D();
			lastPt = aheadPt;
			if (--iPt < 0)
				iPt = pointAmount - 1;
		}
	}

	pathFwd.push_back(PathPointDescriptor(navType, exitPoint));
	pathBwd.push_back(PathPointDescriptor(navType, exitPoint));

	float checkRadius = 0.3f;
	if (distFwd < distBwd)
	{
		if (CheckPath(pathFwd, checkRadius, pFilter))
			m_pathPoints.insert(itCutExitAfter, pathFwd.begin(), pathFwd.end());
		else if (CheckPath(pathBwd, checkRadius, pFilter))
			m_pathPoints.insert(itCutExitAfter, pathBwd.begin(), pathBwd.end());
		else
			return false;
		return true;
	}
	else
	{
		if (CheckPath(pathBwd, checkRadius, pFilter))
			m_pathPoints.insert(itCutExitAfter, pathBwd.begin(), pathBwd.end());
		else if (CheckPath(pathFwd, checkRadius, pFilter))
			m_pathPoints.insert(itCutExitAfter, pathFwd.begin(), pathFwd.end());
		else
			return false;
		return true;
	}
}

//===================================================================
// AdjustPathAroundObstacle
//===================================================================
bool CNavPath::AdjustPathAroundObstacle(const CPathObstacle& obstacle, IAISystem::tNavCapMask navCapMask, const INavMeshQueryFilter* pFilter)
{
	switch (obstacle.GetType())
	{
	case CPathObstacle::ePOT_Shape2D:
		return AdjustPathAroundObstacleShape2D(obstacle.GetShape2D(), navCapMask, pFilter);
	default:
		AIError("CNavPath::AdjustPathAroundObstacle unhandled type %d", obstacle.GetType());
	}
	return false;
}

//===================================================================
// AdjustPathAroundObstacles
//===================================================================
bool CNavPath::AdjustPathAroundObstacles(const CPathObstacles& obstacles, IAISystem::tNavCapMask navCapMask, const INavMeshQueryFilter* pFilter)
{
	MovePathEndsOutOfObstacles(obstacles, pFilter);

	for (TPathObstacles::const_iterator it = obstacles.GetCombinedObstacles().begin(); it != obstacles.GetCombinedObstacles().end(); ++it)
	{
		const CPathObstacle& obstacle = **it;
		if (!AdjustPathAroundObstacle(obstacle, navCapMask, pFilter))
			return false;
	}
	return true;
}

bool CNavPath::AdjustPathAroundObstacles(const Vec3& currentpos, const AgentMovementAbility& movementAbility, const INavMeshQueryFilter* pFilter)
{
	m_obstacles.CalculateObstaclesAroundLocation(currentpos, movementAbility, this);

	MovePathEndsOutOfObstacles(m_obstacles, pFilter);

	for (TPathObstacles::const_iterator it = m_obstacles.GetCombinedObstacles().begin(); it != m_obstacles.GetCombinedObstacles().end(); ++it)
	{
		const CPathObstacle& obstacle = **it;
		if (!AdjustPathAroundObstacle(obstacle, movementAbility.pathfindingProperties.navCapMask, pFilter))
			return false;
	}
	return true;
}

//===================================================================
// GetMaxDistanceFromStart
//===================================================================
float CNavPath::GetMaxDistanceFromStart() const
{
	if (m_pathPoints.size() < 2)
		return .0f;

	float maxDistanceSq = .0f;
	TPathPoints::const_iterator start = m_pathPoints.begin();
	const Vec3& startPathPointPosition = start->vPos;
	TPathPoints::const_iterator it = ++m_pathPoints.begin();
	TPathPoints::const_iterator end = m_pathPoints.end();
	for (; it != end; ++it)
	{
		const PathPointDescriptor& ppd = *it;
		const float distanceSq = Distance::Point_PointSq(startPathPointPosition, ppd.vPos);
		maxDistanceSq = static_cast<float>(__fsel(maxDistanceSq - distanceSq, maxDistanceSq, distanceSq));
	}

	return sqrt_tpl(maxDistanceSq);
}

//===================================================================
// GetAABB
//===================================================================
AABB CNavPath::GetAABB(float dist) const
{
	float distLeft = dist;

	AABB result(AABB::RESET);

	if (m_pathPoints.size() < 2)
		return result;

	Vec3 thisPt;
	if (!GetPosAlongPath(thisPt))
		return result;

	result.Add(thisPt);

	// remaining path
	for (TPathPoints::const_iterator it = ++m_pathPoints.begin(); it != m_pathPoints.end(); ++it)
	{
		const PathPointDescriptor& ppd = *it;

		Vec3 delta = ppd.vPos - thisPt;
		float deltaLen = delta.NormalizeSafe();
		if (deltaLen > distLeft)
		{
			result.Add(thisPt + distLeft * delta);
			return result;
		}
		thisPt = ppd.vPos;
		result.Add(thisPt);
		distLeft -= deltaLen;
	}
	return result;
}

//===================================================================
// GetDistToPath
//===================================================================
float CNavPath::GetDistToPath(Vec3& pathPosOut, float& distAlongPathOut, const Vec3& pos, const float dist, const bool twoD) const
{
	float distOut = std::numeric_limits<float>::max();

	float traversedDist = 0.0f;
	const TPathPoints::const_iterator pathEndIt = m_pathPoints.end();
	for (TPathPoints::const_iterator pathIt = m_pathPoints.begin(); pathIt != pathEndIt; ++pathIt)
	{
		TPathPoints::const_iterator pathNextIt = pathIt;
		++pathNextIt;
		if (pathNextIt == pathEndIt)
			break;

		Lineseg seg(pathIt->vPos, pathNextIt->vPos);

		float segLen = twoD ? Distance::Point_Point2D(seg.start, seg.end) : Distance::Point_Point(seg.start, seg.end);
		if (segLen < 0.001f)
			continue;
		if (segLen + traversedDist > dist)
		{
			float frac = (dist - traversedDist) / (segLen + traversedDist);
			seg.end = frac * seg.end + (1.0f - frac) * seg.start;
		}

		float t;
		float thisDist = twoD ? Distance::Point_Lineseg2D(pos, seg, t) : Distance::Point_Lineseg(pos, seg, t);
		if (thisDist < distOut)
		{
			distOut = thisDist;
			pathPosOut = seg.GetPoint(t);
			distAlongPathOut = traversedDist + t * segLen;
		}

		traversedDist += segLen;
		if (traversedDist > dist)
			break;
	}
	return distOut < std::numeric_limits<float>::max() ? distOut : -1.0f;
}

void CNavPath::GetDirectionToPathFromPoint(const Vec3& point, Vec3& dir) const
{
	if (m_pathPoints.empty())
	{
		dir.zero();
	}
	else if (m_pathPoints.size() == 1)
	{
		dir = m_pathPoints.front().vPos - point;
	}
	else
	{
		const Vec3 currentPositionOnPath = GetPrevPathPoint()->vPos * (1.0f - m_currentFrac) + GetNextPathPoint()->vPos * m_currentFrac;
		dir = currentPositionOnPath - point;
	}
}

//===================================================================
// GetDistToSmartObject
//===================================================================
float CNavPath::GetDistToSmartObject(bool b2D) const
{
	if (m_pathPoints.size() < 2)
		return std::numeric_limits<float>::max();

	TPathPoints::const_iterator pathIt = m_pathPoints.begin();
	IAISystem::ENavigationType typeCur = pathIt->navType;
	++pathIt;
	IAISystem::ENavigationType typeNext = pathIt->navType;

	if (typeCur == IAISystem::NAV_SMARTOBJECT && typeNext == IAISystem::NAV_SMARTOBJECT)
		return 0.0f;

	Vec3 curPos;
	GetPosAlongPath(curPos);

	float dist = 0.0f;
	const TPathPoints::const_iterator pathEndIt = m_pathPoints.end();
	for (; pathIt != pathEndIt; ++pathIt)
	{
		Vec3 thisPos = pathIt->vPos;
		dist += b2D ? Distance::Point_Point2D(curPos, pathIt->vPos) : Distance::Point_Point(curPos, pathIt->vPos);

		TPathPoints::const_iterator pathItNext = pathIt;
		++pathItNext;
		if (pathItNext != pathEndIt)
		{
			if (pathIt->navType == IAISystem::NAV_SMARTOBJECT && pathItNext->navType == IAISystem::NAV_SMARTOBJECT)
				return dist;
		}
		else
		{
			// End of the path
			if (pathIt->navType == IAISystem::NAV_SMARTOBJECT)
				return dist;
		}

		curPos = pathIt->vPos;
	}
	return std::numeric_limits<float>::max();
}

bool CNavPath::CanPassFilter(size_t fromPointIndex, const INavMeshQueryFilter* pFilter)
{
	if (!pFilter)
		return true;
	
	const TPathPoints& pathPoints = GetPath();
	if (fromPointIndex >= pathPoints.size())
		return true;

	const MNM::INavMesh* pNavMesh = gAIEnv.pNavigationSystem->GetMNMNavMesh(m_params.meshID);
	for (auto it = std::next(pathPoints.begin(), fromPointIndex); it != pathPoints.end(); ++it)
	{
		if (!pNavMesh->CanTrianglePassFilter(it->iTriId, *pFilter))
		{
			return false;
		}
	}
	return true;
}

const PathPointDescriptor::OffMeshLinkData* CNavPath::GetLastPathPointMNNSOData() const
{
	if (!m_pathPoints.empty())
	{
		const PathPointDescriptor& lastPoint = m_pathPoints.back();
		if (lastPoint.navType == IAISystem::NAV_SMARTOBJECT)
		{
			if (lastPoint.navSOMethod == nSOmSignalAnimation || lastPoint.navSOMethod == nSOmActionAnimation)
			{
				return &lastPoint.offMeshLinkData;
			}
		}
	}

	return NULL;
}

//===================================================================
// Serialize
//===================================================================
void CNavPath::SVersion::Serialize(TSerialize ser)
{
	ser.Value("version", v);
}
