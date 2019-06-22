// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/SharedData.h"
#include <QAdvancedTreeView.h>

namespace ACE
{
class CTreeView final : public QAdvancedTreeView
{
	Q_OBJECT

public:

	CTreeView() = delete;
	CTreeView(CTreeView const&) = delete;
	CTreeView(CTreeView&&) = delete;
	CTreeView& operator=(CTreeView const&) = delete;
	CTreeView& operator=(CTreeView&&) = delete;

	explicit CTreeView(QWidget* pParent);
	explicit CTreeView(QWidget* pParent, QAdvancedTreeView::BehaviorFlags flags);
	virtual ~CTreeView() override = default;

	bool IsEditing() const { return state() == QAbstractItemView::EditingState; }

	void ExpandSelection();
	void CollapseSelection();

	void BackupExpanded();
	void RestoreExpanded();

	void BackupSelection();
	void RestoreSelection();

	void SetNameColumn(int nameColumn) { m_nameColumn = nameColumn; }

private:

	ControlId GetItemId(QModelIndex const& index) const;

	void      ExpandSelectionRecursively(QModelIndexList const& indexList);
	void      CollapseSelectionRecursively(QModelIndexList const& indexList);

	void      BackupExpandedRecursively(QModelIndex const& index);
	void      RestoreExpandedRecursively(QModelIndex const& index);
	void      RestoreSelectionRecursively(QModelIndex const& index);

	int             m_nameColumn;
	QSet<ControlId> m_expandedBackup;
	QSet<ControlId> m_selectionBackup;
};
} // namespace ACE
