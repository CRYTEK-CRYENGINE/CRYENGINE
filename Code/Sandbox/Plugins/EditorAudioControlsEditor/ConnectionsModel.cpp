// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConnectionsModel.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"

#include <ModelUtils.h>
#include <IItem.h>
#include <QtUtil.h>
#include <DragDrop.h>

#include <QApplication>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
bool ProcessDragDropData(QMimeData const* const pData, ControlIds& ids)
{
	CRY_ASSERT_MESSAGE(ids.empty(), "Passed container must be empty.");
	CDragDropData const* const pDragDropData = CDragDropData::FromMimeData(pData);

	if (pDragDropData->HasCustomData(ModelUtils::s_szImplMimeType))
	{
		QByteArray const byteArray = pDragDropData->GetCustomData(ModelUtils::s_szImplMimeType);
		QDataStream stream(byteArray);

		while (!stream.atEnd())
		{
			ControlId id;
			stream >> id;

			if (id != s_aceInvalidId)
			{
				ids.push_back(id);
			}
		}
	}

	return !ids.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CanDropData(QMimeData const* const pData, CAsset const* pControl)
{
	bool canDrop = false;

	if ((g_pIImpl != nullptr) && (pControl != nullptr))
	{
		ControlIds ids;

		if (ProcessDragDropData(pData, ids))
		{
			canDrop = true;

			for (auto const id : ids)
			{
				Impl::IItem const* const pIItem = g_pIImpl->GetItem(id);

				if (pIItem != nullptr)
				{
					if (!(g_pIImpl->IsTypeCompatible(pControl->GetType(), pIItem)))
					{
						canDrop = false;
						break;
					}
				}
			}
		}
	}

	return canDrop;
}

//////////////////////////////////////////////////////////////////////////
CConnectionsModel::CConnectionsModel(QObject* const pParent)
	: QAbstractItemModel(pParent)
	, m_pControl(nullptr)
{
	ConnectSignals();
}

//////////////////////////////////////////////////////////////////////////
CConnectionsModel::~CConnectionsModel()
{
	DisconnectSignals();
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsModel::ConnectSignals()
{
	g_assetsManager.SignalConnectionAdded.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  ResetModelAndCache();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalConnectionRemoved.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  ResetModelAndCache();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationAboutToChange.Connect([this]()
		{
			beginResetModel();
			m_pControl = nullptr;
			m_connectionsCache.clear();
			endResetModel();
	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationChanged.Connect([this]()
		{
			beginResetModel();
			ResetCache();
			endResetModel();
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsModel::DisconnectSignals()
{
	g_assetsManager.SignalConnectionAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalConnectionRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsModel::Init(CControl* const pControl)
{
	beginResetModel();
	m_pControl = pControl;
	ResetCache();
	endResetModel();
}

//////////////////////////////////////////////////////////////////////////
CItemModelAttribute* GetAttributeForColumn(CConnectionsModel::EColumns const column)
{
	CItemModelAttribute* pAttribute = nullptr;

	switch (column)
	{
	case CConnectionsModel::EColumns::Notification:
		pAttribute = &ModelUtils::s_notificationAttribute;
		break;
	case CConnectionsModel::EColumns::Name:
		pAttribute = &Attributes::s_nameAttribute;
		break;
	case CConnectionsModel::EColumns::Path:
		pAttribute = &ModelUtils::s_pathAttribute;
		break;
	default:
		pAttribute = nullptr;
		break;
	}

	return pAttribute;
}

//////////////////////////////////////////////////////////////////////////
int CConnectionsModel::rowCount(QModelIndex const& parent) const
{
	int rowCount = 0;

	if ((m_pControl != nullptr) && (g_pIImpl != nullptr))
	{
		if ((parent.row() < 0) || (parent.column() < 0))
		{
			rowCount = m_connectionsCache.size();
		}
	}

	return rowCount;
}

//////////////////////////////////////////////////////////////////////////
int CConnectionsModel::columnCount(QModelIndex const& parent) const
{
	return static_cast<int>(EColumns::Count);
}

//////////////////////////////////////////////////////////////////////////
QVariant CConnectionsModel::data(QModelIndex const& index, int role) const
{
	QVariant variant;

	if ((g_pIImpl != nullptr) && (m_pControl != nullptr) && index.isValid())
	{
		if (index.row() < m_connectionsCache.size())
		{
			ConnectionPtr const pConnection = m_connectionsCache[index.row()];

			if (pConnection != nullptr)
			{
				Impl::IItem const* const pIItem = g_pIImpl->GetItem(pConnection->GetID());

				if (pIItem != nullptr)
				{
					if (role == static_cast<int>(ModelUtils::ERoles::Name))
					{
						variant = QtUtil::ToQString(pIItem->GetName());
					}
					else
					{
						switch (index.column())
						{
						case static_cast<int>(EColumns::Notification):
							{
								EItemFlags const flags = pIItem->GetFlags();

								switch (role)
								{
								case Qt::DecorationRole:
									if ((flags& EItemFlags::IsPlaceHolder) != 0)
									{
										variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::Placeholder);
									}
									else if ((flags& EItemFlags::IsLocalized) != 0)
									{
										variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::Localized);
									}
									break;
								case Qt::ToolTipRole:
									if ((flags& EItemFlags::IsPlaceHolder) != 0)
									{
										variant = g_pIImpl->GetItemTypeName(pIItem) + tr(" not found in middleware project");
									}
									else if ((flags& EItemFlags::IsLocalized) != 0)
									{
										variant = g_pIImpl->GetItemTypeName(pIItem) + tr(" is localized");
									}
									break;
								case static_cast<int>(ModelUtils::ERoles::Id):
									variant = pIItem->GetId();
									break;
								}
							}
							break;
						case static_cast<int>(EColumns::Name):
							{
								switch (role)
								{
								case Qt::DecorationRole:
									variant = g_pIImpl->GetItemIcon(pIItem);
									break;
								case Qt::DisplayRole:
								case Qt::ToolTipRole:
									variant = QtUtil::ToQString(pIItem->GetName());
									break;
								case static_cast<int>(ModelUtils::ERoles::Id):
									variant = pIItem->GetId();
									break;
								}
							}
							break;
						case static_cast<int>(EColumns::Path):
							{
								switch (role)
								{
								case Qt::DisplayRole:
								case Qt::ToolTipRole:
									{
										QString path;
										Impl::IItem const* pIItemParent = pIItem->GetParent();

										while (pIItemParent != nullptr)
										{
											QString parentName = QtUtil::ToQString(pIItemParent->GetName());

											if (!parentName.isEmpty())
											{
												if (path.isEmpty())
												{
													path = parentName;
												}
												else
												{
													path = parentName + "/" + path;
												}
											}
											pIItemParent = pIItemParent->GetParent();
										}

										variant = path;
									}
									break;
								}
							}
							break;
						}
					}
				}
			}
		}
	}

	return variant;
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionsModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
QVariant CConnectionsModel::headerData(int section, Qt::Orientation orientation, int role) const
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
				if (section == static_cast<int>(EColumns::Notification))
				{
					variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NotificationHeader);
				}
				break;
			case Qt::DisplayRole:
				// The notification column header uses an icons instead of text.
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
Qt::ItemFlags CConnectionsModel::flags(QModelIndex const& index) const
{
	return QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CConnectionsModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	QModelIndex modelIndex = QModelIndex();

	if ((g_pIImpl != nullptr) && (m_pControl != nullptr))
	{
		if ((row >= 0) && (column >= 0))
		{
			if (row < m_connectionsCache.size())
			{
				ConnectionPtr const pConnection = m_connectionsCache[row];

				if (pConnection != nullptr)
				{
					Impl::IItem const* const pIItem = g_pIImpl->GetItem(pConnection->GetID());

					if (pIItem != nullptr)
					{
						modelIndex = createIndex(row, column);
					}
				}
			}
		}
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CConnectionsModel::parent(QModelIndex const& index) const
{
	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionsModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	bool canDrop = CanDropData(pData, m_pControl);
	QString dragText;

	if (canDrop)
	{
		dragText = tr("Connect to ") + QtUtil::ToQString(m_pControl->GetName());
	}
	else
	{
		dragText = tr("Control types are not compatible.");
	}

	CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), dragText);

	return canDrop;
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionsModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	bool wasDropped = false;

	if (CanDropData(pData, m_pControl))
	{
		ControlIds ids;

		if (ProcessDragDropData(pData, ids))
		{
			ControlId lastConnectedId = s_aceInvalidId;

			for (auto const id : ids)
			{
				Impl::IItem* const pIItem = g_pIImpl->GetItem(id);

				if (pIItem != nullptr)
				{
					ConnectionPtr pConnection = m_pControl->GetConnection(pIItem);

					if (pConnection == nullptr)
					{
						pConnection = g_pIImpl->CreateConnectionToControl(m_pControl->GetType(), pIItem);

						if (pConnection != nullptr)
						{
							m_pControl->AddConnection(pConnection);
							lastConnectedId = pConnection->GetID();
							wasDropped = true;
						}
					}
				}
			}

			if (wasDropped)
			{
				SignalConnectionAdded(lastConnectedId);
			}
		}
	}

	return wasDropped;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CConnectionsModel::supportedDropActions() const
{
	return Qt::CopyAction;
}

//////////////////////////////////////////////////////////////////////////
QStringList CConnectionsModel::mimeTypes() const
{
	QStringList types = QAbstractItemModel::mimeTypes();
	types << CDragDropData::GetMimeFormatForType(ModelUtils::s_szImplMimeType);
	return types;
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsModel::ResetCache()
{
	m_connectionsCache.clear();

	if (m_pControl != nullptr)
	{
		size_t const numConnections = m_pControl->GetConnectionCount();

		for (size_t i = 0; i < numConnections; ++i)
		{
			ConnectionPtr const pConnection = m_pControl->GetConnectionAt(i);

			if (pConnection != nullptr)
			{
				m_connectionsCache.push_back(pConnection);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionsModel::ResetModelAndCache()
{
	beginResetModel();
	ResetCache();
	endResetModel();
}
} // namespace ACE

