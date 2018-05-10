// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FillTool.h"
#include "Tools/Select/SelectTool.h"
#include "Core/Model.h"
#include "Core/Helper.h"
#include "Core/LoopPolygons.h"
#include "DesignerEditor.h"
#include "Viewport.h"
#include "Util/ExcludedEdgeManager.h"

namespace Designer
{
void FillTool::Enter()
{
	__super::Enter();
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	if (!pSelected->IsEmpty())
	{
		if (FillHoleBasedOnSelectedElements())
		{
			pSelected->Clear();
			pSession->GetExcludedEdgeManager()->Clear();
			GetDesigner()->SwitchToSelectTool();
			return;
		}
	}

	CompileHoles();
}

void FillTool::Leave()
{
	m_pHoleContainer = NULL;
	__super::Leave();
}

bool FillTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	int nQueriedIndex = -1;
	DesignerSession* pSession = DesignerSession::GetInstance();

	if (m_pHoleContainer->QueryPolygon(GetDesigner()->GetRay(), nQueriedIndex))
	{
		m_PickedHolePolygon = m_pHoleContainer->GetPolygon(nQueriedIndex);
		pSession->UpdateSelectionMesh(m_PickedHolePolygon, GetCompiler(), GetBaseObject());
	}
	else
	{
		m_PickedHolePolygon = NULL;
		pSession->UpdateSelectionMesh(NULL, GetCompiler(), GetBaseObject());
	}

	return true;
}

bool FillTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (!m_PickedHolePolygon)
		return true;

	CUndo undo("Designer : Fill");

	GetModel()->RecordUndo("Fill", GetBaseObject());

	std::vector<PolygonPtr> perpendicularPolygons;
	GetModel()->QueryAdjacentPerpendicularPolygons(m_PickedHolePolygon, perpendicularPolygons);

	if (perpendicularPolygons.size() == m_PickedHolePolygon->GetEdgeCount() * 2)
	{
		PolygonPtr pOppositePolygon;
		BrushFloat fDistance = 0;
		if (eFindOppositePolygon_Intersection == m_pHoleContainer->QueryOppositePolygon(m_PickedHolePolygon, eFOF_PushDirection, 0.0f, pOppositePolygon, fDistance))
		{
			std::vector<PolygonPtr> perpendicularPolygonsFromOpposite;
			GetModel()->QueryAdjacentPerpendicularPolygons(pOppositePolygon, perpendicularPolygonsFromOpposite);

			if (perpendicularPolygons.size() == perpendicularPolygonsFromOpposite.size())
			{
				bool bShareTunnel = true;
				int iPolygonSize(perpendicularPolygons.size());
				for (int i = 0; i < iPolygonSize; ++i)
				{
					if (!ContainPolygon(perpendicularPolygons[i], perpendicularPolygonsFromOpposite))
					{
						bShareTunnel = false;
						break;
					}
				}

				if (bShareTunnel)
				{
					for (int i = 0; i < iPolygonSize; ++i)
					{
						GetModel()->RemovePolygon(perpendicularPolygons[i]);
						MirrorUtil::RemoveMirroredPolygon(GetModel(), perpendicularPolygons[i]);
					}

					PolygonPtr pClonedOppositePolygon = pOppositePolygon->Clone();

					AddPolygonWithSubMatID(pClonedOppositePolygon, m_Params.bMergeAdjacent);
					MirrorUtil::UpdateMirroredPartWithPlane(GetModel(), pOppositePolygon->GetPlane());
					m_pHoleContainer->AddSubtractedPolygon(pOppositePolygon);
				}
			}
		}
	}

	AddPolygonWithSubMatID(m_PickedHolePolygon->Clone(), m_Params.bMergeAdjacent);
	MirrorUtil::UpdateMirroredPartWithPlane(GetModel(), m_PickedHolePolygon->GetPlane());
	ApplyPostProcess(ePostProcess_ExceptMirror);
	m_pHoleContainer->RemovePolygon(m_PickedHolePolygon);

	return true;
}

bool FillTool::ContainPolygon(PolygonPtr pPolygon, std::vector<PolygonPtr>& polygonList)
{
	for (int i = 0, iPolygonSize(polygonList.size()); i < iPolygonSize; ++i)
	{
		if (pPolygon == polygonList[i])
			return true;
	}
	return false;
}

bool FillTool::FillHoleBasedOnSelectedElements()
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();

	std::vector<BrushEdge3D> validEdgeList;
	for (int i = 0, iElementSize(pSelected->GetCount()); i < iElementSize; ++i)
	{
		if (!(*pSelected)[i].IsEdge())
			continue;
		validEdgeList.push_back((*pSelected)[i].GetEdge());
	}

	if (validEdgeList.size() == 2)
	{
		if (!Comparison::IsEquivalent(validEdgeList[0].m_v[0], validEdgeList[1].m_v[0]) && !Comparison::IsEquivalent(validEdgeList[0].m_v[0], validEdgeList[1].m_v[1]) &&
		    !Comparison::IsEquivalent(validEdgeList[0].m_v[1], validEdgeList[1].m_v[0]) && !Comparison::IsEquivalent(validEdgeList[0].m_v[1], validEdgeList[1].m_v[1]))
		{
			BrushVec3 vEdge0Dir = (validEdgeList[0].m_v[1] - validEdgeList[0].m_v[0]).GetNormalized();
			BrushVec3 vEdge0V0ToEdge1V0 = (validEdgeList[1].m_v[0] - validEdgeList[0].m_v[0]).GetNormalized();
			BrushVec3 vEdge0V0ToEdge1V1 = (validEdgeList[1].m_v[1] - validEdgeList[0].m_v[0]).GetNormalized();
			if (vEdge0Dir.Dot(vEdge0V0ToEdge1V0) < vEdge0Dir.Dot(vEdge0V0ToEdge1V1))
				validEdgeList[1].Invert();

			validEdgeList.push_back(BrushEdge3D(validEdgeList[1].m_v[1], validEdgeList[0].m_v[0]));
			validEdgeList.push_back(BrushEdge3D(validEdgeList[0].m_v[1], validEdgeList[1].m_v[0]));
		}
	}

	if (validEdgeList.empty())
	{
		std::vector<BrushVec3> validVertexList;
		for (int i = 0, iElementSize(pSelected->GetCount()); i < iElementSize; ++i)
		{
			if (!(*pSelected)[i].IsVertex())
				continue;
			validVertexList.push_back((*pSelected)[i].m_Vertices[0]);
		}
		if (validVertexList.size() < 2)
			return false;
		for (int i = 0, iVertexSize(validVertexList.size()); i < iVertexSize; ++i)
			validEdgeList.push_back(BrushEdge3D(validVertexList[i], validVertexList[(i + 1) % iVertexSize]));
	}

	if (validEdgeList.empty())
		false;
	std::vector<BrushEdge3D> linkedEdgeList;
	std::set<int> usedEdgeSet;

	linkedEdgeList.push_back(validEdgeList[0]);
	usedEdgeSet.insert(0);
	const int nValidEdgeSize(validEdgeList.size());
	bool bFinishLoop = false;

	while (linkedEdgeList.size() < nValidEdgeSize)
	{
		bool bFoundNext = false;
		auto iEdgeIndex(usedEdgeSet.begin());
		for (; iEdgeIndex != usedEdgeSet.end(); ++iEdgeIndex)
		{
			int nCurrentIndex = *iEdgeIndex;
			int k = 0;
			for (; k < nValidEdgeSize; ++k)
			{
				if (nCurrentIndex == k || usedEdgeSet.find(k) != usedEdgeSet.end())
					continue;

				if (Comparison::IsEquivalent(validEdgeList[nCurrentIndex].m_v[0], validEdgeList[k].m_v[0]) ||
				    Comparison::IsEquivalent(validEdgeList[nCurrentIndex].m_v[1], validEdgeList[k].m_v[1]))
				{
					usedEdgeSet.insert(k);
					linkedEdgeList.insert(linkedEdgeList.begin(), validEdgeList[k].GetInverted());
					bFoundNext = true;
					break;
				}
				else if (Comparison::IsEquivalent(validEdgeList[nCurrentIndex].m_v[0], validEdgeList[k].m_v[1]) ||
				         Comparison::IsEquivalent(validEdgeList[nCurrentIndex].m_v[1], validEdgeList[k].m_v[0]))
				{
					usedEdgeSet.insert(k);
					linkedEdgeList.push_back(validEdgeList[k]);
					bFoundNext = true;
					break;
				}
			}
			if (bFoundNext)
				break;
		}
		if (!bFoundNext)
			break;
	}

	if (linkedEdgeList.size() < 2)
		return false;

	PolygonPtr pFilledPolygon = new Polygon;
	for (int i = 0, iEdgeSize(linkedEdgeList.size()); i < iEdgeSize; ++i)
		pFilledPolygon->AddEdge(linkedEdgeList[i]);

	if (pFilledPolygon->GetEdgeCount() < 2)
		return false;

	CUndo undo("Designer : Fill a hole");
	GetModel()->RecordUndo("Designer : Fill a hole", GetBaseObject());

	std::vector<Vertex> linkedVertices;
	pFilledPolygon->GetLinkedVertices(linkedVertices);
	BrushPlane plane;
	if (!ComputePlane(linkedVertices, plane))
		plane = BrushPlane(linkedVertices[0].pos, linkedVertices[1].pos, linkedVertices[2].pos);
	pFilledPolygon->SetPlane(plane);

	if (pFilledPolygon->IsOpen())
	{
		pFilledPolygon->AddEdge(BrushEdge3D(linkedVertices[linkedVertices.size() - 1].pos, linkedVertices[0].pos));
		if (pFilledPolygon->IsOpen())
		{
			undo.Cancel();
			return false;
		}
	}

	Matrix34 invWorldTM = GetBaseObject()->GetWorldTM().GetInverted();

	const CCamera& camera = GetDesigner()->GetCamera();
	BrushVec3 vLocalCameraNormal = ToBrushVec3(invWorldTM.TransformVector(camera.GetViewdir()));
	BrushFloat fAngle = vLocalCameraNormal.Dot(plane.Normal());
	if (fAngle > 0)
		pFilledPolygon->Flip();

	pFilledPolygon->Optimize();

	AddPolygonWithSubMatID(pFilledPolygon, m_Params.bMergeAdjacent);
	ApplyPostProcess();

	return true;
}

void FillTool::CompileHoles()
{
	std::vector<PolygonPtr> polygonList[2];

	int nSource = 0;
	int nTarget = 1;

	for (int i = 0, iPolygonSize(GetModel()->GetPolygonCount()); i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon = GetModel()->GetPolygon(i);
		if (pPolygon->CheckFlags(ePolyFlag_Mirrored | ePolyFlag_Hidden) || pPolygon->IsOpen())
			continue;
		PolygonPtr pClonedPolygon = pPolygon->Clone();
		if (pClonedPolygon->HasBridgeEdges())
			pClonedPolygon->RemoveBridgeEdges();
		polygonList[nSource].push_back(pClonedPolygon);
	}

	bool bHasAnyChanges = true;

	while (bHasAnyChanges)
	{
		for (int i = 0, iPolygonSize(polygonList[nSource].size()); i < iPolygonSize; ++i)
		{
			PolygonPtr pPolygon = polygonList[nSource][i];
			PolygonPtr pAdjacentPolygon = QueryAdjacentPolygon(pPolygon, polygonList[nTarget]);
			if (pAdjacentPolygon == NULL)
				polygonList[nTarget].push_back(pPolygon);
			else
				pAdjacentPolygon->Union(pPolygon);
		}
		if (polygonList[nSource].size() == polygonList[nTarget].size())
		{
			bHasAnyChanges = false;
		}
		else
		{
			nSource = 1 - nSource;
			nTarget = 1 - nTarget;
			polygonList[nTarget].clear();
		}
	}

	if (!m_pHoleContainer)
	{
		m_pHoleContainer = new Model;
		m_pHoleContainer->SetFlag(0);
	}
	m_pHoleContainer->Clear();

	for (int i = 0, iPolygonSize(polygonList[nTarget].size()); i < iPolygonSize; ++i)
	{
		std::vector<PolygonPtr> innerPolygons = polygonList[nTarget][i]->GetLoops()->GetHoleClones();
		for (int k = 0, innerPolygonSize(innerPolygons.size()); k < innerPolygonSize; ++k)
		{
			innerPolygons[k]->ReverseEdges();
			for (int a = 0; a < iPolygonSize; ++a)
			{
				if (a == i)
					continue;
				if (polygonList[nTarget][a]->GetPlane().IsEquivalent(innerPolygons[k]->GetPlane()))
					innerPolygons[k]->Subtract(polygonList[nTarget][a]);
			}

			if (innerPolygons[k]->IsValid() && !innerPolygons[k]->IsOpen())
				m_pHoleContainer->AddPolygon(innerPolygons[k]);
		}
	}
}

PolygonPtr FillTool::QueryAdjacentPolygon(PolygonPtr pPolygon, std::vector<PolygonPtr>& polygonList)
{
	for (int i = 0, iPolygonSize(polygonList.size()); i < iPolygonSize; ++i)
	{
		EIntersectionType it = Polygon::HasIntersection(polygonList[i], pPolygon);
		if (it == eIT_JustTouch)
			return polygonList[i];
	}
	return NULL;
}

void FillTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	__super::OnEditorNotifyEvent(event);
	switch (event)
	{
	case eNotify_OnEndUndoRedo:
		CompileHoles();
		break;
	}
}

void FillTool::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("Merge", " "))
	{
		ar(m_Params.bMergeAdjacent, "Merge", "^^Merge Adjacent");
		ar.closeBlock();
	}
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Fill, eToolGroup_Edit, "Fill", FillTool,
                                                           fill, "runs fill tool", "designer.fill")

