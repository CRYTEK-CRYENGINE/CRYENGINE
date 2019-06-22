// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

// Sandbox
#include "CryEdit.h"
#include "IEditorImpl.h"
#include "Objects/ObjectLayer.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/SelectionGroup.h"
#include "Util/BoostPythonHelpers.h"

// Editor Interface
#include <IUndoObject.h>

// Editor Common
#include <Commands/ICommandManager.h>
#include <Dialogs/QNumericBoxDialog.h>
#include <EditorFramework/Events.h>
#include <LevelEditor/LevelEditorSharedState.h>

namespace Private_EditorCommands
{
void Select()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditMode(CLevelEditorSharedState::EditMode::Select);
}

void Move()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditMode(CLevelEditorSharedState::EditMode::Move);
}

void Rotate()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditMode(CLevelEditorSharedState::EditMode::Rotate);
}

void Scale()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditMode(CLevelEditorSharedState::EditMode::Scale);
}

void PickMaterial()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool("Material.PickTool");
}

static float s_fastRotateAngle {
	45
};

void FastRotateX()
{
	CUndo undo("Rotate X");
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	pSelection->Rotate(Ang3(s_fastRotateAngle, 0, 0));
}

void FastRotateY()
{
	CUndo undo("Rotate Y");
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	pSelection->Rotate(Ang3(0, s_fastRotateAngle, 0));
}

void FastRotateZ()
{
	CUndo undo("Rotate Z");
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	pSelection->Rotate(Ang3(0, 0, s_fastRotateAngle));
}

void SetFastRotateAngle()
{
	QNumericBoxDialog dlg(QObject::tr("Angle"), s_fastRotateAngle);
	dlg.SetRange(-359.99f, 359.99f);
	dlg.SetStep(5.0f);

	if (dlg.exec() == QDialog::Accepted)
	{
		s_fastRotateAngle = dlg.GetValue();
	}
}

void EnableXAxisConstraint()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::X);
}

void EnableYAxisConstraint()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::Y);
}

void EnableZAxisConstraint()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::Z);
}

void EnableXYAxisConstraint()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::XY);
}
}

DECLARE_PYTHON_MODULE(tools);

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::Move, tools, move,
                                   CCommandDescription("Sets the current edit mode to move"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, move, "Move", "1", "icons:Navigation/Basics_Move.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionMove, tools, move)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::Rotate, tools, rotate,
                                   CCommandDescription("Sets the current edit mode to rotate"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, rotate, "Rotate", "2", "icons:Navigation/Basics_Rotate.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionRotate, tools, rotate)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::Scale, tools, scale,
                                   CCommandDescription("Sets the current edit mode to scale"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, scale, "Scale", "3", "icons:Navigation/Basics_Scale.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionScale, tools, scale)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::Select, tools, select,
                                   CCommandDescription("Sets the current edit mode to select"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, select, "Select", "4", "icons:Navigation/Basics_Select.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionSelect_Mode, tools, select)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::PickMaterial, tools, pick_material,
                                   CCommandDescription("Sets the material picker as the current active tool"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, pick_material, "Pick Material", "", "icons:General/Picker.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionPick_Material, tools, pick_material)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::FastRotateX, tools, fast_rotate_x,
                                   CCommandDescription("Quickly rotate in X axis by customized angle amount"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, fast_rotate_x, "Fast Rotate X", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionRotate_X_Axis, tools, fast_rotate_x)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::FastRotateY, tools, fast_rotate_y,
                                   CCommandDescription("Quickly rotate in Y axis by customized angle amount"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, fast_rotate_y, "Fast Rotate Y", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionRotate_Y_Axis, tools, fast_rotate_y)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::FastRotateZ, tools, fast_rotate_z,
                                   CCommandDescription("Quickly rotate in Z axis by customized angle amount"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, fast_rotate_z, "Fast Rotate Z", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionRotate_Z_Axis, tools, fast_rotate_z)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::SetFastRotateAngle, tools, set_fast_rotate_angle,
                                   CCommandDescription("Sets angle to be used with fast rotate"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, set_fast_rotate_angle, "Customize Fast Rotate Angle...", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionRotate_Angle, tools, set_fast_rotate_angle)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::EnableXAxisConstraint, tools, enable_x_axis_constraint,
                                   CCommandDescription("Sets X axis transform constraint"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, enable_x_axis_constraint, "X Axis Constraint", "", "icons:Navigation/Axis_X.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionLock_X_Axis, tools, enable_x_axis_constraint)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::EnableYAxisConstraint, tools, enable_y_axis_constraint,
                                   CCommandDescription("Sets Y axis transform constraint"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, enable_y_axis_constraint, "Y Axis Constraint", "", "icons:Navigation/Axis_Y.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionLock_Y_Axis, tools, enable_y_axis_constraint)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::EnableZAxisConstraint, tools, enable_z_axis_constraint,
                                   CCommandDescription("Sets Z axis transform constraint"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, enable_z_axis_constraint, "Z Axis Constraint", "", "icons:Navigation/Axis_Z.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionLock_Z_Axis, tools, enable_z_axis_constraint)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_EditorCommands::EnableXYAxisConstraint, tools, enable_xy_axis_constraint,
                                   CCommandDescription("Sets XY axis transform constraint"));
REGISTER_EDITOR_UI_COMMAND_DESC(tools, enable_xy_axis_constraint, "XY Axis Constraint", "", "icons:Navigation/Axis_XY.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionLock_XY_Axis, tools, enable_xy_axis_constraint)
