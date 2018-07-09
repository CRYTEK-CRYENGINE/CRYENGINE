// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "XRenderD3D9/DX12/API/DX12CommandList.hpp"

class CDeviceGraphicsCommandInterfaceImpl;
class CDeviceComputeCommandInterfaceImpl;
class CDeviceNvidiaCommandInterfaceImpl;

class CDeviceCopyCommandInterfaceImpl;
class CDeviceRenderPass;

struct SEmpty {};

struct SSharedState
{
	DX12_PTR(NCryDX12::CCommandList) pCommandList;
};

struct SCustomGraphicsState
{
	SCachedValue<D3D12_PRIMITIVE_TOPOLOGY> primitiveTopology;
};

class CDeviceCommandListImpl : public CDeviceCommandListCommon<SSharedState, SCustomGraphicsState, SEmpty>
{
public:
	inline CDeviceGraphicsCommandInterfaceImpl* GetGraphicsInterfaceImpl()
	{
		return reinterpret_cast<CDeviceGraphicsCommandInterfaceImpl*>(this);
	}

	inline CDeviceComputeCommandInterfaceImpl* GetComputeInterfaceImpl()
	{
		return reinterpret_cast<CDeviceComputeCommandInterfaceImpl*>(this);
	}

	inline CDeviceNvidiaCommandInterfaceImpl* GetNvidiaCommandInterfaceImpl()
	{
		return nullptr;
	}

	inline CDeviceCopyCommandInterfaceImpl* GetCopyInterfaceImpl()
	{
		return reinterpret_cast<CDeviceCopyCommandInterfaceImpl*>(this);
	}

	void SetProfilerMarker(const char* label);
	void BeginProfilerEvent(const char* label);
	void EndProfilerEvent(const char* label);

	void CeaseCommandListEvent(int nPoolId);
	void ResumeCommandListEvent(int nPoolId);

	// Helper functions for DX12
	NCryDX12::CCommandList* GetDX12CommandList() const { return m_sharedState.pCommandList; }

protected:
	void ClearStateImpl(bool bOutputMergerOnly) const;

	void ResetImpl();
	void LockToThreadImpl();
	void CloseImpl();

private:
	DynArray<const char*> m_profilerEventStack;
};

class CDeviceGraphicsCommandInterfaceImpl : public CDeviceCommandListImpl
{
protected:
	void PrepareUAVsForUseImpl(uint32 viewCount, CGpuBuffer** pViews, bool bCompute) const;
	void PrepareRenderPassForUseImpl(CDeviceRenderPass& renderPass) const;
	void PrepareResourceForUseImpl(uint32 bindSlot, CTexture* pTexture, const ResourceViewHandle TextureView, ::EShaderStage srvUsage) const;
	void PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const;
	void PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const;
	void PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages) const;
	void PrepareVertexBuffersForUseImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams) const;
	void PrepareIndexBufferForUseImpl(const CDeviceInputStream* indexStream) const;
	void BeginResourceTransitionsImpl(uint32 numTextures, CTexture** pTextures, EResourceTransitionType type);

	void BeginRenderPassImpl(const CDeviceRenderPass& renderPass, const D3DRectangle& renderArea);
	void EndRenderPassImpl(const CDeviceRenderPass& renderPass) {}
	void SetViewportsImpl(uint32 vpCount, const D3DViewPort* pViewports);
	void SetScissorRectsImpl(uint32 rcCount, const D3DRectangle* pRects);
	void SetPipelineStateImpl(const CDeviceGraphicsPSO* pDevicePSO);
	void SetResourceLayoutImpl(const CDeviceResourceLayout* pResourceLayout);
	void SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources);
	void SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass);
	void SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages);
	void SetVertexBuffersImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams);
	void SetIndexBufferImpl(const CDeviceInputStream* indexStream);       // NOTE: Take care with PSO strip cut/restart value and 32/16 bit indices
	void SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants);
	void SetStencilRefImpl(uint8 stencilRefValue);
	void SetDepthBiasImpl(float constBias, float slopeBias, float biasClamp);
	void SetDepthBoundsImpl(float fMin, float fMax);

	void DrawImpl(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation);
	void DrawIndexedImpl(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation);

	void ClearSurfaceImpl(D3DSurface* pView, const FLOAT Color[4], UINT NumRects, const D3D11_RECT* pRects);
	void ClearSurfaceImpl(D3DDepthSurface* pView, int clearFlags, float depth, uint8 stencil, uint32 numRects, const D3D11_RECT* pRects);

	void BeginOcclusionQueryImpl(D3DOcclusionQuery* pQuery);
	void EndOcclusionQueryImpl(D3DOcclusionQuery* pQuery);
};

class CDeviceComputeCommandInterfaceImpl : public CDeviceCommandListImpl
{
protected:
	void PrepareUAVsForUseImpl(uint32 viewCount, CGpuBuffer** pViews) const;
	void PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const;
	void PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlots, ::EShaderStage shaderStages) const;

	void SetPipelineStateImpl(const CDeviceComputePSO* pDevicePSO);
	void SetResourceLayoutImpl(const CDeviceResourceLayout* pResourceLayout);
	void SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources);
	void SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot);
	void SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants);

	void DispatchImpl(uint32 X, uint32 Y, uint32 Z);

	void ClearUAVImpl(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects);
	void ClearUAVImpl(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects);
};

class CDeviceCopyCommandInterfaceImpl : public CDeviceCommandListImpl
{
protected:
	void CopyImpl(CDeviceBuffer*  pSrc, CDeviceBuffer*  pDst);
	void CopyImpl(D3DBuffer*      pSrc, D3DBuffer*      pDst);
	void CopyImpl(CDeviceTexture* pSrc, CDeviceTexture* pDst);
	void CopyImpl(CDeviceTexture* pSrc, D3DTexture*     pDst);
	void CopyImpl(D3DTexture*     pSrc, D3DTexture*     pDst);
	void CopyImpl(D3DTexture*     pSrc, CDeviceTexture* pDst);

	void CopyImpl(CDeviceBuffer*  pSrc, CDeviceBuffer*  pDst, const SResourceRegionMapping& regionMapping);
	void CopyImpl(D3DBuffer*      pSrc, D3DBuffer*      pDst, const SResourceRegionMapping& regionMapping);
	void CopyImpl(CDeviceTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& regionMapping);
	void CopyImpl(D3DTexture*     pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& regionMapping);

	void CopyImpl(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryAlignment& memoryLayout);
	void CopyImpl(const void* pSrc, CDeviceBuffer*   pDst, const SResourceMemoryAlignment& memoryLayout);
	void CopyImpl(const void* pSrc, CDeviceTexture*  pDst, const SResourceMemoryAlignment& memoryLayout);

	void CopyImpl(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryMapping& memoryMapping);
	void CopyImpl(const void* pSrc, CDeviceBuffer*   pDst, const SResourceMemoryMapping& memoryMapping);
	void CopyImpl(const void* pSrc, CDeviceTexture*  pDst, const SResourceMemoryMapping& memoryMapping);

	void CopyImpl(CDeviceBuffer*  pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout);
	void CopyImpl(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout);

	void CopyImpl(CDeviceBuffer*  pSrc, void* pDst, const SResourceMemoryMapping& memoryMapping);
	void CopyImpl(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryMapping& memoryMapping);
};

class CDeviceNvidiaCommandInterfaceImpl : public CDeviceCommandListImpl
{
};