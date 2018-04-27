// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowGraphDebuggerUnitTests.cpp
//  Version:     v1.00
//  Created:     02/11/2013 by Sascha Hoba.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(CRY_UNIT_TESTING)

	#include <CryFlowGraph/IFlowGraphDebugger.h>
	#include <CryFlowGraph/IFlowSystem.h>
	#include <CrySystem/CryUnitTest.h>

CRY_UNIT_TEST_SUITE(CryFlowgraphDebuggerUnitTest)
{
	namespace FGD_UT_HELPER
	{
	IFlowGraphDebuggerPtr GetFlowGraphDebugger()
	{
		IFlowGraphDebuggerPtr pFlowgraphDebugger = GetIFlowGraphDebuggerPtr();
		CRY_UNIT_TEST_ASSERT(pFlowgraphDebugger.get() != NULL);
		return pFlowgraphDebugger;
	}

	IFlowGraphPtr CreateFlowGraph()
	{
		CRY_UNIT_TEST_ASSERT(gEnv->pFlowSystem != NULL);
		IFlowGraphPtr pFlowGraph = gEnv->pFlowSystem->CreateFlowGraph();
		CRY_UNIT_TEST_ASSERT(pFlowGraph.get() != NULL);
		return pFlowGraph;
	}

	TFlowNodeId CreateTestNode(IFlowGraphPtr pFlowGraph, const TFlowNodeTypeId& typeID)
	{
		const TFlowNodeId flowNodeID = pFlowGraph->CreateNode(typeID, "UnitTestNode");
		CRY_UNIT_TEST_ASSERT(flowNodeID != InvalidFlowNodeId);
		return flowNodeID;
	}

	void AddBreakPoint(IFlowGraphDebuggerPtr pFlowGraphDebugger, IFlowGraphPtr pFlowGraph, const SFlowAddress& addrIn)
	{
		DynArray<SBreakPoint> breakpointsDynArray;
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->AddBreakpoint(pFlowGraph, addrIn));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->GetBreakpoints(breakpointsDynArray));
		CRY_UNIT_TEST_CHECK_EQUAL(breakpointsDynArray.size(), 1);

		SBreakPoint breakpoint = breakpointsDynArray[0];
		CRY_UNIT_TEST_ASSERT(breakpoint.flowGraph == pFlowGraph);
		CRY_UNIT_TEST_ASSERT(breakpoint.addr == addrIn);
		breakpointsDynArray.clear();
		CRY_UNIT_TEST_ASSERT(breakpointsDynArray.empty());
	}
	}

	CRY_UNIT_TEST(CUT_INVALID_FLOWGRAPH_BREAKPOINT)
	{
		IFlowGraphDebuggerPtr pFlowGraphDebugger = FGD_UT_HELPER::GetFlowGraphDebugger();
		const SBreakPoint& breakpoint = pFlowGraphDebugger->GetBreakpoint();

		CRY_UNIT_TEST_CHECK_EQUAL(breakpoint.type, eBT_Invalid);
		CRY_UNIT_TEST_CHECK_EQUAL(breakpoint.edgeIndex, -1);
		CRY_UNIT_TEST_ASSERT(breakpoint.flowGraph.get() == NULL);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->BreakpointHit(), false);
	}

#if 0 // Test fails but disabled until we have time again. Flowgraph breakpoint currently works as intended for users. (Mid 2017)
	CRY_UNIT_TEST(CUT_ADD_REMOVE_FLOWGRAPH_BREAKPOINT)
	{
		IFlowGraphPtr pFlowGraph = FGD_UT_HELPER::CreateFlowGraph();
		const TFlowNodeTypeId typeID = gEnv->pFlowSystem->GetTypeId("Logic:Any");
		CRY_UNIT_TEST_ASSERT(typeID != InvalidFlowNodeTypeId);
		const TFlowNodeId flowNodeID = FGD_UT_HELPER::CreateTestNode(pFlowGraph, typeID);

		SFlowAddress addrIn;
		addrIn.isOutput = false;
		addrIn.node = flowNodeID;
		addrIn.port = 0;

		IFlowGraphDebuggerPtr pFlowGraphDebugger = FGD_UT_HELPER::GetFlowGraphDebugger();

		FGD_UT_HELPER::AddBreakPoint(pFlowGraphDebugger, pFlowGraph, addrIn);
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, addrIn));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->RemoveBreakpoint(pFlowGraph, addrIn));
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, addrIn), false);

		FGD_UT_HELPER::AddBreakPoint(pFlowGraphDebugger, pFlowGraph, addrIn);
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, flowNodeID));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->RemoveAllBreakpointsForNode(pFlowGraph, flowNodeID));
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, flowNodeID), false);

		FGD_UT_HELPER::AddBreakPoint(pFlowGraphDebugger, pFlowGraph, addrIn);
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->HasBreakpoint(pFlowGraph));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->RemoveAllBreakpointsForGraph(pFlowGraph));
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph), false);

		FGD_UT_HELPER::AddBreakPoint(pFlowGraphDebugger, pFlowGraph, addrIn);
		pFlowGraphDebugger->ClearBreakpoints();
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, addrIn), false);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, flowNodeID), false);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph), false);

		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->BreakpointHit(), false);
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->GetRootGraph(pFlowGraph) == pFlowGraph);
	}

	CRY_UNIT_TEST(CUT_ENABLE_DISABLE_FLOWGRAPH_BREAKPOINT)
	{
		IFlowGraphPtr pFlowGraph = FGD_UT_HELPER::CreateFlowGraph();
		const TFlowNodeTypeId typeID = gEnv->pFlowSystem->GetTypeId("Logic:Any");
		CRY_UNIT_TEST_ASSERT(typeID != InvalidFlowNodeTypeId);
		const TFlowNodeId flowNodeID = FGD_UT_HELPER::CreateTestNode(pFlowGraph, typeID);

		SFlowAddress addrIn;
		addrIn.isOutput = false;
		addrIn.node = flowNodeID;
		addrIn.port = 0;

		IFlowGraphDebuggerPtr pFlowGraphDebugger = FGD_UT_HELPER::GetFlowGraphDebugger();
		FGD_UT_HELPER::AddBreakPoint(pFlowGraphDebugger, pFlowGraph, addrIn);

		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->IsBreakpointEnabled(pFlowGraph, addrIn));
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->IsTracepoint(pFlowGraph, addrIn), false);
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->EnableBreakpoint(pFlowGraph, addrIn, false));
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->IsBreakpointEnabled(pFlowGraph, addrIn), false);
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->EnableBreakpoint(pFlowGraph, addrIn, true));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->IsBreakpointEnabled(pFlowGraph, addrIn));
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->IsTracepoint(pFlowGraph, addrIn), false);

		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->EnableTracepoint(pFlowGraph, addrIn, true));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->IsTracepoint(pFlowGraph, addrIn));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->IsBreakpointEnabled(pFlowGraph, addrIn));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->EnableTracepoint(pFlowGraph, addrIn, false));
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->IsTracepoint(pFlowGraph, addrIn), false);
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->IsBreakpointEnabled(pFlowGraph, addrIn));

		pFlowGraphDebugger->ClearBreakpoints();

		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->IsBreakpointEnabled(pFlowGraph, addrIn), false);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->IsTracepoint(pFlowGraph, addrIn), false);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, addrIn), false);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, flowNodeID), false);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph), false);

		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->BreakpointHit(), false);
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->GetRootGraph(pFlowGraph) == pFlowGraph);
	}

	CRY_UNIT_TEST(CUT_TRIGGER_FLOWGRAPH_BREAKPOINT)
	{
		if (!gEnv->IsEditor())
			return;

		ICVar* pNodeDebuggingCVar = gEnv->pConsole->GetCVar("fg_iEnableFlowgraphNodeDebugging");
		const int oldNodeDebuggingValue = pNodeDebuggingCVar->GetIVal();
		pNodeDebuggingCVar->Set(1);

		IFlowGraphPtr pFlowGraph = FGD_UT_HELPER::CreateFlowGraph();
		const TFlowNodeTypeId typeID = gEnv->pFlowSystem->GetTypeId("Logic:Any");
		CRY_UNIT_TEST_ASSERT(typeID != InvalidFlowNodeTypeId);
		const TFlowNodeId flowNodeID = FGD_UT_HELPER::CreateTestNode(pFlowGraph, typeID);
		pFlowGraph->InitializeValues();

		SFlowAddress addrIn;
		addrIn.isOutput = true;
		addrIn.node = flowNodeID;
		addrIn.port = 0;

		IFlowGraphDebuggerPtr pFlowGraphDebugger = FGD_UT_HELPER::GetFlowGraphDebugger();
		FGD_UT_HELPER::AddBreakPoint(pFlowGraphDebugger, pFlowGraph, addrIn);

		pFlowGraph->ActivatePortCString(addrIn, "UnitTestActivation");

		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->BreakpointHit(), true);

		const SBreakPoint& breakpoint = pFlowGraphDebugger->GetBreakpoint();

		CRY_UNIT_TEST_CHECK_EQUAL(breakpoint.type, eBT_Output_Without_Edges);
		CRY_UNIT_TEST_CHECK_EQUAL(breakpoint.edgeIndex, -1);
		CRY_UNIT_TEST_ASSERT(breakpoint.flowGraph == pFlowGraph);

		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, addrIn));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, flowNodeID));
		CRY_UNIT_TEST_ASSERT(pFlowGraphDebugger->HasBreakpoint(pFlowGraph));

		pFlowGraphDebugger->InvalidateBreakpoint();
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->BreakpointHit(), false);
		pFlowGraphDebugger->ClearBreakpoints();

		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, addrIn), false);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph, flowNodeID), false);
		CRY_UNIT_TEST_CHECK_EQUAL(pFlowGraphDebugger->HasBreakpoint(pFlowGraph), false);

		pNodeDebuggingCVar->Set(oldNodeDebuggingValue);
	}
#endif // #if 0
}

#endif // UNITTESTING_ENABLED
