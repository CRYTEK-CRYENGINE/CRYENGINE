// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CFileChangeMonitorListener;
class CFileChangeMonitorThread;
class CFileChangeMonitorWindow;

struct SFileChangeInfo
{
	enum EChangeType
	{
		//! error or unknown change type
		eChangeType_Unknown,
		//! the file was created
		eChangeType_Created,
		//! the file was deleted
		eChangeType_Deleted,
		//! the file was modified (size changed,write)
		eChangeType_Modified,
		//! this is the old name of a renamed file
		eChangeType_RenamedOldName,
		//! this is the new name of a renamed file
		eChangeType_RenamedNewName
	};

	SFileChangeInfo()
		: changeType(eChangeType_Unknown)
	{
	}

	bool operator==(const SFileChangeInfo& rhs) const
	{
		return changeType == rhs.changeType && filename == rhs.filename;
	}

	string     filename;
	EChangeType changeType;
};

// Monitors directory for any changed files
class CFileChangeMonitor
{
public:
	friend class CFileChangeMonitorWindow;
	friend class CEditorFileMonitor;
	typedef std::set<CFileChangeMonitorListener*> TListeners;

protected:
	CFileChangeMonitor();
	~CFileChangeMonitor();

	void        Initialize();
	static void DeleteInstance();

	static CFileChangeMonitor* s_pFileMonitorInstance;

public:

	static CFileChangeMonitor* Instance();

	bool                       MonitorItem(const string& sItem);
	void                       StopMonitor();
	void                       SetEnabled(bool bEnable);
	bool                       IsEnabled();
	//! check if any files where modified,
	//! this is a polling function, call it every frame or so
	bool HaveModifiedFiles() const;
	//! get next modified file, this file will be delete from list after calling this function,
	//! call it until HaveModifiedFiles return true or this function returns false
	bool PopNextFileChange(SFileChangeInfo& /*out*/ rPoppedChange);
	void Subscribe(CFileChangeMonitorListener* pListener);
	void Unsubscribe(CFileChangeMonitorListener* pListener);
	bool LogFileChanges() const
	{
		return ed_logFileChanges != 0;
	}
	bool CombineFileChanges() const
	{
		return ed_combineFileChanges != 0;
	}
	void AddIgnoreFileMask(const char* pMask);
	void RemoveIgnoreFileMask(const char* pMask, int aAfterDelayMsec = 1000);

private:
	void NotifyListeners();

	int                                     ed_logFileChanges;
	int                                     ed_combineFileChanges;
	TListeners                              m_listeners;
	//! Pointer to implementation class.
	CFileChangeMonitorThread*               m_pThread;
	std::auto_ptr<CFileChangeMonitorWindow> m_window;
};

// Used as base class (aka interface) to subscribe for file change events
class CFileChangeMonitorListener
{
public:
	CFileChangeMonitorListener()
		: m_pMonitor(NULL)
	{
	}

	virtual ~CFileChangeMonitorListener()
	{
		if (m_pMonitor)
		{
			m_pMonitor->Unsubscribe(this);
		}
	}

	virtual void OnFileMonitorChange(const SFileChangeInfo& rChange) = 0;

	void         SetMonitor(CFileChangeMonitor* pMonitor)
	{
		m_pMonitor = pMonitor;
	}

private:
	CFileChangeMonitor* m_pMonitor;
};

