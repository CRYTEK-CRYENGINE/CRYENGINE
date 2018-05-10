// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2012
// -------------------------------------------------------------------------
//  Created:     24 June 2011 by Sergiy Shaykin.
//  Description: Attachment to the Sandbox plug-in system
//
////////////////////////////////////////////////////////////////////////////
#include <IPlugin.h>

class CFBXPlugin : public IPlugin
{
public:
	CFBXPlugin();
	~CFBXPlugin();

	int32       GetPluginVersion() override;
	const char* GetPluginName() override;
	const char* GetPluginDescription() override;
};

