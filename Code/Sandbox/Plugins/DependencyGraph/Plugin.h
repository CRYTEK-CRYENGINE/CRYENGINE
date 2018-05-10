// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IPlugin.h"

class CDependencyGraph : public IPlugin
{
public:
	CDependencyGraph();
	~CDependencyGraph();

	int32       GetPluginVersion()     { return 1; };
	const char* GetPluginName()        { return "Dependency Graph Plugin"; };
	const char* GetPluginDescription() { return "Plugin provides a graph view of asset dependencies"; };

	void SetPersonalizationProperty(const QString& propName, const QVariant& value);
	const QVariant& GetPersonalizationProperty(const QString& propName);

	static CDependencyGraph* GetInstance();
private:
};

