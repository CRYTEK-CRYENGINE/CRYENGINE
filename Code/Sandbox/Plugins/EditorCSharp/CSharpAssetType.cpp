// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#include <StdAfx.h>
#include "CSharpAssetType.h"

#include <AssetSystem/Loader/AssetLoaderHelpers.h>
#include <AssetSystem/Asset.h>

#include <CrySystem/IProjectManager.h>

#include <cctype> 
#include <clocale>

REGISTER_ASSET_TYPE(CSharpSourcefileAssetType)

CryIcon CSharpSourcefileAssetType::GetIconInternal() const
{
	return CryIcon("icons:csharp/assettype.ico");
}

CAssetEditor* CSharpSourcefileAssetType::Edit(CAsset* pAsset) const
{
#if CRY_PLATFORM_WINDOWS
	if (pAsset->GetFilesCount() > 0)
	{
		string filePath = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute(), pAsset->GetFile(0));

		ShellExecute(0, 0, filePath.c_str(), 0, 0, SW_SHOW);
	}
#endif

	return nullptr;
}

bool CSharpSourcefileAssetType::OnCreate(CEditableAsset& editAsset, const void* pCreateParams) const
{
	const string basePath = PathUtil::RemoveExtension(PathUtil::RemoveExtension(editAsset.GetAsset().GetMetadataFile()));
	const string csFilePath = basePath + ".cs";
	const string assetName = PathUtil::GetFileName(basePath);

	string projectName = gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName();

	string cleanProjectName;
	cleanProjectName.reserve(projectName.size());
	for (int c : projectName)
	{
		if (std::isalpha(c) != 0)
		{
			cleanProjectName += c;
		}
	}

	string sourceContents = string().Format(
		"using System;\n"
		"using CryEngine;\n\n"
		"namespace %s\n"
		"{\n"
		"	public class %s : EntityComponent\n"
		"	{\n"
		"	}\n"
		"}", cleanProjectName.c_str(), assetName.c_str());

	CCryFile file(csFilePath.c_str(), "wt");
	if (file.GetHandle() != nullptr)
	{
		if (file.Write(sourceContents.c_str(), sourceContents.size()) != 0)
		{
			editAsset.SetFiles("", { csFilePath });
		}
	}

	return true;
}
