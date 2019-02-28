// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ShaderResources.h"
#include "GraphicsPipeline/Common/GraphicsPipelineStateSet.h"
#include "Common/Textures/TextureHelpers.h"

#include "DriverD3D.h"

//===============================================================================

void CShaderResources::ConvertToInputResource(SInputShaderResources* pDst)
{
	pDst->m_ResFlags = m_ResFlags;
	pDst->m_AlphaRef = m_AlphaRef;
	pDst->m_FurAmount = m_FurAmount;
	pDst->m_VoxelCoverage = m_VoxelCoverage;
	pDst->m_CloakAmount = m_CloakAmount;
	pDst->m_SortPrio = m_SortPrio;
	if (m_pDeformInfo)
		pDst->m_DeformInfo = *m_pDeformInfo;
	else
		pDst->m_DeformInfo.m_eType = eDT_Unknown;

	pDst->m_TexturePath = m_TexturePath;
	if (m_pDeformInfo)
		pDst->m_DeformInfo = *m_pDeformInfo;

	if (m_pDetailDecalInfo)
		pDst->m_DetailDecalInfo = *m_pDetailDecalInfo;
	else
		pDst->m_DetailDecalInfo.Reset();

	for (int i = 0; i < EFTT_MAX; i++)
	{
		if (m_Textures[i])
		{
			pDst->m_Textures[i] = *m_Textures[i];
		}
		else
		{
			pDst->m_Textures[i].Reset();
		}
	}

	ToInputLM(pDst->m_LMaterial);
}

size_t CShaderResources::GetResourceMemoryUsage(ICrySizer* pSizer)
{
	size_t nCurrentElement(0);
	size_t nCurrentElementSize(0);
	size_t nTotalSize(0);

	SIZER_COMPONENT_NAME(pSizer, "CShaderResources");
	for (nCurrentElement = 0; nCurrentElement < EFTT_MAX; ++nCurrentElement)
	{
		SEfResTexture*& rpTexture = m_Textures[nCurrentElement];
		if (rpTexture)
		{
			if (rpTexture->m_Sampler.m_pITex)
			{
				nCurrentElementSize = rpTexture->m_Sampler.m_pITex->GetDataSize();
				pSizer->AddObject(rpTexture->m_Sampler.m_pITex, nCurrentElementSize);
				nTotalSize += nCurrentElementSize;
				IResourceCollector* pColl = pSizer->GetResourceCollector();
				if (pColl)
				{
					pColl->AddResource(rpTexture->m_Sampler.m_pITex->GetName(), nCurrentElementSize);
				}
			}
		}
	}

	return nTotalSize;

	//TArray<struct SHRenderTarget *> m_RTargets;
	//SSkyInfo *m_pSky;
	//SDeformInfo *m_pDeformInfo;
	//SDetailDecalInfo *m_pDetailDecalInfo;
	//DynArray<Vec4> m_Constants[3];
}

void CShaderResources::Release() const
{
	if (CryInterlockedDecrement(&m_nRefCounter) <= 0) // is checked inside render thread
	{
		if (gRenDev && gRenDev->m_pRT)
		{
			// Do delete itself inside render thread
			gRenDev->ExecuteRenderThreadCommand( [=]{ const_cast<CShaderResources*>(this)->RT_Release(); },
				ERenderCommandFlags::RenderLoadingThread_defer | ERenderCommandFlags::LevelLoadingThread_defer );
		}
		else
		{
			delete this;
		}
	}
}

void CShaderResources::RT_Release()
{
	if (CryInterlockedDecrement(&m_nRefCounter) <= 0)
	{
		delete this;
	}
}

void CShaderResources::Cleanup()
{
	//assert(gRenDev->m_pRT->IsRenderThread());
	m_resources.ClearResources();

	for (int i = 0; i < EFTT_MAX; i++)
	{
		SAFE_DELETE(m_Textures[i]);
	}
	SAFE_DELETE(m_pDeformInfo);
	SAFE_DELETE(m_pDetailDecalInfo);
	if (m_pSky)
	{
		for (size_t i = 0; i < sizeof(m_pSky->m_SkyBox) / sizeof(m_pSky->m_SkyBox[0]); ++i)
		{
			SAFE_RELEASE(m_pSky->m_SkyBox[i]);
		}
		SAFE_DELETE(m_pSky);
	}
	ReleaseConstants();

	// not thread safe main thread can potentially access this destroyed entry in mfCreateShaderResources()
	// (if flushing of unloaded textures (UnloadLevel) is not complete before pre-loading of new materials)
	if (CShader::s_ShaderResources_known.Num() > m_Id && CShader::s_ShaderResources_known[m_Id] == this)
	{
		CShader::s_ShaderResources_known[m_Id] = NULL;
	}
}

void CShaderResources::ClearPipelineStateCache()
{
	if (m_pipelineStateCache)
	{
		m_pipelineStateCache->Clear();
	}
}

CShaderResources::~CShaderResources()
{
	Cleanup();
}

CShaderResources::CShaderResources()
	: m_resources()
{
	m_pipelineStateCache = std::make_shared<CGraphicsPipelineStateLocalCache>();
	Reset();
}

CShaderResources::CShaderResources(const CShaderResources& src)
	: m_resources(src.m_resources)
{
	m_pipelineStateCache = std::make_shared<CGraphicsPipelineStateLocalCache>();
	Reset();

	SBaseShaderResources::operator=(src);

	for (int i = 0; i < EFTT_MAX; i++)
	{
		if (!src.m_Textures[i])
			continue;
		AddTextureMap(i);
		*m_Textures[i] = *src.m_Textures[i];
	}

	m_Constants = src.m_Constants;
	m_IdGroup = src.m_IdGroup;
}

CShaderResources::CShaderResources(SInputShaderResources* pSrc)
	: m_resources()
{
	assert(pSrc);
	PREFAST_ASSUME(pSrc);

	m_pipelineStateCache = std::make_shared<CGraphicsPipelineStateLocalCache>();
	Reset();

	m_szMaterialName = pSrc->m_szMaterialName;
	m_TexturePath = pSrc->m_TexturePath;
	m_ResFlags = pSrc->m_ResFlags;
	m_AlphaRef = pSrc->m_AlphaRef;
	m_FurAmount = pSrc->m_FurAmount;
	m_VoxelCoverage = pSrc->m_VoxelCoverage;
	m_HeatAmount = pSrc->m_HeatAmount;
	m_CloakAmount = pSrc->m_CloakAmount;
	m_SortPrio = pSrc->m_SortPrio;
	m_ShaderParams = pSrc->m_ShaderParams;
	if (pSrc->m_DeformInfo.m_eType)
	{
		m_pDeformInfo = new SDeformInfo;
		*m_pDeformInfo = pSrc->m_DeformInfo;
	}
	if ((m_ResFlags & MTL_FLAG_DETAIL_DECAL))
	{
		m_pDetailDecalInfo = new SDetailDecalInfo;
		*m_pDetailDecalInfo = pSrc->m_DetailDecalInfo;
	}

	for (int i = 0; i < EFTT_MAX; i++)
	{
		if (pSrc && (!pSrc->m_Textures[i].m_Name.empty() || pSrc->m_Textures[i].m_Sampler.m_pTex))
		{
			if (!m_Textures[i])
				AddTextureMap(i);
			assert(m_Textures[i]);
			pSrc->m_Textures[i].CopyTo(m_Textures[i]);
		}
		else
		{
			if (m_Textures[i])
				m_Textures[i]->Reset();
			m_Textures[i] = NULL;
		}
	}

	SetInputLM(pSrc->m_LMaterial);
}


CShaderResources* CShaderResources::Clone() const
{
	CShaderResources* pSR = new CShaderResources(*this);
	pSR->m_nRefCounter = 1;
	for (uint32 i = 0; i < CShader::s_ShaderResources_known.Num(); i++)
	{
		if (!CShader::s_ShaderResources_known[i])
		{
			pSR->m_Id = i;
			CShader::s_ShaderResources_known[i] = pSR;
			return pSR;
		}
	}
	if (CShader::s_ShaderResources_known.Num() >= MAX_REND_SHADER_RES)
	{
		Warning("ERROR: CShaderMan::mfCreateShaderResources: MAX_REND_SHADER_RESOURCES hit");
		pSR->Release();
		return CShader::s_ShaderResources_known[1];
	}
	pSR->m_Id = CShader::s_ShaderResources_known.Num();
	CShader::s_ShaderResources_known.AddElem(pSR);

	return pSR;
}

bool CShaderResources::HasDynamicTexModifiers() const
{
	return m_Textures[EFTT_DIFFUSE] && m_Textures[EFTT_DIFFUSE]->IsNeedTexTransform();
}

void CShaderResources::SetInputLM(const CInputLightMaterial& lm)
{
	if (!m_Constants.size())
		m_Constants.resize(NUM_PM_CONSTANTS);

	ColorF* pDst = (ColorF*)&m_Constants[0];

	memcpy(pDst, lm.m_Channels, min(int(EFTT_MAX), REG_PM_DIFFUSE_COL / 2) * sizeof(lm.m_Channels[0]));

	const float fMinStepSignedFmt = (1.0f / 127.0f) * 255.0f;
	const float fSmoothness = max(fMinStepSignedFmt, lm.m_Smoothness) / 255.0f;
	const float fAlpha = lm.m_Opacity;

	pDst[REG_PM_DIFFUSE_COL] = lm.m_Diffuse;
	pDst[REG_PM_SPECULAR_COL] = lm.m_Specular;
	pDst[REG_PM_EMISSIVE_COL] = lm.m_Emittance;

	pDst[REG_PM_DIFFUSE_COL][3] = fAlpha;
	pDst[REG_PM_SPECULAR_COL][3] = fSmoothness;
}

void CShaderResources::ToInputLM(CInputLightMaterial& lm)
{
	if (!m_Constants.size())
		return;

	ColorF* pDst = (ColorF*)&m_Constants[0];

	lm.m_Diffuse = pDst[REG_PM_DIFFUSE_COL];
	lm.m_Specular = pDst[REG_PM_SPECULAR_COL];
	lm.m_Emittance = pDst[REG_PM_EMISSIVE_COL];

	lm.m_Opacity = pDst[REG_PM_DIFFUSE_COL][3];
	lm.m_Smoothness = pDst[REG_PM_SPECULAR_COL][3] * 255.0f;
}

static ColorF sColBlack = Col_Black;
static ColorF sColWhite = Col_White;

const ColorF& CShaderResources::GetColorValue(EEfResTextures slot) const
{
	if (!m_Constants.size())
		return sColBlack;

	int majoroffs;
	switch (slot)
	{
	case EFTT_DIFFUSE:
		majoroffs = REG_PM_DIFFUSE_COL;
		break;
	case EFTT_SPECULAR:
		majoroffs = REG_PM_SPECULAR_COL;
		break;
	case EFTT_OPACITY:
		return sColWhite;
	case EFTT_SMOOTHNESS:
		return sColWhite;
	case EFTT_EMITTANCE:
		majoroffs = REG_PM_EMISSIVE_COL;
		break;
	default:
		return sColWhite;
	}

	ColorF* pDst = (ColorF*)&m_Constants[0];
	return pDst[majoroffs];
}

float CShaderResources::GetStrengthValue(EEfResTextures slot) const
{
	if (!m_Constants.size())
		return sColBlack.a;

	int majoroffs, minoroffs;
	switch (slot)
	{
	case EFTT_DIFFUSE:
		return 1.0f;
	case EFTT_SPECULAR:
		return 1.0f;
	case EFTT_OPACITY:
		majoroffs = REG_PM_DIFFUSE_COL;
		minoroffs = 3;
		break;
	case EFTT_SMOOTHNESS:
		majoroffs = REG_PM_SPECULAR_COL;
		minoroffs = 3;
		break;
	case EFTT_EMITTANCE:
		majoroffs = REG_PM_EMISSIVE_COL;
		minoroffs = 3;
		break;
	default:
		return 1.0f;
	}

	ColorF* pDst = (ColorF*)&m_Constants[0];
	return pDst[majoroffs][minoroffs];
}

void CShaderResources::SetColorValue(EEfResTextures slot, const ColorF& color)
{
	if (!m_Constants.size())
		return;

	// NOTE: ideally the switch goes away and values are indexed directly
	int majoroffs;
	switch (slot)
	{
	case EFTT_DIFFUSE:
		majoroffs = REG_PM_DIFFUSE_COL;
		break;
	case EFTT_SPECULAR:
		majoroffs = REG_PM_SPECULAR_COL;
		break;
	case EFTT_OPACITY:
		return;
	case EFTT_SMOOTHNESS:
		return;
	case EFTT_EMITTANCE:
		majoroffs = REG_PM_EMISSIVE_COL;
		break;
	default:
		return;
	}

	ColorF* pDst = (ColorF*)&m_Constants[0];
	pDst[majoroffs] = ColorF(color.toVec3(), pDst[majoroffs][3]);
}

void CShaderResources::SetStrengthValue(EEfResTextures slot, float value)
{
	if (!m_Constants.size())
		return;

	// NOTE: ideally the switch goes away and values are indexed directly
	int majoroffs, minoroffs;
	switch (slot)
	{
	case EFTT_DIFFUSE:
		return;
	case EFTT_SPECULAR:
		return;
	case EFTT_OPACITY:
		majoroffs = REG_PM_DIFFUSE_COL;
		minoroffs = 3;
		break;
	case EFTT_SMOOTHNESS:
		majoroffs = REG_PM_SPECULAR_COL;
		minoroffs = 3;
		break;
	case EFTT_EMITTANCE:
		majoroffs = REG_PM_EMISSIVE_COL;
		minoroffs = 3;
		break;
	default:
		return;
	}

	ColorF* pDst = (ColorF*)&m_Constants[0];
	pDst[majoroffs][minoroffs] = value;
}

//////////////////////////////////////////////////////////////////////////
void CShaderResources::SetInvalid()
{
	m_flags |= eFlagInvalid;

	if (m_pipelineStateCache)
	{
		m_pipelineStateCache->Clear();
	}
}

void CShaderResources::UpdateConstants(IShader* pISH)
{
	_smart_ptr<CShaderResources> pSelf(this);
	_smart_ptr<IShader> pShader = pISH;

	ERenderCommandFlags flags = ERenderCommandFlags::LevelLoadingThread_defer;
	if (gcpRendD3D->m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Disabled) 
		flags |= ERenderCommandFlags::MainThread_defer;

	gRenDev->ExecuteRenderThreadCommand( [=]{ pSelf->RT_UpdateConstants(pShader);}, flags);
}

static char* sSetParameterExp(const char* szExpr, Vec4& vVal, DynArray<SShaderParam>& Params, bool& bResult);

static char* sSetOperand(char* szExpr, Vec4& vVal, DynArray<SShaderParam>& Params, bool& bResult)
{
	char temp[256];
	SkipCharacters(&szExpr, kWhiteSpace);
	if (*szExpr == '(')
	{
		szExpr = sSetParameterExp(szExpr, vVal, Params, bResult);
	}
	else
	{
		fxFillNumber(&szExpr, temp);
		if (temp[0] != '.' && (temp[0] < '0' || temp[0] > '9'))
		{
			bResult &= SShaderParam::GetValue(temp, &Params, &vVal[0], 0);
			//assert(bF);
		}
		else
			vVal[0] = shGetFloat(temp);
	}
	return szExpr;
}

static char* sSetParameterExp(const char* szExpr, Vec4& vVal, DynArray<SShaderParam>& Params, bool& bResult)
{
	if (szExpr[0] != '(')
		return NULL;
	char expr[1024];
	int n = 0;
	int nc = 0;
	char theChar;
	while (theChar = *szExpr)
	{
		if (theChar == '(')
		{
			n++;
			if (n == 1)
			{
				szExpr++;
				continue;
			}
		}
		else if (theChar == ')')
		{
			n--;
			if (!n)
			{
				szExpr++;
				break;
			}
		}
		expr[nc++] = theChar;
		szExpr++;
	}
	expr[nc++] = 0;
	assert(!n);
	if (n)
		return NULL;

	char* szE = expr;
	Vec4 vVals[2];
	vVals[0] = Vec4(0, 0, 0, 0);
	szE = sSetOperand(szE, vVals[0], Params, bResult);
	SkipCharacters(&szE, kWhiteSpace);
	char szOp = *szE++;
	vVals[1] = Vec4(0, 0, 0, 0);
	SkipCharacters(&szE, kWhiteSpace);
	szE = sSetOperand(szE, vVals[1], Params, bResult);

	switch (szOp)
	{
	case '+':
		vVal[0] = vVals[0][0] + vVals[1][0];
		break;
	case '-':
		vVal[0] = vVals[0][0] - vVals[1][0];
		break;
	case '*':
		vVal[0] = vVals[0][0] * vVals[1][0];
		break;
	case '/':
		vVal[0] = vVals[0][0];
		if (vVals[1][0])
			vVal[0] /= vVals[1][0];
		break;
	}

	return (char*)szExpr;
}

static bool sSetParameter(SFXParam* pPR, Vec4* Constants, DynArray<SShaderParam>& Params, int nOffs, EHWShaderClass eSHClass)
{
	uint32 nFlags = pPR->GetParamFlags();
	bool bFound = false;
	for (int i = 0; i < 4; i++)
	{
		if (nFlags & PF_AUTOMERGED)
		{
			CryFixedStringT<128> n;
			pPR->GetCompName(i, n);
			bool bF = SShaderParam::GetValue(n.c_str(), &Params, &Constants[pPR->m_nRegister[eSHClass] - nOffs][0], i);
			if (!bF)
			{
				CryFixedStringT<128> v;
				pPR->GetParamComp(i, v);
				if (!v.empty() && v.at(0) == '(')
				{
					Vec4 vVal = Vec4(-10000, -10000, -10000, -10000);
					bool bResult = true;
					sSetParameterExp(v.c_str(), vVal, Params, bResult);
					if (bResult)
					{
						Constants[pPR->m_nRegister[eSHClass] - nOffs][i] = vVal[0];
						if (vVal[0] != -10000)
							bF = true;
					}
				}
			}
			bFound |= bF;
		}
		else
			bFound |= SShaderParam::GetValue(pPR->m_Name.c_str(), &Params, &Constants[pPR->m_nRegister[eSHClass] - nOffs][0], i);
	}
	return bFound;
}

static int s_nClass;
struct CompareItem
{
	bool operator()(const SFXParam* a, const SFXParam* b)
	{
		return (a->m_nRegister[s_nClass] < b->m_nRegister[s_nClass]);
	}
};

inline void AddShaderParamToArray(SShaderFXParams& FXParams, FixedDynArray<SFXParam*>& OutParams, EHWShaderClass shaderClass)
{
	uint32 nMergeMaskPerShaderClass[eHWSC_Num] = { 2, 1, 2, 2, 2, 2 };
	uint32 nMergeMask = nMergeMaskPerShaderClass[shaderClass];

	const int numFXParams = FXParams.m_FXParams.size();
	for (int n = 0; n < numFXParams; n++)
	{
		SFXParam* pShaderParam = &FXParams.m_FXParams[n];
		if (pShaderParam->m_nFlags & (nMergeMask << PF_MERGE_SHIFT))
			continue;
		if (pShaderParam->m_nCB == CB_PER_MATERIAL)
		{
			if (pShaderParam->m_nRegister[shaderClass] < FIRST_REG_PM || pShaderParam->m_nRegister[shaderClass] >= 10000)
				continue;
			size_t m = 0;
			for (; m < (int)OutParams.size(); m++)
			{
				if (OutParams[m]->m_Name == pShaderParam->m_Name)
					break;
			}
			if (m == OutParams.size())
				OutParams.push_back(pShaderParam);
		}
	}
}

void CShaderResources::RT_UpdateConstants(IShader* pISH)
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CShaderResources::RT_UpdateConstants");
	//assert(gRenDev->m_pRT->IsRenderThread());

	CShader* pSH = (CShader*)pISH;
	assert(pSH->m_Flags & EF_LOADED); // Make sure shader is parsed

	uint32 nMDMask = 0;

	// Update common PM parameters
	{
		Matrix44 matrixTCM(IDENTITY);
		SEfResTexture* pTex = m_Textures[EFTT_DIFFUSE];

		if (pTex && pTex->m_Ext.m_pTexModifier)
		{
			pTex->Update(EFTT_DIFFUSE, nMDMask);
			matrixTCM = pTex->m_Ext.m_pTexModifier->m_TexMatrix;
		}

		Vec4 detailTilingAndAlpharef(1, 1, 0, 0);
		pTex = m_Textures[EFTT_DETAIL_OVERLAY];
		if (pTex && pTex->m_Ext.m_pTexModifier)
		{
			pTex->Update(EFTT_DETAIL_OVERLAY, nMDMask);
			detailTilingAndAlpharef.x = pTex->m_Ext.m_pTexModifier->m_Tiling[0];
			detailTilingAndAlpharef.y = pTex->m_Ext.m_pTexModifier->m_Tiling[1];
		}

		detailTilingAndAlpharef.z = GetAlphaRef();

		Vec4 deformWave(0, 0, 0, 1);
		if (m_pDeformInfo && m_pDeformInfo->m_fDividerX != 0)
		{
			deformWave.x = m_pDeformInfo->m_WaveX.m_Freq + m_pDeformInfo->m_WaveX.m_Phase;
			deformWave.y = m_pDeformInfo->m_WaveX.m_Amp;
			deformWave.z = m_pDeformInfo->m_WaveX.m_Level;
			deformWave.w = 1.0f / m_pDeformInfo->m_fDividerX;
		}

		Vec4* pDst = (Vec4*)&m_Constants[0];
		*alias_cast<Matrix44f*>(&pDst[REG_PM_TCM_MATRIX]) = matrixTCM;
		pDst[REG_PM_DEFORM_WAVE] = deformWave;
		pDst[REG_PM_DETAILTILING_ALPHAREF] = detailTilingAndAlpharef;

		Vec4 silPomDetailParams(1.0f);
		if (SEfResTexture* pTex = m_Textures[EFTT_DETAIL_OVERLAY])
		{
			if (auto pTexModifier = pTex->m_Ext.m_pTexModifier)
			{
				const float detailUScale = pTexModifier->m_Tiling[0];
				const float detailVScale = pTexModifier->m_Tiling[1];
				const bool apply = detailUScale < 1.0f && detailVScale < 1.0f;
				const float uScale = apply ? detailUScale : 1.0f;
				const float vScale = apply ? detailVScale : 1.0f;

				silPomDetailParams = Vec4(uScale, vScale, 1.0f / uScale, 1.0f / vScale);
			}
		}
		pDst[REG_PM_SILPOM_DETAIL_PARAMS] = silPomDetailParams;
	}

	// Premultiply alpha when additive blending is enabled
	if (m_ResFlags & MTL_FLAG_ADDITIVE)
	{
		const float opacity = m_Constants[REG_PM_DIFFUSE_COL][3];

		m_Constants[REG_PM_DIFFUSE_COL][0] *= opacity;
		m_Constants[REG_PM_DIFFUSE_COL][1] *= opacity;
		m_Constants[REG_PM_DIFFUSE_COL][2] *= opacity;

		m_Constants[REG_PM_SPECULAR_COL][0] *= opacity;
		m_Constants[REG_PM_SPECULAR_COL][1] *= opacity;
		m_Constants[REG_PM_SPECULAR_COL][2] *= opacity;

		m_Constants[REG_PM_EMISSIVE_COL][0] *= opacity;
		m_Constants[REG_PM_EMISSIVE_COL][1] *= opacity;
		m_Constants[REG_PM_EMISSIVE_COL][2] *= opacity;
	}

	SShaderFXParams& FXParams = gRenDev->m_cEF.m_Bin.mfGetFXParams(pSH);

	DynArray<SShaderParam> PublicParams = pSH->GetPublicParams();
	// Copy material tweakables to shader public params
	for (int i = 0; i < m_ShaderParams.size(); i++)
	{
		SShaderParam& TW = m_ShaderParams[i];
		for (int j = 0; j < PublicParams.size(); j++)
		{
			SShaderParam& PB = PublicParams[j];
			if (!strcmp(PB.m_Name, TW.m_Name))
			{
				TW.CopyType(PB);
				PB.CopyValueNoString(TW); // there should not be 'string' values set to shader
				break;
			}
		}
	}

	const int numFXParams = FXParams.m_FXParams.size();
	const size_t paramsByteSize = numFXParams * sizeof(SFXParam*);

	// Allocate memory for param arrays on stack
	FixedDynArray<SFXParam*> Params;
	PREFAST_SUPPRESS_WARNING(6255) Params.set(ArrayT((SFXParam**)alloca(paramsByteSize), numFXParams));

	for (int i = 0; i < (int)pSH->m_HWTechniques.Num(); i++)
	{
		SShaderTechnique* pTech = pSH->m_HWTechniques[i];
		for (int j = 0; j < (int)pTech->m_Passes.Num(); j++)
		{
			SShaderPass* pPass = &pTech->m_Passes[j];
			//assert((pPass->m_VShader && pPass->m_PShader) || pPass->m_CShader);
			if (pPass->m_VShader)
			{
				AddShaderParamToArray(FXParams, Params, eHWSC_Vertex);
			}
			if (pPass->m_PShader)
			{
				AddShaderParamToArray(FXParams, Params, eHWSC_Pixel);
			}
			if (pPass->m_GShader)
			{
				AddShaderParamToArray(FXParams, Params, eHWSC_Geometry);
			}
			if (pPass->m_HShader)
			{
				AddShaderParamToArray(FXParams, Params, eHWSC_Hull);
			}
			if (pPass->m_DShader)
			{
				AddShaderParamToArray(FXParams, Params, eHWSC_Domain);
			}
			if (pPass->m_CShader)
			{
				AddShaderParamToArray(FXParams, Params, eHWSC_Compute);
			}
		}
	}
	s_nClass = eHWSC_Vertex;
	if (Params.size() > 1)
	{
		std::sort(Params.begin(), Params.end(), CompareItem());
	}

	auto* __restrict pConstants = &m_Constants;
	SFXParam* pPR;
	int nFirstConst = FIRST_REG_PM;
	int nLastConst = -1;

	if (Params.size())
	{
		nLastConst = Params[Params.size() - 1]->m_nRegister[eHWSC_Vertex];
		assert(nLastConst >= FIRST_REG_PM && Params[0]->m_nRegister[eHWSC_Vertex] >= FIRST_REG_PM);
	}
	nLastConst = max(nFirstConst + NUM_PM_CONSTANTS - 1, nLastConst);
	int nCur = pConstants->size();
	if (nCur < nLastConst + 1 - nFirstConst)
		pConstants->resize(nLastConst + 1 - nFirstConst);
	for (int ii = nCur; ii <= nLastConst - nFirstConst; ii++)
	{
		(*pConstants)[ii] = Vec4(-100000, -200000, -300000, -400000);
	}

	if (PublicParams.size())
	{
		for (int i = 0; i < (int)Params.size(); i++)
		{
			pPR = Params[i];
			sSetParameter(pPR, &(*pConstants)[0], PublicParams, nFirstConst, eHWSC_Vertex);
		}
	}

	CConstantBufferPtr temp;
	temp.swap(m_pConstantBuffer);

	if (m_Constants.size())
	{
		// NOTE: The pointers and the size is 16 byte aligned
		size_t nSize = m_Constants.size() * sizeof(Vec4);

		m_pConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(nSize, false);
		m_pConstantBuffer->UpdateBuffer(&m_Constants[0], Align(nSize, 256));

#if !defined(_RELEASE) && (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_ORBIS) && !CRY_RENDERER_GNM
		if (m_pConstantBuffer)
		{
			string name = string("PM CBuffer ") + pSH->GetName() + "@" + m_szMaterialName;

			#if CRY_RENDERER_VULKAN || CRY_PLATFORM_ORBIS
				m_pConstantBuffer->GetD3D()->DebugSetName(name.c_str());
			#else
				m_pConstantBuffer->GetD3D()->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.length(), name.c_str());
			#endif
		}
#endif
	}


	RT_UpdateResourceSet();
}

void CShaderResources::RT_UpdateResourceSet()
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());
	uint32_t flags = m_flags;

	// create resource set if necessary
	if (!m_pCompiledResourceSet || (flags & eFlagRecreateResourceSet))
	{
		flags &= ~eFlagRecreateResourceSet;

		m_resources.MarkBindingChanged();
		m_pCompiledResourceSet = GetDeviceObjectFactory().CreateResourceSet();
	}

	flags &= ~(EFlags_AnimatedSequence | EFlags_DynamicUpdates);

	// TODO: default material created first doesn't have a constant buffer
	if (!m_pConstantBuffer)
		return;

	// per material constant buffer
	m_resources.SetConstantBuffer(eConstantBufferShaderSlot_PerMaterial, m_pConstantBuffer, EShaderStage_AllWithoutCompute);

	// material textures
	bool bContainsInvalidTexture = false;
	for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
	{
		ResourceViewHandle hView = EDefaultResourceViews::Default;
		CTexture* pTex = nullptr;

		if (m_Textures[texType])
		{
			const STexSamplerRT& smp = m_Textures[texType]->m_Sampler;
			if (smp.m_pDynTexSource)
			{
				hView = EDefaultResourceViews::sRGB;
				flags |= EFlags_DynamicUpdates;

				if (ITexture* pITex = smp.m_pDynTexSource->GetTexture())
				{
					pTex = CTexture::GetByID(pITex->GetTextureID());
				}
				else
				{
					m_flags = flags;
					CRY_ASSERT(m_resources.HasChanged());
					return; // flash texture has not been allocated yet. abort
				}
			}
			else if (smp.m_pAnimInfo && (smp.m_pAnimInfo->m_NumAnimTexs > 1))
			{
				flags |= EFlags_AnimatedSequence;
				pTex = smp.m_pTex;
			}
			else if (smp.m_pTex)
			{
				pTex = smp.m_pTex;
			}
		}
		else
		{
			pTex = TextureHelpers::LookupTexDefault(texType);
		}

		bContainsInvalidTexture |= !CTexture::IsTextureExist(pTex);
		m_resources.SetTexture(IShader::GetTextureSlot(texType), pTex, hView, EShaderStage_AllWithoutCompute);
	}

	// TODO: default material created first doesn't have a constant buffer
	if (m_pConstantBuffer && !bContainsInvalidTexture)
	{
		m_pCompiledResourceSet->Update(m_resources);
	}

	m_flags = flags;
}

void CShaderResources::CloneConstants(const IRenderShaderResources* pISrc)
{
	//assert(gRenDev->m_pRT->IsRenderThread());

	CShaderResources* pSrc = (CShaderResources*)pISrc;

	if (!pSrc)
	{
		m_Constants.clear();
		m_pConstantBuffer.reset();
		return;
	}
	else
	{
		m_HeatAmount = pSrc->m_HeatAmount;
		m_Constants = pSrc->m_Constants;

		m_pConstantBuffer = pSrc->m_pConstantBuffer;

		m_pCompiledResourceSet = pSrc->m_pCompiledResourceSet;
		m_flags |= eFlagRecreateResourceSet;
	}
}

void CShaderResources::ReleaseConstants()
{
	m_Constants.clear();

	m_pConstantBuffer.reset();
	m_pCompiledResourceSet.reset();
}

static void sChangeAniso(SEfResTexture* pTex)
{
	SamplerStateHandle nTS = pTex->m_Sampler.m_nTexState;
	int8 nAniso = min(CRenderer::CV_r_texminanisotropy, CRenderer::CV_r_texmaxanisotropy);
	if (nAniso < 1)
		return;
	SSamplerState ST = CDeviceObjectFactory::LookupSamplerState(nTS).first;
	if (ST.m_nAnisotropy == nAniso)
		return;

	if (nAniso >= 16)
		ST.m_nMipFilter =
		  ST.m_nMinFilter =
		    ST.m_nMagFilter = FILTER_ANISO16X;
	else if (nAniso >= 8)
		ST.m_nMipFilter =
		  ST.m_nMinFilter =
		    ST.m_nMagFilter = FILTER_ANISO8X;
	else if (nAniso >= 4)
		ST.m_nMipFilter =
		  ST.m_nMinFilter =
		    ST.m_nMagFilter = FILTER_ANISO4X;
	else if (nAniso >= 2)
		ST.m_nMipFilter =
		  ST.m_nMinFilter =
		    ST.m_nMagFilter = FILTER_ANISO2X;
	else
		ST.m_nMipFilter =
		  ST.m_nMinFilter =
		    ST.m_nMagFilter = FILTER_TRILINEAR;

	ST.m_nAnisotropy = nAniso;
	pTex->m_Sampler.m_nTexState = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(ST);
}

void CShaderResources::AdjustForSpec()
{
	// Note: Anisotropic filtering for smoothness maps is deliberately disabled, otherwise
	//       mip transitions become too obvious when using maps prefiltered with normal variance

	if (m_Textures[EFTT_DIFFUSE])
		sChangeAniso(m_Textures[EFTT_DIFFUSE]);
	if (m_Textures[EFTT_NORMALS])
		sChangeAniso(m_Textures[EFTT_NORMALS]);
	if (m_Textures[EFTT_SPECULAR])
		sChangeAniso(m_Textures[EFTT_SPECULAR]);

	if (m_Textures[EFTT_CUSTOM])
		sChangeAniso(m_Textures[EFTT_CUSTOM]);
	if (m_Textures[EFTT_CUSTOM_SECONDARY])
		sChangeAniso(m_Textures[EFTT_CUSTOM_SECONDARY]);

	if (m_Textures[EFTT_EMITTANCE])
		sChangeAniso(m_Textures[EFTT_EMITTANCE]);
}
