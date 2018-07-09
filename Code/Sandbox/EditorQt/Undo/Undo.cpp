// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryMemory/CrySizer.h>             // ICrySizer
#include "Undo.h"
#include "IUndoObject.h"
#include "Objects\ObjectManager.h"
#include "CryEditDoc.h"

#define UNDOREDO_BUTTON_POPUP_TEXT_WIDTH 81
#define UNDOREDO_MULTIPLE_OBJECTS_TEXT   " (Multiple Objects)"

//! CUndo is a collection of IUndoObjects instances that forms single undo step.
class CUndoStep
{
public:
	CUndoStep() {};
	virtual ~CUndoStep() { ClearObjects(); }

	//! Set undo object name.
	void           SetName(const string& name) { m_name = name; };
	//! Get undo object name.
	const string& GetName()                    { return m_name; };

	//! Add new undo object to undo step.
	void AddUndoObject(IUndoObject* o)
	{
		// Because we don't have granular UNDO for flow graphs (we always serialize the whole FG every time!)
		// there is no point of stacking multiple flowgraph undo steps.
		// E.g. |paste a node| => create node -> move node to position. So we actually want to record only the
		// state before the create node (the whole graph without the node in it).
		// TODO: Implement a complete set of FG undo objects (For now this looks like a HACK)
		if (!strcmpi(o->GetDescription(), "FlowGraph Undo"))
		{
			for (auto i = 0; i < m_undoObjects.size(); ++i)
			{
				if (!strcmpi(m_undoObjects[i]->GetDescription(), "FlowGraph Undo"))
				{
					return;
				}
			}
		}

		m_undoObjects.push_back(o);
	}
	void ClearObjects()
	{
		int i;
		// Release all undo objects.
		for (i = 0; i < m_undoObjects.size(); i++)
		{
			m_undoObjects[i]->Release();
		}
		m_undoObjects.clear();
	};
	virtual bool IsEmpty() const { return m_undoObjects.empty(); };
	virtual void Undo(bool bUndo)
	{
		for (int i = m_undoObjects.size() - 1; i >= 0; i--)
		{
			m_undoObjects[i]->Undo(bUndo);
		}
	}
	virtual void Redo()
	{
		for (int i = 0; i < m_undoObjects.size(); i++)
		{
			m_undoObjects[i]->Redo();
		}
	}

	const string GetObjectNames()
	{
		string objNamesStr("");
		std::vector<string> objNames;

		bool bFirst = true;
		for (auto pUndoObj : m_undoObjects)
		{
			if (!pUndoObj)
				continue;

			string objName = string(pUndoObj->GetObjectName());
			if (objName.IsEmpty())
				continue;

			if (stl::find(objNames, objName))
				continue;

			if (bFirst == false)
			{
				objNamesStr += ",";
			}
			else
			{
				bFirst = false;
			}

			objNamesStr += objName;

			objNames.emplace_back(std::move(objName));
		}

		return objNamesStr;
	};

private: // ------------------------------------------------------

	friend class CUndoManager;
	string                   m_name;
	// Undo objects registered for this step.
	std::vector<IUndoObject*> m_undoObjects;
};

//! CSuperUndo objects groups together a block of UndoStepto to allow them to be Undo by single operation.
class CSuperUndoStep : public CUndoStep
{
public:
	//! Add new undo object to undo step.
	void AddUndoStep(CUndoStep* step)
	{
		m_undoSteps.push_back(step);
	}
	virtual bool IsEmpty() const
	{
		return m_undoSteps.empty();
	}
	virtual void Undo(bool bUndo)
	{
		for (int i = m_undoSteps.size() - 1; i >= 0; i--)
		{
			m_undoSteps[i]->Undo(bUndo);
		}
	}
	virtual void Redo()
	{
		for (int i = 0; i < m_undoSteps.size(); i++)
		{
			m_undoSteps[i]->Redo();
		}
	}

private:
	//! Undo steps included in this step.
	std::vector<CUndoStep*> m_undoSteps;
};

//////////////////////////////////////////////////////////////////////////
CUndoManager::CUndoManager()
{
	m_bRecording = false;
	m_bSuperRecording = false;

	m_currentUndo = 0;
	m_superUndo = 0;

	m_suspendCount = 0;

	m_bUndoing = false;
	m_bRedoing = false;
}

//////////////////////////////////////////////////////////////////////////
CUndoManager::~CUndoManager()
{
	m_bRecording = false;
	ClearRedoStack();
	ClearUndoStack();

	delete m_superUndo;
	delete m_currentUndo;
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Begin()
{
	//CryLog( "<Undo> Begin SuspendCount=%d",m_suspendCount );
	//if (m_bSuperRecording)
	//CryLog( "<Undo> Begin (Inside SuperSuper)" );
	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	//	assert( m_bRecording == false );

	if (m_bRecording)
	{
		//CLogFile::WriteLine( "<Undo> Begin (already recording)" );
		// Not cancel, just combine.
		return;
	}

	// Begin Creates a new undo object.
	m_currentUndo = new CUndoStep;

	m_bRecording = true;
	//CLogFile::WriteLine( "<Undo> Begin OK" );
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Restore(bool bUndo)
{
	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	if (m_currentUndo)
	{
		BeginRestoreTransaction();
		Suspend();
		if (bUndo && m_currentUndo)
			m_currentUndo->Undo(false); // Undo not by Undo command (no need to store Redo)
		Resume();
		if (m_currentUndo)
			m_currentUndo->ClearObjects();
		EndRestoreTransaction();
	}
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Accept(const char* szName)
{
	//CryLog( "<Undo> Accept, Suspend Count=%d",m_suspendCount );
	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	if (!m_bRecording)
	{
		//CLogFile::WriteLine( "<Undo> Accept (Not recording)" );
		return;
	}

	string name(szName);
	if (!m_currentUndo->IsEmpty())
	{
		GetIEditorImpl()->SetModifiedFlag();

		// If accepting new undo object, must clear all redo stack.
		ClearRedoStack();

		m_currentUndo->SetName(name);
		if (m_bSuperRecording)
		{
			m_superUndo->AddUndoStep(m_currentUndo);
		}
		else
		{
			// Normal recording.
			m_undoStack.push_back(m_currentUndo);

			signalBufferChanged(m_currentUndo, CUndoManager::eCommandChangeType_Insert, (int)m_undoStack.size() - 1);
			
			QString logLine = name + ": " + m_currentUndo->GetObjectNames();
			//CryLog(logLine.toStdString().c_str());
		}
		//CryLog( "Undo Object Accepted (Undo:%d,Redo:%d, Size=%dKb)",m_undoStack.size(),m_redoStack.size(),GetDatabaseSize()/1024 );

		// If undo accepted, document modified.
		GetIEditorImpl()->SetModifiedFlag();
	}
	else
	{
		// If no any object was recorded, Cancel undo operation.
		Cancel();
	}

	m_bRecording = false;
	m_currentUndo = 0;

	//CLogFile::WriteLine( "<Undo> Accept OK" );
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Cancel()
{
	//CryLog( "<Undo> Cancel" );
	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	if (!m_bRecording)
		return;

	assert(m_currentUndo != 0);

	m_bRecording = false;

	if (!m_currentUndo->IsEmpty())
	{
		// Restore all objects to the state they was at Begin call and throws out all undo objects.
		Restore(true);
		CryLog("Undo Object Canceled (Undo:%d,Redo:%d)", m_undoStack.size(), m_redoStack.size());
	}

	delete m_currentUndo;
	m_currentUndo = 0;
	//CLogFile::WriteLine( "<Undo> Cancel OK" );
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Redo(int numSteps)
{
	GetIEditorImpl()->Notify(eNotify_OnBeginUndoRedo);

	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	if (m_bRecording || m_bSuperRecording)
	{
		CryLog("Cannot Redo while Recording");
		return;
	}

	m_bRedoing = true;
	BeginUndoTransaction();
	m_bRedoing = false;

	if (!m_redoStack.empty())
	{
		Suspend();
		while (numSteps-- > 0 && !m_redoStack.empty())
		{
			m_bRedoing = true;
			CUndoStep* redo = m_redoStack.back();
			redo->Redo();
			m_redoStack.pop_back();
			// Push undo object to redo stack.
			m_undoStack.push_back(redo);
			m_bRedoing = false;

			signalBufferChanged(redo, eCommandChangeType_Move, (int)m_undoStack.size() - 1);
		}
		Resume();
	}
	if (m_suspendCount == 0)
		GetIEditorImpl()->UpdateViews(eUpdateObjects);
	CryLog("Redo (Undo:%d,Redo:%d)", m_undoStack.size(), m_redoStack.size());
	GetIEditorImpl()->GetObjectManager()->InvalidateVisibleList();

	m_bRedoing = true;
	EndUndoTransaction();
	m_bRedoing = false;

	GetIEditorImpl()->Notify(eNotify_OnEndUndoRedo);
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Undo(int numSteps)
{
	GetIEditorImpl()->Notify(eNotify_OnBeginUndoRedo);

	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	if (m_bRecording || m_bSuperRecording)
	{
		CryLog("Cannot Undo while Recording");
		return;
	}

	m_bUndoing = true;
	BeginUndoTransaction();
	m_bUndoing = false;

	if (!m_undoStack.empty())
	{
		Suspend();
		while (numSteps-- > 0 && !m_undoStack.empty())
		{
			m_bUndoing = true;
			CUndoStep* undo = m_undoStack.back();
			undo->Undo(true);
			m_undoStack.pop_back();
			// Push undo object to redo stack.
			m_redoStack.push_back(undo);
			m_bUndoing = false;

			signalBufferChanged(undo, eCommandChangeType_Move, (int)m_undoStack.size() - 1);
		}
		Resume();
	}
	// Update viewports.
	if (m_suspendCount == 0)
		GetIEditorImpl()->UpdateViews(eUpdateObjects);
	CryLog("Undo (Undo:%d,Redo:%d)", m_undoStack.size(), m_redoStack.size());
	GetIEditorImpl()->GetObjectManager()->InvalidateVisibleList();

	m_bUndoing = true;
	EndUndoTransaction();
	m_bUndoing = false;

	GetIEditorImpl()->Notify(eNotify_OnEndUndoRedo);
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::RecordUndo(IUndoObject* obj)
{
	//CryLog( "<Undo> RecordUndo Name=%s",obj->GetDescription() );

	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
	{
		//CLogFile::WriteLine( "<Undo> RecordUndo (Undoing or Redoing)" );
		obj->Release();
		return;
	}

	if (m_bRecording && (m_suspendCount == 0))
	{
		assert(m_currentUndo != 0);
		m_currentUndo->AddUndoObject(obj);
		//CryLog( "Undo Object Added: %s",obj->GetDescription() );
	}
	else
	{
		//CLogFile::WriteLine( "<Undo> RecordUndo (Not Recording)" );
		// Ignore this object.
		obj->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::ClearRedoStack()
{
	int redoCount = m_redoStack.size();
	ClearRedoStack(redoCount);
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::ClearUndoStack()
{
	int undoCount = m_undoStack.size();
	ClearUndoStack(undoCount);
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::ClearUndoStack(int num)
{
	int i = num;
	while (i > 0 && !m_undoStack.empty())
	{
		CUndoStep* pStep = m_undoStack.front();
		m_undoStack.pop_front();

		signalBufferChanged(pStep, eCommandChangeType_Remove, 0);

		delete pStep;
		i--;
	}
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::ClearRedoStack(int num)
{
	int i = num;
	while (i > 0 && !m_redoStack.empty())
	{
		CUndoStep* pStep = m_redoStack.back();
		m_redoStack.pop_back();

		signalBufferChanged(pStep, eCommandChangeType_Remove, m_undoStack.size() + m_redoStack.size());

		delete pStep;
		i--;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CUndoManager::IsHaveRedo() const
{
	return !m_redoStack.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CUndoManager::IsHaveUndo() const
{
	return !m_undoStack.empty();
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Suspend()
{
	m_suspendCount++;
	//CryLog( "<Undo> Suspend %d",m_suspendCount );
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Resume()
{
	assert(m_suspendCount >= 0);
	if (m_suspendCount > 0)
	{
		m_suspendCount--;
	}
	//CryLog( "<Undo> Resume %d",m_suspendCount );
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::SuperBegin()
{
	//CryLog( "<Undo> SuperBegin (SuspendCount%d)",m_suspendCount );
	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	m_bSuperRecording = true;
	m_superUndo = new CSuperUndoStep;
	//CLogFile::WriteLine( "<Undo> SuperBegin OK" );
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::SuperAccept(const string& name)
{
	//CLogFile::WriteLine( "<Undo> SupperAccept" );
	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	if (!m_bSuperRecording)
		return;

	assert(m_superUndo != 0);

	if (m_bRecording)
		Accept(name);

	if (!m_superUndo->IsEmpty())
	{
		m_superUndo->SetName(name);
		m_undoStack.push_back(m_superUndo);

		signalBufferChanged(m_superUndo, eCommandChangeType_Insert, (int)m_undoStack.size() - 1);
	}
	else
	{
		// If no any object was recorded, Cancel undo operation.
		SuperCancel();
	}

	//CryLog( "Undo Object Accepted (Undo:%d,Redo:%d)",m_undoStack.size(),m_redoStack.size() );
	m_bSuperRecording = false;
	m_superUndo = 0;
	//CLogFile::WriteLine( "<Undo> SupperAccept OK" );
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::SuperCancel()
{
	//CLogFile::WriteLine( "<Undo> SuperCancel" );
	if (m_bUndoing || m_bRedoing) // If Undoing or redoing now, ignore this calls.
		return;

	if (!m_bSuperRecording)
		return;

	assert(m_superUndo != 0);

	if (m_bRecording)
		Cancel();

	Suspend();
	//! Undo all changes already made.
	m_superUndo->Undo(false); // Undo not by Undo command (no need to store Redo)
	Resume();

	m_bSuperRecording = false;
	delete m_superUndo;
	m_superUndo = 0;
	//CLogFile::WriteLine( "<Undo> SuperCancel OK" );
}

//////////////////////////////////////////////////////////////////////////
int CUndoManager::GetUndoStackLen() const
{
	return m_undoStack.size();
}

//////////////////////////////////////////////////////////////////////////
int CUndoManager::GetRedoStackLen() const
{
	return m_redoStack.size();
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::GetCommandName(CUndoStep* pStep, string& name)
{
	string objNames = pStep->GetObjectNames();
	string text = pStep->GetName() + objNames;
	if (text.GetLength() > UNDOREDO_BUTTON_POPUP_TEXT_WIDTH)
		name = pStep->GetName() + UNDOREDO_MULTIPLE_OBJECTS_TEXT;
	else
		name = pStep->GetName() + (objNames.IsEmpty() ? "" : " (" + objNames + ")");
}

//////////////////////////////////////////////////////////////////////////
void CUndoManager::Flush()
{
	m_bRecording = false;
	ClearRedoStack();
	ClearUndoStack();

	delete m_superUndo;
	delete m_currentUndo;
	m_superUndo = 0;
	m_currentUndo = 0;
	m_suspendCount = 0;
}

void CUndoManager::AddListener(IUndoManagerListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

void CUndoManager::RemoveListener(IUndoManagerListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

void CUndoManager::BeginUndoTransaction()
{
	for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->BeginUndoTransaction();
	}
}

void CUndoManager::EndUndoTransaction()
{
	for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->EndUndoTransaction();
	}
}

void CUndoManager::BeginRestoreTransaction()
{
	for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->BeginRestoreTransaction();
	}
}

void CUndoManager::EndRestoreTransaction()
{
	for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->EndRestoreTransaction();
	}
}

