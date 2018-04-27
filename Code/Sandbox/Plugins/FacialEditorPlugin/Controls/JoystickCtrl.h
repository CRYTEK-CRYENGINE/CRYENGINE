// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __JOYSTICKCTRL_H__
#define __JOYSTICKCTRL_H__

#include <CryInput/IJoystick.h>
#include "ScrollableWindow.h"

class IJoystickCtrlContainer
{
public:
	enum JoystickAction
	{
		JOYSTICK_ACTION_START,
		JOYSTICK_ACTION_END
	};

	virtual void              OnAction(JoystickAction action) = 0;
	virtual void              OnFreezeLayoutChanged() = 0;
	virtual IJoystickChannel* GetPotentialJoystickChannel() = 0;
	virtual float             GetCurrentEvaluationTime() = 0;
	virtual float             GetMaxEvaluationTime() = 0;
	virtual void              OnSplineChanged() = 0;
	virtual void              OnJoysticksChanged() = 0;
	virtual void              OnBeginDraggingJoystickKnob(IJoystick* pJoystick) = 0;
	virtual void              OnJoystickSelected(IJoystick* pJoystick, bool exclusive) = 0;
	virtual bool              GetPlaying() const = 0;
	virtual void              SetPlaying(bool playing) = 0;
};

class IJoystickActionMode;

class IJoystickActionContext
{
public:
	virtual void BeginUndo() = 0;
	virtual void AcceptUndo(const char* name) = 0;
	virtual void RestoreUndo() = 0;
	virtual void StoreSplineUndo(IJoystick* pJoystick) = 0;
	virtual void StoreJoystickUndo() = 0;
	virtual void JoysticksChanged() = 0;
	virtual void TerminateActionMode() = 0;
	virtual void ReleaseMouseCapture() = 0;
	virtual void SetJoystickBeingManipulated(IJoystick* pJoystickBeingManipulated) = 0;
	virtual void SetJoystickBeingManipulatedPosition(const Vec2& vJoystickBeingManipulatedPosition) = 0;
};

class IJoystickUndoContext
{
public:
	virtual void OnSplineChangesUnOrReDone() = 0;
	virtual void OnJoytickChangesUnOrReDone() = 0;
	virtual void SerializeJoystickSet(IJoystickSet* pJoystickSet, XmlNodeRef node, bool bLoading) = 0;
};

class IJoystickMiddleMouseButtonHandlerContext
{
public:
	virtual Vec2 GetPosition() const = 0;
	virtual void SetPosition(const Vec2& position) = 0;
};

class JoystickMiddleMouseButtonHandler;

class CJoystickCtrl : public CScrollableWindow, public IJoystickActionContext, public IJoystickUndoContext, public IJoystickMiddleMouseButtonHandlerContext
{
public:
	DECLARE_DYNAMIC(CJoystickCtrl)

	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	CJoystickCtrl();
	virtual ~CJoystickCtrl();

	void  SetJoystickSet(IJoystickSet* pJoysticks);
	void  SetJoystickContext(IJoystickContext* pJoystickContext);

	void  SetFreezeLayout(bool bFreezeLayout);
	bool  GetFreezeLayout() const;

	void  SetContainer(IJoystickCtrlContainer* pContainer);

	void  SetSnapMargin(float snapMargin);
	float GetSnapMargin() const;

	void  SetAutoCreateKey(bool bAutoCreateKey);
	bool  GetAutoCreateKey() const;

	void  UpdatePreview();
	void  Redraw();

	void  KeyAll();
	void  ZeroAll();

	bool  HitTestPoint(IJoystick*& pJoystick, IJoystick::ChannelType& axis, CPoint point);

	void  Update();

	// IJoystickActionContext
	virtual void BeginUndo();
	virtual void AcceptUndo(const char* name);
	virtual void RestoreUndo();
	virtual void StoreSplineUndo(IJoystick* pJoystick);
	virtual void StoreJoystickUndo();
	virtual void JoysticksChanged();
	virtual void TerminateActionMode();
	virtual void ReleaseMouseCapture();
	virtual void SetJoystickBeingManipulated(IJoystick* pJoystickBeingManipulated);
	virtual void SetJoystickBeingManipulatedPosition(const Vec2& vJoystickBeingManipulatedPosition);

	// IJoystickUndoContext
	virtual void OnSplineChangesUnOrReDone();
	virtual void OnJoytickChangesUnOrReDone();
	virtual void SerializeJoystickSet(IJoystickSet* pJoystickSet, XmlNodeRef node, bool bLoading);

	// IJoystickMiddleMouseButtonHandlerContext
	virtual Vec2 GetPosition() const;
	virtual void SetPosition(const Vec2& position);

	void         OnSetJoystickChannel(IJoystick* pSelectedJoystick, IJoystick::ChannelType channelType, IJoystickChannel* pChannel);

	DECLARE_MESSAGE_MAP()

	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* window, CPoint position);
	afx_msg void OnLButtonDown(UINT, CPoint);
	afx_msg void OnLButtonUp(UINT, CPoint);
	afx_msg void OnMButtonDown(UINT, CPoint);
	afx_msg void OnMButtonUp(UINT, CPoint);
	afx_msg void OnMouseMove(UINT, CPoint);

private:
	void                            Render(CDC& dc);

	void                            OnCreateJoystick(const Vec2& position);
	void                            OnFlipJoystickChannel(IJoystick* pSelectedJoystick, IJoystick::ChannelType channelType);
	void                            OnEditJoystickName(IJoystick* pJoystick);
	void                            OnDeleteJoystick(IJoystick* pJoystick);
	void                            OnJoystickProperties(IJoystick* pJoystick);
	void                            OnEditJoystickColour(IJoystick* pJoystick);
	_smart_ptr<IJoystickActionMode> FindActionModeForMouseDown(const Vec2& position);
	IJoystick*                      HitTestJoysticks(const Vec2& position);

	void                            RenderString(CDC& dc, const char* szString, const Vec2& position, int maxWidth, COLORREF colour);

	uint64                          GenerateJoystickID();
	string                          GenerateDefaultNameForJoystick(IJoystick* pJoystick);
	bool                            HasDefaultName(IJoystick* pJoystick);
	void                            UpdateDocumentRect();

	_smart_ptr<IJoystickSet>          m_pJoysticks;
	IJoystickContext*                 m_pJoystickContext;
	CBitmap                           m_buffer;
	bool                              m_bFreezeLayout;
	IJoystickCtrlContainer*           m_pContainer;
	_smart_ptr<IJoystickActionMode>   m_pActionMode;
	Vec2                              m_vActionStart;
	bool                              m_bStartedDragging;
	bool                              m_bClickDragging;
	IJoystick*                        m_pJoystickBeingManipulated;
	Vec2                              m_vJoystickBeingManipulatedPosition;

	bool                              m_bAutoCreateKey;

	float                             m_snapMargin;
	float                             m_minSize;

	JoystickMiddleMouseButtonHandler* m_pMiddleMouseButtonHandler;
};

#endif //__JOYSTICKCTRL_H__

