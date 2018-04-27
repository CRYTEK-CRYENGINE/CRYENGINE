// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LensFlareElementTree.h"
#include "LensFlareItem.h"
#include "LensFlareManager.h"
#include "LensFlareUndo.h"
#include "LensFlareEditor.h"
#include "LensFlareLibrary.h"
#include "LensFlareItemTree.h"
#include "Util/Clipboard.h"

#include "QtUtil.h"

enum
{
	ID_DB_FLARE_ADDGROUP = 1,
	ID_DB_FLARE_COPY,
	ID_DB_FLARE_CUT,
	ID_DB_FLARE_PASTE,
	ID_DB_FLARE_CLONE,
	ID_DB_FLARE_RENAME,
	ID_DB_FLARE_REMOVE,
	ID_DB_FLARE_REMOVEALL,
	ID_DB_FLARE_ITEMUP,
	ID_DB_FLARE_ITEMDOWN
};

BEGIN_MESSAGE_MAP(CLensFlareElementTree, CXTTreeCtrl)
ON_NOTIFY_REFLECT(NM_RCLICK, OnNotifyTreeRClick)
ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnTvnSelchangedTree)
ON_NOTIFY_REFLECT(TVN_BEGINLABELEDIT, OnTvnBeginlabeleditTree)
ON_NOTIFY_REFLECT(NM_CLICK, OnTvnItemClicked)
ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, OnTvnEndlabeleditTree)
ON_NOTIFY_REFLECT(TVN_KEYDOWN, OnTvnKeydownTree)
ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBeginDrag)
ON_COMMAND(ID_DB_FLARE_ADDGROUP, OnAddGroup)
ON_COMMAND(ID_DB_FLARE_RENAME, OnRenameItem)
ON_COMMAND(ID_DB_FLARE_COPY, OnCopy)
ON_COMMAND(ID_DB_FLARE_CUT, OnCut)
ON_COMMAND(ID_DB_FLARE_PASTE, OnPaste)
ON_COMMAND(ID_DB_FLARE_CLONE, OnClone)
ON_COMMAND(ID_DB_FLARE_REMOVE, OnRemoveItem)
ON_COMMAND(ID_DB_FLARE_REMOVEALL, OnRemoveAll)
ON_COMMAND(ID_DB_FLARE_ITEMUP, OnItemUp)
ON_COMMAND(ID_DB_FLARE_ITEMDOWN, OnItemDown)
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CLensFlareElementTree::CLensFlareElementTree()
{
	CLensFlareEditor::GetLensFlareEditor()->RegisterLensFlareItemChangeListener(this);
}

CLensFlareElementTree::~CLensFlareElementTree()
{
	CLensFlareEditor::GetLensFlareEditor()->UnregisterLensFlareItemChangeListener(this);
}

void CLensFlareElementTree::UpdateLensFlareItem(CLensFlareItem* pLensFlareItem)
{
	ClearElementList();

	if (pLensFlareItem == NULL)
		return;

	if (pLensFlareItem->GetOptics() == NULL)
		return;

	m_pLensFlareItem = pLensFlareItem;
	UpdateLensFlareElementsRecusively(m_pLensFlareItem->GetOptics());

}

void CLensFlareElementTree::ClearElementList()
{
	m_LensFlareElements.clear();
}

CLensFlareElement::LensFlareElementPtr CLensFlareElementTree::FindRootElement() const
{
	for (int i = 0, iElementSize(m_LensFlareElements.size()); i < iElementSize; ++i)
	{
		if (m_LensFlareElements[i]->GetOpticsElement()->GetType() == eFT_Root)
			return m_LensFlareElements[i];
	}
	return NULL;
}

int CLensFlareElementTree::FindElementIndexByOptics(IOpticsElementBasePtr pOptics) const
{
	for (int i = 0, iElementSize(m_LensFlareElements.size()); i < iElementSize; ++i)
	{
		if (m_LensFlareElements[i]->GetOpticsElement() == pOptics)
			return i;
	}
	return -1;
}

void CLensFlareElementTree::AddLensElementRecursively(HTREEITEM hParent, IOpticsElementBasePtr pOptics)
{
	if (pOptics == NULL)
		return;

	CString shortName = LensFlareUtil::GetShortName(pOptics->GetName());
	HTREEITEM hItem = InsertItem(shortName, 2, 3, hParent);
	if (hItem == NULL)
		return;

	int nIndex = FindElementIndexByOptics(pOptics);
	if (nIndex == -1)
		return;

	SetItemData(hItem, (DWORD_PTR)nIndex);
	SetCheck(hItem, m_LensFlareElements[nIndex]->IsEnable());

	for (int i = 0, iChildCount(pOptics->GetElementCount()); i < iChildCount; ++i)
	{
		IOpticsElementBasePtr pChild = pOptics->GetElementAt(i);
		if (pChild == NULL)
			continue;
		AddLensElementRecursively(hItem, pChild);
	}
}

void CLensFlareElementTree::Reload()
{
	DeleteAllItems();

	CLensFlareElement::LensFlareElementPtr pRootElement = FindRootElement();
	if (pRootElement == NULL)
		return;

	IOpticsElementBasePtr pRootOptics = pRootElement->GetOpticsElement();
	if (pRootOptics == NULL)
		return;

	AddLensElementRecursively(NULL, pRootOptics);

	HTREEITEM hVisibleItem = GetFirstVisibleItem();
	while (hVisibleItem)
	{
		Expand(hVisibleItem, TVE_EXPAND);
		hVisibleItem = GetNextVisibleItem(hVisibleItem);
	}
}

IOpticsElementBasePtr CLensFlareElementTree::GetOpticsElement() const
{
	return m_pLensFlareItem->GetOptics();
}

CLensFlareElement::LensFlareElementPtr CLensFlareElementTree::AddAtomicElement(EFlareType flareType, IOpticsElementBasePtr pParent)
{
	IOpticsElementBasePtr pNewOptics = gEnv->pOpticsManager->Create(flareType);
	if (pNewOptics == NULL)
		return NULL;
	CLensFlareElement::LensFlareElementPtr pElement = AddElement(pNewOptics, pParent);
	if (pElement == NULL)
		return false;
	pParent->AddElement(pNewOptics);
	return pElement;
}

CLensFlareElement::LensFlareElementPtr CLensFlareElementTree::InsertAtomicElement(int nIndex, EFlareType flareType, IOpticsElementBasePtr pParent)
{
	if (pParent == NULL)
		return NULL;

	IOpticsElementBasePtr pNewOptics = gEnv->pOpticsManager->Create(flareType);
	if (pNewOptics == NULL)
		return NULL;

	if (nIndex < 0)
		return NULL;

	CLensFlareElement::LensFlareElementPtr pElement = AddElement(pNewOptics, pParent);
	if (pElement == NULL)
		return false;

	pParent->InsertElement(nIndex, pNewOptics);

	return pElement;
}

CLensFlareElement::LensFlareElementPtr CLensFlareElementTree::AddElement(IOpticsElementBasePtr pOptics, IOpticsElementBasePtr pParent)
{
	if (pOptics == NULL)
		return NULL;

	CLensFlareElement::LensFlareElementPtr pLensFlareElement = NULL;

	if (pParent == NULL)
	{
		if (pOptics->GetType() != eFT_Root)
		{
			assert(0 && "CFlareManager::AddItem() - pOptics must be a root optics if the parent doesn't exist.");
			return NULL;
		}
		pLensFlareElement = CreateElement(pOptics);
	}
	else
	{
		if (!LensFlareUtil::IsGroup(pParent->GetType()))
			return NULL;

		CLensFlareElement::LensFlareElementPtr pParentElement = FindLensFlareElement(pParent);
		if (pParentElement == NULL)
			return NULL;

		string fullElementName;
		if (!pParentElement->GetName(fullElementName))
			return NULL;
		if (!fullElementName.IsEmpty())
			fullElementName += ".";
		fullElementName += LensFlareUtil::GetShortName(pOptics->GetName()).MakeLower();

		string validName;
		MakeValidElementName(fullElementName, validName);
		pOptics->SetName(validName);

		pLensFlareElement = CreateElement(pOptics);
	}

	if (LensFlareUtil::IsGroup(pOptics->GetType()))
	{
		for (int i = 0, iElementCount(pOptics->GetElementCount()); i < iElementCount; ++i)
			AddElement(pOptics->GetElementAt(i), pOptics);
	}

	GetIEditorImpl()->GetLensFlareManager()->Modified();
	if (m_pLensFlareItem)
		m_pLensFlareItem->UpdateLights();

	return pLensFlareElement;
}

bool CLensFlareElementTree::AddElement(EFlareType flareType)
{
	CLensFlareElement* pSelectedElement = GetCurrentLensFlareElement();
	if (pSelectedElement == NULL)
		return false;

	CLensFlareElement::LensFlareElementPtr pNewElement = NULL;

	if (!LensFlareUtil::IsGroup(pSelectedElement->GetOpticsType()))
	{
		HTREEITEM hCurrentItem = GetSelectedItem();
		if (!hCurrentItem)
			return false;

		HTREEITEM hParentItem = GetParentItem(hCurrentItem);
		CLensFlareElement* pParentElement = GetLensFlareElement(hParentItem);
		if (pParentElement == NULL)
			return false;

		if (!LensFlareUtil::IsGroup(pParentElement->GetOpticsType()))
			return false;

		IOpticsElementBasePtr pSelectedOptics = pSelectedElement->GetOpticsElement();
		IOpticsElementBasePtr pParentOptics = pParentElement->GetOpticsElement();
		if (!pSelectedOptics || !pParentOptics)
			return false;

		int nIndex = LensFlareUtil::FindOpticsIndexUnderParentOptics(pSelectedElement->GetOpticsElement(), pParentElement->GetOpticsElement());
		if (nIndex != -1)
			pNewElement = InsertAtomicElement(nIndex, flareType, pParentOptics);
	}
	else
	{
		IOpticsElementBasePtr pSelectedOptics = pSelectedElement->GetOpticsElement();
		if (pSelectedOptics == NULL)
			return false;
		pNewElement = InsertAtomicElement(0, flareType, pSelectedOptics);
	}

	if (!pNewElement)
		return false;

	Reload();
	SelectTreeItemByLensFlareElement(pNewElement);
	CallChangeListeners();
	if (m_pLensFlareItem)
		m_pLensFlareItem->UpdateLights();
	return true;
}

void CLensFlareElementTree::StoreUndo(const CString& undoDescription)
{
	if (CUndo::IsRecording())
	{
		if (undoDescription.IsEmpty())
			CUndo::Record(new CUndoLensFlareItem(GetLensFlareItem()));
		else
			CUndo::Record(new CUndoLensFlareItem(GetLensFlareItem(), undoDescription));
	}
}

CLensFlareElement::LensFlareElementPtr CLensFlareElementTree::FindLensFlareElement(IOpticsElementBasePtr pElement) const
{
	for (size_t i = 0, iLensElementsSize(m_LensFlareElements.size()); i < iLensElementsSize; ++i)
	{
		if (m_LensFlareElements[i]->GetOpticsElement() == pElement)
			return m_LensFlareElements[i];
	}
	return NULL;
}

void CLensFlareElementTree::MakeValidElementName(const string& seedName, string& outValidName) const
{
	if (!IsExistElement(seedName))
	{
		outValidName = seedName;
		return;
	}

	int counter = 0;
	do
	{
		string numberString;
		numberString.Format("%d", counter);
		string candidateName = seedName + numberString;
		if (!IsExistElement(candidateName))
		{
			outValidName = candidateName;
			return;
		}
	}
	while (++counter < 100000); // prevent from infinite loop
}

bool CLensFlareElementTree::IsExistElement(const string& name) const
{
	for (size_t i = 0, iLensElementSize(m_LensFlareElements.size()); i < iLensElementSize; ++i)
	{
		string flareName;
		if (!m_LensFlareElements[i]->GetName(flareName))
			continue;
		if (!stricmp(flareName.GetString(), name.GetString()))
			return true;
	}
	return false;
}

void CLensFlareElementTree::UpdateLensFlareElementsRecusively(IOpticsElementBasePtr pOptics)
{
	if (pOptics == NULL)
		return;

	CLensFlareElement::LensFlareElementPtr pElement = CreateElement(pOptics);
	assert(pElement);
	pElement->SetOpticsElement(pOptics);

	for (int i = 0, iCount(pOptics->GetElementCount()); i < iCount; i++)
		UpdateLensFlareElementsRecusively(pOptics->GetElementAt(i));
}

HTREEITEM CLensFlareElementTree::InsertElementToTree(int nIndex, HTREEITEM hParent)
{
	if (nIndex < 0 || nIndex >= m_LensFlareElements.size())
		return NULL;
	CLensFlareElement* pElement = m_LensFlareElements[nIndex];
	if (pElement == NULL)
		return NULL;
	string shortName;
	if (!pElement->GetShortName(shortName))
		return NULL;
	HTREEITEM hItem = InsertItem(CString(shortName), 2, 3, hParent);
	if (hItem == NULL)
		return NULL;
	SetItemData(hItem, (DWORD_PTR)nIndex);
	SetCheck(hItem, pElement->IsEnable());
	return hItem;
}

void CLensFlareElementTree::OnInternalVariableChange(IVariable* pVar)
{
	CLensFlareElement::LensFlareElementPtr pCurrentElement = GetCurrentLensFlareElement();
	if (pCurrentElement == NULL)
		return;
	pCurrentElement->OnInternalVariableChange(pVar);
}

CLensFlareElement::LensFlareElementPtr CLensFlareElementTree::GetCurrentLensFlareElement() const
{
	return GetLensFlareElement(GetSelectedItem());
}

CLensFlareElement::LensFlareElementPtr CLensFlareElementTree::GetLensFlareElement(HTREEITEM hItem) const
{
	if (hItem == NULL)
		return NULL;
	int nItemIndex = (int)GetItemData(hItem);
	if (nItemIndex < 0 || nItemIndex >= m_LensFlareElements.size())
		return NULL;
	return m_LensFlareElements[nItemIndex];
}

void CLensFlareElementTree::SelectTreeItemByLensFlareElement(const CLensFlareElement* pElement)
{
	if (pElement == NULL)
		return;

	// Select an item in tree ctrl.
	HTREEITEM hVisibleItem = GetFirstVisibleItem();
	while (hVisibleItem)
	{
		int nItemIndex = (int)GetItemData(hVisibleItem);
		CLensFlareElement::LensFlareElementPtr pLensFlareElement = m_LensFlareElements[nItemIndex];
		if (pElement == pLensFlareElement)
		{
			SelectItem(hVisibleItem);
			break;
		}
		hVisibleItem = GetNextVisibleItem(hVisibleItem);
	}
}

void CLensFlareElementTree::StartEditItem(HTREEITEM hItem)
{
	if (hItem == NULL)
		return;
	SetFocus();
	EditLabel(hItem);
}

void CLensFlareElementTree::OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CLensFlareElement* pLensFlareElement = 0;

	HTREEITEM hItem = LensFlareUtil::GetTreeItemByHitTest(*this);
	if (hItem)
		pLensFlareElement = GetLensFlareElement(hItem);

	// Copy, Cut and Clone can't be done about a Root item so those menus should be disable when the selected item is a root item.
	int bGrayed = 0;
	if (GetParentItem(hItem) == NULL)
		bGrayed = MF_GRAYED;

	SelectTreeItemByLensFlareElement(pLensFlareElement);
	CallChangeListeners();

	CMenu menu;
	menu.CreatePopupMenu();

	menu.AppendMenu(MF_STRING, ID_DB_FLARE_ADDGROUP, "Add Group");
	menu.AppendMenu(MF_SEPARATOR, 0, "");
	menu.AppendMenu(MF_STRING, ID_DB_FLARE_COPY, "Copy");
	menu.AppendMenu(MF_STRING | bGrayed, ID_DB_FLARE_CUT, "Cut");
	menu.AppendMenu(MF_STRING, ID_DB_FLARE_PASTE, "Paste");
	menu.AppendMenu(MF_STRING, ID_DB_FLARE_CLONE, "Clone");
	menu.AppendMenu(MF_SEPARATOR, 0, "");
	menu.AppendMenu(MF_STRING, ID_DB_FLARE_RENAME, "Rename\tF2");
	menu.AppendMenu(MF_STRING, ID_DB_FLARE_REMOVE, "Delete\tDel");
	menu.AppendMenu(MF_STRING, ID_DB_FLARE_REMOVEALL, "Delete All");
	menu.AppendMenu(MF_SEPARATOR, 0, "");
	menu.AppendMenu(MF_STRING, ID_DB_FLARE_ITEMUP, "Up");
	menu.AppendMenu(MF_STRING, ID_DB_FLARE_ITEMDOWN, "Down");

	CPoint point;
	GetCursorPos(&point);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);

	// Mark message as handled and suppress default handling
	*pResult = 1;
}

void CLensFlareElementTree::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* pNMTreeView = (NMTREEVIEW*)pNMHDR;

	if (pNMTreeView->itemOld.hItem)
		ElementChanged(GetLensFlareElement(pNMTreeView->itemOld.hItem));

	CallChangeListeners();

	*pResult = 1;
}

void CLensFlareElementTree::ElementChanged(CLensFlareElement* pPrevLensFlareElement)
{
	if (!pPrevLensFlareElement)
		return;
	string itemName;
	if (pPrevLensFlareElement->GetName(itemName))
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoLensFlareElementSelection(GetLensFlareItem(), itemName));
	}
}

void CLensFlareElementTree::OnTvnBeginlabeleditTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);

	// Mark message as handled and suppress default handling
	*pResult = 0;
}

void CLensFlareElementTree::OnTvnItemClicked(NMHDR* pNMHDR, LRESULT* pResult)
{
	TVHITTESTINFO ht = { 0 };
	DWORD dwPos = ::GetMessagePos();
	ht.pt.x = GET_X_LPARAM(dwPos);
	ht.pt.y = GET_Y_LPARAM(dwPos);

	::MapWindowPoints(HWND_DESKTOP, pNMHDR->hwndFrom, &ht.pt, 1);
	TreeView_HitTest(pNMHDR->hwndFrom, &ht);

	CUndo undo("Update an enable checkbox for tree ctrl.");

	if (TVHT_ONITEMSTATEICON & ht.flags)
	{
		HTREEITEM hItem(ht.hItem);
		if (hItem == NULL)
			return;

		CLensFlareElement* pLensFlareElement = GetLensFlareElement(hItem);
		if (pLensFlareElement == NULL)
			return;

		EnableElement(pLensFlareElement, !(GetCheck(hItem)));
		SelectItem(hItem);

		CallChangeListeners();
	}

	*pResult = 0;
}

void CLensFlareElementTree::EnableElement(CLensFlareElement* pLensFlareElement, bool bEnable)
{
	StoreUndo();
	pLensFlareElement->SetEnable(bEnable);
}

void CLensFlareElementTree::OnTvnEndlabeleditTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
	HTREEITEM hItem = GetSelectedItem();

	if (hItem == NULL)
		return;

	if (pTVDispInfo->item.pszText == NULL)
		return;

	CUndo undo("Rename library item");

	if (IsExistElement(pTVDispInfo->item.pszText))
	{
		MessageBox("The identical name exists in a database", "Warning", MB_OK);
		StartEditItem(hItem);
	}
	else if (strstr(pTVDispInfo->item.pszText, "."))
	{
		MessageBox("The name must not contain \".\"", "Warning", MB_OK);
		StartEditItem(hItem);
	}
	else
	{
		CLensFlareElement* pLensFlareElement = GetCurrentLensFlareElement();

		string prevName;
		if (pLensFlareElement && pLensFlareElement->GetName(prevName))
		{
			string parentName(prevName);
			int offset = parentName.ReverseFind('.');
			if (offset == -1)
				parentName = "";
			else
				parentName.Delete(offset + 1, parentName.GetLength() - offset);

			if (hItem == GetRootItem())
			{
				CLensFlareEditor* pLensFlareEditor = CLensFlareEditor::GetLensFlareEditor();
				if (pLensFlareEditor)
				{
					CLensFlareItem* pLensFlareItem = pLensFlareEditor->GetSelectedLensFlareItem();
					if (pLensFlareItem)
					{
						string candidateName = LensFlareUtil::ReplaceLastName(pLensFlareItem->GetName(), pTVDispInfo->item.pszText);
						if (pLensFlareEditor->IsExistTreeItem(candidateName, true))
						{
							MessageBox("The identical name exists in a database", "Warning", MB_OK);
							*pResult = 0;
							return;
						}
						pLensFlareEditor->RenameLensFlareItem(pLensFlareItem, pLensFlareItem->GetGroupName(), pTVDispInfo->item.pszText);
					}
				}
				else
				{
					MessageBox("Renaming is not possible.", "Warning", MB_OK);
					*pResult = 0;
					return;
				}
			}

			SetItemText(hItem, pTVDispInfo->item.pszText);
			RenameElement(pLensFlareElement, CString(parentName) + pTVDispInfo->item.pszText);

			CallChangeListeners();
			GetIEditorImpl()->GetLensFlareManager()->Modified();
		}
	}

	// Mark message as handled and suppress default handling
	*pResult = 0;
}

void CLensFlareElementTree::RenameElement(CLensFlareElement* pLensFlareElement, const CString& newName)
{
	StoreUndo();
	IOpticsElementBasePtr pOptics = pLensFlareElement->GetOpticsElement();
	if (pOptics)
	{
		pOptics->SetName(newName);
		LensFlareUtil::UpdateOpticsName(pOptics);
	}
}

void CLensFlareElementTree::OnTvnKeydownTree(NMHDR* pNMHDR, LRESULT* pResult)
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

XmlNodeRef CLensFlareElementTree::CreateXML(const char* type) const
{
	std::vector<LensFlareUtil::SClipboardData> clipboardData;
	if (GetClipboardList(type, clipboardData))
		return LensFlareUtil::CreateXMLFromClipboardData(type, "", false, clipboardData);
	return NULL;
}

void CLensFlareElementTree::UpdateClipboard(const char* type, bool bPasteAtSameLevel)
{
	std::vector<LensFlareUtil::SClipboardData> clipboardData;
	if (GetClipboardList(type, clipboardData))
		LensFlareUtil::UpdateClipboard(type, "", bPasteAtSameLevel, clipboardData);
}

bool CLensFlareElementTree::GetClipboardList(const char* type, std::vector<LensFlareUtil::SClipboardData>& outList) const
{
	CLensFlareElement::LensFlareElementPtr pElement = GetCurrentLensFlareElement();
	if (pElement == NULL)
		return false;
	string name;
	if (!pElement->GetName(name))
		return false;

	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return false;

	CLensFlareItem* pLensFlareItem = pEditor->GetSelectedLensFlareItem();
	if (pLensFlareItem == NULL)
		return false;

	outList.push_back(LensFlareUtil::SClipboardData(LENSFLARE_ELEMENT_TREE, pLensFlareItem->GetFullName(), name));

	return true;
}

void CLensFlareElementTree::OnAddGroup()
{
	CUndo undo("Add library item");
	AddElement(eFT_Group);
	GetIEditorImpl()->GetLensFlareManager()->Modified();
}

void CLensFlareElementTree::OnCopy()
{
	UpdateClipboard(FLARECLIPBOARDTYPE_COPY, false);
}

void CLensFlareElementTree::OnCut()
{
	Cut(false);
}

void CLensFlareElementTree::Cut(bool bPasteAtSameLevel)
{
	UpdateClipboard(FLARECLIPBOARDTYPE_CUT, bPasteAtSameLevel);
}

void CLensFlareElementTree::OnPaste()
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;

	XmlNodeRef xmlNode = clipboard.Get();
	if (xmlNode == NULL)
		return;

	CUndo undo("Copy/Cut & Paste library item");
	Paste(xmlNode);
}

void CLensFlareElementTree::Paste(XmlNodeRef xmlNode)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return;

	CString type;
	xmlNode->getAttr("Type", type);

	bool bPasteAtSameLevel = false;
	xmlNode->getAttr("PasteAtSameLevel", bPasteAtSameLevel);

	HTREEITEM hParentItem = NULL;
	HTREEITEM hSelectedTreeItem = GetSelectedItem();
	if (hSelectedTreeItem == NULL)
		return;

	IOpticsElementBasePtr pSelectedOptics = GetOpticsElementByTreeItem(hSelectedTreeItem);
	if (pSelectedOptics == NULL)
		return;

	CLensFlareItem* pSelectedLensFlareItem = pEditor->GetSelectedLensFlareItem();
	if (!pSelectedLensFlareItem)
		return;

	if (!LensFlareUtil::IsGroup(pSelectedOptics->GetType()))
	{
		HTREEITEM hParentTreeItem = GetParentItem(hSelectedTreeItem);
		if (hParentTreeItem == NULL)
			hParentItem = TVI_ROOT;
		else
			hParentItem = hParentTreeItem;
	}
	else
	{
		if (bPasteAtSameLevel)
			hParentItem = GetParentItem(hSelectedTreeItem);
		else
			hParentItem = hSelectedTreeItem;
	}

	if (hParentItem == NULL)
		return;

	IOpticsElementBasePtr pParentOptics = GetOpticsElementByTreeItem(hParentItem);
	if (pParentOptics == NULL)
		return;

	for (int i = 0, iChildCount(xmlNode->getChildCount()); i < iChildCount; ++i)
	{
		LensFlareUtil::SClipboardData clipboardData;
		clipboardData.FillThisFromXmlNode(xmlNode->getChild(i));

		if (clipboardData.m_LensFlareFullPath == GetLensFlareItem()->GetFullName() && clipboardData.m_From == LENSFLARE_ITEM_TREE)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[%s] lens item can be pasted into the same item", clipboardData.m_LensFlareFullPath);
			continue;
		}

		IOpticsElementBasePtr pSourceOptics = pEditor->FindOptics(clipboardData.m_LensFlareFullPath, clipboardData.m_LensOpticsPath);
		if (pSourceOptics == NULL)
			continue;

		CLensFlareItem* pSourceLensFlareItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(clipboardData.m_LensFlareFullPath);
		if (pSourceLensFlareItem == NULL)
			continue;

		if (type == FLARECLIPBOARDTYPE_CUT)
		{
			if (pSelectedLensFlareItem->GetFullName() == clipboardData.m_LensFlareFullPath && LensFlareUtil::FindOptics(pSourceOptics, pSelectedOptics->GetName()))
			{
				MessageBox("You can't paste this item here.", "Warning", MB_OK);
				return;
			}
		}

		// if the copied optics type is root, the type should be converted to group type.
		bool bForceConvertType = false;
		if (pSourceOptics->GetType() == eFT_Root)
			bForceConvertType = true;

		IOpticsElementBasePtr pNewOptics = LensFlareUtil::CreateOptics(pSourceOptics, bForceConvertType);
		if (pNewOptics == NULL)
			return;

		if (type == FLARECLIPBOARDTYPE_CUT)
		{
			if (pSourceLensFlareItem == pSelectedLensFlareItem)
			{
				CLensFlareElement::LensFlareElementList removedElements;
				GatherItemsRecusively(GetTreeItemByOpticsElement(pSourceOptics), removedElements);
				DeleteOnesInElementList(removedElements);
			}
			else if (clipboardData.m_From == LENSFLARE_ITEM_TREE)
			{
				CLensFlareLibrary* pLibrary = pEditor->GetCurrentLibrary();
				if (pLibrary)
				{
					string lensFlareFullName = m_pLensFlareItem ? m_pLensFlareItem->GetFullName() : "";
					pLibrary->RemoveItem(pSourceLensFlareItem);
					pEditor->ReloadItems();
					pSourceLensFlareItem->UpdateLights();
					pEditor->UpdateLensOpticsNames(pSourceLensFlareItem->GetFullName(), "");
					if (!lensFlareFullName.IsEmpty())
						pEditor->SelectItemByName(lensFlareFullName);
				}
			}
			LensFlareUtil::RemoveOptics(pSourceOptics);
		}

		// The children optics items were already added in a creation phase so we don't need to update the optics object.
		CLensFlareElement::LensFlareElementPtr pNewElement = AddElement(pNewOptics, pParentOptics);
		assert(pNewElement);
		if (!pNewElement)
			return;
		if (hParentItem == hSelectedTreeItem)
		{
			pParentOptics->InsertElement(0, pNewOptics);
		}
		else
		{
			int nInsertedPos = LensFlareUtil::FindOpticsIndexUnderParentOptics(pSelectedOptics, pParentOptics);
			if (nInsertedPos >= pParentOptics->GetElementCount() || nInsertedPos == -1)
				pParentOptics->AddElement(pNewOptics);
			else
				pParentOptics->InsertElement(nInsertedPos, pNewOptics);
		}

		LensFlareUtil::UpdateOpticsName(pNewOptics);
		Reload();
		SelectTreeItemByLensFlareElement(pNewElement);
		CallChangeListeners();
		pSelectedLensFlareItem->UpdateLights();
	}
}

void CLensFlareElementTree::GatherItemsRecusively(HTREEITEM hItem, CLensFlareElement::LensFlareElementList& outList)
{
	if (hItem == NULL)
		return;

	int nIndex = (int)GetItemData(hItem);
	CLensFlareElement* pElement = GetElement(nIndex);
	if (pElement)
		outList.push_back(pElement);

	HTREEITEM hChildItem = GetChildItem(hItem);
	while (hChildItem)
	{
		GatherItemsRecusively(hChildItem, outList);
		hChildItem = GetNextSiblingItem(hChildItem);
	}
}

void CLensFlareElementTree::DeleteOnesInElementList(CLensFlareElement::LensFlareElementList& elementList)
{
	CLensFlareElement::LensFlareElementList::iterator ii = m_LensFlareElements.begin();

	for (; ii != m_LensFlareElements.end(); )
	{
		bool bErased(false);
		for (int i = 0, elementListSize(elementList.size()); i < elementListSize; ++i)
		{
			if (elementList[i] == *ii)
			{
				ii = m_LensFlareElements.erase(ii);
				bErased = true;
				break;
			}
		}
		if (bErased == false)
			++ii;
	}

	GetIEditorImpl()->GetLensFlareManager()->Modified();
	if (m_pLensFlareItem)
		m_pLensFlareItem->UpdateLights();
}

void CLensFlareElementTree::OnClone()
{
	OnCopy();
	OnPaste();
}

void CLensFlareElementTree::OnRenameItem()
{
	StartEditItem(GetSelectedItem());
}

void CLensFlareElementTree::OnRemoveItem()
{
	HTREEITEM hItem = GetSelectedItem();
	if (hItem == NULL)
		return;

	// A root item must not be removed.
	HTREEITEM hParent = GetParentItem(hItem);
	if (hParent == NULL)
		return;

	CLensFlareElement::LensFlareElementPtr pCurrentElement = GetLensFlareElement(hItem);
	if (pCurrentElement == NULL)
		return;

	string name;
	pCurrentElement->GetName(name);

	string str;
	str.Format(_T("Delete %s?"), name.GetString());
	if (MessageBox(str, _T("Delete Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES)
	{
		CUndo undo("Remove an optics element");
		HTREEITEM hNextItem = GetNextSiblingItem(hItem);
		RemoveElement(hItem);
		SelectItem(hNextItem);
		CallChangeListeners();
		if (m_pLensFlareItem)
			m_pLensFlareItem->UpdateLights();
		Reload();
	}
}

void CLensFlareElementTree::OnRemoveAll()
{
	CUndo undo("Remove All in FlareTreeCtrl");

	if (MessageBox(_T("Do you want delete all?"), _T("Delete Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES)
	{
		RemoveAllElements();
		UpdateLensFlareItem(m_pLensFlareItem);
		Reload();
		if (m_pLensFlareItem)
			m_pLensFlareItem->UpdateLights();

		GetIEditorImpl()->GetLensFlareManager()->Modified();
	}
}

void CLensFlareElementTree::RemoveAllElements()
{
	CLensFlareElement::LensFlareElementPtr pElement = GetLensFlareElement(GetRootItem());
	if (pElement == NULL)
		return;
	IOpticsElementBasePtr pOptics = pElement->GetOpticsElement();
	if (pOptics == NULL)
		return;
	StoreUndo();
	pOptics->RemoveAll();
}

void CLensFlareElementTree::OnItemUp()
{
	HTREEITEM hSelectedItem = GetSelectedItem();
	if (hSelectedItem == NULL)
		return;

	HTREEITEM hPrevItem = GetPrevSiblingItem(hSelectedItem);
	if (hPrevItem == NULL)
		return;

	hPrevItem = GetPrevSiblingItem(hPrevItem);

	bool bPasteAtSameLevel = true;
	if (!hPrevItem)
	{
		bPasteAtSameLevel = false;
		hPrevItem = GetParentItem(hSelectedItem);
	}

	if (hPrevItem)
	{
		Cut(bPasteAtSameLevel);
		SelectItem(hPrevItem);
		OnPaste();
	}
}

void CLensFlareElementTree::OnItemDown()
{
	HTREEITEM hSelectedItem = GetSelectedItem();
	if (hSelectedItem == NULL)
		return;

	HTREEITEM hNextItem = GetNextSiblingItem(hSelectedItem);
	if (hNextItem)
	{
		Cut(true);
		SelectItem(hNextItem);
		OnPaste();
	}
}

IOpticsElementBasePtr CLensFlareElementTree::GetOpticsElementByTreeItem(HTREEITEM hItem) const
{
	if (hItem == NULL)
		return NULL;

	int nIndex = (int)GetItemData(hItem);
	CLensFlareElement::LensFlareElementPtr pElement = GetElement(nIndex);

	if (pElement == NULL)
		return NULL;

	return pElement->GetOpticsElement();
}

HTREEITEM CLensFlareElementTree::GetTreeItemByOpticsElement(const IOpticsElementBasePtr pOptics) const
{
	HTREEITEM hItem = GetFirstVisibleItem();

	while (hItem)
	{
		CLensFlareElement::LensFlareElementPtr pElement = GetElement((int)GetItemData(hItem));
		if (pElement)
		{
			if (pElement->GetOpticsElement() == pOptics)
				return hItem;
		}
		hItem = GetNextVisibleItem(hItem);
	}

	return NULL;
}

void CLensFlareElementTree::RemoveElement(HTREEITEM hItem)
{
	StoreUndo();
	IOpticsElementBasePtr pOptics = GetOpticsElementByTreeItem(hItem);
	if (pOptics)
		LensFlareUtil::RemoveOptics(pOptics);
	CLensFlareElement::LensFlareElementList removedElementList;
	GatherItemsRecusively(hItem, removedElementList);
	DeleteOnesInElementList(removedElementList);
}

void CLensFlareElementTree::SelectTreeItemByName(const CString& name)
{
	HTREEITEM hVisibleItem = GetFirstVisibleItem();
	while (hVisibleItem)
	{
		int nItemIndex = (int)GetItemData(hVisibleItem);
		CLensFlareElement::LensFlareElementPtr pElement = GetElement(nItemIndex);
		string flareName;
		if (pElement->GetName(flareName))
		{
			if (flareName == name.GetString())
			{
				SelectItem(hVisibleItem);
				break;
			}
		}

		hVisibleItem = GetNextVisibleItem(hVisibleItem);
	}
}

void CLensFlareElementTree::SelectTreeItemByCursorPos(bool* pOutChanged)
{
	HTREEITEM hOldItem = GetSelectedItem();
	HTREEITEM hItem = LensFlareUtil::GetTreeItemByHitTest(*this);
	if (hItem == NULL)
		return;
	if (pOutChanged)
		*pOutChanged = hOldItem != hItem;
	SelectItem(hItem);
}

void CLensFlareElementTree::OnLensFlareChangeItem(CLensFlareItem* pLensFlareItem)
{
	if (pLensFlareItem)
		UpdateLensFlareItem(pLensFlareItem);
	else
		ClearElementList();
	Reload();
}

void CLensFlareElementTree::OnLensFlareDeleteItem(CLensFlareItem* pLensFlareItem)
{
}

void CLensFlareElementTree::CallChangeListeners()
{
	CLensFlareElement* pElement = GetCurrentLensFlareElement();
	for (int i = 0, iSize(m_LensFlaresElementListeners.size()); i < iSize; ++i)
		m_LensFlaresElementListeners[i]->OnLensFlareChangeElement(pElement);
}

CLensFlareElement::LensFlareElementPtr CLensFlareElementTree::CreateElement(IOpticsElementBasePtr pOptics)
{
	StoreUndo();
	_smart_ptr<CLensFlareElement> pElement = new CLensFlareElement;
	m_LensFlareElements.push_back(pElement);
	pElement->SetOpticsElement(pOptics);
	return pElement;
}

void CLensFlareElementTree::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	if (!pNMTreeView)
		return;

	if (!pNMTreeView->itemNew.hItem)
		return;

	SelectItem(pNMTreeView->itemNew.hItem);

	bool bControlPressed = QtUtil::IsModifierKeyDown(Qt::ControlModifier);

	XmlNodeRef XMLNode;
	XMLNode = CreateXML(bControlPressed ? FLARECLIPBOARDTYPE_COPY : FLARECLIPBOARDTYPE_CUT);
	if (XMLNode == NULL)
		return;

	LensFlareUtil::BeginDragDrop(*this, pNMTreeView->ptDrag, XMLNode);
}

void CLensFlareElementTree::OnMouseMove(UINT nFlags, CPoint point)
{
	LensFlareUtil::MoveDragDrop(*this, point, &functor(*this, &CLensFlareElementTree::UpdateLensFlareElement));
	__super::OnMouseMove(nFlags, point);
}

void CLensFlareElementTree::OnLButtonUp(UINT nFlags, CPoint point)
{
	LensFlareUtil::EndDragDrop(*this, point, &functor(*this, &CLensFlareElementTree::Drop));
	__super::OnLButtonUp(nFlags, point);
}

void CLensFlareElementTree::OnLButtonDown(UINT nFlags, CPoint point)
{
	CUndo undo("Changed Lens flares element");
	__super::OnLButtonDown(nFlags, point);
}

void CLensFlareElementTree::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CUndo undo("Changed Lens flares element");
	__super::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CLensFlareElementTree::UpdateLensFlareElement(HTREEITEM hSelectedItem, const CPoint& screenPos)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (!pEditor)
		return;

	if (pEditor->IsPointInLensFlareElementTree(screenPos))
	{
		SelectTreeItemByCursorPos();
	}
	else if (pEditor->IsPointInLensFlareItemTree(screenPos))
	{
		pEditor->GetLensFlareItemTree()->UpdateDraggingFromOtherWindow();
	}
}

void CLensFlareElementTree::Drop(XmlNodeRef xmlNode, const CPoint& screenPos)
{
	if (xmlNode == NULL)
		return;

	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (!pEditor)
		return;

	if (pEditor->IsPointInLensFlareElementTree(screenPos))
	{
		SelectTreeItemByCursorPos();
		CUndo undo("Copy/Cut & Paste library item");
		Paste(xmlNode);
	}
	else if (pEditor->IsPointInLensFlareItemTree(screenPos))
	{
		CUndo undo("Copy/Cut & Paste library item");
		pEditor->Paste(xmlNode);
	}
}

void CLensFlareElementTree::UpdateDraggingFromOtherWindow()
{
	bool bChanged(false);
	SelectTreeItemByCursorPos(&bChanged);
	if (bChanged)
		RedrawWindow();
}

