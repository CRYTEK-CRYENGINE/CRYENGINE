// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IDataBaseItem;

//! Dialog to list and select all library and level game tokens (but not graph tokens). Used as picker by Nodes with a token port.
class CSelectGameTokenDialog : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CSelectGameTokenDialog)

public:
	CSelectGameTokenDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSelectGameTokenDialog();

	CString GetSelectedGameToken() const { return m_selectedItem; }
	void    PreSelectGameToken(const CString& name) { m_preselect = name; }

	// Dialog Data
	enum { IDD = IDD_SELECT_GAMETOKEN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg void OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTvnDoubleClick(NMHDR* pNMHDR, LRESULT* pResult);
	BOOL         OnInitDialog();

	void         ReloadGameTokens();

	DECLARE_MESSAGE_MAP()

	CString     m_preselect;
	CTreeCtrl   m_tree;
	CImageList  m_imageList;
	CString     m_selectedItem;

	std::map<HTREEITEM, CString> m_itemsMap;
};


