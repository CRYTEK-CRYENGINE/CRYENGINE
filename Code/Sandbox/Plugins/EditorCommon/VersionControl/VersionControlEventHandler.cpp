// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "VersionControl/VersionControlEventHandler.h"
#include "VersionControl/AssetsVCSStatusProvider.h"
#include "VersionControl/AssetsVCSSynchronizer.h"
#include "VersionControl/AssetsVCSCheckerOut.h"
#include "VersionControl/AssetsVCSReverter.h"
#include "VersionControl/VersionControl.h"
#include "VersionControl/AssetFilesProvider.h"
#include "VersionControl/VersionControlPathUtils.h"
#include "VersionControl/UI/VersionControlSubmissionPopup.h"

#include "EditorFramework/WidgetsGlobalActionRegistry.h"
#include "AssetSystem/Browser/IAssetBrowserContext.h"
#include "LevelEditor/ILevelExplorerContext.h"

#include "Objects/IObjectLayer.h"
#include "PathUtils.h"

namespace Private_VersionControlEvenHandler
{

void InsertAssetsFromFolders(const std::vector<string>& folders, std::vector<CAsset*>& assets)
{
	for (const string& folder : folders)
	{
		const auto& folderAssets = CAssetManager::GetInstance()->GetAssetsFromDirectory(folder, [](CAsset* pAsset)
		{
			return pAsset->GetMetadataFile()[0] != '%';
		});
		assets.reserve(assets.size() + folderAssets.size());
		for (const auto& pAsset : folderAssets)
		{
			assets.push_back(pAsset);
		}
	}
}

std::vector<IObjectLayer*> GetTrackedIObjectLayers(const std::vector<IObjectLayer*>& layers)
{
	std::vector<IObjectLayer*> result;
	std::copy_if(layers.cbegin(), layers.cend(), std::back_inserter(result), [](IObjectLayer* pLayer)
	{
		return !CAssetsVCSStatusProvider::HasStatus(*pLayer, CVersionControlFileStatus::eState_NotTracked);
	});
	return result;
}

std::vector<string> ToSelectedFolders(const std::vector<IObjectLayer*>& layerFolders, bool hasSelectedLayer)
{
	std::vector<string> result;
	result.reserve(layerFolders.size());
	for (IObjectLayer* pFolderLayer : layerFolders)
	{
		result.push_back(PathUtil::MakeGamePath(pFolderLayer->GetLayerFilepath()));
	}

	if (result.empty() && !hasSelectedLayer)
	{
		result = { PathUtil::MakeGamePath(GetIEditor()->GetLevelPath()) };
	}

	return result;
}

void GetAssetBrowserSelection(const IAssetBrowserContext* pContext, std::vector<CAsset*>& assets, std::vector<string>& folders)
{
	pContext->GetSelection(assets, folders);
	const bool hasMainViewSelection = !assets.empty() || !folders.empty();
	if (!hasMainViewSelection)
	{
		folders = pContext->GetSelectedFolders();
	}
}

std::vector<string> GetAssetBrowsersWorkFiles(const IAssetBrowserContext* pContext)
{
	std::vector<string> workFiles;
	std::vector<CAsset*> assets;
	std::vector<string> folders;
	GetAssetBrowserSelection(pContext, assets, folders);
	if (assets.size() == 1 && folders.empty() && !assets[0]->GetWorkFiles().empty())
	{
		workFiles = assets[0]->GetWorkFiles();
		VersionControlPathUtils::MatchCaseAndRemoveUnmatched(workFiles);
	}
	return workFiles;
}

void OnAssetBrowserRefresh(const IAssetBrowserContext* pContext)
{
	std::vector<CAsset*> assets;
	std::vector<string> folders;
	Private_VersionControlEvenHandler::GetAssetBrowserSelection(pContext, assets, folders);
	CAssetsVCSStatusProvider::UpdateStatus(assets, folders);
} 

void OnAssetBrowserGetLatest(const IAssetBrowserContext* pContext)
{
	std::vector<CAsset*> assets;
	std::vector<string> folders;
	Private_VersionControlEvenHandler::GetAssetBrowserSelection(pContext, assets, folders);
	CAssetsVCSSynchronizer::Sync(assets, std::move(folders));
} 

void OnAssetBrowserCheckOut(const IAssetBrowserContext* pContext)
{
	std::vector<CAsset*> assets;
	std::vector<string> folders;
	Private_VersionControlEvenHandler::GetAssetBrowserSelection(pContext, assets, folders);
	Private_VersionControlEvenHandler::InsertAssetsFromFolders(folders, assets);
	CAssetsVCSCheckerOut::CheckOut(assets);
} 

void OnAssetBrowserRevert(const IAssetBrowserContext* pContext)
{
	std::vector<CAsset*> assets;
	std::vector<string> folders;
	Private_VersionControlEvenHandler::GetAssetBrowserSelection(pContext, assets, folders);
	CAssetsVCSReverter::RevertAssets(assets, folders);
} 

void OnAssetBrowserRevertUnchanged(const IAssetBrowserContext* pContext)
{
	std::vector<CAsset*> assets;
	std::vector<string> folders;
	Private_VersionControlEvenHandler::GetAssetBrowserSelection(pContext, assets, folders);
	CVersionControl::GetInstance().ClearLocalState(CAssetFilesProvider::GetForAssets(assets), std::move(folders), true);
} 

void OnAssetBrowserSubmit(const IAssetBrowserContext* pContext)
{
	std::vector<CAsset*> assets;
	std::vector<string> folders;
	Private_VersionControlEvenHandler::GetAssetBrowserSelection(pContext, assets, folders);
	CVersionControlSubmissionPopup::ShowPopup(assets, folders);
} 

void OnAssetBrowserRefreshWorkFiles(const IAssetBrowserContext* pContext)
{
	auto workFiles = Private_VersionControlEvenHandler::GetAssetBrowsersWorkFiles(pContext);
	if (!workFiles.empty())
	{
		CVersionControl::GetInstance().UpdateFileStatus(std::move(workFiles));
	}
} 

void OnAssetBrowserGetLatestWorkFiles(const IAssetBrowserContext* pContext)
{
	auto workFiles = Private_VersionControlEvenHandler::GetAssetBrowsersWorkFiles(pContext);
	if (!workFiles.empty())
	{
		CVersionControl::GetInstance().GetLatest(std::move(workFiles), {});
	}
} 

void OnAssetBrowserCheckOutWorkFiles(const IAssetBrowserContext* pContext)
{
	auto workFiles = Private_VersionControlEvenHandler::GetAssetBrowsersWorkFiles(pContext);
	if (!workFiles.empty())
	{
		CVersionControl::GetInstance().EditFiles(std::move(workFiles));
	}
} 

void OnAssetBrowserRevertWorkFiles(const IAssetBrowserContext* pContext)
{
	auto workFiles = Private_VersionControlEvenHandler::GetAssetBrowsersWorkFiles(pContext);
	if (!workFiles.empty())
	{
		CVersionControl::GetInstance().Revert(std::move(workFiles));
	}
} 

void OnAssetBrowserRevertUnchangedWorkFiles(const IAssetBrowserContext* pContext)
{
	auto workFiles = Private_VersionControlEvenHandler::GetAssetBrowsersWorkFiles(pContext);
	if (!workFiles.empty())
	{
		CVersionControl::GetInstance().ClearLocalState(std::move(workFiles), {}, true);
	}
} 

void OnAssetBrowserSubmitWorkFiles(const IAssetBrowserContext* pContext)
{
	auto workFiles = Private_VersionControlEvenHandler::GetAssetBrowsersWorkFiles(pContext);
	if (!workFiles.empty())
	{
		CVersionControlSubmissionPopup::ShowPopup(std::move(workFiles));
	}
} 

void OnAssetBrowserRemoveLocalWorkFiles(const IAssetBrowserContext* pContext)
{
	auto workFiles = Private_VersionControlEvenHandler::GetAssetBrowsersWorkFiles(pContext);
	if (!workFiles.empty() && CQuestionDialog::SQuestion(QObject::tr("Warning"),
		QObject::tr("Are you sure you want to remove local work files?")) == QDialogButtonBox::Yes)
	{
		CVersionControl::GetInstance().RemoveFilesLocally(workFiles);
	}
} 

void OnLevelExplorerRefresh(const ILevelExplorerContext* pContext)
{
	using namespace Private_VersionControlEvenHandler;
	CAssetsVCSStatusProvider::UpdateStatus(GetTrackedIObjectLayers(pContext->GetSelectedIObjectLayers()));
}

void OnLevelExplorerGetLatest(const ILevelExplorerContext* pContext)
{
	using namespace Private_VersionControlEvenHandler;
	std::vector<IObjectLayer*> layers;
	std::vector<IObjectLayer*> layerFolders;
	pContext->GetSelection(layers, layerFolders);
	CAssetsVCSSynchronizer::Sync(GetTrackedIObjectLayers(layers), ToSelectedFolders(
		layerFolders, !layers.empty()));
}

void OnLevelExplorerCheckOut(const ILevelExplorerContext* pContext)
{
	using namespace Private_VersionControlEvenHandler;
	CAssetsVCSCheckerOut::CheckOut(GetTrackedIObjectLayers(pContext->GetSelectedIObjectLayers()));
}

void OnLevelExplorerRevert(const ILevelExplorerContext* pContext)
{
	using namespace Private_VersionControlEvenHandler;
	CAssetsVCSReverter::RevertLayers(GetTrackedIObjectLayers(pContext->GetSelectedIObjectLayers()));
}

void OnLevelExplorerRevertUnchanged(const ILevelExplorerContext* pContext)
{
	using namespace Private_VersionControlEvenHandler;
	CVersionControl::GetInstance().ClearLocalState(CAssetFilesProvider::GetForLayers(
		GetTrackedIObjectLayers(pContext->GetSelectedIObjectLayers())), {}, true);
}

void OnLevelExplorerSubmit(const ILevelExplorerContext* pContext)
{
	using namespace Private_VersionControlEvenHandler;
	std::vector<IObjectLayer*> layers;
	std::vector<IObjectLayer*> layerFolders;
	pContext->GetSelection(layers, layerFolders);
	CVersionControlSubmissionPopup::ShowPopup(CAssetFilesProvider::GetForLayers(
		GetTrackedIObjectLayers(layers)), ToSelectedFolders(layerFolders, !layers.empty()));
}

}

void CVersionControlEventHandler::Activate()
{
	using namespace Private_VersionControlEvenHandler;
	auto id = reinterpret_cast<uintptr_t>(&CVersionControlEventHandler::Activate);
	auto& registry = CWidgetsGlobalActionRegistry::GetInstance();
	registry.Register("Asset Browser", "version_control_system.refresh",                     ToStdFunction(&OnAssetBrowserRefresh), id);
	registry.Register("Asset Browser", "version_control_system.get_latest",                  ToStdFunction(&OnAssetBrowserGetLatest), id);
	registry.Register("Asset Browser", "version_control_system.check_out",                   ToStdFunction(&OnAssetBrowserCheckOut), id);
	registry.Register("Asset Browser", "version_control_system.revert",                      ToStdFunction(&OnAssetBrowserRevert), id);
	registry.Register("Asset Browser", "version_control_system.revert_unchanged",            ToStdFunction(&OnAssetBrowserRevertUnchanged), id);
	registry.Register("Asset Browser", "version_control_system.submit",                      ToStdFunction(&OnAssetBrowserSubmit), id);
	registry.Register("Asset Browser", "version_control_system.refresh_work_files",          ToStdFunction(&OnAssetBrowserRefreshWorkFiles), id);
	registry.Register("Asset Browser", "version_control_system.get_latest_work_files",       ToStdFunction(&OnAssetBrowserGetLatestWorkFiles), id);
	registry.Register("Asset Browser", "version_control_system.check_out_work_files",        ToStdFunction(&OnAssetBrowserCheckOutWorkFiles), id);
	registry.Register("Asset Browser", "version_control_system.revert_work_files",           ToStdFunction(&OnAssetBrowserRevertWorkFiles), id);
	registry.Register("Asset Browser", "version_control_system.revert_unchanged_work_files", ToStdFunction(&OnAssetBrowserRevertUnchangedWorkFiles), id);
	registry.Register("Asset Browser", "version_control_system.submit_work_files",           ToStdFunction(&OnAssetBrowserSubmitWorkFiles), id);
	registry.Register("Asset Browser", "version_control_system.remove_local_work_files",     ToStdFunction(&OnAssetBrowserRemoveLocalWorkFiles), id);

	registry.Register("Level Explorer", "version_control_system.refresh",          ToStdFunction(&OnLevelExplorerRefresh), id);
	registry.Register("Level Explorer", "version_control_system.get_latest",       ToStdFunction(&OnLevelExplorerGetLatest), id);
	registry.Register("Level Explorer", "version_control_system.check_out",        ToStdFunction(&OnLevelExplorerCheckOut), id);
	registry.Register("Level Explorer", "version_control_system.revert",           ToStdFunction(&OnLevelExplorerRevert), id);
	registry.Register("Level Explorer", "version_control_system.revert_unchanged", ToStdFunction(&OnLevelExplorerRevertUnchanged), id);
	registry.Register("Level Explorer", "version_control_system.submit",           ToStdFunction(&OnLevelExplorerSubmit), id);
}

void CVersionControlEventHandler::Deactivate()
{
	using namespace Private_VersionControlEvenHandler;
	auto id = reinterpret_cast<uintptr_t>(&CVersionControlEventHandler::Activate);
	auto& registry = CWidgetsGlobalActionRegistry::GetInstance();
	registry.Unregister("Asset Browser", "version_control_system.refresh", id);
	registry.Unregister("Asset Browser", "version_control_system.get_latest", id);
	registry.Unregister("Asset Browser", "version_control_system.check_out", id);
	registry.Unregister("Asset Browser", "version_control_system.revert", id);
	registry.Unregister("Asset Browser", "version_control_system.revert_unchanged", id);
	registry.Unregister("Asset Browser", "version_control_system.submit", id);
	registry.Unregister("Asset Browser", "version_control_system.refresh_work_files", id);
	registry.Unregister("Asset Browser", "version_control_system.get_latest_work_files", id);
	registry.Unregister("Asset Browser", "version_control_system.check_out_work_files", id);
	registry.Unregister("Asset Browser", "version_control_system.revert_work_files", id);
	registry.Unregister("Asset Browser", "version_control_system.revert_unchanged_work_files", id);
	registry.Unregister("Asset Browser", "version_control_system.submit_work_files", id);
	registry.Unregister("Asset Browser", "version_control_system.remove_local_work_files", id);

	registry.Unregister("Level Explorer", "version_control_system.refresh", id);
	registry.Unregister("Level Explorer", "version_control_system.get_latest", id);
	registry.Unregister("Level Explorer", "version_control_system.check_out", id);
	registry.Unregister("Level Explorer", "version_control_system.revert", id);
	registry.Unregister("Level Explorer", "version_control_system.revert_unchanged", id);
	registry.Unregister("Level Explorer", "version_control_system.submit", id);
}
