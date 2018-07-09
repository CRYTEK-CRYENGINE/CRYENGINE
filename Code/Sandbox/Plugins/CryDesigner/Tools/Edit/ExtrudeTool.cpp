// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExtrudeTool.h"
#include "ViewManager.h"
#include "Core/Model.h"
#include "Core/Helper.h"
#include "Util/HeightManipulator.h"
#include "Util/ExtrusionSnappingHelper.h"
#include "DesignerEditor.h"
#include "Util/OffsetManipulator.h"
#include "Objects/DisplayContext.h"
#include "Util/MFCUtil.h"

namespace Designer
{
void ExtrudeTool::Enter()
{
	__super::Enter();
	m_ec.pushPull = ePP_None;
}

void ExtrudeTool::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("ScalePolygon", " "))
	{
		ar(m_Params.bScalePolygonPhase, "ScalePolygon", "^^Scale Polygon");
		ar.closeBlock();
	}
	if (ar.openBlock("LeaveLoopEdges", " "))
	{
		ar(m_Params.bLeaveLoopEdges, "LeaveLoopEdges", "^^Leave loop edges");
		ar.closeBlock();
	}
	if (ar.openBlock("Alignment", " "))
	{
		ar(m_Params.bAlignment, "Alignment", "^^Alignment to another polygon");
		ar.closeBlock();
	}
	DISPLAY_MESSAGE("ALT + LMB : Repeat the previous");

	m_Phase = eExtrudePhase_Select;
	s_OffsetManipulator.Invalidate();
}

bool ExtrudeTool::StartPushPull(CViewport* view, UINT nFlags, CPoint point)
{
	m_ec.pArgumentModel = NULL;
	m_ec.pushPull = m_ec.initPushPull = ePP_None;

	if (m_pSelectedPolygon)
	{
		GetIEditor()->GetIUndoManager()->Begin();
		GetModel()->RecordUndo("Designer : Extrusion", GetBaseObject());

		BrushVec3 vPivot;
		BrushPlane plane;
		if (GetModel()->QueryPosition(GetDesigner()->GetRay(), vPivot, &plane))
			s_HeightManipulator.Init(plane, vPivot);
		else
			s_HeightManipulator.Init(plane, m_pSelectedPolygon->GetCenterPosition());

		m_ec.pCompiler = GetCompiler();
		m_ec.pObject = GetBaseObject();
		m_ec.pModel = GetModel();
		m_ec.pPolygon = m_pSelectedPolygon;

		return PrepareExtrusion(m_ec);
	}
	return false;
}

bool ExtrudeTool::PrepareExtrusion(ExtrusionContext& ec)
{
	DESIGNER_ASSERT(ec.pPolygon);
	if (!ec.pPolygon)
		return false;

	MODEL_SHELF_RECONSTRUCTOR(ec.pModel);

	ec.backupPolygons.clear();
	ec.pModel->QueryPerpendicularPolygons(ec.pPolygon, ec.backupPolygons);
	MirrorUtil::RemoveMirroredPolygons(ec.backupPolygons);
	if (ec.pModel->CheckFlag(eModelFlag_Mirror))
	{
		ec.mirroredBackupPolygons.clear();
		ec.pModel->QueryPerpendicularPolygons(ec.pPolygon->Clone()->Mirror(ec.pModel->GetMirrorPlane()), ec.mirroredBackupPolygons);
		MirrorUtil::RemoveNonMirroredPolygons(ec.mirroredBackupPolygons);
	}

	MakeArgumentBrush(ec);
	if (ec.pArgumentModel)
	{
		s_SnappingHelper.SearchForOppositePolygons(ec.pArgumentModel->GetCapPolygon());
		ec.pModel->SetShelf(eShelf_Base);
		ec.pModel->RemovePolygon(ec.pPolygon);
		MirrorUtil::RemoveMirroredPolygon(ec.pModel, ec.pPolygon);
		ec.bFirstUpdate = true;
		ec.pModel->InvalidateSmoothingGroups();
		return true;
	}

	return false;
}

bool ExtrudeTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	s_SnappingHelper.Init(GetModel());

	if (Deprecated::CheckVirtualKey(VK_MENU))
	{
		RepeatPrevAction(view, nFlags, point);
		return true;
	}

	if (m_Phase == eExtrudePhase_Select && m_Params.bScalePolygonPhase)
	{
		if (m_pSelectedPolygon)
		{
			s_OffsetManipulator.Init(m_pSelectedPolygon->Clone(), GetModel(), true);
			m_Phase = eExtrudePhase_Resize;
		}
	}
	else if (m_Phase == eExtrudePhase_Select || (m_Phase == eExtrudePhase_Resize && m_Params.bScalePolygonPhase))
	{
		m_ec.pArgumentModel = NULL;
		m_ec.bTouchedMirrorPlane = false;
		m_Phase = eExtrudePhase_Extrude;
	}

	return true;
}

void ExtrudeTool::RaiseHeight(const CPoint& point, CViewport* view, int nFlags)
{
	BrushFloat fHeight = s_HeightManipulator.Update(GetWorldTM(), view, GetDesigner()->GetRay());

	m_ec.pArgumentModel->SetHeight(fHeight);
	m_ec.pArgumentModel->Update(ArgumentModel::eBAU_None);
	m_ec.bTouchedMirrorPlane = false;

	if (!GetModel()->CheckFlag(eModelFlag_Mirror))
		return;

	PolygonPtr pCapPolygon = m_ec.pArgumentModel->GetCapPolygon();
	for (int i = 0, iVertexCount(pCapPolygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		const BrushVec3& v = pCapPolygon->GetPos(i);
		BrushFloat distFromVertexToMirrorPlane = GetModel()->GetMirrorPlane().Distance(v);
		if (distFromVertexToMirrorPlane <= -kDesignerEpsilon)
			continue;
		BrushFloat distFromVertexMirrorPlaneInCapNormalDir = 0;
		GetModel()->GetMirrorPlane().HitTest(BrushRay(v, pCapPolygon->GetPlane().Normal()), &distFromVertexMirrorPlaneInCapNormalDir);
		m_ec.pArgumentModel->SetHeight(fHeight + distFromVertexMirrorPlaneInCapNormalDir);
		m_ec.pArgumentModel->Update(ArgumentModel::eBAU_None);
		m_ec.bTouchedMirrorPlane = true;
		break;
	}
}

void ExtrudeTool::RaiseLowerPolygon(CViewport* view, UINT nFlags, CPoint point)
{
	if (!(nFlags & MK_LBUTTON))
		return;

	BrushFloat fPrevHeight = m_ec.pArgumentModel->GetHeight();

	if (!m_Params.bAlignment || AlignHeight(m_ec.pArgumentModel->GetCapPolygon(), view, point) == NULL)
		RaiseHeight(point, view, nFlags);

	BrushFloat fHeight = m_ec.pArgumentModel->GetHeight();
	BrushFloat fHeightDifference(fHeight - fPrevHeight);
	if (fHeightDifference > 0)
		m_ec.pushPull = ePP_Pull;
	else if (fHeightDifference < 0)
		m_ec.pushPull = ePP_Push;

	if (m_ec.initPushPull == ePP_None)
		m_ec.initPushPull = m_ec.pushPull;

	m_ec.bLeaveLoopEdges = m_Params.bLeaveLoopEdges;

	CheckBoundary(m_ec);
	UpdateModel(m_ec);
}

void ExtrudeTool::ResizePolygon(CViewport* view, UINT nFlags, CPoint point)
{
	s_OffsetManipulator.UpdateOffset(view, point);
	m_ec.fScale = s_OffsetManipulator.GetScale();
}

void ExtrudeTool::SelectPolygon(CViewport* view, UINT nFlags, CPoint point)
{
	if (GetBaseObject() == NULL)
		return;

	DesignerSession* pSession = DesignerSession::GetInstance();

	int nPolygonIndex(0);
	if (GetModel()->QueryPolygon(GetDesigner()->GetRay(), nPolygonIndex))
	{
		PolygonPtr pCandidatePolygon = GetModel()->GetPolygon(nPolygonIndex);
		if (pCandidatePolygon != m_pSelectedPolygon && !pCandidatePolygon->CheckFlags(ePolyFlag_Mirrored) && !pCandidatePolygon->IsOpen())
		{
			m_pSelectedPolygon = pCandidatePolygon;
			m_ec.fScale = 0;
			pSession->UpdateSelectionMesh(m_pSelectedPolygon, GetCompiler(), GetBaseObject());
		}
	}
	else
	{
		m_pSelectedPolygon = NULL;
		pSession->UpdateSelectionMesh(NULL, GetCompiler(), GetBaseObject());
	}
}

bool ExtrudeTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	DesignerSession* pSession = DesignerSession::GetInstance();

	if (m_Phase == eExtrudePhase_Select)
	{
		SelectPolygon(view, nFlags, point);
	}
	else if (m_Phase == eExtrudePhase_Resize)
	{
		ResizePolygon(view, nFlags, point);
	}
	else if (m_Phase == eExtrudePhase_Extrude)
	{
		if (!(nFlags & MK_LBUTTON))
			return true;

		if (m_ec.pArgumentModel)
		{
			RaiseLowerPolygon(view, nFlags, point);
		}
		else
		{
			if (StartPushPull(view, nFlags, point))
			{
				if (s_OffsetManipulator.IsValid())
					pSession->UpdateSelectionMesh(s_OffsetManipulator.GetScaledPolygon(), GetCompiler(), GetBaseObject());
				else
					pSession->UpdateSelectionMesh(m_pSelectedPolygon, GetCompiler(), GetBaseObject());
			}
		}
	}

	return true;
}

void ExtrudeTool::Display(DisplayContext& dc)
{
	__super::Display(dc);

	s_OffsetManipulator.Display(dc);
	s_HeightManipulator.Display(dc);

	if (m_ec.pArgumentModel && gDesignerSettings.bDisplayDimensionHelper)
	{
		Matrix34 poppedTM = dc.GetMatrix();
		dc.PopMatrix();
		GetBaseObject()->DrawDimensionsImpl(dc, m_ec.pArgumentModel->GetBoundBox());
		dc.PushMatrix(poppedTM);
	}
}

bool ExtrudeTool::CheckBoundary(ExtrusionContext& ec)
{
	if (ec.pushPull == ePP_None || ec.pArgumentModel == NULL)
		return false;

	ec.bIsLocatedAtOpposite = false;

	if (s_SnappingHelper.IsOverOppositePolygon(ec.pArgumentModel->GetCapPolygon(), ec.pushPull))
	{
		ec.pArgumentModel->SetHeight(s_SnappingHelper.GetNearestDistanceToOpposite(ec.pushPull));
		ec.pArgumentModel->Update(ArgumentModel::eBAU_None);
		ec.bIsLocatedAtOpposite = true;
		return true;
	}

	return false;
}

void ExtrudeTool::MakeArgumentBrush(ExtrusionContext& ec)
{
	if (ec.pPolygon == NULL)
		return;

	ec.pArgumentModel = new ArgumentModel(ec.pPolygon, ec.fScale, ec.pObject, &ec.backupPolygons, ec.pModel->GetDB());
	ec.pArgumentModel->SetHeight(0);
	ec.pArgumentModel->Update(ArgumentModel::eBAU_None);
}

void ExtrudeTool::FinishPushPull(ExtrusionContext& ec)
{
	if (!ec.pArgumentModel)
		return;

	if (!ec.bIsLocatedAtOpposite)
	{
		ec.pModel->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
	}
	else
	{
		s_SnappingHelper.ApplyOppositePolygons(ec.pArgumentModel->GetCapPolygon(), ec.pushPull);
		ec.pModel->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
		DesignerSession::GetInstance()->UpdateSelectionMesh(NULL, ec.pCompiler, ec.pObject, true);
		MirrorUtil::UpdateMirroredPartWithPlane(ec.pModel, ec.pArgumentModel->GetCapPolygon()->GetPlane());
		MirrorUtil::UpdateMirroredPartWithPlane(ec.pModel, ec.pArgumentModel->GetCapPolygon()->GetPlane().GetInverted());
		ec.bIsLocatedAtOpposite = false;
	}

	Designer::ApplyPostProcess(ec, ePostProcess_ExceptSyncPrefab);
	ec.pArgumentModel = NULL;
}

bool ExtrudeTool::AlignHeight(PolygonPtr pCapPolygon, CViewport* view, const CPoint& point)
{
	PolygonPtr pAlignedPolygon = s_SnappingHelper.FindAlignedPolygon(pCapPolygon, GetWorldTM(), view, point);
	if (!pAlignedPolygon)
		return false;

	BrushFloat updatedHeight = m_ec.pArgumentModel->GetBasePlane().Distance() - pAlignedPolygon->GetPlane().Distance();
	if (updatedHeight != m_ec.pArgumentModel->GetHeight())
	{
		if (!CheckBoundary(m_ec))
		{
			m_ec.pArgumentModel->SetHeight(updatedHeight);
			m_ec.pArgumentModel->Update(ArgumentModel::eBAU_None);
		}
		m_ec.pArgumentModel->GetCapPolygon()->UpdatePlane(m_ec.pArgumentModel->GetCurrentCapPlane());
	}

	return true;
}

bool ExtrudeTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Phase != eExtrudePhase_Extrude)
		return true;

	if (m_ec.pArgumentModel)
	{
		if (m_ec.pushPull != ePP_None)
		{
			m_PrevAction.m_Type = m_ec.pushPull;
			m_PrevAction.m_Distance = m_ec.pArgumentModel ? m_ec.pArgumentModel->GetHeight() : 0;
			m_PrevAction.m_Scale = m_ec.fScale;
		}
		CheckBoundary(m_ec);
		UpdateSurfaceInfo();
		FinishPushPull(m_ec);
		GetIEditor()->GetIUndoManager()->Accept("Designer : Extrude");
		ApplyPostProcess(ePostProcess_SyncPrefab);
	}
	else
	{
		GetIEditor()->GetIUndoManager()->Cancel();
	}
	s_OffsetManipulator.Invalidate();
	m_Phase = eExtrudePhase_Select;

	return true;
}

void ExtrudeTool::Extrude(MainContext& mc, PolygonPtr pPolygon, float fHeight, float fScale)
{
	ExtrusionContext ec;
	ec.pObject = mc.pObject;
	ec.pCompiler = mc.pCompiler;
	ec.pModel = mc.pModel;
	ec.pPolygon = pPolygon;
	ec.pushPull = ec.initPushPull = fHeight > 0 ? ePP_Pull : ePP_Push;
	ec.fScale = fScale;
	ec.bUpdateBrush = false;
	s_SnappingHelper.Init(ec.pModel);
	PrepareExtrusion(ec);
	ec.pArgumentModel->SetHeight(fHeight);
	ec.pArgumentModel->Update(ArgumentModel::eBAU_None);
	CheckBoundary(ec);
	UpdateModel(ec);
	FinishPushPull(ec);
}

void ExtrudeTool::RepeatPrevAction(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_ec.pArgumentModel)
	{
		m_ec.pArgumentModel = NULL;
		return;
	}

	if (std::abs(m_PrevAction.m_Distance) < kDesignerEpsilon || m_PrevAction.m_Type == ePP_None)
		return;

	m_ec.fScale = m_PrevAction.m_Scale;
	if (!StartPushPull(view, nFlags, point))
		return;

	m_ec.pushPull = m_PrevAction.m_Type;
	m_ec.pArgumentModel->SetHeight(m_PrevAction.m_Distance);
	m_ec.pArgumentModel->Update(ArgumentModel::eBAU_None);
	m_ec.bLeaveLoopEdges = m_Params.bLeaveLoopEdges;

	CheckBoundary(m_ec);
	UpdateModel(m_ec);
	if (m_ec.pArgumentModel)
	{
		FinishPushPull(m_ec);
		GetIEditor()->GetIUndoManager()->Accept("Designer : Extrude");
	}
	else
	{
		GetIEditor()->GetIUndoManager()->Cancel();
	}
	ApplyPostProcess(ePostProcess_SyncPrefab);
	m_Phase = eExtrudePhase_Select;
	s_OffsetManipulator.Invalidate();
	m_ec.pArgumentModel = NULL;
}

void ExtrudeTool::UpdateModel(ExtrusionContext& ec)
{
	MODEL_SHELF_RECONSTRUCTOR(ec.pModel);

	if (!ec.pArgumentModel)
		return;

	if (ec.bFirstUpdate)
	{
		ec.pModel->SetShelf(eShelf_Base);
		for (int i = 0, iPolygonCount(ec.backupPolygons.size()); i < iPolygonCount; ++i)
			ec.pModel->RemovePolygon(ec.backupPolygons[i]);
		for (int i = 0, iPolygonCount(ec.mirroredBackupPolygons.size()); i < iPolygonCount; ++i)
			ec.pModel->RemovePolygon(ec.mirroredBackupPolygons[i]);
	}

	ec.pModel->SetShelf(eShelf_Construction);
	if (ec.bResetShelf)
	{
		ec.pModel->Clear();
		for (int i = 0, iPolygonCount(ec.backupPolygons.size()); i < iPolygonCount; ++i)
			ec.pModel->AddPolygon(ec.backupPolygons[i]->Clone());
	}

	const BrushPlane& mirrorPlane = ec.pModel->GetMirrorPlane();
	BrushPlane invertedMirrorPlane = mirrorPlane.GetInverted();

	std::vector<PolygonPtr> sidePolygons;
	if (ec.pArgumentModel->GetSidePolygonList(sidePolygons))
	{
		for (int i = 0, iSidePolygonSize(sidePolygons.size()); i < iSidePolygonSize; ++i)
		{
			if (mirrorPlane.IsEquivalent(sidePolygons[i]->GetPlane()) || invertedMirrorPlane.IsEquivalent(sidePolygons[i]->GetPlane()))
				continue;

			bool bOperated = false;
			PolygonPtr pSidePolygon = sidePolygons[i];
			PolygonPtr pSideFlipedPolygon = sidePolygons[i]->Clone()->Flip();

			std::set<int> removedBackUpPolygons;

			for (int k = 0, iBackupPolygonSize(ec.backupPolygons.size()); k < iBackupPolygonSize; ++k)
			{
				bool bIncluded = pSidePolygon->IncludeAllEdges(ec.backupPolygons[k]);
				bool bFlippedIncluded = pSideFlipedPolygon->IncludeAllEdges(ec.backupPolygons[k]);
				if (!bIncluded && !bFlippedIncluded)
					continue;
				ec.pModel->RemovePolygon(ec.pModel->QueryEquivalentPolygon(ec.backupPolygons[k]));
				if (bIncluded)
				{
					pSidePolygon->Subtract(ec.backupPolygons[k]);
					pSideFlipedPolygon->Subtract(ec.backupPolygons[k]->Clone()->Flip());
				}
				if (bFlippedIncluded)
				{
					pSideFlipedPolygon->Subtract(ec.backupPolygons[k]);
					pSidePolygon->Subtract(ec.backupPolygons[k]->Clone()->Flip());
				}
				removedBackUpPolygons.insert(k);
			}

			if (!pSidePolygon->IsValid() || !pSideFlipedPolygon->IsValid())
				continue;

			for (int k = 0, iBackupPolygonSize(ec.backupPolygons.size()); k < iBackupPolygonSize; ++k)
			{
				if (removedBackUpPolygons.find(k) != removedBackUpPolygons.end())
					continue;

				EIntersectionType it = Polygon::HasIntersection(ec.backupPolygons[k], pSidePolygon);
				EIntersectionType itFliped = Polygon::HasIntersection(ec.backupPolygons[k], pSideFlipedPolygon);

				if (it == eIT_None && itFliped == eIT_None)
					continue;

				PolygonPtr pThisSidePolygon = pSidePolygon;
				if (itFliped == eIT_Intersection || itFliped == eIT_JustTouch && ec.pArgumentModel->GetHeight() < 0)
				{
					it = itFliped;
					pThisSidePolygon = pSideFlipedPolygon;
				}

				if (it == eIT_Intersection || it == eIT_JustTouch && ec.backupPolygons[k]->HasOverlappedEdges(pThisSidePolygon))
				{
					if (ec.bLeaveLoopEdges)
					{
						if (ec.pArgumentModel->GetHeight() > 0)
							ec.pModel->AddPolygon(pThisSidePolygon->Clone());
						else
							ec.pModel->AddXORPolygon(pThisSidePolygon, false);
					}
					else
					{
						ec.pModel->AddXORPolygon(pThisSidePolygon);
					}
					bOperated = true;
					break;
				}
			}
			if (!bOperated)
			{
				if (ec.pArgumentModel->GetHeight() < 0)
				{
					PolygonPtr pFlipPolygon = sidePolygons[i]->Clone()->Flip();
					ec.pModel->AddPolygon(pFlipPolygon);
				}
				else
				{
					ec.pModel->AddPolygon(sidePolygons[i]->Clone());
				}
			}
		}
	}

	PolygonPtr pCapPolygon(ec.pArgumentModel->GetCapPolygon());
	if (pCapPolygon)
	{
		if (!ec.bTouchedMirrorPlane || !Comparison::IsEquivalent(ec.pModel->GetMirrorPlane().Normal(), pCapPolygon->GetPlane().Normal()))
			ec.pModel->AddPolygon(pCapPolygon);

		if (GetDesigner() && ec.bUpdateSelectionMesh)
		{
			Matrix34 worldTM = ec.pObject->GetWorldTM();
			worldTM.SetTranslation(worldTM.GetTranslation() + worldTM.TransformVector(pCapPolygon->GetPlane().Normal() * (ec.pArgumentModel->GetHeight() + 0.01f)));
			DesignerSession::GetInstance()->GetSelectionMesh()->SetWorldTM(worldTM);
		}
	}

	if (ec.bUpdateBrush)
	{
		Designer::ApplyPostProcess(ec, ePostProcess_Mirror);
		if (ec.bFirstUpdate)
		{
			Designer::ApplyPostProcess(ec, ePostProcess_Mesh);
			ec.bFirstUpdate = false;
		}
		else
		{
			ec.pCompiler->Compile(ec.pObject, ec.pModel, eShelf_Construction);
		}
	}
}

void ExtrudeTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	__super::OnEditorNotifyEvent(event);
	switch (event)
	{
	case eNotify_OnEndUndoRedo:
		DesignerSession::GetInstance()->UpdateSelectionMesh(NULL, GetCompiler(), GetBaseObject(), true);
		break;
	}
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Extrude, eToolGroup_Edit, "Extrude", ExtrudeTool,
                                                           extrude, "runs extrude tool", "designer.extrude")

