// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QTreeWidget.h>
#include <QAdvancedTreeView.h>
#include <QDropEvent>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <ACETypes.h>

class QAudioControlSortProxy : public QSortFilterProxyModel
{
public:
	explicit QAudioControlSortProxy(QObject* pParent = 0);
	bool setData(const QModelIndex& index, const QVariant& value, int role /* = Qt::EditRole */);

protected:
	bool lessThan(const QModelIndex& left, const QModelIndex& right) const;
};

class QAudioControlsTreeView : public QAdvancedTreeView
{
public:
	explicit QAudioControlsTreeView(QWidget* pParent = 0);
	void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible);
	bool IsEditing();
	void SaveExpandedState();
	void RestoreExpandedState();

private:
	void SaveExpandedState(QModelIndex& index);

	std::vector<QPersistentModelIndex> m_storedExpandedItems;

};
