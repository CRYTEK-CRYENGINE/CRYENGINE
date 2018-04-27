// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetLoaderHelpers.h"
#include "Metadata.h"
#include <AssetSystem/Asset.h>
#include <AssetSystem/EditableAsset.h>

#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/IChunkFile.h>  
#include <Cry3DEngine/CGF/CGFContent.h>
#include <CryString/CryPath.h>

namespace AssetLoader
{

string GetAssetName(const char* szPath)
{
	const char* szExt = nullptr;
	const char* szFile = szPath;
	for (; *szPath; ++szPath)
	{
		// Asset name begins with the position past the last directory separator.
		if (*szPath == '/' || *szPath == '\\')
		{
			szFile = szPath;
			++szFile;
			szExt = nullptr;
		}

		// Asset name ends at the position of the first extension separator.
		if (!szExt && *szPath == '.')
		{
			szExt = szPath;
		}
	}

	return szExt ? string(szFile, szExt - szFile) : string(szFile);
}

CAsset* CAssetFactory::LoadAssetFromXmlFile(const char* szAssetPath)
{
	ICryPak* const pPak = GetISystem()->GetIPak();
	FILE* pFile = pPak->FOpen(szAssetPath, "rbx");
	if (!pFile)
	{
		return nullptr;
	}

	std::vector<char> buffer(pPak->FGetSize(pFile));
	const uint64 timestamp = pPak->GetModificationTime(pFile);
	const size_t numberOfBytesRead = pPak->FReadRawAll(buffer.data(), buffer.size(), pFile);
	pPak->FClose(pFile);

	return CAssetFactory::LoadAssetFromInMemoryXmlFile(szAssetPath, timestamp, buffer.data(), numberOfBytesRead);
}

CAsset* CAssetFactory::LoadAssetFromInMemoryXmlFile(const char* szAssetPath, uint64 timestamp, const char* pBuffer, size_t numberOfBytes)
{
	AssetLoader::SAssetMetadata metadata;
	{
		XmlNodeRef container = GetISystem()->LoadXmlFromBuffer(pBuffer, numberOfBytes);
		if (!container || !AssetLoader::ReadMetadata(container, metadata))
		{
			return nullptr;
		}
	}

	const string path = PathUtil::GetPathWithoutFilename(szAssetPath);

	CryGUID guid = (metadata.guid != CryGUID::Null()) ? metadata.guid : CryGUID::Create();

	CAsset* const pAsset = new CAsset(metadata.type, guid, GetAssetName(szAssetPath));

	CEditableAsset editableAsset(*pAsset);
	editableAsset.SetLastModifiedTime(timestamp);
	editableAsset.SetFiles(path, metadata.files);
	editableAsset.SetMetadataFile(szAssetPath);
	editableAsset.SetDetails(metadata.details);

	// Source file may be next to the asset or relative to the assets root directory.
	if (!metadata.sourceFile.empty() && !strpbrk(metadata.sourceFile.c_str(), "/\\"))
	{
		metadata.sourceFile = PathUtil::Make(path, metadata.sourceFile);
	}
	editableAsset.SetSourceFile(metadata.sourceFile);

	// Make dependency paths relative to the assets root directory.
	{
		std::vector<SAssetDependencyInfo> dependencies;
		std::transform(metadata.dependencies.begin(), metadata.dependencies.end(), std::back_inserter(dependencies), [&path](const auto& x) -> SAssetDependencyInfo
		{
			if (strncmp("./", x.first.c_str(), 2) == 0)
			{
				return { PathUtil::Make(path.c_str(), x.first.c_str() + 2), x.second };
			}
			else
			{
				return { x.first, x.second };
			}
		});

		// This is a quick solution to fix dependency lookup for tif files.
		// TODO: Cryasset file should never have dependencies on source files of other assets.
		for (auto& item : dependencies)
		{
			if (stricmp(PathUtil::GetExt(item.path.c_str()), "tif") == 0)
			{
				item.path = PathUtil::ReplaceExtension(item.path.c_str(), "dds");
			}
		}

		editableAsset.SetDependencies(dependencies);
	}

	return pAsset;

}

std::vector<CAsset*> CAssetFactory::LoadAssetsFromPakFile(const char* szArchivePath, std::function<bool(const char* szAssetRelativePath, uint64 timestamp)> predicate)
{
	std::vector<CAsset*> assets;
	assets.reserve(1024);

	std::vector<char> buffer(1 << 24);

	std::stack<string> stack;
	stack.push("");
	while (!stack.empty())
	{
		const CryPathString mask = PathUtil::Make(stack.top(), "*");
		const CryPathString folder = stack.top();
		stack.pop();

		GetISystem()->GetIPak()->ForEachArchiveFolderEntry(szArchivePath, mask, [&stack, &folder, &buffer, &assets, predicate](const ICryPak::ArchiveEntryInfo& entry)
		{
			const CryPathString path(PathUtil::Make(folder.c_str(), entry.szName));
			if (entry.bIsFolder)
			{
				stack.push(path);
				return;
			}

			if (stricmp(PathUtil::GetExt(path), "cryasset") != 0)
			{
				return;
			}

			ICryPak* const pPak = GetISystem()->GetIPak();

			auto closeFile = [pPak](FILE* f) { return pPak->FClose(f); };
			std::unique_ptr<FILE, decltype(closeFile)> file(pPak->FOpen(path, "rbx"), closeFile);
			if (!file)
			{
				return;
			}

			const uint64 timestamp = pPak->GetModificationTime(file.get());
			if (!predicate(path.c_str(), timestamp))
			{
				return;
			}

			buffer.resize(pPak->FGetSize(file.get()));
			const size_t numberOfBytesRead = pPak->FReadRawAll(buffer.data(), buffer.size(), file.get());
			CAsset* pAsset = LoadAssetFromInMemoryXmlFile(path.c_str(), timestamp, buffer.data(), numberOfBytesRead);
			if (pAsset)
			{
				assets.push_back(pAsset);
			}
		});
	}

	return assets;
}

} // namespace AssetLoader

