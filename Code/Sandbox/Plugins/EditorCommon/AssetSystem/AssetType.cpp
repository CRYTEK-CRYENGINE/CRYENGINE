// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetType.h"

#include "AssetSystem/AssetImportContext.h"
#include "AssetSystem/AssetImporter.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/EditableAsset.h"
#include "AssetSystem/Loader/AssetLoaderHelpers.h"
#include "AssetSystem/Loader/Metadata.h"
#include "FileUtils.h"
#include "PathUtils.h"

#include <CryString/StringUtils.h>
#include <IEditor.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>

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
static bool RenameAssetFile(const string& oldFilename, const string& newFilename, string rootFolder = "")
{
	if (rootFolder.empty())
	{
		rootFolder = PathUtil::GetGameProjectAssetsPath();
	}
	const string oldFilepath = PathUtil::Make(rootFolder, oldFilename);
	const string newFilepath = PathUtil::Make(rootFolder, newFilename);

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
static bool CopyAssetFile(const string& oldFilename, const string& newFilename, const string& destRoot = "")
{
	if (oldFilename.empty() || newFilename.empty() || oldFilename.CompareNoCase(newFilename) == 0)
	{
		return true;
	}

	const string newFilepath = PathUtil::ToUnixPath(PathUtil::Make(
		destRoot.empty() ? PathUtil::GetGameProjectAssetsPath() : destRoot, newFilename));
	if (FileUtils::Pak::CopyFileAllowOverwrite(oldFilename.c_str(), newFilepath.c_str()))
	{
		return true;
	}

	CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to copy asset file %s to %s", oldFilename.c_str(), newFilename.c_str());
	return false;
}

class CAssetMetadata : public INewAsset
{
public:
	CAssetMetadata(const CAssetType& type, const char* szMetadataFile, const char* szName)
		: m_metadataFile(szMetadataFile)
		, m_name(szName)
		, m_folder(PathUtil::GetDirectory(m_metadataFile))
	{
		m_metadata.type = type.GetTypeName();
	}

	virtual void SetGUID(const CryGUID& guid) override
	{
		m_metadata.guid = guid;
	}

	virtual void SetSourceFile(const char* szFilepath) override
	{
		m_metadata.sourceFile = szFilepath;
	}

	virtual void AddFile(const char* szFilepath) override
	{
		// Convert filepath to relative to metadata file.

		string filename(PathUtil::ToGamePath(szFilepath));

		if (strnicmp(m_folder.c_str(), filename.c_str(), m_folder.size()) == 0)
		{
			filename.erase(0, m_folder.size());
		}

		m_metadata.files.push_back(filename);
	}

	virtual void SetFiles(const std::vector<string>& filenames) override
	{
		m_metadata.files.clear();
		m_metadata.files.reserve(filenames.size());
		for (const string& filename : filenames)
		{
			AddFile(filename);
		}
	}

	virtual void AddWorkFile(const char* szFilepath) override
	{
		m_metadata.workFiles.push_back(PathUtil::ToGamePath(szFilepath));
	}

	virtual void SetWorkFiles(const std::vector<string>& filenames) override
	{
		m_metadata.workFiles.clear();
		m_metadata.workFiles.reserve(filenames.size());
		for (const string& filename : filenames)
		{
			AddWorkFile(filename);
		}
	}

	virtual void SetDetails(const std::vector<std::pair<string, string>>& details) override
	{
		m_metadata.details = details;
	}

	virtual void SetDependencies(const std::vector<SAssetDependencyInfo>& dependencies) override
	{
		m_metadata.dependencies.clear();
		m_metadata.dependencies.reserve(dependencies.size());
		std::transform(dependencies.cbegin(), dependencies.cend(), std::back_inserter(m_metadata.dependencies), [](const auto& x)
		{
			return std::make_pair(x.path, x.usageCount);
		});
	}

	const AssetLoader::SAssetMetadata& GetMetadata()
	{
		return m_metadata;
	}

	virtual const char* GetMetadataFile() const override
	{
		return m_metadataFile;
	}

	virtual const char* GetFolder() const override
	{
		return m_folder;
	}

	virtual const char* GetName() const override
	{
		return m_name;
	}

private:
	AssetLoader::SAssetMetadata m_metadata;
	const string                m_metadataFile;
	const string                m_name;
	const string                m_folder;
};

}

void CAssetType::Init()
{
	m_icon = GetIconInternal();
	OnInit();
}

string CAssetType::MakeMetadataFilename(const char* szAssetName) const
{
	return string().Format("%s.%s.cryasset", szAssetName, GetFileExtension());
}

QVariant CAssetType::GetVariantFromDetail(const string& detailValue, const CItemModelAttribute* pAttrib)
{
	CRY_ASSERT(pAttrib);
	return pAttrib->GetType()->ToQVariant(detailValue);
}

bool CAssetType::Create(const char* szFilepath, const SCreateParams* pCreateParams) const
{
	using namespace Private_AssetType;

	if (szFilepath && *szFilepath == '%')
	{
		const char* szMsg = QT_TR_NOOP("Can not create the asset. The destination folder is read only");
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "%s: %s", szMsg, szFilepath);
		return false;
	}

	string reasonToReject;
	if (!CAssetType::IsValidAssetPath(szFilepath, reasonToReject))
	{
		const char* szMsg = QT_TR_NOOP("Can not create asset");
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "%s %s: %s", szMsg, szFilepath, reasonToReject.c_str());
		return false;
	}

	const CryPathString adjustedFilepath = PathUtil::AdjustCasing(szFilepath);

	ICryPak* const pPak = GetISystem()->GetIPak();
	if (!pPak->MakeDir(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::GetPathWithoutFilename(adjustedFilepath)).c_str()))
	{
		return false;
	}

	CAssetMetadata metadata(*this, adjustedFilepath, AssetLoader::GetAssetName(adjustedFilepath));

	if (pCreateParams && pCreateParams->pSourceAsset)
	{
		if (!OnCopy(metadata, *pCreateParams->pSourceAsset))
		{
			return false;
		}
	}
	else if (!OnCreate(metadata, pCreateParams))
	{
		return false;
	}

	CAssetPtr const pNewAsset = AssetLoader::CAssetFactory::CreateFromMetadata(adjustedFilepath, metadata.GetMetadata());
	CEditableAsset editAsset(*pNewAsset);
	if (!editAsset.WriteToFile())
	{
		return false;
	}
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

std::vector<string> CAssetType::GetAssetFiles(const CAsset& asset, bool includeSourceFile, bool makeAbsolute /* = false*/
	, bool includeThumbnail /*= true*/, bool includeDerived /*= false*/) const
{
	const bool includeDataFiles = !HasDerivedFiles() || includeDerived;
	std::vector<string> files;
	files.reserve(3 + (includeDataFiles ? 0 : asset.GetFilesCount()));
	files.emplace_back(asset.GetMetadataFile());
	if (includeThumbnail && HasThumbnail())
	{
		files.emplace_back(asset.GetThumbnailPath());
	}
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
		const string assetsPath = PathUtil::GetGameProjectAssetsPath();
		for (string& filename : files)
		{
			filename = PathUtil::Make(assetsPath, filename);
		}
	}

	if (includeDataFiles)
	{
		for (size_t i = 0, N = asset.GetFilesCount(); i < N; ++i)
		{
			if (makeAbsolute)
			{
				files.emplace_back(PathUtil::Make(HasDerivedFiles() ? 
					PathUtil::GetEditorCachePath() : PathUtil::GetGameProjectAssetsPath(), asset.GetFile(i)));
			}
			else
			{
				files.emplace_back(asset.GetFile(i));
			}
		}
	}

	return files;
}

bool CAssetType::IsInPakOnly(const CAsset& asset) const
{
	std::vector<string> filepaths(GetAssetFiles(asset, false, true));
	for (string& path : filepaths)
	{
		if (FileUtils::Pak::IsFileInPakOnly(path))
		{
			return true;
		}
	}
	return false;
}

bool CAssetType::IsValidAssetPath(const char* szFilepath, /*out*/string& reasonToReject)
{
	if (!(AssetLoader::IsMetadataFile(szFilepath)))
	{
		const char* const szReason = QT_TR_NOOP("The filename extension is not recognized as a valid asset extension");
		reasonToReject = string().Format("%s: %s", szReason, PathUtil::GetExt(szFilepath));
		return false;
	}

	const std::vector<CAssetType*>& assetTypes = CAssetManager::GetInstance()->GetAssetTypes();

	const CryPathString dataFilename(PathUtil::RemoveExtension(PathUtil::GetFile(szFilepath)));
	const char* const szExt = PathUtil::GetExt(dataFilename);
	const bool unknownAssetType = std::none_of(assetTypes.begin(), assetTypes.end(), [szExt](const CAssetType* pAssetType)
	{
		return stricmp(szExt, pAssetType->GetFileExtension()) == 0;
	});

	if (unknownAssetType)
	{
		const char* const szReason = QT_TR_NOOP("The file extension does not match any registered asset type");
		reasonToReject = string().Format("%s: %s", szReason, szExt);
		return false;
	}

	const CryPathString baseName(PathUtil::RemoveExtension(dataFilename));
	if (baseName.find('.') != CryPathString::npos)
	{
		const char* const szReason = QT_TR_NOOP("The dot symbol (.) is not allowed in the asset name");
		reasonToReject = string().Format("%s: \"%s\"", szReason, baseName.c_str());
		return false;
	}

	if (!CryStringUtils::IsValidFileName(baseName.c_str()))
	{
		reasonToReject = QT_TR_NOOP("Asset filename is invalid. Only English alphanumeric characters, underscores, and the dash character are permitted.");
		return false;
	}

	for (const CAssetType* pAssetType: assetTypes)
	{ 
		if (!pAssetType->OnValidateAssetPath(szFilepath, reasonToReject))
		{
			return false;
		}
	}
	return true;
}

bool CAssetType::OnCopy(INewAsset& asset, CAsset& assetToCopy) const
{
	const string destinationFolder = asset.GetFolder();
	const bool copyToAnotherFolder = destinationFolder.CompareNoCase(assetToCopy.GetFolder()) != 0;

	// Copy source file.
	if (assetToCopy.GetSourceFile() && *assetToCopy.GetSourceFile())
	{
		if (copyToAnotherFolder)
		{
			const string oldSourceFile = assetToCopy.GetSourceFile();
			const string newSourceFile = PathUtil::AdjustCasing(PathUtil::Make(destinationFolder, PathUtil::GetFile(assetToCopy.GetSourceFile())));

			if (!Private_AssetType::CopyAssetFile(oldSourceFile, newSourceFile))
			{
				return false;
			}

			asset.SetSourceFile(newSourceFile);
		}
		else
		{
			asset.SetSourceFile(assetToCopy.GetSourceFile());
		}
	}

	// Active editing session is responsible to make a copy of the unsaved data.
	if (assetToCopy.IsModified() && assetToCopy.GetEditingSession())
	{
		return assetToCopy.GetEditingSession()->OnCopyAsset(asset);
	}

	// just copy the asset data files.
	std::vector<string> files = assetToCopy.GetFiles();
	for (string& file : files)
	{
		string newFile;
		if (Private_AssetType::TryReplaceFilename(assetToCopy.GetName(), asset.GetName(), file, newFile))
		{
			newFile = PathUtil::AdjustCasing(PathUtil::Make(destinationFolder, PathUtil::GetFile(newFile)));
			Private_AssetType::CopyAssetFile(file, newFile, 
				assetToCopy.GetType()->HasDerivedFiles() ? PathUtil::GetEditorCachePath() : "");
			file = newFile;
		}
	}
	asset.SetFiles(files);
	asset.SetDependencies(assetToCopy.GetDependencies());
	asset.SetDetails(assetToCopy.GetDetails());
	return true;
}

bool CAssetType::IsAssetValid(CAsset* pAsset, string& errorMsg) const
{
	const string assetsPath(PathUtil::GetGameProjectAssetsPath());
	for (size_t i = 0, N = pAsset->GetFilesCount(); i < N; ++i)
	{
		if (!FileUtils::Pak::IsFileInPakOrOnDisk(PathUtil::Make(assetsPath, pAsset->GetFile(i))))
		{
			errorMsg = string("Asset's file %s is missing on the file system.").Format(pAsset->GetFile(i));
			return false;
		}
	}
	return true;
}

bool CAssetType::RenameAsset(CAsset* pAsset, const char* szNewName) const
{
	const string oldName(pAsset->GetName());
	const string newName(szNewName);
	if (oldName == newName)
	{
		return true;
	}

	if (pAsset->IsImmutable())
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
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "The asset name \"%s\" differs from the metadata filename \"%s\"", pAsset->GetName(), pAsset->GetMetadataFile());
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
			if (Private_AssetType::RenameAssetFile(file, newFile, 
				pAsset->GetType()->HasDerivedFiles() ? PathUtil::GetEditorCachePath() : ""))
			{
				file = newFile;
			}
		}
	}
	editableAsset.SetFiles(files);
	editableAsset.SetName(szNewName);
	// The asset file monitor will handle this as if the asset had been modified.
	// See also CAssetManager::CAssetFileMonitor.
	return editableAsset.WriteToFile();
}

bool CAssetType::MoveAsset(CAsset* pAsset, const char* szDestinationFolder, bool bMoveSourcefile) const
{
	if (pAsset->IsImmutable())
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to move asset \"%s\": the asset is read only.", pAsset->GetName());
		return false;
	}

	bool bReadOnly = false;
	{
		std::vector<string> filepaths(GetAssetFiles(*pAsset, bMoveSourcefile, true, true, true));
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
		if (Private_AssetType::RenameAssetFile(file, newFile,
			pAsset->GetType()->HasDerivedFiles() ? PathUtil::GetEditorCachePath() : ""))
		{
			file = newFile;
		}
	}
	editableAsset.SetFiles(files);
	return editableAsset.WriteToFile();
}

// Fallback asset type if the actual type is not registered.
class CCryAssetType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CCryAssetType);

	virtual const char* GetTypeName() const override           { return "cryasset"; }
	virtual const char* GetUiTypeName() const override         { return "cryasset"; }
	virtual const char* GetFileExtension() const override      { return "unregistered"; }
	virtual bool        IsImported() const override            { return false; }
	virtual bool        CanBeEdited() const override           { return false; }
	virtual bool        CanAutoRepairMetadata() const override { return false; }

	//Cannot be picked
	virtual bool IsUsingGenericPropertyTreePicker() const override { return false; }
};
REGISTER_ASSET_TYPE(CCryAssetType)
