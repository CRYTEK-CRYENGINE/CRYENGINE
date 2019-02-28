// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define SANDBOX_PLUGIN_SYSTEM_VERSION 1

#include "PluginAPI.h"
#include "ICommandManager.h"
#include "EditorFramework/TrayArea.h"
#include "EditorFramework/Preferences.h"
#include "QtViewPane.h"
#include "IEditorClassFactory.h"
#include "BoostPythonMacros.h"
#include "IResourceSelectorHost.h"

//! Interface for declaring an editor plugin
//! Register in a .cpp file using REGISTER_PLUGIN() macro
//! Use constructors and destructors as entry and exit point for your plugin's code.
struct IPlugin
{
	enum EError
	{
		eError_None            = 0,
		eError_VersionMismatch = 1
	};

	//! Returns the version number of the plugin
	virtual int32       GetPluginVersion() = 0;
	//! Returns the human readable name of the plugin
	virtual const char* GetPluginName() = 0;
	//! Returns the human readable description of the plugin
	virtual const char* GetPluginDescription() = 0;
};

#ifndef eCryModule
#define eCryModule eCryM_EditorPlugin
#endif

// Initialization structure
struct PLUGIN_INIT_PARAM
{
	IEditor* pIEditor;
	int             pluginVersion;
	IPlugin::EError outErrorCode;
};

// Factory API
extern "C"
{
	DLL_EXPORT IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM* pInitParam);
	DLL_EXPORT void DeletePluginInstance(IPlugin* pPlugin);
}

//Note: Use REGISTER_PLUGIN, do not invoke those directly.
//These methods are meant to facilitate programming in plugins and automate registration of components. 
//The idea is that you can use similar macros for registration relying on the CAutoRegister pattern such as REGISTER_CLASS_DESC.
//However because of this relying on static initialization, the objects in DLLs will reside in another static space and
//we will need to call the auto registration in every DLL space. This is what these methods provide.
//THESE METHODS MUST REMAIN INLINE FOR THIS TO WORK

inline void RegisterPlugin()
{
	CAutoRegisterClassHelper::RegisterAll();
	CAutoRegisterCommandHelper::RegisterAll();
	CAutoRegisterUiCommandHelper::RegisterAll();
	CAutoRegisterPythonCommandHelper::RegisterAll();
	CAutoRegisterPythonModuleHelper::RegisterAll();
	CAutoRegisterPreferencesHelper::RegisterAll();
	CAutoRegisterTrayAreaHelper::RegisterAll();
	CAutoRegisterResourceSelector::RegisterAll();
}

inline void UnregisterPlugin()
{
	CAutoRegisterClassHelper::UnregisterAll();
	CAutoRegisterCommandHelper::UnregisterAll();
	CAutoRegisterUiCommandHelper::UnregisterAll();
	CAutoRegisterPythonCommandHelper::UnregisterAll();
	CAutoRegisterPythonModuleHelper::UnregisterAll();
	CAutoRegisterPreferencesHelper::UnregisterAll();
	CAutoRegisterTrayAreaHelper::UnregisterAll();
	CAutoRegisterResourceSelector::UnregisterAll();
}

//Use this in a cpp file in your plugin to register it properly
#define REGISTER_PLUGIN(PluginClass)													\
static IEditor* g_pEditor = nullptr;                                                    \
IEditor* GetIEditor() { return g_pEditor; }                                             \
                                                                                        \
DLL_EXPORT IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM* pInitParam)                 \
{                                                                                       \
	if (pInitParam->pluginVersion != SANDBOX_PLUGIN_SYSTEM_VERSION)                     \
	{                                                                                   \
		pInitParam->outErrorCode = IPlugin::eError_VersionMismatch;                     \
		return 0;                                                                       \
	}                                                                                   \
                                                                                        \
	g_pEditor = pInitParam->pIEditor;													\
	ModuleInitISystem(g_pEditor->GetSystem(), #PluginClass);                            \
	PluginClass* pPlugin = new PluginClass();                                           \
                                                                                        \
	RegisterPlugin();                                                                   \
                                                                                        \
	return pPlugin;                                                                     \
}                                                                                       \
                                                                                        \
DLL_EXPORT void DeletePluginInstance(IPlugin* pPlugin)                                  \
{                                                                                       \
	UnregisterPlugin();                                                                 \
	delete pPlugin;                                                                     \
}                                                                                       \



