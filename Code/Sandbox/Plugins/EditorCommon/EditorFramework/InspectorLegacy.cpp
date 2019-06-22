// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "InspectorLegacy.h"

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

CInspectorLegacy::CInspectorLegacy(QWidget* pParent)
	: CDockableWidget(pParent)
	, m_pScrollableBox(nullptr)
{
	CRY_ASSERT(pParent);
	Init();
	Connect(CBroadcastManager::Get(pParent));
}

CInspectorLegacy::CInspectorLegacy(CEditor* pParent)
	: m_pScrollableBox(nullptr)
{
	CRY_ASSERT(pParent);
	Init();
	Connect(&pParent->GetBroadcastManager());
}

CInspectorLegacy::CInspectorLegacy(CBroadcastManager* pBroadcastManager)
	: m_pScrollableBox(nullptr)
{
	Init();
	Connect(pBroadcastManager);
}

void CInspectorLegacy::Init()
{
	m_pLockButton = new QToolButton();
	m_pTitleLabel = new QLabel();
	QFont font = m_pTitleLabel->font();
	font.setBold(true);
	m_pTitleLabel->setFont(font);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	CLegacyInspectorHeaderWidget* pHeader = new CLegacyInspectorHeaderWidget();
	QHBoxLayout* pToolbarLayout = new QHBoxLayout(pHeader);
	pToolbarLayout->addWidget(m_pTitleLabel);
	pToolbarLayout->addWidget(pSpacer);
	pToolbarLayout->addWidget(m_pLockButton);

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(1, 1, 1, 1);
	m_pMainLayout->addWidget(pHeader);
	setLayout(m_pMainLayout);

	Unlock();
	ClearAndFillSpace();

	connect(m_pLockButton, &QPushButton::clicked, this, &CInspectorLegacy::ToggleLock);
}

CInspectorLegacy::~CInspectorLegacy()
{
	Disconnect();
	Clear();
}

void CInspectorLegacy::SetLockable(bool lockable)
{
	if (!lockable)
	{
		if (IsLocked())
		{
			Unlock();
		}

		m_pLockButton->setVisible(false);
	}
	else
	{
		m_pLockButton->setVisible(true);
	}
}

void CInspectorLegacy::OnPopulate(PopulateLegacyInspectorEvent& event)
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
		connect(m_pScrollableBox, &QObject::destroyed, this, &CInspectorLegacy::OnWidgetDeleted);
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

void CInspectorLegacy::AddWidget(QWidget* pWidget)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);
	m_pScrollableBox->addWidget(pWidget);
}

void CInspectorLegacy::closeEvent(QCloseEvent* event)
{
	Disconnect();

	event->setAccepted(true);
}

void CInspectorLegacy::Clear()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (m_pScrollableBox != nullptr)
	{
		// Remove everything but the toolbar added in Init
		QVBoxLayout* const pLayout = static_cast<QVBoxLayout*>(layout());

		disconnect(m_pScrollableBox, &QObject::destroyed, this, &CInspectorLegacy::OnWidgetDeleted);
		pLayout->removeWidget(m_pScrollableBox);
		// Note: We hide the widget to avoid redrawing elements of it while it is waiting for deletion.
		m_pScrollableBox->hide();
		m_pScrollableBox->deleteLater();
		m_pScrollableBox = nullptr;
	}
}

void CInspectorLegacy::ClearAndFillSpace()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	Clear();

	m_pScrollableBox = new QScrollableBox();
	m_pScrollableBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	connect(m_pScrollableBox, &QObject::destroyed, this, &CInspectorLegacy::OnWidgetDeleted);
	layout()->addWidget(m_pScrollableBox);
}

void CInspectorLegacy::OnWidgetDeleted(QObject* pObj)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	m_pScrollableBox = nullptr;
	ClearAndFillSpace();
	Unlock();
}

void CInspectorLegacy::Connect(CBroadcastManager* pBroadcastManager)
{
	CRY_ASSERT(pBroadcastManager);
	m_pBroadcastManager = pBroadcastManager;
	m_pBroadcastManager->Connect(BroadcastEvent::PopulateLegacyInspector, this, &CInspectorLegacy::OnPopulate);
}

void CInspectorLegacy::Disconnect()
{
	if (m_pBroadcastManager)
	{
		m_pBroadcastManager->DisconnectObject(this);
		m_pBroadcastManager = nullptr;
	}
}

void CInspectorLegacy::Lock()
{
	m_locked = true;
	m_pLockButton->setToolTip("Unlock Inspector");
	m_pLockButton->setIcon(CryIcon("icons:general_lock_true.ico"));
}

void CInspectorLegacy::Unlock()
{
	m_locked = false;
	m_pLockButton->setToolTip("Lock Inspector");
	m_pLockButton->setIcon(CryIcon("icons:general_lock_false.ico"));
}
