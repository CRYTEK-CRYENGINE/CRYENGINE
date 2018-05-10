// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IEditor.h"
#include "IUndoManager.h"

//! IUndoObject is a interface of general Undo object.
struct IUndoObject
{
	// Virtual destructor.
	virtual ~IUndoObject() {};

	//! Called to delete undo object.
	virtual void        Release() { delete this; };

	//! Return description of this Undo object.
	virtual const char* GetDescription() = 0;

	//! Undo this object.
	//! @param bUndo If true this operation called in response to Undo operation.
	virtual void Undo(bool bUndo = true) = 0;

	//! Redo undone changes on object.
	virtual void Redo() = 0;

	// Returns the name of undo object
	virtual const char* GetObjectName() { return 0; };
};

//! Undo utility class.
class CUndo
{
public:
	CUndo(const char* description) : m_bCancelled(false)
	{
		if (!IsRecording())
		{
			GetIEditor()->GetIUndoManager()->Begin();
			m_description = description;
			m_bStartedRecord = true;
		}
		else
		{
			m_bStartedRecord = false;
		}
	};

	~CUndo()
	{
		if (m_bStartedRecord)
		{
			if (m_bCancelled)
				GetIEditor()->GetIUndoManager()->Cancel();
			else
				GetIEditor()->GetIUndoManager()->Accept(m_description);
		}
	};

	void Cancel()
	{
		m_bCancelled = true;
	}

	//! Check if undo is recording.
	static bool IsRecording() { return GetIEditor()->GetIUndoManager()->IsUndoRecording(); };
	//! Check if undo is suspended.
	static bool IsSuspended() { return GetIEditor()->GetIUndoManager()->IsUndoSuspended(); };
	//! Record specified object.
	static void Record(IUndoObject* undo) { return GetIEditor()->GetIUndoManager()->RecordUndo(undo); };

private:
	string m_description;
	bool    m_bCancelled;
	bool    m_bStartedRecord;
};

class CScopedSuspendUndo
{
public:
	CScopedSuspendUndo() { GetIEditor()->GetIUndoManager()->Suspend(); }
	~CScopedSuspendUndo() { GetIEditor()->GetIUndoManager()->Resume(); }
};



