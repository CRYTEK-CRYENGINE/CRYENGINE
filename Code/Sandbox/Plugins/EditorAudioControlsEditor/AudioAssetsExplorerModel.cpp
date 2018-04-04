// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IAudioSystemItem.h"
#include "AudioControlsEditorPlugin.h"
#include "AudioSystemModel.h"
#include "QAudioControlEditorIcons.h"

#include <QtUtil.h>

#include "AudioAssets.h"
#include "AudioControlsEditorPlugin.h"
#include "AudioControlsEditorUndo.h"
#include "AudioSystemModel.h"
#include "IAudioSystemEditor.h"
#include "IAudioSystemItem.h"
#include "IEditor.h"
#include "AudioAssetsExplorerModel.h"
#include "QAudioControlTreeWidget.h"

#include "Controls/QuestionDialog.h"
#include <CryString/CryPath.h>
#include <DragDrop.h>

namespace ACE
{
bool IsParentValid(const IAudioAsset& parent, const EItemType type)
{
	switch (parent.GetType())
	{
	case EItemType::eItemType_Folder:
	case EItemType::eItemType_Library: // intentional fall through
		return type != eItemType_State;
	default: // assumes the rest are actual controls
		{
			CAudioControl const* pControl = static_cast<CAudioControl const*>(&parent);
			if (pControl && pControl->GetType() == eItemType_Switch)
			{
				return type == eItemType_State;
			}
		}
	}

	return false;

}

void DecodeMimeData(const QMimeData* pData, std::vector<IAudioAsset*>& outItems)
{
	const CDragDropData* dragDropData = CDragDropData::FromMimeData(pData);
	if (dragDropData->HasCustomData("Items"))
	{
		QByteArray byteArray = dragDropData->GetCustomData("Items");
		QDataStream stream(byteArray);
		while (!stream.atEnd())
		{
			intptr_t ptr;
			stream >> ptr;
			outItems.push_back(reinterpret_cast<IAudioAsset*>(ptr));
		}
	}
}

bool CanDropMimeData(const QMimeData* pData, const IAudioAsset& parent)
{
	// Handle first if mime data is an external (from the implementation side) source
	std::vector<IAudioSystemItem*> implItems;
	AudioModelUtils::DecodeImplMimeData(pData, implItems);
	if (!implItems.empty())
	{
		IAudioSystemEditor* pImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
		for (IAudioSystemItem* pImplItem : implItems)
		{
			if (!IsParentValid(parent, pImpl->ImplTypeToATLType(pImplItem->GetType())))
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		// Handle if mime data is an internal move (rearranging controls)
		std::vector<IAudioAsset*> droppedItems;
		DecodeMimeData(pData, droppedItems);
		for (IAudioAsset* pItem : droppedItems)
		{
			if (!IsParentValid(parent, pItem->GetType()))
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

void DropMimeData(const QMimeData* pData, IAudioAsset& parent)
{
	// Handle first if mime data is an external (from the implementation side) source
	std::vector<IAudioSystemItem*> implItems;
	AudioModelUtils::DecodeImplMimeData(pData, implItems);
	if (!implItems.empty())
	{
		CAudioAssetsManager* pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();
		for (IAudioSystemItem* pImplControl : implItems)
		{
			pAssetsManager->CreateAndConnectImplItems(pImplControl, &parent);
		}
	}
	else
	{
		std::vector<IAudioAsset*> droppedItems;
		DecodeMimeData(pData, droppedItems);
		std::vector<IAudioAsset*> topLevelDroppedItems;
		Utils::SelectTopLevelAncestors(droppedItems, topLevelDroppedItems);
		CAudioControlsEditorPlugin::GetAssetsManager()->MoveItems(&parent, topLevelDroppedItems);
	}
}

CAudioAssetsExplorerModel::CAudioAssetsExplorerModel(CAudioAssetsManager* pAssetsManager)
	: m_pAssetsManager(pAssetsManager)
{
	ConnectToSystem();
}

CAudioAssetsExplorerModel::~CAudioAssetsExplorerModel()
{
	DisconnectFromSystem();
}

int CAudioAssetsExplorerModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
	{
		return m_pAssetsManager->GetLibraryCount();
	}
	return 0;
}

int CAudioAssetsExplorerModel::columnCount(const QModelIndex& parent) const
{
	return 1;
}

QVariant CAudioAssetsExplorerModel::data(const QModelIndex& index, int role) const
{
	CAudioLibrary* pLibrary = static_cast<CAudioLibrary*>(index.internalPointer());
	if (pLibrary)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			if (pLibrary->IsModified())
			{
				return QtUtil::ToQStringSafe(pLibrary->GetName()) + " *";
			}
			return QtUtil::ToQStringSafe(pLibrary->GetName());

		case Qt::EditRole:
			return QtUtil::ToQStringSafe(pLibrary->GetName());

		case Qt::DecorationRole:
			return GetItemTypeIcon(EItemType::eItemType_Library);

		case EDataRole::eDataRole_ItemType:
			return EItemType::eItemType_Library;

		case EDataRole::eDataRole_InternalPointer:
			return reinterpret_cast<intptr_t>(pLibrary);
			break;
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
bool CAudioAssetsExplorerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (index.isValid())
	{
		IAudioAsset* const pItem = static_cast<IAudioAsset*>(index.internalPointer());

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
							pItem->SetModified(true);
						}

						return true;
					}
				}
			default:
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] The role \'%d\' is not handled!", role);
				break;
			}
		}
	}

	return false;
}

QVariant CAudioAssetsExplorerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		return "Name";
	}
	return QVariant();
}

Qt::ItemFlags CAudioAssetsExplorerModel::flags(const QModelIndex& index) const
{
	if (index.isValid())
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
	}
	return Qt::NoItemFlags;
}

QModelIndex CAudioAssetsExplorerModel::index(int row, int column, const QModelIndex& parent /*= QModelIndex()*/) const
{
	if ((row >= 0) && (column >= 0))
	{
		return createIndex(row, column, reinterpret_cast<quintptr>(m_pAssetsManager->GetLibrary(row)));
	}
	return QModelIndex();
}

QModelIndex CAudioAssetsExplorerModel::parent(const QModelIndex& index) const
{
	return QModelIndex();
}

bool CAudioAssetsExplorerModel::canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		IAudioAsset* pAsset = static_cast<IAudioAsset*>(parent.internalPointer());
		CRY_ASSERT(pAsset != nullptr);
		return CanDropMimeData(pData, *pAsset);
	}
	return false;
}

bool CAudioAssetsExplorerModel::dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
	if (parent.isValid())
	{
		CAudioLibrary* pLibrary = static_cast<CAudioLibrary*>(parent.internalPointer());
		CRY_ASSERT(pLibrary != nullptr);
		DropMimeData(pData, *pLibrary);
		return true;
	}
	return false;
}

Qt::DropActions CAudioAssetsExplorerModel::supportedDropActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

QStringList CAudioAssetsExplorerModel::mimeTypes() const
{
	QStringList result;
	result << CDragDropData::GetMimeFormatForType("Items");
	result << QAudioSystemModel::ms_szMimeType;
	return result;
}

void CAudioAssetsExplorerModel::ConnectToSystem()
{
	m_pAssetsManager->signalLibraryAboutToBeAdded.Connect([&]()
		{
			const int row = m_pAssetsManager->GetLibraryCount();
			beginInsertRows(QModelIndex(), row, row);
	  }, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalLibraryAdded.Connect([&](CAudioLibrary* pLibrary) { endInsertRows(); }, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalLibraryAboutToBeRemoved.Connect([&](CAudioLibrary* pLibrary)
		{
			const int libCount = m_pAssetsManager->GetLibraryCount();
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

void CAudioAssetsExplorerModel::DisconnectFromSystem()
{
	m_pAssetsManager->signalLibraryAboutToBeAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalLibraryAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalLibraryAboutToBeRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalLibraryRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

QControlsProxyFilter::QControlsProxyFilter(QObject* parent)
	: QDeepFilterProxyModel(QDeepFilterProxyModel::BehaviorFlags(QDeepFilterProxyModel::AcceptIfChildMatches), parent)
{

}

bool QControlsProxyFilter::rowMatchesFilter(int source_row, const QModelIndex& source_parent) const
{
	if (QDeepFilterProxyModel::rowMatchesFilter(source_row, source_parent))
	{
		QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
		if (index.isValid())
		{
			if (m_validControlsMask != std::numeric_limits<uint>::max())
			{
				// has filtering
				const EItemType itemType = (EItemType)sourceModel()->data(index, EDataRole::eDataRole_ItemType).toUInt();
				if (itemType < eItemType_Folder)
				{
					return m_validControlsMask & (1 << itemType);
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

bool QControlsProxyFilter::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
	if (left.column() == right.column())
	{
		EItemType leftType = static_cast<EItemType>(sourceModel()->data(left, EDataRole::eDataRole_ItemType).toInt());
		EItemType rightType = static_cast<EItemType>(sourceModel()->data(right, EDataRole::eDataRole_ItemType).toInt());
		if (leftType != rightType)
		{
			return rightType < EItemType::eItemType_Folder;
		}

		QVariant valueLeft = sourceModel()->data(left, Qt::DisplayRole);
		QVariant valueRight = sourceModel()->data(right, Qt::DisplayRole);

		return valueLeft < valueRight;
	}
	return QSortFilterProxyModel::lessThan(left, right);
}

void QControlsProxyFilter::EnableControl(const bool bEnabled, const EItemType type)
{
	if (bEnabled)
	{
		m_validControlsMask |= (1 << (int)type);
	}
	else
	{
		m_validControlsMask &= ~(1 << (int)type);
	}
	invalidate();
}

/////////////////////////////////////////////////////////////////////////////////////////

CAudioLibraryModel::CAudioLibraryModel(CAudioAssetsManager* pAssetsManager, CAudioLibrary* pLibrary)
	: m_pAssetsManager(pAssetsManager)
	, m_pLibrary(pLibrary)
{
	ConnectToSystem();
}

CAudioLibraryModel::~CAudioLibraryModel()
{
	DisconnectFromSystem();
}

QModelIndex CAudioLibraryModel::IndexFromItem(const IAudioAsset* pItem) const
{
	if (pItem)
	{
		IAudioAsset* pParent = pItem->GetParent();
		if (pParent)
		{
			const int size = pParent->ChildCount();
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

int CAudioLibraryModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
	{
		return m_pLibrary->ChildCount();
	}

	IAudioAsset* pAsset = static_cast<IAudioAsset*>(parent.internalPointer());
	if (pAsset)
	{
		return pAsset->ChildCount();
	}

	return 0;
}

int CAudioLibraryModel::columnCount(const QModelIndex& parent) const
{
	return 1;
}

QVariant CAudioLibraryModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	IAudioAsset* pItem = static_cast<IAudioAsset*>(index.internalPointer());
	if (pItem)
	{
		const EItemType itemType = pItem->GetType();
		switch (role)
		{
		case Qt::DisplayRole:
			return QtUtil::ToQStringSafe(pItem->GetName());

		case Qt::EditRole:
			return QtUtil::ToQStringSafe(pItem->GetName());

		case Qt::DecorationRole:
			return GetItemTypeIcon(itemType);

		case EDataRole::eDataRole_ItemType:
			return pItem->GetType();

		case EDataRole::eDataRole_InternalPointer:
			return reinterpret_cast<intptr_t>(pItem);
			break;
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
bool CAudioLibraryModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (index.isValid())
	{
		IAudioAsset* const pItem = static_cast<IAudioAsset*>(index.internalPointer());

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
							EItemType const itemType = pItem->GetType();

							switch (itemType)
							{
							case EItemType::eItemType_Preload:
							case EItemType::eItemType_RTPC:
							case EItemType::eItemType_State:
							case EItemType::eItemType_Switch:
							case EItemType::eItemType_Trigger:
							case EItemType::eItemType_Environment:
								pItem->SetName(Utils::GenerateUniqueControlName(newName, itemType, *m_pAssetsManager));
								pItem->SetModified(true);
								break;
							case EItemType::eItemType_Folder:
								pItem->SetName(Utils::GenerateUniqueName(newName, itemType, pItem->GetParent()));
								pItem->SetModified(true);
								break;
							default:
								CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] The item type \'%d\' is not handled!", itemType);
								break;
							}
						}

						return true;
					}
				}
			default:
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] The role \'%d\' is not handled!", role);
				break;
			}
		}
	}

	return false;
}

QVariant CAudioLibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		return "Name";
	}

	return QVariant();
}

Qt::ItemFlags CAudioLibraryModel::flags(const QModelIndex& index) const
{
	if (index.isValid())
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
	}
	return Qt::NoItemFlags;
}

QModelIndex CAudioLibraryModel::index(int row, int column, const QModelIndex& parent) const
{
	if ((row >= 0) && (column >= 0))
	{
		if (parent.isValid())
		{
			IAudioAsset* pParent = static_cast<IAudioAsset*>(parent.internalPointer());
			if (pParent && row < pParent->ChildCount())
			{
				IAudioAsset* pChild = pParent->GetChild(row);
				if (pChild)
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
QModelIndex CAudioLibraryModel::parent(const QModelIndex& index) const
{
	if (index.isValid())
	{
		IAudioAsset* pItem = static_cast<IAudioAsset*>(index.internalPointer());
		if (pItem)
		{
			if (pItem->GetParent()->GetType() != EItemType::eItemType_Library)
			{
				return IndexFromItem(pItem->GetParent());
			}
		}
	}
	return QModelIndex();
}

bool CAudioLibraryModel::canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		IAudioAsset* pAsset = static_cast<IAudioAsset*>(parent.internalPointer());
		CRY_ASSERT(pAsset != nullptr);
		return CanDropMimeData(pData, *pAsset);
	}
	return false;
}

bool CAudioLibraryModel::dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
	if (parent.isValid())
	{
		IAudioAsset* pAsset = static_cast<IAudioAsset*>(parent.internalPointer());
		CRY_ASSERT(pAsset != nullptr);
		DropMimeData(pData, *pAsset);
		return true;
	}
	return false;
}

QMimeData* CAudioLibraryModel::mimeData(const QModelIndexList& indexes) const
{
	CDragDropData* pDragDropData = new CDragDropData();
	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);
	for (auto index : indexes)
	{
		stream << reinterpret_cast<intptr_t>(index.internalPointer());
	}
	pDragDropData->SetCustomData("Items", byteArray);
	return pDragDropData;
}

Qt::DropActions CAudioLibraryModel::supportedDropActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

QStringList CAudioLibraryModel::mimeTypes() const
{
	QStringList result;
	result << CDragDropData::GetMimeFormatForType("Items");
	result << QAudioSystemModel::ms_szMimeType;
	return result;
}

void CAudioLibraryModel::ConnectToSystem()
{
	CAudioAssetsManager* pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();
	pAssetsManager->signalItemAboutToBeAdded.Connect([&](IAudioAsset* pParent)
		{
			if (Utils::GetParentLibrary(pParent) == m_pLibrary)
			{
			  int row = pParent->ChildCount();

			  if (pParent->GetType() == EItemType::eItemType_Library)
			  {
			    CRY_ASSERT(pParent == m_pLibrary);
			    beginInsertRows(QModelIndex(), row, row);
			  }
			  else
			  {
			    QModelIndex parent = IndexFromItem(pParent);
			    beginInsertRows(parent, row, row);
			  }
			}

	  }, reinterpret_cast<uintptr_t>(this));

	pAssetsManager->signalItemAdded.Connect([&](IAudioAsset* pAsset)
		{
			if (Utils::GetParentLibrary(pAsset) == m_pLibrary)
			{
			  endInsertRows();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	pAssetsManager->signalItemAboutToBeRemoved.Connect([&](IAudioAsset* pAsset)
		{
			if (Utils::GetParentLibrary(pAsset) == m_pLibrary)
			{
			  IAudioAsset* pParent = pAsset->GetParent();
			  const int childCount = pParent->ChildCount();
			  for (int i = 0; i < childCount; ++i)
			  {
			    if (pParent->GetChild(i) == pAsset)
			    {
			      if (pParent->GetType() == EItemType::eItemType_Library)
			      {
			        CRY_ASSERT(pParent == m_pLibrary);
			        beginRemoveRows(QModelIndex(), i, i);
			      }
			      else
			      {
			        QModelIndex parent = IndexFromItem(pParent);
			        beginRemoveRows(parent, i, i);
			      }
			      break;
			    }
			  }
			}
	  }, reinterpret_cast<uintptr_t>(this));

	pAssetsManager->signalItemRemoved.Connect([&](IAudioAsset* pParent, IAudioAsset* pAsset)
		{
			if (Utils::GetParentLibrary(pParent) == m_pLibrary)
			{
			  endRemoveRows();
			}
	  }, reinterpret_cast<uintptr_t>(this));
}

void CAudioLibraryModel::DisconnectFromSystem()
{
	CAudioAssetsManager* pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();
	pAssetsManager->signalItemAboutToBeAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	pAssetsManager->signalItemAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	pAssetsManager->signalItemAboutToBeRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	pAssetsManager->signalItemRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

namespace AudioModelUtils
{
void GetAssetsFromIndices(const QModelIndexList& list, std::vector<CAudioLibrary*>& outLibraries, std::vector<CAudioFolder*>& outFolders, std::vector<CAudioControl*>& outControls)
{
	for (auto& index : list)
	{
		QVariant internalPtr = index.data(EDataRole::eDataRole_InternalPointer);
		if (internalPtr.isValid())
		{
			QVariant type = index.data(EDataRole::eDataRole_ItemType);
			switch (type.toInt())
			{
			case EItemType::eItemType_Library:
				outLibraries.push_back(reinterpret_cast<CAudioLibrary*>(internalPtr.value<intptr_t>()));
				break;
			case EItemType::eItemType_Folder:
				outFolders.push_back(reinterpret_cast<CAudioFolder*>(internalPtr.value<intptr_t>()));
				break;
			default:
				outControls.push_back(reinterpret_cast<CAudioControl*>(internalPtr.value<intptr_t>()));
				break;
			}
		}
	}
}

IAudioAsset* GetAssetFromIndex(const QModelIndex& index)
{
	QVariant internalPtr = index.data(EDataRole::eDataRole_InternalPointer);
	if (internalPtr.isValid())
	{
		return reinterpret_cast<IAudioAsset*>(internalPtr.value<intptr_t>());
	}
	return nullptr;
}
} // namespace AudioModelUtils
} // namespace ACE
