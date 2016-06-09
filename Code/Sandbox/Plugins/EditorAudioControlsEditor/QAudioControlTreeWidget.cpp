// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QAudioControlTreeWidget.h"
#include "QAudioControlEditorIcons.h"
#include "QtUtil.h"
#include <ACETypes.h>

#include <QAdvancedItemDelegate.h>

using namespace ACE;

QFolderItem::QFolderItem(const QString& sName) : QStandardItem(sName)
{
	setIcon(GetFolderIcon());
	setData(eItemType_Folder, eDataRole_Type);
	setData(ACE_INVALID_ID, eDataRole_Id);

	setFlags(flags() | Qt::ItemIsDropEnabled);
	setFlags(flags() | Qt::ItemIsDragEnabled);

	setData(false, eDataRole_Modified);
}

QAudioControlItem::QAudioControlItem(const QString& sName, CATLControl* pControl) : QStandardItem(sName)
{
	setIcon(GetControlTypeIcon(pControl->GetType()));
	setData(eItemType_AudioControl, eDataRole_Type);
	setData(pControl->GetId(), eDataRole_Id);

	EACEControlType eType = pControl->GetType();
	if (eType == eACEControlType_Switch)
	{
		setFlags(flags() | Qt::ItemIsDropEnabled);
		setFlags(flags() | Qt::ItemIsDragEnabled);
	}
	else if (eType == eACEControlType_State)
	{
		setFlags(flags() & ~Qt::ItemIsDropEnabled);
		setFlags(flags() & ~Qt::ItemIsDragEnabled);
	}
	else
	{
		setFlags(flags() & ~Qt::ItemIsDropEnabled);
		setFlags(flags() | Qt::ItemIsDragEnabled);
	}
	setData(false, eDataRole_Modified);
}

QAudioControlSortProxy::QAudioControlSortProxy(QObject* pParent /*= 0*/)
	: QSortFilterProxyModel(pParent)
{

}

bool QAudioControlSortProxy::setData(const QModelIndex& index, const QVariant& value, int role /* = Qt::EditRole */)
{
	if ((role == Qt::EditRole))
	{
		QString sInitialName = value.toString();
		if (sInitialName.isEmpty() || sInitialName.contains(" "))
		{
			// TODO: Prevent user from inputing spaces to name
			return false;
		}

		if (index.data(eDataRole_Type) == eItemType_Folder)
		{
			// Validate that the new folder name is valid
			bool bFoundValidName = false;
			QString sCandidateName = sInitialName;
			int nNumber = 1;
			while (!bFoundValidName)
			{
				bFoundValidName = true;
				int i = 0;
				QModelIndex sibiling = index.sibling(i, 0);
				while (sibiling.isValid())
				{
					QString sSibilingName = sibiling.data(Qt::DisplayRole).toString();
					if ((sibiling != index) && (sibiling.data(eDataRole_Type) == eItemType_Folder) && (QString::compare(sCandidateName, sSibilingName, Qt::CaseInsensitive) == 0))
					{
						sCandidateName = sInitialName + "_" + QString::number(nNumber);
						++nNumber;
						bFoundValidName = false;
						break;
					}
					++i;
					sibiling = index.sibling(i, 0);
				}
			}
			return QSortFilterProxyModel::setData(index, sCandidateName, role);
		}
	}
	return QSortFilterProxyModel::setData(index, value, role);
}

bool QAudioControlSortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
	uint eLeftType = sourceModel()->data(left, eDataRole_Type).toUInt();
	uint eRightType = sourceModel()->data(right, eDataRole_Type).toUInt();
	if (eLeftType != eRightType)
	{
		return eLeftType > eRightType;
	}
	return left.data(Qt::DisplayRole) > right.data(Qt::DisplayRole);
}

QAudioControlsTreeView::QAudioControlsTreeView(QWidget* pParent /*= 0*/)
	: QTreeView(pParent)
{
	setItemDelegate(new QAdvancedItemDelegate(this));
}

void QAudioControlsTreeView::scrollTo(const QModelIndex& index, ScrollHint hint /*= EnsureVisible*/)
{
	// QTreeView::scrollTo() expands all the parent items but
	// it is disabled when handling a Drag&Drop event so have to do it manually
	if (state() != NoState)
	{
		QModelIndex parent = index.parent();
		while (parent.isValid())
		{
			if (!isExpanded(parent))
			{
				expand(parent);
			}
			parent = parent.parent();
		}
	}
	QTreeView::scrollTo(index, hint);
}

bool QAudioControlsTreeView::IsEditing()
{
	return state() == QAbstractItemView::EditingState;
}

void QAudioControlsTreeView::SaveExpandedState()
{
	m_storedExpandedItems.clear();
	const size_t size = model()->rowCount();
	for (int i = 0; i < size; ++i)
	{
		SaveExpandedState(model()->index(i, 0));
	}
}

void QAudioControlsTreeView::SaveExpandedState(QModelIndex& index)
{
	if (isExpanded(index))
	{
		m_storedExpandedItems.push_back(QPersistentModelIndex(index));
	}

	int row = 0;
	QModelIndex ind = index.child(row, 0);
	while (ind.isValid())
	{
		SaveExpandedState(ind);
		ind = index.child(++row, 0);
	}
}

void QAudioControlsTreeView::RestoreExpandedState()
{
	std::for_each(m_storedExpandedItems.begin(), m_storedExpandedItems.end(), [&](QPersistentModelIndex& index) { if (index.isValid()) setExpanded(index, true); });
}
