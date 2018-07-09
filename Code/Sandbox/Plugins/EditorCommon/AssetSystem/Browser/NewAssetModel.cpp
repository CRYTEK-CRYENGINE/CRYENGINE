// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NewAssetModel.h"

#include "AssetSystem/AssetType.h"
#include "AssetModel.h"

#include "QtUtil.h"
#include "FilePathUtil.h"
#include "QThumbnailView.h"

#include <AssetSystem/AssetManager.h>
#include <QDirIterator>

namespace Private_NewAssetModel
{
	//! Returns a list of asset names in folder without extension.
	//! \param folder Path relative to asset directory.
	//! \param assetFileExtension Without trailing cryasset. See CAssetType::GetFileExtension.
	std::vector<string> GetAllAssetNamesInFolder(const string& folderPath, const string& assetFileExtension)
	{
		std::vector<string> resultSet;
		const string absFolder = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), folderPath);
		const size_t absFolderLen = PathUtil::AddSlash(absFolder).size();
		const string mask = string().Format("*.%s.cryasset", assetFileExtension.c_str());
		QDirIterator iterator(QtUtil::ToQString(absFolder), QStringList() << mask.c_str(), QDir::Files, QDirIterator::Subdirectories);
		while (iterator.hasNext())
		{
			string filePath = QtUtil::ToString(iterator.next()).substr(absFolderLen); // Remove leading path to search directory.
																					  // Removing two extensions gives A from A.<ext>.cryasset.
			PathUtil::RemoveExtension(filePath);
			PathUtil::RemoveExtension(filePath);
			resultSet.push_back(filePath);
		}
		return resultSet;
	}
}

CNewAssetModel::CNewAssetModel()
{
	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.Connect([this](const std::vector<CAsset*>& assets)
	{
		if (std::find(assets.begin(), assets.end(), m_pAsset) != assets.end())
		{
			m_pAsset = nullptr;
		}
	}, (uintptr_t)this);
}

CAsset* CNewAssetModel::GetNewAsset() const
{
	return m_pAsset;
}

void CNewAssetModel::BeginCreateAsset(const string& path, const string& name, const CAssetType& type, const void* pTypeSpecificParameter)
{
	if (m_pRow)
	{
		return;
	}

	m_defaultName = name;
	if (m_defaultName.empty())
	{
		m_defaultName = "Untitled";
	}

	beginResetModel();

	m_pRow.reset(new SAssetDescription());
	m_pRow->path = path;
	m_pRow->name = name;
	m_pRow->pAssetType = &type;
	m_pRow->pTypeSpecificParameter = pTypeSpecificParameter;

	endResetModel();
}

void CNewAssetModel::EndCreateAsset()
{
	if (!m_pRow)
	{
		return;
	}

	if (m_pRow->bChanged)
	{
		using namespace Private_NewAssetModel;

		string rowName = m_pRow->name;
		if (rowName.empty())
		{
			rowName = m_defaultName;
		}

		const string name = PathUtil::GetUniqueName(rowName, GetAllAssetNamesInFolder(m_pRow->path, m_pRow->pAssetType->GetFileExtension()));

		const string assetPath = string().Format("%s.%s.cryasset", PathUtil::Make(m_pRow->path, name), m_pRow->pAssetType->GetFileExtension());
		const CAssetType* const pAssetType = m_pRow->pAssetType;

		if (pAssetType->Create(assetPath, m_pRow->pTypeSpecificParameter))
		{
			m_pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(assetPath);
		}
	}

	beginResetModel();

	m_pRow.reset(nullptr);

	endResetModel();
}

int CNewAssetModel::rowCount(const QModelIndex& parent) const
{
	return !parent.isValid() && m_pRow ? 1 : 0;
}

int CNewAssetModel::columnCount(const QModelIndex& parent) const
{
	return CAssetModel::GetColumnCount();
}

QVariant CNewAssetModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	CRY_ASSERT(m_pRow);

	if (role == (int)CAssetModel::Roles::TypeCheckRole)
	{
		return (int)eAssetModelRow_Asset;
	}

	if (index.column() == eAssetColumns_Name || index.column() == eAssetColumns_Thumbnail)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			return QtUtil::ToQString(m_pRow->name);
		case Qt::DecorationRole:
			// Fall-through.
		case QThumbnailsView::s_ThumbnailRole:
			return m_pRow->pAssetType->GetIcon();
		default:
			break;
		};
	}
	else if (index.column() == eAssetColumns_Type)
	{
		if (role == Qt::DisplayRole)
		{
			return QString(m_pRow->pAssetType->GetUiTypeName());
		}
	}
	else if (index.column() == eAssetColumns_Folder)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			return QtUtil::ToQString(PathUtil::AddSlash(m_pRow->path));
		default:
			break;
		}
	}

	return QVariant();
}

bool CNewAssetModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (index.isValid() && role == Qt::EditRole && value.canConvert<QString>())
	{
		CRY_ASSERT(m_pRow);
		m_pRow->name = QtUtil::ToString(value.toString());
		m_pRow->bChanged = true;
		return true;
	}
	return false;
}

QModelIndex CNewAssetModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!parent.isValid())
	{	
		return createIndex(row, column, nullptr);
	}
	return QModelIndex();
}

QModelIndex CNewAssetModel::parent(const QModelIndex&) const
{
	return QModelIndex();
}

QVariant CNewAssetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	return CAssetModel::GetHeaderData(section, orientation, role);
}

Qt::ItemFlags CNewAssetModel::flags(const QModelIndex& index) const
{
	return Qt::ItemIsEditable | QAbstractItemModel::flags(index);
}


