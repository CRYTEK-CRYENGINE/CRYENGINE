// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once
#ifndef XMLCPB_ZLIBCOMPRESSOR_H
	#define XMLCPB_ZLIBCOMPRESSOR_H

	#include <CryCore/Platform/IPlatformOS.h>
	#include "../XMLCPB_Common.h"

namespace XMLCPB {

bool InitializeCompressorThread();
void ShutdownCompressorThread();

//////////////////////////////////////////////////////////////////////////

struct SZLibBlock
{
	SZLibBlock(class CZLibCompressor* pCompressor);
	~SZLibBlock();

	CZLibCompressor* m_pCompressor;
	uint8*           m_pZLibBuffer;                         // data that is going to be compressed is stored here
	uint32           m_ZLibBufferSizeUsed;                  // how much of m_pZLibBuffer is currently used
};

class CZLibCompressor
{
public:
	CZLibCompressor(const char* pFileName);
	~CZLibCompressor();

	void         WriteDataIntoFile(void* pSrc, uint32 numBytes);
	void         AddDataToZLibBuffer(uint8*& pSrc, uint32& numBytes);
	void         CompressAndWriteZLibBufferIntoFile(bool bFlush);
	void         FlushZLibBuffer();
	SFileHeader& GetFileHeader() { return m_fileHeader; }

public:

	SFileHeader  m_fileHeader;                              // actually a footer
	class CFile* m_pFile;
	SZLibBlock*  m_currentZlibBlock;
	bool         m_bUseZLibCompression;
	bool         m_errorWritingIntoFile;
};

}  // end namespace

#endif // XMLCPB_ZLIBCOMPRESSOR_H
