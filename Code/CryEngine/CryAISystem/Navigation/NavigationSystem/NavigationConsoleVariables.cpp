// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavigationConsoleVariables.h"
#include "NavigationSystem.h"

void SAIConsoleVarsNavigation::Init()
{
	DefineConstIntCVarName("ai_DebugDrawNavigation", DebugDrawNavigation, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays the navigation debug information.\n"
		"Usage: ai_DebugDrawNavigation [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - triangles and contour\n"
		"2 - triangles, mesh and contours\n"
		"3 - triangles, mesh contours and external links\n"
		"4 - triangles, mesh contours, external links and triangle IDs\n"
		"5 - triangles, mesh contours, external links and island IDs\n"
		"6 - triangles with backfaces, mesh contours and external links\n");

	DefineConstIntCVarName("ai_MNMDebugTriangleOnCursor", DebugTriangleOnCursor, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays the basic information about the MNM Triangle where the cursor is pointing.\n"
		"Usage: ai_MNMDebugTriangleOnCursor [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - show triangle information\n");

	DefineConstIntCVarName("ai_NavigationSystemMT", NavigationSystemMT, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables navigation information updates on a separate thread.\n"
		"Usage: ai_NavigationSystemMT [0/1]\n"
		"Default is 1 (on)\n"
		"0 - off\n"
		"1 - on\n");

	DefineConstIntCVarName("ai_NavGenThreadJobs", NavGenThreadJobs, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Number of tile generation jobs per thread per frame.\n"
		"Usage: ai_NavGenThreadJobs [1+]\n"
		"Default is 1. The more you have, the faster it will go but the frame rate will drop while it works.\n"
		"Recommendations:\n"
		" Fast machine [10]\n"
		" Slow machine [4]\n"
		" Smooth [1]\n");

	DefineConstIntCVarName("ai_DebugDrawNavigationWorldMonitor", DebugDrawNavigationWorldMonitor, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables displaying bounding boxes for world changes.\n"
		"Usage: ai_DebugDrawNavigationWorldMonitor [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - on\n");

	DefineConstIntCVarName("ai_MNMRaycastImplementation", MNMRaycastImplementation, 2, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Defines which type of raycast implementation to use on the MNM meshes."
		"0 - Version 1. This version will be deprecated as it sometimes does not handle correctly the cases where the ray coincides with triangle egdes, which has been fixed in the new version.\n"
		"1 - Version 2. This version also fails in some cases when the ray goes exactly through triangle corners and then hitting the wall. \n"
		"2 - Version 3. The newest version that should be working in all special cases. This version is around 40% faster then the previous one. \n"
		"Any other value is used for the biggest version.");

	DefineConstIntCVarName("ai_MNMRemoveInaccessibleTrianglesOnLoad", MNMRemoveInaccessibleTrianglesOnLoad, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Defines whether inaccessible triangles can be removed from NavMeshes after they are loaded (Note: the triangles are never removed in Editor).");

	DefineConstIntCVarName("ai_StoreNavigationQueriesHistory", StoreNavigationQueriesHistory, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables the storage of navigation queries in a history. History can be displayed enabling the cvar ai_DebugDrawNavigationQueries.\n"
		"Requires ai_debugDraw and ai_debugDrawNavigation to be enabled.\n"
		"Usage: ai_StoreNavigationQueriesHistory [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - on\n");

	DefineConstIntCVarName("ai_DebugDrawNavigationQueriesUDR", DebugDrawNavigationQueriesUDR, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays the navigation queries debug information using UDR.\n"
		"Requires ai_debugDraw, ai_debugDrawNavigation and ai_storeNavigationQueriesHistory to be enabled.\n"
		"Usage: ai_DebugDrawNavigationQueriesUDR [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - Displays the query config, triangles by batches and invalidations\n");

	DefineConstIntCVarName("ai_DebugDrawNavigationQueriesList", DebugDrawNavigationQueriesList, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays a navigation queries list in the screen with all batches and elapsed times.\n"
		"Requires ai_debugDraw, ai_debugDrawNavigation and ai_storeNavigationQueriesHistory to be enabled.\n"
		"Note that, by their nature, instant queries won't be listed here.\n"
		"Usage: ai_DebugDrawNavigationQueriesList [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - on\n");

	REGISTER_CVAR2("ai_DebugDrawNavigationQueryListFilterByCaller", &DebugDrawNavigationQueryListFilterByCaller, "None", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Navigation Queries list only displays those queries that contain the given filter string. \n"
		"By default value is set to None (no filter is applied). \n"
		"Requires ai_debugDraw, ai_debugDrawNavigation and ai_DebugDrawNavigationQueriesList to be enabled.");

	REGISTER_CVAR2("ai_MNMDebugDrawFlag", &MNMDebugDrawFlag
		, "", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Color the MNM triangles is overriden by the specified annotation flag color.\n"
		"Usage: ai_MNMDebugDrawFlag 'flagName'\n"
		"Default is 'empty'\n");

	REGISTER_CVAR2("ai_NavmeshTileDistanceDraw", &NavmeshTileDistanceDraw, 200.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Maximum distance from the camera for tile to be drawn.\n"
		"Usage: ai_NavmeshTileDistanceDraw [0.0-...]\n"
		"Default is 200.0\n");

	REGISTER_CVAR2("ai_NavmeshStabilizationTimeToUpdate", &NavmeshStabilizationTimeToUpdate, 0.3f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Time that navmesh needs to be without any new updates to apply the latest changes.\n"
		"Usage: ai_NavmeshStabilizationTimeToUpdate [0.0-...]\n"
		"Default is 0.3\n");

	REGISTER_CVAR2("ai_MNMProfileMemory", &MNMProfileMemory, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Display navigation system memory statistics");

	REGISTER_CVAR2("ai_MNMDebugAccessibility", &MNMDebugAccessibility, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Display navigation unreachable areas in gray color.");

	REGISTER_CVAR2("ai_MNMEditorBackgroundUpdate", &MNMEditorBackgroundUpdate, 1, VF_NULL,
		"[0-1] Enable/Disable editor background update of the Navigation Meshes");

	REGISTER_CVAR2("ai_MNMAllowDynamicRegenInEditor", &MNMAllowDynamicRegenInEditor, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Allow dynamic regeneration of MNM when in the editor, when in game mode. Put this CVar in your configuration file rather than changing it during execution.");

	REGISTER_CVAR2("ai_MNMDebugDrawTileStates", &MNMDebugDrawTileStates, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Display tiles' borders based on their update state. \n"
		"orange - in active updates queue\n"
		"yellow - update after stabilization\n"
		"blue - changed during disabled regeneration \n"
		"gray - changed during level load");

	REGISTER_COMMAND("ai_debugMNMAgentType", DebugMNMAgentType, VF_NULL,
		"Enabled MNM debug draw for an specific agent type.\n"
		"Usage: ai_debugMNMAgentType [AgentTypeName]\n");

	REGISTER_COMMAND("ai_MNMCalculateAccessibility", MNMCalculateAccessibility, VF_NULL,
		"Calculate mesh reachability starting from designers placed MNM Seeds.\n");

	REGISTER_COMMAND("ai_MNMComputeConnectedIslands", MNMComputeConnectedIslands, VF_DEV_ONLY,
		"Computes connected islands on the mnm mesh.\n");

	REGISTER_COMMAND("ai_NavigationReloadConfig", NavigationReloadConfig, VF_DEV_ONLY,
		"Usage: ai_NavigationReloadConfig\n"
		"Reloads navigation config file. May lead to broken subsystems and weird bugs. For DEBUG purposes ONLY!\n");
}

void SAIConsoleVarsNavigation::DebugMNMAgentType(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() < 2)
	{
		AIWarning("<DebugMNMAgentType> Expecting Agent Type as parameter.");
		return;
	}

	const char* agentTypeName = args->GetArg(1);

	NavigationAgentTypeID agentTypeID = gAIEnv.pNavigationSystem->GetAgentTypeID(agentTypeName);
	gAIEnv.pNavigationSystem->SetDebugDisplayAgentType(agentTypeID);
}

void SAIConsoleVarsNavigation::MNMCalculateAccessibility(IConsoleCmdArgs* args)
{
	gAIEnv.pNavigationSystem->CalculateAccessibility();
}

void SAIConsoleVarsNavigation::MNMComputeConnectedIslands(IConsoleCmdArgs* args)
{
	gAIEnv.pNavigationSystem->ComputeAllIslands();
}

void SAIConsoleVarsNavigation::NavigationReloadConfig(IConsoleCmdArgs* args)
{
	if (INavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem)
	{
		// TODO pavloi 2016.03.09: hack implementation. See comments in ReloadConfig.
		if (pNavigationSystem->ReloadConfig())
		{
			pNavigationSystem->ClearAndNotify();
		}
		else
		{
			AIWarning("Errors during config reloading");
		}
		return;
	}
	AIWarning("Unable to obtain navigation system to reload config.");
}