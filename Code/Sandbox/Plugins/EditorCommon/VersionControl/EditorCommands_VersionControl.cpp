// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "BoostPythonMacros.h"
#include "EditorFramework/Events.h"
#include "IEditor.h"

DECLARE_PYTHON_MODULE(version_control_system);

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, refresh, CCommandDescription("Refresh status of assets/layers"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, get_latest, CCommandDescription("Get latest assets/layers"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, check_out, CCommandDescription("Check out assets/layers"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, revert, CCommandDescription("Revert assets/layers"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, revert_unchanged, CCommandDescription("Revert assets/layers if unchanged"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, submit, CCommandDescription("Open Submit dialog with selected assets/layers"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, refresh_work_files, CCommandDescription("Refresh status of work files"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, get_latest_work_files, CCommandDescription("Get latest work files"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, check_out_work_files, CCommandDescription("Check out work files"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, revert_work_files, CCommandDescription("Revert work files"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, revert_unchanged_work_files, CCommandDescription("Revert work files if unchanged"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, submit_work_files, CCommandDescription("Open Submit dialog with selected work files"));

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(version_control_system, remove_local_work_files, CCommandDescription("Open Submit dialog with selected work files"));
