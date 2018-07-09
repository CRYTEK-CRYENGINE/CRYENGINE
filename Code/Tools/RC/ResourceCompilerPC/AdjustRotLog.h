// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _AdjustRotLog_function_hdr_
#define _AdjustRotLog_function_hdr_

inline double DLength (const Vec3& v)
{
	return sqrt (double(v.x)*double(v.x) + double(v.y)*double(v.y) + double(v.z)*double(v.z));
}

//////////////////////////////////////////////////////////////////////////
// Given the rotations in logarithmic space, adjusts the target rotation so
// that it's the same in rotational group, but the closest to the reference
// in the QLog space.
inline void AdjustRotLog (Vec3& vTgt, const Vec3& vRef)
{
	double dLenTgt = DLength(vTgt);
	const double dPi = 3.1415926535897932384626433832795;
	if (dLenTgt < 1e-4)
	{
		// the target is very small rotation, so the algorithm is to find 
		// ANY vector of length n*PI closest to the vRef point
		double dLenRef = DLength(vRef);
		if (dLenRef > dPi/2) // Otherwise the vRef vector is small enough not to make any adjustments
		{
			double f = (dPi * floor(dLenRef/dPi+0.5) / dLenRef);
			vTgt.x = float(vRef.x * f);
			vTgt.y = float(vRef.y * f);
			vTgt.z = float(vRef.z * f);
		}
	}
	else
	{
		// the target is big enough rotation to pick the rotation axis out

		// find the projection of the reference to the target axis
		// then find the target (projection) mod PI
		// there are basically three possibilities: the new target is in the same PI interval
		// as the reference projection, in the next or in the previous. Find the closest.

		double dProjRef = (vRef * vTgt) / dLenTgt;
		double dModTgt = fmod (dLenTgt, dPi);
		if (dModTgt < 0) dModTgt += dPi;
		assert (dModTgt >= 0 && dModTgt < dPi);

		double dBaseTgt = dPi * floor (dProjRef/dPi + 0.5);

		double dNewTgtR = dBaseTgt + dModTgt;
		double dNewTgtL = dBaseTgt + dModTgt - dPi;
		double dNewTgt = (fabs(dNewTgtR - dProjRef) < fabs(dNewTgtL  - dProjRef))
			? dNewTgtR : dNewTgtL;

		assert (fabs(dNewTgt + dPi - dProjRef) > fabs(dNewTgt - dProjRef));
		assert (fabs(dNewTgt - dPi - dProjRef) > fabs(dNewTgt - dProjRef));

		double f = (dNewTgt / dLenTgt);
		vTgt.x = float(vTgt.x*f);
		vTgt.y = float(vTgt.y*f);
		vTgt.z = float(vTgt.z*f);
	}
}

#endif