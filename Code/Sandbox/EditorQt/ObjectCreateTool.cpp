// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectCreateTool.h"
#include "Viewport.h"
#include "ViewManager.h"

#include "EditMode/ObjectMode.h"
#include "Objects/ObjectLayerManager.h"

#include "DragDrop.h"

#include "QEvent.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CObjectCreateTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CObjectCreateTool_ClassDesc : public IClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.ObjectCreate2"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char*    Category()        { return "Object"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CObjectCreateTool); }
	//////////////////////////////////////////////////////////////////////////
};

REGISTER_CLASS_DESC(CObjectCreateTool_ClassDesc)

//////////////////////////////////////////////////////////////////////////

CObjectCreateTool::CObjectCreateTool()
	: m_objectFile(nullptr)
	, m_objectClassDesc(nullptr)
	, m_createdObject(nullptr)
	, m_bStartedCreation(false)
	, m_bContinuousCreation(false)
{
}

CObjectCreateTool::~CObjectCreateTool()
{
	CancelCreation(false);
}

bool CObjectCreateTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	switch (event)
	{
	case eMouseFocusLeave:
		CancelCreation(false);
		break;
	case eMouseEnter:
		// Mouse enter events are dispatched immediately. There are cases in which an enter frame event will be
		// fired & received before the previous has been digested. StartCreation will eventually clear the selection,
		// which will trigger the inspector to repopulate, which will hide the widgets and then fire an Enter Frame event on
		// the widget that is currently under the mouse (viewport) which will eventually call this function again with an enter frame event.
		// This used to be a problem because the object itself hadn't been returned by the time we got the second event and would proceed
		// to create another object.
		if (m_objectClassDesc && !m_bStartedCreation)
		{
			StartCreation();
		}
		break;
	case eMouseLeave:
		if (!m_bContinuousCreation)
			CancelCreation(false);
		break;
	case eMouseLDown:
		m_bContinuousCreation = true;
	// intentional fallthrough
	default:
		if (m_createdObject)
		{
			GetIEditorImpl()->GetIUndoManager()->Suspend();
			int res = m_createdObject->MouseCreateCallback(view, event, point, flags);
			GetIEditorImpl()->GetIUndoManager()->Resume();

			if (res == MOUSECREATE_ABORT)
			{
				CancelCreation();
			}
			else if (res == MOUSECREATE_OK)
			{
				FinishCreation(flags & MK_CONTROL);
			}

			return true; // handled
		}
		break;
	}

	return false; // not handled, fallback to parent
}

bool CObjectCreateTool::IsValidDragData(const QMimeData* data, bool acceptValue)
{
	const CDragDropData* dragDropData = CDragDropData::FromMimeData(data);
	if (dragDropData->HasCustomData("EngineFilePaths"))
	{
		//TODO: better encapsulation of drag data, too many internals are required right now
		QByteArray byteArray = dragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);
		QStringList list;
		stream >> list;

		if (list.count() == 1)
		{
			if (acceptValue)
			{
				m_objectFile = list[0].toStdString().c_str();
			}
			return true;
		}
	}

	return false;
}

bool CObjectCreateTool::OnDragEvent(CViewport* view, EDragEvent eventId, QEvent* event, uint32 flags)
{
	switch (eventId)
	{
	case eDragEnter:
		{
			QDragEnterEvent* dragEnter = static_cast<QDragEnterEvent*>(event);
			if (IsValidDragData(dragEnter->mimeData(), true))
			{
				CDragDropData::ShowDragText(view->GetViewWidget(), QObject::tr("Create object"));
				dragEnter->accept();
				StartCreation();
				return IsCreating();
			}
		}
		break;
	case eDragLeave:
		{
			if(IsCreating())
			{
				QDragLeaveEvent* dragLeave = static_cast<QDragLeaveEvent*>(event);
				dragLeave->accept();
				CancelCreation(false);
				return true;
			}
		}
		break;
	case eDragMove:
		{
			if(IsCreating())
			{
				QDragMoveEvent* dragMove = static_cast<QDragMoveEvent*>(event);
				CRY_ASSERT(m_createdObject);
				GetIEditorImpl()->GetIUndoManager()->Suspend();
				CPoint point(dragMove->pos().x(), dragMove->pos().y());
				int res = m_createdObject->MouseCreateCallback(view, eMouseMove, point, flags);
				GetIEditorImpl()->GetIUndoManager()->Resume();
				dragMove->accept();
				return true;
			}
		}
		break;
	case eDrop:
		{
			QDropEvent* drop = static_cast<QDropEvent*>(event);
			if (IsValidDragData(drop->mimeData(), false))
			{
				auto action = drop->proposedAction();
				drop->acceptProposedAction();
				FinishCreation(flags & MK_CONTROL);
				drop->accept();
				return true;
			}
		}
		break;
	default:
		assert(0);
		break;
	}

	return false;
}

bool CObjectCreateTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape || nChar == Qt::Key_Delete)
	{
		CancelCreation();
	}
	return false;
}

void CObjectCreateTool::SelectObjectToCreate(const char* objectClassName, const char* filepath /*= nullptr*/)
{
	CObjectClassDesc* clsDesc = GetIEditorImpl()->GetObjectManager()->FindClass(objectClassName);
	if (clsDesc)
		SelectObjectToCreate(clsDesc, filepath);
}

void CObjectCreateTool::SelectObjectToCreate(CObjectClassDesc* objectType, const char* filepath)
{
	if (!CanStartCreation())
		CancelCreation();

	if (objectType != m_objectClassDesc || (m_objectFile && !filepath) || m_objectFile.compare(filepath) != 0)
	{
		CancelCreation(false);

		m_objectClassDesc = objectType;
		m_objectFile = filepath;

		//creation will start on mouse focus on the viewport
	}
}

void CObjectCreateTool::StartCreation()
{
	m_bStartedCreation = true;
	assert(m_objectClassDesc);

	GetIEditorImpl()->GetIUndoManager()->Begin();

	m_createdObject = GetIEditorImpl()->NewObject(m_objectClassDesc->ClassName(), m_objectFile, true);  //Note: very strange API, sometimes file will be empty string anyway
	if(!m_createdObject)
		CancelCreation();
}

void CObjectCreateTool::FinishCreation(bool restart)
{
	m_bContinuousCreation = false;
	m_bStartedCreation = false;
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
	{
		GetIEditorImpl()->GetIUndoManager()->Accept(string("New ") + m_createdObject->GetTypeName());
		m_createdObject = nullptr; //we don't own the created object anymore
	}

	//Keep placing if CTRL is held
	if (restart)
	{
		StartCreation();//restart the process
	}
	else
	{
		Abort();
	}
}

void CObjectCreateTool::CancelCreation(bool clear /*= true*/)
{
	m_bContinuousCreation = false;
	m_bStartedCreation = false;
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();

	if (clear)
	{
		m_objectFile = "";
		m_objectClassDesc = nullptr;
	}

	if (m_createdObject)
	{
		GetIEditorImpl()->DeleteObject(m_createdObject);
		m_createdObject = nullptr;
	}
}

bool CObjectCreateTool::CanStartCreation()
{
	return GetIEditorImpl()->GetObjectManager()->GetLayersManager() && GetIEditorImpl()->GetObjectManager()->CanCreateObject();
}

