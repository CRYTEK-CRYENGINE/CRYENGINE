// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __numberctrledit_h__
#define __numberctrledit_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryCore/functor.h>

class CNumberCtrlEdit : public CEdit
{
	CNumberCtrlEdit(const CNumberCtrlEdit& d);
	CNumberCtrlEdit& operator=(const CNumberCtrlEdit& d);

public:
	typedef Functor0 UpdateCallback;

	CNumberCtrlEdit() :
		m_bUpdateOnKillFocus(true),
		m_bSwallowReturn(false)
	{};

	// Attributes
	void SetText(const CString& strText);

	//! Set callback function called when number edit box is really updated.
	void SetUpdateCallback(UpdateCallback func) { m_onUpdate = func; }
	void EnableUpdateOnKillFocus(bool bEnable)  { m_bUpdateOnKillFocus = bEnable; }
	void SetSwallowReturn(bool bDoSwallow)      { m_bSwallowReturn = bDoSwallow; }

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNumberCtrlEdit)
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

	// Generated message map functions
protected:
	//{{AFX_MSG(CNumberCtrlEdit)
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG

	bool IsValidChar(UINT nChar);

	DECLARE_MESSAGE_MAP()

	// Data
protected:

	bool           m_bUpdateOnKillFocus;
	bool           m_bSwallowReturn;
	CString        m_strInitText;
	UpdateCallback m_onUpdate;
};

#endif // __numberctrledit_h__

