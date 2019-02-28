// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcess.h : Post processing techniques base interface

   Revision history:
* 18/06/2005: Re-organized (to minimize code dependencies/less annoying compiling times)
* 23/02/2005: Re-factored/Converted to CryEngine 2.0
* Created by Tiago Sousa

   =============================================================================*/

#ifndef _POSTPROCESS_H_
#define _POSTPROCESS_H_

struct SRenderViewInfo;

class CShader;
class CTexture;
class CPostEffectContext;

// Declare effects ID - this will also be used as rendering sort order
enum EPostEffectID : int8
{
	Invalid     = -1,

	// Don't change order of post processes before sunshafts (on pc we doing some trickery to avoid redundant stretchrects)
	SunShafts = 0,
	MotionBlur,
	ColorGrading,
	DepthOfField,
	HUDSilhouettes,

	PostAA,

	UnderwaterGodRays,
	VolumetricScattering,

	// todo: merge all following into UberGamePostProcess
	Sharpening,
	Blurring,
	UberGamePostProcess,

	NightVision,
	SonarVision,
	ThermalVision,

	FlashBang,

	ImageGhosting,
	NanoGlass,

	RainDrops,
	WaterDroplets,
	WaterFlow,
	ScreenBlood,
	ScreenFrost,

	KillCamera,
	AlienInterference,
	PostStereo,
	HUD3D,

	ScreenFader,

	Post3DRenderer,

	Num
};

//////////////////////////////////////////////////////////////////////
enum ERenderModeFlags
{
	eRMF_THERMALVISION = (1 << 0),
	eRMF_SONARVISION   = (1 << 1),
	eRMF_NIGHTVISION   = (1 << 2),
	eRMF_MASK          = eRMF_THERMALVISION | eRMF_SONARVISION | eRMF_NIGHTVISION
};

// Base effect parameter class, derive all new from this one
class CEffectParam
{
public:

	CEffectParam(){}

	virtual ~CEffectParam()
	{
		Release();
	}

	// Should implement where necessary. For example check CParamTexture
	virtual void Release() {}

	// Set parameters
	virtual void SetParam(float fParam, bool bForceValue = false)           {}
	virtual void SetParamVec4(const Vec4& pParam, bool bForceValue = false) {}
	virtual void SetParamString(const char* pszParam)                       {}
	virtual void ResetParam(float fParam)                                   { SetParam(fParam, true); }
	virtual void ResetParamVec4(const Vec4& pParam)                         { SetParamVec4(pParam, true); }

	// Get parameters
	virtual float       GetParam()       const { return 1.0f; }
	virtual Vec4        GetParamVec4()   const { return Vec4(1.0f, 1.0f, 1.0f, 1.0f); }
	virtual const char* GetParamString() const { return 0; }

	// Sync main thread data with render thread data
	virtual void SyncMainWithRender() {}

	// Create effect parameter
	template<typename ParamT, typename T> static CEffectParam* Create(const T& pParam, bool bSmoothTransition = true)
	{
		return new ParamT(pParam, bSmoothTransition);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Bool type effect param
class CParamBool : public CEffectParam
{
public:
	CParamBool(bool bParam, bool bSmoothTransition)
	{
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			m_threadSafeData[i].bParam = bParam;
			m_threadSafeData[i].bSetThisFrame = false;
		}
	}

	virtual void  SetParam(float fParam, bool bForceValue);
	virtual float GetParam() const;
	virtual void  SyncMainWithRender();

private:

	struct CParamBoolThreadSafeData
	{
		bool bParam;
		bool bSetThisFrame;
	};

	CParamBoolThreadSafeData m_threadSafeData[RT_COMMAND_BUF_COUNT];
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Int type effect param
class CParamInt : public CEffectParam
{
public:
	CParamInt(int nParam, bool bSmoothTransition)
	{
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			m_threadSafeData[i].nParam = nParam;
			m_threadSafeData[i].bSetThisFrame = false;
		}
	}

	virtual void  SetParam(float fParam, bool bForceValue);
	virtual float GetParam() const;
	virtual void  SyncMainWithRender();

private:

	struct CParamIntThreadSafeData
	{
		int  nParam;
		bool bSetThisFrame;
	};

	CParamIntThreadSafeData m_threadSafeData[RT_COMMAND_BUF_COUNT];
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Float type effect param
class CParamFloat : public CEffectParam
{
public:
	CParamFloat(float fParam, bool bSmoothTransition)
		: m_fParamDefault(fParam)
		, m_bSmoothTransition(bSmoothTransition)
	{
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			m_threadSafeData[i].fParam = fParam;
			m_threadSafeData[i].fFrameParamAcc = fParam;
			m_threadSafeData[i].nFrameSetCount = 0;
			m_threadSafeData[i].bValueForced = false;
		}
	}

	virtual void  SetParam(float fParam, bool bForceValue);
	virtual float GetParam() const;
	virtual void  SyncMainWithRender();

private:

	struct CParamFloatThreadSafeData
	{
		float fParam;
		float fFrameParamAcc;
		uint8 nFrameSetCount;
		bool  bValueForced;
	};

	CParamFloatThreadSafeData m_threadSafeData[RT_COMMAND_BUF_COUNT];

	float                     m_fParamDefault;
	bool                      m_bSmoothTransition;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Vec4 type effect param
class CParamVec4 : public CEffectParam
{
public:
	CParamVec4(const Vec4& vParam, bool bSmoothTransition)
		: m_vParamDefault(vParam)
		, m_bSmoothTransition(bSmoothTransition)
	{
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			m_threadSafeData[i].vParam = vParam;
			m_threadSafeData[i].vFrameParamAcc = vParam;
			m_threadSafeData[i].nFrameSetCount = 0;
			m_threadSafeData[i].bValueForced = false;
		}
	}

	virtual void SetParamVec4(const Vec4& vParam, bool bForceValue);
	virtual Vec4 GetParamVec4() const;
	virtual void SyncMainWithRender();

private:

	struct CParamVec4ThreadSafeData
	{
		Vec4  vParam;
		Vec4  vFrameParamAcc;
		uint8 nFrameSetCount;
		bool  bValueForced;
	};

	CParamVec4ThreadSafeData m_threadSafeData[RT_COMMAND_BUF_COUNT];

	Vec4                     m_vParamDefault;
	bool                     m_bSmoothTransition;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// CTexture type effect param
class CParamTexture : public CEffectParam
{
public:
	CParamTexture()
	{
		Reset();
	}

	CParamTexture(int nInit, bool bSmoothTransition)
	{
		Reset();
	}

	virtual ~CParamTexture()
	{
		Release();
	}

	// Create texture
	int          Create(const char* pszFileName);
	// Release resources
	virtual void Release();

	virtual void SetParamString(const char* pParam)
	{
		Create(pParam);
	}

	virtual const char* GetParamString() const;
	const CTexture*     GetParamTexture() const
	{
		const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
		return m_threadSafeData[threadID].pTexParam;
	}

	virtual void SyncMainWithRender();

private:

	void Reset()
	{
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			m_threadSafeData[i].pTexParam = NULL;
			m_threadSafeData[i].bSetThisFrame = false;
		}
	}

	struct CParamTextureThreadSafeData
	{
		CTexture* pTexParam;
		bool      bSetThisFrame;
	};

	CParamTextureThreadSafeData m_threadSafeData[RT_COMMAND_BUF_COUNT];
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Post processing render flags
enum EPostProcessRenderFlag
{
	PSP_UPDATE_BACKBUFFER        = (1 << 0),    // updates back-buffer texture for technique
	PSP_REQUIRES_UPDATE          = (1 << 1),    // required calling update member function
	PSP_LAST_ENABLED_POSTPROCESS = (1 << 2),    // last enabled post process (used to determine if rendering directly to backbuffer required)
};

// Post effects base structure interface. All techniques derive from this one.
class CPostEffect
{
public:
	CPostEffect() : m_pActive(0), /*m_nID(ePFX_DefaultID),*/ m_nRenderFlags(PSP_UPDATE_BACKBUFFER)
	{
	}

	virtual ~CPostEffect()
	{
		Release();
	}

	// Initialize post processing technique - device access allowed (queries, ...)
	virtual int  Initialize()      { return 1; }
	// Create all the resources for the pp effects which don't require the device (such as textures)
	virtual int  CreateResources() { return 1; }
	// Free resources used
	virtual void Release()         {}
	// Preprocess technique
	virtual bool Preprocess(const SRenderViewInfo& viewInfo)      { return IsActive(); }
	// Some effects might require updating data/parameters, etc
	virtual void Update()          {};
	// Render technique
	virtual void Render() = 0;
	// Reset technique state to default
	virtual void Reset(bool bOnSpecChange = false) = 0;
	// release resources when required
	virtual void OnLostDevice() {}

	// Add render element/object to post process (use for custom geometry)
	virtual void AddRE(const CRenderElement* pRE, const SShaderItem* pShaderItem, CRenderObject* pObj, const SRenderingPassInfo& passInfo) {}
	// release resources when required
	virtual void OnBeginFrame(const SRenderingPassInfo& passInfo)                                                                            {}

	// Get technique render flags
	int GetRenderFlags() const
	{
		return m_nRenderFlags;
	}

	// Get effect name
	virtual const char* GetName() const
	{
		return "PostEffectDefault";
	}

	// Is technique active ?
	virtual bool IsActive() const
	{
		float fActive = m_pActive->GetParam();
		return (fActive) ? 1 : 0;
	}

	inline EPostEffectID GetID() const
	{
		return m_nID;
	}

public:
	CPostEffectContext* GetCurrentContext() { return m_pCurrentContext; }
	void                SetCurrentContext(CPostEffectContext* p) { m_pCurrentContext = p; }

protected:
	uint8         m_nRenderFlags;
	EPostEffectID m_nID;
	CEffectParam* m_pActive;

	// For using in old effects
	CPostEffectContext* m_pCurrentContext = nullptr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::map<string, CEffectParam*> StringEffectMap;
typedef StringEffectMap::iterator       StringEffectMapItor;

typedef std::map<uint32, CEffectParam*> KeyEffectMap;
typedef KeyEffectMap::iterator          KeyEffectMapItor;

typedef std::vector<CPostEffect*>       CPostEffectVec;
typedef CPostEffectVec::iterator        CPostEffectItor;

#ifndef _RELEASE
	#define POSTSEFFECTS_DEBUGINFO_TIMEOUT (1.0f)
struct SPostEffectsDebugInfo
{
	SPostEffectsDebugInfo(CPostEffect* _pEffect) : pEffect(_pEffect), szParamName(NULL), fParamVal(0.0f), fTimeOut(POSTSEFFECTS_DEBUGINFO_TIMEOUT){}
	SPostEffectsDebugInfo(const string& _szParamName, float val) : pEffect(NULL), szParamName(_szParamName), fParamVal(val), fTimeOut(POSTSEFFECTS_DEBUGINFO_TIMEOUT){}
	CPostEffect* pEffect;
	string       szParamName;
	float        fParamVal;
	float        fTimeOut;
};
typedef std::vector<SPostEffectsDebugInfo> CPostEffectDebugVec;
#endif

class CPostEffectsMgr;
static CPostEffectsMgr* PostEffectMgr();

// Post process effects manager
class CPostEffectsMgr
{
public:
	CPostEffectsMgr()
		: m_bPostReset(false)
		, m_bCreated(false)
		, m_nPostBlendEffectsFlags(0)
		// Note: These parameters do not affect the result
		// , m_pBrightness(nullptr)
		// , m_pContrast(nullptr)
		// , m_pSaturation(nullptr)
		// , m_pSharpening(nullptr)
		// , m_pColorC(nullptr)
		// , m_pColorY(nullptr)
		// , m_pColorM(nullptr)
		// , m_pColorK(nullptr)
		// , m_pColorHue(nullptr)
		, m_pUserBrightness(nullptr)
		, m_pUserContrast(nullptr)
		, m_pUserSaturation(nullptr)
		, m_pUserSharpening(nullptr)
		, m_pUserColorC(nullptr)
		, m_pUserColorY(nullptr)
		, m_pUserColorM(nullptr)
		, m_pUserColorK(nullptr)
		, m_pUserColorHue(nullptr)
	{
		ClearCache();
#ifndef _RELEASE
		m_activeEffectsDebug.reserve(16);
		m_activeParamsDebug.reserve(32);
#endif
	}

	virtual ~CPostEffectsMgr()
	{
		Release();
	}

	// Create/Initialize post processing effects
	int  Init();
	void CreateResources();
	// Free resources used
	void Release();
	void ReleaseResources();
	// Reset all post effects
	void Reset(bool bOnSpecChange = false);
	// Start processing effects
	void Begin();
	// End processing effects
	void End(CRenderView* pRenderView);
	// release resources when required
	void OnLostDevice();
	// release resources when required
	void OnBeginFrame(const SRenderingPassInfo& passInfo);
	// Sync main thread post effect data with render thread post effect data
	void SyncMainWithRender();

	// Get techniques list
	CPostEffectVec& GetEffects()
	{
		return m_pEffects;
	}
	inline bool IsCreated()
	{
		return m_pEffects.size() != 0;
	}

	// Get post effect
	CPostEffect* GetEffect(EPostEffectID nID)
	{
		assert(nID < EPostEffectID::Num);
		return m_pEffects[nID];
	}

	// Get post effect ID
	EPostEffectID GetEffectID(const char* pEffectName);

	// Get name to id map
	KeyEffectMap& GetNameIdMap()
	{
		return m_pNameIdMap;
	}

	// Given a string returns corresponding SEffectParam if exists, else returns null
	CEffectParam* GetByName(const char* pszParam);

	// Given a string returns containing value if exists, else returns 0
	float GetByNameF(const char* pszParam);
	Vec4  GetByNameVec4(const char* pszParam);

	// Register effect
	void RegisterEffect(CPostEffect* pEffect)
	{
		assert(pEffect);
		m_pEffects.push_back(pEffect);
	}

	// Register a parameter
	template<typename paramT, typename T>
	void RegisterParam(const char* pszName, CEffectParam*& pParam, const T& pParamVal, bool bSmoothTransition = true)
	{
		pParam = CEffectParam::Create<paramT>(pParamVal, bSmoothTransition);
		m_pNameIdMapGen.insert(StringEffectMapItor::value_type(pszName, pParam));
	}

	// Current enabled post blending effects
	uint8 GetPostBlendEffectsFlags()
	{
		return m_nPostBlendEffectsFlags;
	}

	// Enabled/disable post blending effects
	void SetPostBlendEffectsFlags(uint8 nFlags)
	{
		m_nPostBlendEffectsFlags = nFlags;
	}

	friend CPostEffectsMgr* PostEffectMgr();

	static bool             CheckPostProcessQuality(ERenderQuality nMinRQ, EShaderQuality nMinSQ)
	{
		if (gRenDev->EF_GetRenderQuality() >= nMinRQ && gRenDev->EF_GetShaderQuality(eST_PostProcess) >= nMinSQ)
			return true;

		return false;
	}

	StringEffectMap* GetDebugParamsUsedInFrame()
	{
		return &m_pEffectParamsUpdated;
	}

#ifndef _RELEASE
	CPostEffectDebugVec& GetActiveEffectsDebug()       { return m_activeEffectsDebug; }
	CPostEffectDebugVec& GetActiveEffectsParamsDebug() { return m_activeParamsDebug; }
	void                 ClearDebugInfo()
	{
		m_activeEffectsDebug.clear();
		m_activeParamsDebug.clear();
	}
#endif

	static int SortEffectsByID(const CPostEffect* p1, const CPostEffect* p2);

	// Get techniques list
	CPostEffectVec& GetActiveEffects(int threadID)
	{
		return m_activeEffects[threadID];
	}

private:

	// Pass a text string to this function and it will return the CRC
	uint32 GetCRC(const char* pszName);
	// Used only when creating the crc table
	uint32 CRC32Reflect(uint32 ref, char ch);
	// zero out the cache
	void   ClearCache()
	{
#ifndef _RELEASE
		ClearDebugInfo();
#endif
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
		{
			m_pParamCache[i].m_nKey = 0;
			m_pParamCache[i].m_pParam = 0;
		}
	}

protected:

	bool  m_bPostReset;
	bool  m_bCreated;
	uint8 m_nPostBlendEffectsFlags;

	// Shared parameters
	// Note: These parameters do not affect the result
	// CEffectParam *m_pBrightness, *m_pContrast, *m_pSaturation, *m_pSharpening;
	// CEffectParam *m_pColorC, *m_pColorY, *m_pColorM, *m_pColorK, *m_pColorHue;

	CEffectParam*       m_pUserBrightness, * m_pUserContrast, * m_pUserSaturation, * m_pUserSharpening;
	CEffectParam*       m_pUserColorC, * m_pUserColorY, * m_pUserColorM, * m_pUserColorK, * m_pUserColorHue;

	CPostEffectVec      m_pEffects;
	CPostEffectVec      m_activeEffects[RT_COMMAND_BUF_COUNT];
	KeyEffectMap        m_pNameIdMap;
	StringEffectMap     m_pNameIdMapGen;
	// for debugging purposes only
	StringEffectMap     m_pEffectParamsUpdated;
#ifndef _RELEASE
	CPostEffectDebugVec m_activeEffectsDebug;
	CPostEffectDebugVec m_activeParamsDebug;
#endif

	uint32 m_nCRC32Table[256];  // Lookup table array

	struct ParamCache
	{
		uint32        m_nKey;
		CEffectParam* m_pParam;
	} m_pParamCache[RT_COMMAND_BUF_COUNT];
};

static CPostEffectsMgr* PostEffectMgr()
{
	return gRenDev->m_pPostProcessMgr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Some nice utilities for handling post effects containers
//////////////////////////////////////////////////////////////////////////////////////////////////

#define AddEffect(ef)                                  PostEffectMgr()->RegisterEffect(CryAlignedNew<ef>())
#define AddParamBool(szName, pParam, val)              PostEffectMgr()->RegisterParam<CParamBool, bool>((szName), (pParam), val)
#define AddParamInt(szName, pParam, val)               PostEffectMgr()->RegisterParam<CParamInt, int>((szName), (pParam), val)
#define AddParamFloat(szName, pParam, val)             PostEffectMgr()->RegisterParam<CParamFloat, float>((szName), (pParam), val)
#define AddParamVec4(szName, pParam, val)              PostEffectMgr()->RegisterParam<CParamVec4, Vec4>((szName), (pParam), val)
#define AddParamFloatNoTransition(szName, pParam, val) PostEffectMgr()->RegisterParam<CParamFloat, float>((szName), (pParam), val, false)
#define AddParamVec4NoTransition(szName, pParam, val)  PostEffectMgr()->RegisterParam<CParamVec4, Vec4>((szName), (pParam), val, false)
#define AddParamTex(szName, pParam, val)               PostEffectMgr()->RegisterParam<CParamTexture, int>((szName), (pParam), val)

struct container_object_safe_delete
{
	template<typename T>
	void operator()(T* pObj) const
	{
		CryAlignedDelete(pObj);
	}
};

struct container_object_safe_release
{
	template<typename T>
	void operator()(T* pObj) const
	{
		SAFE_RELEASE(pObj);
	}
};

struct SContainerKeyEffectParamDelete
{
	void operator()(KeyEffectMap::value_type& pObj)
	{
		SAFE_DELETE(pObj.second);
	}
};

struct SContainerPostEffectInitialize
{
	void operator()(CPostEffect* pObj) const
	{
		if (pObj)
		{
			pObj->Initialize();
		}
	}
};

struct SContainerPostEffectCreateResources
{
	void operator()(CPostEffect* pObj) const
	{
		if (pObj)
		{
			pObj->CreateResources();
		}
	}
};

struct SContainerPostEffectReset
{
	void operator()(CPostEffect* pObj) const
	{
		if (pObj)
		{
			pObj->Reset();
		}
	}
};

struct SContainerPostEffectResetOnSpecChange
{
	void operator()(CPostEffect* pObj) const
	{
		if (pObj)
		{
			pObj->Reset(true);
		}
	}
};

struct SContainerPostEffectOnLostDevice
{
	void operator()(CPostEffect* pObj) const
	{
		if (pObj)
			pObj->OnLostDevice();
	}
};

struct SContainerPostEffectOnBeginFrame
{
	const SRenderingPassInfo& localPassInfo;
	SContainerPostEffectOnBeginFrame(const SRenderingPassInfo& passInfo) : localPassInfo(passInfo) {}
	void operator()(CPostEffect* pObj) const
	{
		if (pObj)
			pObj->OnBeginFrame(localPassInfo);
	}
};

#endif
