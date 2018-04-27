// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "HyperGraph/HyperGraph.h"

class CEntityObject;
class CPrefabObject;
class CHyperFlowGraph;

/*
 * Panel with the list of available Graphs
 *
 * There is a fixed structure with folders for the different graph types (UI Actions, Modules, Entity Graphs..).
 * Currently all graphs are FlowGraphs and in fact it's only possible to check the graph type of a FG as HG doesn't have it.
 * Therefore, although this is meant to be a generic panel, it's quite specialized for FG.
 *
 * The Tree is responsible for showing all currently available graphs and their state (has modified changes? is disabled? has an error?)
 * The tree should also update when graphs are added, renamed or removed or change type such as when an entity is converted to a prefab.
 * Selecting a graph on the tree should select it for the all dialog - this is in HyperGraphEditorWnd.cpp
 * Right-clicking on one item (folder or graph) should present a context menu with options. - this is also in HyperGraphEditorWnd.cpp,
 *   although it should be refactored to belong here. CHyperGraphDialog::CreateContextMenu/ProcessMenu
 * The tree allows multiple item selection for bulk operations such as deleting or disabling. This is currently making everything complicated :)
 * 
 * Status: The Tree Ctrl was heavily reviewed in June 2017. The context menu still needs to be reviewed.
 * There should be support for drag and drop to move items between folders and in-place renaming without all the modal dialogs.
 * The ability to pin graphs or to have a tabbed system with open graphs is to be considered. A search for graph name would also be useful.
 * -- Ines 2017
 */

class CHyperGraphsTreeCtrl : public CXTTreeCtrl, private IHyperGraphManagerListener
{
private:
	// Important: keep in sync with the order of IFlowGraph::EFlowGraphType
	enum EFlowGraphImage
	{
		eFGI_Entity = 0,
		eFGI_AIAction,
		eFGI_UIAction,
		eFGI_Module,
		eFGI_CustomAction,
		eFGI_MaterialFx,
		eFGI_File,
		eFGI_Folder,
		eFGI_Disabled,
		eFGI_ErrorOffset,
	};

public:
	typedef std::vector<HTREEITEM> TDTreeItems;

	CHyperGraphsTreeCtrl();
	~CHyperGraphsTreeCtrl();

	virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);

	void         PrepareLevelUnload();
	void         SetIgnoreReloads(bool bIgnore) { m_bIgnoreReloads = bIgnore; }

	void         GetSelectedItems(TDTreeItems& rchSelectedTreeItems);
	void         SetCurrentGraph(CHyperGraph* pGraph);
	void         GetTypeDataForGraph(CHyperGraph* pGraph, EFlowGraphImage& outType, HTREEITEM& outHTopLevelFolder);

	void         UpdateGraphLabel(CHyperGraph* pGraph);

	void         RenameFolder(HTREEITEM hFolderItem, CString& newName);
	void         RenameFolderByName(HTREEITEM hParentItem, CString& folderName, CString& newName);
	void         RenameParentFolder(HTREEITEM hChildItem, CString& newName);

	void         AddGraphToTree(CHyperFlowGraph* pFlowGraph, bool isRebuild);
	void         RenameGraphItem(HTREEITEM hGraphItem, CString& newName);
	bool         DeleteGraphItem(CHyperGraph* pGraph);

	void         FullReload();

private:
	virtual void OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode);

	void         SortChildrenWithFoldersFirst(HTREEITEM hParent);

	void         AddGraphItemToTree(bool isRebuild, CString graphName, CHyperFlowGraph* pFlowGraph, EFlowGraphImage eType, HTREEITEM hParentfolder = NULL);
	void         DeleteEmptyParentFolders(HTREEITEM hFirstParent);



	CHyperGraph* m_pCurrentGraph;
	bool         m_bIgnoreReloads;
	CImageList   m_imageList;


	// inner duplicate representation for the tree. the MFC tree is quite inflexible
	struct STreeFolder
	{
		HTREEITEM hItem;
		CString name;
		EFlowGraphImage folderID;
		std::map<CString, HTREEITEM, stl::less_stricmp<CString>> subFolders;

		STreeFolder()
			: hItem(NULL)
			, name("")
			, folderID(eFGI_Disabled)
		{}
		STreeFolder(CXTTreeCtrl* pTreeCtrl, char* name, EFlowGraphImage folderID)
			: name(name), folderID(folderID)
		{
			hItem = pTreeCtrl->InsertItem(name, folderID, folderID);
		}
		~STreeFolder()
		{
			subFolders.clear();
		}
	};
	std::array<STreeFolder, 8> m_topLevelFolders;
};

