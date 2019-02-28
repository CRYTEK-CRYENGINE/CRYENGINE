// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LensFlareEditor.h"
#include "LensFlareView.h"
#include "LensFlareAtomicList.h"
#include "LensFlareElementTree.h"
#include "LensFlareManager.h"
#include "LensFlareItem.h"
#include "LensFlareLibrary.h"
#include "LensFlareUndo.h"
#include "LensFlareLightEntityTree.h"
#include "LensFlareReferenceTree.h"
#include "Controls/PropertyCtrl.h"
#include "Objects/EntityObject.h"
#include "Util/Clipboard.h"
#include "Dialogs/QGroupDialog.h"

CLensFlareEditor* CLensFlareEditor::s_pLensFlareEditor = NULL;
const char* CLensFlareEditor::s_pLensFlareEditorClassName = "Lens Flare Editor";

#define DISABLE_REFERENCETREE

static const CRect kDefaultRect(0, 0, 600, 600);

IMPLEMENT_DYNCREATE(CLensFlareEditor, CDatabaseFrameWnd)

BEGIN_MESSAGE_MAP(CLensFlareEditor, CDatabaseFrameWnd)
ON_COMMAND(ID_DB_ADD, OnAddItem)
ON_COMMAND(ID_DB_COPY_NAME_TO_CLIPBOARD, OnCopyNameToClipboard)
ON_COMMAND(ID_DB_ASSIGNTOSELECTION, OnAssignFlareToLightEntities)
ON_COMMAND(ID_DB_SELECTASSIGNEDOBJECTS, OnSelectAssignedObjects)
ON_COMMAND(ID_DB_GETFROMSELECTION, OnGetFlareFromSelection)
ON_NOTIFY(TVN_BEGINLABELEDIT, IDC_LIBRARY_ITEMS_TREE, OnTvnBeginlabeleditTree)
ON_NOTIFY(TVN_ENDLABELEDIT, IDC_LIBRARY_ITEMS_TREE, OnTvnEndlabeleditTree)
ON_NOTIFY(TVN_KEYDOWN, IDC_LIBRARY_ITEMS_TREE, OnTvnKeydownTree)
ON_NOTIFY(TVN_SELCHANGED, IDC_LIBRARY_ITEMS_TREE, OnTvnItemSelChanged)
ON_WM_SIZE()
ON_MESSAGE(WM_FLAREEDITOR_UPDATETREECONTROL, OnUpdateTreeCtrl)
ON_NOTIFY(NM_RCLICK, IDC_LIBRARY_ITEMS_TREE, OnNotifyTreeRClick)
END_MESSAGE_MAP()

enum
{
	IDW_LENSFLARE_TREE_PANE = AFX_IDW_CONTROLBAR_FIRST + 1,
	IDW_LENSFLARE_ATOMICLIST_PANE,
	IDW_LENSFLARE_PREVIEW_PANE,
	IDW_LENSFLARE_ELEMENTTREE_PANE,
	IDW_LENSFLARE_PROPERTY_PANE,
	IDW_LENSFLARE_LIGHTENTITYTREE_PANE,
	IDW_LENSFLARE_REFERENCETREE_PANE
};

static const char* LENS_FLARES_CLASSNAME("LENS_FLARE_EDITOR");

CLensFlareEditor::CLensFlareEditor() :
	m_pWndProps(NULL),
	m_pLensFlareAtomicList(NULL),
	m_pLensFlareView(NULL),
	m_pLensFlareElementTree(NULL),
	m_pLensFlareLightEntityTree(NULL),
	m_pLensFlareReferenceTree(NULL)
{
	assert(!s_pLensFlareEditor);
	s_pLensFlareEditor = this;
	m_pItemManager = GetIEditorImpl()->GetLensFlareManager();

	CRect rc(0, 0, 0, 0);
	BOOL bRes = Create(WS_CHILD, rc, AfxGetMainWnd());
	if (!bRes)
		return;
	ASSERT(bRes);
}

CLensFlareEditor::~CLensFlareEditor()
{
	if (m_pLensFlareElementTree)
	{
		m_pLensFlareElementTree->UnregisterListener(this);
		m_pLensFlareElementTree->UnregisterListener(m_pLensFlareView);
	}

	ReleaseWindowsToBePutIntoPanels();
	s_pLensFlareEditor = NULL;
}

BOOL CLensFlareEditor::OnInitDialog()
{
	GetDockingPaneManager()->HideClient(TRUE);

	InitToolBars();
	InitTreeCtrl();

	CreateWindowsToBePutIntoPanels();

	if (!AutoLoadFrameLayout("LensFlare", 11))
		CreatePanes();
	AttachWindowsToPanes();

	m_pLensFlareAtomicList->FillAtomicItems();

	ReloadLibs();

	return TRUE;
}

void CLensFlareEditor::CreatePanes()
{
	const CSize basicMinimumSize(16, 16);
	CXTPDockingPane* pElementTreePane = CreatePane(_T("Element Tree"), basicMinimumSize, IDW_LENSFLARE_ELEMENTTREE_PANE, kDefaultRect, xtpPaneDockBottom);
	CXTPDockingPane* pViewPane = CreatePane(_T("Preview"), basicMinimumSize, IDW_LENSFLARE_PREVIEW_PANE, kDefaultRect, xtpPaneDockTop, pElementTreePane);
	CXTPDockingPane* pBasicSetPane = CreatePane(_T("Basic Set"), basicMinimumSize, IDW_LENSFLARE_ATOMICLIST_PANE, kDefaultRect, xtpPaneDockLeft, pElementTreePane);
#ifndef DISABLE_REFERENCETREE
	CXTPDockingPane* pReferencePane = CreatePane(_T("Reference Tree"), basicMinimumSize, IDW_LENSFLARE_REFERENCETREE_PANE, kDefaultRect, xtpPaneDockRight, pBasicSetPane);
#endif
	CXTPDockingPane* pLensFlareTreePane = CreatePane(_T("Lens Flare Tree"), basicMinimumSize, IDW_LENSFLARE_TREE_PANE, CRect(0, 0, 200, 600), xtpPaneDockLeft);
	CXTPDockingPane* pPropertyPane = CreatePane(_T("Properties"), basicMinimumSize, IDW_LENSFLARE_PROPERTY_PANE, CRect(0, 0, 200, 600), xtpPaneDockRight);
	CXTPDockingPane* pLensFlareEntityTreePane = CreatePane(_T("Light Entities"), basicMinimumSize, IDW_LENSFLARE_LIGHTENTITYTREE_PANE, CRect(0, 0, 200, 200), xtpPaneDockBottom, pPropertyPane);
}

CXTPDockingPane* CLensFlareEditor::CreatePane(const string& name, const CSize& minSize, UINT nID, CRect rc, XTPDockingPaneDirection direction, CXTPDockingPaneBase* pNeighbour)
{
	CXTPDockingPane* pPane = GetDockingPaneManager()->CreatePane(nID, rc, direction, pNeighbour);
	pPane->SetTitle(name);
	pPane->SetOptions(xtpPaneNoCloseable);
	pPane->SetMinTrackSize(minSize);
	return pPane;
}

void CLensFlareEditor::CreateWindowsToBePutIntoPanels()
{
	ReleaseWindowsToBePutIntoPanels();

	m_pWndProps = new CPropertyCtrl;
	m_pLensFlareAtomicList = new CLensFlareAtomicList;
	m_pLensFlareView = new CLensFlareView;
	m_pLensFlareElementTree = new CLensFlareElementTree;
	m_pLensFlareLightEntityTree = new CLensFlareLightEntityTree;
#ifndef DISABLE_REFERENCETREE
	m_pLensFlareReferenceTree = new CLensFlareReferenceTree;
#endif

	m_pLensFlareElementTree->RegisterListener(this);
	m_pLensFlareElementTree->RegisterListener(m_pLensFlareView);

	DWORD dwCommonTreeStyle = WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_BORDER | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | TVS_LINESATROOT | TVS_HASLINES | TVS_EDITLABELS |
	                          TVS_FULLROWSELECT | TVS_NOHSCROLL | TVS_CHECKBOXES | TVS_INFOTIP;

	m_pLensFlareAtomicList->Create(WS_VISIBLE | WS_TABSTOP | WS_CHILD | WS_BORDER, kDefaultRect, this, 0x1006);
	m_pLensFlareElementTree->Create(dwCommonTreeStyle, kDefaultRect, this, 0x1007);
	m_pLensFlareLightEntityTree->Create(dwCommonTreeStyle, kDefaultRect, this, 0x1008);
#ifndef DISABLE_REFERENCETREE
	m_pLensFlareReferenceTree->Create(dwCommonTreeStyle, kDefaultRect, this, 0x1009);
#endif
	m_pWndProps->Create(WS_CHILD, kDefaultRect, this);
	m_pWndProps->SetItemHeight(16);
	m_pWndProps->ExpandAll();
	m_pWndProps->SetUpdateCallback(functor(*this, &CLensFlareEditor::OnUpdateProperties));
	m_pWndProps->SetCallbackOnNonModified(false);

	m_pLensFlareView->Create(this, kDefaultRect, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, IDW_LENSFLARE_PREVIEW_PANE);
}

void CLensFlareEditor::ReleaseWindowsToBePutIntoPanels()
{
	SAFE_DELETE(m_pWndProps);
	SAFE_DELETE(m_pLensFlareAtomicList);
	SAFE_DELETE(m_pLensFlareView);
	SAFE_DELETE(m_pLensFlareElementTree);
	SAFE_DELETE(m_pLensFlareLightEntityTree);
#ifndef DISABLE_REFERENCETREE
	SAFE_DELETE(m_pLensFlareReferenceTree);
#endif
}

void CLensFlareEditor::AttachWindowsToPanes()
{
	AttachWindowToPane(IDW_LENSFLARE_ELEMENTTREE_PANE, m_pLensFlareElementTree);
	AttachWindowToPane(IDW_LENSFLARE_TREE_PANE, &GetTreeCtrl());
	AttachWindowToPane(IDW_LENSFLARE_ATOMICLIST_PANE, m_pLensFlareAtomicList);
#ifndef DISABLE_REFERENCETREE
	AttachWindowToPane(IDW_LENSFLARE_REFERENCETREE_PANE, m_pLensFlareReferenceTree);
#endif
	AttachWindowToPane(IDW_LENSFLARE_PROPERTY_PANE, m_pWndProps);
	AttachWindowToPane(IDW_LENSFLARE_PREVIEW_PANE, m_pLensFlareView);
	AttachWindowToPane(IDW_LENSFLARE_LIGHTENTITYTREE_PANE, m_pLensFlareLightEntityTree);
}

void CLensFlareEditor::AttachWindowToPane(UINT nID, CWnd* pWin)
{
	CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(nID);
	if (pPane)
		pPane->Attach(pWin);
}

void CLensFlareEditor::DoDataExchange(CDataExchange* pDX)
{
}

class CLensFlareEditorClass : public IViewPaneClass
{
	virtual ESystemClassID SystemClassID()   override { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       override { return CLensFlareEditor::s_pLensFlareEditorClassName; };
	virtual const char*    Category()        override { return "Database"; };
	virtual const char*    GetMenuPath()     override { return ""; }
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CLensFlareEditor); };
	virtual const char*    GetPaneTitle()    override { return CLensFlareEditor::s_pLensFlareEditorClassName; };
	virtual bool           SinglePane()      override { return true; };
};

REGISTER_CLASS_DESC(CLensFlareEditorClass)

CLensFlareItem * CLensFlareEditor::GetSelectedLensFlareItem() const
{
	return GetLensFlareItem(GetTreeCtrl().GetSelectedItem());
}

CLensFlareItem* CLensFlareEditor::GetLensFlareItem(HTREEITEM hItem) const
{
	if (!hItem)
		return NULL;
	if (GetTreeCtrl().ItemHasChildren(hItem))
		return NULL;
	return (CLensFlareItem*)GetTreeCtrl().GetItemData(hItem);

}

void CLensFlareEditor::ResetElementTreeControl()
{
	m_pLensFlareElementTree->InvalidateLensFlareItem();
	m_pLensFlareElementTree->DeleteAllItems();
}

void CLensFlareEditor::OnTvnItemSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

	SelectLensFlareItem(pNMTreeView->itemNew.hItem, pNMTreeView->itemOld.hItem);
	UpdateLensFlareItem(GetSelectedLensFlareItem());

	*pResult = 0;
}

void CLensFlareEditor::SelectLensFlareItem(HTREEITEM hItem, HTREEITEM hPrevItem)
{
	if (hPrevItem && CUndo::IsRecording())
	{
		string fullItemName;
		if (GetFullLensFlareItemName(hPrevItem, fullItemName))
			CUndo::Record(new CUndoLensFlareItemSelectionChange(fullItemName));
	}
	m_pWndProps->DeleteAllItems();
	if (hItem == NULL)
		return;
	HTREEITEM hParentItem = GetTreeCtrl().GetParentItem(hItem);
	if (hParentItem)
		GetTreeCtrl().Expand(hParentItem, TVE_EXPAND);
	GetTreeCtrl().SelectItem(hItem);
	CLensFlareItem* pSelectedLensFlareItem = GetSelectedLensFlareItem();
	SelectItem(pSelectedLensFlareItem);
}

void CLensFlareEditor::SelectLensFlareItem(const string& fullItemName)
{
	HTREEITEM hNewItem = FindLensFlareItemByFullName(fullItemName);
	if (hNewItem == NULL)
		return;
	SelectLensFlareItem(hNewItem, GetTreeCtrl().GetSelectedItem());
}

HTREEITEM CLensFlareEditor::FindLensFlareItemByFullName(const string& fullName) const
{
	for (int i = 0, iItemSize(m_AllLensFlareTreeItems.size()); i < iItemSize; ++i)
	{
		string comparedFullName;
		if (!GetFullLensFlareItemName(m_AllLensFlareTreeItems[i], comparedFullName))
			continue;
		if (comparedFullName == fullName)
			return m_AllLensFlareTreeItems[i];
	}
	return NULL;
}

void CLensFlareEditor::RemovePropertyItems()
{
	m_pWndProps->DeleteAllItems();
}

bool CLensFlareEditor::IsExistTreeItem(const string& name, bool bExclusiveSelectedItem)
{
	for (int i = 0, iSize(m_AllLensFlareTreeItems.size()); i < iSize; ++i)
	{
		HTREEITEM hItem = m_AllLensFlareTreeItems[i];
		if (hItem == NULL)
			continue;
		if (!bExclusiveSelectedItem || hItem != GetTreeCtrl().GetSelectedItem())
		{
			string itemFullName;
			if (GetNameFromTreeItem(hItem, itemFullName))
			{
				if (itemFullName == name)
					return true;
			}
		}
	}

	return false;
}

void CLensFlareEditor::RenameLensFlareItem(CLensFlareItem* pLensFlareItem, const string& newGroupName, const string& newShortName)
{
	if (pLensFlareItem == NULL || newShortName.IsEmpty() || newGroupName.IsEmpty())
		return;

	string oldFullName = pLensFlareItem->GetFullName();
	SetItemName(pLensFlareItem, newGroupName, newShortName);

	HTREEITEM hItem = GetLensFlareTreeItem(pLensFlareItem);
	if (hItem)
		GetTreeCtrl().SetItemText(hItem, newShortName);

	UpdateLensOpticsNames(oldFullName, pLensFlareItem->GetFullName());

	pLensFlareItem->SetModified();
}

IOpticsElementBasePtr CLensFlareEditor::FindOptics(const string& itemPath, const string& opticsPath)
{
	CLensFlareItem* pItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(itemPath);
	if (pItem == NULL)
		return NULL;

	IOpticsElementBasePtr pOptics = pItem->GetOptics();
	if (pOptics == NULL)
		return NULL;

	return LensFlareUtil::FindOptics(pOptics, opticsPath);
}

void CLensFlareEditor::UpdateLensOpticsNames(const string& oldFullName, const string& newFullName)
{
	std::vector<CBaseObject*> pEntityObjects;
	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CEntityObject), pEntityObjects);
	for (int i = 0, iObjectSize(pEntityObjects.size()); i < iObjectSize; ++i)
	{
		CEntityObject* pEntity = (CEntityObject*)pEntityObjects[i];
		if (pEntity == NULL)
			continue;
		if (pEntity->CheckFlags(OBJFLAG_DELETED))
			continue;
		if (!pEntity->IsLight())
			continue;
		if (oldFullName == pEntity->GetEntityPropertyString(CEntityObject::s_LensFlarePropertyName))
			pEntity->SetOpticsName(newFullName);
	}
}

bool CLensFlareEditor::GetNameFromTreeItem(HTREEITEM hItem, string& outName) const
{
	if (hItem == NULL)
		return false;

	if (hItem == TVI_ROOT)
	{
		outName = "";
		return true;
	}

	while (hItem)
	{
		outName.Insert(0, GetTreeCtrl().GetItemText(hItem).GetString());
		hItem = GetTreeCtrl().GetParentItem(hItem);
		if (hItem)
			outName.Insert(0, ".");
	}

	return true;
}

HTREEITEM CLensFlareEditor::GetLensFlareTreeItem(CLensFlareItem* pItem) const
{
	ItemsToTreeMap::const_iterator iItem = m_itemsToTree.find(pItem);
	if (iItem != m_itemsToTree.end())
		return iItem->second;
	return NULL;
}

void CLensFlareEditor::OnCopy()
{
	UpdateClipboard(FLARECLIPBOARDTYPE_COPY);
}

void CLensFlareEditor::OnPaste()
{
	if (!m_pLibrary)
		return;

	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;

	XmlNodeRef clipboardXML = clipboard.Get();
	if (clipboardXML == NULL)
		return;

	Paste(clipboardXML);
}

void CLensFlareEditor::Paste(XmlNodeRef node)
{
	if (!m_pLibrary || node == NULL)
		return;

	string type;
	node->getAttr("Type", type);

	string sourceGroupName;
	node->getAttr("GroupName", sourceGroupName);

	if (m_pLibrary == NULL)
		return;

	HTREEITEM hSelectedTreeItem = GetTreeCtrl().GetSelectedItem();
	if (hSelectedTreeItem == NULL)
		return;

	bool bShouldCreateNewGroup(false);
	if (!sourceGroupName.IsEmpty())
	{
		QDialogButtonBox::StandardButton nAnswer = CQuestionDialog::SQuestion("Question", "Do you want to create a new group(YES) or add to the selected group(NO)?", QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
		if (nAnswer == QDialogButtonBox::Cancel)
			return;
		bShouldCreateNewGroup = nAnswer == QDialogButtonBox::Yes;
	}

	string targetGroupName;

	if (bShouldCreateNewGroup)
	{
		targetGroupName = MakeValidName("NewGroup", functor(*this, &CDatabaseFrameWnd::DoesGroupExist));
	}
	else
	{
		HTREEITEM hTargetItem = NULL;
		if (!GetTreeCtrl().ItemHasChildren(hSelectedTreeItem))
		{
			HTREEITEM hParentTreeItem = GetTreeCtrl().GetParentItem(hSelectedTreeItem);
			if (hParentTreeItem == NULL)
				hTargetItem = TVI_ROOT;
			else
				hTargetItem = hParentTreeItem;
		}
		else
		{
			hTargetItem = hSelectedTreeItem;
		}

		if (hTargetItem == NULL)
			return;

		if (!GetNameFromTreeItem(hTargetItem, targetGroupName))
			return;
	}

	CUndo undo("Copy/Cut & Paste for Lens Flare");
	CLensFlareItem* pNewItem = NULL;

	for (int i = 0, iChildCount(node->getChildCount()); i < iChildCount; ++i)
	{
		LensFlareUtil::SClipboardData clipboardData;
		clipboardData.FillThisFromXmlNode(node->getChild(i));

		_smart_ptr<CLensFlareItem> pSourceItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(clipboardData.m_LensFlareFullPath);
		if (pSourceItem == NULL)
			continue;

		IOpticsElementBasePtr pSourceOptics = FindOptics(clipboardData.m_LensFlareFullPath, clipboardData.m_LensOpticsPath);
		if (pSourceOptics == NULL)
			continue;

		if (type == FLARECLIPBOARDTYPE_CUT)
		{
			if (clipboardData.m_From == LENSFLARE_ELEMENT_TREE)
				LensFlareUtil::RemoveOptics(pSourceOptics);
			else
				DeleteItem(pSourceItem);
		}

		string sourceName;
		if (clipboardData.m_From == LENSFLARE_ELEMENT_TREE)
			sourceName = LensFlareUtil::GetShortName(pSourceOptics->GetName());
		else
			sourceName = pSourceItem->GetShortName();

		string candidateName = targetGroupName + string(".") + sourceName;
		string validName = MakeValidName(candidateName, functor(*this, &CDatabaseFrameWnd::DoesItemExist));
		string validShortName = LensFlareUtil::GetShortName(validName);
		pNewItem = AddNewLensFlareItem(targetGroupName, validShortName);
		assert(pNewItem);
		if (pNewItem == NULL)
			continue;

		if (LensFlareUtil::IsGroup(pSourceOptics->GetType()))
		{
			LensFlareUtil::CopyOptics(pSourceOptics, pNewItem->GetOptics(), true);
		}
		else
		{
			IOpticsElementBasePtr pNewOptics = LensFlareUtil::CreateOptics(pSourceOptics);
			if (pNewOptics)
				pNewItem->GetOptics()->AddElement(pNewOptics);
		}

		if (type == FLARECLIPBOARDTYPE_CUT)
		{
			if (clipboardData.m_From == LENSFLARE_ITEM_TREE)
				UpdateLensOpticsNames(pSourceItem->GetFullName(), pNewItem->GetFullName());
			if (pSourceItem != GetSelectedLensFlareItem())
				pSourceItem->UpdateLights();
		}

		LensFlareUtil::UpdateOpticsName(pNewItem->GetOptics());
		LensFlareUtil::ChangeOpticsRootName(pNewItem->GetOptics(), validShortName);

		ReloadItems();
		SelectLensFlareItem(GetTreeLensFlareItem(pNewItem), NULL);
	}
}

void CLensFlareEditor::OnCut()
{
	UpdateClipboard(FLARECLIPBOARDTYPE_CUT);
}

XmlNodeRef CLensFlareEditor::CreateXML(const char* type) const
{
	std::vector<LensFlareUtil::SClipboardData> clipboardDataList;
	string groupName;
	if (!GetClipboardDataList(clipboardDataList, groupName))
		return NULL;
	return LensFlareUtil::CreateXMLFromClipboardData(type, groupName, false, clipboardDataList);
}

void CLensFlareEditor::UpdateClipboard(const char* type) const
{
	std::vector<LensFlareUtil::SClipboardData> clipboardDataList;
	string groupName;
	if (!GetClipboardDataList(clipboardDataList, groupName))
		return;
	LensFlareUtil::UpdateClipboard(type, groupName, false, clipboardDataList);
}

bool CLensFlareEditor::GetClipboardDataList(std::vector<LensFlareUtil::SClipboardData>& outList, string& outGroupName) const
{
	CLensFlareLibrary* pLibrary = GetCurrentLibrary();
	if (pLibrary == NULL)
		return false;

	std::vector<LensFlareUtil::SClipboardData> clipboardDataList;

	CLensFlareItem* pLensFlareItem = GetSelectedLensFlareItem();
	if (pLensFlareItem)
	{
		clipboardDataList.push_back(LensFlareUtil::SClipboardData(LENSFLARE_ITEM_TREE, pLensFlareItem->GetFullName(), pLensFlareItem->GetShortName()));
	}
	else if (GetSelectedItemStatus() == eSIS_Group)
	{
		HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();
		if (hSelectedItem == NULL)
			return false;
		outGroupName = GetTreeCtrl().GetItemText(hSelectedItem);
		HTREEITEM hChildItem = GetTreeCtrl().GetChildItem(hSelectedItem);
		while (hChildItem)
		{
			pLensFlareItem = GetLensFlareItem(hChildItem);
			if (pLensFlareItem)
				clipboardDataList.push_back(LensFlareUtil::SClipboardData(LENSFLARE_ITEM_TREE, pLensFlareItem->GetFullName(), pLensFlareItem->GetShortName()));
			hChildItem = GetTreeCtrl().GetNextSiblingItem(hChildItem);
		}
	}
	else
	{
		return false;
	}

	outList = clipboardDataList;
	return true;
}

void CLensFlareEditor::OnAddItem()
{
	if (!m_pLibrary)
		return;

	QGroupDialog dlg(QObject::tr("New Flare Name"));
	dlg.SetGroup(m_selectedGroup);

	if (dlg.exec() != QDialog::Accepted || dlg.GetString().IsEmpty())
	{
		return;
	}

	string fullName = m_pItemManager->MakeFullItemName(m_pLibrary, dlg.GetGroup().GetString(), dlg.GetString().GetString());
	if (m_pItemManager->FindItemByName(fullName))
	{
		Warning("Item with name %s already exist", (const char*)fullName);
		return;
	}

	CUndo undo("Add flare library item");

	CLensFlareItem* pNewLensFlare = AddNewLensFlareItem(dlg.GetGroup().GetString(), dlg.GetString().GetString());
	if (pNewLensFlare)
	{
		ReloadItems();
		SelectItem(pNewLensFlare);
	}
}

CLensFlareItem* CLensFlareEditor::AddNewLensFlareItem(const string& groupName, const string& shortName)
{
	CLensFlareItem* pNewFlare = (CLensFlareItem*)m_pItemManager->CreateItem(m_pLibrary);
	if (pNewFlare == NULL)
		return NULL;
	if (!SetItemName(pNewFlare, groupName, shortName))
	{
		m_pItemManager->DeleteItem(pNewFlare);
		return NULL;
	}
	pNewFlare->GetOptics()->SetName(pNewFlare->GetShortName());
	return pNewFlare;
}

HTREEITEM CLensFlareEditor::GetTreeLensFlareItem(CLensFlareItem* pItem) const
{
	ItemsToTreeMap::const_iterator iItem = m_itemsToTree.find(pItem);
	if (iItem != m_itemsToTree.end())
		return iItem->second;
	return NULL;
}

void CLensFlareEditor::OnCopyNameToClipboard()
{
	HTREEITEM hSelectedTreeItem = GetTreeCtrl().GetSelectedItem();
	if (hSelectedTreeItem == NULL)
		return;
	string itemName;
	if (GetNameFromTreeItem(hSelectedTreeItem, itemName) == false)
		return;
	CClipboard clipboard;
	clipboard.PutString(itemName);
}

void CLensFlareEditor::OnAssignFlareToLightEntities()
{
	CLensFlareItem* pLensFlareItem = GetSelectedLensFlareItem();
	if (pLensFlareItem == NULL)
		return;
	std::vector<CEntityObject*> entityList;
	LensFlareUtil::GetSelectedLightEntities(entityList);

	for (int i = 0, iEntityCount(entityList.size()); i < iEntityCount; ++i)
		entityList[i]->ApplyOptics(pLensFlareItem->GetFullName(), pLensFlareItem->GetOptics());

	LensFlareUtil::ApplyOpticsToSelectedEntityWithComponent(pLensFlareItem->GetFullName(), pLensFlareItem->GetOptics());

	m_pLensFlareLightEntityTree->OnLensFlareChangeItem(pLensFlareItem);
}

void CLensFlareEditor::OnSelectAssignedObjects()
{
	std::vector<CEntityObject*> lightEntities;
	LensFlareUtil::GetLightEntityObjects(lightEntities);

	CLensFlareItem* pLensFlareItem = GetSelectedLensFlareItem();
	if (pLensFlareItem == NULL)
		return;

	int iLightSize(lightEntities.size());
	std::vector<CBaseObject*> assignedObjects;
	assignedObjects.reserve(iLightSize);

	for (int i = 0; i < iLightSize; ++i)
	{
		CEntityObject* pLightEntity = lightEntities[i];
		if (pLightEntity == NULL)
			continue;

		IOpticsElementBasePtr pTargetOptics = pLightEntity->GetOpticsElement();
		if (pTargetOptics == NULL)
			continue;

		if (stricmp(pLensFlareItem->GetFullName(), pTargetOptics->GetName()))
			continue;

		assignedObjects.push_back(pLightEntity);
	}

	if (assignedObjects.empty())
		return;

	GetIEditorImpl()->ClearSelection();
	for (int i = 0, iAssignedObjectsSize(assignedObjects.size()); i < iAssignedObjectsSize; ++i)
		GetIEditorImpl()->SelectObject(assignedObjects[i]);
}

void CLensFlareEditor::OnGetFlareFromSelection()
{
	CEntityObject* pEntity = LensFlareUtil::GetSelectedLightEntity();
	if (pEntity == NULL)
	{
		MessageBox(_T("Only one light entity need to be selected."), _T("Warning"), MB_OK);
		return;
	}
	if (m_pLibrary == NULL)
		return;

	string fullFlareName = pEntity->GetEntityPropertyString(CEntityObject::s_LensFlarePropertyName);
	int nDotPosition = fullFlareName.Find(".");
	if (nDotPosition == -1)
		return;

	string libraryName = fullFlareName.Left(nDotPosition);
	if (m_pLibrary->GetName() != libraryName)
	{
		GetIEditorImpl()->GetLensFlareManager()->LoadItemByName(fullFlareName);
		SelectLibrary(libraryName);
	}

	ExpandTreeCtrl();
	SelectItemByName(fullFlareName);
}

void CLensFlareEditor::OnTvnBeginlabeleditTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
	HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();

	if (hItem == GetRootItem())
	{
		*pResult = 1;
		return;
	}

	// Mark message as handled and suppress default handling
	*pResult = 0;
}

void CLensFlareEditor::OnTvnEndlabeleditTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
	HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();

	if (hItem == NULL)
		return;

	if (pTVDispInfo->item.pszText == NULL)
		return;

	string fullItemName;
	HTREEITEM hParentItem = GetTreeCtrl().GetParentItem(pTVDispInfo->item.hItem);
	if (hParentItem == NULL)
	{
		fullItemName = pTVDispInfo->item.pszText;
	}
	else if (GetNameFromTreeItem(hParentItem, fullItemName))
	{
		fullItemName += ".";
		fullItemName += pTVDispInfo->item.pszText;
	}
	else
	{
		MessageBox("Fail to generate full name from a tree item", "Warning", MB_OK);
		StartEditItem(hItem);
		*pResult = 0;
		return;
	}

	if (IsExistTreeItem(fullItemName))
	{
		MessageBox("The identical name exists.", "Warning", MB_OK);
		StartEditItem(hItem);
	}
	else if (strstr(pTVDispInfo->item.pszText, "."))
	{
		MessageBox("The name must not contain \".\"", "Warning", MB_OK);
		StartEditItem(hItem);
	}
	else
	{
		CUndo undo("Rename FlareGroupItem");
		if (GetTreeCtrl().ItemHasChildren(pTVDispInfo->item.hItem))
		{
			ChangeGroupName(pTVDispInfo->item.hItem, pTVDispInfo->item.pszText);
		}
		else
		{
			CLensFlareItem* pCurrentGroupItem = GetSelectedLensFlareItem();
			if (pCurrentGroupItem)
			{
				RenameLensFlareItem(pCurrentGroupItem, pCurrentGroupItem->GetGroupName(), pTVDispInfo->item.pszText);
				LensFlareUtil::ChangeOpticsRootName(pCurrentGroupItem->GetOptics(), pTVDispInfo->item.pszText);
				UpdateLensFlareItem(pCurrentGroupItem);
			}
		}
		GetIEditorImpl()->SetModifiedFlag();
	}

	// Mark message as handled and suppress default handling
	*pResult = 0;
}

void CLensFlareEditor::ChangeGroupName(HTREEITEM hItem, const string& newGroupName)
{
	if (hItem == NULL || newGroupName.IsEmpty())
		return;

	bool bRenamed = false;
	string fullGroupName;
	if (!GetFullLensFlareItemName(hItem, fullGroupName))
		return;

	string fullChangedGroupName(fullGroupName);
	int nPos = fullChangedGroupName.ReverseFind('.');
	if (nPos == -1)
		fullChangedGroupName = newGroupName + ".";
	else
		fullChangedGroupName = fullChangedGroupName.Left(nPos + 1) + newGroupName + ".";

	fullGroupName += ".";

	CLensFlareLibrary* pLensFlareLibrary = GetCurrentLibrary();
	if (!pLensFlareLibrary)
		return;

	std::vector<CLensFlareItem*> ItemsWillBeChanged;
	for (int i = 0, iItemCount(pLensFlareLibrary->GetItemCount()); i < iItemCount; ++i)
	{
		CLensFlareItem* pItem = (CLensFlareItem*)pLensFlareLibrary->GetItem(i);
		if (pItem == NULL)
			continue;
		if (pItem->GetName().Find(fullGroupName) == 0)
			ItemsWillBeChanged.push_back(pItem);
	}

	for (int i = 0, iItemCount(ItemsWillBeChanged.size()); i < iItemCount; ++i)
	{
		CLensFlareItem* pItem = ItemsWillBeChanged[i];

		string oldItemFullName = pItem->GetFullName();
		string newItemName = pItem->GetName();
		newItemName.Replace(fullGroupName, fullChangedGroupName);

		if (i == 0)
			pItem->SetName(newItemName, true, false);
		else if (i == iItemCount - 1)
			pItem->SetName(newItemName, false, true);
		else
			pItem->SetName(newItemName);

		newItemName.Insert(0, ".");
		newItemName.Insert(0, pLensFlareLibrary->GetName());

		UpdateLensOpticsNames(oldItemFullName, newItemName);
	}

	GetTreeCtrl().SetItemText(hItem, newGroupName);
	GetIEditorImpl()->GetLensFlareManager()->Modified();
}

void CLensFlareEditor::OnTvnKeydownTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTVKEYDOWN pTVKeyDown = reinterpret_cast<LPNMTVKEYDOWN>(pNMHDR);

	if (pTVKeyDown->wVKey == VK_F2)
	{
		OnRenameItem();
	}
	else if (pTVKeyDown->wVKey == VK_DELETE)
	{
		OnRemoveItem();
	}

	// Mark message as handled and suppress default handling
	*pResult = 0;
}

LRESULT CLensFlareEditor::OnUpdateTreeCtrl(WPARAM wp, LPARAM lp)
{
	OnGetFlareFromSelection();
	return S_OK;
}

void CLensFlareEditor::OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CPoint point;
	CLensFlareItem* pLensFlareItem = 0;

	HTREEITEM hItem = LensFlareUtil::GetTreeItemByHitTest(GetTreeCtrl());
	if (hItem)
		pLensFlareItem = (CLensFlareItem*)GetTreeCtrl().GetItemData(hItem);

	SelectItem(pLensFlareItem);

	int nGrayedFlag = 0;
	if (!pLensFlareItem)
		nGrayedFlag = MF_GRAYED;

	CMenu menu;
	menu.CreatePopupMenu();

	CClipboard clipboard;
	int nPasteFlags = 0;
	if (clipboard.IsEmpty())
		nPasteFlags |= MF_GRAYED;

	int nCopyPasteCloneFlag = 0;
	if (GetSelectedItemStatus() != eSIS_Flare && GetSelectedItemStatus() != eSIS_Group)
		nCopyPasteCloneFlag |= MF_GRAYED;

	menu.AppendMenu(MF_STRING | nCopyPasteCloneFlag, ID_DB_CUT, "Cut");
	menu.AppendMenu(MF_STRING | nCopyPasteCloneFlag, ID_DB_COPY, "Copy");
	menu.AppendMenu(MF_STRING | nPasteFlags, ID_DB_PASTE, "Paste");
	menu.AppendMenu(MF_STRING | nCopyPasteCloneFlag, ID_DB_CLONE, "Clone");
	menu.AppendMenu(MF_SEPARATOR, 0, "");
	menu.AppendMenu(MF_STRING, ID_DB_RENAME, "Rename\tF2");
	menu.AppendMenu(MF_STRING, ID_DB_REMOVE, "Delete\tDel");
	menu.AppendMenu(MF_SEPARATOR, 0, "");
	menu.AppendMenu(MF_STRING | nGrayedFlag, ID_DB_ASSIGNTOSELECTION, "Assign to Selected Objects");
	menu.AppendMenu(MF_STRING | nGrayedFlag, ID_DB_SELECTASSIGNEDOBJECTS, "Select Assigned Objects");
	menu.AppendMenu(MF_STRING, ID_DB_COPY_NAME_TO_CLIPBOARD, "Copy Name to ClipBoard");

	GetCursorPos(&point);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
}

void CLensFlareEditor::ExpandTreeCtrl()
{
	for (int i = 0, iSize(m_AllLensFlareTreeItems.size()); i < iSize; ++i)
	{
		if (m_AllLensFlareTreeItems[i] == NULL)
			continue;
		GetTreeCtrl().Expand(m_AllLensFlareTreeItems[i], TVE_EXPAND);
	}
}

bool CLensFlareEditor::SelectItemByName(const string& itemName)
{
	CLensFlareItem* pLensFlareItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(itemName);
	if (pLensFlareItem == NULL)
		return false;
	GetIEditorImpl()->GetLensFlareManager()->SetSelectedItem(pLensFlareItem);

	for (int i = 0, iSize(m_AllLensFlareTreeItems.size()); i < iSize; ++i)
	{
		if (m_AllLensFlareTreeItems[i] == NULL)
			continue;
		if (pLensFlareItem == (CLensFlareItem*)GetTreeCtrl().GetItemData(m_AllLensFlareTreeItems[i]))
		{
			HTREEITEM hParent = GetTreeCtrl().GetParentItem(m_AllLensFlareTreeItems[i]);
			if (hParent)
				GetTreeCtrl().Expand(hParent, TVE_EXPAND);
			GetTreeCtrl().SelectItem(m_AllLensFlareTreeItems[i]);
			break;
		}
	}

	return true;
}

void CLensFlareEditor::StartEditItem(HTREEITEM hItem)
{
	if (hItem == NULL || hItem == GetRootItem())
		return;
	GetTreeCtrl().SetFocus();
	GetTreeCtrl().EditLabel(hItem);
}

bool CLensFlareEditor::GetFullLensFlareItemName(HTREEITEM hItem, string& outFullName) const
{
	if (hItem == NULL)
		return false;
	do
	{
		outFullName.Insert(0, GetTreeCtrl().GetItemText(hItem));
		hItem = GetTreeCtrl().GetParentItem(hItem);
		if (hItem)
			outFullName.Insert(0, ".");
	}
	while (hItem);

	return true;
}

CLensFlareEditor::ESelectedItemStatus CLensFlareEditor::GetSelectedItemStatus() const
{
	HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();
	if (hItem == NULL)
		return eSIS_Unselected;
	if (GetTreeCtrl().ItemHasChildren(hItem))
		return eSIS_Group;
	return eSIS_Flare;
}

void CLensFlareEditor::SelectItemInLensFlareElementTreeByName(const string& name)
{
	m_pLensFlareElementTree->SelectTreeItemByName(name.GetString());
}

bool CLensFlareEditor::GetSelectedLensFlareName(string& outName) const
{
	string flareItemName;
	const CLensFlareElement::LensFlareElementPtr pElement = m_pLensFlareElementTree->GetCurrentLensFlareElement();
	if (pElement == NULL)
		return false;
	return pElement->GetName(outName);
}

void CLensFlareEditor::ReloadItems()
{
	__super::ReloadItems();
	ResetElementTreeControl();
	ResetLensFlareItems();
}

void CLensFlareEditor::ResetLensFlareItems()
{
	m_AllLensFlareTreeItems.clear();
	GetAllLensFlareItems(m_AllLensFlareTreeItems);
}

void CLensFlareEditor::GetAllLensFlareItems(std::vector<HTREEITEM>& outItemList) const
{
	HTREEITEM hItem = GetTreeCtrl().GetRootItem();
	while (hItem)
	{
		GetLensFlareItemsUnderSpecificItem(hItem, outItemList);
		hItem = GetTreeCtrl().GetNextItem(hItem, TVGN_NEXT);
	}
	;
}

void CLensFlareEditor::GetLensFlareItemsUnderSpecificItem(HTREEITEM hItem, std::vector<HTREEITEM>& outItemList) const
{
	if (hItem)
		outItemList.push_back(hItem);

	if (!GetTreeCtrl().ItemHasChildren(hItem))
		return;

	HTREEITEM hChildItem = GetTreeCtrl().GetChildItem(hItem);
	while (hChildItem)
	{
		GetLensFlareItemsUnderSpecificItem(hChildItem, outItemList);
		hChildItem = GetTreeCtrl().GetNextItem(hChildItem, TVGN_NEXT);
	}
}

void CLensFlareEditor::OnRemoveItem()
{
	CLensFlareItem* pSelectedLensFlareItem = GetSelectedLensFlareItem();
	CUndo undo("Remove Flare Group Item");

	if (pSelectedLensFlareItem == NULL)
	{
		HTREEITEM hLensFlareItem = GetTreeCtrl().GetSelectedItem();
		if (hLensFlareItem == NULL)
			return;
		string fullLensFlareItemName;
		if (GetFullLensFlareItemName(hLensFlareItem, fullLensFlareItemName))
		{
			string deleteMsgStr;
			deleteMsgStr.Format(_T("Delete %s?"), (const char*)fullLensFlareItemName.GetBuffer());
			if (MessageBox(deleteMsgStr, _T("Delete Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				CLensFlareLibrary* pLensFlareLibrary = GetCurrentLibrary();
				if (pLensFlareLibrary)
				{
					std::vector<_smart_ptr<CLensFlareItem>> deletedItems;
					for (int i = 0, iItemCount(pLensFlareLibrary->GetItemCount()); i < iItemCount; ++i)
					{
						CLensFlareItem* pItem = (CLensFlareItem*)pLensFlareLibrary->GetItem(i);
						if (pItem == NULL)
							continue;
						if (pItem->GetName().Find(fullLensFlareItemName) == -1)
							continue;
						HTREEITEM hFlareItem = stl::find_in_map(m_itemsToTree, pItem, (HTREEITEM)0);
						if (hFlareItem)
						{
							UpdateLensOpticsNames(pItem->GetFullName(), "");
							m_itemsToTree.erase(pItem);
							deletedItems.push_back(pItem);
						}
					}
					for (int i = 0, iDeleteItemSize(deletedItems.size()); i < iDeleteItemSize; ++i)
					{
						m_pLensFlareLightEntityTree->OnLensFlareDeleteItem(deletedItems[i]);
						DeleteItem(deletedItems[i]);
					}
				}
				GetTreeCtrl().DeleteItem(hLensFlareItem);
				GetIEditorImpl()->SetModifiedFlag();
				ResetLensFlareItems();
			}
		}
	}
	else
	{
		string deleteMsgStr;
		deleteMsgStr.Format(_T("Delete %s?"), (const char*)pSelectedLensFlareItem->GetName());
		// Remove prototype from prototype manager and library.
		if (MessageBox(deleteMsgStr, _T("Delete Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			m_pLensFlareLightEntityTree->OnLensFlareDeleteItem(pSelectedLensFlareItem);

			DeleteItem(pSelectedLensFlareItem);
			HTREEITEM hItem = stl::find_in_map(m_itemsToTree, pSelectedLensFlareItem, (HTREEITEM)0);
			if (hItem)
			{
				CLensFlareItem* pGroupItemAsTreeItemData = (CLensFlareItem*)GetTreeCtrl().GetItemData(hItem);
				UpdateLensOpticsNames(pSelectedLensFlareItem->GetFullName(), "");
				GetTreeCtrl().DeleteItem(hItem);
				m_itemsToTree.erase(pSelectedLensFlareItem);
				m_pCurrentItem = NULL;
			}

			GetIEditorImpl()->SetModifiedFlag();
			SelectItem(0);
			ResetLensFlareItems();
		}
	}
}

void CLensFlareEditor::OnAddLibrary()
{
	QStringDialog dlg(QObject::tr("New Library Name"), nullptr, true);

	dlg.SetCheckCallback([this](const string& library) -> bool
	{
		string path = m_pItemManager->MakeFilename(library.GetString());
		if (CFileUtil::FileExists(path))
		{
		  CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, string("Library '") + library + "' already exists.");
		  return false;
		}
		return true;
	});

	if (dlg.exec() == QDialog::Accepted)
	{
		if (!dlg.GetString().IsEmpty())
		{
			SelectItem(0);
			// Make new library.
			string library = dlg.GetString();
			NewLibrary(library);
			ReloadLibs();
			SelectLibrary(library);
			GetIEditorImpl()->SetModifiedFlag();
		}
	}
}

void CLensFlareEditor::OnRenameItem()
{
	HTREEITEM hSelectedItem(GetTreeCtrl().GetSelectedItem());

	if (hSelectedItem == NULL)
		return;

	StartEditItem(hSelectedItem);
}

void CLensFlareEditor::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	RecalcLayout();
}

void CLensFlareEditor::OnReloadLib()
{
	__super::OnReloadLib();
	CLensFlareLibrary* pLibrary = GetCurrentLibrary();
	if (pLibrary == NULL)
		return;
	for (int i = 0, iItemCount(pLibrary->GetItemCount()); i < iItemCount; ++i)
	{
		CLensFlareItem* pItem = (CLensFlareItem*)pLibrary->GetItem(i);
		if (pItem == NULL)
			continue;
		pItem->UpdateLights(NULL);
	}
	ResetLensFlareItems();
}

void CLensFlareEditor::RegisterLensFlareItemChangeListener(ILensFlareChangeItemListener* pListener)
{
	for (int i = 0, iSize(m_LensFlareChangeItemListenerList.size()); i < iSize; ++i)
	{
		if (m_LensFlareChangeItemListenerList[i] == pListener)
			return;
	}
	m_LensFlareChangeItemListenerList.push_back(pListener);
}

void CLensFlareEditor::UnregisterLensFlareItemChangeListener(ILensFlareChangeItemListener* pListener)
{
	auto iter = m_LensFlareChangeItemListenerList.begin();
	for (; iter != m_LensFlareChangeItemListenerList.end(); ++iter)
	{
		if (*iter == pListener)
		{
			m_LensFlareChangeItemListenerList.erase(iter);
			return;
		}
	}
}

void CLensFlareEditor::UpdateLensFlareItem(CLensFlareItem* pLensFlareItem)
{
	for (int i = 0, iSize(m_LensFlareChangeItemListenerList.size()); i < iSize; ++i)
		m_LensFlareChangeItemListenerList[i]->OnLensFlareChangeItem(pLensFlareItem);
}

void CLensFlareEditor::OnLensFlareChangeElement(CLensFlareElement* pLensFlareElement)
{
	CPropertyCtrl* pPropertyCtrl = m_pWndProps;
	pPropertyCtrl->DeleteAllItems();

	if (pLensFlareElement == NULL)
		return;

	// Update properties in a property panel
	if (pLensFlareElement && pLensFlareElement->GetProperties())
	{
		pPropertyCtrl->AddVarBlock(pLensFlareElement->GetProperties());
		pPropertyCtrl->ExpandAllChilds(pPropertyCtrl->GetRootItem(), false);
		pPropertyCtrl->EnableWindow(TRUE);
	}
}

void CLensFlareEditor::OnUpdateSelected(CCmdUI* pCmdUI)
{
}

bool CLensFlareEditor::IsPointInLensFlareElementTree(const CPoint& screenPos) const
{
	return LensFlareUtil::IsPointInWindow(m_pLensFlareElementTree, screenPos);
}

bool CLensFlareEditor::IsPointInLensFlareItemTree(const CPoint& screenPos) const
{
	return LensFlareUtil::IsPointInWindow(&m_LensFlareItemTree, screenPos);
}

void CLensFlareEditor::AddNewItemByAtomicOptics(EFlareType flareType)
{
	if (!LensFlareUtil::IsValidFlare(flareType))
		return;

	HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();
	if (hSelectedItem == NULL)
		return;

	string groupName;
	HTREEITEM hParentItem = GetTreeCtrl().GetParentItem(hSelectedItem);
	if (hParentItem)
		groupName = GetTreeCtrl().GetItemText(hParentItem);
	else
		groupName = GetTreeCtrl().GetItemText(hSelectedItem);

	FlareInfoArray::Props flareProps = FlareInfoArray::Get();
	string itemName = MakeValidName(groupName + string(".") + flareProps.p[flareType].name, functor(*this, &CDatabaseFrameWnd::DoesItemExist));
	CLensFlareItem* pNewItem = AddNewLensFlareItem(groupName, LensFlareUtil::GetShortName(itemName));

	if (pNewItem)
	{
		pNewItem->GetOptics()->AddElement(gEnv->pOpticsManager->Create(flareType));
		ReloadItems();
		SelectLensFlareItem(GetTreeLensFlareItem(pNewItem), NULL);
	}
}

void CLensFlareEditor::OnUpdateProperties(IVariable* var)
{
	GetIEditorImpl()->GetLensFlareManager()->Modified();
}

