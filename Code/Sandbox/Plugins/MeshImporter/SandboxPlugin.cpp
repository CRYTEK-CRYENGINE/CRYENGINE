// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Sandbox plugin wrapper.
#include "StdAfx.h"
#include "SandboxPlugin.h"

#include <CryCore/Platform/platform_impl.inl>

#include "MainDialog.h"
#include "QtViewPane.h"
#include "IEditor.h"
#include <CrySystem/ISystem.h>
#include <EditorFramework/PersonalizationManager.h>

// Plugin versioning
static const char* g_szPluginName = "MeshImporter";
static const char* g_szPluginDesc = "FBX Importer tools for meshes, and animations";
static DWORD g_pluginVersion = 1;

// Plugin instance
static CFbxToolPlugin* g_pInstance = nullptr;

// Global lock instance
CryCriticalSection Detail::g_lock;


REGISTER_PLUGIN(CFbxToolPlugin);

class CEditorImpl;
CEditorImpl* GetIEditorImpl()
{
	return (CEditorImpl*)GetIEditor();
}

CFbxToolPlugin::CFbxToolPlugin()
{
	REGISTER_STRING("mi_defaultMaterial", "%ENGINE%/EngineAssets/TextureMsg/DefaultSolids", 0, "Default material.");
	REGISTER_INT("mi_lazyLodGeneration", 1, 0, "When non-zero, LOD auto-generation is deferred until LODs are actually visible.");
	REGISTER_FLOAT("mi_jointSize", 0.02f, 0, "Joint size");

	CScopedGlobalLock lock;
	assert(g_pInstance == nullptr);
	g_pInstance = this;
}

CFbxToolPlugin::~CFbxToolPlugin()
{
	CScopedGlobalLock lock;
	assert(g_pInstance == this);
	g_pInstance = nullptr;
}

CFbxToolPlugin* CFbxToolPlugin::GetInstance()
{
	return g_pInstance;
}

const char* CFbxToolPlugin::GetPluginName()
{
	return g_szPluginName;
}

const char* CFbxToolPlugin::GetPluginDescription()
{
	return g_szPluginDesc;
}

int32 CFbxToolPlugin::GetPluginVersion()
{
	return g_pluginVersion;
}

static void LogVPrintf(const char* szFormat, va_list args)
{
	string format;
	format.Format("[%s] %s", g_szPluginName, szFormat);
	gEnv->pLog->LogV(IMiniLog::eMessage, format.c_str(), args);
}

void LogPrintf(const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	LogVPrintf(szFormat, args);
	va_end(args);
}

namespace FbxTool
{
namespace Detail
{

void Log(const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	LogVPrintf(szFormat, args);
	va_end(args);
}

} // namespace Detail
} // namespace FbxTool

void CFbxToolPlugin::SetPersonalizationProperty(const QString& propName, const QVariant& value)
{
	GetIEditor()->GetPersonalizationManager()->SetProperty(MESH_IMPORTER_NAME, propName, value);
}

const QVariant& CFbxToolPlugin::GetPersonalizationProperty(const QString& propName)
{
	return GetIEditor()->GetPersonalizationManager()->GetProperty(MESH_IMPORTER_NAME, propName);
}


