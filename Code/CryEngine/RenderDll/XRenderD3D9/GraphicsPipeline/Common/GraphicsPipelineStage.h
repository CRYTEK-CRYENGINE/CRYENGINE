// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Range.h>

// forward declarations
class CGraphicsPipelineStage;
class CGraphicsPipelineStateLocalCache;
class CStandardGraphicsPipeline;
class CSceneRenderPass;
class CRenderView;
class CCVarUpdateRecorder;
struct SRenderViewInfo;
class CGraphicsPipeline;
class CRenderOutput;

enum class GraphicsPipelinePassType : std::uint8_t
{
	renderPass,
	resolve,
};

struct SGraphicsPipelinePassContext
{
	SGraphicsPipelinePassContext() = default;

	SGraphicsPipelinePassContext(CRenderView* renderView, CSceneRenderPass* pSceneRenderPass, EShaderTechniqueID technique, uint32 filter, uint32 excludeFilter)
		: pSceneRenderPass(pSceneRenderPass)
		, batchFilter(filter)
		, batchExcludeFilter(excludeFilter)
		, pRenderView(renderView)
		, techniqueID(technique)
	{

		;
	}

	SGraphicsPipelinePassContext(GraphicsPipelinePassType type, CRenderView* renderView, CSceneRenderPass* pSceneRenderPass) 
		: type(type), pSceneRenderPass(pSceneRenderPass), pRenderView(renderView)
	{}

	GraphicsPipelinePassType type = GraphicsPipelinePassType::renderPass;

	CSceneRenderPass*  pSceneRenderPass;

	uint32             batchFilter;
	uint32             batchExcludeFilter;

	ERenderListID      renderListId = EFSLIST_INVALID;

	// Stage ID of a scene stage (EStandardGraphicsPipelineStage)
	uint32 stageID = 0;
	// Pass ID, in case a stage has several different scene passes
	uint32 passID = 0;

	uint32 renderItemGroup;
	uint32 profilerSectionIndex;

	// rend items
	CRenderView* pRenderView;

	std::vector<TRect_tpl<uint16>> resolveScreenBounds;

	// Uses the range if rendItems is empty, otherwise uses the rendItems vector
	TRange<int>        rendItems;

	bool               renderNearest = false;
	EShaderTechniqueID techniqueID;

	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo>* pDrawCallInfoPerNode = nullptr;
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo>* pDrawCallInfoPerMesh = nullptr;
};

class CGraphicsPipelineStage
{
public:
	virtual ~CGraphicsPipelineStage() {}

	// Allocate resources needed by the graphics pipeline stage
	virtual void Init()                                    {}
	// Change internal values and resources when the render-resolution has changed (or the underlying resource dimensions)
	// If the resources are considering sub-view/rectangle support then interpret CRendererResources::s_resourceWidth/Height
	virtual void Resize(int renderWidth, int renderHeight) {}
	// Update stage before actual rendering starts (called every "RenderScene")
	virtual void Update()                                  {}

	// Reset any cvar dependent states.
	virtual void OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater) {}

	// If this stage should be updated based on the given flags
	virtual bool IsStageActive(EShaderRenderingFlags flags) const { return true; }

public:
	void                             SetRenderView(CRenderView* pRenderView) { m_pRenderView = pRenderView; }

	CRenderView*                     RenderView() const { return m_pRenderView; }
	const SRenderViewport&           GetViewport() const;

	const SRenderViewInfo&           GetCurrentViewInfo() const;

	CGraphicsPipeline&               GetGraphicsPipeline()       { assert(m_pGraphicsPipeline); return *m_pGraphicsPipeline; }
	const CGraphicsPipeline&         GetGraphicsPipeline() const { assert(m_pGraphicsPipeline); return *m_pGraphicsPipeline; }

	CStandardGraphicsPipeline&       GetStdGraphicsPipeline();
	const CStandardGraphicsPipeline& GetStdGraphicsPipeline() const;

	const CRenderOutput&             GetRenderOutput() const;
	CRenderOutput&                   GetRenderOutput();

protected:
	void                             ClearDeviceOutputState();

protected:
	friend class CGraphicsPipeline;

	uint32             m_stageID;

private:
	CGraphicsPipeline* m_pGraphicsPipeline = nullptr;
	CRenderView*       m_pRenderView = nullptr;
};
