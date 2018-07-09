// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if (CRY_RENDERER_DIRECT3D >= 120)
#elif (CRY_RENDERER_DIRECT3D >= 110)
	#if defined(USE_NV_API)
		#include "D3D11/DeviceResources_D3D11_NVAPI.h"
	#endif
#endif

typedef uintptr_t DeviceBufferHandle;
typedef uint32    buffer_size_t;

class CDeviceTexture;
typedef CDeviceTexture* LPDEVICETEXTURE;

class CDeviceBuffer;
typedef CDeviceBuffer* LPDEVICEBUFFER;

////////////////////////////////////////////////////////////////////////////

struct CDeviceInputStream
{
	friend class CDeviceObjectFactory;
	friend class CDeviceGraphicsCommandList;
	friend class CDeviceGraphicsCommandInterfaceImpl;

	/*explicit*/ operator SStreamInfo() const
	{
		SStreamInfo stream;
		stream.hStream = hStream;
		stream.nStride = nStride;
		stream.nSlot = nSlot;
		return stream;
	}

	bool operator!() const
	{
		return hStream == ~0u;
	}

	const CDeviceInputStream& operator[](buffer_size_t offset) const
	{
		return *(this + offset);
	}

private:
	DeviceBufferHandle hStream;
	buffer_size_t      nStride; // NOTE: needs to contain index format for index buffers
	buffer_size_t      nSlot;

	CDeviceInputStream() { hStream = ~0u; nStride = 0; nStride = 0; nSlot = 0; }
	CDeviceInputStream(const SStreamInfo& stream) { *this = stream; }
	CDeviceInputStream(DeviceBufferHandle stream, buffer_size_t stride, buffer_size_t slot) : hStream(stream), nStride(stride), nSlot(slot) {}

	CDeviceInputStream& operator=(const SStreamInfo& stream)
	{
		hStream = stream.hStream;
		nStride = stream.nStride;
		nSlot = stream.nSlot;
		return *this;
	}

	inline bool operator==(const CDeviceInputStream& other) const
	{
		return (hStream == other.hStream) & (nStride == other.nStride) & (nSlot == other.nSlot);
	}
};

////////////////////////////////////////////////////////////////////////////
#include "DeviceObjects.h" // CComposedDeviceObjectStorage,STexturePayload
////////////////////////////////////////////////////////////////////////////

struct SResourceLayout
{
	buffer_size_t m_byteCount;
	buffer_size_t m_elementCount;
	uint32        m_eFlags;           // e.g. CDeviceObjectFactory::BIND_VERTEX_BUFFER

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	int32         m_eSRAMOffset;
#endif
};

class CDeviceResource
{

public:
	CDeviceResource()
		: m_pNativeResource(nullptr)
		, m_eNativeFormat(DXGI_FORMAT_UNKNOWN) {}
	virtual ~CDeviceResource()
	{ Cleanup();  }

	SResourceLayout     GetLayout() const;

	inline D3DResource* GetNativeResource() const
	{
		return m_pNativeResource;
	}

	inline D3DFormat GetNativeFormat() const
	{
		return m_eNativeFormat;
	}

	uint32 GetFlags() const
	{
		return m_eFlags;
	}

	bool IsWritable() const
	{
		return (!!(m_eFlags & (CDeviceObjectFactory::BIND_DEPTH_STENCIL | CDeviceObjectFactory::BIND_RENDER_TARGET | CDeviceObjectFactory::USAGE_UAV_READWRITE)));
	}

	bool IsResolvable() const
	{
		return (!!(m_eFlags & (CDeviceObjectFactory::BIND_DEPTH_STENCIL))) | m_bIsMSAA;
	}

	enum ESubstitutionResult
	{
		eSubResult_Kept,
		eSubResult_Substituted,
		eSubResult_Failed
	};

	ESubstitutionResult SubstituteUsedResource();

protected:
	enum
	{
		eSubResource_Mips,
		eSubResource_Slices,
		eSubResource_Num
	};

	D3DResource*  m_pNativeResource;
	D3DFormat     m_eNativeFormat;
	uint32        m_eFlags;
	buffer_size_t m_resourceElements; // For buffers: in bytes; for textures: in texels
	uint16        m_subResources[eSubResource_Num];
	ETEX_Type     m_eTT;
	bool          m_bFilterable : 1;
	bool          m_bIsSrgb     : 1;
	bool          m_bAllowSrgb  : 1;
	bool          m_bIsMSAA     : 1;

	int32 Cleanup();

	////////////////////////////////////////////////////////////////////////////
	// ResourceView API
	PlaceComposedDeviceObjectManagementAPI(ResourceView, false, CDeviceResource, 0);

public:
	// Helper for type-casting
	ILINE D3DShaderResource* GetOrCreateSRV(const SResourceView pView) { return reinterpret_cast<D3DShaderResource*>(LookupResourceView(GetOrCreateResourceViewHandle(pView)).second); }
	ILINE D3DSurface*        GetOrCreateRTV(const SResourceView pView) { return reinterpret_cast<D3DSurface*>(LookupResourceView(GetOrCreateResourceViewHandle(pView)).second); }
	ILINE D3DDepthSurface*   GetOrCreateDSV(const SResourceView pView) { return reinterpret_cast<D3DDepthSurface*>(LookupResourceView(GetOrCreateResourceViewHandle(pView)).second); }
	ILINE D3DUAV*            GetOrCreateUAV(const SResourceView pView) { return reinterpret_cast<D3DUAV*>(LookupResourceView(GetOrCreateResourceViewHandle(pView)).second); }

	ILINE D3DShaderResource* LookupSRV(ResourceViewHandle hView) const { return reinterpret_cast<D3DShaderResource*>(LookupResourceView(hView).second); }
	ILINE D3DSurface*        LookupRTV(ResourceViewHandle hView) const { return reinterpret_cast<D3DSurface*>(LookupResourceView(hView).second); }
	ILINE D3DDepthSurface*   LookupDSV(ResourceViewHandle hView) const { return reinterpret_cast<D3DDepthSurface*>(LookupResourceView(hView).second); }
	ILINE D3DUAV*            LookupUAV(ResourceViewHandle hView) const { return reinterpret_cast<D3DUAV*>(LookupResourceView(hView).second); }
};

////////////////////////////////////////////////////////////////////////////

struct DirtyRECT
{
	RECT   srcRect;
	uint16 dstX;
	uint16 dstY;
};

// properties of the render targets ONLY
struct RenderTargetData
{
	int               m_nRTSetFrameID; // last read access, compare with GetFrameID(false)
	TArray<DirtyRECT> m_DirtyRects;
	struct
	{
		uint8 m_nMSAASamples : 4;
		uint8 m_nMSAAQuality : 4;
	};

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	int32           m_nESRAMOffset;
#endif
	CDeviceTexture* m_pDeviceTextureMSAA;

	RenderTargetData()
	{
		memset(this, 0, sizeof(*this));
		m_nRTSetFrameID = -1;
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
		m_nESRAMOffset = SKIP_ESRAM;
#endif
	}

	~RenderTargetData();
};

////////////////////////////////////////////////////////////////////////////

struct SBufferLayout
{
	DXGI_FORMAT   m_eFormat;

	buffer_size_t m_elementCount;
	uint16        m_elementSize;

	uint32        m_eFlags;           // e.g. CDeviceObjectFactory::BIND_VERTEX_BUFFER

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	int32         m_eSRAMOffset;
#endif
};

class CDeviceBuffer : public CDeviceResource
{
	friend class CDeviceObjectFactory;

	// for native hand-made buffers
	size_t m_nBaseAllocatedSize;

	bool   m_bNoDelete;

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
	void* m_handleMGPU;
#endif
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && DEVRES_USE_PINNING
	SGPUMemHdl               m_gpuHdl;
#endif
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
	const SDeviceBufferDesc* m_pLayout;
#endif

public:
	static CDeviceBuffer*    Create(const SBufferLayout& pLayout, const void* pData);
	static CDeviceBuffer*    Associate(const SBufferLayout& pLayout, D3DResource* pBuf);

	SBufferLayout            GetLayout() const;
	// This calculates the hardware alignments of a texture resource, respecting block-compression and tiling mode (typeStride will round up if a fraction)
	SResourceMemoryAlignment GetAlignment() const;
	SResourceDimension       GetDimension() const;

	inline D3DBaseBuffer*    GetBaseBuffer() const
	{
		return reinterpret_cast<D3DBaseBuffer*>(GetNativeResource());
	}
	inline D3DBuffer* GetBuffer() const
	{
		return reinterpret_cast<D3DBuffer*>(GetBaseBuffer());
	}

	void Unbind()
	{
		// Not implemented on any platform
	}

	virtual ~CDeviceBuffer();

	size_t GetDeviceSize() const
	{
		return m_nBaseAllocatedSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	int32 Release()
	{
		int32 nRef = Cleanup();

		assert(nRef >= 0);

		if (nRef == 0 && !m_bNoDelete)
		{
			delete this;
		}

		return nRef;
	}

	void SetNoDelete(bool noDelete)
	{
		m_bNoDelete = noDelete;
	}

private:
	CDeviceBuffer() : m_nBaseAllocatedSize(0), m_bNoDelete(false)
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
		, m_handleMGPU(NULL)
#endif
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
		, m_pLayout(NULL)
#endif
	{
	}

	CDeviceBuffer(const CDeviceBuffer&);
	CDeviceBuffer& operator=(const CDeviceBuffer&);

private:
	int32 Cleanup();
};

////////////////////////////////////////////////////////////////////////////

struct STextureLayout
{
	// TODO: optimize size/alignments

	// 64bits, [0,64)
	const SPixFormat* m_pPixelFormat;

	// 32bits, [64,96)
	uint16            m_nWidth;
	uint16            m_nHeight;
	uint16            m_nDepth;
	uint16            m_nArraySize;
	int8              m_nMips;

	// 32bits, [96,128)
	ETEX_Format       m_eSrcFormat;   // TODO: remove
	ETEX_Format       m_eDstFormat;
	ETEX_Type         m_eTT;
	
	// 32bits, [128,160)
	uint32            m_eFlags;       // CDeviceObjectFactory::BIND_RENDER_TARGET etc.
	// 96bits, [160,256)
	bool              m_bIsSRGB;

	// 128bits, [256,384)
	ColorF            m_cClearColor;

	// 32bits, [384,416)
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	int32             m_nESRAMOffset;
#endif
};

class CDeviceTexture : public CDeviceResource
{
	friend class CDeviceObjectFactory;

	// for native hand-made textures
	size_t m_nBaseAllocatedSize;

	bool   m_bNoDelete;
	bool   m_bCube;

#ifdef DEVRES_USE_STAGING_POOL
	D3DResource*      m_pStagingResource[2];
	void*             m_pStagingMemory[2];
	DeviceFenceHandle m_hStagingFence[2];
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
	void*                     m_handleMGPU;
#endif
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && DEVRES_USE_PINNING
	SGPUMemHdl                m_gpuHdl;
#endif
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
	uint32                    m_nBaseAddressInvalidated;
	const SDeviceTextureDesc* m_pLayout;
#endif

#if defined(DEVICE_TEXTURE_STORE_OWNER)
	CTexture*                 m_pDebugOwner;
#endif

	RenderTargetData*         m_pRenderTargetData;

public:
	static CDeviceTexture*   Create(const STextureLayout& pLayout, const STexturePayload* pPayload);
	static CDeviceTexture*   Associate(const STextureLayout& pLayout, D3DResource* pTex);

	STextureLayout           GetLayout() const;
	static STextureLayout    GetLayout(D3DBaseView* pView); // Layout object adjusted to include only the sub-resources in the view.

	// This calculates the hardware alignments of a texture resource, respecting block-compression and tiling mode (typeStride will round up if a fraction)
	SResourceMemoryAlignment GetAlignment(uint8 mip = 0, uint8 slices = 0) const;
	SResourceDimension       GetDimension(uint8 mip = 0, uint8 slices = 0) const;

	inline D3DBaseTexture*   GetBaseTexture() const
	{
		return reinterpret_cast<D3DBaseTexture*>(GetNativeResource());
	}
	D3DLookupTexture* GetLookupTexture() const
	{
		return (D3DLookupTexture*)GetBaseTexture();
	}
	D3DTexture* Get2DTexture() const
	{
		return (D3DTexture*)GetBaseTexture();
	}
	D3DCubeTexture* GetCubeTexture() const
	{
		return (D3DCubeTexture*)GetBaseTexture();
	}
	D3DVolumeTexture* GetVolumeTexture() const
	{
		return (D3DVolumeTexture*)GetBaseTexture();
	}

	CDeviceTexture* GetMSAATexture() const
	{
		if (m_pRenderTargetData)
			return m_pRenderTargetData->m_pDeviceTextureMSAA;
		return nullptr;
	}

	bool IsMSAAChanged();
	void AddDirtRect(RECT& rcSrc, uint32 dstX, uint32 dstY);

	bool IsCube() const
	{
		return m_bCube;
	}

#if defined(DEVICE_TEXTURE_STORE_OWNER)
	void SetOwner(CTexture* pOwner) { m_pDebugOwner = pOwner; }
#else
	void SetOwner(CTexture* pOwner) {}
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
	void DisableMgpuSync();
	void MgpuResourceUpdate(bool bUpdating = true);
#endif

	virtual ~CDeviceTexture();

	typedef std::function<bool (void*, uint32, uint32)> StagingHook;

	void   DownloadToStagingResource(uint32 nSubRes, StagingHook cbTransfer);
	void   DownloadToStagingResource(uint32 nSubRes);
	void   UploadFromStagingResource(uint32 nSubRes, StagingHook cbTransfer);
	void   UploadFromStagingResource(uint32 nSubRes);
	void   AccessCurrStagingResource(uint32 nSubRes, bool forUpload, StagingHook cbTransfer);
	bool   AccessCurrStagingResource(uint32 nSubRes, bool forUpload);

	size_t GetDeviceSize() const
	{
		return m_nBaseAllocatedSize;
	}

	static uint32 TextureDataSize(D3DBaseView* pView);
	static uint32 TextureDataSize(D3DBaseView* pView, const uint numRects, const RECT* pRects);
	static uint32 TextureDataSize(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nSlices, const ETEX_Format eTF, ETEX_TileMode eTM, uint32 eFlags);

#if DEVRES_USE_PINNING
	void* WeakPin();
	void* Pin();
	void Unpin();

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	bool IsMoveable() const { return m_gpuHdl.IsValid() && !m_gpuHdl.IsFixed(); }
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
	void GpuPin();
	void GpuUnpin(ID3DXboxPerformanceContext* pCtx);
	void GpuUnpin(ID3D11DmaEngineContextX* pCtx);
#endif
#endif

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	// TODO: -> GetLayout()
	void SetESRAMOffset(int32 offset)
	{
		if (m_pRenderTargetData)
		{
			m_pRenderTargetData->m_nESRAMOffset = offset;
		}
	}

	int32 GetESRAMOffset()
	{
		if (m_pRenderTargetData)
		{
			return m_pRenderTargetData->m_nESRAMOffset;
		}

		return SKIP_ESRAM;
	}
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
	void InitD3DTexture();
	bool IsInPool() const { return m_gpuHdl.IsValid(); }
	size_t GetSurfaceOffset(int nMip, int nSlice) const
	{
		const XG_RESOURCE_LAYOUT& l = m_pLayout->layout;
		return l.Plane[0].MipLayout[nMip].OffsetBytes + l.Plane[0].MipLayout[nMip].Slice2DSizeBytes * nSlice;
	}
	size_t GetSurfaceSize(int nMip) const
	{
		const XG_RESOURCE_LAYOUT& l = m_pLayout->layout;
		return l.Plane[0].MipLayout[nMip].Slice2DSizeBytes;
	}
	XG_TILE_MODE GetXGTileMode() const
	{
		const XG_RESOURCE_LAYOUT& l = m_pLayout->layout;
		return l.Plane[0].MipLayout[0].TileMode;
	}
	void                      GPUFlush();
	const SDeviceTextureDesc* GetTDesc() const
	{
		return m_pLayout;
	}

	void ReplaceTexture(ID3D11Texture2D* pReplacement);
	uint32 GetBaseAddressInvalidated() const { return m_nBaseAddressInvalidated; }
#endif

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	int32 Release()
	{
		int32 nRef = Cleanup();

#if !defined(_RELEASE) && defined(_DEBUG)
		IF (nRef < 0, 0)
			__debugbreak();
#endif
		if (nRef == 0 && !m_bNoDelete)
		{
			delete this;
		}

		return nRef;
	}

	void SetNoDelete(bool noDelete)
	{
		m_bNoDelete = noDelete;
	}

private:
	CDeviceTexture() 
		: m_nBaseAllocatedSize(0)
		, m_bNoDelete(false)
		, m_bCube(false)
		, m_pRenderTargetData(nullptr)
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
		, m_handleMGPU(NULL)
#endif
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
		, m_pLayout(NULL)
		, m_nBaseAddressInvalidated(0)
#endif
#if defined(DEVICE_TEXTURE_STORE_OWNER)
		, m_pDebugOwner(NULL)
#endif
	{
#ifdef DEVRES_USE_STAGING_POOL
		m_pStagingResource[0] = m_pStagingResource[1] = nullptr;
		m_pStagingMemory  [0] = m_pStagingMemory  [1] = nullptr;
		m_hStagingFence   [0] = m_hStagingFence   [1] = 0;
#endif
	}

	CDeviceTexture(const CDeviceTexture&);
	CDeviceTexture& operator=(const CDeviceTexture&);

private:
	int32 Cleanup();
};

#ifdef DEVRES_USE_PINNING
class CDeviceTexturePin
{
public:
	CDeviceTexturePin(CDeviceTexture* pDevTex)
		: m_pTexture(pDevTex)
	{
		m_pBaseAddress = pDevTex->Pin();
	}

	~CDeviceTexturePin()
	{
		m_pTexture->Unpin();
	}

	void* GetBaseAddress() const { return m_pBaseAddress; }

#if CRY_PLATFORM_DURANGO
	void* GetSurfaceAddress(int nMip, int nSlice) const
	{
		return (char*)m_pBaseAddress + m_pTexture->GetSurfaceOffset(nMip, nSlice);
	}
#endif

private:
	CDeviceTexturePin(const CDeviceTexturePin&);
	CDeviceTexturePin& operator=(const CDeviceTexturePin&);

private:
	CDeviceTexture* m_pTexture;
	void* m_pBaseAddress;
};
#endif

#if DEVRES_USE_PINNING && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
class CDeviceTextureGpuPin
{
public:
	CDeviceTextureGpuPin(ID3DXboxPerformanceContext* pCtx, CDeviceTexture* pDevTex)
		: m_pCtx(pCtx)
		, m_pTexture(pDevTex)
	{
		pDevTex->GpuPin();
	}

	CDeviceTextureGpuPin(CCryPerformanceDeviceContextWrapper& ctx, CDeviceTexture* pDevTex)
		: m_pCtx(ctx.GetRealPerformanceDeviceContext())
		, m_pTexture(pDevTex)
	{
		pDevTex->GpuPin();
	}

	~CDeviceTextureGpuPin()
	{
		m_pTexture->GpuUnpin(m_pCtx);
	}

private:
	CDeviceTextureGpuPin(const CDeviceTextureGpuPin&);
	CDeviceTextureGpuPin& operator = (const CDeviceTextureGpuPin&);

private:
	ID3DXboxPerformanceContext* m_pCtx;
	CDeviceTexture* m_pTexture;
};


class CDeviceTextureDmaPin
{
public:
	CDeviceTextureDmaPin(ID3D11DmaEngineContextX* pDmaCtx, CDeviceTexture* pDevTex)
		: m_pCtx(pDmaCtx)
		, m_pTexture(pDevTex)
	{
		m_pTexture->GpuPin();
	}

	~CDeviceTextureDmaPin()
	{
		m_pTexture->GpuUnpin(m_pCtx);
	}

private:
	CDeviceTextureDmaPin(const CDeviceTextureDmaPin&);
	CDeviceTextureDmaPin& operator = (const CDeviceTextureDmaPin&);

private:
	ID3D11DmaEngineContextX* m_pCtx;
	CDeviceTexture* m_pTexture;
};

#define PIN_CONCAT_(a,b) a ## b
#define PIN_CONCAT(a,b) PIN_CONCAT_(a,b)
#define GPUPIN_DEVICE_TEXTURE(...) CDeviceTextureGpuPin PIN_CONCAT(gpupin, __LINE__) (__VA_ARGS__)
#define DMAPIN_DEVICE_TEXTURE(...) CDeviceTextureDmaPin PIN_CONCAT(dmapin, __LINE__) (__VA_ARGS__)

#ifndef _RELEASE
//#define ASSERT_DEVICE_TEXTURE_IS_FIXED(tex) do { if (tex->IsMoveable()) __debugbreak(); } while (0)
#endif
#endif

#ifndef GPUPIN_DEVICE_TEXTURE
#define GPUPIN_DEVICE_TEXTURE(...) do {} while (0)
#endif

#ifndef DMAPIN_DEVICE_TEXTURE
#define DMAPIN_DEVICE_TEXTURE(...) do {} while (0)
#endif

#ifndef ASSERT_DEVICE_TEXTURE_IS_FIXED
#define ASSERT_DEVICE_TEXTURE_IS_FIXED(...) do {} while(0)
#endif

class CDeviceVertexBuffer : public CDeviceResource
{

};

class CDeviceIndexBuffer : public CDeviceResource
{

};
