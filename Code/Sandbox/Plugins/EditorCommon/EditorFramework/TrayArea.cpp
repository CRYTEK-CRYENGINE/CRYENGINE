// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "TrayArea.h"

#include <QDynamicPropertyChangeEvent>
#include <QHBoxLayout>
#include <QVariant>

CTrayArea::CTrayArea(QWidget* pParent /* = nullptr*/)
	: QWidget(pParent)
{
	m_pMainLayout = new QHBoxLayout();

	m_pMainLayout->setMargin(0);
	m_pMainLayout->setSpacing(0);
	m_pMainLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

	setLayout(m_pMainLayout);
}

CTrayArea::~CTrayArea()
{
	for (auto i = 0; i < m_trayObjects.size(); ++i)
	{
		delete m_trayObjects[i];
	}
}

void CTrayArea::AddTrayObject(CTrayObject* pTrayObject)
{
	m_trayObjects.push_back(pTrayObject);
	ReorderTrayObjects();
}

void CTrayArea::ReorderTrayObjects()
{
	std::sort(m_trayObjects.begin(), m_trayObjects.end(), [](const CTrayObject* pFirst, const CTrayObject* pSecond)
	{
		return pFirst->GetPriority() < pSecond->GetPriority();
	});

	for (auto i = 0; i < m_trayObjects.size(); ++i)
	{
		m_pMainLayout->removeWidget(m_trayObjects[i]->GetWidget());
		m_pMainLayout->addWidget(m_trayObjects[i]->GetWidget(), Qt::AlignRight);
	}
}

