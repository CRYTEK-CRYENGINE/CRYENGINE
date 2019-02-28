// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SliderCtrlEx.h"

#define FLOAT_SCALE 100

IMPLEMENT_DYNAMIC(CSliderCtrlEx, CSliderCtrl)
IMPLEMENT_DYNAMIC(CSliderCtrlCustomDraw, CSliderCtrlEx)

BEGIN_MESSAGE_MAP(CSliderCtrlEx, CSliderCtrl)
ON_WM_LBUTTONDOWN()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CSliderCtrlEx::CSliderCtrlEx()
{
	m_bDragging = false;
	m_bDragChanged = false;

	m_value = 0;
	m_min = 0;
	m_max = 100;
	m_noNotify = false;
	m_integer = false;
	m_bUndoEnabled = false;
	m_bDragged = false;
	m_bUndoStarted = false;
	m_bLocked = false;
	m_bInNotifyCallback = false;
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlEx::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_bUndoStarted = false;
	// Start undo.
	if (m_bUndoEnabled)
	{
		GetIEditor()->GetIUndoManager()->Begin();
		m_bUndoStarted = true;
	}

	m_bDragging = true;
	m_bDragChanged = false;
	SetCapture();
	SetFocus();
	if (SetThumb(point))
	{
		m_bDragChanged = true;
		PostMessageToParent(TB_THUMBTRACK);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlEx::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		if (SetThumb(point))
		{
			m_bDragChanged = true;
			PostMessageToParent(TB_THUMBTRACK);
		}
	}
	else
	{
		CSliderCtrl::OnMouseMove(nFlags, point);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlEx::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		::ReleaseCapture();
		if (SetThumb(point))
		{
			PostMessageToParent(TB_THUMBTRACK);
			m_bDragChanged = true;
		}
		if (m_bDragChanged)
		{
			PostMessageToParent(TB_THUMBPOSITION);
			m_bDragChanged = false;
		}
		m_bDragging = false;

		if (m_bUndoStarted)
		{
			if (CUndo::IsRecording())
				GetIEditor()->GetIUndoManager()->Accept(m_undoText.GetString());
			m_bUndoStarted = false;
		}
	}
	else
	{
		CSliderCtrl::OnLButtonUp(nFlags, point);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSliderCtrlEx::SetThumb(const CPoint& point)
{
	const int nMin = GetRangeMin();
	const int nMax = GetRangeMax() + 1;
	CRect rc;
	GetChannelRect(rc);
	double dPos;
	double dCorrectionFactor = 0.0;
	if (GetStyle() & TBS_VERT)
	{
		// note: there is a bug in GetChannelRect, it gets the orientation of the rectangle mixed up
		dPos = (double)(point.y - rc.left) / (rc.right - rc.left);
	}
	else
	{
		dPos = (double)(point.x - rc.left) / (rc.right - rc.left);
	}
	// This correction factor is needed when you click inbetween tick marks
	// so that the thumb will move to the nearest one
	dCorrectionFactor = 0.5 * (1 - dPos) - 0.5 * dPos;
	int nNewPos = (int)(nMin + (nMax - nMin) * dPos + dCorrectionFactor);
	const bool bChanged = (nNewPos != GetPos());
	if (bChanged)
	{
		SetPos(nNewPos);
		float dt = ((float)(nNewPos - nMin) / (nMax - nMin));
		SetValue(dt * (m_max - m_min) + m_min);
	}
	return bChanged;
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlEx::PostMessageToParent(const int nTBCode)
{
	if (m_noNotify)
		return;

	m_bInNotifyCallback = true;

	if (nTBCode == TB_THUMBTRACK && m_bUndoEnabled && CUndo::IsRecording())
	{
		m_bLocked = true;
		GetIEditor()->GetIUndoManager()->Restore();
		m_bLocked = false;
	}

	if (m_updateCallback)
		m_updateCallback(this);

	m_lastUpdateValue = m_value;

	CWnd* pWnd = GetParent();
	if (pWnd)
	{
		int nMsg = (GetStyle() & TBS_VERT) ? WM_VSCROLL : WM_HSCROLL;
		pWnd->PostMessage(nMsg, (WPARAM)((GetPos() << 16) | nTBCode), (LPARAM)GetSafeHwnd());
	}

	m_bInNotifyCallback = false;
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlEx::EnableUndo(const CString& undoText)
{
	m_undoText = undoText;
	m_bUndoEnabled = true;
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlEx::SetRangeFloat(float min, float max, float step)
{
	if (m_bLocked)
		return;
	m_min = min;
	m_max = max;

	// Set internal slider range.
	float range = FLOAT_SCALE;
	if (step != 0.f)
		range = pow(0.1f, floor(log(step) / log(10.f)));
	SetRange(int_round(min * range), int_round(max * range));

	if (m_hWnd && !m_bInNotifyCallback)
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlEx::SetValue(float val)
{
	if (m_bLocked)
		return;
	m_value = val;
	if (m_value < m_min)
		m_value = m_min;
	if (m_value > m_max)
		m_value = m_max;

	if (!m_bDragging)
	{
		const int nMin = GetRangeMin();
		const int nMax = GetRangeMax() + 1;
		float pos = (m_value - m_min) / (m_max - m_min) * (nMax - nMin) + nMin;
		SetPos(int_round(pos));
	}

	if (m_hWnd && !m_bInNotifyCallback)
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
float CSliderCtrlEx::GetValue() const
{
	return m_value;
}

//////////////////////////////////////////////////////////////////////////
CString CSliderCtrlEx::GetValueAsString() const
{
	CString str;
	str.Format("%g", m_value);
	return str;
}

//////////////////////////////////////////////////////////////////////////
// CSliderCtrlCustomDraw
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CSliderCtrlCustomDraw, CSliderCtrlEx)
ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
END_MESSAGE_MAP()

void CSliderCtrlCustomDraw::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMCUSTOMDRAW* pCD = (NMCUSTOMDRAW*)pNMHDR;
	switch (pCD->dwDrawStage)
	{
	case CDDS_POSTERASE:  // After the erasing cycle is complete.
		*pResult = CDRF_DODEFAULT;
		break;
	case CDDS_POSTPAINT:  // After the painting cycle is complete.
		*pResult = CDRF_DODEFAULT;
		break;
	case CDDS_PREERASE:   // Before the erasing cycle begins.
		*pResult = CDRF_DODEFAULT;
		break;
	case CDDS_PREPAINT:   // Before the painting cycle begins.
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPOSTPAINT:  // After an item is drawn.
		DrawTicks(CDC::FromHandle(pCD->hdc));
		*pResult = CDRF_DODEFAULT;
		break;
	case CDDS_ITEMPREPAINT: // Before an item is drawn.
		switch (pCD->dwItemSpec)
		{
		case TBCD_CHANNEL:
			m_channelRc = pCD->rc;
			*pResult = CDRF_NOTIFYPOSTPAINT;
			break;
		case TBCD_TICS:
			//DrawTicks( CDC::FromHandle(pCD->hdc) );
			*pResult = CDRF_SKIPDEFAULT;
			break;
		case TBCD_THUMB:
			*pResult = CDRF_DODEFAULT;
			break;
		default:
			*pResult = CDRF_DODEFAULT;
			break;
		}
		break;
	default:
		*pResult = CDRF_DODEFAULT;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
int CSliderCtrlCustomDraw::ValueToPos(int x)
{
	CRect trc;
	GetThumbRect(trc);
	double pixelsPerStep = (double)(m_channelRc.Width() - trc.Width()) / (GetRangeMax() - GetRangeMin());
	return m_channelRc.left + pixelsPerStep * (x - GetRangeMin()) + trc.Width() / 2;
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlCustomDraw::DrawTick(CDC* pDC, int x, bool bMajor)
{
	int s = (bMajor) ? 10 : 6;
	pDC->MoveTo(x, m_channelRc.bottom + 4);
	pDC->LineTo(x, m_channelRc.bottom + s);
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlCustomDraw::DrawTicks(CDC* pDC)
{
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
	CPen* pPrevPen = pDC->SelectObject(&pen);

	int sel1 = 0;
	int sel2 = 0;
	if (m_selMax != m_selMin)
	{
		sel1 = ValueToPos(m_selMin);
		sel2 = ValueToPos(m_selMax);
	}
	if (sel1 != sel2)
	{
		if (m_selMin == 0 && m_selMax > 0)
		{
			XTPPaintManager()->GradientFill(pDC, CRect(sel1, m_channelRc.top + 1, sel2, m_channelRc.bottom - 1),
			                                RGB(0, 160, 0), RGB(100, 255, 100), TRUE);
		}
		else if (m_selMax == 0 && m_selMin < 0)
		{
			XTPPaintManager()->GradientFill(pDC, CRect(sel1, m_channelRc.top + 1, sel2, m_channelRc.bottom - 1),
			                                RGB(255, 100, 100), RGB(160, 0, 0), TRUE);
		}
		else
			pDC->Rectangle(CRect(sel1, m_channelRc.top + 1, sel2, m_channelRc.bottom - 1));
	}

	if (GetStyle() & TBS_AUTOTICKS)
	{
		bool bTop = ((GetStyle() & TBS_BOTH) == TBS_BOTH) || ((GetStyle() & TBS_TOP) == TBS_TOP);
		bool bBottom = ((GetStyle() & TBS_BOTH) == TBS_BOTH) || ((GetStyle() & TBS_BOTTOM) == TBS_BOTTOM);

		int r0 = GetRangeMin();
		int r1 = GetRangeMax();
		if (m_tickFreq < 1)
			m_tickFreq = 1;
		for (int i = r0; i < r1; i += m_tickFreq)
		{
			if (bBottom)
			{
				int x = ValueToPos(i);
				DrawTick(pDC, x);
			}
		}
		if (bBottom)
		{
			if (GetRangeMin() < 0 && GetRangeMax() > 0)
			{
				// Draw tick at 0.
				DrawTick(pDC, ValueToPos(0), true);
			}
			DrawTick(pDC, ValueToPos(GetRangeMin()), true);
			DrawTick(pDC, ValueToPos(GetRangeMax()), true);
		}
	}

	pDC->SelectObject(pPrevPen);
}

//////////////////////////////////////////////////////////////////////////
void CSliderCtrlCustomDraw::SetSelection(int nMin, int nMax)
{
	if (nMin != m_selMin || nMax != m_selMax)
	{
		m_selMin = nMin;
		m_selMax = nMax;
		InvalidateRect(m_channelRc);

		// Simulate resize of the window, to force redraw of the control subitem.
		CRect rc;
		GetClientRect(rc);
		int cx = rc.Width();
		int cy = rc.Height();
		SendMessage(WM_SIZE, (WPARAM)SIZE_RESTORED, MAKELPARAM(cx, cy));

		//		CRect rc;
		GetWindowRect(rc);
		GetParent()->ScreenToClient(rc);
		rc.right += 1;
		//MoveWindow(rc,FALSE);
		rc.right -= 1;
		//MoveWindow(rc,FALSE);
	}
}

