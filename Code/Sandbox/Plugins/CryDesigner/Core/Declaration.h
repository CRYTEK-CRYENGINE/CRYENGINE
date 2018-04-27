// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer
{
typedef f64                      BrushFloat;
typedef Vec2_tpl<BrushFloat>     BrushVec2;
typedef Vec3_tpl<BrushFloat>     BrushVec3;
typedef Vec4_tpl<BrushFloat>     BrushVec4;
typedef Matrix33_tpl<BrushFloat> BrushMatrix33;
typedef Matrix34_tpl<BrushFloat> BrushMatrix34;
typedef Matrix44_tpl<BrushFloat> BrushMatrix44;

extern const BrushFloat kDesignerEpsilon;
extern const BrushFloat kDesignerLooseEpsilon;
extern const BrushFloat kDistanceLimitation;
extern const BrushFloat kLimitForMagnetic;
extern const BrushFloat kInitialPrimitiveHeight;

static BrushMatrix33 ToBrushMatrix33(const Matrix33& matrix)
{
	if (sizeof(BrushMatrix33) == sizeof(Matrix33))
		return matrix;

	static BrushMatrix33 brushTM;

	for (int i = 0; i < 3; ++i)
		for (int k = 0; k < 3; ++k)
			brushTM(i, k) = BrushFloat(matrix(i, k));

	return brushTM;
}

static BrushMatrix34 ToBrushMatrix34(const Matrix34& matrix)
{
	if (sizeof(BrushMatrix34) == sizeof(Matrix34))
		return matrix;

	static BrushMatrix34 brushTM;

	for (int i = 0; i < 3; ++i)
		for (int k = 0; k < 4; ++k)
			brushTM(i, k) = BrushFloat(matrix(i, k));

	return brushTM;
}

static Vec3 ToVec3(const BrushVec3& v)
{
	if (sizeof(BrushVec3) == sizeof(Vec3))
		return v;
	return Vec3((float)v.x, (float)v.y, (float)v.z);
}

static BrushVec3 ToBrushVec3(const Vec3& v)
{
	if (sizeof(BrushVec3) == sizeof(Vec3))
		return v;
	return BrushVec3((BrushFloat)v.x, (BrushFloat)v.y, (BrushFloat)v.z);
}

static float ToFloat(const BrushFloat f)
{
	if (sizeof(BrushFloat) == sizeof(float))
		return f;
	return (float)f;
}

namespace Comparison
{
template<class _Type>
bool IsEquivalent(const _Type& v0, const _Type& v1)
{
	return v0.IsEquivalent(v1, (_Type::value_type)kDesignerEpsilon);
}

struct less_BrushVec3
{
	bool operator()(const BrushVec3& v0, const BrushVec3& v1) const
	{
		return v0.x < v1.x || v0.x == v1.x && v0.y < v1.y || v0.x == v1.x && v0.y == v1.y && v0.z < v1.z;
	}
};

struct less_UV
{
	bool operator()(const Vec2& v0, const Vec2& v1) const
	{
		if (std::abs(v0.x - v1.x) < kDesignerEpsilon)
			return v0.y < v1.y - kDesignerEpsilon;

		return v0.x < v1.x - kDesignerEpsilon;
	}
};
}

struct Vertex
{
	typedef float value_type;

	Vertex() : pos(0, 0, 0), uv(0, 0), id(0) {}
	explicit Vertex(const BrushVec3& _pos) : pos(_pos), uv(Vec2(0, 0)), id(0) {}
	Vertex(const BrushVec3& _pos, const Vec2& _uv, int _id = 0) : pos(_pos), uv(_uv), id(_id) {}
	Vertex(const BrushVec3& _pos, int _id) : pos(_pos), uv(Vec2(0, 0)), id(_id) {}

	void operator=(const Vertex& rv)
	{
		pos = rv.pos;
		uv = rv.uv;
		id = rv.id;
	}

	bool operator==(const Vertex& rv) const
	{
		return Comparison::IsEquivalent(pos, rv.pos) && Comparison::IsEquivalent(rv.uv, uv) && id == rv.id;
	}

	bool IsEquivalent(const Vertex& rv, value_type kEpsilon) const
	{
		return Comparison::IsEquivalent(pos, rv.pos) && Comparison::IsEquivalent(rv.uv, uv) && id == rv.id;
	}

	BrushVec3     pos;
	Vec2          uv;
	unsigned char id;
};

struct IndexPair
{
	IndexPair(){}

	IndexPair(int index0, int index1)
	{
		m_i[0] = index0;
		m_i[1] = index1;
	}
	IndexPair(const IndexPair& ip)
	{
		m_i[0] = ip.m_i[0];
		m_i[1] = ip.m_i[1];
	}
	bool operator==(const IndexPair& ip)
	{
		return m_i[0] == ip.m_i[0] && m_i[1] == ip.m_i[1];
	}

	bool operator<(const IndexPair& ip) const
	{
		return m_i[0] < ip.m_i[0] || m_i[0] == ip.m_i[0] && m_i[1] < ip.m_i[1];
	}

	int m_i[2];
};
typedef std::vector<IndexPair>      EdgeList;
typedef std::set<IndexPair>         EdgeSet;
typedef std::set<int>               EdgeIndexSet;
typedef std::map<int, EdgeIndexSet> EdgeMap;
}

