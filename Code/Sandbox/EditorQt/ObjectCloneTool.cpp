// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectCloneTool.h"
#include "Viewport.h"
#include "Grid.h"

IMPLEMENT_DYNCREATE(CObjectCloneTool, CEditTool)

CObjectCloneTool::CObjectCloneTool()
{
	m_bSetConstrPlane = true;
	//m_bSetCapture = false;

	GetIEditorImpl()->GetIUndoManager()->Begin();
	m_selection = 0;
	if (!GetIEditorImpl()->GetSelection()->IsEmpty())
	{
		CWaitCursor wait;
		CloneSelection();
		m_selection = GetIEditorImpl()->GetSelection();
		GetIEditorImpl()->GetIUndoManager()->Suspend();
	}
	GetIEditor()->GetObjectManager()->signalSelectionChanged.Connect(this, &CObjectCloneTool::OnSelectionChanged);
}

void CObjectCloneTool::OnSelectionChanged()
{
	GetIEditor()->GetObjectManager()->signalSelectionChanged.DisconnectObject(this);
	Abort();
}

CObjectCloneTool::~CObjectCloneTool()
{
	GetIEditor()->GetObjectManager()->signalSelectionChanged.DisconnectObject(this);

	IUndoManager* pUndoManager = GetIEditorImpl()->GetIUndoManager();
	if (pUndoManager->IsUndoSuspended())
	{
		pUndoManager->Resume();
	}

	if (pUndoManager->IsUndoRecording())
	{
		pUndoManager->Accept("Clone");
	}
}

void CObjectCloneTool::CloneSelection()
{
	CSelectionGroup selObjects;
	CSelectionGroup sel;

	const CSelectionGroup* currSelection = GetIEditorImpl()->GetSelection();
	currSelection->FilterParents();

	std::vector<CBaseObject*> newObjects;
	newObjects.reserve(currSelection->GetFilteredCount());

	CObjectCloneContext cloneContext;

	auto pObjMan = GetIEditor()->GetObjectManager();

	// Clone every object.
	for (int i = 0; i < currSelection->GetFilteredCount(); i++)
	{
		CBaseObject* pFromObject = currSelection->GetFilteredObject(i);
		CBaseObject* newObj = pObjMan->CloneObject(pFromObject);
		if (!newObj) // can be null, e.g. sequence can't be cloned
		{
			continue;
		}

		cloneContext.AddClone(pFromObject, newObj);
		newObjects.push_back(newObj);
	}

	// Only after everything was cloned, call PostClone on all cloned objects.
	//Copy objects map as it can be invalidated during PostClone
	auto objectsMap = cloneContext.m_objectsMap;
	for (auto it : objectsMap)
	{
		CBaseObject* pFromObject = it.first;
		CBaseObject* pClonedObject = it.second;
		if (pClonedObject)
			pClonedObject->PostClone(pFromObject, cloneContext);
	}

	GetIEditorImpl()->ClearSelection();
	GetIEditorImpl()->SelectObjects(newObjects);
}

void CObjectCloneTool::SetConstrPlane(CViewport* view, CPoint point)
{
	Matrix34 originTM;
	originTM.SetIdentity();
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (!pSelection->GetManipulatorMatrix(GetIEditorImpl()->GetReferenceCoordSys(), originTM))
		originTM.SetIdentity();

	m_initPosition = pSelection->GetCenter();

	view->SetConstructionMatrix(originTM);
}

void CObjectCloneTool::Display(DisplayContext& dc)
{
	//dc.SetColor( 1,1,0,1 );
	//dc.DrawBall( gP1,1.1f );
	//dc.DrawBall( gP2,1.1f );
}

bool CObjectCloneTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (m_selection)
	{
		// Set construction plane origin to selection origin.
		if (m_bSetConstrPlane)
		{
			SetConstrPlane(view, point);
			GetIEditorImpl()->GetSelection()->FilterParents();
			GetIEditorImpl()->GetSelection()->SaveFilteredTransform();
			m_bSetConstrPlane = false;
		}

		if (event == eMouseLDown)
		{
			// Accept group.
			Accept();
			GetIEditorImpl()->GetSelection()->FinishChanges();
			return true;
		}
		if (event == eMouseMove)
		{
			// Move selection.
			const CSelectionGroup* selection = GetIEditorImpl()->GetSelection();
			if (!selection->IsEmpty())
			{
				bool followTerrain = false;

				int axis = GetIEditorImpl()->GetAxisConstrains();
				
				Vec3 p1 = m_initPosition;
				Vec3 p2 = view->MapViewToCP(point);
				if (p2.IsZero())
					return true;

				Vec3 offset = view->GetCPVector(p1, p2);

				offset = view->SnapToGrid(offset);
				
				int selectionFlags = CSelectionGroup::eMS_None;
				if (GetIEditorImpl()->IsSnapToTerrainEnabled())
					selectionFlags = CSelectionGroup::eMS_FollowTerrain;

				if (GetIEditorImpl()->IsSnapToGeometryEnabled())
					selectionFlags |= CSelectionGroup::eMS_FollowGeometry;

				if (GetIEditorImpl()->IsSnapToNormalEnabled())
					selectionFlags |= CSelectionGroup::eMS_SnapToNormal;

				GetIEditorImpl()->GetSelection()->Move(offset, selectionFlags, GetIEditorImpl()->GetReferenceCoordSys(), point, true);
			}
		}
		if (event == eMouseWheel)
		{
			const CSelectionGroup* selection = GetIEditorImpl()->GetSelection();
			if (!selection->IsEmpty())
			{
				double angle = 1;

				if (gSnappingPreferences.angleSnappingEnabled())
					angle = gSnappingPreferences.angleSnap();

				for (int i = 0; i < selection->GetCount(); ++i)
				{
					CBaseObject* pObj = selection->GetFilteredObject(i);
					Quat rot = pObj->GetRotation();
					rot.SetRotationXYZ(Ang3(0, 0, rot.GetRotZ() + DEG2RAD(point.x > 0 ? angle * (-1) : angle)));
					pObj->SetRotation(rot);
				}
				GetIEditorImpl()->GetIUndoManager()->Accept("Rotate Selection");
			}
		}
	}
	return true;
}

void CObjectCloneTool::Abort()
{
	GetIEditorImpl()->GetIUndoManager()->Resume();
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();

	CEditTool::Abort();
}

void CObjectCloneTool::Accept()
{
	GetIEditorImpl()->GetIUndoManager()->Resume();
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Accept("Clone");

	GetIEditorImpl()->SetEditTool(0);
}

bool CObjectCloneTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		Abort();
	}
	return false;
}

