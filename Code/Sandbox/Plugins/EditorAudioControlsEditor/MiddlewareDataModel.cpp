// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MiddlewareDataModel.h"
#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "ModelUtils.h"

#include <IItem.h>
#include <CryIcon.h>

#include <DragDrop.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CMiddlewareDataModel::CMiddlewareDataModel(QObject* const pParent)
	: QAbstractItemModel(pParent)
{
	ConnectSignals();
}

//////////////////////////////////////////////////////////////////////////
CMiddlewareDataModel::~CMiddlewareDataModel()
{
	DisconnectSignals();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataModel::ConnectSignals()
{
	g_implementationManager.SignalImplementationAboutToChange.Connect([this]()
		{
			beginResetModel();
			endResetModel();
	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationChanged.Connect([this]()
		{
			beginResetModel();
			endResetModel();
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataModel::DisconnectSignals()
{
	g_implementationManager.SignalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataModel::Reset()
{
	beginResetModel();
	endResetModel();
}

//////////////////////////////////////////////////////////////////////////
CItemModelAttribute* CMiddlewareDataModel::GetAttributeForColumn(EColumns const column)
{
	CItemModelAttribute* pAttribute = nullptr;

	switch (column)
	{
	case EColumns::Notification:
		pAttribute = &ModelUtils::s_notificationAttribute;
		break;
	case EColumns::Connected:
		pAttribute = &ModelUtils::s_connectedAttribute;
		break;
	case EColumns::Localized:
		pAttribute = &ModelUtils::s_localizedAttribute;
		break;
	case EColumns::Name:
		pAttribute = &Attributes::s_nameAttribute;
		break;
	default:
		pAttribute = nullptr;
		break;
	}

	return pAttribute;
}

//////////////////////////////////////////////////////////////////////////
QVariant CMiddlewareDataModel::GetHeaderData(int const section, Qt::Orientation const orientation, int const role)
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
					variant = CryIcon(ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NotificationHeader));
				}
				break;
			case Qt::DisplayRole:
				// For Notification we use an icons instead.
				if (section == static_cast<int>(EColumns::Name))
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
int CMiddlewareDataModel::rowCount(QModelIndex const& parent) const
{
	int rowCount = 0;

	if (g_pIImpl != nullptr)
	{
		Impl::IItem const* pIItem = ItemFromIndex(parent);

		if (pIItem == nullptr)
		{
			// If not valid it must be a top level item, so get root.
			pIItem = g_pIImpl->GetRoot();
		}

		rowCount = pIItem->GetNumChildren();
	}

	return rowCount;
}

//////////////////////////////////////////////////////////////////////////
int CMiddlewareDataModel::columnCount(QModelIndex const& parent) const
{
	return static_cast<int>(EColumns::Count);
}

//////////////////////////////////////////////////////////////////////////
QVariant CMiddlewareDataModel::data(QModelIndex const& index, int role) const
{
	QVariant variant;

	if (index.isValid())
	{
		Impl::IItem const* const pIItem = ItemFromIndex(index);

		if (pIItem != nullptr)
		{
			if (role == static_cast<int>(ModelUtils::ERoles::Name))
			{
				variant = static_cast<char const*>(pIItem->GetName());
			}
			else
			{
				EItemFlags const flags = pIItem->GetFlags();

				switch (index.column())
				{
				case static_cast<int>(EColumns::Notification):
					{
						switch (role)
						{
						case Qt::DecorationRole:
							if ((flags & (EItemFlags::IsConnected | EItemFlags::IsContainer)) == 0)
							{
								variant = CryIcon(ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NoConnection));
							}
							else if ((flags& EItemFlags::IsLocalized) != 0)
							{
								variant = CryIcon(ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::Localized));
							}
							break;
						case Qt::ToolTipRole:
							if ((flags & (EItemFlags::IsConnected | EItemFlags::IsContainer)) == 0)
							{
								variant = tr("Item is not connected to any audio system control");
							}
							else if ((flags& EItemFlags::IsLocalized) != 0)
							{
								variant = tr("Item is localized");
							}
							break;
						case static_cast<int>(ModelUtils::ERoles::Id):
							variant = pIItem->GetId();
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
				case static_cast<int>(EColumns::Localized):
					if ((role == Qt::CheckStateRole) && ((flags& EItemFlags::IsContainer) == 0))
					{
						variant = ((flags& EItemFlags::IsLocalized) != 0) ? Qt::Checked : Qt::Unchecked;
					}
					break;
				case static_cast<int>(EColumns::Name):
					{
						switch (role)
						{
						case Qt::DecorationRole:
							variant = CryIcon(g_pIImpl->GetTypeIcon(pIItem));
							break;
						case Qt::DisplayRole:
						case Qt::ToolTipRole:
							variant = static_cast<char const*>(pIItem->GetName());
							break;
						case static_cast<int>(ModelUtils::ERoles::Id):
							variant = pIItem->GetId();
							break;
						case static_cast<int>(ModelUtils::ERoles::SortPriority):
							variant = pIItem->GetSortPriority();
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
QVariant CMiddlewareDataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	return GetHeaderData(section, orientation, role);
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CMiddlewareDataModel::flags(QModelIndex const& index) const
{
	Qt::ItemFlags flags = QAbstractItemModel::flags(index);

	if (index.isValid())
	{
		Impl::IItem const* const pIItem = ItemFromIndex(index);

		if ((pIItem != nullptr) && ((pIItem->GetFlags() & EItemFlags::IsPlaceHolder) == 0))
		{
			flags |= Qt::ItemIsDragEnabled;
		}
	}

	return flags;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CMiddlewareDataModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	QModelIndex modelIndex = QModelIndex();

	if ((row >= 0) && (column >= 0))
	{
		Impl::IItem const* pParent = ItemFromIndex(parent);

		if (pParent == nullptr)
		{
			pParent = g_pIImpl->GetRoot();
		}

		if ((pParent != nullptr) && pParent->GetNumChildren() > row)
		{
			Impl::IItem const* const pIItem = pParent->GetChildAt(row);

			if (pIItem != nullptr)
			{
				modelIndex = createIndex(row, column, reinterpret_cast<quintptr>(pIItem));
			}
		}
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CMiddlewareDataModel::parent(QModelIndex const& index) const
{
	QModelIndex modelIndex = QModelIndex();

	if (index.isValid())
	{
		Impl::IItem const* const pIItem = ItemFromIndex(index);

		if (pIItem != nullptr)
		{
			modelIndex = IndexFromItem(pIItem->GetParent());
		}
	}

	return modelIndex;

}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CMiddlewareDataModel::supportedDragActions() const
{
	return Qt::CopyAction;
}

//////////////////////////////////////////////////////////////////////////
QStringList CMiddlewareDataModel::mimeTypes() const
{
	QStringList types = QAbstractItemModel::mimeTypes();
	types << CDragDropData::GetMimeFormatForType(ModelUtils::s_szImplMimeType);
	return types;
}

//////////////////////////////////////////////////////////////////////////
QMimeData* CMiddlewareDataModel::mimeData(QModelIndexList const& indexes) const
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
		Impl::IItem const* const pIItem = ItemFromIndex(index);

		if (pIItem != nullptr)
		{
			stream << pIItem->GetId();
		}
	}

	pDragDropData->SetCustomData(ModelUtils::s_szImplMimeType, byteArray);
	return pDragDropData;
}

//////////////////////////////////////////////////////////////////////////
Impl::IItem* CMiddlewareDataModel::ItemFromIndex(QModelIndex const& index) const
{
	Impl::IItem* pIItem = nullptr;

	if (index.isValid())
	{
		pIItem = static_cast<Impl::IItem*>(index.internalPointer());
	}

	return pIItem;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CMiddlewareDataModel::IndexFromItem(Impl::IItem const* const pIItem) const
{
	QModelIndex modelIndex = QModelIndex();

	if (pIItem != nullptr)
	{
		Impl::IItem const* pParent = pIItem->GetParent();

		if (pParent == nullptr)
		{
			pParent = g_pIImpl->GetRoot();
		}

		if (pParent != nullptr)
		{
			size_t const numChildren = pParent->GetNumChildren();

			for (size_t i = 0; i < numChildren; ++i)
			{
				if (pParent->GetChildAt(i) == pIItem)
				{
					modelIndex = createIndex(static_cast<int>(i), 0, reinterpret_cast<quintptr>(pIItem));
					break;
				}
			}
		}
	}

	return modelIndex;
}
} // namespace ACE

