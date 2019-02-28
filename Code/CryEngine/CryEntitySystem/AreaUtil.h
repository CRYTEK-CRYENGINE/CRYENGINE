// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace AreaUtil
{
extern const float EPSILON;

class CPlaneBase
{
public:
	CPlaneBase(){}
	CPlaneBase(const Vec3& v0, const Vec3& v1, const Vec3& v2)
	{
		m_Normal = (v2 - v1) ^ (v0 - v1);
		m_Normal.Normalize();
		if (fabs(m_Normal.x) < AreaUtil::EPSILON)
			m_Normal.x = 0;
		if (fabs(m_Normal.y) < AreaUtil::EPSILON)
			m_Normal.y = 0;
		if (fabs(m_Normal.z) < AreaUtil::EPSILON)
			m_Normal.z = 0;
		m_Normal.Normalize();
		m_Distance = -m_Normal.Dot(v2);
	}

	float Distance(const Vec3& p) const
	{
		return m_Normal.Dot(p) + m_Distance;
	}

	bool IsEquivalent(const CPlaneBase& p) const
	{
		return m_Normal.IsEquivalent(p.m_Normal, AreaUtil::EPSILON) && fabs(p.m_Distance - m_Distance) < AreaUtil::EPSILON;
	}

	bool HitTest(const Vec3& p0, const Vec3& p1, float* tout = NULL, Vec3* vout = NULL) const
	{
		float denominator = (m_Normal | (p1 - p0));
		if (fabs(denominator) < AreaUtil::EPSILON)
			return false;
		float t = -((m_Normal | p0) + m_Distance) / denominator;
		if (tout)
			*tout = t;
		if (vout)
			*vout = p0 + t * (p1 - p0);
		return true;
	}

	const Vec3& GetNormal() const
	{
		return m_Normal;
	}
protected:
	Vec3  m_Normal;
	float m_Distance;
};

class CPlane : public CPlaneBase
{
public:

	CPlane(){}
	CPlane(const Vec3& v0, const Vec3& v1, const Vec3& v2) :
		CPlaneBase(v0, v1, v2)
	{
		ComputeBasisTM();
	}

	Vec2 WorldToPlane(const Vec3& worldPos) const
	{
		Vec3 planePos(worldPos * m_BasisTM);
		return Vec2(planePos.x, planePos.z);
	}

	Vec3 PlaneToWorld(const Vec2& planePos) const
	{
		return m_BasisTM * Vec3(planePos.x, -m_Distance, planePos.y);
	}

private:

	void ComputeBasisTM()
	{
		Vec3 u, v, w;
		w = m_Normal;
		if (fabs(m_Normal.x) >= fabs(m_Normal.y))
		{
			float invLength = 1 / sqrtf(m_Normal.x * m_Normal.x + m_Normal.z * m_Normal.z);
			u.x = m_Normal.z * invLength;
			u.y = 0;
			u.z = -m_Normal.x * invLength;
		}
		else
		{
			float invLength = 1 / sqrtf(m_Normal.y * m_Normal.y + m_Normal.z * m_Normal.z);
			u.x = 0;
			u.y = m_Normal.z * invLength;
			u.z = -m_Normal.y * invLength;
		}
		v = u.Cross(w);
		m_BasisTM = Matrix33(u, w, v);
	}

private:

	Matrix33 m_BasisTM;
};

class CLine
{
public:
	CLine(){}
	CLine(const Vec2& p0, const Vec2& p1)
	{
		Vec3 direction(p1.x - p0.x, p1.y - p0.y, 0);
		Vec3 up(0, 0, 1);
		Vec3 normal = up.Cross(direction);
		m_Normal.x = normal.x;
		m_Normal.y = normal.y;
		m_Normal.Normalize();
		m_Distance = m_Normal.Dot(p0);
	}

	float Distance(const Vec2& position) const
	{
		return m_Normal.Dot(position) - m_Distance;
	}

	const Vec2& GetNormal() const
	{
		return m_Normal;
	}

	float DotWithNormal(const Vec2& direction) const
	{
		return m_Normal.Dot(direction);
	}

private:
	Vec2  m_Normal;
	float m_Distance;
};

enum EPointPosEnum
{
	ePP_BORDER,
	ePP_INSIDE,
	ePP_OUTSIDE
};

enum ESplitResult
{
	eSR_CROSS,
	eSR_POSITIVE,
	eSR_NEGATIVE,
	eSR_COINCIDENCE
};
};
