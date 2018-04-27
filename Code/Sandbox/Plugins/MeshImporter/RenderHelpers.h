// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryAnimation/ICryAnimation.h>
#include <CryRenderer/IRenderAuxGeom.h>

// ==================================================
// FROM CT
// ==================================================

namespace Private_RenderHelpers
{

static const char* strstri(const char* pString, const char* pSubstring)
{
	int i, j, k;
	for (i = 0; pString[i]; i++)
		for (j = i, k = 0; tolower(pString[j]) == tolower(pSubstring[k]); j++, k++)
			if (!pSubstring[k + 1])
				return (pString + i);

	return NULL;
}

} // namespace Private_RenderHelpers

static ILINE ColorB shader(Vec3 n, Vec3 d0, Vec3 d1, ColorB c)
{
	f32 a = 0.5f * n | d0, b = 0.1f * n | d1, l = min(1.0f, fabs_tpl(a) - a + fabs_tpl(b) - b + 0.05f);
	return RGBA8(uint8(l * c.r), uint8(l * c.g), uint8(l * c.b), c.a);
}

static void DrawDirectedConnection(IRenderAuxGeom* pAuxGeom, const Vec3& start, const Vec3& end, const uint8 opacity = 255)
{
	const ColorB colStart(255, 255, 0, opacity);//255,255
	const ColorB colEnd(150, 100, 0, opacity);//150,100

	Vec3 vBoneVec = end - start;
	float fBoneLength = vBoneVec.GetLength();
	const f32 t = fBoneLength * 0.03f;

	if (fBoneLength < 1e-4)
		return;

	QuatT qt(Quat::CreateRotationV0V1(Vec3(1, 0, 0), vBoneVec / fBoneLength), start);

	//bone points in x-direction
	Vec3 left = qt * Vec3(t, 0, +t);
	Vec3 right = qt * Vec3(t, 0, -t);

	pAuxGeom->DrawLine(start, colStart, left, colStart);
	pAuxGeom->DrawLine(start, colStart, right, colStart);
	pAuxGeom->DrawLine(end, colEnd, left, colStart);
	pAuxGeom->DrawLine(end, colEnd, right, colStart);
}

static void DrawDirectedConnection(IRenderAuxGeom* pAuxGeom, const Vec3& p, const Vec3& c, ColorB clr, const Vec3& vdir)
{
	const f32 fAxisLen = (p - c).GetLength();
	if (fAxisLen < 0.001f)
		return;

	const Vec3 vAxisNormalized = (p - c).GetNormalized();
	QuatTS qt(Quat::CreateRotationV0V1(Vec3(1, 0, 0), vAxisNormalized), p, 1.0f);

	ColorB clr2 = clr;

	f32 cr = 0.01f + fAxisLen / 50.0f;
	uint32 subdiv = 4 * 4;
	f32 rad = gf_PI * 2.0f / f32(subdiv);
	const float cos15 = cos_tpl(rad), sin15 = sin_tpl(rad);

	Vec3 pt0, pt1, pt2, pt3;
	ColorB pclrlit0, pclrlit1, pclrlit2, pclrlit3;
	ColorB nclrlit0, nclrlit1, nclrlit2, nclrlit3;

	Matrix34 m34 = Matrix34(qt);
	Matrix33 m33 = Matrix33(m34);

	//	Matrix34 t34p; t34p.SetTranslationMat(Vec3( 0,0,0));

	Matrix34 t34m;
	t34m.SetTranslationMat(Vec3(-fAxisLen, 0, 0));
	Matrix34 m34p = m34;
	Matrix34 m34m = m34 * t34m;
	Matrix34 r34m = m34 * t34m;
	r34m.m00 = -r34m.m00;
	r34m.m10 = -r34m.m10;
	r34m.m20 = -r34m.m20;
	Matrix33 r33 = Matrix33(r34m);

	Vec3 ldir0 = vdir;
	ldir0.z = 0;
	(ldir0 = ldir0.normalized() * 0.5f).z = f32(sqrt3 * -0.5f);
	Vec3 ldir1 = Vec3(0, 0, 1);

	f32 x = cos15, y = sin15;
	pt0 = Vec3(0, 0, cr);
	pclrlit0 = shader(m33 * Vec3(0, 0, 1), ldir0, ldir1, clr2);
	f32 s = 0.20f;
	Vec3 _p[0x4000];
	ColorB _c[0x4000];
	uint32 m_dwNumFaceVertices = 0;
	// child part
	for (uint32 i = 0; i < subdiv; i++)
	{
		pt1 = Vec3(0, -y * cr, x * cr);
		pclrlit1 = shader(m33 * Vec3(0, -y, x), ldir0, ldir1, clr2);
		Vec3 x0 = m34m * Vec3(pt0.x * s, pt0.y * s, pt0.z * s);
		Vec3 x1 = m34p * pt0;
		Vec3 x3 = m34m * Vec3(pt1.x * s, pt1.y * s, pt1.z * s);
		Vec3 x2 = m34p * pt1;
		_p[m_dwNumFaceVertices] = x1;
		_c[m_dwNumFaceVertices++] = pclrlit0;
		_p[m_dwNumFaceVertices] = x2;
		_c[m_dwNumFaceVertices++] = pclrlit1;
		_p[m_dwNumFaceVertices] = x3;
		_c[m_dwNumFaceVertices++] = pclrlit1;

		_p[m_dwNumFaceVertices] = x3;
		_c[m_dwNumFaceVertices++] = pclrlit1;
		_p[m_dwNumFaceVertices] = x0;
		_c[m_dwNumFaceVertices++] = pclrlit0;
		_p[m_dwNumFaceVertices] = x1;
		_c[m_dwNumFaceVertices++] = pclrlit0;
		f32 t = x;
		x = x * cos15 - y * sin15;
		y = y * cos15 + t * sin15;
		pt0 = pt1, pclrlit0 = pclrlit1;
	}

	// parent part
	f32 cost = 1, sint = 0, costup = cos15, sintup = sin15;
	for (uint32 j = 0; j < subdiv / 4; j++)
	{
		Vec3 n = Vec3(sint, 0, cost);
		pt0 = n * cr;
		pclrlit0 = shader(m33 * n, ldir0, ldir1, clr);
		nclrlit0 = shader(r33 * n, ldir0, ldir1, clr);
		n = Vec3(sintup, 0, costup);
		pt2 = n * cr;
		pclrlit2 = shader(m33 * n, ldir0, ldir1, clr);
		nclrlit2 = shader(r33 * n, ldir0, ldir1, clr);

		x = cos15, y = sin15;
		for (uint32 i = 0; i < subdiv; i++)
		{
			n = Vec3(0, -y, x) * costup, n.x += sintup;
			pt3 = n * cr;
			pclrlit3 = shader(m33 * n, ldir0, ldir1, clr);
			nclrlit3 = shader(r33 * n, ldir0, ldir1, clr);
			n = Vec3(0, -y, x) * cost, n.x += sint;
			pt1 = n * cr;
			pclrlit1 = shader(m33 * n, ldir0, ldir1, clr);
			nclrlit1 = shader(r33 * n, ldir0, ldir1, clr);

			Vec3 v0 = m34p * pt0;
			Vec3 v1 = m34p * pt1;
			Vec3 v2 = m34p * pt2;
			Vec3 v3 = m34p * pt3;
			_p[m_dwNumFaceVertices] = v1;
			_c[m_dwNumFaceVertices++] = pclrlit1;
			_p[m_dwNumFaceVertices] = v0;
			_c[m_dwNumFaceVertices++] = pclrlit0;
			_p[m_dwNumFaceVertices] = v2;
			_c[m_dwNumFaceVertices++] = pclrlit2;
			_p[m_dwNumFaceVertices] = v3;
			_c[m_dwNumFaceVertices++] = pclrlit3;
			_p[m_dwNumFaceVertices] = v1;
			_c[m_dwNumFaceVertices++] = pclrlit1;
			_p[m_dwNumFaceVertices] = v2;
			_c[m_dwNumFaceVertices++] = pclrlit2;

			Vec3 w0 = r34m * (pt0 * s);
			Vec3 w1 = r34m * (pt1 * s);
			Vec3 w3 = r34m * (pt3 * s);
			Vec3 w2 = r34m * (pt2 * s);
			_p[m_dwNumFaceVertices] = w0;
			_c[m_dwNumFaceVertices++] = pclrlit0;
			_p[m_dwNumFaceVertices] = w1;
			_c[m_dwNumFaceVertices++] = pclrlit1;
			_p[m_dwNumFaceVertices] = w2;
			_c[m_dwNumFaceVertices++] = pclrlit2;
			_p[m_dwNumFaceVertices] = w1;
			_c[m_dwNumFaceVertices++] = pclrlit1;
			_p[m_dwNumFaceVertices] = w3;
			_c[m_dwNumFaceVertices++] = pclrlit3;
			_p[m_dwNumFaceVertices] = w2;
			_c[m_dwNumFaceVertices++] = pclrlit2;
			f32 t = x;
			x = x * cos15 - y * sin15;
			y = y * cos15 + t * sin15;
			pt0 = pt1, pt2 = pt3;
			pclrlit0 = pclrlit1, pclrlit2 = pclrlit3;
			nclrlit0 = nclrlit1, nclrlit2 = nclrlit3;
		}
		cost = costup;
		sint = sintup;
		costup = cost * cos15 - sint * sin15;
		sintup = sint * cos15 + cost * sin15;
	}

	pAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Mode3D | e_DrawInFrontOff | e_FillModeSolid | e_CullModeFront | e_DepthWriteOn | e_DepthTestOn));
	pAuxGeom->DrawTriangles(&_p[0], m_dwNumFaceVertices, &_c[0]);
	pAuxGeom->DrawLine(p, clr, c, clr);
}

static void DrawFrame(IRenderAuxGeom* pAuxGeom, const QuatT& location, const float scale = 0.02f, const uint8 opacity = 255)
{
	pAuxGeom->DrawLine(location.t, RGBA8(0xbf, 0x00, 0x00, opacity), location.t + location.GetColumn0() * scale, RGBA8(0xff, 0x00, 0x00, opacity));
	pAuxGeom->DrawLine(location.t, RGBA8(0x00, 0xbf, 0x00, opacity), location.t + location.GetColumn1() * scale, RGBA8(0x00, 0xff, 0x00, opacity));
	pAuxGeom->DrawLine(location.t, RGBA8(0x00, 0x00, 0xbf, opacity), location.t + location.GetColumn2() * scale, RGBA8(0x00, 0x00, 0xff, opacity));
}

static void DrawSkeleton(IRenderAuxGeom* pAuxGeom, IDefaultSkeleton* pDefaultSkeleton, ISkeletonPose* pPose, const QuatT& location, string filter, QuatT cameraFrame)
{
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags | e_DepthTestOff);

	uint32 jointCount = pDefaultSkeleton->GetJointCount();
	bool filtered = !filter.empty();

	for (uint32 i = 0; i < jointCount; ++i)
	{
		const uint8 opacity = (filtered && Private_RenderHelpers::strstri(pDefaultSkeleton->GetJointNameByID(i), filter) == 0) ? 20 : 255;

		const int32 parentIndex = pDefaultSkeleton->GetJointParentIDByID(i);
		const QuatT jointFrame = location * pPose->GetAbsJointByID(i);
		const Diag33 jointScale = pPose->GetAbsJointScalingByID(i);

		if (parentIndex > -1)
		{
			DrawDirectedConnection(pAuxGeom, location * pPose->GetAbsJointByID(parentIndex).t, jointFrame.t, opacity);
		}

		DrawFrame(pAuxGeom, jointFrame, 0.02f * jointScale.x, opacity);
	}
}

// ==================================================
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// FROM CT
// ==================================================

