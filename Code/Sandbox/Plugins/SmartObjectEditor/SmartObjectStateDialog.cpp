// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemDescriptionDlg.h"
#include "SmartObjectsEditorDialog.h"
#include "SmartObjectStateDialog.h"
#include "AI\AIManager.h"
#include "Controls/QuestionDialog.h"
#include "Util/MFCUtil.h"

// CSmartObjectStateDialog dialog

IMPLEMENT_DYNAMIC(CSmartObjectStateDialog, CDialog)
CSmartObjectStateDialog::CSmartObjectStateDialog(CWnd* pParent /*=NULL*/, bool multi /*=false*/, bool hasAdvanced /*= false*/)
	: CDialog(multi ? CSmartObjectStateDialog::IDD_MULTIPLE : CSmartObjectStateDialog::IDD, pParent)
	, m_bMultiple(multi)
	, m_bHasAdvanced(hasAdvanced)
{
}

CSmartObjectStateDialog::~CSmartObjectStateDialog()
{
}

void CSmartObjectStateDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE, m_TreeCtrl);

	CMFCUtils::LoadTrueColorImageList(m_ImageList, IDB_SOSTATES_TREE, 16, RGB(255, 0, 255));
	m_TreeCtrl.SetImageList(&m_ImageList, TVSIL_NORMAL);
}

HTREEITEM CSmartObjectStateDialog::FindOrInsertItem(const CString& name, HTREEITEM parent)
{
	HTREEITEM item = m_TreeCtrl.GetChildItem(parent);
	while (item && name.CompareNoCase(m_TreeCtrl.GetItemText(item)) > 0)
		item = m_TreeCtrl.GetNextSiblingItem(item);
	if (!item)
		return m_TreeCtrl.InsertItem(name, parent, TVI_LAST);
	if (name.CompareNoCase(m_TreeCtrl.GetItemText(item)) == 0)
		return item;
	item = m_TreeCtrl.InsertItem(name, parent, TVI_SORT);
	return item;
}

HTREEITEM CSmartObjectStateDialog::ForcePath(const CString& location)
{
	HTREEITEM item = 0;
	CString token;
	int i = 0;
	while (!(token = location.Tokenize("/", i)).IsEmpty())
		item = FindOrInsertItem('/' + token, item);
	return item;
}

void CSmartObjectStateDialog::RemoveItemAndDummyParents(HTREEITEM item)
{
	assert(item && !m_TreeCtrl.ItemHasChildren(item));

	unsigned count = m_mapStringToItem.erase(m_TreeCtrl.GetItemText(item));
	assert(count == 1);

	while (item && !m_TreeCtrl.ItemHasChildren(item))
	{
		HTREEITEM parent = m_TreeCtrl.GetParentItem(item);
		m_TreeCtrl.DeleteItem(item);
		item = parent;
	}
}

HTREEITEM CSmartObjectStateDialog::AddItemInTreeCtrl(const char* name, const CString& location)
{
	HTREEITEM parent = ForcePath(location);
	HTREEITEM item = m_TreeCtrl.InsertItem(name, 1, 1, parent, TVI_SORT);
	m_mapStringToItem[name] = item;
	return item;
}

HTREEITEM CSmartObjectStateDialog::FindItemInTreeCtrl(const CString& name)
{
	MapStringToItem::iterator it = m_mapStringToItem.find(name);
	if (it == m_mapStringToItem.end())
		return 0;
	else
		return it->second;
}

BEGIN_MESSAGE_MAP(CSmartObjectStateDialog, CDialog)
//	ON_LBN_DBLCLK(IDC_ANCHORS, OnLbnDblClk)
//	ON_LBN_SELCHANGE(IDC_ANCHORS, OnLbnSelchangeState)
ON_BN_CLICKED(IDC_NEW, OnNewBtn)
ON_BN_CLICKED(IDEDIT, OnEditBtn)
ON_BN_CLICKED(IDREFRESH, OnRefreshBtn)
//	ON_BN_CLICKED(IDC_RADIO1, OnRefreshBtn)
//	ON_BN_CLICKED(IDC_RADIO2, OnRefreshBtn)
ON_BN_CLICKED(IDCONTINUE, OnAdvancedBtn)
ON_WM_DRAWITEM()
ON_WM_COMPAREITEM()
ON_NOTIFY(TVN_KEYDOWN, IDC_TREE, OnTVKeyDown)
ON_NOTIFY(NM_CLICK, IDC_TREE, OnTVClick)
ON_NOTIFY(NM_DBLCLK, IDC_TREE, OnTVDblClk)
ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, OnTVSelChanged)
END_MESSAGE_MAP()

// CSmartObjectStateDialog message handlers

void CSmartObjectStateDialog::OnOK()
{
	if (m_bMultiple)
	{
		CString positive, negative;
		m_sSOState.Empty();
		TStatesSet::const_iterator it, itEnd = m_positive.end();
		for (it = m_positive.begin(); it != itEnd; ++it)
		{
			if (positive.IsEmpty())
				positive = *it;
			else
			{
				positive += ',';
				positive += *it;
			}
		}
		itEnd = m_negative.end();
		for (it = m_negative.begin(); it != itEnd; ++it)
		{
			if (negative.IsEmpty())
				negative = *it;
			else
			{
				negative += ',';
				negative += *it;
			}
		}
		m_sSOState = positive;
		if (!negative.IsEmpty())
		{
			m_sSOState += '-';
			m_sSOState += negative;
		}
	}
	__super::OnOK();
}

/*
   int CSmartObjectStateDialog::OnCompareItem( int id, LPCOMPAREITEMSTRUCT lpCompareItemStruct )
   {
   return stricmp( (LPCTSTR) lpCompareItemStruct->itemData1, (LPCTSTR) lpCompareItemStruct->itemData2 );
   }

   void CSmartObjectStateDialog::OnDrawItem( int id, LPDRAWITEMSTRUCT lpDrawItemStruct )
   {
   ASSERT( lpDrawItemStruct->CtlType == ODT_LISTBOX );

   CDC dc;
   dc.Attach(lpDrawItemStruct->hDC);

   // Save these value to restore them when done drawing.
   COLORREF crOldTextColor = dc.GetTextColor();
   COLORREF crOldBkColor = dc.GetBkColor();

   // erase rect by filling it with the background color
   dc.SetTextColor( 0 );
   dc.SetBkMode( TRANSPARENT );
   CString item;
   m_wndStateList.GetText( lpDrawItemStruct->itemID, item );
   CRect rc = lpDrawItemStruct->rcItem;
   if ( m_positive.find( item ) != m_positive.end() )
   {
    dc.FillSolidRect( &lpDrawItemStruct->rcItem, 0xC0FFC0 );

    dc.MoveTo( rc.left + 4, rc.top + 9 );
    dc.LineTo( rc.left + 6, rc.top + 11 );
    dc.LineTo( rc.left + 11, rc.top + 6 );
    dc.MoveTo( rc.left + 4, rc.top + 8 );
    dc.LineTo( rc.left + 6, rc.top + 10 );
    dc.LineTo( rc.left + 11, rc.top + 5 );
   }
   else if ( m_negative.find( item ) != m_negative.end() )
   {
    dc.FillSolidRect( &lpDrawItemStruct->rcItem, 0xC0C0FF );

    dc.MoveTo( rc.left + 4, rc.top + 5 );
    dc.LineTo( rc.left + 10, rc.top + 11 );
    dc.MoveTo( rc.left + 5, rc.top + 5 );
    dc.LineTo( rc.left + 11, rc.top + 11 );
    dc.MoveTo( rc.left + 4, rc.top + 10 );
    dc.LineTo( rc.left + 10, rc.top + 4 );
    dc.MoveTo( rc.left + 5, rc.top + 10 );
    dc.LineTo( rc.left + 11, rc.top + 4 );
   }
   else
    dc.FillSolidRect( &lpDrawItemStruct->rcItem, crOldBkColor );

   if ((lpDrawItemStruct->itemAction & ODA_FOCUS) && (lpDrawItemStruct->itemState & ODS_FOCUS))
   {
    dc.DrawFocusRect( &lpDrawItemStruct->rcItem );
   }

   // Draw the text
   rc.left += 15;
   rc.right -= 2;
   dc.DrawText( item, rc, DT_SINGLELINE|DT_VCENTER );

   // Reset the background color and the text color back to their original values
   dc.SetTextColor( crOldTextColor );
   dc.SetBkColor( crOldBkColor );

   dc.Detach();
   }
 */

void CSmartObjectStateDialog::OnNewBtn()
{
	CItemDescriptionDlg dlg(this, true, false, true);
	dlg.m_sCaption = "New State";
	dlg.m_sItemCaption = "State &name:";
	dlg.m_sDescription = "";
	dlg.m_sLocation = "";
	HTREEITEM current = m_TreeCtrl.GetSelectedItem();
	while (current)
	{
		if (m_TreeCtrl.ItemHasChildren(current))
		{
			if (!dlg.m_sLocation.IsEmpty())
				dlg.m_sLocation = m_TreeCtrl.GetItemText(current) + '/' + dlg.m_sLocation;
			else
				dlg.m_sLocation = m_TreeCtrl.GetItemText(current);
		}
		current = m_TreeCtrl.GetParentItem(current);
	}
	if (dlg.DoModal() == IDOK && CSOLibrary::StartEditing())
	{
		HTREEITEM item = FindItemInTreeCtrl(dlg.m_sItemEdit);
		if (item)
		{
			if (m_bMultiple)
			{
				m_TreeCtrl.EnsureVisible(item);
				m_TreeCtrl.SelectItem(item);
			}
			else
			{
				m_TreeCtrl.EnsureVisible(item);
				m_TreeCtrl.SelectItem(item);

				SetDlgItemText(IDCANCEL, "Cancel");
				GetDlgItem(IDOK)->EnableWindow(TRUE);
			}

			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Entered state name already exists...\n\nA new state with the same name can not be created!"));
		}
		else
		{
			CSOLibrary::AddState(dlg.m_sItemEdit, dlg.m_sDescription, dlg.m_sLocation);
			item = AddItemInTreeCtrl(dlg.m_sItemEdit, dlg.m_sLocation);

			if (m_bMultiple)
			{
				m_TreeCtrl.EnsureVisible(item);
				m_TreeCtrl.SelectItem(item);
			}
			else
			{
				m_TreeCtrl.EnsureVisible(item);
				m_TreeCtrl.SelectItem(item);

				SetDlgItemText(IDCANCEL, "Cancel");
				GetDlgItem(IDOK)->EnableWindow(TRUE);
			}
		}
	}
}

void CSmartObjectStateDialog::OnEditBtn()
{
	HTREEITEM item = m_TreeCtrl.GetSelectedItem();
	if (!item || m_TreeCtrl.ItemHasChildren(item))
		return;

	CString name = m_TreeCtrl.GetItemText(item);
	CSOLibrary::VectorStateData::iterator it = CSOLibrary::FindState(name);
	assert(it != CSOLibrary::GetStates().end());

	CItemDescriptionDlg dlg(this, false, false, true);
	dlg.m_sCaption = "Edit Smart Object State";
	dlg.m_sItemCaption = "State &name:";
	dlg.m_sItemEdit = name;
	dlg.m_sDescription = it->description;
	dlg.m_sLocation = it->location;
	if (dlg.DoModal() == IDOK)
	{
		if (CSOLibrary::StartEditing())
		{
			it->description = dlg.m_sDescription;
			if (it->location != dlg.m_sLocation)
			{
				it->location = dlg.m_sLocation;
				RemoveItemAndDummyParents(item);
				item = AddItemInTreeCtrl(name, dlg.m_sLocation);
				m_TreeCtrl.EnsureVisible(item);
				m_TreeCtrl.SelectItem(item);
			}
			SetDlgItemText(IDC_DESCRIPTION, dlg.m_sDescription);
		}
	}
}

void CSmartObjectStateDialog::OnAdvancedBtn()
{
	CString positive, negative;
	m_sSOState.Empty();
	TStatesSet::const_iterator it, itEnd = m_positive.end();
	for (it = m_positive.begin(); it != itEnd; ++it)
	{
		if (positive.IsEmpty())
			positive = *it;
		else
		{
			positive += ',';
			positive += *it;
		}
	}
	itEnd = m_negative.end();
	for (it = m_negative.begin(); it != itEnd; ++it)
	{
		if (negative.IsEmpty())
			negative = *it;
		else
		{
			negative += ',';
			negative += *it;
		}
	}
	m_sSOState = positive;
	if (!negative.IsEmpty())
	{
		m_sSOState += '-';
		m_sSOState += negative;
	}

	EndDialog(IDCONTINUE);
}

void CSmartObjectStateDialog::OnRefreshBtn()
{
	//	m_TreeCtrl.UpdateWindow();
	m_positive.clear();
	m_negative.clear();

	if (m_bMultiple)
	{
		CString token;
		int i = 0;
		CString sPositive = !m_sSOState.IsEmpty() && m_sSOState.GetAt(0) != '-' ? m_sSOState.Tokenize("-", i) : "";
		CString sNegative = !m_sSOState.IsEmpty() ? m_sSOState.Tokenize("-", i) : "";

		i = 0;
		while (!(token = sPositive.Tokenize(" !\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", i)).IsEmpty())
		{
			if (CItemDescriptionDlg::ValidateItem(token))
			{
				if (CSOLibrary::FindState(token) == CSOLibrary::GetStates().end())
					CSOLibrary::AddState(token, "", "");
				m_positive.insert(token);
			}
		}

		i = 0;
		while (!(token = sNegative.Tokenize(" !\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", i)).IsEmpty())
		{
			if (CItemDescriptionDlg::ValidateItem(token))
			{
				if (CSOLibrary::FindState(token) == CSOLibrary::GetStates().end())
					CSOLibrary::AddState(token, "", "");
				m_negative.insert(token);
			}
		}

		UpdateListCurrentStates();
	}

	m_mapStringToItem.clear();
	m_TreeCtrl.DeleteAllItems();
	CSOLibrary::VectorStateData::iterator it, itEnd = CSOLibrary::GetStates().end();
	for (it = CSOLibrary::GetStates().begin(); it != itEnd; ++it)
		AddItemInTreeCtrl(it->name, it->location);

	if (m_bMultiple)
	{
		TStatesSet::iterator it, itEnd = m_positive.end();
		for (it = m_positive.begin(); it != itEnd; ++it)
		{
			HTREEITEM item = FindItemInTreeCtrl(*it);
			m_TreeCtrl.EnsureVisible(item);
			m_TreeCtrl.SelectItem(item);
			m_TreeCtrl.SetItemImage(item, 2, 2);
			m_TreeCtrl.SetItemState(item, TVIS_BOLD, TVIS_BOLD);
		}

		itEnd = m_negative.end();
		for (it = m_negative.begin(); it != itEnd; ++it)
		{
			HTREEITEM item = FindItemInTreeCtrl(*it);
			m_TreeCtrl.EnsureVisible(item);
			m_TreeCtrl.SelectItem(item);
			m_TreeCtrl.SetItemImage(item, 3, 3);
			m_TreeCtrl.SetItemState(item, TVIS_BOLD, TVIS_BOLD);
		}
	}
	else
	{
		HTREEITEM item = FindItemInTreeCtrl(m_sSOState);
		m_TreeCtrl.SetItemState(item, TVIS_BOLD, TVIS_BOLD);
		m_TreeCtrl.EnsureVisible(item);
		m_TreeCtrl.SelectItem(item);
	}
}

void CSmartObjectStateDialog::UpdateListCurrentStates()
{
	// calls only allowed if multiple selections are needed
	assert(m_bMultiple);

	CString positive, negative;
	TStatesSet::const_iterator it, itEnd = m_positive.end();
	for (it = m_positive.begin(); it != itEnd; ++it)
	{
		if (positive.IsEmpty())
			positive = *it;
		else
		{
			positive += ',';
			positive += *it;
		}
	}
	SetDlgItemText(IDC_SELECTIONLIST, positive);
	itEnd = m_negative.end();
	for (it = m_negative.begin(); it != itEnd; ++it)
	{
		if (negative.IsEmpty())
			negative = *it;
		else
		{
			negative += ',';
			negative += *it;
		}
	}
	SetDlgItemText(IDC_SELECTIONLIST2, negative);

	SetDlgItemText(IDCANCEL, "Cancel");
	GetDlgItem(IDOK)->EnableWindow(TRUE);
}

/*
   void CSmartObjectStateDialog::OnLbnDblClk()
   {
   if ( !m_bMultiple && m_wndStateList.GetCurSel() >= 0 )
    EndDialog( IDOK );
   else if ( m_bMultiple && m_wndStateList.GetCaretIndex() >= 0 )
   {
    m_selCount = -1;
    OnTVSelChanged(NULL,NULL);
    OnLbnSelchangeState();
   }
   }
 */

void CSmartObjectStateDialog::OnTVKeyDown(NMHDR* pNMHdr, LRESULT* pResult)
{
	if (!m_bMultiple)
		return;

	NMTVKEYDOWN* nmkd = (NMTVKEYDOWN*) pNMHdr;
	if (nmkd->wVKey != ' ')
		return;

	if (HTREEITEM item = m_TreeCtrl.GetSelectedItem())
	{
		if (m_TreeCtrl.ItemHasChildren(item))
		{
			//	m_TreeCtrl.SetCheck( item, !m_TreeCtrl.GetCheck(item) );
		}
		else
		{
			int nImage, nSelImage;
			m_TreeCtrl.GetItemImage(item, nImage, nSelImage);
			nImage = nImage == 3 ? 1 : nImage + 1;
			m_TreeCtrl.SetItemImage(item, nImage, nImage);
			m_TreeCtrl.SetItemState(item, nImage == 1 ? 0 : TVIS_BOLD, TVIS_BOLD);

			CString text(m_TreeCtrl.GetItemText(item));
			switch (nImage)
			{
			case 1:
				m_positive.erase(text);
				m_negative.erase(text);
				break;
			case 2:
				m_positive.insert(text);
				m_negative.erase(text);
				break;
			case 3:
				m_positive.erase(text);
				m_negative.insert(text);
				break;
			}
			UpdateListCurrentStates();
		}
	}
}

void CSmartObjectStateDialog::OnTVClick(NMHDR* pNMHdr, LRESULT* pResult)
{
	if (!m_bMultiple)
		return;

	TVHITTESTINFO hti;
	GetCursorPos(&hti.pt);
	m_TreeCtrl.ScreenToClient(&hti.pt);
	m_TreeCtrl.HitTest(&hti);
	if (hti.hItem && hti.flags == TVHT_ONITEMICON)
	{
		if (m_TreeCtrl.ItemHasChildren(hti.hItem))
		{
			//	m_TreeCtrl.SetCheck( hti.hItem, !m_TreeCtrl.GetCheck(hti.hItem) );
		}
		else
		{
			int nImage, nSelImage;
			m_TreeCtrl.GetItemImage(hti.hItem, nImage, nSelImage);
			nImage = nImage == 3 ? 1 : nImage + 1;
			m_TreeCtrl.SetItemImage(hti.hItem, nImage, nImage);
			m_TreeCtrl.SetItemState(hti.hItem, nImage == 1 ? 0 : TVIS_BOLD, TVIS_BOLD);

			CString text(m_TreeCtrl.GetItemText(hti.hItem));
			switch (nImage)
			{
			case 1:
				m_positive.erase(text);
				m_negative.erase(text);
				break;
			case 2:
				m_positive.insert(text);
				m_negative.erase(text);
				break;
			case 3:
				m_positive.erase(text);
				m_negative.insert(text);
				break;
			}
			UpdateListCurrentStates();
		}
	}
}

void CSmartObjectStateDialog::OnTVDblClk(NMHDR*, LRESULT*)
{
	if (HTREEITEM item = m_TreeCtrl.GetSelectedItem())
	{
		if (!m_TreeCtrl.ItemHasChildren(item))
		{
			if (!m_bMultiple)
				EndDialog(IDOK);
			else
			{
				int nImage, nSelImage;
				m_TreeCtrl.GetItemImage(item, nImage, nSelImage);
				nImage = nImage == 3 ? 1 : nImage + 1;
				m_TreeCtrl.SetItemImage(item, nImage, nImage);
				m_TreeCtrl.SetItemState(item, nImage == 1 ? 0 : TVIS_BOLD, TVIS_BOLD);

				CString text(m_TreeCtrl.GetItemText(item));
				switch (nImage)
				{
				case 1:
					m_positive.erase(text);
					m_negative.erase(text);
					break;
				case 2:
					m_positive.insert(text);
					m_negative.erase(text);
					break;
				case 3:
					m_positive.erase(text);
					m_negative.insert(text);
					break;
				}
				UpdateListCurrentStates();
			}
		}
	}
}

void CSmartObjectStateDialog::OnTVSelChanged(NMHDR*, LRESULT*)
{
	HTREEITEM item = m_TreeCtrl.GetSelectedItem();
	if (!item || m_TreeCtrl.ItemHasChildren(item))
	{
		SetDlgItemText(IDC_DESCRIPTION, "");
	}
	else
	{
		CSOLibrary::VectorStateData::iterator it = CSOLibrary::FindState(m_TreeCtrl.GetItemText(item));
		SetDlgItemText(IDC_DESCRIPTION, it->description);
	}

	if (m_bMultiple || !item)
		return;
	if (!m_TreeCtrl.ItemHasChildren(item))
	{
		SetDlgItemText(IDCANCEL, "Cancel");
		GetDlgItem(IDOK)->EnableWindow(TRUE);

		if (HTREEITEM old = FindItemInTreeCtrl(m_sSOState))
			m_TreeCtrl.SetItemState(old, 0, TVIS_BOLD);
		m_sSOState = m_TreeCtrl.GetItemText(item);
		m_TreeCtrl.SetItemState(item, TVIS_BOLD, TVIS_BOLD);
		SetWindowText("Smart Object State: " + m_sSOState);
	}
}

/*
   void CSmartObjectStateDialog::OnLbnSelchangeState()
   {
   SetDlgItemText( IDCANCEL, "Cancel" );
   GetDlgItem( IDOK )->EnableWindow( TRUE );

   if ( !m_bMultiple )
   {
    int nSel = m_wndStateList.GetCurSel();
    if ( nSel == LB_ERR)
      return;
    m_wndStateList.GetText( nSel, m_sSOState );
   }
   else
   {
    // ignore arrow keys
    int newSelCount = m_wndStateList.GetSelCount();
    if ( newSelCount != m_selCount )
    {
      m_selCount = newSelCount;

      int nSel = m_wndStateList.GetCaretIndex();
      if ( nSel == LB_ERR)
        return;
      CString item;
      m_wndStateList.GetText( nSel, item );
      if ( m_positive.find( item ) != m_positive.end() )
      {
        m_positive.erase( item );
        m_negative.insert( item );
      }
      else if ( m_negative.find( item ) != m_negative.end() )
      {
        m_negative.erase( item );
      }
      else
      {
        m_positive.insert( item );
      }
      m_wndStateList.Invalidate( FALSE );

      UpdateListCurrentStates();
    }
   }
   }
 */

BOOL CSmartObjectStateDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText("Smart Object States");

	CWnd* pAdvancedBtn = GetDlgItem(IDCONTINUE);
	if (pAdvancedBtn)
		pAdvancedBtn->ShowWindow(m_bHasAdvanced ? SW_SHOW : SW_HIDE);

	if (m_bMultiple)
		SetDlgItemText(IDC_LISTCAPTION, "&Choose Smart Object States:");
	else
		SetDlgItemText(IDC_LISTCAPTION, "&Choose Smart Object State:");
	OnRefreshBtn();
	SetDlgItemText(IDCANCEL, "Close");
	GetDlgItem(IDOK)->EnableWindow(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

