// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PathMarker.h

   Description: CPathMarker contains the history of a given moving point (i.e. an entity position)
   and can return a previous position (interpolated) given the distance from the current point
   computed along the path

   (MATT) Used only by CFormation {2009/06/04}

   -------------------------------------------------------------------------
   History:
   - 17:11:2004   14:23 : Created by Luciano Morpurgo

 *********************************************************************/

#ifndef __PathMarker_H__
#define __PathMarker_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAgent.h>
#include <vector>

struct CSteeringDebugInfo
{
	std::vector<Vec3> pts;
	Vec3              segStart, segEnd;
};

struct pathStep_t
{
	Vec3  vPoint;
	float fDistance;
	bool  bPassed;

	pathStep_t() :
		vPoint(ZERO),
		fDistance(0.f),
		bPassed(false)
	{
	}
};

class CPathMarker
{
private:
	std::vector<pathStep_t> m_cBuffer; //using vector instead of deque as a FIFO, optimizing memory allocation and speed
	float                   m_fStep;
	int                     m_iCurrentPoint;
	int                     m_iSize;
	int                     m_iUsed;
	float                   m_fTotalDistanceRun;
public:
	CPathMarker(float fMaxDistanceNeeded, float fStep);
	void Init(const Vec3& vInitPoint, const Vec3& vEndPoint);
	Vec3 GetPointAtDistance(const Vec3& vNewPoint, float fDistance) const;
	Vec3 GetPointAtDistanceFromNewestPoint(float fDistanceFromNewestPoint) const;   // returns a point that is at most fDistanceFromNewestPoint meters away from the head (latest) point (clamped to [head, tail] point)
	Vec3 GetDirectionAtDistance(const Vec3& vTargetPoint, float fDesiredDistance) const;
	Vec3 GetDirectionAtDistanceFromNewestPoint(float fDistanceFromNewestPoint) const;
	Vec3 GetMoveDirectionAtDistance(Vec3& vTargetPoint, float fDesiredDistance,
	                                const Vec3& vUserPos, float falloff,
	                                float* alignmentWithPath,
	                                CSteeringDebugInfo* debugInfo);
	float        GetDistanceToPoint(Vec3& vTargetPoint, Vec3& vMyPoint);
	inline float GetTotalDistanceRun() { return m_fTotalDistanceRun; }
	void         Update(const Vec3& vNewPoint, bool b2D = false);
	size_t       GetPointCount() const { return m_cBuffer.size(); }
	void         DebugDraw();
	void         Serialize(TSerialize ser);

};

#endif // __PathMarker_H__
