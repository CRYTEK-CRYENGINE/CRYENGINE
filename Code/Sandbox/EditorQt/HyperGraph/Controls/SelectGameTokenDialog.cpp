// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SelectGameTokenDialog.h"

#include <IDataBaseItem.h>
#include <IDataBaseManager.h>
#include <IDataBaseLibrary.h>
#include <GameTokens/GameTokenItem.h>
#include <GameTokens/GameTokenManager.h>
#include "HyperGraph/FlowGraph.h"
#include "HyperGraphEditorWnd.h"
#include "Util/MFCUtil.h"

IMPLEMENT_DYNAMIC(CSelectGameTokenDialog, CXTResizeDialog)
CSelectGameTokenDialog::CSelectGameTokenDialog(CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(CSelectGameTokenDialog::IDD, pParent)
{}

CSelectGameTokenDialog::~CSelectGameTokenDialog() {}


void CSelectGameTokenDialog::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE, m_tree);
}

BEGIN_MESSAGE_MAP(CSelectGameTokenDialog, CXTResizeDialog)
ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, OnTvnSelchangedTree)
ON_NOTIFY(NM_DBLCLK, IDC_TREE, OnTvnDoubleClick)
END_MESSAGE_MAP()


BOOL CSelectGameTokenDialog::OnInitDialog()
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

	ReloadGameTokens();

	AutoLoadPlacement("Dialogs\\SelGameToken");

	return TRUE;
}

void CSelectGameTokenDialog::ReloadGameTokens()
{
	std::map<CString, HTREEITEM> treeItems;

	CGameTokenManager* pMgr = GetIEditorImpl()->GetGameTokenManager();
	IDataBaseItemEnumerator* pEnum = pMgr->GetItemEnumerator();
	IDataBaseItem* pItem = pEnum->GetFirst();

	std::map<CString, CGameTokenItem*> items;

	while (pItem)
	{
		CGameTokenItem* pToken = (CGameTokenItem*) pItem;
		items[pToken->GetFullName().GetString()] = pToken;
		pItem = pEnum->GetNext();
	}
	pEnum->Release();

	// items now sorted, make the tree
	std::map<CString, CGameTokenItem*>::iterator iter = items.begin();
	HTREEITEM hSelected = 0;

	CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		CHyperGraphDialog* pHyperGraphDialog = static_cast<CHyperGraphDialog*>(pWnd);

		if (CHyperGraphView* pGraphView = pHyperGraphDialog->GetGraphView())
		{
			if (CHyperGraph* pGraph = pGraphView->GetGraph())
			{
				if (pGraph->IsFlowGraph())
				{
					IFlowGraph* pIFlowGraph = ((CHyperFlowGraph*)pGraph)->GetIFlowGraph();

					if (size_t tokenCount = pIFlowGraph->GetGraphTokenCount())
					{
						HTREEITEM hRoot = TVI_ROOT;
						HTREEITEM hRootGraphTokens = m_tree.InsertItem("Graph Tokens", 0, 1, hRoot);

						for (size_t i = 0; i < tokenCount; ++i)
						{
							string graphTokenName = pIFlowGraph->GetGraphToken(i)->name;
							HTREEITEM hNewItem = m_tree.InsertItem(graphTokenName, 2, 3, hRootGraphTokens);
							m_tree.SetItemState(hNewItem, TVIS_BOLD, TVIS_BOLD);
							m_itemsMap[hNewItem] = graphTokenName;

							if (!m_preselect.IsEmpty() && m_preselect.CompareNoCase(graphTokenName) == 0)
								hSelected = hNewItem;
						}
					}
				}
			}
		}
	}

	while (iter != items.end())
	{
		HTREEITEM hRootTokens = TVI_ROOT;

		string libName;
		IDataBaseLibrary* pLib = (*iter).second->GetLibrary();
		if (pLib != 0)
			libName = pLib->GetName();
		const string& groupName = (*iter).second->GetGroupName();

		// for now circumvent a bug in the database GetShortName when the item name
		// contains a ".". then short name returns only the last piece of it
		// which is wrong (say group: GROUP1 and name "NAME.SUBNAME" then
		// short name returns only SUBNAME.
		string shortName = (*iter).second->GetName(); // incl. group
		int i = shortName.Find(groupName + ".");
		if (i >= 0)
			shortName = shortName.Mid(groupName.GetLength() + 1);

		if (!libName.IsEmpty())
		{
			auto treeItemIt = treeItems.find(libName.GetString());

			if (treeItemIt != treeItems.end())
			{
				hRootTokens = treeItemIt->second;
			}
			else
			{
				hRootTokens = m_tree.InsertItem(libName, 0, 1, hRootTokens);
				treeItems.emplace(std::make_pair(libName.GetString(), hRootTokens));
			}
		}
		if (!groupName.IsEmpty())
		{
			string combinedName = libName;
			if (!libName.IsEmpty())
				combinedName += ".";
			combinedName += groupName;

			auto treeItemIt = treeItems.find(combinedName.GetString());

			if (treeItemIt != treeItems.end())
			{
				hRootTokens = treeItemIt->second;
			}
			else
			{
				hRootTokens = m_tree.InsertItem(groupName.GetString(), 0, 1, hRootTokens);
				treeItems.emplace(std::make_pair(combinedName.GetString(), hRootTokens));
			}
		}

		HTREEITEM hNewItem = m_tree.InsertItem(shortName, 2, 3, hRootTokens);
		m_tree.SetItemState(hNewItem, TVIS_BOLD, TVIS_BOLD);
		m_itemsMap[hNewItem] = (*iter).second->GetFullName();
		if (!m_preselect.IsEmpty() && m_preselect.CompareNoCase((*iter).first) == 0)
			hSelected = hNewItem;

		++iter;
	}
	m_selectedItem = "";

	if (hSelected != 0)
	{
		// all parent nodes have to be expanded first
		HTREEITEM hParent = hSelected;
		while (hParent = m_tree.GetParentItem(hParent))
			m_tree.Expand(hParent, TVE_EXPAND);

		m_tree.Select(hSelected, TVGN_CARET);
		m_tree.EnsureVisible(hSelected);
	}
}


void CSelectGameTokenDialog::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	std::map<HTREEITEM, CString>::const_iterator found;
	found = m_itemsMap.find(pNMTreeView->itemNew.hItem);
	if (found != m_itemsMap.end())
		m_selectedItem = (*found).second;
	else
		m_selectedItem = "";
	*pResult = 0;
}

void CSelectGameTokenDialog::OnTvnDoubleClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_selectedItem != "")
	{
		EndDialog(IDOK);
	}
}

