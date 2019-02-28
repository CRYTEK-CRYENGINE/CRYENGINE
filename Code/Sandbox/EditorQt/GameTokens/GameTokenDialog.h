// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GameTokenDialog_h__
#define __GameTokenDialog_h__
#pragma once

#include "BaseLibraryDialog.h"
#include "Controls\SplitterCtrl.h"
#include "Controls\TreeCtrlEx.h"
#include "Controls\PropertyCtrl.h"
#include "Controls\PreviewModelCtrl.h"
#include "Controls/ListCtrlEx.h"

class CGameTokenItem;
class CGameTokenManager;

class CGameTokenTreeContainerDialog : public CToolbarDialog
{
public:
	CTreeCtrl* m_pTreeCtrl;

public:
	enum { IDD = IDD_DATABASE };
	CGameTokenTreeContainerDialog() : CToolbarDialog(IDD) { m_pTreeCtrl = 0; };

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy)
	{
		__super::OnSize(nType, cx, cy);
		if (m_pTreeCtrl)
			m_pTreeCtrl->SetWindowPos(NULL, 0, 0, cx, cy, SWP_NOREDRAW | SWP_NOCOPYBITS);
		RecalcLayout();
	}
};

/** Dialog which hosts entity prototype library.
 */
class CGameTokenDialog : public CBaseLibraryDialog
{
	DECLARE_DYNAMIC(CGameTokenDialog);

	// Copy/Paste/Modify from CTextureBrowserPanel::CXTPTaskPanelSpecific
	class CXTPTaskPanelSpecific : public CXTPTaskPanel
	{
	public:
		CXTPTaskPanelSpecific() : CXTPTaskPanel(){};
		~CXTPTaskPanelSpecific(){};

		BOOL              OnCommand(WPARAM wParam, LPARAM lParam);

		CGameTokenDialog* GetOwner() const                { return m_poMyOwner; }
		void              SetOwner(CGameTokenDialog* val) { m_poMyOwner = val; }
	protected:
		CGameTokenDialog* m_poMyOwner;
	};

public:
	CGameTokenDialog(CWnd* pParent);
	~CGameTokenDialog();

	// Called every frame.
	void            Update();

	virtual UINT    GetDialogMenuID();

	CGameTokenItem* GetSelectedGameToken();
	void            SetSelectedGameToken(CGameTokenItem *item);
	void            UpdateSelectedItemInReport();

protected:
	void            UpdateField(int nControlId, const CString& strValue, bool bBoolValue = false);

	void            DoDataExchange(CDataExchange* pDX);
	BOOL            OnInitDialog();

	afx_msg void    OnDestroy();
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg void    OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void    OnLButtonUp(UINT nFlags, CPoint point);

	afx_msg void    OnAddItem();
	afx_msg void    OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnCopy();
	afx_msg void    OnPaste();

	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void    OnReportDblClick(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void    OnReportSelChange(NMHDR* pNotifyStruct, LRESULT* /*result*/);

	afx_msg void    OnTokenTypeChange();
	afx_msg void    OnTokenValueChange();
	afx_msg void    OnTokenInfoChnage();
	afx_msg void    OnTokenLocalOnlyChange();

	//////////////////////////////////////////////////////////////////////////
	// Some functions can be overriden to modify standart functionality.
	//////////////////////////////////////////////////////////////////////////
	virtual void InitToolbar(UINT nToolbarResID);
	virtual void ReloadItems();
	virtual void SelectItem(CBaseLibraryItem* item, bool bForceReload = false);
	virtual void DeleteItem(CBaseLibraryItem* pItem);

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener listener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//void OnUpdateProperties( IVariable *var );

	void DropToItem(HTREEITEM hItem, HTREEITEM hSrcItem, CGameTokenItem* pMtl);

	DECLARE_MESSAGE_MAP()

private:
	CSplitterCtrl                 m_wndHSplitter;
	CSplitterCtrl                 m_wndVSplitter;

	CGameTokenTreeContainerDialog m_treeContainerDlg;

	CPropertyCtrl                 m_propsCtrl;
	CImageList                    m_imageList;

	CImageList*                   m_dragImage;

	// Manager.
	CGameTokenManager*    m_pGameTokenManager;

	HTREEITEM             m_hDropItem;
	HTREEITEM             m_hDraggedItem;

	CXTPReportControl     m_wndReport;
	CXTPTaskPanelSpecific m_wndTaskPanel;
	CImageList            m_taskImageList;

	CEdit                 m_tokenValueEdit;
	CEdit                 m_tokenInfoEdit;
	CButton               m_tokenLocalOnlyCheck;
	CComboBox             m_tokenTypeCombo;

	bool                  m_bSkipUpdateItems;
};

#endif // __GameTokenDialog_h__

