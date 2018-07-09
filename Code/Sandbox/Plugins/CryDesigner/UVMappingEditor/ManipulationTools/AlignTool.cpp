// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../UVMappingEditor.h"
#include "AlignTool.h"
#include "../UVCluster.h"
#include "../UVUndo.h"

namespace Designer {
namespace UVMapping
{
void AlignTool::Enter()
{
	CUndo undo("UVMapping Editor : Alignment");
	CUndo::Record(new UVMoveUndo);
	AlignSelectedVertices();
	GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
}

void AlignTool::AlignSelectedVertices()
{
	UVElementSetPtr pSelected = GetUVEditor()->GetElementSet();
	if (pSelected->IsEmpty())
		return;

	AABB aabb;
	aabb.Reset();
	for (int i = 0, iCount(pSelected->GetCount()); i < iCount; ++i)
	{
		UVElement& element = pSelected->GetElement(i);
		for (int k = 0, iVertexCount(element.m_UVVertices.size()); k < iVertexCount; ++k)
			aabb.Add(element.m_UVVertices[k].GetUV());
	}

	const UVElement& lastElement = pSelected->GetElement(pSelected->GetCount() - 1);
	Vec3 alignmentPos = aabb.min;
	if (!lastElement.m_UVVertices.empty())
		alignmentPos = lastElement.m_UVVertices[lastElement.m_UVVertices.size() - 1].GetUV();

	float width = std::abs(aabb.max.x - aabb.min.x);
	float height = std::abs(aabb.max.y - aabb.min.y);
	EPrincipleAxis axis = width > height ? ePrincipleAxis_X : ePrincipleAxis_Y;
	float value = width > height ? alignmentPos.y : alignmentPos.x;

	UVCluster cluster(true);
	cluster.AlignToAxis(axis, value);
	cluster.Unshelve();
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Alignment, eUVMappingToolGroup_Manipulation, "Alignment", AlignTool,
                                    alignment, "runs alignment tool", "designer.alignment")

