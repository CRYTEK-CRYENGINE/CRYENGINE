// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QtViewPane.h>
#include <IEditor.h>

class QTimeOfDayWidget;
class QPresetsWidget;
class QSunSettingsWidget;

class CEditorEnvironmentWindow : public CDockableWindow, public IEditorNotifyListener
{
	Q_OBJECT
public:
	CEditorEnvironmentWindow();
	~CEditorEnvironmentWindow();

	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }

	virtual const char*                       GetPaneTitle() const override        { return "Environment Editor"; }

protected:
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void customEvent(QEvent* event) override;

protected:
	QPresetsWidget*     m_presetsWidget;
	QTimeOfDayWidget*   m_timeOfDayWidget;
	QSunSettingsWidget* m_sunSettingsWidget;
	QDockWidget*        m_pPresetsDock;
	QDockWidget*        m_pSunSettingsDock;
};

