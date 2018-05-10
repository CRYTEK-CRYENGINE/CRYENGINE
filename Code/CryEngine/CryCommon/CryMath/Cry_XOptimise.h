// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	Description: Misc mathematical functions
//
//	History:
//	Formerly Cry_Math.h
//	-Jan 31,2001: Created by Marco Corbetta
//	-Jan 04,2003: SSE and 3DNow optimizations by Andrey Honich
//
//////////////////////////////////////////////////////////////////////

#pragma once

inline float AngleMod(float a)
{
	a = (float)((360.0 / 65536) * ((int)(a * (65536 / 360.0)) & 65535));
	return a;
}
inline float AngleModRad(float a)
{
	a = (float)((gf_PI2 / 65536) * ((int)(a * (65536 / gf_PI2)) & 65535));
	return a;
}
inline unsigned short Degr2Word(float f)
{
	return (unsigned short)(AngleMod(f) / 360.0f * 65536.0f);
}
inline float Word2Degr(unsigned short s)
{
	return (float)s / 65536.0f * 360.0f;
}

inline void mathMatrixPerspectiveFov(Matrix44A* pMatr, f32 fovY, f32 Aspect, f32 zn, f32 zf)
{
	f32 yScale = 1.0f / tan_tpl(fovY / 2.0f);
	f32 xScale = yScale / Aspect;

	f32 m22 = f32(f64(zf) / (f64(zn) - f64(zf)));
	f32 m32 = f32(f64(zn) * f64(zf) / (f64(zn) - f64(zf)));

	(*pMatr)(0, 0) = xScale;
	(*pMatr)(0, 1) = 0;
	(*pMatr)(0, 2) = 0;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = 0;
	(*pMatr)(1, 1) = yScale;
	(*pMatr)(1, 2) = 0;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = 0;
	(*pMatr)(2, 1) = 0;
	(*pMatr)(2, 2) = m22;
	(*pMatr)(2, 3) = -1.0f;
	(*pMatr)(3, 0) = 0;
	(*pMatr)(3, 1) = 0;
	(*pMatr)(3, 2) = m32;
	(*pMatr)(3, 3) = 0;
}

inline void mathMatrixOrtho(Matrix44A* pMatr, f32 w, f32 h, f32 zn, f32 zf)
{
	f32 m22 = f32(1.0 / (f64(zn) - f64(zf)));
	f32 m32 = f32(f64(zn) / (f64(zn) - f64(zf)));

	(*pMatr)(0, 0) = 2.0f / w;
	(*pMatr)(0, 1) = 0;
	(*pMatr)(0, 2) = 0;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = 0;
	(*pMatr)(1, 1) = 2.0f / h;
	(*pMatr)(1, 2) = 0;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = 0;
	(*pMatr)(2, 1) = 0;
	(*pMatr)(2, 2) = m22;
	(*pMatr)(2, 3) = 0;
	(*pMatr)(3, 0) = 0;
	(*pMatr)(3, 1) = 0;
	(*pMatr)(3, 2) = m32;
	(*pMatr)(3, 3) = 1;

}

inline void mathMatrixOrthoOffCenter(Matrix44A* pMatr, f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf)
{
	f32 m22 = f32(1.0 / (f64(zn) - f64(zf)));
	f32 m32 = f32(f64(zn) / (f64(zn) - f64(zf)));

	(*pMatr)(0, 0) = 2.0f / (r - l);
	(*pMatr)(0, 1) = 0;
	(*pMatr)(0, 2) = 0;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = 0;
	(*pMatr)(1, 1) = 2.0f / (t - b);
	(*pMatr)(1, 2) = 0;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = 0;
	(*pMatr)(2, 1) = 0;
	(*pMatr)(2, 2) = m22;
	(*pMatr)(2, 3) = 0;
	(*pMatr)(3, 0) = (l + r) / (l - r);
	(*pMatr)(3, 1) = (t + b) / (b - t);
	(*pMatr)(3, 2) = m32;
	(*pMatr)(3, 3) = 1.0f;
}

inline void mathMatrixOrthoOffCenterLH(Matrix44A* pMatr, f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf)
{
	f32 m22 = f32(1.0 / (f64(zf) - f64(zn)));
	f32 m32 = f32(f64(zn) / (f64(zn) - f64(zf)));

	(*pMatr)(0, 0) = 2.0f / (r - l);
	(*pMatr)(0, 1) = 0;
	(*pMatr)(0, 2) = 0;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = 0;
	(*pMatr)(1, 1) = 2.0f / (t - b);
	(*pMatr)(1, 2) = 0;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = 0;
	(*pMatr)(2, 1) = 0;
	(*pMatr)(2, 2) = m22;
	(*pMatr)(2, 3) = 0;
	(*pMatr)(3, 0) = (l + r) / (l - r);
	(*pMatr)(3, 1) = (t + b) / (b - t);
	(*pMatr)(3, 2) = m32;
	(*pMatr)(3, 3) = 1.0f;
}

inline void mathMatrixOrthoOffCenterLHReverseDepth(Matrix44A* pMatr, f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf)
{
	f32 m22 = f32(-1.0 / (f64(zf) - f64(zn)));
	f32 m32 = f32(f64(zf) / (f64(zf) - f64(zn)));

	(*pMatr)(0, 0) = 2.0f / (r - l);
	(*pMatr)(0, 1) = 0;
	(*pMatr)(0, 2) = 0;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = 0;
	(*pMatr)(1, 1) = 2.0f / (t - b);
	(*pMatr)(1, 2) = 0;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = 0;
	(*pMatr)(2, 1) = 0;
	(*pMatr)(2, 2) = m22;
	(*pMatr)(2, 3) = 0;
	(*pMatr)(3, 0) = (l + r) / (l - r);
	(*pMatr)(3, 1) = (t + b) / (b - t);
	(*pMatr)(3, 2) = m32;
	(*pMatr)(3, 3) = 1.0f;
}

inline void mathMatrixPerspectiveOffCenter(Matrix44A* pMatr, f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf)
{
	f32 m22 = f32(f64(zf) / (f64(zn) - f64(zf)));
	f32 m32 = f32(f64(zn) * f64(zf) / (f64(zn) - f64(zf)));

	(*pMatr)(0, 0) = 2 * zn / (r - l);
	(*pMatr)(0, 1) = 0;
	(*pMatr)(0, 2) = 0;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = 0;
	(*pMatr)(1, 1) = 2 * zn / (t - b);
	(*pMatr)(1, 2) = 0;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = (l + r) / (r - l);
	(*pMatr)(2, 1) = (t + b) / (t - b);
	(*pMatr)(2, 2) = m22;
	(*pMatr)(2, 3) = -1;
	(*pMatr)(3, 0) = 0;
	(*pMatr)(3, 1) = 0;
	(*pMatr)(3, 2) = m32;
	(*pMatr)(3, 3) = 0;
}

inline void mathMatrixPerspectiveOffCenterReverseDepth(Matrix44A* pMatr, f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf)
{
	f32 m22 = f32(-f64(zn) / (f64(zn) - f64(zf)));
	f32 m32 = f32(-f64(zn) * f64(zf) / (f64(zn) - f64(zf)));

	(*pMatr)(0, 0) = 2 * zn / (r - l);
	(*pMatr)(0, 1) = 0;
	(*pMatr)(0, 2) = 0;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = 0;
	(*pMatr)(1, 1) = 2 * zn / (t - b);
	(*pMatr)(1, 2) = 0;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = (l + r) / (r - l);
	(*pMatr)(2, 1) = (t + b) / (t - b);
	(*pMatr)(2, 2) = m22;
	(*pMatr)(2, 3) = -1;
	(*pMatr)(3, 0) = 0;
	(*pMatr)(3, 1) = 0;
	(*pMatr)(3, 2) = m32;
	(*pMatr)(3, 3) = 0;
}

inline void mathMatrixLookAt(Matrix44A* pMatr, const Vec3& Eye, const Vec3& At, const Vec3& Up)
{
	Vec3 vLightDir = (Eye - At);
	Vec3 zaxis = vLightDir.GetNormalized();
	Vec3 xaxis = (Up.Cross(zaxis)).GetNormalized();
	Vec3 yaxis = zaxis.Cross(xaxis);

	(*pMatr)(0, 0) = xaxis.x;
	(*pMatr)(0, 1) = yaxis.x;
	(*pMatr)(0, 2) = zaxis.x;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = xaxis.y;
	(*pMatr)(1, 1) = yaxis.y;
	(*pMatr)(1, 2) = zaxis.y;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = xaxis.z;
	(*pMatr)(2, 1) = yaxis.z;
	(*pMatr)(2, 2) = zaxis.z;
	(*pMatr)(2, 3) = 0;
	(*pMatr)(3, 0) = -xaxis.Dot(Eye);
	(*pMatr)(3, 1) = -yaxis.Dot(Eye);
	(*pMatr)(3, 2) = -zaxis.Dot(Eye);
	(*pMatr)(3, 3) = 1;
}

inline bool mathMatrixPerspectiveFovInverse(Matrix44_tpl<f64>* pResult, const Matrix44A* pProjFov)
{
	if ((*pProjFov)(0, 1) == 0.0f && (*pProjFov)(0, 2) == 0.0f && (*pProjFov)(0, 3) == 0.0f &&
	    (*pProjFov)(1, 0) == 0.0f && (*pProjFov)(1, 2) == 0.0f && (*pProjFov)(1, 3) == 0.0f &&
	    (*pProjFov)(3, 0) == 0.0f && (*pProjFov)(3, 1) == 0.0f && (*pProjFov)(3, 2) != 0.0f)
	{
		(*pResult)(0, 0) = 1.0 / (*pProjFov).m00;
		(*pResult)(0, 1) = 0;
		(*pResult)(0, 2) = 0;
		(*pResult)(0, 3) = 0;
		(*pResult)(1, 0) = 0;
		(*pResult)(1, 1) = 1.0 / (*pProjFov).m11;
		(*pResult)(1, 2) = 0;
		(*pResult)(1, 3) = 0;
		(*pResult)(2, 0) = 0;
		(*pResult)(2, 1) = 0;
		(*pResult)(2, 2) = 0;
		(*pResult)(2, 3) = 1.0 / (*pProjFov).m32;
		(*pResult)(3, 0) = (*pProjFov).m20 / (*pProjFov).m00;
		(*pResult)(3, 1) = (*pProjFov).m21 / (*pProjFov).m11;
		(*pResult)(3, 2) = -1;
		(*pResult)(3, 3) = (*pProjFov).m22 / (*pProjFov).m32;

		return true;
	}

	return false;
}

template<class M_out, class M_in> inline void mathMatrixLookAtInverse(M_out* pResult, const M_in* pLookAt)
{
	(*pResult)(0, 0) = (*pLookAt).m00;
	(*pResult)(0, 1) = (*pLookAt).m10;
	(*pResult)(0, 2) = (*pLookAt).m20;
	(*pResult)(0, 3) = (*pLookAt).m03;
	(*pResult)(1, 0) = (*pLookAt).m01;
	(*pResult)(1, 1) = (*pLookAt).m11;
	(*pResult)(1, 2) = (*pLookAt).m21;
	(*pResult)(1, 3) = (*pLookAt).m13;
	(*pResult)(2, 0) = (*pLookAt).m02;
	(*pResult)(2, 1) = (*pLookAt).m12;
	(*pResult)(2, 2) = (*pLookAt).m22;
	(*pResult)(2, 3) = (*pLookAt).m23;

	(*pResult)(3, 0) = (-(f64((*pLookAt).m00) * f64((*pLookAt).m30) + f64((*pLookAt).m01) * f64((*pLookAt).m31) + f64((*pLookAt).m02) * f64((*pLookAt).m32)));
	(*pResult)(3, 1) = (-(f64((*pLookAt).m10) * f64((*pLookAt).m30) + f64((*pLookAt).m11) * f64((*pLookAt).m31) + f64((*pLookAt).m12) * f64((*pLookAt).m32)));
	(*pResult)(3, 2) = (-(f64((*pLookAt).m20) * f64((*pLookAt).m30) + f64((*pLookAt).m21) * f64((*pLookAt).m31) + f64((*pLookAt).m22) * f64((*pLookAt).m32)));
	(*pResult)(3, 3) = (*pLookAt).m33;
};

// Fix replace viewport by int16 array.
// Fix for d3d viewport.
inline f32 mathVec3Project(Vec3* pvWin, const Vec3* pvObj, const int32 pViewport[4], const Matrix44A* pProjection, const Matrix44A* pView, const Matrix44A* pWorld)
{
	Vec4 in, out;

	in.x = pvObj->x;
	in.y = pvObj->y;
	in.z = pvObj->z;
	in.w = 1.0f;
	out = in * *pWorld;
	in = out * *pView;
	out = in * *pProjection;

	if (out.w == 0.0f)
		return 0.f;

	out.x /= out.w;
	out.y /= out.w;
	out.z /= out.w;

	//output coords
	pvWin->x = pViewport[0] + (1 + out.x) * pViewport[2] / 2;
	pvWin->y = pViewport[1] + (1 - out.y) * pViewport[3] / 2;  //flip coords for y axis

	//FIX: update fViewportMinZ fViewportMaxZ support for Viewport everywhere
	float fViewportMinZ = 0, fViewportMaxZ = 1.0f;

	pvWin->z = fViewportMinZ + out.z * (fViewportMaxZ - fViewportMinZ);

	return out.w;
}

inline Vec3* mathVec3UnProject(Vec3* pvObj, const Vec3* pvWin, const int32 pViewport[4], const Matrix44A* pProjection, const Matrix44A* pView, const Matrix44A* pWorld)
{
	Matrix44A m, mA;
	Vec4 in, out;

	//FIX: update fViewportMinZ fViewportMaxZ support for Viewport everywhere
	float fViewportMinZ = 0, fViewportMaxZ = 1.0f;

	in.x = (pvWin->x - pViewport[0]) * 2 / pViewport[2] - 1.0f;
	in.y = 1.0f - ((pvWin->y - pViewport[1]) * 2 / pViewport[3]);   //flip coords for y axis
	in.z = (pvWin->z - fViewportMinZ) / (fViewportMaxZ - fViewportMinZ);
	in.w = 1.0f;

	//prepare inverse projection matrix
	mA = ((*pWorld) * (*pView)) * (*pProjection);
	m = mA.GetInverted();

	out = in * m;
	if (out.w == 0.0f)
		return NULL;

	pvObj->x = out.x / out.w;
	pvObj->y = out.y / out.w;
	pvObj->z = out.z / out.w;

	return pvObj;
}

inline Vec3* mathVec3ProjectArray(Vec3* pOut, uint32 OutStride, const Vec3* pV, uint32 VStride, const int32 pViewport[4], const Matrix44A* pProjection, const Matrix44A* pView, const Matrix44A* pWorld, uint32 n)
{
	Matrix44A m;
	Vec4 in, out;

	int8* pOutT = (int8*)pOut;
	int8* pInT = (int8*)pV;

	Vec3* pvWin;
	Vec3* pvObj;

	//FIX: update fViewportMinZ fViewportMaxZ support for Viewport everywhere
	float fViewportMinZ = 0, fViewportMaxZ = 1.0f;

	m = ((*pWorld) * (*pView)) * (*pProjection);

	for (uint32 i = 0; i < n; i++)
	{

		pvObj = (Vec3*)pInT;
		pvWin = (Vec3*)pOutT;

		in.x = pvObj->x;
		in.y = pvObj->y;
		in.z = pvObj->z;
		in.w = 1.0f;

		out = in * m;

		if (out.w == 0.0f)
			return NULL;

		float fInvW = 1.0f / out.w;
		out.x *= fInvW;
		out.y *= fInvW;
		out.z *= fInvW;

		//output coords
		pvWin->x = pViewport[0] + (1 + out.x) * pViewport[2] / 2;
		pvWin->y = pViewport[1] + (1 - out.y) * pViewport[3] / 2;  //flip coords for y axis

		pvWin->z = fViewportMinZ + out.z * (fViewportMaxZ - fViewportMinZ);

		pOutT += OutStride;
		pInT += VStride;
	}

	return pOut;
}

inline Vec3* mathVec3UnprojectArray(Vec3* pOut, uint32 OutStride, const Vec3* pV, uint32 VStride, const int32 pViewport[4], const Matrix44* pProjection, const Matrix44* pView, const Matrix44* pWorld, uint32 n)
{
	Vec4 in, out;
	Matrix44 m, mA;

	int8* pOutT = (int8*)pOut;
	int8* pInT = (int8*)pV;

	Vec3* pvWin;
	Vec3* pvObj;

	//FIX: update fViewportMinZ fViewportMaxZ support for Viewport everywhere
	float fViewportMinZ = 0, fViewportMaxZ = 1.0f;

	mA = ((*pWorld) * (*pView)) * (*pProjection);
	m = mA.GetInverted();

	for (uint32 i = 0; i < n; i++)
	{
		pvWin = (Vec3*)pInT;
		pvObj = (Vec3*)pOutT;

		in.x = (pvWin->x - pViewport[0]) * 2 / pViewport[2] - 1.0f;
		in.y = 1.0f - ((pvWin->y - pViewport[1]) * 2 / pViewport[3]);   //flip coords for y axis
		in.z = (pvWin->z - fViewportMinZ) / (fViewportMaxZ - fViewportMinZ);
		in.w = 1.0f;

		out = in * m;

		CRY_MATH_ASSERT(out.w != 0.0f);

		if (out.w == 0.0f)
			return NULL;

		pvObj->x = out.x / out.w;
		pvObj->y = out.y / out.w;
		pvObj->z = out.z / out.w;

		pOutT += OutStride;
		pInT += VStride;
	}

	return pOut;
}
