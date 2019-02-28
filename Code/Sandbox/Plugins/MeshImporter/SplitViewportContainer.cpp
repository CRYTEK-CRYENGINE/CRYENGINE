// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Viewport.h"
#include "DisplayOptions.h"

#include <QCheckBox>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>

CSplitViewportContainer::CSplitViewportContainer(QWidget* pParent)
	: QWidget(pParent)
	, m_pSplitViewport(nullptr)
	, m_pDisplayOptionsWidget(nullptr)
	, m_pHeader(nullptr)
{

	m_pSplitViewport = new CSplitViewport(this);
	m_pSplitViewport->setMinimumSize(QSize(100, 100));
	m_pSplitViewport->setStatusTip(tr("Preview of the resulting mesh"));

	m_pDisplayOptionsWidget = new CDisplayOptionsWidget();
	m_pDisplayOptionsWidget->hide();

	auto applyDisplayOptions = [this]()
	{
		const SDisplayOptions& options = m_pDisplayOptionsWidget->GetSettings();

		m_pSplitViewport->GetPrimaryViewport()->SetSettings(options.viewport);
		m_pSplitViewport->GetSecondaryViewport()->SetSettings(options.viewport);
	};
	connect(m_pDisplayOptionsWidget, &CDisplayOptionsWidget::SigChanged, applyDisplayOptions);
	applyDisplayOptions();

	QToolButton* pToggleDisplayOptionsButton = new QToolButton();
	pToggleDisplayOptionsButton->setText(tr("Display"));
	pToggleDisplayOptionsButton->setCheckable(true);

	connect(
	  pToggleDisplayOptionsButton, &QToolButton::toggled,
	  m_pDisplayOptionsWidget, &QWidget::setVisible);

	m_pHeaderLayout = new QHBoxLayout();
	m_pHeaderLayout->setContentsMargins(0, 0, 0, 0);
	// TODO: Use QLayout::addSeparator, or use extra separation widget.
	m_pHeaderLayout->addStretch(100);
	m_pHeaderLayout->addWidget(pToggleDisplayOptionsButton);

	QSplitter* const pSplitter = new QSplitter();
	pSplitter->setContentsMargins(0, 0, 0, 0);
	pSplitter->addWidget(m_pSplitViewport);
	pSplitter->addWidget(m_pDisplayOptionsWidget);
	pSplitter->setStretchFactor(0, 1);
	pSplitter->setStretchFactor(1, 0);
	QList<int> sizes;
	sizes.push_back(kDefaultWidthOfDisplayOptionWidget * 2);
	sizes.push_back(kDefaultWidthOfDisplayOptionWidget);
	pSplitter->setSizes(sizes);

	QVBoxLayout* const pVBox = new QVBoxLayout();
	pVBox->setContentsMargins(0, 0, 0, 0);
	pVBox->addLayout(m_pHeaderLayout);
	pVBox->addWidget(pSplitter);

	setLayout(pVBox);
}

void CSplitViewportContainer::SetHeaderWidget(QWidget* pHeader)
{
	if (m_pHeader != pHeader)
	{
		if (m_pHeader)
		{
			m_pHeaderLayout->removeWidget(m_pHeader);
			m_pHeader = nullptr;
		}
		if (pHeader)
		{
			m_pHeader = pHeader;
			m_pHeaderLayout->insertWidget(0, m_pHeader);
		}
	}
}

CDisplayOptionsWidget* CSplitViewportContainer::GetDisplayOptionsWidget()
{
	return m_pDisplayOptionsWidget;
}

const SDisplayOptions& CSplitViewportContainer::GetDisplayOptions() const
{
	return m_pDisplayOptionsWidget->GetSettings();
}

CSplitViewport* CSplitViewportContainer::GetSplitViewport()
{
	return m_pSplitViewport;
}

QSize CSplitViewportContainer::minimumSizeHint() const
{
	return QSize(0, 0);
}

