// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/VertexFormats.h>
#include <CryRenderer/ITexture.h>
#include <array>
#include <bitset>

class CHWShader_D3D;
class CShader;
class CTexture;
class CCryNameTSCRC;
class CCryDeviceWrapper;
class CConstantBuffer;
class CShaderResources;
struct SGraphicsPipelineStateDescription;
struct SComputePipelineStateDescription;

/////////////////////////////////////////////////////////////////////////////////
//// include concrete device/context implementations
#if defined(CRY_USE_DX12)
	#define CRY_USE_DX12_NATIVE
	#include "XRenderD3D9/DX12/API/DX12CommandList.hpp"
#endif

enum EConstantBufferShaderSlot
{
	eConstantBufferShaderSlot_PerBatch          = 0,
	eConstantBufferShaderSlot_PerInstanceLegacy = 1,
	eConstantBufferShaderSlot_PerFrame          = 2,
	eConstantBufferShaderSlot_PerMaterial       = 3,
	eConstantBufferShaderSlot_PerLight          = 4,
	eConstantBufferShaderSlot_PerPass           = 5,
	eConstantBufferShaderSlot_SkinData          = 6,
	eConstantBufferShaderSlot_InstanceData      = 7,
	eConstantBufferShaderSlot_SPI               = 8,
	eConstantBufferShaderSlot_SkinQuat          = 9,
	eConstantBufferShaderSlot_SkinQuatPrev      = 10,
	eConstantBufferShaderSlot_SPIIndex          = 11,
	eConstantBufferShaderSlot_PerInstance       = 12,
	eConstantBufferShaderSlot_PerView           = 13,

	eConstantBufferShaderSlot_Count
};

enum EResourceLayoutSlot
{
	EResourceLayoutSlot_PerInstanceCB      = 0,
	EResourceLayoutSlot_PerMaterialRS      = 1,
	EResourceLayoutSlot_PerInstanceExtraRS = 2,
	EResourceLayoutSlot_PerPassRS          = 3,

	EResourceLayoutSlot_Max                = 7
};

enum EReservedTextureSlot
{
	EReservedTextureSlot_SkinExtraWeights = 14,
	EReservedTextureSlot_AdjacencyInfo    = 15,
	EReservedTextureSlot_PatchID          = 15,
	EReservedTextureSlot_ComputeSkinVerts = 16,
	EReservedTextureSlot_TerrainBaseMap   = 29,
};

enum EShaderStage
{
	EShaderStage_Vertex            = BIT(eHWSC_Vertex),
	EShaderStage_Pixel             = BIT(eHWSC_Pixel),
	EShaderStage_Geometry          = BIT(eHWSC_Geometry),
	EShaderStage_Compute           = BIT(eHWSC_Compute),
	EShaderStage_Domain            = BIT(eHWSC_Domain),
	EShaderStage_Hull              = BIT(eHWSC_Hull),

	EShaderStage_Count             = eHWSC_Num,
	EShaderStage_None              = 0,
	EShaderStage_All               = EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Geometry | EShaderStage_Domain | EShaderStage_Hull | EShaderStage_Compute,
	EShaderStage_AllWithoutCompute = EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Geometry | EShaderStage_Domain | EShaderStage_Hull
};
DEFINE_ENUM_FLAG_OPERATORS(EShaderStage)
#define SHADERSTAGE_FROM_SHADERCLASS(SHADERCLASS) ::EShaderStage(BIT(SHADERCLASS))

enum { InlineConstantsShaderSlot = eConstantBufferShaderSlot_PerInstance };
const int ResourceSetBufferCount = 3;

typedef int                                      ShaderSlot;
typedef std::bitset<EResourceLayoutSlot_Max + 1> UsedBindSlotSet;

////////////////////////////////////////////////////////////////////////////

struct CDeviceInputStream
{
	friend class CDeviceObjectFactory;
	friend class CDeviceGraphicsCommandList;

	/*explicit*/ operator SStreamInfo() const
	{
		SStreamInfo stream;
		stream.hStream = hStream;
		stream.nStride = nStride;
		return stream;
	}

	bool operator!() const
	{
		return hStream == ~0u;
	}

	const CDeviceInputStream& operator[](uint32 offset) const
	{
		return *(this + offset);
	}

private:
	buffer_handle_t hStream;
	uint32          nStride;

	CDeviceInputStream() { hStream = ~0u; nStride = 0; }
	CDeviceInputStream(const SStreamInfo& stream) { *this = stream; }
	CDeviceInputStream(buffer_handle_t stream, int stride) : hStream(stream), nStride(stride) {}

	CDeviceInputStream& operator=(const SStreamInfo& stream)
	{
		hStream = stream.hStream;
		nStride = stream.nStride;
		return *this;
	}

	inline bool operator==(const CDeviceInputStream& other) const
	{
		return hStream == other.hStream && nStride == other.nStride;
	}
};

////////////////////////////////////////////////////////////////////////////

class CDeviceResourceSet
{
	friend class CDeviceObjectFactory;
	friend struct SDeviceResourceLayoutDesc;
	friend class CDeviceResourceLayout;
	friend class CDeviceResourceLayout_DX12;

public:
	enum EFlags
	{
		EFlags_None              = 0,
		EFlags_ForceSetAllState  = BIT(0),
		EFlags_DynamicUpdates    = BIT(1),
		EFlags_PendingAllocation = BIT(2),
	};

	CDeviceResourceSet(EFlags flags);
	CDeviceResourceSet(const CDeviceResourceSet& other);
	virtual ~CDeviceResourceSet();

	bool         IsValid() const { return m_bValid; }
	bool         IsDirty() const { return m_bDirty; }
	void         SetDirty(bool bDirty);

	void         Clear(bool bTextures = true);

	void         SetTexture(ShaderSlot shaderSlot, _smart_ptr<CTexture> pTexture, SResourceView::KeyType resourceViewID = SResourceView::DefaultView, EShaderStage shaderStages = EShaderStage_Pixel);
	void         SetSampler(ShaderSlot shaderSlot, int sampler, EShaderStage shaderStages = EShaderStage_Pixel);
	void         SetConstantBuffer(ShaderSlot shaderSlot, _smart_ptr<CConstantBuffer> pBuffer, EShaderStage shaderStages = EShaderStage_Pixel);
	void         SetBuffer(ShaderSlot shaderSlot, const CGpuBuffer& buffer, EShaderStage shaderStages = EShaderStage_Pixel);

	EShaderStage GetShaderStages() const;
	EFlags       GetFlags() const { return m_Flags; }

	bool         Fill(CShader* pShader, CShaderResources* pResources, EShaderStage shaderStages = EShaderStage_Pixel);

	virtual void Build() = 0;

protected:
	template<typename T>
	struct SResourceData
	{
		SResourceData() : resource(), shaderStages(EShaderStage_None) {}
		SResourceData(const T& _resource, ::EShaderStage _shaderStages) : resource(_resource), shaderStages(_shaderStages) {}
		SResourceData(T&& _resource, ::EShaderStage _shaderStages) : resource(std::move(_resource)), shaderStages(_shaderStages) {}

		bool operator==(const SResourceData<T>& other) const
		{
			return resource == other.resource && shaderStages == other.shaderStages;
		}

		void swap(SResourceData& other)
		{
			std::swap(resource, other.resource);
			std::swap(shaderStages, other.shaderStages);
		}

		T              resource;
		::EShaderStage shaderStages;
	};

	typedef SResourceData<std::tuple<SResourceView::KeyType, _smart_ptr<CTexture>>> STextureData;
	typedef SResourceData<int>                                                      SSamplerData;
	typedef SResourceData<_smart_ptr<CConstantBuffer>>                              SConstantBufferData;
	typedef SResourceData<CGpuBuffer>                                               SBufferData;

	void OnTextureChanged(uint32 dirtyFlags);

	VectorMap<ShaderSlot, STextureData>        m_Textures;
	VectorMap<ShaderSlot, SSamplerData>        m_Samplers;
	VectorMap<ShaderSlot, SBufferData>         m_Buffers;
	VectorMap<ShaderSlot, SConstantBufferData> m_ConstantBuffers;

	bool   m_bValid : 1;
	bool   m_bDirty : 1;
	EFlags m_Flags  : 8;
};

typedef std::shared_ptr<CDeviceResourceSet> CDeviceResourceSetPtr;

////////////////////////////////////////////////////////////////////////////

struct SDeviceResourceLayoutDesc
{
	SDeviceResourceLayoutDesc() : m_InlineConstantCount(0) {}

	void            SetInlineConstants(uint32 numConstants);
	void            SetResourceSet(uint32 bindSlot, CDeviceResourceSetPtr pResourceSet);
	void            SetConstantBuffer(uint32 bindSlot, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages);

	bool            IsValid() const;
	UsedBindSlotSet GetRequiredResourceBindings() const;

	struct SInlineConstantBuffer
	{
		EConstantBufferShaderSlot shaderSlot;
		EShaderStage              shaderStages;
	};

	uint32 m_InlineConstantCount;
	VectorMap<uint32, SInlineConstantBuffer> m_ConstantBuffers;
	VectorMap<uint32, CDeviceResourceSetPtr> m_ResourceSets;

	bool operator<(const SDeviceResourceLayoutDesc& other) const;
};

class CDeviceResourceLayout
{
public:
	CDeviceResourceLayout(UsedBindSlotSet requiredBindings)
		: m_requiredResourceBindings(requiredBindings)
	{}

	UsedBindSlotSet GetRequiredResourceBindings() const { return m_requiredResourceBindings; }

protected:
	UsedBindSlotSet m_requiredResourceBindings;
};

typedef std::shared_ptr<const CDeviceResourceLayout> CDeviceResourceLayoutConstPtr;
typedef std::weak_ptr<const CDeviceResourceLayout>   CDeviceResourceLayoutConstWPtr;

typedef std::shared_ptr<CDeviceResourceLayout>       CDeviceResourceLayoutPtr;
typedef std::weak_ptr<CDeviceResourceLayout>         CDeviceResourceLayoutWPtr;

////////////////////////////////////////////////////////////////////////////
class CDeviceGraphicsPSODesc
{
public:
	CDeviceGraphicsPSODesc(const CDeviceGraphicsPSODesc& other);
	CDeviceGraphicsPSODesc(CDeviceResourceLayout* pResourceLayout, const SGraphicsPipelineStateDescription& pipelineDesc);
	CDeviceGraphicsPSODesc(uint32 renderTargetCount, CTexture* const* pRenderTargets, const SDepthTexture* pDepthTarget, SResourceView::KeyType* pRenderTargetViews = nullptr);

	CDeviceGraphicsPSODesc& operator=(const CDeviceGraphicsPSODesc& other);
	bool                    operator==(const CDeviceGraphicsPSODesc& other) const;

	uint64                  GetHash() const;

public:
	void  InitWithDefaults();

	void  FillDescs(D3D11_RASTERIZER_DESC& rasterizerDesc, D3D11_BLEND_DESC& blendDesc, D3D11_DEPTH_STENCIL_DESC& depthStencilDesc) const;
	uint8 CombineVertexStreamMasks(uint8 fromShader, uint8 fromObject) const;

public:
	_smart_ptr<CShader>        m_pShader;
	CCryNameTSCRC              m_technique;
	bool                       m_bAllowTesselation;
	uint64                     m_ShaderFlags_RT;
	uint32                     m_ShaderFlags_MD;
	uint32                     m_ShaderFlags_MDV;
	EShaderQuality             m_ShaderQuality;
	uint32                     m_RenderState;
	uint32                     m_StencilState;
	uint8                      m_StencilReadMask;
	uint8                      m_StencilWriteMask;
	EVertexFormat              m_VertexFormat;
	uint8                      m_ObjectStreamMask;
	std::array<ETEX_Format, 4> m_RenderTargetFormats;
	ETEX_Format                m_DepthStencilFormat;
	ECull                      m_CullMode;
	int32                      m_DepthBias;
	f32                        m_DepthBiasClamp;
	f32                        m_SlopeScaledDepthBias;
	ERenderPrimitiveType       m_PrimitiveType;
	CDeviceResourceLayout*     m_pResourceLayout;
};

class CDeviceComputePSODesc
{
public:
	CDeviceComputePSODesc(const CDeviceComputePSODesc& other);
	CDeviceComputePSODesc(CDeviceResourceLayout* pResourceLayout, const SComputePipelineStateDescription& pipelineDesc);
	CDeviceComputePSODesc(CDeviceResourceLayout* pResourceLayout, CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags);

	CDeviceComputePSODesc& operator=(const CDeviceComputePSODesc& other);
	bool                   operator==(const CDeviceComputePSODesc& other) const;

	uint64                 GetHash() const;

public:
	void InitWithDefaults();

public:
	_smart_ptr<CShader>    m_pShader;
	CCryNameTSCRC          m_technique;
	uint64                 m_ShaderFlags_RT;
	uint32                 m_ShaderFlags_MD;
	uint32                 m_ShaderFlags_MDV;
	CDeviceResourceLayout* m_pResourceLayout;
};

namespace std
{
template<>
struct hash<CDeviceGraphicsPSODesc>
{
	uint64 operator()(const CDeviceGraphicsPSODesc& psoDesc) const
	{
		return psoDesc.GetHash();
	}
};

template<>
struct equal_to<CDeviceGraphicsPSODesc>
{
	bool operator()(const CDeviceGraphicsPSODesc& psoDesc1, const CDeviceGraphicsPSODesc& psoDesc2) const
	{
		return psoDesc1 == psoDesc2;
	}
};

template<>
struct hash<CDeviceComputePSODesc>
{
	uint64 operator()(const CDeviceComputePSODesc& psoDesc) const
	{
		return psoDesc.GetHash();
	}
};

template<>
struct equal_to<CDeviceComputePSODesc>
{
	bool operator()(const CDeviceComputePSODesc& psoDesc1, const CDeviceComputePSODesc& psoDesc2) const
	{
		return psoDesc1 == psoDesc2;
	}
};

template<>
struct less<SDeviceResourceLayoutDesc>
{
	bool operator()(const SDeviceResourceLayoutDesc& layoutDesc1, const SDeviceResourceLayoutDesc& layoutDesc2) const
	{
		return layoutDesc1 < layoutDesc2;
	}
};
}

class CDeviceGraphicsPSO
{
public:
	CDeviceGraphicsPSO()
		: m_bValid(false)
		, m_nUpdateCount(0)
	{}

	virtual bool Init(const CDeviceGraphicsPSODesc& psoDesc) = 0;
	bool         IsValid() const        { return m_bValid; }
	uint32       GetUpdateCount() const { return m_nUpdateCount;  }

	std::array<void*, eHWSC_Num>          m_pHwShaderInstances; // TODO: remove once we don't need shader reflection anymore
	std::array<CHWShader_D3D*, eHWSC_Num> m_pHwShaders;         // TODO: remove once we don't need shader reflection anymore

#if defined(ENABLE_PROFILING_CODE)
	ERenderPrimitiveType m_PrimitiveTypeForProfiling;
#endif

protected:
	bool   m_bValid;
	uint32 m_nUpdateCount;
};

class CDeviceComputePSO
{
public:
	CDeviceComputePSO()
		: m_bValid(false)
	{}

	virtual bool Init(const CDeviceComputePSODesc& psoDesc) = 0;
	bool         IsValid() const { return m_bValid; }

	std::array<void*, eHWSC_Num>          m_pHwShaderInstances; // TODO: remove once we don't need shader reflection anymore
	std::array<CHWShader_D3D*, eHWSC_Num> m_pHwShaders;         // TODO: remove once we don't need shader reflection anymore

#if defined(ENABLE_PROFILING_CODE)
	ERenderPrimitiveType m_PrimitiveTypeForProfiling;
#endif

protected:
	bool m_bValid;
};

typedef std::shared_ptr<const CDeviceGraphicsPSO> CDeviceGraphicsPSOConstPtr;
typedef std::weak_ptr<const CDeviceGraphicsPSO>   CDeviceGraphicsPSOConstWPtr;

typedef std::shared_ptr<const CDeviceComputePSO>  CDeviceComputePSOConstPtr;
typedef std::weak_ptr<const CDeviceComputePSO>    CDeviceComputePSOConstWPtr;

typedef std::shared_ptr<CDeviceGraphicsPSO>       CDeviceGraphicsPSOPtr;
typedef std::weak_ptr<CDeviceGraphicsPSO>         CDeviceGraphicsPSOWPtr;

typedef std::shared_ptr<CDeviceComputePSO>        CDeviceComputePSOPtr;
typedef std::weak_ptr<CDeviceComputePSO>          CDeviceComputePSOWPtr;

////////////////////////////////////////////////////////////////////////////

class CDeviceCommandList
{
	friend CDeviceObjectFactory;

public:
	template<typename T>
	struct SCachedValue
	{
		T cachedValue;
		SCachedValue() {}
		SCachedValue(const T& value) : cachedValue(value) {}

		template<typename U>
		inline bool Set(U newValue)
		{
			if (cachedValue == newValue)
				return false;

			cachedValue = newValue;
			return true;
		}

		template<typename U>
		inline bool operator!=(U otherValue) const
		{
			return !(cachedValue == otherValue);
		}
	};

public:
	CDeviceCommandList() {};

	void         SpecifyResourceUsage(/* resource, state */);  // set => begin+end
	void         AnnounceResourceUsage(/* resource, state */); // begin
	void         ApproveResourceUsage(/* resource, state */);  // end

	virtual void LockToThread() = 0;
	virtual void Build() = 0;
};

class CDeviceCopyCommandList : public CDeviceCommandList
{
	friend CDeviceObjectFactory;

public:
	enum ECopyType
	{
		eCT_GraphicsResources,                           // RenderTarget && DepthStencil && SwapChain -> Direct
		eCT_GenericResources,                            // ShaderResource && UnorderedAccess -> Compute
		eCT_OffCardResources                             // everything crossing PCIe -> XDMA
	};

	static ECopyType DetermineCopyType(ECopyType eCurrent, D3DResource* pResource);

public:
	CDeviceCopyCommandList(ECopyType eType = eCT_OffCardResources) {};

	void         CopySubresourceRegion(D3DResource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, D3DResource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags);
	void         CopyResource(D3DResource* pDstResource, ID3D11Resource* pSrcResource, UINT CopyFlags);
	void         UpdateSubresource(D3DResource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags);

	virtual void LockToThread() override;
	virtual void Build() override;
};

class CDeviceGraphicsCommandList : public CDeviceCopyCommandList
{
	friend CDeviceObjectFactory;

public:
	CDeviceGraphicsCommandList() : CDeviceCopyCommandList(eCT_GraphicsResources) {}

	void         PrepareRenderTargetsForUse(uint32 targetCount, CTexture** pTargets, SDepthTexture* pDepthTarget, SResourceView::KeyType* pRenderTargetViews = nullptr) const;
	void         PrepareResourcesForUse(uint32 bindSlot, CDeviceResourceSet* pResources) const;
	void         PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const;
	void         PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages) const;
	void         PrepareVertexBuffersForUse(uint32 streamCount, const CDeviceInputStream* vertexStreams) const;
	void         PrepareIndexBufferForUse(const CDeviceInputStream* indexStream) const;

	void         SetRenderTargets(uint32 targetCount, CTexture* const* pTargets, const SDepthTexture* pDepthTarget, SResourceView::KeyType* pRenderTargetViews = nullptr);
	void         SetViewports(uint32 vpCount, const D3DViewPort* pViewports);
	void         SetScissorRects(uint32 rcCount, const D3DRectangle* pRects);
	void         SetPipelineState(CDeviceGraphicsPSOConstPtr devicePSO);
	void         SetResourceLayout(CDeviceResourceLayoutConstPtr resourceLayout);
	void         SetResources(uint32 bindSlot, const CDeviceResourceSet* pResources);
	void         SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass);
	void         SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages);
	void         SetVertexBuffers(uint32 streamCount, const CDeviceInputStream* vertexStreams);
	void         SetIndexBuffer(const CDeviceInputStream* indexStream); // NOTE: Take care with PSO strip cut/restart value and 32/16 bit indices
	void         SetInlineConstants(uint32 bindSlot, uint32 constantCount, float* pConstants);
	void         SetStencilRef(uint8 stencilRefValue);

	void         Draw(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation);
	void         DrawIndexed(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation);

	void         ClearSurface(D3DSurface* pView, const FLOAT Color[4], UINT NumRects, const D3D11_RECT* pRect);
	void         ClearDepthSurface(D3DDepthSurface* pView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D11_RECT* pRect);

	virtual void LockToThread() final;
	virtual void Build() final;

	void         Reset();

protected:
	void PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const;
	void PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const;
	void PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages) const;

	void SetPipelineStateImpl(const CDeviceGraphicsPSOConstPtr& devicePSO);
	void SetResourceLayoutImpl(const CDeviceResourceLayoutConstPtr& resourceLayout);
	void SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources);
	void SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass);
	void SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages);
	void SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants);
	void SetStencilRefImpl(uint8 stencilRefValue);

	void DrawImpl(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation);
	void DrawIndexedImpl(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation);

	void ResetImpl();

	CDeviceGraphicsPSOConstPtr                           m_pCurrentPipelineState;
	CDeviceResourceLayoutConstPtr                        m_pCurrentResourceLayout;
	std::array<const void*, EResourceLayoutSlot_Max + 1> m_pCurrentResources;
	int32 m_CurrentStencilRef;

	SCachedValue<const CDeviceInputStream*> m_CurrentVertexStreams;
	SCachedValue<const CDeviceInputStream*> m_CurrentIndexStream;

	UsedBindSlotSet                         m_RequiredResourceBindings;
	UsedBindSlotSet                         m_ValidResourceBindings;
};

class CDeviceComputeCommandList : public CDeviceCopyCommandList
{
	friend CDeviceObjectFactory;

public:
	CDeviceComputeCommandList() : CDeviceCopyCommandList(eCT_GenericResources) {}

	void         PrepareUATargetsForUse(uint32 targetCount, CGpuBuffer** pTargets) const;
	void         PrepareResourcesForUse(uint32 bindSlot, CDeviceResourceSet* pResources) const;
	void         PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const;

	void         SetUATargets(uint32 targetCount, const CGpuBuffer* const* pTargets);
	void         SetPipelineState(CDeviceComputePSOConstPtr devicePSO);
	void         SetResourceLayout(CDeviceResourceLayoutConstPtr resourceLayout);
	void         SetResources(uint32 bindSlot, const CDeviceResourceSet* pResources);
	void         SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderStage);
	void         SetInlineConstants(uint32 bindSlot, uint32 constantCount, float* pConstants);

	void         Dispatch(uint32 X, uint32 Y, uint32 Z);

	void         ClearUAV(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRect);
	void         ClearUAV(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRect);

	virtual void LockToThread() final;
	virtual void Build() final;

	void         Reset();

protected:
	void PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const;
	void PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const;
	void PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages) const;

	void SetPipelineStateImpl(const CDeviceComputePSOConstPtr& devicePSO);
	void SetResourceLayoutImpl(const CDeviceResourceLayoutConstPtr& resourceLayout);
	void SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources);
	void SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass);
	void SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages);
	void SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants);

	void DispatchImpl(uint32 X, uint32 Y, uint32 Z);

	void ResetImpl();

	CDeviceComputePSOConstPtr                            m_pCurrentPipelineState;
	CDeviceResourceLayoutConstPtr                        m_pCurrentResourceLayout;
	std::array<const void*, EResourceLayoutSlot_Max + 1> m_pCurrentResources;

	UsedBindSlotSet m_RequiredResourceBindings;
	UsedBindSlotSet m_ValidResourceBindings;
};

typedef std::shared_ptr<CDeviceGraphicsCommandList> CDeviceGraphicsCommandListPtr;
typedef std::shared_ptr<CDeviceComputeCommandList>  CDeviceComputeCommandListPtr;
typedef std::shared_ptr<CDeviceCopyCommandList>     CDeviceCopyCommandListPtr;

typedef CDeviceGraphicsCommandList&                 CDeviceGraphicsCommandListRef;
typedef CDeviceComputeCommandList&                  CDeviceComputeCommandListRef;
typedef CDeviceCopyCommandList&                     CDeviceCopyCommandListRef;

typedef std::unique_ptr<CDeviceGraphicsCommandList> CDeviceGraphicsCommandListUPtr;
typedef std::unique_ptr<CDeviceComputeCommandList>  CDeviceComputeCommandListUPtr;
typedef std::unique_ptr<CDeviceCopyCommandList>     CDeviceCopyCommandListUPtr;

////////////////////////////////////////////////////////////////////////////
// Device Object Factory

class CDeviceObjectFactory
{
public:
	CDeviceObjectFactory();

	CDeviceResourceSetPtr    CreateResourceSet(CDeviceResourceSet::EFlags flags = CDeviceResourceSet::EFlags_None) const;
	CDeviceResourceSetPtr    CloneResourceSet(const CDeviceResourceSetPtr pSrcResourceSet) const; // NOTE: does not copy device dependent resources. be sure to call Build() before usage

	CDeviceResourceLayoutPtr CreateResourceLayout(const SDeviceResourceLayoutDesc& resourceLayoutDesc);
	CDeviceInputStream*      CreateVertexStreamSet(uint32 streamCount, const SStreamInfo* streams);
	CDeviceInputStream*      CreateIndexStreamSet(const SStreamInfo* stream);

	CDeviceGraphicsPSOPtr    CreateGraphicsPSO(const CDeviceGraphicsPSODesc& psoDesc);
	CDeviceComputePSOPtr     CreateComputePSO(const CDeviceComputePSODesc& psoDesc);

	// Get a pointer to the core graphics command-list, which runs on the command-queue assigned to Present().
	// Only the allocating thread is allowed to call functions on this command-list (DX12 restriction).
	CDeviceGraphicsCommandListPtr GetCoreGraphicsCommandList() const;

	// Acquire one or more command-lists which are independent of the core command-list
	// Only one thread is allowed to call functions on this command-list (DX12 restriction).
	// The thread that gets the permition is the one calling Begin() on it AFAICS
	CDeviceGraphicsCommandListUPtr              AcquireGraphicsCommandList();
	CDeviceComputeCommandListUPtr               AcquireComputeCommandList();
	CDeviceCopyCommandListUPtr                  AcquireCopyCommandList(CDeviceCopyCommandList::ECopyType eType);

	std::vector<CDeviceGraphicsCommandListUPtr> AcquireGraphicsCommandLists(uint32 listCount);
	std::vector<CDeviceComputeCommandListUPtr>  AcquireComputeCommandLists(uint32 listCount);
	std::vector<CDeviceCopyCommandListUPtr>     AcquireCopyCommandLists(uint32 listCount, CDeviceCopyCommandList::ECopyType eType);

	// Command-list sinks, will automatically submit command-lists in [global] allocation-order
	void ForfeitGraphicsCommandList(CDeviceGraphicsCommandListUPtr pCommandList);
	void ForfeitComputeCommandList(CDeviceComputeCommandListUPtr pCommandList);
	void ForfeitCopyCommandList(CDeviceCopyCommandListUPtr pCommandList);

	void ForfeitGraphicsCommandLists(std::vector<CDeviceGraphicsCommandListUPtr> pCommandLists);
	void ForfeitComputeCommandLists(std::vector<CDeviceComputeCommandListUPtr> pCommandLists);
	void ForfeitCopyCommandLists(std::vector<CDeviceCopyCommandListUPtr> pCommandLists);

	void ReleaseResources();
	void ReloadPipelineStates();

	void UpdatePipelineStates();

private:
	CDeviceGraphicsPSOPtr    CreateGraphicsPSOImpl(const CDeviceGraphicsPSODesc& psoDesc) const;
	CDeviceComputePSOPtr     CreateComputePSOImpl(const CDeviceComputePSODesc& psoDesc) const;

	CDeviceResourceLayoutPtr CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const;

	void                     ReleaseResourcesImpl();

	CDeviceGraphicsCommandListPtr m_pCoreCommandList;

	std::unordered_map<CDeviceGraphicsPSODesc, CDeviceGraphicsPSOPtr>  m_GraphicsPsoCache;
	std::unordered_map<CDeviceGraphicsPSODesc, CDeviceGraphicsPSOWPtr> m_InvalidGraphicsPsos;

	std::unordered_map<CDeviceComputePSODesc, CDeviceComputePSOPtr>    m_ComputePsoCache;
	std::unordered_map<CDeviceComputePSODesc, CDeviceComputePSOWPtr>   m_InvalidComputePsos;

	VectorMap<SDeviceResourceLayoutDesc, CDeviceResourceLayoutPtr>     m_ResourceLayoutCache;

	template<class T, const size_t N> struct SDeviceStreamInfoHash
	{
		size_t operator()(const T& x) const
		{
			return size_t(XXH64(x, sizeof(CDeviceInputStream) * N, 61));
		}
	};

	template<class T, const size_t N> struct SDeviceStreamInfoEquality
	{
		bool operator()(const T& x, const T& y) const
		{
			return !memcmp(x, y, sizeof(CDeviceInputStream) * N);
		}
	};

	std::unordered_set<CDeviceInputStream*, SDeviceStreamInfoHash<CDeviceInputStream*, 1>, SDeviceStreamInfoEquality<CDeviceInputStream*, 1>>             m_UniqueIndexStreams;
	std::unordered_set<CDeviceInputStream*, SDeviceStreamInfoHash<CDeviceInputStream*, VSF_NUM>, SDeviceStreamInfoEquality<CDeviceInputStream*, VSF_NUM>> m_UniqueVertexStreams;
};

struct SDeviceObjectHelpers
{
	struct SShaderInstanceInfo
	{
		SShaderInstanceInfo() : pHwShader(NULL), pHwShaderInstance(NULL), pDeviceShader(NULL) {}

		CHWShader_D3D* pHwShader;
		CCryNameTSCRC  technique; // temp
		void*          pHwShaderInstance;
		void*          pDeviceShader;
	};

	struct SConstantBufferBindInfo
	{
		EConstantBufferShaderSlot   shaderSlot;
		int                         vectorCount;
		EHWShaderClass              shaderClass;
		_smart_ptr<CConstantBuffer> pBuffer;
		_smart_ptr<CConstantBuffer> pPreviousBuffer;
		SShaderInstanceInfo         shaderInfo;
	};

	// Get shader instances for each shader stage
	static std::array<SShaderInstanceInfo, eHWSC_Num> GetShaderInstanceInfo(CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags, UPipelineState pipelineState[eHWSC_Num], bool bAllowTesselation);

	// Check if shader has tessellation support
	static bool CheckTessellationSupport(SShaderItem& shaderItem);

	// get constant buffers required by shader. NOTE: only CB_PER_BATCH, CB_PER_INSTANCE and CB_PER_FRAME supported currently
	static bool GetConstantBuffersFromShader(std::vector<SConstantBufferBindInfo>& outResult, CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags);

	// set up constant buffers and fill via reflection NOTE: only per batch, per instance, per frame and per camera supported
	static void BeginUpdateConstantBuffers(std::vector<SConstantBufferBindInfo>& constantBuffers);
	static void EndUpdateConstantBuffers(std::vector<SConstantBufferBindInfo>& constantBuffers);
};
