// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UnmapTool.h"
#include "../UVMappingEditor.h"
#include "../UVUndo.h"
#include "../UVCluster.h"

namespace Designer {
namespace UVMapping
{
void UnmapTool::Enter()
{
	if (GetUVEditor()->GetUVIslandMgr())
	{
		CUndo undo("UVMapping Editor : Remove");
		CUndo::Record(new UVIslandUndo);
		UnmapSelectedElements();
	}
	GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
	GetUVEditor()->GetElementSet()->Clear();
	GetUVEditor()->GetUVIslandMgr()->ConvertIsolatedAreasToIslands();
	GetUVEditor()->ApplyPostProcess();
}

void UnmapTool::UnmapSelectedElements()
{
	UVElementSetPtr pUVElementSet = GetUVEditor()->GetElementSet();
	std::set<PolygonPtr> removePolygons;

	for (int i = 0, iCount(pUVElementSet->GetCount()); i < iCount; ++i)
	{
		UVElement& element = pUVElementSet->GetElement(i);
		for (int k = 0, iVertexCount(element.m_UVVertices.size()); k < iVertexCount; ++k)
		{
			element.m_UVVertices[k].pUVIsland->DeletePolygon(element.m_UVVertices[k].pPolygon);
			removePolygons.insert(element.m_UVVertices[k].pPolygon);
		}

		if (element.IsEdge())
		{
			std::set<PolygonPtr> polygons;
			SearchUtil::FindPolygonsSharingEdge(element.m_UVVertices[0].GetUV(), element.m_UVVertices[1].GetUV(), element.m_pUVIsland, polygons);
			auto iPolygon = polygons.begin();
			for (; iPolygon != polygons.end(); ++iPolygon)
			{
				element.m_pUVIsland->DeletePolygon(*iPolygon);
				removePolygons.insert(*iPolygon);
			}
		}
		else if (element.IsVertex())
		{
			UVCluster cluster;
			UVElementSetPtr pVertexElementSet = new UVElementSet;
			pVertexElementSet->AddElement(element);
			cluster.SetElementSetPtr(pVertexElementSet);
			cluster.GetPolygons(removePolygons);
			cluster.RemoveUVs();
		}
		else if (element.IsUVIsland())
		{
			for (int k = 0, iPolygonCount(element.m_pUVIsland->GetCount()); k < iPolygonCount; ++k)
				removePolygons.insert(element.m_pUVIsland->GetPolygon(k));
			element.m_pUVIsland->Clear();
		}
	}

	auto iter = removePolygons.begin();
	for (; iter != removePolygons.end(); ++iter)
		(*iter)->ResetUVs();

	GetUVEditor()->GetUVIslandMgr()->RemoveEmptyUVIslands();
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Unmap, eUVMappingToolGroup_Manipulation, "Unmap", UnmapTool,
                                    unmap, "runs unmap tool", "uvmapping.unmap")

