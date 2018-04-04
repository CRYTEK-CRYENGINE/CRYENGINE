// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "RenderView.h"

#include "DriverD3D.h"

#include "GraphicsPipeline/ShadowMap.h"
#include "CompiledRenderObject.h"

//////////////////////////////////////////////////////////////////////////
// Multi-threaded draw-command recording /////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <CryThreading/IJobManager.h>
#include <CryThreading/IJobManager_JobDelegator.h>

void DrawCompiledRenderItemsToCommandList(
  const SGraphicsPipelinePassContext* pInputPassContext,
  CRenderView::RenderItems* renderItems,
  CDeviceCommandList* commandList,
  int startRenderItem,
  int endRenderItem
  )
{
	FUNCTION_PROFILER_RENDERER

	SGraphicsPipelinePassContext passContext = *pInputPassContext;
	passContext.pCommandList = commandList;

	bool bContextStart = passContext.renderItemGroup == 0 && startRenderItem == passContext.rendItems.start;
	passContext.pSceneRenderPass->BeginRenderPass(*commandList, passContext.renderNearest, passContext.profilerSectionIndex, bContextStart);

	static const int cDynamicInstancingMaxCount = 128;
	int dynamicInstancingCount = 0;
	CryStackAllocWithSizeVector(CCompiledRenderObject::SPerInstanceShaderData, cDynamicInstancingMaxCount + 1, dynamicInstancingBuffer, CDeviceBufferManager::AlignBufferSizeForStreaming);
	CCompiledRenderObject* pDynamicInstancedObject = nullptr;

	const bool cvarInstancing = CRendererCVars::CV_r_geominstancing != 0;

	CConstantBufferPtr tempInstancingCB;
	if (cvarInstancing)
	{
		// Constant buffer return with reference count of 1
		tempInstancingCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(cDynamicInstancingMaxCount * sizeof(CCompiledRenderObject::SPerInstanceShaderData), false, true);
	}

	// NOTE: doesn't load-balance well when the conditions for the draw mask lots of draws
	for (int32 i = startRenderItem; i < endRenderItem; ++i)
	{
		SRendItem& ri = (*renderItems)[i];
		if (!(ri.nBatchFlags & passContext.batchFilter))
			continue;

		if (ri.nBatchFlags & passContext.batchExcludeFilter)
			continue;

		if (!ri.pCompiledObject)
			continue;

		if (cvarInstancing && (i < endRenderItem - 1 && dynamicInstancingCount < cDynamicInstancingMaxCount))
		{
			// Look ahead to see if we can instance multiple sequential draw calls that have same draw parameters, with only difference in per instance constant buffer
			SRendItem& nextri = (*renderItems)[i + 1];
			if ((nextri.nBatchFlags & passContext.batchFilter)
			   && !(nextri.nBatchFlags & passContext.batchExcludeFilter)
			   && nextri.pCompiledObject)
			{
				if (ri.pCompiledObject->CheckDynamicInstancing(passContext, nextri.pCompiledObject))
				{
					dynamicInstancingBuffer[dynamicInstancingCount] = ri.pCompiledObject->GetInstancingData();
					dynamicInstancingCount++;

					if (!pDynamicInstancedObject)
						pDynamicInstancedObject = ri.pCompiledObject;

					ri.nBatchFlags |= FB_COMPILED_OBJECT;
					continue;
				}
			}
		}

		if (dynamicInstancingCount == 0)
		{
			// Draw single instance
			ri.pCompiledObject->DrawToCommandList(passContext);
		}
		else
		{
			// Add current object to the dynamic array.
			dynamicInstancingBuffer[dynamicInstancingCount] = ri.pCompiledObject->GetInstancingData();
			dynamicInstancingCount++;

#if defined(ENABLE_PROFILING_CODE)
			CryInterlockedAdd(&SPipeStat::Out()->m_nInsts, dynamicInstancingCount);
			CryInterlockedIncrement(&SPipeStat::Out()->m_nInstCalls);
#endif //ENABLE_PROFILING_CODE

			tempInstancingCB->UpdateBuffer(&dynamicInstancingBuffer[0], dynamicInstancingCount * sizeof(CCompiledRenderObject::SPerInstanceShaderData));

			// Draw object with dynamic instancing
			pDynamicInstancedObject->DrawToCommandList(passContext, tempInstancingCB.get(), dynamicInstancingCount);
			pDynamicInstancedObject = nullptr;
			dynamicInstancingCount = 0;
		}

		ri.nBatchFlags |= FB_COMPILED_OBJECT;
	}

	bool bContextEnd = passContext.renderItemGroup == passContext.pSceneRenderPass->GetNumRenderItemGroups() - 1 && endRenderItem == passContext.rendItems.end;
	passContext.pSceneRenderPass->EndRenderPass(*commandList, passContext.renderNearest, passContext.profilerSectionIndex, bContextEnd);
}

// NOTE: Job-System can't handle references (copies) and can't use static member functions or __restrict (doesn't execute)
// NOTE: Job-System can't handle unique-ptr, we have to pass a pointer to get the std::move
void ListDrawCommandRecorderJob(
  SGraphicsPipelinePassContext* passContext,
  CDeviceCommandListUPtr* commandList,
  int startRenderItem,
  int endRenderItem
  )
{
	FUNCTION_PROFILER_RENDERER

	  (*commandList)->LockToThread();

	int cursor = 0;
	do
	{
		int length = passContext->rendItems.Length();

		// bounding-range check (1D bbox)
		if ((startRenderItem <= (cursor + length)) && (cursor < endRenderItem))
		{
			int offset = std::max(startRenderItem, cursor) - cursor;
			int count = std::min(cursor + length, endRenderItem) - std::max(startRenderItem, cursor);

			auto& RESTRICT_REFERENCE renderItems = passContext->pRenderView->GetRenderItems(passContext->renderListId);

			DrawCompiledRenderItemsToCommandList(
			  passContext,
			  &renderItems,
			  (*commandList).get(),
			  passContext->rendItems.start + offset,
			  passContext->rendItems.start + offset + count
			  );
		}

		cursor += length;
		passContext += 1;
	}
	while (cursor < endRenderItem);

	(*commandList)->Close();
	GetDeviceObjectFactory().ForfeitCommandList(std::move(*commandList));
}

DECLARE_JOB("ListDrawCommandRecorder", TListDrawCommandRecorder, ListDrawCommandRecorderJob);

void CRenderItemDrawer::DrawCompiledRenderItems(const SGraphicsPipelinePassContext& passContext) const
{
	if (gcpRendD3D->m_nGraphicsPipeline < 2)
		return;
	if (passContext.rendItems.IsEmpty())
		return;
	if (CRenderer::CV_r_NoDraw == 2) // Completely skip filling of the command list.
		return;

	const bool bAllowRenderNearest = CRenderer::CV_r_nodrawnear == 0;
	if (!bAllowRenderNearest && passContext.renderNearest)
		return;

	PROFILE_FRAME(DrawCompiledRenderItems);

	// Should take items from passContext and be view dependent.
	CRenderView* pRenderView = passContext.pRenderView;

	// collect all compiled object pointers into the root of the RenderView hierarchy
	while (pRenderView->GetParentView())
	{
		pRenderView = pRenderView->GetParentView();
	}

	pRenderView->GetDrawer().m_CoalescedContexts.Reserve(1);
	pRenderView->GetDrawer().m_CoalescedContexts.Expand(passContext);
}

void CRenderItemDrawer::InitDrawSubmission()
{
	m_CoalescedContexts.Init();
}

void CRenderItemDrawer::JobifyDrawSubmission(bool bForceImmediateExecution)
{
	if (gcpRendD3D->m_nGraphicsPipeline < 2)
		return;

	PROFILE_FRAME(JobifyDrawSubmission);

	SGraphicsPipelinePassContext* pStart = m_CoalescedContexts.pStart;
	SGraphicsPipelinePassContext* pEnd = m_CoalescedContexts.pEnd;
	SGraphicsPipelinePassContext* pCursor = pStart;

	uint32 numItems = 0;
	pCursor = pStart;
	while (pCursor != pEnd)
	{
		numItems += pCursor->rendItems.Length();
		pCursor += 1;
	}

	if (numItems <= 0)
		return;

#if (CRY_RENDERER_DIRECT3D >= 120) || CRY_RENDERER_GNM || CRY_RENDERER_VULKAN
	if (!CRenderer::CV_r_multithreadedDrawing)
		bForceImmediateExecution = true;

	if (!bForceImmediateExecution)
	{
		int32 numTasksTentative = CRenderer::CV_r_multithreadedDrawing;
		uint32 numTasks = std::min(numItems, uint32(numTasksTentative > 0 ? numTasksTentative : std::max(1U, gEnv->GetJobManager()->GetNumWorkerThreads() - 2U)));
		uint32 numItemsPerTask = (numItems + (numTasks - 1)) / numTasks;

		if (CRenderer::CV_r_multithreadedDrawingMinJobSize > 0)
		{
			if ((numTasks > 1) && (numItemsPerTask < CRenderer::CV_r_multithreadedDrawingMinJobSize))
			{
				numTasks = std::max(1U, numItems / CRenderer::CV_r_multithreadedDrawingMinJobSize);
				numItemsPerTask = (numItems + (numTasks - 1)) / numTasks;
			}
		}

		if (((numTasksTentative = numTasks) > 1) || (CRenderer::CV_r_multithreadedDrawingMinJobSize == 0))
		{
			m_CoalescedContexts.PrepareJobs(numTasks);

			for (uint32 curTask = 1; curTask < numTasks; ++curTask)
			{
				const uint32 taskRIStart = 0 + ((curTask + 0) * numItemsPerTask);
				const uint32 taskRIEnd = 0 + ((curTask + 1) * numItemsPerTask);

				TListDrawCommandRecorder job(pStart, &m_CoalescedContexts.pCommandLists[curTask], taskRIStart, taskRIEnd < numItems ? taskRIEnd : numItems);

				job.RegisterJobState(&m_CoalescedContexts.jobState);
				job.SetPriorityLevel(JobManager::eHighPriority);
				job.Run();
			}

			// Execute first task directly on render thread
			{
				PROFILE_FRAME("ListDrawCommandRecorderJob");

				const uint32 taskRIStart = 0;
				const uint32 taskRIEnd = numItemsPerTask;

				ListDrawCommandRecorderJob(pStart, &m_CoalescedContexts.pCommandLists[0], taskRIStart, taskRIEnd < numItems ? taskRIEnd : numItems);
			}
		}

		bForceImmediateExecution = (numTasksTentative <= 1) && (CRenderer::CV_r_multithreadedDrawingMinJobSize != 0);
	}

	if (bForceImmediateExecution)
#endif
	{
		pCursor = pStart;
		while (pCursor != pEnd)
		{
			const SGraphicsPipelinePassContext& passContext = *pCursor;

			// Should take items from passContext and be view dependent.
			CRenderView* pRenderView = passContext.pRenderView;
			auto& RESTRICT_REFERENCE renderItems = pRenderView->GetRenderItems(passContext.renderListId);
			auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

			DrawCompiledRenderItemsToCommandList(
			  &passContext,
			  &renderItems,
			  &commandList,
			  passContext.rendItems.start,
			  passContext.rendItems.end
			  );

			pCursor += 1;
		}
	}
}

void CRenderItemDrawer::WaitForDrawSubmission()
{
	if (gcpRendD3D->m_nGraphicsPipeline < 2)
		return;

#if (CRY_RENDERER_DIRECT3D >= 120) || CRY_RENDERER_GNM || CRY_RENDERER_VULKAN
	if (CRenderer::CV_r_multithreadedDrawing == 0)
		return;

	CRY_PROFILE_FUNCTION_WAITING(PROFILE_RENDERER)

	m_CoalescedContexts.WaitForJobs();
#endif
}
