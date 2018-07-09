// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include "Cry_Geo.h"
#include <CryCore/Containers/CryArray.h>
#include "Random.h"

//! Extents cache.
class CGeomExtent
{
public:

	CGeomExtent()
		: m_nEmptyEndParts(0) {}

	operator bool() const
	{
		return m_afCumExtents.capacity() + m_nEmptyEndParts != 0;
	}
	int NumParts() const
	{
		return m_afCumExtents.size();
	}
	float TotalExtent() const
	{
		return !m_afCumExtents.empty() ? m_afCumExtents.back() : 0.f;
	}
	float PartExtent(int nPart) const
	{
		return nPart == 0 ?
			m_afCumExtents[nPart] :
			m_afCumExtents[nPart] - m_afCumExtents[nPart - 1];
	}

	void Clear()
	{
		m_afCumExtents.clear();
		m_nEmptyEndParts = 0;
	}
	void AddPart(float fExtent)
	{
		// Defer empty parts until a non-empty part is added.
		if (fExtent <= 0.f)
			m_nEmptyEndParts++;
		else
		{
			float fTotal = TotalExtent();
			for (; m_nEmptyEndParts; m_nEmptyEndParts--)
				m_afCumExtents.push_back(fTotal);
			m_afCumExtents.push_back(fTotal + fExtent);
		}
	}
	void ReserveParts(int nCount)
	{
		m_afCumExtents.reserve(nCount);
	}

	//! Find element in sorted array <= index (normalized 0 to 1).
	int GetPart(float fIndex) const
	{
		int last = m_afCumExtents.size() - 1;
		if (last <= 0)
			return last;

		fIndex *= m_afCumExtents[last];

		// Binary search thru array.
		int lo = 0, hi = last;
		while (lo < hi)
		{
			int i = (lo + hi) >> 1;
			if (fIndex < m_afCumExtents[i])
				hi = i;
			else
				lo = i + 1;
		}

		assert(lo == 0 || m_afCumExtents[lo] > m_afCumExtents[lo - 1]);
		return lo;
	}

	int RandomPart(CRndGen& seed) const
	{
		return GetPart(seed.GenerateFloat());
	}

	void RandomParts(Array<int> aParts, CRndGen& seed) const
	{
		for (int& part : aParts)
			part = RandomPart(seed);
	}

	// Get multiple random parts, aliased to a PosNorm array
	Array<int> RandomPartsAlias(Array<PosNorm> aPoints, CRndGen& seed) const;

	// Get multiple random parts, and sum them into a PartSum array, aliased to a PosNorm array
	struct PartSum
	{
		Array<PosNorm> aPoints;
		int iPart;
	};
	Array<PartSum> RandomPartsAliasSum(Array<PosNorm> aPoints, CRndGen& seed) const;

protected:

	DynArray<float> m_afCumExtents;
	int             m_nEmptyEndParts;
};

class CGeomExtents
{
public:

	CGeomExtents()
		: m_aExtents(0) {}
	~CGeomExtents()
	{ delete[] m_aExtents; }

	operator bool() const
	{
		return m_aExtents != 0;
	}

	void Clear()
	{
		delete[] m_aExtents;
		m_aExtents = 0;
	}

	CGeomExtent const& operator[](EGeomForm eForm) const
	{
		assert(eForm >= 0 && eForm < GeomForm_COUNT);
		if (m_aExtents)
			return m_aExtents[eForm];

		static CGeomExtent s_empty;
		return s_empty;
	}

	CGeomExtent& Make(EGeomForm eForm)
	{
		assert(eForm >= 0 && eForm < GeomForm_COUNT);
		if (!m_aExtents)
			m_aExtents = new CGeomExtent[GeomForm_COUNT];
		return m_aExtents[eForm];
	}

protected:
	CGeomExtent* m_aExtents;
};

// Other random/extent functions

inline float ScaleExtent(EGeomForm eForm, float fScale)
{
	switch (eForm)
	{
	default:
		return 1.0f;
	case GeomForm_Edges:
		return fScale;
	case GeomForm_Surface:
		return fScale * fScale;
	case GeomForm_Volume:
		return fScale * fScale * fScale;
	}
}

inline float ScaleExtent(EGeomForm eForm, const Matrix34& tm)
{
	switch (eForm)
	{
	default:
		return 1.0f;
	case GeomForm_Edges:
		return powf(tm.Determinant(), 1.0f / 3.0f);
	case GeomForm_Surface:
		return powf(tm.Determinant(), 2.0f / 3.0f);
	case GeomForm_Volume:
		return tm.Determinant();
	}
}

inline float BoxExtent(EGeomForm eForm, Vec3 const& vSize)
{
	switch (eForm)
	{
	default:
		assert(0);
	case GeomForm_Vertices:
		return 8.f;
	case GeomForm_Edges:
		return (vSize.x + vSize.y + vSize.z) * 8.f;
	case GeomForm_Surface:
		return (vSize.x * vSize.y + vSize.x * vSize.z + vSize.y * vSize.z) * 8.f;
	case GeomForm_Volume:
		return vSize.x * vSize.y * vSize.z * 8.f;
	}
}

// Utility functions.

template<class T> inline
const typename T::value_type& RandomElem(CRndGen& seed, const T& array)
{
	int n = seed.GetRandom(0U, array.size() - 1);
	return array[n];
}

inline void RandomSplit2(CRndGen& seed, float vals[2])
{
	vals[0] = seed.GenerateFloat();
	vals[1] = 1.0f - vals[0];
}

inline void RandomSplit3(CRndGen& seed, float vals[3])
{
	float a = seed.GenerateFloat();
	float b = seed.GenerateFloat();
	vals[0] = min(a, b);
	vals[1] = max(a, b) - vals[0];
	vals[2] = 1.0f - vals[1] - vals[0];
}

inline void RandomSplit4(CRndGen& seed, float vals[4])
{
	float a = seed.GenerateFloat();
	float b = seed.GenerateFloat();
	float c = seed.GenerateFloat();
	vals[0] = min(min(a, b), c);
	vals[1] = a == vals[0] ? min(b, c) : b == vals[0] ? min(a, c) : min(a, b);
	vals[2] = max(max(a, b), c);
	vals[3] = 1.0f - vals[2];
	vals[2] -= vals[1];
	vals[1] -= vals[0];
}

// Geometric primitive randomizing functions.
void BoxRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm, Vec3 const& vSize);

inline float CircleExtent(EGeomForm eForm, float fRadius)
{
	switch (eForm)
	{
	case GeomForm_Edges:
		return gf_PI2 * fRadius;
	case GeomForm_Surface:
		return gf_PI * square(fRadius);
	default:
		return 1.f;
	}
}

inline Vec2 CircleRandomPoint(CRndGen& seed, EGeomForm eForm, float fRadius)
{
	Vec2 vPt;
	switch (eForm)
	{
	case GeomForm_Edges:
		// Generate random angle.
		sincos_tpl(seed.GetRandom(0.0f, gf_PI2), &vPt.y, &vPt.x);
		vPt *= fRadius;
		break;
	case GeomForm_Surface:
		// Generate random angle, and radius, adjusted for even distribution.
		sincos_tpl(seed.GetRandom(0.0f, gf_PI2), &vPt.y, &vPt.x);
		vPt *= sqrt(seed.GetRandom(0.0f, 1.0f)) * fRadius;
		break;
	default:
		vPt.x = vPt.y = 0.f;
	}
	return vPt;
}

inline float SphereExtent(EGeomForm eForm, float fRadius)
{
	switch (eForm)
	{
	default:
		assert(0);
	case GeomForm_Vertices:
	case GeomForm_Edges:
		return 0.f;
	case GeomForm_Surface:
		return gf_PI * 4.f * sqr(fRadius);
	case GeomForm_Volume:
		return gf_PI * 4.f / 3.f * cube(fRadius);
	}
}

void SphereRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm, float fRadius);

// Triangle randomisation functions
inline float TriExtent(EGeomForm eForm, Vec3 const aPos[3], Vec3 const& vCenter)
{
	switch (eForm)
	{
	default:
		assert(0);
	case GeomForm_Vertices:
		return 1.0f;
	case GeomForm_Edges:
		return (aPos[1] - aPos[0]).GetLengthFast();
	case GeomForm_Surface:
		return ((aPos[1] - aPos[0]) % (aPos[2] - aPos[0])).GetLengthFast() * 0.5f;
	case GeomForm_Volume:
		// Generate signed volume of pyramid by computing triple product of vertices.
		return (((aPos[0] - vCenter) ^ (aPos[1] - vCenter)) | (aPos[2] - vCenter)) / 6.0f;
	}
}

void TriRandomPos(PosNorm& ran, CRndGen& seed, EGeomForm eForm, PosNorm const aRan[3], Vec3 const& vCenter, bool bDoNormals);

// Mesh random pos functions

inline int TriMeshPartCount(EGeomForm eForm, int nIndices)
{
	switch (eForm)
	{
	default:
		assert(0);
	case GeomForm_Vertices:
	case GeomForm_Edges:
		// Number of edges = verts.
		return nIndices;
	case GeomForm_Surface:
	case GeomForm_Volume:
		// Number of tris.
		assert(nIndices % 3 == 0);
		return nIndices / 3;
	}
}

inline int TriIndices(int aIndices[3], int nPart, EGeomForm eForm)
{
	switch (eForm)
	{
	default:
		assert(0);
	case GeomForm_Vertices:       // Part is vert index
		aIndices[0] = nPart;
		return 1;
	case GeomForm_Edges:          // Part is vert index
		aIndices[0] = nPart;
		aIndices[1] = nPart + 1 - ((nPart % 3) >> 1) * 3;
		return 2;
	case GeomForm_Surface:        // Part is tri index
	case GeomForm_Volume:
		aIndices[0] = nPart * 3;
		aIndices[1] = aIndices[0] + 1;
		aIndices[2] = aIndices[0] + 2;
		return 3;
	}
}

// Polygon 2D triangulation

template<class T, class Vec = T, size_t offset = 0>
struct TPolygon2D: Array<const T, size_t>
{
	using Array<const T, size_t>::Array;

	TPolygon2D(const std::vector<T>& in)
		: Array<const T, size_t>(&in[0], in.size()) {}

	const Vec& operator[](size_t idx) const
	{
		assert(idx < this->size());
		const T* pVertex = &this->at(idx);
		return *(Vec*)((size_t)pVertex + offset);
	}

	float Area() const
	{
		int n = this->size();
		float area = 0.0f;

		for (int p = n - 1, q = 0; q < n; p = q++)
			area += (*this)[p].x * (*this)[q].y - (*this)[q].x * (*this)[p].y;

		return area * 0.5f;
	}

	template<typename Indices>
	bool Triangulate(Indices& tri_indices)
	{
		using S = typename Indices::value_type;

		// reset tri_indices
		tri_indices.resize(0);

		//C6255: _alloca indicates failure by raising a stack overflow exception. Consider using _malloca instead
		PREFAST_SUPPRESS_WARNING(6255);
		// allocate and initialize list of vertices in polygon
		int n = this->size();
		if (n < 3)
			return false;

		S* V = (S*)alloca(n * sizeof(S));

		// we want a counter-clockwise polygon in V
		if (0.0f < Area())
			for (int v = 0; v < n; v++)
				V[v] = v;
		else
			for (int v = 0; v < n; v++)
				V[v] = (n - 1) - v;

		int nv = n;

		//  remove nv-2 vertices, creating 1 triangle every time
		int count = 2 * nv;     // error detection

		for (int m = 0, v = nv - 1; nv > 2; )
		{
			// if we loop, it is probably a non-simple polygon
			if (0 >= (count--))
				return false;   // ERROR - probably bad polygon!

								// three consecutive vertices in current polygon, <u,v,w>
			int u = v;
			if (nv <= u) u = 0;                 // previous
			v = u + 1;
			if (nv <= v) v = 0;                 // new v
			int w = v + 1;
			if (nv <= w) w = 0;                 // next

			if (Snip(u, v, w, nv, V))
			{
				// true names of the vertices
				PREFAST_SUPPRESS_WARNING(6385);
				S a = V[u];
				S b = V[v];
				S c = V[w];

				// output triangle
				tri_indices.push_back(a);
				tri_indices.push_back(b);
				tri_indices.push_back(c);

				m++;

				// remove v from remaining polygon
				for (int s = v, t = v + 1; t < nv; s++, t++)
				{
					PREFAST_SUPPRESS_WARNING(6386);
					V[s] = V[t];
				}

				nv--;

				// reset error detection counter
				count = 2 * nv;
			}
		}

		return true;
	}

	static bool InsideTriangle(float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py)
	{
		float ax = Cx - Bx;
		float ay = Cy - By;
		float bx = Ax - Cx;
		float by = Ay - Cy;
		float cx = Bx - Ax;
		float cy = By - Ay;
		float apx = Px - Ax;
		float apy = Py - Ay;
		float bpx = Px - Bx;
		float bpy = Py - By;
		float cpx = Px - Cx;
		float cpy = Py - Cy;

		float aCROSSbp = ax * bpy - ay * bpx;
		float cCROSSap = cx * apy - cy * apx;
		float bCROSScp = bx * cpy - by * cpx;

		const float fEpsilon = -0.01f;
		return (aCROSSbp >= fEpsilon) && (bCROSScp >= fEpsilon) && (cCROSSap >= fEpsilon);
	};

protected:

	template<typename S>
	bool Snip(int u, int v, int w, int n, const S* V) const
	{
		float Ax = (*this)[V[u]].x;
		float Ay = (*this)[V[u]].y;

		float Bx = (*this)[V[v]].x;
		float By = (*this)[V[v]].y;

		float Cx = (*this)[V[w]].x;
		float Cy = (*this)[V[w]].y;

		if ((((Bx - Ax) * (Cy - Ay)) - ((By - Ay) * (Cx - Ax))) < 1e-6f)
			return false;

		for (int p = 0; p < n; p++)
		{
			if ((p == u) || (p == v) || (p == w))
				continue;

			float Px = (*this)[V[p]].x;
			float Py = (*this)[V[p]].y;

			if (InsideTriangle(Ax, Ay, Bx, By, Cx, Cy, Px, Py))
				return false;
		}

		return true;
	}
};

//! \endcond