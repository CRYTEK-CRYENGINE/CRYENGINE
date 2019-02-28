// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UICommon.h"
#include "DesignerEditor.h"

class QPropertyTree;
class DesignerEditor;
class QTabWidget;
class QWidget;

namespace Designer
{
class DesignerSubPanel : public QWidget, public IDesignerSubPanel
{
public:

	DesignerSubPanel(QWidget* parent);
	~DesignerSubPanel();

	void     Init() override;
	void     Done() override;
	void     Update() override;
	void     UpdateBackFaceCheckBox(Model* pModel) override;
	void     OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	void     UpdateBackFaceCheckBoxFromContext();
	void     UpdateBackFaceFlag(MainContext& mc);
	void     OnDesignerNotifyHandler(EDesignerNotify notification, void* param);
	QWidget* GetWidget() override { return this; }

private:

	void     NotifyDesignerSettingChanges(bool continuous);
	void     UpdateSettingsTab();
	void     UpdateEngineFlagsTab();
	QWidget* OrganizeSettingLayout(QWidget* pParent);

	QPropertyTree* m_pSettingProperties;
};
}

