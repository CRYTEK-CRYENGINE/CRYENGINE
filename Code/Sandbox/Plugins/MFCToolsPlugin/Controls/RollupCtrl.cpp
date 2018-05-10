// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "RollupCtrl.h"
#include "Controls/ListCtrlEx.h"
#include "Controls/TreeCtrlReport.h"
#include "Controls/TreeCtrlEx.h"
#include "Controls/SharedFonts.h"

/////////////////////////////////////////////////////////////////////////////
// CRollupCtrl Message Map

BEGIN_MESSAGE_MAP(CRollupCtrl, CWnd)
//{{AFX_MSG_MAP(CRollupCtrl)
ON_WM_DESTROY()
ON_WM_PAINT()
ON_WM_SIZE()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_MOUSEWHEEL()
ON_WM_MOUSEACTIVATE()
ON_WM_CONTEXTMENU()
ON_WM_CTLCOLOR()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRollupCtrl Implementation

IMPLEMENT_DYNCREATE(CRollupCtrl, CWnd)

LRESULT CALLBACK TransparentDlgItemWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WNDPROC pOldProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	//if (uMsg == WM_ERASEBKGND)
	//return TRUE;

	return ::CallWindowProc(pOldProc, hWnd, uMsg, wParam, lParam);
}

//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
CRollupCtrl::CRollupCtrl()
{
	//SetBkColor( CXTColorRef::GetColor(COLOR_BTNFACE) );

	m_nStartYPos = m_nPageHeight = 0;
	m_lastId = 0;
	m_bRecalcLayout = false;

	m_bkColor = 0;
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
CRollupCtrl::~CRollupCtrl()
{
	DestroyAllPages();
}

//////////////////////////////////////////////////////////////////////////
void CRollupCtrl::OnDestroy()
{
	DestroyAllPages();
	CWnd::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CRollupCtrl::DestroyAllPages()
{
	//Remove all pages allocations
	const size_t numPages = m_PageList.size();
	for (int i = 0; i < numPages; i++)
	{
		RC_PAGEINFO* pPage = m_PageList[i];

		delete pPage->pwndButton;
		delete pPage->pwndGroupBox;

		if (pPage->pwndTemplate && pPage->bAutoDestroyTpl)
		{
			pPage->pwndTemplate->DestroyWindow();
			delete pPage->pwndTemplate;
		}

		delete pPage;
	}
	m_PageList.clear();
}

//---------------------------------------------------------------------------
// Create
//---------------------------------------------------------------------------
BOOL CRollupCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	m_strMyClass = AfxRegisterWndClass(
	  CS_VREDRAW | CS_HREDRAW,
	  (HCURSOR)::LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1), NULL);

	return CWnd::Create(m_strMyClass, "RollupCtrl", dwStyle, rect, pParentWnd, nID);
}

//---------------------------------------------------------------------------
// Function name	: InsertPage
// Description		: Return -1 if an error occurs
//					  Make sure template had WS_CHILD style
//---------------------------------------------------------------------------
int CRollupCtrl::InsertPage(LPCTSTR caption, UINT nIDTemplate, CRuntimeClass* rtc, int idx)
{
	if (idx > 0 && idx >= m_PageList.size())    idx = -1;

	//Create Template
	ASSERT(rtc != NULL);
	CDialog* pwndTemplate = (CDialog*)rtc->CreateObject();
	BOOL b = pwndTemplate->Create(nIDTemplate, this);
	if (!b) { delete pwndTemplate; return -1; }

	//Insert Page
	return _InsertPage(caption, pwndTemplate, idx, TRUE);
}

//---------------------------------------------------------------------------
// Function name	: InsertPage
// Description		: return -1 if an error occurs
//					  Make sure template had WS_CHILD style
//---------------------------------------------------------------------------
int CRollupCtrl::InsertPage(LPCTSTR caption, CDialog* pwndTemplate, BOOL bAutoDestroyTpl, int idx, BOOL bAutoExpand)
{
	if (!pwndTemplate)    return -1;

	if (idx > 0 && idx >= m_PageList.size())    idx = -1;

	pwndTemplate->SetParent(this);

	//Insert Page
	return _InsertPage(caption, pwndTemplate, idx, bAutoDestroyTpl, bAutoExpand);
}

//---------------------------------------------------------------------------
// Function name	: InsertPage
// Description		: Called by InsertPage(...) methods
//					  Return -1 if an error occurs
//					  Make sure template had WS_CHILD style
//---------------------------------------------------------------------------
int CRollupCtrl::_InsertPage(LPCTSTR caption, CDialog* pwndTemplate, int idx, BOOL bAutoDestroyTpl, BOOL bAutoExpand)
{
	ASSERT(pwndTemplate != NULL);
	ASSERT(pwndTemplate->m_hWnd != NULL);

	//Get client rect
	CRect r;
	GetClientRect(r);

	//Create GroupBox
	CColorCtrl<CButton>* groupbox = new CColorCtrl<CButton>;
	groupbox->Create("", WS_CHILD | BS_GROUPBOX, r, this, 0);
	if (m_bkColor != 0)
	{
		groupbox->SetBkColor(m_bkColor);
	}

	//Create Button
	CXTPButton* but = new CXTPButton;
	//but->Create(caption, WS_CHILD|BS_AUTOCHECKBOX|BS_PUSHLIKE|BS_FLAT, r, this, 0 );
	but->Create(caption, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, r, this, 0);
	but->SetTheme(xtpButtonThemeOfficeXP);
	//but->SetFlatStyle(TRUE);
	//but->SetUseVisualStyle(FALSE);

	//Change Button's font
	HFONT hfont = (HFONT)gMFCFonts.hSystemFont;
	CFont* font = CFont::FromHandle(hfont);
	but->SetFont(font);

	//Add page at pagelist
	RC_PAGEINFO* pi = new RC_PAGEINFO;
	pi->id = m_lastId++;
	pi->bExpanded = FALSE;
	pi->bEnable = TRUE;
	pi->pwndTemplate = pwndTemplate;
	pi->pwndButton = but;
	pi->pwndGroupBox = groupbox;
	pi->pOldDlgProc = (WNDPROC)::GetWindowLongPtr(pwndTemplate->m_hWnd, DWLP_DLGPROC);
	pi->pOldButProc = (WNDPROC)::GetWindowLongPtr(but->m_hWnd, GWLP_WNDPROC);
	pi->bAutoDestroyTpl = bAutoDestroyTpl;

	int newidx;
	if (idx < 0)
	{
		m_PageList.push_back(pi);
		newidx = m_PageList.size() - 1;
	}
	else { m_PageList.insert(m_PageList.begin() + idx, pi); newidx = idx; }

	//Set Dlg Window datas
	::SetWindowLongPtr(pwndTemplate->m_hWnd, GWLP_USERDATA, (LONG_PTR)m_PageList[newidx]);
	::SetWindowLongPtr(pwndTemplate->m_hWnd, DWLP_USER, (LONG_PTR)this);

	//Set But Window data
	::SetWindowLongPtr(but->m_hWnd, GWLP_USERDATA, (LONG_PTR)m_PageList[newidx]);

	//SubClass Template window proc
	::SetWindowLongPtr(pwndTemplate->m_hWnd, DWLP_DLGPROC, (LONG_PTR)CRollupCtrl::DlgWindowProc);

	//SubClass Button window proc
	::SetWindowLongPtr(but->m_hWnd, GWLP_WNDPROC, (LONG_PTR)CRollupCtrl::ButWindowProc);

	// Override expanded flag based on settings in map.
	pi->bExpanded = stl::find_in_map(m_expandedMap, caption, bAutoExpand);

	RecalcHeight();
	RecalLayout();

	return pi->id;
}

//---------------------------------------------------------------------------
// Function name	: RemovePage
// Description		:
//---------------------------------------------------------------------------
void CRollupCtrl::RemovePage(int idx)
{
	if (!FindPage(idx))
		return;

	//Remove
	_RemovePage(idx);

	//Update
	RecalLayout();
}

//---------------------------------------------------------------------------
// Function name	: RemoveAllPages
// Description		:
//---------------------------------------------------------------------------
void CRollupCtrl::RemoveAllPages()
{
	//Remove all
	for (; m_PageList.size(); )
		_RemovePage(m_PageList[0]->id);

	//Update
	RecalLayout();
}

//---------------------------------------------------------------------------
// Function name	: _RemovePage
// Description		: Called by RemovePage or RemoveAllPages methods
//---------------------------------------------------------------------------
void CRollupCtrl::_RemovePage(int idx)
{
	// Find page
	RC_PAGEINFO* pi = FindPage(idx);

	//Remove page from array
	m_PageList.erase(m_PageList.begin() + FindPageIndex(idx));

	//Get Page Rect
	CRect tr;
	if (pi->pwndTemplate && pi->pwndTemplate->GetSafeHwnd() != NULL)
	{
		pi->pwndTemplate->GetWindowRect(&tr);
	}

	//Update PageHeight
	m_nPageHeight -= RC_PGBUTTONHEIGHT + (RC_GRPBOXINDENT * 2);
	if (pi->bExpanded)    m_nPageHeight -= tr.Height();

	//Remove wnds
	if (pi->pwndButton)       delete pi->pwndButton;
	if (pi->pwndGroupBox)     delete pi->pwndGroupBox;

	if (pi->pwndTemplate && pi->pwndTemplate->GetSafeHwnd() != NULL)
	{
		// fix
		::SetWindowLongPtr(pi->pwndTemplate->m_hWnd, DWLP_DLGPROC, (LONG_PTR)pi->pOldDlgProc);
		pi->pwndTemplate->DestroyWindow();
		if (pi->bAutoDestroyTpl)
			delete pi->pwndTemplate;
	}

	//Delete pageinfo
	delete pi;
}

//---------------------------------------------------------------------------
// Function name	: ExpandPage
// Description		:
//---------------------------------------------------------------------------
void CRollupCtrl::ExpandPage(int idx, BOOL bExpand, BOOL bScroll, BOOL bFromUI)
{
	if (!FindPage(idx))
		return;

	//Expand-collapse
	_ExpandPage(FindPage(idx), bExpand, bFromUI);

	//Update
	RecalLayout();

	//Scroll to this page (Automatic page visibility)
	if (bExpand && bScroll)
		ScrollToPage(idx, FALSE);

	if (GetOwner())
	{
		CRollupCtrlNotify n;
		n.hdr.hwndFrom = m_hWnd;
		n.hdr.idFrom = GetDlgCtrlID();
		n.hdr.code = ROLLUPCTRLN_EXPAND;
		n.nPageId = idx;
		n.bExpand = bExpand == TRUE;

		GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&n);
	}
}

//---------------------------------------------------------------------------
// Function name	: ExpandAllPages
// Description		:
//---------------------------------------------------------------------------
void CRollupCtrl::ExpandAllPages(BOOL bExpand, BOOL bFromUI)
{
	//Expand-collapse All
	for (int i = 0; i < m_PageList.size(); i++)
		_ExpandPage(m_PageList[i], bExpand, bFromUI);

	//Update
	RecalLayout();
}

//---------------------------------------------------------------------------
// Function name	: _ExpandPage
// Description		: Called by ExpandPage or ExpandAllPages methods
//---------------------------------------------------------------------------
void CRollupCtrl::_ExpandPage(RC_PAGEINFO* pi, BOOL bExpand, BOOL bFromUI)
{
	//Check if we need to change state
	if (pi->bExpanded == bExpand)         return;
	if (!pi->bEnable)             return;

	//Get Page Rect
	CRect tr;
	pi->pwndTemplate->GetWindowRect(&tr);

	//Expand-collapse
	pi->bExpanded = bExpand;

	CString caption;
	pi->pwndButton->GetWindowText(caption);
	if (bFromUI)
		m_expandedMap[caption] = pi->bExpanded == TRUE;

	if (bExpand)  m_nPageHeight += tr.Height();
	else      m_nPageHeight -= tr.Height();

}

//---------------------------------------------------------------------------
// Function name	: RenamePage
// Description		:
//---------------------------------------------------------------------------
void CRollupCtrl::RenamePage(int idx, const char* caption)
{
	if (!FindPage(idx))
		return;

	_RenamePage(FindPage(idx), caption);

	//Update
	RecalLayout();
}

//---------------------------------------------------------------------------
// Function name	: _RenamePage
// Description		: Change the text of the page's button
//---------------------------------------------------------------------------
void CRollupCtrl::_RenamePage(RC_PAGEINFO* pi, LPCTSTR caption)
{
	CString strCaption(caption);
	pi->pwndButton->SetWindowText(strCaption);

	m_expandedMap[strCaption] = pi->bExpanded;
}

//---------------------------------------------------------------------------
// Function name	: EnablePage
// Description		:
//---------------------------------------------------------------------------
void CRollupCtrl::EnablePage(int idx, BOOL bEnable)
{
	if (!FindPage(idx))
		return;

	//Enable-Disable
	_EnablePage(FindPage(idx), bEnable);

	//Update
	RecalLayout();
}

//---------------------------------------------------------------------------
// Function name	: EnableAllPages
// Description		:
//---------------------------------------------------------------------------
void CRollupCtrl::EnableAllPages(BOOL bEnable)
{
	//Enable-disable All
	for (int i = 0; i < m_PageList.size(); i++)
		_EnablePage(m_PageList[i], bEnable);

	//Update
	RecalLayout();
}

//---------------------------------------------------------------------------
// Function name	: _EnablePage
// Description		: Called by EnablePage or EnableAllPages methods
//---------------------------------------------------------------------------
void CRollupCtrl::_EnablePage(RC_PAGEINFO* pi, BOOL bEnable)
{
	//Check if we need to change state
	if (pi->bEnable == bEnable)   return;

	//Get Page Rect
	CRect tr;
	pi->pwndTemplate->GetWindowRect(&tr);

	//Change state
	pi->bEnable = bEnable;

	if (pi->bExpanded)  { m_nPageHeight -= tr.Height(); pi->bExpanded = FALSE; }
}

//---------------------------------------------------------------------------
// Function name	: ScrollToPage
// Description		: Scroll a page at the top of the RollupCtrl if bAtTheTop=TRUE
//					  or just ensure page visibility into view if bAtTheTop=FALSE
//---------------------------------------------------------------------------
void CRollupCtrl::ScrollToPage(int idx, BOOL bAtTheTop)
{
	if (!FindPage(idx))
		return;

	//Get page infos
	RC_PAGEINFO* pi = FindPage(idx);

	//Get windows rect
	CRect r;
	GetWindowRect(&r);
	CRect tr;
	pi->pwndTemplate->GetWindowRect(&tr);

	//Check page visibility
	if (bAtTheTop || ((tr.bottom > r.bottom) || (tr.top < r.top)))
	{
		//Compute new m_nStartYPos
		pi->pwndButton->GetWindowRect(&tr);
		m_nStartYPos -= (tr.top - r.top);

		//Update
		RecalLayout();
	}

}

//---------------------------------------------------------------------------
// Function name	: MovePageAt
// Description		: newidx can be equal to -1 (move at end)
//					  Return -1 if an error occurs
//---------------------------------------------------------------------------
int CRollupCtrl::MovePageAt(int id, int newidx)
{
	if (!FindPage(id)) return -1;
	int idx = FindPageIndex(id);
	if (idx == newidx) return -1;

	if (newidx > 0 && newidx >= m_PageList.size())    newidx = -1;

	//Remove page from its old position
	RC_PAGEINFO* pi = FindPage(id);
	m_PageList.erase(m_PageList.begin() + FindPageIndex(id));

	//Insert at its new position
	int retidx;
	if (newidx < 0)
	{
		m_PageList.push_back(pi);
		retidx = m_PageList.size() - 1;
	}
	else { m_PageList.insert(m_PageList.begin() + newidx, pi); retidx = newidx; }

	//Update
	RecalLayout();
	UpdateWindow();

	return retidx;
}

//---------------------------------------------------------------------------
// Function name	: IsPageExpanded
// Description		:
//---------------------------------------------------------------------------
BOOL CRollupCtrl::IsPageExpanded(int idx)
{
	if (!FindPage(idx))
		return FALSE;
	return FindPage(idx)->bExpanded;
}

//---------------------------------------------------------------------------
// Function name	: IsPageEnabled
// Description		:
//---------------------------------------------------------------------------
BOOL CRollupCtrl::IsPageEnabled(int idx)
{
	if (!FindPage(idx))
		return FALSE;
	return FindPage(idx)->bEnable;
}

//////////////////////////////////////////////////////////////////////////
void CRollupCtrl::RecalcHeight()
{
	//Add all pages with checked style for expanded ones
	m_nPageHeight = 0;
	for (int i = 0; i < m_PageList.size(); i++)
	{
		CRect rc;
		m_PageList[i]->pwndTemplate->GetWindowRect(rc);
		m_nPageHeight += RC_PGBUTTONHEIGHT + (RC_GRPBOXINDENT * 2);
		if (m_PageList[i]->bExpanded && m_PageList[i]->bEnable)
			m_nPageHeight += rc.Height() + 4;
	}
}

//---------------------------------------------------------------------------
// Function name	: RecalLayout
// Description		:
//---------------------------------------------------------------------------
void CRollupCtrl::RecalLayout()
{
	m_bRecalcLayout = true;

	//Check StartPosY
	CRect r;
	GetClientRect(&r);
	int BottomPagePos = m_nStartYPos + m_nPageHeight;

	if (BottomPagePos < r.Height())   m_nStartYPos = r.Height() - m_nPageHeight;
	if (m_nStartYPos > 0)         m_nStartYPos = 0;

	//Update layout
	HDWP hdwp = BeginDeferWindowPos(m_PageList.size() * 3); //*3 for pwndButton+pwndTemplate+pwndGroupBox

	int posy = m_nStartYPos;

	for (int i = 0; i < m_PageList.size(); i++)
	{
		RC_PAGEINFO* pi = m_PageList[i];

		//Enable-Disable Button
		((CXTPButton*)pi->pwndButton)->SetChecked(pi->bEnable & pi->bExpanded);
		pi->pwndButton->EnableWindow(pi->bEnable);

		//Expanded
		if (pi->bExpanded && pi->bEnable)
		{
			CRect tr;
			pi->pwndTemplate->GetWindowRect(&tr);

			//Update GroupBox position and size
			if (pi->pwndGroupBox)
				if (hdwp) hdwp = DeferWindowPos(hdwp, pi->pwndGroupBox->m_hWnd, 0, 2, posy, r.Width() - 3 - RC_SCROLLBARWIDTH, tr.Height() + RC_PGBUTTONHEIGHT + RC_GRPBOXINDENT - 4, SWP_NOZORDER | SWP_SHOWWINDOW);

			//Update Template position and size
			if (hdwp) hdwp = DeferWindowPos(hdwp, pi->pwndTemplate->m_hWnd, 0, RC_GRPBOXINDENT, posy + RC_PGBUTTONHEIGHT, r.Width() - RC_SCROLLBARWIDTH - (RC_GRPBOXINDENT * 2), tr.Height(), SWP_NOZORDER | SWP_SHOWWINDOW);

			//Update Button's position and size
			if (hdwp) hdwp = DeferWindowPos(hdwp, pi->pwndButton->m_hWnd, 0, RC_GRPBOXINDENT, posy, r.Width() - RC_SCROLLBARWIDTH - (RC_GRPBOXINDENT * 2), RC_PGBUTTONHEIGHT, SWP_NOZORDER | SWP_SHOWWINDOW);

			posy += tr.Height() + RC_PGBUTTONHEIGHT + 4;

			//Collapsed
		}
		else
		{

			//Update GroupBox position and size
			if (pi->pwndGroupBox)
				if (hdwp) hdwp = DeferWindowPos(hdwp, pi->pwndGroupBox->m_hWnd, 0, 2, posy, r.Width() - 3 - RC_SCROLLBARWIDTH, 16, SWP_NOZORDER | SWP_SHOWWINDOW);

			//Update Template position and size
			if (hdwp) hdwp = DeferWindowPos(hdwp, pi->pwndTemplate->m_hWnd, 0, RC_GRPBOXINDENT, 0, 0, 0, SWP_NOZORDER | SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE);

			//Update Button's position and size
			if (hdwp) hdwp = DeferWindowPos(hdwp, pi->pwndButton->m_hWnd, 0, RC_GRPBOXINDENT, posy, r.Width() - RC_SCROLLBARWIDTH - (RC_GRPBOXINDENT * 2), RC_PGBUTTONHEIGHT, SWP_NOZORDER | SWP_SHOWWINDOW);

			posy += RC_PGBUTTONHEIGHT;
		}

		posy += (RC_GRPBOXINDENT / 2);

	}
	if (hdwp)
		EndDeferWindowPos(hdwp);

	//Update Scroll Bar
	CRect br = CRect(r.right - RC_SCROLLBARWIDTH, r.top, r.right, r.bottom);
	InvalidateRect(&br, FALSE);
	//UpdateWindow();

	m_bRecalcLayout = false;
}

//---------------------------------------------------------------------------
// Function name	: GetPageIdxFromButtonHWND
// Description		: Return -1 if matching hwnd not found
//---------------------------------------------------------------------------
int CRollupCtrl::GetPageIdxFromButtonHWND(HWND hwnd)
{
	//Search matching button's hwnd
	for (int i = 0; i < m_PageList.size(); i++)
		if (hwnd == m_PageList[i]->pwndButton->m_hWnd)    return i;

	return -1;
}

//---------------------------------------------------------------------------
// Function name	: GetPageInfo
// Description		: Return -1 if an error occurs
//---------------------------------------------------------------------------
RC_PAGEINFO* CRollupCtrl::GetPageInfo(int idx)
{
	if (!FindPage(idx))
		return (RC_PAGEINFO*)0;

	return FindPage(idx);
}

//---------------------------------------------------------------------------
// Dialog SubClasser
//---------------------------------------------------------------------------
LRESULT CALLBACK CRollupCtrl::DlgWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RC_PAGEINFO* pi = (RC_PAGEINFO*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	CRollupCtrl* _this = (CRollupCtrl*)GetWindowLongPtr(hWnd, DWLP_USER);

	switch (uMsg)
	{
	case WM_SIZE:
		if (!_this->m_bRecalcLayout)
		{
			_this->RecalcHeight();
			_this->RecalLayout();
			_this->UpdateWindow();
		}
		return 0;
		break;

	case WM_CTLCOLOR:
		return (LRESULT)_this->m_bkColor;
		break;
	}

	CRect r;
	_this->GetClientRect(&r);
	if (_this->m_nPageHeight > r.Height())  //Can Scroll ?
	{

		switch (uMsg)
		{
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
			{
				CPoint pos;
				GetCursorPos(&pos);
				_this->m_nOldMouseYPos = pos.y;
				::SetCapture(hWnd);
				return 0;
			}

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
			{
				if (::GetCapture() == hWnd) { ::ReleaseCapture(); return 0; }
				break;
			}

		case WM_MOUSEMOVE:
			{
				if ((::GetCapture() == hWnd) && (wParam == MK_LBUTTON || wParam == MK_MBUTTON))
				{
					CPoint pos;
					GetCursorPos(&pos);
					_this->m_nStartYPos += (pos.y - _this->m_nOldMouseYPos);
					_this->RecalLayout();
					_this->m_nOldMouseYPos = pos.y;
					_this->UpdateWindow();
					return 0;
				}

				break;
			}

		case WM_SETCURSOR:
			if ((HWND)wParam == hWnd) { ::SetCursor(::LoadCursor(NULL, RC_ROLLCURSOR)); return TRUE; }
			break;

		}//switch(uMsg)
	}

	return ::CallWindowProc(pi->pOldDlgProc, hWnd, uMsg, wParam, lParam);
}

//---------------------------------------------------------------------------
// Button SubClasser
//---------------------------------------------------------------------------
LRESULT CALLBACK CRollupCtrl::ButWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_SETFOCUS)    return FALSE;

	RC_PAGEINFO* pi = (RC_PAGEINFO*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	return ::CallWindowProc(pi->pOldButProc, hWnd, uMsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////
// CRollupCtrl message handlers

//---------------------------------------------------------------------------
// OnCommand
//---------------------------------------------------------------------------
BOOL CRollupCtrl::OnCommand(WPARAM wParam, LPARAM lParam)
{
	//PopupMenu command ExpandAllPages
	if (LOWORD(wParam) == RC_IDM_EXPANDALL)      ExpandAllPages(TRUE, TRUE);
	else if (LOWORD(wParam) == RC_IDM_COLLAPSEALL)
		ExpandAllPages(FALSE, TRUE);

	//PopupMenu command ExpandPage
	else if (LOWORD(wParam) >= RC_IDM_STARTPAGES
	         && LOWORD(wParam) < RC_IDM_STARTPAGES + GetPagesCount())
	{
		int idx = LOWORD(wParam) - RC_IDM_STARTPAGES;
		int id = m_PageList[idx]->id;
		ExpandPage(id, !IsPageExpanded(id), TRUE, TRUE);
	}
	//Button command
	else if (HIWORD(wParam) == BN_CLICKED)
	{
		int idx = GetPageIdxFromButtonHWND((HWND)lParam);
		if (idx != -1)
		{
			RC_PAGEINFO* pi = m_PageList[idx];
			ExpandPage(pi->id, !pi->bExpanded, TRUE, TRUE);
			return 0;
		}
	}

	return CWnd::OnCommand(wParam, lParam);
}

//---------------------------------------------------------------------------
// OnPaint
//---------------------------------------------------------------------------
void CRollupCtrl::OnPaint()
{
	CPaintDC dc(this);

	//Draw ScrollBar
	CRect r;
	GetClientRect(&r);

	//m_clrBackground.SetColors(RGB(196, 200, 212), RGB(177, 179, 200));
	//clrBackground.SetColors(RGB(196, 200, 212), RGB(177, 179, 200));

	//XTPPaintManager()->GradientFill( &dc,r,RGB(196, 200, 212),RGB(177, 179, 200),FALSE );
	//XTPPaintManager()->GradientFill( &dc,r,GetXtremeColor(COLOR_3DFACE),GetXtremeColor(COLOR_3DHIGHLIGHT),FALSE );

	CRect br = CRect(r.right - RC_SCROLLBARWIDTH, r.top, r.right, r.bottom);
	dc.DrawEdge(&br, EDGE_RAISED, BF_RECT);

	int SB_Pos = 0;
	int SB_Size = 0;
	int ClientHeight = r.Height() - 4;

	if (m_nPageHeight > r.Height())
	{
		SB_Size = ClientHeight - (((m_nPageHeight - r.Height()) * ClientHeight) / m_nPageHeight);
		SB_Pos = -(m_nStartYPos * ClientHeight) / m_nPageHeight;
	}
	else
	{
		SB_Size = ClientHeight;
	}

	br.left += 2;
	br.right -= 1;
	br.top = SB_Pos + 2;
	br.bottom = br.top + SB_Size;

	//dc.FillSolidRect(&br, ::GetSysColor(COLOR_HIGHLIGHT));
	dc.FillSolidRect(&br, GetXtremeColor(COLOR_3DSHADOW));
	dc.FillSolidRect(CRect(br.left, 2, br.right, br.top), RGB(60, 60, 60));
	dc.FillSolidRect(CRect(br.left, br.bottom, br.right, 2 + ClientHeight), RGB(60, 60, 60));

	// Do not call CWnd::OnPaint() for painting messages
}

//---------------------------------------------------------------------------
// OnSize
//---------------------------------------------------------------------------
void CRollupCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	RecalLayout();
	UpdateWindow();
}

//---------------------------------------------------------------------------
// OnLButtonDown
//---------------------------------------------------------------------------
void CRollupCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	CRect r;
	GetClientRect(&r);
	if (m_nPageHeight <= r.Height())    return; //Can't Scroll

	CRect br = CRect(r.right - RC_SCROLLBARWIDTH, r.top, r.right, r.bottom);

	if ((nFlags & MK_LBUTTON) && br.PtInRect(point))
	{

		SetCapture();

		int ClientHeight = r.Height() - 4;

		int SB_Size = ClientHeight - (((m_nPageHeight - r.Height()) * ClientHeight) / m_nPageHeight);
		int SB_Pos = -(m_nStartYPos * ClientHeight) / m_nPageHeight;

		//Click inside scrollbar cursor
		if ((point.y < (SB_Pos + SB_Size)) && (point.y > SB_Pos))
		{

			m_nSBOffset = SB_Pos - point.y + 1;

			//Click outside scrollbar cursor (2 cases => above or below cursor)
		}
		else
		{
			int distup = point.y - SB_Pos;
			int distdown = (SB_Pos + SB_Size) - point.y;
			if (distup < distdown)  m_nSBOffset = 0; //above
			else          m_nSBOffset = -SB_Size;    //below
		}

		//Calc new m_nStartYPos from mouse pos
		int TargetPos = point.y + m_nSBOffset;
		m_nStartYPos = -(TargetPos * m_nPageHeight) / ClientHeight;

		//Update
		RecalLayout();
	}

	// we must set focus so we can scroll with the Wheel Of The Mouse
	SetFocus();

	CWnd::OnLButtonDown(nFlags, point);
}

//---------------------------------------------------------------------------
// OnLButtonUp
//---------------------------------------------------------------------------
void CRollupCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (GetCapture() == this)   ReleaseCapture();
	CWnd::OnLButtonUp(nFlags, point);
}

//---------------------------------------------------------------------------
// OnMouseMove
//---------------------------------------------------------------------------
void CRollupCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	CRect r;
	GetClientRect(&r);
	if (m_nPageHeight <= r.Height())    return; //Can't Scroll

	CRect br = CRect(r.right - RC_SCROLLBARWIDTH, r.top, r.right, r.bottom);

	if ((nFlags & MK_LBUTTON) && (GetCapture() == this))
	{

		//Calc new m_nStartYPos from mouse pos
		int ClientHeight = r.Height() - 4;
		int TargetPos = point.y + m_nSBOffset;
		m_nStartYPos = -(TargetPos * m_nPageHeight) / ClientHeight;

		//Update
		RecalLayout();
		UpdateWindow();
	}

	CWnd::OnMouseMove(nFlags, point);
}

//---------------------------------------------------------------------------
// OnMouseWheel
//---------------------------------------------------------------------------
BOOL CRollupCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CWnd* pFocusWnd = GetFocus();

	if (pFocusWnd)
	{
		// we should not scroll when the current focused control is a list, listview or a tree
		if (pFocusWnd->IsKindOf(RUNTIME_CLASS(CListCtrl)) ||
		    pFocusWnd->IsKindOf(RUNTIME_CLASS(CListCtrlEx)) ||
		    pFocusWnd->IsKindOf(RUNTIME_CLASS(CTreeCtrlReport)) ||
		    pFocusWnd->IsKindOf(RUNTIME_CLASS(CTreeCtrl)) ||
		    pFocusWnd->IsKindOf(RUNTIME_CLASS(CTreeCtrlEx)) ||
		    pFocusWnd->IsKindOf(RUNTIME_CLASS(CListBox)))
		{
			return FALSE;
		}
	}

	//Calc new m_nStartYPos
	m_nStartYPos += (zDelta / 4);

	//Update
	RecalLayout();
	UpdateWindow();

	return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}

//---------------------------------------------------------------------------
// OnMouseActivate
//---------------------------------------------------------------------------
int CRollupCtrl::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	//Timur
	//SetFocus();
	//Timur
	return CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);
}

//---------------------------------------------------------------------------
// OnContextMenu
//---------------------------------------------------------------------------
void CRollupCtrl::OnContextMenu(CWnd* pWnd, CPoint point)
{
	CMenu menu;
	if (menu.CreatePopupMenu())
	{
		menu.AppendMenu(MF_STRING, RC_IDM_EXPANDALL, "Expand all");
		menu.AppendMenu(MF_STRING, RC_IDM_COLLAPSEALL, "Collapse all");
		menu.AppendMenu(MF_SEPARATOR, 0, "");

		//Add all pages with checked style for expanded ones
		for (int i = 0; i < m_PageList.size(); i++)
		{
			CString cstrPageName;
			m_PageList[i]->pwndButton->GetWindowText(cstrPageName);
			menu.AppendMenu(MF_STRING, RC_IDM_STARTPAGES + i, cstrPageName);
			if (m_PageList[i]->bExpanded)
				menu.CheckMenuItem(RC_IDM_STARTPAGES + i, MF_CHECKED);
		}

		menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
	}
}

RC_PAGEINFO* CRollupCtrl::FindPage(int id)
{
	for (int i = 0; i < m_PageList.size(); i++)
	{
		if (m_PageList[i]->id == id)
		{
			return m_PageList[i];
		}
	}
	return 0;
}

int CRollupCtrl::FindPageIndex(int id)
{
	for (int i = 0; i < m_PageList.size(); i++)
	{
		if (m_PageList[i]->id == id)
		{
			return i;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
HBRUSH CRollupCtrl::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	/*
	   if (m_bkgBrush.GetSafeHandle())
	   {
	   return m_bkgBrush;
	   }
	 */
	return __super::OnCtlColor(pDC, pWnd, nCtlColor);
}

//////////////////////////////////////////////////////////////////////////
void CRollupCtrl::SetBkColor(COLORREF bkColor)
{
	m_bkColor = bkColor;
	if (m_bkgBrush.GetSafeHandle())
		m_bkgBrush.DeleteObject();
	m_bkgBrush.CreateSolidBrush(bkColor);
}

