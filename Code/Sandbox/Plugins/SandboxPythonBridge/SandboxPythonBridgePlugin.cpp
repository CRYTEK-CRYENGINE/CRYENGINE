// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SandboxPythonBridgePlugin.h"

#include <CryCore/Platform/platform_impl.inl>

REGISTER_PLUGIN(SandboxPythonBridgePlugin)

SandboxPythonBridgePlugin* g_pPlugin = nullptr;

SandboxPythonBridgePlugin* GetSandboxPythonBridge()
{
	return g_pPlugin;
}

namespace PluginInfo
{
	const char* kName = "Sandbox Python Bridge";
	const char* kDesc = "Enables usage of Python plugins";
	const int kVersion = 0;
}

SandboxPythonBridgePlugin::SandboxPythonBridgePlugin()
{
}


SandboxPythonBridgePlugin::~SandboxPythonBridgePlugin()
{
}

int32 SandboxPythonBridgePlugin::GetPluginVersion()
{
	return PluginInfo::kVersion;
}

const char* SandboxPythonBridgePlugin::GetPluginName()
{
	return PluginInfo::kName;
}

const char* SandboxPythonBridgePlugin::GetPluginDescription()
{
	return PluginInfo::kDesc;
}


