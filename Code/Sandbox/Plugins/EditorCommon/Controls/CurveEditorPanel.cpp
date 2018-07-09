// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CurveEditorPanel.h"

#include "EditorFramework/Events.h"
#include "EditorFramework/BroadcastManager.h"

#include "CurveEditor.h"

#include <QLabel>
#include <QToolbar>
#include <QVBoxLayout>

CCurveEditorPanel::CCurveEditorPanel(QWidget* pParent /* = nullptr */)
	: CDockableWidget(pParent)
{
	m_pTitle = new QLabel();
	auto font = m_pTitle->font();
	font.setBold(true);
	m_pTitle->setFont(font);

	SetTitle(nullptr);

	const auto pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	QVBoxLayout* const pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(1, 1, 1, 1);
	pLayout->setSpacing(0);

	QToolBar* const pHeaderBar = new QToolBar();
	pHeaderBar->addWidget(m_pTitle);
	pHeaderBar->addWidget(pSpacer);
	pLayout->addWidget(pHeaderBar);

	QToolBar* pToolbar = new QToolBar(this);
	pLayout->addWidget(pToolbar);

	m_pEditor = new CCurveEditor(this);
	m_pEditor->FillWithCurveToolsAndConnect(pToolbar);
	m_pEditor->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	pLayout->addWidget(m_pEditor);

	setParent(pParent);
	setLayout(pLayout);

	CRY_ASSERT(pParent); //must have a valid parent in order to connect to its broadcast manager
	ConnectToBroadcast(CBroadcastManager::Get(pParent));
}

CCurveEditorPanel::~CCurveEditorPanel()
{
	Disconnect();
}

void CCurveEditorPanel::SetTitle(const char* szTitle)
{
	if (szTitle && szTitle[0])
	{
		QString title = QString("Curve Editor: ") + szTitle;
		m_pTitle->setText(title);
	}
	else
	{
		m_pTitle->setText("Curve Editor");
	}
}

SCurveEditorContent* CCurveEditorPanel::GetEditorContent() const
{
	if (m_pEditor)
		return m_pEditor->Content();
	return nullptr;
}

void CCurveEditorPanel::SetEditorContent(SCurveEditorContent* pContent)
{
	if (SCurveEditorContent* pCurrentContent = GetEditorContent())
	{
		QObject::disconnect(this);
	}

	if (pContent)
	{
		m_pEditor->SetContent(pContent);
		if (pContent->m_curves.size() > 0)
		{
			m_pEditor->SetPriorityCurve(pContent->m_curves.front());
		}
		m_pEditor->OnFitCurvesVertically();
	}
	else
	{
		SetTitle(nullptr);
		m_pEditor->SetContent(nullptr);
	}
	update();
}

void CCurveEditorPanel::OnPopulate(BroadcastEvent& event)
{
	CRY_ASSERT(event.type() == BroadcastEvent::EditCurve);

	const EditCurveEvent& editCurveEvent = static_cast<const EditCurveEvent&>(event);

	editCurveEvent.SetupEditor(*this);
	if (SCurveEditorContent* pContent = GetEditorContent())
	{
		QObject::connect(pContent, &QObject::destroyed, this, &CCurveEditorPanel::OnContentDestroyed);
	}
}

void CCurveEditorPanel::ConnectToBroadcast(CBroadcastManager* pBroadcastManager)
{
	CRY_ASSERT(pBroadcastManager);
	m_broadcastManager = pBroadcastManager;
	pBroadcastManager->Connect(BroadcastEvent::EditCurve, this, &CCurveEditorPanel::OnPopulate);
}

void CCurveEditorPanel::Disconnect()
{
	if (m_broadcastManager)
	{
		m_broadcastManager->DisconnectObject(this);
	}

	SetEditorContent(nullptr);
}

void CCurveEditorPanel::OnContentDestroyed(QObject* pObject)
{
	SetEditorContent(nullptr);
}

