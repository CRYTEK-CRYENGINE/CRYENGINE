// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DShadows.cpp : shadows support.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/IRenderNode.h>
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

	auto& arrDynLights = pRenderView->GetLightsArray(eDLT_DynamicLight);
	auto& arrDeferLights = pRenderView->GetLightsArray(eDLT_DeferredLight);

	int NumDynLights = arrDynLights.size();
	int NumDeferLights = arrDeferLights.size();
	if ((NumDynLights + NumDeferLights) <= 0)
		return;

	int nSunID = -1;

	{
		auto itr = arrDynLights.begin();
		for (uint32 nDynLightID = 0; itr != arrDynLights.end(); ++itr, ++nDynLightID)
		{
			EF_PrepareShadowGenForLight(pRenderView, &*itr, nDynLightID);

			if (itr->m_Flags & DLF_SUN)
				nSunID = nDynLightID;
		}
	}

	{
		auto itr = arrDeferLights.begin();
		for (uint32 nDeferLightID = NumDynLights; itr != arrDeferLights.end(); ++itr, ++nDeferLightID)
		{
			EF_PrepareShadowGenForLight(pRenderView, &*itr, nDeferLightID);
		}
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

bool CD3D9Renderer::PrepareShadowGenForFrustum(CRenderView* pRenderView, ShadowMapFrustum* pCurFrustum, const SRenderLight* pLight, int nLightID, int nLOD)
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
					CShadowUtils::GetShadowMatrixOrtho(pCurFrustum->mLightProjMatrix, pCurFrustum->mLightViewMatrix, m_CameraMatrix, pCurFrustum, false);
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
	IRenderAuxText::Draw2dLabel((float)x, (float)y, fScale, color, false, "%s", buffer);
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
	FX_DeferredShadowPassSetup(/*shadowMat*/ gRenDev->m_TempMatrices[Num][0], GetCamera(), pFr, (float)vpWidth, (float)vpHeight,
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

			FX_DeferredShadowPassSetupBlend(shadowMatPrev.GetTransposed(), GetCamera(), Num, (float)vpWidth, (float)vpHeight);

			m_cEF.m_TempVecs[2][2] = 1.f / (pPrevFr->fFarDist);

			float fBlendValPrev = pPrevFr->fBlendVal;

			m_cEF.m_TempVecs[15][2] = fBlendValPrev;
			m_cEF.m_TempVecs[15][3] = 1.0f / (1.0f - fBlendValPrev);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	// Noise projection for shadow mask
	Matrix33 mRotMatrix(mLightView);
	mRotMatrix.orthonormalizeFastLH();
	gRenDev->m_TempMatrices[0][1] = Matrix44r(mRotMatrix).GetTransposed() * Matrix44r(gRenDev->m_TempMatrices[Num][0]).GetInverted();

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

			m_cEF.m_TempVecs[2][Num] = 1.f / (pFr->fFarDist);

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
	const int nThreadID = m_RP.m_nProcessThreadID;

	auto& SMFrustums = pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunDynamic);

	if (bUseShaderPermutations)
	{
		m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] |
		                           g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ]);
	}
	else
	{
		for (int i = SMFrustums.size() * 2; i < CRY_ARRAY_COUNT(m_RP.m_ShadowCustomTexBind); ++i)
			m_RP.m_ShadowCustomTexBind[i] = CTexture::s_ptexFarPlane->GetID();
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
		nCascadeMask |= CShadowUtils::eForwardShadowFlags_Cascade0_SingleTap;

	if (m_bCloudShadowsEnabled && m_cloudShadowTexId > 0)
	{
		nCascadeMask |= CShadowUtils::eForwardShadowFlags_CloudsShadows;

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
	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW];

	if (m_shadowJittering > 0.0f)
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SHADOW_JITTERING];

	// Always use PCF for shadows for transparent
	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE];

	const bool bLegacyShaderPermutations = m_RP.m_pShader->m_eShaderType == eST_Particle;
	FX_SetupForwardShadows(m_RP.m_pCurrentRenderView, bLegacyShaderPermutations);
}

