// Copyright 2001-2018 Crytek GmbH. All rights reserved.
#include "StdAfx.h"
#include "AssetFilesGroupController.h"

#include "Asset.h"
#include "AssetManager.h"
#include "AssetSystem/Loader/AssetLoaderHelpers.h"
#include "FileUtils.h"
#include "PathUtils.h"

CAssetFilesGroupController::CAssetFilesGroupController(CAsset* pAsset, bool shouldIncludeSourceFile) 
	: m_pAsset(pAsset)
	, m_metadata(pAsset->GetMetadataFile())
	, m_name(pAsset->GetName())
	, m_shouldIncludeSourceFile(shouldIncludeSourceFile)
{}

std::vector<string> CAssetFilesGroupController::GetFiles(bool includeGeneratedFile /*= true*/) const
{
	std::vector<string> result;
	if (m_pAsset)
	{
		result = m_pAsset->GetType()->GetAssetFiles(*m_pAsset, m_shouldIncludeSourceFile, false, includeGeneratedFile);

		if (!result.empty() && result.back() != m_pAsset->GetMetadataFile())
		{
			for (auto i = 0; i < result.size(); ++i)
			{
				if (result[i] == m_pAsset->GetMetadataFile())
				{
					result[i] = result.back();
					result[result.size() - 1] = m_pAsset->GetMetadataFile();
				}
			}
		}
	}

	return result;
}

string CAssetFilesGroupController::GetGeneratedFile() const
{
	return !m_pAsset || !m_pAsset->GetType()->HasThumbnail() ? "" : m_pAsset->GetThumbnailPath();
}

void CAssetFilesGroupController::Update()
{
	if (FileUtils::FileExists(PathUtil::Make(PathUtil::GetGameProjectAssetsRelativePath(), m_metadata)))
	{
		auto pAssetManager = CAssetManager::GetInstance();
		CryLog("Executing reloading of asset %s", m_metadata.c_str());
		pAssetManager->MergeAssets(AssetLoader::CAssetFactory::LoadAssetsFromMetadataFiles({ m_metadata }));
		m_pAsset = pAssetManager->FindAssetForMetadata(m_metadata);
	}
	else
	{
		CryLog("No reloading of asset %s because the file doesn't exist.", m_metadata.c_str());
		m_pAsset = nullptr;
	}
}
