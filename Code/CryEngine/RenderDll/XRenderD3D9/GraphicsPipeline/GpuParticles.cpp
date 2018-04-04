// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "Gpu/Particles/GpuParticleManager.h"
#include "GpuParticles.h"

CGpuParticlesStage::CGpuParticlesStage()
	: m_oldFrameIdExecute(-1)
	, m_oldFrameIdPreDraw(-1)
	, m_oldFrameIdPostDraw(-1)
{
}

CGpuParticlesStage::~CGpuParticlesStage()
{
	if (m_pGpuParticleManager)
		m_pGpuParticleManager->CleanupResources();
}

void CGpuParticlesStage::Init()
{
	if (!m_pGpuParticleManager)
		m_pGpuParticleManager = std::unique_ptr<gpu_pfx2::CManager>(new gpu_pfx2::CManager());
}

void CGpuParticlesStage::Prepare(CRenderView* pRenderView)
{
}

void CGpuParticlesStage::Execute(CRenderView* pRenderView)
{
	int CurrentFrameID = gcpRendD3D.GetFrameID(false);
	if (CurrentFrameID != m_oldFrameIdExecute)
	{
		m_pGpuParticleManager->RenderThreadUpdate();
		m_oldFrameIdExecute = CurrentFrameID;
	}
}

void CGpuParticlesStage::PreDraw(CRenderView* pRenderView)
{
	int CurrentFrameID = gcpRendD3D.GetFrameID(false);
	if (CurrentFrameID != m_oldFrameIdPreDraw)
	{
		m_pGpuParticleManager->RenderThreadPreUpdate();
		m_oldFrameIdPreDraw = CurrentFrameID;
	}
}

void CGpuParticlesStage::PostDraw(CRenderView* pRenderView)
{
	int CurrentFrameID = gcpRendD3D.GetFrameID(false);
	if (CurrentFrameID != m_oldFrameIdPostDraw)
	{
		m_pGpuParticleManager->RenderThreadPostUpdate();
		m_oldFrameIdPostDraw = CurrentFrameID;
	}
}

gpu_pfx2::IManager* CD3D9Renderer::GetGpuParticleManager()
{
	return GetGraphicsPipeline().GetGpuParticlesStage()->GetGpuParticleManager();
}