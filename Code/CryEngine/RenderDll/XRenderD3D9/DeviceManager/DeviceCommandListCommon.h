// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DeviceResourceSet.h"

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
	void SetDepthBounds(float fMin, float fMax);
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

	void ClearUAV(D3DUAV* pView, const ColorF& Values, UINT NumRects = 0, const D3D11_RECT* pRects = nullptr);
	void ClearUAV(D3DUAV* pView, const ColorI& Values, UINT NumRects = 0, const D3D11_RECT* pRects = nullptr);
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
	void Copy(CDeviceBuffer*  pSrc, D3DBuffer*      pDst, const SResourceRegionMapping& regionMapping);
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
