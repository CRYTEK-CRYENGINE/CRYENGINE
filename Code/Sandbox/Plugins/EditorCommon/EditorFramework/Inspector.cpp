// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Inspector.h"

#include <QApplication>
#include <QLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>
#include <QToolBar>
#include <QToolButton>
#include <QCloseEvent>
#include "QScrollableBox.h"

#include "CryIcon.h"
#include "QtViewPane.h"
#include "EditorFramework/Editor.h"
#include "EditorFramework/Events.h"
#include "EditorFramework/BroadcastManager.h"

CInspector::CInspector(QWidget* pParent)
	: CDockableWidget(pParent)
{
	CRY_ASSERT(pParent);
	Init();
	Connect(CBroadcastManager::Get(pParent));
}

CInspector::CInspector(CEditor* pParent)
{
	CRY_ASSERT(pParent);
	Init();
	Connect(&pParent->GetBroadcastManager());
}

CInspector::CInspector(CBroadcastManager* pBroadcastManager)
{
	Init();
	Connect(pBroadcastManager);
}

void CInspector::Init()
{
	m_pLockButton = new QToolButton();
	m_pTitleLabel = new QLabel();
	auto font = m_pTitleLabel->font();
	font.setBold(true);
	m_pTitleLabel->setFont(font);

	const auto pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	CInspectorHeaderWidget* pHeader = new CInspectorHeaderWidget();
	auto toolbarLayout = new QHBoxLayout(pHeader);
	toolbarLayout->addWidget(m_pTitleLabel);
	toolbarLayout->addWidget(pSpacer);
	toolbarLayout->addWidget(m_pLockButton);

	QVBoxLayout* const pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(1, 1, 1, 1);
	pLayout->addWidget(pHeader);
	setLayout(pLayout);

	Unlock();
	ClearAndFillSpace();

	connect(m_pLockButton, &QPushButton::clicked, this, &CInspector::ToggleLock);
}

CInspector::~CInspector()
{
	Disconnect();
	Clear();
}

void CInspector::SetLockable(bool bLockable)
{
	if (!bLockable)
	{
		if(IsLocked())
			Unlock();
		m_pLockButton->setVisible(false);
	}
	else
	{
		m_pLockButton->setVisible(true);
	}
}

void CInspector::OnPopulate(PopulateInspectorEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	// Skip adding if we locked to the previous selection
	if (IsLocked())
	{
		return;
	}

	setUpdatesEnabled(false);

	if (m_pScrollableBox == nullptr)
	{
		m_pScrollableBox = new QScrollableBox();
		connect(m_pScrollableBox, &QObject::destroyed, this, &CInspector::OnWidgetDeleted);
	}
	else
	{
		m_pScrollableBox->clearWidgets();
	}

	// Trigger creation of widgets handled by whoever invoked the populate event
	event.GetCallback()(*this);

	layout()->addWidget(m_pScrollableBox);

	// Set the final title
	m_pTitleLabel->setText(event.GetTitle());

	setUpdatesEnabled(true);
}

void CInspector::AddWidget(QWidget* pWidget)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);
	m_pScrollableBox->addWidget(pWidget);
}

void CInspector::closeEvent(QCloseEvent* event)
{
	Disconnect();

	event->setAccepted(true);
}

void CInspector::Clear()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (m_pScrollableBox != nullptr)
	{
		// Remove everything but the toolbar added in Init
		QVBoxLayout* const pLayout = static_cast<QVBoxLayout*>(layout());

		disconnect(m_pScrollableBox, &QObject::destroyed, this, &CInspector::OnWidgetDeleted);
		pLayout->removeWidget(m_pScrollableBox);
		// Note: We hide the widget to avoid redrawing elements of it while it is waiting for deletion.
		m_pScrollableBox->hide();
		m_pScrollableBox->deleteLater();
		m_pScrollableBox = nullptr;
	}
}

void CInspector::ClearAndFillSpace()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	Clear();

	m_pScrollableBox = new QScrollableBox();
	m_pScrollableBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	connect(m_pScrollableBox, &QObject::destroyed, this, &CInspector::OnWidgetDeleted);
	layout()->addWidget(m_pScrollableBox);
}

void CInspector::OnWidgetDeleted(QObject* pObj)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	m_pScrollableBox = nullptr;
	ClearAndFillSpace();
	Unlock();
}

void CInspector::Connect(CBroadcastManager* pBroadcastManager)
{
	CRY_ASSERT(pBroadcastManager);
	m_pBroadcastManager = pBroadcastManager;
	m_pBroadcastManager->Connect(BroadcastEvent::PopulateInspector, this, &CInspector::OnPopulate);
}

void CInspector::Disconnect()
{
	if (m_pBroadcastManager)
	{
		m_pBroadcastManager->DisconnectObject(this);
		m_pBroadcastManager = nullptr;
	}
}

void CInspector::Lock()
{
	m_bLocked = true;
	m_pLockButton->setToolTip("Unlock Inspector");
	m_pLockButton->setIcon(CryIcon("icons:General/Lock_True.ico"));
}

void CInspector::Unlock()
{
	m_bLocked = false;
	m_pLockButton->setToolTip("Lock Inspector");
	m_pLockButton->setIcon(CryIcon("icons:General/Lock_False.ico"));
}

