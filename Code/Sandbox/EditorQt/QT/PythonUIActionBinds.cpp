// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2014.
// -------------------------------------------------------------------------
//  File name: PythonUiActionBinds.cpp
//  Created:   06/10/2014 by timur
//  Description: Python stabs for ui actions
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>

#include "CryEdit.h"
#include "QtMainFrame.h"
#include "Util/BoostPythonHelpers.h"

#include <EditorFramework/Preferences.h>

#include <QApplication>
#include <QtGui/QtEvents>

class QAction;

DECLARE_PYTHON_MODULE(ui_action);

extern CCryEditApp theApp;
namespace
{
CCryEditApp* GetEditApp() { return &theApp; }
}

class UiActionCommandModuleDescription : public CCommandModuleDescription
{
public:

	UiActionCommandModuleDescription(const char* name, const char* uiName, const char* description)
		: CCommandModuleDescription(name, uiName, description)
	{}

	virtual string FormatCommandForUI(const char* command) const override
	{
		string forUi(command);

		if (forUi.find("action") == 0)
		{
			forUi = forUi.substr(6);
		}

		return CCommandModuleDescription::FormatCommandForUI(forUi);
	}
};

REGISTER_EDITOR_COMMAND_MODULE_WITH_DESC(UiActionCommandModuleDescription, ui_action, "Menus and Toolbars", "");

#define REGISTER_EDIT_APP_UI_ACTION(EditAppMemberFuncName, action_name)                  \
  void PyBind_ui_action_ ## action_name() { GetEditApp()-> ## EditAppMemberFuncName(); } \
  REGISTER_PYTHON_COMMAND(PyBind_ui_action_ ## action_name, ui_action, action_name, "");

REGISTER_EDIT_APP_UI_ACTION(OnEditmodeSelect, actionSelect_Mode);
REGISTER_EDIT_APP_UI_ACTION(OnEditmodeMove, actionMove);
REGISTER_EDIT_APP_UI_ACTION(OnEditmodeRotate, actionRotate);
REGISTER_EDIT_APP_UI_ACTION(OnEditmodeScale, actionScale);

REGISTER_EDIT_APP_UI_ACTION(OnExportSelectedObjects, actionExport_Selected_Objects);

REGISTER_EDIT_APP_UI_ACTION(OnExportToEngine, actionExport_to_Engine);

REGISTER_EDIT_APP_UI_ACTION(OnFileExportOcclusionMesh, actionExport_Occlusion_Mesh);

REGISTER_EDIT_APP_UI_ACTION(OnViewSwitchToGame, actionSwitch_to_Game);
REGISTER_EDIT_APP_UI_ACTION(OnEditSelectNone, actionSelect_None);

REGISTER_EDIT_APP_UI_ACTION(OnScriptCompileScript, actionCompile_Script);

REGISTER_EDIT_APP_UI_ACTION(OnEditToolLink, actionLink);
REGISTER_EDIT_APP_UI_ACTION(OnEditToolUnlink, actionUnlink);

REGISTER_EDIT_APP_UI_ACTION(OnSelectionSave, actionSave_Selected_Objects);
REGISTER_EDIT_APP_UI_ACTION(OnSelectionLoad, actionLoad_Selected_Objects);

REGISTER_EDIT_APP_UI_ACTION(OnAlignObject, actionAlign_To_Object);
REGISTER_EDIT_APP_UI_ACTION(OnAlignToGrid, actionAlign_To_Grid);
REGISTER_EDIT_APP_UI_ACTION(OnGroupAttach, actionGroup_Attach);
REGISTER_EDIT_APP_UI_ACTION(OnGroupClose, actionGroup_Close);
REGISTER_EDIT_APP_UI_ACTION(OnGroupDetach, actionGroup_Detach);
REGISTER_EDIT_APP_UI_ACTION(OnGroupDetachToRoot, actionGroup_DetachToRoot);

REGISTER_EDIT_APP_UI_ACTION(OnGroupMake, actionGroup);

REGISTER_EDIT_APP_UI_ACTION(OnGroupOpen, actionGroup_Open);

REGISTER_EDIT_APP_UI_ACTION(OnGroupUngroup, actionUngroup);

REGISTER_EDIT_APP_UI_ACTION(OnLockSelection, actionLock_Selection);
REGISTER_EDIT_APP_UI_ACTION(OnFileEditLogFile, actionShow_Log_File);

REGISTER_EDIT_APP_UI_ACTION(OnReloadTextures, actionReload_Texture_Shaders);
REGISTER_EDIT_APP_UI_ACTION(OnReloadArchetypes, actionReload_Archetypes);
REGISTER_EDIT_APP_UI_ACTION(OnReloadAllScripts, actionReload_All_Scripts);
REGISTER_EDIT_APP_UI_ACTION(OnReloadEntityScripts, actionReload_Entity_Scripts);
REGISTER_EDIT_APP_UI_ACTION(OnReloadItemScripts, actionReload_Item_Scripts);
REGISTER_EDIT_APP_UI_ACTION(OnReloadAIScripts, actionReload_AI_Scripts);
REGISTER_EDIT_APP_UI_ACTION(OnReloadUIScripts, actionReload_UI_Scripts);
REGISTER_EDIT_APP_UI_ACTION(OnReloadGeometry, actionReload_Geometry);

REGISTER_EDIT_APP_UI_ACTION(OnSwitchPhysics, actionEnable_Physics_AI);

REGISTER_EDIT_APP_UI_ACTION(OnSyncPlayer, actionSynchronize_Player_with_Camera);

REGISTER_EDIT_APP_UI_ACTION(OnResourcesReduceworkingset, actionReduce_Working_Set);
REGISTER_EDIT_APP_UI_ACTION(OnToolsUpdateProcVegetation, actionUpdate_Procedural_Vegetation);

REGISTER_EDIT_APP_UI_ACTION(ToggleHelpersDisplay, actionShow_Hide_Helpers);

REGISTER_EDIT_APP_UI_ACTION(OnEditHide, actionHide_Selection);

REGISTER_EDIT_APP_UI_ACTION(OnEditUnhideall, actionUnhide_All);
REGISTER_EDIT_APP_UI_ACTION(OnEditFreeze, actionFreeze_Selection);
REGISTER_EDIT_APP_UI_ACTION(OnEditUnfreezeall, actionUnfreeze_All);

REGISTER_EDIT_APP_UI_ACTION(OnWireframe, actionWireframe);

// Tag Locations.
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation1, actionRemember_Location_1);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation2, actionRemember_Location_2);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation3, actionRemember_Location_3);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation4, actionRemember_Location_4);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation5, actionRemember_Location_5);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation6, actionRemember_Location_6);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation7, actionRemember_Location_7);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation8, actionRemember_Location_8);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation9, actionRemember_Location_9);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation10, actionRemember_Location_10);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation11, actionRemember_Location_11);
REGISTER_EDIT_APP_UI_ACTION(OnTagLocation12, actionRemember_Location_12);

REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation1, actionGoto_Location_1)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation2, actionGoto_Location_2)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation3, actionGoto_Location_3)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation4, actionGoto_Location_4)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation5, actionGoto_Location_5)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation6, actionGoto_Location_6)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation7, actionGoto_Location_7)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation8, actionGoto_Location_8)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation9, actionGoto_Location_9)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation10, actionGoto_Location_10)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation11, actionGoto_Location_11)
REGISTER_EDIT_APP_UI_ACTION(OnGotoLocation12, actionGoto_Location_12)

REGISTER_EDIT_APP_UI_ACTION(OnToolsLogMemoryUsage, actionSave_Level_Statistics);

REGISTER_EDIT_APP_UI_ACTION(OnDisplayGotoPosition, actionGoto_Position);

REGISTER_EDIT_APP_UI_ACTION(OnRuler, actionRuler);

REGISTER_EDIT_APP_UI_ACTION(OnRotateselectionXaxis, actionRotate_X_Axis);
REGISTER_EDIT_APP_UI_ACTION(OnRotateselectionYaxis, actionRotate_Y_Axis);
REGISTER_EDIT_APP_UI_ACTION(OnRotateselectionZaxis, actionRotate_Z_Axis);
REGISTER_EDIT_APP_UI_ACTION(OnRotateselectionRotateangle, actionRotate_Angle);

REGISTER_EDIT_APP_UI_ACTION(OnMaterialAssigncurrent, actionMaterial_Assign_Current);
REGISTER_EDIT_APP_UI_ACTION(OnMaterialResettodefault, actionMaterial_Reset_to_Default);
REGISTER_EDIT_APP_UI_ACTION(OnMaterialGetmaterial, actionMaterial_Get_from_Selected);
REGISTER_EDIT_APP_UI_ACTION(OnPhysicsGetState, actionGet_Physics_State);
REGISTER_EDIT_APP_UI_ACTION(OnPhysicsResetState, actionReset_Physics_State);
REGISTER_EDIT_APP_UI_ACTION(OnPhysicsSimulateObjects, actionPhysics_Simulate_Objects);
REGISTER_EDIT_APP_UI_ACTION(OnPhysicsGenerateJoints, actionPhysics_Generate_Joints);
REGISTER_EDIT_APP_UI_ACTION(OnFileSavelevelresources, actionSave_Level_Resources);
REGISTER_EDIT_APP_UI_ACTION(OnValidateLevel, actionCheck_Level_for_Errors);
REGISTER_EDIT_APP_UI_ACTION(OnValidateObjectPositions, actionCheck_Object_Positions);
REGISTER_EDIT_APP_UI_ACTION(OnEditInvertselection, actionSelect_Invert);

REGISTER_EDIT_APP_UI_ACTION(OnSwitchToDefaultCamera, actionCamera_Default);
REGISTER_EDIT_APP_UI_ACTION(OnSwitchToSequenceCamera, actionCamera_Sequence);
REGISTER_EDIT_APP_UI_ACTION(OnSwitchToSelectedcamera, actionCamera_Selected_Object);
REGISTER_EDIT_APP_UI_ACTION(OnSwitchcameraNext, actionCamera_Cycle);

REGISTER_EDIT_APP_UI_ACTION(OnMaterialPicktool, actionPick_Material);
REGISTER_EDIT_APP_UI_ACTION(OnResolveMissingObjects, actionResolve_Missing_Objects);

namespace
{
void PyOpenViewPane(const char* viewClassName)
{
	GetIEditorImpl()->OpenView(viewClassName);
}

std::vector<std::string> PyGetViewPaneClassNames()
{
	IEditorClassFactory* pClassFactory = GetIEditorImpl()->GetClassFactory();
	std::vector<IClassDesc*> classDescs;
	pClassFactory->GetClassesBySystemID(ESYSTEM_CLASS_VIEWPANE, classDescs);

	std::vector<std::string> classNames;
	for (auto iter = classDescs.begin(); iter != classDescs.end(); ++iter)
	{
		classNames.push_back((*iter)->ClassName());
	}

	return classNames;
}

void PyExit()
{
	if (CEditorMainFrame::m_pInstance)
	{
		CEditorMainFrame::m_pInstance->window()->close();
	}
	else
	{
		qApp->exit();
	}
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyExit, general, exit,
                                     "Exits the editor.",
                                     "general.exit()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyOpenViewPane, general, open_pane,
                                     "Opens a view pane specified by the pane class name.",
                                     "general.open_pane(str paneClassName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetViewPaneClassNames, general, get_pane_class_names,
                                          "Get all available class names for use with open_pane & close_pane.",
                                          "[str] general.get_pane_class_names()");

//////////////////////////////////////////////////////////////////////////
//TODO : move this out of python files, this does not belong here
struct SAudioGeneralPreferences : public SPreferencePage
{
	SAudioGeneralPreferences()
		: SPreferencePage("General", "Audio/General")
		, bMuteAudio(false)
	{
	}

	virtual bool Serialize(yasli::Archive& ar) override
	{
		ar.openBlock("general", "General");
		ar(bMuteAudio, "muteAudio", "Mute Audio");
		ar.closeBlock();

		if (ar.isInput())
		{
			if (bMuteAudio)
			{
				gEnv->pAudioSystem->ExecuteTrigger(CryAudio::MuteAllTriggerId);
			}
			else
			{
				gEnv->pAudioSystem->ExecuteTrigger(CryAudio::UnmuteAllTriggerId);
			}
		}

		return true;
	}

	bool bMuteAudio;
};

SAudioGeneralPreferences gAudioGeneralPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SAudioGeneralPreferences, &gAudioGeneralPreferences)

//////////////////////////////////////////////////////////////////////////

namespace
{
//////////////////////////////////////////////////////////////////////////
void PyStopAllSounds()
{
	CryLogAlways("<Audio> Executed \"Stop All Sounds\" command.");
	gEnv->pAudioSystem->StopAllSounds();
}

//////////////////////////////////////////////////////////////////////////
void PyRefreshAudioSystem()
{
	CryAudio::SRequestUserData const data(CryAudio::ERequestFlags::ExecuteBlocking);
	gEnv->pAudioSystem->Refresh(GetIEditorImpl()->GetLevelName(), data);
}

//////////////////////////////////////////////////////////////////////////
void PyToggleMuteAudioSystem()
{
	gAudioGeneralPreferences.bMuteAudio = !gAudioGeneralPreferences.bMuteAudio;
	if (gAudioGeneralPreferences.bMuteAudio)
	{
		gEnv->pAudioSystem->ExecuteTrigger(CryAudio::MuteAllTriggerId);
	}
	else
	{
		gEnv->pAudioSystem->ExecuteTrigger(CryAudio::UnmuteAllTriggerId);
	}
}
};

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyStopAllSounds, ui_action, actionStop_All_Sounds, "Stop all playing sounds.", "");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRefreshAudioSystem, ui_action, actionRefresh_Audio, "Send refresh event to the audio system.", "");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyToggleMuteAudioSystem, ui_action, actionMute_Audio, "Toggle Mute of the Sound System.", "");

namespace
{
void PyConstrainXAxis()
{
	GetIEditorImpl()->SetAxisConstrains(AXIS_X);
}

void PyConstrainYAxis()
{
	GetIEditorImpl()->SetAxisConstrains(AXIS_Y);
}

void PyConstrainZAxis()
{
	GetIEditorImpl()->SetAxisConstrains(AXIS_Z);
}

void PyConstrainXYPlane()
{
	GetIEditorImpl()->SetAxisConstrains(AXIS_XY);
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyConstrainXAxis, ui_action, actionLock_X_Axis, "Constrain to X axis", "");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyConstrainYAxis, ui_action, actionLock_Y_Axis, "Constrain to Y axis", "");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyConstrainZAxis, ui_action, actionLock_Z_Axis, "Constrain to Z axis", "");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyConstrainXYPlane, ui_action, actionLock_XY_Axis, "Constrain to XY plane", "");

namespace
{
void PyExecute(const char* pythonCmd)
{
	PyScript::Execute(pythonCmd);
}
}

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyExecute, python, execute, "Executes python code", "");

