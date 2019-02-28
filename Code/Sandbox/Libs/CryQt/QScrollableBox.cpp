// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "QScrollableBox.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSpacerItem>
#include <QSizePolicy>
#include <Qt>

QScrollableBox::QScrollableBox(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->setMargin(0);
	m_scrollArea = new QScrollArea(this);
	QSizePolicy sizePolicy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	m_scrollArea->setSizePolicy(sizePolicy);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scrollArea->setWidgetResizable(true);

	mainLayout->addWidget(m_scrollArea);
	setLayout(mainLayout);
	QWidget* scrollContents = new QWidget;
	m_layout = new QVBoxLayout;
	m_layout->setMargin(0);//old was 1
	m_layout->setSizeConstraint(QLayout::SetNoConstraint);
	scrollContents->setLayout(m_layout);

	m_scrollArea->setWidget(scrollContents);
}

void QScrollableBox::addWidget(QWidget* w)
{
	if (!w)
		return;

	// Remove last spacer item if present.
	int count = m_layout->count();
	if (count > 1)
	{
		m_layout->removeItem(m_layout->itemAt(count - 1));
	}
	// Add item and make sure it stretches the remaining space.
	m_layout->addWidget(w);
	w->show();
	m_layout->addStretch();
	m_scrollArea->update();
}

void QScrollableBox::removeWidget(QWidget* w)
{
	m_layout->removeWidget(w);
	m_scrollArea->update();
}

void QScrollableBox::insertWidget(int i, QWidget * widget)
{
	m_layout->insertWidget(i, widget);
	m_scrollArea->update();
}

void QScrollableBox::clearWidgets()
{
	QLayoutItem* item;
	while ((item = m_layout->takeAt(0)) != nullptr)
	{
		item->widget()->deleteLater();
		m_layout->removeItem(item);
		delete item;
	}

	m_scrollArea->update();
}

int QScrollableBox::indexOf(QWidget* w)
{
	return m_layout->indexOf(w);
}

