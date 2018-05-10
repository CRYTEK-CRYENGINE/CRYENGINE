// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef INCLUDE_SCALEFORM_SDK

	#include "GFileCryPak.h"
	#include "SharedStates.h"

	#include <CrySystem/ISystem.h>
	#include <CrySystem/File/ICryPak.h>
	#include <CrySystem/IStreamEngine.h>

GFileCryPak::GFileCryPak(const char* pPath, unsigned int openFlags)
	: GFile()
	, m_pPak(0)
	, m_fileHandle(0)
	, m_relativeFilePath(pPath)
{
	m_pPak = gEnv->pCryPak;
	assert(m_pPak);
	m_fileHandle = m_pPak->FOpen(pPath, "rb", openFlags);
}

GFileCryPak::~GFileCryPak()
{
	if (IsValidInternal())
		Close();
}

const char* GFileCryPak::GetFilePath()
{
	return m_relativeFilePath.ToCStr();
}

bool GFileCryPak::IsValid()
{
	return IsValidInternal();
}

bool GFileCryPak::IsWritable()
{
	// writable files are not supported!
	return false;
}

inline long GFileCryPak::TellInternal()
{
	if (IsValidInternal())
		return m_pPak->FTell(m_fileHandle);
	return 0;
}

SInt GFileCryPak::Tell()
{
	return TellInternal();
}

SInt64 GFileCryPak::LTell()
{
	static_assert(sizeof(SInt64) >= sizeof(long), "Invalid type size!");
	return TellInternal();
}

inline size_t GFileCryPak::GetLengthInternal()
{
	if (IsValidInternal())
		return m_pPak->FGetSize(m_fileHandle);
	return 0;
}

SInt GFileCryPak::GetLength()
{
	size_t len = GetLengthInternal();
	assert(len <= UInt(-1) >> 1);
	return (SInt) len;
}

SInt64 GFileCryPak::LGetLength()
{
	size_t len = GetLengthInternal();
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	assert(len <= ((UInt64) - 1) >> 1);
	#endif
	return (SInt64) len;
}

SInt GFileCryPak::GetErrorCode()
{
	if (IsValidInternal())
	{
		if (!m_pPak->FError(m_fileHandle))
			return 0;
		return GFileConstants::Error_IOError;
	}
	return GFileConstants::Error_FileNotFound;
}

SInt GFileCryPak::Write(const UByte* pBuf, SInt numBytes)
{
	// writable files are not supported!
	return -1;
}

SInt GFileCryPak::Read(UByte* pBuf, SInt numBytes)
{
	if (IsValidInternal())
		return m_pPak->FReadRaw(pBuf, 1, numBytes, m_fileHandle);
	return -1;
}

SInt GFileCryPak::SkipBytes(SInt numBytes)
{
	if (IsValidInternal())
	{
		long pos = m_pPak->FTell(m_fileHandle);
		if (pos == -1)
			return -1;

		long newPos = SeekInternal(numBytes, SEEK_CUR);
		if (newPos == -1)
			return -1;

		return newPos - pos;
	}
	return 0;
}

SInt GFileCryPak::BytesAvailable()
{
	if (IsValidInternal())
	{
		long pos = m_pPak->FTell(m_fileHandle);
		if (pos == -1)
			return 0;

		size_t endPos = m_pPak->FGetSize(m_fileHandle);
		assert(endPos <= ((unsigned long)-1) >> 1);

		return long(endPos) - pos;
	}
	return 0;
}

bool GFileCryPak::Flush()
{
	// writable files are not supported!
	return false;
}

inline long GFileCryPak::SeekInternal(long offset, int origin)
{
	if (IsValidInternal())
	{
		int newOrigin = 0;
		switch (origin)
		{
		case Seek_Set:
			newOrigin = SEEK_SET;
			break;
		case Seek_Cur:
			newOrigin = SEEK_CUR;
			break;
		case Seek_End:
			newOrigin = SEEK_END;
			break;
		}

		if (newOrigin == SEEK_SET)
		{
			long curPos = m_pPak->FTell(m_fileHandle);
			if (offset == curPos)
				return curPos;
		}

		if (!m_pPak->FSeek(m_fileHandle, offset, newOrigin))
			return m_pPak->FTell(m_fileHandle);
	}
	return -1;
}

SInt GFileCryPak::Seek(SInt offset, SInt origin)
{
	return SeekInternal(offset, origin);
}

SInt64 GFileCryPak::LSeek(SInt64 offset, SInt origin)
{
	assert(offset <= (long)(((unsigned long)-1) >> 1));

	static_assert(sizeof(SInt64) >= sizeof(long), "Invalid type size!");
	return SeekInternal((long) offset, origin);
}

bool GFileCryPak::ChangeSize(SInt newSize)
{
	// not supported!
	return false;
}

SInt GFileCryPak::CopyFromStream(GFile* pStream, SInt byteSize)
{
	// not supported!
	return -1;
}

bool GFileCryPak::Close()
{
	if (IsValidInternal())
	{
		bool res = !m_pPak->FClose(m_fileHandle);
		m_fileHandle = 0;
		return res;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

GFileCryStream::GFileCryStream(const char* pPath, EStreamTaskType streamType)
	: GFile()
	, m_pStreamEngine(0)
	, m_relativeFilePath(pPath)
	, m_fileSize(0)
	, m_curPos(0)
	, m_isValid(false)
	, m_readError(false)
	, m_streamType(streamType)
{
	ISystem* pSystem = gEnv->pSystem;
	assert(pSystem);

	m_pStreamEngine = pSystem->GetStreamEngine();
	assert(m_pStreamEngine);

	ICryPak* pPak = gEnv->pCryPak;
	assert(pPak);

	if (pPath && *pPath)
	{
		m_fileSize = pPak->FGetSize(pPath); // assume zero size files are invalid
		m_isValid = m_fileSize != 0;
		if (m_isValid && pPak->IsFileCompressed(pPath))
		{
			CryGFxLog::GetAccess().LogError("\"%s\" is compressed inside pak. File access not granted! Streamed "
			                                "random file access would be highly inefficient. Make sure file is stored not compressed.", pPath);
			m_isValid = false;
		}
	}
}

GFileCryStream::~GFileCryStream()
{
}

const char* GFileCryStream::GetFilePath()
{
	return m_relativeFilePath.ToCStr();
}

bool GFileCryStream::IsValid()
{
	return IsValidInternal();
}

bool GFileCryStream::IsWritable()
{
	// writable files are not supported!
	return false;
}

inline long GFileCryStream::TellInternal()
{
	if (IsValidInternal())
		return m_curPos;
	return 0;
}

SInt GFileCryStream::Tell()
{
	return TellInternal();
}

SInt64 GFileCryStream::LTell()
{
	static_assert(sizeof(SInt64) >= sizeof(long), "Invalid type size!");
	return TellInternal();
}

inline size_t GFileCryStream::GetLengthInternal()
{
	if (IsValidInternal())
		return m_fileSize;
	return 0;
}

SInt GFileCryStream::GetLength()
{
	size_t len = GetLengthInternal();
	assert(len <= UInt(-1) >> 1);
	return (SInt) len;
}

SInt64 GFileCryStream::LGetLength()
{
	size_t len = GetLengthInternal();
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	assert(len <= ((UInt64) - 1) >> 1);
	#endif
	return (SInt64) len;
}

SInt GFileCryStream::GetErrorCode()
{
	if (IsValidInternal())
	{
		if (!m_readError)
			return 0;
		return GFileConstants::Error_IOError;
	}
	return GFileConstants::Error_FileNotFound;
}

SInt GFileCryStream::Write(const UByte* pBuf, SInt numBytes)
{
	// writable files are not supported!
	return -1;
}

SInt GFileCryStream::Read(UByte* pBuf, SInt numBytes)
{
	m_readError = true;

	if (IsValidInternal())
	{
		if (!InRange(m_curPos) || numBytes < 0)
			return -1;

		if ((size_t) (m_curPos + numBytes) > m_fileSize)
			numBytes = m_fileSize - m_curPos;

		StreamReadParams params(0, estpUrgent, 0, 0, (unsigned) m_curPos, (unsigned) numBytes, pBuf, IStreamEngine::FLAGS_NO_SYNC_CALLBACK);
		IReadStreamPtr p = m_pStreamEngine->StartRead(m_streamType, m_relativeFilePath.ToCStr(), 0, &params);

		if (p)
		{
			p->Wait();
			m_readError = p->IsError();
			if (!m_readError)
			{
				unsigned int bytesRead = p->GetBytesRead();
				m_curPos += (SInt) bytesRead;
				return (SInt) bytesRead;
			}
		}
	}
	return -1;
}

SInt GFileCryStream::SkipBytes(SInt numBytes)
{
	if (IsValidInternal())
	{
		long pos = m_curPos;
		if (!InRange(pos))
			return -1;

		long newPos = m_curPos + numBytes;
		if (!InRange(newPos))
			return -1;

		return newPos - pos;
	}
	return 0;
}

SInt GFileCryStream::BytesAvailable()
{
	if (IsValidInternal())
	{
		long pos = m_curPos;
		if (!InRange(pos))
			return 0;

		size_t endPos = m_fileSize;
		assert(endPos <= ((unsigned long)-1) >> 1);

		return long(endPos) - pos;
	}
	return 0;
}

bool GFileCryStream::Flush()
{
	// writable files are not supported!
	return false;
}

inline long GFileCryStream::SeekInternal(long offset, int origin)
{
	if (IsValidInternal())
	{
		switch (origin)
		{
		case Seek_Set:
			m_curPos = offset;
			break;
		case Seek_Cur:
			m_curPos += offset;
			break;
		case Seek_End:
			m_curPos = m_fileSize - offset;
			break;
		}

		if (InRange(m_curPos))
			return m_curPos;
	}
	return -1;
}

SInt GFileCryStream::Seek(SInt offset, SInt origin)
{
	return SeekInternal(offset, origin);
}

SInt64 GFileCryStream::LSeek(SInt64 offset, SInt origin)
{
	assert(offset <= (long)(((unsigned long)-1) >> 1));

	static_assert(sizeof(SInt64) >= sizeof(long), "Invalid type size!");
	return SeekInternal((long) offset, origin);
}

bool GFileCryStream::ChangeSize(SInt newSize)
{
	// not supported!
	return false;
}

SInt GFileCryStream::CopyFromStream(GFile* pStream, SInt byteSize)
{
	// not supported!
	return -1;
}

bool GFileCryStream::Close()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////

GFileInMemoryCryStream::GFileInMemoryCryStream(const char* pPath, EStreamTaskType streamType)
	: GFile()
	, m_pMemFile(0)
	, m_pData(0)
	, m_dataSize(0)
	, m_errCode(EC_Undefined)
{
	ISystem* pSystem = gEnv->pSystem;
	assert(pSystem);

	IStreamEngine* pStreamEngine = pSystem->GetStreamEngine();
	assert(pStreamEngine);

	ICryPak* pPak = gEnv->pCryPak;
	assert(pPak);

	if (pPath && *pPath)
	{
		pPak->CheckFileAccessDisabled(pPath, "rb");

		m_dataSize = pPak->FGetSize(pPath); // assume zero size files are invalid
		m_pData = m_dataSize ? (unsigned char*) GALLOC(m_dataSize, GStat_Default_Mem) : 0;

		if (m_dataSize && m_pData)
		{
			StreamReadParams params(0, estpUrgent, 0, 0, 0, (unsigned) m_dataSize, m_pData, IStreamEngine::FLAGS_NO_SYNC_CALLBACK);
			IReadStreamPtr pReadStream = pStreamEngine->StartRead(streamType, pPath, 0, &params);

			if (pReadStream)
			{
				pReadStream->Wait();
				if (!pReadStream->IsError())
				{
					m_pMemFile = *new GMemoryFile(pPath, (const UByte*) m_pData, (SInt) m_dataSize);
					m_errCode = EC_Ok;
				}
				else
				{
					m_errCode = pReadStream->GetError() == ERROR_CANT_OPEN_FILE ? EC_FileNotFound : EC_IOError;
				}
			}
		}
	}

	if (!m_pMemFile)
		m_pMemFile = *new GMemoryFile(0, 0, 0);
}

GFileInMemoryCryStream::~GFileInMemoryCryStream()
{
	m_pMemFile = 0;
	if (m_pData)
	{
		GFREE(m_pData);
		m_pData = 0;
	}
}

const char* GFileInMemoryCryStream::GetFilePath()
{
	return m_pMemFile->GetFilePath();
}

bool GFileInMemoryCryStream::IsValid()
{
	return m_pMemFile->IsValid();
}

bool GFileInMemoryCryStream::IsWritable()
{
	// writable files are not supported!
	return m_pMemFile->IsWritable();
}

SInt GFileInMemoryCryStream::Tell()
{
	return m_pMemFile->Tell();
}

SInt64 GFileInMemoryCryStream::LTell()
{
	return m_pMemFile->LTell();
}

SInt GFileInMemoryCryStream::GetLength()
{
	return m_pMemFile->GetLength();
}

SInt64 GFileInMemoryCryStream::LGetLength()
{
	return m_pMemFile->LGetLength();
}

SInt GFileInMemoryCryStream::GetErrorCode()
{
	switch (m_errCode)
	{
	case EC_Ok:
		return 0;
	case EC_FileNotFound:
		return GFileConstants::Error_FileNotFound;
	case EC_IOError:
		return GFileConstants::Error_IOError;
	case EC_Undefined:
	default:
		return GFileConstants::Error_IOError;
	}
}

SInt GFileInMemoryCryStream::Write(const UByte* pBuf, SInt numBytes)
{
	// writable files are not supported!
	return -1;
}

SInt GFileInMemoryCryStream::Read(UByte* pBuf, SInt numBytes)
{
	return m_pMemFile->Read(pBuf, numBytes);
}

SInt GFileInMemoryCryStream::SkipBytes(SInt numBytes)
{
	return m_pMemFile->SkipBytes(numBytes);
}

SInt GFileInMemoryCryStream::BytesAvailable()
{
	return m_pMemFile->BytesAvailable();
}

bool GFileInMemoryCryStream::Flush()
{
	// writable files are not supported!
	return false;
}

SInt GFileInMemoryCryStream::Seek(SInt offset, SInt origin)
{
	return m_pMemFile->Seek(offset, origin);
}

SInt64 GFileInMemoryCryStream::LSeek(SInt64 offset, SInt origin)
{
	return m_pMemFile->LSeek(offset, origin);
}

bool GFileInMemoryCryStream::ChangeSize(SInt newSize)
{
	// not supported!
	return false;
}

SInt GFileInMemoryCryStream::CopyFromStream(GFile* pStream, SInt byteSize)
{
	// not supported!
	return -1;
}

bool GFileInMemoryCryStream::Close()
{
	return m_pMemFile->Close();
}

#endif // #ifdef INCLUDE_SCALEFORM_SDK
