// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace pfx2
{

// Vector functions

ILINE Vec3 HMin(const Vec3v& v0)
{
	return Vec3(
	  hmin(v0.x),
		hmin(v0.y),
		hmin(v0.z));
}

ILINE Vec3 HMax(const Vec3v& v0)
{
	return Vec3(
	  hmax(v0.x),
		hmax(v0.y),
		hmax(v0.z));
}

ILINE Vec3v Add(const Vec3v& a, floatv b)
{
	return Vec3v(a.x + b, a.y + b, a.z + b);
}
ILINE Vec3v Sub(const Vec3v& a, floatv b)
{
	return Vec3v(a.x - b, a.y - b, a.z - b);
}

template<typename T>
ILINE Vec3_tpl<T> MAdd(const Vec3_tpl<T>& a, const Vec3_tpl<T>& b, const Vec3_tpl<T>& c)
{
	return Vec3_tpl<T>(
	  MAdd(a.x, b.x, c.x),
	  MAdd(a.y, b.y, c.y),
	  MAdd(a.z, b.z, c.z));
}

template<typename T>
ILINE Vec3_tpl<T> MAdd(const Vec3_tpl<T>& a, T b, const Vec3_tpl<T>& c)
{
	return Vec3_tpl<T>(
	  MAdd(a.x, b, c.x),
	  MAdd(a.y, b, c.y),
	  MAdd(a.z, b, c.z));
}

template<typename T>
ILINE T DeltaTime(T frameTime, T normAge, T lifeTime)
{
	// Birth time = -age * life
	// Death time = -age * life + life
	// Delta time = min(- age * life + life, 0) - max(-age * life, -dT)
	T age = normAge * lifeTime;
	T startTime = -min(age, frameTime);
	T endTime = min(lifeTime - age, convert<T>());
	T deltaTime = max(endTime - startTime, convert<T>());
	return deltaTime;
}

template<typename T>
ILINE T StartTime(T curTime, T frameTime, T absAge)
{
	// Birth time = T - age*life
	// Start time = T - min(age*life, dT)
	return curTime - min(absAge, frameTime);
}


ILINE uint8 FloatToUFrac8Saturate(float v)
{
	return uint8(saturate(v) * 255.0f + 0.5f);
}

ILINE ColorF ToColorF(UCol color)
{
	return ColorF(
	  ufrac8_to_float(color.r),
	  ufrac8_to_float(color.g),
	  ufrac8_to_float(color.b));
}

ILINE ColorFv ToColorFv(ColorF color)
{
	return ColorFv(
	  ToFloatv(color.r),
	  ToFloatv(color.g),
	  ToFloatv(color.b));
}

ILINE UCol ColorFToUCol(const ColorF& color)
{
	UCol result;
	result.r = float_to_ufrac8(color.r);
	result.g = float_to_ufrac8(color.g);
	result.b = float_to_ufrac8(color.b);
	result.a = 0xff;
	return result;
}

ILINE void RotateAxes(Vec3* v0, Vec3* v1, float angle)
{
	CRY_PFX2_ASSERT(v0 && v1);
	Vec2 vRot;
	sincos(angle, &vRot.x, &vRot.y);
	Vec3 vXAxis = *v0 * vRot.y - *v1 * vRot.x;
	Vec3 vYAxis = *v0 * vRot.x + *v1 * vRot.y;
	*v0 = vXAxis;
	*v1 = vYAxis;
}

ILINE Vec3 PolarCoordToVec3(float azimuth, float altitude)
{
	float azc, azs;
	float alc, als;
	sincos(azimuth, &azs, &azc);
	sincos(altitude, &als, &alc);
	return Vec3(azc*alc, azs*alc, als);
}

}

#ifdef CRY_PFX2_USE_SSE
	#include "ParticleMathImplSSE.h"
#else

// PFX2_TODO : move all of this to another file (or just implement NEON for once)
namespace pfx2
{

ILINE floatv ToFloatv(float v)
{
	return v;
}

ILINE uint32v ToUint32v(uint32 v)
{
	return v;
}

ILINE floatv ToFloatv(int32v v)
{
	return floatv(v);
}

ILINE Vec3v ToVec3v(Vec3 v)
{
	return v;
}

ILINE Vec4v ToVec4v(Vec4 v)
{
	return v;
}

ILINE Planev ToPlanev(Plane v)
{
	return v;
}

ILINE ColorFv ToColorFv(UColv color)
{
	return ColorFv(
	  ufrac8_to_float(color.r),
	  ufrac8_to_float(color.g),
	  ufrac8_to_float(color.b));
}

ILINE UColv ColorFvToUColv(const ColorFv& color)
{
	UColv result;
	result.r = float_to_ufrac8(color.r);
	result.g = float_to_ufrac8(color.g);
	result.b = float_to_ufrac8(color.b);
	result.a = 0xff;
	return result;
}

ILINE UColv ToUColv(UCol color)
{
	return color;
}

ILINE Quat AddAngularVelocity(Quat initial, Vec3 angularVel, float deltaTime)
{
	const float haldDt = deltaTime * 0.5f;
	const Quat rotated = Quat::exp(angularVel * haldDt) * initial;
	return rotated.GetNormalized();
}


}

#endif
