// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QToolWindowTabBar.h"
#include <QToolButton>
#include <CryIcon.h>
#include <QAction>
#include <QStylePainter>
#include <QMenu>
#include <QSignalMapper>
#include <QPaintEvent>

QToolWindowTabBar::QToolWindowTabBar(QWidget* parent)
	: QTabBar(parent)
#if QT_VERSION <= 0x050000
	, bAutoHide(false)
#endif
{
	m_tabSelectionButton = new QToolButton(this);
	m_tabSelectionMenu = new QTabSelectionMenu(m_tabSelectionButton, this);

	m_tabSelectionButton->setEnabled(true);
	m_tabSelectionButton->setIcon(CryIcon("icons:General/Pointer_Down_Expanded.ico"));
	m_tabSelectionButton->setMenu(m_tabSelectionMenu);
	m_tabSelectionButton->setPopupMode(QToolButton::InstantPopup);
	QString styleSheet = QString("QToolWindowArea > QTabBar::scroller{	width: %1px;}").arg(m_tabSelectionButton->sizeHint().width()/2);//use half the size of the button for scrollers (Because size is used for each of the scroller
	setStyleSheet(styleSheet);
	connect(m_tabSelectionMenu, &QMenu::aboutToShow, this, &QToolWindowTabBar::onSelectionMenuClicked);
}

void QToolWindowTabBar::paintEvent(QPaintEvent *e)
{
	QTabBar::paintEvent(e);
	QRect tabbarRect = rect();
	//Only draw button, if the first or last tab is not fully shown
	if (!tabbarRect.contains(tabRect(count() - 1)) || !tabbarRect.contains(tabRect(0)))
	{
		m_tabSelectionButton->show();
		m_tabSelectionButton->raise();
		QRect rect = contentsRect();
		QSize size = m_tabSelectionButton->sizeHint();
		m_tabSelectionButton->move(QPoint(rect.width()-m_tabSelectionButton->width(), 0));
	}
	else
	{
		m_tabSelectionButton->hide();
	}
}

void QToolWindowTabBar::onSelectionMenuClicked()
{
	QSignalMapper* signalMapper = new QSignalMapper(this);
	QList<QAction*> acts = m_tabSelectionMenu->actions();
	int activeAction = currentIndex();

	for (int i = 0; i < acts.size(); i++)
	{
		m_tabSelectionMenu->removeAction(acts[i]);
	}

	for (int i = 0; i < count(); i++)
	{
		QAction* action = m_tabSelectionMenu->addAction(tabText(i));
		connect(action, SIGNAL(triggered()), signalMapper, SLOT(map()));
		signalMapper->setMapping(action, i);
	}
	if (activeAction >= 0)
	{
		QList<QAction*> acts = m_tabSelectionMenu->actions();
		acts[activeAction]->setIcon(CryIcon("icons:General/Tick.ico"));
	}
	connect(signalMapper, SIGNAL(mapped(int)), this, SLOT(setCurrentIndex(int)));
}

QToolWindowTabBar::~QToolWindowTabBar()
{

}

QTabSelectionMenu::QTabSelectionMenu(QToolButton* pToolButton, QToolWindowTabBar* pParentTabbar)
	: QMenu(pToolButton)
	, m_pParentTabbar(pParentTabbar)
{
};
