// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <EditorFramework/Preferences.h>

//////////////////////////////////////////////////////////////////////////
// General Preferences
//////////////////////////////////////////////////////////////////////////
struct SFlowGraphGeneralPreferences : public SPreferencePage
{
	SFlowGraphGeneralPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	bool enableMigration;
	bool showNodeIDs;
	bool showToolTip;
	bool edgesOnTopOfNodes;
	bool splineArrows;
	bool highlightEdges;
};

//////////////////////////////////////////////////////////////////////////
// Color Preferences
//////////////////////////////////////////////////////////////////////////
struct SFlowGraphColorPreferences : public SPreferencePage
{
	SFlowGraphColorPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

public:
	float    opacity;
	COLORREF colorArrow;
	COLORREF colorInArrowHighlighted;
	COLORREF colorOutArrowHighlighted;
	COLORREF colorPortEdgeHighlighted;
	COLORREF colorArrowDisabled;
	COLORREF colorNodeOutline;
	COLORREF colorNodeOutlineSelected;
	COLORREF colorNodeBkg;
	COLORREF colorNodeSelected;
	COLORREF colorTitleTextSelected;
	COLORREF colorTitleText;
	COLORREF colorText;
	COLORREF colorBackground;
	COLORREF colorGrid;
	COLORREF colorBreakPoint;
	COLORREF colorBreakPointDisabled;
	COLORREF colorBreakPointArrow;
	COLORREF colorEntityPortNotConnected;
	COLORREF colorPort;
	COLORREF colorPortSelected;
	COLORREF colorEntityTextInvalid;
	COLORREF colorDownArrow;
	COLORREF colorCustomNodeBkg;
	COLORREF colorCustomSelectedNodeBkg;
	COLORREF colorPortDebuggingInitialization;
	COLORREF colorPortDebugging;
	COLORREF colorPortDebuggingText;
	COLORREF colorQuickSearchBackground;
	COLORREF colorQuickSearchResultText;
	COLORREF colorQuickSearchCountText;
	COLORREF colorQuickSearchBorder;
	COLORREF colorDebugNodeTitle;
	COLORREF colorDebugNode;
	COLORREF colorNodeBckgdError;
	COLORREF colorNodeTitleError;
};

extern SFlowGraphGeneralPreferences gFlowGraphGeneralPreferences;
extern SFlowGraphColorPreferences gFlowGraphColorPreferences;

#define GET_TRANSPARENCY (gFlowGraphColorPreferences.opacity * 2.55f)
#define GET_GDI_COLOR(x)                 (Gdiplus::Color(GET_TRANSPARENCY, GetRValue(x), GetGValue(x), GetBValue(x)))
#define GET_GDI_COLOR_NO_TRANSPARENCY(x) (Gdiplus::Color(GetRValue(x), GetGValue(x), GetBValue(x)))

