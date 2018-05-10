// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QMainFrameMenuBar.h"

// Qt
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStyleOption>
#include <QPainter>
#include <QStandardItemModel>

//Editor
#include "Commands/CommandModel.h"
#include "Commands/QCommandAction.h"
#include "QtUtil.h"

QMainFrameMenuBar::QMainFrameMenuBar(QMenuBar* pMenuBar /* = 0*/, QWidget* pParent /* = 0*/)
	: QWidget(pParent)
	, m_pMenuBar(pMenuBar)
{
	QHBoxLayout* pMainLayout = new QHBoxLayout();
	pMainLayout->setMargin(0);
	pMainLayout->setSpacing(0);

	if (pMenuBar)
	{
		pMainLayout->addWidget(pMenuBar);
	}

	// Add tray area to the menu bar
	pMainLayout->addWidget(GetIEditorImpl()->GetTrayArea());

	setLayout(pMainLayout);
}

void QMainFrameMenuBar::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

QStandardItemModel* QMainFrameMenuBar::CreateMainMenuModel()
{
	QStandardItemModel* pModel = new QStandardItemModel();
	QStringList headerLabels;
	headerLabels.push_back("Name");
	headerLabels.push_back("Path");
	pModel->setHorizontalHeaderLabels(headerLabels);

	QList<QMenu*> menus = m_pMenuBar->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);

	// Must go through menus because actions may have been created with a parent other than the menu that holds it.
	// In fact it may be attached to several different widgets we don't really care about, so the only
	// way is to traverse the menus in the main menu bar.
	for (QMenu* pMenu : menus)
	{
		AddActionsToModel(pMenu, pModel);
	}

	return pModel;
}

void QMainFrameMenuBar::AddActionsToModel(QMenu* pParentMenu, QStandardItemModel* pModel)
{
	QList<QMenu*> menus = pParentMenu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);
	QList<QAction*> actions = pParentMenu->actions();

	// Go through all child menus as well
	for (QMenu* pMenu : menus)
	{
		AddActionsToModel(pMenu, pModel);
	}

	for (QAction* pAction : actions)
	{
		// Don't add actions that are actually menus, or if they have empty text
		if (pAction->menu() || pAction->text().isEmpty())
			continue;

		QMenu* pParent = pParentMenu;
		QString path(pParent->title()); // Figure out the path to the action
		while (pParent = qobject_cast<QMenu*>(pParent->parent()))
		{
			path.prepend(pParent->title() + QChar(8594)); // Unicode character for arrow right
		}

		QStandardItem* pNameItem = new QStandardItem(pAction->text().remove("&"));
		pNameItem->setEditable(false);
		pNameItem->setToolTip(pAction->text().remove("&"));
		pNameItem->setIcon(QIcon(pAction->icon().pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off)));
		pNameItem->setData(QVariant::fromValue(pAction));
		pModel->appendRow(pNameItem);

		QStandardItem* pPathItem = new QStandardItem(path.remove("&"));
		pPathItem->setData(QVariant::fromValue(pAction));
		pModel->setItem(pModel->rowCount() - 1, 1, pPathItem);
	}
}

