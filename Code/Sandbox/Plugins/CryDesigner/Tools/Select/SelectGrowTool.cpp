// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectGrowTool.h"
#include "DesignerEditor.h"
#include "SelectGrowTool.h"
#include "Util/ElementSet.h"
#include "DesignerSession.h"

namespace Designer
{
void SelectGrowTool::Enter()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	if (pSelected->IsEmpty())
	{
		MessageBox("Warning", "At least one element should be selected to use this selection.");
		GetDesigner()->SwitchToPrevTool();
		return;
	}
	CUndo undo("Designer : Growing Selection");
	GetDesigner()->StoreSelectionUndo();
	GrowSelection(GetMainContext());
	GetDesigner()->SwitchToPrevTool();
	pSession->signalDesignerEvent(eDesignerNotify_Select, nullptr);
}

void SelectGrowTool::GrowSelection(MainContext& mc)
{
	SelectAdjacentElements(mc);
}

bool SelectGrowTool::SelectAdjacentEdgesToPolygonVertex(CBaseObject* pObject, ElementSet* pSelected, PolygonPtr pPolygon, int index)
{
	int prevEdge, nextEdge;
	if (!pPolygon->GetAdjacentEdgesByVertexIndex(index, &prevEdge, &nextEdge))
	{
		return false;
	}

	bool bAddedNewAdjacent = false;
	if (prevEdge != -1)
	{
		const BrushEdge3D edge = pPolygon->GetEdge(prevEdge);
		const Element newEle(pObject, edge);
		bAddedNewAdjacent |= pSelected->Add(newEle);
	}

	if (nextEdge != -1)
	{
		const BrushEdge3D edge = pPolygon->GetEdge(nextEdge);
		const Element newEle(pObject, edge);
		bAddedNewAdjacent |= pSelected->Add(newEle);
	}

	return bAddedNewAdjacent;
}

bool SelectGrowTool::SelectAdjacentElements(MainContext& mc)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();

	if (pSelected->IsEmpty())
	{
		return false;
	}

	// save the initial selection locally, since we will be adding to the selection during iteration
	std::vector<Element> initSelection;
	initSelection.assign(&((*pSelected)[0]), &((*pSelected)[0]) + pSelected->GetCount());

	bool bAddedNewAdjacent = false;
	for (int i = 0, nPolygonCount(mc.pModel->GetPolygonCount()); i < nPolygonCount; ++i)
	{
		const PolygonPtr pPolygon = mc.pModel->GetPolygon(i);
		DESIGNER_ASSERT(pPolygon);
		if (!pPolygon || pPolygon->CheckFlags(ePolyFlag_Hidden))
			continue;

		for (int i = 0, iSelectionCount(initSelection.size()); i < iSelectionCount; ++i)
		{
			const Element& ele = initSelection[i];

			if (ele.IsPolygon())
			{
				if (pPolygon->HasOverlappedEdges(ele.m_pPolygon))
				{
					bAddedNewAdjacent |= pSelected->Add(Element(mc.pObject, pPolygon));
				}
			}
			else if (ele.IsVertex())
			{
				int index;
				int prevEdge, nextEdge;
				if (pPolygon->GetVertexIndex(ele.GetPos(), index) 
					&& pPolygon->GetAdjacentEdgesByVertexIndex(index, &prevEdge, &nextEdge))
				{
					if (prevEdge != -1)
					{
						const IndexPair& edgePair = pPolygon->GetEdgeIndexPair(prevEdge);
						const int otherIndex = edgePair.m_i[0] == index ? edgePair.m_i[1] : edgePair.m_i[0];
						const Element newEle(mc.pObject, pPolygon->GetVertex(otherIndex).pos);
						bAddedNewAdjacent |= pSelected->Add(newEle);
					}

					if (nextEdge != -1)
					{
						const IndexPair& edgePair = pPolygon->GetEdgeIndexPair(nextEdge);
						const int otherIndex = edgePair.m_i[0] == index ? edgePair.m_i[1] : edgePair.m_i[0];
						const Element newEle(mc.pObject, pPolygon->GetVertex(otherIndex).pos);
						bAddedNewAdjacent |= pSelected->Add(newEle);
					}
				}
			}
			else if (ele.IsEdge())
			{
				int index;
				const BrushEdge3D edge = ele.GetEdge();

				if (pPolygon->GetVertexIndex(edge.m_v[0], index))
				{
					bAddedNewAdjacent |= SelectAdjacentEdgesToPolygonVertex(mc.pObject, pSelected, pPolygon, index);
				}
				
				if (pPolygon->GetVertexIndex(edge.m_v[1], index))
				{
					bAddedNewAdjacent |= SelectAdjacentEdgesToPolygonVertex(mc.pObject, pSelected, pPolygon, index);
				}
			}
		}
	}

	return bAddedNewAdjacent;
}
}
REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Grow, eToolGroup_Selection, "Grow", SelectGrowTool,
                                   growselection, "runs grow selection tool", "designer.growselection");

