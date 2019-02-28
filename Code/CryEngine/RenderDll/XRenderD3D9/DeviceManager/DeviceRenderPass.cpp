// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "xxhash.h"
#include "DeviceRenderPass.h"
#include "DeviceObjects.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64 CDeviceRenderPassDesc::SHash::operator()(const CDeviceRenderPassDesc& renderPassDesc) const
{
	auto addDescToHash = [](XXH64_state_t& hashState, const CDeviceRenderPassDesc& desc)
	{
		uintptr_t resources[MaxRendertargetCount + MaxOutputUAVCount + 1] =
		{
			desc.m_renderTargets[0].fastCompare, desc.m_renderTargets[1].fastCompare, desc.m_renderTargets[2].fastCompare, desc.m_renderTargets[3].fastCompare,
			desc.m_outputUAVs[0].fastCompare,    desc.m_outputUAVs[1].fastCompare,    desc.m_outputUAVs[2].fastCompare,
			desc.m_depthTarget.fastCompare,
		};

		ResourceViewHandle::ValueType views[MaxRendertargetCount + MaxOutputUAVCount + 1] =
		{
			desc.m_renderTargets[0].view, desc.m_renderTargets[1].view, desc.m_renderTargets[2].view, desc.m_renderTargets[3].view,
			desc.m_outputUAVs[0].view,    desc.m_outputUAVs[1].view,    desc.m_outputUAVs[2].view,
			desc.m_depthTarget.view,
		};

		XXH64_update(&hashState, resources, sizeof(resources));
		XXH64_update(&hashState, views, sizeof(views));
	};

	XXH64_state_t hashState;
	XXH64_reset(&hashState, 0x69);

	addDescToHash(hashState, renderPassDesc);

	return XXH64_digest(&hashState);
}

bool CDeviceRenderPassDesc::SEqual::operator()(const CDeviceRenderPassDesc& lhs, const CDeviceRenderPassDesc& rhs) const
{
	return lhs.m_renderTargets == rhs.m_renderTargets &&
		lhs.m_depthTarget == rhs.m_depthTarget   &&
		lhs.m_outputUAVs == rhs.m_outputUAVs;
}

bool CDeviceRenderPassDesc::SetResources(const CDeviceRenderPassDesc& other)
{
	bool bResourcesInvalidated = false;

	for (size_t slot = 0; slot < m_renderTargets.size(); ++slot)
	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, uint8(slot), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_renderTargets[slot], other.m_renderTargets[slot]);
	}

	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, uint8(-1), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_depthTarget, other.m_depthTarget);
	}

	for (size_t slot = 0; slot < m_outputUAVs.size(); ++slot)
	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::UnorderedAccessView, uint8(slot), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_outputUAVs[slot], other.m_outputUAVs[slot]);
	}

	return bResourcesInvalidated;
}

CDeviceRenderPassDesc::CDeviceRenderPassDesc(const CDeviceRenderPassDesc& other)
{
	m_invalidateCallback = OnResourceInvalidated;
	m_invalidateCallbackOwner = this;

	m_bResourcesInvalidated = SetResources(other);
}

CDeviceRenderPassDesc::CDeviceRenderPassDesc(const CDeviceRenderPassDesc& other, void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback)
{
	m_invalidateCallback = invalidateCallback;
	m_invalidateCallbackOwner = pInvalidateCallbackOwner;

	m_bResourcesInvalidated = SetResources(other);
}

CDeviceRenderPassDesc::~CDeviceRenderPassDesc()
{
	ClearResources();
}

bool CDeviceRenderPassDesc::GetDeviceRendertargetViews(std::array<D3DSurface*, MaxRendertargetCount>& views, int& viewCount) const
{
	viewCount = 0;

	for (auto& rt : m_renderTargets)
	{
		if (!rt.pTexture)
			break;

		ID3D11RenderTargetView* pRenderTargetView = rt.GetDeviceResourceView<ID3D11RenderTargetView>();

		if (!pRenderTargetView)
			return false;

		views[viewCount++] = pRenderTargetView;
	}

	return true;
}

bool CDeviceRenderPassDesc::GetDeviceDepthstencilView(D3DDepthSurface*& pView) const
{
	if (m_depthTarget.pTexture)
	{
		pView = m_depthTarget.GetDeviceResourceView<ID3D11DepthStencilView>();

		if (!pView)
			return false;
	}
	else
	{
		pView = nullptr;
	}

	return true;
}

bool CDeviceRenderPassDesc::SetRenderTarget(uint32 slot, CTexture* pTexture, ResourceViewHandle hView)
{
	CRY_ASSERT(slot < MaxRendertargetCount);
	const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, slot, EShaderStage_Pixel);
	bool result = UpdateResource(bindPoint, m_renderTargets[slot], SResourceBinding(pTexture, hView));
	return result;
}

bool CDeviceRenderPassDesc::SetDepthTarget(CTexture* pTexture, ResourceViewHandle hView)
{
	CRY_ASSERT(!pTexture || !m_renderTargets[0].pTexture || (
		pTexture->GetWidth() == m_renderTargets[0].pTexture->GetWidth() &&
		pTexture->GetHeight() == m_renderTargets[0].pTexture->GetHeight()));

	const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, -1, EShaderStage_Pixel);
	bool result = UpdateResource(bindPoint, m_depthTarget, SResourceBinding(pTexture, hView));
	return result;
}

bool CDeviceRenderPassDesc::SetOutputUAV(uint32 slot, CGpuBuffer* pBuffer)
{
	CRY_ASSERT(slot < MaxOutputUAVCount);
	const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::UnorderedAccessView, slot, EShaderStage_Pixel);
	bool result = UpdateResource(bindPoint, m_outputUAVs[slot], SResourceBinding(pBuffer, EDefaultResourceViews::Default));
	return result;
}

bool CDeviceRenderPassDesc::UpdateResource(SResourceBindPoint bindPoint, SResourceBinding& dstResource, const SResourceBinding& srcResource)
{
	bool bResourcesInvalidated = false;

	if (dstResource.fastCompare != srcResource.fastCompare || dstResource.view != srcResource.view)
	{
		if (dstResource.fastCompare && m_invalidateCallbackOwner)
		{
			dstResource.RemoveInvalidateCallback(m_invalidateCallbackOwner, bindPoint);
		}

		dstResource = srcResource;

		if (srcResource.fastCompare && m_invalidateCallbackOwner)
		{
			srcResource.AddInvalidateCallback(m_invalidateCallbackOwner, bindPoint, m_invalidateCallback);
		}

		m_bResourcesInvalidated = bResourcesInvalidated = true;
	}

	return bResourcesInvalidated;
}

bool CDeviceRenderPassDesc::ClearResources()
{
	bool bResourcesInvalidated = false;

	for (size_t slot = 0; slot < m_renderTargets.size(); ++slot)
	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, uint8(slot), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_renderTargets[slot], SResourceBinding());
	}

	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, uint8(-1), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_depthTarget, SResourceBinding());
	}

	for (size_t slot = 0; slot < m_outputUAVs.size(); ++slot)
	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::UnorderedAccessView, uint8(slot), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_outputUAVs[slot], SResourceBinding());
	}

	return bResourcesInvalidated;
}

bool CDeviceRenderPassDesc::OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe
{
	CDeviceRenderPassDesc* pPass = reinterpret_cast<CDeviceRenderPassDesc*>(pThis);
	bool bResourcesInvalidated = false;

	// Device-resource views have changed
	if (flags & eDeviceResourceViewDirty)
		bResourcesInvalidated = true;
	// Device-resource has changed
	if (flags & eDeviceResourceDirty)
		bResourcesInvalidated = true;
	// Resource has been destructed
	if (flags & eResourceDestroyed)
	{
		UResourceReference pFoundResource = (void*)nullptr;

		if (bindPoint.slotType == SResourceBindPoint::ESlotType::UnorderedAccessView)
		{
			pFoundResource = CryInterlockedCompareExchangePointer((void* volatile*)&pPass->m_outputUAVs[bindPoint.slotNumber], nullptr, pResource.pAnonymous);
		}
		else if (bindPoint.slotType == SResourceBindPoint::ESlotType::TextureAndBuffer)
		{
			if ((int8)bindPoint.slotNumber >= 0)
				pFoundResource = CryInterlockedCompareExchangePointer((void* volatile*)&pPass->m_renderTargets[bindPoint.slotNumber], nullptr, pResource.pAnonymous);
			else
				pFoundResource = CryInterlockedCompareExchangePointer((void* volatile*)&pPass->m_depthTarget, nullptr, pResource.pAnonymous);
		}

		if (pFoundResource.pAnonymous == pResource.pAnonymous)
		{
			bResourcesInvalidated = true;
		}
	}

	if (bResourcesInvalidated)
		pPass->m_bResourcesInvalidated = bResourcesInvalidated;

	// Don't keep the pointer and unregister the callback when the resource goes out of scope
	return !(flags & eResourceDestroyed);
}

CDeviceRenderPass_Base::CDeviceRenderPass_Base()
	: m_nHash(0)
	, m_nUpdateCount(0)
	, m_bValid(true)
{}

bool CDeviceRenderPass_Base::Update(const CDeviceRenderPassDesc& passDesc)
{
	// validate resource formats: must not update pass with different layout as this will make PSOs for this pass invalid
#if !defined(RELEASE)
	std::array<DXGI_FORMAT, CDeviceRenderPassDesc::MaxRendertargetCount + 1> currentFormats;

	int formatCount = 0;
	currentFormats[formatCount++] = passDesc.GetDepthTarget().GetResourceFormat();

	for (const auto& target : passDesc.GetRenderTargets())
	{
		PREFAST_SUPPRESS_WARNING(28020)
			currentFormats[formatCount++] = target.GetResourceFormat();
	}

	if (m_nUpdateCount == 0)
	{
		m_targetFormats = currentFormats;
	}

	CRY_ASSERT(m_targetFormats == currentFormats);
#endif

	m_bValid = UpdateImpl(passDesc);
	++m_nUpdateCount;

	const CDeviceRenderPassDesc::SHash hash;
	m_nHash = hash(passDesc);

	return m_bValid;
}

bool CDeviceRenderPass_Base::UpdateWithReevaluation(CDeviceRenderPassPtr& pRenderPass, CDeviceRenderPassDesc& passDesc)
{
	bool bOutputsDirty = passDesc.HasChanged();

	if (bOutputsDirty) // request new render pass in case resource layout has changed
	{
		pRenderPass = GetDeviceObjectFactory().GetOrCreateRenderPass(passDesc);
		if (pRenderPass->Update(passDesc))
		{
			passDesc.AcceptAllChanges();
		}
	}

	else if (!pRenderPass->IsValid()) // make sure render pass is up to date
	{
		if (pRenderPass->Update(passDesc))
		{
			passDesc.AcceptAllChanges();
		}
	}

	return bOutputsDirty;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSO(const CDeviceGraphicsPSODesc& psoDesc)
{
	CRY_ASSERT(psoDesc.m_pRenderPass != nullptr);

	CDeviceGraphicsPSOPtr pPso;

	auto it = m_GraphicsPsoCache.find(psoDesc);
	if (it != m_GraphicsPsoCache.end())
	{
		pPso = it->second;
	}
	else
	{
		pPso = CreateGraphicsPSOImpl(psoDesc);
		m_GraphicsPsoCache.emplace(psoDesc, pPso);

		if (!pPso->IsValid())
			m_InvalidGraphicsPsos.emplace(psoDesc, pPso);
	}

	pPso->SetLastUseFrame(gRenDev->GetRenderFrameID());
	return pPso;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSO(const CDeviceComputePSODesc& psoDesc)
{
	CDeviceComputePSOPtr pPso;

	auto it = m_ComputePsoCache.find(psoDesc);
	if (it != m_ComputePsoCache.end())
	{
		pPso = it->second;
	}
	else
	{
		pPso = CreateComputePSOImpl(psoDesc);
		m_ComputePsoCache.emplace(psoDesc, pPso);

		if (!pPso->IsValid())
			m_InvalidComputePsos.emplace(psoDesc, pPso);
	}

	pPso->SetLastUseFrame(gRenDev->GetRenderFrameID());
	return pPso;
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayout(const SDeviceResourceLayoutDesc& resourceLayoutDesc)
{
	CDeviceResourceLayoutPtr pResult = nullptr;

	auto it = m_ResourceLayoutCache.find(resourceLayoutDesc);
	if (it != m_ResourceLayoutCache.end())
		return it->second;

	if (resourceLayoutDesc.IsValid())
	{
		if (pResult = CreateResourceLayoutImpl(resourceLayoutDesc))
		{
			m_ResourceLayoutCache[resourceLayoutDesc] = pResult;
		}
	}

	return pResult;
}

void CDeviceObjectFactory::TrimResourceLayouts()
{
	EraseUnusedEntriesFromCache(m_ResourceLayoutCache);
}

const CDeviceInputStream* CDeviceObjectFactory::CreateVertexStreamSet(uint32 numStreams, const SStreamInfo* streams)
{
	TVertexStreams vertexStreams = {};
	bool vertexFilled = false;

	for (int i = 0; i < numStreams; ++i)
		vertexFilled |= !!(vertexStreams[i] = streams[i]);

	return (vertexFilled ? m_uniqueVertexStreams.insert(vertexStreams).first->data() : nullptr);
}

const CDeviceInputStream* CDeviceObjectFactory::CreateIndexStreamSet(const SStreamInfo* stream)
{
	TIndexStreams indexStream = {};
	bool indexFilled = false;

	indexFilled |= !!(indexStream[0] = stream[0]);

	return (indexFilled ? m_uniqueIndexStreams.insert(indexStream).first->data() : nullptr);
}

CDeviceRenderPassPtr CDeviceObjectFactory::GetOrCreateRenderPass(const CDeviceRenderPassDesc& passDesc)
{
	if (auto pRenderPass = GetRenderPass(passDesc))
		return pRenderPass;

	auto pPass = std::make_shared<CDeviceRenderPass>();
	{
		CryAutoCriticalSectionNoRecursive lock(m_RenderPassCacheLock);
		m_RenderPassCache.emplace(std::piecewise_construct,
			std::forward_as_tuple(passDesc, pPass.get(), OnRenderPassInvalidated),
			std::forward_as_tuple(pPass));
	}

	return pPass;
}

CDeviceRenderPassPtr CDeviceObjectFactory::GetRenderPass(const CDeviceRenderPassDesc& passDesc)
{
	CryAutoCriticalSectionNoRecursive lock(m_RenderPassCacheLock);

	auto it = m_RenderPassCache.find(passDesc);
	if (it != m_RenderPassCache.end())
		return it->second;

	return nullptr;
}


const CDeviceRenderPassDesc* CDeviceObjectFactory::GetRenderPassDesc(const CDeviceRenderPass_Base* pPass)
{
	CryAutoCriticalSectionNoRecursive lock(m_RenderPassCacheLock);

	for (const auto& it : m_RenderPassCache)
	{
		if (it.second.get() == pPass)
		{
			return &it.first;
		}
	}

	return nullptr;
}

