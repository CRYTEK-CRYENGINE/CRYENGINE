// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAdvancedTreeView.h>

namespace ACE
{
class CAudioTreeView final : public QAdvancedTreeView
{
	Q_OBJECT

public:

	CAudioTreeView();

	QModelIndexList GetSelectedIndexes() const { return selectedIndexes(); }
	bool            IsEditing() const;

	void            ExpandSelection(QModelIndexList const& indexList);
	void            CollapseSelection(QModelIndexList const& indexList);

	void            BackupExpanded();
	void            RestoreExpanded();

	void            BackupSelection();
	void            RestoreSelection();

public slots:

	void OnSelectionChanged(QItemSelection const& selected, QItemSelection const& deselected);

private:

	uint32 GetItemId(QModelIndex const& index) const;
	void   BackupExpandedChildren(QModelIndex const& index);
	void   RestoreExpandedChildren(QModelIndex const& index);
	void   RestoreSelectionChildren(QModelIndex const& index);

	bool         m_hasSelectionChanged;
	QSet<uint32> m_expandedBackup;
	QSet<uint32> m_selectionBackup;
};
} // namespace ACE
