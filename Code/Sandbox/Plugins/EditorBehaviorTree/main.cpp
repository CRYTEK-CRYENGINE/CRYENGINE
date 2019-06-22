// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <IPlugin.h>

#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>

class BehaviorTreeEditorPlugin : public IPlugin
{
	enum
	{
		Version = 1,
	};

public:

	BehaviorTreeEditorPlugin() {}

	int         GetPluginVersion() override     { return Version; }
	const char* GetPluginName() override        { return "Behavior Tree Editor"; }
	const char* GetPluginDescription() override { return GetPluginName(); }
};

REGISTER_PLUGIN(BehaviorTreeEditorPlugin)
