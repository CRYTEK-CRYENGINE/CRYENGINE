// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShaderSerialize.cpp : implementation of the Shaders serialization manager.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"

#ifdef SHADERS_SERIALIZING

bool CShaderSerialize::_OpenSResource(float fVersion, SSShaderRes* pSR, CShader* pSH, cacheSource nCache, CResFile* pRF, bool bReadOnly)
{
	assert(nCache == cacheSource::user || nCache == cacheSource::readonly);

	SSShaderCacheHeader hd;
	ZeroStruct(hd);
	bool bValid = true;
	bool bCheckValid = true;
	if (!CRenderer::CV_r_shadersAllowCompilation)
		bCheckValid = false;

	//uint64 writeTimeCFX=0;

	//See if resfile is in assets dir
	if (!pRF->mfOpen(RA_READ | (CParserBin::m_bEndians ? RA_ENDIANS : 0), NULL, NULL))
	{
		pRF->mfClose();
		bValid = false;
	}

	if (bValid)
	{
		pRF->mfFileSeek(CShaderMan::s_cNameHEAD, 0, SEEK_SET);
		pRF->mfFileRead2(CShaderMan::s_cNameHEAD, sizeof(SSShaderCacheHeader), &hd);
		if (CParserBin::m_bEndians)
			SwapEndian(hd, eBigEndian);

		if (hd.m_SizeOf != sizeof(SSShaderCacheHeader))
		{
			bValid = false;
		}
		else if (fVersion && (hd.m_MajorVer != (int)fVersion || hd.m_MinorVer != (int)(((float)fVersion - (float)(int)fVersion) * 10.1f)))
		{
			bValid = false;

			//if (CRenderer::CV_r_shadersdebug==2)
			{
				LogWarning("WARNING: Shader resource '%s' version mismatch (Resource: %s, Expected: %.1f)", pRF->mfGetFileName(), hd.m_szVer, fVersion);
			}
		}

		if (bCheckValid)
		{
			char nameSrc[256];
			cry_sprintf(nameSrc, "%sCryFX/%s.%s", gRenDev->m_cEF.m_ShadersPath, pSH->GetName(), "cfx");
			uint32 srcCRC = pSH->m_SourceCRC32;

			if (!srcCRC)
			{
				srcCRC = gEnv->pCryPak->ComputeCRC(nameSrc);
				pSH->m_SourceCRC32 = srcCRC; //Propagate to shader, prevent recalculation
			}

			if (srcCRC && srcCRC != hd.m_SourceCRC32)
			{
				bValid = false;

				//if (CRenderer::CV_r_shadersdebug==2)
				{
					LogWarning("WARNING: Shader resource '%s' src CRC mismatch", pRF->mfGetFileName());
				}
			}
		}
		if (bValid && nCache == cacheSource::user)
		{
			pRF->mfClose();
			if (!pRF->mfOpen(RA_READ | (CParserBin::m_bEndians ? RA_ENDIANS : 0) | RA_WRITE, NULL, NULL))
			{
				pRF->mfClose();
				bValid = false;
			}
		}
	}
	if (!bValid && nCache == cacheSource::user && !bReadOnly && pSH->m_CRC32) //Create ResFile
	{
		if (!pRF->mfOpen(RA_CREATE | (CParserBin::m_bEndians ? RA_ENDIANS : 0), NULL, NULL))
			return false;

		CDirEntry de(CShaderMan::s_cNameHEAD ,sizeof(SSShaderCacheHeader));
		hd.m_SizeOf = sizeof(SSShaderCacheHeader);
		hd.m_MinorVer = (int)(((float)fVersion - (float)(int)fVersion) * 10.1f);
		hd.m_MajorVer = (int)fVersion;
		hd.m_CRC32 = pSH->m_CRC32;
		hd.m_SourceCRC32 = pSH->m_SourceCRC32;
		cry_sprintf(hd.m_szVer, "Ver: %.1f", fVersion);
		SSShaderCacheHeader hdTemp, * pHD;
		pHD = &hd;
		if (CParserBin::m_bEndians)
		{
			hdTemp = hd;
			SwapEndian(hdTemp, eBigEndian);
			pHD = &hdTemp;
		}

		//create dir
		pRF->mfFileAdd(&de);
		//open dir and populate data
		SDirEntryOpen* pOpenDir = pRF->mfOpenEntry(de.GetName());
		pOpenDir->pData = pHD;
		pOpenDir->nSize = de.GetSize();

		pRF->mfFlush();
		bValid = true;
	}

	if (!bValid)
	{
		SAFE_DELETE(pRF);
	}

	pSR->m_pRes[static_cast<int>(nCache)] = pRF;
	pSR->m_Header[static_cast<int>(nCache)] = hd;
	pSR->m_bReadOnly[static_cast<int>(nCache)] = bReadOnly;

	if (bValid && !pSH->m_CRC32)
		pSH->m_CRC32 = hd.m_CRC32;

	return bValid;
}

bool CShaderSerialize::OpenSResource(const char* szName, SSShaderRes* pSR, CShader* pSH, bool bDontUseUserFolder, bool bReadOnly)
{
	bool bValidRO   = false;
	bool bValidUser = false;

	float fVersion = (float)FX_CACHE_VER + (float)(FX_SER_CACHE_VER);

	// check %ENGINE% first
	{
		stack_string szEngine = stack_string("%ENGINE%/") + stack_string(szName);
		CResFile* rfRO = new CResFile(szEngine.c_str());
		bValidRO = _OpenSResource(fVersion, pSR, pSH, bDontUseUserFolder ? cacheSource::readonly : cacheSource::user, rfRO, bReadOnly);
	}

	// now %USER%
#if !defined(SHADER_NO_SOURCES)
	if (!bDontUseUserFolder)
	{
		stack_string szUser = stack_string(gRenDev->m_cEF.m_szUserPath.c_str()) + stack_string(szName);
		CResFile* rfUser = new CResFile(szUser.c_str());
		bValidUser = _OpenSResource(fVersion, pSR, pSH, cacheSource::user, rfUser, bReadOnly);
	}
#endif

	return (bValidRO || bValidUser);
}

bool CShaderSerialize::CreateSResource(CShader* pSH, SSShaderRes* pSR, CCryNameTSCRC& SName, bool bDontUseUserFolder, bool bReadOnly)
{
	string dstName;
	dstName.reserve(512);
	dstName  = gRenDev->m_cEF.m_ShadersCache;
	dstName += pSH->GetName();
	dstName += ".fxb";

	bool bRes = OpenSResource(dstName.c_str(), pSR, pSH, bDontUseUserFolder, bReadOnly);

	return bRes;
}

SSShaderRes* CShaderSerialize::InitSResource(CShader* pSH, bool bDontUseUserFolder, bool bReadOnly)
{
	SSShaderRes* pSR = NULL;
	stack_string shaderName = pSH->GetName();
	shaderName += "_GLOBAL";
	CCryNameTSCRC SName = CCryNameTSCRC(shaderName.c_str());
	FXSShaderResItor itS = m_SShaderResources.find(SName);
	#if SHADER_NO_SOURCES
	bool bCheckValid = false;
	#else
	bool bCheckValid = true;
	#endif
	if (itS != m_SShaderResources.end())
	{
		pSR = itS->second;
		pSR->m_nRefCount++;
		if (bCheckValid)
		{
			int nCache[2] = { -1, -1 };
			if (!bReadOnly || bDontUseUserFolder)
				nCache[0] = static_cast<int>(cacheSource::user);
			else if (!bDontUseUserFolder || bReadOnly)
			{
				nCache[0] = static_cast<int>(cacheSource::user);
				nCache[1] = static_cast<int>(cacheSource::readonly);
			}
			for (int i = 0; i < 2; i++)
			{
				if (nCache[i] < 0 || !pSR->m_pRes[i])
					continue;

				//if the shader has a CRC then we can test, generally it is only valid during cache gen
				if (pSH->m_CRC32 != 0 && pSR->m_Header[i].m_CRC32 != pSH->m_CRC32)
				{
					//CryLogAlways("CRC check failed in serialise: 0x%x source: 0x%x\n", pSR->m_Header[i].m_CRC32, pSH->m_CRC32);
					SAFE_DELETE(pSR->m_pRes[i]);
				}
			}
			bool bValid = true;
			if (!bReadOnly && !pSR->m_pRes[static_cast<int>(cacheSource::user)])
				bValid = false;
			else if ((!bDontUseUserFolder || bReadOnly) && !pSR->m_pRes[static_cast<int>(cacheSource::readonly)] && !pSR->m_pRes[static_cast<int>(cacheSource::user)])
				bValid = false;
			if (!bValid)
			{
				CreateSResource(pSH, pSR, SName, bDontUseUserFolder, bReadOnly);
			}
			else
			{
				//printf("Reusing existing .fxb for %s\n", pSH->GetName() );
			}
		}
	}
	else
	{
		pSR = new SSShaderRes;
		bool bRes = CreateSResource(pSH, pSR, SName, bDontUseUserFolder, bReadOnly);

		if (bRes)
		{
			m_SShaderResources.insert(FXSShaderResItor::value_type(SName, pSR));
		}
		else
		{
			SAFE_DELETE(pSR);
		}
	}

	if (pSH->m_CRC32 == 0 && pSR)
	{
		if (pSR->m_pRes[static_cast<int>(cacheSource::readonly)])
		{
			pSH->m_CRC32 = pSR->m_Header[static_cast<int>(cacheSource::readonly)].m_CRC32;
		}
		else if (pSR->m_pRes[static_cast<int>(cacheSource::user)])
		{
			pSH->m_CRC32 = pSR->m_Header[static_cast<int>(cacheSource::user)].m_CRC32;
		}
	}

	return pSR;
}

void CShaderSerialize::ClearSResourceCache()
{
	const FXSShaderResItor end_it = m_SShaderResources.end();
	FXSShaderResItor it = m_SShaderResources.begin();

	for (; it != end_it; it++)
	{
		SAFE_DELETE(it->second);
	}

	m_SShaderResources.clear();
}

bool CShaderSerialize::ExportHWShader(CShader *pSH, CHWShader* pShader, SShaderSerializeContext& SC)
{
	if (!pShader)
		return false;

	bool bRes = pShader->Export(pSH, SC);

	return bRes;
}
CHWShader* CShaderSerialize::ImportHWShader(SShaderSerializeContext& SC, int nOffs, uint32 CRC32, CShader* pSH)
{
	CHWShader* pRes = CHWShader::Import(SC, nOffs, CRC32, pSH);

	return pRes;
}

bool CShaderSerialize::ExportShader(CShader* pSH, CShaderManBin& binShaderMgr)
{
	CryLogAlways("[CShaderSerialize] ExportShader: %s flags: 0x%" PRIx64 " mdvFlags: 0x%x\n", pSH->GetName(), pSH->m_nMaskGenFX, pSH->m_nMDV);

	bool bRes = true;

	//Use user folder on export?
	SSShaderRes* pSR = InitSResource(pSH, false /*((gRenDev->m_cEF.m_nCombinations > 0)*/, false);

	uint32 i;
	int j;

	if (!pSR || !pSR->m_pRes[static_cast<int>(cacheSource::user)])
		return false;

	SShaderSerializeContext SC;

	SC.SSR.m_eSHDType = pSH->m_eSHDType;
	SC.SSR.m_Flags = pSH->m_Flags;
	SC.SSR.m_Flags2 = pSH->m_Flags2;
	SC.SSR.m_nMDV = pSH->m_nMDV;
	SC.SSR.m_eVertexFormat = pSH->m_eVertexFormat;
	SC.SSR.m_eCull = pSH->m_eCull;
	//SC.SSR.m_nMaskCB = pSH->m_nMaskCB; //maskCB generated on HW shader activation
	SC.SSR.m_eShaderType = pSH->m_eShaderType;
	SC.SSR.m_nMaskGenFX = pSH->m_nMaskGenFX;

	SC.SSR.m_nTechniques = pSH->m_HWTechniques.Num();

	SShaderFXParams& params = binShaderMgr.mfGetFXParams(pSH);

	SC.SSR.m_nPublicParams = params.m_PublicParams.size();
	for (i = 0; i < SC.SSR.m_nPublicParams; i++)
	{
		SSShaderParam PR;
		SShaderParam& P = params.m_PublicParams[i];
		PR.m_nameIdx = SC.AddString(P.m_Name);
		PR.m_Type = P.m_Type;
		PR.m_Value = P.m_Value;
		PR.m_nScriptOffs = SC.AddString(P.m_Script.c_str());
		SC.Params.Add(PR);
	}

	SC.SSR.m_nFXParams = params.m_FXParams.size();
	for (i = 0; i < SC.SSR.m_nFXParams; i++)
	{
		params.m_FXParams[i].Export(SC);
	}

	SC.SSR.m_nFXSamplers = params.m_FXSamplers.size();
	for (i = 0; i < SC.SSR.m_nFXSamplers; i++)
	{
		SSTexSamplerFX texSampler;
		params.m_FXSamplers[i].Export(SC);
	}

	SC.SSR.m_nFXTexRTs = SC.FXTexRTs.Num();

	for (i = 0; i < SC.SSR.m_nTechniques; i++)
	{
		SSShaderTechnique ST;
		SShaderTechnique* pT = pSH->m_HWTechniques[i];
		ST.m_nPreprocessFlags = pT->m_nPreprocessFlags;
		ST.m_nNameOffs = SC.AddString(pT->m_NameStr.c_str());
		ST.m_Flags = pT->m_Flags;

		int TECH_MAX = TTYPE_MAX;

		for (j = 0; j < TECH_MAX; j++)
		{
			ST.m_nTechnique[j] = pT->m_nTechnique[j];
		}
		ST.m_nREs = pT->m_REs.Num();

		ST.m_nPassesOffs = SC.Passes.Num();
		ST.m_nPasses = pT->m_Passes.Num();
		SC.SSR.m_nPasses += ST.m_nPasses;
		for (j = 0; j < ST.m_nPasses; j++)
		{
			SSShaderPass PS;
			SShaderPass& P = pT->m_Passes[j];
			PS.m_RenderState = P.m_RenderState;
			PS.m_eCull = P.m_eCull;
			PS.m_AlphaRef = P.m_AlphaRef;
			PS.m_PassFlags = P.m_PassFlags;

			assert(!(SC.Data.Num() & 0x3));

			if (P.m_VShader)
			{
				PS.m_nVShaderOffs = SC.Data.Num();
				if (!ExportHWShader(pSH, P.m_VShader, SC))
				{
					PS.m_nVShaderOffs = -1;
					CryFatalError("Shader export failed. Set r_shadersExport=0 and inform AndyM");
				}
			}
			else
			{
				PS.m_nVShaderOffs = -1;
			}

			if (P.m_PShader)
			{
				PS.m_nPShaderOffs = SC.Data.Num();
				if (!ExportHWShader(pSH, P.m_PShader, SC))
				{
					PS.m_nPShaderOffs = -1;
					CryFatalError("Shader export failed. Set r_shadersExport=0 and inform AndyM");
				}
			}
			else
			{
				PS.m_nPShaderOffs = -1;
			}
			SC.Passes.Add(PS);
		}

		ST.m_nREsOffs = (ST.m_nREs > 0) ? SC.Data.Num() : -1;
		for (j = 0; j < ST.m_nREs; j++)
		{
			CRenderElement* pRE = pT->m_REs[j];
			uint32 type = pRE->m_Type;
			sAddData(SC.Data, type);
			pRE->mfExport(SC);
			sAlignData(SC.Data, 4);
		}
		SC.Techniques.AddElem(ST);
	}

	TArray<byte> Data;

	//DEBUG - prevent Data reallocs
	//Data.Alloc(1024*1024);

	uint32 nPublicParamsOffset;
	uint32 nFXParamsOffset;
	uint32 nFXTexSamplersOffset;
	uint32 nFXTexRTsOffset;
	uint32 nTechOffset;
	uint32 nPassOffset;
	uint32 nStringsOffset;
	uint32 nDataOffset;

	//sAddData(Data, SC.SSR);
	SC.SSR.Export(Data);
	sAddDataArray(Data, SC.Params, nPublicParamsOffset);
	sAddDataArray(Data, SC.FXParams, nFXParamsOffset);
	sAddDataArray(Data, SC.FXTexSamplers, nFXTexSamplersOffset);
	sAddDataArray(Data, SC.FXTexRTs, nFXTexRTsOffset);
	sAddDataArray(Data, SC.Techniques, nTechOffset);
	sAddDataArray(Data, SC.Passes, nPassOffset);
	sAddDataArray_POD(Data, SC.Strings, nStringsOffset);
	sAddDataArray_POD(Data, SC.Data, nDataOffset);

	SSShader* pSSR = (SSShader*)&Data[0];
	pSSR->m_nPublicParamsOffset = nPublicParamsOffset;
	pSSR->m_nFXParamsOffset = nFXParamsOffset;
	pSSR->m_nFXSamplersOffset = nFXTexSamplersOffset;
	pSSR->m_nFXTexRTsOffset = nFXTexRTsOffset;
	pSSR->m_nTechOffset = nTechOffset;
	pSSR->m_nPassOffset = nPassOffset;
	pSSR->m_nStringsOffset = nStringsOffset;
	pSSR->m_nDataOffset = nDataOffset;
	pSSR->m_nDataSize = SC.Data.Num();
	pSSR->m_nStringsSize = SC.Strings.Num();

	if (CParserBin::m_bEndians)
	{
		SwapEndian(pSSR->m_nPublicParamsOffset, eBigEndian);
		SwapEndian(pSSR->m_nFXParamsOffset, eBigEndian);
		SwapEndian(pSSR->m_nFXSamplersOffset, eBigEndian);
		SwapEndian(pSSR->m_nFXTexRTsOffset, eBigEndian);
		SwapEndian(pSSR->m_nTechOffset, eBigEndian);
		SwapEndian(pSSR->m_nPassOffset, eBigEndian);
		SwapEndian(pSSR->m_nStringsOffset, eBigEndian);
		SwapEndian(pSSR->m_nDataOffset, eBigEndian);
		SwapEndian(pSSR->m_nDataSize, eBigEndian);
		SwapEndian(pSSR->m_nStringsSize, eBigEndian);
	}

	int nLen = Data.Num();
	char sName[128];
	#if defined(__GNUC__)
	cry_sprintf(sName, "(%llx)", pSH->m_nMaskGenFX);
	#else
	cry_sprintf(sName, "(%I64x)", pSH->m_nMaskGenFX);
	#endif

	CDirEntry de(sName, nLen, RF_COMPRESS);
	pSR->m_pRes[static_cast<int>(cacheSource::user)]->mfFileAdd(&de);

	//create open dir and populate data
	SDirEntryOpen* pOpenDir = pSR->m_pRes[static_cast<int>(cacheSource::user)]->mfOpenEntry(de.GetName());
	pOpenDir->pData = &Data[0];
	pOpenDir->nSize = de.GetSize();

	//Preserve modification time
	uint64 modTime = pSR->m_pRes[static_cast<int>(cacheSource::user)]->mfGetModifTime();

	pSR->m_pRes[static_cast<int>(cacheSource::user)]->mfFlush();

	return bRes;
}

float g_fTime0;
float g_fTime1;
float g_fTime2;

bool CShaderSerialize::CheckFXBExists(CShader* pSH)
{
	SSShaderRes* pSR = InitSResource(pSH, false, true);
	if (!pSR || (!pSR->m_Header[static_cast<int>(cacheSource::user)].m_CRC32 && !pSR->m_Header[static_cast<int>(cacheSource::readonly)].m_CRC32))
		return false;

	char sName[128];
	#if defined(__GNUC__)
	cry_sprintf(sName, "(%llx)", pSH->m_nMaskGenFX);
	#else
	cry_sprintf(sName, "(%I64x)", pSH->m_nMaskGenFX);
	#endif

	CCryNameTSCRC CName = CCryNameTSCRC(sName);
	CDirEntry* pDE = NULL;
	CResFile* pRes = NULL;

	for (int i = 0; i < 2; i++)
	{
		pRes = pSR->m_pRes[i];
		if (!pRes)
			continue;
		pDE = pRes->mfGetEntry(CName);

		if (pDE)
		{
			return true;
		}
	}
	return false;
}

bool CShaderSerialize::ImportShader(CShader* pSH, CShaderManBin& binShaderMgr)
{
	if (CParserBin::m_bEndians)
	{
		CryFatalError("CShaderSerialize - cross platform import not supported");
	}

	float fTime0 = iTimer->GetAsyncCurTime();

	bool bRes = true;
	SSShaderRes* pSR = NULL;
	uint32 i;
	int j;
	char sName[128];
	#if defined(__GNUC__)
	cry_sprintf(sName, "(%llx)", pSH->m_nMaskGenFX);
	#else
	cry_sprintf(sName, "(%I64x)", pSH->m_nMaskGenFX);
	#endif
	CCryNameTSCRC CName = CCryNameTSCRC(sName);
	CDirEntry* pDE = NULL;
	CResFile* pRes = NULL;

	// Not found yet
	if (!pDE || !pRes)
	{
		//try global cache
		pSR = InitSResource(pSH, !CRenderer::CV_r_shadersAllowCompilation, true);

		if (pSR && (pSR->m_Header[static_cast<int>(cacheSource::user)].m_CRC32 != 0 || pSR->m_Header[static_cast<int>(cacheSource::readonly)].m_CRC32 != 0))
		{
			for (i = 0; i < 2; i++)
			{
				pRes = pSR->m_pRes[i];
				if (!pRes)
					continue;
				pDE = pRes->mfGetEntry(CName);
				if (pDE)
					break;
			}
		}

		if (!pDE)
		{
			gRenDev->LogShaderImportMiss(pSH);
			return false;
		}
	}

	if (CRenderer::CV_r_shaderssubmitrequestline > 1)
	{
		gRenDev->LogShaderImportMiss(pSH);
	}

	assert(pSH != nullptr);

	int nSize = pRes->mfFileRead(pDE);
	byte* pData = (byte*)pRes->mfFileGetBuf(pDE);
	if (!pData)
	{
		return false;
	}

	//printf("[CShaderSerialize] Import Shader: %s flags: 0x%llx mdvFlags: 0x%x from %s cache %s\n", pSH->GetName(), pSH->m_nMaskGenFX, pSH->m_nMDV, bLoadedFromLevel ? "LEVEL" : "GLOBAL", pSR->m_pRes[i]->mfGetFileName());

	byte* pSrc = pData;
	g_fTime0 += iTimer->GetAsyncCurTime() - fTime0;

	float fTime1 = iTimer->GetAsyncCurTime();

	SShaderFXParams& fxParams = binShaderMgr.mfGetFXParams(pSH);

	SShaderSerializeContext SC;
	SC.SSR.Import(pSrc);

	CryLog("[CShaderSerialize] Import Shader: %s flags: 0x%" PRIx64 " mdvFlags: 0x%x from global cache %s\n", pSH->GetName(), pSH->m_nMaskGenFX, pSH->m_nMDV, pSR->m_pRes[i]->mfGetFileName());

	if (SC.SSR.m_nPublicParams)
	{
		SC.Params.ReserveNoClear(SC.SSR.m_nPublicParams);

		if (!CParserBin::m_bEndians)
		{
			memcpy(&SC.Params[0], &pSrc[SC.SSR.m_nPublicParamsOffset], sizeof(SSShaderParam) * SC.SSR.m_nPublicParams);
		}
		else
		{
			uint32 offset = SC.SSR.m_nPublicParamsOffset;
			for (i = 0; i < SC.SSR.m_nPublicParams; i++)
			{
				SC.Params[i].Import(&pSrc[offset]);
				offset += sizeof(SSShaderParam);
			}
		}
	}

	if (SC.SSR.m_nFXParams)
	{
		SC.FXParams.ReserveNoClear(SC.SSR.m_nFXParams);

		if (!CParserBin::m_bEndians)
		{
			memcpy(&SC.FXParams[0], &pSrc[SC.SSR.m_nFXParamsOffset], sizeof(SSFXParam) * SC.SSR.m_nFXParams);
		}
		else
		{
			uint32 offset = SC.SSR.m_nFXParamsOffset;
			for (i = 0; i < SC.SSR.m_nFXParams; i++)
			{
				SC.FXParams[i].Import(&pSrc[offset]);
				offset += sizeof(SSFXParam);
			}
		}
	}

	if (SC.SSR.m_nFXSamplers)
	{
		SC.FXTexSamplers.ReserveNoClear(SC.SSR.m_nFXSamplers);

		if (!CParserBin::m_bEndians)
		{
			memcpy(&SC.FXTexSamplers[0], &pSrc[SC.SSR.m_nFXSamplersOffset], sizeof(SSTexSamplerFX) * SC.SSR.m_nFXSamplers);
		}
		else
		{
			uint32 offset = SC.SSR.m_nFXSamplersOffset;
			for (i = 0; i < SC.SSR.m_nFXSamplers; i++)
			{
				SC.FXTexSamplers[i].Import(&pSrc[offset]);
				offset += sizeof(SSTexSamplerFX);
			}
		}
	}

	if (SC.SSR.m_nFXTexRTs)
	{
		SC.FXTexRTs.ReserveNoClear(SC.SSR.m_nFXTexRTs);

		if (!CParserBin::m_bEndians)
		{
			memcpy(&SC.FXTexRTs[0], &pSrc[SC.SSR.m_nFXTexRTsOffset], sizeof(SSHRenderTarget) * SC.SSR.m_nFXTexRTs);
		}
		else
		{
			uint32 offset = SC.SSR.m_nFXTexRTsOffset;
			for (i = 0; i < SC.SSR.m_nFXTexRTs; i++)
			{
				SC.FXTexRTs[i].Import(&pSrc[offset]);
				offset += sizeof(SSHRenderTarget);
			}
		}
	}

	if (SC.SSR.m_nTechniques)
	{
		SC.Techniques.ReserveNoClear(SC.SSR.m_nTechniques);

		if (!CParserBin::m_bEndians)
		{
			memcpy(&SC.Techniques[0], &pSrc[SC.SSR.m_nTechOffset], sizeof(SSShaderTechnique) * SC.SSR.m_nTechniques);
		}
		else
		{
			uint32 offset = SC.SSR.m_nTechOffset;
			for (i = 0; i < SC.SSR.m_nTechniques; i++)
			{
				SC.Techniques[i].Import(&pSrc[offset]);
				offset += sizeof(SSShaderTechnique);
			}
		}
	}

	if (SC.SSR.m_nPasses)
	{
		SC.Passes.ReserveNoClear(SC.SSR.m_nPasses);

		if (!CParserBin::m_bEndians)
		{
			memcpy(&SC.Passes[0], &pSrc[SC.SSR.m_nPassOffset], sizeof(SSShaderPass) * SC.SSR.m_nPasses);
		}
		else
		{
			uint32 offset = SC.SSR.m_nPassOffset;
			for (i = 0; i < SC.SSR.m_nPasses; i++)
			{
				SC.Passes[i].Import(&pSrc[offset]);
				offset += sizeof(SSShaderPass);
			}
		}
	}

	if (SC.SSR.m_nStringsSize)
	{
		SC.Strings.ReserveNoClear(SC.SSR.m_nStringsSize);
		memcpy(&SC.Strings[0], &pSrc[SC.SSR.m_nStringsOffset], SC.SSR.m_nStringsSize);
	}

	if (SC.SSR.m_nDataSize)
	{
		SC.Data.ReserveNoClear(SC.SSR.m_nDataSize);
		memcpy(&SC.Data[0], &pSrc[SC.SSR.m_nDataOffset], SC.SSR.m_nDataSize);
	}

	pRes->mfFileClose(pDE->GetName(), pDE->GetFlags());

	g_fTime1 += iTimer->GetAsyncCurTime() - fTime1;

	float fTime2 = iTimer->GetAsyncCurTime();

	pSH->m_eSHDType = SC.SSR.m_eSHDType;

	//TODO |= on flags? will we lose flags at runtime
	pSH->m_Flags = SC.SSR.m_Flags;
	pSH->m_Flags2 = SC.SSR.m_Flags2;
	pSH->m_nMDV = SC.SSR.m_nMDV;
	pSH->m_eVertexFormat = SC.SSR.m_eVertexFormat;
	pSH->m_eCull = SC.SSR.m_eCull;
	pSH->m_eShaderType = SC.SSR.m_eShaderType;
	pSH->m_nMaskGenFX = SC.SSR.m_nMaskGenFX;

	fxParams.m_PublicParams.reserve(fxParams.m_PublicParams.size() + SC.SSR.m_nPublicParams);

	for (i = 0; i < SC.SSR.m_nPublicParams; i++)
	{
		SSShaderParam& PR = SC.Params[i];
		SShaderParam P;
		const char* pName = sString(PR.m_nameIdx, SC.Strings);
		cry_strcpy(P.m_Name, pName);
		P.m_Type = PR.m_Type;
		P.m_Value = PR.m_Value;
		P.m_Script = sString(PR.m_nScriptOffs, SC.Strings);
		fxParams.m_PublicParams.push_back(P);
	}

	fxParams.m_FXParams.reserve(fxParams.m_FXParams.size() + SC.SSR.m_nFXParams);

	for (i = 0; i < SC.SSR.m_nFXParams; i++)
	{
		SFXParam fxParam;
		fxParam.Import(SC, &SC.FXParams[i]);
		fxParams.m_FXParams.push_back(fxParam);
	}

	fxParams.m_FXSamplers.reserve(fxParams.m_FXSamplers.size() + SC.SSR.m_nFXSamplers);

	for (i = 0; i < SC.SSR.m_nTechniques; i++)
	{
		SSShaderTechnique& ST = SC.Techniques[i];
		SShaderTechnique* pT = new SShaderTechnique(pSH);
		pT->m_NameStr = sString(ST.m_nNameOffs, SC.Strings);
		pT->m_NameCRC = pT->m_NameStr.c_str();
		pT->m_Flags = ST.m_Flags;
		pT->m_nPreprocessFlags = ST.m_nPreprocessFlags;
		for (j = 0; j < TTYPE_MAX; j++)
		{
			pT->m_nTechnique[j] = ST.m_nTechnique[j];
		}

		if (ST.m_nPasses)
		{
			int nOffs = ST.m_nPassesOffs;
			pT->m_Passes.reserve(ST.m_nPasses);
			for (j = 0; j < ST.m_nPasses; j++)
			{
				SSShaderPass& PS = SC.Passes[j + nOffs];
				SShaderPass* P = pT->m_Passes.AddIndex(1);
				P->m_RenderState = PS.m_RenderState;
				P->m_eCull = PS.m_eCull;
				P->m_AlphaRef = PS.m_AlphaRef;
				P->m_PassFlags = PS.m_PassFlags;

				P->m_VShader = ImportHWShader(SC, PS.m_nVShaderOffs, pSH->m_CRC32, pSH);
				P->m_PShader = ImportHWShader(SC, PS.m_nPShaderOffs, pSH->m_CRC32, pSH);
				P->m_GShader = ImportHWShader(SC, PS.m_nGShaderOffs, pSH->m_CRC32, pSH);
				P->m_HShader = ImportHWShader(SC, PS.m_nHShaderOffs, pSH->m_CRC32, pSH);
				P->m_DShader = ImportHWShader(SC, PS.m_nDShaderOffs, pSH->m_CRC32, pSH);
				P->m_CShader = ImportHWShader(SC, PS.m_nCShaderOffs, pSH->m_CRC32, pSH);
			}
		}

		uint32 nREOffset = ST.m_nREsOffs;

		for (j = 0; j < ST.m_nREs; j++)
		{
			EDataType dataType = *((EDataType*)&SC.Data[nREOffset]);

			if (CParserBin::m_bEndians)
			{
				SwapEndianEnum(dataType, eBigEndian);
			}

			nREOffset += sizeof(EDataType);

			switch (dataType)
			{
			case eDATA_LensOptics:
				{
					CRELensOptics* pLensOptics = new CRELensOptics;
					pLensOptics->mfImport(SC, nREOffset);
					pT->m_REs.push_back(pLensOptics);
				}
				break;

			default:
				CryFatalError("Render element not supported for shader serialising");
				break;
			}

			//expects 4 byte aligned
			assert(!(nREOffset & 3));
		}

		pSH->m_HWTechniques.AddElem(pT);
	}

	g_fTime2 += iTimer->GetAsyncCurTime() - fTime2;

	return bRes;
}

#endif
