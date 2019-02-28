// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "InPlaceEdit.h"

#include "Controls/SharedFonts.h"

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CInPlaceEdit, CEdit)
ON_WM_CREATE()
ON_WM_KILLFOCUS()
ON_WM_ERASEBKGND()
ON_WM_GETDLGCODE()
ON_CONTROL_REFLECT(EN_CHANGE, OnEnChange)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInPlaceEdit
CInPlaceEdit::CInPlaceEdit(const CString& srtInitText, OnChange onchange)
	: m_bUpdateOnKillFocus(true)
	, m_bUpdateOnEnChange(false)
{
	m_strInitText = srtInitText;
	m_onChange = onchange;
}

//////////////////////////////////////////////////////////////////////////
CInPlaceEdit::~CInPlaceEdit()
{
}

//////////////////////////////////////////////////////////////////////////
UINT CInPlaceEdit::OnGetDlgCode()
{
	UINT code = CEdit::OnGetDlgCode();
	code |= DLGC_WANTALLKEYS;
	return code;
}

//////////////////////////////////////////////////////////////////////////
void CInPlaceEdit::SetText(const CString& strText)
{
	m_strInitText = strText;

	SetWindowText(strText);
	SetSel(0, -1);
}

//////////////////////////////////////////////////////////////////////////
BOOL CInPlaceEdit::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_ESCAPE:
		case VK_RETURN:
			::PeekMessage(pMsg, NULL, NULL, NULL, PM_REMOVE);

		case VK_TAB:
		case VK_UP:
		case VK_DOWN:
			GetParent()->SetFocus();
			if (m_onChange)
				m_onChange();

			NMKEY nmkey;
			nmkey.hdr.code = NM_KEYDOWN;
			nmkey.hdr.hwndFrom = GetSafeHwnd();
			nmkey.hdr.idFrom = GetDlgCtrlID();
			nmkey.nVKey = pMsg->wParam;
			nmkey.uFlags = 0;
			if (GetOwner())
				GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmkey);

			return TRUE;
		default:
			;
		}
	}
	else if (pMsg->message == WM_MOUSEWHEEL)
	{
		if (GetOwner())
			GetOwner()->SendMessage(WM_MOUSEWHEEL, pMsg->wParam, pMsg->lParam);
		return TRUE;
	}

	return CEdit::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////
// CInPlaceEdit message handlers

int CInPlaceEdit::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CEdit::OnCreate(lpCreateStruct) == -1)
		return -1;

	CFont* pFont = GetParent()->GetFont();
	SetFont(pFont);

	SetWindowText(m_strInitText);

	return 0;
}

void CInPlaceEdit::OnKillFocus(CWnd* pNewWnd)
{
	CEdit::OnKillFocus(pNewWnd);
	if (m_onChange && m_bUpdateOnKillFocus)
		m_onChange();
}

BOOL CInPlaceEdit::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

void CInPlaceEdit::OnEnChange()
{
	if (m_bUpdateOnEnChange && m_onChange)
		m_onChange();
}

