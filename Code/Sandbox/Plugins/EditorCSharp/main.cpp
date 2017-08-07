// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/Platform/platform_impl.inl>
#include "QT/QToolTabManager.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/AssetType.h>
#include <CrySystem/File/IFileChangeMonitor.h>
#include <CryMono/IMonoRuntime.h>

class CCSharpEditorPlugin final
	: public IPlugin
	, public IFileChangeListener
{
public:
	// IPlugin
	int32       GetPluginVersion() override{ return 1; }
	const char* GetPluginName() override { return "CSharp Editor"; }
	const char* GetPluginDescription() override{ return "CSharp Editor plugin"; }
	// ~IPlugin

	CCSharpEditorPlugin()
	{
		GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "cs");
	}

	virtual ~CCSharpEditorPlugin()
	{
		GetIEditor()->GetFileMonitor()->UnregisterListener(this);
	}

	virtual void OnFileChange(const char* sFilename, EChangeType eType) override
	{
		if (gEnv->pMonoRuntime != nullptr)
		{
			gEnv->pMonoRuntime->ReloadPluginDomain();
		}
	}
};

REGISTER_PLUGIN(CCSharpEditorPlugin)
