// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ToolCommon.h"
#include "Core/Helper.h"
#include "DesignerEditor.h"
#include "Objects/PrefabObject.h"
#include "Util/ElementSet.h"
#include "UIs/UICommon.h"
#include "Util/ExcludedEdgeManager.h"
#include "Controls/QuestionDialog.h"
#include "Gizmos/ITransformManipulator.h"

namespace Designer
{
SERIALIZATION_ENUM_BEGIN(EPlaneAxis, "Plane Axis")
SERIALIZATION_ENUM(ePlaneAxis_Average, "Average", "Average")
SERIALIZATION_ENUM(ePlaneAxis_X, "X", "X")
SERIALIZATION_ENUM(ePlaneAxis_Y, "Y", "Y")
SERIALIZATION_ENUM(ePlaneAxis_Z, "Z", "Z")
SERIALIZATION_ENUM_END()

bool IsCreationTool(EDesignerTool tool)
{
	return tool == eDesigner_Box || tool == eDesigner_Sphere ||
	       tool == eDesigner_Cylinder || tool == eDesigner_Cone ||
	       tool == eDesigner_Rectangle || tool == eDesigner_Disc ||
	       tool == eDesigner_Stair || tool == eDesigner_Polyline ||
	       tool == eDesigner_Curve || tool == eDesigner_StairProfile;
}

bool IsSelectElementMode(EDesignerTool mode)
{
	return mode == eDesigner_Vertex || mode == eDesigner_Edge || mode == eDesigner_Polygon ||
	       mode == eDesigner_VertexEdge || mode == eDesigner_VertexPolygon || mode == eDesigner_EdgePolygon ||
	       mode == eDesigner_VertexEdgePolygon;
}

bool IsEdgeSelectMode(EDesignerTool mode)
{
	return mode == eDesigner_Edge ||
	       mode == eDesigner_VertexEdge || mode == eDesigner_EdgePolygon ||
	       mode == eDesigner_VertexEdgePolygon || mode == eDesigner_ExtrudeEdge;
}

void GetSelectedObjectList(std::vector<MainContext>& selections)
{
	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	if (pSelection == NULL)
		return;

	for (int i = 0, iCount(pSelection->GetCount()); i < iCount; ++i)
	{
		MainContext selectedInfo;
		CBaseObject* pObj = pSelection->GetObject(i);
		if (pObj == NULL || pObj->GetType() != OBJTYPE_SOLID && pObj->GetType() != OBJTYPE_VOLUMESOLID)
			continue;

		selectedInfo.pObject = pObj;

		GetCompiler(pObj, selectedInfo.pCompiler);
		GetModel(pObj, selectedInfo.pModel);

		selections.push_back(selectedInfo);
	}
}

DesignerEditor* GetDesigner()
{
	CEditTool* pEditor = GetIEditor()->GetEditTool();
	if (!pEditor || !pEditor->IsKindOf(RUNTIME_CLASS(DesignerEditor)))
		return NULL;
	return (DesignerEditor*)pEditor;
}

void SyncPrefab(MainContext& mc)
{
	if (!mc.IsValid())
		return;

	mc.pObject->UpdateGroup();
	mc.pObject->UpdatePrefab();
}

void SyncMirror(Model* pModel)
{
	if (!pModel->CheckFlag(eModelFlag_Mirror))
		return;

	MODEL_SHELF_RECONSTRUCTOR(pModel);

	for (int i = eShelf_Base; i < cShelfMax; ++i)
	{
		pModel->SetShelf(static_cast<ShelfID>(i));
		pModel->RemovePolygonsWithSpecificFlagsAndPlane(ePolyFlag_Mirrored);
		for (int i = 0, iPolygonSize(pModel->GetPolygonCount()); i < iPolygonSize; ++i)
		{
			PolygonPtr pPolygon = pModel->GetPolygon(i);
			if (pPolygon->CheckFlags(ePolyFlag_Mirrored))
				continue;
			MirrorUtil::AddMirroredPolygon(pModel, pPolygon);
		}
	}
}

void RunTool(EDesignerTool tool)
{
	if (GetDesigner())
		GetDesigner()->SwitchTool(tool, false, false);
}

void ApplyPostProcess(MainContext& mc, int postprocesses)
{
	if (!mc.IsValid())
		return;

	bool bMirror = mc.pModel->CheckFlag(eModelFlag_Mirror);

	if (postprocesses & ePostProcess_Mirror)
	{
		if (bMirror)
			SyncMirror(mc.pModel);
	}

	if (postprocesses & ePostProcess_CenterPivot)
	{
		if (gDesignerSettings.bKeepPivotCentered && !bMirror)
			PivotToCenter(mc);
	}

	if (postprocesses & ePostProcess_DataBase)
		mc.pModel->ResetDB(eDBRF_ALL);

	if (postprocesses & ePostProcess_SmoothingGroup)
		mc.pModel->InvalidateSmoothingGroups();

	if (postprocesses & ePostProcess_Mesh)
		mc.pCompiler->Compile(mc.pObject, mc.pModel);

	if (postprocesses & ePostProcess_GameResource)
		UpdateGameResource(mc.pObject);

	if (postprocesses & ePostProcess_SyncPrefab)
		SyncPrefab(mc);
}

void UpdateDrawnEdges(MainContext& mc)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();

	pSession->GetExcludedEdgeManager()->Clear();

	for (int i = 0, iElementSize(pSelected->GetCount()); i < iElementSize; ++i)
	{
		const Element& element = pSelected->Get(i);
		if (element.IsEdge())
		{
			DESIGNER_ASSERT(element.m_Vertices.size() == 2);
			pSession->GetExcludedEdgeManager()->Add(BrushEdge3D(element.m_Vertices[0], element.m_Vertices[1]));
		}
		else if (element.IsPolygon() && element.m_pPolygon->IsOpen())
		{
			for (int a = 0, iEdgeCount(element.m_pPolygon->GetEdgeCount()); a < iEdgeCount; ++a)
			{
				BrushEdge3D e = element.m_pPolygon->GetEdge(a);
				pSession->GetExcludedEdgeManager()->Add(e);
			}
		}
	}
}

BrushMatrix34 GetOffsetTM(ITransformManipulator* pManipulator, const BrushVec3& vOffset, const BrushMatrix34& worldTM)
{
	int editMode = GetIEditor()->GetEditMode();

	BrushMatrix34 worldRefTM(ToBrushMatrix34(pManipulator->GetTransform()));
	BrushMatrix34 invWorldTM = worldTM.GetInverted();
	BrushMatrix34 modRefFrame = invWorldTM * worldRefTM;
	BrushMatrix34 modRefFrameInverse = worldRefTM.GetInverted() * worldTM;

	if (editMode == eEditModeMove)
		return modRefFrame * BrushMatrix34::CreateTranslationMat(worldRefTM.GetInverted().TransformVector(vOffset)) * modRefFrameInverse;
	else if (editMode == eEditModeRotate)
		return modRefFrame * BrushMatrix34::CreateRotationXYZ(Ang3_tpl<BrushFloat>(-vOffset)) * modRefFrameInverse;
	else if (editMode == eEditModeScale)
		return modRefFrame * BrushMatrix34::CreateScale(vOffset) * modRefFrameInverse;

	return BrushMatrix34::CreateIdentity();
}

void MessageBox(const QString& title, const QString& msg)
{
	CQuestionDialog::SWarning(title, msg);
}

}

