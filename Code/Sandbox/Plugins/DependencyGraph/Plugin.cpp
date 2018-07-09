// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Plugin.h"
#include "Menu/AbstractMenu.h"
#include <AssetSystem/AssetManager.h>
#include <CryCore/Platform/platform_impl.inl>

// Plugin instance
static CDependencyGraph* g_pInstance = nullptr;

REGISTER_PLUGIN(CDependencyGraph);

CDependencyGraph::CDependencyGraph()
{
	CRY_ASSERT(g_pInstance == nullptr);
	g_pInstance = this;

	GetIEditor()->GetAssetManager()->signalContextMenuRequested.Connect([](CAbstractMenu& menu, const std::vector<CAsset*>& assets, std::shared_ptr<IUIContext> context)
	{
		if (assets.size() == 1)
		{
			auto action = menu.CreateAction(QObject::tr("Show Dependency Graph"), menu.FindSectionByName("Assets"));
			QObject::connect(action, &QAction::triggered, [asset = assets.front()]()
			{ 
				GetIEditor()->ExecuteCommand("asset.show_dependency_graph '%s'", asset->GetMetadataFile());
			});
		}
	}, (uintptr_t)this);
}

CDependencyGraph::~CDependencyGraph()
{
	GetIEditor()->GetAssetManager()->signalContextMenuRequested.DisconnectById((uintptr_t)this);
	CRY_ASSERT(g_pInstance == this);
	g_pInstance = nullptr;
}

CDependencyGraph* CDependencyGraph::GetInstance()
{
	return g_pInstance;
}

void CDependencyGraph::SetPersonalizationProperty(const QString& propName, const QVariant& value)
{
	GetIEditor()->GetPersonalizationManager()->SetProperty(GetPluginName(), propName, value);
}

const QVariant& CDependencyGraph::GetPersonalizationProperty(const QString& propName)
{
	return GetIEditor()->GetPersonalizationManager()->GetProperty(GetPluginName(), propName);
}

