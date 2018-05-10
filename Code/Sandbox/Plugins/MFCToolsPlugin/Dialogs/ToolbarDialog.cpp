// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ToolbarDialog.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CToolbarDialog, CXTPDialogBase<CXTResizeDialog> )

BEGIN_MESSAGE_MAP(CToolbarDialog, CXTPDialogBase<CXTResizeDialog> )
//{{AFX_MSG_MAP(CTerrainDialog)
ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
ON_WM_DESTROY()
ON_WM_MENUSELECT()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CToolbarDialog::CToolbarDialog()
	: CXTPDialogBase<CXTResizeDialog>((UINT)0, NULL)
{

}

CToolbarDialog::CToolbarDialog(UINT nIDTemplate, CWnd* pParentWnd)
	: CXTPDialogBase<CXTResizeDialog>(nIDTemplate, pParentWnd)
{}

CToolbarDialog::~CToolbarDialog()
{

}

BOOL CToolbarDialog::OnToolTipText(UINT, NMHDR* pNMHDR, LRESULT* pResult)
{
	////////////////////////////////////////////////////////////////////////
	// Handle tooltip text requests from the toolbars
	////////////////////////////////////////////////////////////////////////

	ASSERT(pNMHDR->code == TTN_NEEDTEXTA || pNMHDR->code == TTN_NEEDTEXTW);

	// Allow top level routing frame to handle the message
	// if (GetRoutingFrame() != NULL)
	//   return FALSE;

	// Need to handle both ANSI and UNICODE versions of the message
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*) pNMHDR;
	TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*) pNMHDR;
	TCHAR szFullText[256];
	CString cstTipText;
	CString cstStatusText;

	UINT_PTR nID = pNMHDR->idFrom;
	if (pNMHDR->code == TTN_NEEDTEXTA && (pTTTA->uFlags & TTF_IDISHWND) ||
	    pNMHDR->code == TTN_NEEDTEXTW && (pTTTW->uFlags & TTF_IDISHWND))
	{
		// idFrom is actually the HWND of the tool
		nID = ((UINT_PTR) (WORD)::GetDlgCtrlID((HWND) nID));
	}

	if (nID != 0) // will be zero on a separator
	{
		AfxLoadString(nID, szFullText);
		// this is the command id, not the button index
		AfxExtractSubString(cstTipText, szFullText, 1, '\n');
		AfxExtractSubString(cstStatusText, szFullText, 0, '\n');
	}

	// Non-UNICODE Strings only are shown in the tooltip window...
	if (pNMHDR->code == TTN_NEEDTEXTA)
		lstrcpyn(pTTTA->szText, cstTipText,
		         (sizeof(pTTTA->szText) / sizeof(pTTTA->szText[0])));
	else
		;//Unicode::Convert(pTTTW->szText, cstTipText);

	*pResult = 0;

	// Bring the tooltip window above other popup windows
	::SetWindowPos(pNMHDR->hwndFrom, HWND_TOP, 0, 0, 0, 0,
	               SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);

	return TRUE;    // Message was handled
}

BOOL CToolbarDialog::OnInitDialog()
{
	__super::OnInitDialog();

	RecalcLayout();

	return TRUE;  // Return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CToolbarDialog::RecalcLayout()
{
	////////////////////////////////////////////////////////////////////////
	// Place the toolbars and move the controls in the dialog to make space
	// for them
	////////////////////////////////////////////////////////////////////////

	CRect rcClientStart;
	CRect rcClientNow;
	CRect rcChild;
	CRect rcWindow;
	CWnd* pwndChild = GetWindow(GW_CHILD);
	if (pwndChild == NULL)
		return;

	CRect clientRect;
	GetClientRect(clientRect);
	// We need to resize the dialog to make room for control bars.
	// First, figure out how big the control bars are.
	GetClientRect(rcClientStart);
	RepositionBarsInternal(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST,
	                       0, reposQuery, rcClientNow);

	// Now move all the controls so they are in the same relative
	// position within the remaining client area as they would be
	// with no control bars.
	CPoint ptOffset(rcClientNow.left - rcClientStart.left,
	                rcClientNow.top - rcClientStart.top);

	while (pwndChild)
	{
		UINT nID = pwndChild->GetDlgCtrlID();
		if (nID >= AFX_IDW_CONTROLBAR_FIRST && nID < AFX_IDW_CONTROLBAR_LAST)
		{
			pwndChild = pwndChild->GetNextWindow();
			continue;
		}
		pwndChild->GetWindowRect(rcChild);
		ScreenToClient(rcChild);
		rcChild.OffsetRect(ptOffset);
		rcChild.IntersectRect(rcChild, rcClientNow);
		pwndChild->MoveWindow(rcChild, FALSE);
		pwndChild = pwndChild->GetNextWindow();
	}

	/*
	   // Adjust the dialog window dimensions
	   GetWindowRect(rcWindow);
	   rcWindow.right += rcClientStart.Width() - rcClientNow.Width();
	   rcWindow.bottom += rcClientStart.Height() - rcClientNow.Height();
	   MoveWindow(rcWindow, FALSE);
	 */

	// And position the control bars
	RepositionBarsInternal(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
}

void CToolbarDialog::RepositionBarsInternal(UINT nIDFirst, UINT nIDLast, UINT nIDLeftOver,
                                            UINT nFlags, LPRECT lpRectParam, LPCRECT lpRectClient, BOOL bStretch)
{
	ASSERT(nFlags == 0 || (nFlags & ~reposNoPosLeftOver) == reposQuery ||
	       (nFlags & ~reposNoPosLeftOver) == reposExtra);

	// walk kids in order, control bars get the resize notification
	//   which allow them to shrink the client area
	// remaining size goes to the 'nIDLeftOver' pane
	// NOTE: nIDFirst->nIDLast are usually 0->0xffff

	AFX_SIZEPARENTPARAMS layout;
	HWND hWndLeftOver = NULL;

	layout.bStretch = bStretch;
	layout.sizeTotal.cx = layout.sizeTotal.cy = 0;
	if (lpRectClient != NULL)
		layout.rect = *lpRectClient;    // starting rect comes from parameter
	else
		GetClientRect(&layout.rect);    // starting rect comes from client rect

	if ((nFlags & ~reposNoPosLeftOver) != reposQuery)
		layout.hDWP = ::BeginDeferWindowPos(8); // reasonable guess
	else
		layout.hDWP = NULL; // not actually doing layout

	for (HWND hWndChild = ::GetTopWindow(m_hWnd); hWndChild != NULL;
	     hWndChild = ::GetNextWindow(hWndChild, GW_HWNDNEXT))
	{
		UINT_PTR nIDC = ((UINT)(WORD)::GetDlgCtrlID(hWndChild));
		CWnd* pWnd = CWnd::FromHandlePermanent(hWndChild);
		if (nIDC == nIDLeftOver)
			hWndLeftOver = hWndChild;
		else if (nIDC >= nIDFirst && nIDC <= nIDLast && pWnd != NULL)
			::SendMessage(hWndChild, WM_SIZEPARENT, 0, (LPARAM)&layout);
	}

	// if just getting the available rectangle, return it now...
	if ((nFlags & ~reposNoPosLeftOver) == reposQuery)
	{
		ASSERT(lpRectParam != NULL);
		if (bStretch)
			::CopyRect(lpRectParam, &layout.rect);
		else
		{
			lpRectParam->left = lpRectParam->top = 0;
			lpRectParam->right = layout.sizeTotal.cx;
			lpRectParam->bottom = layout.sizeTotal.cy;
		}
		return;
	}

	// the rest is the client size of the left-over pane
	if (nIDLeftOver != 0 && hWndLeftOver != NULL)
	{
		CWnd* pLeftOver = CWnd::FromHandle(hWndLeftOver);
		// allow extra space as specified by lpRectBorder
		if ((nFlags & ~reposNoPosLeftOver) == reposExtra)
		{
			ASSERT(lpRectParam != NULL);
			layout.rect.left += lpRectParam->left;
			layout.rect.top += lpRectParam->top;
			layout.rect.right -= lpRectParam->right;
			layout.rect.bottom -= lpRectParam->bottom;
		}
		// reposition the window
		if ((nFlags & reposNoPosLeftOver) != reposNoPosLeftOver)
		{
			pLeftOver->CalcWindowRect(&layout.rect);
			RepositionWindowInternal(&layout, hWndLeftOver, &layout.rect);
		}
	}

	// move and resize all the windows at once!
	if (layout.hDWP == NULL || !::EndDeferWindowPos(layout.hDWP))
		TRACE(traceAppMsg, 0, "Warning: DeferWindowPos failed - low system resources.\n");
}

//////////////////////////////////////////////////////////////////////////
void CToolbarDialog::RepositionWindowInternal(AFX_SIZEPARENTPARAMS* lpLayout, HWND hWnd, LPCRECT lpRect)
{
	ASSERT(hWnd != NULL);
	ASSERT(lpRect != NULL);
	HWND hWndParent = ::GetParent(hWnd);
	ASSERT(hWndParent != NULL);

	if (lpLayout != NULL && lpLayout->hDWP == NULL)
		return;

	// first check if the new rectangle is the same as the current
	CRect rectOld;
	::GetWindowRect(hWnd, rectOld);
	::ScreenToClient(hWndParent, &rectOld.TopLeft());
	::ScreenToClient(hWndParent, &rectOld.BottomRight());
	if (::EqualRect(rectOld, lpRect))
		return;     // nothing to do

	// try to use DeferWindowPos for speed, otherwise use SetWindowPos
	if (lpLayout != NULL)
	{
		lpLayout->hDWP = ::DeferWindowPos(lpLayout->hDWP, hWnd, NULL,
		                                  lpRect->left, lpRect->top, lpRect->right - lpRect->left,
		                                  lpRect->bottom - lpRect->top, SWP_NOACTIVATE | SWP_NOZORDER);
	}
	else
	{
		::SetWindowPos(hWnd, NULL, lpRect->left, lpRect->top,
		               lpRect->right - lpRect->left, lpRect->bottom - lpRect->top,
		               SWP_NOACTIVATE | SWP_NOZORDER);
	}
}

void CToolbarDialog::OnDestroy()
{
	////////////////////////////////////////////////////////////////////////
	// Set the status bar text back to "Ready"
	////////////////////////////////////////////////////////////////////////

	// TODO: Crashed in static object dialog
	// GetIEditor()->SetStatusText("Ready");
}

void CToolbarDialog::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	////////////////////////////////////////////////////////////////////////
	// Set the menu help text in the status bar of the main frame
	////////////////////////////////////////////////////////////////////////

	__super::OnMenuSelect(nItemID, nFlags, hSysMenu);

	TCHAR szFullText[256];
	CString cstStatusText;

	// TODO: Add your message handler code here

	// Displays in the mainframe's status bar
	if (nItemID != 0) // Will be zero on a separator
	{
		AfxLoadString(nItemID, szFullText);

		// This is the command id, not the button index
		AfxExtractSubString(cstStatusText, szFullText, 0, '\n');
	}
}

/////////////////////////////////////////////////////////////////////////////
// CToolbarDialog::OnInitMenuPopup
//		OnInitMenuPopup updates the state of items on a popup menu.
//
//    This code is based on CFrameWnd::OnInitMenuPopup.  We assume the
//		application does not support context sensitive help.

void CToolbarDialog::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	if (!bSysMenu)
	{
		ASSERT(pPopupMenu != NULL);

		// check the enabled state of various menu items
		CCmdUI state;
		state.m_pMenu = pPopupMenu;
		ASSERT(state.m_pOther == NULL);

		state.m_nIndexMax = pPopupMenu->GetMenuItemCount();
		for (state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax;
		     state.m_nIndex++)
		{
			state.m_nID = pPopupMenu->GetMenuItemID(state.m_nIndex);
			if (state.m_nID == 0)
				continue; // menu separator or invalid cmd - ignore it

			ASSERT(state.m_pOther == NULL);
			ASSERT(state.m_pMenu != NULL);
			if (state.m_nID == (UINT)-1)
			{
				// possibly a popup menu, route to first item of that popup
				state.m_pSubMenu = pPopupMenu->GetSubMenu(state.m_nIndex);
				if (state.m_pSubMenu == NULL ||
				    (state.m_nID = state.m_pSubMenu->GetMenuItemID(0)) == 0 ||
				    state.m_nID == (UINT)-1)
				{
					continue;     // first item of popup can't be routed to
				}
				state.DoUpdate(this, FALSE);    // popups are never auto disabled
			}
			else
			{
				// normal menu item
				// Auto enable/disable if command is _not_ a system command
				state.m_pSubMenu = NULL;
				state.DoUpdate(this, state.m_nID < 0xF000);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CToolbarDialog::OnEnterIdle
//		OnEnterIdle updates the status bar when there's nothing better to do.
//    This code is based on CFrameWnd::OnEnterIdle.

void CToolbarDialog::OnEnterIdle(UINT nWhy, CWnd* pWho)
{
	/*
	   if (nWhy != MSGF_MENU || m_nIDTracking == m_nIDLastMessage)
	   return;

	   OnSetMessageString(m_nIDTracking);
	   ASSERT(m_nIDTracking == m_nIDLastMessage);
	 */
}

//////////////////////////////////////////////////////////////////////////
LRESULT CToolbarDialog::OnKickIdle(WPARAM wParam, LPARAM)
{
	SendMessageToDescendants(WM_IDLEUPDATECMDUI, 1);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// CCustomFrameWnd
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CCustomFrameWnd, CXTPFrameWnd)

BEGIN_MESSAGE_MAP(CCustomFrameWnd, CXTPFrameWnd)
ON_WM_DESTROY()
ON_WM_ERASEBKGND()

//////////////////////////////////////////////////////////////////////////
// XT Commands.
ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

CCustomFrameWnd::CCustomFrameWnd()
{
	m_pViewWnd = 0;
}

//////////////////////////////////////////////////////////////////////////
BOOL CCustomFrameWnd::Create(DWORD dwStyle, const CRect& rect, CWnd* pParentWnd, UINT nID)
{
	ASSERT(m_hWnd == NULL);

	//////////////////////////////////////////////////////////////////////////
	// Create window.
	//////////////////////////////////////////////////////////////////////////
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
	BOOL bRes = CreateEx(NULL, lpClassName, NULL, dwStyle, rect, pParentWnd, nID);

	if (bRes)
	{
		bRes = OnInitDialog();
	}

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CCustomFrameWnd::OnDestroy()
{
	if (!m_profile.IsEmpty())
	{
		CXTPDockingPaneLayout layout(GetDockingPaneManager());
		GetDockingPaneManager()->GetLayout(&layout);
		layout.Save(m_profile);
	}

	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CCustomFrameWnd::LoadLayout(const CString& profile)
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CCustomFrameWnd::PreTranslateMessage(MSG* pMsg)
{
	bool bFramePreTranslate = true;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		CWnd* pWnd = CWnd::GetFocus();
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CEdit)))
			bFramePreTranslate = false;
	}

	if (bFramePreTranslate)
	{
		// allow tooltip messages to be filtered
		if (__super::PreTranslateMessage(pMsg))
			return TRUE;
	}

	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		// All key presses are translated by this frame window

		::TranslateMessage(pMsg);
		::DispatchMessage(pMsg);

		return TRUE;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CCustomFrameWnd::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CCustomFrameWnd::OnToggleBar(UINT nID)
{
	CXTPDockingPane* pBar = GetDockingPaneManager()->FindPane(nID);
	if (pBar)
	{
		if (pBar->IsClosed())
			GetDockingPaneManager()->ShowPane(pBar);
		else
			GetDockingPaneManager()->ClosePane(pBar);
		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CCustomFrameWnd::OnUpdateControlBar(CCmdUI* pCmdUI)
{
	UINT nID = pCmdUI->m_nID;

	CXTPCommandBars* pCommandBars = GetCommandBars();
	if (pCommandBars != NULL)
	{
		CXTPToolBar* pBar = pCommandBars->GetToolBar(nID);
		if (pBar)
		{
			pCmdUI->SetCheck((pBar->IsVisible()) ? 1 : 0);
			return;
		}
	}
	CXTPDockingPane* pBar = GetDockingPaneManager()->FindPane(nID);
	if (pBar)
	{
		pCmdUI->SetCheck((!pBar->IsClosed()) ? 1 : 0);
		return;
	}
	pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
BOOL CCustomFrameWnd::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	BOOL res = FALSE;

	//CWnd *pView = GetDlgItem(AFX_IDW_PANE_FIRST);
	CWnd* pView = m_pViewWnd;
	if (pView && pView->m_hWnd)
	{
		res = pView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		if (TRUE == res)
			return res;
	}

	/*
	   CPushRoutingFrame push(this);
	   return CWnd::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	 */

	return __super::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

//////////////////////////////////////////////////////////////////////////
void CCustomFrameWnd::InstallDockingPanes()
{
	//////////////////////////////////////////////////////////////////////////
	GetDockingPaneManager()->InstallDockingPanes(this);
	GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);

	//////////////////////////////////////////////////////////////////////////
}

