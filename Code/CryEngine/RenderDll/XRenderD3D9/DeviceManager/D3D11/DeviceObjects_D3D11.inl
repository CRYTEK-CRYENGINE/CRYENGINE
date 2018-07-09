// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !CRY_RENDERER_VULKAN

#if CRY_RENDERER_GNM
	#error This file should not be included for GNM renderer (DXOrbis does though)
#elif !defined(CRY_PLATFORM_ORBIS)

class CDeviceTimestampGroup : public CDeviceTimestampGroup_Base<CDeviceTimestampGroup>
{
public:
	CDeviceTimestampGroup();
	~CDeviceTimestampGroup();
	
	void Init();

	void BeginMeasurement();
	void EndMeasurement();

	uint32 IssueTimestamp(CDeviceCommandList* pCommandList);
	bool ResolveTimestamps();

	uint64 GetTime(uint32 timestamp)
	{
		return m_timeValues[timestamp];
	}
	
	float GetTimeMS(uint32 timestamp0, uint32 timestamp1)
	{
		uint64 duration = std::max(m_timeValues[timestamp0], m_timeValues[timestamp1]) - std::min(m_timeValues[timestamp0], m_timeValues[timestamp1]);
		return duration / (float)(m_frequency / 1000);
	}

protected:
	uint32                                    m_numTimestamps;
	
	std::array<ID3D11Query*, kMaxTimestamps>  m_timestampQueries;
	ID3D11Query*                              m_pDisjointQuery;

	UINT64                                    m_frequency;
	std::array<uint64, kMaxTimestamps>        m_timeValues;

	bool                                      m_measurable;
};

#else

// DXOrbis implementation
class CDeviceTimestampGroup : public CDeviceTimestampGroup_Base<CDeviceTimestampGroup>
{
public:
	~CDeviceTimestampGroup()
	{
		DXOrbis::FreeVideoMemoryDelayed(m_gpuMemory);
	}

	void Init()
	{
		m_gpuMemory = DXOrbis::AllocatePinnedVideoMemory(DXOrbis::kMemVideoShared, sizeof(uint64) * kMaxTimestamps, sizeof(uint64));
		m_pTimestamps = static_cast<uint64*>(m_gpuMemory.GetAddress());
	}

	void BeginMeasurement()
	{
		m_timestampIndex = 0;
		m_lastIssued = 0;
	}

	void EndMeasurement()
	{
		// Empty on purpose
	}

	uint32 IssueTimestamp(CDeviceCommandList* pCommandList)
	{
		if (m_timestampIndex < kMaxTimestamps)
		{
			const uint32 index = m_timestampIndex++;
			m_lastIssued = DXOrbis::Device()->IssueTimestamp(m_pTimestamps + index);
			return index;
		}
		return kMaxTimestamps;
	}

	bool ResolveTimestamps()
	{
		if (m_lastIssued)
		{
			const GpuSyncId gpuLastExecuted = DXOrbis::Device()->GetLastCompletedDrawCall();
			return gpuLastExecuted > m_lastIssued;
		}
		return true; // Nothing was issued
	}

	uint64 GetTime(uint32 timestamp)
	{
		return m_timeValues[timestamp];
	}

	float GetTimeMS(uint32 timestamp0, uint32 timestamp1)
	{
		const float invGpuClockRate = 1.0f / (800.0f * 1000.0f); // sce::Gnm::kEventWriteSourceGpuCoreClockCounter runs at 800 MHz
		if (std::max(timestamp0, timestamp1) < kMaxTimestamps)
		{
			const uint64 firstStampValue = m_pTimestamps[timestamp0];
			const uint64 secondStampValue = m_pTimestamps[timestamp1];
			return (float)(std::max(firstStampValue, secondStampValue) - std::min(firstStampValue, secondStampValue)) * invGpuClockRate;
		}
		return 0.0f;
	}

private:
	uint32                    m_timestampIndex = 0;
	GpuSyncId                 m_lastIssued = 0;
	uint64*                   m_pTimestamps;
	DXOrbis::DXOrbisMemHandle m_gpuMemory;
};

#endif

class CDeviceRenderPass final : public CDeviceRenderPass_Base
{
private:
	virtual bool UpdateImpl(const CDeviceRenderPassDesc& passDesc) override;

public:
	std::array<ID3D11RenderTargetView*, CDeviceRenderPassDesc::MaxRendertargetCount>    m_RenderTargetViews;
	int                                                                                 m_RenderTargetCount;
	ID3D11DepthStencilView*                                                             m_pDepthStencilView;

	// NOTE: UAVs are currently bound via resource set only
	// std::array<ID3D11UnorderedAccessView*, CDeviceRenderPassDesc::MaxOutputUAVCount> m_UnorderedAccessViews;
};

////////////////////////////////////////////////////////////////////////////
template<typename T>
static inline D3D11_BIND_FLAG ConvertToDX11BindFlags(const T& desc)
{
	CRY_ASSERT_MESSAGE((desc & (CDeviceObjectFactory::BIND_RENDER_TARGET | CDeviceObjectFactory::BIND_DEPTH_STENCIL)) != (CDeviceObjectFactory::BIND_RENDER_TARGET | CDeviceObjectFactory::BIND_DEPTH_STENCIL), "RenderTarget and DepthStencil can't be requested together!");

	// *INDENT-OFF*
	return D3D11_BIND_FLAG(
		((desc & CDeviceObjectFactory::BIND_VERTEX_BUFFER   ) ? D3D11_BIND_VERTEX_BUFFER              : 0) |
		((desc & CDeviceObjectFactory::BIND_INDEX_BUFFER    ) ? D3D11_BIND_INDEX_BUFFER               : 0) |
		((desc & CDeviceObjectFactory::BIND_CONSTANT_BUFFER ) ? D3D11_BIND_CONSTANT_BUFFER            : 0) |
		((desc & CDeviceObjectFactory::BIND_SHADER_RESOURCE ) ? D3D11_BIND_SHADER_RESOURCE            : 0) |
	//	((desc & CDeviceObjectFactory::BIND_STREAM_OUTPUT   ) ? D3D11_BIND_STREAM_OUTPUT              : 0) |
		((desc & CDeviceObjectFactory::BIND_RENDER_TARGET   ) ? D3D11_BIND_RENDER_TARGET              : 0) |
		((desc & CDeviceObjectFactory::BIND_DEPTH_STENCIL   ) ? D3D11_BIND_DEPTH_STENCIL              : 0) |
		((desc & CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? D3D11_BIND_UNORDERED_ACCESS           : 0));
	// *INDENT-ON*
}

template<typename T>
static inline D3D11_RESOURCE_MISC_FLAG ConvertToDX11MiscFlags(const T& desc)
{
	// D3D11_RESOURCE_MISC_RESOURCE_CLAMP and D3D11_RESOURCE_MISC_GENERATE_MIPS are mutually exclusive.
	// *INDENT-OFF*
	return D3D11_RESOURCE_MISC_FLAG(
		((desc & CDeviceObjectFactory::USAGE_AUTOGENMIPS ) ? D3D11_RESOURCE_MISC_GENERATE_MIPS          :
		((desc & CDeviceObjectFactory::USAGE_STREAMING   ) ? D3D11_RESOURCE_MISC_RESOURCE_CLAMP         : 0)  |
		((desc & CDeviceObjectFactory::USAGE_LODABLE     ) ? D3D11_RESOURCE_MISC_RESOURCE_CLAMP         : 0)) |
		((desc & CDeviceObjectFactory::USAGE_STRUCTURED  ) ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED      : 0)  |
		((desc & CDeviceObjectFactory::USAGE_RAW         ) ? D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS : 0)  |
		((desc & CDeviceObjectFactory::USAGE_INDIRECTARGS) ? D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS      : 0)  |
		((desc & CDeviceObjectFactory::USAGE_UAV_OVERLAP ) ? D3D11_RESOURCE_MISC_UAV_OVERLAP            : 0)  |
		((desc & CDeviceObjectFactory::USAGE_HIFREQ_HEAP ) ? D3D11_RESOURCE_MISC_HIFREQ_HEAP            : 0));
	// *INDENT-ON*
}

template<typename T>
static inline D3D11_CPU_ACCESS_FLAG ConvertToDX11CPUAccessFlags(const T& desc)
{
	// *INDENT-OFF*
	return D3D11_CPU_ACCESS_FLAG(
		((desc & CDeviceObjectFactory::USAGE_CPU_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0)  |
		((desc & CDeviceObjectFactory::USAGE_CPU_READ ) ? D3D11_CPU_ACCESS_READ  : 0));
	// *INDENT-ON*
}

template<typename T>
static inline D3D11_USAGE ConvertToDX11Usage(const T& desc)
{
	CRY_ASSERT_MESSAGE((desc & (CDeviceObjectFactory::USAGE_CPU_READ | CDeviceObjectFactory::USAGE_CPU_WRITE)) != (CDeviceObjectFactory::USAGE_CPU_READ | CDeviceObjectFactory::USAGE_CPU_WRITE), "CPU Read and Write can't be requested together!");

	// *INDENT-OFF*
	return D3D11_USAGE(
		((desc & CDeviceObjectFactory::USAGE_CPU_READ ) ? D3D11_USAGE_STAGING :
		((desc & CDeviceObjectFactory::USAGE_CPU_WRITE) ? D3D11_USAGE_DYNAMIC :
		                                                  D3D11_USAGE_DEFAULT)));
	// *INDENT-ON*
}

static inline D3D11_SUBRESOURCE_DATA* ConvertToDX11Data(uint32 numSubs, const STexturePayload* pSrc, D3D11_SUBRESOURCE_DATA* pDst)
{
	if (!pSrc || !pSrc->m_pSysMemSubresourceData)
		return nullptr;

	for (uint32 i = 0; i < numSubs; ++i)
	{
		pDst[i].pSysMem = pSrc->m_pSysMemSubresourceData[i].m_pSysMem;
		pDst[i].SysMemPitch = pSrc->m_pSysMemSubresourceData[i].m_sSysMemAlignment.rowStride;
		pDst[i].SysMemSlicePitch = pSrc->m_pSysMemSubresourceData[i].m_sSysMemAlignment.planeStride;

#if CRY_PLATFORM_ORBIS
		pDst[i].SysMemTileMode = (D3D11_TEXTURE_TILE_MODE)pSrc.m_eSysMemTileMode;
#endif
	}

	// Terminator
	pDst[numSubs].pSysMem = nullptr;
	pDst[numSubs].SysMemPitch = 0;
	pDst[numSubs].SysMemSlicePitch = 0;

	return pDst;
}

////////////////////////////////////////////////////////////////////////////
// Low-level resource management API (TODO: remove D3D-dependency by abstraction)

inline D3DResource* CDeviceObjectFactory::GetNullResource(D3D11_RESOURCE_DIMENSION eType)
{
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////

inline CDeviceCommandListRef CDeviceObjectFactory::GetCoreCommandList() const
{
	return *m_pCoreCommandList.get();
}

#endif