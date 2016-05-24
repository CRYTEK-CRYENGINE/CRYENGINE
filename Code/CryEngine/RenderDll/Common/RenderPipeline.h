// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   RenderPipeline.h : Shaders pipeline declarations.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#ifndef __RENDERPIPELINE_H__
#define __RENDERPIPELINE_H__

#pragma  once

#include <CryThreading/CryThreadSafeRendererContainer.h>
#include <CryThreading/CryThreadSafeWorkerContainer.h>
#include "Shadow_Renderer.h"

#include "LightVolumeBuffer.h"
#include "ParticleBuffer.h"

//====================================================================

#define MAX_HWINST_PARAMS          32768

#define MAX_REND_OBJECTS           16384
#define TEMP_REND_OBJECTS_POOL     2048
#define MAX_REND_GEOMS_IN_BATCH    16

#define MAX_REND_SHADERS           4096
#define MAX_REND_SHADER_RES        16384
#define MAX_REND_LIGHTS            32
#define MAX_DEFERRED_LIGHTS        256

#define MAX_SHADOWMAP_LOD          20
#define MAX_SHADOWMAP_FRUSTUMS     1024

#define MAX_SORT_GROUPS            64
#define MAX_INSTANCES_THRESHOLD_HW 8
#define MAX_PREDICTION_ZONES       MAX_STREAM_PREDICTION_ZONES

#define CULLER_MAX_CAMS            4
//#define CULLER_DEBUG
//#define CULLER_POSITION_ONLY

#define HW_INSTANCING_ENABLED

class CRenderView;

struct SViewport
{
	int   nX, nY, nWidth, nHeight;
	float fMinZ, fMaxZ;
	SViewport()
		: nX(0)
		, nY(0)
		, nWidth(0)
		, nHeight(0)
		, fMinZ(0.0f)
		, fMaxZ(0.0f)
	{}

	SViewport(int nNewX, int nNewY, int nNewWidth, int nNewHeight)
		: nX(nNewX)
		, nY(nNewY)
		, nWidth(nNewWidth)
		, nHeight(nNewHeight)
		, fMinZ(0.0f)
		, fMaxZ(0.0f)
	{
	}
	inline friend bool operator!=(const SViewport& m1, const SViewport& m2)
	{
		if (m1.nX != m2.nX || m1.nY != m2.nY || m1.nWidth != m2.nWidth || m1.nHeight != m2.nHeight || m1.fMinZ != m2.fMinZ || m1.fMaxZ != m2.fMaxZ)
			return true;
		return false;
	}
};

// FIXME: probably better to sort by shaders (Currently sorted by resources)
// TODO: DURANGO doesn't like _MS_ALIGN(32) when passed to std::sort
struct SRendItem
{
	uint32 SortVal;
	uint32 nBatchFlags;
	union
	{
		uint32 ObjSort;
		float  fDist;
	};
	SRendItemSorter        rendItemSorter;
	CRenderObject*         pObj;
	CCompiledRenderObject* pCompiledObject;
	CRendElementBase*      pElem;
	//uint32 nStencRef  : 8;

	//==================================================
	static void*       mfGetPointerCommon(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags);

	static inline void mfGet(uint32 nVal, int& nTechnique, CShader*& Shader, CShaderResources*& Res)
	{
		Shader = (CShader*)CShaderMan::s_pContainer->m_RList[CBaseResource::RListIndexFromId((nVal >> 20) & (MAX_REND_SHADERS - 1))];
		nTechnique = (nVal & 0x3f);
		if (nTechnique == 0x3f)
			nTechnique = -1;
		int nRes = (nVal >> 6) & (MAX_REND_SHADER_RES - 1);
		Res = (nRes) ? CShader::s_ShaderResources_known[nRes] : NULL;
	}
	static inline CShader* mfGetShader(uint32 flag)
	{
		return (CShader*)CShaderMan::s_pContainer->m_RList[CBaseResource::RListIndexFromId((flag >> 20) & (MAX_REND_SHADERS - 1))];
	}
	static inline CShaderResources* mfGetRes(uint32 nVal)
	{
		int nRes = (nVal >> 6) & (MAX_REND_SHADER_RES - 1);
		return ((nRes) ? CShader::s_ShaderResources_known[nRes] : NULL);
	}
	static inline void ExtractShaderItem(uint32 nVal, SShaderItem& sh)
	{
		CShader* pShader = (CShader*)(CShaderMan::s_pContainer->m_RList[CBaseResource::RListIndexFromId((nVal >> 20) & (MAX_REND_SHADERS - 1))]);
		sh.m_pShader = static_cast<IShader*>(pShader);
		uint32 nTechnique = (nVal & 0x3f);
		if (nTechnique == 0x3f)
			nTechnique = -1;
		sh.m_nTechnique = nTechnique;
		int nRes = (nVal >> 6) & (MAX_REND_SHADER_RES - 1);
		sh.m_pShaderResources = (IRenderShaderResources*)((nRes) ? CShader::s_ShaderResources_known[nRes] : nullptr);
	}
	static bool   IsListEmpty(int nList);
	static uint32 BatchFlags(int nList);

	// Sort by SortVal member of RI
	static void mfSortPreprocess(SRendItem* First, int Num);
	// Sort by distance
	static void mfSortByDist(SRendItem* First, int Num, bool bDecals, bool InvertedOrder = false);
	// Sort by light
	static void mfSortByLight(SRendItem* First, int Num, bool bSort, const bool bIgnoreRePtr, bool bSortDecals);
	// Special sorting for ZPass (compromise between depth and batching)
	static void mfSortForZPass(SRendItem* First, int Num);
};

// Helper function to be used in logging
bool IsRecursiveRenderView();

//==================================================================

struct SShaderPass;

union UVertStreamPtr
{
	void*                 Ptr;
	byte*                 PtrB;
	SVF_P3F_C4B_T4B_N3F2* PtrVF_P3F_C4B_T4B_N3F2;
};

//==================================================================
// StencilStates

//Note: If these are altered, g_StencilFuncLookup and g_StencilOpLookup arrays
//			need to be updated in turn

#define FSS_STENCFUNC_ALWAYS   0x0
#define FSS_STENCFUNC_NEVER    0x1
#define FSS_STENCFUNC_LESS     0x2
#define FSS_STENCFUNC_LEQUAL   0x3
#define FSS_STENCFUNC_GREATER  0x4
#define FSS_STENCFUNC_GEQUAL   0x5
#define FSS_STENCFUNC_EQUAL    0x6
#define FSS_STENCFUNC_NOTEQUAL 0x7
#define FSS_STENCFUNC_MASK     0x7

#define FSS_STENCIL_TWOSIDED   0x8

#define FSS_CCW_SHIFT          16

#define FSS_STENCOP_KEEP       0x0
#define FSS_STENCOP_REPLACE    0x1
#define FSS_STENCOP_INCR       0x2
#define FSS_STENCOP_DECR       0x3
#define FSS_STENCOP_ZERO       0x4
#define FSS_STENCOP_INCR_WRAP  0x5
#define FSS_STENCOP_DECR_WRAP  0x6
#define FSS_STENCOP_INVERT     0x7

#define FSS_STENCFAIL_SHIFT    4
#define FSS_STENCFAIL_MASK     (0x7 << FSS_STENCFAIL_SHIFT)

#define FSS_STENCZFAIL_SHIFT   8
#define FSS_STENCZFAIL_MASK    (0x7 << FSS_STENCZFAIL_SHIFT)

#define FSS_STENCPASS_SHIFT    12
#define FSS_STENCPASS_MASK     (0x7 << FSS_STENCPASS_SHIFT)

#define STENC_FUNC(op)        (op)
#define STENC_CCW_FUNC(op)    (op << FSS_CCW_SHIFT)
#define STENCOP_FAIL(op)      (op << FSS_STENCFAIL_SHIFT)
#define STENCOP_ZFAIL(op)     (op << FSS_STENCZFAIL_SHIFT)
#define STENCOP_PASS(op)      (op << FSS_STENCPASS_SHIFT)
#define STENCOP_CCW_FAIL(op)  (op << (FSS_STENCFAIL_SHIFT + FSS_CCW_SHIFT))
#define STENCOP_CCW_ZFAIL(op) (op << (FSS_STENCZFAIL_SHIFT + FSS_CCW_SHIFT))
#define STENCOP_CCW_PASS(op)  (op << (FSS_STENCPASS_SHIFT + FSS_CCW_SHIFT))

//Stencil masks
#define BIT_STENCIL_RESERVED          0x80
#define BIT_STENCIL_INSIDE_CLIPVOLUME 0x40
#define STENC_VALID_BITS_NUM          7
#define STENC_MAX_REF                 ((1 << STENC_VALID_BITS_NUM) - 1)

//==================================================================

struct SOnDemandD3DStreamProperties
{
	D3D11_INPUT_ELEMENT_DESC* m_pElements;
	uint32                    m_nNumElements;
};

struct SOnDemandD3DVertexDeclaration
{
	TArray<D3D11_INPUT_ELEMENT_DESC> m_Declaration;
};

struct SOnDemandD3DVertexDeclarationCache
{
	ID3D11InputLayout* m_pDeclaration;
};

struct SVertexDeclaration
{
	int                              StreamMask;
	int                              VertFormat;
	int                              InstAttrMask;
	TArray<D3D11_INPUT_ELEMENT_DESC> m_Declaration;
	ID3D11InputLayout*               m_pDeclaration;

	~SVertexDeclaration()
	{
		SAFE_RELEASE(m_pDeclaration);
	}
};

struct SMSAA
{
	SMSAA() : Type(0), Quality(0), m_pZTexture(0)
	{
	}

	UINT      Type;
	DWORD     Quality;

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

struct CRY_ALIGN(128) SPipeStat
{
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
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

	int m_ImpostersSizeUpdate;
	int m_CloudImpostersSizeUpdate;

#if REFRACTION_PARTIAL_RESOLVE_STATS
	float m_fRefractionPartialResolveEstimatedCost;
	int m_refractionPartialResolveCount;
	int m_refractionPartialResolvePixelCount;
#endif

#if defined(ENABLE_PROFILING_CODE)
	int m_NumRendMaterialBatches;
	int m_NumRendGeomBatches;
	int m_NumRendInstances;

	int m_nDIPs[EFSLIST_NUM];
	int m_nInsts;
	int m_nInstCalls;
	int m_nPolygons[EFSLIST_NUM];
	int m_nPolygonsByTypes[EFSLIST_NUM][EVCT_NUM][2];

	int m_nModifiedCompiledObjects;
	int m_nTempCompiledObjects;
	int m_nIncompleteCompiledObjects;
	int m_nNumPSOSwitches;
	int m_nNumLayoutSwitches;
	int m_nNumResourceSetSwitches;
	int m_nNumInlineSets;
	int m_nNumTopologySets;

	int m_nNumBoundVertexBuffers[2];   // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundIndexBuffers[2];    // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundConstBuffers[2];    // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundInlineBuffers[2];   // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundUniformBuffers[2];  // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
	int m_nNumBoundUniformTextures[2]; // Local=0,PCIe=1 - or in tech-speak, L1=0 and L0=1
#endif
};

//Batch flags.
// - When adding/removing batch flags, please, update sBatchList static list in D3DRendPipeline.cpp
enum EBatchFlags
{
	FB_GENERAL         = 0x1,
	FB_TRANSPARENT     = 0x2,
	FB_SKIN            = 0x4,
	FB_Z               = 0x8,
	FB_BELOW_WATER     = 0x10,
	FB_ZPREPASS        = 0x20,
	FB_PREPROCESS      = 0x40,
	FB_MOTIONBLUR      = 0x80,
	FB_POST_3D_RENDER  = 0x100,
	FB_MULTILAYERS     = 0x200,
	FB_COMPILED_OBJECT = 0x400,
	FB_CUSTOM_RENDER   = 0x800,
	FB_SOFTALPHATEST   = 0x1000,
	FB_LAYER_EFFECT    = 0x2000,
	FB_WATER_REFL      = 0x4000,
	FB_WATER_CAUSTIC   = 0x8000,
	FB_DEBUG           = 0x10000,
	FB_EYE_OVERLAY     = 0x80000,

	FB_MASK            = 0xfffff //! FB flags cannot exceed 0xfffff
};

// Commit flags
#define FC_TARGETS             1
#define FC_GLOBAL_PARAMS       2
#define FC_PER_INSTANCE_PARAMS 4
#define FC_PER_BATCH_PARAMS    8
#define FC_MATERIAL_PARAMS     0x10
#define FC_ALL                 0x1f

// m_RP.m_Flags
#define RBF_NEAREST 0x10000

// m_RP.m_TI.m_PersFlags
#define RBPF_DRAWTOTEXTURE            (1 << 16) // 0x10000
#define RBPF_MIRRORCAMERA             (1 << 17) // 0x20000
#define RBPF_MIRRORCULL               (1 << 18) // 0x40000

#define RBPF_ZPASS                    (1 << 19) // 0x80000
#define RBPF_SHADOWGEN                (1 << 20) // 0x100000

#define RBPF_FP_DIRTY                 (1 << 21) // 0x200000
#define RBPF_FP_MATRIXDIRTY           (1 << 22) // 0x400000

#define RBPF_IMPOSTERGEN              (1 << 23)
#define RBPF_MAKESPRITE               (1 << 24) // 0x1000000

#define RBPF_HDR                      (1 << 26)
#define RBPF_REVERSE_DEPTH            (1 << 27)
#define RBPF_ENCODE_HDR               (1 << 29)
#define RBPF_OBLIQUE_FRUSTUM_CLIPPING (1 << 30)

// m_RP.m_PersFlags1
#define RBPF1_USESTREAM               (1 << 0)
#define RBPF1_USESTREAM_MASK          ((1 << VSF_NUM) - 1)

#define RBPF1_IN_CLEAR                (1 << 17)

#define RBPF1_SKIP_AFTER_POST_PROCESS (1 << 18)

// m_RP.m_PersFlags2
#define RBPF2_NOSHADERFOG                         (1 << 0)
#define RBPF2_RAINRIPPLES                         (1 << 1)
#define RBPF2_NOALPHABLEND                        (1 << 2)
#define RBPF2_SINGLE_FORWARD_LIGHT_PASS           (1 << 3)
#define RBPF2_MSAA_RESTORE_SAMPLE_MASK            (1 << 4)
#define RBPF2_READMASK_RESERVED_STENCIL_BIT       (1 << 5)
#define RBPF2_POST_3D_RENDERER_PASS               (1 << 6)
#define RBPF2_LENS_OPTICS_COMPOSITE               (1 << 7)
#define RBPF2_COMMIT_SG                           (1 << 8)
#define RBPF2_HDR_FP16                            (1 << 9)
#define RBPF2_CUSTOM_SHADOW_PASS                  (1 << 10)
#define RBPF2_CUSTOM_RENDER_PASS                  (1 << 11)
#define RBPF2_THERMAL_RENDERMODE_PASS             (1 << 12)

#define RBPF2_COMMIT_CM                           (1 << 13)
#define RBPF2_ZPREPASS                            (1 << 14)

#define RBPF2_FORWARD_SHADING_PASS                (1 << 15)

#define RBPF2_MSAA_STENCILCULL                    (1 << 16)

#define RBPF2_THERMAL_RENDERMODE_TRANSPARENT_PASS (1 << 17)
#define RBPF2_NOALPHATEST                         (1 << 18)
#define RBPF2_WATERRIPPLES                        (1 << 19)
#define RBPF2_ALLOW_DEFERREDSHADING               (1 << 20)

#define RBPF2_COMMIT_PF                           (1 << 21)
#define RBPF2_MSAA_SAMPLEFREQ_PASS                (1 << 22)
#define RBPF2_DRAWTOCUBE                          (1 << 23)

#define RBPF2_MOTIONBLURPASS                      (1 << 24)
#define RBPF2_MATERIALLAYERPASS                   (1 << 25)
#define RBPF2_DISABLECOLORWRITES                  (1 << 26)

#define RBPF2_NOPOSTAA                            (1 << 27)
#define RBPF2_SKIN                                (1 << 28)

#define RBPF2_LIGHTSHAFTS                         (1 << 29)
#define RBPF2_WRITEMASK_RESERVED_STENCIL_BIT      (1 << 30)   // 0x010
#define RBPF2_HALFRES_PARTICLES                   (1 << 31)

// m_RP.m_FlagsPerFlush
#define RBSI_LOCKCULL          0x2
#define RBSI_INSTANCED         0x10000000
#define RBSI_CUSTOM_PREVMATRIX 0x20000000

// m_RP.m_ShaderLightMask
#define SLMF_DIRECT      0
#define SLMF_POINT       1
#define SLMF_PROJECTED   2
#define SLMF_TYPE_MASK   (SLMF_POINT | SLMF_PROJECTED)

#define SLMF_LTYPE_SHIFT 8
#define SLMF_LTYPE_BITS  4

struct SLightPass
{
	SRenderLight* pLights[4];
	uint32        nStencLTMask;
	uint32        nLights;
	uint32        nLTMask;
	bool          bRect;
	RectI         rc;
};

#define MAX_STREAMS 16

struct SFogState
{
	bool   m_bEnable;
	ColorF m_FogColor;
	ColorF m_CurColor;

	bool operator!=(const SFogState& fs) const
	{
		return m_FogColor != fs.m_FogColor;
	}
};

enum EShapeMeshType
{
	SHAPE_PROJECTOR = 0,
	SHAPE_PROJECTOR1,
	SHAPE_PROJECTOR2,
	SHAPE_CLIP_PROJECTOR,
	SHAPE_CLIP_PROJECTOR1,
	SHAPE_CLIP_PROJECTOR2,
	SHAPE_SIMPLE_PROJECTOR,
	SHAPE_SPHERE,
	SHAPE_BOX,
	SHAPE_MAX,
};

struct SThreadInfo
{
	uint32              m_PersFlags; // Never reset
	float               m_RealTime;
	class CMatrixStack* m_matView;
	class CMatrixStack* m_matProj;
	CCamera             m_cam;                                   // current camera
	CRenderCamera       m_rcam;                                  // current camera
	int                 m_nFrameID;                              // with recursive calls, access through GetFrameID(true)
	uint32              m_nFrameUpdateID;                        // without recursive calls, access through GetFrameID(false)
	int                 m_arrZonesRoundId[MAX_PREDICTION_ZONES]; // rounds ID from 3D engine, useful for texture streaming
	SFogState           m_FS;
	CRenderObject*      m_pIgnoreObject;

	Plane               m_pObliqueClipPlane;
	bool                m_bObliqueClipPlane;

	byte                m_eCurColorOp;
	byte                m_eCurAlphaOp;
	byte                m_eCurColorArg;
	byte                m_eCurAlphaArg;

	Vec4                m_vFrustumInfo;

	SThreadInfo()
		: m_PersFlags(0)
		, m_RealTime(0.0f)
		, m_matView(nullptr)
		, m_matProj(nullptr)
		, m_nFrameID(0)
		, m_nFrameUpdateID(0)
		, m_pIgnoreObject(nullptr)
		, m_bObliqueClipPlane(false)
		, m_eCurColorOp(0)
		, m_eCurAlphaOp(0)
		, m_eCurColorArg(0)
		, m_eCurAlphaArg(0)
		, m_vFrustumInfo(ZERO)
	{
		ZeroArray(m_arrZonesRoundId);
		ZeroStruct(m_FS);
	}

	~SThreadInfo() {}
	SThreadInfo& operator=(const SThreadInfo& ti)
	{
		if (&ti == this) return *this;
		m_PersFlags = ti.m_PersFlags;
		m_RealTime = ti.m_RealTime;
		m_matView = ti.m_matView;
		m_matProj = ti.m_matProj;
		m_cam = ti.m_cam;
		m_rcam = ti.m_rcam;
		m_nFrameID = ti.m_nFrameID;
		for (int z = 0; z < MAX_PREDICTION_ZONES; z++)
			m_arrZonesRoundId[z] = ti.m_arrZonesRoundId[z];
		m_FS = ti.m_FS;
		m_pIgnoreObject = ti.m_pIgnoreObject;
		memcpy(&m_pObliqueClipPlane, &ti.m_pObliqueClipPlane, sizeof(m_pObliqueClipPlane));
		m_bObliqueClipPlane = ti.m_bObliqueClipPlane;
		memcpy(&m_vFrustumInfo, &ti.m_vFrustumInfo, sizeof(m_vFrustumInfo));
		m_eCurColorOp = ti.m_eCurColorOp;
		m_eCurAlphaOp = ti.m_eCurAlphaOp;
		m_eCurColorArg = ti.m_eCurColorArg;
		m_eCurAlphaArg = ti.m_eCurAlphaArg;
		return *this;
	}
};

#ifdef STRIP_RENDER_THREAD
struct SSingleThreadInfo : public SThreadInfo
{
	SThreadInfo&       operator[](const int)       { return *this; }
	const SThreadInfo& operator[](const int) const { return *this; }
};
#endif

// Render pipeline structure
struct SRenderPipeline
{
	CShader*                             m_pShader;
	CShader*                             m_pReplacementShader;
	CRenderObject*                       m_pCurObject;
	CRenderObject*                       m_pIdendityRenderObject;
	CRendElementBase*                    m_pRE;
	CRendElementBase*                    m_pEventRE;
	int                                  m_RendNumVerts;
	uint32                               m_nBatchFilter; // Batch flags ( FB_ )
	SShaderTechnique*                    m_pRootTechnique;
	SShaderTechnique*                    m_pCurTechnique;
	SShaderPass*                         m_pCurPass;
	uint32                               m_CurPassBitMask;
	int                                  m_nShaderTechnique;
	int                                  m_nShaderTechniqueType;

	CShaderResources*                    m_pShaderResources;
	CRenderObject*                       m_pPrevObject;
	int                                  m_nLastRE;
	TArray<SRendItem*>                   m_RIs[MAX_REND_GEOMS_IN_BATCH];

	ColorF                               m_CurGlobalColor;

	float                                m_fMinDistance; // min distance to texture
	uint64                               m_ObjFlags;     // Instances flag for batch (merged)
	int                                  m_Flags;        // Reset on start pipeline

	EShapeMeshType                       m_nDeferredPrimitiveID;
	Matrix44                             m_newOcclusionCameraProj;
	Matrix44                             m_newOcclusionCameraView;
	Matrix44                             m_OcclusionCameraBuffer[CULLER_MAX_CAMS];
#ifdef CULLER_DEBUG
	int                                  m_OcclusionCameraBufferID[CULLER_MAX_CAMS];
#endif
	int                                  m_nZOcclusionProcess;
	int                                  m_nZOcclusionReady;
	int                                  m_nZOcclusionBufferID;

	threadID                             m_nFillThreadID;
	threadID                             m_nProcessThreadID;
	SThreadInfo                          m_TI[RT_COMMAND_BUF_COUNT];
	SThreadInfo                          m_OldTI[MAX_RECURSION_LEVELS];
	CThreadSafeRendererContainer<ColorF> m_fogVolumeContibutions[RT_COMMAND_BUF_COUNT];

	uint32                               m_PersFlags1;    // Persistent flags - never reset
	uint32                               m_PersFlags2;    // Persistent flags - never reset
	int                                  m_FlagsPerFlush; // Flags which resets for each shader flush
	uint32                               m_nCommitFlags;
	uint32                               m_FlagsStreams_Decl;
	uint32                               m_FlagsStreams_Stream;
	EVertexFormat                        m_CurVFormat;
	uint32                               m_FlagsShader_LT;  // Shader light mask
	uint64                               m_FlagsShader_RT;  // Shader runtime mask
	uint32                               m_FlagsShader_MD;  // Shader texture modificator mask
	uint32                               m_FlagsShader_MDV; // Shader vertex modificator mask
	uint32                               m_nShaderQuality;

	void                                 (* m_pRenderFunc)();

	uint32                               m_CurGPRAllocStateCommit;
	uint32                               m_CurGPRAllocState;
	int                                  m_CurHiZState;

	uint32                               m_CurState;
	uint32                               m_StateOr;
	uint32                               m_StateAnd;
	int                                  m_CurAlphaRef;
	uint32                               m_MaterialStateOr;
	uint32                               m_MaterialStateAnd;
	int                                  m_MaterialAlphaRef;
	uint32                               m_ForceStateOr;
	uint32                               m_ForceStateAnd;
	bool                                 m_bIgnoreObjectAlpha;
	ECull                                m_eCull;

	int                                  m_CurStencilState;
	uint32                               m_CurStencMask;
	uint32                               m_CurStencWriteMask;
	uint32                               m_CurStencRef;
	int                                  m_CurStencilRefAndMask;
	int                                  m_CurStencilCullFunc;

	struct SPipelineStreamInfo
	{
		D3DBuffer* pStream;
		int        nOffset;
		int        nStride;

		SPipelineStreamInfo() {}
		SPipelineStreamInfo(D3DBuffer* stream, int offset, int stride) : pStream(stream), nOffset(offset), nStride(stride) {}
	};

	SPipelineStreamInfo m_VertexStreams[MAX_STREAMS];
	D3DIndexBuffer*     m_pIndexStream;

	uint32              m_IndexStreamOffset;
	RenderIndexType     m_IndexStreamType;

	bool                m_bFirstPass;
	uint32              m_nNumRendPasses;
	int                 m_NumShaderInstructions;

	string              m_sExcludeShader;

	CCamera             m_PrevCamera;

	uint32              m_nRendFlags;
	bool                m_bUseHDR;
	int                 m_nPassGroupID;  // EFSLIST_ pass type
	int                 m_nPassGroupDIP; // EFSLIST_ pass type
	uint32              m_nFlagsShaderBegin;
	uint8               m_nCurrResolveBounds[4];

	Vec2                m_CurDownscaleFactor;

	ERenderQuality      m_eQuality;

	// Array of render objects that need to be deleted next frame
	CThreadSafeRendererContainer<class CPermanentRenderObject*> m_persistentRenderObjectsToDelete[RT_COMMAND_BUF_COUNT];

	SLightPass m_LPasses[MAX_REND_LIGHTS];
	float      m_fProfileTime;

	struct ShadowInfo
	{
		ShadowMapFrustum* m_pCurShadowFrustum;
		Vec3              vViewerPos;
		int               m_nOmniLightSide;
	}              m_ShadowInfo;

	UVertStreamPtr m_StreamPtrTang;
	UVertStreamPtr m_NextStreamPtrTang;

	UVertStreamPtr m_StreamPtr;
	UVertStreamPtr m_NextStreamPtr;
	int            m_StreamStride;
	int            m_StreamOffsetTC;
	int            m_StreamOffsetColor;

	float          m_fLastWaterFOVUpdate;
	Vec3           m_LastWaterViewdirUpdate;
	Vec3           m_LastWaterUpdirUpdate;
	Vec3           m_LastWaterPosUpdate;
	float          m_fLastWaterUpdate;
	int            m_nLastWaterFrameID;

	int            m_nSPIUpdateFrameID;

	SMSAA          m_MSAAData;
	bool           IsMSAAEnabled() const { return m_MSAAData.Type > 0; }

	// light volume data
	CLightVolumeBuffer m_lightVolumeBuffer;

	// particle data for writing directly to VMEM
	ParticleBufferSet                  m_particleBuffer;

	int                                m_nStreamOffset[3]; // deprecated!
	SOnDemandD3DVertexDeclaration      m_D3DVertexDeclaration[eVF_Max];
	SOnDemandD3DVertexDeclarationCache m_D3DVertexDeclarationCache[1 << VSF_NUM][eVF_Max][2]; // [StreamMask][VertexFmt][Morph]
	SOnDemandD3DStreamProperties       m_D3DStreamProperties[VSF_NUM];

	TArray<SVertexDeclaration*>        m_CustomVD;

	uint16*                            m_RendIndices;
	uint16*                            m_SysRendIndices;
	byte*                              m_SysArray;
	int                                m_SizeSysArray;

	TArray<byte>                       m_SysVertexPool[RT_COMMAND_BUF_COUNT];
	TArray<uint16>                     m_SysIndexPool[RT_COMMAND_BUF_COUNT];

	int                                m_RendNumGroup;
	int                                m_RendNumIndices;
	int                                m_FirstIndex;
	int                                m_FirstVertex;

	SEfResTexture*                     m_ShaderTexResources[MAX_TMU];

	int                                m_Frame;
	int                                m_FrameMerge;

	float                              m_fCurOpacity;

	SPipeStat                          m_PS[RT_COMMAND_BUF_COUNT];
	DynArray<SRTargetStat>             m_RTStats;

	int                                m_MaxVerts;
	int                                m_MaxTris;

	int                                m_RECustomTexBind[8];
	int                                m_ShadowCustomTexBind[8];
	bool                               m_ShadowCustomComparisonSampling[8];

	CRenderView*                       RenderView() const { return m_pCurrentRenderView; }
	CRenderView*                       m_pCurrentRenderView;

	// Separate render views per recursion
	_smart_ptr<CRenderView> m_pRenderViews[RT_COMMAND_BUF_COUNT][MAX_REND_RECURSION_LEVELS];

	//===================================================================
	// Input render data
	SRenderLight*                              m_pSunLight;

	CThreadSafeWorkerContainer<CRenderObject*> m_TempObjects[RT_COMMAND_BUF_COUNT];

	CRenderObject*                             m_ObjectsPool;
	uint32 m_nNumObjectsInPool;

	std::shared_ptr<class CRenderObjectsPools> m_renderObjectsPools;

#if !defined(_RELEASE)
	//===================================================================
	// Drawcall count debug view - per Node - r_stats 6
	std::map<struct IRenderNode*, IRenderer::SDrawCallCountInfo> m_pRNDrawCallsInfoPerNode[RT_COMMAND_BUF_COUNT];

	//===================================================================
	// Drawcall count debug view - per mesh - perf hud renderBatchStats
	std::map<struct IRenderMesh*, IRenderer::SDrawCallCountInfo> m_pRNDrawCallsInfoPerMesh[RT_COMMAND_BUF_COUNT];
#endif

	//================================================================
	// Render elements..

	class CREHDRProcess*      m_pREHDR;
	class CREDeferredShading* m_pREDeferredShading;
	class CREPostProcess*     m_pREPostProcess;

	//=================================================================
	// WaveForm tables
	static const uint32 sSinTableCount = 1024;
	float               m_tSinTable[sSinTableCount];

	// For explicit geometry cache motion blur
	Matrix44A* m_pPrevMatrix;

public:
	SRenderPipeline();
	~SRenderPipeline();

	inline SShaderTechnique* GetStartTechnique() const
	{
		if (m_pShader)
			return m_pShader->mfGetStartTechnique(m_nShaderTechnique);
		return NULL;
	}
};

extern CryCriticalSection m_sREResLock;

///////////////////////////////////////////////////////////////////////////////
// sort operators for render items
struct SCompareItemPreprocess
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		if (a.nBatchFlags != b.nBatchFlags)
			return a.nBatchFlags < b.nBatchFlags;

		if (a.SortVal != b.SortVal)
			return a.SortVal < b.SortVal;

		return a.rendItemSorter < b.rendItemSorter;
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareRendItem
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		/// Nearest objects should be rendered first
		int nNearA = (a.ObjSort & FOB_HAS_PREVMATRIX);
		int nNearB = (b.ObjSort & FOB_HAS_PREVMATRIX);
		if (nNearA != nNearB)               // Sort by nearest flag
			return nNearA > nNearB;

		if (a.SortVal != b.SortVal)         // Sort by shaders
			return a.SortVal < b.SortVal;

		if (a.pElem != b.pElem)               // Sort by geometry
			return a.pElem < b.pElem;

		return (a.ObjSort & 0xFFFF) < (b.ObjSort & 0xFFFF);   // Sort by distance
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareRendItemZPass
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		const int layerSize = 50;  // Note: ObjSort contains round(entityDist * 2) for meshes

		// Sort by nearest flag
		int nNearA = (a.ObjSort & FOB_HAS_PREVMATRIX);
		int nNearB = (b.ObjSort & FOB_HAS_PREVMATRIX);
		if (nNearA != nNearB)
			return nNearA > nNearB;

		// Sort by depth/distance layers
		int depthLayerA = (a.ObjSort & 0xFFFF) / layerSize;
		int depthLayerB = (b.ObjSort & 0xFFFF) / layerSize;
		if (depthLayerA != depthLayerB)
			return depthLayerA < depthLayerB;

		if (a.SortVal != b.SortVal)    // Sort by shaders
			return a.SortVal < b.SortVal;

		// Sorting by geometry less important than sorting by shaders
		if (a.pElem != b.pElem)    // Sort by geometry
			return a.pElem < b.pElem;

		return (a.ObjSort & 0xFFFF) < (b.ObjSort & 0xFFFF);    // Sort by distance
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareItem_Decal
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		uint32 objSortA_Low(a.ObjSort & 0xFFFF);
		uint32 objSortA_High(a.ObjSort & ~0xFFFF);
		uint32 objSortB_Low(b.ObjSort & 0xFFFF);
		uint32 objSortB_High(b.ObjSort & ~0xFFFF);

		if (objSortA_Low != objSortB_Low)
			return objSortA_Low < objSortB_Low;

		if (a.SortVal != b.SortVal)
			return a.SortVal < b.SortVal;

		return objSortA_High < objSortB_High;
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareItem_NoPtrCompare
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		if (a.ObjSort != b.ObjSort)
			return a.ObjSort < b.ObjSort;

		float pSurfTypeA = ((float*)a.pElem->m_CustomData)[8];
		float pSurfTypeB = ((float*)b.pElem->m_CustomData)[8];
		if (pSurfTypeA != pSurfTypeB)
			return (pSurfTypeA < pSurfTypeB);

		pSurfTypeA = ((float*)a.pElem->m_CustomData)[9];
		pSurfTypeB = ((float*)b.pElem->m_CustomData)[9];
		if (pSurfTypeA != pSurfTypeB)
			return (pSurfTypeA < pSurfTypeB);

		pSurfTypeA = ((float*)a.pElem->m_CustomData)[11];
		pSurfTypeB = ((float*)b.pElem->m_CustomData)[11];
		return (pSurfTypeA < pSurfTypeB);
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareDist
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		if (fcmp(a.fDist, b.fDist))
			return a.rendItemSorter.ParticleCounter() < b.rendItemSorter.ParticleCounter();

		return (a.fDist > b.fDist);
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareDistInverted
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		if (fcmp(a.fDist, b.fDist))
			return a.rendItemSorter.ParticleCounter() > b.rendItemSorter.ParticleCounter();

		return (a.fDist < b.fDist);
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareByRenderingPass
{
	bool operator()(const SRendItem& rA, const SRendItem& rB) const
	{
		return rA.rendItemSorter.IsRecursivePass() < rB.rendItemSorter.IsRecursivePass();
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareByOnlyStableFlagsOctreeID
{
	bool operator()(const SRendItem& rA, const SRendItem& rB) const
	{
		return rA.rendItemSorter < rB.rendItemSorter;
	}
};

#endif  // __RENDERPIPELINE_H__
