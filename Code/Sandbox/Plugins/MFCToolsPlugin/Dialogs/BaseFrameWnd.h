// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BaseFrameWnd__
#define __BaseFrameWnd__

#pragma once

//////////////////////////////////////////////////////////////////////////
//
// Base class for Sandbox frame windows
//
//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CBaseFrameWnd : public CXTPFrameWnd
{
public:
	CBaseFrameWnd();

	CXTPDockingPaneManager* GetDockingPaneManager() { return &m_paneManager; }
	BOOL                    Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd);

	// Will automatically save and restore frame docking layout.
	// Place at the end of the window dock panes creation.
	bool             AutoLoadFrameLayout(const CString& name, int nVersion = 0);
	// Every docking pane windows should be registered with this function
	CXTPDockingPane* CreateDockingPane(const char* sPaneTitle, CWnd* pWindow, UINT nID, CRect rc, XTPDockingPaneDirection direction, CXTPDockingPaneBase* pNeighbour = NULL);

protected:

	// This methods are used to save and load layout of the frame to the registry.
	// Activated by the call to AutoSaveFrameLayout
	virtual bool LoadFrameLayout();
	virtual void SaveFrameLayout();

	// Override this to provide code to cancel a window close, e.g. for unsaved changes
	virtual bool CanClose() { return true; }

	DECLARE_MESSAGE_MAP()

	virtual void OnOK()     {};
	virtual void OnCancel() {};
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();

	afx_msg void OnDestroy();

	//////////////////////////////////////////////////////////////////////////
	// Implement in derived class
	//////////////////////////////////////////////////////////////////////////
	virtual BOOL    OnInitDialog() { return TRUE; };
	virtual LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);
	virtual LRESULT OnFrameCanClose(WPARAM wParam, LPARAM lParam);
	//////////////////////////////////////////////////////////////////////////

protected:
	// Docking panes manager.
	CXTPDockingPaneManager             m_paneManager;
	CString                            m_frameLayoutKey;
	int                                m_frameLayoutVersion;

	std::vector<std::pair<int, CWnd*>> m_dockingPaneWindows;
};

#endif //__BaseFrameWnd__

