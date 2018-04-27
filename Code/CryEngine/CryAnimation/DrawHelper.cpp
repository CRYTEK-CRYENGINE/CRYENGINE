// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DrawHelper.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "CharacterManager.h"
#include "Model.h"

namespace
{

ILINE Matrix33 ComputeOrientationAlongDirectionMatrix(
  const uint forwardIndex, const Vec3& forwardVector,
  const uint upIndex, const Vec3& upVector)
{
	assert(forwardIndex < 3);
	assert(upIndex < 3);
	assert(forwardIndex != upIndex);
	const uint sideIndex = 3 - (forwardIndex + upIndex);

	Vec3 m[3];

	m[forwardIndex] = forwardVector.GetNormalized();
	m[upIndex] = upVector.GetNormalized();

	if (abs(m[forwardIndex].Dot(m[upIndex])) > 0.99f)
		m[upIndex] = m[forwardIndex].GetOrthogonal();

	m[sideIndex] = m[forwardIndex].Cross(m[upIndex]);
	m[sideIndex].Normalize();
	m[upIndex] = m[forwardIndex].Cross(m[sideIndex]);

	return Matrix33(m[0], m[1], m[2]);
}

} // namespace

namespace DrawHelper {

void DepthTest(bool bTest)
{
	SAuxGeomRenderFlags flags = g_pAuxGeom->GetRenderFlags();
	flags.SetDepthTestFlag(bTest ? e_DepthTestOn : e_DepthTestOff);
	g_pAuxGeom->SetRenderFlags(flags);
}

void DepthWrite(bool bWrite)
{
	SAuxGeomRenderFlags flags = g_pAuxGeom->GetRenderFlags();
	flags.SetDepthWriteFlag(bWrite ? e_DepthWriteOn : e_DepthWriteOff);
	g_pAuxGeom->SetRenderFlags(flags);
}

void Draw(const Segment& segment)
{
	g_pAuxGeom->DrawLines(segment.positions, 2, segment.colors);
}

void Draw(const Sphere& sphere)
{
	g_pAuxGeom->DrawSphere(
	  sphere.position,
	  sphere.radius,
	  sphere.color);
}

void Draw(const Cylinder& cylinder)
{
	g_pAuxGeom->DrawCylinder(
	  cylinder.position,
	  cylinder.direction,
	  cylinder.radius,
	  cylinder.height,
	  cylinder.color);
}

void Draw(const Cone& cone)
{
	g_pAuxGeom->DrawCone(
	  cone.position,
	  cone.direction,
	  cone.radius,
	  cone.height,
	  cone.color);
}

void Frame(const Vec3& position, const Vec3& axisX, const Vec3& axisY, const Vec3& axisZ)
{
	g_pAuxGeom->DrawLine(position, RGBA8(0x00, 0x00, 0x00, 0xff), position + axisX, RGBA8(0xff, 0x00, 0x00, 0xff));
	g_pAuxGeom->DrawLine(position, RGBA8(0x00, 0x00, 0x00, 0xff), position + axisY, RGBA8(0x00, 0xff, 0x00, 0xff));
	g_pAuxGeom->DrawLine(position, RGBA8(0x00, 0x00, 0x00, 0xff), position + axisZ, RGBA8(0x00, 0x00, 0xff, 0xff));
}

void Frame(const Matrix34& location, const Vec3& scale)
{
	Frame(location.GetTranslation(), location.GetColumn0() * scale.x, location.GetColumn1() * scale.y, location.GetColumn2() * scale.z);
}
void Frame(const QuatT& location, const Vec3& scale)
{
	Frame(location.t, location.GetColumn0() * scale.x, location.GetColumn1() * scale.y, location.GetColumn2() * scale.z);
}

void Arrow(const QuatT& location, const Vec3& direction, float length, ColorB color)
{
	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	g_pAuxGeom->SetRenderFlags(renderFlags);

	Vec3 absAxisY = location.q.GetColumn1();

	Vec3 vdir = direction.GetNormalized();
	Matrix33 m;
	m.m00 = 1;
	m.m01 = 0;
	m.m02 = 0;
	m.m10 = 0;
	m.m11 = 0;
	m.m12 = -1;
	m.m20 = 0;
	m.m21 = 1;
	m.m22 = 0;
	f64 l = sqrt(vdir.x * vdir.x + vdir.y * vdir.y);
	if (l > 0.0001)
	{
		f32 rad = f32(atan2(-vdir.z * (vdir.y / l), l));
		m.SetRotationX(-rad);
	}

	//----------------------------------------------------------------------

	const f32 scale = 1.0f;
	const f32 size = 0.009f;
	AABB yaabb = AABB(Vec3(-size * scale, -0.0f * scale, -size * scale), Vec3(size * scale, length * scale, size * scale));

	Matrix33 m2 = Matrix33(location.q) * m;
	OBB obb = OBB::CreateOBBfromAABB(Matrix33(location.q) * m, yaabb);
	g_pAuxGeom->DrawOBB(obb, location.t, 1, color, eBBD_Extremes_Color_Encoded);

	if (l > 0.0001)
	{
		f64 xl = -vdir.x / l;
		f64 yl = vdir.y / l;
		m.m00 = f32(yl);
		m.m01 = f32(vdir.x);
		m.m02 = f32(xl * vdir.z);
		m.m10 = f32(xl);
		m.m11 = f32(vdir.y);
		m.m12 = f32(-vdir.z * yl);
		m.m20 = 0;
		m.m21 = f32(vdir.z);
		m.m22 = f32(l);
	}
	g_pAuxGeom->DrawCone(m2 * (Vec3(0, 1, 0) * length * scale) + location.t, m2 * Vec3(0, 1, 0), 0.03f, 0.15f, color);
}

void CurvedArrow(const QuatT& location, float moveSpeed, float travelAngle, float turnSpeed, float slope, ColorB color)
{
	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	g_pAuxGeom->SetRenderFlags(renderFlags);

	static Ang3 angles = Ang3(0, 0, 0);
	angles.x += 0.01f;
	angles.y += 0.02f;
	angles.z += 0.03f;
	Matrix33 _m33 = Matrix33::CreateRotationXYZ(angles);
	AABB aabb1 = AABB(Vec3(-0.02f, -0.02f, -0.02f), Vec3(0.02f, 0.02f, 0.02f));
	OBB _obb1 = OBB::CreateOBBfromAABB(_m33, aabb1);

	f64 fStepSize = 1.0 / 50.0;
	QuatTd wpos = location * Quat::CreateRotationZ(travelAngle);
	f64 fTurnSpeed = turnSpeed * fStepSize;
	g_pAuxGeom->DrawSphere(wpos.t, 0.05f, ColorB(0xff, 0, 0));

	Vec3d absAxisX = wpos.q.GetColumn0();
	Vec3d absAxisY = wpos.q.GetColumn1();

	const f64 size = 0.015;
	Matrix33d SlopeMat33 = Matrix33::CreateRotationAA(slope, absAxisX);
	Matrix33d ZRot1 = Matrix33d::CreateRotationZ(f32(turnSpeed * fStepSize * 0.5));
	Matrix33d ZRot2 = Matrix33d::CreateRotationZ(f32(turnSpeed * fStepSize));

	Vec3d vBase = Vec3d(0, 0, 0);
	Vec3d vRelTurn = ZRot1 * Vec3d(0, moveSpeed * fStepSize, 0);
	for (f64 i = 0.0; i < 1.0; i = i + fStepSize)
	{
		Vec3d w0 = wpos.t + SlopeMat33 * (wpos.q * vBase);
		Vec3d w1 = wpos.t + SlopeMat33 * (wpos.q * (vBase + vRelTurn));
		Vec3d wdir = w1 - w0;
		Vec3d sidedir = Vec3d(-wdir.y, wdir.x, 0).GetNormalizedSafe(Vec3d(0, 0, 1)) * size;
		Vec3d a0 = w0 + Vec3d(0, 0, size);
		Vec3d b0 = w1 + Vec3d(0, 0, size);
		Vec3d a1 = w0 + Vec3d(0, 0, -size);
		Vec3d b1 = w1 + Vec3d(0, 0, -size);
		Vec3d a2 = w0 + sidedir;
		Vec3d b2 = w1 + sidedir;
		Vec3d a3 = w0 - sidedir;
		Vec3d b3 = w1 - sidedir;
		g_pAuxGeom->DrawTriangle(a0, color, b0, color, b2, color);
		g_pAuxGeom->DrawTriangle(a0, color, b2, color, a2, color);
		g_pAuxGeom->DrawTriangle(a2, color, b2, color, b1, color);
		g_pAuxGeom->DrawTriangle(a2, color, b1, color, a1, color);
		g_pAuxGeom->DrawTriangle(b0, color, a0, color, a3, color);
		g_pAuxGeom->DrawTriangle(b0, color, a3, color, b3, color);
		g_pAuxGeom->DrawTriangle(b3, color, a3, color, a1, color);
		g_pAuxGeom->DrawTriangle(b3, color, a1, color, b1, color);
		vBase += vRelTurn;
		vRelTurn = ZRot2 * vRelTurn;
	}
	g_pAuxGeom->DrawCone(wpos.t + SlopeMat33 * (wpos.q * vBase), SlopeMat33 * (wpos.q * vRelTurn).GetNormalizedSafe(Vec3(0, 0, 1)), 0.03f, 0.15f, color);
}

void BonePrism(const Vec3& origin, const Vec3& forward, const Vec3& up, const ColorB color)
{
	const float length = forward.GetLength();
	Vec3 forwardNormalized = forward * (1.0f / length);
	const Vec3 side = forwardNormalized.Cross(up);
	forwardNormalized *= up.GetLength();
	const Vec3 points[] =
	{
		origin + forwardNormalized + up - side,
		origin + forwardNormalized + up + side,
		origin + forwardNormalized - up + side,
		origin + forwardNormalized - up - side,
		origin + forward,
	};

	const Vec3 vertices[] =
	{
		origin,    points[1], points[0],
		origin,    points[2], points[1],
		origin,    points[3], points[2],
		origin,    points[0], points[3],
		points[4], points[0], points[1],
		points[4], points[1], points[2],
		points[4], points[2], points[3],
		points[4], points[3], points[0],
	};

	const ColorB color0(uint8(color.r * 0.5f), uint8(color.g * 0.5f), uint8(color.b * 0.5f), uint8(0xff * 1.0f));
	const ColorB color1(uint8(color.r * 0.75f), uint8(color.g * 0.75f), uint8(color.b * 0.75f), uint8(0xff * 1.0f));

	const ColorB color00 = ColorB(uint8(color0.r * 0.9f), uint8(color0.g * 0.9f), uint8(color0.b * 0.9f), uint8(color0.a * 1.0f));
	const ColorB color01 = ColorB(uint8(color0.r * 0.75f * 0.9f), uint8(color0.g * 0.75f * 0.9f), uint8(color0.b * 0.75f * 0.9f), uint8(color0.a * 1.0f));
	const ColorB color11 = ColorB(uint8(color0.r * 0.75f), uint8(color0.g * 0.75f), uint8(color0.b * 0.75f), uint8(color0.a * 1.0f));
	const ColorB color21 = ColorB(uint8(color1.r * 0.75f), uint8(color1.g * 0.75f), uint8(color1.b * 0.75f), uint8(color1.a * 1.0f));

	const ColorB colors[] =
	{
		color00, color00, color00,
		color01, color01, color01,
		color00, color00, color00,
		color01, color01, color01,
		color1,  color0,  color0,
		color21, color11, color11,
		color1,  color0,  color0,
		color21, color11, color11,
	};

	g_pAuxGeom->DrawTriangles(vertices, 3 * 8, colors);
}

float BonePrism(QuatT location, const Vec3& target, const ColorB color)
{
	const Vec3 forward = target - location.t;
	const float length = forward.GetLength();
	if (length > 0.0f)
	{
		const Vec3 forwardVector = forward * 1.0f / length;

		const uint forwardAxisIndex = idxmax3((forwardVector * location.q).abs());
		const uint upAxisIndex = (forwardAxisIndex + 2) % 3;

		Matrix33 frame = ComputeOrientationAlongDirectionMatrix(forwardAxisIndex, forward, upAxisIndex, location.q.GetColumn(upAxisIndex));
		BonePrism(location.t, forward, frame.GetColumn(1) * 0.075f * min(0.3f, length), color);
	}
	return length;

}

void Pose(const CDefaultSkeleton& rDefaultSkeleton, const Skeleton::CPoseData& poseData, const QuatT& location, ColorB color)
{
	const uint jointCount = poseData.GetJointCount();
	for (uint i = 0; i < jointCount; ++i)
	{
		const QuatT jointLocation = location * poseData.GetJointAbsolute(i);

		float length = 1.0f;
		int parentIndex = poseData.GetParentIndex(rDefaultSkeleton, i);
		if (parentIndex > -1)
		{
			Vec3 direction = location * (poseData.GetJointAbsolute(i).t - poseData.GetJointAbsolute(parentIndex).t);
			length = direction.GetLength();
			if (length > 0.0f)
			{
				length = BonePrism(location * poseData.GetJointAbsolute(parentIndex),
				                   location * poseData.GetJointAbsolute(i).t, color);
			}
			else
			{
				length = 1.0f;
			}
		}

		Frame(jointLocation, Vec3(length * 0.25f));
	}
}

} // namespace DrawHelper
