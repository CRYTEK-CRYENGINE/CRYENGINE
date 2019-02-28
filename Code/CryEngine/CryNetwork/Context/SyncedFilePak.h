// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SYNCEDFILEPAK_H__
#define __SYNCEDFILEPAK_H__

#include "Config.h"

#pragma once

#if SERVER_FILE_SYNC_MODE

class CSyncedFileSet;

// wrapper around ICryPak
// supports just enough to make CCryFile work!
class CSyncedFilePak : public ICryPak
{
public:
	explicit CSyncedFilePak(CSyncedFileSet& fileset) : m_fileset(fileset) {}

	virtual const char*                  AdjustFileName(const char* src, char dst[g_nMaxPath], unsigned nFlags, bool* bFoundInPak);
	virtual bool                         Init(const char* szBasePath);
	virtual void                         Release();
	virtual bool                         OpenPack(const char* pName, unsigned nFlags);
	virtual bool                         OpenPack(const char* pBindingRoot, const char* pName, unsigned nFlags);
	virtual bool                         ClosePack(const char* pName, unsigned nFlags);
	virtual bool                         OpenPacks(const char* pWildcard, unsigned nFlags);
	virtual bool                         OpenPacks(const char* pBindingRoot, const char* pWildcard, unsigned nFlags);
	virtual bool                         ClosePacks(const char* pWildcard, unsigned nFlags);
	virtual void                         AddMod(const char* szMod);
	virtual void                         RemoveMod(const char* szMod);
	virtual void                         ParseAliases(const char* szCommandLine);
	virtual void                         SetAlias(const char* szName, const char* szAlias, bool bAdd);
	virtual const char*                  GetAlias(const char* szName, bool bReturnSame);
	virtual void                         Lock();
	virtual void                         Unlock();
	virtual void                         SetGameFolder(const char* szFolder);
	virtual const char*                  GetGameFolder() const;
	virtual ICryPak::PakInfo*            GetPakInfo();
	virtual void                         FreePakInfo(PakInfo*);
	virtual FILE*                        FOpen(const char* pName, const char* mode, unsigned nFlags);
	virtual FILE*                        FOpen(const char* pName, const char* mode, char* szFileGamePath, int nLen);
	virtual size_t                       FReadRaw(void* data, size_t length, size_t elems, FILE* handle);
	virtual size_t                       FReadRawAll(void* data, size_t nFileSize, FILE* handle);
	virtual void*                        FGetCachedFileData(FILE* handle, size_t& nFileSize);
	virtual size_t                       FWrite(const void* data, size_t length, size_t elems, FILE* handle);
	virtual int                          FPrintf(FILE* handle, const char* format, ...) PRINTF_PARAMS(3, 4);
	virtual char*                        FGets(char*, int, FILE*);
	virtual int                          Getc(FILE*);
	virtual size_t                       FGetSize(FILE* f);
	virtual size_t                       FGetSize(const char* pName);
	virtual int                          Ungetc(int c, FILE*);
	virtual bool                         IsInPak(FILE* handle);
	virtual bool                         RemoveFile(const char* pName);               // remove file from FS (if supported)
	virtual bool                         RemoveDir(const char* pName, bool bRecurse); // remove directory from FS (if supported)
	virtual bool                         IsAbsPath(const char* pPath);                // determines if pPath is an absolute or relative path
	virtual bool                         CopyFileOnDisk(const char* source, const char* dest, bool bFailIfExist);
	virtual size_t                       FSeek(FILE* handle, long seek, int mode);
	virtual long                         FTell(FILE* handle);
	virtual int                          FClose(FILE* handle);
	virtual int                          FEof(FILE* handle);
	virtual int                          FError(FILE* handle);
	virtual int                          FGetErrno();
	virtual int                          FFlush(FILE* handle);
	virtual void*                        PoolMalloc(size_t size);
	virtual void                         PoolFree(void* p);
	virtual intptr_t                     FindFirst(const char* pDir, _finddata_t* fd, unsigned int nFlags);
	virtual int                          FindNext(intptr_t handle, _finddata_t* fd);
	virtual int                          FindClose(intptr_t handle);
	virtual ICryPak::FileTime            GetModificationTime(FILE* f);
	virtual bool                         IsFileExist(const char* sFilename);
	virtual bool                         MakeDir(const char* szPath, bool bGamePathMapping);
	virtual ICryArchive*                 OpenArchive(const char* szPath, unsigned int nFlags);
	virtual const char*                  GetFileArchivePath(FILE* f);
	virtual int                          RawCompress(const void* pUncompressed, unsigned long* pDestSize, void* pCompressed, unsigned long nSrcSize, int nLevel);
	virtual int                          RawUncompress(void* pUncompressed, unsigned long* pDestSize, const void* pCompressed, unsigned long nSrcSize);
	virtual void                         RecordFileOpen(const ERecordFileOpenList eList);
	virtual void                         RecordFile(FILE* in, const char* szFilename);
	virtual IResourceList*               GetResourceList(const ERecordFileOpenList eList);
	virtual ICryPak::ERecordFileOpenList GetRecordFileOpenList();
	virtual void                         Notify(ENotifyEvent event);
	virtual uint32                       ComputeCRC(const char* szPath);
	virtual bool                         ComputeMD5(const char* szPath, unsigned char* md5);
	virtual void                         RegisterFileAccessSink(ICryPakFileAcesssSink* pSink);
	virtual void                         UnregisterFileAccessSink(ICryPakFileAcesssSink* pSink);
	virtual bool                         GetLvlResStatus() const;

private:
	struct IFileManip
	{
		inline virtual ~IFileManip() {}
		virtual size_t ReadRaw(void* data, size_t length, size_t elems) = 0;
		virtual size_t Seek(long seek, int mode) = 0;
		virtual size_t Tell() = 0;
		virtual int    Eof() = 0;
	};

	class CNonSyncedManip;
	class CSyncedManip;

	CSyncedFileSet& m_fileset;
};

#endif

#endif
