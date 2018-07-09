// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CSmartObjectClassDialog dialog

class CSmartObjectClassDialog : public CDialog
{
	DECLARE_DYNAMIC(CSmartObjectClassDialog)

private:
	CString   m_sSOClass;
	CTreeCtrl m_TreeCtrl;
	bool      m_bMultiple;
	bool      m_bEnableOK;

	typedef std::map<CString, HTREEITEM> MapStringToItem;
	MapStringToItem m_mapStringToItem;

	typedef std::set<CString> TSOClassesSet;
	TSOClassesSet m_setClasses;
	void      UpdateListCurrentClasses();

	HTREEITEM FindOrInsertItem(const CString& name, HTREEITEM parent);
	HTREEITEM ForcePath(const CString& location);
	void      RemoveItemAndDummyParents(HTREEITEM item);
	HTREEITEM AddItemInTreeCtrl(const char* name, const CString& location);
	HTREEITEM FindItemInTreeCtrl(const CString& name);

public:
	CSmartObjectClassDialog(CWnd* pParent = NULL, bool multi = false);   // standard constructor
	virtual ~CSmartObjectClassDialog();

	void    SetSOClass(const CString& sSOClass) { m_sSOClass = sSOClass; }
	CString GetSOClass()                        { return m_sSOClass; };
	void    EnableOK()                          { m_bEnableOK = true; }

	// Dialog Data
	enum { IDD = IDD_AITREEVIEW };
	enum { IDD_MULTIPLE = IDD_AITVMULTIPLE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnTVKeyDown(NMHDR*, LRESULT*);
	afx_msg void OnTVClick(NMHDR*, LRESULT*);
	afx_msg void OnTVDblClk(NMHDR*, LRESULT*);
	afx_msg void OnTVSelChanged(NMHDR*, LRESULT*);
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	//afx_msg void OnDeleteBtn();
	afx_msg void OnRefreshBtn();
	afx_msg void OnShowWindow(BOOL, UINT);

public:
	virtual BOOL OnInitDialog();
};

