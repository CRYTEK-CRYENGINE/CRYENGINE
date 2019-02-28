// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Model.h"
#include <IEditor.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/AssetImporter.h>

CModel::CModel()
	: m_pAsset(nullptr)
{
	CAssetManager* pAssetManager = GetIEditor()->GetAssetManager();

	pAssetManager->signalBeforeAssetsInserted.Connect([this](const std::vector<CAsset*>&)
	{
		signalBeginChange();
	}, (uintptr_t)this);
	pAssetManager->signalAfterAssetsInserted.Connect([this](const std::vector<CAsset*>&)
	{
		signalEndChange();
	}, (uintptr_t)this);

	pAssetManager->signalBeforeAssetsUpdated.Connect([this]()
	{
		signalBeginChange();
	}, (uintptr_t)this);
	pAssetManager->signalAfterAssetsUpdated.Connect([this]()
	{
		signalEndChange();
	}, (uintptr_t)this);

	pAssetManager->signalBeforeAssetsRemoved.Connect([this](const std::vector<CAsset*>& assets)
	{
		signalBeginChange();
		if (std::find(assets.begin(), assets.end(), m_pAsset) != assets.end())
		{
		  m_pAsset = nullptr;
		}
	}, (uintptr_t)this);
	pAssetManager->signalAfterAssetsRemoved.Connect([this]()
	{
		signalEndChange();
	}, (uintptr_t)this);

	pAssetManager->signalAssetChanged.Connect([this](CAsset&)
	{
		signalBeginChange();
		signalEndChange();
	}, (uintptr_t)this);
}

CModel::~CModel()
{
	CAssetManager* pAssetManager = CAssetManager::GetInstance();

	pAssetManager->signalBeforeAssetsInserted.DisconnectById((uintptr_t)this);
	pAssetManager->signalAfterAssetsInserted.DisconnectById((uintptr_t)this);

	pAssetManager->signalBeforeAssetsUpdated.DisconnectById((uintptr_t)this);
	pAssetManager->signalAfterAssetsUpdated.DisconnectById((uintptr_t)this);

	pAssetManager->signalBeforeAssetsRemoved.DisconnectById((uintptr_t)this);
	pAssetManager->signalAfterAssetsRemoved.DisconnectById((uintptr_t)this);

	pAssetManager->signalAssetChanged.DisconnectById((uintptr_t)this);
}

void CModel::OpenAsset(CAsset* pAsset)
{
	signalBeginChange();
	CRY_ASSERT(pAsset);
	m_pAsset = pAsset;
	signalEndChange();
}

const std::vector<CAssetType*>& CModel::GetSupportedTypes() const
{
	return GetIEditor()->GetAssetManager()->GetAssetTypes();
}

const CAssetType* CModel::FindAssetTypeByFile(const char* szFile) const
{
	const char* szExt = PathUtil::GetExt(szFile);

	// Expects the path without "cryasset" extension.
	CRY_ASSERT(stricmp(szExt, "cryasset") != 0);

	auto types = GetSupportedTypes();
	auto it = std::find_if(types.begin(), types.end(), [szExt](const CAssetType* p)
	{
		return stricmp(szExt, p->GetFileExtension()) == 0;
	});
	return it != types.end() ? *it : nullptr;
}

const std::vector<string> CModel::GetSourceFileExtensions(const CAssetType* assetType) const
{
	const std::vector<CAssetImporter*>& importers = GetIEditor()->GetAssetManager()->GetAssetImporters();
	for (const CAssetImporter* pImporter : importers)
	{
		std::vector<string> types = pImporter->GetAssetTypes();
		if (std::find(types.begin(), types.end(), assetType->GetTypeName()) != types.end())
		{
			return pImporter->GetFileExtensions();
		}
	}
	return {};
}

