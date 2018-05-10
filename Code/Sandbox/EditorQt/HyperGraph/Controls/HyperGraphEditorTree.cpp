// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperGraphEditorTree.h"

#include <CryGame/IGameFramework.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIAction.h>
#include <CryAction/ICustomActions.h>
#include <CryFlowGraph/IFlowGraphModuleManager.h>

#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraphEditorWnd.h"

#include "Objects/EntityObject.h"
#include "Objects/PrefabObject.h"

#include "Util/MFCUtil.h"


// ---- Sorting Kludge - all this to sort alphabetically with folders first

struct SHGTCReloadItem
{
	CString          name;
	int              image;
	HTREEITEM        parent;
	CHyperFlowGraph* pFlowGraph;

	SHGTCReloadItem(CString name, int image, HTREEITEM parent, CHyperFlowGraph* pFlowGraph)
		: name(name), image(image), parent(parent), pFlowGraph(pFlowGraph)
	{ }

	bool operator<(const SHGTCReloadItem& rhs) const
	{
		return name.Compare(rhs.name) < 0;
	}

	HTREEITEM Add(CHyperGraphsTreeCtrl* pCtrl)
	{
		HTREEITEM hItem = pCtrl->InsertItem(name, image, image, parent, TVI_LAST);
		pCtrl->SetItemData(hItem, DWORD_PTR(pFlowGraph)); // necessary for the custom sorting function
		pFlowGraph->SetHTreeItem(hItem);
		return hItem;
	}

};
std::vector<SHGTCReloadItem> items;

int CALLBACK SortAlphabeticallyWithFoldersFirstCB(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CHyperGraphsTreeCtrl* pTreeCtrl = (CHyperGraphsTreeCtrl*)lParamSort;

	bool bIsFolderItem1 = pTreeCtrl->ItemHasChildren((HTREEITEM)lParam1);
	bool bIsFolderItem2 = pTreeCtrl->ItemHasChildren((HTREEITEM)lParam2);

	if (bIsFolderItem1 && !bIsFolderItem2)
		return -1;
	if (!bIsFolderItem1 && bIsFolderItem2)
		return 1;

	// both are folders or items
	CString sItem1 = pTreeCtrl->GetItemText((HTREEITEM)lParam1);
	CString sItem2 = pTreeCtrl->GetItemText((HTREEITEM)lParam2);
	return sItem1.Compare(sItem2);
}

void CHyperGraphsTreeCtrl::SortChildrenWithFoldersFirst(HTREEITEM hParent)
{
	// MFC does not allow sorting configuration. Therefore, this function is a huge workaround to simply
	// ensure that the first level children of hParent are sorted alphabetically with folders first
	TVSORTCB tvs;
	tvs.hParent = hParent;
	tvs.lpfnCompare = SortAlphabeticallyWithFoldersFirstCB;
	tvs.lParam = (LPARAM)this;

	std::map<HTREEITEM, DWORD_PTR> tempFGPointerHolder;
	HTREEITEM hChildItem = GetNextItem(hParent, TVGN_CHILD);
	while (hChildItem)
	{
		tempFGPointerHolder.emplace(hChildItem, GetItemData(hChildItem));
		SetItemData(hChildItem, DWORD_PTR(hChildItem)); // because MFC is incredibly retarded
		hChildItem = GetNextItem(hChildItem, TVGN_NEXT);
	}

	SortChildrenCB(&tvs);

	hChildItem = GetNextItem(hParent, TVGN_CHILD);
	while (hChildItem)
	{
		SetItemData(hChildItem, DWORD_PTR(tempFGPointerHolder[hChildItem]));
		hChildItem = GetNextItem(hChildItem, TVGN_NEXT);
	}
}

// ---- ~Sorting Kludge



CHyperGraphsTreeCtrl::CHyperGraphsTreeCtrl()
	: m_pCurrentGraph(nullptr)
	, m_bIgnoreReloads(false)
{
	GetIEditorImpl()->GetFlowGraphManager()->AddListener(this);
}


CHyperGraphsTreeCtrl::~CHyperGraphsTreeCtrl()
{
	GetIEditorImpl()->GetFlowGraphManager()->RemoveListener(this);
}


BOOL CHyperGraphsTreeCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	BOOL ret = __super::Create(dwStyle | WS_BORDER | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS, rect, pParentWnd, nID);

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_HYPERGRAPH_TREE, 16, RGB(255, 0, 255));
	SetImageList(&m_imageList, TVSIL_NORMAL);
	return ret;
}


void CHyperGraphsTreeCtrl::GetSelectedItems(TDTreeItems& rchSelectedTreeItems)
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	HTREEITEM hCurrentItem = GetFirstSelectedItem();
	if (hCurrentItem == NULL)
	{
		return;
	}
	else
	{
		do
		{
			rchSelectedTreeItems.push_back(hCurrentItem);
			hCurrentItem = GetNextSelectedItem(hCurrentItem);
		} while (hCurrentItem);
	}
}


void CHyperGraphsTreeCtrl::SetCurrentGraph(CHyperGraph* pCurrentGraph)
{
	if (pCurrentGraph == m_pCurrentGraph)
		return;

	m_pCurrentGraph = pCurrentGraph;
	if (pCurrentGraph)
	{
		HTREEITEM hItem = pCurrentGraph->GetHTreeItem();
		if (hItem)
		{
			EnsureVisible(hItem);
			Select(hItem, TVGN_CARET);
		}
	}
}


void CHyperGraphsTreeCtrl::UpdateGraphLabel(CHyperGraph* pGraph)
{
	if (pGraph && !m_bIgnoreReloads)
	{
		HTREEITEM hItem = pGraph->GetHTreeItem();

		string label(pGraph->GetName());

		// graph with modifications?
		const bool bIsModified = pGraph->IsModified();
		SetItemText(hItem, bIsModified ? label.append("*") : label);
		SetItemBold(hItem, bIsModified);

		// enabled/disabled + error icon
		if (pGraph->IsFlowGraph())
		{
			CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
			const bool bIsDisabled = !pFlowGraph->IsEnabled();
			const bool bHasError = pFlowGraph->HasError();

			SetItemColor(hItem, bIsDisabled ? RGB(135,135,135) : GetTextColor());

			int img = bIsDisabled ? eFGI_Disabled : pFlowGraph->GetIFlowGraph()->GetType();
			if (bHasError)
				img += eFGI_ErrorOffset;
			SetItemImage(hItem, img, img);
		}
	}
}


void CHyperGraphsTreeCtrl::GetTypeDataForGraph(CHyperGraph* pGraph, EFlowGraphImage& outType, HTREEITEM& outHTopLevelFolder)
{
	outType = eFGI_ErrorOffset;
	outHTopLevelFolder = NULL;
	if (static_cast<CHyperGraph*>(pGraph)->IsFlowGraph())
	{
		CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
		IFlowGraph::EFlowGraphType type = pFlowGraph->GetIFlowGraph()->GetType();

		outType = static_cast<EFlowGraphImage>(type);
		if (type == IFlowGraph::eFGT_Default)
		{
			if (CEntityObject* pEntity = pFlowGraph->GetEntity())
			{
				outType = eFGI_Entity;
				if (pEntity->GetGroup() && pEntity->GetPrefab())
				{
					outType = eFGI_Folder;
				}
			}
			else
			{
				outType = eFGI_File;
			}
		}

		outHTopLevelFolder = m_topLevelFolders[outType].hItem;
		if (outType == eFGI_Module)
		{
			bool bIsGlobal = true;
			if (IFlowGraphModule* pModule = gEnv->pFlowSystem->GetIModuleManager()->GetModule(pFlowGraph->GetName()))
			{
				bIsGlobal = (pModule->GetType() == IFlowGraphModule::eT_Global);
			}
			outHTopLevelFolder = m_topLevelFolders[eFGI_Module].subFolders[(bIsGlobal ? "Global" : "Level")];
		}
	}
}


void CHyperGraphsTreeCtrl::OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode)
{
	if (m_bIgnoreReloads)
		return;

	switch (event)
	{
	case EHG_GRAPH_OWNER_CHANGE: // graph's type was set, or the entity, or the group name. In either case, the graph is moving folder
		if (static_cast<CHyperGraph*>(pGraph)->IsFlowGraph())
		{
			CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
			bool bIsCurrentlyInView = (m_pCurrentGraph == pFlowGraph);

			// delete current entry & check for empty folders to remove
			HTREEITEM hOldItem = pFlowGraph->GetHTreeItem();
			HTREEITEM hParentItem = GetNextItem(hOldItem, TVGN_PARENT);
			DeleteItem(hOldItem);
			DeleteEmptyParentFolders(hParentItem);

			// add new item
			AddGraphToTree(pFlowGraph, false);

			// re-select and expand the new item
			if (bIsCurrentlyInView)
			{
				CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
				HTREEITEM hItem = pFlowGraph->GetHTreeItem();
				if (hItem)
				{
					EnsureVisible(hItem);
					Select(hItem, TVGN_DROPHILITE);
				}
			}
		}
		break;
	case EHG_GRAPH_ADDED:
		{
			if (static_cast<CHyperGraph*>(pGraph)->IsFlowGraph())
			{
				CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
				AddGraphToTree(pFlowGraph, false);
			}
		}
		break;
	case EHG_GRAPH_MODIFIED:
		UpdateGraphLabel((CHyperGraph*)pGraph); // visually mark the graph as having changes
		break;
	//case EHG_GRAPH_REMOVED: the main dialog handles this event and calls DeleteGraphItem
	}


}


void CHyperGraphsTreeCtrl::RenameGraphItem(HTREEITEM hGraphItem, CString& newName)
{
	if (hGraphItem)
	{
		// rename the entry for this graph and sort the containing folder alphabetically
		SetItemText(hGraphItem, newName);
		HTREEITEM hParent = GetParentItem(hGraphItem);
		if (hParent)
		{
			SortChildrenWithFoldersFirst(hParent);
		}
	}
}


void CHyperGraphsTreeCtrl::RenameFolder(HTREEITEM hFolderItem, CString& newName)
{
	// assume it's not a top level folder. The option shouldn't show in the UI to allow that.

	SetItemText(hFolderItem, newName);
	HTREEITEM hParentItem = GetParentItem(hFolderItem);
	SortChildrenWithFoldersFirst(hParentItem);

	HTREEITEM hChildItem = GetNextItem(hFolderItem, TVGN_CHILD);
	while (hChildItem)
	{
		CHyperGraph* pGraph = (CHyperGraph*)GetItemData(hChildItem);
		if (pGraph)
		{
			if (pGraph->IsFlowGraph())
			{
				CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
				pFlowGraph->SetGroupName(newName);
			}
		}
		hChildItem = GetNextItem(hChildItem, TVGN_NEXT);
	}
}


void CHyperGraphsTreeCtrl::RenameFolderByName(HTREEITEM hParentItem, CString& folderName, CString& newName)
{
	HTREEITEM hItem = GetNextItem(hParentItem, TVGN_CHILD);
	while (hItem && !GetItemData(hItem)) // folders are first. search only that
	{
		if (folderName.Compare(GetItemText(hItem)) == 0)
		{
			RenameFolder(hItem, newName);
			return;
		}
		hItem = GetNextItem(hItem, TVGN_NEXT);
	}
}


void CHyperGraphsTreeCtrl::RenameParentFolder(HTREEITEM hChildItem, CString& newName)
{
	HTREEITEM hParentItem = GetParentItem(hChildItem);
	RenameFolder(hParentItem, newName);
}


bool CHyperGraphsTreeCtrl::DeleteGraphItem(CHyperGraph* pGraph)
{
	if (pGraph == nullptr)
		return false;

	HTREEITEM hItem = pGraph->GetHTreeItem();
	if (hItem)
	{
		HTREEITEM hParent = GetParentItem(hItem);
		if (DeleteItem(hItem))
		{
			pGraph->SetHTreeItem(NULL);

			DeleteEmptyParentFolders(hParent);
			return true;
		}
	}

	return false;
}


void CHyperGraphsTreeCtrl::PrepareLevelUnload()
{
	m_bIgnoreReloads = true;
	// TODO: explicitly delete level graphs?
}


void CHyperGraphsTreeCtrl::AddGraphItemToTree(bool isRebuild, CString graphName, CHyperFlowGraph* pFlowGraph, EFlowGraphImage eType, HTREEITEM hParentfolder /*= NULL*/)
{
	HTREEITEM hTopLevelfolder = m_topLevelFolders[eType].hItem;

	HTREEITEM hDefaultParentFolder = (hParentfolder) ? hParentfolder : hTopLevelfolder;  // default group is the top level folder
	HTREEITEM hGroup = hDefaultParentFolder;
	CString groupName(GetItemText(hDefaultParentFolder));
	bool bCreatedNewFolder = false;
	if(!pFlowGraph->GetGroupName().IsEmpty())
	{
		// if there is an actual group, check if it was already created when adding other item or if we do it now
		// limitation: group map is per the top level folder only, but at the moment nested hierarchies are unlikely
		hGroup = stl::find_in_map(m_topLevelFolders[eType].subFolders, pFlowGraph->GetGroupName(), NULL);
		if (!hGroup)
		{
			hGroup = InsertItem(pFlowGraph->GetGroupName(), eFGI_Folder, eFGI_Folder, hDefaultParentFolder, TVI_SORT);
			SetItemData(hGroup, DWORD_PTR(NULL));
			m_topLevelFolders[eType].subFolders[pFlowGraph->GetGroupName()] = hGroup;
			bCreatedNewFolder = true;
		}
	}

	if (pFlowGraph->HasError())
	{
		// propagate error icon up the folder hierarchy
		HTREEITEM hParentItem = hGroup;
		do {
			SetItemImage(hParentItem, eFGI_Folder + eFGI_ErrorOffset, eFGI_Folder + eFGI_ErrorOffset);
		} while (hParentItem = GetNextItem(hParentItem, TVGN_PARENT));
		SetItemImage(hTopLevelfolder, eType + eFGI_ErrorOffset, eType + eFGI_ErrorOffset);
	}

	// set icon for this item
	int img = pFlowGraph->IsEnabled() ? (eType + (pFlowGraph->HasError() ? eFGI_ErrorOffset : 0)) : eFGI_Disabled;
	SHGTCReloadItem item = SHGTCReloadItem(graphName, img, hGroup, pFlowGraph);
	if (isRebuild)
	{
		// queue it for bulk insertion (done later so that all folders are inserted first)
		items.push_back(item);
	}
	else
	{
		item.Add(this);
		SortChildrenWithFoldersFirst(hGroup);
		if (bCreatedNewFolder)
		{
			SortChildrenWithFoldersFirst(hDefaultParentFolder);
		}
	}
	
}


void CHyperGraphsTreeCtrl::AddGraphToTree(CHyperFlowGraph* pFlowGraph, bool isRebuild)
{
	CEntityObject* pEntity = pFlowGraph->GetEntity();
	IFlowGraph::EFlowGraphType type = pFlowGraph->GetIFlowGraph()->GetType();

	if (type == IFlowGraph::eFGT_Default && pEntity)
	{
		if (pEntity->GetGroup())
		{
			CGroup* pObjGroup = (CGroup*)pEntity->GetGroup();
			if (!pObjGroup)
				return;

			if (CPrefabObject* pPrefab = (CPrefabObject*)pEntity->GetPrefab())
			{
				if (pObjGroup->IsOpen())
				{
					const CString sPrefabInstanceName(pPrefab->GetName().c_str());
					HTREEITEM hGroup = stl::find_in_map(m_topLevelFolders[eFGI_Folder].subFolders, sPrefabInstanceName, NULL);
					if (!hGroup)
					{
						hGroup = InsertItem(sPrefabInstanceName, eFGI_Folder, eFGI_Folder, m_topLevelFolders[eFGI_Folder].hItem, TVI_SORT);
						m_topLevelFolders[eFGI_Folder].subFolders[sPrefabInstanceName] = hGroup;
						SortChildrenWithFoldersFirst(m_topLevelFolders[eFGI_Folder].hItem);
					}
					AddGraphItemToTree(isRebuild, pEntity->GetName().GetString(), pFlowGraph, eFGI_Folder, hGroup);
				}
			}
			else // entity inside a group
			{
				CString name;
				name.Format("[%s] %s", pObjGroup->GetName(), pEntity->GetName());
				AddGraphItemToTree(isRebuild, name, pFlowGraph, eFGI_Entity);
			}
		}
		else // normal entity
		{
			AddGraphItemToTree(isRebuild, pEntity->GetName().GetString(), pFlowGraph, eFGI_Entity);
		}
	}
	else if (type == IFlowGraph::eFGT_AIAction)
	{
		IAIAction* pAIAction = pFlowGraph->GetAIAction();
		if (!pAIAction || pAIAction->GetUserEntity() || pAIAction->GetObjectEntity())
			return;
		AddGraphItemToTree(isRebuild, pAIAction->GetName(), pFlowGraph, eFGI_AIAction);
	}
	else if (type == IFlowGraph::eFGT_UIAction)
	{
		IUIAction* pUIAction = pFlowGraph->GetUIAction();
		assert(pUIAction);
		if (!pUIAction) return;
		AddGraphItemToTree(isRebuild, pUIAction->GetName(), pFlowGraph, eFGI_UIAction);
	}
	else if (type == IFlowGraph::eFGT_CustomAction)
	{
		ICustomAction* pCustomAction = pFlowGraph->GetCustomAction();
		if (!pCustomAction || pCustomAction->GetObjectEntity())
			return;
		AddGraphItemToTree(isRebuild, pCustomAction->GetCustomActionGraphName(), pFlowGraph, eFGI_CustomAction);
	}
	else if (type == IFlowGraph::eFGT_Module)
	{
		if (IFlowGraphModule* pModule = gEnv->pFlowSystem->GetIModuleManager()->GetModule(pFlowGraph->GetName()))
		{
			IFlowGraphModule::EType moduleType = pModule->GetType();
			const bool bGlobal = (moduleType == IFlowGraphModule::eT_Global);
			AddGraphItemToTree(isRebuild, pFlowGraph->GetName(), pFlowGraph, eFGI_Module,
				bGlobal ? m_topLevelFolders[eFGI_Module].subFolders["Global"] : m_topLevelFolders[eFGI_Module].subFolders["Level"]);
		}
	}
	else if (type == IFlowGraph::eFGT_MaterialFx)
	{
		AddGraphItemToTree(isRebuild, pFlowGraph->GetName(), pFlowGraph, eFGI_MaterialFx);
	}
	else
	{
		AddGraphItemToTree(isRebuild, pFlowGraph->GetName(), pFlowGraph, eFGI_File);
	}
}


void CHyperGraphsTreeCtrl::FullReload()
{
	if (m_bIgnoreReloads)
		return;

	LOADING_TIME_PROFILE_SECTION;

	SetRedraw(FALSE);

	// nuke existing tree
	DeleteAllItems();

	// create top level folders - inserted in the tree in order
	if (GetISystem()->GetAISystem())
	{
		m_topLevelFolders[eFGI_AIAction] = STreeFolder(this, "AI Actions", eFGI_AIAction);
	}
	if (gEnv->pFlashUI)
	{
		m_topLevelFolders[eFGI_UIAction] = STreeFolder(this, "UI Actions", eFGI_UIAction);
	}
	if (gEnv->pGameFramework->GetICustomActionManager())
	{
		m_topLevelFolders[eFGI_CustomAction] = STreeFolder(this, "Custom Actions", eFGI_CustomAction);
	}
	m_topLevelFolders[eFGI_MaterialFx] = STreeFolder(this, "Material FX", eFGI_MaterialFx);
	m_topLevelFolders[eFGI_Module] = STreeFolder(this, "FG Modules", eFGI_Module);
	m_topLevelFolders[eFGI_Entity] = STreeFolder(this, "Entities", eFGI_Entity);
	m_topLevelFolders[eFGI_Folder] = STreeFolder(this, "Prefabs", eFGI_Folder);
	m_topLevelFolders[eFGI_File] = STreeFolder(this, "Files", eFGI_File);

	HTREEITEM hModules = m_topLevelFolders[eFGI_Module].hItem;
	m_topLevelFolders[eFGI_Module].subFolders["Global"] = InsertItem("Global", eFGI_Folder, eFGI_Folder, hModules);
	m_topLevelFolders[eFGI_Module].subFolders["Level"] = InsertItem("Level", eFGI_Folder, eFGI_Folder, hModules);

	// populate
	CFlowGraphManager* const pMgr = GetIEditorImpl()->GetFlowGraphManager();
	CHyperFlowGraph* pFlowGraph = nullptr;
	size_t numFGraphs = pMgr->GetFlowGraphCount();
	for (size_t i = 0; i < numFGraphs; ++i)
	{
		pFlowGraph = pMgr->GetFlowGraph(i);
		AddGraphToTree(pFlowGraph, true);
	}

	// sort all the items and add them to the tree in this order, after the folders were already inserted
	std::sort(items.begin(), items.end());
	for (size_t i = 0; i < items.size(); ++i)
	{
		HTREEITEM hItem = items[i].Add(this);
		if (m_pCurrentGraph && (m_pCurrentGraph == items[i].pFlowGraph))
		{
			SelectItem(hItem);
		}
	}
	items.clear();

	SetRedraw(TRUE);
}


void CHyperGraphsTreeCtrl::DeleteEmptyParentFolders(HTREEITEM hFirstParent)
{
	HTREEITEM hParent = hFirstParent;
	while (hParent && !ItemHasChildren(hParent)) // hParent is an empty folder
	{
		if (GetParentItem(hParent) && GetParentItem(hParent) != m_topLevelFolders[eFGI_Module].hItem) // hParent is not a top level folder
		{
			HTREEITEM hNextParent = GetParentItem(hParent);
			CString folderName = GetItemText(hParent);
			for (STreeFolder topFolder : m_topLevelFolders)
			{
				topFolder.subFolders.erase(folderName);
			}
			DeleteItem(hParent);

			hParent = hNextParent;
		}
		else // reached top level. we're done
			return;
	}
}


