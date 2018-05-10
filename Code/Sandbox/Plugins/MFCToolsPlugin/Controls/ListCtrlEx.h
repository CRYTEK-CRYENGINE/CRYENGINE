// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ListCtrlEx_h__
#define __ListCtrlEx_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CListCtrlEx : public CXTListCtrl
{
	// Construction
public:
	CListCtrlEx();

public:
	void RepositionControls();
	void InsertSomeItems();
	void CreateColumns();
	virtual ~CListCtrlEx();

	CWnd* GetItemControl(int nIndex, int nColumn);
	void  SetItemControl(int nIndex, int nColumn, CWnd* pWnd);
	BOOL  DeleteAllItems();

	// Generated message map functions
protected:
	virtual BOOL SubclassWindow(HWND hWnd);

	DECLARE_MESSAGE_MAP()
	afx_msg void    OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void    OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg void    OnWindowPosChanged(WINDOWPOS FAR* lpwndpos);
	afx_msg LRESULT OnNotifyMsg(WPARAM wParam, LPARAM lParam);
	afx_msg void    OnHeaderEndTrack(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg int     OnCreate(LPCREATESTRUCT lpCreateStruct);

	//////////////////////////////////////////////////////////////////////////
	typedef std::map<int, CWnd*> ControlsMap;
	ControlsMap m_controlsMap;
};

//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CControlsListBox : public CListBox
{
	// Construction
public:
	CControlsListBox();
	virtual ~CControlsListBox();

public:
	void  RepositionControls();

	CWnd* GetItemControl(int nIndex);
	void  SetItemControl(int nIndex, CWnd* pWnd);
	void  ResetContent();

	// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void    OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void    OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg void    OnWindowPosChanged(WINDOWPOS FAR* lpwndpos);
	afx_msg LRESULT OnNotifyMsg(WPARAM wParam, LPARAM lParam);
	afx_msg void    OnHeaderEndTrack(NMHDR* pNMHDR, LRESULT* pResult);

	//////////////////////////////////////////////////////////////////////////
	typedef std::map<int, CWnd*> ControlsMap;
	ControlsMap m_controlsMap;
};

#endif // __ListCtrlEx_h__

