// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DialogEditorPlugin.h"
#include <CryCore/Platform/platform_impl.inl>


//TODO : make macro of dat shiz
/*
static IEditor* g_pEditor = nullptr;
IEditor* GetIEditor() { return g_pEditor; }

PLUGIN_API IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM* pInitParam)
{
	if (pInitParam->pluginVersion != SANDBOX_PLUGIN_SYSTEM_VERSION)
	{
		pInitParam->outErrorCode = IPlugin::eError_VersionMismatch;
		return 0;
	}

	IEditor* g_pEditor = pInitParam->pIEditor;
	ModuleInitISystem(g_pEditor->GetSystem(), "CDialogEditorPlugin");
	CDialogEditorPlugin* pPlugin = new CDialogEditorPlugin();

	RegisterPlugin();

	return pPlugin;
}

PLUGIN_API void DeletePluginInstance(IPlugin* pPlugin)
{
	UnregisterPlugin();
	delete pPlugin;
}
*/


