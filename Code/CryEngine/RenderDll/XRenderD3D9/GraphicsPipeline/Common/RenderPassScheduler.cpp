// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"

#include "FullscreenPass.h"
#include "ComputeRenderPass.h"
#include "SceneRenderPass.h"


void CRenderPassScheduler::AddPass(CSceneRenderPass* pScenePass)
{
	for (uint32 i = 0; i < m_renderPasses.size(); i++)
	{
		assert(m_renderPasses[i].pScenePass != pScenePass);
	}

	m_renderPasses.push_back(SQueuedRenderPass(pScenePass));
}


void CRenderPassScheduler::AddPass(CComputeRenderPass* pComputePass)
{
	for (uint32 i = 0; i < m_renderPasses.size(); i++)
	{
		assert(m_renderPasses[i].pComputePass != pComputePass);
	}

	m_renderPasses.push_back(SQueuedRenderPass(pComputePass));
}


void CRenderPassScheduler::AddPass(CPrimitiveRenderPass* pPass)
{
	for (uint32 i = 0; i < m_renderPasses.size(); i++)
	{
		assert(m_renderPasses[i].pPrimitivePass != pPass);
	}
	
	m_renderPasses.push_back(SQueuedRenderPass(pPass));

	InsertResourceTransitions(pPass);
}


void CRenderPassScheduler::InsertResourceTransitions(CPrimitiveRenderPass* pPass)
{
	// TODO: Function is just a pure POF at the moment
	
	uint32 curRenderPassId = (int)m_renderPasses.size() - 1;

	for (uint32 i = 0; i < CDeviceRenderPassDesc::MaxRendertargetCount; i++)
	{
		CTexture* pTex = pPass->GetRenderTarget(i);
		if (pTex)
		{
			SResourceState& resState = m_resourceStates[pTex];
			if (resState.passID != -1 && resState.state != eState_RenderTargetWrite)
			{
				SQueuedRenderPass& renderPass = m_renderPasses[resState.passID];
				renderPass.transitions[renderPass.numTransitions++] = STransition(pTex, eTransition_RenderTargetReadToWrite, curRenderPassId);
				assert(renderPass.numTransitions < CRY_ARRAY_COUNT(renderPass.transitions));
			}
			resState.passID = curRenderPassId;
			resState.state = eState_RenderTargetWrite;
		}
	}

	for (uint32 i = 0; i < pPass->m_compiledPrimitives.size(); i++)
	{
		CRenderPrimitive* pPrimitive = reinterpret_cast<CRenderPrimitive*>(pPass->m_compiledPrimitives[i]);

		for (const auto& it : pPrimitive->GetResourceDesc().GetResources())
		{
			const SResourceBinding& resource = it.second;
			if (resource.type == SResourceBinding::EResourceType::Texture)
			{
				CRY_ASSERT(resource.pTexture);
				SResourceState& resState = m_resourceStates[resource.pTexture];
				if (resState.passID != -1 && resState.state != eState_RenderTargetRead)
				{
					SQueuedRenderPass& renderPass = m_renderPasses[resState.passID];
					renderPass.transitions[renderPass.numTransitions++] = STransition(resource.pTexture, eTransition_RenderTargetWriteToRead, curRenderPassId);
					assert(renderPass.numTransitions < CRY_ARRAY_COUNT(renderPass.transitions));
				}
				resState.passID = curRenderPassId;
				resState.state = eState_RenderTargetRead;
			}
		}
	}
}


void CRenderPassScheduler::Execute()
{
	if (m_renderPasses.empty())
		return;
	
	PROFILE_LABEL_SCOPE("PASS_SCHEDULER");
	
	assert(m_bEnabled == false);

	stack_string prevLabel;
	
	// Submit passes
	for (auto& pass : m_renderPasses)
	{
		//for (uint32 i = 0; i < pass.numTransitions; i++)
		//{
		//	PROFILE_LABEL_SCOPE(pass.transitions[i].pRenderTarget->GetName());
		//}
		
		if (pass.passType == ePassType_Primitive)
		{
			if (pass.pPrimitivePass->GetLabel()[0] != '\0')
			{
				if (!prevLabel.empty())
				{
					PROFILE_LABEL_POP(prevLabel.c_str());
				}

				prevLabel = pass.pPrimitivePass->GetLabel();
				PROFILE_LABEL_PUSH(prevLabel.c_str());
			}
			pass.pPrimitivePass->Execute();
		}
		else if (pass.passType == ePassType_Scene)
		{
			if (!prevLabel.empty())
			{
				PROFILE_LABEL_POP(prevLabel.c_str());
				prevLabel = "";
			}
			
			PROFILE_LABEL_SCOPE(pass.pPrimitivePass->GetLabel());
			gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView()->GetDrawer().InitDrawSubmission();
			pass.pScenePass->Execute();
			gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView()->GetDrawer().JobifyDrawSubmission();
			gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView()->GetDrawer().WaitForDrawSubmission();
			pass.pScenePass->m_passContexts.resize(0);
		}
		else if (pass.passType == ePassType_Compute)
		{
			// TODO: Workaround
			//CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
			//CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();
			//pCommandInterface->SetRenderTargets(0, nullptr, nullptr, nullptr);
			GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);
			
			pass.pComputePass->Execute(GetDeviceObjectFactory().GetCoreCommandList());
		}
	}

	if (!prevLabel.empty())
	{
		PROFILE_LABEL_POP(prevLabel.c_str());
	}

	m_renderPasses.resize(0);
	m_resourceStates.clear();
}