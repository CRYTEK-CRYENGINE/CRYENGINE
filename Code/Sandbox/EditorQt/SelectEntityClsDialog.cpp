// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SelectEntityClsDialog.h"
#include "Util/MFCUtil.h"
#include "Objects\EntityScript.h"

// CSelectEntityClsDialog dialog

IMPLEMENT_DYNAMIC(CSelectEntityClsDialog, CXTResizeDialog)
CSelectEntityClsDialog::CSelectEntityClsDialog(CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(CSelectEntityClsDialog::IDD, pParent)
{
}

CSelectEntityClsDialog::~CSelectEntityClsDialog()
{
}

void CSelectEntityClsDialog::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE, m_tree);
}

BEGIN_MESSAGE_MAP(CSelectEntityClsDialog, CXTResizeDialog)
ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, OnTvnSelchangedTree)
ON_NOTIFY(NM_DBLCLK, IDC_TREE, OnTvnDoubleClick)
END_MESSAGE_MAP()

// CSelectEntityClsDialog message handlers

//////////////////////////////////////////////////////////////////////////
BOOL CSelectEntityClsDialog::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	// Create the list
	//m_imageList.Create(IDB_TREE_VIEW, 16, 1, RGB (255, 0, 255));
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_ENTITY_TREE, 16, RGB(255, 0, 255));

	// Attach it to the control
	m_tree.SetImageList(&m_imageList, TVSIL_NORMAL);
	//m_tree.SetImageList(&m_imageList, TVSIL_STATE);

	m_tree.SetIndent(0);

	SetResize(IDC_TREE, SZ_RESIZE(1));
	SetResize(IDOK, SZ_REPOS(1));
	SetResize(IDCANCEL, SZ_REPOS(1));

	ReloadEntities();

	AutoLoadPlacement("Dialogs\\SelEntCls");

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CSelectEntityClsDialog::ReloadEntities()
{
	std::map<string, HTREEITEM> items;

	CEntityScriptRegistry::Instance()->IterateOverScripts([this, &items](CEntityScript& script)
	{
		// If class is not usable simply skip it.
		if (!script.IsUsable())
			return;

		string name = script.GetName();

		HTREEITEM hRoot = TVI_ROOT;
		char* token;

		string clsFile = script.GetFile();
		clsFile.Replace("Scripts\\Default\\Entities\\", "");
		char classFile[1024];
		cry_strcpy(classFile, clsFile);

		token = strtok(classFile, "\\/");

		string itemName;
		while (token)
		{
			string strToken = token;

			token = strtok(NULL, "\\/");
			if (!token)
				break;

			itemName += strToken + "\\";

			auto itemIt = items.find(itemName);
			if (itemIt != items.end())
			{
				hRoot = itemIt->second;
			}
			else
			{
				hRoot = m_tree.InsertItem(strToken, 0, 1, hRoot);
				items.emplace(std::make_pair(itemName, hRoot));
			}
		}

		HTREEITEM hNewItem = m_tree.InsertItem(name, 2, 3, hRoot);
		m_tree.SetItemState(hNewItem, TVIS_BOLD, TVIS_BOLD);
		m_itemsMap[hNewItem] = name;
	});

	m_tree.SortChildren(TVI_ROOT);
}

//////////////////////////////////////////////////////////////////////////
void CSelectEntityClsDialog::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	//////////////////////////////////////////////////////////////////////////
	// Return entity class.
	//////////////////////////////////////////////////////////////////////////
	m_entityClass = stl::find_in_map(m_itemsMap, pNMTreeView->itemNew.hItem, string(""));

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CSelectEntityClsDialog::OnTvnDoubleClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_entityClass.IsEmpty())
	{
		EndDialog(IDOK);
	}
}

