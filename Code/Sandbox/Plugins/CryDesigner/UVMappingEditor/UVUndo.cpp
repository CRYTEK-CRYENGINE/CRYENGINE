// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVUndo.h"
#include "Core/UVIslandManager.h"
#include "UVMappingEditor.h"
#include "UVCluster.h"
#include "Util/ElementSet.h"

namespace Designer {
namespace UVMapping
{

MainContext GetMainContext(CryGUID objGuid)
{
	MainContext mc;

	CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(objGuid);
	if (pObj == NULL || !pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
		return mc;

	mc.pObject = pObj;
	mc.pModel = ((DesignerObject*)pObj)->GetModel();
	mc.pCompiler = ((DesignerObject*)pObj)->GetCompiler();

	return mc;
}

UVIslandUndo::UVIslandUndo()
{
	m_ObjGUID = DesignerSession::GetInstance()->GetBaseObject()->GetId();
	Record(m_ObjGUID, m_undoData);
}

void UVIslandUndo::Undo(bool bUndo)
{
	if (bUndo)
		Record(m_ObjGUID, m_redoData);

	Apply(m_undoData);
}

void UVIslandUndo::Redo()
{
	Apply(m_redoData);
}

void UVIslandUndo::Record(CryGUID& objGUID, UndoData& undoData)
{
	MainContext mc(GetMainContext(objGUID));
	if (!mc.IsValid())
		return;

	undoData.pUVIslandMgr = mc.pModel->GetUVIslandMgr()->Clone();
	undoData.subMatID = GetUVEditor() ? GetUVEditor()->GetSubMatID() : DesignerSession::GetInstance()->GetCurrentSubMatID();
}

void UVIslandUndo::Apply(const UndoData& undoData)
{
	MainContext mc = GetMainContext(m_ObjGUID);
	if (!mc.IsValid())
		return;

	UVIslandManager* pUVIslandMgr = mc.pModel->GetUVIslandMgr();
	*pUVIslandMgr = *undoData.pUVIslandMgr;

	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		pUVIsland->ResetPolygonsInModel(mc.pModel);
	}

	if (GetUVEditor())
		GetUVEditor()->SetSubMatID(undoData.subMatID);
	ApplyPostProcess(mc);
}

UVSelectionUndo::UVSelectionUndo()
{
	m_ObjGUID = DesignerSession::GetInstance()->GetBaseObject()->GetId();

	UVElementSetPtr pUVElementPtr = GetUVEditor()->GetElementSet();
	m_UndoSelections = pUVElementPtr->Clone();
}

void UVSelectionUndo::Undo(bool bUndo)
{
	if (!GetUVEditor())
		return;

	UVElementSetPtr pUVElementPtr = GetUVEditor()->GetElementSet();

	if (bUndo)
		m_RedoSelections = pUVElementPtr->Clone();

	Apply(m_UndoSelections);
}

void UVSelectionUndo::Redo()
{
	if (!GetUVEditor())
		return;

	Apply(m_RedoSelections);
}

void UVSelectionUndo::Apply(UVElementSetPtr pElementSet)
{
	if (!GetUVEditor())
		return;

	UVElementSetPtr pUVElementPtr = GetUVEditor()->GetElementSet();
	RefreshUVIslands(pElementSet);
	pUVElementPtr->CopyFrom(pElementSet);

	int nSubMatID = GetUVEditor()->GetSubMatID();
	if (!pUVElementPtr->IsEmpty())
		nSubMatID = pUVElementPtr->GetElement(0).m_pUVIsland->GetSubMatID();

	SyncSelection();
	GetUVEditor()->UpdateGizmoPos();
}

void UVSelectionUndo::RefreshUVIslands(UVElementSetPtr pElementSet)
{
	if (!GetUVEditor())
		return;
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	for (int i = 0, iCount(pElementSet->GetCount()); i < iCount; ++i)
	{
		UVElement& element = pElementSet->GetElement(i);
		UVIslandPtr pUVIsland = pUVIslandMgr->FindUVIsland(element.m_pUVIsland->GetGUID());
		if (pUVIsland)
		{
			element.m_pUVIsland = pUVIsland;
			for (int k = 0, iVertexCount(element.m_UVVertices.size()); k < iVertexCount; ++k)
				element.m_UVVertices[k].pUVIsland = pUVIsland;
		}
	}
}

UVMoveUndo::UVMoveUndo()
{
	CBaseObject* pObj = DesignerSession::GetInstance()->GetBaseObject();
	if (pObj)
	{
		m_ObjGUID = pObj->GetId();
		GatherInvolvedPolygons(m_UndoPolygons);
	}
}

void UVMoveUndo::GatherInvolvedPolygons(std::vector<PolygonPtr>& outInvolvedPolygons)
{
	std::set<PolygonPtr> involvedPolygons;
	UVCluster cluster;
	cluster.GetPolygons(involvedPolygons);

	auto iter = involvedPolygons.begin();
	for (; iter != involvedPolygons.end(); ++iter)
		outInvolvedPolygons.push_back(new Polygon(**iter));
}

void UVMoveUndo::GatherInvolvedPolygons(std::vector<PolygonPtr>& involvedPolygons, std::vector<PolygonPtr>& outInvolvedPolygons)
{
	MainContext mc(GetMainContext(m_ObjGUID));

	auto iter = involvedPolygons.begin();
	for (; iter != involvedPolygons.end(); ++iter)
	{
		PolygonPtr polygon = mc.pModel->QueryPolygon((*iter)->GetGUID());
		if (polygon)
			outInvolvedPolygons.push_back(new Polygon(*polygon));
	}
}

void UVMoveUndo::ApplyInvolvedPolygons(std::vector<PolygonPtr>& involvedPolygons)
{
	MainContext mc(GetMainContext(m_ObjGUID));
	if (!mc.IsValid())
		return;

	int nSubMatID = GetUVEditor() ? GetUVEditor()->GetSubMatID() : 0;

	auto iter = involvedPolygons.begin();
	for (; iter != involvedPolygons.end(); ++iter)
	{
		PolygonPtr polygon = mc.pModel->QueryPolygon((*iter)->GetGUID());
		if (polygon)
		{
			*polygon = **iter;
			nSubMatID = polygon->GetSubMatID();
		}
	}

	if (GetUVEditor())
	{
		GetUVEditor()->GetUVIslandMgr()->Invalidate();
		GetUVEditor()->SetSubMatID(nSubMatID);
	}

	ApplyPostProcess(GetMainContext(m_ObjGUID));
}

void UVMoveUndo::Undo(bool bUndo)
{
	if (bUndo)
		GatherInvolvedPolygons(m_UndoPolygons, m_RedoPolygons);

	ApplyInvolvedPolygons(m_UndoPolygons);
	if (GetUVEditor())
		GetUVEditor()->UpdateGizmoPos();
}

void UVMoveUndo::Redo()
{
	ApplyInvolvedPolygons(m_RedoPolygons);
	if (GetUVEditor())
		GetUVEditor()->UpdateGizmoPos();
}

UVProjectionUndo::UVProjectionUndo()
{
	m_ObjGUID = DesignerSession::GetInstance()->GetBaseObject()->GetId();
	SavePolygons(m_UndoPolygons);
}

void UVProjectionUndo::Undo(bool bUndo)
{
	if (bUndo)
		SavePolygons(m_RedoPolygons);

	UndoPolygons(m_UndoPolygons);
	ApplyPostProcess(GetMainContext(m_ObjGUID));
}

void UVProjectionUndo::Redo()
{
	UndoPolygons(m_RedoPolygons);
	ApplyPostProcess(GetMainContext(m_ObjGUID));
}

void UVProjectionUndo::SavePolygons(std::vector<PolygonPtr>& polygons)
{
	ElementSet* pSelectedElement = DesignerSession::GetInstance()->GetSelectedElements();
	for (int i = 0, iCount(pSelectedElement->GetCount()); i < iCount; ++i)
	{
		const Element& element = pSelectedElement->Get(i);
		if (element.m_pPolygon)
			polygons.push_back(new Polygon(*element.m_pPolygon));
	}
}

void UVProjectionUndo::UndoPolygons(const std::vector<PolygonPtr>& polygons)
{
	MainContext mc = GetMainContext(m_ObjGUID);
	auto ii = polygons.begin();
	for (; ii != polygons.end(); ++ii)
	{
		PolygonPtr polygonInModel = mc.pModel->QueryPolygon((*ii)->GetGUID());
		if (polygonInModel)
			*polygonInModel = **ii;
	}
}

}
}

