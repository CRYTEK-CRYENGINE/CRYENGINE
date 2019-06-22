// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"

// Sandbox
#include "CryEditDoc.h"
#include "Geometry/EdMesh.h"
#include "IEditorImpl.h"
#include "LogFile.h"
#include "Material/MaterialManager.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/ObjectManager.h"
#include "QT/QtMainFrame.h"
#include "Terrain/Heightmap.h"
#include "Terrain/TerrainManager.h"
#include "Vegetation/VegetationMap.h"
#include "ViewManager.h"

// EditorCommon
#include <Commands/ICommandManager.h>
#include <IUndoObject.h>
#include <EditorFramework/Events.h>
#include <LevelEditor/LevelEditorSharedState.h>
#include <ModelViewport.h>
#include <Qt/Widgets/QWaitProgress.h>
#include <UsedResources.h>

// CryCommon
#include <CryGame/IGameFramework.h>
#include <CrySystem/ICryLink.h>
#include <IItemSystem.h>

namespace
{
namespace Private_LevelCommands
{

void PySnapToGrid(const bool bEnable)
{
	char buffer[31];
	cry_sprintf(buffer, "level.snap_to_grid %i", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToGrid()
{
	CommandEvent("level.toggle_snap_to_grid").SendToKeyboardFocus();
}

void PySnapToAngle(const bool bEnable)
{
	char buffer[32];
	cry_sprintf(buffer, "level.snap_to_angle %i", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToAngle()
{
	CommandEvent("level.toggle_snap_to_angle").SendToKeyboardFocus();
}

void PySnapToScale(const bool bEnable)
{
	char buffer[32];
	cry_sprintf(buffer, "level.snap_to_scale %i", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToScale()
{
	CommandEvent("level.toggle_snap_to_scale").SendToKeyboardFocus();
}

void PySnapToVertex(const bool bEnable)
{
	char buffer[33];
	cry_sprintf(buffer, "level.snap_to_vertex %i", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToVertex()
{
	CommandEvent("level.toggle_snap_to_vertex").SendToKeyboardFocus();
}

void PySnapToPivot(const bool bEnable)
{
	char buffer[32];
	cry_sprintf(buffer, "level.snap_to_pivot %i", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToPivot()
{
	CommandEvent("level.toggle_snap_to_pivot").SendToKeyboardFocus();
}

void PySnapToTerrain(const bool bEnable)
{
	char buffer[34];
	cry_sprintf(buffer, "level.snap_to_terrain %i", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToTerrain()
{
	CommandEvent("level.toggle_snap_to_terrain").SendToKeyboardFocus();
}

void PySnapToGeometry(const bool bEnable)
{
	char buffer[35];
	cry_sprintf(buffer, "level.snap_to_geometry %i", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToGeometry()
{
	CommandEvent("level.toggle_snap_to_geometry").SendToKeyboardFocus();
}

void PySnapToSurfaceNormal(const bool bEnable)
{
	char buffer[41];
	cry_sprintf(buffer, "level.snap_to_surface_normal %i", bEnable);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void PyToggleSnapToSurfaceNormal()
{
	CommandEvent("level.toggle_snap_to_surface_normal").SendToKeyboardFocus();
}

void PyToggleDisplayInfo()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->ToggleDisplayInfo();
}

void PyDisplayInfoLow()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetDisplayInfoLevel(CLevelEditorSharedState::eDisplayInfoLevel_Low);
}

void PyDisplayInfoMedium()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetDisplayInfoLevel(CLevelEditorSharedState::eDisplayInfoLevel_Med);
}

void PyDisplayInfoHigh()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetDisplayInfoLevel(CLevelEditorSharedState::eDisplayInfoLevel_High);
}

void ReloadAllScripts()
{
	GetIEditorImpl()->GetICommandManager()->Execute("entity.reload_all_scripts");
	gEnv->pConsole->ExecuteString("i_reload");
	GetIEditorImpl()->GetICommandManager()->Execute("ai.reload_all_scripts");
	GetIEditorImpl()->GetICommandManager()->Execute("ui.reload_all_scripts");
}

void ReloadTexturesAndShaders()
{
	CWaitCursor wait;
	CLogFile::WriteLine("Reloading Static objects textures and shaders.");
	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_RELOAD_TEXTURES);
	GetIEditorImpl()->GetRenderer()->EF_ReloadTextures();
}

void ReloadGeometry()
{
	if (!GetIEditor()->GetDocument()->IsDocumentReady())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Please load a level before reloading all geometry");
		return;
	}

	CWaitProgress wait("Reloading static geometry");

	CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	if (pVegetationMap)
		pVegetationMap->ReloadGeometry();

	CLogFile::WriteLine("Reloading Static objects geometries.");
	CEdMesh::ReloadAllGeometries();

	// Reload CHRs
	GetIEditorImpl()->GetSystem()->GetIAnimationSystem()->ReloadAllModels();

	//GetIEditorImpl()->Get3DEngine()->UnlockCGFResources();

	if (gEnv->pGameFramework->GetIItemSystem() != NULL)
	{
		gEnv->pGameFramework->GetIItemSystem()->ClearGeometryCache();
	}
	//GetIEditorImpl()->Get3DEngine()->LockCGFResources();
	// Force entity system to collect garbage.
	GetIEditorImpl()->GetSystem()->GetIEntitySystem()->Update();
	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_RELOAD_GEOM);

	// Rephysicalize viewport meshes
	for (int i = 0; i < GetIEditorImpl()->GetViewManager()->GetViewCount(); ++i)
	{
		CViewport* vp = GetIEditorImpl()->GetViewManager()->GetView(i);
		if (vp->GetType() == ET_ViewportModel)
		{
			((CModelViewport*)vp)->RePhysicalize();
		}
	}

	GetIEditorImpl()->GetICommandManager()->Execute("entity.reload_all_scripts");
	IRenderNode** plist = new IRenderNode*[max(gEnv->p3DEngine->GetObjectsByType(eERType_Vegetation, 0), gEnv->p3DEngine->GetObjectsByType(eERType_Brush, 0))];
	for (int i = 0; i < 2; i++)
		for (int j = gEnv->p3DEngine->GetObjectsByType(i ? eERType_Brush : eERType_Vegetation, plist) - 1; j >= 0; j--)
			plist[j]->Physicalize(true);
	delete[] plist;
}

void ValidateLevel()
{
	//TODO : There are two different code paths to validate objects, most likely this should be moved somewhere else or unified at least.
	CWaitCursor cursor;

	// Validate all objects
	CBaseObjectsArray objects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objects);

	int i;

	CLogFile::WriteLine("Validating Objects...");
	for (i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];

		pObject->Validate();

		CUsedResources rs;
		pObject->GatherUsedResources(rs);
		rs.Validate();
	}

	CLogFile::WriteLine("Validating Duplicate Objects...");
	//////////////////////////////////////////////////////////////////////////
	// Find duplicate objects, Same objects with same transform.
	// Use simple grid to speed up the check.
	//////////////////////////////////////////////////////////////////////////
	int gridSize = 256;

	SSectorInfo si;
	GetIEditorImpl()->GetHeightmap()->GetSectorsInfo(si);
	float worldSize = si.numSectors * si.sectorSize;
	float fGridToWorld = worldSize / gridSize;

	// Put all objects into partition grid.
	std::vector<std::list<CBaseObject*>> grid;
	grid.resize(gridSize * gridSize);
	// Put objects to grid.
	for (i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		Vec3 pos = pObject->GetWorldPos();
		int px = int(pos.x / fGridToWorld);
		int py = int(pos.y / fGridToWorld);
		if (px < 0) px = 0;
		if (py < 0) py = 0;
		if (px >= gridSize) px = gridSize - 1;
		if (py >= gridSize) py = gridSize - 1;
		grid[py * gridSize + px].push_back(pObject);
	}

	std::list<CBaseObject*>::iterator it1, it2;
	// Check objects in grid.
	for (i = 0; i < gridSize * gridSize; i++)
	{
		std::list<CBaseObject*>::iterator first = grid[i].begin();
		std::list<CBaseObject*>::iterator last = grid[i].end();
		for (it1 = first; it1 != last; ++it1)
		{
			for (it2 = first; it2 != it1; ++it2)
			{
				// Check if same object.
				CBaseObject* p1 = *it1;
				CBaseObject* p2 = *it2;
				if (p1 != p2 && p1->GetClassDesc() == p2->GetClassDesc())
				{
					// Same class.
					if (p1->GetWorldPos() == p2->GetWorldPos() && p1->GetRotation() == p2->GetRotation() && p1->GetScale() == p2->GetScale())
					{
						// Same transformation
						// Check if objects are really same.
						if (p1->IsSimilarObject(p2))
						{
							const Vec3 pos = p1->GetWorldPos();
							// Report duplicate objects.
							CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Found multiple objects in the same location (class %s): %s and %s are located at (%.2f, %.2f, %.2f) %s",
							           p1->GetClassDesc()->ClassName(), (const char*)p1->GetName(), (const char*)p2->GetName(), pos.x, pos.y, pos.z,
							           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "selection.select_and_go_to %s", p1->GetName()));
						}
					}
				}
			}
		}
	}

	//Validate materials
	GetIEditorImpl()->GetMaterialManager()->Validate();
}

void SaveLevelStats()
{
	gEnv->pConsole->ExecuteString("SaveLevelStats");
}

void UpdateProceduralVegetation()
{
	GetIEditorImpl()->GetTerrainManager()->ReloadSurfaceTypes();

	CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	if (pVegetationMap)
		pVegetationMap->SetEngineObjectsParams();

	GetIEditorImpl()->SetModifiedFlag();
}

void SetCoordSys(int value)
{
	GetIEditor()->GetLevelEditorSharedState()->SetCoordSystem((CLevelEditorSharedState::CoordSystem)value);
}

void SetViewCoords()
{
	GetIEditor()->GetLevelEditorSharedState()->SetCoordSystem(CLevelEditorSharedState::CoordSystem::View);
}

void SetLocalCoords()
{
	GetIEditor()->GetLevelEditorSharedState()->SetCoordSystem(CLevelEditorSharedState::CoordSystem::Local);
}

void SetParentCoords()
{
	GetIEditor()->GetLevelEditorSharedState()->SetCoordSystem(CLevelEditorSharedState::CoordSystem::Parent);
}

void SetWorldCoords()
{
	GetIEditor()->GetLevelEditorSharedState()->SetCoordSystem(CLevelEditorSharedState::CoordSystem::World);
}

void SetCustomCoords()
{
	GetIEditor()->GetLevelEditorSharedState()->SetCoordSystem(CLevelEditorSharedState::CoordSystem::UserDefined);
}
}
}

DECLARE_PYTHON_MODULE(level);

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToGrid, level, snap_to_grid,
                                   CCommandDescription("Enable/Disable snapping to grid").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToGrid, level, toggle_snap_to_grid,
                                   CCommandDescription("Toggle snapping to grid"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_grid, "", "I", "icons:Viewport/viewport-snap-grid.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToAngle, level, snap_to_angle,
                                   CCommandDescription("Enable/Disable snapping to angle").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToAngle, level, toggle_snap_to_angle,
                                   CCommandDescription("Toggle snapping to angle"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_angle, "", "L", "icons:Viewport/viewport-snap-angle.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToScale, level, snap_to_scale,
	CCommandDescription("Enable/Disable snapping to scale").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToScale, level, toggle_snap_to_scale,
                                   CCommandDescription("Toggle snapping to scale"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_scale, "", "K", "icons:Viewport/viewport-snap-scale.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToVertex, level, snap_to_vertex,
                                   CCommandDescription("Enable/Disable snapping to vertex").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToVertex, level, toggle_snap_to_vertex,
                                   CCommandDescription("Toggle snapping to vertex"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_vertex, "", "V", "icons:Viewport/viewport-snap-vertex.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToPivot, level, snap_to_pivot,
                                   CCommandDescription("Enable/Disable snapping to privot").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToPivot, level, toggle_snap_to_pivot,
                                   CCommandDescription("Toggle snapping to pivot"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_pivot, "", "P", "icons:Viewport/viewport-snap-pivot.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionAlign_To_Object, level, toggle_snap_to_pivot)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToTerrain, level, snap_to_terrain,
                                   CCommandDescription("Enable/Disable snapping to terrain").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToTerrain, level, toggle_snap_to_terrain,
                                   CCommandDescription("Toggle snapping to terrain"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_terrain, "", "T", "icons:common/viewport-snap-terrain.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToGeometry, level, snap_to_geometry,
                                   CCommandDescription("Enable/Disable snapping to geometry").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToGeometry, level, toggle_snap_to_geometry,
                                   CCommandDescription("Toggle snapping to geometry"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_geometry, "", "O", "icons:common/viewport-snap-geometry.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PySnapToSurfaceNormal, level, snap_to_surface_normal,
                                   CCommandDescription("Enable/Disable snapping to surface normal").Param("enable", "0: Disable, 1: Enable"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleSnapToSurfaceNormal, level, toggle_snap_to_surface_normal,
                                   CCommandDescription("Toggle snapping to surface normal"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_snap_to_surface_normal, "", "N", "icons:common/viewport-snap-normal.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyToggleDisplayInfo, level, toggle_display_info,
                                   CCommandDescription("Toggle display info in the level editor viewport"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, toggle_display_info, "Display Info", "", "icons:Viewport/viewport-displayinfo.ico", true)
REGISTER_COMMAND_REMAPPING(general, cycle_displayinfo, level, toggle_display_info)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyDisplayInfoLow, level, display_info_low, CCommandDescription("Verbosity level low"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, display_info_low, "Verbosity Level Low", "", "", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyDisplayInfoMedium, level, display_info_medium, CCommandDescription("Verbosity level medium"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, display_info_medium, "Verbosity Level Medium", "", "", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::PyDisplayInfoHigh, level, display_info_high, CCommandDescription("Verbosity level high"));
REGISTER_EDITOR_UI_COMMAND_DESC(level, display_info_high, "Verbosity Level High", "", "", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::ReloadAllScripts, level, reload_all_scripts, CCommandDescription("Reloads all scripts"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, reload_all_scripts, "Reload All Scripts", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionReload_All_Scripts, level, reload_all_scripts)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::ReloadTexturesAndShaders, level, reload_textures_shaders, CCommandDescription("Reloads all textures and shaders"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, reload_textures_shaders, "Reload Texture/Shaders", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionReload_Texture_Shaders, level, reload_textures_shaders)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::ReloadGeometry, level, reload_geometry, CCommandDescription("Reloads all geometry"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, reload_geometry, "Reload Geometry", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionReload_Geometry, level, reload_geometry)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::ValidateLevel, level, validate,
                                   CCommandDescription("Validates all objects in the level and makes sure there aren't duplicate objects of the same type with the same transform"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, validate, "Check Level for Errors", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionCheck_Level_for_Errors, level, validate)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::SaveLevelStats, level, save_stats, CCommandDescription("Saves level statistics"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, save_stats, "Save Level Statistics", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionSave_Level_Statistics, level, save_stats)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::UpdateProceduralVegetation, level, update_procedural_vegetation,
                                   CCommandDescription("Updates procedural vegetation"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, update_procedural_vegetation, "Update Procedural Vegetation", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionUpdate_Procedural_Vegetation, level, update_procedural_vegetation)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::SetCoordSys, level, set_coordinate_system,
                                   CCommandDescription("Sets the coordinate system to use for level editor"))
REGISTER_COMMAND_REMAPPING(general, set_coordinate_system, level, set_coordinate_system)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::SetViewCoords, level, set_view_coordinate_system,
                                   CCommandDescription("Use view coordinate system"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, set_view_coordinate_system, "View", "", "icons:Navigation/Coordinates_View.ico", true)
REGISTER_COMMAND_REMAPPING(general, set_view_coordinates, level, set_view_coordinate_system)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::SetLocalCoords, level, set_local_coordinate_system,
                                   CCommandDescription("Use local coordinate system"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, set_local_coordinate_system, "Local", "", "icons:Navigation/Coordinates_Local.ico", true)
REGISTER_COMMAND_REMAPPING(general, set_local_coordinates, level, set_local_coordinate_system)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::SetParentCoords, level, set_parent_coordinate_system,
                                   CCommandDescription("Use parent coordinate system"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, set_parent_coordinate_system, "Parent", "", "icons:Navigation/Coordinates_Parent.ico", true)
REGISTER_COMMAND_REMAPPING(general, set_parent_coordinates, level, set_parent_coordinate_system)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::SetWorldCoords, level, set_world_coordinate_system,
                                   CCommandDescription("Use world coordinate system"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, set_world_coordinate_system, "World", "", "icons:Navigation/Coordinates_World.ico", true)
REGISTER_COMMAND_REMAPPING(general, set_world_coordinates, level, set_world_coordinate_system)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelCommands::SetCustomCoords, level, set_custom_coordinate_system,
                                   CCommandDescription("Use custom coordinate system"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, set_custom_coordinate_system, "Custom", "", "", true)
REGISTER_COMMAND_REMAPPING(general, set_custom_coordinate_system, level, set_custom_coordinate_system)
