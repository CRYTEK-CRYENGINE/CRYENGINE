// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DesignerEditor.h"
#include "Util/HeightManipulator.h"
#include "ExtrudeTool.h"
#include "ExtrudeMultipleTool.h"

namespace Designer
{
SERIALIZATION_ENUM_BEGIN(EExtrudePlaneAxis, "Extrude Plane Axis")
SERIALIZATION_ENUM(eExtrudePlaneAxis_Average, "Average", "Average")
SERIALIZATION_ENUM(eExtrudePlaneAxis_Individual, "Individual", "Individual")
SERIALIZATION_ENUM(eExtrudePlaneAxis_X, "X", "X")
SERIALIZATION_ENUM(eExtrudePlaneAxis_Y, "Y", "Y")
SERIALIZATION_ENUM(eExtrudePlaneAxis_Z, "Z", "Z")
SERIALIZATION_ENUM_END()

void ExtrudeMultipleTool::Enter()
{
	__super::Enter();
	Initialize();
	m_Context.Invalidate();
}

void ExtrudeMultipleTool::Leave()
{
	__super::Leave();
	s_HeightManipulator.Invalidate();
}

void ExtrudeMultipleTool::Serialize(Serialization::IArchive& ar)
{
	if (ar.isEdit())
		ar(m_Params.axis, "ExtrudeAxis", "^^Extrude Axis");

	if (ar.openBlock("LeaveEdgeLoop", " "))
	{
		ar(m_Params.bLeaveEdgeLoop, "LeaveEdgeLoop", "^^Leave Edge Loop");
		ar.closeBlock();
	}

	Initialize();
	m_bStartedManipluation = false;
}

bool ExtrudeMultipleTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (s_HeightManipulator.Start(GetWorldTM(), view, GetDesigner()->GetRay()))
	{
		StartExtrusion();
		UpdateModel(view);
		ApplyPostProcess(ePostProcess_Mesh);
		m_bStartedManipluation = true;
	}
	else
	{
		__super::OnLButtonDown(view, nFlags, point);
		Initialize();
		m_bStartedManipluation = false;
	}

	return true;
}

bool ExtrudeMultipleTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	bool bCursorCloseToPole = s_HeightManipulator.IsCursorCloseToPole(GetWorldTM(), view, GetDesigner()->GetRay());
	s_HeightManipulator.HighlightPole(bCursorCloseToPole);

	if (nFlags & MK_LBUTTON)
	{
		if (m_bStartedManipluation)
		{
			UpdateModel(view);
			UpdateSelectedElements();
			GetCompiler()->Compile(GetBaseObject(), GetModel(), eShelf_Construction);
		}
	}

	return true;
}

bool ExtrudeMultipleTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	if (!m_bStartedManipluation)
		return true;

	s_HeightManipulator.Invalidate();

	if (m_Params.bLeaveEdgeLoop)
	{
		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
	}
	else
	{
		MODEL_SHELF_RECONSTRUCTOR(GetModel());
		GetModel()->SetShelf(eShelf_Construction);
		std::vector<PolygonPtr> polygons;
		for (int i = 0, iCount(GetModel()->GetPolygonCount()); i < iCount; ++i)
			polygons.push_back(GetModel()->GetPolygon(i));
		GetModel()->Clear();
		GetModel()->SetShelf(eShelf_Base);
		auto ii = polygons.begin();
		for (; ii != polygons.end(); ++ii)
			GetModel()->AddUnionPolygon(*ii);
	}

	DesignerSession* pSession = DesignerSession::GetInstance();

	if (!m_Context.argumentModels.empty())
	{
		ElementSet* pElements = pSession->GetSelectedElements();
		pElements->Clear();
		auto ii = m_Context.argumentModels.begin();
		for (; ii != m_Context.argumentModels.end(); ++ii)
		{
			if ((*ii)->GetCapPolygon())
				pElements->Add(Element(GetBaseObject(), (*ii)->GetCapPolygon()));
		}
		pSession->UpdateSelectionMeshFromSelectedElements();
		Initialize();
	}

	m_bStartedManipluation = false;
	m_Context.Invalidate();

	UpdateSurfaceInfo();
	ApplyPostProcess();
	GetIEditor()->GetIUndoManager()->Accept("Designer : Extrude Multiple");

	return true;
}

void ExtrudeMultipleTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	__super::OnEditorNotifyEvent(event);

	switch (event)
	{
	case eNotify_OnEndUndoRedo:
		Initialize();
		break;
	}
}

void ExtrudeMultipleTool::Display(DisplayContext& dc)
{
	__super::Display(dc);
	s_HeightManipulator.Display(dc);
}

void ExtrudeMultipleTool::Initialize()
{
	BrushVec3 vPivot(0, 0, 0);
	BrushVec3 vNormal(0, 0, 0);
	BrushVec3 vAverageNormal(0, 0, 0);
	int polygonCounter = 0;

	ElementSet* pElements = DesignerSession::GetInstance()->GetSelectedElements();
	for (int i = 0, iCount(pElements->GetCount()); i < iCount; ++i)
	{
		const Element& element = pElements->Get(i);
		if (!element.IsPolygon())
			continue;
		++polygonCounter;
		vPivot += element.m_pPolygon->GetRepresentativePosition();
		vAverageNormal += element.m_pPolygon->GetPlane().Normal();
	}

	if (polygonCounter == 0)
	{
		s_HeightManipulator.Invalidate();
		return;
	}

	if (polygonCounter > 0 && !vAverageNormal.IsZero(kDesignerEpsilon))
	{
		vPivot /= polygonCounter;
		vNormal.Normalize();
	}

	if (m_Params.axis == eExtrudePlaneAxis_X)
		vNormal = BrushVec3(1, 0, 0);
	else if (m_Params.axis == eExtrudePlaneAxis_Y)
		vNormal = BrushVec3(0, 1, 0);
	else if (m_Params.axis == eExtrudePlaneAxis_Z)
		vNormal = BrushVec3(0, 0, 1);
	else
		vNormal = vAverageNormal;

	if (vNormal.IsZero(kDesignerEpsilon))
		vNormal = BrushVec3(0, 0, 1);

	if (vNormal.Dot(vAverageNormal) < kDesignerEpsilon)
		vNormal = -vNormal;

	s_HeightManipulator.Init(BrushPlane(vNormal, -vNormal.Dot(vPivot)), vPivot, true);
}

void ExtrudeMultipleTool::UpdateSelectedElements()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pElements = pSession->GetSelectedElements();
	pElements->Clear();
	auto ii = m_Context.argumentModels.begin();
	for (; ii != m_Context.argumentModels.end(); ++ii)
	{
		if ((*ii)->GetCapPolygon())
			pElements->Add(Element(GetBaseObject(), (*ii)->GetCapPolygon()));
	}
	pSession->UpdateSelectionMeshFromSelectedElements();
}

void ExtrudeMultipleTool::StartExtrusion()
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Base);

	m_Context.Invalidate();

	m_Context.pModel = GetModel();
	m_Context.pObject = GetBaseObject();
	m_Context.pCompiler = GetCompiler();

	ElementSet* pElements = DesignerSession::GetInstance()->GetSelectedElements();

	std::vector<PolygonPtr> polygons;
	for (int i = 0, iCount(pElements->GetCount()); i < iCount; ++i)
	{
		const Element& element = pElements->Get(i);
		if (!element.IsPolygon())
			continue;
		polygons.push_back(element.m_pPolygon);
	}

	if (polygons.empty())
		return;

	GetIEditor()->GetIUndoManager()->Begin();
	GetDesigner()->StoreSelectionUndo(false);
	GetModel()->RecordUndo("Designer : Extrude Multiple", GetBaseObject());

	std::vector<BrushEdge3D> sharedEdges = FindSharedEdges(polygons);

	for (auto iPolygon = polygons.begin(); iPolygon != polygons.end(); ++iPolygon)
	{
		ArgumentModelPtr pArgument = new ArgumentModel(*iPolygon, 0, GetBaseObject(), NULL, GetModel()->GetDB());
		pArgument->SetExcludedEdges(sharedEdges);
		m_Context.argumentModels.push_back(pArgument);
		int nIndex = -1;
		if (GetModel()->QueryEquivalentPolygon(*iPolygon, &nIndex))
		{
			GetModel()->RemovePolygon(nIndex);
			MirrorUtil::UpdateMirroredPartWithPlane(GetModel(), (*iPolygon)->GetPlane());
		}

		for (int i = 0, iEdgeCount((*iPolygon)->GetEdgeCount()); i < iEdgeCount; ++i)
		{
			BrushEdge3D e = (*iPolygon)->GetEdge(i);
			if (HasEdge(sharedEdges, e))
				continue;

			BrushVec3 vNormal = m_Params.axis == eExtrudePlaneAxis_Individual ? (*iPolygon)->GetPlane().Normal() : s_HeightManipulator.GetDir();
			std::vector<PolygonPtr> perpendicularPolygons;
			GetModel()->QueryPerpendicularPolygons(*iPolygon, perpendicularPolygons, &vNormal);

			for (auto ii = perpendicularPolygons.begin(); ii != perpendicularPolygons.end(); ++ii)
				AddPolygonToListCheckingDuplication(m_Context.affectedPolygons, *ii);
		}
	}

	for (auto iPolygon = m_Context.affectedPolygons.begin(); iPolygon != m_Context.affectedPolygons.end(); ++iPolygon)
		GetModel()->RemovePolygon(*iPolygon);
}

std::vector<BrushEdge3D> ExtrudeMultipleTool::FindSharedEdges(const std::vector<PolygonPtr>& polygons)
{
	std::vector<BrushEdge3D> sharedEdges;

	int iPolygonCount(polygons.size());
	for (int i = 0; i < iPolygonCount; ++i)
	{
		for (int a = 0, iEdgeCount(polygons[i]->GetEdgeCount()); a < iEdgeCount; ++a)
		{
			BrushEdge3D e = polygons[i]->GetEdge(a);
			for (int k = i + 1; k < iPolygonCount; ++k)
			{
				if (!polygons[k]->HasEdge(e))
					continue;

				if (!polygons[k]->GetPlane().IsEquivalent(polygons[i]->GetPlane()) && m_Params.axis == eExtrudePlaneAxis_Individual)
					continue;

				sharedEdges.push_back(e);
				break;
			}
		}
	}

	return sharedEdges;
}

bool ExtrudeMultipleTool::HasEdge(const std::vector<BrushEdge3D>& edges, const BrushEdge3D& e) const
{
	for (auto ii = edges.begin(); ii != edges.end(); ++ii)
	{
		if (e.IsEquivalent(*ii) || e.IsEquivalent((*ii).GetInverted()))
			return true;
	}
	return false;
}

void ExtrudeMultipleTool::UpdateModel(CViewport* view)
{
	m_Context.height = s_HeightManipulator.Update(GetWorldTM(), view, GetDesigner()->GetRay());
	m_Context.dir = s_HeightManipulator.GetDir();
	m_Context.bIndividual = m_Params.axis == eExtrudePlaneAxis_Individual;
	m_Context.bLeaveLoopEdge = m_Params.bLeaveEdgeLoop;
	UpdateModel(m_Context);
}

void ExtrudeMultipleTool::UpdateModel(ExtrudeMultipleContext& emc)
{
	MODEL_SHELF_RECONSTRUCTOR(emc.pModel);

	emc.pModel->SetShelf(eShelf_Construction);
	emc.pModel->Clear();
	for (auto ii = emc.affectedPolygons.begin(); ii != emc.affectedPolygons.end(); ++ii)
		emc.pModel->AddPolygon((*ii)->Clone(), false);

	ExtrusionContext ec;
	ec.pObject = emc.pObject;
	ec.pCompiler = emc.pCompiler;
	ec.pModel = emc.pModel;
	ec.bFirstUpdate = false;
	ec.bTouchedMirrorPlane = false;
	ec.bUpdateBrush = false;
	ec.bUpdateSelectionMesh = false;
	ec.bResetShelf = false;
	ec.bLeaveLoopEdges = emc.bLeaveLoopEdge;
	ec.backupPolygons = emc.affectedPolygons;

	for (auto ii = emc.argumentModels.begin(); ii != emc.argumentModels.end(); ++ii)
	{
		(*ii)->SetHeight(emc.height);
		if (!emc.bIndividual)
			(*ii)->SetRaiseDir(emc.dir);
		(*ii)->Update();
		ec.pArgumentModel = *ii;
		ExtrudeTool::UpdateModel(ec);
	}

	Designer::ApplyPostProcess(MainContext(ec.pObject, ec.pCompiler, ec.pModel), ePostProcess_Mirror);
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_ExtrudeMultiple, eToolGroup_Edit, "Extrude Multiple", ExtrudeMultipleTool,
                                                           extrudemultiple, "runs extrude multiple tool", "designer.extrudemultiple")

