// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Asset.h"

#include "AssetType.h"
#include "AssetManager.h"
#include "AssetEditor.h"
#include "Loader/Metadata.h"

#include "FilePathUtil.h"
#include "QtUtil.h"
#include "ThreadingUtils.h"

#include <CryString/CryPath.h>

namespace Private_Asset
{

static AssetLoader::SAssetMetadata GetMetadata(const CAsset& asset)
{
	AssetLoader::SAssetMetadata metadata;
	metadata.type = asset.GetType()->GetTypeName();
	metadata.guid = asset.GetGUID();
	metadata.sourceFile = PathUtil::GetFile(asset.GetSourceFile());
	metadata.files = asset.GetFiles();
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
		if (strncmp(path.c_str(), str.c_str(), path.size()) == 0)
		{
			str.erase(0, path.size());
		}
	}

	// Depencencies may be relative to:
	// - the asset 
	// - the assets root directory.
	for (auto& item : metadata.dependencies)
	{
		string& str = item.first;
		if (strncmp(path.c_str(), str.c_str(), path.size()) == 0)
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
	FILE* pFile = pPak->FOpen(filePath.c_str(), "rbx");
	if (pFile)
	{
		timestamp = pPak->GetModificationTime(pFile);
		pPak->FClose(pFile);
	}
	return timestamp;
}

} // namespace Private_Asset

CAsset::CAsset(const char* type, const CryGUID& guid, const char* name)
	: m_name(name)
	, m_guid(guid)
	, m_editor(nullptr)
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
{

}

const char* CAsset::GetFolder() const
{
	if (m_folder.empty())
		m_folder = PathUtil::GetDirectory(m_metadataFile);
	return m_folder.c_str();
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

void CAsset::Reimport()
{
	if (!IsReadOnly() && GetType()->IsImported())
		CAssetManager::GetInstance()->Reimport(this);
}

bool CAsset::CanBeEdited() const
{
	return m_type->CanBeEdited();
}

bool CAsset::IsReadOnly() const
{
	return m_flags.readOnly;
}

void CAsset::Edit(CAssetEditor* pEditor)
{
	if (!CanBeEdited())
		return;

	if (m_editor)
	{
		m_editor->Raise();
		m_editor->Highlight();
	}
	else if (pEditor != nullptr)
	{
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
	if (m_editor != pEditor)
	{
		m_editor = pEditor;
		m_flags.open = true;

		m_editor->signalAssetClosed.Connect(std::function<void(CAsset*)>([this, pEditor](CAsset* pAsset)
		{
			if (pAsset == this && pEditor == m_editor)
			{
				m_editor = nullptr;
				m_flags.open = false;

				NotifyChanged(eAssetChangeFlags_Open);
			}
		}));

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
	if (m_editor)
		m_editor->Save();
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
	if (!IsReadOnly() && m_type->HasThumbnail())
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

void CAsset::WriteToFile()
{
	using namespace Private_Asset;

	XmlNodeRef node = XmlHelpers::CreateXmlNode(AssetLoader::GetMetadataTag());
	AssetLoader::WriteMetaData(node, GetMetadata(*this));

	if (XmlHelpers::SaveXmlNode(node, m_metadataFile))
	{
		m_lastModifiedTime = GetModificationTime(m_metadataFile);
	}
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
	m_flags.readOnly = strnicmp(szEngine, szFilepath, strlen(szEngine)) == 0;
	m_folder.clear();
}

void CAsset::SetSourceFile(const char* szFilepath)
{
	m_sourceFile = PathUtil::ToUnixPath(szFilepath);
}

void CAsset::AddFile(const string& file)
{
	// Ignore empty files.
	if (file.empty())
	{
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_WARNING, string().Format("Ignoring addition of empty file '%s' to asset '%s'", file.c_str(), m_name.c_str()));
		return;
	}

	// Ignore duplicate files.
	const string unixFile = PathUtil::ToUnixPath(file);
	const auto it = std::find_if(m_files.begin(), m_files.end(), [&unixFile](auto& other)
	{
		return !stricmp(PathUtil::ToUnixPath(other), unixFile);
	});
	if (it != m_files.end())
	{
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_WARNING, string().Format("Ignoring addition of duplicate file '%s' to asset '%s'", file.c_str(), m_name.c_str()));
		return;
	}

	m_files.push_back(file);
}

void CAsset::SetFiles(const char* szCommonPath, const std::vector<string>& filenames)
{
	m_files.clear();

	if (szCommonPath && *szCommonPath)
	{
		for (const string& filename : filenames)
		{
			const string filepath = PathUtil::Make(szCommonPath, filename.c_str());
			AddFile(filepath);
		}
	}
	else
	{
		for (const string& filename : filenames)
		{
			AddFile(filename);
		}
	}
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

void CAsset::SetDependencies(std::vector<SAssetDependencyInfo> dependencies)
{
	for (SAssetDependencyInfo& filename : dependencies)
	{
		filename.path = PathUtil::ToUnixPath(filename.path);
	}
	m_dependencies = std::move(dependencies);
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

