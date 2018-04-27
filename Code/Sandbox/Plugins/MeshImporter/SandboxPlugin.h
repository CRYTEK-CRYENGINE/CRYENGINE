// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Sandbox plugin wrapper.
#pragma once
#include "IEditor.h"
#include "IPlugin.h"

// Base class for plugin
class CFbxToolPlugin : public IPlugin
{
public:
	static CFbxToolPlugin* GetInstance();

	explicit CFbxToolPlugin();
	~CFbxToolPlugin();

	void SetPersonalizationProperty(const QString& propName, const QVariant& value);
	const QVariant& GetPersonalizationProperty(const QString& propName);

	virtual const char* GetPluginName() override;
	virtual const char* GetPluginDescription() override;
	virtual int32       GetPluginVersion() override;

private:
};

