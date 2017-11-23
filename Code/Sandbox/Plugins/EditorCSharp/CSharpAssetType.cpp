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

string CSharpSourcefileAssetType::GetCleanName(const string& name) const
{
	string cleanName;
	cleanName.reserve(name.size());
	for (int c : name)
	{
		if (std::isalpha(c) != 0)
		{
			cleanName += c;
		}
		else if (std::isdigit(c) != 0)
		{
			cleanName += c;
		}
	}
	return cleanName;
}

CAssetEditor* CSharpSourcefileAssetType::Edit(CAsset* pAsset) const
{
#if CRY_PLATFORM_WINDOWS
	if (pAsset->GetFilesCount() > 0)
	{
		string filePath = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute(), pAsset->GetFile(0));
		CCSharpEditorPlugin::GetInstance()->OpenCSharpFile(filePath, 10);
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

	string cleanProjectName = GetCleanName(projectName);
	string cleanAssetName = GetCleanName(assetName);
	CryGUID guid = CryGUID::Create();

	string sourceContents = string().Format(
		"using System;\n"
		"using CryEngine;\n\n"
		"namespace %s\n"
		"{\n"
		"	[EntityComponent(Guid=\"%s\")]\n"
		"	public class %s : EntityComponent\n"
		"	{\n"
		"		/// <summary>\n"
		"		/// Called at the start of the game.\n"
		"		/// </summary>\n"
		"		protected override void OnGameplayStart()\n"
		"		{\n\n"
		"		}\n\n"
		"		/// <summary>\n"
		"		/// Called once every frame when the game is running.\n"
		"		/// </summary>\n"
		"		/// <param name=\"frameTime\">The time difference between this and the previous frame.</param>\n"
		"		protected override void OnUpdate(float frameTime)\n"
		"		{\n\n"
		"		}\n"
		"	}\n"
		"}", cleanProjectName.c_str(), guid.ToString().c_str(), cleanAssetName.c_str());

	CCryFile file(csFilePath.c_str(), "wt", ICryPak::FLAGS_NO_LOWCASE);

	if (file.GetHandle() != nullptr)
	{
		if (file.Write(sourceContents.c_str(), sourceContents.size()) != 0)
		{
			editAsset.SetFiles("", { csFilePath });
		}
	}

	return true;
}
