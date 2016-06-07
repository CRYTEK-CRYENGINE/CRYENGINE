// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IAudioSystemItem.h"
#include "AudioControlsEditorPlugin.h"
#include "AudioSystemModel.h"

#include <QtUtil.h>
#include <QStandardItem>

#include "AudioControl.h"
#include "AudioControlsEditorPlugin.h"
#include "AudioControlsEditorUndo.h"
#include "AudioSystemModel.h"
#include "IAudioSystemEditor.h"
#include "IAudioSystemItem.h"
#include "IEditor.h"
#include "QATLControlsTreeModel.h"
#include "QAudioControlTreeWidget.h"

#include "Controls/QuestionDialog.h"

namespace ACE
{

QATLTreeModel::QATLTreeModel()
	: m_pControlsModel(nullptr)
{
}

QATLTreeModel::~QATLTreeModel()
{
	if (m_pControlsModel)
	{
		m_pControlsModel->RemoveListener(this);
	}
}

void QATLTreeModel::Initialize(CATLControlsModel* pControlsModel)
{
	m_pControlsModel = pControlsModel;
	m_pControlsModel->AddListener(this);
}

QStandardItem* QATLTreeModel::GetItemFromControlID(CID nID)
{
	QModelIndexList indexes = match(index(0, 0, QModelIndex()), eDataRole_Id, nID, 1, Qt::MatchRecursive);
	if (!indexes.empty())
	{
		return itemFromIndex(indexes.at(0));
	}
	return nullptr;
}

QStandardItem* QATLTreeModel::AddControl(CATLControl* pControl, QStandardItem* pParent, int nRow)
{
	if (pControl && pParent)
	{
		QStandardItem* pItem = new QAudioControlItem(QtUtil::ToQString(pControl->GetName()), pControl);
		if (pItem)
		{
			pParent->insertRow(nRow, pItem);
			SetItemAsDirty(pItem);
		}
		return pItem;
	}
	return nullptr;
}

QStandardItem* QATLTreeModel::CreateFolder(QStandardItem* pParent, const string& sName, int nRow)
{
	if (pParent == nullptr)
	{
		pParent = ControlsRootItem();
	}

	if (pParent)
	{
		// Make a valid name for the folder (avoid having folders with the same name under same root)
		QString sRootName = QtUtil::ToQString(sName);
		QString sFolderName = sRootName;
		int number = 1;
		bool bFoundName = false;
		while (!bFoundName)
		{
			bFoundName = true;
			const int size = pParent->rowCount();
			for (int i = 0; i < size; ++i)
			{
				QStandardItem* pItem = pParent->child(i);
				if (pItem && (pItem->data(eDataRole_Type) == eItemType_Folder) && (QString::compare(sFolderName, pItem->text(), Qt::CaseInsensitive) == 0))
				{
					bFoundName = false;
					sFolderName = sRootName + "_" + QString::number(number);
					++number;
					break;
				}
			}
		}

		if (!sFolderName.isEmpty())
		{
			QStandardItem* pFolderItem = new QFolderItem(sFolderName);
			if (pFolderItem)
			{
				pParent->insertRow(nRow, pFolderItem);
				SetItemAsDirty(pFolderItem);
				if (!CUndo::IsSuspended())
				{
					CUndo undo("Audio Folder Created");
					CUndo::Record(new CUndoFolderAdd(pFolderItem));
				}
				return pFolderItem;
			}
		}
	}
	return nullptr;
}

void QATLTreeModel::RemoveItem(QModelIndex index)
{
	QModelIndexList indexList;
	indexList.push_back(index);
	RemoveItems(indexList);
}

void QATLTreeModel::RemoveItems(QModelIndexList indexList)
{
	// Sort the controls by the level they are inside the tree
	// (the deepest in the tree first) and then by their row number.
	// This way we guarantee we don't delete the parent of a
	// control before its children
	struct STreeIndex
	{
		QPersistentModelIndex m_index;
		int                   m_level;
		STreeIndex(QPersistentModelIndex index, int level) : m_index(index), m_level(level) {}

		bool operator<(const STreeIndex& index)
		{
			if (m_level == index.m_level)
			{
				return m_index.row() > index.m_index.row();
			}
			return m_level > index.m_level;
		}
	};
	std::vector<STreeIndex> sortedIndexList;

	const int size = indexList.length();
	for (int i = 0; i < size; ++i)
	{
		int level = 0;
		QModelIndex index = indexList[i];
		while (index.isValid())
		{
			++level;
			index = index.parent();
		}
		sortedIndexList.push_back(STreeIndex(indexList[i], level));
	}
	std::sort(sortedIndexList.begin(), sortedIndexList.end());

	for (int i = 0; i < size; ++i)
	{
		QModelIndex index = sortedIndexList[i].m_index;
		if (index.isValid())
		{
			DeleteInternalData(index);

			// Mark parent as modified
			QModelIndex parent = index.parent();
			if (parent.isValid())
			{
				SetItemAsDirty(itemFromIndex(parent));
			}
			removeRow(index.row(), index.parent());
		}
	}
}

void QATLTreeModel::DeleteInternalData(QModelIndex root)
{
	// Delete children first and in reverse order
	// of their row (so that we can undo them in the same order)
	std::vector<QModelIndex> childs;
	QModelIndex child = root.child(0, 0);
	for (int i = 1; child.isValid(); ++i)
	{
		childs.push_back(child);
		child = root.child(i, 0);
	}

	const size_t size = childs.size();
	for (size_t i = 0; i < size; ++i)
	{
		DeleteInternalData(childs[(size - 1) - i]);
	}

	if (root.data(eDataRole_Type) == eItemType_AudioControl)
	{
		m_pControlsModel->RemoveControl(root.data(eDataRole_Id).toUInt());
	}
	else
	{
		if (!CUndo::IsSuspended())
		{
			CUndo::Record(new CUndoFolderRemove(itemFromIndex(root)));
		}
	}
}

CATLControl* QATLTreeModel::GetControlFromIndex(QModelIndex index)
{
	if (m_pControlsModel && index.isValid() && (index.data(eDataRole_Type) == eItemType_AudioControl))
	{
		return m_pControlsModel->GetControlByID(index.data(eDataRole_Id).toUInt());
	}
	return nullptr;
}

void QATLTreeModel::OnControlModified(CATLControl* pControl)
{
	if (pControl)
	{
		QStandardItem* pItem = GetItemFromControlID(pControl->GetId());
		if (pItem)
		{
			QString sNewName = QtUtil::ToQString(pControl->GetName());
			if (pItem->text() != sNewName)
			{
				pItem->setText(sNewName);
			}
			SetItemAsDirty(pItem);
		}
	}
}

void QATLTreeModel::SetItemAsDirty(QStandardItem* pItem)
{
	if (pItem)
	{
		blockSignals(true);
		pItem->setData(true, eDataRole_Modified);
		blockSignals(false);

		SetItemAsDirty(pItem->parent());
	}
}

QStandardItem* QATLTreeModel::ControlsRootItem()
{
	QStandardItem* pRoot = invisibleRootItem();
	if (pRoot)
	{
		QStandardItem* pControlsParent = pRoot->child(0);
		if (pControlsParent)
		{
			return pControlsParent;
		}
		else
		{
			pControlsParent = new QStandardItem("Audio Controls");
			if (pControlsParent)
			{
				invisibleRootItem()->insertRow(0, pControlsParent);
				pControlsParent->setData(ACE_INVALID_ID, eDataRole_Id);
				pControlsParent->setData(eItemType_Root, eDataRole_Type);
				pControlsParent->setData(false, eDataRole_Modified);
				pControlsParent->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);

				QFont boldFont;
				boldFont.setBold(true);
				pControlsParent->setFont(boldFont);
				return pControlsParent;
			}
		}
	}

	return nullptr;
}

CATLControl* QATLTreeModel::CreateControl(EACEControlType eControlType, const string& sName, CATLControl* pParent)
{
	return m_pControlsModel->CreateControl(sName, eControlType, pParent);
}

bool QATLTreeModel::dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
	QStandardItem* pRoot = ControlsRootItem();
	QStandardItem* pTargetItem = pRoot;
	if (parent.isValid())
	{
		pTargetItem = itemFromIndex(parent);
	}
	if (pTargetItem)
	{
		if (pTargetItem && (pTargetItem->data(eDataRole_Type) == eItemType_Folder || pTargetItem == pRoot))
		{
			const QString format = "application/x-qabstractitemmodeldatalist";
			if (pData->hasFormat(format))
			{
				QByteArray encoded = pData->data(format);
				QDataStream stream(&encoded, QIODevice::ReadOnly);
				while (!stream.atEnd())
				{
					int row, col;
					QMap<int, QVariant> roleDataMap;
					stream >> row >> col >> roleDataMap;
					if (!roleDataMap.isEmpty())
					{
						// If dropping a folder, make sure that folder name doesn't already exist where it is being dropped
						if (roleDataMap[eDataRole_Type] == eItemType_Folder)
						{
							// Make sure the target folder doesn't have a folder with the same name
							QString droppedFolderName = roleDataMap[Qt::DisplayRole].toString();
							const int size = pTargetItem->rowCount();
							for (int i = 0; i < size; ++i)
							{
								QStandardItem* pItem = pTargetItem->child(i);
								if (pItem && (pItem->data(eDataRole_Type) == eItemType_Folder) && (QString::compare(droppedFolderName, pItem->text(), Qt::CaseInsensitive) == 0))
								{
									QString msg("This destination already contains a folder named '%1'.");
									msg = msg.arg(droppedFolderName);
									CQuestionDialog::SCritical("Audio Controls Editor", msg);
									return false;
								}
							}
						}
					}
				}
			}
		}
	}

	if (pData && action == Qt::MoveAction)
	{
		if (!CUndo::IsSuspended())
		{
			CUndo undo("Audio Control Moved");
			CUndo::Record(new CUndoItemMove());
		}
	}

	return QStandardItemModel::dropMimeData(pData, action, row, column, parent);
}

bool QATLTreeModel::IsValidParent(const QModelIndex& parent, const EACEControlType controlType) const
{
	if (parent.data(eDataRole_Type) == eItemType_Folder)
	{
		return controlType != eACEControlType_State;
	}
	else if (controlType == eACEControlType_State)
	{
		const CATLControl* const pControl = m_pControlsModel->GetControlByID(parent.data(eDataRole_Id).toUInt());
		if (pControl)
		{
			return pControl->GetType() == eACEControlType_Switch;
		}
	}

	return false;
}

bool QATLTreeModel::canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
	if (!parent.isValid())
	{
		return false;
	}

	const IAudioSystemEditor* const pAudioSystem = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
	if (pAudioSystem)
	{
		const QString audioMiddlewareFormat = QAudioSystemModel::ms_szMimeType;
		if (pData->hasFormat(audioMiddlewareFormat))
		{
			QByteArray encoded = pData->data(audioMiddlewareFormat);
			QDataStream stream(&encoded, QIODevice::ReadOnly);
			while (!stream.atEnd())
			{
				CID id;
				stream >> id;
				if (id != ACE_INVALID_ID)
				{
					const IAudioSystemItem* const pItem = pAudioSystem->GetControl(id);
					if (pItem)
					{
						if (!IsValidParent(parent, pAudioSystem->ImplTypeToATLType(pItem->GetType())))
						{
							return false;
						}
					}
				}
			}
		}
		else if (parent.data(eDataRole_Type) == eItemType_Root)
		{
			// If dropping at the root, only allow folders
			const QString format = "application/x-qabstractitemmodeldatalist";
			if (pData->hasFormat(format))
			{
				QByteArray encoded = pData->data(format);
				QDataStream stream(&encoded, QIODevice::ReadOnly);
				while (!stream.atEnd())
				{
					int row, col;
					QMap<int, QVariant> roleDataMap;
					stream >> row >> col >> roleDataMap;
					if (!roleDataMap.isEmpty())
					{
						return roleDataMap[eDataRole_Type] == eItemType_Folder;
					}
				}
			}
			return false;
		}

	}
	return QStandardItemModel::canDropMimeData(pData, action, row, column, parent);
}
}
