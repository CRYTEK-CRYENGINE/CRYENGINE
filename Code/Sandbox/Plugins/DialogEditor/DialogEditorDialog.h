// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DialogScriptView.h"
#include "Controls/ColorCtrl.h"
#include "Util/Variable.h"

class CBaseObject;
class CEntityObject;
class CDialogManager;

#define DIALOG_EDITOR_NAME "Dialog Editor"
#define DIALOG_EDITOR_VER  "1.00"


namespace DialogEditor
{

template<class BASE_TYPE>
class CFlatFramed : public BASE_TYPE
{
protected:
	//{{AFX_MSG(CFlatFramed)
	afx_msg void OnNcPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

BEGIN_TEMPLATE_MESSAGE_MAP(CFlatFramed, BASE_TYPE, BASE_TYPE)
//{{AFX_MSG_MAP(CFlatFramed)
ON_WM_NCPAINT()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

template<class BASE_TYPE>
void CFlatFramed<BASE_TYPE >::OnNcPaint()
{
	__super::OnNcPaint();
	//	if ( !IsAppThemed() )
	//	{
	CWindowDC dc(this);
	CRect rc;
	dc.GetClipBox(rc);
	COLORREF color = GetSysColor(COLOR_3DSHADOW);
	dc.Draw3dRect(rc, color, color);
	//	}
}
};

class CDialogEditorDialog;
class CTreeScriptRecord;

//////////////////////////////////////////////////////////////////////////
class CDialogFolderCtrl : public CXTPReportControl
{
	DECLARE_MESSAGE_MAP()
public:
	CDialogFolderCtrl();
	~CDialogFolderCtrl();

	void    Reload();
	void    SetDialog(CDialogEditorDialog* pDialog) { m_pDialog = pDialog; }
	void    GotoEntry(const CString& entry);
	void    UpdateEntry(const CString& entry);
	CString GetCurrentGroup();

protected:

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd*);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportRowExpandChanged(NMHDR* pNotifyStruct, LRESULT* result);

	afx_msg void OnDestroy();

	void         OnSelectionChanged();
	void         UpdateSCStatus(CTreeScriptRecord* pRec, CEditorDialogScript* pScript = 0, bool bUseCached = false);
	void         UpdateChildren(CXTPReportRow* pRow);

protected:
	CImageList           m_imageList;
	CDialogEditorDialog* m_pDialog;
};

//////////////////////////////////////////////////////////////////////////
//
// Main Dialog, the Smart Objects Editor.
//
//////////////////////////////////////////////////////////////////////////
class CDialogEditorDialog
	: public CXTPFrameWnd
	  , public IEditorNotifyListener
{
	DECLARE_DYNCREATE(CDialogEditorDialog)

public:
	CDialogEditorDialog();
	~CDialogEditorDialog();

	BOOL           Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd);
	CXTPTaskPanel& GetTaskPanel() { return m_taskPanel; }

	void           RecalcLayout(BOOL bNotify = TRUE);

protected:
	DECLARE_MESSAGE_MAP()

	virtual BOOL    PreTranslateMessage(MSG* pMsg);
	virtual BOOL    OnInitDialog();
	virtual void    PostNcDestroy();
	afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
	afx_msg void    OnSetFocus(CWnd* pOldWnd);
	afx_msg void    OnDestroy();
	afx_msg void    OnClose();
	afx_msg LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);
	virtual BOOL    OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

public:
	afx_msg void OnAddDialog();
	afx_msg void OnDelDialog();
	afx_msg void OnRenameDialog();
	afx_msg void OnLocalEdit();
protected:
	afx_msg void OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportChecked(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportEdit(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnDescriptionEdit();
	afx_msg void OnMouseMove(UINT, CPoint);
	afx_msg void OnLButtonUp(UINT, CPoint);

	void                    CreatePanes();
	CXTPDockingPaneManager* GetDockingPaneManager() { return &m_paneManager; }

	// Called by the editor to notify the listener about the specified event.
	void OnEditorNotifyEvent(EEditorNotifyEvent event);

	// Called by the property editor control
	void OnUpdateProperties(IVariable* pVar);

	// Reload values in Dialog Browser Tree
	void ReloadDialogBrowser();

	// Save current script
	bool SaveCurrent();

	void UpdateWindowText(CEditorDialogScript* pScript, bool bModified, bool bTitleOnly);

public:
	CDialogManager* GetManager() const { return m_pDM; }
	bool            SetCurrentScript(const CString& script, bool bSelectInTree, bool bSaveModified = true, bool bForceUpdate = false);
	bool            SetCurrentScript(CEditorDialogScript* pScript, bool bSelectInTree, bool bSaveModified = true, bool bForceUpdate = false);
	void            NotifyChange(CEditorDialogScript* pScript, bool bChange); // called from CDialogScriptView
	void            UpdateHelp(const CString& help);

	enum ESourceControlOp
	{
		ESCM_IMPORT,
		ESCM_CHECKIN,
		ESCM_CHECKOUT,
		ESCM_UNDO_CHECKOUT,
		ESCM_GETLATEST,
	};

	bool DoSourceControlOp(CEditorDialogScript * pScript, ESourceControlOp);

protected:
	//	CXTPDockingPane*		m_pPropertiesPane;
	//  DialogEditor::CFlatFramed< CPropertyCtrl > m_Properties;
	//  CVarBlockPtr			m_vars;

	// dialog stuff
	DialogEditor::CFlatFramed<CDialogScriptView> m_View;
	// DialogEditor::CFlatFramed< CDialogScriptViewView > m_View;
	CXTPDockingPaneManager                       m_paneManager;
	CXTPTaskPanel                                m_taskPanel;
	CXTPToolBar                                  m_wndToolBar;

	CXTPTaskPanelGroupItem*                      m_pItemHelpersEdit;
	CXTPTaskPanelGroupItem*                      m_pItemHelpersNew;
	CXTPTaskPanelGroupItem*                      m_pItemHelpersDelete;
	CXTPTaskPanelGroupItem*                      m_pItemHelpersDone;

	DialogEditor::CFlatFramed<CDialogFolderCtrl> m_ctrlBrowser;     // Folder Tree
	DialogEditor::CFlatFramed<CEdit>             m_ctrlDescription; // Description
	DialogEditor::CFlatFramed<CColoredEdit>      m_ctrlHelp;        // Help

	CImageList      m_imageList;
	CDialogManager* m_pDM;
};

