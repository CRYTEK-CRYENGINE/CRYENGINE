// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIRadialOcclusion.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 2 Mar 2009			 : Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/
#ifndef _AIRADIALOCCLUSION_H_
#define _AIRADIALOCCLUSION_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMath/Cry_Vector3.h>
#include <math.h>
#include <vector>

//====================================================================
// CAIRadialOcclusion
// Radial occlusion buffer.
//====================================================================
class CAIRadialOcclusion
{
public:
	CAIRadialOcclusion() : m_center(0, 0, 0)
	{
		for (unsigned i = 0; i < SAMPLE_COUNT; ++i)
			m_buffer[i] = 0;
	}

	// Reset the buffer to specified center and range
	void Reset(const Vec3& c, float r);

	// Rasterize circle like object into the buffer.
	void RasterizeCircle(const Vec3& pos, float r);

	// Rasterize linear object into the buffer.
	void RasterizeEdge(const Vec3& start, const Vec3& end, bool cull);

	// Returns true if the specified point is visible.
	inline bool IsVisible(const Vec3& pos) const
	{
		Vec3 v = pos - m_center;
		float a = atan2f(v.y, v.x);
		int ia = ((int)floor(a / gf_PI2 * SAMPLE_COUNT)) & SAMPLE_MASK;
		return v.GetLengthSquared2D() < sqr(m_buffer[ia]);
	}

	// Returns sample index from a position
	inline int GetIndex(const Vec3& pos) const
	{
		Vec3 v = pos - m_center;
		float a = atan2f(v.y, v.x);
		return ((int)floor(a / gf_PI2 * SAMPLE_COUNT)) & SAMPLE_MASK;
	}

	// Returns the center of the occlusion buffer.
	inline const Vec3& GetCenter() const           { return m_center; }
	// Returns an occlusion buffer sample.
	inline float       GetSample(unsigned i) const { return m_buffer[i]; }

	void               operator=(const CAIRadialOcclusion& rhs);

	static const unsigned SAMPLE_COUNT = (1 << 8);
	static const unsigned SAMPLE_MASK = SAMPLE_COUNT - 1;

private:
	Vec3  m_center;
	float m_buffer[SAMPLE_COUNT];
};

//====================================================================
// CAIRadialOccypancy
//====================================================================
class CAIRadialOccypancy
{
public:
	CAIRadialOccypancy();
	~CAIRadialOccypancy();

	void               Reset(const Vec3& center, const Vec3& forward, float radius);

	Vec3               GetNearestUnoccupiedDirection(const Vec3& dir, float& bias);

	inline const Vec3& GetCenter() const { return m_center; }
	inline const Vec3& GetAxisX() const  { return m_axisx; }
	inline const Vec3& GetAxisY() const  { return m_axisy; }

	void               AddObstructionCircle(const Vec3& pos, float radius);
	void               AddObstructionLine(const Vec3& p0, const Vec3& p1);
	void               AddObstructionDirection(const Vec3& dir);

	struct SSpan
	{
		SSpan(float smin, float smax) : smin(smin), smax(smax) {}
		float smin, smax;
	};
	inline unsigned     GetSpanCount() const      { return m_spans.size(); }
	inline const SSpan& GetSpan(unsigned i) const { return m_spans[i]; }

	void                DebugDraw(const Vec3& center, ColorB color);

private:

	void AddAndMergeSegment(const Vec3& p0, const Vec3& p1);
	void AddAndMergeSpan(float a, float b);
	bool IntersectSegmentCircle(const Vec3& p0, const Vec3& p1, const Vec3& center, float rad, float& t0, float& t1);

	std::vector<SSpan> m_spans;
	Vec3               m_center;
	Vec3               m_axisy, m_axisx;
	float              m_radius;
};

//====================================================================
// CAIRadialOcclusionRaycast
//====================================================================
class CAIRadialOcclusionRaycast
{
public:
	CAIRadialOcclusionRaycast(float rangeDepth, float rangeHeight, float spread, unsigned width, unsigned height);
	~CAIRadialOcclusionRaycast();

	void        Reset();

	void        Update(const Vec3& center, const Vec3& target, float unitHeight, bool flat, unsigned raysPerUpdate);

	const Vec3& GetCenter() const { return m_center; }
	const Vec3& GetTarget() const { return m_target; }

	inline bool IsVisible(const Vec3& pos) const
	{
		if (pos.z < m_heightMin || pos.z > m_heightMax)
			return false;
		Vec3 v = pos - m_center;
		float lx = m_basis.GetColumn0().Dot(v);
		float ly = m_basis.GetColumn1().Dot(v);
		float a = atan2f(lx, ly) + m_spread / 2;
		if (a < 0.0f || a > m_spread)
			return false;
		unsigned i = (unsigned)floorf(a / m_spread * m_width);
		if (i > m_width - 1) i = m_width - 1;
		float dist = sqrtf(lx * lx + ly * ly);
		return dist < m_combined[i];
	}

	void        DebugDraw(bool drawFilled = true);

	static void UpdateActiveCount();

private:

	struct SSample
	{
		float dist;
		float height;
		bool  pending;

	};

	CTimeValue           m_lastUpdatedTime;
	Vec3                 m_center;
	Vec3                 m_target;
	Matrix33             m_basis;
	float                m_heightMin, m_heightMax;
	std::vector<float>   m_combined;
	std::vector<SSample> m_samples;
	unsigned             m_cx, m_cy;
	bool                 m_firstUpdate;
	const float          m_depthRange;
	const float          m_heightRange;
	const float          m_spread;
	const unsigned       m_width, m_height;
	/*	INT_PTR m_id;
	   static INT_PTR m_idGen;*/

	int m_id;

	struct SSampleRayQuery
	{
		SSampleRayQuery() : id(0), i(0), pRaycaster(0) {}
		unsigned                   id;
		unsigned                   i;
		CAIRadialOcclusionRaycast* pRaycaster;
	};

	/*	static std::map<INT_PTR, SSampleRayQuery> m_queries;*/

	static const int                               MAX_QUERIES = 100;
	static SSampleRayQuery                         m_queries[MAX_QUERIES];
	static bool                                    m_physListenerInit;
	static std::vector<CAIRadialOcclusionRaycast*> m_physListeners;
	static int m_idGen;

	static int m_activeCount;

	static SSampleRayQuery* GetQuery(int idx, CAIRadialOcclusionRaycast* pCaller)
	{
		for (unsigned i = 0; i < MAX_QUERIES; ++i)
		{
			if (m_queries[i].id == 0)
			{
				m_queries[i].id = pCaller->m_id;
				m_queries[i].i = idx;
				m_queries[i].pRaycaster = pCaller;
				return &m_queries[i];
			}
		}
		return 0;
	}

	static void InitPhysCallback(CAIRadialOcclusionRaycast* occ);
	static void RemovePhysCallback(CAIRadialOcclusionRaycast* occ);
	static int  OnRwiResult(const EventPhys* pEvent);
};

#endif
