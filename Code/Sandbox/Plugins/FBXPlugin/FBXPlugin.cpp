// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FBXPlugin.h"

#include <CryCore/Platform/platform_impl.inl>

#include "FBXExporter.h"

REGISTER_PLUGIN(CFBXPlugin);

namespace PluginInfo
{
const char* kName = "FBX Exporter";
const char* kDesc = "FBX Exporter plugin";
const int kVersion = 1;
}

CFBXPlugin::CFBXPlugin()
{
	IExportManager* pExportManager = GetIEditor()->GetExportManager();
	if (pExportManager)
	{
		pExportManager->RegisterExporter(new CFBXExporter());
	}
}

CFBXPlugin::~CFBXPlugin()
{
	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "CFBXPlugin cannot be unloaded as FBX exporter cannot be unregistered");
}

int32 CFBXPlugin::GetPluginVersion()
{
	return PluginInfo::kVersion;
}

const char* CFBXPlugin::GetPluginName()
{
	return PluginInfo::kName;
}

const char* CFBXPlugin::GetPluginDescription()
{
	return PluginInfo::kDesc;
}

