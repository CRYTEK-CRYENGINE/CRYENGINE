// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannAnimDBEditorDialog.h"
#include "MannNewSubADBFilterDialog.h"
#include "MannequinDialog.h"
#include "Controls/TreeCtrlEx.h"
#include "Util/MFCUtil.h"
#include <CryString/StringUtils.h>

#define KTreeItemStrLength      1024
#define KImageForFilter         0
#define KImageSelectedForFilter 0

enum
{
	IDC_NEW_SUBADB = AFX_IDW_PANE_FIRST,
	IDC_SUBADB_TREECTRL
};

namespace
{
// name - the name of the item that is searched for
// tree - a reference to the tree control
// hRoot - the handle to the item where the search begins
HTREEITEM FindItem(const CString& name, CTreeCtrl& tree, HTREEITEM hRoot)
{
	// check whether the current item is the searched one
	CString text = tree.GetItemText(hRoot);
	if (text.Compare(name) == 0)
		return hRoot;

	// get a handle to the first child item
	HTREEITEM hSub = tree.GetChildItem(hRoot);
	// iterate as long a new item is found
	while (hSub)
	{
		// check the children of the current item
		HTREEITEM hFound = FindItem(name, tree, hSub);
		if (hFound)
			return hFound;

		// get the next sibling of the current item
		hSub = tree.GetNextSiblingItem(hSub);
	}

	// return NULL if nothing was found
	return NULL;
}

HTREEITEM FindItem(const CString& name, CTreeCtrl& tree)
{
	HTREEITEM root = tree.GetRootItem();
	while (root != NULL)
	{
		HTREEITEM hFound = FindItem(name, tree, root);
		if (hFound)
			return hFound;

		root = tree.GetNextSiblingItem(root);
	}

	return NULL;
}
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMannAnimDBEditorDialog, CDialog)
ON_BN_CLICKED(IDC_MANN_NEW_CONTEXT, &CMannAnimDBEditorDialog::OnNewContext)
ON_BN_CLICKED(IDC_MANN_EDIT_CONTEXT, &CMannAnimDBEditorDialog::OnEditContext)
ON_BN_CLICKED(IDC_MANN_CLONE_CONTEXT, &CMannAnimDBEditorDialog::OnCloneAndEditContext)
ON_BN_CLICKED(IDC_MANN_DELETE_CONTEXT, &CMannAnimDBEditorDialog::OnDeleteContext)

ON_BN_CLICKED(IDC_MANN_ADB_MOVEUP, &CMannAnimDBEditorDialog::OnMoveUp)
ON_BN_CLICKED(IDC_MANN_ADB_MOVEDOWN, &CMannAnimDBEditorDialog::OnMoveDown)

ON_NOTIFY(TVN_SELCHANGED, IDC_SUBADB_TREECTRL, &CMannAnimDBEditorDialog::OnSubADBTreeSelChanged)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannAnimDBEditorDialog::CMannAnimDBEditorDialog(IAnimationDatabase* pAnimDB, CWnd* pParent)
	: CXTResizeDialog(IDD_MANN_ANIMDB_EDITOR, pParent)
	, m_animDB(pAnimDB)
{
	CRY_ASSERT(m_animDB);

	// flush changes so nothing is lost.
	CMannequinDialog::GetCurrentInstance()->FragmentEditor()->TrackPanel()->FlushChanges();
}

//////////////////////////////////////////////////////////////////////////
CMannAnimDBEditorDialog::~CMannAnimDBEditorDialog()
{
	CMannequinDialog::GetCurrentInstance()->LoadNewPreviewFile(NULL);
	CMannequinDialog::GetCurrentInstance()->StopEditingFragment();
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_MANN_NEW_CONTEXT, m_newContext);
	DDX_Control(pDX, IDC_MANN_EDIT_CONTEXT, m_editContext);
	DDX_Control(pDX, IDC_MANN_CLONE_CONTEXT, m_cloneContext);
	DDX_Control(pDX, IDC_MANN_DELETE_CONTEXT, m_deleteContext);
	DDX_Control(pDX, IDC_MANN_ADB_MOVEUP, m_moveUp);
	DDX_Control(pDX, IDC_MANN_ADB_MOVEDOWN, m_moveDown);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannAnimDBEditorDialog::OnInitDialog()
{
	__super::OnInitDialog();

	const CTagDefinition& fragDefs = m_animDB->GetFragmentDefs();
	const CTagDefinition& tagDefs = m_animDB->GetTagDefs();

	std::vector<string> TagNames;
	RetrieveTagNames(TagNames, fragDefs);
	RetrieveTagNames(TagNames, tagDefs);

	CRect rc;
	GetWindowRect(rc);
	ScreenToClient(rc);
	rc.DeflateRect(13, 64, 13, 40);
	m_adbTree.reset(new CTreeCtrlEx);
	m_adbTree->Create(
	  WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP |
	  TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
	  rc, this, IDC_SUBADB_TREECTRL);
	m_adbTree->SetNoDrag(true);
	if (CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_MANN_TAGDEF_TREE, 16, RGB(255, 0, 255)))
	{
		m_adbTree->SetImageList(&m_imageList, TVSIL_NORMAL);
	}

	AutoLoadPlacement("Dialogs\\MannequinAnimDBEditor");

	PopulateSubADBTree();

	EnableControls();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

void CMannAnimDBEditorDialog::RetrieveTagNames(std::vector<string>& tagNames, const CTagDefinition& tagDef)
{
	tagNames.clear();
	const TagID maxTag = tagDef.GetNum();
	tagNames.reserve(maxTag);
	for (TagID it = 0; it < maxTag; ++it)
	{
		const string TagName = tagDef.GetTagName(it);
		tagNames.push_back(TagName);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::EnableControls()
{
	CRY_ASSERT(m_adbTree.get() != nullptr);

	const HTREEITEM pRow = m_adbTree->GetSelectedItem();
	const bool isValidRowSelected = pRow != nullptr;

	m_newContext.EnableWindow(true);
	m_editContext.EnableWindow(isValidRowSelected);
	m_cloneContext.EnableWindow(isValidRowSelected);
	m_deleteContext.EnableWindow(isValidRowSelected);
	m_moveUp.EnableWindow(isValidRowSelected);
	m_moveDown.EnableWindow(isValidRowSelected);
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::OnSubADBTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	EnableControls();
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::PopulateTreeSubADBFilters(const SMiniSubADB& miniSubADB, HTREEITEM parent)
{
	//char szBuffer[KTreeItemStrLength];
	HTREEITEM item = m_adbTree->InsertItem(PathUtil::GetFile(miniSubADB.filename), KImageForFilter, KImageSelectedForFilter, parent, 0);

	const SMiniSubADB::TSubADBArray& vSubADBs = miniSubADB.vSubADBs;
	for (SMiniSubADB::TSubADBArray::const_iterator itSubADB = vSubADBs.begin(); itSubADB != vSubADBs.end(); ++itSubADB)
	{
		PopulateTreeSubADBFilters((*itSubADB), item);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::PopulateSubADBTree()
{
	m_adbTree->SetRedraw(FALSE);
	m_adbTree->DeleteAllItems();

	RefreshMiniSubADBs();

	// Populate the CTreeCtrl-m_adbTree with the above tree like data
	// however, need to keep an item handle to each entry so that in the second pass
	// we can populate it with the correct non-filtered/non-SubADB'd tags.
	// NB: This populating process is partially recursive, the first level iteration is
	//	done here but subsequent levels are done via calls to PopulateTreeSubADBFilters
	HTREEITEM parent = TVI_ROOT;
	HTREEITEM item = NULL;
	for (SMiniSubADB::TSubADBArray::const_iterator itSubADB = m_vSubADBs.begin(); itSubADB != m_vSubADBs.end(); ++itSubADB)
	{
		const SMiniSubADB& subADB = (*itSubADB);
		item = m_adbTree->InsertItem(PathUtil::GetFile(subADB.filename), KImageForFilter, KImageSelectedForFilter, parent, 0);
		for (SMiniSubADB::TSubADBArray::const_iterator itSubSubADB = subADB.vSubADBs.begin(); itSubSubADB != subADB.vSubADBs.end(); ++itSubSubADB)
		{
			PopulateTreeSubADBFilters((*itSubSubADB), item);
		}
	}

	m_adbTree->SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::RefreshMiniSubADBs()
{
	// Get the SubADB tree structure from the CAnimationDatabase in CryAction(.dll)
	m_animDB->GetSubADBFragmentFilters(m_vSubADBs);
}

//////////////////////////////////////////////////////////////////////////
SMiniSubADB* CMannAnimDBEditorDialog::GetSelectedSubADBInternal(const string& text, SMiniSubADB* pSub)
{
	const string tempFilename = PathUtil::GetFile(pSub->filename.c_str());
	if (tempFilename.compareNoCase(text) == 0)
	{
		return pSub;
	}

	for (SMiniSubADB::TSubADBArray::iterator itSubADB = pSub->vSubADBs.begin(); itSubADB != pSub->vSubADBs.end(); ++itSubADB)
	{
		const SMiniSubADB& subADB = (*itSubADB);
		const string tempFilename = PathUtil::GetFile(subADB.filename.c_str());
		if (tempFilename.compareNoCase(text) == 0)
		{
			return &(*itSubADB);
		}
		else
		{
			for (SMiniSubADB::TSubADBArray::iterator itSubs = (*itSubADB).vSubADBs.begin(); itSubs != (*itSubADB).vSubADBs.end(); ++itSubs)
			{
				SMiniSubADB* pSelectedSubADB = GetSelectedSubADBInternal(text, itSubs);
				if (pSelectedSubADB != nullptr)
					return pSelectedSubADB;
			}
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
SMiniSubADB* CMannAnimDBEditorDialog::GetSelectedSubADB()
{
	const HTREEITEM item = m_adbTree->GetSelectedItem();
	const string itemText = m_adbTree->GetItemText(item);

	for (SMiniSubADB::TSubADBArray::iterator itSubADB = m_vSubADBs.begin(); itSubADB != m_vSubADBs.end(); ++itSubADB)
	{
		const SMiniSubADB& subADB = (*itSubADB);
		const string tempFilename = PathUtil::GetFile(subADB.filename.c_str());
		if (tempFilename.compareNoCase(itemText) == 0)
		{
			return &(*itSubADB);
		}
		else
		{
			for (SMiniSubADB::TSubADBArray::iterator itSubs = (*itSubADB).vSubADBs.begin(); itSubs != (*itSubADB).vSubADBs.end(); ++itSubs)
			{
				SMiniSubADB* pSelectedSubADB = GetSelectedSubADBInternal(itemText, itSubs);
				if (pSelectedSubADB != nullptr)
					return pSelectedSubADB;
			}
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::OnNewContext()
{
	const HTREEITEM selItem = m_adbTree->GetSelectedItem();
	const string parText = (selItem == NULL) ? m_animDB->GetFilename() : m_adbTree->GetItemText(selItem);

	CMannNewSubADBFilterDialog dialog(nullptr, m_animDB, parText, CMannNewSubADBFilterDialog::eContextDialog_New, this);
	if (dialog.DoModal() == IDOK)
	{
		PopulateSubADBTree();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::OnEditContext()
{
	const HTREEITEM selItem = m_adbTree->GetSelectedItem();
	const HTREEITEM parItem = m_adbTree->GetParentItem(selItem);
	const string parText = parItem ? m_adbTree->GetItemText(parItem) : m_animDB->GetFilename();

	CMannNewSubADBFilterDialog dialog(GetSelectedSubADB(), m_animDB, parText, CMannNewSubADBFilterDialog::eContextDialog_Edit, this);
	if (dialog.DoModal() == IDOK)
	{
		PopulateSubADBTree();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::OnCloneAndEditContext()
{
	const HTREEITEM selItem = m_adbTree->GetSelectedItem();
	const HTREEITEM parItem = m_adbTree->GetParentItem(selItem);
	const string parText = parItem ? m_adbTree->GetItemText(parItem) : m_animDB->GetFilename();

	CMannNewSubADBFilterDialog dialog(GetSelectedSubADB(), m_animDB, parText, CMannNewSubADBFilterDialog::eContextDialog_CloneAndEdit, this);
	if (dialog.DoModal() == IDOK)
	{
		PopulateSubADBTree();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::OnDeleteContext()
{
	SMiniSubADB* pSelectedSubADB = GetSelectedSubADB();
	if (pSelectedSubADB)
	{
		m_animDB->DeleteSubADBFilter(string(PathUtil::GetFile(pSelectedSubADB->filename.c_str())));
		PopulateSubADBTree();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::OnMoveUp()
{
	SMiniSubADB* pSelectedSubADB = GetSelectedSubADB();
	if (pSelectedSubADB)
	{
		const string selectedFilename = pSelectedSubADB->filename;
		const bool bMoved = m_animDB->MoveSubADBFilter(selectedFilename, true);
		if (bMoved)
		{
			PopulateSubADBTree();
		}
		// find the item at it's new location and select it, so user can keep moving it if they want too.
		HTREEITEM item = FindItem(PathUtil::GetFile(selectedFilename.c_str()), *m_adbTree.get());
		if (item)
			m_adbTree->SelectItem(item);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannAnimDBEditorDialog::OnMoveDown()
{
	SMiniSubADB* pSelectedSubADB = GetSelectedSubADB();
	if (pSelectedSubADB)
	{
		const string selectedFilename = pSelectedSubADB->filename;
		const bool bMoved = m_animDB->MoveSubADBFilter(selectedFilename, false);
		if (bMoved)
		{
			PopulateSubADBTree();
		}
		// find the item at it's new location and select it, so user can keep moving it if they want too.
		HTREEITEM item = FindItem(PathUtil::GetFile(selectedFilename.c_str()), *m_adbTree.get());
		if (item)
			m_adbTree->SelectItem(item);
	}
}

