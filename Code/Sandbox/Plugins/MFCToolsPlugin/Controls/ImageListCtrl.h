// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CImageListCtrl_h__
#define __CImageListCtrl_h__
#pragma once

#include "Util/Image.h"

struct PLUGIN_API CImageListCtrlItem : public _i_reference_target_t
{
	// Where to draw item.
	CString   text;
	CRect     rect;
	// Items image.
	CImageEx* pImage;
	CBitmap   bitmap;
	void*     pUserData;
	void*     pUserData2;
	bool      bBitmapValid;
	bool      bSelected;

	CImageListCtrlItem();
	virtual ~CImageListCtrlItem();
};

//////////////////////////////////////////////////////////////////////////
// CImageListCtrl styles.
//////////////////////////////////////////////////////////////////////////
enum EImageListCtrl
{
	ILC_STYLE_HORZ      = 0x0001,   // Horizontal only.
	ILC_STYLE_VERT      = 0x0002,   // Vertical only.
	ILC_MULTI_SELECTION = 0x0004,   // Multiple items can be selected.
};

//////////////////////////////////////////////////////////////////////////
// Custom control to display list of images.
//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CImageListCtrl : public CWnd
{
public:
	typedef std::vector<_smart_ptr<CImageListCtrlItem>> Items;

	CImageListCtrl();
	virtual ~CImageListCtrl();

	BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
	void SetItemSize(CSize size);
	void SetBorderSize(CSize size);

	// Get all items inside specified rectangle.
	void GetItemsInRect(const CRect& rect, Items& items) const;
	void GetAllItems(Items& items) const;

	// Test if point is within an item. return NULL if point is not hit any item.
	CImageListCtrlItem* HitTest(CPoint point);

	virtual bool        InsertItem(CImageListCtrlItem* pItem);
	virtual void        DeleteAllItems();

	virtual void        SelectItem(CImageListCtrlItem* pItem, bool bSelect = true);
	virtual void        SelectItem(int nIndex, bool bSelect = true);
	virtual void        ResetSelection();

	void                InvalidateItemRect(CImageListCtrlItem* pItem);
	void                InvalidateAllItems();

protected:
	afx_msg int          OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void         OnDestroy();
	afx_msg void         OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL         OnEraseBkgnd(CDC* pDC);
	afx_msg void         OnPaint();
	afx_msg void         OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void         OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg virtual void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void         OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void         OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void         OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void         OnRButtonUp(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

	virtual bool OnBeginDrag(UINT nFlags, CPoint point);
	virtual void OnDropItem(CPoint point)     {}
	virtual void OnDraggingItem(CPoint point) {}

	virtual void DrawControl(CDC* pDC, const CRect& rcUpdate);
	virtual void DrawItem(CDC* pDC, CImageListCtrlItem* pItem, bool bForceMoveToOrigin);
	virtual void CalcLayout(bool bUpdateScrollBar = true);

	virtual void OnUpdateItem(CImageListCtrlItem* pItem);
	virtual void OnSelectItem(CImageListCtrlItem* pItem, bool bSelected);
	void         InvalidateAllBitmaps();

	CPoint       GetDragPos(const CPoint& point);

	//////////////////////////////////////////////////////////////////////////
	// Member Variables
	//////////////////////////////////////////////////////////////////////////
	// Items.
	Items m_items;

	enum EMouseDragDropStatue
	{
		eMDDS_None,
		eMDDS_Begin,
		eMDDS_InTheMiddle
	};
	EMouseDragDropStatue           m_DragDropStatus;
	CPoint                         m_PointWhenHoldingLButton;
	CImageList                     m_DraggedImage;

	_smart_ptr<CImageListCtrlItem> m_pSelectedItem;

	CBitmap                        m_offscreenBitmap;
	CSize                          m_itemSize;
	CSize                          m_borderSize;
	CBrush                         m_bkgrBrush;
	CPoint                         m_scrollOffset;
	CPen                           m_selectedPen;
};

#endif //__CImageListCtrl_h__

