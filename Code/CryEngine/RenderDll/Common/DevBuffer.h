// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _DevBuffer_H_
#define _DevBuffer_H_

#if !defined(D3DBuffer)
	#define D3DBuffer void
#endif
#if !defined(D3DShaderResource)
	#define D3DShaderResource void
#endif
#if !defined(D3DSamplerState)
	#define D3DSamplerState void
#endif

#include <CryCore/Platform/CryWindows.h>

////////////////////////////////////////////////////////////////////////////////////////
// Usage hints
enum BUFFER_USAGE
{
	BU_IMMUTABLE = 0,             // For data that never, ever changes
	BU_STATIC,                    // For long-lived data that changes infrequently (every n-frames)
	BU_DYNAMIC,                   // For short-lived data that changes frequently (every frame)
	BU_TRANSIENT,                 // For very short-lived data that can be considered garbage after first usage
	BU_TRANSIENT_RT,              // For very short-lived data that can be considered garbage after first usage
	BU_WHEN_LOADINGTHREAD_ACTIVE, // yes we can ... because renderloadingthread frames not synced with mainthread frames
	BU_MAX
};

////////////////////////////////////////////////////////////////////////////////////////
// Binding flags
enum BUFFER_BIND_TYPE
{
	BBT_VERTEX_BUFFER = 0,
	BBT_INDEX_BUFFER,
	BBT_CONSTANT_BUFFER,
	BBT_MAX
};

typedef uintptr_t buffer_handle_t;
typedef uint32    item_handle_t;

////////////////////////////////////////////////////////////////////////////////////////
struct SDescriptorBlock
{
	SDescriptorBlock(uint32 id)
		: blockID(id)
		, pBuffer(NULL)
		, size(0)
		, offset(~0u)
	{}

	const uint32 blockID;

	void*        pBuffer;
	uint32       size;
	uint32       offset;
};

////////////////////////////////////////////////////////////////////////////////////////
// Constant buffer wrapper class
class CConstantBuffer
{
public:
	D3DBuffer*    m_buffer;
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
	void*         m_allocator;
#endif
	void*         m_base_ptr;
	item_handle_t m_handle;
	uint32        m_offset;
	uint32        m_size;
	int32         m_nRefCount;
	union
	{
		struct
		{
			uint8 m_used              : 1;
			uint8 m_lock              : 1;
			uint8 m_dynamic           : 1;
			uint8 m_intentionallyNull : 1;
#if defined(OPENGL)
			uint8 m_no_streaming      : 1;
#endif
		};

		uint8 m_clearFlags;
	};

	CConstantBuffer(uint32 handle);
	~CConstantBuffer();

	void       AddRef();
	void       Release();
	D3DBuffer* GetD3D(size_t* offset, size_t* size) const
	{
		*offset = m_offset;
		*size = m_size;
		return m_buffer;
	}

	uint64 GetCode() const
	{
#if CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS || defined(OPENGL)
		uint64 code = reinterpret_cast<uintptr_t>(m_buffer) | ((uint64)m_offset << 40);//|(((uint64)m_size>>4)<<60); Size will follow buffer address, so we just need offset
		return code;
#else
		return reinterpret_cast<uint64>(m_buffer);
#endif
	}

	void* BeginWrite();
	void  EndWrite(bool requires_flush = false);
	void UpdateBuffer(const void*, size_t);
};
typedef _smart_ptr<CConstantBuffer> CConstantBufferPtr;

// Wrapper on the Constant buffer, allowing using it from outside of renderer and RenderThread
class CGraphicsDeviceConstantBuffer : public IGraphicsDeviceConstantBuffer
{
public:
	CGraphicsDeviceConstantBuffer() : m_size(0), m_bDirty(false) {};
	virtual void       SetData(const uint8* data, size_t size) override;
	// Should only be called from the thread that can create and update constant buffers
	CConstantBufferPtr GetConstantBuffer();
	CConstantBufferPtr GetNullConstantBuffer();
	size_t             GetSize() const { return m_size; }

protected:
	virtual void DeleteThis() override { delete this; }

private:
	size_t                    m_size;
	std::vector<uint8>        m_data;
	CConstantBufferPtr        m_pConstantBuffer;
	bool                      m_bDirty;

	static CryCriticalSection s_accessLock;
};

////////////////////////////////////////////////////////////////////////////////////////
// Pool statistics
struct SDeviceBufferPoolStats : private NoCopy
{
	string                buffer_descr;
	size_t                bank_size;       // size of a pool bank in bytes
	size_t                num_banks;       // number of banks currently allocated
	size_t                num_allocs;      // number of allocs present in the device pool
	IDefragAllocatorStats allocator_stats; // backing allocator statistics

	SDeviceBufferPoolStats()
		: buffer_descr()
		, bank_size()
		, num_banks()
		, num_allocs()
		, allocator_stats()
	{ memset(&allocator_stats, 0x0, sizeof(allocator_stats)); }

	~SDeviceBufferPoolStats() {}
};

class CVertexBuffer;
class CIndexBuffer;

class CDeviceBufferManager
{
	////////////////////////////////////////////////////////////////////////////////////////
	// Debug consistency functions
	// Should only be called from the below befriended function! Please do not abuse!
#if defined(CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK)
	// Wanted to only expose the actual function using these, however impossible to forward declare a method
	// without including the full renderer here
	friend class CD3D9Renderer;
	void* BeginReadDirectIB(D3DIndexBuffer*, size_t size, size_t offset);
	void* BeginReadDirectVB(D3DVertexBuffer*, size_t size, size_t offset);
	void  EndReadDirectIB(D3DIndexBuffer*);
	void  EndReadDirectVB(D3DVertexBuffer*);
#endif

	friend class CGuardedDeviceBufferManager;
	friend class CDeviceManager;

	void            PinItem_Locked(buffer_handle_t);
	void            UnpinItem_Locked(buffer_handle_t);
	buffer_handle_t Create_Locked(BUFFER_BIND_TYPE, BUFFER_USAGE, size_t);
	void            Destroy_Locked(buffer_handle_t);
	void* BeginRead_Locked(buffer_handle_t handle);
	void* BeginWrite_Locked(buffer_handle_t handle);
	void  EndReadWrite_Locked(buffer_handle_t handle);
	bool   UpdateBuffer_Locked(buffer_handle_t handle, const void*, size_t);
	size_t Size_Locked(buffer_handle_t);

public:
	CDeviceBufferManager();
	~CDeviceBufferManager();

	////////////////////////////////////////////////////////////////////////////////////////
	// Initialization and destruction and high level update funcationality
	bool Init();
	void Update(uint32 frameId, bool called_during_loading);
	void ReleaseEmptyBanks(uint32 frameId);
	void Sync(uint32 frameId);
	bool Shutdown();

	////////////////////////////////////////////////////////////////////////////////////////
	// ConstantBuffer creation
	CConstantBuffer*          CreateConstantBuffer(size_t size, bool dynamic = true, bool needslock = false);
	static CConstantBufferPtr CreateNullConstantBuffer();

	////////////////////////////////////////////////////////////////////////////////////////
	// Descriptor blocks
	SDescriptorBlock* CreateDescriptorBlock(size_t size);
	void              ReleaseDescriptorBlock(SDescriptorBlock* pBlock);

	////////////////////////////////////////////////////////////////////////////////////////
	// Locks the global devicebuffer lock
	void LockDevMan();
	void UnlockDevMan();

	////////////////////////////////////////////////////////////////////////////////////////
	// Pin/Unpin items for async access outside of the renderthread
	void PinItem(buffer_handle_t);
	void UnpinItem(buffer_handle_t);

	// Returns the size in bytes of the allocation
	size_t Size(buffer_handle_t);

	////////////////////////////////////////////////////////////////////////////////////////
	// Buffer Resource creation methods
	//
	buffer_handle_t Create(BUFFER_BIND_TYPE, BUFFER_USAGE, size_t);
	void            Destroy(buffer_handle_t);

	////////////////////////////////////////////////////////////////////////////////////////
	// Manual IO operations
	//
	// Note: it's an error to NOT end an IO operation with EndReadWrite!!!
	//
	// Note: If you are writing (updating) a buffer only partially, please be aware that the
	//       the contents of the untouched areas might be undefined as a copy-on-write semantic
	//       ensures that the updating of buffers does not synchronize with the GPU at any cost.
	//
	void* BeginRead(buffer_handle_t handle);
	void* BeginWrite(buffer_handle_t handle);
	void  EndReadWrite(buffer_handle_t handle);
	bool UpdateBuffer(buffer_handle_t handle, const void*, size_t);

	////////////////////////////////////////////////////////////////////////////////////////
	// Get Stats back from the devbuffer
	bool GetStats(BUFFER_BIND_TYPE, BUFFER_USAGE, SDeviceBufferPoolStats &);

	////////////////////////////////////////////////////////////////////////////////////////
	// Retrieval of the backing d3d buffers
	//
	// Note: Getting vertexbuffer for an index buffer or vice-versa is not an error!
	//
	// Note: Do not store the returned device buffer pointer outside of the function performing
	//       calls below.

	D3DBuffer*       GetD3D(buffer_handle_t, size_t*);
	D3DVertexBuffer* GetD3DVB(buffer_handle_t, size_t*);
	D3DIndexBuffer*  GetD3DIB(buffer_handle_t, size_t*);

	/////////////////////////////////////////////////////////////
	// Legacy interface
	//
	// Use with care, can be removed at any point!
	CVertexBuffer* CreateVBuffer(size_t, EVertexFormat, const char*, BUFFER_USAGE usage = BU_STATIC);
	void ReleaseVBuffer(CVertexBuffer*);
	bool           UpdateVBuffer(CVertexBuffer*, void*, size_t);

	CIndexBuffer*  CreateIBuffer(size_t, const char*, BUFFER_USAGE usage = BU_STATIC);
	void ReleaseIBuffer(CIndexBuffer*);
	bool           UpdateIBuffer(CIndexBuffer*, void*, size_t);
};

class CGuardedDeviceBufferManager : public NoCopy
{
	CDeviceBufferManager* m_pDevMan;
public:

	explicit CGuardedDeviceBufferManager(CDeviceBufferManager* pDevMan)
		: m_pDevMan(pDevMan)
	{ m_pDevMan->LockDevMan(); }

	~CGuardedDeviceBufferManager() { m_pDevMan->UnlockDevMan(); }

	////////////////////////////////////////////////////////////////////////////////////////
	// Pin/Unpin items for async access outside of the renderthread
	void PinItem(buffer_handle_t handle)   { m_pDevMan->PinItem_Locked(handle); }
	void UnpinItem(buffer_handle_t handle) { m_pDevMan->UnpinItem_Locked(handle); }

	////////////////////////////////////////////////////////////////////////////////////////
	// Buffer Resource creation methods
	//
	buffer_handle_t Create(BUFFER_BIND_TYPE type, BUFFER_USAGE usage, size_t size)
	{ return m_pDevMan->Create_Locked(type, usage, size); }
	void            Destroy(buffer_handle_t handle) { return m_pDevMan->Destroy_Locked(handle); }

	////////////////////////////////////////////////////////////////////////////////////////
	// Manual IO operations
	//
	// Note: it's an error to NOT end an IO operation with EndReadWrite!!!
	//
	// Note: If you are writing (updating) a buffer only partially, please be aware that the
	//       the contents of the untouched areas might be undefined as a copy-on-write semantic
	//       ensures that the updating of buffers does not synchronize with the GPU at any cost.
	//
	void* BeginRead(buffer_handle_t handle)    { return m_pDevMan->BeginRead_Locked(handle); }
	void* BeginWrite(buffer_handle_t handle)   { return m_pDevMan->BeginWrite_Locked(handle); }
	void  EndReadWrite(buffer_handle_t handle) { m_pDevMan->EndReadWrite_Locked(handle); }
	bool  UpdateBuffer(buffer_handle_t handle, const void* src, size_t size)
	{ return m_pDevMan->UpdateBuffer_Locked(handle, src, size); }

	D3DBuffer*       GetD3D(buffer_handle_t handle, size_t* offset) { return m_pDevMan->GetD3D(handle, offset); }
	D3DVertexBuffer* GetD3DVB(buffer_handle_t handle, size_t* offset)
	{ return static_cast<D3DVertexBuffer*>(m_pDevMan->GetD3D(handle, offset)); }
	D3DIndexBuffer*  GetD3DIB(buffer_handle_t handle, size_t* offset)
	{ return static_cast<D3DIndexBuffer*>(m_pDevMan->GetD3D(handle, offset)); }

};

class SRecursiveSpinLock
{
	volatile LONG     m_lock;
	volatile threadID m_owner;
	volatile uint16   m_counter;

	enum { SPIN_COUNT = 10 };

public:

	SRecursiveSpinLock()
		: m_lock()
		, m_owner()
		, m_counter()
	{}

	~SRecursiveSpinLock() {}

	void Lock()
	{
		threadID threadId = CryGetCurrentThreadId();
		int32 iterations = 0;
retry:
		IF (CryInterlockedCompareExchange(&(this->m_lock), 1L, 0L) == 0L, 1)
		{
			assert(m_owner == 0u && m_counter == 0u);
			m_owner = threadId;
			m_counter = 1;
		}
		else
		{
			IF (m_owner == threadId, 1)
				++m_counter;
			else
			{
				CrySleep((1 & isneg(SPIN_COUNT - iterations++)));
				goto retry;
			}
		}
	}

	bool TryLock()
	{
		threadID threadId = CryGetCurrentThreadId();
		IF (CryInterlockedCompareExchange(&m_lock, 1L, 0L) == 0L, 1)
		{
			assert(m_owner == 0u && m_counter == 0u);
			m_owner = threadId;
			m_counter = 1;
			return true;
		}
		else
		{
			IF (m_owner == threadId, 1)
			{
				++m_counter;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	void Unlock()
	{
		assert(m_owner == CryGetCurrentThreadId() && m_counter != 0u);
		IF ((m_counter -= 1) == 0u, 1)
		{
			m_owner = 0u;
			m_lock = 0L;
			MemoryBarrier();
		}
	}
};

class SRecursiveSpinLocker
{
	SRecursiveSpinLock* lock;
public:
	SRecursiveSpinLocker(SRecursiveSpinLock* _lock)
		: lock(_lock)
	{
		lock->Lock();
	}
	~SRecursiveSpinLocker() { lock->Unlock(); }
};
#define SREC_AUTO_LOCK(x) SRecursiveSpinLocker _lock(&(x))

class CConditonalDevManLock
{
	CDeviceBufferManager* m_pDevBufMan;
	int                   m_Active;
public:
	explicit CConditonalDevManLock(CDeviceBufferManager* DevMan, int active)
		: m_pDevBufMan(DevMan)
		, m_Active(active)
	{
		if (m_Active)
			m_pDevBufMan->LockDevMan();
	}

	~CConditonalDevManLock()
	{
		if (m_Active)
			m_pDevBufMan->UnlockDevMan();
	}
};

// CGpuBuffer Flags
#define DX11BUF_DYNAMIC            BIT(0)
#define DX11BUF_STRUCTURED         BIT(1)
#define DX11BUF_BIND_VERTEX_BUFFER BIT(2)
#define DX11BUF_BIND_INDEX_BUFFER  BIT(3)
#define DX11BUF_BIND_SRV           BIT(4)
#define DX11BUF_BIND_UAV           BIT(5)
#define DX11BUF_UAV_APPEND         BIT(6)
#define DX11BUF_UAV_COUNTER        BIT(7)
#define DX11BUF_DRAWINDIRECT       BIT(8)
#define DX11BUF_NULL_RESOURCE      BIT(9)   // The buffer shall be created with DeviceManager::AllocateNullResource, as such it cannot be accessed on the CPU, and all reads on the GPU shall return zero.

struct CGpuBuffer
{
private:
	struct STrackedBuffer
	{
		STrackedBuffer() : m_nLastWriteFrame(0), m_nLastReadFrame(0), m_BufferPersistentMapMode(D3D11_MAP(0)) {}
		STrackedBuffer(const STrackedBuffer& other);
		STrackedBuffer(STrackedBuffer&& other);

		~STrackedBuffer();

		void EnablePersistentMap(bool bEnable);

		_smart_ptr<ID3D11Buffer>              m_pBuffer;
		_smart_ptr<ID3D11ShaderResourceView>  m_pSRV;
		_smart_ptr<ID3D11UnorderedAccessView> m_pUAV;

		uint32                                m_nLastWriteFrame;
		mutable uint32                        m_nLastReadFrame;

		D3D11_MAP                             m_BufferPersistentMapMode;
	};

	typedef std::queue<STrackedBuffer> BufferQueue;

public:
	CGpuBuffer(int maxBufferCopies = MAX_FRAMES_IN_FLIGHT)
		: m_numElements(0)
		, m_flags(0)
		, m_MapMode(D3D11_MAP(0))
		, m_bLocked(false)
		, m_MaxBufferCopies(maxBufferCopies)
	{
		ZeroStruct(m_bufferDesc);
		ZeroStruct(m_srvDesc);
		ZeroStruct(m_uavDesc);
	}

	~CGpuBuffer();

	bool                       operator==(const CGpuBuffer& other) const;

	void                       Create(uint32 numElements, uint32 elementSize, DXGI_FORMAT elementFormat, uint32 flags, const void* pData);
	void                       Release();

	void                       UpdateBufferContent(const void* pData, size_t nSize);
	void*                      Lock();
	void                       Unlock();

	ID3D11Buffer*              GetBuffer() const;
	ID3D11ShaderResourceView*  GetSRV() const;
	ID3D11UnorderedAccessView* GetUAV() const;

	int                        m_numElements;
	int                        m_flags;

private:
	void                  PrepareFreeBuffer(const void* pInitialData = nullptr);
	STrackedBuffer&       GetCurrentBuffer();
	const STrackedBuffer& GetCurrentBuffer() const;

	std::shared_ptr<BufferQueue>     m_pBuffersInUse;

	bool                             m_bLocked;
	int                              m_MaxBufferCopies;

	D3D11_MAP                        m_MapMode;
	D3D11_BUFFER_DESC                m_bufferDesc;
	D3D11_SHADER_RESOURCE_VIEW_DESC  m_srvDesc;
	D3D11_UNORDERED_ACCESS_VIEW_DESC m_uavDesc;

};

#endif // _D3DBuffer_H
