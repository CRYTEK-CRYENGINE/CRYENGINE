// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>

///////////////////////////////////////////////////////////////////////////////
// Forward declarations                                                      //
///////////////////////////////////////////////////////////////////////////////

struct Ray;
template<typename F> struct Lineseg_tpl;
template<typename F> struct Triangle_tpl;

struct AABB;
template<typename F> struct OBB_tpl;

struct Sphere;
struct AAEllipsoid;
struct Ellipsoid;
struct Cone;

// Definitions

//
// Random geometry generation functions.
//

enum EGeomForm
{
	GeomForm_Vertices,
	GeomForm_Edges,
	GeomForm_Surface,
	GeomForm_Volume,

	GeomForm_COUNT
};
AUTO_TYPE_INFO(EGeomForm);

enum EGeomType
{
	GeomType_None,
	GeomType_BoundingBox,
	GeomType_Physics,
	GeomType_Render,
};
AUTO_TYPE_INFO(EGeomType);

struct PosNorm
{
	Vec3 vPos;
	Vec3 vNorm;

	PosNorm() {}
	PosNorm(type_zero)
		: vPos(ZERO), vNorm(ZERO) {}
	PosNorm(const Vec3& pos, const Vec3& norm)
		: vPos(pos), vNorm(norm) {}

	void zero()
	{
		vPos.zero();
		vNorm.zero();
	}

	//! Transform by matrix, etc.
	void operator<<=(Matrix34 const& mx)
	{
		vPos = mx * vPos;
		vNorm = Matrix33(mx) * vNorm;
	}
	void operator<<=(QuatTS const& qts)
	{
		vPos = qts * vPos;
		vNorm = qts.q * vNorm;
	}
	void operator<<=(DualQuat const& dq)
	{
		vPos = dq * vPos;
		vNorm = dq.nq * vNorm;
	}
};

struct RectF
{
	float x, y, w, h;
	RectF()
		: x(0), y(0), w(1), h(1)
	{
	}
	AUTO_STRUCT_INFO;
};

struct RectI
{
	int x, y, w, h;
	RectI()
		: x(0), y(0), w(1), h(1)
	{
	}
	RectI(int inX, int inY, int inW, int inH)
		: x(inX), y(inY), w(inW), h(inH)
	{
	}
	inline void Add(RectI rcAdd)
	{
		int x2 = x + w;
		int y2 = y + h;
		int x22 = rcAdd.x + rcAdd.w;
		int y22 = rcAdd.y + rcAdd.h;
		x = min(x, rcAdd.x);
		y = min(y, rcAdd.y);
		x2 = max(x2, x22);
		y2 = max(y2, y22);
		w = x2 - x;
		h = y2 - y;
	}
	inline void Add(int inX, int inY, int inW, int inH)
	{
		Add(RectI(inX, inY, inW, inH));
	}
};

struct Line
{

	Vec3 pointonline;
	Vec3 direction; //!< Caution: the direction is important for any intersection test.

	//! Default Line constructor (without initialisation).
	inline Line(void) {}
	inline Line(const Vec3& o, const Vec3& d) { pointonline = o; direction = d; }
	inline void operator()(const Vec3& o, const Vec3& d) { pointonline = o; direction = d; }

	~Line(void) {};
};

struct Ray
{

	Vec3 origin;
	Vec3 direction;

	//! Default Ray constructor (without initialisation).
	inline Ray(void) {}
	inline Ray(const Vec3& o, const Vec3& d) { origin = o; direction = d; }
	inline void operator()(const Vec3& o, const Vec3& d) { origin = o; direction = d; }

	~Ray(void) {};
};

template<typename F> struct Lineseg_tpl
{

	Vec3_tpl<F> start;
	Vec3_tpl<F> end;

	//! Default Lineseg constructor (without initialisation).
	inline Lineseg_tpl(void) {}
	inline Lineseg_tpl(const Vec3_tpl<F>& s, const Vec3_tpl<F>& e) { start = s; end = e; }
	inline void operator()(const Vec3_tpl<F>& s, const Vec3_tpl<F>& e) { start = s; end = e; }

	Vec3        GetPoint(F t) const                                    { return t * end + (F(1.0) - t) * start; }

	~Lineseg_tpl(void) {};
};

typedef Lineseg_tpl<float> Lineseg;
typedef Lineseg_tpl<real>  Linesegr;

struct Cone
{

	Vec3  mTip;
	Vec3  mDir;
	Vec3  mBase;

	float mHeight;
	float mBaseRadius;

	Cone() {}
	Cone(const Vec3& tip, const Vec3& dir, float height, float baseRadius)
		: mTip(tip), mDir(dir), mBase(tip + dir * height), mHeight(height), mBaseRadius(baseRadius) {}
};

//! Represents an axis-aligned bounding box
struct AABB
{

	Vec3 min;
	Vec3 max;

	//! Default AABB constructor (without initialisation).
	ILINE AABB()
	{}
	enum type_reset { RESET };
	//! AABB aabb(RESET) generates a reset aabb.
	ILINE AABB(type_reset)
	{ Reset(); }
	ILINE explicit AABB(float radius)
	{ max = Vec3(radius); min = -max; }
	ILINE explicit AABB(const Vec3& v)
	{ min = max = v; }
	ILINE AABB(const Vec3& v, float radius)
	{ Vec3 ext(radius); min = v - ext; max = v + ext; }
	ILINE AABB(const Vec3& vmin, const Vec3& vmax)
	{ min = vmin; max = vmax; }
	ILINE AABB(const AABB& aabb)
	{
		min.x = aabb.min.x;
		min.y = aabb.min.y;
		min.z = aabb.min.z;
		max.x = aabb.max.x;
		max.y = aabb.max.y;
		max.z = aabb.max.z;
	}
	inline AABB(const Vec3* points, int num)
	{
		Reset();
		for (int i = 0; i < num; i++)
			Add(points[i]);
	}

	//! Reset Bounding box before calculating bounds.
	//! These values ensure that Add() functions work correctly for Reset bbs, without additional comparisons.
	ILINE void Reset()
	{ min = Vec3(1e15f);  max = Vec3(-1e15f); }

	ILINE bool IsReset() const
	{ return min.x > max.x; }

	ILINE float IsResetSel(float ifReset, float ifNotReset) const
	{ return (float)__fsel(max.x - min.x, ifNotReset, ifReset); }

	//! Check if bounding box is empty (Zero volume).
	ILINE bool IsEmpty() const
	{ return min == max; }

	//! Check if bounding box has valid, non zero volume
	ILINE bool IsNonZero() const
	{ return min.x < max.x && min.y < max.y && min.z < max.z; }

	ILINE Vec3 GetCenter() const
	{ return (min + max) * 0.5f; }

	ILINE Vec3 GetSize() const
	{ return (max - min) * IsResetSel(0.0f, 1.0f); }

	ILINE float GetRadius() const
	{ return IsResetSel(0.0f, (max - min).GetLength() * 0.5f); }

	ILINE float GetRadiusSqr() const
	{ return IsResetSel(0.0f, ((max - min) * 0.5f).GetLengthSquared()); }

	ILINE float GetVolume() const
	{ return IsResetSel(0.0f, (max.x - min.x) * (max.y - min.y) * (max.z - min.z)); }

	ILINE void Add(const Vec3& v)
	{
		min.CheckMin(v);
		max.CheckMax(v);
	}

	inline void Add(const Vec3& v, float radius)
	{
		Vec3 ext(radius);
		min.CheckMin(v - ext);
		max.CheckMax(v + ext);
	}

	ILINE void Add(const AABB& bb)
	{
		min.CheckMin(bb.min);
		max.CheckMax(bb.max);
	}

	inline void Move(const Vec3& v)
	{
		const float moveMult = IsResetSel(0.0f, 1.0f);
		const Vec3 vMove = v * moveMult;
		min += vMove;
		max += vMove;
	}

	inline void Expand(Vec3 const& v)
	{
		if (!IsReset())
		{
			min -= v;
			max += v;
		}
	}

	//! Augment the box on all sides by a box.
	inline void Augment(AABB const& bb)
	{
		if (!IsReset() && !bb.IsReset())
		{
			Add(min + bb.min);
			Add(max + bb.max);
		}
	}

	void ClipToBox(AABB const& bb)
	{
		min.CheckMax(bb.min);
		max.CheckMin(bb.max);
	}

	void ClipMoveToBox(AABB const& bb)
	{
		for (int a = 0; a < 3; a++)
		{
			if (max[a] - min[a] > bb.max[a] - bb.min[a])
			{
				min[a] = bb.min[a];
				max[a] = bb.max[a];
			}
			else if (min[a] < bb.min[a])
			{
				max[a] += bb.min[a] - min[a];
				min[a] = bb.min[a];
			}
			else if (max[a] > bb.max[a])
			{
				min[a] += bb.max[a] - max[a];
				max[a] = bb.max[a];
			}
		}
	}

	//! Check if this bounding box overlap with bounding box of sphere.
	bool IsOverlapSphereBounds(const Vec3& pos, float radius) const
	{
		CRY_MATH_ASSERT(min.IsValid());
		CRY_MATH_ASSERT(max.IsValid());
		CRY_MATH_ASSERT(pos.IsValid());

		if (pos.x > min.x && pos.x < max.x && pos.y > min.y && pos.y < max.y && pos.z > min.z && pos.z < max.z)
			return true;

		if (pos.x + radius < min.x) return false;
		if (pos.y + radius < min.y) return false;
		if (pos.z + radius < min.z) return false;
		if (pos.x - radius > max.x) return false;
		if (pos.y - radius > max.y) return false;
		if (pos.z - radius > max.z) return false;
		return true;
	}

	//! Check if this bounding box contain sphere within itself.
	bool IsContainSphere(const Vec3& pos, float radius) const
	{
		CRY_MATH_ASSERT(min.IsValid());
		CRY_MATH_ASSERT(max.IsValid());
		CRY_MATH_ASSERT(pos.IsValid());
		if (pos.x - radius < min.x) return false;
		if (pos.y - radius < min.y) return false;
		if (pos.z - radius < min.z) return false;
		if (pos.x + radius > max.x) return false;
		if (pos.y + radius > max.y) return false;
		if (pos.z + radius > max.z) return false;
		return true;
	}

	//! Check if this bounding box contains a point within itself.
	bool IsContainPoint(const Vec3& pos) const
	{
		CRY_MATH_ASSERT(min.IsValid());
		CRY_MATH_ASSERT(max.IsValid());
		CRY_MATH_ASSERT(pos.IsValid());
		if (pos.x < min.x) return false;
		if (pos.y < min.y) return false;
		if (pos.z < min.z) return false;
		if (pos.x > max.x) return false;
		if (pos.y > max.y) return false;
		if (pos.z > max.z) return false;
		return true;
	}

	float GetDistanceSqr(Vec3 const& v) const
	{
		Vec3 vNear = v;
		vNear.CheckMax(min);
		vNear.CheckMin(max);
		return vNear.GetSquaredDistance(v);
	}

	float GetDistance(Vec3 const& v) const
	{
		return sqrt(GetDistanceSqr(v));
	}

	float GetFarDistance(Vec3 const& v) const
	{
		Vec3 vFar = v;
		vFar.CheckMin(min);
		vFar.CheckMax(max);
		return vFar.GetDistance(v);
	}

	bool ContainsBox(AABB const& b) const
	{
		CRY_MATH_ASSERT(min.IsValid());
		CRY_MATH_ASSERT(max.IsValid());
		CRY_MATH_ASSERT(b.min.IsValid());
		CRY_MATH_ASSERT(b.max.IsValid());
		return min.x <= b.min.x && min.y <= b.min.y && min.z <= b.min.z
		       && max.x >= b.max.x && max.y >= b.max.y && max.z >= b.max.z;
	}

	bool ContainsBox2D(AABB const& b) const
	{
		CRY_MATH_ASSERT(min.IsValid());
		CRY_MATH_ASSERT(max.IsValid());
		CRY_MATH_ASSERT(b.min.IsValid());
		CRY_MATH_ASSERT(b.max.IsValid());
		return min.x <= b.min.x && min.y <= b.min.y
		       && max.x >= b.max.x && max.y >= b.max.y;
	}

	//! Check two bounding boxes for intersection.
	inline bool IsIntersectBox(const AABB& b) const
	{
		CRY_MATH_ASSERT(min.IsValid());
		CRY_MATH_ASSERT(max.IsValid());
		CRY_MATH_ASSERT(b.min.IsValid());
		CRY_MATH_ASSERT(b.max.IsValid());
		// Check for intersection on X axis.
		if ((min.x > b.max.x) || (b.min.x > max.x)) return false;
		// Check for intersection on Y axis.
		if ((min.y > b.max.y) || (b.min.y > max.y)) return false;
		// Check for intersection on Z axis.
		if ((min.z > b.max.z) || (b.min.z > max.z)) return false;
		// Boxes overlap in all 3 axises.
		return true;
	}

	/*!
	 * calculate the new bounds of a transformed AABB
	 *
	 * Example:
	 * AABB aabb = AABB::CreateAABBfromOBB(m34,aabb);
	 *
	 * return values:
	 *  expanded AABB in world-space
	 */
	ILINE void SetTransformedAABB(const Matrix34& m34, const AABB& aabb)
	{
		if (aabb.IsReset())
		{
			Reset();
		}
		else
		{
			Matrix34 m34a = m34.GetMagnitude();
			m34a.SetTranslation(Vec3(0.0f));

			Vec3 sz = m34a.TransformPoint((aabb.max - aabb.min) * 0.5f);
			Vec3 pos = m34.TransformPoint((aabb.max + aabb.min) * 0.5f);
			min = pos - sz;
			max = pos + sz;
		}
	}
	ILINE static AABB CreateTransformedAABB(const Matrix34& m34, const AABB& aabb)
	{ AABB taabb; taabb.SetTransformedAABB(m34, aabb); return taabb;   }

	ILINE void SetTransformedAABB(const QuatT& qt, const AABB& aabb)
	{
		if (aabb.IsReset())
		{
			Reset();
		}
		else
		{
			SetTransformedAABB(Matrix34(qt), aabb);
		}
	}
	ILINE static AABB CreateTransformedAABB(const QuatT& qt, const AABB& aabb)
	{ AABB taabb; taabb.SetTransformedAABB(qt, aabb); return taabb;  }

	//! Create an AABB using just the extensions of the OBB and ignore the orientation.
	template<typename F>
	ILINE void SetAABBfromOBB(const OBB_tpl<F>& obb)
	{ min = obb.c - obb.h; max = obb.c + obb.h; }
	template<typename F>
	ILINE static AABB CreateAABBfromOBB(const OBB_tpl<F>& obb)
	{ return AABB(obb.c - obb.h, obb.c + obb.h); }

	/*!
	 * converts an OBB into an tight fitting AABB
	 *
	 * Example:
	 * AABB aabb = AABB::CreateAABBfromOBB(wposition,obb,1.0f);
	 *
	 * return values:
	 *  expanded AABB in world-space
	 */
	template<typename F>
	ILINE void SetAABBfromOBB(const Vec3& wpos, const OBB_tpl<F>& obb, f32 scaling = 1.0f)
	{
		Vec3 pos = obb.m33 * obb.c * scaling + wpos;
		Vec3 sz = obb.m33.GetFabs() * obb.h * scaling;
		min = pos - sz;
		max = pos + sz;
	}
	template<typename F>
	ILINE static AABB CreateAABBfromOBB(const Vec3& wpos, const OBB_tpl<F>& obb, f32 scaling = 1.0f)
	{ AABB taabb; taabb.SetAABBfromOBB(wpos, obb, scaling); return taabb;   }

	/* Converts a Cone into a tight fitting AABB */
	ILINE static AABB CreateAABBfromCone(const Cone& c)
	{
		// Construct AABB for cone base
		Vec3 baseX = Vec3(1.f - c.mDir.x * c.mDir.x, c.mDir.x * c.mDir.y, c.mDir.x * c.mDir.z).GetNormalized() * c.mBaseRadius;
		Vec3 baseY = Vec3(c.mDir.y * c.mDir.x, 1.f - c.mDir.y * c.mDir.y, c.mDir.y * c.mDir.z).GetNormalized() * c.mBaseRadius;
		Vec3 baseZ = Vec3(c.mDir.z * c.mDir.x, c.mDir.z * c.mDir.y, 1.f - c.mDir.z * c.mDir.z).GetNormalized() * c.mBaseRadius;

		Vec3 aabbMax = Vec3(baseX.x, baseY.y, baseZ.z).abs();
		Vec3 aabbMin = -aabbMax;

		AABB result(aabbMin, aabbMax);
		result.Move(c.mBase);

		// add tip
		result.Add(c.mTip);
		return result;
	}

	AUTO_STRUCT_INFO;
};

ILINE bool IsEquivalent(const AABB& a, const AABB& b, float epsilon = VEC_EPSILON)
{
	return IsEquivalent(a.min, b.min, epsilon) && IsEquivalent(a.max, b.max, epsilon);
}

//! Exposes oriented bounding box functionality
template<typename F> struct OBB_tpl
{

	Matrix33 m33; //!< Orientation vectors.
	Vec3     h;   //!< Half-length-vector.
	Vec3     c;   //!< Center of obb.

	//! Default OBB constructor (without initialisation).
	inline OBB_tpl() {}

	ILINE void              SetOBB(const Matrix33& matrix, const Vec3& hlv, const Vec3& center) { m33 = matrix; h = hlv; c = center; }
	ILINE static OBB_tpl<F> CreateOBB(const Matrix33& m33, const Vec3& hlv, const Vec3& center) { OBB_tpl<f32> obb; obb.m33 = m33; obb.h = hlv; obb.c = center; return obb; }

	ILINE void              SetOBBfromAABB(const Matrix33& mat33, const AABB& aabb)
	{
		m33 = mat33;
		h = (aabb.max - aabb.min) * 0.5f; //!< Calculate the half-length-vectors.
		c = (aabb.max + aabb.min) * 0.5f; //!< The center is relative to the PIVOT.
	}
	ILINE void SetOBBfromAABB(const Quat& q, const AABB& aabb)
	{
		m33 = Matrix33(q);
		h = (aabb.max - aabb.min) * 0.5f; //!< Calculate the half-length-vectors.
		c = (aabb.max + aabb.min) * 0.5f; //!< The center is relative to the PIVOT.
	}
	ILINE static OBB_tpl<F> CreateOBBfromAABB(const Matrix33& m33, const AABB& aabb) { OBB_tpl<f32> obb; obb.SetOBBfromAABB(m33, aabb); return obb;  }
	ILINE static OBB_tpl<F> CreateOBBfromAABB(const Quat& q, const AABB& aabb)       { OBB_tpl<f32> obb; obb.SetOBBfromAABB(q, aabb); return obb;  }

	~OBB_tpl(void) {};
};

template<typename F> struct Triangle_tpl
{

	Vec3_tpl<F> v0, v1, v2;

	//! Default Lineseg constructor (without initialisation).
	ILINE Triangle_tpl(void) {}
	ILINE Triangle_tpl(type_zero) { v0 = ZERO; v1 = ZERO; v2 = ZERO; }
	ILINE Triangle_tpl(const Vec3_tpl<F>& a, const Vec3_tpl<F>& b, const Vec3_tpl<F>& c) { v0 = a; v1 = b; v2 = c; }
	ILINE void operator()(const Vec3_tpl<F>& a, const Vec3_tpl<F>& b, const Vec3_tpl<F>& c) { v0 = a; v1 = b; v2 = c; }

	~Triangle_tpl(void) {};

	Vec3_tpl<F> GetNormal() const
	{
		return ((v1 - v0) ^ (v2 - v0)).GetNormalized();
	}

	F GetArea() const
	{
		return 0.5f * (v1 - v0).Cross(v2 - v0).GetLength();
	}
};

template<typename F> struct Quad_tpl
{
	Vec3_tpl<F> vCorners[4];

	ILINE Quad_tpl(void) {}
	ILINE Quad_tpl(type_zero) { vCorners[0] = ZERO; vCorners[1] = ZERO; vCorners[2] = ZERO; vCorners[3] = ZERO; }
	ILINE Quad_tpl(const Vec3_tpl<F>& a, const Vec3_tpl<F>& b, const Vec3_tpl<F>& c, const Vec3_tpl<F>& d) { vCorners[0] = a; vCorners[1] = b; vCorners[2] = c; vCorners[3] = d; }
	ILINE void operator()(const Vec3_tpl<F>& a, const Vec3_tpl<F>& b, const Vec3_tpl<F>& c, const Vec3_tpl<F>& d) { vCorners[0] = a; vCorners[1] = b; vCorners[2] = c; vCorners[3] = d; }

	AABB GetBounds() const
	{
		AABB bounds(AABB::RESET);
		bounds.Add(vCorners[0]);
		bounds.Add(vCorners[1]);
		bounds.Add(vCorners[2]);
		bounds.Add(vCorners[3]);
		return bounds;
	}
};

typedef Quad_tpl<float> Quad;

struct Sphere
{

	Vec3  center;
	float radius;

	Sphere() {}
	Sphere(const Vec3& c, float r) : center(c), radius(r) {}
	void operator()(const Vec3& c, float r) { center = c; radius = r; }
};

struct AAEllipsoid
{

	Vec3 center;
	Vec3 radius_vec;

	//! Default AAEllipsoid constructor (without initialisation).
	inline AAEllipsoid(void) {}
	inline AAEllipsoid(const Vec3& c, const Vec3& rv) { radius_vec = rv; center = c; }
	inline void operator()(const Vec3& c, const Vec3& rv) { radius_vec = rv; center = c; }

	~AAEllipsoid(void) {};
};

struct Ellipsoid
{

	Matrix34 ExtensionPos;

	//! Default Ellipsoid constructor (without initialisation).
	inline Ellipsoid(void) {}
	inline Ellipsoid(const Matrix34& ep) { ExtensionPos = ep; }
	inline void operator()(const Matrix34& ep) { ExtensionPos = ep; }

	~Ellipsoid(void) {};
};

template<class Num>
struct TRect_tpl
{
	typedef Vec2_tpl<Num> Vec;

	Vec Min, Max;

	TRect_tpl() = default;
	TRect_tpl(Num x1, Num y1, Num x2, Num y2) : Min(x1, y1), Max(x2, y2) {}
	TRect_tpl(const Vec& min, const Vec& max) : Min(min), Max(max) {}

	TRect_tpl(const TRect_tpl& rc) = default;
	TRect_tpl(TRect_tpl&& rc) = default;
	TRect_tpl &operator=(const TRect_tpl& rc) = default;
	TRect_tpl &operator=(TRect_tpl&& rc) = default;

	inline TRect_tpl<Num> operator*(Num k) const
	{
		return TRect_tpl<Num>(Min.x * k, Min.y * k, Max.x * k, Max.y * k);
	}
	inline TRect_tpl<Num> operator/(Num k) const
	{
		return TRect_tpl<Num>(Min.x / k, Min.y / k, Max.x / k, Max.y / k);
	}

	inline bool            IsEmpty() const                         { return Max.x < Min.x && Max.y < Min.y; }
	inline TRect_tpl<Num>& SetEmpty()                              { Max = Vec(-1, -1); Min = Vec(0, 0); return *this; }

	inline Vec             GetDim() const                          { return Max - Min; }
	inline Num             GetWidth() const                        { return Max.x - Min.x; }
	inline Num             GetHeight() const                       { return Max.y - Min.y; }

	inline bool            IsEqual(const TRect_tpl<Num>& rc) const { return Min.x == rc.Min.x && Min.y == rc.Min.y && Max.x == rc.Max.x && Max.y == rc.Max.y; }

	inline bool            InRect(const TRect_tpl<Num>& rc) const  { return rc.Min.x >= Min.x && rc.Max.x <= Max.x && rc.Min.y >= Min.y && rc.Max.y <= Max.y; }
	inline bool            InRect(Vec pt) const                    { return pt.x >= Min.x && pt.x <= Max.x && pt.y >= Min.y && pt.y <= Max.y; }
	inline Vec&            IntoRect(Vec& pt) const
	{
		if (pt.x < Min.x) pt.x = Min.x;
		else if (pt.x > Max.x) pt.x = Max.x;
		if (pt.y < Min.y) pt.y = Min.y;
		else if (pt.y > Max.y) pt.y = Max.y;
		return pt;
	}

	inline bool operator==(const TRect_tpl& rc)const { return this->IsEqual(rc); }
	inline bool operator!=(const TRect_tpl& rc)const { return !this->IsEqual(rc); }

	inline bool Intersects(const TRect_tpl<Num>& rc) const
	{
		return !IsEmpty() && !rc.IsEmpty() &&
		       !(Min.x > rc.Max.x || Max.x < rc.Min.x ||
		         Min.y > rc.Max.y || Max.y < rc.Min.y);
	}

	inline TRect_tpl<Num>& DoUnite(const TRect_tpl<Num>& rc)
	{
		if (IsEmpty()) { Min = rc.Min; Max = rc.Max; return *this; }
		if (rc.IsEmpty()) return *this;
		if (Min.x > rc.Min.x) Min.x = rc.Min.x;
		if (Min.y > rc.Min.y) Min.y = rc.Min.y;
		if (Max.x < rc.Max.x) Max.x = rc.Max.x;
		if (Max.y < rc.Max.y) Max.y = rc.Max.y;
		return *this;
	}

	inline TRect_tpl<Num>& DoIntersect(const TRect_tpl<Num>& rc)
	{
		if (IsEmpty()) return *this;
		if (rc.IsEmpty()) return SetEmpty();
		if (Min.x < rc.Min.x) Min.x = rc.Min.x;
		if (Min.y < rc.Min.y) Min.y = rc.Min.y;
		if (Max.x > rc.Max.x) Max.x = rc.Max.x;
		if (Max.y > rc.Max.y) Max.y = rc.Max.y;

		if (Min.x == Max.x || Min.y == Max.y) return SetEmpty();

		return *this;
	}

	inline TRect_tpl<Num> GetSubRect(const TRect_tpl<Num>& rc) const
	{
		if (IsEmpty()) return *this;
		if (rc.IsEmpty()) return rc;
		return TRect_tpl<Num>(Min.x + rc.Min.x * GetWidth(),
		                      Min.y + rc.Min.y * GetHeight(),
		                      Min.x + rc.Max.x * GetWidth(),
		                      Min.y + rc.Max.y * GetHeight());
	}

	inline TRect_tpl<Num> GetSubRectInv(const TRect_tpl<Num>& rcSub) const
	{
		if (IsEmpty()) return *this;
		if (rcSub.IsEmpty()) return rcSub;
		return TRect_tpl((rcSub.Min.x - Min.x) / GetWidth(),
		                 (rcSub.Min.y - Min.y) / GetHeight(),
		                 (rcSub.Max.x - Min.x) / GetWidth(),
		                 (rcSub.Max.y - Min.y) / GetHeight());
	}
};

typedef TRect_tpl<float>  Rectf;
typedef TRect_tpl<int>    Recti;

typedef Triangle_tpl<f32> Triangle;
typedef Triangle_tpl<f64> Triangle_f64;
typedef OBB_tpl<f32>      OBB;

//! Manage linear and rotational 3D velocity in a class.
class Velocity3
{
public:
	Vec3 vLin, vRot;

	Velocity3()
	{}
	Velocity3(type_zero)
		: vLin(ZERO), vRot(ZERO) {}
	Velocity3(Vec3 const& vLin)
		: vLin(vLin), vRot(ZERO) {}
	Velocity3(Vec3 const& vLin, Vec3 const& vRot)
		: vLin(vLin), vRot(vRot) {}

	void FromDelta(QuatT const& loc0, QuatT const& loc1, float fTime)
	{
		float fInvT = 1.f / fTime;
		vLin = (loc1.t - loc0.t) * fInvT;
		vRot = 2.0f * Quat::log(loc1.q * loc0.q.GetInverted()) * fInvT;
	}

	Vec3 VelocityAt(Vec3 const& vPosRel) const
	{ return vLin + (vRot % vPosRel); }

	void operator+=(Velocity3 const& vv)
	{
		vLin += vv.vLin;
		vRot += vv.vRot;
	}
	void operator*=(float f)
	{
		vLin *= f;
		vRot *= f;
	}
	void Interp(Velocity3 const& vv, float f)
	{
		vLin += (vv.vLin - vLin) * f;
		vRot += (vv.vRot - vRot) * f;
	}
};

#include "Cry_GeoDistance.h"
#include "Cry_GeoOverlap.h"
#include "Cry_GeoIntersect.h"

//this is some special engine stuff, should be moved to a better location

// for bbox's checks and calculations
#define MAX_BB +99999.0f
#define MIN_BB -99999.0f

//! Checks if this has been set to minBB.
inline bool IsMinBB(const Vec3& v)
{
	if (v.x <= MIN_BB) return (true);
	if (v.y <= MIN_BB) return (true);
	if (v.z <= MIN_BB) return (true);
	return (false);
}

//! Checks if this has been set to maxBB.
inline bool IsMaxBB(const Vec3& v)
{
	if (v.x >= MAX_BB) return (true);
	if (v.y >= MAX_BB) return (true);
	if (v.z >= MAX_BB) return (true);
	return (false);
}

inline Vec3 SetMaxBB(void) { return Vec3(MAX_BB, MAX_BB, MAX_BB); }
inline Vec3 SetMinBB(void) { return Vec3(MIN_BB, MIN_BB, MIN_BB); }

inline void AddToBounds(const Vec3& v, Vec3& mins, Vec3& maxs)
{
	if (v.x < mins.x) mins.x = v.x;
	if (v.x > maxs.x) maxs.x = v.x;
	if (v.y < mins.y) mins.y = v.y;
	if (v.y > maxs.y) maxs.y = v.y;
	if (v.z < mins.z) mins.z = v.z;
	if (v.z > maxs.z) maxs.z = v.z;
}

//! Calculate the area of a polygon giving a list of vertices and normal.
inline float CalcArea(const Vec3* vertices, int numvertices, const Vec3& normal)
{
	Vec3 csum(0, 0, 0);

	int n = numvertices;
	for (int i = 0, j = 1; i <= n - 2; i++, j++)
	{
		csum.x += vertices[i].y * vertices[j].z - vertices[i].z * vertices[j].y;
		csum.y += vertices[i].z * vertices[j].x - vertices[i].x * vertices[j].z;
		csum.z += vertices[i].x * vertices[j].y - vertices[i].y * vertices[j].x;
	}

	csum.x += vertices[n - 1].y * vertices[0].z - vertices[n - 1].z * vertices[0].y;
	csum.y += vertices[n - 1].z * vertices[0].x - vertices[n - 1].x * vertices[0].z;
	csum.z += vertices[n - 1].x * vertices[0].y - vertices[n - 1].y * vertices[0].x;

	float area = 0.5f * (float)fabs(normal | csum);
	return (area);
}