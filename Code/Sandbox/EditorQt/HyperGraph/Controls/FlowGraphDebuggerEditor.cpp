// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphDebuggerEditor.h"

#include <CryFlowGraph/IFlowGraphModuleManager.h>
#include <CryGame/IGameFramework.h>  //Pause game
#include <CryInput/IHardwareMouse.h> //SetGameMode
#include <CryInput/IInput.h>
#include "GameEngine.h"   //enable flowsystem update in game engine

#include "HyperGraph/HyperGraph.h" //IsFlowgraph check
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraphEditorWnd.h"

#define VALIDATE_DEBUGGER(a)      if (!a) return false;
#define VALIDATE_DEBUGGER_VOID(a) if (!a) return;

CFlowGraphDebuggerEditor::CFlowGraphDebuggerEditor() : m_InGame(false), m_CursorVisible(false)
{
}

CFlowGraphDebuggerEditor::~CFlowGraphDebuggerEditor()
{
}

bool CFlowGraphDebuggerEditor::Init()
{
	if (GetIEditorImpl()->GetFlowGraphManager())
	{
		GetIEditorImpl()->GetFlowGraphManager()->AddListener(this);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Flowgraph Manager doesn't exist [CFlowgraphDebugger::Init]");
		return false;
	}

	if (gEnv->pFlowSystem)
	{
		m_pFlowGraphDebugger = GetIFlowGraphDebuggerPtr();

		if (m_pFlowGraphDebugger && !m_pFlowGraphDebugger->RegisterListener(this, "CFlowgraphDebuggerEditor"))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Could not register as Flowgraph Debugger listener! [CFlowgraphDebugger::Init]");
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Flowgraph System doesn't exist [CFlowgraphDebugger::Init]");
		return false;
	}

	GetIEditorImpl()->RegisterNotifyListener(this);

	return true;
}

bool CFlowGraphDebuggerEditor::Shutdown()
{
	CFlowGraphManager* pFlowgraphManager = GetIEditorImpl()->GetFlowGraphManager();
	bool bError = false;

	if (pFlowgraphManager)
	{
		pFlowgraphManager->RemoveListener(this);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Flowgraph Manager doesn't exist [CFlowgraphDebugger::~CFlowgraphDebugger]");
		bError = true;
	}

	GetIEditorImpl()->UnregisterNotifyListener(this);

	if (m_pFlowGraphDebugger)
	{
		m_pFlowGraphDebugger->UnregisterListener(this);
	}
	else
	{
		bError = true;
	}

	return !bError;
}

bool CFlowGraphDebuggerEditor::AddBreakpoint(CFlowNode* pFlowNode, CHyperNodePort* pHyperNodePort)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	SFlowAddress addr;
	addr.node = pFlowNode->GetFlowNodeId();
	addr.port = pHyperNodePort->nPortIndex;
	addr.isOutput = !pHyperNodePort->bInput;

	const bool result = m_pFlowGraphDebugger->AddBreakpoint(pFlowNode->GetIFlowGraph(), addr);

	if (result)
		pFlowNode->Invalidate(true, false);

	return result;
}

bool CFlowGraphDebuggerEditor::RemoveBreakpoint(CFlowNode* pFlowNode, CHyperNodePort* pHyperNodePort)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	SFlowAddress addr;
	addr.node = pFlowNode->GetFlowNodeId();
	addr.port = pHyperNodePort->nPortIndex;
	addr.isOutput = !pHyperNodePort->bInput;

	const bool result = m_pFlowGraphDebugger->RemoveBreakpoint(pFlowNode->GetIFlowGraph(), addr);

	if (result)
		pFlowNode->Invalidate(true, false);

	return result;
}

bool CFlowGraphDebuggerEditor::EnableBreakpoint(CFlowNode* pFlowNode, CHyperNodePort* pHyperNodePort, bool enable)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	SFlowAddress addr;
	addr.node = pFlowNode->GetFlowNodeId();
	addr.port = pHyperNodePort->nPortIndex;
	addr.isOutput = !pHyperNodePort->bInput;

	const bool result = m_pFlowGraphDebugger->EnableBreakpoint(pFlowNode->GetIFlowGraph(), addr, enable);

	if (result)
		pFlowNode->Invalidate(true, false);

	return result;
}

bool CFlowGraphDebuggerEditor::EnableTracepoint(CFlowNode* pFlowNode, CHyperNodePort* pHyperNodePort, bool enable)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	SFlowAddress addr;
	addr.node = pFlowNode->GetFlowNodeId();
	addr.port = pHyperNodePort->nPortIndex;
	addr.isOutput = !pHyperNodePort->bInput;

	const bool result = m_pFlowGraphDebugger->EnableTracepoint(pFlowNode->GetIFlowGraph(), addr, enable);

	if (result)
		pFlowNode->Invalidate(true, false);

	return result;
}

bool CFlowGraphDebuggerEditor::RemoveAllBreakpointsForNode(CFlowNode* pFlowNode)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	const bool result = m_pFlowGraphDebugger->RemoveAllBreakpointsForNode(pFlowNode->GetIFlowGraph(), pFlowNode->GetFlowNodeId());

	CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowNode->GetIFlowGraph());
	if (result && pFlowgraph)
		GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE, pFlowgraph);

	return result;
}

bool CFlowGraphDebuggerEditor::RemoveAllBreakpointsForGraph(IFlowGraphPtr pFlowGraph)
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	const bool result = m_pFlowGraphDebugger->RemoveAllBreakpointsForGraph(pFlowGraph);

	CHyperFlowGraph* pGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);
	if (result && pGraph)
		GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE, pGraph);

	return result;
}

bool CFlowGraphDebuggerEditor::HasBreakpoint(IFlowGraphPtr pFlowGraph, TFlowNodeId nodeID) const
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);
	return m_pFlowGraphDebugger->HasBreakpoint(pFlowGraph, nodeID);
}

bool CFlowGraphDebuggerEditor::HasBreakpoint(CFlowNode* pFlowNode, const CHyperNodePort* pHyperNodePort) const
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);

	SFlowAddress addr;
	addr.node = pFlowNode->GetFlowNodeId();
	addr.port = pHyperNodePort->nPortIndex;
	addr.isOutput = !pHyperNodePort->bInput;

	return m_pFlowGraphDebugger->HasBreakpoint(pFlowNode->GetIFlowGraph(), addr);
}

bool CFlowGraphDebuggerEditor::HasBreakpoint(IFlowGraphPtr pFlowGraph) const
{
	VALIDATE_DEBUGGER(m_pFlowGraphDebugger);
	return m_pFlowGraphDebugger->HasBreakpoint(pFlowGraph);
}

void CFlowGraphDebuggerEditor::PauseGame()
{
	VALIDATE_DEBUGGER_VOID(m_pFlowGraphDebugger);

	const SBreakPoint& breakpoint = m_pFlowGraphDebugger->GetBreakpoint();
	IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(breakpoint.flowGraph);

	CHyperFlowGraph* pGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);

	if (pGraph)
	{
		GetIEditorImpl()->GetFlowGraphManager()->OpenView(pGraph);

		if (CFlowNode* pNode = pGraph->FindFlowNode(breakpoint.addr.node))
		{
			CenterViewAroundNode(pNode);
			pGraph->UnselectAll();
			pNode->SetSelected(true);
		}
	}

	gEnv->pFlowSystem->Enable(false);

	if (gEnv->pGameFramework && (gEnv->pGameFramework->IsGamePaused() == false))
		gEnv->pGameFramework->PauseGame(true, true);

	if (gEnv->pHardwareMouse && (m_CursorVisible == false) && m_InGame)
	{
		gEnv->pHardwareMouse->SetGameMode(false);
		m_CursorVisible = true;
	}
}

void CFlowGraphDebuggerEditor::ResumeGame()
{
	VALIDATE_DEBUGGER_VOID(m_pFlowGraphDebugger);

	if (m_pFlowGraphDebugger->BreakpointHit())
	{
		bool bResume = true;

		const SBreakPoint& breakpoint = m_pFlowGraphDebugger->GetBreakpoint();

		IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(breakpoint.flowGraph);

		if (pFlowGraph)
		{
			bResume = m_pFlowGraphDebugger->RePerformActivation();
		}

		CHyperFlowGraph* pGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);

		if (bResume)
		{
			if (gEnv->pHardwareMouse && m_CursorVisible)
			{
				gEnv->pHardwareMouse->SetGameMode(true);
				m_CursorVisible = false;
			}

			gEnv->pFlowSystem->Enable(true);

			if (gEnv->pGameFramework)
				gEnv->pGameFramework->PauseGame(false, true);

			// give the focus back to the main view
			AfxGetMainWnd()->SetFocus();
		}

		if (pGraph)
			GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE, pGraph);

		if (gEnv->pInput)
			gEnv->pInput->ClearKeyState();
	}
}

void CFlowGraphDebuggerEditor::CenterViewAroundNode(CFlowNode* pNode) const
{
	CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		CHyperGraphDialog* pHyperGraphDialog = static_cast<CHyperGraphDialog*>(pWnd);
		CHyperGraphView* pGraphView = pHyperGraphDialog->GetGraphView();

		if (pGraphView)
		{
			pGraphView->CenterViewAroundNode(pNode, false, 0.95f);
			// we need the focus for the graph view (F5 to continue...)
			pGraphView->SetFocus();
		}
	}
}

void CFlowGraphDebuggerEditor::OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode)
{
	if (!pNode)
		return;

	switch (event)
	{
	case EHG_NODE_DELETE:
		{
			CHyperGraph* pHyperGraph = static_cast<CHyperGraph*>(pNode->GetGraph());
			// we have to check whether a deleted flownode has actually some breakpoints
			// and remove them too
			if (pHyperGraph->IsFlowGraph())
			{
				CFlowNode* pFlowNode = static_cast<CFlowNode*>(pNode);
				if (pFlowNode->IsFlowNode() && HasBreakpoint(pFlowNode->GetIFlowGraph(), pFlowNode->GetFlowNodeId()))
				{
					RemoveAllBreakpointsForNode(pFlowNode);
				}
			}
		}
		break;
	}
}

void CFlowGraphDebuggerEditor::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginLoad:
		{
			if (m_pFlowGraphDebugger)
			{
				m_pFlowGraphDebugger->ClearBreakpoints();
			}

			CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
			if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
			{
				CHyperGraphDialog* pHyperGraphDialog = static_cast<CHyperGraphDialog*>(pWnd);
				CHyperGraphView* pGraphView = pHyperGraphDialog->GetGraphView();

				pHyperGraphDialog->ClearBreakpoints();

				if (pGraphView)
					pGraphView->InvalidateView(true);
			}
		}
		break;
	case eNotify_OnBeginGameMode:
		{
			if (gEnv->pHardwareMouse && m_CursorVisible)
			{
				gEnv->pHardwareMouse->SetGameMode(true);
				m_CursorVisible = false;
			}

			m_InGame = true;
		}
		break;
	case eNotify_OnEndGameMode:
	case eNotify_OnEndSimulationMode:
		{
			if (m_pFlowGraphDebugger)
			{
				if (m_pFlowGraphDebugger->BreakpointHit())
					ResumeGame();

				m_pFlowGraphDebugger->EnableStepMode(false);
			}
			m_InGame = false;
		}
		break;
	}
}

void CFlowGraphDebuggerEditor::OnBreakpointHit(const SBreakPoint& breakpoint)
{
	VALIDATE_DEBUGGER_VOID(m_pFlowGraphDebugger);

	IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(breakpoint.flowGraph);
	CHyperFlowGraph* pGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);

	if (pGraph)
		GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE, pGraph);

	PauseGame();
}

void CFlowGraphDebuggerEditor::OnBreakpointInvalidated(const SBreakPoint& breakpoint)
{
	VALIDATE_DEBUGGER_VOID(m_pFlowGraphDebugger);

	IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(breakpoint.flowGraph);
	CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);

	if (pFlowgraph)
		GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE, pFlowgraph);
}

void CFlowGraphDebuggerEditor::OnTracepointHit(const STracePoint& tracepoint)
{
	VALIDATE_DEBUGGER_VOID(m_pFlowGraphDebugger);

	IFlowGraphPtr pFlowGraph = m_pFlowGraphDebugger->GetRootGraph(tracepoint.flowGraph);
	CHyperFlowGraph* pGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pFlowGraph);

	if (pGraph)
	{
		if (CFlowNode* pNode = pGraph->FindFlowNode(tracepoint.addr.node))
		{
			CHyperNodePort* pPort = pNode->FindPort(tracepoint.addr.port, !tracepoint.addr.isOutput);

			if (pPort)
			{
				string val;
				tracepoint.value.GetValueWithConversion(val);
				CryLog("$2[TRACEPOINT HIT - FrameID: %d] GRAPH: %s (ID: %d) - NODE: %s (ID: %d) - PORT: %s - VALUE: %s", gEnv->pRenderer->GetFrameID(), pGraph->GetName(), pGraph->GetIFlowGraph()->GetGraphId(), pNode->GetClassName(), pNode->GetId(), pNode->GetPortName(*pPort), val.c_str());
			}
		}
	}
}


#undef VALIDATE_DEBUGGER
#undef VALIDATE_DEBUGGER_VOID

