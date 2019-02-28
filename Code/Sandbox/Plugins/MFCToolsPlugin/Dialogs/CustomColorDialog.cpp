// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CustomColorDialog.h"

class CScreenWnd;

static std::vector<std::unique_ptr<CScreenWnd>> g_screenWnds;
static BOOL CALLBACK CreateScreenWndCB(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM data);

/////////////////////////////////////////////////////////////////////////////
// CScreenWnd window

class CScreenWnd : public CWnd
{
	// Construction
public:
	CScreenWnd();

	// Overrides
public:
	virtual BOOL Create(CMFCColorDialog* pColorDlg, CRect rect);

	// Implementation
public:
	virtual ~CScreenWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CScreenWnd)
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CMFCColorDialog * m_pColorDlg;
};

/////////////////////////////////////////////////////////////////////////////
// CCustomColorDialog

IMPLEMENT_DYNAMIC(CCustomColorDialog, CMFCColorDialog)

//////////////////////////////////////////////////////////////////////////
CCustomColorDialog::CCustomColorDialog(COLORREF clrInit, DWORD dwFlags, CWnd* pParentWnd) :
	CMFCColorDialog(clrInit, dwFlags, pParentWnd), m_callback(0), m_colorPrev(0)
{
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CCustomColorDialog, CMFCColorDialog)
ON_WM_MOUSEMOVE()
ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
ON_BN_CLICKED(IDC_AFXBARRES_COLOR_SELECT, OnColorSelect)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CCustomColorDialog::OnInitDialog()
{
	CMFCColorDialog::OnInitDialog();

	m_colorPrev = m_CurrentColor;
	m_pPropSheet->SetActivePage(m_pColourSheetTwo);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CCustomColorDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	CMFCColorDialog::OnMouseMove(nFlags, point);

	if (m_colorPrev != m_NewColor)
		OnColorChange(m_NewColor);
}

//////////////////////////////////////////////////////////////////////////
LRESULT CCustomColorDialog::OnKickIdle(WPARAM, LPARAM lCount)
{
	if (m_colorPrev != m_NewColor)
		OnColorChange(m_NewColor);

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CCustomColorDialog::OnColorChange(COLORREF col)
{
	m_colorPrev = col;
	if (m_callback)
		m_callback(col);
}

//////////////////////////////////////////////////////////////////////////
void CCustomColorDialog::OnColorSelect()
{
	if (m_bPickerMode)
	{
		return;
	}

	CWinThread* pCurrThread = ::AfxGetThread();
	if (pCurrThread == NULL)
	{
		ASSERT(FALSE);
		return;
	}

	MSG msg;
	m_bPickerMode = TRUE;
	;
	::SetCursor(m_hcurPicker);

	LONG_PTR thisPtr = reinterpret_cast<LONG_PTR>(this);
	::EnumDisplayMonitors(NULL, NULL, CreateScreenWndCB, thisPtr);

	SetForegroundWindow();
	BringWindowToTop();

	SetCapture();

	COLORREF colorSaved = m_NewColor;

	while (m_bPickerMode)
	{
		while (m_bPickerMode && ::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_KEYDOWN)
			{
				switch (msg.wParam)
				{
				case VK_RETURN:
					m_bPickerMode = FALSE;
					break;

				case VK_ESCAPE:
					SetNewColor(colorSaved);
					m_bPickerMode = FALSE;
					break;
				}
			}
			else if (msg.message == WM_RBUTTONDOWN || msg.message == WM_MBUTTONDOWN)
			{
				m_bPickerMode = FALSE;
			}
			else
			{
				if (!pCurrThread->PreTranslateMessage(&msg))
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}

				pCurrThread->OnIdle(0);
			}
		}

		WaitMessage();
	}

	ReleaseCapture();
	std::for_each(g_screenWnds.begin(), g_screenWnds.end(), [](const std::unique_ptr<CScreenWnd>& pScreenWnd)
	{
		pScreenWnd->DestroyWindow();
	});
	g_screenWnds.clear();

	m_bPickerMode = FALSE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CALLBACK CreateScreenWndCB(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM data)
{
	MONITORINFOEX monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	::GetMonitorInfo(monitor, &monitorInfo);

	CCustomColorDialog* self = reinterpret_cast<CCustomColorDialog*>(data);

	std::unique_ptr<CScreenWnd> pScreenWnd(new CScreenWnd());
	pScreenWnd->Create(self, monitorInfo.rcMonitor);

	g_screenWnds.push_back(std::move(pScreenWnd));

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CScreenWnd

CScreenWnd::CScreenWnd()
{
}

CScreenWnd::~CScreenWnd()
{
}

BEGIN_MESSAGE_MAP(CScreenWnd, CWnd)
//{{AFX_MSG_MAP(CScreenWnd)
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONDOWN()
ON_WM_SETCURSOR()
ON_WM_ERASEBKGND()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScreenWnd message handlers

BOOL CScreenWnd::Create(CMFCColorDialog* pColorDlg, CRect rect)
{
	m_pColorDlg = pColorDlg;

	string strClassName = ::AfxRegisterWndClass(CS_SAVEBITS, AfxGetApp()->LoadCursor(IDC_AFXBARRES_COLOR), (HBRUSH)(COLOR_BTNFACE + 1), NULL);

	return CWnd::CreateEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT, strClassName, _T(""), WS_VISIBLE | WS_POPUP, rect, NULL, 0);
}

void CScreenWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	MapWindowPoints(m_pColorDlg, &point, 1);
	m_pColorDlg->SendMessage(WM_MOUSEMOVE, nFlags, MAKELPARAM(point.x, point.y));

	CWnd::OnMouseMove(nFlags, point);
}

void CScreenWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	MapWindowPoints(m_pColorDlg, &point, 1);
	m_pColorDlg->SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
}

BOOL CScreenWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

BOOL CCustomColorDialog::PreTranslateMessage(MSG* pMsg)
{
#ifdef _UNICODE
	#define AFX_TCF_TEXT CF_UNICODETEXT
#else
	#define AFX_TCF_TEXT CF_TEXT
#endif

	if (m_colorPrev != m_NewColor)
	{
		OnColorChange(m_NewColor);
	}

	if (pMsg->message == WM_KEYDOWN)
	{
		UINT nChar = (UINT) pMsg->wParam;
		BOOL bIsCtrl = (::GetAsyncKeyState(VK_CONTROL) & 0x8000);

		if (bIsCtrl && (nChar == _T('C') || nChar == VK_INSERT))
		{
			if (OpenClipboard())
			{
				EmptyClipboard();

				string strText;
				strText.Format(_T("RGB(%d, %d, %d)"), GetRValue(m_NewColor), GetGValue(m_NewColor), GetBValue(m_NewColor));

				HGLOBAL hClipbuffer = ::GlobalAlloc(GMEM_DDESHARE, (strText.GetLength() + 1) * sizeof(TCHAR));
				LPTSTR lpszBuffer = (LPTSTR) GlobalLock(hClipbuffer);

				lstrcpy(lpszBuffer, (LPCTSTR) strText);

				::GlobalUnlock(hClipbuffer);
				::SetClipboardData(AFX_TCF_TEXT, hClipbuffer);

				CloseClipboard();
			}
		}
	}

	return CMFCColorDialog::PreTranslateMessage(pMsg);
}

