// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <EditorFramework/Editor.h>
#include <QtViewPane.h>
#include <Util/Variable.h>

class QPropertyTreeLegacy;
class CEnvironmentPresets;

class CLevelSettingsEditor : public CDockableEditor
{
public:
	CLevelSettingsEditor(QWidget* parent = nullptr);

private:
	virtual void                              Initialize() override;
	virtual const char*                       GetEditorName() const override { return "Level Settings"; }
	virtual void                              CreateDefaultLayout(CDockableContainer* pSender) override;
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	virtual QRect                             GetPaneRect() override { return QRect(0, 0, 500, 800); }

	void                                      RegisterActions();
	void                                      CreateMenu();
	void                                      RegisterDockingWidgets();
};
