// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MagnetTool.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "Viewport.h"
#include "Util/Display.h"
#include "Objects/DisplayContext.h"

#include "QtUtil.h"

namespace Designer
{
void MagnetTool::AddVertexToList(const BrushVec3& vertex, ColorB color, std::vector<SourceVertex>& vertices)
{
	bool bHasSame = false;
	for (int i = 0, iVertexCount(vertices.size()); i < iVertexCount; ++i)
	{
		if (Comparison::IsEquivalent(vertices[i].position, vertex))
		{
			bHasSame = true;
			break;
		}
	}
	if (!bHasSame)
		vertices.push_back(SourceVertex(vertex, color));
}

void MagnetTool::Enter()
{
	__super::Enter();

	m_pSelectedPolygon = NULL;
	DesignerSession* pSession = DesignerSession::GetInstance();
	pSession->GetSelectedElements()->Clear();
	pSession->UpdateSelectionMeshFromSelectedElements();
	m_Phase = eMTP_ChoosePolygon;
	m_PickedPos = BrushVec3(0, 0, 0);
	m_vTargetUpDir = BrushVec3(0, 0, 1);
}

void MagnetTool::Leave()
{
	if (m_Phase == eMTP_ChooseMoveToTargetPoint)
	{
		if (m_bApplyUndoForMagnet)
			GetIEditor()->GetIUndoManager()->Accept("Designer : Magnet Tool");
		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
		ApplyPostProcess();
	}

	m_Phase = eMTP_ChooseFirstPoint;
	m_SourceVertices.clear();
	m_nSelectedSourceVertex = -1;
	m_nSelectedUpVertex = -1;
	m_pInitPolygon = NULL;
	m_pSelectedPolygon = NULL;
	m_pTargetPolygon = NULL;

	__super::Leave();
}

void MagnetTool::PrepareChooseFirstPointStep()
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();

	if (pSelected->IsEmpty() || (*pSelected)[0].m_pPolygon == NULL)
		return;

	m_pSelectedPolygon = (*pSelected)[0].m_pPolygon;
	m_pInitPolygon = m_pSelectedPolygon->Clone();
	(*pSelected)[0].m_bIsolated = true;

	m_SourceVertices.clear();

	const BrushPlane& plane = m_pSelectedPolygon->GetPlane();
	const AABB& aabb(m_pSelectedPolygon->GetBoundBox());

	AABB rectangleAABB;
	rectangleAABB.Reset();

	BrushVec2 v0 = plane.W2P(ToBrushVec3(aabb.min));
	BrushVec2 v1 = plane.W2P(ToBrushVec3(aabb.max));

	rectangleAABB.Add(Vec3(v0.x, 0, v0.y));
	rectangleAABB.Add(Vec3(v1.x, 0, v1.y));

	BrushVec3 step = ToBrushVec3((rectangleAABB.max - rectangleAABB.min) * 0.5f);

	for (int i = 0; i <= 2; ++i)
	{
		for (int j = 0; j <= 2; ++j)
		{
			for (int k = 0; k <= 2; ++k)
			{
				BrushVec3 v = ToBrushVec3(rectangleAABB.min) + BrushVec3(i * step.x, j * step.y, k * step.z);
				v = plane.P2W(BrushVec2(v.x, v.z));
				AddVertexToList(
				  v,
				  ColorB(0xFF40FF40),
				  m_SourceVertices);
			}
		}
	}

	for (int i = 0, iVertexCount(m_pSelectedPolygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		AddVertexToList(
		  m_pSelectedPolygon->GetPos(i),
		  kElementBoxColor,
		  m_SourceVertices);
	}

	for (int i = 0, iEdgeCount(m_pSelectedPolygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		BrushEdge3D e = m_pSelectedPolygon->GetEdge(i);
		AddVertexToList(
		  e.GetCenter(),
		  kElementBoxColor,
		  m_SourceVertices);
	}

	AddVertexToList(
	  m_pSelectedPolygon->GetRepresentativePosition(),
	  kElementBoxColor,
	  m_SourceVertices);

	m_nSelectedSourceVertex = -1;
	m_nSelectedUpVertex = -1;
	m_Phase = eMTP_ChooseFirstPoint;
}

bool MagnetTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Phase == eMTP_ChoosePolygon)
	{
		__super::OnLButtonDown(view, nFlags, point);
		ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
		if (pSelected->GetPolygonCount() == 1)
		{
			PrepareChooseFirstPointStep();
			GetDesigner()->ReleaseObjectGizmo();
		}
	}
	else if (m_Phase == eMTP_ChooseFirstPoint && m_nSelectedSourceVertex != -1)
	{
		m_nSelectedUpVertex = -1;
		m_Phase = eMTP_ChooseUpPoint;
		m_PickedPos = m_SourceVertices[m_nSelectedSourceVertex].position;
	}
	else if (m_Phase == eMTP_ChooseUpPoint)
	{
		if (m_bApplyUndoForMagnet)
		{
			GetIEditor()->GetIUndoManager()->Begin();
			GetModel()->RecordUndo("Designer : Magnet Tool", GetBaseObject());
		}
		m_Phase = eMTP_ChooseMoveToTargetPoint;
		MODEL_SHELF_RECONSTRUCTOR(GetModel());
		GetModel()->SetShelf(eShelf_Base);
		GetModel()->RemovePolygon(m_pSelectedPolygon);
		GetModel()->SetShelf(eShelf_Construction);
		GetModel()->AddPolygon(m_pSelectedPolygon);
		ApplyPostProcess(ePostProcess_Mesh);
		m_bPickedTargetPos = false;
		m_bSwitchedSides = false;
		m_pTargetPolygon = NULL;
	}
	else if (m_Phase == eMTP_ChooseMoveToTargetPoint)
	{
		GetDesigner()->SwitchToPrevTool();
	}

	return true;
}

void MagnetTool::SwitchSides()
{
	DESIGNER_ASSERT(m_pInitPolygon);
	if (!m_pInitPolygon || !m_pTargetPolygon)
		return;
	BrushVec3 vPivot = m_SourceVertices[m_nSelectedSourceVertex].position;
	BrushVec3 vUpPos = m_SourceVertices[m_nSelectedUpVertex].position;
	BrushPlane mirrorPlane(vPivot, vUpPos, m_pInitPolygon->GetPlane().Normal() + vPivot);
	m_pInitPolygon->Mirror(mirrorPlane);
	InitializeSelectedPolygonBeforeTransform();

	if (m_bPickedTargetPos)
	{
		AlignSelectedPolygon();
	}
	else
	{
		DesignerSession::GetInstance()->UpdateSelectionMeshFromSelectedElements();
		CompileShelf(eShelf_Construction);
	}

	m_bSwitchedSides = !m_bSwitchedSides;
}

void MagnetTool::AlignSelectedPolygon()
{
	std::vector<int> edgeIndices;
	bool bEdgeExist = false;
	BrushEdge3D dirEdge;
	if (m_pTargetPolygon->QueryEdgesWithPos(m_TargetPos, edgeIndices))
	{
		int iEdgeCount(edgeIndices.size());
		for (int i = 0; i < iEdgeCount; ++i)
		{
			BrushEdge3D e = m_pTargetPolygon->GetEdge(edgeIndices[i]);
			if (Comparison::IsEquivalent(e.m_v[0], m_TargetPos))
			{
				dirEdge = e;
				bEdgeExist = true;
				break;
			}
		}
	}

	if (bEdgeExist)
	{
		InitializeSelectedPolygonBeforeTransform();

		DESIGNER_ASSERT(m_pInitPolygon);
		if (!m_pInitPolygon)
			return;

		BrushVec3 vSelectionNormal = m_pInitPolygon->GetPlane().Normal();
		BrushVec3 vEdgeDir = (dirEdge.m_v[1] - dirEdge.m_v[0]).GetNormalized();
		BrushMatrix34 tmRot1 = ToBrushMatrix33(Matrix33::CreateRotationV0V1(vSelectionNormal, -vEdgeDir));
		BrushMatrix34 tmRot2 = BrushMatrix34::CreateIdentity();
		if (m_nSelectedUpVertex != -1)
		{
			BrushVec3 vUpDir =
			  (m_SourceVertices[m_nSelectedUpVertex].position -
			   m_SourceVertices[m_nSelectedSourceVertex].position).GetNormalized();
			vUpDir = tmRot1.TransformVector(vUpDir);
			BrushVec3 vIntermediateDir = vEdgeDir.Cross(vUpDir);
			tmRot2 = ToBrushMatrix33(
			  Matrix33::CreateRotationV0V1(vIntermediateDir, m_vTargetUpDir) *
			  Matrix33::CreateRotationV0V1(vUpDir, vIntermediateDir));
		}
		BrushMatrix34 tmRot = tmRot2 * tmRot1;
		m_pSelectedPolygon->Translate(-m_TargetPos);
		m_pSelectedPolygon->Transform(tmRot);
		m_pSelectedPolygon->Translate(m_TargetPos);

		DesignerSession::GetInstance()->UpdateSelectionMeshFromSelectedElements();
		CompileShelf(eShelf_Construction);
	}
}

bool MagnetTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		if (m_Phase == eMTP_ChooseMoveToTargetPoint || m_Phase == eMTP_ChooseUpPoint)
		{
			DesignerSession* pSession = DesignerSession::GetInstance();
			ElementSet* pSelected = pSession->GetSelectedElements();
			if (m_bSwitchedSides)
				SwitchSides();

			if (m_pInitPolygon)
				*((*pSelected)[0].m_pPolygon) = *m_pInitPolygon;

			GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
			ApplyPostProcess(ePostProcess_Mesh);
			if (m_bApplyUndoForMagnet)
				GetIEditor()->GetIUndoManager()->Cancel();
			PrepareChooseFirstPointStep();
			pSession->UpdateSelectionMeshFromSelectedElements();
		}
		else if (m_Phase == eMTP_ChooseFirstPoint)
		{
			return GetDesigner()->EndCurrentDesignerTool();
		}
	}
	else if (nChar == VK_CONTROL && m_Phase == eMTP_ChooseMoveToTargetPoint)
	{
		SwitchSides();
	}
	return true;
}

bool MagnetTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Phase == eMTP_ChoosePolygon)
		return true;

	Vec3 raySrc, rayDir;
	view->ViewToWorldRay(point, raySrc, rayDir);

	if (m_Phase == eMTP_ChooseFirstPoint || m_Phase == eMTP_ChooseUpPoint)
	{
		BrushFloat fLeastDist = (BrushFloat)3e10;

		int nSelectedVertex = -1;
		for (int i = 0, iVertexCount(m_SourceVertices.size()); i < iVertexCount; ++i)
		{
			const BrushVec3& v(m_SourceVertices[i].position);
			BrushFloat dist;
			BrushVec3 vWorldPos = GetWorldTM().TransformPoint(v);
			BrushVec3 vBoxSize = GetElementBoxSize(
			  view,
			  view->GetType() != ET_ViewportCamera,
			  vWorldPos);

			if (!GetIntersectionOfRayAndAABB(
			      ToBrushVec3(raySrc),
			      ToBrushVec3(rayDir),
			      AABB(vWorldPos - vBoxSize, vWorldPos + vBoxSize),
			      &dist))
			{
				continue;
			}

			if (dist > 0 && dist < fLeastDist)
			{
				fLeastDist = dist;
				nSelectedVertex = i;
			}
		}

		if (nSelectedVertex != -1)
		{
			if (m_Phase == eMTP_ChooseFirstPoint)
				m_nSelectedSourceVertex = nSelectedVertex;
			else
			{
				if (nSelectedVertex != m_nSelectedSourceVertex)
					m_nSelectedUpVertex = nSelectedVertex;
				else
					m_nSelectedUpVertex = -1;
				m_PickedPos = m_SourceVertices[nSelectedVertex].position;
			}
		}
		else
		{
			GetModel()->QueryPosition(GetDesigner()->GetRay(), m_PickedPos);
		}
	}
	else
	{
		BrushVec3 vPickedPos;
		DesignerSession* pSession = DesignerSession::GetInstance();
		ElementSet* pSelected = pSession->GetSelectedElements();
		PolygonPtr polygon = pSelected->QueryNearestVertex(
		  GetBaseObject(),
		  GetModel(),
		  view,
		  point,
		  GetDesigner()->GetRay(),
		  true,
		  vPickedPos,
		  &m_vTargetUpDir);

		if (polygon)
		{
			m_pTargetPolygon = polygon;
			m_TargetPos = vPickedPos;
			InitializeSelectedPolygonBeforeTransform();
			pSession->UpdateSelectionMeshFromSelectedElements();
			CompileShelf(eShelf_Construction);
			m_bPickedTargetPos = true;
		}

		if (m_bPickedTargetPos)
			AlignSelectedPolygon();
	}

	return true;
}

void MagnetTool::InitializeSelectedPolygonBeforeTransform()
{
	if (!m_pSelectedPolygon)
		return;

	const BrushVec3& vInitPivot = m_SourceVertices[m_nSelectedSourceVertex].position;
	BrushMatrix34 tmDelta = BrushMatrix34::CreateIdentity();
	tmDelta.SetTranslation(m_TargetPos - vInitPivot);

	DESIGNER_ASSERT(m_pInitPolygon);
	if (!m_pInitPolygon)
		return;

	*m_pSelectedPolygon = *m_pInitPolygon;
	m_pSelectedPolygon->Transform(tmDelta);
}

void MagnetTool::Display(DisplayContext& dc)
{
	if (m_Phase == eMTP_ChoosePolygon)
	{
		__super::Display(dc);
		return;
	}

	dc.PopMatrix();
	if (m_Phase == eMTP_ChooseFirstPoint || m_Phase == eMTP_ChooseUpPoint)
	{
		for (int i = 0, iVertexCount(m_SourceVertices.size()); i < iVertexCount; ++i)
		{
			dc.SetColor(m_SourceVertices[i].color);
			const BrushVec3& v = m_SourceVertices[i].position;
			if (m_nSelectedSourceVertex == i || m_nSelectedUpVertex == i)
				continue;
			BrushVec3 vWorldVertexPos = GetWorldTM().TransformPoint(v);
			BrushVec3 vVertexBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, vWorldVertexPos);
			dc.DrawSolidBox(ToVec3(vWorldVertexPos - vVertexBoxSize), ToVec3(vWorldVertexPos + vVertexBoxSize));
		}

		if (m_nSelectedSourceVertex != -1)
		{
			BrushVec3 vWorldVertexPos = GetWorldTM().TransformPoint(m_SourceVertices[m_nSelectedSourceVertex].position);
			BrushVec3 vBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, vWorldVertexPos);
			dc.SetColor(kSelectedColor);
			dc.DrawSolidBox(ToVec3(vWorldVertexPos - vBoxSize), ToVec3(vWorldVertexPos + vBoxSize));

			if (m_Phase == eMTP_ChooseUpPoint)
			{
				dc.SetColor(ColorB(0, 150, 214, 255));
				dc.SetLineWidth(4);
				dc.DrawLine(GetWorldTM().TransformPoint(m_SourceVertices[m_nSelectedSourceVertex].position), GetWorldTM().TransformPoint(m_PickedPos));
			}
		}
		if (m_nSelectedUpVertex != -1)
		{
			BrushVec3 vWorldVertexPos = GetWorldTM().TransformPoint(m_SourceVertices[m_nSelectedUpVertex].position);
			BrushVec3 vBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, vWorldVertexPos);
			dc.SetColor(ColorB(0, 150, 214, 255));
			dc.DrawSolidBox(ToVec3(vWorldVertexPos - vBoxSize), ToVec3(vWorldVertexPos + vBoxSize));
		}
	}
	else if (m_Phase == eMTP_ChooseMoveToTargetPoint)
	{
		std::vector<BrushVec3> excludedVertices;
		if (m_bPickedTargetPos)
		{
			excludedVertices.push_back(m_TargetPos);
			dc.SetColor(kSelectedColor);
			BrushVec3 vWorldPos = GetWorldTM().TransformPoint(m_TargetPos);
			BrushVec3 vBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, vWorldPos);
			dc.DrawSolidBox(ToVec3(vWorldPos - vBoxSize), ToVec3(vWorldPos + vBoxSize));
		}

		dc.PushMatrix(GetWorldTM());
		Display::DisplayHighlightedVertices(dc, GetMainContext(), true);
		dc.PopMatrix();
	}
	dc.PushMatrix(GetWorldTM());
}
}

