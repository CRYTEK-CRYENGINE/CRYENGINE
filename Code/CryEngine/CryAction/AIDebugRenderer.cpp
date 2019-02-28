// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIDebugRenderer.h"

#include <math.h>
#include <CryRenderer/IRenderer.h>
#include <CryMath/Cry_Vector3.h>

CAIDebugRenderer::CAIDebugRenderer(IRenderer* pRenderer)
	: m_pRenderer(pRenderer)
{
	m_pRenderAuxGeom = nullptr;
}

//===================================================================
// GetCameraPos
//===================================================================
Vec3 CAIDebugRenderer::GetCameraPos()
{
	return gEnv->pSystem->GetViewCamera().GetPosition();
}

//====================================================================
// GetDebugDrawZ
//====================================================================
float CAIDebugRenderer::GetDebugDrawZ(const Vec3& vPoint, bool bUseTerrainOrWater)
{
	const float g_drawOffset = 0.1f;

	if (bUseTerrainOrWater)
	{
		if (g_drawOffset <= 0.0f)
			return -g_drawOffset;
		I3DEngine* pEngine = gEnv->p3DEngine;
		float terrainZ = pEngine->GetTerrainElevation(vPoint.x, vPoint.y);
		float waterZ = pEngine->GetWaterLevel(&vPoint);
		return max(terrainZ, waterZ) + g_drawOffset;
	}
	else
		return vPoint.z + g_drawOffset;
}

void CAIDebugRenderer::DrawAABB(const AABB& aabb, bool bSolid, const ColorB& color, const EBoundingBoxDrawStyle& bbDrawStyle)
{
	m_pRenderAuxGeom->DrawAABB(aabb, bSolid, color, bbDrawStyle);
}

void CAIDebugRenderer::DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& color, const EBoundingBoxDrawStyle& bbDrawStyle)
{
	m_pRenderAuxGeom->DrawAABB(aabb, matWorld, bSolid, color, bbDrawStyle);
}

//====================================================================
// DebugDrawArrow
//====================================================================
void CAIDebugRenderer::DrawArrow(const Vec3& vPos, const Vec3& vLength, float fWidth, const ColorB& color)
{
	Vec3 points[7];
	Vec3 tris[5 * 3];

	float len = vLength.GetLength();
	if (len < 0.0001f)
		return;

	float headLen = fWidth * 2.0f;
	float headWidth = fWidth * 2.0f;

	if (headLen > len * 0.8f)
		headLen = len * 0.8f;

	Vec3 vDir(vLength / len);
	Vec3 norm(vLength.y, -vLength.x, 0);
	norm.NormalizeSafe();

	Vec3 end(vPos + vLength);
	Vec3 start(vPos);

	unsigned int n = 0;
	points[n++] = end;
	points[n++] = end - vDir * headLen - norm * headWidth / 2;
	PREFAST_SUPPRESS_WARNING(6386)
	points[n++] = end - vDir * headLen - norm * fWidth / 2;
	points[n++] = end - vDir * headLen + norm * fWidth / 2;
	points[n++] = end - vDir * headLen + norm * headWidth / 2;
	points[n++] = start - norm * fWidth / 2;
	points[n++] = start + norm * fWidth / 2;

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

	DrawTriangles(tris, n, color);
}

//====================================================================
// DebugDrawCapsuleOutline
//====================================================================
void CAIDebugRenderer::DrawCapsuleOutline(const Vec3& vPos0, const Vec3& vPos1, float fRadius, const ColorB& color)
{
	Vec3 points[20];
	Vec3 axisy = vPos1 - vPos0;
	axisy.Normalize();
	Vec3 axisx(axisy.y, -axisy.x, 0);
	axisx.Normalize();

	for (unsigned int i = 0; i < 10; i++)
	{
		float a = ((float)i / 9.0f) * gf_PI;
		points[i] = vPos1 + axisx * cosf(a) * fRadius + axisy * sinf(a) * fRadius;
	}

	for (unsigned int i = 0; i < 10; i++)
	{
		float a = gf_PI + ((float)i / 9.0f) * gf_PI;
		points[i + 10] = vPos0 + axisx * cosf(a) * fRadius + axisy * sinf(a) * fRadius;
	}

	DrawPolyline(points, 20, true, color);
}

//====================================================================
// DebugDrawCircleOutline
//====================================================================
void CAIDebugRenderer::DrawCircleOutline(const Vec3& vPos, float fRadius, const ColorB& color)
{
	Vec3 points[20];

	for (unsigned int i = 0; i < 20; i++)
	{
		float a = ((float)i / 20.0f) * gf_PI2;
		points[i] = vPos + Vec3(cosf(a) * fRadius, sinf(a) * fRadius, 0);
	}
	DrawPolyline(points, 20, true, color);
}

//====================================================================
// DebugDrawCircles
//====================================================================
void CAIDebugRenderer::DrawCircles(const Vec3& vPos,
                                   float fMinRadius, float fMaxRadius, int numRings,
                                   const ColorF& vInsideColor, const ColorF& vOutsideColor)
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
			float angle = gf_PI2 * ((float)i) / numPts;
			unitCircle.push_back(Vec3(sinf(angle), cosf(angle), 0.0f));
		}
	}

	for (int iRing = 0; iRing < numRings; ++iRing)
	{
		float ringFrac = 0.0f;
		if (numRings > 1)
			ringFrac += ((float)iRing) / (numRings - 1);

		float fRadius = (1.0f - ringFrac) * fMinRadius + ringFrac * fMaxRadius;
		ColorF col = (1.0f - ringFrac) * vInsideColor + ringFrac * vOutsideColor;

		Vec3 prevPt = vPos + fRadius * unitCircle[numPts - 1];
		prevPt.z = GetDebugDrawZ(prevPt, true);
		for (int i = 0; i < numPts; ++i)
		{
			Vec3 pt = vPos + fRadius * unitCircle[i];
			pt.z = GetDebugDrawZ(pt, true) + 0.05f;

			DrawLine(prevPt, col, pt, col);
			prevPt = pt;
		}
	}
}

void CAIDebugRenderer::DrawCone(const Vec3& vPos, const Vec3& vDir, float fRadius, float fHeight, const ColorB& color, bool fDrawShaded)
{
	m_pRenderAuxGeom->DrawCone(vPos, vDir, fRadius, fHeight, color, fDrawShaded);
}

void CAIDebugRenderer::DrawCylinder(const Vec3& vPos, const Vec3& vDir, float fRadius, float fHeight, const ColorB& color, bool bDrawShaded)
{
	m_pRenderAuxGeom->DrawCylinder(vPos, vDir, fRadius, fHeight, color, bDrawShaded);
}

//====================================================================
// DebugDrawEllipseOutline
//====================================================================
void CAIDebugRenderer::DrawEllipseOutline(const Vec3& vPos, float fRadiusX, float fRadiusY, float fOrientation, const ColorB& color)
{
	Vec3 points[20];

	float sin_o = sinf(fOrientation);
	float cos_o = cosf(fOrientation);
	for (unsigned int i = 0; i < 20; i++)
	{
		float angle = ((float)i / 20.0f) * gf_PI2;
		float sin_a = sinf(angle);
		float cos_a = cosf(angle);
		float x = (cos_o * cos_a * fRadiusX) - (sin_o * sin_a * fRadiusY);
		float y = (cos_o * sin_a * fRadiusY) + (sin_o * cos_a * fRadiusX);
		points[i] = vPos + Vec3(x, y, 0.0f);
	}

	DrawPolyline(points, 20, true, color);
}

//====================================================================
// DebugDrawLabel
//====================================================================
void CAIDebugRenderer::Draw2dLabel(int nCol, int nRow, const char* szText, const ColorB& color)
{
	float ColumnSize = 11;
	float RowSize = 11;
	float baseY = 10;
	float p_fColor[4] = {
		color.r / 255.0f,
		color.g / 255.0f,
		color.b / 255.0f,
		color.a / 255.0f
	};

	IRenderAuxText::Draw2dLabel(ColumnSize * static_cast<float>(nCol), baseY + RowSize * static_cast<float>(nRow), 1.2f, p_fColor, false, "%s", szText);
}

void CAIDebugRenderer::Draw2dLabel(float fX, float fY, float fFontSize, const ColorB& color, bool bCenter, const char* text, ...)
{
	va_list args;
	va_start(args, text);
	IRenderAuxText::DrawText(Vec3(fX, fY, 0.5f), fFontSize, color, eDrawText_IgnoreOverscan | eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Monospace | (bCenter ? eDrawText_Center : 0), text, args);
	va_end(args);
}

void CAIDebugRenderer::Draw2dLabelEx(float fX, float fY, float fFontSize, const ColorB& color, bool bFixedSize, bool bMonospace, bool bCenter, bool bFramed, const char* text, ...)
{
	va_list args;
	va_start(args, text);

	int flags = eDrawText_IgnoreOverscan | eDrawText_2D | eDrawText_800x600
	           | (bFixedSize ? eDrawText_FixedSize : 0)
	           | (bMonospace ? eDrawText_Monospace : 0)
	           | (bFramed ? eDrawText_Framed : 0)
	           | (bCenter ? eDrawText_Center : 0);

	IRenderAuxText::DrawText(Vec3(fX, fY, 0.5f), fFontSize, color, flags, text, args);

	va_end(args);
}

void CAIDebugRenderer::Draw3dLabel(Vec3 vPos, float fFontSize, const char* text, ...)
{
	va_list args;
	va_start(args, text);
	IRenderAuxText::DrawText(vPos, fFontSize, NULL, eDrawText_FixedSize | eDrawText_800x600, text, args);
	va_end(args);
}

void CAIDebugRenderer::Draw3dLabelEx(Vec3 vPos, float fFontSize, const ColorB& color, bool bFixedSize, bool bCenter, bool bDepthTest, bool bFramed, const char* text, ...)
{
	va_list args;
	va_start(args, text);

	int flags = (bFramed ? eDrawText_Framed : 0) | (bDepthTest ? eDrawText_DepthTest : 0) | (bFixedSize ? eDrawText_FixedSize : 0) | (bCenter ? eDrawText_Center : 0) | eDrawText_800x600;

	IRenderAuxText::DrawText(vPos, fFontSize, color, flags, text, args);

	va_end(args);
}

void CAIDebugRenderer::DrawLabel(Vec3 pos, SDrawTextInfo& ti, const char* text)
{
	IRenderAuxText::DrawText(pos, ti, text);
}

void CAIDebugRenderer::Draw2dImage(float fX, float fY, float fWidth, float fHeight, int nTextureID, float fS0, float fT0, float fS1, float fT1, float fAngle, float fR, float fG, float fB, float fA, float fZ)
{
	if (m_pRenderer)
	{
		IRenderAuxImage::Draw2dImage(fX, fY, fWidth, fHeight, nTextureID, fS0, fT0, fS1, fT1, fAngle, fR, fG, fB, fA, fZ);
	}
}

void CAIDebugRenderer::DrawLine(const Vec3& v0, const ColorB& colorV0, const Vec3& v1, const ColorB& colorV1, float thickness)
{
	m_pRenderAuxGeom->DrawLine(v0, colorV0, v1, colorV1, thickness);
}

void CAIDebugRenderer::DrawOBB(const OBB& obb, const Vec3& vPos, bool bSolid, const ColorB& color, const EBoundingBoxDrawStyle bbDrawStyle)
{
	m_pRenderAuxGeom->DrawOBB(obb, vPos, bSolid, color, bbDrawStyle);
}

void CAIDebugRenderer::DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& color, const EBoundingBoxDrawStyle bbDrawStyle)
{
	m_pRenderAuxGeom->DrawOBB(obb, matWorld, bSolid, color, bbDrawStyle);
}

void CAIDebugRenderer::DrawPolyline(const Vec3* va, uint32 nPoints, bool bClosed, const ColorB& color, float fThickness)
{
	m_pRenderAuxGeom->DrawPolyline(va, nPoints, bClosed, color, fThickness);
}

void CAIDebugRenderer::DrawPolyline(const Vec3* va, uint32 nPoints, bool bClosed, const ColorB* colorArray, float fThickness)
{
	m_pRenderAuxGeom->DrawPolyline(va, nPoints, bClosed, colorArray, fThickness);
}

//====================================================================
// DebugDrawRangeArc
//====================================================================
void CAIDebugRenderer::DrawRangeArc(const Vec3& vPos, const Vec3& vDir, float fAngle, float fRadius, float fWidth,
                                    const ColorB& colorFill, const ColorB& colorOutline, bool bDrawOutline)
{
	const unsigned npts = 12;

	Vec3 points[npts];
	Vec3 pointsOutline[npts];
	Vec3 tris[(npts - 1) * 2 * 3];

	Vec3 forw(vDir.x, vDir.y, 0.0f);
	forw.NormalizeSafe();
	Vec3 right(forw.y, -forw.x, 0);

	if (fWidth > fRadius) fWidth = fRadius;

	for (unsigned int i = 0; i < npts; i++)
	{
		float a = ((float)i / (float)(npts - 1) - 0.5f) * fAngle;
		points[i] = forw * cosf(a) + right * sinf(a);
		pointsOutline[i] = vPos + points[i] * fRadius;
	}

	unsigned int n = 0;
	for (unsigned int i = 0; i < npts - 1; i++)
	{
		tris[n++] = vPos + points[i] * (fRadius - fWidth);
		tris[n++] = vPos + points[i + 1] * fRadius;
		tris[n++] = vPos + points[i] * fRadius;

		tris[n++] = vPos + points[i] * (fRadius - fWidth);
		tris[n++] = vPos + points[i + 1] * (fRadius - fWidth);
		tris[n++] = vPos + points[i + 1] * fRadius;
	}

	DrawTriangles(tris, n, colorFill);
	if (bDrawOutline)
	{
		DrawPolyline(pointsOutline, npts, false, colorOutline);
		DrawLine(vPos + forw * (fRadius - fWidth / 4), colorOutline, vPos + forw * (fRadius + fWidth / 4), colorOutline);
	}
}

//====================================================================
// DebugDrawRange
//====================================================================
void CAIDebugRenderer::DrawRangeBox(const Vec3& vPos, const Vec3& vDir, float fSizeX, float fSizeY, float fWidth,
                                    const ColorB& colorFill, const ColorB& colorOutline, bool bDrawOutline)
{
	float minX = fSizeX - fWidth;
	float maxX = fSizeX;
	float minY = fSizeY - fWidth;
	float maxY = fSizeY;

	if (maxX < 0.001f || maxY < 0.001f || minX > maxX || minY > maxY)
		return;

	Vec3 points[8];
	Vec3 tris[8 * 3];
	Vec3 norm(vDir.y, -vDir.x, vDir.z);

	points[0] = vPos + norm * -minX + vDir * -minY;
	points[1] = vPos + norm * minX + vDir * -minY;
	points[2] = vPos + norm * minX + vDir * minY;
	points[3] = vPos + norm * -minX + vDir * minY;

	points[4] = vPos + norm * -maxX + vDir * -maxY;
	points[5] = vPos + norm * maxX + vDir * -maxY;
	points[6] = vPos + norm * maxX + vDir * maxY;
	points[7] = vPos + norm * -maxX + vDir * maxY;

	unsigned int n = 0;

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

	DrawTriangles(tris, 8 * 3, colorFill);
	if (bDrawOutline)
		DrawPolyline(&points[4], 4, true, colorOutline);
}

//====================================================================
// DebugDrawRangeCircle
//====================================================================
void CAIDebugRenderer::DrawRangeCircle(const Vec3& vPos, float fRadius, float fWidth,
                                       const ColorB& colorFill, const ColorB& colorOutline, bool bDrawOutline)
{
	const unsigned npts = 24;

	Vec3 points[npts];
	Vec3 pointsOutline[npts];
	Vec3 tris[npts * 2 * 3];

	if (fWidth > fRadius) fWidth = fRadius;

	for (unsigned int i = 0; i < npts; i++)
	{
		float a = ((float)i / (float)npts) * gf_PI2;
		points[i] = Vec3(cosf(a), sinf(a), 0);
		pointsOutline[i] = vPos + points[i] * fRadius;
	}

	unsigned int n = 0;
	for (unsigned int i = 0; i < npts; i++)
	{
		tris[n++] = vPos + points[i] * (fRadius - fWidth);
		tris[n++] = vPos + points[i] * fRadius;
		tris[n++] = vPos + points[(i + 1) % npts] * fRadius;

		tris[n++] = vPos + points[i] * (fRadius - fWidth);
		tris[n++] = vPos + points[(i + 1) % npts] * fRadius;
		tris[n++] = vPos + points[(i + 1) % npts] * (fRadius - fWidth);
	}

	DrawTriangles(tris, npts * 2 * 3, colorFill);
	if (bDrawOutline)
		DrawPolyline(pointsOutline, npts, true, colorOutline);
}

//====================================================================
// DebugDrawRangePolygon
//====================================================================
void CAIDebugRenderer::DrawRangePolygon(const Vec3 polygon[], int nVertices, float fWidth,
                                        const ColorB& colorFill, const ColorB& colorOutline, bool bDrawOutline)
{
	static std::vector<Vec3> verts;
	static std::vector<vtx_idx> tris;
	static std::vector<Vec3> outline;

	if (nVertices < 3) return;

	Vec3 prevDir(polygon[0] - polygon[nVertices - 1]);
	prevDir.NormalizeSafe();
	Vec3 prevNorm(-prevDir.y, prevDir.x, 0.0f);
	prevNorm.NormalizeSafe();
	Vec3 prevPos(polygon[nVertices - 1]);

	verts.clear();
	outline.clear();

	const Vec3* li, * linext;
	const Vec3* liend = polygon + nVertices;
	for (li = polygon; li != liend; ++li)
	{
		linext = li;
		++linext;
		if (linext == liend)
			linext = polygon;

		const Vec3& curPos(*li);
		const Vec3& nextPos(*linext);
		Vec3 vDir(nextPos - curPos);
		Vec3 norm(-vDir.y, vDir.x, 0.0f);
		norm.NormalizeSafe();

		Vec3 mid((prevNorm + norm) * 0.5f);
		float dmr2 = sqr(mid.x) + sqr(mid.y);
		if (dmr2 > 0.00001f)
			mid *= 1.0f / dmr2;

		float cross = prevDir.x * vDir.y - vDir.x * prevDir.y;

		outline.push_back(curPos);

		if (cross < 0.0f)
		{
			if (dmr2 * sqr(2.5f) < 1.0f)
			{
				// bevel
				verts.push_back(curPos);
				verts.push_back(curPos + prevNorm * fWidth);
				verts.push_back(curPos);
				verts.push_back(curPos + norm * fWidth);
			}
			else
			{
				verts.push_back(curPos);
				verts.push_back(curPos + mid * fWidth);
			}
		}
		else
		{
			verts.push_back(curPos);
			verts.push_back(curPos + mid * fWidth);
		}

		prevDir = vDir;
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

	m_pRenderAuxGeom->DrawTriangles(&verts[0], verts.size(), &tris[0], tris.size(), colorFill);
	if (bDrawOutline)
		DrawPolyline(&outline[0], outline.size(), true, colorOutline);
}

void CAIDebugRenderer::DrawSphere(const Vec3& vPos, float fRadius, const ColorB& color, bool bDrawShaded)
{
	m_pRenderAuxGeom->DrawSphere(vPos, fRadius, color, bDrawShaded);
}

void CAIDebugRenderer::DrawTriangle(const Vec3& v0, const ColorB& colorV0, const Vec3& v1, const ColorB& colorV1, const Vec3& v2, const ColorB& colorV2)
{
	m_pRenderAuxGeom->DrawTriangle(v0, colorV0, v1, colorV1, v2, colorV2);
}

void CAIDebugRenderer::DrawTriangles(const Vec3* va, unsigned int numPoints, const ColorB& color)
{
	m_pRenderAuxGeom->DrawTriangles(va, numPoints, color);
}

//====================================================================
// DebugDrawWireFOVCone
//====================================================================
void CAIDebugRenderer::DrawWireFOVCone(const Vec3& vPos, const Vec3& vDir, float fRadius, float fFOV, const ColorB& color)
{
	const unsigned int npts = 32;
	const unsigned int npts2 = 16;
	Vec3 points[npts];
	Vec3 pointsx[npts2];
	Vec3 pointsy[npts2];

	Matrix33 base;
	base.SetRotationVDir(vDir);

	const float halfFOV = fFOV * 0.5f;
	const float coneRadius = sinf(halfFOV) * fRadius;
	const float coneHeight = cosf(halfFOV) * fRadius;

	for (unsigned int i = 0; i < npts; i++)
	{
		float a = ((float)i / (float)npts) * gf_PI2;
		float rx = cosf(a) * coneRadius;
		float ry = sinf(a) * coneRadius;
		points[i] = vPos + base.TransformVector(Vec3(rx, coneHeight, ry));
	}

	for (unsigned int i = 0; i < npts2; i++)
	{
		float a = -halfFOV + ((float)i / (float)(npts2 - 1)) * fFOV;
		float rx = sinf(a) * fRadius;
		float ry = cosf(a) * fRadius;
		pointsx[i] = vPos + base.TransformVector(Vec3(rx, ry, 0));
		pointsy[i] = vPos + base.TransformVector(Vec3(0, ry, rx));
	}

	DrawPolyline(points, npts, true, color);
	DrawPolyline(pointsx, npts2, false, color);
	DrawPolyline(pointsy, npts2, false, color);

	DrawLine(points[0], color, vPos, color);
	DrawLine(points[npts / 4], color, vPos, color);
	DrawLine(points[npts / 2], color, vPos, color);
	DrawLine(points[npts / 2 + npts / 4], color, vPos, color);
}

//====================================================================
// DebugDrawWireSphere
//====================================================================
void CAIDebugRenderer::DrawWireSphere(const Vec3& vPos, float fRadius, const ColorB& color)
{
	const unsigned int npts = 32;
	Vec3 xpoints[npts];
	Vec3 ypoints[npts];
	Vec3 zpoints[npts];

	for (unsigned int i = 0; i < npts; i++)
	{
		float a = ((float)i / (float)npts) * gf_PI2;
		float rx = cosf(a) * fRadius;
		float ry = sinf(a) * fRadius;
		xpoints[i] = vPos + Vec3(rx, ry, 0);
		ypoints[i] = vPos + Vec3(0, rx, ry);
		zpoints[i] = vPos + Vec3(ry, 0, rx);
	}

	DrawPolyline(xpoints, npts, true, color);
	DrawPolyline(ypoints, npts, true, color);
	DrawPolyline(zpoints, npts, true, color);
}

ITexture* CAIDebugRenderer::LoadTexture(const char* sNameOfTexture, uint32 nFlags)
{
	return m_pRenderer ? m_pRenderer->EF_LoadTexture(sNameOfTexture, nFlags) : nullptr;
}

// [9/16/2010 evgeny] ProjectToScreen is not guaranteed to work if used outside Renderer
bool CAIDebugRenderer::ProjectToScreen(float fInX, float fInY, float fInZ, float* fOutX, float* fOutY, float* fOutZ)
{
	return m_pRenderer ? m_pRenderer->ProjectToScreen(fInX, fInY, fInZ, fOutX, fOutY, fOutZ) : false;
}

void CAIDebugRenderer::TextToScreen(float fX, float fY, const char* format, ...)
{
	// Copy-pasted from Renderer.cpp
	char buffer[512];
	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	IRenderAuxText::TextToScreen(fX, fY, buffer);
}

void CAIDebugRenderer::TextToScreenColor(int nX, int nY, float fRed, float fGreen, float fBlue, float fAlpha, const char* format, ...)
{
	// Copy-pasted from Renderer.cpp
	char buffer[512];
	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	IRenderAuxText::TextToScreenColor(nX, nY, fRed, fGreen, fBlue, fAlpha, buffer);
}

void CAIDebugRenderer::Init2DMode()
{
	m_pRenderAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Def2DPublicRenderflags));
}

void CAIDebugRenderer::Init3DMode()
{
	m_pRenderAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Def3DPublicRenderflags));
}

void CAIDebugRenderer::SetAlphaBlended(bool bOn)
{
	IRenderAuxGeom* pRenderAuxGeom = m_pRenderAuxGeom;
	SAuxGeomRenderFlags flags = pRenderAuxGeom->GetRenderFlags();
	flags.SetAlphaBlendMode(bOn ? e_AlphaBlended : e_AlphaNone);
	pRenderAuxGeom->SetRenderFlags(flags);
}

void CAIDebugRenderer::SetBackFaceCulling(bool bOn)
{
	IRenderAuxGeom* pRenderAuxGeom = m_pRenderAuxGeom;
	SAuxGeomRenderFlags flags = pRenderAuxGeom->GetRenderFlags();
	flags.SetCullMode(bOn ? e_CullModeBack : e_CullModeNone);
	pRenderAuxGeom->SetRenderFlags(flags);
}

void CAIDebugRenderer::SetDepthTest(bool bOn)
{
	IRenderAuxGeom* pRenderAuxGeom = m_pRenderAuxGeom;
	SAuxGeomRenderFlags flags = pRenderAuxGeom->GetRenderFlags();
	flags.SetDepthTestFlag(bOn ? e_DepthTestOn : e_DepthTestOff);
	pRenderAuxGeom->SetRenderFlags(flags);
}

void CAIDebugRenderer::SetDepthWrite(bool bOn)
{
	IRenderAuxGeom* pRenderAuxGeom = m_pRenderAuxGeom;
	SAuxGeomRenderFlags flags = pRenderAuxGeom->GetRenderFlags();
	flags.SetDepthWriteFlag(bOn ? e_DepthWriteOn : e_DepthWriteOff);
	pRenderAuxGeom->SetRenderFlags(flags);
}

void CAIDebugRenderer::SetDrawInFront(bool bOn)
{
	IRenderAuxGeom* pRenderAuxGeom = m_pRenderAuxGeom;
	SAuxGeomRenderFlags flags = pRenderAuxGeom->GetRenderFlags();
	flags.SetDrawInFrontMode(bOn ? e_DrawInFrontOn : e_DrawInFrontOff);
	pRenderAuxGeom->SetRenderFlags(flags);
}

unsigned int CAIDebugRenderer::PopState()
{
	if (m_pRenderAuxGeom)
	{
		m_pRenderer->SubmitAuxGeom(m_pRenderAuxGeom, false);
		m_pRenderAuxGeom = m_pRenderer->GetOrCreateIRenderAuxGeom();
		m_pRenderAuxGeom->SetRenderFlags(m_FlagsStack.top());
	}
	m_FlagsStack.pop();
	return m_FlagsStack.size();
}

unsigned int CAIDebugRenderer::PushState()
{
	if (!m_pRenderAuxGeom)
	{
		m_pRenderAuxGeom = m_pRenderer->GetOrCreateIRenderAuxGeom();
	}
	m_FlagsStack.push(m_pRenderAuxGeom->GetRenderFlags());
	return m_FlagsStack.size();
}
