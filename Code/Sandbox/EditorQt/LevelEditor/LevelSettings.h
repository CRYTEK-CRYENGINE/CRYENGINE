// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include "EditorFramework/Editor.h"
#include "QtViewPane.h"

class QPropertyTree;

class CLevelSettingsEditor : public CDockableEditor, public ISystemEventListener
{
public:
	CLevelSettingsEditor(QWidget* parent = nullptr);
	~CLevelSettingsEditor();

	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	virtual QRect                             GetPaneRect() override               { return QRect(0, 0, 500, 800); }

	virtual const char*                       GetEditorName() const override       { return "Level Settings"; };
	void                                      InitMenu();

private:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	void         ReloadFromTemplate();
	void         BeforeSerialization(Serialization::IArchive& ar);
	void         AfterSerialization(Serialization::IArchive& ar);
	void         PushUndo();

	QPropertyTree* m_pPropertyTree;
	CVarBlockPtr   m_varBlock;
	bool           m_bIgnoreEvent;
};

