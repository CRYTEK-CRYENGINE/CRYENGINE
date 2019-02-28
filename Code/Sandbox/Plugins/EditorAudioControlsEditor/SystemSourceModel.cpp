// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SystemSourceModel.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "AssetIcons.h"
#include "AssetUtils.h"

#include <ModelUtils.h>
#include <IItem.h>
#include <QtUtil.h>
#include <DragDrop.h>

#include <QApplication>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
QStringList GetScopeNames()
{
	QStringList scopeNames;
	ScopeInfos scopeInfos;
	g_assetsManager.GetScopeInfos(scopeInfos);

	for (auto const& scopeInfo : scopeInfos)
	{
		scopeNames.append(QString(scopeInfo.name));
	}

	scopeNames.sort(Qt::CaseInsensitive);

	return scopeNames;
}

static QStringList const s_typeFilterList {
	"Trigger", "Parameter", "Switch", "State", "Environment", "Preload"
};

static CItemModelAttributeEnum s_typeAttribute("Type", s_typeFilterList, CItemModelAttribute::AlwaysHidden, true);
static CItemModelAttributeEnumFunc s_scopeAttribute("Scope", &GetScopeNames, CItemModelAttribute::StartHidden, true);

//////////////////////////////////////////////////////////////////////////
bool IsParentValid(EAssetType const parentType, EAssetType const assetType)
{
	bool isValid = false;

	switch (parentType)
	{
	case EAssetType::Folder:
	case EAssetType::Library: // Intentional fall-through.
		isValid = (assetType != EAssetType::State);
		break;
	case EAssetType::Switch:
		isValid = (assetType == EAssetType::State) || (assetType == EAssetType::Parameter);
		break;
	default:
		isValid = false;
		break;
	}

	return isValid;
}

//////////////////////////////////////////////////////////////////////////
bool ProcessDragDropData(QMimeData const* const pData, Assets& outItems)
{
	CRY_ASSERT_MESSAGE(outItems.empty(), "Passed container must be empty.");
	CDragDropData const* const pDragDropData = CDragDropData::FromMimeData(pData);

	if (pDragDropData->HasCustomData(ModelUtils::s_szSystemMimeType))
	{
		QByteArray const byteArray = pDragDropData->GetCustomData(ModelUtils::s_szSystemMimeType);
		QDataStream stream(byteArray);

		while (!stream.atEnd())
		{
			intptr_t ptr;
			stream >> ptr;
			outItems.push_back(reinterpret_cast<CAsset*>(ptr));
		}
	}

	return !outItems.empty();
}

//////////////////////////////////////////////////////////////////////////
bool ProcessImplDragDropData(QMimeData const* const pData, std::vector<Impl::IItem*>& outItems)
{
	CRY_ASSERT_MESSAGE(outItems.empty(), "Passed container must be empty.");
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
				Impl::IItem* const pIItem = g_pIImpl->GetItem(id);

				if (pIItem != nullptr)
				{
					outItems.push_back(pIItem);
				}
			}
		}
	}

	return !outItems.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CSystemSourceModel::CanDropData(QMimeData const* const pData, CAsset const& parent)
{
	bool canDrop = false;
	bool hasValidParent = true;

	// Handle first if mime data is an external (from the implementation side) source.
	std::vector<Impl::IItem*> implItems;

	if (ProcessImplDragDropData(pData, implItems))
	{
		for (Impl::IItem const* const pIItem : implItems)
		{
			if (!IsParentValid(parent.GetType(), g_pIImpl->ImplTypeToAssetType(pIItem)))
			{
				hasValidParent = false;
				break;
			}
		}

		canDrop = hasValidParent;
	}
	else
	{
		// Handle if mime data is an internal move (rearranging controls).
		Assets droppedAssets;

		if (ProcessDragDropData(pData, droppedAssets))
		{
			for (auto const pAsset : droppedAssets)
			{
				if (!IsParentValid(parent.GetType(), pAsset->GetType()))
				{
					hasValidParent = false;
					break;
				}
				else
				{
					// Don't drop item on its current parent.
					CAsset const* const pParent = pAsset->GetParent();

					if ((pParent != nullptr) && (pParent == &parent))
					{
						hasValidParent = false;
						break;
					}
				}
			}

			canDrop = hasValidParent;
		}
	}

	return canDrop;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemSourceModel::DropData(QMimeData const* const pData, CAsset* const pParent)
{
	bool wasDropped = false;

	if (CanDropData(pData, *pParent))
	{
		// Handle first if mime data is an external (from the implementation side) source
		std::vector<Impl::IItem*> implItems;

		if (ProcessImplDragDropData(pData, implItems))
		{
			for (auto const pIItem : implItems)
			{
				g_assetsManager.CreateAndConnectImplItems(pIItem, pParent);
			}

			wasDropped = true;
		}
		else
		{
			Assets droppedAssets;

			if (ProcessDragDropData(pData, droppedAssets))
			{
				Assets topLevelDroppedAssets;
				AssetUtils::SelectTopLevelAncestors(droppedAssets, topLevelDroppedAssets);
				g_assetsManager.MoveAssets(pParent, topLevelDroppedAssets);
				wasDropped = true;
			}
		}
	}

	return wasDropped;
}

//////////////////////////////////////////////////////////////////////////
CSystemSourceModel::CSystemSourceModel(QObject* const pParent)
	: QAbstractItemModel(pParent)
	, m_ignoreLibraryUpdates(false)
	, m_nameColumn(static_cast<int>(EColumns::Name))
{
	ConnectSignals();
}

//////////////////////////////////////////////////////////////////////////
CSystemSourceModel::~CSystemSourceModel()
{
	DisconnectSignals();
}

//////////////////////////////////////////////////////////////////////////
void CSystemSourceModel::ConnectSignals()
{
	CAudioControlsEditorPlugin::SignalAboutToLoad.Connect([&]()
		{
			beginResetModel();
			m_ignoreLibraryUpdates = true;
			endResetModel();
	  }, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::SignalLoaded.Connect([&]()
		{
			beginResetModel();
			m_ignoreLibraryUpdates = false;
			endResetModel();
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalLibraryAboutToBeAdded.Connect([this]()
		{
			if (!m_ignoreLibraryUpdates)
			{
			  int const row = static_cast<int>(g_assetsManager.GetLibraryCount());
			  beginInsertRows(QModelIndex(), row, row);
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalLibraryAdded.Connect([this](CLibrary* const pLibrary)
		{
			if (!m_ignoreLibraryUpdates)
			{
			  endInsertRows();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalLibraryAboutToBeRemoved.Connect([this](CLibrary* const pLibrary)
		{
			if (!m_ignoreLibraryUpdates)
			{
			  size_t const libCount = g_assetsManager.GetLibraryCount();

			  for (size_t i = 0; i < libCount; ++i)
			  {
			    if (g_assetsManager.GetLibrary(i) == pLibrary)
			    {
			      int const index = static_cast<int>(i);
			      beginRemoveRows(QModelIndex(), index, index);
			      break;
			    }
			  }
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalLibraryRemoved.Connect([this]()
		{
			if (!m_ignoreLibraryUpdates)
			{
			  endRemoveRows();
			}
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CSystemSourceModel::DisconnectSignals()
{
	CAudioControlsEditorPlugin::SignalAboutToLoad.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::SignalLoaded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalLibraryAboutToBeAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalLibraryAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalLibraryAboutToBeRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalLibraryRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CItemModelAttribute* CSystemSourceModel::GetAttributeForColumn(EColumns const column)
{
	CItemModelAttribute* pAttribute = nullptr;

	switch (column)
	{
	case EColumns::Notification:
		pAttribute = &ModelUtils::s_notificationAttribute;
		break;
	case EColumns::Type:
		pAttribute = &s_typeAttribute;
		break;
	case EColumns::Placeholder:
		pAttribute = &ModelUtils::s_placeholderAttribute;
		break;
	case EColumns::NoConnection:
		pAttribute = &ModelUtils::s_connectedAttribute;
		break;
	case EColumns::NoControl:
		pAttribute = &ModelUtils::s_noControlAttribute;
		break;
	case EColumns::PakStatus:
		pAttribute = &ModelUtils::s_pakStatus;
		break;
	case EColumns::InPak:
		pAttribute = &ModelUtils::s_inPakAttribute;
		break;
	case EColumns::OnDisk:
		pAttribute = &ModelUtils::s_onDiskAttribute;
		break;
	case EColumns::Scope:
		pAttribute = &s_scopeAttribute;
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
QVariant CSystemSourceModel::GetHeaderData(int const section, Qt::Orientation const orientation, int const role)
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
CAsset* CSystemSourceModel::GetAssetFromIndex(QModelIndex const& index, int const column)
{
	CAsset* pAsset = nullptr;
	QModelIndex const& nameColumnIndex = index.sibling(index.row(), column);
	QVariant const internalPtr = nameColumnIndex.data(static_cast<int>(ModelUtils::ERoles::InternalPointer));

	if (internalPtr.isValid())
	{
		pAsset = reinterpret_cast<CAsset*>(internalPtr.value<intptr_t>());
	}

	return pAsset;
}

//////////////////////////////////////////////////////////////////////////
QMimeData* CSystemSourceModel::GetDragDropData(QModelIndexList const& indexes)
{
	auto const pDragDropData = new CDragDropData();
	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);

	QModelIndexList nameIndexes;
	auto const nameColumn = static_cast<int>(EColumns::Name);

	for (auto const& index : indexes)
	{
		nameIndexes.append(index.sibling(index.row(), nameColumn));
	}

	nameIndexes.erase(std::unique(nameIndexes.begin(), nameIndexes.end()), nameIndexes.end());

	for (auto const& index : nameIndexes)
	{
		stream << reinterpret_cast<intptr_t>(index.internalPointer());
	}

	pDragDropData->SetCustomData(ModelUtils::s_szSystemMimeType, byteArray);
	return pDragDropData;
}

//////////////////////////////////////////////////////////////////////////
int CSystemSourceModel::rowCount(QModelIndex const& parent) const
{
	int rowCount = 0;

	if (!m_ignoreLibraryUpdates && !parent.isValid())
	{
		rowCount = static_cast<int>(g_assetsManager.GetLibraryCount());
	}

	return rowCount;
}

//////////////////////////////////////////////////////////////////////////
int CSystemSourceModel::columnCount(QModelIndex const& parent) const
{
	return static_cast<int>(EColumns::Count);
}

//////////////////////////////////////////////////////////////////////////
QVariant CSystemSourceModel::data(QModelIndex const& index, int role) const
{
	QVariant variant;
	CLibrary const* const pLibrary = static_cast<CLibrary*>(index.internalPointer());

	if (pLibrary != nullptr)
	{
		if (role == static_cast<int>(ModelUtils::ERoles::IsDefaultControl))
		{
			variant = (pLibrary->GetFlags() & EAssetFlags::IsDefaultControl) != 0;
		}
		else if (role == static_cast<int>(ModelUtils::ERoles::Name))
		{
			variant = QtUtil::ToQString(pLibrary->GetName());
		}
		else
		{
			EAssetFlags const flags = pLibrary->GetFlags();

			switch (index.column())
			{
			case static_cast<int>(EColumns::Notification):
				{
					switch (role)
					{
					case Qt::DecorationRole:
						if ((flags& EAssetFlags::HasPlaceholderConnection) != 0)
						{
							variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::Placeholder);
						}
						else if ((flags& EAssetFlags::HasConnection) == 0)
						{
							variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NoConnection);
						}
						else if ((flags& EAssetFlags::HasControl) == 0)
						{
							variant = ModelUtils::GetItemNotificationIcon(ModelUtils::EItemStatus::NoControl);
						}
						break;
					case Qt::ToolTipRole:
						if ((flags& EAssetFlags::HasPlaceholderConnection) != 0)
						{
							variant = tr("Contains item whose connection was not found in middleware project");
						}
						else if ((flags& EAssetFlags::HasConnection) == 0)
						{
							variant = tr("Contains item that is not connected to any middleware control");
						}
						else if ((flags& EAssetFlags::HasControl) == 0)
						{
							variant = tr("Contains no audio control");
						}
						break;
					}
				}
				break;
			case static_cast<int>(EColumns::Placeholder):
				{
					if (role == Qt::CheckStateRole)
					{
						variant = ((flags& EAssetFlags::HasPlaceholderConnection) == 0) ? Qt::Checked : Qt::Unchecked;
					}
				}
				break;
			case static_cast<int>(EColumns::NoConnection):
				{
					if (role == Qt::CheckStateRole)
					{
						variant = ((flags& EAssetFlags::HasConnection) != 0) ? Qt::Checked : Qt::Unchecked;
					}
				}
				break;
			case static_cast<int>(EColumns::NoControl):
				{
					if (role == Qt::CheckStateRole)
					{
						variant = ((flags& EAssetFlags::HasControl) == 0) ? Qt::Checked : Qt::Unchecked;
					}
				}
				break;
			case static_cast<int>(EColumns::PakStatus):
				{
					switch (role)
					{
					case Qt::DecorationRole:
						variant = ModelUtils::GetPakStatusIcon(pLibrary->GetPakStatus());
						break;
					case Qt::ToolTipRole:
						{
							EPakStatus const pakStatus = pLibrary->GetPakStatus();

							if (pakStatus == (EPakStatus::InPak | EPakStatus::OnDisk))
							{
								variant = tr("Library is in pak and on disk");
							}
							else if ((pakStatus& EPakStatus::InPak) != 0)
							{
								variant = tr("Library is only in pak file");
							}
							else if ((pakStatus& EPakStatus::OnDisk) != 0)
							{
								variant = tr("Library is only on disk");
							}
							else
							{
								variant = tr("Library does not exist on disk. Save to write file.");
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
						variant = ((pLibrary->GetPakStatus() & EPakStatus::InPak) != 0) ? Qt::Checked : Qt::Unchecked;
					}
				}
				break;
			case static_cast<int>(EColumns::OnDisk):
				{
					if (role == Qt::CheckStateRole)
					{
						variant = ((pLibrary->GetPakStatus() & EPakStatus::OnDisk) != 0) ? Qt::Checked : Qt::Unchecked;
					}
				}
				break;
			case static_cast<int>(EColumns::Name):
				{
					switch (role)
					{
					case Qt::DecorationRole:
						variant = GetAssetIcon(EAssetType::Library);
						break;
					case Qt::DisplayRole:

						if ((pLibrary->GetFlags() & EAssetFlags::IsModified) != 0)
						{
							variant = QtUtil::ToQString(pLibrary->GetName() + " *");
						}
						else
						{
							variant = QtUtil::ToQString(pLibrary->GetName());
						}
						break;
					case Qt::EditRole:
						variant = QtUtil::ToQString(pLibrary->GetName());
						break;
					case Qt::ToolTipRole:
						{
							if (!pLibrary->GetDescription().IsEmpty())
							{
								variant = QtUtil::ToQString(pLibrary->GetName() + ": " + pLibrary->GetDescription());
							}
							else
							{
								variant = QtUtil::ToQString(pLibrary->GetName());
							}
						}
						break;
					case static_cast<int>(ModelUtils::ERoles::SortPriority):
						variant = static_cast<int>(EAssetType::Library);
						break;
					case static_cast<int>(ModelUtils::ERoles::Name):
						variant = QtUtil::ToQString(pLibrary->GetName());
						break;
					case static_cast<int>(ModelUtils::ERoles::InternalPointer):
						variant = reinterpret_cast<intptr_t>(pLibrary);
						break;
					}
				}
				break;
			}
		}
	}

	return variant;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemSourceModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	bool wasDataChanged = false;

	if (index.isValid() && (index.column() == m_nameColumn))
	{
		auto const pAsset = static_cast<CAsset*>(index.internalPointer());

		if (pAsset != nullptr)
		{
			switch (role)
			{
			case Qt::EditRole:
				{
					if (value.canConvert<QString>())
					{
						string const& oldName = pAsset->GetName();
						string const& newName = QtUtil::ToString(value.toString());

						if (!newName.empty() && newName.compareNoCase(oldName) != 0)
						{
							pAsset->SetName(AssetUtils::GenerateUniqueLibraryName(newName));
						}

						wasDataChanged = true;
					}
				}
				break;
			default:
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, R"([Audio Controls Editor] The role '%d' is not handled!)", role);
				break;
			}
		}
	}

	return wasDataChanged;
}

//////////////////////////////////////////////////////////////////////////
QVariant CSystemSourceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	return GetHeaderData(section, orientation, role);
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CSystemSourceModel::flags(QModelIndex const& index) const
{
	Qt::ItemFlags flags = Qt::NoItemFlags;

	if (index.isValid())
	{
		if ((index.column() == m_nameColumn) && !(index.data(static_cast<int>(ModelUtils::ERoles::IsDefaultControl)).toBool()))
		{
			flags = QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
		}
		else
		{
			flags = QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled;
		}
	}

	return flags;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CSystemSourceModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	QModelIndex modelIndex = QModelIndex();

	if ((row >= 0) && (column >= 0))
	{
		modelIndex = createIndex(row, column, reinterpret_cast<quintptr>(g_assetsManager.GetLibrary(static_cast<size_t>(row))));
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CSystemSourceModel::parent(QModelIndex const& index) const
{
	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CSystemSourceModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	bool canDrop = false;
	QString dragText = tr("Invalid operation");

	if (parent.isValid())
	{
		CLibrary const* const pParent = static_cast<CLibrary*>(parent.internalPointer());
		CRY_ASSERT_MESSAGE(pParent != nullptr, "Parent is null pointer.");

		if (pParent != nullptr)
		{
			canDrop = ((pParent->GetFlags() & EAssetFlags::IsDefaultControl) == 0) && CanDropData(pData, *pParent);

			if (canDrop)
			{
				dragText = tr("Add to ") + QtUtil::ToQString(pParent->GetName());
			}
			else if ((pParent->GetFlags() & EAssetFlags::IsDefaultControl) != 0)
			{
				dragText = tr("Cannot add items to the default controls library.");
			}
		}
	}

	CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), dragText);
	return canDrop;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemSourceModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	bool wasDropped = false;

	if (parent.isValid())
	{
		auto const pLibrary = static_cast<CLibrary*>(parent.internalPointer());
		CRY_ASSERT_MESSAGE(pLibrary != nullptr, "Library is null pointer.");

		if (pLibrary != nullptr)
		{
			wasDropped = DropData(pData, pLibrary);
		}
	}

	return wasDropped;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CSystemSourceModel::supportedDropActions() const
{
	return static_cast<Qt::DropActions>(Qt::MoveAction | Qt::CopyAction);
}

//////////////////////////////////////////////////////////////////////////
QStringList CSystemSourceModel::mimeTypes() const
{
	QStringList types;
	types << CDragDropData::GetMimeFormatForType(ModelUtils::s_szSystemMimeType);
	types << CDragDropData::GetMimeFormatForType(ModelUtils::s_szImplMimeType);
	return types;
}
} // namespace ACE
