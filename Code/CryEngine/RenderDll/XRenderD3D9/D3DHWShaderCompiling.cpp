// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"

#include <lzss/LZSS.H>
#include <lzma/Lzma86.h>
#include <cstring>

#if !CRY_PLATFORM_ORBIS && !CRY_RENDERER_OPENGL && !CRY_RENDERER_OPENGLES && !CRY_RENDERER_VULKAN
	#if CRY_PLATFORM_DURANGO
		#include <D3D11Shader_x.h>
		#include <D3DCompiler_x.h>
	#else
		#include <D3D11Shader.h>
		#include <D3DCompiler.h>
	#endif
#endif

#if defined(USE_NV_API)
	#include NV_API_HEADER
#endif

#include "../Common/Shaders/RemoteCompiler.h"

static CryCriticalSection g_cAILock;
static SShaderAsyncInfo g_PendingList;
static SShaderAsyncInfo g_PendingListT;
#ifdef SHADER_ASYNC_COMPILATION
static SShaderAsyncInfo g_BuildList;
#endif

SShaderAsyncInfo& SShaderAsyncInfo::PendingList()  { return g_PendingList; }
SShaderAsyncInfo& SShaderAsyncInfo::PendingListT() { return g_PendingListT; }
#ifdef SHADER_ASYNC_COMPILATION
SShaderAsyncInfo& CAsyncShaderTask::BuildList()    { return g_BuildList; }
#endif

CryEvent SShaderAsyncInfo::s_RequestEv;

int CHWShader_D3D::s_nDevicePSDataSize = 0;
int CHWShader_D3D::s_nDeviceVSDataSize = 0;

namespace
{
	inline int GetCurrentFrameID()
	{
		return (gRenDev->m_pRT->IsRenderThread()) ? gRenDev->GetRenderFrameID() : gRenDev->GetMainFrameID();
	};
}

class CSpinLock
{
public:
	CSpinLock()
	{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		while (CryInterlockedCompareExchange(&s_locked, 1L, 0L) == 1L)
			CrySleep(0);
#endif
	}

	~CSpinLock()
	{
#if CRY_PLATFORM_WINDOWS
		InterlockedExchange(&s_locked, 0L);
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		while (CryInterlockedCompareExchange(&s_locked, 0L, 1L) == 0L);
#endif
	}

private:
	static volatile LONG s_locked;
};

volatile LONG CSpinLock::s_locked = 0L;

volatile int SShaderAsyncInfo::s_nPendingAsyncShaders = 0;
int SShaderAsyncInfo::s_nPendingAsyncShadersFXC = 0;

//==============================================================================================================

bool CHWShader_D3D::mfAddFXSampler(SHWSInstance* pInst, SShaderFXParams& FXParams, SFXSampler* pr, const char* ParamName, SCGBind* pBind, CShader* ef, EHWShaderClass eSHClass)
{
	assert(pBind);
	if (!pBind)
		return false;

	std::vector<SCGSampler>* pParams;
	pParams = &pInst->m_Samplers;
	uint32 nOffs = pParams->size();
	bool bRes = gRenDev->m_cEF.mfParseFXSampler(FXParams, pr, ParamName, ef, pBind->m_nParameters, pParams, eSHClass);
	if (pParams->size() > nOffs)
	{
		for (uint32 i = 0; i < pParams->size() - nOffs; i++)
		{
			SCGSampler& p = (*pParams)[nOffs + i];
			p.m_dwBind = pBind->m_dwBind + i;
			p.m_dwCBufSlot = pBind->m_dwCBufSlot;
			p.m_Name = pBind->m_Name;
		}
	}
	// Parameter without semantic
	return bRes;
}

bool CHWShader_D3D::mfAddFXTexture(SHWSInstance* pInst, SShaderFXParams& FXParams, SFXTexture* pr, const char* ParamName, SCGBind* pBind, CShader* ef, EHWShaderClass eSHClass)
{
	assert(pBind);
	if (!pBind)
		return false;

	std::vector<SCGTexture>* pParams;
	pParams = &pInst->m_Textures;
	uint32 nOffs = pParams->size();
	bool bRes = gRenDev->m_cEF.mfParseFXTexture(FXParams, pr, ParamName, ef, pBind->m_nParameters, pParams, eSHClass);
	if (!bRes)
	{
		int nnn = 0;
	}
	if (pParams->size() > nOffs)
	{
		for (uint32 i = 0; i < pParams->size() - nOffs; i++)
		{
			SCGTexture& p = (*pParams)[nOffs + i];
			p.m_dwBind = pBind->m_dwBind + i;
			p.m_dwCBufSlot = pBind->m_dwCBufSlot;
			p.m_Name = pBind->m_Name;
		}
	}
	// Parameter without semantic
	return bRes;
}

void CHWShader_D3D::mfAddFXParameter(SHWSInstance* pInst, SParamsGroup& OutParams, SShaderFXParams& FXParams, SFXParam* pr, const char* ParamName, SCGBind* pBind, CShader* ef, bool bInstParam, EHWShaderClass eSHClass)
{
	SCGParam CGpr;

	assert(pBind);
	if (!pBind)
		return;

	int nComps = 0;
	int nParams = pBind->m_nParameters;
	if (!pr->m_Semantic.empty())
		nComps = pr->m_nComps;
	else
	{
		for (int i = 0; i < pr->m_nComps; i++)
		{
			CryFixedStringT<128> cur;
			pr->GetParamComp(i, cur);
			if (cur.empty())
				break;
			nComps++;
		}
	}
	// Process parameters only with semantics
	if (nComps && nParams)
	{
		std::vector<SCGParam>* pParams;
		if (pr->m_nParameters > 1)
		{
			if (!bInstParam)
			{
				//if (!pInst->m_pParams[0])
				// pInst->m_pParams[0] = new std::vector<SCGParam>;
				pParams = &OutParams.Params[0];
			}
			else
			{
				//if (!pInst->m_pParams_Inst)
				//  pInst->m_pParams_Inst = new std::vector<SCGParam>;
				pParams = &OutParams.Params_Inst;
			}
		}
		else if (bInstParam)
		{
			//if (!pInst->m_pParams_Inst)
			//  pInst->m_pParams_Inst = new std::vector<SCGParam>;
			pParams = &OutParams.Params_Inst;
		}
		else
		{
			//if (!pInst->m_pParams[0])
			//  pInst->m_pParams[0] = new std::vector<SCGParam>;
			pParams = &OutParams.Params[0];
		}
		uint32 nOffs = pParams->size();
		bool bRes = gRenDev->m_cEF.mfParseFXParameter(FXParams, pr, ParamName, ef, bInstParam, pBind->m_nParameters, pParams, eSHClass, false);
		assert(bRes);
		if (pParams->size() > nOffs)
		{
			for (uint32 i = 0; i < pParams->size() - nOffs; i++)
			{
				//assert(pBind->m_nComponents == 1);
				SCGParam& p = (*pParams)[nOffs + i];
				p.m_dwBind = pBind->m_dwBind + i;
				p.m_dwCBufSlot = pBind->m_dwCBufSlot;
			}
		}
	}
	// Parameter without semantic
}

struct SAliasSampler
{
	STexSamplerRT Sampler;
	string        NameTex;
};

bool CHWShader_D3D::mfAddFXParameter(SHWSInstance* pInst, SParamsGroup& OutParams, SShaderFXParams& FXParams, const char* param, SCGBind* bn, bool bInstParam, EHWShaderClass eSHClass, CShader* pFXShader)
{
	if (bn->m_dwBind & SHADER_BIND_TEXTURE)
	{
		SFXTexture* pr = gRenDev->m_cEF.mfGetFXTexture(FXParams.m_FXTextures, param);
		if (pr)
		{
			if (bn->m_nParameters < 0)
				bn->m_nParameters = pr->m_nArray;
			bool bRes = mfAddFXTexture(pInst, FXParams, pr, param, bn, pFXShader, eSHClass);
			return true;
		}
	}
	else if (bn->m_dwBind & SHADER_BIND_SAMPLER)
	{
		SFXSampler* pr = gRenDev->m_cEF.mfGetFXSampler(FXParams.m_FXSamplers, param);
		if (pr)
		{
			if (bn->m_nParameters < 0)
				bn->m_nParameters = pr->m_nArray;
			bool bRes = mfAddFXSampler(pInst, FXParams, pr, param, bn, pFXShader, eSHClass);
			return true;
		}
	}
	else
	{
		SFXParam* pr = gRenDev->m_cEF.mfGetFXParameter(FXParams.m_FXParams, param);
		if (pr)
		{
			if (bn->m_nParameters < 0)
				bn->m_nParameters = pr->m_nParameters;
			mfAddFXParameter(pInst, OutParams, FXParams, pr, param, bn, pFXShader, bInstParam, eSHClass);
			return true;
		}
	}
	return false;
}

//==================================================================================================================

int CGBindCallback(const VOID* arg1, const VOID* arg2)
{
	SCGBind* pi1 = (SCGBind*)arg1;
	SCGBind* pi2 = (SCGBind*)arg2;

	if (pi1->m_dwBind < pi2->m_dwBind)
		return -1;
	if (pi1->m_dwBind > pi2->m_dwBind)
		return 1;

	return 0;
}

const char* szNamesCB[CB_NUM] = { "PER_BATCH", "PER_MATERIAL" };

void CHWShader_D3D::mfCreateBinds(std::vector<SCGBind> &binds, const void* pConstantTable, std::size_t nSize)
{
	uint32 i;
	ID3D11ShaderReflection* pShaderReflection = (ID3D11ShaderReflection*)pConstantTable;
	D3D11_SHADER_DESC Desc;
	pShaderReflection->GetDesc(&Desc);
	ID3D11ShaderReflectionConstantBuffer* pCB = NULL;
	for (uint32 n = 0; n < Desc.ConstantBuffers; n++)
	{
		pCB = pShaderReflection->GetConstantBufferByIndex(n);
		D3D11_SHADER_BUFFER_DESC SBDesc;
		pCB->GetDesc(&SBDesc);
		if (SBDesc.Type == D3D11_CT_RESOURCE_BIND_INFO)
			continue;
		int nCB;
		if (!strcmp("$Globals", SBDesc.Name))
			nCB = CB_PER_DRAW;
		else
			for (nCB = 0; nCB < CB_NUM; nCB++)
			{
				if (!strcmp(szNamesCB[nCB], SBDesc.Name))
					break;
			}
		//assert(nCB != CB_NUM);
		if (nCB >= CB_NUM)  // Allow having custom cbuffers in shaders
			continue;
		for (i = 0; i < SBDesc.Variables; i++)
		{
			uint32 nCount = 1;
			ID3D11ShaderReflectionVariable* pCV = pCB->GetVariableByIndex(i);
			ID3D11ShaderReflectionType* pVT = pCV->GetType();
			D3D11_SHADER_VARIABLE_DESC CDesc;
			D3D11_SHADER_TYPE_DESC CTDesc;
			pVT->GetDesc(&CTDesc);
			pCV->GetDesc(&CDesc);
			if (!(CDesc.uFlags & D3D10_SVF_USED))
				continue;
			if (CTDesc.Class == D3D10_SVC_VECTOR || CTDesc.Class == D3D10_SVC_SCALAR || CTDesc.Class == D3D10_SVC_MATRIX_COLUMNS || CTDesc.Class == D3D10_SVC_MATRIX_ROWS)
			{
				SCGBind cgp;
				assert(!(CDesc.StartOffset & 0xf));
				//assert(!(CDesc.Size & 0xf));
				int nReg = CDesc.StartOffset >> 4;
				cgp.m_dwBind = nReg; //<<2;
				cgp.m_dwCBufSlot = nCB;
				cgp.m_nParameters = (CDesc.Size + 15) >> 4;
				cgp.m_Name = CDesc.Name;
				cgp.m_Flags = CParserBin::GetCRC32(CDesc.Name);
				binds.push_back(cgp);
			}
			else
			{
				assert(false);
			}
		}
	}

	D3D11_SHADER_INPUT_BIND_DESC IBDesc;
	for (i = 0; i < Desc.BoundResources; i++)
	{
		ZeroStruct(IBDesc);
		pShaderReflection->GetResourceBindingDesc(i, &IBDesc);
		SCGBind cgp;
		if (IBDesc.Type == D3D10_SIT_TEXTURE)
			cgp.m_dwBind = IBDesc.BindPoint | SHADER_BIND_TEXTURE;
		else if (IBDesc.Type == D3D10_SIT_SAMPLER)
			cgp.m_dwBind = IBDesc.BindPoint | SHADER_BIND_SAMPLER;
		else
			continue;

		if (IBDesc.Dimension == D3D10_SRV_DIMENSION_BUFFER)
			continue;

		cgp.m_dwCBufSlot = IBDesc.BindPoint;
		cgp.m_nParameters = IBDesc.BindCount;
		cgp.m_Name = IBDesc.Name;
		cgp.m_Flags = CParserBin::GetCRC32(IBDesc.Name);
		binds.push_back(cgp);
	}
}

void CHWShader_D3D::mfGatherFXParameters(SHWSInstance* pInst, std::vector<SCGBind>& BindVars, CHWShader_D3D* pSH, int nFlags, CShader* pFXShader)
{
	//	LOADING_TIME_PROFILE_SECTION(iSystem);

	uint32 i, j;
	SAliasSampler samps[MAX_TMU];
	int nParam = 0;
	SParamsGroup Group;
	SShaderFXParams& FXParams = gRenDev->m_cEF.m_Bin.mfGetFXParams(pInst->m_bFallback ? CShaderMan::s_ShaderFallback : pFXShader);
	if (pInst->m_pBindVars.size())
	{
		for (i = 0; i < pInst->m_pBindVars.size(); i++)
		{
			SCGBind* bn = &BindVars[i];
			const char* param = bn->m_Name.c_str();

			if (!strncmp(param, "_g_", 3))
				continue;
			mfAddFXParameter(pInst, Group, FXParams, param, bn, false, pSH->m_eSHClass, pFXShader);
		}

		bool setSamplers[256] = { false };
		bool setTextures[256] = { false };

		for (i = 0; i < pInst->m_Samplers.size(); i++)
			setSamplers[pInst->m_Samplers[i].m_dwBind & 0xff] = true;
		for (i = 0; i < pInst->m_Textures.size(); i++)
			setTextures[pInst->m_Textures[i].m_dwBind & 0xff] = true;
	}
	if (nFlags != 1)
	{
		pInst->m_pFXTextures.reserve(FXParams.m_FXTextures.size());
		for (const auto &t : FXParams.m_FXTextures)
			pInst->m_pFXTextures.push_back(&t);;
	}
	else
	{
		assert(pInst->m_pAsync);
	}

	pInst->m_nMaxVecs[0] = pInst->m_nMaxVecs[1] = 0;
	if (pInst->m_pBindVars.size())
	{
		for (i = 0; i < pInst->m_pBindVars.size(); i++)
		{
			SCGBind* pB = &pInst->m_pBindVars[i];
			if (pB->m_dwBind & (SHADER_BIND_SAMPLER | SHADER_BIND_TEXTURE))
				continue;
			if (pB->m_dwCBufSlot < 0 || pB->m_dwCBufSlot > CB_NUM)
				continue;
			for (j = 0; j < Group.Params[0].size(); j++)
			{
				SCGParam* pr = &Group.Params[0][j];
				if (pr->m_dwBind == pB->m_dwBind && pr->m_Name == pB->m_Name)
					break;
			}
			if (j != Group.Params[0].size())
				continue;
			pInst->m_nMaxVecs[pB->m_dwCBufSlot] = max(pB->m_dwBind + pB->m_nParameters, pInst->m_nMaxVecs[pB->m_dwCBufSlot]);
		}
	}
	if (Group.Params[0].size())
	{
		for (i = 0; i < Group.Params[0].size(); i++)
		{
			SCGParam* pr = &Group.Params[0][i];

			if (pr->m_Flags & PF_MATERIAL)
				pInst->m_bHasPMParams = true;
		}

		gRenDev->m_cEF.mfCheckObjectDependParams(Group.Params[0], Group.Params[1], pSH->m_eSHClass, pFXShader);
	}

	for (i = 0; i < CB_NUM; i++)
	{
		if (Group.Params[i].size())
		{
			for (j = 0; j < Group.Params[i].size(); j++)
			{
				SCGParam* pr = &Group.Params[i][j];
				pInst->m_nMaxVecs[i] = max(pr->m_dwBind + pr->m_nParameters, pInst->m_nMaxVecs[i]);
			}
		}
	}
	int nMax = 0;
	if (pSH->m_eSHClass == eHWSC_Vertex)
		nMax = MAX_CONSTANTS_VS;
	else if (pSH->m_eSHClass == eHWSC_Pixel)
		nMax = MAX_CONSTANTS_PS;
	else if (pSH->m_eSHClass == eHWSC_Geometry)
		nMax = MAX_CONSTANTS_GS;
	else if (pSH->m_eSHClass == eHWSC_Domain)
		nMax = MAX_CONSTANTS_DS;
	else if (pSH->m_eSHClass == eHWSC_Hull)
		nMax = MAX_CONSTANTS_HS;
	else if (pSH->m_eSHClass == eHWSC_Compute)
		nMax = MAX_CONSTANTS_CS;
	assert(pInst->m_nMaxVecs[0] < nMax);
	assert(pInst->m_nMaxVecs[1] < nMax);

	if (Group.Params[0].size() > 0)
	{
		qsort(&Group.Params[0][0], Group.Params[0].size(), sizeof(SCGParam), CGBindCallback);
		pInst->m_nParams[0] = CGParamManager::GetParametersGroup(Group, 0);
	}
	if (Group.Params[1].size() > 0)
	{
		qsort(&Group.Params[1][0], Group.Params[1].size(), sizeof(SCGParam), CGBindCallback);
		pInst->m_nParams[1] = CGParamManager::GetParametersGroup(Group, 1);
	}
}

// Vertex shader specific
void CHWShader_D3D::mfUpdateFXVertexFormat(SHWSInstance* pInst, CShader* pSH)
{
	// Update global FX shader's vertex format / flags
	if (pSH)
	{
		InputLayoutHandle eVFormat = pSH->m_eVertexFormat;
		bool bCurrent = false;
		for (uint32 i = 0; i < pSH->m_HWTechniques.Num(); i++)
		{
			SShaderTechnique* hw = pSH->m_HWTechniques[i];
			for (uint32 j = 0; j < hw->m_Passes.Num(); j++)
			{
				SShaderPass* pass = &hw->m_Passes[j];
				if (pass->m_VShader)
				{
					if (pass->m_VShader == this)
						bCurrent = true;
					bool bUseLM = false;
					bool bUseTangs = false;
					bool bUseHWSkin = false;
					InputLayoutHandle eCurVFormat = pass->m_VShader->mfVertexFormat(bUseTangs, bUseLM, bUseHWSkin);
					if (eCurVFormat >= EDefaultInputLayouts::Empty)
						eVFormat = max(eVFormat, eCurVFormat);
					if (bUseTangs)
						pass->m_PassFlags |= VSM_TANGENTS;
					if (bUseHWSkin)
					{
						pass->m_PassFlags |= VSM_HWSKIN;
						pass->m_PassFlags |= VSM_VERTEX_VELOCITY;
					}
				}
			}
		}
		//assert (bCurrent);
		pSH->m_eVertexFormat = eVFormat;
	}
}

void CHWShader_D3D::mfPostVertexFormat(SHWSInstance* pInst, CHWShader_D3D* pHWSH, bool bCol, byte bNormal, bool bTC0, bool bTC1[2], bool bPSize, bool bTangent[2], bool bBitangent[2], bool bHWSkin, bool bSH[2], bool bVelocity, bool bMorph)
{
	if (bBitangent[0])
		pInst->m_VStreamMask_Decl |= 1 << VSF_TANGENTS;
	else if (bTangent[0])
		pInst->m_VStreamMask_Decl |= 1 << VSF_QTANGENTS;
	if (bBitangent[1])
		pInst->m_VStreamMask_Stream |= 1 << VSF_TANGENTS;
	else if (bTangent[1])
		pInst->m_VStreamMask_Stream |= 1 << VSF_QTANGENTS;
	if (pInst->m_VStreamMask_Decl & (1 << VSF_TANGENTS))
		bNormal = false;

	if (bHWSkin)
	{
		pInst->m_VStreamMask_Decl |= VSM_HWSKIN;
		pInst->m_VStreamMask_Stream |= VSM_HWSKIN;
	}

	if (bVelocity)
	{
		pInst->m_VStreamMask_Decl |= VSM_VERTEX_VELOCITY;
		pInst->m_VStreamMask_Stream |= VSM_VERTEX_VELOCITY;
	}
	if (bMorph)
	{
		pInst->m_VStreamMask_Decl |= VSM_MORPHBUDDY;
		pInst->m_VStreamMask_Stream |= VSM_MORPHBUDDY;
	}

	InputLayoutHandle eVF = VertFormatForComponents(bCol, bTC0, bPSize, bNormal != 0);
	pInst->m_nVertexFormat = eVF;
}

InputLayoutHandle CHWShader_D3D::mfVertexFormat(bool& bUseTangents, bool& bUseLM, bool& bUseHWSkin)
{
	uint32 i;

	assert(m_eSHClass == eHWSC_Vertex);

	InputLayoutHandle eVFormat = EDefaultInputLayouts::P3F_C4B_T2F;
	int nStream = 0;
	for (i = 0; i < m_Insts.size(); i++)
	{
		SHWSInstance* pInst = m_Insts[i];
		eVFormat = (InputLayoutHandle)max((uint32)eVFormat, (uint32)pInst->m_nVertexFormat);
		nStream |= pInst->m_VStreamMask_Stream;
	}
	bUseTangents = (nStream & VSM_TANGENTS) != 0;
	bUseLM = false;
	bUseHWSkin = (nStream & VSM_HWSKIN) != 0;
	assert(eVFormat < EDefaultInputLayouts::PreAllocated);

	return eVFormat;
}

InputLayoutHandle CHWShader_D3D::mfVertexFormat(SHWSInstance* pInst, CHWShader_D3D* pSH, D3DBlob* pShader, void* pConstantTable)
{
	/*if (!stricmp(pSH->m_EntryFunc.c_str(), "ParticleVS"))
	   {
	   int nnn = 0;
	   }*/

	assert(pSH->m_eSHClass == eHWSC_Vertex);

	byte bNormal = false;
	bool bTangent[2] = { false, false };
	bool bBitangent[2] = { false, false };
	bool bHWSkin = false;
	bool bVelocity = false;
	bool bMorph = false;
	bool bBoneSpace = false;
	bool bPSize = false;
	bool bSH[2] = { false, false };
	bool bTC0 = false;
	bool bTC1[2] = { false, false };
	bool bCol = false;
	bool bSecCol = false;
	bool bPos = false;
	InputLayoutHandle eVFormat = EDefaultInputLayouts::P3F_C4B_T2F;

	size_t nSize = pShader->GetBufferSize();
	void* pData = pShader->GetBufferPointer();
	void* pShaderReflBuf = pConstantTable;
	HRESULT hr = pConstantTable ? S_OK : D3DReflect(pData, nSize, IID_ID3D11ShaderReflection, &pShaderReflBuf);
	assert(SUCCEEDED(hr));
	ID3D11ShaderReflection* pShaderReflection = (ID3D11ShaderReflection*)pShaderReflBuf;
	if (!SUCCEEDED(hr))
		return InputLayoutHandle::Unspecified;
	D3D11_SHADER_DESC Desc;
	pShaderReflection->GetDesc(&Desc);
	if (!Desc.InputParameters)
		return InputLayoutHandle::Unspecified;
	D3D11_SIGNATURE_PARAMETER_DESC IDesc;
	for (uint32 i = 0; i < Desc.InputParameters; i++)
	{
		pShaderReflection->GetInputParameterDesc(i, &IDesc);
		//if (!IDesc.ReadWriteMask)
		//  continue;
		if (!IDesc.SemanticName)
			continue;
		int nIndex;
		if (!strnicmp(IDesc.SemanticName, "POSITION", 8) || !strnicmp(IDesc.SemanticName, "SV_POSITION", 11))
		{
			nIndex = IDesc.SemanticIndex;
			if (nIndex == 0)
				bPos = true;
			else if (nIndex == 3)
				bVelocity = true;
			else if (nIndex == 4)
				bHWSkin = true;
			else if (nIndex == 8)
				bMorph = true;
			else
				assert(false);
		}
		else if (!strnicmp(IDesc.SemanticName, "NORMAL", 6))
		{
			bNormal = true;
		}
		else if (!strnicmp(IDesc.SemanticName, "TEXCOORD", 8))
		{
			nIndex = IDesc.SemanticIndex;
			if (nIndex == 0)
				bTC0 = true;
			else
			{
				if (nIndex == 1)
				{
					bTC1[0] = true;
					if (IDesc.ReadWriteMask)
						bTC1[1] = true;
				}
				else if (nIndex == 8)
					bMorph = true;
			}
		}
		else if (!strnicmp(IDesc.SemanticName, "COLOR", 5))
		{
			nIndex = IDesc.SemanticIndex;
			if (nIndex == 0)
				bCol = true;
			else if (nIndex == 1)
				bSecCol = true;
			else
			{
				if (nIndex == 2 || nIndex == 3)
				{
					bSH[0] = true;
					if (IDesc.ReadWriteMask)
						bSH[1] = true;
				}
				else
					assert(false);
			}
		}
		else if (!stricmp(IDesc.SemanticName, "TANGENT"))
		{
			bTangent[0] = true;
			if (IDesc.ReadWriteMask)
				bTangent[1] = true;
		}
		else if (!stricmp(IDesc.SemanticName, "BITANGENT"))
		{
			bBitangent[0] = true;
			if (IDesc.ReadWriteMask)
				bBitangent[1] = true;
		}
		else if (!strnicmp(IDesc.SemanticName, "PSIZE", 5) || !strnicmp(IDesc.SemanticName, "AXIS", 4))
		{
			bPSize = true;
		}
		else if (!strnicmp(IDesc.SemanticName, "BLENDWEIGHT", 11) || !strnicmp(IDesc.SemanticName, "BLENDINDICES", 12))
		{
			nIndex = IDesc.SemanticIndex;
			if (nIndex == 0)
				bHWSkin = true;
			else if (nIndex == 1)
				bMorph = true;
			else
				assert(0);
		}
		else if (!strnicmp(IDesc.SemanticName, "SV_", 3))
		{
			// SV_ are valid semantics
		}
#if CRY_PLATFORM_ORBIS
		else if (!strnicmp(IDesc.SemanticName, "S_VERTEX_ID", 11) || !strnicmp(IDesc.SemanticName, "S_INSTANCE_ID", 13))
		{
			// S_VERTEX_ID and S_INSTANCE_ID are valid names
		}
#endif
		else
		{
			CRY_ASSERT_TRACE(0, ("Invalid SemanticName %s", IDesc.SemanticName));
		}

#if CRY_RENDERER_VULKAN
		pInst->m_VSInputStreams.emplace_back(IDesc.SemanticName, IDesc.SemanticIndex, IDesc.AttributeLocation);
#endif
	}

	mfPostVertexFormat(pInst, pSH, bCol, bNormal, bTC0, bTC1, bPSize, bTangent, bBitangent, bHWSkin, bSH, bVelocity, bMorph);
	
	if (pConstantTable != pShaderReflection)
	{
		SAFE_RELEASE(pShaderReflection);
	}

	return (InputLayoutHandle)pInst->m_nVertexFormat;
}

void CHWShader_D3D::mfSetDefaultRT(uint64& nAndMask, uint64& nOrMask)
{
	uint32 i, j;
	SShaderGen* pGen = gRenDev->m_cEF.m_pGlobalExt;

	uint32 nBitsPlatform = 0;
	if (CParserBin::m_nPlatform == SF_ORBIS)
		nBitsPlatform |= SHGD_HW_ORBIS;
	else if (CParserBin::m_nPlatform == SF_DURANGO)
		nBitsPlatform |= SHGD_HW_DURANGO;
	else if (CParserBin::m_nPlatform == SF_D3D11)
		nBitsPlatform |= SHGD_HW_DX11;
	else if (CParserBin::m_nPlatform == SF_GL4)
		nBitsPlatform |= SHGD_HW_GL4;
	else if (CParserBin::m_nPlatform == SF_GLES3)
		nBitsPlatform |= SHGD_HW_GLES3;
	else if (CParserBin::m_nPlatform == SF_VULKAN)
		nBitsPlatform |= SHGD_HW_VULKAN;

	// Make a mask of flags affected by this type of shader
	uint32 nType = m_dwShaderType;
	if (nType)
	{
		for (i = 0; i < pGen->m_BitMask.size(); i++)
		{
			SShaderGenBit* pBit = pGen->m_BitMask[i];
			if (!pBit->m_Mask)
				continue;
			if (nBitsPlatform & pBit->m_nDependencyReset)
			{
				nAndMask &= ~pBit->m_Mask;
				continue;
			}
			for (j = 0; j < pBit->m_PrecacheNames.size(); j++)
			{
				if (pBit->m_PrecacheNames[j] == nType)
				{
					if (nBitsPlatform & pBit->m_nDependencySet)
						nOrMask |= pBit->m_Mask;
					break;
				}
			}
		}
	}
}

//==================================================================================================================

static bool sGetMask(char* str, SShaderGen* pGen, uint64& nMask)
{
	uint32 i;

	for (i = 0; i < pGen->m_BitMask.Num(); i++)
	{
		SShaderGenBit* pBit = pGen->m_BitMask[i];
		if (!strcmp(str, pBit->m_ParamName.c_str()))
		{
			nMask |= pBit->m_Mask;
			return true;
		}
	}
	return false;
}

void CHWShader::mfValidateTokenData(CResFile* pRes)
{
#ifdef _DEBUG
	if (pRes == 0)
		return;

	bool bTokenValid = true;
	ResDir* Dir = pRes->mfGetDirectory();
	for (unsigned int i = 0; i < Dir->size(); i++)
	{
		CDirEntry* pDE = &(*Dir)[i];
		if (pDE->GetFlags() & RF_RES_$TOKENS)
		{
			uint32 nSize = pRes->mfFileRead(pDE);
			byte* pData = (byte*)pRes->mfFileGetBuf(pDE);
			if (!pData)
			{
				bTokenValid = false;
				break;
			}

			uint32 nL = *(uint32*)pData;
			if (CParserBin::m_bEndians)
				SwapEndian(nL, eBigEndian);

			if (nL * sizeof(uint32) > nSize)
			{
				bTokenValid = false;
				break;
			}

			int nTableSize = nSize - (4 + nL * sizeof(uint32));
			if (nTableSize < 0)
			{
				bTokenValid = false;
				break;
			}

			pRes->mfCloseEntry(pDE->GetName(), pDE->GetFlags());
		}
	}

	if (!bTokenValid)
		CryFatalError("Invalid token data in shader cache file");
#endif
}

void CHWShader::mfValidateDirEntries(CResFile* pRF)
{
#ifdef _DEBUG
	auto dirEntries = pRF->mfGetDirectory();
	for (auto it1 = dirEntries->begin(); it1 != dirEntries->end(); it1++)
	{
		for (auto it2 = it1 + 1; it2 != dirEntries->end(); ++it2)
		{
			if (it1->GetName() == it2->GetName())
			{
				CryFatalError("Duplicate dir entry in shader cache file");
			}
		}
	}
#endif
}

bool CHWShader::mfWriteoutTokensToCache()
{
	if (!m_CachedTokens.size())
		return true;

	char szName[256];
#if defined(__GNUC__)
	cry_sprintf(szName, "$MAP_%llx", m_nMaskGenFX);
#else
	cry_sprintf(szName, "$MAP_%I64x", m_nMaskGenFX);
#endif

	auto cache = AcquireDiskCache(cacheSource::user);
	CRY_ASSERT_MESSAGE(cache && cache->m_pRes, "Could not acquire user disk cache");
	if (cache && cache->m_pRes)
	{
		CDirEntry de(szName, m_CachedTokens.size(), RF_RES_$TOKENS);
		cache->m_pRes->mfFileAdd(&de);
		SDirEntryOpen* pOE = cache->m_pRes->mfOpenEntry(de.GetName());
		pOE->pData = const_cast<char*>(m_CachedTokens.data());
		cache->m_pRes->mfFlush();
		cache->m_pRes->mfCloseEntry(de.GetName(), de.GetFlags());

		return true;
	}

	return false;
}

bool CHWShader_D3D::mfStoreCacheTokenMap(const FXShaderToken& Table, const TArray<uint32>& SHData)
{
	TArray<byte> Data;

	FXShaderTokenItor itor;
	uint32 nSize = SHData.size();
	if (CParserBin::m_bEndians)
	{
		uint32 nSizeEnd = nSize;
		SwapEndian(nSizeEnd, eBigEndian);
		Data.Copy((byte*)&nSizeEnd, sizeof(uint32));
		for (uint32 i = 0; i < nSize; i++)
		{
			uint32 nToken = SHData[i];
			SwapEndian(nToken, eBigEndian);
			Data.Copy((byte*)&nToken, sizeof(uint32));
		}
	}
	else
	{
		Data.Copy((byte*)&nSize, sizeof(uint32));
		Data.Copy((byte*)&SHData[0], nSize * sizeof(uint32));
	}
	for (auto T : Table)
	{
		if (CParserBin::m_bEndians)
			SwapEndian(T.Token, eBigEndian);
		Data.Copy((byte*)&T.Token, sizeof(DWORD));
		Data.Copy((byte*)T.SToken.c_str(), T.SToken.size() + 1);
	}
	if (!Data.size())
		return false;

	m_CachedTokens.resize(Data.size());
	std::memcpy(&m_CachedTokens[0], Data.Data(), Data.size());

	mfWriteoutTokensToCache();

	return true;
}

bool CHWShader_D3D::mfGetCacheTokenMap(FXShaderToken& Table, TArray<uint32>& SHData)
{
	std::size_t nSize = m_CachedTokens.size();
	const auto* pData = reinterpret_cast<const byte*>(m_CachedTokens.data());
	if (!pData)
	{
		Table = {};
		return false;
	}

	uint32 nL = *(uint32*)pData;
	if (CParserBin::m_bEndians)
		SwapEndian(nL, eBigEndian);
	SHData.resize(nL);
	if (CParserBin::m_bEndians)
	{
		uint32* pTokens = (uint32*)&pData[4];
		for (uint32_t i = 0; i < nL; i++)
		{
			uint32 nToken = pTokens[i];
			SwapEndian(nToken, eBigEndian);
			SHData[i] = nToken;
		}
	}
	else
	{
		memcpy(&SHData[0], &pData[4], nL * sizeof(uint32));
	}
	pData += 4 + nL * sizeof(uint32);
	nSize -= 4 + nL * sizeof(uint32);
	int nOffs = 0;

	while (nOffs < nSize)
	{
		char* pStr = (char*)&pData[nOffs + sizeof(DWORD)];
		DWORD nToken;
		LoadUnaligned(pData + nOffs, nToken);
		if (CParserBin::m_bEndians)
			SwapEndian(nToken, eBigEndian);
		int nLen = strlen(pStr) + 1;
		STokenD TD;
		TD.Token = nToken;
		TD.SToken = pStr;
		Table.push_back(TD);
		nOffs += sizeof(DWORD) + nLen;
	}

	return true;
}

//==============================================================================================================================================================

bool CHWShader_D3D::mfGenerateScript(CShader* pSH, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, uint32 nFlags, TArray<char>& sNewScr)
{
	uint32 nSFlags = m_Flags;
	FXShaderToken Table;
	TArray<uint32> SHData;

	if (CParserBin::m_bEditable) // Fast path for offline shaders builder
	{
		Table = m_TokenTable;
		SHData = m_TokenData;
	}
	else
	{
		if (!(nSFlags & HWSG_GS_MULTIRES))
		{
			if (!mfGetCacheTokenMap(Table, SHData))
				return false;
		}
		if (CParserBin::m_bEditable)
		{
			Table = m_TokenTable;
			SHData = m_TokenData;
		}
	}
	if (!(nSFlags & HWSG_GS_MULTIRES))
	{
		assert(Table.size() && SHData.size());
		if (!Table.size() || !SHData.size())
			return false;
	}

	ShaderTokensVec NewTokens;

	uint32 eT = eT_unknown;

	switch (pInst->m_eClass)
	{
	case eHWSC_Vertex:
		eT = eT__VS;
		break;
	case eHWSC_Pixel:
		eT = eT__PS;
		break;
	case eHWSC_Geometry:
		eT = eT__GS;
		break;
	case eHWSC_Hull:
		eT = eT__HS;
		break;
	case eHWSC_Compute:
		eT = eT__CS;
		break;
	case eHWSC_Domain:
		eT = eT__DS;
		break;

	default:
		assert(0);
	}
	if (eT != eT_unknown)
		CParserBin::AddDefineToken(eT, NewTokens);

	if (nSFlags & HWSG_GS_MULTIRES)
	{
		// Generate script vor VS first;
		//@TODO: Do this without global variable
		//pInst = s_pCurInstVS;
		assert(pInst);

		CHWShader_D3D* curVS = (CHWShader_D3D *)s_pCurHWVS;
		Table = curVS->m_TokenTable;
		SHData = curVS->m_TokenData;
		nSFlags = curVS->m_Flags;
	}

	// Include runtime mask definitions in the script
	SShaderGen* shg = gRenDev->m_cEF.m_pGlobalExt;
	if (shg && pInst->m_Ident.m_RTMask)
	{
		for (uint32 i = 0; i < shg->m_BitMask.Num(); i++)
		{
			SShaderGenBit* bit = shg->m_BitMask[i];
			if (!(bit->m_Mask & pInst->m_Ident.m_RTMask))
				continue;
			CParserBin::AddDefineToken(bit->m_dwToken, NewTokens);
		}
	}

	// Include light mask definitions in the script
	if (nSFlags & HWSG_SUPPORTS_MULTILIGHTS)
	{
		int nLights = pInst->m_Ident.m_LightMask & 0xf;
		if (nLights)
			CParserBin::AddDefineToken(eT__LT_LIGHTS, NewTokens);
		CParserBin::AddDefineToken(eT__LT_NUM, nLights + eT_0, NewTokens);
		bool bHasProj = false;
		for (int i = 0; i < 4; i++)
		{
			int nLightType = (pInst->m_Ident.m_LightMask >> (SLMF_LTYPE_SHIFT + i * SLMF_LTYPE_BITS)) & SLMF_TYPE_MASK;
			if (nLightType == SLMF_PROJECTED)
				bHasProj = true;

			CParserBin::AddDefineToken(eT__LT_0_TYPE + i, nLightType + eT_0, NewTokens);
		}
		if (bHasProj)
			CParserBin::AddDefineToken(eT__LT_HASPROJ, eT_1, NewTokens);
	}
	else if (nSFlags & HWSG_SUPPORTS_LIGHTING)
	{
		CParserBin::AddDefineToken(eT__LT_LIGHTS, NewTokens);
		int nLightType = (pInst->m_Ident.m_LightMask >> SLMF_LTYPE_SHIFT) & SLMF_TYPE_MASK;
		if (nLightType == SLMF_PROJECTED)
			CParserBin::AddDefineToken(eT__LT_HASPROJ, eT_1, NewTokens);
	}

	// Include modificator mask definitions in the script
	if ((nSFlags & HWSG_SUPPORTS_MODIF) && pInst->m_Ident.m_MDMask)
	{
		const uint32 tcProjMask = HWMD_TEXCOORD_PROJ;
		const uint32 tcMatrixMask = HWMD_TEXCOORD_MATRIX;
		const uint32 tcGenObjectLinearMask = HWMD_TEXCOORD_GEN_OBJECT_LINEAR;

		if (pInst->m_Ident.m_MDMask & tcProjMask)
			CParserBin::AddDefineToken(eT__TT_TEXCOORD_PROJ, NewTokens);
		if (pInst->m_Ident.m_MDMask & tcMatrixMask)
			CParserBin::AddDefineToken(eT__TT_TEXCOORD_MATRIX, NewTokens);
		if (pInst->m_Ident.m_MDMask & tcGenObjectLinearMask)
			CParserBin::AddDefineToken(eT__TT_TEXCOORD_GEN_OBJECT_LINEAR, NewTokens);
	}

	// Include vertex modificator mask definitions in the script
	if ((nSFlags & HWSG_SUPPORTS_VMODIF) && pInst->m_Ident.m_MDVMask)
	{
		int nMDV = pInst->m_Ident.m_MDVMask & 0x0fffffff;
		int nType = nMDV & 0xf;
		if (nType)
			CParserBin::AddDefineToken(eT__VT_TYPE, eT_0 + nType, NewTokens);
		if ((nMDV & MDV_BENDING) || nType == eDT_Bending)
		{
			CParserBin::AddDefineToken(eT__VT_BEND, eT_1, NewTokens);
			if (!(nMDV & 0xf))
			{
				nType = eDT_Bending;
				CParserBin::AddDefineToken(eT__VT_TYPE, eT_0 + nType, NewTokens);
			}
		}
		if (nMDV & MDV_DEPTH_OFFSET)
			CParserBin::AddDefineToken(eT__VT_DEPTH_OFFSET, eT_1, NewTokens);
		if (nMDV & MDV_WIND)
			CParserBin::AddDefineToken(eT__VT_WIND, eT_1, NewTokens);
		if (nMDV & MDV_DET_BENDING)
			CParserBin::AddDefineToken(eT__VT_DET_BEND, eT_1, NewTokens);
		if (nMDV & MDV_DET_BENDING_GRASS)
			CParserBin::AddDefineToken(eT__VT_GRASS, eT_1, NewTokens);
		if (nMDV & ~0xf)
			CParserBin::AddDefineToken(eT__VT_TYPE_MODIF, eT_1, NewTokens);
	}

	if (nSFlags & HWSG_FP_EMULATION)
	{
		CParserBin::AddDefineToken(eT__FT0_COP, eT_0 + (pInst->m_Ident.m_LightMask & 0xff), NewTokens);
		CParserBin::AddDefineToken(eT__FT0_AOP, eT_0 + ((pInst->m_Ident.m_LightMask & 0xff00) >> 8), NewTokens);

		byte CO_0 = ((pInst->m_Ident.m_LightMask & 0xff0000) >> 16) & 7;
		CParserBin::AddDefineToken(eT__FT0_CARG1, eT_0 + CO_0, NewTokens);

		byte CO_1 = ((pInst->m_Ident.m_LightMask & 0xff0000) >> 19) & 7;
		CParserBin::AddDefineToken(eT__FT0_CARG2, eT_0 + CO_1, NewTokens);

		byte AO_0 = ((pInst->m_Ident.m_LightMask & 0xff000000) >> 24) & 7;
		CParserBin::AddDefineToken(eT__FT0_AARG1, eT_0 + AO_0, NewTokens);

		byte AO_1 = ((pInst->m_Ident.m_LightMask & 0xff000000) >> 27) & 7;
		CParserBin::AddDefineToken(eT__FT0_AARG2, eT_0 + AO_1, NewTokens);

		if (CO_0 == eCA_Specular || CO_1 == eCA_Specular || AO_0 == eCA_Specular || AO_1 == eCA_Specular)
			CParserBin::AddDefineToken(eT__FT_SPECULAR, NewTokens);
		if (CO_0 == eCA_Diffuse || CO_1 == eCA_Diffuse || AO_0 == eCA_Diffuse || AO_1 == eCA_Diffuse)
			CParserBin::AddDefineToken(eT__FT_DIFFUSE, NewTokens);
		if (CO_0 == eCA_Texture || CO_1 == eCA_Texture || AO_0 == eCA_Texture || AO_1 == eCA_Texture)
			CParserBin::AddDefineToken(eT__FT_TEXTURE, NewTokens);
		if (CO_0 == eCA_Texture1 || (CO_1 == eCA_Texture1) || AO_0 == eCA_Texture1 || AO_1 == eCA_Texture1
		    || CO_0 == eCA_Previous || (CO_1 == eCA_Previous) || AO_0 == eCA_Previous || AO_1 == eCA_Previous)
			CParserBin::AddDefineToken(eT__FT_TEXTURE1, NewTokens);
		if (CO_0 == eCA_Normal || CO_1 == eCA_Normal || AO_0 == eCA_Normal || AO_1 == eCA_Normal)
			CParserBin::AddDefineToken(eT__FT_NORMAL, NewTokens);
		if (CO_0 == eCA_Constant || (CO_1 == eCA_Constant) || AO_0 == eCA_Constant || AO_1 == eCA_Constant
		    || CO_0 == eCA_Previous || (CO_1 == eCA_Previous) || AO_0 == eCA_Previous || AO_1 == eCA_Previous)
			CParserBin::AddDefineToken(eT__FT_PSIZE, NewTokens);

		if (nFlags & HWSF_STOREDATA)
		{
			int nStreams = pInst->m_Ident.m_LightMask & 0xff;
			if (nStreams & (1 << VSF_QTANGENTS))
				CParserBin::AddDefineToken(eT__FT_QTANGENT_STREAM, NewTokens);
			if (nStreams & (1 << VSF_TANGENTS))
				CParserBin::AddDefineToken(eT__FT_TANGENT_STREAM, NewTokens);
			if (nStreams & VSM_HWSKIN)
				CParserBin::AddDefineToken(eT__FT_SKIN_STREAM, NewTokens);
#if ENABLE_NORMALSTREAM_SUPPORT
			if (CParserBin::m_nPlatform & (SF_D3D11 | SF_DURANGO | SF_ORBIS | SF_GL4 | SF_GLES3))
			{
				if (nStreams & VSM_NORMALS)
					CParserBin::AddDefineToken(eT__FT_NORMAL, NewTokens);
			}
#endif
			if (nStreams & VSM_VERTEX_VELOCITY)
				CParserBin::AddDefineToken(eT__FT_VERTEX_VELOCITY_STREAM, NewTokens);
		}
	}

	int nT = NewTokens.size();
	NewTokens.resize(nT + SHData.size());
	memcpy(&NewTokens[nT], &SHData[0], SHData.size() * sizeof(uint32));

	CParserBin Parser(nullptr, pSH);
	Parser.Preprocess(1, NewTokens, Table);
	CorrectScriptEnums(Parser, pInst, InstBindVars, Table);
	RemoveUnaffectedParameters_D3D10(Parser, pInst, InstBindVars);
	AddResourceLayoutToBinScript(Parser, pInst, &Table);
	ConvertBinScriptToASCII(Parser, pInst, InstBindVars, Table, sNewScr);
	AddResourceLayoutToScriptHeader(pInst, mfProfileString(pInst->m_eClass), m_EntryFunc.c_str(), sNewScr);

	// Generate geometry shader
	if (m_Flags & HWSG_GS_MULTIRES)
	{
		bool bResult = AutoGenMultiresGS(sNewScr, pSH);
		if (!bResult)
			return false;
	}

	return sNewScr.Num() && sNewScr[0];
}

bool CHWShader_D3D::AutoGenMultiresGS(TArray<char>& sNewScr, CShader *pSH)
{
	CHWShader_D3D* curVS = (CHWShader_D3D *)s_pCurHWVS;
	char szEntryVS[128];
	strcpy(szEntryVS, curVS->m_EntryFunc.c_str());
	strcat(szEntryVS, "(");
	char *szStart = strstr(&sNewScr[0], szEntryVS);
	assert(szStart);
	if (szStart)
	{
		char *szEnd = szStart - 1;
		while (*szEnd == 0x20) --szEnd;
		char *szS = szEnd;
		while (*szS > 0x20) --szS;
		char szStrName[128];
		ptrdiff_t nSize = szEnd - szS;
		strncpy(szStrName, &szS[1], nSize);
		szStrName[nSize] = 0;

		char szStruct[128];
		strcpy(szStruct, "struct ");
		strcat(szStruct, szStrName);
		char *szStrStart = strstr(&sNewScr[0], szStruct);
		assert(szStrStart);

		char *szStrEnd = strstr(szStrStart, "};");
		szStrEnd += 2;

		char szPosName[128];
		char *szPosA = strstr(szStrStart, ":POSITION");
		if (!szPosA || szPosA >= szStrEnd)
			szPosA = strstr(szStrStart, ":SV_POSITION");
		if (!szPosA || szPosA >= szStrEnd)
		{
#if !defined(_RELEASE)
			CRY_ASSERT_MESSAGE(false, "Cannot generate a GS for a VS with no SV_POSITION output");
#endif
			return false;
		}
		char *szPosAB = szPosA - 1;
		while (*szPosAB == 0x20) --szPosAB;
		char *szP = szPosAB;
		while (*szP > 0x20) --szP;
		nSize = szPosAB - szP;
		strncpy(szPosName, &szP[1], nSize);
		szPosName[nSize] = 0;

		TArray<char> szNewS;
		//szNewS.Copy(&sNewScr[0], szStrStart - &sNewScr[0]);
		szNewS.Copy(szStrStart, uint32(szStrEnd-szStrStart)+1);

		m_EntryFunc.Format("%s_GS", curVS->m_EntryFunc.c_str());

		string GSDefine;
		GSDefine.Format(
			"#define NV_VR_FASTGS_FUNCTION_NAME %s\n"
			"#define NV_VR_FASTGS_PASSTHROUGH_STRUCT %s\n"
			"#define NV_VR_FASTGS_OUTPUT_STRUCT %s_gs\n"
			"#define NV_VR_FASTGS_POSITION_ATTRIBUTE %s\n\n",
			m_EntryFunc.c_str(), szStrName, szStrName, szPosName);
		szNewS.Copy(GSDefine.c_str(), GSDefine.size());

		ShaderTokensVec NewTokens;

		uint32 eT = eT__GS;
		CParserBin::AddDefineToken(eT, NewTokens);

		// Include runtime mask definitions in the script
		SShaderGen* shg = gRenDev->m_cEF.m_pGlobalExt;
		if (shg && m_pCurInst->m_Ident.m_RTMask)
		{
			for (uint32 i = 0; i < shg->m_BitMask.Num(); i++)
			{
				SShaderGenBit* bit = shg->m_BitMask[i];
				if (!(bit->m_Mask & m_pCurInst->m_Ident.m_RTMask))
					continue;
				CParserBin::AddDefineToken(bit->m_dwToken, NewTokens);
			}
		}
		int nT = NewTokens.size();
		NewTokens.resize(nT + m_TokenData.size());
		memcpy(&NewTokens[nT], &m_TokenData[0], m_TokenData.size() * sizeof(uint32));

		std::vector<SCGBind> InstBindVars;
		CParserBin Parser(nullptr, pSH);
		Parser.Preprocess(1, NewTokens, m_TokenTable);
		ConvertBinScriptToASCII(Parser, m_pCurInst, InstBindVars, m_TokenTable, szNewS);

		sNewScr.Copy(szNewS);
	}
	else
		return false;

	return true;
}

void CHWShader_D3D::RemoveUnaffectedParameters_D3D10(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars)
{
	int nPos = Parser.FindToken(0, Parser.m_Tokens.size() - 1, eT_cbuffer);
	while (nPos >= 0)
	{
		uint32 nName = Parser.m_Tokens[nPos + 1];
		if (nName == eT_PER_BATCH || nName == eT_PER_INSTANCE)
		{
			int nPosEnd = Parser.FindToken(nPos + 3, Parser.m_Tokens.size() - 1, eT_br_cv_2);
			assert(nPosEnd >= 0);
			int nPosN = Parser.FindToken(nPos + 1, Parser.m_Tokens.size() - 1, eT_br_cv_1);
			assert(nPosN >= 0);
			nPosN++;
			while (nPosN < nPosEnd)
			{
				uint32 nT = Parser.m_Tokens[nPosN + 1];
				int nPosCode = Parser.FindToken(nPosEnd + 1, Parser.m_Tokens.size() - 1, nT);
				if (nPosCode < 0)
				{
					assert(nPosN > 0 && nPosN < (int)Parser.m_Tokens.size());
					if (InstBindVars.size())
					{
						size_t i = 0;
						CCryNameR nm(Parser.GetString(nT));
						for (; i < InstBindVars.size(); i++)
						{
							SCGBind& b = InstBindVars[i];
							if (b.m_Name == nm)
								break;
						}
						if (i == InstBindVars.size())
							Parser.m_Tokens[nPosN] = eT_comment;
					}
					else
						Parser.m_Tokens[nPosN] = eT_comment;
				}
				nPosN = Parser.FindToken(nPosN + 2, nPosEnd, eT_semicolumn);
				assert(nPosN >= 0);
				nPosN++;
			}
			nPos = Parser.FindToken(nPosEnd + 1, Parser.m_Tokens.size() - 1, eT_cbuffer);
		}
		else
			nPos = Parser.FindToken(nPos + 2, Parser.m_Tokens.size() - 1, eT_cbuffer);
	}
	//#else
	/*int nStart = 0;
	   while (true)
	   {
	   uint32 nName = sFindVar(Parser, nStart);
	   if (nStart < 0)
	    break;
	   bool bAffect = sIsAffectFuncs(Parser, nName);
	   if (!bAffect)
	    Parser.m_Tokens[nStart] = eT_comment;
	   nStart++;
	   }*/
	//#endif
}

#if CRY_RENDERER_VULKAN
std::string resourceTypeToString(SResourceBindPoint::ESlotType type)
{
	switch (type)
	{
	case SResourceBindPoint::ESlotType::ConstantBuffer:
		return "b";
	case SResourceBindPoint::ESlotType::Sampler:
		return "s";
	case SResourceBindPoint::ESlotType::TextureAndBuffer:
		return "t";
	case SResourceBindPoint::ESlotType::UnorderedAccessView:
		return "u";
	default:
		CRY_ASSERT_MESSAGE(false, "Type is not defined.");
		return "";
	};
};

struct ResourceSetBindingInfo
{
	uint8 set;
	uint8 binding;
	SResourceBindPoint::ESlotType type;

	ResourceSetBindingInfo(uint8 s = (uint8)~0u, uint8 b = (uint8)~0u, SResourceBindPoint::ESlotType t = SResourceBindPoint::ESlotType::ConstantBuffer)
		: set(s), binding(b), type(t) {}

	std::string ToString() const
	{
		return resourceTypeToString(type) + std::to_string(binding) + ", space" + std::to_string(set);
	}
};

uint32 AddNewTableEntry(FXShaderToken& tokenTable, const char * newTokenStr)
{
	uint32 nToken = CParserBin::GetCRC32(newTokenStr);
	FXShaderTokenItor itor = std::lower_bound(tokenTable.begin(), tokenTable.end(), nToken, SortByToken());
	if (itor != tokenTable.end() && (*itor).Token == nToken)
	{
		assert(!strcmp((*itor).SToken.c_str(), newTokenStr));
		return nToken;
	}
	STokenD TD;
	TD.SToken = newTokenStr;
	TD.Token = nToken;
	tokenTable.insert(itor, TD);

	return nToken;
}

struct SRegisterRangeDesc
{
	SRegisterRangeDesc()
		: type(SResourceBindPoint::ESlotType::InvalidSlotType)
		, start(0)
		, count(0)
		, shaderStageMask(0)
	{}

	void setTypeAndStage(uint8_t typeAndStageByte)
	{
		shaderStageMask = typeAndStageByte & 0x3F;
		type = (SResourceBindPoint::ESlotType)(typeAndStageByte >> 6);
	}

	void setSlotNumberAndDescCount(uint8_t slotNumberAndDescCount)
	{
		start = slotNumberAndDescCount & 0x3F;
		count = slotNumberAndDescCount >> 6;
	}

	SResourceBindPoint::ESlotType type;
	uint32_t           start;
	uint32_t           count;
	uint32_t           shaderStageMask;
};

typedef std::vector<std::vector<SRegisterRangeDesc>> RegisterRanges;

RegisterRanges ExtractRegisterRanges(const std::vector<uint8>& RsrcLayoutEncoding)
{
	RegisterRanges registerRanges;

	if (RsrcLayoutEncoding.size() > 0)
	{
		const uint8_t* pLayoutData = (&RsrcLayoutEncoding[0]);

		int setCount = *pLayoutData++;
		registerRanges.resize(setCount);

		for (int i = 0; i < setCount; i++)
		{
			int rangeCount = *pLayoutData++;
			registerRanges[i].resize(rangeCount);

			for (int j = 0; j < rangeCount; j++)
			{
				uint8_t slotTypeStagesByte = *pLayoutData++;
				uint8_t slotNumberDescCountByte = *pLayoutData++;
				registerRanges[i][j].setTypeAndStage(slotTypeStagesByte);
				registerRanges[i][j].setSlotNumberAndDescCount(slotNumberDescCountByte);
			}
		}
	}

	return registerRanges;
}

extern VkShaderStageFlags GetShaderStageFlags(EShaderStage shaderStages);
#endif

void CHWShader_D3D::AddResourceLayoutToBinScript(CParserBin & Parser, SHWSInstance* pInst, FXShaderToken* Table)
{
#if CRY_RENDERER_VULKAN

	if (CRendererCVars::CV_r_VkShaderCompiler && strcmp(CRendererCVars::CV_r_VkShaderCompiler->GetString(), STR_VK_SHADER_COMPILER_HLSLCC) == 0)
		return;

	// Extract mapping from HLSL register definition to vulkan descriptor set and binding.
	// The mapping is stored in vkResourceMapping and will be used later for modifying the shaders.
	std::map<std::string, ResourceSetBindingInfo> vkResourceMapping;
	const std::vector<uint8>* LayoutEncoding = GetDeviceObjectFactory().LookupResourceLayoutEncoding(pInst->m_Ident.m_pipelineState.VULKAN.resourceLayoutHash);
	if (LayoutEncoding)
	{
		RegisterRanges registerRanges = ExtractRegisterRanges(*LayoutEncoding);

		uint32_t bindingIndex;
		uint32_t setIndex;
		for (setIndex = 0; setIndex < (uint32_t)registerRanges.size(); setIndex++)
		{
			bindingIndex = 0;
			for (size_t i = 0; i < registerRanges[setIndex].size(); i++)
			{
				const SRegisterRangeDesc& rangeDesc = registerRanges[setIndex][i];

				for (uint32_t registerIdxOffset = 0; registerIdxOffset < rangeDesc.count; ++registerIdxOffset)
				{
					if (GetShaderStageFlags(SHADERSTAGE_FROM_SHADERCLASS(pInst->m_eClass)) & rangeDesc.shaderStageMask)
					{
						std::string resourceRegisterName = resourceTypeToString(rangeDesc.type);
						resourceRegisterName += std::to_string(rangeDesc.start + registerIdxOffset);
						vkResourceMapping[resourceRegisterName] = ResourceSetBindingInfo(setIndex, bindingIndex, rangeDesc.type);
					}
				}

				bindingIndex += rangeDesc.count;
			}
		}
	}

	// Check for all the used registers in the shader code and replace them with vulkan descriptor set and binding
	// if they were part of resource layout description.
	int nCur = 0;
	int nSize = Parser.m_Tokens.size();
	const uint32* pTokens = &Parser.m_Tokens[0];
	std::map<uint32, uint32> alreadyProcessed;
	while (true)
	{
		// Look for "register(*)" in the shader code.
		nCur = Parser.FindToken(nCur, nSize - 1, eT_register);
		if (nCur < 0)
			break;
		int32 OpenBacketToken = Parser.FindToken(nCur, nSize - 1, eT_br_rnd_1);
		int32 CloseBacketToken = Parser.FindToken(nCur, nSize - 1, eT_br_rnd_2);
		assert(OpenBacketToken >= 0 && CloseBacketToken >= 0);
		if (CloseBacketToken < 0 || OpenBacketToken < 0)
			break;
		
		uint32_t BegRegisterToken = (uint32_t)OpenBacketToken + 1;
		uint32_t EndRegisterToken = (uint32_t)CloseBacketToken;
		auto nToken = CParserBin::NextToken(pTokens, BegRegisterToken, EndRegisterToken);

		// if the register is already hit, use the previously created token for it.
		if (alreadyProcessed.find(nToken) != alreadyProcessed.end())
		{
			Parser.m_Tokens[BegRegisterToken - 1] = alreadyProcessed[nToken];
			nCur = EndRegisterToken;
			continue;
		}
		
		// Map the newly found register if it is necessary.
		uint32 newToken = nToken;
		const char* registerName = Parser.GetString(nToken, *Table, false);
		auto resource = vkResourceMapping.find(registerName);
		if (resource != vkResourceMapping.end())
		{
			std::string newTokenStr = resource->second.ToString();
			newToken = AddNewTableEntry(*Table, newTokenStr.c_str());
			Parser.m_Tokens[BegRegisterToken-1] = newToken;
		}

		alreadyProcessed[nToken] = newToken;
		nCur = EndRegisterToken;
	}
#endif
}

struct SStructData
{
	uint32 m_nName;
	uint32 m_nTCs;
	int    m_nPos;
};

void CHWShader_D3D::CorrectScriptEnums(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, const FXShaderToken& Table)
{
	// correct enumeration of TEXCOORD# interpolators after preprocessing
	int nCur = 0;
	int nSize = Parser.m_Tokens.size();
	uint32* pTokens = &Parser.m_Tokens[0];
	int nInstParam = 0;
	const uint32 Toks[] = { eT_TEXCOORDN, eT_TEXCOORDN_centroid, eT_unknown };

	std::vector<SStructData> SData;
	uint32 i;
	while (true)
	{
		nCur = Parser.FindToken(nCur, nSize - 1, eT_struct);
		if (nCur < 0)
			break;
		int nLastStr = Parser.FindToken(nCur, nSize - 1, eT_br_cv_2);
		assert(nLastStr >= 0);
		if (nLastStr < 0)
			break;
		bool bNested = false;
		for (i = 0; i < SData.size(); i++)
		{
			SStructData& Data = SData[i];
			Data.m_nPos = Parser.FindToken(nCur, nLastStr, Data.m_nName);
			if (Data.m_nPos > 0)
				bNested = true;
		}
		uint32 nName = pTokens[nCur + 1];
		int n = 0;
		while (nCur < nLastStr)
		{
			int nTN = Parser.FindToken(nCur, nLastStr, Toks);
			if (nTN < 0)
			{
				nCur = nLastStr + 1;
				break;
			}
			int nNested = 0;
			if (bNested)
			{
				for (i = 0; i < SData.size(); i++)
				{
					SStructData& Data = SData[i];
					if (Data.m_nPos > 0 && nTN > Data.m_nPos)
						nNested += Data.m_nTCs;
				}
			}
			assert(pTokens[nTN - 1] == eT_colon);
			int nArrSize = 1;
			uint32 nTokName;
			if (pTokens[nTN - 2] == eT_br_sq_2)
			{
				nArrSize = pTokens[nTN - 3] - eT_0;
				if ((unsigned int) nArrSize > 15)
				{
					const char* szArrSize = Parser.GetString(pTokens[nTN - 3], Table);
					nArrSize = szArrSize ? atoi(szArrSize) : 0;
				}
				assert(pTokens[nTN - 4] == eT_br_sq_1);
				//const char *szName = Parser.GetString(pTokens[nTN-5], *Table);
				nTokName = pTokens[nTN - 5];
			}
			else
			{
				uint32 nType = pTokens[nTN - 3];
				assert(nType == eT_float || nType == eT_float2 || nType == eT_float3 || nType == eT_float4 || nType == eT_float4x4 || nType == eT_float3x4 || nType == eT_float2x4 || nType == eT_float3x3 ||
				       nType == eT_half || nType == eT_half2 || nType == eT_half3 || nType == eT_half4 || nType == eT_half4x4 || nType == eT_half3x4 || nType == eT_half2x4 || nType == eT_half3x3);
				if (nType == eT_float4x4 || nType == eT_half4x4)
					nArrSize = 4;
				else if (nType == eT_float3x4 || nType == eT_float3x3 || nType == eT_half3x4 || nType == eT_half3x3)
					nArrSize = 3;
				else if (nType == eT_float2x4 || nType == eT_half2x4)
					nArrSize = 2;
				nTokName = pTokens[nTN - 2];
			}
			assert(nArrSize > 0 && nArrSize < 16);

			EToken eT = (pTokens[nTN] == eT_TEXCOORDN) ? eT_TEXCOORD0 : eT_TEXCOORD0_centroid;

			pTokens[nTN] = n + nNested + eT;
			n += nArrSize;
			nCur = nTN + 1;
		}
		SStructData SD;
		SD.m_nName = nName;
		SD.m_nPos = -1;
		SD.m_nTCs = n;
		SData.push_back(SD);
	}
	if (InstBindVars.size())
		qsort(&InstBindVars[0], InstBindVars.size(), sizeof(SCGBind), CGBindCallback);
	pInst->m_nNumInstAttributes = nInstParam;
}

static int sFetchInst(uint32& nCur, uint32* pTokens, uint32 nT, std::vector<uint32>& Parameter)
{
	while (true)
	{
		uint32 nTok = pTokens[nCur];
		if (nTok != eT_br_rnd_1 && nTok != eT_br_rnd_2 && nTok != eT_comma)
			break;
		nCur++;
	}
	int nC = 0;
	Parameter.push_back(pTokens[nCur]);
	nCur++;
	while (pTokens[nCur] == eT_dot)
	{
		nC = 2;
		Parameter.push_back(pTokens[nCur]);
		Parameter.push_back(pTokens[nCur + 1]);
		nCur += 2;
	}
	return nC;
}

static void sCR(TArray<char>& Text, int nLevel)
{
	Text.AddElem('\n');
	for (int i = 0; i < nLevel; i++)
	{
		Text.AddElem(' ');
		Text.AddElem(' ');
	}
}

bool CHWShader_D3D::ConvertBinScriptToASCII(CParserBin& Parser, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, const FXShaderToken& Table, TArray<char>& Text)
{
	uint32 i;
	bool bRes = true;

	uint32* pTokens = &Parser.m_Tokens[0];
	uint32 nT = Parser.m_Tokens.size();
	const char* szPrev = " ";
	int nLevel = 0;
	for (i = 0; i < nT; i++)
	{
		uint32 nToken = pTokens[i];
		if (nToken == 0)
		{
			Text.Copy("\n", 1);
			continue;
		}
		if (nToken == eT_skip)
		{
			i++;
			continue;
		}
		if (nToken == eT_skip_1)
		{
			while (i < nT)
			{
				nToken = pTokens[i];
				if (nToken == eT_skip_2)
					break;
				i++;
			}
			assert(i < nT);
			continue;
		}
		if (nToken == eT_fetchinst)
		{
			char str[512];
			i++;
			std::vector<uint32> ParamDst, ParamSrc;
			TArray<char> sParamDstFull, sParamDstName, sParamSrc;
			int nDst = sFetchInst(i, &Parser.m_Tokens[0], Parser.m_Tokens.size(), ParamDst);
			assert(Parser.m_Tokens[i] == eT_eq);
			if (Parser.m_Tokens[i] != eT_eq)
			{
				// Should never happen
				int n = CParserBin::FindToken(i, Parser.m_Tokens.size() - 1, &Parser.m_Tokens[0], eT_semicolumn);
				if (n > 0)
					i = n + 1;
				continue;
			}
			i++;
			int nSrc = sFetchInst(i, &Parser.m_Tokens[0], Parser.m_Tokens.size(), ParamSrc);
			CParserBin::ConvertToAscii(&ParamDst[0], ParamDst.size(), Table, sParamDstFull);
			CParserBin::ConvertToAscii(&ParamDst[nDst], 1, Table, sParamDstName);
			CParserBin::ConvertToAscii(&ParamSrc[nSrc], 1, Table, sParamSrc);
			assert(strncmp(&sParamSrc[0], "Inst", 4) == 0);

			{
				sParamSrc.Free();
				CParserBin::ConvertToAscii(&ParamSrc[0], ParamSrc.size(), Table, sParamSrc);
				cry_sprintf(str, "%s = %s;\n", &sParamDstFull[0], &sParamSrc[0]);
				Text.Copy(str, strlen(str));
			}
			while (Parser.m_Tokens[i] != eT_semicolumn)
			{
				i++;
			}
			continue;
		}
		const char* szStr = CParserBin::GetString(nToken, Table, false);
		assert(szStr);
		if (!szStr || !szStr[0])
		{
			assert(0);
			bRes = CParserBin::CorrectScript(pTokens, i, nT, Text);
		}
		else
		{
#if defined(_DEBUG) && !CRY_PLATFORM_ORBIS
			int n = 0;
			while (szStr[n])
			{
				char c = szStr[n++];
				bool bASC = isascii(c);
				assert(bASC);
			}
#endif
			if (nToken == eT_semicolumn || nToken == eT_br_cv_1)
			{
				if (nToken == eT_br_cv_1)
				{
					sCR(Text, nLevel);
					nLevel++;
				}
				Text.Copy(szStr, strlen(szStr));
				if (nToken == eT_semicolumn)
				{
					if (i + 1 < nT && pTokens[i + 1] == eT_br_cv_2)
						sCR(Text, nLevel - 1);
					else
						sCR(Text, nLevel);
				}
				else if (i + 1 < nT)
				{
					if (pTokens[i + 1] < eT_br_rnd_1 || pTokens[i + 1] >= eT_float)
						sCR(Text, nLevel);
				}
			}
			else
			{
				if (i + 1 < nT)
				{
					if (Text.Num())
					{
						char cPrev = Text[Text.Num() - 1];
						if (!SkipChar((uint8)cPrev) && !SkipChar((uint8)szStr[0]))
							Text.AddElem(' ');
					}
				}
				Text.Copy(szStr, strlen(szStr));
				if (nToken == eT_br_cv_2)
				{
					nLevel--;
					if (i + 1 < nT && pTokens[i + 1] != eT_semicolumn)
						sCR(Text, nLevel);
				}
			}
		}
	}
	Text.AddElem(0);

	return bRes;
}

int OrigToBase64Size(int orig_size)
{
	return ((orig_size + 2) / 3) * 4;
}

static const uint8 BASE64_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void Base64EncodeBuffer(const void* orig_buf, int orig_buf_size, void* b64_buf)
{
	const uint8* p = (const uint8*)orig_buf;
	uint8* q = (uint8*)b64_buf;
	int w;

	for (int i = 0, e = orig_buf_size / 3; i < e; ++i, p += 3, q += 4)
	{
		// assuming little endian
		w = (p[0] << 16) | (p[1] << 8) | p[2];
		q[0] = BASE64_TABLE[w >> 18];
		q[1] = BASE64_TABLE[(w >> 12) & 0x3F];
		q[2] = BASE64_TABLE[(w >> 6) & 0x3F];
		q[3] = BASE64_TABLE[w & 0x3F];
	}

	switch (orig_buf_size % 3)
	{
	case 1:
		w = p[0];
		q[0] = BASE64_TABLE[w >> 2];
		q[1] = BASE64_TABLE[(w << 4) & 0x3F];
		q[2] = '=';
		q[3] = '=';
		break;
	case 2:
		w = (p[0] << 8) | p[1];
		q[0] = BASE64_TABLE[w >> 10];
		q[1] = BASE64_TABLE[(w >> 4) & 0x3F];
		q[2] = BASE64_TABLE[(w << 2) & 0x3F];
		q[3] = '=';
		break;
	}
}

bool CHWShader_D3D::AddResourceLayoutToScriptHeader(SHWSInstance* pInst, const char* szProfile, const char* pFunCCryName, TArray<char>& Scr)
{
#if CRY_RENDERER_VULKAN

	if (CRendererCVars::CV_r_VkShaderCompiler && strcmp(CRendererCVars::CV_r_VkShaderCompiler->GetString(), STR_VK_SHADER_COMPILER_GLSLANG) == 0)
	{
		string pragmaRowMajor = "#pragma pack_matrix(row_major)\n";
		Scr.Insert(0, pragmaRowMajor.length());
		memcpy(&Scr[0], pragmaRowMajor.c_str(), pragmaRowMajor.length());
	}

	if (auto pEncodedLayout = GetDeviceObjectFactory().LookupResourceLayoutEncoding(pInst->m_Ident.m_pipelineState.VULKAN.resourceLayoutHash))
	{
		const int TempBufferSize = 4096;
		int bin[TempBufferSize];
		int pointer = 0;

		bin[pointer++] = strlen(szProfile);
		bin[pointer + ((strlen(szProfile) + 3) / 4) - 1] = 0;
		memcpy(&bin[pointer], szProfile, strlen(szProfile));
		pointer += (strlen(szProfile) + 3) / 4;

		bin[pointer++] = strlen(pFunCCryName);
		bin[pointer + ((strlen(pFunCCryName) + 3) / 4) - 1] = 0;
		memcpy(&bin[pointer], pFunCCryName, strlen(pFunCCryName));
		pointer += (strlen(pFunCCryName) + 3) / 4;

		CRY_ASSERT(TempBufferSize - pointer - pEncodedLayout->size() > 0);
		memcpy(&bin[pointer], pEncodedLayout->data(), pEncodedLayout->size());
		pointer += (pEncodedLayout->size() + 3) / 4;

		// vertex input description: currently disabled.
		bin[pointer++] = 0;
		bin[pointer++] = 0;

		string head = "/*\n";
		string tail = "\n*/\n\n";

		Scr.Insert(0, head.length() + OrigToBase64Size(pointer * 4) + tail.length());

		memcpy(&Scr[0], head.c_str(), head.length());
		Base64EncodeBuffer(bin, pointer * 4, &Scr[head.length()]);
		memcpy(&Scr[head.length() + OrigToBase64Size(pointer * 4)], tail.c_str(), tail.length());

		return true;
	}
#endif

	return false;
}

void CHWShader_D3D::mfGetSrcFileName(char* srcName, int nSize)
{
	if (!srcName || nSize <= 0)
		return;
	if (!m_NameSourceFX.empty())
	{
		cry_strcpy(srcName, nSize, m_NameSourceFX.c_str());
		return;
	}
	cry_strcpy(srcName, nSize, gRenDev->m_cEF.m_HWPath);
	if (m_eSHClass == eHWSC_Vertex)
		cry_strcat(srcName, nSize, "Declarations/CGVShaders/");
	else if (m_eSHClass == eHWSC_Pixel)
		cry_strcat(srcName, nSize, "Declarations/CGPShaders/");
	else
		cry_strcat(srcName, nSize, "Declarations/CGGShaders/");
	cry_strcat(srcName, nSize, GetName());
	cry_strcat(srcName, nSize, ".crycg");
}

void CHWShader_D3D::mfGenName(SHWSInstance* pInst, char* dstname, int nSize, byte bType)
{
	if (bType)
		CHWShader::mfGenName(pInst->m_Ident.m_GLMask, pInst->m_Ident.m_RTMask, pInst->m_Ident.m_LightMask, pInst->m_Ident.m_MDMask, pInst->m_Ident.m_MDVMask, pInst->m_Ident.m_pipelineState.opaque, pInst->m_eClass, dstname, nSize, bType);
	else
		CHWShader::mfGenName(0, 0, 0, 0, 0, 0, eHWSC_Num, dstname, nSize, bType);
}

void CHWShader_D3D::mfGetDstFileName(SHWSInstance* pInst, char* dstname, int nSize, byte bType)
{
	cry_strcpy(dstname, nSize, gRenDev->m_cEF.m_ShadersCache);

	if (m_eSHClass == eHWSC_Vertex)
	{
		if (bType == 1 || bType == 4)
			cry_strcat(dstname, nSize, "CGVShaders/Debug/");
		else if (bType == 0)
			cry_strcat(dstname, nSize, "CGVShaders/");
		else if (bType == 2 || bType == 3)
			cry_strcat(dstname, nSize, "CGVShaders/Pending/");
	}
	else if (m_eSHClass == eHWSC_Pixel)
	{
		if (bType == 1 || bType == 4)
			cry_strcat(dstname, nSize, "CGPShaders/Debug/");
		else if (bType == 0)
			cry_strcat(dstname, nSize, "CGPShaders/");
		else if (bType == 2 || bType == 3)
			cry_strcat(dstname, nSize, "CGPShaders/Pending/");
	}
	else if (m_eSHClass == eHWSC_Geometry)
	{
		if (bType == 1 || bType == 4)
			cry_strcat(dstname, nSize, "CGGShaders/Debug/");
		else if (bType == 0)
			cry_strcat(dstname, nSize, "CGGShaders/");
		else if (bType == 2 || bType == 3)
			cry_strcat(dstname, nSize, "CGGShaders/Pending/");
	}
	else if (m_eSHClass == eHWSC_Hull)
	{
		if (bType == 1 || bType == 4)
			cry_strcat(dstname, nSize, "CGHShaders/Debug/");
		else if (bType == 0)
			cry_strcat(dstname, nSize, "CGHShaders/");
		else if (bType == 2 || bType == 3)
			cry_strcat(dstname, nSize, "CGHShaders/Pending/");
	}
	else if (m_eSHClass == eHWSC_Domain)
	{
		if (bType == 1 || bType == 4)
			cry_strcat(dstname, nSize, "CGDShaders/Debug/");
		else if (bType == 0)
			cry_strcat(dstname, nSize, "CGDShaders/");
		else if (bType == 2 || bType == 3)
			cry_strcat(dstname, nSize, "CGDShaders/Pending/");
	}
	else if (m_eSHClass == eHWSC_Compute)
	{
		if (bType == 1 || bType == 4)
			cry_strcat(dstname, nSize, "CGCShaders/Debug/");
		else if (bType == 0)
			cry_strcat(dstname, nSize, "CGCShaders/");
		else if (bType == 2 || bType == 3)
			cry_strcat(dstname, nSize, "CGCShaders/Pending/");
	}

	cry_strcat(dstname, nSize, GetName());

	if (bType == 2)
		cry_strcat(dstname, nSize, "_out");

	if (bType == 0)
	{
		char* s = strchr(dstname, '(');
		if (s)
			s[0] = 0;
	}

	char szGenName[256];
	mfGenName(pInst, szGenName, 256, bType);

	cry_strcat(dstname, nSize, szGenName);
}

//========================================================================================================
// Binary cache support

SDiskShaderCache::SDiskShaderCache(const char* name, cacheSource cacheType) : cacheType(cacheType)
{
	OpenCacheFile(name, cacheType);
}

SDiskShaderCache::SDiskShaderCache(recreateUserCacheTag, const char* name, std::uint32_t CRC32, float cacheVer) : cacheType(cacheSource::user)
{
	stack_string szUser = stack_string(gRenDev->m_cEF.m_szUserPath.c_str()) + stack_string(name);
	CResFile* rfUser = new CResFile(szUser.c_str());

	bool result = rfUser->mfOpen(RA_CREATE | (CParserBin::m_bEndians ? RA_ENDIANS : 0), &gRenDev->m_cEF.m_ResLookupDataMan[static_cast<int>(cacheType)]) > 0;
	if (!result)
	{
		rfUser->mfClose();
		SAFE_DELETE(rfUser);
		return;
	}
	rfUser->StoreLookupData(CRC32, cacheVer);
	rfUser->mfFlush();

	m_pRes = rfUser;
}

SDiskShaderCache::~SDiskShaderCache() noexcept
{
	SAFE_DELETE(m_pRes);
}

std::pair<std::unique_ptr<byte[]>, uint32> SDiskShaderCache::DecompressResource(CResFileOpenScope &scope, size_t offset, size_t size) const
{
	using ReturnType = std::pair<std::unique_ptr<byte[]>, size_t>;

	auto* handle = scope.getHandle();

	// Read resource
	gEnv->pCryPak->FSeek(handle->mfGetHandle(), offset, SEEK_SET);
	std::unique_ptr<byte[]> dataSource = std::unique_ptr<byte[]>(new byte[size]);
	const auto readBytes = gEnv->pCryPak->FReadRaw(dataSource.get(), size, 1, handle->mfGetHandle());
	gEnv->pCryPak->FSeek(handle->mfGetHandle(), 0, SEEK_SET);

	if (!readBytes)
		return ReturnType{};
	byte *buf = dataSource.get();

	const auto resVersion = handle->mfGetVersion();
	const auto swapEndian = handle->RequiresSwapEndianOnRead();

	uint32 decompressedSize = 0;
	std::unique_ptr<byte[]> pData = nullptr;
	switch(resVersion)
	{
	case RESVERSION_LZSS:
		decompressedSize = *reinterpret_cast<uint32*>(buf);
		if (size >= 10000000)
			return ReturnType{};
		if (swapEndian)
			SwapEndian(decompressedSize, eBigEndian);
		pData = std::unique_ptr<byte[]>(new byte[decompressedSize]);
		if (!pData)
		{
			CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_ERROR, "FileRead - Allocation fault");
			return ReturnType{};
		}
		if (!Decodem(&buf[4], pData.get(), size - 4, decompressedSize))
		{
			CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_ERROR, "FileRead - Decodem fault");
			return ReturnType{};
		}

		break;
		
	case RESVERSION_LZMA:
		uint64 outSize64;
		if (Lzma86_GetUnpackSize(buf, size, &outSize64) != 0)
		{
			CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_ERROR, "FileRead - data error");
			return ReturnType{};
		}
		decompressedSize = static_cast<uint32>(outSize64);
		if (decompressedSize != 0)
		{
			pData = std::unique_ptr<byte[]>(new byte[decompressedSize]);
			if (!pData)
			{
				CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_ERROR, "FileRead - can't allocate");
				return ReturnType{};
			}
			size_t sizeOut = decompressedSize;
			const auto res = Lzma86_Decode(pData.get(), &sizeOut, buf, &size);
			decompressedSize = static_cast<uint32_t>(sizeOut);
			if (res != 0)
			{
				CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_ERROR, "FileRead - LzmaDecoder error");
				return ReturnType{};
			}
		}

		break;

	case RESVERSION_DEBUG:
		decompressedSize = size - 20;
		if (decompressedSize < 0 || decompressedSize > 128 * 1024 * 1024)
		{
			CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_ERROR, "FileRead - Corrupt DirEntiy size");
			return ReturnType{};
		}
		pData = std::unique_ptr<byte[]>(new byte[decompressedSize]);
		memcpy(pData.get(), &buf[10], decompressedSize);

		break;
	}

	return std::make_pair(std::move(pData), decompressedSize);
}

void SDiskShaderCache::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_pRes);
}

bool SDiskShaderCache::OpenCacheFileImpl(cacheSource cacheType, CResFile* pRF)
{
	bool bValid = true;
	int nRes = pRF->mfOpen(RA_READ | (CParserBin::m_bEndians ? RA_ENDIANS : 0), &gRenDev->m_cEF.m_ResLookupDataMan[static_cast<int>(cacheType)], nullptr);
	if (nRes == 0)
	{
		pRF->mfClose();
		bValid = false;
	}
	else if (nRes > 0)
	{
		if (cacheType == cacheSource::user)
		{
			// User cache, reopen with read-write access.
			pRF->mfClose();
			if (bValid)
			{
				int nAcc = CRenderer::CV_r_shadersAllowCompilation != 0 ? (RA_READ | RA_WRITE) : RA_READ;
				if (!pRF->mfOpen(nAcc | (CParserBin::m_bEndians ? RA_ENDIANS : 0), &gRenDev->m_cEF.m_ResLookupDataMan[static_cast<int>(cacheType)]))
				{
					pRF->mfClose();
					bValid = false;
				}
			}
		}
	}

	if (!bValid)
	{
		SAFE_DELETE(pRF);
	}
	else
		m_pRes = pRF;

	return bValid;
}

bool SDiskShaderCache::OpenCacheFile(const char* szName, cacheSource src)
{
	bool bValidRO = false;
	bool bValidUser = true;

	// don't load the readonly cache, when shaderediting is true
	if (!CRenderer::CV_r_shadersediting && src == cacheSource::readonly)
	{
		stack_string szEngine = stack_string("%ENGINE%/") + stack_string(szName);
		CResFile* rfRO = new CResFile(szEngine);
		bValidRO = OpenCacheFileImpl(cacheSource::readonly, rfRO);
	}
	else if (src == cacheSource::user)
	{
		stack_string szUser = stack_string(gRenDev->m_cEF.m_szUserPath.c_str()) + stack_string(szName);
		CResFile* rfUser = new CResFile(szUser.c_str());
		bValidUser = OpenCacheFileImpl(cacheSource::user, rfUser);
	}

	return (bValidRO || bValidUser);
}

SDeviceShaderEntry CHWShader_D3D::mfGetCacheItem(CShader* pFX, const char *name, SDiskShaderCache *cache, uint32& nFlags)
{
	if (!cache || !cache->m_pRes)
		return {};

	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);

	CResFileOpenScope rfOpenGuard(cache->m_pRes);
	if (!rfOpenGuard.open(RA_READ | (CParserBin::m_bEndians ? RA_ENDIANS : 0), &gRenDev->m_cEF.m_ResLookupDataMan[static_cast<int>(cache->GetType())], nullptr))
		return {};

	bool bAsync = false;
	const CDirEntry* de = rfOpenGuard.getHandle()->mfGetEntry(name, &bAsync);
	if (de)
	{
		// Validate
		if (mfValidateCache(*cache) != cacheValidationResult::ok)
			return {};

		m_pCurInst->m_bAsyncActivating = false;
		auto entity = mfShaderEntryFromCache(pFX, *de, rfOpenGuard, *cache);
		if (cache->GetType() == cacheSource::user)
			nFlags |= HWSG_CACHE_USER;
		return entity;
	}
	else
	{
		m_pCurInst->m_bAsyncActivating = bAsync;
		return {};
	}
}

bool CHWShader_D3D::mfAddCacheItem(SDiskShaderCache* pCache, SShaderCacheHeaderItem* pItem, const byte* pData, int nLen, bool bFlush, CCryNameTSCRC Name)
{
	if (!pCache || !pCache->m_pRes)
	{
		CRY_ASSERT(false);
		return false;
	}

	if (CRenderer::CV_r_shadersdebug == 3 || CRenderer::CV_r_shadersdebug == 4)
		CryLog("---Cache: StoredToGlobal %s': 0x%x", pCache->m_pRes->mfGetFileName(), Name.get());

	pItem->m_CRC32 = CCrc32::Compute(pData, nLen);
	//CryLog("Size: %d: CRC: %x", nLen, pItem->m_CRC32);

	byte* pNew = new byte[sizeof(SShaderCacheHeaderItem) + nLen];
	if (CParserBin::m_bEndians)
	{
		SShaderCacheHeaderItem IT = *pItem;
		SwapEndian(IT, eBigEndian);
		memcpy(pNew, &IT, sizeof(SShaderCacheHeaderItem));
	}
	else
		memcpy(pNew, pItem, sizeof(SShaderCacheHeaderItem));
	memcpy(&pNew[sizeof(SShaderCacheHeaderItem)], pData, nLen);

	CDirEntry de(Name, nLen + sizeof(SShaderCacheHeaderItem), RF_COMPRESS | RF_TEMPDATA);

	CResFileOpenScope rfOpenGuard(pCache->m_pRes);
	rfOpenGuard.open(RA_WRITE | RA_READ | (CParserBin::m_bEndians ? RA_ENDIANS : 0), &gRenDev->m_cEF.m_ResLookupDataMan[static_cast<int>(pCache->GetType())], nullptr);
	pCache->m_pRes->mfFileAdd(&de);
	SDirEntryOpen* pOE = pCache->m_pRes->mfOpenEntry(de.GetName());
	if (!pOE)
	{
		CRY_ASSERT_MESSAGE(false, "CHWShader_D3D::mfAddCacheItem(): Couldn't add/open cache entry");
		delete[] pNew;
		return false;
	}

	pOE->pData = pNew;
	if (bFlush)
		pCache->m_pRes->mfFlush();

	return true;
}

std::vector<SEmptyCombination> SEmptyCombination::s_Combinations;

bool CHWShader_D3D::mfAddEmptyCombination(uint64 nRT, uint64 nGL, uint32 nLT, const SCacheCombination& cmbSaved)
{
	SEmptyCombination Comb;
	Comb.nGLNew = m_nMaskGenShader;
	Comb.nRTNew = cmbSaved.Ident.m_RTMask & m_nMaskAnd_RT | m_nMaskOr_RT;
	Comb.nLTNew = cmbSaved.Ident.m_LightMask;
	Comb.nGLOrg = nGL;
	Comb.nRTOrg = nRT & m_nMaskAnd_RT | m_nMaskOr_RT;
	Comb.nLTOrg = nLT;
	Comb.nMD = cmbSaved.Ident.m_MDMask;
	Comb.nMDV = cmbSaved.Ident.m_MDVMask;
	if (m_eSHClass == eHWSC_Pixel)
	{
		Comb.nMD &= ~HWMD_TEXCOORD_FLAG_MASK;
		Comb.nMDV = 0;
	}

	Comb.pShader = this;
	if (Comb.nRTNew != Comb.nRTOrg || Comb.nGLNew != Comb.nGLOrg || Comb.nLTNew != Comb.nLTOrg)
		SEmptyCombination::s_Combinations.push_back(Comb);

	m_nMaskGenShader = nGL;

	return true;
}

bool CHWShader_D3D::mfStoreEmptyCombination(SEmptyCombination& Comb)
{
	auto cache = QueryDiskCache(cacheSource::user);
	if (!cache || !cache->m_pRes)
		return false;

	CResFile* rf = cache->m_pRes;
	char nameOrg[128];
	char nameNew[128];
	SShaderCombIdent Ident;
	Ident.m_GLMask = Comb.nGLNew;
	Ident.m_RTMask = Comb.nRTNew;
	Ident.m_LightMask = Comb.nLTNew;
	Ident.m_MDMask = Comb.nMD;
	Ident.m_MDVMask = Comb.nMDV;
	SHWSInstance* pInstNew = mfGetInstance(Ident, 0);
	mfGenName(pInstNew, nameNew, 128, 1);
	CDirEntry* deNew = rf->mfGetEntry(nameNew);
	//assert(deNew);
	if (!deNew)
		return false;

	Ident.m_GLMask = Comb.nGLOrg;
	Ident.m_RTMask = Comb.nRTOrg;
	Ident.m_LightMask = Comb.nLTOrg;
	Ident.m_MDMask = Comb.nMD;
	Ident.m_MDVMask = Comb.nMDV;
	SHWSInstance* pInstOrg = mfGetInstance(Ident, 0);
	mfGenName(pInstOrg, nameOrg, 128, 1);
	CDirEntry* deOrg = rf->mfGetEntry(nameOrg);
	if (deOrg)
	{
		if (deOrg->GetOffset() != deNew->GetOffset())
			deOrg->MarkNotSaved();

		// Also evict in-memory image of user cache
		InvalidateCache(cacheSource::user);

		return true;
	}
	CDirEntry de(nameOrg, deNew->GetSize(), deNew->GetOffset(), deNew->GetFlags());
	de.MarkNotSaved();
	rf->mfFileAdd(&de);

	return true;
}

bool CHWShader_D3D::mfFlushCacheFile()
{
	uint32 i;

	for (i = 0; i < m_Insts.size(); i++)
	{
		SHWSInstance* pInst = m_Insts[i];
		if (pInst->m_Handle.m_bStatus == 2) // Fake
			pInst->m_Handle = SD3DShaderHandle{};
	}

	// Flush
	auto cache = QueryDiskCache(cacheSource::user);
	if (cache && cache->m_pRes)
	{
		cache->m_pRes->mfFlush();	
		return true;
	}

	return false;
}

struct SData
{
	CCryNameTSCRC Name;
	uint32        nSizeDecomp;
	uint32        nSizeComp;
	uint32        nOffset;
	uint16        flags;
	bool          needsProcessing;
	byte*         pData;

	bool operator<(const SData& o) const
	{
		return Name < o.Name;
	}
};
#if CRY_PLATFORM_DESKTOP
// Remove shader duplicates
bool SDiskShaderCache::mfOptimiseCacheFile(SOptimiseStats* pStats)
{
	if (!m_pRes)
		return false;

	CResFileOpenScope rfOpenGuard(m_pRes);
	const auto openResult = rfOpenGuard.open(RA_READ | (CParserBin::m_bEndians ? RA_ENDIANS : 0), &gRenDev->m_cEF.m_ResLookupDataMan[static_cast<int>(cacheType)], nullptr);
	if (!openResult)
		return false;

	ResDir* Dir = m_pRes->mfGetDirectory();
	uint32 i, j;
	std::vector<SData> Data;

	if (pStats)
		pStats->nEntries += Dir->size();

	for (CDirEntry& DE : *Dir)
	{
		if (!DE.IsValid())
			continue;

		if (DE.GetFlags() & RF_RES_$)
		{
			if (DE.GetName() == CShaderMan::s_cNameHEAD)
				continue;

			SData d;
			d.nSizeComp = d.nSizeDecomp = 0;
			d.pData = m_pRes->mfFileReadCompressed(&DE, d.nSizeDecomp, d.nSizeComp);
			assert(d.pData && d.nSizeComp && d.nSizeDecomp);
			if (!d.pData || !d.nSizeComp || !d.nSizeDecomp)
				continue;
			if (pStats)
				pStats->nTokenDataSize += d.nSizeDecomp;
			d.nOffset = 0;
			d.needsProcessing = false;
			d.Name = DE.GetName();
			d.flags = DE.GetFlags();
			Data.push_back(d);
		}
		else
		{
			SData d;
			d.flags = DE.GetFlags();
			d.nSizeComp = d.nSizeDecomp = 0;
			d.pData = m_pRes->mfFileReadCompressed(&DE, d.nSizeDecomp, d.nSizeComp);
			assert(d.pData && d.nSizeComp && d.nSizeDecomp);
			if (!d.pData || !d.nSizeComp || !d.nSizeDecomp)
				continue;
			d.nOffset = DE.GetOffset();
			d.needsProcessing = true;
			d.Name = DE.GetName();
			Data.push_back(d);
			m_pRes->mfCloseEntry(DE.GetName(), DE.GetFlags());
		}
	}

	int nOutFiles = Data.size();

	// detect duplicates
	for (i = 0; i < Data.size(); i++)
	{
		if (!Data[i].needsProcessing)
			continue;

		Data[i].needsProcessing = false;
		int nSizeComp = Data[i].nSizeComp;
		int nSizeDecomp = Data[i].nSizeDecomp;
		for (j = i + 1; j < Data.size(); j++)
		{
			if (!Data[j].needsProcessing)
				continue;

			if (nSizeComp != Data[j].nSizeComp || nSizeDecomp != Data[j].nSizeDecomp)
				continue;

			if (!memcmp(Data[i].pData, Data[j].pData, nSizeComp))
			{
				Data[j].needsProcessing = false;
				Data[j].nOffset = Data[i].nOffset;
				Data[j].flags |= RF_DUPLICATE;
				nOutFiles--;
			}
		}
	}

	if (nOutFiles != Data.size() || CRenderer::CV_r_shaderscachedeterministic)
	{
		if (nOutFiles == Data.size())
		{
			iLog->Log(" Forcing optimise for deterministic order...");
		}

		iLog->Log(" Optimising shaders resource '%s' (%" PRISIZE_T " items)...", m_pRes->mfGetFileName(), Data.size() - 1);

		m_pRes->mfClose();
		m_pRes->mfOpen(RA_CREATE | (CParserBin::m_bEndians ? RA_ENDIANS : 0), &gRenDev->m_cEF.m_ResLookupDataMan[static_cast<int>(GetType())]);

		float fVersion = FX_CACHE_VER;
		uint32 nMinor = (int)(((float)fVersion - (float)(int)fVersion) * 10.1f);
		uint32 nMajor = (int)fVersion;

		SResFileLookupData* pLookupCache = m_pRes->GetLookupData();
		if (pLookupCache == NULL || pLookupCache->m_CacheMajorVer != nMajor || pLookupCache->m_CacheMinorVer != nMinor)
		{
			CRY_ASSERT_MESSAGE(pLookupCache == NULL, "Losing ShaderIdents by recreating lookupdata cache");
			m_pRes->StoreLookupData(0, (float)FX_CACHE_VER);
		}

		m_pRes->mfFlush();

		if (CRenderer::CV_r_shaderscachedeterministic)
			std::sort(Data.begin(), Data.end());

		for (i = 0; i < Data.size(); i++)
		{
			SData* pD = &Data[i];
			CDirEntry de;

			if (pD->flags & RF_RES_$)
			{
				de = CDirEntry(pD->Name, pD->nSizeDecomp, pD->flags);
				SDirEntryOpen* pOE = m_pRes->mfOpenEntry(pD->Name);
				pOE->pData = pD->pData;
			}
			else if (pD->flags & RF_DUPLICATE)
			{
				de = CDirEntry(pD->Name, pD->nSizeComp + 4, pD->nOffset, pD->flags | RF_COMPRESS);
				SAFE_DELETE_ARRAY(pD->pData);
			}
			else
			{
				if (pStats)
				{
					pStats->nSizeUncompressed += pD->nSizeDecomp;
					pStats->nSizeCompressed += pD->nSizeComp;
					pStats->nUniqueEntries++;
				}
				assert(pD->pData);
				if (pD->pData)
				{
					de = CDirEntry(pD->Name, pD->nSizeComp + 4, pD->nOffset, pD->flags | RF_TEMPDATA | RF_COMPRESS | RF_COMPRESSED);

					SDirEntryOpen* pOE = m_pRes->mfOpenEntry(pD->Name);
					byte* pData = new byte[de.GetSize()];
					uint32 nSize = pD->nSizeDecomp;
					memcpy(pData, &nSize, sizeof(uint32));
					memcpy(pData+sizeof(uint32), pD->pData, pD->nSizeComp);
					pOE->pData = pData;
					SAFE_DELETE_ARRAY(pD->pData);
				}
			}
			m_pRes->mfFileAdd(&de);
		}
	}

	if (nOutFiles != Data.size())
		iLog->Log("  -- Removed %" PRISIZE_T " duplicated shaders", Data.size() - nOutFiles);

	Data.clear();
	int nSizeDir = m_pRes->mfFlush();

	if (pStats)
		pStats->nDirDataSize += nSizeDir;

	for (i = 0; i < Data.size(); i++)
	{
		SData* pD = &Data[i];
		SAFE_DELETE_ARRAY(pD->pData);
	}

	if (pStats)
		CryLog("  -- Shader cache '%s' stats: Entries: %d, Unique Entries: %d, Size: %.3f Mb, Compressed Size: %.3f Mb, Token data size: %3f Mb, Directory Size: %.3f Mb", m_pRes->mfGetFileName(), pStats->nEntries, pStats->nUniqueEntries, pStats->nSizeUncompressed / 1024.0f / 1024.0f, pStats->nSizeCompressed / 1024.0f / 1024.0f, pStats->nTokenDataSize / 1024.0f / 1024.0f, pStats->nDirDataSize / 1024.0f / 1024.0f);

	return true;
}
#endif

int __cdecl sSort(const VOID* arg1, const VOID* arg2)
{
	CDirEntry** pi1 = (CDirEntry**)arg1;
	CDirEntry** pi2 = (CDirEntry**)arg2;
	CDirEntry* ti1 = *pi1;
	CDirEntry* ti2 = *pi2;
	if (ti1->GetName() < ti2->GetName())
		return -1;
	if (ti1->GetName() == ti2->GetName())
		return 0;
	return 1;
}

byte* CHWShader_D3D::mfBindsToCache(SHWSInstance* pInst, std::vector<SCGBind>* Binds, int nParams, byte* pP)
{
	int i;
	for (i = 0; i < nParams; i++)
	{
		SCGBind* cgb = &(*Binds)[i];
		SShaderCacheHeaderItemVar* pVar = (SShaderCacheHeaderItemVar*)pP;
		pVar->m_nCount = cgb->m_nParameters;
		pVar->m_Reg = cgb->m_dwBind;
		if (CParserBin::m_bEndians)
		{
			SwapEndian(pVar->m_nCount, eBigEndian);
			SwapEndian(pVar->m_Reg, eBigEndian);
		}
		int len = strlen(cgb->m_Name.c_str()) + 1;
		memcpy(pVar->m_Name, cgb->m_Name.c_str(), len);
		pP += offsetof(SShaderCacheHeaderItemVar, m_Name) + strlen(pVar->m_Name) + 1;
	}
	return pP;
}

const byte* CHWShader_D3D::mfBindsFromCache(std::vector<SCGBind>& Binds, int nParams, const byte* pP)
{
	for (int i = 0; i < nParams; i++)
	{
		SCGBind cgb;
		SShaderCacheHeaderItemVar* pVar = (SShaderCacheHeaderItemVar*)pP;

		short nParameters = pVar->m_nCount;
		if (CParserBin::m_bEndians)
			SwapEndian(nParameters, eBigEndian);
		cgb.m_nParameters = nParameters;

		cgb.m_Name = pVar->m_Name;

		int dwBind = pVar->m_Reg;
		if (CParserBin::m_bEndians)
			SwapEndian(dwBind, eBigEndian);
		cgb.m_dwBind = dwBind;

		Binds.push_back(cgb);
		pP += offsetof(SShaderCacheHeaderItemVar, m_Name) + strlen(pVar->m_Name) + 1;
	}
	return pP;
}

byte* CHWShader::mfIgnoreBindsFromCache(int nParams, byte* pP)
{
	int i;
	for (i = 0; i < nParams; i++)
	{
		SShaderCacheHeaderItemVar* pVar = (SShaderCacheHeaderItemVar*)pP;
		pP += offsetof(SShaderCacheHeaderItemVar, m_Name) + strlen(pVar->m_Name) + 1;
	}
	return pP;
}

bool CHWShader_D3D::mfUploadHW(SHWSInstance* pInst, const byte* pBuf, uint32 nSize, CShader* pSH, uint32 nFlags)
{
	PROFILE_FRAME(Shader_mfUploadHW);

	const char* sHwShaderName = _HELP("Vertex Shader");
	if (m_eSHClass == eHWSC_Pixel)
		sHwShaderName = _HELP("Pixel Shader");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_D3D, 0, "D3D HW %s", sHwShaderName);

	HRESULT hr = S_OK;
	if (!pInst->m_Handle.m_pShader)
	{
		d3dShaderHandleType* handle = nullptr;

		if (m_eSHClass == eHWSC_Pixel)
			hr = (handle = GetDeviceObjectFactory().CreatePixelShader(pBuf, nSize)) ? S_OK : E_FAIL;
		else if (m_eSHClass == eHWSC_Vertex)
			hr = (handle = GetDeviceObjectFactory().CreateVertexShader(pBuf, nSize)) ? S_OK : E_FAIL;
		else if (m_eSHClass == eHWSC_Geometry)
		{
#if 1 // use 0 for FastGS emulation mode
			if (m_Flags & HWSG_GS_MULTIRES)
			{
#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
				if (CVrProjectionManager::IsMultiResEnabledStatic())
				{
					NvAPI_D3D11_CREATE_FASTGS_EXPLICIT_DESC FastGSArgs = { NVAPI_D3D11_CREATEFASTGSEXPLICIT_VER, NV_FASTGS_USE_VIEWPORT_MASK };
					NvAPI_Status Status = NvAPI_D3D11_CreateFastGeometryShaderExplicit(gcpRendD3D->GetDevice().GetRealDevice(), alias_cast<DWORD*>(pBuf), nSize, NULL, &FastGSArgs, alias_cast<ID3D11GeometryShader**>(&handle));
					hr = (Status == NVAPI_OK) ? S_OK : E_FAIL;
				}
				else
#endif
				{
					pInst->m_Handle.m_pShader->m_bDisabled = true;
					handle = nullptr;
					hr = S_OK;
				}
			}
			else
#endif
			{
				hr = (handle = GetDeviceObjectFactory().CreateGeometryShader(pBuf, nSize)) ? S_OK : E_FAIL;
			}
		}
		else if (m_eSHClass == eHWSC_Hull)
			hr = (handle = GetDeviceObjectFactory().CreateHullShader(pBuf, nSize)) ? S_OK : E_FAIL;
		else if (m_eSHClass == eHWSC_Compute)
			hr = (handle = GetDeviceObjectFactory().CreateComputeShader(pBuf, nSize)) ? S_OK : E_FAIL;
		else if (m_eSHClass == eHWSC_Domain)
			hr = (handle = GetDeviceObjectFactory().CreateDomainShader(pBuf, nSize)) ? S_OK : E_FAIL;
		else
			assert(0);

		if (handle && hr == S_OK)
			pInst->m_Handle = SD3DShaderHandle(handle, m_eSHClass, nSize);
	}

	CRY_ASSERT_MESSAGE(pInst->m_Handle.m_pShader && pInst->m_Handle.m_pShader->GetHandle(), "Shader creation failed");

	if ((m_eSHClass == eHWSC_Vertex) && (!(nFlags & HWSF_PRECACHE)) && !pInst->m_bFallback)
		mfUpdateFXVertexFormat(pInst, pSH);

	pInst->m_Shader.m_nDataSize = nSize;
	if (m_eSHClass == eHWSC_Pixel)
		s_nDevicePSDataSize += nSize;
	else
		s_nDeviceVSDataSize += nSize;

	if (pInst->m_Handle.m_pShader->GetHandle())
	{
	#if defined(ORBIS_GPU_DEBUGGER_SUPPORT) && !CRY_RENDERER_GNM
		char name[1024];
		cry_sprintf(name, "%s_%s(LT%x)@(RT%llx)(MD%x)(MDV%x)(GL%llx)(PSS%llx)", pSH->GetName(), m_EntryFunc.c_str(), pInst->m_Ident.m_LightMask, pInst->m_Ident.m_RTMask, pInst->m_Ident.m_MDMask, pInst->m_Ident.m_MDVMask, pInst->m_Ident.m_GLMask, pInst->m_Ident.m_pipelineState.opaque);
		((CCryDXOrbisShader*)pInst->m_Handle.m_pShader->GetHandle())->RegisterWithGPUDebugger(name);
	#endif

		// Assign name to Shader for enhanced debugging
	#if !defined(RELEASE) && (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_ORBIS)
		char name[1024];
		sprintf(name, "%s_%s(LT%x)@(RT%llx)(MD%x)(MDV%x)(GL%llx)(PSS%llx)", pSH->GetName(), m_EntryFunc.c_str(), pInst->m_Ident.m_LightMask, pInst->m_Ident.m_RTMask, pInst->m_Ident.m_MDMask, pInst->m_Ident.m_MDVMask, pInst->m_Ident.m_GLMask, pInst->m_Ident.m_pipelineState.opaque);
		#if CRY_PLATFORM_WINDOWS
			#if CRY_RENDERER_DIRECT3D
				((ID3D11DeviceChild*)pInst->m_Handle.m_pShader->GetHandle())->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(name), name);
			#elif CRY_RENDERER_VULKAN
				reinterpret_cast<NCryVulkan::CShader*>(pInst->m_Handle.m_pShader->GetHandle())->DebugSetName(name);
			#endif
		#elif CRY_PLATFORM_ORBIS && !CRY_RENDERER_GNM		
			((CCryDXOrbisShader*)pInst->m_Handle.m_pShader->GetHandle())->DebugSetName(name);
		#endif
	#endif
	}

	return (hr == S_OK);
}

bool CHWShader_D3D::mfUploadHW(D3DBlob* pShader, SHWSInstance* pInst, CShader* pSH, uint32 nFlags)
{
	bool bResult = true;
	if (m_eSHClass == eHWSC_Vertex && !pInst->m_bFallback)
		mfUpdateFXVertexFormat(pInst, pSH);
	if (pShader)
	{
		DWORD* pCode = (DWORD*)pShader->GetBufferPointer();
		if (gcpRendD3D->m_cEF.m_nCombinationsProcess >= 0)
		{
			pInst->m_Handle.SetFake();
		}
		else
		{
			bResult = mfUploadHW(pInst, (byte*)pCode, (uint32)pShader->GetBufferSize(), pSH, nFlags);
			if (m_eSHClass == eHWSC_Vertex)
			{
				size_t nSize = pShader->GetBufferSize();
				pInst->m_Shader.m_pShaderData = new byte[nSize];
				pInst->m_Shader.m_nDataSize = nSize;
				memcpy(pInst->m_Shader.m_pShaderData, pCode, nSize);
			}
		}
		if (!bResult)
		{
			if (m_eSHClass == eHWSC_Vertex)
				Warning("CHWShader_D3D::mfUploadHW: Could not create vertex shader '%s'(0x%" PRIx64 ")\n", GetName(), pInst->m_Ident.m_GLMask);
			else if (m_eSHClass == eHWSC_Pixel)
				Warning("CHWShader_D3D::mfUploadHW: Could not create pixel shader '%s'(0x%" PRIx64 ")\n", GetName(), pInst->m_Ident.m_GLMask);
			else if (m_eSHClass == eHWSC_Geometry)
				Warning("CHWShader_D3D::mfUploadHW: Could not create geometry shader '%s'(0x%" PRIx64 ")\n", GetName(), pInst->m_Ident.m_GLMask);
			else if (m_eSHClass == eHWSC_Domain)
				Warning("CHWShader_D3D::mfUploadHW: Could not create domain shader '%s'(0x%" PRIx64 ")\n", GetName(), pInst->m_Ident.m_GLMask);
			else if (m_eSHClass == eHWSC_Hull)
				Warning("CHWShader_D3D::mfUploadHW: Could not create hull shader '%s'(0x%" PRIx64 ")\n", GetName(), pInst->m_Ident.m_GLMask);
			else if (m_eSHClass == eHWSC_Compute)
				Warning("CHWShader_D3D::mfUploadHW: Could not create compute shader '%s'(0x%" PRIx64 ")\n", GetName(), pInst->m_Ident.m_GLMask);
		}
	}
	return bResult;
}

bool CHWShader_D3D::mfActivateCacheItem(CShader* pSH, const SDeviceShaderEntry* cacheEntry, uint32 nFlags)
{
	bool bResult = true;

	SHWSInstance* pInst = m_pCurInst;

	pInst->m_eClass = (EHWShaderClass)cacheEntry->header.m_Class;
	pInst->m_nVertexFormat = cacheEntry->header.m_nVertexFormat;
	pInst->m_nInstructions = cacheEntry->header.m_nInstructions;
	pInst->m_VStreamMask_Decl = cacheEntry->header.m_StreamMask_Decl;
	pInst->m_VStreamMask_Stream = cacheEntry->header.m_StreamMask_Stream;
#if CRY_RENDERER_VULKAN
	pInst->m_VSInputStreams = cacheEntry->m_VSInputStreams;
#endif

	pInst->m_pBindVars = cacheEntry->bindVars;

	{
		auto handle = cacheEntry->shader;
		pInst->m_Handle = SD3DShaderHandle(std::move(handle));

#if CRY_PLATFORM_ORBIS || CRY_PLATFORM_DURANGO || CRY_RENDERER_VULKAN
		if (m_eSHClass == eHWSC_Vertex)
		{
			D3DBlob* pS = NULL;
			D3DCreateBlob(cacheEntry->m_VertexShaderBinarySize, (D3DBlob**)&pS);
			DWORD* pBuffer = (DWORD*)pS->GetBufferPointer();
			memcpy(pBuffer, cacheEntry->m_pVertexShaderBinary.get(), cacheEntry->m_VertexShaderBinarySize);
			mfVertexFormat(pInst, this, pS, nullptr);
			SAFE_RELEASE(pS);
		}
#endif
		if ((m_eSHClass == eHWSC_Vertex) && (!(nFlags & HWSF_PRECACHE)) && !pInst->m_bFallback)
			mfUpdateFXVertexFormat(pInst, pSH);
	}

	if (m_eSHClass == eHWSC_Vertex)
	{
		pInst->m_Shader.m_pShaderData = cacheEntry->m_pVertexShaderBinary.get();
		pInst->m_Shader.m_nDataSize = cacheEntry->m_VertexShaderBinarySize;
	}

	mfGatherFXParameters(pInst, pInst->m_pBindVars, this, 0, pSH);

	return bResult;
}

/*CHWShader_D3D::SHWSInstance *g_pInst;
   CHWShader_D3D::SHWSInstance g_Inst;
   CHWShader_D3D::SHWSInstance *g_pInst0;
   CHWShader_D3D::SHWSInstance g_Inst0;
   CHWShader_D3D *g_pSH;*/

bool CHWShader_D3D::mfCreateCacheItem(SHWSInstance* pInst, CShader *ef, std::vector<SCGBind>& InstBinds, byte* pData, int nLen, bool bShaderThread)
{
	auto cache = AcquireDiskCache(cacheSource::user);
	if (!cache->m_pRes)
		return false;

	byte* byteData = NULL;
	DWORD* dwordData = NULL;

	SShaderCacheHeaderItem h;
	h.m_nInstBinds = static_cast<byte>(InstBinds.size());
	h.m_nInstructions = pInst->m_nInstructions;
	h.m_nVertexFormat = pInst->m_nVertexFormat;
	h.m_Class = pData ? pInst->m_eClass : 255;
	h.m_StreamMask_Decl = pInst->m_VStreamMask_Decl;
	h.m_StreamMask_Stream = (byte)pInst->m_VStreamMask_Stream;
	int nNewSize = (h.m_nInstBinds) * sizeof(SShaderCacheHeaderItemVar) + nLen;
	byte* pNewData = new byte[nNewSize];
	byte* pP = pNewData;
	pP = mfBindsToCache(pInst, &InstBinds, h.m_nInstBinds, pP);
	PREFAST_ASSUME(pData);
	memcpy(pP, pData, nLen);
	delete[] byteData;
	delete[] dwordData;
	pP += nLen;
	char name[256];
	mfGenName(pInst, name, 256, 1);
	CCryNameTSCRC nm = CCryNameTSCRC(name);
	bool bRes = mfAddCacheItem(cache, &h, pNewData, (int)(pP - pNewData), false, nm);
	SAFE_DELETE_ARRAY(pNewData);
	if (gRenDev->m_cEF.m_nCombinationsProcess <= 0)
	{
		if (bShaderThread && false)
		{
			if (pInst->m_pAsync)
				pInst->m_pAsync->m_bPendedFlush = true;
		}
		cry_strcpy(name, GetName());
		char* s = strchr(name, '(');
		if (s)
			s[0] = 0;
		if (!bShaderThread || true)
		{
			byte bStore = 1;
			if (m_Flags & HWSG_FP_EMULATION)
				bStore = 2;
			SShaderCombIdent Ident = pInst->m_Ident;
			Ident.m_GLMask = m_nMaskGenFX;
			gRenDev->m_cEF.mfInsertNewCombination(Ident, pInst->m_eClass, name, 0, NULL, bStore);
		}
	}

	return bRes;
}

//============================================================================

void CHWShader_D3D::mfSaveCGFile(const char* scr, const char* path)
{
	if (CRenderer::CV_r_shadersdebug < 1)
		return;
	char name[1024];
	if (path && path[0])
	{
#if defined(__GNUC__)
		cry_sprintf(name, "%s/%s(LT%x)@(RT%llx)(MD%x)(MDV%x)(GL%llx)(PSS%llx).cg", path, GetName(), m_pCurInst->m_Ident.m_LightMask, m_pCurInst->m_Ident.m_RTMask, m_pCurInst->m_Ident.m_MDMask, m_pCurInst->m_Ident.m_MDVMask, m_pCurInst->m_Ident.m_GLMask, m_pCurInst->m_Ident.m_pipelineState.opaque);
#else
		cry_sprintf(name, "%s/%s(LT%x)/(RT%I64x)(MD%x)(MDV%x)(GL%I64x)(PSS%llx).cg", path, GetName(), m_pCurInst->m_Ident.m_LightMask, m_pCurInst->m_Ident.m_RTMask, m_pCurInst->m_Ident.m_MDMask, m_pCurInst->m_Ident.m_MDVMask, m_pCurInst->m_Ident.m_GLMask, m_pCurInst->m_Ident.m_pipelineState.opaque);
#endif
	}
	else
	{
#if defined(__GNUC__)
		cry_sprintf(name, "Shaders/Cache/D3D10/fxerror/%s(GL%llx)@(LT%x)(RT%llx)@(MD%x)(MDV%x)(PSS%llx).cg", GetName(), m_pCurInst->m_Ident.m_GLMask, m_pCurInst->m_Ident.m_LightMask, m_pCurInst->m_Ident.m_RTMask, m_pCurInst->m_Ident.m_MDMask, m_pCurInst->m_Ident.m_MDVMask, m_pCurInst->m_Ident.m_pipelineState.opaque);
#else
		cry_sprintf(name, "FXError/%s(GL%I64x)/(LT%x)(RT%I64x)/(MD%x)(MDV%x)(PSS%llx).cg", GetName(), m_pCurInst->m_Ident.m_GLMask, m_pCurInst->m_Ident.m_LightMask, m_pCurInst->m_Ident.m_RTMask, m_pCurInst->m_Ident.m_MDMask, m_pCurInst->m_Ident.m_MDVMask, m_pCurInst->m_Ident.m_pipelineState.opaque);
#endif
	}

	FILE* fp;

	fp = gEnv->pCryPak->FOpen(name, "w");

	if (fp)
	{
		size_t len = strlen(scr);
		gEnv->pCryPak->FWrite(scr, len, fp);
		gEnv->pCryPak->FClose(fp);
	}
}

void CHWShader_D3D::mfOutputCompilerError(string& strErr, const char* szSrc)
{
	if (CRenderer::CV_r_shadersdebug)
	{
		FILE* fp = fxopen("$$err", "w");
		if (fp)
		{
			fputs(szSrc, fp);
			fclose(fp);
		}
	}

	string strE = strErr;

	size_t newlinePos = strE.find('\n');

	while (newlinePos != string::npos)
	{
		iLog->LogError("%s", strE.substr(0, newlinePos).c_str());
		strE = strE.substr(newlinePos + 1);
		newlinePos = strE.find('\n');
	}

	if (strE.size())
		iLog->LogError("%s", strE.c_str());
}

SShaderAsyncInfo::~SShaderAsyncInfo()
{
	{
		AUTO_LOCK(g_cAILock);
		Unlink();
	}
	if (m_pFXShader)
	{
		assert(m_pFXShader->GetID() >= 0 && m_pFXShader->GetID() < MAX_REND_SHADERS);
	}
	SAFE_RELEASE(m_pFXShader);
	SAFE_RELEASE(m_pShader);
	SAFE_RELEASE(m_pErrors);

	if (ID3D11ShaderReflection* pShaderReflection = static_cast<ID3D11ShaderReflection*>(m_pConstants))
	{
		pShaderReflection->Release();
		m_pConstants = nullptr;
	}
}

// Flush pended or processed shaders (main thread task)
void SShaderAsyncInfo::FlushPendingShaders()
{
	//assert (gRenDev->m_pRT->IsRenderThread());

	if (!SShaderAsyncInfo::PendingList().m_Next)
	{
		SShaderAsyncInfo::PendingList().m_Next = &SShaderAsyncInfo::PendingList();
		SShaderAsyncInfo::PendingList().m_Prev = &SShaderAsyncInfo::PendingList();
		SShaderAsyncInfo::PendingListT().m_Next = &SShaderAsyncInfo::PendingListT();
		SShaderAsyncInfo::PendingListT().m_Prev = &SShaderAsyncInfo::PendingListT();
	}

	SShaderAsyncInfo* pAI, * pAINext;
	{
		AUTO_LOCK(g_cAILock);
		for (pAI = PendingListT().m_Next; pAI != &PendingListT(); pAI = pAINext)
		{
			pAINext = pAI->m_Next;
			pAI->Unlink();
			pAI->Link(&PendingList());
		}
	}

	for (pAI = PendingList().m_Next; pAI != &PendingList(); pAI = pAINext)
	{
		pAINext = pAI->m_Next;

		CHWShader_D3D* pSH = pAI->m_pShader;
		if (pSH)
		{
			SShaderCombIdent Ident;
			Ident.m_GLMask = pSH->m_nMaskGenShader;
			Ident.m_LightMask = pAI->m_LightMask;
			Ident.m_RTMask = pAI->m_RTMask;
			Ident.m_MDMask = pAI->m_MDMask;
			Ident.m_MDVMask = pAI->m_MDVMask;
			Ident.m_pipelineState.opaque = pAI->m_pipelineState.opaque;
			CHWShader_D3D::SHWSInstance* pInst = pSH->mfGetInstance(pAI->m_nHashInstance, Ident);
			if (pInst)
			{
				if (pInst->m_pAsync != pAI)
					CryFatalError("Shader instance async info doesn't match queued async info.");
				pSH->mfAsyncCompileReady(pInst);
			}
		}
	}
}
void CShader::mfFlushPendedShaders()
{
	SShaderAsyncInfo::FlushPendingShaders();
}

void CHWShader::mfFlushPendedShadersWait(int nMaxAllowed)
{
	if (nMaxAllowed > 0 && SShaderAsyncInfo::s_nPendingAsyncShaders < nMaxAllowed)
		return;
	if (CRenderer::CV_r_shadersasynccompiling > 0)
	{
		if (!gRenDev->IsShaderCacheGenMode())
		{
			iLog->Log("Flushing pended shaders...");
		}
Start:
		while (true)
		{
			if (SShaderAsyncInfo::s_nPendingAsyncShaders <= 0)
				break;
			int n = (int)iTimer->GetAsyncCurTime();
			if (!(n % 2))
				iLog->Update();
			if (!(n % 8))
			{
				SShaderAsyncInfo::FlushPendingShaders();
			}
			else
				CrySleep(1);
		}
		// Compile FXC shaders or next iteration of internal shaders
		SShaderAsyncInfo::FlushPendingShaders();

		if (SShaderAsyncInfo::s_nPendingAsyncShaders)
			goto Start;
		if (!gRenDev->IsShaderCacheGenMode())
		{
			iLog->Log("Finished flushing pended shaders...");
		}
	}
}

int CHWShader_D3D::mfAsyncCompileReady(SHWSInstance* pInst)
{
	//SHWSInstance *pInst = m_pCurInst;
	//assert(pInst->m_pAsync);
	if (!pInst->m_pAsync)
		return 0;

	gRenDev->m_cEF.m_ShaderCacheStats.m_nNumShaderAsyncCompiles = SShaderAsyncInfo::s_nPendingAsyncShaders;

	SShaderAsyncInfo* pAsync = pInst->m_pAsync;
	int nFrame = GetCurrentFrameID();
	if (pAsync->m_nFrame != nFrame)
	{
		pAsync->m_nFrame = nFrame;
	}

	std::vector<SCGBind> InstBindVars;
	D3DBlob* pShader = NULL;
	D3DBlob* pErrorMsgs = NULL;
	string strErr;
	char nmDst[256], nameSrc[256];
	bool bResult = true;
	int nRefCount;

	SShaderTechnique* pTech = nullptr;//gRenDev->m_RP.m_pCurTechnique;
	CShader* pSH = pAsync->m_pFXShader;
	{
		if (pAsync->m_bPending)
			return 0;

		if (pSH)
		{
			if (pSH->m_fMinVisibleDistance < pAsync->m_fMinDistance)
				pAsync->m_fMinDistance = pSH->m_fMinVisibleDistance;
		}

		mfPrintCompileInfo(pInst);

		mfGetDstFileName(pInst, nmDst, 256, 3);
		gEnv->pCryPak->AdjustFileName(nmDst, nameSrc, 0);
		if (pAsync->m_pFXShader && pAsync->m_pFXShader->m_HWTechniques.Num())
			pTech = pAsync->m_pFXShader->m_HWTechniques[0];
		if ((pAsync->m_pErrors && !pAsync->m_Errors.empty()) || !pAsync->m_pDevShader)
		{
			if (CRenderer::CV_r_logShaders)
				gcpRendD3D->LogShv("Async %d: **Failed to compile 0x%x '%s' shader\n", GetCurrentFrameID(), pInst, nameSrc);
			string Errors = pAsync->m_Errors;
			string Text = pAsync->m_Text;
			CShader* pFXShader = pAsync->m_pFXShader;
			nRefCount = pFXShader ? pFXShader->GetRefCounter() : 0;
			nRefCount = min(nRefCount, pAsync->m_pShader ? pAsync->m_pShader->GetRefCounter() : 0);
			if (nRefCount <= 1) // Just exit if shader was deleted
			{
				pInst->m_pAsync = NULL;
				SAFE_DELETE(pAsync);
				return -1;
			}

			mfOutputCompilerError(Errors, Text.c_str());

			Warning("Couldn't compile HW shader '%s'", GetName());
			mfSaveCGFile(Text.c_str(), NULL);

			bResult = false;
		}
		else if (CRenderer::CV_r_logShaders)
			gcpRendD3D->LogShv("Async %d: Finished compiling 0x%x '%s' shader\n", GetCurrentFrameID(), pInst, nameSrc);
		pShader = pAsync->m_pDevShader;
		pErrorMsgs = pAsync->m_pErrors;
		strErr = pAsync->m_Errors;
		InstBindVars = pAsync->m_InstBindVars;

		if (pAsync->m_bPendedEnv)
		{
			bResult &= CHWShader_D3D::mfCreateShaderEnv(pAsync->m_nThread, pInst, pAsync->m_pDevShader, pAsync->m_pConstants, pAsync->m_pErrors, pAsync->m_InstBindVars, this, false, pAsync->m_pFXShader, pAsync->m_nCombination);
			assert(bResult == true);
		}

		// Load samplers
		if (pAsync->m_bPendedSamplers)
			mfGatherFXParameters(pInst, pInst->m_pBindVars, this, 2, pAsync->m_pFXShader);

		if (pAsync->m_bPendedFlush)
		{
			mfFlushCacheFile();
			cry_strcpy(nmDst, GetName());
			char* s = strchr(nmDst, '(');
			if (s) s[0] = '\0';
			SShaderCombIdent Ident = pInst->m_Ident;
			Ident.m_GLMask = m_nMaskGenFX;
			gRenDev->m_cEF.mfInsertNewCombination(Ident, pInst->m_eClass, nmDst, 0);
		}

		nRefCount = pAsync->m_pFXShader ? pAsync->m_pFXShader->GetRefCounter() : 0;
		nRefCount = min(nRefCount, pAsync->m_pShader ? pAsync->m_pShader->GetRefCounter() : 0);
		if (nRefCount <= 1) // Just exit if shader was deleted
		{
			pInst->m_pAsync = NULL;
			SAFE_DELETE(pAsync);
			return -1;
		}
		SAFE_DELETE(pInst->m_pAsync);
	}

	if (pErrorMsgs && !strErr.empty())
		return -1;

	bResult &= mfUploadHW(pShader, pInst, pSH, 0);
	SAFE_RELEASE(pShader);

	if (bResult)
	{
		if (pTech)
			mfUpdatePreprocessFlags(pTech);
		return 1;
	}
	return -1;
}

bool CHWShader_D3D::mfRequestAsync(CShader* pSH, SHWSInstance* pInst, std::vector<SCGBind>& InstBindVars, const char* prog_text, const char* szProfile, const char* szEntry)
{
#ifdef SHADER_ASYNC_COMPILATION
	char nameSrc[256], nmDst[256];
	mfGetDstFileName(pInst, nmDst, 256, 3);
	gEnv->pCryPak->AdjustFileName(nmDst, nameSrc, 0);

	if (!SShaderAsyncInfo::PendingList().m_Next)
	{
		SShaderAsyncInfo::PendingList().m_Next = &SShaderAsyncInfo::PendingList();
		SShaderAsyncInfo::PendingList().m_Prev = &SShaderAsyncInfo::PendingList();
		SShaderAsyncInfo::PendingListT().m_Next = &SShaderAsyncInfo::PendingListT();
		SShaderAsyncInfo::PendingListT().m_Prev = &SShaderAsyncInfo::PendingListT();
	}

	pInst->m_pAsync = new SShaderAsyncInfo;
	pInst->m_pAsync->m_nFrame = GetCurrentFrameID();
	pInst->m_pAsync->m_InstBindVars = InstBindVars;
	pInst->m_pAsync->m_pShader = this;
	pInst->m_pAsync->m_pShader->AddRef();
	pInst->m_pAsync->m_pFXShader = pSH;
	pInst->m_pAsync->m_pFXShader->AddRef();
	pInst->m_pAsync->m_nCombination = gRenDev->m_cEF.m_nCombinationsProcess;
	assert(!stricmp(m_NameSourceFX.c_str(), pInst->m_pAsync->m_pFXShader->m_NameFile.c_str()));
	InstContainer* pInstCont = &m_Insts;
	pInst->m_pAsync->m_nHashInstance = pInst->m_Ident.m_nHash;
	pInst->m_pAsync->m_RTMask = pInst->m_Ident.m_RTMask;
	pInst->m_pAsync->m_LightMask = pInst->m_Ident.m_LightMask;
	pInst->m_pAsync->m_MDMask = pInst->m_Ident.m_MDMask;
	pInst->m_pAsync->m_MDVMask = pInst->m_Ident.m_MDVMask;
	pInst->m_pAsync->m_pipelineState.opaque = pInst->m_Ident.m_pipelineState.opaque;
	pInst->m_pAsync->m_eClass = pInst->m_eClass;
	pInst->m_pAsync->m_Text = prog_text;
	pInst->m_pAsync->m_Name = szEntry;
	pInst->m_pAsync->m_Profile = szProfile;

	if (pSH)
	{
		pInst->m_pAsync->m_fMinDistance = pSH->m_fMinVisibleDistance;
	}

	// Generate request line text to store on the shaderlist for next shader cache gen
	{
		char szShaderGenName[512];
		cry_strcpy(szShaderGenName, GetName());
		char* s = strchr(szShaderGenName, '(');
		if (s)
			s[0] = 0;
		string RequestLine;
		SShaderCombIdent Ident = pInst->m_Ident;
		Ident.m_GLMask = m_nMaskGenFX;
		gRenDev->m_cEF.mfInsertNewCombination(Ident, pInst->m_eClass, szShaderGenName, 0, &RequestLine, false);

		pInst->m_pAsync->m_RequestLine = RequestLine;
	}

	CAsyncShaderTask::InsertPendingShader(pInst->m_pAsync);

	if (CRenderer::CV_r_logShaders)
		gcpRendD3D->LogShv("Async %d: Requested compiling 0x%x '%s' shader\n", GetCurrentFrameID(), pInst, nameSrc);
#endif
	return false;
}

const char* GetShaderlistName(uint32 nPlatform)
{
	switch (nPlatform)
	{
	case SF_D3D11:
		return "ShaderList_PC.txt";
	case SF_GL4:
		return "ShaderList_GL4.txt";
	case SF_GLES3:
		return "ShaderList_GLES3.txt";
	case SF_DURANGO:
		return "ShaderList_Durango.txt";
	case SF_ORBIS:
		return "ShaderList_Orbis.txt";
	case SF_VULKAN:
		return "ShaderList_Vulkan.txt";
	default:
		CryFatalError("Unexpected Shader Platform/No platform specified");
		return "ShaderList.txt";
}
}

const char* CurrentPlatformShaderListFile()
{
#if CRY_PLATFORM_ORBIS
	return GetShaderlistName(SF_ORBIS);
#elif CRY_PLATFORM_DURANGO
	return GetShaderlistName(SF_DURANGO);
#elif CRY_RENDERER_OPENGLES && DXGL_INPUT_GLSL
	return GetShaderlistName(SF_GLES3);
#elif CRY_RENDERER_OPENGL && DXGL_INPUT_GLSL
	return GetShaderlistName(SF_GL4);
#elif CRY_RENDERER_VULKAN
	return GetShaderlistName(SF_VULKAN);
#else
	return GetShaderlistName(SF_D3D11);
#endif
}

void CHWShader_D3D::mfSubmitRequestLine(SHWSInstance* pInst, string* pRequestLine)
{
	// Generate request line text.
	char szShaderGenName[512];
	cry_strcpy(szShaderGenName, GetName());
	char* s = strchr(szShaderGenName, '(');
	if (s)
		s[0] = 0;
	string RequestLine;
	SShaderCombIdent Ident = pInst->m_Ident;
	Ident.m_GLMask = m_nMaskGenFX;
	gRenDev->m_cEF.mfInsertNewCombination(Ident, pInst->m_eClass, szShaderGenName, 0, &RequestLine, false);

	if (pRequestLine)
		*pRequestLine = RequestLine;

	if (!CRenderer::CV_r_shaderssubmitrequestline || !CRenderer::CV_r_shadersremotecompiler || pInst->m_bHasSendRequest)
		return;

	// make sure we only send the request once
	pInst->m_bHasSendRequest = true;

#ifdef SHADER_ASYNC_COMPILATION
	if (CRenderer::CV_r_shadersasynccompiling && !(m_Flags & HWSG_SYNC))
	{
		if (!SShaderAsyncInfo::PendingList().m_Next)
		{
			SShaderAsyncInfo::PendingList().m_Next = &SShaderAsyncInfo::PendingList();
			SShaderAsyncInfo::PendingList().m_Prev = &SShaderAsyncInfo::PendingList();
			SShaderAsyncInfo::PendingListT().m_Next = &SShaderAsyncInfo::PendingListT();
			SShaderAsyncInfo::PendingListT().m_Prev = &SShaderAsyncInfo::PendingListT();
		}

		SShaderAsyncInfo* pAsync = new SShaderAsyncInfo;

		if (pAsync)
		{
			pAsync->m_RequestLine = RequestLine;
			pAsync->m_Text = "";
			pAsync->m_bDeleteAfterRequest = true;

			CAsyncShaderTask::InsertPendingShader(pAsync);
		}
	}
	else
#endif
	{
		NRemoteCompiler::CShaderSrv::Instance().RequestLine(CurrentPlatformShaderListFile(), RequestLine.c_str());
	}
}

bool CHWShader_D3D::mfCompileHLSL_Int(CShader* pSH, char* prog_text, D3DBlob** ppShader, void** ppConstantTable, D3DBlob** ppErrorMsgs, string& strErr, std::vector<SCGBind>& InstBindVars)
{
	HRESULT hr = S_OK;
	SHWSInstance* pInst = m_pCurInst;
	const char* szProfile = mfProfileString(pInst->m_eClass);
	const char* pFunCCryName = m_EntryFunc.c_str();

	bool bRes = true;
	if (CRenderer::CV_r_shadersdebug == 2)
	{
		mfSaveCGFile(prog_text, "TestCG");
	}

#if CRY_RENDERER_VULKAN || CRY_RENDERER_GNM
	if (pInst->m_Ident.m_pipelineState.opaque == UPipelineState().opaque)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING,"Cannot compile %s(LT%x)/(RT%I64x)(MD%x)(MDV%x)(GL%I64x)(PSS%llx).cg. Resource layout missing!",
			GetName(), m_pCurInst->m_Ident.m_LightMask, m_pCurInst->m_Ident.m_RTMask, m_pCurInst->m_Ident.m_MDMask,
			m_pCurInst->m_Ident.m_MDVMask, m_pCurInst->m_Ident.m_GLMask, m_pCurInst->m_Ident.m_pipelineState.opaque);

		return false;
	}
#endif

	if (CRenderer::CV_r_shadersasynccompiling && !(m_Flags & HWSG_SYNC))
	{
		return mfRequestAsync(pSH, pInst, InstBindVars, prog_text, szProfile, pFunCCryName);
	}
	else if (CRenderer::CV_r_shadersremotecompiler)
	{
		string sCompiler = gRenDev->m_cEF.mfGetShaderCompileFlags(pInst->m_eClass, pInst->m_Ident.m_pipelineState);

		string RequestLine;
		mfSubmitRequestLine(pInst, &RequestLine);

		std::vector<uint8> Data;
		if (NRemoteCompiler::ESOK != NRemoteCompiler::CShaderSrv::Instance().Compile(Data, szProfile, prog_text, pFunCCryName, sCompiler.c_str(), RequestLine.c_str()))
		{
			string sErrorText;
			sErrorText.reserve(Data.size());
			for (uint32 i = 0; i < Data.size(); i++)
				sErrorText += Unicode::ConvertSafe<Unicode::eErrorRecovery_FallbackWin1252ThenDiscard, string>(Data[i]); // HLSLcc may return garbage data, need to sanitize.
			strErr = sErrorText;

			return false;
		}

		D3DCreateBlob(Data.size(), (D3DBlob**)ppShader);
		D3DBlob* pShader = (D3DBlob*) *ppShader;
		DWORD* pBuf = (DWORD*) pShader->GetBufferPointer();
		memcpy(pBuf, &Data[0], Data.size());

		*ppShader = (D3DBlob*) pShader;
		pBuf = (DWORD*)pShader->GetBufferPointer();
		size_t nSize = pShader->GetBufferSize();

		bool bReflect = true;

#if CRY_PLATFORM_DESKTOP
		if (CParserBin::PlatformIsConsole())
			bReflect = false;
#endif

		if (bReflect)
		{
			void* pShaderReflBuf;
			hr = D3DReflect(pBuf, nSize, IID_ID3D11ShaderReflection, &pShaderReflBuf);
			if (SUCCEEDED(hr))
			{
				ID3D11ShaderReflection* pShaderReflection = (ID3D11ShaderReflection*)pShaderReflBuf;
				*ppConstantTable = (void*)pShaderReflection;
			}
			else
			{
				assert(0);
			}
		}

		return hr == S_OK;
	}
#if CRY_PLATFORM_WINDOWS
	else
	{
		static bool s_logOnce_WrongPlatform = false;
	#if !CRY_RENDERER_OPENGL
		#if !defined(_RELEASE)
		if (!s_logOnce_WrongPlatform && (CParserBin::m_nPlatform & (SF_D3D11 | SF_DURANGO | SF_VULKAN)) == 0)
		{
			s_logOnce_WrongPlatform = true;
			iLog->LogError("Trying to build non DX11 shader via internal compiler which is not supported. Please use remote compiler instead!");
		}
		#endif
		uint32 nFlags = D3D10_SHADER_PACK_MATRIX_ROW_MAJOR | D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
		if (CRenderer::CV_r_shadersdebug == 3 || CRenderer::CV_r_shadersdebug == 4)
			nFlags |= D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION;

		hr = D3DCompile(prog_text, strlen(prog_text), GetName(), NULL, NULL, pFunCCryName, szProfile, nFlags, 0, (ID3DBlob**) ppShader, (ID3DBlob**) ppErrorMsgs);
		if (FAILED(hr) || !*ppShader)
		{
			if (ppErrorMsgs && *ppErrorMsgs)
			{
				const char* err = (const char*)ppErrorMsgs[0]->GetBufferPointer();
				strErr += err;
			}
			else
			{
				strErr += "D3DXCompileShader failed";
			}
			bRes = false;
		}
		else
		{
			void* pShaderReflBuf;
			UINT* pData = (UINT*)ppShader[0]->GetBufferPointer();
			UINT nSize = (uint32)ppShader[0]->GetBufferSize();
			hr = D3DReflect(pData, nSize, IID_ID3D11ShaderReflection, &pShaderReflBuf);
			if (SUCCEEDED(hr))
			{
				ID3D11ShaderReflection* pShaderReflection = (ID3D11ShaderReflection*)pShaderReflBuf;
				*ppConstantTable = (void*)pShaderReflection;
			}
			else
			{
				assert(0);
			}
		}
		return bRes;
	#endif
	}
#endif // #if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO

	return false;
}

D3DBlob* CHWShader_D3D::mfCompileHLSL(CShader* pSH, char* prog_text, void** ppConstantTable, D3DBlob** ppErrorMsgs, uint32 nFlags, std::vector<SCGBind>& InstBindVars)
{
	//	LOADING_TIME_PROFILE_SECTION(iSystem);

	// Test adding source text to context
	SHWSInstance* pInst = m_pCurInst;
	string strErr;
	D3DBlob* pCode = NULL;
	HRESULT hr = S_OK;
	if (!prog_text)
	{
		assert(0);
		return NULL;
	}
	if (!CRenderer::CV_r_shadersAllowCompilation)
		return NULL;

	bool bResult = mfCompileHLSL_Int(pSH, prog_text, &pCode, ppConstantTable, ppErrorMsgs, strErr, InstBindVars);
	if (!pCode)
	{
		if (CRenderer::CV_r_shadersasynccompiling)
			return NULL;
		if (!pCode)
		{
			{
				mfOutputCompilerError(strErr, prog_text);

				Warning("Couldn't compile HW shader '%s'", GetName());
				mfSaveCGFile(prog_text, NULL);
			}
		}
	}

	return pCode;
}

void CHWShader_D3D::mfPrepareShaderDebugInfo(SHWSInstance* pInst, const char* szAsm, std::vector<SCGBind>& InstBindVars, void* pConstantTable)
{
	if (szAsm)
	{
		char* szInst = strstr((char*)szAsm, "pproximately ");
		if (szInst)
			pInst->m_nInstructions = atoi(&szInst[13]);
	}

	if (CParserBin::m_nPlatform & (SF_D3D11 | SF_DURANGO | SF_GL4 | SF_GLES3))
	{
		ID3D11ShaderReflection* pShaderReflection = (ID3D11ShaderReflection*)pConstantTable;

		if (pShaderReflection)
		{
			D3D11_SHADER_DESC Desc;
			pShaderReflection->GetDesc(&Desc);

			pInst->m_nInstructions = Desc.InstructionCount;
			pInst->m_nTempRegs = Desc.TempRegisterCount;
		}
	}

	if (CRenderer::CV_r_shadersdebug)
	{
		char nmdst[256];
		mfGetDstFileName(pInst, nmdst, 256, 4);

		string szName;
		FILE* statusdst;

		{
			szName = gRenDev->m_cEF.m_szUserPath + string(nmdst) + string(".fxca");
			statusdst = gEnv->pCryPak->FOpen(szName.c_str(), "wb");
		}

		if (statusdst)
		{
			gEnv->pCryPak->FPrintf(statusdst, "\n// %s %s\n\n", "%STARTSHADER", mfProfileString(pInst->m_eClass));
			if (m_eSHClass == eHWSC_Vertex)
			{
				for (uint32 i = 0; i < (uint32)InstBindVars.size(); i++)
				{
					SCGBind* pBind = &InstBindVars[i];
					gEnv->pCryPak->FPrintf(statusdst, "//   %s %s %d %d\n", "%%", pBind->m_Name.c_str(), pBind->m_nParameters, pBind->m_dwBind);
				}
			}
			gEnv->pCryPak->FPrintf(statusdst, "%s", szAsm);
			gEnv->pCryPak->FPrintf(statusdst, "\n// %s\n", "%ENDSHADER");
			gEnv->pCryPak->FClose(statusdst);
		}
		pInst->m_Handle = {};
	}
}

void CHWShader_D3D::mfPrintCompileInfo(SHWSInstance* pInst)
{
	int nConsts = 0;
	int nParams = pInst->m_pBindVars.size();
	for (int i = 0; i < nParams; i++)
	{
		SCGBind* pB = &pInst->m_pBindVars[i];
		nConsts += pB->m_nParameters;
	}

	char szGenName[512];
	cry_strcpy(szGenName, GetName());
	char* s = strchr(szGenName, '(');
	if (s)
		s[0] = 0;
	if (CRenderer::CV_r_shadersdebug == 2)
	{
		string pName;
		SShaderCombIdent Ident(m_nMaskGenFX, pInst->m_Ident);
		gRenDev->m_cEF.mfInsertNewCombination(Ident, pInst->m_eClass, szGenName, 0, &pName, false);
		CryLog(" Compile (FX:0x%I64x, GL:0x%I64x) %s (%d instructions, %d tempregs, %d/%d constants) ... ", m_nMaskGenFX, pInst->m_Ident.m_GLMask, pName.c_str(), pInst->m_nInstructions, pInst->m_nTempRegs, nParams, nConsts);
		int nSize = strlen(szGenName);
		mfGenName(pInst, &szGenName[nSize], 512 - nSize, 1);
		CryLog("           --- Cache entry: %s", szGenName);
	}
	else
	{
		int nSize = strlen(szGenName);
		mfGenName(pInst, &szGenName[nSize], 512 - nSize, 1);
		CryLog(" Compile %s (%d instructions, %d tempregs, %d/%d constants) ... ", szGenName, pInst->m_nInstructions, pInst->m_nTempRegs, nParams, nConsts);
	}

	if (gRenDev->m_cEF.m_bActivated && CRenderer::CV_r_shadersdebug > 0)
		CryLog(
		  " Shader %s"
#if defined(__GNUC__)
		  "(%llx)"
#else
		  "(%I64x)"
#endif
		  "(%x)(%x)(%x)(%llx)(%s) wasn't compiled before preactivating phase",
		  GetName(), pInst->m_Ident.m_RTMask, pInst->m_Ident.m_LightMask, pInst->m_Ident.m_MDMask, pInst->m_Ident.m_MDVMask, pInst->m_Ident.m_pipelineState.opaque, mfProfileString(pInst->m_eClass));
}

bool CHWShader_D3D::mfCreateShaderEnv(int nThread, SHWSInstance* pInst, D3DBlob* pShader, void*& pConstantTable, D3DBlob*& pErrorMsgs, std::vector<SCGBind>& InstBindVars, CHWShader_D3D* pSH, bool bShaderThread, CShader* pFXShader, int nCombination, const char* src)
{
	// Create asm (.fxca) cache file
	assert(pInst);
	if (!pInst)
		return false;

	CSpinLock lock;

	if (pInst->m_pBindVars.size())
		return true;

	if (pShader && (nCombination < 0))
	{
#if !CRY_RENDERER_OPENGL
		D3DBlob* pAsm = NULL;
		D3DBlob* pSrc = (D3DBlob*)pShader;
		UINT* pBuf = (UINT*)pSrc->GetBufferPointer();
		D3DDisassemble(pBuf, pSrc->GetBufferSize(), 0, NULL, &pAsm);
		if (pAsm)
		{
			char* szAsm = (char*)pAsm->GetBufferPointer();
			pSH->mfPrepareShaderDebugInfo(pInst, szAsm, InstBindVars, pConstantTable);
		}
		SAFE_RELEASE(pAsm);
#endif
	}
	//assert(!pInst->m_pBindVars);

	if (pShader)
	{
		bool bVF = pSH->m_eSHClass == eHWSC_Vertex;
#if CRY_PLATFORM_DESKTOP
		if (CParserBin::PlatformIsConsole())
			bVF = false;
#endif
#if !CRY_RENDERER_OPENGL
		if (CParserBin::m_nPlatform & (SF_GL4 | SF_GLES3))
			bVF = false;
#endif
		if (bVF)
			mfVertexFormat(pInst, pSH, pShader, pConstantTable);
		if (pConstantTable)
			mfCreateBinds(pInst->m_pBindVars, pConstantTable, (std::size_t)pShader->GetBufferSize());
	}
	int nConsts = 0;
	int nParams = pInst->m_pBindVars.size();
	for (int i = 0; i < nParams; i++)
	{
		SCGBind* pB = &pInst->m_pBindVars[i];
		nConsts += pB->m_nParameters;
	}
	if (gRenDev->m_cEF.m_nCombinationsProcess >= 0)
	{
		if (!CParserBin::m_nPlatform)
		{
			CryLog("%d: Compile %s %s (%d out of %d) - (%d/%d constants) ... ", nThread,
				mfProfileString(pInst->m_eClass), pSH->GetName(), nCombination, gRenDev->m_cEF.m_nCombinationsProcessOverall,
				nParams, nConsts);
		}
		else
		{
			CryLog("%d: Compile %s %s (%d out of %d) ... ", nThread,
				mfProfileString(pInst->m_eClass), pSH->GetName(), nCombination, gRenDev->m_cEF.m_nCombinationsProcessOverall);
		}
	}

	mfGatherFXParameters(pInst, pInst->m_pBindVars, pSH, bShaderThread ? 1 : 0, pFXShader);

	if (pShader)
		pSH->mfCreateCacheItem(pInst, pFXShader, InstBindVars, (byte*)pShader->GetBufferPointer(), (uint32)pShader->GetBufferSize(), bShaderThread);
	else
		pSH->mfCreateCacheItem(pInst, pFXShader, InstBindVars, NULL, 0, bShaderThread);

	SAFE_RELEASE(pErrorMsgs);
	if (ID3D11ShaderReflection* pShaderReflection = (ID3D11ShaderReflection*)pConstantTable)
	{
		pShaderReflection->Release();
		pConstantTable = nullptr;
	}

	return true;
}

// Compile pixel/vertex shader for the current instance properties
bool CHWShader_D3D::mfActivate(CShader* pSH, uint32 nFlags)
{
	PROFILE_FRAME(Shader_HWShaderActivate);

	bool bResult = true;
	SHWSInstance* pInst = m_pCurInst;

	mfLogShaderRequest(pInst);

	if (mfIsValid(pInst, true) == ED3DShError_NotCompiled)
	{
		char name[128];
		mfGenName(pInst, name, 128, 1);
		pInst->m_DeviceObjectID = CCryNameTSCRC{ name }.get();
		const auto devCacheKey = static_cast<SHWShaderCache::deviceShaderCacheKey>(pInst->m_DeviceObjectID);

		const SDeviceShaderEntry *entry = nullptr;
		
		// Try to find in cache
		auto &devCache = GetDevCache();
		auto it = devCache.find(devCacheKey);
		if (it == devCache.end())
		{
			// Read from disk cache and store in cache
			std::vector<cacheSource> cacheTypes = { cacheSource::readonly };
			if (CRendererCVars::CV_r_shadersAllowCompilation)
				cacheTypes = { cacheSource::user, cacheSource::readonly };
			if (CRendererCVars::CV_r_shadersediting || gRenDev->IsShaderCacheGenMode())
				cacheTypes = { cacheSource::user };

			for (const auto &cacheType : cacheTypes)
			{
				auto cache = QueryDiskCache(cacheType);
				auto newEntry = mfGetCacheItem(pSH, name, cache, nFlags);
				if (newEntry)
				{
					CryLog("CHWShader_D3D::mfActivate(): Shader '%s' not found in device cache but found in disk cache.", m_Name.c_str());

					// Store in cache
					SHWShaderCache::deviceShaderCacheValue v;
					v.emplace<SDeviceShaderEntry>(std::move(newEntry));
					auto insertResult = devCache.emplace(
						std::piecewise_construct,
						std::forward_as_tuple(devCacheKey),
						std::forward_as_tuple(std::move(v)));
					entry = &stl::get<SDeviceShaderEntry>(insertResult.first->second);

					break;
				}
			}
		}
		else
		{
			const SHWShaderCache::deviceShaderCacheValue& cacheValue = it->second;
			entry = stl::holds_alternative<SDeviceShaderEntry>(cacheValue) ?
				&stl::get<SDeviceShaderEntry>(cacheValue) :
				stl::get<const SDeviceShaderEntry*>(cacheValue);
		}

		if (entry)
		{
			if (gRenDev->m_cEF.m_nCombinationsProcess >= 0)
				return true;

			if (mfActivateCacheItem(pSH, entry, nFlags))
				return (pInst->m_Handle.m_pShader != nullptr);

			if ((nFlags & HWSF_PRECACHE) == 0)
				return false;
		}

		TArray<char> newScr;

		if (nFlags & HWSF_PRECACHE)
			gRenDev->m_cEF.m_nCombinationsCompiled++;

		D3DBlob* pShader = NULL;
		void* pConstantTable = NULL;
		D3DBlob* pErrorMsgs = NULL;
		std::vector<SCGBind> InstBindVars;
		m_Flags |= HWSG_WASGENERATED;

		bool bScriptSuccess = false;

		if (CRenderer::CV_r_shadersAllowCompilation)
		{
			// MemReplay shows that 16kb should be enough memory to hold the script without having to reallocate
			newScr.reserve(16 * 1024);
			bScriptSuccess = mfGenerateScript(pSH, pInst, InstBindVars, nFlags, newScr);
			ASSERT_IN_SHADER(bScriptSuccess);
		}

		if (!pInst->m_bAsyncActivating)
		{
			// report miss in global cache to log and/or callback
			mfLogShaderCacheMiss(pInst);

			// still sumit the request line when no compiling to be sure that the shadercompiler will
			// compile it in the next build
			if (!CRenderer::CV_r_shadersAllowCompilation)
				mfSubmitRequestLine(pInst);
		}

		if (!bScriptSuccess)
		{
			if (!pInst->m_bAsyncActivating)
			{
#ifdef __GNUC__
				Warning("Warning: Shader %s(%llx)(%x)(%x)(%x)(%llx)(%s) is not existing in the cache\n",
#else
				Warning("Warning: Shader %s(%I64x)(%x)(%x)(%x)(%llx)(%s) is not existing in the cache\n",
#endif
				        GetName(), pInst->m_Ident.m_RTMask, pInst->m_Ident.m_LightMask, pInst->m_Ident.m_MDMask, pInst->m_Ident.m_MDVMask, pInst->m_Ident.m_pipelineState.opaque, mfProfileString(pInst->m_eClass));
			}
			return false;
		}

		{
			PROFILE_FRAME(Shader_CompileHLSL);
			pShader = mfCompileHLSL(pSH, newScr.Data(), &pConstantTable, &pErrorMsgs, nFlags, InstBindVars);
		}

		gRenDev->m_cEF.m_ShaderCacheStats.m_nNumShaderAsyncCompiles = SShaderAsyncInfo::s_nPendingAsyncShaders;

		if (!pShader)
		{
			if (!CRenderer::CV_r_shadersAllowCompilation || pInst->IsAsyncCompiling())
				return false;
		}
		bResult = mfCreateShaderEnv(0, pInst, pShader, pConstantTable, pErrorMsgs, InstBindVars, this, false, pSH, gRenDev->m_cEF.m_nCombinationsProcess, newScr.Data());
		bResult &= mfUploadHW(pShader, pInst, pSH, nFlags);
		SAFE_RELEASE(pShader);
		SAFE_RELEASE(pErrorMsgs);
		if (ID3D11ShaderReflection* pShaderReflection = static_cast<ID3D11ShaderReflection*>(pConstantTable))
		{
			pShaderReflection->Release();
			pConstantTable = nullptr;
		}
	}

	ED3DShError shResult = mfIsValid(pInst, true);
	bool bSuccess = (shResult == ED3DShError_Ok) || (shResult == ED3DShError_Fake);

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////

#ifdef SHADER_ASYNC_COMPILATION

	#pragma warning(disable: 4355)// warning C4355: 'this' : used in base member initializer list

CAsyncShaderTask::CAsyncShaderTask()
	: m_nThread(0)
	, m_nThreadFXC(0)
	, m_thread(this)
{
}

void CAsyncShaderTask::InsertPendingShader(SShaderAsyncInfo* pAsync)
{
	AUTO_LOCK(g_cAILock);
	pAsync->Link(&BuildList());
	CryInterlockedIncrement(&SShaderAsyncInfo::s_nPendingAsyncShaders);
}

void CAsyncShaderTask::FlushPendingShaders()
{
	SShaderAsyncInfo* pAI, * pAI2, * pAINext;
	assert(m_flush_list.m_Prev == &m_flush_list && m_flush_list.m_Next == &m_flush_list); // the flush list must be empty - cleared by the shader compile thread
	if (BuildList().m_Prev == &BuildList() && BuildList().m_Next == &BuildList())
		return; // the build list is empty, might need to do some assert here
	{
		AUTO_LOCK(g_cAILock);
		int n = 0;
		for (pAI = BuildList().m_Prev; pAI != &BuildList(); pAI = pAINext)
		{
			pAINext = pAI->m_Prev;
			pAI->Unlink();
			pAI->Link(&m_flush_list);
		}
	}

	// Sorting by distance
	if (gRenDev->m_cEF.m_nCombinationsProcess < 0)
		for (pAI = m_flush_list.m_Next; pAI != &m_flush_list; pAI = pAI->m_Next)
		{
			assert(pAI);
			PREFAST_ASSUME(pAI);
			pAINext = NULL;
			int nFrame = pAI->m_nFrame;
			float fDist = pAI->m_fMinDistance;
			for (pAI2 = pAI->m_Next; pAI2 != &m_flush_list; pAI2 = pAI2->m_Next)
			{
				if (pAI2->m_nFrame < nFrame)
					continue;
				if (pAI2->m_nFrame > nFrame || pAI2->m_fMinDistance < fDist)
				{
					pAINext = pAI2;
					nFrame = pAI2->m_nFrame;
					fDist = pAI2->m_fMinDistance;
				}
			}
			if (pAINext)
			{
				assert(pAI != pAINext);
				SShaderAsyncInfo* pAIP0 = pAI->m_Prev;
				SShaderAsyncInfo* pAIP1 = pAINext->m_Prev == pAI ? pAINext : pAINext->m_Prev;

				pAI->m_Next->m_Prev = pAI->m_Prev;
				pAI->m_Prev->m_Next = pAI->m_Next;
				pAI->m_Next = pAIP1->m_Next;
				pAIP1->m_Next->m_Prev = pAI;
				pAIP1->m_Next = pAI;
				pAI->m_Prev = pAIP1;

				pAI = pAINext;

				pAI->m_Next->m_Prev = pAI->m_Prev;
				pAI->m_Prev->m_Next = pAI->m_Next;
				pAI->m_Next = pAIP0->m_Next;
				pAIP0->m_Next->m_Prev = pAI;
				pAIP0->m_Next = pAI;
				pAI->m_Prev = pAIP0;
			}
		}

	for (pAI = m_flush_list.m_Next; pAI != &m_flush_list; pAI = pAINext)
	{
		pAINext = pAI->m_Next;
		assert(pAI->m_bPending);
		SubmitAsyncRequestLine(pAI);
		if (pAI->m_Text.length() > 0)
			CompileAsyncShader(pAI);

		CryInterlockedDecrement(&SShaderAsyncInfo::s_nPendingAsyncShaders);
		{
			AUTO_LOCK(g_cAILock);

			pAI->Unlink();
			pAI->m_bPending = 0;
			pAI->Link(&SShaderAsyncInfo::PendingListT());
		}

		if (pAI->m_bDeleteAfterRequest)
		{
			SAFE_DELETE(pAI);
		}
	}
}

bool CAsyncShaderTask::PostCompile(SShaderAsyncInfo* pAsync)
{
	bool bResult = true;
	/*if (pAsync->m_nCombination < 0 && false)
	   {
	   CHWShader_D3D *pSH = pAsync->m_pShader;
	   CHWShader_D3D::SHWSInstance *pInst = pSH->mfGetInstance(pAsync->m_nOwner, pSH->m_nMaskGenShader);
	   bResult = CHWShader_D3D::mfCreateShaderEnv(m_nThread, pInst, pAsync->m_pDevShader, pAsync->m_pConstants, pAsync->m_pErrors, pAsync->m_InstBindVars, pSH, true, pAsync->m_pFXShader, pAsync->m_nCombination);
	   assert(bResult == true);
	   }
	   else*/
	{
		pAsync->m_nThread = m_nThread;
		pAsync->m_bPendedEnv = true;
	}
	return bResult;
}

void CAsyncShaderTask::SubmitAsyncRequestLine(SShaderAsyncInfo* pAsync)
{
	if (CRenderer::CV_r_shadersremotecompiler)
	{
		if (!pAsync->m_shaderList.empty())
		{
			NRemoteCompiler::CShaderSrv::Instance().RequestLine(pAsync->m_shaderList.c_str(), pAsync->m_RequestLine.c_str());
		}
		else
		{
			NRemoteCompiler::CShaderSrv::Instance().RequestLine(CurrentPlatformShaderListFile(), pAsync->m_RequestLine.c_str());
		}
	}
}

bool CAsyncShaderTask::CompileAsyncShader(SShaderAsyncInfo* pAsync)
{
	bool bResult = true;
	if (CRenderer::CV_r_shadersremotecompiler)
	{
		string sCompiler = gRenDev->m_cEF.mfGetShaderCompileFlags(pAsync->m_eClass, pAsync->m_pipelineState);

		std::vector<uint8> Data;
		if (NRemoteCompiler::ESOK != NRemoteCompiler::CShaderSrv::Instance().Compile(Data, pAsync->m_Profile, pAsync->m_Text.c_str(), pAsync->m_Name.c_str(), sCompiler.c_str(), pAsync->m_RequestLine.c_str()))
		{

			D3DCreateBlob(sizeof("D3DXCompileShader failed"), (D3DBlob**)&pAsync->m_pErrors);
			DWORD* pBuf = (DWORD*) pAsync->m_pErrors->GetBufferPointer();
			memcpy(pBuf, "D3DXCompileShader failed", sizeof("D3DXCompileShader failed"));

			string sErrorText;

			if (!Data.empty())
			{
				sErrorText.reserve(Data.size());
				for (uint32 i = 0; i < Data.size(); i++)
					sErrorText += Data[i];
			}
			else
			{
				sErrorText.assign("Unknown Error");
			}

			pAsync->m_Errors += sErrorText;

			return false;
		}

		HRESULT hr = S_OK;
		D3DCreateBlob(Data.size(), (D3DBlob**) &pAsync->m_pDevShader);
		D3DBlob* pShader = (D3DBlob*)*&pAsync->m_pDevShader;
		DWORD* pBuf = (DWORD*)pShader->GetBufferPointer();
		memcpy(pBuf, &Data[0], Data.size());

		pAsync->m_pDevShader = (D3DBlob*)pShader;
		pBuf = (DWORD*)pShader->GetBufferPointer();
		size_t nSize = pShader->GetBufferSize();

		bool bReflect = true;

	#if CRY_PLATFORM_DESKTOP
		if (CParserBin::PlatformIsConsole())
			bReflect = false;
	#endif
	#if !CRY_RENDERER_OPENGL
		if (CParserBin::m_nPlatform & (SF_GL4 | SF_GLES3))
			bReflect = false;
	#endif

		if (bReflect)
		{
			ID3D11ShaderReflection* pShaderReflection;
			hr = D3DReflect(pBuf, nSize, IID_ID3D11ShaderReflection, (void**)&pShaderReflection);
			if (SUCCEEDED(hr))
			{
				pAsync->m_pConstants = (void*)pShaderReflection;
			}
		}

		if (SUCCEEDED(hr))
		{
			bResult = PostCompile(pAsync);
		}
		else
		{
			pAsync->m_pDevShader = 0;
			assert(0);
		}
	}
	#if CRY_PLATFORM_WINDOWS && !CRY_RENDERER_OPENGL
	else
	{
		static bool s_logOnce_WrongPlatform = false;
		#if !defined(_RELEASE)
		if (!s_logOnce_WrongPlatform && (CParserBin::m_nPlatform & (SF_D3D11 | SF_DURANGO | SF_VULKAN)) == 0)
		{
			s_logOnce_WrongPlatform = true;
			iLog->LogError("Trying to build non DX11 shader via internal compiler which is not supported. Please use remote compiler instead!");
		}
		#endif
		uint32 nFlags = D3D10_SHADER_PACK_MATRIX_ROW_MAJOR | D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
		if (CRenderer::CV_r_shadersdebug == 3 || CRenderer::CV_r_shadersdebug == 4)
			nFlags |= D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION;

		const char* Name = pAsync->m_pShader ? pAsync->m_pShader->GetName() : "Unknown";
		HRESULT hr = S_OK;
		hr = D3DCompile(pAsync->m_Text.c_str(), pAsync->m_Text.size(), Name, NULL, NULL, pAsync->m_Name.c_str(), pAsync->m_Profile.c_str(), nFlags, 0, (ID3DBlob**) &pAsync->m_pDevShader, (ID3DBlob**) &pAsync->m_pErrors);
		if (FAILED(hr) || !pAsync->m_pDevShader)
		{
			if (pAsync->m_pErrors)
			{
				const char* err = (const char*)pAsync->m_pErrors->GetBufferPointer();
				pAsync->m_Errors += err;
			}
			else
			{
				pAsync->m_Errors += "D3DXCompileShader failed";
			}
			bResult = false;
		}
		else
		{
			ID3D11ShaderReflection* pShaderReflection;
			UINT* pData = (UINT*)pAsync->m_pDevShader->GetBufferPointer();
			size_t nSize = pAsync->m_pDevShader->GetBufferSize();
			hr = D3DReflect(pData, nSize, IID_ID3D11ShaderReflection, (void**)&pShaderReflection);
			if (SUCCEEDED(hr))
			{
				pAsync->m_pConstants = (void*)pShaderReflection;
				bResult = PostCompile(pAsync);
			}
			else
			{
				assert(0);
			}
		}
	}
	#endif // #if CRY_PLATFORM_WINDOWS
	return bResult;
}

void CAsyncShaderTask::CShaderThread::ThreadEntry()
{
	while (!m_quit)
	{
		m_task->FlushPendingShaders();
		if (!CRenderer::CV_r_shadersasynccompiling)
			CrySleep(250);
		else
			CrySleep(25);
	}
}

#endif

//===============================================================================================
// Export/Import

#ifdef SHADERS_SERIALIZING

bool SFXParam::Export(SShaderSerializeContext& SC)
{
	bool bRes = true;

	SSFXParam PR;
	PR.m_nsName = SC.AddString(m_Name.c_str());
	PR.m_nsAnnotations = SC.AddString(m_Annotations.c_str());
	PR.m_nsSemantic = SC.AddString(m_Semantic.c_str());
	PR.m_nsValues = SC.AddString(m_Values.c_str());

	PR.m_eType = m_eType;
	PR.m_nCB = m_nCB;
	PR.m_nComps = m_nComps;
	PR.m_nFlags = m_nFlags;
	PR.m_nParameters = m_nParameters;
	PR.m_nRegister[0] = m_nRegister[0];
	PR.m_nRegister[1] = m_nRegister[1];
	PR.m_nRegister[2] = m_nRegister[2];

	SC.FXParams.push_back(PR);

	return bRes;
}
bool SFXParam::Import(SShaderSerializeContext& SC, SSFXParam* pPR)
{
	bool bRes = true;

	m_Name = sString(pPR->m_nsName, SC.Strings);
	m_Annotations = sString(pPR->m_nsAnnotations, SC.Strings);
	m_Semantic = sString(pPR->m_nsSemantic, SC.Strings);
	m_Values = sString(pPR->m_nsValues, SC.Strings);

	m_eType = pPR->m_eType;
	m_nCB = pPR->m_nCB;
	m_nComps = pPR->m_nComps;
	m_nFlags = pPR->m_nFlags;
	m_nParameters = pPR->m_nParameters;
	m_nRegister[0] = pPR->m_nRegister[0];
	m_nRegister[1] = pPR->m_nRegister[1];
	m_nRegister[2] = pPR->m_nRegister[2];

	return bRes;
}

bool SFXSampler::Export(SShaderSerializeContext& SC)
{
	bool bRes = true;

	SSFXSampler PR;
	PR.m_nsName = SC.AddString(m_Name.c_str());
	PR.m_nsAnnotations = SC.AddString(m_Annotations.c_str());
	PR.m_nsSemantic = SC.AddString(m_Semantic.c_str());
	PR.m_nsValues = SC.AddString(m_Values.c_str());

	PR.m_eType = m_eType;
	PR.m_nArray = m_nArray;
	PR.m_nFlags = m_nFlags;
	PR.m_nRegister[0] = m_nRegister[0];
	PR.m_nRegister[1] = m_nRegister[1];
	PR.m_nRegister[2] = m_nRegister[2];

	SC.FXSamplers.push_back(PR);

	return bRes;
}
bool SFXSampler::Import(SShaderSerializeContext& SC, SSFXSampler* pPR)
{
	bool bRes = true;

	m_Name = sString(pPR->m_nsName, SC.Strings);
	m_Annotations = sString(pPR->m_nsAnnotations, SC.Strings);
	m_Semantic = sString(pPR->m_nsSemantic, SC.Strings);
	m_Values = sString(pPR->m_nsValues, SC.Strings);

	m_eType = pPR->m_eType;
	m_nArray = pPR->m_nArray;
	m_nFlags = pPR->m_nFlags;
	m_nRegister[0] = pPR->m_nRegister[0];
	m_nRegister[1] = pPR->m_nRegister[1];
	m_nRegister[2] = pPR->m_nRegister[2];

	return bRes;
}

bool SFXTexture::Export(SShaderSerializeContext& SC)
{
	bool bRes = true;

	SSFXTexture PR;
	PR.m_nsName = SC.AddString(m_Name.c_str());
	PR.m_nsAnnotations = SC.AddString(m_Annotations.c_str());
	PR.m_nsSemantic = SC.AddString(m_Semantic.c_str());
	PR.m_nsValues = SC.AddString(m_Values.c_str());

	PR.m_nsNameTexture = SC.AddString(m_szTexture.c_str());
	PR.m_bSRGBLookup = m_bSRGBLookup;
	PR.m_eType = m_eType;
	PR.m_nArray = m_nArray;
	PR.m_nFlags = m_nFlags;
	PR.m_nRegister[0] = m_nRegister[0];
	PR.m_nRegister[1] = m_nRegister[1];
	PR.m_nRegister[2] = m_nRegister[2];

	SC.FXTextures.push_back(PR);

	return bRes;
}
bool SFXTexture::Import(SShaderSerializeContext& SC, SSFXTexture* pPR)
{
	bool bRes = true;

	m_Name = sString(pPR->m_nsName, SC.Strings);
	m_Annotations = sString(pPR->m_nsAnnotations, SC.Strings);
	m_Semantic = sString(pPR->m_nsSemantic, SC.Strings);
	m_Values = sString(pPR->m_nsValues, SC.Strings);

	m_szTexture = sString(pPR->m_nsNameTexture, SC.Strings);
	m_bSRGBLookup = pPR->m_bSRGBLookup;
	m_eType = pPR->m_eType;
	m_nArray = pPR->m_nArray;
	m_nFlags = pPR->m_nFlags;
	m_nRegister[0] = pPR->m_nRegister[0];
	m_nRegister[1] = pPR->m_nRegister[1];
	m_nRegister[2] = pPR->m_nRegister[2];

	return bRes;
}

bool CHWShader_D3D::ExportSamplers(SCHWShader& SHW, SShaderSerializeContext& SC)
{
	bool bRes = true;

	//Samplers no longer stored here
	#if 0
	int i;
	SHW.m_nSamplers = m_Samplers.size();
	for (i = 0; i < m_Samplers.size(); i++)
	{
		STexSampler& TS = m_Samplers[i];
		bRes &= TS.Export(SC);
	}
	#endif

	return bRes;
}
bool CHWShader::ImportSamplers(SShaderSerializeContext& SC, SCHWShader* pSHW, byte*& pData, std::vector<STexSamplerRT>& Samplers)
{
	bool bRes = true;

	//Samplers no longer stored here
	#if 0
	int i;

	for (i = 0; i < pSHW->m_nSamplers; i++)
	{
		STexSamplerRT TS;
		bRes &= TS.Import(SC, pData);
		if (bRes)
			Samplers.push_back(TS);
	}
	#endif
	return bRes;
}

bool CHWShader_D3D::ExportParams(SCHWShader& SHW, SShaderSerializeContext& SC)
{
	bool bRes = true;

	//params no longer here
	#if 0
	int i;
	SHW.m_nParams = m_Params.size();
	for (i = 0; i < m_Params.size(); i++)
	{
		SFXParam& PR = m_Params[i];
		bRes &= PR.Export(SC);
	}
	#else
	SHW.m_nParams = 0;
	#endif

	return bRes;
}
bool CHWShader::ImportParams(SShaderSerializeContext& SC, SCHWShader* pSHW, byte*& pData, std::vector<SFXParam>& Params)
{
	bool bRes = true;

	//params no longer here
	#if 0
	uint32 i;

	for (i = 0; i < pSHW->m_nParams; i++)
	{
		SFXParam PR;
		bRes &= PR.Import(SC, pData);
		if (bRes)
			Params.push_back(PR);
	}
	#endif
	return bRes;
}

bool CHWShader_D3D::Export(CShader *pSH, SShaderSerializeContext& SC)
{
	bool bRes = true;

	SCHWShader SHW;

	char str[256];
	cry_strcpy(str, GetName());
	char* c = strchr(str, '(');
	if (c)
		c[0] = 0;

	SHW.m_nsName = SC.AddString(str);
	SHW.m_nsNameSourceFX = SC.AddString(m_NameSourceFX.c_str());
	SHW.m_nsEntryFunc = SC.AddString(m_EntryFunc.c_str());

	SHW.m_eSHClass = m_eSHClass;
	SHW.m_dwShaderType = m_dwShaderType;
	SHW.m_nMaskGenFX = m_nMaskGenFX;
	SHW.m_nMaskGenShader = m_nMaskGenShader;
	SHW.m_nMaskOr_RT = m_nMaskOr_RT;
	SHW.m_nMaskAnd_RT = m_nMaskAnd_RT;
	SHW.m_Flags = m_Flags;

	FXShaderToken Table;
	TArray<uint32_t> SHData;

	// No longer export any token data, slow and bloated!
	const bool bOutputTokens = false;

	if (bOutputTokens)
		mfGetCacheTokenMap(Table, SHData);
	else
		SHData = m_TokenData;

	if (bOutputTokens && !Table.size())
	{
		assert(0);
		return false;
	}

	SHW.m_nTokens = bOutputTokens ? SHData.size() : 0;
	SHW.m_nTableEntries = bOutputTokens ? SHData.size() : 0;

	SCHWShader SHWTemp = SHW;

	SHW.Export(SC.Data);
	uint32 nOffs = 0;

	if (bOutputTokens)
	{
		sAddDataArray_POD(SC.Data, SHData, nOffs);

		for (const auto &T : Table)
		{
			//String pool method
			sAddData(SC.Data, T.Token);
			uint32 tokenStrIdx = SC.AddString(T.SToken.c_str());
			sAddData(SC.Data, tokenStrIdx);
		}
	}

	bRes &= ExportSamplers(SHW, SC);
	bRes &= ExportParams(SHW, SC);

	if (bRes)
	{
		if (memcmp(&SHW, &SHWTemp, sizeof(SCHWShader)))
		{
			CryFatalError("Export failed");
		}
	}

	return bRes;
}

CHWShader* CHWShader::Import(SShaderSerializeContext& SC, int nOffs, uint32 CRC32, CShader* pSH)
{
	if (nOffs < 0)
		return NULL;

	CHWShader* pHWSH = NULL;
	SCHWShader shaderHW;
	shaderHW.Import(&SC.Data[nOffs]);
	SCHWShader* pSHW = &shaderHW;

	byte* pData = &SC.Data[nOffs + sizeof(SCHWShader)];

	const char* szName = sString(pSHW->m_nsName, SC.Strings);
	const char* szNameSource = sString(pSHW->m_nsNameSourceFX, SC.Strings);
	const char* szNameEntry = sString(pSHW->m_nsEntryFunc, SC.Strings);

	TArray<uint32> SHData;
	SHData.resize(pSHW->m_nTokens);
	memcpy(&SHData[0], pData, pSHW->m_nTokens * sizeof(uint32));

	pHWSH = CHWShader::mfForName(szName, szNameSource, CRC32, szNameEntry, pSHW->m_eSHClass, SHData, FXShaderToken{}, pSHW->m_dwShaderType, pSH, pSHW->m_nMaskGenShader, pSHW->m_nMaskGenFX);

	pHWSH->m_eSHClass = shaderHW.m_eSHClass;
	pHWSH->m_dwShaderType = shaderHW.m_dwShaderType;
	pHWSH->m_nMaskGenFX = shaderHW.m_nMaskGenFX;
	pHWSH->m_nMaskGenShader = shaderHW.m_nMaskGenShader;
	pHWSH->m_nMaskOr_RT = shaderHW.m_nMaskOr_RT;
	pHWSH->m_nMaskAnd_RT = shaderHW.m_nMaskAnd_RT;
	pHWSH->m_Flags = shaderHW.m_Flags;

	return pHWSH;
}

#else
bool CHWShader_D3D::Export(SShaderSerializeContext& SC)
{
	return false;
}
#endif

const char* CHWShader_D3D::mfGetActivatedCombinations(bool bForLevel)
{
	TArray<char> Combinations;
	char* pPtr = NULL;
	uint32 i;

	for (i = 0; i < m_Insts.size(); i++)
	{
		SHWSInstance* pInst = m_Insts[i];
		char name[256];
		cry_strcpy(name, GetName());
		char* s = strchr(name, '(');
		if (s)
			s[0] = 0;
		string str;
		SShaderCombIdent Ident(m_nMaskGenFX, pInst->m_Ident);
		gRenDev->m_cEF.mfInsertNewCombination(Ident, pInst->m_eClass, name, 0, &str, false);
		assert(str.size());
		if (str.size())
		{
			assert(str[0] == '<' && str[2] == '>');
			string s1;
			if (str[0] == '<' && str[2] == '>')
				s1.Format("<%d>%s", pInst->m_nUsed, &str[3]);
			else
				s1 = str;
			Combinations.Copy(s1.c_str(), s1.size());
			Combinations.Copy("\n", 1);
		}
	}

	if (!Combinations.Num())
		return NULL;
	pPtr = new char[Combinations.Num() + 1];
	memcpy(pPtr, &Combinations[0], Combinations.Num());
	pPtr[Combinations.Num()] = 0;
	return pPtr;
}

const char* CHWShader::GetCurrentShaderCombinations(bool bForLevel) threadsafe
{
	CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

	TArray<char> Combinations;
	char* pPtr = NULL;
	CCryNameTSCRC Name;
	SResourceContainer* pRL;

	Name = CHWShader::mfGetClassName(eHWSC_Vertex);
	pRL = CBaseResource::GetResourcesForClass(Name);
	int nVS = 0;
	int nPS = 0;
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CHWShader* vs = (CHWShader*)itor->second;
			if (!vs)
				continue;
			const char* szCombs = vs->mfGetActivatedCombinations(bForLevel);
			if (!szCombs)
				continue;
			Combinations.Copy(szCombs, strlen(szCombs));
			delete[] szCombs;
			nVS++;
		}
	}

	Name = CHWShader::mfGetClassName(eHWSC_Pixel);
	pRL = CBaseResource::GetResourcesForClass(Name);
	int n = 0;
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CHWShader* ps = (CHWShader*)itor->second;
			if (!ps)
				continue;
			const char* szCombs = ps->mfGetActivatedCombinations(bForLevel);
			if (!szCombs)
				continue;
			Combinations.Copy(szCombs, strlen(szCombs));
			delete[] szCombs;
			nPS++;
		}
	}

	if (!Combinations.Num())
		return NULL;
	pPtr = new char[Combinations.Num() + 1];
	memcpy(pPtr, &Combinations[0], Combinations.Num());
	pPtr[Combinations.Num()] = 0;
	return pPtr;
}

