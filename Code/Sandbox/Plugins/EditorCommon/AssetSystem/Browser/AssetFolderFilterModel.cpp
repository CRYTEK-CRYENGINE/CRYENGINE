// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetFolderFilterModel.h"

#include "AssetModel.h"
#include "AssetFoldersModel.h"
#include "NewAssetModel.h"

#include <QStandardItemModel>
#include <QSortFilterProxyModel>

class CAssetFolderFilterModel::FilteredAssetsModel : public QSortFilterProxyModel
{
public:
	FilteredAssetsModel(CAssetFolderFilterModel* parent)
		: QSortFilterProxyModel(parent)
		, m_parent(parent)
	{
	}

	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
	{
		auto& acceptedFolders = m_parent->m_acceptedFolders;

		auto model = sourceModel();
		CRY_ASSERT(model);

		auto index = model->index(sourceRow, EAssetColumns::eAssetColumns_Folder, sourceParent);
		QVariant variant = index.data(Qt::DisplayRole);
		if (variant.isValid())
		{
			if (acceptedFolders.isEmpty())
			{
				if (m_parent->m_isRecursive)
				{
					return !variant.toString().startsWith('%');
				}
				else
				{
					return variant.toString().isEmpty();
				}
			}

			QString folder = variant.toString();

			if (m_parent->m_isRecursive)
			{
				for (auto& filter : acceptedFolders)
				{
					if (folder.startsWith(filter))
					{
						return true;
					}
				}
			}
			else
			{
				for (auto& filter : acceptedFolders)
				{
					if (folder.compare(filter, Qt::CaseInsensitive) == 0)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	// This is a workaround to solve an Access violation exception throws in this metod if it has the default implementation.
	// The exception [sometimes] occurs if the the user changes current folder in the Asset Browser while some cryasset files 
	// are being deleted or renamed on the disk [e.g. from the outside or by the Asset Browser means]. 
	QModelIndex parent(const QModelIndex& proxyChild) const
	{
		// This model is flat
		return QModelIndex();
	}

private:
	CAssetFolderFilterModel* m_parent;
};

//////////////////////////////////////////////////////////////////////////

class CAssetFolderFilterModel::FoldersModel : public QAbstractProxyModel
{
public:
	FoldersModel(CAssetFolderFilterModel* parent)
		: QAbstractProxyModel(parent)
		, m_parent(parent)
	{
		auto sourceModel = CAssetFoldersModel::GetInstance();
		setSourceModel(sourceModel);

		m_connections.push_back(connect(sourceModel, &CAssetFoldersModel::modelReset, [this]() { UpdateFolders(); }));

		//TODO: Do incremental updates instead of recomputing all folders for better performance.
		m_connections.push_back(connect(sourceModel, &CAssetFoldersModel::rowsInserted, [this](const QModelIndex&, int, int) { UpdateFolders(); }));
	}

	virtual ~FoldersModel()
	{
		for (const QMetaObject::Connection &connection : m_connections)
		{
			disconnect(connection);
		}
	}

	void UpdateFolders()
	{
		const int folderCount = m_folders.count();
		if(folderCount > 0)
		{
			beginRemoveRows(QModelIndex(), 0, folderCount - 1);
			m_folders.clear();
			endRemoveRows();
		}

		if (m_parent->m_showFolders)
		{
			QVector<QPersistentModelIndex> mapping;

			if(m_parent->m_acceptedFolders.count() > 0)
			{
				for (auto& folder : m_parent->m_acceptedFolders)
				{
					auto index = CAssetFoldersModel::GetInstance()->FindIndexForFolder(folder);
					if (index.isValid())
					{
						auto subFolderCount = CAssetFoldersModel::GetInstance()->rowCount(index);
						for (int i = 0; i < subFolderCount; i++)
							mapping.push_back(index.child(i, 0));
					}
				}
			}
			else //list folders at root level
			{
				auto index = CAssetFoldersModel::GetInstance()->FindIndexForFolder("");
				if (index.isValid())
				{
					auto subFolderCount = CAssetFoldersModel::GetInstance()->rowCount(index);
					for (int i = 0; i < subFolderCount; i++)
						mapping.push_back(index.child(i, 0));
				}
			}

			const int rowCount = mapping.count();
			if(rowCount > 0)
			{
				beginInsertRows(QModelIndex(), 0, rowCount - 1);
				m_folders.swap(mapping);
				endInsertRows();
			}
		}
	}
	
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		//this model maps the folder model's attributes to the asset model's attributes
		return CAssetModel::GetHeaderData(section, orientation, role);
	}

	QModelIndex mapToSource(const QModelIndex &proxyIndex) const override
	{
		if (proxyIndex.isValid())
		{
			QModelIndex index = m_folders[proxyIndex.row()];
			return index.sibling(index.row(), proxyIndex.column());
		}
		else
			return QModelIndex();
	}

	QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override
	{
		if(sourceIndex.isValid())
		{
			const auto internalId = sourceIndex.internalId();
			for (int i = 0; i < m_folders.count(); i++)
			{
				if (m_folders[i].internalId() == internalId)
					return createIndex(i, sourceIndex.column(), internalId);
			}
		}

		return QModelIndex();
	}

	QVariant data(const QModelIndex& index, int role) const override
	{
		if (index.isValid())
		{
			switch (role)
			{
			case (int)CAssetModel::Roles::TypeCheckRole:
				return (int)eAssetModelRow_Folder;
			case (int)CAssetModel::Roles::InternalPointerRole:
				return 0; //no internal pointer for folders
			default:
				break;
			}

			switch (index.column())
			{
			case EAssetColumns::eAssetColumns_Name:
			case EAssetColumns::eAssetColumns_Folder:
			case EAssetColumns::eAssetColumns_Thumbnail:
				return m_folders[index.row()].data(role);
			default:
				return QVariant();
			}
		}

		return QVariant();
	}

	virtual bool setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/) override
	{
		if (!index.isValid())
			return false;

		QModelIndex sourceIndex = m_folders[index.row()];

		switch (index.column())
		{
		case EAssetColumns::eAssetColumns_Name:
		case EAssetColumns::eAssetColumns_Thumbnail:
			switch (index.column())
			{
			case EAssetColumns::eAssetColumns_Name:
			case EAssetColumns::eAssetColumns_Folder:
			case EAssetColumns::eAssetColumns_Thumbnail:
				return sourceModel()->setData(sourceIndex, value, role);
			default:
				return false;
			}
		default:
			return false;
		}
	}

	int rowCount(const QModelIndex& parent) const override
	{
		if (!parent.isValid())
			return m_folders.count();
		else
			return 0;
	}

	int columnCount(const QModelIndex& parent) const override 
	{
		return CAssetModel::GetInstance()->GetColumnCount();
	}

	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
	{
		if (row >= 0 && row < m_folders.count())
			return createIndex(row, column, m_folders[row].internalId());
		else
			return QModelIndex();
	}

	//this model is flat
	QModelIndex parent(const QModelIndex& index) const override	{ return QModelIndex(); }

private:
	QVector<QPersistentModelIndex> m_folders;
	CAssetFolderFilterModel* m_parent;
	QVector<QMetaObject::Connection> m_connections;
};


//////////////////////////////////////////////////////////////////////////


CAssetFolderFilterModel::CAssetFolderFilterModel(bool recursiveFilter, bool showFolders, QObject* parent /*= nullptr*/)
	: m_isRecursive(recursiveFilter)
	, m_showFolders(showFolders)
{
	// There is an issue that calls of MountAppend() is orded dependent. 
	// It was found out that swapping the order  MountAppend(m_newAssetModel) <-> MountAppend(m_assetsModel) brings a bug 
	// of refreshing the asset browser view in responce to IFileChangeListener::eChangeType_Renamed_XXX. see CAssetManager::CAssetFileMonitor.    

	SetHeaderDataCallbacks(CAssetModel::GetColumnCount(), &CAssetModel::GetHeaderData, Attributes::s_getAttributeRole);

	m_newAssetModel.reset(new FilteredAssetsModel(this));
	m_newAssetModel->setSourceModel(CNewAssetModel::GetInstance());
	MountAppend(m_newAssetModel.get());
	
	m_foldersModel.reset(new FoldersModel(this));
	MountAppend(m_foldersModel.get());
	//first update of the models is manually triggered
	m_foldersModel->UpdateFolders();

	m_assetsModel.reset(new FilteredAssetsModel(this));
	m_assetsModel->setSourceModel(CAssetModel::GetInstance());
	MountAppend(m_assetsModel.get());
}

void CAssetFolderFilterModel::SetAcceptedFolders(const QStringList& folders)
{
	if (folders.size() == 1)
	{
		const QModelIndex assetsRootIndex = CAssetFoldersModel::GetInstance()->GetProjectAssetsFolderIndex();
		const QString assetsRootPath = assetsRootIndex.data((int)CAssetFoldersModel::Roles::FolderPathRole).toString();

		if (folders.first().compare( assetsRootPath, Qt::CaseInsensitive) == 0)
		{
			// folders has a single element that corresponds to the root asset folder.
			ClearAcceptedFolders();
			return;
		}
	}

	if(m_acceptedFolders != folders)
	{
		m_acceptedFolders = folders;

		//adding trailing / for filtering since CAsset::GetFolder() returns with trailing /
		for (auto& folder : m_acceptedFolders)
		{
			if (!folder.endsWith('/'))
				folder += '/';
		}
		
		m_foldersModel->UpdateFolders();
		m_assetsModel->invalidate();
	}
}

void CAssetFolderFilterModel::ClearAcceptedFolders()
{
	if(m_acceptedFolders.size())
	{
		m_acceptedFolders.clear();
		m_foldersModel->UpdateFolders();
		m_assetsModel->invalidate();
	}
}

void CAssetFolderFilterModel::SetRecursive(bool recursive)
{
	if(recursive != m_isRecursive)
	{
		m_isRecursive = recursive;
		m_assetsModel->invalidate();
	}
}

void CAssetFolderFilterModel::SetShowFolders(bool showFolders)
{
	if (showFolders != m_showFolders)
	{
		m_showFolders = showFolders;
		m_foldersModel->UpdateFolders();
		m_assetsModel->invalidate();
	}
}

