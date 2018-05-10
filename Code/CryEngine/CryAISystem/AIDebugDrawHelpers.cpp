// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIDebugDrawHelpers.cpp
   $Id$
   Description: Helper functions to draw some interesting debug shapes.

   -------------------------------------------------------------------------
   History:
   - 2006-9-19   : Created by Mikko Mononen

 *********************************************************************/

#include "StdAfx.h"

#include <math.h>
#include <CryRenderer/IRenderer.h>
#include <CryMath/Cry_Vector3.h>
#include "AIDebugDrawHelpers.h"

static float g_drawOffset = 0.1f;

//====================================================================
// DebugDrawCircleOutline
//====================================================================
void DebugDrawCircleOutline(IRenderer* pRend, const Vec3& pos, float rad, ColorB col)
{
	Vec3 points[20];

	for (unsigned i = 0; i < 20; i++)
	{
		float a = ((float)i / 20.0f) * gf_PI2;
		points[i] = pos + Vec3(cosf(a) * rad, sinf(a) * rad, 0);
	}
	pRend->GetIRenderAuxGeom()->DrawPolyline(points, 20, true, col);
}

//====================================================================
// DebugDrawCapsuleOutline
//====================================================================
void DebugDrawCapsuleOutline(IRenderer* pRend, const Vec3& pos0, const Vec3& pos1, float rad, ColorB col)
{
	Vec3 points[20];
	Vec3 axisy = pos1 - pos0;
	axisy.Normalize();
	Vec3 axisx(axisy.y, -axisy.x, 0);
	axisx.Normalize();

	for (unsigned i = 0; i < 10; i++)
	{
		float a = ((float)i / 9.0f) * gf_PI;
		points[i] = pos1 + axisx * cosf(a) * rad + axisy * sinf(a) * rad;
	}

	for (unsigned i = 0; i < 10; i++)
	{
		float a = gf_PI + ((float)i / 9.0f) * gf_PI;
		points[i + 10] = pos0 + axisx * cosf(a) * rad + axisy * sinf(a) * rad;
	}

	pRend->GetIRenderAuxGeom()->DrawPolyline(points, 20, true, col);
}

//====================================================================
// DebugDrawWireSphere
//====================================================================
void DebugDrawWireSphere(IRenderer* pRend, const Vec3& pos, float rad, ColorB col)
{
	const unsigned npts = 32;
	Vec3 xpoints[npts];
	Vec3 ypoints[npts];
	Vec3 zpoints[npts];

	for (unsigned i = 0; i < npts; i++)
	{
		float a = ((float)i / (float)npts) * gf_PI2;
		float rx = cosf(a) * rad;
		float ry = sinf(a) * rad;
		xpoints[i] = pos + Vec3(rx, ry, 0);
		ypoints[i] = pos + Vec3(0, rx, ry);
		zpoints[i] = pos + Vec3(ry, 0, rx);
	}
	pRend->GetIRenderAuxGeom()->DrawPolyline(xpoints, npts, true, col);
	pRend->GetIRenderAuxGeom()->DrawPolyline(ypoints, npts, true, col);
	pRend->GetIRenderAuxGeom()->DrawPolyline(zpoints, npts, true, col);
}

//====================================================================
// DebugDrawWireFOVCone
//====================================================================
void DebugDrawWireFOVCone(IRenderer* pRend, const Vec3& pos, const Vec3& dir, float rad, float fov, ColorB col)
{
	const unsigned npts = 32;
	const unsigned npts2 = 16;
	Vec3 points[npts];
	Vec3 pointsx[npts2];
	Vec3 pointsy[npts2];

	Matrix33 base;
	base.SetRotationVDir(dir);

	float coneRadius = sinf(fov) * rad;
	float coneHeight = cosf(fov) * rad;

	for (unsigned i = 0; i < npts; i++)
	{
		float a = ((float)i / (float)npts) * gf_PI2;
		float rx = cosf(a) * coneRadius;
		float ry = sinf(a) * coneRadius;
		points[i] = pos + base.TransformVector(Vec3(rx, coneHeight, ry));
	}

	for (unsigned i = 0; i < npts2; i++)
	{
		float a = -fov + ((float)i / (float)(npts2 - 1)) * (fov * 2);
		float rx = sinf(a) * rad;
		float ry = cosf(a) * rad;
		pointsx[i] = pos + base.TransformVector(Vec3(rx, ry, 0));
		pointsy[i] = pos + base.TransformVector(Vec3(0, ry, rx));
	}

	pRend->GetIRenderAuxGeom()->DrawPolyline(points, npts, true, col);
	pRend->GetIRenderAuxGeom()->DrawPolyline(pointsx, npts2, false, col);
	pRend->GetIRenderAuxGeom()->DrawPolyline(pointsy, npts2, false, col);

	pRend->GetIRenderAuxGeom()->DrawLine(points[0], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts / 4], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts / 2], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts / 2 + npts / 4], col, pos, col);
}

//====================================================================
// DebugDrawArrow
//====================================================================
void DebugDrawArrow(IRenderer* pRend, const Vec3& pos, const Vec3& length, float width, ColorB col)
{
	Vec3 points[7];
	Vec3 tris[5 * 3];

	float len = length.GetLength();
	if (len < 0.0001f)
		return;

	float headLen = width * 2.0f;
	float headWidth = width * 2.0f;

	if (headLen > len * 0.8f)
		headLen = len * 0.8f;

	Vec3 dir(length / len);
	Vec3 norm(length.y, -length.x, 0);
	norm.NormalizeSafe();

	Vec3 end(pos + length);
	Vec3 start(pos);

	unsigned n = 0;
	points[n++] = end;
	points[n++] = end - dir * headLen - norm * headWidth / 2;
	PREFAST_SUPPRESS_WARNING(6386)
	points[n++] = end - dir * headLen - norm * width / 2;
	points[n++] = end - dir * headLen + norm * width / 2;
	points[n++] = end - dir * headLen + norm * headWidth / 2;
	points[n++] = start - norm * width / 2;
	points[n++] = start + norm * width / 2;

	// cppcheck-suppress redundantAssignment
	n = 0;
	tris[n++] = points[0];
	tris[n++] = points[1];
	tris[n++] = points[2];

	tris[n++] = points[0];
	tris[n++] = points[2];
	tris[n++] = points[3];

	tris[n++] = points[0];
	tris[n++] = points[3];
	tris[n++] = points[4];

	tris[n++] = points[2];
	tris[n++] = points[5];
	tris[n++] = points[6];

	tris[n++] = points[2];
	tris[n++] = points[6];
	tris[n++] = points[3];

	pRend->GetIRenderAuxGeom()->DrawTriangles(tris, n, col);
}

//====================================================================
// DebugDrawRangeCircle
//====================================================================
void DebugDrawRangeCircle(IRenderer* pRend, const Vec3& pos, float rad, float width,
                          ColorB colFill, ColorB colOutline, bool drawOutline)
{
	const unsigned npts = 24;

	Vec3 points[npts];
	Vec3 pointsOutline[npts];
	Vec3 tris[npts * 2 * 3];

	if (width > rad) width = rad;

	for (unsigned i = 0; i < npts; i++)
	{
		float a = ((float)i / (float)npts) * gf_PI2;
		points[i] = Vec3(cosf(a), sinf(a), 0);
		pointsOutline[i] = pos + points[i] * rad;
	}

	unsigned n = 0;
	for (unsigned i = 0; i < npts; i++)
	{
		tris[n++] = pos + points[i] * (rad - width);
		tris[n++] = pos + points[i] * rad;
		tris[n++] = pos + points[(i + 1) % npts] * rad;

		tris[n++] = pos + points[i] * (rad - width);
		tris[n++] = pos + points[(i + 1) % npts] * rad;
		tris[n++] = pos + points[(i + 1) % npts] * (rad - width);
	}

	pRend->GetIRenderAuxGeom()->DrawTriangles(tris, npts * 2 * 3, colFill);
	if (drawOutline)
		pRend->GetIRenderAuxGeom()->DrawPolyline(pointsOutline, npts, true, colOutline);
}

//====================================================================
// DebugDrawRangeArc
//====================================================================
void DebugDrawRangeArc(IRenderer* pRend, const Vec3& pos, const Vec3& dir, float angle, float rad, float width,
                       ColorB colFill, ColorB colOutline, bool drawOutline)
{
	const unsigned npts = 12;

	Vec3 points[npts];
	Vec3 pointsOutline[npts];
	Vec3 tris[(npts - 1) * 2 * 3];

	Vec3 forw(dir.x, dir.y, 0.0f);
	forw.NormalizeSafe();
	Vec3 right(forw.y, -forw.x, 0);

	if (width > rad) width = rad;

	for (unsigned i = 0; i < npts; i++)
	{
		float a = ((float)i / (float)(npts - 1) - 0.5f) * angle;
		points[i] = forw * cosf(a) + right * sinf(a);
		pointsOutline[i] = pos + points[i] * rad;
	}

	unsigned n = 0;
	for (unsigned i = 0; i < npts - 1; i++)
	{
		tris[n++] = pos + points[i] * (rad - width);
		tris[n++] = pos + points[i + 1] * rad;
		tris[n++] = pos + points[i] * rad;

		tris[n++] = pos + points[i] * (rad - width);
		tris[n++] = pos + points[i + 1] * (rad - width);
		tris[n++] = pos + points[i + 1] * rad;
	}

	pRend->GetIRenderAuxGeom()->DrawTriangles(tris, n, colFill);
	if (drawOutline)
	{
		pRend->GetIRenderAuxGeom()->DrawPolyline(pointsOutline, npts, false, colOutline);
		pRend->GetIRenderAuxGeom()->DrawLine(pos + forw * (rad - width / 4), colOutline, pos + forw * (rad + width / 4), colOutline);
	}
}

//====================================================================
// DebugDrawRange
//====================================================================
void DebugDrawRangeBox(IRenderer* pRend, const Vec3& pos, const Vec3& dir, float sizex, float sizey, float width,
                       ColorB colFill, ColorB colOutline, bool drawOutline)
{
	float minX = sizex - width;
	float maxX = sizex;
	float minY = sizey - width;
	float maxY = sizey;

	if (maxX < 0.001f || maxY < 0.001f || minX > maxX || minY > maxY)
		return;

	Vec3 points[8];
	Vec3 tris[8 * 3];
	Vec3 norm(dir.y, -dir.x, dir.z);

	points[0] = pos + norm * -minX + dir * -minY;
	points[1] = pos + norm * minX + dir * -minY;
	points[2] = pos + norm * minX + dir * minY;
	points[3] = pos + norm * -minX + dir * minY;

	points[4] = pos + norm * -maxX + dir * -maxY;
	points[5] = pos + norm * maxX + dir * -maxY;
	points[6] = pos + norm * maxX + dir * maxY;
	points[7] = pos + norm * -maxX + dir * maxY;

	unsigned n = 0;

	tris[n++] = points[0];
	tris[n++] = points[5];
	tris[n++] = points[1];
	tris[n++] = points[0];
	tris[n++] = points[4];
	tris[n++] = points[5];

	tris[n++] = points[1];
	tris[n++] = points[6];
	tris[n++] = points[2];
	tris[n++] = points[1];
	tris[n++] = points[5];
	tris[n++] = points[6];

	tris[n++] = points[2];
	tris[n++] = points[7];
	tris[n++] = points[3];
	tris[n++] = points[2];
	tris[n++] = points[6];
	tris[n++] = points[7];

	tris[n++] = points[3];
	tris[n++] = points[4];
	tris[n++] = points[0];
	tris[n++] = points[3];
	tris[n++] = points[7];
	tris[n++] = points[4];

	pRend->GetIRenderAuxGeom()->DrawTriangles(tris, 8 * 3, colFill);
	if (drawOutline)
		pRend->GetIRenderAuxGeom()->DrawPolyline(&points[4], 4, true, colOutline);
}

//====================================================================
// DebugDrawRangePolygon
//====================================================================
template<typename containerType>
void DebugDrawRangePolygon(IRenderer* pRend, const containerType& polygon, float width,
                           ColorB colFill, ColorB colOutline, bool drawOutline)
{
	static std::vector<Vec3> verts;
	static std::vector<vtx_idx> tris;
	static std::vector<Vec3> outline;

	if (polygon.size() < 3) return;

	Vec3 prevDir(polygon.front() - polygon.back());
	prevDir.NormalizeSafe();
	Vec3 prevNorm(-prevDir.y, prevDir.x, 0.0f);
	prevNorm.NormalizeSafe();
	Vec3 prevPos(polygon.back());

	verts.clear();
	outline.clear();

	typename containerType::const_iterator li, linext;
	typename containerType::const_iterator liend = polygon.end();
	for (li = polygon.begin(); li != liend; ++li)
	{
		linext = li;
		++linext;
		if (linext == liend)
			linext = polygon.begin();

		const Vec3& curPos(*li);
		const Vec3& nextPos(*linext);
		Vec3 dir(nextPos - curPos);
		Vec3 norm(-dir.y, dir.x, 0.0f);
		norm.NormalizeSafe();

		Vec3 mid((prevNorm + norm) * 0.5f);
		float dmr2 = sqr(mid.x) + sqr(mid.y);
		if (dmr2 > 0.00001f)
			mid *= 1.0f / dmr2;

		float cross = prevDir.x * dir.y - dir.x * prevDir.y;

		outline.push_back(curPos);

		if (cross < 0.0f)
		{
			if (dmr2 * sqr(2.5f) < 1.0f)
			{
				// bevel
				verts.push_back(curPos);
				verts.push_back(curPos + prevNorm * width);
				verts.push_back(curPos);
				verts.push_back(curPos + norm * width);
			}
			else
			{
				verts.push_back(curPos);
				verts.push_back(curPos + mid * width);
			}
		}
		else
		{
			verts.push_back(curPos);
			verts.push_back(curPos + mid * width);
		}

		prevDir = dir;
		prevNorm = norm;
		prevPos = curPos;
	}

	tris.clear();
	size_t n = verts.size() / 2;
	for (size_t i = 0; i < n; ++i)
	{
		size_t j = (i + 1) % n;
		tris.push_back(i * 2);
		tris.push_back(j * 2);
		tris.push_back(j * 2 + 1);

		tris.push_back(i * 2);
		tris.push_back(j * 2 + 1);
		tris.push_back(i * 2 + 1);
	}

	pRend->GetIRenderAuxGeom()->DrawTriangles(&verts[0], verts.size(), &tris[0], tris.size(), colFill);

	if (drawOutline)
		pRend->GetIRenderAuxGeom()->DrawPolyline(&outline[0], outline.size(), true, colOutline);
}

template
void DebugDrawRangePolygon(IRenderer* pRend, const std::list<Vec3>& polygon, float width,
                           ColorB colFill, ColorB colOutline, bool drawOutline);

template
void DebugDrawRangePolygon(IRenderer* pRend, const std::vector<Vec3>& polygon, float width,
                           ColorB colFill, ColorB colOutline, bool drawOutline);

//====================================================================
// DebugDrawRangePolygon
//====================================================================
void DebugDrawLabel(IRenderer* pRenderer, int col, int row, const char* szText, float* pColor)
{
	float ColumnSize = 11;
	float RowSize = 11;
	float baseY = 10;
	IRenderAuxText::Draw2dLabel(ColumnSize * static_cast<float>(col), baseY + RowSize * static_cast<float>(row), 1.2f, pColor, false, "%s", szText);
}

//====================================================================
// DebugDrawCircles
//====================================================================
void DebugDrawCircles(IRenderer* pRenderer, const Vec3& pos,
                      float minRadius, float maxRadius, int numRings,
                      const ColorF& insideCol, const ColorF& outsideCol)
{
	static int numPts = 32;
	static std::vector<Vec3> unitCircle;
	static bool init = false;

	if (init == false)
	{
		init = true;
		unitCircle.reserve(numPts);
		for (int i = 0; i < numPts; ++i)
		{
			float angle = gf_PI2 * ((float) i) / numPts;
			unitCircle.push_back(Vec3(sinf(angle), cosf(angle), 0.0f));
		}
	}
	for (int iRing = 0; iRing < numRings; ++iRing)
	{
		float ringFrac = 0.0f;
		if (numRings > 1)
			ringFrac += ((float) iRing) / (numRings - 1);

		float radius = (1.0f - ringFrac) * minRadius + ringFrac * maxRadius;
		ColorF col = (1.0f - ringFrac) * insideCol + ringFrac * outsideCol;

		Vec3 prevPt = pos + radius * unitCircle[numPts - 1];
		prevPt.z = GetDebugDrawZ(prevPt, true);
		for (int i = 0; i < numPts; ++i)
		{
			Vec3 pt = pos + radius * unitCircle[i];
			pt.z = GetDebugDrawZ(pt, true) + 0.05f;

			pRenderer->GetIRenderAuxGeom()->DrawLine(prevPt, col, pt, col);
			prevPt = pt;
		}
	}
}

//====================================================================
// DebugDrawEllipseOutline
//====================================================================
void DebugDrawEllipseOutline(IRenderer* pRend, const Vec3& pos, float radx, float rady, float orientation, ColorB col)
{
	Vec3 points[20];

	float sin_o = sinf(orientation);
	float cos_o = cosf(orientation);
	for (unsigned i = 0; i < 20; i++)
	{
		float angle = ((float) i / 20.0f) * gf_PI2;
		float sin_a = sinf(angle);
		float cos_a = cosf(angle);
		float x = (cos_o * cos_a * radx) - (sin_o * sin_a * rady);
		float y = (cos_o * sin_a * rady) + (sin_o * cos_a * radx);
		points[i] = pos + Vec3(x, y, 0.0f);
	}

	pRend->GetIRenderAuxGeom()->DrawPolyline(points, 20, true, col);
}

//====================================================================
// GetDebugDrawZ
//====================================================================
float GetDebugDrawZ(const Vec3& pt, bool useTerrain)
{
	if (useTerrain)
	{
		if (gAIEnv.CVars.DebugDrawOffset <= 0.0f)
			return -gAIEnv.CVars.DebugDrawOffset;
		I3DEngine* pEngine = gEnv->p3DEngine;
		float terrainZ = pEngine->GetTerrainElevation(pt.x, pt.y);
		float waterZ = pEngine->GetWaterLevel(&pt);
		return max(terrainZ, waterZ) + gAIEnv.CVars.DebugDrawOffset;
	}
	else
	{
		return pt.z + gAIEnv.CVars.DebugDrawOffset;
	}
}

//===================================================================
// GetDebugCameraPos
//===================================================================
Vec3 GetDebugCameraPos()
{
	return gEnv->pSystem->GetViewCamera().GetPosition();
}
