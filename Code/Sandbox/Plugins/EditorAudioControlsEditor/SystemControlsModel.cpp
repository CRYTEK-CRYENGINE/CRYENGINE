// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SystemControlsModel.h"

#include "AudioControlsEditorPlugin.h"
#include "MiddlewareDataModel.h"
#include "SystemControlsEditorIcons.h"
#include "ItemStatusHelper.h"

#include <ImplItem.h>
#include <QtUtil.h>

#include <DragDrop.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
bool IsParentValid(CSystemAsset const* pParent, ESystemItemType const type)
{
	switch (pParent->GetType())
	{
	case ESystemItemType::Folder:
	case ESystemItemType::Library: // Intentional fall-through.
		return type != ESystemItemType::State;
	case ESystemItemType::Switch:
		return type == ESystemItemType::State;
	default:
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
void DecodeMimeData(QMimeData const* const pData, std::vector<CSystemAsset*>& outItems)
{
	CDragDropData const* const dragDropData = CDragDropData::FromMimeData(pData);

	if (dragDropData->HasCustomData("AudioLibraryItems"))
	{
		QByteArray const byteArray = dragDropData->GetCustomData("AudioLibraryItems");
		QDataStream stream(byteArray);

		while (!stream.atEnd())
		{
			intptr_t ptr;
			stream >> ptr;
			outItems.emplace_back(reinterpret_cast<CSystemAsset*>(ptr));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CanDropMimeData(QMimeData const* const pData, CSystemAsset const* const pParent)
{
	// Handle first if mime data is an external (from the implementation side) source
	std::vector<CImplItem*> implItems;
	AudioModelUtils::DecodeImplMimeData(pData, implItems);

	if (!implItems.empty())
	{
		IEditorImpl const* const pEditorImpl = CAudioControlsEditorPlugin::GetImplEditor();

		for (CImplItem const* const pImplItem : implItems)
		{
			if (!IsParentValid(pParent, pEditorImpl->ImplTypeToSystemType(pImplItem)))
			{
				return false;
			}
		}

		return true;
	}
	else
	{
		// Handle if mime data is an internal move (rearranging controls)
		std::vector<CSystemAsset*> droppedItems;
		DecodeMimeData(pData, droppedItems);

		for (auto const pItem : droppedItems)
		{
			if (!IsParentValid(pParent, pItem->GetType()))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void DropMimeData(QMimeData const* const pData, CSystemAsset* const pParent)
{
	// Handle first if mime data is an external (from the implementation side) source
	std::vector<CImplItem*> implItems;
	AudioModelUtils::DecodeImplMimeData(pData, implItems);

	if (!implItems.empty())
	{
		CSystemAssetsManager* const pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();

		for (CImplItem* const pImplControl : implItems)
		{
			pAssetsManager->CreateAndConnectImplItems(pImplControl, pParent);
		}
	}
	else
	{
		std::vector<CSystemAsset*> droppedItems;
		DecodeMimeData(pData, droppedItems);
		std::vector<CSystemAsset*> topLevelDroppedItems;
		Utils::SelectTopLevelAncestors(droppedItems, topLevelDroppedItems);
		CAudioControlsEditorPlugin::GetAssetsManager()->MoveItems(pParent, topLevelDroppedItems);
	}
}

//////////////////////////////////////////////////////////////////////////
CAudioLibraryModel::CAudioLibraryModel(CSystemAssetsManager* const pAssetsManager)
	: m_pAssetsManager(pAssetsManager)
{
	ConnectToSystem();
}

//////////////////////////////////////////////////////////////////////////
CAudioLibraryModel::~CAudioLibraryModel()
{
	DisconnectFromSystem();
}

//////////////////////////////////////////////////////////////////////////
int CAudioLibraryModel::rowCount(QModelIndex const& parent) const
{
	if (!parent.isValid())
	{
		return m_pAssetsManager->GetLibraryCount();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CAudioLibraryModel::columnCount(QModelIndex const& parent) const
{
	return 1;
}

//////////////////////////////////////////////////////////////////////////
QVariant CAudioLibraryModel::data(QModelIndex const& index, int role) const
{
	CSystemLibrary const* const pLibrary = static_cast<CSystemLibrary*>(index.internalPointer());

	if (pLibrary != nullptr)
	{
		switch (role)
		{
		case Qt::DisplayRole:

			if (pLibrary->IsModified())
			{
				return QtUtil::ToQStringSafe(pLibrary->GetName()) + " *";
			}

			return QtUtil::ToQStringSafe(pLibrary->GetName());
			break;

		case Qt::EditRole:
			return QtUtil::ToQStringSafe(pLibrary->GetName());
			break;

		case Qt::ForegroundRole:

			if (pLibrary->HasPlaceholderConnection())
			{
				return GetItemStatusColor(EItemStatus::Placeholder);
			}
			else if (!pLibrary->HasConnection())
			{
				return GetItemStatusColor(EItemStatus::NoConnection);
			}
			else if (!pLibrary->HasControl())
			{
				return GetItemStatusColor(EItemStatus::NoControl);
			}
			
			break;

		case Qt::ToolTipRole:

			if (pLibrary->HasPlaceholderConnection())
			{
				return tr("Contains item that has an invalid connection");
			}
			else if (!pLibrary->HasConnection())
			{
				return tr("Contains item that is not connected to any audio control");
			}
			else if (!pLibrary->HasControl())
			{
				return tr("Contains no audio control");
			}
			else if (!pLibrary->GetDescription().IsEmpty())
			{
				return tr(pLibrary->GetDescription());
			}

			break;

		case Qt::DecorationRole:
			return GetItemTypeIcon(ESystemItemType::Library);
			break;

		case static_cast<int>(EDataRole::ItemType):
			return static_cast<int>(ESystemItemType::Library);
			break;

		case static_cast<int>(EDataRole::InternalPointer):
			return reinterpret_cast<intptr_t>(pLibrary);
			break;
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
bool CAudioLibraryModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	if (index.isValid())
	{
		CSystemAsset* const pItem = static_cast<CSystemAsset*>(index.internalPointer());

		if (pItem != nullptr)
		{
			switch (role)
			{
			case Qt::EditRole:
				{
					if (value.canConvert<QString>())
					{
						string const& oldName = pItem->GetName();
						string const& newName = QtUtil::ToString(value.toString());

						if (!newName.empty() && newName.compareNoCase(oldName) != 0)
						{
							pItem->SetName(Utils::GenerateUniqueLibraryName(newName, *m_pAssetsManager));
						}

						return true;
					}
				}
			default:
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, R"([Audio Controls Editor] The role '%d' is not handled!)", role);
				break;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
QVariant CAudioLibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		return "Name";
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CAudioLibraryModel::flags(QModelIndex const& index) const
{
	if (index.isValid())
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
	}

	return Qt::NoItemFlags;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CAudioLibraryModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	if ((row >= 0) && (column >= 0))
	{
		return createIndex(row, column, reinterpret_cast<quintptr>(m_pAssetsManager->GetLibrary(row)));
	}

	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CAudioLibraryModel::parent(QModelIndex const& index) const
{
	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CAudioLibraryModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	if (parent.isValid())
	{
		CSystemLibrary const* const pParent = static_cast<CSystemLibrary*>(parent.internalPointer());
		CRY_ASSERT(pParent != nullptr);
		return CanDropMimeData(pData, pParent);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CAudioLibraryModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	if (parent.isValid())
	{
		CSystemLibrary* const pLibrary = static_cast<CSystemLibrary*>(parent.internalPointer());
		CRY_ASSERT(pLibrary != nullptr);
		DropMimeData(pData, pLibrary);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CAudioLibraryModel::supportedDropActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

//////////////////////////////////////////////////////////////////////////
QStringList CAudioLibraryModel::mimeTypes() const
{
	QStringList result;
	result << CDragDropData::GetMimeFormatForType("AudioLibraryItems");
	result << CMiddlewareDataModel::ms_szMimeType;
	return result;
}

//////////////////////////////////////////////////////////////////////////
void CAudioLibraryModel::ConnectToSystem()
{
	m_pAssetsManager->signalLibraryAboutToBeAdded.Connect([&]()
	{
		int const row = m_pAssetsManager->GetLibraryCount();
		beginInsertRows(QModelIndex(), row, row);
	}, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalLibraryAdded.Connect([&](CSystemLibrary* const pLibrary) { endInsertRows(); }, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalLibraryAboutToBeRemoved.Connect([&](CSystemLibrary* const pLibrary)
	{
		int const libCount = m_pAssetsManager->GetLibraryCount();

		for (int i = 0; i < libCount; ++i)
		{
			if (m_pAssetsManager->GetLibrary(i) == pLibrary)
			{
				beginRemoveRows(QModelIndex(), i, i);
				break;
			}
		}
	}, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalLibraryRemoved.Connect([&]() { endRemoveRows(); }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CAudioLibraryModel::DisconnectFromSystem()
{
	m_pAssetsManager->signalLibraryAboutToBeAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalLibraryAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalLibraryAboutToBeRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalLibraryRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

/////////////////////////////////////////////////////////////////////////////////////////
CSystemControlsModel::CSystemControlsModel(CSystemAssetsManager* const pAssetsManager, CSystemLibrary* const pLibrary)
	: m_pAssetsManager(pAssetsManager)
	, m_pLibrary(pLibrary)
{
	ConnectToSystem();
}

//////////////////////////////////////////////////////////////////////////
CSystemControlsModel::~CSystemControlsModel()
{
	DisconnectFromSystem();
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CSystemControlsModel::IndexFromItem(CSystemAsset const* const pItem) const
{
	if (pItem != nullptr)
	{
		CSystemAsset const* const pParent = pItem->GetParent();

		if (pParent != nullptr)
		{
			int const size = pParent->ChildCount();

			for (int i = 0; i < size; ++i)
			{
				if (pParent->GetChild(i) == pItem)
				{
					return createIndex(i, 0, reinterpret_cast<quintptr>(pItem));
				}
			}
		}
	}

	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
int CSystemControlsModel::rowCount(QModelIndex const& parent) const
{
	if (!parent.isValid())
	{
		return m_pLibrary->ChildCount();
	}

	CSystemAsset const* const pAsset = static_cast<CSystemAsset*>(parent.internalPointer());

	if (pAsset != nullptr)
	{
		return pAsset->ChildCount();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CSystemControlsModel::columnCount(QModelIndex const& parent) const
{
	return 1;
}

//////////////////////////////////////////////////////////////////////////
QVariant CSystemControlsModel::data(QModelIndex const& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	CSystemAsset* const pItem = static_cast<CSystemAsset*>(index.internalPointer());

	if (pItem != nullptr)
	{
		ESystemItemType const itemType = pItem->GetType();

		switch (role)
		{
		case Qt::DisplayRole:

			if (pItem->IsModified())
			{
				return QtUtil::ToQStringSafe(pItem->GetName()) + " *";
			}

			return QtUtil::ToQStringSafe(pItem->GetName());
			break;

		case Qt::EditRole:
			return QtUtil::ToQStringSafe(pItem->GetName());
			break;

		case Qt::ForegroundRole:

			if (pItem->HasPlaceholderConnection())
			{
				return GetItemStatusColor(EItemStatus::Placeholder);
			}
			else if (!pItem->HasConnection())
			{
				return GetItemStatusColor(EItemStatus::NoConnection);
			}
			else if (((itemType == ESystemItemType::Folder) || (itemType == ESystemItemType::Switch)) && !pItem->HasControl())
			{
				return GetItemStatusColor(EItemStatus::NoControl);
			}

			break;

		case Qt::ToolTipRole:

			if (pItem->HasPlaceholderConnection())
			{
				if ((itemType == ESystemItemType::Folder) || (itemType == ESystemItemType::Switch))
				{
					return tr("Contains item that has an invalid connection");
				}
				else
				{
					return tr("Item has an invalid connection");
				}
			}
			else if (!pItem->HasConnection())
			{
				if ((itemType == ESystemItemType::Folder) || (itemType == ESystemItemType::Switch))
				{
					return tr("Contains item that is not connected to any audio control");
				}
				else
				{
					return tr("Item is not connected to any audio control");
				}
			}
			else if ((itemType == ESystemItemType::Folder) && !pItem->HasControl())
			{
				return tr("Contains no audio control");
			}
			else if ((itemType == ESystemItemType::Switch) && !pItem->HasControl())
			{
				return tr("Contains no state");
			}
			else if (!pItem->GetDescription().IsEmpty())
			{
				return tr(pItem->GetDescription());
			}

			break;

		case Qt::DecorationRole:
			return GetItemTypeIcon(itemType);
			break;

		case static_cast<int>(EDataRole::ItemType):
			return static_cast<int>(itemType);
			break;

		case static_cast<int>(EDataRole::InternalPointer):
			return reinterpret_cast<intptr_t>(pItem);
			break;
		case static_cast<int>(EDataRole::Id):
			if (itemType != ESystemItemType::Folder)
			{
				CSystemControl const* const pControl = static_cast<CSystemControl*>(pItem);

				if (pControl != nullptr)
				{
					return pControl->GetId();
				}
			}
			return 0;
			break;
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	if (index.isValid())
	{
		CSystemAsset* const pItem = static_cast<CSystemAsset*>(index.internalPointer());

		if (pItem != nullptr)
		{
			switch (role)
			{
			case Qt::EditRole:
				{
					if (value.canConvert<QString>())
					{
						string const& oldName = pItem->GetName();
						string const& newName = QtUtil::ToString(value.toString());

						if (!newName.empty() && newName.compareNoCase(oldName) != 0)
						{
							ESystemItemType const itemType = pItem->GetType();

							switch (itemType)
							{
							case ESystemItemType::Preload:
							case ESystemItemType::Parameter:
							case ESystemItemType::State:
							case ESystemItemType::Switch:
							case ESystemItemType::Trigger:
							case ESystemItemType::Environment:
								pItem->SetName(Utils::GenerateUniqueControlName(newName, itemType, *m_pAssetsManager));
								break;
							case ESystemItemType::Folder:
								pItem->SetName(Utils::GenerateUniqueName(newName, itemType, pItem->GetParent()));
								break;
							default:
								CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, R"([Audio Controls Editor] The item type '%d' is not handled!)", itemType);
								break;
							}
						}

						return true;
					}
				}
			default:
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, R"([Audio Controls Editor] The role '%d' is not handled!)", role);
				break;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
QVariant CSystemControlsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		return "Name";
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CSystemControlsModel::flags(QModelIndex const& index) const
{
	if (index.isValid())
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
	}

	return Qt::NoItemFlags;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CSystemControlsModel::index(int row, int column, QModelIndex const& parent) const
{
	if ((row >= 0) && (column >= 0))
	{
		if (parent.isValid())
		{
			CSystemAsset const* const pParent = static_cast<CSystemAsset*>(parent.internalPointer());

			if ((pParent != nullptr) && (row < pParent->ChildCount()))
			{
				CSystemAsset const* const pChild = pParent->GetChild(row);

				if (pChild != nullptr)
				{
					return createIndex(row, column, reinterpret_cast<quintptr>(pChild));
				}
			}
		}
		else if (row < m_pLibrary->ChildCount())
		{
			return createIndex(row, column, reinterpret_cast<quintptr>(m_pLibrary->GetChild(row)));
		}
	}

	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CSystemControlsModel::parent(QModelIndex const& index) const
{
	if (index.isValid())
	{
		CSystemAsset const* const pItem = static_cast<CSystemAsset*>(index.internalPointer());

		if (pItem != nullptr)
		{
			CSystemAsset const* const pParent = pItem->GetParent();

			if ((pParent != nullptr) && (pParent->GetType() != ESystemItemType::Library))
			{
				return IndexFromItem(pParent);
			}
		}
	}

	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	if (parent.isValid())
	{
		CSystemAsset const* const pParent = static_cast<CSystemAsset*>(parent.internalPointer());
		CRY_ASSERT(pParent != nullptr);
		return CanDropMimeData(pData, pParent);
	}
	else
	{
		CRY_ASSERT(m_pLibrary != nullptr);
		return CanDropMimeData(pData, m_pLibrary);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	if (parent.isValid())
	{
		CSystemAsset* const pAsset = static_cast<CSystemAsset*>(parent.internalPointer());
		CRY_ASSERT(pAsset != nullptr);
		DropMimeData(pData, pAsset);
		return true;
	}
	else
	{
		CRY_ASSERT(m_pLibrary != nullptr);
		DropMimeData(pData, m_pLibrary);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
QMimeData* CSystemControlsModel::mimeData(QModelIndexList const& indexes) const
{
	CDragDropData* const pDragDropData = new CDragDropData();
	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);

	for (auto const& index : indexes)
	{
		stream << reinterpret_cast<intptr_t>(index.internalPointer());
	}

	pDragDropData->SetCustomData("AudioLibraryItems", byteArray);
	return pDragDropData;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CSystemControlsModel::supportedDropActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

//////////////////////////////////////////////////////////////////////////
QStringList CSystemControlsModel::mimeTypes() const
{
	QStringList result;
	result << CDragDropData::GetMimeFormatForType("AudioLibraryItems");
	result << CMiddlewareDataModel::ms_szMimeType;
	return result;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsModel::ConnectToSystem()
{
	m_pAssetsManager->signalItemAboutToBeAdded.Connect([&](CSystemAsset* const pParent)
	{
		if (Utils::GetParentLibrary(pParent) == m_pLibrary)
		{
			int const row = pParent->ChildCount();

			if (pParent->GetType() == ESystemItemType::Library)
			{
				CRY_ASSERT(pParent == m_pLibrary);
				beginInsertRows(QModelIndex(), row, row);
			}
			else
			{
				QModelIndex const& parent = IndexFromItem(pParent);
				beginInsertRows(parent, row, row);
			}
		}
	}, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalItemAdded.Connect([&](CSystemAsset* const pAsset)
	{
		if (Utils::GetParentLibrary(pAsset) == m_pLibrary)
		{
			endInsertRows();
		}
	}, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalItemAboutToBeRemoved.Connect([&](CSystemAsset* const pAsset)
	{
		if (Utils::GetParentLibrary(pAsset) == m_pLibrary)
		{
			CSystemAsset const* const pParent = pAsset->GetParent();

			if (pParent != nullptr)
			{
				int const childCount = pParent->ChildCount();

				for (int i = 0; i < childCount; ++i)
				{
					if (pParent->GetChild(i) == pAsset)
					{
						if (pParent->GetType() == ESystemItemType::Library)
						{
							CRY_ASSERT(pParent == m_pLibrary);
							beginRemoveRows(QModelIndex(), i, i);
						}
						else
						{
							QModelIndex const& parent = IndexFromItem(pParent);
							beginRemoveRows(parent, i, i);
						}

						break;
					}
				}
			}
		}
	}, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalItemRemoved.Connect([&](CSystemAsset* const pParent, CSystemAsset* const pAsset)
	{
		if (Utils::GetParentLibrary(pParent) == m_pLibrary)
		{
			endRemoveRows();
		}
	}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsModel::DisconnectFromSystem()
{
	m_pAssetsManager->signalItemAboutToBeAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemAboutToBeRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CSystemControlsFilterProxyModel::CSystemControlsFilterProxyModel(QObject* parent)
	: QDeepFilterProxyModel(QDeepFilterProxyModel::BehaviorFlags(QDeepFilterProxyModel::AcceptIfChildMatches), parent)
{
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsFilterProxyModel::rowMatchesFilter(int source_row, QModelIndex const& source_parent) const
{
	if (QDeepFilterProxyModel::rowMatchesFilter(source_row, source_parent))
	{
		QModelIndex const index = sourceModel()->index(source_row, 0, source_parent);

		if (index.isValid())
		{
			CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(index);

			if ((pAsset != nullptr) && pAsset->IsHiddenDefault())
			{
				// Hide default controls that should not be connected to any middleware control.
				return false;
			}

			if (m_validControlsMask != std::numeric_limits<uint>::max())
			{
				// Has type filtering.
				ESystemItemType const itemType = (ESystemItemType)sourceModel()->data(index, static_cast<int>(EDataRole::ItemType)).toUInt();

				if (itemType < ESystemItemType::Folder)
				{
					return m_validControlsMask & (1 << static_cast<int>(itemType));
				}
				else
				{
					return false;
				}
			}

			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsFilterProxyModel::lessThan(QModelIndex const& left, QModelIndex const& right) const
{
	if (left.column() == right.column())
	{
		ESystemItemType const leftType = static_cast<ESystemItemType>(sourceModel()->data(left, static_cast<int>(EDataRole::ItemType)).toInt());
		ESystemItemType const rightType = static_cast<ESystemItemType>(sourceModel()->data(right, static_cast<int>(EDataRole::ItemType)).toInt());

		if (leftType != rightType)
		{
			return rightType < ESystemItemType::Folder;
		}

		QVariant const& valueLeft = sourceModel()->data(left, Qt::DisplayRole);
		QVariant const& valueRight = sourceModel()->data(right, Qt::DisplayRole);

		return valueLeft < valueRight;
	}

	return QSortFilterProxyModel::lessThan(left, right);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsFilterProxyModel::EnableControl(bool const isEnabled, ESystemItemType const type)
{
	if (isEnabled)
	{
		m_validControlsMask |= (1 << (int)type);
	}
	else
	{
		m_validControlsMask &= ~(1 << (int)type);
	}

	invalidate();
}

namespace AudioModelUtils
{
//////////////////////////////////////////////////////////////////////////
void GetAssetsFromIndexesSeparated(QModelIndexList const& list, std::vector<CSystemLibrary*>& outLibraries, std::vector<CSystemFolder*>& outFolders, std::vector<CSystemControl*>& outControls)
{
	for (auto const& index : list)
	{
		QVariant const internalPtr = index.data(static_cast<int>(EDataRole::InternalPointer));

		if (internalPtr.isValid())
		{
			QVariant const type = index.data(static_cast<int>(EDataRole::ItemType));

			switch (type.toInt())
			{
			case static_cast<int>(ESystemItemType::Library):
				outLibraries.emplace_back(reinterpret_cast<CSystemLibrary*>(internalPtr.value<intptr_t>()));
				break;
			case static_cast<int>(ESystemItemType::Folder):
				outFolders.emplace_back(reinterpret_cast<CSystemFolder*>(internalPtr.value<intptr_t>()));
				break;
			default:
				outControls.emplace_back(reinterpret_cast<CSystemControl*>(internalPtr.value<intptr_t>()));
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void GetAssetsFromIndexesCombined(QModelIndexList const& list, std::vector<CSystemAsset*>& outAssets)
{
	for (auto const& index : list)
	{
		QVariant const internalPtr = index.data(static_cast<int>(EDataRole::InternalPointer));

		if (internalPtr.isValid())
		{
			outAssets.emplace_back(reinterpret_cast<CSystemAsset*>(internalPtr.value<intptr_t>()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CSystemAsset* GetAssetFromIndex(QModelIndex const& index)
{
	QVariant const internalPtr = index.data(static_cast<int>(EDataRole::InternalPointer));

	if (internalPtr.isValid())
	{
		return reinterpret_cast<CSystemAsset*>(internalPtr.value<intptr_t>());
	}

	return nullptr;
}
} // namespace AudioModelUtils
} // namespace ACE
