// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PluginAPI.h"
#include "Util/Variable.h"

// forward declarations.
class CPropertyItem;
class CVarBlock;
class CBitmapToolTip;

// This notification sent to parent after item in property control is selected.
#define  PROPERTYCTRL_ONSELECT (0x0001)

//////////////////////////////////////////////////////////////////////////
// Notification structure for property control messages to parent
//////////////////////////////////////////////////////////////////////////
struct CPropertyCtrlNotify
{
	NMHDR          hdr;
	CPropertyItem* pItem;
	IVariable*     pVariable;

	CPropertyCtrlNotify() : pItem(0), pVariable(0) {}
};

/** Custom control to handle Properties hierarchies.
 */
class PLUGIN_API CPropertyCtrl : public CWnd
{
	DECLARE_DYNAMIC(CPropertyCtrl)
public:
	typedef std::vector<CPropertyItem*> Items;

	// Flags of property control.
	enum Flags
	{
		F_VARIABLE_HEIGHT  = 0x0010,
		F_VS_DOT_NET_STYLE = 0x0020,  // Provides a look similar to Visual Studio.NET property grid.
		F_READ_ONLY        = 0x0040,  // Sets control to be read only, User cannot modify content of properties.
		F_GRAYED           = 0x0080,  // Control is grayed, but is not readonly.
		F_EXTENDED         = 0x0100,  // Extended control type.
		F_NOBITMAPS        = 0x0200,  // No left bitmaps.
	};

	enum EPropertyPopupActions
	{
		ePPA_Copy = 1,
		ePPA_CopyRecursively,
		ePPA_CopyAll,
		ePPA_Paste,
		ePPA_SwitchUI,
		ePPA_Revert,
		ePPA_CustomItemBase  = 10, // reserved from 10 to 99
		ePPA_CustomPopupBase = 100 // reserved from 100 to x*100+100 where x is size of m_customPopupMenuPopups
	};

	struct SCustomPopupItem
	{
		typedef Functor0 Callback;

		CString  m_text;
		Callback m_callback;

		SCustomPopupItem(const CString& text, const Functor0& callback)
			: m_text(text), m_callback(callback) {}
	};

	struct SCustomPopupMenu
	{
		typedef Functor1<int> Callback;

		CString              m_text;
		Callback             m_callback;
		std::vector<CString> m_subMenuText;

		SCustomPopupMenu(const CString& text, const Callback& callback, const std::vector<CString>& subMenuText)
			: m_text(text), m_callback(callback), m_subMenuText(subMenuText) {}
	};

	//! When item change, this callback fired with name of item.
	typedef Functor1<XmlNodeRef> UpdateCallback;
	//! When selection changes, this callback is fired with name of item.
	typedef Functor1<XmlNodeRef> SelChangeCallback;
	//! When item change, this callback fired variable that changed.
	typedef Functor1<IVariable*> UpdateVarCallback;
	//! When item change, update object.
	typedef Functor1<IVariable*> UpdateObjectCallback;
	//! For alternative undo.
	typedef Functor1<IVariable*> UndoCallback;

	CPropertyCtrl();
	virtual ~CPropertyCtrl();

	virtual void Create(DWORD dwStyle, const CRect& rc, CWnd* pParent = NULL, UINT nID = 0);

	//! Set control flags.
	//! @param flags @see Flags enum.
	void SetFlags(int flags) { m_nFlags = flags; };
	//! get control flags.
	int  GetFlags() const    { return m_nFlags; };

	/** Create Property items from root Xml node
	 */
	void CreateItems(XmlNodeRef& node);
	void CreateItems(XmlNodeRef& node, CVarBlockPtr& outBlockPtr, IVariable::OnSetCallback func);

	/** Delete all items from this control.
	 */
	void DeleteAllItems();

	/** Delete item and all its subitems.
	 */
	virtual void DeleteItem(CPropertyItem* pItem);

	/** Add more variables.
	    @param szCategory Name of category to place var block, if NULL do not create new category.
	    @return Root item where this var block was added.
	 */
	virtual CPropertyItem* AddVarBlock(CVarBlock* varBlock, const char* szCategory = NULL);

	/** Add more variables.
	   @param szCategory Name of category to place var block
	   @param pRoot is item where to add this var block
	   @param bBoldRoot makes root item bold
	 */
	virtual CPropertyItem* AddVarBlockAt(CVarBlock* varBlock, const char* szCategory, CPropertyItem* pRoot, bool bFirstVarIsRoot = false);

	// It doesn't add any property item, but instead it updates all property items with
	// a new variable block (sets display value, flags and user data).
	virtual void UpdateVarBlock(CVarBlock* pVarBlock);

	// Replace category item contents with the specified var block.
	virtual void ReplaceVarBlock(CPropertyItem* pCategoryItem, CVarBlock* varBlock);

	/** Set update callback to be used for this property window.
	 */
	void SetUpdateCallback(UpdateCallback& callback) { m_updateFunc = callback; }

	/** Set update callback to be used for this property window.
	 */
	void SetUpdateCallback(UpdateVarCallback& callback) { m_updateVarFunc = callback; }

	/** Set update callback to be used for this property window.
	 */
	void SetUpdateObjectCallback(UpdateObjectCallback& callback) { m_updateObjectFunc = callback; }
	void ClearUpdateObjectCallback()                             { m_updateObjectFunc = 0; }

	/** Enable of disable calling update callback when some values change.
	 */
	bool EnableUpdateCallback(bool bEnable);

	/** Set alternative undo callback.
	 */
	void SetUndoCallback(UndoCallback& callback) { m_undoFunc = callback; }
	void ClearUndoCallback()                     { m_undoFunc = 0; }

	bool CallUndoFunc(CPropertyItem* item);

	/** Set selchange callback to be used for this property window.
	 */
	void SetSelChangeCallback(SelChangeCallback& callback) { m_selChangeFunc = callback; }

	/** Enable of disable calling selchange callback when the selection changes.
	 */
	bool EnableSelChangeCallback(bool bEnable);

	/** Expand all categories.
	 */
	virtual void ExpandAll();

	/** Collapse all categories.
	 */
	virtual void CollapseAll();

	/** Expand all childs of specified item.
	 */
	virtual void ExpandAllChilds(CPropertyItem* item, bool bRecursive);

	/** Expand the specified variable and they all childs.
	 */
	virtual void ExpandVariableAndChilds(IVariable* varBlock, bool bRecursive);

	// Remove all child items of this property item.
	virtual void RemoveAllChilds(CPropertyItem* pItem);

	//! Expend this item
	virtual void Expand(CPropertyItem* item, bool bExpand, bool bRedraw = true);

	//! Toggle this item
	void Toggle(CPropertyItem* item);

	/** Get pointer to root item
	 */
	CPropertyItem* GetRootItem() const { return m_root; };

	/**  Reload values back from xml nodes.
	 */
	virtual void ReloadValues();

	/** Change splitter value.
	 */
	void SetSplitter(int splitter) { m_splitter = splitter; };

	/** Get current value of splitter.
	 */
	int GetSplitter() const { return m_splitter; };

	/** Enable/Disable the splitter repositioning itself when resizing the control.
	 */
	void SetAutoUpdateSplitter(bool autoUpdateSplitter) { m_autoUpdateSplitter = autoUpdateSplitter; }

	/** Get total height of all visible items.
	 */
	virtual int  GetVisibleHeight();

	static void  RegisterWindowClass();

	virtual void OnItemChange(CPropertyItem* item);

	// Ovveride method defined in CWnd.
	BOOL EnableWindow(BOOL bEnable = TRUE);

	//! When set to true will only display values of modified parameters.
	void  SetDisplayOnlyModified(bool bEnable) { m_bDisplayOnlyModified = bEnable; };

	CRect GetItemValueRect(const CRect& rect);
	void  GetItemRect(CPropertyItem* item, CRect& rect);

	//! Set height of item, (When F_VARIABLE_HEIGHT flag is set, this value is ignored)
	void SetItemHeight(int nItemHeight);

	//! Get height of item.
	int            GetItemHeight(CPropertyItem* item) const;

	void           ClearSelection();

	CPropertyItem* GetSelectedItem() { return m_selected; }

	void           SetRootName(const CString& rootName);

	//! Force modification unconditional to all property items even though the value isn't changed.
	void EnableNotifyWithoutValueChange(bool bFlag);

	//! Find item that reference specified property.
	CPropertyItem*         FindItemByVar(IVariable* pVar);

	void                   GetVisibleItems(CPropertyItem* root, Items& items);
	bool                   IsCategory(CPropertyItem* item);

	virtual CPropertyItem* GetItemFromPoint(CPoint point);
	virtual void           SelectItem(CPropertyItem* item);

	void                   MultiSelectItem(CPropertyItem* pItem);
	void                   MultiUnselectItem(CPropertyItem* pItem);
	void                   MultiSelectRange(CPropertyItem* pAnchorItem);
	void                   MultiClearAll();

	// only shows items containing the string in their name. All items shown if string is empty.
	void RestrictToItemsContaining(const CString& searchName);

	bool IsReadOnly();
	bool IsGrayed();
	bool IsExtenedUI() const { return (m_nFlags & F_EXTENDED) != 0; };

	void RemoveAllItems();

	// Forces all user input from in place controls back to the property item, and destroy in place control.
	void Flush();

	// Set Height for Font, if 0 - default height;
	void SetFontHeight(int nFontHeight)              { m_nFontHeight = nFontHeight; }

	void SetStoreUndoByItems(bool bStoreUndoByItems) { m_bStoreUndoByItems = bStoreUndoByItems; }
	bool IsStoreUndoByItems() const                  { return m_bStoreUndoByItems; }

	void CopyItem(XmlNodeRef rootNode, CPropertyItem* pItem, bool bRecursively);

	// set to false if you don't want to receive callbacks when the item is not modified (when items are expanded etc)
	void SetCallbackOnNonModified(bool bEnable) { m_bSendCallbackOnNonModified = bEnable; }

	void AddCustomPopupMenuItem(const CString& text, const SCustomPopupItem::Callback handler)
	{ m_customPopupMenuItems.push_back(SCustomPopupItem(text, handler)); }
	void AddCustomPopupMenuPopup(const CString& text, const SCustomPopupMenu::Callback handler, const std::vector<CString>& subMenuText)
	{ m_customPopupMenuPopups.push_back(SCustomPopupMenu(text, handler, subMenuText)); }

	void RemoveCustomPopupMenuItem(const CString& text);
	void RemoveCustomPopupMenuPopup(const CString& text);

protected:
	friend CPropertyItem;

	virtual void UpdateVarBlock(CPropertyItem* pPropertyItem, IVariableContainer* pSourceContainer, IVariableContainer* pTargetContainer);

	void         SendNotify(int code, CPropertyCtrlNotify& notify);
	void         DrawItem(CPropertyItem* item, CDC& dc, CRect& itemRect);
	int          CalcOffset(CPropertyItem* item);
	void         DrawSign(CDC& dc, CPoint point, bool plus);

	virtual void CreateInPlaceControl();
	virtual void DestroyControls(CPropertyItem* pItem);
	bool         IsOverSplitter(CPoint point);
	void         ProcessTooltip(CPropertyItem* item);

	void         CalcLayout();
	void         Init();

	// Called when control items are invalidated.
	virtual void InvalidateCtrl();
	virtual void InvalidateItems();
	virtual void InvalidateItem(CPropertyItem* pItem);
	virtual void SwitchUI();

	void         OnCopy(bool bRecursively);
	void         OnCopyAll();
	void         OnPaste();
	void         ShowBitmapTooltip(const CString& imageFilename, CPoint point, CWnd* pToolWnd, const CRect& toolRc);
	void         HideBitmapTooltip();

	DECLARE_MESSAGE_MAP()

	afx_msg UINT OnGetDlgCode();
	afx_msg void OnDestroy();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg LRESULT OnGetFont(WPARAM wParam, LPARAM);
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	//////////////////////////////////////////////////////////////////////////
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PreSubclassWindow();
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	//////////////////////////////////////////////////////////////////////////

	template<typename T> void RemoveCustomPopup(const CString& text, T& customPopup);

	_smart_ptr<CPropertyItem>      m_root;
	XmlNodeRef                    m_xmlRoot;
	bool                          m_bEnableCallback;
	UpdateCallback                m_updateFunc;
	bool                          m_bEnableSelChangeCallback;
	SelChangeCallback             m_selChangeFunc;
	UpdateVarCallback             m_updateVarFunc;
	UpdateObjectCallback          m_updateObjectFunc;
	UndoCallback                  m_undoFunc;

	CImageList                    m_icons;

	_smart_ptr<CPropertyItem>      m_selected;
	CBitmap                       m_offscreenBitmap;

	CPropertyItem*                m_prevTooltipItem;
	std::vector<CPropertyItem*>   m_multiSelectedItems;

	HCURSOR                       m_leftRightCursor;
	int                           m_splitter;
	bool                          m_autoUpdateSplitter;

	CPoint                        m_mouseDownPos;
	bool                          m_bSplitterDrag;

	CPoint                        m_scrollOffset;

	CToolTipCtrl                  m_tooltip;
	std::auto_ptr<CBitmapToolTip> m_pBitmapTooltip;

	CFont*                        m_pBoldFont;

	//! When set to true will only display values of modified items.
	bool m_bDisplayOnlyModified;

	//! Timer to track loose of focus.
	int m_nTimer;

	//! Item height.
	int m_nItemHeight;

	//! Control custom flags.
	int                            m_nFlags;
	bool                           m_bLayoutChange;
	bool                           m_bLayoutValid;

	static std::map<CString, bool> m_expandHistory;

	bool                           m_bIsCanExtended;

	CString                        m_sNameRestriction;
	_smart_ptr<CVarBlock>           m_pVarBlock;

	CFont                          m_pHeightFont;
	int                            m_nFontHeight;

	bool                           m_bStoreUndoByItems;

	std::vector<SCustomPopupItem>  m_customPopupMenuItems;
	std::vector<SCustomPopupMenu>  m_customPopupMenuPopups;

	bool                           m_bSendCallbackOnNonModified;
};

