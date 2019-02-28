// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../DeviceObjects.h"
#include "DriverD3D.h"
#include "../../../Common/ReverseDepth.h"
#include "DeviceObjects_D3D11.h"
#include "DevicePSO_D3D11.h"
#include "DeviceResourceSet_D3D11.h"
#include "DeviceCommandList_D3D11.h"
#include "DeviceRenderPass_D3D11.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define DX11_COMMANDLIST_REDUNDANT_STATE_FILTERING

void CDeviceStatesManagerDX11::ShutDown()
{
	for (auto &state : m_StatesDP)
	{
		SAFE_RELEASE(state.pState);
	}
	for (auto &state : m_StatesRS)
	{
		SAFE_RELEASE(state.pState);
	}
	for (auto &state : m_StatesBL)
	{
		SAFE_RELEASE(state.pState);
	}
	m_StatesBL.clear();
	m_StatesRS.clear();
	m_StatesDP.clear();
}

CDeviceStatesManagerDX11* CDeviceStatesManagerDX11::GetInstance()
{
	static CDeviceStatesManagerDX11 sStatesManager;
	return &sStatesManager;
}

template<class T>
struct SStateCleaner
{
	uint32 curFrame;
	SStateCleaner(uint32 f) : curFrame(f) {};
	bool operator()(const T& m) const
	{  // curFrame==0 is the flag to clear out the whole list, regardless of age
		if ((curFrame != 0) && m.SkipCleanup())
			return false;
		
		uint32 wasItUsed = m.pState->Release();
		if (!wasItUsed)
			return true;
		m.pState->AddRef(); // still used somewhere! ==> revert retirement
		
		return false;
	}
};

// if vector gets half-full, remove any states w/out refCount>1
// TODO: add number of live-objects to r_displayInfo=3
void CDeviceStatesManagerDX11::ReleaseUnusedStates(uint32 currentFrameID) {
	if(m_StatesDP.size()>2048) // half of 4096 limit, why clean up too soon?
		m_StatesDP.erase(
			std::remove_if(m_StatesDP.begin(), m_StatesDP.end(), SStateCleaner<SStateDepth>(currentFrameID)),
			m_StatesDP.end());

	if (m_StatesRS.size()>2048)
		m_StatesRS.erase(
			std::remove_if(m_StatesRS.begin(), m_StatesRS.end(), SStateCleaner<SStateRaster>(currentFrameID)),
			m_StatesRS.end());

	if (m_StatesBL.size()>2048)
		m_StatesBL.erase(
			std::remove_if(m_StatesBL.begin(), m_StatesBL.end(), SStateCleaner<SStateBlend>(currentFrameID)),
			m_StatesBL.end());
}

uint32 CDeviceStatesManagerDX11::GetOrCreateBlendState(const D3D11_BLEND_DESC& desc)
{
	uint32 i;
	HRESULT hr = S_OK;
	uint64 nHash = SStateBlend::GetHash(desc);
	for (i = 0; i < (uint32)m_StatesBL.size(); i++)
	{
		if (m_StatesBL[i].nHashVal == nHash)
			break;
	}
	if (i == m_StatesBL.size())
	{
		m_StatesBL.push_back(SStateBlend());
		SStateBlend* pState = &m_StatesBL.back();
		pState->Desc = desc;
		pState->nHashVal = nHash;
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE) || defined(CRY_RENDERER_DIRECT3D)
		hr = gcpRendD3D->GetDevice().CreateBlendState(&pState->Desc, &pState->pState);
#else
		pState->pState = nullptr;
		hr = S_OK;
#endif
		assert(SUCCEEDED(hr));
	}
	return SUCCEEDED(hr) ? i : uint32(-1);
}

uint32 CDeviceStatesManagerDX11::GetOrCreateRasterState(const D3D11_RASTERIZER_DESC& rasterizerDec, const bool bAllowMSAA)
{
	uint32 i;
	HRESULT hr = S_OK;

	D3D11_RASTERIZER_DESC desc = rasterizerDec;

	//BOOL bMSAA = bAllowMSAA && m_RP.m_MSAAData.Type > 1;
	//if (bMSAA != desc.MultisampleEnable)
	desc.MultisampleEnable = false;

	uint32 nHash = SStateRaster::GetHash(desc);
	uint64 nValuesHash = SStateRaster::GetValuesHash(desc);
	for (i = 0; i < (uint32)m_StatesRS.size(); i++)
	{
		if (m_StatesRS[i].nHashVal == nHash && m_StatesRS[i].nValuesHash == nValuesHash)
			break;
	}
	if (i == m_StatesRS.size())
	{
		m_StatesRS.push_back(SStateRaster());
		SStateRaster* pState = &m_StatesRS.back();
		pState->Desc = desc;
		pState->nHashVal = nHash;
		pState->nValuesHash = nValuesHash;
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE) || defined(CRY_RENDERER_DIRECT3D)
		hr = gcpRendD3D->GetDevice().CreateRasterizerState(&pState->Desc, &pState->pState);
#else
		pState->pState = nullptr;
		hr = S_OK;
#endif
		assert(SUCCEEDED(hr));
	}
	return SUCCEEDED(hr) ? i : uint32(-1);
}

uint32 CDeviceStatesManagerDX11::GetOrCreateDepthState(const D3D11_DEPTH_STENCIL_DESC& desc)
{
	uint32 i;
	HRESULT hr = S_OK;
	uint64 nHash = SStateDepth::GetHash(desc);
	const int kNumStates = (int)m_StatesDP.size();
	for (i = 0; i < kNumStates; i++)
	{
		if (m_StatesDP[i].nHashVal == nHash)
			break;
	}
	if (i == kNumStates)
	{
		m_StatesDP.push_back(SStateDepth());
		SStateDepth* pState = &m_StatesDP.back();
		pState->Desc = desc;
		pState->nHashVal = nHash;
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE) || defined(CRY_RENDERER_DIRECT3D)
		hr = gcpRendD3D->GetDevice().CreateDepthStencilState(&pState->Desc, &pState->pState);
#else
		pState->pState = nullptr;
		hr = S_OK;
#endif
		assert(SUCCEEDED(hr));
	}
	return SUCCEEDED(hr) ? i : uint32(-1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceObjectFactory::CDeviceObjectFactory()
{
	memset(m_NullResources, 0, sizeof(m_NullResources));

	m_pCoreCommandList.reset(new CDeviceCommandList());

	m_pDX11Device = nullptr;
	m_pDX11Scheduler = nullptr;
}

void CDeviceObjectFactory::AssignDevice(D3DDevice* pDevice)
{
	CRY_ASSERT(!m_pDX11Device);
	CRY_ASSERT(!m_pDX11Scheduler);

#if CRY_PLATFORM_DURANGO
	D3D11_DMA_ENGINE_CONTEXT_DESC dmaDesc = { 0 };
	dmaDesc.CreateFlags = D3D11_DMA_ENGINE_CONTEXT_CREATE_SDMA_1;
	dmaDesc.RingBufferSizeBytes = 0;

	gcpRendD3D->GetPerformanceDevice().CreateDmaEngineContext(&dmaDesc, &m_pDMA1);

	UINT poolMemModel =
#if defined(TEXTURES_IN_CACHED_MEM)
		D3D11_GRAPHICS_MEMORY_ACCESS_CPU_CACHE_NONCOHERENT_GPU_READONLY
#else
		D3D11_GRAPHICS_MEMORY_ACCESS_CPU_WRITECOMBINE_NONCOHERENT
#endif
		;

	m_texturePool.Init(
		CRenderer::GetTexturesStreamPoolSize() * 1024ull * 1024ull,
		512 * 1024 * 1024,
		CRenderer::GetTexturesStreamPoolSize() * 1024ull * 1024ull,
		poolMemModel,
		false);

	m_textureStagingRing.Init(m_pDMA1, 128 * 1024 * 1024);
#endif

	m_pDX11Device = NCryDX11::CDevice::Create(pDevice, D3D_FEATURE_LEVEL_11_0);
	m_pDX11Scheduler = &m_pDX11Device->GetScheduler();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSOImpl(const CDeviceGraphicsPSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceGraphicsPSO_DX11>();
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSOImpl(const CDeviceComputePSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceComputePSO_DX11>();
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceResourceSetPtr CDeviceObjectFactory::CreateResourceSet(CDeviceResourceSet::EFlags flags) const
{
	return std::make_shared<CDeviceResourceSet_DX11>(flags);
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const
{
	return std::make_shared<CDeviceResourceLayout_DX11>(resourceLayoutDesc.GetRequiredResourceBindings());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Acquire one or more command-lists which are independent of the core command-list
// Only one thread is allowed to call functions on this command-list (DX12 restriction).
// The thread that gets the permition is the one calling Begin() on it AFAIS
CDeviceCommandListUPtr CDeviceObjectFactory::AcquireCommandList(EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// TODO: implement deferred contexts
	__debugbreak();

	return stl::make_unique<CDeviceCommandList>();
}

std::vector<CDeviceCommandListUPtr> CDeviceObjectFactory::AcquireCommandLists(uint32 listCount, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// TODO: implement deferred contexts
	__debugbreak();

	return std::vector<CDeviceCommandListUPtr>(size_t(listCount));
}

void CDeviceObjectFactory::ForfeitCommandList(CDeviceCommandListUPtr pCommandList, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// TODO: implement deferred contexts
	__debugbreak();

	// pContext->ExecuteCommandList(ID3D11CommandList)
}

void CDeviceObjectFactory::ForfeitCommandLists(std::vector<CDeviceCommandListUPtr> pCommandLists, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// TODO: implement deferred contexts
	__debugbreak();

	// pContext->ExecuteCommandList(ID3D11CommandList)
}

void CDeviceObjectFactory::ReleaseResourcesImpl()
{
	CDeviceStatesManagerDX11::GetInstance()->ShutDown();
}

////////////////////////////////////////////////////////////////////////////
UINT64 CDeviceObjectFactory::QueryFormatSupport(D3DFormat Format)
{
	CD3D9Renderer* rd = gcpRendD3D;

	UINT nOptions;
	HRESULT hr = rd->GetDevice().CheckFormatSupport(Format, &nOptions);
	if (SUCCEEDED(hr))
	{
		// *INDENT-OFF*
		return
			(nOptions & D3D11_FORMAT_SUPPORT_BUFFER                      ? FMTSUPPORT_BUFFER                      : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE1D                   ? FMTSUPPORT_TEXTURE1D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE2D                   ? FMTSUPPORT_TEXTURE2D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE3D                   ? FMTSUPPORT_TEXTURE3D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURECUBE                 ? FMTSUPPORT_TEXTURECUBE                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER            ? FMTSUPPORT_IA_VERTEX_BUFFER            : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER             ? FMTSUPPORT_IA_INDEX_BUFFER             : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SO_BUFFER                   ? FMTSUPPORT_SO_BUFFER                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MIP                         ? FMTSUPPORT_MIP                         : 0) |
//			(nOptions & D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT      ? FMTSUPPORT_SRGB                        : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_LOAD                 ? FMTSUPPORT_SHADER_LOAD                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_GATHER               ? FMTSUPPORT_SHADER_GATHER               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON    ? FMTSUPPORT_SHADER_GATHER_COMPARISON    : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE               ? FMTSUPPORT_SHADER_SAMPLE               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON    ? FMTSUPPORT_SHADER_SAMPLE_COMPARISON    : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW ? FMTSUPPORT_TYPED_UNORDERED_ACCESS_VIEW : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL               ? FMTSUPPORT_DEPTH_STENCIL               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_RENDER_TARGET               ? FMTSUPPORT_RENDER_TARGET               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_BLENDABLE                   ? FMTSUPPORT_BLENDABLE                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_DISPLAY                     ? FMTSUPPORT_DISPLAYABLE                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD            ? FMTSUPPORT_MULTISAMPLE_LOAD            : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE         ? FMTSUPPORT_MULTISAMPLE_RESOLVE         : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET    ? FMTSUPPORT_MULTISAMPLE_RENDERTARGET    : 0);
		// *INDENT-ON*
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Fence API

#if !CRY_PLATFORM_ORBIS && !(CRY_PLATFORM_DURANGO && BUFFER_ENABLE_DIRECT_ACCESS)
HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	D3D11_QUERY_DESC QDesc;
	QDesc.Query = D3D11_QUERY_EVENT;
	QDesc.MiscFlags = 0;
	ID3D11Query* d3d_query = nullptr;
	hr = gcpRendD3D->GetDevice().CreateQuery(&QDesc, &d3d_query);
	if (!FAILED(hr))
	{
		query = reinterpret_cast<DeviceFenceHandle>(d3d_query);
		IssueFence(query);
	}
	return hr;
}

HRESULT CDeviceObjectFactory::ReleaseFence(DeviceFenceHandle query)
{
	HRESULT hr = S_FALSE;
	ID3D11Query* d3d_query = reinterpret_cast<ID3D11Query*>(query);
	SAFE_RELEASE(d3d_query);
	hr = S_OK;
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = S_FALSE;
	ID3D11Query* d3d_query = reinterpret_cast<ID3D11Query*>(query);
	if (d3d_query)
	{
		gcpRendD3D->GetDeviceContext().End(d3d_query);
		hr = S_OK;
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = S_FALSE;
	ID3D11Query* d3d_query = reinterpret_cast<ID3D11Query*>(query);
	if (d3d_query)
	{
		CRY_ASSERT(!flush || !gRenDev->m_pRT || gRenDev->m_pRT->IsRenderThread()); // Can only flush from render thread

		BOOL bQuery = false;
		do
		{
			hr = gcpRendD3D->GetDeviceContext().GetData(d3d_query, (void*)&bQuery, sizeof(BOOL), flush ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH);
		} while (block && hr != S_OK);
	}
	return hr;
}
#elif CRY_PLATFORM_ORBIS
HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	query = reinterpret_cast<DeviceFenceHandle>(new GpuSyncId);
	hr = query ? S_OK : E_FAIL;
	if (!FAILED(hr))
	{
		IssueFence(query);
	}
	return hr;
}

HRESULT CDeviceObjectFactory::ReleaseFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : S_FALSE;
	delete reinterpret_cast<GpuSyncId*>(query); // TODO: CHECK THIS
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : E_FAIL;
	GpuSyncId* handle = reinterpret_cast<GpuSyncId*>(query);
	if (handle)
	{
		*handle = DXOrbis::GetLastDrawCall();
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = E_FAIL;
	GpuSyncId* handle = reinterpret_cast<GpuSyncId*>(query);
	if (handle)
	{
		SSerialCompare<GpuSyncId> serialCompare;
		if (block)
		{
			DXOrbis::Device()->WaitForGPUDrawCallComplete(*handle);
			hr = S_OK;
		}
		else if (serialCompare(*handle, DXOrbis::Device()->GetLastCompletedDrawCall()))
		{
			hr = S_OK;
		}
		else
		{
			hr = S_FALSE;
		}
	}
	return hr;
}
#endif

////////////////////////////////////////////////////////////////////////////
// SamplerState API

static inline D3D11_TEXTURE_ADDRESS_MODE sAddressMode(ESamplerAddressMode nAddress)
{
	static_assert((eSamplerAddressMode_Wrap   + 1) == D3D11_TEXTURE_ADDRESS_WRAP  , "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Mirror + 1) == D3D11_TEXTURE_ADDRESS_MIRROR, "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Clamp  + 1) == D3D11_TEXTURE_ADDRESS_CLAMP , "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Border + 1) == D3D11_TEXTURE_ADDRESS_BORDER, "AddressMode enum not mappable to D3D11 enum by arithmetic");

	assert(nAddress >= eSamplerAddressMode_Wrap && nAddress <= eSamplerAddressMode_Border);
	return D3D11_TEXTURE_ADDRESS_MODE(nAddress + 1);
}

CDeviceSamplerState* CDeviceObjectFactory::CreateSamplerState(const SSamplerState& pState)
{
	D3D11_SAMPLER_DESC Desc;
	ZeroStruct(Desc);
	CDeviceSamplerState* pSamp = NULL;
	// AddressMode of 0 is INVALIDARG
	Desc.AddressU = sAddressMode(ESamplerAddressMode(pState.m_nAddressU));
	Desc.AddressV = sAddressMode(ESamplerAddressMode(pState.m_nAddressV));
	Desc.AddressW = sAddressMode(ESamplerAddressMode(pState.m_nAddressW));
	ColorF col((uint32)pState.m_dwBorderColor);
	Desc.BorderColor[0] = col.r;
	Desc.BorderColor[1] = col.g;
	Desc.BorderColor[2] = col.b;
	Desc.BorderColor[3] = col.a;
	if (pState.m_bComparison)
		Desc.ComparisonFunc = D3D11_COMPARISON_LESS;
	else
		Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	Desc.MaxAnisotropy = 1;
	Desc.MinLOD = 0;
	if (pState.m_nMipFilter == FILTER_NONE)
		Desc.MaxLOD = 0.0f;
	else
		Desc.MaxLOD = 100.0f;

	Desc.MipLODBias = pState.m_fMipLodBias;

	if (pState.m_bComparison)
	{
		if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && pState.m_nMipFilter == FILTER_LINEAR || pState.m_nMinFilter == FILTER_TRILINEAR || pState.m_nMagFilter == FILTER_TRILINEAR)
		{
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		}
		else if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT) || pState.m_nMinFilter == FILTER_BILINEAR || pState.m_nMagFilter == FILTER_BILINEAR)
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		else if (pState.m_nMinFilter == FILTER_POINT && pState.m_nMagFilter == FILTER_POINT && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT))
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		else if (pState.m_nMinFilter >= FILTER_ANISO2X && pState.m_nMagFilter >= FILTER_ANISO2X && pState.m_nMipFilter >= FILTER_ANISO2X)
		{
			Desc.Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
			Desc.MaxAnisotropy = pState.m_nAnisotropy;
		}
	}
	else
	{
		if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && pState.m_nMipFilter == FILTER_LINEAR || pState.m_nMinFilter == FILTER_TRILINEAR || pState.m_nMagFilter == FILTER_TRILINEAR)
		{
			Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		else if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT) || pState.m_nMinFilter == FILTER_BILINEAR || pState.m_nMagFilter == FILTER_BILINEAR)
			Desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		else if (pState.m_nMinFilter == FILTER_POINT && pState.m_nMagFilter == FILTER_POINT && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT))
			Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		else if (pState.m_nMinFilter >= FILTER_ANISO2X && pState.m_nMagFilter >= FILTER_ANISO2X && pState.m_nMipFilter >= FILTER_ANISO2X)
		{
			Desc.Filter = D3D11_FILTER_ANISOTROPIC;
			Desc.MaxAnisotropy = pState.m_nAnisotropy;
		}
		else
			assert(0);
	}

	HRESULT hr = gcpRendD3D->GetDevice().CreateSamplerState(&Desc, &pSamp);
	if (SUCCEEDED(hr))
		return pSamp;
	else
		assert(0);
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////
// InputLayout API

CDeviceInputLayout* CDeviceObjectFactory::CreateInputLayout(const SInputLayout& pLayout, const SShaderBlob* m_pConsumingVertexShader)
{
	if (!m_pConsumingVertexShader || !m_pConsumingVertexShader->m_pShaderData)
		return nullptr;

	const int   nSize = m_pConsumingVertexShader->m_nDataSize;
	const void* pVSData = m_pConsumingVertexShader->m_pShaderData;

	CDeviceInputLayout* Layout;
	HRESULT hr = E_FAIL;
	if (FAILED(hr = gcpRendD3D->GetDevice().CreateInputLayout(&pLayout.m_Declaration[0], pLayout.m_Declaration.size(), pVSData, nSize, &Layout)))
	{
		CRY_ASSERT(false);
		return Layout;
	}

	return Layout;
}

////////////////////////////////////////////////////////////////////////////
// Low-level resource management API (TODO: remove D3D-dependency by abstraction)

void CDeviceObjectFactory::AllocateNullResources()
{
}

void CDeviceObjectFactory::ReleaseNullResources()
{
}

//=============================================================================

#if USE_NV_API
	#include NV_API_HEADER
#endif
#if USE_AMD_API
	#include AMD_API_HEADER
#endif

#ifdef DEVRES_USE_STAGING_POOL
D3DResource* CDeviceObjectFactory::AllocateStagingResource(D3DResource* pForTex, bool bUpload, void*& pMappedAddress)
{
	D3DResource* pStaging = nullptr;
	HRESULT result = GetDX11Device()->CreateOrReuseStagingResource(pForTex, &pStaging, bUpload);
	return pStaging;
}

void CDeviceObjectFactory::ReleaseStagingResource(D3DResource* pStagingRes)
{
	// NOTE: Poor man's resource tracking (take current time as last-used moment)
	GetDX11Device()->ReleaseLater(GetDX11Scheduler()->GetFenceManager().GetCurrentValues(), pStagingRes, true);
	pStagingRes->Release();
}
#endif

void CDeviceObjectFactory::ReleaseResource(D3DResource* pResource)
{
	// NOTE: Poor man's resource tracking (take current time as last-used moment)
	GetDX11Device()->ReleaseLater(GetDX11Scheduler()->GetFenceManager().GetCurrentValues(), pResource, false);
	pResource->Release();
}

void CDeviceObjectFactory::RecycleResource(D3DResource* pResource)
{
	// NOTE: Poor man's resource tracking (take current time as last-used moment)
	GetDX11Device()->ReleaseLater(GetDX11Scheduler()->GetFenceManager().GetCurrentValues(), pResource, true);
	pResource->Release();
}

//=============================================================================

HRESULT CDeviceObjectFactory::Create2DTexture(uint32 nWidth, uint32 nHeight, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload, int32 nESRAMOffset)
{
	HRESULT hr = S_OK;

	// The format(0x2d, D24_UNORM_S8_UINT) cannot be bound as a ShaderResource or cast to a format that could be bound as a ShaderResource.
	// Therefore this format cannot support D3D11_BIND_SHADER_RESOURCE.Specifiying the format R24G8_TYPELESS instead would solve this issue.
	if ((nUsage & (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE)) == (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE))
		Format = DeviceFormats::ConvertToTypeless(Format);

	D3D11_TEXTURE2D_DESC Desc;
	ZeroStruct(Desc);
	Desc.Width = nWidth;
	Desc.Height = nHeight;
	Desc.MipLevels = nMips;
	Desc.SampleDesc.Count = pPayload ? pPayload->m_nDstMSAASamples : 1;
	Desc.SampleDesc.Quality = pPayload ? pPayload->m_nDstMSAAQuality : 0;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.ArraySize = nArraySize;
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	if (nESRAMOffset != SKIP_ESRAM)
	{
		Desc.MiscFlags |= D3D11X_RESOURCE_MISC_ESRAM_RESIDENT;
		Desc.ESRAMOffsetBytes = (UINT)nESRAMOffset;
	}
#endif

#if CRY_PLATFORM_DURANGO
	if (InPlaceConstructable(Desc, nUsage))
	{
		hr = CreateInPlaceTexture2D(Desc, nUsage, pPayload, *ppDevTexture);
	}
	else
#endif
	{
		hr = Create2DTexture(Desc, nUsage, cClearValue, ppDevTexture, pPayload);
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(uint32 nSize, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	HRESULT hr = S_OK;

	// The format(0x2d, D24_UNORM_S8_UINT) cannot be bound as a ShaderResource or cast to a format that could be bound as a ShaderResource.
	// Therefore this format cannot support D3D11_BIND_SHADER_RESOURCE.Specifiying the format R24G8_TYPELESS instead would solve this issue.
	if ((nUsage & (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE)) == (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE))
		Format = DeviceFormats::ConvertToTypeless(Format);

	D3D11_TEXTURE2D_DESC Desc;
	ZeroStruct(Desc);
	Desc.Width = nSize;
	Desc.Height = nSize;
	Desc.MipLevels = nMips;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.ArraySize = nArraySize; assert(!(nArraySize % 6));
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage) | D3D11_RESOURCE_MISC_TEXTURECUBE;

#if CRY_PLATFORM_DURANGO
	if (InPlaceConstructable(Desc, nUsage))
	{
		hr = CreateInPlaceTexture2D(Desc, nUsage, pPayload, *ppDevTexture);
	}
	else
#endif
	{
		hr = CreateCubeTexture(Desc, nUsage, cClearValue, ppDevTexture, pPayload);
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
	HRESULT hr = S_OK;

	// The format(0x2d, D24_UNORM_S8_UINT) cannot be bound as a ShaderResource or cast to a format that could be bound as a ShaderResource.
	// Therefore this format cannot support D3D11_BIND_SHADER_RESOURCE.Specifiying the format R24G8_TYPELESS instead would solve this issue.
	if ((nUsage & (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE)) == (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE))
		Format = DeviceFormats::ConvertToTypeless(Format);

	D3D11_TEXTURE3D_DESC Desc;
	ZeroStruct(Desc);
	Desc.Width = nWidth;
	Desc.Height = nHeight;
	Desc.Depth = nDepth;
	Desc.MipLevels = nMips;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

	return CreateVolumeTexture(Desc, nUsage, cClearValue, ppDevTexture, pTI);
}

HRESULT CDeviceObjectFactory::Create2DTexture(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3DTexture* pD3DTex = NULL; assert(!pPayload || (((Desc.MipLevels * Desc.ArraySize) <= g_nD3D10MaxSupportedSubres)));
	HRESULT hr;

	if ((Desc.MiscFlags & D3D11_RESOURCE_MISC_HIFREQ_HEAP) && !pPayload)
	{
		D3D11_BIND_FLAG       heapFlags = D3D11_BIND_FLAG(Desc.BindFlags);
		D3D11_HEAP_PROPERTIES heapProperties = { Desc.Usage, D3D11_CPU_ACCESS_FLAG(Desc.CPUAccessFlags) };
		D3D11_RESOURCE_DESC   resourceDesc = { D3D11_RESOURCE_DIMENSION_TEXTURE2D, 0, Desc.Width, Desc.Height, Desc.ArraySize, Desc.MipLevels, Desc.Format, 0, Desc.SampleDesc, D3D11_RESOURCE_MISC_FLAG(Desc.MiscFlags & ~D3D11_RESOURCE_MISC_HIFREQ_HEAP) };

		hr = GetDX11Device()->CreateOrReuseCommittedResource(&heapProperties, heapFlags, &resourceDesc, IID_GFX_ARGS(&pD3DTex));
	}
	else
	{
		D3D11_TEXTURE2D_DESC _Desc = Desc; _Desc.MiscFlags &= ~D3D11_RESOURCE_MISC_HIFREQ_HEAP;

		// TODO: no data on creation, then use GetDX11Device()->CreateCommittedResource()
		hr = gcpRendD3D->GetDevice().CreateTexture2D(&_Desc, ConvertToDX11Data(Desc.MipLevels, pPayload, SRD), &pD3DTex);
	}

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, 1, Desc.MipLevels, Desc.ArraySize, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3DCubeTexture* pD3DTex = NULL; assert(!pPayload || (((Desc.MipLevels * Desc.ArraySize) <= g_nD3D10MaxSupportedSubres) && !(Desc.ArraySize % 6)));
	HRESULT hr;

	if ((Desc.MiscFlags & D3D11_RESOURCE_MISC_HIFREQ_HEAP) && !pPayload)
	{
		D3D11_BIND_FLAG       heapFlags = D3D11_BIND_FLAG(Desc.BindFlags);
		D3D11_HEAP_PROPERTIES heapProperties = { Desc.Usage, D3D11_CPU_ACCESS_FLAG(Desc.CPUAccessFlags) };
		D3D11_RESOURCE_DESC   resourceDesc = { D3D11_RESOURCE_DIMENSION_TEXTURE2D, 0, Desc.Width, Desc.Height, Desc.ArraySize, Desc.MipLevels, Desc.Format, 0, Desc.SampleDesc, D3D11_RESOURCE_MISC_FLAG(Desc.MiscFlags & ~D3D11_RESOURCE_MISC_HIFREQ_HEAP) };

		hr = GetDX11Device()->CreateOrReuseCommittedResource(&heapProperties, heapFlags, &resourceDesc, IID_GFX_ARGS(&pD3DTex));
	}
	else
	{
		D3D11_TEXTURE2D_DESC _Desc = Desc; _Desc.MiscFlags &= ~D3D11_RESOURCE_MISC_HIFREQ_HEAP;

		// TODO: no data on creation, then use GetDX11Device()->CreateCommittedResource()
		hr = gcpRendD3D->GetDevice().CreateTexture2D(&_Desc, ConvertToDX11Data(Desc.MipLevels * Desc.ArraySize, pPayload, SRD), &pD3DTex);
	}

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		pDeviceTexture->m_bCube = true;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, 1, Desc.MipLevels, Desc.ArraySize, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(const D3D11_TEXTURE3D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3DVolumeTexture* pD3DTex = NULL;
	HRESULT hr;

	if ((Desc.MiscFlags & D3D11_RESOURCE_MISC_HIFREQ_HEAP) && !pPayload)
	{
		D3D11_BIND_FLAG       heapFlags = D3D11_BIND_FLAG(Desc.BindFlags);
		D3D11_HEAP_PROPERTIES heapProperties = { Desc.Usage, D3D11_CPU_ACCESS_FLAG(Desc.CPUAccessFlags) };
		D3D11_RESOURCE_DESC   resourceDesc = { D3D11_RESOURCE_DIMENSION_TEXTURE3D, 0, Desc.Width, Desc.Height, Desc.Depth, Desc.MipLevels, Desc.Format, 0, {1,0}, D3D11_RESOURCE_MISC_FLAG(Desc.MiscFlags & ~D3D11_RESOURCE_MISC_HIFREQ_HEAP) };

		hr = GetDX11Device()->CreateOrReuseCommittedResource(&heapProperties, heapFlags, &resourceDesc, IID_GFX_ARGS(&pD3DTex));
	}
	else
	{
		D3D11_TEXTURE3D_DESC _Desc = Desc; _Desc.MiscFlags &= ~D3D11_RESOURCE_MISC_HIFREQ_HEAP;

		// TODO: no data on creation, then use GetDX11Device()->CreateCommittedResource()
		hr = gcpRendD3D->GetDevice().CreateTexture3D(&_Desc, ConvertToDX11Data(Desc.MipLevels, pPayload, SRD), &pD3DTex);
	}

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, Desc.Depth, Desc.MipLevels, 1, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateBuffer(
	buffer_size_t nSize
	, buffer_size_t elemSize
	, uint32 nUsage
	, uint32 nBindFlags
	, D3DBuffer** ppBuff
	, const void* pData)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "CreateBuffer");
	HRESULT hr = S_OK;

#ifndef _RELEASE
	// ToDo verify that the usage and bindflags are correctly set (e.g certain
	// bit groups are mutually exclusive).
#endif

	D3D11_BUFFER_DESC Desc;
	ZeroStruct(Desc);

	Desc.StructureByteStride = elemSize;
	Desc.ByteWidth = nSize * elemSize;
	if ((nUsage & USAGE_CPU_WRITE))
		Desc.ByteWidth = CDeviceBufferManager::AlignElementCountForStreaming(nSize, elemSize) * elemSize;

	Desc.BindFlags = ConvertToDX11BindFlags(nBindFlags);
	if (nBindFlags & (BIND_STREAM_OUTPUT | BIND_RENDER_TARGET | BIND_DEPTH_STENCIL))
	{
		CryFatalError("trying to create (currently) unsupported buffer type");
	}

#if CRY_PLATFORM_DURANGO && BUFFER_USE_STAGED_UPDATES == 0
	if (nUsage & USAGE_DIRECT_ACCESS)
	{
	//	if (nUsage & HEAP_STAGING)
	//	{
	//		CryFatalError("staging buffers not supported if BUFFER_USE_STAGED_UPDATES not defined");
	//	}

		Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
		Desc.Usage = (D3D11_USAGE)D3D11_USAGE_DEFAULT;
		Desc.MiscFlags = 0;

		void* BufBasePtr;

		if (!IsAligned(nSize * elemSize, 4096))
		{
			CryFatalError("Memory Allocation Size for Direct Video Memory Access must be a multiple of 4 KB but the supplied size is %u", nSize * elemSize);
		}

		D3D11_GRAPHICS_MEMORY_ACCESS_FLAG access_flag = D3D11_GRAPHICS_MEMORY_ACCESS_CPU_WRITECOMBINE_NONCOHERENT;
		if ((nUsage & (USAGE_DIRECT_ACCESS_CPU_COHERENT | USAGE_DIRECT_ACCESS_GPU_COHERENT)) ==
			(USAGE_DIRECT_ACCESS_CPU_COHERENT | USAGE_DIRECT_ACCESS_GPU_COHERENT))
		{
			access_flag = D3D11_GRAPHICS_MEMORY_ACCESS_CPU_CACHE_COHERENT;
		}
		{
			hr = D3DAllocateGraphicsMemory(
				nSize * elemSize,
				0, 0
				, access_flag
				, &BufBasePtr);
		}
		assert(hr == S_OK);

		// TODO: use GetDX11Device()->CreatePlacedResource()
		hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementBuffer(&Desc, BufBasePtr, ppBuff);
		assert(hr == S_OK);

		UINT size = sizeof(BufBasePtr);
		hr = (*ppBuff)->SetPrivateData(BufferPointerGuid, size, &BufBasePtr);
		assert(hr == S_OK);

		if (pData)
		{
			StreamBufferData(BufBasePtr, pData, nSize * elemSize);
		}

		return hr;
	}
#endif

	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

#if !CRY_RENDERER_OPENGL
	if (Desc.Usage == D3D11_USAGE_STAGING)
	{
		Desc.BindFlags = 0;
	}
#endif

	D3D11_SUBRESOURCE_DATA* pSRD = NULL;
	D3D11_SUBRESOURCE_DATA SRD;
	if (pData)
	{
		pSRD = &SRD;

		SRD.pSysMem = pData;
		SRD.SysMemPitch = Desc.ByteWidth;
		SRD.SysMemSlicePitch = Desc.ByteWidth;
	}

	if ((Desc.MiscFlags & D3D11_RESOURCE_MISC_HIFREQ_HEAP) && !pData)
	{
		D3D11_BIND_FLAG       heapFlags = D3D11_BIND_FLAG(Desc.BindFlags);
		D3D11_HEAP_PROPERTIES heapProperties = { Desc.Usage, D3D11_CPU_ACCESS_FLAG(Desc.CPUAccessFlags) };
		D3D11_RESOURCE_DESC   resourceDesc = { D3D11_RESOURCE_DIMENSION_BUFFER, 0, Desc.ByteWidth, 1, 1, 1, DXGI_FORMAT_UNKNOWN, Desc.StructureByteStride, { 1,0 }, D3D11_RESOURCE_MISC_FLAG(Desc.MiscFlags & ~D3D11_RESOURCE_MISC_HIFREQ_HEAP) };

		hr = GetDX11Device()->CreateOrReuseCommittedResource(&heapProperties, heapFlags, &resourceDesc, IID_GFX_ARGS(ppBuff));
	}
	else
	{
		Desc.MiscFlags &= ~D3D11_RESOURCE_MISC_HIFREQ_HEAP;

		// TODO: no data on creation, then use GetDX11Device()->CreateCommittedResource()
		hr = gcpRendD3D->GetDevice().CreateBuffer(&Desc, pSRD, ppBuff);
	}

	return hr;
}

//=============================================================================

HRESULT CDeviceObjectFactory::InvalidateCpuCache(void* buffer_ptr, size_t size, size_t offset)
{
	return S_OK;
}

HRESULT CDeviceObjectFactory::InvalidateGpuCache(D3DBuffer* buffer, void* buffer_ptr, buffer_size_t size, buffer_size_t offset)
{
	return S_OK;
}

void CDeviceObjectFactory::InvalidateBuffer(D3DBuffer* buffer, void* base_ptr, buffer_size_t offset, buffer_size_t size, uint32 id)
{
}

//=============================================================================

uint8* CDeviceObjectFactory::Map(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	D3D11_MAPPED_SUBRESOURCE mapped_resource = { 0 };
#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
	HRESULT hr = DXGLMapBufferRange(
		gcpRendD3D->GetDeviceContext().GetRealDeviceContext()
		, buffer
		, offset
		, size
		, mode
		, 0
		, &mapped_resource);
	mapped_resource.pData = reinterpret_cast<uint8*>(mapped_resource.pData) - offset;
#else
	SIZE_T BeginEndR[2] = { offset, offset + size };
	HRESULT hr = gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Map(
		buffer
		, subresource
		, BeginEndR
		, mode
		, 0
		, &mapped_resource);
#endif
	return reinterpret_cast<uint8*>(mapped_resource.pData);
}

void CDeviceObjectFactory::Unmap(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	SIZE_T BeginEndW[2] = { offset, offset + size };
	gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Unmap(
		buffer
		, subresource
		, BeginEndW);
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::UploadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks)
{
	if (!bDirectAccess)
	{
		const uint8* pInData = reinterpret_cast<const uint8*>(pInDataCPU);
		uint8* pOutData = CDeviceObjectFactory::Map(buffer, subresource, offset, size, mode) + offset;

		StreamBufferData(pOutData, pInData, size);

		CDeviceObjectFactory::Unmap(buffer, subresource, offset, size, mode);
	}
	else
	{
		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::MarkReadRange(buffer, offset, 0, D3D11_MAP_WRITE);

		StreamBufferData(pOutDataGPU, pInDataCPU, size);

		CDeviceObjectFactory::MarkWriteRange(buffer, offset, size, D3D11_MAP_WRITE);
	}
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::DownloadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks)
{
	if (!bDirectAccess)
	{
		uint8* pOutData = reinterpret_cast<uint8*>(pOutDataCPU);
		const uint8* pInData = CDeviceObjectFactory::Map(buffer, subresource, offset, size, mode) + offset;

		FetchBufferData(pOutData, pInData, size);

		CDeviceObjectFactory::Unmap(buffer, subresource, offset, 0, mode);
	}
	else
	{
		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::MarkReadRange(buffer, offset, size, D3D11_MAP_READ);

		FetchBufferData(pOutDataCPU, pInDataGPU, size);

		CDeviceObjectFactory::MarkWriteRange(buffer, offset, 0, D3D11_MAP_READ);
	}
}

// Explicit instantiation
template
void CDeviceObjectFactory::UploadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::UploadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);

template
void CDeviceObjectFactory::DownloadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::DownloadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);

// Shader API
ID3D11VertexShader* CDeviceObjectFactory::CreateVertexShader(const void* pData, size_t bytes)
{
	ID3D11VertexShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateVertexShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11PixelShader* CDeviceObjectFactory::CreatePixelShader(const void* pData, size_t bytes)
{
	ID3D11PixelShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreatePixelShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11GeometryShader* CDeviceObjectFactory::CreateGeometryShader(const void* pData, size_t bytes)
{
	ID3D11GeometryShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateGeometryShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11HullShader* CDeviceObjectFactory::CreateHullShader(const void* pData, size_t bytes)
{
	ID3D11HullShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateHullShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11DomainShader* CDeviceObjectFactory::CreateDomainShader(const void* pData, size_t bytes)
{
	ID3D11DomainShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateDomainShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11ComputeShader* CDeviceObjectFactory::CreateComputeShader(const void* pData, size_t bytes)
{
	ID3D11ComputeShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateComputeShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

// Occlusion Query API
D3DOcclusionQuery* CDeviceObjectFactory::CreateOcclusionQuery()
{
	D3DOcclusionQuery* pResult;
	D3D11_QUERY_DESC desc;
	desc.Query = D3D11_QUERY_OCCLUSION;
	desc.MiscFlags = 0;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateQuery(&desc, &pResult)) ? pResult : nullptr;
}

bool CDeviceObjectFactory::GetOcclusionQueryResults(D3DOcclusionQuery* pQuery, uint64& samplesPassed)
{
	return gcpRendD3D->GetDeviceContext().GetData(pQuery, &samplesPassed, sizeof(uint64), 0) == S_OK;
}
