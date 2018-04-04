// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*=============================================================================
   TexMan.h : Common texture manager declarations.

   Revision history:
* Created by Khonich Andrey
   - 19:8:2008   12:14 : * Refactored by Anton Kaplanyan

   =============================================================================*/

#ifndef _TEXTURE_H
#define _TEXTURE_H

#include <set>
#include <atomic>
#include "../../XRenderD3D9/DeviceManager/DeviceFormats.h" // SPixFormat
#include "../ResFile.h"
#include "../CommonRender.h" // CBaseResource, SResourceView, SSamplerState
#include "../Shaders/ShaderResources.h" // EHWShaderClass
#include "../Shaders/CShader.h" // MAX_ENVTEXTURES

#include "PowerOf2BlockPacker.h"
#include <CryMemory/STLPoolAllocator.h>

#include "ITextureStreamer.h"

#include "TextureArrayAlloc.h"
#include "Image/DDSImage.h"
#include <CrySystem/Scaleform/IFlashUI.h>

#include <Cry3DEngine/ImageExtensionHelper.h>

class CDeviceTexture;
class CTexture;
class CImageFile;
struct IFlashPlayer;
struct IUIElement;
struct IUILayoutBase;
class CTextureStreamPoolMgr;
class CReactiveTextureStreamer;
class CMipmapGenPass;

#define TEXPOOL_LOADSIZE 6 * 1024 * 1024

#define MAX_MIP_LEVELS   (100)

// Custom Textures IDs
enum
{
	TO_RT_2D = 1,

	TO_FOG,
	TO_FROMOBJ,
	TO_WINDGRID,
	TO_SVOTREE,
	TO_SVOTRIS,
	TO_SVOGLCM,
	TO_SVORGBS,
	TO_SVONORM,
	TO_SVOOPAC,
	TO_FROMOBJ_CM,

	TO_SHADOWID0,
	TO_SHADOWID1,
	TO_SHADOWID2,
	TO_SHADOWID3,
	TO_SHADOWID4,
	TO_SHADOWID5,
	TO_SHADOWID6,
	TO_SHADOWID7,

	TO_FROMRE0,
	TO_FROMRE1,

	TO_FROMRE0_FROM_CONTAINER,
	TO_FROMRE1_FROM_CONTAINER,

	TO_SCREENMAP,
	TO_SHADOWMASK,
	TO_TERRAIN_LM,
	TO_RT_CM,
	TO_CLOUDS_LM,
	TO_BACKBUFFERMAP,
	TO_PREVBACKBUFFERMAP0,
	TO_PREVBACKBUFFERMAP1,
	TO_MIPCOLORS_DIFFUSE,
	TO_MIPCOLORS_BUMP,

	TO_DOWNSCALED_ZTARGET_FOR_AO,
	TO_QUARTER_ZTARGET_FOR_AO,
	TO_WATEROCEANMAP,
	TO_WATERVOLUMEMAP,

	TO_WATERVOLUMEREFLMAP,
	TO_WATERVOLUMEREFLMAPPREV,
	TO_WATERVOLUMECAUSTICSMAP,
	TO_WATERVOLUMECAUSTICSMAPTEMP,

	TO_COLORCHART,

	TO_ZTARGET_MS,

	TO_SCENE_NORMALMAP,
	TO_SCENE_NORMALMAP_MS,

	TO_SCENE_DIFFUSE_ACC,
	TO_SCENE_SPECULAR_ACC,
	TO_SCENE_DIFFUSE_ACC_MS,
	TO_SCENE_SPECULAR_ACC_MS,
	TO_SCENE_TEXTURES,
	TO_SCENE_TARGET,

	TO_BACKBUFFERSCALED_D2,
	TO_BACKBUFFERSCALED_D4,
	TO_BACKBUFFERSCALED_D8,

	TO_SKYDOME_MIE,
	TO_SKYDOME_RAYLEIGH,
	TO_SKYDOME_MOON,

	TO_VOLFOGSHADOW_BUF,

	TO_HDR_MEASURED_LUMINANCE,
	TO_MODELHUD,
};

#define NUM_HDR_TONEMAP_TEXTURES 4
#define NUM_HDR_BLOOM_TEXTURES   3
#define MIN_DOF_COC_K            6

#if CRY_RENDERER_OPENGLES
	#define MAX_OCCLUSION_READBACK_TEXTURES 2
#else
	#define MAX_OCCLUSION_READBACK_TEXTURES 8
#endif

#define DYNTEXTURE_TEXCACHE_LIMIT 32

inline int LogBaseTwo(int iNum)
{
	int i, n;
	for (i = iNum - 1, n = 0; i > 0; i >>= 1, n++)
		;
	return n;
}

enum EShadowBuffers_Pool
{
	SBP_D16,
	SBP_D24S8,
	SBP_D32FS8,
	SBP_R16G16,
	SBP_MAX,
	SBP_UNKNOWN
};

#define SHADOWS_POOL_SIZE     1024//896 //768
#define TEX_POOL_BLOCKLOGSIZE 5  // 32
#define TEX_POOL_BLOCKSIZE    (1 << TEX_POOL_BLOCKLOGSIZE)

struct SDynTexture_Shadow;

//======================================================================
// Dynamic textures
struct SDynTexture : public IDynTexture
{
	static int         s_nMemoryOccupied;
	static SDynTexture s_Root;

	SDynTexture*       m_Next;                //!<
	SDynTexture*       m_Prev;                //!<
	char               m_sSource[128];        //!< pointer to the given name in the constructor call

	CTexture*          m_pTexture;
	ETEX_Format        m_eTF;
	ETEX_Type          m_eTT;
	uint32             m_nWidth;
	uint32             m_nHeight;
	uint32             m_nReqWidth;
	uint32             m_nReqHeight;
	uint32             m_nTexFlags;
	uint32             m_nFrameReset;

	bool               m_bLocked;
	byte               m_nUpdateMask;

	//////////////////////////////////////////////////////////////////////////
	// Shadow specific vars.
	//////////////////////////////////////////////////////////////////////////
	int                 m_nUniqueID;

	SDynTexture_Shadow* m_NextShadow;           //!<
	SDynTexture_Shadow* m_PrevShadow;           //!<

	ShadowMapFrustum*   m_pFrustumOwner;
	IRenderNode*        pLightOwner;
	int                 nObjectsRenderedCount;
	//////////////////////////////////////////////////////////////////////////

	SDynTexture(const char* szSource);
	SDynTexture(int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char* szSource);
	~SDynTexture();

	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete for pool allocator.
	//////////////////////////////////////////////////////////////////////////
	ILINE void* operator new(size_t nSize);
	ILINE void  operator delete(void* ptr);
	//////////////////////////////////////////////////////////////////////////

	virtual int       GetTextureID();
	virtual bool      Update(int nNewWidth, int nNewHeight);
	bool              RT_Update(int nNewWidth, int nNewHeight);
	virtual void      Apply(int nTUnit, SamplerStateHandle nTS = EDefaultSamplerStates::Unspecified);
	virtual bool      ClearRT();
	virtual bool      SetRT(int nRT, bool bPush, SDepthTexture* pDepthSurf, bool bScreenVP = false);
	virtual bool      RT_SetRT(int nRT, int nWidth, int nHeight, bool bPush, bool bScreenVP = false);
	virtual bool      SetRectStates() { return true; }
	virtual bool      RestoreRT(int nRT, bool bPop);
	virtual ITexture* GetTexture()    { return (ITexture*)m_pTexture; }
	virtual void      Release()       { delete this; }
	virtual void      ReleaseForce();
	virtual void      SetUpdateMask();
	virtual void      ResetUpdateMask();
	virtual bool      IsSecondFrame()  { return m_nUpdateMask == 3; }
	virtual byte      GetFlags() const { return 0; }
	virtual void      GetSubImageRect(uint32& nX, uint32& nY, uint32& nWidth, uint32& nHeight)
	{
		nX = 0;
		nY = 0;
		nWidth = m_nWidth;
		nHeight = m_nHeight;
	}
	virtual void GetImageRect(uint32& nX, uint32& nY, uint32& nWidth, uint32& nHeight);
	virtual int  GetWidth()  { return m_nWidth; }
	virtual int  GetHeight() { return m_nHeight; }

	void         Lock()      { m_bLocked = true; }
	void         UnLock()    { m_bLocked = false; }

	virtual void AdjustRealSize();
	virtual bool IsValid();

	inline void  UnlinkGlobal()
	{
		if (!m_Next || !m_Prev)
			return;
		m_Next->m_Prev = m_Prev;
		m_Prev->m_Next = m_Next;
		m_Next = m_Prev = NULL;
	}
	inline void LinkGlobal(SDynTexture* Before)
	{
		if (m_Next || m_Prev)
			return;
		m_Next = Before->m_Next;
		Before->m_Next->m_Prev = this;
		Before->m_Next = this;
		m_Prev = Before;
	}
	virtual void Unlink()
	{
		UnlinkGlobal();
	}
	virtual void Link()
	{
		LinkGlobal(&s_Root);
	}
	ETEX_Format GetFormat() { return m_eTF; }
	bool        FreeTextures(bool bOldOnly, int nNeedSpace);

	typedef std::multimap<unsigned int, CTexture*, std::less<unsigned int>, stl::STLPoolAllocator<std::pair<const unsigned int, CTexture*>, stl::PoolAllocatorSynchronizationSinglethreaded>>           TextureSubset;
	typedef TextureSubset::iterator                                                                                                                                                               TextureSubsetItor;
	typedef std::multimap<unsigned int, TextureSubset*, std::less<unsigned int>, stl::STLPoolAllocator<std::pair<const  unsigned int, TextureSubset*>, stl::PoolAllocatorSynchronizationSinglethreaded>> TextureSet;
	typedef TextureSet::iterator                                                                                                                                                                  TextureSetItor;

	static TextureSet    s_availableTexturePool2D_BC1;
	static TextureSubset s_checkedOutTexturePool2D_BC1;
	static TextureSet    s_availableTexturePool2D_R8G8B8A8;
	static TextureSubset s_checkedOutTexturePool2D_R8G8B8A8;
	static TextureSet    s_availableTexturePool2D_R32F;
	static TextureSubset s_checkedOutTexturePool2D_R32F;
	static TextureSet    s_availableTexturePool2D_R16F;
	static TextureSubset s_checkedOutTexturePool2D_R16F;

	static TextureSet    s_availableTexturePool2D_R16G16F;
	static TextureSubset s_checkedOutTexturePool2D_R16G16F;
	static TextureSet    s_availableTexturePool2D_R8G8B8A8S;
	static TextureSubset s_checkedOutTexturePool2D_R8G8B8A8S;
	static TextureSet    s_availableTexturePool2D_R16G16B16A16F;
	static TextureSubset s_checkedOutTexturePool2D_R16G16B16A16F;
	static TextureSet    s_availableTexturePool2D_R10G10B10A2;
	static TextureSubset s_checkedOutTexturePool2D_R10G10B10A2;

	static TextureSet    s_availableTexturePool2D_R11G11B10F;
	static TextureSubset s_checkedOutTexturePool2D_R11G11B10F;
	static TextureSet    s_availableTexturePoolCube_R11G11B10F;
	static TextureSubset s_checkedOutTexturePoolCube_R11G11B10F;
	static TextureSet    s_availableTexturePool2D_R8G8S;
	static TextureSubset s_checkedOutTexturePool2D_R8G8S;
	static TextureSet    s_availableTexturePoolCube_R8G8S;
	static TextureSubset s_checkedOutTexturePoolCube_R8G8S;

	static TextureSet    s_availableTexturePool2D_Shadows[SBP_MAX];
	static TextureSubset s_checkedOutTexturePool2D_Shadows[SBP_MAX];
	static TextureSet    s_availableTexturePoolCube_Shadows[SBP_MAX];
	static TextureSubset s_checkedOutTexturePoolCube_Shadows[SBP_MAX];

	static TextureSet    s_availableTexturePool2DCustom_R16G16F;
	static TextureSubset s_checkedOutTexturePool2DCustom_R16G16F;

	static TextureSet    s_availableTexturePoolCube_BC1;
	static TextureSubset s_checkedOutTexturePoolCube_BC1;
	static TextureSet    s_availableTexturePoolCube_R8G8B8A8;
	static TextureSubset s_checkedOutTexturePoolCube_R8G8B8A8;
	static TextureSet    s_availableTexturePoolCube_R32F;
	static TextureSubset s_checkedOutTexturePoolCube_R32F;
	static TextureSet    s_availableTexturePoolCube_R16F;
	static TextureSubset s_checkedOutTexturePoolCube_R16F;
	static TextureSet    s_availableTexturePoolCube_R16G16F;
	static TextureSubset s_checkedOutTexturePoolCube_R16G16F;
	static TextureSet    s_availableTexturePoolCube_R8G8B8A8S;
	static TextureSubset s_checkedOutTexturePoolCube_R8G8B8A8S;
	static TextureSet    s_availableTexturePoolCube_R16G16B16A16F;
	static TextureSubset s_checkedOutTexturePoolCube_R16G16B16A16F;

	static TextureSet    s_availableTexturePoolCube_R10G10B10A2;
	static TextureSubset s_checkedOutTexturePoolCube_R10G10B10A2;

	static TextureSet    s_availableTexturePoolCubeCustom_R16G16F;
	static TextureSubset s_checkedOutTexturePoolCubeCustom_R16G16F;

	static uint32        s_iNumTextureBytesCheckedOut;
	static uint32        s_iNumTextureBytesCheckedIn;

	static uint32        s_SuggestedDynTexAtlasCloudsMaxsize;
	static uint32        s_SuggestedDynTexAtlasSpritesMaxsize;
	static uint32        s_SuggestedTexAtlasSize;
	static uint32        s_SuggestedDynTexMaxSize;

	static uint32        s_CurDynTexAtlasCloudsMaxsize;
	static uint32        s_CurDynTexAtlasSpritesMaxsize;
	static uint32        s_CurTexAtlasSize;
	static uint32        s_CurDynTexMaxSize;

	CTexture*           CreateDynamicRT();
	CTexture*           GetDynamicRT();
	void                ReleaseDynamicRT(bool bForce);

	EShadowBuffers_Pool ConvertTexFormatToShadowsPool(ETEX_Format e);

	static bool         FreeAvailableDynamicRT(int nNeedSpace, TextureSet* pSet, bool bOldOnly);

	static void         ShutDown();

	static void         Init();
	static void         Tick();

	void                GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};

typedef stl::PoolAllocatorNoMT<sizeof(SDynTexture)> SDynTexture_PoolAlloc;
extern SDynTexture_PoolAlloc* g_pSDynTexture_PoolAlloc;

//////////////////////////////////////////////////////////////////////////
// Custom new/delete for pool allocator.
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
inline void* SDynTexture::operator new(size_t nSize)
{
	void* ptr = g_pSDynTexture_PoolAlloc->Allocate();
	if (ptr)
		memset(ptr, 0, nSize); // Clear objects memory.
	return ptr;
}
//////////////////////////////////////////////////////////////////////////
inline void SDynTexture::operator delete(void* ptr)
{
	if (ptr)
		g_pSDynTexture_PoolAlloc->Deallocate(ptr);
}

//==============================================================================

enum ETexPool
{
	eTP_Clouds,
	eTP_Sprites,
	eTP_VoxTerrain,
	eTP_DynTexSources,

	eTP_Max
};

struct STextureSetFormat
{
	struct SDynTexture2*               m_pRoot;
	ETEX_Format                        m_eTF;
	ETexPool                           m_eTexPool;
	ETEX_Type                          m_eTT;
	int                                m_nTexFlags;
	std::vector<CPowerOf2BlockPacker*> m_TexPools;

	STextureSetFormat(ETEX_Format eTF, ETexPool eTexPool, uint32 nTexFlags)
	{
		m_eTF = eTF;
		m_eTexPool = eTexPool;
		m_eTT = eTT_2D;
		m_pRoot = NULL;
		m_nTexFlags = nTexFlags;
	}
	~STextureSetFormat();
};

struct SDynTexture2 : public IDynTexture
{
#ifndef _DEBUG
	char*                 m_sSource;          //!< pointer to the given name in the constructor call
#else
	char                  m_sSource[128];         //!< pointer to the given name in the constructor call
#endif
	STextureSetFormat*    m_pOwner;
	CPowerOf2BlockPacker* m_pAllocator;
	CTexture*             m_pTexture;
	uint32                m_nBlockID;
	ETexPool              m_eTexPool;

	SDynTexture2*         m_Next;             //!<
	SDynTexture2**        m_PrevLink;         //!<

	inline void UnlinkGlobal()
	{
		if (m_Next)
			m_Next->m_PrevLink = m_PrevLink;
		if (m_PrevLink)
			*m_PrevLink = m_Next;
	}
	inline void LinkGlobal(SDynTexture2*& Before)
	{
		if (Before)
			Before->m_PrevLink = &m_Next;
		m_Next = Before;
		m_PrevLink = &Before;
		Before = this;
	}
	void Link()
	{
		LinkGlobal(m_pOwner->m_pRoot);
	}
	void Unlink()
	{
		UnlinkGlobal();
		m_Next = NULL;
		m_PrevLink = NULL;
	}
	bool Remove();

	uint32 m_nX;
	uint32 m_nY;
	uint32 m_nWidth;
	uint32 m_nHeight;

	bool   m_bLocked;
	byte   m_nFlags;
	byte   m_nUpdateMask;                   // Crossfire odd/even frames
	uint32 m_nFrameReset;
	uint32 m_nAccessFrame;
	virtual bool IsValid();
	inline bool  _IsValid()
	{
		return IsValid();
	}

	SDynTexture2(uint32 nWidth, uint32 nHeight, uint32 nTexFlags, const char* szSource, ETexPool eTexPool);
	SDynTexture2(const char* szSource, ETexPool eTexPool);
	~SDynTexture2();

	bool              UpdateAtlasSize(int nNewWidth, int nNewHeight);
	void              ReleaseForce();

	virtual bool      Update(int nNewWidth, int nNewHeight);
	virtual void      Apply(int nTUnit, SamplerStateHandle nTS = EDefaultSamplerStates::Unspecified);
	virtual bool      ClearRT();
	virtual bool      SetRT(int nRT, bool bPush, SDepthTexture* pDepthSurf, bool bScreenVP = false);
	virtual bool      SetRectStates();
	virtual bool      RestoreRT(int nRT, bool bPop);
	virtual ITexture* GetTexture() { return (ITexture*)m_pTexture; }
	ETEX_Format       GetFormat()  { return m_pOwner->m_eTF; }
	virtual void      SetUpdateMask();
	virtual void      ResetUpdateMask();
	virtual bool      IsSecondFrame()      { return m_nUpdateMask == 3; }
	virtual byte      GetFlags() const     { return m_nFlags; }
	virtual void      SetFlags(byte flags) { m_nFlags = flags; }

	// IDynTexture implementation
	virtual void Release() { delete this; }
	virtual void GetSubImageRect(uint32& nX, uint32& nY, uint32& nWidth, uint32& nHeight)
	{
		nX = m_nX;
		nY = m_nY;
		nWidth = m_nWidth;
		nHeight = m_nHeight;
	}
	virtual void GetImageRect(uint32& nX, uint32& nY, uint32& nWidth, uint32& nHeight);
	virtual int  GetTextureID();
	virtual void Lock()      { m_bLocked = true; }
	virtual void UnLock()    { m_bLocked = false; }
	virtual int  GetWidth()  { return m_nWidth; }
	virtual int  GetHeight() { return m_nHeight; }

	typedef std::map<uint32, STextureSetFormat*> TextureSet2;
	typedef TextureSet2::iterator                TextureSet2Itor;
	static TextureSet2 s_TexturePool[eTP_Max];
	static int         s_nMemoryOccupied[eTP_Max];

	static void        ShutDown();
	static void        Init(ETexPool eTexPool);
	static int         GetPoolMaxSize(ETexPool eTexPool);
	static void        SetPoolMaxSize(ETexPool eTexPool, int nSize, bool bWarn);
	static const char* GetPoolName(ETexPool eTexPool);
	static ETEX_Format GetPoolTexFormat(ETexPool eTexPool);
	static int         GetPoolTexNum(ETexPool eTexPool)
	{
		int nT = 0;
		for (TextureSet2Itor it = s_TexturePool[eTexPool].begin(); it != s_TexturePool[eTexPool].end(); ++it)
		{
			STextureSetFormat* pF = it->second;
			nT += pF->m_TexPools.size();
		}
		return nT;
	}
};

//////////////////////////////////////////////////////////////////////////
// Dynamic texture for the shadow.
// This class must not contain any non static member variables,
//  because SDynTexture allocated used constant size pool.
//////////////////////////////////////////////////////////////////////////
struct SDynTexture_Shadow : public SDynTexture
{
	static SDynTexture_Shadow s_RootShadow;

	//////////////////////////////////////////////////////////////////////////
	SDynTexture_Shadow(int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char* szSource);
	SDynTexture_Shadow(const char* szSource);
	~SDynTexture_Shadow();

	inline void UnlinkShadow()
	{
		if (!m_NextShadow || !m_PrevShadow)
			return;
		m_NextShadow->m_PrevShadow = m_PrevShadow;
		m_PrevShadow->m_NextShadow = m_NextShadow;
		m_NextShadow = m_PrevShadow = NULL;
	}
	inline void LinkShadow(SDynTexture_Shadow* Before)
	{
		if (m_NextShadow || m_PrevShadow)
			return;
		m_NextShadow = Before->m_NextShadow;
		Before->m_NextShadow->m_PrevShadow = this;
		Before->m_NextShadow = this;
		m_PrevShadow = Before;
	}

	SDynTexture_Shadow* GetByID(int nID)
	{
		SDynTexture_Shadow* pTX = SDynTexture_Shadow::s_RootShadow.m_NextShadow;
		for (pTX = SDynTexture_Shadow::s_RootShadow.m_NextShadow; pTX != &SDynTexture_Shadow::s_RootShadow; pTX = pTX->m_NextShadow)
		{
			if (pTX->m_nUniqueID == nID)
				return pTX;
		}
		return NULL;
	}
	static void  RT_EntityDelete(IRenderNode* pRenderNode);

	virtual void Unlink()
	{
		UnlinkGlobal();
		UnlinkShadow();
	}
	virtual void Link()
	{
		LinkGlobal(&s_Root);
		LinkShadow(&s_RootShadow);
	}
	virtual void AdjustRealSize();

	void         GetMemoryUsage(ICrySizer* pSizer) const
	{
		SDynTexture::GetMemoryUsage(pSizer);
	}

	static SDynTexture_Shadow* GetForFrustum(const ShadowMapFrustum* pFrustum);

	static void                ShutDown();
};

struct SDynTextureArray/* : public SDynTexture*/
{

	//SDynTexture members

	//////////////////////////////////////////////////////////////////////////
	char        m_sSource[128];               //!< pointer to the given name in the constructor call

	CTexture*   m_pTexture;
	ETEX_Format m_eTF;
	ETEX_Type   m_eTT;
	uint32      m_nWidth;
	uint32      m_nHeight;
	uint32      m_nReqWidth;
	uint32      m_nReqHeight;
	int         m_nTexFlags;
	int         m_nPool;
	uint32      m_nFrameReset;

	bool        m_bLocked;
	byte        m_nUpdateMask;

	//////////////////////////////////////////////////////////////////////////
	// Shadow specific vars.
	//////////////////////////////////////////////////////////////////////////
	IRenderNode* pLightOwner;
	int          nObjectsRenderedCount;

	int          m_nUniqueID;
	//////////////////////////////////////////////////////////////////////////

	uint32 m_nArraySize;

	SDynTextureArray(int nWidth, int nHeight, int nArraySize, ETEX_Format eTF, int nTexFlags, const char* szSource);
	~SDynTextureArray();

	bool Update(int nNewWidth, int nNewHeight);

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};

//==========================================================================
// Texture

struct STexCacheFileHeader
{
	int8 m_nSides;
	int8 m_nMipsPersistent;
	STexCacheFileHeader()
	{
		m_nSides = 0;
		m_nMipsPersistent = 0;
	}
};

struct SMipData
{
public:
	byte* DataArray; // Data.
	bool  m_bNative : 1;

	SMipData()
		: m_bNative(false)
		, DataArray(NULL)
	{}
	~SMipData()
	{
		Free();
	}

	void Free()
	{
		SAFE_DELETE_ARRAY(DataArray);
		m_bNative = false;
	}
	void Init(int InSize, int nWidth, int nHeight)
	{
		assert(DataArray == NULL);
		DataArray = new byte[InSize];
		m_bNative = false;
	}
};

struct STexPool;
struct STexPoolItem;

struct STexStreamRoundInfo
{
	STexStreamRoundInfo()
	{
		nRoundUpdateId = -1;
		bHighPriority = false;
		bLastHighPriority = false;
	}

	int32  nRoundUpdateId    : 30;
	uint32 bHighPriority     : 1;
	uint32 bLastHighPriority : 1;
};

struct STexStreamZoneInfo
{
	STexStreamZoneInfo()
	{
		fMinMipFactor = fLastMinMipFactor = 1000000.f;
	}

	float fMinMipFactor;
	float fLastMinMipFactor;
};

struct STexMipHeader
{
	uint32    m_SideSizeWithMips  : 31;
	uint32    m_InPlaceStreamable : 1;
	uint32    m_SideSize          : 29;
	uint32    m_eMediaType        : 3;
#if defined(TEXSTRM_STORE_DEVSIZES)
	uint32    m_DevSideSizeWithMips;
#endif
	SMipData* m_Mips;
	STexMipHeader()
	{
		m_SideSizeWithMips = 0;
		m_InPlaceStreamable = 0;
		m_SideSize = 0;
		m_eMediaType = 0;
#if defined(TEXSTRM_STORE_DEVSIZES)
		m_DevSideSizeWithMips = 0;
#endif
		m_Mips = NULL;
	}
	~STexMipHeader()
	{
		SAFE_DELETE_ARRAY(m_Mips);
	}
};

struct STexStreamingInfo
{
	STexStreamZoneInfo   m_arrSPInfo[MAX_STREAM_PREDICTION_ZONES];

	STexPoolItem*        m_pPoolItem;

	uint32               m_nSrcStart;

	STexMipHeader*       m_pMipHeader;
	DDSSplitted::DDSDesc m_desc;
	float                m_fMinMipFactor;

	STexStreamingInfo(){}
	STexStreamingInfo(const uint8 nMips)
	{
		m_pPoolItem = NULL;
		// +1 to accomodate queries for the size of the texture with no mips
		m_pMipHeader = new STexMipHeader[nMips + 1];
		m_nSrcStart = 0;
		m_fMinMipFactor = 0.0f;
	}

	~STexStreamingInfo()
	{
		SAFE_DELETE_ARRAY(m_pMipHeader);
	}

	void GetMemoryUsage(ICrySizer* pSizer, int nMips, int nSides) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_pMipHeader, sizeof(STexMipHeader) * nMips);
		for (int i = 0; i < nMips; i++)
		{
			pSizer->AddObject(m_pMipHeader[i].m_Mips, sizeof(SMipData) * nSides);
			for (int j = 0; j < nSides; j++)
				pSizer->AddObject(m_pMipHeader[i].m_Mips[j].DataArray, m_pMipHeader[i].m_SideSize);
		}
	}

	int GetSize(int nMips, int nSides)
	{
		int nSize = sizeof(*this);
		for (int i = 0; i < nMips; i++)
		{
			nSize += sizeof(m_pMipHeader[i]);
			for (int j = 0; j < nSides; j++)
			{
				nSize += sizeof(m_pMipHeader[i].m_Mips[j]);
				if (m_pMipHeader[i].m_Mips[j].DataArray)
					nSize += m_pMipHeader[i].m_SideSize;
			}
		}
		return nSize;
	}

	void* operator new(size_t sz, void* p)    { return p; }
	void  operator delete(void* ptr, void* p) {}

private:
	void* operator new(size_t sz);
	void  operator delete(void* ptr) { __debugbreak(); }
};

struct STexStreamInMipState
{
	uint32 m_nSideDelta     : 28;
	bool   m_bStreamInPlace : 1; // If true, the i/o reads the DDS contents directly into the texture surface and subsequent operations are conducted in-place
	bool   m_bExpanded      : 1;
	bool   m_bUploaded      : 1;

	STexStreamInMipState()
	{
		m_nSideDelta = 0;
		m_bStreamInPlace = false;
		m_bExpanded = false;
		m_bUploaded = false;
	}
};

struct STexStreamInState : public IStreamCallback
{
public:
	enum
	{
		MaxMips    = 12,
		MaxStreams = MaxMips * 6,
	};

public:
	STexStreamInState();

	void         Reset();
	bool         TryCommit();
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError);

#ifdef TEXSTRM_ASYNC_TEXCOPY
	void CopyMips();
#endif

public:
	CTexture*     m_pTexture;
	STexPoolItem* m_pNewPoolItem;
	volatile int  m_nAsyncRefCount;
	uint8         m_nHigherUploadedMip;
	uint8         m_nLowerUploadedMip;
	uint8         m_nActivateMip;
	bool          m_bAborted;
	bool          m_bPinned;
	bool          m_bValidLowMips;
	volatile bool m_bAllStreamsComplete;
#if defined(TEXSTRM_COMMIT_COOLDOWN)
	int           m_nStallFrames;
#endif

#ifndef _RELEASE
	float m_fStartTime;
#endif
#if defined(TEXSTRM_DEFERRED_UPLOAD)
	ID3D11CommandList* m_pCmdList;
#endif

#if CRY_PLATFORM_DURANGO
	UINT64 m_tileFence;
	UINT64 m_copyMipsFence;
#endif

	IReadStreamPtr       m_pStreams[MaxStreams];
	STexStreamInMipState m_mips[MaxMips];
};

struct STexStreamPrepState : IImageFileStreamCallback
{
public:
	STexStreamPrepState()
		: m_bCompleted(false)
		, m_bFailed(false)
		, m_bNeedsFinalise(false)
	{
	}

	bool Commit();

public: // IImageFileStreamCallback Members
	virtual void OnImageFileStreamComplete(CImageFile* pImFile);

public:
	_smart_ptr<CTexture>   m_pTexture;
	_smart_ptr<CImageFile> m_pImage;
	volatile bool          m_bCompleted;
	volatile bool          m_bFailed;
	volatile bool          m_bNeedsFinalise;

private:
	STexStreamPrepState(const STexStreamPrepState&);
	STexStreamPrepState& operator=(const STexStreamPrepState&);
};

#ifdef TEXSTRM_ASYNC_TEXCOPY
struct STexStreamOutState
{
public:
	void Reset();
	bool TryCommit();

	void CopyMips();

public:
	JobManager::SJobState m_jobState;

	CTexture*             m_pTexture;
	STexPoolItem*         m_pNewPoolItem;
	uint8                 m_nStartMip;
	volatile bool         m_bDone;
	volatile bool         m_bAborted;

	#if CRY_PLATFORM_DURANGO
	UINT64 m_copyFence;
	#endif
};
#endif

//===================================================================

struct SEnvTexture
{
	bool         m_bInprogress;
	bool         m_bReady;
	bool         m_bWater;
	bool         m_bReflected;
	bool         m_bReserved;
	int          m_MaskReady;
	int          m_Id;
	int          m_TexSize;
	// Result Cube-Map or 2D RT texture
	SDynTexture* m_pTex;
	float        m_TimeLastUpdated;
	Vec3         m_CamPos;
	Vec3         m_ObjPos;
	Ang3         m_Angle;
	int          m_nFrameReset;
	Matrix44     m_Matrix;
	//void *m_pQuery[6];
	//void *m_pRenderTargets[6];

	// Cube maps average colors (used for RT radiosity)
	int    m_nFrameCreated[6];
	ColorF m_EnvColors[6];

	SEnvTexture()
	{
		m_bInprogress = false;
		m_bReady = false;
		m_bWater = false;
		m_bReflected = false;
		m_bReserved = false;
		m_MaskReady = 0;
		m_Id = 0;
		m_TexSize = 0;
		m_pTex = NULL;
		m_TimeLastUpdated = 0.0f;
		m_nFrameReset = -1;
		m_CamPos.Set(0.0f, 0.0f, 0.0f);
		m_ObjPos.Set(0.0f, 0.0f, 0.0f);
		m_Angle.Set(0.0f, 0.0f, 0.0f);

		for (int i = 0; i < 6; i++)
		{
			//m_pQuery[i] = NULL;
			m_nFrameCreated[i] = -1;
			//m_pRenderTargets[i] = NULL;
		}
	}

	void Release();
	void ReleaseDeviceObjects();
	void RT_SetMatrix();
};

//===============================================================================

struct STexStageInfo
{
	int                m_nCurState;
	CDeviceTexture*    m_DevTexture;

	SSamplerState      m_State;
	float              m_fMipBias;

	D3DShaderResource* m_pCurResView;
	EHWShaderClass     m_eHWSC;

	STexStageInfo()
	{
		Flush();
	}
	void Flush()
	{
		m_nCurState = -1;
		m_State.m_nMipFilter = -1;
		m_State.m_nMinFilter = -1;
		m_State.m_nMagFilter = -1;
		m_State.m_nAddressU = eSamplerAddressMode_Wrap;
		m_State.m_nAddressV = eSamplerAddressMode_Wrap;
		m_State.m_nAddressW = eSamplerAddressMode_Wrap;
		m_State.m_nAnisotropy = 0;

		m_pCurResView = NULL;
		m_eHWSC = eHWSC_Pixel;

		m_DevTexture = NULL;
		m_fMipBias = 0.f;
	}
};


struct SDepthTexture
{
	int              nWidth;
	int              nHeight;
	bool             bBusy;
	int              nFrameAccess;
	D3DTexture*      pTarget;
	D3DDepthSurface* pSurface;
	CTexture*        pTexture;

	SDepthTexture()
		: nWidth(0)
		, nHeight(0)
		, bBusy(false)
		, nFrameAccess(-1)
		, pTarget(nullptr)
		, pSurface(nullptr)
		, pTexture(nullptr)
	{}

	~SDepthTexture();

	void Release(bool bReleaseTexture);
};

//////////////////////////////////////////////////////////////////////////

struct SStreamFormatCodeKey
{
	union
	{
		struct
		{
			uint16      nWidth;
			uint16      nHeight;
			ETEX_Format fmt;
			uint8       nTailMips;
		}      s;
		uint64 u;
	};

	SStreamFormatCodeKey(uint32 nWidth, uint32 nHeight, ETEX_Format fmt, uint8 nTailMips)
	{
		u = 0;
		s.nWidth = (uint16)nWidth;
		s.nHeight = (uint16)nHeight;
		s.fmt = fmt;
		s.nTailMips = nTailMips;
	}

	friend bool operator<(const SStreamFormatCodeKey& a, const SStreamFormatCodeKey& b)
	{
		return a.u < b.u;
	}
};

struct SStreamFormatSize
{
	uint32 size        : 31;
	uint32 alignSlices : 1;
};

struct SStreamFormatCode
{
	enum { MaxMips = 14 };
	SStreamFormatSize sizes[MaxMips];
};

//////////////////////////////////////////////////////////////////////////

class CTexture : public ITexture, public CBaseResource
{
	friend struct SDynTexture;
	friend class CD3D9Renderer;
	friend struct STexStreamInState;
	friend struct STexStreamOutState;
	friend class ITextureStreamer;
	friend class CPlanningTextureStreamer;
	friend struct SPlanningTextureOrder;
	friend struct SPlanningTextureRequestOrder;
	friend struct STexPoolItem;
	friend class CReactiveTextureStreamer;
	friend struct CompareStreamingPrioriry;
	friend struct CompareStreamingPrioriryInv;

private:
	CDeviceTexture*   m_pDevTexture;
	const SPixFormat* m_pPixelFormat;

	// Start block that should fit in one cache-line
	// Reason is to minimize cache misses during texture streaming update job
	// Note: This is checked static_assert in ~CTexture implementation (Texture.cpp)

	STexStreamingInfo*  m_pFileTexMips;   // properties of the streamable texture ONLY

	uint16              m_nWidth;
	uint16              m_nHeight;
	uint16              m_nDepth;
	ETEX_Format         m_eDstFormat;
	ETEX_TileMode       m_eSrcTileMode;

	uint32              m_eFlags;     // e.g. FT_USAGE_DYNAMIC

	bool                m_bStreamed             : 1;
	bool                m_bStreamPrepared       : 1;
	bool                m_bStreamRequested      : 1;
	bool                m_bWasUnloaded          : 1;
	bool                m_bPostponed            : 1;
	bool                m_bForceStreamHighRes   : 1;
	bool                m_bIsLocked             : 1;
	bool                m_bNeedRestoring        : 1;

	bool                m_bNoTexture            : 1;
	bool                m_bResolved             : 1;
	bool                m_bUseMultisampledRTV   : 1; // Allows switching rendering between multisampled/non-multisampled rendertarget views
	bool                m_bHighQualityFiltering : 1;
	bool                m_bCustomFormat         : 1; // Allow custom texture formats - for faster texture fetches
	bool                m_bVertexTexture        : 1;
	bool                m_bUseDecalBorderCol    : 1;
	bool                m_bIsSRGB               : 1;

	bool                m_bNoDevTexture         : 1;
	bool                m_bAsyncDevTexCreation  : 1;
	bool                m_bInDistanceSortedList : 1;
	bool                m_bCreatedInLevel       : 1;
	bool                m_bUsedRecently         : 1;
	bool                m_bStatTracked          : 1;
	bool                m_bStreamHighPriority   : 1;

	uint8               m_nStreamFormatCode;
	int8                m_nCustomID;

	uint16              m_nArraySize;
	int8                m_nMips;
	ETEX_Type           m_eTT;

	ETEX_Format         m_eSrcFormat;
	int8                m_nStreamingPriority;
	int8                m_nMinMipVidUploaded;
	int8                m_nMinMipVidActive;

	uint16              m_nStreamSlot;
	int16               m_fpMinMipCur;

	STexCacheFileHeader m_CacheFileHeader;
	SamplerStateHandle  m_nDefState;

	int32               m_nDevTextureSize;
	int32               m_nPersistentSize;

	int                 m_nAccessFrameID; // last read access, compare with GetFrameID(false)
	STexStreamRoundInfo m_streamRounds[MAX_STREAM_PREDICTION_ZONES];

	// End block that should fit in one cache-line

	DynArray<STexComposition> m_composition;
	float                     m_fCurrentMipBias; // streaming mip fading
	float                     m_fAvgBrightness;
	ColorF                    m_cMinColor;
	ColorF                    m_cMaxColor;
	ColorF                    m_cClearColor;

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	uint32                    m_nDeviceAddressInvalidated;
#endif
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	int32                     m_nESRAMOffset;
#endif

	string                    m_SrcName;

#ifndef _RELEASE
	string m_sAssetScopeName;
#endif

public:
	int m_nUpdateFrameID;         // last write access, compare with GetFrameID(false)

private:

#ifdef TEXTURE_GET_SYSTEM_COPY_SUPPORT
	struct SLowResSystemCopy
	{
		SLowResSystemCopy()
		{
			m_nLowResSystemCopyAtlasId = 0;
			m_nLowResCopyWidth = m_nLowResCopyHeight = 0;
		}
		PodArray<ColorB> m_lowResSystemCopy;
		uint16           m_nLowResCopyWidth, m_nLowResCopyHeight;
		int              m_nLowResSystemCopyAtlasId;
	};
	typedef std::map<const CTexture*, SLowResSystemCopy> LowResSystemCopyType;
	static LowResSystemCopyType s_LowResSystemCopy;
	void          PrepareLowResSystemCopy(byte* pTexData, bool bTexDataHasAllMips);
	const ColorB* GetLowResSystemCopy(uint16& nWidth, uint16& nHeight, int** ppLowResSystemCopyAtlasId);
#endif

	static CCryNameTSCRC GenName(const char* name, uint32 nFlags = 0);
	static CTexture*     FindOrRegisterTextureObject(const char* name, uint32 nFlags, ETEX_Format eTFDst, bool& bFound); // NOTE: increases refcount

	ETEX_Format          FormatFixup(ETEX_Format eFormat);
	bool                 FormatFixup(STexData& td);
	bool                 ImagePreprocessing(STexData& td);

#ifndef _RELEASE
	static void OutputDebugInfo();
#endif

	static CCryNameTSCRC s_sClassName;

protected:
	virtual ~CTexture();

public:
	CTexture(const uint32 nFlags, const ColorF& clearColor = ColorF(Clr_Empty), CDeviceTexture* devTexToOwn = nullptr);

	// ITexture interface
	virtual int AddRef() final { return CBaseResource::AddRef(); }
	virtual int Release() final
	{
		if (!(m_eFlags & FT_DONT_RELEASE) || IsLocked())
		{
			return CBaseResource::Release();
		}

		return -1;
	}
	virtual int ReleaseForce()
	{
		m_eFlags &= ~FT_DONT_RELEASE;
		int nRef = 0;
		while (true)
		{
			nRef = Release();
#if !defined(_RELEASE) && defined(_DEBUG)
			IF (nRef < 0, 0)
				__debugbreak();
#endif
			if (nRef == 0)
				break;
		}
		return nRef;
	}

	inline STextureLayout GetLayout() const
	{
		assert((m_eTT != eTT_Cube && m_eTT != eTT_CubeArray) || !(m_nArraySize % 6));
		const STextureLayout Layout =
		{
			m_pPixelFormat,
			m_nWidth, m_nHeight, m_nDepth, m_nArraySize, m_nMips,
			m_eSrcFormat, m_eDstFormat, m_eTT,
			/* TODO: change FT_... to CDeviceObjectFactory::... */
			m_eFlags, m_bIsSRGB,
			m_cClearColor,
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
			m_nESRAMOffset
#endif
		};

		return Layout;
	}


	virtual const char*     GetName() const                    { return GetSourceName(); }
	virtual const int       GetWidth() const                   { return m_nWidth; }
	ILINE const int         GetWidthNonVirtual() const         { return m_nWidth; }
	virtual const int       GetHeight() const                  { return m_nHeight; }
	ILINE const int         GetHeightNonVirtual() const        { return m_nHeight; }
	virtual const int       GetDepth() const                   { return m_nDepth; }
	ILINE const int         GetDepthNonVirtual() const         { return m_nDepth; }
	ILINE const int         GetNumSides() const                { return m_CacheFileHeader.m_nSides; }
	ILINE const int8        GetNumPersistentMips() const       { return m_CacheFileHeader.m_nMipsPersistent; }
	ILINE const bool        IsForceStreamHighRes() const       { return m_bForceStreamHighRes; }
	ILINE const bool        IsStreamHighPriority() const       { return m_bStreamHighPriority; }
	virtual const int       GetTextureID() const;
	void                    SetFlags(uint32 nFlags)            { m_eFlags = nFlags; }
	virtual const uint32    GetFlags() const                   { return m_eFlags; }
	ILINE const int         GetNumMipsNonVirtual() const       { return m_nMips; }
	virtual const int       GetNumMips() const                 { return m_nMips; }
	virtual const int       GetRequiredMip() const             { return max(0, m_fpMinMipCur >> 8); }
	ILINE const int         GetRequiredMipNonVirtual() const   { return max(0, m_fpMinMipCur >> 8); }
	ILINE const int         GetRequiredMipNonVirtualFP() const { return m_fpMinMipCur; }
	virtual const ETEX_Type GetTextureType() const;
	// TODO: deprecate global state based sampler state configuration
	virtual void            SetClamp(bool bEnable)
	{
		ESamplerAddressMode nMode = bEnable ? eSamplerAddressMode_Clamp : eSamplerAddressMode_Wrap;
		SSamplerState::SetDefaultClampingMode(nMode, nMode, nMode);
	}
	virtual const bool IsTextureLoaded() const { return IsLoaded(); }
	virtual void       PrecacheAsynchronously(float fMipFactor, int nFlags, int nUpdateId, int nCounter = 1);
	virtual byte*      GetData32(int nSide = 0, int nLevel = 0, byte* pDst = NULL, ETEX_Format eDstFormat = eTF_R8G8B8A8);
	virtual const int  GetDeviceDataSize() const { return m_nDevTextureSize; }
	virtual const int  GetDataSize() const
	{
		if (IsStreamed())
			return StreamComputeSysDataSize(0);
		return m_nDevTextureSize;
	}
	// TODO: deprecate global state based sampler state configuration
	virtual bool          SetFilter(int nFilter) { return SSamplerState::SetDefaultFilterMode(nFilter); }
	virtual bool          Clear();
	virtual bool          Clear(const ColorF& color);
	virtual float         GetAvgBrightness() const                 { return m_fAvgBrightness; }
	virtual void          SetAvgBrightness(float fBrightness)      { m_fAvgBrightness = fBrightness; }
	virtual const ColorF& GetMinColor() const                      { return m_cMinColor; }
	virtual void          SetMinColor(const ColorF& cMinColor)     { m_cMinColor = cMinColor; }
	virtual const ColorF& GetMaxColor() const                      { return m_cMaxColor; }
	virtual void          SetMaxColor(const ColorF& cMaxColor)     { m_cMaxColor = cMaxColor; }
	virtual const ColorF& GetClearColor() const                    { return m_cClearColor; }
	virtual void          SetClearColor(const ColorF& cClearColor) { m_cClearColor = cClearColor; }

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	// The only reason this access exists, is because RenderTargets are CTexture and not CDeviceTexture,
	// and they are allocated deferred (CDevicetexture doesn't exist when ESRAM-location is configured)
	void SetESRAMOffset(int32 offset) { m_nESRAMOffset = offset; if (m_pDevTexture) m_pDevTexture->SetESRAMOffset(offset); }
	int32 GetESRAMOffset() { if (m_pDevTexture) return m_pDevTexture->GetESRAMOffset(); return m_nESRAMOffset; }
#endif

	virtual void          GetMemoryUsage(ICrySizer* pSizer) const;
	virtual const char*   GetFormatName() const;
	virtual const char*   GetTypeName() const;

	virtual const bool    IsShared() const
	{
		return CBaseResource::GetRefCounter() > 1;
	}

	virtual const ETEX_Format GetTextureDstFormat() const { return m_eDstFormat; }
	virtual const ETEX_Format GetTextureSrcFormat() const { return m_eSrcFormat; }

	virtual const bool        IsParticularMipStreamed(float fMipFactor) const;

	// Internal functions
	const ETEX_Format GetDstFormat() const { return m_eDstFormat; }
	const ETEX_Format GetSrcFormat() const { return m_eSrcFormat; }
	const ETEX_Type   GetTexType() const   { return m_eTT; }
	const uint32      StreamGetNumSlices() const
	{
#if !defined(_RELEASE)
		switch (m_eTT)
		{
		case eTT_1D:
		case eTT_2D:
		case eTT_2DMS:
		case eTT_3D:
			assert((m_nArraySize == 1));
		case eTT_2DArray:
			break;

		case eTT_Cube:
			assert((m_nArraySize == 6));
		case eTT_CubeArray:
			assert(!(m_nArraySize % 6));
			break;

		default:
			__debugbreak();
		}
#endif

		return m_nArraySize;
	}

	void               RT_ReleaseDevice();

	static inline bool IsTextureExist(const CTexture* pTex)          { return pTex && pTex->GetDevTexture(); }

	const bool         IsNoTexture() const                           { return m_bNoTexture; };
	void               SetNeedRestoring()                            { m_bNeedRestoring = true; }
	void               SetWasUnload(bool bSet)                       { m_bWasUnloaded = bSet; }
	const bool         IsPartiallyLoaded() const                     { return m_nMinMipVidUploaded != 0; }
	const bool         IsUnloaded(void) const                        { return m_bWasUnloaded; }
	void               SetKeepSystemCopy(const bool bKeepSystemCopy) { if (bKeepSystemCopy) m_eFlags |= FT_KEEP_LOWRES_SYSCOPY; else m_eFlags &= ~FT_KEEP_LOWRES_SYSCOPY; }
	void               SetStreamingInProgress(uint16 nStreamSlot)
	{
		assert(nStreamSlot == InvalidStreamSlot || m_nStreamSlot == InvalidStreamSlot);
		m_nStreamSlot = nStreamSlot;
	}
	void                             SetStreamingPriority(const uint8 nPriority) { m_nStreamingPriority = nPriority; }
	ILINE const STexStreamRoundInfo& GetStreamRoundInfo(int zone) const          { return m_streamRounds[zone]; }
	void                             ResetNeedRestoring()                        { m_bNeedRestoring = false; }
	const bool                       IsNeedRestoring() const                     { return m_bNeedRestoring; }
	const int                        StreamGetLoadedMip() const                  { return m_nMinMipVidUploaded; }
	const uint8                      StreamGetFormatCode() const                 { return m_nStreamFormatCode; }
	const int                        StreamGetActiveMip() const                  { return m_nMinMipVidActive; }
	const int                        StreamGetPriority() const                   { return m_nStreamingPriority; }
	const bool                       IsResolved() const                          { return m_bResolved; }
	void                             SetUseMultisampledRTV(bool bSet)            { m_bUseMultisampledRTV = bSet; }
	const bool                       UseMultisampledRTV() const                  { return m_bUseMultisampledRTV; }
	const bool                       IsVertexTexture() const                     { return m_bVertexTexture; }
	void                             SetVertexTexture(bool bEnable)              { m_bVertexTexture = bEnable; }
	const bool                       IsDynamic() const                           { return ((m_eFlags & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL)) != 0); }
	bool                             IsStillUsedByGPU();
	void                             Lock()                                      { m_bIsLocked = true; }
	void                             Unlock()                                    { m_bIsLocked = false; }
	const bool                       IsLoaded() const                            { return (m_eFlags & FT_FAILED) == 0; }
	const bool                       IsLocked() const                            { return m_bIsLocked; }
	ILINE const bool                 IsStreamed() const                          { return m_bStreamed; }
	ILINE const bool                 IsInDistanceSortedList() const              { return m_bInDistanceSortedList; }
	bool                             IsPostponed() const                         { return m_bPostponed; }
	ILINE const bool                 IsStreaming() const                         { return m_nStreamSlot != InvalidStreamSlot; }
	virtual const bool               IsStreamedVirtual() const                   { return IsStreamed(); }
	virtual bool                     IsStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES]) const;
	virtual const int                GetAccessFrameId() const                    { return m_nAccessFrameID; }
	ILINE const int                  GetAccessFrameIdNonVirtual() const          { return m_nAccessFrameID; }
	void                             SetResolved(bool bResolved)                 { m_bResolved = bResolved; }
	virtual const int                GetCustomID() const final                   { return m_nCustomID; }
	virtual void                     SetCustomID(int nID) final                  { m_nCustomID = nID; }
	const bool                       UseDecalBorderCol() const                   { return m_bUseDecalBorderCol; }
	const bool                       IsSRGB() const                              { return m_bIsSRGB; }
	void                             SRGBRead(bool bEnable = false)              { m_bIsSRGB = bEnable; }
	const bool                       IsMSAA() const                              { return ((m_eFlags & FT_USAGE_MSAA) && m_bUseMultisampledRTV) != 0; }
	const bool                       IsCustomFormat() const                      { return m_bCustomFormat; }
	void                             SetCustomFormat()                           { m_bCustomFormat = true; }
	void                             SetWidth(int16 width)                       { m_nWidth = std::max<int16>(width, 1); m_nMips = 1; }
	void                             SetHeight(int16 height)                     { m_nHeight = std::max<int16>(height, 1); m_nMips = 1; }
	int                              GetUpdateFrameID() const                    { return m_nUpdateFrameID; }
	ILINE const int32                GetActualSize() const                       { return m_nDevTextureSize; }
	ILINE const int32                GetPersistentSize() const                   { return m_nPersistentSize; }

	ILINE void                       PrefetchStreamingInfo() const               { PrefetchLine(m_pFileTexMips, 0); }
	const STexStreamingInfo*         GetStreamingInfo() const                    { return m_pFileTexMips; }

	virtual const bool               IsStreamable() const                        { return IsStreamed(); }

	const bool                       IsStreamableNonVirtual() const              { return !(m_eFlags & FT_DONT_STREAM) && !(m_eTT == eTT_3D); }

	ILINE void                       DisableMgpuSync()
	{
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
		if (m_pDevTexture)
			m_pDevTexture->DisableMgpuSync();
#endif
	}

	ILINE void MgpuResourceUpdate(bool bUpdating = true)
	{
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
		if (m_pDevTexture)
			m_pDevTexture->MgpuResourceUpdate(bUpdating);
#endif
	}

	bool         IsHighQualityFiltered() const               { return m_bHighQualityFiltering; }
	virtual void SetHighQualityFiltering(bool bState = true) { m_bHighQualityFiltering = bState; }

	bool            IsFPFormat() const    { return CImageExtensionHelper::IsRangeless(m_eDstFormat); };

	CDeviceTexture* GetDevTexture(bool bMultisampled = false) const { return (!bMultisampled ? m_pDevTexture : m_pDevTexture->GetMSAATexture()); }
	void            SetDevTexture(CDeviceTexture* pDeviceTex);
	void            OwnDevTexture(CDeviceTexture* pDeviceTex);
	bool            IsAsyncDevTexCreation() const { return m_bAsyncDevTexCreation; }

	// note: render target should be created with FT_FORCE_MIPS flag
	bool               GenerateMipMaps(bool bSetOrthoProj = false, bool bUseHW = true, bool bNormalMap = false);

	void               SetDefaultShaderResourceView(D3DBaseView* pDeviceShaderResource, bool bMultisampled = false);

	D3DBaseView*       GetResourceView(const SResourceView& rvDesc);
	D3DBaseView*       GetResourceView(const SResourceView& rvDesc) const;
	void               SetResourceView(const SResourceView& rvDesc, D3DBaseView* pView);

	// NOTE: deprecated
	D3DSurface*        GetSurface(int nCMSide, int nLevel);
	D3DSurface*        GetSurface(int nCMSide, int nLevel) const;

	const SPixFormat*  GetPixelFormat() const { return m_pPixelFormat; }
	bool               Invalidate(int nNewWidth, int nNewHeight, ETEX_Format eTF);
	const char*        GetSourceName() const  { return m_SrcName.c_str(); }
	const int          GetSize(bool bIncludePool) const;
	void               PostCreate();

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	void CheckValidateSRVs()
	{
		if (m_pDevTexture && m_pDevTexture->GetBaseAddressInvalidated() != m_nDeviceAddressInvalidated)
		{
			ValidateSRVs();

			m_nDeviceAddressInvalidated = m_pDevTexture->GetBaseAddressInvalidated();
		}
	}

	void ValidateSRVs();
#endif

	//////////////////////////////////////////////////////////////////////////

public:
	//===================================================================
	// Streaming support

	// Global streaming constants
	static int             s_nStreamingMode;
	static int             s_nStreamingUpdateMode;
	static int             s_nStreamingThroughput; // in bytes
	static float           s_nStreamingTotalTime;  // in secs
	static bool            s_bStreamDontKeepSystem;
	static bool            s_bPrecachePhase;
	static bool            s_bInLevelPhase;
	static bool            s_bPrestreamPhase;
	static bool            s_bStreamingFromHDD;
	static int             s_nTexturesDataBytesLoaded;
	static volatile int    s_nTexturesDataBytesUploaded;
	static int             s_nStatsAllocFails;
	static bool            s_bOutOfMemoryTotally;

	static volatile size_t s_nStatsStreamPoolInUseMem;          // Amount of stream pool currently in use by texture streaming
	static volatile size_t s_nStatsStreamPoolBoundMem;          // Amount of stream pool currently bound and in use by textures (avail + non avail)
	static volatile size_t s_nStatsStreamPoolBoundPersMem;      // Amount of stream pool currently bound and in use by persistent texture mem (avail + non avail)
	static volatile size_t s_nStatsCurManagedNonStreamedTexMem;
	static volatile size_t s_nStatsCurDynamicTexMem;
	static volatile size_t s_nStatsStreamPoolWanted;
	static bool            s_bStatsComputeStreamPoolWanted;

	struct WantedStat
	{
		_smart_ptr<ITexture> pTex;
		uint32               nWanted;
	};

	static std::vector<WantedStat>* s_pStatsTexWantedLists;
	static std::set<string>         s_vTexReloadRequests;
	static CryCriticalSection       s_xTexReloadLock;

	static CTextureStreamPoolMgr*   s_pPoolMgr;

	static ITextureStreamer*        s_pTextureStreamer;

	static CryCriticalSection       s_streamFormatLock;
	static SStreamFormatCode        s_formatCodes[256];
	static uint32                   s_nFormatCodes;
	typedef std::map<SStreamFormatCodeKey, uint32> TStreamFormatCodeKeyMap;
	static TStreamFormatCodeKeyMap  s_formatCodeMap;

	enum
	{
		MaxStreamTasks     = 512,
		MaxStreamPrepTasks = 8192,
		StreamOutMask      = 0x8000,
		StreamPrepMask     = 0x4000,
		StreamIdxMask      = 0x4000 - 1,
		InvalidStreamSlot  = 0xffff,
	};

	static CTextureArrayAlloc<STexStreamInState, MaxStreamTasks>        s_StreamInTasks;
	static CTextureArrayAlloc<STexStreamPrepState*, MaxStreamPrepTasks> s_StreamPrepTasks;

#ifdef TEXSTRM_ASYNC_TEXCOPY
	static CTextureArrayAlloc<STexStreamOutState, MaxStreamTasks> s_StreamOutTasks;
#endif

	static volatile int s_nBytesSubmittedToStreaming;
	static volatile int s_nMipsSubmittedToStreaming;
	static volatile int s_nNumStreamingRequests;
	static int          s_nBytesRequiredNotSubmitted;

#if !defined (_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	static int   s_TextureUpdates;
	static float s_TextureUpdatesTime;
	static int   s_TexturesUpdatedRendered;
	static float s_TextureUpdatedRenderedTime;
	static int   s_StreamingRequestsCount;
	static float s_StreamingRequestsTime;
	static int   s_nStatsCurManagedStreamedTexMemRequired;
#endif

#ifdef ENABLE_TEXTURE_STREAM_LISTENER
	static ITextureStreamListener* s_pStreamListener;
#endif

	static void  Precache();
	static void  RT_Precache();
	static void  StreamValidateTexSize();
	static uint8 StreamComputeFormatCode(uint32 nWidth, uint32 nHeight, uint32 nMips, ETEX_Format fmt);

#ifdef ENABLE_TEXTURE_STREAM_LISTENER
	static void StreamUpdateStats();
#endif

	int StreamComputeSysDataSize(int nFromMip) const
	{
#if defined(TEXSTRM_STORE_DEVSIZES)
		if (m_pFileTexMips)
		{
			return m_pFileTexMips->m_pMipHeader[nFromMip].m_DevSideSizeWithMips;
		}
#endif

		return CTexture::TextureDataSize(
			max(1, m_nWidth  >> nFromMip),
			max(1, m_nHeight >> nFromMip),
			max(1, m_nDepth  >> nFromMip),
			m_nMips - nFromMip,
			StreamGetNumSlices(),
			m_eSrcFormat,
			m_eSrcTileMode
		);
	}

	void StreamUploadMip(IReadStream* pStream, int nMip, int nBaseMipOffset, STexPoolItem* pNewPoolItem, STexStreamInMipState& mipState);
	void StreamUploadMipSide(
	  int const iSide, int const Sides, const byte* const pRawData, int nSrcPitch,
	  const STexMipHeader& mh, bool const bStreamInPlace,
	  int const nCurMipWidth, int const nCurMipHeight, int const nMip,
	  CDeviceTexture* pDeviceTexture, uint32 nBaseTexWidth, uint32 nBaseTexHeight, int nBaseMipOffset);

	void          StreamExpandMip(const void* pRawData, int nMip, int nBaseMipOffset, int nSideDelta);
	static void   RT_FlushAllStreamingTasks(const bool bAbort = false);
	static bool   IsStreamingInProgress() { return s_StreamInTasks.GetNumLive() > 0; }
	static void   AbortStreamingTasks(CTexture* pTex);
	static bool   StartStreaming(CTexture* pTex, STexPoolItem* pNewPoolItem, const int nStartMip, const int nEndMip, const int nActivateMip, EStreamTaskPriority estp);
	bool          CanStreamInPlace(int nMip, STexPoolItem* pNewPoolItem);
#if defined(TEXSTRM_ASYNC_TEXCOPY)
	bool          CanAsyncCopy();
#endif
	void          StreamCopyMipsTexToMem(int nStartMip, int nEndMip, bool bToDevice, STexPoolItem* pNewPoolItem);
	static void   StreamCopyMipsTexToTex(
		STexPoolItem* const pSrcItem, int nSrcMipOffset,
		STexPoolItem* const pDstItem, int nDstMipOffset, int nNumMips);
	static void   CopySliceChain(
		CDeviceTexture* const pDstDevTex, int nDstNumMips, int nDstSliceOffset, int nDstMipOffset,
		CDeviceTexture* const pSrcDevTex, int nSrcNumMips, int nSrcSliceOffset, int nSrcMipOffset, int nNumSlices, int nNumMips);

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	void          StreamUploadMip_Durango(const void* pSurfaceData, int nMip, int nBaseMipOffset, STexPoolItem* pNewPoolItem, STexStreamInMipState& mipState);
	void          StreamUploadMips_Durango(int nBaseMip, int nMipCount, STexPoolItem* pNewPoolItem, STexStreamInState& streamState);
	bool          StreamInCheckTileComplete_Durango(STexStreamInState& state);
	bool          StreamInCheckCopyComplete_Durango(STexStreamInState& state);
	bool          StreamOutCheckComplete_Durango(STexStreamOutState& state);
	static UINT64 StreamInsertFence();
	static UINT64 StreamCopyMipsTexToTex_MoveEngine(STexPoolItem* pSrcItem, int nMipSrc, STexPoolItem* pDestItem, int nMipDest, int nNumMips);  // GPU-assisted platform-dependent
#endif

#if defined(TEXSTRM_DEFERRED_UPLOAD)
	ID3D11CommandList*         StreamCreateDeferred(int nStartMip, int nEndMip, STexPoolItem* pNewPoolItem, STexPoolItem* pSrcPoolItem);
	void                       StreamApplyDeferred(ID3D11CommandList* pCmdList);
#endif
	void                       StreamReleaseMipsData(int nStartMip, int nEndMip);
	int16                      StreamCalculateMipsSignedFP(float fMipFactor) const;
	float                      StreamCalculateMipFactor(int16 nMipsSigned) const;
	virtual int                StreamCalculateMipsSigned(float fMipFactor) const;
	virtual int                GetStreamableMipNumber()  const;
	virtual int                GetStreamableMemoryUsage(int nStartMip) const;
	virtual int                GetMinLoadedMip() const { return m_nMinMipVidUploaded; }
	void                       SetMinLoadedMip(int nMinMip);
	void                       StreamUploadMips(int nStartMip, int nEndMip, STexPoolItem* pNewPoolItem);
	int                        StreamUnload();
	int                        StreamTrim(int nToMip);
	void                       StreamActivateLod(int nMinMip);
	void                       StreamLoadFromCache(const int nFlags);
	bool                       StreamPrepare(bool bFromLoad);
	bool                       StreamPrepare(CImageFile* pImage);
	bool                       StreamPrepareComposition();
	bool                       StreamPrepare_Platform();
	bool                       StreamPrepare_Finalise(bool bFromLoad);
	STexPool*                  StreamGetPool(int nStartMip, int nMips);
	STexPoolItem*              StreamGetPoolItem(int nStartMip, int nMips, bool bShouldBeCreated, bool bCreateFromMipData = false, bool bCanCreate = true, bool bForStreamOut = false);
	void                       StreamRemoveFromPool();
	void                       StreamAssignPoolItem(STexPoolItem* pItem, int nMinMip);

	static void                StreamState_Update();
	static void                StreamState_UpdatePrep();

	static STexStreamInState*  StreamState_AllocateIn();
	static void                StreamState_ReleaseIn(STexStreamInState* pState);
#ifdef TEXSTRM_ASYNC_TEXCOPY
	static STexStreamOutState* StreamState_AllocateOut();
	static void                StreamState_ReleaseOut(STexStreamOutState* pState);
#endif
	static STexStreamingInfo*  StreamState_AllocateInfo(int nMips);
	static void                StreamState_ReleaseInfo(CTexture* pOwnerTex, STexStreamingInfo* pInfo);

	void                       Relink();
	void                       Unlink();

	inline void RT_Relink()
	{
		if (!IsStreamed() || IsInDistanceSortedList())
			return;

		s_pTextureStreamer->Relink(this);
	}

	inline void RT_Unlink()
	{
		if (IsInDistanceSortedList())
			s_pTextureStreamer->Unlink(this);
	}

	//=======================================================

	static void ApplyForID(int nTUnit, int nID, SamplerStateHandle nState, int nSUnit)
	{
		CTexture* pTex = GetByID(nID);
		assert(pTex);
		if (pTex)
			pTex->Apply(nTUnit, nState, -1, nSUnit);
	}

	static const CCryNameTSCRC&  mfGetClassName();
	static CTexture*             GetByID(int nID);
	static CTexture*             GetByName(const char* szName, uint32 flags = 0);
	static CTexture*             GetByNameCRC(CCryNameTSCRC Name);
	static CTexture*             ForName(const char* name, uint32 nFlags, ETEX_Format eFormat);
	static _smart_ptr<CTexture>  ForNamePtr(const char* name, uint32 nFlags, ETEX_Format eFormat);

	static void                 InitStreaming();
	static void                 InitStreamingDev();
	static void                 Init();
	static void                 PostInit();
	static void                 RT_FlushStreaming(bool bAbort);
	static void                 ShutDown();

	static void                 CreateSystemTargets();
	static void                 ReleaseSystemTargets();
	static void                 ReleaseSystemTextures(bool bFinalRelease = false);
	static void                 LoadDefaultSystemTextures();
	static void                 LoadScaleformSystemTextures();
	static inline void          ResetTMUs() { memset(s_TexStateIDs, int(EDefaultSamplerStates::Unspecified), sizeof(s_TexStateIDs)); }

	static bool                 ReloadFile(const char* szFileName);
	static bool                 ReloadFile_Request(const char* szFileName);
	static void                 ReloadTextures();
	static void                 Update();
	static void                 RT_LoadingUpdate();
	static void                 RLT_LoadingUpdate();

	// Loading/creating functions
	bool  Load(ETEX_Format eFormat);
	bool  Load(CImageFile* pImage);
	bool  LoadFromImage(const char* name, ETEX_Format eFormat = eTF_Unknown);
	bool  Reload();
	bool  ToggleStreaming(const bool bEnable);
	virtual void UpdateData(STexData &td, int flags);

	byte* GetSubImageData32(int nX, int nY, int nW, int nH, int& nOutTexDim);

	//=======================================================
	// Lowest-level functions calling into the API-specific implementation
	bool               RT_CreateDeviceTexture(const void* pData[]);
	bool               RT_CreateDeviceTexture(D3DResource* pNatTex);
	bool               CreateDeviceTexture(const void* pData[]);
	bool               CreateDeviceTexture(D3DResource* pTex);
	void               ReleaseDeviceTexture(bool bKeepLastMips, bool bFromUnload = false);

	// Low-level functions calling CreateDeviceTexture()
	bool               CreateRenderTarget(ETEX_Format eFormat, const ColorF& cClear);
	bool               CreateDepthStencil(ETEX_Format eFormat, const ColorF& cClear);
	bool               CreateShaderResource(STexData& td);

	// Mid-level functions calling Create...()
	bool               Create2DTexture(int nWidth, int nHeight, int nMips, int nFlags, byte* pData, ETEX_Format eSrcFormat);
	bool               Create3DTexture(int nWidth, int nHeight, int nDepth, int nMips, int nFlags, byte* pData, ETEX_Format eTFSrc);

	// High-level functions calling Create...()
	static CTexture*              GetOrCreateTextureObject(const char* name, uint32 nWidth, uint32 nHeight, int nDepth, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID = -1);
	static _smart_ptr<CTexture>   GetOrCreateTextureObjectPtr(const char* name, uint32 nWidth, uint32 nHeight, int nDepth, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID = -1);
	static CTexture*              GetOrCreateTextureArray(const char* name, uint32 nWidth, uint32 nHeight, uint32 nArraySize, int nMips, ETEX_Type eType, uint32 nFlags, ETEX_Format eFormat, int nCustomID = -1);

	// High-level functions calling GetOrCreate...() and Create...()
	static CTexture*   GetOrCreateRenderTarget(const char* name, uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Type eTT, uint32 nFlags, ETEX_Format eSrcFormat, int nCustomID = -1);
	static CTexture*   GetOrCreateDepthStencil(const char* name, uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Type eTT, uint32 nFlags, ETEX_Format eSrcFormat, int nCustomID = -1);
	static CTexture*   GetOrCreate2DTexture(const char* szName, int nWidth, int nHeight, int nMips, int nFlags, byte* pData, ETEX_Format eSrcFormat, bool bAsyncDevTexCreation = false);
	static CTexture*   GetOrCreate3DTexture(const char* szName, int nWidth, int nHeight, int nDepth, int nMips, int nFlags, byte* pData, ETEX_Format eSrcFormat);
	static CTexture*   GetOrCreate2DCompositeTexture(const char* szName, int nWidth, int nHeight, int nMips, int nFlags, ETEX_Format eSrcFormat, const STexComposition* pCompositions, size_t nCompositions);
	//=======================================================

	// API depended functions
	void               Unbind();
	bool               Resolve(int nTarget = 0, bool bUseViewportSize = false);
	
	void               Apply(int nTUnit, SamplerStateHandle nState = EDefaultSamplerStates::Unspecified, int nTexMatSlot = EFTT_UNKNOWN, int nSUnit = -1, ResourceViewHandle hView = EDefaultResourceViews::Default, bool bMSAA = false, EHWShaderClass eHWSC = eHWSC_Pixel);
	void               ApplySampler(int nSUnit, EHWShaderClass eHWSC = eHWSC_Pixel, SamplerStateHandle eState = EDefaultSamplerStates::Unspecified);
	void               ApplyTexture(int nTUnit, EHWShaderClass eHWSC = eHWSC_Pixel, ResourceViewHandle hView = EDefaultResourceViews::Default, bool bMSAA = false);
	void               SetTexStates();
	void               UpdateTexStates();
	void               UpdateTextureRegion(byte* pSrcData, int X, int Y, int Z, int USize, int VSize, int ZSize, ETEX_Format eSrcFormat);
	void               RT_UpdateTextureRegion(byte* pSrcData, int X, int Y, int Z, int USize, int VSize, int ZSize, ETEX_Format eSrcFormat);
	bool               SetNoTexture(CTexture* pDefaultTexture = s_ptexNoTexture);

	static void        SetSampler(SamplerStateHandle nTS, int nSSlot, EHWShaderClass eHWSC = eHWSC_Pixel);

	// Helper functions
	static void GenerateZMaps();
	static void DestroyZMaps();
	static void GenerateHDRMaps();
	// allocate or deallocate star maps
	static void DestroyHDRMaps();
	static void GenerateSceneMap(ETEX_Format eTF);
	static void DestroySceneMap();
	static void GenerateCachedShadowMaps();
	static void DestroyCachedShadowMaps();
	static void GenerateNearestShadowMap();
	static void DestroyNearestShadowMap();

#if defined(TEXTURE_GET_SYSTEM_COPY_SUPPORT)
	static byte*        Convert(byte* pSrc, int nWidth, int nHeight, int nMips, ETEX_Format eSrcFormat, ETEX_Format eDstFormat, int nOutMips, int& nOutSize, bool bLinear);
#endif
	static int          CalcNumMips(int nWidth, int nHeight);
	// upload mip data from file regarding to platform specifics
	static bool         IsInPlaceFormat(const ETEX_Format fmt);
	static void         ExpandMipFromFile(byte* dest, const int destSize, const byte* src, const int srcSize, const ETEX_Format srcFmt, const ETEX_Format dstFmt);
	static uint32       TextureDataSize(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nSlices, const ETEX_Format eTF, ETEX_TileMode eTM = eTM_None);

	static ETEX_Format  GetClosestFormatSupported(ETEX_Format eTFDst, const SPixFormat*& pPF);
	ETEX_Format         SetClosestFormatSupported();

	static ILINE bool   IsBlockCompressed(const ETEX_Format eTF) { return CImageExtensionHelper::IsBlockCompressed(eTF); }
	static ILINE bool   IsFourBit(ETEX_Format eTF)               { return BitsPerPixel(eTF) == 16; }
	static ILINE int    BytesPerBlock(ETEX_Format eTF)           { return CImageExtensionHelper::BytesPerBlock(eTF); }
	static ILINE int    BitsPerPixel(const ETEX_Format eTF)      { return CImageExtensionHelper::BitsPerPixel(eTF); }
	static ILINE int    BytesPerPixel(ETEX_Format eTF)           { return BitsPerPixel(eTF) / 8; }
	static const char*  NameForTextureFormat(ETEX_Format eTF)    { return CImageExtensionHelper::NameForTextureFormat(eTF); }
	static const char*  NameForTextureType(ETEX_Type eTT)        { return CImageExtensionHelper::NameForTextureType(eTT); }
	static ETEX_Format  TextureFormatForName(const char* str)    { return CImageExtensionHelper::TextureFormatForName(str); }
	static ETEX_Type    TextureTypeForName(const char* str)      { return CImageExtensionHelper::TextureTypeForName(str); }

	static SEnvTexture* FindSuitableEnvTex(Vec3& Pos, Ang3& Angs, bool bMustExist, int RendFlags, bool bUseExistingREs, CShader* pSH, CShaderResources* pRes, CRenderObject* pObj, bool bReflect, CRenderElement* pRE, bool* bMustUpdate);
	static void         DrawSceneToCubeSide(Vec3& Pos, int tex_size, int side);
	static bool         RenderEnvironmentCMHDR(int size, Vec3& Pos, TArray<unsigned short>& vecData);

public:
	static bool          m_bLoadedSystem;

	static STexStageInfo s_TexStages[MAX_TMU];
	static SamplerStateHandle s_TexStateIDs[eHWSC_Num][MAX_TMU];
	static uint32        s_TexState_MipSRGBMask[MAX_TMU];

	static ETEX_Format   s_eTFZ;

	// ==============================================================================
	static CTexture* s_ptexNoTexture;
	static CTexture* s_ptexNoTextureCM;
	static CTexture* s_ptexWhite;
	static CTexture* s_ptexGray;
	static CTexture* s_ptexBlack;
	static CTexture* s_ptexBlackAlpha;
	static CTexture* s_ptexBlackCM;
	static CTexture* s_ptexDefaultProbeCM;
	static CTexture* s_ptexDefaultMergedDetail;
	static CTexture* s_ptexFlatBump;
#if !defined(_RELEASE)
	static CTexture* s_ptexMipMapDebug;
	static CTexture* s_ptexColorBlue;
	static CTexture* s_ptexColorCyan;
	static CTexture* s_ptexColorGreen;
	static CTexture* s_ptexColorPurple;
	static CTexture* s_ptexColorRed;
	static CTexture* s_ptexColorWhite;
	static CTexture* s_ptexColorYellow;
	static CTexture* s_ptexColorMagenta;
	static CTexture* s_ptexColorOrange;
#endif
	static CTexture* s_ptexPaletteDebug;
	static CTexture* s_ptexPaletteTexelsPerMeter;
	static CTexture* s_ptexIconShaderCompiling;
	static CTexture* s_ptexIconStreaming;
	static CTexture* s_ptexIconStreamingTerrainTexture;
	static CTexture* s_ptexIconNavigationProcessing;
	static CTexture* s_ptexMipColors_Diffuse;
	static CTexture* s_ptexMipColors_Bump;
	static CTexture* s_ptexShadowJitterMap;
	static CTexture* s_ptexEnvironmentBRDF;
	static CTexture* s_ptexScreenNoiseMap;
	static CTexture* s_ptexDissolveNoiseMap;
	static CTexture* s_ptexNoise3D;
	static CTexture* s_ptexGrainFilterMap;
	static CTexture* s_ptexFilmGrainMap;
	static CTexture* s_ptexVignettingMap; // todo: create at runtime based on viggneting cvars
	static CTexture* s_ptexAOJitter;
	static CTexture* s_ptexAOVOJitter;
	static CTexture* s_ptexFromRE[8];
	static CTexture* s_ptexShadowID[8];
	static CTexture* s_ptexShadowMask;
	static CTexture* s_ptexCachedShadowMap[MAX_GSM_LODS_NUM];
	static CTexture* s_ptexNearestShadowMap;
	static CTexture* s_ptexFromRE_FromContainer[2];
	static CTexture* s_ptexFromObj;
	static CTexture* s_ptexSvoTree;
	static CTexture* s_ptexSvoTris;
	static CTexture* s_ptexSvoGlobalCM;
	static CTexture* s_ptexSvoRgbs;
	static CTexture* s_ptexSvoNorm;
	static CTexture* s_ptexSvoOpac;
	static CTexture* s_ptexRT_2D;
	static CTexture* s_ptex16_PointsOnSphere;
	static CTexture* s_ptexNormalsFitting;
	static CTexture* s_ptexPerlinNoiseMap;

	static CTexture* s_ptexSceneNormalsMap;           // RT with normals for deferred shading
	static CTexture* s_ptexSceneNormalsMapMS;         // Dummy normals target for binding multisampled rt
	static CTexture* s_ptexSceneNormalsBent;
	static CTexture* s_ptexAOColorBleed;
	static CTexture* s_ptexSceneDiffuse;
	static CTexture* s_ptexSceneSpecular;
	static CTexture* s_ptexWindGrid;

	static CTexture* s_ptexSceneSelectionIDs;         // Selection ID buffer used for selection and highlight passes
	static CTexture* s_ptexSceneHalfDepthStencil;     // half resolution depth-stencil, used for selection and highlight passes.

#if defined(DURANGO_USE_ESRAM)
	static CTexture* s_ptexSceneSpecularESRAM;        // Temporary scene specular in ESRAM, aliased with other ESRAM RTs
#endif

	static CTexture* s_ptexHeightMapAO[2];
	static CTexture* s_ptexHeightMapAODepth[2];

	static CTexture*           s_ptexBackBuffer;              // back buffer copy
	static CTexture*           s_ptexModelHudBuffer;          // used by Menu3DModelRenderer to postprocess render models
	static CTexture*           s_ptexPrevBackBuffer[2][2];    // previous frame back buffer copies (for left and right eye)
	static CTexture*           s_ptexCached3DHud;             // 3d hud cached overframes
	static CTexture*           s_ptexCached3DHudScaled;       // downsampled 3d hud cached overframes
	static CTexture*           s_ptexBackBufferScaled[3];     // backbuffer low-resolution/blurred version. 2x/4x/8x/16x smaller than screen
	static CTexture*           s_ptexBackBufferScaledTemp[2]; // backbuffer low-resolution/blurred version. 2x/4x/8x/16x smaller than screen, temp textures (used for blurring/ping-pong)

	static CTexture*           s_ptexPrevFrameScaled; // 2x

	static CTexture*           s_ptexDepthBufferQuarter;     // Quater res depth buffer
	static CTexture*           s_ptexDepthBufferHalfQuarter; // Eighth res depth buffer

	static CTexture*           s_ptexWaterOcean;         // water ocean vertex texture
	static CTexture*           s_ptexWaterVolumeDDN;     // water volume heightmap
	static CTexture*           s_ptexWaterVolumeTemp[2]; // water volume heightmap
	static CTexture*           s_ptexWaterVolumeRefl[2]; // water volume reflections buffer
	static CTexture*           s_ptexWaterCaustics[2];   // caustics buffers
	static CTexture*           s_ptexRainOcclusion;      // top-down rain occlusion
	static CTexture*           s_ptexRainSSOcclusion[2]; // screen-space rain occlusion accumulation
	
	static CTexture*           s_ptexRainDropsRT[2];

	static CTexture*           s_ptexRT_ShadowPool;
	static CTexture*           s_ptexFarPlane; // shadow map representing the far plane (all tests pass)
	static CTexture*           s_ptexCloudsLM;

	static CTexture*           s_ptexSceneTarget;     // Shared rt for generic usage (refraction/srgb/diffuse accumulation/hdr motionblur/etc)
	static CTexture*           s_ptexCurrSceneTarget; // Pointer to current scene target, mostly for reading from destination rt
	static CTexture*           s_ptexSceneTargetR11G11B10F[2];
	static CTexture*           s_ptexSceneTargetScaledR11G11B10F[4];

	static CTexture*           s_ptexCurrentSceneDiffuseAccMap;
	static CTexture*           s_ptexSceneDiffuseAccMap;
	static CTexture*           s_ptexSceneSpecularAccMap;
	static CTexture*           s_ptexSceneTexturesMap;
	static CTexture*           s_ptexSceneDiffuseAccMapMS;
	static CTexture*           s_ptexSceneSpecularAccMapMS;

	static CTexture*           s_ptexZTarget;
	static CTexture*           s_ptexZOcclusion[2];
	static CTexture*           s_ptexZTargetReadBack[4];
	static CTexture*           s_ptexZTargetDownSample[4];
	static CTexture*           s_ptexZTargetScaled;
	static CTexture*           s_ptexZTargetScaled2;
	static CTexture*           s_ptexZTargetScaled3;

	static CTexture*           s_ptexHDRTarget;
	static CTexture*           s_ptexVelocityObjects[2]; // Dynamic object velocity (for left and right eye)
	static CTexture*           s_ptexVelocity;
	static CTexture*           s_ptexVelocityTiles[3];
	static CTexture*           s_ptexHDRTargetPrev;
	static CTexture*           s_ptexHDRTargetScaled[4];
	static CTexture*           s_ptexHDRTargetScaledTmp[4];
	static CTexture*           s_ptexHDRTargetScaledTempRT[4];
	static CTexture*           s_ptexHDRDofLayers[2];
	static CTexture*           s_ptexSceneCoC[MIN_DOF_COC_K];
	static CTexture*           s_ptexSceneCoCTemp;
	static CTexture*           s_ptexHDRTempBloom[2];
	static CTexture*           s_ptexHDRFinalBloom;
	static CTexture*           s_ptexHDRAdaptedLuminanceCur[8];
	static int                 s_nCurLumTextureIndex;
	static CTexture*           s_ptexCurLumTexture;
	static CTexture*           s_ptexHDRToneMaps[NUM_HDR_TONEMAP_TEXTURES];
	static CTexture*           s_ptexHDRMeasuredLuminance[MAX_GPU_NUM];
	static CTexture*           s_ptexHDRMeasuredLuminanceDummy;
	static CTexture*           s_ptexSkyDomeMie;
	static CTexture*           s_ptexSkyDomeRayleigh;
	static CTexture*           s_ptexSkyDomeMoon;

	static CTexture*           s_ptexSceneTargetScaled;
	static CTexture*           s_ptexSceneTargetScaledBlurred;

	static CTexture*           s_ptexColorChart;

	static CTexture*           s_ptexStereoL;
	static CTexture*           s_ptexStereoR;
	static CTexture*           s_ptexQuadLayers[2];

	static CTexture*           s_ptexFlaresOcclusionRing[MAX_OCCLUSION_READBACK_TEXTURES];
	static CTexture*           s_ptexFlaresGather;

	static CTexture*           s_ptexDepthStencilRemapped;
	static SEnvTexture         s_EnvTexts[MAX_ENVTEXTURES];
	static TArray<SEnvTexture> s_CustomRT_2D;
	static TArray<CTexture>    s_ShaderTemplates;
	static bool                s_ShaderTemplatesInitialized;

	static CTexture*           s_pTexNULL;

	static CTexture*           s_pBackBuffer;
	static CTexture*           s_FrontBufferTextures[2];

	static CTexture*           s_ptexVolumetricFog;
	static CTexture*           s_ptexVolumetricClipVolumeStencil;

	static CTexture*           s_ptexVolCloudShadow;

#if defined(VOLUMETRIC_FOG_SHADOWS)
	static CTexture* s_ptexVolFogShadowBuf[2];
#endif

#if defined(TEXSTRM_DEFERRED_UPLOAD)
	static ID3D11DeviceContext* s_pStreamDeferredCtx;
#endif

};

bool  WriteTGA(byte* dat, int wdt, int hgt, const char* name, int src_bits_per_pixel, int dest_bits_per_pixel);
bool  WritePNG(byte* dat, int wdt, int hgt, const char* name);
bool  WriteJPG(byte* dat, int wdt, int hgt, const char* name, int bpp, int nQuality = 100);
#if CRY_PLATFORM_WINDOWS
byte* WriteDDS(byte* dat, int wdt, int hgt, int dpth, const char* name, ETEX_Format eTF, int nMips, ETEX_Type eTT, bool bToMemory = false, int* nSize = NULL);
#endif
bool  WriteTIF(const void* dat, int wdth, int hgt, int bytesPerChannel, int numChannels, bool bFloat, const char* szPreset, const char* szFileName);

//////////////////////////////////////////////////////////////////////////

struct IDynTextureSourceImpl : public IDynTextureSource
{
	virtual bool Update() = 0;
	virtual bool Apply(int nTUnit, SamplerStateHandle nTS = EDefaultSamplerStates::Unspecified) = 0;
	virtual void GetTexGenInfo(float& offsX, float& offsY, float& scaleX, float& scaleY) const = 0;
	virtual void SetSize(uint32 width, uint32 height) = 0;
};

class CDynTextureSource : public IDynTextureSourceImpl
{
public:
	virtual void   AddRef();
	virtual void   Release();

	virtual void   EnablePerFrameRendering(bool enable) {}
	virtual void   Activate(bool activate)              {}
#if defined(ENABLE_DYNTEXSRC_PROFILING)
	virtual string GetProfileInfo() const               { return string(); }
#endif

	virtual bool      Apply(int nTUnit, SamplerStateHandle nTS = EDefaultSamplerStates::Unspecified);
	virtual void      GetTexGenInfo(float& offsX, float& offsY, float& scaleX, float& scaleY) const;

	virtual ITexture* GetTexture() const                   { return m_pDynTexture->GetTexture(); }
	virtual void      SetSize(uint32 width, uint32 height) { m_width = width; m_height = height; }

public:
	CDynTextureSource();

protected:
	virtual ~CDynTextureSource();
	virtual void CalcSize(uint32& width, uint32& height, float distToCamera = -1) const;

	void         InitDynTexture(ETexPool eTexPool);
protected:
	volatile int  m_refCount;
	uint32        m_width;
	uint32        m_height;
	float         m_lastUpdateTime;
	int           m_lastUpdateFrameID;
	SDynTexture2* m_pDynTexture;
};

class CFlashTextureSourceBase : public IDynTextureSourceImpl, IUIModule
{
public:
	virtual void AddRef();
	virtual void Release();

	virtual void Reload() { CreateTexFromFlashFile(m_pFlashFileName.c_str()); }

	virtual void EnablePerFrameRendering(bool enable)
	{
		assert(m_autoUpdate);
		m_perFrameRendering = enable;
	}
	virtual void Activate(bool activate);

#if defined(ENABLE_DYNTEXSRC_PROFILING)
	virtual string GetProfileInfo() const;
#endif

	virtual bool              Update();
	virtual bool              Apply(int nTUnit, SamplerStateHandle nTS = EDefaultSamplerStates::Unspecified);
	virtual void              GetTexGenInfo(float& offsX, float& offsY, float& scaleX, float& scaleY) const;

	virtual void*             GetSourceTemp(EDynTextureSource type) const;
	virtual void*             GetSourcePerm(EDynTextureSource type);
	virtual const char*       GetSourceFilePath() const;
	virtual EDynTextureSource GetSourceType() const                { return DTS_I_FLASHPLAYER; }
	virtual void              SetSize(uint32 width, uint32 height) { m_width = width; m_height = height; }

	static bool               IsFlashFile(const char* name);
	static bool               IsFlashUIFile(const char* name);
	static bool               IsFlashUILayoutFile(const char* name);

	virtual bool              CreateTexFromFlashFile(const char* name);
	static bool               DestroyTexOfFlashFile(const char* name);

public:
	static void Tick();
	static void TickRT();
	static void RenderLights();
	static void AddToLightRenderList(const IDynTextureSource* pSrc);

public:
	CFlashTextureSourceBase(const char* pFlashFileName, const IRenderer::SLoadShaderItemArgs* pArgs);

	void AutoUpdate(const CTimeValue& curTime, const float delta, const bool isEditing, const bool isPaused);
	void AutoUpdateRT(const int frameID);

	struct IFlashPlayerInstanceWrapper
	{
	public:

		ILINE virtual ~IFlashPlayerInstanceWrapper(){}

		virtual void          Release() = 0;

		virtual bool          CheckPtr() const = 0;
		virtual bool          CanDeactivate() const = 0;

		virtual IFlashPlayer* GetTempPtr() const = 0;
		virtual IFlashPlayer* GetPermPtr(CFlashTextureSourceBase* pSrc) = 0;

		virtual void          Activate(bool activate, CFlashTextureSourceBase* pSrc) = 0;
		virtual void          Clear(CFlashTextureSourceBase* pSrc) = 0;

		virtual const char*   GetSourceFilePath() const = 0;

		virtual void          UpdatePlayer(CFlashTextureSourceBase* pSrc) = 0;
		virtual void          Advance(float delta) = 0;

		virtual int           GetWidth() const = 0;
		virtual int           GetHeight() const = 0;
	};

	struct CFlashPlayerInstanceWrapperNULL : public IFlashPlayerInstanceWrapper
	{
	public:
		void                                    Release()                                              {}

		bool                                    CheckPtr() const                                       { return false; }
		bool                                    CanDeactivate() const                                  { return true; }

		IFlashPlayer*                           GetTempPtr() const                                     { return NULL; }
		IFlashPlayer*                           GetPermPtr(CFlashTextureSourceBase* pSrc)              { return NULL; }

		void                                    Activate(bool activate, CFlashTextureSourceBase* pSrc) {}
		void                                    Clear(CFlashTextureSourceBase* pSrc)                   {};

		const char*                             GetSourceFilePath() const                              { return "NULLWRAPPER"; }

		void                                    UpdatePlayer(CFlashTextureSourceBase* pSrc)            {}
		void                                    Advance(float delta)                                   {};

		int                                     GetWidth() const                                       { return 16; }
		int                                     GetHeight() const                                      { return 16; }

		static CFlashPlayerInstanceWrapperNULL* Get()
		{
			static CFlashPlayerInstanceWrapperNULL inst;
			return &inst;
		}
	private:
		CFlashPlayerInstanceWrapperNULL() {}
		~CFlashPlayerInstanceWrapperNULL() {}
	};

	class CFlashPlayerInstanceWrapper : public IFlashPlayerInstanceWrapper
	{
	public:
		CFlashPlayerInstanceWrapper() : m_pBootStrapper(0), m_pPlayer(0), m_lock(), m_canDeactivate(true), m_width(16), m_height(16) {}
		virtual ~CFlashPlayerInstanceWrapper();

		void SetBootStrapper(IFlashPlayerBootStrapper* p)
		{
			assert(!m_pBootStrapper);
			m_pBootStrapper = p;
		}
		IFlashPlayerBootStrapper* GetBootStrapper() const
		{
			return m_pBootStrapper;
		}

		void          Release()             { delete this; }

		bool          CheckPtr() const      { return m_pPlayer != 0; }
		bool          CanDeactivate() const { return m_canDeactivate; }

		IFlashPlayer* GetTempPtr() const;
		IFlashPlayer* GetPermPtr(CFlashTextureSourceBase* pSrc);

		void          Activate(bool activate, CFlashTextureSourceBase* pSrc);
		void          Clear(CFlashTextureSourceBase* pSrc) {};

		const char*   GetSourceFilePath() const;

		void          CreateInstance(CFlashTextureSourceBase* pSrc);

		void          UpdatePlayer(CFlashTextureSourceBase* pSrc) {}
		void          Advance(float delta);

		int           GetWidth() const  { return m_width; }
		int           GetHeight() const { return m_height; }

	private:
		IFlashPlayerBootStrapper*  m_pBootStrapper;
		IFlashPlayer*              m_pPlayer;
		mutable CryCriticalSection m_lock;
		bool                       m_canDeactivate;
		int                        m_width;
		int                        m_height;
	};

	class CFlashPlayerInstanceWrapperUIElement : public IFlashPlayerInstanceWrapper
	{
	public:
		CFlashPlayerInstanceWrapperUIElement() : m_pUIElement(0), m_pPlayer(0), m_lock(), m_canDeactivate(true), m_activated(false), m_width(16), m_height(16) {}
		virtual ~CFlashPlayerInstanceWrapperUIElement();

		void          SetUIElement(IUIElement* p);
		IUIElement*   GetUIElement() const  { return m_pUIElement; }

		void          Release()             { delete this; }

		bool          CheckPtr() const      { return m_pPlayer != 0; }
		bool          CanDeactivate() const { return m_canDeactivate; }

		IFlashPlayer* GetTempPtr() const;
		IFlashPlayer* GetPermPtr(CFlashTextureSourceBase* pSrc);

		void          Activate(bool activate, CFlashTextureSourceBase* pSrc);
		void          Clear(CFlashTextureSourceBase* pSrc);

		const char*   GetSourceFilePath() const;

		void          UpdatePlayer(CFlashTextureSourceBase* pSrc);
		void          Advance(float delta) {};

		int           GetWidth() const     { return m_width; }
		int           GetHeight() const    { return m_height; }

	private:
		void UpdateUIElementPlayer(CFlashTextureSourceBase* pSrc);

	private:
		IUIElement*                m_pUIElement;
		IFlashPlayer*              m_pPlayer;
		mutable CryCriticalSection m_lock;
		bool                       m_canDeactivate;
		bool                       m_activated;
		int                        m_width;
		int                        m_height;
	};

	class CFlashPlayerInstanceWrapperLayoutElement : public IFlashPlayerInstanceWrapper
	{
	public:
		CFlashPlayerInstanceWrapperLayoutElement() : m_layoutName(""), m_pUILayout(0), m_pPlayer(0), m_lock(), m_width(16), m_height(16), m_canDeactivate(true) {}
		virtual ~CFlashPlayerInstanceWrapperLayoutElement();

		void          Release()             { delete this; }

		bool          CheckPtr() const      { return m_pPlayer != 0; }
		bool          CanDeactivate() const { return m_canDeactivate; }

		IFlashPlayer* GetTempPtr() const;
		IFlashPlayer* GetPermPtr(CFlashTextureSourceBase* pSrc);

		void          Activate(bool activate, CFlashTextureSourceBase* pSrc);
		void          Clear(CFlashTextureSourceBase* pSrc) {};

		const char*   GetSourceFilePath() const;

		void          CreateInstance(CFlashTextureSourceBase* pSrc, const char* layoutName);

		void          UpdatePlayer(CFlashTextureSourceBase* pSrc) {}
		void          Advance(float delta);

		int           GetWidth() const  { return m_width; }
		int           GetHeight() const { return m_height; }

	private:
		string                     m_layoutName;

		IUILayoutBase*             m_pUILayout;
		IFlashPlayer*              m_pPlayer;

		mutable CryCriticalSection m_lock;

		int                        m_width;
		int                        m_height;

		bool                       m_canDeactivate;

	};

	template<typename T>
	class AutoReleasedPlayerPtr
	{
	public:
		AutoReleasedPlayerPtr(T* p) : m_pPlayer(p) {}
		~AutoReleasedPlayerPtr()
		{
			if (m_pPlayer)
			{
				m_pPlayer->Release();
				m_pPlayer = 0;
			}
		}
		IFlashPlayer* operator->()
		{
			return m_pPlayer;
		}
		const IFlashPlayer* operator->() const
		{
			return m_pPlayer;
		}
		operator bool() const
		{
			return m_pPlayer != 0;
		}

	private:
		AutoReleasedPlayerPtr(const AutoReleasedPlayerPtr&);
		AutoReleasedPlayerPtr& operator=(const AutoReleasedPlayerPtr&);

	private:
		T* m_pPlayer;
	};

	typedef AutoReleasedPlayerPtr<IFlashPlayer> AutoReleasedFlashPlayerPtr;

protected:
	virtual ~CFlashTextureSourceBase();

	virtual int                  GetWidth() const = 0;
	virtual int                  GetHeight() const = 0;

	virtual SDynTexture*         GetDynTexture() const = 0;
	virtual bool                 UpdateDynTex(int rtWidth, int rtHeight) = 0;

	IFlashPlayerInstanceWrapper* GetFlashPlayerInstanceWrapper() const { return m_pFlashPlayer; }

	static int                   Align8(int dim)                       { return (dim + 7) & ~7; }

private:
	void Advance(const float delta, bool isPaused);

	struct CachedTexStateID
	{
		SamplerStateHandle original;
		SamplerStateHandle patched;
	};
	static const size_t NumCachedTexStateIDs = 2;

private:
	volatile int                 m_refCount;
	CTimeValue                   m_lastVisible;
	int                          m_lastVisibleFrameID;
	uint32                       m_width;
	uint32                       m_height;
	float                        m_aspectRatio;
	bool                         m_autoUpdate;
	bool                         m_perFrameRendering;
	string                       m_pFlashFileName;
	IFlashPlayerInstanceWrapper* m_pFlashPlayer;
	IUIElement*                  m_pElement;
	CachedTexStateID             m_texStateIDs[NumCachedTexStateIDs];
#if defined(ENABLE_DYNTEXSRC_PROFILING)
	IMaterial*                   m_pMatSrc;
	IMaterial*                   m_pMatSrcParent;
#endif
};

class CFlashTextureSource
	: public CFlashTextureSourceBase
{
public:
	CFlashTextureSource(const char* pFlashFileName, const IRenderer::SLoadShaderItemArgs* pArgs);

	virtual EDynTextureRTType GetRTType() const override  { return DTS_RT_UNIQUE; }
	virtual ITexture*         GetTexture() const override { return m_pDynTexture->GetTexture(); }

protected:
	virtual ~CFlashTextureSource();

	virtual int          GetWidth() const override;
	virtual int          GetHeight() const override;

	virtual SDynTexture* GetDynTexture() const override { return m_pDynTexture; }
	virtual bool         UpdateDynTex(int rtWidth, int rtHeight) override;

	virtual bool         Update() override;

private:
	SDynTexture*    m_pDynTexture;
	CMipmapGenPass* m_pMipMapper;
};

class CFlashTextureSourceSharedRT
	: public CFlashTextureSourceBase
{
public:
	CFlashTextureSourceSharedRT(const char* pFlashFileName, const IRenderer::SLoadShaderItemArgs* pArgs);

	virtual EDynTextureRTType GetRTType() const override  { return DTS_RT_SHARED; }
	virtual ITexture*         GetTexture() const override { return ms_pDynTexture->GetTexture(); }

	static void               SetSharedRTDim(int width, int height);
	static void               SetupSharedRenderTargetRT();

protected:
	virtual ~CFlashTextureSourceSharedRT();

	virtual int          GetWidth() const override                        { return GetSharedRTWidth(); }
	virtual int          GetHeight() const override                       { return GetSharedRTHeight(); }

	virtual SDynTexture* GetDynTexture() const override                   { return ms_pDynTexture; }
	virtual bool         UpdateDynTex(int rtWidth, int rtHeight) override { return ms_pDynTexture->IsValid(); };

	virtual bool         Update() override;

private:
	static int  GetSharedRTWidth();
	static int  GetSharedRTHeight();
	static int  NearestPowerOfTwo(int n);
	static void ProbeDepthStencilSurfaceCreation(int width, int height);

private:
	static SDynTexture*    ms_pDynTexture;
	static CMipmapGenPass* ms_pMipMapper;

	static int             ms_sharedRTWidth;
	static int             ms_sharedRTHeight;
	static int             ms_instCount;
};

class CDynTextureSourceLayerActivator
{
public:
	static void LoadLevelInfo();
	static void ReleaseData();
	static void ActivateLayer(const char* pLayerName, bool activate);

private:
	typedef std::map<string, std::vector<string>> PerLayerDynSrcMtls;
	static PerLayerDynSrcMtls s_perLayerDynSrcMtls;
};

#endif
