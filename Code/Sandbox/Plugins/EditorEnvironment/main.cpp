// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform_impl.inl>

#include <IEditor.h>
#include <IPlugin.h>

#include "QtViewPane.h"
#include "EditorEnvironmentWindow.h"

class CEditorEnvironment : public IPlugin
{
public:
	CEditorEnvironment()
	{
	}

	int32       GetPluginVersion() override                          { return 1; }
	const char* GetPluginName() override                             { return "Environment Editor"; }
	const char* GetPluginDescription() override						 { return ""; }
};

REGISTER_PLUGIN(CEditorEnvironment);

