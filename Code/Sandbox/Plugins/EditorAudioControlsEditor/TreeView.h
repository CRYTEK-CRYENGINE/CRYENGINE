// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAdvancedTreeView.h>

namespace ACE
{
class CTreeView final : public QAdvancedTreeView
{
	Q_OBJECT

public:

	explicit CTreeView(QWidget* const pParent, QAdvancedTreeView::BehaviorFlags const flags = QAdvancedTreeView::BehaviorFlags(UseItemModelAttribute));

	CTreeView() = delete;

	QModelIndexList GetSelectedIndexes() const { return selectedIndexes(); }
	bool            IsEditing() const;

	void            ExpandSelection(QModelIndexList const& indexList);
	void            CollapseSelection(QModelIndexList const& indexList);

	void            BackupExpanded();
	void            RestoreExpanded();

	void            BackupSelection();
	void            RestoreSelection();

	void            SetNameRole(int const nameRole)     { m_nameRole = nameRole; }
	void            SetNameColumn(int const nameColumn) { m_nameColumn = nameColumn; }

private:

	uint32 GetItemId(QModelIndex const& index) const;
	void   BackupExpandedChildren(QModelIndex const& index);
	void   RestoreExpandedChildren(QModelIndex const& index);
	void   RestoreSelectionChildren(QModelIndex const& index);

	int          m_nameRole = 0;
	int          m_nameColumn = 0;
	QSet<uint32> m_expandedBackup;
	QSet<uint32> m_selectionBackup;
};
} // namespace ACE

