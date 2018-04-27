// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <IEditor.h>
#include <IPlugin.h>

#include <QAction>
#include <QFileInfo>
#include <QToolBar>

#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>
#include <EditorFramework/Events.h>
#include "CryIcon.h"
#include "EditorEnvironmentWindow.h"
#include "IUndoManager.h"
#include "Serialization.h"
#include "QPresetsWidget.h"
#include "QTimeOfDayWidget.h"

REGISTER_VIEWPANE_FACTORY(CEditorEnvironmentWindow, "Environment Editor", "Tools", true);

CEditorEnvironmentWindow::CEditorEnvironmentWindow()
	: CDockableEditor(nullptr)
	, m_presetsWidget(new QPresetsWidget)
	, m_timeOfDayWidget(new QTimeOfDayWidget)
{
	auto* topLayout = new QVBoxLayout;
	topLayout->setMargin(0);
	topLayout->addWidget(CreateToolbar());

	auto* centerLayout = new QHBoxLayout;

	QSplitter* splitter = new QSplitter(Qt::Orientation::Horizontal, this);
	splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	splitter->setChildrenCollapsible(false);
	splitter->addWidget(m_presetsWidget);
	splitter->addWidget(m_timeOfDayWidget);

	centerLayout->addWidget(splitter);

	topLayout->addLayout(centerLayout);

	SetContent(topLayout);

	RestorePersonalizationState();

	connect(m_presetsWidget, &QPresetsWidget::SignalCurrentPresetChanged, [&]() { m_timeOfDayWidget->Refresh(); });
	GetIEditor()->RegisterNotifyListener(this);
}

CEditorEnvironmentWindow::~CEditorEnvironmentWindow()
{
	GetIEditor()->UnregisterNotifyListener(this);

	SavePersonalizationState();
}

QWidget* CEditorEnvironmentWindow::CreateToolbar()
{
	QAction* pUndo = GetIEditor()->GetICommandManager()->GetAction("general.undo");
	QAction* pRedo = GetIEditor()->GetICommandManager()->GetAction("general.redo");

	QAction* pRefresh = new QAction(CryIcon("icons:General/Reload.ico"), ("Refresh"), this);
	connect(pRefresh, &QAction::triggered, [&]() { m_presetsWidget->RefreshPresetList(); });

	QAction* pSaveAll = new QAction(CryIcon("icons:General/File_Save.ico"), ("Save all"), this);
	connect(pSaveAll, &QAction::triggered, [&]() { m_presetsWidget->SaveAllPresets(); });

	QToolBar* pToolbar = new QToolBar(this);
	pToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	pToolbar->addAction(pUndo);
	pToolbar->addAction(pRedo);
	pToolbar->addSeparator();
	pToolbar->addAction(pRefresh);
	pToolbar->addSeparator();
	pToolbar->addAction(pSaveAll);

	return pToolbar;
}

void CEditorEnvironmentWindow::RestorePersonalizationState()
{
	const QVariant& var = GetIEditor()->GetPersonalizationManager()->GetProperty(GetEditorName(), "TimeOfDayWidgetState");
	if (var.isValid() && var.type() == QVariant::Map)
	{
		m_timeOfDayWidget->SetPersonalizationState(var.value<QVariantMap>());
	}
}

void CEditorEnvironmentWindow::SavePersonalizationState() const
{
	const QVariant state = m_timeOfDayWidget->GetPersonalizationState();
	GetIEditor()->GetPersonalizationManager()->SetProperty(GetEditorName(), "TimeOfDayWidgetState", state);
}

void CEditorEnvironmentWindow::customEvent(QEvent* event)
{
	// TODO: This handler should be removed whenever this editor is refactored to be a CDockableEditor
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		const string& command = commandEvent->GetCommand();
		if (command == "general.help")
		{
			event->setAccepted(EditorUtils::OpenHelpPage(GetPaneTitle()));
		}
	}

	if (!event->isAccepted())
	{
		QWidget::customEvent(event);
	}
}

void CEditorEnvironmentWindow::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnIdleUpdate)
	{
		m_timeOfDayWidget->OnIdleUpdate();
	}
	else if (event == eNotify_OnEndSceneOpen || event == eNotify_OnEndNewScene)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CEditorEnvironmentWindow::OnEditorNotifyEvent OnEndSceneOpen/NewScene");
		m_presetsWidget->OnNewScene();
	}
	else if (event == eNotify_OnEndSceneSave)
	{
		m_presetsWidget->SaveAllPresets();
	}
	else if (event == eNotify_OnStyleChanged)
	{
		// refresh curves color by updating content
		m_timeOfDayWidget->UpdateCurveContent();
	}
}
