// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TREECONTROLUTILS_H__
#define __TREECONTROLUTILS_H__
#include <iterator>

namespace TreeCtrlUtils
{
template<typename P> class TreeItemIterator : public P
{
public:
	typedef P Traits;

	//iterator traits, required by STL
	typedef ptrdiff_t                 difference_type;
	typedef HTREEITEM                 value_type;
	typedef HTREEITEM*                pointer;
	typedef HTREEITEM&                reference;
	typedef std::forward_iterator_tag iterator_category;

	TreeItemIterator() : pCtrl(0), hItem(0) {}
	explicit TreeItemIterator(const P& traits) : P(traits), pCtrl(0), hItem(0) {}
	TreeItemIterator(const TreeItemIterator& other) : P(other), pCtrl(other.pCtrl), hItem(other.hItem) {}
	TreeItemIterator(CTreeCtrl* pCtrl, HTREEITEM hItem) : pCtrl(pCtrl), hItem(hItem) {}
	TreeItemIterator(CTreeCtrl* pCtrl, HTREEITEM hItem, const P& traits) : P(traits), pCtrl(pCtrl), hItem(hItem) {}

	HTREEITEM         operator*()                                     { return hItem; }
	bool              operator==(const TreeItemIterator& other) const { return pCtrl == other.pCtrl && hItem == other.hItem; }
	bool              operator!=(const TreeItemIterator& other) const { return pCtrl != other.pCtrl || hItem != other.hItem; }

	TreeItemIterator& operator++()
	{
		HTREEITEM hNextItem = 0;
		if (RecurseToChildren(hItem))
			hNextItem = (pCtrl ? pCtrl->GetChildItem(hItem) : 0);
		while (pCtrl && hItem && !hNextItem)
		{
			hNextItem = pCtrl->GetNextSiblingItem(hItem);
			if (!hNextItem)
				hItem = pCtrl->GetParentItem(hItem);
		}
		hItem = hNextItem;

		return *this;
	}

	TreeItemIterator operator++(int) { TreeItemIterator old = *this; ++(*this); return old; }

	CTreeCtrl* pCtrl;
	HTREEITEM  hItem;
};

class NonRecursiveTreeItemIteratorTraits
{
public:
	bool RecurseToChildren(HTREEITEM hItem) { return false; }
};
typedef TreeItemIterator<NonRecursiveTreeItemIteratorTraits> NonRecursiveTreeItemIterator;

class RecursiveTreeItemIteratorTraits
{
public:
	bool RecurseToChildren(HTREEITEM hItem) { return true; }
};
typedef TreeItemIterator<RecursiveTreeItemIteratorTraits> RecursiveTreeItemIterator;

inline RecursiveTreeItemIterator BeginTreeItemsRecursive(CTreeCtrl* pCtrl, HTREEITEM hItem = 0)
{
	if (hItem == 0)
		hItem = (pCtrl ? pCtrl->GetRootItem() : 0);
	return RecursiveTreeItemIterator(pCtrl, hItem);
}

inline RecursiveTreeItemIterator EndTreeItemsRecursive(CTreeCtrl* pCtrl, HTREEITEM hItem = 0)
{
	HTREEITEM hEndItem = 0;
	HTREEITEM hParent = hItem;
	do
	{
		if (hParent)
			hEndItem = pCtrl->GetNextSiblingItem(hParent);
		hParent = (pCtrl && hParent ? pCtrl->GetParentItem(hParent) : 0);
	}
	while (hParent && !hEndItem);
	return RecursiveTreeItemIterator(pCtrl, hEndItem);
}

inline NonRecursiveTreeItemIterator BeginTreeItemsNonRecursive(CTreeCtrl* pCtrl, HTREEITEM hItem = 0)
{
	if (hItem == 0)
		hItem = (pCtrl ? pCtrl->GetRootItem() : 0);
	if (hItem)
		hItem = pCtrl->GetChildItem(hItem);
	return NonRecursiveTreeItemIterator(pCtrl, hItem);
}

inline NonRecursiveTreeItemIterator EndTreeItemsNonRecursive(CTreeCtrl* pCtrl, HTREEITEM hItem = 0)
{
	HTREEITEM hEndItem = 0;
	HTREEITEM hParent = 0;
	while (hParent && !hEndItem)
	{
		hParent = (pCtrl && hItem ? pCtrl->GetParentItem(hItem) : 0);
		if (hParent)
			hEndItem = pCtrl->GetNextSiblingItem(hParent);
	}
	return NonRecursiveTreeItemIterator(pCtrl, hEndItem);
}

template<typename T, typename P> class TreeItemDataIterator
{
public:
	typedef T                   Type;
	typedef TreeItemIterator<P> InternalIterator;

	//iterator traits, required by STL
	typedef ptrdiff_t                 difference_type;
	typedef Type*                     value_type;
	typedef Type**                    pointer;
	typedef Type*&                    reference;
	typedef std::forward_iterator_tag iterator_category;

	TreeItemDataIterator() {}
	TreeItemDataIterator(const TreeItemDataIterator& other) : iterator(other.iterator) { AdvanceToValidIterator(); }
	explicit TreeItemDataIterator(const InternalIterator& iterator) : iterator(iterator) { AdvanceToValidIterator(); }

	Type*                 operator*()                                         { return reinterpret_cast<Type*>(iterator.pCtrl->GetItemData(iterator.hItem)); }
	bool                  operator==(const TreeItemDataIterator& other) const { return iterator == other.iterator; }
	bool                  operator!=(const TreeItemDataIterator& other) const { return iterator != other.iterator; }

	HTREEITEM             GetTreeItem()                                       { return iterator.hItem; }

	TreeItemDataIterator& operator++()
	{
		++iterator;
		AdvanceToValidIterator();
		return *this;
	}

	TreeItemDataIterator operator++(int) { TreeItemDataIterator old = *this; ++(*this); return old; }

private:
	void AdvanceToValidIterator()
	{
		while (iterator.pCtrl && iterator.hItem && !iterator.pCtrl->GetItemData(iterator.hItem))
			++iterator;
	}

	InternalIterator iterator;
};

template<typename T> class RecursiveItemDataIteratorType
{
public: typedef TreeItemDataIterator<T, RecursiveTreeItemIteratorTraits> type;
};
template<typename T> inline TreeItemDataIterator<T, RecursiveTreeItemIteratorTraits> BeginTreeItemDataRecursive(CTreeCtrl* pCtrl, HTREEITEM hItem = 0)
{
	return TreeItemDataIterator<T, RecursiveTreeItemIteratorTraits>(BeginTreeItemsRecursive(pCtrl, hItem));
}

template<typename T> inline TreeItemDataIterator<T, RecursiveTreeItemIteratorTraits> EndTreeItemDataRecursive(CTreeCtrl* pCtrl, HTREEITEM hItem = 0)
{
	return TreeItemDataIterator<T, RecursiveTreeItemIteratorTraits>(EndTreeItemsRecursive(pCtrl, hItem));
}

template<typename T> class NonRecursiveItemDataIteratorType
{
	typedef TreeItemDataIterator<T, NonRecursiveTreeItemIteratorTraits> type;
};
template<typename T> inline TreeItemDataIterator<T, NonRecursiveTreeItemIteratorTraits> BeginTreeItemDataNonRecursive(CTreeCtrl* pCtrl, HTREEITEM hItem = 0)
{
	return TreeItemDataIterator<T, NonRecursiveTreeItemIteratorTraits>(BeginTreeItemsNonRecursive(pCtrl, hItem));
}

template<typename T> inline TreeItemDataIterator<T, NonRecursiveTreeItemIteratorTraits> EndTreeItemDataNonRecursive(CTreeCtrl* pCtrl, HTREEITEM hItem = 0)
{
	return TreeItemDataIterator<T, NonRecursiveTreeItemIteratorTraits>(EndTreeItemsNonRecursive(pCtrl, hItem));
}

class SelectedTreeItemIterator
{
public:
	SelectedTreeItemIterator() : pCtrl(0), hItem(0) {}
	SelectedTreeItemIterator(const SelectedTreeItemIterator& other) : pCtrl(other.pCtrl), hItem(other.hItem) {}
	SelectedTreeItemIterator(CXTTreeCtrl* pCtrl, HTREEITEM hItem) : pCtrl(pCtrl), hItem(hItem) {}

	HTREEITEM                 operator*()                                             { return hItem; }
	bool                      operator==(const SelectedTreeItemIterator& other) const { return pCtrl == other.pCtrl && hItem == other.hItem; }
	bool                      operator!=(const SelectedTreeItemIterator& other) const { return pCtrl != other.pCtrl || hItem != other.hItem; }

	SelectedTreeItemIterator& operator++()
	{
		hItem = (pCtrl ? pCtrl->GetNextSelectedItem(hItem) : 0);

		return *this;
	}

	SelectedTreeItemIterator operator++(int) { SelectedTreeItemIterator old = *this; ++(*this); return old; }

	CXTTreeCtrl* pCtrl;
	HTREEITEM    hItem;
};

SelectedTreeItemIterator BeginSelectedTreeItems(CXTTreeCtrl* pCtrl)
{
	return SelectedTreeItemIterator(pCtrl, (pCtrl ? pCtrl->GetFirstSelectedItem() : 0));
}

SelectedTreeItemIterator EndSelectedTreeItems(CXTTreeCtrl* pCtrl)
{
	return SelectedTreeItemIterator(pCtrl, 0);
}

template<typename T> class SelectedTreeItemDataIterator
{
public:
	typedef T                        Type;
	typedef SelectedTreeItemIterator InternalIterator;

	SelectedTreeItemDataIterator() {}
	SelectedTreeItemDataIterator(const SelectedTreeItemDataIterator& other) : iterator(other.iterator) { AdvanceToValidIterator(); }
	explicit SelectedTreeItemDataIterator(const InternalIterator& iterator) : iterator(iterator) { AdvanceToValidIterator(); }

	Type*                         operator*()                                                 { return reinterpret_cast<Type*>(iterator.pCtrl->GetItemData(iterator.hItem)); }
	bool                          operator==(const SelectedTreeItemDataIterator& other) const { return iterator == other.iterator; }
	bool                          operator!=(const SelectedTreeItemDataIterator& other) const { return iterator != other.iterator; }

	HTREEITEM                     GetTreeItem()                                               { return iterator.hItem; }

	SelectedTreeItemDataIterator& operator++()
	{
		++iterator;
		AdvanceToValidIterator();
		return *this;
	}

	SelectedTreeItemDataIterator operator++(int) { SelectedTreeItemDataIterator old = *this; ++(*this); return old; }

private:
	void AdvanceToValidIterator()
	{
		while (iterator.pCtrl && iterator.hItem && !iterator.pCtrl->GetItemData(iterator.hItem))
			++iterator;
	}

	InternalIterator iterator;
};

template<typename T> SelectedTreeItemDataIterator<T> BeginSelectedTreeItemData(CXTTreeCtrl* pCtrl)
{
	return SelectedTreeItemDataIterator<T>(BeginSelectedTreeItems(pCtrl));
}

template<typename T> SelectedTreeItemDataIterator<T> EndSelectedTreeItemData(CXTTreeCtrl* pCtrl)
{
	return SelectedTreeItemDataIterator<T>(EndSelectedTreeItems(pCtrl));
}
}

#endif //__TREECONTROLUTILS_H__

