// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ToolFactory.h"

struct IDataBaseItem;
enum EDataBaseItemEvent;
struct ITransformManipulator;

namespace Designer
{
class DesignerEditor;

enum EPushPull
{
	ePP_Push,
	ePP_Pull,
	ePP_None,
};

enum EPlaneAxis
{
	ePlaneAxis_Average,
	ePlaneAxis_X,
	ePlaneAxis_Y,
	ePlaneAxis_Z
};

enum EPostProcess
{
	ePostProcess_Mesh                 = 1 << 0,
	ePostProcess_Mirror               = 1 << 1,
	ePostProcess_CenterPivot          = 1 << 2,
	ePostProcess_DataBase             = 1 << 3,
	ePostProcess_GameResource         = 1 << 4,
	ePostProcess_SyncPrefab           = 1 << 5,
	ePostProcess_SmoothingGroup       = 1 << 6,

	ePostProcess_All                  = 0xFFFFFFFF,

	ePostProcess_ExceptMesh           = ePostProcess_All & (~ePostProcess_Mesh),
	ePostProcess_ExceptMirror         = ePostProcess_All & (~ePostProcess_Mirror),
	ePostProcess_ExceptCenterPivot    = ePostProcess_All & (~ePostProcess_CenterPivot),
	ePostProcess_ExceptDataBase       = ePostProcess_All & (~ePostProcess_DataBase),
	ePostProcess_ExceptGameResource   = ePostProcess_All & (~ePostProcess_GameResource),
	ePostProcess_ExceptSyncPrefab     = ePostProcess_All & (~ePostProcess_SyncPrefab),
	ePostProcess_ExceptSmoothingGroup = ePostProcess_All & (~ePostProcess_SmoothingGroup)
};

void            ApplyPostProcess(MainContext& mc, int postprocesses = ePostProcess_All);
bool            IsCreationTool(EDesignerTool mode);
bool            IsSelectElementMode(EDesignerTool mode);
bool            IsEdgeSelectMode(EDesignerTool mode);
DesignerEditor* GetDesigner();
void            GetSelectedObjectList(std::vector<MainContext>& selections);
void            SyncPrefab(MainContext& mc);
void            SyncMirror(Model* pModel);
void            RunTool(EDesignerTool tool);
void            UpdateDrawnEdges(MainContext& mc);
void            MessageBox(const QString& title, const QString& msg);
BrushMatrix34   GetOffsetTM(ITransformManipulator* pManipulator, const BrushVec3& vOffset, const BrushMatrix34& worldTM);

enum EDesignerNotify
{
	eDesignerNotify_RefreshSubMaterialList,
	eDesignerNotify_SubMaterialSelectionChanged,
	eDesignerNotify_Select,
	eDesignerNotify_PolygonsModified,
	eDesignerNotify_EnterDesignerEditor,   // a designer type tool has been set in editor
	eDesignerNotify_BeginDesignerSession, // Designer type objects have been selected, we can select tools/set session state
	eDesignerNotify_EndDesignerSession,   // Designer type objects have been deselected, we have to end the session
	eDesignerNotify_ObjectSelect,         // another object was selected
	eDesignerNotify_SelectModeChanged,    // the selection mode was changed
	eDesignerNotify_SubToolChanged,       // the subtool was changed
	eDesignerNotify_SubtoolOptionChanged, // a subtool option was changed
	eDesignerNotify_DatabaseEvent,        // a database event has occurred
	eDesignerNotify_SettingsChanged       // settings have changed
};

struct DatabaseEvent
{
	EDataBaseItemEvent evt;
	IDataBaseItem*     pItem;
};

struct ToolchangeEvent
{
	EDesignerTool previous;
	EDesignerTool current;
};

struct MaterialChangeEvent
{
	int matID;
};

typedef void* TDesignerNotifyParam;

#define DISPLAY_MESSAGE(msg)        \
  if (ar.openBlock("Message", msg)) \
    ar.closeBlock();
}

