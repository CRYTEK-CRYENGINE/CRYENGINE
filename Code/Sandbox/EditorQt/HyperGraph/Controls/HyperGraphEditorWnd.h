// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"
#include "Dialogs/ToolbarDialog.h"
#include "Controls/PropertyCtrl.h"
#include "Controls/ColorCtrl.h"

#include "HyperGraphView.h"
#include "HyperGraphEditorTree.h"
#include "HyperGraphEditorNodeList.h"
#include "BreakPointsCtrl.h"


class CEntityObject;
class CHyperFlowGraph;
class CFlowGraphSearchResultsCtrl;
class CFlowGraphSearchCtrl;
class CHyperGraphDialog;
class CHGSelNodeInfoPanel;
class CHGGraphPropsPanel;
class CFlowGraphTokensCtrl;
class CHyperGraphsTreeCtrl;
class CHyperGraphComponentsReportCtrl;


//////////////////////////////////////////////////////////////////////////
//! Main Editor Window for HyperGraph. Holds all the other panels such as the graph view, tree list, breakpoints panel, etc.
class SANDBOX_API CHyperGraphDialog : public CXTPFrameWnd, private IEditorNotifyListener, private IHyperGraphManagerListener
{
	DECLARE_DYNCREATE(CHyperGraphDialog)
public:
	CHyperGraphDialog();
	~CHyperGraphDialog();

	BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd);

	//! Get the viewport that displays the graph nodes and edges.
	CHyperGraphView*      GetGraphView() { return &m_view; };
	//! Get the panel with the tree list of the available graphs.
	CHyperGraphsTreeCtrl* GetGraphsTreeCtrl() { return &m_graphsTreeCtrl; }
	//! Get the panel with the input and options for searching.
	CFlowGraphSearchCtrl* GetSearchControl() const { return m_pSearchOptionsView; }

	//! Update all the tool panels with a new graph (or null to clear them)
	void                  SetGraph(CHyperGraph* pGraph);
	//! Get the currently displayed graph. Can be null.
	CHyperGraph*          GetGraph() const;

	//! Force show/hide the search results list.
	void                  ShowResultsPane(bool bShow = true, bool bFocus = true);
	//! Open the dialog for editing the current graph's tokens.
	void                  OpenGraphTokenEditor() { OnEditGraphTokens(); }

	//! Called when the selection (nodes and edges) in the view has changed. Updates the selected node information panels.
	void                  OnViewSelectionChange();
	//! Sets the FG window and its subcomponents to respond or not to object and graph events, effectively (un)freezing the UI. bRefresh to true will reload the UI on unfreeze.
	void                  SetIgnoreEvents(bool bIgnore, bool bRefresh);
	//! Update the Graph Properties and Tokens Panels. Send notification that a graph property was modified.
	void                  UpdateGraphProperties(CHyperGraph* pGraph);
	void                  ReloadBreakpoints() { m_breakpointsTreeCtrl.FillBreakpoints(); }
	void                  ClearBreakpoints() { m_breakpointsTreeCtrl.DeleteAllItems(); }
	void                  ReloadGraphTreeList() { m_graphsTreeCtrl.FullReload(); }
	void                  ReloadComponents() { m_componentsReportCtrl.Reload(); }

	void                  OnComponentBeginDrag(CXTPReportRow* pRow, CPoint pt);

	typedef std::vector<HTREEITEM> TDTreeItems;

	enum { IDD = IDD_TRACKVIEWDIALOG };

	enum EContextMenuID
	{
		ID_GRAPHS_CHANGE_FOLDER = 1,
		ID_GRAPHS_SELECT_ENTITY,
		ID_GRAPHS_SAVE_GRAPH,
		ID_GRAPHS_DISCARD_GRAPH,
		ID_GRAPHS_REMOVE_GRAPH,
		ID_GRAPHS_IMPORT,
		ID_GRAPHS_EXPORT,
		ID_GRAPHS_EXPORTSELECTION,
		ID_GRAPHS_ENABLE,
		ID_GRAPHS_ENABLE_ALL,
		ID_GRAPHS_DISABLE_ALL,
		ID_GRAPHS_RENAME_FOLDER,
		ID_GRAPHS_RENAME_GRAPH,
		ID_BREAKPOINT_REMOVE_GRAPH,
		ID_BREAKPOINT_REMOVE_NODE,
		ID_BREAKPOINT_REMOVE_PORT,
		ID_BREAKPOINT_ENABLE,
		ID_TRACEPOINT_ENABLE,
		ID_GRAPHS_NEW_GLOBAL_MODULE,
		ID_GRAPHS_NEW_LEVEL_MODULE,
		ID_GRAPHS_ENABLE_DEBUGGING,
		ID_GRAPHS_DISABLE_DEBUGGING,
		ID_GRAPHS_RELOAD,
	};

protected:
	DECLARE_MESSAGE_MAP()

	virtual void            OnOK() {}
	virtual void            OnCancel() {}

	virtual BOOL            PreTranslateMessage(MSG* pMsg);
	virtual BOOL            OnInitDialog();
	virtual void            PostNcDestroy();
	afx_msg void            OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void            OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void            OnDestroy();
	afx_msg void            OnFileNew();
	afx_msg void            OnFileOpen();
	afx_msg void            OnFileSaveAllExternal();
	afx_msg void            OnFileNewAction();
	afx_msg void            OnFileNewUIAction();
	afx_msg void            OnFileNewCustomAction();
	afx_msg void            OnFileNewMatFXGraph();
	afx_msg void            OnFileNewGlobalFGModule();
	afx_msg void            OnFileNewLevelFGModule();
	afx_msg void            OnFileImport();
	afx_msg void            OnFileExport();
	afx_msg void            OnFileExportSelection();
	afx_msg void            OnEditGraphTokens();
	afx_msg void            OnEditModuleInputsOutputs();
	afx_msg LRESULT         OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void            OnGraphsRightClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void            OnGraphsDblClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void            OnGraphsSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void            OnBreakpointsRightClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void            OnBreakpointsClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void            OnPlay();
	afx_msg void            OnStop();
	afx_msg void            OnToggleDebug();
	afx_msg void            OnEraseDebug();
	afx_msg void            OnPause();
	afx_msg void            OnStep();
	afx_msg void            OnUpdatePlay(CCmdUI* pCmdUI);
	afx_msg void            OnUpdateStep(CCmdUI* pCmdUI);
	afx_msg void            OnUpdateDebug(CCmdUI* pCmdUI);
	afx_msg void            OnFind();
	afx_msg BOOL            OnComponentsViewToggle(UINT nID);
	afx_msg void            OnComponentsViewUpdate(CCmdUI* pCmdUI);
	afx_msg BOOL            OnDebugFlowgraphTypeToggle(UINT nID);
	afx_msg void            OnDebugFlowgraphTypeUpdate(CCmdUI* pCmdUI);
	afx_msg BOOL            OnToggleBar(UINT nID);
	afx_msg void            OnUpdateControlBar(CCmdUI* pCmdUI);
	afx_msg int             OnCreateControl(LPCREATECONTROLSTRUCT lpCreateControl);
	afx_msg void            OnNavListBoxControlSelChange(NMHDR* pNMHDR, LRESULT* pRes);
	afx_msg void            OnNavListBoxControlPopup(NMHDR* pNMHDR, LRESULT* pRes);
	afx_msg void            OnUpdateNav(CCmdUI* pCmdUI);
	afx_msg void            OnNavForward();
	afx_msg void            OnNavBack();
	afx_msg void            OnUndo();
	afx_msg void            OnRedo();
	afx_msg void            OnSave();
	afx_msg void            OnCustomize();
	afx_msg void            OnExportShortcuts();
	afx_msg void            OnImportShortcuts();
	afx_msg void            OnComponentSearchEditChange();

	virtual BOOL            OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

	void                    NewGraph();
	void                    SaveGraph(CHyperGraph* pGraph);
	void                    ExportGraph(CHyperGraph* pGraph);
	void                    ExportGraphSelection();

	void                    DisableGraph(CHyperFlowGraph* pFlowGraph, bool bEnable);
	void                    EnableItems(HTREEITEM hItem, bool bEnable, bool bRecurse);

	void                    RenameFlowGraph(CHyperFlowGraph* pFlowGraph, CString newName);

	void                    UpdateNavHistory();

	// Creates the right click context menu based on the currently selected items.
	// The current policy will make the menu have only the menu items that are common
	// to the entire selection.
	// OBS: renaming is only allowed for single selections.
	bool CreateContextMenu(CMenu& roMenu, TDTreeItems& rchSelectedItems);

	// This function will take the result of the menu and will process its
	// results applying the appropriate changes.
	void ProcessMenu(int nMenuOption, TDTreeItems& rchSelectedItems);

	bool CreateBreakpointContextMenu(CMenu& roMenu, HTREEITEM hItem);
	void ProcessBreakpointMenu(int nMenuOption, HTREEITEM hItem);

	//////////////////////////////////////////////////////////////////////////
	// This set of functions is used to help determining which menu items will
	// be available. Their usage is not 100% efficient, still, they improved
	// the readability of the code and thus made refactoring a lot easier.
	bool IsAnEntityFolderOrSubFolder(HTREEITEM hSelectedItem);
	bool IsAnEntitySubFolder(HTREEITEM hSelectedItem);
	// As both values, true or false would have interpretations,
	// we need to add another flag identifying if the result
	bool IsGraphEnabled(HTREEITEM hSelectedItem, bool& boIsEnabled);
	bool IsOwnedByAnEntity(HTREEITEM hSelectedItem);
	bool IsAssociatedToFlowGraph(HTREEITEM hSelectedItem);
	bool IsGraphType(HTREEITEM hSelectedItem, IFlowGraph::EFlowGraphType type);
	bool IsEnabledForDebugging(HTREEITEM hSelectedItem);


private:
	void OnEditorNotifyEvent(EEditorNotifyEvent event);
	void OnObjectEvent(CObjectEvent& eventObj);
	void OnObjectsAttached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects);
	void OnObjectsDetached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects);
	virtual void OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode);

	void NewFGModule(bool bGlobal = true);
	//! Outputs a name for the graph, including type and filename, or just the name in the short version.
	void GetGraphPrettyName(CHyperGraph* pGraph, CString& computedName, bool shortVersion);

	//! Update the title/status bar with the graphs name or with a null graph to clear it.
	void UpdateTitle(CHyperGraph* pGraph);

	//! Remove graph from the tree list and navigation history. clear the view and properties if this was the active graph.
	void RemoveGraph(CHyperGraph* pGraph);

	//! Clear the UI and remove all level graphs in one go.
	void PrepareLevelUnload();

	//! Load saved tool status, such as which panes were open.
	void LoadPersonalization();
	void SavePersonalization();

	void CreateRightPane();
	void CreateLeftPane();
	CXTPDockingPaneManager* GetDockingPaneManager() { return &m_paneManager; }



	CXTPDockingPaneManager            m_paneManager;
	CXTPTaskPanel                     m_wndTaskPanel;
	std::auto_ptr<CPropertyCtrl>      m_pWndProps;

	CXTPStatusBar                     m_titleBar; //! displays the name/path of the current graph. technically it's a status bar
	bool                              m_isTitleBarVisible; //! if the title bar is being displayed. saved to personalization.

	CHyperGraphView                   m_view; //! main 2D viewport that displays the graph nodes and edges.

	CHyperGraphsTreeCtrl              m_graphsTreeCtrl; //! panel with the tree list of available graphs.

	CXTPTaskPanel                     m_componentsTaskPanel; //! panel for available nodes (previously called components).
	CHyperGraphComponentsReportCtrl   m_componentsReportCtrl; //! tree list of available nodes per category.
	CColorCtrl<CEdit>                 m_componentSearchEdit; //! search bar for available nodes.
	uint32                            m_componentsViewMask; //! which node categories (EFLN_DEBUG, EFLN_ADVANCED..) should be displayed.
	CImageList*                       m_pDragImage; //! for the drag&drop visual effect from the nodes list to the graph view.
	CString                           m_dragNodeClass; //! for the drag&drop visual effect from the nodes list to the graph view.

	CFlowGraphSearchCtrl*             m_pSearchOptionsView; //! panel with the input and options for searching.
	CFlowGraphSearchResultsCtrl*      m_pSearchResultsCtrl; //! panel with the sortable result list of the search.

	CBreakpointsTreeCtrl              m_breakpointsTreeCtrl; //! panel with the breakpoints list.

	CXTPTaskPanelGroup*               m_pPropertiesFolder; //! panel with the selected node's inputs.
	CXTPTaskPanelGroupItem*           m_pPropertiesItem; //! internal content of m_pPropertiesFolder.
	CHGSelNodeInfoPanel*              m_pSelNodeInfo; //! panel with the selected node information. inside the Properties panel.
	CHGGraphPropsPanel*               m_pGraphProperties; //! panel with the current graph properties. inside the Properties panel.
	CFlowGraphTokensCtrl*             m_pGraphTokens; //! panel with the tokens of the current graph. inside the Properties panel.

	std::list<CHyperGraph*>           m_graphList; //! graph navigation history // TODO needs reviews and fixes.
	std::list<CHyperGraph*>::iterator m_graphIterCur;
	CHyperGraph*                      m_pNavGraph;

	IFlowGraphDebuggerPtr             m_pFlowGraphDebugger; //! FG debugger

	bool                              m_bIgnoreObjectEvents; //! All level contents and graphs are being created or deleted. Ignore certain types of events.
};

