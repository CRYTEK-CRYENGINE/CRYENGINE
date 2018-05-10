// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PerforceSourceControl_h__
#define __PerforceSourceControl_h__

#pragma once

#define USERNAME_LENGTH 64

#include "IEditorClassFactory.h"

#pragma warning(push)
#pragma warning(disable: 4244) // 'conversion' conversion from 'type1' to 'type2', possible loss of data
#include "p4/clientapi.h"
#include "p4/errornum.h"
#pragma warning(pop)

#include "ISourceControl.h"

#include <QToolButton>
#include <CrySandbox/CrySignal.h>

struct CPerforceThread;

class CMyClientUser : public ClientUser
{
public:
	CMyClientUser()
	{
		m_initDesc = "Automatic.";
		cry_strcpy(m_desc, m_initDesc);
		Init();
	}
	void HandleError(Error* e);
	void OutputStat(StrDict* varList);
	void Edit(FileSys* f1, Error* e);
	void OutputInfo(char level, const char* data);
	void Init();
	void PreCreateFileName(const char* file);
	void InputData(StrBuf* buf, Error* e);

	Error          m_e;
	char           m_action[64];
	char           m_headAction[64];
	char           m_otherAction[64];
	char           m_clientHasLatestRev[64];
	char           m_desc[1024];
	char           m_file[MAX_PATH];
	char           m_findedFile[MAX_PATH];
	char           m_depotFile[MAX_PATH];
	char           m_otherUser[USERNAME_LENGTH];
	char           m_lockedBy[USERNAME_LENGTH];
	string        m_clientRoot;
	string        m_clientName;
	string        m_clientHost;
	string        m_currentDirectory;
	bool           m_bIsClientUnknown;

	const char*    m_initDesc;
	bool           m_bIsSetup;
	bool           m_bIsPreCreateFile;
	bool           m_bIsCreatingChangelist;
	string        m_output;
	string        m_input;

	int64          m_nFileHeadRev;
	int64          m_nFileHaveRev;
	ESccLockStatus m_lockStatus;
};

class CMyClientApi : public ClientApi
{
public:
	void Run(const char* func);
	void Run(const char* func, ClientUser* ui);
};

class CPerforceSourceControl : public ISourceControl
{
public:

	// constructor
	CPerforceSourceControl();
	virtual ~CPerforceSourceControl();

	bool Connect();
	bool Reconnect();
	void FreeData();
	void Init();
	bool CheckConnection();

	// from ISourceControl
	uint32 GetFileAttributes(const char* filename);

	// Thread processing
	void           GetFileAttributesThread(const char* filename);

	bool           DoesChangeListExist(const char* pDesc, char* changeid, int nLen);
	bool           CreateChangeList(const char* pDesc, char* changeid, int nLen);
	bool           Add(const char* filename, const char* desc, int nFlags, char* changelistId = NULL);
	bool           CheckIn(const char* filename, const char* desc, int nFlags);
	bool           CheckOut(const char* filename, int nFlags, char* changelistId = NULL);
	bool           UndoCheckOut(const char* filename, int nFlags);
	bool           Rename(const char* filename, const char* newfilename, const char* desc, int nFlags);
	bool           Delete(const char* filename, const char* desc, int nFlags, char* changelistId = NULL);
	bool           GetLatestVersion(const char* filename, int nFlags);
	bool           GetInternalPath(const char* filename, char* outPath, int nOutPathSize);
	bool           GetOtherUser(const char* filename, char* outUser, int nOutUserSize);
	bool           History(const char* filename);

	virtual void   ShowSettings() override;

	bool           GetOtherLockOwner(const char* filename, char* outUser, int nOutUserSize);
	ESccLockStatus GetLockStatus(const char* filename);
	bool           Lock(const char* filename, int nFlags = 0);
	bool           Unlock(const char* filename, int nFlags = 0);
	bool           GetUserName(char* outUser, int nOutUserSize);
	bool           GetFileRev(const char* sFilename, int64* pHaveRev, int64* pHeadRev);
	bool           GetRevision(const char* filename, int64 nRev, int nFlags = 0);
	bool           SubmitChangeList(char* changeid);
	bool           DeleteChangeList(char* changeid);
	bool           Reopen(const char* filename, char* changeid = NULL);

	bool           Run(const char* func, int nArgs, char* argv[], bool bOnlyFatal = false);
	uint32         GetFileAttributesAndFileName(const char* filename, char* FullFileName);
	bool           IsFolder(const char* filename, char* FullFileName);

	// from IClassDesc
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_SCM_PROVIDER; };
	virtual const char*    ClassName()       { return "Perforce source control"; };
	virtual const char*    Category()        { return "SourceControl"; };
	virtual CRuntimeClass* GetRuntimeClass() { return 0; };

	CMyClientApi& GetClientApi() { return m_client; }
	bool IsWorkOffline() const { return m_bIsWorkOffline; }
	void SetWorkOffline(bool bWorkOffline);

protected:
	bool        IsFileManageable(const char* sFilename, bool bCheckFatal = true);
	bool        IsFileExistsInDatabase(const char* sFilename);
	bool        IsFileCheckedOutByUser(const char* sFilename, bool* pIsByAnotherUser = 0);
	bool        IsFileLatestVersion(const char* sFilename);
	void        ConvertFileNameCS(char* sDst, const char* sSrcFilename);
	void        MakePathCS(char* sDst, const char* sSrcFilename);
	void        RenameFolders(const char* path, const char* oldPath);
	bool        FindFile(char* clientFile, const char* folder, const char* file);
	bool        FindDir(char* clientFile, const char* folder, const char* dir);
	bool        IsSomeTimePassed();

	const char* GetErrorByGenericCode(int nGeneric);

public:
	CCrySignal<void()> signalWorkOfflineChanged;

private:
	CMyClientUser    m_ui;
	CMyClientApi     m_client;
	Error            m_e;

	bool             m_bIsWorkOffline;
	bool             m_bIsFailConnectionLogged;

	CPerforceThread* m_thread;
	uint32           m_unRetValue;
	bool             m_isSetuped;
	bool             m_isSecondThread;
	bool             m_isSkipThread;
	DWORD            m_dwLastAccessTime;

	ULONG            m_ref;
};

class CPerforceTrayWidget : public QToolButton
{
	Q_OBJECT
public:
	CPerforceTrayWidget(QWidget* pParent = nullptr);
	void OnClicked(bool bChecked);
	void OnConnectionStatusChanged();

private:
	class QPopupWidget* m_pPopUpMenu;
};

#endif //__PerforceSourceControl_h__

