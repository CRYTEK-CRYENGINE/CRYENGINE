// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowGraphDebugger.cpp
//  Version:     v1.00
//  Created:     13/09/2011 by Sascha Hoba.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#ifdef INCLUDE_FLOWGRAPHDEBUGGER_EXTENSION

	#include "FlowGraphDebugger.h"
	#include "FlowData.h"
	#include "FlowGraph.h"
	#include <CryFlowGraph/IFlowGraphModuleManager.h>
	#include <CryFlowGraph/IFlowGraphDebugger.h>

CRYREGISTER_SINGLETON_CLASS(CFlowGraphDebugger)

CFlowGraphDebugger::CFlowGraphDebugger() :
	m_DebugInfo(),
	m_BreakpointHit(false),
	m_RePerformActivation(false),
	m_bStepModeEnabled(false),
	m_BreakPoint(),
	m_DelayedActivations(),
	m_Listeners(1)
{
}

CFlowGraphDebugger::~CFlowGraphDebugger()
{
	m_DebugInfo.clear();
	m_Listeners.Clear();
	m_IgnoredFlowgraphs.clear();
}

void CFlowGraphDebugger::ClearBreakpoints()
{
	m_DebugInfo.clear();
}

bool CFlowGraphDebugger::AddBreakpoint(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr)
{
	if (!pFlowgraph)
		return false;

	if (!CheckForValidIDs(addr.node, addr.port))
		return false;

	bool bAddedBreakpoint = false;

	TDebugInfo::iterator iter = m_DebugInfo.find(pFlowgraph);
	if (iter != m_DebugInfo.end())
	{
		TFlowNodesDebugInfo* flownodesDebugInfo = &(*iter).second;
		TFlowNodesDebugInfo::iterator iterNode = flownodesDebugInfo->find(addr.node);

		if (iterNode != flownodesDebugInfo->end())
		{
			//we are already debugging this node, check if we have to add a new port
			SBreakPoints* breakPoints = &(*iterNode).second;

			TFlowPortIDS* flowPortIDS = NULL;
			if (addr.isOutput)
				flowPortIDS = &breakPoints->outputPorts;
			else
				flowPortIDS = &breakPoints->inputPorts;

			for (std::vector<SBreakPointPortInfo>::const_iterator iterBreakpoint = (*flowPortIDS).begin(); iterBreakpoint != (*flowPortIDS).end(); ++iterBreakpoint)
			{
				if (iterBreakpoint->portID == addr.port)
				{
					CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "Port %u for Flownode %u is already in debug list [CFlowgraphDebugger::AddBreakPoint]", addr.port, addr.node);
					return false;
				}
			}

			SBreakPointPortInfo portInfo;
			portInfo.portID = addr.port;
			flowPortIDS->push_back(portInfo);
			bAddedBreakpoint = true;
		}
		else
		{
			//we don't debug this flownode yet, so add the flownode and port
			SBreakPoints breakPoints;

			TFlowPortIDS flowPortIDS;
			SBreakPointPortInfo portInfo;
			portInfo.portID = addr.port;
			flowPortIDS.push_back(portInfo);

			if (addr.isOutput)
				breakPoints.outputPorts = flowPortIDS;
			else
				breakPoints.inputPorts = flowPortIDS;

			flownodesDebugInfo->insert(std::make_pair(addr.node, breakPoints));

			bAddedBreakpoint = true;
		}
	}
	else
	{
		// we don't debug any flownode of this flowgraph yet
		TFlowNodesDebugInfo flownodesDebugInfo;
		SBreakPoints breakPoints;

		TFlowPortIDS flowPortIDS;
		SBreakPointPortInfo portInfo;
		portInfo.portID = addr.port;
		flowPortIDS.push_back(portInfo);

		if (addr.isOutput)
			breakPoints.outputPorts = flowPortIDS;
		else
			breakPoints.inputPorts = flowPortIDS;

		flownodesDebugInfo.insert(std::make_pair(addr.node, breakPoints));
		m_DebugInfo.insert(std::make_pair(pFlowgraph, flownodesDebugInfo));

		bAddedBreakpoint = true;
	}

	if (bAddedBreakpoint)
	{
		for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
		{
			SBreakPoint breakpoint;
			breakpoint.flowGraph = pFlowgraph;
			breakpoint.addr = addr;

			notifier->OnBreakpointAdded(breakpoint);
		}
	}

	return bAddedBreakpoint;
}

bool CFlowGraphDebugger::RemoveBreakpoint(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr)
{
	TDebugInfo::iterator iterDebugInfo = m_DebugInfo.find(pFlowgraph);
	if (iterDebugInfo != m_DebugInfo.end())
	{
		TFlowNodesDebugInfo* flownodesDebugInfo = &(*iterDebugInfo).second;
		TFlowNodesDebugInfo::iterator iterNode = flownodesDebugInfo->find(addr.node);

		if (iterNode != flownodesDebugInfo->end())
		{
			SBreakPoints* breakPointInfo = &(*iterNode).second;
			TFlowPortIDS* flowPortIDS = NULL;

			if (addr.isOutput)
				flowPortIDS = &breakPointInfo->outputPorts;
			else
				flowPortIDS = &breakPointInfo->inputPorts;

			const int numBreakpoints = breakPointInfo->inputPorts.size() + breakPointInfo->outputPorts.size();

			TFlowPortIDS::iterator iter = flowPortIDS->begin();
			while (iter != flowPortIDS->end())
			{
				if ((*iter).portID == addr.port)
				{
					if (numBreakpoints == 1)
					{
						// we are about to delete the last port for this flownode from the debug list,
						// just remove the whole flownode information from the list
						flownodesDebugInfo->erase(iterNode);

						if (flownodesDebugInfo->empty())
							m_DebugInfo.erase(iterDebugInfo);
					}
					else
					{
						flowPortIDS->erase(iter);
					}

					for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
					{
						SBreakPoint breakpoint;
						breakpoint.flowGraph = pFlowgraph;
						breakpoint.addr = addr;

						notifier->OnBreakpointRemoved(breakpoint);
					}

					return true;
				}
				++iter;
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "Flownode %u is not in debug list [CFlowgraphDebugger::RemoveBreakPoint]", addr.node);
			return false;
		}
	}

	CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "Port %u of Flownode %u is not in debug list [CFlowgraphDebugger::RemoveBreakPoint]", addr.port, addr.node);
	return false;
}

bool CFlowGraphDebugger::RemoveAllBreakpointsForNode(IFlowGraphPtr pFlowgraph, TFlowNodeId nodeID)
{
	CRY_ASSERT(nodeID != InvalidFlowNodeId);

	if (nodeID == InvalidFlowNodeId)
		return false;

	TDebugInfo::iterator iterDebugInfo = m_DebugInfo.find(pFlowgraph);
	if (iterDebugInfo != m_DebugInfo.end())
	{
		TFlowNodesDebugInfo* flownodesDebugInfo = &(*iterDebugInfo).second;
		TFlowNodesDebugInfo::iterator iterNode = flownodesDebugInfo->find(nodeID);

		if (iterNode != flownodesDebugInfo->end())
		{
			flownodesDebugInfo->erase(iterNode);

			for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
			{
				notifier->OnAllBreakpointsRemovedForNode(pFlowgraph, nodeID);
			}

			return true;
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "Flownode %u is not in debug list [CFlowgraphDebugger::RemoveBreakPointsForNode]", nodeID);
		return false;
	}

	return false;
}

bool CFlowGraphDebugger::RemoveAllBreakpointsForGraph(IFlowGraphPtr pFlowgraph)
{
	TDebugInfo::iterator iterDebugInfo = m_DebugInfo.find(pFlowgraph);
	if (iterDebugInfo != m_DebugInfo.end())
	{
		m_DebugInfo.erase(iterDebugInfo);

		for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
		{
			notifier->OnAllBreakpointsRemovedForGraph(pFlowgraph);
		}
		return true;
	}

	return false;
}

bool CFlowGraphDebugger::HasBreakpoint(IFlowGraphPtr pFlowGraph, const SFlowAddress& addr) const
{
	CRY_ASSERT(NULL != pFlowGraph);

	if (!pFlowGraph)
		return false;

	const SBreakPointPortInfo* breakpointInfo = GetBreakpointInfo(pFlowGraph, addr);

	if (breakpointInfo)
	{
		return true;
	}
	else
	{
		// no breakpoints at all for this flownode
		return false;
	}

	return false;
}

bool CFlowGraphDebugger::HasBreakpoint(IFlowGraphPtr pFlowGraph, TFlowNodeId nodeID) const
{
	CRY_ASSERT(NULL != pFlowGraph);

	if (!pFlowGraph)
		return false;

	CRY_ASSERT(nodeID != InvalidFlowNodeId);

	if (nodeID == InvalidFlowNodeId)
		return false;

	pFlowGraph = GetRootGraph(pFlowGraph);

	TDebugInfo::const_iterator iterDebugInfo = m_DebugInfo.find(pFlowGraph);

	if (iterDebugInfo != m_DebugInfo.end())
	{
		const TFlowNodesDebugInfo* flownodesDebugInfo = &(*iterDebugInfo).second;
		TFlowNodesDebugInfo::const_iterator iterNode = flownodesDebugInfo->find(nodeID);

		if (iterNode != flownodesDebugInfo->end())
		{
			// this flownode has at least one breakpoint
			return true;
		}

		// no breakpoints at all for this flownode
		return false;
	}

	return false;
}

bool CFlowGraphDebugger::HasBreakpoint(IFlowGraphPtr pFlowGraph) const
{
	CRY_ASSERT(NULL != pFlowGraph);

	if (!pFlowGraph)
		return false;

	pFlowGraph = GetRootGraph(pFlowGraph);

	TDebugInfo::const_iterator iter = m_DebugInfo.find(pFlowGraph);

	if (iter != m_DebugInfo.end())
		return true;

	return false;
}

bool CFlowGraphDebugger::CheckForValidIDs(TFlowNodeId nodeID, TFlowPortId portID) const
{
	if (nodeID == InvalidFlowNodeId)
		return false;

	if (portID == InvalidFlowPortId)
		return false;

	if (m_BreakPoint.addr.node == nodeID && m_BreakPoint.addr.port == portID)
		return false;

	if (m_BreakPoint.fromAddr.node == nodeID && m_BreakPoint.fromAddr.port == portID)
		return false;

	return true;
}

bool CFlowGraphDebugger::PerformActivation(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr, const TFlowInputData& value)
{
	if (IsFlowgraphIgnored(pFlowgraph) || IsFlowgraphTypeIgnored(pFlowgraph->GetType()) || !CheckForValidIDs(addr.node, addr.port))
		return true;

	if (m_BreakpointHit && !m_RePerformActivation)
	{
		// The system tries to activate a port after a breakpoint was hit, so cache the activation in a deque.
		// This is a valid case as it is allowed to activate 2 output ports in a row:
		// ActivateOutput(eOP_Done,...);
		// ActivateOutput(eOP_Succeded,...);
		// If the first activation results in a breakpoint the second (if it also results in a breakpoint)
		// would override the first, even though we paused the game/flowsystem.
		SBreakPoint breakpoint;
		breakpoint.addr = addr;
		breakpoint.flowGraph = pFlowgraph;
		breakpoint.value = value;
		breakpoint.type = eBT_Output_Without_Edges;

		m_DelayedActivations.push_back(breakpoint);
		return false;
	}

	const SBreakPointPortInfo* breakpointInfo = GetBreakpointInfo(pFlowgraph, addr);

	if (!breakpointInfo)
	{
		if (!m_bStepModeEnabled)
			return true;
	}

	const bool bIsEnabled = breakpointInfo ? breakpointInfo->bIsEnabled : m_bStepModeEnabled;
	const bool bIsTracepoint = breakpointInfo ? breakpointInfo->bIsTracepoint : false;

	if (bIsEnabled)
	{
		if (!bIsTracepoint)
		{
			InvalidateBreakpoint();
			// Special case: This output has no edges, so it will never activate any input port.
			// We still want to be able to pause the game/flowsystem if an output port with no
			// edges was activated.
			m_BreakPoint.flowGraph = pFlowgraph;
			m_BreakPoint.addr = addr;
			m_BreakPoint.value = value;
			m_BreakPoint.type = eBT_Output_Without_Edges;

			m_BreakpointHit = true;

			for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
			{
				notifier->OnBreakpointHit(m_BreakPoint);
			}

			return false;
		}
		else
		{
			for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
			{
				STracePoint tracepoint;
				tracepoint.addr = addr;
				tracepoint.flowGraph = pFlowgraph;
				tracepoint.value = value;
				notifier->OnTracepointHit(tracepoint);
			}
		}
	}

	return true;
}

bool CFlowGraphDebugger::PerformActivation(IFlowGraphPtr pFlowgraph, int edgeIndex, const SFlowAddress& fromAddr, const SFlowAddress& toAddr, const TFlowInputData& value)
{
	if (m_BreakpointHit && !m_RePerformActivation)
	{
		// The system tries to activate a port after a breakpoint was hit, so cache the activation in a deque.
		// This is a valid case as it is allowed to activate 2 output ports in a row:
		// ActivateOutput(eOP_Done,...);
		// ActivateOutput(eOP_Succeded,...);
		// If the first activation results in a breakpoint the second (if it also results in a breakpoint)
		// would override the first, even though we paused the game/flowsystem.
		SBreakPoint breakpoint;
		breakpoint.addr = fromAddr;
		breakpoint.flowGraph = pFlowgraph;
		breakpoint.value = value;
		breakpoint.type = eBT_Output;
		breakpoint.edgeIndex = edgeIndex;

		m_DelayedActivations.push_back(breakpoint);
		return false;
	}

	bool bInformListeners = false;

	const SBreakPointPortInfo* breakpointInfoFrom = GetBreakpointInfo(pFlowgraph, fromAddr);
	const bool bFromIsEnabled = breakpointInfoFrom ? breakpointInfoFrom->bIsEnabled : m_bStepModeEnabled;
	const bool bFromIsTracepoint = breakpointInfoFrom ? breakpointInfoFrom->bIsTracepoint : false;

	const SBreakPointPortInfo* breakpointInfoTo = GetBreakpointInfo(pFlowgraph, toAddr);
	const bool bToIsEnabled = breakpointInfoTo ? breakpointInfoTo->bIsEnabled : m_bStepModeEnabled;
	const bool bToIsTracepoint = breakpointInfoTo ? breakpointInfoTo->bIsTracepoint : false;

	const bool bIgnoredGraph = (IsFlowgraphIgnored(pFlowgraph) || IsFlowgraphTypeIgnored(pFlowgraph->GetType()));
	if (!bIgnoredGraph && CheckForValidIDs(fromAddr.node, fromAddr.port) && (breakpointInfoFrom || m_bStepModeEnabled))
	{
		if (bFromIsEnabled)
		{
			if (!bFromIsTracepoint)
			{
				InvalidateBreakpoint();
				// First check if the from address has a breakpoint (output port)
				m_BreakPoint.addr = fromAddr;
				m_BreakPoint.flowGraph = pFlowgraph;
				m_BreakPoint.value = value;
				m_BreakPoint.type = eBT_Output;
				m_BreakPoint.edgeIndex = edgeIndex;

				bInformListeners = true;
			}
			else
			{
				for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
				{
					STracePoint tracepoint;
					tracepoint.addr = fromAddr;
					tracepoint.flowGraph = pFlowgraph;
					tracepoint.value = value;
					notifier->OnTracepointHit(tracepoint);
				}
			}
		}
	}
	else if (!bIgnoredGraph && CheckForValidIDs(toAddr.node, toAddr.port) && (breakpointInfoTo || m_bStepModeEnabled))
	{
		if (bToIsEnabled)
		{
			if (!bToIsTracepoint)
			{
				InvalidateBreakpoint();
				// Now check if the to address has a breakpoint (input port)
				m_BreakPoint.addr = toAddr;
				m_BreakPoint.fromAddr = fromAddr;
				m_BreakPoint.flowGraph = pFlowgraph;
				m_BreakPoint.value = value;
				m_BreakPoint.type = eBT_Input;
				m_BreakPoint.edgeIndex = edgeIndex;

				bInformListeners = true;
			}
			else
			{
				for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
				{
					STracePoint tracepoint;
					tracepoint.addr = toAddr;
					tracepoint.flowGraph = pFlowgraph;
					tracepoint.value = value;
					notifier->OnTracepointHit(tracepoint);
				}
			}
		}
	}

	if (bInformListeners)
	{
		m_BreakpointHit = true;

		for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
		{
			notifier->OnBreakpointHit(m_BreakPoint);
		}

		return false;
	}

	return true;
}

bool CFlowGraphDebugger::RePerformActivation()
{
	if (m_BreakPoint.type == eBT_Output_Without_Edges)
	{
		// Special case: Output port with no edges never activates any input port, simply check for delayed
		// activations.
		return CheckForDelayedActivations();
	}
	else if (m_BreakPoint.type == eBT_Output || m_BreakPoint.type == eBT_Input)
	{
		// Output port with edges, simply go through all edges connected to this output port and activate the
		// connected input ports with the stored value.
		m_RePerformActivation = true;

		CRY_ASSERT(m_BreakPoint.flowGraph != 0);
		m_BreakPoint.flowGraph->EnsureSortedEdges();

		IFlowEdgeIteratorPtr pEdgeIter = m_BreakPoint.flowGraph->CreateEdgeIterator();

		if (pEdgeIter)
		{
			IFlowEdgeIterator::Edge edge;

			if (m_BreakPoint.type == eBT_Input)
			{
				CFlowData* pData = static_cast<CFlowData*>(m_BreakPoint.flowGraph->GetNodeData(m_BreakPoint.fromAddr.node));

				if (pData)
				{
					int firstEdge = pData->GetOutputFirstEdge(m_BreakPoint.fromAddr.port) + m_BreakPoint.edgeIndex;

					while (firstEdge > 0 && pEdgeIter->Next(edge))
					{
						--firstEdge;
					}
				}
			}

			int edgeIndex = m_BreakPoint.edgeIndex;
			bool bPerform = true;
			while (pEdgeIter->Next(edge) && bPerform)
			{
				if (m_BreakPoint.type == eBT_Output && edge.fromNodeId == m_BreakPoint.addr.node && edge.fromPortId == m_BreakPoint.addr.port)
				{
					if (!TryActivatePort(m_BreakPoint, edge, bPerform, edgeIndex))
					{
						return false;
					}
					++edgeIndex;
				}
				else if (m_BreakPoint.type == eBT_Input && edge.fromNodeId == m_BreakPoint.fromAddr.node && edge.fromPortId == m_BreakPoint.fromAddr.port)
				{
					if (!TryActivatePort(m_BreakPoint, edge, bPerform, edgeIndex))
					{
						return false;
					}
					++edgeIndex;
				}

			}
		}

		m_RePerformActivation = false;
		return CheckForDelayedActivations();
	}

	return true;
}

bool CFlowGraphDebugger::TryActivatePort(SBreakPoint& breakpoint, IFlowEdgeIterator::Edge& edge, bool& bPerform, int edgeIndex)
{
	SFlowAddress fromAddr;
	fromAddr.node = edge.fromNodeId;
	fromAddr.port = edge.fromPortId;
	fromAddr.isOutput = true;

	SFlowAddress toAddr;
	toAddr.node = edge.toNodeId;
	toAddr.port = edge.toPortId;
	toAddr.isOutput = false;

	bPerform = PerformActivation(breakpoint.flowGraph, edgeIndex, fromAddr, toAddr, breakpoint.value);

	if (bPerform)
	{
		CFlowData* pData = static_cast<CFlowData*>(breakpoint.flowGraph->GetNodeData(edge.toNodeId));

		if (pData)
		{
			pData->ActivateInputPort(edge.toPortId, breakpoint.value);
			breakpoint.flowGraph->ActivateNode(edge.toNodeId);

			string val;
			if (breakpoint.value.GetValueWithConversion(val))
			{
				breakpoint.flowGraph->NotifyFlowNodeActivationListeners(fromAddr.node, fromAddr.port, toAddr.node, toAddr.port, val.c_str());
			}
		}
	}
	else
	{
		m_RePerformActivation = false;
		return false;
	}

	return true;
}

bool CFlowGraphDebugger::CheckForDelayedActivations()
{
	if (m_BreakpointHit)
		InvalidateBreakpoint();

	if (m_DelayedActivations.empty() && m_BreakPoint.type == eBT_Output_Without_Edges)
	{
		// we just stopped at an output port without edges and no delayed activations exist
		return true;
	}

	bool bPerform = true;

	while (bPerform)
	{
		if (m_DelayedActivations.empty())
		{
			break;
		}

		SBreakPoint delayedActivation = m_DelayedActivations.front();

		if (delayedActivation.addr.isOutput && delayedActivation.type == eBT_Output_Without_Edges)
		{
			// We have a delayed output port activation with NO edges, invalidate the breakpoint
			// and perform the activation directly
			bPerform = PerformActivation(delayedActivation.flowGraph, delayedActivation.addr, delayedActivation.value);

			m_DelayedActivations.pop_front();
		}
		else if (delayedActivation.type == eBT_Output)
		{
			// We have a delayed output port activation WITH edges, invalidate the breakpoint
			// and perform the activation to check if some edges result in a new breakpoint

			// ensure sorted edges so we activate in the same order as usual
			delayedActivation.flowGraph->EnsureSortedEdges();

			IFlowEdgeIteratorPtr pEdgeIter = delayedActivation.flowGraph->CreateEdgeIterator();
			if (pEdgeIter)
			{
				IFlowEdgeIterator::Edge edge;
				int edgeIndex = 0;
				while (pEdgeIter->Next(edge) && bPerform)
				{
					if (edge.fromNodeId == delayedActivation.addr.node && edge.fromPortId == delayedActivation.addr.port)
					{
						TryActivatePort(delayedActivation, edge, bPerform, edgeIndex);
						++edgeIndex;
					}
				}
			}

			m_DelayedActivations.pop_front();
		}
	}

	return bPerform;
}

void CFlowGraphDebugger::InvalidateBreakpoint()
{
	if (m_BreakPoint.type == eBT_Invalid)
		return;

	const SBreakPoint temp = m_BreakPoint;

	m_BreakPoint.addr.node = InvalidFlowNodeId;
	m_BreakPoint.addr.port = InvalidFlowPortId;
	m_BreakPoint.fromAddr.node = InvalidFlowNodeId;
	m_BreakPoint.fromAddr.port = InvalidFlowPortId;
	m_BreakPoint.flowGraph = NULL;
	m_BreakPoint.type = eBT_Invalid;
	m_BreakPoint.value.ClearUserFlag();

	m_BreakpointHit = false;

	for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnBreakpointInvalidated(temp);
	}
}

bool CFlowGraphDebugger::BreakpointHit() const
{
	return m_BreakpointHit;
}

bool CFlowGraphDebugger::RegisterListener(IFlowGraphDebugListener* pListener, const char* name)
{
	return m_Listeners.Add(pListener, name);
}

void CFlowGraphDebugger::UnregisterListener(IFlowGraphDebugListener* pListener)
{
	m_Listeners.Remove(pListener);
}

bool CFlowGraphDebugger::GetBreakpoints(DynArray<SBreakPoint>& breakpointsDynArray)
{
	if (m_DebugInfo.empty())
	{
		return false;
	}

	TDebugInfo::const_iterator iterDebugInfo = m_DebugInfo.begin();

	for (iterDebugInfo; iterDebugInfo != m_DebugInfo.end(); ++iterDebugInfo)
	{
		TFlowNodesDebugInfo flownodesDebugInfo = (*iterDebugInfo).second;
		TFlowNodesDebugInfo::const_iterator iterNodesDebugInfo = flownodesDebugInfo.begin();

		for (iterNodesDebugInfo; iterNodesDebugInfo != flownodesDebugInfo.end(); ++iterNodesDebugInfo)
		{
			SBreakPoints breakpoints = (*iterNodesDebugInfo).second;
			// Fill with INPUT port breakpoints
			FillDynArray(breakpointsDynArray, breakpoints.inputPorts, (*iterDebugInfo).first, (*iterNodesDebugInfo).first, false);
			// Fill with OUTPUT port breakpoints
			FillDynArray(breakpointsDynArray, breakpoints.outputPorts, (*iterDebugInfo).first, (*iterNodesDebugInfo).first, true);
		}
	}

	if (breakpointsDynArray.empty())
	{
		return false;
	}

	return true;
}

void CFlowGraphDebugger::FillDynArray(DynArray<SBreakPoint>& breakpointsDynArray, TFlowPortIDS portIDS, IFlowGraphPtr pFlowgraph, TFlowNodeId nodeID, bool bIsOutput)
{
	TFlowPortIDS::const_iterator iterPorts = portIDS.begin();
	for (iterPorts; iterPorts != portIDS.end(); ++iterPorts)
	{
		TFlowPortId port = (*iterPorts).portID;
		SBreakPoint breakpoint;
		breakpoint.flowGraph = pFlowgraph;
		breakpoint.addr.node = nodeID;
		breakpoint.addr.port = port;
		breakpoint.addr.isOutput = bIsOutput;

		breakpointsDynArray.push_back(breakpoint);
	}
}

IFlowGraphPtr CFlowGraphDebugger::GetRootGraph(IFlowGraphPtr pFlowGraph) const
{
	if (pFlowGraph->GetType() == IFlowGraph::eFGT_Module)
	{
		IFlowGraphModule* pModule = gEnv->pFlowSystem->GetIModuleManager()->GetModule(pFlowGraph);

		if (pModule)
		{
			return pModule->GetRootGraph();
		}
	}
	else if (pFlowGraph->GetType() == IFlowGraph::eFGT_AIAction)
	{
		if (IFlowGraphPtr pClonedGraph = pFlowGraph->GetClonedFlowGraph())
		{
			return pClonedGraph;
		}
	}

	return pFlowGraph;
}

bool CFlowGraphDebugger::EnableBreakpoint(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr, bool enable)
{
	SBreakPointPortInfo* breakpointInfo = GetBreakpointInfo(pFlowgraph, addr);

	if (breakpointInfo)
	{
		breakpointInfo->bIsEnabled = enable;

		for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
		{
			SBreakPoint breakpoint;
			breakpoint.addr = addr;
			breakpoint.flowGraph = pFlowgraph;

			notifier->OnEnableBreakpoint(breakpoint, enable);
		}

		return true;
	}

	return false;
}

bool CFlowGraphDebugger::EnableTracepoint(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr, bool enable)
{
	SBreakPointPortInfo* breakpointInfo = GetBreakpointInfo(pFlowgraph, addr);

	if (breakpointInfo)
	{
		breakpointInfo->bIsTracepoint = enable;

		for (CListenerSet<IFlowGraphDebugListener*>::Notifier notifier(m_Listeners); notifier.IsValid(); notifier.Next())
		{
			STracePoint tracepoint;
			tracepoint.addr = addr;
			tracepoint.flowGraph = pFlowgraph;

			notifier->OnEnableTracepoint(tracepoint, enable);
		}
		return true;
	}

	return false;
}

bool CFlowGraphDebugger::IsBreakpointEnabled(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr) const
{
	const SBreakPointPortInfo* breakpointInfo = GetBreakpointInfo(pFlowgraph, addr);
	return breakpointInfo ? breakpointInfo->bIsEnabled : false;
}

bool CFlowGraphDebugger::IsTracepoint(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr) const
{
	const SBreakPointPortInfo* breakpointInfo = GetBreakpointInfo(pFlowgraph, addr);
	return breakpointInfo ? breakpointInfo->bIsTracepoint : false;
}

const SBreakPointPortInfo* CFlowGraphDebugger::GetBreakpointInfo(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr) const
{
	pFlowgraph = GetRootGraph(pFlowgraph);

	TDebugInfo::const_iterator iterDebugInfo = m_DebugInfo.find(pFlowgraph);
	if (iterDebugInfo != m_DebugInfo.end())
	{
		const TFlowNodesDebugInfo* flownodesDebugInfo = &(*iterDebugInfo).second;
		TFlowNodesDebugInfo::const_iterator iterNode = flownodesDebugInfo->find(addr.node);

		if (iterNode != flownodesDebugInfo->end())
		{
			const SBreakPoints* breakPointInfo = &(*iterNode).second;
			const TFlowPortIDS* flowPortIDS = NULL;

			if (addr.isOutput)
				flowPortIDS = &breakPointInfo->outputPorts;
			else
				flowPortIDS = &breakPointInfo->inputPorts;

			TFlowPortIDS::const_iterator iter = flowPortIDS->begin();
			while (iter != flowPortIDS->end())
			{
				if ((*iter).portID == addr.port)
				{
					return &(*iter);
				}
				++iter;
			}
		}
	}

	return NULL;
}

SBreakPointPortInfo* CFlowGraphDebugger::GetBreakpointInfo(IFlowGraphPtr pFlowgraph, const SFlowAddress& addr)
{
	// code duplication vs. casting - casting wins
	return const_cast<SBreakPointPortInfo*>(
	  static_cast<const CFlowGraphDebugger&>(*this).GetBreakpointInfo(pFlowgraph, addr));
}

bool CFlowGraphDebugger::AddIgnoredFlowgraph(IFlowGraphPtr pFlowgraph)
{
	return stl::push_back_unique(m_IgnoredFlowgraphs, pFlowgraph);
}

bool CFlowGraphDebugger::RemoveIgnoredFlowgraph(IFlowGraphPtr pFlowgraph)
{
	return stl::find_and_erase(m_IgnoredFlowgraphs, pFlowgraph);
}

bool CFlowGraphDebugger::IsFlowgraphIgnored(IFlowGraphPtr pFlowgraph)
{
	return stl::find(m_IgnoredFlowgraphs, pFlowgraph);
}

bool CFlowGraphDebugger::IgnoreFlowgraphType(IFlowGraph::EFlowGraphType type, bool bIgnore)
{
	if (bIgnore)
	{
		return stl::push_back_unique(m_IgnoredFlowgraphTypes, type);
	}
	else
	{
		return stl::find_and_erase(m_IgnoredFlowgraphTypes, type);
	}
}

bool CFlowGraphDebugger::IsFlowgraphTypeIgnored(IFlowGraph::EFlowGraphType type)
{
	return stl::find(m_IgnoredFlowgraphTypes, type);
}

#endif
