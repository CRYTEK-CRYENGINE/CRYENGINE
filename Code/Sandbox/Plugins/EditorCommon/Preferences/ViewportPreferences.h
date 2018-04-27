// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/Preferences.h"

enum EDebugSettingsFlags
{
	DBG_MEMINFO                        = 0x002,
	DBG_FRAMEPROFILE                   = 0x400,
	DBG_HIGHLIGHT_BREAKABLE            = 0x2000,
	DBG_HIGHLIGHT_MISSING_SURFACE_TYPE = 0x4000
};

//////////////////////////////////////////////////////////////////////////
// General Preferences
//////////////////////////////////////////////////////////////////////////
struct EDITOR_COMMON_API SViewportGeneralPreferences : public SPreferencePage
{
	SViewportGeneralPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	float defaultFOV;
	float defaultAspectRatio;
	int   dragSquareSize;
	float objectIconsScaleThreshold;
	float objectIconsScaleThresholdSquared;
	float objectHelperMaxDistDisplay;
	float objectHelperMaxDistSquaredDisplay;
	float selectionHelperDisplayThreshold;
	float selectionHelperDisplayThresholdSquared;
	float labelsDistance;
	int   mapViewportResolution;

	bool  applyConfigSpec;
	bool  sync2DViews;
	bool  showSafeFrame;
	bool  hideMouseCursorWhenCaptured;
	bool  enableContextMenu;
	bool  displayLabels;
	bool  displayTracks;
	bool  displayLinks;
	bool  alwaysShowRadiuses;
	bool  alwaysShowPrefabBox;
	bool  showBBoxes;
	bool  drawEntityLabels;
	bool  showTriggerBounds;
	bool  showIcons;
	bool  distanceScaleIcons;
	bool  bHideDistancedHelpers;
	bool  objectIconsOnTop;
	bool  showSizeBasedIcons;
	bool  showFrozenHelpers;
	bool  fillSelectedShapes;
	bool  showGridGuide;
	bool  displayDimension;
	bool  displaySelectedObjectOrientation;
	bool  toolsRenderUpdateMutualExclusive;
	bool  mapViewportSwapXY;
};

//////////////////////////////////////////////////////////////////////////
// Debug Preferences
//////////////////////////////////////////////////////////////////////////
struct EDITOR_COMMON_API SViewportDebugPreferences : public SPreferencePage
{
	SViewportDebugPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	void         SetObjectHideMask(int hideMask);
	int          GetObjectHideMask() const { return objectHideMask; }

	void         SetDebugFlags(int flags);
	int          GetDebugFlags() const { return debugFlags; }

	float warningIconsDrawDistance;
	bool  showMeshStatsOnMouseOver;
	bool  showScaleWarnings;
	bool  showRotationWarnings;

	bool showEntityObjectHelper;
	bool showAreaObjectHelper;
	bool showShapeObjectHelper;
	bool showBrushObjectHelper;
	bool showDecalObjectHelper;
	bool showPrefabObjectHelper;
	bool showPrefabSubObjectHelper;
	bool showGroupObjectHelper;
	bool showRoadObjectHelper;
	bool showEnviromentProbeObjectHelper;

	CCrySignal<void()> objectHideMaskChanged;
	CCrySignal<void()> debugFlagsChanged;

private:
	int objectHideMask;
	int debugFlags;
};

//////////////////////////////////////////////////////////////////////////
// Movement Preferences
//////////////////////////////////////////////////////////////////////////
struct EDITOR_COMMON_API SViewportMovementPreferences : public SPreferencePage
{
	enum EWheelBehavior
	{
		eWheel_ZoomOnly = 0,
		eWheel_SpeedOnly = 1,
		eWheel_ZoomSpeed = 2,
	};

	SViewportMovementPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	float camMoveSpeed;
	float camRotateSpeed;
	float camFastMoveSpeed;
	float wheelZoomSpeed;
	int   mouseWheelBehavior;
};

//////////////////////////////////////////////////////////////////////////
// Selection Preferences
//////////////////////////////////////////////////////////////////////////
struct EDITOR_COMMON_API SViewportSelectionPreferences : public SPreferencePage
{
	SViewportSelectionPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	ColorB colorPrefabBBox;
	ColorB colorGroupBBox;
	ColorB colorEntityBBox;
	ColorB geometryHighlightColor;
	ColorB geometrySelectionColor;
	ColorB solidBrushGeometryColor;
	float  bboxAlpha;
	float  geomAlpha;
	float  childObjectGeomAlpha;
	int    objectSelectMask;
	int    outlinePixelWidth;
	float  outlineGhostAlpha;
};

EDITOR_COMMON_API extern SViewportGeneralPreferences gViewportPreferences;
EDITOR_COMMON_API extern SViewportDebugPreferences gViewportDebugPreferences;
EDITOR_COMMON_API extern SViewportMovementPreferences gViewportMovementPreferences;
EDITOR_COMMON_API extern SViewportSelectionPreferences gViewportSelectionPreferences;

