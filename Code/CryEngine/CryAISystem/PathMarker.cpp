// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PathMarker.cpp

   Description: CPathMarker class implementation

   (MATT) Used only by CFormation {2009/06/04}

   -------------------------------------------------------------------------
   History:
   - 17:11:2004   14:19 : Created by Luciano Morpurgo

 *********************************************************************/

#include "StdAfx.h"
#include "PathMarker.h"
#include <limits>
#include "DebugDrawContext.h"

CPathMarker::CPathMarker(float fMaxDistanceNeeded, float fStep)
{
	if (fStep <= .0f)
		m_fStep = 0.1f;
	else
		m_fStep = fStep;
	m_iSize = (int)ceilf(fMaxDistanceNeeded / m_fStep) + 1;
	m_cBuffer.resize(m_iSize);
	m_iCurrentPoint = 0;
	m_fTotalDistanceRun = 0;
	m_iUsed = 0;
}

void CPathMarker::Update(const Vec3& vNewPoint, bool b2D)
{
	Vec3 vMovement = (vNewPoint - m_cBuffer[m_iCurrentPoint].vPoint);
	if (b2D)
		vMovement.z = 0;

	float fDistance = vMovement.GetLength();

	if (fDistance >= m_fStep)
	{
		++m_iCurrentPoint;
		if (m_iCurrentPoint >= m_iSize)
			m_iCurrentPoint = 0;
		m_cBuffer[m_iCurrentPoint].vPoint = vNewPoint;
		m_cBuffer[m_iCurrentPoint].fDistance = fDistance;
		m_fTotalDistanceRun += fDistance;
		if (m_iUsed < m_iSize) m_iUsed++;
	}
}

Vec3 CPathMarker::GetPointAtDistance(const Vec3& vTargetPoint, float fDesiredDistance) const
{
	if (!m_iUsed)
		return vTargetPoint;

	float fComputedDistance = Distance::Point_Point(vTargetPoint, m_cBuffer[m_iCurrentPoint].vPoint);
	Vec3 vPoint;
	int iPoint = m_iCurrentPoint;

	const Vec3* segStart(&vTargetPoint);
	const Vec3* segEnd(&m_cBuffer[m_iCurrentPoint].vPoint);
	float segLen(fComputedDistance);

	for (int i = 0; i < m_iUsed - 1; i++)
	{
		--iPoint;
		if (iPoint < 0)
			iPoint = m_iSize - 1;

		if (fComputedDistance >= fDesiredDistance)
		{
			if (segLen > 0)
			{
				float fAlpha;
				fAlpha = 1.0f - (fComputedDistance - fDesiredDistance) / segLen;
				if (fAlpha < 0) fAlpha = 0.0f;
				if (fAlpha > 1) fAlpha = 1.0f;
				return *segStart + fAlpha * (*segEnd - *segStart);
			}
		}

		segStart = segEnd;
		segEnd = &m_cBuffer[iPoint].vPoint;
		segLen = m_cBuffer[iPoint].fDistance;

		fComputedDistance += segLen;
	}

	return *segEnd;
}

Vec3 CPathMarker::GetPointAtDistanceFromNewestPoint(float fDesiredDistanceFromNewestPoint) const
{
	if (!m_iUsed)
		return Vec3(0, 0, 0);

	if (m_iUsed < 2 || fDesiredDistanceFromNewestPoint <= 0.0f)
		return m_cBuffer[m_iCurrentPoint].vPoint;

	const pathStep_t* segStart = NULL;
	const pathStep_t* segEnd = NULL;

	for (int i = 0; i < m_iUsed - 1; i++)
	{
		int indexSegEnd = m_iCurrentPoint - i;
		int indexSegStart = m_iCurrentPoint - i - 1;

		if (indexSegStart < 0)
			indexSegStart += m_iUsed;
		if (indexSegEnd < 0)
			indexSegEnd += m_iUsed;

		segEnd = &m_cBuffer[indexSegEnd];
		segStart = &m_cBuffer[indexSegStart];

		const float segLen = segEnd->fDistance;

		if (fDesiredDistanceFromNewestPoint > segLen)
		{
			fDesiredDistanceFromNewestPoint -= segLen;
		}
		else
		{
			Vec3 dir = segStart->vPoint - segEnd->vPoint;
			dir.NormalizeSafe();
			return segEnd->vPoint + dir * fDesiredDistanceFromNewestPoint;
		}
	}

	return segEnd->vPoint;
}

//
//----------------------------------------------------------------------------------------------------------------
Vec3 CPathMarker::GetDirectionAtDistance(const Vec3& vTargetPoint, float fDesiredDistance) const
{
	if (!m_iUsed)
		return Vec3(0, 0, 0);

	float fComputedDistance = Distance::Point_Point(vTargetPoint, m_cBuffer[m_iCurrentPoint].vPoint);
	Vec3 vDir(0, 0, 0);
	int iPoint = m_iCurrentPoint;

	const Vec3* segStart(&vTargetPoint);
	const Vec3* segEnd(&m_cBuffer[m_iCurrentPoint].vPoint);
	float segLen(fComputedDistance);

	for (int i = 0; i < m_iUsed; i++)
	{
		if (--iPoint < 0) iPoint = m_iSize - 1;

		if (fComputedDistance >= fDesiredDistance)
			break;

		segStart = segEnd;
		segEnd = &m_cBuffer[iPoint].vPoint;
		segLen = m_cBuffer[iPoint].fDistance;

		fComputedDistance += segLen;
	}

	if (segLen > 0)
	{
		vDir = *segStart - *segEnd;
		vDir.NormalizeSafe();
	}

	return vDir;

}

//
//----------------------------------------------------------------------------------------------------------------
Vec3 CPathMarker::GetDirectionAtDistanceFromNewestPoint(float fDistanceFromNewestPoint) const
{
	if (!m_iUsed)
		return Vec3(0, 0, 0);

	const pathStep_t* segEnd = NULL;
	const pathStep_t* segStart = NULL;

	for (int i = 0; i < m_iUsed; i++)
	{
		int indexSegEnd = m_iCurrentPoint - i;
		int indexSegStart = m_iCurrentPoint - 1 - i;

		if (indexSegStart < 0)
			indexSegStart += m_iUsed;
		if (indexSegEnd < 0)
			indexSegEnd += m_iUsed;

		segEnd = &m_cBuffer[indexSegEnd];
		segStart = &m_cBuffer[indexSegStart];

		if ((fDistanceFromNewestPoint -= segStart->fDistance) <= 0.0f)
			break;
	}

	Vec3 vDir = segEnd->vPoint - segStart->vPoint;
	vDir.NormalizeSafe();

	return vDir;
}

//
//----------------------------------------------------------------------------------------------------------------
Vec3 CPathMarker::GetMoveDirectionAtDistance(Vec3& vTargetPoint, float fDesiredDistance, const Vec3& vUserPos,
                                             float falloff, float* alignmentWithPath,
                                             CSteeringDebugInfo* debugInfo)
{
	int iPoint = m_iCurrentPoint;
	float closestDist = std::numeric_limits<float>::max();
	int closestIndex = iPoint; // default to last point
	int closestPtsBack = 0;
	int ptsBack = 0;
	for (int i = 0; i < m_iUsed; i++)
	{
		// guarantee first point won't be chosen so we can use closestIndex and
		// closestIndex + 1
		if (i != 0)
		{
			Vec3 delta = m_cBuffer[iPoint].vPoint - vUserPos;
			float dist = delta.GetLength();
			if (dist < closestDist)
			{
				closestDist = dist;
				closestIndex = iPoint;
				closestPtsBack = ptsBack;
			}
		}
		if (--iPoint < 0) iPoint = m_iSize - 1;
		++ptsBack;
	}

	// The direction is based on this segment, but as if this segment is moved ahead.
	// Without a lookahead then we don't anticipte corners. If we just use a segment
	// from in front then its direction on a corner will counteract any advantage.
	// This way if the path ahead turns to the right and we're already on the right
	// of the path, parallel to the path where we are, then we won't turn.
	int nextIndex = (closestIndex + 1) % m_iSize;
	// Get the orientation of the closest line segment
	Vec3 segDir = m_cBuffer[nextIndex].vPoint - m_cBuffer[closestIndex].vPoint;
	segDir.z = 0.0f;
	segDir.NormalizeSafe();
	Vec3 segRightDir(segDir.y, -segDir.x, 0.0f);

	// path can be wonky at the start - if so then use a fallback
	if ((segDir | (vTargetPoint - vUserPos)) < 0.0f)
	{
		segDir = vTargetPoint - vUserPos;
		segDir.z = 0.0f;
		segDir.NormalizeSafe();
		if (alignmentWithPath)
			*alignmentWithPath = 0.0f;
	}

	// now lookahead
	float lookahead = falloff;
	iPoint = closestIndex;
	for (int i = 0; i < closestPtsBack; ++i)
	{
		iPoint = (iPoint + 1) % m_iSize;
		lookahead -= m_cBuffer[iPoint].fDistance;
		if (lookahead <= 0.0f)
			break;
	}

	// Work out how far to one side (the right) we are
	Vec3 delta = vUserPos - m_cBuffer[iPoint].vPoint;
	delta.z = 0.0f;
	Vec3 sideVec = delta - (delta | segDir) * segDir;
	float sideDist = sideVec | segRightDir;

	// Now blend in a steering based on this distance and the falloff
	float steerFactor = -sideDist / falloff;
	if (steerFactor > 1.0f)
		steerFactor = 1.0f;
	else if (steerFactor < -1.0f)
		steerFactor = -1.0f;

	Vec3 userDir = segDir + steerFactor * segRightDir;
	userDir.NormalizeSafe();

	if (alignmentWithPath)
		*alignmentWithPath = userDir | segDir;

	if (debugInfo)
	{
		debugInfo->segStart = m_cBuffer[iPoint].vPoint;
		debugInfo->segEnd = debugInfo->segStart + segDir;

		debugInfo->pts.resize(0);
		iPoint = m_iCurrentPoint;
		for (int i = 0; i < m_iUsed; i++)
		{
			debugInfo->pts.push_back(m_cBuffer[iPoint].vPoint);
			if (--iPoint < 0)
				iPoint = m_iSize - 1;
		}
	}

	return userDir;
}

//
//----------------------------------------------------------------------------------------------------------------
float CPathMarker::GetDistanceToPoint(Vec3& vTargetPoint, Vec3& vMyPoint)
{
	float fComputedDistance = (vTargetPoint - m_cBuffer[m_iCurrentPoint].vPoint).GetLength();
	//the closest navpoint to the given MyPoint is chosen to compute the distance along the path
	//TO DO: that's not always exact, in some cases it could be the wrong point
	float fMinDistance2 = 100000000.0f;
	int iClosestPoint = m_iCurrentPoint;
	for (int i = 0; i < m_iUsed; i++)
	{
		float fCurrentDistance = (vMyPoint - m_cBuffer[i].vPoint).len2();
		if (fCurrentDistance <= fMinDistance2)
		{
			fMinDistance2 = fCurrentDistance;
			iClosestPoint = i;
		}
	}

	int iPoint = m_iCurrentPoint;
	while (iPoint != iClosestPoint)
	{
		fComputedDistance += m_cBuffer[iPoint].fDistance;
		if (--iPoint < 0) iPoint = m_iSize - 1;
	}
	//final adjustment to distance : instead of adding the fDistance of iClosestPoint,
	// we add the distance from the previous point (iClosestPoint+1) to vMyPoint
	iPoint = (iClosestPoint + 1) % m_iSize;
	fComputedDistance += (vMyPoint - m_cBuffer[iPoint].vPoint).GetLength();
	return fComputedDistance;
}

//
//----------------------------------------------------------------------------------------------------------------
void CPathMarker::Init(const Vec3& vEndPoint, const Vec3& vInitPoint)
{
	m_cBuffer[0].vPoint = vEndPoint;
	m_cBuffer[0].fDistance = 0;
	m_cBuffer[0].bPassed = false;
	m_iCurrentPoint = 0;
	m_fTotalDistanceRun = 0;
	m_iUsed = 1;
}
//
//----------------------------------------------------------------------------------------------------------------
void CPathMarker::Serialize(TSerialize ser)
{
	ser.BeginGroup("AIPathMarker");
	{
		ser.Value("m_fStep", m_fStep);
		ser.Value("m_iCurrentPoint", m_iCurrentPoint);
		ser.Value("m_iSize", m_iSize);
		ser.Value("m_iUsed", m_iUsed);
		ser.Value("m_fTotalDistanceRun", m_fTotalDistanceRun);
		ser.BeginGroup("AIPathMarkerPoints");
		{
			if (ser.IsReading())
			{
				m_cBuffer.clear();
				m_cBuffer.resize(m_iSize);
			}
			for (int i = 0; i < m_iUsed; i++)
			{
				ser.BeginGroup("point");
				pathStep_t& point = m_cBuffer[i];
				ser.Value("pos", point.vPoint);
				ser.Value("dist", point.fDistance);
				ser.Value("passed", point.bPassed);
				ser.EndGroup();
			}
			ser.EndGroup();
		}
		ser.EndGroup();
	}
}

void CPathMarker::DebugDraw()
{
	if (m_iUsed < 2)
		return;

	int prev = m_iCurrentPoint;
	int cur = m_iCurrentPoint - 1;
	if (cur < 0) cur = m_iSize - 1;

	CDebugDrawContext dc;

	for (int i = 0; i < m_iUsed - 1; ++i)
	{
		dc->DrawLine(m_cBuffer[prev].vPoint, ColorB(255, 255, 255, 128),
		             m_cBuffer[cur].vPoint, ColorB(255, 255, 255, 128), 3.0f);
		prev = cur;
		--cur;
		if (cur < 0) cur = m_iSize - 1;
	}
}
