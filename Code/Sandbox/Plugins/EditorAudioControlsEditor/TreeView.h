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

	bool IsEditing() const { return state() == QAbstractItemView::EditingState; }

	void ExpandSelection();
	void CollapseSelection();

	void BackupExpanded();
	void RestoreExpanded();

	void BackupSelection();
	void RestoreSelection();

	void SetNameRole(int const nameRole)     { m_nameRole = nameRole; }
	void SetNameColumn(int const nameColumn) { m_nameColumn = nameColumn; }

private:

	uint32 GetItemId(QModelIndex const& index) const;

	void   ExpandSelectionRecursively(QModelIndexList const& indexList);
	void   CollapseSelectionRecursively(QModelIndexList const& indexList);

	void   BackupExpandedRecursively(QModelIndex const& index);
	void   RestoreExpandedRecursively(QModelIndex const& index);
	void   RestoreSelectionRecursively(QModelIndex const& index);

	int          m_nameRole = 0;
	int          m_nameColumn = 0;
	QSet<uint32> m_expandedBackup;
	QSet<uint32> m_selectionBackup;
};
} // namespace ACE

