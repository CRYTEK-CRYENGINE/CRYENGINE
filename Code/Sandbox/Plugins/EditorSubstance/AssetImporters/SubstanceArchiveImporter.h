// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetImporter.h>

namespace EditorSubstance
{
	namespace AssetImporters
	{

		class CAssetImporterSubstanceArchive : public CAssetImporter
		{
			DECLARE_ASSET_IMPORTER_DESC(CAssetImporterSubstanceArchive)
		public:
			virtual std::vector<string> GetFileExtensions() const override;
			virtual std::vector<string> GetAssetTypes() const override;

		private:
			virtual std::vector<string> GetAssetNames(const std::vector<string>& assetTypes, CAssetImportContext& ctx) override;
			virtual std::vector<CAsset*> ImportAssets(const std::vector<string>& assetPaths, CAssetImportContext& ctx) override;
		};
	}
}
