// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubdivisionTool.h"
#include "Core/Model.h"
#include "Core/EdgesSharpnessManager.h"
#include "Util/ElementSet.h"
#include "Core/Subdivision.h"
#include "Core/SmoothingGroup.h"
#include "Core/SmoothingGroupManager.h"
#include "DesignerEditor.h"
#include "Objects/DesignerObject.h"
#include "Objects/DisplayContext.h"
#include "Util/ExcludedEdgeManager.h"

namespace Designer
{
void SubdivisionTool::Enter()
{
	__super::Enter();
	DesignerSession* pSession = DesignerSession::GetInstance();
	pSession->GetExcludedEdgeManager()->Clear();
	m_SelectedEdgesAsEnter.clear();
	ElementSet* pSelected = pSession->GetSelectedElements();
	for (int i = 0, iSize(pSelected->GetCount()); i < iSize; ++i)
	{
		const Element& element = pSelected->Get(i);
		if (element.IsEdge())
		{
			pSession->GetExcludedEdgeManager()->Add(element.GetEdge());
			m_SelectedEdgesAsEnter.push_back(element.GetEdge());
		}
		else if (element.IsPolygon() && element.m_pPolygon)
		{
			for (int k = 0, iEdgeCount(element.m_pPolygon->GetEdgeCount()); k < iEdgeCount; ++k)
			{
				const BrushEdge3D& edge = element.m_pPolygon->GetEdge(k);
				pSession->GetExcludedEdgeManager()->Add(edge);
				m_SelectedEdgesAsEnter.push_back(edge);
			}
		}
	}
	pSelected->Clear();
}

void SubdivisionTool::Leave()
{
	__super::Leave();
	DesignerSession* pSession = DesignerSession::GetInstance(); 
	m_HighlightedSharpEdges.clear();
	pSession->GetExcludedEdgeManager()->Clear();
}

void SubdivisionTool::Subdivide(int nLevel, bool bUpdateBrush)
{
	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	for (int i = 0, iCount(pSelection->GetCount()); i < iCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);
		if (!pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
			continue;
		DesignerObject* pDesignerObj = (DesignerObject*)pObj;
		pDesignerObj->GetModel()->SetSubdivisionLevel(nLevel);
		if (bUpdateBrush)
			Designer::ApplyPostProcess(pDesignerObj->GetMainContext());
	}
}

void SubdivisionTool::HighlightEdgeGroup(const char* edgeGroupName)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	EdgeSharpnessManager* pEdgeSharpnessMgr = GetModel()->GetEdgeSharpnessMgr();
	EdgeSharpness* pEdgeSharpness = pEdgeSharpnessMgr->FindEdgeSharpness(edgeGroupName);
	if (pEdgeSharpness == NULL)
	{
		m_HighlightedSharpEdges.clear();
		pSession->GetExcludedEdgeManager()->Clear();
		return;
	}
	m_HighlightedSharpEdges = pEdgeSharpness->edges;
	for (int i = 0, iSize(m_HighlightedSharpEdges.size()); i < iSize; ++i)
		pSession->GetExcludedEdgeManager()->Add(m_HighlightedSharpEdges[i]);
}

void SubdivisionTool::AddNewEdgeTag()
{
	if (m_SelectedEdgesAsEnter.empty())
		return;
	EdgeSharpnessManager* pEdgeSharpnessMgr = GetModel()->GetEdgeSharpnessMgr();
	string newEdgeGroupName = pEdgeSharpnessMgr->GenerateValidName();
	pEdgeSharpnessMgr->AddEdges(newEdgeGroupName, m_SelectedEdgesAsEnter, 1);
	HighlightEdgeGroup(newEdgeGroupName);
	m_SelectedEdgesAsEnter.clear();
	ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SyncPrefab);
}

void SubdivisionTool::DeleteEdgeTag(const char* name)
{
	if (name == NULL)
		return;
	EdgeSharpnessManager* pEdgeSharpnessMgr = GetModel()->GetEdgeSharpnessMgr();
	pEdgeSharpnessMgr->RemoveEdgeSharpness(name);
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Clear();
	ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SyncPrefab);
}

void SubdivisionTool::Display(DisplayContext& dc)
{
	dc.SetLineWidth(7);
	dc.SetColor(ColorB(150, 255, 50, 255));
	for (int i = 0, iCount(m_HighlightedSharpEdges.size()); i < iCount; ++i)
		dc.DrawLine(m_HighlightedSharpEdges[i].m_v[0], m_HighlightedSharpEdges[i].m_v[1]);
	dc.DepthTestOn();

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	dc.SetColor(kSelectedColor);
	dc.SetLineWidth(kChosenLineThickness);
	for (int i = 0, iCount(m_SelectedEdgesAsEnter.size()); i < iCount; ++i)
		dc.DrawLine(m_SelectedEdgesAsEnter[i].m_v[0], m_SelectedEdgesAsEnter[i].m_v[1]);
}

void SubdivisionTool::ApplySubdividedMesh()
{
	int nSubdivisionLevel = GetModel()->GetSubdivisionLevel();
	Subdivision subdivision;
	SubdivisionContext sc = subdivision.CreateSubdividedMesh(GetModel(), nSubdivisionLevel);

	_smart_ptr<Model> pModel = sc.fullPatches->CreateModel(GetModel()->GetUVIslandMgr());
	if (pModel == NULL)
		return;

	pModel->SetSubdivisionLevel(0);

	if (GetModel()->IsSmoothingSurfaceForSubdividedMeshEnabled())
	{
		SmoothingGroupManager* pSmoothingMgr = pModel->GetSmoothingGroupMgr();
		std::vector<PolygonPtr> polygons;
		pModel->GetPolygonList(polygons);
		pSmoothingMgr->AddSmoothingGroup("SG From Subdivision", new SmoothingGroup(polygons));
	}

	(*GetModel()) = *pModel;

	DesignerSession::GetInstance()->GetSelectedElements()->Clear();
	ApplyPostProcess();
	GetDesigner()->SwitchTool(eDesigner_Invalid);
}

void SubdivisionTool::DeleteAllUnused()
{
	EdgeSharpnessManager* pEdgeSharpnessMgr = GetModel()->GetEdgeSharpnessMgr();
	std::vector<BrushEdge3D> deletedEdges;

	for (int i = 0, iCount(pEdgeSharpnessMgr->GetCount()); i < iCount; ++i)
	{
		const EdgeSharpness& sharpness = pEdgeSharpnessMgr->Get(i);
		for (int k = 0, iEdgeCount(sharpness.edges.size()); k < iEdgeCount; ++k)
		{
			if (!GetModel()->HasEdge(sharpness.edges[k]))
				deletedEdges.push_back(sharpness.edges[k]);
		}
	}

	for (int i = 0, iCount(deletedEdges.size()); i < iCount; ++i)
		pEdgeSharpnessMgr->RemoveEdgeSharpness(deletedEdges[i]);
}
}

#include "UIs/SubdivisionPanel.h"
REGISTER_DESIGNER_TOOL_WITH_PANEL_AND_COMMAND(eDesigner_Subdivision, eToolGroup_Modify, "Subdivision", SubdivisionTool, SubdivisionPanel,
                                              subdivision, "runs subdivision tool", "designer.subdivision")

