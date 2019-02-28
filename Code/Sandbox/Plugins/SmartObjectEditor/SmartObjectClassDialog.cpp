// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemDescriptionDlg.h"
#include "SmartObjectsEditorDialog.h"
#include "SmartObjectClassDialog.h"
#include "AI\AIManager.h"

#include "IResourceSelectorHost.h"
#include "Controls/QuestionDialog.h"

// CSmartObjectClassDialog dialog

IMPLEMENT_DYNAMIC(CSmartObjectClassDialog, CDialog)
CSmartObjectClassDialog::CSmartObjectClassDialog(CWnd* pParent /*=NULL*/, bool multi /*=false*/)
	: CDialog(multi ? CSmartObjectClassDialog::IDD_MULTIPLE : CSmartObjectClassDialog::IDD, pParent)
{
	m_bMultiple = multi;
	m_bEnableOK = false;
}

CSmartObjectClassDialog::~CSmartObjectClassDialog()
{
}

void CSmartObjectClassDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE, m_TreeCtrl);
}

BEGIN_MESSAGE_MAP(CSmartObjectClassDialog, CDialog)
ON_BN_CLICKED(IDC_NEW, OnNewBtn)
ON_BN_CLICKED(IDEDIT, OnEditBtn)
ON_BN_CLICKED(IDREFRESH, OnRefreshBtn)
//ON_LBN_DBLCLK(IDC_TREE, OnDblClk)
//ON_LBN_SELCHANGE(IDC_TREE, OnSelchangeClass)
ON_NOTIFY(TVN_KEYDOWN, IDC_TREE, OnTVKeyDown)
ON_NOTIFY(NM_CLICK, IDC_TREE, OnTVClick)
ON_NOTIFY(NM_DBLCLK, IDC_TREE, OnTVDblClk)
ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, OnTVSelChanged)
ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()

HTREEITEM CSmartObjectClassDialog::FindOrInsertItem(const CString& name, HTREEITEM parent)
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

HTREEITEM CSmartObjectClassDialog::ForcePath(const CString& location)
{
	HTREEITEM item = 0;
	CString token;
	int i = 0;
	while (!(token = location.Tokenize("/", i)).IsEmpty())
		item = FindOrInsertItem('/' + token, item);
	return item;
}

void CSmartObjectClassDialog::RemoveItemAndDummyParents(HTREEITEM item)
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

HTREEITEM CSmartObjectClassDialog::AddItemInTreeCtrl(const char* name, const CString& location)
{
	HTREEITEM parent = ForcePath(location);
	HTREEITEM item = m_TreeCtrl.InsertItem(name, parent, TVI_SORT);
	m_mapStringToItem[name] = item;
	return item;
}

HTREEITEM CSmartObjectClassDialog::FindItemInTreeCtrl(const CString& name)
{
	MapStringToItem::iterator it = m_mapStringToItem.find(name);
	if (it == m_mapStringToItem.end())
		return 0;
	else
		return it->second;
}

// CSmartObjectClassDialog message handlers

void CSmartObjectClassDialog::OnTVKeyDown(NMHDR* pNMHdr, LRESULT* pResult)
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
			m_TreeCtrl.SetCheck(item, !m_TreeCtrl.GetCheck(item));
		}
		else
		{
			UINT state = m_TreeCtrl.GetItemState(item, TVIS_BOLD);
			m_TreeCtrl.SetItemState(item, state ^ TVIS_BOLD, TVIS_BOLD);
			if (state & TVIS_BOLD)
			{
				// It was bold, i.e. it was selected, so deselect it now.
				m_setClasses.erase(m_TreeCtrl.GetItemText(item));
			}
			else
			{
				// It wasn't bold, i.e. it wasn't selected, so select it now.
				m_setClasses.insert(m_TreeCtrl.GetItemText(item));
			}
			UpdateListCurrentClasses();
		}
	}
}

void CSmartObjectClassDialog::OnTVClick(NMHDR* pNMHdr, LRESULT* pResult)
{
	if (!m_bMultiple)
		return;

	TVHITTESTINFO hti;
	GetCursorPos(&hti.pt);
	m_TreeCtrl.ScreenToClient(&hti.pt);
	m_TreeCtrl.HitTest(&hti);
	if (hti.hItem && hti.flags == TVHT_ONITEMSTATEICON)
	{
		if (m_TreeCtrl.ItemHasChildren(hti.hItem))
			m_TreeCtrl.SetCheck(hti.hItem, !m_TreeCtrl.GetCheck(hti.hItem));
		else
		{
			BOOL check = !m_TreeCtrl.GetCheck(hti.hItem);
			m_TreeCtrl.SetItemState(hti.hItem, check ? TVIS_BOLD : 0, TVIS_BOLD);
			if (check)
				m_setClasses.insert(m_TreeCtrl.GetItemText(hti.hItem));
			else
				m_setClasses.erase(m_TreeCtrl.GetItemText(hti.hItem));
			UpdateListCurrentClasses();
		}
	}
}

void CSmartObjectClassDialog::OnTVDblClk(NMHDR*, LRESULT*)
{
	if (HTREEITEM item = m_TreeCtrl.GetSelectedItem())
	{
		if (!m_TreeCtrl.ItemHasChildren(item))
		{
			if (!m_bMultiple)
				EndDialog(IDOK);
			else
			{
				BOOL check = !m_TreeCtrl.GetCheck(item);
				m_TreeCtrl.SetCheck(item, check);
				m_TreeCtrl.SetItemState(item, check ? TVIS_BOLD : 0, TVIS_BOLD);
				if (check)
					m_setClasses.insert(m_TreeCtrl.GetItemText(item));
				else
					m_setClasses.erase(m_TreeCtrl.GetItemText(item));
				UpdateListCurrentClasses();
			}
		}
	}
}

void CSmartObjectClassDialog::OnTVSelChanged(NMHDR*, LRESULT*)
{
	HTREEITEM item = m_TreeCtrl.GetSelectedItem();
	if (!item || m_TreeCtrl.ItemHasChildren(item))
	{
		SetDlgItemText(IDC_DESCRIPTION, "");
	}
	else
	{
		CSOLibrary::VectorClassData::iterator it = CSOLibrary::FindClass(m_TreeCtrl.GetItemText(item));
		SetDlgItemText(IDC_DESCRIPTION, it->description);
	}

	if (m_bMultiple || !item)
		return;
	if (!m_TreeCtrl.ItemHasChildren(item))
	{
		SetDlgItemText(IDCANCEL, "Cancel");
		GetDlgItem(IDOK)->EnableWindow(TRUE);

		if (HTREEITEM old = FindItemInTreeCtrl(m_sSOClass))
			m_TreeCtrl.SetItemState(old, 0, TVIS_BOLD);
		m_sSOClass = m_TreeCtrl.GetItemText(item);
		m_TreeCtrl.SetItemState(item, TVIS_BOLD, TVIS_BOLD);
		SetWindowText("Smart Object Class: " + m_sSOClass);
	}
}

BOOL CSmartObjectClassDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	if (m_bMultiple)
	{
		SetWindowText("Smart Object Classes");
		SetDlgItemText(IDC_LISTCAPTION, "&Choose Smart Object Classes:");
		//. m_wndClassList.ModifyStyle( 0, LBS_MULTIPLESEL );
	}
	else
	{
		SetWindowText("Smart Object Class");
		SetDlgItemText(IDC_LISTCAPTION, "&Choose Smart Object Class:");
		//. m_wndClassList.ModifyStyle( LBS_MULTIPLESEL, 0 );
	}

	//GetDlgItem( IDC_NEW )->EnableWindow( FALSE );
	//GetDlgItem( IDEDIT )->EnableWindow( FALSE );
	GetDlgItem(IDDELETE)->EnableWindow(FALSE);
	GetDlgItem(IDREFRESH)->EnableWindow(FALSE);

	//OnRefreshBtn();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CSmartObjectClassDialog::OnShowWindow(BOOL bShow, UINT nStatus)
{
	if (bShow)
		PostMessage(WM_COMMAND, (BN_CLICKED << 16) | IDREFRESH, (LPARAM) GetDlgItem(IDREFRESH)->m_hWnd);
}

void CSmartObjectClassDialog::OnRefreshBtn()
{
	m_TreeCtrl.UpdateWindow();
	m_setClasses.clear();

	if (m_bMultiple)
	{
		CString token;
		int i = 0;
		while (!(token = m_sSOClass.Tokenize(" !\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", i)).IsEmpty())
		{
			if (CItemDescriptionDlg::ValidateItem(token))
			{
				if (CSOLibrary::FindClass(token) == CSOLibrary::GetClasses().end())
					CSOLibrary::AddClass(token, "", "", "");
				m_setClasses.insert(token);
			}
		}
	}

	m_mapStringToItem.clear();
	m_TreeCtrl.DeleteAllItems();
	CSOLibrary::VectorClassData::iterator it, itEnd = CSOLibrary::GetClasses().end();
	for (it = CSOLibrary::GetClasses().begin(); it != itEnd; ++it)
		AddItemInTreeCtrl(it->name, it->location);

	if (m_bMultiple)
	{
		m_sSOClass.Empty();
		TSOClassesSet::iterator it, itEnd = m_setClasses.end();
		for (it = m_setClasses.begin(); it != itEnd; ++it)
		{
			HTREEITEM item = FindItemInTreeCtrl(*it);
			m_TreeCtrl.EnsureVisible(item);
			m_TreeCtrl.SelectItem(item);
			m_TreeCtrl.SetCheck(item, TRUE);
			assert(m_TreeCtrl.GetCheck(item));
			m_TreeCtrl.SetItemState(item, TVIS_BOLD, TVIS_BOLD);
			if (!m_sSOClass.IsEmpty())
				m_sSOClass += ", ";
			m_sSOClass += *it;
		}
		SetDlgItemText(IDC_SELECTIONLIST, m_sSOClass);
	}
	else
	{
		HTREEITEM item = FindItemInTreeCtrl(m_sSOClass);
		m_TreeCtrl.SetItemState(item, TVIS_BOLD, TVIS_BOLD);
		m_TreeCtrl.EnsureVisible(item);
		m_TreeCtrl.SelectItem(item);
	}

	if (!m_bEnableOK)
	{
		SetDlgItemText(IDCANCEL, "Close");
		GetDlgItem(IDOK)->EnableWindow(FALSE);
	}
}

void CSmartObjectClassDialog::UpdateListCurrentClasses()
{
	m_sSOClass.Empty();
	TSOClassesSet::iterator it, itEnd = m_setClasses.end();
	for (it = m_setClasses.begin(); it != itEnd; ++it)
	{
		if (!m_sSOClass.IsEmpty())
			m_sSOClass += ", ";
		m_sSOClass += *it;
	}
	SetDlgItemText(IDC_SELECTIONLIST, m_sSOClass);
	SetDlgItemText(IDCANCEL, "Cancel");
	GetDlgItem(IDOK)->EnableWindow(TRUE);
}

void CSmartObjectClassDialog::OnEditBtn()
{
	HTREEITEM item = m_TreeCtrl.GetSelectedItem();
	if (!item || m_TreeCtrl.ItemHasChildren(item))
		return;

	CString name = m_TreeCtrl.GetItemText(item);
	CSOLibrary::VectorClassData::iterator it = CSOLibrary::FindClass(name);
	assert(it != CSOLibrary::GetClasses().end());

	CItemDescriptionDlg dlg(this, false, false, true, true);
	dlg.m_sCaption = "Edit Smart Object Class";
	dlg.m_sItemCaption = "Class &name:";
	dlg.m_sItemEdit = name;
	dlg.m_sDescription = it->description;
	dlg.m_sLocation = it->location;
	dlg.m_sTemplate = it->templateName;
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
			it->templateName = dlg.m_sTemplate;
			it->pClassTemplateData = dlg.m_sTemplate.IsEmpty() ? NULL : CSOLibrary::FindClassTemplate(dlg.m_sTemplate);
			SetDlgItemText(IDC_DESCRIPTION, dlg.m_sDescription);
			CSOLibrary::InvalidateSOEntities();
		}
	}
}

void CSmartObjectClassDialog::OnNewBtn()
{
	CItemDescriptionDlg dlg(this, true, false, true, true);
	dlg.m_sCaption = "New Class";
	dlg.m_sItemCaption = "Class &name:";
	dlg.m_sDescription = "";
	dlg.m_sLocation = "";
	dlg.m_sTemplate = "";
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
				//. m_sSOClass = dlg.m_sItemEdit;
				m_TreeCtrl.EnsureVisible(item);
				m_TreeCtrl.SelectItem(item);
			}

			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Entered class name already exists...\n\nA new class with the same name can not be created!"));
		}
		else
		{
			CSOLibrary::AddClass(dlg.m_sItemEdit, dlg.m_sDescription, dlg.m_sLocation, dlg.m_sTemplate);
			item = AddItemInTreeCtrl(dlg.m_sItemEdit, dlg.m_sLocation);

			if (m_bMultiple)
			{
				m_TreeCtrl.EnsureVisible(item);
				m_TreeCtrl.SelectItem(item);
				//OnSelChanged(0,0);
			}
			else
			{
				//. m_sSOClass = dlg.m_sItemEdit;
				m_TreeCtrl.EnsureVisible(item);
				m_TreeCtrl.SelectItem(item);
			}
		}

		SetDlgItemText(IDCANCEL, "Cancel");
		GetDlgItem(IDOK)->EnableWindow(TRUE);
	}
}
namespace
{
dll_string ShowDialog(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	CSmartObjectClassDialog soDlg(nullptr, true);
	soDlg.SetSOClass(szPreviousValue);
	if (soDlg.DoModal() == IDOK)
	{
		CString result = soDlg.GetSOClass();
		return (LPCSTR)result;
	}
	return szPreviousValue;
}

REGISTER_RESOURCE_SELECTOR("SmartObjectClasses", ShowDialog, "")
}

