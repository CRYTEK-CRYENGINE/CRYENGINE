// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:  Helper classes for remote command system
   -------------------------------------------------------------------------
   History:
   - 17/04/2013   : Tomasz Jonarski, Created

*************************************************************************/
#include "StdAfx.h"
#include <CryNetwork/IServiceNetwork.h>
#include "RemoteCommandHelpers.h"

//-----------------------------------------------------------------------------

CDataReadStreamFormMessage::CDataReadStreamFormMessage(const IServiceNetworkMessage* message)
	: m_pMessage(message)
	, m_size(message->GetSize())
	, m_pData(static_cast<const char*>(message->GetPointer()))
	, m_offset(0)
{
	// AddRef() is not const unfortunatelly
	const_cast<IServiceNetworkMessage*>(m_pMessage)->AddRef();
}

CDataReadStreamFormMessage::~CDataReadStreamFormMessage()
{
	// Release() is not const unfortunatelly
	const_cast<IServiceNetworkMessage*>(m_pMessage)->Release();
}

void CDataReadStreamFormMessage::Delete()
{
	delete this;
}

void CDataReadStreamFormMessage::Skip(const uint32 size)
{
	CRY_ASSERT(m_offset + size < m_size);
	m_offset += size;
}

void CDataReadStreamFormMessage::Read(void* pData, const uint32 size)
{
	CRY_ASSERT(m_offset + size < m_size);
	const char* pReadPtr = m_pData + m_offset;
	memcpy(pData, pReadPtr, size);
	m_offset += size;
}

void CDataReadStreamFormMessage::Read8(void* pData)
{
	// it does not actually matter if its uint64, int64 or double so use any
	ReadType<uint64>(pData);
}

void CDataReadStreamFormMessage::Read4(void* pData)
{
	// it does not actually matter if its uint32, int32 or float so use any
	ReadType<uint32>(pData);
}

void CDataReadStreamFormMessage::Read2(void* pData)
{
	// it does not actually matter if its uint16, int16 so use any
	ReadType<uint16>(pData);
}

void CDataReadStreamFormMessage::Read1(void* pData)
{
	// it does not actually matter if its uint8, int8 so use any
	ReadType<uint8>(pData);
}

const void* CDataReadStreamFormMessage::GetPointer()
{
	const char* pReadPtr = m_pData + m_offset;
	return pReadPtr;
}

//-----------------------------------------------------------------------------

CDataWriteStreamToMessage::CDataWriteStreamToMessage(IServiceNetworkMessage* pMessage)
	: m_pMessage(pMessage)
	, m_size(pMessage->GetSize())
	, m_pData(static_cast<char*>(pMessage->GetPointer()))
	, m_offset(0)
{
	m_pMessage->AddRef();
}

CDataWriteStreamToMessage::~CDataWriteStreamToMessage()
{
	m_pMessage->Release();
}

void CDataWriteStreamToMessage::Delete()
{
	delete this;
}

const uint32 CDataWriteStreamToMessage::GetSize() const
{
	return m_size;
}

void CDataWriteStreamToMessage::CopyToBuffer(void* pData) const
{
	memcpy(pData, m_pData, m_size);
}

IServiceNetworkMessage* CDataWriteStreamToMessage::BuildMessage() const
{
	m_pMessage->AddRef();
	return m_pMessage;
}

void CDataWriteStreamToMessage::Write(const void* pData, const uint32 size)
{
	CRY_ASSERT(m_offset + size < m_size);
	memcpy((char*)m_pData + m_offset, pData, size);
	m_offset += size;
}

void CDataWriteStreamToMessage::Write8(const void* pData)
{
	// it does not actually matter if its uint64, int64 or double so use any
	WriteType<uint64>(pData);
}

void CDataWriteStreamToMessage::Write4(const void* pData)
{
	// it does not actually matter if its uint32, int32 or float so use any
	WriteType<uint32>(pData);
}

void CDataWriteStreamToMessage::Write2(const void* pData)
{
	// it does not actually matter if its uint16, int16 so use any
	WriteType<uint16>(pData);
}

void CDataWriteStreamToMessage::Write1(const void* pData)
{
	// it does not actually matter if its uint8, int8 so use any
	WriteType<uint8>(pData);
}

//-----------------------------------------------------------------------------

CDataReadStreamMemoryBuffer::CDataReadStreamMemoryBuffer(const void* pData, const uint32 size)
	: m_size(size)
	, m_offset(0)
{
	m_pData = new uint8[size];
	memcpy(m_pData, pData, size);
}

CDataReadStreamMemoryBuffer::~CDataReadStreamMemoryBuffer()
{
	delete[] m_pData;
	m_pData = NULL;
}

void CDataReadStreamMemoryBuffer::Delete()
{
	delete this;
}

void CDataReadStreamMemoryBuffer::Skip(const uint32 size)
{
	CRY_ASSERT(m_offset + size <= m_size);
	m_offset += size;
}

void CDataReadStreamMemoryBuffer::Read8(void* pData)
{
	Read(pData, 8);
	SwapEndian(*reinterpret_cast<uint64*>(pData));
}

void CDataReadStreamMemoryBuffer::Read4(void* pData)
{
	Read(pData, 4);
	SwapEndian(*reinterpret_cast<uint32*>(pData));
}

void CDataReadStreamMemoryBuffer::Read2(void* pData)
{
	Read(pData, 2);
	SwapEndian(*reinterpret_cast<uint16*>(pData));
}

void CDataReadStreamMemoryBuffer::Read1(void* pData)
{
	return Read(pData, 1);
}

const void* CDataReadStreamMemoryBuffer::GetPointer()
{
	return m_pData + m_offset;
};

void CDataReadStreamMemoryBuffer::Read(void* pData, const uint32 size)
{
	CRY_ASSERT(m_offset + size <= m_size);
	memcpy(pData, m_pData + m_offset, size);
	m_offset += size;
}

//-----------------------------------------------------------------------------

CDataWriteStreamBuffer::CDataWriteStreamBuffer()
	: m_size(0)
{
	// Start with the initial (preallocated) partition
	// This optimization assumes that initial size of most of the messages will be small.
	// NOTE: default partition is not added to the partition table (that would require push_backs to vector)
	char* partitionMemory = &m_defaultPartition[0];
	m_pCurrentPointer = partitionMemory;
	m_leftInPartition = sizeof(m_defaultPartition);
}

CDataWriteStreamBuffer::~CDataWriteStreamBuffer()
{
	// Free all memory partitions that were allocated dynamically
	for (size_t i = 0; i < m_pPartitions.size(); ++i)
	{
		free(m_pPartitions[i]);
	}
}

void CDataWriteStreamBuffer::Delete()
{
	delete this;
}

const uint32 CDataWriteStreamBuffer::GetSize() const
{
	return m_size;
}

void CDataWriteStreamBuffer::CopyToBuffer(void* pData) const
{
	uint32 dataLeft = m_size;
	char* pWritePtr = (char*)pData;

	// Copy data from default (preallocated) partition
	{
		const uint32 partitionSize = sizeof(m_defaultPartition);
		const uint32 dataToCopy = min<uint32>(partitionSize, dataLeft);
		memcpy(pWritePtr, &m_defaultPartition[0], dataToCopy);

		// advance
		pWritePtr += dataToCopy;
		dataLeft -= dataToCopy;
	}

	// Copy data from dynamic partitions
	for (uint32 i = 0; i < m_pPartitions.size(); ++i)
	{
		// get size of data to copy
		const uint32 partitionSize = m_partitionSizes[i];
		const uint32 dataToCopy = min<uint32>(partitionSize, dataLeft);
		memcpy(pWritePtr, m_pPartitions[i], dataToCopy);

		// advance
		pWritePtr += dataToCopy;
		dataLeft -= dataToCopy;
	}

	// Make sure all data was written
	CRY_ASSERT(dataLeft == 0);
}

IServiceNetworkMessage* CDataWriteStreamBuffer::BuildMessage() const
{
	// No data written, no message created
	if (0 == m_size)
	{
		return NULL;
	}

	// Create message to hold all the data
	IServiceNetworkMessage* pMessage = gEnv->pServiceNetwork->AllocMessageBuffer(m_size);
	if (NULL == pMessage)
	{
		return NULL;
	}

	// Copy data to messages
	CopyToBuffer(pMessage->GetPointer());
	return pMessage;
}

void CDataWriteStreamBuffer::Write(const void* pData, const uint32 size)
{
	static const uint32 kAdditionalPartitionSize = 65536;

	uint32 dataLeft = size;
	while (dataLeft > 0)
	{
		// new partition needed
		if (m_leftInPartition == 0)
		{
			// Allocate new partition data
			char* partitionMemory = (char*)malloc(kAdditionalPartitionSize);
			CRY_ASSERT(partitionMemory != NULL);

			// add new partition to list
			m_partitionSizes.push_back(kAdditionalPartitionSize);
			m_pPartitions.push_back(partitionMemory);
			m_pCurrentPointer = partitionMemory;
			m_leftInPartition = kAdditionalPartitionSize;
		}

		// how many bytes can we write to current partition ?
		const uint32 maxToWrite = min<uint32>(m_leftInPartition, dataLeft);
		memcpy(m_pCurrentPointer, pData, maxToWrite);

		// advance
		m_size += maxToWrite;
		dataLeft -= maxToWrite;
		pData = (const char*)pData + maxToWrite;
		m_pCurrentPointer += maxToWrite;
		m_leftInPartition -= maxToWrite;
	}
}

void CDataWriteStreamBuffer::Write8(const void* pData)
{
	// it does not actually matter if its uint64, int64 or double so use any
	WriteType<uint64>(pData);
}

void CDataWriteStreamBuffer::Write4(const void* pData)
{
	// it does not actually matter if its uint32, int32 or float so use any
	WriteType<uint32>(pData);
}

void CDataWriteStreamBuffer::Write2(const void* pData)
{
	// it does not actually matter if its uint16, int16 so use any
	WriteType<uint16>(pData);
}

void CDataWriteStreamBuffer::Write1(const void* pData)
{
	// it does not actually matter if its uint8, int8 so use any
	WriteType<uint8>(pData);
}

//-----------------------------------------------------------------------------
