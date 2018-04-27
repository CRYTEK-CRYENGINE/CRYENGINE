// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_BufferWriter.h"
#include "XMLCPB_Writer.h"

using namespace XMLCPB;

//////////////////////////////////////////////////////////////////////////

void CBufferWriter::SSimpleBuffer::WriteToFile(CWriter& writer)
{
	if (m_used > 0)
		writer.WriteDataIntoFile(GetBasePointer(), m_used);
}

void CBufferWriter::SSimpleBuffer::WriteToMemory(CWriter& Writer, uint8*& rpData, uint32& outWriteLoc)
{
	if (m_used > 0)
		Writer.WriteDataIntoMemory(rpData, GetBasePointer(), m_used, outWriteLoc);
}

//////////////////////////////////////////////////////////////////////////

CBufferWriter::CBufferWriter(int bufferSize)
	: m_bufferSize(bufferSize)
	, m_usingStreaming(false)
	, m_pWriter(NULL)
{
	assert(bufferSize <= 64 * 1024); // because SAddr is using 16 bits to offset inside buffers
}

void CBufferWriter::Init(CWriter* pWriter, bool useStreaming)
{
	assert(pWriter);
	m_pWriter = pWriter;
	m_usingStreaming = useStreaming;
	AddBuffer();
}

//////////////////////////////////////////////////////////////////////////

void CBufferWriter::AddBuffer()
{
	if (m_usingStreaming && !m_buffers.empty())
	{
		m_buffers.back()->WriteToFile(*m_pWriter);
		m_buffers.back()->FreeBuffer();
	}

	SSimpleBufferT pBuffer = SSimpleBufferT(new SSimpleBuffer(m_bufferSize));

	m_buffers.push_back(pBuffer);
}

//////////////////////////////////////////////////////////////////////////

void CBufferWriter::GetCurrentAddr(SAddr& addr) const
{
	addr.bufferIndex = static_cast<uint16>(m_buffers.size() - 1);
	addr.bufferOffset = m_buffers.back()->GetUsed();
}

//////////////////////////////////////////////////////////////////////////

FlatAddr CBufferWriter::ConvertSegmentedAddrToFlatAddr(const SAddr& addr)
{
	FlatAddr flatAddr = addr.bufferOffset;

	for (int b = 0; b < addr.bufferIndex; ++b)
	{
		flatAddr += m_buffers[b]->GetUsed();
	}

	return flatAddr;
}

//////////////////////////////////////////////////////////////////////////

void CBufferWriter::AddData(const void* _pSource, int size)
{
	const uint8* pSource = (const uint8*)_pSource;

	SSimpleBuffer* pBuf = m_buffers.back().get();

	while (!pBuf->CanAddData(size))
	{
		uint32 size1 = pBuf->GetFreeSpaceSize();
		pBuf->AddData(pSource, size1);

		pSource += size1;
		size = size - size1;

		AddBuffer();
		pBuf = m_buffers.back().get();
	}

	pBuf->AddData(pSource, size);

	if (pBuf->IsFull())
		AddBuffer();
}

//////////////////////////////////////////////////////////////////////////
// NoSplit means that all the data will be in the same internal buffer, so all of it can be read directly using the address conversion

void CBufferWriter::AddDataNoSplit(SAddr& addr, const void* _pSource, int size)
{
	const uint8* pSource = (const uint8*)_pSource;

	if (!m_buffers.back()->CanAddData(size))
		AddBuffer();

	GetCurrentAddr(addr);

	SSimpleBufferT& buffer = m_buffers.back();
	buffer->AddData(pSource, size);

	if (buffer->IsFull())
		AddBuffer();
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////

uint32 CBufferWriter::GetUsedMemory() const
{
	uint32 memTot = 0;

	for (uint i = 0; i < m_buffers.size(); i++)
	{
		memTot += m_buffers[i]->GetUsed();
	}

	return memTot;
}

//////////////////////////////////////////////////////////////////////////
// when is using streaming, this actually writes only the remaning not-yet writen data

void CBufferWriter::WriteToFile()
{
	if (m_usingStreaming)
	{
		m_buffers.back()->WriteToFile(*m_pWriter);
		m_buffers.back()->FreeBuffer();
	}
	else
	{
		for (uint b = 0; b < m_buffers.size(); ++b)
		{
			SSimpleBufferT& buffer = m_buffers[b];
			buffer->WriteToFile(*m_pWriter);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CBufferWriter::WriteToMemory(uint8*& rpData, uint32& outWriteLoc)
{
	for (uint b = 0; b < m_buffers.size(); ++b)
	{
		SSimpleBufferT& buffer = m_buffers[b];
		buffer->WriteToMemory(*m_pWriter, rpData, outWriteLoc);
	}
}

//////////////////////////////////////////////////////////////////////////

uint32 CBufferWriter::GetDataSize() const
{
	uint32 uSize = 0;
	for (uint b = 0; b < m_buffers.size(); ++b)
	{
		const SSimpleBufferT& buffer = m_buffers[b];
		uSize += buffer->GetDataSize();
	}
	return uSize;
}
