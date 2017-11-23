// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/BoostHelpers.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ArrayView.h>

namespace Schematyc2
{
	typedef TemplateUtils::CArrayView<const size_t> TSizeTConstArray;

	class CCustomTreeCtrlItem;

	DECLARE_SHARED_POINTERS(CCustomTreeCtrlItem)

	typedef std::vector<CCustomTreeCtrlItemPtr> TCustomTreeCtrlItemPtrVector;

	class CCustomTreeCtrlItem
	{
		friend class CCustomTreeCtrl;

	public:

		CCustomTreeCtrlItem(const char* text, size_t icon, const CCustomTreeCtrlItemPtr& pParent, const SGUID& guid);
		virtual ~CCustomTreeCtrlItem();

		const char* GetText() const;
		size_t GetIcon() const;
		CCustomTreeCtrlItemPtr GetParent() const;
		CCustomTreeCtrlItemPtr FindAncestor(const char* text) const;
		CCustomTreeCtrlItemPtr FindAncestor(size_t icon) const;
		CCustomTreeCtrlItemPtr FindAncestor(const TSizeTConstArray& icons) const;
		void SetHTreeItem(HTREEITEM hTreeItem);
		HTREEITEM GetHTreeItem() const;
		const SGUID& GetGUID() const;

		void AddChild(CCustomTreeCtrlItemPtr pItem);
		const TCustomTreeCtrlItemPtrVector& GetChildren();
		void RemoveChild(CCustomTreeCtrlItemPtr pItem);

	private:

		void SetText(const char* text);

		string									m_text;
		size_t									m_icon;
		CCustomTreeCtrlItemPtr	m_pParent;		// #SchematycTODO : Shouldn't this be a weak pointer?
		SGUID m_guid;
		TCustomTreeCtrlItemPtrVector			m_children;
		HTREEITEM								m_hTreeItem;
	};

	typedef std::function<bool(const CCustomTreeCtrlItemPtr&, const CCustomTreeCtrlItemPtr&)> TCustomTreeCtrlItemCompFunc;

	class CCustomTreeCtrl : public CXTTreeCtrl
	{
	public:

		virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		void ExpandItem(const CCustomTreeCtrlItemPtr& pItem);
		void EnsureVisible(const CCustomTreeCtrlItemPtr& pItem);

	protected:
		CCustomTreeCtrl(TCustomTreeCtrlItemCompFunc itemCompare = NULL);
		virtual ~CCustomTreeCtrl();
		
		void AddItem(const CCustomTreeCtrlItemPtr& pItem);
		void SetItemText(const CCustomTreeCtrlItemPtr& pItem, const char* text);
		void SetItemColor(const CCustomTreeCtrlItemPtr& pItem, COLORREF color);
		void SetItemBold(const CCustomTreeCtrlItemPtr& pItem, bool bBold);
		void RemoveItem(const CCustomTreeCtrlItemPtr& pItem);
		void RemoveItemChildren(const CCustomTreeCtrlItemPtr& pItem);
		CCustomTreeCtrlItemPtr FindItem(const char* text, const CCustomTreeCtrlItemPtr& pParentItem = CCustomTreeCtrlItemPtr());
		CCustomTreeCtrlItemPtr FindItem(CPoint pos);
		CCustomTreeCtrlItemPtr FindItem(HTREEITEM hTreeItem);
		CCustomTreeCtrlItemPtr FindItem(SGUID guid);
		TCustomTreeCtrlItemPtrVector& GetItems();
		const TCustomTreeCtrlItemPtrVector& GetItems() const;
		void GetChildItems(CCustomTreeCtrlItem& item, TCustomTreeCtrlItemPtrVector &childItems);
		void Reset();

		virtual void LoadImageList(CImageList& imageList);
		virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
		virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);

	private:

		class CDropTarget : public COleDropTarget
		{
		public:

			CDropTarget(CCustomTreeCtrl& customTreeCtrl);

			// COleDropTarget
			virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
			virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
			// ~COleDropTarget

		private:

			CCustomTreeCtrl&	m_customTreeCtrl;
		};

		TCustomTreeCtrlItemCompFunc m_itemCompare;
		CImageList										m_imageList;
		TCustomTreeCtrlItemPtrVector	m_items;
		std::map<HTREEITEM, CCustomTreeCtrlItemPtr> m_htreeitem_map;
		std::map<SGUID, CCustomTreeCtrlItemPtr> m_guid_map;
		CDropTarget										m_dropTarget;
		CCustomTreeCtrlItemPtr m_root;
	};
}
