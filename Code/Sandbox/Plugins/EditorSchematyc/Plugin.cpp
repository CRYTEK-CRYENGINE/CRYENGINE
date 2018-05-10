// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Plugin.h"

#include <CryCore/Platform/platform_impl.inl>

#include "IResourceSelectorHost.h"

#include <CrySchematyc/Compiler/ICompiler.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/SerializationUtils/SerializationEnums.inl>
#include <CrySchematyc/Utils/GUID.h>

#include "CryLinkCommands.h"

const int g_pluginVersion = 1;
const char* g_szPluginName = "Schematyc Plugin";
const char* g_szPluginDesc = "Schematyc Editor integration";

CryGUID GenerateGUID()
{
	CryGUID guid;
#if CRY_PLATFORM_WINDOWS
	GUID winGuid;
	::CoCreateGuid(&winGuid);
	static_assert(sizeof(winGuid)==sizeof(guid),"GUID and CryGUID sizes should match.");
	memcpy(&guid,&winGuid,sizeof(guid));
#else
	guid = CryGUID::Create();
#endif
	return guid;
}

//REGISTER_PLUGIN(CSchematycPlugin);
	
// TODO: Temp solution for the case experimental Schematyc plugin was not loaded.
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
	if (gEnv->pSchematyc == nullptr)
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

CSchematycPlugin::CSchematycPlugin()
{
	// In case the Schematyc Core module hasn't been loaded we would crash here without this check.
	// This condition can be removed once Editor plugins are properly handled by the Plugin Manager.
	if (gEnv->pSchematyc != nullptr)
	{
		Schematyc::CCryLinkCommands::GetInstance().Register(g_pEditor->GetSystem()->GetIConsole());

		// Hook up GUID generator then fix-up script files and resolve broken/deprecated dependencies.
		CryLogAlways("[SchematycEditor]: Initializing...");
		gEnv->pSchematyc->SetGUIDGenerator(SCHEMATYC_DELEGATE(&GenerateGUID));
		CryLogAlways("[SchematycEditor]: Fixing up script files");
		gEnv->pSchematyc->GetScriptRegistry().ProcessEvent(Schematyc::SScriptEvent(Schematyc::EScriptEventId::EditorFixUp));
		CryLogAlways("[SchematycEditor]: Compiling script files");
		gEnv->pSchematyc->GetCompiler().CompileAll();
		CryLogAlways("[SchematycEditor]: Initialization complete");
	}
}

int32 CSchematycPlugin::GetPluginVersion()
{
	return g_pluginVersion;
}

const char* CSchematycPlugin::GetPluginName()
{
	return g_szPluginName;
}

const char* CSchematycPlugin::GetPluginDescription()
{
	return g_szPluginDesc;
}



