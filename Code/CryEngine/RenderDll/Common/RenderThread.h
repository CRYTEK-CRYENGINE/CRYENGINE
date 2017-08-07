// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   RenderThread.h : Render thread commands processing.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#ifndef __RENDERTHREAD_H__
#define __RENDERTHREAD_H__

#include <CryCore/AlignmentTools.h>

#define RENDER_THREAD_NAME         "RenderThread"
#define RENDER_LOADING_THREAD_NAME "RenderLoadingThread"

#include <CryThreading/IThreadManager.h>

//====================================================================

struct IFlashPlayer;
struct IFlashPlayer_RenderProxy;
struct IRenderAuxGeomImpl;
struct SAuxGeomCBRawDataPackaged;
struct IColorGradingControllerInt;
struct SColorChartLayer;
class CRenderMesh;
struct IDynTextureSourceImpl;
struct SDynTexture;
struct STexStreamInState;
class CDeviceResourceSet;

#if CRY_PLATFORM_ORBIS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_MOBILE
	#define USE_LOCKS_FOR_FLUSH_SYNC
#endif

enum ERenderCommand
{
	eRC_Unknown = 0,
	eRC_Init,
	eRC_ShutDown,
	eRC_CreateDevice,
	eRC_ResetDevice,
	eRC_SuspendDevice,
	eRC_ResumeDevice,
	eRC_BeginFrame,
	eRC_EndFrame,
	eRC_ClearBuffersImmediately,
	eRC_FlushTextureStreaming,
	eRC_ReleaseSystemTextures,
	eRC_PreloadTextures,
	eRC_ReadFrameBuffer,
	eRC_ForceSwapBuffers,
	eRC_SwitchToNativeResolutionBackbuffer,

	eRC_DrawLines,
	eRC_UpdateTexture,
	eRC_UpdateMesh2,
	eRC_ReleaseShaderResource,
	eRC_ReleaseBaseResource,
	eRC_ReleaseSurfaceResource,
	eRC_ReleaseIB,
	eRC_ReleaseVB,
	eRC_ReleaseCB,
	eRC_ReleaseRS,
	eRC_ReleaseVBStream,
	eRC_CreateResource,
	eRC_ReleaseResource,
	eRC_ReleaseOptics,
	eRC_ReleaseRenderResources,
	eRC_PrecacheDefaultShaders,
	eRC_SubmitWind,
	eRC_UnbindTMUs,
	eRC_UnbindResources,
	eRC_CreateRenderResources,
	eRC_CreateSystemTargets,
	eRC_CreateDeviceTexture,
	eRC_CopyDataToTexture,
	eRC_ClearTarget,
	eRC_ParseShader,
	eRC_SetShaderQuality,
	eRC_UpdateShaderItem,
	eRC_RefreshShaderResourceConstants,
	eRC_ReleaseDeviceTexture,
	eRC_FlashRender,
	eRC_FlashRenderLockless,
	eRC_AuxFlush,
	eRC_RenderScene,
	eRC_PrepareStereo,
	eRC_SetStereoEye,
	eRC_SetCamera,

	eRC_PushProfileMarker,
	eRC_PopProfileMarker,

	eRC_PrepareLevelTexStreaming,
	eRC_PostLevelLoading,
	eRC_SetState,
	eRC_PushWireframeMode,
	eRC_PopWireframeMode,
	eRC_SetCull,
	eRC_SetScissor,

	eRC_SelectGPU,

	eRC_DrawDynVB,
	eRC_Draw2dImage,
	eRC_Draw2dImageStretchMode,
	eRC_Push2dImage,
	eRC_PushUITexture,
	eRC_Draw2dImageList,
	eRC_DrawImageWithUV,

	eRC_PreprGenerateFarTrees,
	eRC_DynTexUpdate,
	eRC_PushFog,
	eRC_PopFog,
	eRC_PushVP,
	eRC_PopVP,
	eRC_SetEnvTexRT,
	eRC_SetEnvTexMatrix,
	eRC_PushRT,
	eRC_PopRT,
	eRC_SetViewport,
	eRC_TexBlurAnisotropicVertical,
	eRC_UpdateMaterialConstants,

	eRC_OC_ReadResult_Try,

	eRC_CGCSetLayers,
	eRC_EntityDelete,
	eRC_ForceMeshGC,
	eRC_DevBufferSync,

	// streaming queries
	eRC_PrecacheTexture,
	eRC_SetTexture,

	eRC_StartVideoThread,
	eRC_StopVideoThread,

	eRC_RenderDebug,
	eRC_PrecacheShader,

	eRC_RelinkTexture,
	eRC_UnlinkTexture,

	eRC_ReleasePostEffects,
	eRC_ResetPostEffects,
	eRC_ResetPostEffectsOnSpecChange,
	eRC_DisableTemporalEffects,

	eRC_ResetGlass,
	eRC_ResetToDefault,

	eRC_GenerateSkyDomeTextures,

	eRC_PushSkinningPoolId,

	eRC_ReleaseRemappedBoneIndices,

	eRC_SetRendererCVar,

	eRC_ReleaseGraphicsPipeline,

	eRC_ResetDeviceObjectFactory,
};

//====================================================================

class CRenderThread : public IThread
{
public:
	CryEvent m_started;

protected:
	// Start accepting work on thread
	virtual void ThreadEntry();
};

class CRenderThreadLoading : public CRenderThread
{
protected:
	// Start accepting work on thread
	virtual void ThreadEntry();
};

struct CRY_ALIGN(128) SRenderThread
{
	CRenderThread* m_pThread;
	CRenderThreadLoading* m_pThreadLoading;
	ILoadtimeCallback* m_pLoadtimeCallback;
	CryMutex m_rdldLock;
	bool m_bQuit;
	bool m_bQuitLoading;
	bool m_bSuccessful;
	bool m_bBeginFrameCalled;
	bool m_bEndFrameCalled;
#ifndef STRIP_RENDER_THREAD
	int m_nCurThreadProcess;
	int m_nCurThreadFill;
#endif
#ifdef USE_LOCKS_FOR_FLUSH_SYNC
	int m_nFlush;
	CryMutex m_LockFlushNotify;
	CryConditionVariable m_FlushCondition;
	CryConditionVariable m_FlushFinishedCondition;
#else
	volatile int m_nFlush;
#endif
	threadID m_nRenderThread;
	threadID m_nRenderThreadLoading;
	threadID m_nLevelLoadingThread;
	threadID m_nMainThread;
#if CRY_PLATFORM_DURANGO
	volatile uint32 m_suspendWhileLoadingFlag;
	CryEvent m_suspendWhileLoadingEvent;
#endif
	HRESULT m_hResult;
#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
	#if !OGL_SINGLE_CONTEXT
	SDXGLContextThreadLocalHandle m_kDXGLContextHandle;
	#endif
	SDXGLDeviceContextThreadLocalHandle m_kDXGLDeviceContextHandle;
#endif //CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
	float m_fTimeIdleDuringLoading;
	float m_fTimeBusyDuringLoading;
	TArray<byte> m_Commands[RT_COMMAND_BUF_COUNT]; // m_nCurThreadFill shows which commands are filled by main thread

	// The below loading queue contains all commands that were submitted and require full device access during loading.
	// Will be blit into the first render frame's command queue after loading and subsequently resized to 0.
	TArray<byte> m_CommandsLoading;
	CryCriticalSectionNonRecursive m_CommandsLoadingLock;

	enum EVideoThreadMode
	{
		eVTM_Disabled = 0,
		eVTM_RequestStart,
		eVTM_Active,
		eVTM_RequestStop,
		eVTM_ProcessingStop,
	};
	EVideoThreadMode m_eVideoThreadMode;

	SRenderThread();
	~SRenderThread();

	static int GetLocalThreadCommandBufferId();

	inline void SignalFlushFinishedCond()
	{
#ifdef USE_LOCKS_FOR_FLUSH_SYNC
		m_LockFlushNotify.Lock();
#endif
		m_nFlush = 0;
#ifdef USE_LOCKS_FOR_FLUSH_SYNC
		m_LockFlushNotify.Unlock();
		m_FlushFinishedCondition.Notify();
#else
		READ_WRITE_BARRIER
#endif
	}

	inline void SignalFlushCond()
	{
#ifdef USE_LOCKS_FOR_FLUSH_SYNC
		m_LockFlushNotify.Lock();
#endif
		m_nFlush = 1;
#ifdef USE_LOCKS_FOR_FLUSH_SYNC
		m_LockFlushNotify.Unlock();
		m_FlushCondition.Notify();
#else
		READ_WRITE_BARRIER
#endif
	}

	inline void SignalQuitCond()
	{
#ifdef USE_LOCKS_FOR_FLUSH_SYNC
		m_LockFlushNotify.Lock();
#endif
		m_bQuit = 1;
#ifdef USE_LOCKS_FOR_FLUSH_SYNC
		m_FlushCondition.Notify();
		m_LockFlushNotify.Unlock();
#else
		READ_WRITE_BARRIER
#endif
	}

	void WaitFlushCond();

#if CRY_PLATFORM_WINDOWS
	HWND GetRenderWindowHandle();
#endif

	void WaitFlushFinishedCond();

	inline void InitFlushCond()
	{
		m_nFlush = 0;
#if !defined(USE_LOCKS_FOR_FLUSH_SYNC)
		READ_WRITE_BARRIER
#endif
	}

	inline bool CheckFlushCond()
	{
#if !defined(USE_LOCKS_FOR_FLUSH_SYNC)
		READ_WRITE_BARRIER
#endif
		return *(int*)&m_nFlush != 0;
	}

	void StartRenderThread()
	{
		if (m_pThread != NULL)
		{
			if (!gEnv->pThreadManager->SpawnThread(m_pThread, RENDER_THREAD_NAME))
			{
				CryFatalError("Error spawning \"%s\" thread.", RENDER_THREAD_NAME);
			}

			m_pThread->m_started.Wait();
		}
	}

	void StartRenderLoadingThread()
	{
		if (m_pThreadLoading != NULL)
		{
			if (!gEnv->pThreadManager->SpawnThread(m_pThreadLoading, RENDER_LOADING_THREAD_NAME))
			{
				CryFatalError("Error spawning \"%s\" thread.", RENDER_LOADING_THREAD_NAME);
			}

			m_pThreadLoading->m_started.Wait();
		}
	}

	bool IsFailed();

	inline size_t Align4(size_t value)
	{
		return (value + 3) & ~((size_t)3);
	}

	inline byte* AddCommandTo(ERenderCommand eRC, size_t nParamBytes, TArray<byte>& queue)
	{
		uint32 cmdSize = sizeof(uint32) + nParamBytes;
#if !defined(_RELEASE)
		cmdSize += sizeof(uint32);
#endif
		byte* ptr = queue.Grow(cmdSize);
		AddDWORD(ptr, eRC);
#if !defined(_RELEASE)
		// Processed flag
		AddDWORD(ptr, 0);
#endif
		return ptr;
	}

	inline void EndCommandTo(byte* ptr, TArray<byte>& queue)
	{
#ifndef _RELEASE
		if (ptr - queue.Data() != queue.Num())
			CryFatalError("Bad render command size - check the parameters and round each up to 4-byte boundaries [expected queue size = %" PRISIZE_T ", actual size = %u]", (size_t)(ptr - queue.Data()), queue.Num());
#endif
	}

	inline byte* AddCommand(ERenderCommand eRC, size_t nParamBytes)
	{
#ifdef STRIP_RENDER_THREAD
		return NULL;
#else
		return AddCommandTo(eRC, nParamBytes, m_Commands[m_nCurThreadFill]);
#endif
	}

	inline void EndCommand(byte* ptr)
	{
#ifndef STRIP_RENDER_THREAD
		EndCommandTo(ptr, m_Commands[m_nCurThreadFill]);
#endif
	}

	inline void AddDWORD(byte*& ptr, uint32 nVal)
	{
		*(uint32*)ptr = nVal;
		ptr += sizeof(uint32);
	}
	inline void AddDWORD64(byte*& ptr, uint64 nVal)
	{
		StoreUnaligned((uint32*)ptr, nVal); // uint32 because command list maintains 4-byte alignment
		ptr += sizeof(uint64);
	}
	inline void AddTI(byte*& ptr, SThreadInfo& TI)
	{
		memcpy(ptr, &TI, sizeof(SThreadInfo));
		ptr += sizeof(SThreadInfo);
	}
	inline void AddRenderingPassInfo(byte*& ptr, const SRenderingPassInfo* pPassInfo)
	{
		memcpy(ptr, pPassInfo, sizeof(SRenderingPassInfo));
		ptr += sizeof(SRenderingPassInfo);
	}
	inline void AddFloat(byte*& ptr, const float fVal)
	{
		*(float*)ptr = fVal;
		ptr += sizeof(float);
	}
	inline void AddVec3(byte*& ptr, const Vec3& cVal)
	{
		*(Vec3*)ptr = cVal;
		ptr += sizeof(Vec3);
	}
	inline void AddColor(byte*& ptr, const ColorF& cVal)
	{
		float* fData = (float*)ptr;
		fData[0] = cVal[0];
		fData[1] = cVal[1];
		fData[2] = cVal[2];
		fData[3] = cVal[3];
		ptr += sizeof(ColorF);
	}
	inline void AddColorB(byte*& ptr, const ColorB& cVal)
	{
		ptr[0] = cVal[0];
		ptr[1] = cVal[1];
		ptr[2] = cVal[2];
		ptr[3] = cVal[3];
		ptr += sizeof(ColorB);
	}
	inline void AddPointer(byte*& ptr, const void* pVal)
	{
		StoreUnaligned((uint32*)ptr, pVal); // uint32 because command list maintains 4-byte alignment
		ptr += sizeof(void*);
	}
	inline void AddData(byte*& ptr, const void* pData, int nLen)
	{
		unsigned pad = (unsigned)-nLen % 4;
		AddDWORD(ptr, nLen + pad);
		memcpy(ptr, pData, nLen);
		ptr += nLen + pad;
	}
	inline void AddText(byte*& ptr, const char* pText)
	{
		int nLen = strlen(pText) + 1;
		unsigned pad = (unsigned)-nLen % 4;
		AddDWORD(ptr, nLen);
		memcpy(ptr, pText, nLen);
		ptr += nLen + pad;
	}
	inline size_t TextCommandSize(const char* pText)
	{
		return 4 + Align4(strlen(pText) + 1);
	}
	inline void AddText(byte*& ptr, const wchar_t* pText)
	{
		int nLen = (wcslen(pText) + 1) * sizeof(wchar_t);
		unsigned pad = (unsigned)-nLen % 4;
		AddDWORD(ptr, nLen);
		memcpy(ptr, pText, nLen);
		ptr += nLen + pad;
	}
	inline size_t TextCommandSize(const wchar_t* pText)
	{
		return 4 + Align4((wcslen(pText) + 1) * sizeof(wchar_t));
	}
	template<class T> T ReadCommand(int& nIndex)
	{
		T Res;
		LoadUnaligned((uint32*)&m_Commands[m_nCurThreadProcess][nIndex], Res);
		nIndex += (sizeof(T) + 3) & ~((size_t)3);
		return Res;
	}
	template<class T> void ReadCommand(int& nIndex, SUninitialized<T>& value)
	{
		LoadUnaligned((uint32*)&m_Commands[m_nCurThreadProcess][nIndex], value);
		nIndex += (sizeof(T) + 3) & ~((size_t)3);
	}
	template<class T> T ReadTextCommand(int& nIndex)
	{
		unsigned int strLen = ReadCommand<unsigned int>(nIndex);
		T Res = (T)&m_Commands[m_nCurThreadProcess][nIndex];
		nIndex += strLen;
		nIndex = (nIndex + 3) & ~((size_t)3);
		return Res;
	}

	inline threadID GetCurrentThreadId(bool bAlwaysCheck = false) const
	{
#ifdef STRIP_RENDER_THREAD
		return m_nRenderThread;
#else
		if (!bAlwaysCheck && m_nRenderThread == m_nMainThread)
			return m_nRenderThread;
		return ::GetCurrentThreadId();
#endif
	}
	void SwitchMode(bool enableVideo);

	void Init();
	void QuitRenderThread();
	void QuitRenderLoadingThread();
	void SyncMainWithRender();
	void FlushAndWait();
	void ProcessCommands();
	void Process();         // Render thread
	void ProcessLoading();  // Loading thread
	int GetThreadList() const;
	bool IsRenderThread(bool bAlwaysCheck = false) const;
	bool IsRenderLoadingThread(bool bAlwaysCheck = false);
	bool IsLevelLoadingThread(bool bAlwaysCheck=false) const;
	bool IsMainThread(bool bAlwaysCheck = false) const;
	bool IsMultithreaded();
	int CurThreadFill() const;

	void RC_Init();
	void RC_ShutDown(uint32 nFlags);
	bool RC_CreateDevice();
	void RC_ResetDevice();
#if CRY_PLATFORM_DURANGO
	void RC_SuspendDevice();
	void RC_ResumeDevice();
#endif
	void RC_PreloadTextures();
	void RC_ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX, int nScaledY);
	bool RC_CreateDeviceTexture(CTexture * pTex, const void* pData[]);
	bool RC_CreateDeviceTexture(CTexture * pTex, D3DResource* pNatTex);
	void RC_CopyDataToTexture(void* pkTexture, unsigned int uiStartMip, unsigned int uiEndMip);
	void RC_ClearTarget(void* pkTexture, const ColorF &kColor);
	void RC_CreateResource(SResourceAsync * pRes);
	void RC_ReleaseRenderResources(uint32 nFlags);
	void RC_PrecacheDefaultShaders();
	void RC_UnbindResources();
	void RC_UnbindTMUs();
	void RC_FreeObject(CRenderObject * pObj);
	void RC_CreateRenderResources();
	void RC_CreateSystemTargets();
	void RC_ReleaseShaderResource(CShaderResources * pRes);
	void RC_ReleaseBaseResource(CBaseResource * pRes);
	void RC_ReleaseSurfaceResource(SDepthTexture * pRes);
	void RC_ReleaseResource(SResourceAsync * pRes);
	void RC_ReleaseOptics(IOpticsElementBase* pOpticsElement);
	void RC_RelinkTexture(CTexture * pTex);
	void RC_UnlinkTexture(CTexture * pTex);
	bool RC_CheckUpdate2(CRenderMesh * pMesh, CRenderMesh * pVContainer, InputLayoutHandle eVF, uint32 nStreamMask);
	void RC_ReleaseCB(void* pCB);
	void RC_ReleaseRS(std::shared_ptr<CDeviceResourceSet> &pRS);
	void RC_ReleaseVB(buffer_handle_t nID);
	void RC_ReleaseIB(buffer_handle_t nID);
	void RC_DrawDynVB(SVF_P3F_C4B_T2F * pBuf, uint16 * pInds, int nVerts, int nInds, const PublicRenderPrimitiveType nPrimType);
	void RC_Draw2dImage(float xpos, float ypos, float w, float h, CTexture * pTexture, float s0, float t0, float s1, float t1, float angle, float r, float g, float b, float a, float z);
	void RC_Draw2dImageStretchMode(bool bStretch);
	void RC_Push2dImage(float xpos, float ypos, float w, float h, CTexture * pTexture, float s0, float t0, float s1, float t1, float angle, float r, float g, float b, float a, float z, float stereoDepth);
	void RC_PushUITexture(float xpos, float ypos, float w, float h, CTexture * pTexture, float s0, float t0, float s1, float t1, CTexture * pTarget, float r, float g, float b, float a);
	void RC_Draw2dImageList();
	void RC_DrawImageWithUV(float xpos, float ypos, float z, float w, float h, int texture_id, float s[4], float t[4], float r, float g, float b, float a, bool filtered = true);
	void RC_UpdateTextureRegion(CTexture * pTex, byte * data, int nX, int nY, int nZ, int USize, int VSize, int ZSize, ETEX_Format eTFSrc);
	void RC_SetState(int State, int AlphaRef);
	void RC_PushWireframeMode(int nMode);
	void RC_PopWireframeMode();
	void RC_SetCull(int nMode);
	void RC_SetScissor(bool bEnable, int sX, int sY, int sWdt, int sHgt);
	void RC_SelectGPU(int nGPU);
	void RC_SubmitWind(const SWindGrid * pWind);
	void RC_PrecacheShader(CShader * pShader, SShaderCombination & cmb, bool bForce, CShaderResources * pRes);
	void RC_PushProfileMarker(char* label);
	void RC_PopProfileMarker(char* label);
	void RC_DrawLines(Vec3 v[], int nump, ColorF & col, int flags, float fGround);
	void RC_ReleaseDeviceTexture(CTexture * pTex);
	void RC_PrecacheResource(ITexture * pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter = 1);
	void RC_ClearTargetsImmediately(int8 nType, uint32 nFlags, const ColorF &vColor, float depth);
	void RC_FlushTextureStreaming(bool bAbort);
	void RC_ReleaseSystemTextures();
	void RC_AuxFlush(IRenderAuxGeomImpl * pAux, SAuxGeomCBRawDataPackaged & data, size_t begin, size_t end, bool reset);
	void RC_FlashRender(IFlashPlayer_RenderProxy * pPlayer, bool stereo);
	void RC_FlashRenderPlaybackLockless(IFlashPlayer_RenderProxy * pPlayer, int cbIdx, bool stereo, bool finalPlayback);
	void RC_ParseShader(CShader * pSH, uint64 nMaskGen, uint32 flags, CShaderResources * pRes);
	void RC_SetShaderQuality(EShaderType eST, EShaderQuality eSQ);
	void RC_UpdateShaderItem(SShaderItem * pShaderItem, IMaterial * pMaterial);
	void RC_RefreshShaderResourceConstants(SShaderItem * pShaderItem, IMaterial * pMaterial);
	void RC_SetCamera();
	void RC_RenderScene(CRenderView * pRenderView, int nFlags, RenderFunc pRenderFunc);
	void RC_BeginFrame();
	void RC_EndFrame(bool bWait);
	void RC_TryFlush();
	void RC_PrepareStereo(int mode, int output);
	void RC_SetStereoEye(int eye);
	bool RC_DynTexUpdate(SDynTexture * pTex, int nNewWidth, int nNewHeight);
	bool RC_DynTexSourceUpdate(IDynTextureSourceImpl * pSrc, float fDistance);
	void RC_PushFog();
	void RC_PopFog();
	void RC_PushVP();
	void RC_PopVP();
	void RC_ForceSwapBuffers();
	void RC_SwitchToNativeResolutionBackbuffer();

	void RC_StartVideoThread();
	void RC_StopVideoThread();
	void RT_StartVideoThread();
	void RT_StopVideoThread();

	void RC_PrepareLevelTexStreaming();
	void RC_PostLoadLevel();
	void RC_SetEnvTexRT(SEnvTexture * pEnvTex, int nWidth, int nHeight, bool bPush);
	void RC_SetEnvTexMatrix(SEnvTexture * pEnvTex);
	void RC_PushRT(int nTarget, CTexture * pTex, SDepthTexture * pDS, int nS);
	void RC_PopRT(int nTarget);
	void RC_TexBlurAnisotropicVertical(CTexture * Tex, float fAnisoScale);
	void RC_EntityDelete(IRenderNode * pRenderNode);
	void RC_UpdateMaterialConstants(CShaderResources * pSR, IShader * pSH);
	void RC_SetTexture(int nTex, int nUnit);

	bool RC_OC_ReadResult_Try(uint32 nDefaultNumSamples, CREOcclusionQuery * pRE);

	void RC_PreprGenerateFarTrees(CREFarTreeSprites * pRE, const SRenderingPassInfo &passInfo);
	void RC_SetViewport(int x, int y, int width, int height, int id = 0);

	void RC_ReleaseVBStream(void* pVB, int nStream);
	void RC_ForceMeshGC(bool instant = false, bool wait = false);
	void RC_DevBufferSync();

	void RC_ReleasePostEffects();
	void RC_ReleaseGraphicsPipeline();
	void RC_ResetDeviceObjectFactory();

	void RC_ResetPostEffects(bool bOnSpecChange = false);
	void RC_ResetGlass();
	void RC_DisableTemporalEffects();
	void RC_ResetToDefault();

	void RC_CGCSetLayers(IColorGradingControllerInt * pController, const SColorChartLayer * pLayers, uint32 numLayers);

	void RC_GenerateSkyDomeTextures(CREHDRSky * pSky, int32 width, int32 height);

	void RC_SetRendererCVar(ICVar * pCVar, const char* pArgText, const bool bSilentMode = false);

	void RC_RenderDebug(bool bRenderStats = true);
	void RC_PushSkinningPoolId(uint32);
	void RC_ReleaseRemappedBoneIndices(IRenderMesh * pRenderMesh, uint32 guid);

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		for (uint32 i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
			pSizer->AddObject(m_Commands[i]);
	}
};

inline int SRenderThread::GetThreadList() const
{
#ifdef STRIP_RENDER_THREAD
	return m_nCurThreadFill;
#else
	if (IsRenderThread()) return m_nCurThreadProcess;

	return m_nCurThreadFill;
#endif
}

inline bool SRenderThread::IsRenderThread(bool bAlwaysCheck) const
{
#ifdef STRIP_RENDER_THREAD
	return true;
#else
	threadID d = this->GetCurrentThreadId(bAlwaysCheck);
	if (d == m_nRenderThreadLoading || d == m_nRenderThread)
		return true;
	return false;
#endif
}

inline bool SRenderThread::IsRenderLoadingThread(bool bAlwaysCheck)
{
#ifdef STRIP_RENDER_THREAD
	return false;
#else
	threadID d = this->GetCurrentThreadId(bAlwaysCheck);
	if (d == m_nRenderThreadLoading)
		return true;
	return false;
#endif
}

inline bool SRenderThread::IsLevelLoadingThread(bool bAlwaysCheck) const
{
#ifdef STRIP_RENDER_THREAD
	return false;
#else
	threadID d = this->GetCurrentThreadId(bAlwaysCheck);
	if (d == m_nLevelLoadingThread)
		return true;
	return false;
#endif
}

inline bool SRenderThread::IsMainThread(bool bAlwaysCheck) const
{
#ifdef STRIP_RENDER_THREAD
	return false;
#else
	threadID d = this->GetCurrentThreadId(bAlwaysCheck);
	if (d == m_nMainThread)
		return true;
	return false;
#endif
}

inline bool SRenderThread::IsMultithreaded()
{
#ifdef STRIP_RENDER_THREAD
	return false;
#else
	return m_pThread != NULL;
#endif
}

inline int SRenderThread::CurThreadFill() const
{
#ifdef STRIP_RENDER_THREAD
	return 0;
#else
	return m_nCurThreadFill;
#endif
}

#ifdef STRIP_RENDER_THREAD
inline void SRenderThread::FlushAndWait()
{
	return;
}
#endif

#endif  // __RENDERTHREAD_H__
