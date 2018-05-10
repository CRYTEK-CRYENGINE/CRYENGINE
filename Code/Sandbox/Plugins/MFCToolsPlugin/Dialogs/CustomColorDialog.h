// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_CUSTOMCOLORDIALOG_H__E1491910_A0B4_48B3_9556_CB079E1EAC7F__INCLUDED_)
#define AFX_CUSTOMCOLORDIALOG_H__E1491910_A0B4_48B3_9556_CB079E1EAC7F__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000
// CustomColorDialog.h : header file
//
// This class extents CMFCColorDialog for two reasons:
//  1. Enables the 'ColorChangeCallback' registration
//  2. Makes the color picking tool work across multiple monitors

/////////////////////////////////////////////////////////////////////////////
// CCustomColorDialog dialog

class PLUGIN_API CCustomColorDialog : public CMFCColorDialog
{
	DECLARE_DYNAMIC(CCustomColorDialog)
public:
	typedef Functor1<COLORREF> ColorChangeCallback;

	CCustomColorDialog(COLORREF clrInit = 0, DWORD dwFlags = 0, CWnd* pParentWnd = NULL);

	void SetColorChangeCallback(ColorChangeCallback cb)
	{ m_callback = cb; }

	virtual BOOL PreTranslateMessage(MSG* pMsg) override;

protected:
	//{{AFX_MSG(CCustomColorDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnKickIdle(WPARAM, LPARAM lCount);
	afx_msg void OnColorSelect();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	void OnColorChange(COLORREF col);

	ColorChangeCallback m_callback;
	COLORREF            m_colorPrev;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CUSTOMCOLORDIALOG_H__E1491910_A0B4_48B3_9556_CB079E1EAC7F__INCLUDED_)

