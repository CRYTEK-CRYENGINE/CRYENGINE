// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Pool.h"

#include "Memory.h"

namespace Memory
{

/*
   CPool
 */

CPool::CPool(CContext& context) :
	m_pContext(NULL),
	m_pNext(NULL)
{
	context.AddPool(*this);
}

CPool::~CPool()
{
	m_pContext->RemovePool(*this);
}

/*
   CPoolFrameLocal::CBucket
 */

CPoolFrameLocal::CBucket* CPoolFrameLocal::CBucket::Create(uint32 length)
{
	CBucket* pBucket = new CBucket(length);
	if (pBucket->m_bufferSize < length)
	{
		pBucket->Release();
		return NULL;
	}

	return pBucket;
}

CPoolFrameLocal::CBucket::CBucket(uint32 length)
{
	m_buffer = (uint8*) CryModuleMemalign(length, 16);
	m_bufferSize = length;
	Reset();
}

CPoolFrameLocal::CBucket::~CBucket()
{
	CryModuleMemalignFree(m_buffer);
}

//

void CPoolFrameLocal::CBucket::Reset()
{
	m_length = m_bufferSize;
	m_used = 0;
}

size_t CPoolFrameLocal::CBucket::GetGuaranteedAlignment()
{
	return 16;
}

void* CPoolFrameLocal::CBucket::Allocate(uint32 length)
{
	uint32 lengthAligned = Align(length, 16);
	if (lengthAligned < length)
		CryFatalError("Fatal");

	if ((m_length - m_used) < lengthAligned)
		return NULL;

	void* pAddress = &m_buffer[m_used];
	m_used += lengthAligned;
	return pAddress;
}

/*
   CPoolFrameLocal
 */

CPoolFrameLocal* CPoolFrameLocal::Create(CContext& context, uint32 bucketLength)
{
	return new CPoolFrameLocal(context, bucketLength);
}

CPoolFrameLocal::~CPoolFrameLocal()
{
	ReleaseBuckets();
}

//

CPoolFrameLocal::CPoolFrameLocal(CContext& context, uint32 bucketLength) :
	CPool(context),
	m_bucketLength(bucketLength),
	m_bucketCurrent(0)
{
	m_buckets.reserve(16);
}

//

CPoolFrameLocal::CBucket* CPoolFrameLocal::CreateBucket()
{
	uint32 bucketCount = uint32(m_buckets.size());
	if ((m_bucketCurrent + 1) < bucketCount)
	{
		m_bucketCurrent++;
		return m_buckets[m_bucketCurrent];
	}

	CBucket* pBucket = CBucket::Create(m_bucketLength);
	if (!pBucket)
		return NULL;

	m_buckets.push_back(pBucket);
	m_bucketCurrent = uint32(m_buckets.size() - 1);
	return pBucket;
}

void CPoolFrameLocal::ReleaseBuckets()
{
	AUTO_LOCK(m_criticalSection);

	uint32 bucketCount = uint32(m_buckets.size());
	for (uint32 i = 0; i < bucketCount; ++i)
		m_buckets[i]->Release();
	m_buckets.clear();
}

//

void CPoolFrameLocal::Reset()
{
	AUTO_LOCK(m_criticalSection);

	uint32 bucketCount = uint32(m_buckets.size());
	for (uint32 i = 0; i < bucketCount; ++i)
	{
		m_buckets[i]->Reset();
	}

	m_bucketCurrent = 0;
}

// CPool

void* CPoolFrameLocal::Allocate(uint32 length)
{
	if (length == 0)
	{
		return nullptr;
	}

	AUTO_LOCK(m_criticalSection);

	CBucket* pBucket;
	if (!m_buckets.empty())
	{
		pBucket = m_buckets[m_bucketCurrent];
	}
	else
	{
		pBucket = CreateBucket();
		if (!pBucket)
		{
			return nullptr;
		}
	}

	void* pAddress = pBucket->Allocate(length);
	if (!pAddress)
	{
		pBucket = CreateBucket();
		if (!pBucket)
		{
			return nullptr;
		}

		pAddress = pBucket->Allocate(length); // TODO: We're possibly allocating wasted buckets each time this call fails (i.e. requested length is larger than the size of a full bucket).
	}

	return pAddress;
}

size_t CPoolFrameLocal::GetGuaranteedAlignment() const
{
	return CBucket::GetGuaranteedAlignment();
}

void CPoolFrameLocal::Update()
{
	Reset();
}

void CPoolFrameLocal::GetMemoryUsage(ICrySizer* pSizer) const
{
	AUTO_LOCK(m_criticalSection);

	pSizer->AddObject(m_buckets);
	pSizer->AddObject(this, sizeof(*this));
}

} // namespace Memory
