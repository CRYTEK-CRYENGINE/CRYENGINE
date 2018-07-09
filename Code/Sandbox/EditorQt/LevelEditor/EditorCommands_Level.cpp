// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "Objects/ObjectLayer.h"
#include "Objects/ObjectLayerManager.h"
#include "ICommandManager.h"
#include "EditorFramework/Events.h"

#include "Util/BoostPythonHelpers.h"
#include "CryEdit.h"

namespace Private_EditorCommands
{

bool PyNewLayer()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	return pLayerManager->CreateLayer(eObjectLayerType_Layer) != nullptr;
}

bool PyNewFolder()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	return pLayerManager->CreateLayer(eObjectLayerType_Folder) != nullptr;
}

void PyDeleteLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		pLayerManager->DeleteLayer(pLayer);
	}
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s layer not found", szName);
}

void PyFreezeLayer(const char* szName)
{


	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		CUndo undo("Freeze Layer");
		pLayer->SetFrozen(true);
	}
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s layer not found", szName);

}

void PyUnfreezeLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		CUndo undo("Unfreeze Layer");
		pLayer->SetFrozen(false);
	}
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s layer not found", szName);
}

void PyToggleFreezeAllOtherLayers()
{
	CommandEvent("level.toggle_freeze_all_other_layers").SendToKeyboardFocus();
}

bool PyFreezeReadOnly()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	if (pLayerManager)
	{
		pLayerManager->FreezeROnly();
		return true;
	}
	return false;
}

void PyHideLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		CUndo undo("Hide Layer");
		pLayer->SetVisible(false);
	}
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s layer not found", szName);
}

void PyUnhideLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		CUndo undo("Unhide Layer");
		pLayer->SetVisible(true);
	}
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s layer not found", szName);
}

void PyToggleHideAllOtherLayers()
{
	CommandEvent("level.toggle_hide_all_other_layers").SendToKeyboardFocus();
}

void PyGoToSelection()
{
	CCryEditApp::GetInstance()->OnGotoSelected();
}

bool PyDoesLayerExist(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	return pLayerManager->FindLayerByName(szName) != nullptr;
}

void PyRenameLayer(const char* szNameOld, const char* szNameNew)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szNameOld))
	{
		CUndo undo("Rename Layer");
		pLayer->SetName(szNameNew);
	}
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s layer not found", szNameOld);
}

const char* PyGetNameOfSelectedLayer()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->GetCurrentLayer())
	{
		return pLayer->GetName();
	}
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "There is no active layer");

	return "";
}

void PySelectLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
		pLayerManager->SetCurrentLayer(pLayer);
}

std::vector<std::string> PyGetAllLayerNames()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	std::vector<std::string> result;
	const auto& layers = pLayerManager->GetLayers();
	for (size_t i = 0; i < layers.size(); ++i)
	{
		result.push_back(layers[i]->GetName().GetString());
	}
	return result;
}

void PyFreezeAllLayers()
{
	GetIEditor()->GetObjectManager()->GetLayersManager()->SetAllFrozen(true);
}

void PyUnfreezeAllLayers()
{
	GetIEditor()->GetObjectManager()->GetLayersManager()->SetAllFrozen(false);
}

void PyShowAllLayers()
{
	GetIEditor()->GetObjectManager()->GetLayersManager()->SetAllVisible(true);
}

void PyHideAllLayers()
{
	GetIEditor()->GetObjectManager()->GetLayersManager()->SetAllVisible(false);
}

}

DECLARE_PYTHON_MODULE(level);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyNewLayer, level, new_layer, "Create new layer", "level.new_layer()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyNewFolder, level, new_folder, "Create new folder for layers", "level.new_layer()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyDeleteLayer, level, delete_layer, "Delete layer with specific name.", "level.delete_layer(str layerName)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyFreezeLayer, level, freeze_layer, "Freezes layer with specific name.", "level.freeze_layer(str layerName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyUnfreezeLayer, level, unfreeze_layer, "Unfreezes layer with specific name.", "level.unfreeze_layer(str layerName)");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyFreezeAllLayers, level, freeze_all_layers, CCommandDescription("Freeze all layers"));
REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyUnfreezeAllLayers, level, unfreeze_all_layers, CCommandDescription("Unfreeze all layers"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyToggleFreezeAllOtherLayers, level, toggle_freeze_all_other_layers, CCommandDescription("Toggle freeze all other layers"));

REGISTER_EDITOR_COMMAND_SHORTCUT(level, toggle_freeze_all_other_layers, "Ctrl+Shift+F");
REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyFreezeReadOnly, level, freeze_read_only_layers, CCommandDescription("Freeze read-only layers"));


REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyHideLayer, level, hide_layer, "Hides layer with specific name.", "level.hide_layer(str layerName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyUnhideLayer, level, unhide_layer, "Unhides layer with specific name.", "level.unhide_layer(str layerName)");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyShowAllLayers, level, show_all_layers, CCommandDescription("Show all layers"));
REGISTER_EDITOR_COMMAND_SHORTCUT(level, show_all_layers, "Alt+Shift+H");
REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyHideAllLayers, level, hide_all_layers, CCommandDescription("Hide all layers"));
REGISTER_EDITOR_COMMAND_SHORTCUT(level, hide_all_layers, "Alt+H");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyToggleHideAllOtherLayers, level, toggle_hide_all_other_layers, CCommandDescription("Toggle hide all other layers"));
REGISTER_EDITOR_COMMAND_SHORTCUT(level, toggle_hide_all_other_layers, "Ctrl+Shift+H");


REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyGoToSelection, level, go_to_selection, "Focus camera on selection", "level.go_to_selection()");
REGISTER_EDITOR_COMMAND_SHORTCUT(level, go_to_selection, CKeyboardShortcut("G; Num+."))

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyRenameLayer, level, rename_layer, "Renames layer with specific name.", "level.rename_layer(str layerName, str newLayerName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyDoesLayerExist, level, does_layer_exist, "Checks existence of layer with specific name.", "level.does_layer_exist(str layerName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyGetNameOfSelectedLayer, level, get_name_of_selected_layer, "Returns the name of the selected layer.", "level.get_name_of_selected_layer(str layerName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySelectLayer, level, select_layer, "Selects the layer with the given name.", "level.select_layer(str layerName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyGetAllLayerNames, level, get_names_of_all_layers, "Get a list of all layer names in the level.", "level.get_names_of_all_layers()");

