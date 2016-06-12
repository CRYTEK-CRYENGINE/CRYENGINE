// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_XOptimise.h>
#include <CrySystem/IWindowMessageHandler.h>
#include <CryInput/IHardwareMouse.h>

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_JobSystem.h>  // to be removed

#if !defined(_RELEASE)
	#define ENABLE_CONTEXT_THREAD_CHECKING 0
#endif

#if !defined(ENABLE_CONTEXT_THREAD_CHECKING)
	#define ENABLE_CONTEXT_THREAD_CHECKING 0
#endif

/*
   ===========================================
   The DXRenderer interface Class
   ===========================================
 */

inline int _VertBufferSize(D3DVertexBuffer* pVB)
{
	if (!pVB)
		return 0;
	D3D11_BUFFER_DESC Desc;
	pVB->GetDesc(&Desc);
	return Desc.ByteWidth;
}
inline int _IndexBufferSize(D3DIndexBuffer* pIB)
{
	if (!pIB)
		return 0;
	D3D11_BUFFER_DESC Desc;
	pIB->GetDesc(&Desc);
	return Desc.ByteWidth;
}

#define VERSION_D3D 2.0

struct SPixFormat;
struct SGraphicsPipelinePassContext;

#include "D3DRenderAuxGeom.h"
#include "D3DColorGradingController.h"
#include "D3DStereo.h"
#include "ShadowTextureGroupManager.h"    // CShadowTextureGroupManager
#include "GraphicsPipeline/StandardGraphicsPipeline.h"
#include "D3DDeferredShading.h"
#include "D3DTiledShading.h"
#include "D3DVolumetricFog.h"
#include "GPUTimer.h"
#include "PipelineProfiler.h"
#include "D3DDebug.h"
#include "DeviceInfo.h"
#include <memory>
#include "Common/RenderView.h"

inline DWORD FLOATtoDWORD(float f)
{
	union FLOATDWORD
	{
		float f;
		DWORD dw;
	};

	FLOATDWORD val;
	val.f = f;
	return val.dw;
}

//=====================================================

struct STexFiller;

#define BUFFERED_VERTS 256

struct SD3DContext
{
	HWND                                m_hWnd;
	int                                 m_X;
	int                                 m_Y;
	// Real offscreen target width for rendering
	int                                 m_Width;
	// Real offscreen target height for rendering
	int                                 m_Height;
	// Swap-chain and it's back-buffers
	IDXGISwapChain*                     m_pSwapChain;
	std::vector<_smart_ptr<D3DSurface>> m_pBackBuffers;
	// Currently active back-buffer
	D3DSurface*                         m_pBackBuffer;
	unsigned int                        m_pCurrentBackBufferIndex;
	// Width of viewport on screen to display rendered content in
	int                                 m_nViewportWidth;
	// Height of viewport on screen to display rendered content in
	int                                 m_nViewportHeight;
	// Number of samples per output (real offscreen) pixel used in X
	int                                 m_nSSSamplesX;
	// Number of samples per output (real offscreen) pixel used in Y
	int                                 m_nSSSamplesY;
	// Denotes if context refers to main viewport
	bool                                m_bMainViewport;
};

#define D3DAPPERR_NODIRECT3D          0x82000001
#define D3DAPPERR_NOWINDOW            0x82000002
#define D3DAPPERR_NOCOMPATIBLEDEVICES 0x82000003
#define D3DAPPERR_NOWINDOWABLEDEVICES 0x82000004
#define D3DAPPERR_NOHARDWAREDEVICE    0x82000005
#define D3DAPPERR_HALNOTCOMPATIBLE    0x82000006
#define D3DAPPERR_NOWINDOWEDHAL       0x82000007
#define D3DAPPERR_NODESKTOPHAL        0x82000008
#define D3DAPPERR_NOHALTHISMODE       0x82000009
#define D3DAPPERR_NONZEROREFCOUNT     0x8200000a
#define D3DAPPERR_MEDIANOTFOUND       0x8200000b
#define D3DAPPERR_RESIZEFAILED        0x8200000c

const float g_fEyeAdaptionLowerPercent = 0.05f; // 5% percentil
const float g_fEyeAdaptionMidPercent = 0.5f;    // 50% percentil
const float g_fEyeAdaptionUpperPercent = 0.95f; // 95% percentil

// Texture coordinate rectangle
struct CoordRect
{
	float fLeftU, fTopV;
	float fRightU, fBottomV;
};

HRESULT GetTextureCoords(CTexture* pTexSrc, RECT* pRectSrc, CTexture* pTexDest, RECT* pRectDest, CoordRect* pCoords);
bool    DrawFullScreenQuad(float fLeftU, float fTopV, float fRightU, float fBottomV, bool bClampToScreenRes = true);
bool    DrawFullScreenQuad(CoordRect c, bool bClampToScreenRes = true);

struct CRY_ALIGN(16) SStateBlend
{
	uint64 nHashVal;
	D3D11_BLEND_DESC Desc;
	ID3D11BlendState* pState;

	SStateBlend()
	{
		memset(this, 0, sizeof(*this));
	}

	static uint64 GetHash(const D3D11_BLEND_DESC& InDesc)
	{
		uint32 hashLow = InDesc.AlphaToCoverageEnable |
		                 (InDesc.RenderTarget[0].BlendEnable << 1) | (InDesc.RenderTarget[1].BlendEnable << 2) | (InDesc.RenderTarget[2].BlendEnable << 3) | (InDesc.RenderTarget[3].BlendEnable << 4) |
		                 (InDesc.RenderTarget[0].SrcBlend << 5) | (InDesc.RenderTarget[0].DestBlend << 10) |
		                 (InDesc.RenderTarget[0].SrcBlendAlpha << 15) | (InDesc.RenderTarget[0].DestBlendAlpha << 20) |
		                 (InDesc.RenderTarget[0].BlendOp << 25) | (InDesc.RenderTarget[0].BlendOpAlpha << 28);
		uint32 hashHigh = InDesc.RenderTarget[0].RenderTargetWriteMask |
		                  (InDesc.RenderTarget[1].RenderTargetWriteMask << 4) |
		                  (InDesc.RenderTarget[2].RenderTargetWriteMask << 8) |
		                  (InDesc.RenderTarget[3].RenderTargetWriteMask << 12) |
		                  (InDesc.IndependentBlendEnable << 16);

		return (((uint64)hashHigh) << 32) | ((uint64)hashLow);
	}
};

struct CRY_ALIGN(16) SStateRaster
{
	uint64 nValuesHash;
	uint32 nHashVal;
	ID3D11RasterizerState* pState;
	D3D11_RASTERIZER_DESC Desc;

	SStateRaster()
	{
		memset(this, 0, sizeof(*this));
		Desc.DepthClipEnable = true;
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.FrontCounterClockwise = TRUE;
	}

	static uint32 GetHash(const D3D11_RASTERIZER_DESC& InDesc)
	{
		uint32 nHash;
		nHash = InDesc.FillMode | (InDesc.CullMode << 2) |
		        (InDesc.DepthClipEnable << 4) | (InDesc.FrontCounterClockwise << 5) |
		        (InDesc.ScissorEnable << 6) | (InDesc.MultisampleEnable << 7) | (InDesc.AntialiasedLineEnable << 8) |
		        (InDesc.DepthBias << 9);
		return nHash;
	}

	static uint64 GetValuesHash(const D3D11_RASTERIZER_DESC& InDesc)
	{
		uint64 nHash;
		//avoid breaking strict alising rules
		union f32_u
		{
			float        floatVal;
			unsigned int uintVal;
		};
		f32_u uDepthBiasClamp;
		uDepthBiasClamp.floatVal = InDesc.DepthBiasClamp;
		f32_u uSlopeScaledDepthBias;
		uSlopeScaledDepthBias.floatVal = InDesc.SlopeScaledDepthBias;
		nHash = (((uint64)uDepthBiasClamp.uintVal) |
		         ((uint64)uSlopeScaledDepthBias.uintVal) << 32);
		return nHash;
	}

};
inline uint32 sStencilState(const D3D11_DEPTH_STENCILOP_DESC& Desc)
{
	uint32 nST = (Desc.StencilFailOp << 0) |
	             (Desc.StencilDepthFailOp << 4) |
	             (Desc.StencilPassOp << 8) |
	             (Desc.StencilFunc << 12);
	return nST;
}
struct CRY_ALIGN(16) SStateDepth
{
	uint64 nHashVal;
	D3D11_DEPTH_STENCIL_DESC Desc;
	ID3D11DepthStencilState* pState;
	SStateDepth() : nHashVal(), pState()
	{
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		Desc.DepthFunc = D3D11_COMPARISON_LESS;
		Desc.StencilEnable = FALSE;
		Desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		Desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

		Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		Desc.BackFace = Desc.FrontFace;
	}

	static uint64 GetHash(const D3D11_DEPTH_STENCIL_DESC& InDesc)
	{
		uint64 nHash;
		nHash = (InDesc.DepthEnable << 0) |
		        (InDesc.DepthWriteMask << 1) |
		        (InDesc.DepthFunc << 2) |
		        (InDesc.StencilEnable << 6) |
		        (InDesc.StencilReadMask << 7) |
		        (InDesc.StencilWriteMask << 15) |
		        (((uint64)sStencilState(InDesc.FrontFace)) << 23) |
		        (((uint64)sStencilState(InDesc.BackFace)) << 39);
		return nHash;
	}
};

#define MAX_OCCL_QUERIES        4096

#define MAXFRAMECAPTURECALLBACK 1

//======================================================================
// Options for clearing

#define CLEAR_ZBUFFER 0x00000001l            /* Clear target z buffer, equals D3D11_CLEAR_DEPTH */
#define CLEAR_STENCIL 0x00000002l            /* Clear stencil planes, equals D3D11_CLEAR_STENCIL */
#define CLEAR_RTARGET 0x00000004l            /* Clear target surface */

//======================================================================
/// Forward declared classes

struct IStatoscopeDataGroup;
class CVolumetricCloudManager;
namespace gpu_pfx2 {
class CManager;
}

#ifdef ENABLE_BENCHMARK_SENSOR
namespace BenchmarkFramework
{
class IBenchmarkRendererSensor;
};
#endif
//======================================================================
/// Direct3D Render driver class

#ifdef SHADER_ASYNC_COMPILATION
class CAsyncShaderTask;
#endif

class CD3D9Renderer final : public CRenderer, public IWindowMessageHandler
{
	friend struct SPixFormat;
	friend class CD3DStereoRenderer;
	friend class CTexture;
	friend class CShadowMapStage;
	friend class CSceneRenderPass;

public:
	enum EDefShadows_Passes
	{
		DS_STENCIL_PASS,
		DS_HISTENCIL_REFRESH,
		DS_SHADOW_PASS,
		DS_SHADOW_CULL_PASS,
		DS_SHADOW_FRUSTUM_CULL_PASS,
		DS_STENCIL_VOLUME_CLIP,
		DS_CLOUDS_SEPARATE,
		DS_VOLUME_SHADOW_PASS,
		DS_PASS_MAX
	};
	struct S2DImage
	{
		CTexture* pTex;
		CTexture* pTarget;
		float     xpos, ypos, w, h, s0, t0, s1, t1, angle, z, stereoDepth;
		DWORD     col;

		S2DImage(float xpos, float ypos, float w, float h, CTexture* pTex, float s0, float t0, float s1, float t1, float angle, DWORD col, float z, float stereoDepth, CTexture* pTarget = NULL) :
			pTex(pTex), xpos(xpos), ypos(ypos), w(w), h(h), s0(s0), t0(t0), s1(s1), t1(t1), angle(angle), z(z), col(col), stereoDepth(stereoDepth), pTarget(pTarget) {}
	};
	struct SCharacterInstanceCB
	{
		CConstantBuffer*                 boneTransformsBuffer;
		// contains only the bone ids and the weighs [0-100] of the bones that move a morph for a particular frame
		CGpuBuffer                       activeMorphsBuffer;
		SSkinningData*                   m_pSD;
		util::list<SCharacterInstanceCB> list;
		bool                             updated;

		SCharacterInstanceCB()
			: boneTransformsBuffer()
			, m_pSD()
			, list()
			, updated()
		{
		}

		~SCharacterInstanceCB() { SAFE_RELEASE(boneTransformsBuffer); list.erase(); }
	};
	struct SRenderTargetStack
	{
		D3DSurface*      m_pTarget;
		D3DDepthSurface* m_pDepth;

		CTexture*        m_pTex;
		SDepthTexture*   m_pSurfDepth;
		int              m_Width;
		int              m_Height;
		uint32           m_bNeedReleaseRT : 1;
		uint32           m_bWasSetRT      : 1;
		uint32           m_bWasSetD       : 1;
		uint32           m_bScreenVP      : 1;

		uint32           m_ClearFlags;
		ColorF           m_ReqColor;
		float            m_fReqDepth;
		uint8            m_nReqStencil;
	};
	struct SGammaRamp
	{
		uint16 red[256];
		uint16 green[256];
		uint16 blue[256];
	};

public:
	CD3D9Renderer();
	~CD3D9Renderer();

	static void StaticCleanup();

	//////////////////////////////////////////////////////////////////////////
	// Remove pointer indirection.
	ILINE CD3D9Renderer* operator->()              { return this; }
	ILINE const bool     operator!() const         { return false; }
	ILINE                operator bool() const     { return true; }
	ILINE                operator CD3D9Renderer*() { return this; }
	//////////////////////////////////////////////////////////////////////////

	virtual void           InitRenderer() override;
	virtual void           Release() override;

	const SRenderTileInfo& GetRenderTileInfo() const { return m_RenderTileInfo; }

	static unsigned int    GetNumBackBufferIndices(const DXGI_SWAP_CHAIN_DESC& scDesc);
	static unsigned int    GetCurrentBackBufferIndex(IDXGISwapChain* pSwapChain);
	void                   ReleaseBackBuffers();

	uint32                 GetOrCreateBlendState(const D3D11_BLEND_DESC& desc);
	bool                   SetBlendState(const SStateBlend* pNewState);
	uint32                 GetOrCreateRasterState(const D3D11_RASTERIZER_DESC& rasterizerDec, const bool bAllowMSAA = true);
	bool                   SetRasterState(const SStateRaster* pNewState, const bool bAllowMSAA = true);
	uint32                 GetOrCreateDepthState(const D3D11_DEPTH_STENCIL_DESC& desc);
	bool                   SetDepthState(const SStateDepth* pNewState, uint8 newStencRef);
	void                   SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topType);
	virtual void           LockParticleVideoMemory() override;
	virtual void           UnLockParticleVideoMemory() override;
	virtual void           ActivateLayer(const char* pLayerName, bool activate) override;
	SDepthTexture          FX_ReplaceMSAADepthBuffer(CTexture* pDepthBufferRT, bool bMSAA, D3DShaderResource*& pZTargetOrigSRV);
	void                   FX_RestoreMSAADepthBuffer(SDepthTexture sBackup, CTexture* pDepthBufferRT, bool bMSAA, D3DShaderResource*& pZTargetOrigSRV);
	void                   SetDefaultTexParams(bool bUseMips, bool bRepeat, bool bLoad);

public:
#ifdef USE_PIX_DURANGO
	ILINE ID3DUserDefinedAnnotation* GetPixProfiler() { return m_pPixPerf; }
#endif

	/////////////////////////////////////////////////////////////////////////////
	// Functions to access the device and associated objects
	const CCryDeviceWrapper&                   GetDevice() const;
	CCryDeviceWrapper&                         GetDevice();
	ILINE CCryDeviceWrapper&                   GetDevice_Unsynchronized()        { return m_DeviceWrapper; }
	CCryDeviceContextWrapper&                  GetDeviceContext();
	ILINE CCryDeviceContextWrapper&            GetDeviceContext_Unsynchronized() { return m_DeviceContextWrapper; }
#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	CCryPerformanceDeviceWrapper&              GetPerformanceDevice();
	ILINE CCryPerformanceDeviceWrapper&        GetPerformanceDevice_Unsynchronized()        { return m_PerformanceDeviceWrapper; }
	CCryPerformanceDeviceContextWrapper&       GetPerformanceDeviceContext();
	ILINE CCryPerformanceDeviceContextWrapper& GetPerformanceDeviceContext_Unsynchronized() { return m_PerformanceDeviceContextWrapper; }
#endif

	/////////////////////////////////////////////////////////////////////////////
	// Debug Functions to check that the D3D DeviceContext is only accessed
	// by its owning thread
	void  BindContextToThread(DWORD threadID);
	DWORD GetBoundThreadID() const;
	void  CheckContextThreadAccess() const;

	/////////////////////////////////////////////////////////////////////////////
	// Util function to support synchronization if a asynchrounous device is used
	void          WaitForAsynchronousDevice() const;
	volatile int* GetAsynchronousDeviceState();

	/////////////////////////////////////////////////////////////////////////////
	// Util function to bind Device Hooks to all devices/contextes
	void                             RegisterDeviceWrapperHook(ICryDeviceWrapperHook* pDeviceWrapperHook);
	void                             UnregisterDeviceWrapperHook(const char* pDeviceHookName);

	void                             FX_PrepareDepthMapsForLight(CRenderView* pRenderView, const SRenderLight& rLight, int nLightID, bool bClearPool = false);
	void                             EF_PrepareShadowGenRenderList(CRenderView* pRenderView);
	bool                             EF_PrepareShadowGenForLight(CRenderView* pRenderView, SRenderLight* pLight, int nLightID);
	bool                             PrepareShadowGenForFrustum(CRenderView* pRenderView, ShadowMapFrustum* pCurFrustum, SRenderLight* pLight, int nLightID, int nLOD = 0);

	virtual void                     EF_InvokeShadowMapRenderJobs(CRenderView* pRenderView, const int nFlags) override;
	void                             InvokeShadowMapRenderJobs(ShadowMapFrustum* pCurFrustum, SRenderingPassInfo passInfo);
	void                             StartInvokeShadowMapRenderJobs(ShadowMapFrustum* pCurFrustum, const SRenderingPassInfo& passInfo);

	void                             GetReprojectionMatrix(Matrix44A& matReproj, const Matrix44A& matView, const Matrix44A& matProj, const Matrix44A& matPrevView, const Matrix44A& matPrevProj, float fFarPlane) const;

	void                             SetDepthBoundTest(float fMin, float fMax, bool bEnable);
	bool                             GetDepthBoundTestState(float& fMin, float& fMax) const;

	void                             CreateDeferredUnitBox(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff);
	const t_arrDeferredMeshIndBuff&  GetDeferredUnitBoxIndexBuffer() const;
	const t_arrDeferredMeshVertBuff& GetDeferredUnitBoxVertexBuffer() const;
	virtual bool                     PrepareDepthMap(CRenderView* pRenderView, ShadowMapFrustum* SMSource, int nLightFrustumID = 0, bool bClearPool = false) override;
	void                             ConfigShadowTexgen(int Num, ShadowMapFrustum* pFr, int nFrustNum = -1, bool bScreenToLocalBasis = false, bool bUseComparisonSampling = true);
	virtual void                     DrawAllShadowsOnTheScreen() override;
	void                             DrawAllDynTextures(const char* szFilter, const bool bLogNames, const bool bOnlyIfUsedThisFrame);
	virtual void                     OnEntityDeleted(IRenderNode* pRenderNode) override;

	virtual void                     RT_ResetGlass() override;

	void                             D3DSetCull(ECull eCull, bool bSkipMirrorCull = false);

	void                             GetDeviceGamma();
	void                             SetDeviceGamma(SGammaRamp*);

	HRESULT                          AdjustWindowForChange();

	bool                             IsFullscreen()           { return m_bFullScreen; }
	bool                             IsSuperSamplingEnabled() { return m_numSSAASamples > 1; }
	bool                             IsNativeScalingEnabled() { return m_width != m_nativeWidth || m_height != m_nativeHeight; }

#if defined(SUPPORT_DEVICE_INFO)
	static HWND CreateWindowCallback();
	DeviceInfo& DevInfo() { return m_devInfo; }
#endif

	uint32 GetCurrentFrameCounter()   const { return m_frameFenceCounter; }
	uint32 GetCompletedFrameCounter() const { return m_completedFrameFenceCounter; }

private:
	void                    DebugShowRenderTarget();

	bool                    SetWindow(int width, int height, bool fullscreen, WIN_HWND hWnd);
	bool                    SetRes();
	void                    UnSetRes();
	void                    DisplaySplash(); //!< Load a bitmap from a file, blit it to the windowdc and free it

	void                    DestroyWindow(void);
	virtual void            RestoreGamma(void) override;
	void                    SetGamma(float fGamma, float fBrigtness, float fContrast, bool bForce);

	int                     FindTextureInRegistry(const char* filename, int* tex_type);
	int                     RegisterTextureInRegistry(const char* filename, int tex_type, int tid, int low_tid);
	unsigned int            MakeTextureREAL(const char* filename, int* tex_type, unsigned int load_low_res);
	unsigned int            CheckTexturePlus(const char* filename, const char* postfix);

	static HRESULT CALLBACK OnD3D11CreateDevice(D3DDevice* pd3dDevice);

	void                    PostDeviceReset();
	void                    IssueFrameFences();

public:
	static HRESULT CALLBACK OnD3D11PostCreateDevice(D3DDevice* pd3dDevice);

public:
	// Multithreading support
	virtual void RT_BeginFrame() override;
	virtual void RT_EndFrame() override;
	virtual void RT_ForceSwapBuffers() override;
	virtual void RT_SwitchToNativeResolutionBackbuffer(bool resolveBackBuffer) override;
	virtual void RT_Init() override;
	virtual bool RT_CreateDevice() override;
	virtual void RT_Reset() override;
	virtual void RT_RenderScene(CRenderView* pRenderView, int nFlags, SThreadInfo& TI, RenderFunc pRenderFunc) override;
	virtual void RT_SelectGPU(int nGPU) override;
	virtual void RT_SetCull(int nMode) override;
	virtual void RT_SetScissor(bool bEnable, int x, int y, int width, int height) override;
	virtual void RT_SetCameraInfo() override;
	virtual void RT_CreateResource(SResourceAsync* Res) override;
	virtual void RT_ReleaseResource(SResourceAsync* pRes) override;
	virtual void RT_ReleaseRenderResources(uint32 nFlags) override;
	virtual void RT_UnbindResources() override;
	virtual void RT_UnbindTMUs() override;
	virtual void RT_CreateRenderResources() override;
	virtual void RT_PrecacheDefaultShaders() override;
	virtual void RT_ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX, int nScaledY) override;
	virtual void RT_ClearTarget(CTexture* pTex, const ColorF& color) override;
	virtual void RT_ReleaseVBStream(void* pVB, int nStream) override;
	virtual void RT_ReleaseCB(void* pCB) override;
	virtual void RT_ReleaseRS(std::shared_ptr<CDeviceResourceSet>& pRS) override;
	virtual void RT_DrawDynVB(SVF_P3F_C4B_T2F* pBuf, uint16* pInds, uint32 nVerts, uint32 nInds, const PublicRenderPrimitiveType nPrimType) override;

	// =======================================================================================
	// = Functions which draw directly into the swap-chain's backbuffer ======================
	virtual void RT_DrawStringU(IFFont_RenderProxy* pFont, float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx) const override;
	virtual void RT_DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround) override;
	virtual void RT_Draw2dImage(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, float angle, DWORD col, float z) override;
	virtual void RT_Draw2dImageStretchMode(bool bStretch) override;
	virtual void RT_Push2dImage(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, float angle, DWORD col, float z, float stereoDepth) override;
	virtual void RT_PushUITexture(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, CTexture* pTarget, DWORD col) override;
	virtual void RT_Draw2dImageList() override;
	virtual void RT_DrawImageWithUV(float xpos, float ypos, float z, float w, float h, int texture_id, float* s, float* t, DWORD col, bool filtered = true) override;

	void         RT_DrawUITextureInternal(S2DImage& img);
	void         RT_RenderUITextures();
	void         RT_DrawImageWithUVInternal(float xpos, float ypos, float z, float w, float h, int texture_id, float s[4], float t[4], DWORD col, bool filtered = true);
	void         RT_Draw2dImageInternal(S2DImage* images, uint32 numImages, bool stereoLeftEye = true);
	// =======================================================================================

	virtual void RT_PushRenderTarget(int nTarget, CTexture* pTex, SDepthTexture* pDepth, int nS) override;
	virtual void RT_PopRenderTarget(int nTarget) override;
	virtual void RT_SetViewport(int x, int y, int width, int height, int id = -1) override;
	virtual void RT_RenderDebug(bool bRenderStats = true) override;
	virtual void RT_SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode = false) override;

	virtual void RT_PresentFast() override;
	void         RT_CopyScreenToBackBuffer();
	virtual void RT_PrepareStereo(int mode, int output) override;
	virtual void RT_CopyToStereoTex(int channel) override;

	//===============================================================================

	virtual void FlashRenderInternal(IFlashPlayer_RenderProxy* pPlayer, bool bStereo, bool bDoRealRender) override;
	virtual void FlashRenderPlaybackLocklessInternal(IFlashPlayer_RenderProxy* pPlayer, int cbIdx, bool bStereo, bool bFinalPlayback, bool bDoRealRender) override;

	//===============================================================================

	virtual WIN_HWND Init(int x, int y, int width, int height, unsigned int cbpp, int zbpp, int sbits, bool fullscreen, WIN_HINSTANCE hinst, WIN_HWND Glhwnd = 0, bool bReInit = false, const SCustomRenderInitArgs* pCustomArgs = 0, bool bShaderCacheGen = false) override;

	virtual void     GetVideoMemoryUsageStats(size_t& vidMemUsedThisFrame, size_t& vidMemUsedRecently, bool bGetPoolsSizes = false) override;

	/////////////////////////////////////////////////////////////////////////////////
	// Render-context management
	/////////////////////////////////////////////////////////////////////////////////
	virtual bool     SetCurrentContext(WIN_HWND hWnd) override;
	virtual bool     CreateContext(WIN_HWND hWnd, bool bAllowMSAA = false, int SSX = 1, int SSY = 1) override;
	virtual bool     DeleteContext(WIN_HWND hWnd) override;
	virtual void     MakeMainContextActive() override;
	virtual WIN_HWND GetCurrentContextHWND() override                 { return m_CurrContext ? m_CurrContext->m_hWnd : m_hWnd; }
	virtual bool     IsCurrentContextMainVP() override                { return m_CurrContext ? m_CurrContext->m_bMainViewport : true; }

	virtual int      GetCurrentContextViewportWidth() const override  { return (m_bDeviceLost ? -1 : m_CurrContext->m_nViewportWidth); }
	virtual int      GetCurrentContextViewportHeight() const override { return (m_bDeviceLost ? -1 : m_CurrContext->m_nViewportHeight); }
	/////////////////////////////////////////////////////////////////////////////////

	virtual int  CreateRenderTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF = eTF_R8G8B8A8) override;
	virtual bool DestroyRenderTarget(int nHandle) override;
	virtual bool SetRenderTarget(int nHandle, int nFlags = 0) override;

	virtual bool ChangeDisplay(unsigned int width, unsigned int height, unsigned int cbpp) override;
	virtual void ChangeViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height, bool bMainViewport = false) override;
	virtual int  EnumDisplayFormats(SDispFormat* formats) override;
	//! Return all supported by video card video AA formats
	virtual int  EnumAAFormats(SAAFormat* formats) override;

	//! Changes resolution of the window/device (doen't require to reload the level
	virtual bool ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen, bool bForceReset) override;
	virtual void Reset(void) override;

	virtual void SwitchToNativeResolutionBackbuffer() override;

	void         CalculateResolutions(int width, int height, bool bUseNativeResolution, int* pRenderWidth, int* pRenderHeight, int* pNativeWidth, int* pNativeHeight, int* pBackbufferWidth, int* pBackbufferHeight);

	virtual void BeginFrame() override;
	virtual void ShutDown(bool bReInit = false) override;
	virtual void ShutDownFast() override;
	virtual void RenderDebug(bool bRenderStats = true) override;
	virtual void RT_ShutDown(uint32 nFlags) override;

	void DebugPrintShader(class CHWShader_D3D* pSH, void* pInst, int nX, int nY, ColorF colSH);
	void                 DebugPerfBars(int nX, int nY);
	void                 DebugVidResourcesBars(int nX, int nY);
	void                 DebugDrawStats1();
	void                 DebugDrawStats2();
	void                 DebugDrawStats8();
	void                 DebugDrawStats20();
	void                 DebugDrawStats();
	void                 DebugDrawRect(float x1, float y1, float x2, float y2, float* fColor);
	void                 VidMemLog();

	virtual void         EndFrame() override;
	virtual void         LimitFramerate(const int maxFPS, const bool bUseSleep) override;
	virtual void         GetMemoryUsage(ICrySizer* Sizer) override;
	virtual void         GetLogVBuffers() override;

	virtual void         TryFlush() override;

	void                 Draw2dImage(float xpos, float ypos, float w, float h, int textureid, float s0, float t0, float s1, float t1, float angle, const ColorF& col, float z = 1);
	virtual void         Draw2dImage(float xpos, float ypos, float w, float h, int textureid, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 1, float angle = 0, float r = 1, float g = 1, float b = 1, float a = 1, float z = 1) override;
	virtual void         Push2dImage(float xpos, float ypos, float w, float h, int textureid, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 1, float angle = 0,
	                                 float r = 1, float g = 1, float b = 1, float a = 1, float z = 1, float stereoDepth = 0) override;
	virtual void         PushUITexture(int srcId, int dstId, float x0, float y0, float w, float h, float u0, float v0, float u1, float v1, float r, float g, float b, float a) override;
	bool                 RayIntersectMesh(IRenderMesh* pMesh, const Ray& ray, Vec3& hitpos, Vec3& p0, Vec3& p1, Vec3& p2, Vec2& uv0, Vec2& uv1, Vec2& uv2);
	virtual int          RayToUV(const Vec3& vOrigin, const Vec3& vDirection, float* pUOut, float* pVOut) override;
	virtual Vec3         UnprojectFromScreen(int x, int y) override;

	virtual void         Draw2dImageStretchMode(bool bStretch) override;
	virtual void         Draw2dImageList() override;
	virtual void         DrawImage(float xpos, float ypos, float w, float h, int texture_id, float s0, float t0, float s1, float t1, float r, float g, float b, float a, bool filtered = true) override;
	virtual void         DrawImageWithUV(float xpos, float ypos, float z, float w, float h, int texture_id, float* s, float* t, float r, float g, float b, float a, bool filtered = true) override;
	virtual void         SetCullMode(int mode = R_CULL_BACK) override;

	void                 SelectGPU(int nGPU);

	virtual void         PushProfileMarker(char* label) override;
	virtual void         PopProfileMarker(char* label) override;

	virtual bool         EnableFog(bool enable) override;
	virtual void         SetFogColor(const ColorF& color) override;

	virtual void         CreateResourceAsync(SResourceAsync* pResource) override;
	virtual void         ReleaseResourceAsync(SResourceAsync* pResource) override;
	virtual unsigned int DownLoadToVideoMemory(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override;
	unsigned int         DownLoadToVideoMemory(unsigned char* data, int w, int h, int d, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, ETEX_Type eTT, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false);
	virtual unsigned int DownLoadToVideoMemoryCube(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override;
	virtual unsigned int DownLoadToVideoMemory3D(unsigned char* data, int w, int h, int d, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override;
	virtual void         UpdateTextureInVideoMemory(uint32 tnum, unsigned char* newdata, int posx, int posy, int w, int h, ETEX_Format eTF = eTF_R8G8B8A8, int posz = 0, int sizez = 1) override;
	virtual bool         EF_PrecacheResource(SShaderItem* pSI, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter) override;
	virtual bool         EF_PrecacheResource(ITexture* pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter) override;
	virtual void         RemoveTexture(unsigned int TextureId) override;
	virtual ITexture*    EF_CreateCompositeTexture(int type, const char* szName, int nWidth, int nHeight, int nDepth, int nMips, int nFlags, ETEX_Format eTF, const STexComposition* pCompositions, size_t nCompositions, int8 nPriority = -1) override;

	virtual void         PostLevelLoading() override;
	virtual void         PostLevelUnload() override;

	void                 SetRCamera(const CRenderCamera& cam);
	virtual void         SetCamera(const CCamera& cam) override;
	virtual void         SetViewport(int x, int y, int width, int height, int id = 0) override;
	virtual void         GetViewport(int* x, int* y, int* width, int* height) override;
	virtual void         SetScissor(int x = 0, int y = 0, int width = 0, int height = 0) override;
	virtual void         SetRenderTile(f32 nTilesPosX, f32 nTilesPosY, f32 nTilesGridSizeX, f32 nTilesGridSizeY) override;

	void                 DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround);
	virtual void         Graph(byte* g, int x, int y, int wdt, int hgt, int nC, int type, char* text, ColorF& color, float fScale) override;

	virtual void         DrawDynVB(SVF_P3F_C4B_T2F* pBuf, uint16* pInds, int nVerts, int nInds, const PublicRenderPrimitiveType nPrimType) override;

	virtual void         PrintResourcesLeaks() override;

	virtual void         FX_PushWireframeMode(int mode) override;
	virtual void         FX_PopWireframeMode() override;
	void                 FX_SetWireframeMode(int nMode);

	virtual void         PushMatrix() override;
	virtual void         PopMatrix() override;

	virtual void         EnableVSync(bool enable) override;
	virtual void         DrawPrimitivesInternal(CVertexBuffer* src, int vert_num, const ERenderPrimitiveType prim_type) override;
	virtual void         ResetToDefault() override;

	virtual void         SetMaterialColor(float r, float g, float b, float a) override;

	virtual bool         ProjectToScreen(float ptx, float pty, float ptz, float* sx, float* sy, float* sz) override;
	virtual int          UnProject(float sx, float sy, float sz, float* px, float* py, float* pz, const float modelMatrix[16], const float projMatrix[16], const int viewport[4]) override;
	virtual int          UnProjectFromScreen(float sx, float sy, float sz, float* px, float* py, float* pz) override;

	virtual void         GetModelViewMatrix(float* mat) override;
	virtual void         GetProjectionMatrix(float* mat) override;
	virtual void         SetMatrices(float* pProjMat, float* pViewMat) override;

	void                 DrawQuad(float x0, float y0, float x1, float y1, const ColorF& color, float z = 1.0f, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 1);
	void                 DrawQuad3D(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3, const ColorF& color,
	                                float ftx0 = 0, float fty0 = 0, float ftx1 = 1, float fty1 = 1);
	void                 DrawFullScreenQuad(CShader* pSH, const CCryNameTSCRC& TechName, float s0, float t0, float s1, float t1, uint32 nState = GS_NODEPTHTEST);

	void                 SetPerspective(const CCamera& cam);

	// NOTE: deprecated
	virtual void ClearTargetsImmediately(uint32 nFlags) override;
	virtual void ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors, float fDepth) override;
	virtual void ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors) override;
	virtual void ClearTargetsImmediately(uint32 nFlags, float fDepth) override;

	virtual void ClearTargetsLater(uint32 nFlags) override;
	virtual void ClearTargetsLater(uint32 nFlags, const ColorF& Colors, float fDepth) override;
	virtual void ClearTargetsLater(uint32 nFlags, const ColorF& Colors) override;
	virtual void ClearTargetsLater(uint32 nFlags, float fDepth) override;

	virtual void ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX = -1, int nScaledY = -1) override;
	virtual void ReadFrameBufferFast(uint32* pDstARGBA8, int dstWidth, int dstHeight) override;

	virtual void SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode = false) override;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// This routines uses 2 destination surfaces.  It triggers a backbuffer copy to one of its surfaces,
	// and then copies the other surface to system memory.  This hopefully will remove any
	// CPU stalls due to the rect lock call since the buffer will already be in system
	// memory when it is called
	// Inputs :
	//			pDstARGBA8			:	Pointer to a buffer that will hold the captured frame (should be at least 4*dstWidth*dstHieght for RGBA surface)
	//			destinationWidth	:	Width of the frame to copy
	//			destinationHeight	:	Height of the frame to copy
	//
	//			Note :	If dstWidth or dstHeight is larger than the current surface dimensions, the dimensions
	//					of the surface are used for the copy
	//
	virtual bool CaptureFrameBufferFast(unsigned char* pDstRGBA8, int destinationWidth, int destinationHeight) override;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Copy a captured surface to a buffer
	//
	// Inputs :
	//			pDstARGBA8			:	Pointer to a buffer that will hold the captured frame (should be at least 4*dstWidth*dstHieght for RGBA surface)
	//			destinationWidth	:	Width of the frame to copy
	//			destinationHeight	:	Height of the frame to copy
	//
	//			Note :	If dstWidth or dstHeight is larger than the current surface dimensions, the dimensions
	//					of the surface are used for the copy
	//
	virtual bool CopyFrameBufferFast(unsigned char* pDstRGBA8, int destinationWidth, int destinationHeight) override;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// This routine registers a callback address that is called when a new frame is available
	// Inputs :
	//			pCapture			:	Address of the ICaptureFrameListener object
	//
	// Outputs : returns true if successful, otherwise false
	//
	virtual bool RegisterCaptureFrame(ICaptureFrameListener* pCapture) override;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// This routine unregisters a callback address that was previously registered
	// Inputs :
	//			pCapture			:	Address of the ICaptureFrameListener object to unregister
	//
	// Outputs : returns true if successful, otherwise false
	//
	virtual bool UnRegisterCaptureFrame(ICaptureFrameListener* pCapture) override;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// This routine initializes 2 destination surfaces for use by the CaptureFrameBufferFast routine
	// It also, captures the current backbuffer into one of the created surfaces
	//
	// Inputs :
	//			bufferWidth	: Width of capture buffer, on consoles the scaling is done on the GPU. Pass in 0 (the default) to use backbuffer dimensions
	//			bufferHeight	: Height of capture buffer.
	//
	// Outputs : returns true if surfaces were created otherwise returns false
	//
	virtual bool InitCaptureFrameBufferFast(uint32 bufferWidth, uint32 bufferHeight) override;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// This routine releases the 2 surfaces used for frame capture by the CaptureFrameBufferFast routine
	//
	// Inputs : None
	//
	// Returns : None
	//
	virtual void CloseCaptureFrameBufferFast(void) override;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// This routine checks for any frame buffer callbacks that are needed and calls them
	//
	// Inputs : None
	//
	//	Outputs : None
	//
	virtual void CaptureFrameBufferCallBack(void) override;

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// This routine checks for any frame buffer callbacks that are needed and checks their flags, calling preparation functions if required
	//
	// Inputs : None
	//
	//	Outputs : None
	//
	void CaptureFrameBufferPrepare(void);

	//misc
	virtual bool ScreenShot(const char* filename = NULL, int width = 0, EScreenShotMode eScreenShotMode = eScreenShotMode_Normal) override;

	void         UnloadOldTextures() {};

	virtual void Set2DMode(bool enable, int ortox, int ortoy, float znear = -1e10f, float zfar = 1e10f) override;

	virtual int  ScreenToTexture(int nTexID) override;

	virtual bool SetGammaDelta(const float fGamma) override;

	//////////////////////////////////////////////////////////////////////
	// Replacement functions for the Font engine
	virtual bool FontUploadTexture(class CFBitmap*, ETEX_Format eTF = eTF_R8G8B8A8) override;
	virtual int  FontCreateTexture(int Width, int Height, byte* pData, ETEX_Format eTF = eTF_R8G8B8A8, bool genMips = false) override;
	virtual bool FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte* pData) override;
	virtual void FontReleaseTexture(class CFBitmap* pBmp) override;
	virtual void FontSetTexture(class CFBitmap*, int nFilterMode) override;
	virtual void FontSetTexture(int nTexId, int nFilterMode) override;
	virtual void FontSetRenderingState(unsigned int nVirtualScreenWidth, unsigned int nVirtualScreenHeight) override;
	virtual void FontSetBlending(int src, int dst) override;
	virtual void FontRestoreRenderingState() override;

	void         FontSetState(bool bRestore);

	//////////////////////////////////////////////////////////////////////
	void         MakeSprites(TArray<SSpriteGenInfo>& SGI, const SRenderingPassInfo& passInfo);
	virtual void DrawObjSprites(PodArray<struct SVegetationSpriteInfo>* pList) override;
	virtual void GenerateObjSprites(PodArray<struct SVegetationSpriteInfo>* pList, const SRenderingPassInfo& passInfo) override;
	void         DrawObjSprites(PodArray<SVegetationSpriteInfo>* pList, SSpriteInfo* pSPInfo, const int nSPI, bool bZ, bool bShadows);
	void         ObjSpritesFlush(SVF_P3F_C4B_T2F* pVerts, uint16* pInds, int numSprites, void*& pCurVB, SShaderTechnique* pTech, bool bZ);

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	void                     SF_CreateResources();
	void                     SF_PrecacheShaders();
	void                     SF_DestroyResources();
	void                     SF_ResetResources();
	struct SSF_ResourcesD3D& SF_GetResources();
	bool                     SF_SetVertexDeclaration(SSF_GlobalDrawParams::EVertexFmt vertexFmt);
	CShader*                 SF_SetTechnique(const CCryNameTSCRC& techName);
	void                     SF_SetBlendOp(SSF_GlobalDrawParams::EAlphaBlendOp blendOp, bool reset = false);
	uint32                   SF_AdjustBlendStateForMeasureOverdraw(uint32 blendModeStates);

	virtual void             SF_DrawIndexedTriList(int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount, const SSF_GlobalDrawParams& params) override;
	virtual void             SF_DrawLineStrip(int baseVertexIndex, int lineCount, const SSF_GlobalDrawParams& params) override;
	virtual void             SF_DrawGlyphClear(const SSF_GlobalDrawParams& params) override;
	virtual void             SF_DrawBlurRect(const SSF_GlobalDrawParams* pParams) override;
	virtual void             SF_Flush() override;
	virtual bool             SF_UpdateTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, unsigned char* pData, size_t pitch, size_t size, ETEX_Format eTF) override;
	virtual bool             SF_MapTexture(int texID, int level, void*& pBits, uint32& pitch) override;
	virtual bool             SF_UnmapTexture(int texID, int level) override;
#else // defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	virtual void             SF_DrawIndexedTriList(int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount, const SSF_GlobalDrawParams& params) override {}
	virtual void             SF_DrawLineStrip(int baseVertexIndex, int lineCount, const SSF_GlobalDrawParams& params) override                                                               {}
	virtual void             SF_DrawGlyphClear(const SSF_GlobalDrawParams& params) override                                                                                                  {}
	virtual void             SF_DrawBlurRect(const SSF_GlobalDrawParams* pParams) override                                                                                                   {}
	virtual void             SF_Flush() override                                                                                                                                             {}
	virtual bool             SF_UpdateTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, unsigned char* pData, size_t pitch, size_t size, ETEX_Format eTF) override   { return false; }
	virtual bool             SF_MapTexture(int texID, int level, void*& pBits, uint32& pitch) override                                                                                       { return false; }
	virtual bool             SF_UnmapTexture(int texID, int level) override                                                                                                                  { return false; }
#endif // defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)

	virtual void SetProfileMarker(const char* label, ESPM mode) const override;

	//////////////////////////////////////////////////////////////////////

	void FX_ApplyThreadState(SThreadInfo& TI, SThreadInfo* OldTI);
	void EF_RenderScene(int nFlags, SViewport& VP, const SRenderingPassInfo& passInfo);
	void EF_Scene3D(SViewport& VP, int nFlags, const SRenderingPassInfo& passInfo);

	bool CheckMSAAChange();
	bool CheckSSAAChange();

	// FX Shaders pipeline
	void FX_DrawInstances(CShader* ef, SShaderPass* slw, int nRE, uint32 nCurInst, uint32 nLastInst, uint32 nUsedAttr, byte* nInstanceData, int nInstAttrMask, byte Attributes[], short dwCBufSlot);
	void FX_DrawShader_InstancedHW(CShader* ef, SShaderPass* slw);
	void FX_DrawShader_General(CShader* ef, SShaderTechnique* pTech);
	void FX_SetupForwardShadows(CRenderView* pRenderView, bool bUseShaderPermutations = false);
	void FX_SetupShadowsForTransp();
	void FX_SetupShadowsForFog();
	bool FX_DrawToRenderTarget(CShader* pShader, CShaderResources* pRes, CRenderObject* pObj, SShaderTechnique* pTech, SHRenderTarget* pTarg, int nPreprType, CRendElementBase* pRE);

	void FX_UpdateGpuParticles();

	// hdr src texture is optional, if not specified uses default hdr destination target
#if !CRY_PLATFORM_ORBIS
	void CopyFramebufferDX11(CTexture* pDst, D3DResource* pSrcResource, D3DFormat srcFormat);
#endif
	void FX_ScreenStretchRect(CTexture* pDst, CTexture* pHDRSrc = NULL);

#if CRY_PLATFORM_ORBIS
	bool BakeMesh(const SMeshBakingInputParams* pInputParams, SMeshBakingOutput* pReturnValues) override { return false; }
#else
	bool BakeMesh(const SMeshBakingInputParams* pInputParams, SMeshBakingOutput* pReturnValues) override;
#endif

	virtual int  GetOcclusionBuffer(uint16* pOutOcclBuffer, int32 nSizeX, int32 nSizeY, Matrix44* pmViewProj, Matrix44* pmCamBuffer) override;

	virtual void WaitForParticleBuffer() override;
	void         InsertParticleVideoDataFence();

	void         FX_StencilTestCurRef(bool bEnable, bool bNoStencilClear = true, bool bStFuncEqual = true);
	void         EF_PrepareCustomShadowMaps(CRenderView* pRenderView);
	void         EF_PrepareAllDepthMaps(CRenderView* pRenderView);
	void         FX_StencilCullPass(int nStencilID, int nNumVers, int nNumInds);
	void         FX_StencilFrustumCull(int nStencilID, const SRenderLight* pLight, ShadowMapFrustum* pFrustum, int nAxis);
	void         FX_StencilCull(int nStencilID, t_arrDeferredMeshIndBuff& arrDeferredInds, t_arrDeferredMeshVertBuff& arrDeferredVerts, CShader* pShader);
	void         FX_StencilRefreshCustomVolume(int StencilFunc, uint32 nStencRef, uint32 nStencMask,
	                                           Vec3* pVerts, uint32 nNumVerts, uint16* pInds, uint32 nNumInds);
	void         FX_StencilCullNonConvex(int nStencilID, IRenderMesh* pWaterTightMesh, const Matrix34& mWorldTM);

	void         FX_ZTargetReadBack();

	void         FX_UpdateCharCBs();
	virtual void* FX_AllocateCharInstCB(SSkinningData*, uint32) override;
	virtual void  FX_ClearCharInstCB(uint32) override;

	bool CreateAuxiliaryMeshes();
	bool ReleaseAuxiliaryMeshes();

	bool CreateUnitVolumeMesh(t_arrDeferredMeshIndBuff& arrDeferredInds, t_arrDeferredMeshVertBuff& arrDeferredVerts, D3DIndexBuffer*& pUnitFrustumIB, D3DVertexBuffer*& pUnitFrustumVB);
	void FX_CreateDeferredQuad(const SRenderLight* pLight, float maskRTWidth, float maskRTHeight, Matrix44A* pmCurView, Matrix44A* pmCurComposite, float fCustomZ = 0.0f);
	void FX_DeferredShadowPass(const SRenderLight* pLight, ShadowMapFrustum* pShadowFrustum, bool bShadowPass, bool bCloudShadowPass, bool bStencilPrepass, int nLod);
	bool FX_DeferredShadowPassSetup(const Matrix44& mShadowTexGen, ShadowMapFrustum* pShadowFrustum, float maskRTWidth, float maskRTHeight, Matrix44& mScreenToShadow, bool bNearest);
	bool FX_DeferredShadowPassSetupBlend(const Matrix44& mShadowTexGen, int nFrustumNum, float maskRTWidth, float maskRTHeight);
	bool FX_DeferredShadows(CRenderView* pRenderView, SRenderLight* pLight, int maskRTWidth, int maskRTHeight);
	void FX_DeferredShadowsNearFrustum(int maskRTWidth, int maskRTHeight);
	void FX_SetDeferredShadows();
	void FX_DeferredShadowMaskGen(CRenderView* pRenderView, const TArray<uint32>& shadowPoolLights);
	void FX_ShadowBlur(float fShadowBluriness, SDynTexture* tpSrc, CTexture* tpDst, int iShadowMode = -1, bool bScreenVP = false, CTexture* tpDst2 = NULL, CTexture* tpSrc2 = NULL);
	void FX_MergeShadowMaps(CRenderView* pRenderView, ShadowMapFrustum* pDst, const ShadowMapFrustum* pSrc);

	void FX_DrawEffectLayerPasses();
	void FX_DrawDebugPasses();

	void FX_DrawMultiLayers();

	void FX_DrawTechnique(CShader* ef, SShaderTechnique* pTech);

	void FX_RefractionPartialResolve();

	bool FX_HDRScene(bool bEnable, bool bClear = true);
	void FX_HDRRangeAdaptUpdate();

	void FX_RenderForwardOpaque(void (* RenderFunc)(), const bool bLighting, const bool bAllowDeferred);
	void FX_RenderWater(void (* RenderFunc)());
	void FX_RenderFog();

	bool FX_ZScene(bool bEnable, bool bUseHDR, bool bClearZBuffer, bool bRenderNormalsOnly = false, bool bZPrePass = false);
	bool FX_FogScene();
	bool FX_DeferredCaustics();
	bool FX_DeferredWaterVolumeCaustics(const N3DEngineCommon::SCausticInfo& causticInfo);
	bool FX_DeferredRainOcclusionMap(const N3DEngineCommon::ArrOccluders& arrOccluders, const SRainParams& rainVolParams);
	bool FX_DeferredRainOcclusion();
	bool FX_DeferredRainPreprocess();
	bool FX_DeferredRainGBuffer();
	bool FX_DeferredSnowLayer();
	bool FX_DeferredSnowDisplacement();
	bool FX_DisplayFogScene();
	bool FX_MotionVectorGeneration(bool bEnable);
	bool FX_CustomRenderScene(bool bEnable);
	void FX_PostProcessSceneHDR();
	bool FX_PostProcessScene(bool bEnable);
	bool FX_DeferredRendering(CRenderView* pRenderView, bool bDebugPass = false, bool bUpdateRTOnly = false);
	bool FX_DeferredDecals();
	bool FX_SkinRendering(bool bEnable);
	void FX_LinearizeDepth();
	void FX_DepthFixupMerge();

#ifdef SUPPORTS_MSAA
	void FX_MSAACustomResolve();
	void FX_MSAASampleFreqStencilSetup(const uint32 nMSAAFlags = 0, const uint8 nStencilRef = 0);
	void FX_MSAARestoreSampleMask(bool bForceRestore = false);
#endif

	// Performance queries
	//=======================================================================

	virtual float GetGPUFrameTime() override;
	virtual void  GetRenderTimes(SRenderTimes& outTimes) override;

	// Shaders pipeline
	//=======================================================================
	virtual void FX_PipelineShutdown(bool bFastShutdown = false) override;
	virtual void RT_GraphicsPipelineShutdown() override;

	virtual void EF_ClearTargetsImmediately(uint32 nFlags) override;
	virtual void EF_ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors, float fDepth, uint8 nStencil) override;
	virtual void EF_ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors) override;
	virtual void EF_ClearTargetsImmediately(uint32 nFlags, float fDepth, uint8 nStencil) override;

	virtual void EF_ClearTargetsLater(uint32 nFlags) override;
	virtual void EF_ClearTargetsLater(uint32 nFlags, const ColorF& Colors, float fDepth, uint8 nStencil) override;
	virtual void EF_ClearTargetsLater(uint32 nFlags, const ColorF& Colors) override;
	virtual void EF_ClearTargetsLater(uint32 nFlags, float fDepth, uint8 nStencil) override;

	void         FX_Invalidate();
	void         EF_Restore();
	virtual void FX_SetState(int st, int AlphaRef = -1, int RestoreState = 0) override;
	void         FX_StateRestore(int prevState);

	void         ChangeLog();

	virtual void SetCurDownscaleFactor(Vec2 sf) final;
	// sets fullscreen viewport regardless of current scaling -
	// should only be used for final upscale (in Post AA)
	void         SetFullscreenViewport();

	void         SetLogFuncs(bool bEnable);
	void         MemReplayWrapD3DDevice();

	bool         CreateMSAADepthBuffer();
	void         PostMeasureOverdraw();
	void         DrawTexelsPerMeterInfo();
	virtual bool CheckDeviceLost() override;

	void         EF_DirtyMatrix();
	void         EF_PushMatrix();
	void         EF_PopMatrix();
	void         FX_ResetPipe();

	void         EF_SetGlobalColor(float r, float g, float b, float a);
	void         EF_SetVertColor();

	//================================================================================
#if defined(FEATURE_PER_SHADER_INPUT_LAYOUT_CACHE)
	static uint32 FX_GetInputLayoutCacheId(int StreamMask, EVertexFormat eVF);
#endif

	HRESULT FX_SetVertexDeclaration(int StreamMask, EVertexFormat eVF);
	bool    FX_SetStreamFlags(SShaderPass* pPass);

#if CRY_PLATFORM_DURANGO
	virtual void RT_SuspendDevice() override;
	virtual void RT_ResumeDevice() override;
#endif
	void         FX_FlushSkinVSParams(CHWShader_D3D* pVS, int nFirst, int nBones, int nOffsVS, int numBonesPerChunk, int nSlot, DualQuat* pSkinQuats, DualQuat* pMBSkinQuats);
	void         FX_DrawBatch(CShader* pSh, SShaderPass* pPass);
	void         FX_DrawBatchSkinned(CShader* pSh, SShaderPass* pPass, SSkinningData* pSkinningData);

	//========================================================================================

	void FX_SetActiveRenderTargets();

	void FX_SetViewport();
	void FX_ClearTargets();

	/* NOTES:
	 *  - passing D3DSurface/D3DDepthSurface are the close-to-metal calls and might not be
	 *    implemented for a specific device
	 *  - using rectangles and passing optional=true will use the most close-to-metal call
	 *    possible, even if it clears more than the given rect - otherwise a
	 *    shader-implementation will do the exact rect clear
	 *
	 *  - if there is a fallback, then the RT-stack is utilized to clear, which comes with
	 *    moderate CPU-overhead (relative to the cost of the close-to-metal call)
	 */

	void FX_ClearTarget(D3DSurface* pView, const ColorF& cClear, const uint numRects = 0, const RECT* pRects = nullptr);
	void FX_ClearTarget(CTexture* pTex, const ColorF& cClear, const uint numRects, const RECT* pRects, const bool bOptionalRects);
	void FX_ClearTarget(CTexture* pTex, const ColorF& cClear);
	void FX_ClearTarget(CTexture* pTex);

	void FX_ClearTarget(D3DDepthSurface* pView, const int nFlags, const float cDepth, const uint8 cStencil, const uint numRects = 0, const RECT* pRects = nullptr);
	void FX_ClearTarget(SDepthTexture* pTex, const int nFlags, const float cDepth, const uint8 cStencil, const uint numRects, const RECT* pRects, const bool bOptionalRects);
	void FX_ClearTarget(SDepthTexture* pTex, const int nFlags, const float cDepth, const uint8 cStencil);
	void FX_ClearTarget(SDepthTexture* pTex, const int nFlags);
	void FX_ClearTarget(SDepthTexture* pTex);

	// shader-implementation of clear
	void           FX_ClearTargetRegion(const uint32 nAdditionalStates = 0);

	bool           FX_GetTargetSurfaces(CTexture* pTarget, D3DSurface*& pTargSurf, SRenderTargetStack* pCur, int nCMSide = -1, int nTarget = 0, uint32 nTileCount = 1);
	bool           FX_SetRenderTarget(int nTarget, D3DSurface* pTargetSurf, SDepthTexture* pDepthTarget, uint32 nTileCount = 1);
	bool           FX_PushRenderTarget(int nTarget, D3DSurface* pTargetSurf, SDepthTexture* pDepthTarget, uint32 nTileCount = 1);
	bool           FX_SetRenderTarget(int nTarget, CTexture* pTarget, SDepthTexture* pDepthTarget, bool bPush = false, int nCMSide = -1, bool bScreenVP = false, uint32 nTileCount = 1);
	bool           FX_PushRenderTarget(int nTarget, CTexture* pTarget, SDepthTexture* pDepthTarget, int nCMSide = -1, bool bScreenVP = false, uint32 nTileCount = 1);
	bool           FX_RestoreRenderTarget(int nTarget);
	bool           FX_PopRenderTarget(int nTarget);
	SDepthTexture* FX_GetDepthSurface(int nWidth, int nHeight, bool bAA, bool bExactMatch = false);
	SDepthTexture* FX_CreateDepthSurface(int nWidth, int nHeight, bool bAA);

	//========================================================================================

	void                   FX_Commit();
	void                   FX_ZState(uint32& nState);
	void                   FX_HairState(uint32& nState, const SShaderPass* pPass);
	bool                   FX_SetFPMode();

	ILINE uint32           PackBlendModeAndPassGroup() { return ((m_RP.m_nPassGroupID << 24) | (m_RP.m_CurState & GS_BLEND_MASK)); }
	ILINE bool             ShouldApplyFogCorrection()  { return PackBlendModeAndPassGroup() != m_uLastBlendFlagsPassGroup; }

	void                   FX_CommitStates(const SShaderTechnique* pTech, const SShaderPass* pPass, bool bUseMaterialState);

	void                   FX_DrawRE(CShader* sh, SShaderPass* sl);

	HRESULT                FX_SetVStream(int nID, const void* pB, uint32 nOffs, uint32 nStride);
	HRESULT                FX_SetIStream(const void* pB, uint32 nOffs, RenderIndexType idxType);

	uint32                 ApplyIndexBufferBindOffset(uint32 firstIndex);

	HRESULT                FX_ResetVertexDeclaration();

	ILINE D3DPrimitiveType FX_ConvertPrimitiveType(const ERenderPrimitiveType eType) const { assert(eType != eptHWSkinGroups); return (D3DPrimitiveType) eType; }

	void                   FX_DrawIndexedPrimitive(const ERenderPrimitiveType eType, const int nVBOffset, const int nMinVertexIndex, const int nVerticesCount, const int nStartIndex, const int nNumIndices, bool bInstanced = false);

	// This is a cross-platform low-level function for DIP call
	void FX_DrawPrimitive(const ERenderPrimitiveType eType, const int nStartVertex, const int nVerticesCount, const int nInstanceVertices = 0);

	bool FX_CommitStreams(SShaderPass* sl, bool bSetVertexDecl = true);

	// get the anti aliasing formats available for current width, height and BPP
	int  GetAAFormat(TArray<SAAFormat>& Formats);

	void EF_SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa);

	void FX_HDRPostProcessing();

	void FX_PreRender(int Stage);
	void FX_PostRender();

	void EF_Init();

	void EF_InitWaveTables();

	// Arguments
	//   vertexformat - 0..VERTEX_FORMAT_NUMS-1
	void EF_OnDemandVertexDeclaration(SOnDemandD3DVertexDeclaration& out, const int nStreamMask, const int vertexformat, const bool bMorph, const bool bInstanced);

	void EF_InitD3DVertexDeclarations();
	void EF_SetCameraInfo();
	void FX_SetObjectTransform(CRenderObject* obj, CShader* pSH, uint64 nObjFlags);
	bool FX_ObjectChange(CShader* Shader, CShaderResources* pRes, CRenderObject* pObject, CRendElementBase* pRE);

private:
	friend class CStandardGraphicsPipeline;

	bool ScreenShotInternal(const char* filename, int width, EScreenShotMode eScreenShotMode = eScreenShotMode_Normal); //Helper method for Screenshot to reduce stack usage
	void UpdateNearestChange(int flags);                                                                                //Helper method for FX_ObjectChange to avoid I-cache misses
	void HandleDefaultObject();                                                                                         //Helper method for FX_ObjectChange to avoid I-cache misses
	void UpdatePrevMatrix(bool bEnable);                                                                                //Update smoothed previous frame view projection matrix by taking average difference over several frames

	// Get pointers to current D3D11 shaders, set tessellation related RT flags and return true if tessellation is enabled for current object
	bool        FX_SetTessellationShaders(CHWShader_D3D*& pCurHS, CHWShader_D3D*& pCurDS, const SShaderPass* pPass);
#ifdef TESSELLATION_RENDERER
	inline void FX_SetAdjacencyOffsetBuffer();
#endif

public:
	static void FX_DrawWire();
	static void FX_DrawNormals();
	static void FX_DrawTangents();

	static void FX_FlushShader_ShadowGen();
	static void FX_FlushShader_General();
	static void FX_FlushShader_ZPass();

	static bool FX_UpdateDynamicShaderResources(const CShaderResources* shaderResources, uint32 batchFilter, uint32 flags2);

	static void FX_SelectTechnique(CShader* pShader, SShaderTechnique* pTech);

	void        FX_UnbindBuffer(D3DBuffer*);
	void        FX_Flush_ForProfiler(int nEnd);

	void        EF_Scissor(bool bEnable, int sX, int sY, int sWdt, int sHgt);
	bool        EF_GetScissorState(int& sX, int& sY, int& sWdt, int& sHgt);

	void        EF_SetFogColor(const ColorF& Color);
	void        FX_FogCorrection();

#if defined(DO_RENDERSTATS)
	bool FX_ShouldTrackStats();
	void FX_TrackStats(CRenderObject* pObj, IRenderMesh* pRenderMesh);
#endif

	void FX_DrawIndexedMesh(const ERenderPrimitiveType nPrimType);
#ifdef CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK
	bool FX_DebugCheckConsistency(int FirstVertex, int FirstIndex, int RendNumVerts, int RendNumIndices);
#else
	bool FX_DebugCheckConsistency(int FirstVertex, int FirstIndex, int RendNumVerts, int RendNumIndices) { return true; }
#endif
	bool FX_SetResourcesState();

	int  EF_Preprocess(SRendItem* ri, uint32 nums, uint32 nume, RenderFunc pRenderFunc, const SRenderingPassInfo& passInfo);

	void FX_StartBatching();

	void DrawRenderItems(const SGraphicsPipelinePassContext& passContext);

	void FX_ProcessRenderStates(int nums, int nume, const SGraphicsPipelinePassContext& passContext);
	void FX_ProcessZPassRenderLists();
	void FX_ProcessZPassRender_List(ERenderListID list, uint32 filter);
	void FX_ProcessSoftAlphaTestRenderLists();
	void FX_ProcessSkinRenderLists(int nList, void (* RenderFunc)(), bool bLighting);
	void FX_ProcessEyeOverlayRenderLists(int nList, void (* RenderFunc)(), bool bLightin);
	void FX_ProcessHalfResParticlesRenderList(CRenderView* pRenderView, int nList, void (* RenderFunc)(), bool bLighting);
	void FX_WaterVolumesPreprocess();
	void FX_ProcessPostRenderLists(uint32 nBatchFilter);

	void FX_ProcessRenderList(int nList, uint32 nBatchFilter);
	void FX_ProcessRenderList(int nList, void (* RenderFunc)(), bool bLighting, uint32 nBatchFilter = FB_GENERAL, uint32 nBatchExcludeFilter = 0);

	void OldPipeline_ProcessRenderList(CRenderView::RenderItems& renderItems, int nums, int nume, int nList, void (* RenderFunc)(), bool bLighting, uint32 nBatchFilter = FB_GENERAL, uint32 nBatchExcludeFilter = 0);
	void OldPipeline_ProcessBatchesList(CRenderView::RenderItems& renderItems, int nums, int nume, uint32 nBatchFilter, uint32 nBatchExcludeFilter = 0);
	void FX_ProcessCharDeformation(CRenderView* pRenderView);

	void FX_WaterVolumesCaustics(CRenderView* pRenderView);
	void FX_WaterVolumesCausticsPreprocess(N3DEngineCommon::SCausticInfo& causticInfo);
	bool FX_WaterVolumesCausticsUpdateGrid(N3DEngineCommon::SCausticInfo& causticInfo);

	// This method takes CRenderView prepared by 3D engine after it fully finished,and send it to the Renderer for drawing.
	void             SubmitRenderViewForRendering(void (* RenderFunc)(), int nFlags, SViewport& VP, const SRenderingPassInfo& passInfo, bool bSync3DEngineJobs);

	void             FX_ProcessPostGroups(int nums, int nume);

	virtual void     EF_EndEf3D(const int nFlags, const int nPrecacheUpdateId, const int nNearPrecacheUpdateId, const SRenderingPassInfo& passInfo) override;
	virtual void     EF_EndEf2D(const bool bSort) override; // 2d only

	virtual WIN_HWND GetHWND() override { return m_hWnd; }
	virtual bool     SetWindowIcon(const char* path) override;
	static void      SetWindowIconCVar(ICVar* pVar);

	static void      SetMouseCursorIconCVar(ICVar* pVar);

	virtual void     SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa) override;

	//////////////////////////////////////////////////////////////////////////
public:
	bool IsDeviceLost() { return(m_bDeviceLost != 0); }
	void InitNVAPI();
	void InitAMDAPI();

#if defined(FEATURE_SVO_GI)
	virtual ISvoRenderer* GetISvoRenderer() override;
#endif

	virtual IRenderAuxGeom*          GetIRenderAuxGeom(void* jobID = 0) override;

	virtual IColorGradingController* GetIColorGradingController() override;

	virtual IStereoRenderer*         GetIStereoRenderer() override;

	virtual void                     StartLoadtimeFlashPlayback(ILoadtimeCallback* pCallback) override;
	virtual void                     StopLoadtimeFlashPlayback() override;

	CStandardGraphicsPipeline& GetGraphicsPipeline() { return *m_pGraphicsPipeline; }
	CTiledShading&             GetTiledShading()     { return *m_pTiledShading; }

	CVolumetricFog&            GetVolumetricFog()    { return m_volumetricFog; }

	CVolumetricCloudManager&   GetVolumetricCloud()  { return *m_pVolumetricCloudMan; }

	virtual void               PushVolumetricCloudBlocker(const Vec3& pos, const Vec3& param, int flag) override;
	virtual void                   PushFogVolume(class CREFogVolume* pFogVolume, const SRenderingPassInfo& passInfo) override;

	CD3DStereoRenderer&            GetS3DRend() const { return *m_pStereoRenderer; }
	virtual bool                   IsStereoEnabled() const override;

	virtual void                   EnableGPUTimers2(bool bEnabled) override;
	virtual void                   AllowGPUTimers2(bool bAllow) override;

	virtual const RPProfilerStats* GetRPPStats(ERenderPipelineProfilerStats eStat, bool bCalledFromMainThread = true) override;
	virtual const RPProfilerStats* GetRPPStatsArray(bool bCalledFromMainThread = true) override;

	virtual int                    GetPolygonCountByType(uint32 EFSList, EVertexCostTypes vct, uint32 z, bool bCalledFromMainThread = true) override;

	virtual void                   LogShaderImportMiss(const CShader* pShader) override;

	void                           BeginRenderDocCapture();
	void                           EndRenderDocCapture();

#ifdef SUPPORT_HW_MOUSE_CURSOR
	virtual IHWMouseCursor* GetIHWMouseCursor() override;
#endif

	virtual IGraphicsDeviceConstantBufferPtr CreateGraphiceDeviceConstantBuffer() final;

	virtual gpu_pfx2::IManager*              GetGpuParticleManager() override;

	const std::vector<SSkinningData*>&       GetComputeSkinningDataListRT() const;
private:
	void                                     HandleDisplayPropertyChanges();

	bool                                     CaptureFrameBufferToFile(const char* pFilePath, CTexture* pRenderTarget = 0);
	// Store local pointers to CVars used for capturing
	void                                     CaptureFrameBuffer();
	// Resolve supersampled back buffer
	void                                     ResolveSupersampledBackbuffer();
	// Scale back buffer contents to match viewport
	void                                     ScaleBackbufferToViewport();
	virtual void                             EnablePipelineProfiler(bool bEnable) override;

	// Called before starting drawing new frame
	virtual void ClearPerFrameData(const SRenderingPassInfo& passInfo) override;
#if CRY_PLATFORM_WINDOWS
public:
	// Called to inspect window messages sent to this renderer's windows
	virtual bool HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) override;
#endif

	//#FIX , temporary code to define window parameters that don't change when the rendering resolution changes
	void OverrideWindowParameters(bool overrideParameters, int width = 0, int height = 0, bool fullscreen = true);

public:
	//////////////////////////////////////////////////////////////////////////
	// PUBLIC MEMBERS
	//////////////////////////////////////////////////////////////////////////

#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	// sprite cells referenced this frame (particular sprite in particular atlas)
	std::set<uint32> m_SpriteCellsUsed;
	// sprite atlases referenced this frame
	std::set<CTexture*> m_SpriteAtlasesUsed;
#endif

#if ENABLE_STATOSCOPE
	IStatoscopeDataGroup * m_pGPUTimersDG;
	IStatoscopeDataGroup* m_pGraphicsDG;
	IStatoscopeDataGroup* m_pPerformanceOverviewDG;
#endif

	TArray<COcclusionQuery> m_OcclQueries;
	uint32 m_OcclQueriesUsed;

#if defined(SUPPORT_D3D_DEBUG_RUNTIME)
	CD3DDebug m_d3dDebug;
	bool      m_bUpdateD3DDebug;
#endif

	DWORD              m_DeviceOwningthreadID; // thread if of thread who is allows to access the D3D Context
	volatile int       m_nAsyncDeviceState;    // counter of how many jobs are currently executed in parallel on the device

	ID3D11InputLayout* m_pLastVDeclaration;

#if CRY_PLATFORM_DURANGO
	ID3DXboxPerformanceContext* m_pPerformanceDeviceContext;
#endif

	DXGI_SURFACE_DESC        m_d3dsdBackBuffer; // Surface desc of the BackBuffer
	DXGI_FORMAT              m_ZFormat;
	D3D11_PRIMITIVE_TOPOLOGY m_CurTopology;

	TArray<SStateBlend>      m_StatesBL;
	TArray<SStateRaster>     m_StatesRS;
	TArray<SStateDepth>      m_StatesDP;
	uint32                   m_nCurStateBL;
	uint32                   m_nCurStateRS;
	uint32                   m_nCurStateDP;
	uint8                    m_nCurStencRef;

#ifdef USE_PIX_DURANGO
	ID3DUserDefinedAnnotation* m_pPixPerf;
#endif
	SDepthTexture              m_DepthBufferOrig;
	SDepthTexture              m_DepthBufferOrigMSAA;
	SDepthTexture              m_DepthBufferNative;

	// x = average luminance, y = max luminance, z = min luminance,
	Vec4        m_vSceneLuminanceInfo;

	float       m_fScotopicSceneScale;
	float       m_fAdaptedSceneScale;
	float       m_fAdaptedSceneScaleLBuffer;

	ETEX_Format m_HDR_FloatFormat_Scalar;
	ETEX_Format m_HDR_FloatFormat;
	int         m_MaxAnisotropyLevel;
	int         m_nMaterialAnisoHighSampler;
	int         m_nMaterialAnisoLowSampler;
	int         m_nMaterialAnisoSamplerBorder;
	int         m_nPointWrapSampler;
	int         m_nPointClampSampler;

	CCryNameR   m_nmInstancingParams;
	CCryNameR   m_nmInstancingData;

	//////////////////////////////////////////////////////////////////////////
	enum { kUnitObjectIndexSizeof = 2 };

	D3DVertexBuffer* m_pUnitFrustumVB[SHAPE_MAX];
	D3DIndexBuffer*  m_pUnitFrustumIB[SHAPE_MAX];

	int              m_UnitFrustVBSize[SHAPE_MAX];
	int              m_UnitFrustIBSize[SHAPE_MAX];

	D3DVertexBuffer* m_pQuadVB;
	int16            m_nQuadVBSize;

	//////////////////////////////////////////////////////////////////////////
	SPixFormatSupport           m_hwTexFormatSupport;

	byte                        m_GammmaTable[256];

	int                         m_fontBlendMode;

	CCryNameTSCRC               m_LevelShaderCacheMissIcon;

	CColorGradingControllerD3D* m_pColorGradingControllerD3D;

	CRenderPipelineProfiler*    m_pPipelineProfiler;

#if CRY_PLATFORM_DURANGO
	CryCriticalSection       m_dma1Lock;
	ID3D11DmaEngineContextX* m_pDMA1;
#endif

#ifdef SHADER_ASYNC_COMPILATION
	std::vector<CAsyncShaderTask*> m_AsyncShaderTasks;
#endif

	Matrix44             m_matPsmWarp;
	Matrix44             m_matViewInv;
	int                  m_MatDepth;

	string               m_Description;
	bool                 m_bFullScreen;

	TArray<SD3DContext*> m_RContexts;
	SD3DContext*         m_CurrContext;

	TArray<CTexture*>    m_RTargets;

	short                m_nPrepareShadowFrame;

	static const int     MAX_RT_STACK = 8;

#if CRY_PLATFORM_WINDOWS || defined(OPENGL)
	static const int       RT_STACK_WIDTH = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
#else
	static const int       RT_STACK_WIDTH = 4;
#endif
	int                    m_nRTStackLevel[RT_STACK_WIDTH];
	SRenderTargetStack     m_RTStack[RT_STACK_WIDTH][MAX_RT_STACK];

	int                    m_nMaxRT2Commit;
	SRenderTargetStack*    m_pNewTarget[RT_STACK_WIDTH];
	CTexture*              m_pCurTarget[RT_STACK_WIDTH];

	TArray<SDepthTexture*> m_TempDepths;

	enum { MAX_WIREFRAME_STACK = 10 };
	int   m_arrWireFrameStack[MAX_WIREFRAME_STACK];
	int   m_nWireFrameStack;

	bool  m_bDepthBoundsEnabled;
	float m_fDepthBoundsMin, m_fDepthBoundsMax;

private:
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE MEMBERS
	//////////////////////////////////////////////////////////////////////////
	int  m_scissorPrevX, m_scissorPrevY, m_scissorPrevWidth, m_scissorPrevHeight;
	bool m_bScissorPrev;

	// Windows context
	char      m_WinTitle[80];
	HINSTANCE m_hInst;
	HWND      m_hWnd;              // The main app window
	HWND      m_hWndDesktop;       // The desktop window
#if CRY_PLATFORM_WINDOWS
	HICON     m_hIconBig;          // Icon currently being used on the taskbar
	HICON     m_hIconSmall;        // Icon currently being used on the window
	string    m_iconPath;          // Path to the icon currently loaded
#endif

	uint32             m_frameFenceCounter;
	uint32             m_completedFrameFenceCounter;
	DeviceFenceHandle  m_frameFences[MAX_FRAMES_IN_FLIGHT];

	int                m_bDraw2dImageStretchMode;
	int                m_nPointState;
	uint32             m_uLastBlendFlagsPassGroup;

	DWORD              m_FenceOcclusionReady;
	DWORD              m_PrevFenceOcclusionReady;
	int                m_numOcclusionDownsampleStages;
	uint16             m_occlusionDownsampleSizeX;
	uint16             m_occlusionDownsampleSizeY;
	uint16             m_occlusionRequestedSizeX;
	uint16             m_occlusionRequestedSizeY;
	uint16             m_occlusionSourceSizeX;
	uint16             m_occlusionSourceSizeY;
	std::vector<float> m_occlusionZBuffer;
	Matrix44A          m_occlusionViewProjBuffer[4];
	Matrix44           m_occlusionLastProj;
	float              m_occlusionLastZNear;
	float              m_occlusionLastZFar;
	Matrix44A          m_occlusionViewProj;
	size_t             m_occlusionBuffer;

#if CRY_PLATFORM_DURANGO // members used for reprojection on durango
	int               m_occlusionDataPitch;
	float             m_occlusionZNear[2];
	float             m_occlusionZFar[2];
	DeviceFenceHandle m_occlusionFence[2];
	void*             m_occlusionGPUData[2];
	D3DTexture*       m_occlusionReadBackTexture[2];
#endif

	bool                                m_bOcclusionTexturesValid;

	CStandardGraphicsPipeline*          m_pGraphicsPipeline;
	CTiledShading*                      m_pTiledShading;
	CD3DStereoRenderer*                 m_pStereoRenderer;
	CVolumetricFog                      m_volumetricFog;

	CVolumetricCloudManager*            m_pVolumetricCloudMan;

	std::vector<_smart_ptr<D3DSurface>> m_pBackBuffers;
	D3DSurface*                         m_pBackBuffer;
	unsigned int                        m_pCurrentBackBufferIndex;

	CTexture*                           m_pZTexture;
	D3DTexture*                         m_pZTarget;
	D3DDepthSurface*                    m_pZSurface;

	CTexture*                           m_pNativeZTexture;
	D3DTexture*                         m_pNativeZTarget;
	D3DDepthSurface*                    m_pNativeZSurface;

	volatile int                        m_lockCharCB;
	util::list<SCharacterInstanceCB>    m_CharCBFreeList;
	util::list<SCharacterInstanceCB>    m_CharCBActiveList[3];
	volatile int                        m_CharCBFrameRequired[3];
	volatile int                        m_CharCBAllocated;

#if !CRY_PLATFORM_DURANGO
	IDXGISwapChain*  m_pSwapChain;
#else
	IDXGISwapChain1* m_pSwapChain;
#endif
	enum PresentStatus
	{
		epsOccluded     = 1 << 0,
		epsNonExclusive = 1 << 1,
	};
	DWORD            m_dwPresentStatus; // Indicate present status

	DWORD            m_dwCreateFlags;      // Indicate sw or hw vertex processing
	DWORD            m_dwWindowStyle;      // Saved window style for mode switches
	char             m_strDeviceStats[90]; // String to hold D3D device stats

	int              m_SceneRecurseCount;

	SRenderTileInfo  m_RenderTileInfo;

	TArray<S2DImage> m_2dImages;
	TArray<S2DImage> m_uiImages;
	//==================================================================

#if CRY_PLATFORM_WINDOWS
	uint m_nConnectedMonitors;  // The number of monitors currently connected to the system
	bool m_bDisplayChanged;     // Dirty-flag set when the number of monitors in the system changes
#endif

#if defined(ENABLE_PROFILING_CODE)
	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS || defined(OPENGL)
	CTexture* m_pSaveTexture[2];
	#endif

	// Surfaces used to capture the current screen
	unsigned int           m_captureFlipFlop;
	// Variables used for frame buffer capture and callback
	ICaptureFrameListener* m_pCaptureCallBack[MAXFRAMECAPTURECALLBACK];
	unsigned int           m_frameCaptureRegisterNum;
	int                    m_nScreenCaptureRequestFrame[RT_COMMAND_BUF_COUNT];
	int                    m_screenCapTexHandle[RT_COMMAND_BUF_COUNT];
#endif

	// fields to access the D3D Driver: Device, Context and other related objects
	// all should be accessed through the provided getters:
	// GetDevice(), GetDeviceContext() and so on
	// To access the device without synchronization (which happens in presentce of a asynchrounous device)
	// please use GetDevice_Unsynchronized(), GetDeviceContext_Unsynchronized() etc
	CCryDeviceWrapper        m_DeviceWrapper;
	CCryDeviceContextWrapper m_DeviceContextWrapper;

#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	CCryPerformanceDeviceWrapper        m_PerformanceDeviceWrapper;
	CCryPerformanceDeviceContextWrapper m_PerformanceDeviceContextWrapper;
#endif

	t_arrDeferredMeshIndBuff  m_arrDeferredInds;
	t_arrDeferredMeshVertBuff m_arrDeferredVerts;

#if defined(SUPPORT_DEVICE_INFO)
	DeviceInfo m_devInfo;
#endif

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	struct SSF_ResourcesD3D* m_pSFResD3D;
#endif

#if defined(ENABLE_RENDER_AUX_GEOM)
	CRenderAuxGeomD3D*                  m_pRenderAuxGeomD3D;
#endif
	CAuxGeomCB_Null                     m_renderAuxGeomNull;

	std::unique_ptr<gpu_pfx2::CManager> m_gpuParticleManager;

	CShadowTextureGroupManager          m_ShadowTextureGroupManager;  // to combine multiple shadowmaps into one texture

	static TArray<CRenderObject*>       s_tempObjects[2];
	static TArray<SRendItem*>           s_tempRIs;

	//#FIX , temporary code to define window parameters that don't change when the rendering resolution changes
	bool  m_windowParametersOverridden;
	Vec2i m_overriddenWindowSize;
	bool  m_overriddenWindowFullscreenState;
	//////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_BENCHMARK_SENSOR
public:
	BenchmarkFramework::IBenchmarkRendererSensor* m_benchmarkRendererSensor;
private:
#endif
};

enum { SPCBI_NUMBER_OF_BUFFERS = 64 };

struct SPersistentConstBufferInfo
{
	uint64           m_crc[SPCBI_NUMBER_OF_BUFFERS];
	CConstantBuffer* m_pStaticInstCB[SPCBI_NUMBER_OF_BUFFERS];
	int              m_frameID;
	int              m_buffer;
};

///////////////////////////////////////////////////////////////////////////////
inline void CD3D9Renderer::BindContextToThread(DWORD threadID)
{
#if ENABLE_CONTEXT_THREAD_CHECKING
	m_DeviceOwningthreadID = threadID;
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CD3D9Renderer::CheckContextThreadAccess() const
{
#if ENABLE_CONTEXT_THREAD_CHECKING
	if (m_DeviceOwningthreadID != CryGetCurrentThreadId())
		CryFatalError("accessing d3d11 immediate context from unbound thread!");
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline DWORD CD3D9Renderer::GetBoundThreadID() const
{
	return m_DeviceOwningthreadID;
}

///////////////////////////////////////////////////////////////////////////////
inline void CD3D9Renderer::WaitForAsynchronousDevice() const
{
	if (m_nAsyncDeviceState)
	{
		CRY_PROFILE_REGION_WAITING(PROFILE_RENDERER, "Sync Async DIPS");
		CRYPROFILE_SCOPE_PROFILE_MARKER("Sync Async DIPS");

		while (m_nAsyncDeviceState)
		{
#if CRY_PLATFORM_ORBIS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
			CrySleep(0);
#else
			SwitchToThread();
#endif
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
inline volatile int* CD3D9Renderer::GetAsynchronousDeviceState()
{
	CryInterlockedIncrement(&m_nAsyncDeviceState);
	return &m_nAsyncDeviceState;

}

//////////////////////////////////////////////////////////////////////////
inline void CD3D9Renderer::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topType)
{
	if (m_CurTopology != topType)
	{
		m_CurTopology = topType;
		m_DevMan.BindTopology(m_CurTopology);
	}
}

inline bool CD3D9Renderer::GetDepthBoundTestState(float& fMin, float& fMax) const
{
	if (!m_bDeviceSupports_NVDBT)
		return false;
	fMin = m_fDepthBoundsMin;
	fMax = m_fDepthBoundsMax;
	return m_bDepthBoundsEnabled;
}

inline bool CD3D9Renderer::FX_SetStreamFlags(SShaderPass* pPass)
{
	if (CV_r_usehwskinning && m_RP.m_pRE && (m_RP.m_pRE->m_Flags & FCEF_SKINNED))
	{
		m_RP.m_FlagsStreams_Decl |= VSM_HWSKIN;
		m_RP.m_FlagsStreams_Stream |= VSM_HWSKIN;
		return true;
	}

	return false;
}

inline uint32 CD3D9Renderer::ApplyIndexBufferBindOffset(uint32 firstIndex)
{
#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
	return firstIndex + (m_RP.m_IndexStreamOffset >> 1);
#else
	return firstIndex;
#endif
}

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
inline SSF_ResourcesD3D& CD3D9Renderer::SF_GetResources()
{
	assert(m_pSFResD3D);
	return *m_pSFResD3D;
}
#endif // #if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline CCryDeviceWrapper& CD3D9Renderer::GetDevice()
{
	return m_DeviceWrapper;
}

inline const CCryDeviceWrapper& CD3D9Renderer::GetDevice() const
{
	return m_DeviceWrapper;
}

///////////////////////////////////////////////////////////////////////////////
inline CCryDeviceContextWrapper& CD3D9Renderer::GetDeviceContext()
{
	CheckContextThreadAccess();
	WaitForAsynchronousDevice();
	return m_DeviceContextWrapper;
}

#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
///////////////////////////////////////////////////////////////////////////////
inline CCryPerformanceDeviceWrapper& CD3D9Renderer::GetPerformanceDevice()
{
	return m_PerformanceDeviceWrapper;
}

///////////////////////////////////////////////////////////////////////////////
inline CCryPerformanceDeviceContextWrapper& CD3D9Renderer::GetPerformanceDeviceContext()
{
	CheckContextThreadAccess();
	WaitForAsynchronousDevice();
	return m_PerformanceDeviceContextWrapper;
}
#endif // DEVICE_SUPPORTS_PERFORMANCE_DEVICE

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
void UserOverrideDisplayProperties(DXGI_MODE_DESC& desc);
#endif

extern CD3D9Renderer gcpRendD3D;

//=========================================================================================

#include "D3DHWShader.h"

#include "../Common/FencedBuffer.h"
#include "../Common/ParticleBuffer.h"
#include "DeviceManager/TempDynBuffer.h"

//=========================================================================================

void HDR_DrawDebug();

#define STREAMED_TEXTURE_USAGE (CDeviceManager::USAGE_STREAMING)

void EnableCloseButton(void* hWnd, bool enabled);
