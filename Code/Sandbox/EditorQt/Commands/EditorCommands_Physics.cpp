// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "IEditorImpl.h"
#include "PhysTool.h"

namespace Private_EditorCommands
{
void PyPhysicsStep()
{
	gEnv->pPhysicalWorld->GetPhysVars()->bDoStep = 1;
}

void PyPhysicsSingleStep()
{
	gEnv->pPhysicalWorld->GetPhysVars()->bSingleStepMode ^= 1;

	QAction* pAction = GetIEditor()->GetICommandManager()->GetAction("physics.single_step");
	if (pAction)
	{
		pAction->setCheckable(true); // Make sure the action is checkable
		pAction->setChecked(gEnv->pPhysicalWorld->GetPhysVars()->bSingleStepMode == 1);
	}
}

void PySetPhysicsTool()
{
	if (GetIEditorImpl()->GetEditTool() && GetIEditorImpl()->GetEditTool()->IsKindOf(RUNTIME_CLASS(CPhysPullTool)))
		GetIEditorImpl()->SetEditTool(0);
	else
		GetIEditorImpl()->SetEditTool(new CPhysPullTool());
}
}

DECLARE_PYTHON_MODULE(physics);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyPhysicsStep, physics, step,
                                     "Do a single physics step ('>' in game mode)",
                                     "physics.step()");
REGISTER_EDITOR_COMMAND_ICON(physics, step, "icons:common/general_physics_step.ico");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PyPhysicsSingleStep, physics, single_step,
                                     "Toggle single-step mode in physics ('<' in game mode)",
                                     "physics.single_step()");
REGISTER_EDITOR_COMMAND_ICON(physics, single_step, "icons:common/general_physics_pause.ico");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorCommands::PySetPhysicsTool, physics, set_physics_tool,
                                     "Turns on physics tool mode (Ctrl/Shift modify)",
                                     "physics.set_physics_tool()");
REGISTER_EDITOR_COMMAND_ICON(physics, set_physics_tool, "icons:General/Physics_Tool.ico");

