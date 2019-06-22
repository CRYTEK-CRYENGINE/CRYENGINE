// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "AssetManager.h"
#include "AssetSystem/Browser/AssetDropHandler.h"
#include "BoostPythonMacros.h"
#include "Browser/AssetBrowser.h"
#include "EditorFramework/Events.h"
#include "FileDialogs/SystemFileDialog.h"
#include "QtUtil.h"

#include <IEditor.h>

namespace Private_EditorCommands
{

void ShowInAssetBrowser(const char* path)
{
	string cmd = string().Format("asset.show_in_browser '%s'", path);
	CommandEvent(cmd.c_str()).SendToKeyboardFocus();
}

void ShowInNewAssetBrowser(const char* path)
{
	CAssetBrowser* const pAssetBrowser = static_cast<CAssetBrowser*>(GetIEditor()->CreateDockable("Asset Browser"));
	if (pAssetBrowser)
	{
		pAssetBrowser->SelectAsset(path);
	}
}

void EditAsset(const char* path)
{
	CAsset* const pAsset = GetIEditor()->GetAssetManager()->FindAssetForFile(path);
	if (pAsset)
	{
		pAsset->Edit();
	}
}

void ShowNewAssetBrowser()
{
	GetIEditor()->ExecuteCommand("general.open_pane 'Asset Browser'");
}

void ImportFiles(const std::vector<string>& filePaths)
{
	CAssetDropHandler dropHandler;
	if (dropHandler.CanImportAny(filePaths))
	{
		CAssetDropHandler::SImportParams importParams;
		dropHandler.Import(filePaths, importParams);
	}
}

//! Shows file dialog for importing multiple assets.
void PyImportDialog()
{
	std::vector<string> filePaths;
	{
		CSystemFileDialog::RunParams runParams;
		std::vector<QString> v = CSystemFileDialog::RunImportMultipleFiles(runParams, nullptr);
		filePaths.reserve(v.size());
		std::transform(v.begin(), v.end(), std::back_inserter(filePaths), QtUtil::ToString);
	}
	ImportFiles(filePaths);
}

//! Imports multiple assets.
//! \param filePath Absolute file path.
void PyImportFiles(const char* filePath)
{
	ImportFiles({ string(filePath) });
}

}

DECLARE_PYTHON_MODULE(asset);

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, toggle_browser, CCommandDescription("Show/Hide the main Asset Browser"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, toggle_browser, "Quick Asset Browser", "Alt+B", "icons:Tools/tools_asset-browser.ico", false)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ShowInAssetBrowser, asset, show_in_browser,
                                   CCommandDescription("Show asset in asset browser").Param("path", "The path to the asset to be shown"))

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::ShowInNewAssetBrowser, asset, show_in_new_browser,
                                   CCommandDescription("Show asset in a new instance of asset browser").Param("path", "The path to the asset to be shown"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::EditAsset, asset, edit,
                                   CCommandDescription("Open asset for editing").Param("path", "The path to the asset to be edited"))

REGISTER_PYTHON_COMMAND(Private_EditorCommands::ShowNewAssetBrowser, asset, open_browser, "Opens a new Asset Browser")
REGISTER_EDITOR_COMMAND_SHORTCUT(asset, open_browser, "Alt+F2; Ctrl+Alt+B")

REGISTER_PYTHON_COMMAND(Private_EditorCommands::PyImportFiles, asset, import, "Imports assets");
REGISTER_PYTHON_COMMAND(Private_EditorCommands::PyImportDialog, asset, import_dialog, "Imports assets")
REGISTER_EDITOR_COMMAND_TEXT(asset, import_dialog, "Import");

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, generate_thumbnails, CCommandDescription("Generate All Thumbnails"))

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, save_all, CCommandDescription("Saves all assets"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, save_all, "Save All", "", "icons:General/Save_all.ico", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, view_details, CCommandDescription("Details"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, view_details, "Shows Details", "", "icons:common/general_view_list.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, view_thumbnails, CCommandDescription("Thumbnails"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, view_thumbnails, "Shows Thumbnails", "", "icons:common/general_view_thumbnail.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, view_split_horizontally, CCommandDescription("Split Horizontally. Shows both details and thumbnails"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, view_split_horizontally, "Split Horizontally", "", "icons:common/general_view_horizonal.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, view_split_vertically, CCommandDescription("Split Vertically. Shows both details and thumbnails"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, view_split_vertically, "Split Vertically", "", "icons:common/general_view_vertical.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, view_folder_tree, CCommandDescription("Show Folder Tree"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, view_folder_tree, "", "", "icons:common/general_view_folder_tree.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, view_recursive_view, CCommandDescription("Recursive View"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, view_recursive_view, "", "", "icons:common/general_view_recursive_view.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, view_hide_irrelevant_folders, CCommandDescription("Hide Irrelevant Folders"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, view_hide_irrelevant_folders, "", "", "icons:common/general_hide_irrelevant_folders.ico", true)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, manage_work_files, CCommandDescription("Manage Work Files..."))

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, generate_and_repair_all_metadata, CCommandDescription("Generate/Repair All Metadata"))

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, discard_changes, CCommandDescription("Discard Changes"));

#if ASSET_BROWSER_USE_PREVIEW_WIDGET
REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(asset, show_preview, CCommandDescription("Show Preview"))
REGISTER_EDITOR_UI_COMMAND_DESC(asset, show_preview, "", "", "", true)
#endif
