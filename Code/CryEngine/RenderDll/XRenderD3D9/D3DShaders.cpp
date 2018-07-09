// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DShaders.cpp : Direct3D specific effectors/shaders functions implementation.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "../Common/DevBuffer.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CrySystem/File/IResourceManager.h>

//===================================================================================

void CRenderer::RefreshSystemShaders()
{
	// make sure all system shaders are properly refreshed during loading!
#if defined(FEATURE_SVO_GI)
	gRenDev->m_cEF.mfRefreshSystemShader("Total_Illumination", CShaderMan::s_ShaderSVOGI);
#endif
	gRenDev->m_cEF.mfRefreshSystemShader("Common", CShaderMan::s_ShaderCommon);
	gRenDev->m_cEF.mfRefreshSystemShader("Debug", CShaderMan::s_ShaderDebug);
	gRenDev->m_cEF.mfRefreshSystemShader("DeferredCaustics", CShaderMan::s_ShaderDeferredCaustics);
	gRenDev->m_cEF.mfRefreshSystemShader("DeferredRain", CShaderMan::s_ShaderDeferredRain);
	gRenDev->m_cEF.mfRefreshSystemShader("DeferredSnow", CShaderMan::s_ShaderDeferredSnow);
	gRenDev->m_cEF.mfRefreshSystemShader("DeferredShading", CShaderMan::s_shDeferredShading);
	gRenDev->m_cEF.mfRefreshSystemShader("DepthOfField", CShaderMan::s_shPostDepthOfField);
	gRenDev->m_cEF.mfRefreshSystemShader("DXTCompress", CShaderMan::s_ShaderDXTCompress);
	gRenDev->m_cEF.mfRefreshSystemShader("LensOptics", CShaderMan::s_ShaderLensOptics);
	gRenDev->m_cEF.mfRefreshSystemShader("SoftOcclusionQuery", CShaderMan::s_ShaderSoftOcclusionQuery);
	gRenDev->m_cEF.mfRefreshSystemShader("MotionBlur", CShaderMan::s_shPostMotionBlur);
	gRenDev->m_cEF.mfRefreshSystemShader("OcclusionTest", CShaderMan::s_ShaderOcclTest);
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsRenderModes", CShaderMan::s_shPostEffectsRenderModes);
	gRenDev->m_cEF.mfRefreshSystemShader("ShadowBlur", CShaderMan::s_ShaderShadowBlur);
	gRenDev->m_cEF.mfRefreshSystemShader("Stereo", CShaderMan::s_ShaderStereo);
	gRenDev->m_cEF.mfRefreshSystemShader("Sunshafts", CShaderMan::s_shPostSunShafts);
	gRenDev->m_cEF.mfRefreshSystemShader("Clouds", CShaderMan::s_ShaderClouds);
	gRenDev->m_cEF.mfRefreshSystemShader("GpuParticles", CShaderMan::s_ShaderGpuParticles);
	gRenDev->m_cEF.mfRefreshSystemShader("Stars", CShaderMan::s_ShaderStars);
	gRenDev->m_cEF.mfRefreshSystemShader("MobileComposition", CShaderMan::s_ShaderMobileComposition);
}

void CShaderMan::mfCheckObjectDependParams(std::vector<SCGParam>& PNoObj, std::vector<SCGParam>& PObj, EHWShaderClass eSH, CShader* pFXShader)
{
	if (!PNoObj.size())
		return;
	uint32 i;
	for (i = 0; i < PNoObj.size(); i++)
	{
		SCGParam* prNoObj = &PNoObj[i];
		if ((prNoObj->m_eCGParamType & 0xff) == ECGP_PM_Tweakable)
		{
			int nType = prNoObj->m_eCGParamType & 0xff;
			prNoObj->m_eCGParamType = (ECGParam)nType;
		}

		if (prNoObj->m_Flags & PF_INSTANCE)
		{
			PObj.push_back(PNoObj[i]);
			PNoObj.erase(PNoObj.begin() + i);
			i--;
		}
		else if (prNoObj->m_Flags & PF_MATERIAL)
		{
			PNoObj.erase(PNoObj.begin() + i);
			i--;
		}
	}
}

void CHWShader_D3D::mfLogShaderCacheMiss(SHWSInstance* pInst)
{
	CShaderMan& Man = gRenDev->m_cEF;

	// update the stats
	Man.m_ShaderCacheStats.m_nGlobalShaderCacheMisses++;

	// don't do anything else if CVar is disabled and no callback is registered
	if (CRenderer::CV_r_shaderslogcachemisses == 0 && Man.m_ShaderCacheMissCallback == 0)
		return;

	char nameCache[256];
	cry_strcpy(nameCache, GetName());
	char* s = strchr(nameCache, '(');
	if (s)
		s[0] = 0;
	string sNew;
	SShaderCombIdent Ident = pInst->m_Ident;
	Ident.m_GLMask = m_nMaskGenFX;
	gRenDev->m_cEF.mfInsertNewCombination(Ident, pInst->m_eClass, nameCache, 0, &sNew, 0);

	if (CRenderer::CV_r_shaderslogcachemisses > 1 && !gRenDev->m_bShaderCacheGen && !gEnv->IsEditor())
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "[SHADERS] GCM Global Cache Miss: %s\n", sNew.c_str());
	}

	string sEntry;

	sEntry = "[";
	sEntry += CParserBin::GetPlatformShaderlistName();
	sEntry += "]";
	sEntry += sNew;

	CCryNameTSCRC cryName(sEntry);

	// do we already contain this entry (vec is sorted so lower bound gives us already the good position to insert)
	CShaderMan::ShaderCacheMissesVec::iterator it = std::lower_bound(Man.m_ShaderCacheMisses.begin(), Man.m_ShaderCacheMisses.end(), cryName);
	if (it == Man.m_ShaderCacheMisses.end() || cryName != (*it))
	{
		Man.m_ShaderCacheMisses.insert(it, cryName);

		if (CRenderer::CV_r_shaderslogcachemisses)
		{
			FILE* pFile = fopen(Man.m_ShaderCacheMissPath.c_str(), "a+");
			if (pFile)
			{
				fprintf(pFile, "%s\n", sEntry.c_str());
				fclose(pFile);
			}
		}

		// call callback if provided to inform client about misses
		if (Man.m_ShaderCacheMissCallback)
		{
			(*Man.m_ShaderCacheMissCallback)(sEntry.c_str());
		}
	}

#if ENABLE_STATOSCOPE
	if (gEnv->pStatoscope)
	{
		gEnv->pStatoscope->AddUserMarker("Shaders/Global Cache Misses", sNew.c_str());
	}
#endif
}

void CHWShader_D3D::mfLogShaderRequest(SHWSInstance* pInst)
{
#if !defined(_RELEASE)
	IF(CRenderer::CV_r_shaderssubmitrequestline > 1, 0)
		mfSubmitRequestLine(pInst);
#endif
}
