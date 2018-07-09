// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIRadialOcclusion.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Mikko Mononen
   - 2 Mar 2009	: Evgeny Adamenkov: Replaced IRenderer with CDebugDrawContext

 *********************************************************************/

#include "StdAfx.h"
#include "AIRadialOcclusion.h"
#include <CryMath/Cry_Vector3.h>
#include "DebugDrawContext.h"
#include <math.h>

// Comment this out to do immediate raycasts.
#define USE_ASYNC_RAYCASTS 1

//====================================================================
// CAIRadialOcclusion
//====================================================================
void CAIRadialOcclusion::Reset(const Vec3& c, float r)
{
	m_center = c;
	for (unsigned i = 0; i < SAMPLE_COUNT; ++i)
		m_buffer[i] = r;
}

//====================================================================
// RasterizeCircle
// Rasterize circle like object into the buffer.
//====================================================================
void CAIRadialOcclusion::RasterizeCircle(const Vec3& pos, float r)
{
	Vec3 dir = pos - m_center;
	dir.z = 0;

	// Rasterize the span
	float a = atan2f(dir.y, dir.x);
	float d = dir.len();
	float w = r / d;
	float a0 = a - w;
	float a1 = a + w;

	// Move the cover a bit closer.
	d -= r / 2;

	int ia0 = (int)ceilf(a0 / gf_PI2 * SAMPLE_COUNT);
	int ia1 = (int)floorf(a1 / gf_PI2 * SAMPLE_COUNT);

	if (ia1 >= ia0)
	{
		for (int i = ia0; i < ia1; ++i)
		{
			int x = i & SAMPLE_MASK;
			if (d < m_buffer[x])
				m_buffer[x] = d;
		}
	}
}

//====================================================================
// RasterizeEdge
// Rasterize linear object into the buffer.
//====================================================================
void CAIRadialOcclusion::RasterizeEdge(const Vec3& start, const Vec3& end, bool cull)
{
	Vec3 v0(start - m_center);
	v0.z = 0;
	Vec3 v1(end - m_center);
	v1.z = 0;

	Vec3 edge(v1 - v0);
	if (cull)
	{
		Vec3 edgen(edge.y, -edge.x, 0);
		if (edgen.Dot(v0) < 0.0f)
			return;
	}

	// Rasterize the span
	float d0 = v0.len();
	float d1 = v1.len();
	float a0 = atan2f(v0.y, v0.x);
	float a1 = atan2f(v1.y, v1.x);

	if (a1 < a0)
		a1 += gf_PI2;

	int ia0 = (int)floorf(a0 / gf_PI2 * SAMPLE_COUNT);
	int ia1 = (int)floorf(a1 / gf_PI2 * SAMPLE_COUNT);

	int n = (ia1 - ia0);
	if (n >= 0)
	{
		float a = (float)(ia0 + 0.5f) / (float)SAMPLE_COUNT * gf_PI2;
		float da = (float)1.0f / (float)SAMPLE_COUNT * gf_PI2;

		if (fabsf(edge.x) > fabsf(edge.y))
		{
			// y = mx + c
			// sin(a)*r = m*cos(a)*r + c
			// r = c / (sin(a) - m_cos(a))
			float m = (v1.y - v0.y) / (v1.x - v0.x);
			float c = v0.y - m * v0.x;

			for (int i = ia0; i <= ia1; ++i)
			{
				int x = i & SAMPLE_MASK;
				float aa = clamp_tpl(a, a0, a1);
				float d = c / (sinf(aa) - m * cosf(aa));
				if (d > 0 && d < m_buffer[x])
					m_buffer[x] = d;
				a += da;
			}
		}
		else
		{
			// x = my + c
			// sin(a)*r = m*cos(a)*r + c
			// r = c / (sin(a) - m_cos(a))
			float m = (v1.x - v0.x) / (v1.y - v0.y);
			float c = v0.x - m * v0.y;

			for (int i = ia0; i <= ia1; ++i)
			{
				int x = i & SAMPLE_MASK;
				float aa = clamp_tpl(a, a0, a1);
				float d = c / (cosf(aa) - m * sinf(aa));
				if (d > 0 && d < m_buffer[x])
					m_buffer[x] = d;
				a += da;
			}
		}
	}
}

//====================================================================
// operator=
//====================================================================
void CAIRadialOcclusion::operator=(const CAIRadialOcclusion& rhs)
{
	for (unsigned i = 0; i < SAMPLE_COUNT; ++i)
		m_buffer[i] = rhs.m_buffer[i];
	m_center = rhs.m_center;
}

//====================================================================
// CAIRadialOcclusionRaycast
//====================================================================
CAIRadialOccypancy::CAIRadialOccypancy() :
	m_center(0, 0, 0),
	m_axisy(0, 1, 0), m_axisx(1, 0, 0),
	m_radius(1.0f)
{
}

CAIRadialOccypancy::~CAIRadialOccypancy()
{
}

void CAIRadialOccypancy::Reset(const Vec3& center, const Vec3& forward, float radius)
{
	m_spans.clear();
	m_center = center;
	m_axisx = forward;
	m_axisx.z = 0;
	m_axisx.Normalize();
	m_axisy.Set(m_axisx.y, -m_axisx.x, 0.0f);
	m_radius = radius;
}

Vec3 CAIRadialOccypancy::GetNearestUnoccupiedDirection(const Vec3& dir, float& bias)
{
	float x = m_axisx.Dot(dir);
	float y = m_axisy.Dot(dir);
	float a = atan2f(y, x);

	for (unsigned i = 0, ni = m_spans.size(); i < ni; ++i)
	{
		if (a >= m_spans[i].smin && a <= m_spans[i].smax)
		{
			float w0 = a - m_spans[i].smin;
			float w1 = m_spans[i].smax - a;
			if (bias < -0.1f)
				w0 *= 0.4f;
			else if (bias > 0.1f)
				w1 *= 0.4f;
			else
				w1 *= 0.75f;

			if (w0 < w1)
			{
				a = m_spans[i].smin;
				bias = -1.0f;
			}
			else
			{
				a = m_spans[i].smax;
				bias = 1.0f;
			}

			return m_axisx * cosf(a) + m_axisy * sinf(a);
		}
	}

	bias = 0.0f;

	return dir;
}

void CAIRadialOccypancy::AddObstructionCircle(const Vec3& pos, float radius)
{
	Vec3 diff = pos - m_center;
	float x = m_axisx.Dot(diff);
	float y = m_axisy.Dot(diff);

	float dist = sqrtf(sqr(x) + sqr(y));

	if (dist > (m_radius + radius))
	{
		// The circles do not touch at all.
		return;
	}
	else if (dist < radius)
	{
		// The circle contains the center, add half circle to the buffer.
		float a = atan2f(y, x);
		AddAndMergeSpan(a - gf_PI / 2, a + gf_PI / 2);
	}
	else
	{
		// The circle is
		float s = clamp_tpl(radius / dist, -1.0f, 1.0f);
		float ra = asinf(s);
		float a = atan2f(y, x);

		if (dist > m_radius)
		{
			float ss = (dist - m_radius) / radius;
			ss = ss * ss * ss;
			ra *= 1 - ss;
		}

		AddAndMergeSpan(a - ra, a + ra);
	}
}

void CAIRadialOccypancy::AddObstructionLine(const Vec3& p0, const Vec3& p1)
{
	float t0, t1;
	if (!IntersectSegmentCircle(p0, p1, m_center, m_radius, t0, t1))
		return;
	if ((t0 < 0.0f && t1 < 0.0f) || (t0 > 1.0f && t1 > 1.0f))
		return;
	if (t0 < 0) t0 = 0;
	if (t1 > 1) t1 = 1;
	Vec3 dir = p1 - p0;
	AddAndMergeSegment(p0 + dir * t0, p0 + dir * t1);
}

void CAIRadialOccypancy::AddObstructionDirection(const Vec3& dir)
{
	float x = m_axisx.Dot(dir);
	float y = m_axisy.Dot(dir);
	float a = atan2f(y, x);
	AddAndMergeSpan(a - gf_PI / 2, a + gf_PI / 2);
}

void CAIRadialOccypancy::AddAndMergeSegment(const Vec3& p0, const Vec3& p1)
{
	Vec3 diff;
	float x, y;

	diff = p0 - m_center;
	x = m_axisx.Dot(diff);
	y = m_axisy.Dot(diff);
	float a = atan2f(y, x);

	diff = p1 - m_center;
	x = m_axisx.Dot(diff);
	y = m_axisy.Dot(diff);
	float b = atan2f(y, x);

	float range = b - a;
	if (range > gf_PI)
		range -= gf_PI2;
	if (range < -gf_PI)
		range += gf_PI2;
	b = a + range;

	if (a > b)
		std::swap(a, b);

	AddAndMergeSpan(a, b);
}

inline bool OverlapSegment(float min0, float max0, float min1, float max1, float eps)
{
	if (min0 - eps >= max1) return false;
	if (max0 + eps <= min1) return false;
	return true;
}

void CAIRadialOccypancy::AddAndMergeSpan(float a, float b)
{
	float range = b - a;
	a = fmodf(a, gf_PI2);
	if (a < -gf_PI)
		a += gf_PI2;

	float smin[2];
	float smax[2];

	unsigned ns = 1;
	smin[0] = a;
	smax[0] = a + range;
	if (smax[0] > gf_PI)
	{
		smin[1] = -gf_PI;
		smax[1] = smax[0] - gf_PI2;
		smax[0] = gf_PI;
		++ns;
	}

	for (unsigned k = 0; k < ns; ++k)
	{
		for (unsigned i = 0; i < m_spans.size(); )
		{
			if (OverlapSegment(smin[k], smax[k], m_spans[i].smin, m_spans[i].smax, 0.0001f))
			{
				smin[k] = min(smin[k], m_spans[i].smin);
				smax[k] = max(smax[k], m_spans[i].smax);
				unsigned n = m_spans.size();
				if (i + 1 < n)
				{
					for (unsigned j = i; j < n - 1; ++j)
						m_spans[j] = m_spans[j + 1];
				}
				m_spans.pop_back();
			}
			else
				++i;
		}

		unsigned mergedIdx = m_spans.size();
		for (unsigned i = 0; i < m_spans.size(); ++i)
		{
			if (m_spans[i].smin > smax[k])
			{
				mergedIdx = i;
				break;
			}
		}

		m_spans.insert(m_spans.begin() + mergedIdx, SSpan(smin[k], smax[k]));
	}
}

bool CAIRadialOccypancy::IntersectSegmentCircle(const Vec3& p0, const Vec3& p1, const Vec3& center, float rad, float& t0, float& t1)
{
	float dx = p1.x - p0.x;
	float dy = p1.y - p0.y;

	float a = sqr(dx) + sqr(dy);
	float pdx = p0.x - center.x;
	float pdy = p0.y - center.y;
	float b = (dx * pdx + dy * pdy) * 2.0f;
	float c = sqr(pdx) + sqr(pdy) - sqr(rad);
	float desc = (b * b) - (4 * a * c);

	if (desc >= 0.0f)
	{
		desc = sqrtf(desc);
		t0 = (-b - desc) / (2.0f * a);
		t1 = (-b + desc) / (2.0f * a);
		return true;
	}
	return false;
}

void CAIRadialOccypancy::DebugDraw(const Vec3& center, ColorB color)
{
	const unsigned maxpts = 40;
	Vec3 pts[maxpts + 1];

	CDebugDrawContext dc;

	ColorB colorTrans(color);
	colorTrans.a /= 4;

	for (unsigned i = 0, ni = m_spans.size(); i < ni; ++i)
	{
		const SSpan& span = m_spans[i];

		unsigned npts = (unsigned)ceilf((span.smax - span.smin) / gf_PI2 * (float)maxpts);
		if (npts < 2) npts = 2;
		if (npts > maxpts) npts = maxpts;

		for (unsigned j = 0; j < npts; ++j)
		{
			const float u = (float)j / (float)(npts - 1);
			const float a = span.smin + (span.smax - span.smin) * u;
			pts[j] = m_axisx * (cosf(a) * m_radius) + m_axisy * (sinf(a) * m_radius) + center;
		}
		pts[npts] = center;
		npts++;

		//		for (unsigned j = 0; j < npts-1; ++j)
		//			dc->DrawTriangle(center, colorTrans, pts[j+1], colorTrans, pts[j], colorTrans);

		dc->DrawPolyline(pts, npts, true, color, 1.0f);
	}

	//	dc->DrawLine(center, ColorB(255,   0, 0), center + m_axisx * m_radius, ColorB(255,   0, 0), 3.0f);
	//	dc->DrawLine(center, ColorB(  0, 255, 0), center + m_axisy * m_radius, ColorB(  0, 255, 0), 3.0f);
}

//====================================================================
// CAIRadialOcclusionRaycast
//====================================================================
bool CAIRadialOcclusionRaycast::m_physListenerInit = false;
std::vector<CAIRadialOcclusionRaycast*> CAIRadialOcclusionRaycast::m_physListeners;

//====================================================================
// InitPhysCallback
//====================================================================
void CAIRadialOcclusionRaycast::InitPhysCallback(CAIRadialOcclusionRaycast* occ)
{
	if (!m_physListenerInit)
	{
		gEnv->pPhysicalWorld->AddEventClient(EventPhysRWIResult::id, OnRwiResult, 1);
		for (unsigned i = 0; i < MAX_QUERIES; ++i)
		{
			m_queries[i].id = 0;
			m_queries[i].i = 0;
			m_queries[i].pRaycaster = 0;
		}
		m_physListenerInit = true;
	}
	std::vector<CAIRadialOcclusionRaycast*>::iterator it = std::find(m_physListeners.begin(), m_physListeners.end(), occ);
	if (it == m_physListeners.end())
		m_physListeners.push_back(occ);
}

//====================================================================
// RemovePhysCallback
//====================================================================
void CAIRadialOcclusionRaycast::RemovePhysCallback(CAIRadialOcclusionRaycast* occ)
{
	if (!occ) return;
	for (unsigned i = 0; i < MAX_QUERIES; ++i)
	{
		if (m_queries[i].id == occ->m_id)
		{
			m_queries[i].id = 0;
			m_queries[i].i = 0;
			m_queries[i].pRaycaster = 0;
		}
	}
	std::vector<CAIRadialOcclusionRaycast*>::iterator it = std::find(m_physListeners.begin(), m_physListeners.end(), occ);
	if (it != m_physListeners.end())
		m_physListeners.erase(it);
}

//====================================================================
// UpdateActiveCount
//====================================================================
void CAIRadialOcclusionRaycast::UpdateActiveCount()
{
	m_activeCount = 0;
	CTimeValue curTime = GetAISystem()->GetFrameStartTime();
	for (unsigned i = 0, ni = m_physListeners.size(); i < ni; ++i)
	{
		float dt = (curTime - m_physListeners[i]->m_lastUpdatedTime).GetSeconds();
		if (dt < 0.3f)
			m_activeCount++;
	}
}

#define PHYS_FOREIGN_ID_RADIAL_OCCLUSION PHYS_FOREIGN_ID_USER + 2

//====================================================================
// OnRwiResult
//====================================================================
int CAIRadialOcclusionRaycast::OnRwiResult(const EventPhys* pEvent)
{
	EventPhysRWIResult* pRWIResult = (EventPhysRWIResult*)pEvent;
	if (pRWIResult->iForeignData != PHYS_FOREIGN_ID_RADIAL_OCCLUSION)
		return 1;

	SSampleRayQuery* pQuery = (SSampleRayQuery*)pRWIResult->pForeignData;
	if (!pQuery->pRaycaster)
		return 1;

	SSample& sample = pQuery->pRaycaster->m_samples[pQuery->i];
	if (pRWIResult->nHits != 0)
	{
		sample.dist = pRWIResult->pHits[0].dist;
		sample.height = pRWIResult->pHits[0].pt.z;
	}

	sample.pending = false;

	pQuery->id = 0;
	pQuery->i = 0;
	pQuery->pRaycaster = 0;

	return 1;
}

int CAIRadialOcclusionRaycast::m_idGen = 1;
CAIRadialOcclusionRaycast::SSampleRayQuery CAIRadialOcclusionRaycast::m_queries[MAX_QUERIES];
int CAIRadialOcclusionRaycast::m_activeCount = 0;

//====================================================================
// CAIRadialOcclusionRaycast
//====================================================================
CAIRadialOcclusionRaycast::CAIRadialOcclusionRaycast(float depthRange, float heightRange, float spread,
                                                     unsigned width, unsigned height) :
	m_center(0, 0, 0),
	m_target(0, 0, 0),
	m_heightMin(0),
	m_heightMax(0),
	m_spread(spread),
	m_width(width),
	m_height(height),
	m_cx(0),
	m_cy(0),
	m_depthRange(depthRange),
	m_heightRange(heightRange),
	m_firstUpdate(true)
{
	m_basis.SetIdentity();
	m_combined.resize(width);
	m_samples.resize(width * height);

	for (unsigned i = 0; i < m_width * m_height; ++i)
	{
		m_samples[i].dist = 0;
		m_samples[i].height = 0;
		m_samples[i].pending = false;
	}
	for (unsigned i = 0; i < m_width; ++i)
		m_combined[i] = 0.0f;

	m_id = m_idGen++;

#ifdef USE_ASYNC_RAYCASTS
	InitPhysCallback(this);
#endif
}

//====================================================================
// ~CAIRadialOcclusionRaycast
//====================================================================
CAIRadialOcclusionRaycast::~CAIRadialOcclusionRaycast()
{
#ifdef USE_ASYNC_RAYCASTS
	RemovePhysCallback(this);
#endif
}

//====================================================================
// Reset
//====================================================================
void CAIRadialOcclusionRaycast::Reset()
{
	m_center.Set(0, 0, 0);
	m_target.Set(0, 0, 0);
	m_basis.SetIdentity();
	m_heightMin = 0.0f;
	m_heightMax = 0.0f;
	m_cx = 0;
	m_cy = 0;
	m_firstUpdate = true;
	for (unsigned i = 0; i < m_width * m_height; ++i)
	{
		m_samples[i].dist = 0;
		m_samples[i].height = 0;
		m_samples[i].pending = false;
	}
	for (unsigned i = 0; i < m_width; ++i)
		m_combined[i] = 0.0f;
}

//====================================================================
// Update
//====================================================================
void CAIRadialOcclusionRaycast::Update(const Vec3& center, const Vec3& target, float unitHeight,
                                       bool flat, unsigned raysPerUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_lastUpdatedTime = GetAISystem()->GetFrameStartTime();

	m_center = center;
	m_target = target;

	if (m_firstUpdate)
	{
		m_firstUpdate = false;
		for (unsigned i = 0; i < m_width * m_height; ++i)
		{
			m_samples[i].dist = 0;
			m_samples[i].height = m_center.z;
		}
		for (unsigned i = 0; i < m_width; ++i)
			m_combined[i] = 0.0f;
	}

	Vec3 dirToTarget = m_target - m_center;
	if (flat)
	{
		dirToTarget.z = 0;
	}
	else
	{
		float len = sqrtf(sqr(dirToTarget.x) + sqr(dirToTarget.y));
		if (fabsf(dirToTarget.z) > len * 0.25f)
			dirToTarget.z = (len * 0.25f) * (dirToTarget.z < 0.0f ? -1.0f : 1.0f);
	}
	dirToTarget.NormalizeSafe();

	m_basis.SetRotationVDir(dirToTarget);

	m_heightMin = FLT_MAX;
	m_heightMax = -FLT_MAX;

	// Combine samples into one radial buffer.
	unsigned pendingRays = 0;
	const SSample* src = &m_samples[0];
	float* dst = &m_combined[0];
	for (unsigned ix = 0; ix < m_width; ++ix)
	{
		*dst = 0.0f;
		for (unsigned i = 0; i < m_height; ++i)
		{
			*dst = max(*dst, src->dist);
			m_heightMax = max(m_heightMax, src->height);
			m_heightMin = min(m_heightMin, src->height);
			if (src->pending)
				pendingRays++;
			src++;
		}
		dst++;
	}

	// Do not allow to be too greedy.
	raysPerUpdate = clamp_tpl((unsigned)MAX_QUERIES / (unsigned)max(1, m_activeCount), (unsigned)2, raysPerUpdate);

	// Update buffer
	if (pendingRays < raysPerUpdate / 4)
	{
		for (unsigned i = 0; i < raysPerUpdate; ++i)
		{
			int idx = m_cx * m_height + m_cy;

#ifdef USE_ASYNC_RAYCASTS
			SSampleRayQuery* pQuery = GetQuery(idx, this);
			if (!pQuery)
				break;
#endif

			float offset = (m_cy & 1) * 0.6f;
			const float ay = (m_cy + 0.5f - m_height / 2) / (float)m_height;
			const float ax = (m_cx + 0.5f + offset - m_width / 2) / (float)m_width * m_spread;

			Vec3 dir;
			dir.x = sinf(ax) * m_depthRange;
			dir.y = cosf(ax) * m_depthRange;
			dir.z = m_heightRange * ay - unitHeight * ay;

			Vec3 up(0, 0, unitHeight * ay);

			dir = m_basis.TransformVector(dir);
			up = m_basis.TransformVector(up);

			SSample* pSample = &m_samples[idx];

#ifdef USE_ASYNC_RAYCASTS
			pSample->dist = m_depthRange * 2.0f;
			pSample->height = m_center.z + up.z + dir.z;
			pSample->pending = true;

			gEnv->pPhysicalWorld->RayWorldIntersection(m_center + up, dir,
			                                           ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid,
			                                           (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask) | rwi_queue,
			                                           0, 1, 0, 0, (void*)pQuery, PHYS_FOREIGN_ID_RADIAL_OCCLUSION);
#else
			ray_hit hit;
			pSample->dist = m_depthRange * 2.0f;
			pSample->height = m_center.z + up.z + dir.z;

			if (gEnv->pPhysicalWorld->RayWorldIntersection(m_center + up, dir,
			                                               ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid,
			                                               (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask),
			                                               &hit, 1))
			{
				pSample->dist = hit.dist;
				pSample->height = hit.pt.z;
			}
			pSample->pending = false;
#endif

			// Update indices
			m_cx++;
			if (m_cx >= m_width)
			{
				m_cx = 0;
				m_cy++;
				if (m_cy >= m_height)
					m_cy = 0;
			}
		}
	}

}

//====================================================================
// DebugDraw
//====================================================================
void CAIRadialOcclusionRaycast::DebugDraw(bool drawFilled)
{
	if (m_firstUpdate)
		return;

	std::vector<Vec3> points;
	points.resize(m_width * 2 + 1);
	unsigned n = 1;
	points[0] = m_center;
	points[0].z = m_heightMin;

	for (unsigned i = 0; i < m_width; ++i)
	{
		float ax = (i + 0.5f - m_width / 2) / (float)m_width * m_spread;
		float d = m_combined[i];
		Vec3 dir(sinf(ax), cosf(ax), 0.0f);
		Vec3 norm(dir.y, -dir.x, 0.0f);
		dir *= d;
		norm *= tanf(m_spread / (float)m_width * 0.5f) * d;

		if (i == 0 || fabsf(m_combined[i - 1]) > 0.1f)
		{
			Vec3 p0(m_center + m_basis.TransformVector(dir - norm));
			p0.z = m_heightMin + 0.1f;
			points[n++] = p0;
		}
		{
			Vec3 p1(m_center + m_basis.TransformVector(dir + norm));
			p1.z = m_heightMin + 0.1f;
			points[n++] = p1;
		}
	}
	if (n > 1)
	{
		if (drawFilled)
		{
			CDebugDrawContext dc;
			dc->SetAlphaBlended(true);
			dc->SetBackFaceCulling(false);

			Vec3 p0 = points[1];
			p0.z = m_heightMin;
			Vec3 p1 = points[1];
			p1.z = m_heightMax;
			for (unsigned i = 2; i < n; ++i)
			{
				Vec3 q0 = points[i];
				q0.z = m_heightMin;
				Vec3 q1 = points[i];
				q1.z = m_heightMax;
				dc->DrawTriangle(p0, ColorB(255, 0, 0, 32), p1, ColorB(255, 0, 0, 128), q1, ColorB(255, 0, 0, 128));
				dc->DrawTriangle(p0, ColorB(255, 0, 0, 32), q1, ColorB(255, 0, 0, 128), q0, ColorB(255, 0, 0, 32));
				p0 = q0;
				p1 = q1;
			}
		}

		CDebugDrawContext dc;
		dc->DrawPolyline(&points[0], n, true, ColorB(255, 0, 0), 3.0f);
	}
}
