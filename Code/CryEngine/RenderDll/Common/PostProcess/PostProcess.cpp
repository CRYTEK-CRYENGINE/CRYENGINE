// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcess.cpp : Post process main interface

   Revision history:
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "PostEffects.h"
#include <Cry3DEngine/I3DEngine.h>

void CParamBool::SetParam(float fParam, bool bForceValue)
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	CParamBoolThreadSafeData* pThreadSafeData = &m_threadSafeData[threadID];
	pThreadSafeData->bParam = (fParam) ? true : false;
	pThreadSafeData->bSetThisFrame = true;
}

float CParamBool::GetParam() const
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	return static_cast<float>(m_threadSafeData[threadID].bParam);
}

void CParamBool::SyncMainWithRender()
{
	CParamBoolThreadSafeData* pFillData = &m_threadSafeData[gRenDev->GetMainThreadID()];

	const bool bIsMultiThreaded = (gRenDev->m_pRT) ? (gRenDev->m_pRT->IsMultithreaded()) : false;
	CParamBoolThreadSafeData* pProcessData = NULL;
	if (bIsMultiThreaded)
	{
		// If value is set on render thread, then this should override main thread value
		pProcessData = &m_threadSafeData[gRenDev->GetRenderThreadID()];
		if (pProcessData->bSetThisFrame)
		{
			pFillData->bParam = pProcessData->bParam;
		}
	}

	// Reset set value
	pFillData->bSetThisFrame = false;

	if (bIsMultiThreaded)
	{
		// Copy fill data into process data
		memcpy(pProcessData, pFillData, sizeof(CParamBoolThreadSafeData));
	}
}

void CParamInt::SetParam(float fParam, bool bForceValue)
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	CParamIntThreadSafeData* pThreadSafeData = &m_threadSafeData[threadID];
	pThreadSafeData->nParam = static_cast<int>(fParam);
	pThreadSafeData->bSetThisFrame = true;
}

float CParamInt::GetParam() const
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	return static_cast<float>(m_threadSafeData[threadID].nParam);
}
void CParamInt::SyncMainWithRender()
{
	CParamIntThreadSafeData* pFillData = &m_threadSafeData[gRenDev->GetMainThreadID()];

	const bool bIsMultiThreaded = (gRenDev->m_pRT) ? (gRenDev->m_pRT->IsMultithreaded()) : false;
	CParamIntThreadSafeData* pProcessData = NULL;
	if (bIsMultiThreaded)
	{
		// If value is set on render thread, then this should override main thread value
		pProcessData = &m_threadSafeData[gRenDev->GetRenderThreadID()];
		if (pProcessData->bSetThisFrame)
		{
			pFillData->nParam = pProcessData->nParam;
		}
	}

	// Reset set value
	pFillData->bSetThisFrame = false;

	if (bIsMultiThreaded)
	{
		// Copy fill data into process data
		memcpy(pProcessData, pFillData, sizeof(CParamIntThreadSafeData));
	}
}

void CParamFloat::SetParam(float fParam, bool bForceValue)
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	CParamFloatThreadSafeData* pThreadSafeData = &m_threadSafeData[threadID];

	if (m_bSmoothTransition && CRenderer::CV_r_PostProcessParamsBlending)
	{
		if (!bForceValue)
		{
			float initialValue = CRenderer::CV_r_PostProcessParamsBlending == 2 ? m_fParamDefault : 0;

			if (!pThreadSafeData->nFrameSetCount)
			{
				pThreadSafeData->fFrameParamAcc = initialValue;
			}

			pThreadSafeData->fFrameParamAcc += fParam;
			if (!pThreadSafeData->nFrameSetCount && initialValue != 0)
				pThreadSafeData->nFrameSetCount += 2;    // Make sure to initialize frame set count to 2, for non 0 default values
			else
				pThreadSafeData->nFrameSetCount++;
		}
		else
		{
			pThreadSafeData->fParam = pThreadSafeData->fFrameParamAcc = fParam;
			pThreadSafeData->nFrameSetCount = 0;
			pThreadSafeData->bValueForced = true;
		}
	}
	else
	{
		pThreadSafeData->fParam = fParam;
		pThreadSafeData->bValueForced = true;
	}
}

float CParamFloat::GetParam() const
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	return m_threadSafeData[threadID].fParam;
}

void CParamFloat::SyncMainWithRender()
{
	// The Effect params can be set/get from both threads, accumulate and sync data here
	CParamFloatThreadSafeData* pFillData = &m_threadSafeData[gRenDev->GetMainThreadID()];

	const bool bIsMultiThreaded = (gRenDev->m_pRT) ? (gRenDev->m_pRT->IsMultithreaded()) : false;
	CParamFloatThreadSafeData* pProcessData = NULL;
	if (bIsMultiThreaded)
	{
		pProcessData = &m_threadSafeData[gRenDev->GetRenderThreadID()];
		if (pProcessData->nFrameSetCount)
		{
			// Add accumulated data from render thread to main thread data
			if (pFillData->nFrameSetCount)
			{
				if (m_fParamDefault != 0 && CRenderer::CV_r_PostProcessParamsBlending == 2)
				{
					pProcessData->nFrameSetCount--; // Already initialized with the extra increment for non 0 default values
				}
			}
			else
			{
				// Main thread set no value, but render thread did, so need to reset the accumulated data for main thread
				pFillData->fFrameParamAcc = 0.0f;
			}

			pFillData->nFrameSetCount += pProcessData->nFrameSetCount;
			pFillData->fFrameParamAcc += pProcessData->fFrameParamAcc;
		}

		// If value is forced on render thread, then this should override main thread value
		if (pProcessData->bValueForced)
		{
			pFillData->fParam = pProcessData->fParam;
			pFillData->fFrameParamAcc = pProcessData->fParam;
			pFillData->nFrameSetCount = 0;
			pFillData->bValueForced = true;
		}
	}

	// Interpolate accumulated data for smooth transition
	if (m_bSmoothTransition && CRenderer::CV_r_PostProcessParamsBlending)
	{
		if (pFillData->nFrameSetCount)
		{
			pFillData->fFrameParamAcc /= (float) pFillData->nFrameSetCount;
			pFillData->nFrameSetCount = 0;
		}

		float fFrameTime = clamp_tpl<float>(gEnv->pTimer->GetFrameTime() * CRenderer::CV_r_PostprocessParamsBlendingTimeScale, 0.0f, 1.0f);
		pFillData->fParam += (pFillData->fFrameParamAcc - pFillData->fParam) * fFrameTime;
	}

	// Reset forced value
	pFillData->bValueForced = false;

	if (bIsMultiThreaded)
	{
		// Copy fill data into process data
		memcpy(pProcessData, pFillData, sizeof(CParamFloatThreadSafeData));
	}
}

void CParamVec4::SetParamVec4(const Vec4& vParam, bool bForceValue)
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	CParamVec4ThreadSafeData* pThreadSafeData = &m_threadSafeData[threadID];

	if (m_bSmoothTransition && CRenderer::CV_r_PostProcessParamsBlending)
	{
		if (!bForceValue)
		{
			Vec4 initialValue = CRenderer::CV_r_PostProcessParamsBlending == 2 ? m_vParamDefault : Vec4(0, 0, 0, 0);

			if (!pThreadSafeData->nFrameSetCount)
				pThreadSafeData->vFrameParamAcc = initialValue;

			pThreadSafeData->vFrameParamAcc += vParam;

			if (!pThreadSafeData->nFrameSetCount && (initialValue.x + initialValue.y + initialValue.z + initialValue.w != 0))
				pThreadSafeData->nFrameSetCount += 2;  // Make sure to initialize frame set count to 2, for non 0 default values
			else
				pThreadSafeData->nFrameSetCount++;
		}
		else
		{
			pThreadSafeData->vParam = pThreadSafeData->vFrameParamAcc = vParam;
			pThreadSafeData->nFrameSetCount = 0;
			pThreadSafeData->bValueForced = true;
		}
	}
	else
	{
		pThreadSafeData->vParam = vParam;
		pThreadSafeData->bValueForced = true;
	}
}

Vec4 CParamVec4::GetParamVec4() const
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	return m_threadSafeData[threadID].vParam;
}

void CParamVec4::SyncMainWithRender()
{
	// The Effect params can be set/get from both threads, accumulate and sync data here
	CParamVec4ThreadSafeData* pFillData = &m_threadSafeData[gRenDev->GetMainThreadID()];

	const bool bIsMultiThreaded = (gRenDev->m_pRT) ? (gRenDev->m_pRT->IsMultithreaded()) : false;
	CParamVec4ThreadSafeData* pProcessData = NULL;
	if (bIsMultiThreaded)
	{
		pProcessData = &m_threadSafeData[gRenDev->GetRenderThreadID()];
		if (pProcessData->nFrameSetCount)
		{
			// Add accumulated data from render thread to main thread data
			if (pFillData->nFrameSetCount)
			{
				if (m_vParamDefault.x + m_vParamDefault.y + m_vParamDefault.z + m_vParamDefault.w != 0 && CRenderer::CV_r_PostProcessParamsBlending == 2)
				{
					pProcessData->nFrameSetCount--; // Already initialized with the extra increment for non 0 default values
				}
			}
			else
			{
				// Main thread set no value, but render thread did, so need to reset the accumulated data for main thread
				pFillData->vFrameParamAcc = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
			}
			pFillData->nFrameSetCount += pProcessData->nFrameSetCount;
			pFillData->vFrameParamAcc += pProcessData->vFrameParamAcc;
		}

		// If value is forced on render thread, then this should override main thread value
		if (pProcessData->bValueForced)
		{
			pFillData->vParam = pProcessData->vParam;
			pFillData->vFrameParamAcc = pProcessData->vParam;
			pFillData->nFrameSetCount = 0;
			pFillData->bValueForced = true;
		}
	}

	// Interpolate accumulated data for smooth transition
	if (m_bSmoothTransition && CRenderer::CV_r_PostProcessParamsBlending)
	{
		if (pFillData->nFrameSetCount)
		{
			pFillData->vFrameParamAcc /= (float) pFillData->nFrameSetCount;
			pFillData->nFrameSetCount = 0;
		}

		float fFrameTime = clamp_tpl<float>(gEnv->pTimer->GetFrameTime() * CRenderer::CV_r_PostprocessParamsBlendingTimeScale, 0.0f, 1.0f);
		pFillData->vParam += (pFillData->vFrameParamAcc - pFillData->vParam) * fFrameTime;
	}

	// Reset forced value
	pFillData->bValueForced = false;

	if (bIsMultiThreaded)
	{
		// Copy fill data into process data
		memcpy(pProcessData, pFillData, sizeof(CParamVec4ThreadSafeData));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(_RELEASE)

static void SetPostEffectParamF(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() < 3)
	{
		return;
	}

	bool bForceValue = false;
	if (pArgs->GetArgCount() > 3)
	{
		const int iForceValue = (int)atoi(pArgs->GetArg(3));
		if (iForceValue != 0)
		{
			bForceValue = true;
		}
	}

	const char* szPostEffectParamName = pArgs->GetArg(1);
	const float fValue = (float)atof(pArgs->GetArg(2));

	I3DEngine* pEngine = gEnv->p3DEngine;
	pEngine->SetPostEffectParam(szPostEffectParamName, fValue, bForceValue);
}

static void GetPostEffectParamF(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() < 2)
	{
		return;
	}

	const char* szPostEffectParamName = pArgs->GetArg(1);

	I3DEngine* pEngine = gEnv->p3DEngine;
	float fValue = 0.0f;
	pEngine->GetPostEffectParam(szPostEffectParamName, fValue);
	CryLogAlways("\nPost effect param value: %f", fValue);
}

#endif // !defined(_RELEASE)

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CPostEffectsMgr::Init()
{
	m_bPostReset = false;
	ClearCache();

	// Initialize the CRC table
	// This is the official polynomial used by CRC-32
	// in PKZip, WinZip and Ethernet.
	unsigned int ulPolynomial = 0x04c11db7;

	// 256 values representing ASCII character codes.
	for (int i = 0; i <= 0xFF; i++)
	{
		m_nCRC32Table[i] = CRC32Reflect(i, 8) << 24;
		for (int j = 0; j < 8; j++)
			m_nCRC32Table[i] = (m_nCRC32Table[i] << 1) ^ (m_nCRC32Table[i] & (1 << 31) ? ulPolynomial : 0);
		m_nCRC32Table[i] = CRC32Reflect(m_nCRC32Table[i], 32);
	} //i

	// Register default parameters
	// Note: These parameters do not affect the result
	// AddParamFloat("Global_Brightness", m_pBrightness, 1.0f); // brightness
	// AddParamFloat("Global_Contrast", m_pContrast, 1.0f);  // contrast
	// AddParamFloat("Global_Saturation", m_pSaturation, 1.0f);  // saturation

	// AddParamFloat("Global_ColorC", m_pColorC, 0.0f);  // cyan amount
	// AddParamFloat("Global_ColorY", m_pColorY, 0.0f);  // yellow amount
	// AddParamFloat("Global_ColorM", m_pColorM, 0.0f);  // magenta amount
	// AddParamFloat("Global_ColorK", m_pColorK, 0.0f);  // darkness amount

	// AddParamFloat("Global_ColorHue", m_pColorHue, 0.0f);  // image hue rotation

	// User parameters
	AddParamFloat("Global_User_Brightness", m_pUserBrightness, 1.0f); // brightness
	AddParamFloat("Global_User_Contrast", m_pUserContrast, 1.0f);     // contrast
	AddParamFloat("Global_User_Saturation", m_pUserSaturation, 1.0f); // saturation

	AddParamFloat("Global_User_ColorC", m_pUserColorC, 0.0f);  // cyan amount
	AddParamFloat("Global_User_ColorY", m_pUserColorY, 0.0f);  // yellow amount
	AddParamFloat("Global_User_ColorM", m_pUserColorM, 0.0f);  // magenta amount
	AddParamFloat("Global_User_ColorK", m_pUserColorK, 0.0f);  // darkness amount

	AddParamFloat("Global_User_ColorHue", m_pUserColorHue, 0.0f);  // image hue rotation

	// Register all post processes
	AddEffect(CSunShafts);
	AddEffect(CDepthOfField);
	AddEffect(CMotionBlur);
	AddEffect(CUnderwaterGodRays);
	AddEffect(CVolumetricScattering);
	AddEffect(CRainDrops);
	AddEffect(CWaterDroplets);
	AddEffect(CWaterFlow);
	AddEffect(CScreenFrost);
	AddEffect(CAlienInterference);
	AddEffect(CFlashBang);
	AddEffect(CSharpening);
	AddEffect(CBlurring);
	AddEffect(CColorGrading);
	AddEffect(CNightVision);
	AddEffect(CHudSilhouettes);
	AddEffect(CSonarVision);
	AddEffect(CThermalVision);
	AddEffect(CImageGhosting);
	AddEffect(CPostAA);
	AddEffect(CPostStereo);
	AddEffect(CHud3D);
	AddEffect(CFilterKillCamera);
	AddEffect(CNanoGlass);
	AddEffect(CUberGamePostProcess);
	AddEffect(CScreenBlood);
	AddEffect(CScreenFader);
	AddEffect(CPost3DRenderer);

	// Sort all post effects by ID
	std::sort(m_pEffects.begin(), m_pEffects.end(), SortEffectsByID);

	// Initialize all post process techniques
	std::for_each(m_pEffects.begin(), m_pEffects.end(), SContainerPostEffectInitialize());
	m_bCreated = false;

	// Initialize parameters
	StringEffectMapItor pItor = m_pNameIdMapGen.begin(), pEnd = m_pNameIdMapGen.end();
	for (; pItor != pEnd; ++pItor)
	{
		m_pNameIdMap.insert(KeyEffectMapItor::value_type(GetCRC(pItor->first.c_str()), pItor->second));
	}

	m_pNameIdMapGen.clear();

#if !defined(_RELEASE)
	REGISTER_COMMAND("r_setposteffectparamf", SetPostEffectParamF, VF_CHEAT, "Sets post effect param (float)n"
	                                                                         "Usage: r_setposteffectparamf [posteffectparamname, value, forceValue(OPTIONAL)]\n"
	                                                                         "Example: r_setposteffectparamf HUD3D_FOV 35.0	 (Doesn't force value)\n"
	                                                                         "Example: r_setposteffectparamf HUD3D_FOV 35.0 1 (Forces value)\n");

	REGISTER_COMMAND("r_getposteffectparamf", GetPostEffectParamF, VF_CHEAT, "Outputs post effect param value (float) to log"
	                                                                         "Usage: r_setposteffectparamf [posteffectparamname]\n"
	                                                                         "Example: r_getposteffectparamf HUD3D_FOV\n");
#endif

	// TESTING
	static int r_3MonHack;
	static float r_3MonHackHUDFOVX;
	static float r_3MonHackHUDFOVY;
	static float r_3MonHackLeftCGFOffsetX;
	static float r_3MonHackRightCGFOffsetX;

	if (!gEnv->pConsole->GetCVar("r_3MonHack"))
	{
		REGISTER_CVAR(r_3MonHack, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Enables 3 monitor hack hud in center");
		REGISTER_CVAR(r_3MonHackHUDFOVX, 28, VF_CHEAT | VF_CHEAT_NOCHECK, "3 monitor hack hud in center - X FOV");
		REGISTER_CVAR(r_3MonHackHUDFOVY, 60, VF_CHEAT | VF_CHEAT_NOCHECK, "3 monitor hack hud in center - Y FOV");
		REGISTER_CVAR(r_3MonHackLeftCGFOffsetX, 0.93f, VF_CHEAT | VF_CHEAT_NOCHECK, "3 monitor hack hud in center - Adds position offset in X direction to all left CGF planes");
		REGISTER_CVAR(r_3MonHackRightCGFOffsetX, -0.93f, VF_CHEAT | VF_CHEAT_NOCHECK, "3 monitor hack hud in center - Adds position offset in X direction to all right CGF planes");
	}
	return 1;
}

void CPostEffectsMgr::CreateResources()
{
	// Initialize all post process techniques
	if (!m_bCreated)
		std::for_each(m_pEffects.begin(), m_pEffects.end(), SContainerPostEffectCreateResources());
	m_bCreated = true;
}
void CPostEffectsMgr::ReleaseResources()
{
	if (m_bCreated)
		std::for_each(m_pEffects.begin(), m_pEffects.end(), container_object_safe_release());

#ifndef _RELEASE
	ClearDebugInfo();
#endif

	m_bCreated = false;
}

void CPostEffectsMgr::Release()
{
	// Free all resources
	ClearCache();

	std::for_each(m_pNameIdMap.begin(), m_pNameIdMap.end(), SContainerKeyEffectParamDelete());
	m_pNameIdMap.clear();

	std::for_each(m_pEffects.begin(), m_pEffects.end(), container_object_safe_release());
	std::for_each(m_pEffects.begin(), m_pEffects.end(), container_object_safe_delete());
	m_pEffects.clear();

	m_bCreated = false;

#if !defined(_RELEASE)
	IConsole* pConsole = gEnv->pConsole;
	if (pConsole)
	{
		pConsole->RemoveCommand("r_setposteffectparamf");
		pConsole->RemoveCommand("r_getposteffectparamf");
	}
#endif
}

void CPostEffectsMgr::Reset(bool bOnSpecChange)
{
	ClearCache();

	// Note: These parameters do not affect the result
	// m_pBrightness->ResetParam(1.0f);
	// m_pContrast->ResetParam(1.0f);
	// m_pSaturation->ResetParam(1.0f);
	// m_pColorC->ResetParam(0.0f);
	// m_pColorY->ResetParam(0.0f);
	// m_pColorM->ResetParam(0.0f);
	// m_pColorK->ResetParam(0.0f);
	// m_pColorHue->ResetParam(0.0f);
	m_pUserBrightness->ResetParam(1.0f);
	m_pUserContrast->ResetParam(1.0f);
	m_pUserSaturation->ResetParam(1.0f);
	m_pUserColorC->ResetParam(0.0f);
	m_pUserColorY->ResetParam(0.0f);
	m_pUserColorM->ResetParam(0.0f);
	m_pUserColorK->ResetParam(0.0f);
	m_pUserColorHue->ResetParam(0.0f);

	if (bOnSpecChange)
		std::for_each(m_pEffects.begin(), m_pEffects.end(), SContainerPostEffectResetOnSpecChange());
	else
		std::for_each(m_pEffects.begin(), m_pEffects.end(), SContainerPostEffectReset());
}

EPostEffectID CPostEffectsMgr::GetEffectID(const char* pEffectName)
{
	EPostEffectID effectID = EPostEffectID::Invalid;

	CPostEffectsMgr* pPostMgr = PostEffectMgr();
	for (CPostEffectItor pItor = pPostMgr->GetEffects().begin(), pItorEnd = pPostMgr->GetEffects().end(); pItor != pItorEnd; ++pItor)
	{
		CPostEffect* pCurrEffect = (*pItor);
		if (strcmp(pEffectName, pCurrEffect->GetName()) == 0)
		{
			effectID = pCurrEffect->GetID();
			break;
		}
	}

	return effectID;
}

void CPostEffectsMgr::OnLostDevice()
{
	std::for_each(m_pEffects.begin(), m_pEffects.end(), SContainerPostEffectOnLostDevice());
}

void CPostEffectsMgr::OnBeginFrame(const SRenderingPassInfo& passInfo)
{
	std::for_each(m_pEffects.begin(), m_pEffects.end(), SContainerPostEffectOnBeginFrame(passInfo));
}

void CPostEffectsMgr::SyncMainWithRender()
{
	KeyEffectMapItor iter;
	for (iter = m_pNameIdMap.begin(); iter != m_pNameIdMap.end(); iter++)
	{
		iter->second->SyncMainWithRender();
	}
}

// Used only when creating the crc table
uint32 CPostEffectsMgr::CRC32Reflect(uint32 ref, char ch)
{
	uint32 value = 0;

	// Swap bit 0 for bit 7
	// bit 1 for bit 6, etc.
	for (int i = 1; i < (ch + 1); i++)
	{
		if (ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}

uint32 CPostEffectsMgr::GetCRC(const char* pszName)
{
	if (!pszName)
	{
		assert(false && "CPostEffectsMgr::GetCRC() invalid string passed");
		return 0;
	}

	// Once the lookup table has been filled in by the constructor,
	// this function creates all CRCs using only the lookup table.
	// Be sure to use unsigned variables, because negative values introduce high bits where zero bits are required.

	const char* szPtr = pszName;
	uint32 ulCRC = 0xffffffff; // Start out with all bits set high.
	unsigned char c;

	while (*szPtr)
	{
		c = *szPtr++;
		if (c >= 'a' && c <= 'z')
			c -= 32; //convert to uppercase
		ulCRC = (ulCRC >> 8) ^ m_nCRC32Table[(ulCRC & 0xFF) ^ c];
	}

	// Exclusive OR the result with the beginning value...avoids the % operator
	return ulCRC ^ 0xffffffff;

}

CEffectParam* CPostEffectsMgr::GetByName(const char* pszParam)
{
	CRY_ASSERT_MESSAGE(pszParam, "mfGetByName: null FX name");

	uint32 nCurrKey = GetCRC(pszParam);

	int nThreadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;

	// cache per-thread
	ParamCache* pCache = &m_pParamCache[nThreadID];

	// Check cache first
	if (nCurrKey == pCache->m_nKey && pCache->m_pParam)
	{
		if (CRenderer::CV_r_PostProcess == 3)
			m_pEffectParamsUpdated.insert(StringEffectMapItor::value_type(pszParam, pCache->m_pParam));

		return pCache->m_pParam;
	}

	KeyEffectMapItor pItor = m_pNameIdMap.find(nCurrKey);

	if (pItor != m_pNameIdMap.end())
	{
		pCache->m_pParam = pItor->second;
		pCache->m_nKey = nCurrKey;

		if (CRenderer::CV_r_PostProcess == 3)
			m_pEffectParamsUpdated.insert(StringEffectMapItor::value_type(pszParam, pCache->m_pParam));

		return pCache->m_pParam;
	}

	return 0;
}

float CPostEffectsMgr::GetByNameF(const char* pszParam)
{
	CEffectParam* pParam = GetByName(pszParam);
	if (pParam)
	{
		return pParam->GetParam();
	}

	return 0.0f;
}

Vec4 CPostEffectsMgr::GetByNameVec4(const char* pszParam)
{
	CEffectParam* pParam = GetByName(pszParam);
	if (pParam)
	{
		return pParam->GetParamVec4();
	}

	return Vec4(0, 0, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CPostEffectsMgr::SortEffectsByID(const CPostEffect* p1, const CPostEffect* p2)
{
	return (p1->GetID() < p2->GetID());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CParamTexture::Create(const char* pszFileName)
{
	if (!pszFileName)
	{
		return 0;
	}

	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	CParamTextureThreadSafeData* pThreadSafeData = &m_threadSafeData[threadID];

	if (pThreadSafeData->pTexParam)
	{
		// check if texture is same
		if (!strcmpi(pThreadSafeData->pTexParam->GetName(), pszFileName))
		{
			return 0;
		}
		// release texture if required
		SAFE_RELEASE(pThreadSafeData->pTexParam);
	}

	pThreadSafeData->pTexParam = CTexture::ForName(pszFileName, FT_DONT_STREAM, eTF_Unknown);
	pThreadSafeData->bSetThisFrame = true;
	CRY_ASSERT_MESSAGE(pThreadSafeData->pTexParam, "CParamTexture.Create: texture not found!");

	return 1;
}
const char* CParamTexture::GetParamString() const
{
	const int threadID = gRenDev->m_pRT ? gRenDev->m_pRT->GetThreadList() : 0;
	return m_threadSafeData[threadID].pTexParam->GetName();
}

void CParamTexture::Release()
{
	CParamTextureThreadSafeData* pFillData = &m_threadSafeData[gRenDev->GetMainThreadID()];

	const bool bIsMultiThreaded = (gRenDev->m_pRT) ? (gRenDev->m_pRT->IsMultithreaded()) : false;
	if (bIsMultiThreaded)
	{
		CParamTextureThreadSafeData* pProcessData = &m_threadSafeData[gRenDev->GetRenderThreadID()];
		if (pProcessData->pTexParam != pFillData->pTexParam)
		{
			SAFE_RELEASE(pProcessData->pTexParam);
			pProcessData->bSetThisFrame = true;
		}
	}

	SAFE_RELEASE(pFillData->pTexParam);
	pFillData->bSetThisFrame = true;
}

void CParamTexture::SyncMainWithRender()
{
	CParamTextureThreadSafeData* pFillData = &m_threadSafeData[gRenDev->GetMainThreadID()];

	const bool bIsMultiThreaded = (gRenDev->m_pRT) ? (gRenDev->m_pRT->IsMultithreaded()) : false;
	CParamTextureThreadSafeData* pProcessData = NULL;
	if (bIsMultiThreaded)
	{
		// If value is set on render thread, then this should override main thread value
		pProcessData = &m_threadSafeData[gRenDev->GetRenderThreadID()];
		if (pProcessData->bSetThisFrame)
		{
			if (pFillData->bSetThisFrame)
			{
				// If the main thread also set a texture on the same frame (highly unlikely), then release this texture
				// 1st before overriding it with the texture set from the render thread
				SAFE_RELEASE(pFillData->pTexParam);
			}
			pFillData->pTexParam = pProcessData->pTexParam;
		}
	}

	// Reset set value
	pFillData->bSetThisFrame = false;

	if (bIsMultiThreaded)
	{
		// Copy fill data into process data
		memcpy(pProcessData, pFillData, sizeof(CParamTextureThreadSafeData));
	}
}
