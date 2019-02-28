// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer
{

enum ESplitResult
{
	eSR_CROSS,
	eSR_POSITIVE,
	eSR_NEGATIVE,
	eSR_COINCIDENCE,
	eSR_INVALID
};

enum EOperationResult
{
	eOR_Zero = 0,
	eOR_One  = 1,
	eOR_Two  = 2,
	eOR_Invalid
};

enum EResultDistance
{
	eResultDistance_EdgeP0,
	eResultDistance_EdgeP1,
	eResultDistance_Middle
};

template<class T>
struct SBrushLine
{
	SBrushLine(){}
	SBrushLine(const Vec2_tpl<T>& normal, T distance) :
		m_Distance(distance)
	{
		m_Normal = normal.GetNormalized();
	}

	SBrushLine(const Vec2_tpl<T>& v0, const Vec2_tpl<T>& v1)
	{
		MakeLine(v0, v1);
	}

	void MakeLine(const Vec2_tpl<T>& v0, const Vec2_tpl<T>& v1)
	{
		Vec3_tpl<T> direction(v1.x - v0.x, v1.y - v0.y, 0);
		Vec3_tpl<T> up(0, 0, 1);
		Vec3_tpl<T> normal = up.Cross(direction);
		m_Normal.x = normal.x;
		m_Normal.y = normal.y;
		m_Normal.Normalize();
		m_Distance = -m_Normal.Dot(v0);
	}

	void Invert()
	{
		m_Normal = -m_Normal;
		m_Distance = -m_Distance;
	}

	SBrushLine<T> GetInverted()
	{
		SBrushLine<T> invLine(*this);
		invLine.Invert();
		return invLine;
	}

	T Distance(const Vec2_tpl<T>& position) const
	{
		return m_Normal.Dot(position) + m_Distance;
	}

	bool IsEquivalent(const SBrushLine<T>& l) const
	{
		return std::abs(m_Normal.x - l.m_Normal.x) < kDesignerEpsilon && std::abs(m_Normal.y - l.m_Normal.y) < kDesignerEpsilon && std::abs(l.m_Distance - m_Distance) < kDesignerEpsilon;
	}

	bool operator==(const SBrushLine<T>& l) const
	{
		return m_Normal.x == l.m_Normal.x && m_Normal.y == l.m_Normal.y && m_Distance == l.m_Distance;
	}

	bool Intersect(const SBrushLine<T>& line, Vec2_tpl<T>& intersection, const T& kEpsilon = kDesignerEpsilon) const
	{
		T n0 = -line.m_Normal.x * m_Distance + m_Normal.x * line.m_Distance;
		T d0 = line.m_Normal.x * m_Normal.y - m_Normal.x * line.m_Normal.y;
		T n1 = -line.m_Normal.y * m_Distance + m_Normal.y * line.m_Distance;
		T d1 = line.m_Normal.y * m_Normal.x - m_Normal.y * line.m_Normal.x;

		if (std::abs(d0) < kEpsilon || std::abs(d1) < kEpsilon)
			return false;

		intersection.x = n1 / d1;
		intersection.y = n0 / d0;

		return true;
	}

	bool HitTest(const Vec2_tpl<T>& p0, const Vec2_tpl<T>& p1, T* tout = NULL, Vec2_tpl<T>* vout = NULL) const
	{
		Vec2_tpl<T> dir = p1 - p0;
		T denominator = (m_Normal | dir);

		if (std::abs(denominator) < kDesignerEpsilon)
			return false;

		T t = -((m_Normal | p0) + m_Distance) / denominator;

		if (tout)
			*tout = t;

		if (vout)
			*vout = p0 + t * dir;

		return true;
	}

	Vec2_tpl<T> m_Normal;
	T           m_Distance;
};

template<class T>
struct SBrushEdge
{
	SBrushEdge(){}
	SBrushEdge(const Vec2_tpl<T>& v0, const Vec2_tpl<T>& v1)
	{
		m_v[0] = v0;
		m_v[1] = v1;
	}
	SBrushEdge(const SBrushEdge<T>& edge)
	{
		m_v[0] = edge.m_v[0];
		m_v[1] = edge.m_v[1];
	}
	Vec2_tpl<T> m_v[2];

	void        Reset()
	{
		m_v[0] = Vec2_tpl<T>(3e10f, 3e10f);
		m_v[1] = Vec2_tpl<T>(-3e10f, -3e10f);
	}
	void Invert()
	{
		Vec2_tpl<T> temp(m_v[0]);
		m_v[0] = m_v[1];
		m_v[1] = temp;
	}
	SBrushEdge<T> GetInverted()
	{
		SBrushEdge<T> invEdge(*this);
		invEdge.Invert();
		return invEdge;
	}
	bool IsInside(const Vec2_tpl<T>& pos, const T& kEpsilon = kDesignerEpsilon) const
	{
		if (m_v[0].x < m_v[1].x)
		{
			if (pos.x < m_v[0].x - kEpsilon || pos.x > m_v[1].x + kEpsilon)
				return false;
		}
		else
		{
			if (pos.x > m_v[0].x + kEpsilon || pos.x < m_v[1].x - kEpsilon)
				return false;
		}

		if (m_v[0].y < m_v[1].y)
		{
			if (pos.y < m_v[0].y - kEpsilon || pos.y > m_v[1].y + kEpsilon)
				return false;
		}
		else
		{
			if (pos.y > m_v[0].y + kEpsilon || pos.y < m_v[1].y - kEpsilon)
				return false;
		}

		SBrushLine<T> line(m_v[0], m_v[1]);
		T d(std::abs(line.Distance(pos)));
		return d < kEpsilon;
	}
	bool IsEquivalent(const SBrushEdge<T>& edge) const
	{
		return m_v[0].IsEquivalent(edge.m_v[0], kDesignerEpsilon) && m_v[1].IsEquivalent(edge.m_v[1], kDesignerEpsilon);
	}
	bool IsPointInRectangleOfEdge(const Vec2_tpl<T>& point) const
	{
		if (point.x > m_v[0].x + kDesignerEpsilon && point.x < m_v[1].x - kDesignerEpsilon || point.x<m_v[0].x - kDesignerEpsilon&& point.x> m_v[1].x + kDesignerEpsilon)
		{
			if (point.y > m_v[0].y + kDesignerEpsilon && point.y < m_v[1].y - kDesignerEpsilon || point.y<m_v[0].y - kDesignerEpsilon&& point.y> m_v[1].y + kDesignerEpsilon)
			{
				return true;
			}
			else if (std::abs(m_v[0].y - m_v[1].y) < kDesignerEpsilon)
			{
				if (std::abs(point.y - m_v[0].y) < kDesignerEpsilon || std::abs(point.y - m_v[1].y) < kDesignerEpsilon)
					return true;
				return false;
			}
		}
		else if (point.y > m_v[0].y + kDesignerEpsilon && point.y < m_v[1].y - kDesignerEpsilon || point.y<m_v[0].y - kDesignerEpsilon&& point.y> m_v[1].y + kDesignerEpsilon)
		{
			if (std::abs(m_v[0].x - m_v[1].x) < kDesignerEpsilon)
			{
				if (std::abs(point.x - m_v[0].x) < kDesignerEpsilon || std::abs(point.x - m_v[1].x) < kDesignerEpsilon)
					return true;
				return false;
			}
		}
		return false;
	}
	bool IsSameFacing(const SBrushLine<T>& line)
	{
		SBrushLine<T> l(m_v[0], m_v[1]);
		return line.m_Normal.Dot(l.m_Normal) > 0;
	}
	bool GetNearestPoint(const Vec2_tpl<T>& pos, Vec2_tpl<T>& outPos) const
	{
		Vec2_tpl<T> Q = m_v[1] - m_v[0];
		T QLen2(Q.GetLength2());
		if (QLen2 == 0)
			return false;
		Vec2_tpl<T> P = pos - m_v[0];
		outPos = (P.Dot(Q) / QLen2) * Q + m_v[0];
		if (m_v[0].x < m_v[1].x)
		{
			if (outPos.x < m_v[0].x || outPos.x > m_v[1].x)
				return false;
		}
		else
		{
			if (outPos.x < m_v[1].x || outPos.x > m_v[0].x)
				return false;
		}
		if (m_v[0].y < m_v[1].y)
		{
			if (outPos.y < m_v[0].y || outPos.y > m_v[1].y)
				return false;
		}
		else
		{
			if (outPos.y < m_v[1].y || outPos.y > m_v[0].y)
				return false;
		}
		return true;
	}
	bool GetIntersect(const SBrushEdge<T>& edge, Vec2_tpl<T>& outIntersectionPoint) const
	{
		SBrushLine<T> line0(m_v[0], m_v[1]);
		SBrushLine<T> line1(edge.m_v[0], edge.m_v[1]);

		if (!line0.Intersect(line1, outIntersectionPoint, kDesignerEpsilon))
			return false;

		return IsPointInRectangleOfEdge(outIntersectionPoint) && edge.IsPointInRectangleOfEdge(outIntersectionPoint);
	}
	bool IsIntersect(const SBrushEdge<T>& edge) const
	{
		Vec2_tpl<T> intersectionPt;
		return GetIntersect(edge, intersectionPt);
	}
	bool IsIdenticalLine(const SBrushEdge<T>& edge) const
	{
		SBrushLine<T> thisLine(m_v[0], m_v[1]);
		SBrushLine<T> line(edge.m_v[0], edge.m_v[1]);

		if (thisLine.IsEquivalent(line))
			return true;

		line.Invert();
		if (thisLine.IsEquivalent(line))
			return true;

		return false;
	}

	T GetLength() const
	{
		return (m_v[1] - m_v[0]).GetLength();
	}

	static EResultDistance GetSquaredDistance(const SBrushEdge<T>& edge, const Vec2_tpl<T>& vPoint, T& outDistance)
	{
		Vec2_tpl<T> vP0toP1(edge.m_v[1] - edge.m_v[0]);
		Vec2_tpl<T> vP0toPoint(vPoint - edge.m_v[0]);
		T t = vP0toP1.Dot(vP0toPoint);

		if (t <= 0)
		{
			outDistance = vP0toPoint.Dot(vP0toPoint);
			return eResultDistance_EdgeP0;
		}

		T SquaredP0toP1 = vP0toP1.Dot(vP0toP1);
		if (t >= SquaredP0toP1)
		{
			Vec2_tpl<T> vP1toPoint(vPoint - edge.m_v[1]);
			outDistance = vP1toPoint.Dot(vPoint);
			return eResultDistance_EdgeP1;
		}

		outDistance = vP0toPoint.Dot(vP0toPoint) - t * t / SquaredP0toP1;
		return eResultDistance_Middle;
	}

public:
	typedef std::vector<SBrushEdge<T>> EdgeList;
};

template<class T>
struct SBrushLine3D
{
	SBrushLine3D(){}
	SBrushLine3D(const Vec3_tpl<T>& v0, const Vec3_tpl<T>& v1)
	{
		m_Dir = (v1 - v0).GetNormalized();
		m_Pivot = v0;
	}

	void Invert()
	{
		m_Dir = -m_Dir;
	}

	bool IsEquivalent(const SBrushLine3D<T>& p, const T& kEpsilon) const
	{
		return m_Dir.IsEquivalent(p.m_Dir, kEpsilon) && m_Pivot.IsEquivalent(p.m_Pivot, kEpsilon);
	}

	bool IsValid()
	{
		return !m_Dir.IsZero();
	}

	BrushVec3 GetProjectedVector(const BrushVec3& p) const
	{
		return m_Dir * (p - m_Pivot).Dot(m_Dir);
	}

	BrushFloat GetSquaredDistance(const BrushVec3& p) const
	{
		BrushVec3 pivot2p = (p - m_Pivot);
		BrushVec3 projectedV = GetProjectedVector(p);
		return std::abs(pivot2p.Dot(pivot2p) - projectedV.Dot(projectedV));
	}

	BrushFloat GetDistance(const BrushVec3& p) const
	{
		return std::sqrt(GetSquaredDistance(p));
	}

	Vec3_tpl<T> m_Dir;
	Vec3_tpl<T> m_Pivot;
};

template<class T1, class T2>
struct SBrushEdge3D
{
	typedef T1 value_type1;
	typedef T2 value_type2;

	SBrushEdge3D()
	{
		memset(m_data, 0, sizeof(value_type2) * 2);
	}
	SBrushEdge3D(const Vec3_tpl<T1>& v0, const Vec3_tpl<T1>& v1)
	{
		m_v[0] = v0;
		m_v[1] = v1;
		memset(m_data, 0, sizeof(value_type2) * 2);
	}
	SBrushEdge3D(const Vec3_tpl<T1>& v0, const Vec3_tpl<T1>& v1, const T2& data0, const T2& data1)
	{
		m_v[0] = v0;
		m_v[1] = v1;
		m_data[0] = data0;
		m_data[1] = data1;
	}

	bool IsEquivalent(const SBrushEdge3D<T1, T2>& edge) const
	{
		return m_v[0].IsEquivalent(edge.m_v[0], kDesignerEpsilon) && m_v[1].IsEquivalent(edge.m_v[1], kDesignerEpsilon);
	}

	bool IsSameFacing(const SBrushLine3D<T1>& line)
	{
		Vec3_tpl<T1> edgeDir = (m_v[1] - m_v[0]).GetNormalized();
		return line.m_Dir.Dot(edgeDir) > 0;
	}

	void Invert()
	{
		std::swap(m_v[0], m_v[1]);
		std::swap(m_data[0], m_data[1]);
	}

	SBrushEdge3D<T1, T2> GetInverted() const
	{
		SBrushEdge3D<T1, T2> invEdge(*this);
		invEdge.Invert();
		return invEdge;
	}

	bool ContainVertex(const Vec3_tpl<T1>& pos) const
	{
		for (int i = 0; i < 3; ++i)
		{
			if (m_v[0][i] < m_v[1][i])
			{
				if (pos[i] < m_v[0][i] - kDesignerEpsilon || pos[i] > m_v[1][i] + kDesignerEpsilon)
					return false;
			}
			else if (pos[i] > m_v[0][i] + kDesignerEpsilon || pos[i] < m_v[1][i] - kDesignerEpsilon)
			{
				return false;
			}
		}

		T1 d = 3e10f;
		CalculateSquaredDistance(*this, pos, d);
		return d < kDesignerEpsilon;
	}

	static EResultDistance CalculateSquaredDistance(const SBrushEdge3D<T1, T2>& edge, const Vec3_tpl<T1>& vertex, T1& outDistance)
	{
		Vec3_tpl<T1> vV0toV1(edge.m_v[1] - edge.m_v[0]);
		Vec3_tpl<T1> vV0toVertex(vertex - edge.m_v[0]);
		T1 t = vV0toV1.Dot(vV0toVertex);

		if (t < -kDesignerEpsilon)
		{
			outDistance = vV0toVertex.Dot(vV0toVertex);
			return eResultDistance_EdgeP0;
		}

		T1 SquaredV0toV1 = vV0toV1.Dot(vV0toV1);
		if (t > SquaredV0toV1 + kDesignerEpsilon)
		{
			Vec3_tpl<T1> vV0toVertex(vertex - edge.m_v[1]);
			outDistance = vV0toVertex.Dot(vertex);
			return eResultDistance_EdgeP1;
		}

		outDistance = vV0toVertex.Dot(vV0toVertex) - t * t / SquaredV0toV1;
		return eResultDistance_Middle;
	}

	bool CalculateNearestVertex(const Vec3_tpl<T1>& pos, Vec3_tpl<T1>& outPos, bool& bInEdge) const
	{
		Vec3_tpl<T1> Q = m_v[1] - m_v[0];
		T1 QLen2(Q.x * Q.x + Q.y * Q.y + Q.z * Q.z);
		if (QLen2 == 0)
			return false;
		Vec3_tpl<T1> P = pos - m_v[0];
		outPos = (P.Dot(Q) / QLen2) * Q + m_v[0];

		bInEdge = true;

		for (int i = 0; i < 3; ++i)
		{
			if (m_v[0][i] < m_v[1][i])
			{
				if (outPos[i] < m_v[0][i] || outPos[i] > m_v[1][i])
				{
					if (outPos[i] < m_v[0][i])
						outPos = m_v[0];
					else if (outPos[i] > m_v[1][i])
						outPos = m_v[1];
					bInEdge = false;
					break;
				}
			}
			else if (outPos[i] < m_v[1][i] || outPos[i] > m_v[0][i])
			{
				if (outPos[i] < m_v[1][i])
					outPos = m_v[1];
				else if (outPos[i] > m_v[0][i])
					outPos = m_v[0];
				bInEdge = false;
				break;
			}
		}
		return true;
	}

	T1 GetLength() const
	{
		return (m_v[1] - m_v[0]).GetLength();
	}

	bool IsIdenticalLine(const SBrushEdge3D<T1, T2>& edge, const T1& kEpsilon) const
	{
		SBrushLine<T1> thisLine(m_v[0], m_v[1]);
		SBrushLine<T1> line(edge.m_v[0], edge.m_v[1]);

		if (thisLine.IsEquivalent(line, kEpsilon))
			return true;

		line.Invert();
		if (thisLine.IsEquivalent(line, kEpsilon))
			return true;

		return false;
	}

	bool GetProjectedPos(const Vec3_tpl<T1>& pos, Vec3_tpl<T1>& outProjectedPos) const
	{
		Vec3_tpl<T1> v0tov1 = m_v[1] - m_v[0];
		Vec3_tpl<T1> v0topos = pos - m_v[0];
		T1 v0tov1Length = v0tov1.GetLengthSquared();

		if (v0tov1Length == 0)
			return false;

		outProjectedPos = m_v[0] + v0tov1 * (v0topos.Dot(v0tov1) / v0tov1Length);
		return true;
	}

	bool IsEdgeOnIdenticalLine(const SBrushEdge3D<T1, T2>& inEdge, const T1& kEpsilon) const
	{
		SBrushLine3D<T1> thisLine(m_v[0], m_v[1]);
		SBrushLine3D<T1> inLine(inEdge.m_v[0], inEdge.m_v[1]);

		if (!thisLine.m_Dir.IsEquivalent(inLine.m_Dir, kEpsilon) && !thisLine.m_Dir.IsEquivalent(-inLine.m_Dir, kEpsilon))
			return false;

		Vec3_tpl<T1> projectedPos;
		if (!GetProjectedPos(inEdge.m_v[0], projectedPos))
			return false;

		return projectedPos.GetDistance(inEdge.m_v[0]) < kEpsilon;
	}

	bool ProjectedEdges(const SBrushEdge3D<T1, T2>& inEdge,
	                    T1 outProjectedEdge0[2],
	                    T1 outProjectedEdge1[2],
	                    bool& bOutSwapInEdge0,
	                    bool& bOutSwapInEdge1,
	                    const T1& kEpsilon) const
	{
		Vec3_tpl<T1> edgeDir = (m_v[1] - m_v[0]).GetNormalized();

		T1 absX = std::abs(edgeDir.x);
		T1 absY = std::abs(edgeDir.y);
		T1 absZ = std::abs(edgeDir.z);

		if (absX >= absY && absX >= absZ)
		{
			outProjectedEdge0[0] = m_v[0].x;
			outProjectedEdge0[1] = m_v[1].x;
			outProjectedEdge1[0] = inEdge.m_v[0].x;
			outProjectedEdge1[1] = inEdge.m_v[1].x;
		}
		else if (absY >= absX && absY >= absZ)
		{
			outProjectedEdge0[0] = m_v[0].y;
			outProjectedEdge0[1] = m_v[1].y;
			outProjectedEdge1[0] = inEdge.m_v[0].y;
			outProjectedEdge1[1] = inEdge.m_v[1].y;
		}
		else
		{
			outProjectedEdge0[0] = m_v[0].z;
			outProjectedEdge0[1] = m_v[1].z;
			outProjectedEdge1[0] = inEdge.m_v[0].z;
			outProjectedEdge1[1] = inEdge.m_v[1].z;
		}

		if (outProjectedEdge0[0] > outProjectedEdge0[1])
		{
			std::swap(outProjectedEdge0[0], outProjectedEdge0[1]);
			bOutSwapInEdge0 = true;
		}
		else
		{
			bOutSwapInEdge0 = false;
		}

		if (outProjectedEdge1[0] > outProjectedEdge1[1])
		{
			std::swap(outProjectedEdge1[0], outProjectedEdge1[1]);
			bOutSwapInEdge1 = true;
		}
		else
		{
			bOutSwapInEdge1 = false;
		}

		if (std::abs(outProjectedEdge0[0] - outProjectedEdge1[0]) <= kEpsilon)
		{
			if (outProjectedEdge0[0] < outProjectedEdge1[0])
				outProjectedEdge1[0] = outProjectedEdge0[0];
			if (outProjectedEdge0[0] > outProjectedEdge1[0])
				outProjectedEdge0[0] = outProjectedEdge1[0];
		}

		if (std::abs(outProjectedEdge0[1] - outProjectedEdge1[1]) <= kEpsilon)
		{
			if (outProjectedEdge0[1] < outProjectedEdge1[1])
				outProjectedEdge1[1] = outProjectedEdge0[1];
			if (outProjectedEdge0[1] > outProjectedEdge1[1])
				outProjectedEdge0[1] = outProjectedEdge1[1];
		}

		return true;
	}

	bool GetSortedEdges(SBrushEdge3D& edge0, SBrushEdge3D& edge1, int& nOutElementIndex, bool* pbOutEdge0Inverted = NULL, bool* pbOutEdge1Inverted = NULL) const
	{
		SBrushLine3D<T1> line(edge0.m_v[0], edge0.m_v[1]);

		if (!line.IsValid())
			return false;

		if (std::abs(line.m_Dir.x) >= std::abs(line.m_Dir.y) && std::abs(line.m_Dir.x) >= std::abs(line.m_Dir.z))
			nOutElementIndex = 0;
		else if (std::abs(line.m_Dir.y) >= std::abs(line.m_Dir.x) && std::abs(line.m_Dir.y) >= std::abs(line.m_Dir.z))
			nOutElementIndex = 1;
		else
			nOutElementIndex = 2;

		if (pbOutEdge0Inverted)
			*pbOutEdge0Inverted = false;
		if (pbOutEdge1Inverted)
			*pbOutEdge1Inverted = false;

		if (edge0.m_v[0][nOutElementIndex] > edge0.m_v[1][nOutElementIndex])
		{
			if (pbOutEdge0Inverted)
				*pbOutEdge0Inverted = true;
			edge0.Invert();
		}

		if (edge1.m_v[0][nOutElementIndex] > edge1.m_v[1][nOutElementIndex])
		{
			if (pbOutEdge1Inverted)
				*pbOutEdge1Inverted = true;
			edge1.Invert();
		}

		return true;
	}

	bool Include(const SBrushEdge3D<T1, T2>& inEdge) const
	{
		if (!IsEdgeOnIdenticalLine(inEdge, kDesignerEpsilon))
			return false;

		int nElementIndex(0);
		SBrushEdge3D thisEdge(*this);
		SBrushEdge3D inputEdge(inEdge);
		if (!GetSortedEdges(thisEdge, inputEdge, nElementIndex, NULL, NULL))
			return false;

		return thisEdge.m_v[0][nElementIndex] < inputEdge.m_v[0][nElementIndex] && thisEdge.m_v[1][nElementIndex] > inputEdge.m_v[1][nElementIndex];
	}

	bool GetSubtractedEdges(const SBrushEdge3D<T1, T2>& edge, std::vector<SBrushEdge3D<T1, T2>>& outEdges) const
	{
		if (!IsEdgeOnIdenticalLine(edge, kDesignerEpsilon))
			return false;

		int nElementIndex(0);
		SBrushEdge3D thisEdge(*this);
		SBrushEdge3D inputEdge(edge);
		bool bInvertedThisEdge = false;
		bool bInvertedInputEdge = false;
		if (!GetSortedEdges(thisEdge, inputEdge, nElementIndex, &bInvertedThisEdge, &bInvertedInputEdge))
			return false;

		if (inputEdge.m_v[1][nElementIndex] < thisEdge.m_v[0][nElementIndex] ||
		    inputEdge.m_v[0][nElementIndex] > thisEdge.m_v[1][nElementIndex])
		{
			if (bInvertedThisEdge)
				outEdges.push_back(SBrushEdge3D<T1, T2>(thisEdge.m_v[1], thisEdge.m_v[0]));
			else
				outEdges.push_back(SBrushEdge3D<T1, T2>(thisEdge.m_v[0], thisEdge.m_v[1]));
			return true;
		}

		if (inputEdge.m_v[0][nElementIndex] <= thisEdge.m_v[0][nElementIndex] &&
		    inputEdge.m_v[1][nElementIndex] >= thisEdge.m_v[0][nElementIndex] &&
		    inputEdge.m_v[1][nElementIndex] <= thisEdge.m_v[1][nElementIndex])
		{
			if (bInvertedThisEdge)
				outEdges.push_back(SBrushEdge3D<T1, T2>(thisEdge.m_v[1], inputEdge.m_v[1]));
			else
				outEdges.push_back(SBrushEdge3D<T1, T2>(inputEdge.m_v[1], thisEdge.m_v[1]));
			return true;
		}

		if (inputEdge.m_v[0][nElementIndex] > thisEdge.m_v[0][nElementIndex] &&
		    inputEdge.m_v[0][nElementIndex] < thisEdge.m_v[1][nElementIndex] &&
		    inputEdge.m_v[1][nElementIndex] > thisEdge.m_v[0][nElementIndex] &&
		    inputEdge.m_v[1][nElementIndex] < thisEdge.m_v[1][nElementIndex])
		{
			if (bInvertedThisEdge)
			{
				outEdges.push_back(SBrushEdge3D<T1, T2>(inputEdge.m_v[0], thisEdge.m_v[0]));
				outEdges.push_back(SBrushEdge3D<T1, T2>(thisEdge.m_v[1], inputEdge.m_v[1]));
			}
			else
			{
				outEdges.push_back(SBrushEdge3D<T1, T2>(thisEdge.m_v[0], inputEdge.m_v[0]));
				outEdges.push_back(SBrushEdge3D<T1, T2>(inputEdge.m_v[1], thisEdge.m_v[1]));
			}
			return true;
		}

		if (inputEdge.m_v[0][nElementIndex] >= thisEdge.m_v[0][nElementIndex] &&
		    inputEdge.m_v[0][nElementIndex] <= thisEdge.m_v[1][nElementIndex] &&
		    inputEdge.m_v[1][nElementIndex] >= thisEdge.m_v[1][nElementIndex])
		{
			if (bInvertedThisEdge)
				outEdges.push_back(SBrushEdge3D<T1, T2>(inputEdge.m_v[0], thisEdge.m_v[0]));
			else
				outEdges.push_back(SBrushEdge3D<T1, T2>(thisEdge.m_v[0], inputEdge.m_v[0]));
			return true;
		}

		return false;
	}

	BrushVec3    GetDirection() const { return (m_v[1] - m_v[0]).GetNormalized(); }
	Vec3_tpl<T1> GetCenter() const    { return (m_v[0] + m_v[1]) * 0.5f; }
	bool         IsPoint() const      { return m_v[0].IsEquivalent(m_v[1], kDesignerEpsilon); }

	Vec3_tpl<T1> m_v[2];
	T2           m_data[2];
	typedef std::vector<SBrushEdge3D<T1, T2>> List;
};

template<class T>
struct SBrushRay
{
	SBrushRay() : origin(0, 0, 0), direction(0, 0, 0)
	{
	}

	SBrushRay(const Vec3_tpl<T>& _origin, const Vec3_tpl<T>& _direction) : origin(_origin), direction(_direction)
	{
	}

	Vec3_tpl<T> origin;
	Vec3_tpl<T> direction;
};

typedef SBrushEdge<BrushFloat>         BrushEdge;
typedef SBrushLine<BrushFloat>         BrushLine;
typedef SBrushEdge3D<BrushFloat, Vec2> BrushEdge3D;
typedef SBrushLine3D<BrushFloat>       BrushLine3D;
typedef SBrushRay<BrushFloat>          BrushRay;

}

