// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>
#include <QWidget>

class QPropertyTree;

namespace Explorer
{
struct ExplorerEntryModifyEvent;
}

namespace CharacterTool
{

using std::unique_ptr;
struct System;

class SceneParametersPanel : public QWidget
{
	Q_OBJECT
public:
	SceneParametersPanel(QWidget* parent, System* system);
	QSize sizeHint() const override { return QSize(240, 100); }
protected slots:
	void  OnPropertyTreeChanged();
	void  OnPropertyTreeContinuousChange();
	void  OnPropertyTreeSelected();

	void  OnSubselectionChanged(int layer);
	void  OnSceneChanged(bool continuous);
	void  OnCharacterLoaded();
	void  OnExplorerSelectionChanged();
	void  OnExplorerEntryModified(Explorer::ExplorerEntryModifyEvent& ev);
	void  OnBlendShapeOptionsChanged();
private:

	QPropertyTree* m_propertyTree;
	System*        m_system;
	bool           m_ignoreSubselectionChange;
};

}

