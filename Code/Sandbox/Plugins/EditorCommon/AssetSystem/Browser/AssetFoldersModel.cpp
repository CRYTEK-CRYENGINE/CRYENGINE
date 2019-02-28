// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetFoldersModel.h"

#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"

#include "QtUtil.h"
#include "FilePathUtil.h"
#include "CryString/CryPath.h"

#include <QDesktopServices>
#include <QUrl>

CAssetFoldersModel::CAssetFoldersModel(QObject* parent /*= nullptr*/)
	: QAbstractItemModel(parent)
{
	//Initialize roots
	m_root.m_name = "_root_";
	m_root.m_parent = nullptr;

	//Cannot use PathUtil::GetGameFolder() as it is lowercase
	const char* gameFolder = gEnv->pConsole->GetCVar("sys_game_folder")->GetString();

	auto assetFolders = std::make_unique<Folder>();
	assetFolders->m_name = gameFolder;
	assetFolders->m_parent = &m_root;
	m_assetFolders = assetFolders.get();
	m_root.m_subFolders.emplace_back(std::move(assetFolders));

	CAssetManager::GetInstance()->signalBeforeAssetsUpdated.Connect(this, &CAssetFoldersModel::PreUpdate);
	CAssetManager::GetInstance()->signalAfterAssetsUpdated.Connect(this, &CAssetFoldersModel::PostUpdate);

	CAssetManager::GetInstance()->signalBeforeAssetsInserted.Connect(this, &CAssetFoldersModel::PreInsert);

	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.Connect(this, &CAssetFoldersModel::PreRemove);
	CAssetManager::GetInstance()->signalAfterAssetsRemoved.Connect(this, &CAssetFoldersModel::PostRemove);

	//Build initially
	if (CAssetManager::GetInstance()->GetAssetsCount() > 0)
	{
		PreUpdate();
		PostUpdate();
	}
}

CAssetFoldersModel::~CAssetFoldersModel()
{
	CAssetManager::GetInstance()->signalBeforeAssetsUpdated.DisconnectObject(this);
	CAssetManager::GetInstance()->signalAfterAssetsUpdated.DisconnectObject(this);

	CAssetManager::GetInstance()->signalBeforeAssetsInserted.DisconnectObject(this);
	CAssetManager::GetInstance()->signalAfterAssetsInserted.DisconnectObject(this);

	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.DisconnectObject(this);
	CAssetManager::GetInstance()->signalAfterAssetsRemoved.DisconnectObject(this);
}

CAssetFoldersModel* CAssetFoldersModel::GetInstance()
{
	return CAssetManager::GetInstance()->GetAssetFoldersModel();
}

bool CAssetFoldersModel::hasChildren(const QModelIndex& parent /*= QModelIndex()*/) const
{
	const Folder* folder = ToFolder(parent);
	return !folder->m_subFolders.empty();
}

int CAssetFoldersModel::rowCount(const QModelIndex& parent) const
{
	const Folder* folder = ToFolder(parent);
	return folder->m_subFolders.size();
}

QVariant CAssetFoldersModel::data(const QModelIndex& index, int role) const
{
	auto folder = ToFolder(index);
	if (folder && index.column() == 0)
	{
		switch (role)
		{
		case Qt::DisplayRole:
		case Qt::EditRole:
			if (folder->m_name.startsWith("%"))
				return CAssetManager::GetInstance()->GetAliasName(folder->m_name.toStdString().c_str());
			else
				return folder->m_name;
		case Qt::DecorationRole:
			return CryIcon("icons:General/Folder.ico");
		case (int)CAssetModel::Roles::TypeCheckRole:
			return (int)EAssetModelRowType::eAssetModelRow_Folder;
		case (int)Roles::FolderPathRole:
			return GetPath(folder);//TODO: this could be cached for optimization
		case (int)Roles::DisplayFolderPathRole:
			return GetPrettyPath(GetPath(folder));
		case (int)CAssetModel::Roles::InternalPointerRole:
			return reinterpret_cast<intptr_t>(folder);
		default:
			return QVariant();
		}
	}
	else
		return QVariant();
}

bool CAssetFoldersModel::setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/)
{
	if (index.isValid() && index.column() == 0 && role == Qt::EditRole)
	{
		Folder* folder = ToFolder(index);
		CRY_ASSERT(folder->m_empty);
		QString newName = value.toString();
		QString oldName = GetPath(folder);

		if (newName.isEmpty())
		{
			return false;
		}

		if (!PathUtil::IsValidFileName(newName))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Invalid folder name: %s", newName.toStdString().c_str());
			return false;
		}

		//check if name doesn't collide with existing name
		const Folder* parent = folder->m_parent;
		for (auto& subfolder : parent->m_subFolders)
		{
			if (subfolder->m_name.compare(newName, Qt::CaseInsensitive) == 0)
				return false;
		}

		CRY_ASSERT(m_addedFolders.removeOne(oldName));
		folder->m_name = value.toString();
		m_addedFolders.append(GetPath(folder));
		
		//Note that due to this model only having one column but thumbnails being another column, the thumbnails will not be updated immediately.
		//Not sure how to fix this with the current setup
		dataChanged(index, index, QVector<int>() << Qt::DisplayRole);
	}
	return false;
}

QVariant CAssetFoldersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (section == 0 && orientation == Qt::Horizontal)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			return "Folder";
		case Attributes::s_getAttributeRole:
			return QVariant::fromValue(&Attributes::s_nameAttribute);
		default:
			return QVariant();
		}
	}
	else
		return QAbstractItemModel::headerData(section, orientation, role);
}

Qt::ItemFlags CAssetFoldersModel::flags(const QModelIndex& index) const
{
	const Folder* folder = ToFolder(index);
	if (folder->m_empty)
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
	}

	return QAbstractItemModel::flags(index);
}

QModelIndex CAssetFoldersModel::index(int row, int column, const QModelIndex& parent /*= QModelIndex()*/) const
{
	const Folder* folder = ToFolder(parent);
	if (row < folder->m_subFolders.size())
	{
		return createIndex(row, column, reinterpret_cast<uintptr_t>(folder->m_subFolders[row].get()));
	}
	else
	{
		return createIndex(-1, -1, -1);
	}
}

QModelIndex CAssetFoldersModel::parent(const QModelIndex& index) const
{
	const Folder* folder = ToFolder(index);
	if (folder->m_parent && folder->m_parent->m_parent)
	{
		auto& grandParent = folder->m_parent->m_parent;
		auto it = std::find_if(grandParent->m_subFolders.begin(), grandParent->m_subFolders.end(), [&](const std::unique_ptr<Folder>& it) { return it.get() == folder->m_parent; });
		return createIndex(it - grandParent->m_subFolders.begin(), 0, reinterpret_cast<uintptr_t>(folder->m_parent));
	}
	else
	{
		return QModelIndex();
	}
}

void CAssetFoldersModel::PreUpdate()
{
	beginResetModel();
}

void CAssetFoldersModel::PostUpdate()
{
	Clear();
	for (auto& asset : CAssetManager::GetInstance()->m_assets)
	{
		AddFolder(asset->GetFolder(), false);
	}

	for (auto& path : m_addedFolders)
	{
		AddFolder(QtUtil::ToString(path).c_str(), true);
	}
	endResetModel();
}

void CAssetFoldersModel::PreInsert(const std::vector<CAsset*>& assets)
{
	for (CAsset* pAsset : assets)
	{
		Folder* pNewFolder = AddFolder(pAsset->GetFolder(), false);
		if (!pNewFolder)
		{
			continue;
		}

		// Temporarily remove folder and update connected views.

		Folder* const pParent = pNewFolder->m_parent;
		std::vector<std::unique_ptr<Folder>>& parentFolders = pParent->m_subFolders;

		// AddFolder() appends folders.
		CRY_ASSERT(parentFolders.size() && pNewFolder == parentFolders.back().get());

		std::unique_ptr<Folder> pTemp(std::move(parentFolders.back()));
		parentFolders.pop_back();

		beginInsertRows(ToIndex(pParent), parentFolders.size(), parentFolders.size());

		parentFolders.emplace_back(std::move(pTemp));

		endInsertRows();
	}
}

void CAssetFoldersModel::PreRemove(const std::vector<CAsset*>& assets)
{
	beginResetModel();
}

void CAssetFoldersModel::PostRemove()
{
	Clear();
	for (auto& asset : CAssetManager::GetInstance()->m_assets)
	{
		AddFolder(asset->GetFolder(), false);
	}

	for (auto& path : m_addedFolders)
	{
		AddFolder(QtUtil::ToString(path).c_str(), true);
	}
	endResetModel();
}

void CAssetFoldersModel::Clear()
{
	m_assetFolders->m_subFolders.clear();
	m_root.m_subFolders.erase(std::remove_if(m_root.m_subFolders.begin(), m_root.m_subFolders.end(), [this](const auto& x) {
		return x.get() != m_assetFolders;
	}), m_root.m_subFolders.end());
}

CAssetFoldersModel::Folder* CAssetFoldersModel::AddFolder(const char* path, bool empty)
{
	//TODO : mark the folders non empty if empty is false
	auto segments = PathUtil::SplitIntoSegments(path);
	if (segments.size() <= 0)
	{
		return nullptr;
	}

	Folder* current = nullptr;
	if (segments[0][0] != '%')
	{
		current = m_assetFolders;
	}
	else
	{
		//will find the prefix among its subfolders during iteration
		current = &m_root;
	}
	
	Folder* pFirstNewFolder = nullptr;

	for (int i = 0; i < segments.size(); i++)
	{
		auto it = std::find_if(current->m_subFolders.begin(), current->m_subFolders.end(), [&](const std::unique_ptr<Folder>& it) 
		{ 
			return it->m_name.compare(QString(segments[i]), Qt::CaseInsensitive) == 0;
		});

		if (it != current->m_subFolders.end())
		{
			current = it->get();

			//Mark this directory as not empty anymore
			if (!empty && current->m_empty)
				current->m_empty = empty;
		}
		else //insert new folder
		{
			current = CreateSubFolder(current, segments[i], empty);
			if (!pFirstNewFolder)
			{
				pFirstNewFolder = current;
			}
		}
	}

	return pFirstNewFolder;
}

CAssetFoldersModel::Folder* CAssetFoldersModel::CreateSubFolder(Folder* parent, const char* childName, bool empty)
{
	auto f = std::make_unique<Folder>();
	f->m_name = childName;
	f->m_parent = parent;
	f->m_empty = empty;
	parent->m_subFolders.emplace_back(std::move(f));
	return parent->m_subFolders.back().get();
}

QString CAssetFoldersModel::GetPath(const Folder* folder) const
{
	if (folder == &m_root || folder == m_assetFolders)
	{
		return "";
	}

	QString path(folder->m_name);

	const Folder* parent = folder->m_parent;
	while (parent && parent != m_assetFolders)
	{
		if(parent->m_parent)
		{
			folder = folder->m_parent;
			path = folder->m_name + "/" + path;
		}

		parent = parent->m_parent;
	}

	return path;
}

CAssetFoldersModel::Folder* CAssetFoldersModel::ToFolder(const QModelIndex& folder) const
{
	if (!folder.isValid())
	{
		return const_cast<Folder*>(&m_root);
	}
	else
	{
		return static_cast<Folder*>(folder.internalPointer());
	}
}

const CAssetFoldersModel::Folder* CAssetFoldersModel::GetFolder(const char* path) const
{
	auto segments = PathUtil::SplitIntoSegments(path);

	const Folder* current = m_assetFolders;

	if(segments.size() > 0)
	{
		if (segments[0][0] == '%')
		{
			current = &m_root;
		}

		for (int i = 0; i < segments.size(); i++)
		{
			auto it = std::find_if(current->m_subFolders.begin(), current->m_subFolders.end(), [&](const std::unique_ptr<Folder>& it) 
			{ 
				return it->m_name.compare(QString(segments[i]), Qt::CaseInsensitive) == 0; 
			});

			if (it != current->m_subFolders.end())
			{
				current = it->get();
			}
			else
			{
				return nullptr;
			}
		}
	}

	return current;
}

QModelIndex CAssetFoldersModel::FindIndexForFolder(const QString& folder, Roles role) const
{
	QModelIndexList matches;

	QString adjustedFolder = folder; // Without leading and trailing slashes.
	if (adjustedFolder.endsWith('/'))
	{
		adjustedFolder = adjustedFolder.left(adjustedFolder.length() - 1);
	}
	if (adjustedFolder.startsWith('/'))
	{
		adjustedFolder = adjustedFolder.right(adjustedFolder.length() - 1);
	}

	matches = match(index(0, 0, QModelIndex()), (int)role, adjustedFolder, 1, Qt::MatchExactly | Qt::MatchRecursive);

	if (matches.size() == 1)
	{
		return matches.first();
	}

	return QModelIndex();
}

QModelIndex CAssetFoldersModel::ToIndex(const Folder* pFolder) const
{
	QModelIndexList matches;

	matches = match(index(0, 0, QModelIndex()), (int)CAssetModel::Roles::InternalPointerRole, reinterpret_cast<intptr_t>(pFolder), 1, Qt::MatchExactly | Qt::MatchRecursive);

	if (matches.size() == 1)
	{
		return matches.first();
	}

	return QModelIndex();
}


QModelIndex CAssetFoldersModel::GetProjectAssetsFolderIndex() const
{
	// See GetProjectAssetsFolderName().
	return index(0, 0, QModelIndex());
}

QString CAssetFoldersModel::CreateFolder(const QString& parentPath)
{
	int count = 1;
	QString newFolderName = tr("New Folder");

	while (CAssetFoldersModel::GetInstance()->HasSubfolder(parentPath, newFolderName))
	{
		newFolderName = QString(tr("New Folder %1")).arg(count);
		count++;
	}

	CreateFolder(parentPath, newFolderName);

	return parentPath + "/" + newFolderName;
}

void CAssetFoldersModel::CreateFolder(const QString& parentPath, const QString& folderName)
{
	QModelIndex parentIndex = FindIndexForFolder(parentPath);
	CRY_ASSERT(parentIndex.isValid());

	QString folderPath = parentPath;
	if (!folderPath.isEmpty())
	{
		folderPath += "/";
	}
	folderPath += folderName;

	if(!m_addedFolders.contains(folderPath))
	{
		auto parentFolder = ToFolder(parentIndex);
		beginInsertRows(parentIndex, parentFolder->m_subFolders.size(), parentFolder->m_subFolders.size());
		CreateSubFolder(parentFolder, folderName.toStdString().c_str(), true);
		endInsertRows();

		m_addedFolders.append(folderPath);
	}
}

void CAssetFoldersModel::DeleteFolder(const QString& folderPath)
{
	QModelIndex folderIndex = FindIndexForFolder(folderPath);
	if (folderIndex.isValid())
	{
		auto pFolder = ToFolder(folderIndex);
		CRY_ASSERT(pFolder->m_empty);
		beginRemoveRows(folderIndex.parent(), folderIndex.row(), folderIndex.row());
		pFolder->m_parent->m_subFolders.erase(pFolder->m_parent->m_subFolders.begin() + folderIndex.row());
		CRY_ASSERT(m_addedFolders.removeOne(folderPath));
		endRemoveRows();
	}
}

void CAssetFoldersModel::OpenFolderWithShell(const QString& folderPath)
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(GetPrettyPath(folderPath)));
}

bool CAssetFoldersModel::IsEmptyFolder(const QString& folderPath) const
{
	const Folder* pFolder = GetFolder(folderPath.toStdString().c_str());
	if (!pFolder)
	{
		CRY_ASSERT(pFolder);
		return true;
	}
	return pFolder->m_empty;
}

bool CAssetFoldersModel::IsReadOnlyFolder(const QString& folderPath) const
{
	//Quick implementation, a better one would be to check that m_assetsFolder is a parent of the folder
	return !folderPath.isEmpty() && folderPath[0] == '%';
}

bool CAssetFoldersModel::HasSubfolder(const QString& folderPath, const QString& subFolderName) const
{
	const Folder* pFolder = GetFolder(folderPath.toStdString().c_str());
	if (!pFolder)
	{ 
		CRY_ASSERT(pFolder);
		return false;
	}

	for (auto& subfolder : pFolder->m_subFolders)
	{
		if (subfolder->m_name.compare(subFolderName, Qt::CaseInsensitive) == 0)
		{
			return true;
		}
	}
	return false;
}

const QString& CAssetFoldersModel::GetProjectAssetsFolderName() const 
{
	//Note: this may not always hold, perhaps referring to the cvar would be safer
	CRY_ASSERT(m_root.m_subFolders.size() >= 0);
	return m_root.m_subFolders[0]->m_name;
}

QString CAssetFoldersModel::GetPrettyPath(const QString& path) const
{
	if (path.isEmpty())
	{
		return GetProjectAssetsFolderName();
	}
	else if (path.startsWith('%')) //alias
	{
		const int aliasEndIndex = path.indexOf('%', 1);
		QString alias = path.left(aliasEndIndex + 1);
		QString prettyPath(path);
		prettyPath.replace(alias, CAssetManager::GetInstance()->GetAliasName(alias.toStdString().c_str()));
		return prettyPath;
	}
	else
	{
		return GetProjectAssetsFolderName() + "/" + path;
	}
}

