// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConnectionsModel.h"

#include "SystemAssets.h"
#include "AudioControlsEditorPlugin.h"
#include "MiddlewareDataModel.h"
#include "ImplementationManager.h"
#include "ItemStatusHelper.h"

#include <IEditorImpl.h>
#include <ImplItem.h>
#include <IUndoObject.h>
#include <CrySystem/File/CryFile.h>  // Includes CryPath.h in correct order.
#include <CryIcon.h>
#include <QtUtil.h>
#include <ConfigurationManager.h>

#include <QMimeData>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CConnectionModel::CConnectionModel()
	: m_pControl(nullptr)
	, m_pEditorImpl(CAudioControlsEditorPlugin::GetImplEditor())
	, m_pAssetsManager(CAudioControlsEditorPlugin::GetAssetsManager())
{
	auto resetFunction = [&]()
	{
		beginResetModel();
		ResetCache();
		endResetModel();
	};

	m_pAssetsManager->signalItemAdded.Connect(resetFunction, reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemRemoved.Connect(resetFunction, reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalControlModified.Connect(resetFunction, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.Connect([&]()
		{
			beginResetModel();
			m_pEditorImpl = nullptr;
			m_connectionsCache.clear();
			endResetModel();
	  }, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.Connect([&]()
		{
			m_pEditorImpl = CAudioControlsEditorPlugin::GetImplEditor();
			beginResetModel();
			ResetCache();
			endResetModel();
	  }, reinterpret_cast<uintptr_t>(this));

	std::vector<dll_string> const& platforms = GetIEditor()->GetConfigurationManager()->GetPlatformNames();

	for (auto const& platform : platforms)
	{
		m_platformNames.emplace_back(QtUtil::ToQStringSafe(platform.c_str()));
	}
}

//////////////////////////////////////////////////////////////////////////
CConnectionModel::~CConnectionModel()
{
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalControlModified.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CConnectionModel::Init(CSystemControl* const pControl)
{
	beginResetModel();
	m_pControl = pControl;
	ResetCache();
	endResetModel();
}

//////////////////////////////////////////////////////////////////////////
int CConnectionModel::rowCount(QModelIndex const& parent) const
{
	if ((m_pControl != nullptr) && (m_pEditorImpl != nullptr))
	{
		if ((parent.row() < 0) || (parent.column() < 0))
		{
			return m_connectionsCache.size();
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CConnectionModel::columnCount(QModelIndex const& parent) const
{
	return static_cast<int>(EConnectionModelColumns::Size) + static_cast<int>(m_platformNames.size());
}

//////////////////////////////////////////////////////////////////////////
QVariant CConnectionModel::data(QModelIndex const& index, int role) const
{
	if ((m_pEditorImpl != nullptr) && (m_pControl != nullptr) && index.isValid())
	{
		if (index.row() < m_connectionsCache.size())
		{
			ConnectionPtr const pConnection = m_connectionsCache[index.row()];

			if (pConnection != nullptr)
			{
				CImplItem const* const pImplItem = m_pEditorImpl->GetControl(pConnection->GetID());

				if (pImplItem != nullptr)
				{
					switch (role)
					{
					case Qt::DisplayRole:
						switch (index.column())
						{
						case static_cast<int>(EConnectionModelColumns::Name):
							return static_cast<char const*>(pImplItem->GetName());
						case static_cast<int>(EConnectionModelColumns::Path):
							{
								QString path;
								CImplItem const* pImplItemParent = pImplItem->GetParent();

								while (pImplItemParent != nullptr)
								{
									QString parentName = QString(static_cast<char const*>(pImplItemParent->GetName()));
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
									pImplItemParent = pImplItemParent->GetParent();
								}

								return path;
							}
						}
						break;
					case Qt::DecorationRole:
						if (index.column() == static_cast<int>(EConnectionModelColumns::Name))
						{
							return CryIcon(m_pEditorImpl->GetTypeIcon(pImplItem));
						}
						break;
					case Qt::ForegroundRole:
						if (pImplItem->IsPlaceholder())
						{
							return GetItemStatusColor(EItemStatus::Placeholder);
						}
						break;
					case Qt::ToolTipRole:
						if (pImplItem->IsPlaceholder())
						{
							return tr("Control not found in the audio middleware project");
						}
						break;
					case Qt::CheckStateRole:
						{
							if ((m_pControl->GetType() == ESystemItemType::Preload) && (index.column() >= static_cast<int>(EConnectionModelColumns::Size)))
							{
								return pConnection->IsPlatformEnabled(index.column() - static_cast<int>(EConnectionModelColumns::Size)) ? Qt::Checked : Qt::Unchecked;
							}
							break;
						}
					case static_cast<int>(EConnectionModelRoles::Id):
						if (index.column() == static_cast<int>(EConnectionModelColumns::Name))
						{
							return pImplItem->GetId();
						}
						break;
					}
				}
			}
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
QVariant CConnectionModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole*/) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
	{
		if (section < static_cast<int>(EConnectionModelColumns::Size))
		{
			switch (section)
			{
			case static_cast<int>(EConnectionModelColumns::Name):
				return tr("Name");
			case static_cast<int>(EConnectionModelColumns::Path):
				return tr("Path");
			}
		}
		else
		{
			return m_platformNames[section - static_cast<int>(EConnectionModelColumns::Size)];
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CConnectionModel::flags(QModelIndex const& index) const
{
	Qt::ItemFlags flags = QAbstractItemModel::flags(index);

	if (index.isValid() && (index.column() >= static_cast<int>(EConnectionModelColumns::Size)))
	{
		flags |= Qt::ItemIsUserCheckable;
	}

	return flags | Qt::ItemIsDropEnabled;
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	if ((index.column() >= static_cast<int>(EConnectionModelColumns::Size)) && (role == Qt::CheckStateRole))
	{
		ConnectionPtr const pConnection = m_connectionsCache[index.row()];
		pConnection->EnableForPlatform(index.column() - static_cast<int>(EConnectionModelColumns::Size), value == Qt::Checked);
		QVector<int> roleVector(1, role);
		dataChanged(index, index, roleVector);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CConnectionModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	if ((m_pEditorImpl != nullptr) && (m_pControl != nullptr))
	{
		if ((row >= 0) && (column >= 0))
		{
			if (row < m_connectionsCache.size())
			{
				ConnectionPtr const pConnection = m_connectionsCache[row];

				if (pConnection != nullptr)
				{
					CImplItem const* const pImplItem = m_pEditorImpl->GetControl(pConnection->GetID());

					if (pImplItem != nullptr)
					{
						return createIndex(row, column);
					}
				}
			}
		}
	}

	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CConnectionModel::parent(QModelIndex const& index) const
{
	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	if ((m_pEditorImpl != nullptr) && (m_pControl != nullptr))
	{
		std::vector<CID> ids;
		DecodeMimeData(pData, ids);

		for (auto const id : ids)
		{
			CImplItem const* const pImplItem = m_pEditorImpl->GetControl(id);

			if (pImplItem != nullptr)
			{
				// is the type being dragged compatible?
				if (!(m_pEditorImpl->IsTypeCompatible(m_pControl->GetType(), pImplItem)))
				{
					return false;
				}
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
QStringList CConnectionModel::mimeTypes() const
{
	QStringList list = QAbstractItemModel::mimeTypes();
	list << CMiddlewareDataModel::ms_szMimeType;
	return list;
}

//////////////////////////////////////////////////////////////////////////
bool CConnectionModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	if ((m_pEditorImpl != nullptr) && (m_pControl != nullptr))
	{
		std::vector<CID> ids;
		DecodeMimeData(pData, ids);
		bool wasConnectionAdded = false;

		for (auto const id : ids)
		{
			CImplItem* const pImplItem = m_pEditorImpl->GetControl(id);

			if (pImplItem != nullptr)
			{
				ConnectionPtr pConnection = m_pControl->GetConnection(pImplItem);

				if (pConnection == nullptr)
				{
					pConnection = m_pEditorImpl->CreateConnectionToControl(m_pControl->GetType(), pImplItem);

					if (pConnection != nullptr)
					{
						m_pControl->AddConnection(pConnection);
						wasConnectionAdded = true;
					}
				}
			}
		}

		return wasConnectionAdded;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CConnectionModel::supportedDropActions() const
{
	return Qt::CopyAction;
}

//////////////////////////////////////////////////////////////////////////
void CConnectionModel::ResetCache()
{
	m_connectionsCache.clear();

	if (m_pControl != nullptr)
	{
		int const size = m_pControl->GetConnectionCount();

		for (int i = 0; i < size; ++i)
		{
			ConnectionPtr const pConnection = m_pControl->GetConnectionAt(i);

			if (pConnection != nullptr)
			{
				m_connectionsCache.emplace_back(pConnection);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CConnectionModel::DecodeMimeData(QMimeData const* pData, std::vector<CID>& ids) const
{
	QString const format = CMiddlewareDataModel::ms_szMimeType;

	if (pData->hasFormat(format))
	{
		QByteArray encoded = pData->data(format);
		QDataStream stream(&encoded, QIODevice::ReadOnly);

		while (!stream.atEnd())
		{
			CID id;
			stream >> id;

			if (id != ACE_INVALID_ID)
			{
				ids.emplace_back(id);
			}
		}
	}
}
} // namespace ACE
