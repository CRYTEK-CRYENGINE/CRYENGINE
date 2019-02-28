// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Undo.h"
#include "Core/Polygon.h"
#include "Core/Helper.h"
#include "Objects/AreaSolidObject.h"
#include "Objects/DesignerObject.h"
#include "DesignerEditor.h"
#include "Tools/BaseTool.h"
#include "ToolFactory.h"
#include "DesignerSession.h"

namespace Designer
{

bool DesignerUndo::IsAKindOfDesignerTool(CEditTool* pEditor)
{
	return pEditor && pEditor->IsKindOf(RUNTIME_CLASS(DesignerEditor));
}

DesignerUndo::DesignerUndo(CBaseObject* pObj, const Model* pModel, const char* undoDescription) :
	m_ObjGUID(pObj->GetId()),
	m_Tool(eDesigner_Invalid)
{
	DESIGNER_ASSERT(pModel);
	if (pModel)
	{
		if (undoDescription)
			m_undoDescription = undoDescription;
		else
			m_undoDescription = "CryDesignerUndo";

		StoreEditorTool();

		m_UndoWorldTM = pObj->GetWorldTM();
		m_undo = new Model(*pModel);
	}
}

void DesignerUndo::StoreEditorTool()
{
	CEditTool* pEditor = GetIEditor()->GetEditTool();
	if (!IsAKindOfDesignerTool(pEditor))
		m_Tool = eDesigner_Invalid;
	else
	{
		BaseTool* pSubTool = ((DesignerEditor*)pEditor)->GetCurrentTool();
		if (pSubTool)
			m_Tool = pSubTool->Tool();
		else
			m_Tool = eDesigner_Invalid;
	}
}

void DesignerUndo::Undo(bool bUndo)
{
	MainContext mc(GetMainContext());
	if (!mc.IsValid())
		return;

	if (bUndo)
	{
		m_RedoWorldTM = mc.pObject->GetWorldTM();
		m_redo = new Model(*mc.pModel);
		RestoreEditTool(mc.pModel);
	}

	if (m_undo)
	{
		*mc.pModel = *m_undo;
		mc.pObject->SetWorldTM(m_UndoWorldTM);
		ApplyPostProcess(mc);
		m_undo = NULL;
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_PolygonsModified, nullptr);
	}
}

void DesignerUndo::Redo()
{
	MainContext mc(GetMainContext());
	if (!mc.IsValid())
		return;

	m_UndoWorldTM = mc.pObject->GetWorldTM();
	m_undo = new Model(*mc.pModel);

	RestoreEditTool(mc.pModel);

	if (m_redo)
	{
		*mc.pModel = *m_redo;
		mc.pObject->SetWorldTM(m_RedoWorldTM);
		ApplyPostProcess(GetMainContext());
		m_redo = NULL;
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_PolygonsModified, nullptr);
	}
}

CBaseObject* DesignerUndo::GetBaseObject(const CryGUID& objGUID)
{
	CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(objGUID);
	if (pObj == NULL)
		return NULL;
	return pObj;
}

MainContext DesignerUndo::GetMainContext(const CryGUID& objGUID)
{
	CBaseObject* pObj = GetBaseObject(objGUID);
	ModelCompiler* pCompiler = NULL;
	Designer::GetCompiler(pObj, pCompiler);
	return MainContext(pObj, pCompiler, GetModel(objGUID));
}

Model* DesignerUndo::GetModel(const CryGUID& objGUID)
{
	CBaseObject* pObj = GetBaseObject(objGUID);
	if (pObj == NULL)
		return NULL;
	Model* pModel = NULL;
	Designer::GetModel(pObj, pModel);
	return pModel;
}

ModelCompiler* DesignerUndo::GetCompiler(const CryGUID& objGUID)
{
	CBaseObject* pObj = GetBaseObject(objGUID);
	if (pObj == NULL)
		return NULL;
	ModelCompiler* pCompiler = NULL;
	Designer::GetCompiler(pObj, pCompiler);
	return pCompiler;
}

void DesignerUndo::RestoreEditTool(Model* pModel, CryGUID objGUID, EDesignerTool tool)
{
	CBaseObject* pSelectedObj = GetBaseObject(objGUID);
	if (!pSelectedObj)
		return;

	bool bSelected(pSelectedObj->IsSelected());
	if (!bSelected)
	{
		GetIEditor()->GetObjectManager()->ClearSelection();
		GetIEditor()->GetObjectManager()->SelectObject(pSelectedObj);
	}

	CEditTool* pEditor = GetIEditor()->GetEditTool();
	if (pEditor && IsAKindOfDesignerTool(pEditor))
	{
		// if it's a non-modal tool...
		if (tool == eDesigner_Merge ||
		    tool == eDesigner_Weld ||
		    tool == eDesigner_Pivot ||
		    tool == eDesigner_Pivot2Bottom ||
		    tool == eDesigner_ResetXForm ||
		    tool == eDesigner_SnapToGrid)
		{
			// TODO: yup. those are non modal tools, just don't restore anything
			tool = eDesigner_Invalid;
			return;
		}

		DesignerEditor* pDesignerTool = (DesignerEditor*)pEditor;
		DesignerSession* pSession = DesignerSession::GetInstance();
		pSession->SetBaseObject(pSelectedObj);
		pDesignerTool->SwitchTool(tool);
	}
}

UndoSelection::UndoSelection(ElementSet& selectionContext, CBaseObject* pObj, bool bRetoreToolAfterUndo, const char* undoDescription)
{
	SetObjGUID(pObj->GetId());

	if (undoDescription)
		SetDescription(undoDescription);
	else
		SetDescription(selectionContext.GetElementsInfoText());

	CopyElements(selectionContext, m_SelectionContextForUndo);

	int nDesignerMode = 0;

	for (int i = 0, iSelectionCount(selectionContext.GetCount()); i < iSelectionCount; ++i)
	{
		if (selectionContext[i].IsVertex())
			nDesignerMode |= eDesigner_Vertex;
		else if (selectionContext[i].IsEdge())
			nDesignerMode |= eDesigner_Edge;
		else if (selectionContext[i].IsPolygon())
			nDesignerMode |= eDesigner_Polygon;
	}

	// basically we should quit instead of setting mode to invalid
	if (nDesignerMode == 0)
		nDesignerMode = eDesigner_Invalid;

	m_bRestoreToolAfterUndo = bRetoreToolAfterUndo;

	SwitchTool((EDesignerTool)nDesignerMode);
}

void UndoSelection::Undo(bool bUndo)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	MainContext mc(GetMainContext());
	if (m_bRestoreToolAfterUndo)
		RestoreEditTool(mc.pModel);

	ElementSet* pSelected = pSession->GetSelectedElements();
	if (bUndo)
	{
		if (pSelected)
			CopyElements(*pSelected, m_SelectionContextForRedo);
	}

	pSession->ClearPolygonSelections();

	ReplacePolygonsWithExistingPolygonsInModel(m_SelectionContextForUndo);
	if (pSelected)
		pSelected->Set(m_SelectionContextForUndo);

	pSession->UpdateSelectionMeshFromSelectedElements();
	if (GetDesigner())
	{
		GetDesigner()->UpdateTMManipulatorBasedOnElements(pSelected);
	}
	pSession->signalDesignerEvent(eDesignerNotify_Select, nullptr);
	UpdateDrawnEdges(pSession->GetMainContext());
}

void UndoSelection::Redo()
{
	MainContext mc(GetMainContext());
	if (m_bRestoreToolAfterUndo)
		RestoreEditTool(mc.pModel);

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();

	pSession->ClearPolygonSelections();
	ReplacePolygonsWithExistingPolygonsInModel(m_SelectionContextForRedo);
	if (pSelected)
		pSelected->Set(m_SelectionContextForRedo);

	pSession->UpdateSelectionMeshFromSelectedElements();
	if (GetDesigner())
	{
		GetDesigner()->UpdateTMManipulatorBasedOnElements(pSelected);
	}
	pSession->signalDesignerEvent(eDesignerNotify_Select, nullptr);
	UpdateDrawnEdges(pSession->GetMainContext());
}

void UndoSelection::CopyElements(ElementSet& sourceElements, ElementSet& destElements)
{
	destElements = sourceElements;
	for (int i = 0, iElementSize(destElements.GetCount()); i < iElementSize; ++i)
	{
		if (!destElements[i].IsPolygon())
			continue;
		destElements[i].m_pPolygon = destElements[i].m_pPolygon->Clone();
	}
}

void UndoSelection::ReplacePolygonsWithExistingPolygonsInModel(ElementSet& elements)
{
	MainContext mc(GetMainContext());
	std::vector<Element> removedElements;

	for (int i = 0, iCount(elements.GetCount()); i < iCount; ++i)
	{
		if (!elements[i].IsPolygon())
			continue;

		if (elements[i].m_pPolygon == NULL)
			continue;

		PolygonPtr pEquivalentPolygon = mc.pModel->QueryEquivalentPolygon(elements[i].m_pPolygon);
		if (pEquivalentPolygon == NULL)
		{
			removedElements.push_back(elements[i]);
			continue;
		}

		elements[i].m_pPolygon = pEquivalentPolygon;
	}

	for (int i = 0, iRemovedCount(removedElements.size()); i < iRemovedCount; ++i)
		elements.Erase(removedElements[i]);
}

void UndoTextureMapping::Undo(bool bUndo)
{
	Model* pModel = DesignerUndo::GetModel(m_ObjGUID);
	if (pModel == NULL)
		return;
	if (bUndo)
		SaveDesignerTexInfoContext(m_RedoContext);
	RestoreTexInfo(m_UndoContext);
	DesignerUndo::RestoreEditTool(pModel, m_ObjGUID, eDesigner_UVMapping);
}

void UndoTextureMapping::Redo()
{
	Model* pModel = DesignerUndo::GetModel(m_ObjGUID);
	if (pModel == NULL)
		return;
	SaveDesignerTexInfoContext(m_UndoContext);
	RestoreTexInfo(m_RedoContext);
	DesignerUndo::RestoreEditTool(pModel, m_ObjGUID, eDesigner_UVMapping);
}

void UndoTextureMapping::RestoreTexInfo(const std::vector<ContextInfo>& contextList)
{
	Model* pModel = DesignerUndo::GetModel(m_ObjGUID);
	if (pModel == NULL)
		return;

	MODEL_SHELF_RECONSTRUCTOR(pModel);

	for (int i = 0, iContextCount(contextList.size()); i < iContextCount; ++i)
	{
		const ContextInfo& context = contextList[i];
		PolygonPtr pPolygon = pModel->QueryPolygon(context.m_PolygonGUID);
		if (pPolygon == NULL)
			continue;
		pPolygon->SetTexInfo(context.m_TexInfo);
		pPolygon->SetSubMatID(context.m_MatID);
	}

	CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(m_ObjGUID);
	if (pObj && pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
	{
		DesignerObject* pDesignerObject = ((DesignerObject*)pObj);
		ApplyPostProcess(pDesignerObject->GetMainContext(), ePostProcess_Mesh | ePostProcess_SyncPrefab);
	}
}

void UndoTextureMapping::SaveDesignerTexInfoContext(std::vector<ContextInfo>& contextList)
{
	Model* pModel(DesignerUndo::GetModel(m_ObjGUID));

	if (!pModel)
		return;

	MODEL_SHELF_RECONSTRUCTOR(pModel);

	contextList.clear();

	for (int k = eShelf_Base; k < cShelfMax; ++k)
	{
		pModel->SetShelf(static_cast<ShelfID>(k));
		for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr pPolygon = pModel->GetPolygon(i);
			if (pPolygon == NULL)
				continue;
			ContextInfo context;
			context.m_PolygonGUID = pPolygon->GetGUID();
			context.m_MatID = pPolygon->GetSubMatID();
			context.m_TexInfo = pPolygon->GetTexInfo();
			contextList.push_back(context);
		}
	}
}

UndoSubdivision::UndoSubdivision(const MainContext& mc)
{
	m_ObjGUID = mc.pObject->GetId();
	m_SubdivisionLevelForUndo = mc.pModel->GetSubdivisionLevel();
	m_SubdivisionLevelForRedo = 0;
}

void UndoSubdivision::UpdatePanel()
{
	DesignerEditor* pDesignerTool = GetDesigner();
	if (pDesignerTool && pDesignerTool->GetCurrentTool())
	{
		BaseTool* pSubTool = pDesignerTool->GetCurrentTool();
		if (pSubTool->Tool() == eDesigner_Subdivision)
		{
			DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
		}
	}
}

void UndoSubdivision::Undo(bool bUndo)
{
	MainContext mc(DesignerUndo::GetMainContext(m_ObjGUID));
	if (mc.pModel == NULL)
		return;

	if (bUndo)
		m_SubdivisionLevelForRedo = mc.pModel->GetSubdivisionLevel();

	mc.pModel->SetSubdivisionLevel(m_SubdivisionLevelForUndo);
	UpdatePanel();

	if (!mc.pObject || !mc.pCompiler)
		return;

	ApplyPostProcess(mc, ePostProcess_Mesh);
}

void UndoSubdivision::Redo()
{
	MainContext mc(DesignerUndo::GetMainContext(m_ObjGUID));
	if (mc.pModel == NULL)
		return;

	mc.pModel->SetSubdivisionLevel(m_SubdivisionLevelForRedo);
	UpdatePanel();

	if (!mc.pObject || !mc.pCompiler)
		return;

	ApplyPostProcess(mc, ePostProcess_Mesh);
}

};

