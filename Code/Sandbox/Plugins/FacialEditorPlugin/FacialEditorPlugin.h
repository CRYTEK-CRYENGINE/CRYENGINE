// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IPlugin.h"

class CViconClient;
class CFacialEditorPlugin : public IPlugin, public IAutoEditorNotifyListener
{
public:
	CFacialEditorPlugin();
	~CFacialEditorPlugin() { /* exit point of the plugin, perform cleanup */ }

	int32       GetPluginVersion() { return 1; };
	const char* GetPluginName() { return "Sample Plugin"; };
	const char* GetPluginDescription() { return "Plugin used as a code sample to demonstrate Sandbox's plugin system"; };

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	static CFacialEditorPlugin* GetInstance();
private:
	static CFacialEditorPlugin* s_instance;

#ifndef DISABLE_VICON
public:
	CViconClient* GetViconClient() { return m_ViconClient; }

private:
	float             m_fViconScale;
	float             m_fViconBend;
	CViconClient*     m_ViconClient;
#endif
};
