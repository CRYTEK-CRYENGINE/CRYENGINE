// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Plugin.h"

#include <CryCore/Platform/platform_impl.inl>

#include <IResourceSelectorHost.h>
#include <QtViewPane.h>
#include <IEditorClassFactory.h>
#include <CrySchematyc2/IFramework.h>

#include "Editor/MainFrameWnd.h"
#include "Editor/MainWindow.h"
#include "Editor/Plugin.h"
#include "Editor/CryLink.h"

#include <CrySchematyc2/GUID.h>
#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>

namespace
{
	const int   PLUGIN_VERSION = 1;
	const char*	PLUGIN_NAME    = "Schematyc 2 Plug-in";

	Schematyc2::SGUID GenerateGUID()
	{
		Schematyc2::SGUID	guid;
#if defined(WIN32) || defined(WIN64)
		::CoCreateGuid(&guid.sysGUID);
#endif
		return guid;
	}
}

CSchematycPlugin::CSchematycPlugin()
{
	if(gEnv->pSchematyc2)
	{
		CRY_ASSERT_MESSAGE(GetSchematyc() != nullptr, "Schematyc must be available for editor plug-in to function!");

		Schematyc2::CMainFrameWnd::RegisterViewClass();
		Schematyc2::CCryLinkCommands::GetInstance().Register(GetIEditor()->GetSystem()->GetIConsole());


		// Hook up GUID generator then fix-up script files and resolve broken/deprecated dependencies.
		CryLogAlways("[SchematycLegacyEditor]: Initializing...");
		GetSchematyc()->SetGUIDGenerator(Schematyc2::GUIDGenerator::FromGlobalFunction<GenerateGUID>());
		CryLogAlways("[SchematycLegacyEditor]: Fixing up script files");
		GetSchematyc()->GetScriptRegistry().RefreshFiles(Schematyc2::SScriptRefreshParams(Schematyc2::EScriptRefreshReason::EditorFixUp));
		CryLogAlways("[SchematycLegacyEditor]: Compiling script files");
		GetSchematyc()->GetCompiler().CompileAll();
		CryLogAlways("[SchematycLegacyEditor]: Initialization complete");

		GetIEditor()->RegisterURIListener(this, "view");

		m_bRegistered = true;
	}
}

CSchematycPlugin::~CSchematycPlugin()
{
	if (m_bRegistered)
	{
		GetIEditor()->UnregisterURIListener(this, "view");
	}
}

int CSchematycPlugin::GetPluginVersion()
{
	return PLUGIN_VERSION;
}

const char* CSchematycPlugin::GetPluginName()
{
	return PLUGIN_NAME;
}

const char* CSchematycPlugin::GetPluginDescription()
{
	return PLUGIN_NAME;
}

void CSchematycPlugin::OnUriReceived(const char* szUri)
{
	CCryURI uri(szUri);
	GetIEditor()->OpenView(uri.GetAdress());
}

//REGISTER_PLUGIN(CSchematycPlugin);

// TODO: Temp solution for the case Schematyc plugin was not loaded.
static IEditor* g_pEditor = nullptr;
IEditor* GetIEditor() { return g_pEditor; }

DLL_EXPORT IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM* pInitParam)
{
	if (pInitParam->pluginVersion != SANDBOX_PLUGIN_SYSTEM_VERSION)
	{
		pInitParam->outErrorCode = IPlugin::eError_VersionMismatch;
		return nullptr;
	}

	g_pEditor = pInitParam->pIEditor;
	ModuleInitISystem(g_pEditor->GetSystem(), "CSchematycPlugin");
	if (gEnv->pSchematyc2 == nullptr)
	{
		return nullptr;
	}
	CSchematycPlugin* pPlugin = new CSchematycPlugin();

	RegisterPlugin();

	return pPlugin;
}

DLL_EXPORT void DeletePluginInstance(IPlugin* pPlugin)
{
	UnregisterPlugin();
	delete pPlugin;
}
// ~TODO
