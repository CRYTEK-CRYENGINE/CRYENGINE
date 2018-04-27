// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/Platform/platform_impl.inl>
#include "MainEditorWindow.h"
#include "QT/QToolTabManager.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/AssetType.h>

namespace
{

std::shared_ptr<pfx2::IParticleSystem> g_pParticleSystem;

}

pfx2::IParticleSystem* GetParticleSystem()
{
	return g_pParticleSystem.get();
}

void PyShowEffect(const char* szName)
{
	const string metadata = string().Format("%s.cryasset", szName);
	CAssetManager* const pAssetManager = CAssetManager::GetInstance();
	if (pAssetManager)
	{
		CAsset* const pAsset = pAssetManager->FindAssetForMetadata(metadata);
		if (pAsset)
		{
			pAsset->Edit();
		}
	}
}

DECLARE_PYTHON_MODULE(particle);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyShowEffect, particle, show_effect,
                                     "Show a specific particle effect in the Particle Editor.",
                                     "particle.show_effect(str effectName)");

namespace CryParticleEditor
{
	REGISTER_VIEWPANE_FACTORY(CParticleEditor, "Particle Editor", "Tools", false)
}

class CParticleEditorPlugin : public IPlugin
{
public:
	CParticleEditorPlugin()
	{
		g_pParticleSystem = pfx2::GetIParticleSystem();
	}


	// IPlugin
	int32       GetPluginVersion()                        { return 0x01; }
	const char* GetPluginName()                           { return "Particle Editor"; }
	const char* GetPluginDescription()					  { return "PFX Particle Editor plugin"; }
	// ~IPlugin
};

REGISTER_PLUGIN(CParticleEditorPlugin)

