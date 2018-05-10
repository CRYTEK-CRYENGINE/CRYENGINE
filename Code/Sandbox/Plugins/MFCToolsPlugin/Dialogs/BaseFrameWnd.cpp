// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "BaseFrameWnd.h"

#define BASEFRAME_WINDOW_CLASSNAME "CBaseFrameWndClass"

#define WM_FRAME_CAN_CLOSE         (WM_APP + 1000)

BEGIN_MESSAGE_MAP(CBaseFrameWnd, CXTPFrameWnd)
ON_WM_DESTROY()
// XT Commands.
ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)
ON_MESSAGE(WM_FRAME_CAN_CLOSE, OnFrameCanClose)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CBaseFrameWnd::CBaseFrameWnd()
{
	m_frameLayoutVersion = 0;

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, BASEFRAME_WINDOW_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wndcls.lpfnWndProc = ::DefWindowProc;
		wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
		wndcls.hInstance = hInst;
		wndcls.hIcon = NULL;
		wndcls.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground = (HBRUSH) (COLOR_3DFACE + 1);
		wndcls.lpszMenuName = NULL;
		wndcls.lpszClassName = BASEFRAME_WINDOW_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CBaseFrameWnd::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd)
{
	BOOL bRet = __super::Create(BASEFRAME_WINDOW_CLASSNAME, NULL, dwStyle, rect, pParentWnd);
	if (bRet == TRUE)
	{
		try
		{
			//////////////////////////////////////////////////////////////////////////
			// Initialize the command bars
			if (!InitCommandBars())
				return FALSE;

		}
		catch (CResourceException* e)
		{
			e->Delete();
			return FALSE;
		}

		//////////////////////////////////////////////////////////////////////////
		// Install docking panes
		//////////////////////////////////////////////////////////////////////////
		GetDockingPaneManager()->InstallDockingPanes(this);
		GetDockingPaneManager()->SetTheme(xtpPaneThemeOffice2003);
		GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);
		//////////////////////////////////////////////////////////////////////////

		bRet = OnInitDialog();
	}
	return bRet;
}

//////////////////////////////////////////////////////////////////////////
void CBaseFrameWnd::OnDestroy()
{
	if (!m_frameLayoutKey.IsEmpty())
	{
		SaveFrameLayout();
	}
	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CBaseFrameWnd::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
BOOL CBaseFrameWnd::PreTranslateMessage(MSG* pMsg)
{
	// allow tooltip messages to be filtered
	if (__super::PreTranslateMessage(pMsg))
		return TRUE;

	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		// If the key is tab, allow the parent window to handle this message
		if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
		{
			return FALSE;
		}

		// All keypresses are translated by this frame window
		::TranslateMessage(pMsg);
		::DispatchMessage(pMsg);

		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseFrameWnd::AutoLoadFrameLayout(const CString& name, int nVersion)
{
	m_frameLayoutVersion = nVersion;
	m_frameLayoutKey = name;
	return LoadFrameLayout();
}

//////////////////////////////////////////////////////////////////////////
bool CBaseFrameWnd::LoadFrameLayout()
{
	CXTRegistryManager regMgr;
	int paneLayoutVersion = regMgr.GetProfileInt(CString(_T("FrameLayout_")) + m_frameLayoutKey, _T("LayoutVersion_"), 0);
	if (paneLayoutVersion == m_frameLayoutVersion)
	{
		CXTPDockingPaneLayout layout(GetDockingPaneManager());
		if (layout.Load(CString(_T("FrameLayout_")) + m_frameLayoutKey))
		{
			if (layout.GetPaneList().GetCount() > 0)
			{
				GetDockingPaneManager()->SetLayout(&layout);
				return true;
			}
		}
	}
	else
	{
		regMgr.WriteProfileInt(CString(_T("FrameLayout_")) + m_frameLayoutKey, _T("LayoutVersion_"), m_frameLayoutVersion);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBaseFrameWnd::SaveFrameLayout()
{
	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	GetDockingPaneManager()->GetLayout(&layout);
	layout.Save(CString(_T("FrameLayout_")) + m_frameLayoutKey);

	CXTRegistryManager regMgr;
	regMgr.WriteProfileInt(CString(_T("FrameLayout_")) + m_frameLayoutKey, _T("LayoutVersion_"), m_frameLayoutVersion);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
LRESULT CBaseFrameWnd::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (!pwndDockWindow->IsValid())
		{
			int id = pwndDockWindow->GetID();
			for (int i = 0; i < (int)m_dockingPaneWindows.size(); i++)
			{
				if (m_dockingPaneWindows[i].first == id)
				{
					pwndDockWindow->Attach(m_dockingPaneWindows[i].second);
					m_dockingPaneWindows[i].second->ShowWindow(SW_SHOW);
					return TRUE;
				}
			}
			return FALSE;
		}
		return TRUE;
	}
	else if (wParam == XTP_DPN_CLOSEPANE)
	{
		// get a pointer to the docking pane being closed.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (pwndDockWindow->IsValid())
		{
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CBaseFrameWnd::OnFrameCanClose(WPARAM wParam, LPARAM lParam)
{
	*((BOOL*)wParam) = CanClose();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CXTPDockingPane* CBaseFrameWnd::CreateDockingPane(const char* sPaneTitle, CWnd* pWindow, UINT nID, CRect rc, XTPDockingPaneDirection direction, CXTPDockingPaneBase* pNeighbour /*= NULL */)
{
	CXTPDockingPane* pPane = GetDockingPaneManager()->CreatePane(nID, rc, direction, pNeighbour);
	pPane->SetTitle(sPaneTitle);
	m_dockingPaneWindows.push_back(std::pair<int, CWnd*>(nID, pWindow));
	return pPane;
}

