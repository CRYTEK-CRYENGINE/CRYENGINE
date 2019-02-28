// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/RenderView.h"
#include "GraphicsPipelineStage.h"
#include "GraphicsPipelineStateSet.h"
#include "UtilityPasses.h"
#include "RenderPassScheduler.h"

class CGraphicsPipelineStage;
struct SRenderViewInfo;

class CGraphicsPipeline
{
public:
	enum class EPipelineFlags : uint64
	{
		NO_SHADER_FOG = BIT(0),
	};

private:
	struct SUtilityPassCache
	{
		uint32 numUsed = 0;
		std::vector<std::unique_ptr<IUtilityRenderPass>> utilityPasses;
	};

public:
	CGraphicsPipeline();

	virtual ~CGraphicsPipeline();

	// Convention for used verbs:
	//
	//  Get........(): mostly const functions to retreive resource-references
	//  Generate...(): CPU side data generation (may also update upload buffers or GPU side buffers)
	//                 This could produce Copy() commands on a command-list (context free from the core CL)
	//  Prepare....(): Put all the resources used by the corresponding Execute() into the desired state
	//                 This produces a lot of barriers and other commands on the core CL or maybe compute CLs
	//  Execute....(): Recording of the GPU side commands on the core CL or maybe compute CLs

	// Allocate resources needed by the pipeline and its stages
	virtual void Init() = 0;
	virtual void Resize(int renderWidth, int renderHeight);
	// Prepare all stages before actual drawing starts
	virtual void Update(CRenderView* pRenderView, EShaderRenderingFlags renderingFlags);
	virtual void OnCVarsChanged(CCVarUpdateRecorder& rCVarRecs);
	// Execute the pipeline and its stages
	virtual void Execute() = 0;

	virtual void ShutDown();

	virtual size_t GetViewInfoCount() const                      { return 0; };
	virtual size_t GenerateViewInfo(SRenderViewInfo viewInfo[2]) { return 0; };

public:
	// Helper methods

	CGraphicsPipelineStage* GetStage(uint32 stageID)
	{
		assert(stageID < m_pipelineStages.size());
		assert(m_pipelineStages[stageID] != nullptr);
		return m_pipelineStages[stageID];
	}

	CRenderPassScheduler& GetRenderPassScheduler() { return m_renderPassScheduler; }

	void           SetCurrentRenderView(CRenderView* pRenderView);
	CRenderView*   GetCurrentRenderView()    const { return m_pCurrentRenderView; }
	CRenderOutput* GetCurrentRenderOutput()  const { return m_pCurrentRenderView->GetRenderOutput(); }

	//! Set current pipeline flags
	void           SetPipelineFlags(EPipelineFlags pipelineFlags)           { m_pipelineFlags = pipelineFlags; }
	//! Get current pipeline flags
	EPipelineFlags GetPipelineFlags() const                                 { return m_pipelineFlags; }
	bool           IsPipelineFlag(EPipelineFlags pipelineFlagsToTest) const { return 0 != ((uint64)m_pipelineFlags & (uint64)pipelineFlagsToTest); }

	// Animation time is used for rendering animation effects and can be paused if CRenderer::m_bPauseTimer is true
	CTimeValue GetAnimationTime() const { return gRenDev->GetAnimationTime(); }

	// Time of the last main to renderer thread sync
	CTimeValue GetFrameSyncTime() const { return gRenDev->GetFrameSyncTime(); }

#if defined(DO_RENDERSTATS)
	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo>* GetDrawCallInfoPerNode() { return &m_drawCallInfoPerNode; }
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo>* GetDrawCallInfoPerMesh() { return &m_drawCallInfoPerMesh; }
#endif

	void ClearState();
	void ClearDeviceState();

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

protected:
	template<class T> void RegisterStage(T*& pPipelineStage, uint32 stageID)
	{
		assert(stageID < m_pipelineStages.size());
		assert(m_pipelineStages[stageID] == nullptr);
		pPipelineStage = new T();
		pPipelineStage->m_pGraphicsPipeline = this;
		pPipelineStage->m_stageID = stageID;
		m_pipelineStages[stageID] = pPipelineStage;
	}

	// Scene stages contain scene render passes that make use of the PSO cache.
	// Their stage ID is used to index into the PSO cache.
	template<class T, uint32 stageID> void RegisterSceneStage(T*& pPipelineStage)
	{
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

	void ResizeStages(int renderWidth, int renderHeight)
	{
		for (auto pStage : m_pipelineStages)
		{
			if (pStage)
			{
				pStage->Resize(renderWidth, renderHeight);
			}
		}
	}

	void ResetUtilityPassCache()
	{
		for (auto& it : m_utilityPassCaches)
		{
			it.numUsed = 0;
			it.utilityPasses.clear();
		}
	}

protected:
	std::array<CGraphicsPipelineStage*, 48> m_pipelineStages;
	CRenderPassScheduler                    m_renderPassScheduler;

#if defined(DO_RENDERSTATS)
	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo> m_drawCallInfoPerNode;
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo> m_drawCallInfoPerMesh;
#endif

private:
	std::array<SUtilityPassCache, uint32(IUtilityRenderPass::EPassId::MaxPassCount)> m_utilityPassCaches;

	CRenderView*   m_pCurrentRenderView = nullptr;

	//! @see EPipelineFlags
	EPipelineFlags m_pipelineFlags;
};

DEFINE_ENUM_FLAG_OPERATORS(CGraphicsPipeline::EPipelineFlags)
