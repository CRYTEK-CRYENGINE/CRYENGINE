// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   SaveReaderWriter_Memory.h
//  Created:     30/4/2010 by Ian Masters.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SaveReaderWriter_Memory_h__
#define __SaveReaderWriter_Memory_h__

#include <CryCore/Platform/IPlatformOS.h>

struct IMemoryFile
{
	virtual ~IMemoryFile() {}
	virtual size_t                          GetLength() const = 0;
	virtual long                            GetFilePos() const = 0;
	virtual const char*                     GetFilePath() const = 0;
	virtual const void*                     GetFileContents() const = 0;
	virtual IPlatformOS::EFileOperationCode Seek(long seek, IPlatformOS::ISaveReader::ESeekMode mode) = 0;
	virtual bool                            IsDirty() const = 0;
	virtual void                            SetDirty(bool bDirty) = 0;
	virtual void                            GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

DECLARE_SHARED_POINTERS(IMemoryFile);

struct IMemoryFileSystem
{
	virtual ~IMemoryFileSystem(){}

	static IMemoryFileSystem* CreateFileSystem();

	// LinkFile
	// Description:
	//     Links a file to the file system. If the file already exists for the specified filename and user index it is replaced by the new one.
	virtual void LinkFile(IMemoryFilePtr file) = 0;

	// UnlinkFile
	// Description:
	//     Unlinks a file to the file system. If the file no longer has any references it will be destroyed.
	virtual bool UnlinkFile(IMemoryFilePtr file) = 0;

	// GetFile
	// Description:
	//     Find a file by filename and user index.
	virtual IMemoryFilePtr GetFile(const char* fileName, unsigned int user) = 0;

	virtual int            GetNumFiles(unsigned int user) = 0;
	virtual int            GetNumDirtyFiles(unsigned int user) = 0;

	virtual intptr_t       FindFirst(unsigned int userIndex, const char* filePattern, _finddata_t* fd) = 0;
	virtual int            FindNext(intptr_t handle, _finddata_t* fd) = 0;
	virtual int            FindClose(intptr_t handle) = 0;

	// Format
	// Description:
	//     Delete all files and flush all memory.
	virtual void Format(unsigned int user) = 0;

	// Lock / Unlock
	// Description:
	//     Multi-threaded access to file system so files can be written out asynchronously.
	virtual bool TryLock() = 0;
	virtual void Lock() = 0;
	virtual void Unlock() = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	virtual bool DebugSave(unsigned int user, IPlatformOS::SDebugDump& dump) = 0;
	virtual bool DebugLoad(unsigned int user, const char* fileName = NULL) = 0;

	virtual void DumpFiles(const char* label) = 0;
};
DECLARE_SHARED_POINTERS(IMemoryFileSystem);

class CSaveReader_Memory : public IPlatformOS::ISaveReader
{
public:
	CSaveReader_Memory(const IMemoryFileSystemPtr& pFileSystem, const char* fileName, unsigned int userIndex);
	virtual ~CSaveReader_Memory();

	// ISaveReader
	virtual IPlatformOS::EFileOperationCode Seek(long seek, ESeekMode mode);
	virtual IPlatformOS::EFileOperationCode GetFileCursor(long& fileCursor);
	virtual IPlatformOS::EFileOperationCode ReadBytes(void* data, size_t numBytes);
	virtual IPlatformOS::EFileOperationCode GetNumBytes(size_t& numBytes);
	virtual IPlatformOS::EFileOperationCode Close()           { return IPlatformOS::eFOC_Success; }
	virtual IPlatformOS::EFileOperationCode LastError() const { return m_eLastError; }
	virtual void                            GetMemoryUsage(ICrySizer* pSizer) const;
	//~ISaveReader

private:
	IMemoryFileSystemPtr            m_pFileSystem;
	IMemoryFilePtr                  m_file;
	IPlatformOS::EFileOperationCode m_eLastError;
};

class CSaveWriter_Memory : public IPlatformOS::ISaveWriter
{
public:
	CSaveWriter_Memory(const IMemoryFileSystemPtr& pFileSystem, const char* fileName, unsigned int userIndex);
	virtual ~CSaveWriter_Memory();

	// ISaveWriter
	virtual IPlatformOS::EFileOperationCode AppendBytes(const void* data, size_t length);
	virtual IPlatformOS::EFileOperationCode Close()           { m_bClosed = true; return IPlatformOS::eFOC_Success; }
	virtual IPlatformOS::EFileOperationCode LastError() const { return m_eLastError; }
	virtual void                            GetMemoryUsage(ICrySizer* pSizer) const;
	//~ISaveWriter

	bool IsClosed() const { return m_bClosed; }

protected:
	IMemoryFileSystemPtr            m_pFileSystem;
	IMemoryFilePtr                  m_file;
	IPlatformOS::EFileOperationCode m_eLastError;
	unsigned int                    m_user;
	bool                            m_bClosed;
};

class CFileFinderMemory : public IPlatformOS::IFileFinder
{
public:
	CFileFinderMemory(const IMemoryFileSystemPtr& pFileSystem, unsigned int user);
	virtual ~CFileFinderMemory();

	virtual EFileState FileExists(const char* path);
	virtual intptr_t   FindFirst(const char* filePattern, _finddata_t* fd);
	virtual int        FindNext(intptr_t handle, _finddata_t* fd);
	virtual int        FindClose(intptr_t handle);
	virtual void       GetMemoryUsage(ICrySizer* pSizer) const;

	static int         PatternMatch(const char* str, const char* p);

private:
	IMemoryFileSystemPtr m_pFileSystem;
	unsigned int         m_user;
};

#endif // __SaveReaderWriter_Memory_h__
