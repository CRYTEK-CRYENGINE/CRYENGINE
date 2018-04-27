// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "FlowGraphPreferences.h"
#include "FlowGraphManager.h"

#include <CrySerialization/yasli/decorators/Range.h>

SFlowGraphGeneralPreferences gFlowGraphGeneralPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SFlowGraphGeneralPreferences, &gFlowGraphGeneralPreferences)

SFlowGraphColorPreferences gFlowGraphColorPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SFlowGraphColorPreferences, &gFlowGraphColorPreferences)

// Adding this to have colors not change if the value being loaded in failed to load.
// TODO: There should be a more generic fix for this in the preference system
namespace Private_FlowgraphColorHelper
{
	static void SerializeColor(yasli::Archive& ar, COLORREF& color, const char* szName, const char* szLabel)
	{
		ColorB tempColor = ColorB(uint32(color));
		if (ar(tempColor, szName, szLabel) && ar.isInput())
			color = tempColor.pack_abgr8888();
	}
}

//////////////////////////////////////////////////////////////////////////
// General Preferences
//////////////////////////////////////////////////////////////////////////
SFlowGraphGeneralPreferences::SFlowGraphGeneralPreferences()
	: SPreferencePage("General", "Flow Graph/General")
	, enableMigration(true)
	, showNodeIDs(false)
	, showToolTip(true)
	, edgesOnTopOfNodes(false)
	, splineArrows(true)
	, highlightEdges(true)
{
}

bool SFlowGraphGeneralPreferences::Serialize(yasli::Archive& ar)
{
	ar.openBlock("generalSettings", "General");
	ar(enableMigration, "enableMigration", "Automatic Migration");
	ar(showNodeIDs, "showNodeIDs", "Show Node IDs");
	ar(showToolTip, "showToolTip", "Show ToolTips");
	ar(edgesOnTopOfNodes, "edgesOnTopOfNodes", "Edges On Top Of Nodes");
	ar(splineArrows, "splineArrows", "Spline Edges");
	ar(highlightEdges, "highlightEdges", "Highlight Selected Edges");
	ar.closeBlock();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Color Preferences
//////////////////////////////////////////////////////////////////////////
SFlowGraphColorPreferences::SFlowGraphColorPreferences()
	: SPreferencePage("Colors", "Flow Graph/Colors")
	, opacity(75.0f)
	, colorArrow(RGBA8(25, 25, 25, 255))
	, colorInArrowHighlighted(RGBA8(255, 0, 0, 255))
	, colorOutArrowHighlighted(RGBA8(0, 161, 222, 255))
	, colorPortEdgeHighlighted(RGBA8(0, 161, 222, 255))
	, colorArrowDisabled(RGBA8(50, 50, 50, 255))
	, colorNodeOutline(RGBA8(60, 60, 60, 255))
	, colorNodeOutlineSelected(RGBA8(0, 161, 222, 255))
	, colorNodeBkg(RGBA8(60, 60, 60, 255))
	, colorNodeSelected(RGBA8(0, 161, 222, 255))
	, colorTitleText(RGBA8(0, 0, 0, 255))
	, colorTitleTextSelected(RGBA8(0, 0, 0, 255))
	, colorText(RGBA8(191, 191, 191, 255))
	, colorBackground(RGBA8(86, 86, 86, 255))
	, colorGrid(RGBA8(80, 80, 80, 255))
	, colorBreakPoint(RGBA8(255, 0, 0, 255))
	, colorBreakPointDisabled(RGBA8(255, 0, 0, 255))
	, colorBreakPointArrow(RGBA8(255, 255, 0, 255))
	, colorEntityPortNotConnected(RGBA8(255, 90, 90, 255))
	, colorPort(RGBA8(80, 80, 80, 255))
	, colorPortSelected(RGBA8(0, 161, 222, 255))
	, colorEntityTextInvalid(RGBA8(255, 255, 255, 255))
	, colorDownArrow(RGBA8(25, 25, 25, 255))
	, colorCustomNodeBkg(RGBA8(170, 170, 170, 255))
	, colorCustomSelectedNodeBkg(RGBA8(225, 255, 245, 255))
	, colorPortDebuggingInitialization(RGBA8(18, 228, 36, 255))
	, colorPortDebugging(RGBA8(228, 202, 66, 255))
	, colorPortDebuggingText(RGBA8(0, 0, 0, 255))
	, colorQuickSearchBackground(RGBA8(60, 60, 60, 255))
	, colorQuickSearchResultText(RGBA8(0, 161, 222, 255))
	, colorQuickSearchCountText(RGBA8(255, 255, 255, 255))
	, colorQuickSearchBorder(RGBA8(0, 0, 0, 255))
	, colorDebugNodeTitle(RGBA8(220, 190, 40, 255))
	, colorDebugNode(RGBA8(104, 53, 148, 255))
	, colorNodeBckgdError(RGBA8(166, 47, 39, 255))
	, colorNodeTitleError(RGBA8(255, 19, 11, 255))
{
}

bool SFlowGraphColorPreferences::Serialize(yasli::Archive& ar)
{
	ar(yasli::Range(opacity, 1.f, 100.f), "opacity", "Opacity");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorArrow, "colorArrow", "Arrow Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorInArrowHighlighted, "colorInArrowHighlighted", "InArrow Highlighted Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorOutArrowHighlighted, "colorOutArrowHighlighted", "OutArrow Highlighted Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorPortEdgeHighlighted, "colorPortEdgeHighlighted", "Port Edge Highlighted Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorArrowDisabled, "colorArrowDisabled", "Arrow Disabled Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorNodeOutline, "colorNodeOutline", "Node Outline Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorNodeOutlineSelected, "colorNodeOutlineSelected", "Node Outline Color Selected");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorNodeBkg, "colorNodeBkg", "Node Background Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorNodeSelected, "colorNodeSelected", "Node Selected Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorTitleText, "colorTitleText", "Title Text Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorTitleTextSelected, "colorTitleTextSelected", "Title Text Color Selected");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorText, "colorText", "Text Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorBackground, "colorBackground", "Background Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorGrid, "colorGrid", "Grid Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorBreakPoint, "colorBreakPoint", "Breakpoint Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorBreakPointDisabled, "colorBreakPointDisabled", "Breakpoint Disabled Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorBreakPointArrow, "colorBreakPointArrow", "Breakpoint Arrow Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorEntityPortNotConnected, "colorEntityPortNotConnected", "Entity Port Not Connected Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorPort, "colorPort", "Port Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorPortSelected, "colorPortSelected", "Port Selected Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorEntityTextInvalid, "colorEntityTextInvalid", "Entity Invalid Text Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorDownArrow, "colorDownArrow", "Down Arrow Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorCustomNodeBkg, "colorCustomNodeBkg", "Custom Node Background Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorCustomSelectedNodeBkg, "colorCustomSelectedNodeBkg", "Custom Node Selected Background Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorPortDebuggingInitialization, "colorPortDebuggingInitialization", "Port Debugging Color for the Initialization Step");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorPortDebugging, "colorPortDebugging", "Port Debugging Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorPortDebuggingText, "colorPortDebuggingText", "Port Text Debugging Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorQuickSearchBackground, "colorQuickSearchBackground", "Quick Search Background Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorQuickSearchResultText, "colorQuickSearchResultText", "Quick Search Result Text Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorQuickSearchCountText, "colorQuickSearchCountText", "Quick Search Count Text Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorQuickSearchBorder, "colorQuickSearchBorder", "Quick Search Border Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorDebugNodeTitle, "colorNodeDebugTitle", "Debug Node Title Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorDebugNode, "colorNodeDebug", "Debug Node Background Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorNodeBckgdError, "colorNodeBckgdError", "Node w/ Error Background Color");
	Private_FlowgraphColorHelper::SerializeColor(ar, colorNodeTitleError, "colorNodeTitleError", "Node w/ Error Title Color");

	if (ar.isInput())
	{
		CFlowGraphManager* pFlowgraphManager = GetIEditorImpl()->GetFlowGraphManager();
		if (pFlowgraphManager)
			pFlowgraphManager->SendNotifyEvent(EHG_GRAPH_INVALIDATE);
	}

	return true;
}

