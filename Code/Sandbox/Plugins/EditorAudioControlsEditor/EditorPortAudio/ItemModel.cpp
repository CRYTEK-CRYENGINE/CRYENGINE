// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemModel.h"

#include "Common.h"
#include "Impl.h"
#include "../Common/ModelUtils.h"
#include "../Common/FileImportInfo.h"

#include <DragDrop.h>
#include <PathUtils.h>
#include <QtUtil.h>

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
using Items = std::vector<CItem const*>;

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
		{
			pAttribute = &ModelUtils::s_notificationAttribute;
			break;
		}
	case CItemModel::EColumns::Connected:
		{
			pAttribute = &ModelUtils::s_connectedAttribute;
			break;
		}
	case CItemModel::EColumns::PakStatus:
		{
			pAttribute = &ModelUtils::s_pakStatus;
			break;
		}
	case CItemModel::EColumns::InPak:
		{
			pAttribute = &ModelUtils::s_inPakAttribute;
			break;
		}
	case CItemModel::EColumns::OnDisk:
		{
			pAttribute = &ModelUtils::s_onDiskAttribute;
			break;
		}
	case CItemModel::EColumns::Localized:
		{
			pAttribute = &ModelUtils::s_localizedAttribute;
			break;
		}
	case CItemModel::EColumns::Name:
		{
			pAttribute = &Attributes::s_nameAttribute;
			break;
		}
	default:
		{
			pAttribute = nullptr;
			break;
		}
	}

	return pAttribute;
}

//////////////////////////////////////////////////////////////////////////
void GetTopLevelSelectedIds(Items& items, ControlIds& ids)
{
	for (auto const pItem : items)
	{
		// Check if item has ancestors that are also selected
		bool isAncestorAlsoSelected = false;

		for (auto const pOtherItem : items)
		{
			if (pItem != pOtherItem)
			{
				// Find if pOtherItem is the ancestor of pItem
				auto pParent = static_cast<CItem*>(pItem->GetParent());

				while (pParent != nullptr)
				{
					if (pParent == pOtherItem)
					{
						break;
					}

					pParent = static_cast<CItem*>(pParent->GetParent());
				}

				if (pParent != nullptr)
				{
					isAncestorAlsoSelected = true;
					break;
				}
			}
		}

		if (!isAncestorAlsoSelected)
		{
			ids.push_back(pItem->GetId());
		}
	}
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
			switch (index.column())
			{
			case static_cast<int>(EColumns::Notification):
				{
					switch (role)
					{
					case Qt::DecorationRole:
						{
							EItemFlags const flags = pItem->GetFlags();

							if ((flags & (EItemFlags::IsConnected | EItemFlags::IsContainer)) == 0)
							{
								variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NoConnection);
							}
							else if ((flags& EItemFlags::IsLocalized) != 0)
							{
								variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::Localized);
							}

							break;
						}
					case Qt::ToolTipRole:
						{
							EItemFlags const flags = pItem->GetFlags();

							if ((flags & (EItemFlags::IsConnected | EItemFlags::IsContainer)) == 0)
							{
								variant = TypeToString(pItem->GetType()) + tr(" is not connected to any audio system control");
							}
							else if ((flags& EItemFlags::IsLocalized) != 0)
							{
								variant = TypeToString(pItem->GetType()) + tr(" is localized");
							}

							break;
						}
					case static_cast<int>(ModelUtils::ERoles::Id):
						{
							variant = pItem->GetId();
							break;
						}
					default:
						{
							break;
						}
					}

					break;
				}
			case static_cast<int>(EColumns::Connected):
				{
					EItemFlags const flags = pItem->GetFlags();

					if ((role == Qt::CheckStateRole) && ((flags& EItemFlags::IsContainer) == 0))
					{
						variant = ((flags& EItemFlags::IsConnected) != 0) ? Qt::Checked : Qt::Unchecked;
					}

					break;
				}
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

							break;
						}
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

							break;
						}
					default:
						{
							break;
						}
					}

					break;
				}
			case static_cast<int>(EColumns::InPak):
				{
					if (role == Qt::CheckStateRole)
					{
						variant = ((pItem->GetPakStatus() & EPakStatus::InPak) != 0) ? Qt::Checked : Qt::Unchecked;
					}

					break;
				}
			case static_cast<int>(EColumns::OnDisk):
				{
					if (role == Qt::CheckStateRole)
					{
						variant = ((pItem->GetPakStatus() & EPakStatus::OnDisk) != 0) ? Qt::Checked : Qt::Unchecked;
					}

					break;
				}
			case static_cast<int>(EColumns::Localized):
				{
					if (role == Qt::CheckStateRole)
					{
						variant = ((pItem->GetFlags() & EItemFlags::IsLocalized) != 0) ? Qt::Checked : Qt::Unchecked;
					}

					break;
				}
			case static_cast<int>(EColumns::Name):
				{
					switch (role)
					{
					case Qt::DecorationRole:
						{
							variant = GetTypeIcon(pItem->GetType());
							break;
						}
					case Qt::DisplayRole: // Intentional fall-through.
					case Qt::ToolTipRole:
						{
							variant = QtUtil::ToQString(pItem->GetName());
							break;
						}
					case static_cast<int>(ModelUtils::ERoles::Id):
						{
							variant = pItem->GetId();
							break;
						}
					case static_cast<int>(ModelUtils::ERoles::SortPriority):
						{
							variant = static_cast<int>(pItem->GetType());
							break;
						}
					case static_cast<int>(ModelUtils::ERoles::IsPlaceholder):
						{
							variant = (pItem->GetFlags() & EItemFlags::IsPlaceHolder) != 0;
							break;
						}
					case static_cast<int>(ModelUtils::ERoles::InternalPointer):
						{
							variant = reinterpret_cast<intptr_t>(pItem);
							break;
						}
					default:
						{
							break;
						}
					}

					break;
				}
			default:
				{
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
				{
					if (section == static_cast<int>(EColumns::Notification))
					{
						variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NotificationHeader);
					}

					break;
				}
			case Qt::DisplayRole:
				{
					// For the notification header an icon is used instead of text.
					if (section != static_cast<int>(EColumns::Notification))
					{
						variant = pAttribute->GetName();
					}

					break;
				}
			case Qt::ToolTipRole:
				{
					variant = pAttribute->GetName();
					break;
				}
			case Attributes::s_getAttributeRole:
				{
					variant = QVariant::fromValue(pAttribute);
					break;
				}
			default:
				{
					break;
				}
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
	return m_impl.CanDropExternalData(pData);
}

//////////////////////////////////////////////////////////////////////////
bool CItemModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	FileImportInfos fileImportInfos;
	bool const wasDropped = m_impl.DropExternalData(pData, fileImportInfos);

	if (wasDropped)
	{
		QString targetFolderName;
		bool isLocalized = false;

		if (parent.isValid())
		{
			CItem const* const pItem = ItemFromIndex(parent);

			if (pItem != nullptr)
			{
				targetFolderName = GetTargetFolderPath(pItem);
				isLocalized = ((pItem->GetFlags() & EItemFlags::IsLocalized) != 0);
			}
		}

		m_impl.SignalFilesDropped(fileImportInfos, targetFolderName, isLocalized);
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

	Items items;
	items.reserve(static_cast<size_t>(nameIndexes.size()));

	for (auto const& index : nameIndexes)
	{
		CItem const* const pItem = ItemFromIndex(index);

		if (pItem != nullptr)
		{
			items.push_back(pItem);
		}
	}

	ControlIds ids;
	ids.reserve(items.size());

	GetTopLevelSelectedIds(items, ids);

	for (auto const id : ids)
	{
		stream << id;
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
QString CItemModel::GetTargetFolderName(QModelIndex const& index, bool& isLocalized) const
{
	QString targetFolderName;

	if (index.isValid())
	{
		CItem const* const pItem = ItemFromIndex(index);

		if (pItem != nullptr)
		{
			targetFolderName = GetTargetFolderPath(pItem);
			isLocalized = ((pItem->GetFlags() & EItemFlags::IsLocalized) != 0);
		}
	}

	return targetFolderName;
}

//////////////////////////////////////////////////////////////////////////
CItem* CItemModel::GetItemFromIndex(QModelIndex const& index)
{
	CItem* pItem = nullptr;

	QModelIndex const& nameColumnIndex = index.sibling(index.row(), static_cast<int>(EColumns::Name));
	QVariant const internalPtr = nameColumnIndex.data(static_cast<int>(ModelUtils::ERoles::InternalPointer));

	if (internalPtr.isValid())
	{
		pItem = reinterpret_cast<CItem*>(internalPtr.value<intptr_t>());
	}

	return pItem;
}
} // namespace PortAudio
} // namespace Impl
} // namespace ACE
