// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RenderPassBase.h"

struct VrProjectionInfo;

class CSceneRenderPass : public CRenderPassBase
{
	friend class CRenderPassScheduler;

public:
	enum EPassFlags
	{
		ePassFlags_None                         = 0,
		ePassFlags_RenderNearest                = BIT(0),
		ePassFlags_ReverseDepth                 = BIT(1),
		ePassFlags_UseVrProjectionState         = BIT(2),
		ePassFlags_RequireVrProjectionConstants = BIT(3),
		ePassFlags_VrProjectionPass             = ePassFlags_UseVrProjectionState | ePassFlags_RequireVrProjectionConstants // for convenience
	};

	CSceneRenderPass();

	void SetupPassContext(uint32 stageID, uint32 stagePassID, EShaderTechniqueID technique, uint32 filter, ERenderListID renderList = EFSLIST_GENERAL, uint32 excludeFilter = 0, bool drawCompiledRenderObject = true);
	void SetPassResources(CDeviceResourceLayoutPtr pResourceLayout, CDeviceResourceSetPtr pPerPassResources);
	void SetRenderTargets(CTexture* pDepthTarget, CTexture* pColorTarget0, CTexture* pColorTarget1 = NULL, CTexture* pColorTarget2 = NULL, CTexture* pColorTarget3 = NULL);
	void ExchangeRenderTarget(uint32 slot, CTexture* pNewColorTarget, ResourceViewHandle hRenderTargetView = EDefaultResourceViews::RenderTarget);
	void ExchangeDepthTarget(CTexture* pNewDepthTarget, ResourceViewHandle hDepthStencilView = EDefaultResourceViews::DepthStencil);
	void SetFlags(EPassFlags flags)  { m_passFlags = flags; }
	void SetViewport(const D3DViewPort& viewport);
	void SetViewport(const SRenderViewport& viewport);
	void SetDepthBias(float constBias, float slopeBias, float biasClamp);

	void BeginExecution();
	void EndExecution();
	void Execute();

	void DrawRenderItems(CRenderView* pRenderView, ERenderListID list, int listStart = -1, int listEnd = -1);
	void DrawTransparentRenderItems(CRenderView* pRenderView, ERenderListID list);

	// Called from rendering backend (has to be threadsafe)
	void                PrepareRenderPassForUse(CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void                BeginRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest) const;
	void                EndRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest) const;

	void                ResolvePass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, const std::vector<TRect_tpl<uint16>>& screenBounds) const;

	uint32              GetStageID()             const { return m_stageID; }
	uint32              GetPassID()              const { return m_passID; }
	uint32              GetNumRenderItemGroups() const { return m_numRenderItemGroups; }
	EPassFlags          GetFlags()               const { return m_passFlags; }
	const D3DViewPort&  GetViewport(bool n)      const { return m_viewPort[n]; }
	const D3DRectangle& GetScissorRect()         const { return m_scissorRect; }
	
	CDeviceResourceLayoutPtr   GetResourceLayout() const { return m_pResourceLayout; }
	const CDeviceRenderPassPtr GetRenderPass()     const { return m_pRenderPass; }
	ERenderListID GetRenderList()                  const { return m_renderList; }

protected:
	static bool OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe;

protected:
	CDeviceRenderPassDesc    m_renderPassDesc;
	CDeviceRenderPassPtr     m_pRenderPass;
	D3DViewPort              m_viewPort[2];
	D3DRectangle             m_scissorRect;
	CDeviceResourceLayoutPtr m_pResourceLayout;
	CDeviceResourceSetPtr    m_pPerPassResourceSet;

	EShaderTechniqueID       m_technique;
	uint32                   m_stageID : 16;
	uint32                   m_passID  : 16;
	uint32                   m_batchFilter;
	uint32                   m_excludeFilter;
	EPassFlags               m_passFlags;
	ERenderListID            m_renderList;

	uint32                   m_numRenderItemGroups;
	uint32                   m_profilerSectionIndex;

	float                    m_depthConstBias;
	float                    m_depthSlopeBias;
	float                    m_depthBiasClamp;

	std::vector<SGraphicsPipelinePassContext> m_passContexts;

protected:
	static int               s_recursionCounter;  // For asserting Begin/EndExecution get called on pass
};

DEFINE_ENUM_FLAG_OPERATORS(CSceneRenderPass::EPassFlags)
