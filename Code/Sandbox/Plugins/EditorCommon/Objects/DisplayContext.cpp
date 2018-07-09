// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryMath/Cry_Geo.h>
#include "Objects/DisplayContext.h"
#include <CryRenderer/IRenderAuxGeom.h>

#include "Preferences/ViewportPreferences.h"

#include "IIconManager.h"
#include "IDisplayViewport.h"

#include <Cry3DEngine/I3DEngine.h>

#define FREEZE_COLOR RGB(100, 100, 100)

namespace DC_Private
{
	inline Vec3 Rgb2Vec(COLORREF color)
	{
		return Vec3(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f);
	}
}

//////////////////////////////////////////////////////////////////////////
DisplayContext::DisplayContext()
{
	view = 0;
	renderer = 0;
	engine = 0;
	flags = 0;
	pIconManager = 0;
	m_renderState = 0;

	m_currentMatrix = 0;
	m_matrixStack[m_currentMatrix].SetIdentity();
	pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	m_thickness = 0;

	m_width = 0;
	m_height = 0;

	m_textureLabels.reserve(100);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::SetView(IDisplayViewport* pView)
{
	view = pView;
	int width, height;
	view->GetDimensions(&width, &height);
	m_width = float(width);
	m_height = float(height);
	m_textureLabels.resize(0);
}

void DisplayContext::SetCamera(CCamera* pCamera)
{
	assert(pCamera);
	camera = pCamera;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::InternalDrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1)
{
	pRenderAuxGeom->DrawLine(v0, colV0, v1, colV1, m_thickness);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawPoint(const Vec3& p, int nSize)
{
	pRenderAuxGeom->DrawPoint(p, m_color4b, nSize);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTri(const Vec3& p1, const Vec3& p2, const Vec3& p3)
{
	pRenderAuxGeom->DrawTriangle(p1, m_color4b, p2, m_color4b, p3, m_color4b);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawQuad(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4)
{
	pRenderAuxGeom->DrawTriangle(p1, m_color4b, p2, m_color4b, p3, m_color4b);
	pRenderAuxGeom->DrawTriangle(p3, m_color4b, p4, m_color4b, p1, m_color4b);
	//Vec3 p[4] = { p1, p2, p3, p4? };
	//pRenderAuxGeom->DrawPolyline( poly,4,true,m_color4b );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawCylinder(const Vec3& p1, const Vec3& p2, float radius, float height)
{
	Vec3 p[2] = { ToWS(p1), ToWS(p2) };
	Vec3 dir = p[1] - p[0];

	// TODO: pretty weird that 2D drawing should be the exception here. Investigate
	if (flags & DISPLAY_2D)
	{
		Vec3 normalDir = dir.GetNormalized();
		float len = m_matrixStack[m_currentMatrix].TransformVector(normalDir).GetLength();
		radius *= len;
		height *= len;
	}

	pRenderAuxGeom->DrawCylinder(p[0], dir, radius, height, m_color4b);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawWireBox(const Vec3& min, const Vec3& max)
{
	pRenderAuxGeom->DrawAABB(AABB(min, max), false, m_color4b, eBBD_Faceted);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawSolidBox(const Vec3& min, const Vec3& max)
{
	pRenderAuxGeom->DrawAABB(AABB(min, max), true, m_color4b, eBBD_Faceted);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawLine(const Vec3& p1, const Vec3& p2)
{
	InternalDrawLine(p1, m_color4b, p2, m_color4b);
}

void DisplayContext::DrawDottedLine(const Vec3& p1, const Vec3& p2, float interval)
{
	// ideally we would do the interval in screen space, but for now do it in local space
	Vec3 vDir = p2 - p1;
	float length = vDir.len();
	int totsegments = length / interval;

	if (totsegments == 0 || length < 0.00001f)
		return;

	vDir.Normalize();
	// clamp the segments to avoid explosion of primitives
	clamp_tpl(totsegments, 0, 100);

	std::vector <Vec3> lineVerts;
	lineVerts.reserve(totsegments * 2);

	Vec3 v1, v2;

	for (int i = 0; i < totsegments; ++i)
	{
		v1 = p1 + i * vDir * interval;
		v2 = v1 + vDir * interval * 0.5f;
		lineVerts.push_back(v1);
		lineVerts.push_back(v2);
	}

	pRenderAuxGeom->DrawLines(&lineVerts[0], totsegments * 2, m_color4b);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawPolyLine(const Vec3* pnts, int numPoints, bool cycled)
{
	MAKE_SURE(numPoints >= 2, return );
	MAKE_SURE(pnts != 0, return );

	int numSegments = cycled ? numPoints : (numPoints - 1);
	Vec3 p1 = pnts[0];
	Vec3 p2;
	for (int i = 0; i < numSegments; i++)
	{
		int j = (i + 1) % numPoints;
		p2 = pnts[j];
		InternalDrawLine(p1, m_color4b, p2, m_color4b);
		p1 = p2;
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTerrainCircle(const Vec3& worldPos, float radius, float height)
{
	// Draw circle with default radius.
	Vec3 p0, p1;
	p0.x = worldPos.x + radius * sin(0.0f);
	p0.y = worldPos.y + radius * cos(0.0f);
	p0.z = engine->GetTerrainElevation(p0.x, p0.y) + height;

	float step = 10.0f / 180 * gf_PI;
	for (float angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = worldPos.x + radius * sin(angle);
		p1.y = worldPos.y + radius * cos(angle);
		p1.z = engine->GetTerrainElevation(p1.x, p1.y) + height;

		if (p0.z>height && p1.z>height)
		{
			InternalDrawLine(p0, m_color4b, p1, m_color4b);
		}

		p0 = p1;
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTerrainCircle(const Vec3& worldPos, float radius, float angle1, float angle2, float height)
{
	// Draw circle with default radius.
	Vec3 p0, p1;
	p0.x = worldPos.x + radius * sin(angle1);
	p0.y = worldPos.y + radius * cos(angle1);
	p0.z = engine->GetTerrainElevation(p0.x, p0.y) + height;

	float step = 10.0f / 180 * gf_PI;
	for (float angle = step + angle1; angle < angle2; angle += step)
	{
		p1.x = worldPos.x + radius * sin(angle);
		p1.y = worldPos.y + radius * cos(angle);
		p1.z = engine->GetTerrainElevation(p1.x, p1.y) + height;

		if (p0.z>height && p1.z>height)
		{
			InternalDrawLine(p0, m_color4b, p1, m_color4b);
		}

		p0 = p1;
	}

	p1.x = worldPos.x + radius * sin(angle2);
	p1.y = worldPos.y + radius * cos(angle2);
	p1.z = engine->GetTerrainElevation(p1.x, p1.y) + height;

	InternalDrawLine(p0, m_color4b, p1, m_color4b);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawCircle(const Vec3& pos, float radius, int nUnchangedAxis)
{
	// Draw circle with default radius.
	Vec3 p0, p1;
	p0[nUnchangedAxis] = pos[nUnchangedAxis];
	p0[(nUnchangedAxis + 1) % 3] = pos[(nUnchangedAxis + 1) % 3] + radius * sin(0.0f);
	p0[(nUnchangedAxis + 2) % 3] = pos[(nUnchangedAxis + 2) % 3] + radius * cos(0.0f);
	float step = 10.0f / 180 * gf_PI;
	for (float angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1[nUnchangedAxis] = pos[nUnchangedAxis];
		p1[(nUnchangedAxis + 1) % 3] = pos[(nUnchangedAxis + 1) % 3] + radius * sin(angle);
		p1[(nUnchangedAxis + 2) % 3] = pos[(nUnchangedAxis + 2) % 3] + radius * cos(angle);
		InternalDrawLine(p0, m_color4b, p1, m_color4b);
		p0 = p1;
	}
}

void DisplayContext::DrawDisc(float radius, float angle)
{
	Vec3 p0(0.0f, 0.0f, 0.0f);
	Vec3 p1;
	Vec3 p2;

	p1.x = 0.0f;
	p1.y = radius;
	p1.z = 0.0f;

	// split angle in steps of 10 degrees
	float step = 10.0f / 180 * gf_PI;
	if (angle < 0.0f)
	{
		step = -step;
	}
	int numsteps = angle / step;

	std::vector <Vec3> vertices;

	int numverts = numsteps * 3 + 3;
	vertices.reserve(numverts);

	for (int i = 0; i < numsteps; ++i)
	{
		float curangle = (i + 1) * step;

		p2 = p1;

		p1.x = radius * sin(curangle);
		p1.y = radius * cos(curangle);
		p1.z = 0.0f;

		// create our triangle vertices
		vertices.push_back(p0);
		vertices.push_back(p1);
		vertices.push_back(p2);
	}

	// final vertex at the exact angle
	p2 = p1;

	p1.x = radius * sin(angle);
	p1.y = radius * cos(angle);
	p1.z = 0.0f;

	vertices.push_back(p0);
	vertices.push_back(p1);
	vertices.push_back(p2);

	pRenderAuxGeom->DrawTriangles(&vertices[0], numverts, m_color4b);
}

void DisplayContext::DrawCircle3D(float radius, float angle, float thickness)
{
	// split angle in steps of 10 degrees
	float step = 10.0f / 180 * gf_PI;
	if (angle < 0.0f)
	{
		step = -step;
	}
	int numsteps = angle / step;

	std::vector <Vec3> vertices;

	int numverts = numsteps * 2 + 2;
	vertices.reserve(numverts);

	Vec3 p1;
	Vec3 p2;

	p1.x = 0.0f;
	p1.y = radius;
	p1.z = 0.0f;

	for (int i = 0; i < numsteps; ++i)
	{
		float curangle = (i + 1) * step;

		p2 = p1;

		p1.x = radius * sin(curangle);
		p1.y = radius * cos(curangle);
		p1.z = 0.0f;

		// create our triangle vertices
		vertices.push_back(p1);
		vertices.push_back(p2);
	}

	// final vertex at the exact angle
	p2 = p1;

	p1.x = radius * sin(angle);
	p1.y = radius * cos(angle);
	p1.z = 0.0f;

	vertices.push_back(p1);
	vertices.push_back(p2);

	pRenderAuxGeom->DrawLines(&vertices[0], numverts, m_color4b, thickness);
}

void DisplayContext::DrawRing(float innerRadius, float outerRadius, float angle)
{
	Vec3 p1Inner;
	Vec3 p2Inner;

	Vec3 p1Outer;
	Vec3 p2Outer;

	p1Inner.x = 0.0f;
	p1Inner.y = innerRadius;
	p1Inner.z = 0.0f;

	p1Outer.x = 0.0f;
	p1Outer.y = outerRadius;
	p1Outer.z = 0.0f;

	// split angle in steps of 10 degrees
	float step = 10.0f / 180 * gf_PI;
	if (angle < 0.0f)
	{
		step = -step;
	}
	int numsteps = angle / step;

	std::vector <Vec3> vertices;

	int numverts = numsteps * 6 + 6;
	vertices.reserve(numverts);

	for (int i = 0; i < numsteps; ++i)
	{
		float curangle = (i + 1) * step;
		float sinVal = sin(curangle);
		float cosVal = cos(curangle);

		p2Inner = p1Inner;

		p1Inner.x = innerRadius * sinVal;
		p1Inner.y = innerRadius * cosVal;
		p1Inner.z = 0.0f;

		p2Outer = p1Outer;

		p1Outer.x = outerRadius * sinVal;
		p1Outer.y = outerRadius * cosVal;
		p1Outer.z = 0.0f;

		// create our triangle vertices
		vertices.push_back(p2Inner);
		vertices.push_back(p1Inner);
		vertices.push_back(p2Outer);

		vertices.push_back(p1Inner);
		vertices.push_back(p1Outer);
		vertices.push_back(p2Outer);
	}

	// final strip at exact angle
	float sinVal = sin(angle);
	float cosVal = cos(angle);

	p2Inner = p1Inner;

	p1Inner.x = innerRadius * sinVal;
	p1Inner.y = innerRadius * cosVal;
	p1Inner.z = 0.0f;

	p2Outer = p1Outer;

	p1Outer.x = outerRadius * sinVal;
	p1Outer.y = outerRadius * cosVal;
	p1Outer.z = 0.0f;

	// create our triangle vertices
	vertices.push_back(p2Inner);
	vertices.push_back(p1Inner);
	vertices.push_back(p2Outer);

	vertices.push_back(p1Inner);
	vertices.push_back(p1Outer);
	vertices.push_back(p2Outer);

	pRenderAuxGeom->DrawTriangles(&vertices[0], numverts, m_color4b);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawWireCircle2d(const CPoint& center, float radius, float z)
{
	Vec3 p0, p1, pos;
	pos.x = float(center.x);
	pos.y = float(center.y);
	pos.z = z;
	p0.x = pos.x + radius * sin(0.0f);
	p0.y = pos.y + radius * cos(0.0f);
	p0.z = z;
	float step = 10.0f / 180 * gf_PI;

	int prevState = GetState();
	//SetState( (prevState|e_Mode2D) & (~e_Mode3D) );
	for (float angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = pos.x + radius * sin(angle);
		p1.y = pos.y + radius * cos(angle);
		p1.z = z;
		InternalDrawLine(p0, m_color4b, p1, m_color4b);
		p0 = p1;
	}
	SetState(prevState);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawWireSphere(const Vec3& pos, float radius)
{
	Vec3 p0, p1;
	float step = 10.0f / 180 * gf_PI;
	float angle;

	// Z Axis
	p0 = pos;
	p1 = pos;
	p0.x += radius * sin(0.0f);
	p0.y += radius * cos(0.0f);
	for (angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = pos.x + radius * sin(angle);
		p1.y = pos.y + radius * cos(angle);
		p1.z = pos.z;
		InternalDrawLine(p0, m_color4b, p1, m_color4b);
		p0 = p1;
	}

	// X Axis
	p0 = pos;
	p1 = pos;
	p0.y += radius * sin(0.0f);
	p0.z += radius * cos(0.0f);
	for (angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = pos.x;
		p1.y = pos.y + radius * sin(angle);
		p1.z = pos.z + radius * cos(angle);
		InternalDrawLine(p0, m_color4b, p1, m_color4b);
		p0 = p1;
	}

	// Y Axis
	p0 = pos;
	p1 = pos;
	p0.x += radius * sin(0.0f);
	p0.z += radius * cos(0.0f);
	for (angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = pos.x + radius * sin(angle);
		p1.y = pos.y;
		p1.z = pos.z + radius * cos(angle);
		InternalDrawLine(p0, m_color4b, p1, m_color4b);
		p0 = p1;
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawWireSphere(const Vec3& pos, const Vec3 radius)
{
	Vec3 p0, p1;
	float step = 10.0f / 180 * gf_PI;
	float angle;

	// Z Axis
	p0 = pos;
	p1 = pos;
	p0.x += radius.x * sin(0.0f);
	p0.y += radius.y * cos(0.0f);
	for (angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = pos.x + radius.x * sin(angle);
		p1.y = pos.y + radius.y * cos(angle);
		p1.z = pos.z;
		InternalDrawLine(p0, m_color4b, p1, m_color4b);
		p0 = p1;
	}

	// X Axis
	p0 = pos;
	p1 = pos;
	p0.y += radius.y * sin(0.0f);
	p0.z += radius.z * cos(0.0f);
	for (angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = pos.x;
		p1.y = pos.y + radius.y * sin(angle);
		p1.z = pos.z + radius.z * cos(angle);
		InternalDrawLine(p0, m_color4b, p1, m_color4b);
		p0 = p1;
	}

	// Y Axis
	p0 = pos;
	p1 = pos;
	p0.x += radius.x * sin(0.0f);
	p0.z += radius.z * cos(0.0f);
	for (angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = pos.x + radius.x * sin(angle);
		p1.y = pos.y;
		p1.z = pos.z + radius.z * cos(angle);
		InternalDrawLine(p0, m_color4b, p1, m_color4b);
		p0 = p1;
	}
}

void DisplayContext::DrawWireQuad2d(const CPoint& pmin, const CPoint& pmax, float z, bool drawInFront, bool depthTest)
{
	int prevState = GetState();
	SetState((prevState | e_Mode2D | (drawInFront ? e_DrawInFrontOn : e_DrawInFrontOff) | (depthTest ? e_DepthTestOn : e_DepthTestOff) & (~e_Mode3D)));
	InternalDrawLine(Vec3(float(pmin.x), float(pmin.y), z), m_color4b, Vec3(float(pmax.x), float(pmin.y), z), m_color4b);
	InternalDrawLine(Vec3(float(pmax.x), float(pmin.y), z), m_color4b, Vec3(float(pmax.x), float(pmax.y), z), m_color4b);
	InternalDrawLine(Vec3(float(pmax.x), float(pmax.y), z), m_color4b, Vec3(float(pmin.x), float(pmax.y), z), m_color4b);
	InternalDrawLine(Vec3(float(pmin.x), float(pmax.y), z), m_color4b, Vec3(float(pmin.x), float(pmin.y), z), m_color4b);
	SetState(prevState);
}

void DisplayContext::DrawLine2d(const CPoint& p1, const CPoint& p2, float z)
{
	int prevState = GetState();

	SetState((prevState | e_Mode2D) & (~e_Mode3D));

	// If we don't have correct information, we try to get it, but while we
	// don't, we skip rendering this frame.
	if (m_width == 0 || m_height == 0)
	{
		CRect rc;
		if (view)
		{
			// We tell the window to update itself, as it might be needed to
			// get correct information.
			view->Update();
			int width, height;
			view->GetDimensions(&width, &height);
			m_width = float(width);
			m_height = float(height);
		}
	}
	else
	{
		InternalDrawLine(Vec3(p1.x / m_width, p1.y / m_height, z), m_color4b, Vec3(p2.x / m_width, p2.y / m_height, z), m_color4b);
	}

	SetState(prevState);
}

void DisplayContext::DrawLine2dGradient(const CPoint& p1, const CPoint& p2, float z, ColorB firstColor, ColorB secondColor)
{
	int prevState = GetState();

	SetState((prevState | e_Mode2D) & (~e_Mode3D));
	InternalDrawLine(Vec3(p1.x / m_width, p1.y / m_height, z), firstColor, Vec3(p2.x / m_width, p2.y / m_height, z), secondColor);
	SetState(prevState);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawQuadGradient(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4, ColorB firstColor, ColorB secondColor)
{
	pRenderAuxGeom->DrawTriangle(p1, firstColor, p2, firstColor, p3, secondColor);
	pRenderAuxGeom->DrawTriangle(p3, secondColor, p4, secondColor, p1, firstColor);
}

//////////////////////////////////////////////////////////////////////////
COLORREF DisplayContext::GetSelectedColor()
{
	float t = GetTickCount() / 1000.0f;
	float r1 = fabs(sin(t * 8.0f));
	if (r1 > 255)
		r1 = 255;
	return RGB(255, 0, r1 * 255);
	//			float r2 = cos(t*3);
	//dc.renderer->SetMaterialColor( 1,0,r1,0.5f );
}

COLORREF DisplayContext::GetFreezeColor()
{
	return FREEZE_COLOR;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::SetSelectedColor(float fAlpha)
{
	SetColor(GetSelectedColor(), fAlpha);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::SetFreezeColor()
{
	SetColor(FREEZE_COLOR, 0.5f);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawLine(const Vec3& p1, const Vec3& p2, const ColorF& col1, const ColorF& col2)
{
	InternalDrawLine(p1, ColorB(uint8(col1.r * 255.0f), uint8(col1.g * 255.0f), uint8(col1.b * 255.0f), uint8(col1.a * 255.0f)),
	                 p2, ColorB(uint8(col2.r * 255.0f), uint8(col2.g * 255.0f), uint8(col2.b * 255.0f), uint8(col2.a * 255.0f)));
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawLine(const Vec3& p1, const Vec3& p2, COLORREF rgb1, COLORREF rgb2)
{
	Vec3 c1 = DC_Private::Rgb2Vec(rgb1);
	Vec3 c2 = DC_Private::Rgb2Vec(rgb2);
	InternalDrawLine(p1, ColorB(GetRValue(rgb1), GetGValue(rgb1), GetBValue(rgb1), 255), p2, ColorB(GetRValue(rgb2), GetGValue(rgb2), GetBValue(rgb2), 255));
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::PushMatrix(const Matrix34& tm)
{
	assert(m_currentMatrix < 32);
	if (m_currentMatrix < 32)
	{
		int prevMatrixIndex = m_currentMatrix++;
		m_matrixStack[m_currentMatrix] = m_matrixStack[prevMatrixIndex] * tm;
		m_previousMatrixIndex[prevMatrixIndex] = pRenderAuxGeom->PushMatrix(m_matrixStack[m_currentMatrix]);
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::PopMatrix()
{
	assert(m_currentMatrix > 0);
	if (m_currentMatrix > 0)
		m_currentMatrix--;
	pRenderAuxGeom->SetMatrixIndex(m_previousMatrixIndex[m_currentMatrix]);
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& DisplayContext::GetMatrix()
{
	return m_matrixStack[m_currentMatrix];
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawBall(const Vec3& pos, float radius)
{
	// TODO: pretty weird that 2D drawing should be the exception here. Investigate
	if (flags & DISPLAY_2D)
	{
		// add scale to balls accordingly to how vector would be transformed by current matrix
		radius *= m_matrixStack[m_currentMatrix].TransformVector(Vec3(1, 0, 0)).GetLength();
	}
	pRenderAuxGeom->DrawSphere(ToWS(pos), radius, m_color4b);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawArrow(const Vec3& src, const Vec3& trg, float fHeadScale)
{
	float f2dScale = 1.0f;
	float arrowLen = 0.4f * fHeadScale;
	float arrowRadius = 0.1f * fHeadScale;

	// TODO: pretty weird and crappy that 2D drawing should be the exception here. Investigate
	if (flags & DISPLAY_2D)
	{
		f2dScale = 1.2f * m_matrixStack[m_currentMatrix].TransformVector(Vec3(1, 0, 0)).GetLength();
	}
	Vec3 dir = (trg - src).GetNormalized() * arrowLen;
	Vec3 trgnew = trg - dir;
	// InternalDrawLine will transform by world matrix, so this should be calculated against original vertices
	InternalDrawLine(src, m_color4b, trgnew, m_color4b);

	Vec3 p0 = ToWS(src);
	Vec3 p1 = ToWS(trg);
	dir = m_matrixStack[m_currentMatrix].TransformVector(dir);
	p1 = p1 - dir;
	pRenderAuxGeom->DrawCone(p1, dir, arrowRadius * f2dScale, arrowLen * f2dScale, m_color4b);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::RenderObject(int objectType, const Vec3& pos, float scale, const SRenderingPassInfo& passInfo)
{
	Matrix34 tm;
	tm.SetIdentity();

	tm = Matrix33::CreateScale(Vec3(scale, scale, scale)) * tm;

	tm.SetTranslation(pos);
	RenderObject(objectType, tm, passInfo);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::RenderObject(int objectType, const Matrix34& tm, const SRenderingPassInfo& passInfo)
{
	IStatObj* object = pIconManager ? pIconManager->GetObject((EStatObject)objectType) : 0;
	if (object)
	{
		float color[4];
		color[0] = m_color4b.r * (1.0f / 255.0f);
		color[1] = m_color4b.g * (1.0f / 255.0f);
		color[2] = m_color4b.b * (1.0f / 255.0f);
		color[3] = m_color4b.a * (1.0f / 255.0f);

		Matrix34 xform = m_matrixStack[m_currentMatrix] * tm;
		SRendParams rp;
		rp.pMatrix = &xform;
		rp.AmbientColor = ColorF(color[0], color[1], color[2], 1);
		rp.fAlpha = color[3];
		rp.dwFObjFlags |= FOB_TRANS_MASK;
		//rp.nShaderTemplate = EFT_HELPER;

		object->Render(rp, passInfo);
	}
}

/////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTerrainRect(float x1, float y1, float x2, float y2, float height)
{
	Vec3 p1, p2;
	float x, y;

	float step = std::max(y2 - y1, x2 - x1);
	if (step < 0.1)
		return;
	step = step / 100.0f;
	if (step > 10)
		step /= 10;

	for (y = y1; y < y2; y += step)
	{
		float ye = min(y + step, y2);

		p1.x = x1;
		p1.y = y;
		p1.z = engine->GetTerrainElevation(p1.x, p1.y) + height;

		p2.x = x1;
		p2.y = ye;
		p2.z = engine->GetTerrainElevation(p2.x, p2.y) + height;
		DrawLine(p1, p2);

		p1.x = x2;
		p1.y = y;
		p1.z = engine->GetTerrainElevation(p1.x, p1.y) + height;

		p2.x = x2;
		p2.y = ye;
		p2.z = engine->GetTerrainElevation(p2.x, p2.y) + height;
		DrawLine(p1, p2);
	}
	for (x = x1; x < x2; x += step)
	{
		float xe = min(x + step, x2);

		p1.x = x;
		p1.y = y1;
		p1.z = engine->GetTerrainElevation(p1.x, p1.y) + height;

		p2.x = xe;
		p2.y = y1;
		p2.z = engine->GetTerrainElevation(p2.x, p2.y) + height;
		DrawLine(p1, p2);

		p1.x = x;
		p1.y = y2;
		p1.z = engine->GetTerrainElevation(p1.x, p1.y) + height;

		p2.x = xe;
		p2.y = y2;
		p2.z = engine->GetTerrainElevation(p2.x, p2.y) + height;
		DrawLine(p1, p2);
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTerrainLine(Vec3 worldPos1, Vec3 worldPos2)
{
	worldPos1.z = worldPos2.z = 0;

	int steps = int((worldPos2 - worldPos1).GetLength() / 4);
	if (steps == 0) steps = 1;

	Vec3 step = (worldPos2 - worldPos1) / float(steps);

	Vec3 p1 = worldPos1;
	p1.z = engine->GetTerrainElevation(worldPos1.x, worldPos1.y);
	for (int i = 0; i < steps; ++i)
	{
		Vec3 p2 = p1 + step;
		p2.z = 0.1f + engine->GetTerrainElevation(p2.x, p2.y);

		DrawLine(p1, p2);

		p1 = p2;
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTextLabel(const Vec3& pos, float size, const char* text, const bool bCenter, int srcOffsetX, int scrOffsetY)
{
	ColorF col(m_color4b.r * (1.0f / 255.0f), m_color4b.g * (1.0f / 255.0f), m_color4b.b * (1.0f / 255.0f), m_color4b.a * (1.0f / 255.0f));

	float fCol[4] = { col.r, col.g, col.b, col.a };
	if (flags & DISPLAY_2D)
		IRenderAuxText::Draw2dLabel(pos.x, pos.y, size, fCol, bCenter, "%s", text);
	else
		IRenderAuxText::DrawLabelEx(pos, size, fCol, true, true, text);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::Draw2dTextLabel(float x, float y, float size, const char* text, bool bCenter)
{
	float col[4] = { m_color4b.r * (1.0f / 255.0f), m_color4b.g * (1.0f / 255.0f), m_color4b.b * (1.0f / 255.0f), m_color4b.a * (1.0f / 255.0f) };
	IRenderAuxText::Draw2dLabel(x, y, size, col, bCenter, "%s", text);
}

void DisplayContext::DrawTextOn2DBox(const Vec3& pos, const char* text, float textScale, const ColorF& TextColor, const ColorF& TextBackColor)
{
	if (!m_width || !m_height || !camera)
		return;
	Vec3 worldPos = ToWorldPos(pos);
	int vx=0, vy=0, vw=m_width, vh=m_height;

	uint32 backupstate = GetState();
	int backupThickness = int(GetLineWidth());

	SetState(backupstate | e_DepthTestOff);

	const CCamera& cam = *camera;
	Vec3 screenPos;

	cam.Project(worldPos, screenPos, Vec2i(0, 0), Vec2i(0, 0));

	//! Font size information doesn't seem to exist so the proper size is used
	int textlen = strlen(text);
	float fontsize = 7.5f;
	float textwidth = fontsize * textlen;
	float textheight = 16.0f;

	screenPos.x = screenPos.x - textwidth * 0.5f;

	Vec3 textregion[4] = {
		Vec3(screenPos.x,             screenPos.y,              screenPos.z),
		Vec3(screenPos.x + textwidth, screenPos.y,              screenPos.z),
		Vec3(screenPos.x + textwidth, screenPos.y + textheight, screenPos.z),
		Vec3(screenPos.x,             screenPos.y + textheight, screenPos.z)
	};

	Vec3 textworldreign[4];
	Matrix34 dcInvTm = GetMatrix().GetInverted();

	Matrix44A mProj, mView;
	mathMatrixPerspectiveFov(&mProj, cam.GetFov(), cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
	mathMatrixLookAt(&mView, cam.GetPosition(), cam.GetPosition() + cam.GetViewdir(), Vec3(0, 0, 1));
	Matrix44A mInvViewProj = (mView * mProj).GetInverted();

	if (vw == 0)
	{
		vw = 1;
	}

	if (vh == 0)
	{
		vh = 1;
	}

	for (int i = 0; i < 4; ++i)
	{
		Vec4 projectedpos = Vec4((textregion[i].x - vx) / vw * 2.0f - 1.0f,
		                         -((textregion[i].y - vy) / vh) * 2.0f + 1.0f,
		                         textregion[i].z,
		                         1.0f);

		Vec4 wp = projectedpos * mInvViewProj;

		if (wp.w == 0.0f)
		{
			wp.w = 0.0001f;
		}

		wp.x /= wp.w;
		wp.y /= wp.w;
		wp.z /= wp.w;
		textworldreign[i] = dcInvTm.TransformPoint(Vec3(wp.x, wp.y, wp.z));
	}

	ColorB backupcolor = GetColor();

	SetColor(TextBackColor);
	SetDrawInFrontMode(true);
	DrawQuad(textworldreign[3], textworldreign[2], textworldreign[1], textworldreign[0]);
	SetColor(TextColor);
	DrawTextLabel(pos, textScale, text);
	SetDrawInFrontMode(false);
	SetColor(backupcolor);
	SetState(backupstate);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::SetLineWidth(float width)
{
	m_thickness = width;
}

//////////////////////////////////////////////////////////////////////////
bool DisplayContext::IsVisible(const AABB& bounds)
{
	if (flags & DISPLAY_2D)
	{
		if (box.IsIntersectBox(bounds))
		{
			return true;
		}
	}
	else
	{
		return camera->IsAABBVisible_F(AABB(bounds.min, bounds.max));
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
uint32 DisplayContext::GetState() const
{
	return m_renderState;
}

//! Set a new render state flags.
//! @param returns previous render state.
uint32 DisplayContext::SetState(uint32 state)
{
	uint32 old = m_renderState;
	m_renderState = state;
	pRenderAuxGeom->SetRenderFlags(m_renderState);
	return old;
}

//! Set a new render state flags.
//! @param returns previous render state.
uint32 DisplayContext::SetStateFlag(uint32 state)
{
	uint32 old = m_renderState;
	m_renderState |= state;
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags(m_renderState);
	return old;
}

//! Clear specified flags in render state.
//! @param returns previous render state.
uint32 DisplayContext::ClearStateFlag(uint32 state)
{
	uint32 old = m_renderState;
	m_renderState &= ~state;
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags(m_renderState);
	return old;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DepthTestOff()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags((m_renderState | e_DepthTestOff) & (~e_DepthTestOn));
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DepthTestOn()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags((m_renderState | e_DepthTestOn) & (~e_DepthTestOff));
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DepthWriteOff()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags((m_renderState | e_DepthWriteOff) & (~e_DepthWriteOn));
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DepthWriteOn()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags((m_renderState | e_DepthWriteOn) & (~e_DepthWriteOff));
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::CullOff()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags((m_renderState | e_CullModeNone) & (~(e_CullModeBack | e_CullModeFront)));
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::CullOn()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags((m_renderState | e_CullModeBack) & (~(e_CullModeNone | e_CullModeFront)));
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
bool DisplayContext::SetDrawInFrontMode(bool bOn)
{
	int prevState = m_renderState;
	SAuxGeomRenderFlags renderFlags = m_renderState;
	renderFlags.SetDrawInFrontMode((bOn) ? e_DrawInFrontOn : e_DrawInFrontOff);
	pRenderAuxGeom->SetRenderFlags(renderFlags);
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	return (prevState & e_DrawInFrontOn) != 0;
}

int DisplayContext::SetFillMode(int nFillMode)
{
	int prevState = m_renderState;
	SAuxGeomRenderFlags renderFlags = m_renderState;
	renderFlags.SetFillMode((EAuxGeomPublicRenderflags_FillMode)nFillMode);
	pRenderAuxGeom->SetRenderFlags(renderFlags);
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	return prevState;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTextureLabel(const Vec3& pos, int nWidth, int nHeight, int nTexId, int nTexIconFlags, int srcOffsetX, int srcOffsetY, float fDistanceScale, float distanceSquared)
{
	const float fLabelDepthPrecision = 0.05f;
	Vec3 scrpos = view->WorldToView3D(pos);

	float fWidth = (float)nWidth;
	float fHeight = (float)nHeight;

	if (distanceSquared == 0)
		distanceSquared = camera->GetPosition().GetSquaredDistance(pos);

	if (gViewportPreferences.bHideDistancedHelpers && distanceSquared > gViewportPreferences.objectHelperMaxDistSquaredDisplay)
	{
		return;
	}

	if (gViewportPreferences.distanceScaleIcons)
	{
		if (distanceSquared > gViewportPreferences.objectIconsScaleThresholdSquared)
		{
			fWidth = displayHelperSizeSmall;
			fHeight = displayHelperSizeSmall;
		}
		else
		{
			fWidth = displayHelperSizeLarge;
			fHeight = displayHelperSizeLarge;
		}
	}

	STextureLabel tl;
	tl.x = scrpos.x + srcOffsetX;
	tl.y = scrpos.y + srcOffsetY;
	if (nTexIconFlags & TEXICON_ALIGN_BOTTOM)
		tl.y -= fHeight / 2;
	else if (nTexIconFlags & TEXICON_ALIGN_TOP)
		tl.y += fHeight / 2;
	tl.z = scrpos.z - (1.0f - scrpos.z) * fLabelDepthPrecision;
	tl.w = fWidth;
	tl.h = fHeight;
	tl.nTexId = nTexId;
	tl.flags = nTexIconFlags;
	tl.color[0] = m_color4b.r * (1.0f / 255.0f);
	tl.color[1] = m_color4b.g * (1.0f / 255.0f);
	tl.color[2] = m_color4b.b * (1.0f / 255.0f);
	tl.color[3] = m_color4b.a * (1.0f / 255.0f);
	
	if (gViewportPreferences.objectIconsOnTop)
		tl.flags |= TEXICON_ON_TOP;		

	// Try to not overflood memory with labels.
	if (m_textureLabels.size() < 100000)
		m_textureLabels.push_back(tl);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::Flush2D()
{
#ifndef PHYSICS_EDITOR
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);
#endif

	if (m_textureLabels.empty())
		return;

	//renderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
	//renderer->SetCullMode( R_CULL_NONE );
	auto* pAux = gEnv->pRenderer->GetIRenderAuxGeom(/*m_eType*/);

	int nLabels = m_textureLabels.size();
	for (int i = 0; i < nLabels; i++)
	{
		STextureLabel& t = m_textureLabels[i];
		float w2 = t.w * 0.5f;
		float h2 = t.h * 0.5f;

		auto oldFlags = pAux->GetRenderFlags();;
		SAuxGeomRenderFlags flags = EAuxGeomPublicRenderflags_Defaults::e_Def2DImageRenderflags;
		flags.SetDepthTestFlag(EAuxGeomPublicRenderflags_DepthTest::e_DepthTestOn);
		
		if (t.flags & TEXICON_ADDITIVE)
		{
			flags.SetAlphaBlendMode(EAuxGeomPublicRenderflags_AlphaBlendMode::e_AlphaAdditive);
		}
		else if (t.flags & TEXICON_ON_TOP)
		{
			flags.SetDepthTestFlag(EAuxGeomPublicRenderflags_DepthTest::e_DepthTestOff);
		}

		SRender2DImageDescription img;
		img.x = t.x - w2;
		img.y = t.y + h2;
		img.z = t.z;
		img.w = t.w;
		img.h = -t.h;
		img.textureId = t.nTexId;
		img.uv[0] = Vec2(0.f,0.f);
		img.uv[1] = Vec2(1.f,1.f);
		img.color = ColorB(ColorF(t.color[0],t.color[1],t.color[2],t.color[3]));
		img.renderFlags = flags;

		pAux->PushImage(img);
	}
	//renderer->SetCullMode( R_CULL_BACK );

	m_textureLabels.clear();
}

void DisplayContext::SetDisplayContext(const SDisplayContextKey &displayContextKey, IRenderer::EViewportType eType)
{
	m_displayContextKey = displayContextKey;
	m_eType = eType;

	pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom(/*eType*/);
	pRenderAuxGeom->SetCurrentDisplayContext(displayContextKey);
}

