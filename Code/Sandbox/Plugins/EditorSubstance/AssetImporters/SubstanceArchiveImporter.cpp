// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceArchiveImporter.h"
#include <AssetSystem/AssetImportContext.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/Loader/AssetLoaderHelpers.h>
#include <AssetSystem/Asset.h>
#include <AssetSystem/EditableAsset.h>
#include "SandboxPlugin.h"
#include "EditorSubstanceManager.h"
#include "ISubstanceManager.h"
#include "AssetTypes/SubstanceArchive.h"

#include <FilePathUtil.h>
#include "Util/FileUtil.h"


namespace EditorSubstance
{
	namespace AssetImporters
	{
		REGISTER_ASSET_IMPORTER(CAssetImporterSubstanceArchive)

			std::vector<string> CAssetImporterSubstanceArchive::GetFileExtensions() const
		{
			return{ "sbsar" };
		}

		std::vector<string> CAssetImporterSubstanceArchive::GetAssetTypes() const
		{
			return
			{
				"SubstanceDefinition"
			};
		}

		std::vector<string> CAssetImporterSubstanceArchive::GetAssetNames(const std::vector<string>& assetTypes, CAssetImportContext& ctx)
		{
			const string filename = PathUtil::GetFileName(ctx.GetInputFilePath());
			string path = PathUtil::Make(ctx.GetOutputDirectoryPath(), filename);
			path = PathUtil::ReplaceExtension(path, ".sbsar.cryasset");
			return{ path };
		}

		std::vector<CAsset*> CAssetImporterSubstanceArchive::ImportAssets(const std::vector<string>& assetPaths, CAssetImportContext& ctx)
		{
			const string filename = PathUtil::GetFile(ctx.GetInputFilePath());
			const string absToDir = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), ctx.GetOutputDirectoryPath());
			const auto absTargetFilePath = PathUtil::Make(absToDir, filename);
			CFileUtil::CreatePath(absTargetFilePath);
			CFileUtil::CopyFile(ctx.GetInputFilePath(), absTargetFilePath);

			const string relativeFileName = PathUtil::AbsolutePathToGamePath(absTargetFilePath);

			CAsset* currentAsset = CAssetManager::GetInstance()->FindAssetForFile(relativeFileName);
			bool checkDepenencies = true;
			if (!currentAsset)
			{
				currentAsset = new CAsset("SubstanceDefinition", CryGUID::Create(), ctx.GetAssetName());
				checkDepenencies = false;
			}
			CEditableAsset editAsset = ctx.CreateEditableAsset(*currentAsset);
			editAsset.SetMetadataFile(relativeFileName + ".cryasset");
			editAsset.SetFiles("", { relativeFileName });

			//////////////////////////////////////////////////////////////////////////
			// gather the information about substance graph
			std::map < string, std::vector<string>> archiveInfo;
			ISubstanceManager::Instance()->GetArchiveContents(relativeFileName, archiveInfo);

			static_cast<const AssetTypes::CSubstanceArchiveType*>(currentAsset->GetType())->SetArchiveCache(&editAsset, archiveInfo);
			editAsset.WriteToFile();

			// after imported, lest try regenerate everything dependant
			if (checkDepenencies)
			{
				for (auto& item : CAssetManager::GetInstance()->GetReverseDependencies(*currentAsset))
				{
					CAsset* pDepAsset = item.first;
					string dependantAssetTypeName(pDepAsset->GetType()->GetTypeName());
					if (dependantAssetTypeName == "SubstanceInstance")
					{
						EditorSubstance::CManager::Instance()->ForcePresetRegeneration(pDepAsset);
					}
				}
			}

			return{ currentAsset };
		}
	}
}
