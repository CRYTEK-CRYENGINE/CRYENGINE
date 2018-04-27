// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "FolderTreeCtrl.h"
#include "Controls/DynamicPopupMenu.h"
#include "FilePathUtil.h"
#include "Util/MFCUtil.h"

IMPLEMENT_DYNAMIC(CFolderTreeCtrl, CTreeCtrl)

enum ETreeImage
{
	eTreeImage_Folder = 0,
	eTreeImage_File   = 2
};

//////////////////////////////////////////////////////////////////////////
// CFolderTreeCtrl
//////////////////////////////////////////////////////////////////////////
CFolderTreeCtrl::CFolderTreeCtrl(const std::vector<CString>& folders, const CString& fileNameSpec,
                                 const CString& rootName, bool bDisableMonitor, bool bFlatTree) : m_rootTreeItem(nullptr), m_folders(folders),
	m_fileNameSpec(fileNameSpec), m_rootName(rootName), m_bDisableMonitor(bDisableMonitor), m_bFlatStyle(bFlatTree)
{

	for (auto item = m_folders.begin(), end = m_folders.end(); item != end; ++item)
	{
		(*item) = PathUtil::RemoveSlash(PathUtil::ToUnixPath(item->GetString())).c_str();

		if (!CFileUtil::PathExists((*item)))
		{
			(*item).Empty();
		}
	}
}

BEGIN_MESSAGE_MAP(CFolderTreeCtrl, CTreeCtrl)
ON_WM_DESTROY()
ON_NOTIFY_REFLECT(NM_RCLICK, OnRClick)
ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

CString CFolderTreeCtrl::GetPath(HTREEITEM item) const
{
	CTreeItem* treeItem = (CTreeItem*)GetItemData(item);

	if (treeItem)
		return treeItem->GetPath();

	return "";
}

bool CFolderTreeCtrl::IsFolder(HTREEITEM item) const
{
	int itemImage;
	int selectedImage;
	GetItemImage(item, itemImage, selectedImage);

	return itemImage == eTreeImage_Folder;
}

bool CFolderTreeCtrl::IsFile(HTREEITEM item) const
{
	return !IsFolder(item);
}

void CFolderTreeCtrl::PreSubclassWindow()
{
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_ENTITY_TREE, 16, RGB(255, 0, 255));

	SetImageList(&m_imageList, TVSIL_NORMAL);
	SetIndent(0);

	InitTree();

	CTreeCtrl::PreSubclassWindow();
}

void CFolderTreeCtrl::OnDestroy()
{
	// Obliterate tree items before destroying the controls
	m_rootTreeItem.reset(nullptr);

	// Unsubscribe from file change notifications
	if (!m_bDisableMonitor)
		CFileChangeMonitor::Instance()->Unsubscribe(this);

	CTreeCtrl::OnDestroy();
}

void CFolderTreeCtrl::OnFileMonitorChange(const SFileChangeInfo& rChange)
{
	const CString filePath = PathUtil::ToUnixPath(PathUtil::AbsolutePathToCryPakPath(rChange.filename.GetString())).c_str();
	for (auto item = m_folders.begin(), end = m_folders.end(); item != end; ++item)
	{
		// Only look for changes in folder
		if (filePath.Find((*item)) != 0)
			return;

		if (rChange.changeType == rChange.eChangeType_Created || rChange.changeType == rChange.eChangeType_RenamedNewName)
		{
			if (CFileUtil::PathExists(filePath))
				LoadTreeRec(filePath);
			else
				AddItem(filePath);
		}
		else if (rChange.changeType == rChange.eChangeType_Deleted || rChange.changeType == rChange.eChangeType_RenamedOldName)
			RemoveItem(filePath);
	}
}

void CFolderTreeCtrl::OnRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	SendMessage(WM_CONTEXTMENU, (WPARAM)m_hWnd, GetMessagePos());
	// Mark message as handled and suppress default handling
	*pResult = 1;
}

void CFolderTreeCtrl::OnContextMenu(CWnd* pWnd, CPoint mousePos)
{
	// if Shift-F10
	if (mousePos.x == -1 && mousePos.y == -1)
		mousePos = (CPoint)GetMessagePos();

	ScreenToClient(&mousePos);

	UINT flags;
	const HTREEITEM item = HitTest(mousePos, &flags);

	if (item == NULL)
		return;

	const CString path = GetPath(item);

	CDynamicPopupMenu menu;
	CPopupMenuItem& root = menu.GetRoot();
	root.Add<const CString&>("Edit", functor(*this, &CFolderTreeCtrl::Edit), path);
	root.Add<const CString&>("Show in Explorer", functor(*this, &CFolderTreeCtrl::ShowInExplorer), path);
	menu.SpawnAtCursor();
}

void CFolderTreeCtrl::InitTree()
{
	m_rootTreeItem.reset(new CTreeItem(*this, m_rootName));

	for (auto item = m_folders.begin(), end = m_folders.end(); item != end; ++item)
	{
		if (!(*item).IsEmpty())
			LoadTreeRec((*item));
	}

	if (!m_bDisableMonitor)
	{
		CFileChangeMonitor::Instance()->Subscribe(this);
	}

	Expand(*m_rootTreeItem, TVE_EXPAND);
}

void CFolderTreeCtrl::LoadTreeRec(const CString& currentFolder)
{
	CFileEnum fileEnum;
	__finddata64_t fileData;

	const CString currentFolderSlash = PathUtil::AddSlash(currentFolder.GetString());

	for (bool bFoundFile = fileEnum.StartEnumeration(currentFolder, "*", &fileData);
	     bFoundFile; bFoundFile = fileEnum.GetNextFile(&fileData))
	{
		const CString fileName = fileData.name;
		const CString ext = PathUtil::GetExt(fileName);

		// Have we found a folder?
		if (fileData.attrib & _A_SUBDIR)
		{
			// Skip the parent folder entries
			if (fileName == "." || fileName == "..")
				continue;

			LoadTreeRec(currentFolderSlash + fileName);
		}

		AddItem(currentFolderSlash + fileName);
	}
}

void CFolderTreeCtrl::AddItem(const CString& path)
{
	string folder;
	string fileNameWithoutExtension;
	string ext;

	PathUtil::Split(string(path), folder, fileNameWithoutExtension, ext);

	if (PathMatchSpec(path, m_fileNameSpec) && !GetItem(path))
	{
		CTreeItem* folderTreeItem = CreateFolderItems(folder.c_str());
		CTreeItem* fileTreeItem = folderTreeItem->AddChild(fileNameWithoutExtension.c_str(), path, eTreeImage_File);
	}
}

void CFolderTreeCtrl::RemoveItem(const CString& path)
{
	if (!CFileUtil::FileExists(path))
	{
		auto findIter = m_pathToTreeItem.find(path);

		if (findIter != m_pathToTreeItem.end())
		{
			CTreeItem* foundItem = findIter->second;
			foundItem->Remove();
			RemoveEmptyFolderItems(PathUtil::GetPathWithoutFilename(path).c_str());
		}
	}
}

CFolderTreeCtrl::CTreeItem* CFolderTreeCtrl::GetItem(const CString& path)
{
	auto findIter = m_pathToTreeItem.find(path);

	if (findIter == m_pathToTreeItem.end())
		return nullptr;

	return findIter->second;
}

CString CFolderTreeCtrl::CalculateFolderFullPath(const std::vector<string>& splittedFolder, int idx)
{
	CString path;
	for (int segIdx = 0; segIdx <= idx; ++segIdx)
	{
		if (segIdx != 0)
			path.Append("/");
		path.AppendFormat("%s", splittedFolder[segIdx].c_str());
	}
	return path;
}

CFolderTreeCtrl::CTreeItem* CFolderTreeCtrl::CreateFolderItems(const CString& folder)
{
	std::vector<string> splittedFolder = PathUtil::SplitIntoSegments(folder.GetString());
	CTreeItem* currentTreeItem = m_rootTreeItem.get();

	if (!m_bFlatStyle)
	{
		CString currentFolder;
		CString fullpath;
		const int splittedFoldersCount = splittedFolder.size();
		for (int idx = 0; idx < splittedFoldersCount; ++idx)
		{
			currentFolder = PathUtil::RemoveSlash(splittedFolder[idx]).c_str();
			fullpath = CalculateFolderFullPath(splittedFolder, idx);

			CTreeItem* folderItem = GetItem(fullpath);
			if (!folderItem)
			{
				currentTreeItem = currentTreeItem->AddChild(currentFolder, fullpath, eTreeImage_Folder);
			}
			else
			{
				currentTreeItem = folderItem;
			}

			Expand(*currentTreeItem, TVE_EXPAND);
		}
	}

	Expand(*m_rootTreeItem, TVE_EXPAND);
	return currentTreeItem;
}

void CFolderTreeCtrl::RemoveEmptyFolderItems(const CString& folder)
{
	std::vector<string> splittedFolder = PathUtil::SplitIntoSegments(folder.GetString());
	const int splittedFoldersCount = splittedFolder.size();
	CString fullpath;
	for (int idx = 0; idx < splittedFoldersCount; ++idx)
	{
		fullpath = CalculateFolderFullPath(splittedFolder, idx);
		CTreeItem* folderItem = GetItem(fullpath);
		if (!folderItem)
			continue;

		if (!folderItem->HasChildren())
			folderItem->Remove();
	}
}

void CFolderTreeCtrl::Edit(const CString& path)
{
	CFileUtil::EditTextFile(path, 0, CFileUtil::FILE_TYPE_SCRIPT, false);
}

void CFolderTreeCtrl::ShowInExplorer(const CString& path)
{
	char workingDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, workingDirectory);

	CString absolutePath(workingDirectory);

	CTreeItem* root = m_rootTreeItem.get();
	CTreeItem* item = GetItem(path);

	if (item != root)
		absolutePath.AppendFormat("/%s", path);

	// Explorer command lines needs windows style paths
	absolutePath.Replace('/', '\\');

	const CString parameters = "/select," + absolutePath;
	ShellExecute(NULL, "Open", "explorer.exe", parameters, "", SW_NORMAL);
}

//////////////////////////////////////////////////////////////////////////
// CFolderTreeCtrl::CTreeItem
//////////////////////////////////////////////////////////////////////////
CFolderTreeCtrl::CTreeItem::CTreeItem(CFolderTreeCtrl& folderTreeCtrl, const CString& path)
	: m_folderTreeCtrl(folderTreeCtrl), m_path(path), m_parent(nullptr)
{
	m_item = folderTreeCtrl.InsertItem(m_folderTreeCtrl.m_rootName, eTreeImage_Folder, eTreeImage_Folder, TVI_ROOT);
	m_folderTreeCtrl.SetItemData(m_item, (DWORD_PTR)this);

	m_folderTreeCtrl.m_pathToTreeItem[m_path] = this;
}

CFolderTreeCtrl::CTreeItem::CTreeItem(CFolderTreeCtrl& folderTreeCtrl, CFolderTreeCtrl::CTreeItem* parent,
                                      const CString& name, const CString& path, const int image)
	: m_folderTreeCtrl(folderTreeCtrl), m_path(path), m_parent(parent)
{
	m_item = folderTreeCtrl.InsertItem(name, image, image, parent->m_item);
	m_folderTreeCtrl.SetItemData(m_item, (DWORD_PTR)this);

	m_folderTreeCtrl.m_pathToTreeItem[m_path] = this;
}

CFolderTreeCtrl::CTreeItem::~CTreeItem()
{
	m_folderTreeCtrl.m_pathToTreeItem.erase(m_path);
	m_children.clear();
	m_folderTreeCtrl.DeleteItem(m_item);
}

void CFolderTreeCtrl::CTreeItem::Remove()
{
	// Root can't be deleted this way
	if (m_parent)
		m_parent->RemoveChild(this);
}

CFolderTreeCtrl::CTreeItem* CFolderTreeCtrl::CTreeItem::AddChild(const CString& name, const CString& path, const int image)
{
	CTreeItem* newItem = new CTreeItem(m_folderTreeCtrl, this, name, path, image);
	m_folderTreeCtrl.SortChildren(m_item);
	m_children.push_back(std::unique_ptr<CTreeItem>(newItem));
	return newItem;
}

void CFolderTreeCtrl::CTreeItem::RemoveChild(CTreeItem* item)
{
	auto findIter = std::find_if(m_children.begin(), m_children.end(), [&](std::unique_ptr<CTreeItem>& currentItem)
	{
		return currentItem.get() == item;
	});

	if (findIter != m_children.end())
		m_children.erase(findIter);
}

