// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DriverD3D.h"
#include "DeviceResources.h"

#if CRY_RENDERER_GNM
	#include "GNM/DeviceResources_GNM.inl"
#elif (CRY_RENDERER_DIRECT3D >= 120)
	#include "D3D12/DeviceResources_D3D12.inl"

	#if CRY_PLATFORM_DURANGO
		#include "D3D12/DeviceResources_D3D12_Durango.inl"
	#endif

#elif (CRY_RENDERER_DIRECT3D >= 110)
	#include "D3D11/DeviceResources_D3D11.inl"

	#if CRY_PLATFORM_DURANGO
		#include "D3D11/DeviceResources_D3D11_Durango.inl"
	#endif

	#if defined(USE_NV_API)
		#include "D3D11/DeviceResources_D3D11_NVAPI.inl"
	#endif
#elif (CRY_RENDERER_VULKAN >= 10)
	#include "Vulkan/DeviceResources_Vulkan.inl"
#endif

////////////////////////////////////////////////////////////////////////////
// ResourceView API

void CDeviceResource::AllocatePredefinedResourceViews()
{
	const uint32 eFlags  = m_eFlags; 
	const bool bIsShaderResource   = !!(eFlags & (CDeviceObjectFactory::BIND_SHADER_RESOURCE /*| CDeviceObjectFactory::BIND_VERTEX_BUFFER | CDeviceObjectFactory::BIND_INDEX_BUFFER*/));
	const bool bIsRasterizerTarget = !!(eFlags & (CDeviceObjectFactory::BIND_DEPTH_STENCIL | CDeviceObjectFactory::BIND_RENDER_TARGET));
	const bool bIsDepthStencil     = !!(eFlags & (CDeviceObjectFactory::BIND_DEPTH_STENCIL));
	const bool bIsUnordered        = !!(eFlags & (CDeviceObjectFactory::BIND_UNORDERED_ACCESS));
	const bool bIsStructured       = !!(eFlags & (CDeviceObjectFactory::USAGE_STRUCTURED));
	const bool bIsRaw              = !!(eFlags & (CDeviceObjectFactory::USAGE_RAW));
	const bool bAllowWrite         = !!(eFlags & (CDeviceObjectFactory::USAGE_UAV_READWRITE));
	const DXGI_FORMAT eFormatT  =                                                                           m_eNativeFormat;
	const DXGI_FORMAT eFormatD  = bIsDepthStencil ? DeviceFormats::ConvertToDepthOnly   (m_eNativeFormat) : m_eNativeFormat;
	const DXGI_FORMAT eFormatS  = bIsDepthStencil ? DeviceFormats::ConvertToStencilOnly (m_eNativeFormat) : m_eNativeFormat;
	const DXGI_FORMAT eFormatDS = bIsDepthStencil ? DeviceFormats::ConvertToDepthStencil(m_eNativeFormat) : m_eNativeFormat;
	const bool bIsSRGB     = m_bIsSrgb;
	const bool bAllowSrgb  = m_bAllowSrgb;
	const bool bIsMSAA     = m_bIsMSAA;
	const SResourceLayout Layout = GetLayout();
	const UINT nMips     = ~0; // m_nSubResources[eSubResource_Mips];
	const UINT nSlices   = ~0; // m_nSubResources[eSubResource_Slices];
	const UINT nElements = bIsRaw ? Layout.m_byteCount : Layout.m_elementCount;

	// TODO: add a FT_USAGE_SHADERRESOURCE and possibly stop making SRVs of RTs and DSs, because compression ratio is lowered for SRVs on those
	// TODO: make MSAA and sRGB flags (less arguments, easier to switch)
	ReserveResourceViews(EDefaultResourceViews::PreAllocated);

	SResourceView RasterizerTarget;
	SResourceView UnorderedAccess;
	SResourceView Default;
	SResourceView Alternative;

	if (m_bFilterable)
	{
		RasterizerTarget = bIsDepthStencil 
						 ? SResourceView::DepthStencilView   (eFormatDS, 0, nSlices, 0, bIsMSAA)
						 : SResourceView::RenderTargetView   (eFormatT , 0, nSlices, 0, bIsMSAA);
		UnorderedAccess  = SResourceView::UnorderedAccessView(eFormatT , 0, nSlices, 0,                                                    bAllowWrite     * SResourceView::eUAV_ReadWrite  );
		Default          = SResourceView::ShaderResourceView (eFormatD , 0, nSlices, 0, nMips, bIsDepthStencil ? false : bIsSRGB, bIsMSAA, bIsDepthStencil * SResourceView::eSRV_DepthOnly  );
		Alternative      = SResourceView::ShaderResourceView (eFormatS , 0, nSlices, 0, nMips, bIsDepthStencil ? false : true   , bIsMSAA, bIsDepthStencil * SResourceView::eSRV_StencilOnly);
	}
	else
	{
		RasterizerTarget = bIsDepthStencil 
						 ? SResourceView::DepthStencilRawView   (eFormatDS, 0, nElements)
						 : SResourceView::RenderTargetRawView   (eFormatT , 0, nElements);
		UnorderedAccess  = SResourceView::UnorderedAccessRawView(eFormatT , 0, nElements, bAllowWrite     * SResourceView::eUAV_ReadWrite  );
		Default          = SResourceView::ShaderResourceRawView (eFormatD , 0, nElements, bIsDepthStencil * SResourceView::eSRV_DepthOnly  );
		Alternative      = SResourceView::ShaderResourceRawView (eFormatS , 0, nElements, bIsDepthStencil * SResourceView::eSRV_StencilOnly);
	}

	const bool bDefaultAvailable     = (bIsStructured || (eFormatD != DXGI_FORMAT_UNKNOWN)) && (bIsShaderResource);
	const bool bAlternativeAvailable = (bIsStructured || (eFormatS != DXGI_FORMAT_UNKNOWN)) && (bIsShaderResource) && (bIsDepthStencil || bAllowSrgb) && (Default != Alternative);

	ResourceViewHandle a = bDefaultAvailable     ? GetOrCreateResourceViewHandle(Default         ) : ReserveResourceViewHandle(Default         ); assert(a == EDefaultResourceViews::Default         );
	ResourceViewHandle b = bAlternativeAvailable ? GetOrCreateResourceViewHandle(Alternative     ) : ReserveResourceViewHandle(Alternative     ); assert(b == EDefaultResourceViews::Alternative     );
	ResourceViewHandle c = bIsRasterizerTarget   ? GetOrCreateResourceViewHandle(RasterizerTarget) : ReserveResourceViewHandle(RasterizerTarget); assert(c == EDefaultResourceViews::RasterizerTarget);
	ResourceViewHandle e = bIsUnordered          ? GetOrCreateResourceViewHandle(UnorderedAccess ) : ReserveResourceViewHandle(UnorderedAccess ); assert(e == EDefaultResourceViews::UnorderedAccess );

	// Alias Default to Alternative if it's not suppose to be read in an alternative way
	if (bDefaultAvailable && !bAlternativeAvailable)
	{
		auto& pElement  = LookupResourceView(EDefaultResourceViews::Alternative);
		pElement.second = LookupResourceView(EDefaultResourceViews::Default).second;
		if (pElement.second)
			pElement.second->AddRef();
	}
}

int32 CDeviceResource::Cleanup()
{
	ReleaseResourceViews();

	// Gracefully deal with NULL-resources which are nullptr
	int32 nRef = m_resourceElements ? -1 : 0; // -!!bool

	if (m_pNativeResource)
	{
		// Figure out current ref-count
		nRef = m_pNativeResource->AddRef();
		nRef = m_pNativeResource->Release();

		// NOTE: Heap are ref-counting (first register, then release yourself)
		if (m_eFlags & CDeviceObjectFactory::USAGE_HIFREQ_HEAP)
			GetDeviceObjectFactory().RecycleResource(m_pNativeResource);
		else
			GetDeviceObjectFactory().ReleaseResource(m_pNativeResource);

		// NOTE: CDeviceResource might be shared, take care the texture-pointer stays valid for the other aliases
		if (nRef == 1)
		{
			nRef = 0;

			m_pNativeResource = nullptr;
		}
	}

	return nRef;
}

////////////////////////////////////////////////////////////////////////////

RenderTargetData::~RenderTargetData()
{
	CDeviceTexture* pTexMSAA = (CDeviceTexture*)m_pDeviceTextureMSAA;
	SAFE_RELEASE(pTexMSAA);
}

////////////////////////////////////////////////////////////////////////////

CDeviceBuffer* CDeviceBuffer::Create(const SBufferLayout& pLayout, const void* pData)
{
	CDeviceBuffer* pDevBuffer = nullptr;
	D3DBuffer* pBuffer = nullptr;

	int32 nESRAMOffset = SKIP_ESRAM;
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	nESRAMOffset = pLayout.m_eSRAMOffset;
#endif

	buffer_size_t elementSize = pLayout.m_elementSize;
	buffer_size_t elementCount = pLayout.m_elementCount;
	uint32 eFlags = pLayout.m_eFlags; // No FT_ flags used here, no mapping of flags required
	buffer_size_t nSize = elementSize * elementCount;
	CRY_ASSERT(elementSize != 0);

	if (!nSize)
	{
		// No size trigger NULL resource creation, which is a perfectly valid object
		pBuffer = static_cast<D3DBuffer*>(GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_BUFFER));
	}
	else
	{
		static const uint32 bindFlagMask =
			CDeviceObjectFactory::BIND_DEPTH_STENCIL |
			CDeviceObjectFactory::BIND_RENDER_TARGET |
			CDeviceObjectFactory::BIND_UNORDERED_ACCESS |
			CDeviceObjectFactory::BIND_VERTEX_BUFFER |
			CDeviceObjectFactory::BIND_INDEX_BUFFER |
			CDeviceObjectFactory::BIND_CONSTANT_BUFFER |
			CDeviceObjectFactory::BIND_SHADER_RESOURCE |
			CDeviceObjectFactory::BIND_STREAM_OUTPUT;

		const HRESULT hr = GetDeviceObjectFactory().CreateBuffer(elementCount, elementSize, eFlags & ~bindFlagMask, eFlags & bindFlagMask, &pBuffer, pData);
		if (FAILED(hr))
		{
			assert(0);
			return nullptr;
		}
	}

	pDevBuffer = new CDeviceBuffer();
	pDevBuffer->m_pNativeResource = pBuffer;

	pDevBuffer->m_eNativeFormat = pLayout.m_eFormat;
	pDevBuffer->m_resourceElements = elementCount * elementSize;
	pDevBuffer->m_subResources[eSubResource_Mips] = 0;
	pDevBuffer->m_subResources[eSubResource_Slices] = 0; 
	pDevBuffer->m_eTT = eTT_User;
	pDevBuffer->m_bFilterable = false;
	pDevBuffer->m_bIsSrgb = false;
	pDevBuffer->m_bAllowSrgb = false;
	pDevBuffer->m_bIsMSAA = false;
	pDevBuffer->m_eFlags = pLayout.m_eFlags;
	pDevBuffer->AllocatePredefinedResourceViews();

	return pDevBuffer;
}

CDeviceBuffer* CDeviceBuffer::Associate(const SBufferLayout& pLayout, D3DResource* pBuf)
{
	abort();
}

int32 CDeviceBuffer::Cleanup()
{
	Unbind();

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	void* pBackingStorage = m_pNativeResource ? CDeviceObjectFactory::GetBackingStorage((ID3D11Buffer*)m_pNativeResource) : nullptr;
#endif
	int32 nRef = CDeviceResource::Cleanup();
#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	if (!nRef && pBackingStorage)
	{
		CDeviceObjectFactory::FreeBackingStorage(pBackingStorage);
	}
#endif

	if (nRef != -1)
	{
#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		m_handleMGPU = nullptr;
#endif
	}

	return nRef;
}

CDeviceBuffer::~CDeviceBuffer()
{
	Cleanup();
}

////////////////////////////////////////////////////////////////////////////

CDeviceTexture* CDeviceTexture::Create(const STextureLayout& pLayout, const STexturePayload* pPayload)
{
	CDeviceTexture* pDevTexture = nullptr;
	RenderTargetData* pRenderTargetData = nullptr;
	HRESULT hr = S_OK;

	int32 nESRAMOffset = SKIP_ESRAM;
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	nESRAMOffset = pLayout.m_nESRAMOffset;
#endif

	bool bIsSRGB = pLayout.m_bIsSRGB;
#if CRY_PLATFORM_MAC
	if (!(pLayout.m_eFlags & FT_FROMIMAGE) && IsBlockCompressed(m_eTFDst))
		bIsSRGB = true;
#endif

	CD3D9Renderer* r = gcpRendD3D;
	uint32 nWdt = pLayout.m_nWidth;
	uint32 nHgt = pLayout.m_nHeight;
	uint32 nDepth = pLayout.m_nDepth;
	uint32 nMips = pLayout.m_nMips;
	uint32 nArraySize = pLayout.m_nArraySize;
	D3DFormat D3DFmt = pLayout.m_pPixelFormat->DeviceFormat;

	uint32 eFlags = 0;
	uint32 stagingFlags = 0;
	if (!(pLayout.m_eFlags & FT_DONT_READ))
		eFlags |= CDeviceObjectFactory::BIND_SHADER_RESOURCE;
	if (pLayout.m_eFlags & FT_USAGE_DEPTHSTENCIL)
		eFlags |= CDeviceObjectFactory::BIND_DEPTH_STENCIL;
	if (pLayout.m_eFlags & FT_USAGE_RENDERTARGET)
		eFlags |= CDeviceObjectFactory::BIND_RENDER_TARGET;
	if (pLayout.m_eFlags & FT_USAGE_TEMPORARY)
		eFlags |= CDeviceObjectFactory::USAGE_HIFREQ_HEAP;
	if (pLayout.m_eFlags & FT_STAGE_READBACK)
		stagingFlags |= CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_READ;
	if (pLayout.m_eFlags & FT_STAGE_UPLOAD)
		stagingFlags |= CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_WRITE;
	if (!(pLayout.m_eFlags & FT_DONT_STREAM))
		eFlags |= CDeviceObjectFactory::USAGE_STREAMING;
	if (pLayout.m_eFlags & FT_STREAMED_FADEIN)
		eFlags |= CDeviceObjectFactory::USAGE_LODABLE;

	assert(nWdt && nHgt && nDepth && nMips && nArraySize);
	if (!nWdt | !nHgt | !nDepth | !nMips | !nArraySize)
	{
		// No size trigger NULL resource creation, which is a perfectly valid object
		pDevTexture = new CDeviceTexture();
		if (pLayout.m_eTT == eTT_1D)
			pDevTexture->m_pNativeResource = GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_TEXTURE1D);
		else if (pLayout.m_eTT == eTT_2D)
			pDevTexture->m_pNativeResource = GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_TEXTURE2D);
		else if (pLayout.m_eTT == eTT_3D)
			pDevTexture->m_pNativeResource = GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_TEXTURE3D);
	}
	else
	{
		if (pLayout.m_eFlags & FT_USAGE_UNORDERED_ACCESS
#if defined(SUPPORT_DEVICE_INFO)
			&& r->DevInfo().FeatureLevel() >= D3D_FEATURE_LEVEL_11_0
#endif //defined(SUPPORT_DEVICE_INFO)
			)
			eFlags |= CDeviceObjectFactory::BIND_UNORDERED_ACCESS;
		if (pLayout.m_eFlags & FT_USAGE_UAV_RWTEXTURE)
			eFlags |= CDeviceObjectFactory::USAGE_UAV_READWRITE;

		if (pLayout.m_eFlags & FT_FORCE_MIPS)
		{
			eFlags |= CDeviceObjectFactory::USAGE_AUTOGENMIPS;
			if (nMips <= 1)
				nMips = max(1, CTexture::CalcNumMips(nWdt, nHgt) - 2);
			CRY_ASSERT(!pPayload); // Not allowed as this is not what the outside expects
		}

		if (pLayout.m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS))
		{
			pRenderTargetData = new RenderTargetData();
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
			pRenderTargetData->m_nESRAMOffset = nESRAMOffset;
#endif
		}

		DXGI_FORMAT nFormatOrig = D3DFmt;
		DXGI_FORMAT nFormatSRGB = D3DFmt;

		{
			bIsSRGB &= (pLayout.m_pPixelFormat->Options & FMTSUPPORT_SRGB) && (pLayout.m_eFlags & (FT_USAGE_MSAA | FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL)) == 0;
			if (bIsSRGB || (pLayout.m_eFlags & FT_USAGE_ALLOWREADSRGB))
				nFormatSRGB = DeviceFormats::ConvertToSRGB(D3DFmt);

			if (bIsSRGB)
				D3DFmt = nFormatSRGB;

			// must use typeless format to allow runtime casting
#if !CRY_PLATFORM_MAC
			if (pLayout.m_eFlags & FT_USAGE_ALLOWREADSRGB)
				D3DFmt = DeviceFormats::ConvertToTypeless(D3DFmt);
#endif
		}

		if (pLayout.m_eTT == eTT_2D || pLayout.m_eTT == eTT_2DArray)
		{
			if (pLayout.m_eFlags & FT_USAGE_MSAA)
			{
				STexturePayload TI;

				assert(pRenderTargetData);
				pRenderTargetData->m_nMSAASamples = (uint8)gRenDev->GetMSAA().Type;
				pRenderTargetData->m_nMSAAQuality = (uint8)gRenDev->GetMSAA().Quality;

				TI.m_nDstMSAASamples = pRenderTargetData->m_nMSAASamples;
				TI.m_nDstMSAAQuality = pRenderTargetData->m_nMSAAQuality;

				hr = GetDeviceObjectFactory().Create2DTexture(nWdt, nHgt, nMips, nArraySize, eFlags, pLayout.m_cClearColor, D3DFmt, &pRenderTargetData->m_pDeviceTextureMSAA, &TI);

				assert(SUCCEEDED(hr));

				TI = STexturePayload();
			}

			{
				hr = GetDeviceObjectFactory().Create2DTexture(nWdt, nHgt, nMips, nArraySize, eFlags, pLayout.m_cClearColor, D3DFmt, &pDevTexture, pPayload, nESRAMOffset);
				if (!FAILED(hr) && pDevTexture)
					pDevTexture->SetNoDelete(!!(pLayout.m_eFlags & FT_DONT_RELEASE));
			}

			if (FAILED(hr))
			{
				assert(0);
				return nullptr;
			}

			// Restore format
			if (pLayout.m_eFlags & FT_USAGE_ALLOWREADSRGB)
				D3DFmt = nFormatOrig;
		}
		else if (pLayout.m_eTT == eTT_Cube || pLayout.m_eTT == eTT_CubeArray)
		{
			if (pLayout.m_eFlags & FT_USAGE_MSAA)
			{
				STexturePayload TI;

				assert(pRenderTargetData);
				pRenderTargetData->m_nMSAASamples = (uint8)gRenDev->GetMSAA().Type;
				pRenderTargetData->m_nMSAAQuality = (uint8)gRenDev->GetMSAA().Quality;

				TI.m_nDstMSAASamples = pRenderTargetData->m_nMSAASamples;
				TI.m_nDstMSAAQuality = pRenderTargetData->m_nMSAAQuality;

				hr = GetDeviceObjectFactory().CreateCubeTexture(nWdt, nMips, pLayout.m_nArraySize, eFlags, pLayout.m_cClearColor, D3DFmt, &pRenderTargetData->m_pDeviceTextureMSAA, &TI);

				assert(SUCCEEDED(hr));

				TI = STexturePayload();
			}

			{
				hr = GetDeviceObjectFactory().CreateCubeTexture(nWdt, nMips, pLayout.m_nArraySize, eFlags, pLayout.m_cClearColor, D3DFmt, &pDevTexture, pPayload);
				if (!FAILED(hr) && pDevTexture)
					pDevTexture->SetNoDelete(!!(pLayout.m_eFlags & FT_DONT_RELEASE));
			}

			if (FAILED(hr))
			{
				assert(0);
				return nullptr;
			}

			if (pLayout.m_eFlags & FT_USAGE_ALLOWREADSRGB)
				D3DFmt = nFormatOrig;
		}
		else if (pLayout.m_eTT == eTT_3D)
		{
			if (pLayout.m_eFlags & FT_USAGE_MSAA)
			{
				STexturePayload TI;

				assert(pRenderTargetData);
				TI.m_nDstMSAASamples = pRenderTargetData->m_nMSAASamples;
				TI.m_nDstMSAAQuality = pRenderTargetData->m_nMSAAQuality;

				hr = GetDeviceObjectFactory().CreateVolumeTexture(nWdt, nHgt, pLayout.m_nDepth, nMips, eFlags, pLayout.m_cClearColor, D3DFmt, &pRenderTargetData->m_pDeviceTextureMSAA, &TI);

				assert(SUCCEEDED(hr));

				TI = STexturePayload();
			}

			{
				hr = GetDeviceObjectFactory().CreateVolumeTexture(nWdt, nHgt, nDepth, nMips, eFlags, pLayout.m_cClearColor, D3DFmt, &pDevTexture, pPayload);
				if (!FAILED(hr) && pDevTexture)
					pDevTexture->SetNoDelete(!!(pLayout.m_eFlags & FT_DONT_RELEASE));
			}

			if (FAILED(hr))
			{
				assert(0);
				return nullptr;
			}
		}
		else
		{
			assert(0);
			return nullptr;
		}

#ifdef DEVRES_USE_STAGING_POOL
		if (SUCCEEDED(hr) && (stagingFlags & CDeviceObjectFactory::USAGE_STAGE_ACCESS))
		{
			if (stagingFlags & CDeviceObjectFactory::USAGE_CPU_READ)
				pDevTexture->m_pStagingResource[0] = GetDeviceObjectFactory().AllocateStagingResource(pDevTexture->m_pNativeResource, false, pDevTexture->m_pStagingMemory[0]);
			if (stagingFlags & CDeviceObjectFactory::USAGE_CPU_WRITE)
				pDevTexture->m_pStagingResource[1] = GetDeviceObjectFactory().AllocateStagingResource(pDevTexture->m_pNativeResource, true, pDevTexture->m_pStagingMemory[1]);

			GetDeviceObjectFactory().CreateFence(pDevTexture->m_hStagingFence[0]);
			GetDeviceObjectFactory().CreateFence(pDevTexture->m_hStagingFence[1]);
		}
#endif
	}

	if (pDevTexture)
	{
		pDevTexture->m_pRenderTargetData = pRenderTargetData;
		pDevTexture->m_eNativeFormat = D3DFmt;
		pDevTexture->m_resourceElements = CTexture::TextureDataSize(nWdt, nHgt, nDepth, nMips, nArraySize, eTF_A8);
		pDevTexture->m_subResources[eSubResource_Mips] = nMips;
		pDevTexture->m_subResources[eSubResource_Slices] = nArraySize;
		pDevTexture->m_eTT = pLayout.m_eTT;
		pDevTexture->m_bFilterable = true;
		pDevTexture->m_bIsSrgb = bIsSRGB;
		pDevTexture->m_bAllowSrgb = !!(pLayout.m_eFlags & FT_USAGE_ALLOWREADSRGB);
		pDevTexture->m_bIsMSAA = false;
		pDevTexture->m_eFlags = eFlags | stagingFlags;
		pDevTexture->AllocatePredefinedResourceViews();
	}

	if (pRenderTargetData && pRenderTargetData->m_pDeviceTextureMSAA)
	{
		pRenderTargetData->m_pDeviceTextureMSAA->m_pRenderTargetData = nullptr;
		pRenderTargetData->m_pDeviceTextureMSAA->m_eNativeFormat = D3DFmt;
		pRenderTargetData->m_pDeviceTextureMSAA->m_resourceElements = CTexture::TextureDataSize(nWdt, nHgt, nDepth, nMips, nArraySize, eTF_A8) * pRenderTargetData->m_nMSAASamples;
		pRenderTargetData->m_pDeviceTextureMSAA->m_subResources[eSubResource_Mips] = nMips;
		pRenderTargetData->m_pDeviceTextureMSAA->m_subResources[eSubResource_Slices] = nArraySize;
		pRenderTargetData->m_pDeviceTextureMSAA->m_eTT = pLayout.m_eTT;
		pRenderTargetData->m_pDeviceTextureMSAA->m_bFilterable = true;
		pRenderTargetData->m_pDeviceTextureMSAA->m_bIsSrgb = bIsSRGB;
		pRenderTargetData->m_pDeviceTextureMSAA->m_bAllowSrgb = !!(pLayout.m_eFlags & FT_USAGE_ALLOWREADSRGB);
		pRenderTargetData->m_pDeviceTextureMSAA->m_bIsMSAA = true;
		pRenderTargetData->m_pDeviceTextureMSAA->m_eFlags = eFlags | stagingFlags;
		pRenderTargetData->m_pDeviceTextureMSAA->AllocatePredefinedResourceViews();
	}

	return pDevTexture;
}

CDeviceTexture* CDeviceTexture::Associate(const STextureLayout& pLayout, D3DResource* pTex)
{
	CDeviceTexture* pDevTexture = new CDeviceTexture();
	RenderTargetData* pRenderTargetData = nullptr;

	int32 nESRAMOffset = SKIP_ESRAM;
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	nESRAMOffset = pLayout.m_nESRAMOffset;
#endif

	bool bIsSRGB = pLayout.m_bIsSRGB;
#if CRY_PLATFORM_MAC
	if (!(pLayout.m_eFlags & FT_FROMIMAGE) && IsBlockCompressed(m_eTFDst))
		bIsSRGB = true;
#endif

	CD3D9Renderer* r = gcpRendD3D;
	uint32 nWdt = pLayout.m_nWidth;
	uint32 nHgt = pLayout.m_nHeight;
	uint32 nDepth = pLayout.m_nDepth;
	uint32 nMips = pLayout.m_nMips;
	uint32 nArraySize = pLayout.m_nArraySize;
	D3DFormat D3DFmt = pLayout.m_pPixelFormat->DeviceFormat;

	uint32 eFlags = 0;
	if (!(pLayout.m_eFlags & FT_DONT_READ))
		eFlags |= CDeviceObjectFactory::BIND_SHADER_RESOURCE;
	if (pLayout.m_eFlags & FT_USAGE_DEPTHSTENCIL)
		eFlags |= CDeviceObjectFactory::BIND_DEPTH_STENCIL;
	if (pLayout.m_eFlags & FT_USAGE_RENDERTARGET)
		eFlags |= CDeviceObjectFactory::BIND_RENDER_TARGET;
	if (pLayout.m_eFlags & FT_STAGE_READBACK)
		eFlags |= CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_READ;
	if (pLayout.m_eFlags & FT_STAGE_UPLOAD)
		eFlags |= CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_WRITE;

	assert(nWdt && nHgt && nDepth && nMips && nArraySize);
	if (!nWdt | !nHgt | !nDepth | !nMips | !nArraySize | !pTex)
	{
		// No size trigger NULL resource creation, which is a perfectly valid object
		if (pLayout.m_eTT == eTT_1D)
			pDevTexture->m_pNativeResource = GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_TEXTURE1D);
		else if (pLayout.m_eTT == eTT_2D)
			pDevTexture->m_pNativeResource = GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_TEXTURE2D);
		else if (pLayout.m_eTT == eTT_3D)
			pDevTexture->m_pNativeResource = GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_TEXTURE3D);
	}
	else
	{

		if (pLayout.m_eFlags & FT_USAGE_UNORDERED_ACCESS
#if defined(SUPPORT_DEVICE_INFO)
			&& r->DevInfo().FeatureLevel() >= D3D_FEATURE_LEVEL_11_0
#endif //defined(SUPPORT_DEVICE_INFO)
			)
			eFlags |= CDeviceObjectFactory::BIND_UNORDERED_ACCESS;
		if (pLayout.m_eFlags & FT_USAGE_UAV_RWTEXTURE)
			eFlags |= CDeviceObjectFactory::USAGE_UAV_READWRITE;
		if (pLayout.m_eFlags & FT_FORCE_MIPS)
			eFlags |= CDeviceObjectFactory::USAGE_AUTOGENMIPS;
		if (pLayout.m_eFlags & FT_USAGE_MSAA)
			__debugbreak(); // TODO: one texture given, two textures needed

		if (pLayout.m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS))
		{
			pRenderTargetData = new RenderTargetData();
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
			pRenderTargetData->m_nESRAMOffset = nESRAMOffset;
#endif
		}

		bIsSRGB &= (pLayout.m_pPixelFormat->Options & FMTSUPPORT_SRGB) && (pLayout.m_eFlags & (FT_USAGE_MSAA | FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL)) == 0;

		{
			pDevTexture->m_pNativeResource = pTex;
			pDevTexture->m_bCube = (pLayout.m_eTT == eTT_Cube) | (pLayout.m_eTT == eTT_CubeArray);
			if (!pDevTexture->m_nBaseAllocatedSize)
			{
				pDevTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(nWdt, nHgt, nDepth, nMips, nArraySize, DeviceFormats::ConvertToTexFormat(D3DFmt), eTM_Optimal, eFlags);
			}
		}
	}

	pDevTexture->m_pRenderTargetData = pRenderTargetData;
	pDevTexture->m_eNativeFormat = D3DFmt;
	pDevTexture->m_resourceElements = CTexture::TextureDataSize(nWdt, nHgt, nDepth, nMips, nArraySize, eTF_A8);
	pDevTexture->m_subResources[eSubResource_Mips] = nMips;
	pDevTexture->m_subResources[eSubResource_Slices] = nArraySize;
	pDevTexture->m_eTT = pLayout.m_eTT;
	pDevTexture->m_bFilterable = true;
	pDevTexture->m_bIsSrgb = bIsSRGB;
	pDevTexture->m_bAllowSrgb = !!(pLayout.m_eFlags & FT_USAGE_ALLOWREADSRGB);
	pDevTexture->m_bIsMSAA = false;
	pDevTexture->m_eFlags = eFlags;
	pDevTexture->AllocatePredefinedResourceViews();

	if (pRenderTargetData && pRenderTargetData->m_pDeviceTextureMSAA)
	{
		pRenderTargetData->m_pDeviceTextureMSAA->m_pRenderTargetData = nullptr;
		pRenderTargetData->m_pDeviceTextureMSAA->m_eNativeFormat = D3DFmt;
		pRenderTargetData->m_pDeviceTextureMSAA->m_resourceElements = pDevTexture->m_resourceElements * pRenderTargetData->m_nMSAASamples;
		pRenderTargetData->m_pDeviceTextureMSAA->m_subResources[eSubResource_Mips] = nMips;
		pRenderTargetData->m_pDeviceTextureMSAA->m_subResources[eSubResource_Slices] = nArraySize;
		pRenderTargetData->m_pDeviceTextureMSAA->m_eTT = pLayout.m_eTT;
		pRenderTargetData->m_pDeviceTextureMSAA->m_bFilterable = true;
		pRenderTargetData->m_pDeviceTextureMSAA->m_bIsSrgb = bIsSRGB;
		pRenderTargetData->m_pDeviceTextureMSAA->m_bAllowSrgb = !!(pLayout.m_eFlags & FT_USAGE_ALLOWREADSRGB);
		pRenderTargetData->m_pDeviceTextureMSAA->m_bIsMSAA = true;
		pRenderTargetData->m_pDeviceTextureMSAA->m_eFlags = eFlags;
		pRenderTargetData->m_pDeviceTextureMSAA->AllocatePredefinedResourceViews();
	}

	return pDevTexture;
}

int CDeviceTexture::Cleanup()
{
	SAFE_DELETE(m_pRenderTargetData);

	int32 nRef = CDeviceResource::Cleanup();

	if (nRef != -1)
	{
#ifdef DEVRES_USE_STAGING_POOL
		if (m_pStagingResource[0])
			GetDeviceObjectFactory().ReleaseStagingResource(m_pStagingResource[0]);
		if (m_pStagingResource[1])
			GetDeviceObjectFactory().ReleaseStagingResource(m_pStagingResource[1]);

		if (m_hStagingFence[0])
			GetDeviceObjectFactory().ReleaseFence(m_hStagingFence[0]);
		if (m_hStagingFence[1])
			GetDeviceObjectFactory().ReleaseFence(m_hStagingFence[1]);

		m_pStagingResource[0] = nullptr;
		m_pStagingResource[1] = nullptr;

		m_hStagingFence[0] = 0;
		m_hStagingFence[1] = 0;
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && DEVRES_USE_PINNING
		if (nRef <= 0 && m_gpuHdl.IsValid())
		{
			GetDeviceObjectFactory().m_texturePool.Free(m_gpuHdl);
			m_gpuHdl = SGPUMemHdl();
		}
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
		m_handleMGPU = nullptr;
#endif
	}

	return nRef;
}

CDeviceTexture::~CDeviceTexture()
{
	Cleanup();
}

bool CDeviceTexture::IsMSAAChanged()
{
	const RenderTargetData* pRtdt = m_pRenderTargetData;

	return pRtdt && (pRtdt->m_nMSAASamples != gRenDev->GetMSAA().Type || pRtdt->m_nMSAAQuality != gRenDev->GetMSAA().Quality);
}

void CDeviceTexture::AddDirtRect(RECT& rcSrc, uint32 dstX, uint32 dstY)
{
	uint32 i;
	for (i = 0; i < m_pRenderTargetData->m_DirtyRects.size(); i++)
	{
		DirtyRECT& rc = m_pRenderTargetData->m_DirtyRects[i];
		RECT& rcT = rc.srcRect;
		if (rcSrc.left == rcT.left && rcSrc.right == rcT.right && rcSrc.top == rcT.top && rcSrc.bottom == rcT.bottom && dstX == rc.dstX && dstY == rc.dstY)
			break;
	}
	if (i != m_pRenderTargetData->m_DirtyRects.size())
		return;

	DirtyRECT dirtyRect;

	dirtyRect.srcRect = rcSrc;
	dirtyRect.dstX = (uint16)dstX;
	dirtyRect.dstY = (uint16)dstY;

	m_pRenderTargetData->m_DirtyRects.push_back(dirtyRect);
}

uint32 CDeviceTexture::TextureDataSize(D3DBaseView* pView)
{
	if (pView)
	{
		STextureLayout layout = GetLayout(pView);

		return CDeviceTexture::TextureDataSize(
			layout.m_nWidth,
			layout.m_nHeight,
			layout.m_nDepth,
			layout.m_nMips,
			layout.m_nArraySize,
			layout.m_eDstFormat,
			eTM_Optimal,
			layout.m_eFlags);
	}

	return 0;
}

uint32 CDeviceTexture::TextureDataSize(D3DBaseView* pView, const uint numRects, const RECT* pRects)
{
	if (pView)
	{
		STextureLayout layout = GetLayout(pView);

		uint32 fullSize = CDeviceTexture::TextureDataSize(
			layout.m_nWidth,
			layout.m_nHeight,
			layout.m_nDepth,
			layout.m_nMips,
			layout.m_nArraySize,
			layout.m_eDstFormat,
			eTM_Optimal,
			layout.m_eFlags);

		if (!numRects)
		{
			return fullSize;
		}

		uint32 nWidth  = layout.m_nWidth;
		uint32 nHeight = layout.m_nHeight;

		// If overlapping rectangles are cleared multiple times is ambiguous
		uint32 fullDim  = nWidth * nHeight;
		uint32 rectSize = 0;

		for (uint i = 0; i < numRects; ++i)
		{
			uint32 nW = max(0, int32(min(uint32(pRects->right ), nWidth )) - int32(max(0U, uint32(pRects->left))));
			uint32 nH = max(0, int32(min(uint32(pRects->bottom), nHeight)) - int32(max(0U, uint32(pRects->top ))));

			uint32 rectDim = nW * nH;

			rectSize += (uint64(fullSize) * uint64(rectDim)) / fullDim;
		}

		return rectSize;
	}

	return 0;
}

#if !CRY_PLATFORM_CONSOLE
uint32 CDeviceTexture::TextureDataSize(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nSlices, const ETEX_Format eTF, ETEX_TileMode eTM, uint32 eFlags)
{
	return CTexture::TextureDataSize(nWidth, nHeight, nDepth, nMips, nSlices, eTF, eTM_None);
}
#endif
