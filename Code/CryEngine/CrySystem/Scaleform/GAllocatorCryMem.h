// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GALLOCATOR_CRYMEM_H_
#define _GALLOCATOR_CRYMEM_H_

#pragma once

#ifdef INCLUDE_SCALEFORM_SDK

	#pragma warning(push)
	#pragma warning(disable : 6326)// Potential comparison of a constant with another constant
	#pragma warning(disable : 6011)// Dereferencing NULL pointer
	#include <GSysAlloc.h>
	#pragma warning(pop)

class GFxMemoryArenaWrapper : public GSysAllocPaged
{
public:
	// GSysAllocPaged interface
	virtual void  GetInfo(Info* i) const;
	virtual void* Alloc(UPInt size, UPInt align);
	virtual bool  Free(void* ptr, UPInt size, UPInt align);

public:
	GFxMemoryArenaWrapper();
	~GFxMemoryArenaWrapper();

public:
	bool AnyActive() const { return m_arenasActive != 0; }

	int  Create(unsigned int arenaID, bool resetCache);
	void Destroy(unsigned int arenaID);

public:
	void SetAlloc(GSysAllocPaged* pAlloc);

public:
	static void InitCVars();

private:
	static int ms_sys_flash_use_arenas;

private:
	enum { MaxNumArenas = 32 };

private:
	CryCriticalSection m_lock;
	GSysAllocPaged*    m_pAlloc;
	unsigned int       m_arenasActive;
	unsigned int       m_arenasResetCache;
	int                m_arenasRefCnt[MaxNumArenas];
};

struct CryGFxMemInterface
{
public:
	struct Stats
	{
		LONG AllocCount;
		LONG FreeCount;
		LONG Allocated;
		LONG AllocatedInHeap;
	};

public:
	virtual Stats                  GetStats() const = 0;
	virtual void                   GetMemoryUsage(ICrySizer* pSizer) const = 0;
	virtual GSysAllocBase*         GetSysAllocImpl() const = 0;
	virtual GFxMemoryArenaWrapper& GetMemoryArenas() = 0;
	virtual float                  GetFlashHeapFragmentation() const = 0;

	virtual ~CryGFxMemInterface() {}
};

class GSysAllocCryMem : public GSysAllocPaged, public CryGFxMemInterface
{
public:
	// GSysAllocPaged interface
	virtual void  GetInfo(Info* i) const;
	virtual void* Alloc(UPInt size, UPInt align);
	virtual bool  Free(void* ptr, UPInt size, UPInt align);

public:
	// CryGFxMemInterface interface
	virtual Stats                  GetStats() const;
	virtual void                   GetMemoryUsage(ICrySizer* pSizer) const;
	virtual GSysAllocBase*         GetSysAllocImpl() const;
	virtual GFxMemoryArenaWrapper& GetMemoryArenas();
	virtual float                  GetFlashHeapFragmentation() const;

public:
	explicit GSysAllocCryMem(size_t addressSpaceSize);
	virtual ~GSysAllocCryMem();

private:
	enum
	{
		MinGranularity              = 64 * 1024,
		FlashHeapAllocSizeThreshold = 1 * 1024 * 1024
	};

private:
	IPageMappingHeap*     m_pHeap;
	const size_t          m_addressSpaceSize;
	Stats                 m_stats;
	GFxMemoryArenaWrapper m_arenas;
};

class GSysAllocStaticCryMem : public CryGFxMemInterface
{
public:
	// CryGFxMemInterface interface
	virtual Stats                  GetStats() const;
	virtual void                   GetMemoryUsage(ICrySizer* pSizer) const;
	virtual GSysAllocBase*         GetSysAllocImpl() const;
	virtual GFxMemoryArenaWrapper& GetMemoryArenas();
	virtual float                  GetFlashHeapFragmentation() const;

public:
	GSysAllocStaticCryMem(size_t poolSize);
	virtual ~GSysAllocStaticCryMem();

	bool IsValid() const { return m_pStaticAlloc && m_pMem; }

private:
	GSysAllocStatic*      m_pStaticAlloc;
	void*                 m_pMem;
	size_t                m_size;
	GFxMemoryArenaWrapper m_arenas;
};

#endif // #ifdef INCLUDE_SCALEFORM_SDK

#endif // #ifndef _GALLOCATOR_CRYMEM_H_
