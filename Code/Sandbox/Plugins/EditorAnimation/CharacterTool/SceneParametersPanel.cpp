// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "QPropertyTree/ContextList.h"
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include "CharacterDocument.h"
#include "Expected.h"
#include "PlaybackLayers.h"
#include "SceneContent.h"
#include "SceneParametersPanel.h"
#include "Serialization.h"
#include "Serialization/Decorators/INavigationProvider.h"
#include "CharacterGizmoManager.h"
#include "CharacterToolSystem.h"
#include "GizmoSink.h"
#include <CryAnimation/ICryAnimation.h>
#include <QBoxLayout>

namespace CharacterTool
{

// ---------------------------------------------------------------------------

SceneParametersPanel::SceneParametersPanel(QWidget* parent, System* system)
	: QWidget(parent)
	, m_system(system)
	, m_ignoreSubselectionChange(false)
{
	EXPECTED(connect(m_system->scene.get(), &SceneContent::SignalChanged, this, &SceneParametersPanel::OnSceneChanged));
	EXPECTED(connect(m_system->scene.get(), &SceneContent::SignalBlendShapeOptionsChanged, this, &SceneParametersPanel::OnBlendShapeOptionsChanged));
	EXPECTED(connect(m_system->document.get(), &CharacterDocument::SignalExplorerSelectionChanged, this, &SceneParametersPanel::OnExplorerSelectionChanged));
	EXPECTED(connect(m_system->document.get(), &CharacterDocument::SignalCharacterLoaded, this, &SceneParametersPanel::OnCharacterLoaded));
	EXPECTED(connect(m_system->explorerData.get(), &ExplorerData::SignalEntryModified, this, &SceneParametersPanel::OnExplorerEntryModified));
	EXPECTED(connect(m_system->characterGizmoManager.get(), &CharacterGizmoManager::SignalSubselectionChanged, this, &SceneParametersPanel::OnSubselectionChanged));

	QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
	layout->setMargin(0);
	layout->setSpacing(0);

	m_propertyTree = new QPropertyTree(this);
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	treeStyle.groupRectangle = false;
	m_propertyTree->setTreeStyle(treeStyle);
	m_propertyTree->setExpandLevels(0);
	m_propertyTree->setAutoRevert(false);
	m_propertyTree->setSliderUpdateDelay(0);
	m_propertyTree->setShowContainerIndices(false);
	m_propertyTree->setArchiveContext(m_system->contextList->Tail());
	m_propertyTree->attach(Serialization::SStruct(*m_system->scene));
	EXPECTED(connect(m_propertyTree, &QPropertyTree::signalChanged, this, &SceneParametersPanel::OnPropertyTreeChanged));
	EXPECTED(connect(m_propertyTree, &QPropertyTree::signalSelected, this, &SceneParametersPanel::OnPropertyTreeSelected));
	EXPECTED(connect(m_propertyTree, &QPropertyTree::signalContinuousChange, this, &SceneParametersPanel::OnPropertyTreeContinuousChange));

	layout->addWidget(m_propertyTree, 1);
}

void SceneParametersPanel::OnExplorerEntryModified(ExplorerEntryModifyEvent& ev)
{
	if (!ev.continuousChange)
		m_propertyTree->update();
}

void SceneParametersPanel::OnPropertyTreeSelected()
{
	std::vector<PropertyRow*> rows;
	rows.resize(m_propertyTree->selectedRowCount());
	for (int i = 0; i < rows.size(); ++i)
		rows[i] = m_propertyTree->selectedRowByIndex(i);
	if (rows.empty())
		return;

	vector<const void*> handles;
	for (size_t i = 0; i < rows.size(); ++i)
	{
		PropertyRow* row = rows[i];
		while (row->parent())
		{
			if (row->serializer())
				handles.push_back(row->searchHandle());
			row = row->parent();
		}
	}

	m_ignoreSubselectionChange = true;
	m_system->characterGizmoManager->SetSubselection(GIZMO_LAYER_SCENE, handles);
	m_ignoreSubselectionChange = false;
}

void SceneParametersPanel::OnPropertyTreeChanged()
{
	m_system->scene->SignalChanged(false);
	m_system->scene->CheckIfPlaybackLayersChanged(false);
}

void SceneParametersPanel::OnPropertyTreeContinuousChange()
{
	m_system->scene->CheckIfPlaybackLayersChanged(true);
	m_system->scene->SignalChanged(true);
}

void SceneParametersPanel::OnSceneChanged(bool continuous)
{
	if (!continuous)
		m_propertyTree->revert();
}

void SceneParametersPanel::OnCharacterLoaded()
{
	m_propertyTree->revert();
}

void SceneParametersPanel::OnExplorerSelectionChanged()
{
	// We need to update property tree as NavigatableReference-s may change they
	// look depending on selection.
	m_propertyTree->update();
}

void SceneParametersPanel::OnBlendShapeOptionsChanged()
{
	m_propertyTree->revert();
}

void SceneParametersPanel::OnSubselectionChanged(int layer)
{
	if (layer != GIZMO_LAYER_SCENE)
		return;
	if (m_ignoreSubselectionChange)
		return;

	const vector<const void*>& handles = m_system->characterGizmoManager->Subselection(GIZMO_LAYER_SCENE);
	m_propertyTree->selectByAddresses(handles.data(), handles.size(), true);
}

}

