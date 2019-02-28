// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "qmfcviewporthost.h"
#include "QViewportPane.h"

QMfcViewportHost::QMfcViewportHost(CWnd* pParent, CViewport* pViewport, const CRect& rect)
	: m_pHost(nullptr)
{
	QViewportWidget* viewWidget = new QViewportWidget(pViewport);
	// The options header affects only the main editor viewport, so it's disabled now
	//QViewportHeader* headerWidget = new QViewportHeader(pViewport);
	//headerWidget->setContentsMargins(1, 1, 1, 1);
	viewWidget->setMinimumSize(50, 50);
	pViewport->SetViewWidget(viewWidget);
	pParent->GetSafeHwnd();
	m_pHost = new QWinWidget(pParent);

	QVBoxLayout* pLayout = new QVBoxLayout(m_pHost);
	pLayout->setContentsMargins(1, 1, 1, 1);
	//pLayout->addWidget(headerWidget);
	pLayout->addWidget(viewWidget);

	m_pHost->setLayout(pLayout);
	m_pHost->setMinimumSize(50, 50);
	SetClientRect(rect);
	m_pHost->show();
}

BEGIN_MESSAGE_MAP(QMfcContainer, CWnd)
ON_WM_SIZE()
END_MESSAGE_MAP()

void QMfcContainer::OnSize(UINT nType, int cx, int cy)
{
	if (m_pWidgetHost)
		m_pWidgetHost->setGeometry(0, 0, cx, cy);

	__super::OnSize(nType, cx, cy);
}

void QMfcContainer::SetWidgetHost(QWidget* pHost)
{
	m_pWidgetHost = pHost;
	CRect rect;
	GetClientRect(rect);
	if (m_pWidgetHost)
		m_pWidgetHost->setGeometry(rect.left, rect.top, rect.Width(), rect.Height());
}

