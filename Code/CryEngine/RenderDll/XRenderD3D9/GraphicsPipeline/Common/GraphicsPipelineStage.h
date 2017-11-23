// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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

struct SGraphicsPipelinePassContext
{
	SGraphicsPipelinePassContext()
	{
	}

	SGraphicsPipelinePassContext(CRenderView* renderView, CSceneRenderPass* pSceneRenderPass, EShaderTechniqueID technique, uint32 filter, uint32 excludeFilter)
		: pSceneRenderPass(pSceneRenderPass)
		, techniqueID(technique)
		, batchFilter(filter)
		, batchExcludeFilter(excludeFilter)
		, renderListId(EFSLIST_INVALID)
		, stageID(0)
		, passID(0)
		, pRenderView(renderView)
		, renderNearest(false)
		, pCommandList(nullptr)
		, pDrawCallInfoPerMesh(nullptr)
		, pDrawCallInfoPerNode(nullptr)
	{
	}

	SGraphicsPipelinePassContext& operator=(const SGraphicsPipelinePassContext& other)
	{
		memcpy(this, &other, sizeof(*this));
		return *this;
	}

	CSceneRenderPass*  pSceneRenderPass;
	EShaderTechniqueID techniqueID;
	uint32             batchFilter;
	uint32             batchExcludeFilter;

	ERenderListID      renderListId;

	// Stage ID of a scene stage (EStandardGraphicsPipelineStage)
	uint32 stageID;
	// Pass ID, in case a stage has several different scene passes
	uint32 passID;

	uint32 renderItemGroup;
	uint32 profilerSectionIndex;

	// rend items
	CRenderView* pRenderView;
	TRange<int>  rendItems;

	bool         renderNearest;

	// Output command list.
	CDeviceCommandList* pCommandList;

	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo>* pDrawCallInfoPerNode;
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo>* pDrawCallInfoPerMesh;
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
