// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "Util/FileChangeMonitor.h"

#ifndef __FOLDER_TREE_CTRL__H__
	#define __FOLDER_TREE_CTRL__H__

class CFolderTreeCtrl : public CTreeCtrl, public CFileChangeMonitorListener
{
	DECLARE_DYNAMIC(CFolderTreeCtrl)

	friend class CTreeItem;

	class CTreeItem
	{
		// Only allow destruction through std::unique_ptr
		friend struct std::default_delete<CTreeItem>;

	public:
		explicit CTreeItem(CFolderTreeCtrl& folderTreeCtrl, const CString& path);
		explicit CTreeItem(CFolderTreeCtrl& folderTreeCtrl, CTreeItem* parent,
		                   const CString& name, const CString& path, const int image);

		void       Remove();
		CTreeItem* GetParent() const { return m_parent; }
		CTreeItem* AddChild(const CString& name, const CString& path, const int image);
		void       RemoveChild(CTreeItem* item);
		bool       HasChildren() const { return m_children.size() > 0; }
		CString    GetPath() const     { return m_path; }

		operator HTREEITEM() { return m_item; }

	private:
		~CTreeItem();

		CFolderTreeCtrl&                        m_folderTreeCtrl;
		HTREEITEM                               m_item;
		CTreeItem*                              m_parent;
		CString                                 m_path;
		std::vector<std::unique_ptr<CTreeItem>> m_children;
	};

public:
	CFolderTreeCtrl(const std::vector<CString>& folders, const CString& fileNameSpec,
	                const CString& rootName, bool bDisableMonitor = false, bool bFlatTree = true);
	virtual ~CFolderTreeCtrl() {}

	CString GetPath(HTREEITEM item) const;
	bool    IsFolder(HTREEITEM item) const;
	bool    IsFile(HTREEITEM item) const;

protected:
	DECLARE_MESSAGE_MAP();

	virtual void PreSubclassWindow();
	afx_msg void OnDestroy();
	virtual void OnFileMonitorChange(const SFileChangeInfo& rChange);
	afx_msg void OnRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint mousePos);

	void         InitTree();
	void         LoadTreeRec(const CString& currentFolder);

	void         AddItem(const CString& path);
	void         RemoveItem(const CString& path);
	CTreeItem*   GetItem(const CString& path);

	CString      CalculateFolderFullPath(const std::vector<string>& splittedFolder, int idx);
	CTreeItem*   CreateFolderItems(const CString& folder);
	void         RemoveEmptyFolderItems(const CString& folder);

	void         Edit(const CString& path);
	void         ShowInExplorer(const CString& path);

	bool                       m_bDisableMonitor;
	bool                       m_bFlatStyle;
	std::unique_ptr<CTreeItem> m_rootTreeItem;
	CString                    m_fileNameSpec;
	std::vector<CString>       m_folders;
	CString                    m_rootName;
	CImageList                 m_imageList;

	std::map<CString, CTreeItem*, stl::less_stricmp<CString>> m_pathToTreeItem;
};

#endif

