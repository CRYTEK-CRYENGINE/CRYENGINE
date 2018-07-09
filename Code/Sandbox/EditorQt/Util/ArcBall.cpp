// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "arcball.h"

bool CArcBall3D::ArcControl(const Matrix34& reference, const Ray& ray, uint32 mouseleft)
{
	RotControl <<= 1;

	if (mouseleft)
	{
		RotControl |= 1;
	}

	Quat WObjectRotation = Quat(reference * Matrix34(ObjectRotation));
	Matrix34 WMat = reference * Matrix34(Matrix33(DragRotation * ObjectRotation), sphere.center);
	Sphere WSphere(WMat.GetTranslation(), sphere.radius);

	Mouse_CutFlag = 0;
	Vec3 Mouse_CutOn3DSphere(0, 0, 0);
	Mouse_CutOnUnitSphere = Vec3(ZERO);
	Mouse_CutFlag = Intersect::Ray_SphereFirst(ray, WSphere, Mouse_CutOn3DSphere);

	if (Mouse_CutFlag)
	{
		Mouse_CutOnUnitSphere = WObjectRotation.GetInverted() * (Mouse_CutOn3DSphere - WSphere.center).GetNormalized();
	}

	if (RotControl & 3)
	{
		if (Mouse_CutFlag)
		{
			if ((RotControl & 3) == 0x01)
			{
				Mouse_CutFlagStart = 1;
				LineStart3D = Mouse_CutOnUnitSphere;
				AxisSnap = 0;

				Matrix33 bym33;

				//get the distance to the axis
				f32 xdist = fabsf(Mouse_CutOnUnitSphere.x);
				f32 ydist = fabsf(Mouse_CutOnUnitSphere.y);
				f32 zdist = fabsf(Mouse_CutOnUnitSphere.z);

				//if to close to an axis-crossing, disable axis choosing
				if ((xdist < CrossDist) && (zdist < CrossDist)) { xdist = 1.0f; ydist = 1.0f; zdist = 1.0f; }
				if ((xdist < CrossDist) && (ydist < CrossDist)) { xdist = 1.0f; ydist = 1.0f; zdist = 1.0f; }
				if ((ydist < CrossDist) && (zdist < CrossDist)) { xdist = 1.0f; ydist = 1.0f; zdist = 1.0f; }

				//check snap with YZ-plane
				if (xdist < AxisDist)
				{
					bym33.SetIdentity();

					if ((LineStart3D.x) || (LineStart3D.z))
					{
						Vec3 n = Vec3(LineStart3D.x, 0, LineStart3D.z).GetNormalized();
						bym33.SetRotationY(acos_tpl(fabsf(n.z)));
					}

					Vec3 SnapLineStart3D = bym33 * Vec3(fabsf(LineStart3D.x), LineStart3D.y, -fabsf(LineStart3D.z));

					if (LineStart3D.z > 0.0f)
						SnapLineStart3D.z = -SnapLineStart3D.z;

					LineStart3D = SnapLineStart3D;
					AxisSnap = 1;
				}

				//check snap with XZ-plane
				if (ydist < AxisDist)
				{
					bym33.SetIdentity();

					if ((LineStart3D.y) || (LineStart3D.z))
					{
						Vec3 bn_xz = Vec3(0, LineStart3D.y, LineStart3D.z).GetNormalized();
						bym33.SetRotationX(-acos_tpl(fabsf(bn_xz.z)));
					}

					Vec3 SnapLineStart3D = bym33 * Vec3((LineStart3D.x), fabsf(LineStart3D.y), -fabsf(LineStart3D.z));

					if (LineStart3D.z > 0.0f)
						SnapLineStart3D.z = -SnapLineStart3D.z;

					LineStart3D = SnapLineStart3D;
					AxisSnap = 2;
				}

				//check snap with XY-plane
				if (zdist < AxisDist)
				{
					bym33.SetIdentity();

					if ((LineStart3D.x) || (LineStart3D.z))
					{
						Vec3 bn_xz = Vec3(LineStart3D.x, 0, LineStart3D.z).GetNormalized();
						bym33.SetRotationY(-acos_tpl(fabsf(bn_xz.x)));
					}

					Vec3 SnapLineStart3D = bym33 * Vec3(fabsf(LineStart3D.x), LineStart3D.y, -fabsf(LineStart3D.z));

					if (LineStart3D.x < 0.0f)
						SnapLineStart3D.x = -SnapLineStart3D.x;

					LineStart3D = SnapLineStart3D;
					AxisSnap = 3;
				}
			}

			if ((RotControl & 3) == 0x03)
				ArcRotation();

			if ((RotControl & 3) == 0x02)
			{
				ObjectRotation = (DragRotation * ObjectRotation).GetNormalized();
				DragRotation.SetIdentity();
				Mouse_CutFlagStart = 0;
				LineStart3D = Vec3(0, -1, 0);
				AxisSnap = 0;

				return true;
			}
		}
		else
		{
			Vec3 ClostestPointOnLine;
			IntersectSphereLineSegment(WSphere, ray.origin, ray.origin + ray.direction * 1000.0f, ClostestPointOnLine);

			Mouse_CutOn3DSphere = ((ClostestPointOnLine - WSphere.center).GetNormalized() * WSphere.radius) + WSphere.center;
			Mouse_CutOnUnitSphere = WObjectRotation.GetInverted() * (Mouse_CutOn3DSphere - WSphere.center).GetNormalized();

			if ((RotControl & 3) == 0x01)
			{
				LineStart3D = Mouse_CutOnUnitSphere;
				Mouse_CutFlagStart = 0;
				AxisSnap = 0;
			}

			if ((RotControl & 3) == 0x03)
				ArcRotation();

			if ((RotControl & 3) == 0x02)
			{
				ObjectRotation = (DragRotation * ObjectRotation).GetNormalized();
				DragRotation.SetIdentity();
				Mouse_CutFlagStart = 0;
				LineStart3D = Vec3(0, -1, 0);
				AxisSnap = 0;

				return true;
			}
		}
	}

	return false;
}

void CArcBall3D::ArcRotation()
{
	Vec3 rv;
	f32 gradius = 0;
	f32 distance_YZ = 0;
	f32 bias = 0;
	f32 cosine = 0;
	Vec3 XYVector;

	DragRotation.SetIdentity();

	//first we calculate an ordinary drag-quaternion
	cosine = (LineStart3D | Mouse_CutOnUnitSphere);

	if (fabsf(cosine) < 0.99999f)
		DragRotation.SetRotationAA(acos_tpl(cosine), (LineStart3D % Mouse_CutOnUnitSphere).GetNormalized());

	if (AxisSnap == 1)
	{
		//m_ButtonArcRotate an UpVector with our drag-quaternion
		rv = (DragRotation) * Vec3(0, -1, 0);
		DragRotation.SetIdentity();

		//project rotated UpVector into XY_Plane (this is a simple y_axis rotation)
		Matrix33 ym33;
		ym33.SetIdentity();

		if ((rv.x) || (rv.z))
		{
			Vec3 n_xz = Vec3(rv.x, 0, rv.z).GetNormalized();
			ym33.SetRotationY(-acos_tpl(fabsf(n_xz.z)));
		}

		XYVector = ym33 * Vec3(-fabsf(rv.x), rv.y, -fabsf(rv.z));

		//find the rotation direction around z-axis
		if (rv.z > 0)
		{
			XYVector.z = -XYVector.z;
		}

		//calculate the xy-constrained quaternion
		cosine = (Vec3(0, -1, 0) | XYVector);

		if (fabsf(cosine) < 0.99999f)
		{
			gradius = Vec3(rv.x, 0, rv.z).GetLength();
			distance_YZ = fabsf(rv.z);
			bias = distance_YZ / gradius;
			DragRotation.SetRotationAA(acos_tpl(cosine) * bias, (Vec3(0, -1, 0) % XYVector).GetNormalized());
		}
	}

	if (AxisSnap == 2)
	{
		//m_ButtonArcRotate an UpVector with our DRAG-QUATERNION
		rv = DragRotation * Vec3(-1, 0, 0);
		DragRotation.SetIdentity();

		//project rotated UpVector into XY_Plane (this is a simple x_axis rotation)
		Matrix33 ym33;
		ym33.SetIdentity();

		if ((rv.y) || (rv.z))
		{
			Vec3 n_xz = Vec3(0, rv.y, rv.z).GetNormalized();
			ym33.SetRotationX(-acos_tpl(fabsf(n_xz.z)));
		}

		XYVector = ym33 * Vec3((rv.x), fabsf(rv.y), -fabsf(rv.z));

		//find the rotation direction around y-axis
		if (rv.z > 0)
		{
			XYVector.z = -XYVector.z;
		}

		//calculate the xz-constrained quaternion
		cosine = (Vec3(-1, 0, 0) | XYVector);

		if (fabsf(cosine) < 0.99999f)
		{
			gradius = Vec3(0, rv.y, rv.z).GetLength();
			distance_YZ = fabsf(rv.z);
			bias = distance_YZ / gradius;
			DragRotation.SetRotationAA(acos_tpl(cosine) * bias, (Vec3(-1, 0, 0) % XYVector).GetNormalized());
		}
	}

	if (AxisSnap == 3)
	{
		//m_ButtonArcRotate an UpVector with our DRAG-QUATERNION
		rv = DragRotation * Vec3(0, -1, 0);
		DragRotation.SetIdentity();

		//project rotated UpVector into XY_Plane (this is a simple y_axis rotation)
		Matrix33 ym33;

		ym33.SetIdentity();

		if ((rv.x) || (rv.z))
		{
			Vec3 n_xz = Vec3(rv.x, 0, rv.z).GetNormalized();
			ym33.SetRotationY(-acos_tpl(fabsf(n_xz.x)));
		}

		XYVector = ym33 * Vec3(fabsf(rv.x), rv.y, -fabsf(rv.z));

		//find the rotation direction around z-axis
		if (rv.x < 0)
		{
			XYVector.x = -XYVector.x;
		}

		//calculate the xy-constrained quaternion
		cosine = (Vec3(0, -1, 0) | XYVector);

		if (fabsf(cosine) < 0.99999f)
		{
			gradius = Vec3(rv.x, 0, rv.z).GetLength();
			distance_YZ = fabsf(rv.x);
			bias = distance_YZ / gradius;
			DragRotation.SetRotationAA(acos_tpl(cosine) * bias, (Vec3(0, -1, 0) % XYVector).GetNormalized());
		}
	}

	//BINGO!!!!  the final drag quaternion
	DragRotation = ObjectRotation * DragRotation * ObjectRotation.GetInverted();
}

uint32 CArcBall3D::IntersectSphereLineSegment(const Sphere& sphere, const Vec3& LineStart, const Vec3& LineEnd, Vec3& I)
{
	//this is the code to produce a real z-rotation!
	Vec3 LineDir = (LineEnd - LineStart).GetNormalized();
	Vec3 ShereCenterDir = (sphere.center - LineStart).GetNormalized();
	f32 LengthToSphereCenter = (sphere.center - LineStart).GetLength();

	f32 cosine = (ShereCenterDir | LineDir);

	//this vector is perpendicular to the vector "ShereCenterDir"
	Vec3 PerpVector = LengthToSphereCenter / cosine * LineDir + LineStart;
	Vec3 PerpVectorOnSphere = ((PerpVector - sphere.center).GetNormalized() * sphere.radius) + sphere.center;
	I = PerpVectorOnSphere;

	{
		//find closest point on Lineseg
		Vec3 LineDir = (LineEnd - LineStart).GetNormalized();
		f32 proj = LineDir | (sphere.center - LineStart);
		I = LineDir * proj + LineStart;
	}

	return 0;
}

void CArcBall3D::DrawSphere(const Matrix34& reference, const CCamera& cam, IRenderAuxGeom* pRenderer)
{
	f32 thicknessX = 1.0f;
	f32 thicknessY = 1.0f;
	f32 thicknessZ = 1.0f;

	uint32 start;
	Vec3 Vertices3D[64 * 32];
	Vec3 sVertices3D[64 * 32];
	Vec3 tVertices3D[64 * 32];
	uint32 c;

	Matrix34 WMat = reference * Matrix34(Matrix33(DragRotation * ObjectRotation), sphere.center);
	Quat WObjectRotation = Quat(reference * Matrix34(ObjectRotation));
	Quat WRotation = Quat(reference * Matrix34(DragRotation * ObjectRotation));
	Sphere WSphere(WMat.GetTranslation(), sphere.radius);

	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);
	renderFlags.SetFillMode(e_FillModeSolid);

	//------------------------------------------------------------------------------------------------------

	uint32 s = 0;
	uint32 t = 0;
	Vec3 CamPos = cam.GetPosition();

	renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	pRenderer->SetRenderFlags(renderFlags);
	pRenderer->DrawSphere(WSphere.center, WSphere.radius, RGBA8(0x3f, 0x3f, 0x3f, 0x00));

	ColorB col;
	f32 xdist = fabsf(Mouse_CutOnUnitSphere.x);
	f32 ydist = fabsf(Mouse_CutOnUnitSphere.y);
	f32 zdist = fabsf(Mouse_CutOnUnitSphere.z);

	if ((xdist < CrossDist) && (zdist < CrossDist)) { xdist = 1.0f; ydist = 1.0f; zdist = 1.0f; }
	if ((xdist < CrossDist) && (ydist < CrossDist)) { xdist = 1.0f; ydist = 1.0f; zdist = 1.0f; }
	if ((ydist < CrossDist) && (zdist < CrossDist)) { xdist = 1.0f; ydist = 1.0f; zdist = 1.0f; }

	//----------------------------------------------------------------------------------
	// draw circle around X-axis
	//----------------------------------------------------------------------------------
	thicknessX = 1.0f;

	if (AxisSnap == 0)
	{
		if (Mouse_CutFlag)
		{
			if (xdist < AxisDist)
				thicknessX = 5.0f;
		}
	}
	else if (AxisSnap == 1)
		thicknessX = 5.0f;

	c = 0;

	for (f32 cz = 0; cz < (gf_PI * 2); cz = cz + (2 * gf_PI / 256.0f), ++c)
	{
		Vertices3D[c] = WMat * (Vec3(0, -cosf(cz), sinf(cz)) * WSphere.radius);
	}

	assert(c == 0x100);

	for (start = 0; start < c; ++start)
	{
		Vec3 p0 = Vertices3D[(start + 0) & 0xff];
		f32 dot0 = (p0 - CamPos) | (p0 - WSphere.center);
		Vec3 p1 = Vertices3D[(start + 1) & 0xff];
		f32 dot1 = (p1 - CamPos) | (p1 - WSphere.center);

		if ((dot0 < 0) && !(dot1 < 0))
			break;
	}

	s = 0;
	t = 0;
	start = (start + 1) & 0xff;

	for (uint32 i = 0; i < c; ++i)
	{
		Vec3 p = Vertices3D[start];
		f32 dot = (p - CamPos) | (p - WSphere.center);

		if (dot < 0)
		{
			sVertices3D[s] = p;
			s++;
		}
		else
		{
			tVertices3D[t] = p;
			t++;
		}

		start = (start + 1) & 0xff;
	}

	renderFlags.SetAlphaBlendMode(e_AlphaNone);
	pRenderer->SetRenderFlags(renderFlags);

	if (s > 2)
		pRenderer->DrawPolyline(sVertices3D, s, 0, RGBA8(0xff, 0x12, 0x12, 0x00), thicknessX);

	renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	pRenderer->SetRenderFlags(renderFlags);

	if (t > 2)
		pRenderer->DrawPolyline(tVertices3D, t, 0, RGBA8(0x1f, 0x07, 0x07, 0x00), thicknessX);

	//----------------------------------------------------------------------------------
	// draw circle around Y-axis
	//----------------------------------------------------------------------------------
	thicknessY = 1.0f;

	if (AxisSnap == 0)
	{
		if (Mouse_CutFlag)
		{
			if (ydist < AxisDist)
				thicknessY = 5.0f;
		}
	}
	else if (AxisSnap == 2)
	{
		thicknessY = 5.0f;
	}

	c = 0;

	for (f32 cz = 0; cz < (gf_PI * 2); cz = cz + (2 * gf_PI / 256.0f), ++c)
	{
		Vertices3D[c] = WMat * (Vec3(-cosf(cz), 0, sinf(cz)) * WSphere.radius);
	}

	assert(c == 0x100);

	for (start = 0; start < c; ++start)
	{
		Vec3 p0 = Vertices3D[(start + 0) & 0xff];
		f32 dot0 = (p0 - CamPos) | (p0 - WSphere.center);
		Vec3 p1 = Vertices3D[(start + 1) & 0xff];
		f32 dot1 = (p1 - CamPos) | (p1 - WSphere.center);

		if ((dot0 < 0) && !(dot1 < 0))
			break;
	}

	s = 0;
	t = 0;
	start = (start + 1) & 0xff;

	for (uint32 i = 0; i < c; ++i)
	{
		Vec3 p = Vertices3D[start];
		f32 dot = (p - CamPos) | (p - WSphere.center);
		if (dot < 0)
		{
			sVertices3D[s] = p;
			s++;
		}
		else
		{
			tVertices3D[t] = p;
			t++;
		}

		start = (start + 1) & 0xff;
	}

	renderFlags.SetAlphaBlendMode(e_AlphaNone);
	pRenderer->SetRenderFlags(renderFlags);

	if (s > 2)
		pRenderer->DrawPolyline(sVertices3D, s, 0, RGBA8(0x12, 0xff, 0x12, 0x00), thicknessY);

	renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	pRenderer->SetRenderFlags(renderFlags);

	if (t > 2)
		pRenderer->DrawPolyline(tVertices3D, t, 0, RGBA8(0x07, 0x1f, 0x07, 0x00), thicknessY);

	//----------------------------------------------------------------------------------
	// draw circle around Z-axis
	//----------------------------------------------------------------------------------
	thicknessZ = 1.0f;

	if (AxisSnap == 0)
	{
		if (Mouse_CutFlag)
		{
			if (zdist < AxisDist)
				thicknessZ = 5.0f;
		}
	}
	else if (AxisSnap == 3)
		thicknessZ = 5.0f;

	c = 0;
	for (f32 cz = 0; cz < (gf_PI * 2); cz = cz + (2 * gf_PI / 256.0f), ++c)
	{
		Vertices3D[c] = WMat * (Vec3(sinf(cz), -cosf(cz), 0) * WSphere.radius);
	}

	assert(c == 0x100);

	for (start = 0; start < c; ++start)
	{
		Vec3 p0 = Vertices3D[(start + 0) & 0xff];
		f32 dot0 = (p0 - CamPos) | (p0 - WSphere.center);
		Vec3 p1 = Vertices3D[(start + 1) & 0xff];
		f32 dot1 = (p1 - CamPos) | (p1 - WSphere.center);

		if ((dot0 < 0) && !(dot1 < 0))
			break;
	}

	s = 0;
	t = 0;
	start = (start + 1) & 0xff;

	for (uint32 i = 0; i < c; ++i)
	{
		Vec3 p = Vertices3D[start];
		f32 dot = (p - CamPos) | (p - WSphere.center);

		if (dot < 0)
		{
			sVertices3D[s] = p;
			s++;
		}
		else
		{
			tVertices3D[t] = p;
			t++;
		}

		start = (start + 1) & 0xff;
	}

	renderFlags.SetAlphaBlendMode(e_AlphaNone);
	pRenderer->SetRenderFlags(renderFlags);

	if (s > 2)
		pRenderer->DrawPolyline(sVertices3D, s, 0, RGBA8(0x12, 0x12, 0xff, 0x00), thicknessZ);

	renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	pRenderer->SetRenderFlags(renderFlags);

	if (t > 2)
	{
		pRenderer->DrawPolyline(tVertices3D, t, 0, RGBA8(0x07, 0x07, 0x1f, 0x00), thicknessZ);
	}

	uint32 v;

	Vec3 VBuffer[1000];
	ColorB CBuffer[1000];

	if ((RotControl & 3) == 3)
	{
		Vec3 Blue = WObjectRotation * LineStart3D;
		Vec3 Red = WObjectRotation * Mouse_CutOnUnitSphere;

		VBuffer[0] = Vec3(0, 0, 0);
		CBuffer[0] = RGBA8(0x00, 0x00, 0x00, 0x00);

		VBuffer[1] = Blue;
		CBuffer[1] = RGBA8(0x00, 0x00, 0xff, 0x00);

		VBuffer[102] = Red;
		CBuffer[102] = RGBA8(0xff, 0x00, 0x00, 0x00);

		for (v = 0; v < 100; ++v)
		{
			f32 t0 = (1.0f / 101.0f * v) + 1.0f / 101.0f;

			VBuffer[v + 2] = Vec3::CreateSlerp(Blue, Red, t0);

			f32 t1 = (1.0f / 101.0f * v);

			CBuffer[v + 2].b = uint8((1.0f - t1) * CBuffer[1].b + t1 * CBuffer[102].b);
			CBuffer[v + 2].g = uint8((1.0f - t1) * CBuffer[1].g + t1 * CBuffer[102].g);
			CBuffer[v + 2].r = uint8((1.0f - t1) * CBuffer[1].r + t1 * CBuffer[102].r);
		}

		for (v = 0; v < 103; ++v)
		{
			VBuffer[v] = VBuffer[v] * WSphere.radius + WSphere.center;
		}

		if (AxisSnap == 0)
		{
			SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);

			renderFlags.SetFillMode(e_FillModeSolid);
			renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
			pRenderer->SetRenderFlags(renderFlags);

			for (v = 0; v < 100; ++v)
			{
				pRenderer->DrawTriangle(VBuffer[0], CBuffer[0], VBuffer[v + 1], CBuffer[v + 1], VBuffer[v + 2], CBuffer[v + 2]);
				pRenderer->DrawTriangle(VBuffer[0], CBuffer[0], VBuffer[v + 2], CBuffer[v + 2], VBuffer[v + 1], CBuffer[v + 1]);
			}
		}
	}

	if (AxisSnap)
	{
		//project vector into the xy-plane
		VBuffer[0] = Vec3(0, 0, 0);
		CBuffer[0] = RGBA8(0x00, 0x00, 0x00, 0x00);

		VBuffer[1] = WObjectRotation * LineStart3D;
		CBuffer[1] = RGBA8(0x12, 0x1f, 0x12, 0x00);

		VBuffer[102] = WRotation * LineStart3D;
		CBuffer[102] = RGBA8(0x22, 0x7f, 0x22, 0x00);

		ColorB c0 = CBuffer[1];
		ColorB c1 = CBuffer[102];

		for (v = 0; v < 100; ++v)
		{
			f32 t0 = (1.0f / 101.0f * v) + 1.0f / 101.0f;

			VBuffer[v + 2] = Vec3::CreateSlerp(VBuffer[1], VBuffer[102], t0);

			f32 t1 = (1.0f / 101.0f * v);
			CBuffer[v + 2].r = uint8((1.0f - t1) * c0.r + t1 * c1.r);
			CBuffer[v + 2].g = uint8((1.0f - t1) * c0.g + t1 * c1.g);
			CBuffer[v + 2].b = uint8((1.0f - t1) * c0.b + t1 * c1.b);
		}

		for (v = 0; v < 103; ++v)
		{
			VBuffer[v] = VBuffer[v] * WSphere.radius + WSphere.center;
		}

		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		renderFlags.SetFillMode(e_FillModeSolid);
		renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
		pRenderer->SetRenderFlags(renderFlags);

		for (v = 0; v < 100; ++v)
		{
			pRenderer->DrawTriangle(VBuffer[0], CBuffer[0], VBuffer[v + 1], CBuffer[v + 1], VBuffer[v + 2], CBuffer[v + 2]);
			pRenderer->DrawTriangle(VBuffer[0], CBuffer[0], VBuffer[v + 2], CBuffer[v + 2], VBuffer[v + 1], CBuffer[v + 1]);
		}
	}

	renderFlags = e_Def3DPublicRenderflags;
	renderFlags.SetFillMode(e_FillModeSolid);
	renderFlags.SetAlphaBlendMode(e_AlphaNone);
	pRenderer->SetRenderFlags(renderFlags);

#define CROSS (0.25f)

	Vec3 rmin = WMat * Vec3(0, 0.0f, 0.0f);
	Vec3 rmax = WMat * Vec3(CROSS, 0.0f, 0.0f);

	pRenderer->DrawLine(rmin, RGBA8(0xff, 0x00, 0x00, 0x00), rmax, RGBA8(0xff, 0x7f, 0x7f, 0x00), thicknessX);

	Vec3 gmin = WMat * Vec3(0.0f, 0, 0.0f);
	Vec3 gmax = WMat * Vec3(0.0f, CROSS, 0.0f);

	pRenderer->DrawLine(gmin, RGBA8(0x00, 0xff, 0x00, 0x00), gmax, RGBA8(0x7f, 0xff, 0x7f, 0x00), thicknessY);

	Vec3 bmin = WMat * Vec3(0.0f, 0.0f, 0);
	Vec3 bmax = WMat * Vec3(0.0f, 0.0f, CROSS);

	pRenderer->DrawLine(bmin, RGBA8(0x00, 0x00, 0xff, 0x00), bmax, RGBA8(0x7f, 0x7f, 0xff, 0x00), thicknessZ);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOn);
	pRenderer->SetRenderFlags(renderFlags);
}

