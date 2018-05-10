// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IPlugin.h>

class CSchematycPlugin : public IPlugin
{
public:

	CSchematycPlugin();
	~CSchematycPlugin();

	// IPlugin
	int32       GetPluginVersion() override;
	const char* GetPluginName() override;
	const char* GetPluginDescription() override;
	// ~IPlugin
};

