// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

#include "Asset.h"
#include "AssetType.h"

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
	virtual CAsset* QueryNewAsset(const CAssetType& type, const void* pTypeSpecificParameter) = 0;
	virtual ~IUIContext() {}
};

class EDITOR_COMMON_API CAssetManager
{
	friend class CAssetModel;
	friend class CAssetFoldersModel;

public:
	CAssetManager();
	virtual ~CAssetManager();

	static CAssetManager* GetInstance() { return s_instance; }

	//Must be called after IEditor has been initialized and plugins have been loaded
	void Init();

	int GetAssetsCount() const { return m_assets.size(); }

	CAssetType* FindAssetType(const char* name);

	const std::vector<CAssetType*>& GetAssetTypes() const;
	const std::vector<CAssetImporter*>& GetAssetImporters() const;
	CAssetImporter* GetAssetImporter(const string& ext, const string& assetTypeName) const;

	//! Finds an asset with matching metadata file path
	//! \param szFilePath Relative to the assets root directory.
	CAsset* FindAssetForMetadata(const char* szFilePath) const;

	//! Finds an asset with matching file associated. Includes data and metadata files
	//! \param szFilePath Relative to the assets root directory.
	CAsset* FindAssetForFile(const char* szFilePath) const;

	void InsertAssets(const std::vector<CAsset*>& assets);

	//! MergeAssets updates an asset if it already exists, and inserts a new one, otherwise.
	void MergeAssets(std::vector<CAsset*> assets);

	//! Removes specified assets from the asset browser. Deletes asset files if requested.
	//! \param assets A collection of assets to be deleted. The assets pointers are invalid after this operation.
	//! \param bDeleteAssetsFiles a boolean value.  
	//! \sa CAsset::IsReadOnly 
	//! \sa CAssetType::DeleteAssetFiles
	//! \sa CAssetManager::signalBeforeAssetsRemoved
	//! \sa CAssetManager::signalAfterAssetsRemoved
	void DeleteAssets(const std::vector<CAsset*>& assets, bool bDeleteAssetsFiles);

	//! Moves existing assets to the specified folder, including all assets files.
	//! \param assets A collection of assets to be moved.
	//! \param szDestinationFolder The destination folder. The path must be relative to the assets root directory.
	void MoveAssets(const std::vector<CAsset*>& assets, const char* szDestinationFolder) const;

	//! Renames an existing asset.
	//! \param pAsset The asset to be renamed.
	//! \param szNewName The new name for the asset. The asset must not already exist.
	bool RenameAsset(CAsset* pAsset, const char* szNewName);

	CAssetModel* GetAssetModel() { return m_assetModel; }
	CAssetFoldersModel* GetAssetFoldersModel() { return m_assetFoldersModel; }
	bool IsScanning() const { return m_isScanning; }

	//! Applies a function to each asset. 
	//! \param function Function object to be applied to every asset. The signature of the function should be equivalent void f(CAsset*).
	template<typename F>
	void ForeachAsset(F&& function)
	{
		for (CAssetPtr& pAsset : m_assets)
			function(pAsset);
	}

	//! Imports the assets source file again while trying to preserve user settings.
	//! Reimporting is delegated to an asset importer based on the source file's extension, so it
	//! should be handled by the same importer that created the asset in the first place.
	//! If there is no source file, or if no importer can be found, this method does nothing.
	void Reimport(CAsset* pAsset);

	//! Returns a list of assets that use the given asset.
	//! \param asset The asset to enumerate reverse dependencies.
	//! \return Returns a collection of pairs, where	
	//! the first element points to the dependant asset, and 
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
	std::vector<CAssetPtr> GetAssetsFromDirectory(const string& directory) const;

	void AppendContextMenuActions(CAbstractMenu& menu, const std::vector<CAsset*>& assets, const std::shared_ptr<IUIContext>& context) const;

	//! Braces the invalidation of all assets.
	CCrySignal<void()> signalBeforeAssetsUpdated;
	CCrySignal<void()> signalAfterAssetsUpdated;

	//! Braces the insertion of new assets.
	CCrySignal<void(const std::vector<CAsset*>&)> signalBeforeAssetsInserted;
	CCrySignal<void(const std::vector<CAsset*>&)> signalAfterAssetsInserted;

	//! Braces the removing of existing assets.
	CCrySignal<void(const std::vector<CAsset*>&)> signalBeforeAssetsRemoved;
	CCrySignal<void()> signalAfterAssetsRemoved;

	//! Called when any asset changes. Equivalent to CAsset::signalChanged
	//! \sa AssetChangeFlags
	CCrySignal<void(CAsset&, int /*changeFlags*/)> signalAssetChanged;

	CCrySignal<void()> signalScanningCompleted;

	CCrySignal<void(CAbstractMenu&, const std::vector<CAsset*>&, const std::shared_ptr<IUIContext>&)> signalContextMenuRequested;

private:
	void UpdateAssetTypes();
	void UpdateAssetImporters();
	void RegisterAssetResourceSelectors();
	//Init and update asset registry
	void AsyncScanForAssets();
	void RemoveAssets(const std::vector<CAsset*>& assets, bool bDeleteAssetsFiles);

	// Returns true if the specified asset shares the source file with any other assets. 
	// \sa CAsset::GetSourceFile
	bool HasSharedSourceFile(const CAsset& asset) const;

private:
	std::vector<CAssetType*> m_assetTypes;
	std::vector<SStaticAssetSelectorEntry> m_resourceSelectors;
	std::vector<CAssetImporter*> m_assetImporters;

	//TODO : investigate storing them with a different array for each type, 
	//meaning less need for contiguous memory but also
	//possibility to store them in separate model with model merging
	std::vector<CAssetPtr> m_assets;

	std::unordered_map<string, CAsset*, stl::hash_stricmp<string>, stl::hash_stricmp<string>> m_fileToAssetMap;

	// There might be assets inserted while the initial scan for assets is still running (e.g., by the file monitor).
	// Every asset inserted before the initial scanning is completed is added to a batch that will be inserted once scanning is completed.
	std::vector<CAsset*> m_deferredInsertBatch;
	
	bool m_isScanning;

	//Managing of asset models for optimization, all views should refer to those
	//Keep model logic in the models themselves, i.e. models observe the manager. Do not call models methods within AssetManager
	CAssetModel* m_assetModel;
	CAssetFoldersModel* m_assetFoldersModel;

	std::unique_ptr<CDependencyTracker> m_pDependencyTracker;

	// A list of pairs {alias name, user friendly name};
	const static std::pair<const char*, const char*> m_knownAliases[];

	//For convenience and speed, do not expose, only accessible to internals of the asset system through friend status
	static CAssetManager* s_instance;
};


