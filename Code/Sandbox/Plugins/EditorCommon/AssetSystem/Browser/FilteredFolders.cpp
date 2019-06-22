// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FilteredFolders.h"

#include "AssetFolderFilterModel.h"
#include "AssetFoldersModel.h"
#include "AssetSystem/AssetManager.h"
#include "ProxyModels/AttributeFilterProxyModel.h"

void CFilteredFolders::Update(bool enable)
{
	m_nonEmptyFolders.clear();

	if (enable)
	{
		FillNonEmptyFoldersList();
	}

	signalInvalidate();
}

void CFilteredFolders::FillNonEmptyFoldersList()
{
	CAssetFolderFilterModel* const pModel = m_pFolderFilterModel;

	// Select all possible root folders to collect a complete list of assets that have passed the filtering criteria
	QStringList rootFolders;
	const QModelIndex assetsRootIndex = CAssetFoldersModel::GetInstance()->GetProjectAssetsFolderIndex();
	rootFolders += assetsRootIndex.data((int)CAssetFoldersModel::Roles::FolderPathRole).toString();
	for (auto alias : CAssetManager::GetInstance()->GetAliases())
	{
		rootFolders += alias;
	}

	const bool showFolders = pModel->IsShowingFolders();
	const bool isRecursive = pModel->IsRecursive();
	const QStringList folders = pModel->GetAcceptedFolders();

	pModel->SetShowFolders(false);
	pModel->SetRecursive(true);
	pModel->SetAcceptedFolders(rootFolders);

	const int assetsCount = m_pAttributeFilterModel->rowCount();

	m_filteredAssets.clear();
	m_filteredAssets.reserve(assetsCount);
	for (int i = 0; i < assetsCount; ++i)
	{
		const CAsset* pAsset = CAssetModel::ToAsset(m_pAttributeFilterModel->index(i, 0));
		m_filteredAssets.push_back(pAsset);
	}

	std::sort(m_filteredAssets.begin(), m_filteredAssets.end(), [](const CAsset* a, const CAsset* b)
	{
		return a->GetFolder() < b->GetFolder();
	});

	auto uniquePathsEnd = std::unique(m_filteredAssets.begin(), m_filteredAssets.end(), [](const CAsset* a, const CAsset* b)
	{
		return a->GetFolder() == b->GetFolder();
	});

	m_nonEmptyFolders.reserve(uniquePathsEnd - m_filteredAssets.begin());
	std::transform(m_filteredAssets.begin(), uniquePathsEnd, std::back_inserter(m_nonEmptyFolders), [](const CAsset* a)
	{
		return QtUtil::ToQString(a->GetFolder());
	});

	pModel->SetShowFolders(showFolders);
	pModel->SetRecursive(isRecursive);
	pModel->SetAcceptedFolders(folders);
}
