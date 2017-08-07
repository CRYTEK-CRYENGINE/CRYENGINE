// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ModeCharacter.h"
#include "ManipScene.h"
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <QToolBar>
#include <QAction>
#include "Expected.h"
#include "DisplayParameters.h"
#include "CharacterDocument.h"
#include "CharacterDefinition.h"
#include "CharacterToolForm.h"
#include "CharacterList.h"
#include "SceneContent.h"
#include "Serialization.h"
#include "GizmoSink.h"
#include "CharacterGizmoManager.h"
#include "CharacterToolSystem.h"
#include "TransformPanel.h"
#include <CryIcon.h>

namespace CharacterTool
{

ModeCharacter::ModeCharacter()
	: m_scene(new Manip::CScene())
	, m_ignoreSceneSelectionChange(false)
	, m_transformPanel()
{
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalUndo()), this, SLOT(OnSceneUndo())));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalRedo()), this, SLOT(OnSceneRedo())));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalElementsChanged(unsigned int)), this, SLOT(OnSceneElementsChanged(unsigned int))));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalElementContinuousChange(unsigned int)), this, SLOT(OnSceneElementContinuousChange(unsigned int))));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalSelectionChanged()), this, SLOT(OnSceneSelectionChanged())));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalPropertiesChanged()), this, SLOT(OnScenePropertiesChanged())));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalManipulationModeChanged()), this, SLOT(OnSceneManipulationModeChanged())));
}

void ModeCharacter::Serialize(Serialization::IArchive& ar)
{
	m_scene->Serialize(ar);
}

void ModeCharacter::EnterMode(const SModeContext& context)
{
	m_character = context.character;
	m_document = context.document;
	m_system = context.system;
	m_transformPanel = context.transformPanel;
	m_layerPropertyTrees = context.layerPropertyTrees;
	m_scene->SetSpaceProvider(m_system->characterSpaceProvider.get());
	m_system->gizmoSink->SetScene(m_scene.get());
	m_system->characterGizmoManager->ReadGizmos();
	m_window = context.window;

	EXPECTED(connect(m_document, SIGNAL(SignalBindPoseModeChanged()), SLOT(OnBindPoseModeChanged())));
	EXPECTED(connect(m_document, SIGNAL(SignalDisplayOptionsChanged(const DisplayOptions &)), SLOT(OnDisplayOptionsChanged())));
	EXPECTED(connect(m_system->characterGizmoManager.get(), SIGNAL(SignalSubselectionChanged(int)), SLOT(OnSubselectionChanged(int))));
	EXPECTED(connect(m_system->characterGizmoManager.get(), SIGNAL(SignalGizmoChanged()), SLOT(OnGizmoChanged())));

	EXPECTED(connect(m_transformPanel, SIGNAL(SignalChanged()), SLOT(OnTransformPanelChanged())));
	EXPECTED(connect(m_transformPanel, SIGNAL(SignalChangeFinished()), SLOT(OnTransformPanelChangeFinished())));
	EXPECTED(connect(m_transformPanel, SIGNAL(SignalSpaceChanged()), SLOT(OnTransformPanelSpaceChanged())));
	m_transformPanel->SetSpace(m_scene->TransformationSpace());
	WriteTransformPanel();

	QToolBar* toolBar = context.toolbar;
	m_actionMoveTool.reset(new QAction(CryIcon("icons:Navigation/Basics_Move.ico"), "Move", 0));
	m_actionMoveTool->setPriority(QAction::LowPriority);
	m_actionMoveTool->setCheckable(true);
	EXPECTED(connect(m_actionMoveTool.get(), SIGNAL(triggered()), this, SLOT(OnActionMoveTool())));
	toolBar->addAction(m_actionMoveTool.get());

	m_actionRotateTool.reset(new QAction(CryIcon("icons:Navigation/Basics_Rotate.ico"), "Rotate", 0));
	m_actionRotateTool->setPriority(QAction::LowPriority);
	m_actionRotateTool->setCheckable(true);
	EXPECTED(connect(m_actionRotateTool.get(), SIGNAL(triggered()), this, SLOT(OnActionRotateTool())));
	toolBar->addAction(m_actionRotateTool.get());

	m_actionScaleTool.reset(new QAction(CryIcon("icons:Navigation/Basics_Scale.ico"), "Scale", 0));
	EXPECTED(connect(m_actionScaleTool.get(), SIGNAL(triggered()), this, SLOT(OnActionScaleTool())));
	m_actionScaleTool->setPriority(QAction::LowPriority);
	m_actionScaleTool->setCheckable(true);
	toolBar->addAction(m_actionScaleTool.get());

	OnSceneManipulationModeChanged();

	UpdateToolbar();
}

void ModeCharacter::LeaveMode()
{
	EXPECTED(disconnect(m_transformPanel, SIGNAL(SignalChanged()), this, SLOT(OnTransformPanelChanged())));
	EXPECTED(disconnect(m_transformPanel, SIGNAL(SignalChangeFinished()), this, SLOT(OnTransformPanelChangeFinished())));
	EXPECTED(disconnect(m_transformPanel, SIGNAL(SignalSpaceChanged()), this, SLOT(OnTransformPanelSpaceChanged())));

	EXPECTED(disconnect(m_document, SIGNAL(SignalBindPoseModeChanged()), this, SLOT(OnBindPoseModeChanged())));
	EXPECTED(disconnect(m_system->characterGizmoManager.get(), SIGNAL(SignalSubselectionChanged(int)), this, SLOT(OnSubselectionChanged(int))));
	EXPECTED(disconnect(m_system->characterGizmoManager.get(), SIGNAL(SignalGizmoChanged()), this, SLOT(OnGizmoChanged())));
	m_system->gizmoSink->SetScene(0);
	m_document = 0;
	m_scene->SetSpaceProvider(0);
	m_layerPropertyTrees.clear();
}

void ModeCharacter::OnTransformPanelChanged()
{
	if (m_scene->TransformationSpace() != Manip::SPACE_LOCAL)
		m_scene->SetSelectionTransform(m_scene->TransformationSpace(), m_transformPanel->Transform());
	OnSceneElementContinuousChange((1 << GIZMO_LAYER_COUNT) - 1);
}

void ModeCharacter::OnTransformPanelSpaceChanged()
{
	m_scene->SetTransformationSpace(m_transformPanel->Space());

	WriteTransformPanel();
}

void ModeCharacter::OnTransformPanelChangeFinished()
{
	m_scene->SetSelectionTransform(m_scene->TransformationSpace(), m_transformPanel->Transform());
	OnSceneElementsChanged((1 << GIZMO_LAYER_COUNT) - 1);
}

void ModeCharacter::OnSubselectionChanged(int layer)
{
	const vector<const void*>& items = m_system->characterGizmoManager->Subselection(GizmoLayer(layer));

	CharacterDefinition* cdf = m_document->GetLoadedCharacterDefinition();
	if (!cdf)
		return;

	Manip::SSelectionSet selection;
	const Manip::SElements& elements = m_scene->Elements();
	for (size_t i = 0; i < elements.size(); ++i)
	{
		bool selected = std::find(items.begin(), items.end(), elements[i].originalHandle) != items.end();
		if (selected)
			selection.Add(elements[i].id);
	}

	m_ignoreSceneSelectionChange = true;
	m_scene->SetSelection(selection);
	m_ignoreSceneSelectionChange = false;
}

void ModeCharacter::OnGizmoChanged()
{
	WriteTransformPanel();
}

void ModeCharacter::OnBindPoseModeChanged()
{
	UpdateToolbar();
}

void ModeCharacter::OnDisplayOptionsChanged()
{
	DisplayOptions* displayOptions = m_system->document->GetDisplayOptions().get();
	m_scene->SetShowAttachementGizmos(displayOptions->attachmentGizmos);
	m_scene->SetShowPoseModifierGizmos(displayOptions->poseModifierGizmos);

	constexpr int animationFlag = (1 << GIZMO_LAYER_ANIMATION);

	if (m_system->document->GetDisplayOptions()->animation.animationEventGizmos)
		m_scene->SetVisibleLayerMask(0xffffffff);
	else
		m_scene->SetVisibleLayerMask(0xffffffff & ~animationFlag);
}

void ModeCharacter::OnSceneSelectionChanged()
{
	if (m_ignoreSceneSelectionChange)
		return;

	if (ExplorerEntry* entry = m_system->gizmoSink->ActiveEntry())
	{
		if (!m_system->document->IsExplorerEntrySelected(entry))
		{
			ExplorerEntries entries;
			entries.push_back(entry);
			m_document->SetSelectedExplorerEntries(entries, CharacterDocument::SELECT_DO_NOT_REWIND);
		}
	}

	vector<const void*> selection;
	GetPropertySelection(&selection);

	for (int layer = 0; layer < GIZMO_LAYER_COUNT; ++layer)
		m_system->characterGizmoManager->SetSubselection((GizmoLayer)layer, selection);

	WriteTransformPanel();
}

void ModeCharacter::WriteTransformPanel()
{
	if (!m_transformPanel)
		return;

	m_transformPanel->SetMode(m_scene->TransformationMode());
	m_transformPanel->SetTransform(m_scene->GetSelectionTransform(m_scene->TransformationSpace()));
	bool enabled = false;
	if (m_scene->TransformationMode() == Manip::MODE_TRANSLATE && m_scene->SelectionCanBeMoved())
		enabled = true;
	if (m_scene->TransformationMode() == Manip::MODE_ROTATE && m_scene->SelectionCanBeRotated())
		enabled = true;
	m_transformPanel->SetEnabled(enabled);
}

void ModeCharacter::OnSceneManipulationModeChanged()
{
	m_actionMoveTool->setChecked(m_scene->TransformationMode() == Manip::MODE_TRANSLATE);
	m_actionRotateTool->setChecked(m_scene->TransformationMode() == Manip::MODE_ROTATE);
	m_actionScaleTool->setChecked(m_scene->TransformationMode() == Manip::MODE_SCALE);

	WriteTransformPanel();
}

static ExplorerEntry* EntryForGizmoLayer(GizmoLayer layer, System* system)
{
	switch (layer)
	{
	case GIZMO_LAYER_CHARACTER:
		return system->document->GetActiveCharacterEntry();
	case GIZMO_LAYER_ANIMATION:
		return system->document->GetActiveAnimationEntry();
	default:
		return 0;
	}
}

void ModeCharacter::HandleSceneChange(int layerBits, bool continuous)
{
	unsigned int bitsLeft = layerBits;
	for (int layer = 0; bitsLeft != 0; bitsLeft = bitsLeft >> 1, ++layer)
	{
		if (layer >= m_layerPropertyTrees.size())
			break;
		QPropertyTree* tree = m_layerPropertyTrees[layer];
		if (bitsLeft & 1)
		{
			if (tree)
			{
				tree->apply(continuous);
				if (continuous)
					tree->revert();
			}
			if (layer == GIZMO_LAYER_SCENE)
			{
				m_system->scene->SignalChanged(continuous);
			}
			else if (ExplorerEntry* entry = EntryForGizmoLayer((GizmoLayer)layer, m_system))
				m_system->explorerData->CheckIfModified(entry, "Transform", continuous);

		}
	}

	if (continuous && layerBits & (1 << GIZMO_LAYER_CHARACTER))
	{
		if (ExplorerEntry* entry = m_system->document->GetActiveCharacterEntry())
		{
			EntryModifiedEvent ev;
			ev.subtree = entry->subtree;
			ev.id = entry->id;
			ev.continuousChange = true;
			m_document->OnCharacterModified(ev);
		}
	}

	WriteTransformPanel();
}

void ModeCharacter::OnSceneElementsChanged(unsigned int layerBits)
{
	HandleSceneChange(layerBits, false);
}

void ModeCharacter::OnSceneElementContinuousChange(unsigned int layerBits)
{
	HandleSceneChange(layerBits, true);
	;
}

void ModeCharacter::OnSceneUndo()
{
	ExplorerEntries entries;
	m_document->GetEntriesActiveInDocument(&entries);
	if (entries.empty())
		return;
	m_system->explorerData->UndoInOrder(entries);
	WriteTransformPanel();
}

void ModeCharacter::OnSceneRedo()
{
	ExplorerEntries entries;
	m_document->GetEntriesActiveInDocument(&entries);
	if (entries.empty())
		return;
	m_system->explorerData->RedoInOrder(entries);
	WriteTransformPanel();
}

void ModeCharacter::OnScenePropertiesChanged()
{
}

void ModeCharacter::GetPropertySelection(vector<const void*>* selection) const
{
	selection->clear();

	if (CharacterDefinition* cdf = m_document->GetLoadedCharacterDefinition())
	{
		Manip::SElements selectedElements;
		m_scene->GetSelectedElements(&selectedElements);
		for (size_t i = 0; i < selectedElements.size(); ++i)
			selection->push_back(selectedElements[i].originalHandle);
	}
}

void ModeCharacter::SetPropertySelection(const vector<const void*>& items)
{
}

void ModeCharacter::OnViewportRender(const SRenderContext& rc)
{
	m_scene->OnViewportRender(rc);
}

void ModeCharacter::OnViewportKey(const SKeyEvent& ev)
{
	m_scene->OnViewportKey(ev);
}

void ModeCharacter::OnViewportMouse(const SMouseEvent& ev)
{
	m_scene->OnViewportMouse(ev);
}

void ModeCharacter::OnActionRotateTool()
{
	m_scene->SetTransformationMode(Manip::MODE_ROTATE);
	OnSceneManipulationModeChanged();
}

void ModeCharacter::OnActionMoveTool()
{
	m_scene->SetTransformationMode(Manip::MODE_TRANSLATE);
	OnSceneManipulationModeChanged();
}

void ModeCharacter::OnActionScaleTool()
{
	m_scene->SetTransformationMode(Manip::MODE_SCALE);
	OnSceneManipulationModeChanged();
}

void ModeCharacter::UpdateToolbar()
{
	if (!m_document)
		return;

	m_actionMoveTool->setEnabled(true);
	m_actionRotateTool->setEnabled(true);
	m_actionScaleTool->setEnabled(true);
}

}
