// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CollapseTool.h"
#include "WeldTool.h"
#include "DesignerEditor.h"
#include "Core/Model.h"
#include "Util/ElementSet.h"

namespace Designer
{

void CollapseTool::Enter()
{
	ElementSet* pElements = DesignerSession::GetInstance()->GetSelectedElements();

	std::vector<BrushEdge3D> edgeList;
	for (int i = 0, iElementCount(pElements->GetCount()); i < iElementCount; ++i)
	{
		const Element& element = pElements->Get(i);

		if (element.IsEdge())
		{
			if (!HasEdgeInEdgeList(edgeList, element.GetEdge()))
				edgeList.push_back(element.GetEdge());
		}
		else if (element.IsPolygon() && element.m_pPolygon)
		{
			for (int k = 0, iEdgeCount(element.m_pPolygon->GetEdgeCount()); k < iEdgeCount; ++k)
			{
				BrushEdge3D e = element.m_pPolygon->GetEdge(k);
				if (!HasEdgeInEdgeList(edgeList, e))
					edgeList.push_back(e);
			}
		}
	}

	if (edgeList.empty())
		return;

	CUndo undo("Designer : Collapse");
	GetModel()->RecordUndo("Designer : Collapse", GetBaseObject());

	std::set<int> usedEdgeIndices;
	for (int i = 0, iEdgeCount(edgeList.size()); i < iEdgeCount; ++i)
	{
		if (usedEdgeIndices.find(i) != usedEdgeIndices.end())
			continue;

		const BrushEdge3D& e0 = edgeList[i];
		usedEdgeIndices.insert(i);
		std::vector<BrushVec3> connectedSet;
		connectedSet.push_back(e0.m_v[0]);
		connectedSet.push_back(e0.m_v[1]);

		bool bAdded = true;
		while (bAdded)
		{
			bAdded = false;
			for (int k = 0; k < iEdgeCount; ++k)
			{
				if (usedEdgeIndices.find(k) != usedEdgeIndices.end())
					continue;

				const BrushEdge3D& e1 = edgeList[k];

				bool bHasFirstVertex = HasVertexInVertexList(connectedSet, e1.m_v[0]);
				bool bHasSecondVertex = HasVertexInVertexList(connectedSet, e1.m_v[1]);

				if (bHasFirstVertex || bHasSecondVertex)
				{
					if (!bHasFirstVertex)
						connectedSet.push_back(e1.m_v[0]);
					if (!bHasSecondVertex)
						connectedSet.push_back(e1.m_v[1]);
					usedEdgeIndices.insert(k);
					bAdded = true;
				}
			}
		}

		AABB aabbConnected;
		aabbConnected.Reset();
		int iConnectedCount(connectedSet.size());
		for (int k = 0; k < iConnectedCount; ++k)
			aabbConnected.Add(connectedSet[k]);
		BrushVec3 vCenterPos = aabbConnected.GetCenter();

		for (int k = 0; k < iConnectedCount; ++k)
			WeldTool::Weld(GetMainContext(), connectedSet[k], vCenterPos);
	}

	UpdateSurfaceInfo();
	ApplyPostProcess();
	pElements->Clear();
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Collapse, eToolGroup_Edit, "Collapse", CollapseTool,
                                   collapse, "runs collapse tool", "designer.collapse");

