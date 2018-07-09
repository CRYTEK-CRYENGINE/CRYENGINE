// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "TerrainEditor.h"

#include <QTabWidget>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>

#include "Util/BoostPythonHelpers.h"
#include "TerrainDialog.h"
#include "TerrainTexture.h"
#include "MinimapPanel.h"
#include "TerrainTexturePainter.h"
#include "Terrain/TerrainLayerPanel.h"
#include "Terrain/TerrainLayerView.h"
#include "Terrain/TerrainSculptPanel.h"
#include "Commands/QCommandAction.h"
#include "QMfcApp/qwinwidget.h"
#include "CryEdit.h"

extern CCryEditApp theApp;

namespace
{
CTerrainDialog* g_terrainDialog = nullptr;

CTerrainDialog* GetTerrainDialog()
{
	if (!g_terrainDialog)
		g_terrainDialog = new CTerrainDialog();
	return g_terrainDialog;
}

CTerrainTextureDialog* g_terrainLayers = nullptr;

CTerrainTextureDialog* GetTerrainLayers()
{
	if (!g_terrainLayers)
		g_terrainLayers = new CTerrainTextureDialog();
	return g_terrainLayers;
}

// Terrain Functions
void PyImportHeightmap()              { GetTerrainDialog()->OnTerrainLoad(); }
void PyExportHeightmap()              { GetTerrainDialog()->OnExportHeightmap(); }
void PySetOceanHeight()               { GetTerrainDialog()->OnSetWaterLevel(); }
void PyRemoveOcean()                  { GetTerrainDialog()->OnModifyRemovewater(); }
void PyMakeIsle()                     { GetTerrainDialog()->OnModifyMakeisle(); }
void PySetTerrainMaxHeight()          { GetTerrainDialog()->OnSetMaxHeight(); }
void PyFlattenLight()                 { GetTerrainDialog()->OnModifyFlattenLight(); }
void PyFlattenHeavy()                 { GetTerrainDialog()->OnModifyFlattenHeavy(); }
void PySmooth()                       { GetTerrainDialog()->OnModifySmooth(); }
void PySmoothSlope()                  { GetTerrainDialog()->OnModifySmoothSlope(); }
void PySmoothBeachCoast()             { GetTerrainDialog()->OnModifySmoothBeachesOrCoast(); }
void PyNormalize()                    { GetTerrainDialog()->OnModifyNormalize(); }
void PyReduceRangeLight()             { GetTerrainDialog()->OnModifyReduceRangeLight(); }
void PyReduceRangeHeavy()             { GetTerrainDialog()->OnModifyReduceRange(); }
void PyEraseTerrain()                 { GetTerrainDialog()->OnTerrainErase(); }
void PyResizeTerrain()                { GetTerrainDialog()->OnResizeTerrain(); }
void PyInvertHeightmap()              { GetTerrainDialog()->OnTerrainInvert(); }
void PyGenerateTerrain()              { GetTerrainDialog()->OnTerrainGenerate(); }
void PyTerrainTextureDialog()         { GetTerrainDialog()->TerrainTextureExport(); }
void PyImportTerrainBlock()           { GetTerrainDialog()->OnTerrainImportBlock(); }
void PyExportTerrainBlock()           { GetTerrainDialog()->OnTerrainExportBlock(); }
void PyGenerateTerrainTexture()       { GetTerrainDialog()->GenerateTerrainTexture(); }
void PyExportTerrainArea()            { GetTerrainDialog()->OnExportTerrainArea(); }
void PyExportTerrainAreaWithObjects() { GetTerrainDialog()->OnExportTerrainAreaWithObjects(); }
void PyReloadTerrain()                { GetTerrainDialog()->OnReloadTerrain(); }
void PySelectTerrain()                { GetIEditorImpl()->SetEditMode(eEditModeSelectArea); }

// Layer Functions
void PyImportLayers()        { GetTerrainLayers()->OnImport(); }
void PyExportLayers()        { GetTerrainLayers()->OnExport(); }
void PyRefineTiles()         { GetTerrainLayers()->OnRefineTerrainTextureTiles(); }
void PyNewLayer()            { GetTerrainLayers()->OnLayersNewItem(); }
void PyDeleteLayer()         { GetTerrainLayers()->OnLayersDeleteItem(); }
void PyMoveLayerUp()         { GetTerrainLayers()->OnLayersMoveItemUp(); }
void PyMoveLayerDown()       { GetTerrainLayers()->OnLayersMoveItemDown(); }
void PyDuplicateLayer() { GetTerrainLayers()->OnDuplicateItem(); }

void PyFloodLayer()
{
	CEditTool* pTool = GetIEditorImpl()->GetEditTool();
	if (!pTool || !pTool->IsKindOf(RUNTIME_CLASS(CTerrainTexturePainter)))
	{
		pTool = new CTerrainTexturePainter();
		GetIEditorImpl()->SetEditTool(pTool);
	}

	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CTerrainTexturePainter)))
	{
		CTerrainTexturePainter* pPainterTool = (CTerrainTexturePainter*)pTool;
		pPainterTool->Action_StopUndo();
		pPainterTool->Action_Flood();
		pPainterTool->Action_StopUndo();
	}
}
};

REGISTER_VIEWPANE_FACTORY(CTerrainEditor, "Terrain Editor", "Tools", true)

// Terrain commands
DECLARE_PYTHON_MODULE(terrain);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyImportHeightmap, terrain, import_heightmap,
                                     "Creates a terrain from a heightmap.",
                                     "terrain.import_heightmap()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, import_heightmap, "Import Heightmap...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyExportHeightmap, terrain, export_heightmap,
                                     "Exports a heightmap from the current terrain.",
                                     "terrain.export_heightmap()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, export_heightmap, "Export Heightmap...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMakeIsle, terrain, make_isle,
                                     "Makes isle.",
                                     "terrain.make_isle()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, make_isle, "Make Isle");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRemoveOcean, terrain, remove_ocean,
                                     "Removes ocean.",
                                     "terrain.remove_ocean()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, remove_ocean, "Remove Ocean");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetOceanHeight, terrain, set_ocean_height,
                                     "Sets height of the ocean.",
                                     "terrain.set_ocean_height()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, set_ocean_height, "Set Ocean Height");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetTerrainMaxHeight, terrain, set_terrain_max_height,
                                     "Sets maximum terrain height.",
                                     "terrain.set_terrain_max_height()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, set_terrain_max_height, "Set Terrain Max Height");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyFlattenLight, terrain, flatten_light,
                                     "Makes the terrain flat (Light).",
                                     "terrain.flatten_light()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, flatten_light, "Flatten (Light)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyFlattenHeavy, terrain, flatten_heavy,
                                     "Makes the terrain flat (Heavy).",
                                     "terrain.flatten_heavy()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, flatten_heavy, "Flatten (Heavy)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySmooth, terrain, smooth,
                                     "Makes the terrain smooth.",
                                     "terrain.smooth()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, smooth, "Smooth");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySmoothSlope, terrain, smooth_slope,
                                     "Makes the terrain smooth.",
                                     "terrain.smooth_slope()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, smooth_slope, "Smooth Slope");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySmoothBeachCoast, terrain, smooth_beach_coast,
                                     "Makes the terrain smooth.",
                                     "terrain.smooth_beach_coast()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, smooth_beach_coast, "Smooth Beach/Coast");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyNormalize, terrain, normalize,
                                     "Normalizes the terrain.",
                                     "terrain.normalize()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, normalize, "Normalize");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyReduceRangeLight, terrain, reduce_range_light,
                                     "Reduces the range (Light).",
                                     "terrain.reduce_range_light()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, reduce_range_light, "Reduce Range (Light)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyReduceRangeHeavy, terrain, reduce_range_heavy,
                                     "Reduces the range (Heavy).",
                                     "terrain.reduce_range_heavy()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, reduce_range_heavy, "Reduce Range (Heavy)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyEraseTerrain, terrain, erase_terrain,
                                     "Erases the terrain.",
                                     "terrain.erase_terrain()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, erase_terrain, "Erase Terrain");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyResizeTerrain, terrain, resize_terrain,
                                     "Resizes the terrain.",
                                     "terrain.resize_terrain()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, resize_terrain, "Resize Terrain");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyInvertHeightmap, terrain, invert_heightmap,
                                     "Inverts the terrain heightmap.",
                                     "terrain.invert_heightmap()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, invert_heightmap, "Invert Heightmap");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGenerateTerrain, terrain, generate_terrain,
                                     "Generates the terrain.",
                                     "terrain.generate_terrain()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, generate_terrain, "Generate Terrain...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRefineTiles, terrain, refine_tiles,
                                     "Splits the tiles into smaller tiles.",
                                     "terrain.refine_tiles()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, refine_tiles, "Refine Terrain Texture Tiles");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTerrainTextureDialog, terrain, terrain_texture_dialog,
                                     "Shows dialog for terrain texture modifying.",
                                     "terrain.terrain_texture_dialog()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, terrain_texture_dialog, "Export/Import Terrain Texture...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyReloadTerrain, terrain, reload_terrain,
                                     "Reloads the existing terrain.",
                                     "terrain.reload_terrain()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, reload_terrain, "Reload Terrain");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyImportTerrainBlock, terrain, import_block,
                                     "Imports a block of terrain.",
                                     "terrain.import_block()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, import_block, "Import Terrain Block");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyExportTerrainBlock, terrain, export_block,
                                     "Exports a block of terrain.",
                                     "terrain.export_block()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, export_block, "Export Terrain Block");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGenerateTerrainTexture, terrain, generate_terrain_texture,
                                     "Generates a texture for the terrain.",
                                     "terrain.generate_terrain_texture()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, generate_terrain_texture, "Generate Terrain Texture...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyExportTerrainArea, terrain, export_area,
                                     "Exports a terrain area.",
                                     "terrain.export_area()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, export_area, "Export Terrain Area...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyExportTerrainAreaWithObjects, terrain, export_area_with_objects,
                                     "Exports a terrain area with objects.",
                                     "terrain.export_area_with_objects()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, export_area_with_objects, "Export Terrain Area With Objects...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySelectTerrain, terrain, select_terrain,
                                     "Tool for selecting terrain area.",
                                     "terrain.select_terrain()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, select_terrain, "Select Terrain");

// Layers commands
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyImportLayers, terrain, import_layers,
                                     "Imports terrain layers.",
                                     "terrain.import_layers()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, import_layers, "Import Layers...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyExportLayers, terrain, export_layers,
                                     "Exports the layers from the current terrain.",
                                     "terrain.export_layers()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, export_layers, "Export Layers...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyNewLayer, terrain, create_layer,
                                     "Creates a new layer.",
                                     "terrain.create_layer()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, create_layer, "Create Layer");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyDeleteLayer, terrain, delete_layer,
                                     "Deletes the selected layer.",
                                     "terrain.delete_layer()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, delete_layer, "Delete Layer");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyDuplicateLayer, terrain, duplicate_layer,
                                     "Duplicates the selected layer.",
                                     "terrain.duplicate_layer()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, duplicate_layer, "Duplicate Layer");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMoveLayerUp, terrain, move_layer_up,
                                     "Moves the selected layer by one slot up.",
                                     "terrain.move_layer_up()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, move_layer_up, "Move Layer Up");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMoveLayerDown, terrain, move_layer_down,
                                     "Moves the selected layer by one slot down.",
                                     "terrain.move_layer_down()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, move_layer_down, "Move Layer Down");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyFloodLayer, terrain, flood_layer,
                                     "Floods the selected layer over the all terrain.",
                                     "terrain.flood_layer()");
REGISTER_EDITOR_COMMAND_TEXT(terrain, flood_layer, "Flood Layer");

CTerrainEditor::CTerrainEditor(QWidget* parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	QTabWidget* p_Tabs = new QTabWidget(this);
	SetContent(p_Tabs);
	p_Tabs->setTabsClosable(false);

	p_Tabs->addTab(new QTerrainSculptPanel(), "Sculpt");
	p_Tabs->addTab(new QTerrainLayerPanel(), "Paint");
	p_Tabs->addTab(new QMinimapPanel(), "Mini Map");

	InitTerrainMenu();
	InstallReleaseMouseFilter(this);
}

CTerrainEditor::~CTerrainEditor()
{
}

void CTerrainEditor::SetLayout(const QVariantMap& state)
{
	CEditor::SetLayout(state);
	QTerrainLayerView* pLayerView = findChild<QTerrainLayerView*>();
	
	QVariant layerStateVar = state.value("layerView");
	if (layerStateVar.isValid() && layerStateVar.type() == QVariant::Map)
	{
		pLayerView->SetState(layerStateVar.value<QVariantMap>());
	}

}

QVariantMap CTerrainEditor::GetLayout() const
{
	QVariantMap state = CEditor::GetLayout();
	QTerrainLayerView* pLayerView = findChild<QTerrainLayerView*>();
	if (pLayerView)
	{
		state.insert("layerView", pLayerView->GetState());
	}
	return state;
}

CTerrainTextureDialog* CTerrainEditor::GetTextureLayerEditor()
{
	return GetTerrainLayers();
}

void CTerrainEditor::InitTerrainMenu()
{
	const CEditor::MenuItems items[] = {
		CEditor::MenuItems::FileMenu,
		CEditor::MenuItems::EditMenu
	};
	AddToMenu(&items[0], 2);

	CAbstractMenu* const pFileMenu = GetMenu(MenuItems::FileMenu);
	CRY_ASSERT(pFileMenu);
	if (pFileMenu)
	{
		int sec;
		
		sec = pFileMenu->GetNextEmptySection();
		pFileMenu->AddAction(GetAction("terrain.generate_terrain_texture"), sec);
		pFileMenu->AddAction(GetAction("terrain.generate_terrain"), sec);

		sec = pFileMenu->GetNextEmptySection();
		pFileMenu->AddAction(GetAction("terrain.import_heightmap"), sec);
		pFileMenu->AddAction(GetAction("terrain.export_heightmap"), sec);
		pFileMenu->AddAction(GetAction("terrain.import_block"), sec);
		pFileMenu->AddAction(GetAction("terrain.export_block"), sec);
		pFileMenu->AddAction(GetAction("terrain.export_area"), sec);
		pFileMenu->AddAction(GetAction("terrain.export_area_with_objects"), sec);
		pFileMenu->AddAction(GetAction("terrain.terrain_texture_dialog"), sec);

		sec = pFileMenu->GetNextEmptySection();
		pFileMenu->AddAction(GetAction("terrain.import_layers"), sec);
		pFileMenu->AddAction(GetAction("terrain.export_layers"), sec);
	}

	CAbstractMenu* const pEditMenu = GetMenu(MenuItems::EditMenu);
	CRY_ASSERT(pEditMenu);
	if (pEditMenu)
	{
		int sec = 0;

		sec = pEditMenu->GetNextEmptySection();
		pEditMenu->AddAction(GetAction("terrain.select_terrain"), sec);

		sec = pEditMenu->GetNextEmptySection();
		pEditMenu->AddAction(GetAction("terrain.make_isle"), sec);
		pEditMenu->AddAction(GetAction("terrain.remove_ocean"), sec);
		pEditMenu->AddAction(GetAction("terrain.set_ocean_height"), sec);

		sec = pEditMenu->GetNextEmptySection();
		pEditMenu->AddAction(GetAction("terrain.set_terrain_max_height"), sec);

		sec = pEditMenu->GetNextEmptySection();
		pEditMenu->AddAction(GetAction("terrain.flatten_light"), sec);
		pEditMenu->AddAction(GetAction("terrain.flatten_heavy"), sec);

		sec = pEditMenu->GetNextEmptySection();
		pEditMenu->AddAction(GetAction("terrain.smooth"), sec);
		pEditMenu->AddAction(GetAction("terrain.smooth_slope"), sec);
		pEditMenu->AddAction(GetAction("terrain.smooth_beach_coast"), sec);

		sec = pEditMenu->GetNextEmptySection();
		pEditMenu->AddAction(GetAction("terrain.normalize"), sec);
		pEditMenu->AddAction(GetAction("terrain.reduce_range_light"), sec);
		pEditMenu->AddAction(GetAction("terrain.reduce_range_heavy"), sec);

		sec = pEditMenu->GetNextEmptySection();
		pEditMenu->AddAction(GetAction("terrain.erase_terrain"), sec);
		pEditMenu->AddAction(GetAction("terrain.resize_terrain"), sec);
		pEditMenu->AddAction(GetAction("terrain.invert_heightmap"), sec);
		pEditMenu->AddAction(GetAction("terrain.reload_terrain"), sec);

		sec = pEditMenu->GetNextEmptySection();
		pEditMenu->AddAction(GetAction("terrain.refine_tiles"), sec);
	}

	CAbstractMenu* pLayerMenu = GetMenu(tr("Layers"));
	pLayerMenu->AddAction(GetAction("terrain.create_layer"));
	pLayerMenu->AddAction(GetAction("terrain.delete_layer"));
	pLayerMenu->AddAction(GetAction("terrain.duplicate_layer"));
	pLayerMenu->AddAction(GetAction("terrain.move_layer_up"));
	pLayerMenu->AddAction(GetAction("terrain.move_layer_down"));

	int sec = pLayerMenu->GetNextEmptySection();
	pLayerMenu->AddAction(GetAction("terrain.flood_layer"), sec);
}

