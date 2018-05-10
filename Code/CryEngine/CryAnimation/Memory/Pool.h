// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IAllocator.h"

namespace Memory {
class CContext;
}

namespace Memory
{

class CPool
{
	friend class CContext;

public:

	virtual void*  Allocate(uint32 length)                 { return nullptr; }

	virtual void   Free(void* pAddress)                    {}

	virtual size_t GetGuaranteedAlignment() const          { return 1; }

	virtual void   Update()                                {}

	virtual void   GetMemoryUsage(ICrySizer* pSizer) const {}

protected:

	CPool(CContext& context);
	virtual ~CPool();

private:

	CContext* m_pContext;
	CPool*    m_pNext;
};

class CPoolFrameLocal : public CPool
{
public:

	static CPoolFrameLocal* Create(CContext& context, uint32 bucketLength);

	virtual void*           Allocate(uint32 length) override;

	virtual size_t          GetGuaranteedAlignment() const override;

	virtual void            Update() override;

	virtual void            GetMemoryUsage(ICrySizer* pSizer) const override;

	void                    ReleaseBuckets();
	void                    Reset();

private:

	CPoolFrameLocal(CContext& context, uint32 bucketLength);
	~CPoolFrameLocal();

	class CBucket
	{
	public:

		static CBucket* Create(uint32 length);
		void            Release() { delete this; }

		void            Reset();

		static size_t   GetGuaranteedAlignment();
		void*           Allocate(uint32 length);

		ILINE void      GetMemoryUsage(ICrySizer* pSizer) const;

	private:

		CBucket(const CBucket&);        // = delete
		void operator=(const CBucket&); // = delete

		CBucket(uint32 length);
		~CBucket();

		uint8* m_buffer;
		uint32 m_bufferSize;
		uint32 m_length;
		uint32 m_used;
	};

	CBucket* CreateBucket();

	CryCriticalSection    m_criticalSection;
	uint32                m_bucketLength;
	std::vector<CBucket*> m_buckets;
	uint32                m_bucketCurrent;
};

ILINE void CPoolFrameLocal::CBucket::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_buffer);
}

} // namespace Memory
