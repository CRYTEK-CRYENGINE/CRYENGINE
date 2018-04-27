// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "Objects/ObjectLayer.h"
#include "Objects/ObjectLayerManager.h"
#include "ICommandManager.h"
#include "EditorFramework/Events.h"

#include "Util/BoostPythonHelpers.h"
#include "CryEdit.h"

#include <Dialogs/QNumericBoxDialog.h>

namespace Private_EditorCommands
{
std::vector<std::string> PyGetAllLayers()
{
	CObjectLayerManager* pLayerMgr = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	std::vector<std::string> result;
	const auto& layers = pLayerMgr->GetLayers();
	for (size_t i = 0; i < layers.size(); ++i)
		result.push_back(layers[i]->GetName().GetString());
	return result;
}

bool NewLayer()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	return pLayerManager->CreateLayer(eObjectLayerType_Layer) != nullptr;
}

bool NewFolder()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	return pLayerManager->CreateLayer(eObjectLayerType_Folder) != nullptr;
}

void DeleteLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		pLayerManager->DeleteLayer(pLayer);
	}
}

void MakeLayerUneditable(const char* szName)
{

	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		CUndo undo("Make Layer Uneditable");
		pLayer->SetFrozen(true);
	}
}

void MakeLayerEditable(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		CUndo undo("Make Layer Editable");
		pLayer->SetFrozen(false);
	}
}

bool MakeReadOnlyLayersUneditable()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	if (pLayerManager)
	{
		pLayerManager->FreezeROnly();
		return true;
	}
	return false;
}

void HideLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		CUndo undo("Hide Layer");
		pLayer->SetVisible(false);
	}
}

void ShowLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		CUndo undo("Show Layer");
		pLayer->SetVisible(true);
	}
}

void ToggleHideAllOtherLayers()
{
	CommandEvent("layer.toggle_hide_all_other_layers").SendToKeyboardFocus();
}

bool DoesLayerExist(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	return pLayerManager->FindLayerByName(szName) != nullptr;
}

void RenameLayer(const char* szNameOld, const char* szNameNew)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szNameOld))
	{
		CUndo undo("Rename Layer");
		pLayer->SetName(szNameNew);
	}
}

const char* GetNameOfSelectedLayer()
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

void SelectLayer(const char* szName)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CRY_ASSERT(pLayerManager);

	if (CObjectLayer* pLayer = pLayerManager->FindLayerByName(szName))
	{
		pLayerManager->SetCurrentLayer(pLayer);
	}
}

std::vector<std::string> GetAllLayerNames()
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

void MakeAllLayersUneditable()
{
	GetIEditor()->GetObjectManager()->GetLayersManager()->SetAllFrozen(true);
}

void MakeAllLayersEditable()
{
	GetIEditor()->GetObjectManager()->GetLayersManager()->SetAllFrozen(false);
}

void ShowAllLayers()
{
	GetIEditor()->GetObjectManager()->GetLayersManager()->SetAllVisible(true);
}

void HideAllLayers()
{
	GetIEditor()->GetObjectManager()->GetLayersManager()->SetAllVisible(false);
}
}

DECLARE_PYTHON_MODULE(layer);

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyGetAllLayers, layer, get_all_layers,
                                          "Gets the list of all layer names in the level.",
                                          "layer.get_all_layers()");
REGISTER_COMMAND_REMAPPING(general, get_all_layers, layer, get_all_layers)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::NewLayer, layer, new,
                                   CCommandDescription("Create new layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, new, "New Layer", "", "", false)
REGISTER_COMMAND_REMAPPING(level, new_layer, layer, new)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::NewFolder, layer, new_folder,
                                   CCommandDescription("Create new folder for layers"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, new_folder, "New Folder", "", "", false)
REGISTER_COMMAND_REMAPPING(level, new_folder, layer, new_folder)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::DeleteLayer, layer, delete,
                                   CCommandDescription("Delete layer with specific name").Param("layerName", "Name of layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, delete, "Delete Layer", "", "", false)
REGISTER_COMMAND_REMAPPING(level, delete_layer, layer, delete)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::MakeLayerUneditable, layer, make_uneditable,
                                   CCommandDescription("Makes the layer of a specific name uneditable").Param("layerName", "Name of layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, make_uneditable, "Make Layer Uneditable", "", "", false)
REGISTER_COMMAND_REMAPPING(level, freeze_layer, layer, make_uneditable)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::MakeLayerEditable, layer, make_editable,
                                   CCommandDescription("Makes the layer of a specific name editable").Param("layerName", "Name of layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, make_editable, "Make Layer Editable", "", "", false)
REGISTER_COMMAND_REMAPPING(level, unfreeze_layer, layer, make_editable)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::MakeAllLayersUneditable, layer, make_all_uneditable,
                                   CCommandDescription("Make all layers uneditable"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, make_all_uneditable, "Make All Layers Uneditable", "", "", false)
REGISTER_COMMAND_REMAPPING(level, freeze_all_layers, layer, make_all_uneditable)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::MakeAllLayersEditable, layer, make_all_editable,
                                   CCommandDescription("Make all layers editable"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, make_all_editable, "Make All Layers Editable", "", "", false)
REGISTER_COMMAND_REMAPPING(level, unfreeze_all_layers, layer, make_all_editable)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::MakeReadOnlyLayersUneditable, layer, make_read_only_layers_uneditable,
                                   CCommandDescription("Makes read only layers uneditable"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, make_read_only_layers_uneditable, "Make Read-Only Layers Uneditable", "", "", false)
REGISTER_COMMAND_REMAPPING(level, freeze_read_only_layers, layer, make_read_only_layers_uneditable)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::HideLayer, layer, hide,
                                   CCommandDescription("Hides layer with specific name").Param("layerName", "Name of layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, hide, "Hide Layer", "", "", false)
REGISTER_COMMAND_REMAPPING(level, hide_layer, layer, hide)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ShowLayer, layer, show,
                                   CCommandDescription("Show layer with specific name").Param("layerName", "Name of layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, show, "Show Layer", "", "", false)
REGISTER_COMMAND_REMAPPING(level, unhide_layer, layer, show)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ShowAllLayers, layer, show_all,
                                   CCommandDescription("Show all layers"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, show_all, "Show All Layers", "Alt+Shift+H", "", false)
REGISTER_COMMAND_REMAPPING(level, show_all_layers, layer, show_all)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::HideAllLayers, layer, hide_all,
                                   CCommandDescription("Hide all layers"))
REGISTER_EDITOR_UI_COMMAND_DESC(layer, hide_all, "Hide All Layers", "Alt+H", "", false)
REGISTER_COMMAND_REMAPPING(level, hide_all_layers, layer, hide_all)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::RenameLayer, layer, rename,
                                   CCommandDescription("Renames layer with specific name.").Param("layerName", "Current layer name").Param("newLayerName", "New layer name"))
REGISTER_COMMAND_REMAPPING(level, rename_layer, layer, rename)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::DoesLayerExist, layer, exists, CCommandDescription("Checks existence of layer with specific name").Param("layerName", "Name of layer"))
REGISTER_COMMAND_REMAPPING(level, does_layer_exist, layer, exists)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::GetNameOfSelectedLayer, layer, get_name_of_selected_layer, CCommandDescription("Returns the name of the selected layer"))
REGISTER_COMMAND_REMAPPING(level, get_name_of_selected_layer, layer, get_name_of_selected_layer)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::SelectLayer, layer, select, CCommandDescription("Selects the layer with the given name").Param("layerName", "Name of layer"))
REGISTER_COMMAND_REMAPPING(level, select_layer, layer, select)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::GetAllLayerNames, level, get_names_of_all_layers, "Get a list of all layer names in the level.", "level.get_names_of_all_layers()")
REGISTER_COMMAND_REMAPPING(level, get_names_of_all_layers, layer, get_names_of_all_layers)
