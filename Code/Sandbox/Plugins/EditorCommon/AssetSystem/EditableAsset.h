// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Asset.h"
#include "EditorCommonAPI.h"

namespace AssetLoader
{
	struct SAssetSerializer;
	class CAssetFactory;
}

//! CEditableAsset modifies an asset.
//! Modification of assets is restricted to a small number of friend classes of CEditableAsset.
//! Everybody else is supposed to use the read-only CAsset interface.
class EDITOR_COMMON_API CEditableAsset
{
	friend class CAssetType;
	friend class CAssetBrowser;
	friend class CAssetEditor;
	friend class CAssetManager;
	friend class CAssetImportContext;
	friend class AssetLoader::CAssetFactory;
	friend struct AssetLoader::SAssetSerializer;

	// TODO: Remove when CLevelEditor is CAssetEditor.
	friend class CLevelEditor;

public:
	CEditableAsset(const CEditableAsset&);
	CEditableAsset& operator=(const CEditableAsset&) = delete;

	CAsset& GetAsset();

	void WriteToFile();
	void SetName(const char* szName);
	void SetLastModifiedTime(uint64 t);
	void SetMetadataFile(const char* szFilepath);
	void SetSourceFile(const char* szFilepath);
	void AddFile(const char* szFilepath);
	void SetFiles(const char* szCommonPath, const std::vector<string>& filenames);
	void SetDetails(const std::vector<std::pair<string, string>>& details);
	void SetDependencies(std::vector<SAssetDependencyInfo> dependencies);

	//! Invalidates thumbnail, so that subsequent calls to CAsset::StartLoadingThumbnail() will re-load
	//! the thumbnail from disk.
	void InvalidateThumbnail();

	void SetOpenedInAssetEditor(CAssetEditor* pEditor);

private:
	explicit CEditableAsset(CAsset& asset);

private:
	CAsset& m_asset;
};

