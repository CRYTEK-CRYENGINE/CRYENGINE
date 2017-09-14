// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAdvancedTreeView.h>

namespace ACE
{
class CAdvancedTreeView final : public QAdvancedTreeView
{
	Q_OBJECT

public:

	CAdvancedTreeView() = default;

	using QSetItemId = QSet<uint32>;

	QModelIndexList GetSelectedIndexes() const { return selectedIndexes(); }
	bool            IsEditing() const;

	void            ExpandSelection(QModelIndexList const& indexList);
	void            CollapseSelection(QModelIndexList const& indexList);

	void            BackupExpanded();
	QSetItemId      BackupExpandedOnReset();
	void            RestoreExpanded();
	void            RestoreExpandedOnReset(QSetItemId const& expandedBackup);

	void            BackupSelection();
	QSetItemId      BackupSelectionOnReset();
	void            RestoreSelection();
	void            RestoreSelectionOnReset(QSetItemId const& selectedBackup);

public slots:

	void OnSelectionChanged(QItemSelection const& selected, QItemSelection const& deselected);

private:

	uint32 GetItemId(QModelIndex const& index) const;
	void   BackupExpandedChildren(QModelIndex const& index);
	void   RestoreExpandedChildren(QModelIndex const& index);
	void   RestoreSelectionChildren(QModelIndex const& index);

	bool       m_bSelectionChanged;
	QSetItemId m_expandedBackup;
	QSetItemId m_selectionBackup;
};
} // namespace ACE
