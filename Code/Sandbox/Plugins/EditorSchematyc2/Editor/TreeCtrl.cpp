// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TreeCtrl.h"
#include "BrowserIcons.h"

#pragma warning(disable: 4355)

namespace Schematyc2
{
	namespace
	{
		// Temporary (and very hacky) workaround for poor sorting in browser. This can be fixed properly when we port the browsers to QT.
		bool SortCustomTreeCtrlItems(const CCustomTreeCtrlItemPtr& t1, const CCustomTreeCtrlItemPtr& t2)
		{
			static const int BROWSER_ICON_PRIORITIES[] =
			{
				0,	// FOLDER
				1,	// DOC
				0,	// BRANCH
				21,	// FUNCTION_GRAPH
				5,	// ENV_ACTION
				1,	// ENV_SIGNAL
				2,	// ENV_GLOBAL_FUNCTION
				3,	// ENV_COMPONENT
				4,	// ENV_COMPONENT_FUNCTION
				4,	// ENUMERATION
				15,	// VARIABLE
				3,	// USER_GROUP
				1,	// FOR_LOOP
				6,	// SIGNAL
				10,	// CLASS
				20,	// COMPONENT
				22,	// ACTION_INSTANCE
				13,	// STATE
				12,	// STATE_MACHINE
				17,	// TIMER
				26,	// SIGNAL_RECEIVER
				24,	// CONSTRUCTOR
				23,	// CONDITION_GRAPH
				2,	// RETURN
				6,	// ENV_ACTION_FUNCTION
				18,	// ABSTRACT_INTERFACE_IMPLEMENTATION
				19,	// ABSTRACT_INTERFACE_FUNCTION_IMPLEMENTATION
				2,	// INCLUDE
				27,	// INPUT
				28,	// OUTPUT
				5,	// STRUCTURE
				25,	// DESTRUCTOR
				0,	// SETTINGS_FOLDER
				1,	// SETTINGS_DOC
				16,	// CONTAINER
				7,	// ABSTRACT_INTERFACE
				9,	// ABSTRACT_INTERFACE_FUNCTION
				8,	// ABSTRACT_INTERFACE_TASK
				14,	// PROPERTY
				11,	// BASE_CLASS
			};

			static_assert(CRY_ARRAY_COUNT(BROWSER_ICON_PRIORITIES) == BrowserIcon::COUNT, "Size of array does not match number of constants in BrowserIcon enumeration!");

			int lhsImage = t1->GetIcon();
			int rhsImage = t2->GetIcon();

			if (lhsImage == rhsImage)
			{
				return strcmp(t1->GetText(), t2->GetText()) < 0;
			}
			else if (std::max(lhsImage, rhsImage) < CRY_ARRAY_COUNT(BROWSER_ICON_PRIORITIES) && BROWSER_ICON_PRIORITIES[lhsImage] != BROWSER_ICON_PRIORITIES[rhsImage])
			{
				return BROWSER_ICON_PRIORITIES[lhsImage] < BROWSER_ICON_PRIORITIES[rhsImage];
			}
			else
			{
				return lhsImage < rhsImage;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItem::CCustomTreeCtrlItem(const char* text, size_t icon, const CCustomTreeCtrlItemPtr& pParent, const SGUID& guid)
		: m_text(text)
		, m_icon(icon)
		, m_pParent(pParent)
		, m_hTreeItem(NULL)
		, m_guid(guid)
	{}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItem::~CCustomTreeCtrlItem() {}

	//////////////////////////////////////////////////////////////////////////
	const char* CCustomTreeCtrlItem::GetText() const
	{
		return m_text.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CCustomTreeCtrlItem::GetIcon() const
	{
		return m_icon;
	}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItemPtr CCustomTreeCtrlItem::GetParent() const
	{
		return m_pParent;
	}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItemPtr CCustomTreeCtrlItem::FindAncestor(const char* text) const
	{
		CRY_ASSERT(text);
		if(text)
		{
			for(CCustomTreeCtrlItemPtr pAncestor = m_pParent; pAncestor; pAncestor = pAncestor->GetParent())
			{
				if(pAncestor->GetText() == text)
				{
					return pAncestor;
				}
			}
		}
		return CCustomTreeCtrlItemPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItemPtr CCustomTreeCtrlItem::FindAncestor(size_t icon) const
	{
		for(CCustomTreeCtrlItemPtr pAncestor = m_pParent; pAncestor; pAncestor = pAncestor->GetParent())
		{
			if(pAncestor->GetIcon() == icon)
			{
				return pAncestor;
			}
		}
		return CCustomTreeCtrlItemPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItemPtr CCustomTreeCtrlItem::FindAncestor(const TSizeTConstArray& icons) const
	{
		const size_t	*pBeginIcon = icons.begin();
		const size_t	*pEndIcon = icons.end();
		for(CCustomTreeCtrlItemPtr pAncestor = m_pParent; pAncestor; pAncestor = pAncestor->GetParent())
		{
			const size_t	ancestorIcon = pAncestor->GetIcon();
			for(const size_t* pIcon = pBeginIcon; pIcon != pEndIcon; ++ pIcon)
			{
				if(*pIcon == ancestorIcon)
				{
					return pAncestor;
				}
			}
		}
		return CCustomTreeCtrlItemPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrlItem::SetHTreeItem(HTREEITEM hTreeItem)
	{
		m_hTreeItem = hTreeItem;
	}

	//////////////////////////////////////////////////////////////////////////
	HTREEITEM CCustomTreeCtrlItem::GetHTreeItem() const
	{
		return m_hTreeItem;
	}

	//////////////////////////////////////////////////////////////////////////
	const SGUID& CCustomTreeCtrlItem::GetGUID() const
	{
		return m_guid;
	}

	void CCustomTreeCtrlItem::AddChild(CCustomTreeCtrlItemPtr pItem)
	{
		m_children.push_back(pItem);
	}

	const TCustomTreeCtrlItemPtrVector& CCustomTreeCtrlItem::GetChildren()
	{
		return m_children;
	}

	void CCustomTreeCtrlItem::RemoveChild(CCustomTreeCtrlItemPtr pItem)
	{
		TCustomTreeCtrlItemPtrVector::iterator it = std::find(m_children.begin(), m_children.end(), pItem);
		if (it != m_children.end())
		{
			m_children.erase(it);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrlItem::SetText(const char* text)
	{
		m_text = text;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CCustomTreeCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
	{
		LOADING_TIME_PROFILE_SECTION;
		if(__super::Create(dwStyle | WS_BORDER | TVS_HASBUTTONS | TVS_NOTOOLTIPS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, rect, pParentWnd, nID))
		{
			// Register drop target.
			m_dropTarget.Register(this);
			// Load image list.
			LoadImageList(m_imageList);
			return true;
		}
		else
		{
			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::ExpandItem(const CCustomTreeCtrlItemPtr& pItem)
	{
		CRY_ASSERT(pItem);
		if(pItem)
		{
			CXTTreeCtrl::Expand(pItem->GetHTreeItem(), TVE_EXPAND);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::EnsureVisible(const CCustomTreeCtrlItemPtr& pItem)
	{
		CRY_ASSERT(pItem);
		if(pItem)
		{
			CXTTreeCtrl::EnsureVisible(pItem->GetHTreeItem());
		}
	}

	CCustomTreeCtrl::CCustomTreeCtrl(TCustomTreeCtrlItemCompFunc itemCompare /* = NULL */)
		: m_itemCompare(itemCompare ? itemCompare : SortCustomTreeCtrlItems)
		, m_dropTarget(*this)
		, m_root(new CCustomTreeCtrlItem("", (size_t)-1, NULL, SGUID()))
	{}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrl::~CCustomTreeCtrl() {}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::AddItem(const CCustomTreeCtrlItemPtr& pItem)
	{
		CRY_ASSERT(pItem);
		if(pItem)
		{
			CCustomTreeCtrlItemPtr	pParentItem = pItem->GetParent();
			if (!pParentItem)
			{
				pParentItem = m_root;
			}
			HTREEITEM hParentTreeItem = pParentItem ? pParentItem->GetHTreeItem() : NULL;

			//Find correct place to insert node
			const TCustomTreeCtrlItemPtrVector& children = pParentItem->GetChildren();
			auto it = std::lower_bound(children.begin(), children.end(), pItem, m_itemCompare);

			HTREEITEM hTreeItem = CXTTreeCtrl::InsertItem(pItem->GetText(), pItem->GetIcon(), pItem->GetIcon(), hParentTreeItem, !hParentTreeItem || it == children.end() ? TVI_LAST : (*it)->GetHTreeItem());
			CXTTreeCtrl::SetItemData(hTreeItem, (DWORD_PTR)hTreeItem);
			pItem->SetHTreeItem(hTreeItem);
			m_items.push_back(pItem);
			const SGUID& guid = pItem->GetGUID();
			if (guid)
			{
				m_guid_map[guid] = pItem;
			}
			m_htreeitem_map[hTreeItem] = pItem;
			pParentItem->AddChild(pItem);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::SetItemText(const CCustomTreeCtrlItemPtr& pItem, const char* text)
	{
		CRY_ASSERT(pItem);
		if(pItem)
		{
			CXTTreeCtrl::SetItemText(pItem->GetHTreeItem(), text);
			pItem->SetText(text);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::SetItemColor(const CCustomTreeCtrlItemPtr& pItem, COLORREF color)
	{
		CRY_ASSERT(pItem);
		if(pItem)
		{
			CXTTreeCtrl::SetItemColor(pItem->GetHTreeItem(), color);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::SetItemBold(const CCustomTreeCtrlItemPtr& pItem, bool bBold)
	{
		CRY_ASSERT(pItem);
		if(pItem)
		{
			CXTTreeCtrl::SetItemBold(pItem->GetHTreeItem(), bBold);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::RemoveItem(const CCustomTreeCtrlItemPtr& pItem)
	{
		for(TCustomTreeCtrlItemPtrVector::iterator iItem = m_items.begin(), iEndItem = m_items.end(); iItem != iEndItem; ++ iItem)
		{
			const CCustomTreeCtrlItemPtr& _pItem = *iItem;
			if (_pItem == pItem)
			{
				HTREEITEM hTreeItem = _pItem->GetHTreeItem();
				CXTTreeCtrl::DeleteItem(hTreeItem);
				const SGUID& guid = _pItem->GetGUID();
				if (guid)
				{
					m_guid_map.erase(guid);
				}
				m_htreeitem_map.erase(hTreeItem);
				CCustomTreeCtrlItemPtr pParentItem = _pItem->GetParent();
				if (!pParentItem)
				{
					pParentItem = m_root;
				}
				pParentItem->RemoveChild(_pItem);
				if(iItem != (iEndItem - 1))
				{
					*iItem = m_items.back();
				}
				m_items.pop_back();
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::RemoveItemChildren(const CCustomTreeCtrlItemPtr& pItem)
	{
		TCustomTreeCtrlItemPtrVector children = pItem->GetChildren();
		for (CCustomTreeCtrlItemPtr pChild : children)
		{
			RemoveItemChildren(pChild);
			RemoveItem(pChild);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItemPtr CCustomTreeCtrl::FindItem(const char* text, const CCustomTreeCtrlItemPtr& pParentItem)
	{
		TCustomTreeCtrlItemPtrVector children;
		if (pParentItem)
		{
			children = pParentItem->GetChildren();
		}
		else
		{
			children = m_root->GetChildren();
		}
		for(TCustomTreeCtrlItemPtrVector::iterator iItem = children.begin(), iEndItem = children.end(); iItem != iEndItem; ++ iItem)
		{
			const CCustomTreeCtrlItemPtr&	pItem = *iItem;
			if(strcmp(pItem->GetText(), text) == 0)
			{
				return pItem;
			}
		}
		return CCustomTreeCtrlItemPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItemPtr CCustomTreeCtrl::FindItem(CPoint pos)
	{
		UINT			flags = 0;
		HTREEITEM	hTreeItem = CXTTreeCtrl::HitTest(pos, &flags);
		if(hTreeItem && (flags & TVHT_ONITEM))
		{
			return FindItem(hTreeItem);
		}
		return CCustomTreeCtrlItemPtr(); 
	}

	///////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItemPtr CCustomTreeCtrl::FindItem(HTREEITEM hTreeItem)
	{
		auto it = m_htreeitem_map.find(hTreeItem);
		if (it != m_htreeitem_map.end())
			return it->second;
		return CCustomTreeCtrlItemPtr();
	}

	///////////////////////////////////////////////////////////////////////
	CCustomTreeCtrlItemPtr CCustomTreeCtrl::FindItem(SGUID guid)
	{
		auto it = m_guid_map.find(guid);
		if (it != m_guid_map.end())
			return it->second;
		return CCustomTreeCtrlItemPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	TCustomTreeCtrlItemPtrVector& CCustomTreeCtrl::GetItems()
	{
		return m_items;
	}

	//////////////////////////////////////////////////////////////////////////
	const TCustomTreeCtrlItemPtrVector& CCustomTreeCtrl::GetItems() const
	{
		return m_items;
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::GetChildItems(CCustomTreeCtrlItem& item, TCustomTreeCtrlItemPtrVector &childItems)
	{
		auto children = item.GetChildren();
		childItems.insert(childItems.begin(), children.begin(), children.end());
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::Reset()
	{
		m_items.clear();
		m_htreeitem_map.clear();
		m_guid_map.clear();
		CXTTreeCtrl::DeleteAllItems();
	}

	//////////////////////////////////////////////////////////////////////////
	void CCustomTreeCtrl::LoadImageList(CImageList& imageList) {}

	//////////////////////////////////////////////////////////////////////////
	DROPEFFECT CCustomTreeCtrl::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		return DROPEFFECT_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CCustomTreeCtrl::OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	CCustomTreeCtrl::CDropTarget::CDropTarget(CCustomTreeCtrl& customTreeCtrl)
		: m_customTreeCtrl(customTreeCtrl)
	{}

	//////////////////////////////////////////////////////////////////////////
	DROPEFFECT CCustomTreeCtrl::CDropTarget::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		return m_customTreeCtrl.OnDragOver(pWnd, pDataObject, dwKeyState, point);
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CCustomTreeCtrl::CDropTarget::OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		return m_customTreeCtrl.OnDrop(pWnd, pDataObject, dropEffect, point);
	}
}
