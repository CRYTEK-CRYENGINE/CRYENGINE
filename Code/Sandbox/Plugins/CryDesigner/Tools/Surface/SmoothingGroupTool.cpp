// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SmoothingGroupTool.h"
#include "DesignerEditor.h"
#include "Core/Model.h"
#include "ToolFactory.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "DesignerSession.h"

namespace Designer
{
void SmoothingGroupTool::Enter()
{
	__super::Enter();

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	pSelected->Erase(ePF_Vertex | ePF_Edge);
	pSelected->EraseUnusedPolygonElements(GetModel());
	pSession->UpdateSelectionMeshFromSelectedElements();
	GetModel()->GetSmoothingGroupMgr()->Clean(GetModel());
}

bool IsLessThanAngleOfSeedPolygons(PolygonPtr pPolygon, const std::set<PolygonPtr>& seedPolygons, BrushFloat fRadian)
{
	auto ii = seedPolygons.begin();
	for (; ii != seedPolygons.end(); ++ii)
	{
		BrushFloat dot = (*ii)->GetPlane().Normal().Dot(pPolygon->GetPlane().Normal());
		if (std::acos(dot) <= fRadian)
			return true;
	}
	return false;
}

bool IsAdjacentWithSeedPolygons(PolygonPtr pPolygon, const std::set<PolygonPtr>& seedPolygons)
{
	auto ii = seedPolygons.begin();
	for (; ii != seedPolygons.end(); ++ii)
	{
		bool bHadCommonEdge = false;
		int nEdgeCount = pPolygon->GetEdgeCount();
		for (int k = 0; k < nEdgeCount; ++k)
		{
			BrushEdge3D e = pPolygon->GetEdge(k);
			if ((*ii)->HasEdge(e))
			{
				bHadCommonEdge = true;
				break;
			}
		}
		if (bHadCommonEdge)
			return true;
	}
	return false;
}

void SmoothingGroupTool::ApplyAutoSmooth()
{
	BrushFloat fRadian = ((BrushFloat)m_fThresholdAngle / (BrushFloat)180) * PI;

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	std::set<PolygonPtr> usedPolygons;

	SmoothingGroupManager* pSmoothingGroupMgr = GetModel()->GetSmoothingGroupMgr();
	int iSelectedElementCount(pSelected->GetCount());

	std::set<PolygonPtr> seedPolygons;
	std::vector<PolygonPtr> polygonsInGroup;

	while (1)
	{
		PolygonPtr pStartingSeedPolygon = NULL;
		if (seedPolygons.empty())
		{
			for (int i = 0; i < iSelectedElementCount; ++i)
			{
				if (usedPolygons.find((*pSelected)[i].m_pPolygon) == usedPolygons.end())
				{
					pStartingSeedPolygon = (*pSelected)[i].m_pPolygon;
					seedPolygons.insert(pStartingSeedPolygon);
					polygonsInGroup.push_back(pStartingSeedPolygon);
					usedPolygons.insert(pStartingSeedPolygon);
					break;
				}
			}
			if (seedPolygons.empty())
				break;
		}

		bool bFinishLoop = false;
		while (!bFinishLoop && !seedPolygons.empty())
		{
			int nOffset = polygonsInGroup.size();
			for (int i = 0; i < iSelectedElementCount; ++i)
			{
				PolygonPtr pPolygon = (*pSelected)[i].m_pPolygon;
				if (usedPolygons.find(pPolygon) != usedPolygons.end())
					continue;

				if (!IsLessThanAngleOfSeedPolygons(pPolygon, seedPolygons, fRadian))
					continue;

				if (!IsAdjacentWithSeedPolygons(pPolygon, seedPolygons))
					continue;

				polygonsInGroup.push_back(pPolygon);
				usedPolygons.insert(pPolygon);
			}

			seedPolygons.clear();
			if (nOffset < polygonsInGroup.size())
			{
				seedPolygons.insert(polygonsInGroup.begin() + nOffset, polygonsInGroup.end());
			}
			else
			{
				if (polygonsInGroup.empty())
				{
					bFinishLoop = true;
					break;
				}
				for (int i = 0, iPolygonCount(polygonsInGroup.size()); i < iPolygonCount; ++i)
					pSmoothingGroupMgr->RemovePolygon(polygonsInGroup[i]);
				string emptyGroupID = pSmoothingGroupMgr->GetEmptyGroupID();
				if (emptyGroupID.empty())
				{
					bFinishLoop = true;
					break;
				}
				pSmoothingGroupMgr->AddSmoothingGroup(emptyGroupID, new SmoothingGroup(polygonsInGroup));
				polygonsInGroup.clear();
				break;
			}
		}
		if (bFinishLoop)
			break;
	}

	ApplyPostProcess();
}

bool SmoothingGroupTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
		return GetDesigner()->EndCurrentDesignerTool();
	return true;
}

void SmoothingGroupTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	__super::OnEditorNotifyEvent(event);

	switch (event)
	{
	case eNotify_OnEndUndoRedo:
		// firing notifications as response to notifications is not so nice...
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
		break;
	}
}

void SmoothingGroupTool::AddSmoothingGroup()
{
	CUndo undo("Designer : Smoothing Group");
	GetModel()->RecordUndo("Designer : Add Smoothing Group", GetBaseObject());

	SmoothingGroupManager* pSmoothingGroupMgr = GetModel()->GetSmoothingGroupMgr();
	AddPolygonsToSmoothingGroup(pSmoothingGroupMgr->GetEmptyGroupID().c_str());
	ApplyPostProcess();
}

void SmoothingGroupTool::AddPolygonsToSmoothingGroup(const char* name)
{
	CUndo undo("Designer : Smoothing Group");
	GetModel()->RecordUndo("Designer : Add Polygons To Smoothing Group", GetBaseObject());

	SmoothingGroupManager* pSmoothingGroupMgr = GetModel()->GetSmoothingGroupMgr();
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSmoothingGroupMgr->AddSmoothingGroup(name, new SmoothingGroup(pSelected->GetAllPolygons()));
	ApplyPostProcess();
}

void SmoothingGroupTool::SelectPolygons(const char* name)
{
	DesignerSession* pSession = DesignerSession::GetInstance();

	SmoothingGroupManager* pSmoothingGroupMgr = GetModel()->GetSmoothingGroupMgr();
	SmoothingGroupPtr pSmoothingGroup = pSmoothingGroupMgr->GetSmoothingGroup(name);
	if (pSmoothingGroup == NULL)
		return;

	for (int i = 0, iPolygonCount(pSmoothingGroup->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = pSmoothingGroup->GetPolygon(i);
		pSession->GetSelectedElements()->Add(Element(GetBaseObject(), polygon));
	}

	pSession->UpdateSelectionMeshFromSelectedElements();
}

std::vector<std::pair<string, SmoothingGroupPtr>> SmoothingGroupTool::GetSmoothingGroupList() const
{
	return GetModel()->GetSmoothingGroupMgr()->GetSmoothingGroupList();
}

void SmoothingGroupTool::DeleteSmoothingGroup(const char* name)
{
	CUndo undo("Designer : Smoothing Group");
	GetModel()->RecordUndo("Designer : Delete Smoothing Group", GetBaseObject());

	SmoothingGroupManager* pSmoothingGroupMgr = GetModel()->GetSmoothingGroupMgr();
	pSmoothingGroupMgr->RemoveSmoothingGroup(name);
	ApplyPostProcess();
}

bool SmoothingGroupTool::RenameSmoothingGroup(const char* former_name, const char* name)
{
	SmoothingGroupManager* pSmoothingGroupMgr = GetModel()->GetSmoothingGroupMgr();
	if (pSmoothingGroupMgr->GetSmoothingGroup(name))
		return false;

	SmoothingGroupPtr pOldSmoothingGroup = pSmoothingGroupMgr->GetSmoothingGroup(former_name);
	if (pOldSmoothingGroup == NULL)
		return false;

	CUndo undo("Designer : Smoothing Group");
	GetModel()->RecordUndo("Designer : Rename Smoothing Group", GetBaseObject());

	pSmoothingGroupMgr->RemoveSmoothingGroup(former_name);
	pSmoothingGroupMgr->AddSmoothingGroup(name, pOldSmoothingGroup);

	return true;
}
}

#include "UIs/SmoothingGroupPanel.h"
REGISTER_DESIGNER_TOOL_WITH_PANEL_AND_COMMAND(eDesigner_SmoothingGroup, eToolGroup_Surface, "Smoothing Group", SmoothingGroupTool, SmoothingGroupPanel,
                                              smoothinggroup, "runs smoothing group tool", "designer.smoothinggroup")

