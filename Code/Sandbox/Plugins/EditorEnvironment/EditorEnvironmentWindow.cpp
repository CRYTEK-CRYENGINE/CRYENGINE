// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <IEditor.h>
#include <IPlugin.h>

#include <QLayout>
#include <QAction>
#include <QToolBar>
#include <QStyle>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>

#include <EditorFramework/Events.h>

#include "EditorEnvironmentWindow.h"
#include "QTimeOfDayWidget.h"
#include "QPresetsWidget.h"
#include "QSettingsWidget.h"
#include "CryIcon.h"
#include "IUndoManager.h"

REGISTER_VIEWPANE_FACTORY(CEditorEnvironmentWindow, "Environment Editor", "Tools", false);
//////////////////////////////////////////////////////////////////////////

CEditorEnvironmentWindow::CEditorEnvironmentWindow()
	: m_presetsWidget(nullptr)
	, m_timeOfDayWidget(nullptr)
	, m_sunSettingsWidget(nullptr)
{

	QWidget* centralWidget = new QWidget;
	QHBoxLayout* centralLayout = new QHBoxLayout;
	centralWidget->setLayout(centralLayout);
	centralLayout->setContentsMargins(0, 0, 0, 0);

	QPresetsWidget* presetsWidget = new QPresetsWidget;
	m_presetsWidget = presetsWidget;

	QMenu* pView = menuBar()->addMenu("&View");

	{
		QWidget* pPresetsContainer = new QWidget();
		QBoxLayout* pPresetsContainerLayout = new QBoxLayout(QBoxLayout::TopToBottom);
		pPresetsContainerLayout->setContentsMargins(0, 0, 0, 0);

		QTabWidget* presetsTabs = new QTabWidget();
		presetsTabs->addTab(presetsWidget, "File view");

		pPresetsContainerLayout->addWidget(presetsTabs);
		pPresetsContainer->setLayout(pPresetsContainerLayout);

		m_pPresetsDock = new QDockWidget("Presets");
		m_pPresetsDock->setWidget(pPresetsContainer);
		addDockWidget(Qt::LeftDockWidgetArea, m_pPresetsDock);

		QAction* pTogglePresets = new QAction("&Presets", this);
		pTogglePresets->setCheckable(true);
		pTogglePresets->setChecked(true);
		pView->addAction(pTogglePresets);
		connect(pTogglePresets, &QAction::triggered, [=]() { m_pPresetsDock->setVisible(!m_pPresetsDock->isVisible()); });
		connect(m_pPresetsDock, &QDockWidget::visibilityChanged, pTogglePresets, &QAction::setChecked);
	}

	{
		QSunSettingsWidget* pSunSettingsWidget = new QSunSettingsWidget;
		m_sunSettingsWidget = pSunSettingsWidget;

		m_pSunSettingsDock = new QDockWidget("Sun settings");
		m_pSunSettingsDock->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
		m_pSunSettingsDock->setContentsMargins(0, 0, 0, 0);
		m_pSunSettingsDock->setWidget(pSunSettingsWidget);
		addDockWidget(Qt::LeftDockWidgetArea, m_pSunSettingsDock);

		QAction* pToggleSunSettings = new QAction("&Sun settings", this);
		pToggleSunSettings->setCheckable(true);
		pToggleSunSettings->setChecked(true);
		pView->addAction(pToggleSunSettings);
		connect(pToggleSunSettings, &QAction::triggered, [=]() { m_pSunSettingsDock->setVisible(!m_pSunSettingsDock->isVisible()); });
		connect(m_pSunSettingsDock, &QDockWidget::visibilityChanged, pToggleSunSettings, &QAction::setChecked);
	}

	QTimeOfDayWidget* pToDWidget = new QTimeOfDayWidget;
	centralLayout->addWidget(pToDWidget);
	m_timeOfDayWidget = pToDWidget;

	setCentralWidget(centralWidget);

	connect(presetsWidget, &QPresetsWidget::SignalCurrentPresetChanged, [pToDWidget](){ pToDWidget->Refresh(); });

	{
		QAction* pUndo = new QAction(CryIcon("icons:General/History_Undo.ico"), ("Undo"), this);
		pUndo->setShortcuts(QKeySequence::Undo);
		connect(pUndo, &QAction::triggered, [](){ GetIEditor()->GetIUndoManager()->Undo(); });

		QAction* pRedo = new QAction(CryIcon("icons:General/History_Redo.ico"), ("Redo"), this);
		pRedo->setShortcuts(QKeySequence::Redo);
		connect(pRedo, &QAction::triggered, [](){ GetIEditor()->GetIUndoManager()->Redo(); });

		QAction* pRefresh = new QAction(CryIcon("icons:General/Reload.ico"), ("Refresh"), this);
		connect(pRefresh, &QAction::triggered, [presetsWidget](){ presetsWidget->RefreshPresetList(); });

		QAction* pSaveAll = new QAction(CryIcon("icons:General/File_Save.ico"), ("Save all"), this);
		connect(pSaveAll, &QAction::triggered, [presetsWidget](){ presetsWidget->SaveAllPresets(); });

		QToolBar* pToolbar = addToolBar(tr("Toolbar"));
		pToolbar->addAction(pUndo);
		pToolbar->addAction(pRedo);
		pToolbar->addSeparator();
		pToolbar->addAction(pRefresh);
		pToolbar->addSeparator();
		pToolbar->addAction(pSaveAll);
	}

	GetIEditor()->RegisterNotifyListener(this);
}

CEditorEnvironmentWindow::~CEditorEnvironmentWindow()
{
	GetIEditor()->UnregisterNotifyListener(this);
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
		m_sunSettingsWidget->OnNewScene();
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

