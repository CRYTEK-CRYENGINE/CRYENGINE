// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TreeCtrlReport.h"
#include <CryString/StringUtils.h>

IMPLEMENT_DYNAMIC(CTreeItemRecord, CXTPReportRecord)

// Custom text item that supports 2 icons.
class CXTPReportRecordItemText_Custom : public CXTPReportRecordItemText
{
	DECLARE_DYNAMIC(CXTPReportRecordItemText_Custom)
public:
	CXTPReportRecordItemText_Custom(LPCTSTR szText) : CXTPReportRecordItemText(szText), m_nIconIndex2(-1) {};
	void        SetIconIndex2(int nIconIndex) { m_nIconIndex2 = nIconIndex; };

	virtual int Draw(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs)
	{
		int res = __super::Draw(pDrawArgs);
		if (m_nIconIndex2 != -1)
		{
			CXTPReportPaintManager* pPaintManager = pDrawArgs->pControl->GetPaintManager();
			CRect rcItem = pDrawArgs->rcItem;
			rcItem.left -= 24;
			rcItem.top -= 1;
			rcItem.bottom += 1;
			pPaintManager->DrawItemBitmap(pDrawArgs, rcItem, m_nIconIndex2);
		}
		return 1;
	}
private:
	int m_nIconIndex2;
};
IMPLEMENT_DYNAMIC(CXTPReportRecordItemText_Custom, CXTPReportRecordItemText)

CTreeItemRecord::CTreeItemRecord(bool bIsGroup, const char* name)
	: m_name(name),
	m_bIsGroup(bIsGroup),
	m_nHeight(-1),
	m_bDropTarget(false),
	m_bDroppable(!bIsGroup),
	m_pUserData(0)
{
	CreateStdItems();
}

//////////////////////////////////////////////////////////////////////////
void CTreeItemRecord::CreateStdItems()
{
	CXTPReportRecordItem* pItem = new CXTPReportRecordItemText_Custom(m_name);
	AddItem(pItem);
	if (m_bIsGroup)
	{
		pItem->SetBold(TRUE);
		pItem->SetIconIndex(0);
	}
	else
		pItem->SetIconIndex(1);
}

//////////////////////////////////////////////////////////////////////////
void CTreeItemRecord::SetName(const char* name)
{
	m_name = name;
	if (GetItemCount() > 0)
	{
		GetItem(0)->SetCaption(name);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeItemRecord::SetIcon(int nIconIndex)
{
	if (GetItemCount() > 0)
	{
		CXTPReportRecordItemText_Custom* pItem0 = (CXTPReportRecordItemText_Custom*)GetItem(0);
		pItem0->SetIconIndex(nIconIndex);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeItemRecord::SetIcon2(int nIconIndex2)
{
	if (GetItemCount() > 0)
	{
		CXTPReportRecordItemText_Custom* pItem0 = (CXTPReportRecordItemText_Custom*)GetItem(0);
		pItem0->SetIconIndex2(nIconIndex2);
	}
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTreeCtrlReport, CXTPReportControl)
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_CAPTURECHANGED()
ON_WM_DESTROY()
ON_NOTIFY_REFLECT(XTP_NM_REPORT_HEADER_RCLICK, OnReportColumnRClick)
ON_NOTIFY_REFLECT(NM_DBLCLK, OnReportItemDblClick)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_ROWEXPANDED, OnReportRowExpandChanged)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_SELCHANGED, OnReportSelChanged)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_FOCUS_CHANGING, OnFocusChanged)
ON_WM_RBUTTONDOWN()
ON_WM_VSCROLL()
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

class CTreeCtrlReportPaintManager : public CXTPReportPaintManager
{
public:
	CTreeCtrlReportPaintManager(CTreeCtrlReport* pCtrl) : m_bHeaderVisible(true), m_pCtrl(pCtrl)
	{
		m_clrHighlight = GetSysColor(COLOR_HIGHLIGHT);
		m_clrHighlightText = RGB(0, 0, 0);
		m_clrSelectedRowText = RGB(0, 0, 0);
		m_clrSelectedRow = GetSysColor(COLOR_HIGHLIGHT);
	}

	virtual void DrawColumn(
	  CDC* pDC,
	  CXTPReportColumn* pColumn,
	  CXTPReportHeader* pHeader,
	  CRect rcColumn,
	  BOOL bDrawExternal
	  )
	{
		if (m_bHeaderVisible)
			__super::DrawColumn(pDC, pColumn, pHeader, rcColumn, bDrawExternal);
	}
	virtual void DrawItemCaption(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pMetrics)
	{
		if (pDrawArgs->pRow)
		{
			CTreeItemRecord* pRecord = (CTreeItemRecord*)pDrawArgs->pRow->GetRecord();
			if (pRecord)
			{
				pRecord->SetRect(pDrawArgs->pRow->GetRect());

				//if (pRecord->IsDropTarget())
				//pMetrics->clrForeground = RGB(255,0,0);
			}
		}
		//XTPPaintManager()->GradientFill( pDrawArgs->pDC,pDrawArgs->rcItem,RGB(200, 200, 250),RGB(220, 220, 250),FALSE );
		//pDrawArgs->pDC->FillSolidRect(pDrawArgs->rcItem, pMetrics->clrBackground);

		__super::DrawItemCaption(pDrawArgs, pMetrics);
	}

	virtual int GetRowHeight(CDC* pDC, CXTPReportRow* pRow)
	{
		CTreeItemRecord* pRecord = (CTreeItemRecord*)pRow->GetRecord();
		if (pRecord)
		{
			int nHeight = pRecord->GetItemHeight();
			if (nHeight > 0)
				return nHeight;
		}
		return __super::GetRowHeight(pDC, pRow) - 2;
	}

	virtual int GetHeaderHeight()
	{
		if (m_bHeaderVisible)
			return __super::GetHeaderHeight();
		else
			return 0;
	}

	bool             m_bHeaderVisible;
	CTreeCtrlReport* m_pCtrl;
};

//////////////////////////////////////////////////////////////////////////
CTreeCtrlReport::CTreeCtrlReport()
{
	//CXTPReportColumn *pNodeCol   = AddColumn(new CXTPReportColumn(0, _T("NodeClass"), 150, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	//CXTPReportColumn *pCatCol    = AddColumn(new CXTPReportColumn(1, _T("Category"), 50, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	//CXTPReportColumn *pNodeCol    = AddColumn(new CXTPReportColumn(1, _T("Node"), 50, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	//GetColumns()->GetGroupsOrder()->Add(pGraphCol);
	//pNodeCol->SetTreeColumn(true);
	//pNodeCol->SetSortable(FALSE);
	//pCatCol->SetSortable(FALSE);
	//GetColumns()->SetSortColumn(pNodeCol, true);

	GetReportHeader()->AllowColumnRemove(FALSE);
	GetReportHeader()->AllowColumnSort(FALSE);
	SkipGroupsFocus(TRUE);
	SetMultipleSelection(FALSE);
	AllowEdit(FALSE);
	EditOnClick(FALSE);
	ShowHeader(FALSE);
	SetSortRecordChilds(TRUE);

	ShadeGroupHeadings(FALSE);
	SetGroupRowsBold(TRUE);

	m_bDragging = false;
	m_bDragEx = false;
	m_ptDrag.SetPoint(0, 0);
	m_pDragImage = 0;
	m_pDragRows = 0;
	m_pDropTargetRow = 0;

	m_bExpandOnDblClick = false;

	m_bAutoNameGrouping = false;
	m_nGroupIcon = -1;

	CXTPReportPaintManager* pPMgr = new CTreeCtrlReportPaintManager(this);
	pPMgr->m_nTreeIndent = 0x0f;
	pPMgr->m_bShadeSortColumn = false;
	pPMgr->m_treeStructureStyle = xtpReportTreeStructureDots;
	//pPMgr->m_strNoItems = _T("Empty");
	pPMgr->SetGridStyle(FALSE, xtpGridNoLines);
	pPMgr->SetGridStyle(TRUE, xtpGridNoLines);
	SetPaintManager(pPMgr);

	ZeroStruct(m_callbacks);

	m_strPathSeparators = "\\/.";

	//CXTRegistryManager regMgr;
	//int showHeader = regMgr.GetProfileInt(_T("FlowGraph"), _T("ComponentShowHeader"), 1);
	//SetHeaderVisible(showHeader != 0);
	//SetHeaderVisible(true);
}

//////////////////////////////////////////////////////////////////////////
bool CTreeCtrlReport::IsHeaderVisible()
{
	return m_bHeaderVisible;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::SetHeaderVisible(bool bVisible)
{
	CTreeCtrlReportPaintManager* pPMgr = static_cast<CTreeCtrlReportPaintManager*>(GetPaintManager());
	pPMgr->m_bHeaderVisible = bVisible;
	m_bHeaderVisible = bVisible;
}

//////////////////////////////////////////////////////////////////////////
CTreeCtrlReport::~CTreeCtrlReport()
{
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnDestroy()
{
	//CXTRegistryManager regMgr;
	//regMgr.WriteProfileInt(_T("FlowGraph"), _T("ComponentShowHeader"), IsHeaderVisible() ? 1 : 0);
	__super::OnDestroy();
}

#define ID_SORT_ASC        1
#define ID_SORT_DESC       2
#define ID_SHOWHIDE_HEADER 3

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify->pColumn);
	CPoint ptClick = pItemNotify->pt;
	CXTPReportColumn* pColumn = pItemNotify->pColumn;

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	CXTPReportColumns* pColumns = GetColumns();
	CXTPReportColumn* pNodeClassColumn = pColumns->GetAt(0);
	CXTPReportColumn* pCatColumn = pColumns->GetAt(1);

	// create main menu items
	menu.AppendMenu(MF_STRING, ID_SORT_ASC, _T("Sort ascending"));
	menu.AppendMenu(MF_STRING, ID_SORT_DESC, _T("Sort descending"));
	menu.AppendMenu(MF_STRING, ID_SHOWHIDE_HEADER, IsHeaderVisible() ? _T("Hide Header") : _T("Show Header"));

	// track menu
	int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);

	// other general items
	switch (nMenuResult)
	{
	case ID_SORT_ASC:
	case ID_SORT_DESC:
		// only sort by node class
		if (pNodeClassColumn && pNodeClassColumn->IsSortable())
		{
			pColumns->SetSortColumn(pNodeClassColumn, nMenuResult == ID_SORT_ASC);
			Populate();
		}
		break;
	case ID_SHOWHIDE_HEADER:
		SetHeaderVisible(!IsHeaderVisible());
		Populate();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_bDragging)
	{
		__super::OnLButtonDown(nFlags, point);

		CRect reportArea = GetReportRectangle();
		if (reportArea.PtInRect(point))
		{
			CXTPReportRow* pRow = HitTest(point);
			if (pRow)
			{
				m_ptDrag = point;
				m_bDragEx = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_bDragEx = false;

	m_pDropTargetRow = 0;

	if (!m_bDragging)
	{
		__super::OnLButtonUp(nFlags, point);
	}
	else
	{
		m_bDragging = false;
		ReleaseCapture();
	}

	if (m_pDragImage && m_pDragRows)
	{
		CPoint p;
		GetCursorPos(&p);

		//ReleaseCapture();
		m_pDragImage->DragLeave(this);
		m_pDragImage->EndDrag();
		m_pDragImage->DeleteImageList();
		m_pDragImage = 0;

		if (m_pDragRows->GetCount() > 0)
		{
			OnDragAndDrop(m_pDragRows, p);

			if (m_callbacks[eCB_OnDragAndDrop])
			{
				// Must be last line in a function.
				CTreeItemRecord* pRecord = static_cast<CTreeItemRecord*>(m_pDragRows->GetAt(0)->GetRecord());
				m_callbacks[eCB_OnDragAndDrop](pRecord);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_bDragging)
	{
		__super::OnMouseMove(nFlags, point);

		const int dragX = ::GetSystemMetrics(SM_CXDRAG) + 5;
		const int dragY = ::GetSystemMetrics(SM_CYDRAG) + 5;
		if ((m_ptDrag.x || m_ptDrag.y) && (abs(point.x - m_ptDrag.x) > dragX || abs(point.y - m_ptDrag.y) > dragY))
		{
			CXTPReportRow* pRow = HitTest(m_ptDrag);
			if (pRow)
			{
				m_bDragging = true;
				CPoint wpoint = m_ptDrag;
				ClientToScreen(&wpoint);

				CXTPReportSelectedRows* pRows = GetSelectedRows();
				if (OnBeginDragAndDrop(pRows, wpoint))
					m_bDragging = true;
			}
		}
	}
	else
	{
		CXTPReportRow* pRow = HitTest(point);
		if (pRow != NULL && pRow->GetRecord())
		{
			CTreeItemRecord* pItem = static_cast<CTreeItemRecord*>(pRow->GetRecord());
			if (pItem)
			{
				if (m_pDropTargetRow)
					m_pDropTargetRow->SetDropTarget(false);
				m_pDropTargetRow = pItem;
				if (m_pDropTargetRow)
					m_pDropTargetRow->SetDropTarget(true);
			}
		}
	}
	if (m_pDragImage)
	{
		CRect rc;
		GetWindowRect(rc);
		CPoint p = point;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;
		m_pDragImage->DragMove(p);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnCaptureChanged(CWnd* pWnd)
{
	if (pWnd != this)
	{
		// Stop the dragging
		m_pDropTargetRow = 0;
		m_bDragging = false;
		m_bDragEx = false;
		m_ptDrag.SetPoint(0, 0);
	}
	__super::OnCaptureChanged(pWnd);
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	CXTPReportRow* pRow = pItemNotify->pRow;
	if (pRow)
	{
		OnItemDblClick(pRow);
	}
	*result = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnReportRowExpandChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify != NULL);
	CXTPReportRow* pRow = pItemNotify->pRow;
	if (pRow)
	{
		OnItemExpanded(pRow, pRow->IsExpanded() == TRUE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnReportSelChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	OnSelectionChanged();
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnSelectionChanged()
{
	if (m_callbacks[eCB_OnSelect])
	{
		CTreeItemRecord* pSel = 0;
		Records items;
		GetSelectedRecords(items);
		if (items.size() > 0)
			pSel = items[0];
		m_callbacks[eCB_OnSelect](pSel);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnFocusChanged(NMHDR* pNotifyStruct, LRESULT* result)
{
	if (pNotifyStruct)
	{
		//XTP_NM_REPORTREQUESTEDIT* pnm = (XTP_NM_REPORTREQUESTEDIT*) pNotifyStruct;
		//if (pnm->pRow)
		{

		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnFillItems()
{

}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::Reload()
{
	BeginUpdate();
	GetRecords()->RemoveAll();

	OnFillItems();

	EndUpdate();
	Populate();
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::DeleteAllItems()
{
	BeginUpdate();
	GetRecords()->RemoveAll();
	EndUpdate();
	m_groupNameToRecordMap.clear();
}

//////////////////////////////////////////////////////////////////////////
CImageList* CTreeCtrlReport::CreateDragImage(CXTPReportRow* pRow)
{
	CXTPReportRecord* pRecord = pRow->GetRecord();
	if (pRecord == 0)
		return 0;

	CXTPReportRecordItem* pItem = pRecord->GetItem(0);
	if (pItem == 0)
		return 0;

	CXTPReportColumn* pCol = GetColumns()->GetAt(0);
	assert(pCol != 0);

	CClientDC dc(this);
	CDC memDC;
	if (!memDC.CreateCompatibleDC(&dc))
		return NULL;

	CRect rect;
	rect.SetRectEmpty();

	XTP_REPORTRECORDITEM_DRAWARGS drawArgs;
	drawArgs.pDC = &memDC;
	drawArgs.pControl = this;
	drawArgs.pRow = pRow;
	drawArgs.pColumn = pCol;
	drawArgs.pItem = pItem;
	drawArgs.rcItem = rect;
	XTP_REPORTRECORDITEM_METRICS metrics;
	pRow->GetItemMetrics(&drawArgs, &metrics);

	CString text = pItem->GetCaption(drawArgs.pColumn);

	CFont* pOldFont = memDC.SelectObject(GetFont());
	memDC.SelectObject(metrics.pFont);
	memDC.DrawText(text, &rect, DT_CALCRECT);

	HICON hIcon = 0;
	CSize sz(0, 0);
	CXTPImageManagerIconSet* pIconSet = GetImageManager()->GetIconSet(pItem->GetIconIndex());
	if (pIconSet)
	{
		CXTPImageManagerIcon* pIcon = pIconSet->GetIcon(16);
		if (pIcon)
		{
			hIcon = pIcon->GetIcon();
			sz = pIcon->GetExtent();
		}
	}

	rect.right += sz.cx + 6;
	rect.bottom = sz.cy > rect.bottom ? sz.cy : rect.bottom;
	CRect boundingRect = rect;

	CBitmap bitmap;
	if (!bitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height()))
		return NULL;

	CBitmap* pOldMemDCBitmap = memDC.SelectObject(&bitmap);
	memDC.SetBkColor(metrics.clrBackground);
	memDC.FillSolidRect(&rect, metrics.clrBackground);
	memDC.SetTextColor(GetPaintManager()->m_clrWindowText);

	::DrawIconEx(*&memDC, rect.left + 2, rect.top, hIcon, sz.cx, sz.cy, 0, NULL, DI_NORMAL);

	rect.left += sz.cx + 4;
	rect.top -= 1;
	memDC.DrawText(text, &rect, 0);
	memDC.SelectObject(pOldFont);
	memDC.SelectObject(pOldMemDCBitmap);

	// Create image list
	CImageList* pImageList = new CImageList;
	pImageList->Create(boundingRect.Width(), boundingRect.Height(),
	                   ILC_COLOR | ILC_MASK, 0, 1);
	pImageList->Add(&bitmap, metrics.clrBackground); // Here green is used as mask color

	::DestroyIcon(hIcon);
	bitmap.DeleteObject();
	return pImageList;
}

//////////////////////////////////////////////////////////////////////////
int CTreeCtrlReport::GetSelectedCount()
{
	return GetSelectedRows()->GetCount();
}

//////////////////////////////////////////////////////////////////////////
int CTreeCtrlReport::GetSelectedRecords(Records& items)
{
	CXTPReportSelectedRows* pRows = GetSelectedRows();
	items.reserve(pRows->GetCount());
	for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		CTreeItemRecord* pRecord = (CTreeItemRecord*)pRow->GetRecord();
		if (pRecord)
		{
			items.push_back(pRecord);
		}
	}
	return items.size();
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	__super::OnVScroll(nSBCode, nPos, pScrollBar);
	OnVerticalScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
bool CTreeCtrlReport::OnBeginDragAndDrop(CXTPReportSelectedRows* pRows, CPoint pt)
{
	if (!pRows)
		return false;

	if (pRows->GetCount() == 0)
		return false;

	for (int i = 0; i < pRows->GetCount(); ++i)
	{
		CTreeItemRecord* pRec = static_cast<CTreeItemRecord*>(pRows->GetAt(i)->GetRecord());

		if (pRec == NULL)
			return false;

		if (!pRec->IsDroppable())
		{
			// Cannot drag&drop groups by default.
			return false;
		}
	}

	m_pDragRows = pRows;

	m_pDragImage = CreateDragImage(pRows->GetAt(0));
	if (m_pDragImage)
	{
		m_pDragImage->BeginDrag(0, CPoint(-10, -10));

		//m_dragNodeClass = pRec->GetName();

		ScreenToClient(&pt);

		CRect rc;
		GetWindowRect(rc);

		CPoint p = pt;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;

		RedrawControl();
		m_pDragImage->DragEnter(this, p);
		SetCapture();
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::HighlightDropTarget(CTreeItemRecord* pItem)
{
}

//////////////////////////////////////////////////////////////////////////
CXTPReportColumn* CTreeCtrlReport::AddTreeColumn(const CString& name)
{
	int index = GetColumns()->GetCount();
	CXTPReportColumn* pCol = AddColumn(new CXTPReportColumn(index, name, 100, TRUE, XTP_REPORT_NOICON, FALSE, TRUE));
	pCol->SetTreeColumn(TRUE);
	return pCol;
}

//////////////////////////////////////////////////////////////////////////
CTreeItemRecord* CTreeCtrlReport::AddTreeRecord(CTreeItemRecord* pRecord, CTreeItemRecord* pParent)
{
	if (pParent)
	{
		pParent->GetChilds()->Add(pRecord);
		return pRecord;
	}

	if (!m_bAutoNameGrouping)
	{
		AddRecord(pRecord);
	}
	else
	{
		TreeItemPathString itempath = pRecord->GetName();

		int pos = 0;

		TreeItemPathString prevToken;
		TreeItemPathString token = itempath.Tokenize(m_strPathSeparators.GetBuffer(), pos);

		CTreeItemRecord* pParent = 0;

		TreeItemPathString itemName;
		while (!token.empty())
		{
			prevToken = token;

			token = itempath.Tokenize(m_strPathSeparators.GetBuffer(), pos);
			if (token.empty())
			{
				pRecord->SetName(prevToken.c_str());
				break;
			}

			itemName += prevToken;
			itemName += "/";

			CTreeItemRecord* pGroup = stl::find_in_map(m_groupNameToRecordMap, itemName, NULL);
			if (!pGroup)
			{
				pGroup = CreateGroupRecord(prevToken.c_str(), m_nGroupIcon);
				if (pGroup)
				{
					if (pParent && pParent->GetChilds())
					{
						pParent->GetChilds()->Add(pGroup);
					}
					else
					{
						AddRecord(pGroup);
					}

					pParent = pGroup;
					m_groupNameToRecordMap[itemName] = pGroup;
				}
			}
			else
				pParent = pGroup;
		}
		if (pParent)
			pParent->GetChilds()->Add(pRecord);
		else
			AddRecord(pRecord);
	}

	return pRecord;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::EnableAutoNameGrouping(bool bEnable, int nGroupIcon)
{
	m_bAutoNameGrouping = bEnable;
	m_nGroupIcon = nGroupIcon;
}

//////////////////////////////////////////////////////////////////////////
CTreeItemRecord* CTreeCtrlReport::CreateGroupRecord(const char* name, int nGroupIcon)
{
	CTreeItemRecord* pRec = new CTreeItemRecord(true, name);
	pRec->SetIcon(nGroupIcon);
	return pRec;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::SetFilterText(const CString& filterText)
{
	// Change record hiding info.
	m_filterText = filterText;
	m_filterKeywords.clear();

	if (!m_filterText.IsEmpty())
	{
		CString token;
		for (int pos = 0; !(token = filterText.Tokenize(" ", pos)).IsEmpty(); )
		{
			m_filterKeywords.push_back(token);
		}
	}

	UpdateFilterTextRecursive(GetRecords(), 0);
	Populate();
	RedrawControl();
}

//////////////////////////////////////////////////////////////////////////
bool CTreeCtrlReport::FilterTest(const CString& filter, bool bAddOperation)
{
	size_t keywordsSize = m_filterKeywords.size();

	if (bAddOperation)
	{
		for (size_t key = 0; key < keywordsSize; ++key)
		{
			if (CryStringUtils::stristr(filter, m_filterKeywords[key]) == 0)
				return false;
		}
		return true;
	}

	for (size_t key = 0; key < keywordsSize; ++key)
	{
		if (CryStringUtils::stristr(filter, m_filterKeywords[key]) != 0)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CTreeCtrlReport::OnFilterTest(CTreeItemRecord* pRecord)
{
	return FilterTest(pRecord->GetName(), true);
}

//////////////////////////////////////////////////////////////////////////
int CTreeCtrlReport::UpdateFilterTextRecursive(CXTPReportRecords* pRecords, int level)
{
	int AnyVisible = 0;
	int num = pRecords->GetCount();
	for (int i = 0; i < num; i++)
	{
		CTreeItemRecord* pRecord = (CTreeItemRecord*)pRecords->GetAt(i);

		if (m_filterText.IsEmpty())
		{
			pRecord->SetVisible(true);
			UpdateFilterTextRecursive(pRecord->GetChilds(), level + 1);
			if (pRecord->IsExpanded())
				pRecord->SetExpanded(FALSE);
			continue;
		}
		else if (OnFilterTest(pRecord))
		{
			AnyVisible++;

			for (int k = 0, numChilds = pRecord->GetChildCount(); k < numChilds; k++)
			{
				pRecord->GetChild(k)->SetVisible(true);
			}
			SetRecordVisibleRecursive(pRecord, true);
			pRecord->SetExpanded(FALSE);

			continue;
		}

		int AnyChildsVisible = UpdateFilterTextRecursive(pRecord->GetChilds(), level + 1);

		if (AnyChildsVisible)
		{
			AnyVisible += AnyChildsVisible;
		}

		if (AnyChildsVisible)
		{
			pRecord->SetVisible(true);
			if (!pRecord->IsExpanded())
				pRecord->SetExpanded(TRUE);
		}
		else
		{
			pRecord->SetVisible(false);
		}
	}
	return AnyVisible;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::SetRecordVisibleRecursive(CTreeItemRecord* pRecord, bool bVisible)
{
	pRecord->SetVisible(bVisible);
	if (pRecord->HasChildren())
	{
		int num = pRecord->GetChildCount();
		for (int i = 0; i < num; i++)
		{
			CTreeItemRecord* pChildRecord = (CTreeItemRecord*)pRecord->GetChild(i);
			SetRecordVisibleRecursive(pChildRecord, bVisible);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTreeCtrlReport::RemoveItemsFromGroupsMapRecursive(CTreeItemRecord* pRec)
{
	TreeItemPathString path;

	CalculateItemPath(pRec, path);
	path = PathUtil::AddSlash(path.c_str());
	GroupNameToRecordMap::iterator iter = m_groupNameToRecordMap.find(path);

	if (m_groupNameToRecordMap.end() != iter)
	{
		m_groupNameToRecordMap.erase(iter);
	}

	if (pRec->HasChildren())
	{
		for (size_t i = 0, iCount = pRec->GetChildCount(); i < iCount; ++i)
		{
			CTreeItemRecord* pChildRecord = (CTreeItemRecord*)pRec->GetChild(i);

			RemoveItemsFromGroupsMapRecursive(pChildRecord);
		}
	}
}

bool CTreeCtrlReport::SelectRecordByPathRecursive(CTreeItemRecord* pRec, const std::vector<string>& rPathElements, uint32 aCurrentElementIndex)
{
	if (rPathElements.size() <= aCurrentElementIndex)
		return false;

	if (rPathElements[aCurrentElementIndex] == pRec->GetName())
	{
		if (rPathElements.size() == aCurrentElementIndex + 1)
		{
			EnsureItemVisible(pRec);

			// search again through the rows, because the old ones are destroyed
			CXTPReportRows* pRows = GetRows();

			for (int j = 0, nNumRows = pRows->GetCount(); j < nNumRows; j++)
			{
				CXTPReportRow* pRow = pRows->GetAt(j);

				if (pRow->GetRecord() == pRec)
				{
					pRow->SetSelected(TRUE);
					break;
				}
			}

			return true;
		}

		if (!pRec->GetChildCount())
			return true;

		for (size_t i = 0, iCount = pRec->GetChildCount(); i < iCount; ++i)
		{
			// go visit kids
			bool bRet = SelectRecordByPathRecursive(pRec->GetChild(i), rPathElements, aCurrentElementIndex + 1);

			if (!bRet)
			{
				continue;
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::OnItemDblClick(CXTPReportRow* pRow)
{
	if (m_bExpandOnDblClick)
	{
		CTreeItemRecord* pRecord = (CTreeItemRecord*)pRow->GetRecord();
		if (pRecord && pRecord->IsGroup())
		{
			pRow->SetExpanded(!pRow->IsExpanded());
		}
	}
	CTreeItemRecord* pRecord = (CTreeItemRecord*)pRow->GetRecord();
	if (pRecord && m_callbacks[eCB_OnDblClick])
	{
		m_callbacks[eCB_OnDblClick](pRecord);
	}
}

void CTreeCtrlReport::SetExpandOnDblClick(bool bEnable)
{
	m_bExpandOnDblClick = bEnable;
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::EnsureItemVisible(CTreeItemRecord* pRecord)
{
	if (!pRecord)
		return;

	bool bWasExpanded = false;

	CXTPReportRecord* pParent = pRecord->GetParentRecord();
	while (pParent)
	{
		if (!pParent->IsExpanded())
		{
			pParent->SetExpanded(TRUE);
			bWasExpanded = true;
		}
		pParent = pParent->GetParentRecord();
	}
	if (bWasExpanded)
	{
		Populate();
	}
	CXTPReportRows* pRows = GetRows();

	if (!pRows)
		return;

	for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);

		if (!pRow)
			continue;

		if (pRow->GetRecord() == pRecord)
		{
			pRow->SetSelected(TRUE);
			EnsureVisible(pRow);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTreeCtrlReport::SelectRecordByName(const CString& name, CXTPReportRow* pParent)
{
	CXTPReportRows* pRows;
	if (pParent)
		pRows = pParent->GetChilds();
	else
		pRows = GetRows();

	int num = pRows->GetCount();
	for (int i = 0; i < num; i++)
	{
		CTreeItemRecord* pRecord = (CTreeItemRecord*)pRows->GetAt(i)->GetRecord();
		if (!pRecord)
			continue;

		if (stricmp(pRecord->GetName(), name) == 0)
		{
			EnsureItemVisible(pRecord);

			// search again through the rows, because the old ones are destroyed
			CXTPReportRows* pRows = GetRows();
			for (int j = 0, nNumRows = pRows->GetCount(); j < nNumRows; j++)
			{
				CXTPReportRow* pRow = pRows->GetAt(j);
				if (pRow->GetRecord() == pRecord)
				{
					pRow->SetSelected(TRUE);
					break;
				}
			}

			return true;
		}
		if (SelectRecordByName(name, pRows->GetAt(i)))
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CTreeCtrlReport::SelectRecordByUserString(const CString& str, CXTPReportRow* pParent)
{
	CXTPReportRows* pRows(NULL);
	if (pParent)
	{
		pRows = pParent->GetChilds();
	}
	else
	{
		pRows = GetRows();
	}

	if (!pRows)
	{
		assert(!pRows && "CTreeCtrlReport::SelectRecordByUserString");
		return false;
	}

	int num = pRows->GetCount();
	for (int i = 0; i < num; i++)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		if (!pRow)
		{
			assert(!pRow && "CTreeCtrlReport::SelectRecordByUserString");
			continue;
		}

		CTreeItemRecord* pRecord = (CTreeItemRecord*)pRow->GetRecord();
		if (!pRecord)
		{
			assert(!pRecord && "CTreeCtrlReport::SelectRecordByUserString");
			continue;
		}

		const char* pString = pRecord->GetUserString();
		if (!pString)
		{
			assert(!pString && "CTreeCtrlReport::SelectRecordByUserString");
			continue;
		}

		if (stricmp(pString, str) == 0)
		{
			EnsureItemVisible(pRecord);

			// search again through the rows, because the old ones are destroyed
			CXTPReportRows* pCandiateRows = GetRows();
			if (!pCandiateRows)
			{
				assert(!pCandiateRows && "CTreeCtrlReport::SelectRecordByUserString");
				continue;
			}

			for (int j = 0, nNumRows = pCandiateRows->GetCount(); j < nNumRows; j++)
			{
				CXTPReportRow* pSelectedRow = pCandiateRows->GetAt(j);
				if (!pSelectedRow)
				{
					assert(!pSelectedRow && "CTreeCtrlReport::SelectRecordByUserString");
					continue;
				}

				if (pSelectedRow->GetRecord() == pRecord)
				{
					pSelectedRow->SetSelected(TRUE);
					break;
				}
			}

			return true;
		}

		if (SelectRecordByUserString(str, pRows->GetAt(i)))
		{
			return true;
		}

	}

	return false;
}

bool CTreeCtrlReport::SelectRecordByPath(const char* pPath)
{
	std::vector<string> pathElements;

	SplitString(string(pPath), pathElements, '/');

	if (pathElements.empty())
		return false;

	if (!GetRows())
		return false;

	if (!GetRows()->GetCount())
		return false;

	CTreeItemRecord* pRec = (CTreeItemRecord*)GetRows()->GetAt(0)->GetRecord();

	return SelectRecordByPathRecursive(pRec, pathElements, 0);
}

//////////////////////////////////////////////////////////////////////////
void CTreeCtrlReport::SetCallback(ECallbackType cbType, Callback cb)
{
	m_callbacks[cbType] = cb;
}

void CTreeCtrlReport::SetPathSeparators(const char* pSeparators)
{
	m_strPathSeparators = pSeparators;
}

const CString& CTreeCtrlReport::GetPathSeparators() const
{
	return m_strPathSeparators;
}

void CTreeCtrlReport::DeleteRecordItem(CTreeItemRecord* pRec)
{
	if (m_bAutoNameGrouping)
	{
		RemoveItemsFromGroupsMapRecursive(pRec);
	}

	pRec->Delete();
}

void CTreeCtrlReport::CalculateItemPath(CTreeItemRecord* const pRec, TreeItemPathString& rPath, bool bSlashPathSeparator)
{
	rPath = pRec->GetName();
	CTreeItemRecord* pParent = (CTreeItemRecord*)pRec->GetParentRecord();

	TreeItemPathString path;
	while (pParent)
	{
		path = pParent->GetName();
		if (bSlashPathSeparator)
			path = PathUtil::AddSlash(path);
		else
			path = PathUtil::AddBackslash(path.c_str());

		path += rPath;
		rPath = path;
		pParent = (CTreeItemRecord*)pParent->GetParentRecord();
	}
}

