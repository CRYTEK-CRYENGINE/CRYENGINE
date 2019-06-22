// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <CrySandbox/CrySignal.h>
#include <CrySerialization/IArchive.h>
#include <IEditor.h>

class CViewport;
class QEvent;
enum EOperationMode;
struct IClassDesc;
struct ITransformManipulator;
struct SDisplayContext;
struct HitContext;

/*!
 *	CEditTool is an abstract base class for All Editing	Tools supported by Editor.
 *	Edit tools handle specific editing modes in viewports.
 */
class EDITOR_COMMON_API CEditTool : public CObject
{
public:
	DECLARE_DYNAMIC(CEditTool);

	CEditTool();

	//////////////////////////////////////////////////////////////////////////
	// For reference counting.
	//////////////////////////////////////////////////////////////////////////
	void                   AddRef()  { m_nRefCount++; }
	void                   Release() { if (--m_nRefCount <= 0) DeleteThis(); }

	virtual EOperationMode GetMode() { return eOperationModeNone; }
	virtual string         GetDisplayName() const = 0;

	// Abort tool.
	virtual void Abort();
	//! Used to pass user defined data to edit tool from ToolButton.
	virtual void SetUserData(const char* key, void* userData) {}

	//! Called whenever the tool is made the active edit tool
	virtual void Activate() {}

	// Called each frame to display tool for given viewport.
	virtual void Display(SDisplayContext& dc) {}

	//! Mouse callback sent from viewport.
	//! Returns true if event processed by callback, and all other processing for this event should abort.
	//! Return false if event was not processed by callback, and other processing for this event should occur.
	//! @param view Viewport that sent this callback.
	//! @param event Indicate what kind of event occurred in viewport.
	//! @param point 2D coordinate in viewport where event occurred.
	//! @param flags Additional flags (MK_LBUTTON,etc..) or from (MouseEventFlags) specified by viewport when calling callback.
	virtual bool MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags) = 0;

	//! Called when key in viewport is pressed while using this tool.
	//! Returns true if event processed by callback, and all other processing for this event should abort.
	//! Returns false if event was not processed by callback, and other processing for this event should occur.
	//! @param view Viewport where key was pressed.
	//! @param nChar Specifies the virtual key code of the given key. For a list of standard virtual key codes, see Winuser.h
	//! @param nRepCnt Specifies the repeat count, that is, the number of times the keystroke is repeated as a result of the user holding down the key.
	//! @param nFlags	Specifies the scan code, key-transition code, previous key state, and context code, (see WM_KEYDOWN)
	virtual bool OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) { return false; }

	//! Called when key in viewport is released while using this tool.
	//! Returns true if event processed by callback, and all other processing for this event should abort.
	//! Returns false if event was not processed by callback, and other processing for this event should occur.
	//! @param view Viewport where key was pressed.
	//! @param nChar Specifies the virtual key code of the given key. For a list of standard virtual key codes, see Winuser.h
	//! @param nRepCnt Specifies the repeat count, that is, the number of times the keystroke is repeated as a result of the user holding down the key.
	//! @param nFlags	Specifies the scan code, key-transition code, previous key state, and context code, (see WM_KEYDOWN)
	virtual bool OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) { return false; }

	//! Called when a drag is made over the viewport
	//! Returns true if event processed by callback, and all other processing for this event should abort.
	//! Returns false if event was not processed by callback, and other processing for this event should occur.
	//! @param view Viewport where key was pressed.
	//! @param eventId Type of drag event
	//! @param event QEvent object from Qt. Use static_cast to get the derived event type.
	virtual bool OnDragEvent(CViewport* view, EDragEvent eventId, QEvent* event, uint32 flags) { return false; }

	virtual bool IsNeedMoveTool()                                                              { return false; }
	virtual bool IsNeedToSkipPivotBoxForObjects()                                              { return false; }
	virtual bool IsDisplayGrid()                                                               { return true; }

	// Draws object specific helpers for this tool
	virtual void DrawObjectHelpers(CBaseObject* pObject, SDisplayContext& dc) {}

	// Hit test against edit tool
	virtual bool HitTest(CBaseObject* pObject, HitContext& hc) { return false; }

	virtual void Serialize(Serialization::IArchive& ar)        {}

	CCrySignal<void(CEditTool*)> signalPropertiesChanged;

protected:
	virtual ~CEditTool() {}
	//////////////////////////////////////////////////////////////////////////
	// Delete edit tool.
	//////////////////////////////////////////////////////////////////////////
	virtual void DeleteThis() = 0;

protected:
	int m_nRefCount;
};
