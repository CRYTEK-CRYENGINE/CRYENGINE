// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SYNCEDFILESET_H__
#define __SYNCEDFILESET_H__

#pragma once

#include "Config.h"

#if SERVER_FILE_SYNC_MODE

	#include "SyncedFile.h"

class CNetChannel;

class CSyncedFileSet
{
public:
	static const uint32 MAX_SEND_CHUNK_SIZE = 32;

	CSyncedFileSet();
	~CSyncedFileSet();

	void           AddFile(const char* name);
	CSyncedFilePtr GetSyncedFile(const char* name);
	void           SendFilesTo(CNetChannel* pSender);

	// update file implementation
	bool BeginUpdateFile(const char* name, uint32 id);
	bool AddFileData(uint32 id, const uint8* pData, uint32 length);
	bool EndUpdateFile(uint32 id);

	// do we need to update a file?
	bool NeedToUpdateFile(const char* name, const uint8* hash);

private:
	uint8 m_syncsPerformed;

	typedef std::map<string, CSyncedFilePtr> TFiles;
	TFiles m_files;

	typedef std::map<uint32, CSyncedFilePtr> TUpdatingFiles;
	TUpdatingFiles m_updating;
};

#endif // SERVER_FILE_SYNC_MODE

#endif
