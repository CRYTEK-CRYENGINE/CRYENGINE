// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FACIALJOYSTICKDIALOG_H__
#define __FACIALJOYSTICKDIALOG_H__

#include "FacialEdContext.h"
#include "Controls/JoystickCtrl.h"
#include "Dialogs/ToolbarDialog.h"

class CFacialJoystickDialog : public CToolbarDialog, public IFacialEdListener, IJoystickCtrlContainer, IEditorNotifyListener
{
	DECLARE_DYNAMIC(CFacialJoystickDialog)
	friend class CJoystickDialogDropTarget;
public:

	CFacialJoystickDialog();
	~CFacialJoystickDialog();

	void SetContext(CFacialEdContext* pContext);

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK()     {};
	virtual void OnCancel() {};
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnSize(UINT nType, int cx, int cy);
	virtual void OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);
	void         RecalcLayout();
	void         UpdateFreezeLayoutStatus();
	void         UpdateAutoCreateKeyStatus();
	void         ReadDisplayedSnapMargin();
	void         Update();

	// IJoystickCtrlContainer
	virtual void              OnAction(JoystickAction action);
	virtual void              OnFreezeLayoutChanged();
	virtual IJoystickChannel* GetPotentialJoystickChannel();
	virtual float             GetCurrentEvaluationTime();
	virtual float             GetMaxEvaluationTime();
	virtual void              OnSplineChanged();
	virtual void              OnJoysticksChanged();
	virtual void              OnBeginDraggingJoystickKnob(IJoystick* pJoystick);
	virtual void              OnJoystickSelected(IJoystick* pJoystick, bool exclusive);
	virtual bool              GetPlaying() const;
	virtual void              SetPlaying(bool playing);

	// IEditorNotifyListener implementation
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

	void         RefreshControlJoystickSet();

	DECLARE_MESSAGE_MAP()

	afx_msg void OnFreezeLayout();
	afx_msg void OnSnapMarginChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnUpdateSnapMarginSizeUI(CCmdUI* pCmdUI);
	afx_msg void OnAutoCreateKeyChanged();
	afx_msg void OnKeyAll();
	afx_msg void OnZeroAll();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnDestroy();

	CFacialEdContext*  m_pContext;
	CJoystickCtrl      m_ctrl;
	CXTPToolBar        m_toolbar;
	CXTPControlButton* m_pFreezeLayoutButton;
	CXTPControlButton* m_pAutoCreateKeyButton;

	typedef std::vector<int> SnapMarginList;
	SnapMarginList  m_snapMargins;
	int             m_displayedSnapMargin;
	bool            m_bIgnoreSplineChangeEvents;
	HACCEL          m_hAccelerators;
	COleDropTarget* m_pDropTarget;
};

#endif //__FACIALJOYSTICKDIALOG_H__

