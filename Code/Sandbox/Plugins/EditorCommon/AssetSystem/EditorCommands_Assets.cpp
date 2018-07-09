// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "IEditor.h"
#include "BoostPythonMacros.h"

#include "EditorFramework/Events.h"

#include "QtUtil.h"
#include "AssetSystem/Browser/AssetDropHandler.h"
#include "FileDialogs/SystemFileDialog.h"

namespace Private_EditorCommands
{
	//Asset browser is not a unique pane but this one has a unique behavior. It is toggled on F2
	void ToggleAssetBrowser()
	{
		CommandEvent("asset.toggle_asset_browser").SendToKeyboardFocus();
	}

	void ShowInAssetBrowser(const char* path)
	{
		string cmd("asset.show_in_asset_browser ");
		cmd += path;
		CommandEvent(cmd.c_str()).SendToKeyboardFocus();
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
			(void)dropHandler.Import(filePaths, importParams);
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

REGISTER_PYTHON_COMMAND(Private_EditorCommands::ToggleAssetBrowser, asset, toggle_browser, "Show/Hide the main Asset Browser");
REGISTER_EDITOR_COMMAND_SHORTCUT(asset, toggle_browser, "F2; Alt+B");
REGISTER_PYTHON_COMMAND(Private_EditorCommands::ShowInAssetBrowser, asset, show_in_browser, "Show asset in asset browser");

REGISTER_PYTHON_COMMAND(Private_EditorCommands::ShowNewAssetBrowser, asset, open_browser, "Opens a new Asset Browser");
REGISTER_EDITOR_COMMAND_SHORTCUT(asset, open_browser, "Alt+F2; Ctrl+Alt+B");

REGISTER_PYTHON_COMMAND(Private_EditorCommands::PyImportFiles, asset, import, "Imports assets");
REGISTER_PYTHON_COMMAND(Private_EditorCommands::PyImportDialog, asset, import_dialog, "Imports assets");
REGISTER_EDITOR_COMMAND_TEXT(asset, import_dialog, "Import");

