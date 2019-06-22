// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Range.h>

class CCVarUpdateRecorder;
class CGraphicsPipeline;
class CGraphicsPipelineResources;
class CGraphicsPipelineStage;
class CGraphicsPipelineStateLocalCache;
class CRenderOutput;
class CRenderView;
class CSceneRenderPass;
class CStandardGraphicsPipeline;
class CMinimumGraphicsPipeline;
class CBillboardGraphicsPipeline;
class CMobileGraphicsPipeline;

struct SRenderViewInfo;

enum class GraphicsPipelinePassType : std::uint8_t
{
	renderPass,
	resolve,
};

struct SGraphicsPipelinePassContext
{
	SGraphicsPipelinePassContext() = default;

	SGraphicsPipelinePassContext(CRenderView* renderView, CSceneRenderPass* pSceneRenderPass, EShaderTechniqueID technique, uint32 includeFilter, uint32 excludeFilter)
		: pSceneRenderPass(pSceneRenderPass)
		, batchIncludeFilter(includeFilter)
		, batchExcludeFilter(excludeFilter)
		, pRenderView(renderView)
		, techniqueID(technique)
	{
	}

	SGraphicsPipelinePassContext(GraphicsPipelinePassType type, CRenderView* renderView, CSceneRenderPass* pSceneRenderPass)
		: type(type), pSceneRenderPass(pSceneRenderPass), pRenderView(renderView)
	{}

	GraphicsPipelinePassType type = GraphicsPipelinePassType::renderPass;

	CSceneRenderPass*        pSceneRenderPass;

	uint32                   batchIncludeFilter;
	uint32                   batchExcludeFilter;

	ERenderListID            renderListId = EFSLIST_INVALID;
#if defined(ENABLE_PROFILING_CODE)
	ERenderListID            recordListId = EFSLIST_INVALID;
#endif

	// Stage ID of a scene stage (EStandardGraphicsPipelineStage)
	uint32      stageID = 0;
	// Pass ID, in case a stage has several different scene passes
	uint32      passID = 0;

	std::string groupLabel;
	uint32      groupIndex;

#if defined(ENABLE_SIMPLE_GPU_TIMERS)
	uint32 profilerSectionIndex;
#endif

	// rend items
	CRenderView* pRenderView;

	std::vector<TRect_tpl<uint16>> resolveScreenBounds;

	// Uses the range if rendItems is empty, otherwise uses the rendItems vector
	TRange<int>        rendItems;

	bool               renderNearest = false;
	EShaderTechniqueID techniqueID;

#if defined(DO_RENDERSTATS)
	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo>* pDrawCallInfoPerNode = nullptr;
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo>* pDrawCallInfoPerMesh = nullptr;
#endif
};

class CGraphicsPipelineStage
{
public:
	CGraphicsPipelineStage(CGraphicsPipeline& graphicsPipeline);
	virtual ~CGraphicsPipelineStage() {}

	// Allocate resources needed by the graphics pipeline stage
	virtual void Init() {}
	// Change internal values and resources when the render-resolution has changed (or the underlying resource dimensions)
	// If the resources are considering sub-view/rectangle support then interpret CRendererResources::s_resourceWidth/Height
	virtual void Resize(int renderWidth, int renderHeight) {}
	// Update stage before actual rendering starts (called every "RenderScene")
	virtual void Update()                                  {}

	// Reset any cvar dependent states.
	virtual void OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater) {}

	// If this stage should be updated based on the given flags
	virtual bool IsStageActive(EShaderRenderingFlags flags) const { return true; }

	virtual void SetRenderView(CRenderView* pRenderView) { m_pRenderView = pRenderView; }

public:
	CRenderView*                RenderView() const { return m_pRenderView; }
	const SRenderViewport&      GetViewport() const;

	const SRenderViewInfo&      GetCurrentViewInfo() const;

	const CRenderOutput&        GetRenderOutput() const;
	CRenderOutput&              GetRenderOutput();

	CGraphicsPipeline&          m_graphicsPipeline;
	CGraphicsPipelineResources& m_graphicsPipelineResources;

protected:
	void ClearDeviceOutputState();

protected:
	friend class CGraphicsPipeline;

	CRenderView* m_pRenderView = nullptr;

};
