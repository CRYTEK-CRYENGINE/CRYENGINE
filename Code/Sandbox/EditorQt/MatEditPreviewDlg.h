// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_MATEDITPREVIEWDLG_H__AFB120BD_DBC4_40C0_A03C_8531A6530F3B__INCLUDED_)
#define AFX_MATEDITPREVIEWDLG_H__AFB120BD_DBC4_40C0_A03C_8531A6530F3B__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include "Dialogs/ToolbarDialog.h"
#include "Controls\PreviewModelCtrl.h"
#include "IDataBaseManager.h"

// MatEditPreviewDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMatEditPreviewDlg dialog
class CMatEditPreviewDlg : public CToolbarDialog, public IDataBaseManagerListener
{
	// Construction
public:
	CMatEditPreviewDlg(const char* title = NULL, CWnd* pParent = NULL);   // standard constructor

	~CMatEditPreviewDlg();

	// Dialog Data
	//{{AFX_DATA(CMatEditPreviewDlg)
	enum { IDD = IDD_MATEDITPREVIEWDLG };
	//}}AFX_DATA

	//Functions

	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event);

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMatEditPreviewDlg)
protected:
	//virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

	// Implementation
protected:
	virtual void PostNcDestroy();

	// Generated message map functions
	//{{AFX_MSG(CMatEditPreviewDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPreviewSphere();
	afx_msg void OnPreviewPlane();
	afx_msg void OnPreviewBox();
	afx_msg void OnPreviewTeapot();
	afx_msg void OnPreviewCustom();
	afx_msg void OnUpdateAlways();
	afx_msg void OnUpdateAlwaysUpdateUI(CCmdUI* pCmdUI);
	afx_msg void OnDestroy();
	afx_msg void OnClose();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnKickIdle(WPARAM wParam, LPARAM);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	CPreviewModelCtrl m_previewCtrl;

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MATEDITPREVIEWDLG_H__AFB120BD_DBC4_40C0_A03C_8531A6530F3B__INCLUDED_)

