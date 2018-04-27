// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVMappingTool.h"
#include "DesignerEditor.h"
#include "Core/Model.h"
#include "Viewport.h"
#include "Util/Undo.h"
#include "Objects/DesignerObject.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include <CrySerialization/Math.h>
#include <CrySerialization/Enum.h>
#include "ToolFactory.h"
#include "Core/UVIslandManager.h"
#include "Util/UVUnwrapUtil.h"
#include "Core/Helper.h"
#include "DesignerSession.h"
#include "Gizmos/ITransformManipulator.h"

using Serialization::ActionButton;

namespace Designer
{
void UVMappingParameter::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("Advanced", "Advanced"))
	{
		ar(ActionButton([this] { m_pTool->OpenUVMappingWnd();
		                }), "UV Mapping Editor", "Open UV Mapping Editor");
		ar.closeBlock();
	}

	if (ar.openBlock("Transform", "Transform"))
	{
		ar(m_UVOffset, "UVOffset", "Offset");
		ar(m_ScaleOffset, "ScaleOffset", "Scale");
		ar(Serialization::Range(m_Rotate, 0.0f, 360.0f), "Rotate", "Rotate");
		ar.closeBlock();
	}

	if (ar.openBlock("Alignment", "Alignment"))
	{
		ar(m_TilingXY, "Tiling", "Tiling");
		ar(ActionButton(std::bind(&UVMappingTool::OnFitTexture, m_pTool)), "Fit", "^Fit");
		ar(ActionButton(std::bind(&UVMappingTool::OnReset, m_pTool)), "Reset", "^Reset");
		ar.closeBlock();
	}

	if (ar.openBlock("SubMaterial", "Sub Material"))
	{
		ar(ActionButton(std::bind(&UVMappingTool::OnSelectPolygons, m_pTool)), "Select", "^Select");
		ar(ActionButton(std::bind(&UVMappingTool::OnAssignSubMatID, m_pTool)), "Assign", "^Assign");
		ar.closeBlock();
	}

	if (ar.openBlock("CopyPaste", "Copy/Paste"))
	{
		ar(ActionButton(std::bind(&UVMappingTool::OnCopy, m_pTool)), "Copy", "^Copy");
		ar(ActionButton(std::bind(&UVMappingTool::OnPaste, m_pTool)), "Paste", "^Paste");
		ar.closeBlock();
	}
}

TexInfo ConvertToTexInfo(const UVMappingParameter& texParam)
{
	TexInfo texInfo;
	texInfo.shift[0] = texParam.m_UVOffset.x;
	texInfo.shift[1] = texParam.m_UVOffset.y;
	texInfo.scale[0] = texParam.m_ScaleOffset.x;
	texInfo.scale[1] = texParam.m_ScaleOffset.y;
	texInfo.rotate = texParam.m_Rotate;
	return texInfo;
}

UVMappingParameter ConvertToTextureMappingParam(const TexInfo& texInfo)
{
	UVMappingParameter texParam;
	texParam.m_UVOffset.x = texInfo.shift[0];
	texParam.m_UVOffset.y = texInfo.shift[1];
	texParam.m_ScaleOffset.x = texInfo.scale[0];
	texParam.m_ScaleOffset.y = texInfo.scale[1];
	texParam.m_Rotate = texInfo.rotate;
	return texParam;
}

void UVMappingTool::Enter()
{
	__super::Enter();

	FillSecondShelfWithSelectedElements();
	int nCurrentEditMode = GetIEditor()->GetEditMode();

	GetIEditor()->SetEditMode(eEditModeMove);
	GetIEditor()->SetReferenceCoordSys(COORDS_WORLD);
	GetIEditor()->SetEditMode(eEditModeScale);
	GetIEditor()->SetReferenceCoordSys(COORDS_WORLD);
	GetIEditor()->SetEditMode(nCurrentEditMode);

	if (GetIEditor()->GetEditMode() == eEditModeRotate)
	{
		GetIEditor()->SetReferenceCoordSys(COORDS_LOCAL);
	}

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	GetDesigner()->UpdateTMManipulatorBasedOnElements(pSelected);
}

void UVMappingTool::Leave()
{
	if (GetModel())
	{
		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
		GetModel()->SetShelf(eShelf_Base);
		ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SyncPrefab | ePostProcess_Mirror);
	}
	__super::Leave();
}

void UVMappingTool::OpenUVMappingWnd()
{
	GetIEditor()->ExecuteCommand("general.open_pane '%s'", ToolName());
}

bool UVMappingTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	bool bOnlyIncludeCube = GetKeyState(VK_SPACE) & (1 << 15);
	ElementSet pickedElements;

	if (!GetModel()->IsEmpty(eShelf_Construction))
	{
		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
		ApplyPostProcess(ePostProcess_Mesh);
	}

	pickedElements.PickFromModel(GetBaseObject(), GetModel(), view, point, m_nPickFlag, bOnlyIncludeCube, NULL);

	if (!pickedElements.IsEmpty() && pickedElements[0].IsPolygon() && pickedElements[0].m_pPolygon)
	{
		UpdatePanel(pickedElements[0].m_pPolygon->GetTexInfo());
		DesignerSession::GetInstance()->SetCurrentSubMatID(pickedElements[0].m_pPolygon->GetSubMatID());
	}

	return __super::OnLButtonDown(view, nFlags, point);
}

bool UVMappingTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	__super::OnLButtonUp(view, nFlags, point);

	FillSecondShelfWithSelectedElements();

	return true;
}

void UVMappingTool::FillSecondShelfWithSelectedElements()
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	bool bWillUpdate = false;

	if (!GetModel()->IsEmpty(eShelf_Construction))
	{
		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
		bWillUpdate = true;
	}

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	for (int i = 0, iCount(pSelected->GetCount()); i < iCount; ++i)
	{
		if (!(*pSelected)[i].IsPolygon() || (*pSelected)[i].m_pPolygon == NULL)
			continue;
		GetModel()->MovePolygonBetweenShelves((*pSelected)[i].m_pPolygon, eShelf_Base, eShelf_Construction);
		bWillUpdate = true;
	}

	if (bWillUpdate)
		ApplyPostProcess(ePostProcess_Mesh);
}

bool UVMappingTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
		return GetDesigner()->EndCurrentDesignerTool();
	return true;
}

bool UVMappingTool::QueryPolygon(const BrushRay& ray, int& nOutPolygonIndex, bool& bOutNew) const
{
	bOutNew = false;
	GetModel()->SetShelf(eShelf_Construction);
	if (!GetModel()->QueryPolygon(ray, nOutPolygonIndex))
	{
		GetModel()->SetShelf(eShelf_Base);
		if (!GetModel()->QueryPolygon(ray, nOutPolygonIndex))
			return false;
		bOutNew = true;
	}
	return true;
}

void UVMappingTool::ApplyTextureInfo(const TexInfo& texInfo, int nModifiedParts)
{
	CUndo undo("Change Texture Info of a designer");

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	for (int i = 0, iCount(pSelected->GetCount()); i < iCount; ++i)
	{
		if (!(*pSelected)[i].m_pPolygon)
			continue;
		SetTexInfoToPolygon((*pSelected)[i].m_pPolygon, texInfo, nModifiedParts);
	}
}

void UVMappingTool::FitTexture(float fTileU, float fTileV)
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();

	for (int i = 0, iCount(pSelected->GetCount()); i < iCount; ++i)
	{
		if (!(*pSelected)[i].m_pPolygon)
			continue;

		const AABB& polygonAABB = (*pSelected)[i].m_pPolygon->GetBoundBox();
		TexInfo texInfo;
		UnwrapUtil::FitTexture(ToFloatPlane((*pSelected)[i].m_pPolygon->GetPlane()), polygonAABB.min, polygonAABB.max, fTileU, fTileV, texInfo);
		SetTexInfoToPolygon((*pSelected)[i].m_pPolygon, texInfo, eTexParam_All);

		if (i == 0)
			UpdatePanel(texInfo);
	}
}

void UVMappingTool::SelectPolygonsBySubMatID(int subMatID)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Clear();

	for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		GetModel()->SetShelf(eShelf_Base);
		PolygonPtr pPolygon = GetModel()->GetPolygon(i);
		if (pPolygon == NULL)
			continue;
		if (pPolygon->GetSubMatID() == subMatID)
		{
			GetModel()->SetShelf(eShelf_Construction);
			GetModel()->AddPolygon(pPolygon, false);
			Element element;
			element.SetPolygon(GetBaseObject(), pPolygon);
			pSelected->Add(element);
		}
	}

	GetModel()->SetShelf(eShelf_Base);
	for (int i = 0, iSelectionCount(pSelected->GetCount()); i < iSelectionCount; ++i)
		GetModel()->RemovePolygon((*pSelected)[i].m_pPolygon);

	DesignerSession::GetInstance()->UpdateSelectionMeshFromSelectedElements();
}

bool UVMappingTool::GetTexInfoOfSelectedPolygon(TexInfo& outTexInfo) const
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	if (pSelected->IsEmpty())
		return false;
	if (!(*pSelected)[0].m_pPolygon)
		return false;
	outTexInfo = (*pSelected)[0].m_pPolygon->GetTexInfo();
	return true;
}

void UVMappingTool::SetTexInfoToPolygon(PolygonPtr pPolygon, const TexInfo& texInfo, int nModifiedParts)
{
	const TexInfo& polyTexInfo = pPolygon->GetTexInfo();

	TexInfo newTexInfo;

	newTexInfo.shift[0] = nModifiedParts & eTexParam_Offset ? texInfo.shift[0] : polyTexInfo.shift[0];
	newTexInfo.shift[1] = nModifiedParts & eTexParam_Offset ? texInfo.shift[1] : polyTexInfo.shift[1];

	newTexInfo.scale[0] = nModifiedParts & eTexParam_Scale ? texInfo.scale[0] : polyTexInfo.scale[0];
	newTexInfo.scale[1] = nModifiedParts & eTexParam_Scale ? texInfo.scale[1] : polyTexInfo.scale[1];

	newTexInfo.rotate = nModifiedParts & eTexParam_Rotate ? texInfo.rotate : polyTexInfo.rotate;

	if (pPolygon->CheckFlags(ePolyFlag_Mirrored))
	{
		TexInfo mirroredTexInfo(newTexInfo);
		mirroredTexInfo.rotate = -mirroredTexInfo.rotate;
		pPolygon->SetTexInfo(mirroredTexInfo);
	}
	else
	{
		pPolygon->SetTexInfo(newTexInfo);
	}
}

void UVMappingTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginSceneSave:
		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
		ApplyPostProcess(ePostProcess_Mesh);
		break;
	case eNotify_OnEndSceneSave:
		FillSecondShelfWithSelectedElements();
		break;
	}
}

void UVMappingTool::OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& p0, BrushVec3 value, int nFlags)
{
	if (m_MouseDownContext.m_UVInfos.empty())
		return;

	BrushMatrix34 offsetTM;
	offsetTM = GetOffsetTM(pManipulator, value, GetWorldTM());

	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	int editMode = GetIEditor()->GetEditMode();
	if (editMode == eEditModeMove)
	{
		Vec3 vTranslation = offsetTM.GetTranslation();
		BrushVec3 vNormal(0, 0, 0);

		for (int i = 0, iCount(m_MouseDownContext.m_UVInfos.size()); i < iCount; ++i)
		{
			PolygonPtr pPolygon(m_MouseDownContext.m_UVInfos[i].first);
			TexInfo texInfo(m_MouseDownContext.m_UVInfos[i].second);

			SBrushPlane<float> p(ToVec3(pPolygon->GetPlane().Normal()), ToFloat(pPolygon->GetPlane().Distance()));
			Vec3 basis_u, basis_v;
			UnwrapUtil::CalcTextureBasis(p, basis_u, basis_v);

			BrushFloat tu = (BrushFloat)basis_u.Dot(-vTranslation);
			BrushFloat tv = (BrushFloat)basis_v.Dot(-vTranslation);

			texInfo.shift[0] = texInfo.shift[0] + tu;
			texInfo.shift[1] = texInfo.shift[1] + tv;

			SetTexInfoToPolygon(pPolygon, texInfo, eTexParam_Offset);
			UpdatePanel(texInfo);

			vNormal += pPolygon->GetPlane().Normal();
		}

		CompileShelf(eShelf_Construction);
		GetDesigner()->UpdateTMManipulator(m_MouseDownContext.m_MouseDownPos + vTranslation, vNormal.GetNormalized());
	}
	else if (editMode == eEditModeRotate)
	{
		for (int i = 0, iCount(m_MouseDownContext.m_UVInfos.size()); i < iCount; ++i)
		{
			PolygonPtr pPolygon(m_MouseDownContext.m_UVInfos[i].first);
			TexInfo texInfo(m_MouseDownContext.m_UVInfos[i].second);

			BrushVec3 tn = pPolygon->GetPlane().Normal();
			if (std::abs(tn.x) > std::abs(tn.y) && std::abs(tn.x) > std::abs(tn.z))
				tn.y = tn.z = 0;
			else if (std::abs(tn.y) > std::abs(tn.x) && std::abs(tn.y) > std::abs(tn.z))
				tn.x = tn.z = 0;
			else
				tn.x = tn.y = 0;

			float fDeltaRotation = (180.0F / PI) * value.x;
			if (tn.y < 0 || tn.x > 0 || tn.z > 0)
				fDeltaRotation = -fDeltaRotation;

			texInfo.rotate -= fDeltaRotation;
			SetTexInfoToPolygon(pPolygon, texInfo, eTexParam_Rotate);
			UpdatePanel(texInfo);
		}
		CompileShelf(eShelf_Construction);
	}
	else if (editMode == eEditModeScale)
	{
		Vec3 vScale(offsetTM.m00, offsetTM.m11, offsetTM.m22);
		for (int i = 0, iCount(m_MouseDownContext.m_UVInfos.size()); i < iCount; ++i)
		{
			PolygonPtr pPolygon(m_MouseDownContext.m_UVInfos[i].first);
			TexInfo texInfo(m_MouseDownContext.m_UVInfos[i].second);

			SBrushPlane<float> p(ToVec3(pPolygon->GetPlane().Normal()), ToFloat(pPolygon->GetPlane().Distance()));
			Vec3 basis_u, basis_v;
			float fBackupRotate = texInfo.rotate;
			texInfo.rotate = 0;
			UnwrapUtil::CalcTextureBasis(p, basis_u, basis_v);
			texInfo.rotate = fBackupRotate;

			BrushFloat tu = std::abs((BrushFloat)basis_u.Dot(vScale));
			BrushFloat tv = std::abs((BrushFloat)basis_v.Dot(vScale));

			texInfo.scale[0] += tu - 1;
			texInfo.scale[1] += tv - 1;

			if (texInfo.scale[0] < 0.01f)
				texInfo.scale[0] = 0.01f;
			if (texInfo.scale[1] < 0.01f)
				texInfo.scale[1] = 0.01f;

			SetTexInfoToPolygon(pPolygon, texInfo, eTexParam_Scale);
			UpdatePanel(texInfo);
		}
		CompileShelf(eShelf_Construction);
	}
}

void UVMappingTool::OnManipulatorBegin(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& point, int flags)
{
	m_bHitGizmo = true;

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();

	std::vector<PolygonPtr> polygons;
	for (int i = 0, iSelectionCount(pSelected->GetCount()); i < iSelectionCount; ++i)
	{
		if ((*pSelected)[i].IsPolygon() && (*pSelected)[i].m_pPolygon)
			polygons.push_back((*pSelected)[i].m_pPolygon);
	}
	if (polygons.empty())
		return;

	m_MouseDownContext.Init();
	m_MouseDownContext.m_MouseDownPos = GetWorldTM().GetInverted().TransformPoint(ToBrushVec3(pManipulator->GetTransform().GetTranslation()));
	for (int i = 0, iPolygonCount(polygons.size()); i < iPolygonCount; ++i)
		m_MouseDownContext.m_UVInfos.push_back(std::pair<PolygonPtr, TexInfo>(polygons[i], polygons[i]->GetTexInfo()));
}

void UVMappingTool::OnManipulatorEnd(IDisplayViewport* pView, ITransformManipulator* pManipulator)
{
	m_bHitGizmo = false;

	m_MouseDownContext.Init();
}

void UVMappingTool::RecordTextureMappingUndo(const char* sUndoDescription) const
{
	if (CUndo::IsRecording() && GetBaseObject())
		CUndo::Record(new UndoTextureMapping(GetBaseObject(), sUndoDescription));
}

void UVMappingTool::ShowGizmo()
{
	GetDesigner()->UpdateTMManipulatorBasedOnElements(DesignerSession::GetInstance()->GetSelectedElements());
}

void UVMappingTool::UpdatePanel(const TexInfo& texInfo)
{
	UVMappingParameter texParam = ConvertToTextureMappingParam(texInfo);
	texParam.m_TilingXY = m_UVParameter.m_TilingXY;
	texParam.m_pTool = this;
	m_UVParameter = texParam;
	m_PrevUVParameter = m_UVParameter;
	DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
}

void UVMappingTool::OnFitTexture()
{
	CUndo undo("Fit TextureUV");
	RecordTextureMappingUndo("Fit TextureUV");
	FitTexture(m_UVParameter.m_TilingXY.x, m_UVParameter.m_TilingXY.x);
	CompileShelf(eShelf_Construction);
}

void UVMappingTool::OnReset()
{
	TexInfo texInfo;
	CUndo undo("Reset TextureUV");
	RecordTextureMappingUndo("Reset TextureUV");
	ApplyTextureInfo(texInfo, eTexParam_All);
	CompileShelf(eShelf_Construction);
	UpdatePanel(texInfo);
}

void UVMappingTool::OnSelectPolygons()
{
	SelectPolygonsBySubMatID(DesignerSession::GetInstance()->GetCurrentSubMatID());
}

void UVMappingTool::OnAssignSubMatID()
{
	CUndo undo("Assign Material ID");
	RecordTextureMappingUndo("Assign Material ID");
	{
		MODEL_SHELF_RECONSTRUCTOR(GetModel());
		GetModel()->SetShelf(eShelf_Base);
		AssignMatIDToSelectedPolygons(DesignerSession::GetInstance()->GetCurrentSubMatID());
	}
	CompileShelf(eShelf_Construction);
	if (GetModel()->CheckFlag(eModelFlag_Mirror))
		CompileShelf(eShelf_Base);
}

void UVMappingTool::OnCopy()
{
	m_CopiedUVMappingParameter = m_UVParameter;
}

void UVMappingTool::OnPaste()
{
	m_UVParameter = m_CopiedUVMappingParameter;
	TexInfo texInfo = ConvertToTexInfo(m_UVParameter);
	ApplyTextureInfo(texInfo, eTexParam_All);
	UpdatePanel(texInfo);
	CompileShelf(eShelf_Construction);
}

void UVMappingTool::OnChangeParameter(bool continuous)
{
	TexInfo texInfo = ConvertToTexInfo(m_UVParameter);

	int nModifiedParts = 0;

	if (!Comparison::IsEquivalent(m_UVParameter.m_UVOffset, m_PrevUVParameter.m_UVOffset))
		nModifiedParts |= eTexParam_Offset;

	if (!Comparison::IsEquivalent(m_UVParameter.m_ScaleOffset, m_PrevUVParameter.m_ScaleOffset))
		nModifiedParts |= eTexParam_Scale;

	if (m_UVParameter.m_Rotate != m_PrevUVParameter.m_Rotate)
		nModifiedParts |= eTexParam_Rotate;

	ApplyTextureInfo(texInfo, nModifiedParts);
	CompileShelf(eShelf_Construction);

	m_PrevUVParameter = m_UVParameter;
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_UVMapping, eToolGroup_Surface, "UV Mapping", UVMappingTool,
                                                           uvmapping, "runs uvmapping tool", "designer.uvmapping")

