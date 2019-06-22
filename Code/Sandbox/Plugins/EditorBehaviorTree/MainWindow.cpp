// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MainWindow.h"
#include "TreePanel.h"

// EditorCommon
#include <Commands/QCommandAction.h>

// Qt
#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QMenuBar>

REGISTER_VIEWPANE_FACTORY(CMainWindow, "Behavior Tree Editor", "Tools", false);

CMainWindow::CMainWindow(QWidget* pParent)
	: CDockableEditor(pParent)
	, m_pTreePanel(nullptr)
{

	m_pTreePanel = new TreePanel(this);

	m_pMainLayout = new QBoxLayout(QBoxLayout::TopToBottom, this);
	m_pMainLayout->addWidget(m_pTreePanel);
	m_pMainLayout->setMargin(0);
	m_pMainLayout->setSpacing(0);

	SetContent(m_pMainLayout);
	RegisterActions();

	InitCVarsCallbacks();
}

void CMainWindow::Initialize()
{
	CDockableEditor::Initialize();

	InitMenuBar();
}

void CMainWindow::RegisterActions()
{
	// Register general actions
	RegisterAction("general.new", &CMainWindow::OnNew);
	RegisterAction("general.open", &CMainWindow::OnOpen);
	RegisterAction("general.save", &CMainWindow::OnSave);
	RegisterAction("general.save_as", &CMainWindow::OnSaveAs);
	RegisterAction("general.reload", &CMainWindow::OnReload);
}

void CMainWindow::InitCVarsCallbacks()
{
	m_modularBehaviorTreeDebugTreeCvarHandle = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugTreeCvar)->AddOnChange(std::bind(&CMainWindow::OnChanged_ShowVariablesDebugger, this));
	m_modularBehaviorTreeDebugVariablesCvarHandle = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugVariablesCvar)->AddOnChange(std::bind(&CMainWindow::OnChanged_ShowVariablesDebugger, this));
	m_modularBehaviorTreeDebugTimestampsCvarHandle = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugTimestampsCvar)->AddOnChange(std::bind(&CMainWindow::OnChanged_ShowTimestampsDebugger, this));
	m_modularBehaviorTreeDebugEventsCvarHandle = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugEventsCvar)->AddOnChange(std::bind(&CMainWindow::OnChanged_ShowEventsDebugger, this));
	m_modularBehaviorTreeDebugLogCvarHandle = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugLogCvar)->AddOnChange(std::bind(&CMainWindow::OnChanged_ShowLogDebugger, this));
	m_modularBehaviorTreeDebugBlackBoardCvarHandle = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugBlackboardCvar)->AddOnChange(std::bind(&CMainWindow::OnChanged_ShowLogDebugger, this));
	m_modularBehaviorTreeStatisticsCvarHandle = gEnv->pConsole->GetCVar(k_modularBehaviorTreeStatisticsCvar)->AddOnChange(std::bind(&CMainWindow::OnChanged_ShowStatistics, this));
	m_modularBehaviorTreeDebugExecutionLogCvarHandle = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugExecutionLogCvar)->AddOnChange(std::bind(&CMainWindow::OnChanged_ExecutionHistoryLog, this));
}

void CMainWindow::InitMenuBar()
{
	//File
	AddToMenu({ CEditor::MenuItems::FileMenu, MenuItems::New, MenuItems::Open, MenuItems::Save, MenuItems::SaveAs, MenuItems::RecentFiles });

	CAbstractMenu* const pFileMenu = GetMenu(CEditor::MenuItems::FileMenu);
	int section = pFileMenu->GetNextEmptySection();
	pFileMenu->AddAction(GetAction("general.reload"), section);

	//View
	AddToMenu(CEditor::MenuItems::ViewMenu);
	CAbstractMenu* const pMenuView = GetMenu(CEditor::MenuItems::ViewMenu);

	section = pMenuView->GetNextEmptySection();
	m_pShowXmlLineNumberMenuAction = pMenuView->CreateAction("Show XML line numbers", section);
	m_pShowXmlLineNumberMenuAction->setCheckable(true);
	QObject::connect(m_pShowXmlLineNumberMenuAction, &QAction::triggered, [this]()
	{
		m_pTreePanel->OnWindowEvent_ShowXmlLineNumbers(GetShowXmlLineNumbers());
	});

	m_pShowCommentsMenuAction = pMenuView->CreateAction("Show comments", section);
	m_pShowCommentsMenuAction->setCheckable(true);
	QObject::connect(m_pShowCommentsMenuAction, &QAction::triggered, [this]()
	{
		m_pTreePanel->OnWindowEvent_ShowComments(GetShowComments());
	});

	CAbstractMenu* pMenuEvents = pMenuView->CreateMenu("Built-in Events", section);
	section = pMenuEvents->GetNextEmptySection();

	m_pEnableCryEngineEventsMenuAction = pMenuEvents->CreateAction("Enable CryEngine events", section);
	m_pEnableCryEngineEventsMenuAction->setCheckable(true);
	QObject::connect(m_pEnableCryEngineEventsMenuAction, &QAction::triggered, [this]()
	{
		m_pTreePanel->OnWindowEvent_ShowCryEngineSignals(GetEnableCryEngineEvents());
	});

	m_pEnableGameSDKEventsMenuAction = pMenuEvents->CreateAction("Enable GameSDK events", section);
	m_pEnableGameSDKEventsMenuAction->setCheckable(true);
	QObject::connect(m_pEnableGameSDKEventsMenuAction, &QAction::triggered, [this]()
	{
		m_pTreePanel->OnWindowEvent_ShowDeprecatedSignals(GetEnableCryEngineEvents());
	});

	m_pEnableDeprecatedEventsMenuAction = pMenuEvents->CreateAction("Enable Deprecated events", section);
	m_pEnableDeprecatedEventsMenuAction->setCheckable(true);
	QObject::connect(m_pEnableDeprecatedEventsMenuAction, &QAction::triggered, [this]()
	{
		m_pTreePanel->OnWindowEvent_ShowDeprecatedSignals(GetEnableCryEngineEvents());
	});

	// Debug
	CAbstractMenu* pMenuDebug = GetRootMenu()->CreateMenu("Debug", section);
	section = pMenuEvents->GetNextEmptySection();

	m_pEnableTreeDebuggerMenuAction = pMenuDebug->CreateAction("Show Tree", section);
	m_pEnableTreeDebuggerMenuAction->setCheckable(true);
	QObject::connect(m_pEnableTreeDebuggerMenuAction, &QAction::triggered, [this]()
	{
		SetShowTreeDebugger(!GetShowTreeDebuggerCvar());
	});

	m_pEnableVariablesDebuggerMenuAction = pMenuDebug->CreateAction("Show Variables", section);
	m_pEnableVariablesDebuggerMenuAction->setCheckable(true);
	QObject::connect(m_pEnableVariablesDebuggerMenuAction, &QAction::triggered, [this]()
	{
		SetShowVariablesDebugger(!GetShowVariablesDebuggerCvar());
	});

	m_pEnableTimestampsDebuggerMenuAction = pMenuDebug->CreateAction("Show Timestamps", section);
	m_pEnableTimestampsDebuggerMenuAction->setCheckable(true);
	QObject::connect(m_pEnableTimestampsDebuggerMenuAction, &QAction::triggered, [this]()
	{
		SetShowTimestampsDebugger(!GetShowTimestampsDebuggerCvar());
	});

	m_pEnableEventsDebuggerMenuAction = pMenuDebug->CreateAction("Show Events", section);
	m_pEnableEventsDebuggerMenuAction->setCheckable(true);
	QObject::connect(m_pEnableEventsDebuggerMenuAction, &QAction::triggered, [this]()
	{
		SetShowEventsDebugger(!GetShowEventsDebuggerCvar());
	});

	m_pEnableLogDebuggerMenuAction = pMenuDebug->CreateAction("Show Log", section);
	m_pEnableLogDebuggerMenuAction->setCheckable(true);
	QObject::connect(m_pEnableLogDebuggerMenuAction, &QAction::triggered, [this]()
	{
		SetShowLogDebugger(!GetShowLogDebuggerCvar());
	});

	m_pEnableBlackboardDebuggerMenuAction = pMenuDebug->CreateAction("Show Blackboard", section);
	m_pEnableBlackboardDebuggerMenuAction->setCheckable(true);
	QObject::connect(m_pEnableBlackboardDebuggerMenuAction, &QAction::triggered, [this]()
		{
			SetShowBlackboardDebugger(!GetShowBlackboardDebuggerCvar());
		});

	m_pEnableAllDebuggerMenuAction = pMenuDebug->CreateAction("Show All", section);
	m_pEnableAllDebuggerMenuAction->setCheckable(true);
	QObject::connect(m_pEnableAllDebuggerMenuAction, &QAction::triggered, [this]()
	{
		const bool isEnabled = GetShowAllDebugger();

		if (isEnabled)
		{
			const bool isEnabled = GetShowAllDebugger();

			if (isEnabled)
			{
				SetShowTreeDebugger(true);
				SetShowVariablesDebugger(true);
				SetShowTimestampsDebugger(true);
				SetShowEventsDebugger(true);
				SetShowLogDebugger(true);
				SetShowBlackboardDebugger(true);
			}
			else if (GetShowTreeDebuggerCvar()
				&& GetShowVariablesDebuggerCvar()
				&& GetShowTimestampsDebuggerCvar()
				&& GetShowEventsDebuggerCvar()
				&& GetShowLogDebuggerCvar()
				&& GetShowBlackboardDebuggerCvar())
			{
				SetShowTreeDebugger(false);
				SetShowVariablesDebugger(false);
				SetShowTimestampsDebugger(false);
				SetShowEventsDebugger(false);
				SetShowLogDebugger(false);
				SetShowBlackboardDebugger(false);
			}
		}
	});

	section = pMenuDebug->GetNextEmptySection();
	m_pEnableStatisticsMenuAction = pMenuDebug->CreateAction("Show Statistics", section);
	m_pEnableStatisticsMenuAction->setCheckable(true);
	QObject::connect(m_pEnableStatisticsMenuAction, &QAction::triggered, [this]()
	{
		SetShowStatisticsCvar(!GetShowStatisticsCvar());
	});

	CAbstractMenu* pMenuExecutionHistory = pMenuDebug->CreateMenu("Execution History", section);
	m_pEnableExecutionHistoryLogCurrentEntityMenuAction = pMenuExecutionHistory->CreateAction("Log execution for current agent", section);
	m_pEnableExecutionHistoryLogCurrentEntityMenuAction->setCheckable(true);
	QObject::connect(m_pEnableExecutionHistoryLogCurrentEntityMenuAction, &QAction::triggered, [this]()
	{
		const int previousValue = GetEnableExecutionHistoryLogCvar();

		if (previousValue == 0 || previousValue == 2)
		{
		  SetEnableExecutionHistoryLog(1);
		}
		else if (previousValue == 1)
		{
		  SetEnableExecutionHistoryLog(0);
		}
	});

	m_pEnableExecutionHistoryLogAllEntitiesMenuAction = pMenuExecutionHistory->CreateAction("Log execution for all agents", section);
	m_pEnableExecutionHistoryLogAllEntitiesMenuAction->setCheckable(true);
	QObject::connect(m_pEnableExecutionHistoryLogAllEntitiesMenuAction, &QAction::triggered, [this]()
	{
		const int previousValue = GetEnableExecutionHistoryLogCvar();

		if (previousValue == 0 || previousValue == 1)
		{
		  SetEnableExecutionHistoryLog(2);
		}
		else if (previousValue == 2)
		{
		  SetEnableExecutionHistoryLog(0);
		}
	});
}

void CMainWindow::SetLayout(const QVariantMap& state)
{
	CDockableEditor::SetLayout(state);

	QVariant showXmlLineNumbers = state.value("showXmlLineNumbers");
	if (showXmlLineNumbers.isValid())
	{
		SetShowXmlLineNumbers(showXmlLineNumbers.toBool());
	}

	QVariant showComments = state.value("showComments");
	if (showComments.isValid())
	{
		SetShowComments(showComments.toBool());
	}

	QVariant enableCryEngineEvents = state.value("enableCryEngineEvents");
	if (enableCryEngineEvents.isValid())
	{
		SetEnableCryEngineEvents(enableCryEngineEvents.toBool());
	}

	QVariant enableGameSDKEvents = state.value("enableGameSDKEvents");
	if (enableGameSDKEvents.isValid())
	{
		SetEnableGameSDKEvents(enableGameSDKEvents.toBool());
	}

	QVariant enableDeprecatedEvents = state.value("enableDeprecatedEvents");
	if (enableDeprecatedEvents.isValid())
	{
		SetEnableDeprecatedEvents(enableDeprecatedEvents.toBool());
	}

	QVariant showDebugTree = state.value("showDebugTree");
	if (showDebugTree.isValid())
	{
		SetShowTreeDebugger(showDebugTree.toBool());
	}

	QVariant showDebugVariables = state.value("showDebugVariables");
	if (showDebugVariables.isValid())
	{
		SetShowVariablesDebugger(showDebugVariables.toBool());
	}

	QVariant showDebugTimestamps = state.value("showDebugTimestamps");
	if (showDebugTimestamps.isValid())
	{
		SetShowTimestampsDebugger(showDebugTimestamps.toBool());
	}

	QVariant showDebugEvents = state.value("showDebugEvents");
	if (showDebugEvents.isValid())
	{
		SetShowEventsDebugger(showDebugEvents.toBool());
	}

	QVariant showDebugLog = state.value("showDebugLog");
	if (showDebugLog.isValid())
	{
		SetShowLogDebugger(showDebugLog.toBool());
	}

	QVariant showDebugAll = state.value("showDebugAll");
	if (showDebugAll.isValid())
	{
		SetShowAllDebugger(showDebugAll.toBool());
	}

	QVariant showStatistics = state.value("showStatistics");
	if (showStatistics.isValid())
	{
		SetShowStatisticsCvar(showStatistics.toBool());
	}

	QVariant enableExecutionHistory = state.value("enableExecutionHistory");
	if (enableExecutionHistory.isValid())
	{
		SetEnableExecutionHistoryLog(enableExecutionHistory.toInt());
	}
}

QVariantMap CMainWindow::GetLayout() const
{
	QVariantMap state = CDockableEditor::GetLayout();

	state.insert("showXmlLineNumbers", GetShowXmlLineNumbers());
	state.insert("showComments", GetShowComments());

	state.insert("enableCryEngineEvents", GetEnableCryEngineEvents());
	state.insert("enableGameSDKEvents", GetEnableGameSDKEvents());
	state.insert("enableDeprecatedEvents", GetEnableDeprecatedEvents());

	state.insert("showDebugTree", GetShowTreeDebuggerCvar());
	state.insert("showDebugVariables", GetShowVariablesDebuggerCvar());
	state.insert("showDebugTimestamps", GetShowTimestampsDebuggerCvar());
	state.insert("showDebugEvents", GetShowEventsDebuggerCvar());
	state.insert("showDebugLog", GetShowLogDebuggerCvar());
	state.insert("showDebugAll", GetShowAllDebugger());

	state.insert("showStatistics", GetShowStatisticsCvar());
	state.insert("enableExecutionHistory", GetEnableExecutionHistoryLogCvar());

	return state;
}

void CMainWindow::AddRecentFile(const QString& fileName)
{
	CDockableEditor::AddRecentFile(fileName);
}

void CMainWindow::closeEvent(QCloseEvent* closeEvent)
{
	SaveLayoutPersonalization();

	if (!m_pTreePanel->HandleCloseEvent())
	{
		closeEvent->ignore();
		return;
	}

	QWidget::closeEvent(closeEvent);
}

bool CMainWindow::OnNew()
{
	m_pTreePanel->OnWindowEvent_NewFile();
	return true;
}

bool CMainWindow::OnOpen()
{
	m_pTreePanel->OnWindowEvent_OpenFile();
	return true;
}

bool CMainWindow::OnOpenFile(const QString& path)
{
	m_pTreePanel->OnWindowEvent_OpenFile(path);
	return true;
}

bool CMainWindow::OnSave()
{
	m_pTreePanel->OnWindowEvent_Save();
	SaveLayoutPersonalization();
	return true;
}

bool CMainWindow::OnSaveAs()
{
	m_pTreePanel->OnWindowEvent_SaveToFile();
	SaveLayoutPersonalization();
	return true;
}

bool CMainWindow::OnReload()
{
	m_pTreePanel->OnWindowEvent_Reload();
	return true;
}

bool CMainWindow::GetShowXmlLineNumbers() const
{
	if (!m_pShowXmlLineNumberMenuAction)
	{
		return false;
	}

	return m_pShowXmlLineNumberMenuAction->isChecked();
}

void CMainWindow::SetShowXmlLineNumbers(const bool showFlag)
{
	if (!m_pShowXmlLineNumberMenuAction)
	{
		return;
	}

	m_pShowXmlLineNumberMenuAction->setChecked(showFlag);

	m_pTreePanel->ShowXmlLineNumbers(showFlag);
}

bool CMainWindow::GetShowComments() const
{
	if (!m_pShowCommentsMenuAction)
	{
		return false;
	}

	return m_pShowCommentsMenuAction->isChecked();
}

void CMainWindow::SetShowComments(const bool showFlag)
{
	if (!m_pShowCommentsMenuAction)
	{
		return;
	}

	m_pShowCommentsMenuAction->setChecked(showFlag);

	m_pTreePanel->ShowComments(showFlag);
}

bool CMainWindow::GetEnableCryEngineEvents() const
{
	if (!m_pEnableCryEngineEventsMenuAction)
	{
		return false;
	}

	return m_pEnableCryEngineEventsMenuAction->isChecked();
}

void CMainWindow::SetEnableCryEngineEvents(const bool enable)
{
	if (!m_pEnableCryEngineEventsMenuAction)
	{
		return;
	}

	m_pEnableCryEngineEventsMenuAction->setChecked(enable);

	m_pTreePanel->ShowCryEngineSignals(enable);
}

bool CMainWindow::GetEnableGameSDKEvents() const
{
	if (!m_pEnableGameSDKEventsMenuAction)
	{
		return false;
	}

	return m_pEnableGameSDKEventsMenuAction->isChecked();
}

void CMainWindow::SetEnableGameSDKEvents(const bool enable)
{
	if (!m_pEnableGameSDKEventsMenuAction)
	{
		return;
	}

	m_pEnableGameSDKEventsMenuAction->setChecked(enable);

	m_pTreePanel->ShowGameSDKSignals(enable);
}

bool CMainWindow::GetEnableDeprecatedEvents() const
{
	if (!m_pEnableDeprecatedEventsMenuAction)
	{
		return false;
	}

	return m_pEnableDeprecatedEventsMenuAction->isChecked();
}

void CMainWindow::SetEnableDeprecatedEvents(const bool enableFlag)
{
	if (!m_pEnableDeprecatedEventsMenuAction)
	{
		return;
	}

	m_pEnableDeprecatedEventsMenuAction->setChecked(enableFlag);

	m_pTreePanel->ShowDeprecatedSignals(enableFlag);
}

bool CMainWindow::GetShowTreeDebuggerCvar() const
{
	if (!m_pEnableTreeDebuggerMenuAction)
	{
		return false;
	}

	const ICVar* pDebugTree = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugTreeCvar);
	return pDebugTree->GetIVal();
}

void CMainWindow::SetShowTreeDebugger(const bool enableFlag)
{
	if (!m_pEnableTreeDebuggerMenuAction)
	{
		return;
	}

	m_pEnableTreeDebuggerMenuAction->setChecked(enableFlag);

	ICVar* pDebugTree = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugTreeCvar);
	pDebugTree->Set(enableFlag);

	if (!enableFlag)
	{
		SetShowAllDebugger(false);
	}
}

void CMainWindow::OnChanged_ShowTreeDebugger()
{
	const ICVar* pDebugTree = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugTreeCvar);
	const bool debugTreeEnabled = pDebugTree->GetIVal();

	if (debugTreeEnabled != m_pEnableVariablesDebuggerMenuAction->isChecked())
	{
		m_pEnableTreeDebuggerMenuAction->setChecked(debugTreeEnabled);
	}
}

bool CMainWindow::GetShowVariablesDebuggerCvar() const
{
	if (!m_pEnableVariablesDebuggerMenuAction)
	{
		return false;
	}

	const ICVar* pDebugVariables = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugVariablesCvar);
	return pDebugVariables->GetIVal();
}

void CMainWindow::SetShowVariablesDebugger(const bool enableFlag)
{
	if (!m_pEnableVariablesDebuggerMenuAction)
	{
		return;
	}

	m_pEnableVariablesDebuggerMenuAction->setChecked(enableFlag);

	ICVar* pDebugVariables = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugVariablesCvar);
	pDebugVariables->Set(enableFlag);

	if (!enableFlag)
	{
		SetShowAllDebugger(false);
	}
}

void CMainWindow::OnChanged_ShowVariablesDebugger()
{
	const ICVar* pDebugVariables = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugVariablesCvar);
	const bool debugVariablesEnabled = pDebugVariables->GetIVal();

	if (debugVariablesEnabled != m_pEnableVariablesDebuggerMenuAction->isChecked())
	{
		m_pEnableVariablesDebuggerMenuAction->setChecked(debugVariablesEnabled);
	}
}

bool CMainWindow::GetShowTimestampsDebuggerCvar() const
{
	if (!m_pEnableTimestampsDebuggerMenuAction)
	{
		return false;
	}

	const ICVar* pDebugTimestamps = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugTimestampsCvar);
	return pDebugTimestamps->GetIVal();
}

void CMainWindow::SetShowTimestampsDebugger(const bool enableFlag)
{
	if (!m_pEnableTimestampsDebuggerMenuAction)
	{
		return;
	}

	m_pEnableTimestampsDebuggerMenuAction->setChecked(enableFlag);

	ICVar* pDebugTimestamps = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugTimestampsCvar);
	pDebugTimestamps->Set(enableFlag);

	if (!enableFlag)
	{
		SetShowAllDebugger(false);
	}
}

void CMainWindow::OnChanged_ShowTimestampsDebugger()
{
	const ICVar* pDebugTimestamps = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugTimestampsCvar);
	const bool debugTimestampsEnabled = pDebugTimestamps->GetIVal();

	if (debugTimestampsEnabled != m_pEnableTimestampsDebuggerMenuAction->isChecked())
	{
		m_pEnableTimestampsDebuggerMenuAction->setChecked(debugTimestampsEnabled);
	}
}

bool CMainWindow::GetShowEventsDebuggerCvar() const
{
	if (!m_pEnableEventsDebuggerMenuAction)
	{
		return false;
	}

	const ICVar* pDebugEvents = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugEventsCvar);
	return pDebugEvents->GetIVal();
}

void CMainWindow::SetShowEventsDebugger(const bool enableFlag)
{
	if (!m_pEnableEventsDebuggerMenuAction)
	{
		return;
	}

	m_pEnableEventsDebuggerMenuAction->setChecked(enableFlag);

	ICVar* pDebugEvents = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugEventsCvar);
	pDebugEvents->Set(enableFlag);

	if (!enableFlag)
	{
		SetShowAllDebugger(false);
	}
}

void CMainWindow::OnChanged_ShowEventsDebugger()
{
	const ICVar* pDebugEvents = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugEventsCvar);
	const bool debugEventsEnabled = pDebugEvents->GetIVal();

	if (debugEventsEnabled != m_pEnableEventsDebuggerMenuAction->isChecked())
	{
		m_pEnableEventsDebuggerMenuAction->setChecked(debugEventsEnabled);
	}
}

bool CMainWindow::GetShowLogDebuggerCvar() const
{
	if (!m_pEnableLogDebuggerMenuAction)
	{
		return false;
	}

	const ICVar* pDebugLog = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugLogCvar);
	return pDebugLog->GetIVal();
}

void CMainWindow::SetShowLogDebugger(const bool enableFlag)
{
	if (!m_pEnableLogDebuggerMenuAction)
	{
		return;
	}

	m_pEnableLogDebuggerMenuAction->setChecked(enableFlag);

	ICVar* pDebugLog = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugLogCvar);
	pDebugLog->Set(enableFlag);

	if (!enableFlag)
	{
		SetShowAllDebugger(false);
	}
}

void CMainWindow::OnChanged_ShowLogDebugger()
{
	const ICVar* pDebugLog = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugLogCvar);
	const bool debugLogEnabled = pDebugLog->GetIVal();

	if (debugLogEnabled != m_pEnableLogDebuggerMenuAction->isChecked())
	{
		m_pEnableLogDebuggerMenuAction->setChecked(debugLogEnabled);
	}
}

bool CMainWindow::GetShowBlackboardDebuggerCvar() const
{
	if (!m_pEnableBlackboardDebuggerMenuAction)
	{
		return false;
	}

	const ICVar* pDebugBlackboard = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugBlackboardCvar);
	return pDebugBlackboard->GetIVal();
}

void CMainWindow::SetShowBlackboardDebugger(const bool enableFlag)
{
	if (!m_pEnableBlackboardDebuggerMenuAction)
	{
		return;
	}

	m_pEnableBlackboardDebuggerMenuAction->setChecked(enableFlag);

	ICVar* pDebugBlackboard = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugBlackboardCvar);
	pDebugBlackboard->Set(enableFlag);

	if (!enableFlag)
	{
		SetShowAllDebugger(false);
	}
}

void CMainWindow::OnChanged_ShowBlackboardDebugger()
{
	const ICVar* pDebugBlackboard = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugBlackboardCvar);
	const bool debugBlackboardEnabled = pDebugBlackboard->GetIVal();

	if (debugBlackboardEnabled != m_pEnableBlackboardDebuggerMenuAction->isChecked())
	{
		m_pEnableBlackboardDebuggerMenuAction->setChecked(debugBlackboardEnabled);
	}
}

bool CMainWindow::GetShowAllDebugger() const
{
	if (!m_pEnableAllDebuggerMenuAction)
	{
		return false;
	}

	return m_pEnableAllDebuggerMenuAction->isChecked();
}

void CMainWindow::SetShowAllDebugger(const bool enableFlag)
{
	if (!m_pEnableStatisticsMenuAction)
	{
		return;
	}

	m_pEnableStatisticsMenuAction->setChecked(enableFlag);
}

bool CMainWindow::GetShowStatisticsCvar() const
{
	if (!m_pEnableStatisticsMenuAction)
	{
		return false;
	}

	const ICVar* pDebugStatistics = gEnv->pConsole->GetCVar(k_modularBehaviorTreeStatisticsCvar);
	return pDebugStatistics->GetIVal();
}

void CMainWindow::SetShowStatisticsCvar(const bool enableFlag)
{
	if (!m_pEnableStatisticsMenuAction)
	{
		return;
	}

	m_pEnableStatisticsMenuAction->setChecked(enableFlag);

	ICVar* pDebugStatistics = gEnv->pConsole->GetCVar(k_modularBehaviorTreeStatisticsCvar);
	pDebugStatistics->Set(enableFlag);
}

void CMainWindow::OnChanged_ShowStatistics()
{
	const ICVar* pDebugStatistics = gEnv->pConsole->GetCVar(k_modularBehaviorTreeStatisticsCvar);
	const bool debugStatisticsEnabled = pDebugStatistics->GetIVal();

	if (debugStatisticsEnabled != m_pEnableStatisticsMenuAction->isChecked())
	{
		m_pEnableStatisticsMenuAction->setChecked(debugStatisticsEnabled);
	}
}

int CMainWindow::GetEnableExecutionHistoryLogCvar() const
{
	if (!m_pEnableExecutionHistoryLogCurrentEntityMenuAction)
	{
		return false;
	}

	const ICVar* pExecutionLog = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugExecutionLogCvar);
	return pExecutionLog->GetIVal();
}

void CMainWindow::SetEnableExecutionHistoryLog(const int enableFlag)
{
	if (!m_pEnableExecutionHistoryLogCurrentEntityMenuAction || !m_pEnableExecutionHistoryLogAllEntitiesMenuAction)
	{
		return;
	}

	m_pEnableExecutionHistoryLogCurrentEntityMenuAction->setChecked(enableFlag == 1 ? true : false);
	m_pEnableExecutionHistoryLogAllEntitiesMenuAction->setChecked(enableFlag == 2 ? true : false);

	ICVar* pExecutionLog = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugExecutionLogCvar);
	pExecutionLog->Set(enableFlag);
}

void CMainWindow::OnChanged_ExecutionHistoryLog()
{
	const ICVar* pDebugExecutionLog = gEnv->pConsole->GetCVar(k_modularBehaviorTreeDebugExecutionLogCvar);

	switch (pDebugExecutionLog->GetIVal())
	{
	case 0:
		m_pEnableExecutionHistoryLogCurrentEntityMenuAction->setChecked(false);
		m_pEnableExecutionHistoryLogAllEntitiesMenuAction->setChecked(false);
		break;
	case 1:
		m_pEnableExecutionHistoryLogCurrentEntityMenuAction->setChecked(true);
		m_pEnableExecutionHistoryLogAllEntitiesMenuAction->setChecked(false);
		break;
	case 2:
		m_pEnableExecutionHistoryLogCurrentEntityMenuAction->setChecked(false);
		m_pEnableExecutionHistoryLogAllEntitiesMenuAction->setChecked(true);
		break;
	default:
		CRY_ASSERT_MESSAGE(false, "Unknown value for Cvar.");
		break;
	}
}