// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "LevelAssetType.h"

#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/DependencyTracker.h"
#include "AssetSystem/Loader/AssetLoaderHelpers.h"

#include "CryEdit.h"
#include "CryEditDoc.h"
#include "CrySandbox/ScopedVariableSetter.h"
#include "CryString/CryPath.h"
#include "GameExporter.h"
#include "IEditorImpl.h"
#include "LevelEditor.h" // CLevelEditor::tr
#include "LevelEditor/NewLevelDialog.h"
#include "LevelFileUtils.h"
#include "LevelIndependentFileMan.h"
#include "Objects/ObjectLayerManager.h"
#include "PathUtils.h"
#include "Prefabs/PrefabManager.h"
#include "Preferences/GeneralPreferences.h"
#include "QtUtil.h"
#include "ThreadingUtils.h"

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>

#include <Controls/QuestionDialog.h>
#include <QDirIterator> 

REGISTER_ASSET_TYPE(CLevelType)

namespace Private_LevelAssetType
{

string GetLevelFolderFromPath(const char* szLevelMetadataPath)
{
	CRY_ASSERT(AssetLoader::IsMetadataFile(szLevelMetadataPath));

	// Level is a folder, and the cryasset is next to the folder. 
	// Getting the path to the level folder is actually removing the extension twice.
	return PathUtil::RemoveExtension(PathUtil::RemoveExtension(szLevelMetadataPath));
}

template <typename TAsset>
string GetLevelFolder(const TAsset& level)
{
	return GetLevelFolderFromPath(level.GetMetadataFile());
}

bool IsLevel(const char* szLevelMetadataPath)
{
	if (!AssetLoader::IsMetadataFile(szLevelMetadataPath))
	{
		return false;
	}

	const CryPathString dataFilePath = PathUtil::RemoveExtension(szLevelMetadataPath);
	return stricmp(PathUtil::GetExt(dataFilePath.c_str()), CLevelType::GetFileExtensionStatic()) == 0;
}

std::vector<SAssetDependencyInfo> GetDependencies(const IEditableAsset& level)
{
	const string levelFolder = GetLevelFolder(level);

	const CAssetManager* const pAssetManager = GetIEditorImpl()->GetAssetManager();
	std::vector<CAssetType*> types(pAssetManager->GetAssetTypes());
	types.erase(std::remove_if(types.begin(), types.end(), [](const auto x)
	{
		return strcmp(x->GetTypeName(), "Level") == 0 || strcmp(x->GetTypeName(), "Environment") == 0;
	}), types.end());

	std::sort(types.begin(), types.end(), [](const auto a, const auto b)
	{
		return strcmp(a->GetFileExtension(), b->GetFileExtension()) < 0;
	});

	std::vector<SAssetDependencyInfo> dependencies;
	dependencies.reserve(32);

	const string gameFolder(PathUtil::AddSlash(gEnv->pCryPak->GetGameFolder()));
	const string assetPath(PathUtil::AddSlash(PathUtil::GetGameProjectAssetsPath()));

	IResourceList* pResList = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level);
	for (const char* szFilename = pResList->GetFirst(); szFilename; szFilename = pResList->GetNext())
	{
		// Make relative to the game folder
		if (gEnv->pCryPak->IsAbsPath(szFilename))
		{
			if (strnicmp(szFilename, assetPath.c_str(), assetPath.size()) != 0)
			{
				// Ignore data files located outside the asset folder
				continue;
			}
			szFilename += assetPath.size();
		}
		else if (strnicmp(szFilename, gameFolder.c_str(), gameFolder.size()) == 0)
		{
			szFilename += gameFolder.size();
		}

		// Ignore file types that are not registered as assets.
		const char* const szExt = PathUtil::GetExt(szFilename);
		const auto it = std::lower_bound(types.begin(), types.end(), szExt, [](const auto a, const auto b)
		{
			return stricmp(a->GetFileExtension(), b) < 0;
		});
		if (it == types.end() || stricmp((*it)->GetFileExtension(), szExt))
		{
			continue;
		}

		// Ignore content of the level folder
		if (strnicmp(szFilename, levelFolder.c_str(), levelFolder.size()) == 0)
		{
			continue;
		}

		dependencies.emplace_back(szFilename,0);
	}

	// We use a special handling of env presets because GetResourceList may not catch them.
	ITimeOfDay* pTimeOfDay = GetIEditorImpl()->Get3DEngine()->GetTimeOfDay();
	const int presetCount = pTimeOfDay->GetPresetCount();
	if (presetCount)
	{
		dependencies.reserve(dependencies.size() + presetCount);
		std::vector<ITimeOfDay::SPresetInfo> presetInfos(presetCount);
		pTimeOfDay->GetPresetsInfos(presetInfos.data(), presetCount);
		std::transform(presetInfos.cbegin(), presetInfos.cend(), std::back_inserter(dependencies), [](auto& preset)
		{
			return SAssetDependencyInfo(preset.szName, 1);
		});
	}

	// Filter sub-dependencies out. 
	std::sort(dependencies.begin(), dependencies.end(), [](const auto& a, const auto& b) 
	{
		return a.path < b.path;
	});

	CAssetType* const pPrefabType = pAssetManager->FindAssetType("Prefab");
	CPrefabManager* const pPrefabManager = GetIEditorImpl()->GetPrefabManager();

	for (size_t i = 0, N = dependencies.size(); i < N; ++i)
	{
		if (dependencies[i].path.empty())
		{
			continue;
		}

		const CAsset* pSubAsset = pAssetManager->FindAssetForFile(dependencies[i].path);
		if (!pSubAsset)
		{
			continue;
		}

		const std::vector<SAssetDependencyInfo>& subDependencies = pSubAsset->GetDependencies();
		for (const auto& item : subDependencies)
		{
			const char* szFilename = item.path.c_str();
			const auto it = std::lower_bound(dependencies.begin(), dependencies.end(), szFilename, [](const auto a, const auto b)
			{
				return stricmp(a.path.c_str(), b) < 0;
			});

			if (it != dependencies.end() && it->path.Compare(szFilename) == 0)
			{
				it->usageCount = -1;
			}
		}

		if (pSubAsset->GetType() == pPrefabType)
		{
			IDataBaseItem* const pPrefabItem = pPrefabManager->FindItem(pSubAsset->GetGUID());
			if (pPrefabItem)
			{
				dependencies[i].usageCount = pPrefabManager->GetPrefabInstanceCount(pPrefabItem);
			}
		}
	}

	dependencies.erase(std::remove_if(dependencies.begin(), dependencies.end(), [](const auto x) 
	{
		return x.usageCount == -1;
	}), dependencies.end());

	return dependencies;
}

}

CAssetEditor* CLevelType::Edit(CAsset* pAsset) const
{
	// Editing the level type presents a special case, as it does not return an asset editor.
	// Instead we load the level.
	auto levelFullPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetFile(0));
	CCryEditApp::GetInstance()->LoadLevel(PathUtil::AbsolutePathToCryPakPath(levelFullPath));

	return nullptr;
}

std::vector<string> CLevelType::GetAssetFiles(const CAsset& asset, bool includeSourceFile, bool makeAbsolute, bool includeThumbnail, bool includeDerived) const
{
	std::vector<string> files = CAssetType::GetAssetFiles(asset, includeSourceFile, makeAbsolute, includeThumbnail, includeDerived);

	if (makeAbsolute)
	{
		files.push_back(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::GetDirectory(asset.GetFile(0))));
	}
	else
	{
		files.push_back(PathUtil::GetDirectory(asset.GetFile(0)));
	}

	return files;
}

bool CLevelType::OnValidateAssetPath(const char* szFilepath, /*out*/string& reasonToReject) const
{
	using namespace Private_LevelAssetType;

	if (!IsLevel(szFilepath))
	{
		const QString assetFolder = QtUtil::ToQString(PathUtil::GetDirectory(szFilepath));

		if (LevelFileUtils::IsPathToLevel(assetFolder) || LevelFileUtils::IsAnyParentPathLevel(assetFolder))
		{
			reasonToReject = QT_TR_NOOP("Assets can not be located inside a data folder of a level.");
			return false;
		}
	}
	else // szFilepath points to a level
	{
		const QString levelDataFolder = QtUtil::ToQString(GetLevelFolderFromPath(szFilepath));

		if (LevelFileUtils::IsAnyParentPathLevel(levelDataFolder))
		{
			reasonToReject = QT_TR_NOOP("Level can not be located inside a data folder of another level.");
			return false;
		}

		if (LevelFileUtils::IsAnySubFolderLevel(levelDataFolder))
		{
			reasonToReject = QT_TR_NOOP("Level can not be located in a folder with sub-folders that contain levels.");
			return false;
		}
	}

	return true;
}

string CLevelType::MakeLevelFilename(const char* szAssetName)
{
	return PathUtil::Make(szAssetName, PathUtil::GetFile(szAssetName), GetFileExtensionStatic());
}

void CLevelType::PreDeleteAssetFiles(const CAsset& asset) const
{
	const string activeLevelPath = PathUtil::GetDirectory(GetIEditorImpl()->GetDocument()->GetPathName());
	const string levelPath = PathUtil::Make(PathUtil::GetGameFolder(), Private_LevelAssetType::GetLevelFolder(asset));
	if (activeLevelPath.CompareNoCase(levelPath) == 0)
	{
		GetIEditorImpl()->GetSystem()->GetIPak()->ClosePacks(PathUtil::Make(activeLevelPath, "*.*"));
	}
}

CryIcon CLevelType::GetIconInternal() const
{
	return CryIcon("icons:FileType/Level.ico");
}

void CLevelType::UpdateFilesAndDependencies(IEditableAsset& editAsset)
{
	UpdateFiles(editAsset);
	UpdateDependencies(editAsset);
}

void CLevelType::UpdateDependencies(IEditableAsset& editAsset)
{
	std::vector<SAssetDependencyInfo> dependencies = Private_LevelAssetType::GetDependencies(editAsset);
	if (dependencies.size())
	{
		editAsset.SetDependencies(std::move(dependencies));
	}
}

void CLevelType::UpdateFiles(IEditableAsset& editAsset)
{
	using namespace Private_LevelAssetType;

	const string levelFolder = GetLevelFolder(editAsset);
	const string filename = string().Format("%s/%s.%s", levelFolder.c_str(), PathUtil::GetFile(levelFolder).c_str(), GetFileExtensionStatic());
	editAsset.SetFiles({ filename });
}

bool CLevelType::OnCreate(INewAsset& editAsset, const SCreateParams* pCreateParams) const
{
	using namespace Private_LevelAssetType;

	SLevelCreateParams params;

	if (pCreateParams)
	{
		params = *static_cast<const SLevelCreateParams*>(pCreateParams);
	}
	else
	{
		bool isProceed = GetIEditorImpl()->GetDocument()->CanClose();
		if (!isProceed)
		{
			return false;
		}

		if (!GetIEditorImpl()->GetLevelIndependentFileMan()->PromptChangedFiles())
		{
			return false;
		}

		CNewLevelDialog dialog;
		if (dialog.exec() != QDialog::Accepted)
		{
			return false;
		}
		params = dialog.GetResult();
	}

	const string levelFolder = GetLevelFolder(editAsset);
	
	auto createResult = CCryEditApp::GetInstance()->CreateLevel(levelFolder, params.resolution, params.unitSize, params.bUseTerrain);

	switch (createResult)
	{
	case CCryEditApp::ECLR_OK:
		break;

	case CCryEditApp::ECLR_DIR_CREATION_FAILED:
			{
			const auto errorCode = GetLastError();
			const QString errorText = qt_error_string(errorCode);
			const QString dir = QtUtil::ToQString(levelFolder);
			const QString messageText = CLevelEditor::tr("Failed to create level directory: %1\nError: %1").arg(dir).arg(errorText);

			CQuestionDialog::SWarning(CLevelEditor::tr("New Level failed"), messageText);
			return false;
			}
	default:
		CQuestionDialog::SWarning(CLevelEditor::tr("New Level failed"), CLevelEditor::tr("Unknown error"));
		return false;
	}

	if (params.bUseTerrain)
	{
		// Temporarily disable auto backup.
		CScopedVariableSetter<bool> autoBackupEnabledChange(gEditorFilePreferences.autoSaveEnabled, false);

		SGameExporterSettings settings;
		settings.iExportTexWidth = params.texture.resolution;
		settings.bHiQualityCompression = params.texture.bHighQualityCompression;
		settings.fBrMultiplier = params.texture.colorMultiplier;
		settings.eExportEndian = eLittleEndian;

		CGameExporter gameExporter(settings);

		auto exportFlags = eExp_SurfaceTexture | eExp_ReloadTerrain;
		gameExporter.Export(exportFlags, ".");
	}

	UpdateFilesAndDependencies(editAsset);
	return true;
}
