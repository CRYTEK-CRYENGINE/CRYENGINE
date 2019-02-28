// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   DevBuffer.cpp : Generic device Buffer management

   =============================================================================*/

#include "StdAfx.h"
#include <numeric>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryCore/BitFiddling.h> // IntegerLog2_RoundUp
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
	#define DEVBUFFERMAN_ASSERT(x) assert(x)
#else
	#define DEVBUFFERMAN_ASSERT(x) (void)0
#endif

// Change this to != 0 to allocate CConstantBuffers from pool
#ifdef _DEBUG
	#define DEVBUFFERMAN_CCONSTANTBUFFER_POOL 0
#else
	#define DEVBUFFERMAN_CCONSTANTBUFFER_POOL 1 
#endif

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

#define BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS)                                                              \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_VERTEX_BUFFER    ? D3D11_MAP_WRITE_NO_OVERWRITE_VB :      \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_INDEX_BUFFER     ? D3D11_MAP_WRITE_NO_OVERWRITE_IB :      \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_CONSTANT_BUFFER  ? D3D11_MAP_WRITE_NO_OVERWRITE_CB :      \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_SHADER_RESOURCE  ? D3D11_MAP_WRITE_NO_OVERWRITE_SR :      \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_UNORDERED_ACCESS ? D3D11_MAP_WRITE_NO_OVERWRITE_UA : D3D11_MAP_WRITE_NO_OVERWRITE)))))

#define BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS)                                                             \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_VERTEX_BUFFER    ? D3D11_MAP_WRITE_DISCARD_VB :           \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_INDEX_BUFFER     ? D3D11_MAP_WRITE_DISCARD_IB :           \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_CONSTANT_BUFFER  ? D3D11_MAP_WRITE_DISCARD_CB :           \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_SHADER_RESOURCE  ? D3D11_MAP_WRITE_DISCARD_SR :           \
      (BIND_FLAGS == CDeviceObjectFactory::BIND_UNORDERED_ACCESS ? D3D11_MAP_WRITE_DISCARD_UA : D3D11_MAP_WRITE_DISCARD)))))

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
	CDeviceBuffer* buffer;
	void*          base_ptr;
	buffer_size_t  offset;
	buffer_size_t  size;

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

//===============================================================================
struct SPoolConfig
{
	enum
	{
		POOL_STAGING_COUNT       = 1,
		POOL_ALIGNMENT           = 128,
		POOL_FRAME_QUERY_COUNT   = MAX_FRAMES_IN_FLIGHT,
		POOL_BANK_GRANULARITY    = 1 << 20,
#if CRY_PLATFORM_ORBIS
		POOL_MAX_ALLOCATION_SIZE =  4 * POOL_BANK_GRANULARITY,
#else
		POOL_MAX_ALLOCATION_SIZE = 64 * POOL_BANK_GRANULARITY,
#endif
		POOL_FRAME_QUERY_MASK    = POOL_FRAME_QUERY_COUNT - 1
	};

	buffer_size_t m_pool_bank_size;
	buffer_size_t m_transient_pool_size;
	buffer_size_t m_cb_bank_size;
	buffer_size_t m_cb_threshold;
	buffer_size_t m_pool_bank_mask;
	size_t m_pool_max_allocs;
	size_t m_pool_max_moves_per_update;
	bool   m_pool_defrag_static;
	bool   m_pool_defrag_dynamic;

	bool   Configure()
	{
		m_pool_bank_size      = buffer_size_t(NextPower2(gRenDev->CV_r_buffer_banksize         )) * POOL_BANK_GRANULARITY;
		m_transient_pool_size = buffer_size_t(NextPower2(gRenDev->CV_r_transient_pool_size     )) * POOL_BANK_GRANULARITY;
		m_cb_bank_size        = buffer_size_t(NextPower2(gRenDev->CV_r_constantbuffer_banksize )) * POOL_BANK_GRANULARITY;
		m_cb_threshold        = buffer_size_t(NextPower2(gRenDev->CV_r_constantbuffer_watermark)) * POOL_BANK_GRANULARITY;

		m_pool_bank_mask      = m_pool_bank_size - 1;
		m_pool_max_allocs     = gRenDev->CV_r_buffer_pool_max_allocs;
		m_pool_defrag_static  = gRenDev->CV_r_buffer_pool_defrag_static != 0;
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
	return gRenDev->m_pRT->IsRenderThread() ? gRenDev->GetRenderThreadID() : gRenDev->GetMainThreadID();
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
	CDeviceBuffer* m_buffer;

	// Base pointer to buffer (used on platforms with unified memory)
	uint8* m_base_ptr;

	// Size of the backing buffer
	buffer_size_t m_capacity;

	// Number of allocated bytes from within the buffer
	buffer_size_t m_free_space;

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
	CDeviceBuffer* m_buffer;

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
	buffer_size_t m_size;

	// The offset in bytes from the start of the buffer
	buffer_size_t m_offset;

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

	// Set to 0 if not R/W locked, 1 for READ, 2 for WRITE and 3 for READ & WRITE
	uint8 m_marked : 2;

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
		, m_marked(0)
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
	enum { WRITE = 0, READ = 1, NUM = 2 };

	CDeviceBuffer* m_staging_buffers[NUM];
	buffer_size_t  m_staged_base;
	buffer_size_t  m_staged_size;
	buffer_size_t  m_staged_offset;
	bool           m_staged_open[NUM];

	SStagingResources() { memset(this, 0x0, sizeof(*this)); }
};

//////////////////////////////////////////////////////////////////////////
// Does nothing - but still useful
//
template<size_t BIND_FLAGS, size_t USAGE_FLAGS>
class CDefaultUpdater
{
protected:

public:
	CDefaultUpdater() {}
	CDefaultUpdater(SStagingResources&) {}

	bool  CreateResources()                                         { return true;  }
	bool  FreeResources()                                           { return true;  }

	void* BeginRead   (CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset) { CryFatalError("Attempting to use unimplemented CDefaultUpdater"); return nullptr; }
	void* BeginWrite  (CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset) { CryFatalError("Attempting to use unimplemented CDefaultUpdater"); return nullptr; }
	void  EndReadWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset) { CryFatalError("Attempting to use unimplemented CDefaultUpdater"); }

	void  RegisterBank(SBufferPoolBank* bank)                       {}
	void  UnregisterBank(SBufferPoolBank* bank)                     {}

	void  Read (CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset,       void* dst) { CryFatalError("Attempting to use unimplemented CDefaultUpdater"); }
	void  Write(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset, const void* src) { CryFatalError("Attempting to use unimplemented CDefaultUpdater"); }

	void  Move(
		CDeviceBuffer* dst_buffer
	  , buffer_size_t dst_size
	  , buffer_size_t dst_offset
	  , CDeviceBuffer* src_buffer
	  , buffer_size_t src_size
	  , buffer_size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if (CRY_RENDERER_DIRECT3D < 111)
		CryFatalError("Defragmentation not stageable on directx 11.0! Turn off the defragmenter.");
#endif

		const SResourceRegionMapping mapping =
		{
			{ src_offset, 0, 0, 0 }, // src position
			{ dst_offset, 0, 0, 0 }, // dst position
			{ src_size  , 1, 1, 1 }  // size
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			src_buffer,
			dst_buffer,
			mapping);
	}
};

//////////////////////////////////////////////////////////////////////////
// Performs buffer updates over staging buffers, never blocks but transitions to next free fragment or discards
//
template<size_t BIND_FLAGS, size_t USAGE_FLAGS>
class CTransientStagingBufferUpdater : public CDefaultUpdater<BIND_FLAGS, USAGE_FLAGS>
{
	SStagingResources& m_resources;

public:

	CTransientStagingBufferUpdater(SStagingResources& resources)
		: m_resources(resources)
	{}
	~CTransientStagingBufferUpdater() {}

	// Create the staging buffers if supported && enabled
	bool CreateResources()
	{
		MEMORY_SCOPE_CHECK_HEAP();

		const SBufferLayout LayoutW =
		{
			DXGI_FORMAT_UNKNOWN, s_PoolConfig.m_pool_bank_size, 1,
			CDeviceObjectFactory::USAGE_HIFREQ_HEAP |
#if (CRY_RENDERER_DIRECT3D >= 120)
			CDeviceObjectFactory::USAGE_CPU_WRITE /* BIND_FLAGS */ // NOTE: this is a staging buffer, no bind-flags are allowed
#else
			CDeviceObjectFactory::USAGE_CPU_WRITE  | BIND_FLAGS    // NOTE: this is for CPU-to-GPU uploads
#endif
		};

		const SBufferLayout LayoutR =
		{
			DXGI_FORMAT_UNKNOWN, s_PoolConfig.m_pool_bank_size, 1,
			CDeviceObjectFactory::USAGE_HIFREQ_HEAP |
#if (CRY_RENDERER_DIRECT3D >= 120)
			CDeviceObjectFactory::USAGE_CPU_READ /* BIND_FLAGS */ // NOTE: this is a staging buffer, no bind-flags are allowed
#else
			CDeviceObjectFactory::USAGE_CPU_READ  | BIND_FLAGS    // NOTE: this is for CPU-to-GPU uploads
#endif
		};

		if (!(m_resources.m_staging_buffers[SStagingResources::WRITE] = CDeviceBuffer::Create(LayoutW, nullptr)) ||
			!(m_resources.m_staging_buffers[SStagingResources::READ ] = CDeviceBuffer::Create(LayoutR, nullptr)))
		{
			CryLogAlways("CTransientStagingBufferPool::CreateResources: could not create staging buffer");
			FreeResources();
			return false;
		}

		return true;
	}

	bool FreeResources()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		for (size_t i = 0; i < SStagingResources::NUM; ++i)
		{
			SAFE_RELEASE(m_resources.m_staging_buffers[i]);
			m_resources.m_staged_open[i] = false;
		}
		m_resources.m_staged_base = 0;
		m_resources.m_staged_size = 0;
		m_resources.m_staged_offset = 0;
		return true;
	}

	void* BeginRead(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_ASSERT(m_resources.m_staged_open[SStagingResources::READ] == false);

		const SResourceRegionMapping mapping =
		{
			{ offset, 0, 0, 0 }, // src position
			{ 0     , 0, 0, 0 }, // dst position
			{ size  , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		// NOTE: this Copy() needs to be synchronous because the Map() follows immediately
		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			buffer->GetBuffer(),
			m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(),
			mapping);

		m_resources.m_staged_open[SStagingResources::READ] = true;

		return CDeviceObjectFactory::Map(m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(), 0, 0, size, D3D11_MAP_READ);
	}

	void* BeginWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);

		D3D11_MAP map = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
		if (m_resources.m_staged_base + size > s_PoolConfig.m_pool_bank_size)
		{
			map = BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS);
			m_resources.m_staged_base = 0;

			if (m_resources.m_staging_buffers[SStagingResources::WRITE]->SubstituteUsedResource() != CDeviceResource::eSubResult_Failed)
				map = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
		}

		m_resources.m_staged_open[SStagingResources::WRITE] = true;

		return CDeviceObjectFactory::Map(m_resources.m_staging_buffers[SStagingResources::WRITE]->GetBuffer(), 0, m_resources.m_staged_base, 0, map) + m_resources.m_staged_base;
	}

	void EndReadWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		if (m_resources.m_staged_open[SStagingResources::READ])
		{
			m_resources.m_staged_open[SStagingResources::READ] = false;

			CDeviceObjectFactory::Unmap(m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(), 0, 0, 0, D3D11_MAP_READ);
		}
		if (m_resources.m_staged_open[SStagingResources::WRITE])
		{
			m_resources.m_staged_open[SStagingResources::WRITE] = false;

			CDeviceObjectFactory::Unmap(m_resources.m_staging_buffers[SStagingResources::WRITE]->GetBuffer(), 0, m_resources.m_staged_base, size, BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS));

			DEVBUFFERMAN_ASSERT(buffer);

			const SResourceRegionMapping mapping =
			{
				{ m_resources.m_staged_base, 0, 0, 0 }, // src position
				{ offset                   , 0, 0, 0 }, // dst position
				{ size                     , 1, 1, 1 }, // size
				D3D11_COPY_NO_OVERWRITE_REVERT
			};

			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
				m_resources.m_staging_buffers[SStagingResources::WRITE]->GetBuffer(),
				buffer->GetBuffer(),
				mapping);

			m_resources.m_staged_base += size;
		}
	}

	void Read(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset, void* dst)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_ASSERT(m_resources.m_staged_open[SStagingResources::READ] == false);

		const SResourceRegionMapping mapping =
		{
			{ offset, 0, 0, 0 }, // src position
			{ 0     , 0, 0, 0 }, // dst position
			{ size  , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		// NOTE: this Copy() needs to be synchronous because the DownloadContents() follows immediately
		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			buffer->GetBuffer(),
			m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(),
			mapping);

#if CRY_RENDERER_VULKAN
		GetDeviceObjectFactory().GetVKScheduler()->Flush(true); // cpu-gpu synchronization must happen externally in vulkan renderer
#endif

		CDeviceObjectFactory::DownloadContents<false>(m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(), 0, 0, size, D3D11_MAP_READ, dst);
	}

	void Write(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset, const void* src)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);

		D3D11_MAP map = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
		if (m_resources.m_staged_base + size > s_PoolConfig.m_pool_bank_size)
		{
			map = BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS);
			m_resources.m_staged_base = 0;

			if (m_resources.m_staging_buffers[SStagingResources::WRITE]->SubstituteUsedResource() != CDeviceResource::eSubResult_Failed)
				map = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
		}

		CDeviceObjectFactory::UploadContents<false>(m_resources.m_staging_buffers[SStagingResources::WRITE]->GetBuffer(), 0, m_resources.m_staged_base, size, map, src);

		const SResourceRegionMapping mapping =
		{
			{ m_resources.m_staged_base, 0, 0, 0 }, // src position
			{ offset                   , 0, 0, 0 }, // dst position
			{ size                     , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			m_resources.m_staging_buffers[SStagingResources::WRITE]->GetBuffer(),
			buffer->GetBuffer(),
			mapping);

		m_resources.m_staged_base += size;
	}

	void Move(
		CDeviceBuffer* dst_buffer
	  , buffer_size_t dst_size
	  , buffer_size_t dst_offset
	  , CDeviceBuffer* src_buffer
	  , buffer_size_t src_size
	  , buffer_size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if (CRY_RENDERER_DIRECT3D < 111)
		CryFatalError("Defragmentation not stageable on directx 11.0! Turn off the defragmenter.");
#endif

		const SResourceRegionMapping mapping =
		{
			{ src_offset, 0, 0, 0 }, // src position
			{ dst_offset, 0, 0, 0 }, // dst position
			{ src_size  , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			src_buffer,
			dst_buffer,
			mapping);
	}
};

//////////////////////////////////////////////////////////////////////////
// Performs buffer updates over staging buffers, blocks when staging buffer is still to be copied
//
template<size_t BIND_FLAGS, size_t USAGE_FLAGS>
class CStagingBufferUpdater : public CDefaultUpdater<BIND_FLAGS, USAGE_FLAGS>
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

		const SBufferLayout LayoutW =
		{
			DXGI_FORMAT_UNKNOWN, s_PoolConfig.m_pool_bank_size, 1,
			CDeviceObjectFactory::USAGE_HIFREQ_HEAP |
#if (CRY_RENDERER_DIRECT3D >= 120)
			CDeviceObjectFactory::USAGE_CPU_WRITE /* BIND_FLAGS */ // NOTE: this is a staging buffer, no bind-flags are allowed
#else
			CDeviceObjectFactory::USAGE_CPU_WRITE  | BIND_FLAGS    // NOTE: this is for CPU-to-GPU uploads
#endif
		};

		const SBufferLayout LayoutR =
		{
			DXGI_FORMAT_UNKNOWN, s_PoolConfig.m_pool_bank_size, 1,
			CDeviceObjectFactory::USAGE_HIFREQ_HEAP |
#if (CRY_RENDERER_DIRECT3D >= 120)
			CDeviceObjectFactory::USAGE_CPU_READ /* BIND_FLAGS */ // NOTE: this is a staging buffer, no bind-flags are allowed
#else
			CDeviceObjectFactory::USAGE_CPU_READ  | BIND_FLAGS    // NOTE: this is for CPU-to-GPU uploads
#endif
		};

		if (!(m_resources.m_staging_buffers[SStagingResources::WRITE] = CDeviceBuffer::Create(LayoutW, nullptr)) ||
			!(m_resources.m_staging_buffers[SStagingResources::READ ] = CDeviceBuffer::Create(LayoutR, nullptr)))
		{
			CryLogAlways("SStaticBufferPool::CreateResources: could not create staging buffer");
			FreeResources();
			return false;
		}

		return true;
	}

	bool FreeResources()
	{
		MEMORY_SCOPE_CHECK_HEAP();
		for (size_t i = 0; i < SStagingResources::NUM; ++i)
		{
			SAFE_RELEASE(m_resources.m_staging_buffers[i]);
			m_resources.m_staged_open[i] = false;
		}
		m_resources.m_staged_base = 0;
		m_resources.m_staged_size = 0;
		m_resources.m_staged_offset = 0;
		m_resources.m_staged_open[SStagingResources::WRITE] = true;
		return true;
	}

	void* BeginRead(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size && offset);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_ASSERT(m_resources.m_staged_open[SStagingResources::READ] == false);

		const SResourceRegionMapping mapping =
		{
			{ offset, 0, 0, 0 }, // src position
			{ 0     , 0, 0, 0 }, // dst position
			{ size  , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		// NOTE: this Copy() needs to be synchronous because the Map() follows immediately
		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			buffer,
			m_resources.m_staging_buffers[SStagingResources::READ],
			mapping);

		m_resources.m_staged_open[SStagingResources::READ] = true;

		return CDeviceObjectFactory::Map(m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(), 0, 0, size, D3D11_MAP_READ);
	}

	void* BeginWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_ASSERT(m_resources.m_staged_open[SStagingResources::WRITE] == false);

		m_resources.m_staged_open[SStagingResources::WRITE] = true;

		return CDeviceObjectFactory::Map(m_resources.m_staging_buffers[SStagingResources::WRITE]->GetBuffer(), 0, 0, 0, D3D11_MAP_WRITE);
	}

	void EndReadWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		if (m_resources.m_staged_open[SStagingResources::READ])
		{
			m_resources.m_staged_open[SStagingResources::READ] = false;

			CDeviceObjectFactory::Unmap(m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(), 0, 0, 0, D3D11_MAP_READ);
		}
		if (m_resources.m_staged_open[SStagingResources::WRITE])
		{
			m_resources.m_staged_open[SStagingResources::WRITE] = false;

			CDeviceObjectFactory::Unmap(m_resources.m_staging_buffers[SStagingResources::WRITE]->GetBuffer(), 0, 0, size, D3D11_MAP_WRITE);

			DEVBUFFERMAN_ASSERT(buffer);

			const SResourceRegionMapping mapping =
			{
				{ 0     , 0, 0, 0 }, // src position
				{ offset, 0, 0, 0 }, // dst position
				{ size  , 1, 1, 1 }, // size
				D3D11_COPY_NO_OVERWRITE_REVERT
			};

			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
				m_resources.m_staging_buffers[SStagingResources::WRITE],
				buffer,
				mapping);
		}
	}

	void Read(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset, void* dst)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size && offset);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_ASSERT(m_resources.m_staged_open[SStagingResources::READ] == false);

		const SResourceRegionMapping mapping =
		{
			{ offset, 0, 0, 0 }, // src position
			{ 0     , 0, 0, 0 }, // dst position
			{ size  , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		// NOTE: this Copy() needs to be synchronous because the DownloadContents() follows immediately
		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			buffer,
			m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(),
			mapping);

#if CRY_RENDERER_VULKAN
		GetDeviceObjectFactory().GetVKScheduler()->Flush(true); // cpu-gpu synchronization must happen externally in vulkan renderer
#endif

		CDeviceObjectFactory::DownloadContents<false>(m_resources.m_staging_buffers[SStagingResources::READ]->GetBuffer(), 0, 0, size, D3D11_MAP_READ, dst);
	}

	void Write(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset, const void* src)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		DEVBUFFERMAN_ASSERT(size <= s_PoolConfig.m_pool_bank_size);
		DEVBUFFERMAN_ASSERT(m_resources.m_staged_open[SStagingResources::WRITE] == false);

		CDeviceObjectFactory::UploadContents<false>(m_resources.m_staging_buffers[SStagingResources::WRITE]->GetBuffer(), 0, 0, size, D3D11_MAP_WRITE, src);

		const SResourceRegionMapping mapping =
		{
			{ 0     , 0, 0, 0 }, // src position
			{ offset, 0, 0, 0 }, // dst position
			{ size  , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			m_resources.m_staging_buffers[SStagingResources::WRITE],
			buffer,
			mapping);
	}

	void Move(
		CDeviceBuffer* dst_buffer
	  , buffer_size_t dst_size
	  , buffer_size_t dst_offset
	  , CDeviceBuffer* src_buffer
	  , buffer_size_t src_size
	  , buffer_size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if (CRY_RENDERER_DIRECT3D < 111)
		CryFatalError("Defragmentation not stageable on directx 11.0! Turn off the defragmenter.");
#endif

		const SResourceRegionMapping mapping =
		{
			{ src_offset, 0, 0, 0 }, // src position
			{ dst_offset, 0, 0, 0 }, // dst position
			{ src_size  , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			src_buffer,
			dst_buffer,
			mapping);
	}
};

//////////////////////////////////////////////////////////////////////////
// Performs buffer updates over dynamic updates
//
template<size_t BIND_FLAGS, size_t USAGE_FLAGS>
class CDynamicBufferUpdater : public CDefaultUpdater<BIND_FLAGS, USAGE_FLAGS>
{

public:

	CDynamicBufferUpdater(SStagingResources& resources)
		: CDefaultUpdater<BIND_FLAGS, USAGE_FLAGS>(resources)
	{}

	void* BeginWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		return CDeviceObjectFactory::Map(buffer->GetBuffer(), 0, offset, 0, BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS)) + offset;
	}

	void EndReadWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		DEVBUFFERMAN_ASSERT(buffer && size);
		CDeviceObjectFactory::Unmap(buffer->GetBuffer(), 0, offset, size, BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS));
	}

	void Write(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset, const void* src)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);
		CDeviceObjectFactory::UploadContents<false>(buffer->GetBuffer(), 0, offset, size, BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS), src);
	}

	void Move(
		CDeviceBuffer* dst_buffer
	  , buffer_size_t dst_size
	  , buffer_size_t dst_offset
	  , CDeviceBuffer* src_buffer
	  , buffer_size_t src_size
	  , buffer_size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);
#if (CRY_RENDERER_DIRECT3D < 111)
		CryFatalError("Defragmentation not stageable on directx 11.0! Turn off the defragmenter.");
#endif

		const SResourceRegionMapping mapping =
		{
			{ src_offset, 0, 0, 0 }, // src position
			{ dst_offset, 0, 0, 0 }, // dst position
			{ src_size  , 1, 1, 1 }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			src_buffer,
			dst_buffer,
			mapping);
	}
};

//////////////////////////////////////////////////////////////////////////
// Dummy class used when buffers are direct access and no staging is needed
//
template<size_t BIND_FLAGS, size_t USAGE_FLAGS>
class CDirectBufferUpdater : public CDefaultUpdater<BIND_FLAGS, USAGE_FLAGS>
{

public:

	CDirectBufferUpdater(SStagingResources& resources)
		: CDefaultUpdater<BIND_FLAGS, USAGE_FLAGS>(resources)
	{}

#if BUFFER_ENABLE_DIRECT_ACCESS
	void* BeginRead(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);

		uint8* base_ptr = nullptr;
		CDeviceObjectFactory::ExtractBasePointer(buffer->GetBuffer(), D3D11_MAP_READ, base_ptr);
		return base_ptr + offset;
	}

	void* BeginWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);

		uint8* base_ptr = nullptr;
		CDeviceObjectFactory::ExtractBasePointer(buffer->GetBuffer(), D3D11_MAP_WRITE_NO_OVERWRITE, base_ptr);
		return base_ptr + offset;
	}

	void EndReadWrite(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset)
	{
		DEVBUFFERMAN_ASSERT(buffer && size);
		CDeviceObjectFactory::ReleaseBasePointer(buffer->GetBuffer());

		// NOTE: this is only true for writes, reads would need a flush in BeginRead() above
		if ((USAGE_FLAGS & (CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT |
			                CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT)) != 
			               (CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT |
			                CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT))
		{
			uint8* base_ptr = nullptr;
			CDeviceObjectFactory::ExtractBasePointer(buffer->GetBuffer(), D3D11_MAP_WRITE_NO_OVERWRITE, base_ptr);

			if ((USAGE_FLAGS & CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT) == 0)
			{
				STATOSCOPE_TIMER(GetStatoscopeData(0).m_cpu_flush_time);
				CDeviceObjectFactory::InvalidateCpuCache(
					base_ptr, size, offset);
			}
			if ((USAGE_FLAGS & CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT) == 0)
			{
				GetDeviceObjectFactory().InvalidateBuffer(
					buffer->GetBuffer()
					, base_ptr
					, offset
					, size
					, _GetThreadID());
			}

			CDeviceObjectFactory::ReleaseBasePointer(buffer->GetBuffer());
		}
	}

	void Read(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset, void* dst)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);

		void* base_ptr = BeginRead(buffer, size, offset);
		StreamBufferData(dst, base_ptr, size);
		EndReadWrite(buffer, size, offset);

		// if MarkRead/WriteRange would work this would be the better implementation
		//	CDeviceObjectFactory::DownloadContents<true>(buffer->GetBuffer(), 0, offset, size, D3D11_MAP_READ, dst, base_ptr + offset);
	}

	void Write(CDeviceBuffer* buffer, buffer_size_t size, buffer_size_t offset, const void* src)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(buffer && size);

		void* base_ptr = BeginWrite(buffer, size, offset);
		StreamBufferData(base_ptr, src, size);
		EndReadWrite(buffer, size, offset);

		// if MarkRead/WriteRange would work this would be the better implementation
		//	CDeviceObjectFactory::UploadContents<true>(buffer->GetBuffer(), 0, offset, size, BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS), src, base_ptr + offset);
	}

	void Move(
		CDeviceBuffer* dst_buffer
		, buffer_size_t dst_size
		, buffer_size_t dst_offset
		, CDeviceBuffer* src_buffer
		, buffer_size_t src_size
		, buffer_size_t src_offset)
	{
		DEVBUFFERMAN_ASSERT(dst_buffer && src_buffer && dst_size == src_size);

		// NOTE: if the source in in WC memory this would be bad and the GPU-side copy should be used
		// (async gpu + wait, otherwise the whole command-queue needs to be flushed and processed through)
		void* src_ptr = BeginRead (src_buffer, src_size, src_offset);
		void* dst_ptr = BeginWrite(dst_buffer, dst_size, dst_offset);
		StreamBufferData(dst_ptr, src_ptr, src_size);
		EndReadWrite(src_buffer, src_size, src_offset);
		EndReadWrite(dst_buffer, dst_size, dst_offset);
	}
#endif
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

	~CDynamicDefragAllocator() { DEVBUFFERMAN_ASSERT(m_defrag_allocator == NULL); }

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

	item_handle_t Allocate(buffer_size_t size, SBufferPoolItem*& item)
	{
		CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT(size);
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
		CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
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
		UINT_PTR temp = m_defrag_allocator->Pin(item->m_defrag_handle) & s_PoolConfig.m_pool_bank_mask;
		DEVBUFFERMAN_ASSERT(temp == item->m_offset);
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
	CDeviceBuffer*      m_buffer;
	void*               m_base_ptr;
	uint32              m_page_size;
	uint32              m_bucket_size;
	uint32              m_partition;
	uint32              m_capacity;

#if (CRY_RENDERER_DIRECT3D >= 111) && (CRY_RENDERER_DIRECT3D < 120)
	uint32              m_map_count = 0;
#endif

	std::vector<uint32> m_table;
	std::vector<uint32> m_remap;

	CPartitionAllocator(
		CDeviceBuffer* buffer, void* base_ptr, size_t page_size, size_t bucket_size)
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
		DEVBUFFERMAN_ASSERT(m_partition == 0);
		SAFE_RELEASE(m_buffer);
	}

	CDeviceBuffer* buffer() const   { return m_buffer; }
	void*          base_ptr() const { return m_base_ptr; }
	bool           empty() const    { return m_partition == 0; }

	uint32 allocate()
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

#if (CRY_RENDERER_DIRECT3D >= 111) && (CRY_RENDERER_DIRECT3D < 120)
	void map()
	{
		if (0 == m_map_count++)
		{
			CRY_ASSERT(m_base_ptr == nullptr);
			m_base_ptr = CDeviceObjectFactory::Map(m_buffer->GetBuffer(), 0, 0, m_page_size, D3D11_MAP_WRITE_NO_OVERWRITE_CB);
		}
	}

	void unmap()
	{
		if (--m_map_count == 0)
		{
			CRY_ASSERT(m_base_ptr);
			CDeviceObjectFactory::Unmap(m_buffer->GetBuffer(), 0, 0, m_page_size, D3D11_MAP_WRITE_NO_OVERWRITE_CB);
			m_base_ptr = nullptr;
		}
	}
#endif

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
		FUNCTION_PROFILER_RENDERER();
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
			if (m_fences[idx] && GetDeviceObjectFactory().SyncFence(m_fences[idx], false, false) == S_OK)
			{
				for (auto& slot : m_retired_slots[idx])
					slot.first->deallocate(slot.second);

				m_retired_slots[idx].clear();
			}
		}
	}

	bool Allocate(CConstantBuffer* cbuffer)
	{
		CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
		CRY_ASSERT(cbuffer && cbuffer->m_size && "Bad allocation request");
		const unsigned size = cbuffer->m_size;
		const unsigned nsize = NextPower2(size);
		const unsigned bucket = IntegerLog2(nsize) - 8;
		CRY_ASSERT(bucket < CRY_ARRAY_COUNT(m_page_buckets) && "Bad allocation size");
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
			++m_pages;

			const SBufferLayout Layout =
			{
				DXGI_FORMAT_UNKNOWN, s_PoolConfig.m_cb_bank_size, 1,
				CDeviceObjectFactory::USAGE_HIFREQ_HEAP |
#if (CRY_PLATFORM_CONSOLE) // UMA
				CDeviceObjectFactory::USAGE_DIRECT_ACCESS |
				CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT |
				CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT |
#endif
#if (CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS == 1) // UMA || PersistentMap
				CDeviceObjectFactory::USAGE_CPU_WRITE |
#endif
				CDeviceObjectFactory::BIND_CONSTANT_BUFFER
			};

			CDeviceBuffer* buffer;
			if (!(buffer = CDeviceBuffer::Create(Layout, nullptr)))
			{
				CryLogAlways("Failed to create constant buffer pool");
				return false;
			}

#ifdef CRY_RENDERER_VULKAN
			auto setLayout = GetDeviceObjectFactory().GetInlineConstantBufferLayout();
			buffer->GetBaseBuffer()->AsDynamicOffsetBuffer()->CreateDynamicDescriptorSet(setLayout, nsize);
#endif

			uint8* base_ptr;
			CDeviceObjectFactory::ExtractBasePointer(buffer->GetBuffer(), D3D11_MAP_WRITE_NO_OVERWRITE, base_ptr);

			m_page_buckets[bucket].push_back(new CPartitionAllocator(buffer, base_ptr, s_PoolConfig.m_cb_bank_size, nsize));
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

	virtual item_handle_t Allocate(buffer_size_t, bool bUsePool)                                      { return ~0u; }
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
	virtual void          Write(SBufferPoolItem* item, const void* src, buffer_size_t size, buffer_size_t offset) { __debugbreak(); }
	SBufferPoolItem*      Resolve(item_handle_t handle)                                               { return &m_item_table[handle]; }
};

//////////////////////////////////////////////////////////////////////////
// Buffer pool base implementation
template<
  size_t BIND_FLAGS,
  size_t USAGE_FLAGS,
  typename Allocator,
  template<size_t,size_t> class Updater,
  size_t ALIGNMENT = SPoolConfig::POOL_ALIGNMENT>
struct CBufferPoolImpl final
	: public SBufferPool
	  , private IDefragAllocatorPolicy
{
	typedef Allocator                       allocator_t;
	typedef Updater<BIND_FLAGS,USAGE_FLAGS> updater_t;

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

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	// The list of moves we need to perform
	struct SPendingMove
	{
		IDefragAllocatorCopyNotification* m_notification;
		item_handle_t                     m_item_handle;
		buffer_size_t                     m_src_offset;
		buffer_size_t                     m_dst_offset;
		buffer_size_t                     m_size;
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
			if (m_copy_fence) GetDeviceObjectFactory().ReleaseFence(m_copy_fence);
			if (m_relocate_fence) GetDeviceObjectFactory().ReleaseFence(m_relocate_fence);
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
			if (GetDeviceObjectFactory().SyncFence(move.m_copy_fence, block, block) == S_OK)
			{
				move.m_notification->bDstIsValid = true;
				move.m_moving = false;
			}
		}
		// Only finish the relocation by informing the defragger if the gpu has caught up to the
		// point where the new destination has been considered valid
		else if (move.m_relocating)
		{
			if (GetDeviceObjectFactory().SyncFence(move.m_relocate_fence, block, block) == S_OK)
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
			buffer_size_t nDecOffs = move.m_canceled && !move.m_relocated
			                    ? move.m_dst_offset
			                    : move.m_src_offset;

			{
				uint32 nSrcBank = nDecOffs / s_PoolConfig.m_pool_bank_size;
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
		FUNCTION_PROFILER_RENDERER();
		// Allocate a new bank
		size_t bank_index = ~0u;
		CDeviceBuffer* buffer;
		SBufferPoolBank* bank = NULL;
		{
			DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

			const SBufferLayout Layout =
			{
				DXGI_FORMAT_UNKNOWN, s_PoolConfig.m_pool_bank_size, 1,
				CDeviceObjectFactory::USAGE_HIFREQ_HEAP |
#if !BUFFER_USE_STAGED_UPDATES && (CRY_PLATFORM_CONSOLE) // UMA
				CDeviceObjectFactory::USAGE_DIRECT_ACCESS |
#endif
#if !BUFFER_USE_STAGED_UPDATES && (BUFFER_ENABLE_DIRECT_ACCESS == 1) // UMA || PersistentMap
				CDeviceObjectFactory::USAGE_CPU_WRITE |
#endif
				USAGE_FLAGS | BIND_FLAGS
			};

			if (!(buffer = CDeviceBuffer::Create(Layout, nullptr)))
			{
				CryLogAlways("SBufferPoolImpl::Allocate: could not allocate additional bank of size %" PRISIZE_T, s_PoolConfig.m_pool_bank_size);
				return nullptr;
			}
		}
		bank = &m_bank_table[bank_index = m_bank_table.Allocate()];
		bank->m_buffer = buffer;
		bank->m_capacity = s_PoolConfig.m_pool_bank_size;
		bank->m_free_space = s_PoolConfig.m_pool_bank_size;
#if !BUFFER_USE_STAGED_UPDATES
		CDeviceObjectFactory::ExtractBasePointer(buffer->GetBuffer(), D3D11_MAP_WRITE_NO_OVERWRITE, bank->m_base_ptr);
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
		FUNCTION_PROFILER_RENDERER();
		{
			DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

			const SBufferLayout Layout =
			{
				DXGI_FORMAT_UNKNOWN, s_PoolConfig.m_pool_bank_size, 1,
				CDeviceObjectFactory::USAGE_HIFREQ_HEAP |
#if !BUFFER_USE_STAGED_UPDATES && (CRY_PLATFORM_CONSOLE) // UMA
				CDeviceObjectFactory::USAGE_DIRECT_ACCESS |
#endif
#if !BUFFER_USE_STAGED_UPDATES && (BUFFER_ENABLE_DIRECT_ACCESS == 1) // UMA || PersistentMap
				CDeviceObjectFactory::USAGE_CPU_WRITE |
#endif
				USAGE_FLAGS | BIND_FLAGS
			};

			if (!(bank->m_buffer = CDeviceBuffer::Create(Layout, nullptr)))
			{
				CryLogAlways("SBufferPoolImpl::Allocate: could not re-allocate freed bank of size %" PRISIZE_T, s_PoolConfig.m_pool_bank_size);
				return false;
			}
		}
#if !BUFFER_USE_STAGED_UPDATES
		CDeviceObjectFactory::ExtractBasePointer(bank->m_buffer->GetBuffer(), D3D11_MAP_WRITE_NO_OVERWRITE, bank->m_base_ptr);
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
#if !BUFFER_USE_STAGED_UPDATES
			if (bank.m_buffer)
				CDeviceObjectFactory::ReleaseBasePointer(bank.m_buffer->GetBuffer());
#endif
			SAFE_DELETE(bank.m_buffer);
			bank.m_base_ptr = nullptr;
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
	  , IDefragAllocatorCopyNotification* pNotification) final
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
		IF (bank->m_buffer == nullptr, 0)
		{
			if (RecreateBank(bank) == false)
			{
				CryLogAlways("SBufferPoolImpl::Allocate: could not re-allocate freed bank of size %" PRISIZE_T, s_PoolConfig.m_pool_bank_size);
				return 0;
			}
		}

		DEVBUFFERMAN_ASSERT(size      <= UINT_PTR(~buffer_size_t(0)));
		DEVBUFFERMAN_ASSERT(srcOffset <= UINT_PTR(~buffer_size_t(0)));
		DEVBUFFERMAN_ASSERT(dstOffset <= UINT_PTR(~buffer_size_t(0)));

		bank->m_free_space -= buffer_size_t(size);

		SPendingMove& pending = m_pending_moves[pm];
		pending.m_notification = pNotification;
		pending.m_item_handle = handle;
		pending.m_src_offset = buffer_size_t(srcOffset);
		pending.m_dst_offset = buffer_size_t(dstOffset);
		pending.m_size       = buffer_size_t(size);

		// Perform the actual move in (hopefully) hardware
		m_updater.Move(
		  bank->m_buffer
		  , buffer_size_t(size)
		  , dstOffset & s_PoolConfig.m_pool_bank_mask
		  , old_item->m_buffer
		  , old_item->m_size
		  , old_item->m_offset);

		// Issue a fence so that the copy can be synced
		GetDeviceObjectFactory().IssueFence(pending.m_copy_fence);
		pending.m_moving = true;
		// The move will be considered "done" (bDstIsValid) on the next Update call
		// thanks to r_flush being one, this is always true!
		return pm + 1;
	}
	void Relocate(uint32 userMoveId, void* pContext, UINT_PTR newOffset, UINT_PTR oldOffset, UINT_PTR size) final
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
		GetDeviceObjectFactory().IssueFence(move.m_relocate_fence);
		move.m_relocating = true;
		move.m_relocated = true;

		DB_MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
		DB_MEMREPLAY_SCOPE_REALLOC(old_offset, bank->m_base_ptr + item.m_offset, item.m_size, ALIGNMENT);
	}
	void CancelCopy(uint32 userMoveId, void* pContext, bool bSync) final
	{
		// Remove the move from the list of pending moves, free the destination item
		// as it's not going to be used anymore
		SPendingMove& move = m_pending_moves[userMoveId - 1];
		move.m_canceled = true;
	}
	void SyncCopy(void* pContext, UINT_PTR dstOffset, UINT_PTR srcOffset, UINT_PTR size) final { __debugbreak(); }

public:
	CBufferPoolImpl(SStagingResources& resources)
		: m_allocator(m_item_table)
		, m_updater(resources)
		, m_banks()
		, m_current_frame()
		, m_current_fence()
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
	item_handle_t Allocate(buffer_size_t size, bool bUsePool) final
	{
		MEMORY_SCOPE_CHECK_HEAP();
		CDeviceBuffer* buffer = NULL;
		SBufferPoolItem* item = NULL;
		SBufferPoolBank* bank = NULL;
		size_t bank_index = 0u;
		item_handle_t handle;
		bool failed = false;

		// Align the allocation size up to the configured allocation alignment
		size = (max(size, buffer_size_t(1u)) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

		// Handle the case where an allocation cannot be satisfied by a pool bank
		// as the size is too large and create a free standing buffer therefore.
		// Note: Care should be taken to reduce the amount of unpooled items!
		IF (size > s_PoolConfig.m_pool_bank_size || !bUsePool, 0)
		{
freestanding:

			const SBufferLayout Layout =
			{
				DXGI_FORMAT_UNKNOWN, size, 1,
#if !BUFFER_USE_STAGED_UPDATES && (CRY_PLATFORM_CONSOLE) // UMA
				CDeviceObjectFactory::USAGE_DIRECT_ACCESS |
#endif
#if !BUFFER_USE_STAGED_UPDATES && (BUFFER_ENABLE_DIRECT_ACCESS == 1) // UMA || PersistentMap
				CDeviceObjectFactory::USAGE_CPU_WRITE |
#endif
				USAGE_FLAGS | BIND_FLAGS
			};

			if (!(buffer = CDeviceBuffer::Create(Layout, nullptr)))
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
			CDeviceObjectFactory::ExtractBasePointer(buffer->GetBuffer(), D3D11_MAP_WRITE_NO_OVERWRITE, item->m_base_ptr);
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
			IF (bank->m_buffer == nullptr, 0)
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

#if !BUFFER_USE_STAGED_UPDATES
			CDeviceObjectFactory::ReleaseBasePointer(bank->m_buffer->GetBuffer());
#endif
			// Extending the allocator failed, so the newly created bank is rolled back
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
	void Free(SBufferPoolItem* item) final
	{
		DEVBUFFERMAN_ASSERT(item);
		// Handle un pooled buffers
		IF ((item->m_bank) == ~0u, 0)
		{
#if !BUFFER_USE_STAGED_UPDATES
			CDeviceObjectFactory::ReleaseBasePointer(item->m_buffer->GetBuffer());
#endif
			SAFE_RELEASE(item->m_buffer);
			m_item_table.Free(item->m_handle);
			return;
		}
		item->m_deferred_list.relink_tail(m_deferred_items[m_current_frame].m_deleted_items);
	}

	//////////////////////////////////////////////////////////////////////////////////////
	bool CreateResources(bool enable_defragging, bool best_fit) final
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
			if (GetDeviceObjectFactory().CreateFence(m_pending_moves[i].m_copy_fence) != S_OK)
			{
				CryLogAlways("Could not create buffer pool copy gpu fence");
				return false;
			}
			if (GetDeviceObjectFactory().CreateFence(m_pending_moves[i].m_relocate_fence) != S_OK)
			{
				CryLogAlways("Could not create buffer pool relocate fence");
				return false;
			}
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	bool FreeResources() final
	{
		Sync();
		if (m_updater.FreeResources() == false)
			return false;
		if (m_allocator.Shutdown() == false)
			return false;
		for (size_t i = 0, end = m_banks.size(); i < end; ++i)
			m_bank_table.Free((item_handle_t)m_banks[i]);
		stl::free_container(m_banks);
		stl::free_container(m_pending_moves);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	void ReleaseEmptyBanks() final { RetireEmptyBanks(); }

	//////////////////////////////////////////////////////////////////////////////////////
	void Sync() final
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
	void Update(uint32 frame_id, DeviceFenceHandle fence, bool allow_defragmentation) final
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
			if (deferred.m_fence && GetDeviceObjectFactory().SyncFence(deferred.m_fence, false, false) != S_OK)
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
	void* BeginRead(SBufferPoolItem* item) final
	{
		GetDeviceObjectFactory().SyncToGPU();

		DEVBUFFERMAN_ASSERT(item->m_used);
		IF (item->m_bank != ~0u, 1)
			m_allocator.PinItem(item);
#if !BUFFER_USE_STAGED_UPDATES
		IF (item->m_bank != ~0u, 1)
		{
			SBufferPoolBank& bank = m_bank_table[m_banks[item->m_bank]];
			IF(bank.m_base_ptr != NULL && CRenderer::CV_r_buffer_enable_lockless_updates, 1)
			{
				// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
				item->m_marked |= CDeviceObjectFactory::MarkReadRange(item->m_buffer->GetBuffer(), item->m_offset, item->m_size, D3D11_MAP_READ);

				return bank.m_base_ptr + item->m_offset;
			}
		}
#endif
		return m_updater.BeginRead(item->m_buffer, item->m_size, item->m_offset);
	}

private:
	SBufferPoolItem* GetWritableItem(SBufferPoolItem* item)
	{
		// In case item was previously used and the current last fence can not be
		// synced already we allocate a new item and swap it with the existing one
		// to make sure that we do not contend with the gpu on an already
		// used item's buffer update.
		IF(item->m_bank != ~0u && item->m_used /*&& GetDeviceObjectFactory().SyncFence(m_current_fence, false, false) != S_OK*/, 0)
		{
			item_handle_t handle = Allocate(item->m_size, true);
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
		if ((USAGE_FLAGS & CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT) == 0)
		{
			item->m_cpu_flush = 1;
		}
		if ((USAGE_FLAGS & CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT) == 0)
		{
			item->m_gpu_flush = 1;
		}
		return item;
	}

	SBufferPoolItem* ReturnUnwritableItem(SBufferPoolItem* item)
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
		return item;
	}

	void WriteStaged(SBufferPoolItem* item_, const void* src, buffer_size_t size, buffer_size_t offset)
	{
		GetDeviceObjectFactory().SyncToGPU();
		SBufferPoolItem* item = item_;

		IF(item->m_bank != ~0u, 1)
			m_allocator.PinItem(item);

		item = GetWritableItem(item_);

		m_updater.Write(item->m_buffer, size, item->m_offset + offset, src);
		
		item = ReturnUnwritableItem(item_);

		IF(item->m_bank != ~0u, 1)
			m_allocator.UnpinItem(item);

		GetDeviceObjectFactory().SyncToGPU();
	}

#if !BUFFER_USE_STAGED_UPDATES
	void WriteUnstaged(SBufferPoolItem* item_, const void* src, buffer_size_t size, buffer_size_t offset)
	{
		GetDeviceObjectFactory().SyncToGPU();
		SBufferPoolItem* item = item_;
		uint8* item_base_ptr = nullptr;
		const bool requires_flush = false;

		IF(item->m_bank != ~0u, 1)
			m_allocator.PinItem(item);

		item = GetWritableItem(item_);

		IF(item->m_bank != ~0u, 1)
		{
			SBufferPoolBank& bank = m_bank_table[m_banks[item->m_bank]];
			IF(bank.m_base_ptr != NULL && CRenderer::CV_r_buffer_enable_lockless_updates, 1)
			{
				item_base_ptr = bank.m_base_ptr + item->m_offset;
			}
		}

		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::UploadContents<true>(item->m_buffer->GetBuffer(), 0, item->m_offset + offset, size, BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS), src, item_base_ptr);

		item = ReturnUnwritableItem(item_);

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
						CDeviceObjectFactory::InvalidateCpuCache(
							bank->m_base_ptr, item->m_size, item->m_offset);
					}
					item->m_cpu_flush = 0;
				}
				if (item->m_gpu_flush)
				{
					GetDeviceObjectFactory().InvalidateBuffer(
						bank->m_buffer->GetBuffer()
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

		GetDeviceObjectFactory().SyncToGPU();
	}
#endif

public:
	void* BeginWrite(SBufferPoolItem* item) final
	{
		GetDeviceObjectFactory().SyncToGPU();

		IF(item->m_bank != ~0u, 1)
			m_allocator.PinItem(item);

		item = GetWritableItem(item);

#if !BUFFER_USE_STAGED_UPDATES
		IF(item->m_bank != ~0u, 1)
		{
			SBufferPoolBank& bank = m_bank_table[m_banks[item->m_bank]];
			IF(bank.m_base_ptr != NULL && CRenderer::CV_r_buffer_enable_lockless_updates, 1)
			{
				// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
				item->m_marked |= CDeviceObjectFactory::MarkReadRange(item->m_buffer->GetBuffer(), item->m_offset, 0, D3D11_MAP_WRITE);

				return bank.m_base_ptr + item->m_offset;
			}
		}
#endif

		return m_updater.BeginWrite(item->m_buffer, item->m_size, item->m_offset);
	}

	void EndReadWrite(SBufferPoolItem* item, bool requires_flush) final
	{
		item = ReturnUnwritableItem(item);
		IF(item->m_bank != ~0u, 1)
		{
			m_allocator.UnpinItem(item);

#if !BUFFER_USE_STAGED_UPDATES
			SBufferPoolBank* bank = &m_bank_table[m_banks[item->m_bank]];
			IF(bank->m_base_ptr != NULL && CRenderer::CV_r_buffer_enable_lockless_updates, 1)
			{
				// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
				item->m_marked &= ~CDeviceObjectFactory::MarkWriteRange(item->m_buffer->GetBuffer(), item->m_offset, item->m_size, item->m_marked);

	#if BUFFER_ENABLE_DIRECT_ACCESS
				if (item->m_cpu_flush)
				{
					if (requires_flush)
					{
						STATOSCOPE_TIMER(GetStatoscopeData(0).m_cpu_flush_time);
						CDeviceObjectFactory::InvalidateCpuCache(
						  bank->m_base_ptr, item->m_size, item->m_offset);
					}

					item->m_cpu_flush = 0;
				}

				if (item->m_gpu_flush)
				{
					GetDeviceObjectFactory().InvalidateBuffer(
					  bank->m_buffer->GetBuffer()
					  , bank->m_base_ptr
					  , item->m_offset
					  , item->m_size
					  , _GetThreadID());

					item->m_gpu_flush = 0;
				}
	#endif
				return;
			}
#endif
		}

		m_updater.EndReadWrite(item->m_buffer, item->m_size, item->m_offset);

		GetDeviceObjectFactory().SyncToGPU();
	}

	void Write(SBufferPoolItem* item, const void* src, buffer_size_t size, buffer_size_t offset) final
	{
		DEVBUFFERMAN_ASSERT((size + offset) <= item->m_size);

		size = min(item->m_size, size);

		if (item->m_size <= s_PoolConfig.m_pool_bank_size)
		{
#if !BUFFER_USE_STAGED_UPDATES
			WriteUnstaged(item, src, size, offset);
#else
			WriteStaged(item, src, size, offset);
#endif
			return;
		}

		DEVBUFFERMAN_ASSERT(item->m_bank == ~0u);
		DEVBUFFERMAN_ASSERT(item->m_cow_handle == ~0u);

		GetDeviceObjectFactory().SyncToGPU();

		item->m_used = 1u;

		for (buffer_size_t cursor = 0; cursor < size; )
		{
			const buffer_size_t sz = min(size - cursor, s_PoolConfig.m_pool_bank_size);
			const buffer_size_t of = cursor;

			m_updater.Write(item->m_buffer, sz, item->m_offset + offset + of, reinterpret_cast<const uint8*>(src) + of);

			cursor += sz;
		}

		GetDeviceObjectFactory().SyncToGPU();
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
    CDeviceObjectFactory::BIND_VERTEX_BUFFER
    , CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES && BUFFER_SUPPORT_TRANSIENT_POOLS
	, CTransientStagingBufferUpdater
#elif BUFFER_USE_STAGED_UPDATES
    , CStagingBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SStaticBufferPoolVB;
typedef CBufferPoolImpl<
    CDeviceObjectFactory::BIND_INDEX_BUFFER
    , CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES && BUFFER_SUPPORT_TRANSIENT_POOLS
	, CTransientStagingBufferUpdater
#elif BUFFER_USE_STAGED_UPDATES
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
    CDeviceObjectFactory::BIND_VERTEX_BUFFER
    , CDeviceObjectFactory::USAGE_CPU_WRITE
    | CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT
    | CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
	, CDynamicBufferUpdater
#else
	, CDirectBufferUpdater
#endif
    > SDynamicBufferPoolVB;
typedef CBufferPoolImpl<
    CDeviceObjectFactory::BIND_INDEX_BUFFER
    , CDeviceObjectFactory::USAGE_CPU_WRITE
    | CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT
    | CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
	, CDynamicBufferUpdater
#else
	, CDirectBufferUpdater
#endif
    > SDynamicBufferPoolIB;

#if BUFFER_SUPPORT_TRANSIENT_POOLS
template<size_t BIND_FLAGS, buffer_size_t ALIGNMENT = SPoolConfig::POOL_ALIGNMENT>
class CTransientBufferPool final : public SBufferPool
{
	SBufferPoolBank m_backing_buffer;
	buffer_size_t   m_allocation_count;
	D3D11_MAP       m_map_type;

public:
	CTransientBufferPool()
		: m_backing_buffer(~0u)
		, m_allocation_count()
		, m_map_type(BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS))
	{
	}

	item_handle_t Allocate(buffer_size_t size, bool /*bUsePool*/) final
	{
		// Align the allocation size up to the configured allocation alignment
		size = (max(size, buffer_size_t(1u)) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

		DEVBUFFERMAN_ASSERT(size <= m_backing_buffer.m_capacity);

		if (m_backing_buffer.m_free_space + size > m_backing_buffer.m_capacity)
		{
			m_map_type = BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS);
			m_backing_buffer.m_free_space = 0;

			// DISCARD will re-base the resource pointer
			CDeviceObjectFactory::ReleaseBasePointer(m_backing_buffer.m_buffer->GetBuffer());

			if (m_backing_buffer.m_buffer->SubstituteUsedResource() != CDeviceResource::eSubResult_Failed)
				m_map_type = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);

			CDeviceObjectFactory::ExtractBasePointer(m_backing_buffer.m_buffer->GetBuffer(), m_map_type, m_backing_buffer.m_base_ptr);
		}

		SBufferPoolItem* item = &m_item_table[m_item_table.Allocate()];
		item->m_buffer = m_backing_buffer.m_buffer;
		item->m_pool = this;
		item->m_offset = m_backing_buffer.m_free_space;
		item->m_bank = ~0u;
		item->m_size = size;
		item->m_defrag_handle = IDefragAllocator::InvalidHdl;
		item->m_base_ptr = m_backing_buffer.m_base_ptr;

		// Ensure transient range is an aligned range
		assert(!(uintptr_t(item->m_offset  ) & (CRY_PLATFORM_ALIGNMENT - 1ULL)));
		assert(!(uintptr_t(item->m_base_ptr) & (CRY_PLATFORM_ALIGNMENT - 1ULL)));

		m_backing_buffer.m_free_space += size;
		++m_allocation_count;

		return item->m_handle;
	}
	void Free(SBufferPoolItem* item) final
	{
		m_item_table.Free(item->m_handle);
		--m_allocation_count;
	}
	bool CreateResources(bool, bool) final
	{
		const SBufferLayout Layout =
		{
			DXGI_FORMAT_UNKNOWN, s_PoolConfig.m_transient_pool_size, 1,
			CDeviceObjectFactory::USAGE_HIFREQ_HEAP |
			CDeviceObjectFactory::USAGE_CPU_WRITE |
			BIND_FLAGS
		};

		if (!(m_backing_buffer.m_buffer = CDeviceBuffer::Create(Layout, nullptr)))
		{
			CryLogAlways("CTransientBufferPool::CreateResources: could not allocate backing buffer of size %" PRISIZE_T, s_PoolConfig.m_transient_pool_size);
			return false;
		}

		m_backing_buffer.m_capacity = s_PoolConfig.m_transient_pool_size;
		m_backing_buffer.m_free_space = 0;
		m_backing_buffer.m_handle = ~0u;

		CDeviceObjectFactory::ExtractBasePointer(m_backing_buffer.m_buffer->GetBuffer(), m_map_type, m_backing_buffer.m_base_ptr);

		return true;
	}
	bool FreeResources() final
	{
		// NOTE: Context could have been freed by ShutDown()
		if (gcpRendD3D->GetDeviceContext().IsValid())
		{
			CDeviceObjectFactory::ReleaseBasePointer(m_backing_buffer.m_buffer->GetBuffer());
		}

		SAFE_RELEASE(m_backing_buffer.m_buffer);
		m_backing_buffer.m_capacity = 0;
		m_backing_buffer.m_free_space = 0;
		m_backing_buffer.m_handle = ~0u;

		return true;
	}
	bool GetStats(SDeviceBufferPoolStats&) final { return false; }
	bool DebugRender() final                     { return false; }
	void Sync() final                            {}
	void Update(uint32 frameId, DeviceFenceHandle fence, bool allow_defragmentation) final
	{
		if (m_allocation_count)
		{
			CryFatalError(
			  "CTransientBufferPool::Update %" PRISIZE_T " allocations still in transient pool!"
			  , m_allocation_count);
		}

		if (m_backing_buffer.m_free_space > 0)
		{
			m_map_type = BINDFLAGS_to_WRITEDISCARD(BIND_FLAGS);
			m_backing_buffer.m_free_space = 0;

			// DISCARD will re-base the resource pointer
			CDeviceObjectFactory::ReleaseBasePointer(m_backing_buffer.m_buffer->GetBuffer());

			if (m_backing_buffer.m_buffer->SubstituteUsedResource() != CDeviceResource::eSubResult_Failed)
				m_map_type = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);

			CDeviceObjectFactory::ExtractBasePointer(m_backing_buffer.m_buffer->GetBuffer(), m_map_type, m_backing_buffer.m_base_ptr);
		}
	}
	void  ReleaseEmptyBanks() final              {}
	void* BeginRead(SBufferPoolItem* item) final { return NULL; }
	void* BeginWrite(SBufferPoolItem* item) final
	{
		MEMORY_SCOPE_CHECK_HEAP();
		void *ret;
	#if BUFFER_ENABLE_DIRECT_ACCESS
		CDeviceObjectFactory::MarkReadRange(item->m_buffer->GetBuffer(), item->m_offset, 0, D3D11_MAP_WRITE);
		ret = reinterpret_cast<uint8*>(item->m_base_ptr) + item->m_offset;
	#else
		ret = CDeviceObjectFactory::Map(item->m_buffer->GetBuffer(), 0, item->m_offset, 0, m_map_type) + item->m_offset;
	#endif
		return ret;
	}
	void EndReadWrite(SBufferPoolItem* item, bool requires_flush) final
	{
	#if BUFFER_ENABLE_DIRECT_ACCESS
		CDeviceObjectFactory::MarkWriteRange(item->m_buffer->GetBuffer(), item->m_offset, item->m_size, D3D11_MAP_WRITE);
	#else
		CDeviceObjectFactory::Unmap(item->m_buffer->GetBuffer(), 0, item->m_offset, item->m_size, m_map_type);
	#endif

		m_map_type = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
	}
	void Write(SBufferPoolItem* item, const void* src, buffer_size_t size, buffer_size_t offset) final
	{
		MEMORY_SCOPE_CHECK_HEAP();
		DEVBUFFERMAN_ASSERT((size + offset) <= item->m_size);
		DEVBUFFERMAN_ASSERT(item->m_size <= m_backing_buffer.m_capacity);

		size = min(item->m_size, Align(size, CRY_PLATFORM_ALIGNMENT));

	#if BUFFER_ENABLE_DIRECT_ACCESS
		CDeviceObjectFactory::UploadContents<true>(item->m_buffer->GetBuffer(), 0, item->m_offset + offset, size, m_map_type, src, reinterpret_cast<uint8*>(item->m_base_ptr) + item->m_offset);
	#else
		CDeviceObjectFactory::UploadContents<false>(item->m_buffer->GetBuffer(), 0, item->m_offset + offset, size, m_map_type, src);
	#endif

		m_map_type = BINDFLAGS_to_NOOVERWRITE(BIND_FLAGS);
	}
};

//////////////////////////////////////////////////////////////////////////////////////
// CTransientBufferPool is a buffer pool for geometry that can change frequently and
// is only valid for a single frame (fire&forgot geometry).
//
// Corresponding D3D_USAGE : USAGE_DYNAMIC
// Corresponding update strategy : DISCARD + NO_OVERWRITE direct map of the buffer
typedef CTransientBufferPool<CDeviceObjectFactory::BIND_VERTEX_BUFFER> CTransientBufferPoolVB;
typedef CTransientBufferPool<CDeviceObjectFactory::BIND_INDEX_BUFFER>  CTransientBufferPoolIB;
#endif

//////////////////////////////////////////////////////////////////////////
// Freestanding buffer implementation
template<
  size_t BIND_FLAGS,
  size_t USAGE_FLAGS,
  typename Allocator,
  template<size_t,size_t> class Updater>
struct CFreeBufferPoolImpl final : public SBufferPool
{
	typedef Updater<BIND_FLAGS,USAGE_FLAGS> updater_t;

	SBufferPoolBank m_backing_buffer;
	buffer_size_t   m_allocation_size;
	size_t          m_item_handle;
	updater_t       m_updater;

public:
	CFreeBufferPoolImpl(SStagingResources& resources, buffer_size_t size)
		: m_backing_buffer(~0u)
		, m_allocation_size((max(size, buffer_size_t(1u)) + (SPoolConfig::POOL_ALIGNMENT - 1)) & ~(SPoolConfig::POOL_ALIGNMENT - 1))
		, m_item_handle(~0u)
		, m_updater(resources)
	{
		if (!CreateResources(true, true))
		{
			CryLogAlways("DEVBUFFER WARNING: could not create free standing buffer");
		}
	}

	virtual ~CFreeBufferPoolImpl()  { FreeResources();  }

	item_handle_t Allocate(buffer_size_t size, bool /*bUsePool*/) final
	{
		// Align the allocation size up to the configured allocation alignment
		size = (max(size, buffer_size_t(1u)) + (SPoolConfig::POOL_ALIGNMENT - 1)) & ~(SPoolConfig::POOL_ALIGNMENT - 1);

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

		CDeviceObjectFactory::ExtractBasePointer(m_backing_buffer.m_buffer->GetBuffer(), D3D11_MAP_WRITE_NO_OVERWRITE, item->m_base_ptr);

		m_backing_buffer.m_free_space += size;
		return (m_item_handle = item->m_handle);
	}

	void Free(SBufferPoolItem* item) final
	{
		m_item_table.Free(item->m_handle);

		// We can do this safely here as only the item has a reference to
		// this instance.
		delete this;
	}

	bool CreateResources(bool, bool) final
	{
		const SBufferLayout Layout =
		{
			DXGI_FORMAT_UNKNOWN, m_allocation_size, 1,
			USAGE_FLAGS |
			BIND_FLAGS
		};

		if (!(m_backing_buffer.m_buffer = CDeviceBuffer::Create(Layout, nullptr)))
		{
			CryLogAlways("FreeStandingBuffer::CreateResources: could not allocate backing buffer of size %" PRISIZE_T, m_allocation_size);
			return false;
		}

		m_backing_buffer.m_capacity = m_allocation_size;
		m_backing_buffer.m_free_space = 0;
		m_backing_buffer.m_handle = ~0u;

		CDeviceObjectFactory::ExtractBasePointer(m_backing_buffer.m_buffer->GetBuffer(), D3D11_MAP_WRITE_NO_OVERWRITE, m_backing_buffer.m_base_ptr);

		return true;
	}
	bool FreeResources() final
	{
		SAFE_RELEASE(m_backing_buffer.m_buffer);
		m_backing_buffer.m_capacity = 0;
		m_backing_buffer.m_free_space = 0;
		m_backing_buffer.m_handle = ~0u;
		return true;
	}
	bool  GetStats(SDeviceBufferPoolStats&) final                                           { return false; }
	bool  DebugRender() final                                                               { return false; }
	void  Sync() final                                                                      {}
	void  Update(uint32 frameId, DeviceFenceHandle fence, bool allow_defragmentation) final {}
	void  ReleaseEmptyBanks() final                                                         {}
	void* BeginWrite(SBufferPoolItem* item) final
	{
		return m_updater.BeginWrite(item->m_buffer, item->m_size, item->m_offset);
	}
	void EndReadWrite(SBufferPoolItem* item, bool requires_flush) final
	{
		m_updater.EndReadWrite(item->m_buffer, item->m_size, item->m_offset);
	}
	void Write(SBufferPoolItem* item, const void* src, buffer_size_t size, buffer_size_t offset) final
	{
		m_updater.Write(item->m_buffer, item->m_size, item->m_offset + offset, src);
	}
	static SBufferPool* Create(SStagingResources& resources, buffer_size_t size)
	{
		return new CFreeBufferPoolImpl(resources, size);
	}
};
typedef SBufferPool* (* BufferCreateFnc)(SStagingResources&, buffer_size_t);

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
    CDeviceObjectFactory::BIND_VERTEX_BUFFER
    , CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES && BUFFER_SUPPORT_TRANSIENT_POOLS
	, CTransientStagingBufferUpdater
#elif BUFFER_USE_STAGED_UPDATES
    , CStagingBufferUpdater
#else
    , CDirectBufferUpdater
#endif
    > SStaticFreeBufferVB;
typedef CFreeBufferPoolImpl<
    CDeviceObjectFactory::BIND_INDEX_BUFFER
    , 0
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES && BUFFER_SUPPORT_TRANSIENT_POOLS
	, CTransientStagingBufferUpdater
#elif BUFFER_USE_STAGED_UPDATES
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
    CDeviceObjectFactory::BIND_VERTEX_BUFFER
    , CDeviceObjectFactory::USAGE_CPU_WRITE
    | CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT
    | CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
	, CDynamicBufferUpdater
#else
	, CDirectBufferUpdater
#endif
    > SDynamicFreeBufferVB;
typedef CFreeBufferPoolImpl<
    CDeviceObjectFactory::BIND_INDEX_BUFFER
    , CDeviceObjectFactory::USAGE_CPU_WRITE
    | CDeviceObjectFactory::USAGE_DIRECT_ACCESS_CPU_COHERENT
    | CDeviceObjectFactory::USAGE_DIRECT_ACCESS_GPU_COHERENT
    , CDynamicDefragAllocator
#if BUFFER_USE_STAGED_UPDATES
	, CDynamicBufferUpdater
#else
	, CDirectBufferUpdater
#endif
    > SDynamicFreeBufferIB;

//===============================================================================
#if (CRY_RENDERER_DIRECT3D >= 120)
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

	SDescriptorBlock* Allocate(buffer_size_t size)
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
			if (m_fences[i] && GetDeviceObjectFactory().SyncFence(m_fences[i], false, false) == S_OK)
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

#if (CRY_RENDERER_VULKAN >= 10)
class CDescriptorPool
{
public:
	CDescriptorPool()
		: m_frameID(0)
	{
		m_fences.fill(0);
	}

	SDescriptorSet* Allocate(const VkDescriptorSetLayout& descriptorSetLayout)
	{
		AUTO_LOCK(m_lock);

		NCryVulkan::CDevice* pDevice = GetDeviceObjectFactory().GetVKDevice();

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
		ZeroStruct(descriptorSetAllocateInfo);

		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = pDevice->GetDescriptorPool();
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

		VkDescriptorSet vkDescriptorSet = VK_NULL_HANDLE;
		VkResult result = vkAllocateDescriptorSets(pDevice->GetVkDevice(), &descriptorSetAllocateInfo, &vkDescriptorSet);
		CRY_ASSERT_MESSAGE(result == VK_SUCCESS, "Vulkan fails to provide a descriptor set!");

		if (result != VK_SUCCESS)
			return nullptr;

		SDescriptorSet *pDescriptorSet = new SDescriptorSet;
		pDescriptorSet->vkDescriptorSet = vkDescriptorSet;

		return pDescriptorSet;
	}

	void Free(SDescriptorSet* pDescriptorSet)
	{
		AUTO_LOCK(m_lock);
		m_retiredDescriptorSets[m_frameID].push_back(pDescriptorSet);
	}

	void Update(uint32 frameId, DeviceFenceHandle fence)
	{
		NCryVulkan::CDevice* pDevice = GetDeviceObjectFactory().GetVKDevice();

		for (int i = 0; i < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
		{
			if (m_fences[i] && GetDeviceObjectFactory().SyncFence(m_fences[i], false, false) == S_OK)
			{
				for (SDescriptorSet* pDescriptorSet : m_retiredDescriptorSets[i])
				{
					vkFreeDescriptorSets(pDevice->GetVkDevice(), pDevice->GetDescriptorPool(), 1, &pDescriptorSet->vkDescriptorSet);
					SAFE_DELETE(pDescriptorSet);
				}

				m_retiredDescriptorSets[i].clear();
			}
		}

		m_frameID = frameId & SPoolConfig::POOL_FRAME_QUERY_MASK;
		m_fences[m_frameID] = fence;
	}

	void FreeResources()
	{
		for (auto& retiredDescriptorSetList : m_retiredDescriptorSets)
			retiredDescriptorSetList.clear();
	}

private:
	std::array<std::vector<SDescriptorSet*>, SPoolConfig::POOL_FRAME_QUERY_COUNT> m_retiredDescriptorSets;
	std::array<DeviceFenceHandle, SPoolConfig::POOL_FRAME_QUERY_COUNT> m_fences;

	uint32 m_frameID;
	CryCriticalSection m_lock;
};
#endif

//////////////////////////////////////////////////////////////////////////////////////
// Manages all pool - in anonymous namespace to reduce recompiles
struct SPoolManager
{
#if DEVBUFFERMAN_CCONSTANTBUFFER_POOL
	// Storage for constant buffer wrapper instances
	CPartitionTable<CConstantBuffer> m_constant_buffers[2];
#endif

#if (CRY_RENDERER_DIRECT3D >= 120) || (CRY_RENDERER_VULKAN >= 10)
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

#if ENABLE_STATOSCOPE
	SStatoscopeData m_sdata[2];
#endif

	CConstantBufferPtr m_pNullConstantBuffer;
	CGpuBuffer         m_nullBufferTyped;
	CGpuBuffer         m_nullBufferStructured;

	bool m_initialized;

	SPoolManager()
		: m_initialized()
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
			if (GetDeviceObjectFactory().CreateFence(m_fences[i]) != S_OK)
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


#if ENABLE_STATOSCOPE
		memset(m_sdata, 0, sizeof(m_sdata));
#endif

		m_pNullConstantBuffer.Assign_NoAddRef(new CConstantBuffer(~0u));
		m_pNullConstantBuffer->m_size = 0;
		m_pNullConstantBuffer->m_dynamic = false;
		m_pNullConstantBuffer->m_lock = true;
		m_pNullConstantBuffer->m_buffer = CDeviceBuffer::Create({ DXGI_FORMAT_UNKNOWN, 0, 256, CDeviceObjectFactory::BIND_CONSTANT_BUFFER }, nullptr);

		m_nullBufferTyped.Create(0, 0, DXGI_FORMAT_R32G32B32A32_FLOAT, CDeviceObjectFactory::BIND_UNORDERED_ACCESS | CDeviceObjectFactory::BIND_SHADER_RESOURCE, nullptr);
		m_nullBufferStructured.Create(0, sizeof(Vec4), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_STRUCTURED, nullptr);

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

		m_pNullConstantBuffer.reset();
		m_nullBufferTyped.Release();
		m_nullBufferStructured.Release();

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

#if (CRY_RENDERER_DIRECT3D >= 120) || (CRY_RENDERER_VULKAN >= 10)
		m_ResourceDescriptorPool.FreeResources();
#endif

		for (size_t i = 0; i < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
		{
			if (GetDeviceObjectFactory().ReleaseFence(m_fences[i]) != S_OK)
			{
				CryLogAlways("SPoolManager::Initialize: could not releasefence");
				success = false;
			}
			m_fences[i] = DeviceFenceHandle();
		}

		m_pNullConstantBuffer.reset();
		m_nullBufferTyped.Release();
		m_nullBufferStructured.Release();

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

//////////////////////////////////////////////////////////////////////////
CDeviceBufferManager* CDeviceBufferManager::Instance()
{
	return &gcpRendD3D->m_DevBufMan;
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
	FUNCTION_PROFILER_RENDERER();
	MEMORY_SCOPE_CHECK_HEAP();
	SREC_AUTO_LOCK(s_PoolManager.m_lock);

	for (int i = 0; i < SPoolConfig::POOL_FRAME_QUERY_COUNT; ++i)
		GetDeviceObjectFactory().SyncFence(s_PoolManager.m_fences[i], true);

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
	GetDeviceObjectFactory().IssueFence(s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::ReleaseEmptyBanks(uint32 frameId)
{
	FUNCTION_PROFILER_RENDERER();
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
	GetDeviceObjectFactory().IssueFence(s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::Update(uint32 frameId, bool called_during_loading)
{
	FUNCTION_PROFILER_RENDERER();

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

	GetDeviceObjectFactory().SyncFence(s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK], true);

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

#if (CRY_RENDERER_DIRECT3D >= 120) || (CRY_RENDERER_VULKAN >= 10)
	s_PoolManager.m_ResourceDescriptorPool.Update(frameId,
	                                              s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]);
#endif

	// Note: Issue the fence now for COPY_ON_WRITE. If the GPU has caught up to this point, no previous drawcall
	// will be pending and therefore it is safe to just reuse the previous allocation.
	GetDeviceObjectFactory().IssueFence(s_PoolManager.m_fences[frameId & SPoolConfig::POOL_FRAME_QUERY_MASK]);
}

//////////////////////////////////////////////////////////////////////////////////////
CConstantBuffer* CDeviceBufferManager::CreateConstantBufferRaw(buffer_size_t size, bool dynamic, bool ts)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_creator_time);
	size = (max(size, buffer_size_t(1u)) + (255)) & ~(255);
	CConditonalDevManLock lock(this, ts);
#if DEVBUFFERMAN_CCONSTANTBUFFER_POOL
	CConstantBuffer* buffer = &s_PoolManager.m_constant_buffers[ts][s_PoolManager.m_constant_buffers[ts].Allocate()];
#else
	CConstantBuffer* buffer = new CConstantBuffer(0xCACACACA);
#endif
	buffer->m_size = size;
	buffer->m_dynamic = dynamic;
	buffer->m_lock = ts;
	return buffer;
}

CConstantBufferPtr CDeviceBufferManager::GetNullConstantBuffer()
{
	return s_PoolManager.m_pNullConstantBuffer;
}

CGpuBuffer* CDeviceBufferManager::GetNullBufferTyped()
{
	return &s_PoolManager.m_nullBufferTyped;
}

CGpuBuffer* CDeviceBufferManager::GetNullBufferStructured()
{
	return &s_PoolManager.m_nullBufferStructured;
}


#if (CRY_RENDERER_DIRECT3D >= 120)
SDescriptorBlock* CDeviceBufferManager::CreateDescriptorBlock(buffer_size_t size)
{
	return s_PoolManager.m_ResourceDescriptorPool.Allocate(size);
}

void CDeviceBufferManager::ReleaseDescriptorBlock(SDescriptorBlock* pBlock)
{
	CRY_ASSERT(pBlock != NULL);
	s_PoolManager.m_ResourceDescriptorPool.Free(pBlock);
}

#else
SDescriptorBlock* CDeviceBufferManager::CreateDescriptorBlock(buffer_size_t size)               { return NULL; }
void              CDeviceBufferManager::ReleaseDescriptorBlock(SDescriptorBlock* pBlock) {}
#endif

#if (CRY_RENDERER_VULKAN >= 10)
SDescriptorSet* CDeviceBufferManager::AllocateDescriptorSet(const VkDescriptorSetLayout& descriptorSetLayout)
{
	return s_PoolManager.m_ResourceDescriptorPool.Allocate(descriptorSetLayout);
}

void CDeviceBufferManager::ReleaseDescriptorSet(SDescriptorSet *pDescriptorSet)
{
	CRY_ASSERT(pDescriptorSet != NULL);
	s_PoolManager.m_ResourceDescriptorPool.Free(pDescriptorSet);
}
#endif

// PPOL_ALIGNMENT is 128 in general, which is a bit much, we use the smallest alignment necessary for fast mem-copies
#if 0
buffer_size_t CDeviceBufferManager::GetBufferAlignmentForStreaming()
{
	return SPoolConfig::POOL_ALIGNMENT;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////
buffer_handle_t CDeviceBufferManager::Create_Locked(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, buffer_size_t size, bool bUsePool)
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
			item_handle_t item_handle = pool->Allocate(size, bUsePool);
			return item_handle == ~0u
			       ? (buffer_handle_t) ~0u
			       : (buffer_handle_t)pool->Resolve(item_handle);
		}
	}
#endif

	item_handle_t item_handle = s_PoolManager.m_pools[type][usage]->Allocate(size, bUsePool);
	return item_handle == ~0u ? (buffer_handle_t) ~0u : (buffer_handle_t)s_PoolManager.m_pools[type][usage]->Resolve(item_handle);
}

buffer_handle_t CDeviceBufferManager::Create(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, buffer_size_t size, bool bUsePool)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_creator_time);
	if (!s_PoolManager.m_pools[type][usage])
		return ~0u;
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SREC_AUTO_LOCK(s_PoolManager.m_pools[type][usage]->m_lock);
	return Create_Locked(type, usage, size, bUsePool);
}

//////////////////////////////////////////////////////////////////////////////////////
void CDeviceBufferManager::Destroy_Locked(buffer_handle_t handle)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
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
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	return item.m_pool->BeginRead(&item);
}
void* CDeviceBufferManager::BeginRead(buffer_handle_t handle)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	return BeginRead_Locked(handle);
}

//////////////////////////////////////////////////////////////////////////////////////
buffer_size_t CDeviceBufferManager::Size_Locked(buffer_handle_t handle)
{
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	return item.m_size;
}
buffer_size_t CDeviceBufferManager::Size(buffer_handle_t handle)
{
	return Size_Locked(handle);
}

//////////////////////////////////////////////////////////////////////////////////////
void* CDeviceBufferManager::BeginWrite_Locked(buffer_handle_t handle)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_WRITTEN(Size(handle));
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
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
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
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
  buffer_handle_t handle, const void* src, buffer_size_t size, buffer_size_t offset)
{
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_WRITTEN(Size(handle));
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	item.m_pool->Write(&item, src, size, offset);
	return true;
}
bool CDeviceBufferManager::UpdateBuffer
  (buffer_handle_t handle, const void* src, buffer_size_t size, buffer_size_t offset)
{
#if CRY_PLATFORM_WINDOWS
	SRecursiveSpinLocker __lock(&s_PoolManager.m_lock);
#endif
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	SREC_AUTO_LOCK(item.m_pool->m_lock);
	return UpdateBuffer_Locked(handle, src, size, offset);
}
//////////////////////////////////////////////////////////////////////////////////////
D3DBuffer* CDeviceBufferManager::GetD3D(buffer_handle_t handle, buffer_size_t* offset)
{
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	*offset = item.m_offset;
	DEVBUFFERMAN_ASSERT(item.m_buffer);
	return item.m_buffer->GetBuffer();
}

//////////////////////////////////////////////////////////////////////////////////////
D3DBuffer* CDeviceBufferManager::GetD3D(buffer_handle_t handle, buffer_size_t* offset, buffer_size_t* size)
{
	MEMORY_SCOPE_CHECK_HEAP();
	DEVBUFFERMAN_ASSERT(handle != 0);
	SBufferPoolItem& item = *reinterpret_cast<SBufferPoolItem*>(handle);
	*offset = item.m_offset;
	*size = item.m_size;
	DEVBUFFERMAN_ASSERT(item.m_buffer);
	return item.m_buffer->GetBuffer();
}

//////////////////////////////////////////////////////////////////////////////////////
D3DVertexBuffer* CDeviceBufferManager::GetD3DVB(buffer_handle_t handle, buffer_size_t* offset)
{
	MEMORY_SCOPE_CHECK_HEAP();
	return static_cast<D3DVertexBuffer*>(GetD3D(handle, offset));
}

//////////////////////////////////////////////////////////////////////////////////////
D3DIndexBuffer* CDeviceBufferManager::GetD3DIB(buffer_handle_t handle, buffer_size_t* offset)
{
	MEMORY_SCOPE_CHECK_HEAP();
	return static_cast<D3DIndexBuffer*>(GetD3D(handle, offset));
}

//////////////////////////////////////////////////////////////////////////////////////
bool CDeviceBufferManager::GetStats(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, SDeviceBufferPoolStats& stats)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
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
	, m_nUpdCount(0u)
	, m_clearFlags(0)
{}

//////////////////////////////////////////////////////////////////////////////////////
CConstantBuffer::~CConstantBuffer()
{
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS == 0
	if (m_buffer)
	{
		if (!gRenDev->m_pRT || gRenDev->m_pRT->IsRenderThread())
		{
			m_buffer->Release();
		}
		else
		{
			// Only on DX11 have to send Release to the RenderThread
			CDeviceBuffer* pDevBuffer = m_buffer;
			gRenDev->ExecuteRenderThreadCommand([pDevBuffer] { pDevBuffer->Release(); }, 
				ERenderCommandFlags::RenderLoadingThread_defer | ERenderCommandFlags::LevelLoadingThread_defer);
		}
		m_buffer = nullptr;
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////////////
void CConstantBuffer::ReturnToPool()
{
	CConditonalDevManLock lock(&gcpRendD3D->m_DevBufMan, m_lock);
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	if (m_used)
	{
		s_PoolManager.m_constant_allocator[m_lock].Free(this);
		m_used = 0;
	}
#endif

#if DEVBUFFERMAN_CCONSTANTBUFFER_POOL
	if (m_handle != ~0u)
		s_PoolManager.m_constant_buffers[m_lock].Free(m_handle);
#else
	delete this;
#endif
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

		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::MarkReadRange(m_buffer->GetBuffer(), m_offset, 0, D3D11_MAP_WRITE);

		void* base_ptr = m_base_ptr;

	#if (CRY_RENDERER_DIRECT3D >= 111) && (CRY_RENDERER_DIRECT3D < 120)
		auto pAllocator = static_cast<CPartitionAllocator*>(m_allocator);
		pAllocator->map();
		base_ptr = pAllocator->base_ptr();
	#endif

		return (void*)((uintptr_t)base_ptr + m_offset);
	}
#else
	if (!m_used)
	{
		const SBufferLayout Layout =
		{
			DXGI_FORMAT_UNKNOWN, m_size, 1,
			(m_dynamic ? CDeviceObjectFactory::USAGE_CPU_WRITE : 0) |
			CDeviceObjectFactory::BIND_CONSTANT_BUFFER
		};

		if (!(m_buffer = CDeviceBuffer::Create(Layout, nullptr)))
		{
			CryLogAlways("CConstantBuffer::BeginWrite: could not allocate buffer of size %" PRISIZE_T, m_size);
			return nullptr;
		}

		m_used = 1;
	}
	if (m_dynamic)
	{
		return CDeviceObjectFactory::Map(m_buffer->GetBuffer(), 0, m_offset, 0, D3D11_MAP_WRITE_DISCARD_CB);
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
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	if (m_used)
	{
		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::MarkWriteRange(m_buffer->GetBuffer(), m_offset, m_size, D3D11_MAP_WRITE);

#if (CRY_RENDERER_DIRECT3D >= 111) && (CRY_RENDERER_DIRECT3D < 120)
		auto pAllocator = static_cast<CPartitionAllocator*>(m_allocator);
		pAllocator->unmap();
#endif

	}
#else
	if (m_dynamic)
	{
		CDeviceObjectFactory::Unmap(m_buffer->GetBuffer(), 0, m_offset, m_size, D3D11_MAP_WRITE_DISCARD_CB);
	}
	else
	{
		const SResourceMemoryAlignment& layout = { sizeof(char), m_size, 0, 0 };
		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(m_base_ptr, this, layout);
		delete[] alias_cast<char*>(m_base_ptr);
		m_base_ptr = nullptr;
	}
#endif
	m_nUpdCount++;
}

//////////////////////////////////////////////////////////////////////////////////////
bool CConstantBuffer::UpdateBuffer(const void* src, buffer_size_t size, buffer_size_t offset /*= 0*/, int numDataBlocks /*= 1*/)
{
	assert(m_dynamic || size == m_size);
	
	STATOSCOPE_TIMER(s_PoolManager.m_sdata[0].m_io_time);
	STATOSCOPE_IO_WRITTEN(m_size);
	CConditonalDevManLock lock(&gcpRendD3D->m_DevBufMan, m_lock);
	size = std::min((buffer_size_t)m_size, size);
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	if (m_used)
	{
		s_PoolManager.m_constant_allocator[m_lock].Free(this);
		m_used = 0;
	}
	if (s_PoolManager.m_constant_allocator[m_lock].Allocate(this))
	{
		m_used = 1;

		void* base_ptr = m_base_ptr;

#if (CRY_RENDERER_DIRECT3D >= 111) && (CRY_RENDERER_DIRECT3D < 120)
		auto pAllocator = static_cast<CPartitionAllocator*>(m_allocator);
		pAllocator->map();
		base_ptr = pAllocator->base_ptr();
#endif

		CDeviceObjectFactory::UploadContents<true>(m_buffer->GetBuffer(), 0, m_offset + offset, size, D3D11_MAP_WRITE_NO_OVERWRITE_CB, src, reinterpret_cast<uint8*>(base_ptr) + m_offset + offset, numDataBlocks);

#if (CRY_RENDERER_DIRECT3D >= 111) && (CRY_RENDERER_DIRECT3D < 120)
		pAllocator->unmap();
#endif

	}
#else
	if (!m_used)
	{
		const SBufferLayout Layout =
		{
			DXGI_FORMAT_UNKNOWN, m_size, 1,
			(m_dynamic ? CDeviceObjectFactory::USAGE_CPU_WRITE : 0) |
			CDeviceObjectFactory::BIND_CONSTANT_BUFFER
		};

		if (!(m_buffer = CDeviceBuffer::Create(Layout, nullptr)))
		{
			CryLogAlways("CConstantBuffer::BeginWrite: could not allocate buffer of size %" PRISIZE_T, m_size);
			return false;
		}

		m_used = 1;
	}
	// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
	if (m_dynamic)
	{
		CDeviceObjectFactory::UploadContents<false>(m_buffer->GetBuffer(), 0, offset, size, D3D11_MAP_WRITE_DISCARD_CB, src, nullptr, numDataBlocks);
	}
	else
	{
		const SResourceMemoryMapping& mapping =
		{
			{ sizeof(char), m_size, m_size, m_size }, // src layout
			{ offset, 0, 0, 0 },                      // dst position
			{ size, 1, 1, 1 }                         // dst size
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(src, this, mapping);
	}
#endif
	m_nUpdCount++;

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////

SResourceView SResourceView::ShaderResourceRawView(DXGI_FORMAT nFormat, int nFirstElement, int nElementCount, int nFlags)
{
	SResourceView result(0);

	result.m_Desc.eViewType = eShaderResourceView;
	result.m_Desc.nFormat = nFormat;
	result.m_Desc.nOffsetBits = IntegerLog2_RoundUp(uint32(nFirstElement + 1));
	result.m_Desc.nOffsetAndSize = uint64(nFirstElement) << (46 - result.m_Desc.nOffsetBits);
	result.m_Desc.nOffsetAndSize |= uint64(nElementCount) & MASK64(46 - result.m_Desc.nOffsetBits);
	result.m_Desc.nFlags = nFlags;

	CRY_ASSERT(nFirstElement == UINT(result.m_Desc.nOffsetAndSize >> (46 - result.m_Desc.nOffsetBits)));
	CRY_ASSERT(nElementCount == UINT(result.m_Desc.nOffsetAndSize & MASK64(46 - result.m_Desc.nOffsetBits)));

	return result;
}

SResourceView SResourceView::RenderTargetRawView(DXGI_FORMAT nFormat, int nFirstElement, int nElementCount)
{
	SResourceView result(0);

	result.m_Desc.eViewType = eRenderTargetView;
	result.m_Desc.nFormat = nFormat;
	result.m_Desc.nOffsetBits = IntegerLog2_RoundUp(uint32(nFirstElement + 1));
	result.m_Desc.nOffsetAndSize = uint64(nFirstElement) << (46 - result.m_Desc.nOffsetBits);
	result.m_Desc.nOffsetAndSize |= uint64(nElementCount) & MASK64(46 - result.m_Desc.nOffsetBits);

	CRY_ASSERT(nFirstElement == UINT(result.m_Desc.nOffsetAndSize >> (46 - result.m_Desc.nOffsetBits)));
	CRY_ASSERT(nElementCount == UINT(result.m_Desc.nOffsetAndSize & MASK64(46 - result.m_Desc.nOffsetBits)));

	return result;
}

SResourceView SResourceView::DepthStencilRawView(DXGI_FORMAT nFormat, int nFirstElement, int nElementCount, int nFlags)
{
	SResourceView result(0);

	result.m_Desc.eViewType = eDepthStencilView;
	result.m_Desc.nFormat = nFormat;
	result.m_Desc.nOffsetBits = IntegerLog2_RoundUp(uint32(nFirstElement + 1));
	result.m_Desc.nOffsetAndSize = uint64(nFirstElement) << (46 - result.m_Desc.nOffsetBits);
	result.m_Desc.nOffsetAndSize |= uint64(nElementCount) & MASK64(46 - result.m_Desc.nOffsetBits);
	result.m_Desc.nFlags = nFlags;

	CRY_ASSERT(nFirstElement == UINT(result.m_Desc.nOffsetAndSize >> (46 - result.m_Desc.nOffsetBits)));
	CRY_ASSERT(nElementCount == UINT(result.m_Desc.nOffsetAndSize & MASK64(46 - result.m_Desc.nOffsetBits)));

	return result;
}

SResourceView SResourceView::UnorderedAccessRawView(DXGI_FORMAT nFormat, int nFirstElement, int nElementCount, int nFlags)
{
	SResourceView result(0);

	result.m_Desc.eViewType = eUnorderedAccessView;
	result.m_Desc.nFormat = nFormat;
	result.m_Desc.nOffsetBits = IntegerLog2_RoundUp(uint32(nFirstElement + 1));
	result.m_Desc.nOffsetAndSize = uint64(nFirstElement) << (46 - result.m_Desc.nOffsetBits);
	result.m_Desc.nOffsetAndSize |= uint64(nElementCount) & MASK64(46 - result.m_Desc.nOffsetBits);
	result.m_Desc.nFlags = nFlags;

	CRY_ASSERT(nFirstElement == UINT(result.m_Desc.nOffsetAndSize >> (46 - result.m_Desc.nOffsetBits)));
	CRY_ASSERT(nElementCount == UINT(result.m_Desc.nOffsetAndSize & MASK64(46 - result.m_Desc.nOffsetBits)));

	return result;
}

//============================================================
void CGpuBuffer::OwnDevBuffer(CDeviceBuffer* pDeviceBuf)
{
	// TODO: not sure how to handle this
	__debugbreak();

#if 0
	SAFE_RELEASE(m_pDevBuffer);

	m_pDevBuffer = pDeviceBuf;
	if (m_pDevBuffer)
	{
		const SBufferLayout Layput = m_pDevBuffer->GetLayout();

		// *INDENT-OFF*
		m_eMapMode = 
			((m_eFlags & CDeviceObjectFactory::BIND_SHADER_RESOURCE ) ? D3D11_MAP_WRITE_NO_OVERWRITE_SR :
			((m_eFlags & CDeviceObjectFactory::BIND_VERTEX_BUFFER   ) ? D3D11_MAP_WRITE_NO_OVERWRITE_VB :
			((m_eFlags & CDeviceObjectFactory::BIND_INDEX_BUFFER    ) ? D3D11_MAP_WRITE_NO_OVERWRITE_IB :
			((m_eFlags & CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? D3D11_MAP_WRITE_NO_OVERWRITE_UA : D3D11_MAP(0)))));
		// *INDENT-ON*

		m_elementSize  = Layput.m_nElementSize;
		m_elementCount = Layput.m_nElementCount;
		m_eFlags       = Layput.m_eFlags;
		m_eFormat      = Layput.m_eFormat;

		m_nPersistentSize = m_pDevBuffer->GetDeviceSize();
//		CryInterlockedAdd(&CGpuBuffer::s_nStatsCurManagedNonStreamedBufMem, m_nBaseSize);
	}

	InvalidateDeviceResource(this, eDeviceResourceDirty);
#endif
}

CGpuBuffer::~CGpuBuffer()
{
	InvalidateDeviceResource(this, eResourceDestroyed);

	Release();
}

void CGpuBuffer::Release()
{
	MEMORY_SCOPE_CHECK_HEAP();

	if (m_pDeviceBuffer)
	{
		ReleaseDeviceBuffer(m_pDeviceBuffer);
	}

	m_elementCount = 0;
	m_eFlags = 0;
	m_bLocked = false;
	m_eMapMode = D3D11_MAP(0);

	InvalidateDeviceResource(this, eDeviceResourceDirty);
}

void CGpuBuffer::Create(uint32 elementCount, uint32 elementSize, DXGI_FORMAT elementFormat, uint32 eFlags, const void* pData)
{
	Release();

	// *INDENT-OFF*
	m_eMapMode = 
		((eFlags & CDeviceObjectFactory::BIND_SHADER_RESOURCE ) ? D3D11_MAP_WRITE_NO_OVERWRITE_SR :
		((eFlags & CDeviceObjectFactory::BIND_VERTEX_BUFFER   ) ? D3D11_MAP_WRITE_NO_OVERWRITE_VB :
		((eFlags & CDeviceObjectFactory::BIND_INDEX_BUFFER    ) ? D3D11_MAP_WRITE_NO_OVERWRITE_IB :
		((eFlags & CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? D3D11_MAP_WRITE_NO_OVERWRITE_UA : D3D11_MAP(0)))));

	if (!((m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE) && (m_eMapMode == D3D11_MAP_WRITE_DISCARD)))
		eFlags |= CDeviceObjectFactory::USAGE_HIFREQ_HEAP;
	// *INDENT-ON*

	m_elementSize  = elementFormat == DXGI_FORMAT_UNKNOWN ? elementSize : DeviceFormats::GetStride(elementFormat);
	m_elementCount = elementCount;
	m_eFlags       = eFlags;
	m_eFormat      = elementFormat;

	m_pDeviceBuffer = AllocateDeviceBuffer(pData);
}

CDeviceBuffer* CGpuBuffer::AllocateDeviceBuffer(const void* pInitialData) const
{
	const SBufferLayout Layout =
	{
		m_eFormat,
		m_elementCount,
		m_elementSize,
		m_eFlags
	};

	CDeviceBuffer* pResult = CDeviceBuffer::Create(Layout, pInitialData);

	// persistent map on dx12
#if (CRY_RENDERER_DIRECT3D >= 120)
	if (m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE)
	{
		uint8* base_ptr;
		CDeviceObjectFactory::ExtractBasePointer(pResult->GetBuffer(), m_eMapMode, base_ptr);
	}
#endif

	return  pResult;
}

void CGpuBuffer::ReleaseDeviceBuffer(CDeviceBuffer*& pDeviceBuffer) const
{
	// persistent map on dx12
#if (CRY_RENDERER_DIRECT3D >= 120)
	if (m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE)
	{
		CDeviceObjectFactory::ReleaseBasePointer(pDeviceBuffer->GetBuffer());
	}
#endif

	SAFE_RELEASE(pDeviceBuffer);
}

void CGpuBuffer::PrepareUnusedBuffer()
{
	CRY_ASSERT(m_pDeviceBuffer);

	if (!((m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE) && (m_eMapMode == D3D11_MAP_WRITE_DISCARD)))
	{
#if (CRY_RENDERER_DIRECT3D >= 120)
		if (m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE)
		{
			CDeviceObjectFactory::ReleaseBasePointer(m_pDeviceBuffer->GetBuffer());
		}
#endif

		if (m_pDeviceBuffer->SubstituteUsedResource() == CDeviceResource::eSubResult_Substituted)
			InvalidateDeviceResource(this, eDeviceResourceDirty);

#if (CRY_RENDERER_DIRECT3D >= 120)
		if (m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE)
		{
			uint8* base_ptr;
			CDeviceObjectFactory::ExtractBasePointer(m_pDeviceBuffer->GetBuffer(), m_eMapMode, base_ptr);
		}
#endif
	}
}

void CGpuBuffer::UpdateBufferContent(const void* pData, uint32 nSize)
{
	CRY_ASSERT(!m_bLocked);

	if (nSize)
	{
		PrepareUnusedBuffer();

		if (m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE)
		{
			// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
			CDeviceObjectFactory::UploadContents<false>(m_pDeviceBuffer->GetBuffer(), 0, 0, nSize, D3D11_MAP(m_eMapMode), pData);
		}
		else
		{
			// Asynchronous
			const SResourceMemoryAlignment& layout = { m_elementSize, nSize, 0, 0 };
			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pData, m_pDeviceBuffer, layout);
		}
	}
}

void* CGpuBuffer::Lock()
{
	CRY_ASSERT(m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE);
	CRY_ASSERT(!m_bLocked);

	PrepareUnusedBuffer();

	m_bLocked = true;

	return CDeviceObjectFactory::Map(m_pDeviceBuffer->GetBuffer(), 0, 0, 0, D3D11_MAP(m_eMapMode));
}

void CGpuBuffer::Unlock(buffer_size_t nSize)
{
	CRY_ASSERT(m_eFlags & CDeviceObjectFactory::USAGE_CPU_WRITE);
	CRY_ASSERT(m_bLocked);

	m_bLocked = false;

	CDeviceObjectFactory::Unmap(m_pDeviceBuffer->GetBuffer(), 0, 0, nSize, D3D11_MAP(m_eMapMode));
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
	}
	if (m_bDirty && m_data.size() > 0)
	{
		// NOTE: The pointer and the size is optimal aligned
		const buffer_size_t dataSize = Align(m_data.size(), CRY_PLATFORM_ALIGNMENT);
		m_pConstantBuffer->UpdateBuffer(&m_data[0], dataSize);
	}
	return m_pConstantBuffer;
}

CConstantBufferPtr CGraphicsDeviceConstantBuffer::GetNullConstantBuffer()
{
	assert(m_size > 0);
	CryAutoCriticalSection lock(s_accessLock);
	if (!m_pConstantBuffer)
	{
		m_pConstantBuffer = gRenDev->m_DevBufMan.GetNullConstantBuffer();
	}
	if (m_bDirty && m_data.size() > 0)
	{
		// NOTE: A null-buffer isn't suppose to be dirty or have a size
		__debugbreak();
	}
	return m_pConstantBuffer;
}
