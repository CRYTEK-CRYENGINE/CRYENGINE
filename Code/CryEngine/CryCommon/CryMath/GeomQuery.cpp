// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
//
//	Implementation of GeomQuery.h. Should be included in all DLLs
//

#include "GeomQuery.h"

Array<int> CGeomExtent::RandomPartsAlias(Array<PosNorm> aPoints, CRndGen& seed) const
{
	Array<int> aParts((int*)aPoints.end() - aPoints.size(), aPoints.size());
	RandomParts(aParts, seed);
	return aParts;
}

Array<CGeomExtent::PartSum> CGeomExtent::RandomPartsAliasSum(Array<PosNorm> aPoints, CRndGen& seed) const
{
	if (aPoints.size() == 0)
		return {};

	assert(sizeof(PosNorm) >= sizeof(PartSum));
	Array<PartSum> aParts((PartSum*)aPoints.end() - aPoints.size(), aPoints.size());

	if (NumParts() <= 1)
	{
		aParts.back().aPoints = aPoints;
		aParts.back().iPart = NumParts() - 1;
		return aParts(aParts.size() - 1);
	}

	for (auto& part : aParts)
	{
		part.iPart = RandomPart(seed);
	}

	int first = aParts.size() - 1;
	int cur = first;
	if (aPoints.size() > 1)
	{
		std::sort(aParts.begin(), aParts.end(), [](const PartSum& a, const PartSum& b) { return a.iPart < b.iPart; });

		for (int i = first - 1; i > 0; --i)
		{
			if (aParts[i].iPart != aParts[cur].iPart)
			{
				aParts[first].aPoints = aPoints(i + 1, cur - i);
				first--;
				cur = i;
				aParts[first].iPart = aParts[cur].iPart;
			}
		}
	}
	aParts[first].aPoints = aPoints(0, cur + 1);

	return aParts(first);
}

void BoxRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm, Vec3 const& vSize)
{
	for (auto& ran : points)
	{
		ran.vPos = seed.GetRandomComponentwise(-vSize, vSize);
		ran.vNorm = ran.vPos;

		if (eForm != GeomForm_Volume)
		{
			// Generate a random corner, for collapsing random point.
			int nCorner = seed.GetRandom(0, 7);
			ran.vNorm.x = (((nCorner & 1) << 1) - 1) * vSize.x;
			ran.vNorm.y = (((nCorner & 2)) - 1) * vSize.y;
			ran.vNorm.z = (((nCorner & 4) >> 1) - 1) * vSize.z;

			if (eForm == GeomForm_Vertices)
			{
				ran.vPos = ran.vNorm;
			}
			else if (eForm == GeomForm_Surface)
			{
				// Collapse one axis.
				float fAxis = seed.GetRandom(0.0f, vSize.x * vSize.y + vSize.y * vSize.z + vSize.z * vSize.x);
				if ((fAxis -= vSize.y * vSize.z) < 0.f)
				{
					ran.vPos.x = ran.vNorm.x;
					ran.vNorm.y = ran.vNorm.z = 0.f;
				}
				else if ((fAxis -= vSize.z * vSize.x) < 0.f)
				{
					ran.vPos.y = ran.vNorm.y;
					ran.vNorm.x = ran.vNorm.z = 0.f;
				}
				else
				{
					ran.vPos.z = ran.vNorm.z;
					ran.vNorm.x = ran.vNorm.y = 0.f;
				}
			}
			else if (eForm == GeomForm_Edges)
			{
				// Collapse 2 axes.
				float fAxis = seed.GetRandom(0.0f, vSize.x + vSize.y + vSize.z);
				if ((fAxis -= vSize.x) < 0.f)
				{
					ran.vPos.y = ran.vNorm.y;
					ran.vPos.z = ran.vNorm.z;
					ran.vNorm.x = 0.f;
				}
				else if ((fAxis -= vSize.y) < 0.f)
				{
					ran.vPos.x = ran.vNorm.x;
					ran.vPos.z = ran.vNorm.z;
					ran.vNorm.y = 0.f;
				}
				else
				{
					ran.vPos.x = ran.vNorm.x;
					ran.vPos.y = ran.vNorm.y;
					ran.vNorm.z = 0.f;
				}
			}
		}

		ran.vNorm.Normalize();
	}
}

void SphereRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm, float fRadius)
{
	switch (eForm)
	{
	default:
		assert(0);
	case GeomForm_Vertices:
	case GeomForm_Edges:
		return points.fill(ZERO);
	case GeomForm_Surface:
	case GeomForm_Volume:
		for (auto& ran : points)
		{
			// Generate point on surface, as normal.
			float fPhi = seed.GetRandom(0.0f, gf_PI2);
			float fZ = seed.GetRandom(-1.f, 1.f);
			float fH = sqrt_tpl(1.f - fZ * fZ);
			sincos_tpl(fPhi, &ran.vNorm.y, &ran.vNorm.x);
			ran.vNorm.x *= fH;
			ran.vNorm.y *= fH;
			ran.vNorm.z = fZ;

			ran.vPos = ran.vNorm;
			if (eForm == GeomForm_Volume)
			{
				float fV = seed.GetRandom(0.0f, 1.0f);
				float fR = pow_tpl(fV, 0.333333f);
				ran.vPos *= fR;
			}
			ran.vPos *= fRadius;
		}
		break;
	}
}

void TriRandomPos(PosNorm& ran, CRndGen& seed, EGeomForm eForm, PosNorm const aRan[3], Vec3 const& vCenter, bool bDoNormals)
{
	// Generate interpolators for verts.
	switch (eForm)
	{
	default:
		assert(0);
	case GeomForm_Vertices:
		ran = aRan[0];
		return;
	case GeomForm_Edges:
	{
		float t = seed.GenerateFloat();
		ran.vPos = aRan[0].vPos * (1.f - t) + aRan[1].vPos * t;
		if (bDoNormals)
			ran.vNorm = aRan[0].vNorm * (1.f - t) + aRan[1].vNorm * t;
		break;
	}
	case GeomForm_Surface:
	{
		float a = seed.GenerateFloat();
		float b = seed.GenerateFloat();
		float t0 = std::min(a, b);
		float t1 = std::max(a, b) - t0;
		float t2 = 1.0f - t1 - t0;
		ran.vPos = aRan[0].vPos * t0 + aRan[1].vPos * t1 + aRan[2].vPos * t2;
		if (bDoNormals)
			ran.vNorm = aRan[0].vNorm * t0 + aRan[1].vNorm * t1 + aRan[2].vNorm * t2;
		break;
	}
	case GeomForm_Volume:
	{
		// Generate a point in the pyramid defined by the triangle and the origin
		float a = seed.GenerateFloat();
		float b = seed.GenerateFloat();
		float c = seed.GenerateFloat();
		float t0 = std::min(std::min(a, b), c);
		float t1 = a == t0 ? std::min(b, c) : b == t0 ? std::min(a, c) : std::min(a, b);
		float t2 = std::max(std::max(a, b), c);
		float t3 = 1.0f - t2;
		t2 -= t1;
		t1 -= t0;

		ran.vPos = aRan[0].vPos * t0 + aRan[1].vPos * t1 + aRan[2].vPos * t2 + vCenter * t3;
		if (bDoNormals)
			ran.vNorm = (aRan[0].vNorm * t0 + aRan[1].vNorm * t1 + aRan[2].vNorm * t2) * (1.f - t3) + ran.vPos.GetNormalizedFast() * t3;
		break;
	}
	}
	if (bDoNormals)
		ran.vNorm.Normalize();
}
