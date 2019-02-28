// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShaderScript.cpp : loading/reloading/hashing of shader scipts.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include "../Common/Shaders/RemoteCompiler.h"
#include <CryGame/IGameFramework.h>
#include "../../XRenderD3D9/D3DMultiResRendering.h"

#if CRY_PLATFORM_WINDOWS
	#include <direct.h>
	#include <io.h>
#endif

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( push )               //AMD Port
	#pragma warning( disable : 4267 )
#endif

extern char* gShObjectNotFound;

//=================================================================================================

bool CShader::Reload(int nFlags, const char* szShaderName)
{
	CShader* pShaderGen = NULL;
	if (m_ShaderGenParams)
		pShaderGen = this;
	else if (m_pGenShader)
		pShaderGen = m_pGenShader;
	uint32 nFl = EF_RELOAD;
	if (nFlags & FRO_FORCERELOAD)
		nFl |= EF_FORCE_RELOAD;
	if (pShaderGen && pShaderGen->m_DerivedShaders)
	{
		for (uint32 i = 0; i < pShaderGen->m_DerivedShaders->size(); i++)
		{
			CShader* pShader = (*pShaderGen->m_DerivedShaders)[i];
			if (!pShader)
				continue;
			if (pShader->m_nRefreshFrame == gRenDev->m_cEF.m_nFrameForceReload)
				continue;
			pShader->m_nRefreshFrame = gRenDev->m_cEF.m_nFrameForceReload;

			gRenDev->m_cEF.mfForName(szShaderName, pShader->m_Flags | nFl, NULL, pShader->m_nMaskGenFX);
		}
	}
	else
	{
		assert(!m_nMaskGenFX);
		gRenDev->m_cEF.mfForName(szShaderName, m_Flags | nFl, NULL, m_nMaskGenFX);
	}

	return true;
}

bool CShaderMan::mfReloadShaderIncludes(const char* szPath, int nFlags)
{
	assert(szPath);

	bool bChanged = false;
	char dirn[256];
	cry_strcpy(dirn, szPath);
	cry_strcat(dirn, "*.*");
	struct _finddata_t fileinfo;
	intptr_t handle = gEnv->pCryPak->FindFirst(dirn, &fileinfo);
	if (handle != -1)
	{
		do
		{
			if (fileinfo.name[0] == '.')
				continue;
			if (fileinfo.attrib & _A_SUBDIR)
			{
				char ddd[256];
				cry_sprintf(ddd, "%s%s/", szPath, fileinfo.name);

				bChanged = mfReloadShaderIncludes(ddd, nFlags);
				continue;
			}
			char nmf[256];
			cry_strcpy(nmf, szPath);
			cry_strcat(nmf, fileinfo.name);
			int len = strlen(nmf) - 1;
			while (len >= 0 && nmf[len] != '.')
				len--;
			if (len <= 0)
				continue;
			if (!stricmp(&nmf[len], ".cfi"))
			{
				cry_strcpy(nmf, fileinfo.name);
				PathUtil::RemoveExtension(nmf);
				bool bCh = false;
				SShaderBin* pBin = m_Bin.GetBinShader(nmf, true, 0, &bCh);
				if (bCh)
				{
					bChanged = true;
					mfReloadFile(szPath, fileinfo.name, nFlags);
				}
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);

		gEnv->pCryPak->FindClose(handle);
	}
	return bChanged;
}

bool CShaderMan::mfReloadAllShaders(int nFlags, uint32 nFlagsHW, int currentFrameID)
{
	bool bState = true;
	m_nFrameForceReload++;

	gRenDev->FlushRTCommands(true, true, true);
	m_Bin.InvalidateCache();
	CHWShader::mfFlushPendedShadersWait(-1);

	if (!gRenDev->IsShaderCacheGenMode())
	{
		gRenDev->FlushRTCommands(true, true, true);
	}

	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	// Check include changing
	if (m_ShadersPath && !CRenderer::CV_r_shadersignoreincludeschanging)
	{
		bool bChanged = mfReloadShaderIncludes(m_ShadersPath, nFlags);
	}
	CCryNameTSCRC Name = CShader::mfGetClassName();
	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CShader* pS = (CShader*)itor->second;
			if (!pS)
				continue;
			if (nFlagsHW)
			{
				if (!pS->m_pGenShader)
					continue;
				SShaderGen* pGen = pS->m_pGenShader->m_ShaderGenParams;
				assert(pGen);
				if (!pGen)
					continue;
				uint32 i;
				SShaderGenBit* pBit;
				for (i = 0; i < pGen->m_BitMask.Num(); i++)
				{
					pBit = pGen->m_BitMask[i];
					if ((pBit->m_nDependencySet | pBit->m_nDependencyReset) & nFlagsHW)
						break;
				}
				if (i == pGen->m_BitMask.Num())
					continue;
				pS->Reload(nFlags | FRO_FORCERELOAD, pS->GetName());
			}
			else
			{
				stack_string name;
				name.Format("%sCryFX/%s.cfx", m_ShadersGamePath.c_str(), pS->GetName());
				FILE* fp = gEnv->pCryPak->FOpen(name.c_str(), "rb");
				if (!fp)
				{
					name.Format("%sCryFX/%s.cfx", m_ShadersPath, pS->GetName());
					fp = gEnv->pCryPak->FOpen(name.c_str(), "rb");
				}
				if (fp)
				{
					uint32 nSourceCRC32 = gEnv->pCryPak->ComputeCRC(name.c_str());
					gEnv->pCryPak->FClose(fp);
					if ((nFlags & FRO_FORCERELOAD) || nSourceCRC32 != pS->m_SourceCRC32)
					{
						pS->m_SourceCRC32 = nSourceCRC32;
						pS->Reload(nFlags, pS->GetName());
					}
				}
			}
		}
	}

	gRenDev->FlushRTCommands(true, true, true);
	CHWShader::mfFlushPendedShadersWait(-1);

	for (auto pShaderResources : CShader::s_ShaderResources_known)
	{
		if (pShaderResources)
		{
			pShaderResources->ClearPipelineStateCache();
		}
	}

	GetDeviceObjectFactory().ReloadPipelineStates(currentFrameID);

	return bState;
}

static bool sCheckAffecting_r(SShaderBin* pBin, const char* szShaderName)
{
	pBin->Lock();
	int nTok = 0;
	// Check first level
	while (nTok >= 0)
	{
		nTok = CParserBin::FindToken(nTok, pBin->m_Tokens.size() - 1, &pBin->m_Tokens[0], eT_include);
		if (nTok >= 0)
		{
			nTok++;
			uint32 nTokName = pBin->m_Tokens[nTok];
			const char* szNameInc = CParserBin::GetString(nTokName, pBin->m_TokenTable);
			if (!stricmp(szNameInc, szShaderName))
				break;
		}
	}
	// Check recursively
	if (nTok < 0)
	{
		nTok = 0;
		while (nTok >= 0)
		{
			nTok = CParserBin::FindToken(nTok, pBin->m_Tokens.size() - 1, &pBin->m_Tokens[0], eT_include);
			if (nTok >= 0)
			{
				nTok++;
				uint32 nTokName = pBin->m_Tokens[nTok];
				const char* szNameInc = CParserBin::GetString(nTokName, pBin->m_TokenTable);
				if (!stricmp(szNameInc, szShaderName))
					break;
				SShaderBin* pBinIncl = gRenDev->m_cEF.m_Bin.GetBinShader(szNameInc, true, 0);
				bool bAffect = sCheckAffecting_r(pBinIncl, szShaderName);
				if (bAffect)
					break;
			}
		}

	}
	pBin->Unlock();
	return (nTok >= 0);
}

bool CShaderMan::mfReloadFile(const char* szPath, const char* szName, int nFlags)
{
	CHWShader::mfFlushPendedShadersWait(-1);

	m_nFrameForceReload++;

	const char* szExt = PathUtil::GetExt(szName);
	if (!stricmp(szExt, "cfx"))
	{
		m_bReload = true;
		char szShaderName[256];
		cry_strcpy(szShaderName, szName, std::distance(szName, szExt - 1)); // skip '.' as well
		strlwr(szShaderName);

		// Check if this shader already loaded
		CBaseResource* pBR = CBaseResource::GetResource(CShader::mfGetClassName(), szShaderName, false);
		if (pBR)
		{
			CShader* pShader = (CShader*)pBR;
			pShader->Reload(nFlags, szShaderName);
		}
		m_bReload = false;
	}
	else if (!stricmp(szExt, "cfi"))
	{
		CCryNameTSCRC Name = CShader::mfGetClassName();
		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(Name);
		if (pRL)
		{
			m_bReload = true;
			char szShaderName[256];
			cry_strcpy(szShaderName, szName, std::distance(szName, szExt - 1)); // skip '.' as well
			strlwr(szShaderName);
			SShaderBin* pBin = m_Bin.GetBinShader(szShaderName, true, 0);
			bool bAffect = false;

			ResourcesMapItor itor;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CShader* sh = (CShader*)itor->second;
				if (!sh || !sh->GetName()[0])
					continue;
				pBin = m_Bin.GetBinShader(sh->GetName(), false, 0);
				if (!pBin)
					continue;
				bAffect = sCheckAffecting_r(pBin, szShaderName);
				if (bAffect)
					sh->Reload(nFlags | FRO_FORCERELOAD, sh->GetName());
			}
		}
		m_bReload = false;
	}

	return false;
}

//=============================================================================

void CShaderMan::mfFillGenMacroses(SShaderGen* shG, TArray<char>& buf, uint64 nMaskGen)
{
	uint32 i;

	if (!nMaskGen || !shG)
		return;

	for (i = 0; i < shG->m_BitMask.Num(); i++)
	{
		if (shG->m_BitMask[i]->m_Mask & nMaskGen)
		{
			char macro[256];
#if defined(__GNUC__)
			cry_sprintf(macro, "#define %s 0x%llx\n", shG->m_BitMask[i]->m_ParamName.c_str(), (unsigned long long)shG->m_BitMask[i]->m_Mask);
#else
			cry_sprintf(macro, "#define %s 0x%I64x\n", shG->m_BitMask[i]->m_ParamName.c_str(), (uint64)shG->m_BitMask[i]->m_Mask);
#endif
			int size = strlen(macro);
			int nOffs = buf.Num();
			buf.Grow(size);
			memcpy(&buf[nOffs], macro, size);
		}
	}
}

bool CShaderMan::mfModifyGenFlags(CShader* efGen, const CShaderResources* pRes, uint64& nMaskGen, uint64& nMaskGenH)
{
	if (!efGen || !efGen->m_ShaderGenParams)
		return false;
	uint64 nAndMaskHW = -1;
	uint64 nMaskGenHW = 0;

	// Remove non-used flags first
	unsigned int i = 0;
	SShaderGen* pGen = efGen->m_ShaderGenParams;
	int j;

	//char _debug_[256];
	//cry_sprintf( _debug_, "curr shadergen %I64x %I64x\n", nMaskGen, nMaskGenH);
	//OutputDebugString(_debug_);

	if (nMaskGen)
	{
		for (j = 0; j < 64; j++)
		{
			uint64 nMask = (((uint64)1) << j);
			if (nMaskGen & nMask)
			{
				for (i = 0; i < pGen->m_BitMask.Num(); i++)
				{
					SShaderGenBit* pBit = pGen->m_BitMask[i];
					if (pBit->m_Mask & nMask)
						break;
				}

				if (i == pGen->m_BitMask.Num())
					nMaskGen &= ~nMask;

			}
		}
	}

	for (i = 0; i < pGen->m_BitMask.Num(); i++)
	{
		SShaderGenBit* pBit = pGen->m_BitMask[i];
		if (!pBit || (!pBit->m_nDependencySet && !pBit->m_nDependencyReset))
			continue;
		if (pRes)
		{
			if (pBit->m_nDependencySet & SHGD_TEX_NORMALS)
			{
				if (!pRes->IsEmpty(EFTT_NORMALS))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_NORMALS)
			{
				if (pRes->IsEmpty(EFTT_NORMALS))
					nMaskGen &= ~pBit->m_Mask;
			}
			if (pBit->m_nDependencySet & SHGD_TEX_HEIGHT)
			{
				if (!pRes->IsEmpty(EFTT_HEIGHT))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_HEIGHT)
			{
				if (pRes->IsEmpty(EFTT_HEIGHT))
					nMaskGen &= ~pBit->m_Mask;
			}

			if (pBit->m_nDependencySet & SHGD_TEX_DETAIL)
			{
				if (!pRes->IsEmpty(EFTT_DETAIL_OVERLAY))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_DETAIL)
			{
				if (pRes->IsEmpty(EFTT_DETAIL_OVERLAY))
					nMaskGen &= ~pBit->m_Mask;
			}

			if (pBit->m_nDependencySet & SHGD_TEX_TRANSLUCENCY)
			{
				if (!pRes->IsEmpty(EFTT_TRANSLUCENCY))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_TRANSLUCENCY)
			{
				if (pRes->IsEmpty(EFTT_TRANSLUCENCY))
					nMaskGen &= ~pBit->m_Mask;
			}

			if (pBit->m_nDependencySet & SHGD_TEX_SPECULAR)
			{
				if (!pRes->IsEmpty(EFTT_SPECULAR))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_SPECULAR)
			{
				if (pRes->IsEmpty(EFTT_SPECULAR))
					nMaskGen &= ~pBit->m_Mask;
			}

			if (pBit->m_nDependencySet & SHGD_TEX_ENVCM)
			{
				if (!pRes->IsEmpty(EFTT_ENV))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_ENVCM)
			{
				if (pRes->IsEmpty(EFTT_ENV))
					nMaskGen &= ~pBit->m_Mask;
			}

			if (pBit->m_nDependencySet & SHGD_TEX_SUBSURFACE)
			{
				if (!pRes->IsEmpty(EFTT_SUBSURFACE))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_SUBSURFACE)
			{
				if (pRes->IsEmpty(EFTT_SUBSURFACE))
					nMaskGen &= ~pBit->m_Mask;
			}

			if (pBit->m_nDependencySet & SHGD_TEX_DECAL)
			{
				if (!pRes->IsEmpty(EFTT_DECAL_OVERLAY))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_DECAL)
			{
				if (pRes->IsEmpty(EFTT_DECAL_OVERLAY))
					nMaskGen &= ~pBit->m_Mask;
			}

			if (pBit->m_nDependencySet & SHGD_TEX_CUSTOM)
			{
				if (!pRes->IsEmpty(EFTT_CUSTOM))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_CUSTOM)
			{
				if (pRes->IsEmpty(EFTT_CUSTOM))
					nMaskGen &= ~pBit->m_Mask;
			}

			if (pBit->m_nDependencySet & SHGD_TEX_CUSTOM_SECONDARY)
			{
				if (!pRes->IsEmpty(EFTT_CUSTOM_SECONDARY))
					nMaskGen |= pBit->m_Mask;
			}
			if (pBit->m_nDependencyReset & SHGD_TEX_CUSTOM_SECONDARY)
			{
				if (pRes->IsEmpty(EFTT_CUSTOM_SECONDARY))
					nMaskGen &= ~pBit->m_Mask;
			}
		}

		// Specific case - for user gl flags (eg: TEMP_TERRAIN, TEMP_VEGETATION)
		// this is needed since we now use common shader global flags - else define inside shader.cfi will override correct shared value
		if (pBit->m_nDependencySet & SHGD_USER_ENABLED)
			nMaskGen |= pBit->m_Mask;

		//if (m_nCombinationsProcess < 0 || m_bActivatePhase)
		{
			if (CParserBin::m_bShaderCacheGen)
			{
				// during shader cache gen, disable the special features in non D3D11 mode, and just accept
				// the lines as they come in D3D11 mode
				if ((CParserBin::m_nPlatform & (SF_D3D11 | SF_DURANGO | SF_GL4 | SF_VULKAN)) == 0)
				{
					if (pBit->m_nDependencySet & SHGD_HW_WATER_TESSELLATION)
						nAndMaskHW &= ~pBit->m_Mask;
					if (pBit->m_nDependencySet & SHGD_HW_SILHOUETTE_POM)
						nAndMaskHW &= ~pBit->m_Mask;
				}
			}
			else
			{
				PREFAST_SUPPRESS_WARNING(6326)
				bool bWaterTessHW = CRenderer::CV_r_WaterTessellationHW != 0 && gRenDev->m_bDeviceSupportsTessellation;
				if (pBit->m_nDependencySet & SHGD_HW_WATER_TESSELLATION)
				{
					nAndMaskHW &= ~pBit->m_Mask;
					if (bWaterTessHW)
						nMaskGenHW |= pBit->m_Mask;
				}
				if (pBit->m_nDependencyReset & SHGD_HW_WATER_TESSELLATION)
				{
					nAndMaskHW &= ~pBit->m_Mask;
					if (!bWaterTessHW)
						nMaskGenHW &= ~pBit->m_Mask;
				}

				PREFAST_SUPPRESS_WARNING(6326)
				const bool useSilhouettePOM = CRenderer::CV_r_SilhouettePOM != 0 && !CVrProjectionManager::IsMultiResEnabledStatic();
				if (pBit->m_nDependencySet & SHGD_HW_SILHOUETTE_POM)
				{
					nAndMaskHW &= ~pBit->m_Mask;
					if (useSilhouettePOM)
						nMaskGenHW |= pBit->m_Mask;
				}
				if (pBit->m_nDependencyReset & SHGD_HW_SILHOUETTE_POM)
				{
					nAndMaskHW &= ~pBit->m_Mask;
					if (!useSilhouettePOM)
						nMaskGenHW &= ~pBit->m_Mask;
				}
			}
		}
	}
	nMaskGen &= nAndMaskHW;
	nMaskGenH |= nMaskGenHW;

	//cry_sprintf( _debug_, "remap shadergen %I64x %I64x\n", nMaskGen, nMaskGenH);
	//OutputDebugString(_debug_);

	return true;
}

bool CShaderMan::mfUpdateTechnik(SShaderItem& SI, CCryNameTSCRC& Name)
{
	CShader* pSH = (CShader*)SI.m_pShader;
	if (!(pSH->m_Flags & EF_LOADED))
		return false;
	uint32 i;
	for (i = 0; i < pSH->m_HWTechniques.Num(); i++)
	{
		SShaderTechnique* pTech = pSH->m_HWTechniques[i];
		//if (!(pTech->m_Flags & FHF_PUBLIC))
		//  continue;
		if (pTech->m_NameCRC == Name)
			break;
	}
	if (i == pSH->m_HWTechniques.Num())
	{
		SI.m_nTechnique = -1;
		Warning("ERROR: CShaderMan::mfUpdateTechnik: couldn't find public technique for shader '%s'", pSH->GetName());
	}
	else
		SI.m_nTechnique = i;

	return true;
}

SShaderItem CShaderMan::mfShaderItemForName(const char* nameEf, bool bShare, int flags, SInputShaderResources* Res, uint64 nMaskGen, const IRenderer::SLoadShaderItemArgs* pArgs)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Shader, 0, "ShaderItem (%s)", nameEf);

	SShaderItem SI;

	CShaderResources* pResource = NULL;
	if (Res)
	{
		SI.m_pShaderResources = mfCreateShaderResources(Res, bShare);
		pResource = (CShaderResources*)SI.m_pShaderResources;
		pResource->m_ShaderParams = Res->m_ShaderParams;
		m_pCurInputResources = Res;
	}

	string strShader = nameEf;
	string strTechnique;
	string strNew = strShader.SpanExcluding(".");
	if (strNew.size() != strShader.size())
	{
		string second = string(&strShader.c_str()[strNew.size() + 1]);
		strShader = strNew;
		strTechnique = second;
	}

	if (SI.m_pShaderResources)
	{
		mfRefreshResources((CShaderResources*)SI.m_pShaderResources, pArgs);
	}

	char shadName[256];
	if (nameEf && nameEf[0])
		cry_strcpy(shadName, strShader.c_str());
	else
		shadName[0] = 0;

	SI.m_pShader = mfForName(shadName, flags, (CShaderResources*)SI.m_pShaderResources, nMaskGen);
	CShader* pSH = (CShader*)SI.m_pShader;

	// Get technique
	if (strTechnique.size())
	{
		CCryNameTSCRC nTech(strTechnique.c_str());
		if (!mfUpdateTechnik(SI, nTech))
		{
			SI.m_nTechnique = nTech.get(); // Postpone
		}
	}
	SI.m_nPreprocessFlags = -1;
	if (Res)
	{
		SI.m_pShaderResources = pResource;
		if (pResource)
			pResource->CreateModifiers(Res);
	}
	m_pCurInputResources = NULL;
	return SI;
}

CShader* CShaderMan::mfForName(const char* nameSh, int flags, const CShaderResources* Res, uint64 nMaskGen)
{
	if (!nameSh || !nameSh[0])
	{
		Warning("Warning: CShaderMan::mfForName: NULL name\n");
		s_DefaultShader->AddRef();
		return s_DefaultShader;
	}

	char nameEf[256];
	char nameNew[256];
	char nameRes[256];

	uint64 nMaskGenHW = 0;

	cry_strcpy(nameEf, nameSh);

	cry_strcpy(nameRes, nameEf);
	if (CParserBin::m_nPlatform == SF_D3D11)
		strcat(nameRes, "(DX1)");
	else if (CParserBin::m_nPlatform == SF_GL4)
		cry_strcat(nameRes, "(G4)");
	else if (CParserBin::m_nPlatform == SF_GLES3)
		cry_strcat(nameRes, "(E3)");
	else if (CParserBin::m_nPlatform == SF_ORBIS)
		strcat(nameRes, "(O)");
	else if (CParserBin::m_nPlatform == SF_DURANGO)
		cry_strcat(nameRes, "(D)");
	else if (CParserBin::m_nPlatform == SF_VULKAN)
		cry_strcat(nameRes, "(VK)");

	const auto nameEfCrc = CCrc32::ComputeLowercase(nameEf);

	CShader* efGen = nullptr;

	// Check if this shader already loaded
	CBaseResource* pBR = CBaseResource::GetResource(CShader::mfGetClassName(), nameRes, false);
	bool bGenModified = false;
	CShader* ef = (CShader*)pBR;
	if (ef && ef->m_ShaderGenParams)
	{
		efGen = ef;
		//if (!(flags & EF_RELOAD))
		//  nMaskGen = gRenDev->EF_GetRemapedShaderMaskGen(nameSh, nMaskGen | nMaskGenHW);
		mfModifyGenFlags(efGen, Res, nMaskGen, nMaskGenHW);
		bGenModified = true;
#ifdef __GNUC__
		cry_sprintf(nameNew, "%s(%llx)", nameRes, nMaskGen);
#else
		cry_sprintf(nameNew, "%s(%I64x)", nameRes, nMaskGen);
#endif
		pBR = CBaseResource::GetResource(CShader::mfGetClassName(), nameNew, false);
		ef = (CShader*)pBR;
		if (ef)
		{
			// Update the flags if HW specs changed
			ef->m_nMaskGenFX = nMaskGen | nMaskGenHW;
			assert(ef->m_pGenShader == efGen);
		}
	}

	if (ef)
	{
		if (!(flags & EF_RELOAD))
		{
			ef->AddRef();
			ef->m_Flags |= flags;
			return ef;
		}
		else
		{
			ef->mfFree();
			ef->m_Flags |= EF_RELOADED;
		}
	}

	if (!efGen)
	{
		SShaderGen* pShGen = mfCreateShaderGenInfo(nameEf, false);

		if (pShGen)
		{
			efGen = mfNewShader(nameRes);
			efGen->SetRefCounter(0);      // Hack: to avoid leaks in shader-gen's
			efGen->m_NameShader = nameRes;
			efGen->m_ShaderGenParams = pShGen;
		}
	}
	if (!(flags & EF_RELOAD) || !ef)
	{
		if (efGen)
		{
			// Change gen flags based on dependency on resources info
			if (!bGenModified)
			{
				//nMaskGen = gRenDev->EF_GetRemapedShaderMaskGen(nameSh, nMaskGen | nMaskGenHW);
				mfModifyGenFlags(efGen, Res, nMaskGen, nMaskGenHW);
			}
#ifdef __GNUC__
			cry_sprintf(nameNew, "%s(%llx)", nameRes, nMaskGen);
#else
			cry_sprintf(nameNew, "%s(%I64x)", nameRes, nMaskGen);
#endif
			ef = mfNewShader(nameNew);
			if (!ef)
				return s_DefaultShader;

			ef->m_nMaskGenFX = nMaskGen | nMaskGenHW;
			ef->m_pGenShader = efGen;
		}
		if (efGen && ef)
		{
			assert(efGen != ef);
			if (!efGen->m_DerivedShaders)
				efGen->m_DerivedShaders = new std::vector<CShader*>;
			efGen->m_DerivedShaders->push_back(ef);
			efGen->AddRef();
		}
		if (!ef)
		{
			ef = mfNewShader(nameRes);
			if (!ef)
				return s_DefaultShader;
		}
	}
	ef->m_NameShader = nameEf;
	ef->m_NameShaderICRC = nameEfCrc;

	bool bSuccess = false;

	// Check for the new cryFX format
	cry_sprintf(nameNew, "%sCryFX/%s.cfx", m_ShadersPath, nameEf);
	ef->m_NameFile = nameNew;
	ef->m_Flags |= flags;
	
	_smart_ptr<CShader> pShader(ef);
	_smart_ptr<CShaderResources> pResources( const_cast<CShaderResources*>(Res) );
	gRenDev->ExecuteRenderThreadCommand([=] 
		{
			RT_ParseShader(pShader, nMaskGen | nMaskGenHW, flags, pResources);
		}
	);

	return ef;
}

void CShaderMan::CreateShaderExportRequestLine(const CShader* pSH, stack_string& exportString)
{
	if (pSH)
	{
		exportString.Format("<%d>%s/%s(", SHADER_SERIALISE_VER, pSH->GetName(), pSH->GetName());
		CreateShaderMaskGenString(pSH, exportString);
		exportString += ")()(0)(0)(0)(VS)"; //fake normal request line format
	}
}

void CShaderMan::CreateShaderMaskGenString(const CShader* pSH, stack_string& flagString)
{
	uint64 glMask = pSH->m_nMaskGenFX;

	if (glMask)
	{
		SShaderGenComb* c = NULL;
		c = mfGetShaderGenInfo(pSH->GetName());
		if (c && c->pGen && c->pGen->m_BitMask.Num())
		{
			bool bFirstTime = true;
			SShaderGen* pG = c->pGen;
			for (uint32 i = 0; i < 64; i++)
			{
				if (glMask & ((uint64)1 << i))
				{
					for (uint32 j = 0; j < pG->m_BitMask.Num(); j++)
					{
						SShaderGenBit* pBit = pG->m_BitMask[j];
						if (pBit->m_Mask & (glMask & ((uint64)1 << i)))
						{
							if (bFirstTime)
							{
								bFirstTime = false;
							}
							else
							{
								flagString += "|";
							}

							flagString += pBit->m_ParamName.c_str();
							break;
						}
					}
				}
			}
		}
	}
}

void CShaderMan::RT_ParseShader(CShader* pSH, uint64 nMaskGen, uint32 flags, CShaderResources* pRes)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "ParseShader");

	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	bool bSuccess = false;
#ifdef SHADERS_SERIALIZING
	if (CRenderer::CV_r_shadersImport)
	{
		// Do not try and import fxb during cache generation
		// PC would need to support import of console data
		if (!gRenDev->IsShaderCacheGenMode())
		{
			CRY_PROFILE_REGION(PROFILE_RENDERER, "Renderer: ImportShader");
			CRYPROFILE_SCOPE_PROFILE_MARKER("ImportShader");

			bSuccess = ImportShader(pSH, m_Bin);

			if (!bSuccess)
			{
				{
					stack_string flagString;
					CreateShaderMaskGenString(pSH, flagString);
					CryLog("[CShaderSerialize] Failed to import shader %s (0x%p) flags: 0x%llx 0x%x (%s)\n", pSH->GetName(), pSH, pSH->m_nMaskGenFX, pSH->m_nMDV, flagString.empty() ? "0" : flagString.c_str());
				}

				pSH->m_Flags |= EF_FAILED_IMPORT;

				if (CRenderer::CV_r_shadersImport == 2)
				{
					return;
				}
			}
		}
	}
#endif
	if (!bSuccess)
	{
		CRY_PROFILE_REGION(PROFILE_RENDERER, "Renderer: RT_ParseShader");
		CRYPROFILE_SCOPE_PROFILE_MARKER("RT_ParseShader");


		bool nukeCaches = false;

#if !defined(SHADER_NO_SOURCES)
		SShaderBin* pBin = m_Bin.GetBinShader(pSH->m_NameShader, false, 0, &nukeCaches);
		if (pBin)
#endif
		{
#if !defined(SHADER_NO_SOURCES)
			if (flags & EF_FORCE_RELOAD)
			{
				uint32 nCRC32 = pBin->ComputeCRC();
				if (nCRC32 != pBin->m_CRC32)
				{
					FXShaderBinValidCRCItor itor = gRenDev->m_cEF.m_Bin.m_BinValidCRCs.find(pBin->m_dwName);
					if (itor == gRenDev->m_cEF.m_Bin.m_BinValidCRCs.end())
						gRenDev->m_cEF.m_Bin.m_BinValidCRCs.insert(FXShaderBinValidCRCItor::value_type(pBin->m_dwName, false));

					m_Bin.DeleteFromCache(pBin);
					pBin = m_Bin.GetBinShader(pSH->m_NameShader, false, nCRC32, &nukeCaches);
				}
			}
#endif
			bSuccess = m_Bin.ParseBinFX(pBin, pSH, nMaskGen);
#if 0 //def SHADERS_SERIALIZING
			if (CRenderer::CV_r_shadersExport && gRenDev->IsShaderCacheGenMode())
			{
				//Shader compilation must be enabled for export, to allow reading the token table from the fxcbs in the USER dir
				int oldAllowCompilation = CRenderer::CV_r_shadersAllowCompilation;
				CRenderer::CV_r_shadersAllowCompilation = 1;

				if (bSuccess)
				{
					if (!CheckFXBExists(pSH))
					{
						ExportShader(pSH, m_Bin);
					}
					else
					{
						printf("Not exporting shader %s, it already exists\n", pSH->GetName());
					}
				}

				CRenderer::CV_r_shadersAllowCompilation = oldAllowCompilation;
			}
#endif
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "[SHADERS] Failed to load shader '%s'!", pSH->m_NameShader.c_str());
			pSH->m_Flags |= EF_NOTFOUND;
		}

		if (nukeCaches)
		{
			for (auto& pTech : pSH->m_HWTechniques)
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

					for (auto pShader : shaders)
					{
						if (pShader)
							pShader->InvalidateCaches();
					}
				}
			}
		}
	}
	pSH->m_Flags |= EF_LOADED;
}
