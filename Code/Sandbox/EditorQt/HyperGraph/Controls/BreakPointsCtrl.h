// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_BREAKPOINTSCONTROL_H)
#define AFX_BREAKPOINTSCONTROL_H

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryFlowGraph/IFlowGraphDebugger.h>

class CFlowGraphDebuggerEditor;

struct SBreakpointItem
{
	enum EItemType
	{
		eIT_Graph = 0,
		eIT_Node,
		eIT_Port
	};

	IFlowGraphPtr flowgraph;
	SFlowAddress  addr;
	EItemType     type;
};

class CBreakpointsTreeCtrl : public CTreeCtrl, IFlowGraphDebugListener
{
	DECLARE_DYNAMIC(CBreakpointsTreeCtrl)

public:
	CBreakpointsTreeCtrl() {}

	virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
	virtual BOOL DestroyWindow();

	// IFlowgraphDebugListener
	void OnBreakpointAdded(const SBreakPoint& breakpoint);
	void OnBreakpointRemoved(const SBreakPoint& breakpoint);
	void OnAllBreakpointsRemovedForNode(IFlowGraphPtr pFlowGraph, TFlowNodeId nodeID);
	void OnAllBreakpointsRemovedForGraph(IFlowGraphPtr pFlowGraph);
	void OnBreakpointHit(const SBreakPoint& breakpoint);
	void OnTracepointHit(const STracePoint& tracepoint) {}
	void OnBreakpointInvalidated(const SBreakPoint& breakpoint);
	void OnEnableBreakpoint(const SBreakPoint& breakpoint, bool enable);
	void OnEnableTracepoint(const STracePoint& tracepoint, bool enable);
	//~ IFlowgraphDebugListener

	void      RemoveBreakpointsForGraph(HTREEITEM hItem);
	void      RemoveBreakpointsForNode(HTREEITEM hItem);
	void      RemoveBreakpointForPort(HTREEITEM hItem);
	void      EnableBreakpoint(HTREEITEM hItem, bool enable);
	void      EnableTracepoint(HTREEITEM hItem, bool enable);

	void      DeleteItem(HTREEITEM hItem);
	void      DeleteAllItems();
	void      FillBreakpoints();

	HTREEITEM GetNextItem(HTREEITEM hItem);
	HTREEITEM FindItemByGraph(CTreeCtrl* pTreeCtrl, HTREEITEM hRootItem, IFlowGraphPtr pFlowgraph);
	HTREEITEM FindItemByNode(CTreeCtrl* pTreeCtrl, HTREEITEM hGraphItem, TFlowNodeId nodeId);
	HTREEITEM FindItemByPort(CTreeCtrl* pTreeCtrl, HTREEITEM hNodeItem, TFlowPortId portId, bool isOutput);

protected:
	CImageList m_imageList;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();

private:

	HTREEITEM GetPortItem(const SBreakPointBase& breakpoint);

	IFlowGraphDebuggerPtr     m_pFlowGraphDebugger;
	CFlowGraphDebuggerEditor* m_pFlowGraphDebuggerEditor;
};

#endif // !defined(AFX_BREAKPOINTSCONTROL_H)

