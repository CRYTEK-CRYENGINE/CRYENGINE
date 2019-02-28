// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "ViewportPreferences.h"

#include <CrySerialization/Color.h>
#include <CrySerialization/StringList.h>
#include <CrySerialization/yasli/decorators/Range.h>

EDITOR_COMMON_API SViewportGeneralPreferences gViewportPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SViewportGeneralPreferences, &gViewportPreferences)

EDITOR_COMMON_API SViewportMovementPreferences gViewportMovementPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SViewportMovementPreferences, &gViewportMovementPreferences)

EDITOR_COMMON_API SViewportDebugPreferences gViewportDebugPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SViewportDebugPreferences, &gViewportDebugPreferences)

EDITOR_COMMON_API SViewportSelectionPreferences gViewportSelectionPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SViewportSelectionPreferences, &gViewportSelectionPreferences)

//////////////////////////////////////////////////////////////////////////
// General Preferences
//////////////////////////////////////////////////////////////////////////
SViewportGeneralPreferences::SViewportGeneralPreferences()
	: SPreferencePage("General", "Viewport/General")
	, defaultFOV(DEG2RAD(60))
	, defaultAspectRatio(800.0f / 600.0f)
	, dragSquareSize(GetSystemMetrics(SM_CXDRAG) * 2)
	, objectIconsScaleThreshold(50)
	, objectIconsScaleThresholdSquared(objectIconsScaleThreshold * objectIconsScaleThreshold)
	, objectHelperMaxDistDisplay(70)
	, objectHelperMaxDistSquaredDisplay(objectHelperMaxDistDisplay * objectHelperMaxDistDisplay)
	, selectionHelperDisplayThreshold(30)
	, selectionHelperDisplayThresholdSquared(selectionHelperDisplayThreshold * selectionHelperDisplayThreshold)
	, labelsDistance(100)
	, mapViewportResolution(512)
	, applyConfigSpec(true)
	, sync2DViews(true)
	, showSafeFrame(false)
	, hideMouseCursorWhenCaptured(true)
	, enableContextMenu(true)
	, displayLabels(false)
	, displayTracks(true)
	, displayLinks(true)
	, alwaysShowRadiuses(false)
	, alwaysShowPrefabBox(false)
	, showBBoxes(false)
	, drawEntityLabels(false)
	, showTriggerBounds(false)
	, showIcons(true)
	, distanceScaleIcons(true)
	, bHideDistancedHelpers(true)
	, objectIconsOnTop(true)
	, showSizeBasedIcons(false)
	, showFrozenHelpers(true)
	, fillSelectedShapes(false)
	, showGridGuide(true)
	, displayDimension(false)
	, displaySelectedObjectOrientation(false)
	, toolsRenderUpdateMutualExclusive(false)
	, mapViewportSwapXY(false)
{
}

bool SViewportGeneralPreferences::Serialize(yasli::Archive& ar)
{
	char currResString[8];
	sprintf(currResString, "%d", mapViewportResolution);
	Serialization::StringList resolutionOptions;
	resolutionOptions.push_back("128");
	resolutionOptions.push_back("256");
	resolutionOptions.push_back("512");
	resolutionOptions.push_back("1024");
	resolutionOptions.push_back("2048");
	resolutionOptions.push_back("4096");
	Serialization::StringListValue resolutionValue(resolutionOptions,
	                                               std::max(resolutionOptions.find(currResString), 0));
	float defaultFOVDeg = RAD2DEG(defaultFOV);

	ar.openBlock("general", "General");
	ar(applyConfigSpec, "applyConfigSpec", "Hide objects by config spec");
	ar(sync2DViews, "sync2DViews", "Synchronize 2D Viewports");
	ar(defaultFOVDeg, "defaultFOV", "Perspective View FOV"); // IVariable::DT_ANGLE
	ar(defaultAspectRatio, "defaultAspectRatio", "Perspective View Aspect Ratio");
	ar(enableContextMenu, "enableContextMenu", "Enable Right-Click Context Menu");
	ar.closeBlock();

	ar.openBlock("display", "Display");
	ar(showSafeFrame, "showSafeFrame", "Show 4:3 Aspect Ratio Frame");
	ar(hideMouseCursorWhenCaptured, "hideMouseCursorWhenCaptured", "Hide Mouse Cursor When Captured");
	ar(dragSquareSize, "dragSquareSize", "Drag Square Size");

	ar(displayLinks, "displayLinks", "Display Object Links");
	ar(displayTracks, "displayTracks", "Display Animation Tracks");
	ar(alwaysShowRadiuses, "alwaysShowRadiuses", "Always Show Radiuses");
	ar(alwaysShowPrefabBox, "alwaysShowPrefabBox", "Always Show Prefab Bounds");
	ar(showBBoxes, "showBBoxes", "Show Bounding Boxes");
	ar(drawEntityLabels, "drawEntityLabels", "Always Draw Entity Labels");
	ar(showTriggerBounds, "showTriggerBounds", "Always Show Trigger Bounds");
	ar(showIcons, "showIcons", "Show Object Icons");

	ar(bHideDistancedHelpers, "hideDistancedHelpers", "Hide distanced helpers");
	if (bHideDistancedHelpers)
	{
		ar(yasli::Range(objectHelperMaxDistDisplay, 0.0f, 500.0f), "objectHelperMaxDistDisplay", "Display helpers up to given distance");
	}

	ar(distanceScaleIcons, "distanceScaleIcons", "Scale object icons with distance");
	if (distanceScaleIcons)
	{
		ar(yasli::Range(objectIconsScaleThreshold, 0.0f, 500.0f), "objectIconsScaleThreshold", "Threshold for distance scaling");
	}

	ar(objectIconsOnTop, "distanceScaleIconsThreshold", "Show object icons on top of objects");
	ar(yasli::Range(selectionHelperDisplayThreshold, 0.0f, 500.0f), "selectionHelperDisplayThreshold", "Threshold for showing selection helpers");
	ar(showFrozenHelpers, "showFrozenHelpers", "Show Helpers of Frozen Objects");
	ar(fillSelectedShapes, "fillSelectedShapes", "Fill Selected Shapes");
	ar(showGridGuide, "showGridGuide", "Show Snapping Grid Guide");
	ar(displayDimension, "displayDimension", "Display dimension figures");
	ar(displaySelectedObjectOrientation, "displaySelectedObjectOrientation", "Display Selected Object Orientation");
	ar(toolsRenderUpdateMutualExclusive, "toolsRenderUpdateMutualExclusive", "Tools Render Update Mutual Exclusive");
	ar.closeBlock();

	ar.openBlock("mapViewport", "Map Viewport");
	ar(mapViewportSwapXY, "mapViewportSwapXY", "Swap X/Y Axis");
	ar(resolutionValue, "mapViewportResolution", "Map Texture Resolution");
	ar.closeBlock();

	ar.openBlock("textLabels", "Text Labels");
	ar(displayLabels, "displayLabels", "Enabled");
	ar(yasli::Range(labelsDistance, 0.0f, 1000.0f), "labelsDistance", "Distance");
	ar.closeBlock();

	if (ar.isInput())
	{
		mapViewportResolution = atoi(resolutionValue.c_str());
		defaultFOV = DEG2RAD(defaultFOVDeg);
		objectIconsScaleThresholdSquared = pow(objectIconsScaleThreshold, 2);
		selectionHelperDisplayThresholdSquared = pow(selectionHelperDisplayThreshold, 2);
		objectHelperMaxDistSquaredDisplay = objectHelperMaxDistDisplay * objectHelperMaxDistDisplay;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Debug Preferences
//////////////////////////////////////////////////////////////////////////
SViewportDebugPreferences::SViewportDebugPreferences()
	: SPreferencePage("Debug", "Viewport/Debug")
	, objectHideMask(0)
	, debugFlags(0)
	, warningIconsDrawDistance(50.0f)
	, showMeshStatsOnMouseOver(false)
	, showScaleWarnings(false)
	, showRotationWarnings(false)
	, showEntityObjectHelper(true)
	, showAreaObjectHelper(true)
	, showShapeObjectHelper(true)
	, showBrushObjectHelper(true)
	, showDecalObjectHelper(true)
	, showPrefabObjectHelper(true)
	, showPrefabSubObjectHelper(true)
	, showRoadObjectHelper(true)
	, showGroupObjectHelper(true)
	, showEnviromentProbeObjectHelper(true)
{
}

bool SViewportDebugPreferences::Serialize(yasli::Archive& ar)
{
	ar.openBlock("debugHighlight", "Debug Highlight");
	bool dbgHighlightBreakable = debugFlags & DBG_HIGHLIGHT_BREAKABLE;
	bool dbgHighlightMissingSurfaceType = debugFlags & DBG_HIGHLIGHT_MISSING_SURFACE_TYPE;
	ar(dbgHighlightBreakable, "highlightBreakable", "Breakable Materials");
	ar(dbgHighlightMissingSurfaceType, "highlightMissingSurfaceType", "Missing Surface Types");
	ar.closeBlock();

	ar.openBlock("profiling", "Profiling");
	ar(showMeshStatsOnMouseOver, "showMeshStatsOnMouseOver", "Show Mesh Statistics on Mouse Over");
	ar.closeBlock();

	ar.openBlock("warnings", "Warnings");
	ar(yasli::Range(warningIconsDrawDistance, 0.1f, 10000.f), "warningIconsDrawDistance", "Warning Icons Draw Distance");
	ar(showScaleWarnings, "showScaleWarnings", "Show Scale Warnings");
	ar(showRotationWarnings, "showRotationWarnings", "Show Rotation Warnings");
	ar.closeBlock();

	ar.openBlock("helper", "Debug Helper");
	ar(showEntityObjectHelper, "showEntityObjectHelper", "Show Entity Object Helper");
	ar(showAreaObjectHelper, "showAreaObjectHelper", "Show Area Helper");
	ar(showShapeObjectHelper, "showShapeObjectHelper", "Show Shape Helper");
	ar(showBrushObjectHelper, "showBrushObjectHelper", "Show Brush Helper");
	ar(showDecalObjectHelper, "showDecalObjectHelper", "Show Decal Helper");
	ar(showPrefabObjectHelper, "showPrefabObjectHelper", "Show Prefab Helper");
	ar(showPrefabSubObjectHelper, "showPrefabChildrenObjectHelper", "Show Prefab Children Helper");
	ar(showRoadObjectHelper, "showRoadObjectHelper", "Show Road Helper");
	ar(showGroupObjectHelper, "showRoadObjectHelper", "Show Group Helper");
	ar(showEnviromentProbeObjectHelper, "showEnviromentProbeObjectHelper", "Show Environment Probe Helper");
	ar.closeBlock();

	if (ar.isInput())
	{
		int flags = debugFlags;
		flags &= ~(DBG_HIGHLIGHT_BREAKABLE | DBG_HIGHLIGHT_MISSING_SURFACE_TYPE);
		flags |= (dbgHighlightBreakable) ? DBG_HIGHLIGHT_BREAKABLE : 0;
		flags |= (dbgHighlightMissingSurfaceType) ? DBG_HIGHLIGHT_MISSING_SURFACE_TYPE : 0;
		SetDebugFlags(flags);
	}

	return true;
}

void SViewportDebugPreferences::SetDebugFlags(int flags)
{
	debugFlags = flags;
	bool profilerEnabled = false;

	if (gEnv->pConsole)
	{
		const ICVar* const pProfileCVar = gEnv->pConsole->GetCVar("Profile");
		if (pProfileCVar)
		{
			profilerEnabled = (pProfileCVar->GetIVal() == 1);
			if (profilerEnabled)
			{
				debugFlags |= DBG_FRAMEPROFILE;
			}
			else
			{
				debugFlags &= ~DBG_FRAMEPROFILE;
			}
		}
	}

	ISystem* pSystem = GetIEditor()->GetSystem();
	if (pSystem)
	{
		pSystem->GetIProfileSystem()->Enable(profilerEnabled, profilerEnabled);
	}

	debugFlagsChanged();
}

void SViewportDebugPreferences::SetObjectHideMask(int hideMask)
{
	objectHideMask = hideMask;
	objectHideMaskChanged();
};

//////////////////////////////////////////////////////////////////////////
// Movement Preferences
//////////////////////////////////////////////////////////////////////////
SViewportMovementPreferences::SViewportMovementPreferences()
	: SPreferencePage("Speed", "Viewport/Movement")
	, camMoveSpeed(1)
	, camRotateSpeed(1)
	, camFastMoveSpeed(2)
	, wheelZoomSpeed(1)
	, mouseWheelBehavior(eWheel_ZoomSpeed)
{
}

bool SViewportMovementPreferences::Serialize(yasli::Archive& ar)
{
	Serialization::StringList wheelBehaviorOptions;
	wheelBehaviorOptions.push_back("Zoom");
	wheelBehaviorOptions.push_back("Change Speed");
	wheelBehaviorOptions.push_back("Zoom, Change Speed When Moving");
	Serialization::StringListValue wheelBehavior(wheelBehaviorOptions, mouseWheelBehavior);

	ar.openBlock("cameraSpeed", "Speed");
	ar(yasli::Range(camMoveSpeed, 0.1f, 100.f), "camMoveSpeed", "Movement Speed");
	ar(yasli::Range(camRotateSpeed, 0.1f, 100.f), "camRotateSpeed", "Rotation Speed");
	ar(yasli::Range(camFastMoveSpeed, 1.1f, 100.f), "camFastMoveSpeed", "Fast Movement Multiplier");
	ar(yasli::Range(wheelZoomSpeed, 0.1f, 100.0f), "wheelZoomSpeed", "Wheel Zoom Speed");
	ar(wheelBehavior, "wheelBehavior", "Mouse Wheel Behavior");
	ar.closeBlock();

	if (ar.isInput())
		mouseWheelBehavior = wheelBehavior.index();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Selection Preferences
//////////////////////////////////////////////////////////////////////////
SViewportSelectionPreferences::SViewportSelectionPreferences()
	: SPreferencePage("General", "Viewport/Selection")
	, bboxAlpha(0.3f)
	, geomAlpha(0.2f)
	, childObjectGeomAlpha(0.4f)
	, objectSelectMask(0xFFFFFFFF)
	, outlinePixelWidth(2)
	, outlineGhostAlpha(0.2f)
{
	colorPrefabBBox = ColorB(uint32(RGB(214, 172, 5)), geomAlpha);
	colorGroupBBox = ColorB(uint32(RGB(0, 255, 0)), geomAlpha);
	colorEntityBBox = ColorB(uint32(RGB(112, 117, 102)), geomAlpha);
	geometryHighlightColor = ColorB(uint32(RGB(220, 220, 220)), geomAlpha);
	geometrySelectionColor = ColorB(uint32(RGB(255, 146, 21)), geomAlpha);
	solidBrushGeometryColor = ColorB(uint32(RGB(192, 0, 192)), geomAlpha);
}

bool SViewportSelectionPreferences::Serialize(yasli::Archive& ar)
{
	ar.openBlock("selectionPreviewColor", "Selection Preview Color");
	ar(colorPrefabBBox, "colorPrefabBBox", "Prefab Bounding Box");
	ar(colorGroupBBox, "colorGroupBBox", "Group Bounding Box");
	ar(colorEntityBBox, "colorEntityBBox", "Entity Bounding Box");
	ar(yasli::Range(bboxAlpha, 0.f, 1.f), "bboxAlpha", "Bounding Box Highlight Alpha");
	ar(geometryHighlightColor, "geometryHighlightColor", "Highlight Color");
	ar(geometrySelectionColor, "geometrySelectionColor", "Selection Color");
	ar(yasli::Range(outlinePixelWidth, 0, 10), "outlinePixelWidth", "Outline Width");
	ar(yasli::Range(outlineGhostAlpha, 0.f, 1.f), "outlineGhostAlpha", "Outline Ghost Alpha");
	ar(yasli::Range(geomAlpha, 0.f, 1.f), "geomAlpha", "Geometry Highlight Alpha");
	ar(solidBrushGeometryColor, "solidBrushGeometryColor", "Solid Brush Geometry Color");
	ar(yasli::Range(childObjectGeomAlpha, 0.f, 1.f), "childObjectGeomAlpha", "Child Geometry Highlight Alpha");
	ar.closeBlock();

	if (ar.isInput())
	{
		signalSettingsChanged();
	}

	return true;
}

