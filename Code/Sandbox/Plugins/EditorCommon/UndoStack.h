// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// TODO : bring both CUndoStack and CUndoRedo int a single consistent class

#include <CryCore/Platform/platform.h>
#include "EditorCommonAPI.h"
#include "Expected.h"
#include "EditorCommonAPI.h"
#include <CryCore/Containers/CryArray.h>
#include <vector>

class EDITOR_COMMON_API CUndoStack
{
	typedef DynArray<char> DataBuffer;
	struct SUndoState
	{
		string             description;
		DataBuffer         state;
		unsigned long long sequentialIndex;
	};

public:
	void               PushUndo(DataBuffer* previousContentToMove, const char* description, unsigned long long sequentialIndex);
	bool               Undo(DataBuffer* newState, const DataBuffer& currentState, int count, unsigned long long sequentialIndex);
	bool               HasUndo() const         { return !m_undos.empty(); }
	bool               Redo(DataBuffer* newState, const DataBuffer& currentState, unsigned long long sequentialIndex);
	bool               HasRedo() const         { return !m_redos.empty(); }
	unsigned long long NewestUndoIndex() const { return m_undos.empty() ? 0 : m_undos.back().sequentialIndex; }
	unsigned long long NewestRedoIndex() const { return m_redos.empty() ? 0 : m_redos.back().sequentialIndex; }

	void               GetUndoActions(std::vector<string>* actionNames, int maxActionCount);
private:
	std::vector<SUndoState> m_undos;
	std::vector<SUndoState> m_redos;
};

//////////////////////////////////////////////////////////////////////////

struct IOperator
{
	virtual void Undo() = 0;
	virtual void Redo() = 0;
};

typedef std::shared_ptr<IOperator> PBaseOperator;

class EDITOR_COMMON_API CUndoRedo
{
public:
	CUndoRedo();

	void AddOperator(PBaseOperator op);
	void Undo();
	void Redo();

private:
	std::vector<PBaseOperator> m_undoOperators;
	std::vector<PBaseOperator> m_redoOperators;
};

