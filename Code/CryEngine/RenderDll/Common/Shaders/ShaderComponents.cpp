// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShaderComponents.cpp : FX Shaders semantic components implementation.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include "../Shadow_Renderer.h"

#if CRY_PLATFORM_WINDOWS
	#include <direct.h>
	#include <io.h>
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
#endif

static void sParseTimeExpr(const char* szScr, const char* szAnnotations, SCGParam* vpp, int nComp, CShader* ef)
{
	if (szScr)
	{
		if (!vpp->m_pData)
			vpp->m_pData = new SParamData;
		char str[256];
		float f;
		int n = sscanf(szScr, "%255s %f", str, &f);
		if (n == 2)
			vpp->m_pData->d.fData[nComp] = f;
		else
			vpp->m_pData->d.fData[nComp] = 1.0f;
	}
}

static void sParseRuntimeShaderFlag(const char* szScr, const char* szAnnotations, SCGParam* vpp, int nComp, CShader* ef)
{
	if (szScr)
	{
		if (!vpp->m_pData)
			vpp->m_pData = new SParamData;
		char str[256], strval[256];
		str[0] = '\0';
		strval[0] = '\0';
		uint32 n = sscanf(szScr, "%255s %255s", str, strval);
		if (n == 2)
		{
			bool bOK = false;
			if (strval[0] == '%')
			{
				SShaderGen* pGen = gRenDev->m_cEF.m_pGlobalExt;
				for (n = 0; n < pGen->m_BitMask.Num(); n++)
				{
					SShaderGenBit* pBit = pGen->m_BitMask[n];
					if (!strcmp(pBit->m_ParamName.c_str(), strval))
					{
						vpp->m_pData->d.nData64[nComp] = pBit->m_Mask;
						break;
					}
				}
				if (n == pGen->m_BitMask.Num())
				{
					iLog->Log("Error: Couldn't find runtime shader flag '%s' for shader '%s'", strval, ef->GetName());
					vpp->m_pData->d.nData64[nComp] = 0;
				}
			}
			else
				vpp->m_pData->d.nData64[nComp] = shGetHex64(strval);
		}
		else
			vpp->m_pData->d.nData64[nComp] = 0;
	}
	else
	{
		assert(0);
	}
}

//======================================================================================

// GL = global
// PB = Per-Batch
// PI = Per-Instance
// SI = Per-Instance Static
// PF = Per-Frame
// PM = Per-Material
// SK = Skin data
// MP = Morph data

#define PARAM(a, b) # a, b
static SParamDB sParams[] =
{
	SParamDB(PARAM(PM_MatChannelSB,                          ECGP_PM_MatChannelSB),                          0),
	SParamDB(PARAM(PM_MatDiffuseColor,                       ECGP_PM_MatDiffuseColor),                       0),
	SParamDB(PARAM(PM_MatSpecularColor,                      ECGP_PM_MatSpecularColor),                      0),
	SParamDB(PARAM(PM_MatEmissiveColor,                      ECGP_PM_MatEmissiveColor),                      0),
	SParamDB(PARAM(PM_MatMatrixTCM,                          ECGP_PM_MatMatrixTCM),                          0),
	SParamDB(PARAM(PM_MatDeformWave,                         ECGP_PM_MatDeformWave),                         0),
	SParamDB(PARAM(PM_MatDetailTilingAndAlphaRef,            ECGP_PM_MatDetailTilingAndAlphaRef),            0),
	SParamDB(PARAM(PM_MatSilPomDetailParams,                 ECGP_PM_MatSilPomDetailParams),                 0),

	SParamDB(PARAM(PB_HDRParams,                             ECGP_PB_HDRParams),                             0),
	SParamDB(PARAM(PB_StereoParams,                          ECGP_PB_StereoParams),                          0),

	SParamDB(PARAM(PB_VisionMtlParams,                       ECGP_PB_VisionMtlParams),                       0),
	SParamDB(PARAM(PI_EffectLayerParams,                     ECGP_PI_EffectLayerParams),                     0),

	SParamDB(PARAM(PB_IrregKernel,                           ECGP_PB_IrregKernel),                           0),
	SParamDB(PARAM(PB_RegularKernel,                         ECGP_PB_RegularKernel),                         0),

	SParamDB(PARAM(PB_VolumetricFogParams,                   ECGP_PB_VolumetricFogParams),                   0),
	SParamDB(PARAM(PB_VolumetricFogRampParams,               ECGP_PB_VolumetricFogRampParams),               0),
	SParamDB(PARAM(PB_VolumetricFogSunDir,                   ECGP_PB_VolumetricFogSunDir),                   0),
	SParamDB(PARAM(PB_FogColGradColBase,                     ECGP_PB_FogColGradColBase),                     0),
	SParamDB(PARAM(PB_FogColGradColDelta,                    ECGP_PB_FogColGradColDelta),                    0),
	SParamDB(PARAM(PB_FogColGradParams,                      ECGP_PB_FogColGradParams),                      0),
	SParamDB(PARAM(PB_FogColGradRadial,                      ECGP_PB_FogColGradRadial),                      0),
	SParamDB(PARAM(PB_VolumetricFogSamplingParams,           ECGP_PB_VolumetricFogSamplingParams),           0),
	SParamDB(PARAM(PB_VolumetricFogDistributionParams,       ECGP_PB_VolumetricFogDistributionParams),       0),
	SParamDB(PARAM(PB_VolumetricFogScatteringParams,         ECGP_PB_VolumetricFogScatteringParams),         0),
	SParamDB(PARAM(PB_VolumetricFogScatteringBlendParams,    ECGP_PB_VolumetricFogScatteringBlendParams),    0),
	SParamDB(PARAM(PB_VolumetricFogScatteringColor,          ECGP_PB_VolumetricFogScatteringColor),          0),
	SParamDB(PARAM(PB_VolumetricFogScatteringSecondaryColor, ECGP_PB_VolumetricFogScatteringSecondaryColor), 0),
	SParamDB(PARAM(PB_VolumetricFogHeightDensityParams,      ECGP_PB_VolumetricFogHeightDensityParams),      0),
	SParamDB(PARAM(PB_VolumetricFogHeightDensityRampParams,  ECGP_PB_VolumetricFogHeightDensityRampParams),  0),
	SParamDB(PARAM(PB_VolumetricFogDistanceParams,           ECGP_PB_VolumetricFogDistanceParams),           0),
	SParamDB(PARAM(PB_VolumetricFogGlobalEnvProbe0,          ECGP_PB_VolumetricFogGlobalEnvProbe0),          0),
	SParamDB(PARAM(PB_VolumetricFogGlobalEnvProbe1,          ECGP_PB_VolumetricFogGlobalEnvProbe1),          0),
	SParamDB(PARAM(PI_NumInstructions,                       ECGP_PI_NumInstructions),                       PD_INDEXED),

	SParamDB(PARAM(PB_FromRE,                                ECGP_PB_FromRE),                                PD_INDEXED | 0),

	SParamDB(PARAM(PB_WaterLevel,                            ECGP_PB_WaterLevel),                            0),

	SParamDB(PARAM(PB_CausticsParams,                        ECGP_PB_CausticsParams),                        0),
	SParamDB(PARAM(PB_CausticsSmoothSunDirection,            ECGP_PB_CausticsSmoothSunDirection),            0),

	SParamDB(PARAM(PB_Time,                                  ECGP_PB_Time),                                  0,              sParseTimeExpr),
	SParamDB(PARAM(PB_FrameTime,                             ECGP_PB_FrameTime),                             0,              sParseTimeExpr),

	SParamDB(PARAM(PB_ScreenSize,                            ECGP_PB_ScreenSize),                            0),
	SParamDB(PARAM(PB_CameraPos,                             ECGP_PB_CameraPos),                             0),
	SParamDB(PARAM(PB_NearFarDist,                           ECGP_PB_NearFarDist),                           0),

	SParamDB(PARAM(PI_WrinklesMask0,                         ECGP_PI_WrinklesMask0),                         0),
	SParamDB(PARAM(PI_WrinklesMask1,                         ECGP_PI_WrinklesMask1),                         0),
	SParamDB(PARAM(PI_WrinklesMask2,                         ECGP_PI_WrinklesMask2),                         0),

	SParamDB(PARAM(PB_CloudShadingColorSun,                  ECGP_PB_CloudShadingColorSun),                  0),
	SParamDB(PARAM(PB_CloudShadingColorSky,                  ECGP_PB_CloudShadingColorSky),                  0),

	SParamDB(PARAM(PB_CloudShadowParams,                     ECGP_PB_CloudShadowParams),                     0),
	SParamDB(PARAM(PB_CloudShadowAnimParams,                 ECGP_PB_CloudShadowAnimParams),                 0),

	SParamDB(PARAM(PB_ClipVolumeParams,                      ECGP_PB_ClipVolumeParams),                      0),

	SParamDB(PARAM(PB_WaterRipplesLookupParams,              ECGP_PB_WaterRipplesLookupParams),              0),

#if defined(FEATURE_SVO_GI)
	SParamDB(PARAM(PB_SvoViewProj0,                          ECGP_PB_SvoViewProj0),                          0),
	SParamDB(PARAM(PB_SvoViewProj1,                          ECGP_PB_SvoViewProj1),                          0),
	SParamDB(PARAM(PB_SvoViewProj2,                          ECGP_PB_SvoViewProj2),                          0),
	SParamDB(PARAM(PB_SvoNodeBoxWS,                          ECGP_PB_SvoNodeBoxWS),                          0),
	SParamDB(PARAM(PB_SvoNodeBoxTS,                          ECGP_PB_SvoNodeBoxTS),                          0),
	SParamDB(PARAM(PB_SvoNodesForUpdate0,                    ECGP_PB_SvoNodesForUpdate0),                    0),
	SParamDB(PARAM(PB_SvoNodesForUpdate1,                    ECGP_PB_SvoNodesForUpdate1),                    0),
	SParamDB(PARAM(PB_SvoNodesForUpdate2,                    ECGP_PB_SvoNodesForUpdate2),                    0),
	SParamDB(PARAM(PB_SvoNodesForUpdate3,                    ECGP_PB_SvoNodesForUpdate3),                    0),
	SParamDB(PARAM(PB_SvoNodesForUpdate4,                    ECGP_PB_SvoNodesForUpdate4),                    0),
	SParamDB(PARAM(PB_SvoNodesForUpdate5,                    ECGP_PB_SvoNodesForUpdate5),                    0),
	SParamDB(PARAM(PB_SvoNodesForUpdate6,                    ECGP_PB_SvoNodesForUpdate6),                    0),
	SParamDB(PARAM(PB_SvoNodesForUpdate7,                    ECGP_PB_SvoNodesForUpdate7),                    0),

	SParamDB(PARAM(PB_SvoTreeSettings0,                      ECGP_PB_SvoTreeSettings0),                      0),
	SParamDB(PARAM(PB_SvoTreeSettings1,                      ECGP_PB_SvoTreeSettings1),                      0),
	SParamDB(PARAM(PB_SvoTreeSettings2,                      ECGP_PB_SvoTreeSettings2),                      0),
	SParamDB(PARAM(PB_SvoTreeSettings3,                      ECGP_PB_SvoTreeSettings3),                      0),
	SParamDB(PARAM(PB_SvoTreeSettings4,                      ECGP_PB_SvoTreeSettings4),                      0),
	SParamDB(PARAM(PB_SvoTreeSettings5,                      ECGP_PB_SvoTreeSettings5),                      0),
	SParamDB(PARAM(PB_SvoParams0,                            ECGP_PB_SvoParams0),                            0),
	SParamDB(PARAM(PB_SvoParams1,                            ECGP_PB_SvoParams1),                            0),
	SParamDB(PARAM(PB_SvoParams2,                            ECGP_PB_SvoParams2),                            0),
	SParamDB(PARAM(PB_SvoParams3,                            ECGP_PB_SvoParams3),                            0),
	SParamDB(PARAM(PB_SvoParams4,                            ECGP_PB_SvoParams4),                            0),
	SParamDB(PARAM(PB_SvoParams5,                            ECGP_PB_SvoParams5),                            0),
	SParamDB(PARAM(PB_SvoParams6,                            ECGP_PB_SvoParams6),                            0),
	SParamDB(PARAM(PB_SvoParams7,                            ECGP_PB_SvoParams7),                            0),
	SParamDB(PARAM(PB_SvoParams8,                            ECGP_PB_SvoParams8),                            0),
	SParamDB(PARAM(PB_SvoParams9,                            ECGP_PB_SvoParams9),                            0),
#endif

	SParamDB()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

const char* CShaderMan::mfGetShaderParamName(ECGParam ePR)
{
	int n = 0;
	const char* szName;
	while (szName = sParams[n].szName)
	{
		if (sParams[n].eParamType == ePR)
			return szName;
		n++;
	}
	if (ePR == ECGP_PM_Tweakable)
		return "PM_Tweakable";
	return NULL;
}

SParamDB* CShaderMan::mfGetShaderParamDB(const char* szSemantic)
{
	const char* szName;
	int n = 0;
	while (szName = sParams[n].szName)
	{
		int nLen = strlen(szName);
		if (!strnicmp(szName, szSemantic, nLen) || (sParams[n].szAliasName && !strnicmp(sParams[n].szAliasName, szSemantic, strlen(sParams[n].szAliasName))))
			return &sParams[n];
		n++;
	}
	return NULL;
}

bool CShaderMan::mfParseParamComp(int comp, SCGParam* pCurParam, const char* szSemantic, char* params, const char* szAnnotations, SShaderFXParams& FXParams, CShader* ef, uint32 nParamFlags, EHWShaderClass eSHClass, bool bExpressionOperand)
{
	if (comp >= 4 || comp < -1)
		return false;
	if (!pCurParam)
		return false;
	if (comp > 0)
		pCurParam->m_Flags &= ~PF_SINGLE_COMP;
	else
		pCurParam->m_Flags |= PF_SINGLE_COMP;
	if (!szSemantic || !szSemantic[0])
	{
		if (!pCurParam->m_pData)
			pCurParam->m_pData = new SParamData;
		CRY_ASSERT(comp >= 0);
		pCurParam->m_pData->d.fData[comp] = shGetFloat(params);
		if (!((nParamFlags >> comp) & PF_TWEAKABLE_0))
			pCurParam->m_eCGParamType = (ECGParam)((int)pCurParam->m_eCGParamType | (int)(ECGP_PB_Scalar << (comp * 8)));
		else
		{
			pCurParam->m_eCGParamType = (ECGParam)((int)pCurParam->m_eCGParamType | (int)(ECGP_PM_Tweakable << (comp * 8)));
			if (!bExpressionOperand)
			{
				pCurParam->m_eCGParamType = (ECGParam)((int)pCurParam->m_eCGParamType | (int)ECGP_PM_Tweakable);
				pCurParam->m_Flags |= PF_MATERIAL | PF_SINGLE_COMP;
			}
		}
		return true;
	}
	if (!stricmp(szSemantic, "NULL"))
		return true;
	if (szSemantic[0] == '(')
	{
		pCurParam->m_eCGParamType = ECGP_PM_Tweakable;
		pCurParam->m_Flags |= PF_SINGLE_COMP | PF_MATERIAL;
		return true;
	}
	const char* szName;
	int n = 0;
	while (szName = sParams[n].szName)
	{
		int nLen = strlen(szName);
		if (!strnicmp(szName, szSemantic, nLen) || (sParams[n].szAliasName && !strnicmp(sParams[n].szAliasName, szSemantic, strlen(sParams[n].szAliasName))))
		{
			if (sParams[n].nFlags & PD_MERGED)
				pCurParam->m_Flags |= PF_CANMERGED;
			if (!strnicmp(szName, "PI_", 3))
				pCurParam->m_Flags |= PF_INSTANCE;
			else if (!strnicmp(szName, "SI_", 3))
				pCurParam->m_Flags |= PF_INSTANCE;
			else if (!strnicmp(szName, "PM_", 3))
				pCurParam->m_Flags |= PF_MATERIAL;
			static_assert(ECGP_COUNT <= 256, "ECGParam does not fit into 1 byte.");
			if (comp > 0)
				pCurParam->m_eCGParamType = (ECGParam)((int)pCurParam->m_eCGParamType | (int)(sParams[n].eParamType << (comp * 8)));
			else
				pCurParam->m_eCGParamType = sParams[n].eParamType;
			if (pCurParam->m_nParameters > 1)
				pCurParam->m_Flags |= PF_MATRIX;
			if (sParams[n].nFlags & PD_INDEXED)
			{
				if (szSemantic[nLen] == '[')
				{
					int nID = shGetInt(&szSemantic[nLen + 1]);
					assert(nID < 256);
					if (comp > 0)
						nID <<= (comp * 8);
					pCurParam->m_nID |= nID;
				}
			}
			if (sParams[n].ParserFunc)
				sParams[n].ParserFunc(params ? params : szSemantic, szAnnotations, pCurParam, comp, ef);
			break;
		}
		n++;
	}
	if (!szName)
		return false;
	return true;
}

bool CShaderMan::mfParseCGParam(char* scr, const char* szAnnotations, SShaderFXParams& FXParams, CShader* ef, std::vector<SCGParam>* pParams, int nComps, uint32 nParamFlags, EHWShaderClass eSHClass, bool bExpressionOperand)
{
	char* name;
	long cmd;
	char* params;
	char* data;

	int nComp = 0;

	enum {eComp = 1, eParam, eName};
	static STokenDesc commands[] =
	{
		{ eName,  "Name"  },
		{ eComp,  "Comp"  },
		{ eParam, "Param" },
		{ 0,      0       }
	};
	SCGParam vpp;
	int n = pParams->size();
	bool bRes = true;

	while ((cmd = shGetObject(&scr, commands, &name, &params)) > 0)
	{
		data = NULL;
		if (name)
			data = name;
		else if (params)
			data = params;

		switch (cmd)
		{
		case eName:
			vpp.m_Name = data;
			break;
		case eComp:
			{
				if (nComp < 4)
				{
					bRes &= mfParseParamComp(nComp, &vpp, name, params, szAnnotations, FXParams, ef, nParamFlags, eSHClass, bExpressionOperand);
					nComp++;
				}
			}
			break;
		case eParam:
			if (!name)
				name = params;
			bRes &= mfParseParamComp(-1, &vpp, name, params, szAnnotations, FXParams, ef, nParamFlags, eSHClass, bExpressionOperand);
			break;
		}
	}
	if (n == pParams->size())
		pParams->push_back(vpp);
	assert(bRes == true);
	return bRes;
}

bool CShaderMan::mfParseFXParameter(SShaderFXParams& FXParams, SFXParam* pr, const char* ParamName, CShader* ef, bool bInstParam, int nParams, std::vector<SCGParam>* pParams, EHWShaderClass eSHClass, bool bExpressionOperand)
{
	SCGParam CGpr;
	char scr[256];

	uint32 nParamFlags = pr->GetParamFlags();

	CryFixedStringT<512> Semantic = "Name=";
	Semantic += ParamName;
	Semantic += " ";
	int nComps = 0;
	if (!pr->m_Semantic.empty())
	{
		Semantic += "Param=";
		Semantic += pr->m_Semantic.c_str();
		nComps = pr->m_nComps;
	}
	else
	{
		for (int i = 0; i < pr->m_nComps; i++)
		{
			if (i)
				Semantic += " ";
			CryFixedStringT<128> cur;
			pr->GetParamComp(i, cur);
			if (cur.empty())
				break;
			nComps++;
			if ((cur.at(0) == '-' && isdigit(uint8(cur.at(1)))) || isdigit(uint8(cur.at(0))))
				cry_sprintf(scr, "Comp = %s", cur.c_str());
			else
				cry_sprintf(scr, "Comp '%s'", cur.c_str());
			Semantic += scr;
		}
	}
	// Process parameters only with semantics
	if (nComps)
	{
		uint32 nOffs = pParams->size();
		bool bRes = mfParseCGParam((char*)Semantic.c_str(), pr->m_Annotations.c_str(), FXParams, ef, pParams, nComps, nParamFlags, eSHClass, bExpressionOperand);
		assert(bRes);
		if (pParams->size() > nOffs)
		{
			//assert(pBind->m_nComponents == 1);
			SCGParam& p = (*pParams)[nOffs];
			p.m_dwBind = -1;
			p.m_nParameters = nParams;
			p.m_Flags |= nParamFlags;
			if (p.m_Flags & PF_AUTOMERGED)
			{
				if (!p.m_pData)
					p.m_pData = new SParamData;
				const char* src = p.m_Name.c_str();
				for (uint32 i = 0; i < 4; i++)
				{
					char param[128];
					char dig = i + 0x30;
					while (*src)
					{
						if (src[0] == '_' && src[1] == '_' && src[2] == dig)
						{
							int n = 0;
							src += 3;
							while (src[n])
							{
								if (src[n] == '_' && src[n + 1] == '_')
									break;
								param[n] = src[n];
								n++;
							}
							param[n] = 0;
							src += n;
							if (param[0])
								p.m_pData->m_CompNames[i] = param;
							break;
						}
						else
							src++;
					}
				}
			}
		}
		return bRes;
	}
	// Parameter without semantic
	return false;
}

// SM_ - material slots
// SR_ - global engine RT's
static SSamplerDB sSamplers[] =
{
	SSamplerDB(PARAM(SM_Diffuse,                 ECGS_MatSlot_Diffuse),    0),
	SSamplerDB(PARAM(SM_Normalmap,               ECGS_MatSlot_Normalmap),  0),
	SSamplerDB(PARAM(SM_Glossmap,                ECGS_MatSlot_Gloss),      0),
	SSamplerDB(PARAM(SM_Env,                     ECGS_MatSlot_Env),        0),
	SSamplerDB(PARAM(SS_Shadow0,                 ECGS_Shadow0),            0),
	SSamplerDB(PARAM(SS_Shadow1,                 ECGS_Shadow1),            0),
	SSamplerDB(PARAM(SS_Shadow2,                 ECGS_Shadow2),            0),
	SSamplerDB(PARAM(SS_Shadow3,                 ECGS_Shadow3),            0),
	SSamplerDB(PARAM(SS_Shadow4,                 ECGS_Shadow4),            0),
	SSamplerDB(PARAM(SS_Shadow5,                 ECGS_Shadow5),            0),
	SSamplerDB(PARAM(SS_Shadow6,                 ECGS_Shadow6),            0),
	SSamplerDB(PARAM(SS_Shadow7,                 ECGS_Shadow7),            0),
	SSamplerDB(PARAM(SS_TrilinearClamp,          ECGS_TrilinearClamp),     0),
	SSamplerDB(PARAM(SS_TrilinearWrap,           ECGS_TrilinearWrap),      0),
	SSamplerDB(PARAM(SS_MaterialAnisoHighWrap,   ECGS_MatAnisoHighWrap),   0),
	SSamplerDB(PARAM(SS_MaterialAnisoLowWrap,    ECGS_MatAnisoLowWrap),    0),
	SSamplerDB(PARAM(SS_MaterialTrilinearWrap,   ECGS_MatTrilinearWrap),   0),
	SSamplerDB(PARAM(SS_MaterialBilinearWrap,    ECGS_MatBilinearWrap),    0),
	SSamplerDB(PARAM(SS_MaterialTrilinearClamp,  ECGS_MatTrilinearClamp),  0),
	SSamplerDB(PARAM(SS_MaterialBilinearClamp,   ECGS_MatBilinearClamp),   0),
	SSamplerDB(PARAM(SS_MaterialAnisoHighBorder, ECGS_MatAnisoHighBorder), 0),
	SSamplerDB(PARAM(SS_MaterialTrilinearBorder, ECGS_MatTrilinearBorder), 0),
	SSamplerDB(PARAM(SS_PointClamp,              ECGS_PointClamp),         0),
	SSamplerDB(PARAM(SS_PointWrap,               ECGS_PointWrap),          0),
	SSamplerDB()
};

bool CShaderMan::mfParseFXSampler(SShaderFXParams& FXParams, SFXSampler* pr, const char* ParamName, CShader* ef, int nParams, std::vector<SCGSampler>* pParams, EHWShaderClass eSHClass)
{
	SCGSampler CGpr;
	CGpr.m_nStateHandle = pr->m_nTexState;
	if (pr->m_Semantic.empty() && pr->m_Values.empty())
	{
		if (CGpr.m_nStateHandle != EDefaultSamplerStates::Unspecified)
		{
			pParams->push_back(CGpr);
			return true;
		}
		return false;
	}
	const char* szSemantic = pr->m_Semantic.c_str();
	const char* szName;
	int n = 0;
	while (szName = sSamplers[n].szName)
	{
		if (!stricmp(szName, szSemantic))
		{
			static_assert(ECGS_COUNT <= 256, "ECGSampler does not fit into 1 byte.");
			CGpr.m_eCGSamplerType = sSamplers[n].eSamplerType;
			pParams->push_back(CGpr);
			break;
		}
		n++;
	}
	if (!szName)
		return false;
	return true;
}

// TM_  - material slots
// TSF_ - scaleform slots
// TS_  - global engine RT's
static STextureDB sTextures[] =
{
	STextureDB(PARAM(TM_Diffuse,                      ECGT_MatSlot_Diffuse),              0),
	STextureDB(PARAM(TM_Normals,                      ECGT_MatSlot_Normals),              0),
	STextureDB(PARAM(TM_Specular,                     ECGT_MatSlot_Specular),             0),
	STextureDB(PARAM(TM_Env,                          ECGT_MatSlot_Env),                  0),
	STextureDB(PARAM(TM_Detail,                       ECGT_MatSlot_Detail),               0),
	STextureDB(PARAM(TM_Translucency,                 ECGT_MatSlot_Translucency),         0),
	STextureDB(PARAM(TM_Height,                       ECGT_MatSlot_Height),               0),
	STextureDB(PARAM(TM_DecalOverlay,                 ECGT_MatSlot_DecalOverlay),         0),
	STextureDB(PARAM(TM_SubSurface,                   ECGT_MatSlot_SubSurface),           0),
	STextureDB(PARAM(TM_Custom,                       ECGT_MatSlot_Custom),               0),
	STextureDB(PARAM(TM_CustomSecondary,              ECGT_MatSlot_CustomSecondary),      0),
	STextureDB(PARAM(TM_Opacity,                      ECGT_MatSlot_Opacity),              0),
	STextureDB(PARAM(TM_Smoothness,                   ECGT_MatSlot_Smoothness),           0),
	STextureDB(PARAM(TM_Emittance,                    ECGT_MatSlot_Emittance),            0),

	STextureDB(PARAM(TSF_ScaleformInput0,             ECGT_ScaleformInput0),              0),
	STextureDB(PARAM(TSF_ScaleformInput1,             ECGT_ScaleformInput1),              0),
	STextureDB(PARAM(TSF_ScaleformInput2,             ECGT_ScaleformInput2),              0),
	STextureDB(PARAM(TSF_ScaleformInputY,             ECGT_ScaleformInputY),              0),
	STextureDB(PARAM(TSF_ScaleformInputU,             ECGT_ScaleformInputU),              0),
	STextureDB(PARAM(TSF_ScaleformInputV,             ECGT_ScaleformInputV),              0),
	STextureDB(PARAM(TSF_ScaleformInputA,             ECGT_ScaleformInputA),              0),

	STextureDB(PARAM(TS_Shadow0,                      ECGT_Shadow0),                      0),
	STextureDB(PARAM(TS_Shadow1,                      ECGT_Shadow1),                      0),
	STextureDB(PARAM(TS_Shadow2,                      ECGT_Shadow2),                      0),
	STextureDB(PARAM(TS_Shadow3,                      ECGT_Shadow3),                      0),
	STextureDB(PARAM(TS_Shadow4,                      ECGT_Shadow4),                      0),
	STextureDB(PARAM(TS_Shadow5,                      ECGT_Shadow5),                      0),
	STextureDB(PARAM(TS_Shadow6,                      ECGT_Shadow6),                      0),
	STextureDB(PARAM(TS_Shadow7,                      ECGT_Shadow7),                      0),
	STextureDB(PARAM(TS_ShadowMask,                   ECGT_ShadowMask),                   0),

	STextureDB(PARAM(TS_HDR_Target,                   ECGT_HDR_Target),                   0),
	STextureDB(PARAM(TS_HDR_TargetPrev,               ECGT_HDR_TargetPrev),               0),
	STextureDB(PARAM(TS_HDR_AverageLuminance,         ECGT_HDR_AverageLuminance),         0),
	STextureDB(PARAM(TS_HDR_FinalBloom,               ECGT_HDR_FinalBloom),               0),

	STextureDB(PARAM(TS_BackBuffer,                   ECGT_BackBuffer),                   0),
	STextureDB(PARAM(TS_BackBufferScaled_d2,          ECGT_BackBufferScaled_d2),          0),
	STextureDB(PARAM(TS_BackBufferScaled_d4,          ECGT_BackBufferScaled_d4),          0),
	STextureDB(PARAM(TS_BackBufferScaled_d8,          ECGT_BackBufferScaled_d8),          0),

	STextureDB(PARAM(TS_ZTarget,                      ECGT_ZTarget),                      0),
	STextureDB(PARAM(TS_ZTargetMS,                    ECGT_ZTargetMS),                    0),
	STextureDB(PARAM(TS_ZTargetScaled_d2,             ECGT_ZTargetScaled_d2),             0),
	STextureDB(PARAM(TS_ZTargetScaled_d4,             ECGT_ZTargetScaled_d4),             0),

	STextureDB(PARAM(TS_SceneTarget,                  ECGT_SceneTarget),                  0),
	STextureDB(PARAM(TS_SceneNormalsBent,             ECGT_SceneNormalsBent),             0),
	STextureDB(PARAM(TS_SceneNormals,                 ECGT_SceneNormals),                 0),
	STextureDB(PARAM(TS_SceneDiffuse,                 ECGT_SceneDiffuse),                 0),
	STextureDB(PARAM(TS_SceneSpecular,                ECGT_SceneSpecular),                0),
	STextureDB(PARAM(TS_SceneNormalsMapMS,            ECGT_SceneNormalsMS),               0),

	STextureDB(PARAM(TS_VolumetricClipVolumeStencil,  ECGT_VolumetricClipVolumeStencil),  0),
	STextureDB(PARAM(TS_VolumetricFog,                ECGT_VolumetricFog),                0),
	STextureDB(PARAM(TS_VolumetricFogGlobalEnvProbe0, ECGT_VolumetricFogGlobalEnvProbe0), 0),
	STextureDB(PARAM(TS_VolumetricFogGlobalEnvProbe1, ECGT_VolumetricFogGlobalEnvProbe1), 0),
	STextureDB(PARAM(TS_VolumetricFogShadow0,         ECGT_VolumetricFogShadow0),         0),
	STextureDB(PARAM(TS_VolumetricFogShadow1,         ECGT_VolumetricFogShadow1),         0),

	STextureDB(PARAM(TS_WaterOceanMap,                ECGT_WaterOceanMap),                0),
	STextureDB(PARAM(TS_WaterVolumeDDN,               ECGT_WaterVolumeDDN),               0),
	STextureDB(PARAM(TS_WaterVolumeCaustics,          ECGT_WaterVolumeCaustics),          0),
	STextureDB(PARAM(TS_WaterVolumeRefl,              ECGT_WaterVolumeRefl),              0),
	STextureDB(PARAM(TS_WaterVolumeReflPrev,          ECGT_WaterVolumeReflPrev),          0),
	STextureDB(PARAM(TS_RainOcclusion,                ECGT_RainOcclusion),                0),
	STextureDB(PARAM(TS_TerrainNormMap,               ECGT_TerrainNormMap),               0),
	STextureDB(PARAM(TS_TerrainBaseMap,               ECGT_TerrainBaseMap),               0),
	STextureDB(PARAM(TS_TerrainElevMap,               ECGT_TerrainElevMap),               0),
	STextureDB(PARAM(TS_WindGrid,                     ECGT_WindGrid),                     0),
	STextureDB(PARAM(TS_CloudShadow,                  ECGT_CloudShadow),                  0),
	STextureDB(PARAM(TS_VolCloudShadow,               ECGT_VolCloudShadow),               0),

	STextureDB()
};

bool CShaderMan::mfParseFXTexture(SShaderFXParams& FXParams, SFXTexture* pr, const char* ParamName, CShader* ef, int nParams, std::vector<SCGTexture>* pParams, EHWShaderClass eSHClass)
{
	SCGTexture CGpr;

	if (pr->m_Semantic.empty())
	{
		if (pr->m_szTexture.size())
		{
			const char* nameTex = pr->m_szTexture.c_str();

			// FT_DONT_STREAM = disable streaming for explicitly specified textures
			if (nameTex[0] == '$')
			{
				CGpr.m_pTexture = mfCheckTemplateTexName(nameTex, eTT_MaxTexType /*unused*/);
			}
			else if (strchr(nameTex, '#')) // test for " #" to skip max material names
			{
				CGpr.m_pAnimInfo = mfReadTexSequence(nameTex, pr->GetTexFlags() | FT_DONT_STREAM, false);
			}

			if (!CGpr.m_pTexture && !CGpr.m_pAnimInfo)
			{
				CGpr.m_pTexture = (CTexture*)gRenDev->EF_LoadTexture(nameTex, pr->GetTexFlags() | FT_DONT_STREAM);
			}

			if (CGpr.m_pTexture)
			{
				CGpr.m_pTexture->AddRef();
			}

			CGpr.m_bSRGBLookup = pr->m_bSRGBLookup;
			CGpr.m_bGlobal = false;
			pParams->push_back(CGpr);
			return true;
		}

		return false;
	}

	const char* szSemantic = pr->m_Semantic.c_str();
	const char* szName;
	int n = 0;
	while (szName = sTextures[n].szName)
	{
		if (!stricmp(szName, szSemantic))
		{
			static_assert(ECGT_COUNT <= 256, "ECGTexture does not fit into 1 byte.");
			CGpr.m_eCGTextureType = sTextures[n].eTextureType;
			CGpr.m_bSRGBLookup = pr->m_bSRGBLookup;
			CGpr.m_bGlobal = false;
			pParams->push_back(CGpr);
			break;
		}
		n++;
	}

	return szName != nullptr;
}

//===========================================================================================
bool SShaderParam::GetValue(const char* szName, DynArrayRef<SShaderParam>* Params, float* v, int nID)
{
	bool bRes = false;

	for (int i = 0; i < Params->size(); i++)
	{
		SShaderParam* sp = &(*Params)[i];
		if (!sp)
			continue;

		if (!stricmp(sp->m_Name, szName))
		{
			bRes = true;
			switch (sp->m_Type)
			{
			case eType_HALF:
			case eType_FLOAT:
				v[nID] = sp->m_Value.m_Float;
				break;
			case eType_SHORT:
				v[nID] = (float)sp->m_Value.m_Short;
				break;
			case eType_INT:
			case eType_TEXTURE_HANDLE:
				v[nID] = (float)sp->m_Value.m_Int;
				break;

			case eType_VECTOR:
				v[0] = sp->m_Value.m_Vector[0];
				v[1] = sp->m_Value.m_Vector[1];
				v[2] = sp->m_Value.m_Vector[2];
				break;

			case eType_FCOLOR:
				v[0] = sp->m_Value.m_Color[0];
				v[1] = sp->m_Value.m_Color[1];
				v[2] = sp->m_Value.m_Color[2];
				v[3] = sp->m_Value.m_Color[3];
				break;

			case eType_STRING:
				assert(0);
				bRes = false;
				break;
			case eType_UNKNOWN:
				assert(0);
				bRes = false;
				break;
			}

			break;
		}
	}

	return bRes;
}

bool SShaderParam::GetValue(uint8 eSemantic, DynArrayRef<SShaderParam>* Params, float* v, int nID)
{
	bool bRes = false;
	for (int i = 0; i < Params->size(); i++)
	{
		SShaderParam* sp = &(*Params)[i];
		if (!sp)
			continue;

		if (sp->m_eSemantic == eSemantic)
		{
			bRes = true;
			switch (sp->m_Type)
			{
			case eType_HALF:
			case eType_FLOAT:
				v[nID] = sp->m_Value.m_Float;
				break;
			case eType_SHORT:
				v[nID] = (float)sp->m_Value.m_Short;
				break;
			case eType_INT:
			case eType_TEXTURE_HANDLE:
				v[nID] = (float)sp->m_Value.m_Int;
				break;

			case eType_VECTOR:
				v[0] = sp->m_Value.m_Vector[0];
				v[1] = sp->m_Value.m_Vector[1];
				v[2] = sp->m_Value.m_Vector[2];
				break;

			case eType_FCOLOR:
				v[0] = sp->m_Value.m_Color[0];
				v[1] = sp->m_Value.m_Color[1];
				v[2] = sp->m_Value.m_Color[2];
				v[3] = sp->m_Value.m_Color[3];
				break;

			case eType_STRING:
				assert(0);
				bRes = false;
				break;

			case eType_UNKNOWN:
				assert(0);
				bRes = false;
				break;
			}

			break;
		}
	}

	return bRes;
}

SParamData::~SParamData()
{
}

SParamData::SParamData(const SParamData& sp)
{
	for (int i = 0; i < 4; i++)
	{
		m_CompNames[i] = sp.m_CompNames[i];
		d.nData64[i] = sp.d.nData64[i];
	}
}

SCGTexture::~SCGTexture()
{
	if (!m_pAnimInfo)
	{
		SAFE_RELEASE(m_pTexture);
		m_pAnimInfo = nullptr;
	}
	else
	{
		SAFE_RELEASE(m_pAnimInfo);
		m_pTexture = nullptr;
	}
}

SCGTexture::SCGTexture(const SCGTexture& sp) : SCGBind(sp)
{
	if (!sp.m_pAnimInfo)
	{
		m_pAnimInfo = nullptr;
		if (m_pTexture = sp.m_pTexture)
			m_pTexture->AddRef();
	}
	else
	{
		m_pTexture = nullptr;
		if (m_pAnimInfo = sp.m_pAnimInfo)
			m_pAnimInfo->AddRef();
	}

	m_eCGTextureType = sp.m_eCGTextureType;
	m_bSRGBLookup = sp.m_bSRGBLookup;
	m_bGlobal = sp.m_bGlobal;
}
