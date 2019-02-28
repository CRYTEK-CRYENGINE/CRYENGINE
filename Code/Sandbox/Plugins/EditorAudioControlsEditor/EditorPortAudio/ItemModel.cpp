// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemModel.h"

#include "Common.h"
#include "Impl.h"

#include <ModelUtils.h>
#include <FileImportInfo.h>
#include <DragDrop.h>
#include <FilePathUtil.h>
#include <QtUtil.h>

#include <QDirIterator>

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
//////////////////////////////////////////////////////////////////////////
bool HasDirValidData(QDir const& dir)
{
	bool hasValidData = false;

	if (dir.exists())
	{
		QDirIterator itFiles(dir.path(), (QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot));

		while (itFiles.hasNext())
		{
			QFileInfo const& fileInfo(itFiles.next());

			if (fileInfo.isFile())
			{
				hasValidData = true;
				break;
			}
		}

		if (!hasValidData)
		{
			QDirIterator itDirs(dir.path(), (QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot));

			while (itDirs.hasNext())
			{
				QDir const& folder(itDirs.next());

				if (HasDirValidData(folder))
				{
					hasValidData = true;
					break;
				}
			}
		}
	}

	return hasValidData;
}

//////////////////////////////////////////////////////////////////////////
void GetFilesFromDir(QDir const& dir, QString const& folderName, FileImportInfos& outFileImportInfos)
{
	if (dir.exists())
	{
		QString const parentFolderName = (folderName.isEmpty() ? (dir.dirName() + "/") : (folderName + dir.dirName() + "/"));

		for (auto const& fileInfo : dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot))
		{
			if (fileInfo.isFile())
			{
				outFileImportInfos.emplace_back(fileInfo, s_supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive), parentFolderName);
			}
		}

		for (auto const& fileInfo : dir.entryInfoList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot))
		{
			QDir const& folder(fileInfo.absoluteFilePath());
			GetFilesFromDir(folder, parentFolderName, outFileImportInfos);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CanDropExternalData(QMimeData const* const pData)
{
	bool hasValidData = false;
	CDragDropData const* const pDragDropData = CDragDropData::FromMimeData(pData);

	if (pDragDropData->HasFilePaths())
	{
		QStringList& allFiles = pDragDropData->GetFilePaths();

		for (auto const& filePath : allFiles)
		{
			QFileInfo const& fileInfo(filePath);

			if (fileInfo.isFile() && s_supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive))
			{
				hasValidData = true;
				break;
			}
		}

		if (!hasValidData)
		{
			for (auto const& filePath : allFiles)
			{
				QDir const& folder(filePath);

				if (HasDirValidData(folder))
				{
					hasValidData = true;
					break;
				}
			}
		}
	}

	return hasValidData;
}

//////////////////////////////////////////////////////////////////////////
bool DropExternalData(QMimeData const* const pData, FileImportInfos& outFileImportInfos)
{
	CRY_ASSERT_MESSAGE(outFileImportInfos.empty(), "Passed container must be empty.");

	if (CanDropExternalData(pData))
	{
		CDragDropData const* const pDragDropData = CDragDropData::FromMimeData(pData);

		if (pDragDropData->HasFilePaths())
		{
			QStringList const& allFiles = pDragDropData->GetFilePaths();

			for (auto const& filePath : allFiles)
			{
				QFileInfo const& fileInfo(filePath);

				if (fileInfo.isFile())
				{
					outFileImportInfos.emplace_back(fileInfo, s_supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive));
				}
				else
				{
					QDir const& folder(filePath);
					GetFilesFromDir(folder, "", outFileImportInfos);
				}
			}
		}
	}

	return !outFileImportInfos.empty();
}

//////////////////////////////////////////////////////////////////////////
QString GetTargetFolderPath(CItem const* const pItem)
{
	QString folderName;

	if ((pItem->GetFlags() & EItemFlags::IsContainer) != 0)
	{
		folderName = QtUtil::ToQString(pItem->GetName());
	}

	auto pParent = static_cast<CItem const*>(pItem->GetParent());

	while ((pParent != nullptr) && ((pParent->GetFlags() & EItemFlags::IsContainer) != 0))
	{
		QString parentName = QtUtil::ToQString(pParent->GetName());

		if (!parentName.isEmpty())
		{
			if (folderName.isEmpty())
			{
				folderName = parentName;
			}
			else
			{
				folderName = parentName + "/" + folderName;
			}
		}

		pParent = static_cast<CItem const*>(pParent->GetParent());
	}

	return folderName;
}

//////////////////////////////////////////////////////////////////////////
CItemModelAttribute* GetAttributeForColumn(CItemModel::EColumns const column)
{
	CItemModelAttribute* pAttribute = nullptr;

	switch (column)
	{
	case CItemModel::EColumns::Notification:
		pAttribute = &ModelUtils::s_notificationAttribute;
		break;
	case CItemModel::EColumns::Connected:
		pAttribute = &ModelUtils::s_connectedAttribute;
		break;
	case CItemModel::EColumns::PakStatus:
		pAttribute = &ModelUtils::s_pakStatus;
		break;
	case CItemModel::EColumns::InPak:
		pAttribute = &ModelUtils::s_inPakAttribute;
		break;
	case CItemModel::EColumns::OnDisk:
		pAttribute = &ModelUtils::s_onDiskAttribute;
		break;
	case CItemModel::EColumns::Name:
		pAttribute = &Attributes::s_nameAttribute;
		break;
	default:
		pAttribute = nullptr;
		break;
	}

	return pAttribute;
}

//////////////////////////////////////////////////////////////////////////
CItemModel::CItemModel(CImpl const& impl, CItem const& rootItem, QObject* const pParent)
	: QAbstractItemModel(pParent)
	, m_impl(impl)
	, m_rootItem(rootItem)
{
	InitIcons();
	ModelUtils::InitIcons();
}

//////////////////////////////////////////////////////////////////////////
void CItemModel::Reset()
{
	beginResetModel();
	endResetModel();
}

//////////////////////////////////////////////////////////////////////////
int CItemModel::rowCount(QModelIndex const& parent) const
{
	int rowCount = 0;
	CItem const* pItem = ItemFromIndex(parent);

	if (pItem == nullptr)
	{
		// If not valid it must be a top level item, so get root.
		pItem = &m_rootItem;
	}

	rowCount = static_cast<int>(pItem->GetNumChildren());

	return rowCount;
}

//////////////////////////////////////////////////////////////////////////
int CItemModel::columnCount(QModelIndex const& parent) const
{
	return static_cast<int>(EColumns::Count);
}

//////////////////////////////////////////////////////////////////////////
QVariant CItemModel::data(QModelIndex const& index, int role) const
{
	QVariant variant;

	if (index.isValid())
	{
		CItem const* const pItem = ItemFromIndex(index);

		if (pItem != nullptr)
		{
			if (role == static_cast<int>(ModelUtils::ERoles::Name))
			{
				variant = QtUtil::ToQString(pItem->GetName());
			}
			else
			{
				EItemFlags const flags = pItem->GetFlags();

				switch (index.column())
				{
				case static_cast<int>(EColumns::Notification):
					{
						switch (role)
						{
						case Qt::DecorationRole:
							if ((flags & (EItemFlags::IsConnected | EItemFlags::IsContainer)) == 0)
							{
								variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NoConnection);
							}
							break;
						case Qt::ToolTipRole:
							if ((flags & (EItemFlags::IsConnected | EItemFlags::IsContainer)) == 0)
							{
								variant = TypeToString(pItem->GetType()) + tr(" is not connected to any audio system control");
							}
							break;
						case static_cast<int>(ModelUtils::ERoles::Id):
							variant = pItem->GetId();
							break;
						default:
							break;
						}
					}
					break;
				case static_cast<int>(EColumns::Connected):
					if ((role == Qt::CheckStateRole) && ((flags& EItemFlags::IsContainer) == 0))
					{
						variant = ((flags& EItemFlags::IsConnected) != 0) ? Qt::Checked : Qt::Unchecked;
					}
					break;
				case static_cast<int>(EColumns::PakStatus):
					{
						switch (role)
						{
						case Qt::DecorationRole:
							{
								EPakStatus const pakStatus = pItem->GetPakStatus();

								if (pakStatus != EPakStatus::None)
								{
									variant = ModelUtils::GetPakStatusIcon(pItem->GetPakStatus());
								}
							}
							break;
						case Qt::ToolTipRole:
							{
								EPakStatus const pakStatus = pItem->GetPakStatus();

								if (pakStatus == (EPakStatus::InPak | EPakStatus::OnDisk))
								{
									variant = TypeToString(pItem->GetType()) + tr(" is in pak and on disk");
								}
								else if ((pakStatus& EPakStatus::InPak) != 0)
								{
									variant = TypeToString(pItem->GetType()) + tr(" is only in pak file");
								}
								else if ((pakStatus& EPakStatus::OnDisk) != 0)
								{
									variant = TypeToString(pItem->GetType()) + tr(" is only on disk");
								}
							}
							break;
						}
					}
					break;
				case static_cast<int>(EColumns::InPak):
					{
						if (role == Qt::CheckStateRole)
						{
							variant = ((pItem->GetPakStatus() & EPakStatus::InPak) != 0) ? Qt::Checked : Qt::Unchecked;
						}
					}
					break;
				case static_cast<int>(EColumns::OnDisk):
					{
						if (role == Qt::CheckStateRole)
						{
							variant = ((pItem->GetPakStatus() & EPakStatus::OnDisk) != 0) ? Qt::Checked : Qt::Unchecked;
						}
					}
					break;
				case static_cast<int>(EColumns::Name):
					{
						switch (role)
						{
						case Qt::DecorationRole:
							variant = GetTypeIcon(pItem->GetType());
							break;
						case Qt::DisplayRole:
						case Qt::ToolTipRole:
							variant = QtUtil::ToQString(pItem->GetName());
							break;
						case static_cast<int>(ModelUtils::ERoles::Id):
							variant = pItem->GetId();
							break;
						case static_cast<int>(ModelUtils::ERoles::SortPriority):
							variant = static_cast<int>(pItem->GetType());
							break;
						case static_cast<int>(ModelUtils::ERoles::IsPlaceholder):
							variant = (flags& EItemFlags::IsPlaceHolder) != 0;
							break;
						default:
							break;
						}
					}
					break;
				}
			}
		}
	}

	return variant;
}

//////////////////////////////////////////////////////////////////////////
QVariant CItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant variant;

	if (orientation == Qt::Horizontal)
	{
		auto const pAttribute = GetAttributeForColumn(static_cast<EColumns>(section));

		if (pAttribute != nullptr)
		{
			switch (role)
			{
			case Qt::DecorationRole:
				if (section == static_cast<int>(EColumns::Notification))
				{
					variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NotificationHeader);
				}
				break;
			case Qt::DisplayRole:
				// For the notification header an icon is used instead of text.
				if (section != static_cast<int>(EColumns::Notification))
				{
					variant = pAttribute->GetName();
				}
				break;
			case Qt::ToolTipRole:
				variant = pAttribute->GetName();
				break;
			case Attributes::s_getAttributeRole:
				variant = QVariant::fromValue(pAttribute);
				break;
			default:
				break;
			}
		}
	}

	return variant;
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CItemModel::flags(QModelIndex const& index) const
{
	Qt::ItemFlags flags = QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled;

	if (index.isValid())
	{
		CItem const* const pItem = ItemFromIndex(index);

		if ((pItem != nullptr) && ((pItem->GetFlags() & EItemFlags::IsPlaceHolder) == 0))
		{
			flags |= Qt::ItemIsDragEnabled;
		}
	}

	return flags;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CItemModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	QModelIndex modelIndex = QModelIndex();

	if ((row >= 0) && (column >= 0))
	{
		CItem const* pParent = ItemFromIndex(parent);

		if (pParent == nullptr)
		{
			pParent = &m_rootItem;
		}

		if ((pParent != nullptr) && (static_cast<int>(pParent->GetNumChildren()) > row))
		{
			auto const pItem = static_cast<CItem const*>(pParent->GetChildAt(row));

			if (pItem != nullptr)
			{
				modelIndex = createIndex(row, column, reinterpret_cast<quintptr>(pItem));
			}
		}
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CItemModel::parent(QModelIndex const& index) const
{
	QModelIndex modelIndex = QModelIndex();

	if (index.isValid())
	{
		CItem const* const pItem = ItemFromIndex(index);

		if (pItem != nullptr)
		{
			modelIndex = IndexFromItem(static_cast<CItem*>(pItem->GetParent()));
		}
	}

	return modelIndex;

}

//////////////////////////////////////////////////////////////////////////
bool CItemModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	return CanDropExternalData(pData);
}

//////////////////////////////////////////////////////////////////////////
bool CItemModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	FileImportInfos fileImportInfos;
	bool const wasDropped = DropExternalData(pData, fileImportInfos);

	if (wasDropped)
	{
		QString targetFolderName;

		if (parent.isValid())
		{
			CItem const* const pItem = ItemFromIndex(parent);

			if (pItem != nullptr)
			{
				targetFolderName = GetTargetFolderPath(pItem);
			}
		}

		m_impl.SignalFilesDropped(fileImportInfos, targetFolderName);
	}

	return wasDropped;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CItemModel::supportedDragActions() const
{
	return Qt::CopyAction;
}

//////////////////////////////////////////////////////////////////////////
QStringList CItemModel::mimeTypes() const
{
	QStringList types = QAbstractItemModel::mimeTypes();
	types << CDragDropData::GetMimeFormatForType(ModelUtils::s_szImplMimeType);
	return types;
}

//////////////////////////////////////////////////////////////////////////
QMimeData* CItemModel::mimeData(QModelIndexList const& indexes) const
{
	auto const pDragDropData = new CDragDropData();
	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);

	QModelIndexList nameIndexes;

	for (QModelIndex const& index : indexes)
	{
		nameIndexes.append(index.sibling(index.row(), static_cast<int>(EColumns::Name)));
	}

	nameIndexes.erase(std::unique(nameIndexes.begin(), nameIndexes.end()), nameIndexes.end());

	for (auto const& index : nameIndexes)
	{
		CItem const* const pItem = ItemFromIndex(index);

		if (pItem != nullptr)
		{
			stream << pItem->GetId();
		}
	}

	pDragDropData->SetCustomData(ModelUtils::s_szImplMimeType, byteArray);
	return pDragDropData;
}

//////////////////////////////////////////////////////////////////////////
CItem* CItemModel::ItemFromIndex(QModelIndex const& index) const
{
	CItem* pItem = nullptr;

	if (index.isValid())
	{
		pItem = static_cast<CItem*>(index.internalPointer());
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CItemModel::IndexFromItem(CItem const* const pItem) const
{
	QModelIndex modelIndex = QModelIndex();

	if (pItem != nullptr)
	{
		auto pParent = static_cast<CItem const*>(pItem->GetParent());

		if (pParent == nullptr)
		{
			pParent = &m_rootItem;
		}

		if (pParent != nullptr)
		{
			size_t const numChildren = pParent->GetNumChildren();

			for (size_t i = 0; i < numChildren; ++i)
			{
				if (pParent->GetChildAt(i) == pItem)
				{
					modelIndex = createIndex(static_cast<int>(i), 0, reinterpret_cast<quintptr>(pItem));
					break;
				}
			}
		}
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
QString CItemModel::GetTargetFolderName(QModelIndex const& index) const
{
	QString targetFolderName;

	if (index.isValid())
	{
		CItem const* const pItem = ItemFromIndex(index);

		if (pItem != nullptr)
		{
			targetFolderName = GetTargetFolderPath(pItem);
		}
	}

	return targetFolderName;
}
} // namespace PortAudio
} // namespace Impl
} // namespace ACE
