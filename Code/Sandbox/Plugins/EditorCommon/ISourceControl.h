// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "IEditorClassFactory.h"

// Source control status of item.
enum ESccFileAttributes
{
	SCC_FILE_ATTRIBUTE_INVALID         = 0x0000, // File is not found.
	SCC_FILE_ATTRIBUTE_NORMAL          = 0x0001, // Normal file on disk.
	SCC_FILE_ATTRIBUTE_READONLY        = 0x0002, // Read only files cannot be modified at all, either read only file not under source control or file in packfile.
	SCC_FILE_ATTRIBUTE_INPAK           = 0x0004, // File is inside pack file.
	SCC_FILE_ATTRIBUTE_MANAGED         = 0x0008, // File is managed under source control.
	SCC_FILE_ATTRIBUTE_CHECKEDOUT      = 0x0010, // File is under source control and is checked out.
	SCC_FILE_ATTRIBUTE_BYANOTHER       = 0x0020, // File is under source control and is checked out by another user.
	SCC_FILE_ATTRIBUTE_FOLDER          = 0x0040, // Managed folder.
	SCC_FILE_ATTRIBUTE_LOCKEDBYANOTHER = 0x0080, // File is under source control and is checked out and locked by another user.
};

// Source control flags
enum ESccFlags
{
	GETLATEST_REVERT      = 1 << 0, // don't revert when perform GetLatestVersion() on opened files
	GETLATEST_ONLY_CHECK  = 1 << 1, // don't actually get latest version of the file, check if scc has more recent version
	ADD_WITHOUT_SUBMIT    = 1 << 2, // add the files to the default changelist
	ADD_CHANGELIST        = 1 << 3, // add a changelist with the description
	ADD_AS_BINARY_FILE    = 1 << 4, // add the files as binary files
	DELETE_WITHOUT_SUBMIT = 1 << 5, // mark for delete and don't submit
};

// Lock status of an item in source control
enum ESccLockStatus
{
	SCC_LOCK_STATUS_UNLOCKED,
	SCC_LOCK_STATUS_LOCKED_BY_OTHERS,
	SCC_LOCK_STATUS_LOCKED_BY_US,
};

//////////////////////////////////////////////////////////////////////////
// Description
//    This interface provide access to the source control functionality.
//////////////////////////////////////////////////////////////////////////
struct ISourceControl : public IClassDesc, public _i_reference_target_t
{
	// Description:
	//    Returns attributes of the file.
	// Return:
	//    Combination of flags from ESccFileAttributes enumeration.
	virtual uint32 GetFileAttributes(const char* filename) = 0;

	virtual bool   DoesChangeListExist(const char* pDesc, char* changeid, int nLen) = 0;
	virtual bool   CreateChangeList(const char* pDesc, char* changeid, int nLen) = 0;
	virtual bool   Add(const char* filename, const char* desc = 0, int nFlags = 0, char* changelistId = NULL) = 0;
	virtual bool   CheckIn(const char* filename, const char* desc = 0, int nFlags = 0) = 0;
	virtual bool   CheckOut(const char* filename, int nFlags = 0, char* changelistId = NULL) = 0;
	virtual bool   UndoCheckOut(const char* filename, int nFlags = 0) = 0;
	virtual bool   Rename(const char* filename, const char* newfilename, const char* desc = 0, int nFlags = 0) = 0;
	virtual bool   Delete(const char* filename, const char* desc = 0, int nFlags = 0, char* changelistId = NULL) = 0;
	virtual bool   GetLatestVersion(const char* filename, int nFlags = 0) = 0;

	// GetInternalPath - Get internal Source Control path for file filename
	// outPath: output char buffer, provided by user, MAX_PATH - recommended size of this buffer
	// nOutPathSize: size of outPath buffer, MAX_PATH is recommended
	// if a smaller size will be used, it may returned a truncated path.
	virtual bool GetInternalPath(const char* filename, char* outPath, int nOutPathSize) = 0;

	// GetOtherUser - Get other user name who edit file filename
	// outUser: output char buffer, provided by user, 64 - recommended size of this buffer
	// nOutUserSize: size of outUser buffer, 64 is recommended
	virtual bool GetOtherUser(const char* filename, char* outUser, int nOutUserSize) = 0;

	// Show file history
	virtual bool History(const char* filename) = 0;

	// Show settings dialog
	virtual void ShowSettings() = 0;

	// GetOtherLockOwner = Get the user name who has the specifed file locked.
	// outUser: output char buffer, provided by user, 64 - recommended size of this buffer
	// If the file is locked by me or nobody, it returns false.
	virtual bool           GetOtherLockOwner(const char* filename, char* outUser, int nOutUserSize)                       { return false; }
	virtual ESccLockStatus GetLockStatus(const char* filename)                                                            { return SCC_LOCK_STATUS_UNLOCKED; }
	virtual bool           Lock(const char* filename, int nFlags = 0)                                                     { return false; }
	virtual bool           Unlock(const char* filename, int nFlags = 0)                                                   { return false; }
	virtual bool           Integrate(const char* filename, const char* newfilename, const char* desc = 0, int nFlags = 0) { return false; }
	virtual bool           GetFileRev(const char* sFilename, int64* pHaveRev, int64* pHeadRev)                            { return false; }
	virtual bool           GetUserName(char* outUser, int nOutUserSize)                                                   { return false; }
	virtual bool           GetRevision(const char* filename, int64 nRev, int nFlags = 0)                                  { return false; }
	virtual bool           SubmitChangeList(char* changeid)                                                               { return false; }
	virtual bool           DeleteChangeList(char* changeid)                                                               { return false; }
	virtual bool           Reopen(const char* filename, char* changeid = NULL)                                            { return false; }
};

