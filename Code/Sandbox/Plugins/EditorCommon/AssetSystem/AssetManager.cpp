// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#include "StdAfx.h"
#include "AssetManager.h"

#include "AssetImporter.h"
#include "PakImporter.h"
#include "AssetGenerator.h"
#include "AssetFileMonitor.h"
#include "AssetManagerHelpers.h"
#include "EditableAsset.h"
#include "DependencyTracker.h"
#include "FilePathUtil.h"
#include "ThreadingUtils.h"

#include "Browser/AssetModel.h"
#include "Browser/AssetFoldersModel.h"
#include "Loader/AssetLoaderBackgroundTask.h"
#include "Loader/AssetLoaderHelpers.h"
#include "AssetResourceSelector.h"

#include <CrySystem/IConsole.h>
#include <CryString/CryPath.h>

namespace Private_AssetManager
{
	using FileToAssetMap = std::unordered_map<string, CAsset*, stl::hash_stricmp<string>, stl::hash_stricmp<string>>;

	void DeleteAssetFilesFromMap(FileToAssetMap& fileToAssetMap, CAsset* pAsset, bool shouldDeleteMetadataFile = true)
	{
		if (shouldDeleteMetadataFile)
		{
			fileToAssetMap.erase(pAsset->GetMetadataFile());
		}
		for (auto const& file : pAsset->GetFiles())
		{
			fileToAssetMap.erase(file);
		}
	}

	void AddAssetFilesToMap(FileToAssetMap& fileToAssetMap, CAsset* pAsset, bool shouldAddMetadataFile = true)
	{
		if (shouldAddMetadataFile)
		{
			fileToAssetMap[pAsset->GetMetadataFile()] = pAsset;
		}
		for (auto const& file : pAsset->GetFiles())
		{
			fileToAssetMap[file] = pAsset;
		}
	}

	void InitializeFileToAssetMap(FileToAssetMap& fileToAssetMap, const std::vector<CAsset*>& assets)
	{
		// going once through the list of all assets in the beginning is probably not that expensive
		// comparing to penalty that we can incur by rehashing the whole table if the proper number
		// of elements is not reserved upfront. Doing some king of heuristic would be imprecise here.
		int numFiles = 0;
		for (auto pAsset : assets)
		{
			numFiles += pAsset->GetFilesCount() + 1;
		}
		const auto allocNum = numFiles * 1.1; // buffer of 10%
		fileToAssetMap.reserve(allocNum);
	}
}

const std::pair<const char*, const char*> CAssetManager::m_knownAliases[] =
{
	{ "%engine%", "Engine" }
};

CAssetManager* CAssetManager::s_instance = nullptr;

CAssetManager::CAssetManager()
	: m_assetModel(nullptr)
	, m_assetFoldersModel(nullptr)
	, m_isScanning(false)
{
	s_instance = this;
	m_assetModel = new CAssetModel();
}

CAssetManager::~CAssetManager()
{
	delete m_assetModel;
	delete m_assetFoldersModel;

	s_instance = nullptr;
}

void CAssetManager::Init()
{
	using namespace AssetManagerHelpers;

	LOADING_TIME_PROFILE_SECTION;

	UpdateAssetTypes();
	UpdateAssetImporters();
	RegisterAssetResourceSelectors();

	m_assetModel->Init();
	m_assetFoldersModel = new CAssetFoldersModel();
	m_pDependencyTracker.reset(new CDependencyTracker());

	AsyncScanForAssets();

	CAssetFileMonitor::RegisterFileListener();
	CAssetGenerator::RegisterFileListener();
	CPakImporter::RegisterFileListener();

	REGISTER_INT("ed_enableAssetPickers", 1, 0,
		"Value of 1 enables asset pickers for recommended types."
		"Value of 2 enables asset pickers for all types."
		"Any other value disables asset pickers for all types.");
}

CAssetType* CAssetManager::FindAssetType(const char* name)
{
	auto it = std::lower_bound(m_assetTypes.cbegin(), m_assetTypes.cend(), name, [](const auto a, const auto b)
	{
		return strcmp(a->GetTypeName(), b) < 0;
	});

	return ((it != m_assetTypes.cend()) && (strcmp(name, (*it)->GetTypeName()) == 0)) ? *it : nullptr;
}

const std::vector<CAssetType*>& CAssetManager::GetAssetTypes() const
{
	return m_assetTypes;
}

const std::vector<CAssetImporter*>& CAssetManager::GetAssetImporters() const
{
	return m_assetImporters;
}

CAssetImporter* CAssetManager::GetAssetImporter(const string& ext, const string& assetTypeName) const
{
	// Validation in UpdateAssetImporters makes sure there is a single importer for this combination of ext,assetTypeName.
	for (CAssetImporter* pAssetImporter : m_assetImporters)
	{
		const std::vector<string>& exts = pAssetImporter->GetFileExtensions();
		const std::vector<string>& types = pAssetImporter->GetAssetTypes();
		if (std::find(exts.cbegin(), exts.cend(), ext) != exts.cend() && std::find(types.cbegin(), types.cend(), assetTypeName) != types.cend())
		{
			return pAssetImporter;
		}
	}
	return nullptr;
}

CAsset* CAssetManager::FindAssetForMetadata(const char* szFilePath) const
{
	MAKE_SURE(szFilePath, return nullptr);

	auto it = m_fileToAssetMap.find(szFilePath);
	if (it != m_fileToAssetMap.end())
	{
		return it->second;
	}

	return nullptr;
}

CAsset* CAssetManager::FindAssetForFile(const char* szFilePath) const
{
	MAKE_SURE(szFilePath, return nullptr);

	auto it = m_fileToAssetMap.find(szFilePath);
	if (it != m_fileToAssetMap.end())
	{
		return it->second;
	}

	// try to find in aliases.
	if (szFilePath[0] != '%')
	{
		for (const auto& alias : m_knownAliases)
		{
			auto it = m_fileToAssetMap.find(PathUtil::Make(alias.first, szFilePath));
			if (it != m_fileToAssetMap.end())
			{
				return it->second;
			}
		}
	}

	return nullptr;
}

void CAssetManager::AsyncScanForAssets()
{
	m_isScanning = true;

	std::vector<string> paths { PathUtil::GetGameFolder() };
	std::transform(std::cbegin(m_knownAliases), std::cend(m_knownAliases), std::back_inserter(paths), [](const auto& x)
	{
		return x.first;
	});
	AssetLoaderBackgroundTask::Run(paths, [this](const std::vector<CAsset*>& assets)
	{
		using namespace Private_AssetManager;
		m_isScanning = false;
		InitializeFileToAssetMap(m_fileToAssetMap, assets);
		InsertAssets(assets);
		MergeAssets(m_deferredInsertBatch);
		m_deferredInsertBatch.clear();
		signalScanningCompleted();
	});
}

void CAssetManager::InsertAssets(const std::vector<CAsset*>& assets)
{
	using namespace Private_AssetManager;
	if (assets.empty())
	{
		return;
	}

	if (m_isScanning)
	{
		m_deferredInsertBatch.insert(m_deferredInsertBatch.end(), assets.begin(), assets.end());
		return;
	}

	signalBeforeAssetsInserted(assets);

	m_assets.reserve(m_assets.size() + assets.size());

	for (auto& asset : assets)
	{
		m_assets.push_back(asset);
		AddAssetFilesToMap(m_fileToAssetMap, asset);
	}

	signalAfterAssetsInserted(assets);
}

void CAssetManager::MergeAssets(std::vector<CAsset*> assets)
{
	using namespace Private_AssetManager;
	if (m_isScanning)
	{
		m_deferredInsertBatch.insert(m_deferredInsertBatch.end(), assets.begin(), assets.end());
		return;
	}

	ICryPak* const pPak = GetISystem()->GetIPak();

	size_t count = assets.size();
	for (size_t i = 0; i < count; )
	{
		CAsset* const pOther = FindAssetForMetadata(assets[i]->GetMetadataFile());
		if (pOther)
		{
			if (pOther != assets[i])
			{
				// TODO: Better error handling.
				CRY_ASSERT(pOther->GetType() == assets[i]->GetType());
				// CRY_ASSERT(pOther->GetGUID() == assets[i]->GetGUID());  Re-enable this after stabilization.
				CRY_ASSERT(!stricmp(pOther->GetName(), assets[i]->GetName()));

				// TODO: Support of this is error prone. Probably we need something like an assignment operator.
				CEditableAsset editable(*pOther);
				editable.SetSourceFile(assets[i]->GetSourceFile());
				DeleteAssetFilesFromMap(m_fileToAssetMap, pOther, false);
				editable.SetFiles(nullptr, assets[i]->GetFiles());
				AddAssetFilesToMap(m_fileToAssetMap, pOther, false);
				editable.SetLastModifiedTime(assets[i]->GetLastModifiedTime());
				editable.SetDependencies(assets[i]->GetDependencies());
				editable.SetDetails(assets[i]->GetDetails());

				// Force to update the thumbnail.
				if (!pOther->IsReadOnly())
				{
					pPak->RemoveFile(pOther->GetThumbnailPath());
				}
				editable.InvalidateThumbnail();

				delete assets[i];
				assets[i] = nullptr;
			}

			std::swap(assets[i], assets[--count]);

			// TODO: detect if list of files is changed, propagate the corresponding event eAssetChangeFlags_Files
			pOther->NotifyChanged(eAssetChangeFlags_All);

			// Do not increment index i.
			continue;
		}
		++i;
	}

	if (count)
	{
		assets.resize(count);
		InsertAssets(assets);
	}
}

void CAssetManager::DeleteAssets(const std::vector<CAsset*>& assets, bool bDeleteAssetsFiles)
{
	using namespace Private_AssetManager;
	MAKE_SURE(!m_assets.empty(), return );
	MAKE_SURE(!assets.empty(), return );

	signalBeforeAssetsRemoved(assets);

	for (auto x : assets)
	{
		MAKE_SURE(!x->IsReadOnly(), continue);

		for (size_t i = 0, N = m_assets.size(); i < N; ++i)
		{
			if (x != m_assets[i])
			{
				continue;
			}

			// To prevent the function from being recalled again by CAssetManager::CAssetFileMonitor
			// the asset need to be removed from the assets collection prior to delete the asset files.

			const CAssetPtr pAssetToDelete = std::move(m_assets[i]);
			m_assets[i] = std::move(m_assets[--N]);
			m_assets.resize(N);

			if (bDeleteAssetsFiles)
			{
				const bool bDeleteSourceFile = !HasSharedSourceFile(*pAssetToDelete);
				size_t numberOfFilesDeleted;
				if (!pAssetToDelete->GetType()->DeleteAssetFiles(*pAssetToDelete, bDeleteSourceFile, numberOfFilesDeleted) && !numberOfFilesDeleted)
				{
					// Restore the asset if unable to delete.
					m_assets.push_back(pAssetToDelete);
				}
				else
				{
					DeleteAssetFilesFromMap(m_fileToAssetMap, pAssetToDelete);
				}
			}
			else
			{
				DeleteAssetFilesFromMap(m_fileToAssetMap, pAssetToDelete);
			}
			break;
		}
	}

	signalAfterAssetsRemoved();
}

void CAssetManager::MoveAssets(const std::vector<CAsset*>& assets, const char* szDestinationFolder) const
{
	for (CAsset* pAsset : assets)
	{
		if (!pAsset)
		{
			continue;
		}

		const string newMetadataFile = PathUtil::Make(szDestinationFolder, PathUtil::GetFile(pAsset->GetMetadataFile()));
		if (CAssetManager::GetInstance()->FindAssetForMetadata(newMetadataFile))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Asset already exists: %s", newMetadataFile.c_str());
			continue;
		}

		// Make a new copy of the source file if it is shared between several assets, move it to the new location otherwise.
		const bool bMoveSourceFile = !HasSharedSourceFile(*pAsset);
		pAsset->GetType()->MoveAsset(pAsset, szDestinationFolder, bMoveSourceFile);
	}
}

bool CAssetManager::RenameAsset(CAsset* pAsset, const char* szNewName)
{
	using namespace Private_AssetManager;

	DeleteAssetFilesFromMap(m_fileToAssetMap, pAsset);
	const bool result = pAsset->GetType()->RenameAsset(pAsset, szNewName);
	AddAssetFilesToMap(m_fileToAssetMap, pAsset);

	return result;
}

bool CAssetManager::HasSharedSourceFile(const CAsset& asset) const
{
	if (!asset.GetType()->IsImported())
	{
		return false;
	}

	return std::find_if(m_assets.begin(), m_assets.end(), [&asset](const CAssetPtr& x)
	{
		return (&asset != x) && (stricmp(x->GetSourceFile(), asset.GetSourceFile()) == 0);
	}) != m_assets.end();
}

//Reimport is implemented in asset manager to avoid adding dependency from CAsset to CAssetImporter
void CAssetManager::Reimport(CAsset* pAsset)
{
	CRY_ASSERT(pAsset && pAsset->GetType() && !pAsset->IsReadOnly());

	const char* szSourceFile = pAsset->GetSourceFile();
	if (!(szSourceFile && *szSourceFile))
	{
		return; // Empty source file, nothing to do.
	}

	string ext = PathUtil::GetExt(szSourceFile);
	ext.MakeLower();
	const char* const szAssetType = pAsset->GetType()->GetTypeName();
	CAssetImporter* const pAssetImporter = CAssetManager::GetInstance()->GetAssetImporter(ext, { szAssetType });
	if (!pAssetImporter)
	{
		return;
	}

	pAssetImporter->Reimport(pAsset);
}

std::vector<CAssetPtr> CAssetManager::GetAssetsFromDirectory(const string& directory) const
{
	std::vector<CAssetPtr> assets;
	assets.reserve(GetAssetsCount() / 8); // just a non zero assumption

	for (const CAssetPtr& pAsset : m_assets)
	{
		if (!pAsset->GetType()->HasThumbnail())
		{
			continue;
		}

		if (directory.empty() || strncmp(directory.c_str(), pAsset->GetMetadataFile(), directory.size()) == 0)
		{
			assets.emplace_back(pAsset);
		}
	};
	return assets;
}

void CAssetManager::AppendContextMenuActions(CAbstractMenu& menu, const std::vector<CAsset*>& assets, const std::shared_ptr<IUIContext>& context) const
{
	if (menu.FindSectionByName("Assets") == CAbstractMenu::eSections_Default)
	{
		int section = menu.GetNextEmptySection();
		menu.SetSectionName(section, "Assets");
	}

	signalContextMenuRequested(menu, assets, context);
}

void CAssetManager::UpdateAssetTypes()
{
	std::vector<IClassDesc*> classes;
	GetIEditor()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_ASSET_TYPE, classes);

	m_assetTypes.clear();
	std::transform(classes.begin(), classes.end(), std::back_inserter(m_assetTypes), [](const IClassDesc* x) { return (CAssetType*)x; });

	// Validate asset type file extensions.
	// File extensions must...
	// ... be unique among asset types.
	// ... all lower-case.
	// ... not have a leading dot, nor a trailing "cryasset".
	{
		auto isLowerCase = [](const string& s)
		{
			string lower = s;
			lower.MakeLower();
			return s == lower;
		};

		auto endsWithCryasset = [](const string& s)
		{
			size_t n = strlen("cryasset");
			return s.size() >= n && s.substr(s.size() - n) == "cryasset";
		};

		for (CAssetType* pAssetType : m_assetTypes)
		{
			if (pAssetType->GetTypeName() == "cryasset")
			{
				// File extension of fall-back asset type can be arbitrary.
				continue;
			}

			const char* szExt = pAssetType->GetFileExtension();
			CRY_ASSERT_MESSAGE(szExt && *szExt, "File extension of type %s must not be empty", pAssetType->GetTypeName());
			CRY_ASSERT_MESSAGE(szExt[0] != '.', "File extension of type %s must not have leading dot", pAssetType->GetTypeName());
			CRY_ASSERT_MESSAGE(isLowerCase(szExt), "File extension of type %s must be all-lower case", pAssetType->GetTypeName());
			CRY_ASSERT_MESSAGE(!endsWithCryasset(szExt), "File extension of type %s must be all-lower case", pAssetType->GetTypeName());
		}

		std::sort(m_assetTypes.begin(), m_assetTypes.end(), [](const auto a, const auto b)
		{
			return strcmp(a->GetFileExtension(), b->GetFileExtension()) < 0;
		});

		auto it = std::adjacent_find(m_assetTypes.begin(), m_assetTypes.end(), [](const auto a, const auto b)
		{
			return strcmp(a->GetFileExtension(), b->GetFileExtension()) == 0;
		});

		if (it != m_assetTypes.end())
		{
			CRY_ASSERT_MESSAGE(false, "Two asset types cannot be registered with the same file extension : %s", (*it)->GetFileExtension());
		}
	}

	// Validate asset type names.
	// Type names must be unique among asset types.
	{
		std::sort(m_assetTypes.begin(), m_assetTypes.end(), [](const auto a, const auto b)
		{
			return strcmp(a->GetTypeName(), b->GetTypeName()) < 0;
		});

		auto it = std::adjacent_find(m_assetTypes.begin(), m_assetTypes.end(), [](const auto a, const auto b)
		{
			return strcmp(a->GetTypeName(), b->GetTypeName()) == 0;
		});

		if (it != m_assetTypes.end())
		{
			CRY_ASSERT_MESSAGE(false, "Two asset types cannot be registered with the same type name : %s", (*it)->GetTypeName());
		}
	}

	// CAssetManager::FindAssetType() assumes types to be sorted by name.
	CRY_ASSERT(std::is_sorted(m_assetTypes.begin(), m_assetTypes.end(), [](const auto a, const auto b)
	{
		return strcmp(a->GetTypeName(), b->GetTypeName()) < 0;
	}));

	for (CAssetType* pAssetType : m_assetTypes)
	{
		pAssetType->Init();
	}
}

void CAssetManager::UpdateAssetImporters()
{
	std::vector<IClassDesc*> classes;
	GetIEditor()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_ASSET_IMPORTER, classes);

	m_assetImporters.clear();
	m_assetImporters.reserve(classes.size());
	std::transform(classes.begin(), classes.end(), std::back_inserter(m_assetImporters), [](const IClassDesc* x) { return (CAssetImporter*)x; });

	//! TODO: Validate asset importers.
}

void CAssetManager::RegisterAssetResourceSelectors()
{
	m_resourceSelectors.clear();
	m_resourceSelectors.reserve(m_assetTypes.size());
	for (CAssetType* assetType : m_assetTypes)
	{
		if (assetType->IsUsingGenericPropertyTreePicker())
			m_resourceSelectors.emplace_back(assetType->GetTypeName());
	}

	for (const SStaticAssetSelectorEntry& entry : m_resourceSelectors)
	{
		GetIEditor()->GetResourceSelectorHost()->RegisterResourceSelector(&entry);
	}
}

std::vector<std::pair<CAsset*,int32>> CAssetManager::GetReverseDependencies(const CAsset& asset) const
{
	MAKE_SURE(asset.GetFile(0), return{});

	std::vector<SAssetDependencyInfo> dependencyInfos = m_pDependencyTracker.get()->GetReverseDependencies(asset.GetFile(0));
	std::vector<std::pair<CAsset*,int32>> dependencies;
	dependencies.reserve(dependencyInfos.size());

	for (const auto& item : dependencyInfos)
	{
		CAsset* pAsset = CAssetManager::GetInstance()->FindAssetForFile(item.path);
		if (!pAsset)
		{
			continue;
		}
		dependencies.emplace_back(pAsset, item.usageCount);
	}
	return dependencies;
}

std::vector<CAsset*> CAssetManager::GetReverseDependencies(const std::vector<CAsset*>& assets) const
{
	std::vector<CAsset*> ordered(assets);
	std::sort(ordered.begin(), ordered.end());
	std::unordered_set<CAsset*> set;
	for( CAsset* pAsset : ordered)
	{
		if (!pAsset)
		{
			continue;
		}
		std::vector<std::pair<CAsset*,int32>> tmp = GetReverseDependencies(*pAsset);
		std::transform(tmp.begin(), tmp.end(), std::inserter(set, set.begin()), [](auto& x)
		{
			return x.first;
		});
	}
	std::vector<CAsset*> dependencies;
	dependencies.reserve(set.size());

	// The given assets are not considered as dependent assets.
	std::copy_if(set.begin(), set.end(), std::back_inserter(dependencies), [&ordered](const CAsset* x)
	{
		return !std::binary_search(ordered.begin(), ordered.end(), x);
	});
	return dependencies;
}

bool CAssetManager::HasAnyReverseDependencies(const CAsset& asset) const
{
	MAKE_SURE(asset.GetFile(0), return false);

	std::vector<SAssetDependencyInfo> dependencyInfos = m_pDependencyTracker.get()->GetReverseDependencies(asset.GetFile(0));

	for (const auto& item : dependencyInfos)
	{
		CAsset* pAsset = CAssetManager::GetInstance()->FindAssetForFile(item.path);
		if (pAsset)
		{
			return true;
		}
	}
	return false;
}

bool CAssetManager::HasAnyReverseDependencies(const std::vector<CAsset*>& assets) const
{
	return std::any_of(assets.cbegin(), assets.cend(), [&assets, this](CAsset* pAsset)
	{
		if (!pAsset)
		{
			return false;
		}

		std::vector<std::pair<CAsset*, int32>> dependencyInfos(GetReverseDependencies(*pAsset));
		if (dependencyInfos.empty())
		{
			return false;
		}

		// The given assets are not considered as dependent assets.
		return std::any_of(dependencyInfos.cbegin(), dependencyInfos.cend(), [&assets](const auto& item)
		{
			return std::find(assets.cbegin(), assets.cend(), item.first) == assets.end();
		});
	});
}


const char* CAssetManager::GetAliasName(const char* alias) const
{
	const auto it = std::find_if(std::cbegin(m_knownAliases), std::cend(m_knownAliases), [alias](const auto& x)
	{
		return stricmp(x.first, alias) == 0;
	});

	return (it != std::end(m_knownAliases)) ? it->second : alias;
}

std::vector<const char*> CAssetManager::GetAliases() const
{
	std::vector<const char*> aliases;
	std::transform(std::cbegin(m_knownAliases), std::cend(m_knownAliases), std::back_inserter(aliases), [](const auto& x)
	{
		return x.first;
	});
	return aliases;
}

void CAssetManager::GenerateCryassetsAsync(const std::function<void()>& finalize)
{
	using namespace AssetManagerHelpers;
	
	m_isScanning = true;

	ThreadingUtils::Async([this, finalize]()
	{
		using namespace AssetManagerHelpers;

		CAssetGenerator::GenerateCryassets();

		if (GetIEditor()->IsMainFrameClosing())
		{
			return;
		}

		ThreadingUtils::PostOnMainThread([this, finalize]()
		{
			if (GetIEditor()->IsMainFrameClosing())
			{
				return;
			}

			m_isScanning = false;
			MergeAssets(m_deferredInsertBatch);
			m_deferredInsertBatch.clear();

			if (finalize)
			{
				finalize();
			}
		});
	});
}

void CAssetManager::SaveAll(std::function<void(float)> progress)
{
	if (!progress)
	{
		progress = [](float) {};
	}

	for (size_t i = 0, n = m_assets.size(); i < m_assets.size(); ++i)
	{
		CAssetPtr& pAsset = m_assets[i];

		// TODO: not all asset editors maintain the IsModified() state properly.
		if (/* pAsset->IsModified() &&*/ pAsset->GetCurrentEditor())
		{
			pAsset->GetCurrentEditor()->Save();
			progress(float(i) / n);
		}
	}
	progress(1.0f);
}

void CAssetManager::SaveBackup(const string& backupFolder)
{
	for (CAssetPtr& pAsset : m_assets)
	{
		// TODO: not all asset editors maintain the IsModified() state properly.
		if (/* pAsset->IsModified() && */ pAsset->GetCurrentEditor())
		{
			pAsset->GetCurrentEditor()->SaveBackup(backupFolder);
		}
	}
}


