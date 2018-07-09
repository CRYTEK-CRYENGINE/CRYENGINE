// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>

struct IUndoObject;
class ICrySizer;
class CUndoStep;

struct IUndoManagerListener
{
	virtual void BeginUndoTransaction() {}
	virtual void EndUndoTransaction() {}
	virtual void BeginRestoreTransaction() {}
	virtual void EndRestoreTransaction() {}
};

struct IUndoManager
{
	enum ECommandChangeType
	{
		eCommandChangeType_Undo = BIT(1),
		eCommandChangeType_Redo = BIT(2),
		eCommandChangeType_Insert = BIT(3),
		eCommandChangeType_Remove = BIT(4),
		eCommandChangeType_Move = BIT(5),
	};

	virtual ~IUndoManager() {}

	CCrySignal<void(CUndoStep*, IUndoManager::ECommandChangeType, int)> signalBufferChanged;

	//! Begin operation requiring undo.
	//! Undo manager enters holding state.
	virtual void Begin() = 0;
	//! Restore all undo objects registered since last Begin call.
	//! @param bUndo if true all Undo object registered up to this point will be undone.
	virtual void Restore(bool bUndo = true) = 0;
	//! Accept changes and registers an undo object with the undo manager.
	//! This will allow the user to undo the operation.
	virtual void Accept(const char* name) = 0;
	//! Cancel changes and restore undo objects.
	virtual void Cancel() = 0;

	//! Temporarily suspends recording of undo.
	virtual void Suspend() = 0;
	//! Resume recording if was suspended.
	virtual void Resume() = 0;

	// Undo last operation.
	virtual void Undo(int numSteps = 1) = 0;

	//! Redo last undo.
	virtual void Redo(int numSteps = 1) = 0;

	//! Check if undo information is recording now.
	virtual bool IsUndoRecording() const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual bool IsUndoSuspended() const = 0;
	virtual bool IsUndoTransaction() const = 0;

	//! Put new undo object, must be called between Begin and Accept/Cancel methods.
	virtual void RecordUndo(IUndoObject* obj) = 0;

	virtual bool IsHaveUndo() const = 0;
	virtual bool IsHaveRedo() const = 0;

	//! Returns length of undo stack.
	virtual int GetUndoStackLen() const = 0;
	//! Returns length of redo stack.
	virtual int GetRedoStackLen() const = 0;

	//! Retrieves undo object name.
	virtual void GetCommandName(CUndoStep* pStep, string& name) = 0;

	//! Completely flush all Undo and redo buffers.
	//! Must be done on level reloads or global Fetch operation.
	virtual void Flush() = 0;

	virtual void ClearRedoStack() = 0;
	virtual void ClearUndoStack() = 0;

	virtual void ClearUndoStack(int num) = 0;
	virtual void ClearRedoStack(int num) = 0;

	virtual void AddListener(IUndoManagerListener* pListener) = 0;
	virtual void RemoveListener(IUndoManagerListener* pListener) = 0;

	virtual const std::list<CUndoStep*>& GetUndoStack() const = 0;
	virtual const std::list<CUndoStep*>& GetRedoStack() const = 0;
};


