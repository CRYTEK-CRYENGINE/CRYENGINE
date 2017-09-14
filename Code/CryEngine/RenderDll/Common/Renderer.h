// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMemory/CryPool/PoolAlloc.h>
#include "TextMessages.h"                             // CTextMessages
#include "RenderAuxGeom.h"
#include "RenderPipeline.h"                           // SFogState, SViewport
#include "RenderThread.h"                             // SRenderThread
#include "../Scaleform/ScaleformRender.h"
#include "../XRenderD3D9/DeviceManager/D3D11/DeviceSubmissionQueue_D3D11.h" // CSubmissionQueue_DX11

typedef void (PROCRENDEF)(SShaderPass* l, int nPrimType);

#define USE_NATIVE_DEPTH 1

#if (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_ORBIS) && !CRY_RENDERER_OPENGL
	#define SUPPORTS_MSAA 1

enum eMsaaParams
{
	MSAA_STENCILCULL                   = (0x1),
	MSAA_STENCILMASK_SET               = (0x2),
	MSAA_STENCILMASK_RESET_BIT         = (0x4),
	MSAA_SAMPLEFREQ_PASS               = (0x8),
	MSAA_SAMPLEFREQ_MASK_SET           = (0x10),
	MSAA_SAMPLEFREQ_MASK_CLEAR_STENCIL = (0x20),
};

#endif

enum eAntialiasingType
{
	eAT_NOAA = 0,
	eAT_SMAA_1X,
	eAT_SMAA_1TX,
	eAT_SMAA_2TX,
	eAT_TSAA,
	eAT_AAMODES_COUNT,

	eAT_DEFAULT_AA                  = eAT_SMAA_1TX,

	eAT_NOAA_MASK                   = (1 << eAT_NOAA),
	eAT_SMAA_1X_MASK                = (1 << eAT_SMAA_1X),
	eAT_SMAA_1TX_MASK               = (1 << eAT_SMAA_1TX),
	eAT_SMAA_2TX_MASK               = (1 << eAT_SMAA_2TX),
	eAT_TSAA_MASK                   = (1 << eAT_TSAA),

	eAT_SMAA_MASK                   = (eAT_SMAA_1X_MASK | eAT_SMAA_1TX_MASK | eAT_SMAA_2TX_MASK),

	eAT_REQUIRES_PREVIOUSFRAME_MASK = (eAT_SMAA_1TX_MASK | eAT_SMAA_2TX_MASK | eAT_TSAA_MASK),
	eAT_REQUIRES_SUBPIXELSHIFT_MASK = (eAT_SMAA_2TX_MASK | eAT_TSAA_MASK)
};

static const char* s_pszAAModes[eAT_AAMODES_COUNT] =
{
	"NO AA",
	"SMAA 1X",
	"SMAA 1TX",
	"SMAA 2TX",
	"TSAA"
};

extern uint32 ColorMasks[(ColorMask::Count >> GS_COLMASK_SHIFT)][4];
extern std::array<uint32, (ColorMask::Count >> GS_COLMASK_SHIFT)> AvailableColorMasks;

struct ShadowMapFrustum;
struct IStatObj;
struct SShaderPass;
class CREParticle;
class CD3DStereoRenderer;
class CTextureManager;
class CIntroMovieRenderer;
class IOpticsManager;
struct SDynTexture2;
class CDeviceResourceSet;
class CVertexBuffer;
class CIndexBuffer;
struct SThreadInfo;

namespace compute_skinning { struct IComputeSkinningStorage; }

typedef int (* pDrawModelFunc)(void);

//=============================================================

#define D3DRGBA(r, g, b, a)                                \
  ((((int)((a) * 255)) << 24) | (((int)((r) * 255)) << 16) \
   | (((int)((g) * 255)) << 8) | (int)((b) * 255)          \
  )

struct alloc_info_struct
{
	int         ptr;
	int         bytes_num;
	bool        busy;
	const char* szSource;
	unsigned Size()                                  { return sizeof(*this); }

	void     GetMemoryUsage(ICrySizer* pSizer) const {}
};

const float TANGENT30_2 = 0.57735026918962576450914878050196f * 2;   // 2*tan(30)

// Assuming 24 bits of depth precision
#define DBT_SKY_CULL_DEPTH                    0.99999994f

#define DEF_SHAD_DBT_DEFAULT_VAL              1

#define TEXSTREAMING_DEFAULT_VAL              1

#define GEOM_INSTANCING_DEFAULT_VAL           0
#define COLOR_GRADING_DEFAULT_VAL             1
#define SUNSHAFTS_DEFAULT_VAL                 2
#define HDR_RANGE_ADAPT_DEFAULT_VAL           0
#define HDR_RENDERING_DEFAULT_VAL             1
#define SHADOWS_POOL_DEFAULT_VAL              1
#define SHADOWS_CLIP_VOL_DEFAULT_VAL          1
#define SHADOWS_BLUR_DEFAULT_VAL              3
#define TEXPREALLOCATLAS_DEFAULT_VAL          0
#define TEXMAXANISOTROPY_DEFAULT_VAL          8
#if CRY_PLATFORM_DESKTOP
	#define TEXNOANISOALPHATEST_DEFAULT_VAL     0
	#define SHADERS_ALLOW_COMPILATION_DEFAULT_VAL 1
#else
	#define TEXNOANISOALPHATEST_DEFAULT_VAL     1
	#define SHADERS_ALLOW_COMPILATION_DEFAULT_VAL 0
#endif
#define ENVTEXRES_DEFAULT_VAL                 3
#define WATERREFLQUAL_DEFAULT_VAL             4
#define DOF_DEFAULT_VAL                       2
#define SHADERS_PREACTIVATE_DEFAULT_VAL       3
#define CUSTOMVISIONS_DEFAULT_VAL             3
#define FLARES_DEFAULT_VAL                    1
#define WATERVOLCAUSTICS_DEFAULT_VAL          1
#define FLARES_HQSHAFTS_DEFAULT_VAL           1
#define DEF_SHAD_DBT_STENCIL_DEFAULT_VAL      1
#define DEF_SHAD_SSS_DEFAULT_VAL              1

#define MULTITHREADED_DEFAULT_VAL             1
#define ZPASS_DEPTH_SORT_DEFAULT_VAL          1
#define TEXSTREAMING_UPDATETYPE_DEFAULT_VAL   1

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#define CBUFFER_NATIVE_DEPTH_DEAFULT_VAL 1
#else
	#define CBUFFER_NATIVE_DEPTH_DEAFULT_VAL 0
#endif

#include "RendererCVars.h"                    // Can only be included after the default values are defined.

struct SSpriteInfo
{
	SDynTexture2*             m_pTex;
	struct SSectorTextureSet* m_pTerrainTexInfo;
	Vec3                      m_vPos;
	float                     m_fDX;
	float                     m_fDY;
	float                     m_fScaleV;
	UCol                      m_Color;
	int                       m_nVI;
	uint8                     m_ucTexCoordMinX; // 0..128 used for the full range (0..1) in the texture (to fit in byte)
	uint8                     m_ucTexCoordMinY; // 0..128 used for the full range (0..1) in the texture (to fit in byte)
	uint8                     m_ucTexCoordMaxX; // 0..128 used for the full range (0..1) in the texture (to fit in byte)
	uint8                     m_ucTexCoordMaxY; // 0..128 used for the full range (0..1) in the texture (to fit in byte)
};

struct SSpriteGenInfo
{
	float          fAngle;                      // horizontal rotation in degree
	float          fGenDist;
	float          fBrightness;
	int            nMaterialLayers;
	IMaterial*     pMaterial;
	float*         pMipFactor;
	uint8*         pTexturesAreStreamedIn;
	SDynTexture2** ppTexture;
	IStatObj*      pStatObj;
	int            nSP;
};

class CMatrixStack
{
public:
	CMatrixStack(uint32 maxDepth, uint32 dirtyFlag);
	~CMatrixStack();

	// Pops the top of the stack, returns the current top
	// *after* popping the top.
	bool Pop();

	// Pushes the stack by one, duplicating the current matrix.
	bool Push();

	// Loads identity in the current matrix.
	bool LoadIdentity();

	// Loads the given matrix into the current matrix
	bool LoadMatrix(const Matrix44* pMat);

	// Right-Multiplies the given matrix to the current matrix.
	// (transformation is about the current world origin)
	bool MultMatrix(const Matrix44* pMat);

	// Left-Multiplies the given matrix to the current matrix
	// (transformation is about the local origin of the object)
	bool MultMatrixLocal(const Matrix44* pMat);

	// Right multiply the current matrix with the computed rotation
	// matrix, counterclockwise about the given axis with the given angle.
	// (rotation is about the current world origin)
	bool RotateAxis(const Vec3* pV, f32 Angle);

	// Left multiply the current matrix with the computed rotation
	// matrix, counterclockwise about the given axis with the given angle.
	// (rotation is about the local origin of the object)
	bool RotateAxisLocal(const Vec3* pV, f32 Angle);

	// Right multiply the current matrix with the computed rotation
	// matrix. All angles are counterclockwise. (rotation is about the
	// current world origin)

	// Right multiply the current matrix with the computed scale
	// matrix. (transformation is about the current world origin)
	bool Scale(f32 x, f32 y, f32 z);

	// Left multiply the current matrix with the computed scale
	// matrix. (transformation is about the local origin of the object)
	bool ScaleLocal(f32 x, f32 y, f32 z);

	// Right multiply the current matrix with the computed translation
	// matrix. (transformation is about the current world origin)
	bool Translate(f32 x, f32 y, f32 z);

	// Left multiply the current matrix with the computed translation
	// matrix. (transformation is about the local origin of the object)
	bool TranslateLocal(f32 x, f32 y, f32 z);

	// Obtain the current matrix at the top of the stack
	inline Matrix44A* GetTop()
	{
		assert(m_pTop != NULL);
		return m_pTop;
	}

	inline int GetDepth() { return m_nDepth; }

private:
	Matrix44A* m_pTop;        //top of the stack
	Matrix44A* m_pStack;      // array of Matrix44
	uint32     m_nDepth;
	uint32     m_nMaxDepth;
	uint32     m_nDirtyFlag; //flag for new matrices
};

//////////////////////////////////////////////////////////////////////
// Class to manage memory for Skinning Renderer Data
class CSkinningDataPool
{
public:
	CSkinningDataPool()
		: m_pPool(NULL)
		, m_pPages(NULL)
		, m_nPoolSize(0)
		, m_nPoolUsed(0)
		, m_nPageAllocated(0)
	{}

	~CSkinningDataPool()
	{
		// free temp pages
		SPage* pPage = m_pPages;
		while (pPage)
		{
			SPage* pPageToDelete = pPage;
			pPage = pPage->pNext;

			CryModuleMemalignFree(pPageToDelete);
		}

		// free pool
		CryModuleMemalignFree(m_pPool);
	}

	byte* Allocate(size_t nBytes)
	{
		// If available use allocated page space
		uint32 nPoolUsed = ~0;
		do
		{

			nPoolUsed = *const_cast<volatile size_t*>(&m_nPoolUsed);
			size_t nPoolFree = m_nPoolSize - nPoolUsed;
			if (nPoolFree < nBytes)
				break; // not enough memory, use fallback
			if (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nPoolUsed), nPoolUsed + nBytes, nPoolUsed) == nPoolUsed)
				return &m_pPool[nPoolUsed];

		}
		while (true);

		// Create memory
		byte* pMemory = alias_cast<byte*>(CryModuleMemalign(Align(nBytes, 16) + 16, 16));
		SPage* pNewPage = alias_cast<SPage*>(pMemory);

		// Assign page
		volatile SPage* pPages = 0;
		do
		{
			pPages = *(const_cast<volatile SPage**>(&m_pPages));
			pNewPage->pNext = alias_cast<SPage*>(pPages);

		}
		while (CryInterlockedCompareExchangePointer(alias_cast<void* volatile*>(&m_pPages), pNewPage, (void*)pPages) != pPages);

		CryInterlockedAdd((volatile int*)&m_nPageAllocated, nBytes);

		return pMemory + 16;
	}

	void ClearPool()
	{
		m_nPoolUsed = 0;
		if (m_nPageAllocated)
		{
			// free temp pages
			SPage* pPage = m_pPages;
			while (pPage)
			{
				SPage* pPageToDelete = pPage;
				pPage = pPage->pNext;

				CryModuleMemalignFree(pPageToDelete);
			}

			// adjust pool size
			CryModuleMemalignFree(m_pPool);
			m_nPoolSize += m_nPageAllocated;
			m_pPool = alias_cast<byte*>(CryModuleMemalign(m_nPoolSize, 16));

			// reset state
			m_pPages = NULL;
			m_nPageAllocated = 0;
		}
	}

	void FreePoolMemory()
	{
		// free temp pages
		SPage* pPage = m_pPages;
		while (pPage)
		{
			SPage* pPageToDelete = pPage;
			pPage = pPage->pNext;

			CryModuleMemalignFree(pPageToDelete);
		}

		// free pool
		CryModuleMemalignFree(m_pPool);

		m_pPool = NULL;
		m_pPages = NULL;
		m_nPoolSize = 0;
		m_nPoolUsed = 0;
		m_nPageAllocated = 0;
	}

	size_t AllocatedMemory()
	{
		return m_nPoolSize + m_nPageAllocated;
	}
private:
	struct SPage
	{
		SPage* pNext;
	};

	byte*  m_pPool;
	SPage* m_pPages;
	size_t m_nPoolSize;
	size_t m_nPoolUsed;
	size_t m_nPageAllocated;
};

//////////////////////////////////////////////////////////////////////
class CFillRateManager : private stl::PSyncDebug
{
public:
	CFillRateManager()
		: m_fTotalPixels(0.f), m_fMaxPixels(1e9f)
	{}
	void Reset()
	{
		m_fTotalPixels = 0.f;
		m_afPixels.resize(0);
	}
	float GetMaxPixels() const
	{
		return m_fMaxPixels;
	}
	void AddPixelCount(float fPixels);
	void ComputeMaxPixels();

private:
	float               m_fTotalPixels;
	float               m_fMaxPixels;
	FastDynArray<float> m_afPixels;
};

//////////////////////////////////////////////////////////////////////
// 3D engine duplicated data for non-thread safe data
namespace N3DEngineCommon
{

struct SOceanInfo
{
	SOceanInfo() : m_nOceanRenderFlags(0), m_fWaterLevel(0.0f), m_vCausticsParams(0.0f, 0.0f, 0.0f, 0.0f), m_vMeshParams(0.0f, 0.0f, 0.0f, 0.0f) {};
	Vec4  m_vCausticsParams;
	Vec4  m_vMeshParams;
	float m_fWaterLevel;
	uint8 m_nOceanRenderFlags;
};

struct SVisAreaInfo
{
	SVisAreaInfo() : nFlags(0)
	{
	};
	uint32 nFlags;
};

struct SRainOccluder
{
	SRainOccluder() : m_RndMesh(0), m_WorldMat(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0) {}
	_smart_ptr<IRenderMesh> m_RndMesh;
	Matrix34                m_WorldMat;
};

typedef std::vector<SRainOccluder> ArrOccluders;

struct SRainOccluders
{
	ArrOccluders m_arrOccluders;
	ArrOccluders m_arrCurrOccluders[RT_COMMAND_BUF_COUNT];
	size_t       m_nNumOccluders;
	bool         m_bProcessed[MAX_GPU_NUM];

	SRainOccluders() : m_nNumOccluders(0) { for (int i = 0; i < MAX_GPU_NUM; ++i) m_bProcessed[i] = true; }
	~SRainOccluders()                                                  { Release(); }
	void Release(bool bAll = false)
	{
		stl::free_container(m_arrOccluders);
		m_nNumOccluders = 0;
		if (bAll)
		{
			for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
				stl::free_container(m_arrCurrOccluders[i]);
		}
		for (int i = 0; i < MAX_GPU_NUM; ++i) m_bProcessed[i] = true;
	}
};

struct SCausticInfo
{
	SCausticInfo() : m_pCausticQuadMesh(0), m_nCausticMeshWidth(0), m_nCausticMeshHeight(0), m_nCausticQuadTaps(0), m_nVertexCount(0), m_nIndexCount(0),
		m_mCausticMatr(IDENTITY), m_mCausticViewMatr(IDENTITY)
	{
	}

	~SCausticInfo() { Release(); }
	void Release()
	{
		m_pCausticQuadMesh = NULL;
	}

	_smart_ptr<IRenderMesh> m_pCausticQuadMesh;
	uint32                  m_nCausticMeshWidth;
	uint32                  m_nCausticMeshHeight;
	uint32                  m_nCausticQuadTaps;
	uint32                  m_nVertexCount;
	uint32                  m_nIndexCount;

	Matrix44A               m_mCausticMatr;
	Matrix34                m_mCausticViewMatr;
};
}

struct S3DEngineCommon
{
	enum EVisAreaFlags
	{
		VAF_EXISTS_FOR_POSITION    = (1 << 0),
		VAF_CONNECTED_TO_OUTDOOR   = (1 << 1),
		VAF_AFFECTED_BY_OUT_LIGHTS = (1 << 2),
		VAF_MASK                   = VAF_EXISTS_FOR_POSITION | VAF_CONNECTED_TO_OUTDOOR | VAF_AFFECTED_BY_OUT_LIGHTS
	};

	N3DEngineCommon::SVisAreaInfo   m_pCamVisAreaInfo;
	N3DEngineCommon::SOceanInfo     m_OceanInfo;
	N3DEngineCommon::SRainOccluders m_RainOccluders;
	N3DEngineCommon::SCausticInfo   m_CausticInfo;
	SRainParams                     m_RainInfo;
	SSnowParams                     m_SnowInfo;

	void Update(threadID nThreadID);
	void UpdateRainInfo(threadID nThreadID);
	void UpdateRainOccInfo(int nThreadID);
	void UpdateSnowInfo(int nThreadID);
};

struct SShowRenderTargetInfo
{
	SShowRenderTargetInfo() { Reset(); }
	void Reset()
	{
		bShowList = false;
		bDisplayTransparent = false;
		col = 2;
		rtList.clear();
	}

	bool bShowList;
	bool bDisplayTransparent;
	int  col;
	struct RT
	{
		CTexture* pTexture;
		Vec4      channelWeight;
		bool      bFiltered;
		bool      bRGBKEncoded;
		bool      bAliased;
	};
	std::vector<RT> rtList;
};

struct SRenderTileInfo
{
	SRenderTileInfo() { nPosX = nPosY = nGridSizeX = nGridSizeY = 0.f; }
	f32 nPosX, nPosY, nGridSizeX, nGridSizeY;
};

struct SVolumetricCloudTexInfo
{
	int32 cloudTexId;
	int32 cloudNoiseTexId;
	int32 edgeNoiseTexId;
};

//////////////////////////////////////////////////////////////////////
class CRenderer : public IRenderer, public CRendererCVars
{
	friend class CRendererCVars;

public:

	CRenderer();
	virtual ~CRenderer();

	virtual void InitRenderer();

	virtual void PostInit() override;

	virtual void StartRenderIntroMovies() override;
	virtual void StopRenderIntroMovies(bool bWaitForFinished) override;
	virtual bool IsRenderingIntroMovies() const override;

	virtual void PostLevelLoading() override;

	void         PreShutDown();
	void         PostShutDown();

	// Multithreading support
#if CRY_PLATFORM_DURANGO
	virtual void SuspendDevice();
	virtual void ResumeDevice();

	virtual void RT_SuspendDevice() = 0;
	virtual void RT_ResumeDevice() = 0;

	bool m_bDeviceSuspended;
#endif

	virtual void SyncComputeVerticesJobs() override;

	virtual void RT_DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround) = 0;

	virtual void RT_PresentFast() = 0;

	virtual int  RT_CurThreadList() override;
	virtual void RT_BeginFrame() = 0;
	virtual void RT_EndFrame() = 0;
	virtual void RT_ForceSwapBuffers() = 0;
	virtual void RT_SwitchToNativeResolutionBackbuffer(bool resolveBackBuffer) = 0;
	virtual void RT_Init() = 0;
	virtual void RT_ShutDown(uint32 nFlags) = 0;
	virtual bool RT_CreateDevice() = 0;
	virtual void RT_Reset() = 0;
	virtual void RT_SetCull(int nMode) = 0;
	virtual void RT_SetScissor(bool bEnable, int x, int y, int width, int height) = 0;
	virtual void RT_SelectGPU(int nGPU) = 0;
	virtual void RT_RenderScene(CRenderView* pRenderView, int nFlags, SThreadInfo& TI, RenderFunc pRenderFunc) = 0;
	virtual void RT_PrepareStereo(int mode, int output) = 0;
	virtual void RT_SetCameraInfo() = 0;
	virtual void RT_CreateResource(SResourceAsync* pRes) = 0;
	virtual void RT_ReleaseResource(SResourceAsync* pRes) = 0;
	virtual void RT_ReleaseRenderResources(uint32 nFlags) = 0;
	virtual void RT_ReleaseOptics(IOpticsElementBase* pOpticsElement) = 0;
	virtual void RT_UnbindResources() = 0;
	virtual void RT_UnbindTMUs() = 0;
	virtual void RT_CreateRenderResources() = 0;
	virtual void RT_PrecacheDefaultShaders() = 0;
	virtual void RT_ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX, int nScaledY) = 0;
	virtual void RT_FlashRender(IFlashPlayer_RenderProxy* pPlayer, bool stereo) override;
	virtual void RT_FlashRenderPlaybackLockless(IFlashPlayer_RenderProxy* pPlayer, int cbIdx, bool stereo, bool finalPlayback) override;
	virtual void RT_FlashRemoveTexture(ITexture* pTexture) override;
	virtual void RT_ReleaseVBStream(void* pVB, int nStream) = 0;
	virtual void RT_ReleaseCB(void* pCB) = 0;
	virtual void RT_ReleaseRS(std::shared_ptr<CDeviceResourceSet>& pRS) = 0;
	virtual void RT_DrawDynVB(SVF_P3F_C4B_T2F* pBuf, uint16* pInds, uint32 nVerts, uint32 nInds, const PublicRenderPrimitiveType nPrimType) = 0;
	virtual void RT_Draw2dImage(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, float angle, DWORD col, float z) = 0;
	virtual void RT_Draw2dImageStretchMode(bool bStretch) = 0;
	virtual void RT_Push2dImage(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, float angle, DWORD col, float z, float stereoDepth) = 0;
	virtual void RT_PushUITexture(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, CTexture* pTarget, DWORD col) = 0;
	virtual void RT_Draw2dImageList() = 0;
	virtual void RT_DrawImageWithUV(float xpos, float ypos, float z, float w, float h, int texture_id, float* s, float* t, DWORD col, bool filtered = true) = 0;
	virtual void RT_PushRenderTarget(int nTarget, CTexture* pTex, SDepthTexture* pDS, int nS) = 0;
	virtual void RT_PopRenderTarget(int nTarget) = 0;
	virtual void RT_SetViewport(int x, int y, int width, int height, int id = 0) = 0;
	virtual void RT_ClearTarget(CTexture* pTex, const ColorF& color) = 0;
	virtual void RT_RenderDebug(bool bRenderStats = true) = 0;
	virtual void RT_SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode = false) = 0;
	void         RT_PrepareLevelTexStreaming();
	void         RT_PostLevelLoading();
	void         RT_DisableTemporalEffects();

	virtual void FlashRenderInternal(IFlashPlayer_RenderProxy* pPlayer, bool stereo, bool doRealRender) = 0;
	virtual void FlashRenderPlaybackLocklessInternal(IFlashPlayer_RenderProxy* pPlayer, int cbIdx, bool stereo, bool finalPlayback, bool doRealRender) = 0;
	virtual bool FlushRTCommands(bool bWait, bool bImmediatelly, bool bForce) override;
	virtual bool ForceFlushRTCommands();
	virtual void WaitForParticleBuffer() = 0;

	virtual void RequestFlushAllPendingTextureStreamingJobs(int nFrames) override { m_nFlushAllPendingTextureStreamingJobs = nFrames; }
	virtual void SetTexturesStreamingGlobalMipFactor(float fFactor) override      { m_fTexturesStreamingGlobalMipFactor = fFactor; }

	virtual void SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode = false) override = 0;

	virtual void EF_ClearTargetsImmediately(uint32 nFlags) = 0;
	virtual void EF_ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors, float fDepth, uint8 nStencil) = 0;
	virtual void EF_ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors) = 0;
	virtual void EF_ClearTargetsImmediately(uint32 nFlags, float fDepth, uint8 nStencil) = 0;

	virtual void EF_ClearTargetsLater(uint32 nFlags) = 0;
	virtual void EF_ClearTargetsLater(uint32 nFlags, const ColorF& Colors, float fDepth, uint8 nStencil) = 0;
	virtual void EF_ClearTargetsLater(uint32 nFlags, const ColorF& Colors) = 0;
	virtual void EF_ClearTargetsLater(uint32 nFlags, float fDepth, uint8 nStencil) = 0;

	//===============================================================================

	virtual float*      PinOcclusionBuffer(Matrix44A& camera) override = 0;
	virtual void        UnpinOcclusionBuffer() override = 0;

	virtual void        AddListener(IRendererEventListener* pRendererEventListener) override;
	virtual void        RemoveListener(IRendererEventListener* pRendererEventListener) override;

	virtual ERenderType GetRenderType() const override;

	virtual WIN_HWND    Init(int x, int y, int width, int height, unsigned int cbpp, int zbpp, int sbits, bool fullscreen, WIN_HWND Glhwnd = 0, bool bReInit = false, const SCustomRenderInitArgs* pCustomArgs = 0, bool bShaderCacheGen = false) override = 0;

	virtual WIN_HWND    GetCurrentContextHWND() override  { return GetHWND(); }
	virtual bool        IsCurrentContextMainVP() override { return true; }

	virtual int         CreateRenderTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF = eTF_R8G8B8A8) override = 0;
	virtual bool        DestroyRenderTarget(int nHandle) override = 0;
	virtual bool        SetRenderTarget(int nHandle, int nFlags = 0) override = 0;

	virtual int         GetFeatures() override { return m_Features; }

	virtual int         GetNumGeomInstances() override;
	;

	virtual int GetNumGeomInstanceDrawCalls() override;
	;

	virtual int  GetCurrentNumberOfDrawCalls() override;

	virtual void GetCurrentNumberOfDrawCalls(int& nGeneral, int& nShadowGen) override;

	virtual int  GetCurrentNumberOfDrawCalls(const uint32 EFSListMask) override;

	virtual void SetDebugRenderNode(IRenderNode* pRenderNode) override;

	virtual bool IsDebugRenderNode(IRenderNode* pRenderNode) const override;

	//! Fills array of all supported video formats (except low resolution formats)
	//! Returns number of formats, also when called with NULL
	virtual int EnumDisplayFormats(SDispFormat* formats) override = 0;

	//! Return all supported by video card video AA formats
	virtual int EnumAAFormats(SAAFormat* formats) override = 0;

	//! Changes resolution of the window/device (doen't require to reload the level
	virtual bool               ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen, bool bForceReset) override = 0;
	virtual bool               CheckDeviceLost() { return false; };

	virtual EScreenAspectRatio GetScreenAspect(int nWidth, int nHeight) override;

	virtual Vec2               SetViewportDownscale(float xscale, float yscale) override;

	virtual void               Release() override;
	virtual void               FreeResources(int nFlags) override;
	virtual void               InitSystemResources(int nFlags) override;

	virtual void               BeginFrame() override = 0;
	virtual void               RenderDebug(bool bRenderStats = true) override = 0;
	virtual void               EndFrame() override = 0;
	virtual void               LimitFramerate(const int maxFPS, const bool bUseSleep) = 0;

	virtual void               ForceSwapBuffers() override;

	virtual void               TryFlush() override = 0;

	virtual void               Reset(void) = 0;

	virtual void               SetCamera(const CCamera& cam) override = 0;
	virtual void               SetViewport(int x, int y, int width, int height, int id = 0) override = 0;
	virtual void               SetScissor(int x = 0, int y = 0, int width = 0, int height = 0) override = 0;
	virtual void               GetViewport(int* x, int* y, int* width, int* height) override = 0;
	float                      GetDrawNearestFOV() const { return m_drawNearFov; }

	virtual void               SetState(int State, int AlphaRef = -1) override;

	virtual void               PushWireframeMode(int mode) override;
	virtual void               PopWireframeMode() override;

	virtual void               FX_PushWireframeMode(int mode) = 0;
	virtual void               FX_PopWireframeMode() = 0;

	virtual void               SetCullMode(int mode = R_CULL_BACK) override = 0;
	virtual void               EnableVSync(bool enable) override = 0;

	virtual void               DrawPrimitivesInternal(CVertexBuffer* src, int vert_num, const ERenderPrimitiveType prim_type) = 0;

	virtual void               PushMatrix() override = 0;
	virtual void               PopMatrix() override = 0;

	virtual bool               ChangeDisplay(unsigned int width, unsigned int height, unsigned int cbpp) override = 0;
	virtual void               ChangeViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) override = 0;

	virtual bool               SaveTga(unsigned char* sourcedata, int sourceformat, int w, int h, const char* filename, bool flip) const override;

	//download an image to video memory. 0 in case of failure
	virtual unsigned int DownLoadToVideoMemory(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override = 0;
	virtual void         UpdateTextureInVideoMemory(uint32 tnum, unsigned char* newdata, int posx, int posy, int w, int h, ETEX_Format eTFSrc = eTF_R8G8B8A8, int posz = 0, int sizez = 1) override = 0;
	virtual unsigned int DownLoadToVideoMemory3D(unsigned char* data, int w, int h, int d, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override = 0;
	virtual unsigned int DownLoadToVideoMemoryCube(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override = 0;

	virtual bool         DXTCompress(byte* raw_data, int nWidth, int nHeight, ETEX_Format eTF, bool bUseHW, bool bGenMips, int nSrcBytesPerPix, MIPDXTcallback callback) override;
	virtual bool         DXTDecompress(byte* srcData, const size_t srcFileSize, byte* dstData, int nWidth, int nHeight, int nMips, ETEX_Format eSrcTF, bool bUseHW, int nDstBytesPerPix) override;

	virtual bool         SetGammaDelta(const float fGamma) override = 0;

	virtual void         RemoveTexture(unsigned int TextureId) override = 0;

	virtual void         SetTexture(int tnum) override;
	virtual void         SetWhiteTexture() override;
	virtual int          GetWhiteTextureId() const override;

	CTextureManager*     GetTextureManager() { return m_pTextureManager; }

	// =======================================================================================
	// = Functions which draw directly into the swap-chain's backbuffer ======================

	void         Draw2dTextWithDepth(float posX, float posY, float posZ, const char* pStr, const SDrawTextInfo& ti);

	virtual void Draw2dImage(float xpos, float ypos, float w, float h, int texture_id, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 1, float angle = 0,
	                         float r = 1, float g = 1, float b = 1, float a = 1, float z = 1) override = 0;
	virtual void Push2dImage(float xpos, float ypos, float w, float h, int texture_id, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 1, float angle = 0,
	                         float r = 1, float g = 1, float b = 1, float a = 1, float z = 1, float stereoDepth = 0) override = 0;
	virtual void Draw2dImageList() override = 0;
	// =======================================================================================

	virtual void         ResetToDefault() override = 0;

	virtual void         PrintResourcesLeaks() = 0;

	inline float         ScaleCoordXInternal(float value, const SViewport& vp) const        { value *= float(vp.nWidth) / 800.0f; return (value); }
	inline float         ScaleCoordYInternal(float value, const SViewport& vp) const        { value *= float(vp.nHeight) / 600.0f; return (value); }
	inline void          ScaleCoordInternal(float& x, float& y, const SViewport& vp) const  { x = ScaleCoordXInternal(x, vp); y = ScaleCoordYInternal(y, vp); }

	virtual float        ScaleCoordX(float value) const override       { return ScaleCoordXInternal(value, m_CurViewport); }
	virtual float        ScaleCoordY(float value) const override       { return ScaleCoordYInternal(value, m_CurViewport); }
	virtual void         ScaleCoord(float& x, float& y) const override { ScaleCoordInternal(x, y, m_CurViewport); }

	void                 SetWidth(int nW)                              { m_width = nW; }
	void                 SetHeight(int nH)                             { m_height = nH; }
	void                 SetPixelAspectRatio(float fPAR)               { m_pixelAspectRatio = fPAR; }
	
	virtual int          GetWidth() override                           { return (m_width); }
	virtual int          GetHeight() override                          { return (m_height); }
	virtual float        GetPixelAspectRatio() const override          { return (m_pixelAspectRatio); }

	virtual int GetOverlayWidth() override { return IsStereoEnabled() ? m_width : m_nativeWidth; }
	virtual int GetOverlayHeight() override { return IsStereoEnabled() ? m_height : m_nativeHeight; }

	virtual bool         IsStereoEnabled() const override              { return false; }

	virtual float        GetNearestRangeMax() const override           { return (CV_r_DrawNearZRange); }

	virtual int          GetWireframeMode()                            { return(m_wireframe_mode); }

	virtual CRenderView* GetRenderViewForThread(int nThreadID, IRenderView::EViewType Type = IRenderView::eViewType_Default) final;

	Matrix44A            GetCameraMatrix();
	// Get camera matrix from the previous frame.
	const Matrix44A& GetPreviousFrameCameraMatrix() const;
	void             SetPreviousFrameCameraMatrix(const Matrix44A& m);

	void             GetPolyCount(int& nPolygons, int& nShadowPolys) override;

	int              GetPolyCount() override;
	int              RT_GetPolyCount();

	virtual void     SetMaterialColor(float r, float g, float b, float a) override = 0;

	virtual bool     WriteDDS(byte* dat, int wdt, int hgt, int Size, const char* name, ETEX_Format eF, int NumMips) override;
	virtual bool     WriteTGA(byte* dat, int wdt, int hgt, const char* name, int src_bits_per_pixel, int dest_bits_per_pixel) override;
	virtual bool     WriteJPG(byte* dat, int wdt, int hgt, char* name, int src_bits_per_pixel, int nQuality = 100) override;

	virtual void     GetMemoryUsage(ICrySizer* Sizer) override;

	virtual void     GetBandwidthStats(float* fBandwidthRequested) override;

	virtual void     SetTextureStreamListener(ITextureStreamListener* pListener) override;

#if defined(CRY_ENABLE_RC_HELPER)
	virtual void AddAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener);
	virtual void RemoveAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener);
#endif

	virtual void GetLogVBuffers() = 0;

	virtual int  GetFrameID(bool bIncludeRecursiveCalls = true) override;

	// GPU being updated
	int32 RT_GetCurrGpuID() const;

	// Project/UnProject.  Returns true if successful.
	virtual bool ProjectToScreen(float ptx, float pty, float ptz, float* sx, float* sy, float* sz) override = 0;
	virtual int  UnProject(float sx, float sy, float sz,
	                       float* px, float* py, float* pz,
	                       const float modelMatrix[16],
	                       const float projMatrix[16],
	                       const int viewport[4]) override = 0;
	virtual int  UnProjectFromScreen(float sx, float sy, float sz, float* px, float* py, float* pz) override = 0;

	// Shadow Mapping
	virtual void OnEntityDeleted(IRenderNode* pRenderNode) override = 0;

	virtual void SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa) override = 0;

	//for editor
	virtual void GetModelViewMatrix(float* mat) override = 0;
	virtual void GetProjectionMatrix(float* mat) override = 0;
	virtual void GetCameraZeroMatrix(float* mat) override = 0;
	virtual void SetMatrices(float* pProjMat, float* pViewMat, float* pZeroMat) = 0;

	virtual void SetHighlightColor(ColorF color) override { m_highlightColor = color; }
	virtual void SetSelectionColor(ColorF color) override { m_SelectionColor = color; }
	virtual void SetHighlightParams(float outlineThickness, float fGhostAlpha) override
	{ 
		m_highlightParams = Vec4(outlineThickness, fGhostAlpha, 0.0f, 0.0f); 
	}

	ColorF& GetHighlightColor()         { return m_highlightColor; }
	ColorF& GetSelectionColor()         { return m_SelectionColor; }
	Vec4&   GetHighlightParams()        { return m_highlightParams; }

	// NOTE: deprecated
	virtual void ClearTargetsImmediately(uint32 nFlags) override = 0;
	virtual void ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors, float fDepth) override = 0;
	virtual void ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors) override = 0;
	virtual void ClearTargetsImmediately(uint32 nFlags, float fDepth) override = 0;

	virtual void ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX = -1, int nScaledY = -1) override = 0;

	//misc
	virtual bool                ScreenShot(const char* filename = NULL, int width = 0, EScreenShotMode eScreenShotMode = eScreenShotMode_Normal) override = 0;

	virtual int                 GetColorBpp() override   { return m_cbpp; }
	virtual int                 GetDepthBpp() override   { return m_zbpp; }
	virtual int                 GetStencilBpp() override { return m_sbpp; }

	virtual void                Set2DMode(bool enable, int ortox, int ortoy, float znear = -1e10f, float zfar = 1e10f) override = 0;

	virtual int                 ScreenToTexture(int nTexID) override = 0;

	virtual void                LockParticleVideoMemory() override                            {}
	virtual void                UnLockParticleVideoMemory() override                          {}

	virtual void                ActivateLayer(const char* pLayerName, bool activate) override {}

	virtual void                FlushPendingTextureTasks() override;

	virtual void                SetCloakParams(const SRendererCloakParams& cloakParams) override;
	virtual float               GetCloakFadeLightScale() const override;
	virtual void                SetCloakFadeLightScale(float fColorScale) override;
	virtual void                SetShadowJittering(float shadowJittering) override;
	virtual float               GetShadowJittering() const override;

	virtual void                DrawObjSprites(PodArray<struct SVegetationSpriteInfo>* pList)                                         {};
	virtual void                GenerateObjSprites(PodArray<struct SVegetationSpriteInfo>* pList, const SRenderingPassInfo& passInfo) {};

	void                        EF_AddClientPolys(const SRenderingPassInfo& passInfo);

	bool                        FX_TryToMerge(CRenderObject* pNewObject, CRenderObject* pOldObject, CRenderElement* pRE, bool bResIdentical);
	virtual void*               FX_AllocateCharInstCB(SSkinningData*, uint32) { return NULL; }
	virtual void                FX_ClearCharInstCB(uint32)                    {}

	virtual EShaderQuality      EF_GetShaderQuality(EShaderType eST) override;
	virtual ERenderQuality      EF_GetRenderQuality() const override;

	virtual void                EF_SubmitWind(const SWindGrid* pWind) override;
	void                        RT_SubmitWind(const SWindGrid* pWind);

	void                        RefreshSystemShaders();
	uint32                      EF_BatchFlags(SShaderItem& SH, CRenderObject* pObj, CRenderElement* re, const SRenderingPassInfo& passInfo, int nAboveWater);

	virtual float               EF_GetWaterZElevation(float fX, float fY) override;
	virtual void                FX_PipelineShutdown(bool bFastShutdown = false) = 0;
	virtual void                RT_GraphicsPipelineShutdown() = 0;
	virtual void                RT_ResetDeviceObjectFactory() = 0;

	virtual IOpticsElementBase* CreateOptics(EFlareType type) const override;
	void                        ReleaseOptics(IOpticsElementBase* pOpticsElement) const override;

	virtual bool                EF_PrecacheResource(IShader* pSH, float fMipFactor, float fTimeToReady, int Flags) override;
	virtual bool                EF_PrecacheResource(ITexture* pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter) override = 0;
	virtual bool                EF_PrecacheResource(IRenderMesh* pPB, IMaterial* pMaterial, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId) override;
	virtual bool                EF_PrecacheResource(CDLight* pLS, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId) override;

	void                        FX_CheckOverflow(int nVerts, int nInds, CRenderElement* re, int* nNewVerts = NULL, int* nNewInds = NULL);
	void                        FX_Start(CShader* ef, int nTech, CShaderResources* Res, CRenderElement* re);

	// functions for handling particle jobs which cull particles and generate their vertices/indices
	virtual void EF_AddMultipleParticlesToScene(const SAddParticlesToSceneJob* jobs, size_t numJobs, const SRenderingPassInfo& passInfo) override;

	//==========================================================
	// external interface for shaders
	//==========================================================
	// Shaders management
	virtual void        EF_SetShaderMissCallback(ShaderCacheMissCallback callback) override;
	virtual const char* EF_GetShaderMissLogPath() override;
	virtual string*     EF_GetShaderNames(int& nNumShaders) override;
	virtual IShader*    EF_LoadShader(const char* name, int flags = 0, uint64 nMaskGen = 0) override;
	virtual SShaderItem EF_LoadShaderItem(const char* name, bool bShare, int flags = 0, SInputShaderResources* Res = NULL, uint64 nMaskGen = 0, const SLoadShaderItemArgs* pArgs = 0) override;
	virtual uint64      EF_GetRemapedShaderMaskGen(const char* name, uint64 nMaskGen = 0, bool bFixup = 0) override;

	virtual uint64      EF_GetShaderGlobalMaskGenFromString(const char* szShaderName, const char* szShaderGen, uint64 nMaskGen = 0) override;
	virtual const char* EF_GetStringFromShaderGlobalMaskGen(const char* szShaderName, uint64 nMaskGen = 0) override;

	// reload file
	virtual bool                   EF_ReloadFile(const char* szFileName) override;
	virtual bool                   EF_ReloadFile_Request(const char* szFileName) override;
	virtual void                   EF_ReloadShaderFiles(int nCategory) override;
	virtual void                   EF_ReloadTextures() override;
	virtual int                    EF_LoadLightmap(const char* nameTex) override;
	virtual bool                   EF_RenderEnvironmentCubeHDR(int size, Vec3& Pos, TArray<unsigned short>& vecData) override;
	virtual bool                   WriteTIFToDisk(const void* pData, int width, int height, int bytesPerChannel, int numChannels, bool bFloat, const char* szPreset, const char* szFileName) override;
	virtual ITexture*              EF_GetTextureByID(int Id) override;
	virtual ITexture*              EF_GetTextureByName(const char* name, uint32 flags = 0) override;
	virtual ITexture*              EF_LoadTexture(const char* nameTex, const uint32 flags = 0) override;
	virtual IDynTextureSource*     EF_LoadDynTexture(const char* dynsourceName, bool sharedRT = false) override;
	virtual const SShaderProfile&  GetShaderProfile(EShaderType eST) const override;
	virtual void                   EF_SetShaderQuality(EShaderType eST, EShaderQuality eSQ) override;

	virtual _smart_ptr<IImageFile> EF_LoadImage(const char* szFileName, uint32 nFlags) override;

	// Create new RE of type (edt)
	virtual CRenderElement*          EF_CreateRE(EDataType edt) override;

	// Begin using shaders
	virtual void EF_StartEf(const SRenderingPassInfo& passInfo) override;

	// Get Object for RE transformation
	virtual CRenderObject* EF_GetObject_Temp(int nThreadID) final;
	virtual CRenderObject* EF_DuplicateRO(CRenderObject* pObj, const SRenderingPassInfo& passInfo) final;
	virtual CRenderObject* EF_GetObject() final;
	virtual void           EF_FreeObject(CRenderObject* pObj) final;

	// Add shader to the list (virtual)
	virtual void EF_AddEf(CRenderElement* pRE, SShaderItem& pSH, CRenderObject* pObj, const SRenderingPassInfo& passInfo, int nList, int nAW) override;

	// Draw all shaded REs in the list
	virtual void EF_EndEf3D(const int nFlags, const int nPrecacheUpdateId, const int nNearPrecacheUpdateId, const SRenderingPassInfo& passInfo) override = 0;

	virtual void EF_InvokeShadowMapRenderJobs(CRenderView* pRenderView, const int nFlags) override {}
	// 2d interface for shaders
	virtual void EF_EndEf2D(const bool bSort) override = 0;

	// Dynamic lights
	virtual bool                   EF_IsFakeDLight(const CDLight* Source) override;
	virtual void                   EF_ADDDlight(CDLight* Source, const SRenderingPassInfo& passInfo) override;
	virtual bool                   EF_AddDeferredDecal(const SDeferredDecal& rDecal, const SRenderingPassInfo& passInfo) override;

	virtual int                    EF_AddDeferredLight(const CDLight& pLight, float fMult, const SRenderingPassInfo& passInfo) override;

	virtual void                   EF_ReleaseDeferredData() override;
	virtual SInputShaderResources* EF_CreateInputShaderResource(IRenderShaderResources* pOptionalCopyFrom = nullptr) override;
	virtual void                   ClearPerFrameData(const SRenderingPassInfo& passInfo);
	virtual bool                   EF_UpdateDLight(SRenderLight* pDL) override;
	void                           EF_CheckLightMaterial(CDLight* pLight, uint16 nRenderLightID, const SRenderingPassInfo& passInfo);

	virtual void EF_QueryImpl(ERenderQueryTypes eQuery, void* pInOut0, uint32 nInOutSize0, void* pInOut1, uint32 nInOutSize1) override;
	virtual void FX_SetState(int st, int AlphaRef = -1, int RestoreState = 0) = 0;
	void         FX_SetStencilState(int st, uint32 nStencRef, uint32 nStencMask, uint32 nStencWriteMask, bool bForceFullReadMask = 0);

	//////////////////////////////////////////////////////////////////////////
	// Deferred ambient passes
	virtual void Ef_AddDeferredGIClipVolume(const IRenderMesh* pClipVolume, const Matrix34& mxTransform) override;

	//////////////////////////////////////////////////////////////////////////
	// Post processing effects interfaces

	virtual void  EF_SetPostEffectParam(const char* pParam, float fValue, bool bForceValue = false) override;
	virtual void  EF_SetPostEffectParamVec4(const char* pParam, const Vec4& pValue, bool bForceValue = false) override;
	virtual void  EF_SetPostEffectParamString(const char* pParam, const char* pszArg) override;

	virtual void  EF_GetPostEffectParam(const char* pParam, float& fValue) override;
	virtual void  EF_GetPostEffectParamVec4(const char* pParam, Vec4& pValue) override;
	virtual void  EF_GetPostEffectParamString(const char* pParam, const char*& pszArg) override;

	virtual int32 EF_GetPostEffectID(const char* pPostEffectName) override;

	virtual void  EF_ResetPostEffects(bool bOnSpecChange = false) override;

	virtual void  EF_DisableTemporalEffects() override;

	virtual void  ForceGC() override;

	virtual void  RT_ResetGlass() {}


	// create/delete RenderMesh object
	virtual _smart_ptr<IRenderMesh> CreateRenderMesh(
	  const char* szType
	  , const char* szSourceName
	  , IRenderMesh::SInitParamerers* pInitParams = NULL
	  , ERenderMeshType eBufType = eRMT_Static
	  ) override;

	virtual _smart_ptr<IRenderMesh> CreateRenderMeshInitialized(
	  const void* pVertBuffer, int nVertCount, InputLayoutHandle eVF,
	  const vtx_idx* pIndices, int nIndices,
	  const PublicRenderPrimitiveType nPrimetiveType, const char* szType, const char* szSourceName, ERenderMeshType eBufType = eRMT_Static,
	  int nMatInfoCount = 1, int nClientTextureBindID = 0,
	  bool (* PrepareBufferCallback)(IRenderMesh*, bool) = NULL,
	  void* CustomData = NULL,
	  bool bOnlyVideoBuffer = false, bool bPrecache = true, const SPipTangents* pTangents = NULL, bool bLockForThreadAcc = false, Vec3* pNormals = NULL) override;

	virtual int GetMaxActiveTexturesARB() { return 0; }

	//////////////////////////////////////////////////////////////////////
	// Replacement functions for the Font engine ( vlad: for font can be used old functions )
	virtual bool FontUploadTexture(class CFBitmap*, ETEX_Format eSrcFormat = eTF_R8G8B8A8) override = 0;
	virtual int  FontCreateTexture(int Width, int Height, byte* pSrcData, ETEX_Format eSrcFormat = eTF_R8G8B8A8, bool genMips = false) override = 0;
	virtual bool FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte* pSrcData) override = 0;
	virtual void FontReleaseTexture(class CFBitmap* pBmp) override = 0;
	virtual void FontSetTexture(class CFBitmap*, int nFilterMode) override = 0;
	virtual void FontSetTexture(int nTexId, int nFilterMode) override = 0;
	virtual void FontSetRenderingState(unsigned int nVirtualScreenWidth, unsigned int nVirtualScreenHeight) override = 0;
	virtual void FontSetBlending(int src, int dst) override = 0;
	virtual void FontRestoreRenderingState() override = 0;

	//////////////////////////////////////////////////////////////////////
	// Used for pausing timer related stuff (eg: for texture animations, and shader 'time' parameter)
	void                         PauseTimer(bool bPause) override { m_bPauseTimer = bPause; }
	virtual IShaderPublicParams* CreateShaderPublicParams() override;

	virtual void                 SetLevelLoadingThreadId(threadID threadId) override;
	virtual void                 GetThreadIDs(threadID& mainThreadID, threadID& renderThreadID) const override;

#if RENDERER_SUPPORT_SCALEFORM
	void SF_ConfigMask(int st, uint32 ref);
	virtual int SF_CreateTexture(int width, int height, int numMips, const unsigned char* pSrcData, ETEX_Format eSrcFormat, int flags) override;
	virtual void SF_GetMeshMaxSize(int& numVertices, int& numIndices) const override;

	virtual IScaleformPlayback* SF_CreatePlayback() const override;
	virtual void SF_Playback(IScaleformPlayback* pRenderer, GRendererCommandBufferReadOnly* pBuffer) const override;
	virtual void SF_Drain(GRendererCommandBufferReadOnly* pBuffer) const override;
#else // #if RENDERER_SUPPORT_SCALEFORM
	// These dummy functions are required when the feature is disabled, do not remove without testing the RENDERER_SUPPORT_SCALEFORM=0 case!
	virtual int SF_CreateTexture(int width, int height, int numMips, const unsigned char* pSrcData, ETEX_Format eSrcFormat, int flags) override { return 0; }
	virtual void SF_GetMeshMaxSize(int& numVertices, int& numIndices) const override { numVertices = 0; numIndices = 0; }
	virtual IScaleformPlayback* SF_CreatePlayback() const override;
	virtual void SF_Playback(IScaleformPlayback* pRenderer, GRendererCommandBufferReadOnly* pBuffer) const override {}
	virtual void SF_Drain(GRendererCommandBufferReadOnly* pBuffer) const override {}
#endif // #if RENDERER_SUPPORT_SCALEFORM

	virtual ITexture* CreateTexture(const char* name, int width, int height, int numMips, unsigned char* pSrcData, ETEX_Format eSrcFormat, int flags) override;
	virtual ITexture* CreateTextureArray(const char* name, ETEX_Type eType, uint32 nWidth, uint32 nHeight, uint32 nArraySize, int nMips, uint32 nFlags, ETEX_Format eSrcFormat, int nCustomID) override;

	enum ESPM {ESPM_PUSH = 0, ESPM_POP = 1};
	virtual void   SetProfileMarker(const char* label, ESPM mode) const                              {};

	virtual uint16 PushFogVolumeContribution(const ColorF& fogVolumeContrib, const SRenderingPassInfo& passInfo) override;
	void           GetFogVolumeContribution(uint16 idx, ColorF& rColor) const;

	virtual int    GetMaxTextureSize() override                                                               { return m_MaxTextureSize; }

	virtual void   SetCloudShadowsParams(int nTexID, const Vec3& speed, float tiling, bool invert, float brightness) override;
	int            GetCloudShadowTextureId() const { return m_cloudShadowTexId; }
	bool GetCloudShadowsEnabled() const;

	virtual void                                      SetVolumetricCloudParams(int nTexID) override;
	virtual void                                      SetVolumetricCloudNoiseTex(int cloudNoiseTexId, int edgeNoiseTexId) override;
	void                                              GetVolumetricCloudTextureInfo(SVolumetricCloudTexInfo& info) const;

	virtual ShadowFrustumMGPUCache*                   GetShadowFrustumMGPUCache() override                                                          { return &m_ShadowFrustumMGPUCache; }

	virtual const StaticArray<int, MAX_GSM_LODS_NUM>& GetCachedShadowsResolution() const override                                                   { return m_CachedShadowsResolution; }
	virtual void                                      SetCachedShadowsResolution(const StaticArray<int, MAX_GSM_LODS_NUM>& arrResolutions) override { m_CachedShadowsResolution = arrResolutions; }
	virtual void                                      UpdateCachedShadowsLodCount(int nGsmLods) const override;

	virtual bool                                      IsPost3DRendererEnabled() const override;

	virtual void                                      ExecuteAsyncDIP() override;

	alloc_info_struct*                                GetFreeChunk(int bytes_count, int nBufSize, PodArray<alloc_info_struct>& alloc_info, const char* szSource);
	bool                                              ReleaseChunk(int p, PodArray<alloc_info_struct>& alloc_info);

	virtual const char*                               GetTextureFormatName(ETEX_Format eTF) override;
	virtual int                                       GetTextureFormatDataSize(int nWidth, int nHeight, int nDepth, int nMips, ETEX_Format eTF) override;
	virtual void                                      SetDefaultMaterials(IMaterial* pDefMat, IMaterial* pTerrainDefMat) override                          { m_pDefaultMaterial = pDefMat; m_pTerrainDefaultMaterial = pTerrainDefMat; }
	virtual byte*                                     GetTextureSubImageData32(byte* pData, int nDataSize, int nX, int nY, int nW, int nH, CTexture* pTex) { return 0; }

	virtual void                                      PrecacheTexture(ITexture* pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter = 1);

	virtual SSkinningData*                            EF_CreateSkinningData(uint32 nNumBones, bool bNeedJobSyncVar) override;
	virtual SSkinningData*                            EF_CreateRemappedSkinningData(uint32 nNumBones, SSkinningData* pSourceSkinningData, uint32 nCustomDataSize, uint32 pairGuid) override;
	virtual void                                      EF_EnqueueComputeSkinningData(SSkinningData* pData) override;
	virtual int                                       EF_GetSkinningPoolID() override;
	void                                              ClearSkinningDataPool();

	virtual void                                      UpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial) override;
	virtual void                                      ForceUpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial) override;
	virtual void                                      RefreshShaderResourceConstants(SShaderItem* pShaderItem, IMaterial* pMaterial) override;

	void                                              RT_UpdateShaderItem(SShaderItem* pShaderItem);
	void                                              RT_RefreshShaderResourceConstants(SShaderItem* pShaderItem);

	virtual bool                                      LoadShaderStartupCache() override;

	virtual void                                      UnloadShaderStartupCache() override;

	virtual bool                                      LoadShaderLevelCache() override            { return false; }
	virtual void                                      UnloadShaderLevelCache() override          {}

	virtual void                                      SetClearColor(const Vec3& vColor) override { m_cClearColor.r = vColor[0]; m_cClearColor.g = vColor[1]; m_cClearColor.b = vColor[2]; }

	virtual void                                      RegisterSyncWithMainListener(ISyncMainWithRenderListener* pListener) override;
	virtual void                                      RemoveSyncWithMainListener(const ISyncMainWithRenderListener* pListener) override;

	virtual void                                      SetCurDownscaleFactor(Vec2 sf) = 0;

	virtual IGraphicsDeviceConstantBufferPtr          CreateGraphiceDeviceConstantBuffer() override                                              { assert(0);  return 0; };

	virtual void                                      MakeMatrix(const Vec3& pos, const Vec3& angles, const Vec3& scale, Matrix34* mat) override { assert(0); }

public:
	//////////////////////////////////////////////////////////////////////////
	// PUBLIC HELPER METHODS
	//////////////////////////////////////////////////////////////////////////
	void   SyncMainWithRender();

	void   UpdateConstParamsPF(const SRenderingPassInfo& passInfo, bool bSecondaryViewport);

	void*  EF_GetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags);
	uint32 GetActiveGPUCount() const override { return CV_r_multigpu > 0 ? m_nGPUs : 1; }

	void   Logv(const char* format, ...);
	void   LogStrv(char* format, ...);
	void   LogShv(char* format, ...);
	void   Log(char* str);

	void   RT_UpdateLightVolumes();
	void RT_SetSkinningPoolId(uint32);

	void RT_SetLightVolumeShaderFlags();

	// Add shader to the list
	void              EF_AddEf_NotVirtual(CRenderElement* pRE, SShaderItem& pSH, CRenderObject* pObj, const SRenderingPassInfo& passInfo, int nList, int nAW);

	void              EF_TransformDLights();
	void              EF_IdentityDLights();

	void              FX_StartMerging();

	void              EF_PushFog();
	void              EF_PopFog();

	void              FX_PushVP();
	void              FX_PopVP();

	void              EF_AddRTStat(CTexture* pTex, int nFlags = 0, int nW = -1, int nH = -1);
	void              EF_PrintRTStats(const char* szName);

	int               FX_ApplyShadowQuality();

	eAntialiasingType FX_GetAntialiasingType() const;

	uint32            FX_GetMSAAMode() const;
	void              FX_ResetMSAAFlagsRT();
	void              FX_SetMSAAFlagsRT();

	void              FX_ApplyShaderQuality(const EShaderType eST);

	bool              IsHDRModeEnabled() const;

	bool              IsShadowPassEnabled() const;

	void              UpdateRenderingModesInfo();
	bool              IsCustomRenderModeEnabled(uint32 nRenderModeMask);

	const bool        IsEditorMode() const;

	const bool        IsShaderCacheGenMode() const;

	//	Get mipmap distance factor (depends on screen width, screen height and aspect ratio)
	float                GetMipDistFactor()    { return TANGENT30_2 * TANGENT30_2 / (m_height * m_height); }

	const CCamera&       GetCamera(void) final { return(m_RP.m_TI[m_pRT->GetThreadList()].m_cam); }

	const CRenderCamera& GetRCamera(void)      { return(m_RP.m_TI[m_pRT->GetThreadList()].m_rcam); }

	static int           GetTexturesStreamPoolSize();

protected:
	void EF_AddParticle(CREParticle* pParticle, SShaderItem& shaderItem, CRenderObject* pRO, const SRenderingPassInfo& passInfo);
	void EF_RemoveParticlesFromScene();
	void PrepareParticleRenderObjects(Array<const SAddParticlesToSceneJob> aJobs, int nREStart, SRenderingPassInfo passInfo);
	void EF_GetParticleListAndBatchFlags(uint32& nBatchFlags, int& nList, CRenderObject* pRenderObject, const SShaderItem& shaderItem, const SRenderingPassInfo& passInfo);

	void FreePermanentRenderObjects(int bufferId);

public:
	void* operator new(size_t Size)
	{
		void* pPtrRes = CryModuleMemalign(Size, 16);
		memset(pPtrRes, 0, Size);
		return pPtrRes;
	}
	void operator delete(void* Ptr)
	{
		CryModuleMemalignFree(Ptr);
	}

	virtual WIN_HWND GetHWND() override = 0;

	void             SetTextureAlphaChannelFromRGB(byte* pMemBuffer, int nTexSize);

	void             EnableSwapBuffers(bool bEnable) override { m_bSwapBuffers = bEnable; }
	bool m_bSwapBuffers;

	virtual bool StopRendererAtFrameEnd(uint timeoutMilliseconds) override;
	virtual void ResumeRendererFromFrameEnd() override;
	volatile bool m_bStopRendererAtFrameEnd;

	virtual void                   SetTexturePrecaching(bool stat) override;

	virtual const RPProfilerStats* GetRPPStats(ERenderPipelineProfilerStats eStat, bool bCalledFromMainThread = true) override                       { return NULL; }
	virtual const RPProfilerStats* GetRPPStatsArray(bool bCalledFromMainThread = true) override                                                      { return NULL; }

	virtual int                    GetPolygonCountByType(uint32 EFSList, EVertexCostTypes vct, uint32 z, bool bCalledFromMainThread = true) override { return 0; }

	//platform specific
	virtual void  RT_InsertGpuCallback(uint32 context, GpuCallbackFunc callback) override {}
	virtual void  EnablePipelineProfiler(bool bEnable) override = 0;

	virtual float GetGPUFrameTime() override;
	virtual void  GetRenderTimes(SRenderTimes& outTimes) override;
	virtual void  LogShaderImportMiss(const CShader* pShader) {}

#if !defined(_RELEASE)
	//Get debug draw call stats stat
	virtual RNDrawcallsMapMesh& GetDrawCallsInfoPerMesh(bool mainThread = true) override;
	virtual int                 GetDrawCallsPerNode(IRenderNode* pRenderNode) override;

	//Routine to perform an emergency flush of a particular render node from the stats, as not all render node holders are delay-deleted
	virtual void ForceRemoveNodeFromDrawCallsMap(IRenderNode* pNode) override;

	void ClearDrawCallsInfo();
#endif
#ifdef ENABLE_PROFILING_CODE
	void         AddRecordedProfilingStats(const struct SProfilingStats& stats, ERenderListID renderList, bool bScenePass);
#endif

	virtual void                CollectDrawCallsInfo(bool status) override;
	virtual void                CollectDrawCallsInfoPerNode(bool status) override;
	virtual void                EnableLevelUnloading(bool enable) override;
	virtual void                EnableBatchMode(bool enable) override;
	virtual bool                IsStereoModeChangePending() override { return false; }
		
	virtual void QueryActiveGpuInfo(SGpuInfo& info) const override;

	virtual compute_skinning::IComputeSkinningStorage* GetComputeSkinningStorage() = 0;
public:
	Matrix44A m_IdentityMatrix;
	Matrix44A m_ViewMatrix;
	Matrix44A m_CameraMatrix;
	Matrix44A m_CameraZeroMatrix;

	Matrix44A m_ProjMatrix;
	Matrix44A m_TranspOrigCameraProjMatrix;
	Matrix44A m_CameraProjMatrix;
	Matrix44A m_CameraProjZeroMatrix;
	Matrix44A m_InvCameraProjMatrix;
	Matrix44A m_TempMatrices[4][8];

	// todo: generalize for multiple cameras
	Matrix44A      m_CameraMatrixPrev[MAX_NUM_VIEWPORTS][2]; //[RT_COMMAND_BUF_COUNT][2]
	Matrix44A      m_CameraProjMatrixPrev;
	Matrix44A      m_CameraProjMatrixPrevAvg;

	Matrix44A      m_CameraMatrixNearest;               //[RT_COMMAND_BUF_COUNT][2], 16);
	Matrix44A      m_CameraMatrixNearestPrev;           //[RT_COMMAND_BUF_COUNT][2], 16);

	Vec2           m_vProjMatrixSubPixoffset;

	byte           m_bDeviceLost;
	byte           m_bSystemResourcesInit;
	byte           m_bSystemTargetsInit;
	bool           m_bAquireDeviceThread;
	bool           m_bInitialized;

	SRenderThread* m_pRT;

	// Shaders pipeline states
	//=============================================================================================================
	CSubmissionQueue_DX11 m_DevMan;
	CDeviceBufferManager m_DevBufMan;
	SRenderPipeline      m_RP;
	//=============================================================================================================

	CIntroMovieRenderer* m_pIntroMovieRenderer;

	float                m_fTimeWaitForMain[RT_COMMAND_BUF_COUNT];
	float                m_fTimeWaitForRender[RT_COMMAND_BUF_COUNT];
	float                m_fTimeProcessedRT[RT_COMMAND_BUF_COUNT];
	float                m_fTimeProcessedGPU[RT_COMMAND_BUF_COUNT];
	float                m_fTimeWaitForGPU[RT_COMMAND_BUF_COUNT];
	float                m_fTimeGPUIdlePercent[RT_COMMAND_BUF_COUNT];

	float                m_fRTTimeEndFrame;
	float                m_fRTTimeFlashRender;
	float                m_fRTTimeSceneRender;
	float                m_fRTTimeMiscRender;

	int                  m_CurVertBufferSize;
	int                  m_CurIndexBufferSize;

	int                  m_nGPU;
	int                  m_VSync;
	int                  m_Predicated;
	int                  m_MSAA;
	int                  m_MSAA_quality;
	int                  m_MSAA_samples;
	int                  m_deskwidth, m_deskheight;
	int                  m_nHDRType;

	int                  m_nGraphicsPipeline;

	// Index of the current viewport used for rendering.
	int m_CurViewportID;
	// Index of the current eye used for rendering.
	int m_CurRenderEye;

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
	float m_overrideRefreshRate;
	int   m_overrideScanlineOrder;
#endif

#if CRY_PLATFORM_WINDOWS
	int m_prefMonX, m_prefMonY, m_prefMonWidth, m_prefMonHeight;
#endif

	int       m_nStencilMaskRef;

	byte      m_bDeviceSupportsInstancing;

	uint32    m_bDeviceSupports_NVDBT          : 1;
	uint32    m_bDeviceSupportsTessellation    : 1;
	uint32    m_bDeviceSupportsGeometryShaders : 1;

	uint32    m_bEditor                        : 1; // Render instance created from editor
	uint32    m_bShaderCacheGen                : 1; // Render instance create from shader cache gen mode
	uint32    m_bUseHWSkinning                 : 1;
	uint32    m_bShadersPresort                : 1;
	uint32    m_bEndLevelLoading               : 1;
	uint32    m_bLevelUnloading                : 1;
	uint32    m_bStartLevelLoading             : 1;
	uint32    m_bInLevel                       : 1;
	uint32    m_bUseWaterTessHW                : 1;
	uint32    m_bUseSilhouettePOM              : 1;
	uint32    m_bWaterCaustics                 : 1;
	uint32    m_bIsWindowActive                : 1;
	uint32    m_bInShutdown                    : 1;
	uint32    m_bDeferredDecals                : 1;
	uint32    m_bShadowsEnabled                : 1;
	uint32    m_bCloudShadowsEnabled           : 1;
#if defined(VOLUMETRIC_FOG_SHADOWS)
	uint32    m_bVolFogShadowsEnabled          : 1;
	uint32    m_bVolFogCloudShadowsEnabled     : 1;
#endif
	uint32    m_bVolumetricFogEnabled          : 1;
	uint32    m_bVolumetricCloudsEnabled       : 1;
	uint32    m_bDeferredRainEnabled           : 1;
	uint32    m_bDeferredRainOcclusionEnabled  : 1;
	uint32    m_bDeferredSnowEnabled           : 1;

	uint8     m_nDisableTemporalEffects;
	bool      m_bUseGPUFriendlyBatching[2];
	uint32    m_nGPULimited;           // How many frames we are GPU limited
	int8      m_nCurMinAniso;
	int8      m_nCurMaxAniso;
	float     m_fCurMipLodBias;

	uint32    m_nShadowPoolHeight;
	uint32    m_nShadowPoolWidth;

	ColorF    m_CurFontColor;

	SFogState m_FSStack[8];
	int       m_nCurFSStackLevel;

	DWORD     m_Features;
	int       m_MaxTextureSize;
	size_t    m_MaxTextureMemory;
	int       m_nShadowTexSize;

	float     m_fLastGamma;
	float     m_fLastBrightness;
	float     m_fLastContrast;
	float     m_fDeltaGamma;

	float     m_fogCullDistance;

	enum { nMeshPoolMaxTimeoutCounter = 150 /*150 ms*/};
	int m_nMeshPoolTimeoutCounter;

	// Cached verts/inds used for sprites
	SVF_P3F_C4B_T2F* m_pSpriteVerts;
	uint16*          m_pSpriteInds;

	// Custom render modes states
	uint32 m_nThermalVisionMode : 2;
	uint32 m_nSonarVisionMode   : 2;
	uint32 m_nNightVisionMode   : 2;

	int    m_nFlushAllPendingTextureStreamingJobs;
	float  m_fTexturesStreamingGlobalMipFactor;

	SGpuInfo m_adapterInfo = {};
public:
	// these ids can be used for tripple (or more) buffered structures
	// they are incremented in RenderWorld on the mainthread
	// use m_nPoolIndex from the mainthread (or jobs which are synced before Renderworld)
	// and m_nPoolIndexRT from the renderthread
	// right now the skinning data pool and particle are using this id
	uint32  m_nPoolIndex;
	uint32  m_nPoolIndexRT;

	bool    m_bVendorLibInitialized;

	CCamera m_prevCamera;               // camera from previous frame

	uint32  m_nFrameLoad;
	uint32  m_nFrameReset;
	uint32  m_nFrameSwapID;             // without recursive calls, access through GetFrameID(false)

	ColorF  m_cClearColor;
	int     m_NumResourceSlots;
	int     m_NumSamplerSlots;

	////////////////////////////////////////////////////////
	// downscaling viewport information.

	// Set from CrySystem via IRenderer interface
	Vec2 m_ReqViewportScale;

	// Updated in RT_EndFrame. Fixed across the whole frame.
	Vec2 m_CurViewportScale;
	Vec2 m_PrevViewportScale;

	// a RECT that represents the full screen size, accounting for above scaling
	RECT m_FullResRect;
	// same, but half resolution in each direction
	RECT m_HalfResRect;

	////////////////////////////////////////////////////////

	class CPostEffectsMgr* m_pPostProcessMgr;
	class CWater*          m_pWaterSimMgr;

	CTextureManager*       m_pTextureManager;

	// Used for pausing timer related stuff (eg: for texture animations, and shader 'time' parameter)
	bool  m_bPauseTimer;
	float m_fPrevTime;
	uint8 m_nUseZpass : 2;
	bool  m_bCollectDrawCallsInfo;
	bool  m_bCollectDrawCallsInfoPerNode;

	S3DEngineCommon m_p3DEngineCommon;

	typedef std::map<uint64, PodArray<uint16>*> ShadowFrustumListsCache;
	ShadowFrustumListsCache m_FrustumsCache;

	ShadowFrustumMGPUCache  m_ShadowFrustumMGPUCache;

	//Debug Gun
	IRenderNode*     m_pDebugRenderNode;

	const SWindGrid* m_pCurWindGrid;

	//=====================================================================
	// Shaders interface
	CShaderMan            m_cEF;
	_smart_ptr<IMaterial> m_pDefaultMaterial;
	_smart_ptr<IMaterial> m_pTerrainDefaultMaterial;

	int                   m_TexGenID;

	IFFont*               m_pDefaultFont;

	CryCriticalSection    m_renderObjectAccessLock;

	static int            m_iGeomInstancingThreshold; // internal value, auto mapped depending on GPU hardware, 0 means not set yet

	// Limit for local sorting array
	static const int          nMaxParticleContainer = 8 * 1024;

	int                       m_nCREParticleCount[RT_COMMAND_BUF_COUNT];
	JobManager::SJobState     m_ComputeVerticesJobState;

	CFillRateManager          m_FillRateManager;

	FILE*                     m_LogFile;
	FILE*                     m_LogFileStr;
	FILE*                     m_LogFileSh;

	SViewport                 m_MainRTViewport;
	SViewport                 m_MainViewport;
	SViewport                 m_CurViewport;
	SViewport                 m_NewViewport;
	bool                      m_bViewportDirty;
	bool                      m_bViewportDisabled;

protected:
	//================================================================================
	int                                        m_nCurVPStackLevel;
	SViewport                                  m_VPStack[8];

	int                                        m_width, m_height, m_cbpp, m_zbpp, m_sbpp;
	int                                        m_nativeWidth, m_nativeHeight;
	int                                        m_numSSAASamples;
	int                                        m_wireframe_mode, m_wireframe_mode_prev;
	uint32                                     m_nGPUs;                      // Use GetActiveGPUCount() to read
	float                                      m_drawNearFov;
	float                                      m_pixelAspectRatio;
	float                                      m_shadowJittering;
	StaticArray<int, MAX_GSM_LODS_NUM>         m_CachedShadowsResolution;

	CSkinningDataPool                          m_SkinningDataPool[3];        // Tripple Buffered for motion blur
	std::array<std::vector<SSkinningData*>, 3> m_computeSkinningData;
	uint32                                     m_nShadowGenId[RT_COMMAND_BUF_COUNT];

	int                                        m_cloudShadowTexId;
	Vec3                                       m_cloudShadowSpeed;
	float                                      m_cloudShadowTiling;
	bool                                       m_cloudShadowInvert;
	float                                      m_cloudShadowBrightness;
	int                                        m_volumetricCloudTexId;
	int                                        m_volumetricCloudNoiseTexId;
	int                                        m_volumetricCloudEdgeNoiseTexId;

	// Shaders/Shaders support
	// RE - RenderElement
	bool m_bTimeProfileUpdated;
	int  m_PrevProfiler;
	int  m_nCurSlotProfiler;

	int  m_beginFrameCount;

	typedef std::list<IRendererEventListener*> TListRendererEventListeners;
	TListRendererEventListeners               m_listRendererEventListeners;

	SShowRenderTargetInfo                     m_showRenderTargetInfo;

	std::vector<ISyncMainWithRenderListener*> m_syncMainWithRenderListeners;

	CryMutex             m_mtxStopAtRenderFrameEnd;
	CryConditionVariable m_condStopAtRenderFrameEnd;

	ColorF m_highlightColor;
	ColorF m_SelectionColor;
	Vec4  m_highlightParams;
};

//////////////////////////////////////////////////////////////////////////
inline void* CRenderer::EF_GetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags)
{
	void* p;

	if (m_RP.m_pRE)
		p = m_RP.m_pRE->mfGetPointer(ePT, Stride, Type, Dst, Flags);
	else
		p = SRendItem::mfGetPointerCommon(ePT, Stride, Type, Dst, Flags);

	return p;
}

inline int32 CRenderer::RT_GetCurrGpuID() const
{
	return gRenDev->m_nFrameSwapID % gRenDev->GetActiveGPUCount();
}

inline void CRenderer::RT_SetLightVolumeShaderFlags()
{
	const uint64 lightVolumeMask = g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	m_RP.m_FlagsShader_RT |= lightVolumeMask;
}

#include "CommonRender.h"

#define SKY_BOX_SIZE 32.f
