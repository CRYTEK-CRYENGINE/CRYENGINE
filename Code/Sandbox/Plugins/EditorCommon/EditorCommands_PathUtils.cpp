// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "BoostPythonMacros.h"
#include "EditorFramework/Events.h"

DECLARE_PYTHON_MODULE(path_utils)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(path_utils, copy_name, CCommandDescription("Copies name of selected file or asset to the clipboard"))
REGISTER_EDITOR_UI_COMMAND_DESC(path_utils, copy_name, "Copy Name", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(path_utils, copy_path, CCommandDescription("Copies path of selected file or asset to the clipboard"))
REGISTER_EDITOR_UI_COMMAND_DESC(path_utils, copy_path, "Copy Path", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(path_utils, show_in_file_explorer, CCommandDescription("Opens an OS file explorer showing the selected file or folder"))
REGISTER_EDITOR_UI_COMMAND_DESC(path_utils, show_in_file_explorer, "Open in File Explorer", "", "", false)
