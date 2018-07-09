// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetType.h"

#include "Asset.h"
#include "AssetImportContext.h"
#include "AssetImporter.h"
#include "AssetManager.h"
#include "Loader/AssetLoaderHelpers.h"

#include "FilePathUtil.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "QtUtil.h"
#include <QFile>

#include <CryString/CryPath.h>

namespace Private_AssetType
{

// Tries to find oldNamePrefix in the file-part of the oldFilepath and replace it to the newNamePrefix. Returns the updated path in newFilepath.
// Returns true if succeeded, false if the oldNamePrefix does not found in the old file name.
static bool TryReplaceFilename(const string& oldNamePrefix, const string& newNamePrefix, const string& oldFilepath, string& newFilepath)
{
	const char* szFileName = PathUtil::GetFile(oldFilepath.c_str());
	if (strnicmp(szFileName, oldNamePrefix.c_str(), oldNamePrefix.size()) != 0)
	{
		return false;
	}
	newFilepath = oldFilepath;
	const size_t pos = szFileName - oldFilepath.c_str();
	newFilepath.replace(pos, oldNamePrefix.size(), newNamePrefix.c_str(), newNamePrefix.size());
	return true;
}

// Rename asset file. Expects paths relative to the assets root directory.
static bool RenameAssetFile(const string& oldFilename, const string& newFilename)
{
	const string oldFilepath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), oldFilename);
	const string newFilepath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), newFilename);

	if (!QFileInfo(QtUtil::ToQString(oldFilepath)).exists())
	{
		return true;
	}

	if (QFile::rename(QtUtil::ToQString(oldFilepath.c_str()), QtUtil::ToQString(newFilepath.c_str())))
	{
		return true;
	}

	CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to rename asset file %s", oldFilename.c_str());
	return false;
}

// Copy asset file. Expects paths relative to the assets root directory.
static bool CopyAssetFile(const string& oldFilename, const string& newFilename)
{
	const string oldFilepath = PathUtil::ToUnixPath(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), oldFilename));
	const string newFilepath = PathUtil::ToUnixPath(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), newFilename));

	if (oldFilepath.CompareNoCase(newFilepath) == 0)
	{
		return true;
	}

	if (!QFileInfo(QtUtil::ToQString(oldFilepath)).exists())
	{
		return true;
	}

	if (QFile::copy(QtUtil::ToQString(oldFilepath.c_str()), QtUtil::ToQString(newFilepath.c_str())))
	{
		return true;
	}

	CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to copy asset file %s to %s", oldFilename.c_str(), newFilepath.c_str());
	return false;
}

}

void CAssetType::Init()
{
	m_icon = GetIconInternal();
}

QVariant CAssetType::GetVariantFromDetail(const string& detailValue, const CItemModelAttribute* pAttrib)
{
	CRY_ASSERT(pAttrib);
	const QVariant v(QtUtil::ToQString(detailValue));
	switch (pAttrib->GetType())
	{
	case eAttributeType_String:
		return v.value<QString>();
	case eAttributeType_Int:
		return v.value<int>();
	case eAttributeType_Float:
		return v.value<float>();
	case eAttributeType_Boolean:
		return v.value<bool>();
	case eAttributeType_Enum:
		return v.value<int>();
	default:
		CRY_ASSERT(0 && "Unknown attribute type");
		return v;
	}
}

bool CAssetType::Create(const char* szFilepath, const void* pTypeSpecificParameter) const
{
	if (!QDir().mkpath(QtUtil::ToQString(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::GetPathWithoutFilename(szFilepath)))))
	{
		return false;
	}

	CAsset* const pNewAsset(new CAsset(GetTypeName(), CryGUID::Create(), AssetLoader::GetAssetName(szFilepath)));
	CEditableAsset editAsset(*pNewAsset);
	editAsset.SetMetadataFile(szFilepath);

	if (!OnCreate(editAsset, pTypeSpecificParameter))
	{
		return false;
	}

	editAsset.WriteToFile();

	CAssetManager::GetInstance()->MergeAssets({ pNewAsset });

	return true;
}

CryIcon CAssetType::GetIcon() const
{
	return m_icon;
}

CryIcon CAssetType::GetIconInternal() const
{
	return CryIcon("icons:General/Placeholder.ico");
}

QWidget* CAssetType::CreatePreviewWidget(CAsset* pAsset, QWidget* pParent /*= nullptr*/) const
{
	//default implementation will not work for all asset types, using the existing preview widget
	CRY_ASSERT(pAsset);
	if (pAsset->GetFilesCount() > 0)
	{
		QString file(pAsset->GetFile(0));
		if (!file.isEmpty())
		{
			return GetIEditor()->CreatePreviewWidget(file, pParent);
		}
	}

	return nullptr;
}

std::vector<CAsset*> CAssetType::Import(const string& sourceFilePath, const string& outputDirectory, const string& overrideAssetName) const
{
	string ext(PathUtil::GetExt(sourceFilePath.c_str()));
	ext.MakeLower();
	CAssetImporter* pAssetImporter = CAssetManager::GetInstance()->GetAssetImporter(ext, GetTypeName());
	if (!pAssetImporter)
	{
		return {};
	}
	CAssetImportContext ctx;
	ctx.SetInputFilePath(sourceFilePath);
	ctx.SetOutputDirectoryPath(outputDirectory);
	ctx.OverrideAssetName(overrideAssetName);
	return pAssetImporter->Import({ GetTypeName() }, ctx);
}

std::vector<string> CAssetType::GetAssetFiles(const CAsset& asset, bool includeSourceFile, bool makeAbsolute/* = false*/) const
{
	std::vector<string> files;
	files.reserve(asset.GetFilesCount() + 3);
	for (size_t i = 0, N = asset.GetFilesCount(); i < N; ++i)
	{
		files.emplace_back(asset.GetFile(i));
	}
	files.emplace_back(asset.GetMetadataFile());
	files.emplace_back(asset.GetThumbnailPath());
	if (includeSourceFile)
	{
		files.emplace_back(asset.GetSourceFile());
	}

	files.erase(std::remove_if(files.begin(), files.end(), [](const string& filename)
	{
		return filename.empty();
	}), files.end());

	if (makeAbsolute)
	{
		const string assetsPath(PathUtil::GetGameProjectAssetsPath());
		for (string& filename : files)
		{
			filename = PathUtil::Make(assetsPath, filename);
		}
	}

	return files;
}

bool CAssetType::DeleteAssetFiles(const CAsset& asset, bool bDeleteSourceFile, size_t& numberOfFilesDeleted) const
{
	numberOfFilesDeleted = 0;

	if (asset.IsReadOnly())
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to delete asset \"%s\": the asset is read only.", asset.GetName());
		return false;
	}

	bool bReadOnly = false;
	std::vector<string> filesToRemove;

	{
		const std::vector<string> files(GetAssetFiles(asset, bDeleteSourceFile, true));
		filesToRemove.reserve(files.size());
		for (const string& path : files)
		{
			QFileInfo fileInfo(QtUtil::ToQString(path));
			if (!fileInfo.exists())
			{
				if (GetISystem()->GetIPak()->IsFileExist(PathUtil::AbsolutePathToGamePath(path), ICryPak::eFileLocation_InPak))
				{
					CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to delete asset. The file is in pak: \"%s\"", path.c_str());
					bReadOnly = true;
				}
				continue;
			}
			else if (!fileInfo.isWritable())
			{
				CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to delete asset. The file is read-only: \"%s\"", path.c_str());
				bReadOnly = true;
				continue;
			}

			filesToRemove.push_back(path);
		}
	}

	if (bReadOnly)
	{
		return false;
	}

	if (filesToRemove.empty())
	{
		// Expected result already reached.
		return true;
	}

	for (const string& file : filesToRemove)
	{
		if (QFile::remove(QtUtil::ToQString(file.c_str())))
		{
			++numberOfFilesDeleted;
		}
		else
		{
			GetISystem()->GetILog()->LogWarning("Can not delete asset file %s", file.c_str());
		}
	}

	return numberOfFilesDeleted == filesToRemove.size();
}

bool CAssetType::RenameAsset(CAsset* pAsset, const char* szNewName) const
{
	const string oldName(pAsset->GetName());
	const string newName(szNewName);
	if (oldName == newName)
	{
		return true;
	}

	if (pAsset->IsReadOnly())
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to rename asset \"%s\": the asset is read only.", pAsset->GetName());
		return false;
	}

	bool bReadOnly = false;
	{
		std::vector<string> filepaths(GetAssetFiles(*pAsset, false, true));
		for (string& path : filepaths)
		{
			QFileInfo fileInfo(QtUtil::ToQString(path));
			if (!fileInfo.exists())
			{
				if (GetISystem()->GetIPak()->IsFileExist(PathUtil::AbsolutePathToGamePath(path), ICryPak::eFileLocation_InPak))
				{
					CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to rename asset. The file is in pak: \"%s\"", path.c_str());
					bReadOnly = true;
				}
			}
			else if (!fileInfo.isWritable())
			{
				CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to rename asset. The file is read-only: \"%s\"", path.c_str());
				bReadOnly = true;
			}
		}
	}

	if (bReadOnly)
	{
		return false;
	}

	const string oldMetadataFile = pAsset->GetMetadataFile();
	string newMetadataFile;
	if (!Private_AssetType::TryReplaceFilename(oldName, newName, oldMetadataFile, newMetadataFile))
	{
		// Should newer happen.
		CRY_ASSERT_MESSAGE(0, "The asset name \"%s\" differs from the metadata filename \"%s\"", pAsset->GetName(), pAsset->GetMetadataFile());
		return false;
	}

	CEditableAsset editableAsset(*pAsset);
	if (!Private_AssetType::RenameAssetFile(oldMetadataFile, newMetadataFile))
	{
		return false;
	}

	//  SetMetadataFile will update the asset thumbnail path, so save the old one.
	const string oldThumbnailPath(pAsset->GetThumbnailPath());

	editableAsset.SetMetadataFile(newMetadataFile);
	Private_AssetType::RenameAssetFile(oldThumbnailPath, pAsset->GetThumbnailPath());

	std::vector<string> files = pAsset->GetFiles();
	for (string& file : files)
	{
		string newFile;
		if (Private_AssetType::TryReplaceFilename(oldName, newName, file, newFile))
		{
			if (Private_AssetType::RenameAssetFile(file, newFile))
			{
				file = newFile;
			}
		}
	}
	editableAsset.SetFiles(nullptr, files);
	editableAsset.SetName(szNewName);
	// The asset file monitor will handle this as if the asset had been modified.
	// See also CAssetManager::CAssetFileMonitor.
	editableAsset.WriteToFile();
	return true;
}

bool CAssetType::MoveAsset(CAsset* pAsset, const char* szDestinationFolder, bool bMoveSourcefile) const
{
	if (pAsset->IsReadOnly())
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to move asset \"%s\": the asset is read only.", pAsset->GetName());
		return false;
	}

	bool bReadOnly = false;
	{
		std::vector<string> filepaths(GetAssetFiles(*pAsset, bMoveSourcefile, true));
		for (string& path : filepaths)
		{
			QFileInfo fileInfo(QtUtil::ToQString(path));
			if (!fileInfo.exists())
			{
				if (GetISystem()->GetIPak()->IsFileExist(PathUtil::AbsolutePathToGamePath(path), ICryPak::eFileLocation_InPak))
				{
					CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to move asset. The file is in pak: \"%s\"", path.c_str());
					bReadOnly = true;
				}
			}
			else if (!fileInfo.isWritable())
			{
				CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to move asset. The file is read-only: \"%s\"", path.c_str());
				bReadOnly = true;
			}
		}
	}

	if (bReadOnly)
	{
		return false;
	}

	if (!QDir().mkpath(QtUtil::ToQString(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), szDestinationFolder))))
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable create target directory: \"%s\"", szDestinationFolder);
		return false;
	}

	const string oldMetadataFile = pAsset->GetMetadataFile();
	const string newMetadataFile = PathUtil::Make(szDestinationFolder, PathUtil::GetFile(pAsset->GetMetadataFile()));
	if (!Private_AssetType::RenameAssetFile(oldMetadataFile, newMetadataFile))
	{
		return false;
	}

	const string oldThumbnailPath(pAsset->GetThumbnailPath());

	CEditableAsset editableAsset(*pAsset);
	editableAsset.SetMetadataFile(newMetadataFile);

	Private_AssetType::RenameAssetFile(oldThumbnailPath, pAsset->GetThumbnailPath());

	// Copy or move the source. 
	if (pAsset->GetSourceFile() && *pAsset->GetSourceFile())
	{
		const string oldSourceFile = pAsset->GetSourceFile();
		const string newSourceFile = PathUtil::Make(szDestinationFolder, PathUtil::GetFile(pAsset->GetSourceFile()));

		if (bMoveSourcefile)
		{
			if (Private_AssetType::RenameAssetFile(oldSourceFile, newSourceFile))
			{
				editableAsset.SetSourceFile(newSourceFile);
			}
		}
		else
		{
			if (Private_AssetType::CopyAssetFile(oldSourceFile, newSourceFile))
			{
				editableAsset.SetSourceFile(newSourceFile);
			}
		}
	}

	std::vector<string> files = pAsset->GetFiles();
	for (string& file : files)
	{
		string newFile = PathUtil::Make(szDestinationFolder, PathUtil::GetFile(file));
		if (Private_AssetType::RenameAssetFile(file, newFile))
		{
			file = newFile;
		}
	}
	editableAsset.SetFiles(nullptr, files);
	editableAsset.WriteToFile();
	return true;
}

bool CAssetType::CopyAsset(CAsset* pAsset, const char* szNewPath) const
{
	CRY_ASSERT(pAsset);

	const string name(pAsset->GetName());
	const string newName(AssetLoader::GetAssetName(szNewPath));
	const string destinationFolder = PathUtil::GetDirectory(szNewPath);
	const bool bCopyToAnotherFolder = destinationFolder.CompareNoCase(pAsset->GetFolder()) != 0;

	if (bCopyToAnotherFolder && !QDir().mkpath(QtUtil::ToQString(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), destinationFolder))))
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable create target directory: \"%s\"", destinationFolder.c_str());
		return false;
	}

	// Create a new instance of asset with a unique guid.
	std::unique_ptr<CAsset> pNewAsset(new CAsset(GetTypeName(), CryGUID::Create(), newName));
	CEditableAsset editableAsset(*pNewAsset);
	editableAsset.SetMetadataFile(szNewPath);

	// Copy source file.
	if (pAsset->GetSourceFile() && *pAsset->GetSourceFile())
	{
		if (bCopyToAnotherFolder)
		{
			const string oldSourceFile = pAsset->GetSourceFile();
			const string newSourceFile = PathUtil::Make(destinationFolder, PathUtil::GetFile(pAsset->GetSourceFile()));

			if (!Private_AssetType::CopyAssetFile(oldSourceFile, newSourceFile))
			{
				return false;
			}

			editableAsset.SetSourceFile(newSourceFile);
		}
		else
		{
			editableAsset.SetSourceFile(pAsset->GetSourceFile());
		}
	}

	// Try to copy thumbnail file.
	Private_AssetType::CopyAssetFile(pAsset->GetThumbnailPath(), pNewAsset->GetThumbnailPath());

	// Copy data files.
	std::vector<string> files = pAsset->GetFiles();
	for (string& file : files)
	{
		string newFile;
		if (Private_AssetType::TryReplaceFilename(name, newName, file, newFile))
		{
			newFile = PathUtil::Make(destinationFolder, PathUtil::GetFile(newFile));
			if (Private_AssetType::CopyAssetFile(file, newFile))
			{
				file = newFile;
			}
		}
	}
	editableAsset.SetFiles(nullptr, files);
	editableAsset.SetDependencies(pAsset->GetDependencies());
	editableAsset.SetDetails(pAsset->GetDetails());
	editableAsset.InvalidateThumbnail();
	editableAsset.WriteToFile();
	return true;
}


//////////////////////////////////////////////////////////////////////////

// Fallback asset type if the actual type is not registered.
class CCryAssetType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CCryAssetType);

	virtual const char* GetTypeName() const override { return "cryasset"; }
	virtual const char* GetUiTypeName() const override { return "cryasset"; }
	virtual const char* GetFileExtension() const override { return "unregistered"; }
	virtual bool        IsImported() const override { return false; }
	virtual bool        CanBeEdited() const override { return false; }

	//Cannot be picked
	virtual bool IsUsingGenericPropertyTreePicker() const override { return false; }
};
REGISTER_ASSET_TYPE(CCryAssetType)

