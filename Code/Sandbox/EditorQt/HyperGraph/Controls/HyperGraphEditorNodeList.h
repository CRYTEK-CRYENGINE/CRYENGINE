// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CHyperGraphDialog;


class CComponentRecord : public CXTPReportRecord
{
public:
	CComponentRecord(bool bIsGroup = true, const CString& name = "")
		: m_name(name), m_bIsGroup(bIsGroup)
	{}

	bool           IsGroup() const { return m_bIsGroup; }
	const CString& GetName() const { return m_name; }
protected:
	CString        m_name;
	bool           m_bIsGroup;
};


class CHyperGraphComponentsReportCtrl : public CXTPReportControl
{
	DECLARE_MESSAGE_MAP()
public:
	CHyperGraphComponentsReportCtrl();

	CImageList* CreateDragImage(CXTPReportRow* pRow);
	void        SetDialog(CHyperGraphDialog* pDialog) { m_pDialog = pDialog; }
	void        Reload();
	void        SetComponentFilterMask(uint32 mask)   { m_mask = mask; }
	void        SetSearchKeyword(const CString& keyword);

protected:
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd*);
	afx_msg void OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result);

protected:
	CImageList           m_imageList;
	CHyperGraphDialog*   m_pDialog;
	uint32               m_mask;
	CPoint               m_ptDrag;
	bool                 m_bDragging;
	bool                 m_bDragEx;
	std::vector<CString> m_searchKeywords;
};

