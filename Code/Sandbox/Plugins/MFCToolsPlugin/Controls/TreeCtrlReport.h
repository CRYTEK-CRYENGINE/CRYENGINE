// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TreeCtrlReport_h__
#define __TreeCtrlReport_h__
#pragma once

enum { eTreeItemPathOptimalLen = 128 };
typedef CryStackStringT<char, eTreeItemPathOptimalLen> TreeItemPathString;

//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CTreeItemRecord : public CXTPReportRecord
{
	DECLARE_DYNAMIC(CTreeItemRecord)
public:
	CTreeItemRecord() : m_bIsGroup(false), m_nHeight(-1), m_bDropTarget(false), m_bDroppable(true), m_pUserData(0) {}
	CTreeItemRecord(bool bIsGroup, const char* name);

	void             CreateStdItems();

	void             SetGroup(bool bGroup)         { m_bIsGroup = bGroup; m_bDroppable = !m_bIsGroup ? true : m_bDroppable; }
	bool             IsGroup() const               { return m_bIsGroup; }

	void             SetDroppable(bool bDroppable) { m_bDroppable = bDroppable;    }
	bool             IsDroppable()                 { return m_bDroppable;  }

	const char*      GetName() const               { return m_name.c_str(); }
	void             SetName(const char* name);

	virtual void     SetIcon(int nIconIndex);
	virtual void     SetIcon2(int nIconIndex2);

	void             SetRect(const CRect& rc)           { m_clientRect = rc; }
	CRect            GetRect() const                    { return m_clientRect; }

	void             SetItemHeight(int nHeight)         { m_nHeight = nHeight; };
	int              GetItemHeight() const              { return m_nHeight; }

	void             SetDropTarget(bool bDropTarget)    { m_bDropTarget = bDropTarget; }
	bool             IsDropTarget() const               { return m_bDropTarget; }

	void             SetUserData(void* ptr)             { m_pUserData = ptr; }
	void*            GetUserData() const                { return m_pUserData; }

	void             SetUserString(const char* userStr) { m_userString = userStr; };
	const char*      GetUserString()                    { return m_userString.c_str(); };

	int              GetChildCount()                    { return GetChilds()->GetCount(); }
	CTreeItemRecord* GetChild(int nIndex)               { return (CTreeItemRecord*)GetChilds()->GetAt(nIndex); }

protected:
	int                m_nHeight;
	TreeItemPathString m_name;
	TreeItemPathString m_userString;
	CRect              m_clientRect;
	void*              m_pUserData;
	bool               m_bIsGroup;
	bool               m_bDropTarget;
	bool               m_bDroppable;
};

//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CTreeCtrlReport : public CXTPReportControl
{
	DECLARE_MESSAGE_MAP()
public:
	typedef std::vector<CTreeItemRecord*> Records;

	CTreeCtrlReport();
	~CTreeCtrlReport();

	enum ECallbackType
	{
		eCB_OnSelect,
		eCB_OnDblClick,
		eCB_OnDragAndDrop,
		eCB_LAST,
	};
	typedef Functor1<CTreeItemRecord*> Callback;

public:
	//////////////////////////////////////////////////////////////////////////
	virtual void Reload();
	virtual int  GetSelectedCount();
	virtual int  GetSelectedRecords(Records& items);

	// Helper functions.
	CXTPReportColumn* AddTreeColumn(const CString& name);
	CTreeItemRecord*  AddTreeRecord(CTreeItemRecord* pRecord, CTreeItemRecord* pParent);

	// Automatically creates groups from separators in the record name.
	void         EnableAutoNameGrouping(bool bEnable, int nGroupIcon);

	virtual void DeleteAllItems();

	// Only display records that contain filter text.
	void SetFilterText(const CString& filterText);

	// Make sure item is visible and all it's parents are expanded.
	void EnsureItemVisible(CTreeItemRecord* pRecord);

	// Find and select item by name
	bool           SelectRecordByName(const CString& name, CXTPReportRow* pParent = NULL);
	bool           SelectRecordByUserString(const CString& str, CXTPReportRow* pParent = NULL);
	bool           SelectRecordByPath(const char* pPath);

	void           SetExpandOnDblClick(bool bEnable);

	void           SetCallback(ECallbackType cbType, Callback cb);
	void           SetPathSeparators(const char* pSeparators);
	const CString& GetPathSeparators() const;
	void           DeleteRecordItem(CTreeItemRecord* const pRec);
	void           CalculateItemPath(CTreeItemRecord* const pRec, TreeItemPathString& rPath, bool bSlashPathSeparator = true);

protected:
	//////////////////////////////////////////////////////////////////////////
	// Can be overridden by derived classes.
	//////////////////////////////////////////////////////////////////////////
	virtual void             OnFillItems();
	virtual void             OnItemExpanded(CXTPReportRow* pRow, bool bExpanded) {};
	virtual void             OnSelectionChanged();
	virtual bool             OnBeginDragAndDrop(CXTPReportSelectedRows* pRows, CPoint point);
	virtual void             OnDragAndDrop(CXTPReportSelectedRows* pRows, CPoint absoluteCursorPos) {};
	virtual void             OnVerticalScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)      {};
	virtual void             OnItemDblClick(CXTPReportRow* pRow);
	virtual CTreeItemRecord* CreateGroupRecord(const char* name, int nGroupIcon);

	virtual CImageList*      CreateDragImage(CXTPReportRow* pRow);
	virtual bool             OnFilterTest(CTreeItemRecord* pRecord);
	//////////////////////////////////////////////////////////////////////////

protected:
	//////////////////////////////////////////////////////////////////////////
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd*);
	afx_msg void OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportRowExpandChanged(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportSelChanged(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnFocusChanged(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	afx_msg void OnDestroy();

	bool         IsHeaderVisible();
	void         SetHeaderVisible(bool bVisible);
	void         HighlightDropTarget(CTreeItemRecord* pItem);

	int          UpdateFilterTextRecursive(CXTPReportRecords* pRecords, int level);
	void         SetRecordVisibleRecursive(CTreeItemRecord* pRecord, bool bVisible);
	// removes record from m_groupNameToRecordMap, and its children, recursive
	void         RemoveItemsFromGroupsMapRecursive(CTreeItemRecord* pRec);
	bool         SelectRecordByPathRecursive(CTreeItemRecord* pRec, const std::vector<string>& rPathElements, uint32 aCurrentElementIndex);
	bool         FilterTest(const CString& filter, bool bAddOperation);

protected:
	CImageList              m_imageList;
	CImageList*             m_pDragImage;
	CXTPReportSelectedRows* m_pDragRows;

	CTreeItemRecord*        m_pDropTargetRow;

	uint32                  m_mask;
	CPoint                  m_ptDrag;

	bool                    m_bDragging;
	bool                    m_bDragEx;
	bool                    m_bHeaderVisible;

	bool                    m_bExpandOnDblClick;

	bool                    m_bAutoNameGrouping;
	int                     m_nGroupIcon;

	CString                 m_filterText, m_strPathSeparators;
	std::vector<CString>    m_filterKeywords;

	typedef std::map<TreeItemPathString, CTreeItemRecord*, stl::less_stricmp<TreeItemPathString>> GroupNameToRecordMap;
	GroupNameToRecordMap m_groupNameToRecordMap;

	Callback             m_callbacks[eCB_LAST];
};

#endif //__TreeCtrlReport_h__

