// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "Line.h"

namespace Designer
{
static Vec3 s_baseAxis[] =
{
	Vec3(0,  0,  1),  Vec3(1,  0,  0), Vec3(0, -1, 0), // floor
	Vec3(0,  0,  -1), Vec3(-1, 0,  0), Vec3(0, 1, 0),  // ceiling
	Vec3(1,  0,  0),  Vec3(0,  1,  0), Vec3(0, 0, -1), // west wall
	Vec3(-1, 0,  0),  Vec3(0,  -1, 0), Vec3(0, 0, -1), // east wall
	Vec3(0,  1,  0),  Vec3(-1, 0,  0), Vec3(0, 0, -1), // south wall
	Vec3(0,  -1, 0),  Vec3(1,  0,  0), Vec3(0, 0, -1), // north wall
};

template<class T>
struct SBrushPlane
{
	~SBrushPlane()
	{
		if (m_pBasisTM)
			delete m_pBasisTM;
	}

	SBrushPlane() : m_pBasisTM(NULL)
	{
	}

	SBrushPlane(const SBrushPlane<T>& p)
		: normal(p.normal),
		distance(p.distance), m_pBasisTM(NULL)
	{
	}

	SBrushPlane(const Vec3_tpl<T>& _normal, T _dist) : m_pBasisTM(NULL)
	{
		Set(_normal, _dist);
	}

	SBrushPlane(const Vec3_tpl<T>& v0, const Vec3_tpl<T>& v1, const Vec3_tpl<T>& v2) : m_pBasisTM(NULL)
	{
		normal = (v2 - v1) ^ (v0 - v1);
		normal.Normalize();
		if (std::abs(normal.x) < kDesignerEpsilon)
			normal.x = 0;
		if (std::abs(normal.y) < kDesignerEpsilon)
			normal.y = 0;
		if (std::abs(normal.z) < kDesignerEpsilon)
			normal.z = 0;
		normal.Normalize();
		distance = -normal.Dot(v2);
	}

	SBrushPlane& operator=(const SBrushPlane& plane)
	{
		distance = plane.Distance();
		normal = plane.Normal();
		InvalidateBasis();
		return *this;
	}

	void CalcTextureAxis(Vec3_tpl<T>& xv, Vec3_tpl<T>& yv) const
	{
		size_t bestaxis;
		T dot, best;

		best = -1;
		bestaxis = 0;

		for (size_t i(0); i < 6; ++i)
		{
			dot = normal.Dot(s_baseAxis[i * 3]);
			if (dot > best)
			{
				best = dot;
				bestaxis = i;
			}
		}

		xv = s_baseAxis[bestaxis * 3 + 1];
		yv = s_baseAxis[bestaxis * 3 + 2];
	}

	T Distance(const Vec3_tpl<T>& p) const
	{
		return normal.Dot(p) + distance;
	}

	void Invert()
	{
		normal = -normal;
		distance = -distance;
		InvalidateBasis();
	}

	SBrushPlane<T> GetInverted() const
	{
		SBrushPlane<T> invPlane = *this;
		invPlane.Invert();
		return invPlane;
	}

	bool IsSameFacing(const Vec3_tpl<T>& n) const
	{
		return normal.Dot(n) >= 0;
	}

	bool IsSameFacing(const SBrushPlane& p) const
	{
		return IsSameFacing(p.normal);
	}

	bool IsEquivalent(const SBrushPlane& p) const
	{
		return normal.IsEquivalent(p.normal, kDesignerEpsilon) && std::abs(p.distance - distance) < kDesignerEpsilon;
	}

	void CreatePoly(std::vector<Vec3_tpl<T>>& outputlist) const
	{
		int i, x;
		T max, v;
		Vec3_tpl<T> org, vright, vup;

		if (!outputlist.empty())
			outputlist.clear();

		max = -99999;
		x = -1;
		for (i = 0; i < 3; ++i)
		{
			v = (T)std::abs(normal[i]);
			if (v > max)
			{
				x = i;
				max = v;
			}
		}
		if (x == -1)
		{
			CLogFile::WriteLine("Error: SBrushPlane::CreatePoly: no axis found");
			return;
		}

		vup(0, 0, 0);
		if (x != 2)
			vup = Vec3_tpl<T>(0, 0, 1);
		else
			vup = Vec3_tpl<T>(1, 0, 0);

		v = normal | vup;
		vup += normal * (-v);
		vup.Normalize();

		org = normal * (-distance);
		vright = normal ^ vup;

		vup *= 32768.0f;
		vright *= 32768.0f;

		Vec3_tpl<T> pt;
		pt = org - vright;
		pt += vup;
		outputlist.push_back(pt);

		pt = org + vright;
		pt += vup;
		outputlist.push_back(pt);

		pt = org + vright;
		pt -= vup;
		outputlist.push_back(pt);

		pt = org - vright;
		pt -= vup;
		outputlist.push_back(pt);
	}

	bool ClipByPlane(const SBrushPlane& split, const std::vector<Vec3_tpl<T>>& inputlist, std::vector<Vec3_tpl<T>>& outputlist) const
	{
		int inputsize = (int)inputlist.size();
		std::vector<char> sign;
		sign.resize(inputsize);

		T dot_result = std::abs(split.normal.dot(normal));

		if (dot_result > 0.9999999f)
			return false;

		if (!outputlist.empty())
			outputlist.clear();

		for (int i = 0; i < inputsize; ++i)
		{
			T dist = split.Distance(inputlist[i]);

			if (dist > kDesignerEpsilon)
			{
				sign[i] = 1;
			}
			else if (dist < -kDesignerEpsilon)
			{
				sign[i] = -1;
			}
			else
			{
				sign[i] = 0;
			}
		}

		for (int i = 0; i < inputsize; ++i)
		{
			int nexti = (i + 1) % inputsize;
			const Vec3_tpl<T>& p0 = inputlist[i];

			if (sign[i] == 0)
			{
				outputlist.push_back(p0);
				continue;
			}

			if (sign[i] == -1)
				outputlist.push_back(p0);

			if (sign[nexti] == 0 || sign[i] == sign[nexti])
				continue;

			const Vec3_tpl<T>& p1 = inputlist[nexti];

			Vec3_tpl<T> p01;
			if (split.HitTest(p0, p1, kDesignerEpsilon, NULL, &p01))
				outputlist.push_back(p01);
		}

		return true;
	}

	bool HitTest(const SBrushRay<T>& ray, T* tout = NULL, Vec3_tpl<T>* vout = NULL) const
	{
		T denominator = (normal | ray.direction);

		if (denominator == 0)
			return false;

		T t = -((normal | ray.origin) + distance) / denominator;

		if (tout)
			*tout = t;

		if (vout)
			*vout = ray.origin + t * ray.direction;

		return true;
	}

	void Set(const Vec3_tpl<T>& _normal, T _distance)
	{
		if (_normal.IsValid())
			normal = _normal.GetNormalized();
		if (NumberValid(_distance))
			distance = _distance;
		InvalidateBasis();
	}

	Vec3_tpl<T> MirrorVertex(const BrushVec3& pos) const
	{
		T distance = Distance(pos);
		if (std::abs(distance) < kDesignerEpsilon)
			return pos;
		return pos - Normal() * 2 * distance;
	}

	Vec3_tpl<T> MirrorDirection(const BrushVec3& dir) const
	{
		BrushVec3 vertexOnMirrorPlane(0, 0, 0);
		HitTest(BrushRay(Vec3_tpl<T>(0, 0, 0), normal), NULL, &vertexOnMirrorPlane);
		assert(std::abs(Distance(vertexOnMirrorPlane)) < kDesignerEpsilon);
		return MirrorVertex(dir + vertexOnMirrorPlane) - vertexOnMirrorPlane;
	}

	bool IntersectionLine(const SBrushPlane<T>& plane, SBrushLine3D<T>& outLine) const
	{
		Vec3_tpl<T> vDir = normal.Cross(plane.normal).GetNormalized();

		Matrix33_tpl<T> m;
		m.SetZero();

		if (std::abs(vDir.z) > std::abs(vDir.y) && std::abs(vDir.z) > std::abs(vDir.x))
		{
			m.m00 = normal.x;
			m.m01 = normal.y;
			m.m10 = plane.normal.x;
			m.m11 = plane.normal.y;
			m.Invert();
			outLine.m_Pivot = Vec3_tpl<T>(-m.m00 * distance - m.m01 * plane.distance, -m.m10 * distance - m.m11 * plane.distance, 0);
		}
		else if (std::abs(vDir.y) > std::abs(vDir.x) && std::abs(vDir.y) > std::abs(vDir.z))
		{
			m.m00 = normal.x;
			m.m01 = normal.z;
			m.m10 = plane.normal.x;
			m.m11 = plane.normal.z;
			m.Invert();
			outLine.m_Pivot = Vec3_tpl<T>(-m.m00 * distance - m.m01 * plane.distance, 0, -m.m10 * distance - m.m11 * plane.distance);
		}
		else if (std::abs(vDir.x) > kDesignerEpsilon)
		{
			m.m00 = normal.y;
			m.m01 = normal.z;
			m.m10 = plane.normal.y;
			m.m11 = plane.normal.z;
			m.Invert();
			outLine.m_Pivot = Vec3_tpl<T>(0, -m.m00 * distance - m.m01 * plane.distance, -m.m10 * distance - m.m11 * plane.distance);
		}
		else
		{
			return false;
		}

		outLine.m_Dir = vDir;

		return true;
	}

	const Vec3_tpl<T>& Normal() const
	{
		return normal;
	}

	T Distance() const
	{
		return distance;
	}

	Vec2_tpl<T> W2P(const Vec3_tpl<T>& worldPos) const
	{
		Vec3_tpl<T> v = worldPos * GetBasisTM();
		return Vec2_tpl<T>(v.x, v.y);
	}

	Vec3_tpl<T> P2W(const Vec2_tpl<T>& planePos) const
	{
		return GetBasisTM() * Vec3_tpl<T>(planePos.x, planePos.y, -distance);
	}

	SBrushPlane<T> MirrorPlane(const SBrushPlane<T>& plane) const
	{
		Vec3_tpl<T> vertexOnMirrorPlane;
		Vec3_tpl<T> vertexOnPlane;

		HitTest(BrushRay(Vec3_tpl<T>(0, 0, 0), normal), NULL, &vertexOnMirrorPlane);
		plane.HitTest(BrushRay(Vec3_tpl<T>(0, 0, 0), plane.normal), NULL, &vertexOnPlane);

		assert(std::abs(Distance(vertexOnMirrorPlane)) < kDesignerEpsilon);
		assert(std::abs(plane.Distance(vertexOnPlane)) < kDesignerEpsilon);

		BrushVec3 mirroredNormal = MirrorDirection(plane.Normal());
		T distance = -MirrorVertex(vertexOnPlane).Dot(mirroredNormal);

		return SBrushPlane<T>(mirroredNormal, distance);
	}

private:

	Vec3_tpl<T>              normal;
	T                        distance;
	mutable Matrix33_tpl<T>* m_pBasisTM;

	void                     InvalidateBasis() const
	{
		if (!m_pBasisTM)
			return;
		delete m_pBasisTM;
		m_pBasisTM = NULL;
	}

	const Matrix33_tpl<T>& GetBasisTM() const
	{
		if (!m_pBasisTM)
		{
			m_pBasisTM = new Matrix33_tpl<T>;
			UpdateBasis();
		}
		return *m_pBasisTM;
	}

	void UpdateBasis() const
	{
		if (!normal.IsValid())
			return;

		assert(m_pBasisTM);

		if (normal.IsZero())
		{
			m_pBasisTM->SetIdentity();
			return;
		}

		Vec3_tpl<T> u, v;
		if (std::abs(normal.x) >= std::abs(normal.y))
		{
			T invLength = 1 / sqrtf(normal.x * normal.x + normal.z * normal.z);
			u.x = normal.z * invLength;
			u.y = 0;
			u.z = -normal.x * invLength;
		}
		else
		{
			T invLength = 1 / sqrtf(normal.y * normal.y + normal.z * normal.z);
			u.x = 0;
			u.y = normal.z * invLength;
			u.z = -normal.y * invLength;
		}
		v = u.Cross(normal);

		* m_pBasisTM = Matrix33_tpl<T>(u, v, normal);
	}
};

typedef SBrushPlane<BrushFloat> BrushPlane;

static SBrushPlane<float> ToFloatPlane(const BrushPlane& plane)
{
	if (sizeof(BrushPlane) == sizeof(SBrushPlane<float> ))
		return SBrushPlane<float>(plane.Normal(), plane.Distance());
	return SBrushPlane<float>(ToVec3(plane.Normal()), ToFloat(plane.Distance()));
}
}

