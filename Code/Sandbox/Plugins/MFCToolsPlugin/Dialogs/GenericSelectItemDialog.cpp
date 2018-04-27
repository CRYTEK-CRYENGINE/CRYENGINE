// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GenericSelectItemDialog.h"
#include "Dialogs/QStringDialog.h"
#include "Util/MFCUtil.h"

// CGenericSelectItemDialog dialog

IMPLEMENT_DYNAMIC(CGenericSelectItemDialog, CXTResizeDialog)
CGenericSelectItemDialog::CGenericSelectItemDialog(CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(IDD_GENERAL_SELECT_ITEM, pParent)
{
	m_mode = eMODE_LIST;
	m_bSet = false;
	m_bAllowNew = false;
	m_bShowDesc = true;
	m_title = _T("Please choose...");
	m_dialogID = "Dialogs\\GenericSelect";
}

CGenericSelectItemDialog::~CGenericSelectItemDialog()
{
}

void CGenericSelectItemDialog::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST, m_listBox);
	DDX_Control(pDX, IDC_TREE, m_tree);
	DDX_Control(pDX, IDC_DESCRIPTION, m_desc);
}

BEGIN_MESSAGE_MAP(CGenericSelectItemDialog, CXTResizeDialog)
ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, OnTvnSelchangedTree)
ON_NOTIFY(NM_DBLCLK, IDC_TREE, OnTvnDoubleClick)
ON_LBN_SELCHANGE(IDC_LIST, OnLbnSelchangeList)
ON_LBN_DBLCLK(IDC_LIST, OnLbnDoubleClick)
ON_BN_CLICKED(IDNEW, OnBnClickedNew)
END_MESSAGE_MAP()

// CGenericSelectItemDialog message handlers

//////////////////////////////////////////////////////////////////////////
BOOL CGenericSelectItemDialog::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_TREE_VIEW, 16, RGB(255, 0, 255));

	// Attach it to the control
	m_tree.SetImageList(&m_imageList, TVSIL_NORMAL);
	//m_tree.SetImageList(&m_imageList, TVSIL_STATE);

	m_tree.SetIndent(0);

	SetResize(IDC_TREE, CXTResizeRect(0, 0, 1, .8f));
	SetResize(IDC_LIST, CXTResizeRect(0, 0, 1, .8f));
	SetResize(IDC_DESCRIPTION, CXTResizeRect(0, .8f, 1, 1));
	SetResize(IDOK, SZ_REPOS(1));
	SetResize(IDCANCEL, SZ_REPOS(1));
	SetResize(IDNEW, SZ_REPOS(1));

	if (m_mode == eMODE_LIST)
	{
		m_tree.ShowWindow(SW_HIDE);
	}
	else
	{
		m_listBox.ShowWindow(SW_HIDE);
	}

	CWnd* pWnd = GetDlgItem(IDNEW);
	if (pWnd)
	{
		pWnd->ShowWindow(m_bAllowNew ? SW_SHOW : SW_HIDE);
	}

	m_desc.ShowWindow(m_bShowDesc ? SW_SHOW : SW_HIDE);
	if (m_bShowDesc == false)
	{
		RemoveResize(IDC_DESCRIPTION);
	}

	if (m_bSet == false)
	{
		GetItems(m_items);
	}
	ReloadItems();
	AutoLoadPlacement(m_dialogID);

	SetWindowText(m_title);

	if (m_mode == eMODE_TREE)
	{
		m_tree.SetFocus();
	}
	else
	{
		m_listBox.SetFocus();
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::GetItems(std::vector<SItem>& outItems)
{
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::ReloadTree()
{
	m_tree.SetRedraw(FALSE);
	m_tree.DeleteAllItems();

	HTREEITEM hSelected = 0;

	/*
	   std::vector<CString>::const_iterator iter = m_items.begin();
	   while (iter != m_items.end())
	   {
	   const CString& itemName = *iter;
	   HTREEITEM hItem = m_tree.InsertItem(itemName, 0, 0, TVI_ROOT, TVI_SORT);
	   if (!m_preselect.IsEmpty() && m_preselect.CompareNoCase(itemName) == 0)
	   {
	    hSelected = hItem;
	   }
	   ++iter;
	   }
	 */

	CString itemName;
	CString itemLastName;
	std::map<CString, HTREEITEM, stl::less_stricmp<CString>> items;

	CString sep("\\/.");
	sep += m_treeSeparator;
	const char* szSep = sep.GetString();

	std::vector<SItem>::const_iterator iter = m_items.begin();
	char itempath[1024];
	while (iter != m_items.end())
	{
		const SItem* pItem = &*iter;
		const CString& name = iter->name;
		itemLastName = name;

		HTREEITEM hRoot = TVI_ROOT;
		char* token;
		cry_strcpy(itempath, name);

		token = strtok(itempath, szSep);

		itemName = "";
		while (token)
		{
			CString strToken = token;

			token = strtok(NULL, szSep);
			if (!token)
			{
				itemLastName = strToken;
				break;
			}

			itemName += strToken;
			itemName += "/";

			HTREEITEM hParentItem = stl::find_in_map(items, itemName, 0);
			if (!hParentItem)
			{
				hRoot = m_tree.InsertItem(strToken, 0, 0, hRoot);
				items[itemName] = hRoot;
			}
			else
				hRoot = hParentItem;
		}

		HTREEITEM hNewItem = m_tree.InsertItem(itemLastName, 1, 1, hRoot, TVI_SORT);
		m_tree.SetItemData(hNewItem, DWORD_PTR(pItem)); // full text as item data

		if (!m_preselect.IsEmpty() && m_preselect.CompareNoCase(name) == 0)
		{
			hSelected = hNewItem;
		}

		++iter;
	}
	m_tree.SortChildren(TVI_ROOT);
	m_tree.Expand(TVI_ROOT, TVE_EXPAND);

	if (hSelected != 0)
	{
		// all parent nodes have to be expanded first
		HTREEITEM hParent = hSelected;
		while (hParent = m_tree.GetParentItem(hParent))
			m_tree.Expand(hParent, TVE_EXPAND);

		m_tree.Select(hSelected, TVGN_CARET);
		m_tree.EnsureVisible(hSelected);
	}

	m_tree.SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::ReloadItems()
{
	m_selectedItem.Empty();
	m_selectedDesc.Empty();
	if (m_mode == eMODE_TREE)
	{
		ReloadTree();
	}
	else // eMODE_LIST
	{
		std::vector<SItem>::const_iterator iter = m_items.begin();
		while (iter != m_items.end())
		{
			const SItem* pItem = &*iter;
			int nIndex = m_listBox.AddString(pItem->name);
			m_listBox.SetItemData(nIndex, DWORD_PTR(pItem));
			++iter;
		}
		if (m_preselect.IsEmpty() == false)
		{
			int index = m_listBox.SelectString(-1, m_preselect);
			OnLbnSelchangeList();
		}
	}
	ItemSelected();
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	HTREEITEM hItem = m_tree.GetSelectedItem();
	if (hItem != 0)
	{
		SItem* pItem = reinterpret_cast<SItem*>(m_tree.GetItemData(hItem));
		if (pItem)
		{
			m_selectedItem = pItem->name;
			m_selectedDesc = pItem->desc;
			m_selectedDesc.Replace("\\n", "\r\n");
		}
		else
		{
			m_selectedItem.Empty();
			m_selectedDesc.Empty();
		}

		ItemSelected();
	}
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::OnTvnDoubleClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_selectedItem.IsEmpty() == false)
	{
		EndDialog(IDOK);
	}
}

//////////////////////////////////////////////////////////////////////////
CString CGenericSelectItemDialog::GetSelectedItem()
{
	return m_selectedItem;
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::OnLbnDoubleClick()
{
	if (m_selectedItem.IsEmpty() == false)
	{
		EndDialog(IDOK);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::OnLbnSelchangeList()
{
	int index = m_listBox.GetCurSel();
	if (index == LB_ERR)
		m_selectedItem = "";
	else
	{
		m_listBox.GetText(index, m_selectedItem);
		SItem* pItem = reinterpret_cast<SItem*>(m_listBox.GetItemData(index));
		if (pItem != 0)
		{
			m_selectedDesc = pItem->desc;
			m_selectedDesc.Replace("\\n", "\r\n");
		}
		else
			m_selectedDesc.Empty();
	}
	ItemSelected();
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::OnBnClickedNew()
{
	EndDialog(IDNEW);
}

//////////////////////////////////////////////////////////////////////////
void CGenericSelectItemDialog::ItemSelected()
{
	if (m_selectedItem.IsEmpty())
		m_desc.SetWindowText(_T("<Nothing selected>"));
	else
	{
		if (m_selectedDesc.IsEmpty())
			m_desc.SetWindowText(m_selectedItem);
		else
			m_desc.SetWindowText(m_selectedDesc);
	}
}

