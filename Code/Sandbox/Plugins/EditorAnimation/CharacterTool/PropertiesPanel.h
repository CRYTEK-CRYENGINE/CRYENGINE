// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>
#include <QWidget>
#include <QToolButton>

#include <IEditor.h>
#include <CrySerialization/Forward.h>

#include "Explorer/ExplorerDataProvider.h"

class QDockWidget;
class QToolBar;
class QPropertyTree;

namespace Explorer
{
struct ExplorerAction;
struct ExplorerEntryModifyEvent;
}

namespace CharacterTool
{
using std::unique_ptr;
using std::vector;
using Serialization::IArchive;
using namespace Explorer;
class CharacterToolForm;
struct System;

enum FollowMode
{
	FOLLOW_SELECTION,
	FOLLOW_SOURCE_ASSET,
	FOLLOW_LOCK,
	NUM_FOLLOW_MODES
};

enum OutlineMode
{
	OUTLINE_ON_LEFT,
	OUTLINE_ON_TOP,
	OUTLINE_ONE_TREE,
	NUM_OUTLINE_MODES
};

struct InspectorLocation
{
	vector<ExplorerEntryId> entries;
	vector<char>            tree;

	void Serialize(IArchive& ar);
	bool IsValid() const { return !entries.empty(); }

	bool operator==(const InspectorLocation& rhs) const;
	bool operator!=(const InspectorLocation& rhs) const { return !operator==(rhs); }
};

class PropertiesPanel : public QWidget, public IEditorNotifyListener
{
	Q_OBJECT
public:
	PropertiesPanel(QWidget* parent, System* system);
	~PropertiesPanel();
	QPropertyTree* PropertyTree()                     { return m_propertyTree; }
	void           Serialize(Serialization::IArchive& ar);
	void           SetDockWidget(QDockWidget* widget) {}
	void           OnChanged()                        { OnPropertyTreeChanged(); }

protected:
	void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

private slots:
	void OnPropertyTreeSelected();
	void OnPropertyTreeChanged();
	void OnPropertyTreeContinuousChange();
	void OnExplorerSelectionChanged();
	void OnExplorerEntryModified(ExplorerEntryModifyEvent& ev);
	void OnExplorerAction(const ExplorerAction& action);
	void OnCharacterLoaded();
	void OnSubselectionChanged(int changedLayer);
	void OnFollowMenu();
	void OnSettingMenu();
	void OnLocationButton();
	void OnBackButton();
	void OnForwardButton();
	void OnUndo();
	void OnUndoMenu();
	void OnRedo();
	void SetOutlineMode(OutlineMode outlineMode);

private:
	void keyPressEvent(QKeyEvent* ev) override;

private:
	void AttachPropertyTreeToLocation();
	void StorePropertyTreeState();
	void UpdateToolbar();
	void UpdateUndoButtons();
	void UpdateLocationBar();
	void Undo(int count);
	void Redo();

	FollowMode                m_followMode;
	OutlineMode               m_outlineMode;
	OutlineMode               m_outlineModeUsed;
	InspectorLocation         m_location;
	vector<InspectorLocation> m_previousLocations;
	vector<InspectorLocation> m_nextLocations;

	QToolButton*              m_backButton;
	QToolButton*              m_forwardButton;
	QToolButton*              m_locationButton;
	QToolButton*              m_followButton;
	QMenu*                    m_followMenu;
	QToolButton*              m_settingsButton;
	QMenu*                    m_settingsMenu;
	QAction*                  m_followActions[NUM_FOLLOW_MODES];

	QToolBar*                 m_toolbar;
	QAction*                  m_actionUndo;
	QAction*                  m_actionRedo;
	QMenu*                    m_undoMenu;

	QSplitter*                m_splitter;
	QPropertyTree*            m_propertyTree;
	QPropertyTree*            m_detailTree;
	QAction*                  m_settingActions[NUM_OUTLINE_MODES];

	System*                   m_system;
	bool                      m_ignoreSubselectionChange;
	bool                      m_ignoreExplorerSelectionChange;
};

}

