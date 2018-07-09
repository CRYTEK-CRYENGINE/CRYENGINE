// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RecordingBuffer.h"

CRecordingBuffer::CRecordingBuffer(size_t size)
{
	m_pDiscardCallback = NULL;
	m_pDiscardCallbackUserData = NULL;

	m_pBuffer=NULL;
	Init(size,NULL);
}

CRecordingBuffer::CRecordingBuffer(size_t size, unsigned char *buffer)
{
	m_pDiscardCallback = NULL;
	m_pDiscardCallbackUserData = NULL;

	m_pBuffer=NULL;
	Init(size,buffer);
}

CRecordingBuffer::CRecordingBuffer()
{
	m_pDiscardCallback = NULL;
	m_pDiscardCallbackUserData = NULL;

	m_pBuffer=NULL;
	Init(0,NULL);
}

void CRecordingBuffer::Init(size_t size, unsigned char *buffer)
{
	CRY_ASSERT_MESSAGE(m_pBuffer==NULL, "CRecordingBuffer initialised twice");

	m_allocatedBuffer = false;

	if (buffer)
	{
		m_pBuffer = (uint8*)buffer;
	}
	else if (size>0)
	{
		m_pBuffer = (uint8*) new char[size];
		m_allocatedBuffer = true;
	}

	m_dynamicBufferSize = size;
	m_actualBufferSize = size;

	Reset();
}

CRecordingBuffer::~CRecordingBuffer()
{
	if (m_pBuffer && m_allocatedBuffer)
	{
		delete [] (char*) m_pBuffer;
	}
	m_pBuffer = NULL;
}

void CRecordingBuffer::Reset()
{
#ifndef _RELEASE
	if (m_pBuffer)
	{
		memset(m_pBuffer, 0xFE, m_actualBufferSize);
	}
#endif

	m_pStart = m_pBuffer;
	m_usedSize = 0;
	m_dynamicBufferSize = m_actualBufferSize;
}

void CRecordingBuffer::Update()
{
	SRecording_FrameData packet;
	packet.frametime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	AddPacket(packet);
}

void CRecordingBuffer::RemoveFrame()
{
	CRY_ASSERT_MESSAGE(m_usedSize > 0, "Unable to remove frame, no packets found");
	SRecording_Packet* pPacket = (SRecording_Packet*)m_pStart;
	if (pPacket->type == eRBPT_FrameData)
	{
		SRecording_FrameData* pFrameData = (SRecording_FrameData*)pPacket;
		float recordedTime = pFrameData->frametime;
		// If there is frame data then remove packets until we reach the next frame data packet
		do 
		{
			RemovePacket(recordedTime);
			pPacket = (SRecording_Packet*)m_pStart;
		} while (m_usedSize > 0 && pPacket->type != eRBPT_FrameData);
	}
	else
	{
		// If there is no frame data then just remove a single packet
		RemovePacket();
	}
}

void CRecordingBuffer::RemovePacket(float recordedTime)
{
	CRY_ASSERT_MESSAGE(m_usedSize > 0, "Unable to remove packet, no packets found");
	SRecording_Packet* pPacket = (SRecording_Packet*)m_pStart;
	CRY_ASSERT_MESSAGE(pPacket->size <= m_usedSize, "Unable to remove more than we are using");
	if (m_pDiscardCallback)
	{
		m_pDiscardCallback(pPacket, recordedTime, m_pDiscardCallbackUserData);
	}
	m_pStart += pPacket->size;
	if (m_pStart >= m_pBuffer + m_dynamicBufferSize)
	{
		CRY_ASSERT_MESSAGE(m_pStart == m_pBuffer + m_dynamicBufferSize, "The packet size has overrun the size of the dynamic buffer");
		m_pStart = m_pBuffer;
		m_dynamicBufferSize = m_actualBufferSize;
	}
	m_usedSize -= pPacket->size;
	if (m_usedSize == 0)
	{
		Reset();
	}
}

void CRecordingBuffer::EnsureFreeSpace(size_t size)
{
	CRY_ASSERT_MESSAGE(size <= m_actualBufferSize, "Unable to clear enough space");
	while (true)
	{
		uint8* pEnd = GetEnd();
		size_t freeSpaceLeft = 0;
		if (m_pStart > pEnd)
		{
			freeSpaceLeft = m_pStart - pEnd;
		}
		else if (m_pStart == pEnd)
		{
			if (m_usedSize == 0)
			{
				// We have cleared the entire buffer by this point
				break;
			}
			else
			{
				// No free space at all, the buffer is entirely full
			}
		}
		else
		{
			// We need a continuous block of memory, so take the greatest of either
			// the end bit or the start bit
			size_t endSpace = m_pBuffer + m_actualBufferSize - pEnd;
			size_t startSpace = m_pStart - m_pBuffer;
			freeSpaceLeft = max(endSpace, startSpace);
		}
		if (freeSpaceLeft >= size)
		{
			// That's enough space, don't need to clear any more
			break;
		}
		RemoveFrame();
	};
}

SRecording_Packet *CRecordingBuffer::AllocEmptyPacket(
	int			inSize,
	uint8		inType)
{
	SRecording_Packet		*result=NULL;

	CRY_ASSERT_MESSAGE((inSize % 4) == 0, "The packet size must be 4 byte aligned.");
	CRY_ASSERT_MESSAGE(inSize >= sizeof(SRecording_Packet), "The size of the packet must not be large enough to hold a SRecording_Packet header.");
	CRY_ASSERT_MESSAGE(inType != eRBPT_Invalid, "The type of the packet must not be eRBT_Invalid. Has it been initialised properly?");

	EnsureFreeSpace(inSize);

	uint8* pEnd = GetEnd();
	size_t distToEnd = (m_pBuffer + m_dynamicBufferSize) - pEnd;
	if (distToEnd >= (size_t)inSize)
	{
		// We have enough space at the end of the buffer so add it to the end
		result=reinterpret_cast<SRecording_Packet*>(pEnd);
	}
	else
	{
		// There is no space left at the end, so wrap round to the start again
		result=reinterpret_cast<SRecording_Packet*>(m_pBuffer);

		// The dynamic buffer size is reduced to account for the fact that we are not using the last few bytes of the buffer
		m_dynamicBufferSize = m_actualBufferSize - distToEnd;
	}

	m_usedSize += inSize;

	CRY_ASSERT_MESSAGE(m_usedSize <= m_dynamicBufferSize, "Can't use more memory than we have in the buffer, something has gone wrong here");

	result->type=inType;
	result->size=inSize;

	return result;
}

void CRecordingBuffer::AddPacket(const SRecording_Packet& packet)
{
	SRecording_Packet	*newPacket=AllocEmptyPacket(packet.size,packet.type);

	memcpy(newPacket,&packet,packet.size);
}

size_t CRecordingBuffer::GetData(uint8 *pBuffer, size_t bufferSize) const
{
	CRY_ASSERT_MESSAGE(bufferSize >= m_usedSize, "The buffer is not large enough to contain all the data");
	size_t copiedSize = min(bufferSize, m_usedSize);
	if (m_pStart + copiedSize <= m_pBuffer + m_dynamicBufferSize)
	{
		// All the data is in one continuous block
		memcpy(pBuffer, m_pStart, copiedSize);
	}
	else
	{
		// The data is split into two blocks
		size_t firstPartSize = (m_pBuffer + m_dynamicBufferSize) - m_pStart;
		size_t secondPartSize = copiedSize - firstPartSize;
		memcpy(pBuffer, m_pStart, firstPartSize);
		memcpy(pBuffer + firstPartSize, m_pBuffer, secondPartSize);
	}
	return copiedSize;
}

// returns whether the passed ptr is from the memory buffer used by this object
// does not indicate whether the ptr falls into an allocated block currently
bool CRecordingBuffer::ContainsPtr(
	const void		*inPtr) const
{
	return (inPtr>=m_pBuffer && inPtr<(m_pBuffer+m_actualBufferSize));
}

