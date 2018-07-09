// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"
#include <CrySandbox/CrySignal.h>
#include "IUndoManager.h"

struct IUndoObject;
class CUndoStep;
class CSuperUndoStep;

/*!
 *	CUndoManager is keeping and operating on CUndo class instances.
 */
class CUndoManager : public IUndoManager
{
public:
	CUndoManager();
	~CUndoManager();

	//! Begin operation requiring undo.
	//! Undo manager enters holding state.
	void Begin();
	//! Accept changes and registers an undo object with the undo manager.
	//! This will allow the user to undo the operation.
	void Accept(const char* name);
	//! Cancel changes and restore undo objects.
	void Cancel();

	//! Normally this is NOT needed but in special cases this can be useful.
	//! This allows to group a set of Begin()/Accept() sequences to be undone in one operation.
	void SuperBegin();
	//! When a SuperBegin() used, this method is used to Accept.
	//! This leaves the undo database in its modified state and registers the IUndoObjects with the undo system.
	//! This will allow the user to undo the operation.
	void SuperAccept(const string& name);
	//! Cancel changes and restore undo objects.
	void SuperCancel();

	//! Temporarily suspends recording of undo.
	void Suspend();
	//! Resume recording if was suspended.
	void Resume();

	// Undo last operation.
	void Undo(int numSteps = 1);

	//! Redo last undo.
	void Redo(int numSteps = 1);

	//! Check if undo information is recording now.
	bool IsUndoRecording() const { return (m_bRecording || m_bSuperRecording) && m_suspendCount == 0; };

	//////////////////////////////////////////////////////////////////////////
	bool IsUndoSuspended() const   { return m_suspendCount != 0; };
	bool IsUndoTransaction() const { return m_bUndoing || m_bRedoing; }

	//! Put new undo object, must be called between Begin and Accept/Cancel methods.
	void RecordUndo(IUndoObject* obj);

	bool IsHaveUndo() const;
	bool IsHaveRedo() const;

	//! Returns length of undo stack.
	int GetUndoStackLen() const;
	//! Returns length of redo stack.
	int GetRedoStackLen() const;

	//! Retrieves undo object name.
	void GetCommandName(CUndoStep* pStep, string& name);

	//! Completely flush all Undo and redo buffers.
	//! Must be done on level reloads or global Fetch operation.
	void Flush();

	//! Clear all undo objects registered since last Begin call.
	//! @param bUndo if true all Undo object registered up to this point will be undone.
	void Restore(bool bUndo = true);

	//! Clears all undo objects in redo stack
	void ClearRedoStack();

	//! Clears all undo objects in undo stack
	void ClearUndoStack();

	//! Clears num undo objects in redo stack
	void ClearRedoStack(int num);

	//! Clears num undo objects in undo stack
	void ClearUndoStack(int num);

	void                         AddListener(IUndoManagerListener* pListener);
	void                         RemoveListener(IUndoManagerListener* pListener);

	const std::list<CUndoStep*>& GetUndoStack() const { return m_undoStack; }
	const std::list<CUndoStep*>& GetRedoStack() const { return m_redoStack; }

private: // ---------------------------------------------------------------

	void BeginUndoTransaction();
	void EndUndoTransaction();
	void BeginRestoreTransaction();
	void EndRestoreTransaction();

	bool                               m_bRecording;
	bool                               m_bSuperRecording;
	int                                m_suspendCount;

	bool                               m_bUndoing;
	bool                               m_bRedoing;

	CUndoStep*                         m_currentUndo;
	//! Undo step object created by SuperBegin.
	CSuperUndoStep*                    m_superUndo;
	std::list<CUndoStep*>              m_undoStack;
	std::list<CUndoStep*>              m_redoStack;

	std::vector<IUndoManagerListener*> m_listeners;
};


