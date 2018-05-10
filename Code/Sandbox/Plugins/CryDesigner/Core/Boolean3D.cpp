// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BSPTree3D.h"
#include "Model.h"

namespace Designer
{
struct DesignerBooleanStruct
{
	PolygonList           polygonList;
	_smart_ptr<BSPTree3D> pBSPTree;
};

DesignerBooleanStruct GetBooleanStruct(const Model* pModel)
{
	DesignerBooleanStruct sbs;
	pModel->GetPolygonList(sbs.polygonList);
	sbs.pBSPTree = new BSPTree3D(sbs.polygonList);
	return sbs;
}

void Model::Union(Model* BModel)
{
	DesignerBooleanStruct sbsA = GetBooleanStruct(this);
	DesignerBooleanStruct sbsB = GetBooleanStruct(BModel);

	PolygonList outPolygonList;
	Clip(sbsB.pBSPTree, sbsA.polygonList, eCT_Negative, outPolygonList, eCO_Union0);
	BModel->Clip(sbsA.pBSPTree, sbsB.polygonList, eCT_Negative, outPolygonList, eCO_Union1);

	ResetAllPolygonsFromList(outPolygonList);
}

void Model::Subtract(Model* BModel)
{
	DesignerBooleanStruct sbsA = GetBooleanStruct(this);
	DesignerBooleanStruct sbsB = GetBooleanStruct(BModel);

	PolygonList outPolygonList;
	Clip(sbsB.pBSPTree, sbsA.polygonList, eCT_Negative, outPolygonList, eCO_Subtract);
	BModel->Clip(sbsA.pBSPTree, sbsB.polygonList, eCT_Positive, outPolygonList, eCO_Subtract);

	ResetAllPolygonsFromList(outPolygonList);
}

void Model::Intersect(Model* BModel)
{
	DesignerBooleanStruct sbsA = GetBooleanStruct(this);
	DesignerBooleanStruct sbsB = GetBooleanStruct(BModel);

	PolygonList outPolygonList;
	Clip(sbsB.pBSPTree, sbsA.polygonList, eCT_Positive, outPolygonList, eCO_Intersection0IncludingCoSame);
	BModel->Clip(sbsA.pBSPTree, sbsB.polygonList, eCT_Positive, outPolygonList, eCO_Intersection1IncludingCoDiff);

	ResetAllPolygonsFromList(outPolygonList);
}

void Model::ClipOutside(Model* BModel)
{
	DesignerBooleanStruct sbsA = GetBooleanStruct(this);
	DesignerBooleanStruct sbsB = GetBooleanStruct(BModel);

	PolygonList outPolygonList;
	Clip(sbsB.pBSPTree, sbsA.polygonList, eCT_Positive, outPolygonList, eCO_JustClip);

	ResetAllPolygonsFromList(outPolygonList);
}

void Model::ClipInside(Model* BModel)
{
	DesignerBooleanStruct sbsA = GetBooleanStruct(this);
	DesignerBooleanStruct sbsB = GetBooleanStruct(BModel);

	PolygonList outPolygonList;
	Clip(sbsB.pBSPTree, sbsA.polygonList, eCT_Negative, outPolygonList, eCO_JustClip);

	ResetAllPolygonsFromList(outPolygonList);
}

void Model::Clip(const BSPTree3D* pTree, PolygonList& polygonList, EClipType cliptype, PolygonList& outPolygons, EClipObjective clipObjective)
{
	if (pTree == NULL)
		return;

	for (int i = 0, iPolygonSize(polygonList.size()); i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon = polygonList[i];

		if (pPolygon->CheckFlags(ePolyFlag_Mirrored))
			continue;

		OutputPolygons outputPolygons;
		pTree->GetPartitions(polygonList[i], outputPolygons);

		std::vector<PolygonList> validPolygons;

		if (clipObjective == eCO_JustClip)
		{
			if (cliptype == eCT_Negative)
			{
				validPolygons.push_back(outputPolygons.posList);
				validPolygons.push_back(outputPolygons.coSameList);
			}
			else
			{
				validPolygons.push_back(outputPolygons.negList);
				validPolygons.push_back(outputPolygons.coDiffList);
			}
		}
		if (clipObjective == eCO_Union0)
		{
			if (cliptype == eCT_Negative)
			{
				validPolygons.push_back(outputPolygons.posList);
				validPolygons.push_back(outputPolygons.coSameList);
			}
		}
		else if (clipObjective == eCO_Union1)
		{
			if (cliptype == eCT_Negative)
				validPolygons.push_back(outputPolygons.posList);
		}
		else if (clipObjective == eCO_Intersection0 || clipObjective == eCO_Intersection0IncludingCoSame)
		{
			if (cliptype == eCT_Positive)
			{
				validPolygons.push_back(outputPolygons.negList);
				if (clipObjective == eCO_Intersection0IncludingCoSame)
					validPolygons.push_back(outputPolygons.coSameList);
			}
		}
		else if (clipObjective == eCO_Intersection1 || clipObjective == eCO_Intersection1IncludingCoDiff)
		{
			if (cliptype == eCT_Positive)
			{
				validPolygons.push_back(outputPolygons.negList);
				if (clipObjective == eCO_Intersection1IncludingCoDiff)
					validPolygons.push_back(outputPolygons.coDiffList);
			}
		}
		else if (clipObjective == eCO_Subtract)
		{
			if (cliptype == eCT_Positive)
			{
				validPolygons.push_back(outputPolygons.negList);
			}
			else if (cliptype == eCT_Negative)
			{
				validPolygons.push_back(outputPolygons.posList);
				validPolygons.push_back(outputPolygons.coDiffList);
			}
		}

		for (int a = 0; a < validPolygons.size(); ++a)
		{
			for (int k = 0, iValidPolygonsize(validPolygons[a].size()); k < iValidPolygonsize; k++)
			{
				if (clipObjective == eCO_Subtract && cliptype == eCT_Positive)
					validPolygons[a][k]->Flip();
				DESIGNER_ASSERT(!validPolygons[a][k]->IsOpen());
				if (!validPolygons[a][k]->IsOpen())
					outPolygons.push_back(validPolygons[a][k]);
			}
		}
	}
}

bool Model::IsInside(const BrushVec3& vPos) const
{
	DesignerBooleanStruct sbsA = GetBooleanStruct(this);
	return sbsA.pBSPTree->IsInside(vPos);
}

std::vector<PolygonPtr> Model::GetIntersectedParts(PolygonPtr pPolygon) const
{
	PolygonList outNegPolygons;

	DesignerBooleanStruct sbs = GetBooleanStruct(this);
	if (!sbs.pBSPTree)
		return outNegPolygons;

	OutputPolygons outPolygons;
	sbs.pBSPTree->GetPartitions(pPolygon, outPolygons);

	return outPolygons.negList;
}
};

