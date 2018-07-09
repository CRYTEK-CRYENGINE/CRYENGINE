// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IEditor.h>
#include <IPlugin.h>

typedef std::vector<string> TSignalList;

class CEditorDynamicResponseSystemPlugin : public IPlugin
{
public:
	CEditorDynamicResponseSystemPlugin();

	virtual int32       GetPluginVersion() override                          { return 1; }
	virtual const char* GetPluginName() override                             { return "Dynamic Response System Editor"; }
	virtual const char* GetPluginDescription() override						 { return ""; }
};

