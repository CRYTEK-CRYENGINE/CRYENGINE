// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DrsSpokenLinesWindow.h"
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include <QBoxLayout>
#include <QTimer>
#include <QPushButton>
#include <CrySystem/ISystem.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CrySerialization/IArchive.h>

CSpokenLinesWidget::CSpokenLinesWidget()
{
	m_SerializationFilter = 0;
	m_pPropertyTree = new QPropertyTree(this);
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	m_pPropertyTree->setTreeStyle(treeStyle);

	QTimer* m_pAutoUpdateTimer = new QTimer(this);
	m_pAutoUpdateTimer->setInterval(500);
	QObject::connect(m_pAutoUpdateTimer, &QTimer::timeout, this, [ = ]
	{
		m_pPropertyTree->revert();
		m_pAutoUpdateTimer->start();
	});

	m_pUpdateButton = new QPushButton("Auto Update");
	m_pUpdateButton->setCheckable(true);
	QObject::connect(m_pUpdateButton, &QPushButton::toggled, this, [=](bool checked)
	{
		if (checked)
			m_pAutoUpdateTimer->start();
		else
			m_pAutoUpdateTimer->stop();
	});

	QPushButton* m_pClearButton = new QPushButton("Clear", this);
	QObject::connect(m_pClearButton, &QPushButton::clicked, this, [ = ]
	{
		m_SerializationFilter = 1;
		m_pPropertyTree->revert();
		m_SerializationFilter = 0;
	});

	QVBoxLayout* pVLayout = new QVBoxLayout(this);
	QHBoxLayout* pHLayout = new QHBoxLayout(this);
	pHLayout->addWidget(m_pClearButton);
	pHLayout->addWidget(m_pUpdateButton);
	pVLayout->addWidget(m_pPropertyTree);
	pVLayout->addItem(pHLayout);

	m_pPropertyTree->setExpandLevels(1);
	m_pPropertyTree->attach(Serialization::SStruct(*this));
}

//--------------------------------------------------------------------------------------------------
void CSpokenLinesWidget::SetAutoUpdateActive(bool bValue)
{
	m_pUpdateButton->setChecked(bValue);
}

//--------------------------------------------------------------------------------------------------
void CSpokenLinesWidget::Serialize(Serialization::IArchive& ar)
{
	ar.setFilter(m_SerializationFilter);
	gEnv->pDynamicResponseSystem->GetDialogLineDatabase()->SerializeLinesHistory(ar);
}

