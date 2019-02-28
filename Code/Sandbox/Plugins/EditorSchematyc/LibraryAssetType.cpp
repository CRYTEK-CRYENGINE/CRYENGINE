// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LibraryAssetType.h"

#include "SchematycUtils.h"

#include <AssetSystem/EditableAsset.h>
#include <QtUtil.h>

namespace CrySchematycEditor {

REGISTER_ASSET_TYPE(CLibraryAssetType)

bool CLibraryAssetType::OnCreate(CEditableAsset& editAsset, const void* /*pTypeSpecificParameter*/) const
{
	const string szFilePath = PathUtil::RemoveExtension(PathUtil::RemoveExtension(editAsset.GetAsset().GetMetadataFile()));
	const QString basePath = szFilePath.c_str();
	const QString assetName = basePath.section('/', -1);

	// TODO: Actually the backend should ensure that the name is valid!
	Schematyc::CStackString uniqueAssetName = QtUtil::ToString(assetName).c_str();
	MakeScriptElementNameUnique(uniqueAssetName);
	// ~TODO

	Schematyc::IScriptRegistry& scriptRegistry = gEnv->pSchematyc->GetScriptRegistry();
	Schematyc::IScriptModule* pModule = scriptRegistry.AddModule(uniqueAssetName.c_str(), szFilePath.c_str());
	if (pModule)
	{
		if (Schematyc::IScript* pScript = pModule->GetScript())
		{
			const QString assetFilePath = basePath + "." + GetFileExtension();
			editAsset.AddFile(QtUtil::ToString(assetFilePath).c_str());

			scriptRegistry.SaveScript(*pScript);
			return true;
		}
	}

	return false;
}

CAssetEditor* CLibraryAssetType::Edit(CAsset* pAsset) const
{
	return CAssetEditor::OpenAssetForEdit("Schematyc Editor (Experimental)", pAsset);
}

bool CLibraryAssetType::RenameAsset(CAsset* pAsset, const char* szNewName) const
{
	if (pAsset)
	{
		Schematyc::IScript* pScript = GetScript(*pAsset);
		if (pScript)
		{
			stack_string prevName = pAsset->GetName();
			if (CAssetType::RenameAsset(pAsset, szNewName))
			{
				stack_string filePath = pScript->GetFilePath();
				filePath.replace(prevName.c_str(), szNewName);

				gEnv->pSchematyc->GetScriptRegistry().OnScriptRenamed(*pScript, filePath);
				return true;
			}
		}
	}

	return false;
}

bool CLibraryAssetType::DeleteAssetFiles(const CAsset& asset, bool bDeleteSourceFile, size_t& numberOfFilesDeleted) const
{
	if (CAssetType::DeleteAssetFiles(asset, bDeleteSourceFile, numberOfFilesDeleted))
	{
		Schematyc::IScript* pScript = GetScript(asset);
		if (pScript)
		{
			gEnv->pSchematyc->GetScriptRegistry().RemoveElement(pScript->GetRoot()->GetGUID());
		}

		return true;
	}
	return false;
}

CryIcon CLibraryAssetType::GetIconInternal() const
{
	return CryIcon("icons:schematyc/assettype_library.ico");
}

Schematyc::IScript* CLibraryAssetType::GetScript(const CAsset& asset) const
{
	ICrySchematycCore* pSchematycCore = gEnv->pSchematyc;
	if (pSchematycCore)
	{
		Schematyc::IScriptRegistry& scriptRegistry = pSchematycCore->GetScriptRegistry();
		const size_t fileCount = asset.GetFilesCount();
		CRY_ASSERT_MESSAGE(fileCount == 1, "Asset '%s' has an unexpected amount of script files.", asset.GetName());
		if (fileCount > 0)
		{
			const stack_string szFile = PathUtil::GetGameFolder() + "/" + string(asset.GetFile(0));
			return scriptRegistry.GetScriptByFileName(szFile);
		}
	}
	return nullptr;
}
}

