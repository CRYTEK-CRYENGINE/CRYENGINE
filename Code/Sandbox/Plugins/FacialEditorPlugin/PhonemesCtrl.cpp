// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PhonemesCtrl.h"
#include "Controls/MemDC.h"
#include "Controls/SharedFonts.h"
#include "IUndoManager.h"
#include "IUndoObject.h"

#include <CryAnimation/IFacialAnimation.h>
#include <CryAnimation/ICryAnimation.h>

#include "Resource.h"

IMPLEMENT_DYNAMIC(CPhonemesCtrl, CWnd)

#define MIN_TIME_EPSILON 0.01f

class CUndoPhonemeChange : public IUndoObject
{
public:
	CUndoPhonemeChange(IPhonemeUndoContext* pContext)
		: m_pContext(pContext)
	{
		if (m_pContext)
			m_pContext->GetPhonemes(m_undo);
	}

	// IUndoObject
	virtual int GetSize()
	{
		return sizeof(*this);
	}

	virtual const char* GetDescription()
	{
		return "PhonemeCtrl";
	}

	virtual void Undo(bool bUndo)
	{
		if (bUndo && m_pContext)
			m_pContext->GetPhonemes(m_redo);
		if (m_pContext)
			m_pContext->SetPhonemes(m_undo);
		if (m_pContext && bUndo)
			m_pContext->OnPhonemeChangesUnOrRedone();
	}

	virtual void Redo()
	{
		if (m_pContext)
			m_pContext->SetPhonemes(m_redo);
		if (m_pContext)
			m_pContext->OnPhonemeChangesUnOrRedone();
	}

private:
	IPhonemeUndoContext*                             m_pContext;
	std::vector<std::vector<CPhonemesCtrl::Phoneme>> m_undo;
	std::vector<std::vector<CPhonemesCtrl::Phoneme>> m_redo;
};

//////////////////////////////////////////////////////////////////////////
CPhonemesCtrl::CPhonemesCtrl()
{
	m_editMode = NothingMode;

	m_nHitPhoneme = -1;
	m_nHitSentence = -1;

	m_fZoom = 1;
	m_fOrigin = 0;

	ClearSelection();

	m_TimeUpdateRect.SetRectEmpty();
	m_fTimeMarker = -10;
}

CPhonemesCtrl::~CPhonemesCtrl()
{
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CPhonemesCtrl, CWnd)
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_LBUTTONDOWN()
ON_WM_RBUTTONDOWN()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONUP()
ON_WM_SETCURSOR()
ON_WM_LBUTTONDBLCLK()
ON_WM_RBUTTONDOWN()
ON_WM_MENUSELECT()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPhonemesCtrl message handlers

/////////////////////////////////////////////////////////////////////////////
BOOL CPhonemesCtrl::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
	return CreateEx(0, lpClassName, "PhonemesCtrl", dwStyle, rc, pParentWnd, nID);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::PostNcDestroy()
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CPhonemesCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	m_offscreenBitmap.DeleteObject();
	if (!m_offscreenBitmap.GetSafeHandle())
	{
		CDC* pDC = GetDC();
		m_offscreenBitmap.CreateCompatibleBitmap(pDC, cx, cy);
		ReleaseDC(pDC);
	}

	GetClientRect(m_rcClient);
	m_rcPhonemes = m_rcClient;
	m_rcWords = m_rcClient;

	m_rcWords.bottom = m_rcClient.Height() / 2;
	m_rcPhonemes.top = m_rcWords.bottom;

	if (m_tooltip.m_hWnd)
	{
		m_tooltip.DelTool(this, 1);
		m_tooltip.AddTool(this, "", m_rcPhonemes, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CPhonemesCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (!m_tooltip.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_tooltip.Create(this);
		m_tooltip.SetDelayTime(TTDT_AUTOPOP, 500);
		m_tooltip.SetDelayTime(TTDT_INITIAL, 0);
		m_tooltip.SetDelayTime(TTDT_RESHOW, 0);
		m_tooltip.SetMaxTipWidth(600);
		m_tooltip.AddTool(this, "", rc, 1);
		m_tooltip.Activate(FALSE);
	}
	m_tooltip.RelayEvent(pMsg);

	return __super::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
int CPhonemesCtrl::TimeToClient(int time)
{
	return floor((time - m_fOrigin) * (m_fZoom) + 0.5f) + m_rcPhonemes.left;
}

//////////////////////////////////////////////////////////////////////////
int CPhonemesCtrl::ClientToTime(int x)
{
	return ((x - m_rcPhonemes.left) / (m_fZoom) + m_fOrigin);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnPaint()
{
	CPaintDC PaintDC(this);

	CRect rcClient;
	GetClientRect(&rcClient);

	if (!m_offscreenBitmap.GetSafeHandle())
	{
		m_offscreenBitmap.CreateCompatibleBitmap(&PaintDC, rcClient.Width(), rcClient.Height());
	}

	{
		CMemoryDC dc(PaintDC, &m_offscreenBitmap);

		if (m_TimeUpdateRect != CRect(PaintDC.m_ps.rcPaint))
		{
			//////////////////////////////////////////////////////////////////////////
			// Fill keys background.
			//////////////////////////////////////////////////////////////////////////
			CRect rc = m_rcPhonemes;
			rc.top = m_rcWords.top;
			XTPPaintManager()->GradientFill(&dc, rc, RGB(160, 160, 160), RGB(110, 110, 110), FALSE);

			//////////////////////////////////////////////////////////////////////////
			// Draw separator line for words.
			{
				CPen pen;
				pen.CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
				CPen* pOldPen = dc.SelectObject(&pen);
				dc.MoveTo(m_rcWords.left, m_rcWords.bottom);
				dc.LineTo(m_rcWords.right, m_rcWords.bottom);
				dc.SelectObject(pOldPen);
			}
			//////////////////////////////////////////////////////////////////////////

			DrawWords(&dc);
			DrawPhonemes(&dc);
		}
		m_TimeUpdateRect.SetRectEmpty();
	}

	DrawTimeMarker(&PaintDC);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::DrawPhonemes(CDC* pDC)
{
	// create and select a white pen
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
	CPen* pOldPen = pDC->SelectObject(&pen);

	CRect rcClip;
	pDC->GetClipBox(rcClip);

	pDC->SetTextColor(RGB(255, 255, 255));
	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(SMFCFonts::GetInstance().hSystemFont);

	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
	{
		for (int i = 0; i < (int)m_sentences[sentenceIndex].phonemes.size(); i++)
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);

			int x = TimeToClient(ph.time0 + m_sentences[sentenceIndex].startTime * 1000.0f);
			int x1 = TimeToClient(ph.time1 + m_sentences[sentenceIndex].startTime * 1000.0f);

			if (x1 < rcClip.left - 2)
				continue;
			if (x > rcClip.right + 2)
				break;

			COLORREF col = RGB(0, 100, 200);
			if (ph.bActive)
				col = RGB(200, 100, 0);
			CBrush brush(col);
			CBrush* pOldBrush = pDC->SelectObject(&brush);

			// Find the midpoints of the top, right, left, and bottom
			// of the client area. They will be the vertices of our polygon.
			CRect rc(x, m_rcPhonemes.top, x1 + 1, m_rcPhonemes.bottom);
			pDC->Rectangle(rc);

			if (ph.intensity < 0.99f)
			{
				COLORREF col = RGB(0, 100, 230);
				if (ph.bActive)
					col = RGB(200, 100, 30);
				CBrush brush(col);
				int h = ph.intensity * (m_rcPhonemes.bottom - m_rcPhonemes.top);
				CRect rcph(x + 1, m_rcPhonemes.bottom - h, x1, m_rcPhonemes.bottom - 1);
				pDC->FillRect(rcph, &brush);
			}

			pDC->DrawText(ph.sPhoneme, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			//pDC->TextOut( x,m_rcPhonemes.top+8,ph.sPhoneme );

			pDC->SelectObject(pOldBrush);
		}

		pDC->SelectObject(pOldPen);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::DrawWords(CDC* pDC)
{
	// create and select a white pen
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
	CPen* pOldPen = pDC->SelectObject(&pen);

	CRect rcClip;
	pDC->GetClipBox(rcClip);

	pDC->SetTextColor(RGB(255, 255, 255));
	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(SMFCFonts::GetInstance().hSystemFont);

	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
	{
		for (int i = 0; i < (int)m_sentences[sentenceIndex].words.size(); i++)
		{
			Word& w = GetWord(sentenceIndex, i);

			int x = TimeToClient(w.time0 + m_sentences[sentenceIndex].startTime * 1000.0f);
			int x1 = TimeToClient(w.time1 + m_sentences[sentenceIndex].startTime * 1000.0f);

			if (x1 < rcClip.left - 2)
				continue;
			if (x > rcClip.right + 2)
				break;

			COLORREF col = RGB(0, 100, 200);
			if (w.bActive)
				col = RGB(200, 100, 0);
			CBrush brush(col);
			CBrush* pOldBrush = pDC->SelectObject(&brush);

			CRect rc(x, m_rcWords.top, x1 + 1, m_rcWords.bottom + 1);
			pDC->Rectangle(rc);
			pDC->DrawText(w.text, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

			pDC->SelectObject(pOldBrush);
		}

		pDC->SelectObject(pOldPen);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::DrawTimeMarker(CDC* pDC)
{
	CPen timePen;
	timePen.CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
	CPen* pOldPen = pDC->SelectObject(&timePen);
	int x = TimeToClient(m_fTimeMarker);
	if (x >= m_rcPhonemes.left && x <= m_rcPhonemes.right)
	{
		pDC->MoveTo(x, m_rcClient.top + 1);
		pDC->LineTo(x, m_rcClient.bottom - 1);
	}
	pDC->SelectObject(pOldPen);
}

/////////////////////////////////////////////////////////////////////////////
//Mouse Message Handlers
//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();

	m_LButtonDown = point;

	switch (m_hitCode)
	{
	case HIT_PHONEME:
		StartTracking();
		break;
	case HIT_EDGE_LEFT:
		StartTracking();
		break;
	case HIT_EDGE_RIGHT:
		StartTracking();
		break;

	case HIT_NOTHING:
		ClearSelection();
		break;
	}
	Invalidate();

	CWnd::OnLButtonDown(nFlags, point);
}

#define MENU_CMD_INSERT 1
#define MENU_CMD_DELETE 2
#define MENU_CMD_RENAME 3
#define MENU_CMD_CLEAR  4
#define MENU_CMD_BAKE   5

//////////////////////////////////////////////////////////////////////////
static const int nInsertPhonemeId = 1000;
static const int nRenamePhonemeId = 2000;
void CPhonemesCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnRButtonDown(nFlags, point);

	SetFocus();

	SendNotifyEvent(NM_RCLICK);

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	CMenu addPhonemesMenu;
	addPhonemesMenu.CreatePopupMenu();
	AddPhonemes(addPhonemesMenu, nInsertPhonemeId);
	CMenu renamePhonemesMenu;
	renamePhonemesMenu.CreatePopupMenu();
	AddPhonemes(renamePhonemesMenu, nRenamePhonemeId);

	std::pair<int, int> phonemeId = PhonemeFromTime(ClientToTime(point.x));

	if (!m_sentences.empty())
	{
		menu.AppendMenu(MF_POPUP, (UINT_PTR)addPhonemesMenu.GetSafeHmenu(), "Insert");
		m_phonemePopupMenus.insert(addPhonemesMenu.GetSafeHmenu());
	}
	if (phonemeId.first >= 0 && phonemeId.second >= 0)
	{
		const char* sPhoneme = GetPhoneme(phonemeId.first, phonemeId.second).sPhoneme;
		menu.AppendMenu(MF_POPUP, (UINT_PTR)renamePhonemesMenu.GetSafeHmenu(), CString("Change ") + sPhoneme);
		m_phonemePopupMenus.insert(renamePhonemesMenu.GetSafeHmenu());
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, MENU_CMD_DELETE, CString("Delete ") + sPhoneme);
	}
	menu.AppendMenu(MF_STRING, MENU_CMD_CLEAR, _T("Clear All"));
	if (!m_sentences.empty())
		menu.AppendMenu(MF_STRING, MENU_CMD_BAKE, _T("Bake Lipsynch Curves"));

	CPoint pos;
	GetCursorPos(&pos);

	// Set the vertex morph drag to 0 temporarily (hack to get the mouseover preview to work, since we dont update the preview in real-time).
	ICVar* pVar = GetISystem()->GetIConsole()->GetCVar("ca_lipsync_vertex_drag");
	float old_vertex_drag = (pVar ? pVar->GetFVal() : 0.0f);
	if (pVar)
		pVar->Set(1000.0f);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, pos.x, pos.y, this);
	if (pVar)
		pVar->Set(old_vertex_drag);
	m_phonemePopupMenus.clear();
	switch (cmd)
	{
	case MENU_CMD_INSERT:

		break;
	case MENU_CMD_DELETE:
		RemovePhoneme(phonemeId.first, phonemeId.second);
		break;
	case MENU_CMD_RENAME:
		break;
	case MENU_CMD_CLEAR:
		ClearAllPhonemes();
		break;
	case MENU_CMD_BAKE:
		BakeLipsynchCurves();
		break;
	}
	if (cmd >= nInsertPhonemeId && cmd < nInsertPhonemeId + 1000)
	{
		int id = cmd - nInsertPhonemeId;
		InsertPhoneme(point, id);
	}
	if (cmd >= nRenamePhonemeId && cmd < nRenamePhonemeId + 1000)
	{
		int id = cmd - nRenamePhonemeId;
		RenamePhoneme(phonemeId.first, phonemeId.second, id);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	if (m_phonemePopupMenus.find(hSysMenu) != m_phonemePopupMenus.end())
	{
		int phonemeId = -1;
		if (nItemID >= nInsertPhonemeId && nItemID < nInsertPhonemeId + 1000)
			phonemeId = nItemID - nInsertPhonemeId;
		else if (nItemID >= nRenamePhonemeId && nItemID < nRenamePhonemeId + 1000)
			phonemeId = nItemID - nRenamePhonemeId;

		IPhonemeLibrary* pPhonemeLib = GetPhonemeLib();
		bool foundPhonemeInfo = pPhonemeLib && phonemeId >= 0 && phonemeId < pPhonemeLib->GetPhonemeCount();
		SPhonemeInfo phonemeInfo;
		foundPhonemeInfo = foundPhonemeInfo && pPhonemeLib->GetPhonemeInfo(phonemeId, phonemeInfo);
		const char* phonemeString = (foundPhonemeInfo && phonemeInfo.ASCII ? phonemeInfo.ASCII : "");
		m_mouseOverPhoneme = phonemeString;
		SendNotifyEvent(PHONEMECTRLN_PREVIEW);
	}
}

//////////////////////////////////////////////////////////////////////////
IPhonemeLibrary* CPhonemesCtrl::GetPhonemeLib()
{
	IPhonemeLibrary* pPhonemeLib = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->GetPhonemeLibrary();
	return pPhonemeLib;
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::AddWord(int sentenceIndex, const Word& w)
{
	m_sentences[sentenceIndex].words.push_back(w);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::AddPhonemes(CMenu& menu, int nBaseId)
{
	IPhonemeLibrary* pPhonemeLib = GetPhonemeLib();
	if (!pPhonemeLib)
		return;

	for (int i = 0; i < pPhonemeLib->GetPhonemeCount(); i++)
	{
		SPhonemeInfo phonemeInfo;
		if (!pPhonemeLib->GetPhonemeInfo(i, phonemeInfo))
			continue;

		CString str;
		str.Format("%s\t- %s", phonemeInfo.ASCII, phonemeInfo.description);
		menu.AppendMenu(MF_STRING, nBaseId + i, str);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDblClk(nFlags, point);
	SendNotifyEvent(NM_DBLCLK);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (GetCapture() != this)
		StopTracking();

	std::pair<int, int> overPhoneme(-1, -1);

	switch (m_editMode)
	{
	case TrackingMode:
		TrackPoint(point);
		break;
	case NothingMode:
		{
			// Preview this phoneme.
			overPhoneme = PhonemeFromTime(ClientToTime(point.x));
		}
		break;
	}

	const char* phonemeString = "";
	if (overPhoneme.first >= 0 && overPhoneme.second >= 0)
		phonemeString = GetPhoneme(overPhoneme.first, overPhoneme.second).sPhoneme;
	if (stricmp(phonemeString, m_mouseOverPhoneme.c_str()) != 0)
	{
		m_mouseOverPhoneme = phonemeString;
		SendNotifyEvent(PHONEMECTRLN_PREVIEW);
	}

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_editMode == TrackingMode)
		StopTracking();

	CWnd::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	CWnd::OnRButtonUp(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
BOOL CPhonemesCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	BOOL b = FALSE;

	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	switch (HitTest(point))
	{
	case HIT_PHONEME:
		{
			HCURSOR hCursor;
			hCursor = AfxGetApp()->LoadCursor(IDC_ARRWHITE);
			SetCursor(hCursor);
			b = TRUE;
		} break;
	case HIT_EDGE_LEFT:
	case HIT_EDGE_RIGHT:
		{
			HCURSOR hCursor;
			hCursor = AfxGetApp()->LoadCursor(IDC_LEFTRIGHT);
			SetCursor(hCursor);
			b = TRUE;
		} break;
	default:
		break;
	}

	if (!b)
		return CWnd::OnSetCursor(pWnd, nHitTest, message);
	else return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
CPhonemesCtrl::EHitCode CPhonemesCtrl::HitTest(CPoint point)
{
	int time = ClientToTime(point.x);
	int time0 = ClientToTime(point.x - 2);
	int time1 = ClientToTime(point.x + 2);

	CRect rc;
	GetClientRect(rc);

	m_hitCode = HIT_NOTHING;
	m_nHitSentence = -1;
	m_nHitPhoneme = -1;

	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
	{
		int i;
		int num = GetPhonemeCount(sentenceIndex);
		for (i = 0; i < num; i++)
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);
			if (time0 <= ph.time0 + m_sentences[sentenceIndex].startTime * 1000.0f && time1 >= ph.time0 + m_sentences[sentenceIndex].startTime * 1000.0f)
			{
				m_nHitSentence = sentenceIndex;
				m_nHitPhoneme = i;
				m_hitCode = HIT_EDGE_LEFT;
				return m_hitCode;
			}
		}
		for (i = 0; i < num; i++)
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);
			if (time0 <= ph.time1 + m_sentences[sentenceIndex].startTime * 1000.0f && time1 >= ph.time1 + m_sentences[sentenceIndex].startTime * 1000.0f)
			{
				m_nHitSentence = sentenceIndex;
				m_nHitPhoneme = i;
				m_hitCode = HIT_EDGE_RIGHT;
				return m_hitCode;
			}
		}
		for (i = 0; i < num; i++)
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);
			if (time >= ph.time0 + m_sentences[sentenceIndex].startTime * 1000.0f && time <= ph.time1 + m_sentences[sentenceIndex].startTime * 1000.0f)
			{
				m_nHitSentence = sentenceIndex;
				m_nHitPhoneme = i;
				m_hitCode = HIT_PHONEME;
				return m_hitCode;
			}
		}
	}

	return m_hitCode;
}

///////////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::StartTracking()
{
	m_editMode = TrackingMode;
	SetCapture();

	GetIEditor()->GetIUndoManager()->Begin();
	StoreUndo();
	SendNotifyEvent(PHONEMECTRLN_BEFORE_CHANGE);

	HCURSOR hCursor;
	hCursor = AfxGetApp()->LoadCursor(IDC_LEFTRIGHT);
	SetCursor(hCursor);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::SetPhonemeTime(int sentenceIndex, int index, int t0, int t1)
{
	if (index < 0 || index >= GetPhonemeCount(sentenceIndex))
		return;

	Phoneme* pPrev = 0;
	Phoneme* pNext = 0;
	if (index > 0)
		pPrev = &GetPhoneme(sentenceIndex, index - 1);
	if (index < GetPhonemeCount(sentenceIndex) - 1)
		pNext = &GetPhoneme(sentenceIndex, index + 1);

	if (pPrev && (pPrev->time1 > t0 || pPrev->time1 == GetPhoneme(sentenceIndex, index).time0))
		pPrev->time1 = t0;
	if (pNext && (pNext->time0 < t1 || pNext->time0 == GetPhoneme(sentenceIndex, index).time1))
		pNext->time0 = t1;

	GetPhoneme(sentenceIndex, index).time0 = t0;
	GetPhoneme(sentenceIndex, index).time1 = t1;

}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::TrackPoint(CPoint point)
{
	GetIEditor()->GetIUndoManager()->Restore();
	StoreUndo();

	if (point.x < m_rcPhonemes.left && point.y > m_rcPhonemes.right)
		return;

	int nSentenceIndex = m_nHitSentence;
	int nPhoneme = m_nHitPhoneme;

	if (nPhoneme >= 0 && nPhoneme < GetPhonemeCount(nSentenceIndex))
	{
		int time0 = GetPhoneme(nSentenceIndex, nPhoneme).time0 + m_sentences[nSentenceIndex].startTime * 1000.0f;
		int time1 = GetPhoneme(nSentenceIndex, nPhoneme).time1 + m_sentences[nSentenceIndex].startTime * 1000.0f;

		int time = ClientToTime(point.x);

		if (m_hitCode == HIT_EDGE_LEFT)
		{
			if (time <= time1)
				SetPhonemeTime(nSentenceIndex, nPhoneme, time - m_sentences[nSentenceIndex].startTime * 1000.0f, time1 - m_sentences[nSentenceIndex].startTime * 1000.0f);
		}
		else if (m_hitCode == HIT_EDGE_RIGHT)
		{
			if (time >= time0)
				SetPhonemeTime(nSentenceIndex, nPhoneme, time0 - m_sentences[nSentenceIndex].startTime * 1000.0f, time - m_sentences[nSentenceIndex].startTime * 1000.0f);
		}
		else if (m_hitCode == HIT_PHONEME)
		{
			int dt = ClientToTime(point.x) - ClientToTime(m_LButtonDown.x);

			SetPhonemeTime(nSentenceIndex, nPhoneme, time0 + dt - m_sentences[nSentenceIndex].startTime * 1000.0f, time1 + dt - m_sentences[nSentenceIndex].startTime * 1000.0f);
		}

		SendNotifyEvent(PHONEMECTRLN_CHANGE);

		RedrawWindow();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::StopTracking()
{
	if (m_editMode != TrackingMode)
		return;

	GetIEditor()->GetIUndoManager()->Accept("Phoneme Move");

	if (m_nHitPhoneme >= 0)
	{
		CPoint point;
		GetCursorPos(&point);
		ScreenToClient(&point);

		CRect rc;
		GetClientRect(rc);
		rc.InflateRect(100, 100);
		if (!rc.PtInRect(point))
		{
			//RemovePhoneme(m_nHitPhoneme);
		}
	}

	UpdatePhonemeLengths();
	Invalidate();

	m_editMode = NothingMode;
	ReleaseCapture();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::RemovePhoneme(int sentenceIndex, int phonemeId)
{
	if (!(sentenceIndex >= 0 && sentenceIndex < GetSentenceCount()))
		return;
	if (!(phonemeId >= 0 && phonemeId < GetPhonemeCount(sentenceIndex)))
		return;

	CUndo undo("Remove Phoneme");
	StoreUndo();

	SendNotifyEvent(PHONEMECTRLN_BEFORE_CHANGE);
	m_nHitSentence = -1;
	m_nHitPhoneme = -1;

	// Remove
	m_sentences[sentenceIndex].phonemes.erase(m_sentences[sentenceIndex].phonemes.begin() + phonemeId);

	SendNotifyEvent(PHONEMECTRLN_CHANGE);

	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::BakeLipsynchCurves()
{
	SendNotifyEvent(PHONEMECTRLN_BAKE);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::ClearAllPhonemes()
{
	SendNotifyEvent(PHONEMECTRLN_CLEAR);
}

inline bool ComparePhonemes(const CPhonemesCtrl::Phoneme& p1, const CPhonemesCtrl::Phoneme& p2)
{
	return p1.time0 < p2.time0;
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::UpdatePhonemeLengths()
{
	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
	{
		std::stable_sort(m_sentences[sentenceIndex].phonemes.begin(), m_sentences[sentenceIndex].phonemes.end(), ComparePhonemes);

		int i;
		for (i = 0; i < m_sentences[sentenceIndex].phonemes.size(); )
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);
			if (ph.time0 >= ph.time1)
			{
				m_sentences[sentenceIndex].phonemes.erase(m_sentences[sentenceIndex].phonemes.begin() + i);
			}
			else
				i++;
		}

		for (i = 0; i < m_sentences[sentenceIndex].phonemes.size(); )
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);
			if (ph.time0 >= ph.time1)
			{
				m_sentences[sentenceIndex].phonemes.erase(m_sentences[sentenceIndex].phonemes.begin() + i);
			}
			else
				i++;
		}

		// Clip phonemes.
		if (m_sentences[sentenceIndex].phonemes.size() > 1)
		{
			for (i = 0; i < m_sentences[sentenceIndex].phonemes.size() - 1; i++)
			{
				Phoneme& ph = GetPhoneme(sentenceIndex, i);
				Phoneme& phnext = GetPhoneme(sentenceIndex, i + 1);
				if (ph.time1 > phnext.time0)
				{
					ph.time1 = phnext.time0;
				}
			}
		}
	}

	UpdateCurrentActivePhoneme();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::AddPhoneme(int sentenceIndex, Phoneme& phoneme)
{
	phoneme.bSelected = false;
	phoneme.bActive = false;
	m_sentences[sentenceIndex].phonemes.push_back(phoneme);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::InsertPhoneme(CPoint point, int phonemeId)
{
	SPhonemeInfo phonemeInfo;
	if (!GetPhonemeLib()->GetPhonemeInfo(phonemeId, phonemeInfo))
		return;

	int time = ClientToTime(point.x);

	int sentenceIndex = -1;
	for (int i = 0, sentenceCount = m_sentences.size(); i < sentenceCount; ++i)
	{
		if (time >= m_sentences[i].startTime * 1000.0f && time <= m_sentences[i].endTime * 1000.0f)
			sentenceIndex = i;
	}

	if (sentenceIndex != -1)
	{
		Phoneme phoneme;
		phoneme.bSelected = false;
		phoneme.bActive = false;
		memcpy(phoneme.sPhoneme, phonemeInfo.ASCII, sizeof(phoneme.sPhoneme));

		SendNotifyEvent(PHONEMECTRLN_BEFORE_CHANGE);

		CUndo undo("Insert Phoneme");
		StoreUndo();

		ClearSelection();

		int endTime = time + 100;
		// Find phoneme after this time.
		int i;
		for (i = 0; i < GetPhonemeCount(sentenceIndex); i++)
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);
			if (ph.time0 + m_sentences[sentenceIndex].startTime * 1000.0f > time)
			{
				if (endTime - m_sentences[sentenceIndex].startTime * 1000.0f >= ph.time0)
					endTime = ph.time0 - 10 + m_sentences[sentenceIndex].startTime * 1000.0f;
			}
		}

		if (endTime <= time)
			endTime = time + 1;

		phoneme.time0 = time - m_sentences[sentenceIndex].startTime * 1000.0f;
		phoneme.time1 = endTime - m_sentences[sentenceIndex].startTime * 1000.0f;
		m_sentences[sentenceIndex].phonemes.push_back(phoneme);

		UpdatePhonemeLengths();

		Invalidate();

		SendNotifyEvent(PHONEMECTRLN_CHANGE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::RenamePhoneme(int sentenceIndex, int index, int phonemeId)
{
	SPhonemeInfo phonemeInfo;
	if (!GetPhonemeLib()->GetPhonemeInfo(phonemeId, phonemeInfo))
		return;

	SendNotifyEvent(PHONEMECTRLN_BEFORE_CHANGE);

	CUndo undo("Rename Phoneme");
	StoreUndo();

	Phoneme& phoneme = GetPhoneme(sentenceIndex, index);
	memcpy(phoneme.sPhoneme, phonemeInfo.ASCII, sizeof(phoneme.sPhoneme));

	Invalidate();
	SendNotifyEvent(PHONEMECTRLN_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::ClearSelection()
{
	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
	{
		for (int i = 0; i < GetPhonemeCount(sentenceIndex); i++)
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);
			ph.bSelected = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::SetTimeMarker(float fTime)
{
	fTime = fTime * 1000;

	if (fTime == m_fTimeMarker)
		return;

	// Erase old first.
	int pt0 = TimeToClient(m_fTimeMarker);
	int pt1 = TimeToClient(fTime);
	CRect rc = CRect(pt0, m_rcClient.top, pt1, m_rcClient.bottom);
	rc.NormalizeRect();
	rc.InflateRect(5, 0);
	rc.IntersectRect(rc, m_rcClient);

	m_TimeUpdateRect = rc;
	InvalidateRect(rc);

	m_fTimeMarker = fTime;

	UpdateCurrentActivePhoneme();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::UpdateCurrentActivePhoneme()
{
	bool bNoChangeToPhoneme = false;
	bool bNoChangeToWord = false;
	std::pair<int, int> id = PhonemeFromTime(m_fTimeMarker);
	if (id.first >= 0 && id.second >= 0)
	{
		if (GetPhoneme(id.first, id.second).bActive)
		{
			bNoChangeToPhoneme = true;
		}
	}

	bool bChanged = false;

	if (!bNoChangeToPhoneme)
	{
		for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
		{
			int num = GetPhonemeCount(sentenceIndex);
			for (int i = 0; i < num; i++)
			{
				Phoneme& ph = GetPhoneme(sentenceIndex, i);
				if (ph.bActive)
					bChanged = true;
				ph.bActive = false;
			}
		}

		if (id.first >= 0 && id.second >= 0)
		{
			GetPhoneme(id.first, id.second).bActive = true;
			bChanged = true;
		}
	}

	{
		// Words
		id = WordFromTime(m_fTimeMarker);
		if (id.first >= 0 && id.second)
		{
			if (GetWord(id.first, id.second).bActive)
			{
				bNoChangeToWord = true;
			}
		}
		if (!bNoChangeToWord)
		{
			for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
			{
				int num = GetWordCount(sentenceIndex);
				for (int i = 0; i < num; i++)
				{
					Word& w = GetWord(sentenceIndex, i);
					if (w.bActive)
						bChanged = true;
					w.bActive = false;
				}
			}
			if (id.first >= 0 && id.second)
			{
				GetWord(id.first, id.second).bActive = true;
				bChanged = true;
			}
		}
	}

	if (bChanged)
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::SendNotifyEvent(int nEvent)
{
	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
	nmh.code = nEvent;

	GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmh);
}

//////////////////////////////////////////////////////////////////////////
std::pair<int, int> CPhonemesCtrl::PhonemeFromTime(int time)
{
	int i;
	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
	{
		int num = GetPhonemeCount(sentenceIndex);
		for (i = 0; i < num; i++)
		{
			Phoneme& ph = GetPhoneme(sentenceIndex, i);
			if (time >= ph.time0 + m_sentences[sentenceIndex].startTime * 1000.0f && time <= ph.time1 + m_sentences[sentenceIndex].startTime * 1000.0f)
				return std::make_pair(sentenceIndex, i);
		}
	}
	return std::make_pair(-1, -1);
}

//////////////////////////////////////////////////////////////////////////
std::pair<int, int> CPhonemesCtrl::WordFromTime(int time)
{
	int i;
	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
	{
		int num = GetWordCount(sentenceIndex);
		for (i = 0; i < num; i++)
		{
			Word& ph = GetWord(sentenceIndex, i);
			if (time >= ph.time0 + m_sentences[sentenceIndex].startTime * 1000.0f && time <= ph.time1 + m_sentences[sentenceIndex].startTime * 1000.0f)
			{
				return std::make_pair(sentenceIndex, i);
			}
		}
	}
	return std::make_pair(-1, -1);
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::StoreUndo()
{
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoPhonemeChange(this));
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::SetZoom(float fZoom)
{
	m_fZoom = fZoom;
	if (GetSafeHwnd())
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::SetScrollOffset(float fOrigin)
{
	m_fOrigin = fOrigin;
	if (GetSafeHwnd())
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
const char* CPhonemesCtrl::GetMouseOverPhoneme()
{
	return m_mouseOverPhoneme.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::SetPhonemes(const std::vector<std::vector<Phoneme>>& phonemes)
{
	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
		m_sentences[sentenceIndex].phonemes = phonemes[sentenceIndex];
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::GetPhonemes(std::vector<std::vector<Phoneme>>& phonemes)
{
	phonemes.resize(m_sentences.size());
	for (int sentenceIndex = 0, sentenceCount = m_sentences.size(); sentenceIndex < sentenceCount; ++sentenceIndex)
		phonemes[sentenceIndex] = m_sentences[sentenceIndex].phonemes;
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::OnPhonemeChangesUnOrRedone()
{
	SendNotifyEvent(PHONEMECTRLN_CHANGE);

	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
int CPhonemesCtrl::AddSentence()
{
	int sentenceIndex = m_sentences.size();
	m_sentences.resize(m_sentences.size() + 1);
	return sentenceIndex;
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::DeleteSentence(int sentenceIndex)
{
	m_sentences.erase(m_sentences.begin() + sentenceIndex);
}

//////////////////////////////////////////////////////////////////////////
int CPhonemesCtrl::GetSentenceCount()
{
	return m_sentences.size();
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::SetSentenceStartTime(int sentenceIndex, float startTime)
{
	m_sentences[sentenceIndex].startTime = startTime;
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesCtrl::SetSentenceEndTime(int sentenceIndex, float endTime)
{
	m_sentences[sentenceIndex].endTime = endTime;
}

