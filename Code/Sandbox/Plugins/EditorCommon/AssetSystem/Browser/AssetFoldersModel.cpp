// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetFoldersModel.h"

#include "AssetBrowser.h"
#include "AssetDropHandler.h"
#include "AssetReverseDependenciesDialog.h"
#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"

#include "Controls/QuestionDialog.h"
#include "EditorFramework/PersonalizationManager.h"
#include "PathUtils.h"
#include "QThumbnailView.h"
#include "QtUtil.h"

#include <CryString/CryPath.h>

#include <QDesktopServices>
#include <QUrl>

namespace Private_AssetFoldersModel
{

// Make sure none of assets belong to the destination folder.
bool CanMove(const std::vector<CAsset*>& assets, const string& folder)
{
	const string path(PathUtil::AddSlash(folder));
	return std::none_of(assets.begin(), assets.end(), [&path](CAsset* pAsset)
	{
		return strcmp(path.c_str(), pAsset->GetFolder()) == 0;
	});
}

void OnMove(const std::vector<CAsset*>& assets, const QString& destinationFolder)
{
	CAssetManager* const pAssetManager = CAssetManager::GetInstance();

	const QString question = QObject::tr("There is a possibility of undetected dependencies which can be violated after performing the operation.\n"
		"\n"
		"Do you really want to move %n asset(s) to \"%1\"?", "", assets.size()).arg(destinationFolder);

	if (pAssetManager->HasAnyReverseDependencies(assets))
	{
		CAssetReverseDependenciesDialog dialog(
			assets,
			QObject::tr("Assets to be moved"),
			QObject::tr("Dependent Assets"),
			QObject::tr("The following assets depend on the asset(s) to be moved. Therefore they probably will not behave correctly after performing the move operation."),
			question);
		dialog.setWindowTitle(QObject::tr("Move Assets"));

		if (!dialog.Execute())
		{
			return;
		}
	}
	else if (CQuestionDialog::SQuestion(QObject::tr("Move Assets"), question) != QDialogButtonBox::Yes)
	{
		return;
	}

	pAssetManager->MoveAssets(assets, QtUtil::ToString(destinationFolder));
}

QStringList GetAddedFoldersPersonalization()
{
	QStringList addedFolders;
	QVariant addedFoldersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("AssetFolderModel", "AddedFolders");
	if (addedFoldersVariant.isValid() && addedFoldersVariant.canConvert(QVariant::StringList))
	{
		addedFolders = addedFoldersVariant.toStringList();
	}
	return addedFolders;
}

void SetAddedFoldersPersonalization(const QStringList& addedFolders)
{
	GetIEditor()->GetPersonalizationManager()->SetProjectProperty("AssetFolderModel", "AddedFolders", addedFolders);
}

}

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

	CAssetManager::GetInstance()->signalBeforeAssetsReset.Connect(this, &CAssetFoldersModel::PreReset);
	CAssetManager::GetInstance()->signalAfterAssetsReset.Connect(this, &CAssetFoldersModel::PostReset);

	CAssetManager::GetInstance()->signalBeforeAssetsInserted.Connect(this, &CAssetFoldersModel::PreInsert);

	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.Connect(this, &CAssetFoldersModel::PreRemove);
	CAssetManager::GetInstance()->signalAfterAssetsRemoved.Connect(this, &CAssetFoldersModel::PostRemove);

	m_addedFolders = Private_AssetFoldersModel::GetAddedFoldersPersonalization();

	//Build initially
	if (CAssetManager::GetInstance()->GetAssetsCount() > 0 || !m_addedFolders.empty())
	{
		PreReset();
		PostReset();
	}
}

CAssetFoldersModel::~CAssetFoldersModel()
{
	CAssetManager::GetInstance()->signalBeforeAssetsReset.DisconnectObject(this);
	CAssetManager::GetInstance()->signalAfterAssetsReset.DisconnectObject(this);

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
			// falls through
		case QThumbnailsView::s_ThumbnailRole:
			return CryIcon("icons:General/Folder.ico");
		case (int)CAssetModel::Roles::TypeCheckRole:
			return (int)EAssetModelRowType::eAssetModelRow_Folder;
		case (int)Roles::FolderPathRole:
			return GetPath(folder);//TODO: this could be cached for optimization
		case (int)Roles::DisplayFolderPathRole:
			return GetPrettyPath(GetPath(folder));
		case (int)CAssetModel::Roles::InternalPointerRole:
			return reinterpret_cast<intptr_t>(folder);
		case QThumbnailsView::s_ThumbnailBackgroundColorRole:
			return QColor(0x73, 0x73, 0x73);
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

		const QString stringValue = value.toString();
		if (stringValue.isEmpty())
		{
			return false;
		}

		const QString oldName = GetPath(folder);
		const QString newName = QtUtil::ToQString(PathUtil::AdjustCasing(QtUtil::ToString(stringValue).c_str()).c_str());

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
		folder->m_name = newName;
		m_addedFolders.append(GetPath(folder));
		Private_AssetFoldersModel::SetAddedFoldersPersonalization(m_addedFolders);
		
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
	Qt::ItemFlags flags = QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled;

	const Folder* folder = ToFolder(index);
	if (folder->m_empty)
	{
		return flags | Qt::ItemIsEditable;
	}

	return flags;
}

QStringList CAssetFoldersModel::mimeTypes() const
{
	auto typeList = QAbstractItemModel::mimeTypes();
	typeList << CDragDropData::GetMimeFormatForType("EngineFilePaths");
	typeList << CDragDropData::GetMimeFormatForType("Assets");
	typeList << CDragDropData::GetMimeFormatForType("LayersAndObjects");
	return typeList;
}

Qt::DropActions CAssetFoldersModel::supportedDropActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

bool CAssetFoldersModel::canDropMimeData(const QMimeData* pMimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
	using namespace Private_AssetFoldersModel;

	const QModelIndex destination = row != -1 ? index(row, 0, parent) : (parent.isValid() ? parent.sibling(parent.row(), 0) : GetProjectAssetsFolderIndex());
	if (!destination.isValid())
	{
		return false;
	}

	const QVariant variant(destination.data(static_cast<int>(Roles::FolderPathRole)));
	if (!variant.isValid())
	{
		return false;
	}

	const QString folder(variant.toString());
	if (IsReadOnlyFolder(folder))
	{
		return false;
	}

	const CDragDropData* const pDragDropData = CDragDropData::FromMimeData(pMimeData);
	const QStringList filePaths = pDragDropData->GetFilePaths();
	if (!filePaths.empty())
	{
		return true;
	}

	if (pDragDropData->HasCustomData("Assets"))
	{
		std::vector<CAsset*> assets = CAssetModel::GetAssets(*pDragDropData);
		if (CanMove(assets, QtUtil::ToString(folder)))
		{
			CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QObject::tr("Move %n asset(s) to \"%1\"", "", assets.size()).arg(GetPrettyPath(folder)));
			return true;
		}
	}
	else
	{
		CAssetConverter* pConverter = CAssetManager::GetInstance()->GetAssetConverter(*pMimeData);
		if (pConverter)
		{
			CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QObject::tr("%1 in \"%2\"").arg(QtUtil::ToQString(pConverter->ConversionInfo(*pMimeData)), GetPrettyPath(folder)));
			return true;
		}
	}

	return false;
}

bool CAssetFoldersModel::dropMimeData(const QMimeData* pMimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
	using namespace Private_AssetFoldersModel;

	const QModelIndex destination = row != -1 ? index(row, 0, parent) : (parent.isValid() ? parent.sibling(parent.row(), 0) : GetProjectAssetsFolderIndex());
	if (!destination.isValid())
	{
		return false;
	}

	const QVariant variant(destination.data(static_cast<int>(Roles::FolderPathRole)));
	if (!variant.isValid())
	{
		return false;
	}

	const QString folder(variant.toString());
	if (IsReadOnlyFolder(folder))
	{
		return false;
	}

	const CDragDropData* const pDragDropData = CDragDropData::FromMimeData(pMimeData);
	const QStringList filePaths = pDragDropData->GetFilePaths();
	if (!filePaths.empty())
	{
		CAssetDropHandler::SImportParams importParams;
		importParams.outputDirectory = QtUtil::ToString(folder);
		importParams.bHideDialog = !QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
		CAssetDropHandler::ImportAsync(filePaths, importParams);
		return true;
	}

	if (pDragDropData->HasCustomData("Assets"))
	{
		std::vector<CAsset*> assets = CAssetModel::GetAssets(*pDragDropData);
		if (CanMove(assets, QtUtil::ToString(folder)))
		{
			OnMove(assets, folder);
		}
		return true;
	}

	CAssetConverter* pConverter = CAssetManager::GetInstance()->GetAssetConverter(*pMimeData);
	if (pConverter)
	{
		const string folderPath(QtUtil::ToString(folder));

		QWidget* pWidget = qApp->widgetAt(QCursor::pos());
		while (pWidget && pWidget->objectName().compare("Asset Browser") != 0)
		{
			pWidget = pWidget->parentWidget();
		}

		CAssetBrowser* pAssetBrowser = pWidget ? static_cast<CAssetBrowser*>(pWidget) : nullptr;
		if (pAssetBrowser)
		{
			SAssetConverterConversionInfo info{ folderPath, pAssetBrowser };
			pConverter->Convert(*pMimeData, info);
			return true;
		}
	}

	return false;
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

void CAssetFoldersModel::PreReset()
{
	beginResetModel();
}

void CAssetFoldersModel::PostReset()
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
	// Empty name stands for the assets root folder
	if (folder.isEmpty())
	{
		return GetProjectAssetsFolderIndex();
	}

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
	const QString newFolderBaseName = QtUtil::ToQString(PathUtil::AdjustCasing(QT_TR_NOOP("New Folder")).c_str());

	QString	newFolderName(newFolderBaseName);
	for (int count = 1; CAssetFoldersModel::GetInstance()->HasSubfolder(parentPath, newFolderName); ++count)
	{
		newFolderName = QString(tr("%1 %2")).arg(newFolderBaseName).arg(count);
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
		Private_AssetFoldersModel::SetAddedFoldersPersonalization(m_addedFolders);
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
		Private_AssetFoldersModel::SetAddedFoldersPersonalization(m_addedFolders);
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
