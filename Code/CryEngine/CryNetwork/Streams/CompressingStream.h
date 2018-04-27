// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __COMPRESSINGSTREAM_H__
#define __COMPRESSINGSTREAM_H__

#pragma once

#include "Config.h"
#if INCLUDE_DEMO_RECORDING

	#include "Compression/ArithAlphabet.h"
	#include "Streams/SimpleOutputStream.h"
	#include "Streams/SimpleInputStream.h"

	#if USE_ARITHSTREAM
typedef CArithAlphabetOrder0 TArithAlphabetCompressingStream;
	#endif

class CCompressingOutputStream : public CSimpleOutputStream
{
public:
	static const size_t DEFAULT_BUFFER_SIZE = 64 * 1024;

	CCompressingOutputStream(size_t bufsz = DEFAULT_BUFFER_SIZE);
	~CCompressingOutputStream();

	bool Init(const char* filename);

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CCompressingOutputStream");

		pSizer->Add(*this);
		pSizer->Add(m_tempBuffer, m_bufsz);
	}

private:
	FILE*  m_file;
	char*  m_tempBuffer;
	size_t m_bufsz;

	#if COMPRESSING_STREAM_USE_BURROWS_WHEELER
	char**                          m_tempBW;
	TArithAlphabetCompressingStream m_alphabet;
	#endif

	virtual void Flush(const SStreamRecord* pRecords, size_t numRecords);

	#if COMPRESSING_STREAM_DEBUG_WHAT_GETS_WRITTEN
	int   m_nLog;
	FILE* m_file_txt;
	#endif
	#if COMPRESSING_STREAM_SANITY_CHECK_EVERYTHING
	char* m_throwaway;
	#endif
};

class CCompressingInputStream : public CSimpleInputStream
{
public:
	CCompressingInputStream(size_t bufsz = CCompressingOutputStream::DEFAULT_BUFFER_SIZE);
	~CCompressingInputStream();

	bool Init(const char* filename);

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CCompressingInputStream");

		pSizer->Add(*this);
		pSizer->Add(m_tempBuffer, m_nCapacity);
	}

private:
	virtual void Underflow(SStreamRecord* pStream, size_t& count);

	FILE*                           m_file;
	size_t                          m_nCapacity;

	char*                           m_tempBuffer;
	#if COMPRESSING_STREAM_USE_BURROWS_WHEELER
	char**                          m_tempBW;
	TArithAlphabetCompressingStream m_alphabet;
	#endif

	#if COMPRESSING_STREAM_DEBUG_WHAT_GETS_READ
	int   m_nBlock;
	FILE* m_file_txt;
	#endif
};

#endif

#endif
