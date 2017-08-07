// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Range.h>

// forward declarations
typedef std::shared_ptr<CGraphicsPipelineStateLocalCache> CGraphicsPipelineStateLocalCachePtr;
class CGraphicsPipelineStage;
class CSceneRenderPass;
class CRenderView;
class CCVarUpdateRecorder;

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
	uint32             stageID;
	// Pass ID, in case a stage has several different scene passes
	uint32             passID;

	uint32             renderItemGroup;
	uint32             profilerSectionIndex;

	// rend items
	CRenderView*       pRenderView;
	TRange<int>        rendItems;

	bool               renderNearest;

	// Output command list.
	CDeviceCommandList* pCommandList;

	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo> *pDrawCallInfoPerNode;
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo> *pDrawCallInfoPerMesh;
};

class CGraphicsPipelineStage
{
public:
	virtual ~CGraphicsPipelineStage() {}

	// Allocate resources needed by the graphics pipeline stage
	virtual void Init()                            {}
	// Prepare stage before actual rendering starts (called every frame)
	virtual void Prepare(CRenderView* pRenderView) {}

	// Reset any cvar dependent states.
	virtual void OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater) {}

protected:
	// Helpers.
	static CRenderView* RenderView();

protected:
	friend class CGraphicsPipeline;

	uint32 m_stageID;
};
