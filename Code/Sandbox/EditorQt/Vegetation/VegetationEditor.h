// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <EditorFramework/Editor.h>

#include <QWidget>

#include <memory>

class CVegetationEditor : public CDockableEditor
{
public:
	explicit CVegetationEditor(QWidget* parent = nullptr);
	~CVegetationEditor();

	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	virtual QRect                             GetPaneRect() override               { return QRect(0, 0, 800, 500); }
	virtual const char*                       GetEditorName() const override       { return "Vegetation Editor"; }

protected:
	virtual void customEvent(QEvent* pEvent) override;

private:
	void RegisterActions();

	bool OnNew();
	bool OnDelete();
	bool OnDuplicate();
	bool OnSelectAll();

private:
	struct SImplementation;
	std::unique_ptr<SImplementation> p;
};
