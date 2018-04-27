// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "MannTagDefEditorDialog.h"
#include <ICryMannequin.h>
#include <CryGame/IGameFramework.h>
#include <ISourceControl.h>
#include "Dialogs/QStringDialog.h"
#include "MannequinDialog.h"
#include "Controls/TreeCtrlEx.h"
#include "Util/MFCUtil.h"
#include "Helper/MannequinFileChangeWriter.h"

enum
{
	IDC_NEW_TAGDEF = AFX_IDW_PANE_FIRST,
	IDC_NEW_GROUP,
	IDC_EDIT_GROUP,
	IDC_DELETE_GROUP,
	IDC_NEW_TAG,
	IDC_EDIT_TAG,
	IDC_DELETE_TAG,
	IDC_TAGS_TREECTRL
};

//////////////////////////////////////////////////////////////////////////
// Subclass the CTreeCtrlEx to provide custom drag'n'drop behaviour
class CTagDefTreeCtrl : public CTreeCtrlEx
{
public:
	CTagDefTreeCtrl(CMannTagDefEditorDialog* editorDialog)
	{
		m_editorDialog = editorDialog;
	}

protected:
	//! Callback called when items is copied.
	virtual void OnItemCopied(HTREEITEM hItem, HTREEITEM hNewItem)
	{
		m_editorDialog->MoveTagInTree(hItem, hNewItem);
	}

	//! Check if drop source.
	virtual BOOL IsDropSource(HTREEITEM hItem)
	{
		// Only allow dragging of tags
		TagID id;
		return m_editorDialog->IsTagTreeItem(hItem, id);
	}

	//! Get item which is target for drop of this item.
	virtual HTREEITEM GetDropTarget(HTREEITEM hItem)
	{
		if (HTREEITEM hTarget = __super::GetDropTarget(hItem))
		{
			// Allow dropping onto the root
			if (hTarget == TVI_ROOT)
				return hTarget;

			// Allow dropping onto groups
			TagID id;
			if (!m_editorDialog->IsTagTreeItem(hItem, id))
				return hItem;
		}

		return NULL;
	}

private:
	CMannTagDefEditorDialog* m_editorDialog;
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMannTagDefEditorDialog, CXTResizeDialog)
ON_BN_CLICKED(IDC_NEW_TAGDEF, OnNewDefButton)
ON_BN_CLICKED(IDC_NEW_GROUP, OnNewGroupButton)
ON_BN_CLICKED(IDC_DELETE_GROUP, OnDeleteGroupButton)
ON_BN_CLICKED(IDC_NEW_TAG, OnNewTagButton)
ON_BN_CLICKED(IDC_EDIT_TAG, OnEditTagButton)
ON_BN_CLICKED(IDC_DELETE_TAG, OnDeleteTagButton)
ON_NOTIFY(LVN_ITEMCHANGED, IDC_TAG_DEFS_LIST, OnTagDefListItemChanged)
ON_NOTIFY(TVN_SELCHANGED, IDC_TAGS_TREECTRL, OnTagTreeSelChanged)
ON_NOTIFY(TVN_ENDLABELEDIT, IDC_TAGS_TREECTRL, OnTagTreeEndLabelEdit)
ON_EN_KILLFOCUS(IDC_PRIORITY_EDIT, OnPriorityEditChanged)
ON_BN_CLICKED(IDC_OKBUTTON, OnOKButton)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannTagDefEditorDialog::CMannTagDefEditorDialog(const CString& tagFilename, CWnd* pParent)
	: CXTResizeDialog(CMannTagDefEditorDialog::IDD, pParent)
	, m_selectedTagDef(NULL)
	, m_tooltip(NULL)
	, m_initialTagFilename(tagFilename)
{
	CMannequinDialog::GetCurrentInstance()->FragmentEditor()->TrackPanel()->FlushChanges();
}

//////////////////////////////////////////////////////////////////////////
CMannTagDefEditorDialog::~CMannTagDefEditorDialog()
{
	while (m_tagDefLocalCopy.size() > 0)
	{
		CTagDefinition* ptagDef = m_tagDefLocalCopy.back().m_pCopy;
		m_tagDefLocalCopy.pop_back();
		delete ptagDef;
	}

}

//////////////////////////////////////////////////////////////////////////
BOOL CMannTagDefEditorDialog::PreTranslateMessage(MSG* pMsg)
{
	if (m_tooltip)
		m_tooltip->RelayEvent(pMsg);

	return __super::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_TAGDEF_GROUPBOX, m_tagDefsGroupBox);
	DDX_Control(pDX, IDC_TAG_GROUPBOX, m_tagsGroupBox);
	DDX_Control(pDX, IDC_TAG_DEFS_LIST, m_tagDefList);
	DDX_Control(pDX, IDC_PRIORITY_LABEL, m_priorityLabel);
	DDX_Control(pDX, IDC_PRIORITY_EDIT, m_priorityEdit);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::CreateToolbarButton(CInPlaceButtonPtr& button, const char* text, CRect& rc, int index, int nID, CStatic* parent, CInPlaceButton::OnClick onClick)
{
	button.reset(new CInPlaceButton(onClick));
	button->Create("", WS_CHILD | WS_VISIBLE, rc, parent, nID);
	button->SetIcon(CSize(16, 16), m_imageListToolbar.ExtractIcon(index));
	button->SetBorderGap(2);
	button->SetFlatStyle(TRUE);

	m_tooltip->AddTool(button.get(), text);

	rc.OffsetRect(20, 0);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannTagDefEditorDialog::OnInitDialog()
{
	__super::OnInitDialog();

	m_tooltip = new CToolTipCtrl();
	m_tooltip->Create(this);

	CMFCUtils::LoadTrueColorImageList(m_imageListToolbar, IDB_MANN_TAGDEF_TOOLBAR, 16, RGB(255, 0, 255));

	CRect rc(10, 16, 30, 36);
	CreateToolbarButton(m_newDefButton, "New Tag Definition", rc, 0, IDC_NEW_TAGDEF, &m_tagDefsGroupBox, functor(*this, &CMannTagDefEditorDialog::OnNewDefButton));
	CreateToolbarButton(m_filterTagsButton, "Filter Tags", rc, 6, IDC_FILTER_TAGS, &m_tagDefsGroupBox, functor(*this, &CMannTagDefEditorDialog::OnFilterTagsButton));
	m_filterTagsButton->SetChecked(TRUE);

	rc.SetRect(10, 16, 30, 36);
	CreateToolbarButton(m_newGroupButton, "New Group", rc, 2, IDC_NEW_GROUP, &m_tagsGroupBox, functor(*this, &CMannTagDefEditorDialog::OnNewGroupButton));
	CreateToolbarButton(m_editGroupButton, "Edit Group", rc, 3, IDC_EDIT_GROUP, &m_tagsGroupBox, functor(*this, &CMannTagDefEditorDialog::OnEditGroupButton));
	CreateToolbarButton(m_deleteGroupButton, "Delete Group", rc, 4, IDC_DELETE_GROUP, &m_tagsGroupBox, functor(*this, &CMannTagDefEditorDialog::OnDeleteGroupButton));
	CreateToolbarButton(m_newTagButton, "New Tag", rc, 5, IDC_NEW_TAG, &m_tagsGroupBox, functor(*this, &CMannTagDefEditorDialog::OnNewTagButton));
	CreateToolbarButton(m_editTagButton, "Edit Tag", rc, 6, IDC_EDIT_TAG, &m_tagsGroupBox, functor(*this, &CMannTagDefEditorDialog::OnEditTagButton));
	CreateToolbarButton(m_deleteTagButton, "Delete Tag", rc, 7, IDC_DELETE_TAG, &m_tagsGroupBox, functor(*this, &CMannTagDefEditorDialog::OnDeleteTagButton));

	m_tooltip->Activate(TRUE);

	m_pCurrentTagFile = (CEdit*)GetDlgItem(IDC_CURRENT_TAG_FILE);

	m_tagsGroupBox.GetWindowRect(rc);
	ScreenToClient(rc);
	rc.DeflateRect(13, 41, 13, 40);

	m_tagsTree.reset(new CTagDefTreeCtrl(this));
	m_tagsTree->Create(
	  WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP |
	  TVS_HASBUTTONS | TVS_LINESATROOT | TVS_EDITLABELS | TVS_SHOWSELALWAYS,
	  rc, this, IDC_TAGS_TREECTRL);
	if (CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_MANN_TAGDEF_TREE, 16, RGB(255, 0, 255)))
		m_tagsTree->SetImageList(&m_imageList, TVSIL_NORMAL);

	AutoLoadPlacement("Dialogs\\MannequinTagDefEditor");

	m_tagDefList.InsertColumn(0, "TagDef Filename", LVCFMT_LEFT, LVSCW_AUTOSIZE_USEHEADER);

	PopulateTagDefList();
	EnableControls();

	// now select an entry if we were passed one initially
	if (!m_initialTagFilename.IsEmpty())
	{
		LVFINDINFO nfo;
		nfo.flags = LVFI_PARTIAL | LVFI_STRING;
		nfo.psz = m_initialTagFilename.GetBuffer();
		const int itemIdx = m_tagDefList.FindItem(&nfo);
		if (itemIdx >= 0)
		{
			m_tagDefList.SetItemState(itemIdx, LVIS_SELECTED, LVIS_SELECTED);

			CString text(m_selectedTagDef->GetFilename());
			text.Delete(0, MANNEQUIN_FOLDER.GetLength());
			m_pCurrentTagFile->SetWindowText(text);
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnOKButton()
{
	IMannequinEditorManager* pMgr = gEnv->pGameFramework->GetMannequinInterface().GetMannequinEditorManager();

	// Do renaming here
	for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
	{
		const CTagDefinition* pOriginal = m_tagDefLocalCopy[index].m_pOriginal;

		for (uint32 renameIndex = 0; renameIndex < m_tagDefLocalCopy[index].m_renameInfo.size(); ++renameIndex)
		{
			STagDefPair::SRenameInfo& renameInfo = m_tagDefLocalCopy[index].m_renameInfo[renameIndex];

			if (renameInfo.m_isGroup)
			{
				TagGroupID id = pOriginal->FindGroup(renameInfo.m_originalCRC);
				if (id != GROUP_ID_NONE)
				{
					//CryLog("[TAGDEF]: OnOKButton() (%s): renaming tag group [%s] to [%s]", pOriginal->GetFilename(), pOriginal->GetGroupName(id), renameInfo.m_newName);
					pMgr->RenameTagGroup(pOriginal, renameInfo.m_originalCRC, renameInfo.m_newName);
				}
			}
			else
			{
				TagID id = pOriginal->Find(renameInfo.m_originalCRC);
				if (id != TAG_ID_INVALID)
				{
					//CryLog("[TAGDEF]: OnOKButton() (%s): renaming tag [%s] to [%s]", pOriginal->GetFilename(), pOriginal->GetGroupName(id), renameInfo.m_newName);
					pMgr->RenameTag(pOriginal, renameInfo.m_originalCRC, renameInfo.m_newName);
				}
			}
		}
	}

	// Make a snapshot
	pMgr->SaveDatabasesSnapshot(m_snapshotCollection);

	// Apply changes
	bool allSucceeded = true;
	for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
	{
		if (m_tagDefLocalCopy[index].m_modified == true)
		{
			pMgr->ApplyTagDefChanges(m_tagDefLocalCopy[index].m_pOriginal, m_tagDefLocalCopy[index].m_pCopy);
		}
	}

	// Save changes
	IAnimationDatabaseManager& db = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
	CMannequinFileChangeWriter writer;
	db.SaveAll( &writer );

	// Reload the snapshot
	pMgr->LoadDatabasesSnapshot(m_snapshotCollection);

	CMannequinDialog::GetCurrentInstance()->LoadNewPreviewFile(NULL);
	CMannequinDialog::GetCurrentInstance()->StopEditingFragment();

	if (allSucceeded)
	{
		// All validation has passed so let the dialog close
		__super::OnOK();
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "The tag definition you have created exceeds the memory available! This tag definition has not been saved as it will cause data loss.");
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnNewDefButton()
{
	QStringDialog dialog("New Tag Definition Filename (*tags.xml)");
	if (dialog.exec() == QDialog::Accepted)
	{
		CString text = dialog.GetString();
		text.Trim();

		if (!ValidateTagDefName(text))
			return;

		IAnimationDatabaseManager& db = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
		// TODO: This is actually changing the live database - cancelling the tagdef editor will not remove this!
		CTagDefinition* pTagDef = db.CreateTagDefinition(MANNEQUIN_FOLDER + text);
		CTagDefinition* pTagDefCopy = new CTagDefinition(*pTagDef);
		STagDefPair tagDefPair(pTagDef, pTagDefCopy);
		m_tagDefLocalCopy.push_back(tagDefPair);

		AddTagDefToList(pTagDefCopy, true);
		m_tagDefList.SetFocus();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnNewGroupButton()
{
	QStringDialog dialog("New Group Name");
	if (dialog.exec() == QDialog::Accepted)
	{
		CString text = dialog.GetString();
		text.Trim();

		if (!ValidateGroupName(text))
			return;

		TagGroupID groupID = m_selectedTagDef->AddGroup(text);
		m_selectedTagDef->AssignBits();
		AddGroupToTree(text, groupID, true);
		m_tagsTree->SetFocus();

		STagDefPair* pTagDefPair = NULL;
		for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
		{
			if (m_tagDefLocalCopy[index].m_pCopy == m_selectedTagDef)
			{
				pTagDefPair = &m_tagDefLocalCopy[index];
				pTagDefPair->m_modified = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnEditGroupButton()
{
	m_tagsTree->EditLabel(m_tagsTree->GetSelectedItem());
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnDeleteGroupButton()
{
	HTREEITEM item = m_tagsTree->GetSelectedItem();
	CString itemText = m_tagsTree->GetItemText(item);

	CString message;
	message.Format("Delete the group '%s' and all contained tags?", itemText);

	if (ConfirmDelete(message))
	{
		HTREEITEM child = m_tagsTree->GetChildItem(item);
		while (child)
		{
			TagID tagID = m_tagsTree->GetItemData(child);
			m_selectedTagDef->RemoveTag(tagID);

			AdjustTagsForDelete(tagID);
			m_tagsTree->DeleteItem(child);

			child = m_tagsTree->GetNextSiblingItem(child);
		}

		TagGroupID groupID = GetSelectedTagGroupID();
		m_selectedTagDef->RemoveGroup(groupID);
		m_selectedTagDef->AssignBits();

		AdjustGroupsForDelete(groupID);
		m_tagsTree->DeleteItem(item);

		STagDefPair* pTagDefPair = NULL;
		for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
		{
			if (m_tagDefLocalCopy[index].m_pCopy == m_selectedTagDef)
			{
				pTagDefPair = &m_tagDefLocalCopy[index];
				pTagDefPair->m_modified = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnNewTagButton()
{
	QStringDialog dialog("New Tag Name");
	if (dialog.exec() == QDialog::Accepted)
	{
		CString text = dialog.GetString();
		text.Trim();

		if (!ValidateTagName(text))
			return;

		const TagGroupID groupID = GetSelectedTagGroupID();
		const char* const szGroup = groupID != TAG_ID_INVALID ? m_selectedTagDef->GetGroupName(groupID) : NULL;

		// First ensure that there's enough space to create the new tag
		CTagDefinition tempDefinition = *m_selectedTagDef;
		const TagID tagID = tempDefinition.AddTag(text, szGroup);
		const bool success = tempDefinition.AssignBits();
		if (!success)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Cannot add new tag: please check with a programmer to increase the tag definition size if necessary.");
			return;
		}

		HTREEITEM parent = TVI_ROOT;
		if (groupID != TAG_ID_INVALID)
		{
			parent = m_tagsTree->GetSelectedItem();
			m_tagsTree->Expand(parent, TVE_EXPAND);
		}

		*m_selectedTagDef = tempDefinition;
		AddTagToTree(text, tagID, parent, true);
		m_tagsTree->SetFocus();

		STagDefPair* pTagDefPair = NULL;
		for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
		{
			if (m_tagDefLocalCopy[index].m_pCopy == m_selectedTagDef)
			{
				pTagDefPair = &m_tagDefLocalCopy[index];
				pTagDefPair->m_modified = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnEditTagButton()
{
	const CEdit* pEdit = m_tagsTree->EditLabel(m_tagsTree->GetSelectedItem());
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnDeleteTagButton()
{
	HTREEITEM item = m_tagsTree->GetSelectedItem();
	CString itemText = m_tagsTree->GetItemText(item);
	CString message;
	TagID tagID = GetSelectedTagID();

	STagDefPair* pTagDefPair = NULL;
	for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
	{
		if (m_tagDefLocalCopy[index].m_pCopy == m_selectedTagDef)
		{
			pTagDefPair = &m_tagDefLocalCopy[index];
			break;
		}
	}

	char* buffer[4096];
	IMannequinEditorManager* pMgr = gEnv->pGameFramework->GetMannequinInterface().GetMannequinEditorManager();
	pMgr->GetAffectedFragmentsString(pTagDefPair->m_pOriginal, tagID, (char*)buffer, sizeof(buffer));
	message.Format("Deleting tag '%s' will affect:%s\nAre you sure?", itemText, buffer);

	if (ConfirmDelete(message))
	{
		m_selectedTagDef->RemoveTag(tagID);
		m_selectedTagDef->AssignBits();

		AdjustTagsForDelete(tagID);
		m_tagsTree->DeleteItem(item);

		pTagDefPair->m_modified = true;
		RemoveUnmodifiedTagDefs();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMannTagDefEditorDialog::ConfirmDelete(CString& text)
{
	return (MessageBox(text.GetBuffer(), "CryMannequin", MB_YESNO | MB_ICONQUESTION) == IDYES);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::PopulateTagDefListRec(const CString& baseDir)
{
	const bool filterTags = (m_filterTagsButton.get() && (m_filterTagsButton->GetChecked() == TRUE));
	const CString filter = filterTags ? TAGS_FILENAME_ENDING + TAGS_FILENAME_EXTENSION : TAGS_FILENAME_EXTENSION;

	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;

	intptr_t handle = pCryPak->FindFirst(baseDir + "*", &fd);
	if (handle != -1)
	{
		do
		{
			if (fd.name[0] == '.')
			{
				continue;
			}

			CString filename(baseDir + fd.name);
			if (fd.attrib & _A_SUBDIR)
			{
				PopulateTagDefListRec(filename + "/");
			}
			else
			{
				if (filename.Right(filter.GetLength()).CompareNoCase(filter) == 0)
				{
					if (XmlNodeRef root = GetISystem()->LoadXmlFromFile(filename))
					{
						// Ignore non-tagdef XML files
						CString tag = CString(root->getTag());
						if (tag == "TagDefinition")
						{
							IAnimationDatabaseManager& db = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
							const CTagDefinition* pTagDef = db.LoadTagDefs(filename, true);
							if (pTagDef)
							{
								CTagDefinition* pTagDefCopy = new CTagDefinition(*pTagDef);
								STagDefPair tagDefPair(pTagDef, pTagDefCopy);
								m_tagDefLocalCopy.push_back(tagDefPair);
								AddTagDefToList(pTagDefCopy);
							}
							else
							{
								CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "CMannTagDefEditorDialog: Failed to load existing tag definition file '%s'", filename);
							}
						}
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::RemoveUnmodifiedTagDefs(void)
{
	for (int index = m_tagDefList.GetItemCount() - 1; index >= 0; --index)
	{
		CTagDefinition* pItem = (CTagDefinition*)m_tagDefList.GetItemData(index);
		if (pItem != m_selectedTagDef)
		{
			m_tagDefList.DeleteItem(index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::PopulateTagDefList()
{
	m_tagDefList.DeleteAllItems();
	m_tagDefList.SetRedraw(FALSE);

	PopulateTagDefListRec(MANNEQUIN_FOLDER);

	m_tagDefList.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	m_tagDefList.SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::AddTagDefToList(CTagDefinition* pTagDef, bool select)
{
	int nItem = m_tagDefList.GetItemCount();

	CString text(pTagDef->GetFilename());
	text.Delete(0, MANNEQUIN_FOLDER.GetLength());

	nItem = m_tagDefList.InsertItem(nItem, text);
	m_tagDefList.SetItemData(nItem, (DWORD_PTR)pTagDef);

	if (select)
		m_tagDefList.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::EnableControls()
{
	BOOL bTagDefSelected = m_tagDefList.GetSelectedCount() > 0;
	m_tagsTree->EnableWindow(bTagDefSelected);

	BOOL bTagSelected = FALSE;
	BOOL bGroupSelected = FALSE;
	if (HTREEITEM item = m_tagsTree->GetSelectedItem())
	{
		TagID id;
		bTagSelected = IsTagTreeItem(item, id);
		bGroupSelected = !bTagSelected;
	}
	m_priorityLabel.EnableWindow(bTagSelected);
	m_priorityEdit.EnableWindow(bTagSelected);

	m_newGroupButton->EnableWindow(bTagDefSelected);
	m_editGroupButton->EnableWindow(bGroupSelected);
	m_deleteGroupButton->EnableWindow(bGroupSelected);
	m_newTagButton->EnableWindow(bTagDefSelected);
	m_editTagButton->EnableWindow(bTagSelected);
	m_deleteTagButton->EnableWindow(bTagSelected);

	m_newGroupButton->RedrawButton();
	m_editGroupButton->RedrawButton();
	m_deleteGroupButton->RedrawButton();
	m_newTagButton->RedrawButton();
	m_editTagButton->RedrawButton();
	m_deleteTagButton->RedrawButton();
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnTagDefListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* pNMIA = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

	if (m_tagDefList.GetSelectedCount() > 0)
	{
		m_selectedTagDef = (CTagDefinition*)m_tagDefList.GetItemData(pNMIA->iItem);

		CString text(m_selectedTagDef->GetFilename());
		text.Delete(0, MANNEQUIN_FOLDER.GetLength());
		m_pCurrentTagFile->SetWindowText(text);
	}
	else
	{
		m_selectedTagDef = NULL;
		m_pCurrentTagFile->SetWindowText("");
	}

	PopulateTagDefTree();
	EnableControls();
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::PopulateTagDefTree()
{
	m_tagsTree->DeleteAllItems();

	if (!m_selectedTagDef)
		return;

	m_tagsTree->SetRedraw(FALSE);

	// Create the group nodes
	int nGroups = m_selectedTagDef->GetNumGroups();
	for (int i = 0; i < nGroups; i++)
		AddGroupToTree(m_selectedTagDef->GetGroupName(i), i);

	// Create the tag nodes
	int nTags = m_selectedTagDef->GetNum();
	for (int i = 0; i < nTags; i++)
	{
		// Determine the correct parent group node, if one is needed
		HTREEITEM parent = TVI_ROOT;
		TagGroupID groupID = m_selectedTagDef->GetGroupID(i);
		if (groupID != GROUP_ID_NONE)
		{
			HTREEITEM child = m_tagsTree->GetChildItem(parent);
			while (child)
			{
				TagID id;
				if (!IsTagTreeItem(child, id) && (id == groupID))
				{
					parent = child;
					break;
				}
				child = m_tagsTree->GetNextSiblingItem(child);
			}
		}

		AddTagToTree(m_selectedTagDef->GetTagName(i), i, parent);
	}

	m_tagsTree->SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CMannTagDefEditorDialog::AddGroupToTree(const char* szGroup, TagGroupID groupID, bool select)
{
	HTREEITEM item = m_tagsTree->InsertItem(szGroup, 0, 0, TVI_ROOT, TVI_SORT);
	m_tagsTree->SetItemData(item, groupID);
	if (select)
		m_tagsTree->SelectItem(item);
	return item;
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CMannTagDefEditorDialog::AddTagToTree(const char* szTag, TagID tagID, HTREEITEM parent, bool select)
{
	HTREEITEM item = m_tagsTree->InsertItem(szTag, 1, 1, parent, TVI_SORT);
	m_tagsTree->SetItemData(item, tagID);
	if (select)
		m_tagsTree->SelectItem(item);
	return item;
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnTagTreeSelChanged(NMHDR*, LRESULT*)
{
	if (!m_selectedTagDef)
		return;

	if (HTREEITEM item = m_tagsTree->GetSelectedItem())
	{
		TagID id;
		if (IsTagTreeItem(item, id))
		{
			uint32 priority = m_selectedTagDef->GetPriority(id);
			priority = std::max(std::min(priority, 10u), 0u);
			CString priorityText;
			priorityText.Format("%i", priority);
			m_priorityEdit.SetWindowText(priorityText);
		}
		else
		{
			m_priorityEdit.SetWindowText("");
		}
	}

	EnableControls();
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnTagTreeEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
	bool valid = true;

	LPNMTVDISPINFO tvi = (LPNMTVDISPINFO)pNMHDR;
	const char* szText = tvi->item.pszText;

	if (szText)
	{
		TagID id;
		if (IsTagTreeItem(tvi->item.hItem, id))
		{
			// Tag
			if (ValidateTagName(szText))
			{
				for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
				{
					if (m_tagDefLocalCopy[index].m_pCopy == m_selectedTagDef)
					{
						STagDefPair::SRenameInfo renameInfo;
						renameInfo.m_originalCRC = m_selectedTagDef->GetTagCRC(tvi->item.lParam);
						renameInfo.m_newName = szText;
						renameInfo.m_isGroup = false;

						m_tagDefLocalCopy[index].m_renameInfo.push_back(renameInfo);
						m_tagDefLocalCopy[index].m_modified = true;
						RemoveUnmodifiedTagDefs();
						break;
					}
				}

				m_selectedTagDef->SetTagName(tvi->item.lParam, szText);
			}
			else
				valid = false;
		}
		else
		{
			// Group
			if (ValidateGroupName(szText))
			{
				for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
				{
					if (m_tagDefLocalCopy[index].m_pCopy == m_selectedTagDef)
					{
						STagDefPair::SRenameInfo renameInfo;
						renameInfo.m_originalCRC = m_selectedTagDef->GetGroupCRC(tvi->item.lParam);
						renameInfo.m_newName = szText;
						renameInfo.m_isGroup = true;

						m_tagDefLocalCopy[index].m_renameInfo.push_back(renameInfo);
						m_tagDefLocalCopy[index].m_modified = true;
						RemoveUnmodifiedTagDefs();
						break;
					}
				}

				m_selectedTagDef->SetGroupName(tvi->item.lParam, szText);
			}
			else
				valid = false;
		}
	}
	else
	{
		valid = false;
	}

	*pResult = valid;
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnPriorityEditChanged()
{
	if (!m_selectedTagDef)
		return;

	TagID tagID = GetSelectedTagID();
	if (tagID != TAG_ID_INVALID)
	{
		CString text;
		m_priorityEdit.GetWindowText(text);

		m_selectedTagDef->SetPriority(tagID, atoi(text.GetString()));
		for (uint32 index = 0; index < m_tagDefLocalCopy.size(); ++index)
		{
			if (m_selectedTagDef == m_tagDefLocalCopy[index].m_pCopy)
			{
				m_tagDefLocalCopy[index].m_modified = true;
				RemoveUnmodifiedTagDefs();
				break;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
TagID CMannTagDefEditorDialog::GetSelectedTagID()
{
	if (HTREEITEM item = m_tagsTree->GetSelectedItem())
	{
		TagID id;
		if (IsTagTreeItem(item, id))
			return id;
	}
	return TAG_ID_INVALID;
}

//////////////////////////////////////////////////////////////////////////
TagGroupID CMannTagDefEditorDialog::GetSelectedTagGroupID()
{
	if (HTREEITEM item = m_tagsTree->GetSelectedItem())
	{
		TagID id;
		if (!IsTagTreeItem(item, id))
			return id;
	}
	return TAG_ID_INVALID;
}

//////////////////////////////////////////////////////////////////////////
bool CMannTagDefEditorDialog::IsTagTreeItem(HTREEITEM hItem, TagID& tagID)
{
	int nImage, nSelImage;
	m_tagsTree->GetItemImage(hItem, nImage, nSelImage);
	tagID = m_tagsTree->GetItemData(hItem);
	return (nImage == 1);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::MoveTagInTree(HTREEITEM hItem, HTREEITEM hNewItem)
{
	TagID tagID = m_tagsTree->GetItemData(hNewItem);

	if (HTREEITEM hParent = m_tagsTree->GetParentItem(hNewItem))
	{
		TagGroupID groupID = m_tagsTree->GetItemData(hParent);
		m_selectedTagDef->SetTagGroup(tagID, groupID);
	}
	else
	{
		m_selectedTagDef->SetTagGroup(tagID, GROUP_ID_NONE);
	}

	m_tagsTree->SortChildren(m_tagsTree->GetParentItem(hNewItem));
	m_selectedTagDef->AssignBits();
}

//////////////////////////////////////////////////////////////////////////
bool CMannTagDefEditorDialog::ValidateTagName(const CString& name)
{
	CString errorMessage;

	// Check string format
	if (name.IsEmpty())
	{
		errorMessage.Format("Invalid tag name");
		MessageBox(errorMessage.GetBuffer(), "CryMannequin", MB_OK | MB_ICONERROR);
		return false;
	}

	// Check for duplicates
	if (m_selectedTagDef->Find(name) != TAG_ID_INVALID)
	{
		errorMessage.Format("The tag name '%s' already exists", name);
		MessageBox(errorMessage.GetBuffer(), "CryMannequin", MB_OK | MB_ICONERROR);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMannTagDefEditorDialog::ValidateGroupName(const CString& name)
{
	CString errorMessage;

	// Check string format
	if (name.IsEmpty())
	{
		errorMessage.Format("Invalid group name");
		MessageBox(errorMessage.GetBuffer(), "CryMannequin", MB_OK | MB_ICONERROR);
		return false;
	}

	// Check for duplicates
	if (m_selectedTagDef->FindGroup(name) != GROUP_ID_NONE)
	{
		errorMessage.Format("The group name '%s' already exists", name);
		MessageBox(errorMessage.GetBuffer(), "CryMannequin", MB_OK | MB_ICONERROR);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMannTagDefEditorDialog::ValidateTagDefName(CString& name)
{
	CString errorMessage;

	// Check string format
	if (name.IsEmpty())
	{
		errorMessage.Format("Invalid tag definition name");
		MessageBox(errorMessage.GetBuffer(), "CryMannequin", MB_OK | MB_ICONERROR);
		return false;
	}

	if ((name.Find('/', 0) >= 0) || (name.Find('\\', 0) >= 0))
	{
		errorMessage.Format("Invalid tag definition name - cannot contain path information");
		MessageBox(errorMessage.GetBuffer(), "CryMannequin", MB_OK | MB_ICONERROR);
		return false;
	}

	// Ensure the name has the correct ending and extension
	CString filename(name);
	int dot = filename.ReverseFind('.');
	if (dot > 0)
	{
		filename.Delete(dot, filename.GetLength() - dot);
	}

	if (filename.Right(TAGS_FILENAME_ENDING.GetLength()).CompareNoCase(TAGS_FILENAME_ENDING) != 0)
	{
		filename += TAGS_FILENAME_ENDING;
	}

	filename += TAGS_FILENAME_EXTENSION;

	// Check for duplicates
	IAnimationDatabaseManager& db = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
	if (db.FindTagDef(MANNEQUIN_FOLDER + filename))
	{
		errorMessage.Format("The tag definition name '%s' already exists", filename);
		MessageBox(errorMessage.GetBuffer(), "CryMannequin", MB_OK | MB_ICONERROR);
		return false;
	}

	name = filename;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::AdjustTagsForDelete(TagID deletedTagID, HTREEITEM hItem)
{
	HTREEITEM hCurrent = m_tagsTree->GetChildItem(hItem);
	while (hCurrent != NULL)
	{
		TagID tagID;
		if (IsTagTreeItem(hCurrent, tagID))
		{
			if (tagID > deletedTagID)
				m_tagsTree->SetItemData(hCurrent, tagID - 1);
		}
		else
		{
			AdjustTagsForDelete(deletedTagID, hCurrent);
		}

		hCurrent = m_tagsTree->GetNextSiblingItem(hCurrent);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::AdjustGroupsForDelete(TagGroupID deletedGroupID)
{
	HTREEITEM hCurrent = m_tagsTree->GetChildItem(TVI_ROOT);
	while (hCurrent != NULL)
	{
		TagGroupID groupID;
		if (!IsTagTreeItem(hCurrent, groupID))
		{
			if (groupID > deletedGroupID)
				m_tagsTree->SetItemData(hCurrent, groupID - 1);
		}

		hCurrent = m_tagsTree->GetNextSiblingItem(hCurrent);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagDefEditorDialog::OnFilterTagsButton()
{
	m_filterTagsButton->SetChecked(!m_filterTagsButton->GetChecked());

	PopulateTagDefList();
	EnableControls();
}

