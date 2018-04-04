// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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

//==================================================================================
bool CShader::FXSetTechnique(const CCryNameTSCRC& Name)
{
	assert(gRenDev->m_pRT->IsRenderThread());

	uint32 i;
	SShaderTechnique* pTech = NULL;
	for (i = 0; i < m_HWTechniques.Num(); i++)
	{
		pTech = m_HWTechniques[i];
		if (pTech && Name == pTech->m_NameCRC)
			break;
	}

	CRenderer* rd = gRenDev;

	if (i == m_HWTechniques.Num())
	{
		// not found and not set
		rd->m_RP.m_pShader = NULL;
		rd->m_RP.m_nShaderTechnique = -1;
		rd->m_RP.m_pCurTechnique = NULL;
		return false;
	}

	rd->m_RP.m_pShader = this;
	rd->m_RP.m_nShaderTechnique = i;
	rd->m_RP.m_pCurTechnique = m_HWTechniques[i];

	return true;
}

bool CShader::FXSetCSFloat(const CCryNameR& NameParam, const Vec4 fParams[], int nParams)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	if (!pPass)
		return false;
	CHWShader_D3D* curCS = (CHWShader_D3D*)pPass->m_CShader;
	if (!curCS)
		return false;
	SCGBind* pBind = curCS->mfGetParameterBind(NameParam);
	if (!pBind)
		return false;
	curCS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Compute, &fParams[0].x, nParams, curCS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
	return true;
}

bool CShader::FXSetPSFloat(const CCryNameR& NameParam, const Vec4* fParams, int nParams)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	if (!pPass)
		return false;
	CHWShader_D3D* curPS = (CHWShader_D3D*)pPass->m_PShader;
	if (!curPS)
		return false;
	SCGBind* pBind = curPS->mfGetParameterBind(NameParam);
	if (!pBind)
		return false;

	curPS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Pixel, &fParams[0].x, nParams, curPS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
	return true;
}

bool CShader::FXSetVSFloat(const CCryNameR& NameParam, const Vec4* fParams, int nParams)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	if (!pPass)
		return false;
	CHWShader_D3D* curVS = (CHWShader_D3D*)pPass->m_VShader;
	if (!curVS)
		return false;
	SCGBind* pBind = curVS->mfGetParameterBind(NameParam);
	if (!pBind)
		return false;

	curVS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Vertex, &fParams[0].x, nParams, curVS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
	return true;
}

bool CShader::FXSetGSFloat(const CCryNameR& NameParam, const Vec4* fParams, int nParams)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	if (!pPass)
		return false;
	CHWShader_D3D* curGS = (CHWShader_D3D*)pPass->m_GShader;
	if (!curGS)
		return false;
	SCGBind* pBind = curGS->mfGetParameterBind(NameParam);
	if (!pBind)
		return false;
	curGS->mfParameterReg(pBind->m_dwBind, pBind->m_dwCBufSlot, eHWSC_Geometry, &fParams[0].x, nParams, curGS->m_pCurInst->m_nMaxVecs[pBind->m_dwCBufSlot]);
	return true;
}

bool CShader::FXBegin(uint32* uiPassCount, uint32 nFlags)
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique || !rd->m_RP.m_pCurTechnique->m_Passes.Num())
		return false;
	*uiPassCount = rd->m_RP.m_pCurTechnique->m_Passes.Num();
	rd->m_RP.m_nFlagsShaderBegin = nFlags;
	rd->m_RP.m_pCurPass = &rd->m_RP.m_pCurTechnique->m_Passes[0];
	return true;
}

bool CShader::FXBeginPass(uint32 uiPass)
{
	FUNCTION_PROFILER_RENDER_FLAT
	CD3D9Renderer* rd = gcpRendD3D;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique || uiPass >= rd->m_RP.m_pCurTechnique->m_Passes.Num())
		return false;
	rd->m_RP.m_pCurPass = &rd->m_RP.m_pCurTechnique->m_Passes[uiPass];
	SShaderPass* pPass = rd->m_RP.m_pCurPass;
	//assert (pPass->m_VShader && pPass->m_PShader);
	//if (!pPass->m_VShader || !pPass->m_PShader)
	//  return false;
	CHWShader_D3D* curVS = (CHWShader_D3D*)pPass->m_VShader;
	CHWShader_D3D* curPS = (CHWShader_D3D*)pPass->m_PShader;
	CHWShader_D3D* curGS = (CHWShader_D3D*)pPass->m_GShader;
	CHWShader_D3D* curCS = (CHWShader_D3D*)pPass->m_CShader;

	bool bResult = true;

	// Set Pixel-shader and all associated textures
	if (curPS)
	{
		if (rd->m_RP.m_nFlagsShaderBegin & FEF_DONTSETTEXTURES)
			bResult &= curPS->mfSet(0);
		else
			bResult &= curPS->mfSet(HWSF_SETTEXTURES);
		curPS->mfSetParametersPI(NULL, NULL);
	}
	// Set Vertex-shader
	if (curVS)
	{
		curVS->mfSet(0);
		curVS->mfSetParametersPI(NULL, rd->m_RP.m_pShader);
	}

	// Set Geometry-shader
	if (curGS)
		bResult &= curGS->mfSet(0);
	else
		CHWShader_D3D::mfBindGS(NULL, NULL);

	// Set Compute-shader
	if (curCS)
	{
		if (rd->m_RP.m_nFlagsShaderBegin & FEF_DONTSETTEXTURES)
			bResult &= curCS->mfSet(0);
		else
			bResult &= curCS->mfSet(HWSF_SETTEXTURES);
	}
	else
		CHWShader_D3D::mfBindCS(NULL, NULL);

	if (!(rd->m_RP.m_nFlagsShaderBegin & FEF_DONTSETSTATES))
	{
		rd->FX_SetState(pPass->m_RenderState);
		if (pPass->m_eCull != -1)
			rd->D3DSetCull((ECull)pPass->m_eCull);
	}

	return bResult;
}

bool CShader::FXEndPass()
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique || !rd->m_RP.m_pCurTechnique->m_Passes.Num())
		return false;
	rd->m_RP.m_pCurPass = NULL;
	return true;
}

bool CShader::FXEnd()
{
	CRenderer* rd = gRenDev;
	if (!rd->m_RP.m_pShader || !rd->m_RP.m_pCurTechnique)
		return false;
	return true;
}

bool CShader::FXCommit(const uint32 nFlags)
{
	gcpRendD3D->FX_Commit();

	return true;
}

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
	gRenDev->m_cEF.mfRefreshSystemShader("FarTreeSprites", CShaderMan::s_ShaderTreeSprites);
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
	gRenDev->m_cEF.mfRefreshSystemShader("MobileComposition", CShaderMan::s_ShaderMobileComposition);
}

bool CD3D9Renderer::FX_SetFPMode()
{
	assert(gRenDev->m_pRT->IsRenderThread());

	if (!(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & (RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY)) && CShaderMan::s_ShaderFPEmu == m_RP.m_pShader)
		return true;
	if (m_bDeviceLost)
		return false;
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags &= ~RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY;
	m_RP.m_ObjFlags &= ~FOB_TRANS_MASK;
	m_RP.m_pCurObject = m_RP.m_pIdendityRenderObject;
	CShader* pSh = CShaderMan::s_ShaderFPEmu;
	if (!pSh || !pSh->m_HWTechniques.Num())
		return false;
	m_RP.m_FlagsShader_LT = m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurColorOp | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurAlphaOp << 8) | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurColorArg << 16) | (m_RP.m_TI[m_RP.m_nProcessThreadID].m_eCurAlphaArg << 24);
	if (CTexture::s_TexStages[0].m_DevTexture && CTexture::s_TexStages[0].m_DevTexture->IsCube())
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
	else
		m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_CUBEMAP0];

	m_RP.m_pShader = pSh;
	m_RP.m_pCurTechnique = pSh->m_HWTechniques[0];

	bool bRes = pSh->FXBegin(&m_RP.m_nNumRendPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	if (!bRes)
		return false;
	bRes = pSh->FXBeginPass(0);
	FX_Commit();
	return bRes;
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
	if (PObj.size())
	{
		int n = -1;
		for (i = 0; i < PObj.size(); i++)
		{
			if (PObj[i].m_eCGParamType == ECGP_Matr_PI_ViewProj)
			{
				n = i;
				break;
			}
		}
		if (n > 0)
		{
			SCGParam pr = PObj[n];
			PObj[n] = PObj[0];
			PObj[0] = pr;
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
