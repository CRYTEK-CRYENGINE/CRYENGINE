// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PropertyCtrlEx.h"
#include "PropertyItem.h"
#include "Util/Clipboard.h"
#include "Controls/BitmapToolTip.h"
#include "Controls/SharedFonts.h"

BEGIN_MESSAGE_MAP(CPropertyCategoryDialog, CDialog)
//ON_WM_CTLCOLOR()
ON_WM_MOUSEMOVE()
ON_WM_RBUTTONUP()
ON_WM_ERASEBKGND()
ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

#define STATIC_TEXT_WIDTH      160

#define CONTROLS_WIDTH         360
#define CONTROLS_HEIGHT_OFFSET 30
#define CONTROLS_BORDER_OFFSET 0
#define HELP_EDIT_HEIGHT       20
#define SECOND_ROLLUP_BASEID   100000

#define BACKGROUND_COLOR       GetXtremeColor(COLOR_3DLIGHT)

#define ID_ROLLUP_1            1
#define ID_ROLLUP_2            2

//////////////////////////////////////////////////////////////////////////
CPropertyCategoryDialog::CPropertyCategoryDialog(CPropertyCtrlEx* pCtrl) : CDialog()
{
	m_pBkgBrush = 0;
	m_pPropertyCtrl = pCtrl;
}

//////////////////////////////////////////////////////////////////////////
HBRUSH CPropertyCategoryDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	// Call the base class implementation first! Otherwise, it may
	// undo what we're trying to accomplish here.
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	if (m_pBkgBrush)
		hbr = *m_pBkgBrush;

	return hbr;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCategoryDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	__super::OnMouseMove(nFlags, point);
	m_pPropertyCtrl->UpdateItemHelp(this, point);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCategoryDialog::OnRButtonUp(UINT nFlags, CPoint point)
{
	m_pPropertyCtrl->ShowContextMenu(this, point);
}

BOOL CPropertyCategoryDialog::OnEraseBkgnd(CDC* pDC)
{
	return __super::OnEraseBkgnd(pDC);
}
// CPropertyCtrlEx

IMPLEMENT_DYNAMIC(CPropertyCtrlEx, CPropertyCtrl)

BEGIN_MESSAGE_MAP(CPropertyCtrlEx, CPropertyCtrl)
ON_WM_GETDLGCODE()
ON_WM_DESTROY()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONDBLCLK()
ON_WM_MOUSEWHEEL()
ON_WM_RBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_VSCROLL()
ON_WM_KEYDOWN()
ON_WM_KILLFOCUS()
ON_WM_SETFOCUS()
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_SETCURSOR()
ON_WM_CREATE()
ON_MESSAGE(WM_GETFONT, OnGetFont)
ON_WM_TIMER()
ON_NOTIFY(ROLLUPCTRLN_EXPAND, ID_ROLLUP_1, OnNotifyRollupExpand)
ON_NOTIFY(ROLLUPCTRLN_EXPAND, ID_ROLLUP_2, OnNotifyRollupExpand)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CPropertyCtrlEx::CPropertyCtrlEx()
{
	m_bItemsValid = true;
	m_bLayoutValid = true;

	m_nItemHeight = 20;
	m_bValid = false;
	m_nFlags |= F_EXTENDED;

	m_pHelpItem = 0;

	m_bkgBrush.CreateSolidBrush(BACKGROUND_COLOR);

	m_pWnd = 0;

	//COLORREF clr = XTPOffice2007Images()->GetImageColor(_T("DockingPane"), _T("WordPaneBackground"));
	//m_bkgBrush.CreateSolidBrush(clr);

	m_bUse2Rollups = false;
	m_nTotalHeight = 0;
	m_bIgnoreExpandNotify = false;
	m_bIsExtended = true;
	m_bIsCanExtended = true;
	m_bIsEnabled = true;

	m_staticTextWidth = STATIC_TEXT_WIDTH;

	GetIEditor()->RegisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
CPropertyCtrlEx::~CPropertyCtrlEx()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
UINT CPropertyCtrlEx::OnGetDlgCode()
{
	// Want to handle all Tab and arrow keys myself, not delegate it to dialog.
	return __super::OnGetDlgCode();
}

//////////////////////////////////////////////////////////////////////////
// Register your unique class name that you wish to use
void CPropertyCtrlEx::RegisterWindowClass()
{
	WNDCLASS wndcls;

	memset(&wndcls, 0, sizeof(WNDCLASS));   // start with NULL
	// defaults

	wndcls.style = CS_DBLCLKS;

	//you can specify your own window procedure
	wndcls.lpfnWndProc = ::DefWindowProc;
	wndcls.hInstance = AfxGetInstanceHandle();
	wndcls.hIcon = NULL;
	wndcls.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	wndcls.hbrBackground = NULL;
	wndcls.lpszMenuName = NULL;

	// Specify your own class name for using FindWindow later
	wndcls.lpszClassName = _T("PropertyCtrlEx");

	// Register the new class and exit if it fails
	AfxRegisterClass(&wndcls);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::Create(DWORD dwStyle, const CRect& rc, CWnd* pParent, UINT nID)
{
	RegisterWindowClass();
	dwStyle |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	CWnd::Create(_T("PropertyCtrlEx"), "", dwStyle, rc, pParent, nID);
}

int CPropertyCtrlEx::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rc;
	GetClientRect(rc);

	//m_rollupCtrl.SetBkColor( BACKGROUND_COLOR );
	//m_rollupCtrl2.SetBkColor( BACKGROUND_COLOR );
	m_rollupCtrl.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, rc, this, ID_ROLLUP_1);
	m_rollupCtrl2.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, rc, this, ID_ROLLUP_2);

	m_helpCtrl.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY, CRect(0, 0, 0, 0), this, 3);
	m_helpCtrl.SetFont(CFont::FromHandle((HFONT)gMFCFonts.hSystemFont));

	//m_hideDlg.Create( IDD_DATABASE,this );
	//m_hideDlg.ShowWindow(SW_HIDE);

	CalcLayout();

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::PreSubclassWindow()
{
	CWnd::PreSubclassWindow();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::OnDestroy()
{
	DestroyControls(m_pControlsRoot);
	m_pControlsRoot = 0;
	__super::OnDestroy();
}

void CPropertyCtrlEx::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_bIsExtended)
	{
		__super::OnLButtonDown(nFlags, point);
		return;
	}
	CWnd::OnLButtonDown(nFlags, point);
	SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_bIsExtended)
	{
		__super::OnLButtonUp(nFlags, point);
		return;
	}
	CWnd::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (!m_bIsExtended)
	{
		__super::OnLButtonDblClk(nFlags, point);
		return;
	}
	CWnd::OnLButtonDblClk(nFlags, point);
}

BOOL CPropertyCtrlEx::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (!m_bIsExtended)
	{
		return __super::OnMouseWheel(nFlags, zDelta, pt);
	}
	return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}

void CPropertyCtrlEx::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (!m_bIsExtended)
	{
		__super::OnRButtonUp(nFlags, point);
		return;
	}
	CWnd::OnRButtonUp(nFlags, point);
}

void CPropertyCtrlEx::OnRButtonDown(UINT nFlags, CPoint point)
{
	if (!m_bIsExtended)
	{
		__super::OnRButtonDown(nFlags, point);
		return;
	}
	SetFocus();

	CWnd::OnRButtonDown(nFlags, point);
}

void CPropertyCtrlEx::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (!m_bIsExtended)
	{
		__super::OnKeyDown(nChar, nRepCnt, nFlags);
		return;
	}

	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CPropertyCtrlEx::OnKillFocus(CWnd* pNewWnd)
{
	if (!m_bIsExtended)
	{
		__super::OnKillFocus(pNewWnd);
		return;
	}
	CWnd::OnKillFocus(pNewWnd);

	HideBitmapTooltip();
}

void CPropertyCtrlEx::OnSetFocus(CWnd* pOldWnd)
{
	if (!m_bIsExtended)
	{
		__super::OnSetFocus(pOldWnd);
		return;
	}
	CWnd::OnSetFocus(pOldWnd);

	// TODO: Add your message handler code here
}

void CPropertyCtrlEx::OnSize(UINT nType, int cx, int cy)
{
	if (!m_bIsExtended)
	{
		__super::OnSize(nType, cx, cy);
		RedrawWindow();
		return;
	}
	CWnd::OnSize(nType, cx, cy);

	if (!m_rollupCtrl.m_hWnd)
		return;

	if (m_pWnd)
		m_pWnd->MoveWindow(0, 0, cx, cy, FALSE);

	CalcLayout();
}

BOOL CPropertyCtrlEx::OnEraseBkgnd(CDC* pDC)
{
	if (!m_bIsExtended)
	{
		return __super::OnEraseBkgnd(pDC);
	}
	if (!m_bLayoutValid)
		CalcLayout();

	if (!m_bItemsValid)
		ReloadItems();

	return CWnd::OnEraseBkgnd(pDC);
}

void CPropertyCtrlEx::OnPaint()
{
	if (!m_bIsExtended)
		return __super::OnPaint();

	CPaintDC PaintDC(this); // device context for painting
	// TODO: Add your message handler code here
	// Do not call CWnd::OnPaint() for painting messages
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_bIsExtended)
	{
		__super::OnMouseMove(nFlags, point);
		return;
	}

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::Expand(CPropertyItem* item, bool bExpand, bool bRedraw)
{
	if (!m_bIsExtended)
	{
		__super::Expand(item, bExpand);
		return;
	}

	__super::Expand(item, bExpand);

	if (bRedraw)
		ReloadCategoryItem(item);

	if (item->m_nCategoryPageId != -1)
	{
		GetRollupCtrl(item)->ExpandPage(GetItemCategoryPageId(item), bExpand, FALSE);
	}
}
/*
   //////////////////////////////////////////////////////////////////////////
   void CPropertyCtrlEx::DeleteItem( CPropertyItem *pItem )
   {
   ClearSelection();
   assert( pItem );

   _smart_ptr<CPropertyItem> tempHolder = pItem;

   // Find this item and delete.
   CPropertyItem *pParentItem = pItem->GetParent();
   if (pParentItem)
   {
    pParentItem->RemoveChild( pItem );
   }

   if (pItem->m_nCategoryPageId == -1)
   {
    // Non category item.
    ReloadCategoryItem(pItem);
   }
   else
   {
    DestroyControls(pItem);
   }
   }
 */
//////////////////////////////////////////////////////////////////////////
BOOL CPropertyCtrlEx::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (!m_bIsExtended)
	{
		return __super::OnSetCursor(pWnd, nHitTest, message);
	}

	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::CalcLayout()
{
	m_bLayoutValid = true;
	if (!m_bIsExtended)
		return __super::CalcLayout();

	CRect rc;
	GetClientRect(rc);

	HDWP hdwp = BeginDeferWindowPos(4);

	bool bPrevUseRollups = m_bUse2Rollups;

	m_bUse2Rollups = rc.Width() >= CONTROLS_WIDTH * 2 && m_rollupCtrl2.m_hWnd;
	if (m_bUse2Rollups)
	{
		if (m_bIsEnabled && !m_rollupCtrl2.IsWindowVisible())
		{
			m_rollupCtrl2.ShowWindow(SW_SHOW);
		}
	}
	else
	{
		if (m_rollupCtrl2.IsWindowVisible())
		{
			m_rollupCtrl2.ShowWindow(SW_HIDE);
			m_rollupCtrl2.MoveWindow(CRect(0, 0, 0, 0), FALSE);
		}
	}

	CRect rcRollup1(rc);
	CRect rcRollup2(rc);
	CRect rcHelp(rc);

	rcHelp.top = rc.bottom - HELP_EDIT_HEIGHT;
	rcRollup1.bottom = rcHelp.top;
	rcRollup2.bottom = rcHelp.top;

	if (!m_bUse2Rollups)
	{
		hdwp = DeferWindowPos(hdwp, m_rollupCtrl.GetSafeHwnd(), 0, rcRollup1.left, rcRollup1.top, rcRollup1.Width(), rcRollup1.Height(), SWP_NOACTIVATE | SWP_NOZORDER);
	}
	else
	{
		if (m_rollupCtrl2)
		{
			rcRollup1.right = rc.Width() / 2;
			rcRollup2.left = rcRollup1.right;
		}
		hdwp = DeferWindowPos(hdwp, m_rollupCtrl.GetSafeHwnd(), 0, rcRollup1.left, rcRollup1.top, rcRollup1.Width(), rcRollup1.Height(), SWP_NOACTIVATE | SWP_NOZORDER);
		if (m_rollupCtrl2)
		{
			hdwp = DeferWindowPos(hdwp, m_rollupCtrl2.GetSafeHwnd(), 0, rcRollup2.left, rcRollup2.top, rcRollup2.Width(), rcRollup2.Height(), SWP_NOACTIVATE | SWP_NOZORDER);
		}
	}
	hdwp = DeferWindowPos(hdwp, m_helpCtrl.GetSafeHwnd(), 0, rcHelp.left, rcHelp.top, rcHelp.Width(), rcHelp.Height(), SWP_NOZORDER | SWP_SHOWWINDOW);

	if (hdwp)
		EndDeferWindowPos(hdwp);

	if (m_tooltip.m_hWnd)
	{
		m_tooltip.DelTool(this, 1);
		m_tooltip.AddTool(this, "", rc, 1);
	}

	if (bPrevUseRollups != m_bUse2Rollups)
		m_bItemsValid = false;

	RepositionItemControls();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (!m_bIsExtended)
	{
		__super::OnVScroll(nSBCode, nPos, pScrollBar);
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CPropertyCtrlEx::PreTranslateMessage(MSG* pMsg)
{
	return __super::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::Init()
{
	if (m_bIsCanExtended)
		__super::Init();
}

//////////////////////////////////////////////////////////////////////////
LRESULT CPropertyCtrlEx::OnGetFont(WPARAM wParam, LPARAM lParam)
{
	if (!m_bIsExtended)
	{
		return __super::OnGetFont(wParam, lParam);
	}

	LRESULT res = Default();
	if (!res)
	{
		res = (LRESULT)gMFCFonts.hSystemFont;
	}
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::OnTimer(UINT_PTR nIDEvent)
{
	if (!m_bIsExtended)
	{
		__super::OnTimer(nIDEvent);
		return;
	}

	CWnd::OnTimer(nIDEvent);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::OnNotifyRollupExpand(NMHDR* pNMHDR, LRESULT* pResult)
{
	CRollupCtrlNotify* pNotify = (CRollupCtrlNotify*)pNMHDR;
	*pResult = TRUE;

	if (m_bIgnoreExpandNotify)
		return;

	int nId = pNotify->nPageId;
	if (pNotify->hdr.idFrom == ID_ROLLUP_2)
	{
		nId = nId + SECOND_ROLLUP_BASEID;
	}

	Items items;
	GetVisibleItems(m_root, items);

	for (int i = 0; i < items.size(); i++)
	{
		CPropertyItem* pItem = items[i];
		if (pItem->m_nCategoryPageId == nId)
		{
			pItem->SetExpanded(pNotify->bExpand);
			break;
		}
	}
	RepositionItemControls();
}

//////////////////////////////////////////////////////////////////////////
CPropertyCategoryDialog* CPropertyCtrlEx::CreateCategoryDialog(CPropertyItem* pItem)
{
	CPropertyCategoryDialog* pDlg = new CPropertyCategoryDialog(this);
	//pDlg->m_pBkgBrush = &m_bkgBrush;
	pDlg->Create(IDD_DB_ENTITY, this);
	pDlg->ShowWindow(SW_HIDE);
	pDlg->m_pRootItem = pItem;

	ReCreateControls(pDlg, pItem);

	return pDlg;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::ReCreateControls(CWnd* pDlg, CPropertyItem* pRootItem)
{
	pRootItem->m_pControlsHostWnd = pDlg;
	CRect rcWindow;
	pDlg->GetClientRect(rcWindow);

	if (pRootItem)
		pRootItem->DestroyInPlaceControl(true);

	int h = 4;
	Items items;
	GetVisibleItems(pRootItem, items);
	for (int i = 0; i < items.size(); i++)
	{
		CPropertyItem* pItem = items[i];
		if (pItem == m_root)
			continue;

		CRect rcText, rcCtrl;
		CalculateItemRect(items[i], rcWindow, rcText, rcCtrl);

		rcText.top += h;
		rcText.bottom += h;
		rcCtrl.top += h;
		rcCtrl.bottom += h;

		pItem->CreateControls(pDlg, rcText, rcCtrl);
		h += GetItemHeight(items[i]);
	}
	h += 4;

	m_nTotalHeight += h + CONTROLS_HEIGHT_OFFSET;

	pDlg->SetWindowPos(NULL, rcWindow.left, rcWindow.top, rcWindow.Width(), h, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOZORDER);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::CalculateItemRect(CPropertyItem* pItem, const CRect& rcWindow, CRect& rcText, CRect& rcCtrl)
{
	CRect rc(CONTROLS_BORDER_OFFSET, 0, rcWindow.right - CONTROLS_BORDER_OFFSET, 0);

	int xoffset = 0;
	for (CPropertyItem* pParentItem = pItem->GetParent(); pParentItem && !IsCategory(pParentItem); pParentItem = pParentItem->GetParent())
	{
		xoffset += 16;
	}

	rc.bottom = GetItemHeight(pItem);

	rcText = rc;
	//rcText.right = rc.left + nWidth/2;
	rcText.left += xoffset;
	rcText.right = rcText.left + m_staticTextWidth;
	rcText.top += 2;
	rcText.bottom += 4;

	rcCtrl = rc;
	rcCtrl.top = rcCtrl.top - 2;
	rcCtrl.bottom = rcCtrl.top + 18;
	rcCtrl.left = rcText.right;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::UpdateItemHelp(CPropertyCategoryDialog* pDlg, CPoint point)
{
	int h = 1;
	Items items;
	CRect itemRect;
	GetVisibleItems(pDlg->m_pRootItem, items);
	for (int i = 0; i < items.size(); i++)
	{
		CPropertyItem* pItem = items[i];
		if (pItem == m_root)
			continue;

		int itemH = GetItemHeight(pItem);

		if (point.y > h && point.y < h + itemH)
		{
			if (pItem != m_pHelpItem || (m_pBitmapTooltip->m_hWnd && !m_pBitmapTooltip->IsWindowVisible()))
			{
				switch (pItem->GetType())
				{
				case ePropertyFile:
				case ePropertyTexture:
					{
						//GetItemRect( pItem,itemRect );
						//pDlg->ClientToScreen(&itemRect);
						CRect dlgRc;
						pDlg->GetClientRect(dlgRc);
						itemRect.SetRect(0, h, dlgRc.Width(), h + itemH);
						//ShowBitmapTooltip( pItem->GetValue(),CPoint(itemRect.right,itemRect.top) );
						CPoint pnt;
						GetCursorPos(&pnt);
						CRect rc;
						pDlg->GetWindowRect(rc);
						CRect screenItemRc = itemRect;
						pDlg->ClientToScreen(&screenItemRc);
						ShowBitmapTooltip(pItem->GetValue(), CPoint(screenItemRc.right, rc.top + h + itemH), pDlg, itemRect);
					}
					break;
				default:
					HideBitmapTooltip();
					break;
				}
			}
			UpdateItemHelp(pItem);
		}

		h += itemH;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::InvalidateCtrl()
{
	if (!m_bIsExtended)
	{
		__super::InvalidateCtrl();
		return;
	}

	//__super::InvalidateCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::InvalidateItems()
{
	if (!m_bIsExtended)
	{
		__super::InvalidateItems();
		return;
	}

	m_bItemsValid = false;
	if (m_hWnd)
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::InvalidateItem(CPropertyItem* pItem)
{
	if (!m_bIsExtended)
	{
		__super::InvalidateItem(pItem);
		return;
	}

	ReloadCategoryItem(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::SwitchUI()
{
	if (m_bIsExtended)
	{
		SetFlags(GetFlags() & (~F_EXTENDED));
		HideBitmapTooltip();

		m_bItemsValid = true;
		DestroyControls(m_pControlsRoot);

		m_bIgnoreExpandNotify = true; // Needed to prevent accidently collapsing items
		m_rollupCtrl.RemoveAllPages();
		m_rollupCtrl.ShowWindow(SW_HIDE);
		m_helpCtrl.ShowWindow(SW_HIDE);
		if (m_rollupCtrl2)
		{
			m_rollupCtrl2.RemoveAllPages();
			m_rollupCtrl2.ShowWindow(SW_HIDE);
		}
		m_bIsExtended = false;
		//InvalidateItems();
		__super::Init();
		FlatSB_ShowScrollBar(GetSafeHwnd(), SB_VERT, TRUE);
		RECT rc;
		GetWindowRect(&rc);
		OnSize(0, rc.right - rc.left, rc.bottom - rc.top);
		RedrawWindow();
	}
	else
	{
		SetFlags(GetFlags() | F_EXTENDED);
		ModifyStyle(WS_VSCROLL, 0);
		m_bIgnoreExpandNotify = false;
		m_rollupCtrl.ShowWindow(SW_SHOW);
		if (m_rollupCtrl2)
			m_rollupCtrl2.ShowWindow(SW_SHOW);
		m_helpCtrl.ShowWindow(SW_SHOW);
		m_bIsExtended = true;

		SCROLLINFO si;
		ZeroStruct(si);
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		si.nMin = 0;
		si.nMax = 1;
		si.nPage = 0;
		si.nPos = 0;
		m_bItemsValid = false;
		ReloadItems();
		FlatSB_SetScrollInfo(GetSafeHwnd(), SB_VERT, &si, TRUE);
		FlatSB_ShowScrollBar(GetSafeHwnd(), SB_VERT, FALSE);
		//UninitializeFlatSB(GetSafeHwnd());
		RedrawWindow();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::ReloadItems()
{
	if (m_bItemsValid)
		return;
	if (!m_bIsEnabled)
		return;
	if (!m_root)
		return;

	HideBitmapTooltip();

	m_bItemsValid = true;
	DestroyControls(m_pControlsRoot);

	m_bIgnoreExpandNotify = true; // Needed to prevent accidently collapsing items
	m_rollupCtrl.RemoveAllPages();
	if (m_rollupCtrl2)
		m_rollupCtrl2.RemoveAllPages();

	HWND hFocusWnd = ::GetFocus();

	CRollupCtrl* pRollupCtrl = &m_rollupCtrl;

	m_nTotalHeight = 0;

	CRect rc;
	GetClientRect(rc);
	int nHeight = rc.Height();
	int h = 0;

	m_rollupCtrl.SetRedraw(FALSE);
	if (m_rollupCtrl2)
		m_rollupCtrl2.SetRedraw(FALSE);

	// Create dialogs.
	m_pControlsRoot = m_root;
	for (int i = 0; i < m_pControlsRoot->GetChildCount(); i++)
	{
		CPropertyItem* pItem = m_pControlsRoot->GetChild(i);

		if (pItem)
		{
			CRect dlgrc;
			m_bValid = true;
			CPropertyCategoryDialog* pDlg = CreateCategoryDialog(pItem);

			if (pItem->GetVariable() && pItem->GetVariable()->GetFlags() & IVariable::UI_ROLLUP2)
			{
				// Use 2nd rollup.
				if (m_bUse2Rollups)
				{
					pRollupCtrl = &m_rollupCtrl2;
				}
			}
			int nPageId = pRollupCtrl->InsertPage(pItem->GetName(), pDlg, TRUE, -1, pItem->IsExpanded());
			if (pRollupCtrl == &m_rollupCtrl2)
			{
				nPageId = SECOND_ROLLUP_BASEID + nPageId;
			}
			pDlg->m_nPageId = nPageId;
			pItem->m_nCategoryPageId = nPageId;
			pDlg->GetClientRect(dlgrc);
			h += dlgrc.Height() + CONTROLS_HEIGHT_OFFSET;

			//if (h > nHeight && m_bUse2Rollups)
			{
				//h = 0;
				//pRollupCtrl = &m_rollupCtrl2;
			}
		}
	}

	// Make sure focus stay in same window as before.
	if (hFocusWnd && ::GetFocus() != hFocusWnd)
		::SetFocus(hFocusWnd);

	m_rollupCtrl.SetRedraw(TRUE);
	if (m_rollupCtrl2)
		m_rollupCtrl2.SetRedraw(TRUE);

	if (m_nTotalHeight > rc.Height() && rc.Width() >= CONTROLS_WIDTH * 2 && !m_bUse2Rollups)
	{
		// Can use 2 rollups.
		m_bLayoutValid = false;
	}

	m_bIgnoreExpandNotify = false;

	CalcLayout();
}

//////////////////////////////////////////////////////////////////////////
int CPropertyCtrlEx::GetItemCategoryPageId(CPropertyItem* pItem)
{
	if (pItem->m_nCategoryPageId < SECOND_ROLLUP_BASEID)
	{
		return pItem->m_nCategoryPageId;
	}
	else
	{
		return pItem->m_nCategoryPageId - SECOND_ROLLUP_BASEID;
	}
}

//////////////////////////////////////////////////////////////////////////
CRollupCtrl* CPropertyCtrlEx::GetRollupCtrl(CPropertyItem* pItem)
{
	if (pItem == m_root)
		return &m_rollupCtrl;

	if (pItem->m_nCategoryPageId == -1)
	{
		if (pItem->GetParent())
			return GetRollupCtrl(pItem->GetParent());
		else
			return &m_rollupCtrl;
	}
	RC_PAGEINFO* pi = 0;
	if (pItem->m_nCategoryPageId < SECOND_ROLLUP_BASEID)
	{
		return &m_rollupCtrl;
	}
	else
	{
		return &m_rollupCtrl2;
	}
	return &m_rollupCtrl;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::ReloadCategoryItem(CPropertyItem* pItem)
{
	if (pItem == m_root)
		return;
	HideBitmapTooltip();
	if (pItem->m_nCategoryPageId == -1)
	{
		if (pItem->GetParent())
			ReloadCategoryItem(pItem->GetParent());
		else
			return;
	}
	RC_PAGEINFO* pi = GetRollupCtrl(pItem)->GetPageInfo(GetItemCategoryPageId(pItem));
	if (pi && pItem->m_pControlsHostWnd)
	{
		ReCreateControls(pItem->m_pControlsHostWnd, pItem);
		GetRollupCtrl(pItem)->Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::DestroyControls(CPropertyItem* pItem)
{
	if (!m_bIsExtended)
	{
		__super::DestroyControls(pItem);
		return;
	}

	if (pItem)
	{
		pItem->DestroyInPlaceControl(true);
		if (pItem->m_nCategoryPageId != -1)
		{
			m_bIgnoreExpandNotify = true;
			GetRollupCtrl(pItem)->RemovePage(GetItemCategoryPageId(pItem));
			m_bIgnoreExpandNotify = false;
			pItem->m_nCategoryPageId = -1;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::UpdateItemHelp(CPropertyItem* pItem)
{
	if (pItem == m_pHelpItem)
		return;
	CString text;
	if (pItem)
	{
		text = pItem->GetTip();
	}
	m_pHelpItem = pItem;
	m_helpCtrl.SetWindowText(text);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::ShowContextMenu(CPropertyCategoryDialog* pDlg, CPoint point)
{
	CClipboard clipboard;

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 2, _T("Copy Category"));
	menu.AppendMenu(MF_STRING, 3, _T("Copy All"));
	menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
	if (clipboard.IsEmpty())
		menu.AppendMenu(MF_STRING | MF_GRAYED, 4, _T("Paste"));
	else
		menu.AppendMenu(MF_STRING, 4, _T("Paste"));
	if (m_bIsCanExtended)
	{
		menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
		menu.AppendMenu(MF_STRING, 5, _T("Switch UI"));
	}

	CPoint p;
	::GetCursorPos(&p);
	int res = ::TrackPopupMenuEx(menu.GetSafeHmenu(), TPM_LEFTBUTTON | TPM_RETURNCMD, p.x, p.y, GetSafeHwnd(), NULL);
	switch (res)
	{
	case 2:
		if (pDlg->m_pRootItem)
		{
			m_multiSelectedItems.clear();
			m_multiSelectedItems.push_back(pDlg->m_pRootItem);
			OnCopy(true);
			m_multiSelectedItems.clear();
		}
		break;
	case 3:
		OnCopyAll();
		break;
	case 4:
		OnPaste();
		break;
	case 5:
		SwitchUI();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// Ovveride method defined in CWnd.
BOOL CPropertyCtrlEx::EnableWindow(BOOL bEnable)
{
	if (bEnable == (BOOL)m_bIsEnabled)
		return TRUE;

	if (!m_bIsExtended)
	{
		BOOL bRes = __super::EnableWindow(bEnable);
		return bRes;
	}
	m_bIsEnabled = bEnable;

	EnableItemControls(bEnable);

	/*
	   if (bEnable)
	   {
	   //m_hideDlg.ShowWindow(SW_HIDE);
	   if (m_rollupCtrl)
	    m_rollupCtrl.ShowWindow(SW_SHOW);
	   if (m_rollupCtrl2.GetSafeHwnd() && m_bUse2Rollups)
	    m_rollupCtrl2.ShowWindow(SW_SHOW);
	   if (m_helpCtrl)
	    m_helpCtrl.ShowWindow(SW_SHOW);
	   }
	   else
	   {
	   SetWindowText("");
	   //m_hideDlg.ShowWindow(SW_SHOW);

	   if (m_rollupCtrl)
	    m_rollupCtrl.ShowWindow(SW_HIDE);
	   if (m_rollupCtrl2)
	    m_rollupCtrl2.ShowWindow(SW_HIDE);
	   if (m_helpCtrl)
	    m_helpCtrl.ShowWindow(SW_HIDE);
	   }
	 */
	BOOL bRes = __super::EnableWindow(bEnable);
	CalcLayout();
	RedrawWindow();
	return bRes;
}

void CPropertyCtrlEx::SetCanExtended(bool bIsCanExtended)
{
	m_bIsCanExtended = bIsCanExtended;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::RepositionItemControls()
{
	CPropertyItem::s_HDWP = ::BeginDeferWindowPos(20);

	CWnd* pPrevHost = 0;
	Items items;
	GetVisibleItems(GetRootItem(), items);
	for (int i = 0; i < items.size(); i++)
	{
		CPropertyItem* pItem = items[i];
		CWnd* pHost = pItem->m_pControlsHostWnd;
		if (pHost != pPrevHost && pPrevHost != 0)
		{
			::EndDeferWindowPos(CPropertyItem::s_HDWP);
			CPropertyItem::s_HDWP = ::BeginDeferWindowPos(20);
		}
		if (pHost)
		{
			pPrevHost = pHost;
			CRect rcWindow;
			pHost->GetClientRect(rcWindow);
			CRect rcText, rcCtrl;
			CalculateItemRect(pItem, rcWindow, rcText, rcCtrl);
			rcCtrl.top = pItem->m_rcControl.top;
			rcCtrl.bottom = pItem->m_rcControl.bottom;
			pItem->MoveInPlaceControl(rcCtrl);
		}
		//CalculateItemRect( )
	}

	::EndDeferWindowPos(CPropertyItem::s_HDWP);
	CPropertyItem::s_HDWP = 0;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::EnableItemControls(bool bEnable)
{
	Items items;
	GetVisibleItems(GetRootItem(), items);
	for (int i = 0; i < items.size(); i++)
	{
		CPropertyItem* pItem = items[i];
		pItem->EnableControls(bEnable);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlEx::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		{
			if (!m_bLayoutValid)
				CalcLayout();

			if (!m_bItemsValid)
				ReloadItems();
		}
		break;
	}
}

