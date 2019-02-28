// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CPrimitiveRenderPass;
class CFullscreenPass;
class CSceneRenderPass;
class CComputeRenderPass;


class CRenderPassScheduler
{
public:
	CRenderPassScheduler()
		: m_bEnabled(false)
	{
	}

	void SetEnabled(bool bEnabled)  { m_bEnabled = bEnabled; }
	bool IsActive()                 { return CRenderer::CV_r_GraphicsPipelinePassScheduler && m_bEnabled; }

	void AddPass(CPrimitiveRenderPass* pPass);
	void AddPass(CFullscreenPass* pFullscreenPass)  { AddPass((CPrimitiveRenderPass*)pFullscreenPass); }
	void AddPass(CSceneRenderPass* pScenePass);
	void AddPass(CComputeRenderPass* pComputePass);
	
	void Execute();

private:
	void InsertResourceTransitions(CPrimitiveRenderPass* pPass);

private:
	enum EResourceState
	{
		eState_Unknown = 0,
		eState_RenderTargetWrite,
		eState_RenderTargetRead,
	};

	enum EResourceTransition
	{
		eTransition_None = 0,
		eTransition_RenderTargetReadToWrite,
		eTransition_RenderTargetWriteToRead,
	};
	
	struct SResourceState
	{
		int16   passID = -1;
		uint16  state  = 0;
	};

	struct STransition
	{
		CTexture*            pRenderTarget;
		EResourceTransition  transition;
		uint16               nextUsagePass;

		STransition()
			: pRenderTarget(nullptr)
			, transition(eTransition_None)
			, nextUsagePass(0)
		{
		}
		
		STransition(CTexture* pRenderTarget, EResourceTransition transition, uint16 nextUsagePass)
			: pRenderTarget(pRenderTarget)
			, transition(transition)
			, nextUsagePass(nextUsagePass)
		{
		}
	};

	enum EPassType
	{
		ePassType_Primitive,
		ePassType_Scene,
		ePassType_Compute
	};
	
	struct SQueuedRenderPass
	{
		union
		{
			CPrimitiveRenderPass*  pPrimitivePass;
			CSceneRenderPass*      pScenePass;
			CComputeRenderPass*    pComputePass;
		};
		EPassType              passType;    
		uint32                 numTransitions;
		STransition            transitions[8];

		SQueuedRenderPass()
			: pPrimitivePass(nullptr)
			, numTransitions(0)
		{
		}
		
		SQueuedRenderPass(CPrimitiveRenderPass* pPrimitiveRenderPass)
			: pPrimitivePass(pPrimitiveRenderPass)
			, numTransitions(0)
		{
			passType = ePassType_Primitive;
		}

		SQueuedRenderPass(CSceneRenderPass* pSceneRenderPass)
			: pScenePass(pSceneRenderPass)
			, numTransitions(0)
		{
			passType = ePassType_Scene;
		}

		SQueuedRenderPass(CComputeRenderPass* pComputeRenderPass)
			: pComputePass(pComputeRenderPass)
			, numTransitions(0)
		{
			passType = ePassType_Compute;
		}
	};

private:
	bool                                 m_bEnabled;
	std::vector<SQueuedRenderPass>       m_renderPasses;
	std::map<CTexture*, SResourceState>  m_resourceStates;
};