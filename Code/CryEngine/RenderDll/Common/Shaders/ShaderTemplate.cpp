// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShaderTemplate.cpp : implementation of the Shaders templates support.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CryString/StringUtils.h>                // stristr()
#include <CrySystem/Scaleform/IFlashUI.h>
#include "../Common/Textures/TextureHelpers.h"

#if CRY_PLATFORM_WINDOWS
	#include <direct.h>
	#include <io.h>
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID

#endif

CShaderResources* CShaderMan::mfCreateShaderResources(const SInputShaderResources* Res, bool bShare)
{
	uint32 i, j;
	SInputShaderResources RS = *Res;

	// prepare local resources for cache-check, textures are looked up but not triggered for load
	for (i = 0; i < EFTT_MAX; i++)
	{
		RS.m_Textures[i].m_Sampler.Cleanup();
		if (!RS.m_Textures[i].m_Name.empty())
		{
			RS.m_Textures[i].m_Sampler.m_pTex = mfFindResourceTexture(RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str(), RS.m_Textures[i].m_Sampler.GetTexFlags(), &RS.m_Textures[i]);
			if (RS.m_Textures[i].m_Sampler.m_pTex)
				RS.m_Textures[i].m_Sampler.m_pTex->AddRef();
		}
	}

	// check local resources vs. global cache
	int nFree = -1;
	for (i = 1; i < CShader::s_ShaderResources_known.Num(); i++)
	{
		// not thread safe can be modified from render thread in CShaderResources dtor ()
		// (if flushing of unloaded textures (UnloadLevel) is not complete before pre-loading of new materials)
		CShaderResources* pSR = CShader::s_ShaderResources_known[i];
		if (!pSR)
		{
			nFree = i;
			if (!bShare || Res->m_ShaderParams.size())
				break;
			continue;
		}
		if (!bShare || Res->m_ShaderParams.size())
			continue;

		if (RS.m_ResFlags == pSR->GetResFlags() &&
		    RS.m_LMaterial.m_Opacity == pSR->GetStrengthValue(EFTT_OPACITY) &&
		    RS.m_LMaterial.m_Emittance.a == pSR->GetStrengthValue(EFTT_EMITTANCE) &&
		    RS.m_AlphaRef == pSR->GetAlphaRef() &&
		    !stricmp(RS.m_TexturePath.c_str(), pSR->m_TexturePath.c_str()))
		{
			if ((!pSR->m_pDeformInfo && !RS.m_DeformInfo.m_eType) || (pSR->m_pDeformInfo && *pSR->m_pDeformInfo == RS.m_DeformInfo))
			{
				for (j = 0; j < EFTT_MAX; j++)
				{
					if (!pSR->m_Textures[j] || pSR->m_Textures[j]->m_Name.empty())
					{
						if (RS.m_Textures[j].m_Name.empty())
							continue;
						break;
					}
					else if (RS.m_Textures[j].m_Name.empty())
						break;
					if (RS.m_Textures[j] != *pSR->m_Textures[j])
						break;
				}
				if (j == EFTT_MAX)
				{
					pSR->AddRef();
					return pSR;
				}
			}
		}
	}

	CShaderResources* pSR = new CShaderResources(&RS);
	pSR->AddRef();
	if (!CShader::s_ShaderResources_known.Num())
	{
		CShader::s_ShaderResources_known.AddIndex(1);
		CShaderResources* pSRNULL = new CShaderResources;
		pSRNULL->AddRef();
		CShader::s_ShaderResources_known[0] = pSRNULL;
	}
	else if (nFree < 0 && CShader::s_ShaderResources_known.Num() >= MAX_REND_SHADER_RES)
	{
		Warning("ERROR: CShaderMan::mfCreateShaderResources: MAX_REND_SHADER_RESOURCES hit");
		pSR->Release();
		return CShader::s_ShaderResources_known[1];
	}
	if (nFree > 0)
	{
		pSR->m_Id = nFree;
		pSR->m_IdGroup = pSR->m_Id;
		CShader::s_ShaderResources_known[nFree] = pSR;
	}
	else
	{
		pSR->m_Id = CShader::s_ShaderResources_known.Num();
		pSR->m_IdGroup = pSR->m_Id;
		CShader::s_ShaderResources_known.AddElem(pSR);
	}

	return pSR;
}

uint32 SShaderItem::PostLoad()
{
	uint32 nPreprocessFlags = 0;
	CShader* pSH = (CShader*)m_pShader;
	CShaderResources* pR = (CShaderResources*)m_pShaderResources;

	if (pSH->m_Flags2 & EF2_PREPR_SCANWATER)
		nPreprocessFlags |= FSPR_SCANTEXWATER | FB_PREPROCESS;

	nPreprocessFlags |= FB_GENERAL;
	const SShaderTechnique* pTech = GetTechnique();
	if (pR)
	{
		pR->PostLoad(pSH);
		for (int i = 0; i < EFTT_MAX; i++)
		{
			if (!pR->m_Textures[i] || !pR->m_Textures[i]->m_Sampler.m_pTarget)
				continue;

			if (gRenDev->EF_GetRenderQuality() == eRQ_Low)
			{
				STexSamplerRT& sampler(pR->m_Textures[i]->m_Sampler);
				if (sampler.m_eTexType == eTT_Auto2D || sampler.m_eTexType == eTT_Dyn2D)
					sampler.m_eTexType = eTT_2D;
			}

			if (pR->m_Textures[i]->m_Sampler.m_eTexType == eTT_Auto2D || pR->m_Textures[i]->m_Sampler.m_eTexType == eTT_Dyn2D)
				pR->m_ResFlags |= MTL_FLAG_NOTINSTANCED;
			pR->m_RTargets.AddElem(pR->m_Textures[i]->m_Sampler.m_pTarget);
			if (pR->m_Textures[i]->m_Sampler.m_eTexType == eTT_Auto2D || pR->m_Textures[i]->m_Sampler.m_eTexType == eTT_Dyn2D)
				nPreprocessFlags |= FSPR_SCANTEX;
		}
	}
	if (pTech && pTech->m_Passes.Num() && (pTech->m_Passes[0].m_RenderState & GS_ALPHATEST))
	{
		if (pR && !pR->m_AlphaRef)
			pR->m_AlphaRef = 0.5f;
	}

	// Update persistent batch flags
	if (pTech)
	{
		if (pTech->m_nTechnique[TTYPE_Z] > 0)
		{
			nPreprocessFlags |= FB_Z;

			// ZPrepass only for non-alpha tested/blended geometry (decals, terrain).
			// We assume vegetation is special case due to potential massive overdraw
			if (pTech->m_nTechnique[TTYPE_ZPREPASS] > 0)
			{
				const bool bAlphaTested = (pR && pR->IsAlphaTested());
				const bool bVegetation = (pSH && pSH->GetShaderType() == eST_Vegetation);
				if (!bAlphaTested && !bVegetation || bAlphaTested && bVegetation)
					nPreprocessFlags |= FB_ZPREPASS;
			}
		}

		if (!(pTech->m_Flags & FHF_POSITION_INVARIANT) && ((pTech->m_Flags & FHF_TRANSPARENT) || (pR && pR->IsTransparent())))
			nPreprocessFlags |= FB_TRANSPARENT;

		if (pTech->m_nTechnique[TTYPE_WATERREFLPASS] > 0)
			nPreprocessFlags |= FB_WATER_REFL;

		if (pTech->m_nTechnique[TTYPE_WATERCAUSTICPASS] > 0)
			nPreprocessFlags |= FB_WATER_CAUSTIC;

		if ((pSH->m_Flags2 & EF2_SKINPASS))
			nPreprocessFlags |= FB_SKIN;

		if (pSH->m_Flags2 & EF2_EYE_OVERLAY)
			nPreprocessFlags |= FB_EYE_OVERLAY;

		if ((pSH->m_Flags & EF_SUPPORTSDEFERREDSHADING_MIXED) && !(pSH->m_Flags & EF_SUPPORTSDEFERREDSHADING_FULL))
			nPreprocessFlags |= FB_TILED_FORWARD;

		if (CRenderer::CV_r_Refraction && (pSH->m_Flags & EF_REFRACTIVE))
			nPreprocessFlags |= FB_TRANSPARENT;

		if (pTech->m_nTechnique[TTYPE_EFFECTLAYER] > 0 && pR && pR->m_Textures[EFTT_CUSTOM] && pR->m_Textures[EFTT_CUSTOM_SECONDARY])
			nPreprocessFlags |= FB_LAYER_EFFECT;
	}

	if (pTech)
		nPreprocessFlags |= pTech->m_nPreprocessFlags;

	if (nPreprocessFlags & FSPR_MASK)
		nPreprocessFlags |= FB_PREPROCESS;

	return nPreprocessFlags;
}

void CShaderResources::SetShaderParams(SInputShaderResources* pDst, IShader* pSH)
{
	ReleaseParams();
	m_ShaderParams = pDst->m_ShaderParams;

	UpdateConstants(pSH);
}

EEfResTextures CShaderMan::mfCheckTextureSlotName(const char* mapname)
{
	EEfResTextures slot = EFTT_UNKNOWN;

	if (!stricmp(mapname, "$Diffuse"))            slot = EFTT_DIFFUSE;
	else if (!strnicmp(mapname, "$Normals", 7))
		slot = EFTT_NORMALS;
	else if (!stricmp(mapname, "$Specular"))
		slot = EFTT_SPECULAR;
	else if (!strnicmp(mapname, "$Env", 4))
		slot = EFTT_ENV;
	else if (!stricmp(mapname, "$Detail"))
		slot = EFTT_DETAIL_OVERLAY;
	else if (!strnicmp(mapname, "$Translucency", 12))
		slot = EFTT_TRANSLUCENCY;
	else if (!strnicmp(mapname, "$Height", 6))
		slot = EFTT_HEIGHT;
	else if (!stricmp(mapname, "$DecalOverlay"))
		slot = EFTT_DECAL_OVERLAY;
	else if (!strnicmp(mapname, "$Subsurface", 11))
		slot = EFTT_SUBSURFACE;
	else if (!stricmp(mapname, "$CustomMap"))
		slot = EFTT_CUSTOM;
	else if (!stricmp(mapname, "$CustomSecondaryMap"))
		slot = EFTT_CUSTOM_SECONDARY;
	else if (!stricmp(mapname, "$Opacity"))
		slot = EFTT_OPACITY;
	else if (!stricmp(mapname, "$Smoothness"))
		slot = EFTT_SMOOTHNESS;
	else if (!stricmp(mapname, "$Emittance"))
		slot = EFTT_EMITTANCE;

	// backwards compatible names
	else if (!stricmp(mapname, "$Cubemap"))
		slot = EFTT_ENV;
	else if (!strnicmp(mapname, "$BumpDiffuse", 12))
		slot = EFTT_TRANSLUCENCY;                                                  //EFTT_BUMP_DIFFUSE;
	else if (!strnicmp(mapname, "$BumpHeight", 10))
		slot = EFTT_HEIGHT;                                                  //EFTT_BUMP_HEIGHT;
	else if (!strnicmp(mapname, "$Bump", 5))
		slot = EFTT_NORMALS;                                                  //EFTT_BUMP;
	else if (!stricmp(mapname, "$Gloss"))
		slot = EFTT_SPECULAR;                                                  //EFTT_GLOSS;
	else if (!stricmp(mapname, "$GlossNormalA"))
		slot = EFTT_SMOOTHNESS;                                                  //EFTT_GLOSS_NORMAL_A;

	return slot;
}

//=================================================================================================

CTexture* CShaderMan::mfCheckTemplateTexName(const char* mapname, ETEX_Type eTT)
{
	CTexture* TexPic = NULL;
	if (mapname[0] != '$')
		return NULL;

	{
		EEfResTextures slot = mfCheckTextureSlotName(mapname);

		if (slot != EFTT_MAX)
			return &CRendererResources::s_ShaderTemplates[slot];
	}

	if (!stricmp(mapname, "$ShadowPoolAtlas"))
		TexPic = CRendererResources::s_ptexRT_ShadowPool;
	else if (!strnicmp(mapname, "$ShadowID", 9))
	{
		int n = atoi(&mapname[9]);
		TexPic = CRendererResources::s_ptexShadowID[n];
	}
	else if (!stricmp(mapname, "$FromRE") || !stricmp(mapname, "$FromRE0"))
		TexPic = CRendererResources::s_ptexFromRE[0];
	else if (!stricmp(mapname, "$FromRE1"))
		TexPic = CRendererResources::s_ptexFromRE[1];
	else if (!stricmp(mapname, "$FromRE2"))
		TexPic = CRendererResources::s_ptexFromRE[2];
	else if (!stricmp(mapname, "$FromRE3"))
		TexPic = CRendererResources::s_ptexFromRE[3];
	else if (!stricmp(mapname, "$FromRE4"))
		TexPic = CRendererResources::s_ptexFromRE[4];
	else if (!stricmp(mapname, "$FromRE5"))
		TexPic = CRendererResources::s_ptexFromRE[5];
	else if (!stricmp(mapname, "$FromRE6"))
		TexPic = CRendererResources::s_ptexFromRE[6];
	else if (!stricmp(mapname, "$FromRE7"))
		TexPic = CRendererResources::s_ptexFromRE[7];
	else if (!stricmp(mapname, "$ColorChart"))
		TexPic = CRendererResources::s_ptexColorChart;
	else if (!stricmp(mapname, "$FromObj"))
		TexPic = CRendererResources::s_ptexFromObj;
	else if (!stricmp(mapname, "$FromObjCM")) // NOTE: deprecated
		TexPic = CRendererResources::s_ptexBlackCM;
	else if (!strnicmp(mapname, "$White", 6))
		TexPic = CRendererResources::s_ptexWhite;
	else if (!strnicmp(mapname, "$RT_2D", 6))
	{
		TexPic = CRendererResources::s_ptexRT_2D;
	}
	else if (!stricmp(mapname, "$PrevFrameScaled"))
		TexPic = CRendererResources::s_ptexPrevFrameScaled;
	else if (!stricmp(mapname, "$BackBuffer"))
		TexPic = CRendererResources::s_ptexBackBuffer;
	else if (!stricmp(mapname, "$ModelHUD"))
		TexPic = CRendererResources::s_ptexModelHudBuffer;
	else if (!stricmp(mapname, "$BackBufferScaled_d2"))
		TexPic = CRendererResources::s_ptexBackBufferScaled[0];
	else if (!stricmp(mapname, "$BackBufferScaled_d4"))
		TexPic = CRendererResources::s_ptexBackBufferScaled[1];
	else if (!stricmp(mapname, "$BackBufferScaled_d8"))
		TexPic = CRendererResources::s_ptexBackBufferScaled[2];
	else if (!stricmp(mapname, "$HDR_BackBuffer"))
		TexPic = CRendererResources::s_ptexSceneTarget;
	else if (!stricmp(mapname, "$HDR_BackBufferScaled_d2"))
		TexPic = CRendererResources::s_ptexHDRTargetScaled[0];
	else if (!stricmp(mapname, "$HDR_BackBufferScaled_d4"))
		TexPic = CRendererResources::s_ptexHDRTargetScaled[1];
	else if (!stricmp(mapname, "$HDR_BackBufferScaled_d8"))
		TexPic = CRendererResources::s_ptexHDRTargetScaled[2];
	else if (!stricmp(mapname, "$HDR_FinalBloom"))
		TexPic = CRendererResources::s_ptexHDRFinalBloom;
	else if (!stricmp(mapname, "$HDR_TargetPrev"))
		TexPic = CRendererResources::s_ptexHDRTargetPrev;
	else if (!stricmp(mapname, "$HDR_AverageLuminance"))
		TexPic = CRendererResources::s_ptexHDRMeasuredLuminanceDummy;
	else if (!stricmp(mapname, "$ZTarget"))
		TexPic = CRendererResources::s_ptexLinearDepth;
	else if (!stricmp(mapname, "$ZTargetScaled"))
		TexPic = CRendererResources::s_ptexLinearDepthScaled[0];
	else if (!stricmp(mapname, "$ZTargetScaled2"))
		TexPic = CRendererResources::s_ptexLinearDepthScaled[1];
	else if (!stricmp(mapname, "$SceneTarget"))
		TexPic = CRendererResources::s_ptexSceneTarget;
	else if (!stricmp(mapname, "$CloudsLM"))
		TexPic = CRendererResources::s_ptexCloudsLM;
	else if (!stricmp(mapname, "$WaterVolumeDDN"))
		TexPic = CRendererResources::s_ptexWaterVolumeDDN;
	else if (!stricmp(mapname, "$WaterVolumeReflPrev"))
		TexPic = CRendererResources::s_ptexWaterVolumeRefl[1];
	else if (!stricmp(mapname, "$WaterVolumeRefl"))
		TexPic = CRendererResources::s_ptexWaterVolumeRefl[0];
	else if (!stricmp(mapname, "$WaterVolumeCaustics"))
		TexPic = CRendererResources::s_ptexWaterCaustics[0];
	else if (!stricmp(mapname, "$WaterVolumeCausticsTemp"))
		TexPic = CRendererResources::s_ptexWaterCaustics[1];
	else if (!stricmp(mapname, "$SceneNormalsMap"))
		TexPic = CRendererResources::s_ptexSceneNormalsMap;
	else if (!stricmp(mapname, "$SceneDiffuse"))
		TexPic = CRendererResources::s_ptexSceneDiffuse;
	else if (!stricmp(mapname, "$SceneSpecular"))
		TexPic = CRendererResources::s_ptexSceneSpecular;
	else if (!stricmp(mapname, "$SceneNormalsBent"))
		TexPic = CRendererResources::s_ptexSceneNormalsBent;
	else if (!stricmp(mapname, "$VolCloudShadows"))
		TexPic = CRendererResources::s_ptexVolCloudShadow;

	return TexPic;
}

const char* CShaderMan::mfTemplateTexIdToName(int Id)
{
	switch (Id)
	{
	case EFTT_DIFFUSE:
		return "Diffuse";
	case EFTT_SPECULAR:
		return "Gloss";
	case EFTT_NORMALS:
		return "Bump";
	case EFTT_ENV:
		return "Environment";
	case EFTT_SUBSURFACE:
		return "SubSurface";
	case EFTT_CUSTOM:
		return "CustomMap";
	case EFTT_CUSTOM_SECONDARY:
		return "CustomSecondaryMap";
	case EFTT_DETAIL_OVERLAY:
		return "Detail";
	case EFTT_OPACITY:
		return "Opacity";
	case EFTT_DECAL_OVERLAY:
		return "Decal";
	case EFTT_SMOOTHNESS:
		return "GlossNormalA";
	case EFTT_EMITTANCE:
		return "Emittance";
	default:
		return "Unknown";
	}
	return "Unknown";
}

STexAnim* CShaderMan::mfReadTexSequence(const char* na, int Flags, bool bFindOnly)
{
	char prefix[_MAX_PATH];
	char postfix[_MAX_PATH];
	char* nm;
	int i, j, l, m;
	char nam[_MAX_PATH];
	int n;
	CTexture* tx, * tp;
	int startn, endn, nums;
	char name[_MAX_PATH];

	tx = NULL;

	cry_strcpy(name, na);
	const char* ext = PathUtil::GetExt(na);
	PathUtil::RemoveExtension(name);

	char chSep = '#';
	nm = strchr(name, chSep);
	if (!nm)
	{
		nm = strchr(name, '$');
		if (!nm)
			return 0;
		chSep = '$';
	}

	float fSpeed = 0.05f;
	{
		char nName[_MAX_PATH];
		strcpy(nName, name);
		nm = strchr(nName, '(');
		if (nm)
		{
			name[nm - nName] = 0;
			char* speed = &nName[nm - nName + 1];
			if (nm = strchr(speed, ')'))
				speed[nm - speed] = 0;
			fSpeed = (float)atof(speed);
		}
	}

	j = 0;
	n = 0;
	l = -1;
	m = -1;
	while (name[n])
	{
		if (name[n] == chSep)
		{
			j++;
			if (l == -1)
				l = n;
		}
		else if (l >= 0 && m < 0)
			m = n;
		n++;
	}
	if (!j)
		return 0;

	cry_strcpy(prefix, name, l);

	char dig[_MAX_PATH];
	l = 0;
	if (m < 0)
	{
		startn = 0;
		endn = 999;
		postfix[0] = 0;
	}
	else
	{
		while (isdigit((unsigned char)name[m]))
		{
			dig[l++] = name[m];
			m++;
		}
		dig[l] = 0;
		startn = strtol(dig, NULL, 10);
		m++;

		l = 0;
		while (isdigit((unsigned char)name[m]))
		{
			dig[l++] = name[m];
			m++;
		}
		dig[l] = 0;
		endn = strtol(dig, NULL, 10);

		strcpy(postfix, &name[m]);
	}

	nums = endn - startn + 1;

	n = 0;
	char frm[256];
	char frd[4];

	frd[0] = j + '0';
	frd[1] = 0;

	cry_strcpy(frm, "%s%.");
	cry_strcat(frm, frd);
	cry_strcat(frm, "d%s.%s");
	STexAnim* ta = NULL;
	for (i = 0; i < nums; i++)
	{
		sprintf(nam, frm, prefix, startn + i, postfix, ext);
		// TODO: add support for animated sequences to the texture streaming
		tp = (CTexture*)gRenDev->EF_LoadTexture(nam, Flags | FT_DONT_STREAM);
		if (!tp || !tp->IsLoaded())
		{
			if (tp)
				tp->Release();
			break;
		}
		if (!ta)
		{
			ta = new STexAnim;
			ta->m_bLoop = true;
			ta->m_Time = fSpeed;
		}

		ITexture* pTex = (ITexture*)tp;
		ta->m_TexPics.AddElem(tp);
		n++;
	}

	if (ta)
	{
		ta->m_NumAnimTexs = ta->m_TexPics.Num();
	}

	return ta;
}

int CShaderMan::mfReadTexSequence(STexSamplerRT* smp, const char* na, int Flags, bool bFindOnly)
{
	STexAnim* ta;

	if ((ta = smp->m_pAnimInfo) ||
		(ta = mfReadTexSequence(na, Flags, bFindOnly)))
	{
		smp->m_pAnimInfo = ta;

		// Release texture which has been set previously
		SAFE_RELEASE(smp->m_pTex);

		// Set initial texture so that subsequent checks understand that it has been loaded
		smp->m_pTex = ta->m_TexPics[0];
		smp->m_pTex->AddRef();

		return ta->m_NumAnimTexs;
	}

	return 0;
}

void CShaderMan::mfSetResourceTexState(SEfResTexture* Tex)
{
	if (Tex)
	{
		SSamplerState ST;
		
		ST.SetFilterMode(Tex->m_Filter);
		ST.SetClampMode(Tex->m_bUTile ? eSamplerAddressMode_Wrap : eSamplerAddressMode_Clamp, Tex->m_bVTile ? eSamplerAddressMode_Wrap : eSamplerAddressMode_Clamp, Tex->m_bUTile ? eSamplerAddressMode_Wrap : eSamplerAddressMode_Clamp);

		Tex->m_Sampler.m_nTexState = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(ST);
	}
}

CTexture* CShaderMan::mfTryToLoadTexture(const char* nameTex, STexSamplerRT* smp, int Flags, bool bFindOnly)
{
	if (nameTex && strchr(nameTex, '#')) // test for " #" to skip max material names
	{
		int n = mfReadTexSequence(smp, nameTex, Flags, bFindOnly);
	}

	CTexture* tx = smp->m_pTex;

	if (!tx)
	{
		if (bFindOnly)
		{
			tx = (CTexture*)gRenDev->EF_GetTextureByName(nameTex, Flags);
		}
		else
		{
			tx = (CTexture*)gRenDev->EF_LoadTexture(nameTex, Flags);
		}
	}

	return tx;
}

void CShaderMan::mfRefreshResourceTextureConstants(EEfResTextures Id, SInputShaderResources& RS)
{
	RS.m_LMaterial.m_Channels[Id][0] = RS.m_Textures[Id].m_Sampler.m_pTex->GetMinColor();
	RS.m_LMaterial.m_Channels[Id][1] = RS.m_Textures[Id].m_Sampler.m_pTex->GetMaxColor() - RS.m_Textures[Id].m_Sampler.m_pTex->GetMinColor();
}

void CShaderMan::mfRefreshResourceTextureConstants(EEfResTextures Id, CShaderResources& RS)
{
	RS.m_Constants[REG_PM_CHANNELS_SB + Id * 2 + 0] = RS.m_Textures[Id]->m_Sampler.m_pTex->GetMinColor().toVec4();
	RS.m_Constants[REG_PM_CHANNELS_SB + Id * 2 + 1] = RS.m_Textures[Id]->m_Sampler.m_pTex->GetMaxColor().toVec4() - RS.m_Textures[Id]->m_Sampler.m_pTex->GetMinColor().toVec4();
}

CTexture* CShaderMan::mfFindResourceTexture(const char* nameTex, const char* path, int Flags, SEfResTexture* Tex)
{
	mfSetResourceTexState(Tex);

	return mfTryToLoadTexture(nameTex, Tex ? &Tex->m_Sampler : NULL, Flags, true);
}

CTexture* CShaderMan::mfLoadResourceTexture(const char* nameTex, const char* path, int Flags, SEfResTexture* Tex)
{
	mfSetResourceTexState(Tex);

	return mfTryToLoadTexture(nameTex, Tex ? &Tex->m_Sampler : NULL, Flags, false);
}

bool CShaderMan::mfLoadResourceTexture(EEfResTextures Id, SInputShaderResources& RS, uint32 CustomFlags, bool bReplaceMeOnFail)
{
	bool bReturn = false;

	if (!bReturn && !RS.m_Textures[Id].m_Name.empty())
	{
		if (!RS.m_Textures[Id].m_Sampler.m_pTex || !RS.m_Textures[Id].m_Sampler.m_pTex->IsTextureLoaded())
		{
			RS.m_Textures[Id].m_Sampler.m_pTex = mfLoadResourceTexture(RS.m_Textures[Id].m_Name.c_str(), RS.m_TexturePath.c_str(), RS.m_Textures[Id].m_Sampler.GetTexFlags() | CustomFlags, &RS.m_Textures[Id]);
		}

		if (!(bReturn = RS.m_Textures[Id].m_Sampler.m_pTex->IsTextureLoaded()) && bReplaceMeOnFail)
		{
			RS.m_Textures[Id].m_Sampler.m_pTex = mfLoadResourceTexture("%ENGINE%/EngineAssets/TextureMsg/ReplaceMe.tif", RS.m_TexturePath.c_str(), RS.m_Textures[Id].m_Sampler.GetTexFlags() | CustomFlags, &RS.m_Textures[Id]);
		}
	}

	if (bReturn)
	{
		mfRefreshResourceTextureConstants(Id, RS);
	}

	return bReturn;
}

bool CShaderMan::mfLoadResourceTexture(EEfResTextures Id, CShaderResources& RS, uint32 CustomFlags, bool bReplaceMeOnFail)
{
	bool bTextureLoaded = RS.m_Textures[Id]->m_Sampler.m_pTex && RS.m_Textures[Id]->m_Sampler.m_pTex->IsTextureLoaded();

	if (!RS.m_Textures[Id]->m_Name.empty())
	{
		if (!bTextureLoaded || (CustomFlags & FT_ALPHA))
		{
			SAFE_RELEASE(RS.m_Textures[Id]->m_Sampler.m_pTex);
			RS.m_Textures[Id]->m_Sampler.m_pTex = mfLoadResourceTexture(RS.m_Textures[Id]->m_Name.c_str(), RS.m_TexturePath.c_str(), RS.m_Textures[Id]->m_Sampler.GetTexFlags() | CustomFlags, RS.m_Textures[Id]);
		}

		if (!(bTextureLoaded = RS.m_Textures[Id]->m_Sampler.m_pTex->IsTextureLoaded()) && bReplaceMeOnFail)
		{
			RS.m_Textures[Id]->m_Sampler.m_pTex = mfLoadResourceTexture("%ENGINE%/EngineAssets/TextureMsg/ReplaceMe.tif", RS.m_TexturePath.c_str(), RS.m_Textures[Id]->m_Sampler.GetTexFlags() | CustomFlags, RS.m_Textures[Id]);
		}
	}

	if (bTextureLoaded)
	{
		mfRefreshResourceTextureConstants(Id, RS);
	}

	return bTextureLoaded;
}

void CShaderMan::mfLoadDefaultTexture(EEfResTextures Id, CShaderResources& RS, EEfResTextures Def)
{
	bool bReturn = false;

	if (!bReturn)
	{
		RS.m_Textures[Id]->m_Sampler.m_pTex = TextureHelpers::LookupTexDefault(Def);

		if (!(bReturn = RS.m_Textures[Id]->m_Sampler.m_pTex->IsTextureLoaded()))
		{
		}
	}

	if (bReturn)
	{
		mfRefreshResourceTextureConstants(Id, RS);
	}
}

bool CShaderMan::mfRefreshResourceConstants(CShaderResources* Res)
{
	bool bChanged = false;

	if (Res)
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			if (!Res->m_Textures[i] || !Res->m_Textures[i]->m_Sampler.m_pTex)
				continue;

			mfRefreshResourceTextureConstants((EEfResTextures)i, *Res);

			bChanged = true;
		}
	}

	return bChanged;
}

void CShaderMan::mfRefreshResources(CShaderResources* Res, const IRenderer::SLoadShaderItemArgs* pArgs)
{
	if (Res)
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			int Flags = 0;
			if (i == EFTT_NORMALS)
			{
				if ((!Res->m_Textures[i] || Res->m_Textures[i]->m_Name.empty()))
					continue;

				Flags |= FT_TEX_NORMAL_MAP;
				if (!Res->m_Textures[i])
					Res->AddTextureMap(i);

				if (!mfLoadResourceTexture((EEfResTextures)i, *Res, Flags))
					mfLoadDefaultTexture((EEfResTextures)i, *Res, (EEfResTextures)i);

				// Support for gloss in regular normal map
				CTexture* pTexN = (Res->m_Textures[i] ? Res->m_Textures[i]->m_Sampler.m_pTex : NULL);
				if (pTexN && (pTexN->GetFlags() & FT_HAS_ATTACHED_ALPHA))
				{
					if (!Res->m_Textures[EFTT_SMOOTHNESS])
						Res->AddTextureMap(EFTT_SMOOTHNESS);

					Res->m_Textures[EFTT_SMOOTHNESS]->m_Name = pTexN->GetSourceName();
					if (!mfLoadResourceTexture(EFTT_SMOOTHNESS, *Res, Flags | FT_ALPHA))
						mfLoadDefaultTexture(EFTT_SMOOTHNESS, *Res, (EEfResTextures)i);
				}

				continue;
			}
			else if (i == EFTT_HEIGHT)
			{
				if (!Res->m_Textures[EFTT_NORMALS] || !Res->m_Textures[EFTT_NORMALS]->m_Sampler.m_pTex)
					continue;
				if (!Res->m_Textures[i])
					continue; //Res->AddTextureMap(i);

				mfLoadResourceTexture((EEfResTextures)i, *Res, Flags);
			}
			else if (i == EFTT_CUSTOM_SECONDARY)
			{
				if ((!Res->m_Textures[i] || Res->m_Textures[i]->m_Name.empty()))
					continue;

				if (!Res->m_Textures[i])
					Res->AddTextureMap(i);

				if (!mfLoadResourceTexture((EEfResTextures)i, *Res, Flags))
					mfLoadDefaultTexture((EEfResTextures)i, *Res, (EEfResTextures)i);

				// Support for gloss in blend layer normal map
				CTexture* pTexN = (Res->m_Textures[i] ? Res->m_Textures[i]->m_Sampler.m_pTex : NULL);
				if (pTexN && (pTexN->GetFlags() & FT_HAS_ATTACHED_ALPHA))
				{
					if (!Res->m_Textures[EFTT_DECAL_OVERLAY])
						Res->AddTextureMap(EFTT_DECAL_OVERLAY);

					Res->m_Textures[EFTT_DECAL_OVERLAY]->m_Name = pTexN->GetSourceName();
					if (!mfLoadResourceTexture(EFTT_DECAL_OVERLAY, *Res, Flags | FT_ALPHA))
						mfLoadDefaultTexture(EFTT_DECAL_OVERLAY, *Res, EFTT_SMOOTHNESS);
				}

				continue;
			}

			SEfResTexture* Tex = Res->m_Textures[i];
			if (!Tex)
				continue;

			// TODO: fix this bug at the root, a texture is allocated even though "nearest_cubemap"-named textures should be NULL
			if (Tex->m_Sampler.m_eTexType == eTT_NearestCube)
			{
				SAFE_RELEASE(Tex->m_Sampler.m_pTex);
				Tex->m_Sampler.m_pTex = CRendererResources::s_ptexBlackCM;
			}

			if (Tex->m_Sampler.m_eTexType == eTT_Dyn2D)
			{
				SAFE_RELEASE(Tex->m_Sampler.m_pTex);

				if (CFlashTextureSource::IsFlashFile(Tex->m_Name.c_str()))
				{
					mfSetResourceTexState(Tex);

					SAFE_RELEASE(Tex->m_Sampler.m_pTarget);
					Tex->m_Sampler.m_pTarget = new SHRenderTarget;

					const bool defVal = CRenderer::CV_r_DynTexSourceUseSharedRT == 1;
					bool useSharedRT = gEnv->pFlashUI ? gEnv->pFlashUI->UseSharedRT(Tex->m_Name.c_str(), defVal) : defVal;
					if (useSharedRT)
						Tex->m_Sampler.m_pDynTexSource = new CFlashTextureSourceSharedRT(Tex->m_Name.c_str(), pArgs);
					else
						Tex->m_Sampler.m_pDynTexSource = new CFlashTextureSource(Tex->m_Name.c_str(), pArgs);

					Tex->m_Sampler.m_pTarget->m_refSamplerID = i;
					Tex->m_Sampler.m_pTarget->m_bTempDepth = true;
					Tex->m_Sampler.m_pTarget->m_eOrder = eRO_Managed;
					Tex->m_Sampler.m_pTarget->m_eTF = eTF_R8G8B8A8;
					Tex->m_Sampler.m_pTarget->m_nIDInPool = -1;
					Tex->m_Sampler.m_pTarget->m_nFlags |= FRT_CLEAR_DEPTH | FRT_CLEAR_STENCIL | FRT_CLEAR_COLOR;
				}
				else
				{
					Tex->m_Sampler.m_pTex = mfLoadResourceTexture("%ENGINE%/EngineAssets/TextureMsg/NotFound.tif", Res->m_TexturePath.c_str(), Res->m_Textures[i]->m_Sampler.GetTexFlags() | Flags, Res->m_Textures[i]);
				}
			}
			else if (!Tex->m_Sampler.m_pTex)
			{
				if (Tex->m_Sampler.m_eTexType == eTT_Auto2D)
				{
					if (i == EFTT_ENV)
					{
						mfSetResourceTexState(Tex);

						SAFE_RELEASE(Tex->m_Sampler.m_pTarget);
						Tex->m_Sampler.m_pTarget = new SHRenderTarget;

						Tex->m_Sampler.m_pTex = CRendererResources::s_ptexRT_2D;
						Tex->m_Sampler.m_pTarget->m_pTarget = CRendererResources::s_ptexRT_2D;

						Tex->m_Sampler.m_pTarget->m_bTempDepth = true;
						Tex->m_Sampler.m_pTarget->m_eOrder = eRO_PreProcess;
						Tex->m_Sampler.m_pTarget->m_eTF = CRendererResources::GetHDRFormat(false, true);
						Tex->m_Sampler.m_pTarget->m_nIDInPool = -1;
						Tex->m_Sampler.m_pTarget->m_nFlags |= FRT_RENDTYPE_RECURSIVECURSCENE | FRT_CAMERA_CURRENT;
						Tex->m_Sampler.m_pTarget->m_nFlags |= FRT_CLEAR_DEPTH | FRT_CLEAR_STENCIL | FRT_CLEAR_COLOR;
					}
				}
				else if (Tex->m_Sampler.m_eTexType == eTT_User)
					Tex->m_Sampler.m_pTex = nullptr;
				else
				{
					mfLoadResourceTexture((EEfResTextures)i, *Res, Flags);
				}
			}

			// assign streaming priority based on the importance (sampler slot)
			if (Tex && Tex->m_Sampler.m_pITex && Tex->m_Sampler.m_pTex->IsTextureLoaded() && Tex->m_Sampler.m_pTex->IsStreamed())
			{
				CTexture* tp = Tex->m_Sampler.m_pTex;
				tp->SetStreamingPriority(EFTT_MAX - i);
			}
		}
	}

	mfRefreshResourceConstants(Res);
}
