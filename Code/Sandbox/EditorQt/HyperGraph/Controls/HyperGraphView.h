// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <GdiPlus.h>

#include "HyperGraph/HyperGraph.h"

#include "Controls/PropertyCtrl.h"
#include "UserMessageDefines.h"

class CHyperGraphView;
class CHyperGraphNode;

//////////////////////////////////////////////////////////////////////////
class CHyperGraphViewRenameEdit : public CEdit
{
public:
	DECLARE_MESSAGE_MAP()

public:
	void SetView(CHyperGraphView* pView) { m_pView = pView; };

protected:
	afx_msg UINT OnGetDlgCode() { return CEdit::OnGetDlgCode() | DLGC_WANTALLKEYS | DLGC_WANTMESSAGE; }
	afx_msg void OnKillFocus(CWnd* pWnd);
	afx_msg void OnEditKillFocus();

	virtual BOOL PreTranslateMessage(MSG* pMsg);

	CHyperGraphView* m_pView;
};

class CEditPropertyCtrl : public CPropertyCtrl
{
public:
	void SetView(CHyperGraphView* pView) { m_pView = pView; };
	void SelectItem(CPropertyItem* item);
	void Expand(CPropertyItem* item, bool bExpand, bool bRedraw);

	DECLARE_MESSAGE_MAP()
protected:
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

private:
	CHyperGraphView* m_pView;
};

class CHyperGraphViewQuickSearchEdit : public CEdit
{
public:
	DECLARE_MESSAGE_MAP()

public:
	void SetView(CHyperGraphView* pView) { m_pView = pView; };

protected:
	afx_msg UINT OnGetDlgCode() { return CEdit::OnGetDlgCode() | DLGC_WANTALLKEYS | DLGC_WANTMESSAGE; }
	afx_msg void OnKillFocus(CWnd* pWnd);
	afx_msg void OnSearchKillFocus();

	virtual BOOL PreTranslateMessage(MSG* pMsg);

	CHyperGraphView* m_pView;
};

//////////////////////////////////////////////////////////////////////////
//
// CHyperGraphView is a window where all graph nodes are drawn on.
// View also provides an user interface to manipulate these nodes.
//
//////////////////////////////////////////////////////////////////////////
class CHyperGraphView : public CView, public IHyperGraphListener
{
	DECLARE_DYNAMIC(CHyperGraphView)
public:
	CHyperGraphView();
	virtual ~CHyperGraphView();

	// Creates view window.
	BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);

	// Assign graph to the view.
	void         SetGraph(CHyperGraph* pGraph);
	CHyperGraph* GetGraph() const { return m_pGraph; }

	void         SetPropertyCtrl(CPropertyCtrl* propsWnd);

	// Transform local rectangle into the view rectangle.
	CRect           LocalToViewRect(const Gdiplus::RectF& localRect) const;
	Gdiplus::RectF  ViewToLocalRect(const CRect& viewRect) const;

	CPoint          LocalToView(Gdiplus::PointF point);
	Gdiplus::PointF ViewToLocal(CPoint point);

	THyperNodePtr   CreateNode(const CString& sNodeClass, CPoint point);

	template <class T> 
	_smart_ptr<T>   CreateNode(CPoint point)
	{
		return static_cast<T*>(CreateNode(T::GetClassType(), point).get());
	}

	// Retrieve all nodes in give view rectangle, return true if any found.
	bool GetNodesInRect(const CRect& viewRect, std::multimap<int, CHyperNode*>& nodes, bool bFullInside = false);
	// Retrieve all selected nodes in graph, return true if any selected.
	enum SelectionSetType
	{
		SELECTION_SET_ONLY_PARENTS,
		SELECTION_SET_NORMAL,
		SELECTION_SET_INCLUDE_RELATIVES
	};
	bool GetSelectedNodes(std::vector<CHyperNode*>& nodes, SelectionSetType setType = SELECTION_SET_NORMAL);

	void ClearSelection();

	// scroll/zoom pToNode (of current graph) into view and optionally select it
	void ShowAndSelectNode(CHyperNode* pToNode, bool bSelect = true);

	// Invalidates hypergraph view.
	void InvalidateView(bool bComplete = false);

	void FitFlowGraphToView();

	//////////////////////////////////////////////////////////////////////////
	// IHyperGraphListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnHyperGraphEvent(IHyperNode* pNode, EHyperGraphEvent event);
	//////////////////////////////////////////////////////////////////////////

	void        SetComponentViewMask(uint32 mask) { m_componentViewMask = mask; }

	CHyperNode* GetNodeAtPoint(CPoint point);
	CHyperNode* GetMouseSensibleNodeAtPoint(CPoint point);

	void        ShowEditPort(CHyperNode* pNode, CHyperNodePort* pPort);
	void        HideEditPort();
	void        UpdateFrozen();

	// Centers the view over an specific node.
	void CenterViewAroundNode(CHyperNode* poNode, bool boFitZoom = false, float fZoom = -1.0f);

	void SetIgnoreHyperGraphEvents(bool ignore) { m_bIgnoreHyperGraphEvents = ignore; }
	void PropagateChangesToGroupsAndPrefabs();

protected:
	DECLARE_MESSAGE_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnDraw(CDC* pDC) {}
	virtual void PostNcDestroy();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	afx_msg void OnCommandDelete();
	afx_msg void OnCommandDeleteKeepLinks();
	afx_msg void OnCommandCopy();
	afx_msg void OnCommandPaste();
	afx_msg void OnCommandPasteWithLinks();
	afx_msg void OnCommandCut();
	afx_msg void OnCommandFitAll();
	afx_msg void OnUndo();
	afx_msg void OnRedo();
	afx_msg void OnSelectEntity();
	afx_msg void OnToggleMinimize();

	virtual void ShowContextMenu(CPoint point, CHyperNode* pNode);
	virtual void UpdateTooltip(CHyperNode* pNode, CHyperNodePort* pPort);

	void         OnMouseMoveScrollMode(UINT nFlags, CPoint point);
	void         OnMouseMovePortDragMode(UINT nFlags, CPoint point);
	void         OnMouseMoveBorderDragMode(UINT nFlags, CPoint point);

	// this function is called before the common rename edit box shows up
	// return true if you handled the rename by yourself,
	// return false if it should bring up the common edit box
	virtual bool HandleRenameNode(CHyperNode* pNode) { return false; }
	virtual void OnNodesConnected()                  {}
	virtual void OnEdgeRemoved()                     {}
	virtual void OnNodeRenamed()                     {}

	void         InternalPaste(bool bWithLinks, CPoint point, std::vector<CHyperNode*>* pPastedNodes = nullptr);
	void         RenameNode(CHyperNode* pNode);

private:
	// Draw graph nodes in rectangle.
	void           DrawNodes(Gdiplus::Graphics& gr, const CRect& rc);
	void           DrawEdges(Gdiplus::Graphics& gr, const CRect& rc);
	// return the helper rectangle
	Gdiplus::RectF DrawArrow(Gdiplus::Graphics& gr, CHyperEdge* pEdge, bool helper, int selection = 0);
	void           ReroutePendingEdges();
	void           RerouteAllEdges();

	//////////////////////////////////////////////////////////////////////////
	void MoveSelectedNodes(CPoint offset);

	void DrawGrid(Gdiplus::Graphics& gr, const CRect& updateRect);

	struct SClassMenuGroup
	{
		SClassMenuGroup() : pMenu(new CMenu()) { VERIFY(pMenu->CreatePopupMenu()); }

		SClassMenuGroup* CreateSubMenu()
		{
			lSubMenus.push_back(SClassMenuGroup());
			return &(*lSubMenus.rbegin());
		}

		DECLARE_SHARED_POINTERS(CMenu);
		CMenuPtr                   pMenu;
		std::list<SClassMenuGroup> lSubMenus;
	};

	void PopulateClassMenu(SClassMenuGroup& classMenu, std::vector<CString>& classes);
	void PopulateSubClassMenu(SClassMenuGroup& classMenu, std::vector<CString>& classes, int baseIndex);
	void ShowPortsConfigMenu(CPoint point, bool bInputs, CHyperNode* pNode);
	void AddEntityMenu(CHyperNode* pNode, CMenu& menu);
	void HandleEntityMenu(CHyperNode* pNode, const uint entry);
	void UpdateNodeEntity(CHyperNode* pNode, const uint cmd);
	void AddPrefabMenu(CHyperNode* pNode, CMenu& menu);
	void HandlePrefabMenu(CHyperNode* pNode, const uint entry);
	void AddGameTokenMenu(CMenu& menu);
	void HandleGameTokenMenu(IFlowGraph* pGraph, const CString& tokenName, const uint cmd);
	void AddModuleMenu(CHyperNode* pNode, CMenu& menu);
	void HandleModuleMenu(CHyperNode* pNode, const uint entry);
	void HandleGotoModule(const string &moduleGraphName);
	void HandleEditModulePortsFromNode(const string &moduleGraphName);
	void AddSelectionMenu(CMenu& menu, CMenu& selectionMenu);

	void InvalidateEdgeRect(CPoint p1, CPoint p2);
	void InvalidateEdge(CHyperEdge* pEdge);
	void InvalidateNode(CHyperNode* pNode, bool bRedraw = false, bool bIsModified = true);
	void ForceRedraw();

	//////////////////////////////////////////////////////////////////////////
	void CaptureMouse();
	void ReleaseMouse();
	bool CheckVirtualKey(int virtualKey);
	void UpdateWorkingArea();
	void SetScrollExtents(bool isUpdateWorkingArea = true);
	//////////////////////////////////////////////////////////////////////////

	void OnAcceptRename();
	void OnCancelRename();
	void QuickSearchNode(CHyperNode* pNode);
	void OnAcceptSearch();
	void OnCancelSearch();
	void OnSearchNodes();
	void OnSelectNextSearchResult(bool bUp);
	void OnSelectionChange();
	void UpdateNodeProperties(CHyperNode* pNode, IVariable* pVar);
	void OnUpdateProperties(IVariable* pVar);
	void OnToggleMinimizeNode(CHyperNode* pNode);

	bool HitTestEdge(CPoint pnt, CHyperEdge*& pEdge, int& nIndex);

	// Simulate a flow (trigger input of target node)
	void SimulateFlow(CHyperEdge* pEdge);
	void UpdateDebugCount(CHyperNode* pNode);

	void SetZoom(float zoom);
	void NotifyZoomChangeToNodes();

	void CopyLinks(CHyperNode* pNode, bool isInput);
	void PasteLinks(CHyperNode* pNode, bool isInput);
	void DeleteLinks(CHyperNode* pNode, bool isInput);

	void SetupLayerList(CVarBlock* pVarBlock);

	bool QuickSearchMatch(std::vector<CString> tokens, CString name);
private:
	friend CHyperGraphViewRenameEdit;
	friend CHyperGraphViewQuickSearchEdit;
	enum EOperationMode
	{
		NothingMode = 0,
		ScrollZoomMode,
		ScrollMode,
		ZoomMode,
		MoveMode,
		SelectMode,
		PortDragMode,
		EdgePointDragMode,
		ScrollModeDrag,
		NodeBorderDragMode,
		DragQuickSearchMode,
	};

	bool           m_bRefitGraphInOnSize;
	CPoint         m_RButtonDownPos;
	CPoint         m_mouseDownPos;
	CPoint         m_scrollOffset;
	CRect          m_rcSelect;
	EOperationMode m_mode;

	// Port we are dragging now.
	std::vector<_smart_ptr<CHyperEdge>> m_draggingEdges;
	bool                                m_bDraggingFixedOutput;
	bool                                m_bEdgeWasJustDisconnected;

	_smart_ptr<CHyperEdge>              m_pHitEdge;
	int m_nHitEdgePoint;
	float                               m_prevCornerW, m_prevCornerH;

	typedef std::set<_smart_ptr<CHyperEdge>> PendingEdges;
	PendingEdges m_edgesToReroute;

	// Structure to help with bookkeeping if something changed VISUALY in the graph (e.g. node resize, node position change)
	struct SGraphVisualChanges
	{
		SGraphVisualChanges() : m_graphChanged(false) {}

		bool                   HasGraphVisuallyChanged() { return m_graphChanged; }
		void                   Reset(bool resetFlagChanged);
		void                   NodeMoved(const CHyperNode& nodeRef);
		void                   NodeResized();
		const Gdiplus::PointF& GetNodeStartPosition(const CHyperNode& nodeRef) const;

	private:
		std::map<THyperNodePtr, Gdiplus::PointF> m_movedNodesStartPositions;
		bool m_graphChanged;
	};

	SGraphVisualChanges            m_graphVisuallyChangedHelper;
	Gdiplus::RectF                 m_workingArea;
	float                          m_zoom;

	bool                           m_bCopiedBlackBox;

	CHyperGraphViewRenameEdit      m_renameEdit;
	THyperNodePtr                  m_pRenameNode;
	THyperNodePtr                  m_pEditedNode;
	std::vector<CHyperNode*>       m_pMultiEditedNodes;
	_smart_ptr<CVarBlock>          m_pMultiEditVars;

	CHyperGraphViewQuickSearchEdit m_quickSearchEdit;
	THyperNodePtr                  m_pQuickSearchNode;
	std::vector<CString>           m_SearchResults;
	CString                        m_sCurrentSearchSelection;

	THyperNodePtr                  m_pMouseOverNode;
	CHyperNodePort*                m_pMouseOverPort;

	CPropertyCtrl*                 m_pPropertiesCtrl;

	CToolTipCtrl                   m_tooltip;

	// Current graph.
	CHyperGraph* m_pGraph;

	uint32       m_componentViewMask;

	// used when draging the border of a node
	struct TDraggingBorderNodeInfo
	{
		THyperNodePtr   m_pNode;
		Gdiplus::PointF m_origNodePos;
		Gdiplus::RectF  m_origBorderRect;
		Gdiplus::PointF m_clickPoint;
		int             m_border; // actually is an EHyperNodeSubObjectId, but is managed as int in many other places, so
	}                 m_DraggingBorderNodeInfo;

	CPoint            prevMovePoint;

	CEditPropertyCtrl m_editParamCtrl;
	bool              m_isEditPort;
	CHyperNodePort*   m_pEditPort;
	CHyperNode*       m_pSrcDragNode;

	// struct for easier operation batching indication
	struct SStartNodeOperationBatching
	{
		SStartNodeOperationBatching(CHyperGraphView* pView) : m_pGraphView(pView) { pView->m_bBatchingChangesInProgress = true; }
		~SStartNodeOperationBatching() { m_pGraphView->m_bBatchingChangesInProgress = false; }
	private:
		SStartNodeOperationBatching();

		CHyperGraphView* m_pGraphView;
	};

	bool m_bIsFrozen;
	bool m_bIgnoreHyperGraphEvents;
	bool m_bBatchingChangesInProgress;

	static TFlowNodeId s_quickSearchNodeId;
};

