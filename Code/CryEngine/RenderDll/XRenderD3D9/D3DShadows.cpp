// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DShadows.cpp : shadows support.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <CryEntitySystem/IEntityRenderState.h>
#include "../Common/Shadow_Renderer.h"
#include "../Common/ReverseDepth.h"
#include "D3DPostProcess.h"

#include <Cry3DEngine/I3DEngine.h>

#include "GraphicsPipeline/Common/GraphicsPipelineStage.h"

#include "Common/RenderView.h"

#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#if CRY_PLATFORM_DURANGO
	#pragma warning(push)
	#pragma warning(disable : 4273)
BOOL InflateRect(LPRECT lprc, int dx, int dy)
{
	if (lprc == NULL)
		return FALSE;

	lprc->left -= dx;
	lprc->right += dx;
	lprc->top -= dy;
	lprc->bottom += dy;

	return TRUE;
}
	#pragma warning(pop)
#endif

namespace
{
CryCriticalSection g_cDynTexLock;
}

void CD3D9Renderer::EF_PrepareShadowGenRenderList(CRenderView* pRenderView)
{
	FUNCTION_PROFILER_RENDERER
	//if (CV_r_UseShadowsPool)
	//  return;

	int NumDynLights = pRenderView->GetDynamicLightsCount();

	auto& arrDeferLights = pRenderView->GetLightsArray(eDLT_DeferredLight);

	if (NumDynLights <= 0 && arrDeferLights.size() <= 0)
		return;

	int nSunID = -1;
	for (int nLightID = 0; nLightID < NumDynLights; nLightID++)
	{
		SRenderLight* pLight = &pRenderView->GetDynamicLight(nLightID);
		EF_PrepareShadowGenForLight(pRenderView, pLight, nLightID);

		if (pLight->m_Flags & DLF_SUN)
			nSunID = nLightID;
	}

	for (uint32 nDeferLightID = 0; nDeferLightID < arrDeferLights.size(); nDeferLightID++)
	{
		SRenderLight* pLight = &arrDeferLights[nDeferLightID];
		EF_PrepareShadowGenForLight(pRenderView, pLight, (NumDynLights + nDeferLightID));
	}

	// add custom frustums
	if (nSunID >= 0)
	{
		ShadowMapFrustum** arrCustomFrustums;
		int nFrustumCount;
		gEnv->p3DEngine->GetCustomShadowMapFrustums(arrCustomFrustums, nFrustumCount);
		SRenderLight* pSun = &pRenderView->GetDynamicLight(nSunID);

		for (uint32 i = 0; i < nFrustumCount; ++i)
		{
			ShadowMapFrustum* pCurFrustum = arrCustomFrustums[i];
			CRY_ASSERT(pCurFrustum && pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_PerObject);

			if (PrepareShadowGenForFrustum(pRenderView, pCurFrustum, pSun, nSunID, 0))
			{
				ShadowMapFrustumPtr pFrustumForRenderer = pCurFrustum->Clone();
				CRenderView* pShadowView = pCurFrustum->GetNextAvailableShadowsView(pRenderView, pFrustumForRenderer.get());

				pRenderView->AddShadowFrustumToRender(SShadowFrustumToRender(pFrustumForRenderer, pSun, nSunID, pShadowView));
			}
		}
	}

	// Render All frustums.
	SShadowRenderer::RenderFrustumsToView(pRenderView);
}

bool CD3D9Renderer::EF_PrepareShadowGenForLight(CRenderView* pRenderView, SRenderLight* pLight, int nLightID)
{

	assert((unsigned int) nLightID < (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS));
	if ((unsigned int) nLightID >= (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS))
	{
		//Warning("CRenderer::EF_PrepareShadowGenForLight: Too many light sources used ..."); // redundant
		return false;
	}

	if (!(pLight->m_Flags & DLF_CASTSHADOW_MAPS))
		return false;

	ShadowMapFrustum** ppSMFrustumList = pLight->m_pShadowMapFrustums;
	//assert( ppSMFrustumList != 0); fail with differed lights
	if (!ppSMFrustumList)
		return false;

	int32 nCurLOD = 0;//max(nFrustumLOD, nCaster);

	for (int nCaster = 0; *ppSMFrustumList && nCaster != MAX_GSM_LODS_NUM; ++ppSMFrustumList, ++nCaster)
	{
		ShadowMapFrustum* pCurFrustum = (*ppSMFrustumList);
		//use pools
		if (CV_r_UseShadowsPool && pLight->m_Flags & DLF_DEFERRED_LIGHT)
			pCurFrustum->bUseShadowsPool = true;
		else
			pCurFrustum->bUseShadowsPool = false;

		if (PrepareShadowGenForFrustum(pRenderView, pCurFrustum, pLight, nLightID, nCurLOD))
		{
			ShadowMapFrustumPtr pFrustumForRenderer = pCurFrustum->Clone();
			CRenderView* pShadowView = pCurFrustum->GetNextAvailableShadowsView(pRenderView, pFrustumForRenderer.get());

			pRenderView->AddShadowFrustumToRender(SShadowFrustumToRender(pFrustumForRenderer, pLight, nLightID, pShadowView));
			nCurLOD++;
		}
	}

	return true;
}

bool CD3D9Renderer::PrepareShadowGenForFrustum(CRenderView* pRenderView, ShadowMapFrustum* pCurFrustum, SRenderLight* pLight, int nLightID, int nLOD)
{
	int nThreadID = m_RP.m_nFillThreadID;

	PROFILE_FRAME(PrepareShadowGenForFrustum);

	//validate shadow frustum
	assert(pCurFrustum);
	if (!pCurFrustum)
		return false;
	if (pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
		return true;
	if (pCurFrustum->castersList.Count() <= 0 && pCurFrustum->jobExecutedCastersList.Count() <= 0 &&
	    !pCurFrustum->IsCached() && pCurFrustum->m_eFrustumType != ShadowMapFrustum::e_GsmDynamicDistance)
		return false;
	if (pCurFrustum->IsCached() && pCurFrustum->nTexSize == 0)
		return false;
	//////////////////////////////////////////////////////////////////////////

	//regenerate on reset device
	if (pCurFrustum->nResetID != m_nFrameReset)
	{
		pCurFrustum->nResetID = m_nFrameReset;
		pCurFrustum->RequestUpdate();
	}

	int nShadowGenGPU = 0;

	if (GetActiveGPUCount() > 1 && CV_r_ShadowGenMode == 1)
	{
		//TOFIx: make m_nFrameSwapID - double buffered
		nShadowGenGPU = gRenDev->RT_GetCurrGpuID();

		pCurFrustum->nOmniFrustumMask = 0x3F;
		//in case there was switch on the fly - regenerate all faces
		if (pCurFrustum->nInvalidatedFrustMask[nShadowGenGPU] > 0)
			pCurFrustum->nInvalidatedFrustMask[nShadowGenGPU] = 0x3F;
	}

	SDynTexture_Shadow* pTX = NULL;

	bool bNotNeedUpdate = false;
	if (pCurFrustum->bOmniDirectionalShadow)
		bNotNeedUpdate = !(pCurFrustum->nInvalidatedFrustMask[nShadowGenGPU] & pCurFrustum->nOmniFrustumMask);
	else
		bNotNeedUpdate = !pCurFrustum->isUpdateRequested(nShadowGenGPU);

	if (bNotNeedUpdate && !pCurFrustum->bUseShadowsPool)
	{
		memset(pCurFrustum->nShadowGenID[nThreadID], 0xFF, sizeof(pCurFrustum->nShadowGenID[nThreadID]));
		return pCurFrustum->nShadowGenMask != 0;
	}

	if (pCurFrustum->bUseShadowsPool)
	{
		pCurFrustum->nShadowPoolUpdateRate = min<uint8>(CRenderer::CV_r_ShadowPoolMaxFrames, pCurFrustum->nShadowPoolUpdateRate);
		if (!bNotNeedUpdate)
		{
			pCurFrustum->nShadowPoolUpdateRate >>= 2;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//  update is requested - we should generate new shadow map
	//////////////////////////////////////////////////////////////////////////
	//force unwrap frustum
	if (pCurFrustum->bOmniDirectionalShadow)
		pCurFrustum->bUnwrapedOmniDirectional = true;
	else
		pCurFrustum->bUnwrapedOmniDirectional = false;

	pCurFrustum->bUseHWShadowMap = false;

	ETEX_Type eTT = (pCurFrustum->bOmniDirectionalShadow && !pCurFrustum->bUnwrapedOmniDirectional) ? eTT_Cube : eTT_2D;

	//////////////////////////////////////////////////////////////////////////
	//recalculate LOF rendering params
	//////////////////////////////////////////////////////////////////////////

	//calc texture resolution and frustum resolution
	pCurFrustum->nTexSize = max(pCurFrustum->nTexSize, 32);

	pCurFrustum->nTextureWidth = pCurFrustum->nTexSize;
	pCurFrustum->nTextureHeight = pCurFrustum->nTexSize;
	pCurFrustum->nShadowMapSize = pCurFrustum->nTexSize;

	if (pCurFrustum->bUnwrapedOmniDirectional)
	{
		pCurFrustum->nTextureWidth = pCurFrustum->nTexSize * 3;
		pCurFrustum->nTextureHeight = pCurFrustum->nTexSize * 2;
	}

	//////////////////////////////////////////////////////////////////////////
	//Select shadow buffers format
	//////////////////////////////////////////////////////////////////////////
	ETEX_Format eTF = eTF_D32F; //eTF_D24S8;  //fallback formats
	if (pCurFrustum->IsCached())
	{
		eTF = CV_r_ShadowsCacheFormat == 0 ? eTF_D32F : eTF_D16;
	}
	else if (CV_r_shadowtexformat == 0)
	{
		eTF = eTF_D32F;//eTF_D24S8;
	}
	else if (CV_r_shadowtexformat == 1)
	{
		eTF = eTF_D16;
	}
	else
	{
		eTF = eTF_D24S8;
	}

	pCurFrustum->bUseHWShadowMap = true;
	if (pCurFrustum->bOmniDirectionalShadow && !(pCurFrustum->bUnwrapedOmniDirectional))
	{
		pCurFrustum->bHWPCFCompare = false;
	}
	else
	{
		const bool bSun = pLight && (pLight->m_Flags & DLF_SUN) != 0;
		pCurFrustum->bHWPCFCompare = !bSun || (CV_r_ShadowsPCFiltering != 0);
	}

	//depth shift for precision increasing
	if (!pCurFrustum->bUseHWShadowMap && pCurFrustum->fNearDist > 1000.f)   //check for sun
	{
		pCurFrustum->bNormalizedDepth = false;
	}
	else
	{
		pCurFrustum->bNormalizedDepth = true;
	}

	//////////////////////////////////////////////////////////////////////////

	//assign requested texture format
	pCurFrustum->m_eReqTF = eTF;
	pCurFrustum->m_eReqTT = eTT;

	//assign owners
	pCurFrustum->pFrustumOwner = pCurFrustum;

	//////////////////////////////////////////////////////////////////////////

	//enable view space shadow maps
	static ICVar* p_e_gsm_view_dependent = iConsole->GetCVar("e_GsmViewSpace");
	bool bViewSpace = (p_e_gsm_view_dependent->GetIVal() > 0) && pCurFrustum->bAllowViewDependency;

	//actual view camera position
	Vec3 vCamOrigin = iSystem->GetViewCamera().GetPosition();

	CCamera tmpCamera;

	int nSides = 1;
	if (pCurFrustum->bOmniDirectionalShadow)
		nSides = OMNI_SIDES_NUM;

	// Static shadow map might not have any active casters, so don't reset nShadowGenMask every frame
	if (!pCurFrustum->IsCached())
		pCurFrustum->nShadowGenMask = pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance ? 1 : 0;
	Matrix44 m;

	for (int nS = 0; nS < nSides; nS++)
	{
		//update check for shadow frustums
		if (pCurFrustum->bOmniDirectionalShadow && !pCurFrustum->bUseShadowsPool)
		{
			if (!((pCurFrustum->nInvalidatedFrustMask[nShadowGenGPU] & pCurFrustum->nOmniFrustumMask) & (1 << nS)))
			{
				continue;
			}
			else
			{
				pCurFrustum->nInvalidatedFrustMask[nShadowGenGPU] &= ~(1 << nS);
			}
		}
		else
		{
			pCurFrustum->nInvalidatedFrustMask[nShadowGenGPU] = 0;
		}

		//////////////////////////////////////////////////////////////////////////
		// Calc frustum CCamera for current frustum
		//////////////////////////////////////////////////////////////////////////
		if (!pCurFrustum->bOmniDirectionalShadow)
		{
			if (pCurFrustum->m_Flags & (DLF_PROJECT | DLF_AREA_LIGHT))
			{
				SRenderLight instLight = *pLight;
				if (pLight->m_Flags & DLF_AREA_LIGHT)
				{
					// Pull the shadow frustum back to encompas the area of the light source.
					float fMaxSize = max(pLight->m_fAreaWidth, pLight->m_fAreaHeight);
					float fScale = fMaxSize / max(tanf(DEG2RAD(pLight->m_fLightFrustumAngle)), 0.0001f);

					Vec3 vOffsetDir = fScale * pLight->m_ObjMatrix.GetColumn0().GetNormalized();
					instLight.SetPosition(instLight.m_Origin - vOffsetDir);
					instLight.m_fProjectorNearPlane = fScale;
				}

				// Frustum angle is clamped to prevent projection matrix problems.
				// We clamp here because area lights and non-shadow casting lights can cast 180 degree light.
				CShadowUtils::GetCubemapFrustumForLight(&instLight, 0, min(2 * pLight->m_fLightFrustumAngle, 175.0f), &(pCurFrustum->mLightProjMatrix), &(pCurFrustum->mLightViewMatrix), false);
				//TF enable linear shadow space and disable this back faces for projectors
				//m_RP.m_TI.m_PersFlags |= RBPF_MIRRORCULL;
			}
			else
			{
				if (pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_PerObject)
				{
					AABB lsBounds = CShadowUtils::GetShadowMatrixForCasterBox(pCurFrustum->mLightProjMatrix, pCurFrustum->mLightViewMatrix, pCurFrustum, 20.f);

					// normalize filter filter kernel size to dimensions of light space bounding box
					pCurFrustum->fWidthS *= pCurFrustum->nTextureWidth / (lsBounds.max.x - lsBounds.min.x);
					pCurFrustum->fWidthT *= pCurFrustum->nTextureHeight / (lsBounds.max.y - lsBounds.min.y);
				}
				else if (pCurFrustum->m_eFrustumType != ShadowMapFrustum::e_HeightMapAO)
				{
					CShadowUtils::GetShadowMatrixOrtho(pCurFrustum->mLightProjMatrix, pCurFrustum->mLightViewMatrix, m_CameraMatrix, pCurFrustum, bViewSpace);
				}
			}

			//Pre-multiply matrices
			Matrix44 mViewProj = Matrix44(pCurFrustum->mLightViewMatrix) * Matrix44(pCurFrustum->mLightProjMatrix);
			pCurFrustum->mLightViewMatrix = mViewProj;
			pCurFrustum->mLightProjMatrix.SetIdentity();
		}

		//////////////////////////////////////////////////////////////////////////
		// Invoke IRenderNode::Render Jobs
		//////////////////////////////////////////////////////////////////////////
		uint32 nShadowGenID = m_nShadowGenId[nThreadID];
		m_nShadowGenId[nThreadID] += 1;

		// Mark this side of frustum needs rendering.
		// Rendering all all frustums will be done seprately later.
		pCurFrustum->nShadowGenID[nThreadID][nS] = nShadowGenID;
	}//nSides

	return true;
}

//-----------------------------------------------------------------------------
// Name: GetTextureRect()
// Desc: Get the dimensions of the texture
//-----------------------------------------------------------------------------
HRESULT GetTextureRect(CTexture* pTexture, RECT* pRect)
{
	pRect->left = 0;
	pRect->top = 0;
	pRect->right = pTexture->GetWidth();
	pRect->bottom = pTexture->GetHeight();

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_GaussBlur5x5
// Desc: Get the texture coordinate offsets to be used inside the GaussBlur5x5
//       pixel shader.
//-----------------------------------------------------------------------------

HRESULT GetSampleOffsets_GaussBlur5x5(DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, Vec4* avTexCoordOffset, Vec4* avSampleWeight, FLOAT fMultiplier)
{
	float tu = 1.0f / (float)dwD3DTexWidth;
	float tv = 1.0f / (float)dwD3DTexHeight;

	Vec4 vWhite(1.0f, 1.0f, 1.0f, 1.0f);

	float totalWeight = 0.0f;
	int index = 0;
	for (int x = -2; x <= 2; x++)
	{
		for (int y = -2; y <= 2; y++)
		{
			// Exclude pixels with a block distance greater than 2. This will
			// create a kernel which approximates a 5x5 kernel using only 13
			// sample points instead of 25; this is necessary since 2.0 shaders
			// only support 16 texture grabs.
			if (abs(x) + abs(y) > 2)
				continue;

			// Get the unscaled Gaussian intensity for this offset
			avTexCoordOffset[index] = Vec4(x * tu, y * tv, 0, 1);
			avSampleWeight[index] = vWhite * PostProcessUtils().GaussianDistribution2D((float)x, (float)y, 1.0f);
			totalWeight += avSampleWeight[index].x;

			index++;
		}
	}

	// Divide the current weight by the total weight of all the samples; Gaussian
	// blur kernels add to 1.0f to ensure that the intensity of the image isn't
	// changed when the blur occurs. An optional multiplier variable is used to
	// add or remove image intensity during the blur.
	for (int i = 0; i < index; i++)
	{
		avSampleWeight[i] /= totalWeight;
		avSampleWeight[i] *= fMultiplier;
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetTextureCoords()
// Desc: Get the texture coordinates to use when rendering into the destination
//       texture, given the source and destination rectangles
//-----------------------------------------------------------------------------

HRESULT GetTextureCoords(CTexture* pTexSrc, RECT* pRectSrc, CTexture* pTexDest, RECT* pRectDest, CoordRect* pCoords)
{
	float tU, tV;

	// Validate arguments
	if (pTexSrc == NULL || pTexDest == NULL || pCoords == NULL)
		return E_INVALIDARG;

	// Start with a default mapping of the complete source surface to complete
	// destination surface
	pCoords->fLeftU = 0.0f;
	pCoords->fTopV = 0.0f;
	pCoords->fRightU = 1.0f;
	pCoords->fBottomV = 1.0f;

	// If not using the complete source surface, adjust the coordinates
	if (pRectSrc != NULL)
	{
		// These delta values are the distance between source texel centers in
		// texture address space
		tU = 1.0f / pTexSrc->GetWidth();
		tV = 1.0f / pTexSrc->GetHeight();

		pCoords->fLeftU += pRectSrc->left * tU;
		pCoords->fTopV += pRectSrc->top * tV;
		pCoords->fRightU -= (pTexSrc->GetWidth() - pRectSrc->right) * tU;
		pCoords->fBottomV -= (pTexSrc->GetHeight() - pRectSrc->bottom) * tV;
	}

	// If not drawing to the complete destination surface, adjust the coordinates
	if (pRectDest != NULL)
	{
		// These delta values are the distance between source texel centers in
		// texture address space
		tU = 1.0f / pTexDest->GetWidth();
		tV = 1.0f / pTexDest->GetHeight();

		pCoords->fLeftU -= pRectDest->left * tU;
		pCoords->fTopV -= pRectDest->top * tV;
		pCoords->fRightU += (pTexDest->GetWidth() - pRectDest->right) * tU;
		pCoords->fBottomV += (pTexDest->GetHeight() - pRectDest->bottom) * tV;
	}

	return S_OK;
}

void CD3D9Renderer::OnEntityDeleted(IRenderNode* pRenderNode)
{
	m_pRT->RC_EntityDelete(pRenderNode);
}

void _DrawText(ISystem* pSystem, int x, int y, const float fScale, const char* format, ...)
PRINTF_PARAMS(5, 6);

void _DrawText(ISystem* pSystem, int x, int y, const float fScale, const char* format, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	float color[4] = { 0, 1, 0, 1 };
	gEnv->pRenderer->Draw2dLabel((float)x, (float)y, fScale, color, false, "%s", buffer);
}

void CD3D9Renderer::DrawAllShadowsOnTheScreen()
{
	float width = 800;
	float height = 600;
	Set2DMode(true, (int)width, (int)height);
	EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);

	//overwrite 2D-mode viewport
	//int TempX, TempY, TempWidth, TempHeight;
	//GetViewport(&TempX, &TempY, &TempWidth, &TempHeight);

	//RT_SetViewport(0,0,width,height);

	int nMaxCount = 16;

	float fArrDim = max(4.f, sqrtf((float)nMaxCount));
	float fPicDimX = width / fArrDim;
	float fPicDimY = height / fArrDim;
	int nShadowId = 0;
	SDynTexture_Shadow* pTX = SDynTexture_Shadow::s_RootShadow.m_NextShadow;
	for (float x = 0; nShadowId < nMaxCount && x < width - 10; x += fPicDimX)
	{
		for (float y = 0; nShadowId < nMaxCount && y < height - 10; y += fPicDimY)
		{
			static ICVar* pVar = iConsole->GetCVar("e_ShadowsDebug");
			if (pVar && pVar->GetIVal() == 1)
			{
				while (pTX->m_pTexture && (pTX->m_pTexture->m_nAccessFrameID + 2) < GetFrameID(false) && pTX != &SDynTexture_Shadow::s_RootShadow)
					pTX = pTX->m_NextShadow;
			}

			if (pTX == &SDynTexture_Shadow::s_RootShadow)
				break;

			if (pTX->m_pTexture && pTX->pLightOwner)
			{

				CTexture* tp = pTX->m_pTexture;
				if (tp)
				{
					int nSavedAccessFrameID = pTX->m_pTexture->m_nAccessFrameID;

					SetState(/*GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | */ GS_NODEPTHTEST);
					if (tp->GetTextureType() == eTT_2D)
					{
						//Draw2dImage(x, y, fPicDimX-1, fPicDimY-1, tp->GetID(), 0,1,1,0,180);

						// set shader
						CShader* pSH(CShaderMan::s_ShaderShadowMaskGen);

						uint32 nPasses = 0;
						static CCryNameTSCRC TechName = "DebugShadowMap";
						pSH->FXSetTechnique(TechName);
						pSH->FXBegin(&nPasses, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);
						pSH->FXBeginPass(0);

						m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Push();
						EF_PushMatrix();
						Matrix44A* m = m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->GetTop();
						mathMatrixOrthoOffCenterLH(m, 0.0f, (float)width, (float)height, 0.0f, -1e30f, 1e30f);
						m = m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->GetTop();
						m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->LoadIdentity();

						SetState(GS_NODEPTHTEST);
						STexState ts(FILTER_LINEAR, false);
						ts.m_nAnisotropy = 1;
						tp->Apply(0, CTexture::GetTexState(ts));
						D3DSetCull(eCULL_None);

						TempDynVB<SVF_P3F_T3F> vb;
						vb.Allocate(4);
						SVF_P3F_T3F* vQuad = vb.Lock();

						vQuad[0].p = Vec3(x, y, 1);
						vQuad[0].st = Vec3(0, 1, 1);
						//vQuad[0].color.dcolor = (uint32)-1;
						vQuad[1].p = Vec3(x + fPicDimX - 1, y, 1);
						vQuad[1].st = Vec3(1, 1, 1);
						//vQuad[1].color.dcolor = (uint32)-1;
						vQuad[3].p = Vec3(x + fPicDimX - 1, y + fPicDimY - 1, 1);
						vQuad[3].st = Vec3(1, 0, 1);
						//vQuad[3].color.dcolor = (uint32)-1;
						vQuad[2].p = Vec3(x, y + fPicDimY - 1, 1);
						vQuad[2].st = Vec3(0, 0, 1);
						//vQuad[2].color.dcolor = (uint32)-1;

						vb.Unlock();
						vb.Bind(0);
						vb.Release();

						if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_T3F)))
						{
							FX_Commit();
							FX_DrawPrimitive(eptTriangleStrip, 0, 4);
						}

						pSH->FXEndPass();
						pSH->FXEnd();

						EF_PopMatrix();
						m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Pop();

					}
					else
					{
						// set shader
						CShader* pSH(CShaderMan::s_ShaderShadowMaskGen);

						uint32 nPasses = 0;
						static CCryNameTSCRC TechName = "DebugCubeMap";
						pSH->FXSetTechnique(TechName);
						pSH->FXBegin(&nPasses, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);
						pSH->FXBeginPass(0);

						float fSizeX = fPicDimX / 3;
						float fSizeY = fPicDimY / 2;
						float fx = ScaleCoordX(x);
						fSizeX = ScaleCoordX(fSizeX);
						float fy = ScaleCoordY(y);
						fSizeY = ScaleCoordY(fSizeY);
						float fOffsX[] = { fx, fx + fSizeX, fx + fSizeX * 2, fx, fx + fSizeX, fx + fSizeX * 2 };
						float fOffsY[] = { fy, fy, fy, fy + fSizeY, fy + fSizeY, fy + fSizeY };
						Vec3 vTC0[] = { Vec3(1, 1, 1), Vec3(-1, 1, -1), Vec3(-1, 1, -1), Vec3(-1, -1, 1), Vec3(-1, 1, 1), Vec3(1, 1, -1) };
						Vec3 vTC1[] = { Vec3(1, 1, -1), Vec3(-1, 1, 1), Vec3(1, 1, -1), Vec3(1, -1, 1), Vec3(1, 1, 1), Vec3(-1, 1, -1) };
						Vec3 vTC2[] = { Vec3(1, -1, -1), Vec3(-1, -1, 1), Vec3(1, 1, 1), Vec3(1, -1, -1), Vec3(1, -1, 1), Vec3(-1, -1, -1) };
						Vec3 vTC3[] = { Vec3(1, -1, 1), Vec3(-1, -1, -1), Vec3(-1, 1, 1), Vec3(-1, -1, -1), Vec3(-1, -1, 1), Vec3(1, -1, -1) };

						m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Push();
						EF_PushMatrix();
						Matrix44A* m = m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->GetTop();
						mathMatrixOrthoOffCenterLH(m, 0.0f, (float)m_width, (float)m_height, 0.0f, -1e30f, 1e30f);
						m = m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->GetTop();
						m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->LoadIdentity();

						SetState(GS_NODEPTHTEST);
						STexState ts(FILTER_LINEAR, false);
						ts.m_nAnisotropy = 1;
						tp->Apply(0, CTexture::GetTexState(ts));

						D3DSetCull(eCULL_None);

						for (int i = 0; i < 6; i++)
						{
							TempDynVB<SVF_P3F_T3F> vb;
							vb.Allocate(4);
							SVF_P3F_T3F* vQuad = vb.Lock();

							vQuad[0].p = Vec3(fOffsX[i], fOffsY[i], 1);
							vQuad[0].st = vTC0[i];
							//vQuad[0].color.dcolor = (uint32)-1;
							vQuad[1].p = Vec3(fOffsX[i] + fSizeX - 1, fOffsY[i], 1);
							vQuad[1].st = vTC1[i];
							//vQuad[1].color.dcolor = (uint32)-1;
							vQuad[3].p = Vec3(fOffsX[i] + fSizeX - 1, fOffsY[i] + fSizeY - 1, 1);
							vQuad[3].st = vTC2[i];
							//vQuad[3].color.dcolor = (uint32)-1;
							vQuad[2].p = Vec3(fOffsX[i], fOffsY[i] + fSizeY - 1, 1);
							vQuad[2].st = vTC3[i];
							//vQuad[2].color.dcolor = (uint32)-1;

							vb.Unlock();
							vb.Bind(0);
							vb.Release();

							if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_T3F)))
							{
								FX_Commit();
								FX_DrawPrimitive(eptTriangleStrip, 0, 4);
							}
						}

						pSH->FXEndPass();
						pSH->FXEnd();

						EF_PopMatrix();
						m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Pop();
					}

					pTX->m_pTexture->m_nAccessFrameID = nSavedAccessFrameID;

					ILightSource* pLS = (ILightSource*)pTX->pLightOwner;

					_DrawText(iSystem, (int)(x / width * 800.f), (int)(y / height * 600.f), 1.0f, "%s \n %d %d-%d %d x%d",
					          pTX->m_pTexture->GetSourceName(),
					          pTX->m_nUniqueID,
					          pTX->m_pTexture ? pTX->m_pTexture->m_nUpdateFrameID : 0,
					          pTX->m_pTexture ? pTX->m_pTexture->m_nAccessFrameID : 0,
					          pTX->nObjectsRenderedCount,
					          pTX->m_nWidth);

					if (pLS->GetLightProperties().m_sName)
						_DrawText(iSystem, (int)(x / width * 800.f), (int)(y / height * 600.f) + 32, 1.0f, "%s", pLS->GetLightProperties().m_sName);

				}
			}
			pTX = pTX->m_NextShadow;
		}
	}

	Set2DMode(false, m_width, m_height);
}

Vec3 UnProject(ShadowMapFrustum* pFr, Vec3 vPoint)
{
	const int shadowViewport[4] = { 0, 0, 1, 1 };

	Vec3 vRes(0, 0, 0);
	gRenDev->UnProject(vPoint.x, vPoint.y, vPoint.z,
	                   &vRes.x, &vRes.y, &vRes.z,
	                   (float*)&pFr->mLightViewMatrix,
	                   (float*)&pFr->mLightProjMatrix,
	                   shadowViewport);

	return vRes;
}

//////////////////////////////////////////////////////////////////////////
bool CD3D9Renderer::PrepareDepthMap(CRenderView* pRenderView, ShadowMapFrustum* pShadowFrustum, int nLightFrustumID, bool bClearPool) //SRenderLight* pLight,
{
	int nThreadList = m_RP.m_nProcessThreadID;
	assert(pShadowFrustum);
	if (!pShadowFrustum)
		return false;

	//select shadowgen gpu
	int nShadowGenGPU = 0;
	if (GetActiveGPUCount() > 1 && CV_r_ShadowGenMode == 1)
	{
		//TOFIx: make m_nFrameSwapID - double buffered
		nShadowGenGPU = gRenDev->RT_GetCurrGpuID();
	}

	if (GetActiveGPUCount() > 1 && pShadowFrustum->IsCached())
	{
		ShadowFrustumMGPUCache* pFrustumCache = GetShadowFrustumMGPUCache();
		pFrustumCache->nUpdateMaskRT &= ~(1 << gRenDev->RT_GetCurrGpuID());
	}

	//////////////////////////////////////////////////////////////////////////
	// Set matrices
	//////////////////////////////////////////////////////////////////////////

	//enable view space shadow maps
	static ICVar* p_e_gsm_view_dependent = iConsole->GetCVar("e_GsmViewSpace");
	bool bViewSpace = (p_e_gsm_view_dependent->GetIVal() > 0) && pShadowFrustum->bAllowViewDependency;

	// Save previous camera
	int vX, vY, vWidth, vHeight;
	GetViewport(&vX, &vY, &vWidth, &vHeight);
	Matrix44 camMatr = m_CameraMatrix;

	// Setup matrices
	EF_PushMatrix(); //push view matrix
	m_RP.m_TI[nThreadList].m_matProj->Push();

	Matrix44A* m = m_RP.m_TI[nThreadList].m_matProj->GetTop();
	m->SetIdentity();
	m = m_RP.m_TI[nThreadList].m_matView->GetTop();
	m->SetIdentity();

	m_RP.m_TI[nThreadList].m_PersFlags |= RBPF_FP_MATRIXDIRTY;

	pShadowFrustum->mLightProjMatrix.SetIdentity();

	//////////////////////////////////////////////////////////////////////////
	//  Assign RTs
	//////////////////////////////////////////////////////////////////////////
	bool bTextureFromDynPool = false;

	if (pShadowFrustum->bUseShadowsPool)
	{
		pShadowFrustum->nTextureWidth = m_nShadowPoolWidth;
		pShadowFrustum->nTextureHeight = m_nShadowPoolHeight;

		//current eTF should be stored in the shadow frustum
		ETEX_Format ePoolTF = pShadowFrustum->m_eReqTF;
		CTexture::s_ptexRT_ShadowPool->Invalidate(m_nShadowPoolWidth, m_nShadowPoolHeight, ePoolTF);   //lof->nTextureWidth, lof->nTextureHeight

		if (!CTexture::IsTextureExist(CTexture::s_ptexRT_ShadowPool))
		{
#if !defined(_RELEASE) && !CRY_PLATFORM_WINDOWS
			__debugbreak(); // don't want any realloc on consoles
#endif
			CTexture::s_ptexRT_ShadowPool->CreateRenderTarget(eTF_Unknown, Clr_FarPlane);
		}

		pShadowFrustum->pDepthTex = CTexture::s_ptexRT_ShadowPool;
	}
	else
	{
		if (pShadowFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
		{
			CTexture* pTX = CTexture::s_ptexNearestShadowMap;
			if (!CTexture::IsTextureExist(pTX))
			{
				pTX->CreateRenderTarget(pShadowFrustum->m_eReqTF, Clr_FarPlane);
			}
			pShadowFrustum->pDepthTex = pTX;

			pShadowFrustum->fWidthS *= pShadowFrustum->nTextureWidth / (float)pTX->GetWidth();
			pShadowFrustum->fWidthT *= pShadowFrustum->nTextureHeight / (float)pTX->GetHeight();

			pShadowFrustum->nTextureWidth = pTX->GetWidth();
			pShadowFrustum->nTextureHeight = pTX->GetHeight();
		}
		else if (pShadowFrustum->IsCached())
		{
			if (pShadowFrustum->m_eFrustumType != ShadowMapFrustum::e_HeightMapAO)
			{
				int nCache = CV_r_ShadowsCache;
				assert(nCache > 0 && nCache <= MAX_GSM_LODS_NUM);
				const int nStaticMapIndex = clamp_tpl(pShadowFrustum->nShadowMapLod - (nCache - 1), 0, MAX_GSM_LODS_NUM - 1);
				pShadowFrustum->pDepthTex = CTexture::s_ptexCachedShadowMap[nStaticMapIndex];
			}
			else
			{
				pShadowFrustum->pDepthTex = CTexture::s_ptexHeightMapAODepth[0];
			}
		}
		else if (pShadowFrustum->m_eFrustumType != ShadowMapFrustum::e_GsmDynamicDistance)
		{
			bTextureFromDynPool = true;
			SDynTexture_Shadow* pDynTX = SDynTexture_Shadow::GetForFrustum(pShadowFrustum);
			pShadowFrustum->pDepthTex = pDynTX->m_pTexture;
		}
	}

	if (CTexture::IsTextureExist(pShadowFrustum->pDepthTex))
	{
		SDepthTexture D3dSurface;
		ZeroStruct(D3dSurface);

		int nSides = 1;
		if (pShadowFrustum->bOmniDirectionalShadow)
			nSides = OMNI_SIDES_NUM;

		int nS;
		int nFirstShadowGenRI = 0, nLastShadowGenRI = 0;

		int nOldScissor = CV_r_scissor;
		int old_CV_r_nodrawnear = CV_r_nodrawnear;
		int nPersFlags = m_RP.m_TI[nThreadList].m_PersFlags;
		int nPersFlags2 = m_RP.m_PersFlags2;
		int nStateAnd = m_RP.m_StateAnd;
		m_RP.m_TI[nThreadList].m_PersFlags &= ~(RBPF_HDR | RBPF_MIRRORCULL);
		m_RP.m_TI[nThreadList].m_PersFlags |= RBPF_SHADOWGEN;

		if (!(pShadowFrustum->m_Flags & DLF_DIRECTIONAL))
		{
			m_RP.m_PersFlags2 |= RBPF2_DRAWTOCUBE;
			m_RP.m_TI[nThreadList].m_PersFlags |= RBPF_MIRRORCULL;
		}

		//hack remove texkill for eTF_DF24 and eTF_D24S8
		if (pShadowFrustum->m_eReqTF == eTF_R32F || pShadowFrustum->m_eReqTF == eTF_R16G16F || pShadowFrustum->m_eReqTF == eTF_R16F || pShadowFrustum->m_eReqTF == eTF_R16G16B16A16F ||
		    pShadowFrustum->m_eReqTF == eTF_D16 || pShadowFrustum->m_eReqTF == eTF_D24S8 || pShadowFrustum->m_eReqTF == eTF_D32F || pShadowFrustum->m_eReqTF == eTF_D32FS8)
		{
			m_RP.m_PersFlags2 |= RBPF2_NOALPHABLEND;
			m_RP.m_StateAnd &= ~GS_BLEND_MASK;
		}

		if (pShadowFrustum->m_eReqTF == eTF_R32F || pShadowFrustum->m_eReqTF == eTF_R16G16F || pShadowFrustum->m_eReqTF == eTF_R16F || pShadowFrustum->m_eReqTF == eTF_R16G16B16A16F)
		{
			m_RP.m_PersFlags2 |= RBPF2_NOALPHATEST;
			m_RP.m_StateAnd &= ~GS_ALPHATEST_MASK;
		}
		CCamera saveCam = GetCamera();
		Vec3 vPos = pShadowFrustum->vLightSrcRelPos + pShadowFrustum->vProjTranslation;

		for (nS = 0; nS < nSides; nS++)
		{
			if (nLightFrustumID >= 0)//skip for custom fustum
			{
				//////////////////////////////////////////////////////////////////////////
				//compute shadow recursive level
				//////////////////////////////////////////////////////////////////////////
				const int nThreadID = m_RP.m_nProcessThreadID;
				const int nShadowRecur = pShadowFrustum->nShadowGenID[nThreadID][nS];

				if (nShadowRecur == 0xFFFFFFFF)
					continue;
			}

			if (pShadowFrustum->bUseHWShadowMap)
			{
				D3dSurface.nWidth = pShadowFrustum->nTextureWidth;
				D3dSurface.nHeight = pShadowFrustum->nTextureHeight;
				D3dSurface.nFrameAccess = -1;
				D3dSurface.bBusy = false;
				D3dSurface.pTexture = pShadowFrustum->pDepthTex;
				D3dSurface.pTarget = pShadowFrustum->pDepthTex->GetDevTexture()->Get2DTexture();
				D3dSurface.pSurface = pShadowFrustum->bOmniDirectionalShadow && !(pShadowFrustum->bUnwrapedOmniDirectional)
				                      ? pShadowFrustum->pDepthTex->GetDeviceDepthStencilView(nS, 1)
				                      : pShadowFrustum->pDepthTex->GetDeviceDepthStencilView();
			}

			CCamera tmpCamera;
			if (!pShadowFrustum->bOmniDirectionalShadow)
			{
				if (pShadowFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
				{
					const Vec3 camPos = GetCamera().GetPosition();
					pShadowFrustum->aabbCasters.min += camPos;
					pShadowFrustum->aabbCasters.max += camPos;

					CShadowUtils::GetShadowMatrixForObject(pShadowFrustum->mLightProjMatrix, pShadowFrustum->mLightViewMatrix, pShadowFrustum->vFrustInfo, pShadowFrustum->vLightSrcRelPos, pShadowFrustum->aabbCasters);
					pShadowFrustum->mLightViewMatrix = pShadowFrustum->mLightViewMatrix * pShadowFrustum->mLightProjMatrix;
					pShadowFrustum->mLightProjMatrix.SetIdentity();
				}

				*m = pShadowFrustum->mLightViewMatrix;
				m_RP.m_TI[nThreadList].m_PersFlags &= ~RBPF_REVERSE_DEPTH;

				uint32 depthState = ReverseDepthHelper::ConvertDepthFunc(m_RP.m_CurState);
				FX_SetState(m_RP.m_CurState, m_RP.m_CurAlphaRef, depthState);
			}
			else
			{
				const Matrix34& m34 = pShadowFrustum->FrustumPlanes[nS].GetMatrix();
				CRenderCamera c;
				c.Perspective(tmpCamera.GetFov(), tmpCamera.GetProjRatio(), tmpCamera.GetNearPlane(), tmpCamera.GetFarPlane());
				Vec3 vEyeC = tmpCamera.GetPosition();
				Vec3 vAtC = vEyeC + Vec3(m34(0, 1), m34(1, 1), m34(2, 1));
				Vec3 vUpC = Vec3(m34(0, 2), m34(1, 2), m34(2, 2));
				c.LookAt(vEyeC, vAtC, vUpC);
				SetRCamera(c);
				CShadowUtils::GetCubemapFrustum(FTYP_SHADOWOMNIPROJECTION, pShadowFrustum, nS, m_RP.m_TI[nThreadList].m_matProj->GetTop(), m_RP.m_TI[nThreadList].m_matView->GetTop(), NULL);
			}

			if (CV_r_ShadowGenDepthClip == 0 && pShadowFrustum->fRendNear > 0.0f) //&& lof->fRendFar > 0.0f
			{
				m_RP.m_TI[nThreadList].m_vFrustumInfo.x = pShadowFrustum->fRendNear;
				m_RP.m_TI[nThreadList].m_vFrustumInfo.y = pShadowFrustum->m_eFrustumType != ShadowMapFrustum::e_HeightMapAO ? pShadowFrustum->fFarDist : 1.f; //lof->fRendFar;
			}
			else
			{
				m_RP.m_TI[nThreadList].m_vFrustumInfo.x = pShadowFrustum->fNearDist;
				m_RP.m_TI[nThreadList].m_vFrustumInfo.y = pShadowFrustum->m_eFrustumType != ShadowMapFrustum::e_HeightMapAO ? pShadowFrustum->fFarDist : 1.f;
			}

			//depth shift for precision increasing
			if (!pShadowFrustum->bUseHWShadowMap && pShadowFrustum->fNearDist > 1000.f)   //FIX: strange check for sun
			{
				m_RP.m_TI[nThreadList].m_vFrustumInfo.z = 0.5f;
				pShadowFrustum->bNormalizedDepth = false;

			}
			else
			{
				m_RP.m_TI[nThreadList].m_vFrustumInfo.z = 0.0f;
				pShadowFrustum->bNormalizedDepth = true;
			}

			//shadowgen bias
			m_RP.m_TI[nThreadList].m_vFrustumInfo.w = pShadowFrustum->fDepthTestBias;
			EF_SetCameraInfo();

			//assign for shader's parameters
			SRenderPipeline::ShadowInfo& shadowInfo = m_RP.m_ShadowInfo;
			shadowInfo.m_pCurShadowFrustum = pShadowFrustum;
			shadowInfo.m_nOmniLightSide = nS;
			shadowInfo.vViewerPos = saveCam.GetPosition();

			int arrViewport[4];

			CTexture* pColorTarget = pShadowFrustum->bUseHWShadowMap ? NULL : pShadowFrustum->pDepthTex;
			SDepthTexture* pDepthTarget = pShadowFrustum->bUseHWShadowMap? &D3dSurface : FX_GetDepthSurface(pShadowFrustum->nTextureWidth, pShadowFrustum->nTextureHeight, false);
			//SDepthTexture* pDepthTarget = FX_GetDepthSurface(pShadowFrustum->nTextureWidth, pShadowFrustum->nTextureHeight, false);

#if defined(FEATURE_SVO_GI)
			if (CSvoRenderer::GetRsmColorMap(*pShadowFrustum, true) && CSvoRenderer::GetRsmNormlMap(*pShadowFrustum, true))
			{
				FX_PushRenderTarget(0, CSvoRenderer::GetInstance()->GetRsmColorMap(*pShadowFrustum), pDepthTarget, pShadowFrustum->m_eReqTT == eTT_Cube ? nS : -1);
				FX_PushRenderTarget(1, CSvoRenderer::GetInstance()->GetRsmNormlMap(*pShadowFrustum), NULL, pShadowFrustum->m_eReqTT == eTT_Cube ? nS : -1);
			}
			else
#endif
			{
				FX_PushRenderTarget(0, pColorTarget, pDepthTarget, pShadowFrustum->m_eReqTT == eTT_Cube ? nS : -1);   // calls RT_SetViewport() implicitly
			}

			//SDW-GEN_REND_PATH
			//////////////////////////////////////////////////////////////////////////
			// clear frame buffer after RT push
			//////////////////////////////////////////////////////////////////////////
			if (!pShadowFrustum->bIncrementalUpdate)
			{
				const bool bReverseDepth = (m_RP.m_TI[nThreadList].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;

				if (pShadowFrustum->bUseShadowsPool)
				{
					if (bClearPool)
					{
						const RECT rect = { pShadowFrustum->packX[nS], pShadowFrustum->packY[nS], pShadowFrustum->packX[nS] + pShadowFrustum->packWidth[nS], pShadowFrustum->packY[nS] + pShadowFrustum->packHeight[nS] };

						FX_ClearTarget(pDepthTarget, CLEAR_ZBUFFER, Clr_FarPlane_R.r, 0, 1, &rect, false);
					}
				}
				else
				{
#if defined(FEATURE_SVO_GI)
					if (CSvoRenderer::GetRsmColorMap(*pShadowFrustum, true) && CSvoRenderer::GetRsmNormlMap(*pShadowFrustum, true))
					{
						FX_ClearTarget(CSvoRenderer::GetInstance()->GetRsmColorMap(*pShadowFrustum), Clr_Transparent);
						FX_ClearTarget(CSvoRenderer::GetInstance()->GetRsmNormlMap(*pShadowFrustum), Clr_Transparent);
					}
					else
#endif
					if (pColorTarget)
					{
						FX_ClearTarget(pColorTarget, pShadowFrustum->pDepthTex->GetDstFormat() == eTF_R8G8B8A8 ? ColorF(1, 1, 1, 0) : ColorF(1, 0, 0, 0));
					}

					FX_ClearTarget(pDepthTarget, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_R.r, 0);
				}

				m_pNewTarget[0]->m_ClearFlags = 0;
			}
			//////////////////////////////////////////////////////////////////////////

			//set proper side-viewport
			if (pShadowFrustum->bUnwrapedOmniDirectional || pShadowFrustum->bUseShadowsPool)
			{
				pShadowFrustum->GetSideViewport(nS, arrViewport);
				RT_SetViewport(arrViewport[0], arrViewport[1], arrViewport[2], arrViewport[3]);
			}
			//////////////////////////////////////////////////////////////////////////

			FX_Commit();

			//disable color writes
			if (pShadowFrustum->bUseHWShadowMap)
			{
#if defined(FEATURE_SVO_GI)
				if (!CSvoRenderer::GetRsmColorMap(*pShadowFrustum))
#else
				if (true)
#endif
				{
					FX_SetState(GS_COLMASK_NONE, -1);
					m_RP.m_PersFlags2 |= RBPF2_DISABLECOLORWRITES;
					m_RP.m_StateOr |= GS_COLMASK_NONE;
				}
				float fShadowsBias = CV_r_ShadowsBias;
				float fShadowsSlopeScaleBias = pShadowFrustum->fDepthSlopeBias;

				//adjust nearest slope fer nearest custom frustum
				if (pShadowFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
					fShadowsSlopeScaleBias *= 7.0f;

				if (pShadowFrustum->fDepthSlopeBias > 0.0f && (pShadowFrustum->m_Flags & DLF_DIRECTIONAL))
				{
					SStateRaster CurRS = m_StatesRS[m_nCurStateRS];

					CurRS.Desc.DepthBias = 0;//1;
					CurRS.Desc.DepthBiasClamp = fShadowsBias * 20;
					CurRS.Desc.SlopeScaledDepthBias = fShadowsSlopeScaleBias;
					//disable multisampling for shadowgen
					//CurRS.Desc.MultisampleEnable = false;
					//CurRS.Desc.AntialiasedLineEnable = false;

					SetRasterState(&CurRS);
				}
			}

			int nBorder = max(4, pShadowFrustum->nShadowMapSize / 64);
			//if (eTT == eTT_2D)
			//  EF_Scissor(true,nBorder,nBorder,nTextureSize-nBorder*2,nTextureSize-nBorder*2);

			if (!(pShadowFrustum->m_Flags & DLF_LIGHT_BEAM))
			{
				//FIX !!! why CULL_NONE is setting here
				D3DSetCull(eCULL_None);
			}
			else
			{
				D3DSetCull(eCULL_Back);
				m_RP.m_PersFlags2 |= RBPF2_LIGHTSHAFTS;
			}

			if (CRenderView* pShadowView = pRenderView->GetShadowsView(pShadowFrustum))
			{
				auto& rendItems = pShadowView->GetRenderItems(nS);
				OldPipeline_ProcessRenderList(rendItems, -1, -1, EFSLIST_SHADOW_GEN, FX_FlushShader_ShadowGen, false);
			}

			FX_PopRenderTarget(0);

#if defined(FEATURE_SVO_GI)
			if (CSvoRenderer::GetRsmColorMap(*pShadowFrustum) && CSvoRenderer::GetRsmNormlMap(*pShadowFrustum))
				FX_PopRenderTarget(1);
#endif

			//enable color writes
			if (pShadowFrustum->bUseHWShadowMap)
			{
				m_RP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
				m_RP.m_StateOr &= ~GS_COLMASK_NONE;
				if (CV_r_ShadowsBias > 0.0f)
				{
					//D3D11_RASTERIZER_DESC RSShadowGen;

					SStateRaster CurRS = m_StatesRS[m_nCurStateRS];
					CurRS.Desc.DepthBias = 0;
					CurRS.Desc.DepthBiasClamp = 0;
					CurRS.Desc.SlopeScaledDepthBias = 0;

					//CurRS.Desc.MultisampleEnable = false;
					//CurRS.Desc.AntialiasedLineEnable = false;

					SetRasterState(&CurRS);
				}
			}
		}

		m_RP.m_TI[nThreadList].m_PersFlags &= ~RBPF_SHADOWGEN;
		SetCamera(saveCam);
		if (pShadowFrustum->m_eReqTT == eTT_Cube)
		{
			//FIX: replace this by light space matrix computation and move out here
			pShadowFrustum->mLightViewMatrix.SetIdentity();
			pShadowFrustum->mLightViewMatrix.SetTranslation(vPos);
			pShadowFrustum->mLightViewMatrix.Transpose();
		}

		if (!(pShadowFrustum->m_Flags & DLF_DIRECTIONAL))
		{
			m_RP.m_PersFlags2 &= ~RBPF2_DRAWTOCUBE;
		}

		m_RP.m_TI[nThreadList].m_PersFlags &= ~RBPF_MIRRORCULL;

		m_RP.m_PersFlags2 &= ~RBPF2_LIGHTSHAFTS;

		CV_r_nodrawnear = old_CV_r_nodrawnear;

		// restore scissor
		CV_r_scissor = nOldScissor;

		m_RP.m_TI[nThreadList].m_PersFlags = nPersFlags;
		m_RP.m_PersFlags2 = nPersFlags2;
		m_RP.m_StateAnd = nStateAnd;

		EF_Scissor(false, 0, 0, 0, 0);
	}
	else if (bTextureFromDynPool)
	{
		iLog->Log("Error: cannot create depth texture  for frustum '%d' (skipping)", pShadowFrustum->nShadowMapLod);
	}

	EF_PopMatrix();
	m_RP.m_TI[nThreadList].m_matProj->Pop();
	m_CameraMatrix = camMatr;
	EF_SetCameraInfo();

	//don't need to restore viewport (restored in FX_RestoreRenderTarget)
	RT_SetViewport(vX, vY, vWidth, vHeight);

	return true;
}

//TODO:make two independent functions for dx10 and dx9 without processing texture array and resource views
// setup projection texgen
void CD3D9Renderer::ConfigShadowTexgen(int Num, ShadowMapFrustum* pFr, int nFrustNum, bool bScreenToLocalBasis, bool bUseComparisonSampling)
{
	Matrix44A shadowMat, mTexScaleBiasMat, mLightView, mLightProj, mLightViewProj;
	bool bGSM = false;

	//check for successful PrepareDepthMap
	if (!pFr->pDepthTex && !pFr->bUseShadowsPool)
	{
		return;
	}

	//calc view inversion matrix
	//Matrix44 *pCurrViewMatrix = m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->GetTop();
	//Matrix44 mInvertedView = pCurrViewMatrix->GetInverted();

	//shadow frustum adjustment matrix
	//half texel offset
	//TOFIX: should take in account real shadow atlas resolution
	float fOffsetX = 0.5f;
	float fOffsetY = 0.5f;

	const Matrix44A mClipToTexSpace = Matrix44(0.5f, 0.0f, 0.0f, 0.0f,
	                                           0.0f, -0.5f, 0.0f, 0.0f,
	                                           0.0f, 0.0f, 1.0f, 0.0f,
	                                           fOffsetX, fOffsetY, 0.0f, 1.0f);

	mTexScaleBiasMat = mClipToTexSpace;

	//FIX:remove temporarily matrices
	if ((pFr->bOmniDirectionalShadow || pFr->bUseShadowsPool) && nFrustNum > -1)
	{

		if (!pFr->bOmniDirectionalShadow)
		{
			mLightView = pFr->mLightViewMatrix;
			mLightProj.SetIdentity();
		}
		else
		{
			CShadowUtils::GetCubemapFrustum(FTYP_SHADOWOMNIPROJECTION, pFr, nFrustNum, &mLightProj, &mLightView);
		}

		float arrOffs[2];
		float arrScale[2];
		pFr->GetTexOffset(nFrustNum, arrOffs, arrScale, m_nShadowPoolWidth, m_nShadowPoolHeight);

		//calculate crop matrix for  frustum
		//TD: investigate proper half-texel offset with mCropView
		Matrix44 mCropView(arrScale[0], 0.0f, 0.0f, 0.0f,
		                   0.0f, arrScale[1], 0.0f, 0.0f,
		                   0.0f, 0.0f, 1.0f, 0.0f,
		                   arrOffs[0], arrOffs[1], 0.0f, 1.0f);

		// multiply the projection matrix with it
		mTexScaleBiasMat = mTexScaleBiasMat * mCropView;

		//constants for gsm atlas
		m_cEF.m_TempVecs[6].x = arrOffs[0];
		m_cEF.m_TempVecs[6].y = arrOffs[1];
	}
	else
	{
		mLightView = pFr->mLightViewMatrix;

		if (pFr->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance)
		{
			Matrix44 mCropView(IDENTITY);
			mCropView.m00 = (float)pFr->packWidth[0] / pFr->pDepthTex->GetWidth();
			mCropView.m11 = (float)pFr->packHeight[0] / pFr->pDepthTex->GetHeight();
			mCropView.m30 = (float)pFr->packX[0] / pFr->pDepthTex->GetWidth();
			mCropView.m31 = (float)pFr->packY[0] / pFr->pDepthTex->GetHeight();

			mTexScaleBiasMat = mTexScaleBiasMat * mCropView;
		}

		//temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
		mLightProj.SetIdentity(); //pFr->mLightProjMatrix;
		//mLightProj = pFr->mLightProjMatrix;
		bGSM = true;
	}

	//currently mLightProj has already pre-multiplied LightProj matrix
	mLightViewProj = mLightView * mLightProj;

	shadowMat = /*mInvertedView * */ mLightViewProj * mTexScaleBiasMat;

	//set shadow matrix
	gRenDev->m_TempMatrices[Num][0] = shadowMat.GetTransposed(); //TOFIX: construct projection and view matrices with Column Convention
	//set light space transformation
	m_cEF.m_TempVecs[5] = Vec4(mLightViewProj.m30, mLightViewProj.m31, mLightViewProj.m32, 1);

	//////////////////////////////////////////////////////////////////////////
	// Deferred shadow pass setup
	//////////////////////////////////////////////////////////////////////////
	Matrix44 mScreenToShadow;
	int vpX, vpY, vpWidth, vpHeight;
	GetViewport(&vpX, &vpY, &vpWidth, &vpHeight);
	FX_DeferredShadowPassSetup(/*shadowMat*/ gRenDev->m_TempMatrices[Num][0], pFr, (float)vpWidth, (float)vpHeight,
	                                         mScreenToShadow, pFr->m_eFrustumType == ShadowMapFrustum::e_Nearest);

#if defined(VOLUMETRIC_FOG_SHADOWS)
	//use cur TexGen for homogeneous position reconstruction
	if (bScreenToLocalBasis && CRenderer::CV_r_FogShadowsMode == 1)
	{
		Matrix44A mLocalScale;
		mLocalScale.SetIdentity();
		gRenDev->m_TempMatrices[Num][0] = mScreenToShadow.GetTransposed();
		float fScreenScale = (CV_r_FogShadows == 2) ? 4.0f : 2.0f;
		mLocalScale.m00 = fScreenScale;
		mLocalScale.m11 = fScreenScale;
		gRenDev->m_TempMatrices[Num][0] = gRenDev->m_TempMatrices[Num][0] * mLocalScale;
	}
#endif

	gRenDev->m_TempMatrices[Num][2].m33 = 0.0f;
	if (bGSM && pFr->bBlendFrustum)
	{
		const float fBlendVal = pFr->fBlendVal;

		m_cEF.m_TempVecs[15][0] = fBlendVal;
		m_cEF.m_TempVecs[15][1] = 1.0f / (1.0f - fBlendVal);
		m_cEF.m_TempVecs[15][2] = 0.0f;
		m_cEF.m_TempVecs[15][3] = 0.0f;

		m_cEF.m_TempVecs[6] = Vec4(1.f, 1.f, 0.f, 0.f);
		if (pFr->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance)
		{
			m_cEF.m_TempVecs[6].x = pFr->pDepthTex->GetWidth() / float(pFr->packWidth[0]);
			m_cEF.m_TempVecs[6].y = pFr->pDepthTex->GetHeight() / float(pFr->packHeight[0]);
			m_cEF.m_TempVecs[6].z = -pFr->packX[0] / float(pFr->packWidth[0]);
			m_cEF.m_TempVecs[6].w = -pFr->packY[0] / float(pFr->packHeight[0]);
		}

		const ShadowMapFrustum* pPrevFr = pFr->pPrevFrustum;
		if (pPrevFr)
		{
			Matrix44A mLightViewPrev = pPrevFr->mLightViewMatrix;
			Matrix44A shadowMatPrev = mLightViewPrev * mClipToTexSpace;  // NOTE: no sub-rect here as blending code assumes full [0-1] UV range;

			FX_DeferredShadowPassSetupBlend(shadowMatPrev.GetTransposed(), Num, (float)vpWidth, (float)vpHeight);

			m_cEF.m_TempVecs[2][2] = 1.f / (pPrevFr->fFarDist);

			float fBlendValPrev = pPrevFr->fBlendVal;

			m_cEF.m_TempVecs[15][2] = fBlendValPrev;
			m_cEF.m_TempVecs[15][3] = 1.0f / (1.0f - fBlendValPrev);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//rotation matrix for far terrain projection
	//Fix: Generate separate matrix for frustum
	Matrix33 mRotMatrix(mLightView);
	mRotMatrix.OrthonormalizeFast();
	gRenDev->m_TempMatrices[0][1] = Matrix44(mRotMatrix).GetTransposed();

	// enable combined shadow maps
	static ICVar* p_e_gsm_combined = 0; //iConsole->GetCVar("e_GsmCombined");		assert(p_e_gsm_combined);

	if (Num >= 0)
	{
		if (!pFr->pDepthTex && !pFr->bUseShadowsPool)
			Warning("Warning: CD3D9Renderer::ConfigShadowTexgen: pFr->depth_tex_id not set");
		else
		{
			int nID = 0;
			int nIDBlured = 0;
			if (pFr->bUseShadowsPool)
			{
				nID = CTexture::s_ptexRT_ShadowPool->GetID();
			}
			else if (pFr->pDepthTex != NULL)
			{
				nID = pFr->pDepthTex->GetID();
			}

			m_RP.m_ShadowCustomTexBind[Num * 2 + 0] = nID;
			m_RP.m_ShadowCustomTexBind[Num * 2 + 1] = nIDBlured;
			m_RP.m_ShadowCustomComparisonSampling[Num * 2 + 0] = bUseComparisonSampling;
			//m_cEF.m_TempVecs[0][Num] = pFr->fAlpha;

			//SDW-CFG_PRM
			//GSM shadow fading param
			m_cEF.m_TempVecs[8][0] = pFr->fShadowFadingDist;

			assert(Num < 4);
			//per frustum parameters
			//fDepthTestBias param
			if (pFr->bHWPCFCompare)
			{
				if (pFr->m_Flags & DLF_DIRECTIONAL)
				{
					//linear case + constant offset
					m_cEF.m_TempVecs[1][Num] = pFr->fDepthConstBias;
					if (pFr->m_eFrustumType == ShadowMapFrustum::e_Nearest)
						m_cEF.m_TempVecs[1][Num] *= 3.0f;
				}
				else if (pFr->m_Flags & DLF_PROJECT)
				{
					//non-linear case
					m_cEF.m_TempVecs[1][Num] = pFr->fDepthConstBias /** pFr->fFarDist*/;
				}
				else
				{
					m_cEF.m_TempVecs[1][Num] = pFr->fDepthConstBias /** pFr->fFarDist*/;
				}
			}
			else
			{
				//linear case
				m_cEF.m_TempVecs[1][Num] = pFr->fDepthTestBias;
			}

			//fOneDivFarDist param
			if ((pFr->m_Flags & DLF_DIRECTIONAL) || (!(pFr->bUseHWShadowMap) && !(pFr->bHWPCFCompare)))
			{
				//linearized shadows are used for any kind of directional lights and for non-hw point lights
				m_cEF.m_TempVecs[2][Num] = 1.f / (pFr->fFarDist);
			}
			else
			{
				//hw point lights sources have non-linear depth for now
				m_cEF.m_TempVecs[2][Num] = 1.f / (pFr->fFarDist);
			}

			//vInvShadowMapWH param
			m_cEF.m_TempVecs[9][Num] = 1.f / pFr->nTexSize;

			//TOFIX:replace by mask based function to set these parameters
			//depth shift to extend range precision
			if (pFr->bNormalizedDepth)
			{
				m_cEF.m_TempVecs[3][Num] = 0.0f;
			}
			else
			{
				m_cEF.m_TempVecs[3][Num] = 0.5f;
			}

			float fShadowJitter = m_shadowJittering;

			//TOFIX: currently force console variable using
			//should not set it like this but
			//all the time retation samples should be recompiled to reduce pixel shader
			if (pFr->m_Flags & DLF_DIRECTIONAL)
			{
				//FIME: insert calculation for Filtered Area
				float fFilteredArea = fShadowJitter * (pFr->fWidthS + pFr->fBlurS);

				if (pFr->m_eFrustumType == ShadowMapFrustum::e_Nearest)
					fFilteredArea *= 0.1;

				m_cEF.m_TempVecs[4].x = fFilteredArea;
				m_cEF.m_TempVecs[4].y = fFilteredArea;
			}
			else
			{
				fShadowJitter = 2.0; //constant penumbra for now
				//USE frustums in sGetIrregKernel() directly but not through the temp vector
				m_cEF.m_TempVecs[4].x = fShadowJitter; /** pFr->fFrustrumSize*/;
				m_cEF.m_TempVecs[4].y = fShadowJitter;

				if (pFr->bOmniDirectionalShadow)
				{
					m_cEF.m_TempVecs[4].x *= 1.0f / 3.0f;
					m_cEF.m_TempVecs[4].y *= 1.0f / 2.0f;
				}
			}
		}
	}
}

//=============================================================================================================
void CD3D9Renderer::FX_SetupForwardShadows(CRenderView* pRenderView, bool bUseShaderPermutations)
{

	const int FORWARD_SHADOWS_CASCADE0_SINGLE_TAP = 0x10;
	const int FORWARD_SHADOWS_CLOUD_SHADOWS = 0x20;

	const int nThreadID = m_RP.m_nProcessThreadID;

	auto& SMFrustums = pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunDynamic);

	if (bUseShaderPermutations)
	{
		m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] |
		                           g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ]);
	}

	uint32 nCascadeMask = 0;
	for (int a = 0, cascadeCount = 0; a < SMFrustums.size() && cascadeCount < 4; ++a)
	{
		ShadowMapFrustum* pFr = SMFrustums[a]->pFrustum;
		nCascadeMask |= 0x1 << a;

		ConfigShadowTexgen(cascadeCount, pFr, -1, true);

		if (bUseShaderPermutations)
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0 + cascadeCount];

		++cascadeCount;
	}

	// only do full pcf filtering on nearest shadow cascade
	if (nCascadeMask > 0 && SMFrustums[0]->pFrustum->nShadowMapLod != 0)
		nCascadeMask |= FORWARD_SHADOWS_CASCADE0_SINGLE_TAP;

	if (m_bCloudShadowsEnabled && m_cloudShadowTexId > 0)
	{
		nCascadeMask |= FORWARD_SHADOWS_CLOUD_SHADOWS;

		if (bUseShaderPermutations)
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ];
	}

	// store cascade mask in m_TempVecs[4].z
	*alias_cast<uint32*>(&m_cEF.m_TempVecs[4].z) = nCascadeMask;
}

void CD3D9Renderer::FX_SetupShadowsForTransp()
{
	PROFILE_FRAME(SetupShadowsForTransp);

	m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_POINT_LIGHT] | g_HWSR_MaskBit[HWSR_SHADOW_MIXED_MAP_G16R16]);

	CRenderObject* pObj = m_RP.m_pCurObject;
	//SRenderObjData *pOD = pObj->GetObjData(m_RP.m_nProcessThreadID);
	//if (!pOD)
	//	return;
	//const CShadowUtils::ShadowFrustumIDs * pShadowFrustIDs = CShadowUtils::GetShadowFrustumList(pOD->m_ShadowCasters);
	//if(!pShadowFrustIDs || !pShadowFrustIDs->Count())
	//	return;

	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW];

	if (m_shadowJittering > 0.0f)
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SHADOW_JITTERING];

	// Always use PCF for shadows for transparent
	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE];

	// enable combined shadow maps
	{
		static ICVar* p_e_gsm_combined = 0; //iConsole->GetCVar("e_GsmCombined");		assert(p_e_gsm_combined);

		if (p_e_gsm_combined && p_e_gsm_combined->GetIVal())
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_GSM_COMBINED];
		else
			assert((m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_GSM_COMBINED]) == 0);
	}

	const bool bLegacyShaderPermutations = m_RP.m_pShader->m_eShaderType == eST_Particle;
	FX_SetupForwardShadows(m_RP.m_pCurrentRenderView, bLegacyShaderPermutations);
}

void CD3D9Renderer::FX_SetupShadowsForFog()
{
	PROFILE_FRAME(FX_SetupShadowsForFog);

	m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_POINT_LIGHT] | g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE] | g_HWSR_MaskBit[HWSR_SHADOW_JITTERING] |
	                           g_HWSR_MaskBit[HWSR_SHADOW_MIXED_MAP_G16R16]);

	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE] | g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW];

	FX_SetupForwardShadows(m_RP.m_pCurrentRenderView);
}

void CD3D9Renderer::FX_PrepareDepthMapsForLight(CRenderView* pRenderView, const SRenderLight& rLight, int nLightID, bool bClearPool)
{
	int nThreadID = m_RP.m_nProcessThreadID;

	//render all valid shadow frustums
	auto& SMFrustums = pRenderView->GetShadowFrustumsForLight(nLightID);
	//assert(SMFrustums.size()<=MAX_GSM_LODS_NUM);
	for (int nFrustIdx = SMFrustums.size() - 1; nFrustIdx >= 0; --nFrustIdx)
	{
		ShadowMapFrustum* pCurFrustum = SMFrustums[nFrustIdx]->pFrustum;

		int nLightFrustumID = nLightID * MAX_SHADOWMAP_LOD + (nFrustIdx);
		if (!pCurFrustum)
			continue;

		if (pCurFrustum->m_eReqTT == eTT_1D || pCurFrustum->m_eReqTF == eTF_Unknown)
		{
			// looks like unitialized shadow frustum for 1 frame - some mt issue
			continue;
		}

		const bool bSun = (rLight.m_Flags & DLF_SUN) != 0;

#ifndef _RELEASE
		char frustumLabel[64];

		if (bSun)
		{
			const int nShadowRecur = pCurFrustum->nShadowGenID[nThreadID][0];
			int nSide = 0;
			auto shadowItemsRange = pRenderView->GetShadowItemsRange(pCurFrustum, nSide);
			const int nRendItemCount = nShadowRecur != 0xFFFFFFFF ? (shadowItemsRange.Length()) : 0;

			const char* frustumTextSun[] =
			{
				"GSM FRUSTUM %i",
				"GSM DISTANCE FRUSTUM %i",
				"GSM CACHED FRUSTUM %i",
				"HEIGHT MAP AO FRUSTUM %i",
				"NEAREST FRUSTUM",
				"PER OBJECT FRUSTUM",
				"UNKNOWN"
			};

			frustumLabel[0] = 0;

			if (!pCurFrustum->IsCached() || nRendItemCount > 0)
				cry_sprintf(frustumLabel, frustumTextSun[pCurFrustum->m_eFrustumType], SMFrustums[nFrustIdx]->pFrustum->nShadowMapLod);
		}
		else
		{
			cry_sprintf(frustumLabel, "FRUSTUM %i", nFrustIdx);
		}

		if (frustumLabel[0])
			PROFILE_LABEL_PUSH(frustumLabel);
#endif

		// merge cached shadow maps and corresponding dynamic shadow maps.
		if (bSun && pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance)
		{
			auto arrCachedFrustums = pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunCached);
			auto pCachedFrustum = std::find_if(arrCachedFrustums.begin(), arrCachedFrustums.end(),
			                                   [=](const SShadowFrustumToRender* pFrustumToRender)
			{
				return pFrustumToRender->pFrustum->nShadowMapLod == pCurFrustum->nShadowMapLod;
			});

			pCurFrustum->pDepthTex = NULL;
			if (pCachedFrustum != arrCachedFrustums.end())
			{
				if (!pCurFrustum->pDepthTex)
				{
					SDynTexture_Shadow* pDynTex = SDynTexture_Shadow::GetForFrustum(pCurFrustum);
					pCurFrustum->pDepthTex = pDynTex->m_pTexture;
				}

				FX_MergeShadowMaps(pRenderView, pCurFrustum, (*pCachedFrustum)->pFrustum);
			}
		}

		if (PrepareDepthMap(pRenderView, pCurFrustum, nLightFrustumID, bClearPool))
			bClearPool = false;

#ifndef _RELEASE
		if (frustumLabel[0])
			PROFILE_LABEL_POP(frustumLabel);
#endif
	}
}

void CD3D9Renderer::EF_PrepareCustomShadowMaps(CRenderView* pRenderView)
{
	// prepare depth maps for all custom frustums
	for (auto& pFrustumToRender : pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_Custom))
	{
		if (pFrustumToRender->pFrustum->m_eFrustumType != ShadowMapFrustum::e_Nearest)
		{
			ShadowMapFrustum& curFrustum = *pFrustumToRender->pFrustum;
			PrepareDepthMap(pRenderView, &curFrustum, curFrustum.m_eFrustumType == ShadowMapFrustum::e_Nearest ? -1 : 0, true);
		}
	}
}

void CD3D9Renderer::EF_PrepareAllDepthMaps(CRenderView* pRenderView)
{
	//if (CV_r_UseShadowsPool)
	//  return;

	// get list of shadow casters for nLightID;
	//int nLightID = 0 + 0 * 4;
	int nThreadID = m_RP.m_nProcessThreadID;

	pRenderView->PrepareShadowViews();
	int NumDynLights = pRenderView->GetDynamicLightsCount();

	auto& arrDeferLights = pRenderView->GetLightsArray(eDLT_DynamicLight);

	if (NumDynLights <= 0 && arrDeferLights.size() <= 0)
		return;

	for (int nLightID = 0; nLightID < NumDynLights; nLightID++)
	{
		SRenderLight* pLight = &pRenderView->GetDynamicLight(nLightID);

		if (!(pLight->m_Flags & DLF_CASTSHADOW_MAPS))
			continue;

		FX_PrepareDepthMapsForLight(pRenderView, *pLight, nLightID);
	}

	//////////////////////////////////////////////////////////////////////////
	if (!CV_r_UseShadowsPool)
	{
		for (uint32 nDeferLightID = 0; nDeferLightID < arrDeferLights.size(); nDeferLightID++)
		{
			SRenderLight* pLight = &arrDeferLights[nDeferLightID];
			if (!(pLight->m_Flags & DLF_CASTSHADOW_MAPS))
				continue;

			int nDeferLightIdx = nDeferLightID + NumDynLights;
			assert((unsigned int) nDeferLightIdx < (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS));
			if ((unsigned int) nDeferLightIdx >= (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS))
			{
				Warning("CD3D9Renderer::EF_PrepareAllDepthMaps: Too many light sources used ...");
				return;
			}

			FX_PrepareDepthMapsForLight(pRenderView, *pLight, nDeferLightIdx);
		}
	}

	// prepare custom depth maps
	{
		PROFILE_LABEL_SCOPE("CUSTOM MAPS");
		const uint64 nPrevRTFlags = m_RP.m_FlagsShader_RT;
		uint32 nPrefRendFlags = m_RP.m_nRendFlags;
		m_RP.m_nRendFlags = 0;

		EF_PrepareCustomShadowMaps(pRenderView);

		m_RP.m_nRendFlags = nPrefRendFlags;
		m_RP.m_FlagsShader_RT = nPrevRTFlags;
	}
}

void CD3D9Renderer::FX_MergeShadowMaps(CRenderView* pRenderView, ShadowMapFrustum* pDst, const ShadowMapFrustum* pSrc)
{
	if (!pDst || !pSrc)
		return;

	CRY_ASSERT(pSrc->m_eFrustumType == ShadowMapFrustum::e_GsmCached);
	CRY_ASSERT(pDst->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance);
	CRY_ASSERT(pDst->nShadowMapLod == pSrc->nShadowMapLod);

	const int nThreadID = m_RP.m_nProcessThreadID;

	const int nShadowRecur = pDst->nShadowGenID[nThreadID][0];
	int nSide = 0;
	auto shadowItemsRange = pRenderView->GetShadowItemsRange(pDst, nSide);
	const int nRendItemCount = nShadowRecur != 0xFFFFFFFF ? (shadowItemsRange.Length()) : 0;

	// get crop rectangle for projection
	Matrix44r mReproj = Matrix44r(pDst->mLightViewMatrix).GetInverted() * Matrix44r(pSrc->mLightViewMatrix);
	Vec4r srcClipPosTL = Vec4r(-1, -1, 0, 1) * mReproj;
	srcClipPosTL /= srcClipPosTL.w;

	const float fSnap = 2.0f / pSrc->pDepthTex->GetWidth();
	Vec4 crop = Vec4(
	  crop.x = fSnap * int(srcClipPosTL.x / fSnap),
	  crop.y = fSnap * int(srcClipPosTL.y / fSnap),
	  crop.z = 2.0f * pDst->nTextureWidth / float(pSrc->nTextureWidth),
	  crop.w = 2.0f * pDst->nTextureHeight / float(pSrc->nTextureHeight)
	  );

	Matrix44 cropMatrix(IDENTITY);
	cropMatrix.m00 = 2.f / crop.z;
	cropMatrix.m11 = 2.f / crop.w;
	cropMatrix.m30 = -(1.0f + cropMatrix.m00 * crop.x);
	cropMatrix.m31 = -(1.0f + cropMatrix.m11 * crop.y);

	const bool bOutsideFrustum = abs(crop.x) > 1.0f || abs(crop.x + crop.z) > 1.0f || abs(crop.y) > 1.0f || abs(crop.y + crop.w) > 1.0f;
	const bool bEmptyCachedFrustum = pSrc->nShadowGenMask == 0;
	const bool bRequireCopy = bOutsideFrustum || bEmptyCachedFrustum || nRendItemCount > 0;

	pDst->bIncrementalUpdate = true;
	pDst->mLightViewMatrix = pSrc->mLightViewMatrix * cropMatrix;

	// do we need to merge static shadows into the dynamic shadow map?
	if (bRequireCopy)
	{
		ETEX_Format texFormat = CV_r_ShadowsCacheFormat == 0 ? eTF_D32F : eTF_D16;

		if (!pDst->pDepthTex)
		{
			SDynTexture_Shadow* pDynTex = SDynTexture_Shadow::GetForFrustum(pDst);
			pDst->pDepthTex = pDynTex->m_pTexture;
		}

		SDepthTexture depthSurface;

		depthSurface.nWidth = pDst->nTextureWidth;
		depthSurface.nHeight = pDst->nTextureHeight;
		depthSurface.nFrameAccess = -1;
		depthSurface.bBusy = false;
		depthSurface.pTexture = pDst->pDepthTex;
		depthSurface.pTarget = pDst->pDepthTex->GetDevTexture()->Get2DTexture();
		depthSurface.pSurface = pDst->pDepthTex->GetDeviceDepthStencilView();

		if (bEmptyCachedFrustum)
		{
			gcpRendD3D->FX_ClearTarget(&depthSurface, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane.r, 0);
		}
		else
		{
			uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;

			int iTempX, iTempY, iWidth, iHeight;
			gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

			gcpRendD3D->FX_PushRenderTarget(0, (CTexture*)NULL, &depthSurface);
			gcpRendD3D->FX_SetActiveRenderTargets();
			gcpRendD3D->RT_SetViewport(0, 0, pDst->pDepthTex->GetWidth(), pDst->pDepthTex->GetHeight());

			static CCryNameTSCRC pTechCopyShadowMap("ReprojectShadowMap");
			SPostEffectsUtils::ShBeginPass(CShaderMan::s_ShaderShadowMaskGen, pTechCopyShadowMap, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

			gRenDev->FX_SetState(GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);

			Matrix44 mReprojDstToSrc = pDst->mLightViewMatrix.GetInverted() * pSrc->mLightViewMatrix;
			;
			static CCryNameR paramReprojMatDstToSrc("g_mReprojDstToSrc");
			CShaderMan::s_ShaderShadowMaskGen->FXSetPSFloat(paramReprojMatDstToSrc, (Vec4*) mReprojDstToSrc.GetData(), 4);

			Matrix44 mReprojSrcToDst = pSrc->mLightViewMatrix.GetInverted() * pDst->mLightViewMatrix;
			static CCryNameR paramReprojMatSrcToDst("g_mReprojSrcToDst");
			CShaderMan::s_ShaderShadowMaskGen->FXSetPSFloat(paramReprojMatSrcToDst, (Vec4*) mReprojSrcToDst.GetData(), 4);

			pSrc->pDepthTex->Apply(0, CTexture::GetTexState(STexState(FILTER_POINT, true)));

			SPostEffectsUtils::DrawFullScreenTri(depthSurface.nWidth, depthSurface.nHeight);
			SPostEffectsUtils::ShEndPass();

			gcpRendD3D->FX_PopRenderTarget(0);
			gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

			gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
		}

		pDst->packWidth[0] = pDst->nTextureWidth;
		pDst->packHeight[0] = pDst->nTextureHeight;

		pDst->packX[0] = pDst->packY[0] = 0;
	}
	else
	{
		pDst->packX[0] = int((crop.x * 0.5f + 0.5f) * pSrc->pDepthTex->GetWidth() + 0.5f);
		pDst->packY[0] = int((-(crop.y + crop.w) * 0.5f + 0.5f) * pSrc->pDepthTex->GetHeight() + 0.5f);
		pDst->packWidth[0] = pDst->nTextureWidth;
		pDst->packHeight[0] = pDst->nTextureHeight;

		pDst->pDepthTex = pSrc->pDepthTex;
		pDst->nTexSize = pSrc->nTexSize;
		pDst->nTextureWidth = pSrc->nTextureWidth;
		pDst->nTextureHeight = pSrc->nTextureHeight;
	}

	pDst->fNearDist = pSrc->fNearDist;
	pDst->fFarDist = pSrc->fFarDist;
	pDst->fRendNear = pSrc->fRendNear;
	pDst->fDepthConstBias = pSrc->fDepthConstBias;
	pDst->fDepthTestBias = pSrc->fDepthTestBias;
	pDst->fDepthSlopeBias = pSrc->fDepthSlopeBias;
}
