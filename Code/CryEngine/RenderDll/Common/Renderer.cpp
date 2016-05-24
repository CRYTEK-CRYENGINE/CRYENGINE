// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryCore/CryCustomTypes.h>
#include "Shadow_Renderer.h"
#include <Cry3DEngine/IStatObj.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryMovie/IMovieSystem.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryCore/BitFiddling.h>                              // IntegerLog2()
#include <Cry3DEngine/ImageExtensionHelper.h>                 // CImageExtensionHelper
#include "Textures/Image/CImage.h"
#include "Textures/TextureManager.h"
#include "Textures/TextureStreamPool.h"
#include "Textures/TextureCompiler.h"                 // CTextureCompiler
#include <CryRenderer/branchmask.h>
#include "PostProcess/PostEffects.h"
#include "RendElements/CRELensOptics.h"

#include "RendElements/OpticsFactory.h"
#include "IntroMovieRenderer.h"

#include <Cry3DEngine/IGeomCache.h>
#include <Cry3DEngine/ITimeOfDay.h>

#include <CryThreading/IJobManager_JobDelegator.h>
#include <CryThreading/IThreadManager.h>

#include "RenderView.h"
#include "CompiledRenderObject.h"

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning(disable: 4244)
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <CrySystem/ILog.h>
#endif
#include <CryRenderer/IShader.h>

#if !CRY_PLATFORM_ORBIS && !CRY_PLATFORM_DURANGO && !defined(OPENGL)
	#ifdef ENABLE_FRAME_PROFILER_LABELS
// This is need for D3DPERF_ functions
LINK_SYSTEM_LIBRARY("d3d9.lib")
	#endif
#endif

namespace
{
class CConditonalLock
{
	CryCriticalSection& _lock;
	bool                _bActive;
public:
	CConditonalLock(CryCriticalSection& lock, bool bActive)
		: _lock(lock), _bActive(bActive)
	{ if (_bActive) _lock.Lock(); }
	~CConditonalLock()
	{ if (_bActive) _lock.Unlock(); }
};
}

bool QueryIsFullscreen();

#if defined(SUPPORT_D3D_DEBUG_RUNTIME)
string D3DDebug_GetLastMessage();
#endif

//////////////////////////////////////////////////////////////////////////
// Globals.
//////////////////////////////////////////////////////////////////////////
CRenderer* gRenDev = NULL;

int CRenderer::m_iGeomInstancingThreshold = 0;      // 0 means not set yet

#define RENDERER_DEFAULT_FONT "Fonts/default.xml"

#if defined(_LIB)
extern int g_CpuFlags;
#else
int g_CpuFlags;
#endif

//////////////////////////////////////////////////////////////////////////
// Pool allocators.
//////////////////////////////////////////////////////////////////////////
SDynTexture_PoolAlloc* g_pSDynTexture_PoolAlloc = 0;
//////////////////////////////////////////////////////////////////////////

// per-frame profilers: collect the information for each frame for
// displaying statistics at the beginning of each frame
//#define PROFILER(ID,NAME) DEFINE_FRAME_PROFILER(ID,NAME)
//#include "FrameProfilers-list.h"
//#undef PROFILER

/// Used to delete none pre-allocated RenderObject pool elements
struct SDeleteNonePoolRenderObjs
{
	SDeleteNonePoolRenderObjs(CRenderObject* pPoolStart, CRenderObject* pPoolEnd) : m_pPoolStart(pPoolStart), m_pPoolEnd(pPoolEnd) {}

	void operator()(CRenderObject** pData) const
	{
		// Delete elements outside of pool range
		if (*pData && (*pData < m_pPoolStart || *pData > m_pPoolEnd))
			delete *pData;
	}

	CRenderObject* m_pPoolStart;
	CRenderObject* m_pPoolEnd;
};

CRenderer::CRenderer()
	: m_bEditor(false)
	, m_beginFrameCount(0)
{}

void CRenderer::InitRenderer()
{
	if (!gRenDev)
		gRenDev = this;
	m_cEF.m_Bin.m_pCEF = &m_cEF;

	m_pIntroMovieRenderer = 0;
	m_bDualStereoSupport = false;

	m_bShaderCacheGen = false;
	m_bSystemResourcesInit = 0;

	m_bSystemTargetsInit = 0;
	m_bIsWindowActive = true;

	m_bShadowsEnabled = true;
	m_bCloudShadowsEnabled = true;

#if defined(VOLUMETRIC_FOG_SHADOWS)
	m_bVolFogShadowsEnabled = false;
	m_bVolFogCloudShadowsEnabled = false;
#endif
	m_bVolumetricFogEnabled = false;

	m_nDisableTemporalEffects = 0;

	m_nPoolIndex = 0;
	m_nPoolIndexRT = 0;

	m_ReqViewportScale = m_CurViewportScale = m_PrevViewportScale = Vec2(1, 1);

	m_nCurMinAniso = 1;
	m_nCurMaxAniso = 16;
	m_wireframe_mode = R_SOLID_MODE;
	m_wireframe_mode_prev = R_SOLID_MODE;
	m_RP.m_StateOr = 0;
	m_RP.m_StateAnd = -1;

	m_pSpriteVerts = NULL;
	m_pSpriteInds = NULL;

	m_nativeWidth = 0;
	m_nativeHeight = 0;
	m_backbufferWidth = 0;
	m_backbufferHeight = 0;
	m_numSSAASamples = 1;

	CRendererCVars::InitCVars();

	// need to do this because the registering process can modify the default value (getting it from the .cfg) and will not notify the call back
	SetShadowJittering(CV_r_shadow_jittering);

	m_DevMan.Init();

	m_nGPU = 1;

	m_cClearColor = ColorF(0, 0, 0, 128.0f / 255.0f);  // 128 is default GBuffer value
	m_LogFile = NULL;
	m_pDefaultFont = NULL;
	m_TexGenID = 1;
	m_VSync = CV_r_vsync;
#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
	m_overrideRefreshRate = CV_r_overrideRefreshRate;
	m_overrideScanlineOrder = CV_r_overrideScanlineOrder;
#endif
	m_Features = 0;
	m_bVendorLibInitialized = false;
	m_nGraphicsPipeline = CV_r_GraphicsPipeline;
	//init_math();

	for (uint32 id = 0; id < RT_COMMAND_BUF_COUNT; ++id)
	{
		// Initialize frame id to value different from 0, so that comparasion with frameid initialized to 0, in functions that check cache give nagtive result on first frame.
		m_RP.m_TI[id].m_nFrameID = 1;
	}

	m_bPauseTimer = 0;
	m_fPrevTime = -1.0f;

	//  m_RP.m_ShaderCurrTime = 0.0f;

	m_CurFontColor = Col_White;

	m_bUseHWSkinning = CV_r_usehwskinning != 0;
	m_bWaterCaustics = CV_r_watercaustics != 0;

	m_bSwapBuffers = true;
	for (uint32 id = 0; id < RT_COMMAND_BUF_COUNT; ++id)
		m_RP.m_TI[id].m_FS.m_bEnable = true;

#if defined(_DEBUG) && CRY_PLATFORM_WINDOWS
	if (CV_r_printmemoryleaks)
	{
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	}
#endif

	m_nUseZpass = CV_r_usezpass;

	m_nShadowPoolHeight = m_nShadowPoolWidth = 0;

	m_cloudShadowTexId = 0;
	m_cloudShadowSpeed = Vec3(0, 0, 0);
	m_cloudShadowTiling = 1;
	m_cloudShadowInvert = false;
	m_cloudShadowBrightness = 1;

	m_volumetricCloudTexId = 0;

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	m_pSFDrawParams = 0;
#endif

	m_nGPUs = 1;

	//assert(!(FOB_MASK_AFFECTS_MERGING & 0xffff));
	//assert(sizeof(CRenderObject) == 256);
#if !CRY_PLATFORM_64BIT && !CRY_PLATFORM_ANDROID
	// Disabled this check for ShaderCache Gen, it is only a performacne thingy for last gen consoles anyway
	//STATIC_CHECK(sizeof(CRenderObject) == 128, CRenderObject);
#endif
	STATIC_CHECK(!(FOB_MASK_AFFECTS_MERGING & 0xffff), FOB_MASK_AFFECTS_MERGING);

	if (!g_pSDynTexture_PoolAlloc)
		g_pSDynTexture_PoolAlloc = new SDynTexture_PoolAlloc(stl::FHeap().FreeWhenEmpty(true));

	//m_RP.m_VertPosCache.m_nBufSize = 500000 * sizeof(Vec3);
	//m_RP.m_VertPosCache.m_pBuf = new byte [gRenDev->m_RP.m_VertPosCache.m_nBufSize];

	m_pDefaultMaterial = NULL;
	m_pTerrainDefaultMaterial = NULL;

	m_ViewMatrix.SetIdentity();
	m_CameraMatrix.SetIdentity();
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		m_CameraZeroMatrix[i].SetIdentity();
		//	m_CameraMatrixPrev[i][0].SetIdentity();
		//		m_CameraMatrixPrev[i][1].SetIdentity();
	}

	for (int i = 0; i < MAX_NUM_VIEWPORTS; ++i)
	{
		m_CameraMatrixPrev[i][0].SetIdentity();
		m_CameraMatrixPrev[i][1].SetIdentity();
	}

	m_CameraProjMatrixPrev.SetIdentity();
	m_CameraProjMatrixPrevAvg.SetIdentity();
	m_CameraMatrixNearest.SetIdentity();

	m_ProjMatrix.SetIdentity();
	m_TranspOrigCameraProjMatrix.SetIdentity();
	m_CameraProjMatrix.SetIdentity();
	m_CameraProjZeroMatrix.SetIdentity();
	m_InvCameraProjMatrix.SetIdentity();
	m_IdentityMatrix.SetIdentity();

	m_vProjMatrixSubPixoffset = Vec2(0.0f, 0.0f);
	m_vSegmentedWorldOffset = Vec3(ZERO);

	m_RP.m_newOcclusionCameraProj.SetIdentity();
	m_RP.m_newOcclusionCameraView.SetIdentity();

	for (int i = 0; i < CULLER_MAX_CAMS; i++)
	{
		m_RP.m_OcclusionCameraBuffer[i].SetIdentity();

#ifdef CULLER_DEBUG
		m_RP.m_OcclusionCameraBufferID[i] = -1;
#endif
	}

	m_RP.m_nZOcclusionBufferID = -1;

	m_RP.m_nCurrResolveBounds[0] = m_RP.m_nCurrResolveBounds[1] = m_RP.m_nCurrResolveBounds[2] = m_RP.m_nCurrResolveBounds[3] = 0;

	for (int i = 0; i < CRY_ARRAY_COUNT(m_TempMatrices); i++)
		for (int j = 0; j < CRY_ARRAY_COUNT(m_TempMatrices[0]); j++)
			m_TempMatrices[i][j].SetIdentity();

	CParserBin::m_bParseFX = true;
	//CParserBin::m_bEmbeddedSearchInfo = false;
	if (gEnv->IsEditor())
		CParserBin::m_bEditable = true;
#ifndef CONSOLE_CONST_CVAR_MODE
	CV_e_DebugTexelDensity = 0;
#endif
	m_nFlushAllPendingTextureStreamingJobs = 0;
	m_fTexturesStreamingGlobalMipFactor = 0.f;

	m_fogCullDistance = 0.0f;

	m_pDebugRenderNode = NULL;

	m_bCollectDrawCallsInfo = false;
	m_bCollectDrawCallsInfoPerNode = false;
	m_bCopyScreenToBackBuffer = false;

	m_nMeshPoolTimeoutCounter = nMeshPoolMaxTimeoutCounter;

	m_pTextureManager = new CTextureManager;

	// Init Thread Safe Worker Containers
	threadID nThreadId = CryGetCurrentThreadId();
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		m_RP.m_TempObjects[i].Init();
		m_RP.m_TempObjects[i].SetNoneWorkerThreadID(nThreadId);
	}

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
	{
		for (int recursion = 0; recursion < MAX_REND_RECURSION_LEVELS; recursion++)
		{
			const char* name = (!recursion) ? ((i == 0) ? "Normal View 1" : "Normal View 2") : (((i == 0) ? "Recursive View 1" : "Recursive View 2"));
			CRenderView::EViewType type = (!recursion) ? CRenderView::eViewType_Default : CRenderView::eViewType_Recursive;

			m_RP.m_pRenderViews[i][recursion].reset(new CRenderView(name, type));
		}
	}

	m_pRT = new SRenderThread;
	m_pRT->StartRenderThread();

	m_ShadowFrustumMGPUCache.Init();
	RegisterSyncWithMainListener(&m_ShadowFrustumMGPUCache);

	// on console some float values in vertex formats can be 16 bit
	iLog->Log("CRenderer sizeof(Vec2f16)=%" PRISIZE_T " sizeof(Vec3f16)=%" PRISIZE_T, sizeof(Vec2f16), sizeof(Vec3f16));
	CRenderMesh::Initialize();
}

CRenderer::~CRenderer()
{
	//Code now moved to Release()
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::PostInit()
{
	LOADING_TIME_PROFILE_SECTION;
	//////////////////////////////////////////////////////////////////////////
	// Initialize the shader system
	//////////////////////////////////////////////////////////////////////////
	m_cEF.mfPostInit();

	//////////////////////////////////////////////////////////////////////////
	// Load internal renderer font.
	//////////////////////////////////////////////////////////////////////////
	if (gEnv->pCryFont)
	{
		m_pDefaultFont = gEnv->pCryFont->GetFont("default");
		if (!m_pDefaultFont)
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Error getting default font");
	}

	// load all default textures
	if (!m_bShaderCacheGen && m_pTextureManager)
		m_pTextureManager->PreloadDefaultTextures();

	if (!m_bShaderCacheGen)
	{
		ICVar* pIntroMoviesDuringInit = gEnv->pConsole->GetCVar("sys_intromoviesduringinit");
		bool bIntroMoviesDuringInit = pIntroMoviesDuringInit ? pIntroMoviesDuringInit->GetIVal() != 0 : false;

		// Create system resources while in fast load phase
		if (bIntroMoviesDuringInit == false)    // don't create resources here when we have a movies during init, else we get concurrent device context access
			gEnv->pRenderer->InitSystemResources(FRR_SYSTEM_RESOURCES);
	}

}

//////////////////////////////////////////////////////////////////////////

#define INTRO_MOVIES_PAK "_fastload/IntroMovies.pak"
#define USE_INTRO_MOVIES 1               // Change this if your title uses this pak, otherwise "normal" Movies.pak is assumed

void CRenderer::StartRenderIntroMovies()
{
	LOADING_TIME_PROFILE_SECTION;
	assert(m_pIntroMovieRenderer == 0);

#if USE_INTRO_MOVIES
	// Make sure intro pak is valid and excistion else don't bother rendering them
	if (!gEnv->pCryPak->OpenPack(PathUtil::GetGameFolder(), INTRO_MOVIES_PAK,
	                             ICryPak::FLAGS_PAK_IN_MEMORY))
		return;

	gEnv->pCryPak->LoadPakToMemory(INTRO_MOVIES_PAK, ICryPak::eInMemoryPakLocale_GPU);
#endif
	m_pIntroMovieRenderer = new CIntroMovieRenderer;
	m_pIntroMovieRenderer->Initialize();

	StartLoadtimeFlashPlayback(m_pIntroMovieRenderer);
}

void CRenderer::StopRenderIntroMovies(bool bWaitForFinished)
{
	if (m_pIntroMovieRenderer == 0)
		return;

	if (bWaitForFinished)
	{
		m_pIntroMovieRenderer->WaitForCompletion();
	}

	StopLoadtimeFlashPlayback();
	SAFE_DELETE(m_pIntroMovieRenderer);
#if USE_INTRO_MOVIES
	// we don't need the intro movies in memory anymore
	gEnv->pCryPak->LoadPakToMemory(INTRO_MOVIES_PAK, ICryPak::eInMemoryPakLocale_Unload);
	gEnv->pCryPak->ClosePack(INTRO_MOVIES_PAK, 0);
#endif
}

bool CRenderer::IsRenderingIntroMovies() const
{
	return (m_pIntroMovieRenderer != NULL);
}

//////////////////////////////////////////////////////////////////////////

void CRenderer::Release()
{
	// Reminder for Andrey/AntonKap: this needs to be properly handled
	//SAFE_DELETE(g_pSDynTexture_PoolAlloc)
	//g_pSDynTexture_PoolAlloc = NULL;

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
	{
		for (int recursion = 0; recursion < MAX_REND_RECURSION_LEVELS; recursion++)
		{
			m_RP.m_pRenderViews[i][recursion].reset();
		}
	}

	RemoveSyncWithMainListener(&m_ShadowFrustumMGPUCache);
	m_ShadowFrustumMGPUCache.Release();
	CRenderMesh::ShutDown();
	CHWShader::mfCleanupCache();

	if (!m_DevBufMan.Shutdown())
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR_DBGBRK, "could not free all buffers from CDevBufferMan!");

	gRenDev = NULL;
}

//////////////////////////////////////////////////////////////////////
void CRenderer::AddListener(IRendererEventListener* pRendererEventListener)
{
	stl::push_back_unique(m_listRendererEventListeners, pRendererEventListener);
}

//////////////////////////////////////////////////////////////////////
void CRenderer::RemoveListener(IRendererEventListener* pRendererEventListener)
{
	stl::find_and_erase(m_listRendererEventListeners, pRendererEventListener);
}

//////////////////////////////////////////////////////////////////////
/*bool CRenderer::FindImage(CImage *image)
   {
   for (ImageIt i=m_imageList.begin();i!=m_imageList.end();i++)
   {
    CImage *ci=(*i);

    if (ci==image)
      return (true);
   } //i

   return (false);
   } */

//////////////////////////////////////////////////////////////////////
/*CImage *CRenderer::FindImage(const char *filename)
   {

   ImageIt istart=m_imageList.begin();
   ImageIt iend=m_imageList.end();

   for (ImageIt i=m_imageList.begin();i!=iend;i++)
   {
    CImage *ci=(*i);

    if (stricmp(ci->GetName(),filename)==0)
      return (ci);
   } //i

   return (NULL);
   } */

//////////////////////////////////////////////////////////////////////
/*void CRenderer::AddImage(CImage *image)
   {
   m_imageList.push_back(image);
   } */

//////////////////////////////////////////////////////////////////////
//void CRenderer::ShowFps(const char *command/* =NULL */)
/*{
   if (!command)
    return;
   if (stricmp(command,"true")==0)
    m_showfps=true;
   else
   if (stricmp(command,"false")==0)
    m_showfps=false;
   else
    iConsole->Help("ShowFps");
   } */

//////////////////////////////////////////////////////////////////////
void CRenderer::TextToScreenColor(int x, int y, float r, float g, float b, float a, const char* format, ...)
{
	//  if(!cVars->e_text_info)
	//  return;

	char buffer[512];
	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	WriteXY((int)(8 * x), (int)(6 * y), 1, 1, r, g, b, a, "%s", buffer);
}

//////////////////////////////////////////////////////////////////////
void CRenderer::TextToScreen(float x, float y, const char* format, ...)
{
	//  if(!cVars->e_text_info)
	//  return;

	char buffer[512];
	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	WriteXY((int)(8 * x), (int)(6 * y), 1, 1, 1, 1, 1, 1, "%s", buffer);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::Draw2dText(float posX, float posY, const char* pStr, const SDrawTextInfo& ti)
{
	Draw2dTextWithDepth(posX, posY, 1.0f, pStr, ti);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::Draw2dTextWithDepth(float posX, float posY, float posZ, const char* pStr, const SDrawTextInfo& ti)
{
	IF (!m_pDefaultFont, 0)
		return;

	IFFont* pFont = m_pDefaultFont;

	const float r = CLAMP(ti.color[0], 0.0f, 1.0f);
	const float g = CLAMP(ti.color[1], 0.0f, 1.0f);
	const float b = CLAMP(ti.color[2], 0.0f, 1.0f);
	const float a = CLAMP(ti.color[3], 0.0f, 1.0f);

	STextDrawContext ctx;
	ctx.SetColor(ColorF(r, g, b, a));
	ctx.SetCharWidthScale(1.0f);
	ctx.EnableFrame((ti.flags & eDrawText_Framed) != 0);

	if (ti.flags & eDrawText_Monospace)
	{
		if (ti.flags & eDrawText_FixedSize)
			ctx.SetSizeIn800x600(false);
		ctx.SetSize(Vec2(UIDRAW_TEXTSIZEFACTOR * ti.xscale, UIDRAW_TEXTSIZEFACTOR * ti.yscale));
		ctx.SetCharWidthScale(0.5f);
		ctx.SetProportional(false);

		if (ti.flags & eDrawText_800x600)
			ScaleCoordInternal(posX, posY);
	}
	else if (ti.flags & eDrawText_FixedSize)
	{
		ctx.SetSizeIn800x600(false);
		ctx.SetSize(Vec2(UIDRAW_TEXTSIZEFACTOR * ti.xscale, UIDRAW_TEXTSIZEFACTOR * ti.yscale));
		ctx.SetProportional(true);

		if (ti.flags & eDrawText_800x600)
			ScaleCoordInternal(posX, posY);
	}
	else
	{
		ctx.SetSizeIn800x600(true);
		ctx.SetProportional(false);
		ctx.SetCharWidthScale(0.5f);
		ctx.SetSize(Vec2(UIDRAW_TEXTSIZEFACTOR * ti.xscale, UIDRAW_TEXTSIZEFACTOR * ti.yscale));
	}

	// align left/right/center
	if (ti.flags & (eDrawText_Center | eDrawText_CenterV | eDrawText_Right))
	{
		Vec2 textSize = pFont->GetTextSize(pStr, true, ctx);

		// If we're using virtual 800x600 coordinates, convert the text size from
		// pixels to that before using it as an offset.
		if (ctx.m_sizeIn800x600)
		{
			textSize.x /= ScaleCoordXInternal(1.0f);
			textSize.y /= ScaleCoordYInternal(1.0f);
		}

		if (ti.flags & eDrawText_Center)
			posX -= textSize.x * 0.5f;
		else if (ti.flags & eDrawText_Right)
			posX -= textSize.x;

		if (ti.flags & eDrawText_CenterV)
			posY -= textSize.y * 0.5f;
	}

	// Pass flags so that overscan borders can be applied if necessary
	ctx.SetFlags(ti.flags);

	pFont->DrawString(posX, posY, posZ, pStr, true, ctx);
}

void CRenderer::PrintToScreen(float x, float y, float size, const char* buf)
{
	SDrawTextInfo ti;
	ti.xscale = size * 0.5f / 8;
	ti.yscale = size * 1.f / 8;
	ti.color[0] = 1;
	ti.color[1] = 1;
	ti.color[2] = 1;
	ti.color[3] = 1;
	ti.flags = eDrawText_800x600 | eDrawText_2D;
	Draw2dText(x, y, buf, ti);
}

void CRenderer::WriteXY(int x, int y, float xscale, float yscale, float r, float g, float b, float a, const char* format, ...)
{
	//////////////////////////////////////////////////////////////////////
	// Write a string to the screen
	//////////////////////////////////////////////////////////////////////

	va_list args;

	char buffer[512];

	// Check for the presence of a D3D device
	// Format the string
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	SDrawTextInfo ti;
	ti.xscale = xscale;
	ti.yscale = yscale;
	ti.color[0] = r;
	ti.color[1] = g;
	ti.color[2] = b;
	ti.color[3] = a;
	ti.flags = eDrawText_800x600 | eDrawText_2D;
	Draw2dText((float)x, (float)y, buffer, ti);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::DrawTextQueued(Vec3 pos, SDrawTextInfo& ti, const char* text)
{
	int nT = m_pRT->GetThreadList();
	if (text && !gEnv->IsDedicated())
	{
		// ti.yscale is currently ignored, input struct can be refactored

		ColorB col(ColorF(ti.color[0], ti.color[1], ti.color[2], ti.color[3]));

		m_TextMessages[nT].PushEntry_Text(pos, col, ti.xscale, ti.flags, text);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::DrawTextQueued(Vec3 pos, SDrawTextInfo& ti, const char* format, va_list args)
{
	int nT = m_pRT->GetThreadList();
	if (format && !gEnv->IsDedicated())
	{
		char str[512];

		cry_vsprintf(str, format, args);

		// ti.yscale is currently ignored, input struct can be refactored

		ColorB col(ColorF(ti.color[0], ti.color[1], ti.color[2], ti.color[3]));

		m_TextMessages[nT].PushEntry_Text(pos, col, ti.xscale, ti.flags, str);
	}
}

const bool CRenderer::IsEditorMode() const
{
#if CRY_PLATFORM_DESKTOP
	return (m_bEditor != 0);
#else
	return false;
#endif
}

const bool CRenderer::IsShaderCacheGenMode() const
{
#if CRY_PLATFORM_DESKTOP
	return (m_bShaderCacheGen != 0);
#else
	return false;
#endif
}

int CRenderer::GetFrameID(bool bIncludeRecursiveCalls /*=true*/)
{
	int nThreadID = m_pRT ? m_pRT->GetThreadList() : 0;
	if (bIncludeRecursiveCalls)
		return m_RP.m_TI[nThreadID].m_nFrameID;
	return m_RP.m_TI[nThreadID].m_nFrameUpdateID;
}

//////////////////////////////////////////////////////////////////////
void CRenderer::FlushTextMessages()
{
	ASSERT_IS_MAIN_THREAD(m_pRT)

	if (!m_TextMessages[m_RP.m_nFillThreadID].empty())
		m_pRT->RC_FlushTextMessages();
}

void CRenderer::FlushTextMessages(CTextMessages& messages, bool reset)
{
	EnableFog(false);
	int vx, vy, vw, vh;
	GetViewport(&vx, &vy, &vw, &vh);

	while (const CTextMessages::CTextMessageHeader* pEntry = messages.GetNextEntry())
	{
		const CTextMessages::SText* pText = pEntry->CastTo_Text();

		Vec3 vPos(0, 0, 0);
		int nDrawFlags = 0;
		const char* szText = 0;
		Vec4 vColor(1, 1, 1, 1);
		float fSize = 0;

		if (pText)
		{
			nDrawFlags = pText->m_nDrawFlags;
			szText = pText->GetText();
			vPos = pText->m_vPos;
			vColor = pText->m_Color.toVec4() * 1.0f / 255.0f;
			fSize = pText->m_fFontSize;
		}

		bool b800x600 = (nDrawFlags & eDrawText_800x600) != 0;

		float fMaxPosX = 100.0f;
		float fMaxPosY = 100.0f;

		if (!b800x600)
		{
			fMaxPosX = (float)vw;
			fMaxPosY = (float)vh;
		}

		float sx, sy, sz;

		if (!(nDrawFlags & eDrawText_2D))
		{
			float fDist = 1;   //GetDistance(pTextInfo->pos,GetCamera().GetPosition());

			float K = GetCamera().GetFarPlane() / fDist;
			if (fDist > GetCamera().GetFarPlane() * 0.5)
				vPos = GetCamera().GetPosition() + K * (vPos - GetCamera().GetPosition());

			ProjectToScreen(vPos.x, vPos.y, vPos.z, &sx, &sy, &sz);
		}
		else
		{
			if (b800x600)
			{
				// Make 2D coords in range 0-100
				sx = (vPos.x) / vw * 100;
				sy = (vPos.y) / vh * 100;
			}
			else
			{
				sx = vPos.x;
				sy = vPos.y;
			}

			sz = vPos.z;
		}

		if (sx >= 0 && sx <= fMaxPosX)
			if (sy >= 0 && sy <= fMaxPosY)
				if (sz >= 0 && sz <= 1)
				{
					// calculate size
					float sizeX;
					float sizeY;
					if (nDrawFlags & eDrawText_FixedSize)
					{
						sizeX = fSize;
						sizeY = fSize;
						//sizeX = pTextInfo->font_size * 800.0f/vw;
						//sizeY = pTextInfo->font_size * 500.0f/vh;
					}
					else
					{
						sizeX = sizeY = (1.0f - (float)(sz)) * 32.f * fSize;
						sizeX *= 0.5f;
					}

					if (szText)
					{
						// print
						SDrawTextInfo ti;
						ti.flags = nDrawFlags;
						ti.color[0] = vColor.x;
						ti.color[1] = vColor.y;
						ti.color[2] = vColor.z;
						ti.color[3] = vColor.w;
						ti.xscale = sizeX;
						ti.yscale = sizeY;
						if (nDrawFlags & eDrawText_DepthTest)
						{
							sz = 1.0f - 2.0f * sz;
						}
						else
						{
							sz = 1.0f;
						}
						if (b800x600)
							Draw2dTextWithDepth(0.01f * 800 * sx, 0.01f * 600 * sy, sz, szText, ti);
						else
							Draw2dTextWithDepth(sx, sy, sz, szText, ti);
					}
				}
	}

	messages.Clear(!reset);
}

void CRenderer::RT_FlushTextMessages()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	int nT = m_RP.m_nProcessThreadID;

	if (gEnv->IsDedicated() || (m_pRT && m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Disabled))
	{
		m_TextMessages[nT].Clear();
		return;
	}

	FlushTextMessages(m_TextMessages[nT], true);

}

#pragma pack (push)
#pragma pack (1)
typedef struct
{
	unsigned char  id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char  colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char  pixel_size, attributes;
} __PACKED TargaHeader_t;
#pragma pack (pop)

bool CRenderer::SaveTga(unsigned char* sourcedata, int sourceformat, int w, int h, const char* filename, bool flip) const
{
	//assert(0);
	//  return CImage::SaveTga(sourcedata,sourceformat,w,h,filename,flip);

	if (flip)
	{
		int size = w * (sourceformat / 8);
		unsigned char* tempw = new unsigned char[size];
		unsigned char* src1 = sourcedata;
		unsigned char* src2 = sourcedata + (w * (sourceformat / 8)) * (h - 1);
		for (int k = 0; k < h / 2; k++)
		{
			memcpy(tempw, src1, size);
			memcpy(src1, src2, size);
			memcpy(src2, tempw, size);
			src1 += size;
			src2 -= size;
		}
		delete[] tempw;
	}

	unsigned char* oldsourcedata = sourcedata;

	if (sourceformat == FORMAT_8_BIT)
	{

		unsigned char* desttemp = new unsigned char[w * h * 3];
		memset(desttemp, 0, w * h * 3);

		unsigned char* destptr = desttemp;
		unsigned char* srcptr = sourcedata;

		unsigned char col;

		for (int k = 0; k < w * h; k++)
		{
			col = *srcptr++;
			*destptr++ = col;
			*destptr++ = col;
			*destptr++ = col;
		}

		sourcedata = desttemp;

		sourceformat = FORMAT_24_BIT;
	}

	TargaHeader_t header;

	memset(&header, 0, sizeof(header));
	header.image_type = 2;
	header.width = w;
	header.height = h;
	header.pixel_size = sourceformat;

	unsigned char* data = new unsigned char[w * h * (sourceformat >> 3)];
	unsigned char* dest = data;
	unsigned char* source = sourcedata;

	//memcpy(dest,source,w*h*(sourceformat>>3));

	for (int ax = 0; ax < h; ax++)
	{
		for (int by = 0; by < w; by++)
		{
			unsigned char r, g, b, a;
			r = *source;
			source++;
			g = *source;
			source++;
			b = *source;
			source++;
			if (sourceformat == FORMAT_32_BIT)
			{
				a = *source;
				source++;
			}
			*dest = b;
			dest++;
			*dest = g;
			dest++;
			*dest = r;
			dest++;
			if (sourceformat == FORMAT_32_BIT)
			{
				*dest = a;
				dest++;
			}
		}
	}

	FILE* f = fxopen(filename, "wb");
	if (!f)
	{
		//("Cannot save %s\n",filename);
		delete[] data;
		return (false);
	}

	if (!fwrite(&header, sizeof(header), 1, f))
	{
		//CLog::LogToFile("Cannot save %s\n",filename);
		delete[] data;
		fclose(f);
		return (false);
	}

	if (!fwrite(data, w * h * (sourceformat >> 3), 1, f))
	{
		//CLog::LogToFile("Cannot save %s\n",filename);
		delete[] data;
		fclose(f);
		return (false);
	}

	fclose(f);

	delete[] data;
	if (sourcedata != oldsourcedata)
		delete[] sourcedata;

	return (true);
}

//================================================================
SInputShaderResources* CRenderer::EF_CreateInputShaderResource(IRenderShaderResources* pOptionalCopyFrom)
{
	if (pOptionalCopyFrom)
		return new SInputShaderResources(pOptionalCopyFrom);
	return new SInputShaderResources;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

//#include "../Common/Character/CryModel.h"

#if CRY_PLATFORM_DURANGO
void CRenderer::SuspendDevice()
{
	m_pRT->RC_SuspendDevice();
}
void CRenderer::ResumeDevice()
{
	m_pRT->RC_ResumeDevice();
}
#endif
void CRenderer::ForceSwapBuffers()
{
	m_pRT->RC_ForceSwapBuffers();
	ForceFlushRTCommands();
}

void CRenderer::SetState(int State, int AlphaRef /*=-1*/)
{
	m_pRT->RC_SetState(State, AlphaRef);
}

void CRenderer::PushWireframeMode(int mode)
{
	m_pRT->RC_PushWireframeMode(mode);
}

void CRenderer::PopWireframeMode()
{
	m_pRT->RC_PopWireframeMode();
}

void CRenderer::InitSystemResources(int nFlags)
{
	LOADING_TIME_PROFILE_SECTION;
	if (!m_bSystemResourcesInit || m_bDeviceLost == 2)
	{
		iLog->Log("*** Init system render resources ***");

		bool bPrecache = CTexture::s_bPrecachePhase;
		CTexture::s_bPrecachePhase = false;

		ForceFlushRTCommands();
		m_cEF.mfPreloadBinaryShaders();
		m_cEF.mfLoadBasicSystemShaders();
		m_cEF.mfLoadDefaultSystemShaders();

		CTexture::LoadScaleformSystemTextures();
		CTexture::LoadDefaultSystemTextures();
		m_pRT->RC_CreateRenderResources();
		m_pRT->RC_PrecacheDefaultShaders();
		m_pRT->RC_CreateSystemTargets();
		ForceFlushRTCommands();

		/*m_pRT->RC_BeginFrame();
		   SetState(GS_BLSRC_SRCALPHA|GS_BLDST_ONEMINUSSRCALPHA|GS_NODEPTHTEST);
		   SetCullMode(R_CULL_NONE);
		   Draw2dImage(0, 0, 800.f, 600.f, -1,
		   0.0f,0.0f,1.0f,1.0f, 0.f, 0.f,0.f,0.f,1.f,0.f);
		   m_pRT->RC_EndFrame(false);
		   ForceFlushRTCommands();*/

		CTexture::s_bPrecachePhase = bPrecache;

		if (m_bDeviceLost == 2)
			m_bDeviceLost = 0;
		m_bSystemResourcesInit = 1;
	}
}

void CRenderer::FreeResources(int nFlags)
{
	iLog->Log("*** Start clearing render resources ***");

	if (nFlags == FRR_PERMANENT_RENDER_OBJECTS)
	{
		m_pRT->RC_ReleaseRenderResources(nFlags);
		return;
	}

	if (m_bEditor)
		return;

	CTimeValue tBegin = gEnv->pTimer->GetAsyncTime();

	StopLoadtimeFlashPlayback();

#if !defined(_RELEASE)
	ClearDrawCallsInfo();
#endif
	CHWShader::mfFlushPendedShadersWait(-1);

	EF_ReleaseDeferredData();

	if (nFlags & FRR_FLUSH_TEXTURESTREAMING)
	{
		m_pRT->RC_FlushTextureStreaming(true);
	}

	if (nFlags & FRR_DELETED_MESHES)
	{
		for (size_t i = 0; i < MAX_RELEASED_MESH_FRAMES; ++i)
			m_pRT->RC_ForceMeshGC(true, true);
		ForceFlushRTCommands();
	}

	if (nFlags & FRR_SHADERS)
		gRenDev->m_cEF.ShutDown();

	if (nFlags & FRR_RP_BUFFERS)
	{
		ForceFlushRTCommands();

		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			m_RP.m_fogVolumeContibutions[i].clear();
		}

		for (int i = 0; i < sizeof(m_RP.m_RIs) / sizeof(m_RP.m_RIs[0]); ++i)
			m_RP.m_RIs[i].Free();

		stl::for_each_array(m_RP.m_SysVertexPool, stl::container_freer());
		stl::for_each_array(m_RP.m_SysIndexPool, stl::container_freer());

		// Reset render views
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
		{
			for (int recursion = 0; recursion < MAX_REND_RECURSION_LEVELS; recursion++)
			{
				m_RP.m_pRenderViews[i][recursion]->Clear();
			}
		}

		for (ShadowFrustumListsCache::iterator it = m_FrustumsCache.begin();
		     it != m_FrustumsCache.end(); ++it)
			SAFE_DELETE(it->second);
	}

	if (nFlags & (FRR_SYSTEM | FRR_OBJECTS))
	{
		CMotionBlur::FreeData();

		for (int i = 0; i < 3; ++i)
			m_SkinningDataPool[i].FreePoolMemory();

		ForceFlushRTCommands();

		// Get object pool range
		CRenderObject* pObjPoolStart = &m_RP.m_ObjectsPool[0];
		CRenderObject* pObjPoolEnd = &m_RP.m_ObjectsPool[m_RP.m_nNumObjectsInPool * RT_COMMAND_BUF_COUNT];

		// Delete all items that have not been allocated from the object pool
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
		{
			m_RP.m_TempObjects[i].clear(SDeleteNonePoolRenderObjs(pObjPoolStart, pObjPoolEnd));
		}
	}

	if (nFlags & (FRR_TEXTURES | FRR_SYSTEM))
	{
		m_pRT->RC_ReleaseGraphicsPipeline();
		ForceFlushRTCommands();

		if (nFlags & FRR_TEXTURES)
		{
			ForceFlushRTCommands();
			CTexture::ShutDown();
		}

		if (nFlags & FRR_SYSTEM)
		{
			for (uint32 j = 0; j < RT_COMMAND_BUF_COUNT; j++)
			{
				m_RP.m_TempObjects[j].clear();
			}
			if (m_RP.m_ObjectsPool)
			{
				CryModuleMemalignFree(m_RP.m_ObjectsPool);
				m_RP.m_ObjectsPool = NULL;
				SAFE_DELETE(m_RP.m_pIdendityRenderObject);
			}
			FX_PipelineShutdown();
		}
	}

	if (nFlags == FRR_ALL)
	{
		ForceFlushRTCommands();
		CRendElementBase::ShutDown();
	}
	else if (nFlags & FRR_RENDERELEMENTS)
	{
		CRendElement::Cleanup();
		EF_CleanupParticles();
	}

	if (nFlags & FRR_POST_EFFECTS)
	{
		m_pRT->RC_ReleasePostEffects();
		ForceFlushRTCommands();
	}

	if (nFlags & FRR_SYSTEM_RESOURCES)
	{
		m_pRT->RC_ReleaseGraphicsPipeline();
		ForceFlushRTCommands();

		CRenderMesh::ClearJobResources();

		// Free sprite vertices (indices are packed into the same buffer so no need to free them explicitly);
		CryModuleMemalignFree(m_pSpriteVerts);
		m_pSpriteVerts = NULL;
		m_pSpriteInds = NULL;

		m_p3DEngineCommon.m_RainOccluders.Release(true);
		m_p3DEngineCommon.m_CausticInfo.Release();

		m_pRT->RC_UnbindResources();
		ForceFlushRTCommands();

		m_pRT->RC_ResetGlass();
		ForceFlushRTCommands();

		m_pRT->RC_ForceMeshGC(true, true);
		ForceFlushRTCommands();

		m_cEF.mfReleaseSystemShaders();
		ForceFlushRTCommands();

		m_pRT->RC_ReleaseRenderResources(nFlags);
		ForceFlushRTCommands();

		if (m_pPostProcessMgr)
			m_pPostProcessMgr->ReleaseResources();
		ForceFlushRTCommands();

		m_pRT->RC_FlushTextureStreaming(true);
		ForceFlushRTCommands();

		m_pRT->RC_ReleaseSystemTextures();
		ForceFlushRTCommands();

		m_pRT->RC_UnbindTMUs();
		ForceFlushRTCommands();
		CTexture::ResetTMUs();

		CRendElement::Cleanup();
		ForceFlushRTCommands();

		// sync dev buffer only once per frame, to prevent syncing to the currently rendered frame
		// which would result in a deadlock
		if (nFlags & (FRR_SYSTEM_RESOURCES | FRR_DELETED_MESHES))
		{
			m_pRT->RC_DevBufferSync();
			ForceFlushRTCommands();
		}

		PrintResourcesLeaks();

		if (!m_bDeviceLost)
			m_bDeviceLost = 2;
		m_bSystemResourcesInit = 0;

	}

	// free flare queries
	CRELensOptics::ClearResources();

	if ((nFlags & FRR_RESTORE) && !(nFlags & FRR_SYSTEM))
		m_cEF.mfInit();

	CTimeValue tDeltaTime = gEnv->pTimer->GetAsyncTime() - tBegin;
	iLog->Log("*** Clearing render resources took %.1f msec ***", tDeltaTime.GetMilliSeconds());
}

Vec2 CRenderer::SetViewportDownscale(float xscale, float yscale)
{
#if CRY_PLATFORM_WINDOWS
	// refuse to downscale in editor or if MSAA is enabled
	if (gEnv->IsEditor() || m_RP.IsMSAAEnabled())
	{
		m_ReqViewportScale = Vec2(1, 1);
		return m_ReqViewportScale;
	}

	// PC can have awkward resolutions. When setting to full scale, take it as literal (below rounding
	// to multiple of 8 might not work as intended for some resolutions).
	if (xscale >= 1.0f && yscale >= 1.0f)
	{
		m_ReqViewportScale = Vec2(1, 1);
		return m_ReqViewportScale;
	}
#endif

	float fWidth = float(GetWidth());
	float fHeight = float(GetHeight());

	int xres = int(fWidth * xscale);
	int yres = int(fHeight * yscale);

	// clamp to valid value
	xres = CLAMP(xres, 128, GetWidth());
	yres = CLAMP(yres, 128, GetHeight());

	// round down to multiple of 8
	xres &= ~0x7;
	yres &= ~0x7;

	m_ReqViewportScale.x = float(xres) / fWidth;
	m_ReqViewportScale.y = float(yres) / fHeight;

	return m_ReqViewportScale;
}

EScreenAspectRatio CRenderer::GetScreenAspect(int nWidth, int nHeight)
{
	EScreenAspectRatio eSA = eAspect_Unknown;

	float fNeed16_9 = 16.0f / 9.0f;
	float fNeed16_10 = 16.0f / 10.0f;
	float fNeed4_3 = 4.0f / 3.0f;

	float fCur = (float)nWidth / (float)nHeight;
	if (fabs(fCur - fNeed16_9) < 0.1f)
		eSA = eAspect_16_9;

	if (fabs(fCur - fNeed4_3) < 0.1f)
		eSA = eAspect_4_3;

	if (fabs(fCur - fNeed16_10) < 0.1f)
		eSA = eAspect_16_10;

	return eSA;
}

bool CRenderer::WriteTGA(byte* dat, int wdt, int hgt, const char* name, int src_bits_per_pixel, int dest_bits_per_pixel)
{
	return ::WriteTGA((byte*)dat, wdt, hgt, name, src_bits_per_pixel, dest_bits_per_pixel);
}

bool CRenderer::WriteDDS(byte* dat, int wdt, int hgt, int Size, const char* nam, ETEX_Format eFDst, int NumMips)
{
#if CRY_PLATFORM_WINDOWS
	bool bRet = true;

	byte* data = NULL;
	if (Size == 3)
	{
		data = new byte[wdt * hgt * 4];
		for (int i = 0; i < wdt * hgt; i++)
		{
			data[i * 4 + 0] = dat[i * 3 + 0];
			data[i * 4 + 1] = dat[i * 3 + 1];
			data[i * 4 + 2] = dat[i * 3 + 2];
			data[i * 4 + 3] = 255;
		}
		dat = data;
	}
	char name[256];
	fpStripExtension(nam, name);
	cry_strcat(name, ".dds");

	bool bMips = false;
	if (NumMips != 1)
		bMips = true;
	int nDxtSize;
	byte* dst = CTexture::Convert(dat, wdt, hgt, NumMips, eTF_R8G8B8A8, eFDst, NumMips, nDxtSize, true);
	if (dst)
	{
		::WriteDDS(dst, wdt, hgt, 1, name, eFDst, NumMips, eTT_2D);
		delete[] dst;
	}
	if (data)
		delete[] data;

	return bRet;
#else
	return false;
#endif
}

void CRenderer::EF_SetShaderMissCallback(ShaderCacheMissCallback callback)
{
	m_cEF.m_ShaderCacheMissCallback = callback;
}

const char* CRenderer::EF_GetShaderMissLogPath()
{
	return m_cEF.m_ShaderCacheMissPath.c_str();
}

string* CRenderer::EF_GetShaderNames(int& nNumShaders)
{
	nNumShaders = m_cEF.m_ShaderNames.size();
	return nNumShaders ? &m_cEF.m_ShaderNames[0] : NULL;
}

IShader* CRenderer::EF_LoadShader(const char* name, int flags, uint64 nMaskGen)
{
	return m_cEF.mfForName(name, flags, NULL, nMaskGen);
}

void CRenderer::EF_SetShaderQuality(EShaderType eST, EShaderQuality eSQ)
{
	m_pRT->RC_SetShaderQuality(eST, eSQ);

	if (gEnv->p3DEngine)
		gEnv->p3DEngine->GetMaterialManager()->RefreshMaterialRuntime();
}

uint64 CRenderer::EF_GetRemapedShaderMaskGen(const char* name, uint64 nMaskGen, bool bFixup)
{
	return m_cEF.mfGetRemapedShaderMaskGen(name, nMaskGen, bFixup);
}

uint64 CRenderer::EF_GetShaderGlobalMaskGenFromString(const char* szShaderName, const char* szShaderGen, uint64 nMaskGen)
{
	if (!m_cEF.mfUsesGlobalFlags(szShaderName))
		return nMaskGen;

	return m_cEF.mfGetShaderGlobalMaskGenFromString(szShaderGen);
}

// inverse of EF_GetShaderMaskGenFromString
const char* CRenderer::EF_GetStringFromShaderGlobalMaskGen(const char* szShaderName, uint64 nMaskGen)
{
	if (!m_cEF.mfUsesGlobalFlags(szShaderName))
		return "\0";

	return m_cEF.mfGetShaderBitNamesFromGlobalMaskGen(nMaskGen);
}

SShaderItem CRenderer::EF_LoadShaderItem(const char* szName, bool bShare, int flags, SInputShaderResources* Res, uint64 nMaskGen, const SLoadShaderItemArgs* pArgs)
{
	LOADING_TIME_PROFILE_SECTION;

	return m_cEF.mfShaderItemForName(szName, bShare, flags, Res, nMaskGen, pArgs);
}

//////////////////////////////////////////////////////////////////////////
bool CRenderer::EF_ReloadFile_Request(const char* szFileName)
{
	// Replace .tif extensions with .dds extensions.
	char realName[MAX_PATH + 1];
	int nameLength = __min(strlen(szFileName), size_t(MAX_PATH));
	memcpy(realName, szFileName, nameLength);
	realName[nameLength] = 0;
	static const char* tifExtension = "tif";
	static const char* ddsExtension = "dds";
	static const int extensionLength = 3;

	if (nameLength >= extensionLength && memcmp(realName + nameLength - extensionLength, tifExtension, extensionLength) == 0)
		memcpy(realName + nameLength - extensionLength, ddsExtension, extensionLength);

	char nmf[512];
	char drn[512];
	char drv[16];
	char dirn[512];
	char fln[128];
	char extn[16];
	_splitpath(realName, drv, dirn, fln, extn);
	cry_strcpy(drn, drv);
	cry_strcat(drn, dirn);
	cry_strcpy(nmf, fln);
	cry_strcat(nmf, extn);

	//TODO replace this with a single function get file type
	//function should return and enum and the following code replaced with a switch statement
	if (!stricmp(extn, ".dds"))
	{
		return CTexture::ReloadFile_Request(realName);
	}
	return false;
}

bool CRenderer::EF_ReloadFile(const char* szFileName)
{
	// Replace .tif extensions with .dds extensions.
	char realName[MAX_PATH + 1];
	int nameLength = __min(strlen(szFileName), size_t(MAX_PATH));
	memcpy(realName, szFileName, nameLength);
	realName[nameLength] = 0;
	static const char* tifExtension = "tif";
	static const char* ddsExtension = "dds";
	static const int extensionLength = 3;

#if defined(CRY_ENABLE_RC_HELPER)
	if (nameLength >= extensionLength && (memcmp(realName + nameLength - extensionLength, tifExtension, extensionLength) == 0 ||
	                                      memcmp(realName + nameLength - extensionLength, ddsExtension, extensionLength) == 0))
	{
		// Usually reloading a dds will automatically trigger the resource compiler if necessary to
		// compile the .tif into a .dds. However, this does not happen if texture streaming is
		// enabled, so we explicitly run the resource compiler here.
		memcpy(realName + nameLength - extensionLength, tifExtension, extensionLength);

		char unixNameBuffer[MAX_PATH + 1];
		fpConvertDOSToUnixName(unixNameBuffer, realName);

		char gameFolderPath[256];
		cry_strcpy(gameFolderPath, PathUtil::GetGameFolder());
		int gameFolderPathLength = strlen(gameFolderPath);
		if (gameFolderPathLength > 0 && gameFolderPath[gameFolderPathLength - 1] == '\\')
		{
			gameFolderPath[gameFolderPathLength - 1] = '/';
		}
		else if (gameFolderPathLength > 0 && gameFolderPath[gameFolderPathLength - 1] != '/')
		{
			gameFolderPath[gameFolderPathLength++] = '/';
			gameFolderPath[gameFolderPathLength] = 0;
		}

		char* gameRelativePath = unixNameBuffer;
		if (strlen(gameRelativePath) >= (uint32)gameFolderPathLength && memcmp(gameRelativePath, gameFolderPath, gameFolderPathLength) == 0)
			gameRelativePath += gameFolderPathLength;

		char buffer[512];
		return CTextureCompiler::GetInstance().ProcessTextureIfNeeded(gameRelativePath, buffer, sizeof(buffer));
	}
#endif //defined(CRY_ENABLE_RC_HELPER)

	if (nameLength >= extensionLength && memcmp(realName + nameLength - extensionLength, tifExtension, extensionLength) == 0)
		memcpy(realName + nameLength - extensionLength, ddsExtension, extensionLength);

	char nmf[512];
	char drn[512];
	char drv[16];
	char dirn[512];
	char fln[128];
	char extn[16];
	_splitpath(realName, drv, dirn, fln, extn);
	cry_strcpy(drn, drv);
	cry_strcat(drn, dirn);
	cry_strcpy(nmf, fln);
	cry_strcat(nmf, extn);

	//TODO replace this with a single function get file type
	//function should return and enum and the following code replaced with a switch statement
	if (stricmp(extn, ".cgf") == 0)
	{
		IStatObj* pStatObjectToReload = gEnv->p3DEngine->FindStatObjectByFilename(realName);
		if (pStatObjectToReload)
		{
			pStatObjectToReload->Refresh(FRO_GEOMETRY | FRO_SHADERS | FRO_TEXTURES);
			return true;
		}
		return false;
	}
	else if (!stricmp(extn, ".cfx") || (!CV_r_shadersignoreincludeschanging && !stricmp(extn, ".cfi")))
	{
		gRenDev->m_cEF.m_Bin.InvalidateCache();
		// This is a temporary fix so that shaders would reload during hot update.
		bool bRet = gRenDev->m_cEF.mfReloadAllShaders(FRO_SHADERS, 0);
		if (gEnv && gEnv->p3DEngine)
			gEnv->p3DEngine->UpdateShaderItems();
		return bRet;
		//    return gRenDev->m_cEF.mfReloadFile(drn, nmf, FRO_SHADERS);
	}
	else if (!stricmp(extn, ".tif") || !stricmp(extn, ".hdr") || !stricmp(extn, ".tga") || !stricmp(extn, ".pcx") || !stricmp(extn, ".dds") || !stricmp(extn, ".jpg") || !stricmp(extn, ".gif") || !stricmp(extn, ".bmp"))
	{
		return CTexture::ReloadFile(realName);
	}
#if defined(USE_GEOM_CACHES)
	else if (!stricmp(extn, ".cax"))
	{
		IGeomCache* pGeomCache = gEnv->p3DEngine->FindGeomCacheByFilename(realName);
		if (pGeomCache)
		{
			pGeomCache->Reload();
		}
	}
#endif
	return false;
}

void CRenderer::EF_ReloadShaderFiles(int nCategory)
{
	//gRenDev->m_cEF.mfLoadFromFiles(nCategory);
}

void CRenderer::EF_ReloadTextures()
{
	CTexture::ReloadTextures();
}

_smart_ptr<IImageFile> CRenderer::EF_LoadImage(const char* szFileName, uint32 nFlags)
{
	return CImageFile::mfLoad_file(szFileName, nFlags);
}

bool CRenderer::EF_RenderEnvironmentCubeHDR(int size, Vec3& Pos, TArray<unsigned short>& vecData)
{
	return CTexture::RenderEnvironmentCMHDR(size, Pos, vecData);
}

int CRenderer::EF_LoadLightmap(const char* name)
{
	CTexture* tp = (CTexture*)EF_LoadTexture(name, FT_DONT_STREAM | FT_STATE_CLAMP | FT_NOMIPS);
	if (tp->IsTextureLoaded())
		return tp->GetID();
	else
		return -1;
}

ITexture* CRenderer::EF_GetTextureByID(int Id)
{
	if (Id > 0)
	{
		CTexture* tp = CTexture::GetByID(Id);
		if (tp)
			return tp;
	}
	return NULL;
}

ITexture* CRenderer::EF_GetTextureByName(const char* nameTex, uint32 flags)
{
	if (nameTex)
	{
		INDENT_LOG_DURING_SCOPE(true, "While trying to find texture '%s' flags=0x%x...", nameTex, flags);

		const char* ext = fpGetExtension(nameTex);
		if (ext != 0 && (stricmp(ext, ".tif") == 0 || stricmp(ext, ".hdr") == 0))
		{
			// for compilable files, register by the dds file name (to not load it twice)
			char nameDDS[256];
			fpStripExtension(nameTex, nameDDS);
			cry_strcat(nameDDS, ".dds");

			return CTexture::GetByName(nameDDS, flags);
		}
		else
			return CTexture::GetByName(nameTex, flags);
	}

	return NULL;
}

ITexture* CRenderer::EF_LoadTexture(const char* nameTex, const uint32 flags)
{
	if (nameTex)
	{
		INDENT_LOG_DURING_SCOPE(true, "While trying to load texture '%s' flags=0x%x...", nameTex, flags);

		const char* ext = fpGetExtension(nameTex);
		if (ext != 0 && (stricmp(ext, ".tif") == 0 || stricmp(ext, ".hdr") == 0))
		{
			// for compilable files, register by the dds file name (to not load it twice)
			char nameDDS[256];
			fpStripExtension(nameTex, nameDDS);
			cry_strcat(nameDDS, ".dds");

			return CTexture::ForName(nameDDS, flags, eTF_Unknown);
		}
		else
			return CTexture::ForName(nameTex, flags, eTF_Unknown);
	}

	return NULL;
}

IDynTextureSource* CRenderer::EF_LoadDynTexture(const char* dynsourceName, bool sharedRT)
{
	if (sharedRT)
		return new CFlashTextureSourceSharedRT(dynsourceName, NULL);

	return new CFlashTextureSource(dynsourceName, NULL);
}

bool SShaderItem::Update()
{
	if (!(m_pShader->GetFlags() & EF_LOADED))
		return false;
	if ((uint32)m_nTechnique > 1000 && m_nTechnique != -1) // HACK HACK HACK
	{
		CCryNameTSCRC Name(m_nTechnique);
		if (!gRenDev->m_cEF.mfUpdateTechnik(*this, Name))
			return false;
	}

	uint32 nPreprocessFlags = PostLoad();

	// force the write to m_nPreprocessFlags to be last
	// to ensure the main thread has all correct data when the shaderitem is used
	// (m_nPreprocessFlags can indicate a not yet initialized shaderitem)
	MemoryBarrier();
	m_nPreprocessFlags = nPreprocessFlags;
	return true;
}

bool SShaderItem::RefreshResourceConstants()
{
	return gRenDev->m_cEF.mfRefreshResourceConstants(*this);
}

void CRenderer::EF_StartEf(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	m_beginFrameCount++;

	// Prepare for writing into the view, render items can be added after this
	passInfo.GetRenderView()->SetFrameId(passInfo.GetFrameID());
	passInfo.GetRenderView()->SwitchUsageMode(CRenderView::eUsageModeWriting);
	passInfo.GetRenderView()->Clear();

	ASSERT_IS_MAIN_THREAD(m_pRT)
	int nThreadID = passInfo.ThreadID();
	assert(nThreadID == SRenderThread::GetLocalThreadCommandBufferId());
	int nR = passInfo.GetRecursiveLevel();
	assert(nR < 2);
	if (!passInfo.IsRecursivePass())
	{
		m_RP.m_TempObjects[nThreadID].resize(0);
		m_nShadowGenId[nThreadID] = 0;

		//SG frustums
		//SRendItem::m_ShadowGenRecurLevel[nThreadID] = 0;

		// Clear all cached lists of shadow frustums
		for (ShadowFrustumListsCache::iterator it = m_FrustumsCache.begin(); it != m_FrustumsCache.end(); ++it)
		{
			if (it->second)
				it->second->Clear();
		}

		EF_RemovePolysFromScene();
		m_RP.m_fogVolumeContibutions[nThreadID].resize(0);

		m_pRT->RC_SetStereoEye(0);

		ClearSkinningDataPool();
	}

#ifndef _RELEASE
	if (nR >= MAX_REND_RECURSION_LEVELS)
	{
		CryLogAlways("nR (%d) >= MAX_REND_RECURSION_LEVELS (%d)\n", nR, MAX_REND_RECURSION_LEVELS);
		__debugbreak(); // otherwise about to go out of bounds in the loop below
	}
#endif

#if REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS
	// Refraction Partial Resolves debug views
	if (CRenderer::CV_r_RefractionPartialResolvesDebug == eRPR_DEBUG_VIEW_3D_BOUNDS)
	{
		gEnv->pParticleManager->RenderDebugInfo();
	}
#endif

	passInfo.GetRenderView()->PrepareForWriting();
	//EF_PushObjectsList(nID);

	CPostEffectsMgr* pPostEffectMgr = PostEffectMgr();

	if (pPostEffectMgr)
	{
		pPostEffectMgr->OnBeginFrame(passInfo);
	}
	ClearPerFrameData();
}

void CRenderer::RT_PrepareLevelTexStreaming()
{
	if (CRenderer::CV_r_texturesstreampooldefragmentation)
	{
	}
}

void CRenderer::RT_PostLevelLoading()
{
	int nThreadID = m_pRT->GetThreadList();
	m_RP.m_fogVolumeContibutions[nThreadID].reserve(2048);

	m_cEF.m_Bin.InvalidateCache();
	CHWShader::mfCleanupCache();
	CResFile::m_nMaxOpenResFiles = 4;
}

void CRenderer::RT_DisableTemporalEffects()
{
	m_nDisableTemporalEffects = GetActiveGPUCount();
}

void CRenderer::RT_FlashRender(IFlashPlayer_RenderProxy* pPlayer, bool stereo)
{
	m_pRT->RC_FlashRender(pPlayer, stereo);
}

void CRenderer::RT_FlashRenderPlaybackLockless(IFlashPlayer_RenderProxy* pPlayer, int cbIdx, bool stereo, bool finalPlayback)
{
	m_pRT->RC_FlashRenderPlaybackLockless(pPlayer, cbIdx, stereo, finalPlayback);
}

void CRenderer::RT_FlashRemoveTexture(ITexture* pTexture)
{
	pTexture->Release();
}

void CRenderer::DrawStringU(IFFont_RenderProxy* pFont, float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx) const
{
	m_pRT->RC_DrawStringU(pFont, x, y, z, pStr, asciiMultiLine, ctx);
}

void CRenderer::EF_SubmitWind(const SWindGrid* pWind)
{
	FUNCTION_PROFILER_RENDERER

	m_pRT->RC_SubmitWind(pWind);
}

void CRenderer::RT_CreateREPostProcess(CRendElementBase** re)
{
	*re = new CREPostProcess;
}

CRendElementBase* CRenderer::EF_CreateRE(EDataType edt)
{
	CRendElementBase* re = NULL;
	switch (edt)
	{
	case eDATA_Mesh:
		re = new CREMeshImpl;
		break;
	case eDATA_Imposter:
		re = new CREImposter;
		break;
	case eDATA_HDRProcess:
		re = new CREHDRProcess;
		break;

	case eDATA_DeferredShading:
		re = new CREDeferredShading;
		break;

	case eDATA_OcclusionQuery:
		re = new CREOcclusionQuery;
		break;

	case eDATA_LensOptics:
		re = new CRELensOptics;
		break;
	case eDATA_Cloud:
		re = new CRECloud;
		break;
	case eDATA_Sky:
		re = new CRESky;
		break;

	case eDATA_HDRSky:
		re = new CREHDRSky;
		break;

	case eDATA_Beam:
		re = new CREBeam;
		break;

	case eDATA_FarTreeSprites:
		re = new CREFarTreeSprites;
		break;

	case eDATA_PostProcess:
		re = new CREPostProcess;
		break;

	case eDATA_FogVolume:
		re = new CREFogVolume;
		break;

	case eDATA_WaterVolume:
		re = new CREWaterVolume;
		break;

	case eDATA_WaterOcean:
		re = new CREWaterOcean;
		break;
	case eDATA_VolumeObject:
		re = new CREVolumeObject;
		break;
#if !defined(EXCLUDE_DOCUMENTATION_PURPOSE)
	case eDATA_PrismObject:
		re = new CREPrismObject;
		break;
#endif  // EXCLUDE_DOCUMENTATION_PURPOSE

	case eDATA_GameEffect:
		re = new CREGameEffect;
		break;

	case eDATA_BreakableGlass:
		re = new CREBreakableGlass;
		break;
#if defined(USE_GEOM_CACHES)
	case eDATA_GeomCache:
		re = new CREGeomCache;
		break;
#endif
	}
	return re;
}

float CRenderer::EF_GetWaterZElevation(float fX, float fY)
{
	I3DEngine* eng = (I3DEngine*)gEnv->p3DEngine;
	if (!eng)
		return 0;
	return eng->GetWaterLevel();
}

void CRenderer::EF_RemovePolysFromScene()
{
	ASSERT_IS_MAIN_THREAD(m_pRT)

	m_RP.m_SysVertexPool[m_RP.m_nFillThreadID].SetUse(0);
	m_RP.m_SysIndexPool[m_RP.m_nFillThreadID].SetUse(0);
}

void CRenderer::FX_StartMerging()
{
	if (m_RP.m_FrameMerge != m_RP.m_Frame)
	{
		m_RP.m_FrameMerge = m_RP.m_Frame;
		SBufInfoTable* pOffs = &CRenderMesh::m_cBufInfoTable[m_RP.m_CurVFormat];
		int Size = CRenderMesh::m_cSizeVF[m_RP.m_CurVFormat];
		m_RP.m_StreamStride = Size;
		m_RP.m_StreamOffsetColor = pOffs->OffsColor;
		m_RP.m_StreamOffsetTC = pOffs->OffsTC;
		m_RP.m_NextStreamPtr = m_RP.m_StreamPtr;
		m_RP.m_NextStreamPtrTang = m_RP.m_StreamPtrTang;
	}
}

void CRenderer::EF_PushFog()
{
	assert(m_pRT->IsRenderThread());
	int nLevel = m_nCurFSStackLevel;
	if (nLevel >= 8)
		return;
	memcpy(&m_FSStack[nLevel], &m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS, sizeof(SFogState));
	m_nCurFSStackLevel++;
}

void CRenderer::EF_PopFog()
{
	assert(m_pRT->IsRenderThread());
	int nLevel = m_nCurFSStackLevel;
	if (nLevel <= 0)
		return;
	nLevel--;
	bool bFog = m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS.m_bEnable;
	if (m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS != m_FSStack[nLevel])
	{
		memcpy(&m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS, &m_FSStack[nLevel], sizeof(SFogState));
		SetFogColor(m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS.m_FogColor);
	}
	else
		m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS.m_bEnable = m_FSStack[nLevel].m_bEnable;
	bool bNewFog = m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS.m_bEnable;
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS.m_bEnable = bFog;
	EnableFog(bNewFog);
	m_nCurFSStackLevel--;
}

void CRenderer::FX_PushVP()
{
	int nLevel = m_nCurVPStackLevel;
	if (nLevel >= 8)
		return;
	memcpy(&m_VPStack[nLevel], &m_NewViewport, sizeof(SViewport));
	m_nCurVPStackLevel++;
}

void CRenderer::FX_PopVP()
{
	int nLevel = m_nCurVPStackLevel;
	if (nLevel <= 0)
		return;
	nLevel--;
	if (m_NewViewport != m_VPStack[nLevel])
	{
		memcpy(&m_NewViewport, &m_VPStack[nLevel], sizeof(SViewport));
		m_bViewportDirty = true;
	}
	m_nCurVPStackLevel--;
}

eAntialiasingType CRenderer::FX_GetAntialiasingType() const
{
	return (eAntialiasingType)((uint32)1 << min(CV_r_AntialiasingMode, eAT_AAMODES_COUNT - 1));
}

uint32 CRenderer::FX_GetMSAAMode() const
{
#ifdef SUPPORTS_MSAA
	return m_RP.m_MSAAData.Type ? CV_r_msaa_debug + 1 : 0;
#else
	return 0;
#endif
}

void CRenderer::FX_ResetMSAAFlagsRT()
{
#ifdef SUPPORTS_MSAA
	m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_MSAA_QUALITY] | g_HWSR_MaskBit[HWSR_MSAA_QUALITY1] | g_HWSR_MaskBit[HWSR_MSAA_SAMPLEFREQ_PASS]);
#endif
}

void CRenderer::FX_SetMSAAFlagsRT()
{
#ifdef SUPPORTS_MSAA
	const uint32& nMSAAType = m_RP.m_MSAAData.Type;
	const uint32& nPerfsFlag2 = m_RP.m_PersFlags2;
	if (nMSAAType)
	{
		FX_ResetMSAAFlagsRT();
		if ((m_RP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS) && CV_r_msaa_debug != 1)
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_MSAA_SAMPLEFREQ_PASS];

		if (nMSAAType == 8)
			m_RP.m_FlagsShader_RT |= (g_HWSR_MaskBit[HWSR_MSAA_QUALITY] | g_HWSR_MaskBit[HWSR_MSAA_QUALITY1]);
		else if (nMSAAType == 4)
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_MSAA_QUALITY];
		else if (nMSAAType == 2)
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_MSAA_QUALITY1];
	}
#endif
}

CRenderObject* CRenderer::EF_AddPolygonToScene(SShaderItem& si, int numPts, const SVF_P3F_C4B_T2F* verts, const SPipTangents* tangs, CRenderObject* obj, const SRenderingPassInfo& passInfo, uint16* inds, int ninds, int nAW)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)
	int nThreadID = m_RP.m_nFillThreadID;
	const uint32 nPersFlags = m_RP.m_TI[nThreadID].m_PersFlags;

	assert(si.m_pShader && si.m_pShaderResources);
	if (!si.m_pShader || !si.m_pShaderResources)
	{
		Warning("CRenderer::EF_AddPolygonToScene without shader...");
		return NULL;
	}
	if (si.m_nPreprocessFlags == -1)
	{
		if (!si.Update())
			return obj;
	}

	CREClientPoly* pl = passInfo.GetRenderView()->AddClientPoly();

	pl->m_Shader = si;
	pl->m_sNumVerts = numPts;
	pl->m_pObject = obj;
	pl->m_nCPFlags = 0;
	pl->rendItemSorter = passInfo.GetRendItemSorter();
	if (nAW)
		pl->m_nCPFlags |= CREClientPoly::efAfterWater;
	if (passInfo.IsShadowPass())
		pl->m_nCPFlags |= CREClientPoly::efShadowGen;

	int nSize = CRenderMesh::m_cSizeVF[eVF_P3F_C4B_T2F] * numPts;
	int nOffs = m_RP.m_SysVertexPool[nThreadID].Num();
	SVF_P3F_C4B_T2F* vt = (SVF_P3F_C4B_T2F*)m_RP.m_SysVertexPool[nThreadID].GrowReset(nSize);
	pl->m_nOffsVert = nOffs;
	for (int i = 0; i < numPts; i++, vt++)
	{
		vt->xyz = verts[i].xyz;
		vt->st = verts[i].st;
		vt->color.dcolor = verts[i].color.dcolor;
	}
	if (tangs)
	{
		nSize = sizeof(SPipTangents) * numPts;
		nOffs = m_RP.m_SysVertexPool[nThreadID].Num();
		SPipTangents* t = (SPipTangents*)m_RP.m_SysVertexPool[nThreadID].GrowReset(nSize);
		pl->m_nOffsTang = nOffs;
		for (int i = 0; i < numPts; i++, t++)
		{
			*t = tangs[i];
		}
	}
	else
		pl->m_nOffsTang = -1;

	pl->m_nOffsInd = m_RP.m_SysIndexPool[nThreadID].Num();

	if (inds && ninds)
	{
		uint16* dstind = m_RP.m_SysIndexPool[nThreadID].Grow(ninds);
		memcpy(dstind, inds, ninds * sizeof(uint16));
		pl->m_sNumIndices = ninds;
	}
	else
	{
		uint16* dstind = m_RP.m_SysIndexPool[nThreadID].Grow((numPts - 2) * 3);
		for (int i = 0; i < numPts - 2; i++, dstind += 3)
		{
			dstind[0] = 0;
			dstind[1] = i + 1;
			dstind[2] = i + 2;
		}
		pl->m_sNumIndices = (numPts - 2) * 3;
	}

	return obj;
}

CRenderObject* CRenderer::EF_AddPolygonToScene(SShaderItem& si, CRenderObject* obj, const SRenderingPassInfo& passInfo, int numPts, int ninds, SVF_P3F_C4B_T2F*& verts, SPipTangents*& tangs, uint16*& inds, int nAW)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)
	int nThreadID = m_RP.m_nFillThreadID;
	const uint32 nPersFlags = m_RP.m_TI[nThreadID].m_PersFlags;

	assert(si.m_pShader && si.m_pShaderResources);
	if (!si.m_pShader || !si.m_pShaderResources)
	{
		Warning("CRenderer::EF_AddPolygonToScene without shader...");
		return NULL;
	}
	if (si.m_nPreprocessFlags == -1)
	{
		if (!si.Update())
			return obj;
	}
	CREClientPoly* pl = passInfo.GetRenderView()->AddClientPoly();

	pl->m_Shader = si;
	pl->m_pObject = obj;
	if (nAW)
		pl->m_nCPFlags |= CREClientPoly::efAfterWater;
	if (passInfo.IsShadowPass())
		pl->m_nCPFlags |= CREClientPoly::efShadowGen;

	//////////////////////////////////////////////////////////////////////////
	// allocate buffer space for caller to fill

	pl->m_sNumVerts = numPts;
	pl->m_nOffsVert = m_RP.m_SysVertexPool[nThreadID].Num();
	pl->m_nOffsTang = m_RP.m_SysVertexPool[nThreadID].Num() + sizeof(SVF_P3F_C4B_T2F) * numPts;
	m_RP.m_SysVertexPool[nThreadID].GrowReset((sizeof(SVF_P3F_C4B_T2F) + sizeof(SPipTangents)) * numPts);
	verts = (SVF_P3F_C4B_T2F*) &m_RP.m_SysVertexPool[nThreadID][pl->m_nOffsVert];
	tangs = (SPipTangents*) &m_RP.m_SysVertexPool[nThreadID][pl->m_nOffsTang];

	pl->m_sNumIndices = ninds;
	pl->m_nOffsInd = m_RP.m_SysIndexPool[nThreadID].Num();
	inds = m_RP.m_SysIndexPool[nThreadID].Grow(ninds);

	//////////////////////////////////////////////////////////////////////////

	return obj;
}

void CRenderer::Logv(const char* format, ...)
{
	va_list argptr;
	int RecLevel = IsRecursiveRenderView() ? 1 : 0;

	if (m_LogFile)
	{
		for (int i = 0; i < RecLevel; i++)
		{
			fprintf(m_LogFile, "  ");
		}
		va_start(argptr, format);
		vfprintf(m_LogFile, format, argptr);
		va_end(argptr);
	}
}

void CRenderer::LogStrv(char* format, ...)
{
	int RecLevel = IsRecursiveRenderView() ? 1 : 0;
	va_list argptr;

	if (m_LogFileStr)
	{
		for (int i = 0; i < RecLevel; i++)
		{
			fprintf(m_LogFileStr, "  ");
		}
		va_start(argptr, format);
		vfprintf(m_LogFileStr, format, argptr);
		va_end(argptr);
	}
}

void CRenderer::LogShv(char* format, ...)
{
	int RecLevel = IsRecursiveRenderView() ? 1 : 0;
	va_list argptr;

	if (m_LogFileSh)
	{
		for (int i = 0; i < RecLevel; i++)
		{
			fprintf(m_LogFileSh, "  ");
		}
		va_start(argptr, format);
		vfprintf(m_LogFileSh, format, argptr);
		va_end(argptr);
		fflush(m_LogFileSh);
	}
}

void CRenderer::Log(char* str)
{
	if (m_LogFile)
	{
		fprintf(m_LogFile, "%s", str);
	}
}

// Dynamic lights
bool CRenderer::EF_IsFakeDLight(const CDLight* Source)
{
	if (!Source)
	{
		iLog->Log("Warning: EF_IsFakeDLight: NULL light source\n");
		return true;
	}

	bool bIgnore = false;
	if (Source->m_Flags & (DLF_FAKE))
		bIgnore = true;

	return bIgnore;
}

void CRenderer::EF_CheckLightMaterial(CDLight* pLight, uint16 nRenderLightID, const SRenderingPassInfo& passInfo)
{
	ASSERT_IS_MAIN_THREAD(m_pRT);
	const int32 nThreadID = m_RP.m_nFillThreadID;

	if (!(m_RP.m_TI[nThreadID].m_PersFlags & RBPF_IMPOSTERGEN))
	{
		// Add render element if light has mtl bound
		IShader* pShader = pLight->m_Shader.m_pShader;
		TArray<CRendElementBase*>* pRendElemBase = pShader ? pLight->m_Shader.m_pShader->GetREs(pLight->m_Shader.m_nTechnique) : 0;
		if (pRendElemBase && !pRendElemBase->empty())
		{
			CRenderObject* pRO = EF_GetObject_Temp(passInfo.ThreadID());
			pRO->m_fAlpha = 1.0f;
			pRO->m_II.m_AmbColor = Vec3(0, 0, 0);

			SRenderObjData* pOD = pRO->GetObjData();

			pOD->m_fTempVars[0] = 0;
			pOD->m_fTempVars[1] = 0;
			pOD->m_fTempVars[3] = pLight->m_fRadius;
			pOD->m_nLightID = nRenderLightID;

			pRO->m_II.m_AmbColor = pLight->m_Color;
			pRO->m_II.m_Matrix = pLight->m_ObjMatrix;
			pRO->m_ObjFlags |= FOB_TRANS_MASK;

			CRendElementBase* pRE = pRendElemBase->Get(0);
			const int32 nList = (pRE->mfGetType() != eDATA_LensOptics) ? EFSLIST_TRANSP : EFSLIST_LENSOPTICS;

			if (pRE->mfGetType() == eDATA_Beam)
			{
				pLight->m_Flags |= DLF_LIGHT_BEAM;
			}

			const float fWaterLevel = gEnv->p3DEngine->GetWaterLevel();
			const float fCamZ = m_RP.m_TI[nThreadID].m_cam.GetPosition().z;
			const int32 nAW = ((fCamZ - fWaterLevel) * (pLight->m_Origin.z - fWaterLevel) > 0) ? 1 : 0;

			EF_AddEf(pRE, pLight->m_Shader, pRO, passInfo, nList, nAW);
		}
	}
}

void CRenderer::EF_ADDDlight(CDLight* Source, const SRenderingPassInfo& passInfo)
{
	if (!Source)
	{
		iLog->Log("Warning: EF_ADDDlight: NULL light source\n");
		return;
	}

	passInfo.GetRenderView()->AddDynamicLight(*Source);

	EF_PrecacheResource(Source, (passInfo.GetCamera().GetPosition() - Source->m_Origin).GetLengthSquared() / max(0.001f, Source->m_fRadius * Source->m_fRadius), 0.1f, 0, 0);

	//EF_CheckLightMaterial(Source, pNew, passInfo);
}

bool CRenderer::EF_AddDeferredDecal(const SDeferredDecal& rDecal, const SRenderingPassInfo& passInfo)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)

	if (passInfo.GetRenderView()->GetDeferredDecalsCount() < 1024)
	{
		SDeferredDecal& rDecalCopy = *passInfo.GetRenderView()->AddDeferredDecal(rDecal);

		//////////////////////////////////////////////////////////////////////////
		IMaterial* pDecalMaterial = rDecalCopy.pMaterial;
		if (pDecalMaterial == NULL)
		{
			assert(0);
			return false;
		}

		SShaderItem& sItem = pDecalMaterial->GetShaderItem(0);
		if (sItem.m_pShaderResources == NULL)
		{
			assert(0);
			return false;
		}

		if (SEfResTexture* pNormalRes0 = sItem.m_pShaderResources->GetTexture(EFTT_NORMALS))
		{
			if (pNormalRes0->m_Sampler.m_pITex)
			{
				rDecalCopy.nFlags |= DECAL_HAS_NORMAL_MAP;
				passInfo.GetRenderView()->SetDeferredNormalDecals(true);
			}
			else
			{
				rDecalCopy.nFlags &= ~DECAL_HAS_NORMAL_MAP;
			}
		}

		if (SEfResTexture* pSpecularRes0 = sItem.m_pShaderResources->GetTexture(EFTT_SPECULAR))
		{
			if (pSpecularRes0->m_Sampler.m_pITex)
			{
				rDecalCopy.nFlags |= DECAL_HAS_SPECULAR_MAP;
			}
			else
			{
				rDecalCopy.nFlags &= ~DECAL_HAS_SPECULAR_MAP;
			}
		}
		//////////////////////////////////////////////////////////////////////////

		if (CV_r_deferredDecalsDebug)
		{
			Vec3 vCenter = rDecalCopy.projMatrix.GetTranslation();
			float fSize = rDecalCopy.projMatrix.GetColumn(2).GetLength();
			Vec3 vSize(fSize, fSize, fSize);
			AABB aabbCenter(vCenter - vSize * 0.05f, vCenter + vSize * 0.05f);
			GetIRenderAuxGeom()->DrawAABB(aabbCenter, false, Col_Yellow, eBBD_Faceted);
			GetIRenderAuxGeom()->DrawLine(vCenter, Col_Red, vCenter + rDecalCopy.projMatrix.GetColumn(0), Col_Red);
			GetIRenderAuxGeom()->DrawLine(vCenter, Col_Green, vCenter + rDecalCopy.projMatrix.GetColumn(1), Col_Green);
			GetIRenderAuxGeom()->DrawLine(vCenter, Col_Blue, vCenter + rDecalCopy.projMatrix.GetColumn(2), Col_Blue);
		}

		return true;
	}
	return false;
}

void CRenderer::ClearPerFrameData()
{
}

inline Matrix44 ToLightMatrix(const Ang3& angle)
{
	Matrix33 ViewMatZ = Matrix33::CreateRotationZ(-angle.x);
	Matrix33 ViewMatX = Matrix33::CreateRotationX(-angle.y);
	Matrix33 ViewMatY = Matrix33::CreateRotationY(+angle.z);
	return Matrix44(ViewMatX * ViewMatY * ViewMatZ).GetTransposed();
}

bool CRenderer::EF_UpdateDLight(SRenderLight* dl)
{
	if (!dl)
		return false;

	float fTime = iTimer->GetCurrTime() * dl->GetAnimSpeed();

	const uint32 nStyle = dl->m_nLightStyle;

	if (nStyle > 0 && nStyle < CLightStyle::s_LStyles.Num() && CLightStyle::s_LStyles[nStyle])
	{
		CLightStyle* ls = CLightStyle::s_LStyles[nStyle];

		const float fRecipMaxInt8 = 1.0f / 255.0f;

		// Add user light phase
		float fPhaseFromID = ((float)dl->m_nLightPhase) * fRecipMaxInt8;
		fTime += (fPhaseFromID - floorf(fPhaseFromID)) * ls->m_TimeIncr;

		ls->mfUpdate(fTime);

		dl->m_Color = dl->m_BaseColor * ls->m_Color;
		dl->m_SpecMult = dl->m_BaseSpecMult * ls->m_Color.a;
		dl->m_Origin = dl->m_BaseOrigin + ls->m_vPosOffset;
	}
	else
	{
		dl->m_Color = dl->m_BaseColor;
	}

	/*if(IsHDRModeEnabled() && !(dl->m_Flags&(DLF_SUN|DLF_POST_3D_RENDERER)))
	   {
	   I3DEngine *p3DEngine = (I3DEngine *)gEnv->p3DEngine;		assert(p3DEngine);
	   const float fHDR = powf( HDRDynamicMultiplier,dl->m_fHDRDynamic + p3DEngine->GetLightsHDRDynamicPowerFactor() );
	   dl->m_Color *= fHDR;
	   }*/

	return false;
}

void CRenderer::FX_ApplyShaderQuality(const EShaderType eST)
{
	SShaderProfile* const pSP = &m_cEF.m_ShaderProfiles[eST];
	const uint64 quality = g_HWSR_MaskBit[HWSR_QUALITY];
	const uint64 quality1 = g_HWSR_MaskBit[HWSR_QUALITY1];
	m_RP.m_FlagsShader_RT &= ~(quality | quality1);
	int nQuality = (int)pSP->GetShaderQuality();
	m_RP.m_nShaderQuality = nQuality;
	switch (nQuality)
	{
	case eSQ_Medium:
		m_RP.m_FlagsShader_RT |= quality;
		break;
	case eSQ_High:
		m_RP.m_FlagsShader_RT |= quality1;
		break;
	case eSQ_VeryHigh:
		m_RP.m_FlagsShader_RT |= (quality | quality1);
		break;
	}
}

EShaderQuality CRenderer::EF_GetShaderQuality(EShaderType eST)
{
	SShaderProfile* pSP = &m_cEF.m_ShaderProfiles[eST];
	int nQuality = (int)pSP->GetShaderQuality();

	switch (nQuality)
	{
	case eSQ_Low:
		return eSQ_Low;
	case eSQ_Medium:
		return eSQ_Medium;
	case eSQ_High:
		return eSQ_High;
	case eSQ_VeryHigh:
		return eSQ_VeryHigh;
	}

	return eSQ_Low;
}

ERenderQuality CRenderer::EF_GetRenderQuality() const
{
	return (ERenderQuality) m_RP.m_eQuality;
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( push )           //AMD Port
	#pragma warning( disable : 4312 ) // 'type cast' : conversion from 'int' to 'void *' of greater size
#endif

int CRenderer::RT_CurThreadList()
{
	return m_pRT->GetThreadList();
}

namespace {
// util funtion to write to the provided output memory
template<typename T>
void WriteQueryResult(void* pOutput, uint32 nOutputSize, const T& rQueryResult)
{
#if !defined(_RELEASE)
	if (pOutput == NULL)
		CryFatalError("No Output Storage Specified");
	if (sizeof(T) != nOutputSize)
		CryFatalError("Insufficient storage for EF_Query Output");
#endif
	*alias_cast<T*>(pOutput) = rQueryResult;
}

// util function to read POD types from query parameters
template<typename T>
T ReadQueryParameter(void* pInput, uint32 nInputSize)
{
#if !defined(_RELEASE)
	if (pInput == NULL)
		CryFatalError("No Input Storage Specified");
	if (sizeof(T) != nInputSize)
		CryFatalError("Insufficient storage for EF_Query Input");
#endif
	return *alias_cast<T*>(pInput);
}
}

void CRenderer::EF_QueryImpl(ERenderQueryTypes eQuery, void* pInOut0, uint32 nInOutSize0, void* pInOut1, uint32 nInOutSize1)
{
	switch (eQuery)
	{
	case EFQ_DeleteMemoryArrayPtr:
		{
			char* pPtr = ReadQueryParameter<char*>(pInOut0, nInOutSize0);
			delete[] pPtr;
		}
		break;

	case EFQ_DeleteMemoryPtr:
		{
			char* pPtr = ReadQueryParameter<char*>(pInOut0, nInOutSize0);
			delete pPtr;
		}
		break;

	case EFQ_MainThreadList:
		{
			WriteQueryResult(pInOut0, nInOutSize0, m_RP.m_nFillThreadID);
		}
		break;

	case EFQ_RenderThreadList:
		{
			WriteQueryResult(pInOut0, nInOutSize0, m_RP.m_nProcessThreadID);
		}
		break;

	case EFQ_RenderMultithreaded:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(m_pRT->IsMultithreaded()));
		}
		break;

	case EFQ_IncrementFrameID:
		{
			m_RP.m_TI[m_pRT->GetThreadList()].m_nFrameID += 1;
		}
		break;

	case EFQ_DeviceLost:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(m_bDeviceLost != 0));
		}
		break;

	case EFQ_RecurseLevel:
		{
			int recursion = IsRecursiveRenderView() ? 1 : 0;
			WriteQueryResult(pInOut0, nInOutSize0, recursion);
		}
		break;

	case EFQ_Alloc_APITextures:
		{
			int nSize = 0;
			SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
			if (pRL)
			{
				ResourcesMapItor itor;
				for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
				{
					CTexture* tp = (CTexture*)itor->second;
					if (!tp || tp->IsNoTexture())
						continue;
					if (!(tp->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET)))
						nSize += tp->GetDeviceDataSize();
				}
			}
			WriteQueryResult(pInOut0, nInOutSize0, nSize);
		}
		break;

	case EFQ_Alloc_APIMesh:
		{
			uint32 nSize = 0;
			for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.next; iter != &CRenderMesh::m_MeshList; iter = iter->next)
			{
				CRenderMesh* pRM = iter->item<& CRenderMesh::m_Chain>();
				nSize += static_cast<uint32>(pRM->Size(CRenderMesh::SIZE_VB));
				nSize += static_cast<uint32>(pRM->Size(CRenderMesh::SIZE_IB));
			}
			WriteQueryResult(pInOut0, nInOutSize0, nSize);
		}
		break;

	case EFQ_Alloc_Mesh_SysMem:
		{
			uint32 nSize = 0;
			for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.next; iter != &CRenderMesh::m_MeshList; iter = iter->next)
			{
				CRenderMesh* pRM = iter->item<& CRenderMesh::m_Chain>();
				nSize += static_cast<uint32>(pRM->Size(CRenderMesh::SIZE_ONLY_SYSTEM));
			}
			WriteQueryResult(pInOut0, nInOutSize0, nSize);
		}
		break;

	case EFQ_Mesh_Count:
		{
			uint32 nCount = 0;
			AUTO_LOCK(CRenderMesh::m_sLinkLock);
			for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.next; iter != &CRenderMesh::m_MeshList; iter = iter->next)
			{
				++nCount;
			}
			WriteQueryResult(pInOut0, nInOutSize0, nCount);
		}
		break;

	case EFQ_GetAllMeshes:
		{
			//Get render mesh lock, to ensure that the mesh list doesn't change while we're copying
			AUTO_LOCK(CRenderMesh::m_sLinkLock);
			IRenderMesh** ppMeshes = NULL;
			uint32 nSize = 0;
			for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.next; iter != &CRenderMesh::m_MeshList; iter = iter->next)
			{
				++nSize;
			}
			if (pInOut0 && nSize)
			{
				//allocate the array. The calling function is responsible for cleaning it up.
				ppMeshes = new IRenderMesh*[nSize];
				nSize = 0;
				for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.next; iter != &CRenderMesh::m_MeshList; iter = iter->next)
				{
					ppMeshes[nSize] = iter->item<& CRenderMesh::m_Chain>();
					;
					nSize++;
				}
			}
			WriteQueryResult(pInOut0, nInOutSize0, ppMeshes);
			WriteQueryResult(pInOut1, nInOutSize1, nSize);
		}
		break;

	case EFQ_GetAllTextures:
		{
			AUTO_LOCK(CBaseResource::s_cResLock);

			SRendererQueryGetAllTexturesParam* pParam = (SRendererQueryGetAllTexturesParam*)(pInOut0);
			pParam->pTextures = NULL;
			pParam->numTextures = 0;

			SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
			if (pRL)
			{
				for (ResourcesMapItor itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
				{
					CTexture* tp = (CTexture*)itor->second;
					if (!tp || tp->IsNoTexture())
						continue;
					++pParam->numTextures;
				}

				if (pParam->numTextures > 0)
				{
					pParam->pTextures = new _smart_ptr<ITexture>[pParam->numTextures];

					uint32 texIdx = 0;
					for (ResourcesMapItor itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
					{
						CTexture* tp = (CTexture*)itor->second;
						if (!tp || tp->IsNoTexture())
							continue;
						pParam->pTextures[texIdx++] = tp;
					}
				}
			}
		}
		break;

	case EFQ_GetAllTexturesRelease:
		{
			SRendererQueryGetAllTexturesParam* pParam = (SRendererQueryGetAllTexturesParam*)(pInOut0);
			SAFE_DELETE_ARRAY(pParam->pTextures);
		}
		break;

	case EFQ_TexturesPoolSize:
		{
			uint32 streamPoolSize = (uint32)(CRenderer::GetTexturesStreamPoolSize() * 1024 * 1024);
			WriteQueryResult(pInOut0, nInOutSize0, streamPoolSize);
		}
		break;

	case EFQ_RenderTargetPoolSize:
		{
			WriteQueryResult(pInOut0, nInOutSize0, (CRenderer::CV_r_rendertargetpoolsize + 2) * 1024 * 1024);
		}
		break;

	case EFQ_HDRModeEnabled:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(IsHDRModeEnabled() ? 1 : 0));
		}
		break;

	case EFQ_ParticlesTessellation:
		{
#if defined(PARTICLES_TESSELLATION_RENDERER)
			WriteQueryResult<bool>(pInOut0, nInOutSize0, m_bDeviceSupportsTessellation && CV_r_ParticlesTessellation != 0);
#else
			WriteQueryResult<bool>(pInOut0, nInOutSize0, false);
#endif
		}
		break;

	case EFQ_WaterTessellation:
		{
#if defined(WATER_TESSELLATION_RENDERER)
			WriteQueryResult<bool>(pInOut0, nInOutSize0, m_bDeviceSupportsTessellation && CV_r_WaterTessellationHW != 0);
#else
			WriteQueryResult<bool>(pInOut0, nInOutSize0, false);
#endif
		}
		break;

	case EFQ_MeshTessellation:
		{
#if defined(MESH_TESSELLATION_RENDERER)
			WriteQueryResult<bool>(pInOut0, nInOutSize0, m_bDeviceSupportsTessellation);
#else
			WriteQueryResult<bool>(pInOut0, nInOutSize0, false);
#endif
		}
		break;

#ifndef _RELEASE
	case EFQ_GetShadowPoolFrustumsNum:
		{
			WriteQueryResult(pInOut0, nInOutSize0, m_RP.m_PS[m_RP.m_nFillThreadID].m_NumShadowPoolFrustums);
		}
		break;

	case EFQ_GetShadowPoolAllocThisFrameNum:
		{
			WriteQueryResult(pInOut0, nInOutSize0, m_RP.m_PS[m_RP.m_nFillThreadID].m_NumShadowPoolAllocsThisFrame);
		}
		break;

	case EFQ_GetShadowMaskChannelsNum:
		{
			WriteQueryResult(pInOut0, nInOutSize0, m_RP.m_PS[m_RP.m_nFillThreadID].m_NumShadowMaskChannels);
		}
		break;

	case EFQ_GetTiledShadingSkippedLightsNum:
		{
			WriteQueryResult(pInOut0, nInOutSize0, m_RP.m_PS[m_RP.m_nFillThreadID].m_NumTiledShadingSkippedLights);
		}
		break;
#endif

	case EFQ_MultiGPUEnabled:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(GetActiveGPUCount() > 1 ? 1 : 0));
		}
		break;

	// deprecated, always enabled
	case EFQ_sLinearSpaceShadingEnabled:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(1));
		}
		break;

	case EFQ_SetDrawNearFov:
		{
			CV_r_drawnearfov = ReadQueryParameter<float>(pInOut0, nInOutSize0);
		}
		break;

	case EFQ_GetDrawNearFov:
		{
			WriteQueryResult(pInOut0, nInOutSize0, CV_r_drawnearfov);
		}
		break;

	case EFQ_TextureStreamingEnabled:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(CRenderer::CV_r_texturesstreaming ? 1 : 0));
		}
		break;

	case EFQ_MSAAEnabled:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(m_RP.IsMSAAEnabled() ? 1 : 0));
		}
		break;

	case EFQ_AAMode:
		{
			const uint32 nMode = CRenderer::CV_r_AntialiasingMode;
			WriteQueryResult(pInOut0, nInOutSize0, s_pszAAModes[nMode]);
		}
		break;

	case EFQ_GetShaderCombinations:
	case EFQ_SetShaderCombinations:
	case EFQ_CloseShaderCombinations:
		{
			// no longer used, can be ignored
		}
		break;

	case EFQ_Fullscreen:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(QueryIsFullscreen() ? 1 : 0));
		}
		break;

	case EFQ_GetTexStreamingInfo:
		{
			STextureStreamingStats* stats = (STextureStreamingStats*)(pInOut0);
			// Guard CrashHandler for nullptr access if texture streaming is turned off
			if (stats && gRenDev && CTexture::s_pTextureStreamer && CTexture::s_pPoolMgr)
			{
#if CRY_PLATFORM_DURANGO
				IDefragAllocatorStats allocStats = m_DevMan.GetTexturePoolStats();
				stats->nCurrentPoolSize = allocStats.nInUseSize;
#else
				stats->nCurrentPoolSize = CTexture::s_pPoolMgr->GetReservedSize();    // s_nStatsStreamPoolInUseMem;
#endif
				stats->nStreamedTexturesSize = CTexture::s_nStatsStreamPoolInUseMem;

				stats->nStaticTexturesSize = CTexture::s_nStatsCurManagedNonStreamedTexMem;
				stats->bPoolOverflow = CTexture::s_pTextureStreamer->IsOverflowing();
				stats->bPoolOverflowTotally = CTexture::s_bOutOfMemoryTotally;
				CTexture::s_bOutOfMemoryTotally = false;
				stats->nMaxPoolSize = CRenderer::GetTexturesStreamPoolSize() * 1024 * 1024;
				stats->nThroughput = (CTexture::s_nStreamingTotalTime > 0.f) ? size_t((double)CTexture::s_nStreamingThroughput / CTexture::s_nStreamingTotalTime) : 0;

#ifndef _RELEASE
				stats->nNumTexturesPerFrame = gRenDev->m_RP.m_PS[gRenDev->m_RP.m_nProcessThreadID].m_NumTextures;
#endif

#if CRY_PLATFORM_DURANGO
				stats->fPoolFragmentation = (allocStats.nCapacity > 0)
				                            ? (allocStats.nCapacity - allocStats.nInUseSize - allocStats.nLargestFreeBlockSize) / (float)allocStats.nCapacity
				                            : 0.0f;
#endif

				if (stats->bComputeReuquiredTexturesPerFrame)
				{
					stats->nRequiredStreamedTexturesCount = 0;
					stats->nRequiredStreamedTexturesSize = 0;

					AUTO_LOCK(CBaseResource::s_cResLock);

					// compute all sizes
					SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
					if (pRL)
					{
						ResourcesMapItor itor;
						for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
						{
							CTexture* tp = (CTexture*)itor->second;
							if (!tp || tp->IsNoTexture() || !tp->IsStreamed())
								continue;

							const STexStreamingInfo* pTSI = tp->GetStreamingInfo();
							if (!pTSI)
								continue;

							int nPersMip = tp->GetNumMipsNonVirtual() - tp->GetNumPersistentMips();
							bool bStale = CTexture::s_pTextureStreamer->StatsWouldUnload(tp);
							int nCurMip = bStale ? nPersMip : tp->GetRequiredMipNonVirtual();
							if (tp->IsForceStreamHighRes())
								nCurMip = 0;
							int nMips = tp->GetNumMipsNonVirtual();
							nCurMip = min(nCurMip, nPersMip);

							int nTexSize = tp->StreamComputeDevDataSize(nCurMip);

							stats->nRequiredStreamedTexturesSize += nTexSize;
							++stats->nRequiredStreamedTexturesCount;
						}
					}
				}

				stats->bPoolOverflow = CTexture::s_pTextureStreamer->IsOverflowing();
				stats->bPoolOverflowTotally = CTexture::s_bOutOfMemoryTotally;
				CTexture::s_bOutOfMemoryTotally = 0;
			}
			if (pInOut1)
				WriteQueryResult(pInOut1, nInOutSize1, static_cast<bool>(CTexture::s_nStreamingTotalTime > 0.f && stats != NULL));
		}
		break;

	case EFQ_GetShaderCacheInfo:
		{
			SShaderCacheStatistics* pStats = (SShaderCacheStatistics*)(pInOut0);
			if (pStats)
			{
				memcpy(pStats, &m_cEF.m_ShaderCacheStats, sizeof(SShaderCacheStatistics));
				pStats->m_bShaderCompileActive = CV_r_shadersAllowCompilation != 0;
			}
		}
		break;

	case EFQ_OverscanBorders:
		{
			WriteQueryResult(pInOut0, nInOutSize0, s_overscanBorders);
		}
		break;

	case EFQ_NumActivePostEffects:
		{
			int nSize = 0;
			if (CV_r_PostProcess && PostEffectMgr())
			{
				//assume query is from main thread
				nSize = PostEffectMgr()->GetActiveEffects(m_RP.m_nFillThreadID).size();
			}

			WriteQueryResult(pInOut0, nInOutSize0, nSize);
		}
		break;

	case EFQ_GetFogCullDistance:
		{
			WriteQueryResult(pInOut0, nInOutSize0, m_fogCullDistance);
		}
		break;

	case EFQ_GetMaxRenderObjectsNum:
		{
			WriteQueryResult(pInOut0, nInOutSize0, MAX_REND_OBJECTS);
		}
		break;

	case EFQ_IsRenderLoadingThreadActive:
		{
			WriteQueryResult(pInOut0, nInOutSize0, static_cast<bool>(m_pRT && m_pRT->m_pThreadLoading ? 1 : 0));
		}
		break;

	case EFQ_GetParticleVertexBufferSize:
		{
			uint32 nParticleVerticePoolSize = CV_r_ParticleVerticePoolSize * sizeof(SVF_P3F_C4B_T4B_N3F2);
			WriteQueryResult(pInOut0, nInOutSize0, nParticleVerticePoolSize);
		}
		break;

	case EFQ_GetParticleIndexBufferSize:
		{
			uint32 nParticleVerticePoolSize = CV_r_ParticleVerticePoolSize * 3 * sizeof(uint16);
			WriteQueryResult(pInOut0, nInOutSize0, nParticleVerticePoolSize);
		}
		break;

	case EFQ_GetMaxParticleContainer:
		{
			uint32 nMaxPartCon = nMaxParticleContainer;
			WriteQueryResult(pInOut0, nInOutSize0, nMaxPartCon);
		}
		break;

	case EFQ_GetSkinningDataPoolSize:
		{
			int nSkinningPoolSize = 0;
			for (int i = 0; i < 3; ++i)
				nSkinningPoolSize += m_SkinningDataPool[i].AllocatedMemory();
			WriteQueryResult(pInOut0, nInOutSize0, nSkinningPoolSize);
		}
		break;

	case EFQ_GetMeshPoolInfo:
		{
			if (SMeshPoolStatistics* stats = (SMeshPoolStatistics*)(pInOut0))
			{
				CRenderMesh::GetPoolStats(stats);
			}
		}
		break;

	case EFQ_SetDynTexSourceLayerInfo:
		{
			CDynTextureSourceLayerActivator::LoadLevelInfo();
		}
		break;

	case EFQ_SetDynTexSourceSharedRTDim:
		{
			if (pInOut0)
			{
				int p0 = ReadQueryParameter<int>(pInOut0, nInOutSize0);
				int p1 = ReadQueryParameter<int>(pInOut1, nInOutSize1);
				CFlashTextureSourceSharedRT::SetSharedRTDim(p0, p1);
			}
		}
		break;

	case EFQ_GetViewportDownscaleFactor:
		{
			WriteQueryResult(pInOut0, nInOutSize0, m_CurViewportScale);
		}
		break;
	case EFQ_ReverseDepthEnabled:
		{
			const uint32 nThreadID = m_pRT->GetThreadList();
			uint32 nReverseDepth = gRenDev->m_RP.m_TI[nThreadID].m_PersFlags & RBPF_REVERSE_DEPTH;

			WriteQueryResult(pInOut0, nInOutSize0, nReverseDepth);
		}
		break;
	case EFQ_GetLastD3DDebugMessage:
		{
#if defined(SUPPORT_D3D_DEBUG_RUNTIME)
			class D3DDebugMessage : public ID3DDebugMessage
			{
			public:
				D3DDebugMessage(const char* pMsg) : m_msg(pMsg) {}

				virtual void        Release() override      { delete this; }
				virtual const char* GetMsg() const override { return m_msg.c_str(); }

			protected:
				string m_msg;
			};

			if (pInOut0)
				*((ID3DDebugMessage**) pInOut0) = new D3DDebugMessage(D3DDebug_GetLastMessage());
#endif
			break;
		}

	default:
		assert(0);
	}
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop )            //AMD Port
#endif

void CRenderer::ForceGC() { gRenDev->m_pRT->RC_ForceMeshGC(false); }

//================================================================================================================

_smart_ptr<IRenderMesh> CRenderer::CreateRenderMesh(const char* szType, const char* szSourceName, IRenderMesh::SInitParamerers* pInitParams, ERenderMeshType eBufType)
{
	if (pInitParams)
	{
		return CreateRenderMeshInitialized(pInitParams->pVertBuffer, pInitParams->nVertexCount, pInitParams->eVertexFormat, pInitParams->pIndices, pInitParams->nIndexCount, pInitParams->nPrimetiveType, szType, szSourceName,
		                                   pInitParams->eType, pInitParams->nRenderChunkCount, pInitParams->nClientTextureBindID, 0, 0, pInitParams->bOnlyVideoBuffer, pInitParams->bPrecache, pInitParams->pTangents, pInitParams->bLockForThreadAccess, pInitParams->pNormals);
	}

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, szType);
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, szSourceName);

	// make material table with clean elements
	_smart_ptr<CRenderMesh> pRenderMesh = new CRenderMesh(szType, szSourceName);
	pRenderMesh->_SetRenderMeshType(eBufType);

	return pRenderMesh.get();
}

// Creates the RenderMesh with the materials, secondary buffer (system buffer)
// indices and perhaps some other stuff initialized.
// NOTE: if the pVertBuffer is NULL, the system buffer doesn't get initialized with any values
// (trash may be in it)
_smart_ptr<IRenderMesh> CRenderer::CreateRenderMeshInitialized(
  const void* pVertBuffer, int nVertCount, EVertexFormat eVF,
  const vtx_idx* pIndices, int nIndices,
  const PublicRenderPrimitiveType nPrimetiveType, const char* szType, const char* szSourceName, ERenderMeshType eBufType,
  int nMatInfoCount, int nClientTextureBindID,
  bool (* PrepareBufferCallback)(IRenderMesh*, bool),
  void* CustomData, bool bOnlyVideoBuffer, bool bPrecache,
  const SPipTangents* pTangents, bool bLockForThreadAcc, Vec3* pNormals)
{
	FUNCTION_PROFILER_RENDERER;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, szType);
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, szSourceName);

	_smart_ptr<CRenderMesh> pRenderMesh = new CRenderMesh(szType, szSourceName, bLockForThreadAcc);
	pRenderMesh->_SetRenderMeshType(eBufType);
	pRenderMesh->LockForThreadAccess();

	// make mats info list
	pRenderMesh->m_Chunks.reserve(nMatInfoCount);

	pRenderMesh->_SetVertexFormat(eVF);
	pRenderMesh->_SetNumVerts(nVertCount);
	pRenderMesh->_SetNumInds(nIndices);

	// copy vert buffer
	if (pVertBuffer && !PrepareBufferCallback && !bOnlyVideoBuffer)
	{
		pRenderMesh->UpdateVertices(pVertBuffer, nVertCount, 0, VSF_GENERAL, 0u, false);
		if (pTangents)
			pRenderMesh->UpdateVertices(pTangents, nVertCount, 0, VSF_TANGENTS, 0u, false);
#if ENABLE_NORMALSTREAM_SUPPORT
		if (pNormals)
			pRenderMesh->UpdateVertices(pNormals, nVertCount, 0, VSF_NORMALS, 0u, false);
#endif
	}

	if (CustomData)
		CryFatalError("CRenderMesh::CustomData not supported anymore. Will be removed from interface");

	if (pIndices)
		pRenderMesh->UpdateIndices(pIndices, nIndices, 0, 0u, false);
	pRenderMesh->_SetPrimitiveType(GetInternalPrimitiveType(nPrimetiveType));

	pRenderMesh->m_nClientTextureBindID = nClientTextureBindID;

	// Precache for static buffers
	if (CV_r_meshprecache && pRenderMesh->_GetNumVerts() && bPrecache && !m_bDeviceLost && m_pRT->IsRenderThread())
	{
		pRenderMesh->CheckUpdate(eVF, -1);
	}

	pRenderMesh->UnLockForThreadAccess();
	return pRenderMesh.get();
}

//=======================================================================

void CRenderer::SetWhiteTexture()
{
	m_pRT->RC_SetTexture(CTexture::s_ptexWhite->GetID(), 0);
}

int CRenderer::GetWhiteTextureId() const
{
	const int textureId = (CTexture::s_ptexWhite) ? CTexture::s_ptexWhite->GetID() : -1;
	return textureId;
}

void CRenderer::SetTexture(int tnum)
{
	m_pRT->RC_SetTexture(tnum, 0);
}

// used for sprite generation
void CRenderer::SetTextureAlphaChannelFromRGB(byte* pMemBuffer, int nTexSize)
{
	// set alpha channel
	for (int y = 0; y < nTexSize; y++)
		for (int x = 0; x < nTexSize; x++)
		{
			int t = (x + nTexSize * y) * 4;
			if (abs(pMemBuffer[t + 0] - pMemBuffer[0 + 0]) < 2 &&
			    abs(pMemBuffer[t + 1] - pMemBuffer[0 + 1]) < 2 &&
			    abs(pMemBuffer[t + 2] - pMemBuffer[0 + 2]) < 2)
				pMemBuffer[t + 3] = 0;
			else
				pMemBuffer[t + 3] = 255;

			// set border alpha to 0
			if (x == 0 || y == 0 || x == nTexSize - 1 || y == nTexSize - 1)
				pMemBuffer[t + 3] = 0;
		}
}

//=============================================================================
// Precaching
bool CRenderer::EF_PrecacheResource(IRenderMesh* _pPB, IMaterial* pMaterial, float fMipFactor, float fTimeToReady, int nFlags, int nUpdateId)
{
	int i;
	if (!CRenderer::CV_r_texturesstreaming)
		return true;

	CRenderMesh* pPB = (CRenderMesh*)_pPB;

	for (i = 0; i < pPB->m_Chunks.size(); i++)
	{
		CRenderChunk* pChunk = &pPB->m_Chunks[i];
		assert(!"do pre-cache with real materials");

		assert(0);

		//@TODO: Timur
		assert(pMaterial && "RenderMesh must have material");
		CShaderResources* pSR = (CShaderResources*)pMaterial->GetShaderItem(pChunk->m_nMatID).m_pShaderResources;
		if (!pSR)
			continue;
		if (pSR->m_nFrameLoad != m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameID)
		{
			pSR->m_nFrameLoad = m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameID;
			pSR->m_fMinMipFactorLoad = 999999.0f;
		}
		else if (fMipFactor >= pSR->m_fMinMipFactorLoad)
			continue;
		pSR->m_fMinMipFactorLoad = fMipFactor;
		for (int j = 0; j <= pSR->m_nLastTexture; j++)
		{
			if (!pSR->m_Textures[j])
				continue;
			CTexture* tp = pSR->m_Textures[j]->m_Sampler.m_pTex;
			if (!tp)
				continue;
			fMipFactor *= pSR->m_Textures[j]->GetTiling(0) * pSR->m_Textures[j]->GetTiling(1);

			m_pRT->RC_PrecacheResource(tp, fMipFactor, 0, nFlags, nUpdateId);
		}
	}
	return true;
}

bool CRenderer::EF_PrecacheResource(CDLight* pLS, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	if (!CRenderer::CV_r_texturesstreaming)
		return true;

	ITexture* pLightTexture = pLS->m_pLightImage ? pLS->m_pLightImage : pLS->m_pLightDynTexSource ? pLS->m_pLightDynTexSource->GetTexture() : NULL;
	if (pLightTexture)
		m_pRT->RC_PrecacheResource(pLightTexture, fMipFactor, 0, Flags, nUpdateId);
	if (pLS->GetDiffuseCubemap())
		m_pRT->RC_PrecacheResource(pLS->GetDiffuseCubemap(), fMipFactor, 0, Flags, nUpdateId);
	if (pLS->GetSpecularCubemap())
		m_pRT->RC_PrecacheResource(pLS->GetSpecularCubemap(), fMipFactor, 0, Flags, nUpdateId);
	return true;
}

void CRenderer::PrecacheTexture(ITexture* pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter)
{
	if (!CRenderer::CV_r_texturesstreaming)
		return;

	assert(m_pRT->IsRenderThread());

	if (pTP)
		((CTexture*)pTP)->CTexture::PrecacheAsynchronously(fMipFactor, Flags, nUpdateId, nCounter);
}

bool CRenderer::EF_PrecacheResource(IShader* pSH, float fMipFactor, float fTimeToReady, int Flags)
{
	if (!CRenderer::CV_r_texturesstreaming)
		return true;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// HDR_UPPERNORM -> factor used when converting from [0,32768] high dynamic range images
//                  to [0,1] low dynamic range images; 32768 = 2^(2^4-1), 4 exponent bits
// LDR_UPPERNORM -> factor used when converting from [0,1] low dynamic range images
//                  to 8bit outputs

#define HDR_UPPERNORM 1.0f  // factor set to 1.0, to be able to see content in our rather dark HDR images
#define LDR_UPPERNORM 255.0f

static float GammaToLinear(float x)
{
	return (x <= 0.04045f) ? x / 12.92f : powf((x + 0.055f) / 1.055f, 2.4f);
}

static float LinearToGamma(float x)
{
	return (x <= 0.0031308f) ? x * 12.92f : 1.055f * powf(x, 1.0f / 2.4f) - 0.055f;
}

//////////////////////////////////////////////////////////////////////////

#define PROCESS_IN_PARALLEL

// preserve the ability to use the old squish code in parallel
#define squish          squishccr
#define SQUISH_USE_CPP
#define SQUISH_USE_SSE  2
#define SQUISH_USE_XSSE 0
#define SQUISH_USE_CCR

// Squish uses non-standard inline friend templates which Recode cannot parse
#if !defined(__RECODE__) && !defined(EXCLUDE_SQUISH_SDK)
	#ifdef __GNUC__
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Werror"
	#endif // __GNUC__

	#include "../../../SDKs/squish-ccr/squish.h"
	#include "../../../SDKs/squish-ccr/squish.inl"

	#ifdef __GNUC__
		#pragma GCC diagnostic pop
	#endif // __GNUC__
#endif

// number of bytes per block per type
#define BLOCKSIZE_BC1 8
#define BLOCKSIZE_BC2 16
#define BLOCKSIZE_BC3 16
#define BLOCKSIZE_BC4 8
#define BLOCKSIZE_BC5 16
#define BLOCKSIZE_BC6 16
#define BLOCKSIZE_BC7 16

#if !defined(__RECODE__) && !defined(EXCLUDE_SQUISH_SDK)
struct SCompressRowData
{
	struct squish::sqio* pSqio;
	byte*  destinationData;
	byte*  sourceData;
	int    row;
	int    width;
	int    height;
	int    blockWidth;
	int    blockHeight;
	int    pixelStride;
	int    rowStride;
	int    blockStride;
	int    sourceChannels;
	int    destinationChannels;
	int    offs;
};

static void DXTDecompressRow(SCompressRowData data)
{
	#if CRY_PLATFORM_WINDOWS
	SCOPED_DISABLE_FLOAT_EXCEPTIONS();
	#endif

	uint8* dst = ((uint8*)data.destinationData) + (data.row * data.rowStride);
	uint8* src = ((uint8*)data.sourceData) + ((data.row >> 2) * data.blockStride);

	for (int x = 0; x < data.width; x += data.blockWidth)
	{
		uint8 values[4][4][4];

		// decode
		data.pSqio->decoder((uint8*)values, (void*)src, data.pSqio->flags);

		// transfer
		for (int by = 0; by < data.blockHeight; by += 1)
		{
			uint8* bdst = ((uint8*)dst) + (by * data.rowStride);

			for (int bx = 0; bx < data.blockWidth; bx += 1)
			{
				bdst[bx * data.pixelStride + 0] = data.sourceChannels <= 0 ? 0U : (values[by][bx][0] + data.offs);
				bdst[bx * data.pixelStride + 1] = data.sourceChannels <= 1 ? bdst[bx * data.pixelStride + 0] : (values[by][bx][1] + data.offs);
				bdst[bx * data.pixelStride + 2] = data.sourceChannels <= 1 ? bdst[bx * data.pixelStride + 0] : (values[by][bx][2] + data.offs);
				bdst[bx * data.pixelStride + 3] = data.sourceChannels <= 3 ? 255U : (values[by][bx][3]);
			}
		}

		dst += data.blockWidth * data.pixelStride;
		src += data.pSqio->blocksize;
	}
}

static void DXTDecompressRowFloat(SCompressRowData data)
{
	#if CRY_PLATFORM_WINDOWS
	SCOPED_DISABLE_FLOAT_EXCEPTIONS();
	#endif

	uint8* dst = ((uint8*)data.destinationData) + (data.row * data.rowStride);
	uint8* src = ((uint8*)data.sourceData) + ((data.row >> 2) * data.blockStride);

	for (int x = 0; x < data.width; x += data.blockWidth)
	{
		uint8 values[4][4][4];

		// decode
		data.pSqio->decoder((uint8*)values, (void*)src, data.pSqio->flags);

		// transfer
		for (int by = 0; by < data.blockHeight; by += 1)
		{
			uint8* bdst = ((uint8*)dst) + (by * data.rowStride);

			for (int bx = 0; bx < data.blockWidth; bx += 1)
			{
				bdst[bx * data.pixelStride + 0] = data.sourceChannels <= 0 ? 0U : std::min((uint8)255, (uint8)floorf(values[by][bx][0] * LDR_UPPERNORM / HDR_UPPERNORM + 0.5f));
				bdst[bx * data.pixelStride + 1] = data.sourceChannels <= 1 ? bdst[bx * data.pixelStride + 0] : std::min((uint8)255, (uint8)floorf(values[by][bx][1] * LDR_UPPERNORM / HDR_UPPERNORM + 0.5f));
				bdst[bx * data.pixelStride + 2] = data.sourceChannels <= 1 ? bdst[bx * data.pixelStride + 0] : std::min((uint8)255, (uint8)floorf(values[by][bx][2] * LDR_UPPERNORM / HDR_UPPERNORM + 0.5f));
				bdst[bx * data.pixelStride + 3] = data.sourceChannels <= 3 ? 255U : 255U;
			}
		}

		dst += data.blockWidth * data.pixelStride;
		src += data.pSqio->blocksize;
	}
}

static void DXTCompressRow(SCompressRowData data)
{
	#if CRY_PLATFORM_WINDOWS
	SCOPED_DISABLE_FLOAT_EXCEPTIONS();
	#endif

	uint8* dst = ((uint8*)data.destinationData) + ((data.row >> 2) * data.blockStride);
	uint8* src = ((uint8*)data.sourceData) + (data.row * data.rowStride);

	for (int x = 0; x < data.width; x += data.blockWidth)
	{
		uint8 values[4][4][4];

		// transfer
		for (int by = 0; by < data.blockHeight; by += 1)
		{
			uint8* bsrc = ((uint8*)src) + (by * data.rowStride);

			for (int bx = 0; bx < data.blockWidth; bx += 1)
			{
				values[by][bx][0] = data.destinationChannels <= 0 ? 0U : bsrc[bx * data.pixelStride + 0] - data.offs;
				values[by][bx][1] = data.destinationChannels <= 1 ? values[by][bx][0] : bsrc[bx * data.pixelStride + 1] - data.offs;
				values[by][bx][2] = data.destinationChannels <= 1 ? values[by][bx][0] : bsrc[bx * data.pixelStride + 2] - data.offs;
				values[by][bx][3] = data.destinationChannels <= 3 ? 255U : bsrc[bx * data.pixelStride + 3];
			}
		}

		// encode
		data.pSqio->encoder((float*)values, 0xFFFF, (void*)dst, data.pSqio->flags);

		src += data.blockWidth * data.pixelStride;
		dst += data.pSqio->blocksize;
	}
}

static void DXTCompressRowFloat(SCompressRowData data)
{
	#if CRY_PLATFORM_WINDOWS
	SCOPED_DISABLE_FLOAT_EXCEPTIONS();
	#endif

	uint8* dst = ((uint8*)data.destinationData) + ((data.row >> 2) * data.blockStride);
	uint8* src = ((uint8*)data.sourceData) + (data.row * data.rowStride);

	for (int x = 0; x < data.width; x += data.blockWidth)
	{
		float values[4][4][4];

		// transfer
		for (int by = 0; by < data.blockHeight; by += 1)
		{
			uint8* bsrc = ((uint8*)src) + (by * data.rowStride);

			for (int bx = 0; bx < data.blockWidth; bx += 1)
			{
				values[by][bx][0] = data.destinationChannels <= 0 ? 0U : bsrc[bx * data.pixelStride + 0] * HDR_UPPERNORM / LDR_UPPERNORM;
				values[by][bx][1] = data.destinationChannels <= 1 ? values[by][bx][0] : bsrc[bx * data.pixelStride + 1] * HDR_UPPERNORM / LDR_UPPERNORM;
				values[by][bx][2] = data.destinationChannels <= 1 ? values[by][bx][0] : bsrc[bx * data.pixelStride + 2] * HDR_UPPERNORM / LDR_UPPERNORM;
				values[by][bx][3] = data.destinationChannels <= 3 ? 255U : 1.0f;
			}
		}

		// encode
		data.pSqio->encoder((float*)values, 0xFFFF, (void*)dst, data.pSqio->flags);

		src += data.blockWidth * data.pixelStride;
		dst += data.pSqio->blocksize;
	}
}

DECLARE_JOB("DXTDecompressRow", TDXTDecompressRow, DXTDecompressRow);
DECLARE_JOB("DXTDecompressRowFloat", TDXTDecompressRowFloat, DXTDecompressRowFloat);
DECLARE_JOB("DXTCompressRow", TDXTCompressRow, DXTCompressRow);
DECLARE_JOB("DXTCompressRowFloat", TDXTCompressRowFloat, DXTCompressRowFloat);

#endif // #if !defined(__RECODE__) && !defined(EXCLUDE_SQUISH_SDK)

bool CRenderer::DXTDecompress(byte* sourceData, const size_t srcFileSize, byte* destinationData, int width, int height, int mips, ETEX_Format sourceFormat, bool bUseHW, int nDstBytesPerPix)
{
	if (bUseHW)
		return false;

	// Squish uses non-standard inline friend templates which Recode cannot parse
#if !defined(__RECODE__) && !defined(EXCLUDE_SQUISH_SDK)
	int flags = 0;
	int offs = 0;
	int sourceChannels = 4;
	switch (sourceFormat)
	{
	case eTF_BC1:
		sourceChannels = 4;
		flags = squish::kBtc1;
		break;
	case eTF_BC2:
		sourceChannels = 4;
		flags = squish::kBtc2;
		break;
	case eTF_BC3:
		sourceChannels = 4;
		flags = squish::kBtc3;
		break;
	case eTF_BC4U:
		sourceChannels = 1;
		flags = squish::kBtc4;
		break;
	case eTF_BC5U:
		sourceChannels = 2;
		flags = squish::kBtc5 + squish::kColourMetricUnit;
		break;
	case eTF_BC6UH:
		sourceChannels = 3;
		flags = squish::kBtc6;
		break;
	case eTF_BC7:
		sourceChannels = 4;
		flags = squish::kBtc7;
		break;

	case eTF_BC4S:
		sourceChannels = 1;
		flags = squish::kBtc4 + squish::kSignedInternal + squish::kSignedExternal;
		offs = 0x80;
		break;
	case eTF_BC5S:
		sourceChannels = 2;
		flags = squish::kBtc5 + squish::kSignedInternal + squish::kSignedExternal + squish::kColourMetricUnit;
		offs = 0x80;
		break;
	case eTF_BC6SH:
		sourceChannels = 3;
		flags = squish::kBtc6 + squish::kSignedInternal + squish::kSignedExternal;
		offs = 0x80;
		break;

	// unsupported input
	default:
		return false;
	}

	squish::sqio::dtp datatype = !CImageExtensionHelper::IsRangeless(sourceFormat) ? squish::sqio::DT_U8 : squish::sqio::DT_F23;

	if (nDstBytesPerPix == 4)
		datatype = squish::sqio::DT_U8;
	//	else if (nDstBytesPerPix == 8)
	//		datatype = squish::sqio::DT_U16;
	//	else if (nDstBytesPerPix == 16)
	//		datatype = squish::sqio::DT_F23;
	else
		// unsupported output
		return false;

	struct squish::sqio sqio = squish::GetSquishIO(width, height, datatype, flags);

	const int blockChannels = 4;
	const int blockWidth = 4;
	const int blockHeight = 4;

	SCompressRowData data;
	data.pSqio = &sqio;
	data.destinationData = destinationData;
	data.sourceData = sourceData;
	data.row = 0;
	data.width = width;
	data.height = height;
	data.blockWidth = blockWidth;
	data.blockHeight = blockHeight;
	data.pixelStride = blockChannels * sizeof(uint8);
	data.rowStride = data.pixelStride * width;
	data.blockStride = sqio.blocksize * (width >> 2);
	data.sourceChannels = sourceChannels;
	data.destinationChannels = 0;
	data.offs = offs;

	if ((datatype == squish::sqio::DT_U8) && (nDstBytesPerPix == 4))
	{
	#ifdef PROCESS_TEXTURES_IN_PARALLEL
		JobManager::SJobState jobState;
	#endif

		for (int y = 0; y < height; y += blockHeight)
		{
			data.row = y;
	#ifdef PROCESS_TEXTURES_IN_PARALLEL
			TDXTDecompressRow compressJob(data);
			compressJob.RegisterJobState(&jobState);
			compressJob.SetPriorityLevel(JobManager::eStreamPriority);
			compressJob.Run();
	#else
			DXTDecompressRow(data);
	#endif
		}

	#ifdef PROCESS_TEXTURES_IN_PARALLEL
		gEnv->pJobManager->WaitForJob(jobState);
	#endif
	}
	else if ((datatype == squish::sqio::DT_F23) && (nDstBytesPerPix == 4))
	{
	#ifdef PROCESS_TEXTURES_IN_PARALLEL
		JobManager::SJobState jobState;
	#endif

		for (int y = 0; y < height; y += blockHeight)
		{
			data.row = y;
	#ifdef PROCESS_TEXTURES_IN_PARALLEL
			TDXTDecompressRowFloat compressJob(data);
			compressJob.RegisterJobState(&jobState);
			compressJob.SetPriorityLevel(JobManager::eStreamPriority);
			compressJob.Run();
	#else
			DXTDecompressRowFloat(data);
	#endif
		}

	#ifdef PROCESS_TEXTURES_IN_PARALLEL
		gEnv->pJobManager->WaitForJob(jobState);
	#endif
	}
	else
	{
		// implement decode to 16bit and floats
		assert(0);
		delete[] destinationData;
		return false;
	}

	return true;
#else
	return false;
#endif
}

bool CRenderer::DXTCompress(byte* sourceData, int width, int height, ETEX_Format destinationFormat, bool bUseHW, bool bGenMips, int nSrcBytesPerPix, MIPDXTcallback callback)
{
	if (bUseHW)
		return false;
	if (bGenMips)
		return false;
	if (CV_r_TextureCompressor == 0)
		return false;

#if CRY_PLATFORM_WINDOWS
	if (IsBadReadPtr(sourceData, width * height * nSrcBytesPerPix))
	{
		assert(0);
		iLog->Log("Warning: CRenderer::DXTCompress: invalid data passed to the function");
		return false;
	}
#endif

#if !defined(__RECODE__) && !defined(EXCLUDE_SQUISH_SDK)
	int flags = 0;
	int offs = 0;
	int destinationChannels = 4;
	switch (destinationFormat)
	{
	// fastest encoding parameters possible
	case eTF_BC1:
		destinationChannels = 4;
		flags = squish::kBtc1 + squish::kColourMetricPerceptual + squish::kColourRangeFit + squish::kExcludeAlphaFromPalette;
		break;
	case eTF_BC2:
		destinationChannels = 4;
		flags = squish::kBtc2 + squish::kColourMetricPerceptual + squish::kColourRangeFit /*squish::kWeightColourByAlpha*/;
		break;
	case eTF_BC3:
		destinationChannels = 4;
		flags = squish::kBtc3 + squish::kColourMetricPerceptual + squish::kColourRangeFit /*squish::kWeightColourByAlpha*/;
		break;
	case eTF_BC4U:
		destinationChannels = 1;
		flags = squish::kBtc4 + squish::kColourMetricUniform;
		break;
	case eTF_BC5U:
		destinationChannels = 2;
		flags = squish::kBtc5 + squish::kColourMetricUnit;
		break;
	case eTF_BC6UH:
		destinationChannels = 3;
		flags = squish::kBtc6 + squish::kColourMetricPerceptual + squish::kColourRangeFit;
		break;
	case eTF_BC7:
		destinationChannels = 4;
		flags = squish::kBtc7 + squish::kColourMetricPerceptual + squish::kColourRangeFit;
		break;

	case eTF_BC4S:
		destinationChannels = 1;
		flags = squish::kBtc4 + squish::kSignedInternal + squish::kSignedExternal + squish::kColourMetricUniform;
		offs = 0x80;
		break;
	case eTF_BC5S:
		destinationChannels = 2;
		flags = squish::kBtc5 + squish::kSignedInternal + squish::kSignedExternal + squish::kColourMetricUnit;
		offs = 0x80;
		break;
	case eTF_BC6SH:
		destinationChannels = 3;
		flags = squish::kBtc6 + squish::kSignedInternal + squish::kSignedExternal;
		offs = 0x80;
		break;

	// unsupported output
	default:
		return false;
	}

	squish::sqio::dtp datatype = !CImageExtensionHelper::IsRangeless(destinationFormat) ? squish::sqio::DT_U8 : squish::sqio::DT_F23;

	if (nSrcBytesPerPix == 4)
		datatype = squish::sqio::DT_U8;
	//	else if (nSrcBytesPerPix == 8)
	//		datatype = squish::sqio::DT_U16;
	//	else if (nSrcBytesPerPix == 16)
	//		datatype = squish::sqio::DT_F23;
	else
		// unsupported input
		return false;

	struct squish::sqio sqio = squish::GetSquishIO(width, height, datatype, flags);

	uint8* destinationData = new uint8[sqio.compressedsize];
	if (!destinationData)
		return false;

	const int blockChannels = 4;
	const int blockWidth = 4;
	const int blockHeight = 4;

	SCompressRowData data;
	data.pSqio = &sqio;
	data.destinationData = destinationData;
	data.sourceData = sourceData;
	data.width = width;
	data.height = height;
	data.blockWidth = blockWidth;
	data.blockHeight = blockHeight;
	data.pixelStride = blockChannels * sizeof(uint8);
	data.rowStride = data.pixelStride * width;
	data.blockStride = sqio.blocksize * (width >> 2);
	data.sourceChannels = 0;
	data.destinationChannels = destinationChannels;
	data.offs = offs;

	if ((datatype == squish::sqio::DT_U8) && (nSrcBytesPerPix == 4))
	{
	#ifdef PROCESS_TEXTURES_IN_PARALLEL
		JobManager::SJobState jobState;
	#endif

		for (int y = 0; y < height; y += blockHeight)
		{
			data.row = y;
	#ifdef PROCESS_TEXTURES_IN_PARALLEL
			TDXTCompressRow compressJob(data);
			compressJob.RegisterJobState(&jobState);
			compressJob.SetPriorityLevel(JobManager::eHighPriority);
			compressJob.Run();
	#else
			DXTCompressRow(data);
	#endif
		}

	#ifdef PROCESS_TEXTURES_IN_PARALLEL
		gEnv->pJobManager->WaitForJob(jobState);
	#endif
	}
	else if ((datatype == squish::sqio::DT_F23) && (nSrcBytesPerPix == 4))
	{
	#ifdef PROCESS_TEXTURES_IN_PARALLEL
		JobManager::SJobState jobState;
	#endif

		for (int y = 0; y < height; y += blockHeight)
		{
			data.row = y;
	#ifdef PROCESS_TEXTURES_IN_PARALLEL
			TDXTCompressRowFloat compressJob(data);
			compressJob.RegisterJobState(&jobState);
			compressJob.SetPriorityLevel(JobManager::eHighPriority);
			compressJob.Run();
	#else
			DXTCompressRowFloat(data);
	#endif
		}

	#ifdef PROCESS_TEXTURES_IN_PARALLEL
		gEnv->pJobManager->WaitForJob(jobState);
	#endif
	}
	else
	{
		// implement encode from 16bit and floats
		assert(0);
		delete[] destinationData;
		return false;
	}

	(*callback)(destinationData, sqio.compressedsize, NULL);
	delete[] destinationData;

	return true;
#else
	return false;
#endif
}

bool CRenderer::WriteJPG(byte* dat, int wdt, int hgt, char* name, int src_bits_per_pixel, int nQuality)
{
	return ::WriteJPG(dat, wdt, hgt, name, src_bits_per_pixel, nQuality);
}

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)

//////////////////////////////////////////////////////////////////////////
void CRenderer::SF_ConfigMask(ESFMaskOp maskOp, unsigned int stencilRef)
{
	int stencilFunc(FSS_STENCFUNC_ALWAYS);
	int stencilPass(FSS_STENCOP_KEEP);

	switch (maskOp)
	{
	case BeginSubmitMask_Clear:
		{
			stencilFunc = FSS_STENCFUNC_ALWAYS;
			stencilPass = FSS_STENCOP_REPLACE;
			break;
		}
	case BeginSubmitMask_Inc:
		{
			stencilFunc = FSS_STENCFUNC_EQUAL;
			stencilPass = FSS_STENCOP_INCR;
			break;
		}
	case BeginSubmitMask_Dec:
		{
			stencilFunc = FSS_STENCFUNC_EQUAL;
			stencilPass = FSS_STENCOP_DECR;
			break;
		}
	case EndSubmitMask:
		{
			stencilFunc = FSS_STENCFUNC_EQUAL;
			stencilPass = FSS_STENCOP_KEEP;
			break;
		}
	case DisableMask:
		{
			stencilFunc = FSS_STENCFUNC_ALWAYS;
			stencilPass = FSS_STENCOP_KEEP;
			break;
		}
	}

	FX_SetStencilState(STENC_FUNC(stencilFunc) | STENCOP_FAIL(FSS_STENCOP_KEEP) |
	                   STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(stencilPass), stencilRef, 0xFFFFFFFF, 0xFFFFFFFF);
}

//////////////////////////////////////////////////////////////////////////
int CRenderer::SF_CreateTexture(int width, int height, int numMips, unsigned char* pData, ETEX_Format eTF, int flags)
{
	char name[128];
	cry_sprintf(name, "$SF_%d", m_TexGenID++);

	flags |= !numMips ? FT_FORCE_MIPS : 0;

	CTexture* pTexture(CTexture::Create2DTexture(name, width, height, numMips, flags, pData, eTF, eTF));
	return (pTexture != 0) ? pTexture->GetID() : -1;
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::SF_GetMeshMaxSize(int& numVertices, int& numIndices) const
{
	numVertices = m_RP.m_MaxVerts;
	numIndices = m_RP.m_MaxTris * 3;
}

#endif // #if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)

//////////////////////////////////////////////////////////////////////////
ITexture* CRenderer::CreateTexture(const char* name, int width, int height, int numMips, unsigned char* pData, ETEX_Format eTF, int flags)
{
	char uniqueName[128];
	cry_sprintf(uniqueName, "%s%d", name, m_TexGenID++);
	return CTexture::Create2DTexture(uniqueName, width, height, numMips, flags, pData, eTF, eTF);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::GetThreadIDs(threadID& mainThreadID, threadID& renderThreadID) const
{
	if (m_pRT)
	{
		mainThreadID = m_pRT->m_nMainThread;
		renderThreadID = m_pRT->m_nRenderThread;
	}
	else
	{
		mainThreadID = renderThreadID = gEnv->mMainThreadId;
	}
}
//////////////////////////////////////////////////////////////////////////
void CRenderer::PostLevelLoading()
{
	int nThreadID = m_pRT->GetThreadList();
	m_RP.m_fogVolumeContibutions[nThreadID].reserve(2048);
}

const char* CRenderer::GetTextureFormatName(ETEX_Format eTF)
{
	return CTexture::NameForTextureFormat(eTF);
}

int CRenderer::GetTextureFormatDataSize(int nWidth, int nHeight, int nDepth, int nMips, ETEX_Format eTF)
{
	return CTexture::TextureDataSize(nWidth, nHeight, nDepth, nMips, 1, eTF);
}

//////////////////////////////////////////////////////////////////////////
ERenderType CRenderer::GetRenderType() const
{
#if CRY_PLATFORM_ORBIS
	return eRT_PS4;
#elif CRY_PLATFORM_DURANGO
	return eRT_XboxOne;
#elif defined(OPENGL)
	return eRT_OpenGL;
#elif defined(CRY_USE_DX12)
	return eRT_DX12;
#else
	return eRT_DX11;
#endif
}

int CRenderer::GetNumGeomInstances()
{
#if !defined(RELEASE)
	return m_RP.m_PS[m_RP.m_nProcessThreadID].m_nInsts;
#else
	return 0;
#endif
}

int CRenderer::GetNumGeomInstanceDrawCalls()
{
#if !defined(RELEASE)
	return m_RP.m_PS[m_RP.m_nProcessThreadID].m_nInstCalls;
#else
	return 0;
#endif
}

int CRenderer::GetCurrentNumberOfDrawCalls()
{
	int nDIPs = 0;
#if defined(ENABLE_PROFILING_CODE)
	int nThr = m_pRT->GetThreadList();
	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		nDIPs += m_RP.m_PS[nThr].m_nDIPs[i];
	}
#endif
	return nDIPs;
}

void CRenderer::GetCurrentNumberOfDrawCalls(int& nGeneral, int& nShadowGen)
{
	int nDIPs = 0;
#if defined(ENABLE_PROFILING_CODE)
	int nThr = m_pRT->GetThreadList();
	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		if (i == EFSLIST_SHADOW_GEN)
			continue;
		nDIPs += m_RP.m_PS[nThr].m_nDIPs[i];
	}
	nGeneral = nDIPs;
	nShadowGen = m_RP.m_PS[nThr].m_nDIPs[EFSLIST_SHADOW_GEN];
#endif
	return;
}

int CRenderer::GetCurrentNumberOfDrawCalls(const uint32 EFSListMask)
{
	int nDIPs = 0;
#if defined(ENABLE_PROFILING_CODE)
	int nThr = m_pRT->GetThreadList();
	for (uint32 i = 0; i < EFSLIST_NUM; i++)
	{
		if ((1 << i) & EFSListMask)
		{
			nDIPs += m_RP.m_PS[nThr].m_nDIPs[i];
		}
	}
#endif
	return nDIPs;
}

void CRenderer::SetDebugRenderNode(IRenderNode* pRenderNode)
{
	m_pDebugRenderNode = pRenderNode;
}

bool CRenderer::IsDebugRenderNode(IRenderNode* pRenderNode) const
{
	return (m_pDebugRenderNode && m_pDebugRenderNode == pRenderNode);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

void IRenderer::SDrawCallCountInfo::Update(CRenderObject* pObj, IRenderMesh* pRM)
{
	SRenderPipeline& RESTRICT_REFERENCE rRP = gRenDev->m_RP;
	if (((IRenderNode*)pObj->m_pRenderNode))
	{
		pPos = pObj->GetTranslation();

		if (meshName[0] == '\0')
		{
			const char* pMeshName = pRM->GetSourceName();
			if (pMeshName)
			{
				const size_t nameLen = strlen(pMeshName);

				// truncate if necessary
				if (nameLen >= sizeof(meshName))
				{
					pMeshName += nameLen - (sizeof(meshName) - 1);
				}
				cry_strcpy(meshName, pMeshName);
			}

			const char* pTypeName = pRM->GetTypeName();
			if (pTypeName)
			{
				cry_strcpy(typeName, pTypeName);
			}
		}

		if (rRP.m_nBatchFilter & (FB_MOTIONBLUR | FB_CUSTOM_RENDER | FB_POST_3D_RENDER | FB_LAYER_EFFECT | FB_SOFTALPHATEST | FB_DEBUG))
			nMisc++;
		else if (!(rRP.m_TI[rRP.m_nProcessThreadID].m_PersFlags & RBPF_SHADOWGEN))
		{
			if (rRP.m_nBatchFilter & FB_GENERAL)
			{
				if (rRP.m_nPassGroupID == EFSLIST_TRANSP)
				{
					nTransparent++;
				}
				else
				{
					nGeneral++;
				}
			}
			else if (rRP.m_nBatchFilter & (FB_Z | FB_ZPREPASS))
			{
				nZpass++;
			}
		}
		else
			nShadows++;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

void S3DEngineCommon::Update(threadID nThreadID)
{
	I3DEngine* p3DEngine = gEnv->p3DEngine;

	// Camera vis area
	IVisArea* pCamVisArea = p3DEngine->GetVisAreaFromPos(gRenDev->GetRCamera().vOrigin);
	m_pCamVisAreaInfo.nFlags &= ~VAF_MASK;
	if (pCamVisArea)
	{
		m_pCamVisAreaInfo.nFlags |= VAF_EXISTS_FOR_POSITION;
		if (pCamVisArea->IsConnectedToOutdoor())
			m_pCamVisAreaInfo.nFlags |= VAF_CONNECTED_TO_OUTDOOR;
		if (pCamVisArea->IsAffectedByOutLights())
			m_pCamVisAreaInfo.nFlags |= VAF_AFFECTED_BY_OUT_LIGHTS;
	}

	// Update ocean info
	m_OceanInfo.m_fWaterLevel = p3DEngine->GetWaterLevel(&gRenDev->GetRCamera().vOrigin);
	m_OceanInfo.m_nOceanRenderFlags = p3DEngine->GetOceanRenderFlags();
	m_OceanInfo.m_vCausticsParams = p3DEngine->GetCausticsParams();

	if (CRenderer::CV_r_rain)
	{
		const int nFrmID = gRenDev->GetFrameID();
		if (m_RainInfo.nUpdateFrameID != nFrmID)
		{
			UpdateRainInfo(nThreadID);
			UpdateSnowInfo(nThreadID);
			m_RainInfo.nUpdateFrameID = nFrmID;
		}
	}

	// Release rain occluders
	if (CRenderer::CV_r_rain < 2 || m_RainInfo.bDisableOcclusion)
	{
		m_RainOccluders.Release();
		stl::free_container(m_RainOccluders.m_arrCurrOccluders[nThreadID]);
		m_RainInfo.bApplyOcclusion = false;
	}
}

void S3DEngineCommon::UpdateRainInfo(threadID nThreadID)
{
	gEnv->p3DEngine->GetRainParams(m_RainInfo);

	bool bProcessedAll = true;
	const uint32 numGPUs = gRenDev->GetActiveGPUCount();
	for (uint32 i = 0; i < numGPUs; ++i)
		bProcessedAll &= m_RainOccluders.m_bProcessed[i];
	const bool bUpdateOcc = bProcessedAll;
	if (bUpdateOcc)
		m_RainOccluders.Release();

	const Vec3 vCamPos = gRenDev->GetRCamera().vOrigin;
	const float fUnderWaterAtten = clamp_tpl(vCamPos.z - m_OceanInfo.m_fWaterLevel + 1.f, 0.f, 1.f);
	m_RainInfo.fCurrentAmount *= fUnderWaterAtten;

	//#define RAIN_DEBUG
#ifndef RAIN_DEBUG
	if (m_RainInfo.fCurrentAmount < 0.05f)
		return;
#endif

#ifdef RAIN_DEBUG
	m_RainInfo.fAmount = 1.f;
	m_RainInfo.fCurrentAmount = 1.f;
	m_RainInfo.fRadius = 2000.f;
	m_RainInfo.fFakeGlossiness = 0.5f;
	m_RainInfo.fFakeReflectionAmount = 1.5f;
	m_RainInfo.fDiffuseDarkening = 0.5f;
	m_RainInfo.fRainDropsAmount = 0.5f;
	m_RainInfo.fRainDropsSpeed = 1.f;
	m_RainInfo.fRainDropsLighting = 1.f;
	m_RainInfo.fMistAmount = 3.f;
	m_RainInfo.fMistHeight = 8.f;
	m_RainInfo.fPuddlesAmount = 1.5f;
	m_RainInfo.fPuddlesMaskAmount = 1.0f;
	m_RainInfo.fPuddlesRippleAmount = 2.0f;
	m_RainInfo.fSplashesAmount = 1.3f;

	m_RainInfo.vColor.Set(1, 1, 1);
	m_RainInfo.vWorldPos.Set(0, 0, 0);
#endif

	UpdateRainOccInfo(nThreadID);
}

void S3DEngineCommon::UpdateSnowInfo(int nThreadID)
{
	gEnv->p3DEngine->GetSnowSurfaceParams(m_SnowInfo.m_vWorldPos, m_SnowInfo.m_fRadius, m_SnowInfo.m_fSnowAmount, m_SnowInfo.m_fFrostAmount, m_SnowInfo.m_fSurfaceFreezing);
	gEnv->p3DEngine->GetSnowFallParams(m_SnowInfo.m_nSnowFlakeCount, m_SnowInfo.m_fSnowFlakeSize, m_SnowInfo.m_fSnowFallBrightness, m_SnowInfo.m_fSnowFallGravityScale, m_SnowInfo.m_fSnowFallWindScale, m_SnowInfo.m_fSnowFallTurbulence, m_SnowInfo.m_fSnowFallTurbulenceFreq);

	//#define RAIN_DEBUG
#ifndef RAIN_DEBUG
	if (m_SnowInfo.m_fSnowAmount < 0.05f && m_SnowInfo.m_fFrostAmount < 0.05f)
		return;
#endif

	UpdateRainOccInfo(nThreadID);
}

void S3DEngineCommon::UpdateRainOccInfo(int nThreadID)
{
	bool bSnowEnabled = (m_SnowInfo.m_fSnowAmount > 0.05f || m_SnowInfo.m_fFrostAmount > 0.05f) && m_SnowInfo.m_fRadius > 0.05f;

	bool bProcessedAll = true;
	const uint32 numGPUs = gRenDev->GetActiveGPUCount();
	for (uint32 i = 0; i < numGPUs; ++i)
		bProcessedAll &= m_RainOccluders.m_bProcessed[i];
	const bool bUpdateOcc = bProcessedAll;                 // there is no field called bForecedUpdate in RainOccluders - m_RainOccluders.bForceUpdate || bProcessedAll;
	if (bUpdateOcc)
		m_RainOccluders.Release();

	const Vec3 vCamPos = gRenDev->GetRCamera().vOrigin;
	bool bDisableOcclusion = m_RainInfo.bDisableOcclusion;
	static bool bOldDisableOcclusion = true;               // set to true to allow update at first run

	if (CRenderer::CV_r_rain == 2 && !bDisableOcclusion)
	{
		N3DEngineCommon::ArrOccluders& arrOccluders = m_RainOccluders.m_arrOccluders;
		static const unsigned int nMAX_OCCLUDERS = bSnowEnabled ? 768 : 512;
		static const float rainBBHalfSize = 18.f;

		if (bUpdateOcc)
		{

			// Choose world position and radius.
			// Snow takes priority since occlusion has a much stronger impact on it.
			Vec3 vWorldPos = bSnowEnabled ? m_SnowInfo.m_vWorldPos : m_RainInfo.vWorldPos;
			float fRadius = bSnowEnabled ? m_SnowInfo.m_fRadius : m_RainInfo.fRadius;
			float fViewerArea = bSnowEnabled ? 128.f : 32.f;   // Snow requires further view distance, otherwise obvious "unoccluded" snow regions become visible.
			float fOccArea = fViewerArea;

			// Rain volume BB
			AABB bbRainVol(fRadius);
			bbRainVol.Move(vWorldPos);

			// Visible area BB (Expanded to BB diagonal length to allow rotation)
			AABB bbViewer(fViewerArea);
			bbViewer.Move(vCamPos);

			// Area around viewer/rain source BB
			AABB bbArea(bbViewer);
			bbArea.ClipToBox(bbRainVol);

			// Snap BB to grid
			Vec3 vSnapped = bbArea.min / rainBBHalfSize;
			bbArea.min.Set(floor_tpl(vSnapped.x), floor_tpl(vSnapped.y), floor_tpl(vSnapped.z));
			bbArea.min *= rainBBHalfSize;
			vSnapped = bbArea.max / rainBBHalfSize;
			bbArea.max.Set(ceil_tpl(vSnapped.x), ceil_tpl(vSnapped.y), ceil_tpl(vSnapped.z));
			bbArea.max *= rainBBHalfSize;

			// If occlusion map info dirty
			static float fOccTreshold = CRenderer::CV_r_rainOccluderSizeTreshold;
			static float fOldRadius = fRadius;
			const AABB& oldAreaBounds = m_RainInfo.areaAABB;
			if (!oldAreaBounds.min.IsEquivalent(bbArea.min)
			    || !oldAreaBounds.max.IsEquivalent(bbArea.max)
			    || fOldRadius != fRadius      //m_RainInfo.fRadius
			    || bOldDisableOcclusion != bDisableOcclusion
			    || fOccTreshold != CRenderer::CV_r_rainOccluderSizeTreshold)
			{
				// Get occluders inside area
				unsigned int nOccluders(0);

				EERType eFilterType = eERType_Brush;
				nOccluders = gEnv->p3DEngine->GetObjectsByTypeInBox(eFilterType, bbArea);
				std::vector<IRenderNode*> occluders(nOccluders, NULL);
				if (nOccluders)
					gEnv->p3DEngine->GetObjectsByTypeInBox(eFilterType, bbArea, &occluders.front());

				// Set to new values, will be needed for other rain passes
				m_RainInfo.areaAABB = bbArea;
				fOccTreshold = CRenderer::CV_r_rainOccluderSizeTreshold;
				fOldRadius = fRadius;

				AABB geomBB(AABB::RESET);
				const size_t occluderLimit = min(nOccluders, nMAX_OCCLUDERS);
				arrOccluders.resize(occluderLimit);
				// Filter occluders and get bounding box
				for (std::vector<IRenderNode*>::const_iterator it = occluders.begin();
				     it != occluders.end() && m_RainOccluders.m_nNumOccluders < occluderLimit;
				     ++it)
				{
					IRenderNode* pRndNode = *it;
					if (pRndNode)
					{
						const AABB& aabb = pRndNode->GetBBox();
						const Vec3 vDiag = aabb.max - aabb.min;
						const float fSqrFlatRadius = Vec2(vDiag.x, vDiag.y).GetLength2();
						const unsigned nRndNodeFlags = pRndNode->GetRndFlags();
						// TODO: rainoccluder should be the only flag tested
						// (ie. enabled ONLY for small subset of geometry assets - means going through all assets affected by rain)
						if ((fSqrFlatRadius < CRenderer::CV_r_rainOccluderSizeTreshold)
						    || !(nRndNodeFlags & ERF_RAIN_OCCLUDER)
						    || (nRndNodeFlags & (ERF_COLLISION_PROXY | ERF_RAYCAST_PROXY | ERF_HIDDEN | ERF_PICKABLE)))
							continue;

						N3DEngineCommon::SRainOccluder rainOccluder;
						IStatObj* pObj = pRndNode->GetEntityStatObj(0, 0, &rainOccluder.m_WorldMat);
						if (pObj)
						{
							const size_t nPrevIdx = m_RainOccluders.m_nNumOccluders;
							if (pObj->GetFlags() & STATIC_OBJECT_COMPOUND)
							{
								const Matrix34A matParentTM = rainOccluder.m_WorldMat;
								int nSubCount = pObj->GetSubObjectCount();
								for (int nSubId = 0; nSubId < nSubCount && m_RainOccluders.m_nNumOccluders < occluderLimit; nSubId++)
								{
									IStatObj::SSubObject* pSubObj = pObj->GetSubObject(nSubId);
									if (pSubObj->bIdentityMatrix)
										rainOccluder.m_WorldMat = matParentTM;
									else
										rainOccluder.m_WorldMat = matParentTM * pSubObj->localTM;

									IStatObj* pSubStatObj = pSubObj->pStatObj;
									if (pSubStatObj && pSubStatObj->GetRenderMesh())
									{
										rainOccluder.m_RndMesh = pSubStatObj->GetRenderMesh();
										arrOccluders[m_RainOccluders.m_nNumOccluders++] = rainOccluder;
									}
								}
							}
							else if (pObj->GetRenderMesh())
							{
								rainOccluder.m_RndMesh = pObj->GetRenderMesh();
								arrOccluders[m_RainOccluders.m_nNumOccluders++] = rainOccluder;
							}

							if (m_RainOccluders.m_nNumOccluders > nPrevIdx)
								geomBB.Add(pRndNode->GetBBox());
						}
					}
				}
				const bool bProcess = m_RainOccluders.m_nNumOccluders == 0;
				for (uint32 i = 0; i < numGPUs; ++i)
					m_RainOccluders.m_bProcessed[i] = bProcess;
				m_RainInfo.bApplyOcclusion = m_RainOccluders.m_nNumOccluders > 0;

				geomBB.ClipToBox(bbArea);
				// Clip to ocean level
				geomBB.min.z = max(geomBB.min.z, gEnv->p3DEngine->GetWaterLevel()) - 0.5f;

				float fWaterOffset = m_OceanInfo.m_fWaterLevel - geomBB.min.z;
				fWaterOffset = (float)__fsel(fWaterOffset, fWaterOffset, 0.0f);

				geomBB.min.z += fWaterOffset - 0.5f;
				geomBB.max.z += fWaterOffset;

				Vec3 vSnappedCenter = bbArea.GetCenter() / rainBBHalfSize;
				vSnappedCenter.Set(floor_tpl(vSnappedCenter.x), floor_tpl(vSnappedCenter.y), floor_tpl(vSnappedCenter.z));
				vSnappedCenter *= rainBBHalfSize;

				AABB occBB(fOccArea);
				occBB.Move(vSnappedCenter);
				occBB.min.z = max(occBB.min.z, geomBB.min.z);
				occBB.max.z = min(occBB.max.z, geomBB.max.z);

				// Generate rotation matrix part-way from identity
				//	- Typical shadow filtering issues at grazing angles
				Quat qOcc = m_RainInfo.qRainRotation;
				qOcc.SetSlerp(qOcc, Quat::CreateIdentity(), 0.75f);
				Matrix44 matRot(Matrix33(qOcc.GetInverted()));

				// Get occlusion transformation matrix
				Matrix44& matOccTrans = m_RainInfo.matOccTrans;
				Matrix44 matScale;
				matOccTrans.SetIdentity();
				matOccTrans.SetTranslation(-occBB.min);
				matScale.SetIdentity();
				const Vec3 vScale(occBB.max - occBB.min);
				matScale.m00 = 1.f / vScale.x;
				matScale.m11 = 1.f / vScale.y;
				matScale.m22 = 1.f / vScale.z;
				matOccTrans = matRot * matScale * matOccTrans;
			}
		}

#if CRY_PLATFORM_WINDOWS && !defined(_RELEASE)
		if (m_RainOccluders.m_nNumOccluders >= nMAX_OCCLUDERS)
		{
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING,
			           "Reached max rain occluder limit (Max: %i), some objects may have been discarded!", nMAX_OCCLUDERS);
		}
#endif

		m_RainOccluders.m_arrCurrOccluders[nThreadID].resize(m_RainOccluders.m_nNumOccluders);
		std::copy(arrOccluders.begin(), arrOccluders.begin() + m_RainOccluders.m_nNumOccluders, m_RainOccluders.m_arrCurrOccluders[nThreadID].begin());
	}

	bOldDisableOcclusion = bDisableOcclusion;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
namespace WaterVolumeStaticData {
void GetMemoryUsage(ICrySizer* pSizer);
}

void CRenderer::GetMemoryUsage(ICrySizer* pSizer)
{
	// should be called by all derived classes
	for (uint32 i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		pSizer->AddObject(m_TextMessages[i]);

	pSizer->AddObject(m_pRT);
#if 0  //XXXXXXXXXXXXXXXXXXXXXXX
	pSizer->AddObject(m_DevBufMan);
#endif
	WaterVolumeStaticData::GetMemoryUsage(pSizer);
}

// retrieves the bandwidth calculations for the audio streaming
void CRenderer::GetBandwidthStats(float* fBandwidthRequested)
{
#if !defined (_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	if (fBandwidthRequested)
	{
		*fBandwidthRequested = (CTexture::s_nBytesSubmittedToStreaming + CTexture::s_nBytesRequiredNotSubmitted) / 1024.0f;
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::SetTextureStreamListener(ITextureStreamListener* pListener)
{
#ifdef ENABLE_TEXTURE_STREAM_LISTENER
	CTexture::s_pStreamListener = pListener;
#endif
}

#if defined(CRY_ENABLE_RC_HELPER)
void CRenderer::AddAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener)
{
	CTextureCompiler::GetInstance().AddAsyncTextureCompileListener(pListener);
}

void CRenderer::RemoveAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener)
{
	CTextureCompiler::GetInstance().RemoveAsyncTextureCompileListener(pListener);
}
#endif

//////////////////////////////////////////////////////////////////////////
float CRenderer::GetGPUFrameTime()
{
#if 0
	//If we are CPU bound and the GPU is starved for data, this causes idle time on the GPU
	//between events, so the gpu timer events cannot be relied upon

	if (GpuTimerEvent::s_eventCount[GpuTimerEvent::s_readIdx] > 0)
	{
		LARGE_INTEGER ticksPerSecond;
		QueryPerformanceFrequency(&ticksPerSecond);

		//Grab GPU frame time - assume first event
		return GpuTimerEvent::s_events[GpuTimerEvent::s_readIdx][0].totalTime / (double)ticksPerSecond.QuadPart;
	}
	return 0.f;
#else
	int nThr = m_pRT->GetThreadList();
	float fGPUidle = m_fTimeGPUIdlePercent[nThr] * 0.01f;        // normalise %
	float fGPUload = 1.0f - fGPUidle;                            // normalised non-idle time
	float fGPUtime = (m_fTimeProcessedGPU[nThr] * fGPUload);     //GPU time in seconds
	return fGPUtime;
#endif
}

void CRenderer::GetRenderTimes(SRenderTimes& outTimes)
{
	int nThr = m_pRT->GetThreadList();
	//Query render times on main thread
	outTimes.fWaitForMain = m_fTimeWaitForMain[nThr];
	outTimes.fWaitForRender = m_fTimeWaitForRender[nThr];
	outTimes.fWaitForGPU = m_fTimeWaitForGPU[nThr];
	outTimes.fTimeProcessedRT = m_fTimeProcessedRT[nThr];
	outTimes.fTimeProcessedRTScene = m_RP.m_PS[nThr].m_fRenderTime;
	outTimes.fTimeProcessedGPU = m_fTimeProcessedGPU[nThr];
	outTimes.fTimeGPUIdlePercent = m_fTimeGPUIdlePercent[nThr];
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::PreShutDown()
{
	if (m_pTextureManager)
		m_pTextureManager->ReleaseDefaultTextures();
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::PostShutDown()
{
	SAFE_DELETE(m_pTextureManager);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::UpdateRenderingModesInfo()
{
	CPostEffectsMgr* pPostEffectMgr = PostEffectMgr();
	if (!pPostEffectMgr || !pPostEffectMgr->IsCreated())
		return;

	CThermalVision* pThermalVision = (CThermalVision*)pPostEffectMgr->GetEffect(ePFX_ThermalVision);
	CPostEffect* pSonarVision = pPostEffectMgr->GetEffect(ePFX_SonarVision);
	CPostEffect* pNightVision = pPostEffectMgr->GetEffect(ePFX_NightVision);

	//if( m_nThermalVisionMode = (pThermalVision->IsActive() && CV_r_ThermalVision || CV_r_ThermalVision == 2) )
	//{
	//	m_nSonarVisionMode = m_nNightVisionMode = 0;
	//	return;
	//}

	if (m_nSonarVisionMode = (pSonarVision->IsActive() && CV_r_SonarVision || CV_r_SonarVision == 2))
	{
		m_nNightVisionMode = 0;
		m_nThermalVisionMode = 0;
		return;
	}

	m_nNightVisionMode = (pNightVision->IsActive() && (CV_r_NightVision == 2) || (CV_r_NightVision == 3)) && gRenDev->IsHDRModeEnabled();             // check only for HDR version

	if (!m_nNightVisionMode && pThermalVision->GetTransitionEffectState())
		m_nThermalVisionMode = 0;
	else
		m_nThermalVisionMode = m_nNightVisionMode;

	if (m_nThermalVisionMode)
	{
		// Night-vision is applied incorrectly when thermal vision is masked, so disable
		float bIsOffscreen = 0.f;
		EF_GetPostEffectParam("ThermalVision_RenderOffscreen", bIsOffscreen);
		m_nNightVisionMode = bIsOffscreen < 1.f ? m_nNightVisionMode : 0;
	}

	// Allow vision modes to cache state too if necessary
	pThermalVision->UpdateRenderingInfo();
}

//////////////////////////////////////////////////////////////////////////

bool CRenderer::IsCustomRenderModeEnabled(uint32 nRenderModeMask)
{
	assert(nRenderModeMask);

	if (!CV_r_PostProcess)
		return false;

	if ((nRenderModeMask & eRMF_MASK) == eRMF_MASK)
		return m_nThermalVisionMode != 0 || m_nSonarVisionMode != 0 || m_nNightVisionMode != 0;
	if (nRenderModeMask & eRMF_THERMALVISION)
		return m_nThermalVisionMode != 0;
	if (nRenderModeMask & eRMF_SONARVISION)
		return m_nSonarVisionMode != 0;
	if (nRenderModeMask & eRMF_NIGHTVISION)
		return m_nNightVisionMode != 0;

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CRenderer::IsPost3DRendererEnabled() const
{
	CPostEffectsMgr* pPostEffectMgr = PostEffectMgr();
	if (!pPostEffectMgr || !pPostEffectMgr->IsCreated())
		return false;

	CPostEffect* pPost3DRenderer = pPostEffectMgr->GetEffect(ePFX_Post3DRenderer);
	if (pPost3DRenderer)
	{
		return pPost3DRenderer->IsActive();
	}

	return false;
}

void CRenderer::ExecuteAsyncDIP()
{
#if CRY_PLATFORM_DURANGO && DURANGO_ENABLE_ASYNC_DIPS == 1
	m_DevMan.ExecuteAsyncDIP();
#else
	CryFatalError("The external ExecuteAsyncDIP functionality is only supported on Durango");
#endif // CRY_PLATFORM_DURANGO && DURANGO_ENABLE_ASYNC_DIPS == 1
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_SetPostEffectParam(const char* pParam, float fValue, bool bForceValue)
{
	if (pParam && m_RP.m_pREPostProcess)
		m_RP.m_pREPostProcess->mfSetParameter(pParam, fValue, bForceValue);
}

void CRenderer::EF_SetPostEffectParamVec4(const char* pParam, const Vec4& pValue, bool bForceValue)
{
	if (pParam && m_RP.m_pREPostProcess)
		m_RP.m_pREPostProcess->mfSetParameterVec4(pParam, pValue, bForceValue);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_SetPostEffectParamString(const char* pParam, const char* pszArg)
{
	if (pParam && pszArg && m_RP.m_pREPostProcess)
		m_RP.m_pREPostProcess->mfSetParameterString(pParam, pszArg);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_GetPostEffectParam(const char* pParam, float& fValue)
{
	if (pParam && m_RP.m_pREPostProcess)
		m_RP.m_pREPostProcess->mfGetParameter(pParam, fValue);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_GetPostEffectParamVec4(const char* pParam, Vec4& pValue)
{
	if (pParam && m_RP.m_pREPostProcess)
		m_RP.m_pREPostProcess->mfGetParameterVec4(pParam, pValue);

}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_GetPostEffectParamString(const char* pParam, const char*& pszArg)
{
	if (pParam && m_RP.m_pREPostProcess)
		m_RP.m_pREPostProcess->mfGetParameterString(pParam, pszArg);

}

//////////////////////////////////////////////////////////////////////////
int32 CRenderer::EF_GetPostEffectID(const char* pPostEffectName)
{
	if (pPostEffectName && m_RP.m_pREPostProcess)
		return m_RP.m_pREPostProcess->mfGetPostEffectID(pPostEffectName);
	return ePFX_Invalid;
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_ResetPostEffects(bool bOnSpecChange)
{
	m_pRT->RC_ResetPostEffects(bOnSpecChange);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_DisableTemporalEffects()
{
	m_pRT->RC_DisableTemporalEffects();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRenderer::EF_AddWaterSimHit(const Vec3& vPos, const float scale, const float strength)
{

	CPostEffectsMgr* pPostEffectsMgr = PostEffectMgr();
	if (pPostEffectsMgr)
	{
		CWaterRipples* pWaterRipplesTech = (CWaterRipples*)pPostEffectsMgr->GetEffect(ePFX_WaterRipples);
		if (pWaterRipplesTech)
			pWaterRipplesTech->AddHit(vPos, scale, strength);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRenderer::EF_DrawWaterSimHits()
{
	if (CPostEffectsMgr* pPostEffectsMgr = PostEffectMgr())
	{
		if (CWaterRipples* pWaterRipplesTech = (CWaterRipples*)pPostEffectsMgr->GetEffect(ePFX_WaterRipples))
			pWaterRipplesTech->DEBUG_DrawWaterHits();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderer::SetTexturePrecaching(bool stat)
{
	CTexture::s_bPrecachePhase = stat;
}

IOpticsElementBase* CRenderer::CreateOptics(EFlareType type) const
{
	return COpticsFactory::GetInstance()->Create(type);
}

void CRenderer::RT_UpdateLightVolumes()
{
	SRenderPipeline& rp = gRenDev->m_RP;
	rp.m_lightVolumeBuffer.UpdateContent();
}
//////////////////////////////////////////////////////////////////////////
SSkinningData* CRenderer::EF_CreateSkinningData(uint32 nNumBones, bool bNeedJobSyncVar)
{
	int nList = m_nPoolIndex % m_computeSkinningData.size();

	uint32 nNeededSize = Align(sizeof(SSkinningData), 16);
	nNeededSize += Align(bNeedJobSyncVar ? sizeof(JobManager::SJobState) : 0, 16);
	nNeededSize += Align(bNeedJobSyncVar ? sizeof(JobManager::SJobState) : 0, 16);
	nNeededSize += Align(nNumBones * sizeof(DualQuat), 16);
	nNeededSize += Align(nNumBones * sizeof(compute_skinning::SActiveMorphs), 16);

	byte* pData = m_SkinningDataPool[nList].Allocate(nNeededSize);

	SSkinningData* pSkinningRenderData = alias_cast<SSkinningData*>(pData);
	pData += Align(sizeof(SSkinningData), 16);

	pSkinningRenderData->pAsyncJobs = bNeedJobSyncVar ? alias_cast<JobManager::SJobState*>(pData) : NULL;
	pData += Align(bNeedJobSyncVar ? sizeof(JobManager::SJobState) : 0, 16);

	pSkinningRenderData->pAsyncDataJobs = bNeedJobSyncVar ? alias_cast<JobManager::SJobState*>(pData) : NULL;
	pData += Align(bNeedJobSyncVar ? sizeof(JobManager::SJobState) : 0, 16);

	if (bNeedJobSyncVar) // init job state if requiered
	{
		new(pSkinningRenderData->pAsyncJobs) JobManager::SJobState();
		new(pSkinningRenderData->pAsyncDataJobs) JobManager::SJobState();
	}

	pSkinningRenderData->pBoneQuatsS = alias_cast<DualQuat*>(pData);
	pData += Align(nNumBones * sizeof(DualQuat), 16);

	pSkinningRenderData->pActiveMorphs = alias_cast<compute_skinning::SActiveMorphs*>(pData);
	pData += Align(nNumBones * sizeof(compute_skinning::SActiveMorphs), 16);

	pSkinningRenderData->pRemapTable = NULL;
	pSkinningRenderData->pCustomData = NULL;
	pSkinningRenderData->pCustomTag = NULL;

	pSkinningRenderData->nNumActiveMorphs = 0;
	pSkinningRenderData->nNumBones = nNumBones;
	pSkinningRenderData->nHWSkinningFlags = 0;
	pSkinningRenderData->pPreviousSkinningRenderData = NULL;
	pSkinningRenderData->pCharInstCB = FX_AllocateCharInstCB(pSkinningRenderData, m_nPoolIndex);
	pSkinningRenderData->remapGUID = ~0u;

	pSkinningRenderData->pNextSkinningData = NULL;
	pSkinningRenderData->pMasterSkinningDataList = &pSkinningRenderData->pNextSkinningData;

	return pSkinningRenderData;
}

SSkinningData* CRenderer::EF_CreateRemappedSkinningData(uint32 nNumBones, SSkinningData* pSourceSkinningData, uint32 nCustomDataSize, uint32 pairGuid)
{
	assert(pSourceSkinningData);
	assert(pSourceSkinningData->nNumBones >= nNumBones);    // don't try to remap more bones than exist

	int nList = m_nPoolIndex % m_computeSkinningData.size();

	uint32 nNeededSize = Align(sizeof(SSkinningData), 16);
	nNeededSize += Align(nCustomDataSize, 16);

	byte* pData = m_SkinningDataPool[nList].Allocate(nNeededSize);

	SSkinningData* pSkinningRenderData = alias_cast<SSkinningData*>(pData);
	pData += Align(sizeof(SSkinningData), 16);

	pSkinningRenderData->pRemapTable = NULL;

	pSkinningRenderData->pCustomData = nCustomDataSize ? alias_cast<void*>(pData) : NULL;
	pData += nCustomDataSize ? Align(nCustomDataSize, 16) : 0;

	pSkinningRenderData->nNumBones = nNumBones;
	pSkinningRenderData->nNumActiveMorphs = pSourceSkinningData->nNumActiveMorphs;
	pSkinningRenderData->nHWSkinningFlags = 0;
	pSkinningRenderData->pPreviousSkinningRenderData = NULL;
	pSkinningRenderData->pCustomTag = pSourceSkinningData->pCustomTag;

	// use actual bone information from original skinning data
	pSkinningRenderData->pBoneQuatsS = pSourceSkinningData->pBoneQuatsS;
	pSkinningRenderData->pActiveMorphs = pSourceSkinningData->pActiveMorphs;
	pSkinningRenderData->pAsyncJobs = pSourceSkinningData->pAsyncJobs;
	pSkinningRenderData->pAsyncDataJobs = pSourceSkinningData->pAsyncDataJobs;
	pSkinningRenderData->pRenderMesh = pSourceSkinningData->pRenderMesh;

	pSkinningRenderData->pCharInstCB = pSourceSkinningData->pCharInstCB;

	pSkinningRenderData->remapGUID = pairGuid;
	pSkinningRenderData->pNextSkinningData = NULL;
	pSkinningRenderData->pMasterSkinningDataList = &pSourceSkinningData->pNextSkinningData;

	return pSkinningRenderData;
}

void CRenderer::EF_EnqueueComputeSkinningData(SSkinningData* pData)
{
	int nList = m_nPoolIndex % m_computeSkinningData.size();
	m_computeSkinningData[nList].push_back(pData);
}

compute_skinning::CStorage* CRenderer::GetComputeSkinningStorage() const
{
	return m_pComputeSkinningStorage;
}

void CRenderer::RT_SetSkinningPoolId(uint32 poolId)
{
	m_nPoolIndexRT = poolId;
}

int CRenderer::GetTexturesStreamPoolSize()
{
	int poolSize = CV_r_TexturesStreamPoolSize + CV_r_TexturesStreamPoolSecondarySize;
	return gEnv->IsEditor()
	       ? max(poolSize, 512)
	       : poolSize;
}

void CRenderer::ClearSkinningDataPool()
{
	m_pRT->RC_PushSkinningPoolId(++m_nPoolIndex);
	m_SkinningDataPool[m_nPoolIndex % m_computeSkinningData.size()].ClearPool();
	m_computeSkinningData[m_nPoolIndex % m_computeSkinningData.size()].resize(0);
	FX_ClearCharInstCB(m_nPoolIndex);
}

int CRenderer::EF_GetSkinningPoolID()
{
	return m_nPoolIndex;
}

void CRenderer::UpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial)
{
	bool bShaderReloaded = false;
#if !defined(_RELEASE) || CRY_PLATFORM_WINDOWS
	IShader* pShader = pShaderItem->m_pShader;
	if (pShader)
		bShaderReloaded = (pShader->GetFlags() & EF_RELOADED) != 0;
#endif

	if (pShaderItem->m_nPreprocessFlags == -1 || bShaderReloaded)
		CRenderer::ForceUpdateShaderItem(pShaderItem, pMaterial);

	if (auto pShaderResources = reinterpret_cast<CShaderResources*>(pShaderItem->m_pShaderResources))
	{
		if (auto pPsoCache = pShaderResources->m_pipelineStateCache)
		{
			pPsoCache->Clear();
		}
	}
}

void CRenderer::RefreshShaderResourceConstants(SShaderItem* pShaderItem, IMaterial* pMaterial)
{
	m_pRT->RC_RefreshShaderResourceConstants(pShaderItem, pMaterial);
}

void CRenderer::ForceUpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial)
{
	m_pRT->RC_UpdateShaderItem(pShaderItem, pMaterial);
}

void CRenderer::RT_UpdateShaderItem(SShaderItem* pShaderItem)
{
	CShader* pShader = static_cast<CShader*>(pShaderItem->m_pShader);
	if (pShader)
	{
		pShader->m_Flags &= ~EF_RELOADED;
		pShaderItem->Update();
	}
}

void CRenderer::RT_RefreshShaderResourceConstants(SShaderItem* pShaderItem)
{
	CShader* pShader = static_cast<CShader*>(pShaderItem->m_pShader);
	if (pShader)
	{
		if (pShaderItem->RefreshResourceConstants())
			pShaderItem->m_pShaderResources->UpdateConstants(pShader);
	}
}

bool CRenderer::LoadShaderStartupCache()
{
	return m_cEF.LoadShaderStartupCache();
}

void CRenderer::UnloadShaderStartupCache()
{
	m_cEF.UnloadShaderStartupCache();
}

void CRenderer::FlushPendingTextureTasks()
{
	if (m_pRT)
	{
		m_pRT->RC_FlushTextureStreaming(true);
		FlushRTCommands(true, true, true);
	}
}

void CRenderer::SetCloakParams(const SRendererCloakParams& cloakParams)
{
	CV_r_cloakLightScale = cloakParams.fCloakLightScale;
	CV_r_cloakTransitionLightScale = cloakParams.fCloakTransitionLightScale;
	CV_r_cloakFadeByDist = cloakParams.cloakFadeByDist;
	CV_r_cloakFadeLightScale = cloakParams.fCloakFadeLightScale;
	CV_r_cloakFadeStartDistSq = cloakParams.fCloakFadeMinDistSq;
	CV_r_cloakFadeEndDistSq = cloakParams.fCloakFadeMaxDistSq;
	CV_r_cloakFadeMinValue = cloakParams.fCloakFadeMinValue;
	CV_r_cloakRefractionFadeByDist = cloakParams.cloakRefractionFadeByDist;
	CV_r_cloakRefractionFadeStartDistSq = cloakParams.fCloakRefractionFadeMinDistSq;
	CV_r_cloakRefractionFadeEndDistSq = cloakParams.fCloakRefractionFadeMaxDistSq;
	CV_r_cloakRefractionFadeMinValue = cloakParams.fCloakRefractionFadeMinValue;
	CV_r_cloakMinLightValue = cloakParams.fCloakMinLightValue;
	CV_r_cloakHeatScale = cloakParams.fCloakHeatScale;
	CV_r_cloakRenderInThermalVision = cloakParams.cloakRenderInThermalVision;
	CV_r_cloakMinAmbientOutdoors = cloakParams.fCloakMinAmbientOutdoors;
	CV_r_cloakMinAmbientIndoors = cloakParams.fCloakMinAmbientIndoors;
	CV_r_cloakSparksAlpha = cloakParams.fCloakSparksAlpha;
	CV_r_cloakInterferenceSparksAlpha = cloakParams.fCloakInterferenceSparksAlpha;
	CV_r_cloakHighlightStrength = cloakParams.fCloakHighlightStrength;
	CV_r_ThermalVisionViewDistance = cloakParams.fThermalVisionViewDistance;
	CV_r_armourPulseSpeedMultiplier = cloakParams.fArmourPulseSpeedMultiplier;
	CV_r_maxSuitPulseSpeedMultiplier = cloakParams.fMaxSuitPulseSpeedMultiplier;
}

float CRenderer::GetCloakFadeLightScale() const
{
	return CV_r_cloakFadeLightScale;
}

void CRenderer::SetCloakFadeLightScale(float fColorScale)
{
	CV_r_cloakFadeLightScale = fColorScale;
}

void CRenderer::SetShadowJittering(float shadowJittering)
{
	m_shadowJittering = shadowJittering;
}

float CRenderer::GetShadowJittering() const
{
	return m_shadowJittering;
}

CRenderView* CRenderer::GetRenderViewForThread(int nThreadID, bool bRecursive)
{
	int recursion = bRecursive ? 1 : 0;
	return m_RP.m_pRenderViews[nThreadID][recursion].get();
}

Matrix44A CRenderer::GetCameraMatrix()
{
	if (m_vSegmentedWorldOffset.IsZero())
		return m_CameraMatrix;

	static const Matrix33 matRotX = Matrix33::CreateRotationX(-gf_PI / 2);
	Matrix34 matCam = GetCamera().GetMatrix();
	matCam.SetTranslation(matCam.GetTranslation() + m_vSegmentedWorldOffset);
	Matrix44A matView = Matrix44A(matRotX * matCam.GetInverted()).GetTransposed();
	return matView;
}

const Matrix44A& CRenderer::GetPreviousFrameCameraMatrix() const
{
	return gRenDev->m_CameraMatrixPrev[m_CurViewportID][m_CurRenderEye];
}

void CRenderer::SetPreviousFrameCameraMatrix(const Matrix44A& m)
{
	gRenDev->m_CameraMatrixPrev[m_CurViewportID][m_CurRenderEye] = m;
}

void CRenderer::OffsetPosition(const Vec3& delta)
{
#ifdef SEG_WORLD
	m_vSegmentedWorldOffset = delta;
#endif
}

void CRenderer::GetPolyCount(int& nPolygons, int& nShadowPolys)
{
#if defined(ENABLE_PROFILING_CODE)
	nPolygons = GetPolyCount();
	nShadowPolys = m_RP.m_PS[m_RP.m_nFillThreadID].m_nPolygons[EFSLIST_SHADOW_GEN];
	nShadowPolys += m_RP.m_PS[m_RP.m_nFillThreadID].m_nPolygons[EFSLIST_SHADOW_PASS];
	nPolygons -= nShadowPolys;
#endif
}

int CRenderer::GetPolyCount()
{
#if defined(ENABLE_PROFILING_CODE)
	ASSERT_IS_MAIN_THREAD(m_pRT);
	int nPolys = 0;
	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		nPolys += m_RP.m_PS[m_RP.m_nFillThreadID].m_nPolygons[i];
	}
	return nPolys;
#else
	return 0;
#endif
}

int CRenderer::RT_GetPolyCount()
{
#if defined(ENABLE_PROFILING_CODE)
	ASSERT_IS_RENDER_THREAD(m_pRT);
	int nPolys = 0;
	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		nPolys += m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[i];
	}
	return nPolys;
#else
	return 0;
#endif
}

void CRenderer::SyncMainWithRender()
{
	const uint numListeners = m_syncMainWithRenderListeners.size();
	for (uint i = 0; i < numListeners; ++i)
	{
		m_syncMainWithRenderListeners[i]->SyncMainWithRender();
	}
}

void CRenderer::RegisterSyncWithMainListener(ISyncMainWithRenderListener* pListener)
{
	stl::push_back_unique(m_syncMainWithRenderListeners, pListener);
}

void CRenderer::RemoveSyncWithMainListener(const ISyncMainWithRenderListener* pListener)
{
	stl::find_and_erase(m_syncMainWithRenderListeners, pListener);
}

void CRenderer::FreePermanentRenderObjects(int bufferId)
{
	m_RP.m_persistentRenderObjectsToDelete[bufferId].CoalesceMemory();
	for (int i = 0, num = m_RP.m_persistentRenderObjectsToDelete[bufferId].size(); i < num; i++)
	{
		CPermanentRenderObject::FreeToPool(m_RP.m_persistentRenderObjectsToDelete[bufferId][i]);
	}
	m_RP.m_persistentRenderObjectsToDelete[bufferId].clear();
}

void CRenderer::SetCloudShadowsParams(int nTexID, const Vec3& speed, float tiling, bool invert, float brightness)
{
	m_cloudShadowTexId = nTexID;
	m_cloudShadowSpeed = speed;
	m_cloudShadowTiling = tiling;
	m_cloudShadowInvert = invert;
	m_cloudShadowBrightness = brightness;
}

void CRenderer::UpdateCachedShadowsLodCount(int nGsmLods) const
{
	OnChange_CachedShadows(NULL);
}

bool CRenderer::IsHDRModeEnabled() const
{
	return (CV_r_HDRRendering && !CV_r_measureoverdraw && !m_wireframe_mode) ? true : false;
}

bool CRenderer::IsShadowPassEnabled() const
{
	return (CV_r_ShadowPass && CV_r_usezpass && !m_wireframe_mode) ? true : false;
}

#ifndef _RELEASE

CRenderer::RNDrawcallsMapMesh& CRenderer::GetDrawCallsInfoPerMesh(bool mainThread /*=true*/)
{
	if (mainThread)
	{
		return m_RP.m_pRNDrawCallsInfoPerMesh[m_RP.m_nFillThreadID];
	}
	else
	{
		return m_RP.m_pRNDrawCallsInfoPerMesh[m_RP.m_nProcessThreadID];
	}
}

int CRenderer::GetDrawCallsPerNode(IRenderNode* pRenderNode)
{
	uint32 t = m_RP.m_nFillThreadID;
	IRenderer::RNDrawcallsMapNodeItor iter = m_RP.m_pRNDrawCallsInfoPerNode[t].find(pRenderNode);
	if (iter != m_RP.m_pRNDrawCallsInfoPerNode[t].end())
	{
		SDrawCallCountInfo& pInfo = iter->second;
		uint32 nDrawcalls = pInfo.nShadows + pInfo.nZpass + pInfo.nGeneral + pInfo.nTransparent + pInfo.nMisc;
		return nDrawcalls;
	}

	return 0;
}

void CRenderer::ForceRemoveNodeFromDrawCallsMap(IRenderNode* pNode)
{
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
	{
		IRenderer::RNDrawcallsMapNodeItor pItor = m_RP.m_pRNDrawCallsInfoPerNode[i].find(pNode);
		if (pItor != m_RP.m_pRNDrawCallsInfoPerNode[i].end())
		{
			m_RP.m_pRNDrawCallsInfoPerNode[i].erase(pItor);
		}
	}
}

void CRenderer::ClearDrawCallsInfo()
{
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
	{
		m_RP.m_pRNDrawCallsInfoPerMesh[i].clear();
		m_RP.m_pRNDrawCallsInfoPerNode[i].clear();
	}
}
#endif

void CRenderer::CollectDrawCallsInfo(bool status)
{
	m_bCollectDrawCallsInfo = status;
}

void CRenderer::CollectDrawCallsInfoPerNode(bool status)
{
	m_bCollectDrawCallsInfoPerNode = status;
}

void CRenderer::EnableLevelUnloading(bool enable)
{
	gRenDev->m_bLevelUnloading = enable;
}

void CRenderer::EnableBatchMode(bool enable)
{
	RENDER_LOCK_CS(SRenderThread::s_rcLock);
	if (enable)
	{
		gRenDev->m_bEndLevelLoading = false;
		gRenDev->m_bStartLevelLoading = true;
	}
	else
	{
		gRenDev->m_bEndLevelLoading = true;
		gRenDev->m_bStartLevelLoading = false;
	}
}

void CRenderer::SetShouldCopyScreenToBackBuffer(bool bEnable)
{
	m_bCopyScreenToBackBuffer = bEnable;
}

bool CRenderer::StopRendererAtFrameEnd(uint timeoutMilliseconds)
{
	m_mtxStopAtRenderFrameEnd.Lock();
	m_bStopRendererAtFrameEnd = true;

	if (!m_condStopAtRenderFrameEnd.TimedWait(m_mtxStopAtRenderFrameEnd, timeoutMilliseconds))
	{
		m_mtxStopAtRenderFrameEnd.Unlock();
		return false;
	}

	m_mtxStopAtRenderFrameEnd.Unlock();
	return true;
}

void CRenderer::ResumeRendererFromFrameEnd()
{
	m_mtxStopAtRenderFrameEnd.Lock();
	if (!m_bStopRendererAtFrameEnd)
	{
		m_mtxStopAtRenderFrameEnd.Unlock();
		CryFatalError("Trying to resume render thread but render thread was not stopped before. Use StopRendererAtFrameEnd() prior using ResumeRendererFromFrameEnd().");
	}
	m_bStopRendererAtFrameEnd = false;
	m_mtxStopAtRenderFrameEnd.Unlock();
	m_condStopAtRenderFrameEnd.Notify();
}
