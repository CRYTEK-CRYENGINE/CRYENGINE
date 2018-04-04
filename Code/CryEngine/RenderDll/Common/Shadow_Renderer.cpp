// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "Shadow_Renderer.h"

#include "DriverD3D.h"
#include "D3D_SVO.h"

#include "Common/RenderView.h"

int ShadowMapFrustum::GetNumSides() const
{
	int nSides = bOmniDirectionalShadow ? OMNI_SIDES_NUM : 1;
	return nSides;
}

CCamera ShadowMapFrustum::GetCamera(int side) const
{
	if (!bOmniDirectionalShadow)
	{
		return gEnv->p3DEngine->GetRenderingCamera();
	}
	return FrustumPlanes[side];
}

//////////////////////////////////////////////////////////////////////////
void ShadowMapFrustum::Job_RenderShadowCastersToView(const SRenderingPassInfo& passInfo, bool bJobCasters)
{
	auto* casters = &castersList;
	if (bJobCasters)
		casters = &jobExecutedCastersList;

	for (int i = 0; i < casters->Count(); i++)
	{
		IShadowCaster* pShadowCaster = (*casters)[i];
		//TOFIX reactivate OmniDirectionalShadow
		if (bOmniDirectionalShadow)
		{
			AABB aabb = pShadowCaster->GetBBoxVirtual();
			//!!! Reactivate proper camera
			bool bVis = passInfo.GetCamera().IsAABBVisible_F(aabb);
			if (!bVis)
				continue;
		}

		if ((m_Flags & DLF_DIFFUSEOCCLUSION) && pShadowCaster->HasOcclusionmap(0, pLightOwner))
			continue;

		gEnv->p3DEngine->RenderRenderNode_ShadowPass(pShadowCaster, passInfo);
	}
}

void ShadowMapFrustum::SortRenderItemsForFrustumAsync(int side, SRendItem* pFirst, size_t nNumRendItems)
{
	FUNCTION_PROFILER_RENDERER;

	if (nNumRendItems > 0)
	{
		// If any render items on this frustum side, update shadow gen side mask
		CryInterlockedExchangeOr((volatile LONG*)&nShadowGenMask, (1 << side));
	}

	SRendItem::mfSortByLight(pFirst, nNumRendItems, true, false, false);
}

//////////////////////////////////////////////////////////////////////////
CRenderView* ShadowMapFrustum::GetNextAvailableShadowsView(CRenderView* pMainRenderView, ShadowMapFrustum* pOwnerFrustum)
{
	// Get next view.
	std::swap(m_pShadowsView[0], m_pShadowsView[1]);
	if (!m_pShadowsView[0])
	{
		static int globalCounter = 0;
		CryFixedStringT<128> renderViewName;
		renderViewName.Format("Shadow View %d", globalCounter++);
		m_pShadowsView[0] = new CRenderView(renderViewName.c_str(), CRenderView::eViewType_Shadow, pMainRenderView, pOwnerFrustum);
	}

	CRenderView* pShadowsView = static_cast<CRenderView*>(m_pShadowsView[0].get());
	CRY_ASSERT(pShadowsView->GetUsageMode() == IRenderView::eUsageModeUndefined || pShadowsView->GetUsageMode() == IRenderView::eUsageModeReadingDone);

	pShadowsView->SetShadowFrustumOwner(pOwnerFrustum);
	pShadowsView->SetParentView(pMainRenderView);
	pShadowsView->SetFrameId(pMainRenderView->GetFrameId());

	return pShadowsView;
}

ShadowMapFrustumPtr ShadowMapFrustum::Clone()
{
	ShadowMapFrustumPtr pFrustum = new ShadowMapFrustum;
	*pFrustum = (*this);
	pFrustum->m_pShadowsView.fill(nullptr);
	return pFrustum;
}

//////////////////////////////////////////////////////////////////////////
void ShadowMapFrustum::RenderShadowFrustum(CRenderView* pRenderView, CRenderView* pShadowsView, int side, bool bJobCasters)
{
	int nThreadID = SRenderThread::GetLocalThreadCommandBufferId();

	ShadowMapFrustum* pFrustum = this;

	uint32 nRenderingFlags = SRenderingPassInfo::DEFAULT_SHADOWS_FLAGS;

#if defined(FEATURE_SVO_GI)
	if (CSvoRenderer::GetRsmColorMap(*pFrustum))
	{
		// we need correct diffuse texture for every chunk
		nRenderingFlags |= SRenderingPassInfo::DISABLE_RENDER_CHUNK_MERGE;
	}
#endif

	assert(pShadowsView);

	CCamera tmpCamera = GetCamera(side);

	// create a matching rendering pass info for shadows
	SRenderingPassInfo passInfo = SRenderingPassInfo::CreateShadowPassRenderingInfo(
	  pShadowsView,
	  tmpCamera,
	  pFrustum->m_Flags,
	  pFrustum->nShadowMapLod,
	  pFrustum->IsCached(),
	  pFrustum->bIsMGPUCopy,
	  &pFrustum->nShadowGenMask,
	  side,
	  pFrustum->nShadowGenID[nThreadID][side],
	  nRenderingFlags);

	//if (!bJobCasters)
	{
		// Make sure no jobs used during rendering
		passInfo.SetWriteMutex(nullptr);
	}

	if (!bJobCasters)
	{
		// Render not jobified casters in the main thread.
		Job_RenderShadowCastersToView(passInfo, false);
	}
	else
	{
		// Render job casters as multithreaded jobs
		auto lambda = [=]() { Job_RenderShadowCastersToView(passInfo, true); };
		gEnv->pJobManager->AddLambdaJob("RenderShadowCasters", lambda, JobManager::eLowPriority, pShadowsView->GetWriteMutex());
		//lambda();
	}
}

//////////////////////////////////////////////////////////////////////////
void SShadowRenderer::RenderFrustumsToView(CRenderView* pRenderView)
{
	FUNCTION_PROFILER_RENDERER;

	int nThreadID = SRenderThread::GetLocalThreadCommandBufferId();

	CCamera tmpCamera;

	auto& frustumsToRender = pRenderView->GetFrustumsToRender();

	for (SShadowFrustumToRender& rFrustumToRender : frustumsToRender)
	{
		rFrustumToRender.pShadowsView->SwitchUsageMode(IRenderView::eUsageModeWriting);
	}

	// First do a pass and submit to rendering with job enabled casters
	// This will create multithreaded job for each rendering pass
	for (SShadowFrustumToRender& rFrustumToRender : frustumsToRender)
	{
		ShadowMapFrustum* pCurFrustum = rFrustumToRender.pFrustum;
		int numSides = pCurFrustum->GetNumSides();
		for (int side = 0; side < numSides; side++)
		{
			if (pCurFrustum->nShadowGenID[nThreadID][side] == 0xFFFFFFFF)
				continue;
			//////////////////////////////////////////////////////////////////////////
			// Invoke IRenderNode::Render
			pCurFrustum->RenderShadowFrustum(pRenderView, static_cast<CRenderView*>(rFrustumToRender.pShadowsView.get()), side, true);
		}
	}

	// Now do a rendering pass for non job enabled casters, they will be rendered in this thread
	for (SShadowFrustumToRender& rFrustumToRender : frustumsToRender)
	{
		ShadowMapFrustum* pCurFrustum = rFrustumToRender.pFrustum;
		int numSides = pCurFrustum->GetNumSides();
		for (int side = 0; side < numSides; side++)
		{
			if (pCurFrustum->nShadowGenID[nThreadID][side] == 0xFFFFFFFF)
				continue;
			//////////////////////////////////////////////////////////////////////////
			// Invoke IRenderNode::Render
			pCurFrustum->RenderShadowFrustum(pRenderView, static_cast<CRenderView*>(rFrustumToRender.pShadowsView.get()), side, false);
		}
	}

	// Switch all shadow views WritingDone
	for (SShadowFrustumToRender& rFrustumToRender : frustumsToRender)
	{
		rFrustumToRender.pShadowsView->SwitchUsageMode(IRenderView::eUsageModeWritingDone);
	}
	pRenderView->PostWriteShadowViews();
}
