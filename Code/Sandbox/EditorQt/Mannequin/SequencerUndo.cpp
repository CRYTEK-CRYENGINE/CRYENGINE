// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequencerUndo.h"
#include "ISequencerSystem.h"
#include "SequencerSequence.h"

//////////////////////////////////////////////////////////////////////////
CUndoSequencerSequenceModifyObject::CUndoSequencerSequenceModifyObject(CSequencerTrack* track, CSequencerSequence* pSequence)
{
	// Stores the current state of this track.
	assert(track != 0);

	m_pTrack = track;
	m_pSequence = pSequence;

	// Store undo info.
	m_undo = XmlHelpers::CreateXmlNode("Undo");
	m_pTrack->Serialize(m_undo, false);
}

//////////////////////////////////////////////////////////////////////////
void CUndoSequencerSequenceModifyObject::Undo(bool bUndo)
{
	if (!m_undo)
		return;

	if (bUndo)
	{
		m_redo = XmlHelpers::CreateXmlNode("Redo");
		m_pTrack->Serialize(m_redo, false);
	}
	// Undo track state.
	m_pTrack->Serialize(m_undo, true);

	if (bUndo)
	{
		// Refresh stuff after undo.
		GetIEditorImpl()->UpdateSequencer(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CUndoSequencerSequenceModifyObject::Redo()
{
	if (!m_redo)
		return;

	// Redo track state.
	m_pTrack->Serialize(m_redo, true);

	// Refresh stuff after undo.
	GetIEditorImpl()->UpdateSequencer(true);
}

