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
	QVBoxLayout* const pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(1, 1, 1, 1);
	pLayout->setSpacing(0);
	
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
	setWindowTitle("Curve Editor");

	if (szTitle && szTitle[0])
	{
		setWindowTitle(QString("Curve Editor: %1").arg(szTitle));
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
