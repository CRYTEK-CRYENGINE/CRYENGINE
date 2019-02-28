// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/CryCustomTypes.h>
#include <CryCore/BitFiddling.h>
#include "../../DeviceManager/DeviceObjects.h"
#include "DriverD3D.h"
#include "DeviceResourceSet_D3D12.h"
#include "DeviceCommandListCommon_D3D12.h"
#include "DeviceRenderPass_D3D12.h"
#include "DevicePSO_D3D12.h"

#ifndef DEVRES_USE_STAGING_POOL
#error StagingPool is a requirement for DX12
#endif

#ifdef DX12_LINKEDADAPTER
	#include "DX12/API/Redirections/D3D12Device.inl"
#endif

using namespace NCryDX12;

static auto lambdaCeaseCallback = [](void* cmd, uint nPoolId)
{
	reinterpret_cast<CDeviceCommandListImpl*>(cmd)->CeaseCommandListEvent(nPoolId);
};

static auto lambdaResumeCallback = [](void* cmd, uint nPoolId)
{
	reinterpret_cast<CDeviceCommandListImpl*>(cmd)->ResumeCommandListEvent(nPoolId);
};

D3D12_SHADER_VISIBILITY shaderVisibility[eHWSC_Num + 1] =
{
	D3D12_SHADER_VISIBILITY_VERTEX,     // eHWSC_Vertex
	D3D12_SHADER_VISIBILITY_PIXEL,      // eHWSC_Pixel
	D3D12_SHADER_VISIBILITY_GEOMETRY,   // eHWSC_Geometry
	D3D12_SHADER_VISIBILITY_DOMAIN,     // eHWSC_Domain
	D3D12_SHADER_VISIBILITY_HULL,       // eHWSC_Hull
	D3D12_SHADER_VISIBILITY_ALL,        // eHWSC_Compute
	D3D12_SHADER_VISIBILITY_ALL,        // eHWSC_Num
};

D3D12_DESCRIPTOR_RANGE_TYPE mapDescriptorRange[size_t(SResourceBindPoint::ESlotType::Count)] =
{
	D3D12_DESCRIPTOR_RANGE_TYPE_CBV,     // SResourceBindPoint::ESlotType::ConstantBuffer
	D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     // SResourceBindPoint::ESlotType::TextureAndBuffer
	D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, // SResourceBindPoint::ESlotType::Sampler
	D3D12_DESCRIPTOR_RANGE_TYPE_UAV,     // SResourceBindPoint::ESlotType::UnorderedAccessView
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceObjectFactory::CDeviceObjectFactory()
{
	memset(m_NullResources, 0, sizeof(m_NullResources));

	m_pCoreCommandList.reset(new CDeviceCommandList());
	m_pCoreCommandList->m_sharedState.pCommandList = nullptr;

	m_pDX12Device    = nullptr;
	m_pDX12Scheduler = nullptr;
}

void CDeviceObjectFactory::AssignDevice(D3DDevice* pDevice)
{
	auto pEmulatedDevice = reinterpret_cast<CCryDX12Device*>(pDevice);

	m_pDX12Device    = pEmulatedDevice->GetDX12Device();
	m_pDX12Scheduler = &m_pDX12Device->GetScheduler();

	m_pDX12Scheduler->AddQueueEventCallback(m_pCoreCommandList.get(), lambdaCeaseCallback, NCryDX12::CCommandScheduler::CMDQUEUE_EVENT_CEASE);
	m_pDX12Scheduler->AddQueueEventCallback(m_pCoreCommandList.get(), lambdaResumeCallback, NCryDX12::CCommandScheduler::CMDQUEUE_EVENT_RESUME);

	m_pCoreCommandList->m_sharedState.pCommandList = m_pDX12Scheduler->GetCommandList(CMDQUEUE_GRAPHICS);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSOImpl(const CDeviceGraphicsPSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceGraphicsPSO_DX12>(GetDevice());
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSOImpl(const CDeviceComputePSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceComputePSO_DX12>(GetDevice());
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceResourceSetPtr CDeviceObjectFactory::CreateResourceSet(CDeviceResourceSet::EFlags flags) const
{
	return std::make_shared<CDeviceResourceSet_DX12>(flags);
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const
{
	auto pResult = std::make_shared<CDeviceResourceLayout_DX12>(GetDevice(), resourceLayoutDesc.GetRequiredResourceBindings());
	if (pResult->Init(resourceLayoutDesc))
		return pResult;

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Helper functions for DX12 MultiGPU
ID3D12CommandQueue* CDeviceObjectFactory::GetNativeCoreCommandQueue() const
{
	ID3D12CommandQueue* pD3d12Queue = m_pDX12Scheduler->GetCommandListPool(CMDQUEUE_GRAPHICS).GetD3D12CommandQueue();

#ifdef DX12_LINKEDADAPTER
	if (m_pDX12Device->IsMultiAdapter())
	{
		BroadcastableD3D12CommandQueue<2>* pBroadcastQueue = reinterpret_cast<BroadcastableD3D12CommandQueue<2>*>(pD3d12Queue);
		pD3d12Queue = *(*pBroadcastQueue)[0];
	}
#endif

	return pD3d12Queue;
}

// Acquire one or more command-lists which are independent of the core command-list
// Only one thread is allowed to call functions on this command-list (DX12 restriction).
// The thread that gets the permition is the one calling Begin() on it AFAIS
CDeviceCommandListUPtr CDeviceObjectFactory::AcquireCommandList(EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// In theory this whole function need to be atomic, instead it voids the core command-list(s),
	// Synchronization between different threads acquiring command-lists is deferred to the higher level.
	NCryDX12::CCommandListPool& pQueue = m_pDX12Scheduler->GetCommandListPool(eQueueType);
	m_pDX12Scheduler->CeaseAllCommandQueues(false);

	DX12_PTR(CCommandList) pCL;
	pQueue.AcquireCommandList(pCL);

	m_pDX12Scheduler->ResumeAllCommandQueues();
	auto pResult = stl::make_unique<CDeviceCommandList>();
	pResult->m_sharedState.pCommandList = pCL;
	return pResult;
}

std::vector<CDeviceCommandListUPtr> CDeviceObjectFactory::AcquireCommandLists(uint32 listCount, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// In theory this whole function need to be atomic, instead it voids the core command-list(s),
	// Synchronization between different threads acquiring command-lists is deferred to the higher level.
	NCryDX12::CCommandListPool& pQueue = m_pDX12Scheduler->GetCommandListPool(eQueueType);
	m_pDX12Scheduler->CeaseAllCommandQueues(false);

	std::vector<CDeviceCommandListUPtr> pCommandLists;
	DX12_PTR(CCommandList) pCLs[256];

	// Allocate in chunks of 256
	for (uint32 n = 0; n < listCount; n += 256U)
	{
		const uint32 chunkCount = std::min(listCount - n, 256U);
		pQueue.AcquireCommandLists(chunkCount, pCLs);

		for (uint32 b = 0; b < chunkCount; ++b)
		{
			pCommandLists.emplace_back(stl::make_unique<CDeviceCommandList>());
			pCommandLists.back()->m_sharedState.pCommandList = pCLs[b];
		}
	}

	m_pDX12Scheduler->ResumeAllCommandQueues();
	return std::move(pCommandLists);
}

// Command-list sinks, will automatically submit command-lists in allocation-order
void CDeviceObjectFactory::ForfeitCommandList(CDeviceCommandListUPtr pCommandList, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	NCryDX12::CCommandListPool& pQueue = m_pDX12Scheduler->GetCommandListPool(eQueueType);

	if (pCommandList)
	{
		pQueue.ForfeitCommandList(pCommandList->m_sharedState.pCommandList);
	}
}

void CDeviceObjectFactory::ForfeitCommandLists(std::vector<CDeviceCommandListUPtr> pCommandLists, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	NCryDX12::CCommandListPool& pQueue = m_pDX12Scheduler->GetCommandListPool(eQueueType);

	const uint32 listCount = pCommandLists.size();
	DX12_PTR(CCommandList) pCLs[256];

	// Deallocate in chunks of 256
	for (uint32 n = 0; n < listCount; n += 256U)
	{
		const uint32 chunkCount = std::min(listCount - n, 256U);
		uint32 validCount = 0;

		for (uint32 b = 0; b < chunkCount; ++b)
		{
			if (pCommandLists[b])
			{
				pCLs[validCount++] = pCommandLists[b]->m_sharedState.pCommandList;
			}
		}

		if (validCount)
		{
			pQueue.ForfeitCommandLists(validCount, pCLs);
		}
	}
}

void CDeviceObjectFactory::ReleaseResourcesImpl()
{
	m_pDX12Device->FlushAndWaitForGPU();
}

////////////////////////////////////////////////////////////////////////////
UINT64 CDeviceObjectFactory::QueryFormatSupport(D3DFormat Format)
{
	CD3D9Renderer* rd = gcpRendD3D;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT data = { Format };
	HRESULT hr = GetDX12Device()->GetD3D12Device()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &data, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT));
	if (SUCCEEDED(hr))
	{
		UINT nOptions = data.Support1;

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

HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	query = reinterpret_cast<DeviceFenceHandle>(new UINT64);
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
	delete reinterpret_cast<UINT64*>(query);
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : E_FAIL;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		*handle = m_pDX12Scheduler->InsertFence();
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = query ? S_FALSE : E_FAIL;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		hr = m_pDX12Scheduler->TestForFence(*handle);
		if (hr != S_OK)
		{
			// Can only flush from render thread
			CRY_ASSERT(!flush || !gRenDev->m_pRT || gRenDev->m_pRT->IsRenderThread());

			// Test + Flush + No-Block is okay
			// Test + No-Flush + Block may not be okay, caution advised as it could deadlock
			if (flush)
			{
				m_pDX12Scheduler->FlushToFence(*handle);
			}

			if (block)
			{
				hr = m_pDX12Scheduler->WaitForFence(*handle);
			}
		}
	}
	return hr;
}

////////////////////////////////////////////////////////////////////////////
// SamplerState API

static inline D3D11_TEXTURE_ADDRESS_MODE sAddressMode(ESamplerAddressMode nAddress)
{
	static_assert((eSamplerAddressMode_Wrap   + 1) == D3D11_TEXTURE_ADDRESS_WRAP  , "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Clamp  + 1) == D3D11_TEXTURE_ADDRESS_CLAMP , "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Border + 1) == D3D11_TEXTURE_ADDRESS_BORDER, "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Mirror + 1) == D3D11_TEXTURE_ADDRESS_MIRROR, "AddressMode enum not mappable to D3D11 enum by arithmetic");

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
	CDeviceInputLayout* Layout = nullptr;
	if (!m_pConsumingVertexShader || !m_pConsumingVertexShader->m_pShaderData)
		return Layout;

	const int   nSize = m_pConsumingVertexShader->m_nDataSize;
	const void* pVSData = m_pConsumingVertexShader->m_pShaderData;

	HRESULT hr = E_FAIL;
	if (FAILED(hr = gcpRendD3D->GetDevice().CreateInputLayout(&pLayout.m_Declaration[0], pLayout.m_Declaration.size(), pVSData, nSize, &Layout)))
	{
		return Layout;
	}

	return Layout;
}

////////////////////////////////////////////////////////////////////////////
// Low-level resource management API (TODO: remove D3D-dependency by abstraction)

void CDeviceObjectFactory::AllocateNullResources()
{
	for (enum D3D11_RESOURCE_DIMENSION eType = D3D11_RESOURCE_DIMENSION_UNKNOWN; eType <= D3D11_RESOURCE_DIMENSION_TEXTURE3D; eType = D3D11_RESOURCE_DIMENSION(eType + 1))
	{
		D3DResource* pNullResource = m_NullResources[eType];

		if (pNullResource || (S_OK == gcpRendD3D->GetDevice_Unsynchronized().CreateNullResource(eType, &pNullResource)))
		{
			m_NullResources[eType] = pNullResource;
			m_NullResources[eType]->AddRef();
		}
	}
}

void CDeviceObjectFactory::ReleaseNullResources()
{
	for (enum D3D11_RESOURCE_DIMENSION eType = D3D11_RESOURCE_DIMENSION_UNKNOWN; eType <= D3D11_RESOURCE_DIMENSION_TEXTURE3D; eType = D3D11_RESOURCE_DIMENSION(eType + 1))
	{
		if (auto pNullResource = m_NullResources[eType])
		{
			ULONG refCount = pNullResource->Release();
			assert(refCount == 0);
		}
	}
}

//=============================================================================
#include "DX12/Resource/CCryDX12Resource.hpp"
#include "DX12/Resource/Misc/CCryDX12Buffer.hpp"

#ifdef DEVRES_USE_STAGING_POOL
D3DResource* CDeviceObjectFactory::AllocateStagingResource(D3DResource* pForTex, bool bUpload, void*& pMappedAddress)
{
	D3DResource* pStagingResource = nullptr;

#if 0
	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pForTex);
	NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();
	ID3D12Resource* d3d12Resource = rResource.GetD3D12Resource();

	D3D12_RESOURCE_STATES initialState = bUpload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
	ID3D12Resource* stagingResource = nullptr;
	HRESULT result = GetDX12Device()->CreateOrReuseStagingResource(d3d12Resource, &stagingResource, bUpload);

	if (result == S_OK && stagingResource != nullptr)
	{
		pStagingResource = CCryDX12Buffer::Create(gcpRendD3D->GetDevice_Unsynchronized().GetRealDevice(), stagingResource, initialState);
		stagingResource->Release();

		if (bUpload) // Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
		{
			const D3D12_RANGE sNoRead = { 0, 0 }; // It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin
			const D3D12_RANGE sFullRead = { 0, SIZE_T(rResource.GetSize(0, 1)) };

			result = stagingResource->Map(0, bUpload ? &sNoRead : &sFullRead, &pMappedAddress);
		}
	}
#else
	gcpRendD3D->GetDevice_Unsynchronized().CreateStagingResource(pForTex, &pStagingResource, bUpload);

	if (bUpload) // Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().MapStagingResource(pStagingResource, bUpload, &pMappedAddress);
	}
#endif

	return pStagingResource;
}

void CDeviceObjectFactory::ReleaseStagingResource(D3DResource* pStagingTex)
{
	if (pStagingTex)
	{
#if 0
		if (false /* TODO: rResource.IsPersistentMappable() */) // Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
		{
			ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pStagingTex);
			NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();

			const D3D12_RANGE sNoWrite = { 0, 0 }; // It is valid to specify the CPU didn't write any data by passing a range where End is less than or equal to Begin.
			const D3D12_RANGE sFullWrite = { 0, SIZE_T(rResource.GetSize(0, 1)) };

			rResource.GetD3D12Resource()->Unmap(0, true ? &sFullWrite : &sNoWrite);
		}

		RecycleResource(pStagingTex);
#else
		if (false /* TODO: rResource.IsPersistentMappable() */) // Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
		{
			gcpRendD3D->GetDeviceContext_Unsynchronized().UnmapStagingResource(pStagingTex, true);
		}

		gcpRendD3D->GetDevice_Unsynchronized().ReleaseStagingResource(pStagingTex);
#endif
	}
}
#endif

void CDeviceObjectFactory::ReleaseResource(D3DResource* pResource)
{
	pResource->Release(); // Tracking embedded in destructor of wrapped object
}

void CDeviceObjectFactory::RecycleResource(D3DResource* pResource)
{
	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pResource);
	NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();

	rResource.MakeReusableResource(true);
	pResource->Release(); // Tracking embedded in destructor of wrapped object
}

//=============================================================================

HRESULT CDeviceObjectFactory::Create2DTexture(uint32 nWidth, uint32 nHeight, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI, int32 nESRAMOffset)
{
	D3D11_TEXTURE2D_DESC Desc;
	ZeroStruct(Desc);

	Desc.Width = nWidth;
	Desc.Height = nHeight;
	Desc.MipLevels = nMips;
	Desc.SampleDesc.Count = pTI ? pTI->m_nDstMSAASamples : 1;
	Desc.SampleDesc.Quality = pTI ? pTI->m_nDstMSAAQuality : 0;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.ArraySize = nArraySize;
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

	return Create2DTexture(Desc, nUsage, cClearValue, ppDevTexture, pTI);
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(uint32 nSize, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
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

	return CreateCubeTexture(Desc, nUsage, cClearValue, ppDevTexture, pTI);
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
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
	D3D11_SUBRESOURCE_DATA* pSRD = ConvertToDX11Data(Desc.MipLevels * Desc.ArraySize, pPayload, SRD);
	HRESULT hr = S_OK;

	if (Desc.BindFlags & (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS))
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTarget2D(&Desc, (const FLOAT*)&cClearValue, pSRD, &pD3DTex);
	}
	else
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTexture2D(&Desc, pSRD, &pD3DTex);
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
	D3D11_SUBRESOURCE_DATA* pSRD = ConvertToDX11Data(Desc.MipLevels * Desc.ArraySize, pPayload, SRD);
	HRESULT hr = S_OK;

	if (Desc.BindFlags & (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS))
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTarget2D(&Desc, (const FLOAT*)&cClearValue, pSRD, &pD3DTex);
	}
	else
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTexture2D(&Desc, pSRD, &pD3DTex);
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

HRESULT CDeviceObjectFactory::CreateVolumeTexture(const D3D11_TEXTURE3D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3D11_SUBRESOURCE_DATA* pSRD = ConvertToDX11Data(Desc.MipLevels, pPayload, SRD);
	D3DVolumeTexture* pD3DTex = NULL;
	HRESULT hr = S_OK;

	if (Desc.BindFlags & (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS))
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTarget3D(&Desc, (const FLOAT*)&cClearValue, pSRD, &pD3DTex);
	}
	else
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTexture3D(&Desc, pSRD, &pD3DTex);
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

	D3D11_BUFFER_DESC BufDesc;
	ZeroStruct(BufDesc);

	BufDesc.StructureByteStride = elemSize;
	BufDesc.ByteWidth = nSize * elemSize;
	if ((nUsage & USAGE_CPU_WRITE))
		BufDesc.ByteWidth = CDeviceBufferManager::AlignElementCountForStreaming(nSize, elemSize) * elemSize;

	BufDesc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	BufDesc.Usage = ConvertToDX11Usage(nUsage);
	BufDesc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

	if (BufDesc.Usage != D3D11_USAGE_STAGING)
	{
		BufDesc.BindFlags = ConvertToDX11BindFlags(nBindFlags);
		if (nBindFlags & (BIND_STREAM_OUTPUT | BIND_RENDER_TARGET | BIND_DEPTH_STENCIL))
		{
			CryFatalError("trying to create (currently) unsupported buffer type");
		}
	}
	else
	{
		BufDesc.BindFlags = 0;
	}

	D3D11_SUBRESOURCE_DATA* pSRD = NULL;
	D3D11_SUBRESOURCE_DATA SRD;
	if (pData)
	{
		pSRD = &SRD;

		SRD.pSysMem = pData;
		SRD.SysMemPitch = BufDesc.ByteWidth;
		SRD.SysMemSlicePitch = BufDesc.ByteWidth;
	}

	hr = gcpRendD3D->GetDevice_Unsynchronized().CreateBuffer(&BufDesc, pSRD, ppBuff);
	assert(hr == S_OK);
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
	SIZE_T BeginEndR[2] = { offset, offset + size };
	HRESULT hr = gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Map(
		buffer
		, subresource
		, BeginEndR
		, mode
		, 0
		, &mapped_resource);
	assert(hr == S_OK);
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
	gcpRendD3D->GetDeviceContext_ForMapAndUnmap().MappedWriteToSubresource(buffer, subresource, offset, size, mode, pInDataCPU, numDataBlocks);
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::DownloadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks)
{
	gcpRendD3D->GetDeviceContext_ForMapAndUnmap().MappedReadFromSubresource(buffer, subresource, offset, size, mode, pOutDataCPU, numDataBlocks);
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
