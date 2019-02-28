// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QtUtil.h"
#include "Util/FileUtil.h"
#include "FilePathUtil.h"
#include "EditMode/VertexSnappingModeTool.h"
#include "EditMode/ObjectMode.h"
#include "HyperGraph/FlowGraphPreferences.h"
#include "Mannequin/MannPreferences.h"
#include "Grid.h"

#include <EditorFramework/Preferences.h>
#include <EditorFramework/Editor.h>
#include <Preferences/GeneralPreferences.h>
#include <Preferences/LightingPreferences.h>
#include <Preferences/ViewportPreferences.h>

#include <QSettings>

#include "IEditorImpl.h"
#include "AI/AIManager.h"
#include "Gizmos/AxisHelper.h"

static QSettings* s_pCurrentQSettings = 0;

const int EditorSettingsVersion = 1; // bump this up on every substantial settings change

//////////////////////////////////////////////////////////////////////////
SEditorSettings::SEditorSettings()
{
}

inline QVariant GetQtSettingsValue(const char* sSection, const char* sKey, const QVariant& deflt)
{
	QVariant var;
	if (s_pCurrentQSettings)
	{
		s_pCurrentQSettings->beginGroup(sSection);
		var = s_pCurrentQSettings->value(sKey, deflt);
		s_pCurrentQSettings->endGroup();
	}
	return var;
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, int& value)
{
	value = GetQtSettingsValue(sSection, sKey, QVariant(value)).toInt();
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, uint32& value)
{
	value = GetQtSettingsValue(sSection, sKey, QVariant(value)).toUInt();
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, unsigned long& value)
{
	value = GetQtSettingsValue(sSection, sKey, QVariant((uint32)value)).toUInt();
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, float& value)
{
	value = GetQtSettingsValue(sSection, sKey, QVariant(value)).toFloat();
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, bool& value)
{
	value = GetQtSettingsValue(sSection, sKey, QVariant(value)).toBool();
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, string& value)
{
	value = GetQtSettingsValue(sSection, sKey, QVariant(value.c_str())).toString().toStdString().c_str();
}

//////////////////////////////////////////////////////////////////////////

void SEditorSettings::Load()
{
	QString filename = "/Settings.ini";

	// first, check the user data
	QString iniFilename = QtUtil::GetAppDataFolder() + filename;

	if (CFileUtil::FileExists(iniFilename.toStdString().c_str()))
	{
		LoadFrom(iniFilename);
		return;
	}

	// check the project
	iniFilename = PathUtil::GetGameFolder() + filename;

	if (CFileUtil::FileExists(iniFilename.toStdString().c_str()))
	{
		LoadFrom(iniFilename);
		return;
	}

	// check the engine root
	iniFilename = PathUtil::GetEnginePath() + "/Editor" + filename;

	if (CFileUtil::FileExists(iniFilename.toStdString().c_str()))
	{
		LoadFrom(iniFilename);
		return;
	}
}

void SEditorSettings::LoadFrom(const QString& filename)
{
	// Deprecating these settings, if settings file is loaded then set the new preferences
	CPreferences* pPreferences = GetIEditorImpl()->GetPreferences();

	QSettings settings(filename, QSettings::IniFormat);
	s_pCurrentQSettings = &settings;

	CryLog("Loading editor preferences from: %s", filename.toLatin1().data());

	int settingsVersion = EditorSettingsVersion;
	LoadValue("Settings", "EditorSettingsVersion", settingsVersion);

	if (settingsVersion != EditorSettingsVersion)
	{
		return;
	}

	string strPlaceholderString;
	auto autoSaveTime = gEditorFilePreferences.autoSaveTime();
	// Load settings from registry.
	LoadValue("Settings", "AutoBackup", gEditorFilePreferences.autoSaveEnabled);
	LoadValue("Settings", "AutoBackupTime", autoSaveTime);
	LoadValue("Settings", "AutoBackupMaxCount", gEditorFilePreferences.autoSaveMaxCount);
	LoadValue("Settings", "CameraMoveSpeed", gViewportMovementPreferences.camMoveSpeed);
	LoadValue("Settings", "CameraRotateSpeed", gViewportMovementPreferences.camRotateSpeed);
	LoadValue("Settings", "WheelZoomSpeed", gViewportMovementPreferences.wheelZoomSpeed);
	LoadValue("Settings", "CameraFastMoveSpeed", gViewportMovementPreferences.camFastMoveSpeed);

	int navigationUpdateType;
	bool bNavigationShowAreas;
	bool bNavigationDebugDisplay;
	bool bVisualizeNavigationAccessibility;
	bool bNavigationRegenDisabledOnLevelLoad;
	int  navigationDebugAgentType;

	LoadValue("Settings/Navigation", "NavigationUpdateType", navigationUpdateType);
	LoadValue("Settings/Navigation", "NavigationShowAreas", bNavigationShowAreas);
	LoadValue("Settings/Navigation", "DisableNavigationRegenOnLevelLoad", bNavigationRegenDisabledOnLevelLoad);
	LoadValue("Settings/Navigation", "NavigationDebugDisplay", bNavigationDebugDisplay);
	LoadValue("Settings/Navigation", "NavigationDebugAgentType", navigationDebugAgentType);
	LoadValue("Settings/Navigation", "VisualizeNavigationAccessibility", bVisualizeNavigationAccessibility);

	gAINavigationPreferences.setNavigationUpdateType((ENavigationUpdateType)navigationUpdateType);
	gAINavigationPreferences.setNavigationDebugDisplay(bNavigationDebugDisplay);
	gAINavigationPreferences.setNavigationShowAreas(bNavigationShowAreas);
	gAINavigationPreferences.setVisualizeNavigationAccessibility(bVisualizeNavigationAccessibility);
	gAINavigationPreferences.setNavigationDebugAgentType(navigationDebugAgentType);
	gAINavigationPreferences.setNavigationRegenDisabledOnLevelLoad(bNavigationRegenDisabledOnLevelLoad);

	bool bApplyConfigSpecInEditor;
	LoadValue("Settings", "BackupOnSave", gEditorFilePreferences.filesBackup);
	LoadValue("Settings", "ApplyConfigSpecInEditor", bApplyConfigSpecInEditor);

	gViewportPreferences.applyConfigSpec = bApplyConfigSpecInEditor;

	LoadValue("Settings", "TemporaryDirectory", gEditorFilePreferences.strStandardTempDirectory);

	bool bShowTimeInConsole;
	LoadValue("Settings", "ShowTimeInConsole", bShowTimeInConsole);
	gEditorGeneralPreferences.setShowTimeInConsole(bShowTimeInConsole);

	//////////////////////////////////////////////////////////////////////////
	// Viewport Settings.
	//////////////////////////////////////////////////////////////////////////
	LoadValue("Settings", "AlwaysShowRadiuses", gViewportPreferences.alwaysShowRadiuses);
	LoadValue("Settings", "AlwaysShowPrefabBounds", gViewportPreferences.alwaysShowPrefabBox);
	LoadValue("Settings", "Sync2DViews", gViewportPreferences.sync2DViews);
	LoadValue("Settings", "DefaultFov", gViewportPreferences.defaultFOV);
	LoadValue("Settings", "AspectRatio", gViewportPreferences.defaultAspectRatio);
	LoadValue("Settings", "ShowSafeFrame", gViewportPreferences.showSafeFrame);
	LoadValue("Settings", "ShowMeshStatsOnMouseOver", gViewportDebugPreferences.showMeshStatsOnMouseOver);
	LoadValue("Settings", "DrawEntityLabels", gViewportPreferences.drawEntityLabels);
	LoadValue("Settings", "ShowTriggerBounds", gViewportPreferences.showTriggerBounds);
	LoadValue("Settings", "ShowIcons", gViewportPreferences.showIcons);
	LoadValue("Settings", "ShowSizeBasedIcons", gViewportPreferences.showSizeBasedIcons);
	LoadValue("Settings", "ShowFrozenHelpers", gViewportPreferences.showFrozenHelpers);
	LoadValue("Settings", "FillSelectedShapes", gViewportPreferences.fillSelectedShapes);
	LoadValue("Settings", "MapTextureResolution", gViewportPreferences.mapViewportResolution);
	LoadValue("Settings", "MapSwapXY", gViewportPreferences.mapViewportSwapXY);
	LoadValue("Settings", "ShowGridGuide", gViewportPreferences.showGridGuide);
	LoadValue("Settings", "HideMouseCursorOnCapture", gViewportPreferences.hideMouseCursorWhenCaptured);
	LoadValue("Settings", "DragSquareSize", gViewportPreferences.dragSquareSize);
	LoadValue("Settings", "EnableContextMenu", gViewportPreferences.enableContextMenu);
	LoadValue("Settings", "WarningIconsDrawDistance", gViewportDebugPreferences.warningIconsDrawDistance);
	LoadValue("Settings", "ShowScaleWarnings", gViewportDebugPreferences.showScaleWarnings);
	LoadValue("Settings", "ShowRotationWarnings", gViewportDebugPreferences.showRotationWarnings);
	LoadValue("Settings", "ScaleIconsWithDistance", gViewportPreferences.distanceScaleIcons);

	//////////////////////////////////////////////////////////////////////////

	LoadValue("Settings", "TextEditorScript", gEditorFilePreferences.textEditorScript);
	LoadValue("Settings", "TextEditorShaders", gEditorFilePreferences.textEditorShaders);
	LoadValue("Settings", "TextEditorBSpaces", gEditorFilePreferences.textEditorBspaces);
	LoadValue("Settings", "TextureEditor", gEditorFilePreferences.textureEditor);
	LoadValue("Settings", "AnimationEditor", gEditorFilePreferences.animEditor);

	bool enableSourceControl;
	bool saveOnlyModified;
	bool freezeReadOnly;

	LoadValue("Settings", "EnableSourceControl", enableSourceControl);
	LoadValue("Settings", "SaveOnlyModified", saveOnlyModified);
	LoadValue("Settings", "FreezeReadOnly", freezeReadOnly);

	gEditorGeneralPreferences.setEnableSourceControl(enableSourceControl);
	gEditorGeneralPreferences.setFreezeReadOnly(freezeReadOnly);
	gEditorGeneralPreferences.setSaveOnlyModified(saveOnlyModified);

	//////////////////////////////////////////////////////////////////////////
	// Snapping Settings.
	bool markerDisplay;
	int markerColor;
	float markerSize;

	LoadValue("Settings/Snap", "SnapMarkerDisplay", markerDisplay);
	LoadValue("Settings/Snap", "SnapMarkerColor", markerColor);
	LoadValue("Settings/Snap", "SnapMarkerSize", markerSize);

	gSnappingPreferences.setMarkerDisplay(markerDisplay);
	gSnappingPreferences.setMarkerColor(markerColor);
	gSnappingPreferences.setMarkerSize(markerSize);

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// HyperGraph
	//////////////////////////////////////////////////////////////////////////
	LoadValue("Settings/HyperGraph", "Opacity", gFlowGraphColorPreferences.opacity);
	LoadValue("Settings/HyperGraph", "ColorArrow", gFlowGraphColorPreferences.colorArrow);
	LoadValue("Settings/HyperGraph", "ColorInArrowHighlighted", gFlowGraphColorPreferences.colorInArrowHighlighted);
	LoadValue("Settings/HyperGraph", "ColorOutArrowHighlighted", gFlowGraphColorPreferences.colorOutArrowHighlighted);
	LoadValue("Settings/HyperGraph", "ColorPortEdgeHighlighted", gFlowGraphColorPreferences.colorPortEdgeHighlighted);
	LoadValue("Settings/HyperGraph", "ColorArrowDisabled", gFlowGraphColorPreferences.colorArrowDisabled);
	LoadValue("Settings/HyperGraph", "ColorNodeOutline", gFlowGraphColorPreferences.colorNodeOutline);
	LoadValue("Settings/HyperGraph", "ColorNodeBkg", gFlowGraphColorPreferences.colorNodeBkg);
	LoadValue("Settings/HyperGraph", "ColorNodeSelected", gFlowGraphColorPreferences.colorNodeSelected);
	LoadValue("Settings/HyperGraph", "ColorTitleText", gFlowGraphColorPreferences.colorTitleText);
	LoadValue("Settings/HyperGraph", "ColorTitleTextSelected", gFlowGraphColorPreferences.colorTitleTextSelected);
	LoadValue("Settings/HyperGraph", "ColorText", gFlowGraphColorPreferences.colorText);
	LoadValue("Settings/HyperGraph", "ColorBackground", gFlowGraphColorPreferences.colorBackground);
	LoadValue("Settings/HyperGraph", "ColorGrid", gFlowGraphColorPreferences.colorGrid);
	LoadValue("Settings/HyperGraph", "BreakPoint", gFlowGraphColorPreferences.colorBreakPoint);
	LoadValue("Settings/HyperGraph", "BreakPointDisabled", gFlowGraphColorPreferences.colorBreakPointDisabled);
	LoadValue("Settings/HyperGraph", "BreakPointArrow", gFlowGraphColorPreferences.colorBreakPointArrow);
	LoadValue("Settings/HyperGraph", "EntityPortNotConnected", gFlowGraphColorPreferences.colorEntityPortNotConnected);
	LoadValue("Settings/HyperGraph", "Port", gFlowGraphColorPreferences.colorPort);
	LoadValue("Settings/HyperGraph", "PortSelected", gFlowGraphColorPreferences.colorPortSelected);
	LoadValue("Settings/HyperGraph", "EntityTextInvalid", gFlowGraphColorPreferences.colorEntityTextInvalid);
	LoadValue("Settings/HyperGraph", "DownArrow", gFlowGraphColorPreferences.colorDownArrow);
	LoadValue("Settings/HyperGraph", "CustomNodeBkg", gFlowGraphColorPreferences.colorCustomNodeBkg);
	LoadValue("Settings/HyperGraph", "CustomSelectedNodeBkg", gFlowGraphColorPreferences.colorCustomSelectedNodeBkg);
	LoadValue("Settings/HyperGraph", "PortDebuggingInitialization", gFlowGraphColorPreferences.colorPortDebuggingInitialization);
	LoadValue("Settings/HyperGraph", "PortDebugging", gFlowGraphColorPreferences.colorPortDebugging);
	LoadValue("Settings/HyperGraph", "PortDebuggingText", gFlowGraphColorPreferences.colorPortDebuggingText);
	LoadValue("Settings/HyperGraph", "QuickSearchBackground", gFlowGraphColorPreferences.colorQuickSearchBackground);
	LoadValue("Settings/HyperGraph", "QuickSearchResultText", gFlowGraphColorPreferences.colorQuickSearchResultText);
	LoadValue("Settings/HyperGraph", "QuickSearchCountText", gFlowGraphColorPreferences.colorQuickSearchCountText);
	LoadValue("Settings/HyperGraph", "QuickSearchBorder", gFlowGraphColorPreferences.colorQuickSearchBorder);
	LoadValue("Settings/HyperGraph", "ColorDebugNodeTitle", gFlowGraphColorPreferences.colorDebugNodeTitle);
	LoadValue("Settings/HyperGraph", "ColorDebugNode", gFlowGraphColorPreferences.colorDebugNode);
	LoadValue("Settings/HyperGraph", "ColorErrorNode", gFlowGraphColorPreferences.colorNodeBckgdError);
	LoadValue("Settings/HyperGraph", "ColorErrorNodeTitle", gFlowGraphColorPreferences.colorNodeTitleError);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// HyperGraph Expert
	//////////////////////////////////////////////////////////////////////////
	LoadValue("Settings/HyperGraph", "EnableMigration", gFlowGraphGeneralPreferences.enableMigration);
	LoadValue("Settings/HyperGraph", "ShowNodeIDs", gFlowGraphGeneralPreferences.showNodeIDs);
	LoadValue("Settings/HyperGraph", "ShowToolTip", gFlowGraphGeneralPreferences.showToolTip);
	LoadValue("Settings/HyperGraph", "EdgesOnTopOfNodes", gFlowGraphGeneralPreferences.edgesOnTopOfNodes);
	LoadValue("Settings/HyperGraph", "SplineEdges", gFlowGraphGeneralPreferences.splineArrows);
	LoadValue("Settings/HyperGraph", "HighlightEdges", gFlowGraphGeneralPreferences.highlightEdges);

	//////////////////////////////////////////////////////////////////////////
	// Experimental features settings
	//////////////////////////////////////////////////////////////////////////
	LoadValue("Settings/ExperimentalFeatures", "TotalIlluminationEnabled", gLightingPreferences.bTotalIlluminationEnabled);

	//////////////////////////////////////////////////////////////////////////
	// Deep Selection Settings
	//////////////////////////////////////////////////////////////////////////
	LoadValue("Settings", "DeepSelectionNearness", gDeepSelectionPreferences.deepSelectionRange);

	//////////////////////////////////////////////////////////////////////////
	// Object Highlight Colors
	//////////////////////////////////////////////////////////////////////////

	auto& colorPrefabBoxRef = gViewportSelectionPreferences.colorPrefabBBox;
	auto& groupHighlightRef = gViewportSelectionPreferences.colorGroupBBox;
	auto& entityHighlightRef = gViewportSelectionPreferences.colorEntityBBox;
	auto& geometryHighlightColorRef = gViewportSelectionPreferences.geometryHighlightColor;
	auto& solidBrushGeometryColorRef = gViewportSelectionPreferences.solidBrushGeometryColor;

	// assign default values in case of serialization failed
	COLORREF prefabHighlight = RGBA8(colorPrefabBoxRef[0], colorPrefabBoxRef[1], colorPrefabBoxRef[2], colorPrefabBoxRef[3]);
	COLORREF groupHighlight = RGBA8(groupHighlightRef[0], groupHighlightRef[1], groupHighlightRef[2], groupHighlightRef[3]);
	COLORREF entityHighlight = RGBA8(entityHighlightRef[0], entityHighlightRef[1], entityHighlightRef[2], entityHighlightRef[3]);
	COLORREF geometryHighlightColor = RGBA8(geometryHighlightColorRef[0], geometryHighlightColorRef[1], geometryHighlightColorRef[2], geometryHighlightColorRef[3]);
	COLORREF solidBrushGeometryColor = RGBA8(solidBrushGeometryColorRef[0], solidBrushGeometryColorRef[1], solidBrushGeometryColorRef[2], solidBrushGeometryColorRef[3]);

	LoadValue("Settings/ObjectColors", "PrefabHighlight", prefabHighlight);
	LoadValue("Settings/ObjectColors", "GroupHighlight", groupHighlight);
	LoadValue("Settings/ObjectColors", "EntityHighlight", entityHighlight);
	LoadValue("Settings/ObjectColors", "BBoxAlpha", gViewportSelectionPreferences.bboxAlpha);
	LoadValue("Settings/ObjectColors", "GeometryHighlightColor", geometryHighlightColor);
	LoadValue("Settings/ObjectColors", "SolidBrushGeometryHighlightColor", solidBrushGeometryColor);
	LoadValue("Settings/ObjectColors", "GeometryAlpha", gViewportSelectionPreferences.geomAlpha);
	LoadValue("Settings/ObjectColors", "ChildGeometryAlpha", gViewportSelectionPreferences.childObjectGeomAlpha);

	gViewportSelectionPreferences.colorPrefabBBox = ColorB(uint32(prefabHighlight), gViewportSelectionPreferences.geomAlpha);
	gViewportSelectionPreferences.colorGroupBBox = ColorB(uint32(groupHighlight), gViewportSelectionPreferences.geomAlpha);
	gViewportSelectionPreferences.colorEntityBBox = ColorB(uint32(entityHighlight), gViewportSelectionPreferences.geomAlpha);
	gViewportSelectionPreferences.geometryHighlightColor = ColorB(uint32(geometryHighlightColor), gViewportSelectionPreferences.geomAlpha);
	gViewportSelectionPreferences.solidBrushGeometryColor = ColorB(uint32(solidBrushGeometryColor), gViewportSelectionPreferences.geomAlpha);

	LoadValue("Settings", "ForceSkyUpdate", gLightingPreferences.bForceSkyUpdate);

	//////////////////////////////////////////////////////////////////////////
	// Vertex snapping settings
	//////////////////////////////////////////////////////////////////////////
	float vertexCubeSize;

	LoadValue("Settings/VertexSnapping", "VertexCubeSize", vertexCubeSize);

	gVertexSnappingPreferences.setVertexCubeSize(vertexCubeSize);

	//////////////////////////////////////////////////////////////////////////
	// Mannequin settings
	//////////////////////////////////////////////////////////////////////////
	LoadValue("Settings/Mannequin", "DefaultPreviewFile", gMannequinPreferences.defaultPreviewFile);
	LoadValue("Settings/Mannequin", "TrackSize", gMannequinPreferences.trackSize);

	LoadValue("Settings/Mannequin", "CtrlForScrubSnapping", gMannequinPreferences.bCtrlForScrubSnapping);
	LoadValue("Settings/Mannequin", "TimelineWheelZoomSpeed", gMannequinPreferences.timelineWheelZoomSpeed);

	s_pCurrentQSettings = 0;
	//////////////////////////////////////////////////////////////////////////

	QDir dir(filename);
	dir.remove(filename);
}

