// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrimitiveShape.h"
#include "SurfaceInfoPicker.h"
#include "ViewManager.h"
#include "Core/Helper.h"
#include "Core/PolygonDecomposer.h"

namespace Designer
{
void PrimitiveShape::CreateBox(const BrushVec3& mins, const BrushVec3& maxs, std::vector<PolygonPtr>* pOutPolygonList) const
{
	for (int i = 0; i < 3; ++i)
	{
		if (maxs[i] < mins[i])
			CLogFile::WriteLine("Error: Failed to Create a box.");
	}

	BrushVec3 vlist[] = {
		BrushVec3(mins[0], mins[1], mins[2]),
		BrushVec3(mins[0], maxs[1], mins[2]),
		BrushVec3(maxs[0], maxs[1], mins[2]),
		BrushVec3(maxs[0], mins[1], mins[2]),
		BrushVec3(mins[0], mins[1], maxs[2]),
		BrushVec3(mins[0], maxs[1], maxs[2]),
		BrushVec3(maxs[0], maxs[1], maxs[2]),
		BrushVec3(maxs[0], mins[1], maxs[2])
	};

	int indexlist[][4] = {
		{ 0, 4, 5, 1 },
		{ 1, 5, 6, 2 },
		{ 2, 6, 7, 3 },
		{ 3, 7, 4, 0 },
		{ 6, 5, 4, 7 },
		{ 0, 1, 2, 3 }
	};

	std::vector<PolygonPtr> polygonList;

	int indexnum = sizeof(indexlist) / sizeof(*indexlist);
	for (int i = 0; i < indexnum; ++i)
	{
		std::vector<Vertex> vList;
		vList.reserve(4);
		for (int k = 0; k < 4; ++k)
			vList.push_back(Vertex(vlist[indexlist[i][k]]));
		BrushPlane plane;
		if (ComputePlane(vList, plane))
		{
			Polygon* pPolygon = new Polygon(vList);
			if (pPolygon->IsValid() && !pPolygon->IsOpen())
				polygonList.push_back(pPolygon);
		}
	}

	if (pOutPolygonList)
		*pOutPolygonList = polygonList;
}

void PrimitiveShape::CreateSphere(const BrushVec3& mins, const BrushVec3& maxs, int numSides, std::vector<PolygonPtr>* pOutPolygonList) const
{
	BrushFloat radius = (Vec2(maxs.x, mins.y) - Vec2(mins.x, mins.y)).GetLength() * (BrushFloat)0.5;
	BrushVec3 vOffset = (maxs + mins) * (BrushFloat)0.5 + BrushVec3(0, 0, radius);
	CreateSphere(vOffset, radius, numSides, pOutPolygonList);
}

void PrimitiveShape::CreateSphere(const BrushVec3& vCenter, float radius, int numSides, std::vector<PolygonPtr>* pOutPolygonList) const
{
	DESIGNER_ASSERT(numSides >= 3);
	if (numSides < 3)
		return;

	BrushFloat costheta(radius * std::cos((BrushFloat)0));
	BrushFloat sintheta(radius * std::sin((BrushFloat)0));
	BrushFloat cosnexttheta(costheta);
	BrushFloat sinnexttheta(sintheta);

	std::vector<PolygonPtr> polygonList;

	for (int i = 1; i < numSides + 1; ++i)
	{
		BrushFloat nexttheta = PI * (BrushFloat)i / (BrushFloat)numSides;

		cosnexttheta = radius * std::cos(nexttheta);
		sinnexttheta = radius * std::sin(nexttheta);

		BrushFloat cosphi(std::cos((BrushFloat)0));
		BrushFloat sinphi(std::sin((BrushFloat)0));
		BrushFloat cosnextphi(cosphi);
		BrushFloat sinnextphi(sinphi);

		for (int j = 1; j < numSides + 1; ++j)
		{
			BrushFloat nextphi = 2 * PI * (BrushFloat)j / (BrushFloat)numSides;

			cosnextphi = std::cos(nextphi);
			sinnextphi = std::sin(nextphi);

			std::vector<BrushVec3> vList;

			if (i == 1 || i == numSides)
			{
				vList.reserve(3);
				vList.push_back(BrushVec3(sintheta * cosphi, sintheta * sinphi, costheta) + vCenter);

				if (i == 1)
				{
					vList.push_back(BrushVec3(sinnexttheta * cosphi, sinnexttheta * sinphi, cosnexttheta) + vCenter);
					vList.push_back(BrushVec3(sinnexttheta * cosnextphi, sinnexttheta * sinnextphi, cosnexttheta) + vCenter);
				}
				else if (i == numSides)
				{
					vList.push_back(BrushVec3(sinnexttheta * cosnextphi, sinnexttheta * sinnextphi, cosnexttheta) + vCenter);
					vList.push_back(BrushVec3(sintheta * cosnextphi, sintheta * sinnextphi, costheta) + vCenter);
				}

				polygonList.push_back(new Polygon(vList));
			}
			else
			{
				vList.reserve(4);
				vList.push_back(BrushVec3(sinnexttheta * cosphi, sinnexttheta * sinphi, cosnexttheta) + vCenter);
				vList.push_back(BrushVec3(sinnexttheta * cosnextphi, sinnexttheta * sinnextphi, cosnexttheta) + vCenter);
				vList.push_back(BrushVec3(sintheta * cosnextphi, sintheta * sinnextphi, costheta) + vCenter);
				vList.push_back(BrushVec3(sintheta * cosphi, sintheta * sinphi, costheta) + vCenter);
				polygonList.push_back(new Polygon(vList));
			}

			cosphi = cosnextphi;
			sinphi = sinnextphi;
		}

		costheta = cosnexttheta;
		sintheta = sinnexttheta;
	}

	if (pOutPolygonList)
		*pOutPolygonList = polygonList;
}

void PrimitiveShape::CreateCylinder(const BrushVec3& mins, const BrushVec3& maxs, int numSides, std::vector<PolygonPtr>* pOutPolygonList) const
{
	DESIGNER_ASSERT(numSides >= 3);
	if (numSides < 3)
		return;

	BrushVec3 bottomMax(maxs.x, maxs.y, 0);
	BrushVec3 bottomMin(mins.x, mins.y, 0);
	BrushFloat radius = (bottomMax - bottomMin).GetLength() * (BrushFloat)0.5;
	BrushVec3 bottomCenter = (bottomMin + bottomMax) * (BrushFloat)0.5;

	BrushFloat costheta(cosf(0));
	BrushFloat sintheta(sinf(0));

	std::vector<PolygonPtr> polygonList;

	std::vector<BrushVec3> bottomFace;
	bottomFace.resize(numSides);
	for (int i = 0; i < numSides; ++i)
	{
		BrushFloat theta = 2 * PI * (BrushFloat)i / (BrushFloat)numSides;
		costheta = cosf(theta);
		sintheta = sinf(theta);
		BrushFloat x = radius * costheta;
		BrushFloat y = radius * sintheta;
		bottomFace[numSides - i - 1] = BrushVec3(x, y, mins.z) + bottomCenter;
	}

	std::vector<BrushVec3> topFace;
	topFace.resize(numSides);
	for (int i = 0; i < numSides; ++i)
	{
		const BrushVec3& v = bottomFace[i];
		topFace[numSides - i - 1] = BrushVec3(v.x, v.y, maxs.z);
	}

	Polygon* pBottomPolygon = new Polygon(bottomFace);
	Polygon* pTopPolygon = new Polygon(topFace);

	polygonList.push_back(pBottomPolygon);
	polygonList.push_back(pTopPolygon);

	for (int i = 0; i < numSides; ++i)
	{
		std::vector<BrushVec3> vSideList;
		vSideList.reserve(4);

		int nexti = (i + 1) % numSides;

		vSideList.push_back(topFace[numSides - i - 1]);
		vSideList.push_back(topFace[numSides - nexti - 1]);
		vSideList.push_back(bottomFace[nexti]);
		vSideList.push_back(bottomFace[i]);

		PolygonPtr pPolygon = new Polygon(vSideList);
		if (pPolygon->IsValid() && !pPolygon->IsOpen())
			polygonList.push_back(new Polygon(vSideList));
	}

	if (pOutPolygonList)
		*pOutPolygonList = polygonList;
}

void PrimitiveShape::CreateCylinder(PolygonPtr pBaseDiscPolygon, float fHeight, std::vector<PolygonPtr>* pOutPolygonList) const
{
	BrushVec3 vCenter = pBaseDiscPolygon->GetBoundBox().GetCenter();

	std::vector<PolygonPtr> polygonList;

	PolygonPtr pTopPolygon = pBaseDiscPolygon->Clone()->Flip();
	BrushMatrix34 matToTop = BrushMatrix34::CreateIdentity();
	matToTop.SetTranslation(pBaseDiscPolygon->GetPlane().Normal() * (-fHeight));
	pTopPolygon->Transform(matToTop);

	polygonList.push_back(pTopPolygon);

	for (int i = 0, iEdgeCount(pBaseDiscPolygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		BrushEdge3D baseEdge = pBaseDiscPolygon->GetEdge(i);
		BrushEdge3D topEdge = pTopPolygon->GetEdge(iEdgeCount - i - 1);

		std::vector<BrushVec3> vSideList;
		vSideList.reserve(4);
		vSideList.push_back(topEdge.m_v[1]);
		vSideList.push_back(topEdge.m_v[0]);
		vSideList.push_back(baseEdge.m_v[1]);
		vSideList.push_back(baseEdge.m_v[0]);

		PolygonPtr pPolygon = new Polygon(vSideList);
		if (pPolygon->IsValid() && !pPolygon->IsOpen())
			polygonList.push_back(pPolygon);
	}

	polygonList.push_back(pBaseDiscPolygon->Clone());

	if (pOutPolygonList)
		*pOutPolygonList = polygonList;
}

void PrimitiveShape::CreateCone(const BrushVec3& mins, const BrushVec3& maxs, int numSides, std::vector<PolygonPtr>* pOutPolygonList) const
{
	DESIGNER_ASSERT(numSides >= 3);
	if (numSides < 3)
		return;

	std::vector<PolygonPtr> polygonList;
	std::vector<BrushVec3> vBottomList;

	CreateCircle(mins, maxs, numSides, vBottomList);
	polygonList.push_back(new Polygon(vBottomList));

	BrushVec3 bottomCenter((BrushVec3(maxs.x, maxs.y, 0) + BrushVec3(mins.x, mins.y, 0)) * (BrushFloat)0.5);
	BrushVec3 peak(bottomCenter.x, bottomCenter.y, maxs.z);

	for (int i = 0; i < numSides; ++i)
	{
		std::vector<BrushVec3> vSideList;
		vSideList.reserve(3);
		vSideList.push_back(peak);
		vSideList.push_back(vBottomList[(i + 1) % numSides]);
		vSideList.push_back(vBottomList[i]);
		PolygonPtr pPolygon = new Polygon(vSideList);
		if (pPolygon->IsValid() && !pPolygon->IsOpen())
			polygonList.push_back(pPolygon);
	}

	if (pOutPolygonList)
		*pOutPolygonList = polygonList;
}

void PrimitiveShape::CreateCone(PolygonPtr pBaseDiscPolygon, float fHeight, std::vector<PolygonPtr>* pOutPolygonList) const
{
	BrushVec3 vCenter = pBaseDiscPolygon->GetBoundBox().GetCenter();
	BrushVec3 peak = vCenter + pBaseDiscPolygon->GetPlane().Normal() * (-fHeight);

	std::vector<PolygonPtr> polygonList;

	for (int i = 0, iEdgeCount(pBaseDiscPolygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		BrushEdge3D edge = pBaseDiscPolygon->GetEdge(i);
		std::vector<BrushVec3> vSideList;
		vSideList.reserve(3);
		vSideList.push_back(peak);
		vSideList.push_back(edge.m_v[1]);
		vSideList.push_back(edge.m_v[0]);
		PolygonPtr pPolygon = new Polygon(vSideList);
		if (pPolygon->IsValid() && !pPolygon->IsOpen())
			polygonList.push_back(pPolygon);
	}

	polygonList.push_back(pBaseDiscPolygon->Clone());

	if (pOutPolygonList)
		*pOutPolygonList = polygonList;
}

void PrimitiveShape::CreateRectangle(const BrushVec3& mins, const BrushVec3& maxs, std::vector<PolygonPtr>* pOutPolygonList) const
{
	if (maxs[0] < mins[0] || maxs[1] < mins[1])
		CLogFile::WriteLine("Error: Failed to Create a Rectangle");

	std::vector<Vertex> vList;
	vList.reserve(4);
	vList.push_back(Vertex(BrushVec3(mins[0], mins[1], mins[2])));
	vList.push_back(Vertex(BrushVec3(maxs[0], mins[1], mins[2])));
	vList.push_back(Vertex(BrushVec3(maxs[0], maxs[1], mins[2])));
	vList.push_back(Vertex(BrushVec3(mins[0], maxs[1], mins[2])));

	BrushPlane plane;
	if (!ComputePlane(vList, plane))
		return;

	std::vector<PolygonPtr> polygonList;
	polygonList.push_back(new Polygon(vList));

	if (pOutPolygonList)
		*pOutPolygonList = polygonList;
}

void PrimitiveShape::CreateDisc(const BrushVec3& mins, const BrushVec3& maxs, int numSides, std::vector<PolygonPtr>* pOutPolygonList) const
{
	DESIGNER_ASSERT(numSides >= 3);

	std::vector<BrushVec3> vBottomList;
	CreateCircle(mins, maxs, numSides, vBottomList);

	std::vector<BrushVec3> vBottomReverseList;

	vBottomReverseList.insert(vBottomReverseList.end(), vBottomList.rbegin(), vBottomList.rend());

	std::vector<PolygonPtr> polygonList;
	polygonList.push_back(new Polygon(vBottomReverseList));

	if (pOutPolygonList)
		*pOutPolygonList = polygonList;
}

void PrimitiveShape::CreateCircle(const BrushVec3& mins, const BrushVec3& maxs, int numSides, std::vector<BrushVec3>& outVertexList) const
{
	BrushVec3 bottomMax(maxs.x, maxs.y, 0);
	BrushVec3 bottomMin(mins.x, mins.y, 0);
	BrushFloat radius = (bottomMax - bottomMin).GetLength() * (BrushFloat)0.5;
	BrushVec3 bottomCenter((bottomMax + bottomMin) * (BrushFloat)0.5);

	BrushFloat costheta(std::cos((BrushFloat)0));
	BrushFloat sintheta(std::sinf((BrushFloat)0));

	std::vector<PolygonPtr> polygonList;

	outVertexList.clear();
	outVertexList.resize(numSides);
	for (int i = 0; i < numSides; ++i)
	{
		BrushFloat theta = 2 * PI * (BrushFloat)i / (BrushFloat)numSides;

		costheta = cosf(theta);
		sintheta = sinf(theta);

		BrushFloat x = radius * costheta;
		BrushFloat y = radius * sintheta;

		outVertexList[numSides - i - 1] = BrushVec3(x, y, mins.z) + bottomCenter;
	}
}
}

