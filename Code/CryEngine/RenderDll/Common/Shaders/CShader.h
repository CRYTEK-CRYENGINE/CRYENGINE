// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CSHADER_H__
#define __CSHADER_H__

#include <map>
#include "CShaderBin.h"
#include "ShaderSerialize.h"
#include "ShaderCache.h"
#include "ShaderComponents.h" // ECGParam
#include "../ResFileLookupDataMan.h"

struct SRenderBuf;
class CRenderElement;
struct SEmitter;
struct SParticleInfo;
struct SPartMoveStage;
struct SSunFlare;

//===================================================================

struct SMacroFX
{
	string m_szMacro;
	uint32 m_nMask;
};

//typedef std::unordered_map<string, SMacroFX, stl::hash_strcmp<string>, stl::hash_strcmp<string>> FXMacro;
//typedef std::map<string,SMacroFX> FXMacro;
typedef std::unordered_map<string, SMacroFX, stl::hash_strcmp<const char*>, stl::hash_strcmp<const char*>> FXMacro;

typedef FXMacro::iterator                                                                                  FXMacroItor;

//////////////////////////////////////////////////////////////////////////
// Helper class for shader parser, holds temporary strings vector etc...
//////////////////////////////////////////////////////////////////////////
struct CShaderParserHelper
{
	CShaderParserHelper()
	{
	}
	~CShaderParserHelper()
	{
	}

	char* GetTempStringArray(int nIndex, int nLen)
	{
		m_tempString.reserve(nLen + 1);
		return (char*)&(m_tempStringArray[nIndex])[0];
	}

	std::vector<char> m_tempStringArray[32];
	std::vector<char> m_tempString;
};
extern CShaderParserHelper* g_pShaderParserHelper;

enum EShaderFlagType
{
	eSFT_Global = 0,
	eSFT_Runtime,
	eSFT_MDV,
	eSFT_LT,
};

enum EShaderFilterOperation
{
	eSFO_Expand = 0,  // expand all permutations of the mask
	eSFO_And,         // and against the mask
	eSFO_Eq,          // set the mask
};

// includes or excludes
struct CShaderListFilter
{
	CShaderListFilter() :
		m_bInclude(true)
	{}

	bool   m_bInclude;
	string m_ShaderName;

	struct Predicate
	{
		Predicate() :
			m_Negated(false),
			m_Flags(eSFT_Global),
			m_Op(eSFO_And),
			m_Mask(0)
		{}

		bool                   m_Negated;
		EShaderFlagType        m_Flags;
		EShaderFilterOperation m_Op;
		uint64                 m_Mask;
	};
	std::vector<Predicate> m_Predicates;
};

//==================================================================================

#define PD_INDEXED 1
#define PD_MERGED  4

struct SParamDB
{
	const char* szName;
	const char* szAliasName;
	ECGParam    eParamType;
	uint32      nFlags;
	void        (* ParserFunc)(const char* szScr, const char* szAnnotations, SCGParam* vpp, int nComp, CShader* ef);
	SParamDB()
		: szName(nullptr)
		, szAliasName(nullptr)
		, eParamType(ECGP_Unknown)
		, nFlags(0)
		, ParserFunc(nullptr)
	{
	}
	SParamDB(const char* inName, ECGParam ePrmType, uint32 inFlags)
	{
		szName = inName;
		szAliasName = NULL;
		nFlags = inFlags;
		ParserFunc = NULL;
		eParamType = ePrmType;
	}
	SParamDB(const char* inName, ECGParam ePrmType, uint32 inFlags, void(*InParserFunc)(const char* szScr, const char* szAnnotations, SCGParam * vpp, int nComp, CShader * ef))
	{
		szName = inName;
		szAliasName = NULL;
		nFlags = inFlags;
		ParserFunc = InParserFunc;
		eParamType = ePrmType;
	}
};

struct SSamplerDB
{
	const char* szName;
	ECGSampler  eSamplerType;
	uint32      nFlags;
	void        (* ParserFunc)(const char* szScr, const char* szAnnotations, std::vector<SFXSampler>* pSamplers, SCGSampler* vpp, CShader* ef);
	SSamplerDB()
	{
		szName = NULL;
		nFlags = 0;
		ParserFunc = NULL;
		eSamplerType = ECGS_Unknown;
	}
	SSamplerDB(const char* inName, ECGSampler ePrmType, uint32 inFlags)
	{
		szName = inName;
		nFlags = inFlags;
		ParserFunc = NULL;
		eSamplerType = ePrmType;
	}
	SSamplerDB(const char* inName, ECGSampler ePrmType, uint32 inFlags, void(*InParserFunc)(const char* szScr, const char* szAnnotations, std::vector<SFXSampler>* pSamplers, SCGSampler * vpp, CShader * ef))
	{
		szName = inName;
		nFlags = inFlags;
		ParserFunc = InParserFunc;
		eSamplerType = ePrmType;
	}
};

struct STextureDB
{
	const char* szName;
	ECGTexture  eTextureType;
	uint32      nFlags;
	void        (* ParserFunc)(const char* szScr, const char* szAnnotations, std::vector<SFXTexture>* pSamplers, SCGTexture* vpp, CShader* ef);
	STextureDB()
	{
		szName = NULL;
		nFlags = 0;
		ParserFunc = NULL;
		eTextureType = ECGT_Unknown;
	}
	STextureDB(const char* inName, ECGTexture ePrmType, uint32 inFlags)
	{
		szName = inName;
		nFlags = inFlags;
		ParserFunc = NULL;
		eTextureType = ePrmType;
	}
	STextureDB(const char* inName, ECGTexture ePrmType, uint32 inFlags, void(*InParserFunc)(const char* szScr, const char* szAnnotations, std::vector<SFXTexture>* pSamplers, SCGTexture * vpp, CShader * ef))
	{
		szName = inName;
		nFlags = inFlags;
		ParserFunc = InParserFunc;
		eTextureType = ePrmType;
	}
};

enum EShaderCacheMode
{
	eSC_Normal          = 0,
	eSC_BuildGlobal     = 2,
	eSC_BuildGlobalList = 3,
	eSC_Preactivate     = 4,
};

//////////////////////////////////////////////////////////////////////////
class CShaderMan :
	public ISystemEventListener
#if defined(SHADERS_SERIALIZING)
	, public CShaderSerialize
#endif
{
	friend class CShader;
	friend class CParserBin;

	//////////////////////////////////////////////////////////////////////////
	// ISystemEventListener interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	//////////////////////////////////////////////////////////////////////////

private:
	STexAnim*      mfReadTexSequence(const char* name, int Flags, bool bFindOnly);
	int            mfReadTexSequence(STexSamplerRT* smp, const char* name, int Flags, bool bFindOnly);

	CShader*       mfNewShader(const char* szName);

	bool           mfCompileShaderGen(SShaderGen* shg, char* scr);
	SShaderGenBit* mfCompileShaderGenProperty(char* scr);

	void           mfSetResourceTexState(SEfResTexture* Tex);
	CTexture*      mfTryToLoadTexture(const char* nameTex, STexSamplerRT* smp, int Flags, bool bFindOnly);
	void           mfRefreshResourceTextureConstants(EEfResTextures Id, SInputShaderResources& RS);
	void           mfRefreshResourceTextureConstants(EEfResTextures Id, CShaderResources& RS);
	CTexture*      mfFindResourceTexture(const char* nameTex, const char* path, int Flags, SEfResTexture* Tex);
	CTexture*      mfLoadResourceTexture(const char* nameTex, const char* path, int Flags, SEfResTexture* Tex);
	bool           mfLoadResourceTexture(EEfResTextures Id, SInputShaderResources& RS, uint32 CustomFlags, bool bReplaceMeOnFail = false);
	bool           mfLoadResourceTexture(EEfResTextures Id, CShaderResources& RS, uint32 CustomFlags, bool bReplaceMeOnFail = false);
	void           mfLoadDefaultTexture(EEfResTextures Id, CShaderResources& RS, EEfResTextures Alias);
	void           mfCheckShaderResTextures(TArray<SShaderPass>& Dst, CShader* ef, CShaderResources* Res);
	void           mfCheckShaderResTexturesHW(TArray<SShaderPass>& Dst, CShader* ef, CShaderResources* Res);
	CTexture*      mfCheckTemplateTexName(const char* mapname, ETEX_Type eTT);

	CShader*       mfCompile(CShader* ef, char* scr);

	void           mfRefreshResources(CShaderResources* Res, const IRenderer::SLoadShaderItemArgs* pArgs = 0);

	bool           mfReloadShaderFile(const char* szName, int nFlags);
#if CRY_PLATFORM_DESKTOP
	bool           CheckAllFilesAreWritable(const char* szDir) const;
#endif

	static void    FilterShaderCacheGenListForOrbis(FXShaderCacheCombinations& combinations);

public:
	char*                 m_pCurScript;
	CShaderManBin         m_Bin;
	CResFileLookupDataMan m_ResLookupDataMan[2];  // cacheSource::readonly, cacheSource::user

	const char*     mfTemplateTexIdToName(int Id);
	SShaderGenComb* mfGetShaderGenInfo(const char* nmFX);

	bool            mfReloadShaderIncludes(const char* szPath, int nFlags);
	bool            mfReloadAllShaders(int nFlags, uint32 nFlagsHW, int currentFrameID);
	bool            mfReloadFile(const char* szPath, const char* szName, int nFlags);

	void            ParseShaderProfiles();
	void            ParseShaderProfile(char* scr, SShaderProfile* pr);

	EEfResTextures  mfCheckTextureSlotName(const char* mapname);
	SParamDB*       mfGetShaderParamDB(const char* szSemantic);
	const char*     mfGetShaderParamName(ECGParam ePR);
	bool            mfParseParamComp(int comp, SCGParam* pCurParam, const char* szSemantic, char* params, const char* szAnnotations, SShaderFXParams& FXParams, CShader* ef, uint32 nParamFlags, EHWShaderClass eSHClass, bool bExpressionOperand);
	bool            mfParseCGParam(char* scr, const char* szAnnotations, SShaderFXParams& FXParams, CShader* ef, std::vector<SCGParam>* pParams, int nComps, uint32 nParamFlags, EHWShaderClass eSHClass, bool bExpressionOperand);
	bool            mfParseFXParameter(SShaderFXParams& FXParams, SFXParam* pr, const char* ParamName, CShader* ef, bool bInstParam, int nParams, std::vector<SCGParam>* pParams, EHWShaderClass eSHClass, bool bExpressionOperand);

	bool            mfParseFXTexture(SShaderFXParams& FXParams, SFXTexture* pr, const char* ParamName, CShader* ef, int nParams, std::vector<SCGTexture>* pParams, EHWShaderClass eSHClass);
	bool            mfParseFXSampler(SShaderFXParams& FXParams, SFXSampler* pr, const char* ParamName, CShader* ef, int nParams, std::vector<SCGSampler>* pParams, EHWShaderClass eSHClass);

	void            mfCheckObjectDependParams(std::vector<SCGParam>& PNoObj, std::vector<SCGParam>& PObj, EHWShaderClass eSH, CShader* pFXShader);

	void            mfBeginFrame();

	void            mfGetShaderListPath(stack_string& nameOut, int nType);

public:
	bool                       m_bInitialized;
	bool                       m_bLoadedSystem;

	string                     m_ShadersGamePath;
	string                     m_ShadersGameExtPath;
	const char*                m_ShadersPath;
	const char*                m_ShadersExtPath;
	const char*                m_ShadersCache;
	const char*                m_ShadersFilter;
	const char*                m_ShadersMergeCachePath;
	string                     m_szUserPath;

	int                        m_nFrameForceReload;

	char                       m_HWPath[128];

	CShader*                   m_pCurShader;
	static SResourceContainer* s_pContainer;  // List/Map of objects for shaders resource class

	std::vector<string>        m_ShaderNames;

	static CCryNameTSCRC       s_cNameHEAD;

	static CShader*            s_DefaultShader;
	static CShader*            s_shPostEffects;      // engine specific post process effects
	static CShader*            s_shPostDepthOfField; // depth of field
	static CShader*            s_shPostMotionBlur;
	static CShader*            s_shPostSunShafts;
	static CShader*            s_sh3DHUD;

	// Deferred rendering passes
	static CShader*              s_shDeferredShading;
	static CShader*              s_ShaderDeferredCaustics;
	static CShader*              s_ShaderDeferredRain;
	static CShader*              s_ShaderDeferredSnow;

	static CShader*              s_ShaderFPEmu;
	static CShader*              s_ShaderFallback;
	static CShader*              s_ShaderScaleForm;
	static CShader*              s_ShaderStars;
	static CShader*              s_ShaderShadowBlur;
	static CShader*              s_ShaderShadowMaskGen;
#if defined(FEATURE_SVO_GI)
	static CShader*              s_ShaderSVOGI;
#endif
	static CShader*              s_shHDRPostProcess;
	static CShader*              s_shPostEffectsGame; // game specific post process effects
	static CShader*              s_shPostEffectsRenderModes;
	static CShader*              s_shPostAA;
	static CShader*              s_ShaderDebug;
	static CShader*              s_ShaderLensOptics;
	static CShader*              s_ShaderSoftOcclusionQuery;
	static CShader*              s_ShaderLightStyles;
	static CShader*              s_ShaderCommon;
	static CShader*              s_ShaderOcclTest;
	static CShader*              s_ShaderDXTCompress;
	static CShader*              s_ShaderStereo;
	static CShader*              s_ShaderClouds;
	static CShader*              s_ShaderMobileComposition;
	static CShader*              s_ShaderGpuParticles;

	const SInputShaderResources* m_pCurInputResources;
	SShaderGen*                  m_pGlobalExt;

	std::vector<SShaderGenComb>  m_SGC;

	int                          m_nCombinationsProcess;
	int                          m_nCombinationsProcessOverall;
	int                          m_nCombinationsCompiled;
	int                          m_nCombinationsEmpty;

	EShaderCacheMode             m_eCacheMode;

	char*                        m_szShaderPrecache;

	FXShaderCacheCombinations    m_ShaderCacheCombinations[2];
	FXShaderCacheCombinations    m_ShaderCacheExportCombinations;
	FILE*                        m_FPCacheCombinations[2];

	typedef std::vector<CCryNameTSCRC, stl::STLGlobalAllocator<CCryNameTSCRC>> ShaderCacheMissesVec;
	ShaderCacheMissesVec    m_ShaderCacheMisses;
	string                  m_ShaderCacheMissPath;
	ShaderCacheMissCallback m_ShaderCacheMissCallback;

	SShaderCacheStatistics  m_ShaderCacheStats;

	uint32                  m_nFrameLastSubmitted;
	uint32                  m_nFrameSubmit;
	SShaderProfile          m_ShaderProfiles[eST_Max];
	SShaderProfile          m_ShaderFixedProfiles[eSQ_Max];

	int                     m_bActivated;

	CShaderParserHelper     m_shaderParserHelper;

	bool                    m_bReload;

	// Shared common global flags data

	// Map used for storing automatically-generated flags and mapping old shader names masks to generated ones
	//  map< shader flag names, mask >
	typedef std::map<string, uint64> MapNameFlags;
	typedef MapNameFlags::iterator   MapNameFlagsItor;
	MapNameFlags m_pShaderCommonGlobalFlag;

	MapNameFlags m_pSCGFlagLegacyFix;
	uint64       m_nSGFlagsFix;

	// Map stored for convenience mapping betweens old flags and new ones
	//  map < shader name , map< shader flag names, mask > >
	typedef std::map<string, MapNameFlags*> ShaderMapNameFlags;
	typedef ShaderMapNameFlags::iterator    ShaderMapNameFlagsItor;
	ShaderMapNameFlags m_pShadersGlobalFlags;

	typedef std::map<CCryNameTSCRC, SShaderGen*> ShaderExt;
	typedef ShaderExt::iterator                  ShaderExtItor;
	ShaderExt   m_ShaderExts;

	// Concatenated list of shader names using automatic masks generation
	string m_pShadersRemapList;

	// Helper functors for cleaning up

	struct SShaderMapNameFlagsContainerDelete
	{
		void operator()(ShaderMapNameFlags::value_type& pObj)
		{
			SAFE_DELETE(pObj.second);
		}
	};

public:
	CShaderMan();

	void              ShutDown();
	void              mfReleaseShaders();

	SShaderGen*       mfCreateShaderGenInfo(const char* szName, bool bRuntime);
	void              mfRemapShaderGenInfoBits(const char* szName, SShaderGen* pShGen);

	uint64            mfGetRemapedShaderMaskGen(const char* szName, uint64 nMaskGen = 0, bool bFixup = 0);
	string            mfGetShaderBitNamesFromMaskGen(const char* szName, uint64 nMaskGen);

	bool              mfUsesGlobalFlags(const char* szShaderName);
	const char*       mfGetShaderBitNamesFromGlobalMaskGen(uint64 nMaskGen);
	uint64            mfGetShaderGlobalMaskGenFromString(const char* szShaderGen);

	void              mfInitGlobal(void);
	void              mfInitLookups(void);

	void              mfPreloadShaderExts(void);
	void              mfInitCommonGlobalFlags(void);
	void              mfInitCommonGlobalFlagsLegacyFix(void);
	bool              mfRemapCommonGlobalFlagsWithLegacy(void);
	void              mfCreateCommonGlobalFlags(const char* szName);
	void              mfSaveCommonGlobalFlagsToDisk(const char* szName, uint32 nMaskCount);

	void              mfInit(void);
	void              mfPostInit(void);
	void              mfSortResources();
	CShaderResources* mfCreateShaderResources(const SInputShaderResources* Res, bool bShare);
	bool              mfRefreshResourceConstants(CShaderResources* Res);
	inline bool       mfRefreshResourceConstants(SShaderItem& SI) { return mfRefreshResourceConstants((CShaderResources*)SI.m_pShaderResources); }
	bool              mfUpdateTechnik(SShaderItem& SI, CCryNameTSCRC& Name);
	SShaderItem       mfShaderItemForName(const char* szName, bool bShare, int flags, SInputShaderResources* Res = NULL, uint64 nMaskGen = 0, const IRenderer::SLoadShaderItemArgs* pArgs = 0);
	CShader*          mfForName(const char* name, int flags, const CShaderResources* Res = NULL, uint64 nMaskGen = 0);

	bool              mfRefreshSystemShader(const char* szName, CShader*& pSysShader)
	{
		if (!pSysShader)
		{
			CryComment("Load System Shader '%s'...", szName);

			if (pSysShader = mfForName(szName, EF_SYSTEM))
				return true;
		}

		return false;
	}

	void              RT_ParseShader(CShader* pSH, uint64 nMaskGen, uint32 flags, CShaderResources* Res);
	void              RT_SetShaderQuality(EShaderType eST, EShaderQuality eSQ);

	void              CreateShaderMaskGenString(const CShader* pSH, stack_string& flagString);
	void              CreateShaderExportRequestLine(const CShader* pSH, stack_string& exportString);

	SFXParam*         mfGetFXParameter(std::vector<SFXParam>& Params, const char* param);
	SFXSampler*       mfGetFXSampler(std::vector<SFXSampler>& Params, const char* param);
	SFXTexture*       mfGetFXTexture(std::vector<SFXTexture>& Params, const char* param);
	const char*       mfParseFX_Parameter(const string& buf, EParamType eType, const char* szName);
	void              mfParseFX_Annotations_Script(char* buf, CShader* ef, std::vector<SFXStruct>& Structs, bool* bPublic, CCryNameR techStart[2]);
	void              mfParseFX_Annotations(char* buf, CShader* ef, std::vector<SFXStruct>& Structs, bool* bPublic, CCryNameR techStart[2]);
	void              mfParseFXTechnique_Annotations_Script(char* buf, CShader* ef, std::vector<SFXStruct>& Structs, SShaderTechnique* pShTech, bool* bPublic, std::vector<SShaderTechParseParams>& techParams);
	void              mfParseFXTechnique_Annotations(char* buf, CShader* ef, std::vector<SFXStruct>& Structs, SShaderTechnique* pShTech, bool* bPublic, std::vector<SShaderTechParseParams>& techParams);
	void              mfParseFX_Global(SFXParam& pr, CShader* ef, std::vector<SFXStruct>& Structs, CCryNameR techStart[2]);
	bool              mfParseDummyFX_Global(std::vector<SFXStruct>& Structs, char* annot, CCryNameR techStart[2]);
	const string&     mfParseFXTechnique_GenerateShaderScript(std::vector<SFXStruct>& Structs, FXMacro& Macros, std::vector<SFXParam>& Params, std::vector<SFXParam>& AffectedParams, const char* szEntryFunc, CShader* ef, EHWShaderClass eSHClass, const char* szShaderName, uint32& nAffectMask, const char* szType);
	bool              mfParseFXTechnique_MergeParameters(std::vector<SFXStruct>& Structs, std::vector<SFXParam>& Params, std::vector<int>& AffectedFunc, SFXStruct* pMainFunc, CShader* ef, EHWShaderClass eSHClass, const char* szShaderName, std::vector<SFXParam>& NewParams);
	CTexture*         mfParseFXTechnique_LoadShaderTexture(STexSamplerRT* smp, const char* szName, SShaderPass* pShPass, CShader* ef, int nIndex, byte ColorOp, byte AlphaOp, byte ColorArg, byte AlphaArg);
	bool              mfParseFXTechnique_CustomRE(char* buf, const char* name, SShaderTechnique* pShTech, CShader* ef);
	bool              mfParseLightStyle(CLightStyle* ls, char* buf);
	bool              mfParseFXLightStyle(char* buf, int nID, CShader* ef, std::vector<SFXStruct>& Structs);
	CShader*          mfParseFX(char* buf, CShader* ef, CShader* efGen, uint64 nMaskGen);
	void              mfPostLoadFX(CShader* efT, std::vector<SShaderTechParseParams>& techParams, CCryNameR techStart[2]);
	bool              mfParseDummyFX(char* buf, std::vector<string>& ShaderNames, const char* szName);
	bool              mfAddFXShaderNames(const char* szName, std::vector<string>* ShaderNames, bool bUpdateCRC);
	bool              mfInitShadersDummy(bool bUpdateCRC);

	uint64            mfGetRTForName(char* buf);
	uint32            mfGetGLForName(char* buf, CShader* ef);

	void              mfFillGenMacroses(SShaderGen* shG, TArray<char>& buf, uint64 nMaskGen);
	bool              mfModifyGenFlags(CShader* efGen, const CShaderResources* pRes, uint64& nMaskGen, uint64& nMaskGenHW);

	bool              mfGatherShadersList(const char* szPath, bool bCheckIncludes, bool bUpdateCRC, std::vector<string>* Names);
	void              mfGatherFilesList(const char* szPath, std::vector<CCryNameR>& Names, int nLevel, bool bUseFilter, bool bMaterial = false);
	int               mfInitShadersList(std::vector<string>* ShaderNames);
	void              mfSetDefaults(void);
	void              mfReleaseSystemShaders();
	void              mfLoadBasicSystemShaders();
	void              mfLoadDefaultSystemShaders();
	void              mfCloseShadersCache(int nID);
	void              mfInitShadersCacheMissLog();

	void              mfInitShadersCache(byte bForLevel, FXShaderCacheCombinations* Combinations = NULL, const char* pCombinations = NULL, int nType = 0);
	void              mfMergeShadersCombinations(FXShaderCacheCombinations* Combinations, int nType);
	void              mfInsertNewCombination(SShaderCombIdent& Ident, EHWShaderClass eCL, const char* name, int nID, string* Str = NULL, byte bStore = 1);
	string            mfGetShaderCompileFlags(EHWShaderClass eClass, UPipelineState pipelineState) const;

	bool              mfPreloadBinaryShaders();

	bool              LoadShaderStartupCache();
	void              UnloadShaderStartupCache();

#if CRY_PLATFORM_DESKTOP
	void AddCombination(SCacheCombination& cmb, FXShaderCacheCombinations& CmbsMap, CHWShader* pHWS);
	void AddGLCombinations(CShader* pSH, std::vector<SCacheCombination>& CmbsGL);
	void AddLTCombinations(SCacheCombination& cmb, FXShaderCacheCombinations& CmbsMap, CHWShader* pHWS);
	void AddRTCombinations(FXShaderCacheCombinations& CmbsMap, CHWShader* pHWS, CShader* pSH, FXShaderCacheCombinations* Combinations);
	void AddGLCombination(FXShaderCacheCombinations& CmbsMap, SCacheCombination& cc);
	void FilterShaderCombinations(std::vector<SCacheCombination>& Cmbs, const std::vector<CShaderListFilter>& Filters);
	void mfPrecacheShaders(bool bStatsOnly);
	void _PrecacheShaderList(bool bStatsOnly);

	void mfAddRTCombinations(FXShaderCacheCombinations& CmbsMapSrc, FXShaderCacheCombinations& CmbsMapDst, CHWShader* pSH, bool bListOnly);
	void mfAddRTCombination_r(int nComb, FXShaderCacheCombinations& CmbsMapDst, SCacheCombination* cmb, CHWShader* pSH, bool bAutoPrecache);
	void mfAddLTCombinations(SCacheCombination* cmb, FXShaderCacheCombinations& CmbsMapDst);
	void mfAddLTCombination(SCacheCombination* cmb, FXShaderCacheCombinations& CmbsMapDst, DWORD dwL);
#endif

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( push )           //AMD Port
	#pragma warning( disable : 4267 ) // conversion from 'size_t' to 'XXX', possible loss of data
#endif

	int Size()
	{
		int nSize = sizeof(*this);

		nSize += m_SGC.capacity();
		nSize += m_Bin.Size();

		return nSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_Bin);
		pSizer->AddObject(m_SGC);
		pSizer->AddObject(m_ShaderNames);
		pSizer->AddObject(m_ShaderCacheCombinations[0]);
		pSizer->AddObject(m_ShaderCacheCombinations[1]);
	}

	static float EvalWaveForm(SWaveForm* wf);
	static float EvalWaveForm(SWaveForm2* wf);
	static float EvalWaveForm2(SWaveForm* wf, float frac);
};

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop ) //AMD Port
#endif

//=====================================================================

#endif                   // __CSHADER_H__
