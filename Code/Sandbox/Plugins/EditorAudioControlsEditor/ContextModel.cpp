// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ContextModel.h"

#include "Common.h"
#include "Context.h"
#include "ContextManager.h"
#include "AssetsManager.h"
#include "Common/ModelUtils.h"

#include <QtUtil.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CItemModelAttribute* GetAttributeForColumn(CContextModel::EColumns const column)
{
	CItemModelAttribute* pAttribute = nullptr;

	switch (column)
	{
	case CContextModel::EColumns::Notification:
		{
			pAttribute = &ModelUtils::s_notificationAttribute;
			break;
		}
	case CContextModel::EColumns::Name:
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
CContextModel::CContextModel(QObject* const pParent)
	: QAbstractItemModel(pParent)
{
	ConnectSignals();
}

//////////////////////////////////////////////////////////////////////////
CContextModel::~CContextModel()
{
	DisconnectSignals();
}

//////////////////////////////////////////////////////////////////////////
void CContextModel::ConnectSignals()
{
	g_contextManager.SignalOnBeforeContextAdded.Connect([this](CContext* const pContext)
		{
			int const row = static_cast<int>(g_contexts.size());
			beginInsertRows(QModelIndex(), row, row);
		}, reinterpret_cast<uintptr_t>(this));

	g_contextManager.SignalOnAfterContextAdded.Connect([this](CContext* const pContext)
		{
			endInsertRows();
		}, reinterpret_cast<uintptr_t>(this));

	g_contextManager.SignalOnBeforeContextRemoved.Connect([this](CContext const* const pContext)
		{
			size_t const numContexts = g_contexts.size();

			for (size_t i = 0; i < numContexts; ++i)
			{
			  if (g_contexts[i] == pContext)
			  {
			    int const index = static_cast<int>(i);
			    beginRemoveRows(QModelIndex(), index, index);
			    break;
				}
			}
		}, reinterpret_cast<uintptr_t>(this));

	g_contextManager.SignalOnAfterContextRemoved.Connect([this]()
		{
			endRemoveRows();
		}, reinterpret_cast<uintptr_t>(this));

	g_contextManager.SignalOnBeforeClear.Connect([this]()
		{
			beginResetModel();
		}, reinterpret_cast<uintptr_t>(this));

	g_contextManager.SignalOnAfterClear.Connect([this]()
		{
			endResetModel();
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CContextModel::DisconnectSignals()
{
	g_contextManager.SignalOnBeforeContextAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_contextManager.SignalOnAfterContextAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_contextManager.SignalOnBeforeContextRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_contextManager.SignalOnAfterContextRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_contextManager.SignalOnBeforeClear.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_contextManager.SignalOnAfterClear.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
int CContextModel::rowCount(QModelIndex const& parent) const
{
	int rowCount = 0;

	if ((parent.row() < 0) || (parent.column() < 0))
	{
		rowCount = static_cast<int>(g_contexts.size());
	}

	return rowCount;
}

//////////////////////////////////////////////////////////////////////////
int CContextModel::columnCount(QModelIndex const& parent) const
{
	return static_cast<int>(EColumns::Count);
}

//////////////////////////////////////////////////////////////////////////
QVariant CContextModel::data(QModelIndex const& index, int role) const
{
	QVariant variant;

	if (index.isValid() && (index.row() < static_cast<int>(g_contexts.size())))
	{
		CContext const* const pContext = g_contexts[static_cast<size_t>(index.row())];

		if (pContext != nullptr)
		{
			switch (index.column())
			{
			case static_cast<int>(EColumns::Notification):
				{
					switch (role)
					{
					case Qt::DecorationRole:
						{
							if (pContext->IsActive())
							{
								variant = ModelUtils::s_contextActiveIcon;
							}

							break;
						}
					case Qt::ToolTipRole:
						{
							if (pContext->IsActive())
							{
								variant = "Context is active";
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
			case static_cast<int>(EColumns::Name):
				{
					switch (role)
					{
					case Qt::DisplayRole: // Intentional fall-through.
					case Qt::ToolTipRole: // Intentional fall-through.
					case Qt::EditRole:
						{
							variant = QtUtil::ToQString(pContext->GetName());
							break;
						}
					case static_cast<int>(ModelUtils::ERoles::Id):
						{
							variant = pContext->GetId();
							break;
						}
					case static_cast<int>(ModelUtils::ERoles::InternalPointer):
						{
							variant = reinterpret_cast<intptr_t>(pContext);
							break;
						}
					case static_cast<int>(ModelUtils::ERoles::IsDefaultControl):
						{
							variant = pContext->GetId() == CryAudio::GlobalContextId;
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
bool CContextModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	bool wasDataChanged = false;

	if (index.isValid() && (index.column() == static_cast<int>(EColumns::Name)))
	{
		CContext* const pContext = g_contexts[static_cast<size_t>(index.row())];

		if (pContext != nullptr)
		{
			switch (role)
			{
			case Qt::EditRole:
				{
					if (value.canConvert<QString>())
					{
						string const& newName = QtUtil::ToString(value.toString());
						wasDataChanged = g_contextManager.RenameContext(pContext, newName);
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

	return wasDataChanged;
}

//////////////////////////////////////////////////////////////////////////
QVariant CContextModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant variant;

	if (orientation == Qt::Horizontal)
	{
		CItemModelAttribute* const pAttribute = GetAttributeForColumn(static_cast<EColumns>(section));

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
					// The IsActive column header uses an icon instead of text.
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
Qt::ItemFlags CContextModel::flags(QModelIndex const& index) const
{
	Qt::ItemFlags flags = Qt::NoItemFlags;

	if (index.isValid())
	{
		if ((index.column() == static_cast<int>(EColumns::Name)) && !(index.data(static_cast<int>(ModelUtils::ERoles::IsDefaultControl)).toBool()))
		{
			flags = QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
		}
		else
		{
			flags = QAbstractItemModel::flags(index);
		}
	}

	return flags;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CContextModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	QModelIndex modelIndex = QModelIndex();

	if ((row >= 0) && (column >= 0) && (row < static_cast<int>(g_contexts.size())))
	{
		modelIndex = createIndex(row, column);
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CContextModel::parent(QModelIndex const& index) const
{
	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CContextModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CContextModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CContextModel::supportedDropActions() const
{
	return Qt::IgnoreAction;
}

//////////////////////////////////////////////////////////////////////////
void CContextModel::Reset()
{
	beginResetModel();
	endResetModel();
}

//////////////////////////////////////////////////////////////////////////
CContext* CContextModel::GetContextFromIndex(QModelIndex const& index)
{
	CContext* pContext = nullptr;
	QModelIndex const& nameColumnIndex = index.sibling(index.row(), static_cast<int>(EColumns::Name));
	QVariant const internalPtr = nameColumnIndex.data(static_cast<int>(ModelUtils::ERoles::InternalPointer));

	if (internalPtr.isValid())
	{
		pContext = reinterpret_cast<CContext*>(internalPtr.value<intptr_t>());
	}

	return pContext;
}
} // namespace ACE
