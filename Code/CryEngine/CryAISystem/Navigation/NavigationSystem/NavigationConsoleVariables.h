// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsNavigation
{
	void Init();

	static void DebugMNMAgentType(IConsoleCmdArgs* args);
	static void MNMCalculateAccessibility(IConsoleCmdArgs* args); // TODO: Remove when the seeds work
	static void MNMComputeConnectedIslands(IConsoleCmdArgs* args);
	static void NavigationReloadConfig(IConsoleCmdArgs* args);

	DeclareConstIntCVar(DebugDrawNavigation, 0);
	DeclareConstIntCVar(DebugTriangleOnCursor, 0);
	DeclareConstIntCVar(NavigationSystemMT, 1);
	DeclareConstIntCVar(NavGenThreadJobs, 1);
	DeclareConstIntCVar(DebugDrawNavigationWorldMonitor, 0);
	DeclareConstIntCVar(MNMRaycastImplementation, 2);
	DeclareConstIntCVar(MNMRemoveInaccessibleTrianglesOnLoad, 1);

	DeclareConstIntCVar(StoreNavigationQueriesHistory, 0);
	DeclareConstIntCVar(DebugDrawNavigationQueriesUDR, 0);
	DeclareConstIntCVar(DebugDrawNavigationQueriesList, 0);

	const char* MNMDebugDrawFlag;
	const char* DebugDrawNavigationQueryListFilterByCaller;
	float       NavmeshTileDistanceDraw;
	float       NavmeshStabilizationTimeToUpdate;
	int         MNMProfileMemory;
	int         MNMDebugAccessibility;
	int         MNMEditorBackgroundUpdate;
	int         MNMAllowDynamicRegenInEditor;
	int         MNMDebugDrawTileStates;
};