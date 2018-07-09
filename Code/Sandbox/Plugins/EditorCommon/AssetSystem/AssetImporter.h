// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <EditorCommonAPI.h>
#include <IEditorClassFactory.h>

#include <CryString/CryString.h>

#include <vector>

class CAsset;
class CAssetImportContext;
class CAssetType;

class EDITOR_COMMON_API CAssetImporter : public IClassDesc
{
public:
	virtual ESystemClassID SystemClassID() override final;

	//! GetFileExtensions returns a list of file extensions of all supported import file formats.
	//! \return List of file extensions. The list contains no duplicates. A file extension is in lower-case letters and without leading dot. Example: "fbx".
	virtual std::vector<string> GetFileExtensions() const = 0;

	//! GetAssetTypes returns a list of names of all asset types that may be created for any supported import file format.
	//! \return List of asset type names.
	//! \sa CAssetType::GetName
	virtual std::vector<string> GetAssetTypes() const = 0;

	std::vector<CAsset*> Import(const std::vector<string>& assetTypes, CAssetImportContext& ctx);

	void Reimport(CAsset* pAsset);

private:
	//! GetPotentialAssetPaths returns a list of paths of all potentially imported assets.
	//! This method is called prior to actually importing any assets. An implementation
	//! shall neither create new assets, nor modify existing assets.
	//! \return List of asset paths. An asset path is relative to the game asset folder.
	virtual std::vector<string> GetAssetNames(const std::vector<string>& assetTypes, CAssetImportContext& ctx) = 0;

	virtual std::vector<CAsset*> ImportAssets(const std::vector<string>& assetPaths, CAssetImportContext& ctx) = 0;

	virtual void ReimportAsset(CAsset* pAsset);
};

//Helper macro for declaring IClassDesc.
#define DECLARE_ASSET_IMPORTER_DESC(type)                           \
virtual const char* ClassName() override { return #type; }      \
virtual void* CreateObject() override { return nullptr; } 

#define REGISTER_ASSET_IMPORTER(type) REGISTER_CLASS_DESC(type)
