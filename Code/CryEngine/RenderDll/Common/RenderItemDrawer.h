// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryThreading/CryThreadSafeRendererContainer.h>
#include <CryThreading/CryThreadSafeWorkerContainer.h>
#include <CryMath/Range.h>

#include "RenderPipeline.h"
#include "../XRenderD3D9/GraphicsPipeline/Common/GraphicsPipelineStage.h"

class CRenderItemDrawer
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Multi-threaded draw-command recording /////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	struct SRenderListCollection
	{
		void Init()
		{
			pEnd = pStart;
		}

		void Reserve(size_t numAdditionalItems)
		{
			size_t numItems =
			  (pEnd - pStart);
			size_t numItemsAfter = numItems +
			                       numAdditionalItems;

			if (rendList.size() < numItemsAfter)
			{
				rendList.resize(numItemsAfter);
				pStart = &rendList[0];
				pEnd = pStart + numItems;
			}
		}

		void Expand(const SGraphicsPipelinePassContext& rendList)
		{
			* pEnd++ = rendList;
		}

		void WaitForJobs()
		{
			if (pCommandLists.size())
			{
				gEnv->pJobManager->WaitForJob(jobState);
				GetDeviceObjectFactory().ForfeitCommandLists(std::move(pCommandLists));
			}
		}

		void PrepareJobs(uint32 numTasks)
		{
			WaitForJobs();

			pCommandLists = GetDeviceObjectFactory().AcquireCommandLists(numTasks);
		}

		std::vector<SGraphicsPipelinePassContext> rendList;

		SGraphicsPipelinePassContext*             pStart;
		SGraphicsPipelinePassContext*             pEnd;

		std::vector<CDeviceCommandListUPtr>       pCommandLists;
		JobManager::SJobState                     jobState;
	};

	SRenderListCollection m_CoalescedContexts;

	void DrawCompiledRenderItems(const SGraphicsPipelinePassContext& passContext) const;

	void InitDrawSubmission();
	void JobifyDrawSubmission(bool bForceImmediateExecution = false);
	void WaitForDrawSubmission();
};
