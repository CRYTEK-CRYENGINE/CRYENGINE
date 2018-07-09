// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewUndo.h"
#include "TrackViewSequenceManager.h"
#include "Nodes/TrackViewSequence.h"
#include "Nodes/TrackViewTrack.h"
#include "Nodes/TrackViewEntityNode.h"
#include "TrackViewPlugin.h"
#include "Objects/SequenceObject.h"
#include "Objects/EntityObject.h"

CUndoSequenceSettings::CUndoSequenceSettings(CTrackViewSequence* pSequence)
	: m_pSequence(pSequence)
	, m_oldTimeRange(pSequence->GetTimeRange())
	, m_oldPlaybackRange(pSequence->GetPlaybackRange())
	, m_oldFlags(pSequence->GetFlags())
{
}

void CUndoSequenceSettings::Undo(bool bUndo)
{
	m_newTimeRange = m_pSequence->GetTimeRange();
	m_newPlaybackRange = m_pSequence->GetPlaybackRange();
	m_newFlags = m_pSequence->GetFlags();

	m_pSequence->SetTimeRange(m_oldTimeRange);
	m_pSequence->SetPlaybackRange(m_oldPlaybackRange);
	m_pSequence->SetFlags(m_oldFlags);
}

void CUndoSequenceSettings::Redo()
{
	m_pSequence->SetTimeRange(m_newTimeRange);
	m_pSequence->SetPlaybackRange(m_newPlaybackRange);
	m_pSequence->SetFlags(m_newFlags);
}

CUndoAnimKeySelection::CUndoAnimKeySelection(CTrackViewSequence* pSequence)
	: m_pSequence(pSequence)
{
	// Stores the current state of this sequence.
	assert(pSequence);

	// Save sequence name and key selection states
	m_undoKeyStates = SaveKeyStates(pSequence);
}

CUndoAnimKeySelection::CUndoAnimKeySelection(CTrackViewTrack* pTrack)
	: m_pSequence(pTrack->GetSequence())
{
	// This is called from CUndoTrackObject which will save key states itself
}

void CUndoAnimKeySelection::Undo(bool bUndo)
{
	{
		CTrackViewSequenceNoNotificationContext context(m_pSequence);

		// Save key selection states for redo if necessary
		if (bUndo)
		{
			m_redoKeyStates = SaveKeyStates(m_pSequence);
		}

		// Undo key selection state
		RestoreKeyStates(m_pSequence, m_undoKeyStates);
	}
}

void CUndoAnimKeySelection::Redo()
{
	{
		CTrackViewSequenceNoNotificationContext context(m_pSequence);

		// Redo key selection state
		RestoreKeyStates(m_pSequence, m_redoKeyStates);
	}
}

std::vector<bool> CUndoAnimKeySelection::SaveKeyStates(CTrackViewSequence* pSequence) const
{
	CTrackViewKeyBundle keys = pSequence->GetAllKeys();
	const unsigned int numkeys = keys.GetKeyCount();

	std::vector<bool> selectionState;
	selectionState.reserve(numkeys);

	for (unsigned int i = 0; i < numkeys; ++i)
	{
		CTrackViewKeyHandle& keyHandle = keys.GetKey(i);
		selectionState.push_back(keyHandle.IsSelected());
	}

	return selectionState;
}

void CUndoAnimKeySelection::RestoreKeyStates(CTrackViewSequence* pSequence, const std::vector<bool> keyStates)
{
	CTrackViewKeyBundle keys = pSequence->GetAllKeys();
	const unsigned int numkeys = keys.GetKeyCount();

	CTrackViewSequenceNotificationContext context(pSequence);
	for (unsigned int i = 0; i < numkeys; ++i)
	{
		CTrackViewKeyHandle& keyHandle = keys.GetKey(i);
		keyHandle.Select(keyStates[i]);
	}
}

bool CUndoAnimKeySelection::IsSelectionChanged() const
{
	const std::vector<bool> currentKeyState = SaveKeyStates(m_pSequence);
	return m_undoKeyStates != currentKeyState;
}

CUndoTrackObject::CUndoTrackObject(CTrackViewTrack* pTrack, bool bStoreKeySelection)
	: CUndoAnimKeySelection(pTrack), m_pTrack(pTrack), m_bStoreKeySelection(bStoreKeySelection)
{
	assert(pTrack);

	if (m_bStoreKeySelection)
	{
		m_undoKeyStates = SaveKeyStates(m_pSequence);
	}

	// Stores the current state of this track.
	assert(pTrack != 0);

	// Store undo info.
	m_undo = pTrack->GetMemento();

	CSequenceObject* pSequenceObject = m_pSequence->GetSequenceObject();

	assert(pSequenceObject);
	if (pSequenceObject)
	{
		pSequenceObject->SetLayerModified();
	}
}

void CUndoTrackObject::Undo(bool bUndo)
{
	assert(m_pSequence);

	{
		CTrackViewSequenceNoNotificationContext context(m_pSequence);

		if (bUndo)
		{
			m_redo = m_pTrack->GetMemento();

			if (m_bStoreKeySelection)
			{
				// Save key selection states for redo if necessary
				m_redoKeyStates = SaveKeyStates(m_pSequence);
			}
		}

		// Undo track state.
		m_pTrack->RestoreFromMemento(m_undo);

		CSequenceObject* pSequenceObject = m_pSequence->GetSequenceObject();

		assert(pSequenceObject);
		if (pSequenceObject && bUndo)
		{
			pSequenceObject->SetLayerModified();
		}

		if (m_bStoreKeySelection)
		{
			// Undo key selection state
			RestoreKeyStates(m_pSequence, m_undoKeyStates);
		}
	}

	if (bUndo)
	{
		m_pSequence->OnNodeChanged(m_pTrack, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
	}
	else
	{
		m_pSequence->ForceAnimation();
	}
}

void CUndoTrackObject::Redo()
{
	assert(m_pSequence);

	// Redo track state.
	m_pTrack->RestoreFromMemento(m_redo);

	CSequenceObject* pSequenceObject = m_pSequence->GetSequenceObject();

	assert(pSequenceObject);
	if (pSequenceObject)
	{
		pSequenceObject->SetLayerModified();
	}

	if (m_bStoreKeySelection)
	{
		// Redo key selection state
		RestoreKeyStates(m_pSequence, m_redoKeyStates);
	}

	m_pSequence->OnNodeChanged(m_pTrack, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
}

CAbstractUndoSequenceTransaction::CAbstractUndoSequenceTransaction(CTrackViewSequence* pSequence)
	: m_pSequence(pSequence)
{
}

void CAbstractUndoSequenceTransaction::AddSequence()
{
	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();

	// Add sequence back to CryMovie
	IMovieSystem* pMovieSystem = GetIEditor()->GetMovieSystem();
	pMovieSystem->AddSequence(m_pSequence->m_pAnimSequence);

	// Release ownership and add node back to CryMovie
	pSequenceManager->m_sequences.push_back(std::move(m_pStoredTrackViewSequence));

	pSequenceManager->OnSequenceAdded(m_pSequence);
}

void CAbstractUndoSequenceTransaction::RemoveSequence(bool bAquireOwnership)
{
	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();

	for (auto iter = pSequenceManager->m_sequences.begin(); iter != pSequenceManager->m_sequences.end(); ++iter)
	{
		std::unique_ptr<CTrackViewSequence>& currentSequence = *iter;

		if (currentSequence.get() == m_pSequence)
		{
			if (bAquireOwnership)
			{
				// Acquire ownership of sequence
				currentSequence.swap(m_pStoredTrackViewSequence);
				assert(m_pStoredTrackViewSequence.get());
			}

			// Remove from CryMovie and TrackView
			pSequenceManager->m_sequences.erase(iter);
			IMovieSystem* pMovieSystem = GetIEditor()->GetMovieSystem();
			pMovieSystem->RemoveSequence(m_pSequence->m_pAnimSequence);

			break;
		}
	}

	pSequenceManager->OnSequenceRemoved(m_pSequence);
}

void CUndoSequenceAdd::Undo(bool bUndo)
{
	RemoveSequence(bUndo);
}

void CUndoSequenceAdd::Redo()
{
	AddSequence();
}

CUndoSequenceRemove::CUndoSequenceRemove(CTrackViewSequence* pRemovedSequence)
	: CAbstractUndoSequenceTransaction(pRemovedSequence)
{
	RemoveSequence(true);
}

void CUndoSequenceRemove::Undo(bool bUndo)
{
	AddSequence();
}

void CUndoSequenceRemove::Redo()
{
	RemoveSequence(true);
}

CAbstractUndoAnimNodeTransaction::CAbstractUndoAnimNodeTransaction(CTrackViewAnimNode* pNode)
	: m_pParentNode(static_cast<CTrackViewAnimNode*>(pNode->GetParentNode())), m_pNode(pNode)
{
}

void CAbstractUndoAnimNodeTransaction::AddNode()
{
	// Add node back to sequence
	m_pParentNode->m_pAnimSequence->AddNode(m_pNode->m_pAnimNode);

	// Release ownership and add node back to parent node
	CTrackViewNode* pNode = m_pStoredTrackViewNode.release();
	m_pParentNode->AddNode(m_pNode);

	m_pNode->BindToEditorObjects();

	CryLog("Re-Added AnimNode '%s' to Sequence '%s'", m_pNode->GetName(), m_pNode->GetSequence()->GetName());
}

void CAbstractUndoAnimNodeTransaction::RemoveNode(bool bAquireOwnership)
{
	m_pNode->UnBindFromEditorObjects();

	for (auto iter = m_pParentNode->m_childNodes.begin(); iter != m_pParentNode->m_childNodes.end(); ++iter)
	{
		std::unique_ptr<CTrackViewNode>& currentNode = *iter;

		if (currentNode.get() == m_pNode)
		{
			if (bAquireOwnership)
			{
				// Acquire ownership of node
				currentNode.swap(m_pStoredTrackViewNode);
				assert(m_pStoredTrackViewNode.get());
			}

			// Remove from parent node and sequence
			m_pParentNode->m_childNodes.erase(iter);
			m_pParentNode->m_pAnimSequence->RemoveNode(m_pNode->m_pAnimNode);

			CryLog("Removed AnimNode '%s' from Sequence '%s'", m_pNode->GetName(), m_pNode->GetSequence()->GetName());

			break;
		}
	}

	m_pNode->GetSequence()->OnNodeChanged(m_pNode, ITrackViewSequenceListener::eNodeChangeType_Removed);
}

void CUndoAnimNodeAdd::Undo(bool bUndo)
{
	RemoveNode(bUndo);
}

void CUndoAnimNodeAdd::Redo()
{
	AddNode();
}

CUndoAnimNodeRemove::CUndoAnimNodeRemove(CTrackViewAnimNode* pRemovedNode) : CAbstractUndoAnimNodeTransaction(pRemovedNode)
{
	RemoveNode(true);
}

void CUndoAnimNodeRemove::Undo(bool bUndo)
{
	AddNode();
}

void CUndoAnimNodeRemove::Redo()
{
	RemoveNode(true);
}

CUndoDetachAnimEntity::CUndoDetachAnimEntity(CTrackViewEntityNode* pEntityNode)
	: m_pEntityNode(pEntityNode)
{
	DetachEntityObject();
}

void CUndoDetachAnimEntity::Undo(bool bUndo)
{
	AttachEntityObject();
}

void CUndoDetachAnimEntity::Redo()
{
	DetachEntityObject();
}

void CUndoDetachAnimEntity::AttachEntityObject()
{
	m_pEntityNode->SetNodeEntity(CEntityObject::FindFromEntityId(m_entityId));
	m_pEntityNode->BindToEditorObjects();
}

void CUndoDetachAnimEntity::DetachEntityObject()
{
	CEntityObject* entityObject = m_pEntityNode->GetNodeEntity();
	if (entityObject)
	{
		m_entityId = entityObject->GetEntityId(); //save entityId for Redo
	}

	m_pEntityNode->UnBindFromEditorObjects();
	m_pEntityNode->SetNodeEntity(nullptr);
}

CAbstractUndoTrackTransaction::CAbstractUndoTrackTransaction(CTrackViewTrack* pTrack)
	: m_pParentNode(pTrack->GetAnimNode()), m_pTrack(pTrack)
{
	assert(!pTrack->IsSubTrack());
}

void CAbstractUndoTrackTransaction::AddTrack()
{
	// Add node back to sequence
	m_pParentNode->m_pAnimNode->AddTrack(m_pTrack->m_pAnimTrack);

	// Add track back to parent node and release ownership
	CTrackViewNode* pTrack = m_pStoredTrackViewTrack.release();
	m_pParentNode->AddNode(pTrack);
}

void CAbstractUndoTrackTransaction::RemoveTrack(bool bAquireOwnership)
{
	for (auto iter = m_pParentNode->m_childNodes.begin(); iter != m_pParentNode->m_childNodes.end(); ++iter)
	{
		std::unique_ptr<CTrackViewNode>& currentNode = *iter;

		if (currentNode.get() == m_pTrack)
		{
			if (bAquireOwnership)
			{
				// Acquire ownership of track
				currentNode.swap(m_pStoredTrackViewTrack);
			}

			// Remove from parent node and sequence
			m_pParentNode->m_childNodes.erase(iter);
			m_pParentNode->m_pAnimNode->RemoveTrack(m_pTrack->m_pAnimTrack);

			break;
		}
	}

	m_pParentNode->GetSequence()->OnNodeChanged(m_pStoredTrackViewTrack.get(), ITrackViewSequenceListener::eNodeChangeType_Removed);
}

void CUndoTrackAdd::Undo(bool bUndo)
{
	RemoveTrack(bUndo);
}

void CUndoTrackAdd::Redo()
{
	AddTrack();
}

CUndoTrackRemove::CUndoTrackRemove(CTrackViewTrack* pRemovedTrack)
	: CAbstractUndoTrackTransaction(pRemovedTrack)
{
	RemoveTrack(true);
}

void CUndoTrackRemove::Undo(bool bUndo)
{
	AddTrack();
}

void CUndoTrackRemove::Redo()
{
	RemoveTrack(true);
}

CUndoAnimNodeReparent::CUndoAnimNodeReparent(CTrackViewAnimNode* pAnimNode, CTrackViewAnimNode* pNewParent)
	: CAbstractUndoAnimNodeTransaction(pAnimNode), m_pNewParent(pNewParent), m_pOldParent(m_pParentNode)
{
	CTrackViewSequence* pSequence = pAnimNode->GetSequence();
	assert(pSequence == m_pNewParent->GetSequence() && pSequence == m_pOldParent->GetSequence());

	Reparent(pNewParent);
}

void CUndoAnimNodeReparent::Undo(bool bUndo)
{
	Reparent(m_pOldParent);
}

void CUndoAnimNodeReparent::Redo()
{
	Reparent(m_pNewParent);
}

void CUndoAnimNodeReparent::Reparent(CTrackViewAnimNode* pNewParent)
{
	RemoveNode(true);
	m_pParentNode = pNewParent;
	m_pNode->m_pAnimNode->SetParent(pNewParent->m_pAnimNode);
	AddParentsInChildren(m_pNode);
	AddNode();

	// This undo object must never have ownership of the node
	assert(m_pStoredTrackViewNode.get() == nullptr);
}

void CUndoAnimNodeReparent::AddParentsInChildren(CTrackViewAnimNode* pCurrentNode)
{
	const uint numChildren = pCurrentNode->GetChildCount();

	for (uint childIndex = 0; childIndex < numChildren; ++childIndex)
	{
		CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pCurrentNode->GetChild(childIndex));

		if (pChildAnimNode->GetNodeType() != eTVNT_Track)
		{
			pChildAnimNode->m_pAnimNode->SetParent(pCurrentNode->m_pAnimNode);

			if (pChildAnimNode->GetChildCount() > 0 && pChildAnimNode->GetNodeType() != eTVNT_AnimNode)
			{
				AddParentsInChildren(pChildAnimNode);
			}
		}
	}
}

CUndoAnimNodeRename::CUndoAnimNodeRename(CTrackViewAnimNode* pNode, const string& oldName) :
	m_pNode(pNode), m_newName(pNode->GetName()), m_oldName(oldName)
{
}

void CUndoAnimNodeRename::Undo(bool bUndo)
{
	m_pNode->SetName(m_oldName);
}

void CUndoAnimNodeRename::Redo()
{
	m_pNode->SetName(m_newName);
}

CUndoTrackNodeDisable::CUndoTrackNodeDisable(CTrackViewNode* pNode, bool oldState) :
	m_pNode(pNode), m_newState(pNode->IsDisabled()), m_oldState(oldState)
{
}

void CUndoTrackNodeDisable::Undo(bool bUndo)
{
	m_pNode->SetDisabled(m_oldState);
}

void CUndoTrackNodeDisable::Redo()
{
	m_pNode->SetDisabled(m_newState);
}

