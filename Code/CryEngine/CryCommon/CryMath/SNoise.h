// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//               Distributed under the MIT License. See LICENSE file.
//               https://github.com/ashima/webgl-noise
//
// CryEngine Impl : Filipe Amim
//

#pragma once

namespace crydetail
{

// helpers to convert glsl code to cryengine

#ifdef CRY_PLATFORM_SSE2

ILINE __m128 step(__m128 edge, __m128 x)
{
	const __m128 m = _mm_cmpge_ps(x, edge);
	return _mm_and_ps(m, _mm_set1_ps(1.0f));
}

ILINE __m128 lessThan(__m128 v0, __m128 v1)
{
	const __m128 m = _mm_cmplt_ps(v0, v1);
	return _mm_and_ps(m, _mm_set1_ps(1.0f));
}

ILINE __m128 fma(__m128 v0, __m128 v1, __m128 v2)
{
	return _mm_add_ps(_mm_mul_ps(v0, v1), v2);
}

#endif

ILINE float step(float edge, float x)
{
	return x < edge ? 0.0f : 1.0f;
}

ILINE float lessThan(float v0, float v1)
{
	return v0 < v1 ? 1.0f : 0.0f;
}

ILINE float fma(float v0, float v1, float v2)
{
	return v0 * v1 + v2;
}

template<typename Real>
Vec2_tpl<Real> ToVec2(Real x)
{
	return Vec2_tpl<Real>(x, x);
}

template<typename Real>
Vec3_tpl<Real> ToVec3(Real x)
{
	return Vec3_tpl<Real>(x, x, x);
}

template<typename Real>
Vec4_tpl<Real> ToVec4(Real x)
{
	return Vec4_tpl<Real>(x, x, x, x);
}

using crymath::floor;
using crymath::saturate;
using crymath::abs;

template<typename Real>
Vec3_tpl<Real> floor(Vec3_tpl<Real> x)
{
	return Vec3_tpl<Real>(floor(x.x), floor(x.y), floor(x.z));
}

template<typename Real>
Vec4_tpl<Real> floor(Vec4_tpl<Real> x)
{
	return Vec4_tpl<Real>(floor(x.x), floor(x.y), floor(x.z), floor(x.w));
}

template<typename Real>
Vec4_tpl<Real> add(Vec4_tpl<Real> v0, Vec4_tpl<Real> v1)
{
	return Vec4_tpl<Real>(v0.x * v1.x, v0.y * v1.y, v0.z * v1.z, v0.w * v1.w);
}

template<typename Real>
Vec2_tpl<Real> mul(Vec2_tpl<Real> v0, Vec2_tpl<Real> v1)
{
	return Vec2_tpl<Real>(v0.x * v1.x, v0.y * v1.y);
}

template<typename Real>
Vec3_tpl<Real> mul(Vec3_tpl<Real> v0, Vec3_tpl<Real> v1)
{
	return Vec3_tpl<Real>(v0.x * v1.x, v0.y * v1.y, v0.z * v1.z);
}

template<typename Real>
Vec4_tpl<Real> mul(Vec4_tpl<Real> v0, Real v1)
{
	return Vec4_tpl<Real>(v0.x * v1, v0.y * v1, v0.z * v1, v0.w * v1);
}

template<typename Real>
Vec4_tpl<Real> fma(Vec4_tpl<Real> v0, Real v1, Vec4_tpl<Real> v2)
{
	return Vec4_tpl<Real>(
	  fma(v0.x, v1, v2.x),
	  fma(v0.y, v1, v2.y),
	  fma(v0.z, v1, v2.z),
	  fma(v0.w, v1, v2.w));
}

template<typename Real>
Real dot(Vec2_tpl<Real> v0, Vec2_tpl<Real> v1)
{
	return fma(v0.x, v1.x, v0.y * v1.y);
}

template<typename Real>
Real dot(Vec3_tpl<Real> v0, Vec3_tpl<Real> v1)
{
	return fma(v0.x, v1.x, fma(v0.y, v1.y, v0.z * v1.z));
}

template<typename Real>
Real dot(Vec4_tpl<Real> v0, Vec4_tpl<Real> v1)
{
	return fma(v0.x, v1.x, fma(v0.y, v1.y, fma(v0.z, v1.z, v0.w * v1.w)));
}

template<typename Real>
Vec3_tpl<Real> abs(Vec3_tpl<Real> v0)
{
	return Vec3_tpl<Real>(abs(v0.x), abs(v0.y), abs(v0.z));
}

template<typename Real>
Vec4_tpl<Real> saturate(Vec4_tpl<Real> v0)
{
	return Vec4_tpl<Real>(saturate(v0.x), saturate(v0.y), saturate(v0.z), saturate(v0.w));
}

template<typename Real>
Vec3_tpl<Real> fract(Vec3_tpl<Real> x)
{
	return x - floor(x);
}

template<typename Real>
Vec3_tpl<Real> step(Vec3_tpl<Real> v0, Vec3_tpl<Real> v1)
{
	return Vec3_tpl<Real>(step(v0.x, v1.x), step(v0.y, v1.y), step(v0.z, v1.z));
}

template<typename Real>
Vec4_tpl<Real> lessThan(Vec4_tpl<Real> v0, Vec4_tpl<Real> v1)
{
	return Vec4_tpl<Real>(lessThan(v0.x, v1.x), lessThan(v0.y, v1.y), lessThan(v0.z, v1.z), lessThan(v0.w, v1.w));
}

template<typename Real>
Vec2_tpl<Real>& v4_zw(Vec4_tpl<Real>& x)
{
	return *(Vec2_tpl<Real>*)(&x.z);
}

template<typename Real>
Vec3_tpl<Real>& v4_xyz(Vec4_tpl<Real>& x)
{
	return *(Vec3_tpl<Real>*)(&x.x);
}

template<typename Real>
Vec3_tpl<Real>& v4_yzw(Vec4_tpl<Real>& x)
{
	return *(Vec3_tpl<Real>*)(&x.y);
}

template<typename Real>
Real& v4_w(Vec4_tpl<Real>& x)
{
	return x.w;
}

template<typename Real>
Vec2_tpl<Real> xy(Vec3_tpl<Real> x)
{
	return Vec2_tpl<Real>(x.x, x.y);
}

template<typename Real>
Vec2_tpl<Real> zw(Vec4_tpl<Real> x)
{
	return Vec2_tpl<Real>(x.z, x.w);
}

template<typename Real>
Vec3_tpl<Real> xyz(Vec4_tpl<Real> x)
{
	return Vec3_tpl<Real>(x.x, x.y, x.z);
}

template<typename Real>
Vec3_tpl<Real> yzw(Vec4_tpl<Real> x)
{
	return Vec3_tpl<Real>(x.y, x.z, x.w);
}

template<typename Real>
Vec3_tpl<Real> xxx(Vec4_tpl<Real> x)
{
	return Vec3_tpl<Real>(x.x, x.x, x.x);
}

template<typename Real>
Vec3_tpl<Real> yyz(Vec4_tpl<Real> x)
{
	return Vec3_tpl<Real>(x.y, x.y, x.z);
}

template<typename Real>
Vec3_tpl<Real> zww(Vec4_tpl<Real> x)
{
	return Vec3_tpl<Real>(x.z, x.w, x.w);
}

template<typename Real>
Vec3_tpl<Real> www(Vec4_tpl<Real> x)
{
	return Vec3_tpl<Real>(x.w, x.w, x.w);
}

template<typename Real>
Vec4_tpl<Real> xxxx(Vec4_tpl<Real> x)
{
	return Vec4_tpl<Real>(x.x, x.x, x.x, x.x);
}

template<typename Real>
Vec4_tpl<Real> yyyy(Vec4_tpl<Real> x)
{
	return Vec4_tpl<Real>(x.y, x.y, x.y, x.y);
}

template<typename Real>
Vec4_tpl<Real> zzzz(Vec4_tpl<Real> x)
{
	return Vec4_tpl<Real>(x.z, x.z, x.z, x.z);
}

template<typename Real>
Vec4_tpl<Real> wwww(Vec4_tpl<Real> x)
{
	return Vec4_tpl<Real>(x.w, x.w, x.w, x.w);
}

//////////////////////////////////////////////////////////////////////////

template<typename Real>
Vec4_tpl<Real> mod289(Vec4_tpl<Real> x)
{
	const Real v289 = convert<Real>(289.0);
	const Real iv289 = convert<Real>(1.0 / 289.0);
	return x - floor(x * iv289) * v289;
}

template<typename Real>
Real mod289(Real x)
{
	const Real v289 = convert<Real>(289.0);
	const Real iv289 = convert<Real>(1.0 / 289.0);
	return x - floor(x * iv289) * v289;
}

template<typename Real>
Vec4_tpl<Real> permute(Vec4_tpl<Real> x)
{
	const Real one = convert<Real>(1.0);
	const Real v34 = convert<Real>(34.0);
	return mod289(((x * v34) + ToVec4(one)) * x);
}

template<typename Real>
Real permute(Real x)
{
	const Real one = convert<Real>(1.0);
	const Real v34 = convert<Real>(34.0);
	return mod289(((x * v34) + one) * x);
}

template<typename Real>
Vec4_tpl<Real> taylorInvSqrt(Vec4_tpl<Real> r)
{
	return ToVec4(convert<Real>(1.79284291400159)) - r * convert<Real>(0.85373472095314);
}

template<typename Real>
Real taylorInvSqrt(Real r)
{
	return convert<Real>(1.79284291400159) - convert<Real>(0.85373472095314) * r;
}

template<typename Real>
Vec4_tpl<Real> grad4(Real j, Vec4_tpl<Real> ip)
{
	const Real zero = convert<Real>(0.0);
	const Real one = convert<Real>(1.0);
	const Real minusOne = convert<Real>(-1.0);
	const Real v2 = convert<Real>(2.0);
	const Real v7 = convert<Real>(7.0);
	const Real v15 = convert<Real>(1.5);
	const Vec4_tpl<Real> ones = Vec4_tpl<Real>(one, one, one, minusOne);
	Vec4_tpl<Real> p, s;

	v4_xyz(p) = floor(fract(mul(ToVec3(j), xyz(ip))) * v7) * ip.z - ToVec3(one);
	v4_w(p) = v15 - dot(abs(xyz(p)), xyz(ones));
	s = lessThan(p, ToVec4(zero));
	v4_xyz(p) = xyz(p) + mul(xyz(s) * v2 - ToVec3(one), www(s));

	return p;
}

}

template<typename Real>
ILINE Real SNoise(Vec4_tpl<Real> v, Vec4_tpl<Real>* pGrad = nullptr)
{
	using namespace crydetail;

	const Real zero = convert<Real>(0.0);
	const Real one = convert<Real>(1.0);
	const Real two = convert<Real>(2.0);
	const Vec4_tpl<Real> C = Vec4_tpl<Real>(
	  convert<Real>(0.138196601125011),   // (5 - sqrt(5))/20  G4
	  convert<Real>(0.276393202250021),   // 2 * G4
	  convert<Real>(0.414589803375032),   // 3 * G4
	  convert<Real>(-0.447213595499958)); // -1 + 4 * G4

	// First corner
	const Real F4 = convert<Real>((sqrtf(5.0f) - 1.0f) / 4.0f);
	Vec4_tpl<Real> i = floor(v + ToVec4(dot(v, ToVec4(F4))));
	Vec4_tpl<Real> x0 = v - i + ToVec4(dot(i, xxxx(C)));

	// Other corners

	// Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
	Vec4_tpl<Real> i0;
	Vec3_tpl<Real> isX = step(yzw(x0), xxx(x0));
	Vec3_tpl<Real> isYZ = step(zww(x0), yyz(x0));
	//  i0.x = dot( isX, Vec3_tpl<Real>( 1.0 ) );
	i0.x = isX.x + isX.y + isX.z;
	v4_yzw(i0) = ToVec3(one) - isX;
	//  i0.y += dot( isYZ.xy, vec2( 1.0 ) );
	i0.y = i0.y + (isYZ.x + isYZ.y);
	v4_zw(i0) = zw(i0) + (ToVec2(one) - xy(isYZ));
	i0.z = i0.z + isYZ.z;
	i0.w = i0.w + (one - isYZ.z);

	// i0 now contains the unique values 0,1,2,3 in each channel
	Vec4_tpl<Real> i3 = saturate(i0);
	Vec4_tpl<Real> i2 = saturate(i0 - ToVec4(one));
	Vec4_tpl<Real> i1 = saturate(i0 - ToVec4(two));

	//  x0 = x0 - 0.0 + 0.0 * C.xxxx
	//  x1 = x0 - i1  + 1.0 * C.xxxx
	//  x2 = x0 - i2  + 2.0 * C.xxxx
	//  x3 = x0 - i3  + 3.0 * C.xxxx
	//  x4 = x0 - 1.0 + 4.0 * C.xxxx
	Vec4_tpl<Real> x1 = x0 - i1 + xxxx(C);
	Vec4_tpl<Real> x2 = x0 - i2 + yyyy(C);
	Vec4_tpl<Real> x3 = x0 - i3 + zzzz(C);
	Vec4_tpl<Real> x4 = x0 + wwww(C);

	// Permutations
	i = mod289(i);
	Real j0 = permute(permute(permute(permute(i.w) + i.z) + i.y) + i.x);
	Vec4_tpl<Real> j1 = permute(permute(permute(permute(
	                                              ToVec4(i.w) + Vec4_tpl<Real>(i1.w, i2.w, i3.w, one))
	                                            + ToVec4(i.z) + Vec4_tpl<Real>(i1.z, i2.z, i3.z, one))
	                                    + ToVec4(i.y) + Vec4_tpl<Real>(i1.y, i2.y, i3.y, one))
	                            + ToVec4(i.x) + Vec4_tpl<Real>(i1.x, i2.x, i3.x, one));

	// Gradients: 7x7x6 points over a cube, mapped onto a 4-cross polytope
	// 7*7*6 = 294, which is close to the ring size 17*17 = 289.
	Vec4_tpl<Real> ip = Vec4_tpl<Real>(
	  convert<Real>(1.0 / 294.0),
	  convert<Real>(1.0 / 49.0),
	  convert<Real>(1.0 / 7.0),
	  zero);

	Vec4_tpl<Real> p0 = grad4(j0, ip);
	Vec4_tpl<Real> p1 = grad4(j1.x, ip);
	Vec4_tpl<Real> p2 = grad4(j1.y, ip);
	Vec4_tpl<Real> p3 = grad4(j1.z, ip);
	Vec4_tpl<Real> p4 = grad4(j1.w, ip);

	// Normalise gradients
	Vec4_tpl<Real> norm = taylorInvSqrt(Vec4_tpl<Real>(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
	p0 = p0 * norm.x;
	p1 = p1 * norm.y;
	p2 = p2 * norm.z;
	p3 = p3 * norm.w;
	p4 = p4 * taylorInvSqrt(dot(p4, p4));

	// Mix contributions from the five corners
	const Real v06 = convert<Real>(0.6);
	const Real v49 = convert<Real>(49.0);
	const Vec3_tpl<Real> m10 = max(ToVec3(v06) - Vec3_tpl<Real>(dot(x0, x0), dot(x1, x1), dot(x2, x2)), ToVec3(zero));
	const Vec2_tpl<Real> m11 = max(ToVec2(v06) - Vec2_tpl<Real>(dot(x3, x3), dot(x4, x4)), ToVec2(zero));
	const Vec3_tpl<Real> m20 = mul(m10, m10);
	const Vec2_tpl<Real> m21 = mul(m11, m11);
	const Vec3_tpl<Real> m40 = mul(m20, m20);
	const Vec2_tpl<Real> m41 = mul(m21, m21);

	const Real d0 = dot(p0, x0);
	const Real d1 = dot(p1, x1);
	const Real d2 = dot(p2, x2);
	const Real d3 = dot(p3, x3);
	const Real d4 = dot(p4, x4);

	const Real result = v49 * (dot(m40, Vec3_tpl<Real>(d0, d1, d2)) + dot(m41, Vec2_tpl<Real>(d3, d4)));

	if (pGrad)
	{
		const Real vm6 = convert<Real>(-6.0f);
		const Vec3_tpl<Real> m30 = mul(m20, m10);
		const Vec2_tpl<Real> m31 = mul(m21, m11);
		Vec4_tpl<Real> grad = Vec4_tpl<Real>(zero, zero, zero, zero);

		grad = fma(x0 * d0, m30.x, grad);
		grad = fma(x1 * d1, m30.y, grad);
		grad = fma(x2 * d2, m30.z, grad);
		grad = fma(x3 * d3, m31.x, grad);
		grad = fma(x4 * d4, m31.y, grad);
		grad = mul(grad, vm6);

		grad = fma(p0, m40.x, grad);
		grad = fma(p1, m40.y, grad);
		grad = fma(p2, m40.z, grad);
		grad = fma(p3, m41.x, grad);
		grad = fma(p4, m41.y, grad);

		*pGrad = mul(grad, v49);
	}

	return result;
}
