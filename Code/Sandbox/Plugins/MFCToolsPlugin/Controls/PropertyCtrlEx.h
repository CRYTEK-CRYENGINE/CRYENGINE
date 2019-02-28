// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __propertyctrlex_h__
#define __propertyctrlex_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "PropertyCtrl.h"
#include "RollupCtrl.h"

class CPropertyCtrlEx;

class CPropertyCategoryDialog : public CDialog
{
public:
	int                       m_nPageId;
	_smart_ptr<CPropertyItem> m_pRootItem;
	CBrush*                   m_pBkgBrush;

	CPropertyCategoryDialog(CPropertyCtrlEx* pCtrl);

protected:
	DECLARE_MESSAGE_MAP()

	virtual void OnOK()     {};
	virtual void OnCancel() {};

	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnNotifyRollupExpand(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg HBRUSH   OnCtlColor(CDC*, CWnd * pWnd, UINT);
	CPropertyCtrlEx* m_pPropertyCtrl;
};

/** Costom control to handle Properties hierarchies.
 */
class PLUGIN_API CPropertyCtrlEx : public CPropertyCtrl, public IEditorNotifyListener
{
	DECLARE_DYNAMIC(CPropertyCtrlEx)
public:
	CPropertyCtrlEx();
	virtual ~CPropertyCtrlEx();

	virtual void Create(DWORD dwStyle, const CRect& rc, CWnd* pParent = NULL, UINT nID = 0);

	virtual void Expand(CPropertyItem* item, bool bExpand, bool bRedraw = true);

	// Ovveride method defined in CWnd.
	BOOL EnableWindow(BOOL bEnable = TRUE);

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	//////////////////////////////////////////////////////////////////////////

	void ReloadItems();

protected:
	friend CPropertyItem;

	static void  RegisterWindowClass();

	virtual void CalcLayout();
	virtual void InvalidateCtrl();
	virtual void InvalidateItems();
	virtual void InvalidateItem(CPropertyItem* pItem);
	virtual void Init();
	virtual void SwitchUI();

	DECLARE_MESSAGE_MAP()

	afx_msg UINT OnGetDlgCode();
	afx_msg void OnDestroy();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg LRESULT OnGetFont(WPARAM wParam, LPARAM);
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnNotifyRollupExpand(NMHDR* pNMHDR, LRESULT* pResult);

	//////////////////////////////////////////////////////////////////////////
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PreSubclassWindow();
	//////////////////////////////////////////////////////////////////////////

	virtual void             DestroyControls(CPropertyItem* pItem);

	CPropertyCategoryDialog* CreateCategoryDialog(CPropertyItem* pItem);

	void                     CalculateItemRect(CPropertyItem* pItem, const CRect& rcWindow, CRect& rcText, CRect& rcCtrl);
	void                     ReCreateControls(CWnd* pDlg, CPropertyItem* pItem);
	void                     ReloadCategoryItem(CPropertyItem* pItem);

	void                     UpdateItemHelp(CPropertyItem* pItem);
	void                     UpdateItemHelp(CPropertyCategoryDialog* pDlg, CPoint point);
	CRollupCtrl*             GetRollupCtrl(CPropertyItem* pItem);
	int                      GetItemCategoryPageId(CPropertyItem* pItem);

	void                     ShowContextMenu(CPropertyCategoryDialog* pDlg, CPoint point);
	void                     SetCanExtended(bool bIsCanExtended);
	void                     RepositionItemControls();

	// Enable or disable all controls inside items.
	void EnableItemControls(bool bEnable);

protected:
	friend class CPropertyCategoryDialog;

	CDialog                   m_hideDlg;
	CRollupCtrl               m_rollupCtrl;
	CRollupCtrl               m_rollupCtrl2;
	CEdit                     m_helpCtrl;

	CWnd*                     m_pWnd;

	_smart_ptr<CPropertyItem> m_pControlsRoot;
	CPropertyItem*            m_pHelpItem;
	bool                      m_bValid;
	CBrush                    m_bkgBrush;

	int                       m_nTotalHeight;
	bool                      m_bUse2Rollups;
	bool                      m_bItemsValid;
	bool                      m_bIgnoreExpandNotify;

	bool                      m_bIsExtended;
	bool                      m_bIsEnabled;

	int                       m_staticTextWidth;
};

#endif // __propertyctrlex_h__

