// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

#include "Asset.h"
#include "AssetType.h"
#include "AssetConverter.h"
#include "ObjectIndexContainer.h"
#include <CrySandbox/CrySignal.h>
#include <CryCore/StlUtils.h>

#include <future>

class CAssetImporter;
class CDependencyTracker;
struct SStaticAssetSelectorEntry;

//! Provides context of user interaction for CAssetManager::signalContextMenuRequested handlers.
struct IUIContext
{
	//! Returns pointer to the newly created asset, or nullptr if the operation is canceled by user.
	//! \param type Reference to instance of the asset type to be created.
	//! \param pTypeSpecificParameter Pointer to an extra parameter, can be nullptr.
	//! \sa CAssetType::Create
	virtual CAsset* QueryNewAsset(const CAssetType& type, const CAssetType::SCreateParams* pCreateParams) = 0;
	virtual ~IUIContext() {}
};

class EDITOR_COMMON_API CAssetManager
{
	friend class CAssetModel;
	friend class CAssetFoldersModel;

public:
	template<bool isOnePerAsset>
	using AssetFilesTracker = CObjectIndexContainer<string, CAsset, isOnePerAsset, stl::hash_stricmp<string>, stl::hash_stricmp<string>>;

	CAssetManager();
	~CAssetManager();

	static CAssetManager* GetInstance() { return s_instance; }

	//Must be called after IEditor has been initialized and plugins have been loaded
	void                                Init();

	int                                 GetAssetsCount() const { return m_assets.size(); }

	CAssetType*                         FindAssetType(const char* name) const;

	const std::vector<CAssetType*>&     GetAssetTypes() const;
	const std::vector<CAssetImporter*>& GetAssetImporters() const;
	CAssetImporter*                     GetAssetImporter(const string& ext, const string& assetTypeName) const;

	//! Find the correct Asset Converter for this kind of mime data
	CAssetConverter* GetAssetConverter(const QMimeData& data) const;

	//! Find if an Asset Converter  exists for this  kind of mime data
	bool HasAssetConverter(const QMimeData& data) const;

	//! Finds an asset with matching metadata file path
	//! \param szFilePath Relative to the assets root directory.
	CAsset* FindAssetForMetadata(const char* szFilePath) const;

	//! Finds an asset with matching file associated. Includes data and metadata files
	//! \param szFilePath Relative to the assets root directory.
	CAsset* FindAssetForFile(const char* szFilePath) const;

	//! Finds an asset by id.
	//! \param guid.
	CAsset* FindAssetById(const CryGUID& guid);

	void    InsertAssets(const std::vector<CAsset*>& assets);

	//! MergeAssets updates an asset if it already exists, and inserts a new one, otherwise.
	void MergeAssets(std::vector<CAsset*> assets);

	//! Removes specified assets from the asset browser along with files from file system. 
	//! \param assets A collection of assets to be deleted. The assets pointers are invalid after this operation.
	//! \sa CAsset::IsReadOnly 
	//! \sa CAssetType::DeleteAssetFiles
	//! \sa CAssetManager::signalBeforeAssetsRemoved
	//! \sa CAssetManager::signalAfterAssetsRemoved
	std::future<void> DeleteAssetsWithFiles(std::vector<CAsset*> assets);
	
	//! Removes specified assets from the asset browser. 
	//! \param assets A collection of assets to be deleted. The assets pointers are invalid after this operation.
	//! \param bDeleteAssetsFiles a boolean value.
	//! \sa CAsset::IsReadOnly
	//! \sa CAssetType::DeleteAssetFiles
	//! \sa CAssetManager::signalBeforeAssetsRemoved
	//! \sa CAssetManager::signalAfterAssetsRemoved
	void DeleteAssetsOnlyFromData(std::vector<CAsset*> assets);

	//! Moves existing assets to the specified folder, including all assets files.
	//! \param assets A collection of assets to be moved.
	//! \param szDestinationFolder The destination folder. The path must be relative to the assets root directory.
	void MoveAssets(const std::vector<CAsset*>& assets, const char* szDestinationFolder);

	//! Renames an existing asset.
	//! \param pAsset The asset to be renamed.
	//! \param szNewName The new name for the asset. The asset must not already exist.
	bool                RenameAsset(CAsset* pAsset, const char* szNewName);

	CAssetModel*        GetAssetModel()        { return m_assetModel; }
	CAssetFoldersModel* GetAssetFoldersModel() { return m_assetFoldersModel; }
	bool                IsScanning() const     { return m_isScanning; }

	//! Applies a function to each asset.
	//! \param function Function object to be applied to every asset. The signature of the function should be equivalent void f(CAsset*).
	template<typename F>
	void ForeachAsset(F&& function)
	{
		for (CAssetPtr& pAsset : m_assets)
			function(pAsset);
	}

	//! Applies a function to each asset of a specified type.
	//! \param pAssetType The asset type we are going to look for
	//! \param function Function object to be applied to every asset of type pAssetType. The signature of the function should be equivalent void f(CAsset*).
	template<typename F>
	void ForeachAssetOfType(const CAssetType* pAssetType, F&& function)
	{
		ForeachAsset([pAssetType, function](CAsset* pAsset)
		{
			//If it's a pAssetType we have to update it 
			if (pAsset->GetType() == pAssetType)
			{
				function(pAsset);
			}
		});
	}

	//! Imports the assets source file again while trying to preserve user settings.
	//! Reimporting is delegated to an asset importer based on the source file's extension, so it
	//! should be handled by the same importer that created the asset in the first place.
	//! If there is no source file, or if no importer can be found, this method does nothing.
	void Reimport(CAsset* pAsset);

	//! Returns a list of assets that use the given asset.
	//! \param asset The asset to enumerate reverse dependencies.
	//! \return Returns a collection of pairs, where
	//! the first element points to the dependent asset, and
	//! the second element contains the instance count for the dependency or 0 if such information is not available.
	//! \sa CAsset::GetDependencies to get a list of forward dependencies.
	std::vector<std::pair<CAsset*, int32>> GetReverseDependencies(const CAsset& asset) const;

	//! Returns a list of assets that use the given assets.
	//! The assets in the given collection are not considered as dependent assets.
	//! \param assets The collection of assets to enumerate reverse dependencies.
	//! \return Returns a collection of asset pointers.
	//! \sa CAsset::GetDependencies to get a list of forward dependencies.
	std::vector<CAsset*> GetReverseDependencies(const std::vector<CAsset*>& assets) const;

	//! Checks whether any assets depend on the given asset.
	//! \param asset The asset to check reverse dependencies.
	//! \return Returns true if there is at least one asset that depends on the given asset.
	//! \sa CAsset::GetDependencies to get a list of forward dependencies.
	bool HasAnyReverseDependencies(const CAsset& asset) const;

	//! Checks whether any assets depend on any given assets.
	//! The assets in the given collection are not considered as dependent assets.
	//! \param assets The collection of assets to enumerate reverse dependencies.
	//! \return Returns true if there is at least one asset that depends on any of the given assets.
	//! \sa CAsset::GetDependencies to get a list of forward dependencies.
	bool HasAnyReverseDependencies(const std::vector<CAsset*>& assets) const;

	//! Accessor to the Dependency Tracker instance.
	//! \sa CDependencyTracker
	CDependencyTracker* GetDependencyTracker() const { return m_pDependencyTracker.get(); }

	//! Returns a user friendly name for the alias or the parameter string if the alias is not known.
	const char* GetAliasName(const char* alias) const;

	//! Returns a list of known asset root aliases.
	std::vector<const char*> GetAliases() const;

	//! Generates/repairs cryasset files for all known asset types in game content directory of active project.
	//! This process intended to be non destructive and should never break or remove any valid asset metadata.
	//! It runs asynchronously and then calls finalize on the main thread.
	void GenerateCryassetsAsync(const std::function<void()>& finalize);

	//! Saves all assets that are being edited at the moment.
	//! \param progress. A function object that is called each time another portion of the assets have been saved. The progress value is in the range 0..1 inclusive.
	void SaveAll(std::function<void(float)> progress = nullptr);

	//! Creates a backup copy of the assets that are being edited.
	//! This may happen if critical error have occurred and Sandbox gives a user way to save data and not lose it due to crash.
	//! \sa ISystemUserCallback::OnSaveDocument
	//! \param backupFolder string contains a full path to store the backup.
	void SaveBackup(const string& backupFolder);

	//! Returns a collection of assets that belong to the directory (including child directories).
	std::vector<CAssetPtr> GetAssetsFromDirectory(const string& directory, std::function<bool(CAsset*)> predicate = {}) const;

	//! Returns assets' source files tracker.
	const AssetFilesTracker<true>& GetSourceFilesTracker() const { return m_sourceFilesTracker; }
	AssetFilesTracker<true>& GetSourceFilesTracker() { return m_sourceFilesTracker; }

	//! Returns assets' work files tracker.
	const AssetFilesTracker<false>& GetWorkFilesTracker() const { return m_workFilesTracker; }
	AssetFilesTracker<false>& GetWorkFilesTracker() { return m_workFilesTracker; }

	//! Braces the invalidation of all assets.
	CCrySignal<void()> signalBeforeAssetsReset;
	CCrySignal<void()> signalAfterAssetsReset;

	//! Braces the insertion of new assets.
	CCrySignal<void(const std::vector<CAsset*>&)> signalBeforeAssetsInserted;
	CCrySignal<void(const std::vector<CAsset*>&)> signalAfterAssetsInserted;
	
	//! Signals that a group of assets is updated.
	CCrySignal<void(const std::vector<CAsset*>&)> signalAssetsUpdated;

	//! Braces the removing of existing assets.
	CCrySignal<void(const std::vector<CAsset*>&)> signalBeforeAssetsRemoved;
	CCrySignal<void()>                            signalAfterAssetsRemoved;

	//! Called when any asset changes. Equivalent to CAsset::signalChanged
	//! \sa AssetChangeFlags
	CCrySignal<void(CAsset&, int /*changeFlags*/)> signalAssetChanged;

	CCrySignal<void()>                             signalScanningCompleted;

private:
	void UpdateAssetTypes();
	void UpdateAssetImporters();
	void UpdateAssetConverters();
	void RegisterAssetResourceSelectors();
	//Init and update asset registry
	void AsyncScanForAssets();

	// Returns true if the specified asset shares the source file with any other assets.
	// \sa CAsset::GetSourceFile
	bool HasSharedSourceFile(const CAsset& asset) const;

private:
	std::vector<CAssetType*>               m_assetTypes;
	std::vector<SStaticAssetSelectorEntry> m_resourceSelectors;
	std::vector<CAssetImporter*>           m_assetImporters;
	std::vector<CAssetConverter*>          m_assetConverters;

	std::vector<CAssetPtr>                 m_assets;

	std::unordered_map<string, CAsset*, stl::hash_stricmp<string>, stl::hash_stricmp<string>> m_fileToAssetMap;

	// There might be assets inserted while the initial scan for assets is still running (e.g., by the file monitor).
	// Every asset inserted before the initial scanning is completed is added to a batch that will be inserted once scanning is completed.
	std::vector<CAsset*> m_deferredInsertBatch;

	bool                 m_isScanning;

	// The asset collection can be sorted by guids on demand of FindAssetById.
	bool m_orderedByGUID;

	//Managing of asset models for optimization, all views should refer to those
	//Keep model logic in the models themselves, i.e. models observe the manager. Do not call models methods within AssetManager
	CAssetModel*                        m_assetModel;
	CAssetFoldersModel*                 m_assetFoldersModel;

	std::unique_ptr<CDependencyTracker> m_pDependencyTracker;

	// A list of pairs {alias name, user friendly name};
	const static std::pair<const char*, const char*> m_knownAliases[];

	AssetFilesTracker<true>  m_sourceFilesTracker{ [](const CAsset& asset) { return asset.GetSourceFile(); } };
	AssetFilesTracker<false> m_workFilesTracker{ [](const CAsset& asset) { return  asset.GetWorkFiles(); } };

	//For convenience and speed, do not expose, only accessible to internals of the asset system through friend status
	static CAssetManager* s_instance;
};
