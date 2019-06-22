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

	static void Initialize();
	static void Shutdown();

	// Called in Init()
	void SetFlags(EPassFlags flags) { m_passFlags = flags; }
	void SetDepthBias(float constBias, float slopeBias, float biasClamp);
	void SetPassResources(CDeviceResourceLayoutPtr pResourceLayout, CDeviceResourceSetPtr pPerPassResources);

	// Called in Update()
	void SetViewport(const D3DViewPort& viewport);
	void SetViewport(const SRenderViewport& viewport);
	void SetRenderTargets(CTexture* pDepthTarget, CTexture* pColorTarget0, CTexture* pColorTarget1 = NULL, CTexture* pColorTarget2 = NULL, CTexture* pColorTarget3 = NULL);

	// Called in Execute()
	void SetupDrawContext(uint32 stageID, uint32 stagePassID, EShaderTechniqueID technique, uint32 includeFilter, uint32 excludeFilter = 0);
	void ExchangeRenderTarget(uint32 slot, CTexture* pNewColorTarget, ResourceViewHandle hRenderTargetView = EDefaultResourceViews::RenderTarget);
	void ExchangeDepthTarget(CTexture* pNewDepthTarget, ResourceViewHandle hDepthStencilView = EDefaultResourceViews::DepthStencil);

	void BeginExecution(CGraphicsPipeline& activeGraphicsPipeline);
	void EndExecution();
	void Execute();

	void DrawRenderItems(CRenderView* pRenderView, ERenderListID list, int listStart = 0, int listEnd = 0x7FFFFFFF);

	// Called from rendering backend (has to be threadsafe)
	void                       PrepareRenderPassForUse(CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void                       BeginRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest) const;
	void                       EndRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest) const;

	void                       ResolvePass(CGraphicsPipeline& graphicsPipeline, CDeviceCommandListRef RESTRICT_REFERENCE commandList, const std::vector<TRect_tpl<uint16>>& screenBounds) const;

	uint32                     GetStageID()             const { return m_stageID; }
	uint32                     GetPassID()              const { return m_passID; }
	uint32                     GetNumRenderItemGroups() const { return m_numRenderItemGroups; }
	EPassFlags                 GetFlags()               const { return m_passFlags; }
	const D3DViewPort&         GetViewport(bool n)      const { return m_viewPort[n]; }
	const D3DRectangle&        GetScissorRect()         const { return m_scissorRect; }
	
	CDeviceResourceLayoutPtr   GetResourceLayout() const      { return m_pResourceLayout; }
	const CDeviceRenderPassPtr GetRenderPass()     const      { return m_pRenderPass; }

public:
	static bool FillCommonScenePassStates(const SGraphicsPipelineStateDescription& inputDesc, CDeviceGraphicsPSODesc& psoDesc, CVrProjectionManager* pVRProjectionManager);	
	
	static const CDeviceResourceSetDesc& GetDefaultMaterialBindPoints()      { return *s_pDefaultMaterialBindPoints; }
	static const CDeviceResourceSetDesc& GetDefaultDrawExtraResourceLayout() { return *s_pDefaultDrawExtraRL; }
	static       CDeviceResourceSetPtr   GetDefaulDrawExtraResourceSet()     { return  s_pDefaultDrawExtraRS; }

protected:
	static bool OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe;

private:
	void DrawOpaqueRenderItems(SGraphicsPipelinePassContext& passContext, CRenderView* pRenderView, ERenderListID list, int listStart, int listEnd);
	void DrawTransparentRenderItems(SGraphicsPipelinePassContext& passContext, CRenderView* pRenderView, ERenderListID list, int listStart, int listEnd);

protected:
	CDeviceRenderPassDesc                     m_renderPassDesc;
	CDeviceRenderPassPtr                      m_pRenderPass;
	D3DViewPort                               m_viewPort[2];
	D3DRectangle                              m_scissorRect;
	CDeviceResourceLayoutPtr                  m_pResourceLayout;
	CDeviceResourceSetPtr                     m_pPerPassResourceSet;

	EShaderTechniqueID                        m_technique;
	uint32                                    m_stageID : 16;
	uint32                                    m_passID  : 16;
	uint32                                    m_batchIncludeFilter;
	uint32                                    m_batchExcludeFilter;
	EPassFlags                                m_passFlags;

	uint32                                    m_numRenderItemGroups;

	float                                     m_depthConstBias = 0.0f;
	float                                     m_depthSlopeBias = 0.0f;
	float                                     m_depthBiasClamp = 0.0f;

	std::vector<SGraphicsPipelinePassContext> m_passContexts;

protected:
	static int                                s_recursionCounter;            // For asserting Begin/EndExecution get called on pass

private:
	static CDeviceResourceSetDesc*            s_pDefaultMaterialBindPoints;
	static CDeviceResourceSetDesc*            s_pDefaultDrawExtraRL;
	static CDeviceResourceSetPtr              s_pDefaultDrawExtraRS;
};

DEFINE_ENUM_FLAG_OPERATORS(CSceneRenderPass::EPassFlags)
