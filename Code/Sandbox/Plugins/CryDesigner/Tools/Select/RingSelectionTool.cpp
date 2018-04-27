// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RingSelectionTool.h"
#include "DesignerEditor.h"
#include "Util/ElementSet.h"
#include "LoopSelectionTool.h"

namespace Designer
{
void RingSelectionTool::Enter()
{
	CUndo undo("Designer : Ring Selection");
	GetDesigner()->StoreSelectionUndo();
	RingSelection(GetMainContext());
	GetDesigner()->SwitchToPrevTool();
	DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_Select, nullptr);
}

void RingSelectionTool::RingSelection(MainContext& mc)
{
	int nSelectedElementCount = mc.pSelected->GetCount();

	if (mc.pSelected->GetCount() == mc.pSelected->GetPolygonCount())
	{
		SelectFaceRing(mc);
		return;
	}

	for (int i = 0; i < nSelectedElementCount; ++i)
	{
		if (!(*mc.pSelected)[i].IsEdge())
			continue;
		SelectRing(mc, (*mc.pSelected)[i].GetEdge());
	}
}

void RingSelectionTool::SelectRing(MainContext& mc, const BrushEdge3D& inputEdge)
{
	BrushEdge3D edge(inputEdge);

	std::vector<PolygonPtr> polygons;
	mc.pModel->QueryPolygonsSharingEdge(edge, polygons);
	if (polygons.size() == 1)
	{
		BrushEdge3D oppositeEdge;
		if (polygons[0]->FindOppositeEdge(edge, oppositeEdge))
		{
			polygons.clear();
			mc.pModel->QueryPolygonsSharingEdge(oppositeEdge, polygons);
		}
		else
		{
			return;
		}
	}

	if (polygons.size() != 2)
		return;

	std::vector<PolygonPtr> loopPolygons;
	loopPolygons.push_back(polygons[0]);
	loopPolygons.push_back(polygons[1]);
	LoopSelectionTool::GetLoopPolygonsInBothWays(mc, polygons[0], polygons[1], loopPolygons);

	for (int i = 0, iLoopPolygonCount(loopPolygons.size()); i < iLoopPolygonCount; ++i)
	{
		PolygonPtr pPolygon = loopPolygons[i];
		PolygonPtr pNextPolygon = loopPolygons[(i + 1) % iLoopPolygonCount];

		BrushEdge3D sharing_edge;
		if (pPolygon->FindSharingEdge(pNextPolygon, sharing_edge))
		{
			mc.pSelected->Add(Element(mc.pObject, sharing_edge));
		}
		else
		{
			if (loopPolygons[0]->FindSharingEdge(loopPolygons[1], sharing_edge))
			{
				BrushEdge3D oppositeEdge;
				if (loopPolygons[0]->FindOppositeEdge(sharing_edge, oppositeEdge))
					mc.pSelected->Add(Element(mc.pObject, oppositeEdge));
			}

			if (loopPolygons[iLoopPolygonCount - 1]->FindSharingEdge(loopPolygons[iLoopPolygonCount - 2], sharing_edge))
			{
				BrushEdge3D oppositeEdge;
				if (loopPolygons[iLoopPolygonCount - 1]->FindOppositeEdge(sharing_edge, oppositeEdge))
					mc.pSelected->Add(Element(mc.pObject, oppositeEdge));
			}
		}
	}
}

void SelectLoopFromOnePolygon(MainContext& mc, const std::vector<Vertex>& quad, PolygonPtr pQuadPolygon, int nEdgeIndex)
{
	std::vector<PolygonPtr> adjacentPolygons;
	mc.pModel->QueryPolygonsSharingEdge(BrushEdge3D(quad[(nEdgeIndex + 1) % 4].pos, quad[(nEdgeIndex + 2) % 4].pos), adjacentPolygons);
	if (adjacentPolygons.size() != 2)
	{
		adjacentPolygons.clear();
		mc.pModel->QueryPolygonsSharingEdge(BrushEdge3D(quad[(nEdgeIndex + 3) % 4].pos, quad[nEdgeIndex].pos), adjacentPolygons);
	}
	for (int c = 0; c < adjacentPolygons.size(); ++c)
	{
		if (pQuadPolygon == adjacentPolygons[c])
			continue;
		mc.pSelected->Add(Element(mc.pObject, adjacentPolygons[c]));
		LoopSelectionTool::SelectPolygonLoop(mc, pQuadPolygon, adjacentPolygons[c]);
		LoopSelectionTool::SelectPolygonLoop(mc, adjacentPolygons[c], pQuadPolygon);
	}
}

void RingSelectionTool::SelectFaceRing(MainContext& mc)
{
	int nSelectedElementCount = mc.pSelected->GetCount();

	std::vector<PolygonPtr> quads;

	for (int i = 0; i < nSelectedElementCount; ++i)
	{
		const Element& element = mc.pSelected->Get(i);
		if (!element.IsPolygon() || !element.m_pPolygon || element.m_pPolygon->GetEdgeCount() != 4)
			continue;
		quads.push_back(element.m_pPolygon);
	}

	if (quads.empty())
		return;

	std::set<int> usedQuads;
	int quadCount = quads.size();
	for (int i = 0; i < quadCount; ++i)
	{
		if (usedQuads.find(i) != usedQuads.end())
			continue;

		usedQuads.insert(i);

		std::vector<Vertex> quad0;
		quads[i]->GetLinkedVertices(quad0);

		bool bDone = false;
		for (int k = 0; k < quadCount; ++k)
		{
			if (i == k)
				continue;

			std::vector<Vertex> quad1;
			quads[k]->GetLinkedVertices(quad1);

			for (int a = 0; a < 4; ++a)
			{
				BrushEdge3D e0(quad0[a].pos, quad0[(a + 1) % 4].pos);
				for (int b = 0; b < 4; ++b)
				{
					BrushEdge3D e1(quad1[(b + 1) % 4].pos, quad1[b].pos);
					if (e0.IsEquivalent(e1))
					{
						if (usedQuads.find(k) == usedQuads.end())
							SelectLoopFromOnePolygon(mc, quad1, quads[k], b);
						SelectLoopFromOnePolygon(mc, quad0, quads[i], a);
						usedQuads.insert(k);
						bDone = true;
					}
				}
				if (bDone)
					break;
			}
			if (bDone)
				break;
		}
	}
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Ring, eToolGroup_Selection, "Ring", RingSelectionTool,
                                   ringselection, "runs ring selection tool", "designer.ringselection")

