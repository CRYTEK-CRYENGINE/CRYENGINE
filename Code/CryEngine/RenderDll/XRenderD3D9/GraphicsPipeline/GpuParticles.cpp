// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Gpu/Particles/GpuParticleManager.h"
#include "GpuParticles.h"

CComputeParticlesStage::CComputeParticlesStage()
	: m_oldFrameIdExecute(-1)
	, m_oldFrameIdPreDraw(-1)
	, m_oldFrameIdPostDraw(-1)
{
}

CComputeParticlesStage::~CComputeParticlesStage()
{
	if (m_pGpuParticleManager)
		m_pGpuParticleManager->CleanupResources();
}

void CComputeParticlesStage::Init()
{
	if (!m_pGpuParticleManager)
		m_pGpuParticleManager = std::unique_ptr<gpu_pfx2::CManager>(new gpu_pfx2::CManager());
}

void CComputeParticlesStage::Update()
{
	CRenderView* pRenderView = RenderView();
	int CurrentFrameID = pRenderView->GetFrameId();
	if (CurrentFrameID != m_oldFrameIdExecute)
	{
		m_pGpuParticleManager->RenderThreadUpdate(pRenderView);
		m_oldFrameIdExecute = CurrentFrameID;
	}
}

void CComputeParticlesStage::PreDraw()
{
	CRenderView* pRenderView = RenderView();
	int CurrentFrameID = pRenderView->GetFrameId();
	if (CurrentFrameID != m_oldFrameIdPreDraw)
	{
		m_pGpuParticleManager->RenderThreadPreUpdate(pRenderView);
		m_oldFrameIdPreDraw = CurrentFrameID;
	}
}

void CComputeParticlesStage::PostDraw()
{
	CRenderView* pRenderView = RenderView();
	int CurrentFrameID = pRenderView->GetFrameId();
	if (CurrentFrameID != m_oldFrameIdPostDraw)
	{
		m_pGpuParticleManager->RenderThreadPostUpdate(pRenderView);
		m_oldFrameIdPostDraw = CurrentFrameID;
	}
}

gpu_pfx2::IManager* CD3D9Renderer::GetGpuParticleManager()
{
	return GetGraphicsPipeline().GetComputeParticlesStage()->GetGpuParticleManager();
}