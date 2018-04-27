// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NumberCtrl.h"
#include "Util/EditorUtils.h"
#include "UserMessageDefines.h"
#include "Controls/SharedFonts.h"
#include "IUndoManager.h"

inline double Round(double fVal, double fStep)
{
	if (fStep > 0.f)
		fVal = int_round(fVal / fStep) * fStep;
	return fVal;
}

namespace NumberCtrl_Private
{
	inline void FormatFloatForUICString(CString& outstr, int significantDigits, double value)
	{
		string crystr = outstr.GetString();
		FormatFloatForUI(crystr, significantDigits, value);
		outstr = crystr.GetString();
	}
}

/////////////////////////////////////////////////////////////////////////////
// CNumberCtrl

CNumberCtrl::CNumberCtrl()
{
	m_btnStatus = 0;
	m_btnWidth = 10;
	m_draggin = false;
	m_value = 0;
	m_min = -FLT_MAX;
	m_max = FLT_MAX;
	m_step = 0.01;
	m_enabled = true;
	m_noNotify = false;
	m_integer = false;
	m_nFlags = 0;
	m_bUndoEnabled = false;
	m_bDragged = false;
	m_multiplier = 1;
	m_bLocked = false;
	m_bInitialized = false;
	m_bBlackArrows = false;
	m_bForceModified = false;
	m_bInitializedValue = false;

	m_upArrow = 0;
	m_downArrow = 0;

	m_floatFormatPrecision = 7;
}

CNumberCtrl::~CNumberCtrl()
{
	m_bInitialized = false;
}

BEGIN_MESSAGE_MAP(CNumberCtrl, CWnd)
//{{AFX_MSG_MAP(CNumberCtrl)
ON_WM_CREATE()
ON_WM_PAINT()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_ENABLE()
ON_WM_SETFOCUS()
ON_EN_SETFOCUS(IDC_EDIT, OnEditSetFocus)
ON_EN_KILLFOCUS(IDC_EDIT, OnEditKillFocus)

//}}AFX_MSG_MAP
ON_WM_SIZE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNumberCtrl message handlers

void CNumberCtrl::Create(CWnd* parentWnd, CRect& rc, UINT nID, int nFlags)
{
	m_nFlags = nFlags;
	HCURSOR arrowCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	CreateEx(0, AfxRegisterWndClass(NULL, arrowCursor, NULL /*(HBRUSH)GetStockObject(LTGRAY_BRUSH)*/), NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP, rc, parentWnd, nID);
}

void CNumberCtrl::Create(CWnd* parentWnd, UINT ctrlID, int flags)
{
	ASSERT(parentWnd);
	m_nFlags = flags;
	CRect rc;
	CWnd* ctrl = parentWnd->GetDlgItem(ctrlID);
	if (ctrl)
	{
		ctrl->SetDlgCtrlID(ctrlID + 10000);
		ctrl->ShowWindow(SW_HIDE);
		ctrl->GetWindowRect(rc);
		parentWnd->ScreenToClient(rc);
	}

	HCURSOR arrowCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	CreateEx(0, AfxRegisterWndClass(NULL, arrowCursor, NULL /*(HBRUSH)GetStockObject(LTGRAY_BRUSH)*/), NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP, rc, parentWnd, ctrlID);
}

int CNumberCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_upDownCursor = AfxGetApp()->LoadStandardCursor(IDC_SIZENS);
	LoadIcons();

	CRect rc;
	GetClientRect(rc);

	if (m_nFlags & LEFTARROW)
	{
		rc.left += m_btnWidth + 3;
	}
	else
	{
		rc.right -= m_btnWidth + 1;
	}

	DWORD nFlags = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
	if (m_nFlags & LEFTALIGN)
		nFlags |= ES_LEFT;
	else if (m_nFlags & CENTER_ALIGN)
		nFlags |= ES_CENTER;
	else
		nFlags |= ES_RIGHT;

	DWORD nExFlags = 0;
	if (!(m_nFlags & NOBORDER))
		nExFlags |= WS_EX_CLIENTEDGE;

	//m_edit.Create( WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_BORDER|ES_RIGHT|ES_AUTOHSCROLL,rc,this,IDC_EDIT );
	//m_edit.CreateEx( nFlags,rc,this,IDC_EDIT );

	m_edit.CreateEx(nExFlags, _T("EDIT"), "", nFlags, rc, this, IDC_EDIT);

	//	m_edit.Create( WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_BORDER|ES_LEFT|ES_AUTOHSCROLL,rc,this,IDC_EDIT );
	m_edit.SetFont(CFont::FromHandle((HFONT)gMFCFonts.hSystemFont));
	m_edit.SetUpdateCallback(functor(*this, &CNumberCtrl::OnEditChanged));

	//if (!(m_nFlags & NOBORDER))
	//m_edit.ModifyStyleEx( 0,WS_EX_CLIENTEDGE );

	double val = m_value;
	m_value = val + 1;
	SetInternalValue(val);

	m_bInitialized = true;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::LoadIcons()
{
	if (m_upArrow)
		DestroyIcon(m_upArrow);
	if (m_downArrow)
		DestroyIcon(m_downArrow);

	m_upArrow = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_UP_ARROW), IMAGE_ICON, 5, 5, LR_DEFAULTCOLOR | LR_SHARED);
	m_downArrow = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_DOWN_ARROW), IMAGE_ICON, 5, 5, LR_DEFAULTCOLOR | LR_SHARED);
	m_bBlackArrows = false;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::SetLeftAlign(bool left)
{
	if (m_edit)
	{
		if (left)
			m_edit.ModifyStyle(ES_RIGHT, ES_LEFT);
		else
			m_edit.ModifyStyle(ES_LEFT, ES_RIGHT);
	}
}

void CNumberCtrl::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	DrawButtons(dc);

	if (m_bBlackArrows)
	{
		LoadIcons();
	}

	// Do not call CWnd::OnPaint() for painting messages
}

void CNumberCtrl::DrawButtons(CDC& dc)
{
	CRect rc;
	GetClientRect(rc);

	if (!m_enabled)
	{
		//dc.FillRect( rc,CBrush::FromHandle((HBRUSH)GetStockObject(LTGRAY_BRUSH)) );
		dc.FillSolidRect(rc, GetSysColor(COLOR_3DFACE));
		return;
	}
	int x = 0;
	if (!(m_nFlags & LEFTARROW))
	{
		x = rc.right - m_btnWidth;
	}
	int y = rc.top;
	int w = m_btnWidth;
	int h = rc.bottom;
	int h2 = h / 2;
	COLORREF hilight = RGB(130, 130, 130);
	COLORREF shadow = RGB(100, 100, 100);
	//dc.Draw3dRect( x,y,w,h/2-1,hilight,shadow );
	//dc.Draw3dRect( x,y+h/2+1,w,h/2-1,hilight,shadow );

	int smallOfs = 0;
	if (rc.bottom < 18)
		smallOfs = 1;

	//dc.FillRect()
	//dc.SelectObject(b1);
	//dc.Rectangle( x,y, x+w,y+h2 );
	//dc.SelectObject(b2);
	//dc.Rectangle( x,y+h2, x+w,y+h );

	if (m_btnStatus == 1 || m_btnStatus == 3)
	{
		dc.Draw3dRect(x, y, w, h2, shadow, hilight);
	}
	else
	{
		dc.Draw3dRect(x, y, w, h2, hilight, shadow);
		//DrawIconEx( dc,x+1,y+2,m_upArrow,5,5,0,0,DI_NORMAL );
	}

	if (m_btnStatus == 2 || m_btnStatus == 3)
		dc.Draw3dRect(x, y + h2 + 1, w, h2 - 1 + smallOfs, shadow, hilight);
	else
		dc.Draw3dRect(x, y + h2 + 1, w, h2 - 1 + smallOfs, hilight, shadow);

	DrawIconEx(dc, x + 2, y + 2, m_upArrow, 5, 5, 0, 0, DI_NORMAL);

	DrawIconEx(dc, x + 2, y + h2 + 3 - smallOfs, m_downArrow, 5, 5, 0, 0, DI_NORMAL);
}

void CNumberCtrl::GetBtnRect(int btn, CRect& rc)
{
	CRect rcw;
	GetClientRect(rcw);

	int x = 0;
	if (!(m_nFlags & LEFTARROW))
	{
		x = rcw.right - m_btnWidth;
	}
	int y = rcw.top;
	int w = m_btnWidth;
	int h = rcw.bottom;
	int h2 = h / 2;

	if (btn == 0)
	{
		rc.SetRect(x, y, x + w, y + h2);
	}
	else if (btn == 1)
	{
		rc.SetRect(x, y + h2 + 1, x + w, y + h);
	}
}

int CNumberCtrl::GetBtn(CPoint point)
{
	for (int i = 0; i < 2; i++)
	{
		CRect rc;
		GetBtnRect(i, rc);
		if (rc.PtInRect(point))
		{
			return i;
		}
	}
	return -1;
}

void CNumberCtrl::SetBtnStatus(int s)
{
	m_btnStatus = s;
	RedrawWindow();
}

void CNumberCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_enabled)
		return;

	m_btnStatus = 0;
	int btn = GetBtn(point);
	if (btn >= 0)
	{
		SetBtnStatus(btn + 1);
		m_bDragged = false;

		// Start undo.
		if (m_bUndoEnabled && !CUndo::IsRecording())
			GetIEditor()->GetIUndoManager()->Begin();

		CWnd* parent = GetOwner();
		if (parent)
		{
			::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), EN_BEGIN_DRAG), (LPARAM)GetSafeHwnd());
		}
	}
	m_mousePos = point;
	SetCapture();
	m_draggin = true;

	CWnd* parent = GetOwner();
	if (parent)
	{
		::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), WMU_LBUTTONDOWN), (LPARAM)GetSafeHwnd());
	}
	CWnd::OnLButtonDown(nFlags, point);
}

void CNumberCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_enabled)
		return;

	bool bLButonDown = false;
	if (GetCapture() == this)
	{
		bLButonDown = true;
		ReleaseCapture();
	}
	SetBtnStatus(0);

	bool bSendEndDrag = false;
	if (m_draggin)
		bSendEndDrag = true;

	m_draggin = false;

	//if (m_endUpdateCallback)
	//m_endUpdateCallback(this);

	if (bLButonDown)
	{
		int btn = GetBtn(point);
		if (!m_bDragged && btn >= 0 && m_step > 0.f)
		{
			// First round to nearest step.
			double prevValue = m_value;

			if (btn == 0)
				SetInternalValue(Round(GetInternalValue() + m_step, m_step));
			else if (btn == 1)
				SetInternalValue(Round(GetInternalValue() - m_step, m_step));

			if (prevValue != m_value)
				NotifyUpdate(false, true);
		}
		else if (m_bDragged)
		{
			// Send last non tracking update after tracking.
			NotifyUpdate(false, true);
		}
	}

	if (m_bUndoEnabled && CUndo::IsRecording())
		GetIEditor()->GetIUndoManager()->Accept(m_undoText.GetString());

	CWnd* parent = GetOwner();
	if (parent)
	{
		::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), WMU_LBUTTONUP), (LPARAM)GetSafeHwnd());
		if (bSendEndDrag)
			::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), EN_END_DRAG), (LPARAM)GetSafeHwnd());
	}

	///CWnd::OnLButtonUp(nFlags, point);
}

void CNumberCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_enabled)
		return;

	if (point == m_mousePos)
		return;

	if (m_draggin)
	{
		m_bDragged = true;
		double prevValue = m_value;

		SetCursor(m_upDownCursor);
		if (m_btnStatus != 3)
			SetBtnStatus(3);

		int y = point.y - m_mousePos.y;
		if (!m_integer)
			// Floating control.
			y *= abs(y);
		SetInternalValue(Round(GetInternalValue() - m_step * y, m_step));

		// Make sure Edit control is updated here immidietly.
		m_edit.UpdateWindow();

		CPoint cp;
		GetCursorPos(&cp);
		int sX = GetSystemMetrics(SM_CXSCREEN);
		int sY = GetSystemMetrics(SM_CYSCREEN);

		if (cp.y < 20 || cp.y > sY - 20)
		{
			// When near end of screen, prevent cursor from moving.
			CPoint p = m_mousePos;
			ClientToScreen(&p);
			SetCursorPos(p.x, p.y);
		}
		else
		{
			m_mousePos = point;
		}

		if (prevValue != m_value)
			NotifyUpdate(true, true);
	}

	CWnd::OnMouseMove(nFlags, point);
}

void CNumberCtrl::SetRange(double min, double max)
{
	if (m_bLocked)
		return;
	m_min = min;
	m_max = max;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::SetInternalValue(double val)
{
	if (m_bLocked)
		return;
	CString s;

	if (val < m_min) val = m_min;
	if (val > m_max) val = m_max;

	if (m_integer)
	{
		if (int_round(m_value) == int_round(val))
			return;
		s.Format("%d", int_round(val));
	}
	else
	{
		NumberCtrl_Private::FormatFloatForUICString(s, m_floatFormatPrecision, val);
	}
	m_value = val;

	if (m_edit)
	{
		m_noNotify = true;
		m_edit.SetText(s);
		m_noNotify = false;
	}
}

//////////////////////////////////////////////////////////////////////////
double CNumberCtrl::GetInternalValue() const
{
	if (!m_enabled)
		return m_value;

	double v;

	if (m_edit)
	{
		CString str;
		m_edit.GetWindowText(str);
		v = m_value = atof((const char*)str);
	}
	else
		v = m_value;

	if (v < m_min) v = m_min;
	if (v > m_max) v = m_max;

	return v;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::SetValue(double val)
{
	if (m_bLocked)
		return;
	SetInternalValue(val * m_multiplier);
	if (m_bInitializedValue == false)
	{
		m_lastUpdateValue = m_value;
		m_bInitializedValue = true;
	}
}

//////////////////////////////////////////////////////////////////////////
double CNumberCtrl::GetValue() const
{
	return GetInternalValue() / m_multiplier;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::SetStep(double step)
{
	if (m_bLocked)
		return;
	m_step = step;
	if (m_integer && m_step < 1)
		m_step = 1;
}

//////////////////////////////////////////////////////////////////////////
double CNumberCtrl::GetStep() const
{
	return m_step;
}

//////////////////////////////////////////////////////////////////////////
CString CNumberCtrl::GetValueAsString() const
{
	CString str;
	if (fabs(m_multiplier) > 0.000001)
	{
		NumberCtrl_Private::FormatFloatForUICString(str, m_floatFormatPrecision, m_value / m_multiplier);
	}
	else
	{
		NumberCtrl_Private::FormatFloatForUICString(str, m_floatFormatPrecision, 1.0f);
	}
	return str;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::OnEditChanged()
{
	static bool inUpdate = false;

	if (!inUpdate)
	{
		double prevValue = m_value;

		double v = GetInternalValue();
		if (v != m_value)
		{
			SetInternalValue(v);
		}

		bool changed = prevValue != m_value;
		if (changed || m_bForceModified)
			NotifyUpdate(false, changed);

		inUpdate = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::OnEnable(BOOL bEnable)
{
	CWnd::OnEnable(bEnable);

	m_enabled = (bEnable == TRUE);
	if (m_edit)
	{
		m_edit.EnableWindow(bEnable);
		RedrawWindow();
	}
}

void CNumberCtrl::NotifyUpdate(bool tracking, bool valueChanged)
{
	if (m_noNotify)
		return;

	CWnd* parent = GetOwner();

	bool bUndoRecording = CUndo::IsRecording();
	bool bSaveUndo = (!tracking && valueChanged && m_bUndoEnabled && !bUndoRecording);

	if (bSaveUndo)
		GetIEditor()->GetIUndoManager()->Begin();

	if (tracking && m_bUndoEnabled && bUndoRecording)
	{
		m_bLocked = true;
		GetIEditor()->GetIUndoManager()->Restore(); // This can potentially modify m_value;
		m_bLocked = false;
	}

	if (m_updateCallback)
		m_updateCallback(this);

	if (parent)
	{
		::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), EN_UPDATE), (LPARAM)GetSafeHwnd());
		if (!tracking)
		{
			::SendMessage(parent->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), EN_CHANGE), (LPARAM)GetSafeHwnd());
		}
	}
	m_lastUpdateValue = m_value;

	if (bSaveUndo)
		GetIEditor()->GetIUndoManager()->Accept(m_undoText);

}

void CNumberCtrl::OnSetFocus(CWnd* pOldWnd)
{
	CWnd::OnSetFocus(pOldWnd);

	if (m_edit)
	{
		m_edit.SetFocus();
		m_edit.SetSel(0, -1);
	}
}

void CNumberCtrl::SetInteger(bool enable)
{
	m_integer = enable;
	m_step = 1;
	double f = GetInternalValue();
	m_value = f + 1;
	SetInternalValue(f);
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::OnEditSetFocus()
{
	if (!m_bInitialized)
		return;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::OnEditKillFocus()
{
	if (!m_bInitialized)
		return;
	//if (m_bUpdating && m_endUpdateCallback)
	//m_endUpdateCallback(this);

	if (m_value != m_lastUpdateValue)
		NotifyUpdate(false, true);
}

//////////////////////////////////////////////////////////////////////////
// calculate the digits right from the comma
int CNumberCtrl::CalculateDecimalPlaces(double infNumber, int iniMaxPlaces)
{
	assert(iniMaxPlaces >= 0);

	char str[256], * _str = str;

	cry_sprintf(str, "%f", infNumber);

	while (*_str != '.')
		_str++;                                     // search the comma

	int ret = 0;

	if (*_str != 0)                                  // comma?
	{
		int cur = 1;

		_str++;                                     // jump over comma

		while (*_str >= '0' && *_str <= '9')
		{
			if (*_str != '0') ret = cur;

			_str++;
			cur++;
		}

		if (ret > iniMaxPlaces) ret = iniMaxPlaces;       // bound to maximum
	}

	return(ret);
}

void CNumberCtrl::SetInternalPrecision(int iniDigits)
{
	m_step = pow(10.f, -iniDigits);
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::EnableUndo(const CString& undoText)
{
	m_undoText = undoText;
	m_bUndoEnabled = true;
}

void CNumberCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	if (m_edit.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		if (m_nFlags & LEFTARROW)
		{
			rc.left += m_btnWidth + 3;
		}
		else
		{
			rc.right -= m_btnWidth + 1;
		}
		m_edit.MoveWindow(rc, FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::SetFont(CFont* pFont, BOOL bRedraw)
{
	CWnd::SetFont(pFont, bRedraw);
	if (m_edit.m_hWnd)
		m_edit.SetFont(pFont, bRedraw);
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::SetMultiplier(double fMultiplier)
{
	if (fMultiplier != 0)
		m_multiplier = fMultiplier;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::SetFloatFormatPrecision(int significantDigits)
{
	m_floatFormatPrecision = significantDigits;
}

//////////////////////////////////////////////////////////////////////////
void CNumberCtrl::SetSwallowReturn(bool bDoSwallow)
{
	m_edit.SetSwallowReturn(bDoSwallow);
}

//////////////////////////////////////////////////////////////////////////
BOOL CNumberCtrl::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (CWnd::OnNotify(wParam, lParam, pResult))
		return TRUE;

	// route commands from the edit box to the parent.
	if (GetOwner() && wParam == IDC_EDIT)
	{
		NMHDR* pNHDR = (NMHDR*)lParam;
		pNHDR->hwndFrom = GetSafeHwnd();
		pNHDR->idFrom = GetDlgCtrlID();
		*pResult = GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), lParam);
	}
	return TRUE;
}

void CNumberCtrl::EnableNotifyWithoutValueChange(bool bFlag)
{
	m_bForceModified = bFlag;
	m_edit.EnableUpdateOnKillFocus(!bFlag);
}

