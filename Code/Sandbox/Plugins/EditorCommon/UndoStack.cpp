// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "UndoStack.h"
#include "Expected.h"

void CUndoStack::PushUndo(DataBuffer* previousContentToMove, const char* description, unsigned long long sequentialIndex)
{
	assert(description && description[0]);
	m_undos.push_back(SUndoState());
	SUndoState& back = m_undos.back();
	back.state.swap(*previousContentToMove);
	back.description = description;
	back.sequentialIndex = sequentialIndex;

	m_redos.clear();
}

bool CUndoStack::Undo(DataBuffer* newState, const DataBuffer& firstState, int count, unsigned long long redoSequentialIndex)
{
	if (m_undos.empty())
		return false;
	if (count <= 0)
		return false;

	DataBuffer lastState;

	for (int i = 0; i < count; ++i)
	{
		if (m_undos.empty())
			break;
		SUndoState& undo = m_undos.back();
		lastState.swap(undo.state);

		m_redos.push_back(SUndoState());
		SUndoState& redo = m_redos.back();
		redo.description = undo.description;
		redo.state = i == 0 ? firstState : lastState;
		redo.sequentialIndex = redoSequentialIndex;

		m_undos.pop_back();
	}
	newState->swap(lastState);
	return true;
}

bool CUndoStack::Redo(DataBuffer* newState, const DataBuffer& currentState, unsigned long long undoSequentialIndex)
{
	if (m_redos.empty())
		return false;

	SUndoState& redo = m_redos.back();
	newState->swap(redo.state);

	m_undos.push_back(SUndoState());
	SUndoState& undo = m_undos.back();
	undo.description = redo.description;
	undo.state = currentState;
	undo.sequentialIndex = undoSequentialIndex;

	m_redos.pop_back();
	return true;
}

void CUndoStack::GetUndoActions(std::vector<string>* actionNames, int maxActionCount)
{
	actionNames->clear();
	int count = m_undos.size();
	int lastAction = count - maxActionCount < 0 ? 0 : count - maxActionCount;
	for (int i = count - 1; i >= lastAction; --i)
	{
		actionNames->push_back(m_undos[i].description);
	}
}

//////////////////////////////////////////////////////////////////////////

CUndoRedo::CUndoRedo()
{
}

void CUndoRedo::AddOperator(PBaseOperator op)
{
	m_undoOperators.push_back(op);
	m_redoOperators.clear();
}

void CUndoRedo::Undo()
{
	if (m_undoOperators.empty())
		return;
	m_undoOperators.back()->Undo();
	m_redoOperators.push_back(m_undoOperators.back());
	m_undoOperators.pop_back();
}

void CUndoRedo::Redo()
{
	if (m_redoOperators.empty())
		return;
	m_redoOperators.back()->Redo();
	m_undoOperators.push_back(m_redoOperators.back());
	m_redoOperators.pop_back();
}

