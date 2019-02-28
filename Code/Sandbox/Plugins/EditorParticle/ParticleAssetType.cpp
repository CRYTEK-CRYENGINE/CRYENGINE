// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "ParticleAssetType.h"

#include <AssetSystem/Loader/AssetLoaderHelpers.h>
#include <AssetSystem/EditableAsset.h>
#include <FileDialogs/EngineFileDialog.h>
#include <FilePathUtil.h>

#include <CryParticleSystem/IParticlesPfx2.h>

#include <CrySerialization/IArchive.h>
#include <CrySerialization/IArchiveHost.h>

#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

// #TODO Move this somewhere else.
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.inl>
#include <CryCore/ToolsHelpers/SettingsManagerHelpers.inl>
#include <CryCore/ToolsHelpers/EngineSettingsManager.inl>

REGISTER_ASSET_TYPE(CParticlesType)

namespace Private_ParticleAssetType
{

static string ShowSaveDialog()
{
	CEngineFileDialog::RunParams runParams;
	runParams.title = QStringLiteral("Save Particle Effects");
	runParams.extensionFilters << CExtensionFilter(QStringLiteral("Particle Effects (pfx)"), "pfx");

	const QString filePath = CEngineFileDialog::RunGameSave(runParams, nullptr);
	return QtUtil::ToString(filePath);
}

static bool MakeNewComponent(pfx2::IParticleEffectPfx2* pEffect)
{
	pfx2::IParticleComponent* const pComp = pEffect->AddComponent();
	if (!pComp)
	{
		return false;
	}

	const string templateName = "%ENGINE%/EngineAssets/Particles/Default.pfxp";

	return Serialization::LoadJsonFile(*pComp, templateName);
}

static string CreateAssetMetaData(const string& pfxFilePath)
{
	const string options = "/refresh /overwriteextension=cryasset";
	const CResourceCompilerHelper::ERcExePath path = CResourceCompilerHelper::eRcExePath_editor;
	const auto result = CResourceCompilerHelper::CallResourceCompiler(
	  pfxFilePath.c_str(),
	  options.c_str(),
	  nullptr,
	  false, // may show window?
	  path,
	  true,  // silent?
	  true); // no user dialog?
	if (result != CResourceCompilerHelper::eRcCallResult_success)
	{
		return string();
	}
	return pfxFilePath + ".cryasset";
}

} // namespace Private_ParticleAssetType

struct CParticlesType::SCreateParams
{
	bool bUseExistingEffect;

	SCreateParams()
		: bUseExistingEffect(false)
	{
	}
};

CryIcon CParticlesType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_particles.ico");
}

CAssetEditor* CParticlesType::Edit(CAsset* asset) const
{
	return CAssetEditor::OpenAssetForEdit("Particle Editor", asset);
}

bool CParticlesType::CreateForExistingEffect(const char* szFilePath) const
{
	SCreateParams params;
	params.bUseExistingEffect = true;
	return Create(szFilePath, &params);
}

bool CParticlesType::OnCreate(CEditableAsset& editAsset, const void* pCreateParams) const
{
	using namespace Private_ParticleAssetType;

	const string basePath = PathUtil::RemoveExtension(PathUtil::RemoveExtension(editAsset.GetAsset().GetMetadataFile()));

	const string pfxFilePath = basePath + ".pfx";

	std::shared_ptr<pfx2::IParticleSystem> pParticleSystem = pfx2::GetIParticleSystem();
	if (!pParticleSystem)
	{
		return false;
	}

	const bool bCreatePfxFile = !(pCreateParams && ((SCreateParams*)pCreateParams)->bUseExistingEffect);

	if (bCreatePfxFile)
	{
		pfx2::PParticleEffect pEffect = pParticleSystem->CreateEffect();
		pParticleSystem->RenameEffect(pEffect, pfxFilePath.c_str());
		MakeNewComponent(pEffect.get());

		if (!Serialization::SaveJsonFile(pfxFilePath.c_str(), *pEffect))
		{
			return false;
		}
	}

	editAsset.SetFiles("", { pfxFilePath });

	return true;
}

