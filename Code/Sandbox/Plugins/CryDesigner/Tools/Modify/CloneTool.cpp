// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CloneTool.h"
#include "SurfaceInfoPicker.h"
#include "Prefabs/PrefabItem.h"
#include "Prefabs/PrefabManager.h"
#include "Objects/PrefabObject.h"
#include "Objects/DisplayContext.h"
#include "ViewManager.h"
#include "DesignerEditor.h"
#include "ToolFactory.h"
#include <CrySerialization/Enum.h>
#include "Serialization/Decorators/EditorActionButton.h"
#include "Objects/DisplayContext.h"

namespace Designer
{
SERIALIZATION_ENUM_BEGIN(EPlacementType, "PlacementType")
SERIALIZATION_ENUM(ePlacementType_Divide, "ePlacementType_Divide", "Divide")
SERIALIZATION_ENUM(ePlacementType_Multiply, "ePlacementType_Multiply", "Multiply")
SERIALIZATION_ENUM_END()

void CloneParameter::Serialize(Serialization::IArchive& ar)
{
	ar(Serialization::Range(m_NumberOfClones, 1, 64), "CloneCount", "Clone Count");
	if (m_PlacementType != ePlacementType_None)
		ar(m_PlacementType, "PlacementType", "Placement Type");
}

CloneTool::CloneTool(EDesignerTool tool) :
	BaseTool(tool),
	m_bSuspendedUndo(false)
{
	if (Tool() == eDesigner_CircleClone)
		m_CloneParameter.m_PlacementType = ePlacementType_None;
	else
		m_CloneParameter.m_PlacementType = ePlacementType_Divide;
}

const char* CloneTool::ToolClass() const
{
	return Serialization::getEnumDescription<EDesignerTool>().label(eDesigner_CircleClone);
}

void CloneTool::Enter()
{
	__super::Enter();

	m_vStartPos = Vec3(0, 0, 0);
	m_SelectedClone = (Clone*)GetBaseObject();
	m_InitObjectWorldPos = m_SelectedClone->GetWorldPos();

	AABB boundbox;
	m_SelectedClone->GetBoundBox(boundbox);

	m_vStartPos = GetCenterBottom(boundbox);
	m_vPickedPos = m_vStartPos;

	m_fRadius = 0.1f;

	if (!m_bSuspendedUndo)
	{
		GetIEditor()->GetIUndoManager()->Suspend();
		m_bSuspendedUndo = true;
	}

	// TODO: exit cleanly instead of falling back to other tools
	if (GetModel()->IsEmpty(eShelf_Base))
		GetDesigner()->SwitchTool(eDesigner_Invalid);
}

void CloneTool::Leave()
{
	__super::Leave();

	if (m_bSuspendedUndo)
	{
		GetIEditor()->GetIUndoManager()->Resume();
		m_bSuspendedUndo = false;
	}

	if (m_SelectedClone)
		m_SelectedClone->SetWorldPos(m_InitObjectWorldPos);

	m_Clones.clear();
}

void CloneTool::Serialize(Serialization::IArchive& ar)
{
	ar(Serialization::ActionButton([this] { Confirm();
	                               }), "Confirm", "Confirm");
	m_CloneParameter.Serialize(ar);
}

bool CloneTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	UpdateCloneList();
	m_vPickedPos = m_vStartPos;
	m_Plane = BrushPlane(m_vStartPos, m_vStartPos + Vec3(0, 1, 0), m_vStartPos + Vec3(1, 0, 0));

	return true;
}

bool CloneTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	if (nFlags & MK_LBUTTON)
	{
		if (Tool() == eDesigner_ArrayClone)
		{
			CSurfaceInfoPicker picker;
			SRayHitInfo hitInfo;
			int nCloneCount = m_Clones.size();
			DESIGNER_ASSERT(nCloneCount > 0);
			if (nCloneCount > 0)
			{
				CSurfaceInfoPicker::CExcludedObjects excludedObjList;
				excludedObjList.Add(m_SelectedClone);
				for (int i = 0; i < nCloneCount; ++i)
					excludedObjList.Add(m_Clones[i]);

				if (picker.Pick(point, hitInfo, &excludedObjList))
				{
					m_vPickedPos = hitInfo.vHitPos;
					UpdateClonePositions();
				}
			}
		}
		else if (Tool() == eDesigner_CircleClone)
		{
			Vec3 vWorldRaySrc;
			Vec3 vWorldRayDir;
			GetIEditor()->GetActiveView()->ViewToWorldRay(point, vWorldRaySrc, vWorldRayDir);
			BrushVec3 vOut;
			if (m_Plane.HitTest(BrushRay(vWorldRaySrc, vWorldRayDir), NULL, &vOut))
			{
				m_vPickedPos = ToVec3(vOut);
				UpdateClonePositions();
			}
		}
	}
	return true;
}

void CloneTool::Display(struct DisplayContext& dc)
{
	dc.SetColor(ColorB(100, 200, 100, 255));

	Matrix34 tm = dc.GetMatrix();
	dc.PopMatrix();

	if (Tool() == eDesigner_CircleClone)
	{
		dc.DrawCircle(m_vStartPos, m_fRadius);
		dc.DrawLine(m_vStartPos, Vec3(m_vPickedPos.x, m_vPickedPos.y, m_vStartPos.z));
	}
	else
	{
		dc.DrawLine(m_vStartPos, m_vPickedPos);
	}

	dc.PushMatrix(tm);
}

void CloneTool::SetPivotToObject(CBaseObject* pObj, const Vec3& pos)
{
	AABB localBB;
	pObj->GetLocalBounds(localBB);
	Vec3 vLocalCenterBottom = GetCenterBottom(localBB);
	pObj->SetPos(pos - vLocalCenterBottom);
}

Vec3 CloneTool::GetCenterBottom(const AABB& aabb) const
{
	Vec3 vCenterBottom;
	vCenterBottom.x = (aabb.min.x + aabb.max.x) * 0.5f;
	vCenterBottom.y = (aabb.min.y + aabb.max.y) * 0.5f;
	vCenterBottom.z = aabb.min.z;
	return vCenterBottom;
}

void CloneTool::OnChangeParameter(bool continuous)
{
	UpdateCloneList();
	UpdateClonePositions();
}

void CloneTool::UpdateCloneList()
{
	if (!m_SelectedClone || m_Clones.size() == m_CloneParameter.m_NumberOfClones)
		return;

	m_Clones.clear();
	m_Clones.reserve(m_CloneParameter.m_NumberOfClones);

	for (int i = 0; i < m_CloneParameter.m_NumberOfClones; ++i)
	{
		Clone* pClone = new Clone;
		pClone->SetModel(new Model(*m_SelectedClone->GetModel()));
		pClone->SetMaterial(m_SelectedClone->GetMaterial());
		pClone->GetCompiler()->Compile(pClone, pClone->GetModel());
		m_Clones.push_back(pClone);
	}

	GetIEditor()->GetViewManager()->UpdateViews();
}

void CloneTool::UpdateClonePositions()
{
	if (Tool() == eDesigner_ArrayClone)
		UpdateClonePositionsAlongLine();
	else
		UpdateClonePositionsAlongCircle();
}

void CloneTool::UpdateClonePositionsAlongLine()
{
	int nCloneCount = m_Clones.size();
	DESIGNER_ASSERT(nCloneCount > 0);
	if (nCloneCount <= 0)
		return;
	Vec3 vDelta = m_CloneParameter.m_PlacementType == ePlacementType_Divide ? (m_vPickedPos - m_vStartPos) / nCloneCount : (m_vPickedPos - m_vStartPos);
	for (int i = 1; i <= nCloneCount; ++i)
		SetPivotToObject(m_Clones[i - 1], m_vStartPos + vDelta * i);
	SetPivotToObject(m_SelectedClone, m_vStartPos);
}

void CloneTool::UpdateClonePositionsAlongCircle()
{
	int nCloneCount = m_Clones.size();
	DESIGNER_ASSERT(nCloneCount > 0);
	if (nCloneCount <= 0)
		return;
	m_fRadius = (Vec2(m_vStartPos) - Vec2(m_vPickedPos)).GetLength();
	if (m_fRadius < 0.1f)
		m_fRadius = 0.1f;

	BrushVec2 vCenterOnPlane = m_Plane.W2P(ToBrushVec3(m_vStartPos));
	BrushVec2 vPickedPosOnPlane = m_Plane.W2P(ToBrushVec3(m_vPickedPos));
	float fAngle = ComputeAnglePointedByPos(vCenterOnPlane, vPickedPosOnPlane);

	int nCloneCountInPanel = m_CloneParameter.m_NumberOfClones;
	std::vector<BrushVec2> vertices2D;
	MakeSectorOfCircle(m_fRadius, m_Plane.W2P(ToBrushVec3(m_vStartPos)), fAngle, PI2, nCloneCountInPanel + 1, vertices2D);

	for (int i = 0; i < nCloneCountInPanel; ++i)
	{
		if (i == 0)
			SetPivotToObject(m_SelectedClone, ToVec3(m_Plane.P2W(vertices2D[i])));
		else
			SetPivotToObject(m_Clones[i - 1], ToVec3(m_Plane.P2W(vertices2D[i])));
	}
}

void CloneTool::Confirm()
{
	if (m_bSuspendedUndo)
	{
		GetIEditor()->GetIUndoManager()->Resume();
		m_bSuspendedUndo = false;
	}
	FreezeClones();
}

void CloneTool::FreezeClones()
{
	if (!m_SelectedClone || m_Clones.empty())
		return;

	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	if (!pPrefabManager)
		return;

	IDataBaseLibrary* pLibrary = pPrefabManager->FindLibrary("Level");
	if (!pLibrary)
		return;

	CUndo undo("Clone Designer Object");

	CSelectionGroup selectionGroup;
	selectionGroup.AddObject(m_SelectedClone);

	DesignerSession::GetInstance()->SetBaseObject(nullptr);

	// store the clones locally. Once we create a prefab from selection group, the designer object
	// is destroyed and the session ends, deleting this designer tool itself, so we need to keep a copy of
	// the objects on the stack.
	std::vector<ClonePtr> localClones = m_Clones;
	int iCloneCount(localClones.size());

	// circle clone has one less object because it does not keep the original
	if (Tool() == eDesigner_CircleClone)
	{
		--iCloneCount;
	}

	CPrefabItem* pPrefabItem = (CPrefabItem*)GetIEditor()->GetPrefabManager()->CreateItem(pLibrary);
	pPrefabItem->MakeFromSelection(selectionGroup);

	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	if (pSelection && !pSelection->IsEmpty())
	{
		CBaseObject* pSelected = pSelection->GetObject(0);
		if (pSelected->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefabObj = ((CPrefabObject*)pSelected);
			// this prefab still has the old clone name, fix it here
			pPrefabObj->SetUniqName(PREFAB_OBJECT_CLASS_NAME);
			pPrefabObj->Open();
		}
	}

	for (int i = 0; i < iCloneCount; ++i)
	{
		CPrefabObject* pDesignerPrefabObj = (CPrefabObject*)GetIEditor()->GetObjectManager()->NewObject(PREFAB_OBJECT_CLASS_NAME);
		pDesignerPrefabObj->SetPrefab(pPrefabItem, true);

		AABB aabb;
		localClones[i]->GetLocalBounds(aabb);

		pDesignerPrefabObj->SetWorldPos(localClones[i]->GetWorldPos() + aabb.min);
		pDesignerPrefabObj->Open();
	}
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_CircleClone, eToolGroup_Modify, "Circle Clone", CloneTool,
                                                           circleclone, "runs Circle Clone Tool", "designer.circleclone")
REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_ArrayClone, eToolGroup_Modify, "Array Clone", ArrayCloneTool,
                                                           arrayclone, "runs Array Clone Tool", "designer.arrayclone")

