// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Asset.h"

#include "AssetEditor.h"
#include "AssetManager.h"
#include "AssetType.h"
#include "DependencyTracker.h"
#include "EditableAsset.h"
#include "Loader/Metadata.h"
#include "Notifications/NotificationCenter.h"

#include "PathUtils.h"
#include "QtUtil.h"
#include "ThreadingUtils.h"

#include <IEditor.h>

#include <CryString/CryPath.h>
#include <QFileInfo>


namespace Private_Asset
{

static AssetLoader::SAssetMetadata GetMetadata(const CAsset& asset)
{
	AssetLoader::SAssetMetadata metadata;
	metadata.type = asset.GetType()->GetTypeName();
	metadata.guid = asset.GetGUID();
	metadata.sourceFile = PathUtil::GetFile(asset.GetSourceFile());
	metadata.files = asset.GetFiles();
	metadata.workFiles = asset.GetWorkFiles();
	metadata.details = asset.GetDetails();

	const std::vector<SAssetDependencyInfo>& dependencies = asset.GetDependencies();
	std::transform(dependencies.begin(), dependencies.end(), std::back_inserter(metadata.dependencies), [](const SAssetDependencyInfo& x) -> std::pair<string,int32>
	{
		return{ x.path, x.usageCount };
	});

	// The path includes terminating '/'
	const string path = asset.GetFolder();

	// Data files are relative to the asset
	for (string& str : metadata.files)
	{
		if (strnicmp(path.c_str(), str.c_str(), path.size()) == 0)
		{
			str.erase(0, path.size());
		}
	}

	// Dependencies may be relative to:
	// - the asset 
	// - the assets root directory.
	for (auto& item : metadata.dependencies)
	{
		string& str = item.first;
		if (!path.empty() && strnicmp(path.c_str(), str.c_str(), path.size()) == 0)
		{
			// The path is relative to the asset. 
			str.Format("./%s", str.substr(path.size()));
		}
	}

	return metadata;
}

static uint64 GetModificationTime(const string& filePath)
{
	uint64 timestamp = 0;
	ICryPak* const pPak = GetISystem()->GetIPak();
	FILE* pFile = pPak->FOpen(filePath.c_str(), "rb");
	if (pFile)
	{
		timestamp = pPak->GetModificationTime(pFile);
		pPak->FClose(pFile);
	}
	return timestamp;
}

bool AddUniqueFile(const string& file, std::vector<string>& files, const string& assetName, const string& fileType)
{
	// Ignore empty files.
	if (file.empty())
	{
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_WARNING, string().Format("Ignoring addition of empty %s '%s' to asset '%s'", fileType, file.c_str(), assetName));
		return false;
	}

	// Ignore duplicate files.
	const string unixFile = PathUtil::ToUnixPath(file);
	const auto it = std::find_if(files.begin(), files.end(), [&unixFile](auto& other)
	{
		return !stricmp(PathUtil::ToUnixPath(other), unixFile);
	});
	if (it != files.end())
	{
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_WARNING, string().Format("Ignoring addition of duplicate %s '%s' to asset '%s'", fileType, file.c_str(), assetName));
		return false;
	}

	files.push_back(file);
	return true;
}

} // namespace Private_Asset

CAsset::CAsset(const char* type, const CryGUID& guid, const char* name)
	: m_name(name)
	, m_guid(guid)
	, m_pEditor(nullptr)
{
	m_flags.bitField = 0;
	m_type = GetIEditor()->GetAssetManager()->FindAssetType(type);

	// Temporary fallback for debugging.
	if (!m_type)
	{
		m_type = GetIEditor()->GetAssetManager()->FindAssetType("cryasset"); 
		m_name = string().Format("%s (unresolved type: %s)", name, type);
	}

	CRY_ASSERT(m_type);
}

CAsset::~CAsset()
{ }

const string& CAsset::GetFolder() const
{
	if (m_folder.empty())
		m_folder = PathUtil::GetDirectory(m_metadataFile);
	return m_folder;
}

bool CAsset::HasSourceFile() const
{
	return m_type->IsImported() && !m_sourceFile.empty();
}

void CAsset::OpenSourceFile() const
{
	if(m_sourceFile.length() > 0)
		QtUtil::OpenFileForEdit(m_sourceFile);
}

const string CAsset::GetDetailValue(const string& detailName) const
{
	auto it = std::lower_bound(m_details.begin(), m_details.end(), detailName, [](auto& detail, auto& detailName)
	{
		return detail.first < detailName;
	});
	return it != m_details.end() && it->first == detailName ? it->second : string();
}

std::pair<bool, int> CAsset::IsAssetUsedBy(const char* szAnotherAssetPath) const
{
	const CDependencyTracker* const pDependencyTracker = CAssetManager::GetInstance()->GetDependencyTracker();
	return pDependencyTracker->IsAssetUsedBy(GetFile(0), szAnotherAssetPath);
}

std::pair<bool, int> CAsset::DoesAssetUse(const char* szAnotherAssetPath) const
{
	// TODO: binary search
	auto i = std::find_if(m_dependencies.cbegin(), m_dependencies.cend(), [szAnotherAssetPath](const SAssetDependencyInfo& x)
	{
		return x.path.CompareNoCase(szAnotherAssetPath) == 0;
	});

	return (i != m_dependencies.cend()) ? std::make_pair(true, (*i).usageCount) : std::make_pair(false, 0);
}

void CAsset::Reimport()
{
	if (!IsImmutable() && GetType()->IsImported() && IsWritable())
	{
		CAssetManager::GetInstance()->Reimport(this);
	}
}

bool CAsset::CanBeEdited() const
{
	return m_type->CanBeEdited();
}

bool CAsset::IsWritable(bool includeSourceFile /*= true*/) const
{
	const std::vector<string> filepaths(m_type->GetAssetFiles(*this, includeSourceFile, true));
	for (const string& path : filepaths)
	{
		const QFileInfo fileInfo(QtUtil::ToQString(path));
		if (fileInfo.exists() && !fileInfo.isWritable())
		{
			return false;
		}
	}
	return true;
}

bool CAsset::IsImmutable() const
{
	return m_flags.immutable;
}

void CAsset::Edit(CAssetEditor* pEditor)
{
	if (!CanBeEdited())
		return;

	if (m_pEditor)
	{
		m_pEditor->Raise();
		m_pEditor->Highlight();
	}
	else if (pEditor != nullptr)
	{
		pEditor->Raise();
		pEditor->Highlight();
		pEditor->OpenAsset(this);
	}
	else
	{
		m_type->Edit(this);
	}
}

void CAsset::OnOpenedInEditor(CAssetEditor* pEditor)
{
	CRY_ASSERT(pEditor);
	if (m_pEditor != pEditor)
	{
		m_pEditor = pEditor;
		m_flags.open = true;
		if (!m_pEditingSession)
		{
			m_pEditingSession = std::move(m_pEditor->CreateEditingSession());
		}

		m_pEditor->signalAssetClosed.Connect(std::function<void(CAsset*)>([this, pEditor](CAsset* pAsset)
		{
			if (pAsset == this && pEditor == m_pEditor)
			{
				m_pEditor->signalAssetClosed.DisconnectById((uintptr_t)this);

				m_pEditor = nullptr;
				m_flags.open = false;

				if (!IsModified())
				{
					m_pEditingSession.reset();
				}

				NotifyChanged(eAssetChangeFlags_Open);
			}
		}), (uintptr_t)this);

		NotifyChanged(eAssetChangeFlags_Open);
	}
}

// TODO: Limit access to this. Maybe move this to CEditableAsset.
void CAsset::SetModified(bool bModified)
{
	if(bModified != m_flags.modified)
	{
		m_flags.modified = bModified;
		NotifyChanged(eAssetChangeFlags_Modified);
	}
}

void CAsset::Save()
{
	if (!m_pEditingSession && !m_pEditor)
	{
		// Have nothing to save.
		return;
	}

	if (IsImmutable() || !IsWritable(false))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to save \"%s\". The asset is read-only.", m_name.c_str());
		return;
	}

	CEditableAsset editAsset(*this);
	bool saved = false;

	if (m_pEditingSession)
	{
		saved = m_pEditingSession->OnSaveAsset(editAsset);
	}
	else //if (m_pEditor)
	{
		saved = m_pEditor->OnSaveAsset(editAsset);
	}

	if (!saved || !WriteToFile())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to save asset \"%s\"", m_name.c_str());
		return;
	}

	InvalidateThumbnail();
	SetModified(false);
}

void CAsset::Reload()
{
	if (m_pEditor)
	{
		m_pEditor->DiscardAssetChanges();
	}
	else if (m_pEditingSession)
	{
		CEditableAsset editAsset(*this);
		m_pEditingSession->DiscardChanges(editAsset);
		SetModified(false);
	}
}

QIcon CAsset::GetThumbnail() const
{
	// if the HasThumbnail() passed, but loading of the icon failed, it will constantly try to load the icon again, producing infinite loading tasks,
	// so in the loading function m_thumbnail is always filled by m_type->GetIcon() value.
	if (m_type->HasThumbnail() && !m_thumbnail.isNull())
	{
		return m_thumbnail;
	}
	else
	{
		return m_type->GetIcon();
	}
}

void CAsset::GenerateThumbnail() const
{
	if (!IsImmutable() && m_type->HasThumbnail())
		m_type->GenerateThumbnail(this);
}

void CAsset::InvalidateThumbnail()
{
	if (!m_thumbnail.isNull())
	{
		m_thumbnail = QIcon();
	}
}

bool CAsset::Validate() const
{
	bool valid = true;
	valid &= !m_name.empty();
	valid &= m_type != nullptr;
	valid &= m_guid.hipart != 0 && m_guid.lopart != 0;
	valid &= m_files.size() > 0;
	valid &= !m_metadataFile.empty();
	return valid;
}

bool CAsset::WriteToFile()
{
	using namespace Private_Asset;

	XmlNodeRef node = XmlHelpers::CreateXmlNode(AssetLoader::GetMetadataTag());
	AssetLoader::WriteMetaData(node, GetMetadata(*this));

	if (!XmlHelpers::SaveXmlNode(node, m_metadataFile))
	{
		return false;
	}

	m_lastModifiedTime = GetModificationTime(m_metadataFile);
	return true;
}

void CAsset::SetName(const char* szName)
{
	m_name = szName;
}

void CAsset::SetLastModifiedTime(uint64 t)
{
	m_lastModifiedTime = t;
}

void CAsset::SetMetadataFile(const char* szFilepath)
{
	m_metadataFile = PathUtil::ToUnixPath(szFilepath);

	const char* const szEngine = "%engine%";
	m_flags.immutable = strnicmp(szEngine, szFilepath, strlen(szEngine)) == 0;
	m_folder.clear();
}

void CAsset::SetSourceFile(const char* szFilepath)
{
	m_sourceFile = PathUtil::ToUnixPath(szFilepath);
}

void CAsset::AddFile(const string& filePath)
{
	Private_Asset::AddUniqueFile(filePath, m_files, "file", m_name);
}

void CAsset::SetFiles(const std::vector<string>& filenames)
{
	m_files.clear();
	m_files.reserve(filenames.size());
	for (const string& filename : filenames)
	{
		AddFile(filename);
	}
}

void CAsset::AddWorkFile(const string& filePath)
{
	Private_Asset::AddUniqueFile(filePath, m_workFiles, "work file", m_name);
}

void CAsset::SetWorkFiles(const std::vector<string>& filenames)
{
	if (m_workFiles == filenames)
	{
		return;
	}
	m_workFiles = filenames;
}

void CAsset::SetDetail(const string& name, const string& value)
{
	auto it = std::lower_bound(m_details.begin(), m_details.end(), name, [](const std::pair<string, string>& detail, const string& name)
	{
		return detail.first < name;
	});
	if (it != m_details.end())
	{
		if (it->first == name)
		{
			it->second = value;
		}
		else
		{
			m_details.insert(it, std::make_pair(name, value));
		}
	}
	else
	{
		m_details.emplace_back(name, value);
	}
}

void CAsset::SetDependencies(const std::vector<SAssetDependencyInfo>& dependencies)
{
	m_dependencies.clear();
	for (const SAssetDependencyInfo& info : dependencies)
	{
		m_dependencies.emplace_back(PathUtil::ToUnixPath(info.path), info.usageCount);
	}
}

string CAsset::GetThumbnailPath() const
{
	CRY_ASSERT(GetMetadataFile());
	return PathUtil::ReplaceExtension(GetMetadataFile(), "thmb.png");
}


bool CAsset::IsThumbnailLoaded() const
{
	if (m_type->HasThumbnail())
	{
		return !m_thumbnail.isNull();
	}
	else
	{
		return true;
	}
}

void CAsset::SetThumbnail(const QIcon& icon)
{
	m_thumbnail = icon;
}

void CAsset::NotifyChanged(int changeFlags /*= eAssetChangeFlags_All*/)
{
	signalChanged(*this, changeFlags);
	CRY_ASSERT(CAssetManager::GetInstance());
	CAssetManager::GetInstance()->signalAssetChanged(*this, changeFlags);
}

const QString& CAsset::GetFilterString(bool forceCompute /*=false*/) const
{
	if (forceCompute || m_filterString.isEmpty())
	{
		m_filterString = m_name;
		m_filterString += " ";
		m_filterString += GetType()->GetUiTypeName();
	}

	return m_filterString;
}
