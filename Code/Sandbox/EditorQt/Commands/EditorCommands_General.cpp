// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "IEditorImpl.h"
#include "Util/BoostPythonHelpers.h"

#include "EditorFramework/Events.h"

#include "Qt/QToolTabManager.h"

//////////////////////////////////////////////////////////////////////////
// Editor Framework Commands
// TODO : These are handled in EditorCommon, for instance EditorFramework/Editor.cpp. Move these to more appropriate place
//////////////////////////////////////////////////////////////////////////

namespace Private_EditorCommands
{
void PyNew()
{
	CommandEvent("general.new").SendToKeyboardFocus();
}

void PyOpen()
{
	CommandEvent("general.open").SendToKeyboardFocus();
}

void PySave()
{
	CommandEvent("general.save").SendToKeyboardFocus();
}

void PySaveAs()
{
	CommandEvent("general.save_as").SendToKeyboardFocus();
}

void PyClose()
{
	CommandEvent("general.close").SendToKeyboardFocus();
}

void PyCut()
{
	CommandEvent("general.cut").SendToKeyboardFocus();
}

void PyCopy()
{
	CommandEvent("general.copy").SendToKeyboardFocus();
}

void PyPaste()
{
	CommandEvent("general.paste").SendToKeyboardFocus();
}

void PyDelete()
{
	CommandEvent("general.delete").SendToKeyboardFocus();
}

void PyDuplicate()
{
	CommandEvent("general.duplicate").SendToKeyboardFocus();
}

void PyFind()
{
	CommandEvent("general.find").SendToKeyboardFocus();
}

void PyFindPrevious()
{
	CommandEvent("general.find_previous").SendToKeyboardFocus();
}

void PyFindNext()
{
	CommandEvent("general.find_next").SendToKeyboardFocus();
}

void PySelectAll()
{
	CommandEvent("general.select_all").SendToKeyboardFocus();
}

void PyHelp()
{
	CommandEvent("general.help").SendToKeyboardFocus();
}

void PyZoomIn()
{
	CommandEvent("general.zoom_in").SendToKeyboardFocus();
}

void PyZoomOut()
{
	CommandEvent("general.zoom_out").SendToKeyboardFocus();
}

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

}

DECLARE_PYTHON_MODULE(general);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyNew, general, new,
                                     "New...",
                                     "general.new()")
REGISTER_EDITOR_UI_COMMAND_DESC(general, new, "&New...", CKeyboardShortcut::StandardKey::New, "icons:General/File_New.ico", false)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyOpen, general, open,
                                     "Open...",
                                     "general.open()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, open, "&Open...", CKeyboardShortcut::StandardKey::Open, "icons:General/Folder_Open.ico", false)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyClose, general, close,
                                     "Close",
                                     "general.close()");
REGISTER_EDITOR_COMMAND_SHORTCUT(general, close, CKeyboardShortcut::StandardKey::Close);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySave, general, save,
                                     "Save",
                                     "general.save()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, save, "&Save", CKeyboardShortcut::StandardKey::Save, "icons:General/File_Save.ico", false)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySaveAs, general, save_as,
                                     "Save As...",
                                     "general.save_as()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, save_as, "Save As...", CKeyboardShortcut::StandardKey::SaveAs, "icons:General/Save_as.ico", false)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyCut, general, cut,
                                     "Cut",
                                     "general.cut()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, cut, "Cut", CKeyboardShortcut::StandardKey::Cut, "icons:General/Cut.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyCopy, general, copy,
                                     "Copy",
                                     "general.copy()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, copy, "Copy", CKeyboardShortcut::StandardKey::Copy, "icons:General/Copy.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyPaste, general, paste,
                                     "Paste",
                                     "general.paste()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, paste, "Paste", CKeyboardShortcut::StandardKey::Paste, "icons:General/Paste.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyDelete, general, delete,
                                     "Delete",
                                     "general.delete()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, delete, "Delete", CKeyboardShortcut::StandardKey::Delete, "icons:General/Delete.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyDuplicate, general, duplicate,
                                     "Duplicate",
                                     "general.duplicate()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, duplicate, "Duplicate", CKeyboardShortcut("Ctrl+D"), "icons:General/Duplicate.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyFind, general, find,
                                     "Find",
                                     "general.find()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, find, "Find", CKeyboardShortcut::StandardKey::Find, "icons:General/Search.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyFindPrevious, general, find_previous,
                                     "Find Previous",
                                     "general.find_previous()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, find_previous, "Find Previous", CKeyboardShortcut::StandardKey::FindPrevious, "icons:General/Arrow_Left.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyFindNext, general, find_next,
                                     "Find Next",
                                     "general.find_next()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, find_next, "Find Next", CKeyboardShortcut::StandardKey::FindNext, "icons:General/Arrow_Right.ico", false);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySelectAll, general, select_all,
                                     "Select All",
                                     "general.select_all()");
REGISTER_EDITOR_COMMAND_SHORTCUT(general, select_all, CKeyboardShortcut::StandardKey::SelectAll);

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyHelp, general, help, CCommandDescription("Go to documentation..."));
REGISTER_EDITOR_UI_COMMAND_DESC(general, help, "", CKeyboardShortcut::StandardKey::HelpWiki, "icons:Tools/Go_To_Documentation.ico", false);

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyZoomIn, general, zoom_in, CCommandDescription("Zoom In"));
REGISTER_EDITOR_UI_COMMAND_DESC(general, zoom_in, "Zoom In", CKeyboardShortcut::StandardKey::ZoomIn, "icons:General/Zoom_In.ico", false);

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PyZoomOut, general, zoom_out, CCommandDescription("Zoom Out"));
REGISTER_EDITOR_UI_COMMAND_DESC(general, zoom_out, "Zoom Out", CKeyboardShortcut::StandardKey::ZoomOut, "icons:General/Zoom_Out.ico", false);

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

