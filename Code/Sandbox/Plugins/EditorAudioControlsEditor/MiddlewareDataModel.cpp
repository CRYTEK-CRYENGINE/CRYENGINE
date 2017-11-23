// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MiddlewareDataModel.h"
#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "ItemStatusHelper.h"

#include <IEditorImpl.h>
#include <ImplItem.h>
#include <CrySystem/File/CryFile.h>  // Includes CryPath.h in correct order.
#include <QtUtil.h>
#include <CrySandbox/CrySignal.h>
#include <CryIcon.h>

#include <QMimeData>
#include <QDataStream>

namespace ACE
{
char const* const CMiddlewareDataModel::ms_szMimeType = "application/cryengine-audioimplementationitem";

//////////////////////////////////////////////////////////////////////////
CMiddlewareDataModel::CMiddlewareDataModel()
	: m_pEditorImpl(CAudioControlsEditorPlugin::GetImplEditor())
{
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.Connect([&]()
	{
		m_pEditorImpl = CAudioControlsEditorPlugin::GetImplEditor();
		beginResetModel();
		endResetModel();
	});

	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.Connect([&]()
	{
		beginResetModel();
		m_pEditorImpl = nullptr;
		endResetModel();
	});
}

//////////////////////////////////////////////////////////////////////////
int CMiddlewareDataModel::rowCount(QModelIndex const& parent) const
{
	if (m_pEditorImpl != nullptr)
	{
		CImplItem const* pImplItem = ItemFromIndex(parent);

		if (pImplItem == nullptr)
		{
			// if not valid it must be a top level item so get root
			pImplItem = m_pEditorImpl->GetRoot();
		}

		if (pImplItem != nullptr)
		{
			return pImplItem->ChildCount();
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CMiddlewareDataModel::columnCount(QModelIndex const& parent) const
{
	return static_cast<int>(EMiddlewareDataColumns::Count);
}

//////////////////////////////////////////////////////////////////////////
QVariant CMiddlewareDataModel::data(QModelIndex const& index, int role) const
{
	if (m_pEditorImpl != nullptr)
	{
		if (!index.isValid())
		{
			return QVariant();
		}

		CImplItem const* const pImplItem = ItemFromIndex(index);

		if (pImplItem != nullptr)
		{
			switch (index.column())
			{
			case static_cast<int>(EMiddlewareDataColumns::Name):
				{
					switch (role)
					{
					case Qt::DisplayRole:
						return static_cast<char const*>(pImplItem->GetName());
						break;
					case Qt::DecorationRole:
						return CryIcon(m_pEditorImpl->GetTypeIcon(pImplItem));
						break;
					case Qt::ForegroundRole:
						if (pImplItem->IsLocalised())
						{
							return GetItemStatusColor(EItemStatus::Localized);
						}
						else if (!pImplItem->IsConnected() && (m_pEditorImpl->ImplTypeToSystemType(pImplItem) != ESystemItemType::Invalid))
						{
							// Tint non connected controls that can actually be connected to something (ie. exclude folders)
							return GetItemStatusColor(EItemStatus::NoConnection);
						}
						break;
					case Qt::ToolTipRole:
						if (pImplItem->IsLocalised())
						{
							return tr("Item is localized");
						}
						else if (!pImplItem->IsConnected())
						{
							return tr("Item is not connected to any audio system control");
						}
						break;
					case static_cast<int>(EMiddlewareDataAttributes::Connected):
						return pImplItem->IsConnected();
						break;
					case static_cast<int>(EMiddlewareDataAttributes::Container) :
						return pImplItem->IsContainer();
						break;
					case static_cast<int>(EMiddlewareDataAttributes::Placeholder):
						return pImplItem->IsPlaceholder();
						break;
					case static_cast<int>(EMiddlewareDataAttributes::Localized):
						return pImplItem->IsLocalised();
						break;
					case static_cast<int>(EMiddlewareDataAttributes::Id) :
						return pImplItem->GetId();
						break;
					}
				}
				break;
			}
		}
	}
	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
QVariant CMiddlewareDataModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole*/) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
	{
		return tr("Name");
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CMiddlewareDataModel::flags(QModelIndex const& index) const
{
	Qt::ItemFlags flag = QAbstractItemModel::flags(index);

	if (index.isValid() && (m_pEditorImpl != nullptr))
	{
		CImplItem const* const pImplItem = ItemFromIndex(index);

		if ((pImplItem != nullptr) && !pImplItem->IsPlaceholder() && (m_pEditorImpl->ImplTypeToSystemType(pImplItem) != ESystemItemType::NumTypes))
		{
			flag |= Qt::ItemIsDragEnabled;
		}
	}

	return flag;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CMiddlewareDataModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	if (m_pEditorImpl != nullptr)
	{
		if ((row >= 0) && (column >= 0))
		{
			CImplItem const* pParent = ItemFromIndex(parent);

			if (pParent == nullptr)
			{
				pParent = m_pEditorImpl->GetRoot();
			}

			if ((pParent != nullptr) && pParent->ChildCount() > row)
			{
				CImplItem const* const pImplItem = pParent->GetChildAt(row);

				if (pImplItem != nullptr)
				{
					return createIndex(row, column, reinterpret_cast<quintptr>(pImplItem));
				}
			}
		}
	}

	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CMiddlewareDataModel::parent(QModelIndex const& index) const
{
	if (index.isValid())
	{
		CImplItem const* const pItem = ItemFromIndex(index);

		if (pItem != nullptr)
		{
			return IndexFromItem(pItem->GetParent());
		}
	}

	return QModelIndex();

}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CMiddlewareDataModel::supportedDragActions() const
{
	return Qt::CopyAction;
}

//////////////////////////////////////////////////////////////////////////
QStringList CMiddlewareDataModel::mimeTypes() const
{
	QStringList list = QAbstractItemModel::mimeTypes();
	list << ms_szMimeType;
	return list;
}

//////////////////////////////////////////////////////////////////////////
QMimeData* CMiddlewareDataModel::mimeData(QModelIndexList const& indexes) const
{
	QMimeData* const pData = QAbstractItemModel::mimeData(indexes);
	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);

	for (auto const& index : indexes)
	{
		CImplItem const* const pImplItem = ItemFromIndex(index);

		if (pImplItem != nullptr)
		{
			stream << pImplItem->GetId();
		}
	}

	pData->setData(ms_szMimeType, byteArray);
	return pData;
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CMiddlewareDataModel::ItemFromIndex(QModelIndex const& index) const
{
	if ((index.row() < 0) || (index.column() < 0))
	{
		return nullptr;
	}

	return static_cast<CImplItem*>(index.internalPointer());
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CMiddlewareDataModel::IndexFromItem(CImplItem const* const pImplItem) const
{
	if (pImplItem != nullptr)
	{
		CImplItem const* pParent = pImplItem->GetParent();

		if (pParent == nullptr)
		{
			pParent = m_pEditorImpl->GetRoot();
		}

		if (pParent != nullptr)
		{
			int const size = pParent->ChildCount();

			for (int i = 0; i < size; ++i)
			{
				if (pParent->GetChildAt(i) == pImplItem)
				{
					return createIndex(i, 0, reinterpret_cast<quintptr>(pImplItem));
				}
			}
		}
	}

	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataModel::Reset()
{
	beginResetModel();
	endResetModel();
}

//////////////////////////////////////////////////////////////////////////
CMiddlewareDataFilterProxyModel::CMiddlewareDataFilterProxyModel(QObject* parent)
	: QDeepFilterProxyModel(QDeepFilterProxyModel::BehaviorFlags(QDeepFilterProxyModel::AcceptIfChildMatches), parent)
	, m_hideConnected(false)
{
}

//////////////////////////////////////////////////////////////////////////
bool CMiddlewareDataFilterProxyModel::rowMatchesFilter(int source_row, const QModelIndex& source_parent) const
{
	if (QDeepFilterProxyModel::rowMatchesFilter(source_row, source_parent))
	{
		QModelIndex const index = sourceModel()->index(source_row, 0, source_parent);

		if (index.isValid())
		{
			// Hide placeholder
			if (sourceModel()->data(index, static_cast<int>(CMiddlewareDataModel::EMiddlewareDataAttributes::Placeholder)).toBool())
			{
				return false;
			}

			if (m_hideConnected)
			{
				if (sourceModel()->data(index, static_cast<int>(CMiddlewareDataModel::EMiddlewareDataAttributes::Connected)).toBool())
				{
					return false;
				}

				if (sourceModel()->data(index, static_cast<int>(CMiddlewareDataModel::EMiddlewareDataAttributes::Container)).toBool())
				{
					return false;
				}

				if (sourceModel()->hasChildren(index))
				{
					int const rowCount = sourceModel()->rowCount(index);

					for (int i = 0; i < rowCount; ++i)
					{
						if (filterAcceptsRow(i, index))
						{
							return true;
						}
					}

					return false;
				}
			}

			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataFilterProxyModel::SetHideConnected(bool const hideConnected)
{
	m_hideConnected = hideConnected;
	invalidate();
}

//////////////////////////////////////////////////////////////////////////
bool CMiddlewareDataFilterProxyModel::lessThan(QModelIndex const& left, QModelIndex const& right) const
{
	if (left.column() == right.column())
	{
		bool const hasLeftChildren = (sourceModel()->rowCount(left) > 0);
		bool const hasRightChildren = (sourceModel()->rowCount(right) > 0);

		if (hasLeftChildren == hasRightChildren)
		{
			QVariant const& valueLeft = sourceModel()->data(left, Qt::DisplayRole);
			QVariant const& valueRight = sourceModel()->data(right, Qt::DisplayRole);
			return valueLeft < valueRight;
		}
		else
		{
			return !hasRightChildren; //high priority to the one that has children
		}
	}

	return QSortFilterProxyModel::lessThan(left, right);
}

//////////////////////////////////////////////////////////////////////////
namespace AudioModelUtils
{
//////////////////////////////////////////////////////////////////////////
void DecodeImplMimeData(const QMimeData* pData, std::vector<CImplItem*>& outItems)
{
	IEditorImpl const* const pEditorImpl = CAudioControlsEditorPlugin::GetImplEditor();
	QByteArray encoded = pData->data(CMiddlewareDataModel::ms_szMimeType);
	QDataStream stream(&encoded, QIODevice::ReadOnly);
	while (!stream.atEnd())
	{
		CID id;
		stream >> id;

		if (id != ACE_INVALID_ID)
		{
			CImplItem* const pImplControl = pEditorImpl->GetControl(id);

			if (pImplControl != nullptr)
			{
				outItems.emplace_back(pImplControl);
			}
		}
	}
}
} // namespace AudioModelUtils
} // namespace ACE
