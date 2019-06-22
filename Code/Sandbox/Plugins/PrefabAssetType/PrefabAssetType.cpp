// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabAssetType.h"

#include <IEditor.h>
#include <Objects/ISelectionGroup.h>

// Prefab implementation detail
#include "Objects/ObjectManager.h"
#include "Objects/BaseObject.h"
#include <Objects/PrefabObject.h>
#include "Objects/SelectionGroup.h"

#include <Prefabs/PrefabItem.h>
#include <Prefabs/PrefabManager.h>
#include <IUndoObject.h>

#include <PathUtils.h>

REGISTER_ASSET_TYPE(CPrefabAssetType)

namespace Private_PrefabAssetType
{

CPrefabItem* const CreatePrefabItem(INewAsset& prefabAsset)
{
	const string dataFilePath = PathUtil::RemoveExtension(prefabAsset.GetMetadataFile());

	CPrefabManager* const pPrefabManager = GetIEditor()->GetPrefabManager();
	CPrefabItem* const pPrefabItem = (CPrefabItem*)pPrefabManager->CreateItem(dataFilePath);
	if (!pPrefabItem)
	{
		return nullptr;
	}

	pPrefabItem->SetName(prefabAsset.GetName());
	pPrefabItem->Update();
	pPrefabItem->GetLibrary()->Save();
	prefabAsset.SetGUID(pPrefabItem->GetGUID());
	prefabAsset.AddFile(pPrefabItem->GetLibrary()->GetFilename());

	return pPrefabItem;
}

}

CryIcon CPrefabAssetType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_prefab.ico");
}

string CPrefabAssetType::GetObjectFilePath(const CAsset* pAsset) const
{
	return pAsset ? pAsset->GetGUID().ToString() : string();
}

void CPrefabAssetType::PreDeleteAssetFiles(const CAsset& asset) const
{
	CPrefabManager* pManager = GetIEditor()->GetPrefabManager();
	IDataBaseItem* item = pManager->FindItem(asset.GetGUID());
	if (!item)
	{
		return;
	}
	pManager->DeleteItem(item);
}

bool CPrefabAssetType::OnCreate(INewAsset& asset, const SCreateParams* pCreateParams) const
{
	//First find out if we have creation params
	const SPrefabCreateParams* pPrefabCreateParams = pCreateParams ? static_cast<const SPrefabCreateParams*>(pCreateParams) : nullptr;
	
	//Then check out if we need to also create a Prefab object from a selection
	//Note that this is mainly for legacy support, an asset type should only create assets and item, then the calling code can instantiate a prefab object
	const ISelectionGroup* pSelectionToUse = nullptr;
	if (pPrefabCreateParams)
	{
		//if we have creation params try to use this selection 
		pSelectionToUse = pPrefabCreateParams->pNewMembersSelection; 
	}
	else
	{
		//if not use the global selection 
		pSelectionToUse = GetIEditor()->GetISelectionGroup();
	}

	//Create the item
	CPrefabItem* pNewItem = Private_PrefabAssetType::CreatePrefabItem(asset);

	//Create the prefab object
	//If item has been created and we have a valid selection proceed to create a new prefab object with the selection and serialize it to prefab
	if (pNewItem && pSelectionToUse && pSelectionToUse->GetCount())
	{
		CSelectionGroup selectionGroup;
		for (size_t i = 0, n = pSelectionToUse->GetCount(); i < n; ++i)
		{
			selectionGroup.AddObject(pSelectionToUse->GetObject(i));
		}

		pNewItem->MakeFromSelection(selectionGroup);
	}

	return pNewItem != nullptr;
}

bool CPrefabAssetType::OnCopy(INewAsset& asset, CAsset& assetToCopy) const
{
	CPrefabManager* const pManager = GetIEditor()->GetPrefabManager();

	if (!pManager->FindLibrary(assetToCopy.GetFile(0)))
	{
		pManager->LoadLibrary(assetToCopy.GetFile(0));
	}

	IDataBaseItem* const pPrefab = pManager->FindItem(assetToCopy.GetGUID());
	if (!pPrefab)
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR,
			"Unable to copy prefab with id \"%s\".\n"
			"Please make sure that the file \"%s\" exists and has the corresponding id.",
			assetToCopy.GetGUID().ToString().c_str(),
			assetToCopy.GetFile(0).c_str());
		return false;
	}

	std::unique_ptr<CPrefabItem> pNewPrefab(static_cast<CPrefabItem*>(pPrefab)->CreateCopy());
	if (!pNewPrefab)
	{
		return false;
	}
	pNewPrefab->SetName(asset.GetName());

	const auto dataFilePath = PathUtil::RemoveExtension(asset.GetMetadataFile());
	const auto libraryName = PathUtil::RemoveExtension(dataFilePath);
	IDataBaseLibrary* const pLibrary = pManager->AddLibrary(libraryName);
	if (!pLibrary)
	{
		return false;
	}

	asset.SetGUID(pNewPrefab->GetGUID());
	asset.AddFile(pLibrary->GetFilename());

	pLibrary->AddItem(pNewPrefab.release());
	return pLibrary->Save();
}

bool CPrefabAssetType::RenameAsset(CAsset* pAsset, const char* szNewName) const
{
	CPrefabManager* const pManager = GetIEditor()->GetPrefabManager();

	if (!pManager->FindLibrary(pAsset->GetFile(0)))
	{
		pManager->LoadLibrary(pAsset->GetFile(0));
	}

	IDataBaseItem* const pItem = pManager->FindItem(pAsset->GetGUID());
	if (!pItem)
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR,
			"Unable to rename prefab with id \"%s\".\n"
			"Please make sure that the file \"%s\" exists and has the corresponding id.",
			pAsset->GetGUID().ToString().c_str(),
			pAsset->GetFile(0).c_str());
		return false;
	}

	if (!CAssetType::RenameAsset(pAsset, szNewName))
	{
		return false;
	}

	pItem->SetName(pAsset->GetName());
	CBaseLibrary* const pLibrary = static_cast<CBaseLibrary*>(pItem->GetLibrary());
	pLibrary->SetFilename(pAsset->GetFile(0));
	pLibrary->SetName(PathUtil::RemoveExtension(pAsset->GetFile(0)));
	return pLibrary->Save();
}

