// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <AssetSystem/AssetImporter.h>

#include <memory>

namespace FbxTool
{

class CScene;
struct SAnimationTake;
struct SNode;

} // namespace FbxTool

class QTemporaryDir;

class CAssetImporterFBX : public CAssetImporter
{
	DECLARE_ASSET_IMPORTER_DESC(CAssetImporterFBX)
public:
	virtual std::vector<string> GetFileExtensions() const override;
	virtual std::vector<string> GetAssetTypes() const override;

private:
	virtual std::vector<string> GetAssetNames(const std::vector<string>& assetTypes, CAssetImportContext& ctx) override;
	virtual std::vector<CAsset*> ImportAssets(const std::vector<string>& assetPaths, CAssetImportContext& ctx) override;
	virtual void ReimportAsset(CAsset* pAsset) override;

	bool Init(const string& inputFilePath);

	FbxTool::CScene* GetScene();
	string GetTempDirPath() const;

	void Import(const std::vector<string>& assetTypes, CAssetImportContext& ctx);

	void ImportMesh(CAssetImportContext& ctx);
	void ImportSkeleton(CAssetImportContext& ctx);
	void ImportAllSkins(CAssetImportContext& ctx);
	void ImportSkin(const FbxTool::SNode* pFbxNode, const string& name, CAssetImportContext& ctx);
	void ImportAllAnimations(CAssetImportContext& ctx);
	void ImportAnimation(const FbxTool::SAnimationTake* pTake, const string& name, CAssetImportContext& ctx);
	void ImportAllTextures(const std::vector<std::pair<string, string>>& relTexs, const string& outputDirectory, CAssetImportContext& ctx);
	string ImportMaterial(CAssetImportContext& ctx, const std::vector<std::pair<string, string>>& relTexs);
	void ImportCharacterDefinition(CAssetImportContext& ctx, const std::vector<CAsset*> assets);

	//! Transforms a list of absolute file paths into a list of absolute file paths.
	//! Each input file path must look like TEMP/path and is transformed into PROJ/path, where
	//! TEMP is the path of the temporary directory (see GetTempDirPath), PROJ is the path of the
	//! game project path, and path is an arbitrary sub-path.
	//! This function is used when moving all files that make up an asset from the temporary directory
	//! to the output directory.
	//! The return value is a list of pairs of form <input path, output path>.
	std::vector<std::pair<string, string>> GetAbsOutputFilePaths(const CAssetImportContext& ctx, const std::vector<string>& absTempFilePaths) const;

	//! Moves an asset from the temporary directory to the output directory.
	//! All files referenced by the asset meta-data are moved, including the meta-data file itself, i.e.,
	//! meta-data file, source file, and all data files.
	void MoveAsset(CAssetImportContext& ctx, const string& assetPath);

private:
	string m_fbxFilePath;
	std::unique_ptr<FbxTool::CScene> m_pFbxScene;
	std::unique_ptr<QTemporaryDir> m_pTempDir;
	bool m_bCreateMaterial;
};

