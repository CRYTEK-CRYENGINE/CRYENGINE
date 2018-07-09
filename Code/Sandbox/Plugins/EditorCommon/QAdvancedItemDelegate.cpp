// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "QAdvancedItemDelegate.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QEvent.h>
#include <QPainter>

// EditorCommon
#include "QtUtil.h"

QAdvancedItemDelegate::QAdvancedItemDelegate(QAbstractItemView* view)
	: QStyledItemDelegate(view)
	, m_view(view)
	, m_dragChecking(false)
	, m_dragColumn(-1)
	, m_dragCheckButtons(0)
{
	view->setItemDelegate(this);
}

void QAdvancedItemDelegate::SetColumnBehavior(int i, BehaviorFlags behavior)
{
	m_columnBehavior[i] = behavior;
}

bool QAdvancedItemDelegate::TestColumnBehavior(int i, BehaviorFlags behavior)
{
	return m_columnBehavior[i] & behavior;
}

bool QAdvancedItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
	auto it = m_columnBehavior.find(index.column());
	bool bhvIsValid = it != m_columnBehavior.end();

	bool checked = false;
	QVariant value;
	switch (event->type())
	{
	case QEvent::MouseButtonPress:
		{
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (bhvIsValid && *it & Behavior::DragCheck)
			{
				// Make sure neither ALT nor CTRL is pressed
				// Ensure the click is ON an ENABLED checkbox, checking it(left click only)
				if (mouseEvent->modifiers() != Qt::KeyboardModifier::AltModifier && mouseEvent->modifiers() != Qt::KeyboardModifier::ControlModifier && IsCheckBoxCheckEvent(mouseEvent, checked) 
					&& IsEnabledCheckBoxItem(model, option, index, value) && IsInCheckBoxRect(mouseEvent, option, index))
				{
					m_dragChecking = true;
					m_dragColumn = index.column();
					m_checkedIndices.insert(index);
					m_dragCheckButtons = mouseEvent->button();
					m_checkValue = Check(model, index);
					return true;
				}
			}

			if (m_dragChecking && mouseEvent->button() != Qt::LeftButton)
			{
				m_dragCheckButtons |= mouseEvent->button();

				//Cancel action, example a right click has been pressed, revert all states
				for (auto& i : m_checkedIndices)
				{
					RevertDragCheck(model);
				}

				CancelDragCheck();
				return true;
			}
		}
		break;
	case QEvent::MouseButtonRelease:
		{
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

			//Handling this all the time, because of cases where RButton release cancelled the drag check and subsequent LButton release would trigger selection event (which we do not want)
			if (m_dragCheckButtons & mouseEvent->button())
			{
				if (mouseEvent->button() != Qt::LeftButton)
				{
					RevertDragCheck(model);
				}

				CancelDragCheck();
				m_dragCheckButtons &= ~(mouseEvent->button());
				return true;
			}

			//MultiSelectionEdit needs special case for checkboxes, as this does not go through delegate's setData
			//TODO: This will still lose selection as mouse release on an item changes the selection to contain this item only. This needs to preserve selection as with other multi-edit
			//Other problem is when clicking on a checkbox that isn't selected. Selection is not updated properly.
			if (bhvIsValid && *it & Behavior::MultiSelectionEdit)
			{
				if (IsCheckBoxCheckEvent(mouseEvent, checked) && IsEnabledCheckBoxItem(model, option, index, value) && IsInCheckBoxRect(mouseEvent, option, index))
				{
					auto checkValue = Check(model, index);

					if (m_view->selectionModel()->isSelected(index))//Avoid processing the selection if we clicked on index not in selection
					{
						auto list = m_view->selectionModel()->selectedRows(index.column());

						for (auto& selectedRow : list)
						{
							QVariant value;
							if (selectedRow != index && IsEnabledCheckBoxItem(model, option, selectedRow, value) && value != checkValue)
								Check(model, selectedRow);
						}
					}

					return true;
				}
			}
		}
	case QEvent::MouseMove:
		{
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (m_dragChecking)
			{
				if (bhvIsValid && *it & Behavior::DragCheck && index.column() == m_dragColumn)
				{
					if (IsEnabledCheckBoxItem(model, option, index, value) && IsInCheckBoxRect(mouseEvent, option, index))
					{
						if (!m_checkedIndices.contains(index))
						{
							m_checkedIndices.insert(index);
							if (value != m_checkValue)
								Check(model, index);
						}
					}
				}
				return true;
			}
		}
		break;
	default:
		break;
	}

	return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void QAdvancedItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	auto it = m_columnBehavior.find(index.column());
	QRect drawRectOverride;

	if (it != m_columnBehavior.end())
	{
		if (*it & Behavior::OverrideCheckIcon)
		{
			QVariant deco = index.data(s_IconOverrideRole);
			if (deco.isValid())
			{
				const QWidget* widget = option.widget;
				QStyle* style = widget ? widget->style() : QApplication::style();

				QStyleOptionViewItem viewOpt(option);
				initStyleOption(&viewOpt, index);
				QRect checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &viewOpt, widget);

				//Draw tree item but not the checkbox (passing option instead of viewOpt)
				style->drawControl(QStyle::CE_ItemViewItem, &option, painter, widget);

				if (deco.type() == QVariant::Pixmap)
				{
					return QtUtil::DrawStatePixmap(painter, checkRect, deco.value<QPixmap>(), option.state);
				}
				else if (deco.type() == QVariant::Icon)
				{
					//TODO : Handle mode and state of the QIcon to render it unchecked
					QIcon icon = deco.value<QIcon>();
					return QtUtil::DrawStatePixmap(painter, checkRect, icon.pixmap(checkRect.size()), option.state);
				}
				else if (deco.type() == QVariant::Color)
				{
					painter->fillRect(checkRect, deco.value<QColor>());
				}
				else
				{
					assert(0); //Invalid variant type for this role
				}

				return;
			}
		}
		if (*it & Behavior::OverrideDrawRect)
		{
			QVariant deco = index.data(s_DrawRectOverrideRole);
			if (deco.isValid() && deco.type() == QVariant::Rect)
			{
				drawRectOverride = deco.toRect();
			}
		}
	}
	QVariant var = index.data(Qt::DecorationRole);
	if (var.isValid())
	{
		const QWidget* widget = option.widget;
		QStyle* style = widget ? widget->style() : QApplication::style();

		QStyleOptionViewItem viewOpt(option);
		initStyleOption(&viewOpt, index);
		QRect decorationRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &viewOpt, widget);

		if (var.type() == QVariant::Color)
		{
			//Overwrite options to not draw the rectangle, but still draw the hover/selected background
			QStyleOptionViewItem opt = option;
			opt.decorationSize = QSize(0, 0);
			QStyledItemDelegate::paint(painter, opt, index);

			if (!drawRectOverride.isEmpty())
			{
				decorationRect.setX(decorationRect.x() + drawRectOverride.x());
				decorationRect.setY(decorationRect.y() + drawRectOverride.y());
				decorationRect.setWidth(drawRectOverride.width());
				decorationRect.setHeight(drawRectOverride.height());
			}

			QPixmap pixmap(decorationRect.width(), decorationRect.height());
			pixmap.fill(var.value<QColor>());
			return painter->drawPixmap(decorationRect.topLeft(), pixmap);
		}

		if (option.state & QStyle::State_Selected || option.state & QStyle::State_MouseOver)
		{
			QPixmap pixmap;
			if (var.type() == QVariant::Icon)
				pixmap = var.value<QIcon>().pixmap(decorationRect.size());
			else if (var.type() == QVariant::Pixmap)
				pixmap = var.value<QPixmap>();
			else
				return QStyledItemDelegate::paint(painter, option, index);

			// Save the current painter state and call base class paint with a clip rect so it paints everything except the decoration
			painter->save();
			painter->setClipRect(decorationRect, Qt::IntersectClip);
			QStyledItemDelegate::paint(painter, option, index);
			painter->restore();
			// Restore painter state and draw the icon with our tree view custom tint color
			return QtUtil::DrawStatePixmap(painter, decorationRect, pixmap, option.state);
		}
	}

	return QStyledItemDelegate::paint(painter, option, index);
}

void QAdvancedItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	auto it = m_columnBehavior.find(index.column());
	if (it != m_columnBehavior.end())
	{
		if (*it & Behavior::MultiSelectionEdit)
		{
			auto list = m_view->selectionModel()->selectedRows(index.column());
			for (auto& selectedRow : list)
			{
				QStyledItemDelegate::setModelData(editor, model, selectedRow);
			}
			return;
		}
	}

	QStyledItemDelegate::setModelData(editor, model, index);
}

void QAdvancedItemDelegate::RevertDragCheck(QAbstractItemModel* model)
{
	//Cancel action, example a right click has been pressed, revert all states
	for (auto& i : m_checkedIndices)
	{
		Check(model, i);
	}
}

void QAdvancedItemDelegate::CancelDragCheck()
{
	m_dragChecking = false;
	m_checkedIndices.clear();
	m_dragColumn = -1;
}

bool QAdvancedItemDelegate::IsCheckBoxCheckEvent(QEvent* event, bool& outChecked) const
{
	//Reimplemented from QStyledItemDelegate.cpp

	outChecked = false;

	// make sure that we have the right event type
	switch (event->type())
	{
	case QEvent::MouseButtonRelease:
	case QEvent::MouseButtonDblClick:
	case QEvent::MouseButtonPress:
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);

			if (me->button() != Qt::LeftButton)
				return false;

			//Item is checked on release, but handle event anyway
			if (event->type() == QEvent::MouseButtonPress)
			{
				return true;
			}
			break;
		}
	case QEvent::KeyPress:
		if (static_cast<QKeyEvent*>(event)->key() != Qt::Key_Space
		    && static_cast<QKeyEvent*>(event)->key() != Qt::Key_Select)
		{
			return false;
		}
		break;
	default:
		return false;
	}

	outChecked = true;
	return true;
}

bool QAdvancedItemDelegate::IsEnabledCheckBoxItem(QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index, QVariant& outValue) const
{
	//Reimplemented from QStyledItemDelegate.cpp

	outValue = QVariant();

	// make sure that the item is checkable
	Qt::ItemFlags flags = model->flags(index);
	if (!(flags& Qt::ItemIsUserCheckable) || !(option.state & QStyle::State_Enabled)
	    || !(flags & Qt::ItemIsEnabled))
		return false;

	// make sure that we have a check state
	outValue = index.data(Qt::CheckStateRole);
	if (!outValue.isValid())
		return false;

	return true;
}

bool QAdvancedItemDelegate::IsInCheckBoxRect(QMouseEvent* event, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	//Reimplemented from QStyledItemDelegate.cpp

	const QWidget* widget = option.widget;
	QStyle* style = widget ? widget->style() : QApplication::style();

	QStyleOptionViewItem viewOpt(option);
	initStyleOption(&viewOpt, index);
	QRect checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &viewOpt, widget);
	if (checkRect.contains(event->pos()))
	{
		return true;
	}

	return false;
}

Qt::CheckState QAdvancedItemDelegate::Check(QAbstractItemModel* model, const QModelIndex& index)
{
	Qt::CheckState returnVal;

	//Assuming the item has been verified for valid checkbox item
	Qt::ItemFlags flags = model->flags(index);
	QVariant value = index.data(Qt::CheckStateRole);

	Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
	if (flags & Qt::ItemIsUserTristate)
		returnVal = ((Qt::CheckState)((state + 1) % 3));
	else
		returnVal = (state == Qt::Checked) ? Qt::Unchecked : Qt::Checked;

	model->setData(index, returnVal, Qt::CheckStateRole);

	static QVector<int> roles(1, Qt::CheckStateRole);
	model->dataChanged(index, index, roles);

	return returnVal;
}

