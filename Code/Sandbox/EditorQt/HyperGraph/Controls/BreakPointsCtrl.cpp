// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "BreakPointsCtrl.h"

#include <CryFlowGraph/IFlowGraphModuleManager.h>

#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraph/Controls/FlowGraphDebuggerEditor.h"

#include "Util/MFCUtil.h"

static const int FLOWGRAPH_ICON = 0;
static const int FLOWNODE_ICON = 1;
static const int BREAKPOINT_ICON = 2;
static const int OUTPUT_PORT_ICON = 3;
static const int INPUT_PORT_ICON = 4;
static const int DISABLED_BREAKPOINT = 5;
static const int TRACEPOINT = 6;

#define VALIDATE_DEBUGGER(a) if (!a) return;

IMPLEMENT_DYNAMIC(CBreakpointsTreeCtrl, CTreeCtrl)

BEGIN_MESSAGE_MAP(CBreakpointsTreeCtrl, CTreeCtrl)
ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CBreakpointsTreeCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	BOOL ret = __super::Create(dwStyle | WS_BORDER | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS, rect, pParentWnd, nID);
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_FLOWGRAPH_DEBUGGER, 16, RGB(255, 0, 255));
	SetImageList(&m_imageList, TVSIL_NORMAL);

	m_pFlowGraphDebugger = GetIFlowGraphDebuggerPtr();

	if (m_pFlowGraphDebugger)
	{
		if (!m_pFlowGraphDebugger->RegisterListener(this, "CBreakpointsTreeCtrl"))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Could not register as Flowgraph Debugger listener! [CBreakpointsTreeCtrl::Create]");
		}

		FillBreakpoints();
	}

	m_pFlowGraphDebuggerEditor = GetIEditorImpl()->GetFlowGraphDebuggerEditor();

	return ret;
}

BOOL CBreakpointsTreeCtrl::DestroyWindow()
{
	if (m_pFlowGraphDebugger)
	{
		m_pFlowGraphDebugger->UnregisterListener(this);
	}

	DeleteAllItems();

	return __super::DestroyWindow();
}

void CBreakpointsTreeCtrl::OnDestroy()
{
	if (m_pFlowGraphDebugger)
	{
		m_pFlowGraphDebugger->UnregisterListener(this);
	}

	DeleteAllItems();

	return __super::OnDestroy();
}

void CBreakpointsTreeCtrl::DeleteItem(HTREEITEM hItem)
{
	if (hItem)
	{
		SBreakpointItem* pBreakpointItem = reinterpret_cast<SBreakpointItem*>(GetItemData(hItem));
		SAFE_DELETE(pBreakpointItem);
	}

	__super::DeleteItem(hItem);
}

void CBreakpointsTreeCtrl::DeleteAllItems()
{
	for (HTREEITEM hItem = GetRootItem(); hItem; hItem = GetNextItem(hItem))
	{
		SBreakpointItem* pBreakpointItem = reinterpret_cast<SBreakpointItem*>(GetItemData(hItem));
		SAFE_DELETE(pBreakpointItem);
	}

	__super::DeleteAllItems();
}

HTREEITEM CBreakpointsTreeCtrl::GetNextItem(HTREEITEM hItem)
{
	if (ItemHasChildren(hItem))
		return GetChildItem(hItem);

	HTREEITEM tmp;
	if ((tmp = GetNextSiblingItem(hItem)) != NULL)
		return tmp;

	HTREEITEM p = hItem;
	while ((p = GetParentItem(p)) != NULL)
	{
		if ((tmp = GetNextSiblingItem(p)) != NULL)
			return tmp;
	}
	return NULL;
}

HTREEITEM CBreakpointsTreeCtrl::FindItemByGraph(CTreeCtrl* pTreeCtrl, HTREEITEM hRootItem, IFlowGraphPtr pFlowgraph)
{
	if (pTreeCtrl->ItemHasChildren(hRootItem))
	{
		do
		{
			if (HTREEITEM hNodeItem = pTreeCtrl->GetChildItem(hRootItem))
			{
				if (HTREEITEM hPortItem = pTreeCtrl->GetChildItem(hNodeItem))
				{
					SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(pTreeCtrl->GetItemData(hPortItem));
					if (breakpointItem && (breakpointItem->flowgraph == pFlowgraph))
						return hRootItem;
				}
			}
		}
		while (hRootItem = pTreeCtrl->GetNextSiblingItem(hRootItem));
	}

	return NULL;
}

HTREEITEM CBreakpointsTreeCtrl::FindItemByNode(CTreeCtrl* pTreeCtrl, HTREEITEM hGraphItem, TFlowNodeId nodeId)
{
	if (pTreeCtrl->ItemHasChildren(hGraphItem))
	{
		HTREEITEM hNodeItem = pTreeCtrl->GetChildItem(hGraphItem);

		if (hNodeItem)
		{
			do
			{
				HTREEITEM hPortItem = pTreeCtrl->GetChildItem(hNodeItem);
				do
				{
					SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(pTreeCtrl->GetItemData(hPortItem));
					if (breakpointItem && (breakpointItem->addr.node == nodeId))
						return hNodeItem;
				}
				while (hPortItem = pTreeCtrl->GetNextSiblingItem(hPortItem));

			}
			while (hNodeItem = pTreeCtrl->GetNextSiblingItem(hNodeItem));
		}
	}

	return NULL;
}

HTREEITEM CBreakpointsTreeCtrl::FindItemByPort(CTreeCtrl* pTreeCtrl, HTREEITEM hNodeItem, TFlowPortId portId, bool isOutput)
{
	if (pTreeCtrl->ItemHasChildren(hNodeItem))
	{
		HTREEITEM hPortItem = pTreeCtrl->GetChildItem(hNodeItem);

		if (hPortItem)
		{
			do
			{
				SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(pTreeCtrl->GetItemData(hPortItem));
				if (breakpointItem && (breakpointItem->addr.port == portId) && (breakpointItem->addr.isOutput == isOutput))
					return hPortItem;
			}
			while (hPortItem = pTreeCtrl->GetNextSiblingItem(hPortItem));
		}
	}

	return NULL;
}

void CBreakpointsTreeCtrl::OnBreakpointAdded(const SBreakPoint& breakpoint)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(breakpoint.flowGraph);
	CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);

	if (pFlowgraph)
	{
		HTREEITEM hGraphItem = FindItemByGraph(this, GetRootItem(), pFlowGraph);

		if (hGraphItem)
		{
			HTREEITEM nodeItem = FindItemByNode(this, hGraphItem, breakpoint.addr.node);
			CFlowNode* pNode = pFlowgraph->FindFlowNode(breakpoint.addr.node);
			CHyperNodePort* pPort = pNode->FindPort(breakpoint.addr.port, !breakpoint.addr.isOutput);

			if (nodeItem)
			{
				HTREEITEM portItem = FindItemByPort(this, nodeItem, breakpoint.addr.port, breakpoint.addr.isOutput);

				if (!portItem)
				{
					if (pPort)
					{
						const int icon = breakpoint.addr.isOutput ? OUTPUT_PORT_ICON : INPUT_PORT_ICON;
						portItem = InsertItem(pPort->GetHumanName(), icon, icon, nodeItem, TVI_SORT);

						SBreakpointItem* breakPointData = new SBreakpointItem();
						breakPointData->flowgraph = pFlowGraph;
						breakPointData->addr = breakpoint.addr;
						breakPointData->type = SBreakpointItem::eIT_Port;

						SetItemData(portItem, (DWORD_PTR)breakPointData);
						Expand(nodeItem, TVE_EXPAND);
					}
				}
			}
			else
			{
				nodeItem = InsertItem(pNode->GetUIClassName(), FLOWNODE_ICON, FLOWNODE_ICON, hGraphItem, TVI_SORT);

				SBreakpointItem* breakPointData = new SBreakpointItem();
				breakPointData->flowgraph = pFlowGraph;
				breakPointData->addr = breakpoint.addr;
				breakPointData->type = SBreakpointItem::eIT_Node;
				SetItemData(nodeItem, (DWORD_PTR)breakPointData);

				if (pPort)
				{
					const int icon = breakpoint.addr.isOutput ? OUTPUT_PORT_ICON : INPUT_PORT_ICON;
					HTREEITEM portItem = InsertItem(pPort->GetHumanName(), icon, icon, nodeItem, TVI_SORT);
					SBreakpointItem* breakPointData = new SBreakpointItem();
					breakPointData->flowgraph = pFlowGraph;
					breakPointData->addr = breakpoint.addr;
					breakPointData->type = SBreakpointItem::eIT_Port;

					SetItemData(portItem, (DWORD_PTR)breakPointData);
					Expand(nodeItem, TVE_EXPAND);
				}
			}
		}
		else
		{
			hGraphItem = InsertItem(pFlowgraph->GetName(), FLOWGRAPH_ICON, FLOWGRAPH_ICON, NULL, TVI_SORT);

			SBreakpointItem* breakPointData = new SBreakpointItem();
			breakPointData->flowgraph = pFlowGraph;
			breakPointData->addr = breakpoint.addr;
			breakPointData->type = SBreakpointItem::eIT_Graph;
			SetItemData(hGraphItem, (DWORD_PTR)breakPointData);

			CFlowNode* pNode = pFlowgraph->FindFlowNode(breakpoint.addr.node);

			if (pNode)
			{
				HTREEITEM nodeItem = InsertItem(pNode->GetUIClassName(), FLOWNODE_ICON, FLOWNODE_ICON, hGraphItem, TVI_SORT);
				CHyperNodePort* pPort = pNode->FindPort(breakpoint.addr.port, !breakpoint.addr.isOutput);

				SBreakpointItem* breakPointData = new SBreakpointItem();
				breakPointData->flowgraph = pFlowGraph;
				breakPointData->addr = breakpoint.addr;
				breakPointData->type = SBreakpointItem::eIT_Node;
				SetItemData(nodeItem, (DWORD_PTR)breakPointData);

				if (pPort)
				{
					const int icon = breakpoint.addr.isOutput ? OUTPUT_PORT_ICON : INPUT_PORT_ICON;
					HTREEITEM portItem = InsertItem(pPort->GetHumanName(), icon, icon, nodeItem, TVI_SORT);

					SBreakpointItem* breakPointData = new SBreakpointItem();
					breakPointData->flowgraph = pFlowGraph;
					breakPointData->addr = breakpoint.addr;
					breakPointData->type = SBreakpointItem::eIT_Port;

					SetItemData(portItem, (DWORD_PTR)breakPointData);
					Expand(hGraphItem, TVE_EXPAND);
					Expand(nodeItem, TVE_EXPAND);
				}
			}
		}
	}
}

void CBreakpointsTreeCtrl::OnBreakpointRemoved(const SBreakPoint& breakpoint)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(breakpoint.flowGraph);
	HTREEITEM hGraphItem = FindItemByGraph(this, GetRootItem(), pFlowGraph);

	if (hGraphItem)
	{
		HTREEITEM nodeItem = FindItemByNode(this, hGraphItem, breakpoint.addr.node);
		HTREEITEM portItem = FindItemByPort(this, nodeItem, breakpoint.addr.port, breakpoint.addr.isOutput);

		if (portItem && nodeItem)
		{
			DeleteItem(portItem);

			if (ItemHasChildren(nodeItem) == FALSE)
			{
				DeleteItem(nodeItem);
			}

			if (ItemHasChildren(hGraphItem) == FALSE)
			{
				DeleteItem(hGraphItem);
			}
		}
	}
}

void CBreakpointsTreeCtrl::OnAllBreakpointsRemovedForNode(IFlowGraphPtr pFlowGraph, TFlowNodeId nodeID)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(pFlowGraph);
	HTREEITEM hGraphItem = FindItemByGraph(this, GetRootItem(), pFlowGraph);

	if (hGraphItem)
	{
		HTREEITEM nodeItem = FindItemByNode(this, hGraphItem, nodeID);

		if (nodeItem)
		{
			HTREEITEM portItem = GetChildItem(nodeItem);
			while (portItem && ItemHasChildren(nodeItem))
			{
				HTREEITEM tempPortItem = portItem;
				portItem = GetNextSiblingItem(tempPortItem);
				DeleteItem(tempPortItem);
			}

			DeleteItem(nodeItem);

			if (ItemHasChildren(hGraphItem) == FALSE)
			{
				DeleteItem(hGraphItem);
			}
		}
	}
}

void CBreakpointsTreeCtrl::OnBreakpointHit(const SBreakPoint& breakpoint)
{
	HTREEITEM hPortItem = GetPortItem(breakpoint);

	if (hPortItem)
	{
		string val;
		breakpoint.value.GetValueWithConversion(val);
		string itemtext = GetItemText(hPortItem);
		SetItemText(hPortItem, itemtext + " - (" + val + ")");
		SetItemImage(hPortItem, BREAKPOINT_ICON, BREAKPOINT_ICON);
		SelectItem(hPortItem);
	}
}

void CBreakpointsTreeCtrl::OnAllBreakpointsRemovedForGraph(IFlowGraphPtr pFlowGraph)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(pFlowGraph);
	HTREEITEM hGraphItem = FindItemByGraph(this, GetRootItem(), pFlowGraph);

	if (hGraphItem)
	{
		HTREEITEM nodeItem = GetChildItem(hGraphItem);

		while (nodeItem && ItemHasChildren(hGraphItem))
		{
			HTREEITEM portItem = GetChildItem(nodeItem);

			while (portItem && ItemHasChildren(nodeItem))
			{
				HTREEITEM tempPortItem = portItem;
				portItem = GetNextSiblingItem(tempPortItem);
				DeleteItem(tempPortItem);
			}

			HTREEITEM tempItem = nodeItem;
			nodeItem = GetNextSiblingItem(tempItem);
			DeleteItem(tempItem);
		}

		DeleteItem(hGraphItem);
	}
}

void CBreakpointsTreeCtrl::OnBreakpointInvalidated(const SBreakPoint& breakpoint)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	HTREEITEM hPortItem = GetPortItem(breakpoint);

	if (hPortItem)
	{
		IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(breakpoint.flowGraph);
		CHyperFlowGraph* pGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);
		CFlowNode* pNode = pGraph->FindFlowNode(breakpoint.addr.node);

		if (pNode)
		{
			CHyperNodePort* pPort = pNode->FindPort(breakpoint.addr.port, !breakpoint.addr.isOutput);

			if (pPort)
			{
				CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);
				CFlowNode* pNode = pFlowgraph->FindFlowNode(breakpoint.addr.node);
				SetItemText(hPortItem, pPort->GetHumanName());
			}
		}
		const int icon = breakpoint.addr.isOutput ? OUTPUT_PORT_ICON : INPUT_PORT_ICON;
		SetItemImage(hPortItem, icon, icon);
		SelectItem(NULL);
	}
}

void CBreakpointsTreeCtrl::OnEnableBreakpoint(const SBreakPoint& breakpoint, bool enable)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	HTREEITEM hPortItem = GetPortItem(breakpoint);

	if (hPortItem)
	{
		const int defaultIcon = breakpoint.addr.isOutput ? OUTPUT_PORT_ICON : INPUT_PORT_ICON;
		int icon = defaultIcon;

		const bool bIsTracepoint = m_pFlowGraphDebugger->IsTracepoint(breakpoint.flowGraph, breakpoint.addr);

		if (enable && bIsTracepoint)
			icon = TRACEPOINT;
		else if (!enable)
			icon = DISABLED_BREAKPOINT;

		SetItemImage(hPortItem, icon, icon);
	}
}

void CBreakpointsTreeCtrl::OnEnableTracepoint(const STracePoint& tracepoint, bool enable)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	HTREEITEM hPortItem = GetPortItem(tracepoint);
	const bool bIsEnabled = m_pFlowGraphDebugger->IsBreakpointEnabled(tracepoint.flowGraph, tracepoint.addr);

	if (hPortItem && bIsEnabled)
	{
		const int defaultIcon = tracepoint.addr.isOutput ? OUTPUT_PORT_ICON : INPUT_PORT_ICON;
		const int icon = enable ? TRACEPOINT : defaultIcon;
		SetItemImage(hPortItem, icon, icon);
	}
}

HTREEITEM CBreakpointsTreeCtrl::GetPortItem(const SBreakPointBase& breakpoint)
{
	if (!m_pFlowGraphDebugger)
	{
		return NULL;
	}

	IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(breakpoint.flowGraph);
	const HTREEITEM hGraphItem = FindItemByGraph(this, GetRootItem(), pFlowGraph);

	if (hGraphItem)
	{
		const HTREEITEM nodeItem = FindItemByNode(this, hGraphItem, breakpoint.addr.node);
		HTREEITEM portItem = FindItemByPort(this, nodeItem, breakpoint.addr.port, breakpoint.addr.isOutput);

		return portItem;
	}

	return NULL;
}

void CBreakpointsTreeCtrl::RemoveBreakpointForPort(HTREEITEM hItem)
{
	SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(GetItemData(hItem));

	if (breakpointItem)
	{
		if (m_pFlowGraphDebuggerEditor)
		{
			CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(breakpointItem->flowgraph);
			if (pFlowgraph)
			{
				CFlowNode* pNode = pFlowgraph->FindFlowNode(breakpointItem->addr.node);
				CHyperNodePort* pPort = pNode->FindPort(breakpointItem->addr.port, !breakpointItem->addr.isOutput);
				m_pFlowGraphDebuggerEditor->RemoveBreakpoint(pNode, pPort);
				pNode->Invalidate(true, false);
			}
		}
	}
}

void CBreakpointsTreeCtrl::RemoveBreakpointsForGraph(HTREEITEM hItem)
{
	SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(GetItemData(hItem));

	if (breakpointItem)
	{
		if (m_pFlowGraphDebuggerEditor)
		{
			CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(breakpointItem->flowgraph);
			if (pFlowgraph)
			{
				m_pFlowGraphDebuggerEditor->RemoveAllBreakpointsForGraph(breakpointItem->flowgraph);
			}
		}
	}
}

void CBreakpointsTreeCtrl::RemoveBreakpointsForNode(HTREEITEM hItem)
{
	SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(GetItemData(hItem));

	if (breakpointItem)
	{
		if (m_pFlowGraphDebuggerEditor)
		{
			CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(breakpointItem->flowgraph);
			if (pFlowgraph)
			{
				CFlowNode* pNode = pFlowgraph->FindFlowNode(breakpointItem->addr.node);
				m_pFlowGraphDebuggerEditor->RemoveAllBreakpointsForNode(pNode);
			}
		}
	}
}

void CBreakpointsTreeCtrl::FillBreakpoints()
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	DeleteAllItems();
	DynArray<SBreakPoint> breakpoints;
	if (m_pFlowGraphDebugger->GetBreakpoints(breakpoints))
	{
		DynArray<SBreakPoint>::iterator it = breakpoints.begin();
		DynArray<SBreakPoint>::iterator end = breakpoints.end();

		for (; it != end; ++it)
		{
			OnBreakpointAdded(*it);
		}
	}
}

void CBreakpointsTreeCtrl::EnableBreakpoint(HTREEITEM hItem, bool enable)
{
	SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(GetItemData(hItem));

	if (breakpointItem)
	{
		if (m_pFlowGraphDebuggerEditor)
		{
			CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(breakpointItem->flowgraph);
			if (pFlowgraph)
			{
				CFlowNode* pNode = pFlowgraph->FindFlowNode(breakpointItem->addr.node);
				CHyperNodePort* pPort = pNode->FindPort(breakpointItem->addr.port, !breakpointItem->addr.isOutput);
				m_pFlowGraphDebuggerEditor->EnableBreakpoint(pNode, pPort, enable);
			}
		}
	}
}

void CBreakpointsTreeCtrl::EnableTracepoint(HTREEITEM hItem, bool enable)
{
	SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(GetItemData(hItem));

	if (breakpointItem)
	{
		if (m_pFlowGraphDebuggerEditor)
		{
			CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(breakpointItem->flowgraph);
			if (pFlowgraph)
			{
				CFlowNode* pNode = pFlowgraph->FindFlowNode(breakpointItem->addr.node);
				CHyperNodePort* pPort = pNode->FindPort(breakpointItem->addr.port, !breakpointItem->addr.isOutput);
				m_pFlowGraphDebuggerEditor->EnableTracepoint(pNode, pPort, enable);
			}
		}
	}
}

#undef VALIDATE_DEBUGGER

