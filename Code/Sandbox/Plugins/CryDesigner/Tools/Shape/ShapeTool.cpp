// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ShapeTool.h."
#include "DesignerEditor.h"
#include "ViewManager.h"
#include "Core/Model.h"
#include "Core/Helper.h"
#include "Tools/Edit/SeparateTool.h"
#include "Tools/Misc/ResetXFormTool.h"
#include "Tools/Select/SelectTool.h"
#include "Core/SmoothingGroupManager.h"
#include "Util/Display.h"
#include "GameEngine.h"

#include "IAIManager.h"

namespace Designer
{
void ShapeTool::Enter()
{
	__super::Enter();
	SpotManager::ResetAllSpots();
}

void ShapeTool::Leave()
{
	UpdateSurfaceInfo();
	__super::Leave();
}

void ShapeTool::Serialize(Serialization::IArchive& ar)
{
	ar(m_bEnableMagnet, "Enable Magnet", "^^Enable Magnet");
}

bool ShapeTool::UpdateCurrentSpotPosition(
  CViewport* view,
  UINT nFlags,
  CPoint point,
  bool bKeepInitialPlane,
  bool bSearchAllShelves)
{
	EnableMagnetic(m_bEnableMagnet);

	if (!SpotManager::UpdateCurrentSpotPosition(
	      GetModel(),
	      GetBaseObject()->GetWorldTM(),
	      GetPlane(),
	      view,
	      point,
	      bKeepInitialPlane,
	      bSearchAllShelves))
	{
		DesignerSession::GetInstance()->UpdateSelectionMesh(
		  NULL,
		  GetCompiler(),
		  GetBaseObject());
		return false;
	}

	DesignerSession::GetInstance()->UpdateSelectionMesh(GetCurrentSpot().m_pPolygon, GetCompiler(), GetBaseObject());
	return true;
}

void ShapeTool::DisplayCurrentSpot(DisplayContext& dc)
{
	dc.SetFillMode(e_FillModeSolid);
	DrawCurrentSpot(dc, GetWorldTM());
}

TexInfo ShapeTool::GetTexInfo() const
{
	if (GetCurrentSpot().m_pPolygon)
		return GetCurrentSpot().m_pPolygon->GetTexInfo();
	return TexInfo();
}

int ShapeTool::GetMatID() const
{
	if (GetCurrentSpot().m_pPolygon)
		return GetCurrentSpot().m_pPolygon->GetSubMatID();
	return 0;
}

void ShapeTool::Register2DShape(PolygonPtr polygon)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	Separate1stStep();

	bool bEmptyDesigner = GetModel()->IsEmpty(eShelf_Base);

	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();

	if (polygon)
	{
		GetModel()->SetShelf(eShelf_Base);
		if (bEmptyDesigner)
			GetModel()->AddSplitPolygon(polygon->Clone());
		else
			GetModel()->AddSplitPolygon(polygon);
		MirrorUtil::UpdateMirroredPartWithPlane(GetModel(), polygon->GetPlane());
	}

	GetModel()->GetSmoothingGroupMgr()->InvalidateAll();

	if (bEmptyDesigner || gDesignerSettings.bKeepPivotCentered)
		ApplyPostProcess();
	else
		ApplyPostProcess(ePostProcess_ExceptCenterPivot);

	if (!bEmptyDesigner && IsSeparateStatus())
	{
		Separate2ndStep();
	}
	else
	{
		ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
		pSelected->Clear();
	}

	if (polygon)
		polygon->Init();
}

void ShapeTool::Separate1stStep()
{
	if (!IsSeparateStatus() || GetModel()->CheckFlag(eModelFlag_Mirror))
		return;

	if (GetBaseObject()->GetType() != OBJTYPE_SOLID)
		return;

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Clear();
	for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = GetModel()->GetPolygon(i);
		Element de;
		de.SetPolygon(GetBaseObject(), pPolygon);
		pSelected->Add(de);
	}
}

void ShapeTool::Separate2ndStep()
{
	if (!IsSeparateStatus() || GetModel()->CheckFlag(eModelFlag_Mirror))
		return;

	if (GetBaseObject()->GetType() != OBJTYPE_SOLID)
		return;

	DesignerObject* pSparatedObj = SeparateTool::Separate(GetMainContext());
	if (pSparatedObj)
	{
		DesignerSession::GetInstance()->SetBaseObject(pSparatedObj);
	}
}

void ShapeTool::RegisterShape(PolygonPtr pFloorPolygon)
{
	if (!GetModel() || GetModel()->IsEmpty(eShelf_Construction))
		return;

	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	Separate1stStep();

	bool bEmptyModel = IsModelEmpty();

	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->RemovePolygonsWithSpecificFlagsAndPlane(ePolyFlag_Mirrored);

	bool bUniquePolygon = GetModel()->GetPolygonCount() == 1 ? true : false;

	PolygonList polygons;
	GetModel()->GetPolygonList(polygons);
	PolygonPtr pPolygonTouchingFloor = NULL;

	BrushPlane floorPlane = pFloorPolygon ? pFloorPolygon->GetPlane() : GetPlane();
	BrushPlane invFloorPlane = floorPlane.GetInverted();

	for (int i = 0, iPolygonCount(polygons.size()); i < iPolygonCount; ++i)
	{
		const BrushPlane& plane = polygons[i]->GetPlane();
		if (!plane.IsEquivalent(floorPlane) && !plane.IsEquivalent(invFloorPlane))
			continue;
		GetModel()->RemovePolygon(polygons[i]);
		if (plane.IsEquivalent(invFloorPlane))
		{
			pPolygonTouchingFloor = polygons[i]->Flip();
			break;
		}
	}

	GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
	GetModel()->SetShelf(eShelf_Base);

	if (pPolygonTouchingFloor)
	{
		bool bAdded = false;
		if (!IsSeparateStatus())
		{
			if (pFloorPolygon && pPolygonTouchingFloor->IncludeAllEdges(pFloorPolygon))
			{
				GetModel()->RemovePolygon(GetModel()->QueryEquivalentPolygon(pFloorPolygon));
				PolygonPtr pClonedPolygon = pPolygonTouchingFloor->Clone();
				pClonedPolygon->Subtract(pFloorPolygon);
				if (pClonedPolygon->IsValid())
					GetModel()->AddPolygon(pClonedPolygon->Flip());
				bAdded = true;
			}
			else if (GetModel()->HasIntersection(pPolygonTouchingFloor))
			{
				GetModel()->AddXORPolygon(pPolygonTouchingFloor);
				bAdded = true;
			}
		}
		if (!bAdded)
			GetModel()->AddPolygon(pPolygonTouchingFloor->Flip());
	}

	BrushVec3 vNewPivot = GetModel()->GetBoundBox().min;
	GetModel()->GetSmoothingGroupMgr()->InvalidateAll();

	if (!bEmptyModel && IsSeparateStatus())
		Separate2ndStep();

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Clear();

	int nUpdateFlags = ePostProcess_All;
	if (!gDesignerSettings.bKeepPivotCentered)
	{
		nUpdateFlags = ePostProcess_ExceptCenterPivot;
		if (!bEmptyModel && IsSeparateStatus())
		{
			AABB aabb;
			GetBaseObject()->GetLocalBounds(aabb);
			PivotToPos(GetBaseObject(), GetModel(), aabb.min);
		}
	}

	ApplyPostProcess(nUpdateFlags);
}

bool ShapeTool::IsSeparateStatus() const
{
	if (GetBaseObject()->GetType() != OBJTYPE_SOLID)
		return false;
	return m_bSeparatedNewShape;
}

void ShapeTool::CancelCreation()
{
	AABB boundsInWorld;
	GetBaseObject()->GetBoundBox(boundsInWorld);

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();
	CompileShelf(eShelf_Construction);

	GetIEditor()->GetAIManager()->OnAreaModified(boundsInWorld);
}

void ShapeTool::StoreSeparateStatus()
{
	m_bSeparatedNewShape = GetAsyncKeyState(VK_SHIFT);
}
}

