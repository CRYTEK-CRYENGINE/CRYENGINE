// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CSmartObjectStateDialog dialog

class CSmartObjectStateDialog : public CDialog
{
	DECLARE_DYNAMIC(CSmartObjectStateDialog)

private:
	CString    m_sSOState;
	CTreeCtrl  m_TreeCtrl;
	bool       m_bMultiple;
	bool       m_bHasAdvanced;

	CImageList m_ImageList;

	typedef std::map<CString, HTREEITEM> MapStringToItem;
	MapStringToItem m_mapStringToItem;

	typedef std::set<CString> TStatesSet;
	TStatesSet m_positive, m_negative;
	//	int m_selCount;

	void      UpdateListCurrentStates();

	HTREEITEM FindOrInsertItem(const CString& name, HTREEITEM parent);
	HTREEITEM ForcePath(const CString& location);
	void      RemoveItemAndDummyParents(HTREEITEM item);
	HTREEITEM AddItemInTreeCtrl(const char* name, const CString& location);
	HTREEITEM FindItemInTreeCtrl(const CString& name);

public:
	CSmartObjectStateDialog(CWnd* pParent = NULL, bool multi = false, bool hasAdvanced = false);   // standard constructor
	virtual ~CSmartObjectStateDialog();

	void    SetSOState(const CString& sSOState) { m_sSOState = sSOState; }
	CString GetSOState()                        { return m_sSOState; };

	// Dialog Data
	enum { IDD = IDD_AIANCHORS };
	enum { IDD_MULTIPLE = IDD_AIMULTIPLESTATES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnTVKeyDown(NMHDR*, LRESULT*);
	afx_msg void OnTVClick(NMHDR*, LRESULT*);
	afx_msg void OnTVDblClk(NMHDR*, LRESULT*);
	afx_msg void OnTVSelChanged(NMHDR*, LRESULT*);
	//	afx_msg void OnLbnDblClk();
	//	afx_msg void OnLbnSelchangeState();
	afx_msg void OnNewBtn();
	afx_msg void OnEditBtn();
	//	afx_msg void OnDeleteBtn();
	afx_msg void OnRefreshBtn();
	afx_msg void OnAdvancedBtn();

	//	afx_msg void OnDrawItem( int id, LPDRAWITEMSTRUCT lpDrawItemStruct );
	//	afx_msg int OnCompareItem( int id, LPCOMPAREITEMSTRUCT lpCompareItemStruct );

	void OnOK();

public:
	virtual BOOL OnInitDialog();
};

