// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   RenderThread.cpp: Render thread commands processing.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"

#include <CrySystem/Scaleform/IFlashPlayer.h>
#include <Cry3DEngine/I3DEngine.h>
#include "RenderAuxGeom.h"
#include "IColorGradingControllerInt.h"
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryGame/IGameFramework.h>
#include <CryAnimation/ICryAnimation.h>
#include "PostProcess/PostEffects.h"
#include <CryThreading/IThreadManager.h>

#include <DriverD3D.h>

#include "RenderView.h"

#ifdef STRIP_RENDER_THREAD
	#define m_nCurThreadFill    0
	#define m_nCurThreadProcess 0
#endif

#undef MULTITHREADED_RESOURCE_CREATION

// Only needed together with render thread.
struct SRenderThreadLocalStorage
{
	int currentCommandBuffer;

	SRenderThreadLocalStorage() : currentCommandBuffer(0) {};
};

namespace
{
static SRenderThreadLocalStorage g_threadLocalStorage[2];

static SRenderThreadLocalStorage& g_systemThreadLocalStorage = g_threadLocalStorage[0];   // Has to use index 0 as that's the default TLS value
static SRenderThreadLocalStorage& g_renderThreadLocalStorage = g_threadLocalStorage[1];

TLS_DEFINE(int64, g_threadLocalStorageIndex);   // Default value will be 0 on every TLS implementation

//////////////////////////////////////////////////////////////////////////
// This can be called from any thread
static SRenderThreadLocalStorage* GetRenderThreadLocalStorage()
{
	const int64 index = TLS_GET(int64, g_threadLocalStorageIndex);
	CRY_ASSERT(index >= 0 && index < CRY_ARRAY_COUNT(g_threadLocalStorage));
	return &g_threadLocalStorage[index];
}

//////////////////////////////////////////////////////////////////////////
// Must be called from within Render Thread
static void SetRenderThreadLocalStorage(SRenderThreadLocalStorage* pStorage)
{
	const int64 index = pStorage - g_threadLocalStorage;
	CRY_ASSERT(index >= 0 && index < CRY_ARRAY_COUNT(g_threadLocalStorage));
	TLS_SET(g_threadLocalStorageIndex, index);
}
}

int SRenderThread::GetLocalThreadCommandBufferId()
{
	SRenderThreadLocalStorage* pThreadStorage = GetRenderThreadLocalStorage();
	assert(pThreadStorage);
	return pThreadStorage->currentCommandBuffer;
}

#if CRY_PLATFORM_WINDOWS
HWND SRenderThread::GetRenderWindowHandle()
{
	return (HWND)gRenDev->GetHWND();
}
#endif


#define LOADINGLOCK_COMMANDQUEUE (void)0;

void CRenderThread::ThreadEntry()
{
	SetRenderThreadLocalStorage(&g_renderThreadLocalStorage);

	threadID renderThreadId = CryGetCurrentThreadId();
	gRenDev->m_pRT->m_nRenderThread = renderThreadId;
	CNameTableR::m_nRenderThread = renderThreadId;
	gEnv->pCryPak->SetRenderThreadId(renderThreadId);
	m_started.Set();
	gRenDev->m_pRT->Process();
}

void CRenderThreadLoading::ThreadEntry()
{
	threadID renderLoadingThreadId = CryGetCurrentThreadId();
	gRenDev->m_pRT->m_nRenderThreadLoading = renderLoadingThreadId;
	CNameTableR::m_nRenderThread = renderLoadingThreadId;

	// We aren't interested in file access from the render loading thread, and this
	// would overwrite the real render thread id
	// gEnv->pCryPak->SetRenderThreadId( renderThreadId );
	m_started.Set();
	gRenDev->m_pRT->ProcessLoading();
}

void SRenderThread::SwitchMode(bool bEnableVideo)
{
	if (bEnableVideo)
	{
		assert(IsRenderThread());
		if (m_pThreadLoading)
			return;
#if !defined(STRIP_RENDER_THREAD)
		SSystemGlobalEnvironment* pEnv = iSystem->GetGlobalEnvironment();
		if (pEnv && !pEnv->bTesting && !pEnv->IsEditor() && pEnv->pi.numCoresAvailableToProcess > 1 && CRenderer::CV_r_multithreaded > 0)
		{
			m_pThreadLoading = new CRenderThreadLoading();
		}
		m_eVideoThreadMode = eVTM_Active;
		m_bQuitLoading = false;
		StartRenderLoadingThread();
#endif
	}
	else
	{
		m_eVideoThreadMode = eVTM_ProcessingStop;
	}
}

SRenderThread::SRenderThread()
{
	m_eVideoThreadMode = eVTM_Disabled;
	m_nRenderThreadLoading = 0;
	m_nLevelLoadingThread = 0;
	m_pThreadLoading = 0;
	m_pLoadtimeCallback = 0;
	m_bEndFrameCalled = false;
	m_bBeginFrameCalled = false;
	m_bQuitLoading = false;
#if CRY_PLATFORM_DURANGO
	m_suspendWhileLoadingFlag = 0;
#endif
	Init();
}

threadID CNameTableR::m_nRenderThread = 0;

void SRenderThread::Init()
{
	m_bQuit = false;
#ifndef STRIP_RENDER_THREAD
	m_nCurThreadFill = 0;
	m_nCurThreadProcess = 0;
#endif
	InitFlushCond();

	m_nRenderThread = ::GetCurrentThreadId();
	CNameTableR::m_nRenderThread = m_nRenderThread;
	m_nMainThread = m_nRenderThread;
	m_bSuccessful = true;
	m_pThread = NULL;
	m_fTimeIdleDuringLoading = 0;
	m_fTimeBusyDuringLoading = 0;
#if !defined(STRIP_RENDER_THREAD)
	SSystemGlobalEnvironment* pEnv = iSystem->GetGlobalEnvironment();
	if (pEnv && !pEnv->bTesting && !pEnv->IsDedicated() && !pEnv->IsEditor() && pEnv->pi.numCoresAvailableToProcess > 1 && CRenderer::CV_r_multithreaded > 0)
	{
		m_nCurThreadProcess = 1;
		m_pThread = new CRenderThread();
	}
	#ifndef CONSOLE_CONST_CVAR_MODE
	else
		CRenderer::CV_r_multithreaded = 0;
	#endif
#else//STRIP_RENDER_THREAD
	#ifndef CONSOLE_CONST_CVAR_MODE
	CRenderer::CV_r_multithreaded = 0;
	#endif
#endif//STRIP_RENDER_THREAD
	gRenDev->m_RP.m_nProcessThreadID = threadID(m_nCurThreadProcess);
	gRenDev->m_RP.m_nFillThreadID = threadID(m_nCurThreadFill);

	for (uint32 i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		m_Commands[i].Free();
		m_Commands[i].Create(300 * 1024); // 300 to stop growing in MP levels
		m_Commands[i].SetUse(0);
		gRenDev->m_fTimeWaitForMain[i] = 0;
		gRenDev->m_fTimeWaitForRender[i] = 0;
		gRenDev->m_fTimeProcessedRT[i] = 0;
		gRenDev->m_fTimeProcessedGPU[i] = 0;
	}
	m_eVideoThreadMode = eVTM_Disabled;
}

SRenderThread::~SRenderThread()
{
	QuitRenderLoadingThread();
	QuitRenderThread();
}

//==============================================================================================
// NOTE: Render commands can be added from main thread only

bool SRenderThread::RC_CreateDevice()
{
	LOADING_TIME_PROFILE_SECTION;

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	return gRenDev->RT_CreateDevice();
#else
	if (IsRenderThread())
	{
		return gRenDev->RT_CreateDevice();
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_CreateDevice, 0);
	EndCommand(p);

	FlushAndWait();

	return !IsFailed();
#endif
}

void SRenderThread::RC_ResetDevice()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	gRenDev->RT_Reset();
#else
	if (IsRenderThread())
	{
		gRenDev->RT_Reset();
		return;
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ResetDevice, 0);
	EndCommand(p);

	FlushAndWait();
#endif
}

#if CRY_PLATFORM_DURANGO
void SRenderThread::RC_SuspendDevice()
{
	if (m_eVideoThreadMode != eVTM_Disabled)
	{
		if (!gRenDev->m_bDeviceSuspended)
		{
			m_suspendWhileLoadingEvent.Reset();
			m_suspendWhileLoadingFlag = 1;

			do
			{
				CryLowLatencySleep(1);
			}
			while (m_suspendWhileLoadingFlag != 0);
		}
		return;
	}

	if (IsRenderThread())
	{
		return gRenDev->RT_SuspendDevice();
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SuspendDevice, 0);
	EndCommand(p);

	FlushAndWait();
}

void SRenderThread::RC_ResumeDevice()
{
	if (m_eVideoThreadMode != eVTM_Disabled)
	{
		if (gRenDev->m_bDeviceSuspended)
		{
			//here we've got render thread waiting in event
			//wake em up
			m_suspendWhileLoadingEvent.Set();
		}
		return;
	}

	if (IsRenderThread())
	{
		return gRenDev->RT_ResumeDevice();
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ResumeDevice, 0);
	EndCommand(p);

	FlushAndWait();
}
#endif

void SRenderThread::RC_PreloadTextures()
{
	if (IsRenderThread() || IsLevelLoadingThread())
	{
		return CTexture::RT_Precache();
	}
	LOADINGLOCK_COMMANDQUEUE
	{
		byte* p = AddCommand(eRC_PreloadTextures, 0);
		EndCommand(p);
		FlushAndWait();
	}
}

void SRenderThread::RC_Init()
{
	LOADING_TIME_PROFILE_SECTION;

	if (IsRenderThread())
	{
		return gRenDev->RT_Init();
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_Init, 0);
	EndCommand(p);

	FlushAndWait();
}
void SRenderThread::RC_ShutDown(uint32 nFlags)
{
	if (IsRenderThread())
	{
		return gRenDev->RT_ShutDown(nFlags);
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ShutDown, 4);
	AddDWORD(p, nFlags);

	FlushAndWait();
}

void SRenderThread::RC_ResetGlass()
{
	if (IsRenderThread())
	{
		gRenDev->RT_ResetGlass();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ResetGlass, 0);
	EndCommand(p);
}

void SRenderThread::RC_ResetToDefault()
{
	if (IsRenderThread())
	{
		gRenDev->ResetToDefault();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ResetToDefault, 0);
	EndCommand(p);
}

void SRenderThread::RC_ParseShader(CShader* pSH, uint64 nMaskGen, uint32 flags, CShaderResources* pRes)
{
	if (IsRenderThread())
	{
		return gRenDev->m_cEF.RT_ParseShader(pSH, nMaskGen, flags, pRes);
	}
	else if (IsLevelLoadingThread())
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock);

		TArray<byte>& queue = m_CommandsLoading;
		pSH->AddRef();
		if (pRes)
			pRes->AddRef();
		byte* p = AddCommandTo(eRC_ParseShader, 12 + 2 * sizeof(void*), queue);
		AddPointer(p, pSH);
		AddPointer(p, pRes);
		AddDWORD64(p, nMaskGen);
		AddDWORD(p, flags);
		EndCommandTo(p, queue);
	}
	else
	{
		LOADINGLOCK_COMMANDQUEUE
		pSH->AddRef();
		if (pRes)
			pRes->AddRef();
		TArray<byte>& queue = m_Commands[m_nCurThreadFill];
		byte* p = AddCommandTo(eRC_ParseShader, 12 + 2 * sizeof(void*), queue);
		AddPointer(p, pSH);
		AddPointer(p, pRes);
		AddDWORD64(p, nMaskGen);
		AddDWORD(p, flags);
		EndCommand(p);
	}
}

void SRenderThread::RC_UpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial)
{
	if (IsRenderThread())
	{
		return gRenDev->RT_UpdateShaderItem(pShaderItem);
	}

	LOADINGLOCK_COMMANDQUEUE
	if (pMaterial)
		pMaterial->AddRef();

	if (m_eVideoThreadMode == eVTM_Disabled)
	{
		if (IsLevelLoadingThread())
		{
			AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 

			TArray<byte>& queue = m_CommandsLoading;
			byte* p = AddCommandTo(eRC_UpdateShaderItem, sizeof(SShaderItem*) + sizeof(IMaterial*), queue);
			AddPointer(p, pShaderItem);
			AddPointer(p, pMaterial);
			EndCommandTo(p, queue);
		}
		else
		{
			TArray<byte>& queue = m_Commands[m_nCurThreadFill];
			byte* p = AddCommandTo(eRC_UpdateShaderItem, sizeof(SShaderItem*) + sizeof(IMaterial*), queue);
			AddPointer(p, pShaderItem);
			AddPointer(p, pMaterial);
			EndCommandTo(p, queue);
		}
	}
	else
	{
		// Move command into loading queue, which will be executed in first render frame after loading is done
		byte* p = AddCommandTo(eRC_UpdateShaderItem, sizeof(SShaderItem*) + sizeof(IMaterial*), m_CommandsLoading);
		AddPointer(p, pShaderItem);
		AddPointer(p, pMaterial);
		EndCommandTo(p, m_CommandsLoading);
	}
}

void SRenderThread::RC_RefreshShaderResourceConstants(SShaderItem* pShaderItem, IMaterial* pMaterial)
{
	if (IsRenderThread() || IsLevelLoadingThread())
	{
		return gRenDev->RT_RefreshShaderResourceConstants(pShaderItem);
	}

	LOADINGLOCK_COMMANDQUEUE
	if (pMaterial)
		pMaterial->AddRef();
	if (m_eVideoThreadMode == eVTM_Disabled)
	{
		byte* p = AddCommand(eRC_RefreshShaderResourceConstants, sizeof(SShaderItem*) + sizeof(IMaterial*));
		AddPointer(p, pShaderItem);
		AddPointer(p, pMaterial);
		EndCommand(p);
	}
	// Currently move this into loading queue if video playback is enabled during loading
	else
	{
		byte* p = AddCommandTo(eRC_RefreshShaderResourceConstants, sizeof(SShaderItem*) + sizeof(IMaterial*), m_CommandsLoading);
		AddPointer(p, pShaderItem);
		AddPointer(p, pMaterial);
		EndCommandTo(p, m_CommandsLoading);
	}
}

void SRenderThread::RC_SetShaderQuality(EShaderType eST, EShaderQuality eSQ)
{
	if (IsRenderThread())
	{
		return gRenDev->m_cEF.RT_SetShaderQuality(eST, eSQ);
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SetShaderQuality, 8);
	AddDWORD(p, eST);
	AddDWORD(p, eSQ);
	EndCommand(p);
}

void SRenderThread::RC_UpdateMaterialConstants(CShaderResources* pSR, IShader* pSH)
{
	if (IsRenderThread())
	{
		pSR->RT_UpdateConstants(pSH);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	pSH->AddRef();
	if (pSR)
		pSR->AddRef();

	if (m_eVideoThreadMode == eVTM_Disabled)
	{
		if (IsLevelLoadingThread())
		{
			AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 

			TArray<byte>& queue = m_CommandsLoading;
			byte* p = AddCommandTo(eRC_UpdateMaterialConstants, 2*sizeof(void*), queue);
			AddPointer(p, pSR);
			AddPointer(p, pSH);
			EndCommandTo(p, queue);
		}
		else
		{
			TArray<byte>& queue = m_Commands[m_nCurThreadFill];
			byte* p = AddCommandTo(eRC_UpdateMaterialConstants, 2*sizeof(void*), queue);
			AddPointer(p, pSR);
			AddPointer(p, pSH);
			EndCommandTo(p, queue);
		}
	}
	else
	{
		// Move command into loading queue, which will be executed in first render frame after loading is done
		byte* p = AddCommandTo(eRC_UpdateMaterialConstants, 2 * sizeof(void*), m_CommandsLoading);
		AddPointer(p, pSR);
		AddPointer(p, pSH);
		EndCommandTo(p, m_CommandsLoading);
	}
}

void SRenderThread::RC_ReleaseVBStream(void* pVB, int nStream)
{
	if (IsRenderThread())
	{
		gRenDev->RT_ReleaseVBStream(pVB, nStream);
		return;
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseVBStream, 4 + sizeof(void*));
	AddPointer(p, pVB);
	AddDWORD(p, nStream);
	EndCommand(p);
}

void SRenderThread::RC_ForceMeshGC(bool instant, bool wait)
{
	LOADING_TIME_PROFILE_SECTION;

	if (IsRenderThread())
	{
		CRenderMesh::Tick(instant ? MAX_RELEASED_MESH_FRAMES : 1);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ForceMeshGC, 0);
	EndCommand(p);

	if (instant)
	{
		if (wait)
			FlushAndWait();
		else
			SyncMainWithRender();
	}
}

void SRenderThread::RC_DevBufferSync()
{
	LOADING_TIME_PROFILE_SECTION;

	if (IsRenderThread())
	{
		gRenDev->m_DevBufMan.Sync(gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_nFrameUpdateID);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_DevBufferSync, 0);
	EndCommand(p);
}

void SRenderThread::RC_ReleaseGraphicsPipeline()
{
	if (IsRenderThread())
	{
		gRenDev->RT_GraphicsPipelineShutdown();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseGraphicsPipeline, 0);
	EndCommand(p);
}

void SRenderThread::RC_ResetDeviceObjectFactory()
{
	if (IsRenderThread())
	{
		gRenDev->RT_ResetDeviceObjectFactory();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ResetDeviceObjectFactory, 0);
	EndCommand(p);
}

void SRenderThread::RC_ReleasePostEffects()
{
	if (IsRenderThread())
	{
		if (gRenDev->m_pPostProcessMgr)
			gRenDev->m_pPostProcessMgr->ReleaseResources();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleasePostEffects, 0);
	EndCommand(p);
}

void SRenderThread::RC_ResetPostEffects(bool bOnSpecChange)
{
	if (IsRenderThread())
	{
		if (gRenDev->m_pPostProcessMgr)
		{
			gRenDev->m_pPostProcessMgr->Reset(bOnSpecChange);
		}
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(bOnSpecChange ? eRC_ResetPostEffectsOnSpecChange : eRC_ResetPostEffects, 0);
	EndCommand(p);
	FlushAndWait();
}

void SRenderThread::RC_DisableTemporalEffects()
{
	if (IsRenderThread())
	{
		return gRenDev->RT_DisableTemporalEffects();
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_DisableTemporalEffects, 0);
	EndCommand(p);
}

void SRenderThread::RC_UpdateTextureRegion(CTexture* pTex, byte* data, int nX, int nY, int nZ, int USize, int VSize, int ZSize, ETEX_Format eTFSrc)
{
	if (IsRenderThread())
	{
		return pTex->RT_UpdateTextureRegion(data, nX, nY, nZ, USize, VSize, ZSize, eTFSrc);
	}

	LOADINGLOCK_COMMANDQUEUE
	int nSize = CTexture::TextureDataSize(USize, VSize, ZSize, pTex->GetNumMips(), 1, eTFSrc);
	byte* pData = new byte[nSize];
	cryMemcpy(pData, data, nSize);
	pTex->AddRef();

	if (m_eVideoThreadMode == eVTM_Disabled)
	{
		if (IsLevelLoadingThread())
		{
			AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 

			TArray<byte>& queue = m_CommandsLoading;
			byte* p = AddCommandTo(eRC_UpdateTexture, 28+2*sizeof(void*), queue);
			AddPointer(p, pTex);
			AddPointer(p, pData);
			AddDWORD(p, nX);
			AddDWORD(p, nY);
			AddDWORD(p, nZ);
			AddDWORD(p, USize);
			AddDWORD(p, VSize);
			AddDWORD(p, ZSize);
			AddDWORD(p, eTFSrc);
			EndCommandTo(p, queue);
		}
		else
		{
			TArray<byte>& queue = m_Commands[m_nCurThreadFill];
			byte* p = AddCommandTo(eRC_UpdateTexture, 28+2*sizeof(void*), queue);
			AddPointer(p, pTex);
			AddPointer(p, pData);
			AddDWORD(p, nX);
			AddDWORD(p, nY);
			AddDWORD(p, nZ);
			AddDWORD(p, USize);
			AddDWORD(p, VSize);
			AddDWORD(p, ZSize);
			AddDWORD(p, eTFSrc);
			EndCommandTo(p, queue);
		}
	}
	else
	{
		// Move command into loading queue, which will be executed in first render frame after loading is done
		byte* p = AddCommandTo(eRC_UpdateTexture, 28 + 2 * sizeof(void*), m_CommandsLoading);
		AddPointer(p, pTex);
		AddPointer(p, pData);
		AddDWORD(p, nX);
		AddDWORD(p, nY);
		AddDWORD(p, nZ);
		AddDWORD(p, USize);
		AddDWORD(p, VSize);
		AddDWORD(p, ZSize);
		AddDWORD(p, eTFSrc);
		EndCommandTo(p, m_CommandsLoading);
	}
}

bool SRenderThread::RC_DynTexUpdate(SDynTexture* pTex, int nNewWidth, int nNewHeight)
{
	if (IsRenderThread())
	{
		return pTex->RT_Update(nNewWidth, nNewHeight);
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_DynTexUpdate, 8 + sizeof(void*));
	AddPointer(p, pTex);
	AddDWORD(p, nNewWidth);
	AddDWORD(p, nNewHeight);
	EndCommand(p);

	return true;
}

bool SRenderThread::RC_DynTexSourceUpdate(IDynTextureSourceImpl* pSrc, float fDistance)
{
	return true;
}

void SRenderThread::RC_EntityDelete(IRenderNode* pRenderNode)
{
	if (IsRenderThread())
	{
		return SDynTexture_Shadow::RT_EntityDelete(pRenderNode);
	}

	if (IsLevelLoadingThread())
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 

		TArray<byte>& queue = m_CommandsLoading;
		byte* p = AddCommandTo(eRC_EntityDelete, sizeof(void*), queue);
		AddPointer(p, pRenderNode);
		EndCommandTo(p, queue);
	}
	else
	{
		LOADINGLOCK_COMMANDQUEUE
		TArray<byte>& queue = m_Commands[m_nCurThreadFill];
		byte* p = AddCommandTo(eRC_EntityDelete, sizeof(void*), queue);
		AddPointer(p, pRenderNode);
		EndCommandTo(p, queue);
	}
}

void SRenderThread::RC_TexBlurAnisotropicVertical(CTexture* Tex, float fAnisoScale)
{
	if (IsRenderThread())
	{
		CStandardGraphicsPipeline& gp = gcpRendD3D->GetGraphicsPipeline();
		gp.ExecuteAnisotropicVerticalBlur(Tex, 1, 8 * max(1.0f - min(fAnisoScale / 100.0f, 1.0f), 0.2f), 1, false);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_TexBlurAnisotropicVertical, 4 + sizeof(void*));
	AddPointer(p, Tex);
	AddFloat(p, fAnisoScale);
	EndCommand(p);
}

bool SRenderThread::RC_CreateDeviceTexture(CTexture* pTex, const void* pData[])
{
#if !defined(MULTITHREADED_RESOURCE_CREATION)
	if (IsLevelLoadingThread() || IsRenderThread())
#endif
	{
		return pTex->RT_CreateDeviceTexture(pData);
	}

	if (pTex->IsAsyncDevTexCreation())
		return !IsFailed();

	LOADINGLOCK_COMMANDQUEUE
	// double nullptr terminator
	int len = 0; if (pData) while (pData[len] || pData[len + 1]) ++len;
	byte* p = AddCommand(eRC_CreateDeviceTexture, 4 + (len + 1) * sizeof(void*));
	AddPointer(p, pTex);
	AddDWORD(p, len);
	while (--len >= 0)
		AddPointer(p, pData[len]);
	EndCommand(p);
	FlushAndWait();

	return !IsFailed();
}

bool SRenderThread::RC_CreateDeviceTexture(CTexture* pTex, D3DResource* pNatTex)
{
#if !defined(MULTITHREADED_RESOURCE_CREATION)
	if (IsLevelLoadingThread() || IsRenderThread())
#endif
	{
		return pTex->RT_CreateDeviceTexture(pNatTex);
	}

	if (pTex->IsAsyncDevTexCreation())
		return !IsFailed();

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_CreateDeviceTexture, 4 + 2 * sizeof(void*));
	AddPointer(p, pTex);
	AddDWORD(p, 0xFFFFFFFFU);
	AddPointer(p, pNatTex);
	EndCommand(p);
	FlushAndWait();

	return !IsFailed();
}

void SRenderThread::RC_CopyDataToTexture(
  void* pkVoid, unsigned int uiStartMip, unsigned int uiEndMip)
{
	if (IsRenderThread())
	{
		CTexture* pkTexture = (CTexture*) pkVoid;
		pkTexture->StreamCopyMipsTexToMem(uiStartMip, uiEndMip, true, NULL);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_CopyDataToTexture, 8 + sizeof(void*));
	AddPointer(p, pkVoid);
	AddDWORD(p, uiStartMip);
	AddDWORD(p, uiEndMip);
	EndCommand(p);
	// -- kenzo: removing this causes crashes because the texture
	// might have already been destroyed. This needs to be fixed
	// somehow that the createtexture doesn't require the renderthread
	// (PC only issue)
	FlushAndWait();
}

void SRenderThread::RC_ClearTarget(void* pkVoid, const ColorF& kColor)
{
	if (IsRenderThread())
	{
		CTexture* pkTexture = (CTexture*) pkVoid;
		gRenDev->RT_ClearTarget(pkTexture, kColor);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ClearTarget, sizeof(void*) + sizeof(ColorF));
	AddPointer(p, pkVoid);
	AddColor(p, kColor);
	EndCommand(p);
	FlushAndWait();
}

void SRenderThread::RC_CreateResource(SResourceAsync* pRes)
{
	if (IsRenderThread() || IsLevelLoadingThread())
	{
		gRenDev->RT_CreateResource(pRes);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_CreateResource, sizeof(void*));
	AddPointer(p, pRes);
	EndCommand(p);
}

void SRenderThread::RC_StartVideoThread()
{
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_StartVideoThread, 0);
	EndCommand(p);
}

void SRenderThread::RC_StopVideoThread()
{
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_StopVideoThread, 0);
	EndCommand(p);
}

void SRenderThread::RC_PrecacheShader(CShader* pShader, SShaderCombination& cmb, bool bForce, CShaderResources* pRes)
{
	if (IsRenderLoadingThread())
	{
		// Move command into loading queue, which will be executed in first render frame after loading is done
		byte* p = AddCommandTo(eRC_PrecacheShader, sizeof(void*) * 2 + 8 + sizeof(SShaderCombination), m_CommandsLoading);
		pShader->AddRef();
		if (pRes)
			pRes->AddRef();
		AddPointer(p, pShader);
		memcpy(p, &cmb, sizeof(cmb));
		p += sizeof(SShaderCombination);
		AddDWORD(p, bForce);
		AddPointer(p, pRes);
		EndCommandTo(p, m_CommandsLoading);
	}
	else if (IsRenderThread())
	{
		pShader->mfPrecache(cmb, bForce, pRes);
	}
	else if (IsLevelLoadingThread())
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 

		TArray<byte>& queue = m_CommandsLoading;
		byte* p = AddCommandTo(eRC_PrecacheShader, sizeof(void*) * 2 + 8 + sizeof(SShaderCombination), queue);
		pShader->AddRef();
		if (pRes)
			pRes->AddRef();
		AddPointer(p, pShader);
		memcpy(p, &cmb, sizeof(cmb));
		p += sizeof(SShaderCombination);
		AddDWORD(p, bForce);
		AddPointer(p, pRes);
		EndCommandTo(p, queue);
	}
	else
	{
		TArray<byte>& queue = m_Commands[m_nCurThreadFill];
		byte* p = AddCommandTo(eRC_PrecacheShader, sizeof(void*) * 2 + 8 + sizeof(SShaderCombination), queue);
		pShader->AddRef();
		if (pRes)
			pRes->AddRef();
		AddPointer(p, pShader);
		memcpy(p, &cmb, sizeof(cmb));
		p += sizeof(SShaderCombination);
		AddDWORD(p, bForce);
		AddPointer(p, pRes);
		EndCommandTo(p, queue);
	}
}

void SRenderThread::RC_ReleaseShaderResource(CShaderResources* pRes)
{
	if (IsRenderLoadingThread())
	{
		// Move command into loading queue, which will be executed in first render frame after loading is done
		byte* p = AddCommandTo(eRC_ReleaseShaderResource, sizeof(void*), m_CommandsLoading);
		AddPointer(p, pRes);
		EndCommandTo(p, m_CommandsLoading);
	}
	else if (IsRenderThread())
	{
		if (pRes)
			pRes->RT_Release();
		return;
	}
	else if (IsLevelLoadingThread())
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 

		TArray<byte>& queue = m_CommandsLoading;
		byte* p = AddCommandTo(eRC_ReleaseShaderResource, sizeof(void*), queue);
		AddPointer(p, pRes);
		EndCommandTo(p, queue);
	}
	else
	{
		TArray<byte>& queue = m_Commands[m_nCurThreadFill];
		byte* p = AddCommandTo(eRC_ReleaseShaderResource, sizeof(void*), queue);
		AddPointer(p, pRes);
		EndCommandTo(p, queue);
	}
}

void SRenderThread::RC_ReleaseBaseResource(CBaseResource* pRes)
{
	if (IsRenderLoadingThread())
	{
		// Move command into loading queue, which will be executed in first render frame after loading is done
		byte* p = AddCommandTo(eRC_ReleaseBaseResource, sizeof(void*), m_CommandsLoading);
		AddPointer(p, pRes);
		EndCommandTo(p, m_CommandsLoading);
	}
	else if (IsRenderThread())
	{
		SAFE_DELETE(pRes);
		return;
	}
	else if (IsLevelLoadingThread())
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 
		TArray<byte>& queue = m_CommandsLoading;
		byte* p = AddCommandTo(eRC_ReleaseBaseResource, sizeof(void*), queue);
		AddPointer(p, pRes);
		EndCommandTo(p, queue);
	}
	else
	{
		TArray<byte>& queue = m_Commands[m_nCurThreadFill];
		byte* p = AddCommandTo(eRC_ReleaseBaseResource, sizeof(void*), queue);
		AddPointer(p, pRes);
		EndCommandTo(p, queue);
	}
}

void SRenderThread::RC_ReleaseSurfaceResource(SDepthTexture* pRes)
{
	if (IsRenderThread() || IsLevelLoadingThread())
	{
		if (pRes)
		{
			pRes->Release(true);
		}
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseSurfaceResource, sizeof(void*));
	AddPointer(p, pRes);
	EndCommand(p);
}

void SRenderThread::RC_ReleaseResource(SResourceAsync* pRes)
{
	if (IsRenderThread() || IsLevelLoadingThread())
	{
		gRenDev->RT_ReleaseResource(pRes);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseResource, sizeof(void*));
	AddPointer(p, pRes);
	EndCommand(p);
}

void SRenderThread::RC_ReleaseOptics(IOpticsElementBase* pOpticsElement)
{
	if (IsRenderThread() || IsLevelLoadingThread())
	{
		gRenDev->RT_ReleaseOptics(pOpticsElement);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseOptics, sizeof(void*));
	pOpticsElement->AddRef();
	AddPointer(p, pOpticsElement);
	EndCommand(p);
}

void SRenderThread::RC_UnbindTMUs()
{
	if (IsRenderThread())
	{
		gRenDev->RT_UnbindTMUs();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_UnbindTMUs, 0);
	EndCommand(p);
}

void SRenderThread::RC_UnbindResources()
{
	if (IsRenderThread())
	{
		gRenDev->RT_UnbindResources();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_UnbindResources, 0);
	EndCommand(p);
}

void SRenderThread::RC_ReleaseRenderResources(uint32 nFlags)
{
	if (IsRenderThread())
	{
		gRenDev->RT_ReleaseRenderResources(nFlags);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseRenderResources, sizeof(uint32));
	AddDWORD(p, nFlags);
	EndCommand(p);
}
void SRenderThread::RC_CreateRenderResources()
{
	LOADING_TIME_PROFILE_SECTION;
	if (IsRenderThread())
	{
		gRenDev->RT_CreateRenderResources();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_CreateRenderResources, 0);
	EndCommand(p);
}

void SRenderThread::RC_CreateSystemTargets()
{
	LOADING_TIME_PROFILE_SECTION;
	if (IsRenderThread())
	{
		CTexture::CreateSystemTargets();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_CreateSystemTargets, 0);
	EndCommand(p);
}

void SRenderThread::RC_PrecacheDefaultShaders()
{
	LOADING_TIME_PROFILE_SECTION;
	if (IsRenderThread())
	{
		gRenDev->RT_PrecacheDefaultShaders();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PrecacheDefaultShaders, 0);
	EndCommand(p);
}

void SRenderThread::RC_RelinkTexture(CTexture* pTex)
{
	if (IsRenderThread() || IsLevelLoadingThread())
	{
		pTex->RT_Relink();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_RelinkTexture, sizeof(void*));
	AddPointer(p, pTex);
	EndCommand(p);
}

void SRenderThread::RC_UnlinkTexture(CTexture* pTex)
{
	if (IsRenderThread() || IsLevelLoadingThread())
	{
		pTex->RT_Unlink();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_UnlinkTexture, sizeof(void*));
	AddPointer(p, pTex);
	EndCommand(p);
}

bool SRenderThread::RC_CheckUpdate2(CRenderMesh* pMesh, CRenderMesh* pVContainer, InputLayoutHandle eVF, uint32 nStreamMask)
{
	if (IsRenderThread())
	{
		return pMesh->RT_CheckUpdate(pVContainer, eVF, nStreamMask);
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_UpdateMesh2, 8 + 2 * sizeof(void*));
	AddPointer(p, pMesh);
	AddPointer(p, pVContainer);
	AddDWORD(p, eVF);
	AddDWORD(p, nStreamMask);
	EndCommand(p);

	FlushAndWait();

	return !IsFailed();
}

void SRenderThread::RC_ReleaseCB(void* pCB)
{

	if (IsRenderLoadingThread())
	{
		// Move command into loading queue, which will be executed in first render frame after loading is done
		byte* p = AddCommandTo(eRC_ReleaseCB, sizeof(void*), m_CommandsLoading);
		AddPointer(p, pCB);
		EndCommandTo(p, m_CommandsLoading);
	}
	else if (IsRenderThread())
	{
		gRenDev->RT_ReleaseCB(pCB);
		return;
	}
	else if (IsLevelLoadingThread())
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 
		TArray<byte>& queue = m_CommandsLoading;
		byte* p = AddCommandTo(eRC_ReleaseCB, sizeof(void*), queue);
		AddPointer(p, pCB);
		EndCommandTo(p, queue);
	}
	else
	{
		TArray<byte>& queue = m_Commands[m_nCurThreadFill];
		byte* p = AddCommandTo(eRC_ReleaseCB, sizeof(void*), queue);
		AddPointer(p, pCB);
		EndCommandTo(p, queue);
	}

}

void SRenderThread::RC_ReleaseRS(std::shared_ptr<CDeviceResourceSet>& pRS)
{
	const size_t commandSize = sizeof(std::shared_ptr<CDeviceResourceSet> );

	if (IsRenderLoadingThread())
	{
		byte* p = AddCommandTo(eRC_ReleaseRS, commandSize, m_CommandsLoading);
		(::new((void*)p) std::shared_ptr<CDeviceResourceSet>())->swap(pRS);

		p += commandSize;
		EndCommandTo(p, m_CommandsLoading);
	}
	else if (IsRenderThread())
	{
		gRenDev->RT_ReleaseRS(pRS);
		return;
	}
	else
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_ReleaseRS, commandSize);
		(::new((void*)p) std::shared_ptr<CDeviceResourceSet>())->swap(pRS);

		p += commandSize;
		EndCommand(p);
	}
}

void SRenderThread::RC_ReleaseVB(buffer_handle_t nID)
{
	if (IsRenderThread())
	{
		gRenDev->m_DevBufMan.Destroy(nID);
		return;
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseVB, sizeof(buffer_handle_t));
	AddDWORD64(p, nID);
	EndCommand(p);
}
void SRenderThread::RC_ReleaseIB(buffer_handle_t nID)
{
	if (IsRenderThread())
	{
		gRenDev->m_DevBufMan.Destroy(nID);
		return;
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseIB, sizeof(buffer_handle_t));
	AddDWORD64(p, nID);
	EndCommand(p);
}

void SRenderThread::RC_DrawDynVB(SVF_P3F_C4B_T2F* pBuf, uint16* pInds, int nVerts, int nInds, const PublicRenderPrimitiveType nPrimType)
{
	if (IsRenderThread())
	{
		gRenDev->RT_DrawDynVB(pBuf, pInds, nVerts, nInds, nPrimType);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_DrawDynVB, Align4(20 + sizeof(SVF_P3F_C4B_T2F) * nVerts + sizeof(uint16) * nInds));
	AddData(p, pBuf, sizeof(SVF_P3F_C4B_T2F) * nVerts);
	AddData(p, pInds, sizeof(uint16) * nInds);
	AddDWORD(p, nVerts);
	AddDWORD(p, nInds);
	AddDWORD(p, (int)nPrimType);
	EndCommand(p);
}

void SRenderThread::RC_Draw2dImageStretchMode(bool bStretch)
{
	if (IsRenderThread())
	{
		gRenDev->RT_Draw2dImageStretchMode(bStretch);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_Draw2dImageStretchMode, sizeof(DWORD));
	AddDWORD(p, (DWORD)bStretch);
	EndCommand(p);
}

void SRenderThread::RC_Draw2dImage(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, float angle, float r, float g, float b, float a, float z)
{
	DWORD col = D3DRGBA(r, g, b, a);

	if (IsRenderThread())
	{
		// don't render using fixed function pipeline when video mode is active
		if (m_eVideoThreadMode == eVTM_Disabled)
			gRenDev->RT_Draw2dImage(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, angle, col, z);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_Draw2dImage, 44 + sizeof(void*));
	AddFloat(p, xpos);
	AddFloat(p, ypos);
	AddFloat(p, w);
	AddFloat(p, h);
	AddPointer(p, pTexture);
	AddFloat(p, s0);
	AddFloat(p, t0);
	AddFloat(p, s1);
	AddFloat(p, t1);
	AddFloat(p, angle);
	AddDWORD(p, col);
	AddFloat(p, z);
	EndCommand(p);
}

void SRenderThread::RC_Push2dImage(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, float angle, float r, float g, float b, float a, float z, float stereoDepth)
{
	DWORD col = D3DRGBA(r, g, b, a);

	if (IsRenderThread())
	{
		gRenDev->RT_Push2dImage(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, angle, col, z, stereoDepth);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_Push2dImage, 48 + sizeof(void*));
	AddFloat(p, xpos);
	AddFloat(p, ypos);
	AddFloat(p, w);
	AddFloat(p, h);
	AddPointer(p, pTexture);
	AddFloat(p, s0);
	AddFloat(p, t0);
	AddFloat(p, s1);
	AddFloat(p, t1);
	AddFloat(p, angle);
	AddDWORD(p, col);
	AddFloat(p, z);
	AddFloat(p, stereoDepth);
	EndCommand(p);
}

void SRenderThread::RC_PushUITexture(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, CTexture* pTarget, float r, float g, float b, float a)
{
	DWORD col = D3DRGBA(r, g, b, a);

	if (IsRenderThread())
	{
		gRenDev->RT_PushUITexture(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, pTarget, col);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PushUITexture, 36 + 2 * sizeof(void*));
	AddFloat(p, xpos);
	AddFloat(p, ypos);
	AddFloat(p, w);
	AddFloat(p, h);
	AddPointer(p, pTexture);
	AddFloat(p, s0);
	AddFloat(p, t0);
	AddFloat(p, s1);
	AddFloat(p, t1);
	AddPointer(p, pTarget);
	AddDWORD(p, col);
	EndCommand(p);
}

void SRenderThread::RC_Draw2dImageList()
{
	if (IsRenderThread())
	{
		gRenDev->RT_Draw2dImageList();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_Draw2dImageList, 0);
	EndCommand(p);
}

void SRenderThread::RC_DrawImageWithUV(float xpos, float ypos, float z, float w, float h, int textureid, float* s, float* t, float r, float g, float b, float a, bool filtered)
{
	DWORD col = D3DRGBA(r, g, b, a);
	if (IsRenderThread())
	{
		gRenDev->RT_DrawImageWithUV(xpos, ypos, z, w, h, textureid, s, t, col, filtered);
		return;
	}

	int i;

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_DrawImageWithUV, 32 + 8 * 4);
	AddFloat(p, xpos);
	AddFloat(p, ypos);
	AddFloat(p, z);
	AddFloat(p, w);
	AddFloat(p, h);
	AddDWORD(p, textureid);
	for (i = 0; i < 4; i++)
	{
		AddFloat(p, s[i]);
	}
	for (i = 0; i < 4; i++)
	{
		AddFloat(p, t[i]);
	}
	AddDWORD(p, col);
	AddDWORD(p, filtered);
	EndCommand(p);
}

void SRenderThread::RC_SetState(int State, int AlphaRef)
{
	if (IsRenderThread())
	{
		gRenDev->FX_SetState(State, AlphaRef);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SetState, 8);
	AddDWORD(p, State);
	AddDWORD(p, AlphaRef);
	EndCommand(p);
}

void SRenderThread::RC_PushWireframeMode(int nMode)
{
	if (IsRenderThread())
	{
		gRenDev->FX_PushWireframeMode(nMode);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PushWireframeMode, 4);
	AddDWORD(p, nMode);
	EndCommand(p);
}

void SRenderThread::RC_PopWireframeMode()
{
	if (IsRenderThread())
	{
		gRenDev->FX_PopWireframeMode();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PopWireframeMode, 0);
	EndCommand(p);
}

void SRenderThread::RC_SetCull(int nMode)
{
	if (IsRenderThread())
	{
		gRenDev->RT_SetCull(nMode);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SetCull, 4);
	AddDWORD(p, nMode);
	EndCommand(p);
}

void SRenderThread::RC_SelectGPU(int nGPU)
{
	if (IsRenderThread())
	{
		gRenDev->RT_SelectGPU(nGPU);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SelectGPU, 4);
	AddDWORD(p, nGPU);
	EndCommand(p);
}

void SRenderThread::RC_SetScissor(bool bEnable, int sX, int sY, int sWdt, int sHgt)
{
	if (IsRenderThread())
	{
		gRenDev->RT_SetScissor(bEnable, sX, sY, sWdt, sHgt);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SetScissor, sizeof(DWORD) * 5);
	AddDWORD(p, bEnable);
	AddDWORD(p, sX);
	AddDWORD(p, sY);
	AddDWORD(p, sWdt);
	AddDWORD(p, sHgt);
	EndCommand(p);
}

void SRenderThread::RC_PushProfileMarker(char* label)
{
	if (IsRenderLoadingThread())
	{
		return;
	}
	if (IsRenderThread())
	{
		gRenDev->SetProfileMarker(label, CRenderer::ESPM_PUSH);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PushProfileMarker, sizeof(void*));
	AddPointer(p, label);
	EndCommand(p);
}

void SRenderThread::RC_PopProfileMarker(char* label)
{
	if (IsRenderLoadingThread())
	{
		return;
	}
	if (IsRenderThread())
	{
		gRenDev->SetProfileMarker(label, CRenderer::ESPM_POP);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PopProfileMarker, sizeof(void*));
	AddPointer(p, label);
	EndCommand(p);
}

void SRenderThread::RC_ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX, int nScaledY)
{
	if (IsRenderThread())
	{
		gRenDev->RT_ReadFrameBuffer(pRGB, nImageX, nSizeX, nSizeY, eRBType, bRGBA, nScaledX, nScaledY);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReadFrameBuffer, 28 + sizeof(void*));
	AddPointer(p, pRGB);
	AddDWORD(p, nImageX);
	AddDWORD(p, nSizeX);
	AddDWORD(p, nSizeY);
	AddDWORD(p, eRBType);
	AddDWORD(p, bRGBA);
	AddDWORD(p, nScaledX);
	AddDWORD(p, nScaledY);
	EndCommand(p);

	FlushAndWait();
}

void SRenderThread::RC_SetCamera()
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		size_t size = sizeof(Matrix44) * 3;
		byte* pData = AddCommand(eRC_SetCamera, Align4(size));

		gRenDev->GetProjectionMatrix((float*)&pData[0]);
		gRenDev->GetModelViewMatrix((float*)&pData[sizeof(Matrix44)]);
		gRenDev->GetCameraZeroMatrix((float*)&pData[sizeof(Matrix44) * 2]);

		if (gRenDev->m_RP.m_TI[m_nCurThreadFill].m_PersFlags & RBPF_OBLIQUE_FRUSTUM_CLIPPING)
		{
			Matrix44A mObliqueProjMatrix;
			mObliqueProjMatrix.SetIdentity();

			Plane pPlane = gRenDev->m_RP.m_TI[m_nCurThreadFill].m_pObliqueClipPlane;

			mObliqueProjMatrix.m02 = pPlane.n[0];
			mObliqueProjMatrix.m12 = pPlane.n[1];
			mObliqueProjMatrix.m22 = pPlane.n[2];
			mObliqueProjMatrix.m32 = pPlane.d;

			Matrix44f* mProj = (Matrix44f*)&pData[0];
			*mProj = Matrix44A(*mProj) * mObliqueProjMatrix;

			gRenDev->m_RP.m_TI[m_nCurThreadFill].m_PersFlags &= ~RBPF_OBLIQUE_FRUSTUM_CLIPPING;
		}

		pData += Align4(size);
		EndCommand(pData);
	}
	else
		gRenDev->RT_SetCameraInfo();
}

void SRenderThread::RC_PrepareLevelTexStreaming()
{
	if (IsRenderThread())
	{
		gRenDev->RT_PrepareLevelTexStreaming();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PrepareLevelTexStreaming, 0);
	EndCommand(p);
}

void SRenderThread::RC_PostLoadLevel()
{
	if (IsRenderThread())
	{
		gRenDev->RT_PostLevelLoading();
		return;
	}

	if (IsLevelLoadingThread())
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 
		TArray<byte>& queue = m_CommandsLoading;
		byte* p = AddCommandTo(eRC_PostLevelLoading, 0, queue);
		EndCommandTo(p, queue);
	}
	else
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_PostLevelLoading, 0);
		EndCommand(p);
	}
}

void SRenderThread::RC_PushFog()
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_PushFog, 0);
		EndCommand(p);
	}
	else
		gRenDev->EF_PushFog();
}

void SRenderThread::RC_PopFog()
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_PopFog, 0);
		EndCommand(p);
	}
	else
		gRenDev->EF_PopFog();
}

void SRenderThread::RC_PushVP()
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_PushVP, 0);
		EndCommand(p);
	}
	else
		gRenDev->FX_PushVP();
}

void SRenderThread::RC_PopVP()
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_PopVP, 0);
		EndCommand(p);
	}
	else
		gRenDev->FX_PopVP();
}

void SRenderThread::RC_FlushTextureStreaming(bool bAbort)
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_FlushTextureStreaming, sizeof(DWORD));
		AddDWORD(p, bAbort);
		EndCommand(p);
	}
	else
	{
		CTexture::RT_FlushStreaming(bAbort);
	}
}

void SRenderThread::RC_ReleaseSystemTextures()
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_ReleaseSystemTextures, 0);
		EndCommand(p);
	}
	else
	{
		CTexture::ReleaseSystemTextures();
	}
}

void SRenderThread::RC_SetEnvTexRT(SEnvTexture* pEnvTex, int nWidth, int nHeight, bool bPush)
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_SetEnvTexRT, 12 + sizeof(void*));
		AddPointer(p, pEnvTex);
		AddDWORD(p, nWidth);
		AddDWORD(p, nHeight);
		AddDWORD(p, bPush);
		EndCommand(p);
	}
	else
		pEnvTex->m_pTex->RT_SetRT(0, nWidth, nHeight, bPush);
}

void SRenderThread::RC_SetEnvTexMatrix(SEnvTexture* pEnvTex)
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_SetEnvTexMatrix, sizeof(void*));
		AddPointer(p, pEnvTex);
		EndCommand(p);
	}
	else
		pEnvTex->RT_SetMatrix();
}

void SRenderThread::RC_PushRT(int nTarget, CTexture* pTex, SDepthTexture* pDS, int nS)
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_PushRT, 8 + 2 * sizeof(void*));
		AddDWORD(p, nTarget);
		AddPointer(p, pTex);
		AddPointer(p, pDS);
		AddDWORD(p, nS);
		EndCommand(p);
	}
	else
		gRenDev->RT_PushRenderTarget(nTarget, pTex, pDS, nS);
}
void SRenderThread::RC_PopRT(int nTarget)
{
	if (!IsRenderThread())
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_PopRT, 4);
		AddDWORD(p, nTarget);
		EndCommand(p);
	}
	else
		gRenDev->RT_PopRenderTarget(nTarget);
}

void SRenderThread::RC_ForceSwapBuffers()
{
	if (IsRenderThread())
	{
		gRenDev->RT_ForceSwapBuffers();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ForceSwapBuffers, 0);
	EndCommand(p);
}

void SRenderThread::RC_SwitchToNativeResolutionBackbuffer()
{
	if (IsRenderThread())
	{
		gRenDev->RT_SwitchToNativeResolutionBackbuffer(true);
		return;
	}
	else
	{
		gRenDev->SetViewport(0, 0, gRenDev->GetOverlayWidth(), gRenDev->GetOverlayHeight());
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SwitchToNativeResolutionBackbuffer, 0);
	EndCommand(p);
}

void SRenderThread::RC_BeginFrame()
{
	if (IsRenderThread())
	{
		gRenDev->RT_BeginFrame();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_BeginFrame, 0);
	EndCommand(p);
}
void SRenderThread::RC_EndFrame(bool bWait)
{
	if (IsRenderThread())
	{
		gRenDev->RT_EndFrame();
		SyncMainWithRender();
		return;
	}
	if (!bWait && CheckFlushCond())
		return;

	LOADINGLOCK_COMMANDQUEUE
	gRenDev->GetIRenderAuxGeom()->Commit(); // need to issue flush of main thread's aux cb before EndFrame (otherwise it is processed after p3dDev->EndScene())

	if (m_eVideoThreadMode == eVTM_Disabled)
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 

		if (const unsigned int size = m_CommandsLoading.size())
		{
			byte* buf = m_Commands[m_nCurThreadFill].Grow(size);
			memcpy(buf, &m_CommandsLoading[0], size);
			m_CommandsLoading.Free();
		}
	}

	byte* p = AddCommand(eRC_EndFrame, 0);
	EndCommand(p);
	SyncMainWithRender();
}

void SRenderThread::RC_PrecacheResource(ITexture* pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter)
{
	if (IsRenderThread())
	{
		gRenDev->PrecacheTexture(pTP, fMipFactor, fTimeToReady, Flags, nUpdateId, nCounter);
		return;
	}

	if (pTP == NULL) return;

	pTP->AddRef();

	if (IsLevelLoadingThread())
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 
		TArray<byte>& queue = m_CommandsLoading;
		byte* p = AddCommandTo(eRC_PrecacheTexture, 20+sizeof(void*), queue);
		AddPointer(p, pTP);
		AddFloat(p, fMipFactor);
		AddFloat(p, fTimeToReady);
		AddDWORD(p, Flags);
		AddDWORD(p, nUpdateId);
		AddDWORD(p, nCounter);
		EndCommandTo(p, queue);
	}
	else
	{
		LOADINGLOCK_COMMANDQUEUE
		byte* p = AddCommand(eRC_PrecacheTexture, 20+sizeof(void*));
		AddPointer(p, pTP);
		AddFloat(p, fMipFactor);
		AddFloat(p, fTimeToReady);
		AddDWORD(p, Flags);
		AddDWORD(p, nUpdateId);
		AddDWORD(p, nCounter);
		EndCommand(p);
	}
}

void SRenderThread::RC_ReleaseDeviceTexture(CTexture* pTexture)
{
	if (IsRenderThread())
	{
		if (m_eVideoThreadMode != eVTM_Disabled)
		{
			m_rdldLock.Lock();
			pTexture->RT_ReleaseDevice();
			m_rdldLock.Unlock();
		}
		else
		{
			pTexture->RT_ReleaseDevice();
		}
		return;
	}
	else if(IsLevelLoadingThread())
	{
		m_rdldLock.Lock();
		pTexture->RT_ReleaseDevice();
		m_rdldLock.Unlock();
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ReleaseDeviceTexture, sizeof(void*));
	AddPointer(p, pTexture);
	EndCommand(p);

	FlushAndWait();
}

void SRenderThread::RC_TryFlush()
{
	if (IsRenderThread())
	{
		return;
	}

	// do nothing if the render thread is still busy
	if (CheckFlushCond())
		return;

	LOADINGLOCK_COMMANDQUEUE
	gRenDev->GetIRenderAuxGeom()->Flush(); // need to issue flush of main thread's aux cb before EndFrame (otherwise it is processed after p3dDev->EndScene())
	SyncMainWithRender();
}

void SRenderThread::RC_DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround)
{
	if (IsRenderThread())
	{
		gRenDev->RT_DrawLines(v, nump, col, flags, fGround);
	}
	else
	{
		LOADINGLOCK_COMMANDQUEUE
		// since we use AddData(...) - we need to allocate 4 bytes (DWORD) more because AddData(...) adds hidden data size into command buffer
		byte* p = AddCommand(eRC_DrawLines, Align4(sizeof(int) + 2 * sizeof(int) + sizeof(float) + nump * sizeof(Vec3) + sizeof(ColorF)));
		AddDWORD(p, nump);
		AddColor(p, col);
		AddDWORD(p, flags);
		AddFloat(p, fGround);
		AddData(p, v, nump * sizeof(Vec3));
		EndCommand(p);
	}
}

void SRenderThread::RC_ClearTargetsImmediately(int8 nType, uint32 nFlags, const ColorF& vColor, float depth)
{
	if (IsRenderThread())
	{
		switch (nType)
		{
		case 0:
			gRenDev->EF_ClearTargetsImmediately(nFlags);
			return;
		case 1:
			gRenDev->EF_ClearTargetsImmediately(nFlags, vColor, depth, 0);
			return;
		case 2:
			gRenDev->EF_ClearTargetsImmediately(nFlags, vColor);
			return;
		case 3:
			gRenDev->EF_ClearTargetsImmediately(nFlags, depth, 0);
			return;
		}
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_ClearBuffersImmediately, Align4(12 + sizeof(ColorF)));
	AddDWORD(p, nType);
	AddDWORD(p, nFlags);
	AddColor(p, vColor);
	AddFloat(p, depth);
	EndCommand(p);
}

void SRenderThread::RC_SetViewport(int x, int y, int width, int height, int id)
{
	if (IsRenderThread())
	{
		gRenDev->RT_SetViewport(x, y, width, height, id);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SetViewport, 20);
	AddDWORD(p, x);
	AddDWORD(p, y);
	AddDWORD(p, width);
	AddDWORD(p, height);
	AddDWORD(p, id);
	EndCommand(p);
}

void SRenderThread::RC_SubmitWind(const SWindGrid* pWind)
{
	if (IsRenderThread())
	{
		gRenDev->RT_SubmitWind(pWind);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SubmitWind, Align4(sizeof(void*)));
	AddPointer(p, (void*)pWind);
	EndCommand(p);
}

void SRenderThread::RC_RenderScene(CRenderView* pRenderView, int nFlags, RenderFunc pRenderFunc)
{
	if (IsRenderThread())
	{
		gRenDev->RT_RenderScene(pRenderView, nFlags, gRenDev->m_RP.m_TI[m_nCurThreadProcess], pRenderFunc);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_RenderScene, Align4(4 + sizeof(void*) + sizeof(SThreadInfo) + sizeof(CRenderView*)));
	AddDWORD(p, nFlags);
	AddTI(p, gRenDev->m_RP.m_TI[m_nCurThreadFill]);
	AddPointer(p, (void*)pRenderFunc);
	AddPointer(p, pRenderView);
	EndCommand(p);
}

void SRenderThread::RC_PrepareStereo(int mode, int output)
{
	if (IsRenderThread())
	{
		gRenDev->RT_PrepareStereo(mode, output);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PrepareStereo, 8);
	AddDWORD(p, mode);
	AddDWORD(p, output);
	EndCommand(p);
}

void SRenderThread::RC_SetStereoEye(int eye)
{
	if (IsRenderThread())
	{
		gRenDev->m_CurRenderEye = eye;
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SetStereoEye, 4);
	AddDWORD(p, eye);
	EndCommand(p);
}

void SRenderThread::RC_AuxFlush(IRenderAuxGeomImpl* pAux, SAuxGeomCBRawDataPackaged& data, size_t begin, size_t end, bool reset)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	if (IsRenderThread())
	{
		pAux->RT_Flush(data, begin, end);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_AuxFlush, 4 * sizeof(void*) + sizeof(uint32));
	AddPointer(p, pAux);
	AddPointer(p, data.m_pData);
	AddPointer(p, (void*) begin);
	AddPointer(p, (void*) end);
	AddDWORD(p, (uint32) reset);
	EndCommand(p);
#endif
}

void SRenderThread::RC_FlashRender(IFlashPlayer_RenderProxy* pPlayer, bool stereo)
{
	if (IsRenderThread())
	{
		gRenDev->FlashRenderInternal(pPlayer, stereo, true);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_FlashRender, 4 + sizeof(void*));
	AddPointer(p, pPlayer);
	AddDWORD(p, stereo ? 1 : 0);
	EndCommand(p);
}

void SRenderThread::RC_FlashRenderPlaybackLockless(IFlashPlayer_RenderProxy* pPlayer, int cbIdx, bool stereo, bool finalPlayback)
{
	if (IsRenderThread())
	{
		gRenDev->FlashRenderPlaybackLocklessInternal(pPlayer, cbIdx, stereo, finalPlayback, true);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_FlashRenderLockless, 12 + sizeof(void*));
	AddPointer(p, pPlayer);
	AddDWORD(p, (uint32) cbIdx);
	AddDWORD(p, stereo ? 1 : 0);
	AddDWORD(p, finalPlayback ? 1 : 0);
	EndCommand(p);
}

void SRenderThread::RC_SetTexture(int nTex, int nUnit)
{
	if (IsRenderThread())
	{
		CTexture::ApplyForID(nUnit, nTex, EDefaultSamplerStates::Unspecified, -1);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SetTexture, 8);
	AddDWORD(p, nTex);
	AddDWORD(p, nUnit);
	EndCommand(p);
}

void SRenderThread::RC_PreprGenerateFarTrees(CREFarTreeSprites* pRE, const SRenderingPassInfo& passInfo)
{
	if (IsRenderThread())
	{
		threadID nThreadID = passInfo.ThreadID();
		gRenDev->GenerateObjSprites(pRE->m_arrVegetationSprites[nThreadID][0], passInfo);
		return;
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PreprGenerateFarTrees, sizeof(void*) + sizeof(SRenderingPassInfo));
	AddPointer(p, pRE);
	AddRenderingPassInfo(p, const_cast<SRenderingPassInfo*>(&passInfo));
	EndCommand(p);
}

bool SRenderThread::RC_OC_ReadResult_Try(uint32 nDefaultNumSamples, CREOcclusionQuery* pRE)
{
	if (IsRenderThread())
	{
		return pRE->RT_ReadResult_Try(nDefaultNumSamples);
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_OC_ReadResult_Try, 4 + sizeof(void*));
	AddDWORD(p, (uint32)nDefaultNumSamples);
	AddPointer(p, pRE);
	EndCommand(p);

	return true;
}

void SRenderThread::RC_CGCSetLayers(IColorGradingControllerInt* pController, const SColorChartLayer* pLayers, uint32 numLayers)
{
	if (IsRenderThread())
	{
		pController->RT_SetLayers(pLayers, numLayers);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_CGCSetLayers, 4 + sizeof(void*));
	AddPointer(p, pController);
	AddDWORD(p, (uint32)numLayers);
	EndCommand(p);

	if (numLayers)
	{
		const size_t copySize = sizeof(SColorChartLayer) * numLayers;
		SColorChartLayer* pLayersDst = (SColorChartLayer*) m_Commands[m_nCurThreadFill].Grow(copySize);
		memcpy(pLayersDst, pLayers, copySize);
	}
}

void SRenderThread::RC_GenerateSkyDomeTextures(CREHDRSky* pSky, int32 width, int32 height)
{
	if (IsRenderThread())
	{
		pSky->GenerateSkyDomeTextures(width, height);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE

	if (m_eVideoThreadMode == eVTM_Disabled)
	{
		if (IsLevelLoadingThread())
		{
			AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_CommandsLoadingLock); 

			TArray<byte>& queue = m_CommandsLoading;
			byte* p = AddCommandTo(eRC_GenerateSkyDomeTextures, sizeof(void*) + sizeof(int32)*2, queue);
			AddPointer(p, pSky);
			AddDWORD(p, width);
			AddDWORD(p, height);
			EndCommandTo(p, queue);
		}
		else
		{
			TArray<byte>& queue = m_Commands[m_nCurThreadFill];
			byte* p = AddCommandTo(eRC_GenerateSkyDomeTextures, sizeof(void*) + sizeof(int32)*2, queue);
			AddPointer(p, pSky);
			AddDWORD(p, width);
			AddDWORD(p, height);
			EndCommandTo(p, queue);
		}
		
	}
	else
	{
		// Move command into loading queue, which will be executed in first render frame after loading is done
		byte* p = AddCommandTo(eRC_GenerateSkyDomeTextures, sizeof(void*) + sizeof(int32) * 2, m_CommandsLoading);
		AddPointer(p, pSky);
		AddDWORD(p, width);
		AddDWORD(p, height);
		EndCommandTo(p, m_CommandsLoading);
	}
}

void SRenderThread::RC_SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode)
{
	if (IsRenderThread())
	{
		gRenDev->RT_SetRendererCVar(pCVar, pArgText, bSilentMode);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_SetRendererCVar, sizeof(void*) + TextCommandSize(pArgText) + 4);
	AddPointer(p, pCVar);
	AddText(p, pArgText);
	AddDWORD(p, bSilentMode ? 1 : 0);
	EndCommand(p);
}

void SRenderThread::RC_RenderDebug(bool bRenderStats)
{
	if (IsRenderThread())
	{
		gRenDev->RT_RenderDebug(bRenderStats);
		return;
	}
	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_RenderDebug, 0);
	EndCommand(p);
}

void SRenderThread::RC_PushSkinningPoolId(uint32 poolId)
{
	if (IsRenderThread())
	{
		gRenDev->RT_SetSkinningPoolId(poolId);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	byte* p = AddCommand(eRC_PushSkinningPoolId, 4);
	AddDWORD(p, poolId);
	EndCommand(p);
}

void SRenderThread::RC_ReleaseRemappedBoneIndices(IRenderMesh* pRenderMesh, uint32 guid)
{
	if (IsRenderThread())
	{
		pRenderMesh->ReleaseRemappedBoneIndicesPair(guid);
		return;
	}

	LOADINGLOCK_COMMANDQUEUE
	pRenderMesh->AddRef(); // don't allow mesh deletion while this command is pending
	byte* p = AddCommand(eRC_ReleaseRemappedBoneIndices, sizeof(void*) + 4);
	AddPointer(p, pRenderMesh);
	AddDWORD(p, guid);
	EndCommand(p);
}

//===========================================================================================

#ifdef DO_RENDERSTATS
	#define START_PROFILE_RT Time = iTimer->GetAsyncTime();
	#define END_PROFILE_PLUS_RT(Dst) Dst += iTimer->GetAsyncTime().GetDifferenceInSeconds(Time);
	#define END_PROFILE_RT(Dst)      Dst = iTimer->GetAsyncTime().GetDifferenceInSeconds(Time);
#else
	#define START_PROFILE_RT
	#define END_PROFILE_PLUS_RT(Dst)
	#define END_PROFILE_RT(Dst)
#endif

#pragma warning(push)
#pragma warning(disable : 4800)
void SRenderThread::ProcessCommands()
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "SRenderThread::ProcessCommands");
	CRYPROFILE_SCOPE_PROFILE_MARKER("SRenderThread::ProcessCommands");

#ifndef STRIP_RENDER_THREAD
	assert(IsRenderThread());
	if (!CheckFlushCond())
		return;

	DWORD nDeviceOwningThreadID = gcpRendD3D->GetBoundThreadID();
	if (m_eVideoThreadMode == eVTM_Disabled)
		gcpRendD3D->BindContextToThread(CryGetCurrentThreadId());

	#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
		#if OGL_SINGLE_CONTEXT
	if (m_eVideoThreadMode == eVTM_Disabled)
		m_kDXGLDeviceContextHandle.Set(gcpRendD3D->GetDeviceContext().GetRealDeviceContext());
		#else
	if (CRenderer::CV_r_multithreaded)
		m_kDXGLContextHandle.Set(gcpRendD3D->GetDevice().GetRealDevice());
	if (m_eVideoThreadMode == eVTM_Disabled)
		m_kDXGLDeviceContextHandle.Set(gcpRendD3D->GetDeviceContext().GetRealDeviceContext(), !CRenderer::CV_r_multithreaded);
		#endif
	#endif //CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION

	#ifdef DO_RENDERSTATS
	CTimeValue Time;
	#endif
	int threadId = m_nCurThreadProcess;

	#if CRY_PLATFORM_DURANGO
	bool bSuspendDevice = false;
	#endif
	int n = 0;
	m_bSuccessful = true;
	m_hResult = S_OK;
	byte* pP;
	while (n < (int)m_Commands[threadId].Num())
	{
		pP = &m_Commands[threadId][n];
		n += sizeof(int);
		byte nC = (byte) * ((int*)pP);

	#if !defined(_RELEASE)
		// Ensure that the command hasn't been processed already
		int* pProcessed = (int*)(pP + sizeof(int));
		IF_UNLIKELY (*pProcessed)
			__debugbreak();
		*pProcessed = 1;
		n += sizeof(int);
	#endif

		switch (nC)
		{
		case eRC_CreateDevice:
			m_bSuccessful &= gRenDev->RT_CreateDevice();
			break;
		case eRC_ResetDevice:
			if (m_eVideoThreadMode == eVTM_Disabled)
				gRenDev->RT_Reset();
			break;
	#if CRY_PLATFORM_DURANGO
		case eRC_SuspendDevice:
			if (m_eVideoThreadMode == eVTM_Disabled)
				gRenDev->RT_SuspendDevice();
			break;
		case eRC_ResumeDevice:
			if (m_eVideoThreadMode == eVTM_Disabled)
			{
				gRenDev->RT_ResumeDevice();
				//Now we really want to resume the device
				bSuspendDevice = false;
			}
			break;
	#endif
		case eRC_ReleaseGraphicsPipeline:
			gRenDev->RT_GraphicsPipelineShutdown();
			break;
		case eRC_ResetDeviceObjectFactory:
			gRenDev->RT_ResetDeviceObjectFactory();
			break;
		case eRC_ReleasePostEffects:
			if (gRenDev->m_pPostProcessMgr)
				gRenDev->m_pPostProcessMgr->ReleaseResources();
			break;
		case eRC_ResetPostEffects:
			if (gRenDev->m_pPostProcessMgr)
				gRenDev->m_pPostProcessMgr->Reset();
			break;
		case eRC_ResetPostEffectsOnSpecChange:
			if (gRenDev->m_pPostProcessMgr)
				gRenDev->m_pPostProcessMgr->Reset(true);
			break;
		case eRC_DisableTemporalEffects:
			gRenDev->RT_DisableTemporalEffects();
			break;
		case eRC_ResetGlass:
			gRenDev->RT_ResetGlass();
			break;
		case eRC_ResetToDefault:
			if (m_eVideoThreadMode == eVTM_Disabled)
				gRenDev->ResetToDefault();
			break;
		case eRC_Init:
			gRenDev->RT_Init();
			break;
		case eRC_ShutDown:
			{
				uint32 nFlags = ReadCommand<uint32>(n);
				gRenDev->RT_ShutDown(nFlags);
			}
			break;
		case eRC_ForceSwapBuffers:
			if (m_eVideoThreadMode == eVTM_Disabled)
				gRenDev->RT_ForceSwapBuffers();
			break;
		case eRC_SwitchToNativeResolutionBackbuffer:
			{
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_SwitchToNativeResolutionBackbuffer(true);
			}
			break;
		case eRC_BeginFrame:
			if (m_eVideoThreadMode == eVTM_Disabled)
				gRenDev->RT_BeginFrame();
			else
			{
				m_bBeginFrameCalled = true;
			}
			break;
		case eRC_EndFrame:
			if (m_eVideoThreadMode == eVTM_Disabled)
				gRenDev->RT_EndFrame();
			else
			{
				// RLT handles precache commands - so all texture streaming prioritisation
				// needs to happen here. Scheduling and device texture management will happen
				// on the RT later.
				CTexture::RLT_LoadingUpdate();

				m_bEndFrameCalled = true;
				gRenDev->m_nFrameSwapID++;
			}
			break;
		case eRC_PreloadTextures:
			CTexture::RT_Precache();
			break;
		case eRC_PrecacheTexture:
			{
				ITexture* pTP = ReadCommand<ITexture*>(n);

				float fMipFactor = ReadCommand<float>(n);
				float fTimeToReady = ReadCommand<float>(n);
				int Flags = ReadCommand<int>(n);
				int nUpdateId = ReadCommand<int>(n);
				int nCounter = ReadCommand<int>(n);
				gRenDev->PrecacheTexture(pTP, fMipFactor, fTimeToReady, Flags, nUpdateId, nCounter);

				pTP->Release();
			}
			break;
		case eRC_ClearBuffersImmediately:
			{
				uint32 nType = ReadCommand<uint32>(n);
				uint32 nFlags = ReadCommand<uint32>(n);
				ColorF vColor = ReadCommand<ColorF>(n);
				float fDepth = ReadCommand<float>(n);
				if (m_eVideoThreadMode != eVTM_Disabled)
				{
					nFlags &= ~FRT_CLEAR_IMMEDIATE;
					switch (nType)
					{
					case 0:
						gRenDev->EF_ClearTargetsLater(nFlags);
						break;
					case 1:
						gRenDev->EF_ClearTargetsLater(nFlags, vColor, fDepth, 0);
						break;
					case 2:
						gRenDev->EF_ClearTargetsLater(nFlags, vColor);
						break;
					case 3:
						gRenDev->EF_ClearTargetsLater(nFlags, fDepth, 0);
						break;
					}
					return;
				}

				switch (nType)
				{
				case 0:
					gRenDev->EF_ClearTargetsImmediately(nFlags);
					break;
				case 1:
					gRenDev->EF_ClearTargetsImmediately(nFlags, vColor, fDepth, 0);
					break;
				case 2:
					gRenDev->EF_ClearTargetsImmediately(nFlags, vColor);
					break;
				case 3:
					gRenDev->EF_ClearTargetsImmediately(nFlags, fDepth, 0);
					break;
				}
			}
			break;
		case eRC_ReadFrameBuffer:
			{
				byte* pRGB = ReadCommand<byte*>(n);
				int nImageX = ReadCommand<int>(n);
				int nSizeX = ReadCommand<int>(n);
				int nSizeY = ReadCommand<int>(n);
				ERB_Type RBType = ReadCommand<ERB_Type>(n);
				int bRGBA = ReadCommand<int>(n);
				int nScaledX = ReadCommand<int>(n);
				int nScaledY = ReadCommand<int>(n);
				gRenDev->RT_ReadFrameBuffer(pRGB, nImageX, nSizeX, nSizeY, RBType, bRGBA, nScaledX, nScaledY);
			}
			break;
		case eRC_UpdateShaderItem:
			{
				SShaderItem* pShaderItem = ReadCommand<SShaderItem*>(n);
				IMaterial* pMaterial = ReadCommand<IMaterial*>(n);
				gRenDev->RT_UpdateShaderItem(pShaderItem);
				if (pMaterial)
					pMaterial->Release();
			}
			break;
		case eRC_RefreshShaderResourceConstants:
			{
				SShaderItem* pShaderItem = ReadCommand<SShaderItem*>(n);
				IMaterial* pMaterial = ReadCommand<IMaterial*>(n);
				gRenDev->RT_RefreshShaderResourceConstants(pShaderItem);
				if (pMaterial)
					pMaterial->Release();
			}
			break;
		case eRC_ReleaseDeviceTexture:
			{
				CTexture* pTexture = ReadCommand<CTexture*>(n);
				if (m_eVideoThreadMode != eVTM_Disabled)
				{
					m_rdldLock.Lock();
					pTexture->RT_ReleaseDevice();
					m_rdldLock.Unlock();
				}
				else
				{
					pTexture->RT_ReleaseDevice();
				}
			}
			break;
		case eRC_AuxFlush:
			{
	#if defined(ENABLE_RENDER_AUX_GEOM)
				IRenderAuxGeomImpl* pAux = ReadCommand<IRenderAuxGeomImpl*>(n);
				CAuxGeomCB::SAuxGeomCBRawData* pData = ReadCommand<CAuxGeomCB::SAuxGeomCBRawData*>(n);
				size_t begin = ReadCommand<size_t>(n);
				size_t end = ReadCommand<size_t>(n);
				bool reset = ReadCommand<uint32>(n) != 0;

				if (m_eVideoThreadMode == eVTM_Disabled)
				{
					SAuxGeomCBRawDataPackaged data = SAuxGeomCBRawDataPackaged(pData);
					pAux->RT_Flush(data, begin, end, reset);
				}
	#endif
			}
			break;
		case eRC_FlashRender:
			{
				START_PROFILE_RT;
				IFlashPlayer_RenderProxy* pPlayer = ReadCommand<IFlashPlayer_RenderProxy*>(n);
				bool stereo = ReadCommand<int>(n) != 0;
				gRenDev->FlashRenderInternal(pPlayer, stereo, m_eVideoThreadMode == eVTM_Disabled);
				END_PROFILE_PLUS_RT(gRenDev->m_fRTTimeFlashRender);
			}
			break;
		case eRC_FlashRenderLockless:
			{
				IFlashPlayer_RenderProxy* pPlayer = ReadCommand<IFlashPlayer_RenderProxy*>(n);
				int cbIdx = ReadCommand<int>(n);
				bool stereo = ReadCommand<int>(n) != 0;
				bool finalPlayback = ReadCommand<int>(n) != 0;
				gRenDev->FlashRenderPlaybackLocklessInternal(pPlayer, cbIdx, stereo, finalPlayback, m_eVideoThreadMode == eVTM_Disabled);
			}
			break;
		case eRC_SetTexture:
			assert(m_eVideoThreadMode == eVTM_Disabled);
			{
				int nTex = ReadCommand<int>(n);
				int nUnit = ReadCommand<int>(n);
				CTexture::ApplyForID(nUnit, nTex, EDefaultSamplerStates::Unspecified, -1);
			}
			break;
		case eRC_DrawLines:
			{
				int nump = ReadCommand<int>(n);
				ColorF col = ReadCommand<ColorF>(n);
				int flags = ReadCommand<int>(n);
				float fGround = ReadCommand<float>(n);

				int d = *(int*)&m_Commands[threadId][n];

				Vec3* pv = (Vec3*)&m_Commands[threadId][n += 4];
				n += nump * sizeof(Vec3);

				gRenDev->RT_DrawLines(pv, nump, col, flags, fGround);
			}
			break;
		case eRC_SetState:
			{
				int nState = ReadCommand<int>(n);
				int nAlphaRef = ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->FX_SetState(nState, nAlphaRef);
			}
			break;
		case eRC_PushWireframeMode:
			{
				int nMode = ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->FX_PushWireframeMode(nMode);
			}
			break;
		case eRC_PopWireframeMode:
			{
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->FX_PopWireframeMode();
			}
			break;
		case eRC_SelectGPU:
			{
				int nGPU = ReadCommand<int>(n);
				gRenDev->RT_SelectGPU(nGPU);   // nMode
			}
			break;
		case eRC_SetCull:
			{
				int nMode = ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_SetCull(nMode); // nMode
			}
			break;
		case eRC_SetScissor:
			{
				bool bEnable = ReadCommand<DWORD>(n) != 0;
				int sX = (int)ReadCommand<DWORD>(n);
				int sY = (int)ReadCommand<DWORD>(n);
				int sWdt = (int)ReadCommand<DWORD>(n);
				int sHgt = (int)ReadCommand<DWORD>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_SetScissor(bEnable, sX, sY, sWdt, sHgt);
			}
			break;
		case eRC_UpdateTexture:
			{
				CTexture* pTexture = ReadCommand<CTexture*>(n);
				byte* pData = ReadCommand<byte*>(n);
				int nX = ReadCommand<int>(n);
				int nY = ReadCommand<int>(n);
				int nZ = ReadCommand<int>(n);
				int nUSize = ReadCommand<int>(n);
				int nVSize = ReadCommand<int>(n);
				int nZSize = ReadCommand<int>(n);
				ETEX_Format eTFSrc = (ETEX_Format)ReadCommand<uint32>(n);
				pTexture->RT_UpdateTextureRegion(pData, nX, nY, nZ, nUSize, nVSize, nZSize, eTFSrc);
				delete[] pData;
				pTexture->Release();
			}
			break;
		case eRC_CreateResource:
			{
				SResourceAsync* pRA = ReadCommand<SResourceAsync*>(n);
				gRenDev->RT_CreateResource(pRA);
			}
			break;
		case eRC_ReleaseResource:
			{
				SResourceAsync* pRes = ReadCommand<SResourceAsync*>(n);
				gRenDev->RT_ReleaseResource(pRes);
			}
			break;
		case eRC_ReleaseOptics:
			{
				IOpticsElementBase* pOpticsElement = ReadCommand<IOpticsElementBase*>(n);
				gRenDev->RT_ReleaseOptics(pOpticsElement);
			}
		break;
		case eRC_ReleaseRenderResources:
			{
				int nFlags = ReadCommand<int>(n);
				gRenDev->RT_ReleaseRenderResources(nFlags);
			}
			break;
		case eRC_UnbindTMUs:
			gRenDev->RT_UnbindTMUs();
			break;
		case eRC_UnbindResources:
			gRenDev->RT_UnbindResources();
			break;
		case eRC_CreateRenderResources:
			gRenDev->RT_CreateRenderResources();
			break;
		case eRC_CreateSystemTargets:
			CTexture::CreateSystemTargets();
			break;
		case eRC_PrecacheDefaultShaders:
			gRenDev->RT_PrecacheDefaultShaders();
			break;

		case eRC_ReleaseShaderResource:
			{
				CShaderResources* pRes = ReadCommand<CShaderResources*>(n);
				//SAFE_DELETE(pRes);
				if (pRes) pRes->RT_Release();
			}
			break;
		case eRC_ReleaseSurfaceResource:
			{
				SDepthTexture* pRes = ReadCommand<SDepthTexture*>(n);
				if (pRes)
				{
					pRes->Release(true);
				}
			}
			break;
		case eRC_ReleaseBaseResource:
			{
				CBaseResource* pRes = ReadCommand<CBaseResource*>(n);
				RC_ReleaseBaseResource(pRes);
			}
			break;
		case eRC_UpdateMesh2:
			assert(m_eVideoThreadMode == eVTM_Disabled);
			{
				CRenderMesh* pMesh = ReadCommand<CRenderMesh*>(n);
				CRenderMesh* pVContainer = ReadCommand<CRenderMesh*>(n);
				InputLayoutHandle eVF = ReadCommand<InputLayoutHandle>(n);
				uint32 nStreamMask = ReadCommand<uint32>(n);
				pMesh->RT_CheckUpdate(pVContainer, eVF, nStreamMask);
			}
			break;
		case eRC_CreateDeviceTexture:
			{
				m_rdldLock.Lock();
				CTexture* pTex = ReadCommand<CTexture*>(n);
				unsigned int len = ReadCommand<unsigned int>(n);
				if (len != 0xFFFFFFFFU)
				{
					const void** pData = new const void*[len + 2];
					// double nullptr terminator
					pData[len + 0] = nullptr;
					pData[len + 1] = nullptr;
					while (len-- != 0)
						pData[len] = ReadCommand<byte*>(n);
					m_bSuccessful = pTex->RT_CreateDeviceTexture(pData);
					delete[] pData;
				}
				else
				{
					D3DResource* pNatTex = ReadCommand<D3DResource*>(n);
					m_bSuccessful = pTex->RT_CreateDeviceTexture(pNatTex);
				}
				m_rdldLock.Unlock();
			}
			break;
		case eRC_CopyDataToTexture:
			{
				void* pkTexture = ReadCommand<void*>(n);
				unsigned int uiStartMip = ReadCommand<unsigned int>(n);
				unsigned int uiEndMip = ReadCommand<unsigned int>(n);

				RC_CopyDataToTexture(pkTexture, uiStartMip, uiEndMip);
			}
			break;
		case eRC_ClearTarget:
			{
				void* pkTexture = ReadCommand<void*>(n);
				ColorF kColor = ReadCommand<ColorF>(n);

				RC_ClearTarget(pkTexture, kColor);
			}
			break;

		case eRC_DrawDynVB:
			{
				pP = &m_Commands[threadId][0];
				uint32 nSize = *(uint32*)&pP[n];
				SVF_P3F_C4B_T2F* pBuf = (SVF_P3F_C4B_T2F*)&pP[n + 4];
				n += nSize + 4;
				nSize = *(uint32*)&pP[n];
				uint16* pInds = (uint16*)&pP[n + 4];
				n += nSize + 4;
				int nVerts = ReadCommand<int>(n);
				int nInds = ReadCommand<int>(n);
				const PublicRenderPrimitiveType nPrimType = (PublicRenderPrimitiveType)ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_DrawDynVB(pBuf, pInds, nVerts, nInds, nPrimType);
			}
			break;
		case eRC_Draw2dImage:
			{
				START_PROFILE_RT;

				float xpos = ReadCommand<float>(n);
				float ypos = ReadCommand<float>(n);
				float w = ReadCommand<float>(n);
				float h = ReadCommand<float>(n);
				CTexture* pTexture = ReadCommand<CTexture*>(n);
				float s0 = ReadCommand<float>(n);
				float t0 = ReadCommand<float>(n);
				float s1 = ReadCommand<float>(n);
				float t1 = ReadCommand<float>(n);
				float angle = ReadCommand<float>(n);
				int col = ReadCommand<int>(n);
				float z = ReadCommand<float>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_Draw2dImage(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, angle, col, z);
				END_PROFILE_PLUS_RT(gRenDev->m_fRTTimeMiscRender);
			}
			break;

		case eRC_Draw2dImageStretchMode:
			{
				START_PROFILE_RT;

				int mode = ReadCommand<int>(n);
				gRenDev->RT_Draw2dImageStretchMode(mode);
				END_PROFILE_PLUS_RT(gRenDev->m_fRTTimeMiscRender);

			}
			break;
		case eRC_Push2dImage:
			{
				float xpos = ReadCommand<float>(n);
				float ypos = ReadCommand<float>(n);
				float w = ReadCommand<float>(n);
				float h = ReadCommand<float>(n);
				CTexture* pTexture = ReadCommand<CTexture*>(n);
				float s0 = ReadCommand<float>(n);
				float t0 = ReadCommand<float>(n);
				float s1 = ReadCommand<float>(n);
				float t1 = ReadCommand<float>(n);
				float angle = ReadCommand<float>(n);
				int col = ReadCommand<int>(n);
				float z = ReadCommand<float>(n);
				float stereoDepth = ReadCommand<float>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_Push2dImage(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, angle, col, z, stereoDepth);
			}
			break;
		case eRC_PushUITexture:
			{
				float xpos = ReadCommand<float>(n);
				float ypos = ReadCommand<float>(n);
				float w = ReadCommand<float>(n);
				float h = ReadCommand<float>(n);
				CTexture* pTexture = ReadCommand<CTexture*>(n);
				float s0 = ReadCommand<float>(n);
				float t0 = ReadCommand<float>(n);
				float s1 = ReadCommand<float>(n);
				float t1 = ReadCommand<float>(n);
				CTexture* pTarget = ReadCommand<CTexture*>(n);
				int col = ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_PushUITexture(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, pTarget, col);
			}
			break;
		case eRC_Draw2dImageList:
			{
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_Draw2dImageList();
			}
			break;
		case eRC_DrawImageWithUV:
			if (m_eVideoThreadMode == eVTM_Disabled)
			{
				pP = &m_Commands[threadId][n];
				gRenDev->RT_DrawImageWithUV(*(float*)pP,          // xpos
				                            *(float*)&pP[4],      // ypos
				                            *(float*)&pP[8],      // z
				                            *(float*)&pP[12],     // w
				                            *(float*)&pP[16],     // h
				                            *(int*)&pP[20],       // textureid
				                            (float*)&pP[24],      // s
				                            (float*)&pP[40],      // t
				                            *(int*)&pP[56],       // col
				                            *(int*)&pP[60] != 0); // filtered
			}
			n += 64;
			break;
		case eRC_PushProfileMarker:
			{
				char* label = ReadCommand<char*>(n);
				gRenDev->PushProfileMarker(label);
			}
			break;
		case eRC_PopProfileMarker:
			{
				char* label = ReadCommand<char*>(n);
				gRenDev->PopProfileMarker(label);
			}
			break;
		case eRC_SetCamera:
			{
				START_PROFILE_RT;

				Matrix44A ProjMat = ReadCommand<Matrix44>(n);
				Matrix44A ViewMat = ReadCommand<Matrix44>(n);
				Matrix44A CameraZeroMat = ReadCommand<Matrix44>(n);

				if (m_eVideoThreadMode == eVTM_Disabled)
				{
					gRenDev->SetMatrices(ProjMat.GetData(), ViewMat.GetData(), CameraZeroMat.GetData());

					gRenDev->RT_SetCameraInfo();
				}

				END_PROFILE_PLUS_RT(gRenDev->m_fRTTimeMiscRender);
			}
			break;

		case eRC_ReleaseCB:
			{
				void* pCB = ReadCommand<void*>(n);
				gRenDev->RT_ReleaseCB(pCB);
			}
			break;

		case eRC_ReleaseRS:
			{
				SUninitialized<std::shared_ptr<CDeviceResourceSet>> typeless;
				ReadCommand(n, typeless);
				typeless.Destruct();
			}
			break;
		case eRC_ReleaseVBStream:
			assert(m_eVideoThreadMode == eVTM_Disabled);
			{
				void* pVB = ReadCommand<void*>(n);
				int nStream = ReadCommand<int>(n);
				gRenDev->RT_ReleaseVBStream(pVB, nStream);
			}
			break;
		case eRC_ReleaseVB:
			assert(m_eVideoThreadMode == eVTM_Disabled);
			{
				buffer_handle_t nID = ReadCommand<buffer_handle_t>(n);
				gRenDev->m_DevBufMan.Destroy(nID);
			}
			break;
		case eRC_ReleaseIB:
			assert(m_eVideoThreadMode == eVTM_Disabled);
			{
				buffer_handle_t nID = ReadCommand<buffer_handle_t>(n);
				gRenDev->m_DevBufMan.Destroy(nID);
			}
			break;
		case eRC_RenderScene:
			{
				START_PROFILE_RT;

				int nFlags = ReadCommand<int>(n);
				SThreadInfo TI;
				LoadUnaligned((uint32*)&m_Commands[threadId][n], TI);
				n += sizeof(SThreadInfo);
				RenderFunc pRenderFunc = ReadCommand<RenderFunc>(n);
				CRenderView* pRenderView = ReadCommand<CRenderView*>(n);
				// when we are in video mode, don't execute the command
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_RenderScene(pRenderView, nFlags, TI, pRenderFunc);
				else
				{
					// cleanup when showing loading render screen
					if (!pRenderView->IsRecursive())
					{
						////////////////////////////////////////////////
						// to non-thread safe remaing work for *::Render functions
						CRenderMesh::FinalizeRendItems(gRenDev->m_RP.m_nProcessThreadID);
						CMotionBlur::InsertNewElements();
					}
				}
				END_PROFILE_PLUS_RT(gRenDev->m_fRTTimeSceneRender);
			}
			break;
		case eRC_PrepareStereo:
			{
				int mode = ReadCommand<int>(n);
				int output = ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_PrepareStereo(mode, output);
			}
			break;
		case eRC_SetStereoEye:
			{
				int eye = ReadCommand<int>(n);
				gRenDev->m_CurRenderEye = eye;
			}
			break;
		case eRC_PreprGenerateFarTrees:
			{
				CREFarTreeSprites* pRE = ReadCommand<CREFarTreeSprites*>(n);
				SUninitialized<SRenderingPassInfo> passInfoStorage;
				LoadUnaligned((uint32*)&m_Commands[threadId][n], passInfoStorage);
				const SRenderingPassInfo& passInfo = passInfoStorage;
				n += sizeof(SRenderingPassInfo);
				if (m_eVideoThreadMode == eVTM_Disabled)
				{
					int nThreadID = passInfo.ThreadID();
					int nR = passInfo.GetRecursiveLevel();
					gRenDev->GenerateObjSprites(pRE->m_arrVegetationSprites[nThreadID][nR], passInfo);
				}
			}
			break;
		case eRC_DynTexUpdate:
			assert(m_eVideoThreadMode == eVTM_Disabled);
			{
				SDynTexture* pTex = ReadCommand<SDynTexture*>(n);
				int nNewWidth = ReadCommand<int>(n);
				int nNewHeight = ReadCommand<int>(n);
				pTex->RT_Update(nNewWidth, nNewHeight);
			}
			break;
		case eRC_UpdateMaterialConstants:
			{
				CShaderResources* pSR = ReadCommand<CShaderResources*>(n);
				IShader* pSH = ReadCommand<IShader*>(n);
				pSR->RT_UpdateConstants(pSH);
				pSH->Release();
				if (pSR)
					pSR->Release();
			}
			break;
		case eRC_ParseShader:
			{
				CShader* pSH = ReadCommand<CShader*>(n);
				CShaderResources* pRes = ReadCommand<CShaderResources*>(n);
				uint64 nMaskGen = ReadCommand<uint64>(n);
				uint32 nFlags = ReadCommand<uint32>(n);
				gRenDev->m_cEF.RT_ParseShader(pSH, nMaskGen, nFlags, pRes);
				pSH->Release();
				if (pRes)
					pRes->Release();
			}
			break;
		case eRC_SetShaderQuality:
			{
				EShaderType eST = (EShaderType) ReadCommand<uint32>(n);
				EShaderQuality eSQ = (EShaderQuality) ReadCommand<uint32>(n);
				gRenDev->m_cEF.RT_SetShaderQuality(eST, eSQ);
			}
			break;
		case eRC_PushFog:
			gRenDev->EF_PushFog();
			break;
		case eRC_PopFog:
			gRenDev->EF_PopFog();
			break;
		case eRC_PushVP:
			gRenDev->FX_PushVP();
			break;
		case eRC_PopVP:
			gRenDev->FX_PopVP();
			break;
		case eRC_FlushTextureStreaming:
			{
				bool bAbort = ReadCommand<DWORD>(n) != 0;
				CTexture::RT_FlushStreaming(bAbort);
			}
			break;
		case eRC_ReleaseSystemTextures:
			CTexture::ReleaseSystemTextures();
			break;
		case eRC_SetEnvTexRT:
			{
				SEnvTexture* pTex = ReadCommand<SEnvTexture*>(n);
				int nWidth = ReadCommand<int>(n);
				int nHeight = ReadCommand<int>(n);
				int bPush = ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
				{
					pTex->m_pTex->RT_SetRT(0, nWidth, nHeight, bPush);
				}
			}
			break;
		case eRC_SetEnvTexMatrix:
			{
				SEnvTexture* pTex = ReadCommand<SEnvTexture*>(n);
				pTex->RT_SetMatrix();
			}
			break;
		case eRC_PushRT:
			assert(m_eVideoThreadMode == eVTM_Disabled);
			{
				int nTarget = ReadCommand<int>(n);
				CTexture* pTex = ReadCommand<CTexture*>(n);
				SDepthTexture* pDS = ReadCommand<SDepthTexture*>(n);
				int nS = ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_PushRenderTarget(nTarget, pTex, pDS, nS);
			}
			break;
		case eRC_PopRT:
			{
				int nTarget = ReadCommand<int>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_PopRenderTarget(nTarget);
			}
			break;
		case eRC_EntityDelete:
			{
				IRenderNode* pRN = ReadCommand<IRenderNode*>(n);
				SDynTexture_Shadow::RT_EntityDelete(pRN);
			}
			break;
		case eRC_SubmitWind:
			{
				const SWindGrid* pWind = ReadCommand<SWindGrid*>(n);
				gRenDev->RT_SubmitWind(pWind);
			}
			break;
		case eRC_PrecacheShader:
			{
				CShader* pShader = ReadCommand<CShader*>(n);
				SShaderCombination cmb = ReadCommand<SShaderCombination>(n);
				bool bForce = ReadCommand<DWORD>(n) != 0;
				CShaderResources* pRes = ReadCommand<CShaderResources*>(n);

				pShader->mfPrecache(cmb, bForce, pRes);

				SAFE_RELEASE(pRes);
				pShader->Release();
			}
			break;
		case eRC_SetViewport:
			{
				int nX = ReadCommand<int>(n);
				int nY = ReadCommand<int>(n);
				int nWidth = ReadCommand<int>(n);
				int nHeight = ReadCommand<int>(n);
				int nID = ReadCommand<int>(n);
				// when we are in video mode, don't execute the command
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_SetViewport(nX, nY, nWidth, nHeight, nID);
			}
			break;
		case eRC_TexBlurAnisotropicVertical:
			{
				CTexture* pTex = ReadCommand<CTexture*>(n);
				float fAnisoScale = ReadCommand<float>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
				{
					CStandardGraphicsPipeline& gp = gcpRendD3D->GetGraphicsPipeline();
					gp.ExecuteAnisotropicVerticalBlur(pTex, 1, 8 * max(1.0f - min(fAnisoScale / 100.0f, 1.0f), 0.2f), 1, false);
				}
			}
			break;

		case eRC_OC_ReadResult_Try:
			{
				uint32 nDefaultNumSamples = ReadCommand<uint32>(n);
				CREOcclusionQuery* pRE = ReadCommand<CREOcclusionQuery*>(n);
				if (m_eVideoThreadMode == eVTM_Disabled)
					pRE->RT_ReadResult_Try(nDefaultNumSamples);
			}
			break;

		case eRC_PrepareLevelTexStreaming:
			{
				gRenDev->RT_PrepareLevelTexStreaming();
			}
			break;

		case eRC_PostLevelLoading:
			{
				gRenDev->RT_PostLevelLoading();
			}
			break;

		case eRC_StartVideoThread:
			m_eVideoThreadMode = eVTM_RequestStart;
			break;
		case eRC_StopVideoThread:
			m_eVideoThreadMode = eVTM_RequestStop;
			break;

		case eRC_CGCSetLayers:
			{
				IColorGradingControllerInt* pController = ReadCommand<IColorGradingControllerInt*>(n);
				uint32 numLayers = ReadCommand<uint32>(n);
				const SColorChartLayer* pLayers = numLayers ? (const SColorChartLayer*) &m_Commands[threadId][n] : 0;
				n += sizeof(SColorChartLayer) * numLayers;
				if (m_eVideoThreadMode == eVTM_Disabled)
					pController->RT_SetLayers(pLayers, numLayers);
			}
			break;

		case eRC_GenerateSkyDomeTextures:
			{
				CREHDRSky* pSky = ReadCommand<CREHDRSky*>(n);
				int32 width = ReadCommand<int32>(n);
				int32 height = ReadCommand<int32>(n);
				pSky->GenerateSkyDomeTextures(width, height);
			}
			break;

		case eRC_SetRendererCVar:
			{
				ICVar* pCVar = ReadCommand<ICVar*>(n);
				const char* pArgText = ReadTextCommand<const char*>(n);
				const bool bSilentMode = ReadCommand<int>(n) != 0;

				gRenDev->RT_SetRendererCVar(pCVar, pArgText, bSilentMode);
			}
			break;

		case eRC_RenderDebug:
			{
				if (m_eVideoThreadMode == eVTM_Disabled)
					gRenDev->RT_RenderDebug();
			}
			break;

		case eRC_ForceMeshGC:
			if (m_eVideoThreadMode == eVTM_Disabled)
				CRenderMesh::Tick(MAX_RELEASED_MESH_FRAMES);
			break;

		case eRC_DevBufferSync:
			if (m_eVideoThreadMode == eVTM_Disabled)
				gRenDev->m_DevBufMan.Sync(gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_nFrameUpdateID);
			break;

		case eRC_UnlinkTexture:
			{
				CTexture* tex = ReadCommand<CTexture*>(n);
				tex->RT_Unlink();
			}
			break;

		case eRC_RelinkTexture:
			{
				CTexture* tex = ReadCommand<CTexture*>(n);
				tex->RT_Relink();
			}
			break;

		case eRC_PushSkinningPoolId:
			gRenDev->RT_SetSkinningPoolId(ReadCommand<uint32>(n));
			break;
		case eRC_ReleaseRemappedBoneIndices:
			{
				IRenderMesh* pRenderMesh = ReadCommand<IRenderMesh*>(n);
				uint32 guid = ReadCommand<uint32>(n);
				pRenderMesh->ReleaseRemappedBoneIndicesPair(guid);
				pRenderMesh->Release();
			}
			break;

		default:
			{
				assert(0);
			}
			break;
		}
	}

	if (m_eVideoThreadMode == eVTM_Disabled)
		gcpRendD3D->BindContextToThread(nDeviceOwningThreadID);

#endif//STRIP_RENDER_THREAD
}
#pragma warning(pop)

void SRenderThread::Process()
{
	while (true)
	{
		CRY_PROFILE_REGION(PROFILE_RENDERER, "Loop: RenderThread");

		CTimeValue Time = iTimer->GetAsyncTime();

		WaitFlushCond();
		const uint64 start = CryGetTicks();

		if (m_bQuit)
		{
			SignalFlushFinishedCond();
			break;//put it here to safely shut down
		}

		CTimeValue TimeAfterWait = iTimer->GetAsyncTime();
		gRenDev->m_fTimeWaitForMain[m_nCurThreadProcess] += TimeAfterWait.GetDifferenceInSeconds(Time);
		if (gRenDev->m_bStartLevelLoading)
			m_fTimeIdleDuringLoading += TimeAfterWait.GetDifferenceInSeconds(Time);

		float fT = 0.f;

		if (m_eVideoThreadMode == eVTM_Disabled)
		{
			//gRenDev->m_fRTTimeBeginFrame = 0;
			//gRenDev->m_fRTTimeEndFrame = 0;
			gRenDev->m_fRTTimeSceneRender = 0;
			gRenDev->m_fRTTimeFlashRender = 0;
			gRenDev->m_fRTTimeMiscRender = 0;
			ProcessCommands();

			CTimeValue TimeAfterProcess = iTimer->GetAsyncTime();
			fT = TimeAfterProcess.GetDifferenceInSeconds(TimeAfterWait);
			gRenDev->m_fTimeProcessedRT[m_nCurThreadProcess] += fT;

			if (m_eVideoThreadMode == eVTM_RequestStart)
			{
			}

			SignalFlushFinishedCond();
		}

		if (gRenDev->m_bStartLevelLoading)
			m_fTimeBusyDuringLoading += fT;

		if (m_eVideoThreadMode == eVTM_RequestStart)
		{
			uint32 frameId = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_nFrameUpdateID;
			DWORD nDeviceOwningThreadID = gcpRendD3D->GetBoundThreadID();
			gcpRendD3D->BindContextToThread(CryGetCurrentThreadId());
			gRenDev->m_DevBufMan.Sync(frameId); // make sure no request are flying when switching to render loading thread

			// Create another render thread;
			SwitchMode(true);

			{
				CTimeValue lastTime = gEnv->pTimer->GetAsyncTime();

				while (m_eVideoThreadMode != eVTM_ProcessingStop)
				{
#if CRY_PLATFORM_DURANGO
					if (m_suspendWhileLoadingFlag)
					{
						threadID nLoadingThreadId = gEnv->pThreadManager->GetThreadId(RENDER_LOADING_THREAD_NAME);
						HANDLE hHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, nLoadingThreadId);
						DWORD result = SuspendThread(hHandle);

						CryLogAlways("SuspendWhileLoading: Suspend result = %d", result);
						gRenDev->RT_SuspendDevice();

						m_suspendWhileLoadingFlag = 0;     //notify main thread, so suspending deferral can be completed
						m_suspendWhileLoadingEvent.Wait(); //wait until 'resume' will be received

						gRenDev->RT_ResumeDevice();

						result = ResumeThread(hHandle);
						CryLogAlways("SuspendWhileLoading: Resume result = %d", result);
						CloseHandle(hHandle);
					}
#endif

					frameId += 1;
					CTimeValue curTime = gEnv->pTimer->GetAsyncTime();
					float deltaTime = max((curTime - lastTime).GetSeconds(), 0.0f);
					lastTime = curTime;
					gRenDev->m_DevBufMan.Update(frameId, true);

					if (m_pLoadtimeCallback)
						m_pLoadtimeCallback->LoadtimeUpdate(deltaTime);

					{
						m_rdldLock.Lock();

						gRenDev->RT_SwitchToNativeResolutionBackbuffer(false);

						if (m_pLoadtimeCallback)
							m_pLoadtimeCallback->LoadtimeRender();

						gRenDev->m_DevBufMan.ReleaseEmptyBanks(frameId);

						gRenDev->RT_PresentFast();
						CRenderMesh::Tick();
						CTexture::RT_LoadingUpdate();
						m_rdldLock.Unlock();
					}

					// Make sure we aren't running with thousands of FPS with VSync disabled
					gRenDev->LimitFramerate(60, true);

#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
					gcpRendD3D->DevInfo().ProcessSystemEventQueue();
#endif
				}
			}
			if (m_pThreadLoading)
				QuitRenderLoadingThread();
			m_eVideoThreadMode = eVTM_Disabled;

			if (m_bBeginFrameCalled)
			{
				m_bBeginFrameCalled = false;
				gRenDev->RT_BeginFrame();
			}
			if (m_bEndFrameCalled)
			{
				m_bEndFrameCalled = false;
				gRenDev->RT_EndFrame();
			}
			gcpRendD3D->BindContextToThread(nDeviceOwningThreadID);
		}

		const uint64 elapsed = CryGetTicks() - start;
		gEnv->pSystem->GetCurrentUpdateTimeStats().RenderTime = elapsed;
	}
#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
	#if OGL_SINGLE_CONTEXT
	m_kDXGLDeviceContextHandle.Set(NULL);
	#else
	m_kDXGLDeviceContextHandle.Set(NULL, !CRenderer::CV_r_multithreaded);
	m_kDXGLContextHandle.Set(NULL);
	#endif
#endif //CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
}

void SRenderThread::ProcessLoading()
{
	while (true)
	{
		float fTime = iTimer->GetAsyncCurTime();
		WaitFlushCond();
		if (m_bQuitLoading)
		{
			SignalFlushFinishedCond();
			break;//put it here to safely shut down
		}
		float fTimeAfterWait = iTimer->GetAsyncCurTime();
		gRenDev->m_fTimeWaitForMain[m_nCurThreadProcess] += fTimeAfterWait - fTime;
		if (gRenDev->m_bStartLevelLoading)
			m_fTimeIdleDuringLoading += fTimeAfterWait - fTime;
		ProcessCommands();
		SignalFlushFinishedCond();
		float fTimeAfterProcess = iTimer->GetAsyncCurTime();
		gRenDev->m_fTimeProcessedRT[m_nCurThreadProcess] += fTimeAfterProcess - fTimeAfterWait;
		if (gRenDev->m_bStartLevelLoading)
			m_fTimeBusyDuringLoading += fTimeAfterProcess - fTimeAfterWait;
		if (m_eVideoThreadMode == eVTM_RequestStop)
		{
			// Switch to general render thread
			SwitchMode(false);
		}
	}
#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
	#if OGL_SINGLE_CONTEXT
	m_kDXGLDeviceContextHandle.Set(NULL);
	#else
	m_kDXGLDeviceContextHandle.Set(NULL, !CRenderer::CV_r_multithreaded);
	m_kDXGLContextHandle.Set(NULL);
	#endif
#endif //CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
}

#ifndef STRIP_RENDER_THREAD
// Flush current frame and wait for result (main thread only)
void SRenderThread::FlushAndWait()
{
	if (IsRenderThread())
		return;

	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	if (gEnv->pStatoscope)
		gEnv->pStatoscope->LogCallstack("Flush Render Thread");

	if (!m_pThread)
		return;

	SyncMainWithRender();
	SyncMainWithRender();
}
#endif//STRIP_RENDER_THREAD

// Flush current frame without waiting (should be called from main thread)
void SRenderThread::SyncMainWithRender()
{
	CRY_PROFILE_REGION_WAITING(PROFILE_RENDERER, "Wait - SyncMainWithRender");
	CRYPROFILE_SCOPE_PROFILE_MARKER("SyncMainWithRender");

	if (!IsMultithreaded())
	{
		gRenDev->SyncMainWithRender();
		gRenDev->m_fTimeProcessedRT[m_nCurThreadProcess] = 0;
		gRenDev->m_fTimeWaitForMain[m_nCurThreadProcess] = 0;
		gRenDev->m_fTimeWaitForGPU[m_nCurThreadProcess] = 0;
		return;
	}
#ifndef STRIP_RENDER_THREAD

	CTimeValue time = iTimer->GetAsyncTime();
	WaitFlushFinishedCond();

	CPostEffectsMgr* pPostEffectMgr = PostEffectMgr();
	if (pPostEffectMgr)
	{
		// Must be called before the thread ID's get swapped
		pPostEffectMgr->SyncMainWithRender();
	}

	gRenDev->SyncMainWithRender();

	gRenDev->m_fTimeWaitForRender[m_nCurThreadFill] = iTimer->GetAsyncTime().GetDifferenceInSeconds(time);
	//	gRenDev->ToggleMainThreadAuxGeomCB();
	gRenDev->m_RP.m_TI[m_nCurThreadProcess].m_nFrameUpdateID = gRenDev->m_RP.m_TI[m_nCurThreadFill].m_nFrameUpdateID;
	gRenDev->m_RP.m_TI[m_nCurThreadProcess].m_nFrameID = gRenDev->m_RP.m_TI[m_nCurThreadFill].m_nFrameID;
	m_nCurThreadProcess = m_nCurThreadFill;
	m_nCurThreadFill = (m_nCurThreadProcess + 1) & 1;
	gRenDev->m_RP.m_nProcessThreadID = threadID(m_nCurThreadProcess);
	gRenDev->m_RP.m_nFillThreadID = threadID(m_nCurThreadFill);
	m_Commands[m_nCurThreadFill].SetUse(0);
	gRenDev->m_fTimeProcessedRT[m_nCurThreadProcess] = 0;
	gRenDev->m_fTimeWaitForMain[m_nCurThreadProcess] = 0;
	gRenDev->m_fTimeWaitForGPU[m_nCurThreadProcess] = 0;

	// Switches current command buffers in local thread storage.
	g_systemThreadLocalStorage.currentCommandBuffer = m_nCurThreadFill;
	g_renderThreadLocalStorage.currentCommandBuffer = m_nCurThreadProcess;

	//gRenDev->m_RP.m_pCurrentRenderView->PrepareForRendering();

	if (gEnv->pCharacterManager && gEnv->pGameFramework && !gEnv->pGameFramework->IsGamePaused())
	{
		gEnv->pCharacterManager->UpdateRendererFrame();
	}

	SignalFlushCond();
#endif
}

void SRenderThread::QuitRenderThread()
{
	if (IsMultithreaded() && m_pThread)
	{
		SignalQuitCond();

#if defined(USE_LOCKS_FOR_FLUSH_SYNC)
		while (!gEnv->pThreadManager->JoinThread(m_pThread, eJM_TryJoin))
		{
			FlushAndWait();
		}
#else
		gEnv->pThreadManager->JoinThread(m_pThread, eJM_Join);
#endif

		SAFE_DELETE(m_pThread);

#if !defined(STRIP_RENDER_THREAD)
		m_nCurThreadProcess = m_nCurThreadFill;
#endif
	}
	m_bQuit = 1;
	//SAFE_RELEASE(m_pFlashPlayer);
}

void SRenderThread::QuitRenderLoadingThread()
{
	if (IsMultithreaded() && m_pThreadLoading)
	{
		FlushAndWait();
		m_bQuitLoading = true;
		gEnv->pThreadManager->JoinThread(m_pThreadLoading, eJM_Join);
		SAFE_DELETE(m_pThreadLoading);
		m_nRenderThreadLoading = 0;
		CNameTableR::m_nRenderThread = m_nRenderThread;
	}
}

bool SRenderThread::IsFailed()
{
	return !m_bSuccessful;
}

bool CRenderer::FlushRTCommands(bool bWait, bool bImmediatelly, bool bForce)
{
	SRenderThread* pRT = m_pRT;
	IF (!pRT, 0)
		return true;
	if (pRT->IsRenderThread(true))
	{
		SSystemGlobalEnvironment* pEnv = iSystem->GetGlobalEnvironment();
		if (pEnv && pEnv->IsEditor())
		{
			CPostEffectsMgr* pPostEffectMgr = PostEffectMgr();
			if (pPostEffectMgr)
			{
				pPostEffectMgr->SyncMainWithRender();
			}
		}
		return true;
	}
	if (!bForce && (!m_bStartLevelLoading || !pRT->IsMultithreaded()))
		return false;
	if (!bImmediatelly && pRT->CheckFlushCond())
		return false;
	if (bWait)
		pRT->FlushAndWait();

	return true;
}

bool CRenderer::ForceFlushRTCommands()
{
	LOADING_TIME_PROFILE_SECTION;
	return FlushRTCommands(true, true, true);
}

// Must be executed from main thread
void SRenderThread::WaitFlushFinishedCond()
{

	CTimeValue time = iTimer->GetAsyncTime();

#ifdef USE_LOCKS_FOR_FLUSH_SYNC
	m_LockFlushNotify.Lock();
	while (*(volatile int*)&m_nFlush)
	{
		m_FlushFinishedCondition.Wait(m_LockFlushNotify);
	}
	m_LockFlushNotify.Unlock();
#else
	READ_WRITE_BARRIER
	while (*(volatile int*)&m_nFlush)
	{
	#if CRY_PLATFORM_WINDOWS
		const HWND hWnd = GetRenderWindowHandle();
		if (hWnd)
		{
			gEnv->pSystem->PumpWindowMessage(true, hWnd);
		}
		Sleep(0);
	#endif
		READ_WRITE_BARRIER
	}
#endif
}

// Must be executed from render thread
void SRenderThread::WaitFlushCond()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	CTimeValue time = iTimer->GetAsyncTime();
#ifdef USE_LOCKS_FOR_FLUSH_SYNC
	m_LockFlushNotify.Lock();
	while (!*(volatile int*)&m_nFlush)
	{
		m_FlushCondition.Wait(m_LockFlushNotify);
	}
	m_LockFlushNotify.Unlock();
#else
	READ_WRITE_BARRIER
	while (!*(volatile int*)&m_nFlush)
	{
		if (m_bQuit)
			break;
		::Sleep(0);
		READ_WRITE_BARRIER
	}
#endif
}

#undef m_nCurThreadFill
#undef m_nCurThreadProcess
