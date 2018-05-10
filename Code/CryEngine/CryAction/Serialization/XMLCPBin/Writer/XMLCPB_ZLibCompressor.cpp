// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_ZLibCompressor.h"
#include "CryActionCVars.h"
#include <CryThreading/IThreadManager.h>
#include <CrySystem/ZLib/IZLibCompressor.h>

namespace XMLCPB
{

class CFile
{
	friend class CCompressorThread;

public:

	CFile(CZLibCompressor* pCompressor, const char* pFileName, CryEvent& event)
		: m_pCompressor(pCompressor)
		, m_bytesWrittenIntoFile(0)
		, m_bytesWrittenIntoFileUncompressed(0)
		, m_bClosed(false)
		, m_event(event)
	{
		m_startingTime = gEnv->pTimer->GetAsyncTime().GetMilliSeconds();

#ifdef XMLCPB_CHECK_FILE_INTEGRITY
		m_pICompressor = GetISystem()->GetIZLibCompressor();

		if (m_pICompressor)
		{
			m_pICompressor->MD5Init(&m_MD5Context);
		}

#endif
		
		m_pPlatformOSSaveWriter = gEnv->pSystem->GetPlatformOS()->SaveGetWriter(pFileName);
	}

	~CFile()
	{
		delete m_pCompressor;
	}

	void Close()
	{
		m_bClosed = true;
		m_event.Set();
	}

	void Finish()
	{
#ifdef XMLCPB_CHECK_FILE_INTEGRITY
		if (m_pICompressor)
		{
			m_pICompressor->MD5Update(&m_MD5Context, (const char*)(&m_pCompressor->m_fileHeader), sizeof(m_pCompressor->m_fileHeader));
			m_pICompressor->MD5Final(&m_MD5Context, m_pCompressor->m_fileHeader.m_MD5Signature);
		}
		else
		{
			memset(&m_pCompressor->m_fileHeader.m_MD5Signature, 0, sizeof(m_pCompressor->m_fileHeader.m_MD5Signature));
		}
#endif

		Write(&m_pCompressor->m_fileHeader, sizeof(m_pCompressor->m_fileHeader));

		m_pPlatformOSSaveWriter->Close();

		float finalTime = gEnv->pTimer->GetAsyncTime().GetMilliSeconds();

		CryLog("[SAVE GAME] Binary saveload: After writing, result: %s   filesize/uncompressed: %u/%u (%u kb / %u kb)   generation time: %d ms ",
		       (m_pCompressor->m_errorWritingIntoFile) ? "FAIL" : "SUCCESS", m_bytesWrittenIntoFile, m_bytesWrittenIntoFileUncompressed, m_bytesWrittenIntoFile / 1024, m_bytesWrittenIntoFileUncompressed / 1024, int(finalTime - m_startingTime));
	}

	bool Write(void* pSrc, uint32 numBytes)
	{
#ifdef XMLCPB_CHECK_FILE_INTEGRITY
		if (m_pICompressor)
		{
			m_pICompressor->MD5Update(&m_MD5Context, (const char*)pSrc, numBytes);
		}
#endif

		IPlatformOS::EFileOperationCode code = m_pPlatformOSSaveWriter->AppendBytes(pSrc, numBytes);
		bool ok = (code == IPlatformOS::eFOC_Success);
		assert(ok);
		if (ok)
		{
			m_bytesWrittenIntoFile += numBytes;
			return true;
		}
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCompressorThread::WriteDataIntoFileInternal ERROR: failed to AppendBytes");
		return false;
	}

	void AddBlock(SZLibBlock* block)
	{
		m_blocks.push(block);
		m_event.Set();
	}

	bool Closed() const { return m_bClosed; }

private:

#ifdef XMLCPB_CHECK_FILE_INTEGRITY
	SMD5Context m_MD5Context;
	IZLibCompressor* m_pICompressor;
#endif
	CZLibCompressor * m_pCompressor;
	CryEvent&                                m_event;
	IPlatformOS::ISaveWriterPtr              m_pPlatformOSSaveWriter;
	CryMT::CLocklessPointerQueue<SZLibBlock> m_blocks;
	uint32 m_bytesWrittenIntoFile;                                    // used for statistics only
	uint32 m_bytesWrittenIntoFileUncompressed;                        // used for statistics only
	float  m_startingTime;
	bool   m_bClosed;
};

class CCompressorThread : public IThread
{
public:

	CCompressorThread()
		: m_bCancelled(false), m_nBlockCount(0), m_nMaxQueuedBlocks(CCryActionCVars::Get().g_XMLCPBBlockQueueLimit)
	{
	}

	~CCompressorThread()
	{
		SignalStopWork();
		gEnv->pThreadManager->JoinThread(this, eJM_Join);
	}

	// Start accepting work on thread
	virtual void ThreadEntry()
	{
		while (!m_bCancelled || !m_files.empty())
		{
			m_event.Wait();

			uint8* pZLibCompressedBuffer = AllocateBlock();

			while (!m_files.empty())
			{
				CFile* pFile = m_files.pop();
				assert(pFile);
				PREFAST_ASSUME(pFile);

				while (!pFile->Closed() || !pFile->m_blocks.empty())
				{
					if (pFile->m_blocks.empty())
					{
						CrySleep(1); // yield to give other threads a chance to do some work
					}

					while (!pFile->m_blocks.empty())
					{
						SZLibBlock* block = pFile->m_blocks.pop();
						assert(block);
						PREFAST_ASSUME(block);

						if (pFile->m_pCompressor->m_bUseZLibCompression)
						{
							size_t compressedLength = XMLCPB_ZLIB_BUFFER_SIZE;
							bool compressionOk = gEnv->pSystem->CompressDataBlock(block->m_pZLibBuffer, block->m_ZLibBufferSizeUsed, pZLibCompressedBuffer, compressedLength);

							SZLibBlockHeader zlibHeader;
							zlibHeader.m_compressedSize = compressionOk ? compressedLength : SZLibBlockHeader::NO_ZLIB_USED;
							zlibHeader.m_uncompressedSize = block->m_ZLibBufferSizeUsed;
							pFile->m_bytesWrittenIntoFileUncompressed += block->m_ZLibBufferSizeUsed;

							pFile->Write(&zlibHeader, sizeof(SZLibBlockHeader));
							if (compressionOk)
								pFile->Write(pZLibCompressedBuffer, compressedLength);
							else
								pFile->Write(block->m_pZLibBuffer, block->m_ZLibBufferSizeUsed);
						}
						else
						{
							pFile->Write(block->m_pZLibBuffer, block->m_ZLibBufferSizeUsed);
						}

						delete block;
					}
				}

				pFile->Finish();
				delete pFile;
			}

			FreeBlock(pZLibCompressedBuffer);
		}
	}

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork()
	{
		m_bCancelled = true;
		m_event.Set();
	}

	CFile* CreateFile(CZLibCompressor* pCompressor, const char* pFileName)
	{
		CFile* pFile = new CFile(pCompressor, pFileName, m_event);
		m_files.push(pFile);
		return pFile;
	}

	void IncreaseBlockCount()
	{
		if (m_nMaxQueuedBlocks <= 0)
			return;

		m_bufferAvailableLock.Lock();

		while (m_nBlockCount >= m_nMaxQueuedBlocks)
			m_bufferAvailableCond.Wait(m_bufferAvailableLock);

		m_nBlockCount++;

		m_bufferAvailableLock.Unlock();
	}

	void DecreaseBlockCount()
	{
		if (m_nMaxQueuedBlocks <= 0)
			return;

		m_bufferAvailableLock.Lock();

		if (m_nBlockCount > 0)
			m_nBlockCount--;

		m_bufferAvailableCond.Notify();

		m_bufferAvailableLock.Unlock();
	}

	static uint8* AllocateBlock()
	{
		return new uint8[XMLCPB_ZLIB_BUFFER_SIZE];
	}

	static void FreeBlock(uint8* pBlock)
	{
		delete[] pBlock;
	}

private:

	CryMT::CLocklessPointerQueue<CFile> m_files;
	CryEvent                            m_event;
	CryConditionVariable                m_bufferAvailableCond;
	CryMutex                            m_bufferAvailableLock;
	const int                           m_nMaxQueuedBlocks;
	int m_nBlockCount;
	bool                                m_bCancelled;
};

static CCompressorThread* s_pCompressorThread;

bool InitializeCompressorThread()
{
	if (s_pCompressorThread)
		return false;

	s_pCompressorThread = new CCompressorThread;

	if (!gEnv->pThreadManager->SpawnThread(s_pCompressorThread, "ZLibCompressor"))
	{
		CRY_ASSERT_MESSAGE(false, "Error spawning \"ZLibCompressor\" thread.");
		delete s_pCompressorThread;
		s_pCompressorThread = NULL;
		return false;
	}

	return true;
}

void ShutdownCompressorThread()
{
	if (s_pCompressorThread != NULL)
	{
		s_pCompressorThread->SignalStopWork();
		gEnv->pThreadManager->JoinThread(s_pCompressorThread, eJM_Join);
		delete s_pCompressorThread;
		s_pCompressorThread = NULL;
	}
}

SZLibBlock::SZLibBlock(CZLibCompressor* pCompressor)
	: m_pCompressor(pCompressor)
	, m_ZLibBufferSizeUsed(0)
{
	s_pCompressorThread->IncreaseBlockCount();
	m_pZLibBuffer = CCompressorThread::AllocateBlock();
}

SZLibBlock::~SZLibBlock()
{
	CCompressorThread::FreeBlock(m_pZLibBuffer);
	s_pCompressorThread->DecreaseBlockCount();
}

CZLibCompressor::CZLibCompressor(const char* pFileName)
	: m_bUseZLibCompression(!!CCryActionCVars::Get().g_XMLCPBUseExtraZLibCompression)
	, m_errorWritingIntoFile(false)
{
	m_currentZlibBlock = new SZLibBlock(this);
	m_pFile = s_pCompressorThread->CreateFile(this, pFileName);
}

CZLibCompressor::~CZLibCompressor()
{
	delete m_currentZlibBlock;
}

void CZLibCompressor::WriteDataIntoFile(void* pSrc, uint32 numBytes)
{
	uint32 numBytesLeft = numBytes;
	uint8* pNextData = (uint8*)pSrc;
	do
	{
		AddDataToZLibBuffer(pNextData, numBytesLeft);
	}
	while (numBytesLeft > 0 && !m_errorWritingIntoFile);
}

//////////////////////////////////////////////////////////////////////////
// adds data to the to-compress buffer, and calls for compression+writing when the buffer is filled

void CZLibCompressor::AddDataToZLibBuffer(uint8*& pSrc, uint32& numBytes)
{
	uint32 bytesFree = XMLCPB_ZLIB_BUFFER_SIZE - m_currentZlibBlock->m_ZLibBufferSizeUsed;
	uint32 bytesToCopy = min(bytesFree, numBytes);
	memcpy(m_currentZlibBlock->m_pZLibBuffer + m_currentZlibBlock->m_ZLibBufferSizeUsed, pSrc, bytesToCopy);
	m_currentZlibBlock->m_ZLibBufferSizeUsed += bytesToCopy;

	if (m_currentZlibBlock->m_ZLibBufferSizeUsed == XMLCPB_ZLIB_BUFFER_SIZE)
		CompressAndWriteZLibBufferIntoFile(false);

	pSrc += bytesToCopy;
	numBytes -= bytesToCopy;
}

//////////////////////////////////////////////////////////////////////////
void CZLibCompressor::CompressAndWriteZLibBufferIntoFile(bool bFlush)
{
	if (m_currentZlibBlock->m_ZLibBufferSizeUsed > 0)
	{
		m_pFile->AddBlock(m_currentZlibBlock);
		if (bFlush)
			m_currentZlibBlock = NULL;
		else
			m_currentZlibBlock = new SZLibBlock(this);
	}
	else if (bFlush)
	{
		delete m_currentZlibBlock;
		m_currentZlibBlock = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
// forces a compress + writeintofile of the remaining data in the to-compress zlib buffer

void CZLibCompressor::FlushZLibBuffer()
{
	CompressAndWriteZLibBufferIntoFile(true); // to flush the not-yet saved data
	if (m_pFile)
	{
		m_pFile->Close();
		m_pFile = NULL;
	}
}

} // namespace XMLCPB
