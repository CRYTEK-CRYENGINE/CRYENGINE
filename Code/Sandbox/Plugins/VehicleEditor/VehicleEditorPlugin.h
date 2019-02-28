// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IPlugin.h"

class CVehicleEditorPlugin : public IPlugin
{
public:
	CVehicleEditorPlugin() { /* entry point of the plugin, perform initializations */ }
	~CVehicleEditorPlugin() { /* exit point of the plugin, perform cleanup */ }

	int32       GetPluginVersion() { return 1; };
	const char* GetPluginName() { return "Sample Plugin"; };
	const char* GetPluginDescription() { return "Plugin used as a code sample to demonstrate Sandbox's plugin system"; };

private:
};
