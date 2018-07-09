// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"

#include "D3DStereo.h"

#include <CrySystem/Scaleform/IFlashPlayer.h>

#include "D3DREBreakableGlassBuffer.h"

#include <CryCore/CryCustomTypes.h>

#include "Gpu/Particles/GpuParticleManager.h"
#include <CrySystem/VR/IHMDManager.h>

#include "D3D_SVO.h"
#include <Common/RenderDisplayContext.h>
//=======================================================================

bool CD3D9Renderer::RT_CreateDevice()
{
	LOADING_TIME_PROFILE_SECTION;
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_D3D, 0, "Renderer CreateDevice");

#if CRY_PLATFORM_WINDOWS && !defined(SUPPORT_DEVICE_INFO)
	if (!m_bShaderCacheGen && !SetWindow(m_width, m_height))
		return false;
#endif

	return CreateDevice();
}

void CD3D9Renderer::RT_FlashRenderInternal(std::shared_ptr<IFlashPlayer> &&pPlayer)
{
	FUNCTION_PROFILER_RENDERER();

	SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_PUSH);

	// In menu mode we also render to screen in addition to quad layer
	const bool renderToScreen = !GetS3DRend().IsStereoEnabled() || GetS3DRend().IsMenuModeEnabled();

	if (GetS3DRend().IsStereoEnabled())
	{
		if (GetS3DRend().IsQuadLayerEnabled())
		{
			auto quadRenderScope = GetS3DRend().PrepareRenderingToVrQuadLayer(RenderLayer::eQuadLayers_0);
			pPlayer->Render();
		}
	}

	if (renderToScreen)
	{
		pPlayer->Render();
	}

	SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_POP);

	if (CRendererCVars::CV_r_FlushToGPU >= 1)
		GetDeviceObjectFactory().FlushToGPU();
}

void CD3D9Renderer::RT_FlashRenderInternal(std::shared_ptr<IFlashPlayer_RenderProxy> &&pPlayer, bool bDoRealRender)
{
	FUNCTION_PROFILER_RENDERER();

	if (bDoRealRender)
	{
		SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_PUSH);

		// In menu mode we also render to screen in addition to quad layer
		const bool renderToScreen = !GetS3DRend().IsStereoEnabled() || GetS3DRend().IsMenuModeEnabled();

		if (GetS3DRend().IsStereoEnabled())
		{
			if (GetS3DRend().IsQuadLayerEnabled())
			{
				auto quadRenderScope = GetS3DRend().PrepareRenderingToVrQuadLayer(RenderLayer::eQuadLayers_0);
				pPlayer->RenderCallback(IFlashPlayer_RenderProxy::EFT_Mono);
			}
			else
			{
				{
					auto eyeRenderScope = GetS3DRend().PrepareRenderingToEye(CCamera::eEye_Left);
					pPlayer->RenderCallback(IFlashPlayer_RenderProxy::EFT_StereoLeft);
				}

				if (GetS3DRend().RequiresSequentialSubmission())
				{
					auto eyeRenderScope = GetS3DRend().PrepareRenderingToEye(CCamera::eEye_Right);
					pPlayer->RenderCallback(IFlashPlayer_RenderProxy::EFT_StereoRight);
				}
			}
		}

		if (renderToScreen)
		{
			pPlayer->RenderCallback(IFlashPlayer_RenderProxy::EFT_Mono);
		}

		SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_POP);
	}
	else
	{
		pPlayer->DummyRenderCallback(IFlashPlayer_RenderProxy::EFT_Mono);
	}

	if (CRendererCVars::CV_r_FlushToGPU >= 1)
		GetDeviceObjectFactory().FlushToGPU();
}

void CD3D9Renderer::RT_FlashRenderPlaybackLocklessInternal(std::shared_ptr<IFlashPlayer_RenderProxy> &&pPlayer, int cbIdx, bool bFinalPlayback, bool bDoRealRender)
{
	if (bDoRealRender)
	{
		SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_PUSH);

		// In menu mode we also render to screen in addition to quad layer
		const bool renderToScreen = !GetS3DRend().IsStereoEnabled() || GetS3DRend().IsMenuModeEnabled();

		if (GetS3DRend().IsStereoEnabled())
		{
			if (GetS3DRend().IsQuadLayerEnabled())
			{
				auto quadRenderScope = GetS3DRend().PrepareRenderingToVrQuadLayer(RenderLayer::eQuadLayers_0);
				pPlayer->RenderPlaybackLocklessCallback(cbIdx, IFlashPlayer_RenderProxy::EFT_Mono, bFinalPlayback && !renderToScreen);
			}
			else
			{
				{
					auto eyeRenderScope = GetS3DRend().PrepareRenderingToEye(CCamera::eEye_Left);
					pPlayer->RenderPlaybackLocklessCallback(cbIdx, IFlashPlayer_RenderProxy::EFT_StereoLeft, false);
				}

				if (GetS3DRend().RequiresSequentialSubmission())
				{
					auto eyeRenderScope = GetS3DRend().PrepareRenderingToEye(CCamera::eEye_Right);
					pPlayer->RenderPlaybackLocklessCallback(cbIdx, IFlashPlayer_RenderProxy::EFT_StereoRight, bFinalPlayback && !renderToScreen);
				}
			}
		}

		// In menu mode we also render to screen in addition to quad layer
		if (renderToScreen)
		{
			pPlayer->RenderPlaybackLocklessCallback(cbIdx, IFlashPlayer_RenderProxy::EFT_Mono, bFinalPlayback);
		}

		SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_POP);
	}
	else
	{
		pPlayer->DummyRenderCallback(IFlashPlayer_RenderProxy::EFT_Mono);
	}

	if (CRendererCVars::CV_r_FlushToGPU >= 1)
		GetDeviceObjectFactory().FlushToGPU();
}

void CD3D9Renderer::RT_Init()
{
	EF_Init();
}

void CD3D9Renderer::RT_ReleaseRenderResources(uint32 nFlags)
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CD3D9Renderer::RT_ReleaseRenderResources");

	if (nFlags & FRR_FLUSH_TEXTURESTREAMING)
	{
		CTexture::RT_FlushStreaming(true);
	}

	if (nFlags & FRR_PERMANENT_RENDER_OBJECTS)
	{
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
		{
			FreePermanentRenderObjects(i);
		}
	}

	if (nFlags & FRR_DELETED_MESHES)
	{
		CRenderMesh::Tick(MAX_RELEASED_MESH_FRAMES);
	}

	if (nFlags & FRR_POST_EFFECTS)
	{
		if (m_pPostProcessMgr)
			m_pPostProcessMgr->ReleaseResources();
	}

	if (nFlags & FRR_SYSTEM_RESOURCES)
	{
		// 1) Make sure all high level objects (CRenderMesh, CRenderElement,..) are gone
		RT_DelayedDeleteResources(true);
		
		CREBreakableGlassBuffer::RT_ReleaseInstance();

		CRenderMesh::Tick(MAX_RELEASED_MESH_FRAMES);
		CRenderElement::Cleanup();

		// 2) Release renderer created high level stuff (CStandardGraphicsPipeline, CPrimitiveRenderPass, CSceneRenderPass,..)
#if RENDERER_SUPPORT_SCALEFORM
		SF_DestroyResources();
#endif

		// Drop stereo resources
		if (gRenDev->GetIStereoRenderer())
			gRenDev->GetIStereoRenderer()->ReleaseRenderResources();

		RT_GraphicsPipelineShutdown();

		// 3) At this point all device objects should be gone and we can safely reset PSOs, ResourceLayouts,..
		CDeviceObjectFactory::ResetInstance();
		
		// 4) Now release textures and shaders
		m_cEF.mfReleaseSystemShaders();
		m_cEF.m_Bin.InvalidateCache();

		EF_Exit();
		CRendererResources::DestroySystemTargets();
		CRendererResources::UnloadDefaultSystemTextures();
	}

	if (nFlags & FRR_TEXTURES)
	{
		// Must also delete back buffers from Display Contexts
		for (auto& pCtx : m_displayContexts)
			pCtx.second->ReleaseResources();

		CTexture::ShutDown();
		CRendererResources::ShutDown();
	}
	else
	{
		CRendererResources::Clear();
	}

	// sync dev buffer only once per frame, to prevent syncing to the currently rendered frame
	// which would result in a deadlock
	if (nFlags & (FRR_SYSTEM_RESOURCES | FRR_DELETED_MESHES))
	{
		gRenDev->m_DevBufMan.Sync(gRenDev->GetRenderFrameID());
	}
}

void CD3D9Renderer::RT_CreateRenderResources()
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CD3D9Renderer::RT_CreateRenderResources");

	CRendererResources::LoadDefaultSystemTextures();
	CRendererResources::CreateSystemTargets(0, 0);
	EF_Init();

	if (m_pPostProcessMgr)
	{
		m_pPostProcessMgr->CreateResources();
	}

	RT_GraphicsPipelineBootup();
#if RENDERER_SUPPORT_SCALEFORM
	SF_CreateResources();
#endif
}

void CD3D9Renderer::RT_PrecacheDefaultShaders()
{
#if RENDERER_SUPPORT_SCALEFORM
	SF_PrecacheShaders();
#endif
}

void CD3D9Renderer::SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode)
{
	if (!pCVar)
		return;

	string argText = pArgText;
	ExecuteRenderThreadCommand(
		[=]
		{
			pCVar->Set(argText.c_str());

			if (!bSilentMode)
			{
				if (gEnv->IsEditor())
					gEnv->pLog->LogWithType(ILog::eInputResponse, "%s = %s (Renderer CVar)", pCVar->GetName(), pCVar->GetString());
				else
					gEnv->pLog->LogWithType(ILog::eInputResponse, "    $3%s = $6%s $5(Renderer CVar)", pCVar->GetName(), pCVar->GetString());
			}
		},
		ERenderCommandFlags::None
	);
}

//////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::StartLoadtimeFlashPlayback(ILoadtimeCallback* pCallback)
{
	// make sure we can't access loading mode twice!
	assert(!m_pRT->m_pLoadtimeCallback);
	if (m_pRT->m_pLoadtimeCallback)
	{
		return;
	}

	// TODO: check for r_shadersnocompile to prevent issue with device access from render load thread
	if (pCallback)
	{
		FlushRTCommands(true, true, true);

		CRenderDisplayContext* pDC = GetActiveDisplayContext();
		m_pRT->m_pLoadtimeCallback = pCallback;
		//SetViewport(0, 0, pDC->m_Width, pDC->m_Height);
		m_pRT->RC_StartVideoThread();

		// wait until render thread has fully processed the start of the video
		// to reduce the congestion on the IO reading (make sure nothing else
		// beats the video to actually start reading something from the DVD)
		while (m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Active)
		{
			m_pRT->FlushAndWait();
			CrySleep(0);
		}
	}
}

void CD3D9Renderer::StopLoadtimeFlashPlayback()
{
	if (m_pRT->m_pLoadtimeCallback)
	{
		FlushRTCommands(true, true, true);

		m_pRT->RC_StopVideoThread();

		// wait until render thread has fully processed the shutdown of the loading thread
		while (m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Disabled)
		{
			m_pRT->FlushAndWait();
			CrySleep(0);
		}

		m_pRT->m_pLoadtimeCallback = 0;

		m_pRT->RC_BeginFrame({});
		FillFrame(Col_Black);

#if !defined(STRIP_RENDER_THREAD)
		if(m_pRT->m_CommandsLoading.size() > 0)
		{
			// Blit the accumulated commands from the renderloading thread into the current fill command queue
			// : Currently hacked into the RC_UpdateMaterialConstants command, but will be generalized soon
			void* buf = m_pRT->m_Commands[m_pRT->m_nCurThreadFill].Grow(m_pRT->m_CommandsLoading.size());
			memcpy(buf, &m_pRT->m_CommandsLoading[0], m_pRT->m_CommandsLoading.size());
			m_pRT->m_CommandsLoading.Free();
		}
#endif // !defined(STRIP_RENDER_THREAD)
	}
}

