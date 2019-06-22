// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <IPlugin.h>

#include <CryCore/Platform/platform_impl.inl>

class CEnvironmentEditorPlugin : public IPlugin
{
private:
	int32       GetPluginVersion() override     { return 1; }
	const char* GetPluginName() override        { return "Environment Editor"; }
	const char* GetPluginDescription() override { return ""; }
};

REGISTER_PLUGIN(CEnvironmentEditorPlugin);
