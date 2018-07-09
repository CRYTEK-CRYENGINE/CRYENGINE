// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "VegetationPlaceTool.h"

#include "VegetationObject.h"
#include "VegetationMap.h"
#include "VegetationDragDropData.h"
#include "Viewport.h"

IMPLEMENT_DYNCREATE(CVegetationPlaceTool, CBaseObjectCreateTool)

class CVegetationPlaceTool_ClassDesc : public IClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.VegetationPlace"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char*    Category()        { return "Terrain"; };

	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVegetationPlaceTool); }
};

REGISTER_CLASS_DESC(CVegetationPlaceTool_ClassDesc);

CVegetationPlaceTool::CVegetationPlaceTool()
	: m_pSelectedObject(nullptr)
	, m_pCreatedInstance(nullptr)
{}

CVegetationPlaceTool::~CVegetationPlaceTool()
{
	CancelCreation(false);
}

void CVegetationPlaceTool::SelectObjectToCreate(CVegetationObject* pSelectedObject)
{
	if (!CanStartCreation())
	{
		CancelCreation();
	}

	if (m_pSelectedObject != pSelectedObject)
	{
		CancelCreation(false);
		m_pSelectedObject = pSelectedObject;
		// no StartCreation() - creation will start on mouse focus
	}
}

int CVegetationPlaceTool::ObjectMouseCallback(CViewport* pView, EMouseEvent eventId, const CPoint& point, uint32 flags)
{
	if (m_pCreatedInstance && (eventId == eMouseMove || eventId == eMouseLDown))
	{
		auto pObject = m_pCreatedInstance->object;
		const auto bIsAffectedByBrushes = pObject->IsAffectedByBrushes();
		const auto newPos = pView->ViewToWorld(point, 0, !bIsAffectedByBrushes, bIsAffectedByBrushes);

		auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
		pVegetationMap->MoveInstance(m_pCreatedInstance, newPos);

		if (eventId == eMouseLDown)
		{
			return MOUSECREATE_OK;
		}
	}
	return MOUSECREATE_CONTINUE;
}

bool CVegetationPlaceTool::CanStartCreation()
{
	auto pEditor = GetIEditorImpl();
	return pEditor->GetVegetationMap() && pEditor->IsDocumentReady();
}

void CVegetationPlaceTool::StartCreation(CViewport* pView, const CPoint& point)
{
	if (!m_pSelectedObject)
	{
		return;
	}

	CWaitCursor wait;
	GetIEditorImpl()->GetIUndoManager()->Begin();

	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	// remove temporarily created object
	if (m_pCreatedInstance)
	{
		pVegetationMap->DeleteObjInstance(m_pCreatedInstance);
	}

	auto pos = Vec3(1, 1, 1);
	if (pView)
	{
		const auto bIsAffectedByBrushes = m_pSelectedObject->IsAffectedByBrushes();
		pos = pView->ViewToWorld(point, 0, !bIsAffectedByBrushes, bIsAffectedByBrushes);
	}

	m_pCreatedInstance = pVegetationMap->PlaceObjectInstance(pos, m_pSelectedObject);
}

void CVegetationPlaceTool::FinishObjectCreation()
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
	{
		GetIEditorImpl()->GetIUndoManager()->Accept(string("New Vegetation Object"));
	}
	m_pCreatedInstance = nullptr; //we don't own the created object anymore, vegetation map does
}

void CVegetationPlaceTool::CancelCreation(bool clear)
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
	{
		GetIEditorImpl()->GetIUndoManager()->Cancel();
		// cancel undo performs rolls back last creation of the object instance
		m_pCreatedInstance = nullptr;
	}

	if (clear)
	{
		m_pSelectedObject = nullptr;
	}

	if (m_pCreatedInstance)
	{
		auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
		pVegetationMap->DeleteObjInstance(m_pCreatedInstance);
		m_pCreatedInstance = nullptr;
	}
}

bool CVegetationPlaceTool::IsValidDragData(const QMimeData* pData, bool acceptValue)
{
	auto pDragDropData = qobject_cast<const CVegetationDragDropData*>(pData);
	if (pDragDropData->HasObjectListData())
	{
		const auto byteArray = pDragDropData->GetObjectListData();
		QDataStream stream(byteArray);
		QVector<int> objectIds;
		stream >> objectIds;

		if (objectIds.size() == 1)
		{
			if (acceptValue)
			{
				auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
				m_pSelectedObject = pVegetationMap->GetObjectById(*objectIds.cbegin());
			}
			return true;
		}
	}

	return false;
}

bool CVegetationPlaceTool::ObjectWasCreated() const
{
	return m_pCreatedInstance;
}

