// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
		return m_timestampData[timestamp];
	}

	float GetTimeMS(uint32 timestamp0, uint32 timestamp1);

private:
	uint32                              m_numTimestamps;
	VkQueryPool                         m_queryPool;
	DeviceFenceHandle                   m_fence;
	std::array<uint64, kMaxTimestamps>  m_timestampData;

	bool                                m_measurable;
};

////////////////////////////////////////////////////////////////////////////
class CDeviceRenderPass final : public CDeviceRenderPass_Base
{
public:
	CDeviceRenderPass();
	virtual ~CDeviceRenderPass();

private:
	virtual bool UpdateImpl(const CDeviceRenderPassDesc& passDesc) override;

	bool InitVkRenderPass(const CDeviceRenderPassDesc& passDesc);
	bool InitVkFrameBuffer(const CDeviceRenderPassDesc& passDesc);

public:
	VkRenderPass                                                                         m_renderPassHandle;
	VkFramebuffer                                                                        m_frameBufferHandle;
	VkExtent2D                                                                           m_frameBufferExtent;
	std::array<NCryVulkan::CImageResource*, CDeviceRenderPassDesc::MaxRendertargetCount> m_renderTargets;
	NCryVulkan::CImageResource*                                                          m_pDepthStencilTarget;
	int                                                                                  m_RenderTargetCount;
};

////////////////////////////////////////////////////////////////////////////
template<typename T>
static inline VkImageUsageFlagBits ConvertToVKImageUsageBits(const T& desc)
{
	CRY_ASSERT_MESSAGE((desc & (CDeviceObjectFactory::BIND_RENDER_TARGET | CDeviceObjectFactory::BIND_DEPTH_STENCIL)) != (CDeviceObjectFactory::BIND_RENDER_TARGET | CDeviceObjectFactory::BIND_DEPTH_STENCIL), "RenderTarget and DepthStencil can't be requested together!");

	// *INDENT-OFF*
	return VkImageUsageFlagBits(
		((desc & CDeviceObjectFactory::BIND_SHADER_RESOURCE ) ? VK_IMAGE_USAGE_SAMPLED_BIT                  : 0) |
		((desc & CDeviceObjectFactory::BIND_RENDER_TARGET   ) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         : 0) |
		((desc & CDeviceObjectFactory::BIND_DEPTH_STENCIL   ) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
		((desc & CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? VK_IMAGE_USAGE_STORAGE_BIT                  : 0) |
		                                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT                  |
		                                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT                 );
	// *INDENT-ON*
}

template<typename T>
static inline VkBufferUsageFlagBits ConvertToVKBufferUsageBits(const T& desc)
{
	// *INDENT-OFF*
	return VkBufferUsageFlagBits(
		((desc & CDeviceObjectFactory::BIND_VERTEX_BUFFER   ) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT           : 0) |
		((desc & CDeviceObjectFactory::BIND_INDEX_BUFFER    ) ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT            : 0) |
		((desc & CDeviceObjectFactory::BIND_CONSTANT_BUFFER ) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT          : 0) |
		((desc & CDeviceObjectFactory::USAGE_STRUCTURED     ) ? // Constant buffer are structured buffers by definition
		((desc & CDeviceObjectFactory::BIND_SHADER_RESOURCE ) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT               |
		                              /* Workaround for >64k */ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT          : 0) |
		((desc & CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT          : 0) : 0) |
		((desc & CDeviceObjectFactory::BIND_SHADER_RESOURCE ) ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT         |
		                              /* Workaround for >64k */ VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT    : 0) |
		((desc & CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT    : 0) |
		((desc & CDeviceObjectFactory::USAGE_INDIRECTARGS   ) ? VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT         : 0) |
		                                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT                  |
		                                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT                 );
	// *INDENT-ON*
}

template<typename T>
static inline VkImageTiling ConvertToVKImageTiling(const T& desc)
{
	// *INDENT-OFF*
	return VkImageTiling(
		((desc & (CDeviceObjectFactory::USAGE_CPU_READ |
		          CDeviceObjectFactory::USAGE_CPU_WRITE) ? VK_IMAGE_TILING_LINEAR :
		                                                   VK_IMAGE_TILING_OPTIMAL)));
	// *INDENT-ON*
}

////////////////////////////////////////////////////////////////////////////
// Low-level resource management API (TODO: remove D3D-dependency by abstraction)

inline D3DResource* CDeviceObjectFactory::GetNullResource(D3D11_RESOURCE_DIMENSION eType)
{
	assert(m_NullResources[eType]);
	m_NullResources[eType]->AddRef();
	return m_NullResources[eType];
}

////////////////////////////////////////////////////////////////////////////

inline CDeviceCommandListRef CDeviceObjectFactory::GetCoreCommandList() const
{
	// Sanity check
	CRY_ASSERT(m_pCoreCommandList->m_sharedState.pCommandList == m_pVKScheduler->GetCommandList(CMDQUEUE_GRAPHICS));

	return *m_pCoreCommandList.get();
}
