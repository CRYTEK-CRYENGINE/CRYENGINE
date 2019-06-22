// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: SystemRender.cpp
//  Description: CryENGINE system core
//
//	History:
//	-Jan 6,2004: split from system.cpp
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "System.h"
#include <CryEntitySystem/IEntity.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryRenderer/IScaleform.h>
#include <CrySystem/IProcess.h>
#include "Log.h"
#include "XConsole.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAISystem.h>
#include <CryCore/Platform/CryLibrary.h>
#include <CrySystem/IBudgetingSystem.h>
#include "PhysRenderer.h"
#include <CryScriptSystem/IScriptSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryParticleSystem/IParticles.h>
#include <CryMovie/IMovieSystem.h>
#include <CryThreading/IJobManager.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <CrySystem/ConsoleRegistration.h>

#include "CrySizerStats.h"
#include "CrySizerImpl.h"
#include "VisRegTest.h"
#include <CrySystem/Profilers/IDiskProfiler.h>
#include "Profiling/ProfilingRenderer.h"
#include <CrySystem/ITextModeConsole.h>
#include "HardwareMouse.h"
#include <CryEntitySystem/IEntitySystem.h> // <> required for Interfuscator
#include "NullImplementation/NULLRenderAuxGeom.h"

#include "MiniGUI/MiniGUI.h"
#include "PerfHUD.h"
#include "ThreadInfo.h"

#include <CrySystem/Scaleform/IScaleformHelper.h>
#include <CrySystem/VR/IHMDManager.h>

#include "Statistics.h"

#if CRY_PLATFORM_WINDOWS
#	include <psapi.h> // requires <windows.h>
	LINK_SYSTEM_LIBRARY("psapi.lib")
#endif
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
#	include <dlfcn.h>
#endif

class CMTSafeHeap;

extern CMTSafeHeap* g_pPakHeap;

extern int CryMemoryGetAllocatedSize();

// Remap r_fullscreen to keep legacy functionality, r_WindowType is new desired path
void OnFullscreenStateChanged(ICVar* pFullscreenCVar)
{
	if (ICVar* pCVar = gEnv->pConsole->GetCVar("r_WindowType"))
	{
		if (pFullscreenCVar->GetIVal() != 0)
		{
			pCVar->Set(3);
		}
		else if(pCVar->GetIVal() == 3)
		{
			pCVar->Set(0);
		}
	}
}

// Reflect r_windowType changes to the legacy r_fullscreen CVar
void OnWindowStateChanged(ICVar* pCVar)
{
	if (ICVar* pFullscreenCVar = gEnv->pConsole->GetCVar("r_Fullscreen"))
	{
		pFullscreenCVar->Set(pCVar->GetIVal() == 3 ? 1 : 0);
	}
}

/////////////////////////////////////////////////////////////////////////////////
void CSystem::CreateRendererVars(const SSystemInitParams& startupParams)
{
	m_rIntialWindowSizeRatio = REGISTER_FLOAT("r_InitialWindowSizeRatio", 0.666f, VF_DUMPTODISK,
	                                          "Sets the size ratio of the initial application window in relation to the primary monitor resolution.\n"
	                                          "Usage: r_InitialWindowSizeRatio [1.0/0.666/..]");

	int iFullScreenDefault  = 1;
	int iDisplayInfoDefault = 1;
	int iWidthDefault       = 1280;
	int iHeightDefault      = 720;
#if CRY_PLATFORM_DURANGO
	iWidthDefault  = 1920;
	iHeightDefault = 1080;
#elif CRY_PLATFORM_ORBIS
	iWidthDefault  = 1920;
	iHeightDefault = 1080;
#elif CRY_PLATFORM_WINDOWS
	iFullScreenDefault = 0;
	const float initialWindowSizeRatio = m_rIntialWindowSizeRatio->GetFVal();
	iWidthDefault = static_cast<int>(GetSystemMetrics(SM_CXSCREEN) * initialWindowSizeRatio);
	iHeightDefault = static_cast<int>(GetSystemMetrics(SM_CYSCREEN) * initialWindowSizeRatio);
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_APPLE
	iFullScreenDefault = 0;
#endif

#if defined(RELEASE)
	iDisplayInfoDefault = 0;
#endif

	// load renderer settings from engine.ini
	m_rWidth = REGISTER_INT("r_Width", iWidthDefault, VF_DUMPTODISK,
		"Sets the display width, in pixels.\n"
		"Usage: r_Width [800/1024/..]");
	m_rHeight = REGISTER_INT("r_Height", iHeightDefault, VF_DUMPTODISK,
		"Sets the display height, in pixels.\n"
		"Usage: r_Height [600/768/..]");
	m_rColorBits = REGISTER_INT("r_ColorBits", 32, VF_DUMPTODISK | VF_REQUIRE_APP_RESTART,
		"Sets the color resolution, in bits per pixel. Default is 32.\n"
		"Usage: r_ColorBits [32/24/16/8]");
	m_rDepthBits = REGISTER_INT("r_DepthBits", 24, VF_DUMPTODISK | VF_REQUIRE_APP_RESTART,
		"Sets the depth precision, in bits per pixel. Default is 24.\n"
		"Usage: r_DepthBits [32/24/16]");
	m_rStencilBits = REGISTER_INT("r_StencilBits", 8, VF_DUMPTODISK,
		"Sets the stencil precision, in bits per pixel. Default is 8.\n");

#if CRY_PLATFORM_WINDOWS
	REGISTER_INT("r_overrideDXGIAdapter", -1, VF_REQUIRE_APP_RESTART,
	  "Specifies index of the preferred video adapter to be used for rendering (-1=off, loops until first suitable adapter is found).\n"
	  "Use this to resolve which video card to use if more than one DX11 capable GPU is available in the system.");
#endif

#if CRY_PLATFORM_ANDROID
	const char* p_r_DriverDef = STR_VK_RENDERER;
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_APPLE
	const char* p_r_DriverDef = STR_GL_RENDERER;
#elif CRY_PLATFORM_DURANGO
	const char* p_r_DriverDef = STR_DX11_RENDERER;
#elif CRY_PLATFORM_ORBIS
	const char* p_r_DriverDef = STR_DX11_RENDERER;
#else
	const char* p_r_DriverDef = STR_AUTO_RENDERER;
#endif

	if (startupParams.pCvarsDefault)
	{
		// hack to customize the default value of r_Driver
		SCvarsDefault* pCvarsDefault = startupParams.pCvarsDefault;
		if (pCvarsDefault->sz_r_DriverDef && pCvarsDefault->sz_r_DriverDef[0])
			p_r_DriverDef = startupParams.pCvarsDefault->sz_r_DriverDef;
	}

	m_rDriver = REGISTER_STRING("r_Driver", p_r_DriverDef, VF_DUMPTODISK | VF_REQUIRE_APP_RESTART,
		"Sets the renderer driver ( DX11/DX12/GL/VK/AUTO ).\n"
		"Specify in system.cfg like this: r_Driver = \"DX11\"");

	m_rFullscreen = REGISTER_INT_CB("r_Fullscreen", iFullScreenDefault, VF_DUMPTODISK,
		"Toggles fullscreen mode. Default is 1 in normal game and 0 in DevMode.\n"
		"Usage: r_Fullscreen [0=window/1=fullscreen]", OnFullscreenStateChanged);

	m_rWindowState = REGISTER_INT_CB("r_WindowType", m_rFullscreen->GetIVal() != 0 ? 3 : 0, VF_DUMPTODISK,
		"Changes the type of window for the rendered viewport.\n"
		"Usage: r_WindowType [0=normal window/1=borderless window/2=borderless full screen/3=exclusive full screen]", OnWindowStateChanged);

	m_rFullsceenNativeRes = REGISTER_INT("r_FullscreenNativeRes", 0, VF_DUMPTODISK,
		"Toggles native resolution upscaling.\n"
		"If enabled, scene gets upscaled from specified resolution while UI is rendered in native resolution.");

	m_rDisplayInfo = REGISTER_INT("r_DisplayInfo", iDisplayInfoDefault, VF_RESTRICTEDMODE | VF_DUMPTODISK,
		"Toggles debugging information display.\n"
		"Usage: r_DisplayInfo [0=off/1=show/2=enhanced/3=minimal/4=fps bar/5=heartbeat]");

	m_rDisplayInfoTargetPolygons = REGISTER_INT("r_DisplayInfoTargetPolygons", 0, VF_RESTRICTEDMODE | VF_DUMPTODISK,
		"Specifies the max polygon count where display info will highlight a warning\n"
		"0 disables the warning");

	m_rDisplayInfoTargetDrawCalls = REGISTER_INT("r_DisplayInfoTargetDrawCalls", 0, VF_RESTRICTEDMODE | VF_DUMPTODISK,
		"Specifies the max draw call count where display info will highlight a warning\n"
		"0 disables the warning");

	m_rDisplayInfoTargetFPS = REGISTER_FLOAT("r_displayinfoTargetFPS", 30.0f, VF_RESTRICTEDMODE | VF_DUMPTODISK,
		"Specifies the aimed number of FPS that is considered ideal for the game.\n"
		"The value must be positive and is used to display budgeting information with r_DisplayInfo=3");

	m_rOverscanBordersDrawDebugView = REGISTER_INT("r_OverscanBordersDrawDebugView", 0, VF_RESTRICTEDMODE | VF_DUMPTODISK,
		"Toggles drawing overscan borders.\n"
		"Usage: r_OverscanBordersDrawDebugView [0=off/1=show]");
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderBegin(const SDisplayContextKey& displayContextKey, const SGraphicsPipelineKey& graphicsPipelineKey)
{
	if (m_bIgnoreUpdates)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	CRY_PROFILE_MARKER(PROFILE_SYSTEM, "CSystem::RenderBegin");
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CSystem::RenderBegin");

	bool rndAvail = m_env.pRenderer != 0;

	//////////////////////////////////////////////////////////////////////
	//start the rendering pipeline
	if (rndAvail)
	{
		m_env.pRenderer->BeginFrame(displayContextKey, graphicsPipelineKey);
		gEnv->nMainFrameID = m_env.pRenderer->GetFrameID(false);
	}
	else
	{
		if (m_pNULLRenderAuxGeom)
		{
			m_pNULLRenderAuxGeom->BeginFrame();
		}
		++gEnv->nMainFrameID;
	}

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	if (rndAvail && !gEnv->IsDedicated() && gEnv->pScaleformHelper)
	{
		// render thread IDs might get updated in BeginFrame() so update GFx render layer
		threadID mainThread(0), renderThread(0);
		m_env.pRenderer->GetThreadIDs(mainThread, renderThread);
		gEnv->pScaleformHelper->SetRenderThreadIDs(mainThread, renderThread);
	}
#endif
}

char* PhysHelpersToStr(int iHelpers, char* strHelpers);
int   StrToPhysHelpers(const char* strHelpers);

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderEnd(bool bRenderStats)
{
	if (m_bIgnoreUpdates)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	CRY_PROFILE_MARKER(PROFILE_SYSTEM, "CSystem::RenderEnd");
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CSystem::RenderEnd");

	if (!m_env.pRenderer)
	{
		if (bRenderStats)
		{
			RenderStatistics();
		}
		if (m_pNULLRenderAuxGeom)
		{
#if !defined (_RELEASE)
			if (m_pPhysRenderer)
			{
				RenderPhysicsHelpers();
			}
#endif
			m_pNULLRenderAuxGeom->EndFrame();
		}
		return;
	}
	/*
	   if(m_env.pMovieSystem)
	   m_env.pMovieSystem->Render();
	 */

	GetPlatformOS()->RenderEnd();

#if !defined (_RELEASE)
	// Flush render data and swap buffers.
	m_env.pRenderer->RenderDebug(bRenderStats);
#endif

	RenderJobStats();

#if defined(USE_PERFHUD)
	if (m_pPerfHUD)
		m_pPerfHUD->Draw();
	if (m_pMiniGUI)
		m_pMiniGUI->Draw();
#endif

	if (bRenderStats)
	{
		RenderStatistics();
	}

	if (IConsole* pConsole = GetIConsole())
		pConsole->Draw();

	gEnv->GetJobManager()->SetMainDoneTime(m_env.pTimer->GetAsyncTime());

	m_env.pRenderer->ForceGC(); // XXX Rename this
	m_env.pRenderer->EndFrame();

	if (IConsole* pConsole = GetIConsole())
	{
		// if we have pending cvar calculation, execute it here
		// since we know cvars will be correct here after ->EndFrame().
		if (!pConsole->IsHashCalculated())
			pConsole->CalcCheatVarHash();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderPhysicsHelpers()
{
#if !defined (_RELEASE)
	if (m_bIgnoreUpdates)
		return;

	if (gEnv->pPhysicalWorld && m_pPhysRenderer)
	{
		char str[128];
		if (StrToPhysHelpers(m_p_draw_helpers_str->GetString()) != m_env.pPhysicalWorld->GetPhysVars()->iDrawHelpers)
			m_p_draw_helpers_str->Set(PhysHelpersToStr(m_env.pPhysicalWorld->GetPhysVars()->iDrawHelpers, str));

		m_pPhysRenderer->UpdateCamera(GetViewCamera());
		m_pPhysRenderer->DrawAllHelpers(m_env.pPhysicalWorld);
		m_pPhysRenderer->Flush(m_Time.GetFrameTime());

		RenderPhysicsStatistics(gEnv->pPhysicalWorld);
	}

	if (m_env.pAuxGeomRenderer)
		m_env.pAuxGeomRenderer->Submit();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderPhysicsStatistics(IPhysicalWorld* pWorld)
{
#if !defined(_RELEASE)
	//////////////////////////////////////////////////////////////////////
	// draw physics helpers
	if (pWorld && GetIRenderer())
	{
		const float renderMarginX = 10.0f;
		const float renderMarginY = 10.0f;
		const float lineSize      = 12.0f;
		const float fontSize      = 1.3f;
		ITextModeConsole*  pTMC   = GetITextModeConsole();
		phys_profile_info* pInfos;
		PhysicsVars* pVars = pWorld->GetPhysVars();
		int  i             = -1;
		char msgbuf[512];

		if (pVars->bProfileGroups)
		{
			int j = 0, mask, nGroups = pWorld->GetGroupProfileInfo(pInfos);
			float fColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			if (!pVars->bProfileEntities)
				j = 12;

			for (++i; j < nGroups; j++, i++)
			{
				pInfos[j].nTicksAvg   = (int)(((int64)pInfos[j].nTicksAvg * 15 + pInfos[j].nTicksLast) >> 4);
				mask                  = (pInfos[j].nTicksPeak - pInfos[j].nTicksLast) >> 31;
				mask                 |= (70 - pInfos[j].peakAge) >> 31;
				pInfos[j].nTicksPeak += pInfos[j].nTicksLast - pInfos[j].nTicksPeak & mask;
				pInfos[j].nCallsPeak += pInfos[j].nCallsLast - pInfos[j].nCallsPeak & mask;
				float time     = gEnv->pTimer->TicksToSeconds(pInfos[j].nTicksAvg) * 1000.0f;
				float timeNorm = time * (1.0f / 32);
				fColor[1] = fColor[2] = 1.0f - (max(0.7f, min(1.0f, timeNorm)) - 0.7f) * (1.0f / 0.3f);
				IRenderAuxText::Draw2dLabel(renderMarginX, renderMarginY + i * lineSize, fontSize, fColor, false,
				                            "%s %.2fms/%d (peak %.2fms/%d)", pInfos[j].pName, time, pInfos[j].nCallsLast,
				                            gEnv->pTimer->TicksToSeconds(pInfos[j].nTicksPeak) * 1000.0f, pInfos[j].nCallsPeak);
				pInfos[j].peakAge = pInfos[j].peakAge + 1 & ~mask;
				if (j == nGroups - 3) ++i;
			}
		}
		if (pVars->bProfileFunx)
		{
			int j, mask, nFunx = pWorld->GetFuncProfileInfo(pInfos);
			float fColor[4] = { 0.75f, 0.08f, 0.85f, 1.0f };
			for (j = 0, ++i; j < nFunx; j++, i++)
			{
				mask                  = (pInfos[j].nTicksPeak - pInfos[j].nTicks) >> 31;
				mask                 |= (70 - pInfos[j].peakAge) >> 31;
				pInfos[j].nTicksPeak += pInfos[j].nTicks - pInfos[j].nTicksPeak & mask;
				pInfos[j].nCallsPeak += pInfos[j].nCalls - pInfos[j].nCallsPeak & mask;
				IRenderAuxText::Draw2dLabel(renderMarginX, renderMarginY + i * lineSize, fontSize, fColor, false,
				                            "%s %.2fms/%d (peak %.2fms/%d)", pInfos[j].pName, gEnv->pTimer->TicksToSeconds(pInfos[j].nTicks) * 1000.0f, pInfos[j].nCalls,
				                            gEnv->pTimer->TicksToSeconds(pInfos[j].nTicksPeak) * 1000.0f, pInfos[j].nCallsPeak);
				pInfos[j].peakAge = pInfos[j].peakAge + 1 & ~mask;
				pInfos[j].nCalls  = pInfos[j].nTicks = 0;
			}
		}
		if (pVars->bProfileEntities == 1)
		{
			pe_status_pos sp;
			float fColor[4]  = { 0.3f, 0.6f, 1.0f, 1.0f }, dt;
			int nMaxEntities = (GetIRenderer()->GetOverlayHeight() / (int)lineSize) - 2;
			if (pVars->bProfileGroups)
			{
				nMaxEntities -= pWorld->GetGroupProfileInfo(pInfos) + 2;     // + extra lines to account for spacing between groups
			}
			if (pVars->bProfileFunx)
			{
				nMaxEntities -= pWorld->GetFuncProfileInfo(pInfos) + 1;
			}
			if (nMaxEntities < 0)
			{
				nMaxEntities = 0;
			}
			int j, mask, nEnts = pWorld->GetEntityProfileInfo(pInfos), line0 = ++i;
			if (nEnts > nMaxEntities)
			{
				nEnts = nMaxEntities;
			}
			for(; nEnts > 0 && pInfos[nEnts - 1].nCallsLast + pInfos[nEnts - 1].nCallsAvg < 0.2f; nEnts--)
				;

			if (!pVars->bSingleStepMode)
			{
				for (i = 0; i < nEnts; i++)
				{
					pInfos[i].nTicksAvg = (int)(((int64)pInfos[i].nTicksAvg * 15 + pInfos[i].nTicksLast) >> 4);
					pInfos[i].nCallsAvg = pInfos[i].nCallsAvg * (15.0f / 16) + pInfos[i].nCallsLast * (1.0f / 16);
				}
				phys_profile_info ppi;
				for (i = 0; i < nEnts - 1; i++)
					for (j = nEnts - 1; j > i; j--)
						if (pInfos[j - 1].nTicksAvg < pInfos[j].nTicksAvg)
						{
							ppi           = pInfos[j - 1];
							pInfos[j - 1] = pInfos[j];
							pInfos[j]     = ppi;
						}
			}
			for (i = 0; i < nEnts; i++)
			{
				mask                  = (pInfos[i].nTicksPeak - pInfos[i].nTicksLast) >> 31;
				mask                 |= (70 - pInfos[i].peakAge) >> 31;
				mask                 &= (pVars->bSingleStepMode - 1) >> 31;
				pInfos[i].nTicksPeak += pInfos[i].nTicksLast - pInfos[i].nTicksPeak & mask;
				pInfos[i].nCallsPeak += pInfos[i].nCallsLast - pInfos[i].nCallsPeak & mask;
				cry_sprintf(msgbuf, "%.2fms/%.1f (peak %.2fms/%d) %s (id %d)",
				            dt = gEnv->pTimer->TicksToSeconds(pInfos[i].nTicksAvg) * 1000.0f, pInfos[i].nCallsAvg,
				            gEnv->pTimer->TicksToSeconds(pInfos[i].nTicksPeak) * 1000.0f, pInfos[i].nCallsPeak,
				            pInfos[i].pName ? pInfos[i].pName : "", pInfos[i].id);
				IRenderAuxText::Draw2dLabel(renderMarginX, renderMarginY + (i + line0) * lineSize, fontSize, fColor, false, "%s", msgbuf);
				if (pTMC) pTMC->PutText(0, i, msgbuf);
				IPhysicalEntity* pent = pWorld->GetPhysicalEntityById(pInfos[i].id);
				if (dt > 0.1f && pInfos[i].pName && pent && pent->GetStatus(&sp))
					IRenderAuxText::DrawLabelExF(sp.pos + Vec3(0, 0, sp.BBox[1].z), 1.4f, fColor, true, true, "%s %.2fms", pInfos[i].pName, dt);
				pInfos[i].peakAge = pInfos[i].peakAge + 1 & ~mask;
				//pInfos[i].nCalls=pInfos[i].nTicks = 0;
			}

			if (inrange(m_iJumpToPhysProfileEnt, 0, nEnts))
			{
				ScriptHandle hdl;
				hdl.n = ~0;
				m_env.pScriptSystem->GetGlobalValue("g_localActorId", hdl);
				IEntity* pPlayerEnt   = m_env.pEntitySystem->GetEntity((EntityId)hdl.n);
				IPhysicalEntity* pent = pWorld->GetPhysicalEntityById(pInfos[m_iJumpToPhysProfileEnt - 1].id);
				if (pPlayerEnt && pent)
				{
					pe_params_bbox pbb;
					pent->GetParams(&pbb);
					pPlayerEnt->SetPos((pbb.BBox[0] + pbb.BBox[1]) * 0.5f + Vec3(0, -3.0f, 1.5f));
					pPlayerEnt->SetRotation(Quat(IDENTITY));
				}
				m_iJumpToPhysProfileEnt = 0;
			}
		}
		if (pVars->bProfileEntities == 2)
		{
			int nEnts = m_env.pPhysicalWorld->GetEntityProfileInfo(pInfos);

			for (int j = 0; j < nEnts; j++)
			{
				gEnv->pStatoscope->AddPhysEntity(&pInfos[j]);
			}
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderJobStats()
{
	//enable job system filtering to disable async execution at runtime
	const char* const cpFilterName = m_sys_job_system_filter->GetString();
	if (cpFilterName && *cpFilterName != 0 && *cpFilterName != '0')
	{
		gEnv->GetJobManager()->SetJobFilter(cpFilterName);
	}
	else
	{
		gEnv->GetJobManager()->SetJobFilter(NULL);
	}

	gEnv->GetJobManager()->Update(m_sys_job_system_profiler->GetIVal());
	gEnv->GetJobManager()->SetJobSystemEnabled(m_sys_job_system_enable->GetIVal());
}

//! Update screen and call some important tick functions during loading.
void CSystem::SynchronousLoadingTick(const char* pFunc, int line)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	if (gEnv && gEnv->bMultiplayer && !gEnv->IsEditor())
	{
		//UpdateLoadingScreen currently contains a couple of tick functions that need to be called regularly during the synchronous level loading,
		//when the usual engine and game ticks are suspended.
		UpdateLoadingScreen();

#if defined(MAP_LOADING_SLICING)
		GetISystemScheduler()->SliceAndSleep(pFunc, line);
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
#define CHECK_UPDATE_TIMES
void CSystem::UpdateLoadingScreen()
{
	// Do not update the network thread from here - it will cause context corruption - use the NetworkStallTicker thread system

	if (GetCurrentThreadId() != gEnv->mMainThreadId)
	{
		return;
	}

	// Take this opportunity to update streaming engine.
	static CTimeValue t0;
	CTimeValue t    = gEnv->pTimer->GetAsyncTime();
	float timeDelta = fabs((t - t0).GetSeconds());

#if CRY_PLATFORM_DURANGO
	//XBOX ONE TCR - XR-004-01: 'The title should return from its suspending handler within one second'
	static float s_updateFrequencyInSeconds = 0.5f;
#else
	static float s_updateFrequencyInSeconds = 1.0f;   // Update screen not frequent,not more than once in 1 second
#endif

	if (m_env.pConsole)
	{
		static ICVar* con_showonload = m_env.pConsole->GetCVar("con_showonload");
		if (con_showonload && con_showonload->GetIVal() != 0)
		{
			s_updateFrequencyInSeconds = 0.01f;           // Much more frequent when console is visible on load
		}
	}

	if (timeDelta < s_updateFrequencyInSeconds)   // Update screen not frequently
	{
		return;
	}
#if defined(CHECK_UPDATE_TIMES)
	#if CRY_PLATFORM_DURANGO
	else if (timeDelta > 1.0f)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "CSystem::UpdateLoadingScreen %f seconds since last tick: this is a long delay and a serious risk for failing XBOX ONE TCR - XR-004-01 \n", timeDelta);
	}
	#endif
#endif
	t0 = t;

	if (!m_env.IsEditor() && !m_bQuit)
	{
		if (m_pProgressListener)
		{
			m_pProgressListener->OnLoadingProgress(0);
		}
	}
	// This happens during loading, give windows opportunity to process window messages.
#if CRY_PLATFORM_WINDOWS
	if (m_hWnd && ::IsWindow((HWND)m_hWnd))
	{
		gEnv->pSystem->PumpWindowMessage(true, m_hWnd);
	}

#endif

#if CRY_PLATFORM_DURANGO
	// poll system events on durango
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);
#endif
}

//////////////////////////////////////////////////////////////////////////

void CSystem::DisplayErrorMessage(const char* acMessage,
                                  float fTime,
                                  const float* pfColor,
                                  bool bHardError)
{
	SErrorMessage message;
	message.m_Message = acMessage;
	if (pfColor)
	{
		memcpy(message.m_Color, pfColor, 4 * sizeof(float));
	}
	else
	{
		message.m_Color[0] = 1.0f;
		message.m_Color[1] = 0.0f;
		message.m_Color[2] = 0.0f;
		message.m_Color[3] = 1.0f;
	}
	message.m_HardFailure = bHardError;
	message.m_fTimeToShow = fTime;
	m_ErrorMessages.push_back(message);
}

//! Renders the statistics; this is called from RenderEnd, but if the
//! Host application (Editor) doesn't employ the Render cycle in ISystem,
//! it may call this method to render the essential statistics
//////////////////////////////////////////////////////////////////////////
void CSystem::RenderStatistics()
{
	RenderStats();

	// Render profile info.
	if(m_pProfileRenderer && m_pLegacyProfiler)
		m_pProfileRenderer->Render(m_pLegacyProfiler);

#if defined(USE_DISK_PROFILER)
	if (m_pDiskProfiler)
		m_pDiskProfiler->Update();
#endif

	RenderMemStats();
	RenderMemoryInfo();

	if (gEnv->pScaleformHelper)
	{
		gEnv->pScaleformHelper->RenderFlashInfo();
	}

	// [VR]
	// Update debug info
	if (m_sys_vr_support && m_sys_vr_support->GetIVal())
	{
		GetHmdManager()->Action(IHmdManager::EHmdAction::eHmdAction_DrawInfo);
	}

	RenderThreadInfo();
#if !defined(_RELEASE)
	if (m_sys_enable_budgetmonitoring->GetIVal())
		m_pIBudgetingSystem->MonitorBudget();
#endif
}

//////////////////////////////////////////////////////////////////////
void CSystem::Render(const SGraphicsPipelineKey& graphicsPipelineKey)
{
	if (m_bIgnoreUpdates)
		return;

	//check what is the current process
	if (!m_pProcess)
		return; //should never happen

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CSystem::Render");

	//////////////////////////////////////////////////////////////////////
	//draw
	m_env.p3DEngine->PreWorldStreamUpdate(m_ViewCamera);

	const int nRenderingFlags = SHDF_ZPASS | SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS | SHDF_ALLOW_WATER | SHDF_ALLOW_AO | SHDF_ALLOW_SKY;

	if (m_env.pRenderer)
	{
		if (m_pProcess->GetFlags() & PROC_3DENGINE)
		{
			if (!m_env.IsEditing())  // Editor calls it's own rendering update
			{
				if (m_env.p3DEngine && !m_env.IsFMVPlaying())
				{
					if ((!IsEquivalent(m_ViewCamera.GetPosition(), Vec3(0, 0, 0), VEC_EPSILON) && (!IsLoading())) || // never pass undefined camera to p3DEngine->RenderWorld()
					    m_env.IsDedicated() || m_env.pRenderer->IsPost3DRendererEnabled())
					{
						m_env.p3DEngine->RenderWorld(nRenderingFlags, SRenderingPassInfo::CreateGeneralPassRenderingInfo(graphicsPipelineKey, m_ViewCamera), __FUNCTION__);
					}
					else
					{
						m_env.pRenderer->FillFrame(Col_Black);
					}
				}

#if !defined(_RELEASE)
				if (m_pVisRegTest)
					m_pVisRegTest->AfterRender();

				if (m_env.pAISystem)
					m_env.pAISystem->DebugDraw();
#endif

				if (m_env.pMovieSystem)
					m_env.pMovieSystem->Render();
			}
		}
		else
		{
			m_pProcess->RenderWorld(nRenderingFlags, SRenderingPassInfo::CreateGeneralPassRenderingInfo(graphicsPipelineKey, m_ViewCamera), __FUNCTION__);
		}
	}

	m_env.p3DEngine->WorldStreamUpdate();
#if !defined (_RELEASE) && CRY_PLATFORM_DURANGO
	RenderPhysicsHelpers();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderMemStats()
{
#if !defined(_RELEASE)
	// check for the presence of the system
	if (!m_env.pConsole || !m_env.pRenderer || !m_cvMemStats->GetIVal())
	{
		SAFE_DELETE(m_pMemStats);
		return;
	}

	TickMemStats();

	assert(m_pMemStats);
	m_pMemStats->updateKeys();
	// render the statistics
	{
		CrySizerStatsRenderer StatsRenderer(this, m_pMemStats, m_cvMemStatsMaxDepth->GetIVal(), m_cvMemStatsThreshold->GetIVal());
		StatsRenderer.render((m_env.pRenderer->GetFrameID(false) + 2) % m_cvMemStats->GetIVal() <= 1);
	}
#endif

}

//////////////////////////////////////////////////////////////////////////
void CSystem::RenderStats()
{
	if (!m_env.pRenderer)
	{
		return;
	}

#if defined(ENABLE_PROFILING_CODE)
	#ifndef _RELEASE
	// if we rendered an error message on screen during the last frame, then sleep now
	// to force hard stall for 3sec
	if (m_bHasRenderedErrorMessage && !gEnv->IsEditor() && !IsLoading())
	{
		// DO NOT REMOVE OR COMMENT THIS OUT!
		// If you hit this, then you most likely have invalid (synchronous) file accesses
		// which must be fixed in order to not stall the entire game.
		CrySleep(3000);
		m_bHasRenderedErrorMessage = false;
	}
	#endif

	// render info messages on screen
	float fTextPosX  = 5.0f;
	float fTextPosY  = -10;
	float fTextStepY = 13;

	float fFrameTime = gEnv->pTimer->GetRealFrameTime();
	TErrorMessages::iterator itnext;
	for (TErrorMessages::iterator it = m_ErrorMessages.begin(); it != m_ErrorMessages.end(); it = itnext)
	{
		itnext = it;
		++itnext;
		SErrorMessage& message = *it;

		IRenderAuxText::DrawText(Vec3(fTextPosX, fTextPosY += fTextStepY, 1.0f), 1.4f, message.m_Color, eDrawText_FixedSize | eDrawText_2D | eDrawText_Monospace, message.m_Message.c_str());

		message.m_fTimeToShow -= fFrameTime;

		if (message.m_HardFailure)
			m_bHasRenderedErrorMessage = true;

		if (message.m_fTimeToShow < 0.0f)
		{
			m_ErrorMessages.erase(it);
		}
	}
#endif

	if (!m_env.pConsole)
		return;

#ifndef _RELEASE
	if (m_rOverscanBordersDrawDebugView)
	{
		RenderOverscanBorders();
	}
#endif

	int iDisplayInfo = m_rDisplayInfo->GetIVal();
	if (iDisplayInfo == 0)
		return;

	// Draw engine stats
	if (m_env.p3DEngine)
	{
		// Draw 3dengine stats and get last text cursor position
		float nTextPosX = 101 - 20, nTextPosY = -2, nTextStepY = 3;
		m_env.p3DEngine->DisplayInfo(nTextPosX, nTextPosY, nTextStepY, iDisplayInfo != 1);

#if defined(ENABLE_LW_PROFILERS)
		if (m_rDisplayInfo->GetIVal() == 2)
		{
			IRenderAuxText::TextToScreenF(nTextPosX, nTextPosY += nTextStepY, "SysMem %.1f mb", float(DumpMMStats(false)) / 1024.f);
		}
#endif
	}
}

void CSystem::RenderOverscanBorders()
{
#ifndef _RELEASE

	if (m_env.pRenderer && m_rOverscanBordersDrawDebugView)
	{
		int iOverscanBordersDrawDebugView = m_rOverscanBordersDrawDebugView->GetIVal();
		if (iOverscanBordersDrawDebugView)
		{
			const float uv           = 0.0f;
			const float rot          = 0.0f;
			const int whiteTextureId = m_env.pRenderer->GetWhiteTextureId();

			const float r = 1.0f;
			const float g = 1.0f;
			const float b = 1.0f;
			const float a = 0.2f;

			Vec2 overscanBorders = Vec2(0.0f, 0.0f);
			m_env.pRenderer->EF_Query(EFQ_OverscanBorders, overscanBorders);

			const float overscanBorderWidth  = overscanBorders.x * VIRTUAL_SCREEN_WIDTH;
			const float overscanBorderHeight = overscanBorders.y * VIRTUAL_SCREEN_HEIGHT;

			const float xPos   = overscanBorderWidth;
			const float yPos   = overscanBorderHeight;
			const float width  = VIRTUAL_SCREEN_WIDTH - (2.0f * overscanBorderWidth);
			const float height = VIRTUAL_SCREEN_HEIGHT - (2.0f * overscanBorderHeight);

			//m_env.pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);
			IRenderAuxImage::Draw2dImage(xPos, yPos,
			                             width, height,
			                             whiteTextureId,
			                             uv, uv, uv, uv,
			                             rot,
			                             r, g, b, a);
		}
	}
#endif
}

void CSystem::RenderThreadInfo()
{
#if (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO) && !defined(RELEASE)
	if (g_cvars.sys_display_threads)
	{
		static int maxCPU           = 0;
		const int  maxAvailCPU      = 6;
		static unsigned int maxMask = 0;

		if (maxCPU == 0)
		{
			SYSTEM_INFO sysinfo;
			GetSystemInfo(&sysinfo);
			maxCPU  = sysinfo.dwNumberOfProcessors;
			maxMask = (1 << maxCPU) - 1;
		}

		struct SThreadProcessorInfo
		{
			SThreadProcessorInfo(const string& _name, uint32 _id) : name(_name), id(_id) {}
			string name;
			uint32 id;
		};

		static std::multimap<DWORD, SThreadProcessorInfo> sortetThreads;
		static SThreadInfo::TThreads threads;
		static SThreadInfo::TThreadInfo threadInfo;
		static int frame = 0;

		if ((frame++ % 100) == 0) // update thread info every 100 frame
		{
			threads.clear();
			threadInfo.clear();
			sortetThreads.clear();
			SThreadInfo::OpenThreadHandles(threads, SThreadInfo::TThreadIds(), false);
			SThreadInfo::GetCurrentThreads(threadInfo);
			for (SThreadInfo::TThreads::const_iterator it = threads.begin(); it != threads.end(); ++it)
			{
				DWORD mask = (DWORD)SetThreadAffinityMask(it->Handle, maxMask);
				SetThreadAffinityMask(it->Handle, mask);
				sortetThreads.insert(std::make_pair(mask, SThreadProcessorInfo(threadInfo[it->Id], it->Id)));
			}
			SThreadInfo::CloseThreadHandles(threads);
		}

		float  nX     = 5, nY = 10, dY = 12, dX = 10;
		float  fFSize = 1.2f;
		ColorF col1   = Col_Yellow;
		ColorF col2   = Col_Red;

		for (int i = 0; i < maxCPU; ++i)
			IRenderAuxText::Draw2dLabel(nX + i * dX, nY, fFSize, i < maxAvailCPU ? &col1.r : &col2.r, false, "%i", i + 1);

		nY += dY;
		for (std::multimap<DWORD, SThreadProcessorInfo>::const_iterator it = sortetThreads.begin(); it != sortetThreads.end(); ++it)
		{
			for (int i = 0; i < maxCPU; ++i)
				IRenderAuxText::Draw2dLabel(nX + i * dX, nY, fFSize, i < maxAvailCPU ? &col1.r : &col2.r, false, "%s", it->first & BIT(i) ? "X" : "-");

			IRenderAuxText::Draw2dLabel(nX + dX * maxCPU, nY, fFSize, &col1.r, false, "Thread: %s (0x%X)", it->second.name.c_str(), it->second.id);
			nY += dY;
		}
	}
#endif
}

#if CRY_PLATFORM_WINDOWS && !defined(_LIB)
#	define USE_PE_HEADER 1
#else
#	define USE_PE_HEADER 0
#endif

#if USE_PE_HEADER
#	pragma pack(push,1)
	const struct PEHeader
	{
		DWORD                 signature;
		IMAGE_FILE_HEADER     _head;
		IMAGE_OPTIONAL_HEADER opt_head;
		IMAGE_SECTION_HEADER* section_header;  // actual number in NumberOfSections
	};
#	pragma pack(pop)
#endif

void DrawLabel(float col, float row, const float* fColor, const char* szText)
{
	const float ColumnSize = 11;
	const float RowSize = 11;

	const ColorF color(fColor[0], fColor[1], fColor[2], fColor[3]);
	const float scale = 1.5f;
	const int flags = eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace;

	IRenderAuxText::DrawText(Vec3(ColumnSize * col, RowSize * row, 0.5f), scale, color, flags, szText);

	if (ITextModeConsole* pTC = gEnv->pSystem->GetITextModeConsole())
	{
		pTC->PutText((int)col, (int)(row - 1), szText);
	}
}

void CSystem::RenderMemoryInfo()
{
	// write to log, if just changed (turned on)
	static int memProfileValueOld = 0;
	if (memProfileValueOld != profile_meminfo)
	{
		m_logMemoryInfo = true;
		memProfileValueOld = profile_meminfo;
	}
	if (!profile_meminfo)
		return;

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_DURANGO

	if (!gEnv->pRenderer)
		return;

	const float col = 0;
	const float col1 = col + 20;
	const float col2 = col1 + 20;
#if USE_PE_HEADER
	const float col3 = col2 + 12;
#else
	const float col3 = col2;
#endif
	float col4 = col3 + 12;

	const float HeaderColor[4] = { 0.3f, 1, 1, 1 };
	const float ModuleColor[4] = { 1, 1, 1, 1 };
	const float StaticColor[4] = { 1, 1, 1, 1 };
	const float NumColor[4] = { 1, 0, 1, 1 };
	const float TotalColor[4] = { 1, 1, 1, 1 };
	ILog* pLog = gEnv->pLog;

	float row = 1;
	char szText[128];

	//////////////////////////////////////////////////////////////////////////
	// Show memory usage.
	//////////////////////////////////////////////////////////////////////////
	uint64 memUsage = 0;//CryMemoryGetAllocatedSize();
	int64 totalAll = 0;
	int luaMemUsage = gEnv->pScriptSystem->GetScriptAllocSize();

	row++; // reserve for static.
	row++;
	row++;

	cry_sprintf(szText, "Lua Allocated Memory: %d KB", luaMemUsage / 1024);
	DrawLabel(col, row++, HeaderColor, szText);

	if (m_logMemoryInfo) pLog->Log(szText);
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	DrawLabel(col, row++, HeaderColor, "-----------------------------------------------------------------------------------------------------------------------------------");
	DrawLabel(col, row, HeaderColor, "Module");
	DrawLabel(col1, row, HeaderColor, "Dynamic(MB)");
#if USE_PE_HEADER
	DrawLabel(col2, row, HeaderColor, "Static(KB)");
#endif
	DrawLabel(col3, row, HeaderColor, "Num Allocs");
	DrawLabel(col4, row, HeaderColor, "Total Allocs(KB)");
	float col5 = col4 + 20;
	DrawLabel(col5, row, HeaderColor, "Total Wasted(KB)");
	int totalUsedInModulesStatic = 0;

	row++;

	uint64 totalUsedInModules = 0;
	int countedMemoryModules = 0;
	uint64 totalAllocatedInModules = 0;
	int totalNumAllocsInModules = 0;

	const std::vector<string>& szModules = GetModuleNames();
	const int numModules = szModules.size();

	for (int i = 0; i < numModules; i++)
	{
#if USE_PE_HEADER
		PEHeader* header = nullptr;
#endif

		typedef void(*PFN_MODULEMEMORY)(CryModuleMemoryInfo*);
#ifndef _LIB
		const char* szModule = szModules[i];
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		char path[_MAX_PATH];
		cry_sprintf(path, "./%s", szModule);
#if CRY_PLATFORM_ANDROID
		HMODULE hModule = dlopen(path, RTLD_LAZY);
#else
		HMODULE hModule = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
#endif
#else
		HMODULE hModule = GetModuleHandle(szModule);
#endif
		if (!hModule)
		{
			continue;
		}
		
		PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY) CryGetProcAddress(hModule, "CryModuleGetMemoryInfo");
		if (!fpCryModuleGetAllocatedMemory)
			continue;
#else // _LIB
		PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = &CryModuleGetMemoryInfo;
		const char* szModule = "Unknown";
#endif

#if USE_PE_HEADER
		const IMAGE_DOS_HEADER* dos_head = (IMAGE_DOS_HEADER*)hModule;
		if (dos_head->e_magic != IMAGE_DOS_SIGNATURE)
		{
			// Wrong pointer, not to PE header.
			continue;
		}
		header = (PEHeader*)(const void*)((char*)dos_head + dos_head->e_lfanew);
#endif

		CryModuleMemoryInfo memInfo;
		memset(&memInfo, 0, sizeof(memInfo));
		fpCryModuleGetAllocatedMemory(&memInfo);

		uint64 usedInModule = memInfo.allocated - memInfo.freed;

		uint32 moduleStaticSize = 0;
#if USE_PE_HEADER
		PREFAST_SUPPRESS_WARNING(28199);
		PREFAST_SUPPRESS_WARNING(6001) moduleStaticSize = header->opt_head.SizeOfInitializedData + header->opt_head.SizeOfUninitializedData + header->opt_head.SizeOfCode + header->opt_head.SizeOfHeaders;
#endif
#ifndef _LIB
		if (numModules - 1 == i)
		{
			usedInModule = 46 * 1024 * 1024;
			moduleStaticSize = 0;
		}
#endif

		totalNumAllocsInModules += memInfo.num_allocations;
		totalAllocatedInModules += memInfo.allocated;
		totalUsedInModules += usedInModule;
		countedMemoryModules++;
		memUsage += usedInModule + moduleStaticSize;
#if USE_PE_HEADER
		totalUsedInModulesStatic += moduleStaticSize;
#endif
		string szModuleName = PathUtil::GetFileName(szModule);

		cry_sprintf(szText, "%.19s", szModuleName.c_str());
		DrawLabel(col, row, ModuleColor, szText);
		cry_sprintf(szText, "%9.2f", usedInModule / 1024.0f / 1024.0f);
		DrawLabel(col1, row, StaticColor, szText);
#if USE_PE_HEADER
		cry_sprintf(szText, "%7d", moduleStaticSize / 1024);
		DrawLabel(col2, row, StaticColor, szText);
#endif
		cry_sprintf(szText, "%7d", memInfo.num_allocations);
		DrawLabel(col3, row, NumColor, szText);
		cry_sprintf(szText, "%7" PRIu64, memInfo.allocated / 1024u);

		DrawLabel(col4, row, TotalColor, szText);
#if USE_PE_HEADER
		cry_sprintf(szText, "%7" PRIu64, (memInfo.allocated - memInfo.requested) / 1024u);
		DrawLabel(col5, row, TotalColor, szText);
		if (m_logMemoryInfo)
		{
			pLog->Log("    %20s | Alloc: %6d Kb  |  Num: %7d  |  TotalAlloc: %8I64d KB  | StaticTotal: %6d KB  | Code: %6d KB |  Init. Data: %6d KB  |  Uninit. Data: %6d KB | %6d | %6d/%6d",
				szModule,
				usedInModule / 1024, memInfo.num_allocations, memInfo.allocated / 1024,
				moduleStaticSize / 1024,
				header->opt_head.SizeOfCode / 1024,
				header->opt_head.SizeOfInitializedData / 1024,
				header->opt_head.SizeOfUninitializedData / 1024,
				(uint32)memInfo.CryString_allocated / 1024, (uint32)memInfo.STL_allocated / 1024, (uint32)memInfo.STL_wasted / 1024);
		}
#else
		if (m_logMemoryInfo)
		{
			pLog->Log("    %20s | Alloc: %6d Kb  |  Num: %7d  |  TotalAlloc: %" PRIu64 "KB",
				szModule,
				usedInModule / 1024, memInfo.num_allocations, memInfo.allocated / 1024u);
		}
#endif
		row++;
	}

	DrawLabel(col, row++, HeaderColor, "-----------------------------------------------------------------------------------------------------------------------------------");
	cry_sprintf(szText, "Sum %d Modules", countedMemoryModules);
	DrawLabel(col, row, HeaderColor, szText);
	cry_sprintf(szText, "%9d", totalUsedInModules / 1024 / 1024);
	DrawLabel(col1, row, HeaderColor, szText);
#if USE_PE_HEADER
	cry_sprintf(szText, "%7d", totalUsedInModulesStatic / 1024);
	DrawLabel(col2, row, StaticColor, szText);
#endif
	cry_sprintf(szText, "%7d", totalNumAllocsInModules);
	DrawLabel(col3, row, NumColor, szText);

	cry_sprintf(szText, "%7" PRIu64, totalAllocatedInModules / 1024u);
	DrawLabel(col4, row, TotalColor, szText);
	row++;

#if CRY_PLATFORM_WINDOWS
	PROCESS_MEMORY_COUNTERS ProcessMemoryCounters = { sizeof(ProcessMemoryCounters) };
	GetProcessMemoryInfo(GetCurrentProcess(), &ProcessMemoryCounters, sizeof(ProcessMemoryCounters));
	SIZE_T WorkingSetSize = ProcessMemoryCounters.WorkingSetSize;
	SIZE_T QuotaPagedPoolUsage = ProcessMemoryCounters.QuotaPagedPoolUsage;
	SIZE_T PagefileUsage = ProcessMemoryCounters.PagefileUsage;
	SIZE_T PageFaultCount = ProcessMemoryCounters.PageFaultCount;

	cry_sprintf(szText, "WindowsInfo: PagefileUsage: %u WorkingSetSize: %u, QuotaPagedPoolUsage: %u PageFaultCount: %u\n",
		(uint)PagefileUsage / 1024,
		(uint)WorkingSetSize / 1024,
		(uint)QuotaPagedPoolUsage / 1024,
		(uint)PageFaultCount);

	DrawLabel(col, row++, HeaderColor, "-----------------------------------------------------------------------------------------------------------------------------------");
	DrawLabel(col, row, HeaderColor, szText);
#endif

	if (m_logMemoryInfo)
	{
		pLog->Log("Sum of %d Modules %6d Kb  (Static: %6d Kb)  (Num: %8d) (TotalAlloc: %8I64d KB)", countedMemoryModules, totalUsedInModules / 1024,
			totalUsedInModulesStatic / 1024, totalNumAllocsInModules, totalAllocatedInModules / 1024);
	}

	int memUsageInMB_SysCopyMeshes = 0;
	int memUsageInMB_SysCopyTextures = 0;
	gEnv->pRenderer->EF_Query(EFQ_Alloc_APIMesh, memUsageInMB_SysCopyMeshes);
	gEnv->pRenderer->EF_Query(EFQ_Alloc_APITextures, memUsageInMB_SysCopyTextures);

	totalAll += memUsage;
	totalAll += memUsageInMB_SysCopyMeshes;
	totalAll += memUsageInMB_SysCopyTextures;

	cry_sprintf(szText, "Total Allocated Memory: %" PRId64 " KB (DirectX Textures: %d KB, VB: %d Kb)", totalAll / 1024, memUsageInMB_SysCopyTextures / 1024, memUsageInMB_SysCopyMeshes / 1024);

	DrawLabel(col, 1, HeaderColor, szText);

	m_logMemoryInfo = false;
#endif 
}

#undef USE_PE_HEADER
