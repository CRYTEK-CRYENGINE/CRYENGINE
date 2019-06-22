// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "VersionControlInitializer.h"
#include "VersionControl.h"
#include "VersionControlPathUtils.h"
#include "VersionControlEventHandler.h"
#include "AssetsVCSStatusProvider.h"
#include "AssetFilesProvider.h"
#include "DeletedWorkFilesStorage.h"
#include "UI/VersionControlMenuBuilder.h"
#include "UI/VersionControlUIHelper.h"
#include "AssetSystem/Browser/AssetModel.h"
#include "AssetSystem/FileOperationsExecutor.h"
#include "Notifications/NotificationCenter.h"
#include "IObjectManager.h"
#include "Objects/IObjectLayerManager.h"
#include "Objects/IObjectLayer.h"
#include "ThreadingUtils.h"
#include "PathUtils.h"

namespace Private_VersionControlInitializer
{

CVersionControlMenuBuilder menuBuilder;

void NotifyAboutAddedFiles(const std::vector<string>& files, const QString& itemsName)
{
	auto numAdded = std::count_if(files.cbegin(), files.cend(), [](const string& file)
	{
		return CAssetsVCSStatusProvider::HasStatus(file, CVersionControlFileStatus::eState_AddedLocally);
	});
	if (numAdded > 0)
	{
		GetIEditor()->GetNotificationCenter()->ShowInfo("Version Control System notification", 
			QString("%1 new %2 were added").arg(QString::number(numAdded), itemsName));
	}
}

void AddAllWorkFiles()
{
	std::vector<string> filesToAdd;
	CAssetManager::GetInstance()->GetWorkFilesTracker().ForEachIndex([&filesToAdd](const string& file, int count) mutable
	{
		if (count == 0)
		{
			return;
		}
		if (CAssetsVCSStatusProvider::HasStatus(file, CVersionControlFileStatus::eState_NotTracked))
		{
			filesToAdd.push_back(file);
		}
	});

	VersionControlPathUtils::MatchCaseAndRemoveUnmatched(filesToAdd);

	if (!filesToAdd.empty())
	{
		auto OnAddCallback = [filesToAdd](const auto& result) { NotifyAboutAddedFiles(filesToAdd, "work files"); };
		CVersionControl::GetInstance().AddFiles(std::move(filesToAdd), false, std::move(OnAddCallback));
	}
}

void AddAllAssets()
{
	std::vector<CAsset*> assets;
	assets.reserve(CAssetManager::GetInstance()->GetAssetsCount());
	CAssetManager::GetInstance()->ForeachAsset([&assets](CAsset* pAsset)
	{
		if (pAsset->GetMetadataFile()[0] != '%')
		{
			assets.push_back(pAsset);
		}
	});
	
	ThreadingUtils::Async([assets = std::move(assets)]() mutable
	{
		std::vector<string> metadataFiles = CAssetFilesProvider::GetForAssets(assets, CAssetFilesProvider::EInclude::OnlyMainFile);

		// filter out all files that are tracked leaving only not tracked.
		metadataFiles.erase(std::remove_if(metadataFiles.begin(), metadataFiles.end(), [](const string& metadataFile)
		{
			if (metadataFile.empty())
				return true;
			return !CAssetsVCSStatusProvider::HasStatus(metadataFile, CVersionControlFileStatus::eState_NotTracked);
		}), metadataFiles.end());

		assets.clear();
		for (const string& metadataFile : metadataFiles)
		{
			assets.push_back(CAssetManager::GetInstance()->FindAssetForMetadata(metadataFile));
		}

		if (assets.empty())
		{
			return;
		}

		CVersionControl::GetInstance().AddFiles(CAssetFilesProvider::GetForAssets(assets), false
			, [metadataFiles = std::move(metadataFiles)](const auto& result)
		{
			NotifyAboutAddedFiles(metadataFiles, "assets");
		});
	});
}

void OnDeletedWorkFilesStorageSaved()
{
	using FS = CVersionControlFileStatus;
	const CDeletedWorkFilesStorage& storage = CDeletedWorkFilesStorage::GetInstance();
	std::vector<string> filesToDelete;
	std::vector<string> filesToRevert;
	for (int i = 0; i < storage.GetSize(); ++i)
	{
		const string& file = storage.GetFile(i);
		if (CAssetsVCSStatusProvider::HasStatus(file, FS::eState_AddedLocally))
		{
			filesToRevert.push_back(file);
		}
		else if (!CAssetsVCSStatusProvider::HasStatus(file, FS::eState_DeletedLocally | FS::eState_NotTracked))
		{
			filesToDelete.push_back(file);
		}
	}
	if (!filesToRevert.empty())
	{
		CVersionControl::GetInstance().Revert(std::move(filesToRevert));
	}
	if (!filesToDelete.empty())
	{
		CFileOperationExecutor::GetExecutor()->AsyncDelete(filesToDelete);
	}
}

void UpdateStatusIfOnline()
{
	CVersionControl& vcs = CVersionControl::GetInstance();
	if (vcs.IsOnline() && !GetIEditor()->GetAssetManager()->IsScanning())
	{
		vcs.UpdateStatus(false, [](const auto& result)
		{
			AddAllAssets();
			AddAllWorkFiles();
			OnDeletedWorkFilesStorageSaved();
		});
	}
}

void OnAssetsAdded(const std::vector<CAsset*>& assets)
{
	CVersionControl::GetInstance().AddFiles(CAssetFilesProvider::GetForAssets(assets));
}

void OnAssetChanged(CAsset& asset, int flags)
{
	if (CVersionControl::GetInstance().IsOnline() && (flags & eAssetChangeFlags_Files))
	{
		// TODO: this method needs to be called only when there are new files, but ATM flag eACF_Files is not set by AssetManager.
		CVersionControl::GetInstance().AddFiles(CAssetFilesProvider::GetForAssets({ &asset }, CAssetFilesProvider::EInclude::AllButMainFile));
		// TODO: there should be a check if any files from the asset has been deleted
		// for this the old version of the cryasset needs to be compared with the new one.
		// in this case we definitely need to know that the asset was changed due to underlying list of files change.
	}
}

void OnAssetsUpdated(const std::vector<CAsset*>& assets)
{
	CDeletedWorkFilesStorage().GetInstance().Save();
}

void OnBeforeAssetsRemoved(const std::vector<CAsset*>& assets)
{
	for (CAsset* pAsset : assets)
	{
		for (const string& file : pAsset->GetWorkFiles())
		{
			if (GetIEditor()->GetAssetManager()->GetWorkFilesTracker().GetIndexCount(file) <= 1)
			{
				CDeletedWorkFilesStorage::GetInstance().Add(file);
			}
		}
	}
}

void OnLayerSaved(IObjectLayer& layer)
{
	if (CVersionControl::GetInstance().IsOnline())
	{
		const auto& layerFile = PathUtil::MakeGamePath(layer.GetLayerFilepath());
		if (CAssetsVCSStatusProvider::HasStatus(layerFile, CVersionControlFileStatus::eState_NotTracked))
		{
			CVersionControl::GetInstance().AddFiles(CAssetFilesProvider::GetForLayers({ &layer }));
		}
		else if (CAssetsVCSStatusProvider::HasStatus(layerFile, CVersionControlFileStatus::eState_DeletedLocally))
		{
			auto files = CAssetFilesProvider::GetForLayers({ &layer });
			CVersionControl::GetInstance().ClearLocalState(files, {}, false);
			CVersionControl::GetInstance().EditFiles(std::move(files));
		}
	}
}

void AddAssetsThumbnailIconProvider()
{
	CAssetModel::GetInstance()->AddThumbnailIconProvider("vcs", [](const CAsset* pAsset)
	{
		if (!CVersionControl::GetInstance().IsOnline())
		{
			return QIcon();
		}

		return VersionControlUIHelper::GetIconFromStatus(CAssetsVCSStatusProvider::GetStatus(*pAsset));
	});
}

void RemoveAssetsThunbnailIconProvider()
{
	CAssetModel::GetInstance()->RemoveThumbnailIconProvider("vcs");
}

class CEventsListener
{
public:
	void Activate()
	{
		using namespace Private_VersionControlInitializer;

		auto id = reinterpret_cast<uintptr_t>(this);

		auto pAssetManager = GetIEditor()->GetAssetManager();
		pAssetManager->signalAssetChanged.Connect(&OnAssetChanged, id);
		if (pAssetManager->IsScanning())
		{
			pAssetManager->signalScanningCompleted.Connect([id]()
			{
				GetIEditor()->GetAssetManager()->signalAfterAssetsInserted.Connect(&OnAssetsAdded, id);
				GetIEditor()->GetAssetManager()->signalScanningCompleted.DisconnectById(id);
				UpdateStatusIfOnline();
			}, id);
		}
		else
		{
			GetIEditor()->GetAssetManager()->signalAfterAssetsInserted.Connect(&OnAssetsAdded, id);
		}

		pAssetManager->signalAssetsUpdated.Connect(&OnAssetsUpdated, id);

		GetIEditor()->GetObjectManager()->GetIObjectLayerManager()->signalLayerSaved.Connect(&OnLayerSaved, id);

		AddAssetsThumbnailIconProvider();

		menuBuilder.Activate();

		CVersionControlEventHandler::Activate();

		CDeletedWorkFilesStorage::GetInstance().Load();

		CDeletedWorkFilesStorage::GetInstance().signalSaved.Connect(&OnDeletedWorkFilesStorageSaved, id);

		CVersionControl::GetInstance().signalOnlineChanged.Connect(&UpdateStatusIfOnline, id);
	}

	void Deactivate()
	{
		auto id = reinterpret_cast<uintptr_t>(this);

		auto pAssetManager = GetIEditor()->GetAssetManager();
		pAssetManager->signalAfterAssetsInserted.DisconnectById(id);
		pAssetManager->signalAssetChanged.DisconnectById(id);
		pAssetManager->signalScanningCompleted.DisconnectById(id);
		pAssetManager->signalAssetsUpdated.DisconnectById(id);

		GetIEditor()->GetObjectManager()->GetIObjectLayerManager()->signalLayerSaved.DisconnectById(id);

		RemoveAssetsThunbnailIconProvider();

		CVersionControlEventHandler::Deactivate();

		menuBuilder.Deactivate();

		CDeletedWorkFilesStorage::GetInstance().signalSaved.DisconnectById(id);

		CVersionControl::GetInstance().signalOnlineChanged.DisconnectById(id);
	}
};

static CEventsListener g_eventsListener;

void AddAssetsStatusColumn()
{
	CAssetModel::GetInstance()->AddColumn(VersionControlUIHelper::GetVCSIconAttribute(), [](const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role) -> QVariant
	{
		if (!CVersionControl::GetInstance().IsOnline() || role != Qt::DecorationRole)
		{
			return QVariant();
		} 

		return VersionControlUIHelper::GetIconFromStatus(CAssetsVCSStatusProvider::GetStatus(*pAsset));
	});

	CAssetModel::GetInstance()->AddColumn(VersionControlUIHelper::GetVCSStatusAttribute(), [](const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)
	{
		if (!CVersionControl::GetInstance().IsOnline() || role != VersionControlUIHelper::GetVCSStatusRole())
		{
			return QVariant();
		}

		return QVariant(CAssetsVCSStatusProvider::GetStatus(*pAsset));
	});
}

}

void CVersionControlInitializer::ActivateListeners()
{
	using namespace Private_VersionControlInitializer;
	g_eventsListener.Activate();
}

void CVersionControlInitializer::DeactivateListeners()
{
	using namespace Private_VersionControlInitializer;
	g_eventsListener.Deactivate();
}

void CVersionControlInitializer::Initialize()
{
	using namespace Private_VersionControlInitializer;
	static bool isInitialized = false;
	if (!isInitialized)
	{
		AddAssetsStatusColumn();
		isInitialized = true;
	}
}
