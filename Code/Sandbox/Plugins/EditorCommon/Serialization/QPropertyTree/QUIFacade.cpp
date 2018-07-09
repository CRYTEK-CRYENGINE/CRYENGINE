// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QUIFacade.h"
#include <QFont>
#include <QFontMetrics>
#include <QStyleOption>
#include <QDesktopWidget>
#include <QApplication>
#include "QPropertyTree.h"
#include "QDrawContext.h"

#include "InplaceWidgetComboBox.h"
#include "InplaceWidgetNumber.h"
#include "InplaceWidgetString.h"

QCursor translateCursor(property_tree::Cursor cursor);

namespace property_tree {

// ---------------------------------------------------------------------------

int QUIFacade::textWidth(const char* text, Font font)
{
	const QFont* qfont = font == FONT_BOLD ? &tree_->boldFont() : &tree_->font();

	QFontMetrics fm(*qfont);
	return fm.width(text);
}

int QUIFacade::textHeight(int width, const char* text, Font font)
{
	const QFont* qfont = font == FONT_BOLD ? &tree_->boldFont() : &tree_->font();
	QFontMetrics fm(*qfont);
	return fm.boundingRect(0, 0, width, 0, Qt::TextWordWrap|Qt::AlignTop, QString::fromUtf8(text)).height();
}

Point QUIFacade::screenSize()
{
	QSize s = QApplication::desktop()->screenGeometry(tree_).size();
	return Point(s.width(), s.height());
}

IMenu* QUIFacade::createMenu()
{
	return new QtMenu(new QPropertyTreeMenu(nullptr), tree_, "");
}

void QUIFacade::setCursor(Cursor cursor)
{

	tree_->setCursor(translateCursor(cursor));
}

void QUIFacade::unsetCursor()
{
	tree_->unsetCursor();
}

Point QUIFacade::cursorPosition()
{
	return fromQPoint(tree_->mapFromGlobal(QCursor::pos()));
}

QWidget* QUIFacade::qwidget()
{
	return tree_;
}

HWND QUIFacade::hwnd()
{
	return (HWND)tree_->winId();
}

InplaceWidget* QUIFacade::createComboBox(ComboBoxClientRow* client) { return new InplaceWidgetComboBox(client, tree_); }
InplaceWidget* QUIFacade::createNumberWidget(PropertyRowNumberField* row) { return new InplaceWidgetNumber(row, tree_); }
InplaceWidget* QUIFacade::createStringWidget(PropertyRowString* row) { return new InplaceWidgetString(row, tree_); }

}

