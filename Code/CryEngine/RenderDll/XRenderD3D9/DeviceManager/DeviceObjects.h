// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/VectorMap.h>   // VectorMap
#include <CryRenderer/VertexFormats.h>
#include <CryRenderer/ITexture.h>
#include <array>
#include <bitset>
#include <atomic>

#include "DeviceResources.h"                // CDeviceBuffer, CDeviceTexture, CDeviceInputStream
#include "Common/CommonRender.h"            // SResourceView, SSamplerState, SInputLayout
#include "Common/Shaders/ShaderCache.h"     // UPipelineState

class CRendererCVars;
class CHWShader_D3D;
class CShader;
class CTexture;
class CCryNameTSCRC;
class CCryDeviceWrapper;
class CConstantBuffer;
class CDeviceBuffer;
class CDeviceTexture;
class CShaderResources;
struct SGraphicsPipelineStateDescription;
struct SComputePipelineStateDescription;
class CDeviceRenderPass;

#include "DeviceResourceSet.h"
#include "DevicePSO.h"
#include "DeviceCommandListCommon.h"
#include "DeviceRenderPass.h"


////////////////////////////////////////////////////////////////////////////
// Device Object Factory

#include "DeviceObjectHelpers.h"            // CStaticDeviceObjectStorage

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	#if CRY_PLATFORM_DURANGO
		#include "D3D11/DeviceObjects_D3D11_Durango.h"
	#endif
#endif

// Fence API (TODO: offload all to CDeviceFenceHandle)
typedef uintptr_t DeviceFenceHandle;

struct SInputLayoutCompositionDescriptor
{
	const InputLayoutHandle VertexFormat;
	const uint8_t StreamMask;
	const bool bInstanced;
	const uint8_t ShaderMask;

	static uint8_t GenerateShaderMask(const InputLayoutHandle VertexFormat, ID3D11ShaderReflection* pShaderReflection);

	SInputLayoutCompositionDescriptor(InputLayoutHandle VertexFormat, int Stream, ID3D11ShaderReflection* pShaderReflection) noexcept
		: VertexFormat(VertexFormat), StreamMask(static_cast<uint8_t>(Stream % MASK(VSF_NUM))), bInstanced((StreamMask & VSM_INSTANCED) != 0), ShaderMask(GenerateShaderMask(VertexFormat, pShaderReflection))
	{}

	SInputLayoutCompositionDescriptor(SInputLayoutCompositionDescriptor&&) = default;
	SInputLayoutCompositionDescriptor &operator=(SInputLayoutCompositionDescriptor&&) = default;
	SInputLayoutCompositionDescriptor(const SInputLayoutCompositionDescriptor&) = default;
	SInputLayoutCompositionDescriptor &operator=(const SInputLayoutCompositionDescriptor&) = default;

	bool operator==(const SInputLayoutCompositionDescriptor &rhs) const noexcept
	{
		return VertexFormat == rhs.VertexFormat && StreamMask == rhs.StreamMask && ShaderMask == rhs.ShaderMask;
	}
	bool operator!=(const SInputLayoutCompositionDescriptor &rhs) const noexcept { return !(*this == rhs); }

	struct hasher 
	{
		size_t operator()(const SInputLayoutCompositionDescriptor &d) const noexcept
		{
			const auto x = static_cast<size_t>(d.StreamMask) | (static_cast<size_t>(d.ShaderMask) << 8) | (static_cast<size_t>(d.VertexFormat.value) << 16) | (static_cast<size_t>(d.bInstanced) << 24);
			return std::hash<size_t>()(x);
		}
	};
};

class CDeviceObjectFactory
{
	static CDeviceObjectFactory m_singleton;

	CDeviceObjectFactory();
	~CDeviceObjectFactory();

public:
	void AssignDevice(D3DDevice* pDevice);

	static ILINE CDeviceObjectFactory& GetInstance()
	{
		return m_singleton;
	}

	static ILINE void ResetInstance()
	{
		m_singleton.TrimResources();

		CRY_ASSERT(m_singleton.m_InvalidGraphicsPsos.empty());
		CRY_ASSERT(m_singleton.m_InvalidComputePsos.empty());
		CRY_ASSERT(m_singleton.m_GraphicsPsoCache.empty());
		CRY_ASSERT(m_singleton.m_ComputePsoCache.empty());
		CRY_ASSERT(m_singleton.m_RenderPassCache.empty());
	}

	static ILINE void DestroyInstance()
	{
		m_singleton.ReleaseResources();
		memset(&m_singleton, 0xdf, sizeof(m_singleton));
	}

	void OnEndFrame(int frameID);
	void OnBeginFrame(int frameID);

	UINT64 QueryFormatSupport(D3DFormat Format);

	////////////////////////////////////////////////////////////////////////////
	// Fence API (TODO: offload all to CDeviceFenceHandle)

	HRESULT CreateFence(DeviceFenceHandle& query);
	HRESULT ReleaseFence(DeviceFenceHandle query);
	HRESULT IssueFence(DeviceFenceHandle query);
	HRESULT SyncFence(DeviceFenceHandle query, bool block, bool flush = true);

	template<typename S = CRendererCVars>
	void    SyncToGPU() const { if (S::CV_r_enable_full_gpu_sync) FlushToGPU(true); }
	void    FlushToGPU(bool bWait = false, bool bGarbageCollect = false) const;

	////////////////////////////////////////////////////////////////////////////
	// SamplerState API
	
	static void AllocatePredefinedSamplerStates();
	static void TrimSamplerStates();
	static void ReleaseSamplerStates();
	static void ReserveSamplerStates(const uint32 hNum) { s_SamplerStates.Reserve(hNum); }

	static SamplerStateHandle GetOrCreateSamplerStateHandle(const SSamplerState& pState) { return s_SamplerStates.GetOrCreateHandle(pState); }
	static const std::pair<SSamplerState, CDeviceSamplerState*>& LookupSamplerState(const SamplerStateHandle hState) { return s_SamplerStates.Lookup(hState); }

	////////////////////////////////////////////////////////////////////////////
	// InputLayout API
	
	static void AllocatePredefinedInputLayouts();
	static void TrimInputLayouts();
	static void ReleaseInputLayouts();

public:
	using SInputLayoutPair = std::pair<SInputLayout, CDeviceInputLayout*>;

	static const SInputLayout* GetInputLayoutDescriptor(const InputLayoutHandle VertexFormat);

	// Higher level input-layout composition / / / / / / / / / / / / / / / / / /
	static SInputLayout            CreateInputLayoutForPermutation(const SShaderBlob* m_pConsumingVertexShader, const SInputLayoutCompositionDescriptor &compositionDescription, int StreamMask, const InputLayoutHandle VertexFormat);
	static const SInputLayoutPair* GetOrCreateInputLayout(const SShaderBlob* pVS, int StreamMask, const InputLayoutHandle VertexFormat);
	static const SInputLayoutPair* GetOrCreateInputLayout(const SShaderBlob* pVS, const InputLayoutHandle VertexFormat);
	static InputLayoutHandle       CreateCustomVertexFormat(size_t numDescs, const D3D11_INPUT_ELEMENT_DESC* inputLayout);

	////////////////////////////////////////////////////////////////////////////
	// PipelineState API

	CDeviceGraphicsPSOPtr    CreateGraphicsPSO(const CDeviceGraphicsPSODesc& psoDesc);
	CDeviceComputePSOPtr     CreateComputePSO(const CDeviceComputePSODesc& psoDesc);

	void                     ReloadPipelineStates(int currentFrameID);
	void                     UpdatePipelineStates();
	void                     TrimPipelineStates(int currentFrameID, int trimBeforeFrameID = std::numeric_limits<int>::max());

	////////////////////////////////////////////////////////////////////////////
	// Input dataset(s) API

	CDeviceResourceSetPtr     CreateResourceSet(CDeviceResourceSet::EFlags flags = CDeviceResourceSet::EFlags_None) const;
	CDeviceResourceLayoutPtr  CreateResourceLayout(const SDeviceResourceLayoutDesc& resourceLayoutDesc);
	const CDeviceInputStream* CreateVertexStreamSet(uint32 numStreams, const SStreamInfo* streams);
	const CDeviceInputStream* CreateIndexStreamSet(const SStreamInfo* stream);

	void                      TrimResourceLayouts();

#if CRY_RENDERER_VULKAN
	SDeviceResourceLayoutDesc LookupResourceLayoutDesc(uint64 layoutHash);
	const std::vector<uint8>* LookupResourceLayoutEncoding(uint64 layoutHash);
	void RegisterEncodedResourceLayout(uint64 layoutHash, std::vector<uint8>&& encodedLayout);
	void UnRegisterEncodedResourceLayout(uint64 layoutHash);
	uint32 GetEncodedResourceLayoutSize(const std::vector<uint8>& encodedLayout);
	VkDescriptorSetLayout     GetInlineConstantBufferLayout();
	VkDescriptorSetLayout     GetInlineShaderResourceLayout();
#endif

	////////////////////////////////////////////////////////////////////////////
	// Renderpass API
	CDeviceRenderPassPtr         GetOrCreateRenderPass(const CDeviceRenderPassDesc& passDesc);
	CDeviceRenderPassPtr         GetRenderPass(const CDeviceRenderPassDesc& passDesc);
	const CDeviceRenderPassDesc* GetRenderPassDesc(const CDeviceRenderPass_Base* pPass);
	void                         EraseRenderPass(CDeviceRenderPass* pPass, bool bRemoveInvalidateCallbacks=true);
	void                         TrimRenderPasses();
	////////////////////////////////////////////////////////////////////////////

	// Low-level resource management API (TODO: remove D3D-dependency by abstraction)
	enum EResourceAllocationFlags : uint32
	{
		// for Create*Texture() only
		BIND_DEPTH_STENCIL               = BIT(0),  // Bind flag
		BIND_RENDER_TARGET               = BIT(1),  // Bind flag
		BIND_UNORDERED_ACCESS            = BIT(2),  // Bind flag

		BIND_VERTEX_BUFFER               = BIT(3),  // Bind flag, DX11+Vk
		BIND_INDEX_BUFFER                = BIT(4),  // Bind flag, DX11+Vk
		BIND_CONSTANT_BUFFER             = BIT(5),  // Bind flag, DX11+Vk
							             
		BIND_SHADER_RESOURCE             = BIT(6),  // Bind flag, any shader stage
		BIND_STREAM_OUTPUT               = BIT(7),

		// Bits [8, 15] free

		USAGE_UAV_READWRITE              = BIT(16), // Reading from UAVs is only possible through typeless formats under DX11
		USAGE_UAV_OVERLAP                = BIT(17), // Concurrent access to UAVs should be allowed
		USAGE_UAV_COUNTER                = BIT(18), // Allocate a counter resource for the UAV as well (size = ?)
		USAGE_AUTOGENMIPS                = BIT(19), // Generate mip-maps automatically whenever the contents of the resource change
		USAGE_STREAMING                  = BIT(20), // Use placed resources and allow changing LOD clamps for streamable textures
		USAGE_STAGE_ACCESS               = BIT(21), // Use persistent buffer resources to stage uploads/downloads to the texture
		USAGE_STRUCTURED                 = BIT(22), // Resource contains structured data instead of fundamental datatypes
		USAGE_INDIRECTARGS               = BIT(23), // Resource can be used for indirect draw/dispatch argument buffers
		USAGE_RAW                        = BIT(24), // Resource can be bound byte-addressable
		USAGE_LODABLE                    = BIT(25), // Resource consists of multiple LODs which can be selected at GPU run-time

		// for UMA or persistent MA only
		USAGE_DIRECT_ACCESS              = BIT(26),
		USAGE_DIRECT_ACCESS_CPU_COHERENT = BIT(27),
		USAGE_DIRECT_ACCESS_GPU_COHERENT = BIT(28),

		// The CPU access flags determine the heap on which the resource will be placed.
		// For porting old heap flags, you should use the following table to select the heap.
		// Note for CDevTexture: When USAGE_STAGE_ACCESS is set, the heap is always DEFAULT, and additional dedicated staging resources are created alongside.

		// CPU_READ | CPU_WRITE -> D3D HEAP
		//    no    |    no     -> DEFAULT
		//    no    |    yes    -> DYNAMIC
		//    yes   | yes or no -> STAGING
		USAGE_CPU_READ                   = BIT(29),
		USAGE_CPU_WRITE                  = BIT(30),

		USAGE_HIFREQ_HEAP                = BIT(31)  // Resource is reallocated every frame or multiple times each frame, use a recycling heap with delayed deletes
	};

	// Resource Usage	| Default	| Dynamic	| Immutable	| Staging
	// -----------------+-----------+-----------+-----------+--------
	// GPU - Read       | yes       | yes1      | yes       | yes1, 2
	// GPU - Write      | yes1      |           |           | yes1, 2
	// CPU - Read       |           |           |           | yes1, 2
	// CPU - Write      |           | yes       |           | yes1, 2
	//
	// 1 - This is restricted to ID3D11DeviceContext::CopySubresourceRegion, ID3D11DeviceContext::CopyResource,
	//     ID3D11DeviceContext::UpdateSubresource, and ID3D11DeviceContext::CopyStructureCount.
	// 2 - Cannot be a depth - stencil buffer or a multi-sampled render target.

	void           AllocateNullResources();
	void           ReleaseNullResources();
	D3DResource*   GetNullResource(D3D11_RESOURCE_DIMENSION eType);

#if defined(DEVRES_USE_STAGING_POOL)
	D3DResource*   AllocateStagingResource(D3DResource* pForTex, bool bUpload, void*& pMappedAddress); // address is only set if the staging resource is persistently mapped
	void           ReleaseStagingResource(D3DResource* pStagingTex);
#endif

	void           ReleaseResource(D3DResource* pResource);
	void           RecycleResource(D3DResource* pResource);

#define SKIP_ESRAM	-1
	HRESULT        Create2DTexture(uint32 nWidth, uint32 nHeight, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI = nullptr, int32 nESRAMOffset = SKIP_ESRAM);
	HRESULT        CreateCubeTexture(uint32 nSize, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI = nullptr);
	HRESULT        CreateVolumeTexture(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI = nullptr);
	HRESULT        CreateBuffer(buffer_size_t nSize, buffer_size_t elemSize, uint32 nUsage, uint32 nBindFlags, D3DBuffer** ppBuff, const void* pData = nullptr);

	static HRESULT InvalidateGpuCache(D3DBuffer* buffer, void* base_ptr, buffer_size_t size, buffer_size_t offset);
	static HRESULT InvalidateCpuCache(void* base_ptr, size_t size, size_t offset);
	void           InvalidateBuffer(D3DBuffer* buffer, void* base_ptr, buffer_size_t offset, buffer_size_t size, uint32 id);

	// Dataset content update(s) (TODO: move into CCopyCommandList)
	static uint8* Map(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode);
	static void Unmap(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode);

	static inline void ExtractBasePointer(D3DBuffer* buffer, D3D11_MAP mode, uint8*& base_ptr);
	static inline void ReleaseBasePointer(D3DBuffer* buffer);

	static inline uint8 MarkReadRange(D3DBuffer* buffer, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode);
	static inline uint8 MarkWriteRange(D3DBuffer* buffer, buffer_size_t offset, buffer_size_t size, uint8 marker);

	// NOTE: Standard behaviour in the presence of multiple GPUs is to make the same data available to all
	// GPUs. If data should diverge per GPU, it can be uploaded by concatenating multiple divergent data-blocks
	// and passing the appropriate "numDataBlocks". Each GPU will then receive it's own version of the data.
	template<const bool bDirectAccess = false>
	static void UploadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU = nullptr, UINT numDataBlocks = 1);
	template<const bool bDirectAccess = false>
	static void DownloadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU = nullptr, UINT numDataBlocks = 1);

	////////////////////////////////////////////////////////////////////////////
	// CommandList API

	enum EQueueType
	{
		eQueue_Graphics = 0,
		eQueue_Compute = 1,
		eQueue_Copy = 2,
		eQueue_Bundle = 3,
	};

	// Get a reference to the core graphics command-list, which runs on the command-queue assigned to Present().
	// Only one thread at a time is allowed to call functions on this command-list (DX12 restriction).
	// Do not cache the address of this command-list; it may change between frames and AcquireCommandList() calls.
	CDeviceCommandListRef GetCoreCommandList() const;

#if (CRY_RENDERER_DIRECT3D >= 120)
	// Helper functions for DX12 MultiGPU
	ID3D12CommandQueue* GetNativeCoreCommandQueue() const;
#endif

	// Acquire one or more command-lists which are independent of the core command-list
	// Only one thread is allowed to call functions on this command-list (DX12 restriction).
	// The thread that gets the permition is the one calling Begin() on it AFAICS
	CDeviceCommandListUPtr AcquireCommandList(EQueueType eQueueType = eQueue_Graphics);
	std::vector<CDeviceCommandListUPtr> AcquireCommandLists(uint32 listCount, EQueueType eQueueType = eQueue_Graphics);

	// Command-list sinks, will automatically submit command-lists in [global] allocation-order
	void ForfeitCommandList(CDeviceCommandListUPtr pCommandList, EQueueType eQueueType = eQueue_Graphics);
	void ForfeitCommandLists(std::vector<CDeviceCommandListUPtr> pCommandLists, EQueueType eQueueType = eQueue_Graphics);

#if (CRY_RENDERER_DIRECT3D >= 120)
	NCryDX12::CDevice*             GetDX12Device() const { return m_pDX12Device; }
	NCryDX12::CCommandScheduler*   GetDX12Scheduler() const { return m_pDX12Scheduler; }
#elif (CRY_RENDERER_DIRECT3D >= 110)
	NCryDX11::CDevice*             GetDX11Device() const { return m_pDX11Device; }
	NCryDX11::CCommandScheduler*   GetDX11Scheduler() const { return m_pDX11Scheduler; }
#elif (CRY_RENDERER_VULKAN >= 10)
	NCryVulkan::CDevice*           GetVKDevice   () const { return m_pVKDevice; }
	NCryVulkan::CCommandScheduler* GetVKScheduler() const { return m_pVKScheduler; }

	void                           UploadInitialImageData(SSubresourcePayload pSrcMips[], NCryVulkan::CImageResource* pDst, const VkImageCreateInfo& info);
	void                           UpdateDeferredUploads();
	static void                    SelectStagingLayout(const NCryVulkan::CImageResource* pImage, uint32 subResource, SResourceMemoryMapping& result);
#elif CRY_RENDERER_GNM
	sce::Gnm::OwnerHandle          GetResourceOwnerHandle() { return m_resourceOwnerHandle; }
#endif

	static bool CanUseCoreCommandList();

private:
	static bool OnRenderPassInvalidated(void* pRenderPass, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags);

	void ReleaseResources();
	void ReleaseResourcesImpl();

	void TrimResources();

#if (CRY_RENDERER_DIRECT3D >= 120)
	NCryDX12::CDevice*               m_pDX12Device;
	NCryDX12::CCommandScheduler*     m_pDX12Scheduler;
#elif (CRY_RENDERER_DIRECT3D >= 110)
	NCryDX11::CDevice*               m_pDX11Device;
	NCryDX11::CCommandScheduler*     m_pDX11Scheduler;
#elif (CRY_RENDERER_VULKAN >= 10)
	NCryVulkan::CDevice*             m_pVKDevice;
	NCryVulkan::CCommandScheduler*   m_pVKScheduler;
	VkDescriptorSetLayout            m_inlineConstantBufferLayout;
	VkDescriptorSetLayout            m_inlineShaderResourceLayout;
	std::map<uint64, std::vector<uint8>> m_encodedResourceLayouts;

	struct SDeferredUploadData
	{
		_smart_ptr<NCryVulkan::CBufferResource> pStagingBuffer;
		_smart_ptr<NCryVulkan::CMemoryResource> pTarget;
		SResourceMemoryMapping                  targetMapping;
		bool                                    bExtendedAdressing;
	};

	std::vector<SDeferredUploadData> m_deferredUploads;
	CryCriticalSectionNonRecursive   m_deferredUploadCS;
#elif CRY_RENDERER_GNM 
	sce::Gnm::OwnerHandle            m_resourceOwnerHandle;
#endif

	////////////////////////////////////////////////////////////////////////////
	// SamplerState API

	// A heap containing all permutations of SamplerState, they are global and are never evicted
	static CDeviceSamplerState* CreateSamplerState(const SSamplerState& pState);
	static CStaticDeviceObjectStorage<SamplerStateHandle, SSamplerState, CDeviceSamplerState, false, CreateSamplerState> s_SamplerStates;

	////////////////////////////////////////////////////////////////////////////
	// InputLayout API

	// A heap containing all permutations of InputLayout, they are global and are never evicted
	static CDeviceInputLayout* CreateInputLayout(const SInputLayout& pState, const SShaderBlob* m_pConsumingVertexShader);
	static std::vector<SInputLayout> m_vertexFormatToInputLayoutCache;

	// Higher level input-layout composition / / / / / / / / / / / / / / / / / /
	static std::unordered_map<SInputLayoutCompositionDescriptor, SInputLayoutPair, SInputLayoutCompositionDescriptor::hasher> s_InputLayoutCompositions;

	////////////////////////////////////////////////////////////////////////////
	// PipelineState API
	static const int UnusedPsoKeepAliveFrames = MAX_FRAMES_IN_FLIGHT;

	CDeviceGraphicsPSOPtr    CreateGraphicsPSOImpl(const CDeviceGraphicsPSODesc& psoDesc) const;
	CDeviceComputePSOPtr     CreateComputePSOImpl(const CDeviceComputePSODesc& psoDesc) const;

	CDeviceCommandListUPtr m_pCoreCommandList;

	std::unordered_map<CDeviceGraphicsPSODesc, CDeviceGraphicsPSOPtr>  m_GraphicsPsoCache;
	std::unordered_map<CDeviceGraphicsPSODesc, CDeviceGraphicsPSOWPtr> m_InvalidGraphicsPsos;

	std::unordered_map<CDeviceComputePSODesc, CDeviceComputePSOPtr>    m_ComputePsoCache;
	std::unordered_map<CDeviceComputePSODesc, CDeviceComputePSOWPtr>   m_InvalidComputePsos;

	////////////////////////////////////////////////////////////////////////////
	// Input dataset(s) API

	CDeviceResourceLayoutPtr CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const;

	VectorMap<SDeviceResourceLayoutDesc, CDeviceResourceLayoutPtr>     m_ResourceLayoutCache;

	struct SDeviceStreamInfoHash
	{
		template<size_t U>
		size_t operator()(const std::array<CDeviceInputStream, U>& x) const
		{
			// Note: Relies on CDeviceInputStream being tightly packed
			return size_t(XXH64(x.data(), sizeof(CDeviceInputStream) * U, 61));
		}
	};

	struct SDeviceStreamInfoEquality
	{
		template<size_t U>
		bool operator()(const std::array<CDeviceInputStream, U>& x, const std::array<CDeviceInputStream, U>& y) const
		{
			// Note: Relies on CDeviceInputStream being tightly packed
			return !memcmp(x.data(), y.data(), sizeof(CDeviceInputStream) * U);
		}
	};

	typedef std::array<CDeviceInputStream, 1>       TIndexStreams;
	typedef std::array<CDeviceInputStream, VSF_NUM> TVertexStreams;
	std::unordered_set<TIndexStreams, SDeviceStreamInfoHash, SDeviceStreamInfoEquality>  m_uniqueIndexStreams;
	std::unordered_set<TVertexStreams, SDeviceStreamInfoHash, SDeviceStreamInfoEquality> m_uniqueVertexStreams;

	D3DResource* m_NullResources[D3D11_RESOURCE_DIMENSION_TEXTURE3D + 1];

#if (CRY_RENDERER_VULKAN >= 10)
public:
	D3DShaderResource* GetVkNullBufferView(bool bStructured) const
	{
		auto* pNullBufferView = bStructured ? m_NullBufferViewStructured : m_NullBufferViewTyped;
		
		pNullBufferView->AddRef();
		return pNullBufferView;
	}

private:
	D3DShaderResource* m_NullBufferViewTyped;
	D3DShaderResource* m_NullBufferViewStructured;
#endif

#if defined(DEVRES_USE_STAGING_POOL) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	struct StagingTextureDef
	{
		D3D11_TEXTURE2D_DESC desc;
		uint32 lastUsedFrameID;
		D3DTexture*          pStagingResource;

		friend bool operator==(const StagingTextureDef& a, const D3D11_TEXTURE2D_DESC& b)
		{
			return memcmp(&a.desc, &b, sizeof(b)) == 0;
		}
	};

	typedef std::vector<StagingTextureDef, stl::STLGlobalAllocator<StagingTextureDef>> StagingPoolVec;

	StagingPoolVec m_stagingPool;
#endif

#if defined(BUFFER_ENABLE_DIRECT_ACCESS) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && !CRY_RENDERER_GNM
	friend class CSubmissionQueue_DX11;

	// The buffer invalidations
	struct SBufferInvalidation
	{
		D3DBuffer* buffer;
		void*      base_ptr;
		size_t     offset;
		size_t     size;
		bool operator<(const SBufferInvalidation& other) const
		{
			if (buffer == other.buffer)
			{
				return offset < other.offset;
			}
			return buffer < other.buffer;
		}
		bool operator!=(const SBufferInvalidation& other) const
		{
			return buffer != other.buffer
#if CRY_PLATFORM_DURANGO // Should be removed when we have range based invalidations
				&& offset != other.offset
#endif
				;
		}
	};

	typedef std::vector<SBufferInvalidation> BufferInvalidationsT;
	BufferInvalidationsT m_buffer_invalidations[2];
#endif

#if !CRY_RENDERER_GNM
	HRESULT        Create2DTexture(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI = nullptr);
	HRESULT        CreateCubeTexture(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI = nullptr);
	HRESULT        CreateVolumeTexture(const D3D11_TEXTURE3D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI = nullptr);
#endif

	////////////////////////////////////////////////////////////////////////////
	// Renderpass API
	std::unordered_map<CDeviceRenderPassDesc, CDeviceRenderPassPtr, CDeviceRenderPassDesc::SHash, CDeviceRenderPassDesc::SEqual>  m_RenderPassCache; 
	CryCriticalSectionNonRecursive m_RenderPassCacheLock;

public:
	////////////////////////////////////////////////////////////////////////////
	// Shader blob API
	// We still need these types to support current shader management system (which caches these return pointers).
	// TODO: Should this be rolled into PSO creation?
	ID3D11VertexShader*   CreateVertexShader(const void* pData, size_t bytes);
	ID3D11PixelShader*    CreatePixelShader(const void* pData, size_t bytes);
	ID3D11GeometryShader* CreateGeometryShader(const void* pData, size_t bytes);
	ID3D11HullShader*     CreateHullShader(const void* pData, size_t bytes);
	ID3D11DomainShader*   CreateDomainShader(const void* pData, size_t bytes);
	ID3D11ComputeShader*  CreateComputeShader(const void* pData, size_t bytes);

	// Occlusion Query API
	// Note: Begin()/End() function is on the graphics-command-list
	D3DOcclusionQuery* CreateOcclusionQuery();
	bool               GetOcclusionQueryResults(D3DOcclusionQuery* pQuery, uint64& samplesPassed);

public:
#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	IDefragAllocatorStats GetTexturePoolStats();

	// Note: temporary solution, this should be removed as soon as the device
	// layer for Durango is available
	static void* GetBackingStorage(D3DBuffer* buffer);
	static void  FreeBackingStorage(void* base_ptr);

	struct STileRequest
	{
		STileRequest()
			: pLinSurfaceSrc(NULL)
			, nDstSubResource(0)
			, bSrcInGPUMemory(false)
		{
		}

		const void* pLinSurfaceSrc;
		int         nDstSubResource;
		bool        bSrcInGPUMemory; // Operation has to be conducted in-place
	};

	HRESULT BeginTileFromLinear2D(CDeviceTexture* pDst, const STileRequest* pSubresources, size_t nSubresources, UINT64& fenceOut);

//private:
	typedef std::map<SMinimisedTexture2DDesc, SDeviceTextureDesc, std::less<SMinimisedTexture2DDesc>, stl::STLGlobalAllocator<std::pair<SMinimisedTexture2DDesc, SDeviceTextureDesc>>> TLayoutTableMap;

	static bool               InPlaceConstructable(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags);
	HRESULT                   CreateInPlaceTexture2D(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const STexturePayload* pTI, CDeviceTexture*& pDevTexOut);
	const SDeviceTextureDesc* Find2DResourceLayout(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, ETEX_TileMode tileMode);

	CDurangoGPUMemoryManager       m_texturePool;
	CDurangoGPURingMemAllocator    m_textureStagingRing;
	CryCriticalSectionNonRecursive m_layoutTableLock;
	TLayoutTableMap                m_layoutTable;

	CryCriticalSection       m_dma1Lock;
	ID3D11DmaEngineContextX* m_pDMA1;
#endif
};

static ILINE CDeviceObjectFactory& GetDeviceObjectFactory() { return CDeviceObjectFactory::GetInstance(); }

//DEFINE_ENUM_FLAG_OPERATORS(CDeviceObjectFactory::EResourceAllocationFlags);

////////////////////////////////////////////////////////////////////////////
#include "DeviceObjects.inl"

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	#include "D3D11/DeviceObjects_D3D11.inl"
	#if CRY_PLATFORM_DURANGO
		#include "D3D11/DeviceObjects_D3D11_Durango.inl"
	#endif
#endif
#if (CRY_RENDERER_DIRECT3D >= 120)
	#include "D3D12/DeviceObjects_D3D12.inl"
	#if CRY_PLATFORM_DURANGO
		#include "D3D12/DeviceObjects_D3D12_Durango.inl"
	#endif
#endif
#if CRY_PLATFORM_ORBIS
	#include "GNM/DeviceObjects_GNM.inl"
#endif
#if (CRY_RENDERER_VULKAN >= 10)
	#include "Vulkan/DeviceObjects_Vulkan.inl"
#endif

////////////////////////////////////////////////////////////////////////////
struct SScopedComputeCommandList
{
	CDeviceCommandListUPtr m_pCommandList;
	CDeviceComputeCommandInterface* m_pComputeInterface;
	bool m_bAsynchronousCommandList;

	SScopedComputeCommandList(bool bAsynchronousCommandList)
	{
		CDeviceObjectFactory& pFactory = GetDeviceObjectFactory();

		if ((m_bAsynchronousCommandList = bAsynchronousCommandList))
		{
			m_pCommandList = pFactory.AcquireCommandList(CDeviceObjectFactory::eQueue_Compute);
			m_pCommandList->LockToThread();
			m_pComputeInterface = m_pCommandList->GetComputeInterface();
		}
		else
		{
			CDeviceCommandListRef commandList = pFactory.GetCoreCommandList();
			m_pComputeInterface = commandList.GetComputeInterface();
		}
	}

	~SScopedComputeCommandList()
	{
		CDeviceObjectFactory& pFactory = GetDeviceObjectFactory();

		if (m_bAsynchronousCommandList)
		{
			m_pCommandList->Close();
			pFactory.ForfeitCommandList(std::move(m_pCommandList), CDeviceObjectFactory::eQueue_Compute);
		}
	}

	operator CDeviceComputeCommandInterface*() const
	{
		return m_pComputeInterface;
	}

	operator CDeviceCommandListRef() const
	{
		if (m_bAsynchronousCommandList)
			return *m_pCommandList.get();
		else
			return GetDeviceObjectFactory().GetCoreCommandList();
	}
};
