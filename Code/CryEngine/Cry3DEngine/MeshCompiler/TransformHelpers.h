// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>

namespace TransformHelpers
{
// Format of forwardUpAxes: "<signOfForwardAxis><forwardAxis><signOfUpAxis><upAxis>".
// Example of forwardUpAxes: "-Y+Z".
// Returns 0 if successful, or returns a pointer to an error message in case of an error.
// In case of success: X axis in res represents "forward" direction,
// Y axis represents "up" direction.
inline const char* GetForwardUpAxesMatrix(Matrix33& res, const char* forwardUpAxes)
{
	Vec3 axisX(ZERO);
	Vec3 axisY(ZERO);

	for (int i = 0; i < 2; ++i)
	{
		Vec3& v = (i == 0) ? axisX : axisY;

		const float val = forwardUpAxes[i * 2 + 0] == '-' ? -1.0f : +1.0f;

		switch (forwardUpAxes[i * 2 + 1])
		{
		case 'X':
		case 'x':
			v.x = val;
			break;
		case 'Y':
		case 'y':
			v.y = val;
			break;
		case 'Z':
		case 'z':
			v.z = val;
			break;
		default:
			return "Found a bad axis character in forwardUpAxes string";
		}
	}

	if (axisX == axisY)
	{
		return "Forward and up axes are equal in forwardUpAxes string";
	}

	const Vec3 axisZ = axisX.cross(axisY);

	res.SetFromVectors(axisX, axisY, axisZ);

	return 0;
}

// Computes transform matrix that converts everything from forwardUpAxesSrc
// coordinate system to forwardUpAxesDst coordinate system.
// Format of forwardUpAxesXXX: "<signOfForwardAxis><forwardAxis><signOfUpAxis><upAxis>".
// Example of forwardUpAxesXXX: "-Y+Z".
// Returns 0 if successful, or returns a pointer to an error message in case of an error.
// In case of success puts computed transform into res.
// See comments to GetForwardUpAxesMatrix().
inline const char* ComputeForwardUpAxesTransform(Matrix34& res, const char* forwardUpAxesSrc, const char* forwardUpAxesDst)
{
	Matrix33 srcToWorld;
	Matrix33 dstToWorld;

	const char* const err0 = GetForwardUpAxesMatrix(srcToWorld, forwardUpAxesSrc);
	const char* const err1 = GetForwardUpAxesMatrix(dstToWorld, forwardUpAxesDst);

	if (err0 || err1)
	{
		return err0 ? err0 : err1;
	}

	res = Matrix34(dstToWorld * srcToWorld.GetTransposed());

	return 0;
}

inline Matrix34 ComputeOrthonormalMatrix(const Matrix34& m)
{
	Vec3 x = m.GetColumn0();
	x.Normalize();

	Vec3 y = m.GetColumn1();

	Vec3 z = x.cross(y);
	z.Normalize();

	y = z.cross(x);

	Matrix34 result;
	result.SetFromVectors(x, y, z, m.GetTranslation());

	return result;
}
}
