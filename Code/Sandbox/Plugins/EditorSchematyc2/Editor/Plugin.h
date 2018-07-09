// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IPlugin.h>
#include <CrySandbox/CryInterop.h>

class CSchematycPlugin : public IPlugin,
	public IUriEventListener
{
public:

	CSchematycPlugin();
	~CSchematycPlugin();

	// IPlugin
	int GetPluginVersion() override;
	const char* GetPluginName() override;
	const char* GetPluginDescription() override;
	// ~IPlugin

	// IUriEventListener
	virtual void OnUriReceived(const char* szUri);
	// ~IUriEventListener

private:

	bool m_bRegistered = false;
};