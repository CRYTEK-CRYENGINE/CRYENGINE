// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <EditorFramework/Editor.h>

#include <QDialog>
#include <QDockWidget>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

class QAction;
class TreePanel;

class CMainWindow final : public CDockableEditor
{
	Q_OBJECT

public:
	CMainWindow(QWidget* pParent = nullptr);

	// CEditor implementation
	virtual void Initialize() override;
	virtual const char* GetEditorName() const override { return "Behavior Tree Editor"; }
	virtual void        SetLayout(const QVariantMap& state) override;
	virtual QVariantMap GetLayout() const override;
	// ~CEditor implementation

	void                AddRecentFile(const QString& fileName);

private:
	// CEditor implementation
	virtual bool        OnOpenFile(const QString& path) override;
	// ~CEditor implementation

	bool        OnNew();
	bool        OnOpen();
	bool        OnSave();
	bool        OnSaveAs();
	bool        OnReload();

	void        RegisterActions();
	void                InitMenuBar();
	void                InitCVarsCallbacks();

	bool                GetShowXmlLineNumbers() const;
	void                SetShowXmlLineNumbers(const bool showFlag);
		                
	bool                GetShowComments() const;
	void                SetShowComments(const bool showFlag);
		                
	bool                GetEnableCryEngineEvents() const;
	void                SetEnableCryEngineEvents(const bool enable);
		                
	bool                GetEnableGameSDKEvents() const;
	void                SetEnableGameSDKEvents(const bool enable);
		                
	bool                GetEnableDeprecatedEvents() const;
	void                SetEnableDeprecatedEvents(const bool enableFlag);

	bool                GetShowTreeDebuggerCvar() const;
	void                SetShowTreeDebugger(const bool enableFlag);
	void                OnChanged_ShowTreeDebugger();
		                
	bool                GetShowVariablesDebuggerCvar() const;
	void                SetShowVariablesDebugger(const bool enableFlag);
	void                OnChanged_ShowVariablesDebugger();
		                
	bool                GetShowTimestampsDebuggerCvar() const;
	void                SetShowTimestampsDebugger(const bool enableFlag);
	void                OnChanged_ShowTimestampsDebugger();
		                
	bool                GetShowEventsDebuggerCvar() const;
	void                SetShowEventsDebugger(const bool enableFlag);
	void                OnChanged_ShowEventsDebugger();
		                
	bool                GetShowLogDebuggerCvar() const;
	void                SetShowLogDebugger(const bool enableFlag);
	void                OnChanged_ShowLogDebugger();

	bool                GetShowBlackboardDebuggerCvar() const;
	void                SetShowBlackboardDebugger(const bool enableFlag);
	void                OnChanged_ShowBlackboardDebugger();

	bool                GetShowAllDebugger() const;
	void                SetShowAllDebugger(const bool enableFlag);
		                
	bool                GetShowStatisticsCvar() const;
	void                SetShowStatisticsCvar(const bool enableFlag);
	void                OnChanged_ShowStatistics();
		                
	int                 GetEnableExecutionHistoryLogCvar() const;
	void                SetEnableExecutionHistoryLog(const int enableFlag);
	void                OnChanged_ExecutionHistoryLog();

protected:
	virtual void        closeEvent(QCloseEvent* closeEvent) override;

private:
	TreePanel*          m_pTreePanel;
	QBoxLayout*         m_pMainLayout;

	// View
	QAction*            m_pShowXmlLineNumberMenuAction;
	QAction*            m_pShowCommentsMenuAction;

	QAction*            m_pEnableCryEngineEventsMenuAction;
	QAction*            m_pEnableGameSDKEventsMenuAction;
	QAction*            m_pEnableDeprecatedEventsMenuAction;

	// Debug
	QAction*            m_pEnableTreeDebuggerMenuAction;
	QAction*            m_pEnableVariablesDebuggerMenuAction;
	QAction*            m_pEnableTimestampsDebuggerMenuAction;
	QAction*            m_pEnableEventsDebuggerMenuAction;
	QAction*            m_pEnableLogDebuggerMenuAction;
	QAction*            m_pEnableBlackboardDebuggerMenuAction;
	QAction*            m_pEnableAllDebuggerMenuAction;
	QAction*            m_pEnableStatisticsMenuAction;

	QAction*            m_pEnableExecutionHistoryLogCurrentEntityMenuAction;
	QAction*            m_pEnableExecutionHistoryLogAllEntitiesMenuAction;

	// CVars
	const string        k_modularBehaviorTreeDebugTreeCvar = "ai_ModularBehaviorTreeDebugTree";
	const string        k_modularBehaviorTreeDebugVariablesCvar = "ai_ModularBehaviorTreeDebugVariables";
	const string        k_modularBehaviorTreeDebugTimestampsCvar = "ai_ModularBehaviorTreeDebugTimestamps";
	const string        k_modularBehaviorTreeDebugEventsCvar = "ai_ModularBehaviorTreeDebugEvents";
	const string        k_modularBehaviorTreeDebugLogCvar = "ai_ModularBehaviorTreeDebugLog";
	const string        k_modularBehaviorTreeDebugBlackboardCvar = "ai_ModularBehaviorTreeDebugBlackboard";
	const string        k_modularBehaviorTreeStatisticsCvar = "ai_DrawModularBehaviorTreeStatistics";
	const string        k_modularBehaviorTreeDebugExecutionLogCvar = "ai_ModularBehaviorTreeDebugExecutionStacks";

	// CVars callbacks handles
	uint64              m_modularBehaviorTreeDebugTreeCvarHandle;
	uint64              m_modularBehaviorTreeDebugVariablesCvarHandle;
	uint64              m_modularBehaviorTreeDebugTimestampsCvarHandle;
	uint64              m_modularBehaviorTreeDebugEventsCvarHandle;
	uint64              m_modularBehaviorTreeDebugLogCvarHandle;
	uint64              m_modularBehaviorTreeDebugBlackBoardCvarHandle;
	uint64              m_modularBehaviorTreeStatisticsCvarHandle;
	uint64              m_modularBehaviorTreeDebugExecutionLogCvarHandle;
};

struct UnsavedChangesDialog : public QDialog
{
	enum Result
	{
		Yes,
		No,
		Cancel
	};

	Result result;

	UnsavedChangesDialog()
		: result(Cancel)
	{
		setWindowTitle("Behavior Tree Editor");

		QLabel* label = new QLabel("There are unsaved changes. Do you want to save them?");

		QPushButton* yesButton = new QPushButton("Yes");
		QPushButton* noButton = new QPushButton("No");
		QPushButton* cancelButton = new QPushButton("Cancel");

		QGridLayout* gridLayout = new QGridLayout();
		gridLayout->addWidget(label, 0, 0);

		QHBoxLayout* buttonLayout = new QHBoxLayout();
		buttonLayout->addWidget(yesButton);
		buttonLayout->addWidget(noButton);
		buttonLayout->addWidget(cancelButton);

		gridLayout->addLayout(buttonLayout, 1, 0, Qt::AlignCenter);

		QVBoxLayout* mainLayout = new QVBoxLayout();
		mainLayout->addLayout(gridLayout);
		setLayout(mainLayout);

		connect(yesButton, SIGNAL(clicked()), this, SLOT(clickedYes()));
		connect(noButton, SIGNAL(clicked()), this, SLOT(clickedNo()));
		connect(cancelButton, SIGNAL(clicked()), this, SLOT(clickedCancel()));
	}

	Q_OBJECT

public Q_SLOTS:
	void clickedYes()
	{
		result = Yes;
		accept();
	}

	void clickedNo()
	{
		result = No;
		reject();
	}

	void clickedCancel()
	{
		result = Cancel;
		close();
	}
};