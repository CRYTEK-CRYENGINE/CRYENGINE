// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperGraphEditorNodeList.h"

#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraphEditorWnd.h"
#include "Util/MFCUtil.h"

//////////////////////////////////////////////////////////////////////////
// CHyperGraphComponentsReportCtrl

BEGIN_MESSAGE_MAP(CHyperGraphComponentsReportCtrl, CXTPReportControl)
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_CAPTURECHANGED()
ON_NOTIFY_REFLECT(NM_DBLCLK, OnReportItemDblClick)
END_MESSAGE_MAP()

CHyperGraphComponentsReportCtrl::CHyperGraphComponentsReportCtrl()
{
	m_pDialog = 0;

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_HYPERGRAPH_COMPONENTS, 16, RGB(255, 0, 255));
	SetImageList(&m_imageList);

	CXTPReportColumn* pNodeCol = AddColumn(new CXTPReportColumn(0, _T("Node"), 150, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	CXTPReportColumn* pCatCol = AddColumn(new CXTPReportColumn(1, _T("Category"), 50, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	pNodeCol->SetTreeColumn(true);
	pNodeCol->SetSortable(FALSE);
	pCatCol->SetSortable(FALSE);
	GetColumns()->SetSortColumn(pNodeCol, true);
	GetReportHeader()->AllowColumnRemove(FALSE);
	ShowHeader(FALSE);
	ShadeGroupHeadings(FALSE);
	SkipGroupsFocus(TRUE);
	SetMultipleSelection(FALSE);

	m_bDragging = false;
	m_bDragEx = false;
	m_ptDrag.SetPoint(0, 0);

	GetPaintManager()->m_nTreeIndent = 0x0f;
	GetPaintManager()->m_bShadeSortColumn = false;
	GetPaintManager()->m_strNoItems = _T("Use View->Nodes menu\nto show items");
	GetPaintManager()->SetGridStyle(FALSE, xtpGridNoLines);
	GetPaintManager()->SetGridStyle(TRUE, xtpGridNoLines);
}


void CHyperGraphComponentsReportCtrl::OnLButtonDown(UINT nFlags, CPoint point)
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
				if (pRow->GetParentRow() != 0)
				{
					m_ptDrag = point;
					m_bDragEx = true;
				}
			}
		}
	}
}

void CHyperGraphComponentsReportCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_bDragEx = false;

	if (!m_bDragging)
	{
		__super::OnLButtonUp(nFlags, point);
	}
	else
	{
		m_bDragging = false;
		ReleaseCapture();
	}
}

void CHyperGraphComponentsReportCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_bDragging)
	{
		__super::OnMouseMove(nFlags, point);

		const int dragX = ::GetSystemMetrics(SM_CXDRAG);
		const int dragY = ::GetSystemMetrics(SM_CYDRAG);
		if ((m_ptDrag.x || m_ptDrag.y) && (abs(point.x - m_ptDrag.x) > dragX || abs(point.y - m_ptDrag.y) > dragY))
		{
			CXTPReportRow* pRow = HitTest(m_ptDrag);
			if (pRow)
			{
				m_bDragging = true;
				SetCapture();
				point = m_ptDrag;
				ClientToScreen(&point);
				m_pDialog->OnComponentBeginDrag(pRow, point);
			}
		}
	}
}

void CHyperGraphComponentsReportCtrl::OnCaptureChanged(CWnd* pWnd)
{
	// Stop the dragging
	m_bDragging = false;
	m_bDragEx = false;
	m_ptDrag.SetPoint(0, 0);
	__super::OnCaptureChanged(pWnd);
}

void CHyperGraphComponentsReportCtrl::OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	CXTPReportRow* pRow = pItemNotify->pRow;
	if (pRow)
	{
		pRow->SetExpanded(pRow->IsExpanded() ? FALSE : TRUE);
	}
}

namespace
{
struct NodeFilter
{
public:
	NodeFilter(uint32 mask) : mask(mask) {}
	bool Visit(CHyperNode* pNode)
	{
		if (pNode->IsEditorSpecialNode())
			return false;
		CFlowNode* pFlowNode = static_cast<CFlowNode*>(pNode);
		if ((pFlowNode->GetCategory() & mask) == 0)
			return false;
		return true;

		// Only if the usage mask is set check if fulfilled -> this is an exclusive thing
		if ((mask & EFLN_USAGE_MASK) != 0 && (pFlowNode->GetUsageFlags() & mask) == 0)
			return false;
		return true;
	}
	uint32 mask;
};
};

void CHyperGraphComponentsReportCtrl::SetSearchKeyword(const CString& keyword)
{
	CString searchwords = keyword;
	searchwords.MakeLower();

	m_searchKeywords.clear();

	int pos = 0;
	do
	{
		CString search = searchwords.Tokenize(" ", pos);
		search.Trim();
		m_searchKeywords.push_back(search);
	}
	while (pos != -1);

	Reload();
}

void CHyperGraphComponentsReportCtrl::Reload()
{
	std::map<CString, CComponentRecord*> groupMap;

	BeginUpdate();
	GetRecords()->RemoveAll();

	CFlowGraphManager* pMgr = GetIEditorImpl()->GetFlowGraphManager();
	std::vector<THyperNodePtr> prototypes;
	NodeFilter filter(m_mask);

	pMgr->GetPrototypesEx(prototypes, true, functor_ret(filter, &NodeFilter::Visit));
	for (int i = 0; i < prototypes.size(); i++)
	{
		const CHyperNode* pNode = prototypes[i];
		const CFlowNode* pFlowNode = static_cast<const CFlowNode*>(pNode);

		CString fullClassName = pFlowNode->GetUIClassName();

		// If search keyword is given,
		if (!m_searchKeywords.empty())
		{
			CString text = fullClassName;
			text.MakeLower();

			bool ok = true;
			for (size_t word = 0; word < m_searchKeywords.size(); ++word)
			{
				if (-1 == text.Find(m_searchKeywords[word]))
				{
					ok = false;
					break;
				}
			}

			if (!ok)
				continue;
		}

		CComponentRecord* pItemGroupRec = 0;
		int midPos = 0;
		int pos = 0;
		int len = fullClassName.GetLength();
		bool bUsePrototypeName = false; // use real prototype name or stripped from fullClassName

		CString nodeShortName;
		CString groupName = fullClassName.Tokenize(":", pos);

		// check if a group name is given. if not, fake 'Misc:' group
		if (pos < 0 || pos >= len)
		{
			bUsePrototypeName = true; // re-fetch real prototype name
			fullClassName.Insert(0, "Misc:");
			pos = 0;
			len = fullClassName.GetLength();
			groupName = fullClassName.Tokenize(":", pos);
		}

		CString pathName = "";
		while (pos >= 0 && pos < len)
		{
			pathName += groupName;
			pathName += ":";
			midPos = pos;

			CComponentRecord* pGroupRec = stl::find_in_map(groupMap, pathName, 0);
			if (pGroupRec == 0)
			{
				pGroupRec = new CComponentRecord();
				CXTPReportRecordItem* pNewItem = new CXTPReportRecordItemText(groupName);
				pNewItem->SetIconIndex(0);
				pGroupRec->AddItem(pNewItem);
				pGroupRec->AddItem(new CXTPReportRecordItemText(""));

				if (pItemGroupRec != 0)
				{
					pItemGroupRec->GetChilds()->Add(pGroupRec);
				}
				else
				{
					AddRecord(pGroupRec);
				}
				groupMap[pathName] = pGroupRec;
				pItemGroupRec = pGroupRec;
			}
			else
			{
				pItemGroupRec = pGroupRec;
			}

			// continue stripping
			groupName = fullClassName.Tokenize(":", pos);
		}
		;

		// short node name without ':'. used for display in last column
		nodeShortName = fullClassName.Mid(midPos);

		CComponentRecord* pRec = new CComponentRecord(false, pFlowNode->GetClassName());
		CXTPReportRecordItemText* pNewItem = new CXTPReportRecordItemText(nodeShortName);
		CXTPReportRecordItemText* pNewCatItem = new CXTPReportRecordItemText(pFlowNode->GetCategoryName());
		pRec->AddItem(pNewItem);
		pRec->AddItem(pNewCatItem);

		switch (pFlowNode->GetCategory())
		{
		case EFLN_APPROVED:
			pNewItem->SetTextColor(RGB(104, 215, 142));
			pNewCatItem->SetTextColor(RGB(104, 215, 142));
			break;
		case EFLN_ADVANCED:
			pNewItem->SetTextColor(RGB(104, 162, 255));
			pNewCatItem->SetTextColor(RGB(104, 162, 255));
			break;
		case EFLN_DEBUG:
			pNewItem->SetTextColor(RGB(220, 180, 20));
			pNewCatItem->SetTextColor(RGB(220, 180, 20));
			break;
		case EFLN_OBSOLETE:
			pNewItem->SetTextColor(RGB(255, 0, 0));
			pNewCatItem->SetTextColor(RGB(255, 0, 0));
			break;
		default:
			pNewItem->SetTextColor(RGB(0, 0, 0));
			pNewCatItem->SetTextColor(RGB(0, 0, 0));
			break;
		}

		assert(pItemGroupRec != 0);
		if (pItemGroupRec)
			pItemGroupRec->GetChilds()->Add(pRec);
		else
			AddRecord(pRec);
	}

	EndUpdate();
	Populate();
}

CImageList* CHyperGraphComponentsReportCtrl::CreateDragImage(CXTPReportRow* pRow)
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

	HICON hIcon = m_imageList.ExtractIcon(pItem->GetIconIndex());
	int sx, sy;
	ImageList_GetIconSize(*&m_imageList, &sx, &sy);
	rect.right += sx + 6;
	rect.bottom = sy > rect.bottom ? sy : rect.bottom;
	CRect boundingRect = rect;

	CBitmap bitmap;
	if (!bitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height()))
		return NULL;

	CBitmap* pOldMemDCBitmap = memDC.SelectObject(&bitmap);
	memDC.SetBkColor(metrics.clrBackground);
	memDC.FillSolidRect(&rect, metrics.clrBackground);
	memDC.SetTextColor(GetPaintManager()->m_clrWindowText);

	::DrawIconEx(*&memDC, rect.left + 2, rect.top, hIcon, sx, sy, 0, NULL, DI_NORMAL);

	rect.left += sx + 4;
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

