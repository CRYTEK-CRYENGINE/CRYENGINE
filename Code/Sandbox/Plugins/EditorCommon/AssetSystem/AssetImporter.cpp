// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetImporter.h"

#include "Asset.h"
#include "AssetImportContext.h"
#include "AssetType.h"
#include "FilePathUtil.h"
#include <CryString/CryPath.h>

#include <ThreadingUtils.h>

ESystemClassID CAssetImporter::SystemClassID()
{
	return ESYSTEM_CLASS_ASSET_IMPORTER;
}

std::vector<CAsset*> CAssetImporter::Import(const std::vector<string>& assetTypes, CAssetImportContext& ctx)
{
	std::vector<string> potentialAssetPaths = GetAssetNames(assetTypes, ctx);

	potentialAssetPaths.erase(std::remove_if(potentialAssetPaths.begin(), potentialAssetPaths.end(), [&ctx](const string& assetPath)
	{
		const string absAssetPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), assetPath);
		return !ctx.CanWrite(absAssetPath);
	}), potentialAssetPaths.end());
	if (potentialAssetPaths.empty())
	{
		return {};
	}

	std::vector<CAsset*> assets = ImportAssets(potentialAssetPaths, ctx);

	// Remove possible nullptr assets.
	assets.erase(std::remove(assets.begin(), assets.end(), nullptr), assets.end());
	return assets;
}

void CAssetImporter::Reimport(CAsset* pAsset)
{
	ReimportAsset(pAsset);
}

// The default implementation of re-importing just does a fresh import again, so any user settings
// are discarded. Importers should preserve these settings.
void CAssetImporter::ReimportAsset(CAsset* pAsset)
{
	CRY_ASSERT(pAsset);

	const string absInputFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetSourceFile());
	const string outputDirectory = PathUtil::GetPathWithoutFilename(pAsset->GetMetadataFile());
	const string outputName = PathUtil::GetFileName(pAsset->GetMetadataFile());

	CAssetImportContext ctx;
	ctx.SetInputFilePath(absInputFilePath);
	ctx.SetOutputDirectoryPath(outputDirectory);
	ctx.OverrideAssetName(outputName);

	Import({ pAsset->GetType()->GetTypeName() }, ctx);

	// NOTE: The file monitor catches changes to the asset and merges them.
	// If we cannot assume this anymore, CAssetManager::MergeAssets() should be called at the end
	// of this method.
}

