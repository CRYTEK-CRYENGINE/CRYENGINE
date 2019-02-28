// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetDropHandler.h"
#include "AssetImportDialog.h"
#include <AssetSystem/AssetImportContext.h>
#include <AssetSystem/AssetType.h>
#include <AssetSystem/AssetImporter.h>
#include <AssetSystem/AssetManager.h>

#include <Notifications/NotificationCenter.h>
#include <ThreadingUtils.h>

#include <QtUtil.h>

#include <CrySandbox/ScopedVariableSetter.h>
#include <CryString/StringUtils.h>
#include <CryString/CryPath.h>

namespace Private_AssetDropHandler
{

string UnifyExt(const string& ext)
{
	string e = ext;
	e.MakeLower();
	if (e[0] == '.')
	{
		e = e.substr(1);
	}
	return e;
}

std::vector<string> ToStringVector(const QStringList& strings)
{
	std::vector<string> v;
	v.reserve(strings.size());
	for (const QString& s : strings)
	{
		v.push_back(QtUtil::ToString(s));
	}
	return v;
}

bool CanImportExtension(const CAssetImporter* pAssetImporter, const string& ext)
{
	const std::vector<string> exts = pAssetImporter->GetFileExtensions();
	return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

} // namespace Private_AssetDropHandler

struct CAssetDropHandler::SImportParamsEx : CAssetDropHandler::SImportParams
{
	explicit SImportParamsEx(const SImportParams& importParams)
		: SImportParams(importParams)
	{
	}

	std::function<void(const string&)> beginImportingFile;
	std::function<void()> endImportingFile;
};

CAssetDropHandler::CAssetDropHandler()
{}

bool CAssetDropHandler::CanImportAny(const std::vector<string>& filePaths)
{
	using namespace Private_AssetDropHandler;

	for (const CAssetImporter* pAssetImporter : CAssetManager::GetInstance()->GetAssetImporters())
	{
		for (const string& filePath : filePaths)
		{
			const string ext = string(PathUtil::GetExt(filePath)).MakeLower();
			if (CanImportExtension(pAssetImporter, ext))
			{
				return true;
			}
		}
	}

	return false;
}

bool CAssetDropHandler::CanImportAny(const QStringList& filePaths)
{
	using namespace Private_AssetDropHandler;
	return CanImportAny(ToStringVector(filePaths));
}

std::vector<CAsset*> CAssetDropHandler::Import(const std::vector<string>& filePaths, const SImportParams& importParams)
{
	using namespace Private_AssetDropHandler;

	SImportParamsEx importParamsEx(importParams);

	struct SShared
	{
		CProgressNotification notif;
		float fileProgress;

		SShared()
			: notif("Import asset", "", true)
			, fileProgress(0.0f)
		{}
	};

	auto pShared = ThreadingUtils::PostOnMainThread([]()
	{
		return std::make_shared<SShared>();
	}).get();

	const float inc = 1.0f / filePaths.size();
	float fileProgress = 0.0f;

	importParamsEx.beginImportingFile = [pShared](const string& filename)
	{
		ThreadingUtils::PostOnMainThread([pShared, filename]()
		{
			pShared->notif.SetMessage(QObject::tr("File %1").arg(PathUtil::ToUnixPath(filename).c_str()));
		});
	};

	importParamsEx.endImportingFile = [pShared, inc]()
	{
		ThreadingUtils::PostOnMainThread([pShared, inc]()
		{
			pShared->fileProgress += inc;
			pShared->notif.SetProgress(std::min(0.99f, pShared->fileProgress));
		});
	};

	std::vector<std::pair<size_t, string>> sortedExt;
	sortedExt.reserve(filePaths.size());
	for (size_t i = 0; i < filePaths.size(); ++i)
	{
		const string ext = string(PathUtil::GetExt(filePaths[i])).MakeLower();
		sortedExt.push_back(std::make_pair(i, ext));
	}

	const auto compareExt = [](const std::pair<size_t, const char*>& lhp, const std::pair<size_t, const char*>& rhp)
	{
		return 0 > strcmp(lhp.second, rhp.second);
	};
	std::sort(sortedExt.begin(), sortedExt.end(), compareExt);

	std::vector<CAsset*> assets;

	std::vector<string> filePathBatch;
	while (!sortedExt.empty())
	{
		filePathBatch.clear();
		const string batchExt = sortedExt.back().second;

		while (!sortedExt.empty() && sortedExt.back().second == batchExt)
		{
			filePathBatch.push_back(filePaths[sortedExt.back().first]);
			sortedExt.pop_back();
		}

		std::vector<CAsset*> batchAssets = ImportExt(batchExt, filePathBatch, importParamsEx);
		assets.insert(assets.end(), batchAssets.begin(), batchAssets.end());
	}

	return assets;
}

std::vector<CAsset*> CAssetDropHandler::Import(const QStringList& filePaths, const SImportParams& importParams)
{
	using namespace Private_AssetDropHandler;
	return Import(ToStringVector(filePaths), importParams);
}

//! Precondition: pAssetImporter->GetAssetTypes() is a sub-set of assetTypes.
static std::vector<CAssetType*> FilterAssetTypes(const std::vector<CAssetType*>& assetTypes, const CAssetImporter* pAssetImporter)
{
	const std::vector<string>& names = pAssetImporter->GetAssetTypes();
	std::vector<CAssetType*> filteredTypes(assetTypes);
	auto it = std::remove_if(filteredTypes.begin(), filteredTypes.end(), [&names](CAssetType* assetType)
	{
		return std::find(names.begin(), names.end(), assetType->GetTypeName()) == names.end();
	});
	filteredTypes.erase(it, filteredTypes.end());
	return filteredTypes;
}

static std::vector<string> GetAssetTypeNames(const std::vector<CAssetType*>& assetTypes)
{
	std::vector<string> assetTypeNames;
	assetTypeNames.reserve(assetTypes.size());
	std::transform(assetTypes.begin(), assetTypes.end(), std::back_inserter(assetTypeNames), [](const CAssetType* pAssetType)
	{
		return pAssetType->GetTypeName();
	});
	return assetTypeNames;
}

std::vector<CAsset*> CAssetDropHandler::ImportExt(const string& ext, const std::vector<string>& filePaths, const SImportParamsEx& importParams)
{
	using namespace Private_AssetDropHandler;

	// Gather all asset types for this extension.
	std::vector<CAssetImporter*> extAssetImporters;
	for (CAssetImporter* pAssetImporter : CAssetManager::GetInstance()->GetAssetImporters())
	{
		if (CanImportExtension(pAssetImporter, ext))
		{
			extAssetImporters.push_back(pAssetImporter);
		}
	}

	if (extAssetImporters.empty())
	{
		return std::vector<CAsset*>();  // No asset importers can import this extension.
	}

	std::vector<string> assetTypeNames;
	for (const CAssetImporter* pAssetImporter : extAssetImporters)
	{
		// assetTypeNames does not contain any duplicates, because for the same extension, no two importers have the same asset type.
		// See CAssetManager::UpdateAssetImporters.
		const std::vector<string>& names = pAssetImporter->GetAssetTypes();
		assetTypeNames.insert(assetTypeNames.end(), names.begin(), names.end());
	}

	std::vector<CAssetType*> extAssetTypes;
	extAssetTypes.reserve(assetTypeNames.size());
	std::transform(assetTypeNames.begin(), assetTypeNames.end(), std::back_inserter(extAssetTypes), [](const string& str)
	{
		return CAssetManager::GetInstance()->FindAssetType(str.c_str());
	});

	std::vector<bool> assetTypeSelection(assetTypeNames.size(), true);

	std::vector<string> filePathsCopy(filePaths);

	std::vector<CAsset*> assets;

	CAssetImportContext context;
	while (!filePathsCopy.empty())
	{
		const string filePath = filePathsCopy.back();

		context.SetInputFilePath(filePath);
		context.SetOutputDirectoryPath(importParams.outputDirectory);
		context.ClearImportedAssets();
		context.ClearSharedDataAndPointers();

		if (!importParams.bHideDialog)
		{
			const bool ret = ThreadingUtils::PostOnMainThread([&context, &extAssetTypes, &assetTypeSelection, &filePathsCopy]()
			{
				CAssetImportDialog importDialog(context, extAssetTypes, assetTypeSelection, filePathsCopy);
				bool ret = importDialog.Execute();
				assetTypeSelection = importDialog.GetAssetTypeSelection();
				return ret;
			}).get();
			if (!ret)
			{
				return assets; // Canceling one file also cancels the rest of this batch.
			}
			if (std::none_of(assetTypeSelection.begin(), assetTypeSelection.end(), [](bool b) { return b; }))
			{
				continue;  // User did not select any asset types, so we skip this file.
			}
		}
		else
		{
			context.SetImportAll(true);
		}

		std::vector<CAssetType*> selAssetTypes;
		for (size_t i = 0; i < extAssetTypes.size(); ++i)
		{
			if (assetTypeSelection[i])
			{
				selAssetTypes.push_back(extAssetTypes[i]);
			}
		}

		context.SetAssetTypes(selAssetTypes);

		if (context.IsImportAll())
		{
			// Import all residual files in this batch.
			while (!filePathsCopy.empty())
			{
				const string filePath = filePathsCopy.back();
				filePathsCopy.pop_back();

				context.SetInputFilePath(filePath);
				context.ClearImportedAssets();
				context.ClearSharedDataAndPointers();

				importParams.beginImportingFile(filePath);
				for (CAssetImporter* pAssetImporter : extAssetImporters)
				{
					std::vector<CAssetType*> assetTypes(FilterAssetTypes(selAssetTypes, pAssetImporter));
					if (!assetTypes.empty())
					{
						const std::vector<CAsset*>& ret = pAssetImporter->Import(GetAssetTypeNames(assetTypes), context);
						assets.insert(assets.end(), ret.begin(), ret.end());
					}
				}
				importParams.endImportingFile();
			}
		}
		else
		{
			// Only import next file.
			importParams.beginImportingFile(filePath);
			for (CAssetImporter* pAssetImporter : extAssetImporters)
			{
				std::vector<CAssetType*> assetTypes(FilterAssetTypes(selAssetTypes, pAssetImporter));
				if (!assetTypes.empty())
				{
					const std::vector<CAsset*>& ret = pAssetImporter->Import(GetAssetTypeNames(assetTypes), context);
					assets.insert(assets.end(), ret.begin(), ret.end());
				}
			}
			importParams.endImportingFile();
			filePathsCopy.pop_back();
		}
	}

	return assets;
}

