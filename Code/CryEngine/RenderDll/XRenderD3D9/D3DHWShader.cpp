// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryCore/CryCrc32.h>
#include "../Common/Shaders/RemoteCompiler.h"
#include "../Common/PostProcess/PostEffects.h"
#include "D3DPostProcess.h"

#include <CryParticleSystem/IParticles.h>
#include "../Common/Textures/TextureHelpers.h"
#include "../Common/Include_HLSL_CPP_Shared.h"
#include "../Common/TypedConstantBuffer.h"
#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif
#include "GraphicsPipeline/VolumetricClouds.h"
#include "GraphicsPipeline/VolumetricFog.h"
#include "GraphicsPipeline/WaterRipples.h"
#include "GraphicsPipeline/Water.h"

#include "Common/RenderView.h"

#define MAX_PF_TEXTURES (32)
#define MAX_PF_SAMPLERS (4)

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning(disable: 4244)
#endif

//=======================================================================

CRY_ALIGN(16) Vec4 CHWShader_D3D::s_CurPSParams[MAX_CONSTANTS_PS];
CRY_ALIGN(16) Vec4 CHWShader_D3D::s_CurVSParams[MAX_CONSTANTS_VS];

#ifdef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES
D3DBuffer** CHWShader_D3D::s_pCB[eHWSC_Num][CB_NUM];
D3DBuffer* CHWShader_D3D::s_pCurReqCB[eHWSC_Num][CB_NUM];
#else
CConstantBuffer** CHWShader_D3D::s_pCB[eHWSC_Num][CB_NUM];
CConstantBuffer* CHWShader_D3D::s_pCurReqCB[eHWSC_Num][CB_NUM];
#endif
uint64 CHWShader_D3D::s_pCurDevCB[eHWSC_Num][CB_NUM];
Vec4* CHWShader_D3D::s_pDataCB[eHWSC_Num][CB_NUM];
int CHWShader_D3D::s_nCurMaxVecs[eHWSC_Num][CB_NUM];
int CHWShader_D3D::s_nMax_PF_Vecs[eHWSC_Num];
int CHWShader_D3D::s_nMax_SG_Vecs[eHWSC_Num];

CHWShader_D3D::SHWSInstance* CHWShader_D3D::s_pCurInstGS;
bool CHWShader_D3D::s_bFirstGS = true;
CHWShader_D3D::SHWSInstance* CHWShader_D3D::s_pCurInstHS;
bool CHWShader_D3D::s_bFirstHS = true;
CHWShader_D3D::SHWSInstance* CHWShader_D3D::s_pCurInstDS;
bool CHWShader_D3D::s_bFirstDS = true;
CHWShader_D3D::SHWSInstance* CHWShader_D3D::s_pCurInstCS;
bool CHWShader_D3D::s_bFirstCS = true;
CHWShader_D3D::SHWSInstance* CHWShader_D3D::s_pCurInstVS;
bool CHWShader_D3D::s_bFirstVS = true;
CHWShader_D3D::SHWSInstance* CHWShader_D3D::s_pCurInstPS;
bool CHWShader_D3D::s_bFirstPS = true;

int CHWShader_D3D::s_nActivationFailMask = 0;

std::vector<SCGParam> CHWShader_D3D::s_CM_Params[eHWSC_Num];

std::vector<SCGTexture> CHWShader_D3D::s_PF_Textures;        // Per-frame textures
std::vector<STexSamplerRT> CHWShader_D3D::s_PF_Samplers;     // Per-frame samplers

std::vector<SShaderTechniqueStat> g_SelectedTechs;

#if !defined(_RELEASE)
std::set<uint32_t> g_ErrorsLogged; // CRC32 of message, risk of collision acceptable
#endif

bool CHWShader_D3D::s_bInitShaders = true;

int CHWShader_D3D::s_PSParamsToCommit[256];
int CHWShader_D3D::s_NumPSParamsToCommit;
int CHWShader_D3D::s_VSParamsToCommit[256];
int CHWShader_D3D::s_NumVSParamsToCommit;

int CHWShader_D3D::s_nResetDeviceFrame = -1;
int CHWShader_D3D::s_nInstFrame = -1;

SD3DShader* CHWShader::s_pCurPS;
SD3DShader* CHWShader::s_pCurVS;
SD3DShader* CHWShader::s_pCurGS;
SD3DShader* CHWShader::s_pCurDS;
SD3DShader* CHWShader::s_pCurHS;
SD3DShader* CHWShader::s_pCurCS;

CHWShader* CHWShader::s_pCurHWVS;
char *CHWShader::s_GS_MultiRes_NV;

FXShaderCache CHWShader::m_ShaderCache;
FXShaderDevCache CHWShader::m_ShaderDevCache;
FXShaderCacheNames CHWShader::m_ShaderCacheList;

//extern float fTime0;
//extern float fTime1;
//extern float fTime2;

namespace
{
static inline void TransposeAndStore(UFloat4* sData, const Matrix44A& mMatrix)
{
	alias_cast<Matrix44A*>(sData)->Transpose(mMatrix);
}

static inline void Store(UFloat4* sData, const Matrix44A& mMatrix)
{
	*alias_cast<Matrix44A*>(sData) = mMatrix;
}

static inline void Store(UFloat4* sData, const Matrix34A& mMatrix)
{
	*alias_cast<Matrix44A*>(&sData[0]) = mMatrix;
}
}

#if CRY_PLATFORM_SSE2
// Matrix multiplication using SSE instructions set
// IMPORTANT NOTE: much faster if matrices m1 and product are 16 bytes aligned
inline void multMatrixf_Transp2_SSE(float* product, const float* m1, const float* m2)
{
	__m128 x0 = _mm_load_ss(m2);
	__m128 x1 = _mm_load_ps(m1);
	x0 = _mm_shuffle_ps(x0, x0, 0);
	__m128 x2 = _mm_load_ss(&m2[4]);
	x0 = _mm_mul_ps(x0, x1);
	x2 = _mm_shuffle_ps(x2, x2, 0);
	__m128 x3 = _mm_load_ps(&m1[4]);
	__m128 x4 = _mm_load_ss(&m2[8]);
	x2 = _mm_mul_ps(x2, x3);
	x4 = _mm_shuffle_ps(x4, x4, 0);
	x0 = _mm_add_ps(x0, x2);
	x2 = _mm_load_ps(&m1[8]);
	x4 = _mm_mul_ps(x4, x2);
	__m128 x6 = _mm_load_ps(&m1[12]);
	x0 = _mm_add_ps(x0, x4);
	_mm_store_ps(product, x0);
	x0 = _mm_load_ss(&m2[1]);
	x4 = _mm_load_ss(&m2[5]);
	x0 = _mm_shuffle_ps(x0, x0, 0);
	x4 = _mm_shuffle_ps(x4, x4, 0);
	x0 = _mm_mul_ps(x0, x1);
	x4 = _mm_mul_ps(x4, x3);
	__m128 x5 = _mm_load_ss(&m2[9]);
	x0 = _mm_add_ps(x0, x4);
	x5 = _mm_shuffle_ps(x5, x5, 0);
	x5 = _mm_mul_ps(x5, x2);
	x0 = _mm_add_ps(x0, x5);
	_mm_store_ps(&product[4], x0);
	x0 = _mm_load_ss(&m2[2]);
	x4 = _mm_load_ss(&m2[6]);
	x0 = _mm_shuffle_ps(x0, x0, 0);
	x4 = _mm_shuffle_ps(x4, x4, 0);
	x0 = _mm_mul_ps(x0, x1);
	x4 = _mm_mul_ps(x4, x3);
	x5 = _mm_load_ss(&m2[10]);
	x0 = _mm_add_ps(x0, x4);
	x5 = _mm_shuffle_ps(x5, x5, 0);
	x5 = _mm_mul_ps(x5, x2);
	x0 = _mm_add_ps(x0, x5);
	_mm_store_ps(&product[8], x0);
	x0 = _mm_load_ss(&m2[3]);
	x4 = _mm_load_ss(&m2[7]);
	x0 = _mm_shuffle_ps(x0, x0, 0);
	x4 = _mm_shuffle_ps(x4, x4, 0);
	x0 = _mm_mul_ps(x0, x1);
	x4 = _mm_mul_ps(x4, x3);
	x1 = _mm_load_ss(&m2[11]);
	x0 = _mm_add_ps(x0, x4);
	x1 = _mm_shuffle_ps(x1, x1, 0);
	x1 = _mm_mul_ps(x1, x2);
	x1 = _mm_add_ps(x1, x6);
	x0 = _mm_add_ps(x0, x1);
	_mm_store_ps(&product[12], x0);
}
#endif

inline void multMatrixf_Transp2(float* product, const float* m1, const float* m2)
{
	float temp[16];

#define A(row, col) m1[(col << 2) + row]
#define B(row, col) m2[(col << 2) + row]
#define P(row, col) temp[(col << 2) + row]

	int i;
	for (i = 0; i < 4; i++)
	{
		float ai0 = A(i, 0), ai1 = A(i, 1), ai2 = A(i, 2), ai3 = A(i, 3);
		P(i, 0) = ai0 * B(0, 0) + ai1 * B(0, 1) + ai2 * B(0, 2);
		P(i, 1) = ai0 * B(1, 0) + ai1 * B(1, 1) + ai2 * B(1, 2);
		P(i, 2) = ai0 * B(2, 0) + ai1 * B(2, 1) + ai2 * B(2, 2);
		P(i, 3) = ai0 * B(3, 0) + ai1 * B(3, 1) + ai2 * B(3, 2) + ai3;
	}
	// P[i,j] = A[*,i] * B[*,j] = A.transpose * B;
	cryMemcpy(product, temp, sizeof(temp));

#undef A
#undef B
#undef P
}

inline void mathMatrixMultiply_Transp2(float* pOut, const float* pM1, const float* pM2)
{
#if CRY_PLATFORM_SSE2
	multMatrixf_Transp2_SSE(pOut, pM1, pM2);
#else
	multMatrixf_Transp2(pOut, pM1, pM2);
#endif
}

int SD3DShader::Release(EHWShaderClass eSHClass, int nSize)
{
	m_nRef--;
	if (m_nRef)
		return m_nRef;
	void* pHandle = m_pHandle;
	delete this;
	if (!pHandle)
		return 0;
	if (eSHClass == eHWSC_Pixel)
		CHWShader_D3D::s_nDevicePSDataSize -= nSize;
	else
		CHWShader_D3D::s_nDeviceVSDataSize -= nSize;

	if (eSHClass == eHWSC_Pixel)
		return ((ID3D11PixelShader*)pHandle)->Release();
	else if (eSHClass == eHWSC_Vertex)
		return ((ID3D11VertexShader*)pHandle)->Release();
	else if (eSHClass == eHWSC_Geometry)
		return ((ID3D11GeometryShader*)pHandle)->Release();
	else if (eSHClass == eHWSC_Hull)
		return ((ID3D11HullShader*)pHandle)->Release();
	else if (eSHClass == eHWSC_Compute)
		return ((ID3D11ComputeShader*)pHandle)->Release();
	else if (eSHClass == eHWSC_Domain)
		return ((ID3D11DomainShader*)pHandle)->Release();
	else
	{
		assert(0);
		return 0;
	}
}

void CHWShader_D3D::SHWSInstance::Release(SShaderDevCache* pCache, bool bReleaseData)
{
	//SAFE_DELETE(m_pSamplers);
	//SAFE_DELETE(m_pBindVars);
	//SAFE_DELETE(m_pParams[0]);
	//SAFE_DELETE(m_pParams[1]);
	//SAFE_DELETE(m_pParams_Inst);
	if (m_nParams[0] >= 0)
		CGParamManager::FreeParametersGroup(m_nParams[0]);
	if (m_nParams[1] >= 0)
		CGParamManager::FreeParametersGroup(m_nParams[1]);
	if (m_nParams_Inst >= 0)
		CGParamManager::FreeParametersGroup(m_nParams_Inst);

	int nCount = -1;
	if (m_Handle.m_pShader)
	{
		if (m_eClass == eHWSC_Pixel)
		{
			SD3DShader* pPS = m_Handle.m_pShader;
			if (pPS)
			{
				nCount = m_Handle.Release(m_eClass, m_Shader.m_nDataSize);
				if (!nCount && CHWShader::s_pCurPS == pPS)
					CHWShader::s_pCurPS = NULL;
			}
		}
		else if (m_eClass == eHWSC_Vertex)
		{
			SD3DShader* pVS = m_Handle.m_pShader;
			if (pVS)
			{
				nCount = m_Handle.Release(m_eClass, m_Shader.m_nDataSize);
				if (!nCount && CHWShader::s_pCurVS == pVS)
					CHWShader::s_pCurVS = NULL;
			}
		}
		else if (m_eClass == eHWSC_Geometry)
		{
			SD3DShader* pGS = m_Handle.m_pShader;
			if (pGS)
			{
				nCount = m_Handle.Release(m_eClass, m_Shader.m_nDataSize);
				if (!nCount && CHWShader::s_pCurGS == pGS)
					CHWShader::s_pCurGS = NULL;
			}
		}
		else if (m_eClass == eHWSC_Hull)
		{
			SD3DShader* pHS = m_Handle.m_pShader;
			if (pHS)
			{
				nCount = m_Handle.Release(m_eClass, m_Shader.m_nDataSize);
				if (!nCount && CHWShader::s_pCurHS == pHS)
					CHWShader::s_pCurHS = NULL;
			}
		}
		else if (m_eClass == eHWSC_Compute)
		{
			SD3DShader* pCS = m_Handle.m_pShader;
			if (pCS)
			{
				nCount = m_Handle.Release(m_eClass, m_Shader.m_nDataSize);
				if (!nCount && CHWShader::s_pCurCS == pCS)
					CHWShader::s_pCurCS = NULL;
			}
		}
		else if (m_eClass == eHWSC_Domain)
		{
			SD3DShader* pDS = m_Handle.m_pShader;
			if (pDS)
			{
				nCount = m_Handle.Release(m_eClass, m_Shader.m_nDataSize);
				if (!nCount && CHWShader::s_pCurDS == pDS)
					CHWShader::s_pCurDS = NULL;
			}
		}
	}

	if (m_Shader.m_pShaderData)
	{
		delete[] (char*)m_Shader.m_pShaderData;
		m_Shader.m_pShaderData = NULL;
	}

	if (!nCount && pCache && !pCache->m_DeviceShaders.empty())
		pCache->m_DeviceShaders.erase(m_DeviceObjectID);
	m_Handle.m_pShader = NULL;
}

void CHWShader_D3D::SHWSInstance::GetInstancingAttribInfo(uint8 Attributes[32], int32& nUsedAttr, int& nInstAttrMask)
{
	Attributes[0] = (byte)m_nInstMatrixID;
	Attributes[1] = Attributes[0] + 1;
	Attributes[2] = Attributes[0] + 2;
	nInstAttrMask = 0x7 << m_nInstMatrixID;
	if (m_nParams_Inst >= 0)
	{
		SCGParamsGroup& Group = CGParamManager::s_Groups[m_nParams_Inst];
		uint32 nSize = Group.nParams;
		for (uint32 j = 0; j < nSize; ++j)
		{
			SCGParam* pr = &Group.pParams[j];
			for (uint32 na = 0; na < (uint32)pr->m_nParameters; ++na)
			{
				Attributes[nUsedAttr + na] = pr->m_dwBind + na;
				nInstAttrMask |= 1 << Attributes[nUsedAttr + na];
			}
			nUsedAttr += pr->m_nParameters;
		}
	}
}

void CHWShader_D3D::InitialiseContainers()
{
	uint32 i;
	for (i = 0; i < eHWSC_Num; i++)
	{
		s_CM_Params[i].reserve(8);
	}
	s_PF_Textures.reserve(MAX_PF_TEXTURES);
	s_PF_Samplers.reserve(MAX_PF_SAMPLERS);
}

void CHWShader_D3D::ShutDown()
{
	CCryNameTSCRC Name;
	SResourceContainer* pRL;

	uint32 numResourceLeaks = 0;

	// First make sure all HW and FX shaders are released
	Name = CHWShader::mfGetClassName(eHWSC_Vertex);
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CHWShader* vsh = (CHWShader*)itor->second;
			if (vsh)
				++numResourceLeaks;
		}
		pRL->m_RList.clear();
		pRL->m_AvailableIDs.clear();
	}

	Name = CHWShader::mfGetClassName(eHWSC_Pixel);
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CHWShader* psh = (CHWShader*)itor->second;
			if (psh)
				++numResourceLeaks;
		}
		pRL->m_RList.clear();
		pRL->m_AvailableIDs.clear();
	}

	Name = CShader::mfGetClassName();
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CShader* sh = (CShader*)itor->second;
			if (!sh->m_DerivedShaders)
				numResourceLeaks++;
		}
		if (!pRL->m_RMap.size())
		{
			pRL->m_RList.clear();
			pRL->m_AvailableIDs.clear();
		}
	}

	if (numResourceLeaks > 0)
	{
		iLog->LogWarning("Detected shader resource leaks on shutdown");
	}

	for (int i = 0; i < eHWSC_Num; i++)
	{
		stl::free_container(s_CM_Params[i]);
	}
	stl::free_container(s_PF_Textures);
	stl::free_container(s_PF_Samplers);

	gRenDev->m_cEF.m_Bin.mfReleaseFXParams();

	while (!m_ShaderCache.empty())
	{
		SShaderCache* pC = m_ShaderCache.begin()->second;
		CRY_ASSERT(pC->m_nRefCount == 1);

		SAFE_RELEASE(pC);
	}
	m_ShaderCacheList.clear();
	g_SelectedTechs.clear();
#if !defined(_RELEASE)
	g_ErrorsLogged.clear();
#endif
	CGParamManager::Shutdown();

	// free allocated constant buffers
#ifndef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES

	int maxConstantsPerShaderStage[] =
	{
		MAX_CONSTANTS_VS, // eHWSC_Vertex
		MAX_CONSTANTS_PS, // eHWSC_Pixel
		MAX_CONSTANTS_GS, // eHWSC_Geometry
		MAX_CONSTANTS_CS, // eHWSC_Compute
		MAX_CONSTANTS_DS, // eHWSC_Domain
		MAX_CONSTANTS_HS, // eHWSC_Hull

	};

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		for (int cbType = 0; cbType < CB_NUM; ++cbType)
		{
			if (s_pCB[shaderClass][cbType])
			{
				for (int s = 0; s < maxConstantsPerShaderStage[shaderClass]; ++s)
					SAFE_RELEASE(s_pCB[shaderClass][cbType][s]);

				SAFE_DELETE_ARRAY(s_pCB[shaderClass][cbType]);
			}
		}
	}

#endif

}

CHWShader* CHWShader::mfForName(const char* name, const char* nameSource, uint32 CRC32, const char* szEntryFunc, EHWShaderClass eClass, TArray<uint32>& SHData, FXShaderToken* pTable, uint32 dwType, CShader* pFX, uint64 nMaskGen, uint64 nMaskGenFX)
{
	//	LOADING_TIME_PROFILE_SECTION(iSystem);
	if (!name || !name[0])
		return NULL;

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Shader, 0, "%s", name);

	CHWShader_D3D* pSH = NULL;
	stack_string strName = name;
	CCryNameTSCRC className = mfGetClassName(eClass);
	stack_string AddStr;

	if (nMaskGen)
	{
#ifdef __GNUC__
		strName += AddStr.Format("(%llx)", nMaskGen);
#else
		strName += AddStr.Format("(%I64x)", nMaskGen);
#endif
	}
	if (CParserBin::m_nPlatform == SF_ORBIS)
		strName += AddStr.Format("(O)");
	else if (CParserBin::m_nPlatform == SF_DURANGO)
		strName += AddStr.Format("(D)");
	else if (CParserBin::m_nPlatform == SF_D3D11)
		strName += AddStr.Format("(X1)", nMaskGen);
	else if (CParserBin::m_nPlatform == SF_GL4)
		strName + AddStr.Format("(G4)", nMaskGen);
	else if (CParserBin::m_nPlatform == SF_GLES3)
		strName + AddStr.Format("(E3)", nMaskGen);

	CCryNameTSCRC Name = strName.c_str();
	CBaseResource* pBR = CBaseResource::GetResource(className, Name, false);
	if (!pBR)
	{
		pSH = new CHWShader_D3D;
		pSH->m_Name = strName.c_str();
		pSH->m_NameSourceFX = nameSource;
		pSH->Register(className, Name);
		pSH->m_EntryFunc = szEntryFunc;
		pSH->mfFree(CRC32);
	}
	else
	{
		pSH = (CHWShader_D3D*)pBR;
		pSH->AddRef();
		if (pSH->m_CRC32 == CRC32)
		{
			if (pTable && CRenderer::CV_r_shadersAllowCompilation)
			{
				FXShaderToken* pMap = pTable;
				TArray<uint32>* pData = &SHData;
				if (pData->size())
				  pSH->mfGetCacheTokenMap(pMap, pData, pSH->m_nMaskGenShader);
			}
			return pSH;
		}
		pSH->mfFree(CRC32);
		pSH->m_CRC32 = CRC32;
	}

	if (CParserBin::m_bEditable || (CVrProjectionManager::IsMultiResEnabledStatic() && eClass == eHWSC_Vertex))
	{
		if (pTable)
			pSH->m_TokenTable = *pTable;
		pSH->m_TokenData = SHData;
	}

	pSH->m_dwShaderType = dwType;
	pSH->m_eSHClass = eClass;
	pSH->m_nMaskGenShader = nMaskGen;
	pSH->m_nMaskGenFX = nMaskGenFX;
	pSH->m_CRC32 = CRC32;
	// Check for auto MultiRes-Geom shader
	if (eClass == eHWSC_Geometry && szEntryFunc[0] == '$')
	{
		if (!CVrProjectionManager::IsMultiResEnabledStatic())
		{
			pSH->Release();
			return NULL;
		}
		pSH->m_Flags |= HWSG_GS_MULTIRES;
		SShaderBin *pBinGS = gRenDev->m_cEF.m_Bin.GetBinShader("$nv_vr", false, pFX->m_CRC32);
		if (pBinGS)
		{
			CParserBin Parser(pBinGS, pFX);
			CShader* efGen = pFX->m_pGenShader;
			if (efGen && efGen->m_ShaderGenParams)
				gRenDev->m_cEF.m_Bin.AddGenMacros(efGen->m_ShaderGenParams, Parser, pFX->m_nMaskGenFX);
			Parser.Preprocess(0, pBinGS->m_Tokens, &pBinGS->m_TokenTable);
			pSH->m_TokenTable = Parser.m_TokenTable;
			pSH->m_TokenData.Copy(&Parser.m_Tokens[0], Parser.m_Tokens.size());
		}
	}

	pSH->mfConstructFX(pTable, &SHData);

	return pSH;
}

void CHWShader_D3D::SetTokenFlags(uint32 nToken)
{
	switch (nToken)
	{
	case eT__LT_LIGHTS:
		m_Flags |= HWSG_SUPPORTS_LIGHTING;
		break;
	case eT__LT_0_TYPE:
	case eT__LT_1_TYPE:
	case eT__LT_2_TYPE:
	case eT__LT_3_TYPE:
		m_Flags |= HWSG_SUPPORTS_MULTILIGHTS;
		break;
	case eT__TT_TEXCOORD_MATRIX:
	case eT__TT_TEXCOORD_GEN_OBJECT_LINEAR:
		m_Flags |= HWSG_SUPPORTS_MODIF;
		break;
	case eT__VT_TYPE:
		m_Flags |= HWSG_SUPPORTS_VMODIF;
		break;
	case eT__FT_TEXTURE:
		m_Flags |= HWSG_FP_EMULATION;
		break;
	}
}

uint64 CHWShader_D3D::CheckToken(uint32 nToken)
{
	uint64 nMask = 0;
	SShaderGen* pGen = gRenDev->m_cEF.m_pGlobalExt;
	uint32 i;
	for (i = 0; i < pGen->m_BitMask.Num(); i++)
	{
		SShaderGenBit* bit = pGen->m_BitMask[i];
		if (!bit)
			continue;

		if (bit->m_dwToken == nToken)
		{
			nMask |= bit->m_Mask;
			break;
		}
	}
	if (!nMask)
		SetTokenFlags(nToken);

	return nMask;
}

uint64 CHWShader_D3D::CheckIfExpr_r(uint32* pTokens, uint32& nCur, uint32 nSize)
{
	uint64 nMask = 0;

	while (nCur < nSize)
	{
		int nRecurs = 0;
		uint32 nToken = pTokens[nCur++];
		if (nToken == eT_br_rnd_1) // check for '('
		{
			uint32 tmpBuf[64];
			int n = 0;
			int nD = 0;
			while (true)
			{
				nToken = pTokens[nCur];
				if (nToken == eT_br_rnd_1) // check for '('
					n++;
				else if (nToken == eT_br_rnd_2) // check for ')'
				{
					if (!n)
					{
						tmpBuf[nD] = 0;
						nCur++;
						break;
					}
					n--;
				}
				else if (nToken == 0)
					return nMask;
				tmpBuf[nD++] = nToken;
				nCur++;
			}
			if (nD)
			{
				uint32 nC = 0;
				nMask |= CheckIfExpr_r(tmpBuf, nC, nSize);
			}
		}
		else
		{
			bool bNeg = false;
			if (nToken == eT_excl)
			{
				bNeg = true;
				nToken = pTokens[nCur++];
			}
			nMask |= CheckToken(nToken);
		}
		nToken = pTokens[nCur];
		if (nToken == eT_or)
		{
			nCur++;
			assert(pTokens[nCur] == eT_or);
			if (pTokens[nCur] == eT_or)
				nCur++;
		}
		else if (nToken == eT_and)
		{
			nCur++;
			assert(pTokens[nCur] == eT_and);
			if (pTokens[nCur] == eT_and)
				nCur++;
		}
		else
			break;
	}
	return nMask;
}

void CHWShader_D3D::mfConstructFX_Mask_RT(FXShaderToken* Table, TArray<uint32>* pSHData)
{
	assert(gRenDev->m_cEF.m_pGlobalExt);
	m_nMaskAnd_RT = 0;
	m_nMaskOr_RT = 0;
	if (!gRenDev->m_cEF.m_pGlobalExt)
		return;
	SShaderGen* pGen = gRenDev->m_cEF.m_pGlobalExt;

	assert(!pSHData->empty());
	uint32* pTokens = &(*pSHData)[0];
	uint32 nSize = pSHData->size();
	uint32 nCur = 0;
	while (nCur < nSize)
	{
		uint32 nTok = CParserBin::NextToken(pTokens, nCur, nSize - 1);
		if (!nTok)
			continue;
		if (nTok >= eT_if && nTok <= eT_elif_2)
			m_nMaskAnd_RT |= CheckIfExpr_r(pTokens, nCur, nSize);
		else
			SetTokenFlags(nTok);
	}

	// Reset any RT bits for this shader if this shader type is not existing for specific bit
	// See Runtime.ext file
	if (m_dwShaderType)
	{
		for (uint32 i = 0; i < pGen->m_BitMask.Num(); i++)
		{
			SShaderGenBit* bit = pGen->m_BitMask[i];
			if (!bit)
				continue;
			if (bit->m_Flags & SHGF_RUNTIME)
				continue;

			uint32 j;
			if (bit->m_PrecacheNames.size())
			{
				for (j = 0; j < bit->m_PrecacheNames.size(); j++)
				{
					if (m_dwShaderType == bit->m_PrecacheNames[j])
						break;
				}
				if (j == bit->m_PrecacheNames.size())
					m_nMaskAnd_RT &= ~bit->m_Mask;
			}
			else
				m_nMaskAnd_RT &= ~bit->m_Mask;
		}
	}
	mfSetDefaultRT(m_nMaskAnd_RT, m_nMaskOr_RT);
}

void CHWShader_D3D::mfConstructFX(FXShaderToken* Table, TArray<uint32>* pSHData)
{
	if (!strnicmp(m_EntryFunc.c_str(), "Sync_", 5))
		m_Flags |= HWSG_SYNC;

	if (!pSHData->empty())
		mfConstructFX_Mask_RT(Table, pSHData);
	else
	{
		m_nMaskAnd_RT = -1;
		m_nMaskOr_RT = 0;
	}

	if (Table && CRenderer::CV_r_shadersAllowCompilation)
	{
		FXShaderToken* pMap = Table;
		TArray<uint32>* pData = pSHData;
		if (pData->size())
		  mfGetCacheTokenMap(pMap, pData, m_nMaskGenShader);   // Store tokens
	}
}

bool CHWShader_D3D::mfPrecache(SShaderCombination& cmb, bool bForce, bool bFallback, CShader* pSH, CShaderResources* pRes)
{
#if CRY_RENDERER_VULKAN || CRY_RENDERER_GNM
	return false;
#endif

	assert(gRenDev->m_pRT->IsRenderThread());

	bool bRes = true;

	if (!CRenderer::CV_r_shadersAllowCompilation && !bForce)
		return bRes;

	uint64 AndRTMask = 0;
	uint64 OrRTMask = 0;
	mfSetDefaultRT(AndRTMask, OrRTMask);
	SShaderCombIdent Ident;
	Ident.m_RTMask = cmb.m_RTMask & AndRTMask | OrRTMask;
	Ident.m_pipelineState.opaque = cmb.m_pipelineState.opaque;
	Ident.m_MDVMask = cmb.m_MDVMask;
	if (m_eSHClass == eHWSC_Pixel)
		Ident.m_MDVMask = CParserBin::m_nPlatform;
	if (m_Flags & HWSG_SUPPORTS_MULTILIGHTS)
		Ident.m_LightMask = 1;
	Ident.m_GLMask = m_nMaskGenShader;
	uint32 nFlags = HWSF_PRECACHE;
	if (m_eSHClass == eHWSC_Pixel && pRes)
	{
		SHWSInstance* pInst = mfGetInstance(pSH, Ident, HWSF_PRECACHE_INST);
		pInst->m_bFallback = bFallback;
		int nResult = mfCheckActivation(pSH, pInst, HWSF_PRECACHE);
		if (!nResult)
			return bRes;
		mfUpdateSamplers(pSH);
		pInst->m_fLastAccess = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_RealTime;
		Ident.m_MDMask = gRenDev->m_RP.m_FlagsShader_MD & ~HWMD_TEXCOORD_FLAG_MASK;
	}
	if (m_eSHClass == eHWSC_Pixel && gRenDev->m_RP.m_pShaderResources)
		Ident.m_MDMask &= ~HWMD_TEXCOORD_FLAG_MASK;

	if (Ident.m_MDMask || bForce)
	{
		SHWSInstance* pInst = mfGetInstance(pSH, Ident, HWSF_PRECACHE_INST);
		pInst->m_bFallback = bFallback;
		pInst->m_fLastAccess = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_RealTime;
		mfActivate(pSH, nFlags, NULL, NULL);
	}

	return bRes;
}

//==============================================================================================

DynArray<SCGParamPool> CGParamManager::s_Pools;
std::vector<SCGParamsGroup> CGParamManager::s_Groups;
std::vector<uint32, stl::STLGlobalAllocator<uint32>> CGParamManager::s_FreeGroups;

SCGParamPool::SCGParamPool(int nEntries)
	: m_Params(new SCGParam[nEntries], nEntries)
{
}

SCGParamPool::~SCGParamPool()
{
	delete[] m_Params.begin();
}

SCGParamsGroup SCGParamPool::Alloc(int nEntries)
{
	SCGParamsGroup Group;

	alloc_info_struct* pAI = gRenDev->GetFreeChunk(nEntries, m_Params.size(), m_alloc_info, "CGParam");
	if (pAI)
	{
		Group.nParams = nEntries;
		Group.pParams = &m_Params[pAI->ptr];
	}

	return Group;
}

bool SCGParamPool::Free(SCGParamsGroup& Group)
{
	bool bRes = gRenDev->ReleaseChunk((int)(Group.pParams - m_Params.begin()), m_alloc_info);
	return bRes;
}

int CGParamManager::GetParametersGroup(SParamsGroup& InGr, int nId)
{
	std::vector<SCGParam>& InParams = nId > 1 ? InGr.Params_Inst : InGr.Params[nId];
	int32 i;
	int nParams = InParams.size();

	int nGroupSize = s_Groups.size();
	for (i = 0; i < nGroupSize; i++)
	{
		SCGParamsGroup& Gr = s_Groups[i];
		if (Gr.nParams != nParams)
			continue;
		int j;
		for (j = 0; j < nParams; j++)
		{
			if (InParams[j] != Gr.pParams[j])
				break;
		}
		if (j == nParams)
		{
			Gr.nRefCounter++;
			return i;
		}
	}

	SCGParamsGroup Group;
	SCGParamPool* pPool = NULL;
	for (i = 0; i < s_Pools.size(); i++)
	{
		pPool = &s_Pools[i];
		Group = pPool->Alloc(nParams);
		if (Group.nParams)
			break;
	}
	if (!Group.pParams)
	{
		pPool = NewPool(PARAMS_POOL_SIZE);
		Group = pPool->Alloc(nParams);
	}
	assert(Group.pParams);
	if (!Group.pParams)
		return 0;
	Group.nPool = i;
	uint32 n = s_Groups.size();
	if (s_FreeGroups.size())
	{
		n = s_FreeGroups.back();
		s_FreeGroups.pop_back();
		s_Groups[n] = Group;
	}
	else
	{
		s_Groups.push_back(Group);
	}

	for (i = 0; i < nParams; i++)
	{
		s_Groups[n].pParams[i] = InParams[i];
	}
	bool bGeneral = true;
	if (nId == 0)
	{
		for (i = 0; i < nParams; i++)
		{
			SCGParam& Pr = InParams[i];
			if ((Pr.m_eCGParamType & 0xff) != ECGP_PM_Tweakable &&
			    Pr.m_eCGParamType != ECGP_PM_MatChannelSB &&
			    Pr.m_eCGParamType != ECGP_PM_MatDiffuseColor &&
			    Pr.m_eCGParamType != ECGP_PM_MatSpecularColor &&
			    Pr.m_eCGParamType != ECGP_PM_MatEmissiveColor &&
			    Pr.m_eCGParamType != ECGP_PM_MatMatrixTCM &&
			    Pr.m_eCGParamType != ECGP_PM_MatDeformWave &&
			    Pr.m_eCGParamType != ECGP_PM_MatDetailTilingAndAlphaRef &&
			    Pr.m_eCGParamType != ECGP_PM_MatSilPomDetailParams)
			{
				bGeneral = false;
				break;
			}
		}
	}
	else if (nId == 1)
	{
		for (i = 0; i < nParams; i++)
		{
			SCGParam& Pr = InParams[i];
			if (Pr.m_eCGParamType != ECGP_SI_AmbientOpacity &&
			    Pr.m_eCGParamType != ECGP_SI_BendInfo &&
			    Pr.m_eCGParamType != ECGP_Matr_SI_Obj)
			{
				bGeneral = false;
				break;
			}
		}
	}
	s_Groups[n].bGeneral = bGeneral;

	return n;
}

bool CGParamManager::FreeParametersGroup(int nIDGroup)
{
	assert(nIDGroup >= 0 && nIDGroup < (int)s_Groups.size());
	if (nIDGroup < 0 || nIDGroup >= (int)s_Groups.size())
		return false;
	SCGParamsGroup& Group = s_Groups[nIDGroup];
	Group.nRefCounter--;
	if (Group.nRefCounter)
		return true;
	assert(Group.nPool >= 0 && Group.nPool < s_Pools.size());
	if (Group.nPool < 0 || Group.nPool >= s_Pools.size())
		return false;
	SCGParamPool& Pool = s_Pools[Group.nPool];
	if (!Pool.Free(Group))
		return false;
	for (int i = 0; i < Group.nParams; i++)
	{
		Group.pParams[i].m_Name.reset();
		SAFE_DELETE(Group.pParams[i].m_pData);
	}

	Group.nParams = 0;
	Group.nPool = 0;
	Group.pParams = 0;

	s_FreeGroups.push_back(nIDGroup);

	return true;
}

void CGParamManager::Init()
{
	s_FreeGroups.reserve(128); // Based on spear
	s_Groups.reserve(2048);
}
void CGParamManager::Shutdown()
{
	s_FreeGroups.clear();
	s_Pools.clear();
	s_Groups.clear();
}

SCGParamPool* CGParamManager::NewPool(int nEntries)
{
  return s_Pools.emplace_back(nEntries);
}

//===========================================================================================================

const SWaveForm sWFX = SWaveForm(eWF_Sin, 0, 3.5f, 0, 0.2f);
const SWaveForm sWFY = SWaveForm(eWF_Sin, 0, 5.0f, 90.0f, 0.2f);

CRY_ALIGN(16) UFloat4 sDataBuffer[48];
CRY_ALIGN(16) float sTempData[32][4];
CRY_ALIGN(16) float sMatrInstData[3][4];

namespace
{
NO_INLINE void sIdentityLine(UFloat4* sData)
{
	sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.f;
	sData[0].f[3] = 1.0f;
}
NO_INLINE void sOneLine(UFloat4* sData)
{
	sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 1.f;
	sData[0].f[3] = 1.0f;
}
NO_INLINE void sZeroLine(UFloat4* sData)
{
	sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.f;
	sData[0].f[3] = 0.0f;
}

NO_INLINE CRenderElement* sGetContainerRE0(CRenderElement* pRE)
{
	assert(pRE);      // someone assigned wrong shader - function should not be called then

	if (pRE->mfGetType() == eDATA_Mesh && ((CREMeshImpl*)pRE)->m_pRenderMesh->_GetVertexContainer())
	{
		assert(((CREMeshImpl*)pRE)->m_pRenderMesh->_GetVertexContainer()->m_Chunks.size() >= 1);
		return ((CREMeshImpl*)pRE)->m_pRenderMesh->_GetVertexContainer()->m_Chunks[0].pRE;
	}

	return pRE;
}

NO_INLINE float* sGetTerrainBase(UFloat4* sData, CD3D9Renderer* r)
{
	if (r->m_RP.m_pCurObject && r->m_RP.m_pCurObject->m_data.m_pTerrainSectorTextureInfo)
	{
		sData[0].f[0] = r->m_RP.m_pCurObject->m_data.m_pTerrainSectorTextureInfo->fTexScale;
		sData[0].f[1] = r->m_RP.m_pCurObject->m_data.m_pTerrainSectorTextureInfo->fTexOffsetX;
		sData[0].f[2] = r->m_RP.m_pCurObject->m_data.m_pTerrainSectorTextureInfo->fTexOffsetY;
	}
	else
	{
		sZeroLine(sData);
	}

	sData[0].f[3] = gEnv->p3DEngine->GetTerrainTextureMultiplier();

	return &sData[0].f[0];
}
NO_INLINE float* sGetTerrainLayerGen(UFloat4* sData, CD3D9Renderer* r)
{
	if (!r->m_RP.m_pRE)
		return NULL;          // it seems the wrong material was assigned

	CRenderElement* pRE = r->m_RP.m_pRE;

	float* pData = (float*)pRE->m_CustomData;
	if (pData)
	{
		sData[0].f[0] = pData[0];
		sData[0].f[1] = pData[1];
		sData[0].f[2] = pData[2];
		sData[0].f[3] = pData[3];
		sData[1].f[0] = pData[4];
		sData[1].f[1] = pData[5];
		sData[1].f[2] = pData[6];
		sData[1].f[3] = pData[7];
		sData[2].f[0] = pData[8];
		sData[2].f[1] = pData[9];
		sData[2].f[2] = pData[10];
		sData[2].f[3] = pData[11];
		sData[3].f[0] = pData[12];
		sData[3].f[1] = pData[13];
		sData[3].f[2] = pData[14];
		sData[3].f[3] = pData[15];
		return &sData[0].f[0];
	}
	else
		return NULL;
}

float* sGetTexMatrix(UFloat4* sData, CD3D9Renderer* r, const SCGParam* ParamBind)
{
	static Matrix44A m;

	SHRenderTarget* pTarg = (SHRenderTarget*)(UINT_PTR)ParamBind->m_nID;
	//assert(pTarg);
	if (!pTarg)
		return NULL;
	SEnvTexture* pEnvTex = pTarg->GetEnv2D();
	//assert(pEnvTex && pEnvTex->m_pTex);
	if (!pEnvTex || !pEnvTex->m_pTex)
		return NULL;

	if ((pTarg->m_eUpdateType != eRTUpdate_WaterReflect) && r->m_RP.m_pCurObject->m_ObjFlags & FOB_TRANS_MASK)
		m = r->m_RP.m_pCurObject->m_II.m_Matrix * pEnvTex->m_Matrix;
	else
		m = pEnvTex->m_Matrix;
	m.Transpose();

	return m.GetData();
}
void sGetWind(UFloat4* sData, CD3D9Renderer* r)
{
	Vec4 pWind(0, 0, 0, 0);

	sData[0].f[0] = pWind.x;
	sData[0].f[1] = pWind.y;
	sData[0].f[2] = pWind.z;
	sData[0].f[3] = pWind.w;
}
NO_INLINE void sGetRotGridScreenOff(UFloat4* sData, CD3D9Renderer* r)
{
	int iTempX, iTempY, iWidth, iHeight;
	r->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
	sData[0].f[0] = 1.0f / (float)iWidth;
	sData[0].f[1] = 0.f;
	sData[0].f[2] = 0.f;
	sData[0].f[3] = 1.0f / (float)iHeight;
	//rotated grid
	Vec4 t75 = Vec4(0.75f * sData[0].f[0], 0.75f * sData[0].f[1], 0.75f * sData[0].f[2], 0.75f * sData[0].f[3]);
	Vec4 t25 = Vec4(0.25f * sData[0].f[0], 0.25f * sData[0].f[1], 0.25f * sData[0].f[2], 0.25f * sData[0].f[3]);
	Vec2 rotX = Vec2(t75[0] + t25[2], t75[1] + t25[3]);
	Vec2 rotY = Vec2(t75[2] - t25[0], t75[3] - t25[1]);
	sData[0].f[0] = rotX[0];
	sData[0].f[1] = rotX[1];
	sData[0].f[2] = rotY[0];
	sData[0].f[3] = rotY[1];
}

NO_INLINE float sGetMaterialLayersOpacity(UFloat4* sData, CD3D9Renderer* r)
{
	float fMaterialLayersOpacity = 1.0f;

	uint32 nResourcesNoDrawFlags = r->m_RP.m_pShaderResources->GetMtlLayerNoDrawFlags();
	if ((r->m_RP.m_pCurObject->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) && !(nResourcesNoDrawFlags & MTL_LAYER_CLOAK))
	{
		fMaterialLayersOpacity = ((float)((r->m_RP.m_pCurObject->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) >> 8) / 255.0f);
		fMaterialLayersOpacity = min(1.0f, 4.0f * max(1.0f - fMaterialLayersOpacity, 0.0f));
	}

	return fMaterialLayersOpacity;
}

NO_INLINE void sGetScreenSize(UFloat4* sData, CD3D9Renderer* r)
{
	int iTempX, iTempY, iWidth, iHeight;
	r->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
#if CRY_PLATFORM_WINDOWS
	float w = (float) (iWidth > 1 ? iWidth : 1);
	float h = (float) (iHeight > 1 ? iHeight : 1);
#else
	float w = (float) iWidth;
	float h = (float) iHeight;
#endif
	sData[0].f[0] = w;
	sData[0].f[1] = h;
	sData[0].f[2] = 0.5f / (w / r->m_RP.m_CurDownscaleFactor.x);
	sData[0].f[3] = 0.5f / (h / r->m_RP.m_CurDownscaleFactor.y);
}

NO_INLINE void sGetIrregKernel(UFloat4* sData, CD3D9Renderer* r)
{
	int nSamplesNum = 1;
	switch (r->m_RP.m_nShaderQuality)
	{
	case eSQ_Low:
		nSamplesNum = 4;
		break;
	case eSQ_Medium:
		nSamplesNum = 8;
		break;
	case eSQ_High:
		nSamplesNum = 16;
		break;
	case eSQ_VeryHigh:
		nSamplesNum = 16;
		break;
	default:
		assert(0);
	}

	CShadowUtils::GetIrregKernel((float(*)[4]) & sData[0], nSamplesNum);
}

NO_INLINE void sGetRegularKernel(UFloat4* sData, CD3D9Renderer* r)
{
	float fRadius = r->GetShadowJittering();
	float SHADOW_SIZE = 1024.f;

	const Vec4 regular_kernel[9] =
	{
		Vec4(-1, 1,  0, 0),
		Vec4(0,  1,  0, 0),
		Vec4(1,  1,  0, 0),
		Vec4(-1, 0,  0, 0),
		Vec4(0,  0,  0, 0),
		Vec4(1,  0,  0, 0),
		Vec4(-1, -1, 0, 0),
		Vec4(0,  -1, 0, 0),
		Vec4(1,  -1, 0, 0)
	};

	float fFilterRange = fRadius / SHADOW_SIZE;

	for (int32 nInd = 0; nInd < 9; nInd++)
	{
		if ((nInd % 2) == 0)
		{
			sData[nInd / 2].f[0] = regular_kernel[nInd].x * fFilterRange;
			sData[nInd / 2].f[1] = regular_kernel[nInd].y * fFilterRange;
		}
		else
		{
			sData[nInd / 2].f[2] = regular_kernel[nInd].x * fFilterRange;
			;
			sData[nInd / 2].f[3] = regular_kernel[nInd].y * fFilterRange;
			;
		}
	}
}

NO_INLINE void sGetObjMatrix(UFloat4* sData, CD3D9Renderer* r, const register float* pData)
{
	sData[0].Load(pData);
	sData[1].Load(pData + 4);
	sData[2].Load(pData + 8);
}

NO_INLINE void sGetBendInfo(UFloat4* sData, CD3D9Renderer* r, int* vals = NULL)
{
	const SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;
	Vec4 vCurBending(0, 0, 0, 0);
	*(alias_cast<Vec4*>(&sData[0])) = vCurBending;
}

NO_INLINE Vec4 sGetVolumetricFogParams(const CRenderCamera& rcam)
{
	Vec4 pFogParams;

	I3DEngine* pEng = gEnv->p3DEngine;
	assert(pEng);

	Vec3 globalDensityParams(0, 1, 1);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_GLOBAL_DENSITY, globalDensityParams);

	float globalDensity = globalDensityParams.x;
	if (!gRenDev->IsHDRModeEnabled())
		globalDensity *= globalDensityParams.y;

	Vec3 volFogHeightDensity(0, 1, 0);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_HEIGHT_DENSITY, volFogHeightDensity);
	volFogHeightDensity.y = clamp_tpl(volFogHeightDensity.y, 1e-5f, 1.0f);

	Vec3 volFogHeightDensity2(4000.0f, 0, 0);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_HEIGHT_DENSITY2, volFogHeightDensity2);
	volFogHeightDensity2.y = clamp_tpl(volFogHeightDensity2.y, 1e-5f, 1.0f);
	volFogHeightDensity2.x = volFogHeightDensity2.x < volFogHeightDensity.x + 1.0f ? volFogHeightDensity.x + 1.0f : volFogHeightDensity2.x;

	const float ha = volFogHeightDensity.x;
	const float hb = volFogHeightDensity2.x;

	const float da = volFogHeightDensity.y;
	const float db = volFogHeightDensity2.y;

	const float ga = logf(da);
	const float gb = logf(db);

	const float c = (gb - ga) / (hb - ha);
	const float o = ga - c * ha;

	const float viewerHeight = rcam.vOrigin.z;

	float baseHeight = ha;
	float co = clamp_tpl(ga, -50.0f, 50.0f);   // Avoiding FPEs at extreme ranges
	float heightDiffFromBase = c * (viewerHeight - baseHeight);
	if (heightDiffFromBase >= 0.0f)
	{
		baseHeight = viewerHeight;
		co = clamp_tpl(c * baseHeight + o, -50.0f, 50.0f); // Avoiding FPEs at extreme ranges
		heightDiffFromBase = 0.0f;                         // c * (viewerHeight - viewerHeight) = 0.0
	}

	globalDensity *= 0.01f;   // multiply by 1/100 to scale value editor value back to a reasonable range

	pFogParams.x = c;
	pFogParams.y = 1.44269502f * globalDensity * expf(co);   // log2(e) = 1.44269502
	pFogParams.z = heightDiffFromBase;
	pFogParams.w = expf(clamp_tpl(heightDiffFromBase, -80.0f, 80.0f));   // Avoiding FPEs at extreme ranges

	return pFogParams;
}

NO_INLINE Vec4 sGetVolumetricFogRampParams()
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 vfRampParams(0, 100.0f, 0);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_RAMP, vfRampParams);

	vfRampParams.x = vfRampParams.x < 0 ? 0 : vfRampParams.x;                                         // start
	vfRampParams.y = vfRampParams.y < vfRampParams.x + 0.1f ? vfRampParams.x + 0.1f : vfRampParams.y; // end
	vfRampParams.z = clamp_tpl(vfRampParams.z, 0.0f, 1.0f);                                           // influence

	float invRampDist = 1.0f / (vfRampParams.y - vfRampParams.x);
	return Vec4(invRampDist, -vfRampParams.x * invRampDist, vfRampParams.z, -vfRampParams.z + 1.0f);
}

NO_INLINE void sGetFogColorGradientConstants(Vec4& fogColGradColBase, Vec4& fogColGradColDelta)
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 colBase = pEng->GetFogColor();
	fogColGradColBase = Vec4(colBase, 0);

	Vec3 colTop(colBase);
	pEng->GetGlobalParameter(E3DPARAM_FOG_COLOR2, colTop);
	fogColGradColDelta = Vec4(colTop - colBase, 0);
}

NO_INLINE Vec4 sGetFogColorGradientParams()
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 volFogHeightDensity(0, 1, 0);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_HEIGHT_DENSITY, volFogHeightDensity);

	Vec3 volFogHeightDensity2(4000.0f, 0, 0);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_HEIGHT_DENSITY2, volFogHeightDensity2);
	volFogHeightDensity2.x = volFogHeightDensity2.x < volFogHeightDensity.x + 1.0f ? volFogHeightDensity.x + 1.0f : volFogHeightDensity2.x;

	Vec3 gradientCtrlParams(0, 0.75f, 0.5f);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_GRADIENT_CTRL, gradientCtrlParams);

	const float colorHeightOffset = clamp_tpl(gradientCtrlParams.x, -1.0f, 1.0f);
	const float radialSize = -expf((1.0f - clamp_tpl(gradientCtrlParams.y, 0.0f, 1.0f)) * 14.0f) * 1.44269502f;   // log2(e) = 1.44269502;
	const float radialLobe = 1.0f / clamp_tpl(gradientCtrlParams.z, 1.0f / 21.0f, 1.0f) - 1.0f;

	const float invDist = 1.0f / (volFogHeightDensity2.x - volFogHeightDensity.x);
	return Vec4(invDist, -volFogHeightDensity.x * invDist - colorHeightOffset, radialSize, radialLobe);
}

NO_INLINE Vec4 sGetFogColorGradientRadial(const CRenderCamera& rcam)
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 fogColorRadial(0, 0, 0);
	pEng->GetGlobalParameter(E3DPARAM_FOG_RADIAL_COLOR, fogColorRadial);

	const float invFarDist = 1.0f / rcam.fFar;

	return Vec4(fogColorRadial, invFarDist);
}

NO_INLINE Vec4 sGetVolumetricFogSunDir(const Vec3& sunDir)
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 globalDensityParams(0, 1, 1);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_GLOBAL_DENSITY, globalDensityParams);
	const float densityClamp = 1.0f - clamp_tpl(globalDensityParams.z, 0.0f, 1.0f);

	return Vec4(sunDir, densityClamp);
}

NO_INLINE Vec4 sGetVolumetricFogSamplingParams(const CRenderCamera& rc)
{
	Vec3 volFogCtrlParams(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);
	const float raymarchStart = rc.fNear;
	const float raymarchDistance = (volFogCtrlParams.x > raymarchStart) ? (volFogCtrlParams.x - raymarchStart) : 0.0001f;

	Vec4 params;
	params.x = raymarchStart;
	params.y = 1.0f / raymarchDistance;
	params.z = static_cast<f32>(CTexture::s_ptexVolumetricFog ? CTexture::s_ptexVolumetricFog->GetDepth() : 0.0f);
	params.w = params.z > 0.0f ? (1.0f / params.z) : 0.0f;
	return params;
}

NO_INLINE Vec4 sGetVolumetricFogDistributionParams(const CRenderCamera& rc, int32 frameID)
{
	Vec3 volFogCtrlParams(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);
	const float raymarchStart = rc.fNear;
	const float raymarchDistance = (volFogCtrlParams.x > raymarchStart) ? (volFogCtrlParams.x - raymarchStart) : 0.0001f;

	Vec4 params;
	params.x = raymarchStart;
	params.y = raymarchDistance;
	float d = static_cast<f32>(CTexture::s_ptexVolumetricFog ? CTexture::s_ptexVolumetricFog->GetDepth() : 0.0f);
	params.z = d > 1.0f ? (1.0f / (d - 1.0f)) : 0.0f;

	// frame count for jittering
	float frameCount = -static_cast<float>(frameID % 1024);
	frameCount = (CRenderer::CV_r_VolumetricFogReprojectionBlendFactor > 0.0f) ? frameCount : 0.0f;
	params.w = frameCount;

	return params;
}

NO_INLINE Vec4 sGetVolumetricFogScatteringParams(const CRenderCamera& rc)
{
	Vec3 volFogScatterParams(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_SCATTERING_PARAMS, volFogScatterParams);

	Vec3 anisotropy(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY, anisotropy);

	float k = anisotropy.z;
	bool bNegative = k < 0.0f ? true : false;
	k = (abs(k) > 0.99999f) ? (bNegative ? -0.99999f : 0.99999f) : k;

	Vec4 params;
	params.x = volFogScatterParams.x;
	params.y = (volFogScatterParams.y < 0.0001f) ? 0.0001f : volFogScatterParams.y;  // it ensures extinction is more than zero.
	params.z = k;
	params.w = 1.0f - k * k;
	return params;
}

NO_INLINE Vec4 sGetVolumetricFogScatteringBlendParams()
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 volFogCtrlParams(0.0f, 0.0f, 0.0f);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);

	Vec4 params;
	params.x = volFogCtrlParams.y;  // blend factor of two radial lobes
	params.y = volFogCtrlParams.z;  // blend mode of two radial lobes
	params.z = 0.0f;
	params.w = 0.0f;
	return params;
}

NO_INLINE Vec4 sGetVolumetricFogScatteringColor()
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 fogAlbedo(0.0f, 0.0f, 0.0f);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_COLOR1, fogAlbedo);
	Vec3 sunColor = pEng->GetSunColor();
	sunColor = sunColor.CompMul(fogAlbedo);

	if (CRenderer::CV_r_VolumetricFogSunLightCorrection == 1)
	{
		// reconstruct vanilla sun color because it's divided by pi in ConvertIlluminanceToLightColor().
		sunColor *= gf_PI;
	}

	Vec3 anisotropy(0.0f, 0.0f, 0.0f);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY2, anisotropy);

	float k = anisotropy.z;
	bool bNegative = k < 0.0f ? true : false;
	k = (abs(k) > 0.99999f) ? (bNegative ? -0.99999f : 0.99999f) : k;

	return Vec4(sunColor, k);
}

NO_INLINE Vec4 sGetVolumetricFogScatteringSecondaryColor()
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 fogAlbedo(0.0f, 0.0f, 0.0f);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_COLOR2, fogAlbedo);
	Vec3 sunColor = pEng->GetSunColor();
	sunColor = sunColor.CompMul(fogAlbedo);

	if (CRenderer::CV_r_VolumetricFogSunLightCorrection == 1)
	{
		// reconstruct vanilla sun color because it's divided by pi in ConvertIlluminanceToLightColor().
		sunColor *= gf_PI;
	}

	Vec3 anisotropy(0.0f, 0.0f, 0.0f);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY2, anisotropy);

	float k = anisotropy.z;
	bool bNegative = k < 0.0f ? true : false;
	k = (abs(k) > 0.99999f) ? (bNegative ? -0.99999f : 0.99999f) : k;

	return Vec4(sunColor, 1.0f - k * k);
}

NO_INLINE Vec4 sGetVolumetricFogHeightDensityParams(const CRenderCamera& rcam)
{
	Vec4 pFogParams;

	I3DEngine* pEng = gEnv->p3DEngine;
	assert(pEng);

	Vec3 globalDensityParams(0, 1, 1);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_GLOBAL_DENSITY, globalDensityParams);

	float globalDensity = globalDensityParams.x;
	const float clampTransmittance = globalDensityParams.y > 0.9999999f ? 1.0f : globalDensityParams.y;
	const float visibility = globalDensityParams.z;

	Vec3 volFogHeightDensity(0, 1, 0);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY, volFogHeightDensity);
	volFogHeightDensity.y = clamp_tpl(volFogHeightDensity.y, 1e-5f, 1.0f);

	Vec3 volFogHeightDensity2(4000.0f, 0, 0);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY2, volFogHeightDensity2);
	volFogHeightDensity2.y = clamp_tpl(volFogHeightDensity2.y, 1e-5f, 1.0f);
	volFogHeightDensity2.x = volFogHeightDensity2.x < volFogHeightDensity.x + 1.0f ? volFogHeightDensity.x + 1.0f : volFogHeightDensity2.x;

	const float ha = volFogHeightDensity.x;
	const float hb = volFogHeightDensity2.x;

	const float db = volFogHeightDensity2.y;
	const float da = abs(db - volFogHeightDensity.y) < 0.00001f ? volFogHeightDensity.y + 0.00001f : volFogHeightDensity.y;

	const float ga = logf(da);
	const float gb = logf(db);

	const float c = (gb - ga) / (hb - ha);
	const float o = ga - c * ha;

	const float viewerHeight = rcam.vOrigin.z;
	const float co = clamp_tpl(c * viewerHeight + o, -50.0f, 50.0f);   // Avoiding FPEs at extreme ranges

	globalDensity *= 0.01f;   // multiply by 1/100 to scale value editor value back to a reasonable range

	pFogParams.x = c;
	pFogParams.y = 1.44269502f * globalDensity * expf(co);   // log2(e) = 1.44269502
	pFogParams.z = visibility;
	pFogParams.w = 1.0f - clamp_tpl(clampTransmittance, 0.0f, 1.0f);

	return pFogParams;
}

NO_INLINE Vec4 sGetVolumetricFogHeightDensityRampParams()
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 vfRampParams(0, 100.0f, 0);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_RAMP, vfRampParams);

	vfRampParams.x = vfRampParams.x < 0 ? 0 : vfRampParams.x;                                         // start
	vfRampParams.y = vfRampParams.y < vfRampParams.x + 0.1f ? vfRampParams.x + 0.1f : vfRampParams.y; // end

	float t0 = 1.0f / (vfRampParams.y - vfRampParams.x);
	float t1 = vfRampParams.x * t0;

	return Vec4(vfRampParams.x, vfRampParams.y, t0, t1);
}

NO_INLINE Vec4 sGetVolumetricFogDistanceParams(const CRenderCamera& rc)
{
	float l, r, b, t, Ndist, Fdist;
	rc.GetFrustumParams(&l, &r, &b, &t, &Ndist, &Fdist);

	Vec3 volFogCtrlParams(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);
	const float raymarchStart = rc.fNear;
	const float raymarchEnd = (volFogCtrlParams.x > raymarchStart) ? volFogCtrlParams.x : raymarchStart + 0.0001f;

	float l2 = l * l;
	float t2 = t * t;
	float n2 = Ndist * Ndist;
	Vec4 params;
	params.x = raymarchEnd * (Ndist / sqrt(l2 + t2 + n2));
	params.y = raymarchEnd * (Ndist / sqrt(t2 + n2));
	params.z = raymarchEnd * (Ndist / sqrt(l2 + n2));
	params.w = raymarchEnd;

	return params;
}

void sGetMotionBlurData(UFloat4* sData, CD3D9Renderer* r, const CRenderObject::SInstanceInfo& instInfo, SRenderPipeline& rRP)
{
	CRenderObject* pObj = r->m_RP.m_pCurObject;

	Matrix44A mObjPrev;
	if ((rRP.m_FlagsPerFlush & RBSI_CUSTOM_PREVMATRIX) == 0)
	{
		CMotionBlur::GetPrevObjToWorldMat(pObj, mObjPrev);
	}
	else
	{
		mObjPrev = *rRP.m_pPrevMatrix;
	}

	float* pData = mObjPrev.GetData();
	sData[0].Load(pData);
	sData[1].Load(pData + 4);
	sData[2].Load(pData + 8);
}

inline void sPullVerticesInfo(UFloat4* sData, CD3D9Renderer* r)
{
	const SRenderPipeline& RESTRICT_REFERENCE renderPipeline = r->m_RP;
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;
	SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
	sData[0].ui[0] = renderPipeline.m_FirstVertex;
	sData[0].ui[1] = pOD->m_LightVolumeId - 1;
	sData[0].ui[2] = sData[0].ui[3] = 0;
}

void sGetCloakParams(UFloat4* sData, CD3D9Renderer* r, const CRenderObject::SInstanceInfo& instInfo)
{
	CRenderObject* pObj = r->m_RP.m_pCurObject;
	static int nLastFrameID = -1;
	static Matrix44A mCamObjCurr;
	static Vec3 pCamPrevPos = Vec3(0, 0, 0);

	if (nLastFrameID != gRenDev->GetFrameID(true))
	{
		// invert and get camera previous world space position
		Matrix44A CamPrevMatInv = CMotionBlur::GetPrevView();
		CamPrevMatInv.Invert();
		pCamPrevPos = CamPrevMatInv.GetRow(3);
		nLastFrameID = gRenDev->GetFrameID(true);
	}

	if (pObj)
	{
		Matrix44A mObjCurr = instInfo.m_Matrix.GetTransposed();
		Vec3 pObjPosWS = mObjCurr.GetRow(3);

		// Nearest objects are rendered in camera space, so add camera position to matrix for calculations
		if (pObj->m_ObjFlags & FOB_NEAREST)
		{
			pObjPosWS += gRenDev->GetRCamera().vOrigin;
			mObjCurr.SetRow(3, pObjPosWS);
		}

		// Get amount of light on cpu - dont want to add more shader permutations - this might be useful for other stuff - expose
		float fLightAmount = 0.0f;

		// Add ambient
		fLightAmount += (instInfo.m_AmbColor[0] + instInfo.m_AmbColor[1] + instInfo.m_AmbColor[2]) * 0.33f;

		// trying to match luminance bettwen hdr/non-hdr
		if (!CRenderer::CV_r_HDRRendering)
		{
			fLightAmount = 1.0f - expf(-fLightAmount);
			fLightAmount *= 2.5f;
		}
		else
			fLightAmount *= 0.25f;

		// Allow light override if this feature is needed indoors
		fLightAmount = max(CRenderer::CV_r_cloakMinLightValue, fLightAmount);

		// Get cloak blend amount from material layers
		float fCloakBlendAmount = ((pObj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) >> 8) / 255.0f;

		// Get instance speed
		float fMotionBlurAmount(0.3f);

		Matrix44A mObjPrev;
		SRenderObjData* pOD = pObj->GetObjData();
		assert(pOD);
		//if (pOD)
		//  mObjPrev.Transpose(pOD->m_prevMatrix);
		mObjPrev.SetIdentity();

		Matrix44A mCamObjPrev;
		float fSpeedScale = 1.0f;

		mCamObjCurr = mObjCurr;
		mCamObjPrev = mObjPrev;

		// temporary solution for GC demo
		float fExposureTime = 0.0005f;   //CRenderer::CV_r_MotionBlurShutterSpeed * fMotionBlurAmount;

		// renormalize frametime to account for time scaling
		float fCurrFrameTime = gEnv->pTimer->GetFrameTime();
		float fTimeScale = gEnv->pTimer->GetTimeScale();
		if (fTimeScale < 1.0f)
		{
			fTimeScale = max(0.0001f, fTimeScale);
			fCurrFrameTime /= fTimeScale;
		}

		float fAlpha = 0.0f;
		if (fCurrFrameTime != 0.0f)
			fAlpha = fExposureTime / fCurrFrameTime;

		mCamObjPrev = mCamObjCurr * (1.0f - fAlpha) + mCamObjPrev * fAlpha;

		Vec3 camObjCurrPos = mCamObjCurr.GetRow(3);
		Vec3 pVelocity = camObjCurrPos - mCamObjPrev.GetRow(3);
		float fCurrSpeed = max(pVelocity.GetLength() - 0.01f, 0.0f);

		float fCloakFalloff = 1.0f;

		// Apply fade by distance feature to cloak
		if (pOD && r->CV_r_cloakFadeByDist && (pOD->m_nCustomFlags & COB_FADE_CLOAK_BY_DISTANCE))
		{
			float zoomFactor = RAD2DEG(gRenDev->GetCamera().GetFov()) * (1.0f / 60.0f);
			Limit(zoomFactor, 0.0f, 1.0f);
			float objDistFromCamSq = (pObjPosWS - gRenDev->GetRCamera().vOrigin).GetLengthSquared() * zoomFactor;

			float minCloakScale = r->CV_r_cloakFadeMinValue;
			Limit(minCloakScale, 0.0f, 1.0f);
			if (objDistFromCamSq > r->CV_r_cloakFadeEndDistSq)
			{
				// Cloaked obj is further than fade end distance
				fCloakBlendAmount *= minCloakScale;
				fLightAmount *= minCloakScale;
			}
			else if (objDistFromCamSq > r->CV_r_cloakFadeStartDistSq)
			{
				// Cloak obj is between start and end fade params, so apply fade
				float fadeDistRange = r->CV_r_cloakFadeEndDistSq - r->CV_r_cloakFadeStartDistSq;
				if (fadeDistRange > 0.0f)  // Avoid division by 0
				{
					float fadeScale = ((objDistFromCamSq - r->CV_r_cloakFadeStartDistSq) / fadeDistRange);
					float cloakAlpha = LERP(1.0f, minCloakScale, fadeScale);
					fCloakBlendAmount *= cloakAlpha;
					fLightAmount *= cloakAlpha;
				}
				else
				{
					fCloakBlendAmount *= minCloakScale;
					fLightAmount *= minCloakScale;
				}
			}

			if (!(pOD->m_nCustomFlags & COB_IGNORE_CLOAK_REFRACTION_COLOR))
			{
				fLightAmount *= r->CV_r_cloakFadeLightScale;
			}

			if (r->CV_r_cloakRefractionFadeByDist)
			{
				float minCloakRefractionScale = r->CV_r_cloakRefractionFadeMinValue;
				// far away, use min value
				if (objDistFromCamSq > r->CV_r_cloakRefractionFadeEndDistSq)
				{
					fCloakFalloff = minCloakRefractionScale;
				}
				// in fade range
				else if (objDistFromCamSq > r->CV_r_cloakRefractionFadeStartDistSq)
				{
					float range = r->CV_r_cloakRefractionFadeEndDistSq - r->CV_r_cloakRefractionFadeStartDistSq;

					// ensure range is sane
					if (range > 0.0f)
					{
						float delta = ((objDistFromCamSq - r->CV_r_cloakRefractionFadeStartDistSq) / range);
						fCloakFalloff = LERP(1.0f, minCloakRefractionScale, delta);
					}
				}
				// really near by, we're already set to 1.0f so that's ok
				/*
				   else
				   {
				   }
				 */
			}
		}
		else
		{
			fLightAmount *= r->CV_r_cloakLightScale;
		}

		// Allow ability to control light scale during cloak transition
		if (fCloakBlendAmount < 1.0f)
		{
			fLightAmount *= r->CV_r_cloakTransitionLightScale;
		}

		const float cloakPaletteTexelOffset = 0.125f;
		float fCustomData = pOD ? static_cast<float>(pOD->m_nCustomData) : 0.0f;
		float paletteColorChannel = clamp_tpl<float>(((fCustomData + 1.0f) * 0.25f) - cloakPaletteTexelOffset, 0.0f, 1.0f);
		float cloakSparksAlpha = CRenderer::CV_r_cloakSparksAlpha;
		if (pOD)
		{
			if (pOD->m_nCustomFlags & COB_CLOAK_INTERFERENCE)
			{
				cloakSparksAlpha = CRenderer::CV_r_cloakInterferenceSparksAlpha;
			}
			if (pOD->m_nCustomFlags & COB_CLOAK_HIGHLIGHT)
			{
				const float highlightAlpha = pOD->m_fTempVars[5];
				fLightAmount = LERP(fLightAmount, max(fLightAmount, CRenderer::CV_r_cloakHighlightStrength), highlightAlpha);
			}
		}

		sData[0].f[0] = fCloakFalloff;
		sData[0].f[1] = paletteColorChannel;
		sData[0].f[2] = fLightAmount;

		CShaderResources* pRes = gRenDev->m_RP.m_pShaderResources;
		sData[0].f[3] = fCloakBlendAmount * ((pRes) ? pRes->CloakAmount() : 0);

		sData[1].f[0] = cloakSparksAlpha;
		memset(&sData[1].f[1], 0, sizeof(float) * 3);
	}
	else
	{
		memset(sData[0].f, 0, sizeof(float) * 4);
		memset(sData[1].f, 0, sizeof(float) * 4);
	}
}

NO_INLINE void sCausticsSmoothSunDirection(UFloat4* sData)
{
	SCGParamsPF& PF = gRenDev->m_cEF.m_PF[gRenDev->m_RP.m_nProcessThreadID];
	Vec3 v(0.0f, 0.0f, 0.0f);
	I3DEngine* pEng = gEnv->p3DEngine;

	// Caustics are done with projection from sun - ence they update too fast with regular
	// sun direction. Use a smooth sun direction update instead to workaround this
	if (PF.nCausticsFrameID != gRenDev->GetFrameID(false))
	{
		PF.nCausticsFrameID = gRenDev->GetFrameID(false);
		Vec3 pRealtimeSunDirNormalized = pEng->GetRealtimeSunDirNormalized();

		const float fSnapDot = 0.98f;
		float fDot = fabs(PF.vCausticsCurrSunDir.Dot(pRealtimeSunDirNormalized));
		if (fDot < fSnapDot)
			PF.vCausticsCurrSunDir = pRealtimeSunDirNormalized;

		PF.vCausticsCurrSunDir += (pRealtimeSunDirNormalized - PF.vCausticsCurrSunDir) * 0.005f * gEnv->pTimer->GetFrameTime();
		PF.vCausticsCurrSunDir.Normalize();
	}

	v = PF.vCausticsCurrSunDir;

	sData[0].f[0] = v.x;
	sData[0].f[1] = v.y;
	sData[0].f[2] = v.z;
	sData[0].f[3] = 0;
}

NO_INLINE void sAlphaTest(UFloat4* sData, const float dissolveRef)
{
	SRenderPipeline& RESTRICT_REFERENCE rRP = gRenDev->m_RP;

	sData[0].f[0] = dissolveRef * (1.0f / 255.0f);
	sData[0].f[1] = (rRP.m_pCurObject->m_ObjFlags & FOB_DISSOLVE_OUT) ? 1.0f : -1.0f;
	sData[0].f[2] = CRenderer::CV_r_ssdoAmountDirect;   // Use free component for SSDO
	sData[0].f[3] = rRP.m_pShaderResources ? rRP.m_pShaderResources->m_AlphaRef : 0;
}

NO_INLINE void sFurParams(UFloat4* sData)
{
	if (gRenDev->m_RP.m_pShaderResources)
	{
		sData[0].f[0] = gRenDev->m_RP.m_pShaderResources->FurAmount();
		sData[0].f[1] = sData[0].f[0];
		sData[0].f[2] = sData[0].f[0];
		sData[0].f[3] = sData[0].f[0];
	}
	else
		sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0.f;
}
NO_INLINE void sVisionParams(UFloat4* sData)
{
	CRenderObject* pObj = gRenDev->m_RP.m_pCurObject;
	SRenderObjData* pOD = pObj->GetObjData();
	if (pOD)
	{
		float fRecip = (1.0f / 255.0f);
		if (gRenDev->m_nThermalVisionMode && CRenderer::CV_r_cloakRenderInThermalVision && gRenDev->m_RP.m_pCurObject->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK)
			fRecip *= CRenderer::CV_r_cloakHeatScale;

		uint32 nParams(pOD->m_nHUDSilhouetteParams);
		if (gRenDev->m_RP.m_PersFlags2 & RBPF2_THERMAL_RENDERMODE_PASS)
		{
			nParams = pOD->m_nVisionParams;
			fRecip *= (float)pOD->m_nVisionScale;
		}

		sData[0].f[0] = float((nParams & 0xff000000) >> 24) * fRecip;
		sData[0].f[1] = float((nParams & 0x00ff0000) >> 16) * fRecip;
		sData[0].f[2] = float((nParams & 0x0000ff00) >> 8) * fRecip;
		sData[0].f[3] = float((nParams & 0x000000ff)) * fRecip;

		if (CRenderer::CV_r_customvisions == 2 && !gRenDev->IsCustomRenderModeEnabled(eRMF_MASK))
		{
			sData[0].f[3] = gEnv->pTimer->GetCurrTime() + ((float)(2 * pObj->m_Id) / 32768.0f);
		}
	}
	else
	{
		sData[0].f[0] = 0;
		sData[0].f[1] = 0;
		sData[0].f[2] = 0;
		sData[0].f[3] = 0;
	}
}

NO_INLINE void sFromObjSB(UFloat4* sData)
{
	CRenderObject* pObj = gRenDev->m_RP.m_pCurObject;
	if (pObj && (pObj->m_nTextureID > 0))
	{
		CTexture* pTex = CTexture::GetByID(pObj->m_nTextureID);

		// SB == Scale & Bias
		CRY_ALIGN(16) ColorF B = pTex->GetMinColor();
		CRY_ALIGN(16) ColorF S = pTex->GetMaxColor() - pTex->GetMinColor();

		sData[0] = (const UFloat4&)B;
		sData[1] = (const UFloat4&)S;
	}
	else
	{
		// SB == Scale & Bias
		CRY_ALIGN(16) ColorF B = ColorF(0.0f);
		CRY_ALIGN(16) ColorF S = ColorF(1.0f);

		sData[0] = (const UFloat4&)B;
		sData[1] = (const UFloat4&)S;
	}
}

NO_INLINE void sVisionMtlParams(UFloat4* sData)
{
	const Vec3& vCameraPos = gRenDev->GetRCamera().vOrigin;
	Vec3 vObjectPos = gRenDev->m_RP.m_pCurObject->GetTranslation();
	if (gRenDev->m_RP.m_pCurObject->m_ObjFlags & FOB_NEAREST)
	{
		vObjectPos += vCameraPos;   // Nearest objects are rendered in camera space, so convert to world space
	}
	const float fRecipThermalViewDist = 1.0f / (CRenderer::CV_r_ThermalVisionViewDistance * CRenderer::CV_r_ThermalVisionViewDistance + 1e-6f);

	CShaderResources* pRes = gRenDev->m_RP.m_pShaderResources;
	float heatAmount = 0.0f;
	if (gRenDev->m_nThermalVisionMode && pRes)
	{
		const int nThreadID = gRenDev->m_RP.m_nProcessThreadID;
		SRenderObjData* pOD = gRenDev->m_RP.m_pCurObject->GetObjData();
		if (!pOD || !(pOD->m_nCustomFlags & COB_IGNORE_HEAT_AMOUNT))
		{
			heatAmount = pRes->HeatAmount();

			if (gRenDev->m_RP.m_pCurObject->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK)
			{
				if (CRenderer::CV_r_cloakRenderInThermalVision)
				{
					heatAmount *= CRenderer::CV_r_cloakHeatScale;
				}
				else
				{
					heatAmount = 0.0f;
				}
			}
		}
	}
	sData[0].f[0] = heatAmount;
	sData[0].f[0] *= 1.0f - min(1.0f, vObjectPos.GetSquaredDistance(vCameraPos) * fRecipThermalViewDist);

	//Cloaked objects should flicker in thermal vision
	if (gRenDev->m_RP.m_nPassGroupID == EFSLIST_TRANSP && gRenDev->m_RP.m_pCurObject->m_ObjFlags & FOB_REQUIRES_RESOLVE)
	{
		static const void* pPrevNode = NULL;
		static int prevFrame = 0;
		static float fScale = 1.0f;

		if (CRenderer::CV_r_ThermalVisionViewCloakFlickerMinIntensity == CRenderer::CV_r_ThermalVisionViewCloakFlickerMaxIntensity)
		{
			//Simple scaling
			fScale = CRenderer::CV_r_ThermalVisionViewCloakFlickerMinIntensity;
		}
		else if ((pPrevNode != gRenDev->m_RP.m_pCurObject->m_pRenderNode) || (prevFrame != gRenDev->GetFrameID(true)))
		{
			UINT_PTR sineKey = (UINT_PTR)gRenDev->m_RP.m_pCurObject->m_pRenderNode + gRenDev->GetFrameID(true);
			fScale = (float)(
			  clamp_tpl(sin((double)(sineKey / (CRenderer::CV_r_ThermalVisionViewCloakFrequencyPrimary + 1e-6f))), 0.0, 1.0) *
			  clamp_tpl(sin((double)(sineKey / (CRenderer::CV_r_ThermalVisionViewCloakFrequencySecondary + 1e-6f))), 0.0, 1.0)
			  );
			fScale = CRenderer::CV_r_ThermalVisionViewCloakFlickerMinIntensity + ((CRenderer::CV_r_ThermalVisionViewCloakFlickerMaxIntensity - CRenderer::CV_r_ThermalVisionViewCloakFlickerMinIntensity) * fScale);
		}
		sData[0].f[0] *= fScale;

		//Cache parameters for use in the next call, to provide a consistent value for an individual character
		pPrevNode = gRenDev->m_RP.m_pCurObject->m_pRenderNode;
		prevFrame = gRenDev->GetFrameID(true);
	}

	sData[0].f[1] = sData[0].f[2] = 0.0f;
}

NO_INLINE void sEffectLayerParams(UFloat4* sData)
{
	CRenderObject* pObj = gRenDev->m_RP.m_pCurObject;
	if (!CRenderer::CV_r_DebugLayerEffect)
	{
		SRenderObjData* pOD = pObj->GetObjData();
		if (pOD)
		{
			float fRecip = (1.0f / 255.0f);
			sData[0].f[0] = float((pOD->m_pLayerEffectParams & 0xff000000) >> 24) * fRecip;
			sData[0].f[1] = float((pOD->m_pLayerEffectParams & 0x00ff0000) >> 16) * fRecip;
			sData[0].f[2] = float((pOD->m_pLayerEffectParams & 0x0000ff00) >> 8) * fRecip;
			sData[0].f[3] = float((pOD->m_pLayerEffectParams & 0x000000ff)) * fRecip;
		}
		else
			sZeroLine(sData);
	}
	else
	{

		const uint32 nDebugModesCount = 4;
		static const Vec4 pDebugModes[nDebugModesCount] =
		{
			Vec4(1, 0, 0, 0),
			Vec4(0, 1, 0, 0),
			Vec4(0, 0, 1, 0),
			Vec4(0, 0, 0, 1),
		};

		uint32 nCurrDebugMode = min(nDebugModesCount, (uint32)CRenderer::CV_r_DebugLayerEffect) - 1;

		sData[0].f[0] = pDebugModes[nCurrDebugMode].x;
		sData[0].f[1] = pDebugModes[nCurrDebugMode].y;
		sData[0].f[2] = pDebugModes[nCurrDebugMode].z;
		sData[0].f[3] = pDebugModes[nCurrDebugMode].w;
	}

}

NO_INLINE void sMaterialLayersParams(UFloat4* sData)
{
	CRenderObject* pObj = gRenDev->m_RP.m_pCurObject;
	sData[0].f[0] = ((pObj->m_nMaterialLayers & MTL_LAYER_BLEND_DYNAMICFROZEN)) / 255.0f;
	sData[0].f[1] = ((pObj->m_nMaterialLayers & MTL_LAYER_BLEND_WET) >> 16) / 254.0f; // account for missing low end bit
	// Apply attenuation
	sData[0].f[1] *= 1.0f - min(1.0f, pObj->m_fDistance / CRenderer::CV_r_rain_maxviewdist);

	sData[0].f[2] = (pObj->m_ObjFlags & FOB_DISSOLVE) ? (1.f / 255.f) * pObj->m_DissolveRef : ((pObj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) >> 8) / 255.0f;

	sData[0].f[3] = 0;  //(pObj->m_ObjFlags & FOB_MTLLAYERS_OBJSPACE)? 1.0f : 0.0f;
}

NO_INLINE void sLightningColSize(UFloat4* sData)
{
	Vec3 v;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_COLOR, v);
	sData[0].f[0] = v.x;
	sData[0].f[1] = v.y;
	sData[0].f[2] = v.z;

	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_SIZE, v);
	sData[0].f[3] = v.x * 0.01f;
}

NO_INLINE void sTexelsPerMeterInfo(UFloat4* sData)
{
	sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
	CShaderResources* pRes = gRenDev->m_RP.m_pShaderResources;
	if (pRes && pRes->m_Textures[EFTT_DIFFUSE])
	{
		CTexture* pTexture(pRes->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pTex);
		if (pTexture)
		{
			int texWidth(pTexture->GetWidth());
			int texHeight(pTexture->GetHeight());
			float ratio = 0.5f / CRenderer::CV_r_TexelsPerMeter;
			sData[0].f[0] = (float) texWidth * ratio;
			sData[0].f[1] = (float) texHeight * ratio;
		}
	}
}

inline void sAppendClipSpaceAdaptation(Matrix44A* __restrict pTransform)
{
#if CRY_RENDERER_OPENGL && OGL_MODIFY_PROJECTIONS
	#if OGL_FLIP_Y
	(*pTransform)(1, 0) = -(*pTransform)(1, 0);
	(*pTransform)(1, 1) = -(*pTransform)(1, 1);
	(*pTransform)(1, 2) = -(*pTransform)(1, 2);
	(*pTransform)(1, 3) = -(*pTransform)(1, 3);
	#endif //OGL_FLIP_Y
	(*pTransform)(2, 0) = +2.0f * (*pTransform)(2, 0) - (*pTransform)(3, 0);
	(*pTransform)(2, 1) = +2.0f * (*pTransform)(2, 1) - (*pTransform)(3, 1);
	(*pTransform)(2, 2) = +2.0f * (*pTransform)(2, 2) - (*pTransform)(3, 2);
	(*pTransform)(2, 3) = +2.0f * (*pTransform)(2, 3) - (*pTransform)(3, 3);
#endif //CRY_RENDERER_OPENGL && OGL_MODIFY_PROJECTIONS
}

NO_INLINE void sResInfo(UFloat4* sData, int texIdx)    // EFTT_DIFFUSE, EFTT_GLOSS, ... etc (but NOT EFTT_BUMP!)
{
	sIdentityLine(sData);
	CShaderResources* pRes = gRenDev->m_RP.m_pShaderResources;
	if (pRes && pRes->m_Textures[texIdx])
	{
		ITexture* pTexture(pRes->m_Textures[texIdx]->m_Sampler.m_pTex);
		if (pTexture)
		{
			int texWidth(pTexture->GetWidth());
			int texHeight(pTexture->GetHeight());
			sData[0].f[0] = (float) texWidth;
			sData[0].f[1] = (float) texHeight;
			if (texWidth && texHeight)
			{
				sData[0].f[2] = 1.0f / (float) texWidth;
				sData[0].f[3] = 1.0f / (float) texHeight;
			}
		}
	}
}

NO_INLINE void sTexelDensityParam(UFloat4* sData)
{
	sIdentityLine(sData);

	CShaderResources* pRes = gRenDev->m_RP.m_pShaderResources;

	if (pRes && pRes->m_Textures[EFTT_DIFFUSE])
	{
		CRenderChunk* pRenderChunk = NULL;
		int texWidth = 512;
		int texHeight = 512;
		int mipLevel = 0;

		CRenderElement* pRE = gRenDev->m_RP.m_pRE;

		if (pRE)
		{
			pRenderChunk = pRE->mfGetMatInfo();
		}

		CRenderObject* pCurObject = gRenDev->m_RP.m_pCurObject;

		if (pRenderChunk && pCurObject)
		{
			float weight = 1.0f;

			if (pRenderChunk->m_texelAreaDensity > 0.0f)
			{
				float scale = 1.0f;

				IRenderNode* pRenderNode = (IRenderNode*)pCurObject->m_pRenderNode;

				if (pRenderNode && pRenderNode != (void*)(intptr_t)-1 && pRenderNode->GetRenderNodeType() == eERType_Brush)
				{
					scale = ((IBrush*)pRenderNode)->GetMatrix().GetColumn0().GetLength();
				}

				float distance = pCurObject->m_fDistance * TANGENT30_2 / scale;
				int screenHeight = gRenDev->GetHeight();

				weight = pRenderChunk->m_texelAreaDensity * distance * distance * texWidth * texHeight * pRes->m_Textures[EFTT_DIFFUSE]->GetTiling(0) * pRes->m_Textures[EFTT_DIFFUSE]->GetTiling(1) / (screenHeight * screenHeight);
			}

			mipLevel = fastround_positive(0.5f * logf(max(weight, 1.0f)) / gf_ln2);
		}

		texWidth /= (1 << mipLevel);
		texHeight /= (1 << mipLevel);

		if (texWidth == 0)
			texWidth = 1;
		if (texHeight == 0)
			texHeight = 1;

		sData[0].f[0] = (float) texWidth;
		sData[0].f[1] = (float) texHeight;
		sData[0].f[2] = 1.0f / (float) texWidth;
		sData[0].f[3] = 1.0f / (float) texHeight;
	}
}

NO_INLINE void sTexelDensityColor(UFloat4* sData)
{
	sOneLine(sData);

	CShaderResources* pRes = gRenDev->m_RP.m_pShaderResources;

	if (pRes && pRes->m_Textures[EFTT_DIFFUSE])
	{
		if (CRenderer::CV_e_DebugTexelDensity == 2 || gcpRendD3D->CV_e_DebugTexelDensity == 4)
		{
			CRenderChunk* pRenderChunk = NULL;
			int texWidth = 512;
			int texHeight = 512;
			int mipLevel = 0;

			CRenderElement* pRE = gRenDev->m_RP.m_pRE;

			if (pRE)
			{
				pRenderChunk = pRE->mfGetMatInfo();
			}

			CRenderObject* pCurObject = gRenDev->m_RP.m_pCurObject;

			if (pRenderChunk && pCurObject)
			{
				float weight = 1.0f;

				if (pRenderChunk->m_texelAreaDensity > 0.0f)
				{
					float scale = 1.0f;

					IRenderNode* pRenderNode = (IRenderNode*)pCurObject->m_pRenderNode;

					if (pRenderNode && pRenderNode != (void*)(size_t)-1 && pRenderNode != (void*)-1 && pRenderNode->GetRenderNodeType() == eERType_Brush)
					{
						scale = ((IBrush*)pRenderNode)->GetMatrix().GetColumn0().GetLength();
					}

					float distance = pCurObject->m_fDistance * TANGENT30_2 / scale;
					int screenHeight = gRenDev->GetHeight();

					weight = pRenderChunk->m_texelAreaDensity * distance * distance * texWidth * texHeight * pRes->m_Textures[EFTT_DIFFUSE]->GetTiling(0) * pRes->m_Textures[EFTT_DIFFUSE]->GetTiling(1) / (screenHeight * screenHeight);
				}

				mipLevel = fastround_positive(0.5f * logf(max(weight, 1.0f)) / gf_ln2);
			}

			switch (mipLevel)
			{
			case 0:
				sData[0].f[0] = 1.0f;
				sData[0].f[1] = 1.0f;
				sData[0].f[2] = 1.0f;
				break;
			case 1:
				sData[0].f[0] = 0.0f;
				sData[0].f[1] = 0.0f;
				sData[0].f[2] = 1.0f;
				break;
			case 2:
				sData[0].f[0] = 0.0f;
				sData[0].f[1] = 1.0f;
				sData[0].f[2] = 0.0f;
				break;
			case 3:
				sData[0].f[0] = 0.0f;
				sData[0].f[1] = 1.0f;
				sData[0].f[2] = 1.0f;
				break;
			case 4:
				sData[0].f[0] = 1.0f;
				sData[0].f[1] = 0.0f;
				sData[0].f[2] = 0.0f;
				break;
			case 5:
				sData[0].f[0] = 1.0f;
				sData[0].f[1] = 0.0f;
				sData[0].f[2] = 1.0f;
				break;
			default:
				sData[0].f[0] = 1.0f;
				sData[0].f[1] = 1.0f;
				sData[0].f[2] = 0.0f;
				break;
			}
		}
		else
		{
			sData[0].f[0] = 1.0f;
			sData[0].f[1] = 1.0f;
			sData[0].f[2] = 1.0f;
		}
	}
}

NO_INLINE void sResInfoBump(UFloat4* sData)
{
	CShaderResources* pRes = gRenDev->m_RP.m_pShaderResources;
	if (pRes && pRes->m_Textures[EFTT_NORMALS])
	{
		ITexture* pTexture(pRes->m_Textures[EFTT_NORMALS]->m_Sampler.m_pTex);
		if (pTexture)
		{
			int texWidth(pTexture->GetWidth());
			int texHeight(pTexture->GetHeight());
			sData[0].f[0] = (float) texWidth;
			sData[0].f[1] = (float) texHeight;
			sData[0].f[2] = 1.0f / (float) max(1, texWidth);
			sData[0].f[3] = 1.0f / (float) max(1, texHeight);
		}
	}
}

NO_INLINE void sNumInstructions(UFloat4* sData)
{
	sData[0].f[0] = gRenDev->m_RP.m_NumShaderInstructions / CRenderer::CV_r_measureoverdrawscale / 256.0f;
}

NO_INLINE void sAmbient(UFloat4* sData, SRenderPipeline& rRP, const CRenderObject::SInstanceInfo& instInfo)
{
	sData[0].f[0] = instInfo.m_AmbColor[0];
	sData[0].f[1] = instInfo.m_AmbColor[1];
	sData[0].f[2] = instInfo.m_AmbColor[2];
	sData[0].f[3] = instInfo.m_AmbColor[3];

	if (CShaderResources* pRes = rRP.m_pShaderResources)
	{
		if (pRes->m_ResFlags & MTL_FLAG_ADDITIVE)
		{
			sData[0].f[0] *= rRP.m_fCurOpacity;
			sData[0].f[1] *= rRP.m_fCurOpacity;
			sData[0].f[2] *= rRP.m_fCurOpacity;
		}
	}
}
NO_INLINE void sAmbientOpacity(UFloat4* sData, const CRenderObject::SInstanceInfo& instInfo)
{
	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SCGParamsPF& RESTRICT_REFERENCE PF = r->m_cEF.m_PF[r->m_RP.m_nProcessThreadID];
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;
	CRenderObject* const __restrict pObj = rRP.m_pCurObject;

	float op = rRP.m_fCurOpacity;
	float a0 = instInfo.m_AmbColor[0];
	float a1 = instInfo.m_AmbColor[1];
	float a2 = instInfo.m_AmbColor[2];
	float a3 = instInfo.m_AmbColor[3];
	float opal = op * pObj->m_fAlpha;
	float s0 = a0;
	float s1 = a1;
	float s2 = a2;
	float s3 = opal;  // object opacity

	if (pObj->m_nMaterialLayers)
		s3 *= sGetMaterialLayersOpacity(sData, r);

	if (!(rRP.m_PersFlags2 & RBPF2_POST_3D_RENDERER_PASS))
	{
		if (!(pObj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK))
		{
			sData[0].f[0] = s0;
			sData[0].f[1] = s1;
			sData[0].f[2] = s2;
		}
		else
		{
			// Apply minimum ambient to cloaking objects - this avoids the cloak transition going black
			// if there's no lights on the object
			float minCloakAmbient = CRenderer::CV_r_cloakMinAmbientOutdoors;

			// Determine if object is affected by outdoor lighting
			if (rRP.m_ObjFlags & FOB_IN_DOORS)
			{
				// Object is indoors, so use indoors min ambient
				minCloakAmbient = CRenderer::CV_r_cloakMinAmbientIndoors;
			}

			sData[0].f[0] = max(minCloakAmbient, s0);
			sData[0].f[1] = max(minCloakAmbient, s1);
			sData[0].f[2] = max(minCloakAmbient, s2);
		}
		sData[0].f[3] = s3;
	}
	else
	{
		Vec4 ambient = PF.post3DRendererAmbient;
		sData[0].f[0] = ambient.x;
		sData[0].f[1] = ambient.y;
		sData[0].f[2] = ambient.z;
		sData[0].f[3] = ambient.w;
	}
}

NO_INLINE void sObjectAmbColComp(UFloat4* sData, const CRenderObject::SInstanceInfo& instInfo, const float fRenderQuality)
{
	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;
	CRenderObject* pObj = rRP.m_pCurObject;
	sData[0].f[0] = instInfo.m_AmbColor[3];
	sData[0].f[1] = /*instInfo.m_AmbColor[3] * */ rRP.m_fCurOpacity * pObj->m_fAlpha;

	if (pObj->m_nMaterialLayers)
		sData[0].f[1] *= sGetMaterialLayersOpacity(sData, r);

	sData[0].f[2] = 0.f;
	sData[0].f[3] = fRenderQuality * (1.0f / 65535.0f);
}

NO_INLINE void sTextureTileSize(UFloat4* sData, SRenderPipeline& rRP)
{
	SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
	if (pOD && pOD->m_pParticleShaderData)
	{
		memcpy(&sData[0], &pOD->m_pParticleShaderData->m_textureTileSize, sizeof(UFloat4));
	}
}

NO_INLINE void sWrinklesMask(UFloat4* sData, SRenderPipeline& rRP, uint index)
{
	static uint8 WrinkleMask[3] = { ECGP_PI_WrinklesMask0, ECGP_PI_WrinklesMask1, ECGP_PI_WrinklesMask2 };

	SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
	if (pOD)
	{
		if (pOD->m_pShaderParams)
		{
			if (SShaderParam::GetValue(WrinkleMask[index], const_cast<DynArray<SShaderParam>*>(pOD->m_pShaderParams), &sData[0].f[0], 4) == false)
			{
				sData[0].f[0] = 0.f;
				sData[0].f[1] = 0.f;
				sData[0].f[2] = 0.f;
				sData[0].f[3] = 0.f;
			}
		}
		else
		{
			sData[0].f[0] = 0.f;
			sData[0].f[1] = 0.f;
			sData[0].f[2] = 0.f;
			sData[0].f[3] = 0.f;
		}
	}
}

NO_INLINE void sParticleParams(UFloat4* sData, SRenderPipeline& rRP)
{
	SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
	if (pOD && pOD->m_pParticleShaderData)
	{
		memcpy(&sData[0], &pOD->m_pParticleShaderData->m_params, sizeof(UFloat4));
	}
}

NO_INLINE void sParticleLightParams(UFloat4* sData, SRenderPipeline& rRP)
{
	SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
	if (pOD && pOD->m_pParticleShaderData)
	{
		memcpy(&sData[0], &pOD->m_pParticleShaderData->m_lightParams, sizeof(UFloat4));
	}
}

NO_INLINE void sParticleSoftParams(UFloat4* sData, SRenderPipeline& rRP)
{
	SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
	if (pOD && pOD->m_pParticleShaderData)
	{
		memcpy(&sData[0], &pOD->m_pParticleShaderData->m_softness, sizeof(UFloat4));
	}
}

NO_INLINE void sParticleAlphaTest(UFloat4* sData, SRenderPipeline& rRP)
{
	SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
	if (pOD && pOD->m_pParticleShaderData)
	{
		memcpy(&sData[0], &pOD->m_pParticleShaderData->m_alphaTest, 2 * sizeof(UFloat4));
	}
}

NO_INLINE void sSunDirection(UFloat4* sData)
{
	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;
	Vec3 v(0, 0, 0);

	const SRenderLight* pLight = rRP.m_pSunLight;
	if (pLight)
	{
		v = pLight->GetPosition().normalized();
	}
	if ((rRP.m_TI[rRP.m_nProcessThreadID].m_PersFlags & RBPF_MAKESPRITE) && rRP.RenderView()->GetDynamicLightsCount())
	{
		v = rRP.RenderView()->GetDynamicLight(0).m_Origin;
		v.Normalize();
	}
	sData[0].f[0] = v.x;
	sData[0].f[1] = v.y;
	sData[0].f[2] = v.z;

	if (!(rRP.m_PersFlags2 & RBPF2_POST_3D_RENDERER_PASS))
	{
		sData[0].f[3] = r->m_fAdaptedSceneScale;
	}
	else
	{
		sData[0].f[3] = 1.0f;
	}
}

NO_INLINE void sAvgFogVolumeContrib(UFloat4* sData)
{
	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;

	if (!(rRP.m_PersFlags2 & RBPF2_POST_3D_RENDERER_PASS))
	{
		CRenderObject* pObj = rRP.m_pCurObject;
		SRenderObjData* pOD = pObj->GetObjData();
		if (!pOD || pOD->m_FogVolumeContribIdx == (uint16) - 1)
		{
			sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0.0f;
			sData[0].f[3] = 1.0f;
			return;
		}

		ColorF contrib;
		r->GetFogVolumeContribution(pOD->m_FogVolumeContribIdx, contrib);
		// Pre-multiply alpha (saves 1 instruction in pixel shader)
		sData[0].f[0] = contrib.r * (1 - contrib.a);
		sData[0].f[1] = contrib.g * (1 - contrib.a);
		sData[0].f[2] = contrib.b * (1 - contrib.a);
		sData[0].f[3] = contrib.a;
	}
	else
	{
		// RBPF2_POST_3D_RENDERER_PASS
		sData[0].f[0] = 0.0f;
		sData[0].f[1] = 0.0f;
		sData[0].f[2] = 0.0f;
		sData[0].f[3] = 1.0f;
	}
}

NO_INLINE void sNearFarDist(UFloat4* sData, CD3D9Renderer* r)
{
	const CRenderCamera& rc = r->GetRCamera();
	I3DEngine* pEng = gEnv->p3DEngine;
	sData[0].f[0] = rc.fNear;
	sData[0].f[1] = rc.fFar;
	// NOTE : v[2] is used to put the weapon's depth range into correct relation to the whole scene
	// when generating the depth texture in the z pass (_RT_NEAREST)
	sData[0].f[2] = rc.fFar / pEng->GetMaxViewDistance();
	sData[0].f[3] = 1.0f / rc.fFar;
}

NO_INLINE void sGetTempData(UFloat4* sData, CD3D9Renderer* r, const SCGParam* ParamBind)
{
	sData[0].f[0] = r->m_cEF.m_TempVecs[ParamBind->m_nID].x;
	sData[0].f[1] = r->m_cEF.m_TempVecs[ParamBind->m_nID].y;
	sData[0].f[2] = r->m_cEF.m_TempVecs[ParamBind->m_nID].z;
	sData[0].f[3] = r->m_cEF.m_TempVecs[ParamBind->m_nID].w;
}

NO_INLINE void sCameraFront(UFloat4* sData, CD3D9Renderer* r)
{
	Vec3 v = r->GetRCamera().vZ;
	v.Normalize();

	sData[0].f[0] = v.x;
	sData[0].f[1] = v.y;
	sData[0].f[2] = v.z;
	sData[0].f[3] = 0;
}
NO_INLINE void sCameraRight(UFloat4* sData, CD3D9Renderer* r)
{
	Vec3 v = r->GetRCamera().vX;
	v.Normalize();

	sData[0].f[0] = v.x;
	sData[0].f[1] = v.y;
	sData[0].f[2] = v.z;
	sData[0].f[3] = 0;
}
NO_INLINE void sCameraUp(UFloat4* sData, CD3D9Renderer* r)
{
	Vec3 v = r->GetRCamera().vY;
	v.Normalize();

	sData[0].f[0] = v.x;
	sData[0].f[1] = v.y;
	sData[0].f[2] = v.z;
	sData[0].f[3] = 0;
}

NO_INLINE void sRTRect(UFloat4* sData, CD3D9Renderer* r)
{
	sData[0].f[0] = r->m_cEF.m_RTRect.x;
	sData[0].f[1] = r->m_cEF.m_RTRect.y;
	sData[0].f[2] = r->m_cEF.m_RTRect.z;
	sData[0].f[3] = r->m_cEF.m_RTRect.w;
}

}

void CRenderer::UpdateConstParamsPF(const SRenderingPassInfo& passInfo, bool bSecondaryViewport)
{
	// Per frame - hardcoded/fast - update of commonly used data - feel free to improve this
	int nThreadID = m_RP.m_nFillThreadID;

	CRenderView* pRenderView = passInfo.GetRenderView();
	CRY_ASSERT(pRenderView);

	SCGParamsPF& PF = gRenDev->m_cEF.m_PF[nThreadID];
	uint32 nFrameID = gRenDev->m_RP.m_TI[nThreadID].m_nFrameUpdateID;
	if (PF.nFrameID == nFrameID || pRenderView->IsRecursive())
		return;

	PF.nFrameID = nFrameID;

	// Updating..

	I3DEngine* p3DEngine = gEnv->p3DEngine;
	if (p3DEngine == NULL)
		return;

	const CRenderCamera& rcam = pRenderView->GetRenderCamera(CCamera::eEye_Left);
	const int32 frameID = gcpRendD3D->GetFrameID(false);

	D3DViewPort viewport;
	{
		SViewport rendererViewport = gcpRendD3D->GetViewport();
		viewport.TopLeftX = static_cast<float>(rendererViewport.nX);
		viewport.TopLeftY = static_cast<float>(rendererViewport.nY);
		viewport.Width = static_cast<float>(rendererViewport.nWidth);
		viewport.Height = static_cast<float>(rendererViewport.nHeight);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
	}

	Vec3 sunColor = p3DEngine->GetSunColor();
	Vec3 sunDir = p3DEngine->GetSunDirNormalized();
	if (bSecondaryViewport)
	{
		// pick sun light.
		SRenderLight* pSunLight = nullptr;
		for (uint32 i = 0; i < pRenderView->GetDynamicLightsCount(); i++)
		{
			SRenderLight& light = pRenderView->GetDynamicLight(i);
			if (light.m_Flags & DLF_SUN)
			{
				pSunLight = &light;
				break;
			}
		}

		if (pSunLight)
		{
			ColorF& sunLightColor = pSunLight->m_Color;
			sunColor = Vec3(sunLightColor.r, sunLightColor.g, sunLightColor.b);
			sunDir = pSunLight->GetPosition().normalized();
		}
	}

	// ECGP_PF_SunColor
	PF.pSunColor = sunColor;
	PF.pSunDirection = sunDir;
	PF.sunSpecularMultiplier = p3DEngine->GetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER);
	// ECGP_PF_SkyColor
	PF.pSkyColor = p3DEngine->GetSkyColor();

	PF.pCameraPos = rcam.vOrigin;

	// ECGP_PB_WaterLevel - x = static level y = dynamic water ocean/volume level based on camera position, z: dynamic ocean water level
	PF.vWaterLevel = Vec3(p3DEngine->GetWaterLevel());

	// ECGP_PB_HDRDynamicMultiplier
	PF.fHDRDynamicMultiplier = HDRDynamicMultiplier;

	PF.pVolumetricFogParams = sGetVolumetricFogParams(rcam);
	PF.pVolumetricFogRampParams = sGetVolumetricFogRampParams();
	PF.pVolumetricFogSunDir = sGetVolumetricFogSunDir(sunDir);

	sGetFogColorGradientConstants(PF.pFogColGradColBase, PF.pFogColGradColDelta);
	PF.pFogColGradParams = sGetFogColorGradientParams();
	PF.pFogColGradRadial = sGetFogColorGradientRadial(rcam);

	// ECGP_PB_CausticsParams
	Vec4 vTmp = p3DEngine->GetCausticsParams();
	//PF.pCausticsParams = Vec3( vTmp.y, vTmp.z, vTmp.w );
	PF.pCausticsParams = Vec3(vTmp.x, vTmp.z, vTmp.y);

	// ECGP_PB_CloudShadingColorSun
	Vec3 cloudShadingSunColor;
	p3DEngine->GetGlobalParameter(E3DPARAM_CLOUDSHADING_SUNCOLOR, cloudShadingSunColor);
	PF.pCloudShadingColorSun = cloudShadingSunColor;
	// ECGP_PB_CloudShadingColorSky
	Vec3 cloudShadingSkyColor;
	p3DEngine->GetGlobalParameter(E3DPARAM_CLOUDSHADING_SKYCOLOR, cloudShadingSkyColor);
	PF.pCloudShadingColorSky = cloudShadingSkyColor;

	const int heightMapSize = p3DEngine->GetTerrainSize();
	Vec3 cloudShadowOffset = m_cloudShadowSpeed * gEnv->pTimer->GetCurrTime();
	cloudShadowOffset.x -= (int) cloudShadowOffset.x;
	cloudShadowOffset.y -= (int) cloudShadowOffset.y;

	if (CVolumetricCloudsStage::IsRenderable())
	{
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_TILING_OFFSET, PF.pVolCloudTilingOffset);
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_TILING_SIZE, PF.pVolCloudTilingSize);

		Vec3 cloudShadowTilingSize(0.0f, 0.0f, 16000.0f);
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_MISC_PARAM, cloudShadowTilingSize);

		Vec3 cloudGenParams;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_GEN_PARAMS, cloudGenParams);

		// to avoid floating point exception.
		cloudShadowTilingSize.z = max(1e-3f, cloudShadowTilingSize.z);
		PF.pVolCloudTilingSize.x = max(1e-3f, PF.pVolCloudTilingSize.x);
		PF.pVolCloudTilingSize.y = max(1e-3f, PF.pVolCloudTilingSize.y);
		PF.pVolCloudTilingSize.z = max(1e-3f, PF.pVolCloudTilingSize.z);

		// ECGP_PB_CloudShadowParams
		const float cloudAltitude = cloudGenParams.y;
		const float thickness = max(1e-3f, cloudGenParams.z); // to avoid floating point exception.
		const float cloudTopAltitude = cloudAltitude + thickness;
		const float invCloudShadowRamp = 1.0f / (thickness * 0.1f); // cloud shadow starts to decay from 10 percent distance from top of clouds.
		const float invCloudThickness = 1.0f / thickness;
		PF.pCloudShadowParams = Vec4(cloudAltitude, cloudTopAltitude, invCloudShadowRamp, invCloudThickness);

		// ECGP_PB_CloudShadowAnimParams
		const Vec2 cloudOffset(PF.pVolCloudTilingOffset.x, PF.pVolCloudTilingOffset.y);
		const Vec2 vTiling(cloudShadowTilingSize.z, cloudShadowTilingSize.z);
		PF.pCloudShadowAnimParams = CVolumetricCloudsStage::GetVolumetricCloudShadowParams(rcam, cloudOffset, vTiling);
	}
	else
	{
		// ECGP_PB_CloudShadowParams
		PF.pCloudShadowParams = Vec4(0, 0, m_cloudShadowInvert ? 1.0f : 0.0f, m_cloudShadowBrightness);

		// ECGP_PB_CloudShadowAnimParams
		PF.pCloudShadowAnimParams = Vec4(m_cloudShadowTiling / heightMapSize, -m_cloudShadowTiling / heightMapSize, cloudShadowOffset.x, -cloudShadowOffset.y);
	}

	// ECGP_PB_VolumetricFogSamplingParams
	PF.pVolumetricFogSamplingParams = sGetVolumetricFogSamplingParams(rcam);

	// ECGP_PB_VolumetricFogDistributionParams
	PF.pVolumetricFogDistributionParams = sGetVolumetricFogDistributionParams(rcam, frameID);

	// ECGP_PB_VolumetricFogScatteringParams
	PF.pVolumetricFogScatteringParams = sGetVolumetricFogScatteringParams(rcam);

	// ECGP_PB_VolumetricFogScatteringParams
	PF.pVolumetricFogScatteringBlendParams = sGetVolumetricFogScatteringBlendParams();

	// ECGP_PB_VolumetricFogScatteringColor
	PF.pVolumetricFogScatteringColor = sGetVolumetricFogScatteringColor();

	// ECGP_PB_VolumetricFogScatteringSecondaryColor
	PF.pVolumetricFogScatteringSecondaryColor = sGetVolumetricFogScatteringSecondaryColor();

	// ECGP_PB_VolumetricFogHeightDensityParams
	PF.pVolumetricFogHeightDensityParams = sGetVolumetricFogHeightDensityParams(rcam);

	// ECGP_PB_VolumetricFogHeightDensityRampParams
	PF.pVolumetricFogHeightDensityRampParams = sGetVolumetricFogHeightDensityRampParams();

	// ECGP_PB_VolumetricFogDistanceParams
	PF.pVolumetricFogDistanceParams = sGetVolumetricFogDistanceParams(rcam);

	// irregular filter kernel for shadow map sampling
	EShaderQuality shaderQuality = gRenDev->m_cEF.m_ShaderProfiles[eST_Shadow].GetShaderQuality();
	int32 sampleCountByQuality[eSQ_Max] =
	{
		4,  // eSQ_Low
		8,  // eSQ_Medium
		16, // eSQ_High
		16, // eSQ_VeryHigh
	};
	ZeroArray(PF.irregularFilterKernel);
	CShadowUtils::GetIrregKernel(PF.irregularFilterKernel, sampleCountByQuality[shaderQuality]);
}

void CHWShader_D3D::mfCommitParamsMaterial()
{
	DETAILED_PROFILE_MARKER("mfCommitParamsMaterial");
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CShaderResources* const __restrict pRes = rd->m_RP.m_pShaderResources;
	if (pRes)
	{
		bool bVSParams = s_pCurInstVS && s_pCurInstVS->m_bHasPMParams;
		if (s_pCurInstDS && s_pCurInstDS->m_bHasPMParams)
			bVSParams = true;
		else if (s_pCurInstHS && s_pCurInstHS->m_bHasPMParams)
			bVSParams = true;
		if (bVSParams)
		{
			{
				CConstantBuffer* const pCB = pRes->m_pCB;
				mfSetCB(eHWSC_Vertex, CB_PER_MATERIAL, pCB);
				if (CHWShader_D3D::s_pCurInstDS)
				{
					mfSetCB(eHWSC_Domain, CB_PER_MATERIAL, pCB);
					mfSetCB(eHWSC_Hull, CB_PER_MATERIAL, pCB);
				}
				if (CHWShader_D3D::s_pCurInstGS)
					mfSetCB(eHWSC_Geometry, CB_PER_MATERIAL, pCB);
				if (CHWShader_D3D::s_pCurInstCS)
					mfSetCB(eHWSC_Compute, CB_PER_MATERIAL, pCB);
			}
		}
		if (s_pCurInstPS && s_pCurInstPS->m_bHasPMParams)
		{
			mfSetCB(eHWSC_Pixel, CB_PER_MATERIAL, pRes->m_pCB);
		}
	}
}

void CHWShader_D3D::mfCommitParamsGlobal()
{
	DETAILED_PROFILE_MARKER("mfCommitParamsGlobal");
	PROFILE_FRAME(CommitGlobalShaderParams);

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;

	if (rRP.m_PersFlags2 & (RBPF2_COMMIT_PF | RBPF2_COMMIT_CM))
	{
		rRP.m_PersFlags2 &= ~(RBPF2_COMMIT_PF | RBPF2_COMMIT_CM);

		mfSetTextures(s_PF_Textures, eHWSC_Pixel);      // Per-frame textures
		mfSetSamplers_Old(s_PF_Samplers, eHWSC_Pixel);  // Per-frame samplers

		mfSetCM(); // Per-camera parameters
		mfSetPV(); // Per-view parameters
	}


	if (s_pCurInstCS)
	{
		mfSetCB(eHWSC_Compute, CB_PER_BATCH, s_pCurReqCB[eHWSC_Compute][CB_PER_BATCH]);
	}
}

void CHWShader_D3D::mfCommitParams()
{
	DETAILED_PROFILE_MARKER("mfCommitParams");
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	mfCommitCB(CB_PER_BATCH, eHWSC_Vertex, eHWSC_Vertex);
	mfCommitCB(CB_PER_INSTANCE, eHWSC_Vertex, eHWSC_Vertex);
	if (s_pCurInstDS)
	{
		mfCommitCB(CB_PER_BATCH, eHWSC_Domain, eHWSC_Domain);
		mfCommitCB(CB_PER_INSTANCE, eHWSC_Domain, eHWSC_Domain);

		mfCommitCB(CB_PER_BATCH, eHWSC_Hull, eHWSC_Hull);
		mfCommitCB(CB_PER_INSTANCE, eHWSC_Hull, eHWSC_Hull);
	}
	else
	{
		mfFreeCB(CB_PER_BATCH, eHWSC_Domain);
		mfFreeCB(CB_PER_INSTANCE, eHWSC_Domain);

		mfFreeCB(CB_PER_BATCH, eHWSC_Hull);
		mfFreeCB(CB_PER_INSTANCE, eHWSC_Hull);
	}

	if (s_pCurInstCS)
	{
		mfCommitCB(CB_PER_BATCH, eHWSC_Compute, eHWSC_Compute);
		mfCommitCB(CB_PER_INSTANCE, eHWSC_Compute, eHWSC_Compute);
	}
	else
	{
		mfFreeCB(CB_PER_BATCH, eHWSC_Compute);
		mfFreeCB(CB_PER_INSTANCE, eHWSC_Compute);
	}

	mfCommitCB(CB_SKIN_DATA, eHWSC_Vertex, eHWSC_Vertex);

	mfCommitCB(CB_PER_BATCH, eHWSC_Pixel, eHWSC_Pixel);
	mfCommitCB(CB_PER_INSTANCE, eHWSC_Pixel, eHWSC_Pixel);
	if (s_pCurInstGS)
	{
		mfCommitCB(CB_PER_BATCH, eHWSC_Geometry, eHWSC_Geometry);
		mfCommitCB(CB_PER_INSTANCE, eHWSC_Geometry, eHWSC_Geometry);
	}
	else
	{
		mfFreeCB(CB_PER_BATCH, eHWSC_Geometry);
		mfFreeCB(CB_PER_INSTANCE, eHWSC_Geometry);
	}
#ifdef _DEBUG
	for (int j = 0; j < eHWSC_Num; j++)
	{
		for (int i = 0; i < CB_NUM; i++)
		{
			//assert(!s_pDataCB[j][i]);
		}
	}
#endif
}

static char* sSH[] = { "VS", "PS", "GS", "CS", "DS", "HS" };
static char* sComp[] = { "x", "y", "z", "w" };

static int Reps = 100;

void Thing1(Matrix44& out1, Matrix44& out2, const Matrix44& a, const Matrix44& b)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER)
	for (int i = 0; i < Reps; ++i)
	{
		mathMatrixMultiply_Transp2(out1.GetData(), a.GetData(), b.GetData());
		TransposeAndStore((UFloat4*)&out2, out1);
	}
}
void Thing2(Matrix44& out1, Matrix44& out2, const Matrix44& a, const Matrix44& b)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER)
	for (int i = 0; i < Reps; ++i)
	{
		Matrix44 bt;
		bt.Transpose(b);
		out1 = bt * a;
		/*
			a00 b00 + a01 b10 + a02 b20 + a03 b30,  
			out2 = (at.x * b, at.y * b, at.z * b, at.w * b);

			   broadcast(a.xx) * b.x
	         + broadcast(a.yx) * b.y
	         + broadcast(a.zx) * b.z
	         + broadcast(a.wx) * b.w;

		*/

		out2.Transpose(out1);
	}
}

float* CHWShader_D3D::mfSetParametersPI(SCGParam* pParams, const int nINParams, float* pDst, EHWShaderClass eSH, int nMaxVecs)
{
	DETAILED_PROFILE_MARKER("mfSetParametersPI");
	//regarding snTuner this line causes 30% of the function's time and this function is using 12% of the whole frame time

	if (!pParams)
		return pDst;

	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SCGParamsPF& RESTRICT_REFERENCE PF = r->m_cEF.m_PF[r->m_RP.m_nProcessThreadID];
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;
	CRenderObject* const __restrict pObj = rRP.m_pCurObject;
	const int rLog = CRenderer::CV_r_log;

	if (!pObj)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Trying to set PI parameters with NULL object");
		return pDst;
	}

	// precache int to float conversions for some parameters
	float objDissolveRef = (float)(pObj->m_DissolveRef);
	float objRenderQuality = (float)(pObj->m_nRenderQuality);

	float* pSrc, * pData;
	const SCGParam* __restrict ParamBind = pParams;
	int nParams;
	Vec3 v;

	const CRenderObject::SInstanceInfo& rInstInfo = pObj->m_II;

	UFloat4* sData = sDataBuffer;

	for (int nParam = 0; nParam < nINParams; nParam++)
	{
		// Not activated yet for this shader
		if (ParamBind->m_dwCBufSlot < 0)
		{
			ParamBind++;
			continue;
		}

		pSrc = &sData[0].f[0];
		nParams = ParamBind->m_nParameters;
		//uchar* egPrm  = ((uchar*)(ParamBind) + offsetof(SCGParam,m_eCGParamType));
		//int nCurType = *((uchar*)ParamBind); //->m_eCGParamType;
		assert(ParamBind->m_Flags & PF_SINGLE_COMP);

#ifdef DO_RENDERLOG
		if (rLog >= 3)
		{
			int nCurType = ParamBind->m_eCGParamType & 0xff;
			r->Logv(" Set %s parameter '%s:%s' (%d vectors, reg: %d)\n", sSH[eSH], "Unknown" /*ParamBind->m_Name.c_str()*/, r->m_cEF.mfGetShaderParamName((ECGParam)nCurType), nParams, ParamBind->m_dwBind);
		}
#endif

		switch (ParamBind->m_eCGParamType)
		{
		case ECGP_Matr_SI_Obj:
			sGetObjMatrix(sData, r, pData = (float*)rInstInfo.m_Matrix.GetData());
			break;
		case ECGP_SI_AmbientOpacity:
			sAmbientOpacity(sData, rInstInfo);
			break;
		case ECGP_SI_BendInfo:
			sGetBendInfo(sData, r);
			break;
		case ECGP_SI_ObjectAmbColComp:
			sObjectAmbColComp(sData, rInstInfo, objRenderQuality);
			break;

		case ECGP_Matr_PI_Obj_T:
			{
				Store(sData, rInstInfo.m_Matrix);
			}
			break;
		case ECGP_Matr_PI_ViewProj:
			{
				if (!(rRP.m_ObjFlags & FOB_TRANS_MASK))
					TransposeAndStore(sData, r->m_CameraProjMatrix);
				else
				{
					mathMatrixMultiply_Transp2(&sData[4].f[0], r->m_CameraProjMatrix.GetData(), rInstInfo.m_Matrix.GetData());
					TransposeAndStore(sData, *alias_cast<Matrix44A*>(&sData[4]));
				}
				sAppendClipSpaceAdaptation(alias_cast<Matrix44A*>(&sData[0]));
				pSrc = &sData[0].f[0];
			}
			break;
		case ECGP_PI_AlphaTest:
			sAlphaTest(sData, objDissolveRef);
			break;
		case ECGP_PI_Ambient:
			sAmbient(sData, rRP, rInstInfo);
			break;
		case ECGP_PI_TextureTileSize:
			sTextureTileSize(sData, rRP);
			break;
		case ECGP_PI_ParticleParams:
			sParticleParams(sData, rRP);
			break;
		case ECGP_PI_ParticleLightParams:
			sParticleLightParams(sData, rRP);
			break;
		case ECGP_PI_ParticleSoftParams:
			sParticleSoftParams(sData, rRP);
			break;
		case ECGP_PI_ParticleAlphaTest:
			sParticleAlphaTest(sData, rRP);
			break;

		case ECGP_PI_WrinklesMask0:
			sWrinklesMask(sData, rRP, 0);
			break;
		case ECGP_PI_WrinklesMask1:
			sWrinklesMask(sData, rRP, 1);
			break;
		case ECGP_PI_WrinklesMask2:
			sWrinklesMask(sData, rRP, 2);
			break;

		case ECGP_PI_AvgFogVolumeContrib:
			sAvgFogVolumeContrib(sData);
			break;
		case ECGP_Matr_PI_Composite:
			{
				const Matrix44A r = *rRP.m_TI[rRP.m_nProcessThreadID].m_matView->GetTop() * (*rRP.m_TI[rRP.m_nProcessThreadID].m_matProj->GetTop());
				TransposeAndStore(sData, r);
			}
			break;
		case ECGP_PI_MotionBlurData:
			sGetMotionBlurData(sData, r, rInstInfo, rRP);
			break;
		case ECGP_PI_CloakParams:
			sGetCloakParams(sData, r, rInstInfo);
			break;
		case ECGP_Matr_PI_TexMatrix:
			pSrc = sGetTexMatrix(sData, r, ParamBind);
			break;
		case ECGP_Matr_PI_TCGMatrix:
			{
				SEfResTexture* pRT = rRP.m_ShaderTexResources[ParamBind->m_nID];
				if (pRT && pRT->m_Ext.m_pTexModifier)
				{
					Store(sData, pRT->m_Ext.m_pTexModifier->m_TexGenMatrix);
				}
				else
				{
					Store(sData, r->m_IdentityMatrix);
				}
			}
			break;
		case ECGP_PI_ObjColor:
			sData[0].f[0] = rInstInfo.m_AmbColor[0];
			sData[0].f[1] = rInstInfo.m_AmbColor[1];
			sData[0].f[2] = rInstInfo.m_AmbColor[2];
			sData[0].f[3] = rInstInfo.m_AmbColor[3] * rRP.m_pCurObject->m_fAlpha;
			break;
		case ECGP_PI_Wind:
			sGetWind(sData, r);
			break;
		case ECGP_PI_OSCameraPos:
			{
				Matrix44A* pMat1 = alias_cast<Matrix44A*>(&sData[4]);
				Matrix44A* pMat2 = alias_cast<Matrix44A*>(&sData[0]);
				*pMat1 = rInstInfo.m_Matrix.GetTransposed();
				*pMat2 = fabs(pMat1->Determinant()) > 1e-6 ? (*pMat1).GetInverted() : Matrix44(IDENTITY);

				// Respect Camera-Space rendering
				Vec3 cameraPos = (rRP.m_pCurObject->m_ObjFlags & FOB_NEAREST) ? ZERO : r->GetRCamera().vOrigin;
				TransformPosition(v, cameraPos, *pMat2);

				sData[0].f[0] = v.x;
				sData[0].f[1] = v.y;
				sData[0].f[2] = v.z;
				sData[0].f[3] = 1.f;
			}
			break;
		case ECGP_PI_VisionParams:
			sVisionParams(sData);
			break;
		case ECGP_PI_EffectLayerParams:
			sEffectLayerParams(sData);
			break;
		case ECGP_PI_MaterialLayersParams:
			sMaterialLayersParams(sData);
			break;
		case ECGP_PI_NumInstructions:
			sNumInstructions(sData);
			break;
		default:
			assert(0);
			break;
		}
		if (pSrc)
		{
			if (pDst)
			{
#if CRY_PLATFORM_SSE2
				const __m128* __restrict cpSrc = (const __m128*)pSrc;
				__m128* __restrict cpDst = (__m128*)pDst;
				const uint32 cParamCnt = nParams;
				for (uint32 i = 0; i < cParamCnt; i++)
				{
					cpDst[i] = cpSrc[i];
				}
#else
				const float* const __restrict cpSrc = pSrc;
				float* const __restrict cpDst = pDst;
				const uint32 cParamCnt = nParams;
				for (uint32 i = 0; i < cParamCnt * 4; i += 4)
				{
					cpDst[i] = cpSrc[i];
					cpDst[i + 1] = cpSrc[i + 1];
					cpDst[i + 2] = cpSrc[i + 2];
					cpDst[i + 3] = cpSrc[i + 3];
				}
#endif
				pDst += cParamCnt * 4;
			}
			else
			{
				// in Windows 32 pData must be 16 bytes aligned
				assert(!((uintptr_t)pSrc & 0xf) || sizeof(void*) != 4);
				if (!(ParamBind->m_Flags & PF_INTEGER))
					mfParameterfA(ParamBind, pSrc, nParams, eSH, nMaxVecs);
				else
					mfParameteri(ParamBind, pSrc, eSH, nMaxVecs);
			}
		}
		ParamBind++;
	}

	return pDst;
}

void CHWShader_D3D::mfSetGeneralParametersPI(SCGParam* pParams, const int nINParams, EHWShaderClass eSH, int nMaxVecs)
{
	DETAILED_PROFILE_MARKER("mfSetGeneralParametersPI");

	if (!pParams)
		return;

	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;

	if (!rRP.m_pCurObject)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Trying to set general PI parameters with NULL object");
		return;
	}

	float* pSrc, * pData;
	const SCGParam* __restrict ParamBind = pParams;
	int nParams;

	UFloat4* sData = sDataBuffer;

	for (int nParam = 0; nParam < nINParams; nParam++)
	{
		pSrc = &sData[0].f[0];
		nParams = ParamBind->m_nParameters;
		//uchar* egPrm  = ((uchar*)(ParamBind) + offsetof(SCGParam,m_eCGParamType));
		//int nCurType = *((uchar*)ParamBind); //->m_eCGParamType;
		assert(ParamBind->m_Flags & PF_SINGLE_COMP);

#ifdef DO_RENDERLOG
		if (CRenderer::CV_r_log >= 3)
		{
			int nCurType = ParamBind->m_eCGParamType & 0xff;
			r->Logv(" Set '%s' general parameter '%s:%s' (%d vectors, reg: %d)\n", sSH[eSH], "Unknown" /*ParamBind->m_Name.c_str()*/, r->m_cEF.mfGetShaderParamName((ECGParam)nCurType), nParams, ParamBind->m_dwBind);
		}
#endif

		switch (ParamBind->m_eCGParamType)
		{
		case ECGP_Matr_SI_Obj:
			sGetObjMatrix(sData, r, pData = rRP.m_pCurObject->m_II.m_Matrix.GetData());
			break;
		case ECGP_SI_AmbientOpacity:
			sAmbientOpacity(sData, rRP.m_pCurObject->m_II);
			break;
		case ECGP_SI_BendInfo:
			sGetBendInfo(sData, r);
			break;
		default:
			assert(0);
			break;
		}
		if (pSrc)
		{
			// in Windows 32 pData must be 16 bytes aligned
			assert(!((uintptr_t)pSrc & 0xf) || sizeof(void*) != 4);
			if (!(ParamBind->m_Flags & PF_INTEGER))
				mfParameterfA(ParamBind, pSrc, nParams, eSH, nMaxVecs);
			else
				mfParameteri(ParamBind, pSrc, eSH, nMaxVecs);
		}
		ParamBind++;
	}
}

void CHWShader_D3D::mfSetGeneralParameters(SCGParam* pParams, const int nINParams, EHWShaderClass eSH, int nMaxVecs)
{
	DETAILED_PROFILE_MARKER("mfSetGeneralParameters");

	const SCGParam* __restrict ParamBind = pParams;
	int nParams;
	float* pSrc;

	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;

	UFloat4* sData = sDataBuffer;

	for (int nParam = 0; nParam < nINParams; /*nParam++*/)
	{
		assert(ParamBind->m_Flags & PF_SINGLE_COMP);
		nParam++;

		pSrc = &sData[0].f[0];
		nParams = ParamBind->m_nParameters;

#ifdef DO_RENDERLOG
		if (CRenderer::CV_r_log >= 3)
			r->Logv(" Set '%s' general parameter '%s:%s' (%d vectors, reg: %d)\n", sSH[eSH], ParamBind->m_Name.c_str(), r->m_cEF.mfGetShaderParamName(ParamBind->m_eCGParamType), nParams, ParamBind->m_dwBind);
#endif

		switch (ParamBind->m_eCGParamType)
		{
		case ECGP_PM_Tweakable:
		case ECGP_PM_MatChannelSB:
		case ECGP_PM_MatDiffuseColor:
		case ECGP_PM_MatSpecularColor:
		case ECGP_PM_MatEmissiveColor:
		case ECGP_PM_MatMatrixTCM:
		case ECGP_PM_MatDeformWave:
		case ECGP_PM_MatDetailTilingAndAlphaRef:
			// Per Material should be set up in a constant buffer now
			assert(0);
			break;

		default:
			assert(0);
			break;
			//Warning("Unknown Parameter '%s' of type %d", ParamBind->m_Name.c_str(), ParamBind->m_eCGParamType);
			//assert(0);
			//return NULL;
		}
		if (pSrc)
		{
			// in Windows 32 pData must be 16 bytes aligned
			assert(!((uintptr_t)pSrc & 0xf) || sizeof(void*) != 4);
			if (!(ParamBind->m_Flags & PF_INTEGER))
				mfParameterfA(ParamBind, pSrc, nParams, eSH, nMaxVecs);
			else
				mfParameteri(ParamBind, pSrc, eSH, nMaxVecs);
		}
		++ParamBind;
	}
}

void CHWShader_D3D::mfSetParameters(SCGParam* pParams, const int nINParams, EHWShaderClass eSH, int nMaxVecs, Vec4* pOutBuffer, uint32 outBufferSize)
{
	DETAILED_PROFILE_MARKER("mfSetParameters");
	PROFILE_FRAME(Shader_SetParams);
	CRenderElement* pRE;
	float* pSrc, * pData;
	Vec3 v;
	Vec4 v4;
	const SCGParam* ParamBind = pParams;
	int nParams;
	const int rLog = CRenderer::CV_r_log;

	if (!pParams)
		return;

	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SCGParamsPF& PF = r->m_cEF.m_PF[r->m_RP.m_nProcessThreadID];
	SRenderPipeline& RESTRICT_REFERENCE rRP = r->m_RP;

	const Vec3& vCamPos = r->GetRCamera().vOrigin;

	UFloat4* sData = sDataBuffer; // data length must not exceed the lenght of sDataBuffer [=sozeof(36*float4)]

	for (int nParam = 0; nParam < nINParams; /*nParam++*/)
	{
		// saving HUGE LHS, x360 compiler generating quite bad code when nParam incremented inside for loop
		// note: nParam only used for previous line
		nParam++;

		pSrc = &sData[0].f[0];
		nParams = ParamBind->m_nParameters;
		//uchar* egPrm  = ((uchar*)(ParamBind) + offsetof(SCGParam,m_eCGParamType));
		//int nCurType = *((uchar*)ParamBind); //->m_eCGParamType;

		uint32 paramType = (uint32)ParamBind->m_eCGParamType;

		for (int nComp = 0; nComp < 4; nComp++)
		{
#ifdef DO_RENDERLOG
			if (rLog >= 3)
			{
				int nCurType = (ParamBind->m_eCGParamType >> (nComp << 3)) & 0xff;
				if (ParamBind->m_Flags & PF_SINGLE_COMP)
					r->Logv(" Set %s parameter '%s:%s' (%d vectors, reg: %d)\n", sSH[eSH], ParamBind->m_Name.c_str(), r->m_cEF.mfGetShaderParamName((ECGParam)nCurType), nParams, ParamBind->m_dwBind);
				else
					r->Logv(" Set %s parameter '%s:%s' (%d vectors, reg: %d.%s)\n", sSH[eSH], ParamBind->m_Name.c_str(), r->m_cEF.mfGetShaderParamName((ECGParam)nCurType), nParams, ParamBind->m_dwBind, sComp[nComp]);
			}
#endif

			switch (paramType & 0xff)
			{
				case ECGP_Matr_PB_ViewProjMatrixPrev:
				{
					TransposeAndStore(sData, r->m_CameraProjMatrixPrev);
					break;
				}
			case ECGP_Matr_PB_TerrainBase:
				pSrc = sGetTerrainBase(sData, r);
				break;
			case ECGP_Matr_PB_TerrainLayerGen:
				pSrc = sGetTerrainLayerGen(sData, r);
				break;
			case ECGP_Matr_PB_Temp4_0:
			case ECGP_Matr_PB_Temp4_1:
			case ECGP_Matr_PB_Temp4_2:
			case ECGP_Matr_PB_Temp4_3:
				pSrc = r->m_TempMatrices[ParamBind->m_eCGParamType - ECGP_Matr_PB_Temp4_0][ParamBind->m_nID].GetData();
				break;
			case ECGP_PB_AmbientOpacity:
				sAmbientOpacity(sData, rRP.m_pCurObject->m_II);
				break;
			case ECGP_PB_FromRE:
				pRE = rRP.m_pRE;
				if (!pRE || !(pData = (float*)pRE->m_CustomData))
					sData[0].f[nComp] = 0;
				else
					sData[0].f[nComp] = pData[(ParamBind->m_nID >> (nComp * 8)) & 0xff];
				break;
			case ECGP_PB_FromObjSB:
				sFromObjSB(sData);
				break;
			case ECGP_PM_Tweakable:
			case ECGP_PM_MatChannelSB:
			case ECGP_PM_MatDiffuseColor:
			case ECGP_PM_MatSpecularColor:
			case ECGP_PM_MatEmissiveColor:
			case ECGP_PM_MatMatrixTCM:
			case ECGP_PM_MatDeformWave:
			case ECGP_PM_MatDetailTilingAndAlphaRef:
				// Per Material should be set up in a constant buffer now
				assert(0);
				break;

			case ECGP_PB_GlobalShaderFlag:
				assert(ParamBind->m_pData);
				if (ParamBind->m_pData)
				{
					if (!rRP.m_pShader)
						pData = NULL;
					else
					{
						bool bVal = (rRP.m_pShader->m_nMaskGenFX & ParamBind->m_pData->d.nData64[nComp]) != 0;
						sData[0].f[nComp] = (float)(bVal);
					}
				}
				break;
			case ECGP_PB_TempData:
				sGetTempData(sData, r, ParamBind);
				break;
			case ECGP_PB_VolumetricFogParams:
				sData[0].f[0] = PF.pVolumetricFogParams.x;
				sData[0].f[1] = PF.pVolumetricFogParams.y;
				sData[0].f[2] = PF.pVolumetricFogParams.z;
				sData[0].f[3] = PF.pVolumetricFogParams.w;
				break;
			case ECGP_PB_VolumetricFogRampParams:
				sData[0].f[0] = PF.pVolumetricFogRampParams.x;
				sData[0].f[1] = PF.pVolumetricFogRampParams.y;
				sData[0].f[2] = PF.pVolumetricFogRampParams.z;
				sData[0].f[3] = PF.pVolumetricFogRampParams.w;
				break;
			case ECGP_PB_FogColGradColBase:
				sData[0].f[0] = PF.pFogColGradColBase.x;
				sData[0].f[1] = PF.pFogColGradColBase.y;
				sData[0].f[2] = PF.pFogColGradColBase.z;
				sData[0].f[3] = 0.0f;
				break;
			case ECGP_PB_FogColGradColDelta:
				sData[0].f[0] = PF.pFogColGradColDelta.x;
				sData[0].f[1] = PF.pFogColGradColDelta.y;
				sData[0].f[2] = PF.pFogColGradColDelta.z;
				sData[0].f[3] = 0.0f;
				break;
			case ECGP_PB_FogColGradParams:
				sData[0].f[0] = PF.pFogColGradParams.x;
				sData[0].f[1] = PF.pFogColGradParams.y;
				sData[0].f[2] = PF.pFogColGradParams.z;
				sData[0].f[3] = PF.pFogColGradParams.w;
				break;
			case ECGP_PB_FogColGradRadial:
				sData[0].f[0] = PF.pFogColGradRadial.x;
				sData[0].f[1] = PF.pFogColGradRadial.y;
				sData[0].f[2] = PF.pFogColGradRadial.z;
				sData[0].f[3] = PF.pFogColGradRadial.w;
				break;
			case ECGP_PB_VolumetricFogSunDir:
				sData[0].f[0] = PF.pVolumetricFogSunDir.x;
				sData[0].f[1] = PF.pVolumetricFogSunDir.y;
				sData[0].f[2] = PF.pVolumetricFogSunDir.z;
				sData[0].f[3] = PF.pVolumetricFogSunDir.w;
				break;
			case ECGP_PB_RuntimeShaderFlag:
				assert(ParamBind->m_pData);
				if (ParamBind->m_pData)
				{
					bool bVal = (rRP.m_FlagsShader_RT & ParamBind->m_pData->d.nData64[nComp]) != 0;
					sData[0].f[nComp] = (float)(bVal);
				}
				break;

			case ECGP_PB_VolumetricFogSamplingParams:
				sData[0].f[0] = PF.pVolumetricFogSamplingParams.x;
				sData[0].f[1] = PF.pVolumetricFogSamplingParams.y;
				sData[0].f[2] = PF.pVolumetricFogSamplingParams.z;
				sData[0].f[3] = PF.pVolumetricFogSamplingParams.w;
				break;

			case ECGP_PB_VolumetricFogDistributionParams:
				sData[0].f[0] = PF.pVolumetricFogDistributionParams.x;
				sData[0].f[1] = PF.pVolumetricFogDistributionParams.y;
				sData[0].f[2] = PF.pVolumetricFogDistributionParams.z;
				sData[0].f[3] = PF.pVolumetricFogDistributionParams.w;
				break;

			case ECGP_PB_VolumetricFogScatteringParams:
				sData[0].f[0] = PF.pVolumetricFogScatteringParams.x;
				sData[0].f[1] = PF.pVolumetricFogScatteringParams.y;
				sData[0].f[2] = PF.pVolumetricFogScatteringParams.z;
				sData[0].f[3] = PF.pVolumetricFogScatteringParams.w;
				break;

			case ECGP_PB_VolumetricFogScatteringBlendParams:
				sData[0].f[0] = PF.pVolumetricFogScatteringBlendParams.x;
				sData[0].f[1] = PF.pVolumetricFogScatteringBlendParams.y;
				sData[0].f[2] = PF.pVolumetricFogScatteringBlendParams.z;
				sData[0].f[3] = PF.pVolumetricFogScatteringBlendParams.w;
				break;

			case ECGP_PB_VolumetricFogScatteringColor:
				sData[0].f[0] = PF.pVolumetricFogScatteringColor.x;
				sData[0].f[1] = PF.pVolumetricFogScatteringColor.y;
				sData[0].f[2] = PF.pVolumetricFogScatteringColor.z;
				sData[0].f[3] = PF.pVolumetricFogScatteringColor.w;
				break;

			case ECGP_PB_VolumetricFogScatteringSecondaryColor:
				sData[0].f[0] = PF.pVolumetricFogScatteringSecondaryColor.x;
				sData[0].f[1] = PF.pVolumetricFogScatteringSecondaryColor.y;
				sData[0].f[2] = PF.pVolumetricFogScatteringSecondaryColor.z;
				sData[0].f[3] = PF.pVolumetricFogScatteringSecondaryColor.w;
				break;

			case ECGP_PB_VolumetricFogHeightDensityParams:
				sData[0].f[0] = PF.pVolumetricFogHeightDensityParams.x;
				sData[0].f[1] = PF.pVolumetricFogHeightDensityParams.y;
				sData[0].f[2] = PF.pVolumetricFogHeightDensityParams.z;
				sData[0].f[3] = PF.pVolumetricFogHeightDensityParams.w;
				break;

			case ECGP_PB_VolumetricFogHeightDensityRampParams:
				sData[0].f[0] = PF.pVolumetricFogHeightDensityRampParams.x;
				sData[0].f[1] = PF.pVolumetricFogHeightDensityRampParams.y;
				sData[0].f[2] = PF.pVolumetricFogHeightDensityRampParams.z;
				sData[0].f[3] = PF.pVolumetricFogHeightDensityRampParams.w;
				break;

			case ECGP_PB_VolumetricFogDistanceParams:
				sData[0].f[0] = PF.pVolumetricFogDistanceParams.x;
				sData[0].f[1] = PF.pVolumetricFogDistanceParams.y;
				sData[0].f[2] = PF.pVolumetricFogDistanceParams.z;
				sData[0].f[3] = PF.pVolumetricFogDistanceParams.w;
				break;

			case ECGP_PB_VolumetricFogGlobalEnvProbe0:
				{
					Vec4 param(0.0f);
					if ((r->m_nGraphicsPipeline) > 0)
					{
						auto pStage = r->GetGraphicsPipeline().GetVolumetricFogStage();
						CRY_ASSERT(pStage);
						param = pStage->GetGlobalEnvProbeShaderParam0();
					}
					sData[0].f[0] = param.x;
					sData[0].f[1] = param.y;
					sData[0].f[2] = param.z;
					sData[0].f[3] = param.w;
				}
				break;

			case ECGP_PB_VolumetricFogGlobalEnvProbe1:
				{
					Vec4 param(0.0f);
					if ((r->m_nGraphicsPipeline) > 0)
					{
						auto pStage = r->GetGraphicsPipeline().GetVolumetricFogStage();
						CRY_ASSERT(pStage);
						param = pStage->GetGlobalEnvProbeShaderParam1();
					}
					sData[0].f[0] = param.x;
					sData[0].f[1] = param.y;
					sData[0].f[2] = param.z;
					sData[0].f[3] = param.w;
				}
				break;

			case ECGP_Matr_PB_ProjMatrix:
				{
					TransposeAndStore(sData, r->m_ProjMatrix);
					break;
				}
			case ECGP_Matr_PB_UnProjMatrix:
				{
					Matrix44A* pMat = alias_cast<Matrix44A*>(&sData[0]);
					*pMat = *rRP.m_TI[rRP.m_nProcessThreadID].m_matView->GetTop() * (*rRP.m_TI[rRP.m_nProcessThreadID].m_matProj->GetTop());
					*pMat = pMat->GetInverted();
					*pMat = pMat->GetTransposed();
					break;
				}
			case ECGP_Matr_PB_View_IT:
				{
					Matrix44A* pMat = alias_cast<Matrix44A*>(&sData[0]);
					*pMat = rRP.m_pCurObject->m_II.m_Matrix.GetTransposed() * r->m_CameraMatrix;
					*pMat = (*pMat).GetInverted();
					break;
				}
			case ECGP_Matr_PB_View:
				{
					TransposeAndStore(sData, rRP.m_pCurObject->m_II.m_Matrix.GetTransposed() * r->m_CameraMatrix);
					break;
				}
			case ECGP_Matr_PB_View_I:
				{
					Matrix44A* pMat = alias_cast<Matrix44A*>(&sData[0]);
					*pMat = rRP.m_pCurObject->m_II.m_Matrix.GetTransposed() * r->m_CameraMatrix;
					*pMat = pMat->GetInverted();
					*pMat = pMat->GetTransposed();
					break;
				}
			case ECGP_Matr_PB_View_T:
				{
					Matrix44A* pMat = alias_cast<Matrix44A*>(&sData[0]);
					*pMat = rRP.m_pCurObject->m_II.m_Matrix.GetTransposed() * r->m_CameraMatrix;
					break;
				}
			case ECGP_Matr_PB_Camera:
				{
					TransposeAndStore(sData, r->m_CameraMatrix);
					break;
				}
			case ECGP_Matr_PB_Camera_T:
				pSrc = r->m_CameraMatrix.GetData();
				break;
			case ECGP_Matr_PB_Camera_I:
				{
					TransposeAndStore(sData, r->m_CameraMatrix.GetInverted());
					break;
				}
			case ECGP_Matr_PB_Camera_IT:
				{
					Matrix44A* pMat = alias_cast<Matrix44A*>(&sData[0]);
					*pMat = r->m_CameraMatrix.GetInverted();
					break;
				}

			case ECGP_PB_PullVerticesInfo:
				sPullVerticesInfo(sData, r);
				break;

			case ECGP_PB_LightningPos:
				gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_POS, v);
				sData[0].f[0] = v.x;
				sData[0].f[1] = v.y;
				sData[0].f[2] = v.z;
				sData[0].f[3] = 0.0f;
				break;
			case ECGP_PB_LightningColSize:
				sLightningColSize(sData);
				break;
			case ECGP_PB_WaterLevel:
				sData[0].f[0] = PF.vWaterLevel.x;
				sData[0].f[1] = PF.vWaterLevel.y;
				sData[0].f[2] = PF.vWaterLevel.z;
				sData[0].f[3] = 1.0f;
				break;
			case ECGP_PB_HDRDynamicMultiplier:
				sData[0].f[nComp] = PF.fHDRDynamicMultiplier;
				break;
			case ECGP_PB_ObjVal:
				{
					SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
					if (pOD)
					{
						if (pOD->m_pSkinningData)
						{
							// Backward compatibility old pipeline
							pOD->m_fTempVars[0] = pOD->m_pSkinningData->vecPrecisionOffset[0];
							pOD->m_fTempVars[1] = pOD->m_pSkinningData->vecPrecisionOffset[1];
							pOD->m_fTempVars[2] = pOD->m_pSkinningData->vecPrecisionOffset[2];
						}
						pData = (float*)pOD->m_fTempVars;
						sData[0].f[nComp] = pData[(ParamBind->m_nID >> (nComp * 8)) & 0xff];
					}
				}
				break;
			case ECGP_PB_BlendTerrainColInfo:
				{
					if (r->m_RP.m_pCurObject && r->m_RP.m_pCurObject->m_data.m_pTerrainSectorTextureInfo)
					{
						sData[0].f[0] = r->m_RP.m_pCurObject->m_data.m_pTerrainSectorTextureInfo->fTexOffsetX;
						sData[0].f[1] = r->m_RP.m_pCurObject->m_data.m_pTerrainSectorTextureInfo->fTexOffsetY;
						sData[0].f[2] = r->m_RP.m_pCurObject->m_data.m_pTerrainSectorTextureInfo->fTexScale;
					}
					else
					{
						sZeroLine(sData);
					}

					sData[0].f[3] = gEnv->p3DEngine->GetTerrainTextureMultiplier();
				}
				break;
			case ECGP_PB_RotGridScreenOff:
				sGetRotGridScreenOff(sData, r);
				break;
			case ECGP_PB_HDRParams:
				{
					Vec4 vHDRSetupParams[5];
					gEnv->p3DEngine->GetHDRSetupParams(vHDRSetupParams);
					// Film curve setup
					sData[0].f[0] = vHDRSetupParams[0].x * 6.2f;
					sData[0].f[1] = vHDRSetupParams[0].y * 0.5f;
					sData[0].f[2] = vHDRSetupParams[0].z * 0.06f;
					sData[0].f[3] = 1.0f;
					break;
				}
			case ECGP_PB_GlowParams:
				if (rRP.m_pShaderResources)
				{
					ColorF finalEmittance = rRP.m_pShaderResources->GetFinalEmittance();
					sData[0].f[0] = finalEmittance.r;
					sData[0].f[1] = finalEmittance.g;
					sData[0].f[2] = finalEmittance.b;
					sData[0].f[3] = 0.0f;
				}
				else
					sZeroLine(sData);
				break;
			case ECGP_PB_StereoParams:
				if (gRenDev->IsStereoEnabled())
				{
					sData[0].f[0] = gcpRendD3D->GetS3DRend().GetMaxSeparationScene() * (gcpRendD3D->GetS3DRend().GetCurrentEye() == LEFT_EYE ? 1 : -1);
					sData[0].f[1] = gcpRendD3D->GetS3DRend().GetZeroParallaxPlaneDist();
					sData[0].f[2] = gcpRendD3D->GetS3DRend().GetNearGeoShift();
					sData[0].f[3] = gcpRendD3D->GetS3DRend().GetNearGeoScale();
				}
				else
					sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0;
				break;
			case ECGP_PB_FurParams:
				sFurParams(sData);
				break;
			case ECGP_PB_IrregKernel:
				sGetIrregKernel(sData, r);
				break;
			case ECGP_PB_RegularKernel:
				sGetRegularKernel(sData, r);
				break;
			case ECGP_PB_TFactor:
				sData[0].f[0] = r->m_RP.m_CurGlobalColor[0];
				sData[0].f[1] = r->m_RP.m_CurGlobalColor[1];
				sData[0].f[2] = r->m_RP.m_CurGlobalColor[2];
				sData[0].f[3] = r->m_RP.m_CurGlobalColor[3];
				break;
			case ECGP_PB_RandomParams:
				{
					sData[0].f[0] = cry_random(0.0f, 1.0f);
					sData[0].f[1] = cry_random(0.0f, 1.0f);
					sData[0].f[2] = cry_random(0.0f, 1.0f);
					sData[0].f[3] = cry_random(0.0f, 1.0f);
				}
				break;
			case ECGP_PB_CameraFront:
				sCameraFront(sData, r);
				break;
			case ECGP_PB_CameraRight:
				sCameraRight(sData, r);
				break;
			case ECGP_PB_CameraUp:
				sCameraUp(sData, r);
				break;
			case ECGP_PB_RTRect:
				sRTRect(sData, r);
				break;
			case ECGP_PB_ResourcesOpacity:
				if (rRP.m_pShaderResources)
				{
					sData[0].f[0] = r->m_RP.m_pShaderResources->GetStrengthValue(EFTT_OPACITY);
					sData[0].f[1] = sData[0].f[0];
					sData[0].f[2] = sData[0].f[0];
					sData[0].f[3] = sData[0].f[0];
				}
				else
					sZeroLine(sData);
				break;
			case ECGP_PB_Scalar:
				assert(ParamBind->m_pData);
				if (ParamBind->m_pData)
					sData[0].f[nComp] = ParamBind->m_pData->d.fData[nComp];
				break;

			case ECGP_PB_CausticsParams:
				sData[0].f[0] = CRenderer::CV_r_watercausticsdistance;//PF.pCausticsParams.x;
				sData[0].f[1] = PF.pCausticsParams.y;
				sData[0].f[2] = PF.pCausticsParams.z;
				sData[0].f[3] = PF.pCausticsParams.x;
				break;

			case ECGP_PB_SunColor:
				{
					const SRenderLight* pLight = r->m_RP.m_pSunLight;
					if (pLight)
					{
						ColorF col = pLight->m_Color;
						sData[0].f[0] = col.r;
						sData[0].f[1] = col.g;
						sData[0].f[2] = col.b;
						sData[0].f[3] = pLight->m_SpecMult;
					}
					else
					{
						sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
					}
					break;
				}

			case ECGP_PB_CausticsSmoothSunDirection:
				sCausticsSmoothSunDirection(sData);
				break;

			case ECGP_PB_SunDirection:
				sSunDirection(sData);
				break;
			case ECGP_PB_CameraPos:
				sData[0].f[0] = vCamPos.x;
				sData[0].f[1] = vCamPos.y;
				sData[0].f[2] = vCamPos.z;
				sData[0].f[3] = (rRP.m_TI[rRP.m_nProcessThreadID].m_PersFlags & RBPF_MIRRORCULL) ? -1.0f : 1.0f;
				break;
			case ECGP_PB_HPosScale:
				sData[0].f[0] = rRP.m_CurDownscaleFactor.x;
				sData[0].f[1] = rRP.m_CurDownscaleFactor.y;
				sData[0].f[2] = r->m_PrevViewportScale.x;
				sData[0].f[3] = r->m_PrevViewportScale.y;
				break;
			case ECGP_PB_ScreenSize:
				sGetScreenSize(sData, r);
				break;
			case ECGP_PB_Time:
				//sData[0].f[nComp] = r->m_RP.m_ShaderCurrTime; //r->m_RP.m_RealTime;
				sData[0].f[nComp] = rRP.m_TI[rRP.m_nProcessThreadID].m_RealTime;
				assert(ParamBind->m_pData);
				if (ParamBind->m_pData)
					sData[0].f[nComp] *= ParamBind->m_pData->d.fData[nComp];
				break;
			case ECGP_PB_FrameTime:
				sData[0].f[nComp] = 1.f / gEnv->pTimer->GetFrameTime();
				assert(ParamBind->m_pData);
				if (ParamBind->m_pData)
					sData[0].f[nComp] *= ParamBind->m_pData->d.fData[nComp];
				break;
			case ECGP_PB_NearFarDist:
				sNearFarDist(sData, r);
				break;
			case ECGP_PB_CloudShadingColorSun:
				sData[0].f[0] = PF.pCloudShadingColorSun.x;
				sData[0].f[1] = PF.pCloudShadingColorSun.y;
				sData[0].f[2] = PF.pCloudShadingColorSun.z;
				sData[0].f[3] = 0;
				break;

			case ECGP_PB_CloudShadingColorSky:
				sData[0].f[0] = PF.pCloudShadingColorSky.x;
				sData[0].f[1] = PF.pCloudShadingColorSky.y;
				sData[0].f[2] = PF.pCloudShadingColorSky.z;
				sData[0].f[3] = 0;
				break;

			case ECGP_PB_CloudShadowParams:
				sData[0].f[0] = PF.pCloudShadowParams.x;
				sData[0].f[1] = PF.pCloudShadowParams.y;
				sData[0].f[2] = PF.pCloudShadowParams.z;
				sData[0].f[3] = PF.pCloudShadowParams.w;
				break;

			case ECGP_PB_CloudShadowAnimParams:
				sData[0].f[0] = PF.pCloudShadowAnimParams.x;
				sData[0].f[1] = PF.pCloudShadowAnimParams.y;
				sData[0].f[2] = PF.pCloudShadowAnimParams.z;
				sData[0].f[3] = PF.pCloudShadowAnimParams.w;
				break;

			case ECGP_PB_ClipVolumeParams:
				sData[0].f[0] = rRP.m_pCurObject->m_nClipVolumeStencilRef + 1.0f;
				sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
				break;

			case ECGP_PB_AlphaTest:
				{
					sData[0].f[0] = 0;
					sData[0].f[1] = (rRP.m_pCurObject->m_ObjFlags & FOB_DISSOLVE_OUT) ? 1.0f : -1.0f;
					sData[0].f[2] = 0;
					sData[0].f[3] = rRP.m_pShaderResources ? rRP.m_pShaderResources->m_AlphaRef : 0;

					if (!rRP.m_pShaderResources && (rRP.m_nPassGroupID == EFSLIST_TRANSP || rRP.m_nPassGroupID == EFSLIST_HALFRES_PARTICLES))
					{
						const SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
						sData[0].f[3] = pOD->m_fTempVars[9];
					}
				}
				break;
			case ECGP_PB_ResInfoDiffuse:
				sResInfo(sData, EFTT_DIFFUSE);
				break;
			case ECGP_PB_ResInfoGloss:
				sResInfo(sData, EFTT_SPECULAR);
				break;
			case ECGP_PB_ResInfoDetail:
				sResInfo(sData, EFTT_DETAIL_OVERLAY);
				break;
			case ECGP_PB_ResInfoOpacity:
				sResInfo(sData, EFTT_OPACITY);
				break;
			case ECGP_PB_ResInfoCustom:
				sResInfo(sData, EFTT_CUSTOM);
				break;
			case ECGP_PB_ResInfoCustom2nd:
				sResInfo(sData, EFTT_CUSTOM_SECONDARY);
				break;
			case ECGP_PB_ResInfoBump:
				sResInfoBump(sData);
				break;
			case ECGP_PB_TexelDensityParam:
				sTexelDensityParam(sData);
				break;
			case ECGP_PB_TexelDensityColor:
				sTexelDensityColor(sData);
				break;
			case ECGP_PB_TexelsPerMeterInfo:
				sTexelsPerMeterInfo(sData);
				break;
			case ECGP_PB_VisionMtlParams:
				sVisionMtlParams(sData);
				break;
			case ECGP_Matr_PB_TCMMatrix:
				{
					SEfResTexture* pRT = rRP.m_ShaderTexResources[ParamBind->m_nID];
					if (pRT && pRT->m_Ext.m_pTexModifier)
						Store(sData, pRT->m_Ext.m_pTexModifier->m_TexMatrix);
					else
					{
						Store(sData, r->m_IdentityMatrix);
					}
					break;
				}

			case ECGP_PB_WaterRipplesLookupParams:
				if ((r->m_nGraphicsPipeline) > 0)
				{
					auto pStage = r->GetGraphicsPipeline().GetWaterRipplesStage();
					CRY_ASSERT(pStage);
					auto param = pStage->GetWaterRippleLookupParam();
					sData[0].f[0] = param.x;
					sData[0].f[1] = param.y;
					sData[0].f[2] = param.z;
					sData[0].f[3] = param.w;
				}
				break;

			case ECGP_PB_SkinningExtraWeights:
				{
					if (rRP.m_pRE->mfGetType() == eDATA_Mesh && !((CREMeshImpl*)rRP.m_pRE)->m_pRenderMesh->m_extraBonesBuffer.IsNullBuffer())
					{
						sData[0].f[0] = 1.0f;
						sData[0].f[1] = 1.0f;
						sData[0].f[2] = 1.0f;
						sData[0].f[3] = 1.0f;
					}
					else
					{
						sData[0].f[0] = 0.0f;
						sData[0].f[1] = 0.0f;
						sData[0].f[2] = 0.0f;
						sData[0].f[3] = 0.0f;
					}
					break;
				}

			case 0:
				break;

			default:
#if defined(FEATURE_SVO_GI)
				if (CSvoRenderer::SetShaderParameters(pSrc, paramType & 0xff, sData))
					break;
#endif
				assert(0);
				break;
			}
			if (ParamBind->m_Flags & PF_SINGLE_COMP)
				break;

			paramType >>= 8;
		}
		if (pSrc)
		{
			if (pOutBuffer)
			{
				CRY_ASSERT((ParamBind->m_dwBind + nParams) * sizeof(Vec4) <= outBufferSize);
				memcpy(pOutBuffer + ParamBind->m_dwBind, pSrc, nParams * sizeof(Vec4));
			}
			else
			{
				// in Windows 32 pData must be 16 bytes aligned
				assert(!((uintptr_t)pSrc & 0xf) || sizeof(void*) != 4);
				if (!(ParamBind->m_Flags & PF_INTEGER))
					mfParameterfA(ParamBind, pSrc, nParams, eSH, nMaxVecs);
				else
					mfParameteri(ParamBind, pSrc, eSH, nMaxVecs);
			}
		}
		++ParamBind;
	}
}

//=========================================================================================

void CHWShader_D3D::mfReset(uint32 CRC32)
{
	DETAILED_PROFILE_MARKER("mfReset");
	for (uint32 i = 0; i < m_Insts.size(); i++)
	{
		m_pCurInst = m_Insts[i];
		assert(m_pCurInst);
		PREFAST_ASSUME(m_pCurInst);
		if (!m_pCurInst->m_bDeleted)
			m_pCurInst->Release(m_pDevCache);

		delete m_pCurInst;
	}
	m_pCurInst = NULL;
	m_Insts.clear();

	mfCloseCacheFile();
}

CHWShader_D3D::~CHWShader_D3D()
{
	mfFree(0);
}

static bool sCanSet(const STexSamplerRT* pSM, CTexture* pTex)
{
	assert(pTex);
	if (!pTex)
		return false;
	if (!pSM->m_bGlobal)
		return true;
	CD3D9Renderer* pRD = gcpRendD3D;
	if (pRD->m_pNewTarget[0] && pRD->m_pNewTarget[0]->m_pTex == pTex)
		return false;
	if (pRD->m_pNewTarget[1] && pRD->m_pNewTarget[1]->m_pTex == pTex)
		return false;
	return true;
}

#if !defined(_RELEASE)
void CHWShader_D3D::LogSamplerTextureMismatch(const CTexture* pTex, const STexSamplerRT* pSampler, EHWShaderClass shaderClass, const char* pMaterialName)
{
	if (pTex && pSampler)
	{
		const CHWShader_D3D::SHWSInstance* pInst = 0;
		switch (shaderClass)
		{
		case eHWSC_Vertex:
			pInst = s_pCurInstVS;
			break;
		case eHWSC_Pixel:
			pInst = s_pCurInstPS;
			break;
		case eHWSC_Geometry:
			pInst = s_pCurInstGS;
			break;
		case eHWSC_Compute:
			pInst = s_pCurInstCS;
			break;
		case eHWSC_Domain:
			pInst = s_pCurInstDS;
			break;
		case eHWSC_Hull:
			pInst = s_pCurInstHS;
			break;
		default:
			break;
		}

		const char* pSamplerName = "unknown";
		if (pInst)
		{
			const uint32 slot = pSampler->m_nTextureSlot;
			for (size_t idx = 0; idx < pInst->m_pBindVars.size(); ++idx)
			{
				if (slot == (pInst->m_pBindVars[idx].m_dwBind & 0xF) && (pInst->m_pBindVars[idx].m_dwBind & SHADER_BIND_SAMPLER) != 0)
				{
					pSamplerName = pInst->m_pBindVars[idx].m_Name.c_str();
					break;
				}
			}
		}

		const char* pShaderName = gcpRendD3D->m_RP.m_pShader ? gcpRendD3D->m_RP.m_pShader->GetName() : "NULL";
		const char* pTechName = gcpRendD3D->m_RP.m_pCurTechnique ? gcpRendD3D->m_RP.m_pCurTechnique->m_NameStr.c_str() : "NULL";
		const char* pSamplerTypeName = CTexture::NameForTextureType((ETEX_Type) pSampler->m_eTexType);
		const char* pTexName = pTex->GetName();
		const char* pTexTypeName = pTex->GetTypeName();
		const char* pTexSurrogateMsg = pTex->IsNoTexture() ? " (texture doesn't exist!)" : "";

		// Do not keep re-logging the same error every frame, in editor this will pop-up an error dialog (every frame), rendering it unusable (also can't save map)
		const char* pMaterialNameNotNull = pMaterialName ? pMaterialName : "none";
		CCrc32 crc;
		crc.Add(pShaderName);
		crc.Add(pTechName);
		crc.Add(pSamplerName);
		crc.Add(pSamplerTypeName);
		crc.Add(pTexName);
		crc.Add(pTexTypeName);
		crc.Add(pTexSurrogateMsg);
		crc.Add(pMaterialNameNotNull);
		const bool bShouldLog = g_ErrorsLogged.insert(crc.Get()).second;
		if (!bShouldLog) return;

		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR_DBGBRK, "!Mismatch between texture and sampler type detected! ...\n"
		                                                              "- Shader \"%s\" with technique \"%s\"\n"
		                                                              "- Sampler \"%s\" is of type \"%s\"\n"
		                                                              "- Texture \"%s\" is of type \"%s\"%s\n"
		                                                              "- Material is \"%s\"",
		           pShaderName, pTechName,
		           pSamplerName, pSamplerTypeName,
		           pTexName, pTexTypeName, pTexSurrogateMsg,
		           pMaterialName ? pMaterialName : "none");
	}
}
#endif

bool CHWShader_D3D::mfSetSamplers(const std::vector<SCGSampler>& Samplers, EHWShaderClass eSHClass)
{
	DETAILED_PROFILE_MARKER("mfSetSamplers");
	FUNCTION_PROFILER_RENDER_FLAT
	//PROFILE_FRAME(Shader_SetShaderSamplers);
	const uint32 nSize = Samplers.size();
	if (!nSize)
		return true;
	CD3D9Renderer* __restrict rd = gcpRendD3D;
	CShaderResources* __restrict pSR = rd->m_RP.m_pShaderResources;

	uint32 i;
	const SCGSampler* pSamp = &Samplers[0];
	for (i = 0; i < nSize; i++)
	{
		const SCGSampler* pSM = pSamp++;
		int nSUnit = pSM->m_dwCBufSlot;
		SamplerStateHandle nTState = pSM->m_nStateHandle;

		switch (pSM->m_eCGSamplerType)
		{
		case ECGS_Unknown:
			break;

		case ECGS_Shadow0:
		case ECGS_Shadow1:
		case ECGS_Shadow2:
		case ECGS_Shadow3:
		case ECGS_Shadow4:
		case ECGS_Shadow5:
		case ECGS_Shadow6:
		case ECGS_Shadow7:
			{
				const int nShadowMapNum = pSM->m_eCGSamplerType - ECGS_Shadow0;

				//force  MinFilter = Linear; MagFilter = Linear; for HW_PCF_FILTERING
				SShaderProfile* pSP = &gRenDev->m_cEF.m_ShaderProfiles[eST_Shadow];
				const int nShadowQuality = (int)pSP->GetShaderQuality();
				const bool bShadowsVeryHigh = nShadowQuality == eSQ_VeryHigh;
				const bool bForwardShadows = (rd->m_RP.m_pShader->m_Flags2 & EF2_ALPHABLENDSHADOWS) != 0;
				const bool bParticleShadow = (gRenDev->m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW]) != 0;
				const bool bPCFShadow = (gRenDev->m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE]) != 0;

				if ((!bShadowsVeryHigh || (nShadowMapNum != 0) || bForwardShadows || bParticleShadow) && bPCFShadow)
					// non texture array case vs. texture array case
					CTexture::SetSampler(EDefaultSamplerStates::LinearCompare, nSUnit, eSHClass);
				else
					CTexture::SetSampler(EDefaultSamplerStates::PointClamp, nSUnit, eSHClass);
			}
			break;

		case ECGS_TrilinearClamp:
			{
				CTexture::SetSampler(EDefaultSamplerStates::TrilinearClamp, nSUnit, eSHClass);
			}
			break;
		case ECGS_TrilinearWrap:
			{
				CTexture::SetSampler(EDefaultSamplerStates::TrilinearWrap, nSUnit, eSHClass);
			}
			break;
		case ECGS_MatAnisoHighWrap:
			{
				CTexture::SetSampler(gcpRendD3D->m_nMaterialAnisoHighSampler, nSUnit, eSHClass);
			}
			break;
		case ECGS_MatAnisoLowWrap:
			{
				CTexture::SetSampler(gcpRendD3D->m_nMaterialAnisoLowSampler, nSUnit, eSHClass);
			}
			break;
		case ECGS_MatTrilinearWrap:
			{
				CTexture::SetSampler(EDefaultSamplerStates::TrilinearWrap, nSUnit, eSHClass);
			}
			break;
		case ECGS_MatBilinearWrap:
			{
				CTexture::SetSampler(EDefaultSamplerStates::BilinearWrap, nSUnit, eSHClass);
			}
			break;
		case ECGS_MatTrilinearClamp:
			{
				CTexture::SetSampler(EDefaultSamplerStates::TrilinearClamp, nSUnit, eSHClass);
			}
			break;
		case ECGS_MatBilinearClamp:
			{
				CTexture::SetSampler(EDefaultSamplerStates::BilinearClamp, nSUnit, eSHClass);
			}
			break;
		case ECGS_MatAnisoHighBorder:
			{
				CTexture::SetSampler(gcpRendD3D->m_nMaterialAnisoSamplerBorder, nSUnit, eSHClass);
			}
			break;
		case ECGS_MatTrilinearBorder:
			{
				CTexture::SetSampler(EDefaultSamplerStates::TrilinearBorder_Black, nSUnit, eSHClass);
			}
			break;
		case ECGS_PointClamp:
			{
				CTexture::SetSampler(EDefaultSamplerStates::PointClamp, nSUnit, eSHClass);
			}
			break;
		case ECGS_PointWrap:
			{
				CTexture::SetSampler(EDefaultSamplerStates::PointWrap, nSUnit, eSHClass);
			}
			break;

		default:
			assert(0);
			break;
		}

#if 0
		STexSamplerRT* pS = NULL;
		if (pS)
			nTState = pS->m_nTexState;   // Use material texture state
		CTexture::SetSamplerState(nTState, nSUnit, eSHClass);
#endif
	}

	return true;
}

bool CHWShader_D3D::mfSetTextures(const std::vector<SCGTexture>& Textures, EHWShaderClass eSHClass)
{
	DETAILED_PROFILE_MARKER("mfSetTextures");
	FUNCTION_PROFILER_RENDER_FLAT

	const uint32 nSize = Textures.size();
	if (!nSize)
	{
		return true;
	}
	CD3D9Renderer* __restrict rd = gcpRendD3D;
	CShaderResources* __restrict pSR = rd->m_RP.m_pShaderResources;
	if (pSR && pSR->m_Textures[0] && pSR->m_Textures[0]->m_Sampler.m_pDynTexSource)
	{
		IDynTextureSourceImpl* pDynTexSrc = (IDynTextureSourceImpl*)pSR->m_Textures[0]->m_Sampler.m_pDynTexSource;
		if (pDynTexSrc)
		{
			pDynTexSrc->Apply(0, pSR->m_Textures[0]->m_Sampler.m_nTexState);
		}
	}

	uint32 i;
	const SCGTexture* pTexBind = &Textures[0];
	for (i = 0; i < nSize; i++)
	{
		const int nTUnit = pTexBind->m_dwCBufSlot;

		// Get appropriate view for the texture to bind (can be SRGB, MipLevels etc.)
		ResourceViewHandle hView = EDefaultResourceViews::Linear;
		if (pTexBind->m_bSRGBLookup)
			hView = EDefaultResourceViews::sRGB;

		if (pTexBind->m_eCGTextureType == ECGT_Unknown)
		{
			// Applies any animation specified for the texture (mostly by name-pattern)
			CTexture* pTexture = pTexBind->GetTexture();
			if (pTexture)
			{
				pTexture->ApplyTexture(nTUnit, eSHClass, hView);
			}

			pTexBind++;
			continue;
		}

		CTexture* pT = nullptr;
		switch (pTexBind->m_eCGTextureType)
		{
		case ECGT_MatSlot_Diffuse:
		case ECGT_MatSlot_Normals:
		case ECGT_MatSlot_Specular:
		case ECGT_MatSlot_Height:
		case ECGT_MatSlot_SubSurface:
		case ECGT_MatSlot_Smoothness:
		case ECGT_MatSlot_DecalOverlay:
		case ECGT_MatSlot_Custom:
		case ECGT_MatSlot_CustomSecondary:
		case ECGT_MatSlot_Env:
		case ECGT_MatSlot_Opacity:
		case ECGT_MatSlot_Detail:
		case ECGT_MatSlot_Emittance:
			{
				EEfResTextures texType = EEfResTextures(pTexBind->m_eCGTextureType - 1);

				CTexture* pTex = (pSR && pSR->m_Textures[texType])
				                 ? pSR->m_Textures[texType]->m_Sampler.m_pTex
				                 : TextureHelpers::LookupTexDefault(texType);

				if (pTex)
				{
					uint32 textureSlot = IShader::GetTextureSlot(texType);
					pTex->ApplyTexture(textureSlot, eSHClass, hView);
				}
			}
			break;

		case ECGT_Shadow0:
		case ECGT_Shadow1:
		case ECGT_Shadow2:
		case ECGT_Shadow3:
		case ECGT_Shadow4:
		case ECGT_Shadow5:
		case ECGT_Shadow6:
		case ECGT_Shadow7:
			{
				const int nShadowMapNum = pTexBind->m_eCGTextureType - ECGT_Shadow0;
				const int nCustomID = rd->m_RP.m_ShadowCustomTexBind[nShadowMapNum];
				if (nCustomID < 0)
				{
					break;
				}

				CTexture* tex = CTexture::GetByID(nCustomID);
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;

		case ECGT_ShadowMask:
			{
				CTexture* tex = CTexture::IsTextureExist(CTexture::s_ptexShadowMask)
				                ? CTexture::s_ptexShadowMask
				                : CTexture::s_ptexBlack;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;

		case ECGT_HDR_Target:
			{
				CTexture* tex = CTexture::s_ptexHDRTarget;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_HDR_TargetPrev:
			{
				CTexture* tex = CTexture::s_ptexHDRTargetPrev;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_HDR_AverageLuminance:
			{
				CTexture* tex = CTexture::s_ptexHDRMeasuredLuminanceDummy;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_HDR_FinalBloom:
			{
				CTexture* tex = CTexture::s_ptexHDRFinalBloom;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;

		case ECGT_BackBuffer:
			{
				CTexture* tex = CTexture::s_ptexBackBuffer;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_BackBufferScaled_d2:
			{
				CTexture* tex = CTexture::s_ptexBackBufferScaled[0];
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_BackBufferScaled_d4:
			{
				CTexture* tex = CTexture::s_ptexBackBufferScaled[1];
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_BackBufferScaled_d8:
			{
				CTexture* tex = CTexture::s_ptexBackBufferScaled[2];
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;

		case ECGT_ZTarget:
			{
				CTexture* tex = CTexture::s_ptexZTarget;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_ZTargetMS:
			{
				CTexture* tex = CTexture::s_ptexZTarget;
				tex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default, true);
			}
			break;
		case ECGT_ZTargetScaled_d2:
			{
				CTexture* tex = CTexture::s_ptexZTargetScaled;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_ZTargetScaled_d4:
			{
				CTexture* tex = CTexture::s_ptexZTargetScaled2;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;

		case ECGT_SceneTarget:
			{
				CTexture* tex = CTexture::s_ptexSceneTarget;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_SceneNormals:
			{
				CTexture* tex = CTexture::s_ptexSceneNormalsMap;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_SceneNormalsBent:
			{
				CTexture* tex = CTexture::s_ptexSceneNormalsBent;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_SceneNormalsMS:
			{
				CTexture* tex = CTexture::s_ptexSceneNormalsMapMS;
				tex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default, true);
			}
			break;
		case ECGT_SceneDiffuse:
			{
				CTexture* tex = CTexture::s_ptexSceneDiffuse;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_SceneSpecular:
			{
				CTexture* tex = CTexture::s_ptexSceneSpecular;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_SceneDiffuseAcc:
			{
				const uint32 nLightsCount = CDeferredShading::Instance().GetLightsCount();
				CTexture* tex = nLightsCount ? CTexture::s_ptexSceneDiffuseAccMap : CTexture::s_ptexBlack;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_SceneSpecularAcc:
			{
				const uint32 nLightsCount = CDeferredShading::Instance().GetLightsCount();
				CTexture* tex = nLightsCount ? CTexture::s_ptexSceneSpecularAccMap : CTexture::s_ptexBlack;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;

		case ECGT_SceneDiffuseAccMS:
			{
				const uint32 nLightsCount = CDeferredShading::Instance().GetLightsCount();
				CTexture* tex = nLightsCount ? CTexture::s_ptexSceneDiffuseAccMapMS : CTexture::s_ptexBlack;
				tex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default, true);
			}
			break;
		case ECGT_SceneSpecularAccMS:
			{
				const uint32 nLightsCount = CDeferredShading::Instance().GetLightsCount();
				CTexture* tex = nLightsCount ? CTexture::s_ptexSceneSpecularAccMapMS : CTexture::s_ptexBlack;
				tex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default, true);
			}
			break;

		case ECGT_VolumetricClipVolumeStencil:
			{
				CTexture* tex = CTexture::s_ptexVolumetricClipVolumeStencil;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_VolumetricFog:
			{
				CTexture* tex = CTexture::s_ptexVolumetricFog;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_VolumetricFogGlobalEnvProbe0:
			{
				CTexture* pTex = nullptr;
				if ((rd->m_nGraphicsPipeline) > 0)
				{
					auto pStage = rd->GetGraphicsPipeline().GetVolumetricFogStage();
					CRY_ASSERT(pStage);
					pTex = pStage->GetGlobalEnvProbeTex0();
				}
				pTex = (pTex != nullptr) ? pTex : CTexture::s_ptexBlackCM;
				pTex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_VolumetricFogGlobalEnvProbe1:
			{
				CTexture* pTex = nullptr;
				if ((rd->m_nGraphicsPipeline) > 0)
				{
					auto pStage = rd->GetGraphicsPipeline().GetVolumetricFogStage();
					CRY_ASSERT(pStage);
					pTex = pStage->GetGlobalEnvProbeTex1();
				}
				pTex = (pTex != nullptr) ? pTex : CTexture::s_ptexBlackCM;
				pTex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
#if defined(VOLUMETRIC_FOG_SHADOWS)
		case ECGT_VolumetricFogShadow0:
			{
				CTexture* tex = CTexture::s_ptexVolFogShadowBuf[0];
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_VolumetricFogShadow1:
			{
				CTexture* tex = CTexture::s_ptexVolFogShadowBuf[1];
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
#endif
		case ECGT_WaterOceanMap:
			{
				CTexture* tex = CTexture::s_ptexWaterOcean;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_WaterVolumeDDN:
			{
				CTexture* tex = CTexture::s_ptexWaterVolumeDDN;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_WaterVolumeCaustics:
			{
				CTexture* tex = CTexture::s_ptexWaterCaustics[0];
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_WaterVolumeRefl:
			{
				const uint32 nCurrWaterVolID = gRenDev->GetFrameID(false) % 2;
				CTexture* pTex = CTexture::s_ptexWaterVolumeRefl[nCurrWaterVolID] ? CTexture::s_ptexWaterVolumeRefl[nCurrWaterVolID] : CTexture::s_ptexBlack;
				pTex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_WaterVolumeReflPrev:
			{
				const uint32 nPrevWaterVolID = (gRenDev->GetFrameID(false) + 1) % 2;
				CTexture* pTex = CTexture::s_ptexWaterVolumeRefl[nPrevWaterVolID] ? CTexture::s_ptexWaterVolumeRefl[nPrevWaterVolID] : CTexture::s_ptexBlack;
				pTex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_RainOcclusion:
			{
				CTexture* tex = CTexture::s_ptexRainOcclusion;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_TerrainBaseMap:
			{
				int tex0 = 0, tex1 = 0, tex2 = 0;
				ITerrain* const pTerrain = gEnv->p3DEngine->GetITerrain();
				if (pTerrain)
				{
					pTerrain->GetAtlasTexId(tex0, tex1, tex2);
					CTexture* const pTex = CTexture::GetByID(tex0);
					pTex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::sRGB);
				}
				else
				{
					CTexture* const pTex = CTexture::s_ptexBlack;
					pTex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::sRGB);
				}
			}
			break;
		case ECGT_TerrainNormMap:
			{
				int tex0 = 0, tex1 = 0, tex2 = 0;
				ITerrain* const pTerrain = gEnv->p3DEngine->GetITerrain();
				if (pTerrain)
				{
					pTerrain->GetAtlasTexId(tex0, tex1, tex2);
					CTexture* const pTex = CTexture::GetByID(tex1);
					pTex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default);
				}
				else
				{
					CTexture* const pTex = CTexture::s_ptexBlack;
					pTex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default);
				}
			}
			break;
		case ECGT_TerrainElevMap:
			{
				int tex0 = 0, tex1 = 0, tex2 = 0;
				ITerrain* const pTerrain = gEnv->p3DEngine->GetITerrain();
				if (pTerrain)
				{
					pTerrain->GetAtlasTexId(tex0, tex1, tex2);
					CTexture* const pTex = CTexture::GetByID(tex2);
					pTex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default);
				}
				else
				{
					CTexture* const pTex = CTexture::s_ptexBlack;
					pTex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default);
				}
			}
			break;
		case ECGT_WindGrid:
			{
				CTexture* tex = CTexture::s_ptexWindGrid;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		case ECGT_CloudShadow:
			{
				const bool setupCloudShadows = rd->m_bShadowsEnabled && rd->m_bCloudShadowsEnabled;
				CTexture* pCloudShadowTex = CTexture::s_ptexWhite;
				if (setupCloudShadows)
				{
					// cloud shadow map
					pCloudShadowTex =
					  (rd->GetCloudShadowTextureId() > 0)
					  ? CTexture::GetByID(rd->GetCloudShadowTextureId())
					  : pCloudShadowTex;
				}
				pCloudShadowTex->ApplyTexture(nTUnit, eSHClass, EDefaultResourceViews::Default);
			}
			break;
		case ECGT_VolCloudShadow:
			{
				CTexture* tex = CTexture::s_ptexVolCloudShadow;
				tex->ApplyTexture(nTUnit, eSHClass, hView);
			}
			break;
		default:
			assert(0);
			break;
		}

		pTexBind++;
	}

	return true;
}

bool CHWShader_D3D::mfSetSamplers_Old(const std::vector<STexSamplerRT>& Samplers, EHWShaderClass eSHClass)
{
	DETAILED_PROFILE_MARKER("mfSetSamplers_Old");
	FUNCTION_PROFILER_RENDER_FLAT

	const uint32 nSize = Samplers.size();
	if (!nSize)
		return true;
	CD3D9Renderer* __restrict rd = gcpRendD3D;
	CShaderResources* __restrict pSR = rd->m_RP.m_pShaderResources;

	uint32 i;
	const STexSamplerRT* pSamp = &Samplers[0];
	// Loop counter increments moved to resolve an issue where the compiler introduced
	// load hit stores by storing the counters as the last instruction in the loop then
	// immediated reloading and incrementing them after the branch back to the top
	for (i = 0; i < nSize; /*i++, pSamp++*/)
	{
		CTexture* tx = pSamp->m_pTex;
		assert(tx);
		if (!tx)
		{
			++pSamp;
			++i;
			continue;
		}
		//int nSetID = -1;
		int nTexMaterialSlot = EFTT_UNKNOWN;
		const STexSamplerRT* pSM = pSamp++;
		int nSUnit = pSM->m_nSamplerSlot;
		int nTUnit = pSM->m_nTextureSlot;
		assert(nTUnit >= 0);
		SamplerStateHandle nTState = pSM->m_nTexState;
		const ETEX_Type smpTexType = (ETEX_Type) pSM->m_eTexType;
		++i;
		if (tx >= &CTexture::s_ShaderTemplates[0] && tx <= &CTexture::s_ShaderTemplates[EFTT_MAX - 1])
		{
			nTexMaterialSlot = (int)(tx - &CTexture::s_ShaderTemplates[0]);

			if (!pSR || !pSR->m_Textures[nTexMaterialSlot])
				tx = TextureHelpers::LookupTexDefault((EEfResTextures)nTexMaterialSlot);
			else
			{
				pSM = &pSR->m_Textures[nTexMaterialSlot]->m_Sampler;
				tx = pSM->m_pTex;

				if (nTState == EDefaultSamplerStates::Unspecified || !CDeviceObjectFactory::LookupSamplerState(nTState).first.m_bActive)
					nTState = pSM->m_nTexState;   // Use material texture state

				IDynTextureSourceImpl* pDynTexSrc = (IDynTextureSourceImpl*) pSM->m_pDynTexSource;
				if (pDynTexSrc)
				{
					if (pDynTexSrc->Apply(nTUnit, nTState))
						continue;
					else
						tx = CTexture::s_ptexWhite;
				}
			}
		}

		IF (pSM && pSM->m_pAnimInfo, 0)
		{
			STexSamplerRT* pRT = (STexSamplerRT*)pSM;
			pRT->Update();
			tx = pRT->m_pTex;
		}

		IF (!tx || tx->GetCustomID() <= 0 && smpTexType != tx->GetTexType(), 0)
		{
#if !defined(_RELEASE)
			string matName = "unknown";

			if (pSR->m_szMaterialName)
				matName = pSR->m_szMaterialName;

			CRenderObject* pObj = gcpRendD3D->m_RP.m_pCurObject;

			if (pObj && pObj->m_pCurrMaterial)
				matName.Format("%s/%s", pObj->m_pCurrMaterial->GetName(), pSR->m_szMaterialName ? pSR->m_szMaterialName : "unknown");

			if (tx && !tx->IsNoTexture())
				LogSamplerTextureMismatch(tx, pSamp - 1, eSHClass, pSR && (nTexMaterialSlot >= 0 && nTexMaterialSlot < EFTT_UNKNOWN) ? matName : "none");
#endif
			tx = CTexture::s_pTexNULL;
		}

		{
			int nCustomID = tx->GetCustomID();
			if (nCustomID <= 0)
			{
				if (tx->UseDecalBorderCol())
				{
					SSamplerState TS = CDeviceObjectFactory::LookupSamplerState(nTState).first;
					TS.SetClampMode(eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp);
					nTState = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(TS);
				}

				if (CRenderer::CV_r_texNoAnisoAlphaTest && (rd->m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_ALPHATEST]))
				{
					SSamplerState TS = CDeviceObjectFactory::LookupSamplerState(nTState).first;
					if (TS.m_nAnisotropy > 1)
					{
						TS.m_nAnisotropy = 1;
						TS.SetFilterMode(FILTER_TRILINEAR);
						nTState = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(TS);
					}
				}

				tx->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
			}
			else
				switch (nCustomID)
				{
				case TO_FROMRE0:
				case TO_FROMRE1:
					{
						if (rd->m_RP.m_pRE)
							nCustomID = rd->m_RP.m_pRE->m_CustomTexBind[nCustomID - TO_FROMRE0];
						else
							nCustomID = rd->m_RP.m_RECustomTexBind[nCustomID - TO_FROMRE0];
						if (nCustomID < 0)
							break;

						CTexture* pTex = CTexture::GetByID(nCustomID);
						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_FROMRE0_FROM_CONTAINER:
				case TO_FROMRE1_FROM_CONTAINER:
					{
						// take render element from vertex container render mesh if available
						CRenderElement* pRE = sGetContainerRE0(rd->m_RP.m_pRE);
						if (pRE)
							nCustomID = pRE->m_CustomTexBind[nCustomID - TO_FROMRE0_FROM_CONTAINER];
						else
							nCustomID = rd->m_RP.m_RECustomTexBind[nCustomID - TO_FROMRE0_FROM_CONTAINER];
						if (nCustomID < 0)
							break;
						CTexture::ApplyForID(nTUnit, nCustomID, nTState, nSUnit);
					}
					break;

				case TO_ZTARGET_MS:
					{
						CTexture* pTex = CTexture::s_ptexZTarget;
						assert(pTex);
						if (pTex)
							pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nTUnit, EDefaultResourceViews::Default, true);
					}
					break;

				case TO_SCENE_NORMALMAP_MS:
				case TO_SCENE_NORMALMAP:
					{
						CTexture* pTex = CTexture::s_ptexSceneNormalsMap;
						if (sCanSet(pSM, pTex))
						{
							if (nCustomID != TO_SCENE_NORMALMAP_MS)
								pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default);
							else
								pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nTUnit, EDefaultResourceViews::Default, true);
						}
					}
					break;

				case TO_SHADOWID0:
				case TO_SHADOWID1:
				case TO_SHADOWID2:
				case TO_SHADOWID3:
				case TO_SHADOWID4:
				case TO_SHADOWID5:
				case TO_SHADOWID6:
				case TO_SHADOWID7:
					{
						const int nShadowMapNum = nCustomID - TO_SHADOWID0;
						nCustomID = rd->m_RP.m_ShadowCustomTexBind[nShadowMapNum];

						if (nCustomID < 0)
							break;

						//force  MinFilter = Linear; MagFilter = Linear; for HW_PCF_FILTERING
						SSamplerState TS = CDeviceObjectFactory::LookupSamplerState(nTState).first;
						TS.SetClampMode(eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp);

						const bool bComparisonSampling = rd->m_RP.m_ShadowCustomComparisonSampling[nShadowMapNum];
						if (bComparisonSampling)
						{
							TS.SetFilterMode(FILTER_LINEAR);
							TS.SetComparisonFilter(true);
						}
						else
						{
							TS.SetFilterMode(FILTER_POINT);
							TS.SetComparisonFilter(false);
						}

						CTexture* tex = CTexture::GetByID(nCustomID);
						tex->Apply(nTUnit, CDeviceObjectFactory::GetOrCreateSamplerStateHandle(TS), nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
					}
					break;

				case TO_SHADOWMASK:
					{
						CTexture* pTex = CTexture::IsTextureExist(CTexture::s_ptexShadowMask)
						                 ? CTexture::s_ptexShadowMask
						                 : CTexture::s_ptexBlack;

						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_SCENE_DIFFUSE_ACC_MS:
				case TO_SCENE_DIFFUSE_ACC:
					{
						const uint32 nLightsCount = CDeferredShading::Instance().GetLightsCount();
						CTexture* pTex = nLightsCount ? CTexture::s_ptexCurrentSceneDiffuseAccMap : CTexture::s_ptexBlack;
						if (sCanSet(pSM, pTex))
						{
							if (!(nLightsCount && nCustomID == TO_SCENE_DIFFUSE_ACC_MS))
								pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default);
							else
								pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nTUnit, EDefaultResourceViews::Default, true);
						}
					}
					break;

				case TO_SCENE_SPECULAR_ACC_MS:
				case TO_SCENE_SPECULAR_ACC:
					{
						const uint32 nLightsCount = CDeferredShading::Instance().GetLightsCount();
						CTexture* pTex = nLightsCount ? CTexture::s_ptexSceneSpecularAccMap : CTexture::s_ptexBlack;
						if (sCanSet(pSM, pTex))
						{
							if (!(nLightsCount && nCustomID == TO_SCENE_SPECULAR_ACC_MS))
								pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default);
							else
								pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nTUnit, EDefaultResourceViews::Default, true);
						}
					}
					break;

				case TO_SCENE_TARGET:
					{
						CTexture* tex = CTexture::s_ptexCurrSceneTarget;
						if (!tex)
							tex = CTexture::s_ptexWhite;

						tex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_DOWNSCALED_ZTARGET_FOR_AO:
					{
						assert(CTexture::s_ptexZTargetScaled);
						if (CTexture::s_ptexZTargetScaled)
							CTexture::s_ptexZTargetScaled->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_QUARTER_ZTARGET_FOR_AO:
					{
						assert(CTexture::s_ptexZTargetScaled2);
						if (CTexture::s_ptexZTargetScaled2)
							CTexture::s_ptexZTargetScaled2->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_FROMOBJ:
					{
						CTexture* pTex = CTexture::s_ptexBlack;
						if (rd->m_RP.m_pCurObject)
						{
							nCustomID = rd->m_RP.m_pCurObject->m_nTextureID;
							if (nCustomID > 0)
							{
								pTex = CTexture::GetByID(nCustomID);
							}
							else if ((rd->m_RP.m_ObjFlags & FOB_BLEND_WITH_TERRAIN_COLOR) && (nCustomID < 0))
							{
								// terrain atlas texture id
								int nTexID0, nTexID1, nTexID2;
								gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTexID0, nTexID1, nTexID2);
								pTex = CTexture::GetByID(nTexID0);
							}
						}
						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_FROMOBJ_CM:
					{
						CTexture* pTex = CTexture::s_ptexNoTextureCM;
						if (rd->m_RP.m_pCurObject)
						{
							nCustomID = rd->m_RP.m_pCurObject->m_nTextureID;
							if (nCustomID > 0)
								pTex = CTexture::GetByID(nCustomID);
							else if (nCustomID == 0)
								pTex = CTexture::s_ptexBlackCM;
						}
						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_RT_2D:
					{
						SHRenderTarget* pRT = pSM->m_pTarget ? pSM->m_pTarget : pSamp->m_pTarget;
						SEnvTexture* pEnvTex = pRT->GetEnv2D();
						//assert(pEnvTex->m_pTex);
						if (pEnvTex && pEnvTex->m_pTex)
							pEnvTex->m_pTex->Apply(nTUnit, nTState);
						else
							CTexture::s_ptexBlack->Apply(nTUnit, nTState);
					}
					break;

				case TO_WATEROCEANMAP:
					CTexture::s_ptexWaterOcean->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
					break;

				case TO_WATERVOLUMEREFLMAP:
					{
						const uint32 nCurrWaterVolID = gRenDev->GetFrameID(false) % 2;
						CTexture* pTex = CTexture::s_ptexWaterVolumeRefl[nCurrWaterVolID] ? CTexture::s_ptexWaterVolumeRefl[nCurrWaterVolID] : CTexture::s_ptexBlack;
						pTex->Apply(nTUnit, CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_ANISO16X, true)), nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
					}
					break;

				case TO_WATERVOLUMEREFLMAPPREV:
					{
						const uint32 nPrevWaterVolID = (gRenDev->GetFrameID(false) + 1) % 2;
						CTexture* pTex = CTexture::s_ptexWaterVolumeRefl[nPrevWaterVolID] ? CTexture::s_ptexWaterVolumeRefl[nPrevWaterVolID] : CTexture::s_ptexBlack;
						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
					}
					break;

				case TO_WATERVOLUMECAUSTICSMAP:
					{
						const uint32 nCurrWaterVolID = gRenDev->GetFrameID(false) % 2;
						CTexture* pTex = CTexture::s_ptexWaterCaustics[nCurrWaterVolID];
						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
					}
					break;

				case TO_WATERVOLUMECAUSTICSMAPTEMP:
					{
						const uint32 nPrevWaterVolID = (gRenDev->GetFrameID(false) + 1) % 2;
						CTexture* pTex = CTexture::s_ptexWaterCaustics[nPrevWaterVolID];
						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
					}
					break;

				case TO_WATERVOLUMEMAP:
					{
						const auto* pStage = rd->GetGraphicsPipeline().GetWaterStage();
						if (pStage && pStage->IsNormalGenActive() && CTexture::s_ptexWaterVolumeDDN)
						{
							CTexture::s_ptexWaterVolumeDDN->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
						}
						else
							CTexture::s_ptexFlatBump->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);

					}
					break;

				case TO_TERRAIN_LM:
					{
						const bool setupTerrainShadows = rd->m_bShadowsEnabled;
						if (setupTerrainShadows)
						{
							// terrain shadow map
							ITerrain* pTerrain(gEnv->p3DEngine->GetITerrain());
							Vec4 texGenInfo;
							int terrainLightMapID(pTerrain->GetTerrainLightmapTexId(texGenInfo));
							CTexture* pTerrainLightMap(terrainLightMapID > 0 ? CTexture::GetByID(terrainLightMapID) : CTexture::s_ptexWhite);
							assert(pTerrainLightMap);

							pTerrainLightMap->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
						}
						else
						{
							CTexture::s_ptexWhite->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
						}

						break;
					}

				case TO_BACKBUFFERSCALED_D2:
				case TO_BACKBUFFERSCALED_D4:
				case TO_BACKBUFFERSCALED_D8:
					{
						const uint32 nTargetID = nCustomID - TO_BACKBUFFERSCALED_D2;
						CTexture* pTex = CTexture::s_ptexBackBufferScaled[nTargetID] ? CTexture::s_ptexBackBufferScaled[nTargetID] : CTexture::s_ptexBlack;
						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
					}
					break;

				case TO_CLOUDS_LM:
					{
						const bool setupCloudShadows = rd->m_bShadowsEnabled && rd->m_bCloudShadowsEnabled;
						if (setupCloudShadows)
						{
							// cloud shadow map
							CTexture* pCloudShadowTex(rd->GetCloudShadowTextureId() > 0 ? CTexture::GetByID(rd->GetCloudShadowTextureId()) : CTexture::s_ptexWhite);
							assert(pCloudShadowTex);

							pCloudShadowTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
						}
						else
						{
							CTexture::s_ptexWhite->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
						}

						break;
					}

				case TO_MIPCOLORS_DIFFUSE:
					CTexture::s_ptexWhite->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					break;

				case TO_BACKBUFFERMAP:
					{
						CTexture* pBackBufferTex = CTexture::s_ptexBackBuffer;
						if (pBackBufferTex)
							pBackBufferTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_HDR_MEASURED_LUMINANCE:
					{
						CTexture::s_ptexHDRMeasuredLuminance[gRenDev->RT_GetCurrGpuID()]->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;

				case TO_COLORCHART:
					{
						CColorGradingControllerD3D* pCtrl = gcpRendD3D->m_pColorGradingControllerD3D;
						if (pCtrl)
						{
							CTexture* pTex = pCtrl->GetColorChart();
							if (pTex)
							{
								pTex->Apply(nTUnit, EDefaultSamplerStates::LinearClamp);
								break;
							}
						}
						CRenderer::CV_r_colorgrading_charts = 0;
						CTexture::s_ptexWhite->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
						break;
					}

				case TO_SKYDOME_MIE:
				case TO_SKYDOME_RAYLEIGH:
					{
						CRenderElement* pRE = rd->m_RP.m_pRE;
						if (pRE && pRE->mfGetType() == eDATA_HDRSky)
						{
							CTexture* pTex = nCustomID == TO_SKYDOME_MIE ? ((CREHDRSky*) pRE)->m_pSkyDomeTextureMie : ((CREHDRSky*) pRE)->m_pSkyDomeTextureRayleigh;
							if (pTex)
							{
								pTex->Apply(nTUnit, EDefaultSamplerStates::Unspecified, nTexMaterialSlot, nSUnit);
								break;
							}
						}
						CTexture::s_ptexBlack->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
						break;
					}

				case TO_SKYDOME_MOON:
					{
						CRenderElement* pRE = rd->m_RP.m_pRE;
						if (pRE && pRE->mfGetType() == eDATA_HDRSky)
						{
							CREHDRSky* pHDRSky = (CREHDRSky*) pRE;
							CTexture* pMoonTex(pHDRSky->m_moonTexId > 0 ? CTexture::GetByID(pHDRSky->m_moonTexId) : 0);
							if (pMoonTex)
							{
								pMoonTex->Apply(nTUnit, EDefaultSamplerStates::BilinearBorder_Black);
								break;
							}
						}
						CTexture::s_ptexBlack->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
						break;
					}

				case TO_VOLFOGSHADOW_BUF:
					{
#if defined(VOLUMETRIC_FOG_SHADOWS)
						const bool enabled = gRenDev->m_bVolFogShadowsEnabled;
						assert(enabled);
						CTexture* pTex = enabled ? CTexture::s_ptexVolFogShadowBuf[0] : CTexture::s_ptexWhite;
						pTex->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit, EDefaultResourceViews::Default, false, eSHClass);
						break;
#else
						assert(0);
						break;
#endif
					}

				default:
					{
#if defined(FEATURE_SVO_GI)
						if (CSvoRenderer::SetSamplers(nCustomID, eSHClass, nTUnit, nTState, nTexMaterialSlot, nSUnit))
							break;
#endif
						tx->Apply(nTUnit, nTState, nTexMaterialSlot, nSUnit);
					}
					break;
				}
		}
	}

	return true;
}

bool CHWShader_D3D::mfUpdateSamplers(CShader* pSH)
{
	DETAILED_PROFILE_MARKER("mfUpdateSamplers");
	FUNCTION_PROFILER_RENDER_FLAT
	if (!m_pCurInst)
		return false;
	SHWSInstance* pInst = m_pCurInst;
	if (pInst->m_Textures.empty())
		return true;

	CD3D9Renderer* rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	CShaderResources* pSR = gRenDev->m_RP.m_pShaderResources;
	bool bDiffuseSlotUpdated = false;

	static_assert((ECGT_MatSlot_Diffuse - 1) == EFTT_DIFFUSE && (ECGT_MatSlot_Emittance - 1) == EFTT_EMITTANCE, "Mismatch in texture slots");
	static_assert(EFTT_DIFFUSE == 0 && EFTT_EMITTANCE == (EFTT_MAX - 1), "Mismatch in texture slots");

	for (const SCGTexture& tex : pInst->m_Textures)
	{
		if (tex.m_eCGTextureType >= ECGT_MatSlot_Diffuse && tex.m_eCGTextureType <= ECGT_MatSlot_Emittance)
		{
			EEfResTextures texType = EEfResTextures(tex.m_eCGTextureType - 1);

			if (pSR && pSR->m_Textures[texType])
			{
				if (texType != EFTT_DETAIL_OVERLAY)
					pSR->m_Textures[texType]->Update(texType);

				if (texType == EFTT_HEIGHT || texType == EFTT_SMOOTHNESS)
				{
					SEfResTexture* pBumpTexSlot = pSR->m_Textures[EFTT_NORMALS];
					if (pBumpTexSlot)
						pBumpTexSlot->Update(EFTT_NORMALS);
				}
				else if (texType == EFTT_DIFFUSE)
					bDiffuseSlotUpdated = true;

				if ((rTI.m_PersFlags & RBPF_ZPASS) && texType == EFTT_NORMALS && pSR && !bDiffuseSlotUpdated)
				{
					if (pSR->m_Textures[EFTT_DIFFUSE])
						pSR->m_Textures[EFTT_DIFFUSE]->Update(EFTT_DIFFUSE);
				}
			}
		}
	}

	if (pSR && pSR->m_Textures[EFTT_DIFFUSE] && pSR->m_Textures[EFTT_DIFFUSE]->IsNeedTexTransform())
	{
		pSR->RT_UpdateConstants(pSH);
	}

	return true;
}

void CHWShader_D3D::mfInit()
{
	CGParamManager::Init();
}

ED3DShError CHWShader_D3D::mfFallBack(SHWSInstance*& pInst, int nStatus)
{
	// No fallback for:
	//  - ShadowGen pass
	//  - Z-prepass
	//  - Shadow-pass
	if (CParserBin::m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_GL4 | SF_GLES3))
	{
		//assert(gRenDev->m_cEF.m_nCombinationsProcess >= 0);
		return ED3DShError_CompilingError;
	}
	if (
	  m_eSHClass == eHWSC_Geometry || m_eSHClass == eHWSC_Domain || m_eSHClass == eHWSC_Hull ||
	  (gRenDev->m_RP.m_nBatchFilter & FB_Z) || gRenDev->m_RP.m_nPassGroupID == EFSLIST_SHADOW_PASS)
		return ED3DShError_CompilingError;
	if (gRenDev->m_RP.m_pShader)
	{
		if (gRenDev->m_RP.m_pShader->GetShaderType() == eST_HDR || gRenDev->m_RP.m_pShader->GetShaderType() == eST_PostProcess || gRenDev->m_RP.m_pShader->GetShaderType() == eST_Water || gRenDev->m_RP.m_pShader->GetShaderType() == eST_Shadow || gRenDev->m_RP.m_pShader->GetShaderType() == eST_Shadow)
			return ED3DShError_CompilingError;
	}
	// Skip rendering if async compiling Cvar is 2
	if (CRenderer::CV_r_shadersasynccompiling == 2)
		return ED3DShError_CompilingError;

	CShader* pSH = CShaderMan::s_ShaderFallback;
	int nTech = 0;
	if (nStatus == -1)
	{
		pInst->m_Handle.m_bStatus = 1;
		nTech = 1;
	}
	else
	{
		nTech = 0;
		assert(nStatus == 0);
	}
	if (gRenDev->m_RP.m_pShader && gRenDev->m_RP.m_pShader->GetShaderType() == eST_Terrain)
		nTech += 2;

	assert(pSH);
	if (CRenderer::CV_r_logShaders)
	{
		char nameSrc[256];
		mfGetDstFileName(pInst, this, nameSrc, 256, 3);
		gcpRendD3D->LogShv("Async %d: using Fallback tech '%s' instead of 0x%x '%s' shader\n", gRenDev->GetFrameID(false), pSH->m_HWTechniques[nTech]->m_NameStr.c_str(), pInst, nameSrc);
	}
	// Fallback
	if (pSH)
	{
		if (gRenDev->m_RP.m_CurState & GS_DEPTHFUNC_EQUAL)
		{
			int nState = gRenDev->m_RP.m_CurState & ~GS_DEPTHFUNC_EQUAL;
			nState |= GS_DEPTHWRITE;
			gRenDev->FX_SetState(nState);
		}
		CHWShader_D3D* pHWSH;
		if (m_eSHClass == eHWSC_Vertex)
		{
			pHWSH = (CHWShader_D3D*)pSH->m_HWTechniques[nTech]->m_Passes[0].m_VShader;
#ifdef DO_RENDERLOG
			if (CRenderer::CV_r_log >= 3)
				gRenDev->Logv("---- Fallback FX VShader \"%s\"\n", pHWSH->GetName());
#endif
		}
		else
		{
			pHWSH = (CHWShader_D3D*)pSH->m_HWTechniques[nTech]->m_Passes[0].m_PShader;
#ifdef DO_RENDERLOG
			if (CRenderer::CV_r_log >= 3)
				gRenDev->Logv("---- Fallback FX PShader \"%s\"\n", pHWSH->GetName());
#endif
		}

		if (!pHWSH->m_Insts.size())
		{
			SShaderCombination cmb;
			pHWSH->mfPrecache(cmb, true, true, gRenDev->m_RP.m_pShader, gRenDev->m_RP.m_pShaderResources);
		}
		if (pHWSH->m_Insts.size())
		{
			SHWSInstance* pInstF = pHWSH->m_Insts[0];
			if (!pInstF->m_Handle.m_pShader || !pInstF->m_Handle.m_pShader->m_pHandle)
				return ED3DShError_CompilingError;
			pInst = pInstF;
			m_pCurInst = pInstF;
			pInstF->m_bFallback = true;
		}
		else
			return ED3DShError_CompilingError;
	}
	//if (nStatus == 0)
	//  return ED3DShError_Compiling;
	return ED3DShError_Ok;
}

ED3DShError CHWShader_D3D::mfIsValid_Int(SHWSInstance*& pInst, bool bFinalise)
{
	//if (stricmp(m_EntryFunc.c_str(), "FPPS") && stricmp(m_EntryFunc.c_str(), "FPVS") && stricmp(m_EntryFunc.c_str(), "AuxGeomPS") && stricmp(m_EntryFunc.c_str(), "AuxGeomVS"))
	//  return mfFallBack(pInst, 0);

	if (pInst->m_Handle.m_bStatus == 1)
	{
		return mfFallBack(pInst, -1);
	}
	if (pInst->m_Handle.m_bStatus == 2)
		return ED3DShError_Fake;
	if (pInst->m_Handle.m_pShader == NULL)
	{
		if (pInst->m_bAsyncActivating)
			return mfFallBack(pInst, 0);

		if (!bFinalise || !pInst->m_pAsync)
			return ED3DShError_NotCompiled;

		int nStatus = 0;
		if (!pInst->m_bAsyncActivating)
		{
			nStatus = mfAsyncCompileReady(pInst);
			if (nStatus == 1)
			{
				if (gcpRendD3D->m_cEF.m_nCombinationsProcess <= 0)
				{
					assert(pInst->m_Handle.m_pShader != NULL);
				}
				return ED3DShError_Ok;
			}
		}
		return mfFallBack(pInst, nStatus);
	}
	return ED3DShError_Ok;
}

//========================================================================================

void CHWShader::mfLazyUnload()
{
	int nScanned = 0;
	int nUnloaded = 0;
	static int nLastScannedPS = 0;
	static int nLastScannedVS = 0;
	static int sbReset = 0;
	if (!gRenDev->m_bEndLevelLoading)
	{
		sbReset = 0;
		nLastScannedPS = 0;
		nLastScannedVS = 0;
		return;
	}

	AUTO_LOCK(CBaseResource::s_cResLock);

	CCryNameTSCRC Name = CHWShader::mfGetClassName(eHWSC_Pixel);
	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(Name);
	uint32 i;
	uint32 j;
	float fTime = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_RealTime;

	float fThr = (float)CRenderer::CV_r_shaderslazyunload;
	if (pRL)
	{
		for (i = nLastScannedPS; i < pRL->m_RList.size(); i++)
		{
			CHWShader_D3D* pPS = (CHWShader_D3D*)pRL->m_RList[i];
			int nDeleted = 0;
			for (j = 0; j < pPS->m_Insts.size(); j++)
			{
				CHWShader_D3D::SHWSInstance* pInst = pPS->m_Insts[j];
				if (pInst->m_bDeleted)
					continue;
				if (pInst->m_pAsync)
					continue;
				if (sbReset != 3)
					pInst->m_fLastAccess = fTime;
				else if (fTime - pInst->m_fLastAccess > fThr)
				{
					pPS->m_pCurInst = pInst;
					pInst->Release(pPS->m_pDevCache, false);
					pInst->m_bDeleted = true;
					nDeleted++;
					nUnloaded++;
					pPS->m_pCurInst = NULL;
				}
			}
			//if (nDeleted == pPS->m_Insts.size())
			//  pPS->mfReset(0);
			nScanned++;
			if (nUnloaded > 16)
				break;
			if (nScanned > 32)
				break;
		}
		if (i >= pRL->m_RList.size())
		{
			sbReset |= 1;
			i = 0;
		}
		nLastScannedPS = i;
	}
	Name = CHWShader::mfGetClassName(eHWSC_Vertex);
	pRL = CBaseResource::GetResourcesForClass(Name);
	nUnloaded = 0;
	nScanned = 0;
	if (pRL)
	{
		for (i = nLastScannedVS; i < pRL->m_RList.size(); i++)
		{
			CHWShader_D3D* pVS = (CHWShader_D3D*)pRL->m_RList[i];
			int nDeleted = 0;
			for (j = 0; j < pVS->m_Insts.size(); j++)
			{
				CHWShader_D3D::SHWSInstance* pInst = pVS->m_Insts[j];
				if (pInst->m_bDeleted)
					continue;
				if (pInst->m_pAsync)
					continue;
				if (sbReset != 3)
					pInst->m_fLastAccess = fTime;
				else if (fTime - pInst->m_fLastAccess > CRenderer::CV_r_shaderslazyunload)
				{
					pVS->m_pCurInst = pInst;
					pInst->Release(pVS->m_pDevCache, false);
					pInst->m_bDeleted = true;
					nDeleted++;
					nUnloaded++;
					pVS->m_pCurInst = NULL;
				}
			}
			//if (nDeleted == pVS->m_Insts.size())
			//  pVS->mfReset(0);
			nScanned++;
			if (nUnloaded > 16)
				break;
			if (nScanned > 32)
				break;
		}
		if (i >= pRL->m_RList.size())
		{
			sbReset |= 2;
			i = 0;
		}
		nLastScannedVS = i;
	}
}

struct InstContainerByHash
{
	bool operator()(const CHWShader_D3D::SHWSInstance* left, const CHWShader_D3D::SHWSInstance* right) const
	{
		return left->m_Ident.m_nHash < right->m_Ident.m_nHash;
	}
	bool operator()(const uint32 left, const CHWShader_D3D::SHWSInstance* right) const
	{
		return left < right->m_Ident.m_nHash;
	}
	bool operator()(const CHWShader_D3D::SHWSInstance* left, uint32 right) const
	{
		return left->m_Ident.m_nHash < right;
	}
};

CHWShader_D3D::SHWSInstance* CHWShader_D3D::mfGetHashInst(InstContainer* pInstCont, uint32 identHash, SShaderCombIdent& Ident, InstContainerIt& it)
{
	SHWSInstance* pInst = NULL;
	it = std::lower_bound(pInstCont->begin(), pInstCont->end(), identHash, InstContainerByHash());
	InstContainerIt itOther = it;
	while (it != pInstCont->end() && identHash == (*it)->m_Ident.m_nHash)
	{
		const SShaderCombIdent& other = (*it)->m_Ident;
		if (Ident == other)
		{
			pInst = *it;
			break;
		}
		++it;
	}

	if (!pInst && itOther != pInstCont->begin())
	{
		--itOther;
		while (identHash == (*itOther)->m_Ident.m_nHash)
		{
			const SShaderCombIdent& other = (*itOther)->m_Ident;
			if (Ident == other)
			{
				pInst = *itOther;
				break;
			}
			if (itOther == pInstCont->begin())
				break;
			--itOther;
		}
	}
	return pInst;
}

CHWShader_D3D::SHWSInstance* CHWShader_D3D::mfGetInstance(CShader* pSH, int nHashInstance, SShaderCombIdent& Ident)
{
	DETAILED_PROFILE_MARKER("mfGetInstance");
	FUNCTION_PROFILER_RENDER_FLAT
	InstContainer* pInstCont = &m_Insts;

	InstContainerIt it;
	SHWSInstance* pInst = mfGetHashInst(pInstCont, nHashInstance, Ident, it);

	return pInst;
}

CHWShader_D3D::SHWSInstance* CHWShader_D3D::mfGetInstance(CShader* pSH, SShaderCombIdent& Ident, uint32 nFlags)
{
	DETAILED_PROFILE_MARKER("mfGetInstance");
	FUNCTION_PROFILER_RENDER_FLAT
	SHWSInstance* pInst = m_pCurInst;
	if (pInst && !pInst->m_bFallback)
	{
		assert(pInst->m_eClass < eHWSC_Num);

		const SShaderCombIdent& other = pInst->m_Ident;
		// other will have been through PostCreate, and so won't have the platform mask set anymore
		if ((Ident.m_MDVMask & ~SF_PLATFORM) == other.m_MDVMask && Ident.m_RTMask == other.m_RTMask && Ident.m_GLMask == other.m_GLMask && Ident.m_FastCompare1 == other.m_FastCompare1 && Ident.m_pipelineState.opaque == other.m_pipelineState.opaque)
			return pInst;
	}
	InstContainer* pInstCont = &m_Insts;

	uint32 identHash = Ident.PostCreate();

	InstContainerIt it;
	pInst = mfGetHashInst(pInstCont, identHash, Ident, it);

	if (pInst == 0)
	{
		pInst = new SHWSInstance;
		pInst->m_nVertexFormat = 1;
		pInst->m_nCache = -1;
		s_nInstFrame++;
		pInst->m_Ident = Ident;
		pInst->m_eClass = m_eSHClass;
		size_t i = std::distance(pInstCont->begin(), it);
		pInstCont->insert(it, pInst);
		if (nFlags & HWSF_FAKE)
			pInst->m_Handle.SetFake();
	}
	m_pCurInst = pInst;
	return pInst;
}

//=================================================================================

void CHWShader_D3D::mfSetForOverdraw(SHWSInstance* pInst, uint32 nFlags, uint64& RTMask)
{
	if (mfIsValid(pInst, false) == ED3DShError_NotCompiled)
		mfActivate(gRenDev->m_RP.m_pShader, nFlags);
	RTMask |= g_HWSR_MaskBit[HWSR_DEBUG0] | g_HWSR_MaskBit[HWSR_DEBUG1] | g_HWSR_MaskBit[HWSR_DEBUG2] | g_HWSR_MaskBit[HWSR_DEBUG3];
	RTMask &= m_nMaskAnd_RT;
	RTMask |= m_nMaskOr_RT;
	CD3D9Renderer* rd = gcpRendD3D;
	if (CRenderer::CV_r_measureoverdraw == 1 && m_eSHClass == eHWSC_Pixel)
		rd->m_RP.m_NumShaderInstructions = pInst->m_nInstructions;
	else if (CRenderer::CV_r_measureoverdraw == 3 && m_eSHClass == eHWSC_Vertex)
		rd->m_RP.m_NumShaderInstructions = pInst->m_nInstructions;
	else if (CRenderer::CV_r_measureoverdraw == 2 || CRenderer::CV_r_measureoverdraw == 4)
		rd->m_RP.m_NumShaderInstructions = 30;
}

void CHWShader_D3D::ModifyLTMask(uint32& nMask)
{
	if (nMask)
	{
		if (!(m_Flags & (HWSG_SUPPORTS_MULTILIGHTS | HWSG_SUPPORTS_LIGHTING | HWSG_FP_EMULATION)))
			nMask = 0;
		else if (!(m_Flags & HWSG_SUPPORTS_MULTILIGHTS) && (m_Flags & HWSG_SUPPORTS_LIGHTING))
		{
			int nLightType = (nMask >> SLMF_LTYPE_SHIFT) & SLMF_TYPE_MASK;
			if (nLightType != SLMF_PROJECTED)
				nMask = 1;
		}
	}
}

bool CHWShader_D3D::mfSetVS(int nFlags)
{
	DETAILED_PROFILE_MARKER("mfSetVS");

	CD3D9Renderer* rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	SShaderCombIdent Ident;
	Ident.m_LightMask = rRP.m_FlagsShader_LT;
	Ident.m_RTMask = rRP.m_FlagsShader_RT & m_nMaskAnd_RT | m_nMaskOr_RT;
	Ident.m_MDMask = rRP.m_FlagsShader_MD;
	Ident.m_MDVMask = rRP.m_FlagsShader_MDV | CParserBin::m_nPlatform;
	Ident.m_GLMask = m_nMaskGenShader;
	Ident.m_pipelineState.opaque = gRenDev->m_RP.m_FlagsShader_PipelineState;
	/*if (RTMask == 0x40004 && !stricmp(m_EntryFunc.c_str(), "Common_ZPassVS"))
	   {
	   int nnn = 0;
	   }*/

	ModifyLTMask(Ident.m_LightMask);

	SHWSInstance* pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);

	if (CRenderer::CV_r_measureoverdraw == 3)
	{
		mfSetForOverdraw(pInst, nFlags, Ident.m_RTMask);
		pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);
	}

	pInst->m_fLastAccess = rTI.m_RealTime;

	if (!mfCheckActivation(rRP.m_pShader, pInst, nFlags))
	{
		s_pCurInstVS = NULL;
		s_pCurHWVS = NULL;
		s_nActivationFailMask |= (1 << eHWSC_Vertex);
		return false;
	}

	// Update vertex modificator if required
	if (CShaderResources* pSR = rRP.m_pShaderResources)
	{
		if (pSR->m_pDeformInfo)
			pSR->RT_UpdateConstants(rRP.m_pShader);
	}

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
	{
	#if defined(__GNUC__)
		rd->Logv("--- Set FX VShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%llx, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#else
		rd->Logv("--- Set FX VShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%I64x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#endif
	}
#endif
	if (m_nFrame != rTI.m_nFrameUpdateID)
	{
		m_nFrame = rTI.m_nFrameUpdateID;
#ifndef _RELEASE
		rRP.m_PS[rRP.m_nProcessThreadID].m_NumVShaders++;
		if (pInst->m_nInstructions > rRP.m_PS[rRP.m_nProcessThreadID].m_NumVSInstructions)
		{
			rRP.m_PS[rRP.m_nProcessThreadID].m_NumVSInstructions = pInst->m_nInstructions;
			rRP.m_PS[rRP.m_nProcessThreadID].m_pMaxVShader = this;
			rRP.m_PS[rRP.m_nProcessThreadID].m_pMaxVSInstance = pInst;
		}
#endif
	}
	if (!(nFlags & HWSF_PRECACHE))
	{
		if (s_pCurVS != pInst->m_Handle.m_pShader)
		{
			s_pCurVS = pInst->m_Handle.m_pShader;
#ifndef _RELEASE
			rRP.m_PS[rRP.m_nProcessThreadID].m_NumVShadChanges++;
#endif
			mfBind();
		}
		s_pCurHWVS = this;
		s_pCurInstVS = pInst;
		rRP.m_FlagsStreams_Decl = pInst->m_VStreamMask_Decl;
		rRP.m_FlagsStreams_Stream = pInst->m_VStreamMask_Stream;
		// Make sure we don't use any texture attributes except baseTC in instancing case
		if (nFlags & HWSF_INSTANCED)
		{
			rRP.m_FlagsStreams_Decl &= ~(VSM_MORPHBUDDY);
			rRP.m_FlagsStreams_Stream &= ~(VSM_MORPHBUDDY);
		}

		mfSetParametersPB();
	}
	if (nFlags & HWSF_SETTEXTURES)
		mfSetSamplers(pInst->m_pSamplers, m_eSHClass);

	s_nActivationFailMask &= ~(1 << eHWSC_Vertex);

	return true;
}

bool CHWShader_D3D::mfSetPS(int nFlags)
{
	DETAILED_PROFILE_MARKER("mfSetPS");

	CD3D9Renderer* rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	SShaderCombIdent Ident;
	Ident.m_LightMask = rRP.m_FlagsShader_LT;
	Ident.m_RTMask = rRP.m_FlagsShader_RT & m_nMaskAnd_RT | m_nMaskOr_RT;
	Ident.m_MDMask = rRP.m_FlagsShader_MD & ~HWMD_TEXCOORD_FLAG_MASK;
	Ident.m_MDVMask = CParserBin::m_nPlatform;
	Ident.m_GLMask = m_nMaskGenShader;
	Ident.m_pipelineState.opaque = gRenDev->m_RP.m_FlagsShader_PipelineState;

	ModifyLTMask(Ident.m_LightMask);

	SHWSInstance* pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);

	// Update texture modificator flags based on active samplers state
	if (nFlags & HWSF_SETTEXTURES)
	{
		int nResult = mfCheckActivation(rRP.m_pShader, pInst, nFlags);
		if (!nResult)
		{
			CHWShader_D3D::s_pCurInstPS = NULL;
			s_nActivationFailMask |= (1 << eHWSC_Pixel);
			return false;
		}
		mfUpdateSamplers(rRP.m_pShader);
		if ((rRP.m_FlagsShader_MD ^ Ident.m_MDMask) & ~HWMD_TEXCOORD_FLAG_MASK)
		{
			pInst->m_fLastAccess = rTI.m_RealTime;
			if (rd->m_nFrameSwapID != pInst->m_nUsedFrame)
			{
				pInst->m_nUsedFrame = rd->m_nFrameSwapID;
				pInst->m_nUsed++;
			}
			Ident.m_MDMask = rRP.m_FlagsShader_MD & ~HWMD_TEXCOORD_FLAG_MASK;
			Ident.m_MDVMask = CParserBin::m_nPlatform;
			pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);
		}
	}
	if (CRenderer::CV_r_measureoverdraw > 0 && CRenderer::CV_r_measureoverdraw < 5)
	{
		mfSetForOverdraw(pInst, nFlags, Ident.m_RTMask);
		pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);
	}
	pInst->m_fLastAccess = rTI.m_RealTime;

	if (!mfCheckActivation(rRP.m_pShader, pInst, nFlags))
	{
		s_pCurInstPS = NULL;
		s_nActivationFailMask |= (1 << eHWSC_Pixel);
		return false;
	}

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
	{
	#if defined(__GNUC__)
		rd->Logv("--- Set FX PShader \"%s\" (%d instr) LTMask: 0x%x, GLMask: 0x%llx, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask & 0x0fffffff, Ident.m_pipelineState.opaque);
	#else
		rd->Logv("--- Set FX PShader \"%s\" (%d instr) LTMask: 0x%x, GLMask: 0x%I64x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask & 0x0fffffff, Ident.m_pipelineState.opaque);
	#endif
	}
#endif

	if (m_nFrame != rTI.m_nFrameUpdateID)
	{
		m_nFrame = rTI.m_nFrameUpdateID;
#ifndef _RELEASE
		rRP.m_PS[rRP.m_nProcessThreadID].m_NumPShaders++;
		if (pInst->m_nInstructions > rRP.m_PS[rRP.m_nProcessThreadID].m_NumPSInstructions)
		{
			rRP.m_PS[rRP.m_nProcessThreadID].m_NumPSInstructions = pInst->m_nInstructions;
			rRP.m_PS[rRP.m_nProcessThreadID].m_pMaxPShader = this;
			rRP.m_PS[rRP.m_nProcessThreadID].m_pMaxPSInstance = pInst;
		}
#endif
	}
	if (!(nFlags & HWSF_PRECACHE))
	{
		if (s_pCurPS != pInst->m_Handle.m_pShader)
		{
			s_pCurPS = pInst->m_Handle.m_pShader;
#ifndef _RELEASE
			rRP.m_PS[rRP.m_nProcessThreadID].m_NumPShadChanges++;
#endif
			mfBind();
		}
		s_pCurInstPS = pInst;
		mfSetParametersPB();
		if (nFlags & HWSF_SETTEXTURES)
			mfSetSamplers(pInst->m_pSamplers, m_eSHClass);
	}

	s_nActivationFailMask &= ~(1 << eHWSC_Pixel);

	return true;
}

bool CHWShader_D3D::mfSetGS(int nFlags)
{
	DETAILED_PROFILE_MARKER("mfSetGS");

	CD3D9Renderer* rd = gcpRendD3D;
	if (m_Flags & HWSG_GS_MULTIRES)
	{
		if (!s_pCurInstVS || !CVrProjectionManager::IsMultiResEnabledStatic())
			return false;
	}

	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	SShaderCombIdent Ident;
	Ident.m_LightMask = rRP.m_FlagsShader_LT;
	Ident.m_RTMask = rRP.m_FlagsShader_RT & m_nMaskAnd_RT | m_nMaskOr_RT;
	Ident.m_MDMask = rRP.m_FlagsShader_MD;
	Ident.m_MDVMask = rRP.m_FlagsShader_MDV | CParserBin::m_nPlatform;
	Ident.m_GLMask = m_nMaskGenShader;
	Ident.m_pipelineState.opaque = gRenDev->m_RP.m_FlagsShader_PipelineState;

	ModifyLTMask(Ident.m_LightMask);

	SHWSInstance* pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);
	pInst->m_fLastAccess = rTI.m_RealTime;

	if (!mfCheckActivation(rRP.m_pShader, pInst, nFlags))
	{
		s_pCurInstGS = NULL;
		s_nActivationFailMask |= (1 << eHWSC_Geometry);
		return false;
	}

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
	{
	#if defined(__GNUC__)
		rd->Logv("--- Set FX GShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%llx, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#else
		rd->Logv("--- Set FX GShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%I64x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#endif
	}
#endif

	rRP.m_PersFlags2 |= s_bFirstGS * (RBPF2_COMMIT_PF | RBPF2_COMMIT_CM);
	rRP.m_nCommitFlags |= s_bFirstGS * (FC_GLOBAL_PARAMS);

	s_bFirstGS = false;
	s_pCurInstGS = pInst;

	if (!(nFlags & HWSF_PRECACHE))
	{
		mfBindGS(pInst->m_Handle.m_pShader, pInst->m_Handle.m_pShader->m_pHandle);
		mfSetParametersPB();
	}

	s_nActivationFailMask &= ~(1 << eHWSC_Geometry);

	return true;
}

bool CHWShader_D3D::mfSetHS(int nFlags)
{
	DETAILED_PROFILE_MARKER("mfSetHS");

	CD3D9Renderer* rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	SShaderCombIdent Ident;
	Ident.m_LightMask = rRP.m_FlagsShader_LT;
	Ident.m_RTMask = rRP.m_FlagsShader_RT & m_nMaskAnd_RT | m_nMaskOr_RT;
	Ident.m_MDMask = rRP.m_FlagsShader_MD;
	Ident.m_MDVMask = rRP.m_FlagsShader_MDV | CParserBin::m_nPlatform;
	Ident.m_GLMask = m_nMaskGenShader;
	Ident.m_pipelineState.opaque = gRenDev->m_RP.m_FlagsShader_PipelineState;

	ModifyLTMask(Ident.m_LightMask);

	SHWSInstance* pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);
	pInst->m_fLastAccess = rTI.m_RealTime;

	if (!mfCheckActivation(rRP.m_pShader, pInst, nFlags))
	{
		s_pCurInstHS = NULL;
		s_nActivationFailMask |= (1 << eHWSC_Hull);
		return false;
	}

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
	{
	#if defined(__GNUC__)
		rd->Logv("--- Set FX HShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%llx, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#else
		rd->Logv("--- Set FX HShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%I64x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#endif
	}
#endif

	rRP.m_PersFlags2 |= s_bFirstHS * (RBPF2_COMMIT_PF | RBPF2_COMMIT_CM);
	rRP.m_nCommitFlags |= s_bFirstHS * (FC_GLOBAL_PARAMS);

	s_bFirstHS = false;
	s_pCurInstHS = pInst;

	if (!(nFlags & HWSF_PRECACHE))
	{
#if CRY_PLATFORM_DURANGO
		if (pInst->m_Handle.m_pShader == NULL)
		{
			gEnv->pLog->Log("NULL shader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%I64x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask);
			return false;
		}
#endif

		mfBindHS(pInst->m_Handle.m_pShader, pInst->m_Handle.m_pShader->m_pHandle);
		mfSetParametersPB();
	}

	if (nFlags & HWSF_SETTEXTURES)
		mfSetSamplers(pInst->m_pSamplers, m_eSHClass);

	s_nActivationFailMask &= ~(1 << eHWSC_Hull);

	return true;
}

bool CHWShader_D3D::mfSetDS(int nFlags)
{
	DETAILED_PROFILE_MARKER("mfSetDS");

	CD3D9Renderer* rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	SShaderCombIdent Ident;
	Ident.m_LightMask = rRP.m_FlagsShader_LT;
	Ident.m_RTMask = rRP.m_FlagsShader_RT & m_nMaskAnd_RT | m_nMaskOr_RT;
	Ident.m_MDMask = rRP.m_FlagsShader_MD;
	Ident.m_MDVMask = rRP.m_FlagsShader_MDV | CParserBin::m_nPlatform;
	Ident.m_GLMask = m_nMaskGenShader;
	Ident.m_pipelineState.opaque = gRenDev->m_RP.m_FlagsShader_PipelineState;

	ModifyLTMask(Ident.m_LightMask);

	SHWSInstance* pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);
	pInst->m_fLastAccess = rTI.m_RealTime;

	if (!mfCheckActivation(rRP.m_pShader, pInst, nFlags))
	{
		s_pCurInstDS = NULL;
		s_nActivationFailMask |= (1 << eHWSC_Domain);
		return false;
	}

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
	{
	#if defined(__GNUC__)
		rd->Logv("--- Set FX DShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%llx, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#else
		rd->Logv("--- Set FX DShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%I64x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#endif
	}
#endif

	rRP.m_PersFlags2 |= s_bFirstDS * (RBPF2_COMMIT_PF | RBPF2_COMMIT_CM);
	rRP.m_nCommitFlags |= s_bFirstDS * (FC_GLOBAL_PARAMS);

	s_bFirstDS = false;
	s_pCurInstDS = pInst;

	if (!(nFlags & HWSF_PRECACHE))
	{
#if CRY_PLATFORM_DURANGO
		if (!pInst->m_Handle.m_pShader)
		{
			gEnv->pLog->Log("NULL shader2 \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%llx, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask);
			return false;
		}
#endif

		mfBindDS(pInst->m_Handle.m_pShader, pInst->m_Handle.m_pShader->m_pHandle);
		mfSetParametersPB();
	}

	if (nFlags & HWSF_SETTEXTURES)
		mfSetSamplers(pInst->m_pSamplers, m_eSHClass);

	s_nActivationFailMask &= ~(1 << eHWSC_Domain);

	return true;
}

bool CHWShader_D3D::mfSetCS(int nFlags)
{
	DETAILED_PROFILE_MARKER("mfSetCS");

	CD3D9Renderer* rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	SShaderCombIdent Ident;
	Ident.m_LightMask = rRP.m_FlagsShader_LT;
	Ident.m_RTMask = rRP.m_FlagsShader_RT & m_nMaskAnd_RT | m_nMaskOr_RT;
	Ident.m_MDMask = rRP.m_FlagsShader_MD;
	Ident.m_MDVMask = rRP.m_FlagsShader_MDV | CParserBin::m_nPlatform;
	Ident.m_GLMask = m_nMaskGenShader;
	Ident.m_pipelineState.opaque = gRenDev->m_RP.m_FlagsShader_PipelineState;

	ModifyLTMask(Ident.m_LightMask);

	SHWSInstance* pInst = mfGetInstance(rRP.m_pShader, Ident, nFlags);
	pInst->m_fLastAccess = rTI.m_RealTime;

	if (!mfCheckActivation(rRP.m_pShader, pInst, nFlags))
	{
		s_pCurInstCS = NULL;
		s_nActivationFailMask |= (1 << eHWSC_Compute);
		return false;
	}

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
	{
	#if defined(__GNUC__)
		rd->Logv("--- Set FX CShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%llx, RTMask: 0x%llx, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#else
		rd->Logv("--- Set FX CShader \"%s\" (%d instr), LTMask: 0x%x, GLMask: 0x%I64x, RTMask: 0x%I64x, MDMask: 0x%x, MDVMask: 0x%x, PSS: 0x%llx\n", GetName(), pInst->m_nInstructions, Ident.m_LightMask, Ident.m_GLMask, Ident.m_RTMask, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque);
	#endif
	}
#endif

	rRP.m_PersFlags2 |= s_bFirstCS * (RBPF2_COMMIT_PF | RBPF2_COMMIT_CM);
	rRP.m_nCommitFlags |= s_bFirstCS * (FC_GLOBAL_PARAMS);

	s_bFirstCS = false;
	s_pCurInstCS = pInst;

	if (!(nFlags & HWSF_PRECACHE))
	{
		mfBindCS(pInst->m_Handle.m_pShader, pInst->m_Handle.m_pShader->m_pHandle);

		mfSetParametersPB();
	}

	if (nFlags & HWSF_SETTEXTURES)
		mfSetSamplers(pInst->m_pSamplers, m_eSHClass);

	s_nActivationFailMask = 0;  // Reset entire mask since CS does not need any other shader stages

	return true;
}

//=======================================================================

void CHWShader_D3D::mfSetCM()
{
	DETAILED_PROFILE_MARKER("mfSetCM");

	for (EHWShaderClass eClass = eHWSC_Vertex; eClass < eHWSC_Num; eClass = EHWShaderClass(eClass + 1))
	{
		if (!s_CM_Params[eClass].empty())
		{
			mfSetParameters(&s_CM_Params[eClass][0], s_CM_Params[eClass].size(), eClass, s_nMax_PF_Vecs[eClass]);
		}
	}
}

void CHWShader_D3D::mfSetPV(const D3DViewPort* pCustomViewport)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;

	assert(pCustomViewport == nullptr);
	
	CConstantBufferPtr pPerViewCB = rd->GetGraphicsPipeline().GetMainViewConstantBuffer();

	CConstantBuffer* pBuffer = pPerViewCB ? pPerViewCB.get() : NULL;
	rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, pBuffer, 13);
	rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_PS, pBuffer, 13);
#if !defined(OPENGL) || DXGL_ENABLE_GEOMETRY_SHADERS
	rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_GS, pBuffer, 13);
#endif

#if !defined(OPENGL) || DXGL_ENABLE_TESSELLATION_SHADERS
	rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_HS, pBuffer, 13);
	rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_DS, pBuffer, 13);
#endif

#if !defined(OPENGL) || DXGL_ENABLE_COMPUTE_SHADERS
	rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_CS, pBuffer, 13);
#endif
}

void CHWShader_D3D::mfSetGlobalParams()
{
	DETAILED_PROFILE_MARKER("mfSetGlobalParams");
#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
		gRenDev->Logv("--- Set global shader constants...\n");
#endif

	Vec4 v;
	CD3D9Renderer* r = gcpRendD3D;

	// Preallocate global constant buffer arrays
	if (!s_pCB[eHWSC_Vertex][CB_PER_BATCH])
	{
		int i, j;
		for (i = 0; i < CB_NUM; i++)
		{
			for (j = 0; j < eHWSC_Num; j++)
			{
				int nSize = 0;
				switch (j)
				{
				case eHWSC_Pixel:
					nSize = MAX_CONSTANTS_PS;
					break;
				case eHWSC_Vertex:
					nSize = MAX_CONSTANTS_VS;
					break;
				case eHWSC_Geometry:
					nSize = MAX_CONSTANTS_GS;
					break;
				case eHWSC_Domain:
					nSize = MAX_CONSTANTS_DS;
					break;
				case eHWSC_Hull:
					nSize = MAX_CONSTANTS_HS;
					break;
				case eHWSC_Compute:
					nSize = MAX_CONSTANTS_CS;
					break;
				default:
					assert(0);
					break;
				}

				if (nSize)
				{
#ifdef USE_PER_FRAME_CONSTANT_BUFFER_UPDATES
					s_pCB[j][i] = new D3DBuffer*[nSize];
					memset(s_pCB[j][i], 0, sizeof(D3DBuffer*) * (nSize));
#else
					s_pCB[j][i] = new CConstantBuffer*[nSize];
					memset(s_pCB[j][i], 0, sizeof(CConstantBuffer*) * (nSize));
#endif
				}
			}
		}
	}

	r->m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF | RBPF2_COMMIT_CM;
	r->m_RP.m_nCommitFlags |= FC_GLOBAL_PARAMS;
}

void CHWShader_D3D::mfSetCameraParams()
{
	DETAILED_PROFILE_MARKER("mfSetCameraParams");
#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
		gRenDev->Logv("--- Set camera shader constants...\n");
#endif
	gRenDev->m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF | RBPF2_COMMIT_CM;
	gRenDev->m_RP.m_nCommitFlags |= FC_GLOBAL_PARAMS;
}

bool CHWShader_D3D::mfAddGlobalTexture(SCGTexture& Texture)
{
	DETAILED_PROFILE_MARKER("mfAddGlobalTexture");
	uint32 i;
	if (!Texture.m_bGlobal)
	{
		return false;
	}
	for (i = 0; i < s_PF_Textures.size(); i++)
	{
		SCGTexture* pP = &s_PF_Textures[i];
		if (pP->m_pTexture == Texture.m_pTexture)
		{
			break;
		}
	}
	if (i == s_PF_Textures.size())
	{
		s_PF_Textures.push_back(Texture);
		assert(s_PF_Textures.size() <= MAX_PF_TEXTURES);
		return true;
	}
	return false;
}

bool CHWShader_D3D::mfAddGlobalSampler(STexSamplerRT& Sampler)
{
	DETAILED_PROFILE_MARKER("mfAddGlobalSampler");
	uint32 i;
	if (!Sampler.m_bGlobal)
	{
		return false;
	}
	for (i = 0; i < s_PF_Samplers.size(); i++)
	{
		STexSamplerRT* pP = &s_PF_Samplers[i];
		if (pP->m_pTex == Sampler.m_pTex)
		{
			break;
		}
	}
	if (i == s_PF_Samplers.size())
	{
		s_PF_Samplers.push_back(Sampler);
		assert(s_PF_Samplers.size() <= MAX_PF_SAMPLERS);
		return true;
	}
	return false;
}

void CHWShader_D3D::mfUpdatePreprocessFlags(SShaderTechnique* pTech)
{
	DETAILED_PROFILE_MARKER("mfUpdatePreprocessFlags");
	uint32 nFlags = 0;

	for (uint32 i = 0; i < (uint32)m_Insts.size(); i++)
	{
		SHWSInstance* pInst = m_Insts[i];
		if (pInst->m_pSamplers.size())
		{
			for (uint32 j = 0; j < (uint32)pInst->m_pSamplers.size(); j++)
			{
				STexSamplerRT* pSamp = &pInst->m_pSamplers[j];
				if (pSamp && pSamp->m_pTarget)
				{
					SHRenderTarget* pTarg = pSamp->m_pTarget;
					if (pTarg->m_eOrder == eRO_PreProcess)
						nFlags |= pTarg->m_nProcessFlags;
					if (pTech)
					{
						uint32 n = 0;
						for (n = 0; n < pTech->m_RTargets.Num(); n++)
						{
							if (pTarg == pTech->m_RTargets[n])
								break;
						}
						if (n == pTech->m_RTargets.Num())
							pTech->m_RTargets.AddElem(pTarg);
					}
				}
			}
		}
	}
	if (pTech)
	{
		pTech->m_RTargets.Shrink();
		pTech->m_nPreprocessFlags |= nFlags;
	}
}

Vec4 CHWShader_D3D::GetVolumetricFogParams(const CRenderCamera& rcam)
{
	DETAILED_PROFILE_MARKER("GetVolumetricFogParams");
	return sGetVolumetricFogParams(rcam);
}

Vec4 CHWShader_D3D::GetVolumetricFogRampParams()
{
	DETAILED_PROFILE_MARKER("GetVolumetricFogRampParams");
	return sGetVolumetricFogRampParams();
}

Vec4 CHWShader_D3D::GetVolumetricFogSunDir(const Vec3& sunDir)
{
	DETAILED_PROFILE_MARKER("GetVolumetricFogSunDir");
	return sGetVolumetricFogSunDir(sunDir);
}

void CHWShader_D3D::GetFogColorGradientConstants(Vec4& fogColGradColBase, Vec4& fogColGradColDelta)
{
	DETAILED_PROFILE_MARKER("GetFogColorGradientConstants");
	sGetFogColorGradientConstants(fogColGradColBase, fogColGradColDelta);
};

Vec4 CHWShader_D3D::GetFogColorGradientRadial(const CRenderCamera& rcam)
{
	DETAILED_PROFILE_MARKER("GetFogColorGradientRadial");
	return sGetFogColorGradientRadial(rcam);
}

