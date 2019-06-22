// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include <EditorFramework/Events.h>

namespace Private_LevelExplorerCommands
{
void FocusOnActiveLayer()
{
	CommandEvent("level_explorer.focus_on_active_layer").SendToKeyboardFocus();
}

void ShowAllObjects()
{
	CommandEvent("level_explorer.show_all_objects").SendToKeyboardFocus();
}

void ShowLayers()
{
	CommandEvent("level_explorer.show_layers").SendToKeyboardFocus();
}

void ShowFullHierarchy()
{
	CommandEvent("level_explorer.show_full_hierarchy").SendToKeyboardFocus();
}

void ShowActiveLayerContents()
{
	CommandEvent("level_explorer.show_active_layer_contents").SendToKeyboardFocus();
}

}

DECLARE_PYTHON_MODULE(level_explorer);

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelExplorerCommands::FocusOnActiveLayer, level_explorer, focus_on_active_layer,
                                   CCommandDescription("Focus on active layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(level_explorer, focus_on_active_layer, "Focus On Active Layer", "", "icons:level_explorer_focus_active_layer.ico", false)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelExplorerCommands::ShowAllObjects, level_explorer, show_all_objects,
                                   CCommandDescription("Switches Level Explorer view mode to show only objects"))
REGISTER_EDITOR_UI_COMMAND_DESC(level_explorer, show_all_objects, "Show All Objects", "", "icons:level_explorer_objects_only.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelExplorerCommands::ShowLayers, level_explorer, show_layers,
                                   CCommandDescription("Switches Level Explorer view mode to show only layers"))
REGISTER_EDITOR_UI_COMMAND_DESC(level_explorer, show_layers, "Show Layers", "", "icons:level_explorer_layers_only.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelExplorerCommands::ShowFullHierarchy, level_explorer, show_full_hierarchy,
                                   CCommandDescription("Switches Level Explorer view mode to show the full hierarchy"))
REGISTER_EDITOR_UI_COMMAND_DESC(level_explorer, show_full_hierarchy, "Show Full Hierarchy", "", "icons:level_explorer_full_hierarchy.ico", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelExplorerCommands::ShowActiveLayerContents, level_explorer, show_active_layer_contents,
                                   CCommandDescription("Switches Level Explorer view mode to show only the objects in the active layer"))
REGISTER_EDITOR_UI_COMMAND_DESC(level_explorer, show_active_layer_contents, "Show Active Layer Contents", "", "icons:level_explorer_active_layer_content_only.ico", true)
