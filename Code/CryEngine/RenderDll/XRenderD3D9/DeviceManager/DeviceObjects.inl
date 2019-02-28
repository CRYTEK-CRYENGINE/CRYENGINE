// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool CDeviceResourceSetDesc::IsEmpty() const { return m_resources.empty(); }
inline CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::GetDirtyFlags() const { return (CDeviceResourceSetDesc::EDirtyFlags)m_dirtyFlags.load(); }
inline bool CDeviceResourceSetDesc::HasChanged() const { return m_dirtyFlags != CDeviceResourceSetDesc::EDirtyFlags::eNone; }
inline void CDeviceResourceSetDesc::AcceptAllChanges() { m_dirtyFlags = CDeviceResourceSetDesc::EDirtyFlags::eNone; }

inline void CDeviceResourceSetDesc::MarkBindingChanged() { m_dirtyFlags |= CDeviceResourceSetDesc::EDirtyFlags::eDirtyBinding; }

inline bool CDeviceResourceSetDesc::HasChangedBindPoints() { return m_dirtyFlags & CDeviceResourceSetDesc::EDirtyFlags::eDirtyBindPoint; }
inline void CDeviceResourceSetDesc::AcceptChangedBindPoints() { m_dirtyFlags &= ~CDeviceResourceSetDesc::EDirtyFlags::eDirtyBindPoint; }

template<typename S>
static inline ResourceViewHandle GetConstantBufferView_ForceTwoPhase(S* pBuffer) { return pBuffer->m_nUpdCount; }

inline CDeviceResourceSetDesc::CDeviceResourceSetDesc()
{
	m_invalidateCallbackOwner = this;
	m_invalidateCallback      = OnResourceInvalidated;

	m_dirtyFlags              = eDirtyAll;
}

inline CDeviceResourceSetDesc::CDeviceResourceSetDesc(void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback)
{
	m_invalidateCallbackOwner = pInvalidateCallbackOwner;
	m_invalidateCallback      = invalidateCallback;

	m_dirtyFlags              = eDirtyAll;
}

inline CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::SetConstantBuffer(int shaderSlot, CConstantBuffer* pBuffer, ::EShaderStage shaderStages)
{
	// NOTE: temporary work-around for not having views for constant buffers
	SResourceBinding resource(pBuffer, GetConstantBufferView_ForceTwoPhase<CConstantBuffer>(pBuffer));
	SResourceBindPoint bindPoint(resource, shaderSlot, shaderStages);

	return UpdateResource<SResourceBinding::EResourceType::ConstantBuffer>(bindPoint, resource);
}

inline CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::SetTexture(int shaderSlot, CTexture* pTexture, ResourceViewHandle hView, ::EShaderStage shaderStages)
{
	SResourceBinding resource(pTexture, hView);
	SResourceBindPoint bindPoint(resource, shaderSlot, shaderStages);
	
	return UpdateResource<SResourceBinding::EResourceType::Texture>(bindPoint, resource);
}

inline CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::SetSampler(int shaderSlot, SamplerStateHandle hState, ::EShaderStage shaderStages)
{
	SResourceBinding resource(hState);
	SResourceBindPoint bindPoint(resource, shaderSlot, shaderStages);

	return UpdateResource<SResourceBinding::EResourceType::Sampler>(bindPoint, resource);
}

inline CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::SetBuffer(int shaderSlot, CGpuBuffer* pBuffer, ResourceViewHandle hView, ::EShaderStage shaderStages)
{
	SResourceBinding resource(pBuffer, hView);
	SResourceBindPoint bindPoint(resource, shaderSlot, shaderStages);

	return UpdateResource<SResourceBinding::EResourceType::Buffer>(bindPoint, resource);
}

inline bool SDeviceResourceLayoutDesc::SLayoutBindPoint::operator==(const SLayoutBindPoint& other) const 
{ 
	return slotType == other.slotType && layoutSlot == other.layoutSlot;
};

inline bool SDeviceResourceLayoutDesc::SLayoutBindPoint::operator<(const SLayoutBindPoint& other) const 
{ 
	if (slotType != other.slotType)
		return slotType < other.slotType;

	return layoutSlot < other.layoutSlot;
}

inline CDeviceRenderPassDesc::CDeviceRenderPassDesc()
{
	m_invalidateCallback      = OnResourceInvalidated;
	m_invalidateCallbackOwner = this;

	m_bResourcesInvalidated   = true;
}

inline CDeviceRenderPassDesc::CDeviceRenderPassDesc(void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback)
{
	m_invalidateCallback      = invalidateCallback;
	m_invalidateCallbackOwner = pInvalidateCallbackOwner;

	m_bResourcesInvalidated   = true;
}

template <class Impl> inline void CDeviceTimestampGroup_Base<Impl>::Init()
{
	((Impl*)this)->Init();
}

template <class Impl> inline void CDeviceTimestampGroup_Base<Impl>::BeginMeasurement()
{
	((Impl*)this)->BeginMeasurement();
}

template <class Impl> inline void CDeviceTimestampGroup_Base<Impl>::EndMeasurement()
{
	((Impl*)this)->EndMeasurement();
}

template <class Impl> inline uint32 CDeviceTimestampGroup_Base<Impl>::IssueTimestamp()
{
	return ((Impl*)this)->IssueTimestamp();
}

template <class Impl> inline bool CDeviceTimestampGroup_Base<Impl>::ResolveTimestamps()
{
	return ((Impl*)this)->ResolveTimestamps();
}

template <class Impl> inline float CDeviceTimestampGroup_Base<Impl>::GetTimeMS(uint32 timestamp0, uint32 timestamp1)
{
	return ((Impl*)this)->GetTimeMS(timestamp0, timestamp1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// On UMA system, return the pointer to the start of the buffer
inline void CDeviceObjectFactory::ExtractBasePointer(D3DBuffer* buffer, D3D11_MAP mode, uint8*& base_ptr)
{
#if BUFFER_ENABLE_DIRECT_ACCESS
	#if CRY_RENDERER_GNM
		base_ptr = buffer->GnmGetBaseAddress();
	#elif CRY_PLATFORM_ORBIS
		base_ptr = (uint8*)buffer->GetData();
	#elif CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		// Note: temporary solution, this should be removed as soon as the device
		// layer for Durango is available
		void* data;
		unsigned size = sizeof(data);
		HRESULT hr = buffer->GetPrivateData(BufferPointerGuid, &size, &data);
		assert(hr == S_OK);
		base_ptr = reinterpret_cast<uint8*>(data);
	#elif (CRY_RENDERER_DIRECT3D >= 120)
		base_ptr = CDeviceObjectFactory::Map(buffer, 0, 0, 0, mode /* MAP_DISCARD could affect the ptr */);
	#elif CRY_RENDERER_VULKAN
		base_ptr = (uint8*)buffer->Map();
	#else
		base_ptr = NULL;
	#endif
#else
	base_ptr = NULL;
#endif
}

inline void CDeviceObjectFactory::ReleaseBasePointer(D3DBuffer* buffer)
{
#if BUFFER_ENABLE_DIRECT_ACCESS
	#if (CRY_RENDERER_DIRECT3D >= 120)
		CDeviceObjectFactory::Unmap(buffer, 0, 0, 0, D3D11_MAP(0));
	#elif defined(CRY_RENDERER_VULKAN)
		buffer->Unmap();
	#endif
#endif
}

inline uint8 CDeviceObjectFactory::MarkReadRange(D3DBuffer* buffer, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
#if (CRY_RENDERER_DIRECT3D >= 120)
	DX12_ASSERT(mode == D3D11_MAP_READ || mode == D3D11_MAP_WRITE, "No other access specifier than READ/WRITE allowed for marking!");
	CDeviceObjectFactory::Map(buffer, 0, offset, (mode & D3D11_MAP_READ ? size : 0U), D3D11_MAP(0));
#elif defined(CRY_RENDERER_VULKAN)
	// TODO: flush
#endif

	return uint8(mode);
}

inline uint8 CDeviceObjectFactory::MarkWriteRange(D3DBuffer* buffer, buffer_size_t offset, buffer_size_t size, uint8 marker)
{
#if (CRY_RENDERER_DIRECT3D >= 120)
	DX12_ASSERT(marker == D3D11_MAP_READ || marker == D3D11_MAP_WRITE, "No other access specifier than READ/WRITE allowed for marking!");
	CDeviceObjectFactory::Unmap(buffer, 0, offset, (marker & D3D11_MAP_WRITE ? size : 0U), D3D11_MAP(0));
#elif defined(CRY_RENDERER_VULKAN)
	// TODO: flush
#endif

	return uint8(marker);
}

// Local helper function to erase items with refcount 1 from some cache of shared pointers
template<typename TCache>
inline void EraseUnusedEntriesFromCache(TCache& cache)
{
	for (auto it = cache.begin(); it != cache.end(); )
	{
		it = it->second.use_count() == 1 ?
			cache.erase(it) :
			std::next(it);
	}
}

// Local helper function to erase expired-items from some cache of weak pointers
template<typename TCache>
inline void EraseExpiredEntriesFromCache(TCache& cache)
{
	for (auto it = cache.begin(); it != cache.end(); )
	{
		it = it->second.expired() ?
			cache.erase(it) :
			std::next(it);
	}
}

template<typename TCache>
inline void EraseExpiredEntriesFromCache(TCache& cache, int currentFrame, int eraseBeforeFrame)
{
	for (auto it = cache.begin(); it != cache.end(); )
	{
		auto  itCurrent   = it++;
		auto& pCacheEntry = itCurrent->second;

		if (pCacheEntry.use_count() == 1)
		{
			if (pCacheEntry->GetLastUseFrame() < eraseBeforeFrame)
				cache.erase(itCurrent);
		}
		else
		{
			pCacheEntry->SetLastUseFrame(currentFrame);
		}
	}
}

inline void CDeviceObjectFactory::OnEndFrame(int frameID)
{
	// Garbage collect native API resources and objects
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	m_pDX11Scheduler->EndOfFrame(false);

#if CRY_PLATFORM_DURANGO
	m_texturePool.RT_Tick();
#endif
#endif

	// Garbage collect device layer resources and objects
	TrimPipelineStates(frameID, frameID - UnusedPsoKeepAliveFrames);
}

inline void CDeviceObjectFactory::OnBeginFrame(int frameID)
{
#if CRY_RENDERER_VULKAN
	UpdateDeferredUploads();
#endif
}
