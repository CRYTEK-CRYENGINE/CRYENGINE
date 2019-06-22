// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetManager.h"

#include "AssetEditor.h"
#include "AssetFileMonitor.h"
#include "AssetFilesGroupController.h"
#include "AssetGenerator.h"
#include "AssetImporter.h"
#include "AssetManagerHelpers.h"
#include "AssetResourceSelector.h"
#include "Browser/AssetFoldersModel.h"
#include "Browser/AssetModel.h"
#include "DependencyTracker.h"
#include "EditableAsset.h"
#include "EditorFramework/BroadcastManager.h"
#include "FileOperationsExecutor.h"
#include "Loader/AssetLoaderBackgroundTask.h"
#include "Loader/AssetLoaderHelpers.h"
#include "PakImporter.h"
#include "PathUtils.h"
#include "ThreadingUtils.h"

#include <IEditor.h>
#include <CrySystem/IConsole.h>
#include <CryString/CryPath.h>
#include <CrySystem/ConsoleRegistration.h>

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
	const auto allocNum = numFiles * 1.1;   // buffer of 10%
	fileToAssetMap.reserve(allocNum);
}

std::vector<std::unique_ptr<IFilesGroupController>> ComposeFileGroupsForDeletion(std::vector<CAsset*>& assets)
{
		std::vector<std::unique_ptr<IFilesGroupController>> fileGroups;
		fileGroups.reserve(assets.size());

		std::sort(assets.begin(), assets.end(), [](CAsset* pAsset1, CAsset* pAsset2)
		{
			return pAsset1->GetSourceFile() == pAsset2->GetSourceFile();
		});

		auto rangeStart = assets.begin();
		auto rangeEnd = rangeStart + 1;
		auto it = rangeStart;
		string sourceFile = (*rangeStart)->GetSourceFile();
		while (true)
		{
			if (rangeEnd == assets.end() || (*rangeEnd)->GetSourceFile() != sourceFile)
			{
				bool shouldIncludeSource = !sourceFile.empty();
				if (shouldIncludeSource)
				{
				auto count = CAssetManager::GetInstance()->GetSourceFilesTracker().GetIndexCount(sourceFile);
					auto dist = std::distance(rangeStart, rangeEnd);
					shouldIncludeSource = count == dist;
				}
				fileGroups.emplace_back(new CAssetFilesGroupController(*it, shouldIncludeSource));
				if (rangeEnd == assets.end())
				{
					break;
				}
				rangeStart = rangeEnd;
				sourceFile = (*rangeStart)->GetSourceFile();
			}
			else
			{
				fileGroups.emplace_back(new CAssetFilesGroupController(*it, false));
			}
			++rangeEnd;
			++it;
		}
		return fileGroups;
}
}

const std::pair<const char*, const char*> CAssetManager::m_knownAliases[] =
{
	{ "%engine%", "Engine" } };

CAssetManager* CAssetManager::s_instance = nullptr;

CAssetManager::CAssetManager()
	: m_assetModel(nullptr)
	, m_assetFoldersModel(nullptr)
	, m_isScanning(false)
	, m_orderedByGUID(false)
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

	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	UpdateAssetTypes();
	UpdateAssetImporters();
	UpdateAssetConverters();
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

	GetIEditor()->GetGlobalBroadcastManager()->Connect(BroadcastEvent::AboutToQuit, [this](BroadcastEvent& event)
	{
		CRY_ASSERT(event.type() == BroadcastEvent::AboutToQuit);
		
		std::vector<string> changedFiles;
		for (CAsset* pAsset : m_assets)
		{
			// Here we check only those assets that support the editing sessions. 
			// Active asset editors are responsible for checking otherwise.
			// See CAssetEditor::CanQuit
			if (pAsset->IsModified() && pAsset->GetEditingSession())
			{
				changedFiles.push_back(pAsset->GetName());
			}
		}
		if (!changedFiles.empty())
		{
			AboutToQuitEvent& aboutToQuitEvent = static_cast<AboutToQuitEvent&>(event);
			aboutToQuitEvent.AddChangeList("Asset browser", changedFiles);

			event.ignore();
		}
		
	}, (uintptr_t)this);
}

CAssetType* CAssetManager::FindAssetType(const char* name) const
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

CAssetConverter* CAssetManager::GetAssetConverter(const QMimeData& data) const
{
	for (CAssetConverter* converter : m_assetConverters)
	{
		if (converter->CanConvert(data))
		{
			return converter;
		}
	}

	return nullptr;
}

bool CAssetManager::HasAssetConverter(const QMimeData& data) const
{
	return GetAssetConverter(data) != nullptr;
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

CAsset* CAssetManager::FindAssetById(const CryGUID& guid)
{
	if (!m_orderedByGUID)
	{
		signalBeforeAssetsReset();
		std::sort(m_assets.begin(), m_assets.end(), [](const CAsset* a, const CAsset* b)
		{
			return a->GetGUID() < b->GetGUID();
		});
		m_orderedByGUID = true;
		signalAfterAssetsReset();
	}

	auto it = std::lower_bound(m_assets.cbegin(), m_assets.cend(), guid, [](const CAsset* pAsset, const CryGUID& guid)
	{
		return pAsset->GetGUID() < guid;
	});

	return it != m_assets.cend() && (*it)->GetGUID() == guid ? *it : nullptr;
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

	for (CAsset* pAsset : assets)
	{
		m_sourceFilesTracker.SetIndex(pAsset->GetSourceFile(), *pAsset);
		m_workFilesTracker.SetIndices(pAsset->GetWorkFiles(), *pAsset);
		m_assets.push_back(pAsset);
		m_orderedByGUID = false;
		AddAssetFilesToMap(m_fileToAssetMap, pAsset);
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

	std::vector<CAsset*> updatedAssets;

	size_t count = assets.size();
	for (size_t i = 0; i < count;)
	{
		CAsset* const pExistingAsset = FindAssetForMetadata(assets[i]->GetMetadataFile());
		if (pExistingAsset)
		{
			// TODO: Better error handling.
			if (pExistingAsset != assets[i] && pExistingAsset->GetGUID() == assets[i]->GetGUID() &&
				pExistingAsset->GetType() == assets[i]->GetType())
			{
				// TODO: Support of this is error prone. Probably we need something like an assignment operator.
				CEditableAsset editable(*pExistingAsset);

				m_sourceFilesTracker.SetIndex(assets[i]->GetSourceFile(), *pExistingAsset);
				editable.SetSourceFile(assets[i]->GetSourceFile());
				m_workFilesTracker.SetIndices(assets[i]->GetWorkFiles(), *pExistingAsset);
				editable.SetWorkFiles(assets[i]->GetWorkFiles());
				
				DeleteAssetFilesFromMap(m_fileToAssetMap, pExistingAsset, false);
				editable.SetFiles(assets[i]->GetFiles());
				AddAssetFilesToMap(m_fileToAssetMap, pExistingAsset, false);
				editable.SetLastModifiedTime(assets[i]->GetLastModifiedTime());
				editable.SetDependencies(assets[i]->GetDependencies());
				editable.SetDetails(assets[i]->GetDetails());

				// Force to update the thumbnail.
				if (!pExistingAsset->IsImmutable())
				{
					pPak->RemoveFile(pExistingAsset->GetThumbnailPath());
				}
				editable.InvalidateThumbnail();

				updatedAssets.push_back(pExistingAsset);
				delete assets[i];
				assets[i] = nullptr;

				// TODO: detect if list of files is changed, propagate the corresponding event eAssetChangeFlags_Files
				pExistingAsset->NotifyChanged(eAssetChangeFlags_All);
			}

			std::swap(assets[i], assets[--count]);

			// Do not increment index i.
			continue;
		}
		++i;
	}

	if (!updatedAssets.empty())
	{
		signalAssetsUpdated(updatedAssets);
	}

	if (count)
	{
		assets.resize(count);
		InsertAssets(assets);
	}
}

std::future<void> CAssetManager::DeleteAssetsWithFiles(std::vector<CAsset*> assets)
{
	using namespace Private_AssetManager;
	MAKE_SURE(!m_assets.empty(), return std::future<void>());
	MAKE_SURE(!assets.empty(), return std::future<void>());

	auto fileGroups = ComposeFileGroupsForDeletion(assets);

	m_orderedByGUID = false;

	for (auto pAsset : assets)
	{
		pAsset->GetType()->PreDeleteAssetFiles(*pAsset);
	}

	return ThreadingUtils::AsyncQueue([assets = std::move(assets), fileGroups = std::move(fileGroups), this]() mutable
	{
		CFileOperationExecutor::GetExecutor()->Delete(std::move(fileGroups));
	});
}

void CAssetManager::DeleteAssetsOnlyFromData(std::vector<CAsset*> assets)
{
	MAKE_SURE(!m_assets.empty(), return );
	MAKE_SURE(!assets.empty(), return );

	using namespace Private_AssetManager;

	assets.erase(std::remove_if(assets.begin(), assets.end(), [this](CAsset* pAsset)
	{
		auto it = std::find(m_assets.cbegin(), m_assets.cend(), pAsset);
		return it == m_assets.cend();
	}), assets.end());

	signalBeforeAssetsRemoved(assets);

	int newSize = m_assets.size();
	for (auto pAsset : assets)
	{
		pAsset->signalBeforeRemoved(pAsset);
		m_orderedByGUID = false;

		m_sourceFilesTracker.RemoveIndex(pAsset->GetSourceFile(), *pAsset);
		m_workFilesTracker.RemoveIndices(pAsset->GetWorkFiles(), *pAsset);  

		int index = std::distance(m_assets.cbegin(), std::find(m_assets.cbegin(), m_assets.cend(), pAsset));
		const CAssetPtr pAssetToDelete = std::move(m_assets[index]);
		m_assets[index] = std::move(m_assets[--newSize]);
		m_assets.resize(newSize);
		DeleteAssetFilesFromMap(m_fileToAssetMap, pAssetToDelete);
		pAsset->signalAfterRemoved(pAsset);
	}
	signalAfterAssetsRemoved();
}

void CAssetManager::MoveAssets(const std::vector<CAsset*>& assets, const char* szDestinationFolder) 
{
	using namespace Private_AssetManager;
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
		DeleteAssetFilesFromMap(m_fileToAssetMap, pAsset);
		pAsset->GetType()->MoveAsset(pAsset, szDestinationFolder, bMoveSourceFile);
		AddAssetFilesToMap(m_fileToAssetMap, pAsset);
		m_orderedByGUID = false;
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

	return GetSourceFilesTracker().GetIndexCount(asset.GetSourceFile()) > 1;
}

//Reimport is implemented in asset manager to avoid adding dependency from CAsset to CAssetImporter
void CAssetManager::Reimport(CAsset* pAsset)
{
	CRY_ASSERT(pAsset && pAsset->GetType() && !pAsset->IsImmutable());

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

std::vector<CAssetPtr> CAssetManager::GetAssetsFromDirectory(const string& directory, std::function<bool(CAsset*)> predicate /*={}*/) const
{
	std::vector<CAssetPtr> assets;
	assets.reserve(GetAssetsCount() / 8); // just a non zero assumption

	for (const CAssetPtr& pAsset : m_assets)
	{
		if (predicate && !predicate(pAsset))
		{
			continue;
		}

		if (directory.empty() || strncmp(directory.c_str(), pAsset->GetMetadataFile(), directory.size()) == 0)
		{
			assets.emplace_back(pAsset);
		}
	}
	return assets;
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

	// Log asset types.
	const char* szSeparator = ", ";
	const size_t pad = strlen(szSeparator);
	string types;
	types.reserve(m_assetTypes.size() * 25);
	for (const auto& element : m_assetTypes)
	{
		types.Append(element->GetUiTypeName());
		types.Append(szSeparator);
	}
	if (types.size() && pad)
	{
		types.Truncate(types.size() - pad);
	}
	GetIEditor()->GetSystem()->GetILog()->LogToFile("Asset Browser types: %s", types.c_str());
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

void CAssetManager::UpdateAssetConverters()
{
	std::vector<IClassDesc*> classes;
	GetIEditor()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_ASSET_COVERTER, classes);

	m_assetConverters.clear();
	m_assetConverters.reserve(classes.size());
	std::transform(classes.begin(), classes.end(), std::back_inserter(m_assetConverters), [](const IClassDesc* x) { return (CAssetConverter*)x; });
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

std::vector<std::pair<CAsset*, int32>> CAssetManager::GetReverseDependencies(const CAsset& asset) const
{
	MAKE_SURE(asset.GetFile(0), return {});

	std::vector<SAssetDependencyInfo> dependencyInfos = m_pDependencyTracker.get()->GetReverseDependencies(asset.GetFile(0));
	std::vector<std::pair<CAsset*, int32>> dependencies;
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
	for (CAsset* pAsset : ordered)
	{
		if (!pAsset)
		{
			continue;
		}
		std::vector<std::pair<CAsset*, int32>> tmp = GetReverseDependencies(*pAsset);
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
		if (pAsset->IsModified())
		{
			progress(float(i) / n);
		}
		pAsset->Save();
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
