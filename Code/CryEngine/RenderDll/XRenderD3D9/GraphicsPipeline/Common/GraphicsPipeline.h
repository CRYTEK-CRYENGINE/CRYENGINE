// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "GraphicsPipelineStage.h"
#include "GraphicsPipelineStateSet.h"
#include "UtilityPasses.h"
#include "RenderPassScheduler.h"

class CGraphicsPipelineStage;

class CGraphicsPipeline
{
private:
	struct SUtilityPassCache
	{
		uint32 numUsed = 0;
		std::vector<std::unique_ptr<IUtilityRenderPass>> utilityPasses;
	};

public:
	CGraphicsPipeline()
	{
		m_pipelineStages.fill(nullptr);
	}

	virtual ~CGraphicsPipeline()
	{
		// destroy stages in reverse order to satisfy data dependencies
		for (auto it = m_pipelineStages.rbegin(); it != m_pipelineStages.rend(); ++it)
		{
			SAFE_DELETE(*it);
		}
	}

	// Allocate resources needed by the pipeline and its stages
	virtual void            Init() = 0;
	// Prepare all stages before actual drawing starts
	virtual void            Prepare(CRenderView* pRenderView, EShaderRenderingFlags renderingFlags) = 0;
	// Execute the pipeline and its stages
	virtual void            Execute() = 0;

	CGraphicsPipelineStage* GetStage(uint32 stageID)
	{
		assert(stageID < m_pipelineStages.size());
		assert(m_pipelineStages[stageID] != nullptr);
		return m_pipelineStages[stageID];
	}

#if !defined(_RELEASE)
	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo>* GetDrawCallInfoPerNode() { return &m_drawCallInfoPerNode; }
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo>* GetDrawCallInfoPerMesh() { return &m_drawCallInfoPerMesh; }
#endif

	CRenderPassScheduler& GetRenderPassScheduler()
	{
		return m_renderPassScheduler;
	}

protected:
	template<class T> void RegisterStage(T*& pPipelineStage, uint32 stageID)
	{
		assert(stageID < m_pipelineStages.size());
		assert(m_pipelineStages[stageID] == nullptr);
		pPipelineStage = new T();
		pPipelineStage->m_stageID = stageID;
		m_pipelineStages[stageID] = pPipelineStage;
	}

	// Scene stages contain scene render passes that make use of the PSO cache.
	// Their stage ID is used to index into the PSO cache.
	template<class T, uint32 stageID> void RegisterSceneStage(T*& pPipelineStage)
	{
		static_assert(stageID < MAX_PIPELINE_SCENE_STAGES, "Invalid ID for scene stage");
		RegisterStage<T>(pPipelineStage, stageID);
	}

	void InitStages()
	{
		for (auto pStage : m_pipelineStages)
		{
			if (pStage)
			{
				pStage->Init();
			}
		}
	}

	template<class T> T* GetOrCreateUtilityPass()
	{
		IUtilityRenderPass::EPassId id = T::GetPassId();
		auto& cache = m_utilityPassCaches[uint32(id)];
		if (cache.numUsed >= cache.utilityPasses.size())
		{
			cache.utilityPasses.emplace_back(new T);
		}
		return static_cast<T*>(cache.utilityPasses[cache.numUsed++].get());
	}

	void ResetUtilityPassCache()
	{
		for (auto& it : m_utilityPassCaches)
		{
			it.numUsed = 0;
		}
	}

protected:
	std::array<CGraphicsPipelineStage*, 48> m_pipelineStages;
	CRenderPassScheduler                    m_renderPassScheduler;

#if !defined(_RELEASE)
	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo> m_drawCallInfoPerNode;
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo> m_drawCallInfoPerMesh;
#endif

private:
	std::array<SUtilityPassCache, uint32(IUtilityRenderPass::EPassId::MaxPassCount)> m_utilityPassCaches;
};
