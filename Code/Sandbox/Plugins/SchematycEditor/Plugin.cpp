// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Plugin.h"

#include <CryCore/Platform/platform_impl.inl>

#include "IResourceSelectorHost.h"

#include <CrySchematyc2/Schematyc_ICompiler.h>
#include <CrySchematyc2/Schematyc_IFramework.h>
#include <CrySchematyc2/Script/Schematyc_IScriptElement.h>
#include <CrySchematyc2/Script/Schematyc_IScriptRegistry.h>

const int   g_pluginVersion = 1;
const char* g_szPluginName  = "Schematyc Plugin";
const char* g_szPluginDesc  = "Schematyc Editor integration";

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

REGISTER_PLUGIN(CSchematycPlugin);

CSchematycPlugin::CSchematycPlugin()
{	
	gEnv->pSchematyc2->GetScriptRegistry().Load();	
	gEnv->pSchematyc2->GetCompiler().CompileAll();	
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


