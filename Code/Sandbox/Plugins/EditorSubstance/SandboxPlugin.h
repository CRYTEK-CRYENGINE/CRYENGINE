// Copyright 2001-2015 Crytek GmbH. All rights reserved.
// Sandbox plugin wrapper.
#pragma once
#include "IEditor.h"
#include "IPlugin.h"

// Base class for plugin
class CSubstancePlugin : public IPlugin
{
public:
	static CSubstancePlugin* GetInstance();
	explicit CSubstancePlugin();
	~CSubstancePlugin();
	virtual const char* GetPluginName() override;
	virtual const char* GetPluginDescription() override;
	virtual int32       GetPluginVersion() override;
private:

};

