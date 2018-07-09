// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include <QStyledItemDelegate>
#include <QSet>

//! QAdvancedItemDelegate is meant to provide extra generic functionnality that we want to use in many QTreeViews. 
//! It is designed to be used with QAdvancedTreeView.
//! Check the behavior flags to discover all the features it provides
//! Please add all generic behavior you need for your improved tree views so they can be reused easily, maintained properly and not duplicated.
//TODO : Have a default behavior for all columns which can be overriden for specific ones, for convenience and easier setup. Also useful for models which dynamically add or remove columns.
class EDITOR_COMMON_API QAdvancedItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:

	enum Behavior
	{
		None               = 0,
		MultiSelectionEdit = 0x01, //If editing a value with a multi-selection, all items will get their value set
		DragCheck          = 0x02, //checkable columns: click and drag will set the value (similar to photoshop layer window). Do not use with MultiSelectionEdit on checkbox columns
		OverrideCheckIcon  = 0x04, //currently only works for checkbox-only columns, DisplayRole will be ignored
		OverrideDrawRect   = 0x08, // Override the drawing bounds of the column
	};

	Q_DECLARE_FLAGS(BehaviorFlags, Behavior)

	static constexpr int s_IconOverrideRole = Qt::UserRole + 2202;
	static constexpr int s_DrawRectOverrideRole = s_IconOverrideRole + 1;

	QAdvancedItemDelegate(QAbstractItemView* view);

	void SetColumnBehavior(int i, BehaviorFlags behavior);
	bool TestColumnBehavior(int i, BehaviorFlags behavior);

	bool IsDragChecking() { return m_dragChecking; }

protected:

	//QStyledItemDelegate interface
	bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

	//DragCheck internals
	void RevertDragCheck(QAbstractItemModel* model);
	void CancelDragCheck();

	//Helper methods for handling checked items
	bool           IsCheckBoxCheckEvent(QEvent* event, bool& outChecked) const;
	bool           IsEnabledCheckBoxItem(QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index, QVariant& outValue) const;
	bool           IsInCheckBoxRect(QMouseEvent* event, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	Qt::CheckState Check(QAbstractItemModel* model, const QModelIndex& index);

	QAbstractItemView* GetItemView() const { return m_view; }

private:

	QMap<int, BehaviorFlags> m_columnBehavior;
	QAbstractItemView*       m_view;
	bool                     m_dragChecking;
	int                      m_dragCheckButtons;
	int                      m_dragColumn;
	QVariant                 m_checkValue;
	QSet<QModelIndex>        m_checkedIndices;
};

