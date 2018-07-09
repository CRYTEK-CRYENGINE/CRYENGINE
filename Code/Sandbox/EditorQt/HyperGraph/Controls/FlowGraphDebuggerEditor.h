// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryFlowGraph/IFlowGraphDebugger.h> //IFlowGraphDebugListener
#include "HyperGraph/IHyperGraph.h"          //IHyperGraphManagerListener

class CFlowGraphDebuggerEditor : private IFlowGraphDebugListener, IHyperGraphManagerListener, IEditorNotifyListener
{
public:
	CFlowGraphDebuggerEditor();
	virtual ~CFlowGraphDebuggerEditor();

	bool Init();
	bool Shutdown();

	bool AddBreakpoint(CFlowNode* pFlowNode, CHyperNodePort* pHyperNodePort);
	bool RemoveBreakpoint(CFlowNode* pFlowNode, CHyperNodePort* pHyperNodePort);
	bool RemoveAllBreakpointsForNode(CFlowNode* pFlowNode);
	bool RemoveAllBreakpointsForGraph(IFlowGraphPtr pFlowGraph);
	bool HasBreakpoint(CFlowNode* pFlowNode, const CHyperNodePort* pHyperNodePort) const;
	bool HasBreakpoint(IFlowGraphPtr pFlowGraph, TFlowNodeId nodeID) const;
	bool HasBreakpoint(IFlowGraphPtr pFlowGraph) const;
	bool EnableBreakpoint(CFlowNode* pFlowNode, CHyperNodePort* pHyperNodePort, bool enable);
	bool EnableTracepoint(CFlowNode* pFlowNode, CHyperNodePort* pHyperNodePort, bool enable);
	void PauseGame();
	void ResumeGame();

private:
	// IHyperGraphManagerListener
	virtual void OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode);
	// ~IHyperGraphManagerListener

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	// ~IEditorNotifyListener

	// IFlowgraphDebugListener
	virtual void OnBreakpointAdded(const SBreakPoint& breakpoint)                             {}
	virtual void OnBreakpointRemoved(const SBreakPoint& breakpoint)                           {}
	virtual void OnAllBreakpointsRemovedForNode(IFlowGraphPtr pFlowGraph, TFlowNodeId nodeID) {}
	virtual void OnAllBreakpointsRemovedForGraph(IFlowGraphPtr pFlowGraph)                    {}
	virtual void OnBreakpointHit(const SBreakPoint& breakpoint);
	virtual void OnTracepointHit(const STracePoint& tracepoint);
	virtual void OnBreakpointInvalidated(const SBreakPoint& breakpoint);
	virtual void OnEnableBreakpoint(const SBreakPoint& breakpoint, bool enable) {}
	virtual void OnEnableTracepoint(const STracePoint& tracepoint, bool enable) {}
	// ~IFlowgraphDebugListener

	void CenterViewAroundNode(CFlowNode* pNode) const;

	bool                  m_InGame;
	bool                  m_CursorVisible;
	IFlowGraphDebuggerPtr m_pFlowGraphDebugger;
};


