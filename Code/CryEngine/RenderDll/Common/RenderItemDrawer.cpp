// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

#define PREFETCH_STRIDE	512

void DrawCompiledRenderItemsToCommandList(
	const SGraphicsPipelinePassContext* pInputPassContext,
	const CRenderView::RenderItems* renderItems,
	CDeviceCommandList* commandList,
	int startRenderItem,
	int endRenderItem)
{
	FUNCTION_PROFILER_RENDERER();
	CRY_ASSERT(startRenderItem >= pInputPassContext->rendItems.start && endRenderItem <= pInputPassContext->rendItems.end);

	const SGraphicsPipelinePassContext &passContext = *pInputPassContext;
	const bool shouldIssueStartTimeStamp = startRenderItem == passContext.rendItems.start;
	const bool shouldIssueEndTimeStamp = endRenderItem == passContext.rendItems.end;
	CTimeValue deltaTimestamp(0LL);

	// Prepare command list
	commandList->Reset();

	// Start profile section
#if defined(ENABLE_PROFILING_CODE)
	if (gcpRendD3D->m_pPipelineProfiler)
	{
		gcpRendD3D->m_pPipelineProfiler->UpdateMultithreadedSection(passContext.profilerSectionIndex, true, 0, 0, shouldIssueStartTimeStamp, deltaTimestamp, commandList);
		deltaTimestamp = gEnv->pTimer->GetAsyncTime();
	}

	commandList->BeginProfilingSection();
#endif

	if (shouldIssueStartTimeStamp)
		commandList->GetGraphicsInterface()->BeginProfilerEvent(passContext.pSceneRenderPass->GetLabel());

	// Execute pass
	if (passContext.type == GraphicsPipelinePassType::resolve)
	{
		// Resolve pass
		passContext.pSceneRenderPass->ResolvePass(*commandList, passContext.resolveScreenBounds);
	}
	else
	{
		// Renderpass
		passContext.pSceneRenderPass->BeginRenderPass(*commandList, passContext.renderNearest); 

		// Allow only compiled objects to actually draw
		const uint32 alwaysRequiredFlags = FB_COMPILED_OBJECT;
		const uint32 batchExcludeFilter = passContext.batchExcludeFilter | alwaysRequiredFlags;

		static const int cDynamicInstancingMaxCount = 128;
		int dynamicInstancingCount = 0;
		CryStackAllocWithSizeVector(CCompiledRenderObject::SPerInstanceShaderData, cDynamicInstancingMaxCount + 1, dynamicInstancingBuffer, CDeviceBufferManager::AlignBufferSizeForStreaming);

		const bool cvarInstancing = CRendererCVars::CV_r_geominstancing != 0;

		CConstantBufferPtr tempInstancingCB;
		if (cvarInstancing)
		{
			// Constant buffer return with reference count of 1
			tempInstancingCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(cDynamicInstancingMaxCount * sizeof(CCompiledRenderObject::SPerInstanceShaderData), false, true);
		}

		// NOTE: doesn't load-balance well when the conditions for the draw mask lots of draws
		for (int32 i = startRenderItem, e = endRenderItem - 1; i <= e; ++i)
		{
			{
				// Accessing content of further away rendItem for prefetching also precaches the rendItem itself
				const SRendItem& rif = (*renderItems)[std::min<int32>(i + PREFETCH_STRIDE / sizeof(SRendItem), e)];
				// Last cache-line is the tail of the PSO array
				PrefetchLine(rif.pCompiledObject, 128);
			}

			const SRendItem& ri = (*renderItems)[i];
			if ((ri.nBatchFlags & batchExcludeFilter) == alwaysRequiredFlags)
			{
				if (ri.nBatchFlags & passContext.batchFilter)
				{
					if (cvarInstancing && (i < e && dynamicInstancingCount < cDynamicInstancingMaxCount))
					{
						for (int32 j = i + 1; j <= e; ++j)
						{
							{
								// Accessing content of further away rendItem for prefetching also precaches the rendItem itself
								const SRendItem& nextrif = (*renderItems)[std::min<int32>(j + PREFETCH_STRIDE / sizeof(SRendItem), e)];
								// Last cache-line is the tail of the PSO array
								PrefetchLine(nextrif.pCompiledObject, 128);
							}

							// Look ahead to see if we can instance multiple sequential draw calls that have same draw parameters, with only difference in per instance constant buffer
							const SRendItem& nextri = (*renderItems)[j];
							if (nextri.nBatchFlags & (nextri.nBatchFlags & passContext.batchExcludeFilter ? 0 : passContext.batchFilter))
							{
								if (ri.pCompiledObject->CheckDynamicInstancing(passContext, nextri.pCompiledObject))
								{
									dynamicInstancingBuffer[dynamicInstancingCount] = nextri.pCompiledObject->GetInstancingData();
									dynamicInstancingCount++;

									continue;
								}
							}
						}

						if (dynamicInstancingCount > 0)
						{
							i += dynamicInstancingCount;

							// Add current object to the dynamic array.
							dynamicInstancingBuffer[dynamicInstancingCount] = ri.pCompiledObject->GetInstancingData();
							dynamicInstancingCount++;

#if defined(ENABLE_PROFILING_CODE)
							CryInterlockedAdd(&SRenderStatistics::Write().m_nAsynchNumInsts, dynamicInstancingCount);
							CryInterlockedIncrement(&SRenderStatistics::Write().m_nAsynchNumInstCalls);
#endif //ENABLE_PROFILING_CODE

							tempInstancingCB->UpdateBuffer(&dynamicInstancingBuffer[0], dynamicInstancingCount * sizeof(CCompiledRenderObject::SPerInstanceShaderData));

							// Draw object with dynamic instancing
							// TODO: remove this special case and fall through to below
							ri.pCompiledObject->DrawToCommandList(passContext, commandList, tempInstancingCB.get(), dynamicInstancingCount);

							dynamicInstancingCount = 0;
							continue;
						}
					}

					// Draw single instance
					ri.pCompiledObject->DrawToCommandList(passContext, commandList);
				}
			}
		}

		passContext.pSceneRenderPass->EndRenderPass(*commandList, passContext.renderNearest);
	}

	// End profile section
	if (shouldIssueEndTimeStamp)
		commandList->GetGraphicsInterface()->EndProfilerEvent(passContext.pSceneRenderPass->GetLabel());

#if defined(ENABLE_PROFILING_CODE)
	if (gcpRendD3D->m_pPipelineProfiler)
	{
		deltaTimestamp = gEnv->pTimer->GetAsyncTime() - deltaTimestamp;
		gcpRendD3D->m_pPipelineProfiler->UpdateMultithreadedSection(passContext.profilerSectionIndex, false, commandList->EndProfilingSection().numDIPs,
			commandList->EndProfilingSection().numPolygons, shouldIssueEndTimeStamp, deltaTimestamp, commandList);
	}

	gcpRendD3D->AddRecordedProfilingStats(commandList->EndProfilingSection(), passContext.pSceneRenderPass->GetRenderList(), true);
#endif
}

// NOTE: Job-System can't handle references (copies) and can't use static member functions or __restrict (doesn't execute)
// NOTE: Job-System can't handle unique-ptr, we have to pass a pointer to get the std::move
void ListDrawCommandRecorderJob(
  SGraphicsPipelinePassContext* passContext,
  CDeviceCommandListUPtr* commandList,
  int startJobItem,
  int endJobItem
  )
{
	FUNCTION_PROFILER_RENDERER();

	  (*commandList)->LockToThread();

	int cursor = 0;
	do
	{
		int length = passContext->rendItems.Length();

		// bounding-range check (1D bbox)
		if ((startJobItem <= (cursor + length)) && (cursor < endJobItem))
		{
			int offset = std::max(startJobItem, cursor) - cursor;
			int count = std::min(cursor + length, endJobItem) - std::max(startJobItem, cursor);

			auto& RESTRICT_REFERENCE renderItems = passContext->pRenderView->GetRenderItems(passContext->renderListId);

			DrawCompiledRenderItemsToCommandList(
				passContext,
				&renderItems,
				(*commandList).get(),
				passContext->rendItems.start + offset,
				passContext->rendItems.start + offset + count);
		}

		cursor += length; // NOTE: cursor will always jump to start of next work item (i.e. pass context)
		++passContext;
	}
	while (cursor < endJobItem);

	(*commandList)->Close();
	GetDeviceObjectFactory().ForfeitCommandList(std::move(*commandList));
}

DECLARE_JOB("ListDrawCommandRecorder", TListDrawCommandRecorder, ListDrawCommandRecorderJob);

void CRenderItemDrawer::DrawCompiledRenderItems(const SGraphicsPipelinePassContext& passContext) const
{
	if (passContext.type == GraphicsPipelinePassType::renderPass && passContext.rendItems.IsEmpty())
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
	PROFILE_FRAME(JobifyDrawSubmission);

	SGraphicsPipelinePassContext* pStart = m_CoalescedContexts.pStart;
	SGraphicsPipelinePassContext* pEnd = m_CoalescedContexts.pEnd;
	SGraphicsPipelinePassContext* pCursor = pStart;

	uint32 numItems = 0;
	pCursor = pStart;
	while (pCursor != pEnd)
	{
		numItems += pCursor->rendItems.Length();
		++pCursor;
	}

	if (numItems == 0)
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
				passContext.rendItems.end);

			++pCursor;
		}
	}
}

void CRenderItemDrawer::WaitForDrawSubmission()
{
#if (CRY_RENDERER_DIRECT3D >= 120) || CRY_RENDERER_GNM || CRY_RENDERER_VULKAN
	if (CRenderer::CV_r_multithreadedDrawing == 0)
		return;

	CRY_PROFILE_FUNCTION_WAITING(PROFILE_RENDERER)

	m_CoalescedContexts.WaitForJobs();
#endif
}
