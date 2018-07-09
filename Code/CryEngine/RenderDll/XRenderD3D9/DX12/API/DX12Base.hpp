// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Range.h>

#include "xxhash.h"
#include <fasthash/fasthash.inl>
#include <concqueue/concqueue.hpp>

extern int g_nPrintDX12;

#define DX12_PTR(T)        _smart_ptr < T >
#define DX12_NEW_RAW(ctor) NCryDX12::PassAddRef(new ctor)

#ifdef _DEBUG

	#define DX12_FUNC_LOG do {                        \
		if (g_nPrintDX12)                               \
		{ CryLog("DX12 function call: %s", __FUNC__); } \
	} while (0);

#else
	#define DX12_FUNC_LOG do {} while (0);
#endif

#if CRY_PLATFORM_64BIT && CRY_PLATFORM_DESKTOP
	#define DX12_LINKEDADAPTER            true
	#define DX12_LINKEDADAPTER_SIMULATION 0
#endif

#define DX12_PRECISE_DEDUPLICATION
#define DX12_OMITTABLE_COMMANDLISTS
#define	DX12_MAP_STAGE_MULTIGPU        1
#define DX12_MAP_DISCARD_MARKER        BIT(3)
#define DX12_COPY_REVERTSTATE_MARKER   BIT(2)
#define DX12_COPY_PIXELSTATE_MARKER    BIT(3)
#define DX12_COPY_CONCURRENT_MARKER    BIT(4)
#define DX12_RESOURCE_FLAG_OVERLAP     BIT(1)
#define DX12_RESOURCE_FLAG_HIFREQ_HEAP BIT(31)
#define DX12_GPU_VIRTUAL_ADDRESS_NULL  0ULL
#define INVALID_CPU_DESCRIPTOR_HANDLE  CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT)
#define INVALID_GPU_DESCRIPTOR_HANDLE  CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT)
#define DX12_CONCURRENCY_ANALYZER      false
#define DX12_FENCE_ANALYZER            false
#define DX12_BARRIER_ANALYZER          false


// Extract lowest set isolated bit "intrinsic" -> _blsi_u32 (Jaguar == PS4|XO, PileDriver+, Haswell+)
#define blsi(field) (field & (-static_cast<INT>(field)))

namespace NCryDX12
{

// Forward declarations
class CCommandList;
class CCommandListPool;
class CDescriptorHeap;
class CDevice;
class CGraphicsPSO;
class CPSOCache;
class CResource;
class CView;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
template<typename T>
static T* PassAddRef(T* ptr)
{
	if (ptr)
	{
		ptr->AddRef();
	}

	return ptr;
}

//---------------------------------------------------------------------------------------------------------------------
template<typename T>
static T* PassAddRef(const _smart_ptr<T>& ptr)
{
	if (ptr)
	{
		ptr.get()->AddRef();
	}

	return ptr.get();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef uint32_t THash;
template<size_t length>
ILINE THash ComputeSmallHash(const void* data, UINT seed = 666)
{
	return fasthash::fasthash32<length>(data, seed);
}

//---------------------------------------------------------------------------------------------------------------------

struct NodeMaskCV
{
	UINT creationMask;
	UINT visibilityMask;
};

typedef struct NodeMaskCV NODE64;

//---------------------------------------------------------------------------------------------------------------------
UINT              GetDXGIFormatSize(DXGI_FORMAT format);
D3D12_CLEAR_VALUE GetDXGIFormatClearValue(DXGI_FORMAT format, bool depth);

ILINE bool        IsDXGIFormatCompressed(DXGI_FORMAT format)
{
	return
	  (format >= DXGI_FORMAT_BC1_TYPELESS && format <= DXGI_FORMAT_BC5_SNORM) ||
	  (format >= DXGI_FORMAT_BC6H_TYPELESS && format <= DXGI_FORMAT_BC7_UNORM_SRGB);
}

ILINE bool IsDXGIFormatCompressed4bpp(DXGI_FORMAT format)
{
	return
	  (format >= DXGI_FORMAT_BC1_TYPELESS && format <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
	  (format >= DXGI_FORMAT_BC4_TYPELESS && format <= DXGI_FORMAT_BC4_SNORM);
}

ILINE bool IsDXGIFormatCompressed8bpp(DXGI_FORMAT format)
{
	return
	  (format >= DXGI_FORMAT_BC2_TYPELESS && format <= DXGI_FORMAT_BC3_UNORM_SRGB) ||
	  (format >= DXGI_FORMAT_BC5_TYPELESS && format <= DXGI_FORMAT_BC5_SNORM) ||
	  (format >= DXGI_FORMAT_BC6H_TYPELESS && format <= DXGI_FORMAT_BC7_UNORM_SRGB);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CRefCounted
{
	volatile int m_RefCount;

public:
	CRefCounted() : m_RefCount(0)
	{}

	CRefCounted(CRefCounted&& r) : m_RefCount(std::move(r.m_RefCount))
	{}

	CRefCounted& operator=(CRefCounted&& r)
	{ m_RefCount = std::move(r.m_RefCount); return *this; }

	void AddRef() threadsafe
	{
		CryInterlockedIncrement(&m_RefCount);
	}
	void Release() threadsafe
	{
		if (!CryInterlockedDecrement(&m_RefCount))
		{
			delete this;
		}
	}
	void ReleaseNoDelete() threadsafe
	{
		CryInterlockedDecrement(&m_RefCount);
	}

protected:
	virtual ~CRefCounted() {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceObject : public CRefCounted
{
public:
	ILINE CDevice* GetDevice() const
	{
		return m_pDevice;
	}

protected:
	CDeviceObject(CDevice* device);
	virtual ~CDeviceObject();

	CDeviceObject(CDeviceObject&& r)
		: CRefCounted(std::move(r))
		, m_pDevice(std::move(r.m_pDevice))
	{}

	CDeviceObject& operator=(CDeviceObject&& r)
	{
		CRefCounted::operator=(std::move(r));

		m_pDevice = std::move(r.m_pDevice);

		return *this;
	}

	ILINE void SetDevice(CDevice* device)
	{
		m_pDevice = device;
	}

private:
	CDevice* m_pDevice;
};

static const UINT CONSTANT_BUFFER_ELEMENT_SIZE = 16U;
}

#include "DX12Device.hpp"
