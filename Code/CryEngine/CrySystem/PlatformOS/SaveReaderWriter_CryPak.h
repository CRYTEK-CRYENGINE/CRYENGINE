// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   SaveReaderWriter_CryPak.h
//  Created:     15/02/2010 by Alex McCarthy.
//  Description: Implementation of the ISaveReader and ISaveWriter
//               interfaces using CryPak
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SAVE_READER_WRITER_CRYPAK_H__
#define __SAVE_READER_WRITER_CRYPAK_H__

#include <CryCore/Platform/IPlatformOS.h>

class CCryPakFile
{
protected:
	CCryPakFile(const char* fileName, const char* szMode);
	virtual ~CCryPakFile();

	IPlatformOS::EFileOperationCode CloseImpl();

	std::vector<uint8>              m_data;
	string                          m_fileName;
	FILE*                           m_pFile;
	size_t                          m_filePos;
	IPlatformOS::EFileOperationCode m_eLastError;
	bool                            m_bWriting;
	static const int                s_dataBlockSize = 128 * 1024;

	NO_INLINE_WEAK static FILE*  FOpen(const char* pName, const char* mode, unsigned nFlags = 0);
	NO_INLINE_WEAK static int    FClose(FILE* fp);
	NO_INLINE_WEAK static size_t FGetSize(FILE* fp);
	NO_INLINE_WEAK static size_t FReadRaw(void* data, size_t length, size_t elems, FILE* fp);
	NO_INLINE_WEAK static size_t FWrite(const void* data, size_t length, size_t elems, FILE* fp);
	NO_INLINE_WEAK static size_t FSeek(FILE* fp, long offset, int mode);

private:
	CScopedAllowFileAccessFromThisThread m_allowFileAccess;
};

class CSaveReader_CryPak : public IPlatformOS::ISaveReader, public CCryPakFile
{
public:
	CSaveReader_CryPak(const char* fileName);

	// ISaveReader
	virtual IPlatformOS::EFileOperationCode Seek(long seek, ESeekMode mode);
	virtual IPlatformOS::EFileOperationCode GetFileCursor(long& fileCursor);
	virtual IPlatformOS::EFileOperationCode ReadBytes(void* data, size_t numBytes);
	virtual IPlatformOS::EFileOperationCode GetNumBytes(size_t& numBytes);
	virtual IPlatformOS::EFileOperationCode Close()           { return CloseImpl(); }
	virtual IPlatformOS::EFileOperationCode LastError() const { return m_eLastError; }
	virtual void                            GetMemoryUsage(ICrySizer* pSizer) const;
	virtual void                            TouchFile();
	//~ISaveReader
};

DECLARE_SHARED_POINTERS(CSaveReader_CryPak);

class CSaveWriter_CryPak : public IPlatformOS::ISaveWriter, public CCryPakFile
{
public:
	CSaveWriter_CryPak(const char* fileName);

	// ISaveWriter
	virtual IPlatformOS::EFileOperationCode AppendBytes(const void* data, size_t length);
	virtual IPlatformOS::EFileOperationCode Close()           { return CloseImpl(); }
	virtual IPlatformOS::EFileOperationCode LastError() const { return m_eLastError; }
	virtual void                            GetMemoryUsage(ICrySizer* pSizer) const;
	//~ISaveWriter
};

DECLARE_SHARED_POINTERS(CSaveWriter_CryPak);

#endif //__SAVE_READER_WRITER_CRYPAK_H__
