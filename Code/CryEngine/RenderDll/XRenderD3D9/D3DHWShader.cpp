// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

#include <string>

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning(push)
	#pragma warning(disable: 4244)
#endif

int CHWShader_D3D::s_nActivationFailMask = 0;

bool CHWShader_D3D::s_bInitShaders = true;

int CHWShader_D3D::s_nResetDeviceFrame = -1;
int CHWShader_D3D::s_nInstFrame = -1;

CHWShader* CHWShader::s_pCurHWVS;
char *CHWShader::s_GS_MultiRes_NV;

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

SD3DShader::~SD3DShader() noexcept
{
	if (m_eSHClass == eHWSC_Pixel)
		CHWShader_D3D::s_nDevicePSDataSize -= m_nSize;
	else
		CHWShader_D3D::s_nDeviceVSDataSize -= m_nSize;
	SAFE_RELEASE(m_pHandle);
}

void SHWShaderCache::GetMemoryUsage(ICrySizer* pSizer) const
{
	for (const auto &s : m_shaders)
	{
		if (stl::holds_alternative<SDeviceShaderEntry>(s.second))
			pSizer->AddObject(&stl::get<SDeviceShaderEntry>(s.second));
	}
	if (m_pDiskShaderCache[0])
		pSizer->AddObject(m_pDiskShaderCache[0]);
	if (m_pDiskShaderCache[1])
		pSizer->AddObject(m_pDiskShaderCache[1]);
}

void CHWShader_D3D::SHWSInstance::Release()
{
	if (m_nParams[0] >= 0)
		CGParamManager::FreeParametersGroup(m_nParams[0]);
	if (m_nParams[1] >= 0)
		CGParamManager::FreeParametersGroup(m_nParams[1]);
	if (m_nParams_Inst >= 0)
		CGParamManager::FreeParametersGroup(m_nParams_Inst);
	
	m_Handle.m_pShader = nullptr;

	if (m_Shader.m_pShaderData)
	{
		delete[] (char*)m_Shader.m_pShaderData;
		m_Shader.m_pShaderData = nullptr;
	}
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


void CHWShader_D3D::ShutDown()
{
	uint32 numResourceLeaks = 0;

	// First make sure all HW and FX shaders are released
	EHWShaderClass classes[] = {
		eHWSC_Vertex,
		eHWSC_Pixel,
		eHWSC_Geometry,
		eHWSC_Domain,
		eHWSC_Hull,
		eHWSC_Compute
	};
	for (const auto &c : classes)
	{
		const CCryNameTSCRC Name = CHWShader::mfGetClassName(c);
		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(Name);
		if (pRL)
		{
			for (const auto &s : pRL->m_RMap)
			{
				if (s.second)
					++numResourceLeaks;
			}
			pRL->m_RList.clear();
			pRL->m_AvailableIDs.clear();
		}
	}

	const CCryNameTSCRC Name = CShader::mfGetClassName();
	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(Name);
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

	gRenDev->m_cEF.m_Bin.mfReleaseFXParams();

	CGParamManager::Shutdown();

}

CHWShader::~CHWShader() noexcept
{
	if (m_pCache)
		m_pCache->Release();
}

CCryNameTSCRC CHWShader::mfGetClassName(EHWShaderClass eClass)
{
	static const auto s_sClassNameVertex = CCryNameTSCRC("CHWShader_VS");
	static const auto s_sClassNamePixel = CCryNameTSCRC("CHWShader_PS");
	static const auto s_sClassNameGeometry = CCryNameTSCRC("CHWShader_GS");
	static const auto s_sClassNameDomain = CCryNameTSCRC("CHWShader_DS");
	static const auto s_sClassNameHull = CCryNameTSCRC("CHWShader_HS");
	static const auto s_sClassNameCompute = CCryNameTSCRC("CHWShader_CS");

	switch (eClass)
	{
	case eHWSC_Vertex:
		return s_sClassNameVertex;
	case eHWSC_Pixel:
		return s_sClassNamePixel;
	case eHWSC_Geometry:
		return s_sClassNameGeometry;
	case eHWSC_Domain:
		return s_sClassNameDomain;
	case eHWSC_Hull:
		return s_sClassNameHull;
	default:
		CRY_ASSERT("WTF");
	case eHWSC_Compute:
		return s_sClassNameCompute;
	}
}

CCryNameTSCRC CHWShader::mfGetCacheClassName(EHWShaderClass eClass)
{
	static const auto s_sClassNameVertex = CCryNameTSCRC("CHWShader_cache_VS");
	static const auto s_sClassNamePixel = CCryNameTSCRC("CHWShader_cache_PS");
	static const auto s_sClassNameGeometry = CCryNameTSCRC("CHWShader_cache_GS");
	static const auto s_sClassNameDomain = CCryNameTSCRC("CHWShader_cache_DS");
	static const auto s_sClassNameHull = CCryNameTSCRC("CHWShader_cache_HS");
	static const auto s_sClassNameCompute = CCryNameTSCRC("CHWShader_cache_CS");

	switch (eClass)
	{
	case eHWSC_Vertex:
		return s_sClassNameVertex;
	case eHWSC_Pixel:
		return s_sClassNamePixel;
	case eHWSC_Geometry:
		return s_sClassNameGeometry;
	case eHWSC_Domain:
		return s_sClassNameDomain;
	case eHWSC_Hull:
		return s_sClassNameHull;
	default:
		CRY_ASSERT("WTF");
	case eHWSC_Compute:
		return s_sClassNameCompute;
	}
}

CHWShader* CHWShader::mfForName(const char* name, const char* nameSource, uint32 CRC32, const char* szEntryFunc, EHWShaderClass eClass, const TArray<uint32>& SHData, const FXShaderToken& Table, uint32 dwType, CShader* pFX, uint64 nMaskGen, uint64 nMaskGenFX)
{
	//	LOADING_TIME_PROFILE_SECTION(iSystem);
	if (!name || !name[0])
		return nullptr;

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Shader, 0, "%s", name);

	CHWShader_D3D* pSH = nullptr;
	stack_string strName = name;
	stack_string AddStr;
	const auto className = mfGetClassName(eClass);

	if (CParserBin::m_nPlatform == SF_ORBIS)
		strName += AddStr.Format("(O)");
	else if (CParserBin::m_nPlatform == SF_DURANGO)
		strName += AddStr.Format("(D)");
	else if (CParserBin::m_nPlatform == SF_D3D11)
		strName += AddStr.Format("(X1)");
	else if (CParserBin::m_nPlatform == SF_GL4)
		strName + AddStr.Format("(G4)");
	else if (CParserBin::m_nPlatform == SF_GLES3)
		strName + AddStr.Format("(E3)");

	const auto cacheClassName = mfGetCacheClassName(eClass);
	const string cacheName = strName;
	const auto cacheNameCrc = CCryNameTSCRC{ cacheName };

	if (nMaskGen)
	{
#ifdef __GNUC__
		strName += AddStr.Format("(%llx)", nMaskGen);
#else
		strName += AddStr.Format("(%I64x)", nMaskGen);
#endif
	}
	const CCryNameTSCRC Name = strName.c_str();

	CBaseResource* pBR = CBaseResource::GetResource(className, Name, true);
	if (!pBR)
	{
		pSH = new CHWShader_D3D;
		pSH->m_Name = strName.c_str();
		pSH->m_NameSourceFX = nameSource;
		pSH->Register(className, Name);
		pSH->mfFree();

		pSH->m_EntryFunc = szEntryFunc;
		pSH->m_CRC32 = CRC32;
		pSH->m_eSHClass = eClass;

		// Acquire cache resource
		auto* hwSharedCache = CBaseResource::GetResource(cacheClassName, cacheNameCrc, true);
		if (!hwSharedCache)
		{
			char dstName[256];
			pSH->mfGetDstFileName(nullptr, dstName, 256, 0);

			hwSharedCache = new SHWShaderCache(string{ dstName });
			hwSharedCache->Register(cacheClassName, cacheNameCrc);
		}
		pSH->m_pCache = static_cast<SHWShaderCache*>(hwSharedCache);
	}
	else
	{
		pSH = static_cast<CHWShader_D3D*>(pBR);
		if (pSH->m_CRC32 == CRC32)
		{
			if (CRenderer::CV_r_shadersAllowCompilation)
			{
				if (SHData.size())
				{
					char strName[256];
#if defined(__GNUC__)
					cry_sprintf(strName, "$MAP_%llx", pSH->m_nMaskGenShader);
#else
					cry_sprintf(strName, "$MAP_%I64x", pSH->m_nMaskGenShader);
#endif
					pSH->mfStoreCacheTokenMap(Table, SHData);
				}
			}
			return pSH;
		}

		// CRC mismatch
		pSH->m_pCache->Reset();

		pSH->mfFree();
		pSH->m_CRC32 = CRC32;
		pSH->m_eSHClass = eClass;
	}

	if (CParserBin::m_bEditable || (CVrProjectionManager::IsMultiResEnabledStatic() && eClass == eHWSC_Vertex))
	{
		pSH->m_TokenTable = Table;
		pSH->m_TokenData = SHData;
	}

	pSH->m_dwShaderType = dwType;
	pSH->m_nMaskGenShader = nMaskGen;
	pSH->m_nMaskGenFX = nMaskGenFX;

	pSH->mfWarmupCache(pFX);

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
			Parser.Preprocess(0, pBinGS->m_Tokens, pBinGS->m_TokenTable);
			pSH->m_TokenTable = Parser.m_TokenTable;
			pSH->m_TokenData.Copy(&Parser.m_Tokens[0], Parser.m_Tokens.size());
		}
	}

	pSH->mfConstructFX(Table, SHData);

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

uint64 CHWShader_D3D::CheckIfExpr_r(const uint32* pTokens, uint32& nCur, uint32 nSize)
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

void CHWShader_D3D::mfConstructFX_Mask_RT(const TArray<uint32>& SHData)
{
	assert(gRenDev->m_cEF.m_pGlobalExt);
	m_nMaskAnd_RT = 0;
	m_nMaskOr_RT = 0;
	if (!gRenDev->m_cEF.m_pGlobalExt)
		return;
	SShaderGen* pGen = gRenDev->m_cEF.m_pGlobalExt;

	assert(!SHData.empty());
	const uint32* pTokens = &SHData[0];
	uint32 nSize = SHData.size();
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

void CHWShader_D3D::mfConstructFX(const FXShaderToken& Table, const TArray<uint32>& SHData)
{
	if (!strnicmp(m_EntryFunc.c_str(), "Sync_", 5))
		m_Flags |= HWSG_SYNC;

	if (!SHData.empty())
		mfConstructFX_Mask_RT(SHData);
	else
	{
		m_nMaskAnd_RT = -1;
		m_nMaskOr_RT = 0;
	}

	if (Table.size() && CRenderer::CV_r_shadersAllowCompilation)
	{
		if (SHData.size())
		{
			char strName[256];
#if defined(__GNUC__)
			cry_sprintf(strName, "$MAP_%llx", m_nMaskGenShader);
#else
			cry_sprintf(strName, "$MAP_%I64x", m_nMaskGenShader);
#endif
			mfStoreCacheTokenMap(Table, SHData);
		}
	}
}

CHWShader_D3D::cacheValidationResult CHWShader_D3D::mfValidateCache(const SDiskShaderCache &cache)
{
	static constexpr auto fVersion = static_cast<float>(FX_CACHE_VER);
	const auto cacheType = cache.GetType();

	const SResFileLookupData* pLookup = cache.m_pRes->GetLookupData();
	if (!pLookup)
		return cacheValidationResult::no_lookup;
	if (pLookup->m_CacheMajorVer != (int)fVersion || pLookup->m_CacheMinorVer != (int)(((float)fVersion - (float)(int)fVersion) * 10.1f))
		return cacheValidationResult::version_mismatch;
	if (pLookup->m_CRC32 != m_CRC32)
		return cacheValidationResult::checksum_mismatch;

	return cacheValidationResult::ok;
}

bool CHWShader_D3D::mfWarmupCache(CShader* pFX)
{ 
	static constexpr auto fVersion = static_cast<float>(FX_CACHE_VER);

	std::vector<cacheSource> cacheTypes = { cacheSource::readonly };
	if (CRendererCVars::CV_r_shadersAllowCompilation)
		cacheTypes = { cacheSource::user, cacheSource::readonly };
	if (CRendererCVars::CV_r_shadersediting || gRenDev->IsShaderCacheGenMode())
		cacheTypes = { cacheSource::user };

 	for (const auto &cacheType : cacheTypes)
	{
		auto cache = AcquireDiskCache(cacheType);

		// Validate version and crc
		bool validCache = !!cache->m_pRes;
		if (validCache)
		{
			const auto validationResult = mfValidateCache(*cache);

			validCache = validationResult == cacheValidationResult::ok;

			std::string error;
			switch (validationResult)
			{
			case cacheValidationResult::no_lookup:
				error = std::string("WARNING: Shader cache '") + cache->m_pRes->mfGetFileName() + "' does not have lookup data!";
				break;
			case cacheValidationResult::version_mismatch:
				error = std::string("WARNING: Shader cache '") + cache->m_pRes->mfGetFileName() + "' version mismatch";
				break;
			case cacheValidationResult::checksum_mismatch:
				error = std::string("WARNING: Shader cache '") + cache->m_pRes->mfGetFileName() + "' CRC mismatch";
				break;
			default: {}
			}

			// Output error only for readonly cache
			if (cacheType == cacheSource::readonly && !validCache)
				LogWarningEngineOnly(error.c_str());
		}

		if (cacheType == cacheSource::user && !validCache)
		{
			// Recreate user cache
			cache = 
				(m_pCache->m_pDiskShaderCache[static_cast<int>(cacheType)] = stl::make_unique<SDiskShaderCache>(SDiskShaderCache::recreateUserCacheTag{}, m_pCache->GetName().c_str(), m_CRC32, fVersion)).get();
			validCache = cache && cache->m_pRes;
		}

		if (!validCache)
			continue;

		if (CRendererCVars::CV_r_ShadersCachePrecacheAll && gcpRendD3D->m_cEF.m_nCombinationsProcess <= 0)
		{
			CResFileOpenScope rfOpenGuard(cache->m_pRes);
			const auto openResult = rfOpenGuard.open(RA_READ | (CParserBin::m_bEndians ? RA_ENDIANS : 0), &gRenDev->m_cEF.m_ResLookupDataMan[static_cast<int>(cacheType)], nullptr);
			if (!openResult)
			{
				CryLog("CHWShader_D3D::mfWarmupCache(): Failed to open cache '%s'", rfOpenGuard.getHandle()->mfGetFileName());
				continue;
			}

			// Precache combinations
			mfPrecacheAllCombinations(pFX, rfOpenGuard, *cache);
		}
	}

	return true;
}

SDeviceShaderEntry CHWShader_D3D::mfShaderEntryFromCache(CShader* pFX, const CDirEntry& de, CResFileOpenScope &rfOpenGuard, SDiskShaderCache &cache)
{
	const auto offset = de.GetOffset();
	const auto size = de.GetSize();
	const auto name = de.GetName().get();

	// Decompress from library
	auto decompressedDataAndSize = cache.DecompressResource(
		rfOpenGuard,
		offset,
		size);

	// Read resource
	const std::unique_ptr<byte[]> pData = std::move(decompressedDataAndSize.first);
	const int32 dataSize = static_cast<int32>(decompressedDataAndSize.second);
	auto cacheItemHeader = pData ? *reinterpret_cast<const SShaderCacheHeaderItem*>(pData.get()) : SShaderCacheHeaderItem{};
	CRY_ASSERT(dataSize && pData);
	// Bad bad shader (invalid size, data or uncompiled stub entry)
	if (dataSize <= sizeof(SShaderCacheHeaderItem) || !pData || cacheItemHeader.m_Class == 255)
		return {};

	CRY_ASSERT(this->m_eSHClass == cacheItemHeader.m_Class);

	// Create fake instance
	SHWSInstance instance = {};
	instance.m_DeviceObjectID = name;
	instance.m_eClass = this->m_eSHClass;
	instance.m_nVertexFormat = cacheItemHeader.m_nVertexFormat;
	instance.m_nInstructions = cacheItemHeader.m_nInstructions;
	instance.m_VStreamMask_Decl = cacheItemHeader.m_StreamMask_Decl;
	instance.m_VStreamMask_Stream = cacheItemHeader.m_StreamMask_Stream;

	std::vector<SCGBind> bindsFromCache;
	const byte* pShaderData = pData.get() + sizeof(SShaderCacheHeaderItem);
	const byte* pBuf = mfBindsFromCache(bindsFromCache, cacheItemHeader.m_nInstBinds, pShaderData);
	const auto nSize = dataSize - sizeof(SShaderCacheHeaderItem) - std::distance(pBuf, pShaderData);

	// Store whole binary for vertex shaders and other vertex properties, as they are special that way (and need the binary for generating input layout).
	std::unique_ptr<byte[]> pVertexShaderBinary;
#if CRY_RENDERER_VULKAN
	std::vector<SVertexInputStream>  VSInputStreams;
#endif
	std::vector<SCGBind> bindVars;

	if (!gRenDev->IsShaderCacheGenMode())
	{
		// Upload to device
		if (!mfUploadHW(&instance, pBuf, nSize, pFX, 0))
		{
			CRY_ASSERT_MESSAGE(false, "CHWShader_D3D::mfShaderEntryFromCache(): mfUploadHW() failed!");
			return {};
		}

		// Reflect binds
		void* pShaderReflBuf = nullptr;
		HRESULT hr = D3DReflect(pBuf, nSize, IID_ID3D11ShaderReflection, &pShaderReflBuf);
		ID3D11ShaderReflection* pShaderReflection = (ID3D11ShaderReflection*)pShaderReflBuf;
		if (hr != S_OK)
		{
			CRY_ASSERT_MESSAGE(false, "CHWShader_D3D::mfShaderEntryFromCache(): D3DReflect() failed!");
			return {};
		}
		if (pShaderReflection)
			mfCreateBinds(bindVars, pShaderReflection, (std::size_t)nSize);

		// Vertex shenanigans
		if (instance.m_eClass == EHWShaderClass::eHWSC_Vertex)
		{
			pVertexShaderBinary = std::unique_ptr<byte[]>{ new byte[nSize] };
			std::memcpy(pVertexShaderBinary.get(), pBuf, nSize);

#if CRY_PLATFORM_ORBIS || CRY_PLATFORM_DURANGO || CRY_RENDERER_VULKAN
			D3DBlob* pS = NULL;
			D3DCreateBlob(nSize, (D3DBlob**)&pS);
			DWORD* pBuffer = (DWORD*)pS->GetBufferPointer();
			memcpy(pBuffer, pBuf, nSize);
			mfVertexFormat(&instance, this, pS, nullptr);
			SAFE_RELEASE(pS);

			// Override header vertex format and stream stuff
			cacheItemHeader.m_nVertexFormat = instance.m_nVertexFormat;
			cacheItemHeader.m_StreamMask_Stream = instance.m_VStreamMask_Stream;
			cacheItemHeader.m_StreamMask_Decl = instance.m_VStreamMask_Decl;
#if CRY_RENDERER_VULKAN
			VSInputStreams = std::move(instance.m_VSInputStreams);
#endif
#endif
		}
	}

	const auto vertexShaderBinarySize = static_cast<std::size_t>(pVertexShaderBinary ? nSize : 0);
	return SDeviceShaderEntry
	{
		cacheItemHeader,
		std::move(bindVars),
		std::move(pVertexShaderBinary),
		vertexShaderBinarySize,
#if CRY_RENDERER_VULKAN
		std::move(VSInputStreams),
#endif
		std::move(instance.m_Handle.m_pShader) 
	};
}

void CHWShader_D3D::mfPrecacheAllCombinations(CShader* pFX, CResFileOpenScope &rfOpenGuard, SDiskShaderCache &cache)
{
#if CRY_RENDERER_VULKAN || CRY_RENDERER_GNM
	return;
#endif

	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CHWShader_D3D::mfPrecacheAllCombinations()");

	std::vector<std::pair<size_t, const SDeviceShaderEntry*>> entries;
	const auto entries_pred = [](const typename decltype(entries)::value_type &lhs, std::size_t rhs) noexcept { return std::get<0>(lhs) < rhs; };
	// Vector of duplicated entries (offset and name)
	std::vector<std::pair<size_t, SHWShaderCache::deviceShaderCacheKey>> duplicates;

	// Log
	CryLog("---Shader Cache: mfPrecacheAllCombinations() %s", rfOpenGuard.getHandle()->mfGetFileName());

	// Go over all entries
	auto &devCache = GetDevCache();
	for (const auto& shaderEntry : *rfOpenGuard.getHandle()->mfGetDirectory())
	{
		const auto offset = shaderEntry.GetOffset();
		const auto name = shaderEntry.GetName().get();
		const auto devCacheKey = static_cast<SHWShaderCache::deviceShaderCacheKey>(name);

		// Already exists or invalid
		if (!shaderEntry.IsValid() || 
			(shaderEntry.GetFlags() & (RF_RES_$TOKENS | RF_RES_$)))
			continue;

		// Store duplicates for later and continue
		if (shaderEntry.IsDuplicate())
		{
			duplicates.emplace_back(offset, devCacheKey);
			continue;
		}

		// Find where this goes into the entries list
		auto it = std::lower_bound(entries.begin(), entries.end(), offset, entries_pred);
		CRY_ASSERT_MESSAGE(it == entries.end() || std::get<0>(*it) > offset, "CHWShader_D3D::mfPrecacheAllCombinations(): Unmarked duplicate entry!");

		// Check for dev cache items with identical name
		const auto devCacheIt = devCache.find(devCacheKey);
		if (devCacheIt != devCache.end())
		{
			entries.emplace(it, offset, reinterpret_cast<const SDeviceShaderEntry*>(&devCacheIt->second));
			continue;
		}

		// Generate entry
		auto entry = mfShaderEntryFromCache(pFX, shaderEntry, rfOpenGuard, cache);
		if (!entry)
		{
			entries.emplace(it, offset, nullptr);
			continue;
		}

		// Store in the entries vector
		SHWShaderCache::deviceShaderCacheValue v;
		v.emplace<SDeviceShaderEntry>(std::move(entry));
		auto insertResult = devCache.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(devCacheKey), 
			std::forward_as_tuple(std::move(v)));
 		const auto* ptr = &insertResult.first->second;
		entries.emplace(it, offset, reinterpret_cast<const SDeviceShaderEntry*>(ptr));
	}

	// Handle duplicates
	for (const auto &p : duplicates)
	{
		const auto offset = p.first;
		auto it = std::lower_bound(entries.begin(), entries.end(), offset, entries_pred);
		CRY_ASSERT_MESSAGE(it != entries.end() && std::get<0>(*it) == offset, "CHWShader_D3D::mfPrecacheAllCombinations(): Duplicate entry not found!");

		// Add duplicate entry
		const auto ptr = std::get<1>(*it);
		if (it != entries.end() && std::get<0>(*it) == offset && ptr)
			devCache.insert(std::make_pair(p.second, SHWShaderCache::deviceShaderCacheValue{ ptr }));
	}
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
		SCGParam& Pr = InParams[0];
		bGeneral = false;
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

NO_INLINE void sGetScreenSize(UFloat4* sData, CD3D9Renderer* r)
{
	SRenderViewInfo viewInfo[2];
	gcpRendD3D.GetGraphicsPipeline().GenerateViewInfo(viewInfo);
	int iWidth = viewInfo[0].viewport.width;
	int iHeight = viewInfo[0].viewport.height;

	const auto& downscaleFactor = gRenDev->GetRenderQuality().downscaleFactor;
#if CRY_PLATFORM_WINDOWS
	float w = (float) (iWidth > 1 ? iWidth : 1);
	float h = (float) (iHeight > 1 ? iHeight : 1);
#else
	float w = (float) iWidth;
	float h = (float) iHeight;
#endif
	sData[0].f[0] = w;
	sData[0].f[1] = h;
	sData[0].f[2] = 0.5f / (w / downscaleFactor.x);
	sData[0].f[3] = 0.5f / (h / downscaleFactor.y);
}

NO_INLINE void sGetIrregKernel(UFloat4* sData, CD3D9Renderer* r)
{
	int nSamplesNum = 1;
	switch (gRenDev->GetRenderQuality().shaderQuality)
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

NO_INLINE Vec4 sGetVolumetricFogParams(const CCamera& camera)
{
	Vec4 pFogParams;

	I3DEngine* pEng = gEnv->p3DEngine;
	assert(pEng);

	Vec3 globalDensityParams(0, 1, 1);
	pEng->GetGlobalParameter(E3DPARAM_VOLFOG_GLOBAL_DENSITY, globalDensityParams);

	float globalDensity = globalDensityParams.x;

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

	const float viewerHeight = camera.GetPosition().z;

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

NO_INLINE Vec4 sGetFogColorGradientRadial(const CCamera& camera)
{
	I3DEngine* pEng = gEnv->p3DEngine;

	Vec3 fogColorRadial(0, 0, 0);
	pEng->GetGlobalParameter(E3DPARAM_FOG_RADIAL_COLOR, fogColorRadial);

	const float invFarDist = 1.0f / camera.GetFarPlane();

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

NO_INLINE Vec4 sGetVolumetricFogSamplingParams(const CCamera& camera)
{
	Vec3 volFogCtrlParams(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);
	const float raymarchStart = camera.GetNearPlane();
	const float raymarchDistance = (volFogCtrlParams.x > raymarchStart) ? (volFogCtrlParams.x - raymarchStart) : 0.0001f;

	Vec4 params;
	params.x = raymarchStart;
	params.y = 1.0f / raymarchDistance;
	params.z = static_cast<f32>(CRendererResources::s_ptexVolumetricFog ? CRendererResources::s_ptexVolumetricFog->GetDepth() : 0.0f);
	params.w = params.z > 0.0f ? (1.0f / params.z) : 0.0f;
	return params;
}

NO_INLINE Vec4 sGetVolumetricFogDistributionParams(const CCamera& camera, int32 frameID)
{
	Vec3 volFogCtrlParams(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);
	const float raymarchStart = camera.GetNearPlane();
	const float raymarchDistance = (volFogCtrlParams.x > raymarchStart) ? (volFogCtrlParams.x - raymarchStart) : 0.0001f;

	Vec4 params;
	params.x = raymarchStart;
	params.y = raymarchDistance;
	float d = static_cast<f32>(CRendererResources::s_ptexVolumetricFog ? CRendererResources::s_ptexVolumetricFog->GetDepth() : 0.0f);
	params.z = d > 1.0f ? (1.0f / (d - 1.0f)) : 0.0f;

	// frame count for jittering
	float frameCount = -static_cast<float>(frameID % 1024);
	frameCount = (CRenderer::CV_r_VolumetricFogReprojectionBlendFactor > 0.0f) ? frameCount : 0.0f;
	params.w = frameCount;

	return params;
}

NO_INLINE Vec4 sGetVolumetricFogScatteringParams(const CCamera& camera)
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

NO_INLINE Vec4 sGetVolumetricFogHeightDensityParams(const CCamera& camera)
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

	const float viewerHeight = camera.GetPosition().z;
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

NO_INLINE Vec4 sGetVolumetricFogDistanceParams(const CCamera& camera)
{
	float l, r, b, t, Ndist, Fdist;

	camera.GetAsymmetricFrustumParams(l,r,b,t);
	Ndist = camera.GetNearPlane();
	Fdist = camera.GetFarPlane();

	Vec3 volFogCtrlParams(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);
	const float raymarchStart = Ndist;
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

NO_INLINE void sCausticsSmoothSunDirection(UFloat4* sData)
{
	SRenderViewShaderConstants& PF = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView()->GetShaderConstants();
	Vec3 v(0.0f, 0.0f, 0.0f);
	I3DEngine* pEng = gEnv->p3DEngine;

	// Caustics are done with projection from sun - ence they update too fast with regular
	// sun direction. Use a smooth sun direction update instead to workaround this
	if (PF.nCausticsFrameID != gRenDev->GetRenderFrameID())
	{
		PF.nCausticsFrameID = gRenDev->GetRenderFrameID();
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

NO_INLINE void sNearFarDist(UFloat4* sData, CD3D9Renderer* r)
{
	float nearPlane = gcpRendD3D.GetGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left).nearClipPlane;
	float farPlane = gcpRendD3D.GetGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left).farClipPlane;
	sData[0].f[0] = nearPlane;
	sData[0].f[1] = farPlane;
	// NOTE : v[2] is used to put the weapon's depth range into correct relation to the whole scene
	// when generating the depth texture in the z pass (_RT_NEAREST)
	sData[0].f[2] = farPlane / gEnv->p3DEngine->GetMaxViewDistance();
	sData[0].f[3] = 1.0f / farPlane;
}

}
//////////////////////////////////////////////////////////////////////////
void CRenderer::ReadPerFrameShaderConstants(const SRenderingPassInfo& passInfo, bool bSecondaryViewport)
{
	// Per frame - hardcoded/fast - update of commonly used data - feel free to improve this
	CRenderView* pRenderView = passInfo.GetRenderView();

	SRenderViewShaderConstants& PF = pRenderView->GetShaderConstants();;
	uint32 nFrameID = passInfo.GetFrameID();
	if (PF.nFrameID == nFrameID)
		return;

	PF.nFrameID = nFrameID;

	// Updating..

	I3DEngine* p3DEngine = gEnv->p3DEngine;
	if (p3DEngine == NULL)
		return;

	const CCamera& camera = pRenderView->GetCamera(CCamera::eEye_Left);

	const int32 frameID = passInfo.GetFrameID();

	D3DViewPort viewport;
	{
		const SRenderViewport& vp = pRenderView->GetViewport();

		viewport.TopLeftX = static_cast<float>(vp.x);
		viewport.TopLeftY = static_cast<float>(vp.y);
		viewport.Width = static_cast<float>(vp.width);
		viewport.Height = static_cast<float>(vp.height);
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

	PF.pCameraPos = camera.GetPosition();

	// ECGP_PB_WaterLevel - x = static level y = dynamic water ocean/volume level based on camera position, z: dynamic ocean water level
	PF.vWaterLevel = Vec3(p3DEngine->GetWaterLevel());

	// ECGP_PB_HDRDynamicMultiplier
	PF.fHDRDynamicMultiplier = HDRDynamicMultiplier;

	PF.pVolumetricFogParams = sGetVolumetricFogParams(camera);
	PF.pVolumetricFogRampParams = sGetVolumetricFogRampParams();
	PF.pVolumetricFogSunDir = sGetVolumetricFogSunDir(sunDir);

	sGetFogColorGradientConstants(PF.pFogColGradColBase, PF.pFogColGradColDelta);
	PF.pFogColGradParams = sGetFogColorGradientParams();
	PF.pFogColGradRadial = sGetFogColorGradientRadial(camera);

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
		PF.pCloudShadowAnimParams = CVolumetricCloudsStage::GetVolumetricCloudShadowParams(camera, cloudOffset, vTiling);
	}
	else
	{
		// ECGP_PB_CloudShadowParams
		PF.pCloudShadowParams = Vec4(0, 0, m_cloudShadowInvert ? 1.0f : 0.0f, m_cloudShadowBrightness);

		// ECGP_PB_CloudShadowAnimParams
		PF.pCloudShadowAnimParams = Vec4(m_cloudShadowTiling / heightMapSize, -m_cloudShadowTiling / heightMapSize, cloudShadowOffset.x, -cloudShadowOffset.y);
	}

	// ECGP_PB_ScreenspaceShadowsParams
	PF.pScreenspaceShadowsParams = Vec4(CRenderer::CV_r_ShadowsScreenSpaceLength * passInfo.GetCamera().GetFov() / DEG2RAD(60.f), 0, 0, 0);

	// ECGP_PB_VolumetricFogSamplingParams
	PF.pVolumetricFogSamplingParams = sGetVolumetricFogSamplingParams(camera);

	// ECGP_PB_VolumetricFogDistributionParams
	PF.pVolumetricFogDistributionParams = sGetVolumetricFogDistributionParams(camera, frameID);

	// ECGP_PB_VolumetricFogScatteringParams
	PF.pVolumetricFogScatteringParams = sGetVolumetricFogScatteringParams(camera);

	// ECGP_PB_VolumetricFogScatteringParams
	PF.pVolumetricFogScatteringBlendParams = sGetVolumetricFogScatteringBlendParams();

	// ECGP_PB_VolumetricFogScatteringColor
	PF.pVolumetricFogScatteringColor = sGetVolumetricFogScatteringColor();

	// ECGP_PB_VolumetricFogScatteringSecondaryColor
	PF.pVolumetricFogScatteringSecondaryColor = sGetVolumetricFogScatteringSecondaryColor();

	// ECGP_PB_VolumetricFogHeightDensityParams
	PF.pVolumetricFogHeightDensityParams = sGetVolumetricFogHeightDensityParams(camera);

	// ECGP_PB_VolumetricFogHeightDensityRampParams
	PF.pVolumetricFogHeightDensityRampParams = sGetVolumetricFogHeightDensityRampParams();

	// ECGP_PB_VolumetricFogDistanceParams
	PF.pVolumetricFogDistanceParams = sGetVolumetricFogDistanceParams(camera);

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

bool CHWShader_D3D::PrecacheShader(CShader* pShader, const SShaderCombIdent &cacheIdent,uint32 nFlags)
{
	SShaderCombIdent Ident;
	Ident.m_LightMask = cacheIdent.m_LightMask;
	Ident.m_RTMask = cacheIdent.m_RTMask & m_nMaskAnd_RT | m_nMaskOr_RT;
	Ident.m_MDMask = cacheIdent.m_MDMask;
	Ident.m_MDVMask = cacheIdent.m_MDVMask | CParserBin::m_nPlatform;
	Ident.m_GLMask = m_nMaskGenShader;
	Ident.m_pipelineState = cacheIdent.m_pipelineState;

	ModifyLTMask(Ident.m_LightMask);

	SHWSInstance* pInst = mfGetInstance(Ident, nFlags);
	if (!pInst)
		return false;

	return CheckActivation(pShader, pInst, nFlags) != 0;
}

int CHWShader_D3D::CheckActivation(CShader* pSH, SHWSInstance*& pInst, uint32 nFlags)
{
	ED3DShError eError = mfIsValid(pInst, true);
	if (eError == ED3DShError_NotCompiled)
	{
		if (!mfActivate(pSH, nFlags))
		{
			pInst = m_pCurInst;
			if (!pInst->IsAsyncCompiling())
				pInst->m_Handle.SetNonCompilable();
			else
			{
				eError = mfIsValid(pInst, true);
				if (eError == ED3DShError_CompilingError || pInst->m_bAsyncActivating)
					return 0;
				if (m_eSHClass == eHWSC_Vertex)
					return 1;
				else
					return -1;
			}
			return 0;
		}
		//if (gRenDev->m_RP.m_pCurTechnique)
			//mfUpdatePreprocessFlags(gRenDev->m_RP.m_pCurTechnique);
		pInst = m_pCurInst;
	}
	else if (eError == ED3DShError_Fake)
	{
		if (pInst->m_Handle.m_pData)
		{
			if (gRenDev && !gRenDev->CheckDeviceLost())
			{
				mfUploadHW(pInst, pInst->m_Handle.m_pData, pInst->m_Handle.m_nData, pSH, nFlags);
				SAFE_DELETE_ARRAY(pInst->m_Handle.m_pData);
				pInst->m_Handle.m_nData = 0;
			}
			else
				eError = ED3DShError_CompilingError;
		}
	}
	if (eError == ED3DShError_CompilingError)
		return 0;
	return 1;
}

void CHWShader_D3D::mfSetParameters(SCGParam* pParams, const int nINParams, EHWShaderClass eSH, int nMaxVecs, Vec4* pOutBuffer, uint32 outBufferSize, const D3DViewPort* pVP)
{
	DETAILED_PROFILE_MARKER("mfSetParameters");
	PROFILE_FRAME(Shader_SetParams);

	float* pSrc;
	Vec3 v;
	Vec4 v4;
	const SCGParam* ParamBind = pParams;
	int nParams;
	const int rLog = CRenderer::CV_r_log;

	if (!pParams)
		return;

	CD3D9Renderer* const __restrict r = gcpRendD3D;
	SRenderViewShaderConstants& PF = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView()->GetShaderConstants();

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
			switch (paramType & 0xff)
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
				CRY_ASSERT(0);
				CryLogAlways("ERROR: Material value reflection found in shader! This does not work any more, please change your shader-code to use the PerMaterial constant-buffer instead.");
				break;

			case ECGP_PB_CameraPos:
				{
					const Vec3& vCamPos = gcpRendD3D.GetGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left).cameraOrigin;
					sData[0].f[0] = vCamPos.x;
					sData[0].f[1] = vCamPos.y;
					sData[0].f[2] = vCamPos.z;
					sData[0].f[3] = gcpRendD3D.GetGraphicsPipeline().GetCurrentRenderView()->IsViewFlag(SRenderViewInfo::eFlags_MirrorCull) ? -1.0f : 1.0f;
				}
				break;

			case ECGP_PB_ScreenSize:
			//	sGetScreenSize(sData, r);
				CRY_ASSERT(pVP);
				sData[0].f[0] =        pVP->Width;
				sData[0].f[1] =        pVP->Height;
				sData[0].f[2] = 0.5f / pVP->Width;
				sData[0].f[3] = 0.5f / pVP->Height;
				break;
			case ECGP_PB_NearFarDist:
				sNearFarDist(sData, r);
				break;

			case ECGP_PB_ClipVolumeParams:
				sData[0].f[0] = 1.0f; //rRP.m_pCurObject->m_nClipVolumeStencilRef + 1.0f;
				sData[0].f[1] = sData[0].f[2] = sData[0].f[3] = 0;
				break;

			case ECGP_PB_FromRE:
				ASSERT_LEGACY_PIPELINE;
				sZeroLine(sData);
				/*
				pRE = rRP.m_pRE;
				if (!pRE || !(pData = (float*)pRE->m_CustomData))
					sData[0].f[nComp] = 0;
				else
					sData[0].f[nComp] = pData[(ParamBind->m_nID >> (nComp * 8)) & 0xff];
					*/
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

					{
						auto pStage = r->GetGraphicsPipeline().GetVolumetricFogStage();
						assert(pStage);
						if (pStage)
						{
							param = pStage->GetGlobalEnvProbeShaderParam0();
						}
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

			case ECGP_PB_WaterLevel:
				sData[0].f[0] = PF.vWaterLevel.x;
				sData[0].f[1] = PF.vWaterLevel.y;
				sData[0].f[2] = PF.vWaterLevel.z;
				sData[0].f[3] = 1.0f;
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

			case ECGP_PB_StereoParams:
				if (gRenDev->IsStereoEnabled())
				{
					sData[0].f[0] = gcpRendD3D->GetS3DRend().GetMaxSeparationScene() * (gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView()->GetCurrentEye() == CCamera::eEye_Left ? 1 : -1);
					sData[0].f[1] = gcpRendD3D->GetS3DRend().GetZeroParallaxPlaneDist();
					sData[0].f[2] = gcpRendD3D->GetS3DRend().GetNearGeoShift();
					sData[0].f[3] = gcpRendD3D->GetS3DRend().GetNearGeoScale();
				}
				else
					sData[0].f[0] = sData[0].f[1] = sData[0].f[2] = 0;
				break;
			case ECGP_PB_IrregKernel:
				sGetIrregKernel(sData, r);
				break;
			case ECGP_PB_RegularKernel:
				sGetRegularKernel(sData, r);
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

			case ECGP_PB_CausticsSmoothSunDirection:
				sCausticsSmoothSunDirection(sData);
				break;

			case ECGP_PB_Time:
				//sData[0].f[nComp] = gRenDev->m_RP.m_ShaderCurrTime; //gRenDev->m_RP.m_RealTime;
				sData[0].f[nComp] = gRenDev->GetFrameSyncTime().GetSeconds();
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

			case ECGP_PB_VisionMtlParams:
				sOneLine(sData);
				break;

			case ECGP_PB_WaterRipplesLookupParams:

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
				ASSERT_LEGACY_PIPELINE
			}
		}
		++ParamBind;
	}
}

//=========================================================================================

void CHWShader_D3D::mfReset()
{
	DETAILED_PROFILE_MARKER("mfReset");
	for (uint32 i = 0; i < m_Insts.size(); i++)
	{
		m_pCurInst = m_Insts[i];
		assert(m_pCurInst);
		PREFAST_ASSUME(m_pCurInst);

		delete m_pCurInst;
	}
	m_pCurInst = NULL;
	m_Insts.clear();

	if (m_pCache)
	{
		m_pCache->Release();
		m_pCache = nullptr;
	}
	m_CachedTokens = {};
}

CHWShader_D3D::~CHWShader_D3D()
{
	mfFree();
}

void CHWShader_D3D::mfInit()
{
	CGParamManager::Init();
}

void CHWShader_D3D::mfConstruct()
{
	if (s_bInitShaders)
	{
		s_bInitShaders = false;
	}

	m_pCurInst = nullptr;
	m_nCurInstFrame = 0;

	m_dwShaderType = 0;
}

ED3DShError CHWShader_D3D::mfFallBack(SHWSInstance*& pInst, int nStatus)
{
	return ED3DShError_CompilingError;
}


ED3DShError CHWShader_D3D::mfIsValid_Int(SHWSInstance*& pInst, bool bFinalise)
{
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

	// TODO: make it a RLock if resource-library isn't manipulated
	CryAutoWriteLock<CryRWLock> lock(CBaseResource::s_cResLock);

	CCryNameTSCRC Name = CHWShader::mfGetClassName(eHWSC_Pixel);
	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(Name);
	uint32 i;
	uint32 j;
	float fTime = gRenDev->GetFrameSyncTime().GetSeconds();

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

CHWShader_D3D::SHWSInstance* CHWShader_D3D::mfGetInstance(int nHashInstance, SShaderCombIdent& Ident)
{
	DETAILED_PROFILE_MARKER("mfGetInstance");
	FUNCTION_PROFILER_RENDER_FLAT
	InstContainer* pInstCont = &m_Insts;

	InstContainerIt it;
	SHWSInstance* pInst = mfGetHashInst(pInstCont, nHashInstance, Ident, it);

	return pInst;
}

CHWShader_D3D::SHWSInstance* CHWShader_D3D::mfGetInstance(SShaderCombIdent& Ident, uint32 nFlags)
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

	if (!pInst)
	{
		pInst = new SHWSInstance;
		pInst->m_nVertexFormat = 1;
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

void CHWShader_D3D::mfUpdatePreprocessFlags(SShaderTechnique* pTech)
{
	DETAILED_PROFILE_MARKER("mfUpdatePreprocessFlags");
	uint32 nFlags = 0;

	for (uint32 i = 0; i < (uint32)m_Insts.size(); i++)
	{
		SHWSInstance* pInst = m_Insts[i];
		for (const auto &t : pInst->m_pFXTextures)
		{
			if (t && t->m_pTarget)
			{
				SHRenderTarget* pTarg = t->m_pTarget;
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
	if (pTech)
	{
		pTech->m_RTargets.Shrink();
		pTech->m_nPreprocessFlags |= nFlags;
	}
}

Vec4 CHWShader_D3D::GetVolumetricFogParams(const CCamera& camera)
{
	DETAILED_PROFILE_MARKER("GetVolumetricFogParams");
	return sGetVolumetricFogParams(camera);
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

Vec4 CHWShader_D3D::GetFogColorGradientRadial(const CCamera& camera)
{
	DETAILED_PROFILE_MARKER("GetFogColorGradientRadial");
	return sGetFogColorGradientRadial(camera);
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning(pop)
#endif
