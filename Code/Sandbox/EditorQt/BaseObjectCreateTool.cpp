// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "BaseObjectCreateTool.h"

#include <QEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>

IMPLEMENT_DYNAMIC(CBaseObjectCreateTool, CEditTool)

CBaseObjectCreateTool::CBaseObjectCreateTool()
{}

CBaseObjectCreateTool::~CBaseObjectCreateTool()
{}

bool CBaseObjectCreateTool::MouseCallback(CViewport* pView, EMouseEvent eventId, CPoint& point, int flags)
{
	switch (eventId)
	{
	case eMouseFocusEnter:
		StartCreation(pView, point);
		break;

	case eMouseFocusLeave:
		CancelCreation(false);
		break;

	case eMouseMove:
		if (!ObjectWasCreated())
		{
			StartCreation(pView, point);
			break;
		}

	// Intentional fallthrough
	default:
		{
			GetIEditorImpl()->GetIUndoManager()->Suspend();
			const auto callbackResult = ObjectMouseCallback(pView, eventId, point, flags);
			GetIEditorImpl()->GetIUndoManager()->Resume();

			if (callbackResult == MOUSECREATE_ABORT)
			{
				CancelCreation();
			}
			else if (callbackResult == MOUSECREATE_OK)
			{
				FinishCreation(flags & MK_CONTROL);
			}
			return true; // handled
		}
		break;
	}

	return false; // not handled
}

bool CBaseObjectCreateTool::OnDragEvent(CViewport* pView, EDragEvent eventId, QEvent* pEvent, uint32 flags)
{
	switch (eventId)
	{
	case eDragEnter:
		{
			auto pDragEnter = static_cast<QDragEnterEvent*>(pEvent);
			if (!IsValidDragData(pDragEnter->mimeData(), true))
			{
				pEvent->ignore();
				return false;
			}
			CPoint point(pDragEnter->pos().x(), pDragEnter->pos().y());
			StartCreation(pView, point);
		}
		break;

	case eDragLeave:
		{
			CancelCreation(false);
		}
		break;

	case eDragMove:
		{
			auto pDragMove = static_cast<QDragMoveEvent*>(pEvent);
			assert(ObjectWasCreated());
			GetIEditorImpl()->GetIUndoManager()->Suspend();
			CPoint point(pDragMove->pos().x(), pDragMove->pos().y());
			ObjectMouseCallback(pView, eMouseMove, point, flags);
			GetIEditorImpl()->GetIUndoManager()->Resume();
		}
		break;

	case eDrop:
		{
			auto pDrop = static_cast<QDropEvent*>(pEvent);
			if (IsValidDragData(pDrop->mimeData(), false))
			{
				pDrop->acceptProposedAction();
				FinishCreation(flags & MK_CONTROL);
			}
		}
		break;

	default:
		assert(0);
		break;
	}

	pEvent->accept();
	return true;
}

bool CBaseObjectCreateTool::OnKeyDown(CViewport* pView, uint32 key, uint32, uint32)
{
	if (key == Qt::Key_Escape || key == Qt::Key_Delete)
	{
		Abort();
	}
	return true;
}

void CBaseObjectCreateTool::FinishCreation(bool restart, CViewport* pView, const CPoint& point)
{
	FinishObjectCreation();

	//Keep creating if CTRL is held
	if (restart)
	{
		StartCreation(pView, point); //restart the process
	}
	else
	{
		Abort();
	}
}

