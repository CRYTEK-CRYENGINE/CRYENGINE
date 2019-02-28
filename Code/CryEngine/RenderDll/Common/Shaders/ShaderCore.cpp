// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShaderCore.cpp : implementation of the Shaders manager.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/CryHeaders.h>

#include "DriverD3D.h"
#include "Common/RenderView.h"
#include "Common/RendererResources.h"

CShader* CShaderMan::s_DefaultShader;
CShader* CShaderMan::s_shPostEffects;
CShader* CShaderMan::s_shPostDepthOfField;
CShader* CShaderMan::s_shPostMotionBlur;
CShader* CShaderMan::s_shPostSunShafts;
CShader* CShaderMan::s_sh3DHUD;
CShader* CShaderMan::s_shDeferredShading;
CShader* CShaderMan::s_ShaderDeferredCaustics;
CShader* CShaderMan::s_ShaderDeferredRain;
CShader* CShaderMan::s_ShaderDeferredSnow;

CShader* CShaderMan::s_ShaderFPEmu;
CShader* CShaderMan::s_ShaderFallback;
CShader* CShaderMan::s_ShaderScaleForm;
CShader* CShaderMan::s_ShaderStars;
CShader* CShaderMan::s_ShaderShadowBlur;
CShader* CShaderMan::s_ShaderShadowMaskGen;
#if defined(FEATURE_SVO_GI)
CShader* CShaderMan::s_ShaderSVOGI;
#endif
CShader* CShaderMan::s_shHDRPostProcess;
CShader* CShaderMan::s_ShaderDebug;
CShader* CShaderMan::s_ShaderLensOptics;
CShader* CShaderMan::s_ShaderSoftOcclusionQuery;
CShader* CShaderMan::s_ShaderLightStyles;
CShader* CShaderMan::s_shPostEffectsGame;
CShader* CShaderMan::s_shPostEffectsRenderModes;
CShader* CShaderMan::s_shPostAA;
CShader* CShaderMan::s_ShaderCommon;
CShader* CShaderMan::s_ShaderOcclTest;
CShader* CShaderMan::s_ShaderDXTCompress = nullptr;
CShader* CShaderMan::s_ShaderStereo = nullptr;
CShader* CShaderMan::s_ShaderClouds = nullptr;
CShader* CShaderMan::s_ShaderGpuParticles = nullptr;
CShader* CShaderMan::s_ShaderMobileComposition = nullptr;
CCryNameTSCRC CShaderMan::s_cNameHEAD;

TArray<CShaderResources*> CShader::s_ShaderResources_known(0, 2600);  // Based on BatteryPark
TArray<CLightStyle*> CLightStyle::s_LStyles;

SResourceContainer* CShaderMan::s_pContainer;                        // List/Map of objects for shaders resource class

uint64 g_HWSR_MaskBit[HWSR_MAX];

//////////////////////////////////////////////////////////////////////////
// WaveForm table
struct SStaticSinusTable
{
	static const uint32 sSinTableCount = 1024;
	static float m_tSinTable[sSinTableCount];

	static void InitWaveTables()
	{
		//Init wave Tables
		for (uint32 i = 0; i < sSinTableCount; i++)
		{
			float f = (float)i;
			m_tSinTable[i] = sin_tpl(f * (360.0f / (float)sSinTableCount) * (float)M_PI / 180.0f);
		}
	}
};
float SStaticSinusTable::m_tSinTable[sSinTableCount];

////////////////////////////////////////////////////////////////////////////////
// Pool for texture modificators

#if POOL_TEXMODIFICATORS

SEfTexModPool::ModificatorList SEfTexModPool::s_pool;
volatile int SEfTexModPool::s_lockState = 0;

SEfTexModificator* SEfTexModPool::Add(SEfTexModificator& mod)
{
	mod.m_crc = CCrc32::Compute((const char*)&mod, sizeof(SEfTexModificator) - sizeof(uint16) - sizeof(uint32));
	ModificatorList::iterator it = s_pool.find(mod.m_crc);
	if (it != s_pool.end())
	{
		++((*it).second->m_refs);
		return it->second;
	}
	mod.m_refs = 1;
	SEfTexModificator* pMod = new SEfTexModificator(mod);
	s_pool.insert(std::pair<uint32, SEfTexModificator*>(mod.m_crc, pMod));

	return pMod;
}

void SEfTexModPool::AddRef(SEfTexModificator* pMod)
{
	Lock();
	if (pMod)
	{
		++(pMod->m_refs);
	}
	Unlock();
}

void SEfTexModPool::Remove(SEfTexModificator* pMod)
{
	Lock();
	Remove_NoLock(pMod);
	Unlock();
}

void SEfTexModPool::Remove_NoLock(SEfTexModificator* pMod)
{
	if (pMod)
	{
		if (pMod->m_refs > 1)
		{
			--(pMod->m_refs);
		}
		else
		{
			ModificatorList::iterator it = s_pool.find(pMod->m_crc);
			if (it != s_pool.end())
			{
				delete pMod;
				s_pool.erase(it);
			}
		}
	}
}

void SEfTexModPool::Update(SEfTexModificator*& pMod, SEfTexModificator& newMod)
{
	Lock();
	if (pMod)
	{
		if (pMod->m_refs == 1)
		{
			*pMod = newMod;
		}
		else if (memcmp(pMod, &newMod, sizeof(SEfTexModificator) - sizeof(uint16) - sizeof(uint32)))            // Ignore reference count and crc
		{
			Remove_NoLock(pMod);
			pMod = Add(newMod);
		}
	}
	else
	{
		pMod = Add(newMod);
	}
	Unlock();
}

void SEfTexModPool::Lock(void)
{
	CrySpinLock(&s_lockState, 0, 1);
	// Locked
}

void SEfTexModPool::Unlock(void)
{
	CrySpinLock(&s_lockState, 1, 0);
	// Unlocked
}
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Global shader parser helper pointer.
CShaderParserHelper* g_pShaderParserHelper = 0;

//=================================================================================================

int CShader::GetTexId()
{
	CTexture* tp = (CTexture*)GetBaseTexture(NULL, NULL);
	if (!tp)
		return -1;
	return tp->GetTextureID();
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( push )               //AMD Port
	#pragma warning( disable : 4267 )
#endif

int CShader::mfSize()
{
	uint32 i;

	int nSize = sizeof(CShader);
	nSize += m_NameFile.capacity();
	nSize += m_NameShader.capacity();
	nSize += m_HWTechniques.GetMemoryUsage();
	for (i = 0; i < m_HWTechniques.Num(); i++)
		nSize += m_HWTechniques[i]->Size();

	return nSize;
}

void CShader::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddObject(m_NameFile);
	pSizer->AddObject(m_NameShader);
	pSizer->AddObject(m_HWTechniques);
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop )                //AMD Port
#endif

void CShader::mfFree()
{
	uint32 i;

	for (i = 0; i < m_HWTechniques.Num(); i++)
	{
		SShaderTechnique* pTech = m_HWTechniques[i];
		SAFE_DELETE(pTech);
	}
	m_HWTechniques.Free();

	//SAFE_DELETE(m_ShaderGenParams);
	m_Flags &= ~(EF_PARSE_MASK | EF_NODRAW);
	m_nMDV = 0;
}

CShader::~CShader()
{
	CRY_ASSERT(GetRefCounter() == 0);
	gRenDev->m_cEF.m_Bin.mfRemoveFXParams(this);

	if (m_pGenShader && m_pGenShader->m_DerivedShaders)
	{
		uint32 i;
		for (i = 0; i < m_pGenShader->m_DerivedShaders->size(); i++)
		{
			CShader* pSH = (*m_pGenShader->m_DerivedShaders)[i];
			if (pSH == this)
			{
				(*m_pGenShader->m_DerivedShaders)[i] = NULL;
				break;
			}
		}
		assert(i != m_pGenShader->m_DerivedShaders->size());
	}
	mfFree();

	SAFE_RELEASE(m_pGenShader);
	SAFE_DELETE(m_DerivedShaders);
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( push )               //AMD Port
	#pragma warning( disable : 4311 )     // I believe the int cast below is okay.
#endif

CShader& CShader::operator=(const CShader& src)
{
	uint32 i;

	mfFree();

	int Offs = (int)(INT_PTR)&(((CShader*)0)->m_eSHDType);
	byte* d = (byte*)this;
	byte* s = (byte*)&src;
	memcpy(&d[Offs], &s[Offs], sizeof(CShader) - Offs);

	m_NameShader = src.m_NameShader;
	m_NameFile = src.m_NameFile;
	m_NameShaderICRC = src.m_NameShaderICRC;

	if (src.m_HWTechniques.Num())
	{
		m_HWTechniques.Create(src.m_HWTechniques.Num());
		for (i = 0; i < src.m_HWTechniques.Num(); i++)
		{
			m_HWTechniques[i] = new SShaderTechnique(this);
			*m_HWTechniques[i] = *src.m_HWTechniques[i];
			m_HWTechniques[i]->m_shader = this;   // copy operator will override m_shader
		}
	}

	return *this;
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop )                  //AMD Port
#endif

SShaderPass::SShaderPass()
{
	m_RenderState = GS_DEPTHWRITE;

	m_PassFlags = 0;
	m_AlphaRef = ~0;

	m_VShader = NULL;
	m_PShader = NULL;
	m_GShader = NULL;
	m_DShader = NULL;
	m_HShader = NULL;
}

bool SShaderItem::IsMergable(SShaderItem& PrevSI)
{
	if (!PrevSI.m_pShader)
		return true;
	CShaderResources* pRP = (CShaderResources*)PrevSI.m_pShaderResources;
	CShaderResources* pR = (CShaderResources*)m_pShaderResources;
	if (pRP && pR)
	{
		if (pRP->m_AlphaRef != pR->m_AlphaRef)
			return false;
		if (pRP->GetStrengthValue(EFTT_OPACITY) != pR->GetStrengthValue(EFTT_OPACITY))
			return false;
		if (pRP->m_pDeformInfo != pR->m_pDeformInfo)
			return false;
		if (pRP->m_pDetailDecalInfo != pR->m_pDetailDecalInfo)
			return false;
		if ((pRP->m_ResFlags & MTL_FLAG_2SIDED) != (pR->m_ResFlags & MTL_FLAG_2SIDED))
			return false;
		if ((pRP->m_ResFlags & MTL_FLAG_NOSHADOW) != (pR->m_ResFlags & MTL_FLAG_NOSHADOW))
			return false;
		if (m_pShader->GetCull() != PrevSI.m_pShader->GetCull())
			return false;
	}
	return true;
}

void SShaderTechnique::UpdatePreprocessFlags(CShader* pSH)
{
	uint32 i;
	for (i = 0; i < m_Passes.Num(); i++)
	{
		SShaderPass* pPass = &m_Passes[i];
		if (pPass->m_PShader)
			pPass->m_PShader->mfUpdatePreprocessFlags(this);
	}
}

SShaderTechnique* CShader::GetTechnique(int nStartTechnique, int nRequestedTechnique, bool bSilent)
{
	SShaderTechnique* pTech = 0;
	if (m_HWTechniques.Num())
	{
		pTech = m_HWTechniques[0];
		if (nStartTechnique > 0)
		{
			assert(nStartTechnique < (int)m_HWTechniques.Num());
			if (nStartTechnique < (int)m_HWTechniques.Num())
			{
				pTech = m_HWTechniques[nStartTechnique];
			}
			else
			{
				if (!bSilent)
					LogWarning("ERROR: CShader::GetTechnique: Technique %d for shader '%s' is out of range", nStartTechnique, GetName());
				pTech = nullptr;
			}
		}
	}
	else
		pTech = nullptr;

	if (nRequestedTechnique == TTYPE_GENERAL)
	{
		return pTech;
	}

	if (!pTech ||
	    nRequestedTechnique < 0 ||
	    nRequestedTechnique >= TTYPE_MAX ||
	    pTech->m_nTechnique[nRequestedTechnique] < 0 ||
	    pTech->m_nTechnique[nRequestedTechnique] >= (int)m_HWTechniques.Num())
	{
		if (!bSilent)
			LogWarning("ERROR: CShader::GetTechnique: No Technique (%d,%d) for shader '%s' ", nStartTechnique, nRequestedTechnique, GetName());
		return nullptr;
	}

	pTech = m_HWTechniques[pTech->m_nTechnique[nRequestedTechnique]];

	return pTech;
}

#if CRY_PLATFORM_DESKTOP
void CShader::mfFlushCache()
{
	mfFlushPendedShaders();

	if (SEmptyCombination::s_Combinations.size())
	{
		// Flush the cache before storing any empty combinations
		CHWShader::mfFlushPendedShadersWait(-1);
		for (uint32_t m = 0; m < SEmptyCombination::s_Combinations.size(); m++)
		{
			SEmptyCombination& Comb = SEmptyCombination::s_Combinations[m];
			Comb.pShader->mfStoreEmptyCombination(Comb);
		}
		SEmptyCombination::s_Combinations.clear();
	}

	for (auto& pTech : m_HWTechniques)
	{
		for (auto& techPass : pTech->m_Passes)
		{
			CHWShader* shaders[] =
			{
				techPass.m_VShader,
				techPass.m_PShader,
				techPass.m_GShader,
				techPass.m_DShader,
				techPass.m_HShader,
				techPass.m_CShader
			};

			for (const auto& pShader : shaders)
			{
				if (pShader)
					pShader->mfFlushCacheFile();
			}
		}
	}
}
#endif

void CShaderResources::PostLoad(CShader* pSH)
{
	AdjustForSpec();
	if (pSH && (pSH->m_Flags & EF_SKY))
	{
		if (m_Textures[EFTT_DIFFUSE])
		{
			char sky[128];
			char path[1024];
			cry_strcpy(sky, m_Textures[EFTT_DIFFUSE]->m_Name.c_str());
			int size = strlen(sky);
			const char* ext = PathUtil::GetExt(sky);
			while (sky[size] != '_')
			{
				size--;
				if (!size)
					break;
			}
			sky[size] = 0;
			if (size)
			{
				m_pSky = new SSkyInfo;
				cry_sprintf(path, "%s_12.%s", sky, ext);
				m_pSky->m_SkyBox[0] = CTexture::ForName(path, 0, eTF_Unknown);
				cry_sprintf(path, "%s_34.%s", sky, ext);
				m_pSky->m_SkyBox[1] = CTexture::ForName(path, 0, eTF_Unknown);
				cry_sprintf(path, "%s_5.%s", sky, ext);
				m_pSky->m_SkyBox[2] = CTexture::ForName(path, 0, eTF_Unknown);
			}
		}
	}

	UpdateConstants(pSH);
}

SShaderTexSlots* CShader::GetUsedTextureSlots(int nTechnique)
{
	if (!CRenderer::CV_r_ReflectTextureSlots)
		return NULL;
	if (nTechnique < 0 || nTechnique >= TTYPE_MAX)
		return NULL;
	return m_ShaderTexSlots[nTechnique];
}

DynArrayRef<SShaderParam>& CShader::GetPublicParams()
{
	SShaderFXParams& FXP = gRenDev->m_cEF.m_Bin.mfGetFXParams(this);
	return FXP.m_PublicParams;
}

void CShader::CopyPublicParamsTo(SInputShaderResources& copyToResource)
{
	copyToResource.m_ShaderParams = GetPublicParams();
}

CTexture* CShader::mfFindBaseTexture(TArray<SShaderPass>& Passes, int* nPass, int* nTU)
{
	return NULL;
}

ITexture* CShader::GetBaseTexture(int* nPass, int* nTU)
{
	for (uint32 i = 0; i < m_HWTechniques.Num(); i++)
	{
		SShaderTechnique* hw = m_HWTechniques[i];
		CTexture* tx = mfFindBaseTexture(hw->m_Passes, nPass, nTU);
		if (tx)
			return tx;
	}
	if (nPass)
		*nPass = -1;
	if (nTU)
		*nTU = -1;
	return NULL;
}

unsigned int CShader::GetUsedTextureTypes(void)
{
	uint32 nMask = 0xffffffff;

	return nMask;
}

//================================================================================
CShaderMan::CShaderMan()
{
	m_bInitialized = false;
	m_bLoadedSystem = false;
	s_DefaultShader = NULL;
	m_pGlobalExt = NULL;
	g_pShaderParserHelper = &m_shaderParserHelper;
	m_nCombinationsProcess = -1;
	m_nCombinationsProcessOverall = -1;
	m_nCombinationsCompiled = -1;
	m_nCombinationsEmpty = -1;
	m_szShaderPrecache = NULL;

	m_eCacheMode = eSC_Normal;
	m_nFrameSubmit = 1;

	SStaticSinusTable::InitWaveTables();
}

void CShaderMan::mfReleaseShaders()
{
	CCryNameTSCRC Name = CShader::mfGetClassName();

	// TODO: If Release() doesn't manipulate the resource-library, make it a RLock
	CryAutoWriteLock<CryRWLock> lock(CBaseResource::s_cResLock);

	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		int n = 0;
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); )
		{
			CShader* sh = (CShader*)itor->second;
			itor++;
			if (!sh)
				continue;
			if (CRenderer::CV_r_printmemoryleaks && !(sh->m_Flags & EF_SYSTEM))
				iLog->Log("Warning: CShaderMan::mfClearAll: Shader %s was not deleted (%d)", sh->GetName(), sh->GetRefCounter());
			SAFE_RELEASE(sh);
			n++;
		}
	}
}

void CShaderMan::ShutDown(void)
{
	uint32 i;

	m_Bin.InvalidateCache();

	mfReleaseSystemShaders();
	gRenDev->ForceFlushRTCommands();

	if (CRenderer::CV_r_releaseallresourcesonexit)
	{
		//if (gRenDev->m_cEF.m_bInitialized)
		//  mfReleaseShaders();

		for (i = 0; i < CShader::s_ShaderResources_known.Num(); i++)
		{
			CShaderResources* pSR = CShader::s_ShaderResources_known[i];
			if (!pSR)
				continue;
			if (i)
			{
				if (CRenderer::CV_r_printmemoryleaks)
					iLog->Log("Warning: CShaderMan::mfClearAll: Shader resource %p was not deleted", pSR);
			}
			delete pSR;
		}
		CShader::s_ShaderResources_known.Free();
	}

	m_ShaderNames.clear();

	std::for_each(m_pShadersGlobalFlags.begin(), m_pShadersGlobalFlags.end(), SShaderMapNameFlagsContainerDelete());
	m_pShadersGlobalFlags.clear();

	m_pShaderCommonGlobalFlag.clear();
	m_pShadersGlobalFlags.clear();

	CCryNameTSCRC Name;

	SAFE_DELETE(m_pGlobalExt);

	for (i = 0; i < CLightStyle::s_LStyles.Num(); i++)
	{
		delete CLightStyle::s_LStyles[i];
	}
	CLightStyle::s_LStyles.Free();
	m_SGC.clear();
	m_ShaderCacheCombinations[0].clear();
	m_ShaderCacheCombinations[1].clear();
	m_ShaderCacheExportCombinations.clear();
	SAFE_DELETE(m_pGlobalExt);
	mfCloseShadersCache(0);
	mfCloseShadersCache(1);

	if (gEnv && gEnv->pSystem)
	{
		ISystemEventDispatcher* pSystemEventDispatcher = gEnv->pSystem->GetISystemEventDispatcher();
		if (pSystemEventDispatcher)
		{
			pSystemEventDispatcher->RemoveListener(this);
		}
	}

	m_bInitialized = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CShaderMan::mfCreateCommonGlobalFlags(const char* szName)
{
	// Create globals.txt

	assert(szName);
	uint32 nCurrMaskCount = 0;
	const char* pszShaderExtPath = "Shaders/";

	struct _finddata_t fileinfo;
	intptr_t handle;
	char dirn[256];
	cry_strcpy(dirn, pszShaderExtPath);
	cry_strcat(dirn, "*.*");

	handle = gEnv->pCryPak->FindFirst(dirn, &fileinfo);
	if (handle == -1)   // failed search
		return;

	do
	{
		// Scan for extension script files - add common flags names into globals list

		if (fileinfo.name[0] == '.' || fileinfo.attrib & _A_SUBDIR)
			continue;

		const char* pszExt = PathUtil::GetExt(fileinfo.name);
		if (stricmp(pszExt, "ext"))
			continue;

		char pszFileName[256];
		cry_sprintf(pszFileName, "%s%s", pszShaderExtPath, fileinfo.name);

		FILE* fp = gEnv->pCryPak->FOpen(pszFileName, "r");
		if (fp)
		{
			gEnv->pCryPak->FSeek(fp, 0, SEEK_END);
			int len = gEnv->pCryPak->FTell(fp);
			char* buf = new char[len + 1];
			gEnv->pCryPak->FSeek(fp, 0, SEEK_SET);
			len = gEnv->pCryPak->FRead(buf, len, fp);
			gEnv->pCryPak->FClose(fp);
			buf[len] = 0;

			// Check if global flags are common
			char* pCurrOffset = strstr(buf, "UsesCommonGlobalFlags");
			if (pCurrOffset)
			{
				// add shader to list
				string pszShaderName = PathUtil::GetFileName(fileinfo.name);
				pszShaderName.MakeUpper();
				m_pShadersRemapList += string("%") + pszShaderName;

				while (pCurrOffset = strstr(pCurrOffset, "Name"))
				{
					pCurrOffset += 4;
					char dummy[256] = "\n";
					char name[256] = "\n";
					int res = sscanf(pCurrOffset, "%255s %255s", dummy, name);         // Get flag name..
					assert(res);

					string pszNameFlag = name;
					int nSzSize = pszNameFlag.size();
					pszNameFlag.MakeUpper();

					MapNameFlagsItor pCheck = m_pShaderCommonGlobalFlag.find(pszNameFlag);
					if (pCheck == m_pShaderCommonGlobalFlag.end())
					{
						m_pShaderCommonGlobalFlag.insert(MapNameFlagsItor::value_type(pszNameFlag, 0));
						if (++nCurrMaskCount >= 64)   // sanity check
						{
							assert(0);
							break;
						}
					}
				}
			}

			SAFE_DELETE_ARRAY(buf);
		}

		if (nCurrMaskCount >= 64)
			break;

	}
	while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);

	gEnv->pCryPak->FindClose(handle);

	if (nCurrMaskCount >= 64)
		iLog->Log("ERROR: CShaderMan::mfCreateCommonGlobalFlags: too many common global flags");

	uint64 nCurrGenMask = 0;
	MapNameFlagsItor pIter = m_pShaderCommonGlobalFlag.begin();
	MapNameFlagsItor pEnd = m_pShaderCommonGlobalFlag.end();
	for (; pIter != pEnd; ++pIter, ++nCurrGenMask)
	{
		// Set mask value
		pIter->second = ((uint64)1) << nCurrGenMask;
	}

	mfRemapCommonGlobalFlagsWithLegacy();
#if !defined (_RELEASE)
	mfSaveCommonGlobalFlagsToDisk(szName, nCurrMaskCount);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void CShaderMan::mfSaveCommonGlobalFlagsToDisk(const char* szName, uint32 nMaskCount)
{
	// Write all flags
	assert(nMaskCount);

	FILE* fp = gEnv->pCryPak->FOpen(szName, "w");
	if (fp)
	{
		gEnv->pCryPak->FPrintf(fp, "FX_CACHE_VER %f\n", FX_CACHE_VER);
		gEnv->pCryPak->FPrintf(fp, "%s\n\n", m_pShadersRemapList.c_str());

		MapNameFlagsItor pIter = m_pShaderCommonGlobalFlag.begin();
		MapNameFlagsItor pEnd = m_pShaderCommonGlobalFlag.end();

		for (; pIter != pEnd; ++pIter)
		{
			gEnv->pCryPak->FPrintf(fp, "%s "
#if defined(__GNUC__)
			                       "%llx\n"
#else
			                       "%I64x\n"
#endif
			                       , (pIter->first).c_str(), pIter->second);
		}

		gEnv->pCryPak->FClose(fp);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void CShaderMan::mfInitCommonGlobalFlagsLegacyFix(void)
{
	// Store original values since common material flags where introduced
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%ALPHAGLOW", (uint64)0x2));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%ALPHAMASK_DETAILMAP", (uint64)0x4));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%ANISO_SPECULAR", (uint64)0x8));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%BILINEAR_FP16", (uint64)0x10));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%BUMP_DIFFUSE", (uint64)0x20));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%CHARACTER_DECAL", (uint64)0x40));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%CUSTOM_SPECULAR", (uint64)0x400));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%DECAL", (uint64)0x800));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%DETAIL_BENDING", (uint64)0x1000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%DETAIL_BUMP_MAPPING", (uint64)0x2000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%DISABLE_RAIN_PASS", (uint64)0x4000));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%ENVIRONMENT_MAP", (uint64)0x10000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%EYE_OVERLAY", (uint64)0x20000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%GLOSS_DIFFUSEALPHA", (uint64)0x40000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%GLOSS_MAP", (uint64)0x80000));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%GRADIENT_COLORING", (uint64)0x100000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%GRASS", (uint64)0x200000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%IRIS", (uint64)0x400000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%LEAVES", (uint64)0x800000));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%NANOSUIT_EFFECTS", (uint64)0x1000000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%OFFSET_BUMP_MAPPING", (uint64)0x2000000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%PARALLAX_OCCLUSION_MAPPING", (uint64)0x8000000));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%REALTIME_MIRROR_REFLECTION", (uint64)0x10000000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%REFRACTION_MAP", (uint64)0x20000000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%RIM_LIGHTING", (uint64)0x40000000));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%SPECULARPOW_GLOSSALPHA", (uint64)0x80000000));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%BILLBOARD", (uint64)0x100000000ULL));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%TEMP_TERRAIN", (uint64)0x200000000ULL));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%TEMP_VEGETATION", (uint64)0x400000000ULL));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%TERRAINHEIGHTADAPTION", (uint64)0x800000000ULL));

	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%TWO_SIDED_SORTING", (uint64)0x1000000000ULL));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%VERTCOLORS", (uint64)0x2000000000ULL));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%WIND_BENDING", (uint64)0x4000000000ULL));
	m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%WRINKLE_BLENDING", (uint64)0x8000000000ULL));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool CShaderMan::mfRemapCommonGlobalFlagsWithLegacy(void)
{
	MapNameFlagsItor pFixIter = m_pSCGFlagLegacyFix.begin();
	MapNameFlagsItor pFixEnd = m_pSCGFlagLegacyFix.end();

	MapNameFlagsItor pCommonGlobalsEnd = m_pShaderCommonGlobalFlag.end();

	bool bRemaped = false;
	for (; pFixIter != pFixEnd; ++pFixIter)
	{
		MapNameFlagsItor pFoundMatch = m_pShaderCommonGlobalFlag.find(pFixIter->first);
		if (pFoundMatch != pCommonGlobalsEnd)
		{
			// Found a match, replace value
			uint64 nRemapedMask = pFixIter->second;
			uint64 nOldMask = pFoundMatch->second;
			pFoundMatch->second = nRemapedMask;

			// Search for duplicates and swap with old mask
			MapNameFlagsItor pCommonGlobalsIter = m_pShaderCommonGlobalFlag.begin();
			uint64 test = (uint64)0x10;
			for (; pCommonGlobalsIter != pCommonGlobalsEnd; ++pCommonGlobalsIter)
			{
				if (pFoundMatch != pCommonGlobalsIter && pCommonGlobalsIter->second == nRemapedMask)
				{
					uint64 nPrev = pCommonGlobalsIter->second;
					pCommonGlobalsIter->second = nOldMask;
					bRemaped = true;
					break;
				}
			}
		}
	}

	// Create existing flags mask
	MapNameFlagsItor pIter = m_pShaderCommonGlobalFlag.begin();
	MapNameFlagsItor pEnd = m_pShaderCommonGlobalFlag.end();
	m_nSGFlagsFix = 0;
	for (; pIter != pEnd; ++pIter)
	{
		m_nSGFlagsFix |= (pIter->second);
	}

	return bRemaped;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void CShaderMan::mfInitCommonGlobalFlags(void)
{
	mfInitCommonGlobalFlagsLegacyFix();

	string pszGlobalsPath = string(m_szUserPath) + string("Shaders/Cache/globals.txt");
	FILE* fp = gEnv->pCryPak->FOpen(pszGlobalsPath.c_str(), "r", ICryPak::FOPEN_HINT_QUIET);
	if (fp)
	{
		char str[256] = "\n";
		char name[128] = "\n";

		gEnv->pCryPak->FGets(str, 256, fp);
		if (strstr(str, "FX_CACHE_VER"))
		{
			float fCacheVer = 0.0f;
			int res = sscanf(str, "%127s %f", name, &fCacheVer);
			assert(res);
			if (!stricmp(name, "FX_CACHE_VER") && fabs(FX_CACHE_VER - fCacheVer) >= 0.01f)
			{
				// re-create common global flags (shader cache bumped)
				mfCreateCommonGlobalFlags(pszGlobalsPath.c_str());

				gEnv->pCryPak->FClose(fp);
				return;
			}
		}

		// get shader that need remapping slist
		gEnv->pCryPak->FGets(str, 256, fp);
		m_pShadersRemapList = str;

		uint32 nCurrMaskCount = 0;
		while (!gEnv->pCryPak->FEof(fp))
		{
			uint64 nGenMask = 0;
			gEnv->pCryPak->FGets(str, 256, fp);

			if (sscanf(str, "%127s "
#if defined(__GNUC__)
			           "%llx"
#else
			           "%I64x"
#endif
			           , name, &nGenMask) > 1)
			{
				m_pShaderCommonGlobalFlag.insert(MapNameFlagsItor::value_type(name, nGenMask));
				nCurrMaskCount++;
			}
		}

		gEnv->pCryPak->FClose(fp);

		if (mfRemapCommonGlobalFlagsWithLegacy())
			mfSaveCommonGlobalFlagsToDisk(pszGlobalsPath.c_str(), nCurrMaskCount);

		return;
	}

	// create common global flags - not existing globals.txt
	mfCreateCommonGlobalFlags(pszGlobalsPath.c_str());
}

void CShaderMan::mfInitLookups()
{
	m_ResLookupDataMan[static_cast<int>(cacheSource::readonly)].Clear();
	string dirdatafilename("%ENGINE%/" + string(m_ShadersCache));
	dirdatafilename += "lookupdata.bin";
	m_ResLookupDataMan[static_cast<int>(cacheSource::readonly)].LoadData(dirdatafilename.c_str(), CParserBin::m_bEndians, true);

	m_ResLookupDataMan[static_cast<int>(cacheSource::user)].Clear();
	dirdatafilename = m_szUserPath + string(m_ShadersCache);
	dirdatafilename += "lookupdata.bin";
	m_ResLookupDataMan[static_cast<int>(cacheSource::user)].LoadData(dirdatafilename.c_str(), CParserBin::m_bEndians, false);
}

void CShaderMan::mfInitGlobal(void)
{
	SAFE_DELETE(m_pGlobalExt);
	SShaderGen* pShGen = mfCreateShaderGenInfo("RunTime", true);
	m_pGlobalExt = pShGen;
	if (pShGen)
	{
		uint32 i;

		for (i = 0; i < pShGen->m_BitMask.Num(); i++)
		{
			SShaderGenBit* gb = pShGen->m_BitMask[i];
			if (!gb)
				continue;
			if (gb->m_ParamName == "%_RT_FOG")
				g_HWSR_MaskBit[HWSR_FOG] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_ALPHATEST")
				g_HWSR_MaskBit[HWSR_ALPHATEST] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SECONDARY_VIEW")
				g_HWSR_MaskBit[HWSR_SECONDARY_VIEW] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_MSAA_QUALITY")
				g_HWSR_MaskBit[HWSR_MSAA_QUALITY] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_MSAA_QUALITY1")
				g_HWSR_MaskBit[HWSR_MSAA_QUALITY1] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_MSAA_SAMPLEFREQ_PASS")
				g_HWSR_MaskBit[HWSR_MSAA_SAMPLEFREQ_PASS] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_NEAREST")
				g_HWSR_MaskBit[HWSR_NEAREST] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SHADOW_MIXED_MAP_G16R16")
				g_HWSR_MaskBit[HWSR_SHADOW_MIXED_MAP_G16R16] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_HW_PCF_COMPARE")
				g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SAMPLE0")
				g_HWSR_MaskBit[HWSR_SAMPLE0] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SAMPLE1")
				g_HWSR_MaskBit[HWSR_SAMPLE1] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SAMPLE2")
				g_HWSR_MaskBit[HWSR_SAMPLE2] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SAMPLE3")
				g_HWSR_MaskBit[HWSR_SAMPLE3] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_ALPHABLEND")
				g_HWSR_MaskBit[HWSR_ALPHABLEND] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_QUALITY")
				g_HWSR_MaskBit[HWSR_QUALITY] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_QUALITY1")
				g_HWSR_MaskBit[HWSR_QUALITY1] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_NO_TESSELLATION")
				g_HWSR_MaskBit[HWSR_NO_TESSELLATION] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_VERTEX_VELOCITY")
				g_HWSR_MaskBit[HWSR_VERTEX_VELOCITY] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_OBJ_IDENTITY")
				g_HWSR_MaskBit[HWSR_OBJ_IDENTITY] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SKELETON_SSD")
				g_HWSR_MaskBit[HWSR_SKELETON_SSD] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SKELETON_SSD_LINEAR")
				g_HWSR_MaskBit[HWSR_SKELETON_SSD_LINEAR] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_COMPUTE_SKINNING")
				g_HWSR_MaskBit[HWSR_COMPUTE_SKINNING] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_DISSOLVE")
				g_HWSR_MaskBit[HWSR_DISSOLVE] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SOFT_PARTICLE")
				g_HWSR_MaskBit[HWSR_SOFT_PARTICLE] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_LIGHT_TEX_PROJ")
				g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SHADOW_JITTERING")
				g_HWSR_MaskBit[HWSR_SHADOW_JITTERING] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_PARTICLE_SHADOW")
				g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_BLEND_WITH_TERRAIN_COLOR")
				g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SPRITE")
				g_HWSR_MaskBit[HWSR_SPRITE] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_AMBIENT_OCCLUSION")
				g_HWSR_MaskBit[HWSR_AMBIENT_OCCLUSION] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_DEBUG0")
				g_HWSR_MaskBit[HWSR_DEBUG0] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_DEBUG1")
				g_HWSR_MaskBit[HWSR_DEBUG1] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_DEBUG2")
				g_HWSR_MaskBit[HWSR_DEBUG2] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_DEBUG3")
				g_HWSR_MaskBit[HWSR_DEBUG3] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_POINT_LIGHT")
				g_HWSR_MaskBit[HWSR_POINT_LIGHT] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_CUBEMAP0")
				g_HWSR_MaskBit[HWSR_CUBEMAP0] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_DECAL_TEXGEN_2D")
				g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_OCEAN_PARTICLE")
				g_HWSR_MaskBit[HWSR_OCEAN_PARTICLE] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SAMPLE4")
				g_HWSR_MaskBit[HWSR_SAMPLE4] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SAMPLE5")
				g_HWSR_MaskBit[HWSR_SAMPLE5] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_SAMPLE6")
				g_HWSR_MaskBit[HWSR_SAMPLE6] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_ANIM_BLEND")
				g_HWSR_MaskBit[HWSR_ANIM_BLEND] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_MOTION_BLUR")
				g_HWSR_MaskBit[HWSR_MOTION_BLUR] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_ENVIRONMENT_CUBEMAP")
				g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_LIGHTVOLUME0")
				g_HWSR_MaskBit[HWSR_LIGHTVOLUME0] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_LIGHTVOLUME1")
				g_HWSR_MaskBit[HWSR_LIGHTVOLUME1] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_TILED_SHADING")
				g_HWSR_MaskBit[HWSR_TILED_SHADING] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_VOLUMETRIC_FOG")
				g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_REVERSE_DEPTH")
				g_HWSR_MaskBit[HWSR_REVERSE_DEPTH] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_PROJECTION_MULTI_RES")
				g_HWSR_MaskBit[HWSR_PROJECTION_MULTI_RES] = gb->m_Mask;
			else if (gb->m_ParamName == "%_RT_PROJECTION_LENS_MATCHED")
				g_HWSR_MaskBit[HWSR_PROJECTION_LENS_MATCHED] = gb->m_Mask;
			else
				assert(0);
		}
	}
}

void CShaderMan::mfInit(void)
{
	LOADING_TIME_PROFILE_SECTION;
	s_cNameHEAD = CCryNameTSCRC("HEAD");

	CTexture::Init();

	if (!m_bInitialized)
	{
		GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CShaderMan");

		m_ShadersPath = "Shaders/HWScripts/";
		m_ShadersExtPath = "Shaders/";
		m_ShadersGamePath = gEnv->pCryPak->GetGameFolder() + string("/GameShaders/HWScripts/");
		m_ShadersGameExtPath = gEnv->pCryPak->GetGameFolder() + string("/GameShaders/");
		m_ShadersMergeCachePath = "Shaders/MergeCache/";
#if (CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT) || CRY_PLATFORM_ANDROID
		m_ShadersCache = "Shaders/Cache/LINUX32/";
#elif (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT)
		m_ShadersCache = "Shaders/Cache/LINUX64/";
#elif CRY_PLATFORM_MAC
		m_ShadersCache = "Shaders/Cache/Mac/";
#elif CRY_PLATFORM_IOS
		m_ShadersCache = "Shaders/Cache/iOS/";
#else
		m_ShadersCache = "Shaders/Cache/D3D11";
#endif
		m_szUserPath = "%USER%/";
#ifndef CONSOLE_CONST_CVAR_MODE
		if (CRenderer::CV_r_shadersediting)
		{
			CRenderer::CV_r_shadersAllowCompilation = 1; // allow compilation
			CRenderer::CV_r_shaderslogcachemisses = 0;   // don't bother about cache misses
			CParserBin::m_bEditable = true;
			CRenderer::CV_r_shadersImport = 0;           // don't allow shader importing

			ICVar* pPakPriortyCVar = gEnv->pConsole->GetCVar("sys_PakPriority");
			if (pPakPriortyCVar)
				pPakPriortyCVar->Set(0);                   // shaders are loaded from disc, always
			ICVar* pInvalidFileAccessCVar = gEnv->pConsole->GetCVar("sys_PakLogInvalidFileAccess");
			if (pInvalidFileAccessCVar)
				pInvalidFileAccessCVar->Set(0);            // don't bother logging invalid access when editing shaders
		}
#endif
		if (CRenderer::CV_r_shadersAllowCompilation)
		{
			CRenderer::CV_r_shadersasyncactivation = 0;
			CRenderer::CV_r_shadersImport = 0;           // don't allow shader importing
		}

		// make sure correct paks are open - shaders.pak will be unloaded from memory after init
		gEnv->pCryPak->OpenPack(PathUtil::GetGameFolder() + "/", "%ENGINEROOT%/Engine/Shaders.pak",
		                        ICryPak::FLAGS_PAK_IN_MEMORY | ICryPak::FLAGS_PATH_REAL);

		ParseShaderProfiles();

		fxParserInit();
		CParserBin::Init();
		CResFile::Tick();

		//CShader::m_Shaders_known.Alloc(MAX_SHADERS);
		//memset(&CShader::m_Shaders_known[0], 0, sizeof(CShader *)*MAX_SHADERS);

		mfInitGlobal();

		//mfInitLookups();

		// Generate/or load globals.txt - if not existing or shader cache version bumped
		mfInitCommonGlobalFlags();

		mfPreloadShaderExts();

		if (CRenderer::CV_r_shadersAllowCompilation && !gRenDev->IsShaderCacheGenMode())
			mfInitShadersList(&m_ShaderNames);

		mfInitShadersCacheMissLog();

		if (CRenderer::CV_r_shadersAllowCompilation)
		{
			mfInitShadersCache(false, NULL, NULL, 0);
		}

#if CRY_PLATFORM_DESKTOP
		if (!CRenderer::CV_r_shadersAllowCompilation)
		{
			// make sure we can write to the shader cache
			if (!CheckAllFilesAreWritable((string(m_ShadersCache) + "cgpshaders").c_str())
				|| !CheckAllFilesAreWritable((string(m_ShadersCache) + "cgvshaders").c_str()))

			{
				// message box causes problems in fullscreen
				//			MessageBox(0,"WARNING: Shader cache cannot be updated\n\n"
				//				"files are write protected / media is read only / windows user setting don't allow file changes","CryEngine",MB_ICONEXCLAMATION|MB_OK);
				gEnv->pLog->LogError("ERROR: Shader cache cannot be updated (files are write protected / media is read only / windows user setting don't allow file changes)");
			}
		}
#endif  // WIN

		mfSetDefaults();

		//mfPrecacheShaders(NULL);

		// flash all the current commands (parse default shaders)
		gRenDev->m_pRT->FlushAndWait();

		m_bInitialized = true;
	}
	//mfPrecacheShaders();
}

bool CShaderMan::LoadShaderStartupCache()
{
	return true; //gEnv->pCryPak->OpenPack(PathUtil::GetGameFolder() + "/", "Engine/ShaderCacheStartup.pak", ICryPak::FLAGS_PAK_IN_MEMORY | ICryPak::FLAGS_PATH_REAL);
}

void CShaderMan::UnloadShaderStartupCache()
{
	// Called from the MT so need to flush RT
	IF (gRenDev->m_pRT, 1)
		gRenDev->m_pRT->FlushAndWait();

#if defined(SHADERS_SERIALIZING)
	// Free all import data allowing us to close the startup pack
	ClearSResourceCache();
#endif

	//gEnv->pCryPak->ClosePack("Engine/ShaderCacheStartup.pak");
}

void CShaderMan::mfPostInit()
{
	LOADING_TIME_PROFILE_SECTION;

	CTexture::PostInit();

	if (!gRenDev->IsEditorMode() && !gRenDev->IsShaderCacheGenMode())
	{
		mfPreloadBinaryShaders();
	}

	// (enabled also for NULL Renderer, so at least the default shader is initialized)
	if (!gRenDev->IsShaderCacheGenMode())
	{
		mfLoadDefaultSystemShaders();
	}

	gRenDev->m_pRT->FlushAndWait(); // TODO: Remove as soon as DeviceTexture's FlushAndWait is removed
}

void CShaderMan::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			break;
		}
	}
}

void CShaderMan::ParseShaderProfile(char* scr, SShaderProfile* pr)
{
	char* name;
	long cmd;
	char* params;
	char* data;

	enum {eUseNormalAlpha = 1};
	static STokenDesc commands[] =
	{
		{ eUseNormalAlpha, "UseNormalAlpha" },

		{ 0,               0                }
	};

	while ((cmd = shGetObject(&scr, commands, &name, &params)) > 0)
	{
		data = NULL;
		if (name)
			data = name;
		else if (params)
			data = params;

		switch (cmd)
		{
		case eUseNormalAlpha:
			pr->m_nShaderProfileFlags |= SPF_LOADNORMALALPHA;
			break;
		}
	}
}

void CShaderMan::ParseShaderProfiles()
{
	int i;

	for (i = 0; i < eSQ_Max; i++)
	{
		m_ShaderFixedProfiles[i].m_iShaderProfileQuality = i;
		m_ShaderFixedProfiles[i].m_nShaderProfileFlags = 0;
	}

	char* name;
	long cmd;
	char* params;
	char* data;

	enum {eProfile = 1, eVersion};
	static STokenDesc commands[] =
	{
		{ eProfile, "Profile" },
		{ eVersion, "Version" },

		{ 0,        0         }
	};

	char* scr = NULL;
	FILE* fp = gEnv->pCryPak->FOpen("Shaders/ShaderProfiles.txt", "rb");
	if (fp)
	{
		gEnv->pCryPak->FSeek(fp, 0, SEEK_END);
		int ln = gEnv->pCryPak->FTell(fp);
		scr = new char[ln + 1];
		if (scr)
		{
			scr[ln] = 0;
			gEnv->pCryPak->FSeek(fp, 0, SEEK_SET);
			gEnv->pCryPak->FRead(scr, ln, fp);
			gEnv->pCryPak->FClose(fp);
		}
	}
	char* pScr = scr;
	if (scr)
	{
		while ((cmd = shGetObject(&scr, commands, &name, &params)) > 0)
		{
			data = NULL;
			if (name)
				data = name;
			else if (params)
				data = params;

			switch (cmd)
			{
			case eProfile:
				{
					SShaderProfile* pr = NULL;
					assert(name);
					PREFAST_ASSUME(name);
					if (!stricmp(name, "Low"))
						pr = &m_ShaderFixedProfiles[eSQ_Low];
					else
					{
						pr = &m_ShaderFixedProfiles[eSQ_High];
					}
					if (pr)
						ParseShaderProfile(params, pr);
					break;
				}
			case eVersion:
				break;
			}
		}
	}
	SAFE_DELETE_ARRAY(pScr);
}

const SShaderProfile& CRenderer::GetShaderProfile(EShaderType eST) const
{
	assert((int)eST < sizeof(m_cEF.m_ShaderProfiles) / sizeof(SShaderProfile));

	return m_cEF.m_ShaderProfiles[eST];
}

void CShaderMan::RT_SetShaderQuality(EShaderType eST, EShaderQuality eSQ)
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CShaderMan::RT_SetShaderQuality");

	eSQ = CLAMP(eSQ, eSQ_Low, eSQ_VeryHigh);
	if (eST == eST_All)
	{
		for (int i = 0; i < eST_Max; i++)
		{
			m_ShaderProfiles[i] = m_ShaderFixedProfiles[eSQ];
			m_ShaderProfiles[i].m_iShaderProfileQuality = eSQ;        // good?
		}
	}
	else
	{
		m_ShaderProfiles[eST] = m_ShaderFixedProfiles[eSQ];
		m_ShaderProfiles[eST].m_iShaderProfileQuality = eSQ;        // good?
	}
	if (eST == eST_All || eST == eST_General)
	{
		bool bPS20 = ((gRenDev->m_Features & (RFT_HW_SM2X | RFT_HW_SM30)) == 0) || (eSQ == eSQ_Low);
		m_Bin.InvalidateCache();
		mfReloadAllShaders(FRO_FORCERELOAD, 0, gRenDev->GetRenderFrameID());
	}
}

static byte bFirst = 1;

static bool sLoadShader(const char* szName, CShader*& pStorage)
{
	if (pStorage) // Do not try to overwrite existing shader
		return false;

	CShader* ef = NULL;
	bool bRes = true;
	if (bFirst)
		CryComment("Load System Shader '%s'...", szName);
	ef = gRenDev->m_cEF.mfForName(szName, EF_SYSTEM);
	if (bFirst)
	{
		if (!ef || (ef->m_Flags & EF_NOTFOUND))
		{
			Warning("Load System Shader Failed %s", szName);
			bRes = false;
		}
		else
			CryComment("ok");
	}
	pStorage = ef;
	return bRes;
}

void CShaderMan::mfReleaseSystemShaders()
{
	SAFE_RELEASE_FORCE(s_DefaultShader);
	SAFE_RELEASE_FORCE(s_shPostEffects);
	SAFE_RELEASE_FORCE(s_shPostDepthOfField);
	SAFE_RELEASE_FORCE(s_shPostMotionBlur);
	SAFE_RELEASE_FORCE(s_shPostSunShafts);
	SAFE_RELEASE_FORCE(s_sh3DHUD);
	SAFE_RELEASE_FORCE(s_shDeferredShading);
	SAFE_RELEASE_FORCE(s_ShaderDeferredCaustics);
	SAFE_RELEASE_FORCE(s_ShaderDeferredRain);
	SAFE_RELEASE_FORCE(s_ShaderDeferredSnow);
	SAFE_RELEASE_FORCE(s_ShaderFPEmu);
	SAFE_RELEASE_FORCE(s_ShaderFallback);
	SAFE_RELEASE_FORCE(s_ShaderScaleForm);
	SAFE_RELEASE_FORCE(s_ShaderStars);
	SAFE_RELEASE_FORCE(s_ShaderShadowBlur);
	SAFE_RELEASE_FORCE(s_ShaderShadowMaskGen);
#if defined(FEATURE_SVO_GI)
	SAFE_RELEASE_FORCE(s_ShaderSVOGI);
#endif
	SAFE_RELEASE_FORCE(s_shHDRPostProcess);
	SAFE_RELEASE_FORCE(s_ShaderDebug);
	SAFE_RELEASE_FORCE(s_ShaderLensOptics);
	SAFE_RELEASE_FORCE(s_ShaderSoftOcclusionQuery);
	SAFE_RELEASE_FORCE(s_ShaderLightStyles);
	SAFE_RELEASE_FORCE(s_shPostEffectsGame);
	SAFE_RELEASE_FORCE(s_shPostEffectsRenderModes);
	SAFE_RELEASE_FORCE(s_shPostAA);
	SAFE_RELEASE_FORCE(s_ShaderCommon);
	SAFE_RELEASE_FORCE(s_ShaderOcclTest);
	SAFE_RELEASE_FORCE(s_ShaderDXTCompress);
	SAFE_RELEASE_FORCE(s_ShaderStereo);
	SAFE_RELEASE_FORCE(s_ShaderClouds);
	SAFE_RELEASE_FORCE(s_ShaderGpuParticles);
	SAFE_RELEASE_FORCE(s_ShaderMobileComposition);
	m_bLoadedSystem = false;

}

void CShaderMan::mfLoadBasicSystemShaders()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!s_DefaultShader)
	{
		s_DefaultShader = mfNewShader("<Default>");
		s_DefaultShader->m_NameShader = "<Default>";
		s_DefaultShader->m_Flags |= EF_SYSTEM;
	}

	if (!m_bLoadedSystem)
	{
		sLoadShader("Fallback", s_ShaderFallback);
		sLoadShader("FixedPipelineEmu", s_ShaderFPEmu);
		sLoadShader("Scaleform", s_ShaderScaleForm);

		mfRefreshSystemShader("Stereo", CShaderMan::s_ShaderStereo);
	}

}

void CShaderMan::mfLoadDefaultSystemShaders()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!s_DefaultShader)
	{
		s_DefaultShader = mfNewShader("<Default>");
		s_DefaultShader->m_NameShader = "<Default>";
		s_DefaultShader->m_Flags |= EF_SYSTEM;
	}

	if (!m_bLoadedSystem)
	{
		m_bLoadedSystem = true;

		sLoadShader("Fallback", s_ShaderFallback);
		sLoadShader("FixedPipelineEmu", s_ShaderFPEmu);
		sLoadShader("Scaleform", s_ShaderScaleForm);
		sLoadShader("Light", s_ShaderLightStyles);

		sLoadShader("ShadowMaskGen", s_ShaderShadowMaskGen);
		sLoadShader("HDRPostProcess", s_shHDRPostProcess);
		sLoadShader("Hud3D", s_sh3DHUD);

		sLoadShader("PostEffects", s_shPostEffects);

#if defined(FEATURE_SVO_GI)
		mfRefreshSystemShader("Total_Illumination", CShaderMan::s_ShaderSVOGI);
#endif
		mfRefreshSystemShader("Common", CShaderMan::s_ShaderCommon);
		mfRefreshSystemShader("Debug", CShaderMan::s_ShaderDebug);
		mfRefreshSystemShader("DeferredCaustics", CShaderMan::s_ShaderDeferredCaustics);
		mfRefreshSystemShader("DeferredRain", CShaderMan::s_ShaderDeferredRain);
		mfRefreshSystemShader("DeferredSnow", CShaderMan::s_ShaderDeferredSnow);
		mfRefreshSystemShader("DeferredShading", CShaderMan::s_shDeferredShading);
		mfRefreshSystemShader("DepthOfField", CShaderMan::s_shPostDepthOfField);
		mfRefreshSystemShader("DXTCompress", CShaderMan::s_ShaderDXTCompress);
		mfRefreshSystemShader("LensOptics", CShaderMan::s_ShaderLensOptics);
		mfRefreshSystemShader("SoftOcclusionQuery", CShaderMan::s_ShaderSoftOcclusionQuery);
		mfRefreshSystemShader("MotionBlur", CShaderMan::s_shPostMotionBlur);
		mfRefreshSystemShader("OcclusionTest", CShaderMan::s_ShaderOcclTest);
		mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);
		mfRefreshSystemShader("PostEffectsRenderModes", CShaderMan::s_shPostEffectsRenderModes);
		mfRefreshSystemShader("PostAA", CShaderMan::s_shPostAA);
		mfRefreshSystemShader("ShadowBlur", CShaderMan::s_ShaderShadowBlur);
		mfRefreshSystemShader("Sunshafts", CShaderMan::s_shPostSunShafts);
		mfRefreshSystemShader("Clouds", CShaderMan::s_ShaderClouds);
		mfRefreshSystemShader("GpuParticles", CShaderMan::s_ShaderGpuParticles);
	}

}

void CShaderMan::mfSetDefaults(void)
{

	mfReleaseSystemShaders();
	mfLoadBasicSystemShaders();

	if (gRenDev->IsEditorMode())
		gRenDev->RefreshSystemShaders();

	bFirst = 0;

	m_bInitialized = true;
}

bool CShaderMan::mfGatherShadersList(const char* szPath, bool bCheckIncludes, bool bUpdateCRC, std::vector<string>* Names)
{
	struct _finddata_t fileinfo;
	intptr_t handle;
	char nmf[256];
	char dirn[256];

	cry_strcpy(dirn, szPath);
	cry_strcat(dirn, "*.*");

	bool bChanged = false;

	handle = gEnv->pCryPak->FindFirst(dirn, &fileinfo);
	if (handle == -1)
		return bChanged;
	do
	{
		if (fileinfo.name[0] == '.')
			continue;
		if (fileinfo.attrib & _A_SUBDIR)
		{
			char ddd[256];
			cry_sprintf(ddd, "%s%s/", szPath, fileinfo.name);

			bChanged = mfGatherShadersList(ddd, bCheckIncludes, bUpdateCRC, Names);
			if (bChanged)
				break;
			continue;
		}
		cry_strcpy(nmf, szPath);
		cry_strcat(nmf, fileinfo.name);
		int len = strlen(nmf) - 1;
		while (len >= 0 && nmf[len] != '.')
			len--;
		if (len <= 0)
			continue;
		if (bCheckIncludes)
		{
			if (!stricmp(&nmf[len], ".cfi"))
			{
				cry_strcpy(nmf, fileinfo.name);
				PathUtil::RemoveExtension(nmf);
				SShaderBin* pBin = m_Bin.GetBinShader(nmf, true, 0, &bChanged);

				// If any include file was not found in the read only cache, we'll need to update the CRCs
				if (pBin && pBin->IsReadOnly() == false)
				{
					bChanged = true;
				}

				if (bChanged)
					break;
			}
			continue;
		}
		if (stricmp(&nmf[len], ".cfx"))
			continue;
		cry_strcpy(nmf, fileinfo.name);
		PathUtil::RemoveExtension(nmf);
		mfAddFXShaderNames(nmf, Names, bUpdateCRC);
	}
	while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);

	gEnv->pCryPak->FindClose(handle);

	return bChanged;
}

void CShaderMan::mfGatherFilesList(const char* szPath, std::vector<CCryNameR>& Names, int nLevel, bool bUseFilter, bool bMaterial)
{
	struct _finddata_t fileinfo;
	intptr_t handle;
	char nmf[256];
	char dirn[256];

	cry_strcpy(dirn, szPath);
	cry_strcat(dirn, "*.*");

	handle = gEnv->pCryPak->FindFirst(dirn, &fileinfo);
	if (handle == -1)
		return;
	do
	{
		if (fileinfo.name[0] == '.')
			continue;
		if (fileinfo.attrib & _A_SUBDIR)
		{
			if (!bUseFilter || nLevel != 1 || !m_ShadersFilter && !stricmp(fileinfo.name, m_ShadersFilter))
			{
				char ddd[256];
				cry_sprintf(ddd, "%s%s/", szPath, fileinfo.name);

				mfGatherFilesList(ddd, Names, nLevel + 1, bUseFilter, bMaterial);
			}
			continue;
		}
		cry_strcpy(nmf, szPath);
		cry_strcat(nmf, fileinfo.name);
		int len = strlen(nmf) - 1;
		while (len >= 0 && nmf[len] != '.')
			len--;
		if (len <= 0)
			continue;
		if (!bMaterial && stricmp(&nmf[len], ".fxcb"))
			continue;
		if (bMaterial && stricmp(&nmf[len], ".mtl"))
			continue;
		Names.push_back(CCryNameR(nmf));
	}
	while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);

	gEnv->pCryPak->FindClose(handle);
}

int CShaderMan::mfInitShadersList(std::vector<string>* Names)
{
	// Detect include changes
	bool bChanged = mfGatherShadersList(m_ShadersGamePath, true, false, Names);
	bChanged |= mfGatherShadersList(m_ShadersPath, true, false, Names);

	if (gRenDev->m_bShaderCacheGen)
	{
		// flush out EXT files, so we reload them again after proper per-platform setup
		for (ShaderExtItor it = gRenDev->m_cEF.m_ShaderExts.begin(); it != gRenDev->m_cEF.m_ShaderExts.end(); ++it)
		{
			delete it->second;
		}
		gRenDev->m_cEF.m_ShaderExts.clear();
		gRenDev->m_cEF.m_SGC.clear();

		m_ShaderCacheCombinations[0].clear();
		m_ShaderCacheCombinations[1].clear();
		m_ShaderCacheExportCombinations.clear();

		mfInitShadersCache(false, NULL, NULL, 0);

		if (CParserBin::m_nPlatform == SF_ORBIS)
		{
			FilterShaderCacheGenListForOrbis(m_ShaderCacheCombinations[0]);
		}
	}

	mfGatherShadersList(m_ShadersGamePath, false, bChanged, Names);
	mfGatherShadersList(m_ShadersPath, false, bChanged, Names);
	return m_ShaderNames.size();
}

void CShaderMan::mfPreloadShaderExts(void)
{
	struct _finddata_t fileinfo;
	intptr_t handle;

	handle = gEnv->pCryPak->FindFirst("Shaders/*.ext", &fileinfo);
	if (handle == -1)
		return;
	do
	{
		if (fileinfo.name[0] == '.')
			continue;
		if (fileinfo.attrib & _A_SUBDIR)
			continue;
		// Ignore runtime.ext
		if (!stricmp(fileinfo.name, "runtime.ext"))
			continue;
		char s[256];
		cry_strcpy(s, fileinfo.name);
		PathUtil::RemoveExtension(s);
		SShaderGen* pShGen = mfCreateShaderGenInfo(s, false);
		assert(pShGen);
	}
	while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);

	gEnv->pCryPak->FindClose(handle);

}

//===================================================================

CShader* CShaderMan::mfNewShader(const char* szName)
{
	CShader* pSH;

	CCryNameTSCRC className = CShader::mfGetClassName();
	CCryNameTSCRC Name = szName;
	CBaseResource* pBR = CBaseResource::GetResource(className, Name, false);
	if (!pBR)
	{
		pSH = new CShader;
		pSH->Register(className, Name);
	}
	else
	{
		pSH = (CShader*)pBR;
		pSH->AddRef();
	}
	if (!s_pContainer)
		s_pContainer = CBaseResource::GetResourcesForClass(className);

	if (pSH->GetID() >= MAX_REND_SHADERS)
	{
		SAFE_RELEASE(pSH);
		iLog->Log("ERROR: MAX_REND_SHADERS hit\n");
		return NULL;
	}

	return pSH;
}

SEnvTexture* SHRenderTarget::GetEnv2D()
{
	CRenderer* rd = gRenDev;
	SEnvTexture* pEnvTex = NULL;
	if (m_nIDInPool >= 0)
	{
		assert(m_nIDInPool < (int)CRendererResources::s_CustomRT_2D.Num());
		if (m_nIDInPool < (int)CRendererResources::s_CustomRT_2D.Num())
			pEnvTex = &CRendererResources::s_CustomRT_2D[m_nIDInPool];
	}
	else
	{
		CRenderView* pRenderView = gcpRendD3D.GetGraphicsPipeline().GetCurrentRenderView();
		const CCamera& cam = (pRenderView) ? pRenderView->GetCamera(CCamera::eEye_Left) : GetISystem()->GetViewCamera();
		Matrix33 orientation = Matrix33(cam.GetMatrix());
		Ang3 Angs = CCamera::CreateAnglesYPR(orientation);
		Vec3 Pos = cam.GetPosition();
		bool bReflect = false;
		if (m_nFlags & (FRT_CAMERA_REFLECTED_PLANE | FRT_CAMERA_REFLECTED_WATERPLANE))
			bReflect = true;
		pEnvTex = CRendererResources::FindSuitableEnvTex(Pos, Angs, true, 0, false, nullptr, nullptr, nullptr, bReflect, nullptr, NULL, nullptr);
	}
	return pEnvTex;
}

void SHRenderTarget::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddObject(m_TargetName);
	pSizer->AddObject(m_pTarget);
}

void CShaderResources::CreateModifiers(SInputShaderResources* pInRes)
{
	uint32 i;
	m_nLastTexture = 0;
	for (i = 0; i < EFTT_MAX; i++)
	{
		if (!m_Textures[i])
			continue;

		SEfResTexture* pDst = m_Textures[i];
		pDst->m_Ext.m_nUpdateFlags = 0;

		IF (pDst->m_Sampler.m_pDynTexSource, 0)
		{
			if (!pDst->m_Ext.m_pTexModifier)
			{
				SEfTexModificator* pDstMod = new SEfTexModificator;
				pDst->m_Ext.m_pTexModifier = pDstMod;
			}
			pDst->m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_MATRIX;
			continue;
		}

		SEfResTexture& InTex = pInRes->m_Textures[i];
		if (!InTex.m_Ext.m_pTexModifier)
			continue;

		InTex.m_Ext.m_nUpdateFlags = 0;
		InTex.m_Ext.m_nLastRecursionLevel = -1;
		m_nLastTexture = i;
		SEfResTexture* pTex = m_Textures[i];
		SEfTexModificator* pMod = InTex.m_Ext.m_pTexModifier;
		if (i != EFTT_DETAIL_OVERLAY)
		{
			if (pMod->m_eMoveType[0] >= ETMM_Max)
				pMod->m_eMoveType[0] = ETMM_NoChange;
			if (pMod->m_eMoveType[1] >= ETMM_Max)
				pMod->m_eMoveType[1] = ETMM_NoChange;
			if (pMod->m_eTGType >= ETG_Max)
				pMod->m_eTGType = ETG_Stream;
			if (pMod->m_eRotType >= ETMR_Max)
				pMod->m_eRotType = ETMR_NoChange;
			if (pMod->m_eTGType != ETG_Stream)
			{
				m_ResFlags |= MTL_FLAG_NOTINSTANCED;
			}
		}

		InTex.UpdateForCreate();
		if (InTex.m_Ext.m_nUpdateFlags & HWMD_TEXCOORD_FLAG_MASK)
		{
			SEfTexModificator* pDstMod = new SEfTexModificator;
			*pDstMod = *pMod;
			SAFE_DELETE(pDst->m_Ext.m_pTexModifier);
			pDst->m_Ext.m_pTexModifier = pDstMod;
		}
		else
		{
			if (pDst->m_Sampler.m_eTexType == eTT_Auto2D)
			{
				if (!pDst->m_Ext.m_pTexModifier)
				{
					m_ResFlags |= MTL_FLAG_NOTINSTANCED;
					SEfTexModificator* pDstMod = new SEfTexModificator;
					*pDstMod = *pMod;
					pDst->m_Ext.m_pTexModifier = pDstMod;
				}
			}

			if (pMod->m_bTexGenProjected)
				pDst->m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_PROJ;
		}
	}
}

void SEfResTexture::UpdateForCreate()
{
	FUNCTION_PROFILER_RENDER_FLAT

	SEfTexModificator* pMod = m_Ext.m_pTexModifier;
	if (!pMod)
		return;

	m_Ext.m_nUpdateFlags = 0;

	ETEX_Type eTT = (ETEX_Type)m_Sampler.m_eTexType;
	if (eTT == eTT_Auto2D && m_Sampler.m_pTarget && !m_Sampler.m_pDynTexSource)
	{
		SEnvTexture* pEnv = m_Sampler.m_pTarget->GetEnv2D();
		assert(pEnv);
		if (pEnv && pEnv->m_pTex)
			m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_PROJ | HWMD_TEXCOORD_GEN_OBJECT_LINEAR;
	}

	bool bTr = false;

	if (pMod->m_Tiling[0] == 0)
		pMod->m_Tiling[0] = 1.0f;
	if (pMod->m_Tiling[1] == 0)
		pMod->m_Tiling[1] = 1.0f;

	bTr = (pMod->m_eMoveType[0] != ETMM_NoChange ||
	       pMod->m_eMoveType[1] != ETMM_NoChange ||
	       pMod->m_eRotType != ETMR_NoChange ||
	       pMod->m_Offs[0] != 0.0f ||
	       pMod->m_Offs[1] != 0.0f ||
	       pMod->m_Tiling[0] != 1.0f ||
	       pMod->m_Tiling[1] != 1.0f);

	if (pMod->m_eTGType != ETG_Stream)
	{
		switch (pMod->m_eTGType)
		{
		case ETG_World:
			m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_GEN_OBJECT_LINEAR;
			break;
		case ETG_Camera:
			m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_GEN_OBJECT_LINEAR;
			break;
		}
	}

	if (bTr)
		m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_MATRIX;

	if (pMod->m_bTexGenProjected)
		m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_PROJ;
}

// Update TexGen and TexTransform matrices for current material texture
void SEfResTexture::Update(int nTSlot, uint32& nMDMask)
{
	FUNCTION_PROFILER_RENDER_FLAT
	  PrefetchLine(m_Sampler.m_pTex, 0);
	CRenderer* rd = gRenDev;

	assert(nTSlot < MAX_TMU);


	const SEfTexModificator* const pMod = m_Ext.m_pTexModifier;

	IF (!pMod, 1)
	{
		if (nTSlot == 0)
			nMDMask |= m_Ext.m_nUpdateFlags;
	}
	else
	{
		UpdateWithModifier(nTSlot, nMDMask);
	}
}

void SEfResTexture::UpdateWithModifier(int nTSlot, uint32& nMDMask)
{
	CRenderer* rd = gRenDev;
	int nFrameID = gRenDev->GetRenderFrameID();
	int recursion = 0;
	if (m_Ext.m_nFrameUpdated == nFrameID && m_Ext.m_nLastRecursionLevel == recursion)
	{
		if (nTSlot == 0)
			nMDMask |= m_Ext.m_nUpdateFlags;

		return;
	}

	m_Ext.m_nFrameUpdated = nFrameID;
	m_Ext.m_nLastRecursionLevel = recursion;
	m_Ext.m_nUpdateFlags = 0;

	ETEX_Type eTT = (ETEX_Type)m_Sampler.m_eTexType;
	SEfTexModificator* pMod = m_Ext.m_pTexModifier;
	if (eTT == eTT_Auto2D && m_Sampler.m_pTarget && !m_Sampler.m_pDynTexSource)
	{
		SEnvTexture* pEnv = m_Sampler.m_pTarget->GetEnv2D();
		assert(pEnv);
		if (pEnv && pEnv->m_pTex)
		{
			//pMod->m_TexGenMatrix = Matrix44A(gRenDev->m_RP.m_pCurObject->m_II.m_Matrix).GetTransposed() * pEnv->m_Matrix;
			//pMod->m_TexGenMatrix = pMod->m_TexGenMatrix.GetTransposed();
			m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_PROJ | HWMD_TEXCOORD_GEN_OBJECT_LINEAR;
		}
	}

	bool bTr = false;
	bool bTranspose = false;
	Plane Pl;
	Plane PlTr;

	const float fTiling0 = pMod->m_Tiling[0];
	const float fTiling1 = pMod->m_Tiling[1];
	pMod->m_Tiling[0] = (float)__fsel(-fabsf(fTiling0), 1.0f, fTiling0);
	pMod->m_Tiling[1] = (float)__fsel(-fabsf(fTiling1), 1.0f, fTiling1);

	IDynTextureSourceImpl* pDynTexSrcImpl = (IDynTextureSourceImpl*) m_Sampler.m_pDynTexSource;
	IF (pDynTexSrcImpl, 0)
	{
		Matrix44& m = pMod->m_TexMatrix;
		m.SetIdentity();
		pDynTexSrcImpl->GetTexGenInfo(m.m30, m.m31, m.m00, m.m11);
		m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_MATRIX;
	}

	float currentTime = gRenDev->GetAnimationTime().GetSeconds();

	if (pMod->m_eMoveType[0] != ETMM_NoChange ||
	    pMod->m_eMoveType[1] != ETMM_NoChange ||
	    pMod->m_eRotType != ETMR_NoChange ||
	    pMod->m_Offs[0] != 0.0f ||
	    pMod->m_Offs[1] != 0.0f ||
	    pMod->m_Tiling[0] != 1.0f ||
	    pMod->m_Tiling[1] != 1.0f)
	{
		pMod->m_TexMatrix.SetIdentity();
		float fTime = currentTime;

		bTr = true;

		switch (pMod->m_eRotType)
		{
		case ETMR_Fixed:
			{
				//translation matrix for rotation center
				pMod->m_TexMatrix = pMod->m_TexMatrix *
				                    Matrix44(1, 0, 0, 0,
				                             0, 1, 0, 0,
				                             0, 0, 1, 0,
				                             -pMod->m_RotOscCenter[0], -pMod->m_RotOscCenter[1], -pMod->m_RotOscCenter[2], 1);

				if (pMod->m_RotOscAmplitude[0]) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationX(Word2Degr(pMod->m_RotOscAmplitude[0]) * PI / 180.0f);
				if (pMod->m_RotOscAmplitude[1]) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationY(Word2Degr(pMod->m_RotOscAmplitude[1]) * PI / 180.0f);
				if (pMod->m_RotOscAmplitude[2]) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationZ(Word2Degr(pMod->m_RotOscAmplitude[2]) * PI / 180.0f);

				//translation matrix for rotation center
				pMod->m_TexMatrix = pMod->m_TexMatrix *
				                    Matrix44(1, 0, 0, 0,
				                             0, 1, 0, 0,
				                             0, 0, 1, 0,
				                             pMod->m_RotOscCenter[0], pMod->m_RotOscCenter[1], pMod->m_RotOscCenter[2], 1);
			}
			break;

		case ETMR_Constant:
			{
				fTime *= 1000.0f;
				float fxAmp = Word2Degr(pMod->m_RotOscAmplitude[0]) * fTime * PI / 180.0f + Word2Degr(pMod->m_RotOscPhase[0]);
				float fyAmp = Word2Degr(pMod->m_RotOscAmplitude[1]) * fTime * PI / 180.0f + Word2Degr(pMod->m_RotOscPhase[1]);
				float fzAmp = Word2Degr(pMod->m_RotOscAmplitude[2]) * fTime * PI / 180.0f + Word2Degr(pMod->m_RotOscPhase[2]);

				//translation matrix for rotation center
				pMod->m_TexMatrix = pMod->m_TexMatrix *
				                    Matrix44(1, 0, 0, 0,
				                             0, 1, 0, 0,
				                             0, 0, 1, 0,
				                             -pMod->m_RotOscCenter[0], -pMod->m_RotOscCenter[1], -pMod->m_RotOscCenter[2], 1);

				//rotation matrix
				if (fxAmp) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix44A(Matrix33::CreateRotationX(fxAmp)).GetTransposed();
				if (fyAmp) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix44A(Matrix33::CreateRotationY(fyAmp)).GetTransposed();
				if (fzAmp) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix44A(Matrix33::CreateRotationZ(fzAmp)).GetTransposed();

				//translation matrix for rotation center
				pMod->m_TexMatrix = pMod->m_TexMatrix *
				                    Matrix44(1, 0, 0, 0,
				                             0, 1, 0, 0,
				                             0, 0, 1, 0,
				                             pMod->m_RotOscCenter[0], pMod->m_RotOscCenter[1], pMod->m_RotOscCenter[2], 1);

			}
			break;

		case ETMR_Oscillated:
			{
				//translation matrix for rotation center
				pMod->m_TexMatrix = pMod->m_TexMatrix *
				                    Matrix44(1, 0, 0, 0,
				                             0, 1, 0, 0,
				                             0, 0, 1, 0,
				                             -pMod->m_RotOscCenter[0], -pMod->m_RotOscCenter[1], -pMod->m_RotOscCenter[2], 1);

				float S_X = fTime * Word2Degr(pMod->m_RotOscRate[0]);
				float S_Y = fTime * Word2Degr(pMod->m_RotOscRate[1]);
				float S_Z = fTime * Word2Degr(pMod->m_RotOscRate[2]);

				float d_X = Word2Degr(pMod->m_RotOscAmplitude[0]) * sin_tpl(2.0f * PI * ((S_X - floor_tpl(S_X)) + Word2Degr(pMod->m_RotOscPhase[0])));
				float d_Y = Word2Degr(pMod->m_RotOscAmplitude[1]) * sin_tpl(2.0f * PI * ((S_Y - floor_tpl(S_Y)) + Word2Degr(pMod->m_RotOscPhase[1])));
				float d_Z = Word2Degr(pMod->m_RotOscAmplitude[2]) * sin_tpl(2.0f * PI * ((S_Z - floor_tpl(S_Z)) + Word2Degr(pMod->m_RotOscPhase[2])));

				if (d_X) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationX(d_X);
				if (d_Y) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationY(d_Y);
				if (d_Z) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationZ(d_Z);

				//translation matrix for rotation center
				pMod->m_TexMatrix = pMod->m_TexMatrix *
				                    Matrix44(1, 0, 0, 0,
				                             0, 1, 0, 0,
				                             0, 0, 1, 0,
				                             pMod->m_RotOscCenter[0], pMod->m_RotOscCenter[1], pMod->m_RotOscCenter[2], 1);
			}
			break;
		}

		float Su = currentTime * pMod->m_OscRate[0];
		float Sv = currentTime * pMod->m_OscRate[1];
		switch (pMod->m_eMoveType[0])
		{
		case ETMM_Pan:
			{
				float du = pMod->m_OscAmplitude[0] * sin_tpl(2.0f * PI * (Su - floor_tpl(Su)) + 2.f * PI * pMod->m_OscPhase[0]);
				pMod->m_TexMatrix(3, 0) = du;
			}
			break;
		case ETMM_Fixed:
			{
				float du = pMod->m_OscRate[0];
				pMod->m_TexMatrix(3, 0) = du;
			}
			break;
		case ETMM_Constant:
			{
				float du = pMod->m_OscAmplitude[0] * Su;       //(Su - floor_tpl(Su));
				pMod->m_TexMatrix(3, 0) = du;
			}
			break;
		case ETMM_Jitter:
			{
				if (pMod->m_LastTime[0] < 1.0f || pMod->m_LastTime[0] > Su + 1.0f)
				{
					pMod->m_LastTime[0] = pMod->m_OscPhase[0] + floor_tpl(Su);
				}
				if (Su - pMod->m_LastTime[0] > 1.0f)
				{
					pMod->m_CurrentJitter[0] = cry_random(0.0f, pMod->m_OscAmplitude[0]);
					pMod->m_LastTime[0] = pMod->m_OscPhase[0] + floor_tpl(Su);
				}
				pMod->m_TexMatrix(3, 0) = pMod->m_CurrentJitter[0];
			}
			break;
		case ETMM_Stretch:
			{
				float du = pMod->m_OscAmplitude[0] * sin_tpl(2.0f * PI * (Su - floor_tpl(Su)) + 2.0f * PI * pMod->m_OscPhase[0]);
				pMod->m_TexMatrix(0, 0) = 1.0f + du;
			}
			break;
		case ETMM_StretchRepeat:
			{
				float du = pMod->m_OscAmplitude[0] * sin_tpl(0.5f * PI * (Su - floor_tpl(Su)) + 2.0f * PI * pMod->m_OscPhase[0]);
				pMod->m_TexMatrix(0, 0) = 1.0f + du;
			}
			break;
		}

		switch (pMod->m_eMoveType[1])
		{
		case ETMM_Pan:
			{
				float dv = pMod->m_OscAmplitude[1] * sin_tpl(2.0f * PI * (Sv - floor_tpl(Sv)) + 2.0f * PI * pMod->m_OscPhase[1]);
				pMod->m_TexMatrix(3, 1) = dv;
			}
			break;
		case ETMM_Fixed:
			{
				float dv = pMod->m_OscRate[1];
				pMod->m_TexMatrix(3, 1) = dv;
			}
			break;
		case ETMM_Constant:
			{
				float dv = pMod->m_OscAmplitude[1] * Sv;       //(Sv - floor_tpl(Sv));
				pMod->m_TexMatrix(3, 1) = dv;
			}
			break;
		case ETMM_Jitter:
			{
				if (pMod->m_LastTime[1] < 1.0f || pMod->m_LastTime[1] > Sv + 1.0f)
				{
					pMod->m_LastTime[1] = pMod->m_OscPhase[1] + floor_tpl(Sv);
				}
				if (Sv - pMod->m_LastTime[1] > 1.0f)
				{
					pMod->m_CurrentJitter[1] = cry_random(0.0f, pMod->m_OscAmplitude[1]);
					pMod->m_LastTime[1] = pMod->m_OscPhase[1] + floor_tpl(Sv);
				}
				pMod->m_TexMatrix(3, 1) = pMod->m_CurrentJitter[1];
			}
			break;
		case ETMM_Stretch:
			{
				float dv = pMod->m_OscAmplitude[1] * sin_tpl(2.0f * PI * (Sv - floor_tpl(Sv)) + 2.0f * PI * pMod->m_OscPhase[1]);
				pMod->m_TexMatrix(1, 1) = 1.0f + dv;
			}
			break;
		case ETMM_StretchRepeat:
			{
				float dv = pMod->m_OscAmplitude[1] * sin_tpl(0.5f * PI * (Sv - floor_tpl(Sv)) + 2.0f * PI * pMod->m_OscPhase[1]);
				pMod->m_TexMatrix(1, 1) = 1.0f + dv;
			}
			break;
		}

		if (pMod->m_Offs[0] != 0.0f ||
		    pMod->m_Offs[1] != 0.0f ||
		    pMod->m_Tiling[0] != 1.0f ||
		    pMod->m_Tiling[1] != 1.0f ||
		    pMod->m_Rot[0] != 0.0f ||
		    pMod->m_Rot[1] != 0.0f ||
		    pMod->m_Rot[2] != 0.0f)
		{
			float du = pMod->m_Offs[0];
			float dv = pMod->m_Offs[1];
			float su = pMod->m_Tiling[0];
			float sv = pMod->m_Tiling[1];

			pMod->m_TexMatrix = pMod->m_TexMatrix *
			                    Matrix44(su, 0, 0, 0,
			                             0, sv, 0, 0,
			                             0, 0, 1, 0,
			                             du, dv, 0, 1);

			if (pMod->m_Rot[0]) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationX(Word2Degr(pMod->m_Rot[0]) * PI / 180.0f);
			if (pMod->m_Rot[1]) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationY(Word2Degr(pMod->m_Rot[1]) * PI / 180.0f);
			if (pMod->m_Rot[2]) pMod->m_TexMatrix = pMod->m_TexMatrix * Matrix33::CreateRotationZ(Word2Degr(pMod->m_Rot[2]) * PI / 180.0f);

			if (pMod->m_Rot[0] ||
			    pMod->m_Rot[1] ||
			    pMod->m_Rot[2])
			{
				pMod->m_TexMatrix = pMod->m_TexMatrix *
				                    Matrix44(su, 0, 0, 0,
				                             0, sv, 0, 0,
				                             0, 0, 1, 0,
				                             -du, -dv, 0, 1);
			}
		}
	}

	if (pMod->m_eTGType != ETG_Stream)
	{
		switch (pMod->m_eTGType)
		{
		case ETG_World:
			{
				m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_GEN_OBJECT_LINEAR;
				for (int i = 0; i < 4; i++)
				{
					memset(&Pl, 0, sizeof(Pl));
					float* fPl = (float*)&Pl;
					fPl[i] = 1.0f;
					{
						PlTr = Pl;
					}
					pMod->m_TexGenMatrix(i, 0) = PlTr.n.x;
					pMod->m_TexGenMatrix(i, 1) = PlTr.n.y;
					pMod->m_TexGenMatrix(i, 2) = PlTr.n.z;
					pMod->m_TexGenMatrix(i, 3) = PlTr.d;
				}
			}
			break;
		case ETG_Camera:
			{
				m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_GEN_OBJECT_LINEAR;
				for (int i = 0; i < 4; i++)
				{
					memset(&Pl, 0, sizeof(Pl));
					float* fPl = (float*)&Pl;
					fPl[i] = 1.0f;
					Matrix44A matView = gcpRendD3D.GetGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left).viewMatrix;
					PlTr = TransformPlane2_NoTrans(matView, Pl);
					pMod->m_TexGenMatrix(i, 0) = PlTr.n.x;
					pMod->m_TexGenMatrix(i, 1) = PlTr.n.y;
					pMod->m_TexGenMatrix(i, 2) = PlTr.n.z;
					pMod->m_TexGenMatrix(i, 3) = PlTr.d;
				}
			}
			break;
		}
	}

	if (bTr)
	{
		m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_MATRIX;
		pMod->m_TexMatrix(0, 3) = pMod->m_TexMatrix(0, 2);
		pMod->m_TexMatrix(1, 3) = pMod->m_TexMatrix(1, 2);
		pMod->m_TexMatrix(2, 3) = pMod->m_TexMatrix(2, 2);
	}

	if (pMod->m_bTexGenProjected)
		m_Ext.m_nUpdateFlags |= HWMD_TEXCOORD_PROJ;

	if (nTSlot == 0)
		nMDMask |= m_Ext.m_nUpdateFlags;
}

//---------------------------------------------------------------------------
// Wave evaluator

float CShaderMan::EvalWaveForm(SWaveForm* wf)
{
	int val;

	float Amp;
	float Freq;
	float Phase;
	float Level;

	float animationTime = gRenDev->GetAnimationTime().GetSeconds();

	if (wf->m_Flags & WFF_LERP)
	{
		val = (int)(animationTime * 597.0f);
		val &= SStaticSinusTable::sSinTableCount - 1;
		float fLerp = SStaticSinusTable::m_tSinTable[val] * 0.5f + 0.5f;

		if (wf->m_Amp != wf->m_Amp1)
			Amp = LERP(wf->m_Amp, wf->m_Amp1, fLerp);
		else
			Amp = wf->m_Amp;

		if (wf->m_Freq != wf->m_Freq1)
			Freq = LERP(wf->m_Freq, wf->m_Freq1, fLerp);
		else
			Freq = wf->m_Freq;

		if (wf->m_Phase != wf->m_Phase1)
			Phase = LERP(wf->m_Phase, wf->m_Phase1, fLerp);
		else
			Phase = wf->m_Phase;

		if (wf->m_Level != wf->m_Level1)
			Level = LERP(wf->m_Level, wf->m_Level1, fLerp);
		else
			Level = wf->m_Level;
	}
	else
	{
		Level = wf->m_Level;
		Amp = wf->m_Amp;
		Phase = wf->m_Phase;
		Freq = wf->m_Freq;
	}

	const char *sShaderName = "Unknown"; //gRenDev->m_RP.m_pShader->GetName();

	switch (wf->m_eWFType)
	{
	case eWF_None:
		Warning("WARNING: CShaderMan::EvalWaveForm called with 'EWF_None' in Shader '%s'\n", sShaderName);
		break;

	case eWF_Sin:
		val = (int)((animationTime * Freq + Phase) * (float)SStaticSinusTable::sSinTableCount);
		return Amp * SStaticSinusTable::m_tSinTable[val & (SStaticSinusTable::sSinTableCount - 1)] + Level;

	// Other wave types aren't supported anymore
	case eWF_HalfSin:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType,sShaderName );
		assert(0);
		return 0;

	case eWF_InvHalfSin:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_SawTooth:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_InvSawTooth:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_Square:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_Triangle:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_Hill:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_InvHill:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	default:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		break;
	}
	return 1;
}

float CShaderMan::EvalWaveForm(SWaveForm2* wf)
{
	int val;

	const char *sShaderName = "Unknown"; //gRenDev->m_RP.m_pShader->GetName();

	float animationTime = gRenDev->GetAnimationTime().GetSeconds();

	switch (wf->m_eWFType)
	{
	case eWF_None:
		//Warning( 0,0,"WARNING: CShaderMan::EvalWaveForm called with 'EWF_None' in Shader '%s'\n", sShaderName);
		break;

	case eWF_Sin:
		val = (int)((animationTime * wf->m_Freq + wf->m_Phase) * (float)SStaticSinusTable::sSinTableCount);
		return wf->m_Amp * SStaticSinusTable::m_tSinTable[val & (SStaticSinusTable::sSinTableCount - 1)] + wf->m_Level;

	// Other wave types aren't supported anymore
	case eWF_HalfSin:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_InvHalfSin:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_SawTooth:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_InvSawTooth:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_Square:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_Triangle:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_Hill:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	case eWF_InvHill:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		assert(0);
		return 0;

	default:
		Warning("WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
		break;
	}
	return 1;
}

float CShaderMan::EvalWaveForm2(SWaveForm* wf, float frac)
{
	int val;

	const char *sShaderName = "Unknown"; //gRenDev->m_RP.m_pShader->GetName();

	if (!(wf->m_Flags & WFF_CLAMP))
		switch (wf->m_eWFType)
		{
		case eWF_None:
			Warning("CShaderMan::EvalWaveForm2 called with 'EWF_None' in Shader '%s'\n", sShaderName);
			break;

		case eWF_Sin:
			val = QRound((frac * wf->m_Freq + wf->m_Phase) * (float)SStaticSinusTable::sSinTableCount);
			val &= (SStaticSinusTable::sSinTableCount - 1);
			return wf->m_Amp * SStaticSinusTable::m_tSinTable[val] + wf->m_Level;

		// Other wave types aren't supported anymore
		case eWF_SawTooth:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_InvSawTooth:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_Square:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_Triangle:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_Hill:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_InvHill:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		default:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			break;
		}
	else
		switch (wf->m_eWFType)
		{
		case eWF_None:
			Warning("Warning: CShaderMan::EvalWaveForm2 called with 'EWF_None' in Shader '%s'\n", sShaderName);
			break;

		case eWF_Sin:
			val = QRound((frac * wf->m_Freq + wf->m_Phase) * (float)SStaticSinusTable::sSinTableCount);
			val &= (SStaticSinusTable::sSinTableCount - 1);
			return wf->m_Amp * SStaticSinusTable::m_tSinTable[val] + wf->m_Level;

		// Other wave types aren't supported anymore
		case eWF_SawTooth:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_InvSawTooth:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_Square:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_Triangle:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_Hill:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		case eWF_InvHill:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			assert(0);
			return 0;

		default:
			Warning("Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, sShaderName);
			break;
		}
	return 1;
}

void CShaderMan::mfBeginFrame()
{
	LOADING_TIME_PROFILE_SECTION;
}

void CHWShader::mfCleanupCache()
{
	assert(CResFile::m_nNumOpenResources == 0);
	CResFile::m_nMaxOpenResFiles = 4;
}

EHWShaderClass CHWShader::mfStringProfile(const char* profile)
{
	EHWShaderClass Profile = eHWSC_Num;
	if (!strncmp(profile, "vs_5_0", 6) || !strncmp(profile, "vs_4_0", 6) || !strncmp(profile, "vs_3_0", 6))
		Profile = eHWSC_Vertex;
	else if (!strncmp(profile, "ps_5_0", 6) || !strncmp(profile, "ps_4_0", 6) || !strncmp(profile, "ps_3_0", 6))
		Profile = eHWSC_Pixel;
	else if (!strncmp(profile, "gs_5_0", 6) || !strncmp(profile, "gs_4_0", 6))
		Profile = eHWSC_Geometry;
	else if (!strncmp(profile, "hs_5_0", 6))
		Profile = eHWSC_Hull;
	else if (!strncmp(profile, "ds_5_0", 6))
		Profile = eHWSC_Domain;
	else if (!strncmp(profile, "cs_5_0", 6))
		Profile = eHWSC_Compute;
	else
		assert(0);

	return Profile;
}
EHWShaderClass CHWShader::mfStringClass(const char* szClass)
{
	EHWShaderClass Profile = eHWSC_Num;
	if (!strnicmp(szClass, "VS", 2))
		Profile = eHWSC_Vertex;
	else if (!strnicmp(szClass, "PS", 2))
		Profile = eHWSC_Pixel;
	else if (!strnicmp(szClass, "GS", 2))
		Profile = eHWSC_Geometry;
	else if (!strnicmp(szClass, "HS", 2))
		Profile = eHWSC_Hull;
	else if (!strnicmp(szClass, "DS", 2))
		Profile = eHWSC_Domain;
	else if (!strnicmp(szClass, "CS", 2))
		Profile = eHWSC_Compute;
	else
		assert(0);

	return Profile;
}
const char* CHWShader::mfProfileString(EHWShaderClass eClass)
{
	char* szProfile = "Unknown";
	switch (eClass)
	{
	case eHWSC_Vertex:
		if (CParserBin::m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_VULKAN | SF_GL4 | SF_GLES3))
			szProfile = "vs_5_0";
		else
			assert(0);
		break;
	case eHWSC_Pixel:
		if (CParserBin::m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_VULKAN | SF_GL4 | SF_GLES3))
			szProfile = "ps_5_0";
		else
			assert(0);
		break;
	case eHWSC_Geometry:
		if (CParserBin::PlatformSupportsGeometryShaders())
			szProfile = "gs_5_0";
		else
			assert(0);
		break;
	case eHWSC_Domain:
		if (CParserBin::PlatformSupportsDomainShaders())
			szProfile = "ds_5_0";
		else
			assert(0);
		break;
	case eHWSC_Hull:
		if (CParserBin::PlatformSupportsHullShaders())
			szProfile = "hs_5_0";
		else
			assert(0);
		break;
	case eHWSC_Compute:
		if (CParserBin::PlatformSupportsComputeShaders())
			szProfile = "cs_5_0";
		else
			assert(0);
		break;
	default:
		assert(0);
	}
	return szProfile;
}
const char* CHWShader::mfClassString(EHWShaderClass eClass)
{
	char* szClass = "Unknown";
	switch (eClass)
	{
	case eHWSC_Vertex:
		szClass = "VS";
		break;
	case eHWSC_Pixel:
		szClass = "PS";
		break;
	case eHWSC_Geometry:
		szClass = "GS";
		break;
	case eHWSC_Domain:
		szClass = "DS";
		break;
	case eHWSC_Hull:
		szClass = "HS";
		break;
	case eHWSC_Compute:
		szClass = "CS";
		break;
	default:
		assert(0);
	}
	return szClass;
}

//==========================================================================================================

inline bool sCompareRes(CShaderResources* a, CShaderResources* b)
{
	if (!a || !b)
		return a < b;

	if (a->m_AlphaRef != b->m_AlphaRef)
		return a->m_AlphaRef < b->m_AlphaRef;

	if (a->GetStrengthValue(EFTT_OPACITY) != b->GetStrengthValue(EFTT_OPACITY))
		return a->GetStrengthValue(EFTT_OPACITY) < b->GetStrengthValue(EFTT_OPACITY);

	if (a->m_pDeformInfo != b->m_pDeformInfo)
		return a->m_pDeformInfo < b->m_pDeformInfo;

	if (a->m_pDetailDecalInfo != b->m_pDetailDecalInfo)
		return a->m_pDetailDecalInfo < b->m_pDetailDecalInfo;

	if (a->m_RTargets.Num() != b->m_RTargets.Num())
		return a->m_RTargets.Num() < b->m_RTargets.Num();

	if ((a->m_ResFlags & MTL_FLAG_2SIDED) != (b->m_ResFlags & MTL_FLAG_2SIDED))
		return (a->m_ResFlags & MTL_FLAG_2SIDED) < (b->m_ResFlags & MTL_FLAG_2SIDED);

	ITexture* pTA = NULL;
	ITexture* pTB = NULL;
	if (a->m_Textures[EFTT_SPECULAR])
		pTA = a->m_Textures[EFTT_SPECULAR]->m_Sampler.m_pITex;
	if (b->m_Textures[EFTT_SPECULAR])
		pTB = b->m_Textures[EFTT_SPECULAR]->m_Sampler.m_pITex;
	if (pTA != pTB)
		return pTA < pTB;

	pTA = NULL;
	pTB = NULL;
	if (a->m_Textures[EFTT_DIFFUSE])
		pTA = a->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pITex;
	if (b->m_Textures[EFTT_DIFFUSE])
		pTB = b->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pITex;
	if (pTA != pTB)
		return pTA < pTB;

	pTA = NULL;
	pTB = NULL;
	if (a->m_Textures[EFTT_NORMALS])
		pTA = a->m_Textures[EFTT_NORMALS]->m_Sampler.m_pITex;
	if (b->m_Textures[EFTT_NORMALS])
		pTB = b->m_Textures[EFTT_NORMALS]->m_Sampler.m_pITex;
	if (pTA != pTB)
		return pTA < pTB;

	pTA = NULL;
	pTB = NULL;
	if (a->m_Textures[EFTT_ENV])
		pTA = a->m_Textures[EFTT_ENV]->m_Sampler.m_pITex;
	if (b->m_Textures[EFTT_ENV])
		pTB = b->m_Textures[EFTT_ENV]->m_Sampler.m_pITex;
	if (pTA != pTB)
		return pTA < pTB;

	pTA = NULL;
	pTB = NULL;
	if (a->m_Textures[EFTT_DECAL_OVERLAY])
		pTA = a->m_Textures[EFTT_DECAL_OVERLAY]->m_Sampler.m_pITex;
	if (b->m_Textures[EFTT_DECAL_OVERLAY])
		pTB = b->m_Textures[EFTT_DECAL_OVERLAY]->m_Sampler.m_pITex;
	if (pTA != pTB)
		return pTA < pTB;

	pTA = NULL;
	pTB = NULL;
	if (a->m_Textures[EFTT_DETAIL_OVERLAY])
		pTA = a->m_Textures[EFTT_DETAIL_OVERLAY]->m_Sampler.m_pITex;
	if (b->m_Textures[EFTT_DETAIL_OVERLAY])
		pTB = b->m_Textures[EFTT_DETAIL_OVERLAY]->m_Sampler.m_pITex;
	return (pTA < pTB);
}

inline bool sIdenticalRes(CShaderResources* a, CShaderResources* b, bool bCheckTexture)
{
	if (a->m_AlphaRef != b->m_AlphaRef)
		return false;

	if (a->GetStrengthValue(EFTT_OPACITY) != b->GetStrengthValue(EFTT_OPACITY))
		return false;

	if (a->m_pDeformInfo != b->m_pDeformInfo)
		return false;

	if (a->m_pDetailDecalInfo != b->m_pDetailDecalInfo)
		return false;

	if (a->m_RTargets.Num() != b->m_RTargets.Num())
		return false;

	if ((a->m_ResFlags & (MTL_FLAG_2SIDED | MTL_FLAG_ADDITIVE)) != (b->m_ResFlags & (MTL_FLAG_2SIDED | MTL_FLAG_ADDITIVE)))
		return false;

	if (bCheckTexture)
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			if (a->m_Textures[i] != b->m_Textures[i])
				return false;
		}
	}
	else
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			if (i == EFTT_DETAIL_OVERLAY)
			{
				if (a->m_Textures[i] != b->m_Textures[i])
					return false;
			}
			if (a->m_Textures[i] && a->m_Textures[i]->IsHasModificators())
				return false;
			if (b->m_Textures[i] && b->m_Textures[i]->IsHasModificators())
				return false;
		}
	}

	const float emissiveIntensity = a->GetStrengthValue(EFTT_EMITTANCE);
	if (emissiveIntensity != b->GetStrengthValue(EFTT_EMITTANCE))
		return false;
	if (emissiveIntensity > 0)
	{
		if (a->GetColorValue(EFTT_EMITTANCE) != b->GetColorValue(EFTT_EMITTANCE))
			return false;
	}

	const SEfResTexture* pDiffuseTexA = a->m_Textures[EFTT_DIFFUSE];
	const SEfResTexture* pDiffuseTexB = b->m_Textures[EFTT_DIFFUSE];
	const IDynTextureSource* pDynTexSrcA = pDiffuseTexA ? pDiffuseTexA->m_Sampler.m_pDynTexSource : 0;
	const IDynTextureSource* pDynTexSrcB = pDiffuseTexB ? pDiffuseTexB->m_Sampler.m_pDynTexSource : 0;
	if (pDynTexSrcA != pDynTexSrcB)
		return false;

	return true;
}

inline bool sCompareShd(CBaseResource* a, CBaseResource* b)
{
	if (!a || !b)
		return a < b;

	CShader* pA = (CShader*)a;
	CShader* pB = (CShader*)b;
	SShaderPass* pPA = NULL;
	SShaderPass* pPB = NULL;
	if (pA->m_HWTechniques.Num() && pA->m_HWTechniques[0]->m_Passes.Num())
		pPA = &pA->m_HWTechniques[0]->m_Passes[0];
	if (pB->m_HWTechniques.Num() && pB->m_HWTechniques[0]->m_Passes.Num())
		pPB = &pB->m_HWTechniques[0]->m_Passes[0];
	if (!pPA || !pPB)
		return pPA < pPB;

	if (pPA->m_VShader != pPB->m_VShader)
		return pPA->m_VShader < pPB->m_VShader;
	return (pPA->m_PShader < pPB->m_PShader);
}

void CShaderMan::mfSortResources()
{
	uint32 i;
	
	iLog->Log("-- Presort shaders by states...");
	//return;
	std::sort(&CShader::s_ShaderResources_known.begin()[1], CShader::s_ShaderResources_known.end(), sCompareRes);

	int nGroups = 20000;
	CShaderResources* pPrev = NULL;
	for (i = 1; i < CShader::s_ShaderResources_known.Num(); i++)
	{
		CShaderResources* pRes = CShader::s_ShaderResources_known[i];
		if (pRes)
		{
			pRes->m_Id = i;
			pRes->m_IdGroup = i;
			if (CRenderer::CV_r_materialsbatching)
			{
				if (pPrev)
				{
					if (!sIdenticalRes(pRes, pPrev, false))
						nGroups++;
				}
				pRes->m_IdGroup = nGroups;
			}
			pPrev = pRes;
		}
	}
	iLog->Log("--- %u Resources, %d Resource groups.", CShader::s_ShaderResources_known.Num(), nGroups - 20000);

	{
		CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

		SResourceContainer* pRL = CShaderMan::s_pContainer;
		assert(pRL);
		if (pRL)
		{
			std::sort(pRL->m_RList.begin(), pRL->m_RList.end(), sCompareShd);
			for (i = 0; i < pRL->m_RList.size(); i++)
			{
				CShader* pSH = (CShader*)pRL->m_RList[i];
				if (pSH)
					pSH->SetID(CBaseResource::IdFromRListIndex(i));
			}

			pRL->m_AvailableIDs.clear();
			for (i = 0; i < pRL->m_RList.size(); i++)
			{
				CShader* pSH = (CShader*)pRL->m_RList[i];
				if (!pSH)
					pRL->m_AvailableIDs.push_back(CBaseResource::IdFromRListIndex(i));
			}
		}
	}
}

//---------------------------------------------------------------------------

void CLightStyle::mfUpdate(float fTime)
{
	float m = fTime * m_TimeIncr;
	//    if (m != m_LastTime)
	{
		m_LastTime = m;
		if (m_Map.Num())
		{
			if (m_Map.Num() == 1)
			{
				m_Color = m_Map[0].cColor;
			}
			else
			{
				int first = (int)QInt(m);
				int second = (first + 1);
				float fLerp = m - (float)first;

				// Interpolate between key-frames
				// todo: try different interpolation method

				ColorF& cColA = m_Map[first % m_Map.Num()].cColor;
				ColorF& cColB = m_Map[second % m_Map.Num()].cColor;
				m_Color = LERP(cColA, cColB, fLerp);

				Vec3& vPosA = m_Map[first % m_Map.Num()].vPosOffset;
				Vec3& vPosB = m_Map[second % m_Map.Num()].vPosOffset;
				m_vPosOffset = LERP(vPosA, vPosB, fLerp);
			}
		}
	}
}

void CShaderMan::FilterShaderCacheGenListForOrbis(FXShaderCacheCombinations& combinations)
{
	// This function will perform shader list mutation for Orbis targets based on a target ISA list.
	// The target list is stored in the CVar r_ShadersOrbisFiltering, with one character per item.
	// L: Legacy combinations (ie, those without pipeline state), when present, are passed through.
	// B/C/N/R: ISA combinations (ie, those with pipeline state), for each present character, the combination is generated if not present.
	enum ECombinationKind
	{
		kCombinationB,
		kCombinationC,
		kCombinationN,
		kCombinationR,
		kCombinationLegacy,
		kCombinationCount,
	};
	bool combinationFilter[kCombinationCount] = { false };

	static const char* const szFilter = REGISTER_STRING("r_ShadersOrbisFiltering", "L", VF_NULL, "")->GetString();
	for (const char* szFilterItem = szFilter; szFilterItem && *szFilterItem; ++szFilterItem)
	{
		switch (*szFilterItem)
		{
		case 'l':
		case 'L':
			combinationFilter[kCombinationLegacy] = true;
			break;
		case 'b':
		case 'B':
			combinationFilter[kCombinationB] = true;
			break;
		case 'c':
		case 'C':
			combinationFilter[kCombinationC] = true;
			break;
		case 'n':
		case 'N':
			combinationFilter[kCombinationN] = true;
			break;
		case 'r':
		case 'R':
			combinationFilter[kCombinationR] = true;
			break;
		}
	}

	FXShaderCacheCombinations result;
	for (auto& item : combinations)
	{
		if (!item.second.Ident.m_pipelineState.opaque)
		{
			if (combinationFilter[kCombinationLegacy])
			{
				result[item.first] = item.second; // Copy legacy combination unchanged
			}
		}
		else
		{
			uint8 isa = item.second.Ident.m_pipelineState.GNM.GetISA(item.second.eCL);
			const auto insertIsa = [&result, &item, isa](uint8 targetIsa)
			{
				item.second.Ident.m_pipelineState.GNM.SetISA(item.second.eCL, targetIsa);

				string name = item.first.c_str();
				const string::size_type endPos = name.find_last_of('(', string::npos) - 1;
				const string::size_type beginPos = name.find_last_of('(', endPos) + 1;
				const string prefix = name.substr(0, beginPos);
				const string affix = name.substr(endPos);
				name.Format("%s%llx%s", prefix.c_str(), item.second.Ident.m_pipelineState.opaque, affix.c_str());

				result[CCryNameR(name.c_str())] = item.second;
			};
			
			// Create ISA mutations
			if (combinationFilter[kCombinationB])
			{
				insertIsa(kCombinationB);
			}
			if (combinationFilter[kCombinationC])
			{
				insertIsa(kCombinationC);
			}
			if (combinationFilter[kCombinationN])
			{
				insertIsa(kCombinationN);
			}
			if (combinationFilter[kCombinationR])
			{
				insertIsa(kCombinationR);
			}
		}
	}
	result.swap(combinations);
}
