// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HERMITE__H__
#define __HERMITE__H__



inline float Hermite1(float x)
{
	return 2.0f*x*x*x - 3*x*x + 1.0f;
}



inline float Hermite2(float x)
{
	return -2.0f*x*x*x + 3*x*x;
}



inline float Hermite3(float x)
{
	return x*x*x - 2.0f*x*x + x;
}



inline float Hermite4(float x)
{
	return x*x*x - x*x;
}



inline float HermiteInterpolate(float point1, float tension1, float point2, float tension2, float x)
{
	return
		Hermite1(x) * point1 +
		Hermite2(x) * point2 +
		Hermite3(x) * tension1 +
		Hermite4(x) * tension2;
}



inline Vec2 HermiteInterpolate(Vec2 point1, Vec2 tension1, Vec2 point2, Vec2 tension2, float x)
{
	return
		Hermite1(x) * point1 +
		Hermite2(x) * point2 +
		Hermite3(x) * tension1 +
		Hermite4(x) * tension2;
}



inline Vec3 HermiteInterpolate(Vec3 point1, Vec3 tension1, Vec3 point2, Vec3 tension2, float x)
{
	return
		Hermite1(x) * point1 +
		Hermite2(x) * point2 +
		Hermite3(x) * tension1 +
		Hermite4(x) * tension2;
}



inline Ang3 HermiteInterpolate(Ang3 point1, Ang3 tension1, Ang3 point2, Ang3 tension2, float x)
{
	return Ang3(HermiteInterpolate(Vec3(point1), Vec3(tension1), Vec3(point2), Vec3(tension2), x));
}



inline float CatmullRom(float point0, float point1, float point2, float point3, float x)
{
	float xx = x * x;
	float xxx = xx * x;
	return
		0.5f * ((2.0f * point1) +
		(-point0 + point2)*x +
		(2.0f*point0 - 5.0f*point1 + 4.0f*point2 - point3)*xx +
		(-point0 + 3.0f*point1- 3.0f*point2 + point3)*xxx);
}



inline Vec3 CatmullRom(Vec3 point0, Vec3 point1, Vec3 point2, Vec3 point3, float x)
{
	return Vec3(
		CatmullRom(point0.x, point1.x, point2.x, point3.x, x),
		CatmullRom(point0.y, point1.y, point2.y, point3.y, x),
		CatmullRom(point0.z, point1.z, point2.z, point3.z, x));
}



#endif
