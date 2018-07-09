// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <array>
#include <atomic>

#include "RenderPassBase.h"
#include "../../DeviceManager/DeviceObjects.h" // SDeviceObjectHelpers
#include "../../../Common/DevBuffer.h" // buffer_handle_t, CConstantBuffer, CGpuBuffer

class CPrimitiveRenderPass;

struct SCompiledRenderPrimitive : private NoCopy
{
	enum EType : uint8
	{
		eType_Base = 0,
		eType_RenderPrimitive,
	};

	SCompiledRenderPrimitive(SCompiledRenderPrimitive::EType type = eType_Base) : m_type(type) {};
	SCompiledRenderPrimitive(SCompiledRenderPrimitive&& other);

	void Reset();

	struct SDrawInfo
	{
		uint32        vertexOrIndexCount  = 0;
		uint32        vertexOrIndexOffset = 0;
		uint32        vertexBaseOffset    = 0;
	};

	CDeviceGraphicsPSOPtr      m_pPipelineState;
	CDeviceResourceLayoutPtr   m_pResourceLayout;
	CDeviceResourceSetPtr      m_pResources;
	const CDeviceInputStream*  m_pVertexInputSet = nullptr;
	const CDeviceInputStream*  m_pIndexInputSet  = nullptr;
	
	// Reserve 3 optional inline constant buffers for a primitive
	std::array<SDeviceObjectHelpers::SConstantBufferBindInfo,3> m_inlineConstantBuffers;

	uint8                      m_stencilRef = 0;
	EType                      m_type = eType_Base;
	SDrawInfo                  m_drawInfo;
};


class CRenderPrimitive : public SCompiledRenderPrimitive
{
public:
	enum EPrimitiveType
	{
		ePrim_Triangle,
		ePrim_ProceduralTriangle,     // Triangle generated procedurally on GPU (no vertex stream)
		ePrim_ProceduralQuad,         // Quad generated procedurally on GPU (no vertex stream)
		ePrim_UnitBox,                // axis aligned box ( 0, 0, 0) - (1,1,1)
		ePrim_CenteredBox,            // axis aligned box (-1,-1,-1) - (1,1,1)
		ePrim_Projector,              // pyramid shape with sparsely tessellated ground plane
		ePrim_Projector1,             // pyramid shape with semi densely tessellated ground plane
		ePrim_Projector2,             // pyramid shape with densely tessellated ground plane
		ePrim_ClipProjector,          // same as ePrim_Projector  but with even denser tessellation
		ePrim_ClipProjector1,         // same as ePrim_Projector1 but with even denser tessellation
		ePrim_ClipProjector2,         // same as ePrim_Projector2 but with even denser tessellation
		ePrim_FullscreenQuad,         // fullscreen quad             ( 0  0, 0) - ( 1  1, 0)
		ePrim_FullscreenQuadCentered, // fullscreen quad             (-1,-1, 0) - ( 1, 1, 0). UV layout (0,0)=bottom left, (1,1)=top right
		ePrim_FullscreenQuadTess,     // tessellated fullscreen quad (-1,-1, 0) - ( 1, 1, 0). UV layout (0,0)=bottom left, (1,1)=top right
		ePrim_Custom,

		ePrim_Count,
		ePrim_First = ePrim_Triangle
	};

	enum EDirtyFlags : uint32
	{
		eDirty_ResourceLayout = BIT(0),
		eDirty_Resources      = BIT(1),
		eDirty_Technique      = BIT(2),
		eDirty_RenderState    = BIT(3),
		eDirty_Geometry       = BIT(4),
		eDirty_InstanceData   = BIT(5),
		eDirty_Topology       = BIT(6),
		eDirty_RenderPass     = BIT(7),

		eDirty_None = 0,
		eDirty_All = eDirty_Technique | eDirty_RenderState | eDirty_Resources | eDirty_Geometry | eDirty_ResourceLayout | eDirty_InstanceData | eDirty_Topology | eDirty_RenderPass
	};

	static_assert(uint(eDirty_ResourceLayout) == uint(CDeviceResourceSetDesc::EDirtyFlags::eDirtyBindPoint), "eDirty_ResourceLayout needs to match CDeviceResourceSetDesc::EDirtyFlags::eDirtyBindPoint");
	static_assert(uint(eDirty_Resources)      == uint(CDeviceResourceSetDesc::EDirtyFlags::eDirtyBinding),   "eDirty_Resources needs to match CDeviceResourceSetDesc::EDirtyFlags::eDirtyBinding");

	enum EPrimitiveFlags
	{
		eFlags_None                      = 0,
		eFlags_ReflectShaderConstants_VS = BIT(0),
		eFlags_ReflectShaderConstants_PS = BIT(1),
		eFlags_ReflectShaderConstants_GS = BIT(2),

		eFlags_ReflectShaderConstants      = eFlags_ReflectShaderConstants_VS | eFlags_ReflectShaderConstants_PS, // default: reflect vs and ps constants
		eFlags_ReflectShaderConstants_Mask = eFlags_ReflectShaderConstants_VS | eFlags_ReflectShaderConstants_PS | eFlags_ReflectShaderConstants_GS
	};

	typedef SDeviceObjectHelpers::CShaderConstantManager ConstantManager;

public:
	CRenderPrimitive(CRenderPrimitive&& other);
	CRenderPrimitive(EPrimitiveFlags flags = eFlags_None);

	void          Reset(EPrimitiveFlags flags = eFlags_None);

	void          AllocateTypedConstantBuffer(EConstantBufferShaderSlot shaderSlot, int size, EShaderStage shaderStages);

	void          SetFlags(EPrimitiveFlags flags);
	void          SetRenderState(int state);
	void          SetStencilState(int state, uint8 stencilRef, uint8 stencilReadMask = 0xFF, uint8 stencilWriteMask = 0xFF);
	void          SetCullMode(ECull cullMode);
	void          SetEnableDepthClip(bool bEnable);
	void          SetTechnique(CShader* pShader, const CCryNameTSCRC& techName, uint64 rtMask);
	void          SetTexture(uint32 shaderSlot, CTexture* pTexture, ResourceViewHandle resourceViewID = EDefaultResourceViews::Default, EShaderStage shaderStages = EShaderStage_Pixel);
	void          SetSampler(uint32 shaderSlot, SamplerStateHandle sampler, EShaderStage shaderStages = EShaderStage_Pixel);
	void          SetConstantBuffer(uint32 shaderSlot, CConstantBuffer* pBuffer, EShaderStage shaderStages = EShaderStage_Pixel);
	void          SetBuffer(uint32 shaderSlot, CGpuBuffer* pBuffer, ResourceViewHandle resourceViewID = EDefaultResourceViews::Default, EShaderStage shaderStages = EShaderStage_Pixel);
	void          SetInlineConstantBuffer(EConstantBufferShaderSlot shaderSlot, CConstantBuffer* pBuffer, EShaderStage shaderStages = EShaderStage_Pixel);
	void          SetPrimitiveType(EPrimitiveType primitiveType);
	void          SetCustomVertexStream(buffer_handle_t vertexBuffer, InputLayoutHandle vertexFormat, uint32 vertexStride);
	void          SetCustomIndexStream(buffer_handle_t indexBuffer, RenderIndexType indexType);
	void          SetDrawInfo(ERenderPrimitiveType primType, uint32 vertexBaseOffset, uint32 vertexOrIndexOffset, uint32 vertexOrIndexCount);
	void          SetDrawTopology(ERenderPrimitiveType primType);

	bool          IsDirty() const;

	CShader*      GetShader()             const { return m_pShader; }
	CCryNameTSCRC GetShaderTechnique()    const { return m_techniqueName; }
	uint64        GetShaderRtMask()       const { return m_rtMask; }

	ConstantManager&       GetConstantManager()       { return m_constantManager; }
	const ConstantManager& GetConstantManager() const { return m_constantManager; }

	const CDeviceResourceSetDesc& GetResourceDesc() const { return m_resourceDesc; }

	static void      AddPrimitiveGeometryCacheUser();
	static void      RemovePrimitiveGeometryCacheUser();

	EDirtyFlags Compile(const CPrimitiveRenderPass& targetPass);

private:

	struct SPrimitiveGeometry
	{
		ERenderPrimitiveType primType;
		InputLayoutHandle        vertexFormat;

		uint32        vertexBaseOffset;
		uint32        vertexOrIndexCount;
		uint32        vertexOrIndexOffset;

		SStreamInfo   vertexStream;
		SStreamInfo   indexStream;

		SPrimitiveGeometry();
	};

private:
	EPrimitiveFlags           m_flags;
	EDirtyFlags               m_dirtyMask;
	uint32                    m_renderState;
	uint32                    m_stencilState;
	uint8                     m_stencilReadMask;
	uint8                     m_stencilWriteMask;
	ECull                     m_cullMode;
	bool                      m_bDepthClip;
	CShader*                  m_pShader;
	CCryNameTSCRC             m_techniqueName;
	uint64                    m_rtMask;

	EPrimitiveType            m_primitiveType;
	SPrimitiveGeometry        m_primitiveGeometry;

	CDeviceResourceSetDesc    m_resourceDesc;

	ConstantManager           m_constantManager;

	uint32                    m_currentPsoUpdateCount;

	uint64                    m_renderPassHash;

	static SPrimitiveGeometry s_primitiveGeometryCache[ePrim_Count];
	static int                s_nPrimitiveGeometryCacheUsers;
};

DEFINE_ENUM_FLAG_OPERATORS(CRenderPrimitive::EDirtyFlags);
DEFINE_ENUM_FLAG_OPERATORS(CRenderPrimitive::EPrimitiveFlags);

struct VrProjectionInfo;

class CPrimitiveRenderPass : public CRenderPassBase, private NoCopy
{
	friend class CRenderPassScheduler;

public:
	enum EPrimitivePassFlags
	{
		ePassFlags_None                         = 0,
		ePassFlags_UseVrProjectionState         = BIT(0),
		ePassFlags_RequireVrProjectionConstants = BIT(1),

		ePassFlags_VrProjectionPass             = ePassFlags_UseVrProjectionState | ePassFlags_RequireVrProjectionConstants // for convenience
	};

	enum EClearMask
	{
		eClear_None    = 0,
		eClear_Stencil = BIT(1),
		eClear_Color0  = BIT(2)
	};


	CPrimitiveRenderPass(bool createGeometryCache = true);
	CPrimitiveRenderPass(CPrimitiveRenderPass&& other);
	~CPrimitiveRenderPass();

	void   SetFlags(EPrimitivePassFlags flags) { m_passFlags = flags; }

	void   SetRenderTarget(uint32 slot, CTexture* pRenderTarget, ResourceViewHandle hRenderTargetView = EDefaultResourceViews::RenderTarget);
	void   SetDepthTarget(CTexture* pDepthTarget, ResourceViewHandle hDepthTargetView = EDefaultResourceViews::DepthStencil);
	void   SetOutputUAV(uint32 slot, CGpuBuffer* pBuffer);
	void   SetViewport(const D3DViewPort& viewport);
	void   SetViewport(const SRenderViewport& viewport);
	void   SetScissor(bool bEnable, const D3DRectangle& scissor);
	void   SetTargetClearMask(uint32 clearMask);

	bool   IsDirty() const override { return m_renderPassDesc.HasChanged(); }
	using  CRenderPassBase::IsDirty;

	const  D3DViewPort& GetViewport() const { return m_viewport; }

	void   Reset();

	void   BeginAddingPrimitives(bool bClearPrimitiveList = true);
	bool   AddPrimitive(CRenderPrimitive* pPrimitive);
	bool   AddPrimitive(SCompiledRenderPrimitive* pPrimitive);
	void   UndoAddPrimitive() { CRY_ASSERT(!m_compiledPrimitives.empty()); m_compiledPrimitives.pop_back(); }
	void   ClearPrimitives();

	uint32                     GetPrimitiveCount()            const { return m_compiledPrimitives.size(); }
	CTexture*                  GetRenderTarget(int index)     const { return m_renderPassDesc.GetRenderTargets()[index].pTexture; }
	ResourceViewHandle         GetRenderTargetView(int index) const { return m_renderPassDesc.GetRenderTargets()[index].view; }
	const CTexture*            GetDepthTarget()               const { return m_renderPassDesc.GetDepthTarget().pTexture; }
	ResourceViewHandle         GetDepthTargetView()           const { return m_renderPassDesc.GetDepthTarget().view; }
	
	CDeviceResourceSetPtr         GetOutputResourceSet()      const { return m_pOutputResourceSet; }
	const CDeviceResourceSetDesc& GetOutputResources()        const { return m_outputResources; }
	const CDeviceRenderPassPtr    GetRenderPass()             const { return m_pRenderPass; }

	void   Execute();

protected:
	void   Prepare(CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void   Compile();

	static bool OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe;

protected:
	EPrimitivePassFlags                     m_passFlags;
	CDeviceRenderPassDesc                   m_renderPassDesc;
	CDeviceRenderPassPtr                    m_pRenderPass;
	CDeviceResourceSetDesc                  m_outputResources;
	CDeviceResourceSetPtr                   m_pOutputResourceSet;
	CDeviceResourceSetDesc                  m_outputNULLResources;
	CDeviceResourceSetPtr                   m_pOutputNULLResourceSet;
	D3DViewPort                             m_viewport;
	D3DRectangle                            m_scissor;
	bool                                    m_scissorEnabled;
	bool                                    m_bAddingPrimitives;
	uint32                                  m_clearMask;
	std::vector<SCompiledRenderPrimitive*>  m_compiledPrimitives;
};


///////////////////////////////////// Inline functions for CRenderPrimitive /////////////////////////////////////

#define ASSIGN_VALUE(dst, src, dirtyFlag)                     \
  m_dirtyMask |= !((dst)==(src)) ? (dirtyFlag) : eDirty_None; \
  (dst) = (src);

inline void CRenderPrimitive::SetFlags(EPrimitiveFlags flags)
{
	ASSIGN_VALUE(m_flags, flags, eDirty_ResourceLayout);
}

inline void CRenderPrimitive::SetRenderState(int state)
{
	ASSIGN_VALUE(m_renderState, state, eDirty_RenderState);
}

inline void CRenderPrimitive::SetStencilState(int state, uint8 stencilRef, uint8 stencilReadMask, uint8 stencilWriteMask)
{
	ASSIGN_VALUE(m_stencilState, state, eDirty_RenderState);
	ASSIGN_VALUE(m_stencilReadMask, stencilReadMask, eDirty_RenderState);
	ASSIGN_VALUE(m_stencilWriteMask, stencilWriteMask, eDirty_RenderState);
	m_stencilRef = stencilRef;
}

inline void CRenderPrimitive::SetCullMode(ECull cullMode)
{
	ASSIGN_VALUE(m_cullMode, cullMode, eDirty_RenderState);
}

inline void CRenderPrimitive::SetEnableDepthClip(bool bEnable)
{
	ASSIGN_VALUE(m_bDepthClip, bEnable, eDirty_RenderState);
}

inline void CRenderPrimitive::SetTechnique(CShader* pShader, const CCryNameTSCRC& techName, uint64 rtMask)
{
	ASSIGN_VALUE(m_pShader, pShader, eDirty_Technique);
	ASSIGN_VALUE(m_techniqueName, techName, eDirty_Technique);
	ASSIGN_VALUE(m_rtMask, rtMask, eDirty_Technique);

	if (m_pPipelineState && m_currentPsoUpdateCount != m_pPipelineState->GetUpdateCount())
		m_dirtyMask |= eDirty_Technique;
}

inline void CRenderPrimitive::SetInlineConstantBuffer(EConstantBufferShaderSlot shaderSlot, CConstantBuffer* pBuffer, EShaderStage shaderStages)
{
	if (m_constantManager.SetTypedConstantBuffer(shaderSlot, pBuffer, shaderStages))
		m_dirtyMask |= eDirty_ResourceLayout;
}

inline void CRenderPrimitive::SetPrimitiveType(EPrimitiveType primitiveType)
{
	ASSIGN_VALUE(m_primitiveType, primitiveType, eDirty_Geometry);
}

inline void CRenderPrimitive::SetCustomVertexStream(buffer_handle_t vertexBuffer, InputLayoutHandle vertexFormat, uint32 vertexStride)
{
	ASSIGN_VALUE(m_primitiveGeometry.vertexStream.hStream, vertexBuffer, eDirty_Geometry);
	ASSIGN_VALUE(m_primitiveGeometry.vertexStream.nStride, vertexStride, eDirty_Geometry);
	ASSIGN_VALUE(m_primitiveGeometry.vertexFormat, vertexFormat, eDirty_Geometry | eDirty_Topology);
	ASSIGN_VALUE(m_primitiveType, ePrim_Custom, eDirty_Geometry);
}

inline void CRenderPrimitive::SetCustomIndexStream(buffer_handle_t indexBuffer, RenderIndexType indexType)
{
	ASSIGN_VALUE(m_primitiveGeometry.indexStream.hStream, indexBuffer, eDirty_Geometry);
	ASSIGN_VALUE(m_primitiveGeometry.indexStream.nStride, indexType, eDirty_Geometry);
	ASSIGN_VALUE(m_primitiveType, ePrim_Custom, eDirty_Geometry);
}

inline void CRenderPrimitive::SetDrawInfo(ERenderPrimitiveType primType, uint32 vertexBaseOffset, uint32 vertexOrIndexOffset, uint32 vertexOrIndexCount)
{
	ASSIGN_VALUE(m_primitiveGeometry.primType, primType, eDirty_Topology);
	ASSIGN_VALUE(m_primitiveGeometry.vertexBaseOffset, vertexBaseOffset, eDirty_InstanceData);
	ASSIGN_VALUE(m_primitiveGeometry.vertexOrIndexOffset, vertexOrIndexOffset, eDirty_InstanceData);
	ASSIGN_VALUE(m_primitiveGeometry.vertexOrIndexCount, vertexOrIndexCount, eDirty_InstanceData);
}

inline void CRenderPrimitive::SetDrawTopology(ERenderPrimitiveType primType)
{
	ASSIGN_VALUE(m_primitiveGeometry.primType, primType, eDirty_Topology);
}

#undef ASSIGN_VALUE

// ------------------------------------------------------------------------

inline void CRenderPrimitive::SetTexture(uint32 shaderSlot, CTexture* pTexture, ResourceViewHandle resourceViewID, EShaderStage shaderStages)
{
	m_resourceDesc.SetTexture(shaderSlot, pTexture, resourceViewID, shaderStages);
}

inline void CRenderPrimitive::SetSampler(uint32 shaderSlot, SamplerStateHandle sampler, EShaderStage shaderStages)
{
	m_resourceDesc.SetSampler(shaderSlot, sampler, shaderStages);
}

inline void CRenderPrimitive::SetConstantBuffer(uint32 shaderSlot, CConstantBuffer* pBuffer, EShaderStage shaderStages)
{
	m_resourceDesc.SetConstantBuffer(shaderSlot, pBuffer, shaderStages);
}

inline void CRenderPrimitive::SetBuffer(uint32 shaderSlot, CGpuBuffer* pBuffer, ResourceViewHandle resourceViewID, EShaderStage shaderStages)
{
	m_resourceDesc.SetBuffer(shaderSlot, pBuffer, resourceViewID, shaderStages);
}

///////////////////////////////////// Inline functions for CRenderPrimitiveRenderPass /////////////////////////////////////
inline void CPrimitiveRenderPass::SetRenderTarget(uint32 slot, CTexture* pRenderTarget, ResourceViewHandle hRenderTargetView)
{
	m_renderPassDesc.SetRenderTarget(slot, pRenderTarget, hRenderTargetView);
}

inline void CPrimitiveRenderPass::SetOutputUAV(uint32 slot, CGpuBuffer* pBuffer)
{
	m_renderPassDesc.SetOutputUAV(slot, pBuffer);
}

inline void CPrimitiveRenderPass::SetDepthTarget(CTexture* pDepthTarget, ResourceViewHandle hDepthTargetView)
{
	m_renderPassDesc.SetDepthTarget(pDepthTarget, hDepthTargetView);
}
