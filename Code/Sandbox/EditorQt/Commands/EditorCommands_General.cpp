// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "IEditorImpl.h"
#include "IUndoManager.h"
#include "Util/BoostPythonHelpers.h"

#include "EditorFramework/Events.h"

#include "Qt/QToolTabManager.h"

//////////////////////////////////////////////////////////////////////////
// Editor Framework Commands
// TODO : These are handled in EditorCommon, for instance EditorFramework/Editor.cpp. Move these to more appropriate place
//////////////////////////////////////////////////////////////////////////

namespace Private_EditorCommands
{
//Undo/Redo is not yet handled per editor, it still calls a global shortcut
void PyUndo()
{
	GetIEditorImpl()->GetIUndoManager()->Undo();
}

void PyRedo()
{
	GetIEditorImpl()->GetIUndoManager()->Redo();
}

void PyFocusOnLevelEditor()
{
	auto mng = CTabPaneManager::GetInstance();
	if (mng)
	{
		auto pane = mng->FindPaneByTitle("Perspective");
		mng->BringToFront(pane);
	}
}

void PyOpenOrFocusViewPane(const char* viewClassName)
{
	auto mng = CTabPaneManager::GetInstance();
	if (mng)
	{
		mng->OpenOrCreatePane(viewClassName);
	}
}

void OpenEditorMenu()
{
	CommandEvent("general.open_editor_menu").SendToKeyboardFocus();
}

}

DECLARE_PYTHON_MODULE(general);

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, new, CCommandDescription("Context driven 'new' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, new, "&New...", CKeyboardShortcut::StandardKey::New, "icons:General/File_New.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, new_folder, CCommandDescription("Context driven 'new folder' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, new_folder, "New Folder", "", "icons:General/Folder_Add.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, open, CCommandDescription("Context driven 'open' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, open, "&Open...", CKeyboardShortcut::StandardKey::Open, "icons:General/Folder_Open.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, import, CCommandDescription("Context driven 'import' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, import, "Import...", "", "icons:General/File_Import.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, export, CCommandDescription("Context driven 'export' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, export, "Export...", "", "icons:General/File_Export.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, reimport, CCommandDescription("Context driven 're-import' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, reimport, "Reimport", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, reload, CCommandDescription("Context driven 'reload' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, reload, "Reload", "", "icons:General/Reload.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, refresh, CCommandDescription("Context driven 'refresh' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, refresh, "Refresh", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, close, CCommandDescription("Context driven 'close' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, close, "Close", CKeyboardShortcut::StandardKey::Close, "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, save, CCommandDescription("Context driven 'save' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, save, "&Save", CKeyboardShortcut::StandardKey::Save, "icons:General/File_Save.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, save_as, CCommandDescription("Context driven 'save as' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, save_as, "Save As...", CKeyboardShortcut::StandardKey::SaveAs, "icons:General/Save_as.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, rename, CCommandDescription("Context driven 'rename' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, rename, "Rename", "F2", "icons:General/editable_true.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, cut, CCommandDescription("Context driven 'cut' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, cut, "Cut", CKeyboardShortcut::StandardKey::Cut, "icons:General/Cut.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, copy, CCommandDescription("Context driven 'copy' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, copy, "Copy", CKeyboardShortcut::StandardKey::Copy, "icons:General/Copy.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, paste, CCommandDescription("Context driven 'paste' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, paste, "Paste", CKeyboardShortcut::StandardKey::Paste, "icons:General/Paste.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, delete, CCommandDescription("Context driven 'delete' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, delete, "Delete", CKeyboardShortcut::StandardKey::Delete, "icons:General/Delete.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, clear, CCommandDescription("Context driven 'clear' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, clear, "Clear", "", "icons:General/Element_Clear.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, duplicate, CCommandDescription("Context driven 'duplicate' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, duplicate, "Duplicate", CKeyboardShortcut("Ctrl+D"), "icons:General/Duplicate.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, find, CCommandDescription("Context driven 'find' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, find, "Find", CKeyboardShortcut::StandardKey::Find, "icons:General/Search.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, find_previous, CCommandDescription("Context driven 'find previous' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, find_previous, "Find Previous", CKeyboardShortcut::StandardKey::FindPrevious, "icons:General/Arrow_Left.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, find_next, CCommandDescription("Context driven 'find next' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, find_next, "Find Next", CKeyboardShortcut::StandardKey::FindNext, "icons:General/Arrow_Right.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, select_all, CCommandDescription("Context driven 'select all' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, select_all, "Select All", CKeyboardShortcut::StandardKey::SelectAll, "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, help, CCommandDescription("Go to documentation..."))
REGISTER_EDITOR_UI_COMMAND_DESC(general, help, "", CKeyboardShortcut::StandardKey::HelpWiki, "icons:Tools/Go_To_Documentation.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, expand_all, CCommandDescription("Context driven 'expand all' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, expand_all, "Expand All", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, collapse_all, CCommandDescription("Context driven 'collapse all' command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, collapse_all, "Collapse All", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, zoom_in, CCommandDescription("Context driven 'zoom in' command"));
REGISTER_EDITOR_UI_COMMAND_DESC(general, zoom_in, "Zoom In", CKeyboardShortcut::StandardKey::ZoomIn, "icons:General/Zoom_In.ico", false);

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, zoom_out, CCommandDescription("Context driven 'zoom out' command"));
REGISTER_EDITOR_UI_COMMAND_DESC(general, zoom_out, "Zoom Out", CKeyboardShortcut::StandardKey::ZoomOut, "icons:General/Zoom_Out.ico", false);

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, lock, CCommandDescription("Context driven lock command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, lock, "", "", "icons:general_lock_true.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, unlock, CCommandDescription("Context driven unlock command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, unlock, "", "", "icons:general_lock_false.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, toggle_lock,
                                                  CCommandDescription("Context driven toggling lock command. If in a multi selection with multiple state, items will be unlocked"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, toggle_lock, "Locked", "", "icons:general_lock_true.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, isolate_locked, CCommandDescription("Isolate editability of objects or layers"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, isolate_locked, "Isolate Locked", "Ctrl+Shift+F", "", true)
REGISTER_COMMAND_REMAPPING(level, toggle_freeze_all_other_layers, general, isolate_locked)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, lock_all, CCommandDescription("Context driven lock all command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, lock_all, "Lock All", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, unlock_all, CCommandDescription("Context driven unlock all command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, unlock_all, "Unlock All", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, lock_children,
                                                  CCommandDescription("Makes the children of the selected item(s) locked"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, lock_children, "Lock Children", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, unlock_children,
                                                  CCommandDescription("Makes the children of the selected item(s) unlocked"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, unlock_children, "Unlock Children", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, toggle_children_locking,
                                                  CCommandDescription("Context driven toggling lock children command. If in a multi selection with multiple state, children will be unlocked"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, toggle_children_locking, "Toggle Children Locking", "", "", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, hide, CCommandDescription("Context driven hide command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, hide, "", "", "icons:General/Visibility_False.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, unhide, CCommandDescription("Context driven unhide command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, unhide, "", "", "icons:General/Visibility_True.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, toggle_visibility,
                                                  CCommandDescription("Context driven toggling visibility command. If in a multi selection with multiple state, items will be unhidden"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, toggle_visibility, "Visible", "", "icons:General/Visibility_True.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, isolate_visibility, CCommandDescription("Context driven visibility isolation command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, isolate_visibility, "Isolate Visibility", "Ctrl+Shift+H", "", true)
REGISTER_COMMAND_REMAPPING(level, toggle_hide_all_other_layers, general, isolate_visibility)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, hide_all, CCommandDescription("Context driven hide all command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, hide_all, "Hide All", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, unhide_all, CCommandDescription("Context driven unhide all command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, unhide_all, "Unhide All", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, hide_children, CCommandDescription("Hides children of the selected item(s)"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, hide_children, "Hide Children", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, unhide_children, CCommandDescription("Unhides children of the selected item(s)"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, unhide_children, "Unhide Children", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, toggle_children_visibility,
                                                  CCommandDescription("Context driven toggling children visibility command. If in a multi selection with multiple state, children will be unhidden"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, toggle_children_visibility, "Toggle Children Visibility", "", "", true)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyUndo, general, undo,
                                     "Undoes the last operation",
                                     "general.undo()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, undo, "Undo", CKeyboardShortcut::StandardKey::Undo, "icons:General/History_Undo.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyRedo, general, redo,
                                     "Redoes the last undone operation",
                                     "general.redo()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, redo, "Redo", CKeyboardShortcut::StandardKey::Redo, "icons:General/History_Redo.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyFocusOnLevelEditor, general, focus_level_editor,
                                     "Brings the level editor window to front",
                                     "general.focus_level_editor()");
REGISTER_EDITOR_COMMAND_SHORTCUT(general, focus_level_editor, "F4");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyOpenOrFocusViewPane, general, open_or_focus_pane,
                                     "Opens an existing view pane specified by the pane class name. If no such pane exists, create it.",
                                     "general.open_or_focus_pane(str paneClassName)");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::OpenEditorMenu, general, open_editor_menu,
                                   CCommandDescription("Open Editor Menu"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, open_editor_menu, "", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(general, toggle_sync_selection, CCommandDescription("Context driven toggle sync selection command"))
REGISTER_EDITOR_UI_COMMAND_DESC(general, toggle_sync_selection, "Sync Selection", "", "icons:general_sync_selection.ico", true)

//////////////////////////////////////////////////////////////////////////
// Main frame UI Commands
//////////////////////////////////////////////////////////////////////////

namespace Private_EditorCommands
{
void PySetConfigSpec(int spec)
{
	GetIEditorImpl()->SetEditorConfigSpec((ESystemConfigSpec)spec);
}
}

REGISTER_PYTHON_ENUM_BEGIN(ESystemConfigSpec, general, system_config_spec)
REGISTER_PYTHON_ENUM_ITEM(CONFIG_CUSTOM, custom)
REGISTER_PYTHON_ENUM_ITEM(CONFIG_LOW_SPEC, low)
REGISTER_PYTHON_ENUM_ITEM(CONFIG_MEDIUM_SPEC, medium)
REGISTER_PYTHON_ENUM_ITEM(CONFIG_HIGH_SPEC, high)
REGISTER_PYTHON_ENUM_ITEM(CONFIG_VERYHIGH_SPEC, veryhigh)
REGISTER_PYTHON_ENUM_ITEM(CONFIG_DURANGO, durango)
REGISTER_PYTHON_ENUM_ITEM(CONFIG_ORBIS, orbis)
REGISTER_PYTHON_ENUM_END

  REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySetConfigSpec, general, set_config_spec,
                                       "Sets the system config spec.",
                                       "general.set_config_spec(int specNumber)");
