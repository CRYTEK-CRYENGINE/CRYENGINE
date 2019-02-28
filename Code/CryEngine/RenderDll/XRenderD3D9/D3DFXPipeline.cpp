// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DFXPipeline.cpp : Direct3D specific FX shaders rendering pipeline.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IRenderNode.h>
#include "../Common/ReverseDepth.h"
#include "../Common/ComputeSkinningStorage.h"
#include "DeviceManager/DeviceObjects.h" // SInputLayout

#if defined(FEATURE_SVO_GI)
#include "D3D_SVO.h"
#endif

//====================================================================================

static int sTexLimitRes(uint32 nSrcsize, uint32 nDstSize)
{
	while (true)
	{
		if (nSrcsize > nDstSize)
			nSrcsize >>= 1;
		else
			break;
	}
	return nSrcsize;
}

static Matrix34 sMatrixLookAt(const Vec3& dir, const Vec3& up, float rollAngle = 0)
{
	Matrix34 M;
	// LookAt transform.
	Vec3 xAxis, yAxis, zAxis;
	Vec3 upVector = up;

	yAxis = -dir.GetNormalized();

	//if (zAxis.x == 0.0 && zAxis.z == 0) up.Set( -zAxis.y,0,0 ); else up.Set( 0,1.0f,0 );

	xAxis = upVector.Cross(yAxis).GetNormalized();
	zAxis = xAxis.Cross(yAxis).GetNormalized();

	// OpenGL kind of matrix.
	M(0, 0) = xAxis.x;
	M(0, 1) = yAxis.x;
	M(0, 2) = zAxis.x;
	M(0, 3) = 0;

	M(1, 0) = xAxis.y;
	M(1, 1) = yAxis.y;
	M(1, 2) = zAxis.y;
	M(1, 3) = 0;

	M(2, 0) = xAxis.z;
	M(2, 1) = yAxis.z;
	M(2, 2) = zAxis.z;
	M(2, 3) = 0;

	if (rollAngle != 0)
	{
		Matrix34 RollMtx;
		RollMtx.SetIdentity();

		float cossin[2];
		//   sincos_tpl(rollAngle, cossin);
		sincos_tpl(rollAngle, &cossin[1], &cossin[0]);

		RollMtx(0, 0) = cossin[0];
		RollMtx(0, 2) = -cossin[1];
		RollMtx(2, 0) = cossin[1];
		RollMtx(2, 2) = cossin[0];

		// Matrix multiply.
		M = RollMtx * M;
	}

	return M;
}

bool CD3D9Renderer::FX_DrawToRenderTarget(
	CShader* pShader,
	CShaderResources* pRes,
	CRenderObject* pObj,
	SShaderTechnique* pTech, 
	SHRenderTarget* pRT,
	int nPreprType,
	CRenderElement* pRE,
	const SRenderingPassInfo& passInfo)
{
	if (!pRT)
		return false;

	int nThreadList = m_pRT->GetThreadList();

	uint32 nPrFlags = pRT->m_nFlags;
	if (nPrFlags & FRT_RENDTYPE_CURSCENE)
		return false;

	CTexture* Tex           = pRT->m_pTarget;
	SEnvTexture* pEnvTex    = NULL;

	if (nPreprType == SPRID_SCANTEX)
	{
		nPrFlags     |= FRT_CAMERA_REFLECTED_PLANE;
		pRT->m_nFlags = nPrFlags;
	}

	if (nPrFlags & FRT_RENDTYPE_CURSCENE)
		return false;

	uint32 nWidth  = pRT->m_nWidth;
	uint32 nHeight = pRT->m_nHeight;

	if (pRT->m_nIDInPool >= 0)
	{
		assert((int)CRendererResources::s_CustomRT_2D.Num() > pRT->m_nIDInPool);
		if ((int)CRendererResources::s_CustomRT_2D.Num() <= pRT->m_nIDInPool)
			return false;
		pEnvTex = &CRendererResources::s_CustomRT_2D[pRT->m_nIDInPool];

		// Very hi specs render reflections at half res - lower specs (and consoles) at quarter res
		float fSizeScale = (CV_r_waterreflections_quality == 5) ? 0.5f : 0.25f;

		if (nWidth == -1)
			nWidth = uint32(sTexLimitRes(CRendererResources::s_renderWidth, uint32(CRendererResources::s_resourceWidth)) * fSizeScale);
		if (nHeight == -1)
			nHeight = uint32(sTexLimitRes(CRendererResources::s_renderHeight, uint32(CRendererResources::s_resourceHeight)) * fSizeScale);

		ETEX_Format eTF = pRT->m_eTF;
		if (!pEnvTex->m_pTex || pEnvTex->m_pTex->GetFormat() != eTF)
		{
			char name[128];
			cry_sprintf(name, "$RT_ENV_2D_%d", m_TexGenID++);
			int flags = FT_NOMIPS | FT_STATE_CLAMP | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
			pEnvTex->m_pTex = new SDynTexture(nWidth, nHeight, Clr_Transparent, eTF, eTT_2D, flags, name);
			pEnvTex->m_pTex->Update(nWidth, nHeight);
			pRT->m_nWidth = nWidth;
			pRT->m_nHeight = nHeight;
			pRT->m_pTarget = pEnvTex->m_pTex->m_pTexture;
		}

		CRY_ASSERT(nWidth  > 0 && nWidth  <= CRendererResources::s_resourceWidth);
		CRY_ASSERT(nHeight > 0 && nHeight <= CRendererResources::s_resourceHeight);

		Tex = pEnvTex->m_pTex->m_pTexture;
	}
	else if (Tex)
	{
		if (Tex->GetCustomID() == TO_RT_2D)
		{
			bool bReflect = false;
			if (nPrFlags & (FRT_CAMERA_REFLECTED_PLANE | FRT_CAMERA_REFLECTED_WATERPLANE))
				bReflect = true;
			Matrix33 orientation = Matrix33(passInfo.GetCamera().GetMatrix());
			Ang3 Angs            = CCamera::CreateAnglesYPR(orientation);
			Vec3 Pos             = passInfo.GetCamera().GetPosition();
			bool bNeedUpdate     = false;
			pEnvTex = CRendererResources::FindSuitableEnvTex(Pos, Angs, false, -1, false, pShader, pRes, pObj, bReflect, pRE, &bNeedUpdate, &passInfo);

			if (!bNeedUpdate)
			{
				if (!pEnvTex)
					return false;
				if (pEnvTex->m_pTex && pEnvTex->m_pTex->m_pTexture)
					return true;
			}

			switch (CRenderer::CV_r_envtexresolution)
			{
			case 0:
				nWidth = 64;
				break;
			case 1:
				nWidth = 128;
				break;
			case 2:
			default:
				nWidth = 256;
				break;
			case 3:
				nWidth = 512;
				break;
			}
			nHeight = nWidth;
			if (!pEnvTex || !pEnvTex->m_pTex)
				return false;
			if (!pEnvTex->m_pTex->m_pTexture)
			{
				pEnvTex->m_pTex->Update(nWidth, nHeight);
			}
			Tex = pEnvTex->m_pTex->m_pTexture;
		}
	}
	if (m_pRT->IsRenderThread() && Tex && Tex->IsLocked())
		return true;

	bool bMGPUAllowNextUpdate = (!(gRenDev->RT_GetCurrGpuID())) && (CRenderer::CV_r_waterreflections_mgpu);

	// always allow for non-mgpu
	if (gRenDev->GetActiveGPUCount() == 1 || !CRenderer::CV_r_waterreflections_mgpu)
		bMGPUAllowNextUpdate = true;

	ETEX_Format eTF = pRT->m_eTF;
	if (pEnvTex && (!pEnvTex->m_pTex || pEnvTex->m_pTex->GetFormat() != eTF))
	{
		SAFE_DELETE(pEnvTex->m_pTex);
		char name[128];
		cry_sprintf(name, "$RT_2D_%d", m_TexGenID++);
		int flags = FT_NOMIPS | FT_STATE_CLAMP | FT_DONT_STREAM;
		pEnvTex->m_pTex = new SDynTexture(nWidth, nHeight, Clr_Transparent, eTF, eTT_2D, flags, name);
		assert(nWidth > 0 && nWidth <= m_d3dsdBackBuffer.Width);
		assert(nHeight > 0 && nHeight <= m_d3dsdBackBuffer.Height);
		pEnvTex->m_pTex->Update(nWidth, nHeight);
	}

	bool bEnableAnisotropicBlur = true;
	switch (pRT->m_eUpdateType)
	{
	case eRTUpdate_WaterReflect:
	{
		bool bSkip = false;
		if (m_waterUpdateInfo.m_nLastWaterFrameID < 100)
		{
			m_waterUpdateInfo.m_nLastWaterFrameID++;
			bSkip = true;
		}
		if (bSkip || !CRenderer::CV_r_waterreflections)
		{
			assert(pEnvTex != NULL);
			if (pEnvTex && pEnvTex->m_pTex && pEnvTex->m_pTex->m_pTexture)
				pEnvTex->m_pTex->m_pTexture->Clear(Clr_Empty);

			return true;
		}

		if (m_waterUpdateInfo.m_nLastWaterFrameID == gRenDev->GetMainFrameID())
			// water reflection already created this frame, share it
			return true;

		I3DEngine* eng               = (I3DEngine*)gEnv->p3DEngine;
		int nVisibleWaterPixelsCount = eng->GetOceanVisiblePixelsCount() / 2;            // bug in occlusion query returns 2x more
		int nPixRatioThreshold       = (int)(CRendererResources::s_renderArea * CRenderer::CV_r_waterreflections_min_visible_pixels_update);

		static int nVisWaterPixCountPrev = nVisibleWaterPixelsCount;
		if (CRenderer::CV_r_waterreflections_mgpu)
		{
			nVisWaterPixCountPrev = bMGPUAllowNextUpdate ? nVisibleWaterPixelsCount : nVisWaterPixCountPrev;
		}
		else
			nVisWaterPixCountPrev = nVisibleWaterPixelsCount;

		float fUpdateFactorMul   = 1.0f;
		float fUpdateDistanceMul = 1.0f;
		if (nVisWaterPixCountPrev < nPixRatioThreshold / 4)
		{
			bEnableAnisotropicBlur = false;
			fUpdateFactorMul       = CV_r_waterreflections_minvis_updatefactormul * 10.0f;
			fUpdateDistanceMul     = CV_r_waterreflections_minvis_updatedistancemul * 5.0f;
		}
		else if (nVisWaterPixCountPrev < nPixRatioThreshold)
		{
			fUpdateFactorMul   = CV_r_waterreflections_minvis_updatefactormul;
			fUpdateDistanceMul = CV_r_waterreflections_minvis_updatedistancemul;
		}

		float fMGPUScale           = CRenderer::CV_r_waterreflections_mgpu ? (1.0f / (float) gRenDev->GetActiveGPUCount()) : 1.0f;
		float fWaterUpdateFactor   = CV_r_waterupdateFactor * fUpdateFactorMul * fMGPUScale;
		float fWaterUpdateDistance = CV_r_waterupdateDistance * fUpdateDistanceMul * fMGPUScale;

		float fTimeUpd = min(0.3f, eng->GetDistanceToSectorWithWater());
		fTimeUpd *= fWaterUpdateFactor;
		//if (fTimeUpd > 1.0f)
		//fTimeUpd = 1.0f;
		
		Vec3 camView = -passInfo.GetCamera().GetRenderVectorZ();
		Vec3 camUp   = passInfo.GetCamera().GetRenderVectorY();

		m_waterUpdateInfo.m_nLastWaterFrameID = gRenDev->GetMainFrameID();

		CTimeValue animTime = GetAnimationTime();

		Vec3  camPos   = passInfo.GetCamera().GetPosition();
		float fDistCam = (camPos - m_waterUpdateInfo.m_LastWaterPosUpdate).GetLength();
		float fDotView = camView * m_waterUpdateInfo.m_LastWaterViewdirUpdate;
		float fDotUp   = camUp * m_waterUpdateInfo.m_LastWaterUpdirUpdate;
		float fFOV     = passInfo.GetCamera().GetFov();
		if (m_waterUpdateInfo.m_fLastWaterUpdate - 1.0f > animTime.GetSeconds())
			m_waterUpdateInfo.m_fLastWaterUpdate = animTime.GetSeconds();

		const float fMaxFovDiff = 0.1f;       // no exact test to prevent slowly changing fov causing per frame water reflection updates

		static bool bUpdateReflection = true;
		if (bMGPUAllowNextUpdate)
		{
			bUpdateReflection = animTime.GetSeconds() - m_waterUpdateInfo.m_fLastWaterUpdate >= fTimeUpd || fDistCam > fWaterUpdateDistance;
			bUpdateReflection = bUpdateReflection || fDotView<0.9f || fabs(fFOV - m_waterUpdateInfo.m_fLastWaterFOVUpdate)>fMaxFovDiff;
		}

		if (bUpdateReflection && bMGPUAllowNextUpdate)
		{
			m_waterUpdateInfo.m_fLastWaterUpdate       = animTime.GetSeconds();
			m_waterUpdateInfo.m_LastWaterViewdirUpdate = camView;
			m_waterUpdateInfo.m_LastWaterUpdirUpdate   = camUp;
			m_waterUpdateInfo.m_fLastWaterFOVUpdate    = fFOV;
			m_waterUpdateInfo.m_LastWaterPosUpdate     = camPos;
			assert(pEnvTex != NULL);
			if (pEnvTex)
			{
				pEnvTex->m_pTex->ResetUpdateMask();
			}
		}
		else if (!bUpdateReflection)
		{
			assert(pEnvTex != NULL);
			PREFAST_ASSUME(pEnvTex != NULL);
			if (pEnvTex && pEnvTex->m_pTex && pEnvTex->m_pTex->IsValid())
				return true;
		}

		assert(pEnvTex != NULL);
		PREFAST_ASSUME(pEnvTex != NULL);
		pEnvTex->m_pTex->SetUpdateMask();
	}
	break;
	}

	// Just copy current BB to the render target and exit
	if (nPrFlags & FRT_RENDTYPE_COPYSCENE)
	{
		CRY_ASSERT(m_pRT->IsRenderThread());

		// Get current render target from the RT stack
		if (!CRenderer::CV_r_debugrefraction)
		{
			//FX_ScreenStretchRect(Tex); // should encode hdr format
		}
		else
			CClearSurfacePass::Execute(Tex, Clr_Debug);

		return true;
	}

	I3DEngine* eng = (I3DEngine*)gEnv->p3DEngine;
	Matrix44A  matProj, matView;

	float plane[4];
	bool  bUseClipPlane  = false;
	bool  bChangedCamera = false;

	static CCamera tmp_cam_mgpu = passInfo.GetCamera();
	CCamera tmp_cam             = passInfo.GetCamera();
	CCamera prevCamera          = tmp_cam;
	bool bMirror                = false;
	bool bOceanRefl             = false;

	SRenderViewInfo::EFlags recusriveViewFlags = SRenderViewInfo::eFlags_None;
	passInfo.GetRenderView()->CalculateViewInfo();
	const auto& viewInfo = passInfo.GetRenderView()->GetViewInfo(CCamera::eEye_Left);

	// Set the camera
	if (nPrFlags & FRT_CAMERA_REFLECTED_WATERPLANE)
	{
		bOceanRefl = true;


		float fMinDist = min(SKY_BOX_SIZE * 0.5f, eng->GetDistanceToSectorWithWater());      // 16 is half of skybox size
		float fMaxDist = eng->GetMaxViewDistance();

		Vec3 vPrevPos = tmp_cam.GetPosition();
		Vec4 pOceanParams0, pOceanParams1;
		eng->GetOceanAnimationParams(pOceanParams0, pOceanParams1);

		Plane Pl;
		Pl.n = Vec3(0, 0, 1);
		Pl.d = eng->GetWaterLevel();                                                      // + CRenderer::CV_r_waterreflections_offset;// - pOceanParams1.x;
		if ((vPrevPos | Pl.n) - Pl.d < 0)
		{
			Pl.d = -Pl.d;
			Pl.n = -Pl.n;
		}

		plane[0] = Pl.n[0];
		plane[1] = Pl.n[1];
		plane[2] = Pl.n[2];
		plane[3] = -Pl.d;

		Matrix44 camMat = viewInfo.viewMatrix;
		Vec3  vPrevDir  = Vec3(camMat(0, 2), camMat(1, 2), camMat(2, 2));
		Vec3  vPrevUp   = Vec3(camMat(0, 1), camMat(1, 1), camMat(2, 1));
		Vec3  vNewDir   = -Pl.MirrorVector(vPrevDir);
		Vec3  vNewUp    = -Pl.MirrorVector(vPrevUp);
		float fDot      = vPrevPos.Dot(Pl.n) - Pl.d;
		Vec3  vNewPos   = vPrevPos - Pl.n * 2.0f * fDot;
		Matrix34 m      = sMatrixLookAt(vNewDir, vNewUp, tmp_cam.GetAngles()[2]);

		// New position + offset along view direction - minimizes projection artefacts
		m.SetTranslation(vNewPos + Vec3(vNewDir.x, vNewDir.y, 0));

		tmp_cam.SetMatrix(m);

		float fDistOffset = fMinDist;
		if (CV_r_waterreflections_use_min_offset)
		{
			fDistOffset = max(fMinDist, 2.0f * gEnv->p3DEngine->GetDistanceToSectorWithWater());
			if (fDistOffset >= fMaxDist)                                                                                                                             // engine returning bad value
				fDistOffset = fMinDist;
		}

		assert(pEnvTex);
		PREFAST_ASSUME(pEnvTex);
		tmp_cam.SetFrustum((int)(pEnvTex->m_pTex->GetWidth() * tmp_cam.GetProjRatio()), pEnvTex->m_pTex->GetHeight(), tmp_cam.GetFov(), fDistOffset, fMaxDist);

		// Allow camera update
		if (bMGPUAllowNextUpdate)
			tmp_cam_mgpu = tmp_cam;

		bChangedCamera = true;
		bUseClipPlane  = true;
		bMirror        = true;
		//gRenDev->m_renderThreadInfo[nThreadList].m_PersFlags |= RBPF_MIRRORCULL;
	}
	else if (nPrFlags & FRT_CAMERA_REFLECTED_PLANE)
	{

		float fMinDist = 0.25f;
		float fMaxDist = eng->GetMaxViewDistance();

		Vec3 vPrevPos = tmp_cam.GetPosition();

		if (pRes && pRes->m_pCamera)
		{
			tmp_cam = *pRes->m_pCamera;                                                                                                                             // Portal case
			//tmp_cam.SetPosition(Vec3(310, 150, 30));
			//tmp_cam.SetAngles(Vec3(-90,0,0));
			//tmp_cam.SetFrustum((int)(Tex->GetWidth()*tmp_cam.GetProjRatio()), Tex->GetHeight(), tmp_cam.GetFov(), fMinDist, tmp_cam.GetFarPlane());

			bUseClipPlane = false;
			bMirror       = false;
		}
		else
		{
			// Mirror case
			Plane Pl;
			pRE->mfGetPlane(Pl);
			//Pl.d = -Pl.d;
			if (pObj)
			{
				Matrix44 mat = pObj->GetMatrix(passInfo).GetTransposed();
				Pl = TransformPlane(mat, Pl);
			}
			if ((vPrevPos | Pl.n) - Pl.d < 0)
			{
				Pl.d = -Pl.d;
				Pl.n = -Pl.n;
			}

			plane[0] = Pl.n[0];
			plane[1] = Pl.n[1];
			plane[2] = Pl.n[2];
			plane[3] = -Pl.d;

			//this is the new code to calculate the reflection matrix

			Matrix44 camMat = viewInfo.viewMatrix;

			Vec3  vPrevDir = Vec3(-camMat(0, 2), -camMat(1, 2), -camMat(2, 2));
			Vec3  vPrevUp  = Vec3(camMat(0, 1), camMat(1, 1), camMat(2, 1));
			Vec3  vNewDir  = Pl.MirrorVector(vPrevDir);
			Vec3  vNewUp   = Pl.MirrorVector(vPrevUp);
			float fDot     = vPrevPos.Dot(Pl.n) - Pl.d;
			Vec3  vNewPos  = vPrevPos - Pl.n * 2.0f * fDot;
			Matrix34A m    = sMatrixLookAt(vNewDir, vNewUp, tmp_cam.GetAngles()[2]);
			m.SetTranslation(vNewPos);
			tmp_cam.SetMatrix(m);

			//Matrix34 RefMatrix34 = CreateReflectionMat3(Pl);
			//Matrix34 matMir=RefMatrix34*tmp_cam.GetMatrix();
			//tmp_cam.SetMatrix(matMir);
			assert(Tex);
			tmp_cam.SetFrustum((int)(Tex->GetWidth() * tmp_cam.GetProjRatio()), Tex->GetHeight(), tmp_cam.GetFov(), fMinDist, fMaxDist);       //tmp_cam.GetFarPlane());
			bMirror       = true;
			bUseClipPlane = true;
		}

		bChangedCamera = true;
		//gRenDev->m_renderThreadInfo[nThreadList].m_PersFlags |= RBPF_MIRRORCULL;
	}
	else if (((nPrFlags & FRT_CAMERA_CURRENT) || (nPrFlags & FRT_RENDTYPE_CURSCENE)) && pRT->m_eOrder == eRO_PreDraw)
	{
		CRY_ASSERT(m_pRT->IsRenderThread());

		// Always restore stuff after explicitly changing...

		// get texture surface
		// Get current render target from the RT stack
		if (!CRenderer::CV_r_debugrefraction)
		{
			//FX_ScreenStretchRect(Tex);                                                                                                   // should encode hdr format
		}
		else
			CClearSurfacePass::Execute(Tex, Clr_Debug);

		return true;
	}
	/*	if (pRT->m_nFlags & FRT_CAMERA_CURRENT)
	   {
	   //m_RP.m_pIgnoreObject = pObj;

	   SetCamera(tmp_cam);
	   bChangedCamera = true;
	   bUseClipPlane = true;
	   }*/

	bool bRes = true;

	//gRenDev->m_renderThreadInfo[nThreadList].m_PersFlags |= RBPF_DRAWTOTEXTURE | RBPF_ENCODE_HDR;

	if (m_LogFile)
		Logv("*** Set RT for Water reflections ***\n");

	// TODO: ClearTexture()
	assert(pEnvTex);
	PREFAST_ASSUME(pEnvTex);
	
	CRenderOutputPtr pRenderOutput = nullptr;
	if (m_nGraphicsPipeline >= 3)
	{
		pRenderOutput = std::make_shared<CRenderOutput>(pEnvTex->m_pTex, pEnvTex->m_pTex->m_nWidth, pEnvTex->m_pTex->m_nHeight, pRT->m_bTempDepth, (pRT->m_nFlags & FRT_CLEAR), pRT->m_ClearColor, pRT->m_fClearDepth);
	}

	float fAnisoScale = 1.0f;
	{
		//gRenDev->m_renderThreadInfo[nThreadList].m_PersFlags |= RBPF_OBLIQUE_FRUSTUM_CLIPPING | RBPF_MIRRORCAMERA;     // | RBPF_MIRRORCULL; ??

		Plane obliqueClipPlane;
		obliqueClipPlane.n[0]      = plane[0];
		obliqueClipPlane.n[1]      = plane[1];
		obliqueClipPlane.n[2]      = plane[2];
		obliqueClipPlane.d         = plane[3];                                                                  // +0.25f;
		fAnisoScale = plane[3];
		fAnisoScale = fabs(fabs(fAnisoScale) - passInfo.GetCamera().GetPosition().z);

		// put clipplane in clipspace..
		Matrix44A mView, mProj, mCamProj, mInvCamProj;
		mView = viewInfo.viewMatrix;
		mProj = viewInfo.projMatrix;
		mCamProj    = mView * mProj;
		mInvCamProj = mCamProj.GetInverted();

		tmp_cam.SetObliqueClipPlane(TransformPlane2(mInvCamProj, obliqueClipPlane));
		tmp_cam.SetObliqueClipPlaneEnabled(true);

		tmp_cam_mgpu.SetObliqueClipPlane(TransformPlane2(mInvCamProj, obliqueClipPlane));
		tmp_cam_mgpu.SetObliqueClipPlaneEnabled(true);

		int nRenderPassFlags = (gRenDev->EF_GetRenderQuality()) ? SRenderingPassInfo::TERRAIN : 0;

		int nReflQuality = (bOceanRefl) ? (int)CV_r_waterreflections_quality : (int)CV_r_reflections_quality;

		// set reflection quality setting
		switch (nReflQuality)
		{
		case 1:
			nRenderPassFlags |= SRenderingPassInfo::ENTITIES;
			break;
		case 2:
			nRenderPassFlags |= SRenderingPassInfo::TERRAIN_DETAIL_MATERIALS | SRenderingPassInfo::ENTITIES;
			break;
		case 3:
			nRenderPassFlags |= SRenderingPassInfo::STATIC_OBJECTS | SRenderingPassInfo::ENTITIES | SRenderingPassInfo::TERRAIN_DETAIL_MATERIALS;
			break;
		case 4:
		case 5:
			nRenderPassFlags |= SRenderingPassInfo::STATIC_OBJECTS | SRenderingPassInfo::ENTITIES | SRenderingPassInfo::TERRAIN_DETAIL_MATERIALS | SRenderingPassInfo::PARTICLES;
			break;
		}

		int nRFlags = SHDF_ALLOWHDR | SHDF_NO_DRAWNEAR | SHDF_FORWARD_MINIMAL;

		// disable caustics if camera above water
		if (obliqueClipPlane.d < 0)
			nRFlags |= SHDF_NO_DRAWCAUSTICS;

		if (bMirror)
		{
			pEnvTex->SetMatrix(tmp_cam);
		}

		auto recursivePassInfo = SRenderingPassInfo::CreateRecursivePassRenderingInfo(bOceanRefl ? tmp_cam_mgpu : tmp_cam, nRenderPassFlags);
		
		SRenderViewInfo::EFlags recursiveViewFlags = SRenderViewInfo::eFlags_None;
		recursiveViewFlags |= SRenderViewInfo::eFlags_DrawToTexure;
		if (!(nPrFlags & FRT_CAMERA_REFLECTED_WATERPLANE)) // projection matrix is already mirrored for ocean reflections
			recursiveViewFlags |= SRenderViewInfo::eFlags_MirrorCamera;

		CRenderView* pRecursiveRenderView = recursivePassInfo.GetRenderView();
		pRecursiveRenderView->SetViewFlags(recursiveViewFlags);
		pRecursiveRenderView->AssignRenderOutput(pRenderOutput);
		pRecursiveRenderView->SetSkinningDataPools(GetSkinningDataPools());

		CCamera cam = recursivePassInfo.GetCamera();
		pRecursiveRenderView->SetCameras(&cam, 1);
		pRecursiveRenderView->SetPreviousFrameCameras(&cam, 1);

		ExecuteRenderThreadCommand(
			[=]	{
				pRenderOutput->BeginRendering(pRecursiveRenderView);
			},
			ERenderCommandFlags::None
		);

		eng->RenderWorld(nRFlags, recursivePassInfo, __FUNCTION__);

		ExecuteRenderThreadCommand(
			[=]
			{
				pRenderOutput->EndRendering(pRecursiveRenderView);
			},
			ERenderCommandFlags::None
		);
	}

	// Very Hi specs get anisotropic reflections
	int nReflQuality = (bOceanRefl) ? (int)CV_r_waterreflections_quality : (int)CV_r_reflections_quality;
	
	if (nReflQuality >= 4 && bEnableAnisotropicBlur && Tex && Tex->GetDevTexture())
	{
		ExecuteRenderThreadCommand(
			[=]
			{
				GetGraphicsPipeline().ExecuteAnisotropicVerticalBlur(Tex, 1, 8 * max(1.0f - min(fAnisoScale / 100.0f, 1.0f), 0.2f), 1, false);
			},
			ERenderCommandFlags::None
		);
	}

	if (m_LogFile)
		Logv("*** End RT for Water reflections ***\n");

	// todo: encode hdr format
	
	// increase frame id to support multiple recursive draws
	//gRenDev->m_renderThreadInfo[nThreadList].m_nFrameID++;

	return bRes;
}
