// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RemoveDoublesTool.h"
#include "Tools/Select/SelectTool.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "WeldTool.h"
#include "Serialization/Decorators/EditorActionButton.h"

using Serialization::ActionButton;

namespace Designer
{
void RemoveDoublesTool::Enter()
{
	BaseTool::Enter();
	if (RemoveDoubles())
	{
		GetDesigner()->SwitchToSelectTool();
		UpdateSurfaceInfo();
	}
}

void RemoveDoublesTool::Serialize(Serialization::IArchive& ar)
{
	m_RemoveDoubleParameter.Serialize(ar);
	if (ar.isEdit())
		ar(ActionButton([this] { RemoveDoubles();
		                }), "Apply", "Apply");
}

bool RemoveDoublesTool::RemoveDoubles()
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	if (!pSelected || pSelected->IsEmpty())
		return false;
	CUndo undo("Designer : Remove Doubles");
	GetModel()->RecordUndo("Designer : Remove Doubles", GetBaseObject());
	RemoveDoubles(GetMainContext(), m_RemoveDoubleParameter.m_Distance);
	pSelected->Clear();
	ApplyPostProcess();
	return true;
}

void RemoveDoublesTool::RemoveDoubles(MainContext& mc, float fDistance)
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();

	std::vector<BrushVec3> uniqueVertices;
	for (int i = 0, iSelectedElementCount(pSelected->GetCount()); i < iSelectedElementCount; ++i)
	{
		for (int k = 0, iVertexCount((*pSelected)[i].m_Vertices.size()); k < iVertexCount; ++k)
		{
			if (!HasVertexInList(uniqueVertices, (*pSelected)[i].m_Vertices[k]))
				uniqueVertices.push_back((*pSelected)[i].m_Vertices[k]);
		}
	}

	int iVertexCount(uniqueVertices.size());
	BrushVec3 vTargetPos;
	std::set<int> alreadyUsedVertices;

	while (alreadyUsedVertices.size() < iVertexCount)
	{
		for (int i = iVertexCount - 1; i >= 0; --i)
		{
			if (alreadyUsedVertices.find(i) == alreadyUsedVertices.end())
			{
				vTargetPos = uniqueVertices[i];
				alreadyUsedVertices.insert(i);
				break;
			}
		}

		for (int i = 0; i < iVertexCount; ++i)
		{
			if (alreadyUsedVertices.find(i) != alreadyUsedVertices.end())
				continue;
			if (vTargetPos.GetDistance(uniqueVertices[i]) < fDistance)
			{
				WeldTool::Weld(mc, uniqueVertices[i], vTargetPos);
				alreadyUsedVertices.insert(i);
			}
		}
	}
}

bool RemoveDoublesTool::HasVertexInList(const std::vector<BrushVec3>& vList, const BrushVec3& vPos)
{
	for (int i = 0, iVListCount(vList.size()); i < iVListCount; ++i)
	{
		if (Comparison::IsEquivalent(vList[i], vPos))
			return true;
	}
	return false;
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_RemoveDoubles, eToolGroup_Edit, "Remove Doubles", RemoveDoublesTool,
                                                           removedoubles, "runs remove doubles tool", "designer.removedoubles")

