// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "GeometryCacheAssetType.h"
#include "AssetSystem/AssetImporter.h"
#include "AssetSystem/AssetImportContext.h"
#include "Alembic/AlembicCompiler.h"
#include "FilePathUtil.h"

REGISTER_ASSET_TYPE(CGeometryCacheType)

CAssetEditor* CGeometryCacheType::Edit(CAsset* pAsset) const
{
	// Editing a geometry cache type presents a special case, as it does not return an asset editor.
	// Instead, we present the modal dialog of the RC.  In the future, this should be changed, and
	// a properly integrated editor should exist in the Sandbox.

	if (!pAsset->GetSourceFile() || !*pAsset->GetSourceFile())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to edit %s. No source file.", pAsset->GetName());
		return nullptr;
	}

	CRY_ASSERT(pAsset && pAsset->GetType() && pAsset->GetType()->GetTypeName() == GetTypeNameStatic());

	if (!GetISystem()->GetIPak()->IsFileExist(pAsset->GetSourceFile()))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to edit %s. The source file not found: %s.", pAsset->GetName(), pAsset->GetSourceFile());
		return nullptr;
	}

	CAlembicCompiler compiler;
	string assetFilename;
	compiler.CompileAlembic(assetFilename, pAsset->GetSourceFile(), true);
	return nullptr;
}

CryIcon CGeometryCacheType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_geomcache.ico");
}


class CAssetImporterGeometryCache : public CAssetImporter
{
public:
	DECLARE_ASSET_IMPORTER_DESC(CAssetImporterGeometryCache)

	virtual std::vector<string> GetFileExtensions() const override { return { "abc" }; }
	virtual std::vector<string> GetAssetTypes() const override { return{ CGeometryCacheType::GetTypeNameStatic() }; }

private:
	virtual std::vector<string> GetAssetNames(const std::vector<string>& assetTypes, CAssetImportContext& ctx) override
	{
		const string basename = PathUtil::GetFileName(ctx.GetInputFilePath());
		return{ ctx.GetOutputFilePath(".cax.cryasset") };
	}
	virtual std::vector<CAsset*> ImportAssets(const std::vector<string>& assetPaths, CAssetImportContext& ctx) override
	{
		const string absOutputSourceFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), ctx.GetOutputSourceFilePath());

		// We already have confirmation to overwrite the file. see CAssetImporter::Import() 
		if (!PathUtil::CopyFileAllowOverwrite(ctx.GetInputFilePath(), absOutputSourceFilePath))
		{
			CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, 
				"Failed to copy source file to: %s \n"
				"Make sure the file does not exist in the target location or is writable."
				, absOutputSourceFilePath.c_str());
			return{};
		}

		PathUtil::MakeFileWritable(absOutputSourceFilePath);

		CAlembicCompiler compiler;
		string assetFilename;

		if (!compiler.CompileAlembic(assetFilename, ctx.GetOutputSourceFilePath(), false))
		{
			return{};
		}

		CAsset* const pAsset = ctx.LoadAsset(ctx.GetOutputFilePath("cax.cryasset"));
		return{ pAsset };
	}
};

REGISTER_ASSET_IMPORTER(CAssetImporterGeometryCache)