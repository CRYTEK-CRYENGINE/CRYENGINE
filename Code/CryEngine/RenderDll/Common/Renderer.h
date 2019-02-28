// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMemory/CryPool/PoolAlloc.h>
#include <concqueue/concqueue.hpp>
#include "TextMessages.h"                             // CTextMessages
#include "RenderAuxGeom.h"
#include "RenderPipeline.h"
#include "RenderThread.h"                             // SRenderThread
#include "../Scaleform/ScaleformRender.h"
#include "../XRenderD3D9/DeviceManager/D3D11/DeviceSubmissionQueue_D3D11.h" // CSubmissionQueue_DX11
#include "ElementPool.h"

typedef void (PROCRENDEF)(SShaderPass* l, int nPrimType);

#define USE_NATIVE_DEPTH 1

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

extern uint32 ColorMasks[(ColorMask::GS_NOCOLMASK_COUNT >> GS_COLMASK_SHIFT)][4];
extern std::array<uint32, (ColorMask::GS_NOCOLMASK_COUNT >> GS_COLMASK_SHIFT)> AvailableColorMasks;

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
class CStandardGraphicsPipeline;
class CBaseResource;

namespace compute_skinning {
struct IComputeSkinningStorage;
}

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

#define TEXSTREAMING_DEFAULT_VAL              2

#define GEOM_INSTANCING_DEFAULT_VAL           0
#define COLOR_GRADING_DEFAULT_VAL             1
#define SUNSHAFTS_DEFAULT_VAL                 2
#define HDR_RANGE_ADAPT_DEFAULT_VAL           0
#define HDR_RENDERING_DEFAULT_VAL             1
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

#define MAX_PREDICTION_ZONES                  MAX_STREAM_PREDICTION_ZONES

#define MAX_SHADOWMAP_FRUSTUMS                1024
#define MAX_DEFERRED_LIGHTS                   256

#define TEMP_REND_OBJECTS_POOL                (2048)

#define MAX_REND_LIGHTS                       32

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#define CBUFFER_NATIVE_DEPTH_DEAFULT_VAL 1
#else
	#define CBUFFER_NATIVE_DEPTH_DEAFULT_VAL 0
#endif

#include "RendererResources.h"
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

struct SSkinningDataPoolInfo
{
	int                          poolIndex;
	CSkinningDataPool*           pDataCurrentFrame;
	CSkinningDataPool*           pDataPreviousFrame;
	std::vector<SSkinningData*>* pDataComputeSkinning;
};

//////////////////////////////////////////////////////////////////////
class CFillRateManager : private stl::PSyncMultiThread
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

	void Update(const SRenderingPassInfo& passInfo);
	void UpdateRainInfo(const SRenderingPassInfo& passInfo);
	void UpdateRainOccInfo(const SRenderingPassInfo& passInfo);
	void UpdateSnowInfo(const SRenderingPassInfo& passInfo);
};

struct SVolumetricCloudTexInfo
{
	int32 cloudTexId;
	int32 cloudNoiseTexId;
	int32 edgeNoiseTexId;
};

// Structure that describe properties of the current rendering quality
struct SRenderQuality
{
	EShaderQuality shaderQuality = eSQ_Max;
	ERenderQuality renderQuality = eRQ_Max;

	Vec2           downscaleFactor;

	//////////////////////////////////////////////////////////////////////////
	SRenderQuality() : downscaleFactor(Vec2(1.0f,1.0f)) {}
};

struct SMSAA
{
	SMSAA() : Type(0), Quality(0), m_pZTexture(nullptr) {}
	void Clear()
	{
		Type = 0;
		Quality = 0;
		m_pZTexture = nullptr;
	}

	uint32    Type;
	uint32    Quality;
	CTexture* m_pZTexture;
};

struct SRTargetStat
{
	string      m_Name;
	uint32      m_nSize;
	uint32      m_nWidth;
	uint32      m_nHeight;
	ETEX_Format m_eTF;

	void        GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_Name);
	}
};

struct CRY_ALIGN(128) SRenderStatistics
{
	static SRenderStatistics& Write() { return *s_pCurrentOutput; }

	int m_NumRendHWInstances;
	int m_RendHWInstancesPolysAll;
	int m_RendHWInstancesPolysOne;
	int m_RendHWInstancesDIPs;
	int m_NumTextChanges;
	int m_NumRTChanges;
	int m_NumStateChanges;
	int m_NumRendSkinnedObjects;
	int m_NumVShadChanges;
	int m_NumPShadChanges;
	int m_NumGShadChanges;
	int m_NumDShadChanges;
	int m_NumHShadChanges;
	int m_NumCShadChanges;
	int m_NumVShaders;
	int m_NumPShaders;
	int m_NumGShaders;
	int m_NumDShaders;
	int m_NumHShaders;
	int m_NumRTs;
	int m_NumSprites;
	int m_NumSpriteDIPS;
	int m_NumSpritePolys;
	int m_NumSpriteUpdates;
	int m_NumSpriteAltasesUsed;
	int m_NumSpriteCellsUsed;
	int m_NumQIssued;
	int m_NumQOccluded;
	int m_NumQNotReady;
	int m_NumQStallTime;
	int m_NumImpostersUpdates;
	int m_NumCloudImpostersUpdates;
	int m_NumImpostersDraw;
	int m_NumCloudImpostersDraw;
	int m_NumTextures;

#if defined(ENABLE_PROFILING_CODE)
	uint32 m_NumShadowPoolFrustums;
	uint32 m_NumShadowPoolAllocsThisFrame;
	uint32 m_NumShadowMaskChannels;
	uint32 m_NumTiledShadingSkippedLights;
#endif

	int m_NumPSInstructions;
	int m_NumVSInstructions;
	int m_RTCleared;
	int m_RTClearedSize;
	int m_RTCopied;
	int m_RTCopiedSize;
	int m_RTSize;

	CHWShader* m_pMaxPShader;
	CHWShader* m_pMaxVShader;
	void* m_pMaxPSInstance;
	void* m_pMaxVSInstance;

	size_t m_ManagedTexturesStreamSysSize;
	size_t m_ManagedTexturesStreamVidSize;
	size_t m_ManagedTexturesSysMemSize;
	size_t m_ManagedTexturesVidMemSize;
	size_t m_DynTexturesSize;
	size_t m_MeshUpdateBytes;
	size_t m_DynMeshUpdateBytes;
	float m_fOverdraw;
	float m_fSkinningTime;
	float m_fPreprocessTime;
	float m_fSceneTimeMT;
	float m_fTexUploadTime;
	float m_fTexRestoreTime;
	float m_fOcclusionTime;
	float m_fRenderTime;
	float m_fEnvCMapUpdateTime;
	float m_fEnvTextUpdateTime;

	float m_fRefractionPartialResolveEstimatedCost;
	int m_refractionPartialResolveCount;
	int m_refractionPartialResolvePixelCount;

	int m_NumRendMaterialBatches;
	int m_NumRendGeomBatches;
	int m_NumRendInstances;

#if defined(ENABLE_PROFILING_CODE)
	// Synchronously recorded stats
	int m_nNumInsts;
	int m_nNumInstCalls;

	int m_nNumPSOSwitches;
	int m_nNumLayoutSwitches;
	int m_nNumResourceSetSwitches;
	int m_nNumInlineSets;
	int m_nNumTopologySets;
	int m_nDIPs[EFSLIST_NUM];
	int m_nPolygons[EFSLIST_NUM];
	int m_nPolygonsByTypes[EFSLIST_NUM][EVCT_NUM][2];

	// Asynchronously recorded stats
	int m_nAsynchNumInsts;
	int m_nAsynchNumInstCalls;

	int m_nAsynchNumPSOSwitches;
	int m_nAsynchNumLayoutSwitches;
	int m_nAsynchNumResourceSetSwitches;
	int m_nAsynchNumInlineSets;
	int m_nAsynchNumTopologySets;
	int m_nAsynchDIPs[EFSLIST_NUM];
	int m_nAsynchPolygons[EFSLIST_NUM];
	int m_nAsynchPolygonsByTypes[EFSLIST_NUM][EVCT_NUM][2];
#endif

	int m_nModifiedCompiledObjects;
	int m_nTempCompiledObjects;
	int m_nIncompleteCompiledObjects;

	int m_nNumBoundVertexBuffers[2];   // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundIndexBuffers[2];    // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundConstBuffers[2];    // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundInlineBuffers[2];   // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundUniformBuffers[2];  // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundUniformTextures[2]; // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1

	static SRenderStatistics* s_pCurrentOutput;

	void Begin(const SRenderStatistics* prevData);
	void Finish();

#if defined(ENABLE_PROFILING_CODE)
	int  GetNumGeomInstances() const;
	int  GetNumGeomInstanceDrawCalls() const;

	int  GetNumberOfDrawCalls() const;
	int  GetNumberOfDrawCalls(const uint32 EFSListMask) const;
	int  GetNumberOfPolygons() const;
	int  GetNumberOfPolygons(const uint32 EFSListMask) const;
#endif
};

//////////////////////////////////////////////////////////////////////
class CRenderer : public IRenderer, public CRendererResources, public CRendererCVars
{
	friend class CRendererResources;
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

	virtual void RT_PresentFast() = 0;

	virtual int  CurThreadList() override;
	virtual void RT_BeginFrame(const SDisplayContextKey& displayContextKey) = 0;
	virtual void RT_EndFrame() = 0;

	virtual void RT_Init() = 0;
	virtual void RT_ShutDown(uint32 nFlags) = 0;
	virtual bool RT_CreateDevice() = 0;
	virtual void RT_Reset() = 0;

	virtual void RT_RenderScene(CRenderView* pRenderView) = 0;

	virtual void RT_ReleaseRenderResources(uint32 nFlags) = 0;

	virtual void RT_CreateRenderResources() = 0;
	virtual void RT_PrecacheDefaultShaders() = 0;
	virtual bool RT_ReadTexture(void* pDst, int destinationWidth, int destinationHeight, EReadTextureFormat dstFormat, CTexture* pSrc) = 0;
	virtual bool RT_StoreTextureToFile(const char* szFilePath, CTexture* pSrc) = 0;
	virtual void FlashRenderPlayer(std::shared_ptr<IFlashPlayer> &&pPlayer) override;
	virtual void FlashRender(std::shared_ptr<IFlashPlayer_RenderProxy> &&pPlayer) override;
	virtual void FlashRenderPlaybackLockless(std::shared_ptr<IFlashPlayer_RenderProxy> &&pPlayer, int cbIdx, bool finalPlayback) override;
	virtual void FlashRemoveTexture(ITexture* pTexture) override;

	virtual void RT_RenderDebug(bool bRenderStats = true) = 0;

	virtual void RT_FlashRenderInternal(std::shared_ptr<IFlashPlayer> &&pPlayer) = 0;
	virtual void RT_FlashRenderInternal(std::shared_ptr<IFlashPlayer_RenderProxy> &&pPlayer, bool doRealRender) = 0;
	virtual void RT_FlashRenderPlaybackLocklessInternal(std::shared_ptr<IFlashPlayer_RenderProxy> &&pPlayer, int cbIdx, bool finalPlayback, bool doRealRender) = 0;
	virtual bool FlushRTCommands(bool bWait, bool bImmediatelly, bool bForce) override;
	virtual bool ForceFlushRTCommands();
	virtual void WaitForParticleBuffer(int frameId) = 0;

	virtual void RequestFlushAllPendingTextureStreamingJobs(int nFrames) override { m_nFlushAllPendingTextureStreamingJobs = nFrames; }
	virtual void SetTexturesStreamingGlobalMipFactor(float fFactor) override      { m_fTexturesStreamingGlobalMipFactor = fFactor; }

	virtual void SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode = false) override = 0;

	//===============================================================================

	virtual float*      PinOcclusionBuffer(Matrix44A& camera) override = 0;
	virtual void        UnpinOcclusionBuffer() override = 0;

	virtual void        AddListener(IRendererEventListener* pRendererEventListener) override;
	virtual void        RemoveListener(IRendererEventListener* pRendererEventListener) override;

	virtual ERenderType GetRenderType() const override;

	virtual WIN_HWND    Init(int x, int y, int width, int height, unsigned int cbpp, int zbpp, int sbits, WIN_HWND Glhwnd = 0, bool bReInit = false, bool bShaderCacheGen = false) override = 0;

	virtual WIN_HWND    GetCurrentContextHWND() { return GetHWND(); }

	virtual int         GetFeatures() override { return m_Features; }

	virtual int         GetNumGeomInstances() override;

	virtual int GetNumGeomInstanceDrawCalls() override;

	virtual int  GetCurrentNumberOfDrawCalls() override;

	virtual void GetCurrentNumberOfDrawCalls(int& nGeneral, int& nShadowGen) override;

	virtual int  GetCurrentNumberOfDrawCalls(const uint32 EFSListMask) override;

	virtual void SetDebugRenderNode(IRenderNode* pRenderNode) override;

	virtual bool IsDebugRenderNode(IRenderNode* pRenderNode) const override;

	virtual bool         CheckDeviceLost() { return false; };

	EScreenAspectRatio   GetScreenAspect(int nWidth, int nHeight);

	virtual Vec2         SetViewportDownscale(float xscale, float yscale) override;

	virtual void         Release() override;
	virtual void         FreeSystemResources(int nFlags) override;
	virtual void         InitSystemResources(int nFlags) override;

	virtual void         BeginFrame(const SDisplayContextKey& displayContextKey) override = 0;
	virtual void         FillFrame(ColorF clearColor) override = 0;
	virtual void         RenderDebug(bool bRenderStats = true) override = 0;
	virtual void         EndFrame() override = 0;
	virtual void         LimitFramerate(const int maxFPS, const bool bUseSleep) = 0;

	virtual void         TryFlush() override = 0;

	virtual void         Reset(void) = 0;

	float                GetDrawNearestFOV() const { return m_drawNearFov; }

	virtual void         EnableVSync(bool enable) override = 0;

	virtual bool         SaveTga(unsigned char* sourcedata, int sourceformat, int w, int h, const char* filename, bool flip) const override;

	//download an image to video memory. 0 in case of failure
	virtual unsigned int UploadToVideoMemory(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override = 0;
	virtual void         UpdateTextureInVideoMemory(uint32 tnum, unsigned char* newdata, int posx, int posy, int w, int h, ETEX_Format eTFSrc = eTF_R8G8B8A8, int posz = 0, int sizez = 1) override = 0;
	virtual unsigned int UploadToVideoMemory3D(unsigned char* data, int w, int h, int d, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override = 0;
	virtual unsigned int UploadToVideoMemoryCube(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) override = 0;

	virtual bool         DXTCompress(byte* raw_data, int nWidth, int nHeight, ETEX_Format eTF, bool bUseHW, bool bGenMips, int nSrcBytesPerPix, MIPDXTcallback callback) override;
	virtual bool         DXTDecompress(byte* srcData, const size_t srcFileSize, byte* dstData, int nWidth, int nHeight, int nMips, ETEX_Format eSrcTF, bool bUseHW, int nDstBytesPerPix) override;

	virtual bool         SetGammaDelta(const float fGamma) override = 0;

	virtual void         RemoveTexture(unsigned int TextureId) override = 0;

	virtual int          GetWhiteTextureId() const override;

	CTextureManager*     GetTextureManager() { return m_pTextureManager; }

	virtual void         PrintResourcesLeaks() = 0;

	inline float         ScaleCoordXInternal(float value, const SRenderViewport& vp) const        { value *= float(vp.width) / 800.0f; return (value); }
	inline float         ScaleCoordYInternal(float value, const SRenderViewport& vp) const        { value *= float(vp.height) / 600.0f; return (value); }
	inline void          ScaleCoordInternal(float& x, float& y, const SRenderViewport& vp) const  { x = ScaleCoordXInternal(x, vp); y = ScaleCoordYInternal(y, vp); }

	virtual float        ScaleCoordX(float value) const override;
	virtual float        ScaleCoordY(float value) const override;
	virtual void         ScaleCoord(float& x, float& y) const override;

//	void                 SetWidth(int nW)                              { ChangeRenderResolution(nW, CRendererResources::s_renderHeight); }
//	void                 SetHeight(int nH)                             { ChangeRenderResolution(CRendererResources::s_renderWidth, nH); }
//	void                 SetPixelAspectRatio(float fPAR)               { m_pixelAspectRatio = fPAR; }

	virtual int          GetWidth() const override                     { return CRendererResources::s_renderWidth; }
	virtual int          GetHeight() const override                    { return CRendererResources::s_renderHeight; }

	virtual int          GetOverlayWidth() const override;
	virtual int          GetOverlayHeight() const override;

	virtual float        GetPixelAspectRatio() const override          { return (m_pixelAspectRatio); }

	virtual bool         IsStereoEnabled() const override              { return false; }

	virtual float        GetNearestRangeMax() const override           { return (CV_r_DrawNearZRange); }

	virtual int          GetWireframeMode()                            { return(m_wireframe_mode); }

	virtual CRenderView* GetOrCreateRenderView(IRenderView::EViewType Type = IRenderView::eViewType_Default) threadsafe final;
	virtual void         ReturnRenderView(CRenderView* pRenderView) threadsafe final;
	void                 DeleteRenderViews();

	void             GetPolyCount(int& nPolygons, int& nShadowPolys) override;
	int              GetPolyCount() override;

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
	virtual void OnEntityDeleted(IRenderNode* pRenderNode) override;

	virtual void SetHighlightColor(ColorF color) override { m_highlightColor = color; }
	virtual void SetSelectionColor(ColorF color) override { m_SelectionColor = color; }
	virtual void SetHighlightParams(float outlineThickness, float fGhostAlpha) override
	{ 
		m_highlightParams = Vec4(outlineThickness, fGhostAlpha, 0.0f, 0.0f); 
	}

	ColorF& GetHighlightColor()         { return m_highlightColor; }
	ColorF& GetSelectionColor()         { return m_SelectionColor; }
	Vec4&   GetHighlightParams()        { return m_highlightParams; }

	//misc
	virtual bool                ScreenShot(const char* filename = NULL, const SDisplayContextKey& displayContextKey = {}) override = 0;
	virtual bool                ReadFrameBuffer(uint32* pDstRGBA8, int destinationWidth, int destinationHeight, bool readPresentedBackBuffer = true, EReadTextureFormat format = EReadTextureFormat::RGB8) override = 0;

	virtual int                 GetColorBpp() override   { return m_cbpp; }
	virtual int                 GetDepthBpp() override   { return m_zbpp; }
	virtual int                 GetStencilBpp() override { return m_sbpp; }

	virtual void                Set2DMode(bool enable, int ortox, int ortoy, float znear = -1e10f, float zfar = 1e10f) override = 0;

	virtual void                LockParticleVideoMemory(int frameId) override           {}
	virtual void                UnLockParticleVideoMemory(int frameId) override         {}

	virtual void                ActivateLayer(const char* pLayerName, bool activate) override {}

	virtual void                FlushPendingTextureTasks() override;
	virtual void                FlushPendingUploads() override;

	virtual void                SetCloakParams(const SRendererCloakParams& cloakParams) override;
	virtual float               GetCloakFadeLightScale() const override;
	virtual void                SetCloakFadeLightScale(float fColorScale) override;
	virtual void                SetShadowJittering(float shadowJittering) override;
	virtual float               GetShadowJittering() const override;

	void                        EF_AddClientPolys(const SRenderingPassInfo& passInfo);

	virtual void*               FX_AllocateCharInstCB(SSkinningData*, uint32) { return NULL; }
	virtual void                FX_ClearCharInstCB(uint32)                    {}

	virtual EShaderQuality      EF_GetShaderQuality(EShaderType eST) final;
	virtual ERenderQuality      EF_GetRenderQuality() const final             { return m_renderQuality.renderQuality; };

	virtual void                EF_SubmitWind(const SWindGrid* pWind) override;

	void                        RefreshSystemShaders();

	virtual float               EF_GetWaterZElevation(float fX, float fY) override;

	virtual IOpticsElementBase* CreateOptics(EFlareType type) const override;

	virtual bool                EF_PrecacheResource(IShader* pSH, float fMipFactor, float fTimeToReady, int Flags) override;
	virtual bool                EF_PrecacheResource(ITexture* pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter) override = 0;
	virtual bool                EF_PrecacheResource(IRenderMesh* pPB, IMaterial* pMaterial, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId) override;
	virtual bool                EF_PrecacheResource(SRenderLight* pLS, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId) override;

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
	virtual DynArray<uint16_t>     EF_RenderEnvironmentCubeHDR(std::size_t size, const Vec3& Pos) override;
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
	virtual CRenderObject* EF_DuplicateRO(CRenderObject* pObj, const SRenderingPassInfo& passInfo) final;
	virtual CRenderObject* EF_GetObject() final;
	virtual void           EF_FreeObject(CRenderObject* pObj) final;

	// Draw all shaded REs in the list
	virtual void EF_EndEf3D(const int nFlags, const int nPrecacheUpdateId, const int nNearPrecacheUpdateId, const SRenderingPassInfo& passInfo) override = 0;

	virtual void EF_InvokeShadowMapRenderJobs(const SRenderingPassInfo& passInfo, const int nFlags) override {}
	virtual IRenderView* GetNextAvailableShadowsView(IRenderView* pMainRenderView, ShadowMapFrustum* pOwnerFrustum) override;
	void PrepareShadowFrustumForShadowPool(ShadowMapFrustum* pFrustum, uint32 frameID, const SRenderLight& light, uint32 *timeSlicedShadowsUpdated) override final;

	// 2d interface for shaders
	virtual void EF_EndEf2D(const bool bSort) override = 0;

	// Dynamic lights
	virtual bool                   EF_IsFakeDLight(const SRenderLight* Source) override;
	virtual void                   EF_ADDDlight(SRenderLight* Source, const SRenderingPassInfo& passInfo) override;
	virtual bool                   EF_AddDeferredDecal(const SDeferredDecal& rDecal, const SRenderingPassInfo& passInfo) override;

	virtual int                    EF_AddDeferredLight(const SRenderLight& pLight, float fMult, const SRenderingPassInfo& passInfo) override;

	virtual void                   EF_ReleaseDeferredData() override;
	virtual SInputShaderResources* EF_CreateInputShaderResource(IRenderShaderResources* pOptionalCopyFrom = nullptr) override;
	virtual void                   ClearPerFrameData(const SRenderingPassInfo& passInfo);
	virtual bool                   EF_UpdateDLight(SRenderLight* pDL) override;
	void                           EF_CheckLightMaterial(SRenderLight* pLight, uint16 nRenderLightID, const SRenderingPassInfo& passInfo);

	virtual void EF_QueryImpl(ERenderQueryTypes eQuery, void* pInOut0, uint32 nInOutSize0, void* pInOut1, uint32 nInOutSize1) override;

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

	virtual SSkinningData*                            EF_CreateSkinningData(IRenderView* pRenderView, uint32 nNumBones, bool bNeedJobSyncVar) override;
	virtual SSkinningData*                            EF_CreateRemappedSkinningData(IRenderView* pRenderView, uint32 nNumBones, SSkinningData* pSourceSkinningData, uint32 nCustomDataSize, uint32 pairGuid) override;
	virtual void                                      EF_EnqueueComputeSkinningData(IRenderView* pRenderView, SSkinningData* pData) override;
	virtual int                                       EF_GetSkinningPoolID() override;
	SSkinningDataPoolInfo                             GetSkinningDataPools();
	void                                              ClearSkinningDataPool();

	virtual void                                      UpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial) override;
	virtual void                                      ForceUpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial) override;
	virtual void                                      RefreshShaderResourceConstants(SShaderItem* pShaderItem, IMaterial* pMaterial) override;

	virtual bool                                      LoadShaderStartupCache() override;

	virtual void                                      UnloadShaderStartupCache() override;

	virtual void                                      CopyTextureRegion(ITexture* pSrc, RectI srcRegion, ITexture* pDst, RectI dstRegion, ColorF& color, const int renderStateFlags) override;

	virtual bool                                      LoadShaderLevelCache() override            { return false; }
	virtual void                                      UnloadShaderLevelCache() override          {}

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

	void   ReadPerFrameShaderConstants(const SRenderingPassInfo& passInfo, bool bSecondaryViewport);

	uint32 GetActiveGPUCount() const override { return CV_r_multigpu > 0 ? m_nGPUs : 1; }

	void   Logv(const char* format, ...);
	void   LogStrv(char* format, ...);
	void   LogShv(char* format, ...);
	void   Log(char* str);

	// Add shader to the list
	void              EF_AddEf_NotVirtual(CRenderElement* pRE, SShaderItem& pSH, CRenderObject* pObj, const SRenderingPassInfo& passInfo, int nList, int nAW);

	void              EF_TransformDLights();
	void              EF_IdentityDLights();

	void              EF_AddRTStat(CTexture* pTex, int nFlags = 0, int nW = -1, int nH = -1);
	void              EF_PrintRTStats(const char* szName);

	static inline eAntialiasingType FX_GetAntialiasingType () { return (eAntialiasingType)((uint32)1 << min(CV_r_AntialiasingMode, eAT_AAMODES_COUNT - 1)); }
	static inline bool              IsHDRDisplayEnabled    () { return (CV_r_HDRSwapChain && !CV_r_measureoverdraw) ? true : false; }
	static inline bool              IsPostProcessingEnabled() { return (CV_r_PostProcess && !CV_r_measureoverdraw) ? true : false; }

	void              UpdateRenderingModesInfo();
	bool              IsCustomRenderModeEnabled(uint32 nRenderModeMask);

	bool              IsEditorMode() const
	{
#if CRY_PLATFORM_DESKTOP
		return (m_bEditor != 0);
#else
		return false;
#endif
	}

	bool              IsShaderCacheGenMode() const
	{
#if CRY_PLATFORM_DESKTOP
		return (m_bShaderCacheGen != 0);
#else
		return false;
#endif
	}

	//	Get mipmap distance factor (depends on screen width, screen height and aspect ratio)
	float                GetMipDistFactor(uint32 twidth, uint32 theight) { float ratio = std::max(twidth, theight) / float(CRendererResources::s_renderMinDim); return ratio * ratio; }
//	float                GetMipDistFactor(uint32 twidth, uint32 theight) { return ((TANGENT30_2 * TANGENT30_2) / (m_rheight * m_rheight)) * std::max(twidth, theight) * std::max(twidth, theight); }

	static int           GetTexturesStreamPoolSize();

protected:
	void EF_AddParticle(CREParticle* pParticle, SShaderItem& shaderItem, CRenderObject* pRO, const SRenderingPassInfo& passInfo);
	void EF_RemoveParticlesFromScene();
	void PrepareParticleRenderObjects(Array<const SAddParticlesToSceneJob> aJobs, int nREStart, const SRenderingPassInfo& passInfo);
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

	virtual const RPProfilerStats*                   GetRPPStats(ERenderPipelineProfilerStats eStat, bool bCalledFromMainThread = true) override  { return nullptr; }
	virtual const RPProfilerStats*                   GetRPPStatsArray(bool bCalledFromMainThread = true) override                                 { return nullptr; }
	virtual const DynArray<RPProfilerDetailedStats>* GetRPPDetailedStatsArray(bool bCalledFromMainThread = true ) override                        { return nullptr; }

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
	void         AddRecordedProfilingStats(const struct SProfilingStats& stats, ERenderListID renderList, bool bAsynchronous);
#endif

	virtual void                CollectDrawCallsInfo(bool status) override;
	virtual void                CollectDrawCallsInfoPerNode(bool status) override;
	virtual void                EnableLevelUnloading(bool enable) override;
	virtual void                EnableBatchMode(bool enable) override;
	virtual bool                IsStereoModeChangePending() override { return false; }
		
	virtual void QueryActiveGpuInfo(SGpuInfo& info) const override;

	virtual compute_skinning::IComputeSkinningStorage* GetComputeSkinningStorage() = 0;

	int   GetStreamZoneRoundId( int zone ) const { assert(zone >=0 && zone < MAX_PREDICTION_ZONES); return m_streamZonesRoundId[zone]; };

	// Only should be used to get current frame id internally in the render thread.
	int GetRenderFrameID() const;
	int GetMainFrameID()   const;

	threadID GetMainThreadID() const { return m_nFillThreadID; }
	threadID GetRenderThreadID() const { return m_nProcessThreadID; }
	SRenderObjectAccessThreadConfig GetObjectAccessorThreadConfig() const
	{
		CRY_ASSERT(m_pRT->IsRenderThread());
		return SRenderObjectAccessThreadConfig(GetRenderThreadID());
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Query Anti-Aliasing information.
	bool                  IsMSAAEnabled() const { return m_MSAAData.Type > 0; }
	const SMSAA&          GetMSAA() const { return m_MSAAData; }

	void                  SetRenderQuality( const SRenderQuality &quality );
	const SRenderQuality& GetRenderQuality() const { return m_renderQuality; };

	// Animation time is used for rendering animation effects and can be paused if CRenderer::m_bPauseTimer is true
	void                  SetAnimationTime(CTimeValue time) { m_animationTime = time; }
	CTimeValue            GetAnimationTime() const { return m_animationTime; }

	// Time of the last main to renderer thread sync
	void                  SetFrameSyncTime(CTimeValue time) { m_frameSyncTime = time; }
	CTimeValue            GetFrameSyncTime() const { return m_frameSyncTime; }
		
	// Return current graphics pipeline
	CStandardGraphicsPipeline&     GetGraphicsPipeline() { return *m_pGraphicsPipeline; }

	template<typename RenderThreadCallback>
	void ExecuteRenderThreadCommand(RenderThreadCallback&& callback, ERenderCommandFlags flags = ERenderCommandFlags::None)
	{
		m_pRT->ExecuteRenderThreadCommand(std::forward<RenderThreadCallback>(callback), flags);
	}

	// Called every frame from the Render Thread to reclaim deleted resources.
	void                   ScheduleResourceForDelete(CBaseResource* pResource);
	void                   RT_DelayedDeleteResources(bool bAllResources=false);

public:
	Matrix44A m_IdentityMatrix;

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

	int                  m_nGraphicsPipeline;

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
	float m_overrideRefreshRate;
	int   m_overrideScanlineOrder;
#endif

	int       m_nStencilMaskRef;

	byte      m_bDeviceSupportsInstancing;

	uint32    m_bDeviceSupports_AMDExt;
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
	uint32  m_nPoolIndex = 0;

	bool    m_bVendorLibInitialized;

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

	static int            m_iGeomInstancingThreshold; // internal value, auto mapped depending on GPU hardware, 0 means not set yet

	// Limit for local sorting array
	static const int          nMaxParticleContainer = 8 * 1024;

	int                       m_nCREParticleCount[RT_COMMAND_BUF_COUNT];
	JobManager::SJobState     m_ComputeVerticesJobState;

	CFillRateManager          m_FillRateManager;

	FILE*                     m_LogFile;
	FILE*                     m_LogFileStr;
	FILE*                     m_LogFileSh;

protected:
	//================================================================================
	int                                        m_cbpp, m_zbpp, m_sbpp;
	int                                        m_wireframe_mode, m_wireframe_mode_prev;
	uint32                                     m_nGPUs;                      // Use GetActiveGPUCount() to read
	float                                      m_drawNearFov;
	float                                      m_pixelAspectRatio;
	float                                      m_shadowJittering;
	StaticArray<int, MAX_GSM_LODS_NUM>         m_CachedShadowsResolution;

	CSkinningDataPool                          m_SkinningDataPool[3];        // Tripple Buffered for motion blur
	std::array<std::vector<SSkinningData*>, 3> m_computeSkinningData;

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

	std::vector<ISyncMainWithRenderListener*> m_syncMainWithRenderListeners;

	CryMutex             m_mtxStopAtRenderFrameEnd;
	CryConditionVariable m_condStopAtRenderFrameEnd;

	ColorF m_highlightColor;
	ColorF m_SelectionColor;
	Vec4  m_highlightParams;

	// Separate render views per recursion
	SElementPool<CRenderView> m_pRenderViewPool[IRenderView::eViewType_Count];
	void InitRenderViewPool();

	// Temporary render objects storage
	struct STempObjects
	{
		std::shared_ptr<class CRenderObjectsPools> m_renderObjectsPools;
		// Array of render objects that need to be deleted next frame
		CThreadSafeRendererContainer<class CPermanentRenderObject*> m_persistentRenderObjectsToDelete[RT_COMMAND_BUF_COUNT];
	};
	STempObjects m_tempRenderObjects;

	// Resource deletion is delayed for at least 3 frames.
	CThreadSafeRendererContainer<CBaseResource*> m_resourcesToDelete[RT_COMMAND_BUF_COUNT];
	volatile int m_currentResourceDeleteBuffer = 0;

	// rounds ID from 3D engine, useful for texture streaming
	int m_streamZonesRoundId[MAX_PREDICTION_ZONES];

	int m_nRenderThreadFrameID = 0;

	struct SWaterUpdateInfo
	{
		float m_fLastWaterFOVUpdate;
		Vec3  m_LastWaterViewdirUpdate;
		Vec3  m_LastWaterUpdirUpdate;
		Vec3  m_LastWaterPosUpdate;
		float m_fLastWaterUpdate;
		int   m_nLastWaterFrameID;
	};
	SWaterUpdateInfo m_waterUpdateInfo;

	// Antialiasing data.
	SMSAA    m_MSAAData;

	// Render frame statistics
	SRenderStatistics m_frameRenderStats[RT_COMMAND_BUF_COUNT];

	// Render target statistics
	std::vector<SRTargetStat> m_renderTargetStats;

	// Rendering Quality
	SRenderQuality    m_renderQuality;

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Animation time is used for rendering animation effects and can be paused if CRenderer::m_bPauseTimer is true
	CTimeValue     m_animationTime;
	// Time of the last main to renderer thread sync
	CTimeValue     m_frameSyncTime;
	//////////////////////////////////////////////////////////////////////////

	std::unique_ptr<CStandardGraphicsPipeline> m_pGraphicsPipeline;

public: // TEMPORARY PUBLIC
	friend struct SRenderThread;
	//////////////////////////////////////////////////////////////////////////
	// Render Thread support
	threadID     m_nFillThreadID;
	threadID     m_nProcessThreadID;
	//////////////////////////////////////////////////////////////////////////
	
private:
};

inline int32 CRenderer::RT_GetCurrGpuID() const
{
	return gRenDev->m_nFrameSwapID % gRenDev->GetActiveGPUCount();
}

inline int CRenderer::GetRenderFrameID() const
{
	assert(!m_pRT->IsMultithreaded() || m_pRT->IsRenderThread());
	return m_nRenderThreadFrameID;
}

inline int CRenderer::GetMainFrameID() const
{
	assert(!m_pRT->IsMultithreaded() || !m_pRT->IsRenderThread());
	return gEnv->nMainFrameID;
}

#include "CommonRender.h"

#define SKY_BOX_SIZE 32.f
