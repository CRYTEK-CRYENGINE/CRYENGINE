// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FacialSlidersCtrl.h"
#include "FacialEdContext.h"
#include "Util/MFCUtil.h"
#include "Resource.h"

IMPLEMENT_DYNCREATE(CFacialSlidersCtrl, CListCtrlEx)

#define ID_BASE_TEXT     1000
#define ID_BASE_NUM_CTRL 2000
#define SLIDER_SCALE     100

#define COLUMN_NAME      0
#define COLUMN_SLIDER    1
#define COLUMN_NUMBER    2
#define COLUMN_BALANCE   3

/////////////////////////////////////////////////////////////////////////////
// CFacialSlidersCtrl
CFacialSlidersCtrl::CFacialSlidersCtrl()
{
	m_pContext = 0;
	m_bCreated = false;
	m_nTextWidth = 120;
	m_nSliderWidth = 149;
	m_nSliderHeight = 20;
	m_bIgnoreSliderChange = false;
	m_bLDragging = false;
	m_pDragImage = 0;
	m_nDraggedItem = -1;
	m_bShowExpressions = false;
}

CFacialSlidersCtrl::~CFacialSlidersCtrl()
{
}

BEGIN_MESSAGE_MAP(CFacialSlidersCtrl, CListCtrlEx)
ON_WM_SIZE()
ON_WM_HSCROLL()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()

//ON_NOTIFY_RANGE( NM_RCLICK,0,200,OnRClickSlider )
//ON_NOTIFY_RANGE( NM_SETFOCUS,0,200,OnRClickSlider )
//ON_NOTIFY_RANGE( NM_CLICK,0,200,OnRClickSlider )
ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnItemChanged)
ON_NOTIFY_REFLECT(LVN_BEGINDRAG, OnBeginDrag)
ON_NOTIFY_REFLECT(NM_RCLICK, OnRClick)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFacialSlidersCtrl message handlers

BOOL CFacialSlidersCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	BOOL bRes = CListCtrlEx::Create(dwStyle | LVS_REPORT | WS_TABSTOP, rect, pParentWnd, nID);
	m_bCreated = true;

	SetExtendedStyle(LVS_EX_FLATSB | LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP | LVS_EX_CHECKBOXES);
	InsertColumn(COLUMN_NAME, "Effector", LVCFMT_LEFT, m_nTextWidth, 0);
	InsertColumn(COLUMN_SLIDER, "Weight", LVCFMT_LEFT, m_nSliderWidth, 1);
	InsertColumn(COLUMN_NUMBER, "Val", LVCFMT_CENTER, 45, 2);
	InsertColumn(COLUMN_BALANCE, "Balance", LVCFMT_CENTER, 50, 3);
	EnableUserRowColor(true);
	//SetItemHeight(0,20);

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_FACED_LIBTREE, 16, RGB(255, 0, 255));
	SetImageList(&m_imageList, LVSIL_SMALL);

	RecalcLayout();
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::ClearSliders()
{
	/*
	   for (int i = 0; i < m_sliders.size(); i++)
	   {
	   SSliderInfo &si = m_sliders[i];
	   delete si.pText;
	   delete si.pSlider;
	   }
	 */
	m_sliders.clear();
	if (m_hWnd)
		DeleteAllItems();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::CreateSlider(int nIndex, IFacialEffector* pEffector)
{
	CRect rc(0, 0, 0, 0);
	CString str;
	str.Format("%s", pEffector->GetName());
	SSliderInfo& si = m_sliders[nIndex];
	si.pEffector = pEffector;
	si.pSlider = new CSliderCtrlCustomDraw;
	si.pSlider->Create(WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_NOTICKS | TBS_TOOLTIPS | TBS_FIXEDLENGTH, rc, this, nIndex);
	si.pSlider->SendMessage(TBM_SETTHUMBLENGTH, 15, 0);
	si.pSlider->ModifyStyle(WS_BORDER, 0);
	si.pSlider->SetRange(-SLIDER_SCALE, SLIDER_SCALE);
	si.pSlider->SetTicFreq(100);
	//si.pSlider->EnableWindow(FALSE);

	//CEdit *pEdit = new CEdit;
	//pEdit->Create( WS_CHILD|WS_VISIBLE,rc,this,ID_BASE_NUM_CTRL+i );
	//pEdit->SetWindowText
	//si.pNumberCtrl = new CNumberCtrl;
	//si.pNumberCtrl->Create( this,rc,ID_BASE_NUM_CTRL+i,CNumberCtrl::NOBORDER );
	//si.pNumberCtrl->SetRange( -1,1 );

	LVITEM lvi;
	lvi.mask = LVIF_INDENT | LVIF_TEXT | LVIF_IMAGE;
	lvi.iItem = nIndex;
	lvi.iIndent = 0;
	lvi.iSubItem = COLUMN_NAME;
	lvi.stateMask = 0;
	lvi.state = 0;
	lvi.pszText = const_cast<char*>((const char*)str);
	if (m_bShowExpressions)
		lvi.iImage = 1;
	else
		lvi.iImage = 2;
	int id = InsertItem(&lvi);

	SetItemControl(id, COLUMN_SLIDER, si.pSlider);
	//SetItemControl( id,COLUMN_NUMBER,si.pNumberCtrl );
	SetItemText(id, COLUMN_NUMBER, "0");

	si.pBalance = new CNumberCtrl;
	si.pBalance->Create(this, nIndex);
	si.pBalance->SetRange(-1, 1);
	si.pBalance->SetInternalPrecision(3);
	si.pBalance->SetValue(0);
	si.pBalance->EnableWindow(TRUE);
	si.pBalance->SetUpdateCallback(functor(*this, &CFacialSlidersCtrl::OnBalanceChanged));
	SetItemControl(id, COLUMN_BALANCE, si.pBalance);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::CreateSliders()
{
	int i;
	ClearSliders();

	if (!m_pContext)
		return;

	std::vector<IFacialEffector*> effectors;
	if (m_bShowExpressions)
		m_pContext->GetAllEffectors(effectors, 0, EFE_TYPE_EXPRESSION);
	else
		m_pContext->GetAllEffectors(effectors, 0, EFE_TYPE_MORPH_TARGET);

	int nSliders = (int)effectors.size();
	m_sliders.resize(nSliders);
	for (i = 0; i < nSliders; i++)
	{
		IFacialEffector* pEffector = effectors[i];
		CreateSlider(i, pEffector);
	}
	RecalcLayout();
	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::RecalcLayout()
{
	if (!m_bCreated)
		return;

	CRect rcClient;
	GetClientRect(rcClient);

	int nSliderWidth = GetColumnWidth(COLUMN_SLIDER);
	int nTextrWidth = GetColumnWidth(COLUMN_NAME);
	int nNewTextWidth = rcClient.Width() - nSliderWidth - 4 - 45 - 50;
	if (nNewTextWidth < m_nTextWidth)
		nNewTextWidth = m_nTextWidth;
	SetColumnWidth(COLUMN_NAME, nNewTextWidth);

	return;

	rcClient.DeflateRect(1, 1, 1, 1);

	CRect rc(rcClient);
	int y = rc.top;
	int x = 1;
	for (int i = 0; i < m_sliders.size(); i++)
	{
		SSliderInfo& si = m_sliders[i];
		CRect rcText = CRect(x + rc.left, y, x + rc.left + m_nTextWidth, y + m_nSliderHeight);
		CRect rcSlider = CRect(x + rc.left + m_nTextWidth, y, x + rc.left + m_nSliderWidth, y + m_nSliderHeight);
		si.rc.UnionRect(rcText, rcSlider);
		//		si.pText->MoveWindow(rcText);
		//	si.pSlider->MoveWindow(rcSlider);
		y += m_nSliderHeight + 1;
		if ((y + m_nSliderHeight - 4) > rc.Height())
		{
			if (rc.right > si.rc.right + si.rc.Width() / 2)
			{
				y = rc.top;
				x += si.rc.Width() + 5;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	for (int i = 0; i < m_sliders.size(); i++)
	{
		SSliderInfo& si = m_sliders[i];
		if ((void*)si.pSlider == (void*)pScrollBar)
		{
			OnSliderChanged(i);

			// Send a special facial event to redraw preview window to make preview smooth.
			if (m_pContext)
				m_pContext->SendEvent(EFD_EVENT_REDRAW_PREVIEW);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::UpdateSliderUI(int nSlider)
{
	if (nSlider < 0 || nSlider >= m_sliders.size())
		return;

	SSliderInfo& si = m_sliders[nSlider];

	float fWeight = (float)si.pSlider->GetPos() / SLIDER_SCALE;

	COLORREF rowColor = GetXtremeColor(COLOR_WINDOWTEXT);
	if (si.bEnabled)
	{
		if (fWeight < 0)
			rowColor = RGB(255, 0, 0);
		else
			rowColor = RGB(0, 160, 0);
	}

	if (si.bEnabled)
	{
		if (si.rowColor != rowColor)
		{
			si.rowColor = rowColor;
			SetRedraw(FALSE);
			SetRowColor(nSlider, rowColor, GetXtremeColor(COLOR_WINDOW));
			SetCheck(nSlider, TRUE);
			SetRedraw(TRUE);
		}

		int p = si.pSlider->GetPos();
		if (p > 0)
			si.pSlider->SetSelection(0, p);
		else if (p < 0)
			si.pSlider->SetSelection(p, 0);
		else
			si.pSlider->SetSelection(0, 0);
	}
	else
	{
		si.pSlider->SetSelection(0, 0);
		if (si.rowColor != rowColor)
		{
			si.rowColor = rowColor;
			SetRowColor(nSlider, rowColor, GetXtremeColor(COLOR_WINDOW));
			SetCheck(nSlider, FALSE);
		}
	}

	CString str;
	if (fabs(fWeight) > 0.001f)
		str.Format("%.2f", fWeight);
	else
		str = "0";
	SetItemText(nSlider, COLUMN_NUMBER, str);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnSliderChanged(int nSlider)
{
	if (m_bIgnoreSliderChange)
		return;
	if (nSlider < 0 || nSlider >= m_sliders.size())
		return;
	if (!m_pContext)
		return;

	{
		SSliderInfo& si = m_sliders[nSlider];
		float fWeight = (float)si.pSlider->GetPos() / SLIDER_SCALE;

		if ((fabs(fWeight) > 0.00001f || fabs(si.pBalance->GetValue()) > 0.00001f) && !si.bEnabled)
			si.bEnabled = true;
		UpdateSliderUI(nSlider);

		CString str;
		str.Format("%.2f", fWeight);
		SetItemText(nSlider, COLUMN_NUMBER, str);
	}

	std::vector<float> weights;
	std::vector<float> balances;
	weights.resize(m_sliders.size());
	balances.resize(m_sliders.size());
	for (int i = 0; i < m_sliders.size(); i++)
	{
		weights[i] = 0;
		balances[i] = 0;
		SSliderInfo& si = m_sliders[i];
		if (si.bEnabled)
		{
			if (si.pSlider->GetPos() != 0)
			{
				float fWeight = (float)si.pSlider->GetPos() / SLIDER_SCALE;
				weights[i] = fWeight;
			}
			balances[i] = si.pBalance->GetValue();
		}
	}
	if (!m_bShowExpressions)
	{
		m_pContext->SetFromSliders(weights, balances);
	}
	else
	{
		std::vector<IFacialEffector*> effectors;
		effectors.resize(m_sliders.size());
		for (int i = 0; i < m_sliders.size(); i++)
		{
			effectors[i] = m_sliders[i].pEffector;
		}
		if (m_pContext->pInstance && m_sliders.size() > 0)
			m_pContext->pInstance->PreviewEffectors(&effectors[0], &weights[0], &balances[0], m_sliders.size());
	}
}

#define MENU_CMD_CLEAR_ALL 1
//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, MENU_CMD_CLEAR_ALL, _T("Clear All"));

	//menu.AppendMenu

	CPoint pos;
	GetCursorPos(&pos);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, pos.x, pos.y, this);
	switch (cmd)
	{
	case MENU_CMD_CLEAR_ALL:
		OnClearAll();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::SetRowColor(int iRow, COLORREF crText, COLORREF crBack)
{
	ROWCOLOR* lpRowClr = Lookup(iRow);
	if (lpRowClr)
	{
		lpRowClr->crText = crText;
		lpRowClr->crBack = crBack;
	}
	else
	{
		// initialize row color struct.
		ROWCOLOR rowclr;
		rowclr.iRow = iRow;
		rowclr.crText = crText;
		rowclr.crBack = crBack;

		m_arRowColor.AddHead(rowclr);
	}

	if (!m_pListCtrl || !m_pListCtrl->GetSafeHwnd())
		return;

	// Redraw window if row is visible
	const int nTopIndex = m_pListCtrl->GetTopIndex();
	const int nBotIndex = nTopIndex + m_pListCtrl->GetCountPerPage();
	if (iRow >= nTopIndex && iRow <= nBotIndex)
	{
		CRect rect;
		m_pListCtrl->GetItemRect(iRow, rect, LVIR_BOUNDS);
		m_pListCtrl->InvalidateRect(rect, TRUE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnClearAll()
{
	bool bAnyChanged = false;
	for (int i = 0; i < m_sliders.size(); i++)
	{
		if (m_sliders[i].pSlider->GetPos() != 0 || m_sliders[i].pBalance->GetValue() != 0)
		{
			m_sliders[i].pSlider->SetPos(0);
			m_sliders[i].pBalance->SetValue(0);
			OnSliderChanged(i);
			bAnyChanged = true;
		}
	}
	if (bAnyChanged)
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::SetMorphWeight(IFacialEffector* pEffector, float fWeight)
{
	for (SliderContainer::iterator itSlider = m_sliders.begin(), itEnd = m_sliders.end(); itSlider != itEnd; ++itSlider)
	{
		if (stricmp((*itSlider).pEffector->GetName(), pEffector->GetName()) == 0)
		{
			(*itSlider).pSlider->SetPos(fWeight * SLIDER_SCALE);
			(*itSlider).pBalance->SetValue(0);
			OnSliderChanged(itSlider - m_sliders.begin());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnRClickSlider(UINT nID, NMHDR* pNMHDR, LRESULT* pResult)
{
	int nIndex = nID;
	if (nIndex >= 0 && nIndex < m_sliders.size())
	{
		SSliderInfo& si = m_sliders[nIndex];
		//si.bEnabled = si.pText->GetCheck();
		si.pSlider->SetPos(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;
	if (m_pContext)
		m_pContext->RegisterListener(this);
	CreateSliders();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	switch (event)
	{
	case EFD_EVENT_ADD:
	case EFD_EVENT_REMOVE:
	case EFD_EVENT_CHANGE_RELOAD:
		if (m_bShowExpressions)
		{
			CreateSliders();
		}
		break;
	case EFD_EVENT_CHANGE:
		break;
	case EFD_EVENT_SELECT_EFFECTOR:
		{
			//OnClearAll();
			//UpdateValuesFromSelected();
		}
		break;
	}
}

void CFacialSlidersCtrl::OnBalanceChanged(CNumberCtrl* numberCtrl)
{
	OnSliderChanged(numberCtrl->GetDlgCtrlID());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::UpdateValuesFromSelected()
{
	if (m_bShowExpressions)
		return;

	SetRedraw(FALSE);
	if (m_pContext->pSelectedEffector && m_pContext->pSelectedEffector->GetType() == EFE_TYPE_EXPRESSION)
	{
		m_bIgnoreSliderChange = true;
		for (int i = 0; i < m_sliders.size(); i++)
		{
			SSliderInfo& si = m_sliders[i];
			si.bEnabled = false;
			si.pSlider->SetPos(0);
			for (int j = 0; j < m_pContext->pSelectedEffector->GetSubEffectorCount(); j++)
			{
				IFacialEffCtrl* pController = m_pContext->pSelectedEffector->GetSubEffCtrl(j);
				if (pController->GetType() != IFacialEffCtrl::CTRL_LINEAR)
					continue;
				if (stricmp(pController->GetEffector()->GetName(), si.pEffector->GetName()) == 0)
				{
					float fWeight = pController->GetConstantWeight();
					si.pSlider->SetPos(fWeight * SLIDER_SCALE);
					si.bEnabled = true;
				}
			}
			UpdateSliderUI(i);
		}
		m_bIgnoreSliderChange = false;
	}
	Invalidate();
	SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	*pResult = 0;

	if (pNMListView->uOldState == 0 && pNMListView->uNewState == 0)
		return; // No change

	BOOL bPrevState = (BOOL)(((pNMListView->uOldState &
	                           LVIS_STATEIMAGEMASK) >> 12) - 1); // Old check box state
	if (bPrevState < 0)                                          // On startup there's no previous state
		bPrevState = 0;                                            // so assign as false (unchecked)

	// New check box state
	BOOL bChecked = (BOOL)(((pNMListView->uNewState & LVIS_STATEIMAGEMASK) >> 12) - 1);
	if (bChecked < 0) // On non-checkbox notifications assume false
		bChecked = 0;

	if (bPrevState == bChecked) // No change in check box
		return;

	int nIndex = pNMListView->iItem;
	if (nIndex >= 0 && nIndex < m_sliders.size())
	{
		SSliderInfo& si = m_sliders[nIndex];
		si.bEnabled = bChecked;
		if (!bChecked)
		{
			si.pSlider->SetPos(0);
			si.pBalance->SetValue(0);
		}
		UpdateSliderUI(nIndex);
		OnSliderChanged(nIndex);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* pNMListView = (NMLISTVIEW*)pNMHDR;
	*pResult = 0;

	IFacialEffector* pEffector = 0;
	if (pNMListView->iItem >= 0)
	{
		SSliderInfo& si = m_sliders[pNMListView->iItem];
		pEffector = si.pEffector;
	}

	if (pEffector && m_pContext)
		m_pContext->DoExpressionNameDragDrop(pEffector, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bLDragging)
	{
		POINT pt = point;
		ClientToScreen(&pt);

		CRect rc;
		AfxGetMainWnd()->GetWindowRect(rc);
		pt.x -= rc.left;
		pt.y -= rc.top;

		CImageList::DragMove(pt);

		/*
		   hitem = HitTest(point, &flags);
		   if (m_hitemDrop != hitem)
		   {
		   CImageList::DragShowNolock(FALSE);
		   SelectDropTarget(hitem);
		   m_hitemDrop = hitem;
		   CImageList::DragShowNolock(TRUE);
		   }

		   if(hitem)
		   hitem = GetDropTarget(hitem);
		   if (hitem)
		   SetCursor(m_dropCursor);
		   else
		   SetCursor(m_noDropCursor);
		 */
	}

	__super::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bLDragging)
	{
		m_bLDragging = false;
		CImageList::DragLeave(this);
		CImageList::EndDrag();
		ReleaseCapture();

		delete m_pDragImage;

		/*
		   // Remove drop target highlighting
		   SelectDropTarget(NULL);

		   m_hitemDrop = GetDropTarget(m_hitemDrop);
		   if(m_hitemDrop == NULL)
		   return;

		   if( m_hitemDrag == m_hitemDrop )
		   return;

		   Expand( m_hitemDrop, TVE_EXPAND ) ;

		   HTREEITEM htiNew = CopyBranch( m_hitemDrag, m_hitemDrop, TVI_LAST );
		   DeleteItem(m_hitemDrag);
		   SelectItem( htiNew );
		 */
	}

	__super::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersCtrl::SetShowExpressions(bool bShowExpressions)
{
	m_bShowExpressions = bShowExpressions;
}

