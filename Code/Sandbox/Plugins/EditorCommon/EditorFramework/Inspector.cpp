// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Inspector.h"

#include "CryIcon.h"
#include "EditorFramework/BroadcastManager.h"
#include "EditorFramework/Editor.h"
#include "EditorFramework/Events.h"
#include "QScrollableBox.h"
#include "QtViewPane.h"
#include "Serialization\QPropertyTree\PropertyTree.h"

#include <QApplication>
#include <QCloseEvent>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSpacerItem>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>

CInspector::CInspector(QWidget* pParent)
	: CDockableWidget(pParent)
	, m_pOwnedPropertyTree(nullptr)
{
	CRY_ASSERT(pParent);
	Init();
	Connect(CBroadcastManager::Get(pParent));
}

CInspector::CInspector(CEditor* pParent)
	: m_pOwnedPropertyTree(nullptr)
{
	CRY_ASSERT(pParent);
	Init();
	Connect(&pParent->GetBroadcastManager());
}

CInspector::CInspector(CBroadcastManager* pBroadcastManager)
	: m_pOwnedPropertyTree(nullptr)
{
	Init();
	Connect(pBroadcastManager);
}

void CInspector::Init()
{
	m_pLockButton = new QToolButton();
	m_pTitleLabel = new QLabel();
	QFont font = m_pTitleLabel->font();
	font.setBold(true);
	m_pTitleLabel->setFont(font);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	CInspectorHeaderWidget* pHeader = new CInspectorHeaderWidget();
	QHBoxLayout* pToolbarLayout = new QHBoxLayout(pHeader);
	pToolbarLayout->addWidget(m_pTitleLabel);
	pToolbarLayout->addWidget(pSpacer);
	pToolbarLayout->addWidget(m_pLockButton);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(1, 1, 1, 1);
	pLayout->addWidget(pHeader);
	setLayout(pLayout);

	m_pWidgetLayout = new QVBoxLayout();
	pLayout->addLayout(m_pWidgetLayout);

	Unlock();

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

void CInspector::OnPopulate(PopulateInspectorEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	// Skip adding if we locked to the previous selection
	if (IsLocked())
	{
		return;
	}

	Clear();

	setUpdatesEnabled(false);

	// Trigger creation of widgets handled by whoever invoked the populate event
	event.GetCallback()(*this);

	// Set the final title
	m_pTitleLabel->setText(event.GetTitle());

	setUpdatesEnabled(true);
}

void CInspector::AddPropertyTree(QPropertyTree* pPropertyTree)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);
	//this inspector supports only one widget at time, clear the previous widget if we add a new one
	Clear();
	m_pWidgetLayout->addWidget(pPropertyTree);
	m_pOwnedPropertyTree = pPropertyTree;
}

void CInspector::closeEvent(QCloseEvent* event)
{
	Disconnect();
	event->setAccepted(true);
}

void CInspector::Clear()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (m_pOwnedPropertyTree)
	{
		m_pOwnedPropertyTree->deleteLater();
	}

	m_pOwnedPropertyTree = nullptr;

	return;
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
	m_isLocked = true;
	m_pLockButton->setToolTip("Unlock Inspector");
	m_pLockButton->setIcon(CryIcon("icons:general_lock_true.ico"));
}

void CInspector::Unlock()
{
	m_isLocked = false;
	m_pLockButton->setToolTip("Lock Inspector");
	m_pLockButton->setIcon(CryIcon("icons:general_lock_false.ico"));
}
