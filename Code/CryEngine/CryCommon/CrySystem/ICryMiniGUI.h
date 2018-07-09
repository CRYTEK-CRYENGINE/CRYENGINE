// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ICryMiniGUI.h
//  Created:     26/08/2009 by Timur.
//  Description: Interface to the Mini GUI subsystem
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ICryMiniGUI_h__
#define __ICryMiniGUI_h__

#include <CryCore/smartptr.h>
#include <CryMath/Cry_Color.h>
#include <CryExtension/ICryUnknown.h>

namespace minigui
{
struct IMiniCtrl;
class CDrawContext;

//! Rectangle class.
struct Rect
{
	float left;
	float top;
	float right;
	float bottom;

	Rect() : left(0), top(0), right(0), bottom(0) {}
	Rect(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
	Rect(const Rect& rc) { left = rc.left; top = rc.top; right = rc.right; bottom = rc.bottom; }
	bool  IsPointInside(float x, float y) const { return x >= left && x <= right && y >= top && y <= bottom; }
	float Width() const                         { return right - left; }
	float Height() const                        { return bottom - top; }
};

typedef void (* ClickCallback)(void* data, bool onOff);
typedef void (* RenderCallback)(float x, float y);

enum EMiniCtrlStatus
{
	eCtrl_Hidden          = BIT(0),    //!< Control is hidden.
	eCtrl_Highlight       = BIT(1),    //!< Control is highlight (probably mouse over).
	eCtrl_Focus           = BIT(2),    //!< Control have focus (from keyboard).
	eCtrl_Checked         = BIT(3),    //!< Control have checked mark.
	eCtrl_NoBorder        = BIT(4),    //!< Control have no border.
	eCtrl_CheckButton     = BIT(5),    //!< Button control behave as a check button.
	eCtrl_TextAlignCentre = BIT(6),    //!< Draw text aligned centre.
	eCtrl_AutoResize      = BIT(7),    //!< Auto resize depending on text length.
	eCtrl_Moveable        = BIT(8),    //!< Dynamically reposition ctrl.
	eCtrl_CloseButton     = BIT(9),    //!< Control has close button.
};
enum EMiniCtrlEvent
{
	eCtrlEvent_LButtonDown    = BIT(0),
	eCtrlEvent_LButtonUp      = BIT(1),
	eCtrlEvent_LButtonPressed = BIT(2),
	eCtrlEvent_MouseOver      = BIT(3),
	eCtrlEvent_MouseOff       = BIT(4),
	eCtrlEvent_DPadLeft       = BIT(5),
	eCtrlEvent_DPadRight      = BIT(6),
	eCtrlEvent_DPadUp         = BIT(7),
	eCtrlEvent_DPadDown       = BIT(8),
};

//! Types of the supported controls.
enum EMiniCtrlType
{
	eCtrlType_Unknown = 0,
	eCtrlType_Button,
	eCtrlType_Menu,
	eCtrlType_InfoBox,
	eCtrlType_Table,
};

struct SMetrics
{
	float fTextSize;
	float fTitleSize;

	// Colors.
	ColorB clrFrameBorder;
	ColorB clrFrameBorderHighlight;
	ColorB clrFrameBorderOutOfFocus;
	ColorB clrChecked;
	ColorB clrBackground;
	ColorB clrBackgroundHighlight;
	ColorB clrBackgroundSelected;
	ColorB clrTitle;
	ColorB clrText;
	ColorB clrTextSelected;

	uint8  outOfFocusAlpha;
};

enum ECommand
{
	eCommand_ButtonPress,
	eCommand_ButtonChecked,
	eCommand_ButtonUnchecked,
};
//! Command sent from the control.
struct SCommand
{
	ECommand   command;
	IMiniCtrl* pCtrl;
	int        nCtrlID;
};

// Event listener interface for the MiniGUI.
struct IMiniGUIEventListener
{
	// <interfuscator:shuffle>
	virtual ~IMiniGUIEventListener(){}
	virtual void OnCommand(SCommand& cmd) = 0;
	// </interfuscator:shuffle>
};

//! Interface to the GUI.
struct IMiniGUI : public ICryUnknown
{
public:
	CRYINTERFACE_DECLARE_GUID(IMiniGUI, "ea09d342-6881-4f2a-af10-34e04b076011"_cry_guid);

	// <interfuscator:shuffle>
	virtual void      Init() = 0;
	virtual void      Done() = 0;
	virtual void      Draw() = 0;
	virtual void      Reset() = 0;

	virtual void      SaveState() = 0;
	virtual void      RestoreState() = 0;

	virtual void      SetEnabled(bool status) = 0;
	virtual void      SetInFocus(bool status) = 0;
	virtual bool      InFocus() = 0;

	virtual void      SetEventListener(IMiniGUIEventListener* pListener) = 0;

	virtual SMetrics& Metrics() = 0;

	//! Makes a new control.
	virtual IMiniCtrl* CreateCtrl(IMiniCtrl* pParentCtrl, int nCtrlID, EMiniCtrlType type, int nCtrlFlags, const Rect& rc, const char* title) = 0;

	//! Remove all controls.
	virtual void       RemoveAllCtrl() = 0;

	virtual void       OnCommand(SCommand& cmd) = 0;

	virtual IMiniCtrl* GetCtrlFromPoint(float x, float y) const = 0;

	virtual void       SetMovingCtrl(IMiniCtrl* pCtrl) = 0;
	// </interfuscator:shuffle>
};

DECLARE_SHARED_POINTERS(IMiniGUI);

struct IMiniCtrl : public _reference_target_t
{
	// <interfuscator:shuffle>
	virtual void Reset() = 0;

	virtual void SaveState() = 0;
	virtual void RestoreState() = 0;

	//! For system call only.
	virtual void          SetGUI(IMiniGUI* pGUI) = 0;
	virtual IMiniGUI*     GetGUI() const = 0;

	virtual EMiniCtrlType GetType() const = 0;

	virtual int           GetId() const = 0;
	virtual void          SetId(int id) = 0;

	virtual const char*   GetTitle() const = 0;
	virtual void          SetTitle(const char* title) = 0;

	virtual Rect          GetRect() const = 0;
	virtual void          SetRect(const Rect& rc) = 0;

	virtual void          SetFlag(uint32 flag) = 0;
	virtual void          ClearFlag(uint32 flag) = 0;
	virtual bool          CheckFlag(uint32 flag) const = 0;

	//! Sub Controls handling.
	virtual void       AddSubCtrl(IMiniCtrl* pCtrl) = 0;
	virtual void       RemoveSubCtrl(IMiniCtrl* pCtrl) = 0;
	virtual void       RemoveAllSubCtrl() = 0;
	virtual int        GetSubCtrlCount() const = 0;
	virtual IMiniCtrl* GetSubCtrl(int nIndex) const = 0;
	virtual IMiniCtrl* GetParent() const = 0;

	//! Check if point is inside any of the sub controls.
	virtual IMiniCtrl* GetCtrlFromPoint(float x, float y) = 0;

	virtual void       OnPaint(CDrawContext& dc) = 0;

	virtual void       SetVisible(bool state) = 0;

	//! Events from GUI.
	virtual void OnEvent(float x, float y, EMiniCtrlEvent) {};

	//! When set, this control will be enabling/disabling specified cvar.
	//! When button not checked fOffValue will be set on cvar, when checked fOnValue will be set.
	virtual bool SetControlCVar(const char* sCVarName, float fOffValue, float fOnValue) = 0;

	virtual bool SetClickCallback(ClickCallback callback, void* pCallbackData) = 0;

	virtual bool SetRenderCallback(RenderCallback callback) = 0;

	virtual bool SetConnectedCtrl(IMiniCtrl* pConnectedCtrl) = 0;

	//! Resize text box based what text is present.
	virtual void AutoResize() = 0;

	//! Create close 'X' button for control.
	virtual void CreateCloseButton() = 0;

	//! Move control.
	virtual void Move(float x, float y) = 0;
	// </interfuscator:shuffle>
};
typedef _smart_ptr<IMiniCtrl> IMiniCtrlPtr;

class IMiniGuiCommon
{
public:
	// <interfuscator:shuffle>
	virtual ~IMiniGuiCommon(){}
	virtual bool IsHidden() = 0;
	virtual void Hide(bool stat) = 0;
	// </interfuscator:shuffle>
};

class IMiniTable : public IMiniGuiCommon
{
public:
	// <interfuscator:shuffle>
	virtual int  AddColumn(const char* name) = 0;
	virtual void RemoveColumns() = 0;
	virtual int  AddData(int columnIndex, ColorB col, const char* format, ...) = 0;
	virtual void ClearTable() = 0;
	// </interfuscator:shuffle>
};

class IMiniInfoBox : public IMiniGuiCommon
{
public:
	// <interfuscator:shuffle>
	virtual void SetTextIndent(float x) = 0;
	virtual void SetTextSize(float sz) = 0;
	virtual void ClearEntries() = 0;
	virtual void AddEntry(const char* str, ColorB col, float textSize) = 0;
	// </interfuscator:shuffle>
};
}

#define MINIGUI_BEGIN namespace minigui {
#define MINIGUI_END   }

#endif //__ICryMiniGUI_h__
