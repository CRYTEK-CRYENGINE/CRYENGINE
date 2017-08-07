// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#pragma once
#include <array>
#include <atomic>

#include "RenderPassBase.h"

class CComputeRenderPass : public CRenderPassBase
{
public:
	enum EPassFlags
	{
		eFlags_None = 0,
		eFlags_ReflectConstantBuffersFromShader = BIT(0)
	};

	CComputeRenderPass(EPassFlags flags = eFlags_None);

#define ASSIGN_VALUE(dst, src, dirtyFlag) \
  if ((dst) != (src))                     \
    m_dirtyMask |= (dirtyFlag);           \
  (dst) = (src);

	bool InputChanged(int var0 = 0, int var1 = 0, int var2 = 0, int var3 = 0)
	{
		bool bChanged = IsDirty() || 
		                var0 != m_inputVars[0] || var1 != m_inputVars[1] ||
		                var2 != m_inputVars[2] || var3 != m_inputVars[3];

		if (bChanged)
		{
			m_inputVars[0] = var0;
			m_inputVars[1] = var1;
			m_inputVars[2] = var2;
			m_inputVars[3] = var3;
		}

		return bChanged;
	}

	bool IsDirty()
	{
		if (m_dirtyMask != eDirty_None)
			return true;

		if (m_currentPsoUpdateCount != m_pPipelineState->GetUpdateCount())
			return true;

		return false;
	}

	void SetOutputUAV(uint32 slot, CTexture* pTexture, ResourceViewHandle resourceViewID = EDefaultResourceViews::UnorderedAccess, ::EShaderStage shaderStages = EShaderStage_Compute)
	{
		m_dirtyMask |= (EDirtyFlags)m_resourceDesc.SetTexture(slot, pTexture, resourceViewID, shaderStages);
	}

	void SetOutputUAV(uint32 slot, const CGpuBuffer* pBuffer, ResourceViewHandle resourceViewID = EDefaultResourceViews::UnorderedAccess, ::EShaderStage shaderStages = EShaderStage_Compute)
	{
		m_dirtyMask |= (EDirtyFlags)m_resourceDesc.SetBuffer(slot, pBuffer, resourceViewID, shaderStages);
	}

	void SetDispatchSize(uint32 sizeX, uint32 sizeY, uint32 sizeZ)
	{
		m_dispatchSizeX = sizeX;
		m_dispatchSizeY = sizeY;
		m_dispatchSizeZ = sizeZ;
	}

	void SetTechnique(CShader* pShader, const CCryNameTSCRC& techName, uint64 rtMask)
	{
		ASSIGN_VALUE(m_pShader, pShader, eDirty_Technique);
		ASSIGN_VALUE(m_techniqueName, techName, eDirty_Technique);
		ASSIGN_VALUE(m_rtMask, rtMask, eDirty_Technique);

		if (m_pPipelineState && (!m_pPipelineState->IsValid() || m_currentPsoUpdateCount != m_pPipelineState->GetUpdateCount()))
		{
			m_dirtyMask |= eDirty_Technique;
		}
	}

	void SetFlags(EPassFlags flags)
	{
		ASSIGN_VALUE(m_flags, flags, eDirty_ResourceLayout);
	}

	void SetTexture(uint32 slot, CTexture* pTexture, ResourceViewHandle resourceViewID = EDefaultResourceViews::Default)
	{
		m_dirtyMask |= (EDirtyFlags)m_resourceDesc.SetTexture(slot, pTexture, resourceViewID, EShaderStage_Compute);
	}

	void SetTextureSamplerPair(uint32 slot, CTexture* pTex, SamplerStateHandle sampler, ResourceViewHandle resourceViewID = EDefaultResourceViews::Default)
	{
		m_dirtyMask |= (EDirtyFlags)m_resourceDesc.SetTexture(slot, pTex, resourceViewID, EShaderStage_Compute);
		m_dirtyMask |= (EDirtyFlags)m_resourceDesc.SetSampler(slot, sampler, EShaderStage_Compute);
	}

	void SetBuffer(uint32 slot, const CGpuBuffer* pBuffer)
	{
		m_dirtyMask |= (EDirtyFlags)m_resourceDesc.SetBuffer(slot, pBuffer, EDefaultResourceViews::Default, EShaderStage_Compute);
	}

	void SetSampler(uint32 slot, SamplerStateHandle sampler)
	{
		m_dirtyMask |= (EDirtyFlags)m_resourceDesc.SetSampler(slot, sampler, EShaderStage_Compute);
	}

	void SetInlineConstantBuffer(uint32 slot, CConstantBuffer* pConstantBuffer)
	{
		if (m_constantManager.SetTypedConstantBuffer(EConstantBufferShaderSlot(slot), pConstantBuffer, EShaderStage_Compute))
			m_dirtyMask |= eDirty_ResourceLayout;
	}

	void SetConstant(const CCryNameR& paramName, const Vec4 param)
	{
		CRY_ASSERT(m_bPendingConstantUpdate);
		m_constantManager.SetNamedConstant(paramName, param, eHWSC_Compute);
	}

	void SetConstantArray(const CCryNameR& paramName, const Vec4 params[], uint32 numParams)
	{
		CRY_ASSERT(m_bPendingConstantUpdate);
		m_constantManager.SetNamedConstantArray(paramName, params, numParams, eHWSC_Compute);
	}

	void PrepareResourcesForUse(CDeviceCommandListRef RESTRICT_REFERENCE commandList);

	void BeginConstantUpdate();

	void BeginRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void Dispatch(CDeviceCommandListRef RESTRICT_REFERENCE commandList, ::EShaderStage srvUsage);
	void EndRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList);

	// Executes this pass as if it's the only participant of a compute stage (see CSceneGBufferStage + CSceneRenderPass)
	void Execute(CDeviceCommandListRef RESTRICT_REFERENCE commandList, ::EShaderStage srvUsage = EShaderStage_Compute);

	void Reset();

protected:

#undef ASSIGN_VALUE

	static bool OnResourceInvalidated(void* pThis, uint32 flags);

private:
	enum EDirtyFlags
	{
		eDirty_ResourceLayout = BIT(0),
		eDirty_Resources      = BIT(1),
		eDirty_Technique      = BIT(2),

		eDirty_None           = 0,
		eDirty_All            = eDirty_Technique | eDirty_Resources | eDirty_ResourceLayout
	};

	static_assert(uint(eDirty_ResourceLayout) == uint(CDeviceResourceSetDesc::EDirtyFlags::eDirtyBindPoint), "eDirty_ResourceLayout needs to match CDeviceResourceSetDesc::EDirtyFlags::eDirtyBindPoint");
	static_assert(uint(eDirty_Resources)      == uint(CDeviceResourceSetDesc::EDirtyFlags::eDirtyBinding), "eDirty_Resources needs to match CDeviceResourceSetDesc::EDirtyFlags::eDirtyBinding");

	uint Compile();

	typedef SDeviceObjectHelpers::CShaderConstantManager ConstantManager;
private:

	int                      m_inputVars[4];

	EPassFlags               m_flags;
	uint32                   m_dirtyMask;
	std::atomic<bool>        m_bResourcesInvalidated;
	CShader*                 m_pShader;
	CCryNameTSCRC            m_techniqueName;
	uint64                   m_rtMask;
	uint32                   m_dispatchSizeX;
	uint32                   m_dispatchSizeY;
	uint32                   m_dispatchSizeZ;
	CDeviceResourceSetDesc   m_resourceDesc;
	CDeviceResourceSetPtr    m_pResourceSet;
	CDeviceResourceLayoutPtr m_pResourceLayout;
	CDeviceComputePSOPtr     m_pPipelineState;
	int                      m_currentPsoUpdateCount;

	ConstantManager          m_constantManager;

	bool                     m_bPendingConstantUpdate;
	bool                     m_bCompiled;

	SProfilingStats          m_profilingStats;
};
