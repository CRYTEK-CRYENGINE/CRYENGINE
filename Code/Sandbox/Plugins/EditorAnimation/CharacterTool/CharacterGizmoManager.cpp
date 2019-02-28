// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "CharacterGizmoManager.h"
#include "CharacterToolSystem.h"
#include "CharacterDocument.h"
#include "AnimationContent.h"
#include "Expected.h"
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include "QPropertyTree/ContextList.h"
#include "GizmoSink.h"
#include "SceneContent.h"
#include "AnimationList.h"
#include "CharacterDefinition.h"

namespace CharacterTool
{

CharacterGizmoManager::CharacterGizmoManager(System* system)
	: m_system(system)
	, m_contextList(new Serialization::CContextList(system->contextList->Tail()))
	, m_attachedAnimationEntry(0)
{
	m_contextList->Update<Serialization::IGizmoSink>(m_system->gizmoSink.get());

	m_subselections.resize(GIZMO_LAYER_COUNT);
	m_trees.resize(GIZMO_LAYER_COUNT);
	for (int i = 0; i < GIZMO_LAYER_COUNT; ++i)
	{
		unique_ptr<QPropertyTree>& tree = m_trees[i];
		tree.reset(new QPropertyTree(0));
		PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
		treeStyle.propertySplitter = false;
		treeStyle.groupRectangle = false;
		tree->setTreeStyle(treeStyle);
		tree->setUndoEnabled(false);
		tree->setArchiveContext(m_contextList->Tail());

		EXPECTED(connect(tree.get(), &QPropertyTree::signalAboutToSerialize, this, &CharacterGizmoManager::OnTreeAboutToSerialize));
		EXPECTED(connect(tree.get(), &QPropertyTree::signalSerialized, this, &CharacterGizmoManager::OnTreeSerialized));
	}

	EXPECTED(connect(system->document.get(), SIGNAL(SignalActiveCharacterChanged()), SLOT(OnActiveCharacterChanged())));
	EXPECTED(connect(system->document.get(), SIGNAL(SignalCharacterLoaded()), SLOT(OnActiveCharacterChanged())));
	EXPECTED(connect(system->document.get(), SIGNAL(SignalActiveAnimationSwitched()), SLOT(OnActiveAnimationSwitched())));
	EXPECTED(connect(system->explorerData.get(), SIGNAL(SignalEntryModified(ExplorerEntryModifyEvent &)), SLOT(OnExplorerEntryModified(ExplorerEntryModifyEvent &))));
	EXPECTED(connect(system->explorerData.get(), SIGNAL(SignalBeginRemoveEntry(ExplorerEntry*)), SLOT(OnExplorerBeginRemoveEntry(ExplorerEntry*))));
	EXPECTED(connect(system->scene.get(), SIGNAL(SignalChanged(bool)), SLOT(OnSceneChanged())));
	EXPECTED(connect(system->scene.get(), SIGNAL(SignalCharacterChanged()), SLOT(OnSceneChanged())));
}

void CharacterGizmoManager::OnTreeAboutToSerialize(Serialization::IArchive& ar)
{
	QObject* tree = sender();
	int layer = -1;
	for (int i = 0; i < m_trees.size(); ++i)
	{
		if (tree == m_trees[i].get())
		{
			layer = i;
			break;
		}
	}
	if (layer == -1)
		return;
	ExplorerEntry* entry = 0;

	if (ar.isInput())
		m_system->gizmoSink->BeginRead((GizmoLayer)layer);
	else
		m_system->gizmoSink->BeginWrite(entry, (GizmoLayer)layer);
}

void CharacterGizmoManager::OnTreeSerialized(Serialization::IArchive& ar)
{
	if (ar.isInput())
		m_system->gizmoSink->EndRead();
	SignalGizmoChanged();
}

void CharacterGizmoManager::OnActiveCharacterChanged()
{
	CharacterDefinition* loadedCharacter = m_system->document->GetLoadedCharacterDefinition();
	if (loadedCharacter)
		m_trees[GIZMO_LAYER_CHARACTER]->attach(Serialization::SStruct(*loadedCharacter));
	else
	{
		m_trees[GIZMO_LAYER_CHARACTER]->detach();
		m_system->gizmoSink->Clear(GIZMO_LAYER_CHARACTER);
	}
}

void CharacterGizmoManager::OnActiveAnimationSwitched()
{
	ExplorerEntry* activeAnimationEntry = m_system->document->GetActiveAnimationEntry();

	m_attachedAnimationEntry = activeAnimationEntry;

	SEntry<AnimationContent>* animationContent = 0;
	if (activeAnimationEntry)
		animationContent = m_system->animationList->GetEntry(activeAnimationEntry->id);

	if (animationContent)
		m_trees[GIZMO_LAYER_ANIMATION]->attach(Serialization::SStruct(*animationContent));
	else
		m_trees[GIZMO_LAYER_ANIMATION]->detach();
}

void CharacterGizmoManager::OnExplorerBeginRemoveEntry(ExplorerEntry* entry)
{
	if (entry == m_attachedAnimationEntry)
		OnActiveAnimationSwitched();
}

void CharacterGizmoManager::OnExplorerEntryModified(ExplorerEntryModifyEvent& ev)
{
	if ((ev.entryParts & ENTRY_PART_CONTENT) == 0)
		return;
	if (!ev.entry)
		return;
	if (!ev.continuousChange)
	{
		if (ev.entry == m_system->document->GetActiveAnimationEntry())
			m_trees[GIZMO_LAYER_ANIMATION]->revert();
	}
}

QPropertyTree* CharacterGizmoManager::Tree(GizmoLayer layer)
{
	if (size_t(layer) >= m_trees.size())
		return 0;

	if(layer == GIZMO_LAYER_SCENE && !m_trees[GIZMO_LAYER_SCENE]->attached())
		m_trees[GIZMO_LAYER_SCENE]->attach(Serialization::SStruct(*m_system->scene));

	return m_trees[size_t(layer)].get();
}

void CharacterGizmoManager::SetSubselection(GizmoLayer layer, const SelectionHandles& handles)
{
	if (size_t(layer) >= m_subselections.size())
		return;
	int index = int(layer);
	m_subselections[index] = handles;
	SignalSubselectionChanged(index);
}

const CharacterGizmoManager::SelectionHandles& CharacterGizmoManager::Subselection(GizmoLayer layer) const
{
	size_t index = size_t(layer);
	if (index >= m_subselections.size())
	{
		static SelectionHandles wrong;
		return wrong;
	}
	return m_subselections[index];
}

void CharacterGizmoManager::OnSceneChanged()
{
	if (m_trees[GIZMO_LAYER_SCENE])
		m_trees[GIZMO_LAYER_SCENE]->revert();
}

void CharacterGizmoManager::ReadGizmos()
{
	for (int i = 0; i < (int)m_trees.size(); ++i)
	{
		QPropertyTree* tree = m_trees[i].get();
		if (!tree)
			continue;
		tree->revert();
	}
}

}

