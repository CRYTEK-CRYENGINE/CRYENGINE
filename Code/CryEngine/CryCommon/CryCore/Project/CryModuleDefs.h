// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#define eCryM_3DEngine 1
#define eCryM_GameFramework 2
#define eCryM_AISystem 3
#define eCryM_Animation 4
#define eCryM_DynamicResponseSystem 5
#define eCryM_EntitySystem 6
#define eCryM_Font 7
#define eCryM_Input 8
#define eCryM_Movie 9
#define eCryM_Network 10
// CryLobby is now an engine plug-in
//#define eCryM_Lobby 11
#define eCryM_Physics 12
#define eCryM_ScriptSystem 13
#define eCryM_AudioSystem 14
#define eCryM_System 15
#define eCryM_LegacyGameDLL 16
#define eCryM_Render 17
#define eCryM_Launcher 18
#define eCryM_Editor 19
#define eCryM_LiveCreate 20
// CryOnline has been removed from the engine build for now
//#define eCryM_Online 21
#define eCryM_AudioImplementation 22
#define eCryM_MonoBridge 23
#define eCryM_ScaleformHelper 24
#define eCryM_FlowGraph 25
//! Legacy module, does not need a specific name and is only kept for backwards compatibility
#define eCryM_Legacy 26

#define eCryM_EnginePlugin 27
#define eCryM_EditorPlugin 28
#define eCryM_Schematyc2 29

#define eCryM_Num 30

static const wchar_t* g_moduleNames[] =
{
	L"",
	L"Cry3DEngine",
	L"CryAction",
	L"CryAISystem",
	L"CryAnimation",
	L"CryDynamicResponseSystem",
	L"CryEntitySystem",
	L"CryFont",
	L"CryInput",
	L"CryMovie",
	L"CryNetwork",
	L"CryLobby",
	L"CryPhysics",
	L"CryScriptSystem",
	L"CryAudioSystem",
	L"CrySystem",
	L"CryGame",
	L"CryRenderer",
	L"Launcher",
	L"Sandbox",
	L"CryLiveCreate",
	L"CryOnline",
	L"CryAudioImplementation",
	L"CryMonoBridge",
	L"CryScaleformHelper",
	L"CryFlowGraph",
	L"Legacy Module",
	L"Engine Plug-ins",
	L"Editor Plug-ins",
	L"Schematyc2"
};