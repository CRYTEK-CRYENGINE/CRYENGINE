// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"

//=========================================================================================

#include <Cry3DEngine/IStatObj.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IRenderNode.h>

#include "Common/RenderView.h"

static SSpriteInfo* sSPInfo = NULL;
static int snSPInfo = 0;
static int snSPInfoMax = -1;

static float sfTableTCFloat[129];
static int snAtlasSize = 0;

static void sInitTCTable()
{
	if (snAtlasSize == CRenderer::CV_r_texatlassize)
		return;
	snAtlasSize = CRenderer::CV_r_texatlassize;

	int i;
	const float fByte2TexMul = 1.0f / 128.0f;
	float fHalfInvWidth = 0.5f / CRenderer::CV_r_texatlassize;
	for (i = 0; i <= 128; i++)
	{
		sfTableTCFloat[i] = i * fByte2TexMul + fHalfInvWidth;
	}
}

void sSetViewPort(int nCurX, int nCurY, int nSpriteRes)
{
	// one pixel border for proper border handling
	gcpRendD3D->RT_SetViewport(nCurX * nSpriteRes + 1, nCurY * nSpriteRes + 1, nSpriteRes - 3, nSpriteRes - 3);
}

static const float sf60Sin = sin(60.0f * (gf_PI / 180.0f));
static const float sf60Cos = cos(60.0f * (gf_PI / 180.0f));

//#define SPRITES_2_TEXTURES 1

void sFlushSprites(SSpriteGenInfo* pSGI, int nCurX, int nCurY, CTexture* pSrcTex, int nTexSizeInt, int nTexHeightFinal)
{
	PROFILE_LABEL_SCOPE("FLUSH_SPRITES");
	int n = 0;
	int ncX = 0;
	int ncY = 0;
	CD3D9Renderer* r = gcpRendD3D;
	r->Set2DMode(true, 1, 1);
	CTexture* pCurRT = r->m_pNewTarget[0]->m_pTex;

	SDynTexture2* pNewTex;
	while (true)
	{
		SSpriteGenInfo* pSG = &pSGI[n++];
		SSpriteInfo* pSP = &sSPInfo[pSG->nSP];
		/*if ((int)pSP->m_vPos[0] == 1049 && (int)pSP->m_vPos[1] == 2069 && (int)pSP->m_vPos[2] == 248)
		   {
		   int nnn = 0;
		   }*/

		// dilate rendered sprite and put into sprite texture -------------------
		pNewTex = (SDynTexture2*)*pSG->ppTexture;
		assert(pNewTex);
		if (!pNewTex)
		{
			r->Set2DMode(false, 1, 1);
			return;
		}

		pNewTex->Update(pNewTex->GetWidth(), pNewTex->GetHeight());

		if (!CTexture::IsTextureExist(pNewTex->m_pTexture))
		{
			assert(0);
			SAFE_RELEASE(pNewTex);
			*pSG->ppTexture = NULL;
			return;
		}
		*pSG->ppTexture = pNewTex;

		uint32 nX, nY, nW, nH;
		pNewTex->GetSubImageRect(nX, nY, nW, nH);
#ifdef SPRITES_2_TEXTURES
		nW >>= 1;
#endif

		CTexture* pT = (CTexture*)pNewTex->GetTexture();
		pSP->m_pTex = pNewTex;
		const uint32 dwWidth = pT->GetWidth();
		const uint32 dwHeight = pT->GetHeight();

		pSP->m_ucTexCoordMinX = (nX * 128) / dwWidth;
		assert((pSP->m_ucTexCoordMinX * dwWidth) / 128 == nX);
		pSP->m_ucTexCoordMinY = (nY * 128) / dwHeight;
		assert((pSP->m_ucTexCoordMinY * dwHeight) / 128 == nY);
		pSP->m_ucTexCoordMaxX = ((nX + nW) * 128) / dwWidth;
		assert((pSP->m_ucTexCoordMaxX * dwWidth) / 128 == nX + nW);
		pSP->m_ucTexCoordMaxY = ((nY + nH) * 128) / dwHeight;
		assert((pSP->m_ucTexCoordMaxY * dwHeight) / 128 == nY + nH);

		CShader* pPostEffects = CShaderMan::s_shPostEffects;

		gcpRendD3D->FX_ApplyShaderQuality(eST_PostProcess);   // needed for GetShaderQuality()

		r->LogStrv("Generating '%s' - %s (%d, %d, %d, %d) (%d)\n", pT->GetName(), pNewTex->IsSecondFrame() ? "Second" : "First", nX, nY, nW, nH, r->GetFrameID(false));

		if (pCurRT != pT)
		{
			pCurRT = pT;
			pNewTex->SetRT(0, false, NULL);
		}
		else
			pNewTex->SetRectStates();
		bool bOk;
#ifdef SPRITES_2_TEXTURES
		bOk = pPostEffects->FXSetTechnique("Dilate2");
#else
		bOk = pPostEffects->FXSetTechnique("Dilate");
#endif
		r->FX_Commit();

		assert(bOk);

		uint32 nPasses = 0;
		pPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		pPostEffects->FXBeginPass(0);

		r->FX_SetState(GS_NODEPTHTEST);

		float fTexSize = (float)nTexSizeInt;
		float fHalfX = 1.0f / (float)pSrcTex->GetWidth();
		float fHalfY = 1.0f / (float)pSrcTex->GetHeight();
		float fSizeX = fTexSize * fHalfX;
		float fSizeY = fTexSize * fHalfY;
		float fOffsX = (float)ncX * fSizeX;
		float fOffsY = (float)ncY * fSizeY;

		Vec4 v;
		v[0] = fHalfX * pNewTex->GetHeight() / pNewTex->GetWidth();
		v[1] = fHalfY;
		v[2] = fSizeX;
		static CCryNameR vPixelOffsetName("vPixelOffset");
		pPostEffects->FXSetPSFloat(vPixelOffsetName, &v, 1);

#ifdef SPRITES_2_TEXTURES
		fSizeX *= 2;
#endif

#ifndef SPRITES_2_TEXTURES
		pSrcTex->Apply(0, EDefaultSamplerStates::PointClamp);
#else
		pSrcTex->Apply(0, EDefaultSamplerStates::LinearClamp);
#endif

		float fZ = 0.5f;
		r->DrawQuad3D(Vec3(0, 0, fZ), Vec3(1, 0, fZ), Vec3(1, 1, fZ), Vec3(0, 1, fZ), Col_White, fOffsX, fOffsY, fOffsX + fSizeX, fOffsY + fSizeY);

		pPostEffects->FXEndPass();

		//    pNewTex->RestoreRT(0, false);

		ncX++;
		if ((ncX + 1) * nTexSizeInt > pSrcTex->GetWidth())
		{
			ncX = 0;
			ncY++;
		}
		if (ncX == nCurX && ncY == nCurY)
			break;
	}

	r->Set2DMode(false, 1, 1);
	gcpRendD3D->EF_Scissor(false, 0, 0, 0, 0);
}

// Overrides of "StartEF / AddEF / EndEF" for rendering thread
static void sRT_StartEf()
{
}
static void sRT_AddEf(CRenderElement* pRE, SShaderItem& SH, CRenderObject* pObj, int nList, int nAW, const SRenderingPassInfo& passInfo)
{
	CShader* pSH = (CShader*)SH.m_pShader;
	if (pSH->m_Flags & EF_NODRAW)
		return;
	CD3D9Renderer* rd = gcpRendD3D;

	if (SH.m_nPreprocessFlags == -1 || (pSH->m_Flags & EF_RELOADED))
	{
		if (!SH.Update())
			return;
	}

	int nThreadID = passInfo.ThreadID();
	uint32 nBatchFlags = rd->EF_BatchFlags(SH, pObj, pRE, passInfo, nAW);
	if (nBatchFlags & FB_TRANSPARENT)
		nList = EFSLIST_TRANSP;

	passInfo.GetRenderView()->AddRenderItem(pRE, pObj, SH, nList, nBatchFlags, passInfo.GetRendItemSorter(), passInfo.IsShadowPass(), false);
}

static void sRT_RenderObject(IStatObj* pEngObj, SRendParams& RP, const SRenderingPassInfo& passInfo)
{
	CD3D9Renderer* rd = gcpRendD3D;
	int nThreadID = rd->m_RP.m_nProcessThreadID;

	CRenderObject* pObj = rd->EF_GetObject_Temp(nThreadID);

	pObj->m_ObjFlags |= FOB_TRANS_MASK;
	pObj->m_ObjFlags |= RP.dwFObjFlags;
	pObj->m_II.m_Matrix = *RP.pMatrix;
	pObj->m_nClipVolumeStencilRef = 0;
	pObj->m_II.m_AmbColor = RP.AmbientColor;
	pObj->m_ObjFlags |= FOB_INSHADOW;
	pObj->m_fAlpha = RP.fAlpha;
	pObj->m_fDistance = RP.fDistance;
	pObj->m_nMaterialLayers = RP.nMaterialLayersBlend;
	pObj->m_pCurrMaterial = RP.pMaterial;
	pObj->m_pRenderNode = RP.pRenderNode;
	assert(pEngObj->GetRenderMesh());
	if (!pEngObj->GetRenderMesh())
		return;
	CRenderMesh* pMesh = (CRenderMesh*)pEngObj->GetRenderMesh();
	IMaterial* pMaterial = RP.pMaterial;
	if (!pMaterial)
		pMaterial = pEngObj->GetMaterial();
	assert(pMaterial);
	if (!pMaterial)
		return;

	TRenderChunkArray* pChunks = &pMesh->m_Chunks;
	const uint32 ni = (uint32)pChunks->size();
	for (uint32 i = 0; i < ni; i++)
	{
		CRenderChunk* pChunk = &pChunks->at(i);
		CRenderElement* pREMesh = pChunk->pRE;

		SShaderItem& ShaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
		if (pREMesh && ShaderItem.m_pShader && ShaderItem.m_pShaderResources)
		{
			sRT_AddEf(pREMesh, ShaderItem, pObj, EFSLIST_GENERAL, 0, passInfo);
		}
	}
}

static void sRT_ADDDlight(SRenderLight* Source, const SRenderingPassInfo& passInfo)
{
	Source->m_Id = gRenDev->m_RP.RenderView()->AddDynamicLight(*Source);
}

static void sRT_EndEf(const SRenderingPassInfo& passInfo)
{
	CD3D9Renderer* rd = gcpRendD3D;
	int nThreadID = rd->m_RP.m_nProcessThreadID;

	rd->m_RP.m_ForceStateAnd |= GS_ALPHATEST;

	rd->m_pRT->RC_RenderScene(passInfo.GetRenderView(), SHDF_ALLOWHDR, CD3D9Renderer::FX_FlushShader_General);

	rd->m_RP.m_ForceStateAnd &= ~GS_ALPHATEST;
}

static void sCreateFT(bool bHDR)
{
	if (bHDR)
		CTexture::GenerateHDRMaps();
	else
		CTexture::GenerateSceneMap(eTF_R8G8B8A8);
}

static bool AreTexturesStreamed(IMaterial* pMaterial, float fMipFactor)
{
	// check texture streaming distances
	if (pMaterial)
	{
		if (IRenderShaderResources* pRes = pMaterial->GetShaderItem().m_pShaderResources)
		{
			for (int iSlot = 0; iSlot < EFTT_MAX; ++iSlot)
			{
				if (SEfResTexture* pResTex = pRes->GetTexture(iSlot))
				{
					if (ITexture* pITex = pResTex->m_Sampler.m_pITex)
					{
						float fCurMipFactor = fMipFactor * pResTex->GetTiling(0) * pResTex->GetTiling(1);

						if (!pITex->IsParticularMipStreamed(fCurMipFactor))
							return false;
					}
				}
			}
		}
	}

	return true;
}

void CD3D9Renderer::MakeSprites(TArray<SSpriteGenInfo>& SGI, const SRenderingPassInfo& passInfoOriginal)
{
	PROFILE_LABEL_SCOPE("MAKE_SPRITES");
	PROFILE_FRAME(MakeSprites);

	assert(m_pRT->IsRenderThread());

	int nThreadID = m_RP.m_nProcessThreadID;

	bool bHDR = false;

	CTexture* pTempTex = bHDR ? CTexture::s_ptexHDRTarget : CTexture::s_ptexBackBuffer;
	if (!pTempTex)
	{
		sCreateFT(bHDR);
		pTempTex = bHDR ? CTexture::s_ptexHDRTarget : CTexture::s_ptexBackBuffer;
	}
	assert(CTexture::IsTextureExist(pTempTex));
	if (!CTexture::IsTextureExist(pTempTex))
		return;
	int nWidth = GetWidth();
	int nHeight = GetHeight();
	assert(pTempTex && nWidth == pTempTex->GetWidth() && nHeight == pTempTex->GetHeight());
	int nSpriteResFinal = CV_r_VegetationSpritesTexRes;
	int nSpriteResInt = nSpriteResFinal << 1;
	int nSprX = nWidth / nSpriteResInt;
	int nSprY = nHeight / nSpriteResInt;
	int nCurX = 0;
	int nCurY = 0;
	SDepthTexture* pDepthSurf = &m_DepthBufferOrig;
	if (m_RP.m_MSAAData.Type > 1)
	{
		assert(pTempTex->GetFlags() & FT_USAGE_RENDERTARGET);
	}

	SRendParams rParms;
	CShader* sh = m_RP.m_pShader;
	int nTech = m_RP.m_nShaderTechnique;
	CShaderResources* pRes = m_RP.m_pShaderResources;
	CRenderObject* pObj = m_RP.m_pCurObject;
	SShaderTechnique* pTech = m_RP.m_pCurTechnique;
	SShaderTechnique* pRootTech = m_RP.m_pRootTechnique;
	CRenderElement* pRE = m_RP.m_pRE;

	//int nPrevStr = CV_r_texturesstreamingsync;
	int nPrevAsync = CV_r_shadersasynccompiling;
	//CV_r_texturesstreamingsync = 1;
	CV_r_shadersasynccompiling = 0;
	int nPrevZpass = CV_r_usezpass;
	CV_r_usezpass = 0;

	rParms.dwFObjFlags |= FOB_TRANS_MASK;
	rParms.pRenderNode = (struct IRenderNode*)(intptr_t)-1; // avoid random skipping of rendering

	Vec3 cSkyBackup = gEnv->p3DEngine->GetSkyColor();
	Vec3 cSunBackup = gEnv->p3DEngine->GetSunColor();

	SRenderLight light;
	light.m_Flags |= DLF_DIRECTIONAL | DLF_SUN | DLF_THIS_AREA_ONLY | DLF_LM | DLF_SPECULAROCCLUSION | DLF_CASTSHADOW_MAPS;
	light.SetLightColor(cSunBackup);
	light.m_SpecMult = 0;
	light.SetRadius(100000000);

	int vX, vY, vWidth, vHeight;
	GetViewport(&vX, &vY, &vWidth, &vHeight);

	CCamera origCam = GetCamera();
	uint32 saveFlags = m_RP.m_TI[nThreadID].m_PersFlags;
	uint32 saveFlags2 = m_RP.m_PersFlags2;
	m_RP.m_TI[nThreadID].m_PersFlags |= RBPF_MAKESPRITE;
	m_RP.m_PersFlags2 &= ~(RBPF2_NOALPHABLEND | RBPF2_NOALPHATEST);
	m_RP.m_StateAnd |= (GS_BLEND_MASK | GS_ALPHATEST);

	EF_PushFog();
	EF_PushMatrix();
	m_RP.m_TI[nThreadID].m_matProj->Push();
	EnableFog(false);

	const Vec3 vEye(0, 0, 0);
	const Vec3 vAt(-1, 0, 0);
	const Vec3 vUp(0, 0, 1);
	Matrix44A mView;
	mathMatrixLookAt(&mView, vEye, vAt, vUp);

	const bool bReverseDepth = !!(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH);
	const RECT rect = { 0, 0, m_width, m_height };

	FX_ClearTarget(pTempTex, Clr_WhiteTrans, 1, &rect, true);
	FX_ClearTarget(pDepthSurf, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_R.r, 0, 1, &rect, true);
	FX_PushRenderTarget(0, pTempTex, pDepthSurf);
	RT_SetViewport(0, 0, m_width, m_height);
	FX_Commit();

	int nStart = 0;
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpriteUpdates += SGI.Num();
#endif
	for (uint32 i = 0; i < SGI.Num(); i++)
	{
		// Needs a custom dedicated CRenderView
		CRenderView spritesRenderView("Sprites View", CRenderView::eViewType_Default);

		// Make a recursive pass info
		SRenderingPassInfo passInfo(SRenderingPassInfo::CreateTempRenderingInfo(0, passInfoOriginal));

		SSpriteGenInfo& SG = SGI[i];
		SSpriteInfo* pSP = &sSPInfo[SG.nSP];
		/*if ((int)pSP->m_vPos[0] == 1049 && (int)pSP->m_vPos[1] == 2069 && (int)pSP->m_vPos[2] == 248)
		   {
		   int nnn = 0;
		   }*/
		if (CV_r_VegetationSpritesNoGen && CTexture::s_ptexNoTexture)
		{
			SAFE_DELETE(*SG.ppTexture);
			SSpriteInfo* pSpriteInfo = &sSPInfo[SG.nSP];
			pSpriteInfo->m_pTex = NULL;
			continue;
		}
		if (!nCurX && !nCurY && i)
		{
			FX_ClearTarget(pTempTex, Clr_WhiteTrans, 1, &rect, true);
			FX_ClearTarget(pDepthSurf, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_R.r, 0, 1, &rect, true);
			FX_SetRenderTarget(0, pTempTex, pDepthSurf);
			RT_SetViewport(0, 0, m_width, m_height);
			FX_Commit();
		}

		rParms.nMaterialLayers = SG.nMaterialLayers;  // pass material layers info
		rParms.pMaterial = SG.pMaterial;

#ifdef DO_RENDERLOG
		if (CV_r_log)
			Logv("****************** CD3D9Renderer::MakeSprite - Begin ******************\n");
#endif
		float fRadiusHors = SG.pStatObj->GetRadiusHors();
		float fRadiusVert = SG.pStatObj->GetRadiusVert();

		Vec3 vCenter = SG.pStatObj->GetVegCenter();

		float fFOV = 0.565f / SG.fGenDist * 200.f * (gf_PI / 180.0f);
		float fDrawDist = fRadiusVert * SG.fGenDist;

		// make fake camera to pass some camera into to the rendering pipeline
		CCamera tmpCam = origCam;
		Matrix33 matRot = Matrix33::CreateOrientation(vAt - vEye, vUp, 0);
		tmpCam.SetMatrix(Matrix34(matRot, Vec3(0, 0, 0)));
		tmpCam.SetFrustum(nSpriteResInt, nSpriteResInt, fFOV, max(0.1f, fDrawDist - fRadiusHors), fDrawDist + fRadiusHors);
		SetCamera(tmpCam);

		Matrix44A* m = m_RP.m_TI[nThreadID].m_matProj->GetTop();
		mathMatrixPerspectiveFov(m, fFOV, fRadiusHors / fRadiusVert, fDrawDist - fRadiusHors, fDrawDist + fRadiusHors);
		m_RP.m_TI[nThreadID].m_PersFlags |= RBPF_FP_MATRIXDIRTY;

		*m_RP.m_TI[nThreadID].m_matView->GetTop() = mView;
		m_RP.m_TI[nThreadID].m_matCameraZero = mView;

		RT_SetCameraInfo();

		// Start sprite gen
		float globalMultiplier(1);

		// setup environment ------------------
		Matrix34A matTrans;
		matTrans.SetIdentity();
		matTrans.SetTranslation(Vec3(-fDrawDist, 0, 0));

		Matrix34A matRotation;
		matRotation = matRotation.CreateRotationZ((SG.fAngle) * (gf_PI / 180.0f));

		//	  Matrix34A matRotation2;
		//	  matRotation2 = matRotation2.CreateRotationY((SG.fAngle2)*(gf_PI/180.0f));
		//	  matRotation = matRotation2 * matRotation;

		Matrix34A matCenter;
		matCenter.SetIdentity();
		matCenter.SetTranslation(-vCenter);
		matCenter = matRotation * matCenter;

		Matrix34 mat = matTrans * matCenter;

		rParms.pMatrix = &mat;

		// sun (rotate sun direction)
		light.SetPosition(matRotation.TransformVector(gEnv->p3DEngine->GetSunDir()));

		{
			rParms.AmbientColor = gEnv->p3DEngine->GetSkyColor(); // Vegetation ambient colour comes from the sky
			rParms.AmbientColor.a = SG.fBrightness;
			// gEnv->p3DEngine->SetSkyColor(Vec3(0,0,0));

			sRT_StartEf();
			sSetViewPort(nCurX, nCurY, nSpriteResInt);
			sRT_ADDDlight(&light, passInfo);
			sRT_RenderObject(SG.pStatObj, rParms, passInfo);
			sRT_EndEf(passInfo);

			//gEnv->p3DEngine->SetSkyColor(cSkyBackup);

			nCurX++;
		}

		if ((nCurX + 1) * nSpriteResInt > pTempTex->GetWidth())
		{
			nCurX = 0;
			nCurY++;
			if ((nCurY + 1) * nSpriteResInt > pTempTex->GetHeight())
			{
				sFlushSprites(&SGI[nStart], nCurX, nCurY, pTempTex, nSpriteResInt, nSpriteResFinal);
				nStart = i + 1;
				nCurY = 0;
			}
		}

		// check regeneration in case of streaming
		{
			if (*SG.ppTexture)
			{
				IMaterial* pMaterial = SG.pMaterial ? SG.pMaterial : SG.pStatObj->GetMaterial();
				IRenderMesh* pRenderMesh = SG.pStatObj->GetRenderMesh();
				bool needRegenerate = false;

				if (pRenderMesh)
				{
					float fRealDistance = fDrawDist * tan(fFOV / 2.0f) * 2.0f;
					float fMipFactor = fRealDistance * fRealDistance / (nSpriteResInt * nSpriteResInt) * fRadiusHors / fRadiusVert;

					*SG.pMipFactor = fMipFactor;

					TRenderChunkArray* pChunks = &pRenderMesh->GetChunks();

					if (pChunks)
					{
						for (int j = 0; j < pChunks->size(); j++)
						{
							CRenderChunk& currentChunk = (*pChunks)[j];
							IMaterial* pCurrentSubMtl = pMaterial->GetSubMtl(currentChunk.m_nMatID);
							float fCurrentMipFactor = fMipFactor * currentChunk.m_texelAreaDensity;

							if (!AreTexturesStreamed(pCurrentSubMtl, fCurrentMipFactor))
							{
								pCurrentSubMtl->RequestTexturesLoading(fCurrentMipFactor);
								needRegenerate = true;
							}
						}
					}
				}

				if (needRegenerate)
				{
					((SDynTexture2*)(*SG.ppTexture))->m_nFlags |= IDynTexture::fNeedRegenerate;
					*SG.pTexturesAreStreamedIn = false;
				}
				else
				{
					((SDynTexture2*)(*SG.ppTexture))->m_nFlags &= ~IDynTexture::fNeedRegenerate;
					*SG.pTexturesAreStreamedIn = true;
				}
			}
		}
	}
	if (nCurX || nCurY)
		sFlushSprites(&SGI[nStart], nCurX, nCurY, pTempTex, nSpriteResInt, nSpriteResFinal);

	FX_PopRenderTarget(0);

	/*char name[256];
	   cry_sprintf(name, "%s.dds", pTempTex->GetName());
	   pTempTex->SaveDDS(name, false);

	   IDynTexture *pD = (*SGI[0].ppTexture);
	   CTexture *pDst = (CTexture *)pD->GetTexture();
	   cry_sprintf(name, "%s.dds", pDst->GetName());
	   pDst->SaveDDS(name, false);*/

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// End sprite gen
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//SAFE_DELETE(pTempTex);

	EF_PopMatrix();
	m_RP.m_TI[nThreadID].m_matProj->Pop();

	//  gEnv->p3DEngine->SetSkyColor(cSkyBackup);
	//gEnv->p3DEngine->SetSunColor(cSunBackup);
	CV_r_usezpass = nPrevZpass;

	//CV_r_texturesstreamingsync = nPrevStr;
	CV_r_shadersasynccompiling = nPrevAsync;

	RT_SetViewport(vX, vY, vWidth, vHeight);
	EF_PopFog();
	FX_Commit();

	m_RP.m_TI[nThreadID].m_PersFlags = saveFlags;
	m_RP.m_PersFlags2 = saveFlags2;

	SetCamera(origCam);
#ifdef DO_RENDERLOG
	if (CV_r_log)
		Logv("****************** CD3D9Renderer::MakeSprite - End ******************\n");
#endif
	m_RP.m_TI[nThreadID].m_PersFlags &= ~RBPF_MAKESPRITE;
	CHWShader_D3D::mfSetGlobalParams();

	if (!passInfoOriginal.IsRecursivePass())
	{
		EF_ClearTargetsLater(FRT_CLEAR);
	}

	m_RP.m_pShader = sh;
	m_RP.m_pCurTechnique = pTech;
	m_RP.m_pShaderResources = pRes;
	m_RP.m_pCurObject = pObj;
	m_RP.m_pRootTechnique = pRootTech;
	m_RP.m_nShaderTechnique = nTech;
	m_RP.m_pRE = pRE;
}

inline bool CompareSpriteItem(const SSpriteInfo& p1, const SSpriteInfo& p2)
{
	if (p1.m_pTerrainTexInfo != p2.m_pTerrainTexInfo)
		return p1.m_pTerrainTexInfo < p2.m_pTerrainTexInfo;
	return p1.m_pTex < p2.m_pTex;
}

void CD3D9Renderer::GenerateObjSprites(PodArray<struct SVegetationSpriteInfo>* pList, const SRenderingPassInfo& passInfo)
{
	if (pList == 0)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

#ifndef _RELEASE
	bool bFirstInFrame = false;
#endif
	int nLightingUpdates = 0;

	static TArray<SSpriteGenInfo> sSGI;

	// reset stats counting
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	m_SpriteCellsUsed.clear();
	m_SpriteAtlasesUsed.clear();
#endif

	I3DEngine* pEngine = gEnv->p3DEngine;

	const uint32 dwSunSkyRel = (uint32)(256.0f * pEngine->GetSunRel());
	assert(dwSunSkyRel <= 256);                                                                     // 0..256
	const uint32 dwSkySunRel = 256 - dwSunSkyRel;
	assert(dwSkySunRel <= 256);                                                                     // 0..256

	const CCamera& rCamera = GetCamera();
	const Vec3& vCamPos = rCamera.GetPosition();//m_RP.m_TI[m_RP.m_nProcessThreadID].m_rcam.vOrigin;
	const float rad2deg = 180.0f / PI;

	int nPolygonMode = CV_r_wireframe;
	int nZPass = CV_r_usezpass;
#ifndef CONSOLE_CONST_CVAR_MODE
	CV_r_wireframe = 0;
#endif
	CV_r_usezpass = 0;

	static uint32 s_dwOldMakeSpriteCalls = 0;

	int e_debug_mask = 0;//pVarDebugMask->GetIVal();

	static ICVar* pVarDR = iConsole->GetCVar("e_VegetationSpritesDistanceRatio");

	float fCustomDistRatio = pVarDR->GetFVal();
	float fTerrainLMThreshold = .1f;
	float fDirThreshold = fTerrainLMThreshold * pEngine->GetSunDir().GetLength();
	float fColorThreshold = fTerrainLMThreshold;

	int nTexHeightFinal = CV_r_VegetationSpritesTexRes;
	int nSizeSprX = nTexHeightFinal;
#ifdef SPRITES_2_TEXTURES
	nSizeSprX <<= 1;
#endif

	Vec3 vSunDirWS = pEngine->GetSunDir();
	Vec3 vSunCol = pEngine->GetSunColor();
	Vec3 vSkyCol = pEngine->GetSkyColor();

	sSGI.SetUse(0);
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSprites += snSPInfo;
#endif
	snSPInfo = 0;
	//static byte *sPtr;
	if (snSPInfoMax < pList->Count())
	{
		snSPInfoMax = pList->Count();
		if (sSPInfo)
		{
			delete[] sSPInfo;
		}
		sSPInfo = new SSpriteInfo[snSPInfoMax];
	}
	int nS = 0;

	SVegetationSpriteInfo* const pStart = &pList->GetAt(0);
	int i;
	for (i = 0; i < pList->Count(); i++)
	{
		SVegetationSpriteInfo& vi = pStart[i];

		Vec3 vDir = vi.sp.center - vCamPos;

		float fSqDist2d = vDir.x * vDir.x + vDir.y * vDir.y;

		float fSqDist3d = (fSqDist2d + vDir.z * vDir.z);

		if (!rCamera.IsSphereVisible_F(vi.sp))
			continue;

		SSpriteInfo* const pSP = &sSPInfo[nS];
		nS++;

		pSP->m_nVI = i;

		pSP->m_pTex = NULL;
		pSP->m_vPos = vi.sp.center;

		// isqrt corrected by one newton-raphson iteration on platforms where isqrt is approx,
		// otherwise no iteration.
		float fMul = vi.fScaleH * isqrt_tpl(fSqDist2d);
		pSP->m_fDY = vDir.x * fMul;
		pSP->m_fDX = vDir.y * fMul;
		pSP->m_fScaleV = vi.fScaleV;

		pSP->m_pTerrainTexInfo = vi.pTerrainTexInfo;

		SVegetationSpriteLightInfo* pLightInfo = vi.pLightInfo;

		// only do N updates per frame - slice across multiple frames if many sprites are invalidated
		bool bLightInfoIsUpToDate = nLightingUpdates > CRenderer::CV_r_VegetationSpritesMaxLightingUpdates ||
		                            pLightInfo->IsEqual(vSunDirWS, fDirThreshold);
		//upcasting violates strict aliasing, since a reference is required, use alias_cast to not break strict aliasing rules
		SDynTexture2*& pTex = *alias_cast<SDynTexture2**>(&pLightInfo->m_pDynTexture);

		bool bStreamingStateIsUpToDate = !(bLightInfoIsUpToDate && pTex && (pTex->GetFlags() & IDynTexture::fNeedRegenerate) && vi.pStatInstGroup->nTexturesAreStreamedIn);

		if (!bLightInfoIsUpToDate || !bStreamingStateIsUpToDate || !pTex || !pTex->_IsValid() || (pTex->m_nHeight != nTexHeightFinal) || CV_r_VegetationSpritesGenAlways)
		{
			IStatObj* pReadyLod = NULL;

			if (vi.pStatInstGroup->pStatObj)
			{
				int nReadyLod = vi.pStatInstGroup->pStatObj->FindHighestLOD(0);
				if (nReadyLod >= 0)
					pReadyLod = vi.pStatInstGroup->pStatObj->GetLodObject(nReadyLod);
			}

			if (!pReadyLod)
			{
				nS--;
				continue;
			}

#ifndef _RELEASE
			if (CV_r_VegetationSpritesGenDebug)
			{
				if (!bFirstInFrame)
					gEnv->pLog->Log("Sprites this frame------------");

				bFirstInFrame = true;

				gEnv->pLog->Log("Regenerating vegetation sprite: ");

				if (!pTex)
					gEnv->pLog->LogPlus(" !pTex");
				else if (pTex && !pTex->_IsValid())
					gEnv->pLog->LogPlus(" !pTex->_IsValid()");
				else if (!bLightInfoIsUpToDate)
					gEnv->pLog->LogPlus(" !bLightInfoIsUpToDate");
				else if (!bStreamingStateIsUpToDate)
					gEnv->pLog->LogPlus(" !bStreamingStateIsUpToDate");
				else if (pTex && (pTex->m_nHeight != nTexHeightFinal))
					gEnv->pLog->LogPlus(" pTex->m_nHeight != nTexHeightFinal");
				else if (CV_r_VegetationSpritesGenAlways)
					gEnv->pLog->LogPlus(" CV_r_VegetationSpritesGenAlways");
				else
					gEnv->pLog->LogPlus(" Unknown");
			}
#endif

			if (!pTex || (pTex->m_nHeight != nTexHeightFinal))
			{
				SAFE_RELEASE(pTex);

				char* szName;

#ifdef _DEBUG
				char Name[128];
				cry_sprintf(Name, "$SpriteTex_(%1.f %1.f %1.f)_%d", pSP->m_vPos.x, pSP->m_vPos.y, pSP->m_vPos.z, vi.ucSlotId);
				szName = Name;
#else
				szName = "$SpriteTex";
#endif

				int nDownScaleX = 1;
				Vec3 objSize = vi.pStatInstGroup->pStatObj->GetAABB().GetSize();
				if (objSize.z > objSize.x * 2 && objSize.z > objSize.y * 2)
					nDownScaleX = 2;

				int nTexX = nTexHeightFinal / nDownScaleX;
#ifdef SPRITES_2_TEXTURES
				nTexX <<= 1;
#endif

				pTex = new SDynTexture2(nTexX, nTexHeightFinal, FT_STATE_CLAMP | FT_NOMIPS, szName, eTP_Sprites);
				bool bRet = pTex->Update(nTexX, nTexHeightFinal);
				if (bRet == false || !pTex->_IsValid())
				{
					__debugbreak();
					SDynTexture2* pTexTest = new SDynTexture2(nTexX, nTexHeightFinal, FT_STATE_CLAMP | FT_NOMIPS, szName, eTP_Sprites);
					bRet = pTexTest->Update(nTexX, nTexHeightFinal);
				}
				if (!CTexture::IsTextureExist(pTex->m_pTexture))
				{
					Warning("Failed to allocate texture atlas for vegetation sprites.\n");
					SAFE_RELEASE(pTex);
					nS--;
					continue;
				}
			}

			if (!bLightInfoIsUpToDate || !bStreamingStateIsUpToDate)
			{
				if (pTex)
					pTex->ResetUpdateMask();
				pLightInfo->SetLightingData(pEngine->GetSunDir());
			}

			if (!bLightInfoIsUpToDate)
			{
				nLightingUpdates++;
			}

			if (pTex)
				pTex->SetUpdateMask();

			SSpriteGenInfo SGI;
			SGI.fAngle = FAR_TEX_ANGLE * vi.ucSlotId + 90.f;
			SGI.fGenDist = 18.f * max(0.5f, vi.pStatInstGroup->fSpriteDistRatio * fCustomDistRatio);
			SGI.nMaterialLayers = vi.pStatInstGroup->nMaterialLayers;
			SGI.pMipFactor = &pLightInfo->m_MipFactor;
			SGI.pTexturesAreStreamedIn = &vi.pStatInstGroup->nTexturesAreStreamedIn;
			SGI.ppTexture = &pTex;
			SGI.fBrightness = vi.pStatInstGroup->fBrightness;
			SGI.pMaterial = vi.pStatInstGroup->pMaterial;
			SGI.fBrightness = vi.pStatInstGroup->fBrightness;
			SGI.pStatObj = pReadyLod;
			SGI.nSP = nS - 1;
#if !defined(_RELEASE)
			bool add = true;
			if (SGI.pStatObj->GetRadiusVert() < 1e-2)
			{
				const char* pGroupFilePath = vi.pStatInstGroup->szFileName;
				Vec3 loc = vi.sp.center;
				gEnv->pLog->LogWarning("Trying to generate sprite for non suitable stat object. Please turn off sprite gen for group \"%s\" around (%.3f, %.3f, %.3f) and re-export the level!", pGroupFilePath, loc.x, loc.y, loc.z);
				add = false;
			}
			IF (add, 1)
#endif
			sSGI.AddElem(SGI);
		}
		else
		{
			CTexture* pT = (CTexture*)pTex->GetTexture();
			pT->m_nAccessFrameID = m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameUpdateID;
			const uint32 dwWidth = pT->GetWidth();
			const uint32 dwHeight = pT->GetHeight();
			pSP->m_pTex = pTex;
			uint32 nX, nY, nW, nH;
			pTex->GetSubImageRect(nX, nY, nW, nH);
#ifdef SPRITES_2_TEXTURES
			nW >>= 1;
#endif
			pSP->m_ucTexCoordMinX = (nX * 128) / dwWidth;
			assert((pSP->m_ucTexCoordMinX * dwWidth) / 128 == nX);
			pSP->m_ucTexCoordMinY = (nY * 128) / dwHeight;
			assert((pSP->m_ucTexCoordMinY * dwHeight) / 128 == nY);
			pSP->m_ucTexCoordMaxX = ((nX + nW) * 128) / dwWidth;
			assert((pSP->m_ucTexCoordMaxX * dwWidth) / 128 == nX + nW);
			pSP->m_ucTexCoordMaxY = ((nY + nH) * 128) / dwHeight;
			assert((pSP->m_ucTexCoordMaxY * dwHeight) / 128 == nY + nH);
		}

#define  bcolor0 0
#define  bcolor1 1
#define  bcolor2 2
#define  bcolor3 3

		pSP->m_Color.bcolor[bcolor0] = vi.ucAlphaTestRef;
		pSP->m_Color.bcolor[bcolor1] = 255;

#ifdef SPRITES_2_TEXTURES
		if (pTex)
		{
			assert((pTex)->GetWidth() == 64 || (pTex)->GetWidth() == 128);
			pSP->m_Color.bcolor[bcolor2] = (pTex)->GetWidth() == 64 ? 0x40 : 0xc0;
		}
#endif

		pSP->m_Color.bcolor[bcolor2] = vi.ucDissolveOut;
	}

	if (sSGI.Num())
		MakeSprites(sSGI, passInfo);

	snSPInfo = nS;

	if (!CV_r_VegetationSpritesGenAlways)
	{
		if (sSGI.Num() > 100 && s_dwOldMakeSpriteCalls > 100)
		{
			gEnv->pLog->LogWarning("MakeSprite() was called %u times in this frame and %u last frame (quick player view change/fast moving sun/too many sprites/low dynamic texture memory)", sSGI.Num(), s_dwOldMakeSpriteCalls);
		}
	}

	if ((e_debug_mask & 0x20) != 0)
	{
		gEnv->pLog->Log("MakeSprite() was called %u times in this frame (%d objects)", sSGI.Num(), pList->Count());
	}

	s_dwOldMakeSpriteCalls = sSGI.Num();

#ifndef CONSOLE_CONST_CVAR_MODE
	CV_r_wireframe = nPolygonMode;
#endif
	CV_r_usezpass = nZPass;
}

void CD3D9Renderer::ObjSpritesFlush(SVF_P3F_C4B_T2F* pVerts, uint16* pInds, int nSprites, void*& pCurVB, SShaderTechnique* pTech, bool bZ)
{
	assert(pVerts && pInds);
	assert(pTech);
	if (!pTech)
		return;

	const int nVerts = nSprites * 4;
	TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(pVerts, nVerts, 0);

	const int nInds = nSprites * 6;
	TempDynIB16::CreateFillAndBind(pInds, nInds);

	bool bSunShadowExist = false;

	if (m_RP.RenderView()->GetDynamicLightsCount() > 0 &&
	    ((m_RP.RenderView()->GetDynamicLight(0).m_Flags & (DLF_SUN | DLF_CASTSHADOW_MAPS)) == (DLF_SUN | DLF_CASTSHADOW_MAPS)))
	{
		bSunShadowExist = true;
	}

	//set shadow mask texture
	if (!bZ)
	{
		if (bSunShadowExist)
			CTexture::s_ptexBackBuffer->Apply(1, EDefaultSamplerStates::PointClamp);
		else
			CTexture::s_ptexBlackAlpha->Apply(1, EDefaultSamplerStates::PointClamp);
	}

	SShaderPass* pPass = &pTech->m_Passes[0];
	for (uint32 i = 0; i < pTech->m_Passes.Num(); i++, pPass++)
	{
		m_RP.m_pCurPass = pPass;
		if (!pPass->m_VShader || !pPass->m_PShader)
			continue;
		CHWShader_D3D* curVS = (CHWShader_D3D*)pPass->m_VShader;
		CHWShader_D3D* curPS = (CHWShader_D3D*)pPass->m_PShader;

		curPS->mfSet(0);
		curPS->mfSetParametersPI(NULL, NULL);

		curVS->mfSet(0);
		curVS->mfSetParametersPI(NULL, NULL);

		FX_Commit();

		FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, nVerts, 0, nInds);
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpriteDIPS++;
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpritePolys += nInds / 3;
#endif
	}
}

CTexture* gTexture2;

void CD3D9Renderer::DrawObjSprites(PodArray<SVegetationSpriteInfo>* pList, SSpriteInfo* pSPInfo, const int nSPI, bool bZ, bool bShadows)
{
	uint32 i;

	gRenDev->m_cEF.mfRefreshSystemShader("FarTreeSprites", CShaderMan::s_ShaderTreeSprites);

	CTexture* pPrevTex = NULL;
	SSectorTextureSet* pPrevTerrainTexInfo = NULL;

	uint32 nState;
	int nAlphaRef = 0;
	uint64 nPrevFlagsShader_RT = m_RP.m_FlagsShader_RT;

	{
		if (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_IMPOSTERGEN)
			nState = GS_DEPTHWRITE;
		else
		{
			nState = GS_DEPTHWRITE;
			//nAlphaRef = 40;
		}
	}

	FX_ZState(nState);

	if (bShadows)
	{
		int nColorMask = ~(1 << GS_COLMASK_SHIFT) & GS_COLMASK_MASK;
		nState |= nColorMask | GS_BLSRC_ONE | GS_BLDST_ONE;
	}

	FX_SetState(nState, nAlphaRef);

	D3DSetCull(eCULL_None);

	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];
#ifdef SPRITES_2_TEXTURES
	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
#endif
	CShaderResources rs;
	CShaderResources* pResSave = m_RP.m_pShaderResources;
	m_RP.m_pShaderResources = &rs;
	rs.m_AlphaRef = (float)nAlphaRef / 255.0f;

	m_RP.m_PersFlags1 &= ~RBPF1_USESTREAM_MASK;

	m_RP.m_FlagsStreams_Decl = 0;
	m_RP.m_FlagsStreams_Stream = 0;

	InputLayoutHandle nf = EDefaultInputLayouts::P3F_C4B_T2F;
	m_RP.m_CurVFormat = nf;
	m_RP.m_FlagsShader_MD = 0;
	m_RP.m_FlagsShader_MDV = 0;
	m_RP.m_FlagsShader_LT = 0;

	m_cEF.s_ShaderTreeSprites->FXBegin(&m_RP.m_nNumRendPasses, bShadows ? FEF_DONTSETSTATES : FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	m_cEF.s_ShaderTreeSprites->FXBeginPass(0);

	m_RP.m_CurVFormat = nf;
	HRESULT hr = FX_SetVertexDeclaration(m_RP.m_FlagsStreams_Decl & 7, m_RP.m_CurVFormat);
	if (hr != S_OK)
		return;

	// set atlas size of custom texture filter
	static CCryNameR TexAtlasSizeName("TexAtlasSize");
	const Vec4 cATVal((float)CV_r_texatlassize, (float)CV_r_texatlassize, 1.f / (float)CV_r_texatlassize, 1.f / (float)CV_r_texatlassize);
	m_cEF.s_ShaderTreeSprites->FXSetPSFloat(TexAtlasSizeName, &cATVal, 1);
	m_cEF.s_ShaderTreeSprites->FXSetVSFloat(TexAtlasSizeName, &cATVal, 1);

	void* pCurVB = NULL;

	const float fFracMem = ((float)(sizeof(short)) * 6.0f) / ((float)(sizeof(SVF_P3F_C4B_T2F)) * 4.0f);
	const int nMemV = (int)(128.0f * 1024.0f * (1 - fFracMem));
	const int nMemI = (int)(128.0f * 1024.0f * fFracMem);
	assert(nMemI + nMemV <= 128 * 1024);
	const int nMaxVerts = nMemV / sizeof(SVF_P3F_C4B_T2F);
	const int nMaxSprites = nMaxVerts / 4;
	const int nMaxInds = nMemI / sizeof(short);
	if (!m_pSpriteVerts)
	{
		byte* pMem = (byte*)CryModuleMemalign(128 * 1028, 128);
		m_pSpriteVerts = (SVF_P3F_C4B_T2F*)pMem;
		m_pSpriteInds = (uint16*)(pMem + nMemV + 256);
	}

#ifdef SPRITES_2_TEXTURES
	SamplerStateHandle nTexState = EDefaultSamplerStates::LinearClamp;
#else
	SamplerStateHandle nTexState = EDefaultSamplerStates::LinearClamp; // SAMPLERSTATE_POINT
#endif
	SamplerStateHandle nNoiseTexState = EDefaultSamplerStates::LinearWrap;

	uint32 nPrevLMask = (uint32) - 1;
	int nPasses = 0;
	bool bMultiGPU = GetActiveGPUCount() > 1;

	SVegetationSpriteInfo* const pStart = &pList->GetAt(0);

	int nO = 0;  // Sprite ID
	int nLV = 256;
	int nLI = 128;
	for (i = 0; (int)i < nSPI; i++)
	{
		const SSpriteInfo* __restrict pSP = &pSPInfo[i];

		SDynTexture2* pDT = (SDynTexture2*)pSP->m_pTex;
		if (!pDT)
			continue;
		CTexture* pTex = pDT->m_pTexture;
#ifdef _DEBUG
		// Make sure we updated the texture on this frame
		if (bMultiGPU)
		{
			assert(pDT->IsValid());
		}
#endif
		if (!pTex)
			continue;

		SVegetationSpriteInfo& vi = pStart[pSP->m_nVI];

		if (pPrevTex != pTex || pPrevTerrainTexInfo != pSP->m_pTerrainTexInfo)
		{
#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
			m_SpriteAtlasesUsed.insert(pTex);
			m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpriteAltasesUsed = m_SpriteAtlasesUsed.size();
#endif

			if (nO)
			{
				ObjSpritesFlush(m_pSpriteVerts, m_pSpriteInds, nO, pCurVB, m_RP.m_pCurTechnique, bZ);
				nO = 0;
				nLI = 128;
				nLV = 256;
			}
			if (!bShadows)
			{
#if !defined(RELEASE)
				if (CV_r_VegetationSpritesGenDebug == 2)
				{
					CTexture::s_ptexColorMagenta->Apply(0, nTexState);
				}
				else
#endif
				{
					gTexture2 = pTex;
					pTex->Apply(0, nTexState);
				}

				if (bZ)
					CTexture::s_ptexDissolveNoiseMap->Apply(2, nNoiseTexState);

				if (m_RP.m_nPassGroupID == EFSLIST_GENERAL && pSP->m_pTerrainTexInfo)
				{
					SSectorTextureSet* pTexInfo = pSP->m_pTerrainTexInfo;

					// set new RT flags
					m_cEF.s_ShaderTreeSprites->FXEndPass();
					m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];

					m_cEF.s_ShaderTreeSprites->FXBeginPass(0);

					CTexture* pTerrTex = CTexture::GetByID(pTexInfo->nTex0);

					SSamplerState pTerrainTexState = SSamplerState(FILTER_LINEAR, true);
					pTerrainTexState.m_bSRGBLookup = true;
					SamplerStateHandle nTerrainTexState = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(pTerrainTexState);
					pTerrTex->Apply(3, nTerrainTexState);

					static CCryNameR SpritesOutdoorAOVertInfoName("SpritesOutdoorAOVertInfo");
					const Vec4 cSPVal(pTexInfo->fTexOffsetX, pTexInfo->fTexOffsetY, pTexInfo->fTexScale, 0);
					m_cEF.s_ShaderTreeSprites->FXSetVSFloat(SpritesOutdoorAOVertInfoName, &cSPVal, 1);
				}
				else
					m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];

				{
					// set RT_AMBIENT if no light sources present, used by blend with terrain color feature
					bool bSunExist = false;
					if (m_RP.RenderView()->GetDynamicLightsCount() > 0 &&
					    m_RP.RenderView()->GetDynamicLight(0).m_Flags & DLF_SUN)
						bSunExist = true;
				}
			}
			pPrevTex = pTex;
			pPrevTerrainTexInfo = pSP->m_pTerrainTexInfo;

			m_cEF.s_ShaderTreeSprites->FXSetPSFloat(TexAtlasSizeName, &cATVal, 1);
			m_cEF.s_ShaderTreeSprites->FXSetVSFloat(TexAtlasSizeName, &cATVal, 1);
		}

		int nOffs = nO * 4;
		if (nOffs + 4 >= nMaxVerts)
		{
			ObjSpritesFlush(m_pSpriteVerts, m_pSpriteInds, nO, pCurVB, m_RP.m_pCurTechnique, bZ);
			nOffs = 0;
			nO = 0;
			nLI = 128;
			nLV = 256;
		}

		float fX0 = pSP->m_vPos.x + pSP->m_fDX;
		float fX1 = pSP->m_vPos.x - pSP->m_fDX;
		float fY0 = pSP->m_vPos.y + pSP->m_fDY;
		float fY1 = pSP->m_vPos.y - pSP->m_fDY;

		float vUpz = pSP->m_fScaleV;
		float pSPm_vPoszvUpz = pSP->m_vPos.z - vUpz;
		float pSPm_vPoszvUpzm = pSP->m_vPos.z + vUpz;

		// update alpha test ref
		UCol uCol = pSP->m_Color;
		uCol.bcolor[bcolor0] = vi.ucAlphaTestRef;
		uCol.bcolor[bcolor2] = vi.ucDissolveOut;

		uint32 dCol = uCol.dcolor;

		float fST00 = sfTableTCFloat[pSP->m_ucTexCoordMaxX] - 2.0f * cATVal.z;  // 1 pixel back to stay inside this sprite, 1 for the bilinear filtering in the shader
		float fST01 = sfTableTCFloat[pSP->m_ucTexCoordMaxY] - 2.0f * cATVal.z;  // 1 pixel back to stay inside this sprite, 1 for the bilinear filtering in the shader
		float fST10 = sfTableTCFloat[pSP->m_ucTexCoordMinX];
		float fST11 = sfTableTCFloat[pSP->m_ucTexCoordMinY];

#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)

		// hash up this cell
		uint32 cellHash = 0;
		cellHash |= ((uint32)(TRUNCATE_PTR)pSP->m_pTex) << 16; // store texture low-end 16 bits, enough to give us uniquely which texture we're using.
		cellHash |= ((uint32)pSP->m_ucTexCoordMinX) << 8;      // store min texture co-ords
		cellHash |= ((uint32)pSP->m_ucTexCoordMinY);

		m_SpriteCellsUsed.insert(cellHash);
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpriteCellsUsed = m_SpriteCellsUsed.size();
#endif

		SVF_P3F_C4B_T2F* __restrict pQuad = &m_pSpriteVerts[nOffs];

		pQuad[0].xyz(fX0, fY1, pSPm_vPoszvUpz);
		pQuad[0].color.dcolor = dCol;
		pQuad[0].st.x = fST00;
		pQuad[0].st.y = fST01;

		pQuad[1].xyz(fX1, fY0, pSPm_vPoszvUpz);
		pQuad[1].color.dcolor = dCol;
		pQuad[1].st.x = fST10;
		pQuad[1].st.y = fST01;

		// 2,3 verts
		pQuad[2].xyz(fX0, fY1, pSPm_vPoszvUpzm);
		pQuad[2].color.dcolor = dCol;
		pQuad[2].st.x = fST00;
		pQuad[2].st.y = fST11;

		pQuad[3].xyz(fX1, fY0, pSPm_vPoszvUpzm);
		pQuad[3].color.dcolor = dCol;
		pQuad[3].st.x = fST10;
		pQuad[3].st.y = fST11;

		int nIOffs = nO * 6;
		uint16* __restrict pInds = &m_pSpriteInds[nIOffs];
		pInds[0] = nOffs;
		pInds[1] = nOffs + 1;
		pInds[3] = nOffs + 1;
		pInds[2] = nOffs + 2;
		pInds[4] = nOffs + 2;
		pInds[5] = nOffs + 3;

		nO++;
	}

	if (nO)
		ObjSpritesFlush(m_pSpriteVerts, m_pSpriteInds, nO, pCurVB, m_RP.m_pCurTechnique, bZ);
	else
		FX_Commit();

	//XUnlockL2( XLOCKL2_INDEX_TITLE );

	m_RP.m_FlagsShader_RT = nPrevFlagsShader_RT;
	m_RP.m_pShaderResources = pResSave;
}

void CD3D9Renderer::DrawObjSprites(PodArray<SVegetationSpriteInfo>* pList)
{
	assert(pList);
	if (!pList)
		return;

	if (IsRecursiveRenderView())
		return;

	gRenDev->m_cEF.mfRefreshSystemShader("FarTreeSprites", CShaderMan::s_ShaderTreeSprites);

	sInitTCTable();

	PROFILE_LABEL_SCOPE("SPRITES");
	if (m_RP.m_nBatchFilter & FB_Z)
	{
		if (CV_r_usezpass && !(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_IMPOSTERGEN))
		{
			static CCryNameTSCRC TechName("General_Z");
			m_cEF.s_ShaderTreeSprites->FXSetTechnique(TechName);
			{
				PROFILE_FRAME(Draw_ObjSprites_Z);
				DrawObjSprites(pList, sSPInfo, snSPInfo, true, false);
			}
		}
	}
	else
	{
		//DrawSpritesShadows();
		static CCryNameTSCRC TechName("General");
		m_cEF.s_ShaderTreeSprites->FXSetTechnique(TechName);
		{
			PROFILE_FRAME(Draw_ObjSprites);
			DrawObjSprites(pList, sSPInfo, snSPInfo, false, false);
		}
	}

}
