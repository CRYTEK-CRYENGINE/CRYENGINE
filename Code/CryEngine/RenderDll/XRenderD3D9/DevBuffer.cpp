// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   DevBuffer.cpp : Generic device Buffer management

   =============================================================================*/

#include "StdAfx.h"
#include <numeric>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryMemory/IMemory.h>
#include "DriverD3D.h"

CryCriticalSection CGraphicsDeviceConstantBuffer::s_accessLock;

#if defined(min)
	#undef min
#endif
#if defined(max)
	#undef max
#endif

// Change this to != 0 to enable runtime assertions on the devbufferman
#if !defined(_RELEASE)
	#define DEVBUFFERMAN_DEBUG 1
#else
	#define DEVBUFFERMAN_DEBUG 0
#endif

#if DEVBUFFERMAN_DEBUG
	#define DEVBUFFERMAN_ASSERT(x) \
	  do {                         \
	    if (!(x))                  \
	      __debugbreak();          \
	  } while (false)
#else
	#define DEVBUFFERMAN_ASSERT(x) (void)0
#endif

// VERIFY
#define DEVBUFFERMAN_VERIFY(x) \
  do {                         \
    if (!(x))                  \
      __debugbreak();          \
  } while (false)

#ifdef TRACK_DEVBUFFER_WITH_MEMREPLAY
	#define DB_MEMREPLAY_SCOPE(a, b)               MEMREPLAY_SCOPE(a, b)
	#define DB_MEMREPLAY_SCOPE_ALLOC(a, b, c)      MEMREPLAY_SCOPE_ALLOC(a, b, c)
	#define DB_MEMREPLAY_SCOPE_REALLOC(a, b, c, d) MEMREPLAY_SCOPE_REALLOC(a, b, c, d)
	#define DB_MEMREPLAY_SCOPE_FREE(id)            MEMREPLAY_SCOPE_FREE(id)
#else
	#define DB_MEMREPLAY_SCOPE(...)
	#define DB_MEMREPLAY_SCOPE_ALLOC(...)
	#define DB_MEMREPLAY_SCOPE_REALLOC(...)
	#define DB_MEMREPLAY_SCOPE_FREE(id)
#endif

#define BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS)                                                 \
  (BIND_FLAGS == CDeviceManager::BIND_VERTEX_BUFFER ? D3D11_MAP_WRITE_NO_OVERWRITE_VB :      \
   (BIND_FLAGS == CDeviceManager::BIND_INDEX_BUFFER ? D3D11_MAP_WRITE_NO_OVERWRITE_IB :      \
    (BIND_FLAGS == CDeviceManager::BIND_CONSTANT_BUFFER ? D3D11_MAP_WRITE_NO_OVERWRITE_CB :  \
     (BIND_FLAGS == CDeviceManager::BIND_SHADER_RESOURCE ? D3D11_MAP_WRITE_NO_OVERWRITE_SR : \
      (BIND_FLAGS == CDeviceManager::BIND_UNORDERED_ACCESS ? D3D11_MAP_WRITE_NO_OVERWRITE_UA : D3D11_MAP_WRITE_NO_OVERWRITE)))))

#define BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS)                                           \
  (BIND_FLAGS == CDeviceManager::BIND_VERTEX_BUFFER ? D3D11_MAP_WRITE_DISCARD_VB :      \
   (BIND_FLAGS == CDeviceManager::BIND_INDEX_BUFFER ? D3D11_MAP_WRITE_DISCARD_IB :      \
    (BIND_FLAGS == CDeviceManager::BIND_CONSTANT_BUFFER ? D3D11_MAP_WRITE_DISCARD_CB :  \
     (BIND_FLAGS == CDeviceManager::BIND_SHADER_RESOURCE ? D3D11_MAP_WRITE_DISCARD_SR : \
      (BIND_FLAGS == CDeviceManager::BIND_UNORDERED_ACCESS ? D3D11_MAP_WRITE_DISCARD_UA : D3D11_MAP_WRITE_DISCARD)))))

#if ENABLE_STATOSCOPE
struct SStatoscopeData
{
	size_t m_written_bytes;      // number of writes
	size_t m_read_bytes;         // number of reads
	int64  m_creator_time;       // time spent in others
	int64  m_io_time;            // time spent in maps
	int64  m_cpu_flush_time;     // time spent flushing the cpu
	int64  m_gpu_flush_time;     // time spent flushing the gpu
};

struct SStatoscopeTimer
{
	int64  start;
	int64* value;
	SStatoscopeTimer(int64* _value)
		: start(CryGetTicks())
		, value(_value)
	{}

	~SStatoscopeTimer()
	{
		* value += CryGetTicks() - start;
	}
};

SStatoscopeData& GetStatoscopeData(uint32 nIndex);

	#define STATOSCOPE_TIMER(x)      SStatoscopeTimer _timer(&(x))
	#define STATOSCOPE_IO_WRITTEN(y) GetStatoscopeData(0).m_written_bytes += (y)
	#define STATOSCOPE_IO_READ(y)    GetStatoscopeData(0).m_read_bytes += (y)
#else
	#define STATOSCOPE_TIMER(x)      (void)0
	#define STATOSCOPE_IO_WRITTEN(y) (void)0
	#define STATOSCOPE_IO_READ(y)    (void)0
#endif

//===============================================================================
//////////////////////////////////////////////////////////////////////////////////////////
// The buffer invalidations
struct SBufferInvalidation
{
	D3DBuffer* buffer;
	void*      base_ptr;
	size_t     offset;
	size_t     size;
	bool operator<(const SBufferInvalidation& other) const
	{
		if (buffer == other.buffer)
			return offset < other.offset;
		return buffer < other.buffer;
	}
	bool operator!=(const SBufferInvalidation& other) const
	{
		return buffer != other.buffer
#if CRY_PLATFORM_DURANGO  // Should be removed when we have range based invalidations
		       && offset != other.offset
#endif
		;
	}
};
typedef std::vector<SBufferInvalidation> BufferInvalidationsT;
// returns a reference to the internal buffer invalidates (used to break cyclic depencies in file)
BufferInvalidationsT& GetBufferInvalidations(uint32 threadid);

namespace
{

static inline bool CopyData(void* dst, const void* src, size_t size)
{
	bool requires_flush = true;
#if defined(_CPU_SSE)
	if ((((uintptr_t)dst | (uintptr_t)src | size) & 0xf) == 0u)
	{
		__m128* d = (__m128*)dst;
		const __m128* s = (const __m128*)src;
		const __m128* e = (const __m128*)src + (size >> 4);
		while (s < e)
			_mm_stream_ps((float*)(d++), _mm_load_ps((const float*)(s++)));
		_mm_sfence();
		requires_flush = false;
	}
	else
#endif
	{
		cryMemcpy(dst, src, size, MC_CPU_TO_GPU);
	}
	return requires_flush;
}

//===============================================================================
struct SPoolConfig
{
	enum
	{
		POOL_STAGING_COUNT       = 1,
		POOL_ALIGNMENT           = 128,
		POOL_FRAME_QUERY_COUNT   = 4,
#if CRY_PLATFORM_ORBIS
		POOL_MAX_ALLOCATION_SIZE = 4 << 20,
#else
		POOL_MAX_ALLOCATION_SIZE = 64 << 20,
#endif
		POOL_FRAME_QUERY_MASK    = POOL_FRAME_QUERY_COUNT - 1
	};

	size_t m_pool_bank_size;
	size_t m_transient_pool_size;
	size_t m_cb_bank_size;
	size_t m_cb_threshold;
	size_t m_pool_bank_mask;
	size_t m_pool_max_allocs;
	size_t m_pool_max_moves_per_update;
	bool   m_pool_defrag_static;
	bool   m_pool_defrag_dynamic;

	bool   Configure()
	{
		m_pool_bank_size = (size_t)NextPower2(gRenDev->CV_r_buffer_banksize) << 20;
		m_transient_pool_size = (size_t)NextPower2(gRenDev->CV_r_transient_pool_size) << 20;
		m_cb_bank_size = NextPower2(gRenDev->CV_r_constantbuffer_banksize) << 20;
		m_cb_threshold = NextPower2(gRenDev->CV_r_constantbuffer_watermark) << 20;
		m_pool_bank_mask = m_pool_bank_size - 1;
		m_pool_max_allocs = gRenDev->CV_r_buffer_pool_max_allocs;
		m_pool_defrag_static = gRenDev->CV_r_buffer_pool_defrag_static != 0;
		m_pool_defrag_dynamic = gRenDev->CV_r_buffer_pool_defrag_dynamic != 0;
		if (m_pool_defrag_static | m_pool_defrag_dynamic)
			m_pool_max_moves_per_update = gRenDev->CV_r_buffer_pool_defrag_max_moves;
		else
			m_pool_max_moves_per_update = 0;
		return true;
	}
};
static SPoolConfig s_PoolConfig;

static const char* ConstantToString(BUFFER_USAGE usage)
{
	switch (usage)
	{
	case BU_IMMUTABLE:
		return "IMMUTABLE";
	case BU_STATIC:
		return "STATIC";
	case BU_DYNAMIC:
		return "DYNAMIC";
	case BU_TRANSIENT:
		return "BU_TRANSIENT";
	case BU_TRANSIENT_RT:
		return "BU_TRANSIENT_RT";
	case BU_WHEN_LOADINGTHREAD_ACTIVE:
		return "BU_WHEN_LOADINGTHREAD_ACTIVE";
	}
	return NULL;
}

static const char* ConstantToString(BUFFER_BIND_TYPE type)
{
	switch (type)
	{
	case BBT_VERTEX_BUFFER:
		return "VB";
	case BBT_INDEX_BUFFER:
		return "IB";
	case BBT_CONSTANT_BUFFER:
		return "CB";
	}
	return NULL;
}

static inline int _GetThreadID()
{
	return gRenDev->m_pRT->IsRenderThread() ? gRenDev->m_RP.m_nProcessThreadID : gRenDev->m_RP.m_nFillThreadID;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// If buffer is bound to device, remove it from device (Must be called before releasing the buffer!)
static inline void UnsetStreamSources(D3DBuffer* buffer)
{
	gcpRendD3D->FX_UnbindBuffer(buffer);
}

//===============================================================================
// forward declarations
struct SBufferPoolBank;
struct SBufferPool;
struct SBufferPoolItem;

//////////////////////////////////////////////////////////////////////////////////////////
// Utility class that provides small constant time allocation and deallocation at the cost
// of requiring two indirections to get to the final storage.
//
// Note: Never shrinks, could be made shrinkable by moving allocated items before
//       the partition marker
template<typename T, size_t TableSize = 4u << 10>
class CPartitionTable
{
	enum
	{
		TableShift = CompileTimeIntegerLog2<TableSize>::result,
		TableMask  = TableSize - 1,
		MaxTables  = 0x2ff,
	};
	typedef std::vector<uint32> key_storage_t;

	T**           m_storage;
	key_storage_t m_table;
	key_storage_t m_remap_table;

	uint32        m_partition;
	uint32        m_capacity;

	CPartitionTable<T>& operator=(const CPartitionTable<T>& other);
	CPartitionTable(const CPartitionTable<T>& other);

public:

	CPartitionTable<T>& operator=(CPartitionTable<T>&& other)
	{
		m_storage = std::move(other.m_storage);
		m_table = std::move(other.m_table);
		m_remap_table = std::move(other.m_remap_table);
		m_partition = std::move(other.m_partition);
		m_capacity = std::move(other.m_capacity);

		other.m_capacity = 0;
		other.m_storage = 0;

		return *this;
	}

	CPartitionTable(CPartitionTable<T>&& other)
		: m_storage(std::move(other.m_storage))
		, m_table(std::move(other.m_table))
		, m_remap_table(std::move(other.m_remap_table))
		, m_partition(std::move(other.m_partition))
		, m_capacity(std::move(other.m_capacity))
	{
		other.m_capacity = 0;
		other.m_storage = 0;
	}

	CPartitionTable()
		: m_storage()
		, m_table()
		, m_remap_table()
		, m_partition()
		, m_capacity()
	{
		m_storage = (T**)realloc(m_storage, MaxTables * sizeof(T*));
		memset(m_storage, 0x0, MaxTables * sizeof(T*));
	}

	~CPartitionTable()
	{
		for (size_t i = 0; i < (m_capacity >> TableShift); ++i)
			CryModuleMemalignFree(m_storage[i]);
		realloc(m_storage, 0);
	}

	uint32 Capacity() const { return m_capacity; }
	uint32 Count() const    { return m_partition; }

	T&     operator[](size_t key)
	{
		size_t table = key >> TableShift;
		size_t index = key & TableMask;
		return m_storage[table][index];
	}

	const T& operator[](size_t key) const
	{
		size_t table = key >> TableShift;
		size_t index = key & TableMask;
		return m_storage[table][index];
	}

	uint32 Allocate()
	{
		size_t key = ~0u;
		IF (m_partition + 1 >= m_capacity, 0)
		{
			size_t old_capacity = m_capacity;
			m_capacity += TableSize;
			if ((m_capacity >> TableShift) > MaxTables)
				CryFatalError("ran out of number of items, please increase MaxTables enum");
			m_storage[old_capacity >> TableShift] = (T*)(CryModuleMemalign(TableSize * sizeof(T), 16));
			m_table.resize(m_capacity, uint32());
			m_remap_table.resize(m_capacity, uint32());
			std::iota(m_table.begin() + old_capacity, m_table.end(), old_capacity);
		}
		uint32 storage_index = m_table[key = m_partition++];
		m_remap_table[storage_index] = key;
		new(&(this->operator[](storage_index)))T(storage_index);
		return storage_index;
	}

	void Free(item_handle_t key)
	{
		DEVBUFFERMAN_ASSERT(m_partition && key < m_remap_table.size());
		uint32 roster_index = m_remap_table[key];
		(&(this->operator[](key)))->~T();
		std::swap(m_table[roster_index], m_table[--m_partition]);
		std::swap(m_remap_table[key], m_remap_table[m_table[roster_index]]);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
// A backing device buffer serving as a memory bank from which further allocations can be sliced out
//
struct SBufferPoolBank
{
	// The pointer to backing device buffer.
	D3DBuffer* m_buffer;

	// Base pointer to buffer (used on platforms with unified memory)
	uint8* m_base_ptr;

	// Size of the backing buffer
	size_t m_capacity;

	// Number of allocated bytes from within the buffer
	size_t m_free_space;

	// Handle into the bank table
	size_t m_handle;

	SBufferPoolBank(size_t handle)
		: m_buffer()
		, m_base_ptr(NULL)
		, m_capacity()
		, m_free_space()
		, m_handle(handle)
	{}

	~SBufferPoolBank()
	{
		UnsetStreamSources(m_buffer);
		SAFE_RELEASE(m_buffer);
	}
};
typedef CPartitionTable<SBufferPoolBank> CBufferPoolBankTable;

//////////////////////////////////////////////////////////////////////////
// An allocation within a pool bank is represented by this structure
//
// Note: In case the allocation request could not be satisfied by a pool
// the pool item contains a pointer to the backing buffer directly.
// On destruction the backing device buffer will be released.
struct SBufferPoolItem
{
	// The pointer to the backing buffer
	D3DBuffer* m_buffer;

	// The pool that maintains this item (will be null if pool-less)
	SBufferPool* m_pool;

	// Base pointer to buffer
	uint8* m_base_ptr;

	// The pointer to the defragging allocator if backed by one
	IDefragAllocator* m_defrag_allocator;

	// The intrusive list member for deferred unpinning/deletion
	// Note: only one list because deletion overrides unpinning
	util::list<SBufferPoolItem> m_deferred_list;

	// The intrusive list member for deferred relocations
	// due to copy on writes performed on non-renderthreads
	util::list<SBufferPoolItem> m_cow_list;

	// The table handle for this item
	item_handle_t m_handle;

	// If this item has been relocated on update, this is the item
	// handle of the new item (to be swapped)
	item_handle_t m_cow_handle;

	// The size of the item in bytes
	uint32 m_size;

	// The offset in bytes from the start of the buffer
	uint32 m_offset;

	// The bank index this item resides in
	uint32 m_bank;

	// The defrag allocation handle for this item
	IDefragAllocator::Hdl m_defrag_handle;

	// Set to one if the item was already used once
	uint8 m_used : 1;

	// Set to one if the item is backed by the defrag allocator (XXXX)
	uint8 m_defrag : 1;

	// Set to one if the item is backed by the defrag allocator (XXXX)
	uint8 m_cpu_flush : 1;

	// Set to one if the item is backed by the defrag allocator (XXXX)
	uint8 m_gpu_flush : 1;

	SBufferPoolItem(size_t handle)
		: m_buffer()
		, m_pool()
		, m_size()
		, m_offset(~0u)
		, m_bank(~0u)
		, m_base_ptr(NULL)
		, m_defrag_allocator()
		, m_defrag_handle(IDefragAllocator::InvalidHdl)
		, m_handle(handle)
		, m_deferred_list()
		, m_cow_list()
		, m_cow_handle(~0u)
		, m_used()
		, m_defrag()
		, m_cpu_flush()
		, m_gpu_flush()
	{
		//CryLogAlways("ITEM %4d created", handle);
	}

	~SBufferPoolItem()
	{
		//CryLogAlways("ITEM %4d destroyed", m_handle);
#if DEVBUFFERMAN_DEBUG
		m_offset = ~0u;
		m_bank = ~0u;
		m_base_ptr = (uint8*)-1;
		m_defrag_handle = IDefragAllocator::InvalidHdl;
#endif
	}

	void Relocate(SBufferPoolItem& item)
	{
		std::swap(m_buffer, item.m_buffer);
		DEVBUFFERMAN_ASSERT(m_pool == item.m_pool);
		DEVBUFFERMAN_ASSERT(m_size == item.m_size);
		std::swap(m_offset, item.m_offset);
		std::swap(m_bank, item.m_bank);
		std::swap(m_base_ptr, item.m_base_ptr);
		if (m_defrag)
		{
			DEVBUFFERMAN_ASSERT(m_defrag_allocator == item.m_defrag_allocator);
			DEVBUFFERMAN_ASSERT(item.m_defrag_handle != m_defrag_handle);
			m_defrag_allocator->ChangeContext(m_defrag_handle, reinterpret_cast<void*>(static_cast<uintptr_t>(item.m_handle)));
			m_defrag_allocator->ChangeContext(item.m_defrag_handle, reinterpret_cast<void*>(static_cast<uintptr_t>(m_handle)));
		}
		std::swap(m_defrag_allocator, item.m_defrag_allocator);
		std::swap(m_defrag_handle, item.m_defrag_handle);
		m_cpu_flush = item.m_cpu_flush;
		m_gpu_flush = item.m_gpu_flush;
	}
};
typedef CPartitionTable<SBufferPoolItem> CBufferItemTable;

struct SStagingResources
{
	enum { WRITE = 0, READ = 1 };

	D3DBuffer* m_staging_buffers[2];
	size_t     m_staged_open[2];
	size_t     m_staged_base;
	size_t     m_staged_size;
	size_t     m_staged_offset;
	D3DBuffer* m_staged_buffer;

	SStagingResources() { memset(this, 0x0, sizeof(*this)); }
};

//////////////////////////////////////////////////////////////////////////
// Does nothing - but still useful
//
template<size_t BIND_FLAGS>
class CDefaultUpdater
{
protected:

public:
	CDefaultUpdater() {}
	CDefaultUpdater(SStagingResources&) {}

	bool  CreateResources()                                         { return true;  }
	bool  FreeResources()                                           { return true;  }

	void* BeginRead(D3DBuffer* buffer, size_t size, size_t offset)  { return NULL; }
	void* BeginWrite(D3DBuffer* buffer, size_t size, size_t offset) { return NULL; }
	void  EndReadWrite()                                            {}

	void  RegisterBank(SBufferPoolBank* bank)                       {}
	void  UnregisterBank(SBufferPoolBank* bank)                     {}

	void  Move(
	  D3DBuffer* dst_buffer
	  , size_t dst_size
	  , size_t dst_offset
	  , D3DBuffer* src_buffer
	  , size_t src_size
	  , size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if CRY_PLATFORM_ORBIS
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents);
#elif defined(DEVICE_SUPPORTS_D3D11_1)
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents
		  , 0);
#endif
	}
};

//////////////////////////////////////////////////////////////////////////
// Performs buffer updates over staging buffers
//
template<size_t BIND_FLAGS>
class CDynamicStagingBufferUpdater : public CDefaultUpdater<BIND_FLAGS>
{
	SStagingResources& m_resources;

public:

	CDynamicStagingBufferUpdater(SStagingResources& resources)
		: m_resources(resources)
	{}
	~CDynamicStagingBufferUpdater() {}

	// Create the staging buffers if supported && enabled
	bool CreateResources()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		if (!m_resources.m_staging_buffers[SStagingResources::WRITE] && gRenDev->m_DevMan.CreateBuffer(
		      s_PoolConfig.m_pool_bank_size
		      , 1
		      , CDeviceManager::USAGE_CPU_WRITE | CDeviceManager::USAGE_DYNAMIC
		      , BIND_FLAGS
		      , &m_resources.m_staging_buffers[SStagingResources::WRITE]) != S_OK)
		{
			CryLogAlways("SStaticBufferPool::CreateResources: could not create staging buffer");
			goto error;
		}
		if (!m_resources.m_staging_buffers[SStagingResources::READ] && gRenDev->m_DevMan.CreateBuffer(
		      s_PoolConfig.m_pool_bank_size
		      , 1
		      , CDeviceManager::USAGE_CPU_READ | CDeviceManager::USAGE_STAGING
		      , BIND_FLAGS
		      , &m_resources.m_staging_buffers[SStagingResources::READ]) != S_OK)
		{
			CryLogAlways("SStaticBufferPool::CreateResources: could not create staging buffer");
			goto error;
		}
		if (false)
		{
error:
			FreeResources();
			return false;
		}
		return true;
	}

	bool FreeResources()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		for (size_t i = 0; i < 2; ++i)
		{
			UnsetStreamSources(m_resources.m_staging_buffers[i]);
			SAFE_RELEASE(m_resources.m_staging_buffers[i]);
			m_resources.m_staged_open[i] = 0;
		}
		m_resources.m_staged_base = 0;
		m_resources.m_staged_size = 0;
		m_resources.m_staged_offset = 0;
		m_resources.m_staged_buffer = NULL;
		return true;
	}

	void* BeginRead(D3DBuffer* buffer, size_t size, size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_VERIFY(m_resources.m_staged_open[SStagingResources::READ] == 0);

		D3D11_BOX contents;
		contents.left = offset;
		contents.right = offset + size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , 0
		  , 0
		  , 0
		  , buffer
		  , 0
		  , &contents);

		D3D11_MAPPED_SUBRESOURCE mapped_resource;
		D3D11_MAP map = D3D11_MAP_READ;
		HRESULT hr = gcpRendD3D->GetDeviceContext().Map(
		  m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , map
		  , 0
		  , &mapped_resource);
		if (!CHECK_HRESULT(hr))
		{
			CryLogAlways("map of staging buffer for READING failed!");
			return NULL;
		}
		m_resources.m_staged_open[SStagingResources::READ] = 1;
		return mapped_resource.pData;
	}

	void* BeginWrite(D3DBuffer* buffer, size_t size, size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_ASSERT(!m_resources.m_staged_buffer);

		D3D11_MAPPED_SUBRESOURCE mapped_resource;
		D3D11_MAP map = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
		if (m_resources.m_staged_base + size > s_PoolConfig.m_pool_bank_size)
		{
			map = BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS);
			m_resources.m_staged_base = 0;
		}
		HRESULT hr = gcpRendD3D->GetDeviceContext().Map(
		  m_resources.m_staging_buffers[SStagingResources::WRITE]
		  , 0
		  , map
		  , 0
		  , &mapped_resource);
		if (!CHECK_HRESULT(hr))
		{
			CryLogAlways("map of staging buffer for WRITING failed!");
			return NULL;
		}
		void* result = reinterpret_cast<uint8*>(mapped_resource.pData) + m_resources.m_staged_base;
		m_resources.m_staged_size = size;
		m_resources.m_staged_offset = offset;
		m_resources.m_staged_buffer = buffer;
		m_resources.m_staged_open[SStagingResources::WRITE] = 1;
		return result;
	}

	void EndReadWrite()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		D3D11_BOX contents;
		if (m_resources.m_staged_open[SStagingResources::READ])
		{
			gcpRendD3D->GetDeviceContext().Unmap(m_resources.m_staging_buffers[SStagingResources::READ], 0);
			m_resources.m_staged_open[SStagingResources::READ] = 0;
		}
		if (m_resources.m_staged_open[SStagingResources::WRITE])
		{
			DEVBUFFERMAN_ASSERT(m_resources.m_staged_buffer);
			gcpRendD3D->GetDeviceContext().Unmap(m_resources.m_staging_buffers[SStagingResources::WRITE], 0);
			contents.left = m_resources.m_staged_base;
			contents.right = m_resources.m_staged_base + m_resources.m_staged_size;
			contents.top = 0;
			contents.bottom = 1;
			contents.front = 0;
			contents.back = 1;
			gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
			  m_resources.m_staged_buffer
			  , 0
			  , m_resources.m_staged_offset
			  , 0
			  , 0
			  , m_resources.m_staging_buffers[SStagingResources::WRITE]
			  , 0
			  , &contents);
			m_resources.m_staged_base += m_resources.m_staged_size;
			m_resources.m_staged_offset = 0;
			m_resources.m_staged_size = 0;
			m_resources.m_staged_buffer = NULL;
			m_resources.m_staged_open[SStagingResources::WRITE] = 0;
		}
	}

	void Move(
	  D3DBuffer* dst_buffer
	  , size_t dst_size
	  , size_t dst_offset
	  , D3DBuffer* src_buffer
	  , size_t src_size
	  , size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if defined(DEVICE_SUPPORTS_D3D11_1)
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents
		  , 0);
#else
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , 0
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents);
#endif
		contents.left = 0;
		contents.right = src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , &contents);
	}
};

//////////////////////////////////////////////////////////////////////////
// Performs buffer updates over staging buffers
//
template<size_t BIND_FLAGS>
class CStagingBufferUpdater : public CDefaultUpdater<BIND_FLAGS>
{
	SStagingResources& m_resources;

public:

	CStagingBufferUpdater(SStagingResources& resources)
		: m_resources(resources)
	{}
	~CStagingBufferUpdater() {}

	// Create the staging buffers if supported && enabled
	bool CreateResources()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		if (!m_resources.m_staging_buffers[SStagingResources::WRITE] && gRenDev->m_DevMan.CreateBuffer(
		      s_PoolConfig.m_pool_bank_size
		      , 1
#if CRY_USE_DX12
		      , CDeviceManager::USAGE_CPU_WRITE | CDeviceManager::USAGE_DYNAMIC
#else
		      , CDeviceManager::USAGE_CPU_WRITE | CDeviceManager::USAGE_STAGING // NOTE: this is for GPU-to-CPU downloads, but DYNAMIC doesn't allow MAP_WRITE under DX11
#endif
		      , BIND_FLAGS
		      , &m_resources.m_staging_buffers[SStagingResources::WRITE]) != S_OK)
		{
			CryLogAlways("SStaticBufferPool::CreateResources: could not create staging buffer");
			goto error;
		}
		/*
		   if (!m_resources.m_staging_buffers[SStagingResources::READ] && gRenDev->m_DevMan.CreateBuffer(
		   max(s_PoolConfig.m_pool_bank_size, (size_t)SPoolConfig::POOL_MAX_ALLOCATION_SIZE)
		   , 1
		   , CDeviceManager::USAGE_CPU_READ|CDeviceManager::USAGE_STAGING
		   , BIND_FLAGS
		   , &m_resources.m_staging_buffers[SStagingResources::READ]) != S_OK)
		   {
		   CryLogAlways("SStaticBufferPool::CreateResources: could not create staging buffer");
		   goto error;
		   }
		 */
		if (false)
		{
error:
			FreeResources();
			return false;
		}
		return true;
	}

	bool FreeResources()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		for (size_t i = 0; i < 2; ++i)
		{
			UnsetStreamSources(m_resources.m_staging_buffers[i]);
			SAFE_RELEASE(m_resources.m_staging_buffers[i]);
			m_resources.m_staged_open[i] = 0;
		}
		m_resources.m_staged_base = 0;
		m_resources.m_staged_size = 0;
		m_resources.m_staged_offset = 0;
		m_resources.m_staged_buffer = 0;
		m_resources.m_staged_open[SStagingResources::WRITE] = 1;
		return true;
	}

	void* BeginRead(D3DBuffer* buffer, size_t size, size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size && offset);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_VERIFY(m_resources.m_staged_open[SStagingResources::READ] == 0);

		D3D11_BOX contents;
		contents.left = offset;
		contents.right = offset + size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , 0
		  , 0
		  , 0
		  , buffer
		  , 0
		  , &contents);

		D3D11_MAPPED_SUBRESOURCE mapped_resource;
		D3D11_MAP map = D3D11_MAP_READ;
		HRESULT hr = gcpRendD3D->GetDeviceContext().Map(
		  m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , map
		  , 0
		  , &mapped_resource);
		if (!CHECK_HRESULT(hr))
		{
			CryLogAlways("map of staging buffer for READING failed!");
			return NULL;
		}
		m_resources.m_staged_open[SStagingResources::READ] = 1;
		return mapped_resource.pData;
	}

	void* BeginWrite(D3DBuffer* buffer, size_t size, size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);

		D3D11_MAPPED_SUBRESOURCE mapped_resource;
		D3D11_MAP map = D3D11_MAP_WRITE;
		HRESULT hr = gcpRendD3D->GetDeviceContext().Map(
		  m_resources.m_staging_buffers[SStagingResources::WRITE]
		  , 0
		  , map
		  , 0
		  , &mapped_resource);
		if (!CHECK_HRESULT(hr))
		{
			CryLogAlways("map of staging buffer for WRITING failed!");
			return NULL;
		}
		void* result = reinterpret_cast<uint8*>(mapped_resource.pData);
		m_resources.m_staged_size = size;
		m_resources.m_staged_offset = offset;
		m_resources.m_staged_buffer = buffer;
		m_resources.m_staged_open[SStagingResources::WRITE] = 1;
		return result;
	}

	void EndReadWrite()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		D3D11_BOX contents;
		if (m_resources.m_staged_open[SStagingResources::READ])
		{
			gcpRendD3D->GetDeviceContext().Unmap(m_resources.m_staging_buffers[SStagingResources::READ], 0);
			m_resources.m_staged_open[SStagingResources::READ] = 0;
		}
		if (m_resources.m_staged_open[SStagingResources::WRITE])
		{
			DEVBUFFERMAN_ASSERT(m_resources.m_staged_buffer);
			gcpRendD3D->GetDeviceContext().Unmap(m_resources.m_staging_buffers[SStagingResources::WRITE], 0);
			contents.left = 0;
			contents.right = m_resources.m_staged_size;
			contents.top = 0;
			contents.bottom = 1;
			contents.front = 0;
			contents.back = 1;
			gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
			  m_resources.m_staged_buffer
			  , 0
			  , m_resources.m_staged_offset
			  , 0
			  , 0
			  , m_resources.m_staging_buffers[SStagingResources::WRITE]
			  , 0
			  , &contents);
			m_resources.m_staged_size = 0;
			m_resources.m_staged_offset = 0;
			m_resources.m_staged_buffer = 0;
			m_resources.m_staged_open[SStagingResources::WRITE] = 0;
		}
	}

	void Move(
	  D3DBuffer* dst_buffer
	  , size_t dst_size
	  , size_t dst_offset
	  , D3DBuffer* src_buffer
	  , size_t src_size
	  , size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if defined(DEVICE_SUPPORTS_D3D11_1)
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents
		  , 0);
#else
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , 0
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents);
#endif
		contents.left = 0;
		contents.right = src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , &contents);
	}
};

//////////////////////////////////////////////////////////////////////////
// Performs buffer updates over dynamic updates
//
template<size_t BIND_FLAGS>
class CDynamicBufferUpdater : public CDefaultUpdater<BIND_FLAGS>
{
	// DX11 has the (imho) arbitrary limitation that it
	// will refuse to perform copies from the same subresource, in this
	// case we have a local temp. buffer that acts as a tmp resource
	// for copying.
	SStagingResources& m_resources;

	D3DBuffer*         m_locked_buffer;

public:
	CDynamicBufferUpdater(SStagingResources& resources)
		: m_resources(resources)
		, m_locked_buffer()
	{}
	~CDynamicBufferUpdater() {}

	bool CreateResources()
	{
		if (!m_resources.m_staging_buffers[SStagingResources::READ] &&
		    gRenDev->m_DevMan.CreateBuffer(
		      s_PoolConfig.m_pool_bank_size
		      , 1
		      , CDeviceManager::USAGE_DEFAULT
		      , BIND_FLAGS
		      , &m_resources.m_staging_buffers[SStagingResources::READ]) != S_OK)
		{
			CryLogAlways("SStaticBufferPool::CreateResources: could not create temporary buffer");
			goto error;
		}
		if (false)
		{
error:
			FreeResources();
			return false;
		}
		return true;
	}

	bool FreeResources()
	{
		UnsetStreamSources(m_resources.m_staging_buffers[SStagingResources::READ]);
		SAFE_RELEASE(m_resources.m_staging_buffers[SStagingResources::READ]);
		return true;
	}

	void* BeginWrite(D3DBuffer* buffer, size_t size, size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		D3D11_MAPPED_SUBRESOURCE mapped_resource;
		D3D11_MAP map = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
#if defined(OPENGL) && !DXGL_FULL_EMULATION
		HRESULT hr = DXGLMapBufferRange(
		  gcpRendD3D->GetDeviceContext().GetRealDeviceContext()
		  , m_locked_buffer = buffer
		  , offset
		  , size
		  , map
		  , 0
		  , &mapped_resource);
#else
		HRESULT hr = gcpRendD3D->GetDeviceContext().Map(
		  m_locked_buffer = buffer
		  , 0
		  , map
		  , 0
		  , &mapped_resource);
#endif
		if (!CHECK_HRESULT(hr))
		{
			CryLogAlways("map of staging buffer for WRITING failed!");
			return NULL;
		}
#if defined(OPENGL) && !DXGL_FULL_EMULATION
		return reinterpret_cast<uint8*>(mapped_resource.pData);
#else
		return reinterpret_cast<uint8*>(mapped_resource.pData) + offset;
#endif
	}

	void EndReadWrite()
	{
		DEVBUFFERMAN_ASSERT(m_locked_buffer);
		gcpRendD3D->GetDeviceContext().Unmap(m_locked_buffer, 0);
		m_locked_buffer = NULL;
	}

	void Move(
	  D3DBuffer* dst_buffer
	  , size_t dst_size
	  , size_t dst_offset
	  , D3DBuffer* src_buffer
	  , size_t src_size
	  , size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if defined(DEVICE_SUPPORTS_D3D11_1)
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents
		  , 0);
#else
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , 0
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents);
#endif
		contents.left = 0;
		contents.right = src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , m_resources.m_staging_buffers[SStagingResources::READ]
		  , 0
		  , &contents);
	}
};

template<size_t BIND_FLAGS>
class CDirectBufferUpdater : public CDefaultUpdater<BIND_FLAGS>
{
	D3DBuffer* m_locked_buffer;

public:

	CDirectBufferUpdater(SStagingResources& resources)
		: m_locked_buffer()
	{}

	void* BeginRead(D3DBuffer* buffer, size_t size, size_t offset)
	{
		return NULL;
	}

	void* BeginWrite(D3DBuffer* buffer, size_t size, size_t offset)
	{
		return NULL;
	}
	void EndReadWrite()
	{
	}

	void Move(
	  D3DBuffer* dst_buffer
	  , size_t dst_size
	  , size_t dst_offset
	  , D3DBuffer* src_buffer
	  , size_t src_size
	  , size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if defined(DEVICE_SUPPORTS_D3D11_1)
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents
		  , 0);
#else
		D3D11_BOX contents;
		contents.left = src_offset;
		contents.right = src_offset + src_size;
		contents.top = 0;
		contents.bottom = 1;
		contents.front = 0;
		contents.back = 1;
		gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
		  dst_buffer
		  , 0
		  , dst_offset
		  , 0
		  , 0
		  , src_buffer
		  , 0
		  , &contents);
#endif
	}
};

//////////////////////////////////////////////////////////////////////////
// Wraps the defragging allocator for pool items
//
struct CDynamicDefragAllocator
{
	// Instance of the defragging allocator
	IDefragAllocator* m_defrag_allocator;

	// Instance of the defragging allocator policy (if not set, do not perform defragging)
	IDefragAllocatorPolicy* m_defrag_policy;

	// Manages the item storage
	CBufferItemTable& m_item_table;

	CDynamicDefragAllocator(CBufferItemTable& table)
		: m_defrag_allocator()
		, m_defrag_policy()
		, m_item_table(table)
	{}

	~CDynamicDefragAllocator() { DEVBUFFERMAN_VERIFY(m_defrag_allocator == NULL); }

	bool Initialize(IDefragAllocatorPolicy* policy, bool bestFit)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		if (m_defrag_allocator = CryGetIMemoryManager()->CreateDefragAllocator())
		{
			IDefragAllocator::Policy pol;
			pol.pDefragPolicy = m_defrag_policy = policy;
			pol.maxAllocs = ((policy) ? s_PoolConfig.m_pool_max_allocs : 1024);
			pol.maxSegments = 256;
			pol.blockSearchKind = bestFit ? IDefragAllocator::eBSK_BestFit : IDefragAllocator::eBSK_FirstFit;
			m_defrag_allocator->Init(0, SPoolConfig::POOL_ALIGNMENT, pol);
		}
		return m_defrag_allocator != NULL;
	}

	bool Shutdown()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		SAFE_RELEASE(m_defrag_allocator);
		return m_defrag_allocator == NULL;
	}

	void GetStats(IDefragAllocatorStats& stats)
	{
		if (m_defrag_allocator)
			stats = m_defrag_allocator->GetStats();
	}

	item_handle_t Allocate(size_t size, SBufferPoolItem*& item)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_VERIFY(size);
		IDefragAllocator::Hdl hdl = m_defrag_allocator->Allocate(size, NULL);
		if (hdl == IDefragAllocator::InvalidHdl)
			return ~0u;
		item_handle_t item_hdl = m_item_table.Allocate();
		item = &m_item_table[item_hdl];
		item->m_size = size;
		item->m_offset = (uint32)m_defrag_allocator->WeakPin(hdl);
		item->m_defrag_allocator = m_defrag_allocator;
		item->m_defrag_handle = hdl;
		item->m_defrag = true;
		m_defrag_allocator->ChangeContext(hdl, reinterpret_cast<void*>(static_cast<uintptr_t>(item_hdl)));
		return item_hdl;
	}

	void Free(SBufferPoolItem* item)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
		MEMORY_SCOPE_CHECK_HEAP();
		IF (item->m_defrag_handle != IDefragAllocator::InvalidHdl, 1)
			m_defrag_allocator->Free(item->m_defrag_handle);
		m_item_table.Free(item->m_handle);
	}

	bool Extend(SBufferPoolBank* bank) { return m_defrag_allocator->AppendSegment(bank->m_capacity); }

	void Update(uint32 inflight, uint32 frame_id, bool allow_defragmentation)
	{
		IF (m_defrag_policy && allow_defragmentation, 1)
			m_defrag_allocator->DefragmentTick(s_PoolConfig.m_pool_max_moves_per_update - inflight, s_PoolConfig.m_pool_bank_size);
	}

	void PinItem(SBufferPoolItem* item)
	{
		DEVBUFFERMAN_VERIFY((m_defrag_allocator->Pin(item->m_defrag_handle) & s_PoolConfig.m_pool_bank_mask) == item->m_offset);
	}
	void UnpinItem(SBufferPoolItem* item)
	{
		m_defrag_allocator->Unpin(item->m_defrag_handle);
	}
};

//////////////////////////////////////////////////////////////////////////
// Partition based allocator for constant buffers of roughly the same size
struct CPartitionAllocator
{
	D3DBuffer*          m_buffer;
	void*               m_base_ptr;
	uint32              m_page_size;
	uint32              m_bucket_size;
	uint32              m_partition;
	uint32              m_capacity;

	std::vector<uint32> m_table;
	std::vector<uint32> m_remap;

	CPartitionAllocator(
	  D3DBuffer* buffer, void* base_ptr, size_t page_size, size_t bucket_size)
		: m_buffer(buffer)
		, m_base_ptr(base_ptr)
		, m_page_size((uint32)page_size)
		, m_bucket_size((uint32)bucket_size)
		, m_partition(0)
		, m_capacity(page_size / bucket_size)
		, m_table()
		, m_remap()
	{
		m_table.resize(page_size / bucket_size);
		m_remap.resize(page_size / bucket_size);
		std::iota(m_table.begin(), m_table.end(), 0);
	}
	~CPartitionAllocator()
	{
		DEVBUFFERMAN_VERIFY(m_partition == 0);
		UnsetStreamSources(m_buffer);
		SAFE_RELEASE(m_buffer);
	}

	D3DBuffer* buffer() const   { return m_buffer; }
	void*      base_ptr() const { return m_base_ptr; }
	bool       empty() const    { return m_partition == 0; }

	uint32     allocate()
	{
		size_t key = ~0u;
		IF (m_partition + 1 >= m_capacity, 0)
			return ~0u;
		uint32 storage_index = m_table[key = m_partition++];
		m_remap[storage_index] = key;
		return storage_index;
	}

	void deallocate(size_t key)
	{
		DEVBUFFERMAN_ASSERT(m_partition && key < m_remap.size());
		uint32 roster_index = m_remap[key];
		std::swap(m_table[roster_index], m_table[--m_partition]);
		std::swap(m_remap[key], m_remap[m_table[roster_index]]);
	}
};

//////////////////////////////////////////////////////////////////////////
// Special Allocator for constant buffers
//
#if  CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
struct CConstantBufferAllocator
{
	// The page buckets
	typedef std::vector<CPartitionAllocator*> PageBucketsT;
	PageBucketsT m_page_buckets[18];

	// The retired allocations
	typedef std::pair<CPartitionAllocator*, uint16> RetiredSlot;
	std::vector<RetiredSlot> m_retired_slots[SPoolConfig::POOL_FRAME_QUERY_COUNT];

	// Device fences issues at the end of a frame
	DeviceFenceHandle m_fences[SPoolConfig::POOL_FRAME_QUERY_COUNT];

	// Current frameid
	uint32 m_frameid;

	// The number of allocate pages
	uint32 m_pages;

	CConstantBufferAllocator()
		: m_frameid()
		, m_pages()
	{
		memset(m_fences, 0, sizeof(m_fences));
	}
	~CConstantBufferAllocator() {}

	void ReleaseEmptyBanks()
	{
		if (m_pages * s_PoolConfig.m_cb_bank_size <= s_PoolConfig.m_cb_threshold)
			return;
		FUNCTION_PROFILER_RENDERER;
		for (size_t i = 0; i < 16; ++i)
		{
			for (PageBucketsT::iterator j = m_page_buckets[i].begin(),
			     end = m_page_buckets[i].end(); j != end; )
			{
				if ((*j)->empty())
				{
					delete *j;
					--m_pages;
					j = m_page_buckets[i].erase(j);
					end = m_page_buckets[i].end();
				}
				else
				{
					++j;
				}
			}
		}
	}

	bool Initialize() { return true; }

	bool Shutdown()
	{
		for (size_t i = 0; i < 16; ++i)
		{
			for (size_t j = 0, end = m_page_buckets[i].size(); j < end; ++j)
				delete m_page_buckets[i][j];
			m_page_buckets[i].clear();
		}
		return true;
	}

	void FlushRetiredSlots()
	{
		for (size_t idx = 0; idx < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++idx)
		{
			if (m_fences[idx] && gRenDev->m_DevMan.SyncFence(m_fences[idx], false, false) == S_OK)
			{
				for (auto& slot : m_retired_slots[idx])
					slot.first->deallocate(slot.second);

				m_retired_slots[idx].clear();
			}
		}
	}

	bool Allocate(CConstantBuffer* cbuffer)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
		const unsigned size = cbuffer->m_size;
		const unsigned nsize = NextPower2(size);
		const unsigned bucket = IntegerLog2(nsize) - 8;
		bool failed = false;
retry:
		for (size_t i = m_page_buckets[bucket].size(); i > 0; --i)
		{
			unsigned key = m_page_buckets[bucket][i - 1]->allocate();
			if (key != ~0u)
			{
				cbuffer->m_buffer = m_page_buckets[bucket][i - 1]->buffer();
				cbuffer->m_base_ptr = m_page_buckets[bucket][i - 1]->base_ptr();
				cbuffer->m_offset = key * nsize;
				cbuffer->m_allocator = reinterpret_cast<void*>(m_page_buckets[bucket][i - 1]);
				return true;
			}
		}
		if (!failed)
		{
			uint8* base_ptr;
			++m_pages;
			D3DBuffer* buffer = NULL;
			if (gRenDev->m_DevMan.CreateBuffer(
			      s_PoolConfig.m_cb_bank_size
			      , 1
			      , CDeviceManager::USAGE_DIRECT_ACCESS
			      | CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT
			      | CDeviceManager::USAGE_DIRECT_ACCESS_GPU_COHERENT
	#if defined(CRY_USE_DX12) && (BUFFER_ENABLE_DIRECT_ACCESS == 1)
			      // under dx12 there is direct access, but through the dynamic-usage flag
			      | CDeviceManager::USAGE_DYNAMIC
			      | CDeviceManager::USAGE_CPU_WRITE
	#endif
			      , CDeviceManager::BIND_CONSTANT_BUFFER
			      , &buffer) != S_OK)
			{
				CryLogAlways("failed to create constant buffer pool");
				return false;
			}
			CDeviceManager::ExtractBasePointer<BBT_CONSTANT_BUFFER>(buffer, base_ptr);
			m_page_buckets[bucket].push_back(
			  new CPartitionAllocator(buffer, base_ptr, s_PoolConfig.m_cb_bank_size, nsize));
			failed = true;
			goto retry;
		}
		return false;
	}

	void Free(CConstantBuffer* cbuffer)
	{
		const unsigned size = cbuffer->m_size;
		const unsigned nsize = NextPower2(size);
		const unsigned bucket = IntegerLog2(nsize) - 8;
		CPartitionAllocator* allocator =
		  reinterpret_cast<CPartitionAllocator*>(cbuffer->m_allocator);
		m_retired_slots[m_frameid].push_back(
		  std::make_pair(allocator, (uint16)(cbuffer->m_offset >> (bucket + 8))));
	}

	void Update(uint32 frame_id, DeviceFenceHandle fence, bool allow_defragmentation)
	{
		FlushRetiredSlots();
		m_frameid = frame_id & SPoolConfig::POOL_FRAME_QUERY_MASK;
		m_fences[m_frameid] = fence;
	}
};
#endif

//////////////////////////////////////////////////////////////////////////
// Base class
struct SBufferPool
{
protected:
	// The item table to create items
	CBufferItemTable     m_item_table;
	// The item table to create items
	CBufferPoolBankTable m_bank_table;
public:
	// This lock must be held when operating on the buffers
	SRecursiveSpinLock m_lock;

	SBufferPool()
		: m_item_table()
		, m_bank_table()
	{}
	virtual ~SBufferPool() {}

	virtual item_handle_t Allocate(size_t)                                                            { return ~0u; }
	virtual void          Free(SBufferPoolItem* item)                                                 {}
	virtual bool          CreateResources(bool, bool)                                                 { return false; }
	virtual bool          FreeResources()                                                             { return false; }
	virtual bool          GetStats(SDeviceBufferPoolStats&)                                           { return false; }
	virtual bool          DebugRender()                                                               { return false; }
	virtual void          Sync()                                                                      {}
	virtual void          Update(uint32 frameId, DeviceFenceHandle fence, bool allow_defragmentation) {}
	virtual void          ReleaseEmptyBanks()                                                         {}
	virtual void*         BeginRead(SBufferPoolItem* item)                                            { return NULL; }
	virtual void*         BeginWrite(SBufferPoolItem* item)                                           { return NULL; }
	virtual void          EndReadWrite(SBufferPoolItem* item, bool requires_flush)                    {}
	virtual void          Write(SBufferPoolItem* item, const void* src, size_t size)                  { __debugbreak(); }
	SBufferPoolItem*      Resolve(item_handle_t handle)                                               { return &m_item_table[handle]; }
};

//////////////////////////////////////////////////////////////////////////
// Buffer pool base implementation
template<
  size_t BIND_FLAGS,
  size_t USAGE_FLAGS,
  typename Allocator,
  template<size_t> class Updater,
  size_t ALIGNMENT = SPoolConfig::POOL_ALIGNMENT>
struct CBufferPoolImpl
	: public SBufferPool
	  , private IDefragAllocatorPolicy
{
	typedef Allocator           allocator_t;
	typedef Updater<BIND_FLAGS> updater_t;

	// The item allocator backing this storage
	allocator_t m_allocator;

	// The update strategy implementation
	updater_t m_updater;

	// The list of banks this pool uses
	std::vector<size_t, stl::STLGlobalAllocator<size_t>> m_banks;

	// Deferred items for unpinning && deletion
	struct SDeferredItems
	{
		DeviceFenceHandle           m_fence;
		util::list<SBufferPoolItem> m_deleted_items;
		SDeferredItems()
			: m_fence()
			, m_deleted_items()
		{}
		~SDeferredItems()
		{
			DEVBUFFERMAN_ASSERT(m_deleted_items.empty());
		}
	};
	SDeferredItems m_deferred_items[SPoolConfig::POOL_FRAME_QUERY_COUNT];

	// The relocation list of all items that need to be relocated at the
	// beginning of the next frame
	util::list<SBufferPoolItem> m_cow_relocation_list;

	// The current frame id
	uint32 m_current_frame;

	// The current fence of the device
	DeviceFenceHandle m_current_fence;

	// The current fence of the device
	DeviceFenceHandle m_lockstep_fence;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Syncs to gpu should (debugging only)
	void SyncToGPU(bool block)
	{
#if !defined(_RELEASE)
		if (m_lockstep_fence && block)
		{
			gRenDev->m_DevMan.IssueFence(m_lockstep_fence);
			gRenDev->m_DevMan.SyncFence(m_lockstep_fence, true);
		}
#endif
	}

	// The list of moves we need to perform
	struct SPendingMove
	{
		IDefragAllocatorCopyNotification* m_notification;
		item_handle_t                     m_item_handle;
		UINT_PTR                          m_src_offset;
		UINT_PTR                          m_dst_offset;
		UINT_PTR                          m_size;
		DeviceFenceHandle                 m_copy_fence;
		DeviceFenceHandle                 m_relocate_fence;
		bool                              m_moving     : 1;
		bool                              m_relocating : 1;
		bool                              m_relocated  : 1;
		bool                              m_canceled   : 1;

		SPendingMove()
			: m_notification()
			, m_item_handle(~0u)
			, m_src_offset(-1)
			, m_dst_offset(-1)
			, m_size()
			, m_copy_fence()
			, m_relocate_fence()
			, m_moving()
			, m_relocating()
			, m_relocated()
			, m_canceled()
		{}
		~SPendingMove()
		{
			if (m_copy_fence) gRenDev->m_DevMan.ReleaseFence(m_copy_fence);
			if (m_relocate_fence) gRenDev->m_DevMan.ReleaseFence(m_relocate_fence);
		}
	};
	std::vector<SPendingMove, stl::STLGlobalAllocator<SPendingMove>> m_pending_moves;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	void ProcessPendingMove(SPendingMove& move, bool block)
	{
		bool done = false;
		// Should have finished by now ... soft-sync to fence, if not done, don't finish
		if (move.m_moving)
		{
			if (gRenDev->m_DevMan.SyncFence(move.m_copy_fence, block, block) == S_OK)
			{
				move.m_notification->bDstIsValid = true;
				move.m_moving = false;
			}
		}
		// Only finish the relocation by informing the defragger if the gpu has caught up to the
		// point where the new destination has been considered valid
		else if (move.m_relocating)
		{
			if (gRenDev->m_DevMan.SyncFence(move.m_relocate_fence, block, block) == S_OK)
			{
				move.m_notification->bSrcIsUnneeded = true;
				move.m_relocating = false;
				done = true;
			}
		}
		else if (move.m_canceled)
		{
			move.m_notification->bSrcIsUnneeded = true;
			done = true;
		}
		if (done)
		{
			UINT_PTR nDecOffs = move.m_canceled && !move.m_relocated
			                    ? move.m_dst_offset
			                    : move.m_src_offset;

			{
				int nSrcBank = nDecOffs / s_PoolConfig.m_pool_bank_size;
				SBufferPoolBank* bank = &m_bank_table[m_banks[nSrcBank]];
				bank->m_free_space += move.m_size;
			}

			move.m_moving = false;
			move.m_relocating = false;
			move.m_relocated = false;
			move.m_canceled = false;
			move.m_notification = NULL;
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Creates a new bank for the buffer
	SBufferPoolBank* CreateBank()
	{
		FUNCTION_PROFILER_RENDERER;
		// Allocate a new bank
		size_t bank_index = ~0u;
		D3DBuffer* buffer;
		SBufferPoolBank* bank = NULL;
		{
			DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
			if (gRenDev->m_DevMan.CreateBuffer(
			      s_PoolConfig.m_pool_bank_size
			      , 1
			      , USAGE_FLAGS | CDeviceManager::USAGE_DIRECT_ACCESS
#if defined(CRY_USE_DX12) && (BUFFER_ENABLE_DIRECT_ACCESS == 1) && !BUFFER_USE_STAGED_UPDATES
			      // under dx12 there is direct access, but through the dynamic-usage flag
			      | CDeviceManager::USAGE_DYNAMIC
			      | CDeviceManager::USAGE_CPU_WRITE
#endif
			      , BIND_FLAGS
			      , &buffer) != S_OK)
			{
				CryLogAlways("SBufferPoolImpl::Allocate: could not allocate additional bank of size %" PRISIZE_T, s_PoolConfig.m_pool_bank_size);
				return NULL;
			}
		}
		bank = &m_bank_table[bank_index = m_bank_table.Allocate()];
		bank->m_buffer = buffer;
		bank->m_capacity = s_PoolConfig.m_pool_bank_size;
		bank->m_free_space = s_PoolConfig.m_pool_bank_size;
#if !BUFFER_USE_STAGED_UPDATES
		CDeviceManager::ExtractBasePointer<BIND_FLAGS>(buffer, bank->m_base_ptr);
#endif
		m_banks.push_back(bank_index);
		return bank;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	void PrintDebugStats()
	{
		SDeviceBufferPoolStats stats;
		stats.bank_size = s_PoolConfig.m_pool_bank_size;
		for (size_t i = 0, end = m_banks.size(); i < end; ++i)
		{
			const SBufferPoolBank& bank = m_bank_table[m_banks[i]];
			stats.num_banks += bank.m_buffer ? 1 : 0;
		}
		m_allocator.GetStats(stats.allocator_stats);
		stats.num_allocs = stats.allocator_stats.nInUseBlocks;

		CryLogAlways("SBufferPoolImpl Stats : %04" PRISIZE_T " num_banks %06" PRISIZE_T " allocations"
		             , stats.num_banks, stats.num_allocs);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Recreates a previously freed bank
	bool RecreateBank(SBufferPoolBank* bank)
	{
		FUNCTION_PROFILER_RENDERER;
		{
			DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
			if (gRenDev->m_DevMan.CreateBuffer(
			      s_PoolConfig.m_pool_bank_size
			      , 1
			      , USAGE_FLAGS | CDeviceManager::USAGE_DIRECT_ACCESS
#if defined(CRY_USE_DX12) && (BUFFER_ENABLE_DIRECT_ACCESS == 1) && !BUFFER_USE_STAGED_UPDATES
			      // under dx12 there is direct access, but through the dynamic-usage flag
			      | CDeviceManager::USAGE_DYNAMIC
			      | CDeviceManager::USAGE_CPU_WRITE
#endif
			      , BIND_FLAGS
			      , &bank->m_buffer) != S_OK)
			{
				CryLogAlways("SBufferPoolImpl::Allocate: could not re-allocate freed bank of size %" PRISIZE_T, s_PoolConfig.m_pool_bank_size);
				return false;
			}
		}
#if !BUFFER_USE_STAGED_UPDATES
		CDeviceManager::ExtractBasePointer<BIND_FLAGS>(bank->m_buffer, bank->m_base_ptr);
#endif
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	void RetireEmptyBanks()
	{
		for (size_t i = 0, end = m_banks.size(); i < end; ++i)
		{
			SBufferPoolBank& bank = m_bank_table[m_banks[i]];
			IF (bank.m_capacity != bank.m_free_space, 1)
				continue;
			DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
			UnsetStreamSources(bank.m_buffer);
			SAFE_RELEASE(bank.m_buffer);
			bank.m_base_ptr = NULL;
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////
	void RetirePendingFrees(SDeferredItems& deferred)
	{
		for (util::list<SBufferPoolItem>* iter = deferred.m_deleted_items.next;
		     iter != &deferred.m_deleted_items; iter = iter->next)
		{
			SBufferPoolItem* item = iter->item<& SBufferPoolItem::m_deferred_list>();
			SBufferPoolBank& bank = m_bank_table[m_banks[item->m_bank]];
			bank.m_free_space += item->m_size;
			{
				DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
				DB_MEMREPLAY_SCOPE_FREE(bank.m_base_ptr + item->m_offset);
			}
			m_allocator.Free(item);
		}
		deferred.m_deleted_items.erase();
	}

	//////////////////////////////////////////////////////////////////////////////////////
	void PerformPendingCOWRelocations()
	{
		for (util::list<SBufferPoolItem>* iter = m_cow_relocation_list.next;
		     iter != &m_cow_relocation_list; iter = iter->next)
		{
			SBufferPoolItem* item = iter->item<& SBufferPoolItem::m_deferred_list>();
			SBufferPoolItem* new_item = &m_item_table[item->m_cow_handle];

			item->Relocate(*new_item);
			Free(new_item);
			item->m_cow_handle = ~0u;
		}
		m_cow_relocation_list.erase();
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Implementation of IDefragAllocatorPolicy below
	uint32 BeginCopy(
	  void* pContext
	  , UINT_PTR dstOffset
	  , UINT_PTR srcOffset
	  , UINT_PTR size
	  , IDefragAllocatorCopyNotification* pNotification)
	{
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT // Workaround for Win64, using a C-cast here breaks Orbis
	#pragma warning( push )
	#pragma warning( disable : 4244)
#endif
#if CRY_PLATFORM_ORBIS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		item_handle_t handle = reinterpret_cast<TRUNCATE_PTR>(pContext);
#else
		item_handle_t handle = static_cast<item_handle_t>(reinterpret_cast<uintptr_t>(pContext));
#endif
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop )
#endif
		SBufferPoolItem* old_item = &m_item_table[handle];
		SBufferPoolBank* bank = NULL;
		size_t pm = ~0u, bank_index;
		for (size_t i = 0; i < m_pending_moves.size(); ++i)
		{
			if (m_pending_moves[i].m_notification != NULL)
				continue;
			pm = i;
			break;
		}
		if (pm == ~0u)
		{
			return 0;
		}
		old_item = &m_item_table[handle];
		bank_index = (dstOffset / s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_ASSERT(bank_index < m_banks.size());
		bank = &m_bank_table[m_banks[bank_index]];
		// The below should never happen in practice, but who knows for sure, so to be
		// on the safe side we account for the fact that the allocator might want to move
		// an allocation onto an empty bank.
		IF (bank->m_buffer == NULL, 0)
		{
			if (RecreateBank(bank) == false)
			{
				CryLogAlways("SBufferPoolImpl::Allocate: could not re-allocate freed bank of size %" PRISIZE_T, s_PoolConfig.m_pool_bank_size);
				return 0;
			}
		}
		bank->m_free_space -= size;

		SPendingMove& pending = m_pending_moves[pm];
		pending.m_notification = pNotification;
		pending.m_item_handle = handle;
		pending.m_src_offset = srcOffset;
		pending.m_dst_offset = dstOffset;
		pending.m_size = size;

		// Perform the actual move in (hopefully) hardware
		m_updater.Move(
		  bank->m_buffer
		  , size
		  , dstOffset & s_PoolConfig.m_pool_bank_mask
		  , old_item->m_buffer
		  , old_item->m_size
		  , old_item->m_offset);

		// Issue a fence so that the copy can be synced
		gRenDev->m_DevMan.IssueFence(pending.m_copy_fence);
		pending.m_moving = true;
		// The move will be considered "done" (bDstIsValid) on the next Update call
		// thanks to r_flush being one, this is always true!
		return pm + 1;
	}
	void Relocate(uint32 userMoveId, void* pContext, UINT_PTR newOffset, UINT_PTR oldOffset, UINT_PTR size)
	{
		// Swap both items. The previous item will be the new item and will get freed upon
		// the next update loop
		SPendingMove& move = m_pending_moves[userMoveId - 1];
		DEVBUFFERMAN_ASSERT(move.m_relocating == false);
		SBufferPoolItem& item = m_item_table[move.m_item_handle];
		SBufferPoolBank* bank = &m_bank_table[m_banks[item.m_bank]];
		uint8* old_offset = bank->m_base_ptr + item.m_offset;
		item.m_bank = move.m_dst_offset / s_PoolConfig.m_pool_bank_size;
		item.m_offset = move.m_dst_offset & s_PoolConfig.m_pool_bank_mask;
		bank = &m_bank_table[m_banks[item.m_bank]];
		item.m_buffer = bank->m_buffer;
		// Issue a fence so that the previous location will only be able
		// to be shelled after this point in terms of gpu execution
		gRenDev->m_DevMan.IssueFence(move.m_relocate_fence);
		move.m_relocating = true;
		move.m_relocated = true;

		DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
		DB_MEMREPLAY_SCOPE_REALLOC(old_offset, bank->m_base_ptr + item.m_offset, item.m_size, ALIGNMENT);
	}
	void CancelCopy(uint32 userMoveId, void* pContext, bool bSync)
	{
		// Remove the move from the list of pending moves, free the destination item
		// as it's not going to be used anymore
		SPendingMove& move = m_pending_moves[userMoveId - 1];
		move.m_canceled = true;
	}
	void SyncCopy(void* pContext, UINT_PTR dstOffset, UINT_PTR srcOffset, UINT_PTR size) { __debugbreak(); }

public:
	CBufferPoolImpl(SStagingResources& resources)
		: m_allocator(m_item_table)
		, m_updater(resources)
		, m_banks()
		, m_current_frame()
		, m_current_fence()
		, m_lockstep_fence()
		, m_pending_moves()
	{}
	virtual ~CBufferPoolImpl() {}

	//////////////////////////////////////////////////////////////////////////////////////
	bool GetStats(SDeviceBufferPoolStats& stats)
	{
		stats.bank_size = s_PoolConfig.m_pool_bank_size;
		for (size_t i = 0, end = m_banks.size(); i < end; ++i)
		{
			const SBufferPoolBank& bank = m_bank_table[m_banks[i]];
			stats.num_banks += bank.m_buffer ? 1 : 0;
		}
		m_allocator.GetStats(stats.allocator_stats);
		stats.num_allocs = stats.allocator_stats.nInUseBlocks;
		return true;
	}

	// Try to satisfy an allocation of a given size from within the pool
	// allocating a new bank if all previously created banks are full
	item_handle_t Allocate(size_t size)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		D3DBuffer* buffer = NULL;
		SBufferPoolItem* item = NULL;
		SBufferPoolBank* bank = NULL;
		size_t offset = 0u, bank_index = 0u;
		item_handle_t handle;
		bool failed = false;

		// Align the allocation size up to the configured allocation alignment
		size = (max(size, size_t(1u)) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

		// Handle the case where an allocation cannot be satisfied by a pool bank
		// as the size is too large and create a free standing buffer therefore.
		// Note: Care should be taken to reduce the amount of unpooled items!
		IF (size > s_PoolConfig.m_pool_bank_size, 0)
		{
freestanding:
			if (gRenDev->m_DevMan.CreateBuffer(
			      size
			      , 1
			      , USAGE_FLAGS | CDeviceManager::USAGE_DIRECT_ACCESS
#if defined(CRY_USE_DX12) && (BUFFER_ENABLE_DIRECT_ACCESS == 1) && !BUFFER_USE_STAGED_UPDATES
			      // under dx12 there is direct access, but through the dynamic-usage flag
			      | CDeviceManager::USAGE_DYNAMIC
			      | CDeviceManager::USAGE_CPU_WRITE
#endif
			      , BIND_FLAGS
			      , &buffer) != S_OK)
			{
				CryLogAlways("SBufferPoolImpl::Allocate: could not allocate buffer of size %" PRISIZE_T, size);
				gEnv->bIsOutOfVideoMemory = true;
				return ~0u;
			}
			item = &m_item_table[handle = m_item_table.Allocate()];
			item->m_buffer = buffer;
			item->m_pool = this;
			item->m_offset = 0u;
			item->m_bank = ~0u;
			item->m_size = size;
			item->m_defrag_handle = IDefragAllocator::InvalidHdl;
#if !BUFFER_USE_STAGED_UPDATES
			CDeviceManager::ExtractBasePointer<BIND_FLAGS>(buffer, item->m_base_ptr);
#endif
			return handle;
		}

		// Find a bank that can satisfy the allocation. If none could be found,
		// add an additional bank and retry, if allocations still fail, flag error
retry:
		if ((handle = m_allocator.Allocate(size, item)) != ~0u)
		{
			item->m_pool = this;
			item->m_bank = (bank_index = (item->m_offset / s_PoolConfig.m_pool_bank_size));
			item->m_offset &= s_PoolConfig.m_pool_bank_mask;
			DEVBUFFERMAN_ASSERT(bank_index < m_banks.size());
			bank = &m_bank_table[m_banks[bank_index]];
			IF (bank->m_buffer == NULL, 0)
			{
				if (RecreateBank(bank) == false)
				{
					m_allocator.Free(item);
					return ~0u;
				}
			}
			DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
			DB_MEMREPLAY_SCOPE_ALLOC(bank->m_base_ptr + item->m_offset, size, ALIGNMENT);
			item->m_buffer = bank->m_buffer;
			bank->m_free_space -= size;
			return handle;
		}
		if (failed)   // already tried once
		{
			CryLogAlways("SBufferPoolImpl::Allocate: could not allocate pool item of size %" PRISIZE_T, size);
			// Try to allocate a free standing buffer now ... fingers crossed
			goto freestanding;
		}
		if ((bank = CreateBank()) == NULL)
		{
			gEnv->bIsOutOfVideoMemory = true;
			return ~0u;
		}
		if (!m_allocator.Extend(bank))
		{
#ifndef _RELEASE
			CryLogAlways("SBufferPoolImpl::Allocate: WARNING: "
			             "could not extend allocator segment. Performing a free standing allocation!"
			             "(backing allocator might have run out of handles, please check)");
			PrintDebugStats();
#endif

			// Extending the allocator failed, so the newly created bank is rolled back
			UnsetStreamSources(bank->m_buffer);
			SAFE_RELEASE(bank->m_buffer);
			m_bank_table.Free(bank->m_handle);
			m_banks.erase(m_banks.end() - 1);
			// Try to allocate a free standing buffer now ... fingers crossed
			goto freestanding;
		}

		failed = true;   // Prevents an infinite loop
		goto retry;
	}

	// Free a previously made allocation
	void Free(SBufferPoolItem* item)
	{
		DEVBUFFERMAN_ASSERT(item);
		// Handle un pooled buffers
		IF ((item->m_bank) == ~0u, 0)
		{
			UnsetStreamSources(item->m_buffer);
			SAFE_RELEASE(item->m_buffer);
			m_item_table.Free(item->m_handle);
			return;
		}
		item->m_deferred_list.relink_tail(m_deferred_items[m_current_frame].m_deleted_items);
	}

	//////////////////////////////////////////////////////////////////////////////////////
	bool CreateResources(bool enable_defragging, bool best_fit)
	{
		IDefragAllocatorPolicy* defrag_policy = enable_defragging ? this : NULL;
		if (!m_allocator.Initialize(defrag_policy, best_fit))
		{
			CryLogAlways("buffer pool allocator failed to create resources");
			return false;
		}
		if (!m_updater.CreateResources())
		{
			CryLogAlways("Buffer pool updater failed to create resources");
			return false;
		}
		m_pending_moves.resize(s_PoolConfig.m_pool_max_moves_per_update);
		for (size_t i = 0; i < s_PoolConfig.m_pool_max_moves_per_update; ++i)
		{
			if (gRenDev->m_DevMan.CreateFence(m_pending_moves[i].m_copy_fence) != S_OK)
			{
				CryLogAlways("Could not create buffer pool copy gpu fence");
				return false;
			}
			if (gRenDev->m_DevMan.CreateFence(m_pending_moves[i].m_relocate_fence) != S_OK)
			{
				CryLogAlways("Could not create buffer pool relocate fence");
				return false;
			}
		}
		if (gRenDev->m_DevMan.CreateFence(m_lockstep_fence) != S_OK)
		{
			CryLogAlways("Could not create lockstep debugging fence");
			return false;
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	bool FreeResources()
	{
		Sync();
		if (m_updater.FreeResources() == false)
			return false;
		if (m_allocator.Shutdown() == false)
			return false;
		for (size_t i = 0, end = m_banks.size(); i < end; ++i)
			m_bank_table.Free((item_handle_t)m_banks[i]);
		if (m_lockstep_fence && gRenDev->m_DevMan.ReleaseFence(m_lockstep_fence) != S_OK)
			return false;
		stl::free_container(m_banks);
		stl::free_container(m_pending_moves);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	void ReleaseEmptyBanks() { RetireEmptyBanks(); }

	//////////////////////////////////////////////////////////////////////////////////////
	void Sync()
	{
		for (size_t i = 0, end = m_pending_moves.size(); i < end; ++i)
		{
			SPendingMove& move = m_pending_moves[i];
			if (move.m_notification == NULL)
				continue;
			ProcessPendingMove(move, true);
		}

		// Update all deferred items
		for (int32 i = 0; i < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
		{
			RetirePendingFrees(m_deferred_items[i]);
		}

		PerformPendingCOWRelocations();

		// Free any banks that remained free until now
		RetireEmptyBanks();
	}

	//////////////////////////////////////////////////////////////////////////////////////
	void Update(uint32 frame_id, DeviceFenceHandle fence, bool allow_defragmentation)
	{
		// Loop over the pending moves and update their state accordingly
		uint32 inflight = 0;
		for (size_t i = 0, end = m_pending_moves.size(); i < end; ++i)
		{
			SPendingMove& move = m_pending_moves[i];
			if (move.m_notification == NULL)
				continue;
			ProcessPendingMove(move, false);
			++inflight;
		}

		// Update the current deferred items
		m_current_frame = (frame_id + 1) & SPoolConfig::POOL_FRAME_QUERY_MASK;
		for (uint32 i = m_current_frame; i < m_current_frame + SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
		{
			SDeferredItems& deferred = m_deferred_items[i & SPoolConfig::POOL_FRAME_QUERY_MASK];
			if (deferred.m_fence && gRenDev->m_DevMan.SyncFence(deferred.m_fence, false, false) != S_OK)
				continue;
			RetirePendingFrees(deferred);
		}
		m_deferred_items[m_current_frame & SPoolConfig::POOL_FRAME_QUERY_MASK].m_fence = fence;
		m_current_fence = fence;

		PerformPendingCOWRelocations();

		// Let the allocator free the items that were retired
		m_allocator.Update(min(inflight, (uint32)s_PoolConfig.m_pool_max_moves_per_update)
		                   , frame_id, allow_defragmentation);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Buffer IO methods
	void* BeginRead(SBufferPoolItem* item)
	{
		SyncToGPU(CRenderer::CV_r_enable_full_gpu_sync != 0);

		DEVBUFFERMAN_VERIFY(item->m_used);
		IF (item->m_bank != ~0u, 1)
			m_allocator.PinItem(item);
#if !BUFFER_USE_STAGED_UPDATES
		IF (item->m_bank != ~0u, 1)
		{
			SBufferPoolBank& bank = m_bank_table[m_banks[item->m_bank]];
			IF (bank.m_base_ptr != NULL && CRenderer::CV_r_buffer_enable_lockless_updates, 1)
				return bank.m_base_ptr + item->m_offset;
		}
#endif
		return m_updater.BeginRead(item->m_buffer, item->m_size, item->m_offset);
	}

	void* BeginWrite(SBufferPoolItem* item)
	{
		SyncToGPU(CRenderer::CV_r_enable_full_gpu_sync != 0);
		// In case item was previously used and the current last fence can not be
		// synced already we allocate a new item and swap it with the existing one
		// to make sure that we do not contend with the gpu on an already
		// used item's buffer update.
		size_t item_handle = item->m_handle;
		IF (item->m_bank != ~0u, 1)
			m_allocator.PinItem(item);
		IF (item->m_bank != ~0u && item->m_used /*&& gRenDev->m_DevMan.SyncFence(m_current_fence, false, false) != S_OK*/, 0)
		{
			item_handle_t handle = Allocate(item->m_size);
			if (handle == ~0u)
			{
				CryLogAlways("failed to allocate new slot on write");
				return NULL;
			}
			item->m_cow_handle = handle;

			SBufferPoolItem* new_item = &m_item_table[handle];
			// Pin the item so that the defragger does not come up with
			// the idea of moving this item because it will be invalidated
			// soon as we are moving the allocation to a pristine location (not used by the gpu).
			// Relocate the old item to the new pristine allocation
			IF (new_item->m_bank != ~0u, 1)
				m_allocator.PinItem(new_item);

			// Return the memory of the newly allocated item
			item = new_item;
		}
		item->m_used = 1u;
		PREFAST_SUPPRESS_WARNING(6326)
		if ((USAGE_FLAGS& CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT) == 0)
		{
			item->m_cpu_flush = 1;
		}
		if ((USAGE_FLAGS& CDeviceManager::USAGE_DIRECT_ACCESS_GPU_COHERENT) == 0)
		{
			item->m_gpu_flush = 1;
		}
#if !BUFFER_USE_STAGED_UPDATES
		IF (item->m_bank != ~0u, 1)
		{
			SBufferPoolBank& bank = m_bank_table[m_banks[item->m_bank]];
			IF (bank.m_base_ptr != NULL && CRenderer::CV_r_buffer_enable_lockless_updates, 1)
				return bank.m_base_ptr + item->m_offset;
		}
#endif
		return m_updater.BeginWrite(item->m_buffer, item->m_size, item->m_offset);
	}

	void EndReadWrite(SBufferPoolItem* item, bool requires_flush)
	{
		IF (item->m_cow_handle != ~0u, 0)
		{
			SBufferPoolItem* new_item = &m_item_table[item->m_cow_handle];
			IF (gRenDev->m_pRT->IsRenderThread(), 1)
			{
				// As we are now relocating the allocation, we also need
				// to free the previous allocation
				item->Relocate(*new_item);
				Free(new_item);

				item->m_cow_handle = ~0u;
			}
			else
			{
				item->m_cow_list.relink_tail(m_cow_relocation_list);
				item = new_item;
			}
		}
#if !BUFFER_USE_STAGED_UPDATES
		IF (item->m_bank != ~0u, 1)
		{
			m_allocator.UnpinItem(item);
			SBufferPoolBank* bank = &m_bank_table[m_banks[item->m_bank]];
			IF (bank->m_base_ptr != NULL && CRenderer::CV_r_buffer_enable_lockless_updates, 1)
			{
	#if BUFFER_ENABLE_DIRECT_ACCESS
				if (item->m_cpu_flush)
				{
					if (requires_flush)
					{
						STATOSCOPE_TIMER(GetStatoscopeData(0).m_cpu_flush_time);
						CDeviceManager::InvalidateCpuCache(
						  bank->m_base_ptr, item->m_size, item->m_offset);
					}
					item->m_cpu_flush = 0;
				}
				if (item->m_gpu_flush)
				{
					gRenDev->m_DevMan.InvalidateBuffer(
					  bank->m_buffer
					  , bank->m_base_ptr
					  , item->m_offset
					  , item->m_size
					  , _GetThreadID());
					item->m_gpu_flush = 0;
				}
	#endif
				return;
			}
		}
#endif
		m_updater.EndReadWrite();

		SyncToGPU(CRenderer::CV_r_enable_full_gpu_sync != 0);
	}

	void Write(SBufferPoolItem* item, const void* src, size_t size)
	{
		DEVBUFFERMAN_ASSERT(size <= item->m_size);

		if (item->m_size <= s_PoolConfig.m_pool_bank_size)
		{
			void* const dst = BeginWrite(item);
			IF (dst, 1)
			{
				const size_t csize = min((size_t)item->m_size, size);
				const bool requires_flush = CopyData(dst, src, csize);
				EndReadWrite(item, requires_flush);
			}
			return;
		}

		DEVBUFFERMAN_ASSERT(item->m_bank == ~0u);
		DEVBUFFERMAN_ASSERT(item->m_cow_handle == ~0u);

		SyncToGPU(gRenDev->CV_r_enable_full_gpu_sync != 0);

		item->m_used = 1u;

		for (size_t offset = 0; offset < size; )
		{
			const size_t sz = min(size - offset, s_PoolConfig.m_pool_bank_size);
			void* const dst = m_updater.BeginWrite(item->m_buffer, sz, item->m_offset + offset);
			IF (dst, 1)
			{
				const bool requires_flush = CopyData(dst, ((char*)src) + offset, sz);
			}
			m_updater.EndReadWrite();
			offset += sz;
		}

		SyncToGPU(gRenDev->CV_r_enable_full_gpu_sync != 0);
	}
};

//////////////////////////////////////////////////////////////////////////////////////
// SStaticBufferPool A buffer pool for geometry that change infrequently and have a
// significant lifetime
//
// Use this pool for example for :
//    - streamed static geometry
//    - geometry that rarely changes
//
// Corresponding D3D_USAGE : USAGE_DEFAULT
// Corresponding update strategy : d3d11 staging buffers (CopySubResource)
//
typedef CBufferPoolImpl<
    CDeviceManager::BIND_VERTEX_BUFFER
    , CDeviceManager::USAGE_DEFAULT | CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
    , CStagingBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SStaticBufferPoolVB;
typedef CBufferPoolImpl<
    CDeviceManager::BIND_INDEX_BUFFER
    , CDeviceManager::USAGE_DEFAULT | CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
    , CStagingBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SStaticBufferPoolIB;

//////////////////////////////////////////////////////////////////////////////////////
// SDynamicBufferPool A buffer pool for geometry that can change frequently but rarely
// changes topology
//
// Use this pool for example for :
//    - deforming geometry that is updated on the CPU
//    - characters skinned in software
//
// Corresponding D3D_USAGE : USAGE_DYNAMIC
// Corresponding update strategy : NO_OVERWRITE direct map of the buffer
typedef CBufferPoolImpl<
    CDeviceManager::BIND_VERTEX_BUFFER
    , CDeviceManager::USAGE_DYNAMIC
    | CDeviceManager::USAGE_CPU_WRITE
    | CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT
    | CDeviceManager::USAGE_DIRECT_ACCESS_GPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
    , CDynamicBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SDynamicBufferPoolVB;
typedef CBufferPoolImpl<
    CDeviceManager::BIND_INDEX_BUFFER
    , CDeviceManager::USAGE_DYNAMIC
    | CDeviceManager::USAGE_CPU_WRITE
    | CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT
    | CDeviceManager::USAGE_DIRECT_ACCESS_GPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
    , CDynamicBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SDynamicBufferPoolIB;

#if BUFFER_SUPPORT_TRANSIENT_POOLS
template<size_t BIND_FLAGS, size_t ALIGNMENT = SPoolConfig::POOL_ALIGNMENT>
class CTransientBufferPool : public SBufferPool
{
	SBufferPoolBank m_backing_buffer;
	size_t          m_allocation_count;
	D3D11_MAP       m_map_type;

public:
	CTransientBufferPool()
		: m_backing_buffer(~0u)
		, m_allocation_count()
		, m_map_type(BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS))
	{
	}

	item_handle_t Allocate(size_t size)
	{
		// Align the allocation size up to the configured allocation alignment
		size = (max(size, size_t(1u)) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

		DEVBUFFERMAN_ASSERT(size <= m_backing_buffer.m_capacity);

		if (m_backing_buffer.m_free_space + size >= m_backing_buffer.m_capacity)
		{
			m_map_type = BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS);
			m_backing_buffer.m_free_space = 0;
		}

		SBufferPoolItem* item = &m_item_table[m_item_table.Allocate()];
		item->m_buffer = m_backing_buffer.m_buffer;
		item->m_pool = this;
		item->m_offset = m_backing_buffer.m_free_space;
		item->m_bank = ~0u;
		item->m_size = size;
		item->m_defrag_handle = IDefragAllocator::InvalidHdl;
		CDeviceManager::ExtractBasePointer<BIND_FLAGS>(m_backing_buffer.m_buffer, item->m_base_ptr);

		m_backing_buffer.m_free_space += size;
		++m_allocation_count;

		return item->m_handle;
	}
	void Free(SBufferPoolItem* item)
	{
		m_item_table.Free(item->m_handle);
		--m_allocation_count;
	}
	bool CreateResources(bool, bool)
	{
		if (gRenDev->m_DevMan.CreateBuffer(
		      s_PoolConfig.m_transient_pool_size
		      , 1
		      , CDeviceManager::USAGE_CPU_WRITE | CDeviceManager::USAGE_DYNAMIC
		      , BIND_FLAGS
		      , &m_backing_buffer.m_buffer) != S_OK)
		{
			CryLogAlways(
			  "CTransientBufferPool::CreateResources: could not allocate backing buffer of size %" PRISIZE_T
			  , s_PoolConfig.m_transient_pool_size);
			return false;
		}
		m_backing_buffer.m_capacity = s_PoolConfig.m_transient_pool_size;
		m_backing_buffer.m_free_space = 0;
		m_backing_buffer.m_handle = ~0u;
		CDeviceManager::ExtractBasePointer<BIND_FLAGS>(m_backing_buffer.m_buffer, m_backing_buffer.m_base_ptr);
		return true;
	}
	bool FreeResources()
	{
		UnsetStreamSources(m_backing_buffer.m_buffer);
		SAFE_RELEASE(m_backing_buffer.m_buffer);
		m_backing_buffer.m_capacity = 0;
		m_backing_buffer.m_free_space = 0;
		m_backing_buffer.m_handle = ~0u;
		return true;
	}
	bool GetStats(SDeviceBufferPoolStats&) { return false; }
	bool DebugRender()                     { return false; }
	void Sync()                            {}
	void Update(uint32 frameId, DeviceFenceHandle fence, bool allow_defragmentation)
	{
		if (m_allocation_count)
		{
			CryFatalError(
			  "CTransientBufferPool::Update %" PRISIZE_T " allocations still in transient pool!"
			  , m_allocation_count);
		}
		m_map_type = BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS);
		m_backing_buffer.m_free_space = 0;
	}
	void  ReleaseEmptyBanks()              {}
	void* BeginRead(SBufferPoolItem* item) { return NULL; }
	void* BeginWrite(SBufferPoolItem* item)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		D3DBuffer* buffer = m_backing_buffer.m_buffer;
		size_t size = item->m_size;
		D3D11_MAPPED_SUBRESOURCE mapped_resource;
		D3D11_MAP map = m_map_type;
	#if defined(OPENGL) && !DXGL_FULL_EMULATION
		HRESULT hr = DXGLMapBufferRange(
		  gcpRendD3D->GetDeviceContext().GetRealDeviceContext()
		  , buffer
		  , item->m_offset
		  , item->m_size
		  , map
		  , 0
		  , &mapped_resource);
	#else
		HRESULT hr = gcpRendD3D->GetDeviceContext().Map(
		  buffer
		  , 0
		  , map
		  , 0
		  , &mapped_resource);
	#endif
		if (!CHECK_HRESULT(hr))
		{
			CryLogAlways("map of staging buffer for WRITING failed!");
			return NULL;
		}
	#if defined(OPENGL) && !DXGL_FULL_EMULATION
		return reinterpret_cast<uint8*>(mapped_resource.pData);
	#else
		return reinterpret_cast<uint8*>(mapped_resource.pData) + item->m_offset;
	#endif
	}
	void EndReadWrite(SBufferPoolItem* item, bool requires_flush)
	{
		gcpRendD3D->GetDeviceContext().Unmap(m_backing_buffer.m_buffer, 0);
		m_map_type = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
	}
	void Write(SBufferPoolItem* item, const void* src, size_t size)
	{
		DEVBUFFERMAN_ASSERT(size <= item->m_size);
		DEVBUFFERMAN_ASSERT(item->m_size <= m_backing_buffer.m_capacity);
		void* const dst = BeginWrite(item);
		IF (dst, 1)
		{
			const size_t csize = min((size_t)item->m_size, size);
			const bool requires_flush = CopyData(dst, src, csize);
			EndReadWrite(item, requires_flush);
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////
// CTransientBufferPool is a buffer pool for geometry that can change frequently and
// is only valid for a single frame (fire&forgot geometry).
//
// Corresponding D3D_USAGE : USAGE_DYNAMIC
// Corresponding update strategy : DISCARD + NO_OVERWRITE direct map of the buffer
typedef CTransientBufferPool<CDeviceManager::BIND_VERTEX_BUFFER> CTransientBufferPoolVB;
typedef CTransientBufferPool<CDeviceManager::BIND_INDEX_BUFFER>  CTransientBufferPoolIB;
#endif

//////////////////////////////////////////////////////////////////////////
// Freestanding buffer implementation
template<
  size_t BIND_FLAGS,
  size_t USAGE_FLAGS,
  typename Allocator,
  template<size_t> class Updater>
struct CFreeBufferPoolImpl : public SBufferPool
{
	typedef Updater<BIND_FLAGS> updater_t;

	SBufferPoolBank m_backing_buffer;
	size_t          m_allocation_size;
	size_t          m_item_handle;
	updater_t       m_updater;

public:
	CFreeBufferPoolImpl(SStagingResources& resources, size_t size)
		: m_backing_buffer(~0u)
		, m_allocation_size((max(size, size_t(1u)) + (SPoolConfig::POOL_ALIGNMENT - 1)) & ~(SPoolConfig::POOL_ALIGNMENT - 1))
		, m_item_handle(~0u)
		, m_updater(resources)
	{
		if (!CreateResources(true, true))
		{
			CryLogAlways("DEVBUFFER WARNING: could not create free standing buffer");
		}
	}

	virtual ~CFreeBufferPoolImpl()  { FreeResources();  }

	item_handle_t Allocate(size_t size)
	{
		// Align the allocation size up to the configured allocation alignment
		size = (max(size, size_t(1u)) + (SPoolConfig::POOL_ALIGNMENT - 1)) & ~(SPoolConfig::POOL_ALIGNMENT - 1);
		if (m_item_handle != ~0u || size != m_allocation_size)
		{
			CryFatalError("free standing buffer allocated twice?!");
			return ~0u;
		}

		SBufferPoolItem* item = &m_item_table[m_item_table.Allocate()];
		item->m_buffer = m_backing_buffer.m_buffer;
		item->m_pool = this;
		item->m_offset = 0u;
		item->m_bank = ~0u;
		item->m_size = size;
		item->m_defrag_handle = IDefragAllocator::InvalidHdl;
		CDeviceManager::ExtractBasePointer<BIND_FLAGS>(m_backing_buffer.m_buffer, item->m_base_ptr);

		m_backing_buffer.m_free_space += size;
		return (m_item_handle = item->m_handle);
	}

	void Free(SBufferPoolItem* item)
	{
		m_item_table.Free(item->m_handle);

		// We can do this safely here as only the item has a reference to
		// this instance.
		delete this;
	}

	bool CreateResources(bool, bool)
	{
		if (gRenDev->m_DevMan.CreateBuffer(
		      m_allocation_size
		      , 1
		      , USAGE_FLAGS
		      , BIND_FLAGS
		      , &m_backing_buffer.m_buffer) != S_OK)
		{
			CryLogAlways(
			  "FreeStandingBuffer::CreateResources: could not allocate backing buffer of size %" PRISIZE_T
			  , s_PoolConfig.m_transient_pool_size);
			return false;
		}
		m_backing_buffer.m_capacity = m_allocation_size;
		m_backing_buffer.m_free_space = 0;
		m_backing_buffer.m_handle = ~0u;
		CDeviceManager::ExtractBasePointer<BIND_FLAGS>(m_backing_buffer.m_buffer, m_backing_buffer.m_base_ptr);
		return true;
	}
	bool FreeResources()
	{
		UnsetStreamSources(m_backing_buffer.m_buffer);
		SAFE_RELEASE(m_backing_buffer.m_buffer);
		m_backing_buffer.m_capacity = 0;
		m_backing_buffer.m_free_space = 0;
		m_backing_buffer.m_handle = ~0u;
		return true;
	}
	bool  GetStats(SDeviceBufferPoolStats&)                                           { return false; }
	bool  DebugRender()                                                               { return false; }
	void  Sync()                                                                      {}
	void  Update(uint32 frameId, DeviceFenceHandle fence, bool allow_defragmentation) {}
	void  ReleaseEmptyBanks()                                                         {}
	void* BeginRead(SBufferPoolItem* item)                                            { return NULL; }
	void* BeginWrite(SBufferPoolItem* item)
	{
		return m_updater.BeginWrite(item->m_buffer, item->m_size, item->m_offset);
	}
	void EndReadWrite(SBufferPoolItem* item, bool requires_flush)
	{
		m_updater.EndReadWrite();
	}
	static SBufferPool* Create(SStagingResources& resources, size_t size)
	{ return new CFreeBufferPoolImpl(resources, size); }
};
typedef SBufferPool* (* BufferCreateFnc)(SStagingResources&, size_t);

//////////////////////////////////////////////////////////////////////////////////////
// A freestanding buffer for geometry that change infrequently and have a
// significant lifetime
//
// Use this pool for example for :
//    - streamed static geometry
//    - geometry that rarely changes
//
// Corresponding D3D_USAGE : USAGE_DEFAULT
// Corresponding update strategy : d3d11 staging buffers (CopySubResource)
//
typedef CFreeBufferPoolImpl<
    CDeviceManager::BIND_VERTEX_BUFFER
    , CDeviceManager::USAGE_DEFAULT | CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
    , CStagingBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SStaticFreeBufferVB;
typedef CFreeBufferPoolImpl<
    CDeviceManager::BIND_INDEX_BUFFER
    , CDeviceManager::USAGE_DEFAULT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
    , CStagingBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SStaticFreeBufferIB;

//////////////////////////////////////////////////////////////////////////////////////
// A free standing buffer for geometry that can change frequently but rarely
// changes topology
//
// Use this pool for example for :
//    - deforming geometry that is updated on the CPU
//    - characters skinned in software
//
// Corresponding D3D_USAGE : USAGE_DYNAMIC
// Corresponding update strategy : NO_OVERWRITE direct map of the buffer
typedef CFreeBufferPoolImpl<
    CDeviceManager::BIND_VERTEX_BUFFER
    , CDeviceManager::USAGE_DYNAMIC
    | CDeviceManager::USAGE_CPU_WRITE
    | CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT
    | CDeviceManager::USAGE_DIRECT_ACCESS_GPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
    , CDynamicBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SDynamicFreeBufferVB;
typedef CFreeBufferPoolImpl<
    CDeviceManager::BIND_INDEX_BUFFER
    , CDeviceManager::USAGE_DYNAMIC
    | CDeviceManager::USAGE_CPU_WRITE
    | CDeviceManager::USAGE_DIRECT_ACCESS_CPU_COHERENT
    | CDeviceManager::USAGE_DIRECT_ACCESS_GPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
    , CDynamicBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SDynamicFreeBufferIB;

//===============================================================================
#if defined(CRY_USE_DX12)
class CDescriptorPool
{
	struct SDescriptorBlockList
	{
		CPartitionTable<SDescriptorBlock>       items;
		std::vector<NCryDX12::CDescriptorBlock> blocks;

		SDescriptorBlockList() {}
		SDescriptorBlockList(SDescriptorBlockList&& other)
		{
			items = std::move(other.items);
			blocks = std::move(other.blocks);
		}
	};

	struct SRetiredBlock
	{
		uint32        listIndex;
		item_handle_t itemHandle;
	};

	std::unordered_map<uint32_t, SDescriptorBlockList>                          m_DescriptorBlocks;
	std::array<std::vector<SRetiredBlock>, SPoolConfig::POOL_FRAME_QUERY_COUNT> m_RetiredBlocks;
	std::array<DeviceFenceHandle, SPoolConfig::POOL_FRAME_QUERY_COUNT>          m_fences;

	uint32             m_frameID;
	CryCriticalSection m_lock;

public:
	CDescriptorPool()
		: m_frameID(0)
	{
		m_fences.fill(0);
	}

	SDescriptorBlock* Allocate(size_t size)
	{
		AUTO_LOCK(m_lock);

		SDescriptorBlockList& blockList = m_DescriptorBlocks[size];
		item_handle_t itemHandle = blockList.items.Allocate();

		if (blockList.blocks.size() < blockList.items.Capacity())
			blockList.blocks.resize(blockList.items.Capacity());

		NCryDX12::CDescriptorBlock& block = blockList.blocks[itemHandle];
		if (block.GetCapacity() == 0)
		{
			NCryDX12::CDevice* pDevice = reinterpret_cast<CCryDX12Device*>(gcpRendD3D->GetDevice().GetRealDevice())->GetDX12Device();
			block = pDevice->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, size);
		}

		SDescriptorBlock& item = blockList.items[itemHandle];
		item.offset = block.GetStartOffset();
		item.size = size;
		item.pBuffer = reinterpret_cast<ID3D11Buffer*>(block.GetDescriptorHeap());

		return &item;
	}

	void Free(SDescriptorBlock* pItem)
	{
		AUTO_LOCK(m_lock);
		SRetiredBlock retiredBlock = { pItem->size, pItem->blockID };
		m_RetiredBlocks[m_frameID].push_back(retiredBlock);
	}

	void Update(uint32 frameId, DeviceFenceHandle fence)
	{
		for (int i = 0; i < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
		{
			if (m_fences[i] && gRenDev->m_DevMan.SyncFence(m_fences[i], false, false) == S_OK)
			{
				for (auto& block : m_RetiredBlocks[i])
					m_DescriptorBlocks[block.listIndex].items.Free(block.itemHandle);

				m_RetiredBlocks[i].clear();
			}
		}

		m_frameID = frameId & SPoolConfig::POOL_FRAME_QUERY_MASK;
		m_fences[m_frameID] = fence;
	}

	void FreeResources()
	{
		for (auto& retiredBlockList : m_RetiredBlocks)
			retiredBlockList.clear();

		m_DescriptorBlocks.clear();
	}
};
#endif

//////////////////////////////////////////////////////////////////////////////////////
// Manages all pool - in anonymous namespace to reduce recompiles
struct SPoolManager
{
	// Storage for constant buffer wrapper instances
	CPartitionTable<CConstantBuffer> m_constant_buffers[2];

#if defined(CRY_USE_DX12)
	CDescriptorPool m_ResourceDescriptorPool;
#endif

	// The allocator for constant buffers
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	CConstantBufferAllocator m_constant_allocator[2];
#endif

	// The pools segregated by usage and binding
	SBufferPool* m_pools[BBT_MAX][BU_MAX];

	// Freestanding buffer creator functions
	BufferCreateFnc m_buffer_creators[BBT_MAX][BU_MAX];

	// The pools fences
	DeviceFenceHandle m_fences[SPoolConfig::POOL_FRAME_QUERY_COUNT];

	// The resources used for updating buffers
	SStagingResources m_staging_resources[BU_MAX];

	// This lock must be held when operating on the buffers
	SRecursiveSpinLock m_lock;

	// Special debugging staging buffers if debug consistency check is enabled
#if defined(CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK) && BUFFER_USE_STAGED_UPDATES
	CStagingBufferUpdater<CDeviceManager::BIND_VERTEX_BUFFER> m_debug_staging_vb;
	CStagingBufferUpdater<CDeviceManager::BIND_INDEX_BUFFER>  m_debug_staging_ib;
#endif

#if ENABLE_STATOSCOPE
	SStatoscopeData m_sdata[2];
#endif

	bool m_initialized;

	SPoolManager()
		: m_initialized()
#if defined(CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK) && BUFFER_USE_STAGED_UPDATES
		, m_debug_staging_vb(m_staging_resources[BU_STATIC])
		, m_debug_staging_ib(m_staging_resources[BU_STATIC])
#endif
	{
		memset(m_pools, 0x0, sizeof(m_pools));
		memset(m_fences, 0x0, sizeof(m_fences));
		memset(m_buffer_creators, 0x0, sizeof(m_buffer_creators));
#if ENABLE_STATOSCOPE
		ZeroArray(m_sdata);
#endif

	}

	~SPoolManager() {}

	bool CreatePool(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, bool enable_defragging, bool best_fit, SBufferPool* pool)
	{
		if ((m_pools[type][usage] = pool)->CreateResources(enable_defragging, best_fit) == false)
		{
			CryLogAlways("SPoolManager::Initialize: could not initialize buffer pool of type '%s|%s'"
			             , ConstantToString(type)
			             , ConstantToString(usage));
			return false;
		}
		return true;
	}

	bool Initialize()
	{
		bool success = true;
		if (!s_PoolConfig.Configure())
			goto error;

		for (size_t i = 0; i < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
			if (gRenDev->m_DevMan.CreateFence(m_fences[i]) != S_OK)
			{
				CryLogAlways("SPoolManager::Initialize: could not create per-frame gpu fence");
				goto error;
			}

#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
		m_constant_allocator[0].Initialize();
		m_constant_allocator[1].Initialize();
#endif

		success &= CreatePool(BBT_VERTEX_BUFFER, BU_STATIC, gRenDev->CV_r_buffer_pool_defrag_static > 0 && gRenDev->GetActiveGPUCount() == 1, true, new SStaticBufferPoolVB(m_staging_resources[BU_STATIC]));
		success &= CreatePool(BBT_INDEX_BUFFER, BU_STATIC, gRenDev->CV_r_buffer_pool_defrag_static > 0 && gRenDev->GetActiveGPUCount() == 1, true, new SStaticBufferPoolIB(m_staging_resources[BU_STATIC]));
		success &= CreatePool(BBT_VERTEX_BUFFER, BU_DYNAMIC, gRenDev->CV_r_buffer_pool_defrag_dynamic > 0 && gRenDev->GetActiveGPUCount() == 1, true, new SDynamicBufferPoolVB(m_staging_resources[BU_DYNAMIC]));
		success &= CreatePool(BBT_INDEX_BUFFER, BU_DYNAMIC, gRenDev->CV_r_buffer_pool_defrag_dynamic > 0 && gRenDev->GetActiveGPUCount() == 1, true, new SDynamicBufferPoolIB(m_staging_resources[BU_DYNAMIC]));
		success &= CreatePool(BBT_VERTEX_BUFFER, BU_TRANSIENT, false, false, new SDynamicBufferPoolVB(m_staging_resources[BU_DYNAMIC]));
		success &= CreatePool(BBT_INDEX_BUFFER, BU_TRANSIENT, false, false, new SDynamicBufferPoolIB(m_staging_resources[BU_DYNAMIC]));

#if BUFFER_SUPPORT_TRANSIENT_POOLS
		success &= CreatePool(BBT_VERTEX_BUFFER, BU_TRANSIENT_RT, false, false, new CTransientBufferPoolVB());
		success &= CreatePool(BBT_INDEX_BUFFER, BU_TRANSIENT_RT, false, false, new CTransientBufferPoolIB());
		success &= CreatePool(BBT_VERTEX_BUFFER, BU_WHEN_LOADINGTHREAD_ACTIVE, false, false, new CTransientBufferPoolVB());
		success &= CreatePool(BBT_INDEX_BUFFER, BU_WHEN_LOADINGTHREAD_ACTIVE, false, false, new CTransientBufferPoolIB());
#else
		success &= CreatePool(BBT_VERTEX_BUFFER, BU_TRANSIENT_RT, false, false, new SDynamicBufferPoolVB(m_staging_resources[BU_DYNAMIC]));
		success &= CreatePool(BBT_INDEX_BUFFER, BU_TRANSIENT_RT, false, false, new SDynamicBufferPoolIB(m_staging_resources[BU_DYNAMIC]));
		success &= CreatePool(BBT_VERTEX_BUFFER, BU_WHEN_LOADINGTHREAD_ACTIVE, false, false, new SDynamicBufferPoolVB(m_staging_resources[BU_DYNAMIC]));
		success &= CreatePool(BBT_INDEX_BUFFER, BU_WHEN_LOADINGTHREAD_ACTIVE, false, false, new SDynamicBufferPoolIB(m_staging_resources[BU_DYNAMIC]));
#endif

		if (!success)
		{
			CryLogAlways("SPoolManager::Initialize: could not initialize a buffer pool");
			goto error;
		}

		m_buffer_creators[BBT_VERTEX_BUFFER][BU_STATIC] = &SStaticFreeBufferVB::Create;
		m_buffer_creators[BBT_INDEX_BUFFER][BU_STATIC] = &SStaticFreeBufferIB::Create;
		m_buffer_creators[BBT_VERTEX_BUFFER][BU_DYNAMIC] = &SDynamicFreeBufferVB::Create;
		m_buffer_creators[BBT_INDEX_BUFFER][BU_DYNAMIC] = &SDynamicFreeBufferIB::Create;
		m_buffer_creators[BBT_VERTEX_BUFFER][BU_TRANSIENT] = &SDynamicFreeBufferVB::Create;
		m_buffer_creators[BBT_INDEX_BUFFER][BU_TRANSIENT] = &SDynamicFreeBufferIB::Create;
		m_buffer_creators[BBT_VERTEX_BUFFER][BU_TRANSIENT_RT] = &SDynamicFreeBufferVB::Create;
		m_buffer_creators[BBT_INDEX_BUFFER][BU_TRANSIENT_RT] = &SDynamicFreeBufferIB::Create;

#if defined(CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK) && BUFFER_USE_STAGED_UPDATES
		if (m_debug_staging_vb.CreateResources() == false || m_debug_staging_ib.CreateResources() == false)
		{
			CryLogAlways("SPoolManager::Initialize: could not create debug staging buffers");
			goto error;
		}
#endif

#if ENABLE_STATOSCOPE
		memset(m_sdata, 0, sizeof(m_sdata));
#endif

		if (false)
		{
error:
			Shutdown();
			return false;
		}
		m_initialized = true;
		return true;
	}

	bool Shutdown()
	{
		bool success = true;
		for (size_t i = 0; i < BBT_MAX; ++i)
			for (size_t j = 0; j < BU_MAX; ++j)
			{
				if (m_pools[i][j] && !m_pools[i][j]->FreeResources())
				{
					CryLogAlways("SPoolManager::Initialize: could not shutdown buffer pool of type '%s|%s'"
					             , ConstantToString((BUFFER_USAGE)i)
					             , ConstantToString((BUFFER_USAGE)j));
					success = false;
				}
				SAFE_DELETE(m_pools[i][j]);
			}

#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
		m_constant_allocator[0].Shutdown();
		m_constant_allocator[1].Shutdown();
#endif

#if defined(CRY_USE_DX12)
		m_ResourceDescriptorPool.FreeResources();
#endif

		for (size_t i = 0; i < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
		{
			if (gRenDev->m_DevMan.ReleaseFence(m_fences[i]) != S_OK)
			{
				CryLogAlways("SPoolManager::Initialize: could not releasefence");
				success = false;
			}
			m_fences[i] = DeviceFenceHandle();
		}

#if defined(CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK) && BUFFER_USE_STAGED_UPDATES
		if (m_debug_staging_vb.FreeResources() == false || m_debug_staging_ib.FreeResources() == false)
		{
			CryLogAlways("SPoolManager::Initialize: could not destroy debug staging buffers");
			success = false;
		}
#endif

		m_initialized = false;
		return success;
	}
};
// One instance to rule them all
static SPoolManager s_PoolManager;

#if ENABLE_STATOSCOPE
class CStatoscopeDevBufferStats : public IStatoscopeDataGroup
{
	SDeviceBufferPoolStats m_stats[BBT_MAX][BU_MAX];
public:
	virtual SDescription GetDescription() const
	{
		return SDescription('b', "devbuffer", "['/DevBuffer/' "
		                                      "(float written_kb) "
		                                      "(float read_kb) "
		                                      "(float creation_time) "
		                                      "(float io_time) "
		                                      "(float cpu_flush) "
		                                      "(float gpu_flush) "
		                                      "(int cb kb)"
		                                      "]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		SREC_AUTO_LOCK(s_PoolManager.m_lock);
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		double rfreq = 1. / (double)freq.QuadPart;

		fr.AddValue(s_PoolManager.m_sdata[1].m_written_bytes / 1024.f);
		fr.AddValue(s_PoolManager.m_sdata[1].m_read_bytes / 1024.f);
		fr.AddValue((float)(s_PoolManager.m_sdata[1].m_creator_time * rfreq));
		fr.AddValue((float)(s_PoolManager.m_sdata[1].m_io_time * rfreq) * 1000.f);
		fr.AddValue((float)(s_PoolManager.m_sdata[1].m_cpu_flush_time * rfreq) * 1000.f);
		fr.AddValue((float)(s_PoolManager.m_sdata[1].m_gpu_flush_time * rfreq) * 1000.f);

	#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
		size_t cbsize =
		  s_PoolManager.m_constant_allocator[0].m_pages +
		  s_PoolManager.m_constant_allocator[1].m_pages;
		cbsize *= s_PoolConfig.m_pool_bank_size;
		fr.AddValue((int)(cbsize >> 10));
	#else
		fr.AddValue(0);
	#endif

		memset(&s_PoolManager.m_sdata[1], 0, sizeof(s_PoolManager.m_sdata[1]));
	}
};

class CStatoscopeDevBufferDG : public IStatoscopeDataGroup
{
	SDeviceBufferPoolStats m_stats[BBT_MAX][BU_MAX];
public:
	virtual SDescription GetDescription() const
	{
		return SDescription('D', "devbuffers", "['/DevBuffers/$' "
		                                       "(int poolAllocatedSize) "
		                                       "(int poolNumBanks) "
		                                       "(int poolNumAllocs) "
		                                       "(int poolFreeBlocks) "
		                                       "(int poolMovingBlocks) "
		                                       "(float poolUsageMB) "
		                                       "(float poolFreeMB) "
		                                       "(float poolFrag) "
		                                       "]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		for (int i = 0; i < BU_MAX; ++i)
			for (int j = 0; j < BBT_MAX; ++j)
			{
				new(&m_stats[j][i])SDeviceBufferPoolStats();
				SDeviceBufferPoolStats& stats = m_stats[j][i];
				gRenDev->m_DevBufMan.GetStats((BUFFER_BIND_TYPE)j, (BUFFER_USAGE)i, stats);
				fr.AddValue(stats.buffer_descr.c_str());
				fr.AddValue((int)stats.bank_size * (int)stats.num_banks);
				fr.AddValue((int)stats.num_banks);
				fr.AddValue((int)stats.allocator_stats.nInUseBlocks);
				fr.AddValue((int)stats.allocator_stats.nFreeBlocks);
				fr.AddValue((int)stats.allocator_stats.nMovingBlocks);
				fr.AddValue(stats.allocator_stats.nInUseSize / (1024.0f * 1024.f));
				fr.AddValue((stats.allocator_stats.nCapacity - stats.allocator_stats.nInUseSize) / (1024.0f * 1024.0f));
				fr.AddValue((stats.allocator_stats.nCapacity - stats.allocator_stats.nInUseSize - stats.allocator_stats.nLargestFreeBlockSize) / (float)max(stats.allocator_stats.nCapacity, (size_t)1u));
				m_stats[j][i].~SDeviceBufferPoolStats();
			}
	}
	uint32 PrepareToWrite()
	{
		return BU_MAX * BBT_MAX;
	}
};
#endif

}

//////////////////////////////////////////////////////////////////////////////////////
CDeviceBufferManager::CDeviceBufferManager()
{
}

//////////////////////////////////////////////////////////////////////////////////////
CDeviceBufferManager::~CDeviceBufferManager()
{
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::LockDevMan()
{
	MEMORY_SCOPE_CHECK_HEAP();
	s_PoolManager.m_lock.Lock();
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::UnlockDevMan()
{
	MEMORY_SCOPE_CHECK_HEAP();
	s_PoolManager.m_lock.Unlock();
}

//////////////////////////////////////////////////////////////////////////////////////
bool CDeviceBufferManager::Init()
{
	LOADING_TIME_PROFILE_SECTION;
	MEMORY_SCOPE_CHECK_HEAP();
	SREC_AUTO_LOCK(s_PoolManager.m_lock);
	if (s_PoolManager.m_initialized == true)
		return true;

	// Initialize the pool manager
	if (!s_PoolManager.Initialize())
	{
		CryFatalError("CDeviceBufferManager::Init(): pool manager failed to initialize");
		return false;
	}

#if ENABLE_STATOSCOPE
	gEnv->pStatoscope->RegisterDataGroup(new CStatoscopeDevBufferDG());
	gEnv->pStatoscope->RegisterDataGroup(new CStatoscopeDevBufferStats());
#endif
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////
bool CDeviceBufferManager::Shutdown()
{
	MEMORY_SCOPE_CHECK_HEAP();
	SREC_AUTO_LOCK(s_PoolManager.m_lock);
	if (s_PoolManager.m_initialized == false)
		return true;

	// Initialize the pool manager
	if (!s_PoolManager.Shutdown())
	{
		CryFatalError("CDeviceBufferManager::Init(): pool manager failed during shutdown");
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::Sync(uint32 frameId)
{
	FUNCTION_PROFILER_RENDERER;
	MEMORY_SCOPE_CHECK_HEAP();
	SREC_AUTO_LOCK(s_PoolManager.m_lock);

	for (int i = 0; i < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
		gRenDev->m_DevMan.SyncFence(s_PoolManager.m_fences[i], true);

	for (size_t i = 0; i < BBT_MAX; ++i)
		for (size_t j = 0; j < BU_MAX; ++j)
		{
			IF (s_PoolManager.m_pools[i][j] == NULL, 0)
				continue;
			SREC_AUTO_LOCK(s_PoolManager.m_pools[i][j]->m_lock);
			s_PoolManager.m_pools[i][j]->Sync();
		}

#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	s_PoolManager.m_constant_allocator[0].FlushRetiredSlots();
	s_PoolManager.m_constant_allocator[1].FlushRetiredSlots();
#endif

	// Note: Issue the fence now for COPY_ON_WRITE. If the GPU has caught up to this point, no previous drawcall
	// will be pending and therefore it is safe to just reuse the previous allocation.
	gRenDev->m_DevMan.IssueFence(s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::ReleaseEmptyBanks(uint32 frameId)
{
	FUNCTION_PROFILER_RENDERER;
	MEMORY_SCOPE_CHECK_HEAP();
	SREC_AUTO_LOCK(s_PoolManager.m_lock);

	for (size_t i = 0; i < BBT_MAX; ++i)
		for (size_t j = 0; j < BU_MAX; ++j)
		{
			IF (s_PoolManager.m_pools[i][j] == NULL, 0)
				continue;
			SREC_AUTO_LOCK(s_PoolManager.m_pools[i][j]->m_lock);
			s_PoolManager.m_pools[i][j]->ReleaseEmptyBanks();
		}

	// Release empty constant buffers
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	for (size_t i = 0; i < 2; ++i)
		s_PoolManager.m_constant_allocator[i].ReleaseEmptyBanks();
#endif

	// Note: Issue the current fence for retiring allocations. This is the same fence shelled out
	// to the pools during the update stage for COW, now we are reusing it to ensure the gpu caught
	// up to this point and therefore give out reclaimed memory again.
	gRenDev->m_DevMan.IssueFence(s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::Update(uint32 frameId, bool called_during_loading)
{
	FUNCTION_PROFILER_RENDERER;

	MEMORY_SCOPE_CHECK_HEAP();
	SREC_AUTO_LOCK(s_PoolManager.m_lock);

#if ENABLE_STATOSCOPE
	s_PoolManager.m_sdata[1].m_written_bytes += s_PoolManager.m_sdata[0].m_written_bytes;
	s_PoolManager.m_sdata[1].m_read_bytes += s_PoolManager.m_sdata[0].m_read_bytes;
	s_PoolManager.m_sdata[1].m_creator_time += s_PoolManager.m_sdata[0].m_creator_time;
	s_PoolManager.m_sdata[1].m_io_time += s_PoolManager.m_sdata[0].m_io_time;
	s_PoolManager.m_sdata[1].m_cpu_flush_time += s_PoolManager.m_sdata[0].m_cpu_flush_time;
	s_PoolManager.m_sdata[1].m_gpu_flush_time += s_PoolManager.m_sdata[0].m_gpu_flush_time;
	memset(&s_PoolManager.m_sdata[0], 0, sizeof(s_PoolManager.m_sdata[0]));
#endif

	gRenDev->m_DevMan.SyncFence(s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK], true);

	for (size_t i = 0; i < BBT_MAX; ++i)
		for (size_t j = 0; j < BU_MAX; ++j)
		{
			IF (s_PoolManager.m_pools[i][j] == NULL, 0)
				continue;
			SREC_AUTO_LOCK(s_PoolManager.m_pools[i][j]->m_lock);
			s_PoolManager.m_pools[i][j]->Update(frameId
			                                    , s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]
			                                    , called_during_loading == false);
		}

	// Update the constant buffers
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	for (size_t i = 0; i < 2; ++i)
	{
		s_PoolManager.m_constant_allocator[i].Update(frameId
		                                             , s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]
		                                             , called_during_loading == false);
	}
#endif

#if defined(CRY_USE_DX12)
	s_PoolManager.m_ResourceDescriptorPool.Update(frameId,
	                                              s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]);
#endif

	// Note: Issue the fence now for COPY_ON_WRITE. If the GPU has caught up to this point, no previous drawcall
	// will be pending and therefore it is safe to just reuse the previous allocation.
	gRenDev->m_DevMan.IssueFence(s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]);
}

#if defined(CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK)
//////////////////////////////////////////////////////////////////////////////////////
void* CDeviceBufferManager::BeginReadDirectIB(D3DIndexBuffer* pIB, size_t size, size_t offset)
{
	#if BUFFER_ENABLE_DIRECT_ACCESS
	uint8* base_ptr = NULL;
	CDeviceManager::ExtractBasePointer<CDeviceManager::BIND_INDEX_BUFFER>(pIB, base_ptr);
	return base_ptr + offset;
	#elif BUFFER_USE_STAGED_UPDATES
	return s_PoolManager.m_debug_staging_ib.BeginRead(pIB, size, offset);
	#else
	return NULL;
	#endif
}

//////////////////////////////////////////////////////////////////////////////////////
void* CDeviceBufferManager::BeginReadDirectVB(D3DVertexBuffer* pVB, size_t size, size_t offset)
{
	#if BUFFER_ENABLE_DIRECT_ACCESS
	uint8* base_ptr = NULL;
	CDeviceManager::ExtractBasePointer<CDeviceManager::BIND_VERTEX_BUFFER>(pVB, base_ptr);
	return base_ptr + offset;
	#elif BUFFER_USE_STAGED_UPDATES
	return s_PoolManager.m_debug_staging_vb.BeginRead(pVB, size, offset);
	#else
	return NULL;
	#endif
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::EndReadDirectIB(D3DIndexBuffer*)
{
	#if BUFFER_USE_STAGED_UPDATES
	s_PoolManager.m_debug_staging_ib.EndReadWrite();
	#endif
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::EndReadDirectVB(D3DVertexBuffer*)
{
	#if BUFFER_USE_STAGED_UPDATES
	s_PoolManager.m_debug_staging_vb.EndReadWrite();
	#endif
}
#endif

//////////////////////////////////////////////////////////////////////////////////////
CConstantBuffer* CDeviceBufferManager::CreateConstantBuffer(size_t size, bool dynamic, bool ts)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_creator_time);
	size = (max(size, size_t(1u)) + (255)) & ~(255);
	CConditonalDevManLock lock(this, ts);
	CConstantBuffer* buffer = &s_PoolManager.m_constant_buffers[ts][s_PoolManager.m_constant_buffers[ts].Allocate()];
	buffer->m_size = size;
	buffer->m_dynamic = dynamic;
	buffer->m_lock = ts;
	return buffer;
}

CConstantBufferPtr CDeviceBufferManager::CreateNullConstantBuffer()
{
	CConstantBufferPtr pBuffer;
	pBuffer.Assign_NoAddRef(new CConstantBuffer(0));
	pBuffer->m_size = 0;
	pBuffer->m_dynamic = false;
	pBuffer->m_lock = false;
	pBuffer->m_intentionallyNull = 1;
	pBuffer->m_buffer = static_cast<D3DBuffer*>(gRenDev->m_DevMan.AllocateNullResource(D3D11_RESOURCE_DIMENSION_BUFFER));
	return pBuffer;
}

#if defined(CRY_USE_DX12)
SDescriptorBlock* CDeviceBufferManager::CreateDescriptorBlock(size_t size)
{
	return s_PoolManager.m_ResourceDescriptorPool.Allocate(size);
}

void CDeviceBufferManager::ReleaseDescriptorBlock(SDescriptorBlock* pBlock)
{
	CRY_ASSERT(pBlock != NULL);
	s_PoolManager.m_ResourceDescriptorPool.Free(pBlock);
}

#else
SDescriptorBlock* CDeviceBufferManager::CreateDescriptorBlock(size_t size)               { return NULL; }
void              CDeviceBufferManager::ReleaseDescriptorBlock(SDescriptorBlock* pBlock) {}
#endif

//////////////////////////////////////////////////////////////////////////////////////
buffer_handle_t CDeviceBufferManager::Create_Locked(
  BUFFER_BIND_TYPE type
  , BUFFER_USAGE usage
  , size_t size)
{
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT((type >= BBT_VERTEX_BUFFER && type < BBT_MAX));
	DEVBUFFERMAN_ASSERT((usage >= BU_IMMUTABLE && usage < BU_MAX));
	DEVBUFFERMAN_ASSERT(s_PoolManager.m_pools[type][usage] != NULL);

	// Workaround for NVIDIA SLI issues with latest drivers. GFE should disable the cvar below when fixed
	// Disabled for now
#if CRY_PLATFORM_WINDOWS
	if (s_PoolManager.m_buffer_creators[type][usage])
	{
		if (gRenDev->GetActiveGPUCount() > 1
		    && gRenDev->m_bVendorLibInitialized
		    && gRenDev->CV_r_buffer_sli_workaround
		    && (usage == BU_DYNAMIC || usage == BU_TRANSIENT))
		{
			SBufferPool* pool = s_PoolManager.m_buffer_creators[type][usage](
			  s_PoolManager.m_staging_resources[usage],
			  size
			  );
			item_handle_t item_handle = pool->Allocate(size);
			return item_handle == ~0u
			       ? (buffer_handle_t) ~0u
			       : (buffer_handle_t)pool->Resolve(item_handle);
		}
	}
#endif

	item_handle_t item_handle = s_PoolManager.m_pools[type][usage]->Allocate(size);
	return item_handle == ~0u ? (buffer_handle_t) ~0u : (buffer_handle_t)s_PoolManager.m_pools[type][usage]->Resolve(item_handle);
}

buffer_handle_t CDeviceBufferManager::Create(
  BUFFER_BIND_TYPE type
  , BUFFER_USAGE usage
  , size_t size)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_creator_time);
	if (!s_PoolManager.m_pools[type][usage])
		return ~0u;
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SREC_AUTO_LOCK(s_PoolManager.m_pools[type][usage]->m_lock);
	return Create_Locked(type, usage, size);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::Destroy_Locked(buffer_handle_t handle)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	item.m_pool->Free(&item);
}
void CDeviceBufferManager::Destroy(buffer_handle_t handle)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_creator_time);
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	Destroy_Locked(handle);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::PinItem_Locked(buffer_handle_t handle)
{
	DEVBUFFERMAN_ASSERT(handle != ~0u);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	IF (item.m_bank != ~0u, 1)
		item.m_defrag_allocator->Pin(item.m_defrag_handle);
}
void CDeviceBufferManager::PinItem(buffer_handle_t handle)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_creator_time);
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	PinItem_Locked(handle);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::UnpinItem_Locked(buffer_handle_t handle)
{
	DEVBUFFERMAN_ASSERT(handle != ~0u);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	IF (item.m_bank != ~0u, 1)
		item.m_defrag_allocator->Unpin(item.m_defrag_handle);
}
void CDeviceBufferManager::UnpinItem(buffer_handle_t handle)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_creator_time);
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	UnpinItem_Locked(handle);
}
//////////////////////////////////////////////////////////////////////////////////////
void* CDeviceBufferManager::BeginRead_Locked(buffer_handle_t handle)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_READ(Size(handle));
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	return item.m_pool->BeginRead(&item);
}
void* CDeviceBufferManager::BeginRead(buffer_handle_t handle)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	return BeginRead_Locked(handle);
}

//////////////////////////////////////////////////////////////////////////////////////
size_t CDeviceBufferManager::Size_Locked(buffer_handle_t handle)
{
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	return item.m_size;
}
size_t CDeviceBufferManager::Size(buffer_handle_t handle)
{
	return Size_Locked(handle);
}

//////////////////////////////////////////////////////////////////////////////////////
void* CDeviceBufferManager::BeginWrite_Locked(buffer_handle_t handle)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_WRITTEN(Size(handle));
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	return item.m_pool->BeginWrite(&item);
}
void* CDeviceBufferManager::BeginWrite(buffer_handle_t handle)
{
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	return BeginWrite_Locked(handle);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::EndReadWrite_Locked(buffer_handle_t handle)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_WRITTEN(Size(handle));
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	item.m_pool->EndReadWrite(&item, true);
}
void CDeviceBufferManager::EndReadWrite(buffer_handle_t handle)
{
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	return EndReadWrite_Locked(handle);
}

//////////////////////////////////////////////////////////////////////////////////////
bool CDeviceBufferManager::UpdateBuffer_Locked(
  buffer_handle_t handle, const void* src, size_t size)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_WRITTEN(Size(handle));
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	item.m_pool->Write(&item, src, size);
	return true;
}
bool CDeviceBufferManager::UpdateBuffer
  (buffer_handle_t handle, const void* src, size_t size)
{
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	return UpdateBuffer_Locked(handle, src, size);
}
//////////////////////////////////////////////////////////////////////////////////////
D3DBuffer* CDeviceBufferManager::GetD3D(buffer_handle_t handle, size_t* offset)
{
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	*offset = item.m_offset;
	DEVBUFFERMAN_ASSERT(item.m_buffer);
	return item.m_buffer;
}

//////////////////////////////////////////////////////////////////////////////////////
D3DVertexBuffer* CDeviceBufferManager::GetD3DVB(buffer_handle_t handle, size_t* offset)
{
	MEMORY_SCOPE_CHECK_HEAP();
	return static_cast<D3DVertexBuffer*>(GetD3D(handle, offset));
}

//////////////////////////////////////////////////////////////////////////////////////
D3DIndexBuffer* CDeviceBufferManager::GetD3DIB(buffer_handle_t handle, size_t* offset)
{
	MEMORY_SCOPE_CHECK_HEAP();
	return static_cast<D3DIndexBuffer*>(GetD3D(handle, offset));
}

//////////////////////////////////////////////////////////////////////////////////////
bool CDeviceBufferManager::GetStats(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, SDeviceBufferPoolStats& stats)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	stats.buffer_descr = string(ConstantToString(type));
	stats.buffer_descr += "_";
	stats.buffer_descr += string(ConstantToString(usage));
	stats.buffer_descr += "_";
	if (!s_PoolManager.m_pools[type][usage])
		return false;
	SREC_AUTO_LOCK(s_PoolManager.m_pools[type][usage]->m_lock);
	return s_PoolManager.m_pools[type][usage]->GetStats(stats);
}

////////////////////////////////////////////////////////////////////////////////////////
// returns a reference to the internal statoscope data (used to break cyclic depencies in file)
#if ENABLE_STATOSCOPE
SStatoscopeData& GetStatoscopeData(uint32 nIndex)
{
	return s_PoolManager.m_sdata[nIndex];
}
#endif
/////////////////////////////////////////////////////////////
// Legacy interface
//
// Use with care, can be removed at any point!
//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::ReleaseVBuffer(CVertexBuffer* pVB)
{
	MEMORY_SCOPE_CHECK_HEAP();
	SAFE_DELETE(pVB);
}
//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::ReleaseIBuffer(CIndexBuffer* pIB)
{
	MEMORY_SCOPE_CHECK_HEAP();
	SAFE_DELETE(pIB);
}
//////////////////////////////////////////////////////////////////////////////////////
CVertexBuffer* CDeviceBufferManager::CreateVBuffer(size_t nVerts, EVertexFormat eVF, const char* szName, BUFFER_USAGE usage)
{
	MEMORY_SCOPE_CHECK_HEAP();
	CVertexBuffer* pVB = new CVertexBuffer(NULL, eVF);
	pVB->m_nVerts = nVerts;
	pVB->m_VS.m_BufferHdl = Create(BBT_VERTEX_BUFFER, usage, nVerts * CRenderMesh::m_cSizeVF[eVF]);

	return pVB;
}
//////////////////////////////////////////////////////////////////////////////////////
CIndexBuffer* CDeviceBufferManager::CreateIBuffer(size_t nInds, const char* szNam, BUFFER_USAGE usage)
{
	MEMORY_SCOPE_CHECK_HEAP();
	CIndexBuffer* pIB = new CIndexBuffer(NULL);
	pIB->m_nInds = nInds;
	pIB->m_VS.m_BufferHdl = Create(BBT_INDEX_BUFFER, usage, nInds * sizeof(uint16));
	return pIB;
}
//////////////////////////////////////////////////////////////////////////////////////
bool CDeviceBufferManager::UpdateVBuffer(CVertexBuffer* pVB, void* pVerts, size_t nVerts)
{
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(pVB->m_VS.m_BufferHdl != ~0u);
	return UpdateBuffer(pVB->m_VS.m_BufferHdl, pVerts, nVerts * CRenderMesh::m_cSizeVF[pVB->m_eVF]);
}
//////////////////////////////////////////////////////////////////////////////////////
bool CDeviceBufferManager::UpdateIBuffer(CIndexBuffer* pIB, void* pInds, size_t nInds)
{
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(pIB->m_VS.m_BufferHdl != ~0u);
	return UpdateBuffer(pIB->m_VS.m_BufferHdl, pInds, nInds * sizeof(uint16));
}

//////////////////////////////////////////////////////////////////////////////////////
CConstantBuffer::CConstantBuffer(uint32 handle)
	: m_buffer()
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	, m_allocator()
#endif
	, m_base_ptr()
	, m_handle(handle)
	, m_offset(0)
	, m_size(0)
	, m_nRefCount(1u)
	, m_clearFlags(0)
{}

//////////////////////////////////////////////////////////////////////////////////////
CConstantBuffer::~CConstantBuffer()
{
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS == 0
	UnsetStreamSources(m_buffer);
	SAFE_RELEASE(m_buffer);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////
void CConstantBuffer::AddRef()
{
	CryInterlockedIncrement(&m_nRefCount);
}

//////////////////////////////////////////////////////////////////////////////////////
void CConstantBuffer::Release()
{
	assert(gRenDev->m_pRT->IsRenderThread());

	if (CryInterlockedDecrement(&m_nRefCount) == 0)
	{
		CConditonalDevManLock lock(&gcpRendD3D->m_DevBufMan, m_lock);
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
		if (m_used)
		{
			s_PoolManager.m_constant_allocator[m_lock].Free(this);
			m_used = 0;
		}
#endif
		if (!m_intentionallyNull)
		{
			s_PoolManager.m_constant_buffers[m_lock].Free(m_handle);
		}
		else
		{
			SAFE_RELEASE(m_buffer);
		}

		m_used = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////////////
void* CConstantBuffer::BeginWrite()
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_WRITTEN(m_size);
	CConditonalDevManLock lock(&gcpRendD3D->m_DevBufMan, m_lock);
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	if (m_used)
	{
		s_PoolManager.m_constant_allocator[m_lock].Free(this);
		m_used = 0;
	}
	if (s_PoolManager.m_constant_allocator[m_lock].Allocate(this))
	{
		m_used = 1;
		return (void*)((uintptr_t)m_base_ptr + m_offset);
	}
#else
	if (!m_used)
	{
		HRESULT hr;
		D3D11_BUFFER_DESC bd;
		ZeroStruct(bd);
		bd.Usage = m_dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = m_dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
		bd.MiscFlags = 0;
	#if defined(OPENGL)
		bd.MiscFlags |= m_no_streaming * D3D11_RESOURCE_MISC_DXGL_NO_STREAMING;
	#endif
		bd.ByteWidth = m_size;
		hr = gcpRendD3D->GetDevice().CreateBuffer(&bd, NULL, &m_buffer);
		CHECK_HRESULT(hr);
		assert(m_buffer);

		m_used = 1;
	}
	if (m_dynamic)
	{
		assert(m_buffer);
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		gcpRendD3D->GetDeviceContext().Map(m_buffer, 0, D3D11_MAP_WRITE_DISCARD_CB, 0, &mappedResource);
		return mappedResource.pData;
	}
	else
	{
		return m_base_ptr = new char[m_size];
	}
#endif
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////
void CConstantBuffer::EndWrite(bool requires_flush)
{
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS == 0
	if (m_dynamic)
	{
		gcpRendD3D->GetDeviceContext().Unmap(m_buffer, 0);
	}
	else
	{
		gcpRendD3D->GetDeviceContext().UpdateSubresource(m_buffer, 0, NULL, m_base_ptr, 0, 0);
		delete[] alias_cast<char*>(m_base_ptr);
		m_base_ptr = NULL;
	}
#endif
}
//////////////////////////////////////////////////////////////////////////////////////
void CConstantBuffer::UpdateBuffer(const void* src, size_t size)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_WRITTEN(m_size);
	if (void* dst = BeginWrite())
	{
		bool requires_flush = CopyData(dst, src, std::min((size_t)m_size, size));
		EndWrite();
	}
}

//////////////////////////////////////////////////////////////////////////////////////
CVertexBuffer::~CVertexBuffer()
{
	MEMORY_SCOPE_CHECK_HEAP();
	if (m_VS.m_BufferHdl != ~0u)
	{
		//STATIC UNINITIALISATION. This can be called after gRenDev has been released
		if (gRenDev)
			gRenDev->m_DevBufMan.Destroy(m_VS.m_BufferHdl);
		m_VS.m_BufferHdl = ~0u;
	}
}
//////////////////////////////////////////////////////////////////////////////////////
CIndexBuffer::~CIndexBuffer()
{
	MEMORY_SCOPE_CHECK_HEAP();
	if (m_VS.m_BufferHdl != ~0u)
	{
		if (gRenDev)
			gRenDev->m_DevBufMan.Destroy(m_VS.m_BufferHdl);
		m_VS.m_BufferHdl = ~0u;
	}
}

CGpuBuffer::STrackedBuffer::STrackedBuffer(const STrackedBuffer& other)
{
	m_pBuffer = other.m_pBuffer;
	m_pSRV = other.m_pSRV;
	m_pUAV = other.m_pUAV;

	m_nLastWriteFrame = other.m_nLastWriteFrame;
	m_nLastReadFrame = other.m_nLastReadFrame;
	m_BufferPersistentMapMode = other.m_BufferPersistentMapMode;

	EnablePersistentMap(true);
}

CGpuBuffer::STrackedBuffer::STrackedBuffer(STrackedBuffer&& other)
{
	m_pBuffer = std::move(other.m_pBuffer);
	m_pSRV = std::move(other.m_pSRV);
	m_pUAV = std::move(other.m_pUAV);

	m_nLastWriteFrame = std::move(other.m_nLastWriteFrame);
	m_nLastReadFrame = std::move(other.m_nLastReadFrame);
	m_BufferPersistentMapMode = std::move(other.m_BufferPersistentMapMode);

	other.m_nLastWriteFrame = 0;
	other.m_nLastReadFrame = 0;
	other.m_BufferPersistentMapMode = D3D11_MAP(0);
}

CGpuBuffer::STrackedBuffer::~STrackedBuffer()
{
	EnablePersistentMap(false);
}

void CGpuBuffer::STrackedBuffer::EnablePersistentMap(bool bEnable)
{
#if defined(CRY_USE_DX12)
	if (!m_pBuffer || m_BufferPersistentMapMode == 0)
		return;

	if (bEnable)
	{
		D3D11_MAPPED_SUBRESOURCE mappedRes;
		gcpRendD3D->GetDeviceContext().Map(m_pBuffer, 0, m_BufferPersistentMapMode, 0, &mappedRes);
	}
	else
	{
		gcpRendD3D->GetDeviceContext().Unmap(m_pBuffer, 0);
	}
#endif
}

bool CGpuBuffer::operator==(const CGpuBuffer& other) const
{
	return m_pBuffersInUse == other.m_pBuffersInUse &&
	       m_numElements == other.m_numElements &&
	       m_flags == other.m_flags;
}

CGpuBuffer::~CGpuBuffer()
{
	Release();
}

void CGpuBuffer::Release()
{
	MEMORY_SCOPE_CHECK_HEAP();

	m_pBuffersInUse.reset();
	m_numElements = 0;
	m_flags = 0;
	m_bLocked = false;
	m_MapMode = D3D11_MAP(0);

	ZeroStruct(m_bufferDesc);
	ZeroStruct(m_srvDesc);
	ZeroStruct(m_uavDesc);
}

void CGpuBuffer::Create(uint32 numElements, uint32 elementSize, DXGI_FORMAT elementFormat, uint32 flags, const void* pData)
{
	Release();

	m_bufferDesc.BindFlags = ((flags & DX11BUF_BIND_VERTEX_BUFFER) ? D3D11_BIND_VERTEX_BUFFER : 0) |
	                         ((flags & DX11BUF_BIND_INDEX_BUFFER) ? D3D11_BIND_INDEX_BUFFER : 0) |
	                         ((flags & DX11BUF_BIND_SRV) ? D3D11_BIND_SHADER_RESOURCE : 0) |
	                         ((flags & DX11BUF_BIND_UAV) ? D3D11_BIND_UNORDERED_ACCESS : 0);
	m_bufferDesc.CPUAccessFlags = ((flags & DX11BUF_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE : 0);
	m_bufferDesc.MiscFlags = ((flags & DX11BUF_STRUCTURED) ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED : 0) |
	                         ((flags & DX11BUF_DRAWINDIRECT) ? D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS : 0);
	m_bufferDesc.ByteWidth = numElements * elementSize;
	m_bufferDesc.StructureByteStride = elementSize;

	// D3D11_USAGE_IMMUTABLE
	// A resource that can only be read by the GPU.It cannot be written by the GPU, and cannot be accessed
	// at all by the CPU.This type of resource must be initialized when it is created, since it cannot be
	// changed after creation.
	//
	// Buffers without initial data can't possibly IMMUTABLE, they would be buffers without defined content!
	m_bufferDesc.Usage = (flags & DX11BUF_DYNAMIC) ? D3D11_USAGE_DYNAMIC : (flags & DX11BUF_BIND_UAV || !pData ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE);

	m_MapMode = ((flags & DX11BUF_BIND_SRV) ? D3D11_MAP_WRITE_NO_OVERWRITE_SR :
	             ((flags & DX11BUF_BIND_VERTEX_BUFFER) ? D3D11_MAP_WRITE_NO_OVERWRITE_VB :
	              ((flags & DX11BUF_BIND_INDEX_BUFFER) ? D3D11_MAP_WRITE_NO_OVERWRITE_IB :
	               ((flags & DX11BUF_BIND_UAV) ? D3D11_MAP_WRITE_NO_OVERWRITE_UA : D3D11_MAP(0)))));

	// SRV desc
	m_srvDesc.Format = elementFormat;
	m_srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	m_srvDesc.Buffer.ElementOffset = 0;
	m_srvDesc.Buffer.NumElements = numElements;

	// UAV desc
	m_uavDesc.Format = elementFormat;
	m_uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	m_uavDesc.Buffer.FirstElement = 0;
	m_uavDesc.Buffer.Flags = (flags & DX11BUF_UAV_APPEND) ? D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER : 0;
	m_uavDesc.Buffer.Flags |= (flags & DX11BUF_UAV_COUNTER) ? D3D11_BUFFER_UAV_FLAG_COUNTER : 0;
	m_uavDesc.Buffer.NumElements = numElements;

	m_numElements = numElements;
	m_flags = flags;

	PrepareFreeBuffer(pData);
}

void CGpuBuffer::PrepareFreeBuffer(const void* pInitialData)
{
	if (!m_pBuffersInUse)
	{
		m_pBuffersInUse = std::make_shared<BufferQueue>();
	}

	if (m_pBuffersInUse->empty() || m_pBuffersInUse->front().m_nLastReadFrame > gcpRendD3D->GetCompletedFrameCounter())
	{
		CRY_ASSERT(m_pBuffersInUse->size() < m_MaxBufferCopies);
		m_pBuffersInUse->push(STrackedBuffer());

		STrackedBuffer& curBuffer = m_pBuffersInUse->back();
		ZeroStruct(curBuffer);

		D3D11_SUBRESOURCE_DATA Data;
		Data.pSysMem = pInitialData;
		Data.SysMemPitch = m_bufferDesc.ByteWidth;
		Data.SysMemSlicePitch = m_bufferDesc.ByteWidth;

		HRESULT hr = S_OK;

		if (m_flags & DX11BUF_NULL_RESOURCE)
		{
			D3DBuffer* pBuffer = static_cast<D3DBuffer*>(gRenDev->m_DevMan.AllocateNullResource(D3D11_RESOURCE_DIMENSION_BUFFER));
			curBuffer.m_pBuffer.Assign_NoAddRef(pBuffer);
		}
		else
		{
			ID3D11Buffer* pBuffer = nullptr;
			hr = gcpRendD3D->GetDevice().CreateBuffer(&m_bufferDesc, (pInitialData != nullptr) ? &Data : nullptr, &pBuffer);
			curBuffer.m_pBuffer.Assign_NoAddRef(pBuffer);
		}

		if (SUCCEEDED(hr) && curBuffer.m_pBuffer)
		{
			if (pInitialData)
			{
				curBuffer.m_nLastWriteFrame = gcpRendD3D->GetCurrentFrameCounter();
			}

			if (curBuffer.m_pBuffer && (m_flags & DX11BUF_DYNAMIC))
			{
				curBuffer.m_BufferPersistentMapMode = m_MapMode;
				curBuffer.EnablePersistentMap(true);
			}

			if (m_flags & DX11BUF_BIND_SRV)
			{
				ID3D11ShaderResourceView* pSrv;
				gcpRendD3D->GetDevice().CreateShaderResourceView(curBuffer.m_pBuffer, &m_srvDesc, &pSrv);
				curBuffer.m_pSRV.Assign_NoAddRef(pSrv);
			}

			if (m_flags & DX11BUF_BIND_UAV)
			{
				ID3D11UnorderedAccessView* pUav;
				gcpRendD3D->GetDevice().CreateUnorderedAccessView(curBuffer.m_pBuffer, &m_uavDesc, &pUav);
				curBuffer.m_pUAV.Assign_NoAddRef(pUav);
			}
		}
	}
	else
	{
		CRY_ASSERT(!pInitialData);

		STrackedBuffer oldestBuffer(std::move(m_pBuffersInUse->front()));
		m_pBuffersInUse->pop();
		m_pBuffersInUse->push(std::move(oldestBuffer));
	}
}

CGpuBuffer::STrackedBuffer& CGpuBuffer::GetCurrentBuffer()
{
	CRY_ASSERT(m_pBuffersInUse && !m_pBuffersInUse->empty());
	return m_pBuffersInUse->back();
}

const CGpuBuffer::STrackedBuffer& CGpuBuffer::GetCurrentBuffer() const
{
	CRY_ASSERT(m_pBuffersInUse && !m_pBuffersInUse->empty());
	return m_pBuffersInUse->back();
}

void CGpuBuffer::UpdateBufferContent(const void* pData, size_t nSize)
{
	CRY_ASSERT(!m_pBuffersInUse->empty());
	CRY_ASSERT(!m_bLocked);

	PrepareFreeBuffer();

	STrackedBuffer& curBuffer = GetCurrentBuffer();
	curBuffer.m_nLastWriteFrame = gcpRendD3D->GetCurrentFrameCounter();

	if (m_flags & DX11BUF_DYNAMIC)
	{
		// Synchronous
		D3D11_MAPPED_SUBRESOURCE mappedRes;
		gcpRendD3D->GetDeviceContext().Map(curBuffer.m_pBuffer, 0, m_MapMode, 0, &mappedRes);
		memcpy(mappedRes.pData, pData, nSize);
		gcpRendD3D->GetDeviceContext().Unmap(curBuffer.m_pBuffer, 0);
	}
	else
	{
		// Asynchronous
		gcpRendD3D->GetDeviceContext().UpdateSubresource(curBuffer.m_pBuffer, 0, nullptr, pData, 0, 0);
	}
}

void* CGpuBuffer::Lock()
{
	CRY_ASSERT(!m_pBuffersInUse->empty());
	CRY_ASSERT(m_flags & DX11BUF_DYNAMIC);
	CRY_ASSERT(!m_bLocked);

	PrepareFreeBuffer();

	STrackedBuffer& curBuffer = GetCurrentBuffer();
	CRY_ASSERT(curBuffer.m_nLastReadFrame < gcpRendD3D->GetCurrentFrameCounter()); // read before write on same frame
	curBuffer.m_nLastWriteFrame = gcpRendD3D->GetCurrentFrameCounter();

	m_bLocked = true;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	gcpRendD3D->GetDeviceContext().Map(curBuffer.m_pBuffer, 0, D3D11_MAP(m_MapMode), 0, &mappedResource);
	return mappedResource.pData;
}

void CGpuBuffer::Unlock()
{
	CRY_ASSERT(!m_pBuffersInUse->empty());
	CRY_ASSERT(m_flags & DX11BUF_DYNAMIC);
	CRY_ASSERT(m_bLocked);

	STrackedBuffer& curBuffer = GetCurrentBuffer();
	CRY_ASSERT(curBuffer.m_nLastReadFrame < gcpRendD3D->GetCurrentFrameCounter()); // read before write on same frame

	gcpRendD3D->GetDeviceContext().Unmap(curBuffer.m_pBuffer, 0);
	m_bLocked = false;
}

ID3D11Buffer* CGpuBuffer::GetBuffer() const
{
	if (!m_pBuffersInUse)
		return nullptr;

	return GetCurrentBuffer().m_pBuffer;
}

ID3D11ShaderResourceView* CGpuBuffer::GetSRV() const
{
	if (!m_pBuffersInUse)
		return nullptr;

	const STrackedBuffer& curBuffer = GetCurrentBuffer();
	curBuffer.m_nLastReadFrame = gcpRendD3D->GetCurrentFrameCounter();
	return curBuffer.m_pSRV;
}

ID3D11UnorderedAccessView* CGpuBuffer::GetUAV() const
{
	if (!m_pBuffersInUse)
		return nullptr;

	const STrackedBuffer& curBuffer = GetCurrentBuffer();
	curBuffer.m_nLastReadFrame = gcpRendD3D->GetCurrentFrameCounter();
	return curBuffer.m_pUAV;
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsDeviceConstantBuffer::SetData(const uint8* data, size_t size)
{
	CryAutoCriticalSection lock(s_accessLock);
	m_size = size;
	m_data.resize(0);
	m_data.insert(m_data.begin(), data, data + size);
	m_bDirty = true;
}

//////////////////////////////////////////////////////////////////////////
CConstantBufferPtr CGraphicsDeviceConstantBuffer::GetConstantBuffer()
{
	assert(m_size > 0);
	CryAutoCriticalSection lock(s_accessLock);
	if (!m_pConstantBuffer)
	{
		m_pConstantBuffer = gRenDev->m_DevBufMan.CreateConstantBuffer(m_size);
		// m_DevBufMan.CreateConstantBuffer creates a structure with a reference count already set to 1,
		// to assign it to smart pointer properly we need to free one original reference.
		m_pConstantBuffer->Release();
	}
	if (m_bDirty && m_data.size() > 0)
	{
		m_pConstantBuffer->UpdateBuffer(&m_data[0], m_data.size());
	}
	return m_pConstantBuffer;
}

CConstantBufferPtr CGraphicsDeviceConstantBuffer::GetNullConstantBuffer()
{
	assert(m_size > 0);
	CryAutoCriticalSection lock(s_accessLock);
	if (!m_pConstantBuffer)
	{
		m_pConstantBuffer = gRenDev->m_DevBufMan.CreateNullConstantBuffer();
	}
	if (m_bDirty && m_data.size() > 0)
	{
		// NOTE: A null-buffer isn't suppose to be dirty or have a size
		__debugbreak();
	}
	return m_pConstantBuffer;
}
