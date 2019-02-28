// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AssetManager.h"
#include "Cryasset.h"
#include "IResCompiler.h"
#include "IRCLog.h"
#include "FileUtil.h"
#include "CryGUIDHelper.h"
#include "StringHelpers.h"
#include <CryString/CryString.h>

namespace
{

class CDictionary
{
public:
	CDictionary(string keyValuePairs)
	{
		std::vector<string> pairs;
		std::vector<string> type;
		StringHelpers::Split(keyValuePairs, ";", false, pairs);
		types.reserve(pairs.size());
		for (const string& pair : pairs)
		{
			type.resize(0);
			StringHelpers::Split(pair, ",", false, type);
			if (type.size() == 2)
			{
				types.emplace_back(type[0], type[1]);
			}
		}

		std::sort(types.begin(), types.end(), [](const auto& a, const auto& b)
		{
			return stricmp(a.first.c_str(), b.first.c_str()) < 0;
		});
	}

	const char* GetValue(const char* szKey, const char* szNotFoundValue) const
	{
		auto it = std::lower_bound(types.begin(), types.end(), szKey, [](const auto& a, const auto& b)
		{
			return stricmp(a.first.c_str(), b) < 0;
		});

		return ((it != types.end()) && (stricmp(szKey, (*it).first) == 0)) ? (*it).second.c_str() : szNotFoundValue;
	}

private:
	std::vector<std::pair<string, string>> types;
};

string GetMetadataType(const char* szExt, const IConfig* pConfig)
{
	const CDictionary assetTypes(pConfig->GetAsString("AssetTypes", "", ""));
	return assetTypes.GetValue(szExt, szExt);
}

void UpdateFiles(AssetManager::SAssetMetadata& metadata, IResourceCompiler* const pRc, const IConfig* const pConfig, const string& sourceFilepath, const std::vector<string>& files)
{
	assert(files.size());

	using namespace AssetManager;

	if (metadata.guid == CryGUID::Null())
	{
		metadata.guid = CryGUIDHelper::Create();
	}

	const char* szExt = PathUtil::GetExt(files.begin()->c_str());
	metadata.type = GetMetadataType(szExt, pConfig);
	metadata.files.clear();
	metadata.files.reserve(files.size());
	for (const string& path : files)
	{
		metadata.files.emplace_back(PathUtil::GetFile(path));
	}

	const CDictionary userValues(pConfig->GetAsString("cryasset", "", ""));
	const char* szSource = userValues.GetValue("source", nullptr);
	if (szSource)
	{
		metadata.source = szSource;
	}
	else if (!sourceFilepath.empty() && (sourceFilepath != files[0]))
	{
		metadata.source = PathUtil::GetFile(sourceFilepath);
	}

	const string parentAsset = IAssetManager::GetMetadataFilename(sourceFilepath);

	// Keep in metadata of intermediate steps for multi-step import.
	if (FileUtil::FileExists(parentAsset))
	{
		AssetManager::SAssetMetadata parentMetadata;
		auto ancestorXml = pRc->LoadXml(parentAsset);
		AssetManager::ReadMetadata(ancestorXml, parentMetadata);

		if (!parentMetadata.source.empty())
		{
			// We want to keep the original source for a chain of transformations like ".tif" -> .$dds -> ... -> ".dds". 
			metadata.source = parentMetadata.source;
		}

		// Maintain others necessary fields (if any) here.
	}
}

void UpdateFiles(AssetManager::CAsset* pAsset, IResourceCompiler* const pRc, const IConfig* const pConfig, const string& sourceFilepath, const std::vector<string>& files)
{
	AssetManager::SAssetMetadata metadata;
	pAsset->GetMetadata(metadata);
	UpdateFiles(metadata, pRc, pConfig, sourceFilepath, files);
	pAsset->SetMetadata(metadata);
}

}

CAssetManager::CAssetManager(IResourceCompiler* pRc) 
	: m_pRc(pRc)
{
}

void CAssetManager::RegisterDetailProvider(FnDetailsProvider detailsProvider, const char* szExt)
{
	m_providers.insert(std::make_pair(szExt, detailsProvider));
}

bool CAssetManager::SaveCryasset(const IConfig* const pConfig, const char* szSourceFilepath, size_t filesCount, const char** pFiles, const char* szOutputFolder) const
{
	assert(szSourceFilepath);
	assert(filesCount);
	assert(pFiles);

	using namespace AssetManager;

	if (pConfig->GetAsBool("stripMetadata", false, true))
	{
		return true;
	}

	std::vector<string> files;
	files.reserve(filesCount);
	for (size_t i = 0; i < filesCount; ++i)
	{
		files.emplace_back(pFiles[i]);
	}

	const string metadataFilename = GetMetadataFilename(files.front());
	std::unique_ptr<CAsset> pAsset = CAsset::Create(m_pRc, metadataFilename);
	if (!pAsset)
	{
		return false;
	}

	UpdateFiles(pAsset.get(), m_pRc, pConfig, szSourceFilepath, files);

	if (!CollectMetadataDetails(pAsset->GetMetadataRoot(), files))
	{
		return false;
	}

	const string targetFile = szOutputFolder ? PathUtil::Make(szOutputFolder, PathUtil::GetFile(metadataFilename)) : metadataFilename;
	if (!pAsset->Save(targetFile))
	{
		RCLogError("Can't write metadata file '%s'", targetFile.c_str());
		return false;
	}
	m_pRc->AddInputOutputFilePair(szSourceFilepath ? files.front() : szSourceFilepath, targetFile.c_str());
	return true;
}

bool CAssetManager::CollectMetadataDetails(XmlNodeRef& xml, const std::vector<string>& files) const
{
	for (const string& filename : files)
	{
		const string ext = string(PathUtil::GetExt(filename)).MakeLower();
		auto detailsProvider = m_providers.find(ext);
		if (detailsProvider == m_providers.end())
		{
			continue;
		}

		if (!detailsProvider->second(xml, filename, m_pRc))
		{
			RCLogWarning("Can't collect detail information for the file '%s'", filename.c_str());
		}

		// For the moment we get details only for the main data file (the first one).
		break;
	}
	return true;
}
