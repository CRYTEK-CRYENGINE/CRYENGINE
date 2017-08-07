// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

enum EHWShaderClass
{
	eHWSC_Vertex = 0,
	eHWSC_Pixel = 1,
	eHWSC_Geometry = 2,
	eHWSC_Compute = 3,
	eHWSC_Domain = 4,
	eHWSC_Hull = 5,
	eHWSC_Num = 6
};

#include <CryCore/Containers/VectorMap.h>   // VectorMap
#include <CryRenderer/VertexFormats.h>
#include <CryRenderer/ITexture.h>
#include <array>
#include <bitset>

#include "DeviceResources.h"                // CDeviceBuffer, CDeviceTexture, CDeviceInputStream
#include "Common/CommonRender.h"            // SResourceView, SSamplerState, SInputLayout
#include "Common/Shaders/ShaderCache.h"     // UPipelineState

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
typedef std::shared_ptr<CDeviceRenderPass> CDeviceRenderPassPtr;
/////////////////////////////////////////////////////////////////////////////////

enum EConstantBufferShaderSlot
{
	// Scaleform
	eConstantBufferShaderSlot_ScaleformMeshAttributes   = 0,
	eConstantBufferShaderSlot_ScaleformRenderParameters = 0,

	// Z/G-Buffer
	eConstantBufferShaderSlot_PerBatch          = 0,
	eConstantBufferShaderSlot_PerInstanceLegacy = 1, // Deprecated
	eConstantBufferShaderSlot_PerMaterial       = 3,
	eConstantBufferShaderSlot_PerPass           = 5,
	eConstantBufferShaderSlot_SkinQuat          = 9,
	eConstantBufferShaderSlot_SkinQuatPrev      = 10,
	eConstantBufferShaderSlot_VrProjection      = 11,
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
	EResourceLayoutSlot_VrProjectionCB     = 4,

	EResourceLayoutSlot_Max                = 7
};

enum EReservedTextureSlot
{
	EReservedTextureSlot_SkinExtraWeights       = 14,
	EReservedTextureSlot_AdjacencyInfo          = 15,
	EReservedTextureSlot_ComputeSkinVerts       = 16,
	EReservedTextureSlot_GpuParticleStream      = 14,
	EReservedTextureSlot_LightvolumeInfos       = 33,
	EReservedTextureSlot_LightVolumeRanges      = 34,
	EReservedTextureSlot_ParticlePositionStream = 35,
	EReservedTextureSlot_ParticleAxesStream     = 36,
	EReservedTextureSlot_ParticleColorSTStream  = 37,
	EReservedTextureSlot_TerrainBaseMap         = 29,
};

enum EShaderStage : uint8
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
const int ResourceSetBufferCount = 8;

typedef std::bitset<EResourceLayoutSlot_Max + 1> UsedBindSlotSet;

struct SProfilingStats
{
	int  numPSOSwitches;
	int  numLayoutSwitches;
	int  numResourceSetSwitches;
	int  numInlineSets;
	int  numPolygons;
	int  numDIPs;
	int  numInvalidDIPs;

	void Reset()
	{
		ZeroStruct(*this);
	}

	void Merge(const SProfilingStats& other)
	{
		CryInterlockedAdd(&numPSOSwitches, other.numPSOSwitches);
		CryInterlockedAdd(&numLayoutSwitches, other.numLayoutSwitches);
		CryInterlockedAdd(&numResourceSetSwitches, other.numResourceSetSwitches);
		CryInterlockedAdd(&numInlineSets, other.numInlineSets);
		CryInterlockedAdd(&numPolygons, other.numPolygons);
		CryInterlockedAdd(&numDIPs, other.numDIPs);
		CryInterlockedAdd(&numInvalidDIPs, other.numInvalidDIPs);
	}
};

////////////////////////////////////////////////////////////////////////////
struct SResourceMemoryAlignment
{
	UINT typeStride;   // in bytes
	UINT rowStride;    // in bytes
	UINT planeStride;  // in bytes
	UINT volumeStride; // in bytes

	template<typename T>
	static inline SResourceMemoryAlignment Linear(UINT width = 1, UINT height = 1, UINT depth = 1)
	{
		SResourceMemoryAlignment linear =
		{
			sizeof(T),
			sizeof(T) * width,
			sizeof(T) * width * height,
			sizeof(T) * width * height * depth
		};

		return linear;
	}

	static inline SResourceMemoryAlignment Linear(UINT type, UINT width = 1, UINT height = 1, UINT depth = 1)
	{
		SResourceMemoryAlignment linear =
		{
			type,
			type * width,
			type * width * height,
			type * width * height * depth
		};

		return linear;
	}
};

struct SResourceCoordinate
{
	UINT Left;         // Buffer: in bytes      Texture: in texels
	UINT Top;          // Buffer: N/A           Texture: in texels
	UINT Front;        // Buffer: N/A           Texture: in texels
	UINT Subresource;  // Buffer: arraySlice    Texture: arraySlice * tex->numMips + mipSlice
};

struct SResourceDimension
{
	UINT Width;        // Buffer: in bytes      Texture: in texels
	UINT Height;       // Buffer: N/A           Texture: in texels
	UINT Depth;        // Buffer: N/A           Texture: in texels
	UINT Subresources; // Buffer: arraySlices   Texture: arraySlices * tex->numMips + mipSlices (mipSlices currently must be a multiple of tex->numMips)
};

struct SResourceRegion
{
	SResourceCoordinate Offset;
	SResourceDimension Extent;
};

struct SResourceMemoryMapping
{
	SResourceMemoryAlignment MemoryLayout;
	SResourceCoordinate ResourceOffset;
	SResourceDimension Extent;
	UINT /*D3D11_COPY_FLAGS*/ Flags; // TODO: abstract flags
};

struct SResourceRegionMapping
{
	SResourceCoordinate SourceOffset;
	SResourceCoordinate DestinationOffset;
	SResourceDimension Extent;
	UINT /*D3D11_COPY_FLAGS*/ Flags; // TODO: abstract flags
};

// -------------------------------------------------------------------------

struct SSubresourcePayload
{
	SResourceMemoryAlignment m_sSysMemAlignment;
	const void*              m_pSysMem;
};

struct STexturePayload
{
	SSubresourcePayload* m_pSysMemSubresourceData;
	ETEX_TileMode        m_eSysMemTileMode;
	uint8                m_nDstMSAASamples;
	uint8                m_nDstMSAAQuality;

	STexturePayload()
	{
		m_pSysMemSubresourceData = nullptr;
		m_eSysMemTileMode = eTM_None;
		m_nDstMSAASamples = 1;
		m_nDstMSAAQuality = 0;
	}

	~STexturePayload()
	{
		SAFE_DELETE_ARRAY(m_pSysMemSubresourceData);
	}
};

////////////////////////////////////////////////////////////////////////////

class CConstantBuffer;
class CGpuBuffer;
class CTexture;

// Will notify resource's user that some data of the the resource was invalidated.
// dirtyFlags - one or more of the EDeviceDirtyFlags enum bits
//! Dirty flags will indicate what kind of device data was invalidated
enum EDeviceDirtyFlags
{
	eDeviceResourceDirty     = BIT(0),
	eDeviceResourceViewDirty = BIT(1),
	eResourceDestroyed       = BIT(2)
};

struct SResourceBinding
{
	typedef bool InvalidateCallbackSignature(void*, uint32);
	typedef std::function<InvalidateCallbackSignature> InvalidateCallbackFunction;

	enum class EResourceType : uint32
	{
		ConstantBuffer,
		Texture,
		Buffer,
		Sampler,
		InvalidType,
	};

	SResourceBinding();
	SResourceBinding(CTexture* pTexture, ResourceViewHandle view);
	SResourceBinding(const CGpuBuffer* pBuffer, ResourceViewHandle view);
	SResourceBinding(SamplerStateHandle samplerState);
	SResourceBinding(CConstantBuffer* pConstantBuffer);

	bool IsValid() const;
	void AddInvalidateCallback(void* pCallbackOwner, const InvalidateCallbackFunction& callback) const;
	void RemoveInvalidateCallback(void* pCallbackOwner) const;

	const std::pair<SResourceView, CDeviceResourceView*>* GetDeviceResourceViewInfo() const;
	template<typename T> T*                               GetDeviceResourceView() const;
	DXGI_FORMAT                                           GetResourceFormat() const;

	bool operator==(const SResourceBinding& other) const { return fastCompare == other.fastCompare && view == other.view && type == other.type; }

	union
	{
		CTexture*          pTexture;
		const CGpuBuffer*  pBuffer;
		CConstantBuffer*   pConstantBuffer;
		SamplerStateHandle samplerState;
		uintptr_t          fastCompare;
	};

	ResourceViewHandle view;
	EResourceType      type;
};

struct SResourceBindPoint
{
	enum class EFlags : uint8
	{
		None             = 0,
		IsTexture        = BIT(0), // need to distinguish between textures and buffers on vulkan
		IsStructured     = BIT(1)  // need to distinguish between structured and typed resources on vulkan as they produce different descriptors
	};

	enum class ESlotType : uint8 // NOTE: enum values need to match ResourceGroup enum from hlslcc and request enum from hlsl2spirv
	{
		ConstantBuffer      = 0,          // HLSL b slot
		TextureAndBuffer    = 1,          // HLSL t slot
		Sampler             = 2,          // HLSL s slot
		UnorderedAccessView = 3,          // HLSL u slot

		Count
	};

	SResourceBindPoint() : fastCompare(0) {}
	SResourceBindPoint(const SResourceBinding& resource, uint8 slotNumber, EShaderStage shaderStages);
	SResourceBindPoint(ESlotType type, uint8 slotNumber, EShaderStage shaderStages, EFlags flags = EFlags::None);
	
	bool operator<(const SResourceBindPoint& other) const
	{
		// ignore flags here
		constexpr uint32 flagsMask = ~(0xFF << (offsetof(SResourceBindPoint, flags) * 8));
		return (fastCompare & flagsMask) < (other.fastCompare & flagsMask);
	}

	union
	{
		struct
		{
			EShaderStage    stages;
			EFlags          flags;
			uint8           slotNumber;
			ESlotType       slotType;
		};

		uint32 fastCompare;
	};
};
static_assert(sizeof(SResourceBindPoint::fastCompare) == sizeof(SResourceBindPoint), "Size mismatch between fastCompare and bind point struct");

DEFINE_ENUM_FLAG_OPERATORS(SResourceBindPoint::EFlags)

class CDeviceResourceSetDesc : NoCopy
{
public:
	enum class EDirtyFlags
	{
		eNone           = 0,
		eDirtyBindPoint = BIT(0),
		eDirtyBinding   = BIT(1),

		eDirtyAll = eDirtyBindPoint | eDirtyBinding
	};

public:
	CDeviceResourceSetDesc(void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback);
	CDeviceResourceSetDesc(const CDeviceResourceSetDesc& other, void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback);
	~CDeviceResourceSetDesc();

	EDirtyFlags SetTexture(int shaderSlot, CTexture* pTexture, ResourceViewHandle hView, ::EShaderStage shaderStages);
	EDirtyFlags SetSampler(int shaderSlot, SamplerStateHandle hState, ::EShaderStage shaderStages);
	EDirtyFlags SetConstantBuffer(int shaderSlot, CConstantBuffer* pBuffer, ::EShaderStage shaderStages);
	EDirtyFlags SetBuffer(int shaderSlot, const CGpuBuffer* pBuffer, ResourceViewHandle hView, ::EShaderStage shaderStages);

	template<SResourceBinding::EResourceType resourceType>
	EDirtyFlags UpdateResource(const SResourceBindPoint& bindPoint, const SResourceBinding& binding);

	EDirtyFlags RemoveResource(const SResourceBindPoint& bindPoint);

	bool IsEmpty() const { return m_resources.empty(); }
	void Clear();

	const VectorMap <SResourceBindPoint, SResourceBinding>& GetResources() const { return m_resources; }

private:
	VectorMap <SResourceBindPoint, SResourceBinding> m_resources;
	SResourceBinding::InvalidateCallbackFunction     m_invalidateCallback;
	void*                                            m_invalidateCallbackOwner;
};
DEFINE_ENUM_FLAG_OPERATORS(CDeviceResourceSetDesc::EDirtyFlags)


class CDeviceResourceSet : NoCopy
{
	friend class CDeviceObjectFactory;
	friend struct SDeviceResourceLayoutDesc;
	friend class CDeviceResourceLayout;
	friend class CDeviceResourceLayout_DX12;
	friend class CRenderPassScheduler;

public:
	enum EFlags
	{
		EFlags_None                    = 0,
		EFlags_ForceSetAllState        = BIT(0), // Dx11 only: don't rely on shader reflection, set all resources contained in the resource set
	};

	CDeviceResourceSet(EFlags flags);
	virtual ~CDeviceResourceSet();

	bool         IsValid()  const { return m_bValid; }
	EFlags       GetFlags() const { return m_Flags;  }

	bool         Update(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags = CDeviceResourceSetDesc::EDirtyFlags::eDirtyAll);

protected:
	virtual bool UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags) = 0;

protected:
	EFlags m_Flags;

private:
	bool   m_bValid;
};
DEFINE_ENUM_FLAG_OPERATORS(CDeviceResourceSet::EFlags)

typedef std::shared_ptr<CDeviceResourceSet> CDeviceResourceSetPtr;

////////////////////////////////////////////////////////////////////////////

struct SDeviceResourceLayoutDesc
{
	enum class ELayoutSlotType : uint8
	{
		InlineConstantBuffer,
		ResourceSet
	};

	struct SLayoutBindPoint
	{
		ELayoutSlotType slotType;
		uint8           layoutSlot;

		bool operator==(const SLayoutBindPoint& other) const;
		bool operator< (const SLayoutBindPoint& other) const;
	};

	void            SetConstantBuffer(uint32 bindSlot, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages);
	void            SetResourceSet(uint32 bindSlot, const CDeviceResourceSetDesc& resourceSet);

	bool            IsValid() const;
	UsedBindSlotSet GetRequiredResourceBindings() const;

	uint64          GetHash() const;

	VectorMap<SLayoutBindPoint, VectorMap<SResourceBindPoint, SResourceBinding> > m_resourceBindings;

	bool operator<(const SDeviceResourceLayoutDesc& other) const;
};

static_assert(sizeof(SDeviceResourceLayoutDesc::SLayoutBindPoint) == sizeof(uint8) + sizeof(SDeviceResourceLayoutDesc::ELayoutSlotType), 
	"SDeviceResourceLayoutDesc::SLayoutBindPoint must not have padding since we directly calculate hashes based on the struct data");


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
	CDeviceGraphicsPSODesc();
	CDeviceGraphicsPSODesc(const CDeviceGraphicsPSODesc& other);
	CDeviceGraphicsPSODesc(CDeviceResourceLayoutPtr pResourceLayout, const SGraphicsPipelineStateDescription& pipelineDesc);

	CDeviceGraphicsPSODesc& operator=(const CDeviceGraphicsPSODesc& other);
	bool                    operator==(const CDeviceGraphicsPSODesc& other) const;

	uint64                  GetHash() const;

public:
	void  InitWithDefaults();

	void  FillDescs(D3D11_RASTERIZER_DESC& rasterizerDesc, D3D11_BLEND_DESC& blendDesc, D3D11_DEPTH_STENCIL_DESC& depthStencilDesc) const;
	uint32 CombineVertexStreamMasks(uint32 fromShader, uint32 fromObject) const;

public:
	_smart_ptr<CShader>        m_pShader;
	CCryNameTSCRC              m_technique;
	uint64                     m_ShaderFlags_RT;
	uint32                     m_ShaderFlags_MD;
	uint32                     m_ShaderFlags_MDV;
	EShaderQuality             m_ShaderQuality;
	uint32                     m_RenderState;
	uint32                     m_StencilState;
	uint8                      m_StencilReadMask;
	uint8                      m_StencilWriteMask;
	InputLayoutHandle          m_VertexFormat;
	uint32                     m_ObjectStreamMask;
	ECull                      m_CullMode;
	ERenderPrimitiveType       m_PrimitiveType;
	CDeviceResourceLayoutPtr   m_pResourceLayout;
	CDeviceRenderPassPtr       m_pRenderPass;
	bool                       m_bDepthClip;
	bool                       m_bAllowTesselation;
	bool                       m_bDynamicDepthBias; // When clear, SetDepthBias() may be ignored by the PSO. This may be faster on PS4 and VK. It has no effect on DX11 (always on) and DX12 (always off).
};

class CDeviceComputePSODesc
{
public:
	CDeviceComputePSODesc(const CDeviceComputePSODesc& other);
	CDeviceComputePSODesc(CDeviceResourceLayoutPtr pResourceLayout, CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags);

	CDeviceComputePSODesc& operator=(const CDeviceComputePSODesc& other);
	bool                   operator==(const CDeviceComputePSODesc& other) const;

	uint64                 GetHash() const;

public:
	void InitWithDefaults();

public:
	_smart_ptr<CShader>      m_pShader;
	CCryNameTSCRC            m_technique;
	uint64                   m_ShaderFlags_RT;
	uint32                   m_ShaderFlags_MD;
	uint32                   m_ShaderFlags_MDV;
	CDeviceResourceLayoutPtr m_pResourceLayout;
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
	enum class EInitResult : uint8
	{
		Success,
		Failure,
		ErrorShadersAndTopologyCombination,
	};

	static bool ValidateShadersAndTopologyCombination(const CDeviceGraphicsPSODesc& psoDesc, const std::array<void*, eHWSC_Num>& hwShaderInstances);

public:
	CDeviceGraphicsPSO()
		: m_bValid(false)
		, m_nUpdateCount(0)
	{}

	virtual ~CDeviceGraphicsPSO() {}

	virtual EInitResult Init(const CDeviceGraphicsPSODesc& psoDesc) = 0;
	bool                IsValid() const        { return m_bValid; }
	uint32              GetUpdateCount() const { return m_nUpdateCount;  }

	std::array<void*, eHWSC_Num>          m_pHwShaderInstances;

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
		, m_nUpdateCount(0)
		, m_pHwShaderInstance(nullptr)
	{}

	virtual ~CDeviceComputePSO() {}

	virtual bool Init(const CDeviceComputePSODesc& psoDesc) = 0;
	bool         IsValid() const { return m_bValid; }
	uint32       GetUpdateCount() const { return m_nUpdateCount; }

	void*          m_pHwShaderInstance;

protected:
	bool m_bValid;
	uint32 m_nUpdateCount;
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

template<class Impl>
class CDeviceTimestampGroup_Base
{
public:
	enum { kMaxTimestamps = 1024 };

	void   Init();

	void   BeginMeasurement();
	void   EndMeasurement();

	uint32 IssueTimestamp();
	bool   ResolveTimestamps();

	float  GetTimeMS(uint32 timestamp0, uint32 timestamp1);
};

////////////////////////////////////////////////////////////////////////////
template<typename T>
struct SCachedValue
{
	T cachedValue;
	SCachedValue() {}
	SCachedValue(const T& value) : cachedValue(value) {}

	template<typename U>
	ILINE bool Set(U newValue)
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

template<typename CustomSharedState, typename CustomGraphicsState, typename CustomComputeState>
class CDeviceCommandListCommon
{
protected:
	struct SCachedResourceState
	{
		SCachedValue<const CDeviceResourceLayout*> pResourceLayout;
		SCachedValue<const void*>                  pResources[EResourceLayoutSlot_Max + 1];

		UsedBindSlotSet                            requiredResourceBindings;
		UsedBindSlotSet                            validResourceBindings;
	};

	struct SCachedGraphicsState : SCachedResourceState
	{
		SCachedValue<const CDeviceGraphicsPSO*> pPipelineState;
		SCachedValue<const CDeviceInputStream*> vertexStreams;
		SCachedValue<const CDeviceInputStream*> indexStream;
		SCachedValue<int32>                     stencilRef;
		CustomGraphicsState                     custom;
	};

	struct SCachedComputeState : SCachedResourceState
	{
		SCachedValue<const CDeviceComputePSO*> pPipelineState;
		CustomComputeState                     custom;
	};

protected:
	SCachedGraphicsState m_graphicsState;
	SCachedComputeState  m_computeState;
	CustomSharedState    m_sharedState;

#if defined(ENABLE_PROFILING_CODE)
	ERenderPrimitiveType m_primitiveTypeForProfiling;
	SProfilingStats      m_profilingStats;
#endif
};

enum EResourceTransitionType
{
	eResTransition_TextureRead
};

#if CRY_RENDERER_GNM
	#include "GNM/DeviceCommandList_GNM.h"
#elif (CRY_RENDERER_VULKAN >= 10)
	#include "Vulkan/DeviceCommandList_Vulkan.h"
#elif (CRY_RENDERER_DIRECT3D >= 120)
	#include "D3D12/DeviceCommandList_D3D12.h"
#elif (CRY_RENDERER_DIRECT3D >= 110)
	#include "D3D11/DeviceCommandList_D3D11.h"
#endif

class CDeviceGraphicsCommandInterface : public CDeviceGraphicsCommandInterfaceImpl
{
public:
	void ClearState(bool bOutputMergerOnly) const;

	void PrepareUAVsForUse(uint32 viewCount, CGpuBuffer** pViews, bool bCompute) const;
	void PrepareRenderPassForUse(CDeviceRenderPass& renderPass) const;
	void PrepareResourceForUse(uint32 bindSlot, CTexture* pTexture, const ResourceViewHandle TextureView, ::EShaderStage srvUsage) const;
	void PrepareResourcesForUse(uint32 bindSlot, CDeviceResourceSet* pResources) const;
	void PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const;
	void PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages) const;
	void PrepareVertexBuffersForUse(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams) const;
	void PrepareIndexBufferForUse(const CDeviceInputStream* indexStream) const;
	void BeginResourceTransitions(uint32 numTextures, CTexture** pTextures, EResourceTransitionType type);

	void BeginRenderPass(const CDeviceRenderPass& renderPass, const D3DRectangle& renderArea);
	void EndRenderPass(const CDeviceRenderPass& renderPass);

	void SetViewports(uint32 vpCount, const D3DViewPort* pViewports);
	void SetScissorRects(uint32 rcCount, const D3DRectangle* pRects);
	void SetPipelineState(const CDeviceGraphicsPSO* devicePSO);
	void SetResourceLayout(const CDeviceResourceLayout* resourceLayout);
	void SetResources(uint32 bindSlot, const CDeviceResourceSet* pResources);
	void SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass);
	void SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages);
	void SetVertexBuffers(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams);
	void SetIndexBuffer(const CDeviceInputStream* indexStream); // NOTE: Take care with PSO strip cut/restart value and 32/16 bit indices
	void SetInlineConstants(uint32 bindSlot, uint32 constantCount, float* pConstants);
	void SetStencilRef(uint8 stencilRefValue);
	void SetDepthBias(float constBias, float slopeBias, float biasClamp);
	void SetModifiedWMode(bool enabled, uint32_t numViewports, const float* pA, const float* pB);

	void Draw(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation);
	void DrawIndexed(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation);

#define CLEAR_ZBUFFER           0x00000001l  /* Clear target z buffer, equals D3D11_CLEAR_DEPTH */
#define CLEAR_STENCIL           0x00000002l  /* Clear stencil planes, equals D3D11_CLEAR_STENCIL */
#define CLEAR_RTARGET           0x00000004l  /* Clear target surface */

	void ClearSurface(D3DSurface* pView, const ColorF& color, uint32 numRects = 0, const D3D11_RECT* pRects = nullptr);
	void ClearSurface(D3DDepthSurface* pView, int clearFlags, float depth = 0, uint8 stencil = 0, uint32 numRects = 0, const D3D11_RECT* pRects = nullptr);

	void BeginOcclusionQuery(D3DOcclusionQuery* pQuery);
	void EndOcclusionQuery(D3DOcclusionQuery* pQuery);
};

static_assert(sizeof(CDeviceGraphicsCommandInterface) == sizeof(CDeviceCommandListImpl), "CDeviceGraphicsCommandInterface cannot contain data members");

class CDeviceComputeCommandInterface : public CDeviceComputeCommandInterfaceImpl
{
public:
	void PrepareUAVsForUse(uint32 viewCount, CGpuBuffer** pViews) const;
	void PrepareResourcesForUse(uint32 bindSlot, CDeviceResourceSet* pResources) const;
	void PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlots, ::EShaderStage shaderStages) const;

	void SetPipelineState(const CDeviceComputePSO* pDevicePSO);
	void SetResourceLayout(const CDeviceResourceLayout* pResourceLayout);
	void SetResources(uint32 bindSlot, const CDeviceResourceSet* pResources);
	void SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot);
	void SetInlineConstants(uint32 bindSlot, uint32 constantCount, float* pConstants);

	void Dispatch(uint32 X, uint32 Y, uint32 Z);

	void ClearUAV(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects);
	void ClearUAV(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects);
};

static_assert(sizeof(CDeviceGraphicsCommandInterface) == sizeof(CDeviceCommandListImpl), "CDeviceComputeCommandInterface cannot contain data members");

class CDeviceNvidiaCommandInterface : public CDeviceNvidiaCommandInterfaceImpl
{
public:
	void SetModifiedWMode(bool enabled, uint32 numViewports, const float* pA, const float* pB);
};

static_assert(sizeof(CDeviceNvidiaCommandInterface) == sizeof(CDeviceCommandListImpl), "CDeviceNvidiaCommandInterface cannot contain data members");

class CDeviceCopyCommandInterface : public CDeviceCopyCommandInterfaceImpl
{
	// TODO: CopyStructureCount    (DX11, graphics/compute queue only)
	// TODO: ResolveSubresource    (graphics queue only)
	// TODO: CopyResourceOvercross (MultiGPU, copy-queue)

public:
	void Copy(CDeviceBuffer*  pSrc, CDeviceBuffer*  pDst);
	void Copy(D3DBuffer*      pSrc, D3DBuffer*      pDst);
	void Copy(CDeviceTexture* pSrc, CDeviceTexture* pDst);
	void Copy(CDeviceTexture* pSrc, D3DTexture*     pDst);
	void Copy(D3DTexture*     pSrc, D3DTexture*     pDst);
	void Copy(D3DTexture*     pSrc, CDeviceTexture* pDst);

	void Copy(CDeviceBuffer*  pSrc, CDeviceBuffer*  pDst, const SResourceRegionMapping& regionMapping);
	void Copy(D3DBuffer*      pSrc, D3DBuffer*      pDst, const SResourceRegionMapping& regionMapping);
	void Copy(CDeviceTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& regionMapping);
	void Copy(D3DTexture*     pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& regionMapping);

	void Copy(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryAlignment& memoryLayout);
	void Copy(const void* pSrc, CDeviceBuffer*   pDst, const SResourceMemoryAlignment& memoryLayout);
	void Copy(const void* pSrc, CDeviceTexture*  pDst, const SResourceMemoryAlignment& memoryLayout);

	void Copy(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryMapping& memoryMapping);
	void Copy(const void* pSrc, CDeviceBuffer*   pDst, const SResourceMemoryMapping& memoryMapping);
	void Copy(const void* pSrc, CDeviceTexture*  pDst, const SResourceMemoryMapping& memoryMapping);

	void Copy(CDeviceBuffer*  pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout);
	void Copy(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout);

	void Copy(CDeviceBuffer*  pSrc, void* pDst, const SResourceMemoryMapping& memoryMapping);
	void Copy(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryMapping& memoryMapping);
};

static_assert(sizeof(CDeviceCopyCommandInterface) == sizeof(CDeviceCommandListImpl), "CDeviceCopyCommandInterface cannot contain data members");

class CDeviceCommandList : public CDeviceCommandListImpl
{
	friend class CDeviceObjectFactory;

public:
	CDeviceCommandList() { Reset(); }

	CDeviceGraphicsCommandInterface* GetGraphicsInterface();
	CDeviceComputeCommandInterface*  GetComputeInterface();
	CDeviceNvidiaCommandInterface*   GetNvidiaCommandInterface();
	CDeviceCopyCommandInterface*     GetCopyInterface();

	void                             Reset();
	void                             LockToThread();
	void                             Close();

#if defined(ENABLE_PROFILING_CODE)
	void                   BeginProfilingSection();
	const SProfilingStats& EndProfilingSection();
#endif
};

static_assert(sizeof(CDeviceCommandList) == sizeof(CDeviceCommandListImpl), "CDeviceCommandList cannot contain data members");

typedef CDeviceCommandList&                 CDeviceCommandListRef;
typedef std::unique_ptr<CDeviceCommandList> CDeviceCommandListUPtr;

////////////////////////////////////////////////////////////////////////////
// Device Render Pass

class CDeviceRenderPassDesc : NoCopy
{
	friend class CDeviceObjectFactory;

public:
	enum { MaxRendertargetCount = 4 };
	enum { MaxOutputUAVCount = 3 };

	struct SHash  { uint64 operator() (const CDeviceRenderPassDesc& desc)                                  const; };
	struct SEqual { bool   operator() (const CDeviceRenderPassDesc& lhs, const CDeviceRenderPassDesc& rhs) const; };

public:
	CDeviceRenderPassDesc(void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback);
	CDeviceRenderPassDesc(const CDeviceRenderPassDesc& other, void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback);
	~CDeviceRenderPassDesc();

	bool SetRenderTarget(uint32 slot, CTexture* pTexture, ResourceViewHandle hView = EDefaultResourceViews::RenderTarget);
	bool SetDepthTarget(CTexture* pTexture, ResourceViewHandle hView = EDefaultResourceViews::DepthStencil);
	bool SetOutputUAV(uint32 slot, CGpuBuffer* pBuffer);

	void Clear();

	bool GetDeviceRendertargetViews(std::array<D3DSurface*, MaxRendertargetCount>& views, int& viewCount) const;
	bool GetDeviceDepthstencilView(D3DDepthSurface*& pView) const;

	const std::array<SResourceBinding, MaxRendertargetCount>& GetRenderTargets()           const { return m_renderTargets; }
	const SResourceBinding&                                   GetDepthTarget()             const { return m_depthTarget; }
	const std::array<SResourceBinding, MaxOutputUAVCount>&    GetOutputUAVs()              const { return m_outputUAVs;  }

protected:
	bool UpdateResource(SResourceBinding& dstResource, const SResourceBinding& srcResource);

	std::array<SResourceBinding, MaxRendertargetCount> m_renderTargets;
	std::array<SResourceBinding, MaxOutputUAVCount>    m_outputUAVs;
	SResourceBinding                                   m_depthTarget;

	SResourceBinding::InvalidateCallbackFunction       m_invalidateCallback;
	void*                                              m_invalidateCallbackOwner;
};

class CDeviceRenderPass_Base : public NoCopy
{
	friend class CDeviceObjectFactory;

public:
	CDeviceRenderPass_Base();
	virtual ~CDeviceRenderPass_Base() {};
	bool         Update(const CDeviceRenderPassDesc& passDesc);
	bool         IsValid() const { return m_bValid; }
	void         Invalidate()    { m_bValid = false; }
	uint64       GetHash() const { return m_nHash; }

private:
	virtual bool UpdateImpl(const CDeviceRenderPassDesc& passDesc) = 0;

protected:
	uint64                 m_nHash;
	uint32                 m_nUpdateCount;
	bool                   m_bValid;

#if !defined(RELEASE)
	std::array<DXGI_FORMAT, CDeviceRenderPassDesc::MaxRendertargetCount+1>  m_targetFormats;
#endif
};
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

	void OnEndFrame()
	{
#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		m_texturePool.RT_Tick();
#endif
	}

	void OnBeginFrame()
	{
#if CRY_RENDERER_VULKAN
		UpdateDeferredUploads();
#endif
	}

	UINT64 QueryFormatSupport(D3DFormat Format);

	////////////////////////////////////////////////////////////////////////////
	// Fence API (TODO: offload all to CDeviceFenceHandle)

	HRESULT CreateFence(DeviceFenceHandle& query);
	HRESULT ReleaseFence(DeviceFenceHandle query);
	HRESULT IssueFence(DeviceFenceHandle query);
	HRESULT SyncFence(DeviceFenceHandle query, bool block, bool flush = true);

	void    SyncToGPU();
	void    IssueFrameFences();
	void    ReleaseFrameFences();

	uint32  GetCurrentFrameCounter()   const { return m_frameFenceCounter; }
	uint32  GetCompletedFrameCounter() const { return m_completedFrameFenceCounter; }

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
	static void ReserveInputLayouts(const uint32 hNum) { s_InputLayouts.Reserve(hNum); }

	static InputLayoutHandle GetOrCreateInputLayoutHandle(const SInputLayout& pState) { return s_InputLayouts.GetOrCreateHandle(pState); }
	static const std::pair<SInputLayout, CDeviceInputLayout*>& LookupInputLayout(const InputLayoutHandle hState) { return s_InputLayouts.Lookup(hState); }

	// Higher level input-layout composition / / / / / / / / / / / / / / / / / /
	static InputLayoutHandle GetOrCreateInputLayoutHandle(const SShaderBlob* pVS, int StreamMask, int InstAttrMask, uint32 nUsedAttr, byte Attributes[], const InputLayoutHandle VertexFormat);
	static InputLayoutHandle GetOrCreateInputLayoutHandle(const SShaderBlob* pVS, size_t numDescs, const D3D11_INPUT_ELEMENT_DESC* inputLayout);

	////////////////////////////////////////////////////////////////////////////
	// PipelineState API

	CDeviceGraphicsPSOPtr    CreateGraphicsPSO(const CDeviceGraphicsPSODesc& psoDesc);
	CDeviceComputePSOPtr     CreateComputePSO(const CDeviceComputePSODesc& psoDesc);

	void                     ReloadPipelineStates();
	void                     UpdatePipelineStates();
	void                     TrimPipelineStates();

	////////////////////////////////////////////////////////////////////////////
	// Input dataset(s) API

	CDeviceResourceSetPtr     CreateResourceSet(CDeviceResourceSet::EFlags flags = CDeviceResourceSet::EFlags_None) const;
	CDeviceResourceLayoutPtr  CreateResourceLayout(const SDeviceResourceLayoutDesc& resourceLayoutDesc);
	const CDeviceInputStream* CreateVertexStreamSet(uint32 numStreams, const SStreamInfo* streams);
	const CDeviceInputStream* CreateIndexStreamSet(const SStreamInfo* stream);

	void                      TrimResourceLayouts();

#if CRY_RENDERER_VULKAN
	const std::vector<uint8>* LookupResourceLayoutEncoding(uint64 layoutHash);
	VkDescriptorSetLayout     GetInlineConstantBufferLayout();
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

		USAGE_HIFREQ_HEAP = BIT(31)  // Resource is reallocated every frame or multiple times each frame, use a recycling heap with delayed deletes
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
	NCryDX12::CDevice*           GetDX12Device   () const { return m_pDX12Device; }
	NCryDX12::CCommandScheduler* GetDX12Scheduler() const { return m_pDX12Scheduler; }
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
	static bool OnRenderPassInvalidated(void* pRenderPass, uint32 flags);

	void ReleaseResources();
	void ReleaseResourcesImpl();

	void TrimResources();

#if (CRY_RENDERER_DIRECT3D >= 120)
	NCryDX12::CDevice*           m_pDX12Device;
	NCryDX12::CCommandScheduler* m_pDX12Scheduler;
#elif (CRY_RENDERER_VULKAN >= 10)
	NCryVulkan::CDevice*             m_pVKDevice;
	NCryVulkan::CCommandScheduler*   m_pVKScheduler;
	VkDescriptorSetLayout            m_inlineConstantBufferLayout;

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
	// Fence API (TODO: offload all to CDeviceFenceHandle)

	// Internal handle for debugging
	DeviceFenceHandle m_fence_handle;

	uint32 m_frameFenceCounter;
	uint32 m_completedFrameFenceCounter;
	DeviceFenceHandle m_frameFences[MAX_FRAMES_IN_FLIGHT];

	////////////////////////////////////////////////////////////////////////////
	// SamplerState API

	// A heap containing all permutations of SamplerState, they are global and are never evicted
	static CDeviceSamplerState* CreateSamplerState(const SSamplerState& pState);
	static CStaticDeviceObjectStorage<SamplerStateHandle, SSamplerState, CDeviceSamplerState, false, CreateSamplerState> s_SamplerStates;

	////////////////////////////////////////////////////////////////////////////
	// InputLayout API

	// A heap containing all permutations of InputLayout, they are global and are never evicted
	static CDeviceInputLayout* CreateInputLayout(const SInputLayout& pState);
	static CStaticDeviceObjectStorage<InputLayoutHandle, SInputLayout, CDeviceInputLayout, true, CreateInputLayout> s_InputLayouts;

	// Higher level input-layout composition / / / / / / / / / / / / / / / / / /
	static std::vector<InputLayoutHandle> s_InputLayoutPermutations[1 << VSF_NUM][3]; // [StreamMask][Morph][VertexFmt]

	////////////////////////////////////////////////////////////////////////////
	// PipelineState API

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
	static void  FreebackingStorage(void* base_ptr);

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
