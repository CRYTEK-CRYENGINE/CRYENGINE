// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================

   Revision history:
* Created by Vladimir Kajalin

   =============================================================================*/

#include "StdAfx.h"

#if defined(FEATURE_SVO_GI)

	#include "DriverD3D.h"
	#include <Cry3DEngine/I3DEngine.h>
	#include "D3DPostProcess.h"
	#include "D3D_SVO.h"
	#include "Common/RenderView.h"
	#include "GraphicsPipeline/TiledLightVolumes.h"

_smart_ptr<CTexture> CSvoRenderer::s_pRsmColorMap;
_smart_ptr<CTexture> CSvoRenderer::s_pRsmNormlMap;
_smart_ptr<CTexture> CSvoRenderer::s_pRsmPoolCol;
_smart_ptr<CTexture> CSvoRenderer::s_pRsmPoolNor;

CSvoRenderer* CSvoRenderer::s_pInstance = 0;

CSvoRenderer::CSvoRenderer()
{
	//	InitCVarValues();

	ZeroStruct(m_texInfo.arrAnalyticalOccluders);

	m_pNoiseTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/noise.dds", FT_DONT_STREAM, eTF_Unknown);

	m_pShader = CShaderMan::s_ShaderSVOGI;

	ZeroStruct(m_mGpuVoxViewProj);
	ZeroStruct(m_wsOffset);
	ZeroStruct(m_tcOffset);
	ZeroStruct(m_arrNodesForUpdate);
	ZeroStruct(m_nCurPropagationPassID);
}

CSvoRenderer* CSvoRenderer::GetInstance(bool bCheckAlloce)
{
	if (!s_pInstance && bCheckAlloce)
		s_pInstance = new CSvoRenderer();

	return s_pInstance;
}

void CSvoRenderer::Release()
{
	SAFE_DELETE(s_pInstance);

	s_pRsmColorMap.reset();
	s_pRsmNormlMap.reset();
	s_pRsmPoolCol.reset();
	s_pRsmPoolNor.reset();
}

void CSvoRenderer::SetEditingHelper(const Sphere& sp)
{
	m_texInfo.helperInfo = sp;
}

void CSvoRenderer::UpdateCompute(CRenderView* pRenderView)
{
	InitCVarValues();

	if (!e_svoEnabled)
		return;

	if (!e_svoRender)
		return;

	static int nTI_Compute_FrameId = -1;
	if (nTI_Compute_FrameId == gRenDev->GetRenderFrameID())
		return;
	nTI_Compute_FrameId = gRenDev->GetRenderFrameID();

	if (!gEnv->p3DEngine->GetSvoStaticTextures(m_texInfo, &m_arrLightsStatic, &m_arrLightsDynamic))
		return;

	m_pRenderView = pRenderView;

	m_arrNodesForUpdateIncr.Clear();
	m_arrNodesForUpdateNear.Clear();

	#ifdef FEATURE_SVO_GI_ALLOW_HQ

	if (GetIntegratioMode())
	{
		gEnv->p3DEngine->GetSvoBricksForUpdate(m_arrNodeInfo, (float)0, &m_arrVerts);

		for (int n = 0; n < m_arrNodeInfo.Count(); n++)
		{
			m_arrNodesForUpdateIncr.Add(m_arrNodeInfo[n]);
		}

		m_arrNodeInfo.Clear();

		if (e_svoTI_DynLights)
		{
			gEnv->p3DEngine->GetSvoBricksForUpdate(m_arrNodeInfo, e_svoMinNodeSize, 0);

			for (int n = 0; n < m_arrNodeInfo.Count(); n++)
			{
				m_arrNodesForUpdateNear.Add(m_arrNodeInfo[n]);
			}
		}
	}

	{
		// get UAV access
		vp_RGB0.Init(m_texInfo.pTexRgb0);
		vp_RGB1.Init(m_texInfo.pTexRgb1);
		vp_DYNL.Init(m_texInfo.pTexDynl);
		vp_RGB2.Init(m_texInfo.pTexRgb2);
		vp_RGB3.Init(m_texInfo.pTexRgb3);
		vp_RGB4.Init(m_texInfo.pTexRgb4);
		vp_NORM.Init(m_texInfo.pTexNorm);
		vp_ALDI.Init(m_texInfo.pTexAldi);
		vp_OPAC.Init(m_texInfo.pTexOpac);
	}

	if ((!(e_svoTI_Active && e_svoTI_Apply) && !e_svoDVR) || !m_texInfo.bSvoReady || !GetIntegratioMode())
		return;

	// force sync shaders compiling
	int nPrevAsync = CRenderer::CV_r_shadersasynccompiling;
	CRenderer::CV_r_shadersasynccompiling = 0;

	// clear pass
	{
		PROFILE_LABEL_SCOPE("TI_INJECT_CLEAR");

		for (int nNodesForUpdateStartIndex = 0; nNodesForUpdateStartIndex < m_arrNodesForUpdateIncr.Count(); )
			ExecuteComputeShader("ComputeClearBricks", m_passClearBricks, &nNodesForUpdateStartIndex, 0, m_arrNodesForUpdateIncr);
	}

	// voxelize dynamic meshes
	{
		//		PROFILE_LABEL_SCOPE( "TI_VOXELIZE" );

		//gcpRendD3D->SVO_VoxelizeMeshes(0, 0);
	}

	if (e_svoTI_Troposphere_Active && (e_svoTI_Troposphere_CloudGen_Freq || e_svoTI_Troposphere_Layer0_Dens || e_svoTI_Troposphere_Layer1_Dens))
	{
		PROFILE_LABEL_SCOPE("TI_INJECT_AIR");

		for (int nNodesForUpdateStartIndex = 0; nNodesForUpdateStartIndex < m_arrNodesForUpdateIncr.Count(); )
			ExecuteComputeShader("ComputeInjectAtmosphere", m_passInjectAirOpacity, &nNodesForUpdateStartIndex, 0, m_arrNodesForUpdateIncr);
	}

	{
		PROFILE_LABEL_SCOPE("TI_INJECT_LIGHT");

		for (int nNodesForUpdateStartIndex = 0; nNodesForUpdateStartIndex < m_arrNodesForUpdateIncr.Count(); )
			ExecuteComputeShader("ComputeDirectStaticLighting", m_passInjectStaticLights, &nNodesForUpdateStartIndex, 0, m_arrNodesForUpdateIncr);
	}

	if (e_svoTI_PropagationBooster || e_svoTI_InjectionMultiplier)
	{
		if (e_svoTI_NumberOfBounces > 1)
		{
			PROFILE_LABEL_SCOPE("TI_INJECT_REFL0");

			m_nCurPropagationPassID = 0;
			for (int nNodesForUpdateStartIndex = 0; nNodesForUpdateStartIndex < m_arrNodesForUpdateIncr.Count(); )
				ExecuteComputeShader("ComputePropagateLighting", m_passPropagateLighting_1to2, &nNodesForUpdateStartIndex, 0, m_arrNodesForUpdateIncr);
		}

		if (e_svoTI_NumberOfBounces > 2)
		{
			PROFILE_LABEL_SCOPE("TI_INJECT_REFL1");

			m_nCurPropagationPassID++;
			for (int nNodesForUpdateStartIndex = 0; nNodesForUpdateStartIndex < m_arrNodesForUpdateIncr.Count(); )
				ExecuteComputeShader("ComputePropagateLighting", m_passPropagateLighting_2to3, &nNodesForUpdateStartIndex, 0, m_arrNodesForUpdateIncr);
		}
	}

	static int nLightsDynamicCountPrevFrame = 0;

	if ((e_svoTI_DynLights && (m_arrLightsDynamic.Count() || nLightsDynamicCountPrevFrame)) || e_svoTI_SunRSMInject)
	{
		PROFILE_LABEL_SCOPE("TI_INJECT_DYNL");

		// TODO: cull not affected nodes

		for (int nNodesForUpdateStartIndex = 0; nNodesForUpdateStartIndex < m_arrNodesForUpdateNear.Count(); )
			ExecuteComputeShader("ComputeDirectDynamicLighting", m_passInjectDynamicLights, &nNodesForUpdateStartIndex, 0, m_arrNodesForUpdateNear);
	}

	nLightsDynamicCountPrevFrame = m_arrLightsDynamic.Count();

	CRenderer::CV_r_shadersasynccompiling = nPrevAsync;

	#endif

	m_pRenderView = nullptr;
}

bool CSvoRenderer::VoxelizeMeshes(CShader* ef, SShaderPass* sfm)
{
	return true;
}

void CSvoRenderer::VoxelizeRE()
{

}

void CSvoRenderer::UpdateGpuVoxParams(I3DEngine::SSvoNodeInfo& nodeInfo)
{
	float fBoxSize = nodeInfo.wsBox.GetSize().x;
	Vec3 vOrigin = Vec3(0, 0, 0);
	m_wsOffset = Vec4(nodeInfo.wsBox.GetCenter(), nodeInfo.wsBox.GetSize().x);
	m_tcOffset = Vec4(nodeInfo.tcBox.min, (float)gRenDev->GetMainFrameID());

	Matrix44A m_mOrthoProjection;
	mathMatrixOrtho(&m_mOrthoProjection, fBoxSize, fBoxSize, 0.0f, fBoxSize);

	// Direction Vectors moved to shader constants
	// as we always use global X,-Y,Z vectors

	// Matrices
	Matrix44A VoxelizationView[3];
	Vec3 EdgeCenter = vOrigin - (Vec3(1.0f, 0.0f, 0.0f) * (fBoxSize / 2));
	mathMatrixLookAt(&(VoxelizationView[0]), EdgeCenter, vOrigin, Vec3(0.0f, 0.0f, 1.0f));

	EdgeCenter = vOrigin - (Vec3(0.0f, -1.0f, 0.0f) * (fBoxSize / 2));
	mathMatrixLookAt(&(VoxelizationView[1]), EdgeCenter, vOrigin, Vec3(0.0f, 0.0f, 1.0f));

	EdgeCenter = vOrigin - (Vec3(0.0f, 0.0f, 1.0f) * (fBoxSize / 2));
	mathMatrixLookAt(&(VoxelizationView[2]), EdgeCenter, vOrigin, Vec3(0.0f, -1.0f, 0.0f));

	m_mGpuVoxViewProj[0] = VoxelizationView[0] * m_mOrthoProjection;
	m_mGpuVoxViewProj[1] = VoxelizationView[1] * m_mOrthoProjection;
	m_mGpuVoxViewProj[2] = VoxelizationView[2] * m_mOrthoProjection;
}

void CSvoRenderer::ExecuteComputeShader(const char* szTechFinalName, CSvoComputePass& rp, int* pnNodesForUpdateStartIndex, int nObjPassId, PodArray<I3DEngine::SSvoNodeInfo>& arrNodesForUpdate)
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ

	FUNCTION_PROFILER_RENDERER();

	rp.SetTechnique(m_pShader, szTechFinalName, GetRunTimeFlags(true, false));

	// setup in/out textures

	if (&rp == &m_passInjectAirOpacity)
	{
		// update OPAC
		rp.SetOutputUAV(2, vp_RGB0.pTex);
		if (vp_OPAC.pUAV)
			rp.SetOutputUAV(7, vp_OPAC.pTex);

		rp.SetTexture(15, m_pNoiseTex);
		SetupSvoTexturesForRead(m_texInfo, rp, 0, 1);
		SetupCommonSamplers(rp);

		rp.BeginConstantUpdate();

		SetupCommonConstants(NULL, rp, NULL);
		SetupLightSources(m_arrLightsStatic, rp);
		SetupNodesForUpdate(*pnNodesForUpdateStartIndex, arrNodesForUpdate, rp);
	}
	else if (&rp == &m_passInjectStaticLights)
	{
		// update RGB1
		rp.SetOutputUAV(2, vp_RGB0.pTex);
		rp.SetOutputUAV(7, vp_RGB1.pTex);

		if (vp_DYNL.pUAV)
			rp.SetOutputUAV(6, vp_DYNL.pTex);

		SetupSvoTexturesForRead(m_texInfo, rp, 0);
		SetupRsmSunTextures(rp);
		SetupCommonSamplers(rp);

		rp.BeginConstantUpdate();

		SetupCommonConstants(NULL, rp, NULL);
		SetupRsmSunConstants(rp);
		SetupLightSources(m_arrLightsStatic, rp);
		SetupNodesForUpdate(*pnNodesForUpdateStartIndex, arrNodesForUpdate, rp);
	}
	else if (&rp == &m_passInjectDynamicLights)
	{
		BindTiledLights(m_arrLightsDynamic, (CComputeRenderPass&)rp);

		// update RGB
		rp.SetOutputUAV(2, vp_RGB0.pTex);

		if (e_svoTI_NumberOfBounces == 1)
			rp.SetOutputUAV(7, vp_RGB1.pTex);
		if (e_svoTI_NumberOfBounces == 2)
			rp.SetOutputUAV(7, vp_RGB2.pTex);
		if (e_svoTI_NumberOfBounces == 3)
			rp.SetOutputUAV(7, vp_RGB3.pTex);

		rp.SetOutputUAV(5, vp_DYNL.pTex);
		rp.SetTexture(15, m_pNoiseTex);

		SetupSvoTexturesForRead(m_texInfo, rp, 0);
		SetupRsmSunTextures(rp);
		SetupCommonSamplers(rp);

		rp.BeginConstantUpdate();

		SetupRsmSunConstants(rp);
		SetupCommonConstants(NULL, rp, NULL);
		SetupLightSources(m_arrLightsDynamic, rp);
		SetupNodesForUpdate(*pnNodesForUpdateStartIndex, arrNodesForUpdate, rp);
	}
	else if (&rp == &m_passPropagateLighting_1to2)
	{
		// update RGB2
		if (vp_RGB0.pUAV)
			rp.SetOutputUAV(0, vp_RGB0.pTex);
		SetupSvoTexturesForRead(m_texInfo, rp, 1); // input
		if (vp_RGB2.pUAV)
			rp.SetOutputUAV(5, vp_RGB2.pTex);
		if (vp_ALDI.pUAV)
			rp.SetOutputUAV(6, vp_ALDI.pTex);
		if (vp_DYNL.pUAV)
			rp.SetOutputUAV(7, vp_DYNL.pTex);

		SetupCommonSamplers(rp);

		rp.BeginConstantUpdate();

		SetupCommonConstants(NULL, rp, NULL);
		SetupLightSources(m_arrLightsStatic, rp);
		SetupNodesForUpdate(*pnNodesForUpdateStartIndex, arrNodesForUpdate, rp);
	}
	else if (&rp == &m_passPropagateLighting_2to3)
	{
		// update RGB3
		if (vp_RGB0.pUAV)
			rp.SetOutputUAV(0, vp_RGB0.pTex);
		if (vp_RGB1.pUAV)
			rp.SetOutputUAV(1, vp_RGB1.pTex);
		SetupSvoTexturesForRead(m_texInfo, rp, 2); // input
		if (vp_RGB3.pUAV)
			rp.SetOutputUAV(5, vp_RGB3.pTex);
		if (vp_DYNL.pUAV)
			rp.SetOutputUAV(7, vp_DYNL.pTex);

		SetupCommonSamplers(rp);

		rp.BeginConstantUpdate();

		SetupCommonConstants(NULL, rp, NULL);
		SetupLightSources(m_arrLightsStatic, rp);
		SetupNodesForUpdate(*pnNodesForUpdateStartIndex, arrNodesForUpdate, rp);
	}
	else if (&rp == &m_passClearBricks)
	{
		if (vp_RGB4.pUAV)
			rp.SetOutputUAV(3, vp_RGB4.pTex);
		if (vp_OPAC.pUAV)
			rp.SetOutputUAV(4, vp_OPAC.pTex);
		if (vp_RGB3.pUAV)
			rp.SetOutputUAV(6, vp_RGB3.pTex);
		if (vp_RGB1.pUAV)
			rp.SetOutputUAV(7, vp_RGB1.pTex);
		if (vp_RGB2.pUAV)
			rp.SetOutputUAV(5, vp_RGB2.pTex);
		if (vp_ALDI.pUAV)
			rp.SetOutputUAV(0, vp_ALDI.pTex);

		SetupCommonSamplers(rp);

		rp.BeginConstantUpdate();

		SetupCommonConstants(NULL, rp, NULL);
		SetupNodesForUpdate(*pnNodesForUpdateStartIndex, arrNodesForUpdate, rp);
	}

	{
		rp.SetDispatchSize(e_svoDispatchX, e_svoDispatchY, 1);

		rp.PrepareResourcesForUse(GetDeviceObjectFactory().GetCoreCommandList());

		SScopedComputeCommandList computeCommandList(e_svoTI_AsyncCompute != 0);
		rp.Execute(computeCommandList, EShaderStage_All);
	}

	#endif
}

CTexture* CSvoRenderer::GetGBuffer(int nId) // simplify branch compatibility
{
	CTexture* pRes;

	if (nId == 0)
		pRes = CRendererResources::s_ptexSceneNormalsMap;
	else if (nId == 1)
		pRes = CRendererResources::s_ptexSceneDiffuse;
	else if (nId == 2)
		pRes = CRendererResources::s_ptexSceneSpecular;
	else
		pRes = 0;

	return pRes;
}

void CSvoRenderer::TropospherePass()
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ

	CSvoFullscreenPass& rp = m_passTroposphere;

	if (m_texInfo.bSvoFreeze || !m_texInfo.pTexTree)
		return;

	const char* szTechFinalName = "RenderAtmosphere";

	rp.SetTechnique(m_pShader, szTechFinalName, GetRunTimeFlags(0));
	rp.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
	rp.SetState(GS_NODEPTHTEST);

	rp.SetRenderTarget(0, m_pRT_AIR_MIN);
	rp.SetRenderTarget(1, m_pRT_SHAD_MIN_MAX);
	rp.SetRenderTarget(2, m_pRT_AIR_MAX);
	rp.SetRequireWorldPos(true);
	rp.SetRequirePerViewConstantBuffer(true);

	SetupCommonSamplers(rp);
	SetupSvoTexturesForRead(m_texInfo, rp, e_svoTI_NumberOfBounces, 0, 0);
	SetupGBufferTextures(rp);

	rp.BeginConstantUpdate();

	SetupCommonConstants(NULL, rp, rp.GetRenderTarget(0));
	SetupLightSources(m_arrLightsStatic, rp);

	rp.Execute();

	#endif
}

void CSvoRenderer::TraceSunShadowsPass()
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ

	CSvoFullscreenPass& rp = m_passTroposphere;

	if (m_texInfo.bSvoFreeze || !m_texInfo.pTexTree)
		return;

	const char* szTechFinalName = "TraceSunShadows";

	rp.SetTechnique(m_pShader, szTechFinalName, GetRunTimeFlags(0));
	rp.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
	rp.SetState(GS_NODEPTHTEST);

	rp.SetRenderTarget(0, m_pRT_SHAD_MIN_MAX);

	rp.SetRequireWorldPos(true);
	rp.SetRequirePerViewConstantBuffer(true);

	SetupCommonSamplers(rp);
	SetupSvoTexturesForRead(m_texInfo, rp, e_svoTI_NumberOfBounces, 0, 0);
	SetupGBufferTextures(rp);

	int nTex0, nTex1, nTex2;
	ITerrain* pTerrain = gEnv->p3DEngine->GetITerrain();
	if (pTerrain)
		pTerrain->GetAtlasTexId(nTex0, nTex1, nTex2);
	CTexture* pHM = CTexture::GetByID(nTex2);
	rp.SetTexture(8, pHM);

	rp.BeginConstantUpdate();

	SetupCommonConstants(NULL, rp, rp.GetRenderTarget(0));
	SetupLightSources(m_arrLightsStatic, rp);

	rp.Execute();

	#endif
}

void CSvoRenderer::SetupGBufferTextures(CSvoFullscreenPass& rp)
{
	rp.SetTexture(4, CRendererResources::s_ptexLinearDepth);
	rp.SetTexture(14, GetGBuffer(0));
	rp.SetTexture(5, GetGBuffer(1));
	rp.SetTexture(7, GetGBuffer(2));
}

void CSvoRenderer::ConeTracePass(SSvoTargetsSet* pTS)
{
	CSvoFullscreenPass& rp = pTS->passConeTrace;

	CheckAllocateRT(pTS == &m_tsSpec);

	if (!e_svoTI_Active || !e_svoTI_Apply || !e_svoRender || !m_pShader || m_texInfo.bSvoFreeze || !m_texInfo.pTexTree)
		return;

	const char* szTechFinalName = "ConeTracePass";
	const bool bBindDynamicLights = !GetIntegratioMode() && e_svoTI_InjectionMultiplier && m_arrLightsDynamic.Count();

	rp.SetTechnique(m_pShader, szTechFinalName, GetRunTimeFlags(pTS == &m_tsDiff));
	rp.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
	rp.SetState(GS_NODEPTHTEST);

	rp.SetRenderTarget(0, pTS->pRT_ALD_0);
	rp.SetRenderTarget(1, pTS->pRT_RGB_0);
	rp.SetRequireWorldPos(true);
	rp.SetRequirePerViewConstantBuffer(true);

	SetupRsmSunTextures(rp);

	SetupSvoTexturesForRead(m_texInfo, rp, (e_svoTI_Active ? e_svoTI_NumberOfBounces : 0), 0, 0);

	rp.SetTexture(10, pTS->pRT_ALD_1);
	rp.SetTexture(11, pTS->pRT_RGB_1);

	SetupGBufferTextures(rp);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (m_pTexIndA)
	{
		rp.SetTexture(8, m_pTexTexA);
		rp.SetTexture(9, m_pTexTriA);
		rp.SetTexture(13, m_pTexIndA);
	}
	#endif

	if (bBindDynamicLights)
	{
		BindTiledLights(m_arrLightsDynamic, (CFullscreenPass&)rp);
	}

	if (GetIntegratioMode() && e_svoTI_SSDepthTrace)
	{
		if (CRendererResources::s_ptexHDRTargetPrev->GetUpdateFrameID() > 1)
			rp.SetTexture(12, CRendererResources::s_ptexHDRTargetPrev);
		else
			rp.SetTexture(12, CRendererResources::s_ptexBlack);
	}

	{
		const bool setupCloudShadows = gcpRendD3D->m_bShadowsEnabled && gcpRendD3D->m_bCloudShadowsEnabled;
		if (setupCloudShadows)
		{
			// cloud shadow map
			m_pCloudShadowTex = gcpRendD3D->GetCloudShadowTextureId() > 0 ? CTexture::GetByID(gcpRendD3D->GetCloudShadowTextureId()) : CRendererResources::s_ptexWhite;
			assert(m_pCloudShadowTex);

			rp.SetTexture(15, m_pCloudShadowTex);
		}
		else
		{
			rp.SetTexture(15, CRendererResources::s_ptexWhite);
		}
	}

	rp.SetTexture(8, GetUtils().GetVelocityObjectRT(RenderView()));

	SetupCommonSamplers(rp);

	rp.BeginConstantUpdate();
	SetupCommonConstants(pTS, rp, pTS->pRT_ALD_0);
	SetupRsmSunConstants(rp);

	if (bBindDynamicLights)
	{
		SetupLightSources(m_arrLightsDynamic, rp);
	}

	rp.Execute();
}

template<class T>
void CSvoRenderer::SetupCommonConstants(SSvoTargetsSet* pTS, T& rp, CTexture* pRT)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CRenderView* pRenderView = RenderView();

	const int32 renderWidth = pRenderView->GetRenderResolution()[0];
	const int32 renderHeight = pRenderView->GetRenderResolution()[1];

	const SRenderViewInfo& viewInfo = pRenderView->GetViewInfo(CCamera::eEye_Left);

	{
		static CCryNameR paramName("SVO_ReprojectionMatrix");

		static int nReprojFrameId = -1;
		if ((pTS == &m_tsDiff) && nReprojFrameId != pRenderView->GetFrameId())
		{
			nReprojFrameId = pRenderView->GetFrameId();

			const CCamera& cam = pRenderView->GetCamera(CCamera::eEye_Left);

			Matrix44A matView = cam.GetViewMatrix();

			Vec3 zAxis = matView.GetRow(1);
			matView.SetRow(1, -matView.GetRow(2));
			matView.SetRow(2, zAxis);
			float z = matView.m13;
			matView.m13 = -matView.m23;
			matView.m23 = z;

			Matrix44A matProj;
			mathMatrixPerspectiveFov(&matProj, cam.GetFov(), cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
			static Matrix44A matPrevView = matView;
			static Matrix44A matPrevProj = matProj;
			rd->GetReprojectionMatrix(m_matReproj, matView, matProj, matPrevView, matPrevProj, cam.GetFarPlane());
			matPrevView = matView;
			matPrevProj = matProj;
		}
		rp.SetConstantArray(paramName, (Vec4*)m_matReproj.GetData(), 3);
	}

	{
		static CCryNameR paramName("SVO_FrameIdByte");
		Vec4 vData((float)(pRenderView->GetFrameId() & 255), (float)pRenderView->GetFrameId(), 3, 4);
		if (rd->GetActiveGPUCount() > 1)
			vData.x = 0;
		rp.SetConstantArray(paramName, (Vec4*)&vData, 1);
	}

	{
		static CCryNameR paramName("SVO_CamPos");
		Vec4 vData(gEnv->pSystem->GetViewCamera().GetPosition(), 0);
		rp.SetConstantArray(paramName, (Vec4*)&vData, 1);
	}

	{
		static CCryNameR paramName("SVO_ViewProj");
		static CCryNameR paramNamePrev("SVO_ViewProjPrev");

		Matrix44A mViewProj;
		mViewProj = viewInfo.cameraProjMatrix;
		mViewProj.Transpose();

		rp.SetConstantArray(paramName, alias_cast<Vec4*>(&mViewProj), 4);
		rp.SetConstantArray(paramNamePrev, alias_cast<Vec4*>(&m_matViewProjPrev), 4);
	}

	if (pRT)
	{
		int nTargetSize = pRT->GetWidth() + pRT->GetHeight() + int(e_svoTI_SkyColorMultiplier > 0) + e_svoTI_Diffuse_Cache + e_svoTI_ShadowsFromSun;
		bool bNoReprojection = (rp.nPrevTargetSize != nTargetSize) || (pRenderView->GetShaderRenderingFlags() & SHDF_CUBEMAPGEN) || (rd->GetActiveGPUCount() > 1);
		rp.nPrevTargetSize = nTargetSize;

		static CCryNameR paramName("SVO_TargetResScale");
		float fSizeRatioW = float(renderWidth) / pRT->GetWidth();
		float fSizeRatioH = float(renderHeight) / pRT->GetHeight();
		Vec4 vData(fSizeRatioW, fSizeRatioH, e_svoTI_TemporalFilteringBase, (float)bNoReprojection);
		rp.SetConstantArray(paramName, (Vec4*)&vData, 1);
	}

	{
		static CCryNameR paramName("SVO_helperInfo");
		rp.SetConstantArray(paramName, (Vec4*)&m_texInfo.helperInfo, 1);
	}

	{
		static CCryNameR paramName0("SVO_FrustumVerticesCam0");
		static CCryNameR paramName1("SVO_FrustumVerticesCam1");
		static CCryNameR paramName2("SVO_FrustumVerticesCam2");
		static CCryNameR paramName3("SVO_FrustumVerticesCam3");
		Vec3 pvViewFrust[8];
		pRenderView->GetCamera(CCamera::eEye_Left).CalcAsymmetricFrustumVertices(pvViewFrust);
		Vec3 vOrigin = pRenderView->GetCamera(CCamera::eEye_Left).GetPosition();
		Vec4 vData0(pvViewFrust[4] - vOrigin, 0);
		Vec4 vData1(pvViewFrust[5] - vOrigin, 0);
		Vec4 vData2(pvViewFrust[6] - vOrigin, 0);
		Vec4 vData3(pvViewFrust[7] - vOrigin, 0);
		rp.SetConstantArray(paramName0, (Vec4*)&vData0, 1);
		rp.SetConstantArray(paramName1, (Vec4*)&vData1, 1);
		rp.SetConstantArray(paramName2, (Vec4*)&vData2, 1);
		rp.SetConstantArray(paramName3, (Vec4*)&vData3, 1);
	}

	if ((e_svoTI_AnalyticalGI || e_svoTI_AnalyticalOccluders) && m_texInfo.arrAnalyticalOccluders[0][0].radius)
	{
		static CCryNameR paramName("SVO_AnalyticalOccluders");
		rp.SetConstantArray(paramName, (Vec4*)&m_texInfo.arrAnalyticalOccluders[0][0], sizeof(m_texInfo.arrAnalyticalOccluders[0]) / sizeof(Vec4));
	}

	if (e_svoTI_AnalyticalOccluders && m_texInfo.arrAnalyticalOccluders[1][0].radius)
	{
		static CCryNameR paramName("SVO_PostOccluders");
		rp.SetConstantArray(paramName, (Vec4*)&m_texInfo.arrAnalyticalOccluders[1][0], sizeof(m_texInfo.arrAnalyticalOccluders[1]) / sizeof(Vec4));
	}

	if (m_texInfo.arrPortalsPos[0].z)
	{
		static CCryNameR paramName_PortalsPos("SVO_PortalsPos");
		rp.SetConstantArray(paramName_PortalsPos, (Vec4*)&m_texInfo.arrPortalsPos[0], SVO_MAX_PORTALS);
		static CCryNameR paramName_PortalsDir("SVO_PortalsDir");
		rp.SetConstantArray(paramName_PortalsDir, (Vec4*)&m_texInfo.arrPortalsDir[0], SVO_MAX_PORTALS);
	}

	{
		static CCryNameR paramName("SVO_CloudShadowAnimParams");
		CD3D9Renderer* const __restrict r = gcpRendD3D;
		SRenderViewShaderConstants& PF = pRenderView->GetShaderConstants();
		Vec4 vData;
		vData[0] = PF.pCloudShadowAnimParams.x;
		vData[1] = PF.pCloudShadowAnimParams.y;
		vData[2] = PF.pCloudShadowAnimParams.z;
		vData[3] = PF.pCloudShadowAnimParams.w;
		rp.SetConstantArray(paramName, (Vec4*)&vData, 1);
	}

	{
		static CCryNameR paramName("SVO_CloudShadowParams");
		CD3D9Renderer* const __restrict r = gcpRendD3D;
		SRenderViewShaderConstants& PF = pRenderView->GetShaderConstants();
		Vec4 vData;
		vData[0] = PF.pCloudShadowParams.x;
		vData[1] = PF.pCloudShadowParams.y;
		vData[2] = PF.pCloudShadowParams.z;
		vData[3] = PF.pCloudShadowParams.w;
		rp.SetConstantArray(paramName, (Vec4*)&vData, 1);
	}

	if (pTS)
	{
		static CCryNameR paramName("SVO_SrcPixSize");
		Vec4 vData(0, 0, 0, 0);
		vData.x = 1.f / float(pTS->pRT_ALD_DEM_MIN_0->GetWidth());
		vData.y = 1.f / float(pTS->pRT_ALD_DEM_MIN_0->GetHeight());
		rp.SetConstantArray(paramName, (Vec4*)&vData, 1);
	}

	{
		auto screenResolution = Vec2i(CRendererResources::s_renderWidth, CRendererResources::s_renderHeight);
		static CCryNameR paramName("SVO_DepthTargetRes");
		Vec4 vData((float)screenResolution.x, (float)screenResolution.y, 0, 0);
		rp.SetConstantArray(paramName, (Vec4*)&vData, 1);
	}
}

template<class T>
void CSvoRenderer::SetupCommonSamplers(T& rp)
{
	rp.SetSampler(0, EDefaultSamplerStates::PointClamp);
	rp.SetSampler(1, EDefaultSamplerStates::LinearClamp);
	rp.SetSampler(2, EDefaultSamplerStates::LinearWrap);
}

void CSvoRenderer::DrawPonts(PodArray<SVF_P3F_C4B_T2F>& arrVerts)
{
	SPostEffectsUtils::UpdateFrustumCorners();

	CVertexBuffer strip(arrVerts.GetElements(), EDefaultInputLayouts::P3F_C4B_T2F);

	// OLD PIPELINE
	ASSERT_LEGACY_PIPELINE
	//gRenDev->DrawPrimitivesInternal(&strip, arrVerts.Count() / max(1, e_svoRender), eptPointList);
}

void CSvoRenderer::UpdateRender(CRenderView* pRenderView)
{
	m_pRenderView = pRenderView;
	int nPrevAsync = CRenderer::CV_r_shadersasynccompiling;
	if (gEnv->IsEditor())
		CRenderer::CV_r_shadersasynccompiling = 0;

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (e_svoEnabled && e_svoTI_Active && e_svoTI_Apply && m_texInfo.bSvoReady && e_svoTI_Troposphere_Active && m_pRT_AIR_MIN)
	{
		PROFILE_LABEL_SCOPE("TI_GEN_AIR");

		if (e_svoTI_DiffuseAmplifier || e_svoTI_SpecularAmplifier || !e_svoTI_DiffuseBias)
			TropospherePass();
	}

	if (e_svoEnabled && e_svoTI_Active && e_svoTI_Apply && m_texInfo.bSvoReady && e_svoTI_ShadowsFromSun && m_pRT_SHAD_MIN_MAX)
	{
		PROFILE_LABEL_SCOPE("TI_GEN_SHAD");

		TraceSunShadowsPass();
	}
	#endif

	{
		PROFILE_LABEL_SCOPE("TI_GEN_DIFF");

		ConeTracePass(&m_tsDiff);
	}
	if (GetIntegratioMode() == 2 && e_svoTI_SpecularAmplifier)
	{
		PROFILE_LABEL_SCOPE("TI_GEN_SPEC");

		ConeTracePass(&m_tsSpec);
	}

	{
		PROFILE_LABEL_SCOPE("TI_DEMOSAIC_DIFF");

		DemosaicPass(&m_tsDiff);
	}
	if (GetIntegratioMode() == 2 && e_svoTI_SpecularAmplifier)
	{
		PROFILE_LABEL_SCOPE("TI_DEMOSAIC_SPEC");

		DemosaicPass(&m_tsSpec);
	}

	{
		PROFILE_LABEL_SCOPE("TI_UPSCALE_DIFF");

		UpscalePass(&m_tsDiff);
	}
	if (GetIntegratioMode() == 2 && e_svoTI_SpecularAmplifier && !e_svoTI_SpecularFromDiffuse)
	{
		PROFILE_LABEL_SCOPE("TI_UPSCALE_SPEC");

		UpscalePass(&m_tsSpec);
	}

	const SRenderViewInfo& viewInfo = pRenderView->GetViewInfo(CCamera::eEye_Left);

	{
		m_matViewProjPrev = viewInfo.cameraProjMatrix;
		m_matViewProjPrev.Transpose();
	}

	if (gEnv->IsEditor())
		CRenderer::CV_r_shadersasynccompiling = nPrevAsync;

	m_pRenderView = nullptr;
}

void CSvoRenderer::DemosaicPass(SSvoTargetsSet* pTS)
{
	CSvoFullscreenPass& rp = pTS->passDemosaic;

	const char* szTechFinalName = "DemosaicPass";

	if (!e_svoTI_Active || !e_svoTI_Apply || !e_svoRender || !m_pShader || !pTS->pRT_ALD_0 || m_texInfo.bSvoFreeze || !m_texInfo.pTexTree)
		return;

	rp.SetTechnique(m_pShader, szTechFinalName, GetRunTimeFlags(pTS == &m_tsDiff));
	rp.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
	rp.SetState(GS_NODEPTHTEST);

	rp.SetRenderTarget(0, pTS->pRT_RGB_DEM_MIN_0);
	rp.SetRenderTarget(1, pTS->pRT_ALD_DEM_MIN_0);
	rp.SetRenderTarget(2, pTS->pRT_RGB_DEM_MAX_0);
	rp.SetRenderTarget(3, pTS->pRT_ALD_DEM_MAX_0);
	rp.SetRequireWorldPos(true);
	rp.SetRequirePerViewConstantBuffer(true);

	SetupCommonSamplers(rp);

	SetupSvoTexturesForRead(m_texInfo, rp, (e_svoTI_Active ? e_svoTI_NumberOfBounces : 0), 0, 0);

	SetupGBufferTextures(rp);

	rp.SetTexture(10, pTS->pRT_ALD_0);
	rp.SetTexture(11, pTS->pRT_RGB_0);

	rp.SetTexture(6, pTS->pRT_RGB_DEM_MIN_1);
	rp.SetTexture(9, pTS->pRT_ALD_DEM_MIN_1);
	rp.SetTexture(12, pTS->pRT_RGB_DEM_MAX_1);
	rp.SetTexture(13, pTS->pRT_ALD_DEM_MAX_1);

	rp.SetTexture(8, GetUtils().GetVelocityObjectRT(RenderView()));

	rp.BeginConstantUpdate();

	SetupCommonConstants(pTS, rp, pTS->pRT_ALD_0);

	rp.Execute();
}

template<class T>
void CSvoRenderer::SetupLightSources(PodArray<I3DEngine::SLightTI>& lightsTI, T& rp)
{
	const int nLightGroupsNum = 2;

	static CCryNameR paramNamesLightPos[nLightGroupsNum] =
	{
		CCryNameR("SVO_LightPos0"),
		CCryNameR("SVO_LightPos1"),
	};
	static CCryNameR paramNamesLightDir[nLightGroupsNum] =
	{
		CCryNameR("SVO_LightDir0"),
		CCryNameR("SVO_LightDir1"),
	};
	static CCryNameR paramNamesLightCol[nLightGroupsNum] =
	{
		CCryNameR("SVO_LightCol0"),
		CCryNameR("SVO_LightCol1"),
	};

	for (int g = 0; g < nLightGroupsNum; g++)
	{
		Vec4 LightPos[4];
		Vec4 LightDir[4];
		Vec4 LightCol[4];

		for (int x = 0; x < 4; x++)
		{
			int nId = g * 4 + x;

			LightPos[x] = (nId < lightsTI.Count()) ? lightsTI[nId].vPosR : Vec4(0, 0, 0, 0);
			LightDir[x] = (nId < lightsTI.Count()) ? lightsTI[nId].vDirF : Vec4(0, 0, 0, 0);
			LightCol[x] = (nId < lightsTI.Count()) ? lightsTI[nId].vCol : Vec4(0, 0, 0, 0);
		}

		rp.SetConstantArray(paramNamesLightPos[g], alias_cast<Vec4*>(&LightPos[0][0]), 4);
		rp.SetConstantArray(paramNamesLightDir[g], alias_cast<Vec4*>(&LightDir[0][0]), 4);
		rp.SetConstantArray(paramNamesLightCol[g], alias_cast<Vec4*>(&LightCol[0][0]), 4);
	}
}

void CSvoRenderer::SetupNodesForUpdate(int& nNodesForUpdateStartIndex, PodArray<I3DEngine::SSvoNodeInfo>& arrNodesForUpdate, CSvoComputePass& rp)
{
	static CCryNameR paramNames[SVO_MAX_NODE_GROUPS] =
	{
		CCryNameR("SVO_NodesForUpdate0"),
		CCryNameR("SVO_NodesForUpdate1"),
		CCryNameR("SVO_NodesForUpdate2"),
		CCryNameR("SVO_NodesForUpdate3"),
	};

	for (int g = 0; g < SVO_MAX_NODE_GROUPS; g++)
	{
		Vec4 matVal[4];

		for (int x = 0; x < 4; x++)
		{
			for (int y = 0; y < 4; y++)
			{
				int nId = nNodesForUpdateStartIndex + g * 16 + x * 4 + y;
				matVal[x][y] = 0.1f + ((nId < arrNodesForUpdate.Count()) ? arrNodesForUpdate[nId].nAtlasOffset : -2);

				if (nId < arrNodesForUpdate.Count())
				{
					float fNodeSize = arrNodesForUpdate[nId].wsBox.GetSize().x;

					if (e_svoDebug == 8)
						gRenDev->GetIRenderAuxGeom()->DrawAABB(arrNodesForUpdate[nId].wsBox, 0, ColorF(fNodeSize == 4.f, fNodeSize == 8.f, fNodeSize == 16.f), eBBD_Faceted);
				}
			}
		}

		rp.SetConstantArray(paramNames[g], &matVal[0], 4);
	}

	nNodesForUpdateStartIndex += 4 * 4 * SVO_MAX_NODE_GROUPS;
}

template<class T>
void CSvoRenderer::SetupSvoTexturesForRead(I3DEngine::SSvoStaticTexInfo& texInfo, T& rp, int nStage, int nStageOpa, int nStageNorm)
{
	rp.SetTexture(0, (static_cast<CTexture*>(texInfo.pTexTree.get())));

	rp.SetTexture(1, CRendererResources::s_ptexBlack);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ

	if (nStage == 0)
		rp.SetTexture(1, CTexture::GetByID(vp_RGB0.nTexId));
	else if (nStage == 1)
		rp.SetTexture(1, CTexture::GetByID(vp_RGB1.nTexId));
	else if (nStage == 2)
		rp.SetTexture(1, CTexture::GetByID(vp_RGB2.nTexId));
	else if (nStage == 3)
		rp.SetTexture(1, CTexture::GetByID(vp_RGB3.nTexId));
	else if (nStage == 4)
		rp.SetTexture(1, CTexture::GetByID(vp_RGB4.nTexId));

	if (nStageNorm == 0)
		rp.SetTexture(2, CTexture::GetByID(vp_NORM.nTexId));

	#endif

	if (nStageOpa == 0)
		rp.SetTexture(3, static_cast<CTexture*>(texInfo.pTexOpac.get()));

	#ifdef FEATURE_SVO_GI_ALLOW_HQ

	else if (nStageOpa == 1)
		rp.SetTexture(3, CTexture::GetByID(vp_RGB4.nTexId));

	if (texInfo.pTexTris)
		rp.SetTexture(6, static_cast<CTexture*>(texInfo.pTexTris.get()));
	if (texInfo.pGlobalSpecCM)
		rp.SetTexture(6, static_cast<CTexture*>(texInfo.pGlobalSpecCM.get()));

	#endif
}

void CSvoRenderer::CheckAllocateRT(bool bSpecPass)
{
	auto screenResolution = Vec2i(CRendererResources::s_renderWidth, CRendererResources::s_renderHeight);
	int nWidth = screenResolution.x;
	int nHeight = screenResolution.y;

	int nVoxProxyViewportDim = e_svoVoxGenRes;

	int nResScaleBase = SATURATEB(e_svoTI_ResScaleBase + e_svoTI_LowSpecMode);
	int nDownscaleAir = SATURATEB(e_svoTI_ResScaleAir + e_svoTI_LowSpecMode);

	int nInW = (nWidth / nResScaleBase);
	int nInH = (nHeight / nResScaleBase);
	int nAirW = (nWidth / nDownscaleAir);
	int nAirH = (nHeight / nDownscaleAir);

	if (!bSpecPass)
	{
		CheckCreateUpdateRT(m_tsDiff.pRT_ALD_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_DIFF_ALD");
		CheckCreateUpdateRT(m_tsDiff.pRT_ALD_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_DIFF_ALD");
		CheckCreateUpdateRT(m_tsDiff.pRT_RGB_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_DIFF_RGB");
		CheckCreateUpdateRT(m_tsDiff.pRT_RGB_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_DIFF_RGB");

		CheckCreateUpdateRT(m_tsSpec.pRT_ALD_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_SPEC_ALD");
		CheckCreateUpdateRT(m_tsSpec.pRT_ALD_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_SPEC_ALD");
		CheckCreateUpdateRT(m_tsSpec.pRT_RGB_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_SPEC_RGB");
		CheckCreateUpdateRT(m_tsSpec.pRT_RGB_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_SPEC_RGB");

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		if (e_svoTI_Troposphere_Active)
		{
			CheckCreateUpdateRT(m_pRT_NID_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_NID");
			CheckCreateUpdateRT(m_pRT_AIR_MIN, nAirW, nAirH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_AIR_MIN");
			CheckCreateUpdateRT(m_pRT_AIR_MAX, nAirW, nAirH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_AIR_MAX");
		}
		if (e_svoTI_ShadowsFromSun || e_svoTI_SpecularFromDiffuse)
		{
			CheckCreateUpdateRT(m_pRT_SHAD_MIN_MAX, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_SHAD_MIN_MAX");
			CheckCreateUpdateRT(m_pRT_SHAD_FIN_0, nWidth, nHeight, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_SHAD_FIN");
			CheckCreateUpdateRT(m_pRT_SHAD_FIN_1, nWidth, nHeight, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_SHAD_FIN");
		}
	#endif

		CheckCreateUpdateRT(m_tsDiff.pRT_RGB_DEM_MIN_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_DIFF_FIN_RGB_MIN");
		CheckCreateUpdateRT(m_tsDiff.pRT_ALD_DEM_MIN_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_DIFF_FIN_ALD_MIN");
		CheckCreateUpdateRT(m_tsDiff.pRT_RGB_DEM_MAX_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_DIFF_FIN_RGB_MAX");
		CheckCreateUpdateRT(m_tsDiff.pRT_ALD_DEM_MAX_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_DIFF_FIN_ALD_MAX");
		CheckCreateUpdateRT(m_tsDiff.pRT_RGB_DEM_MIN_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_DIFF_FIN_RGB_MIN");
		CheckCreateUpdateRT(m_tsDiff.pRT_ALD_DEM_MIN_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_DIFF_FIN_ALD_MIN");
		CheckCreateUpdateRT(m_tsDiff.pRT_RGB_DEM_MAX_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_DIFF_FIN_RGB_MAX");
		CheckCreateUpdateRT(m_tsDiff.pRT_ALD_DEM_MAX_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_DIFF_FIN_ALD_MAX");

		CheckCreateUpdateRT(m_tsDiff.pRT_FIN_OUT_0, nWidth, nHeight, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_FIN_DIFF_OUT");
		CheckCreateUpdateRT(m_tsDiff.pRT_FIN_OUT_1, nWidth, nHeight, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_FIN_DIFF_OUT");

		CheckCreateUpdateRT(m_tsSpec.pRT_RGB_DEM_MIN_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_SPEC_FIN_RGB_MIN");
		CheckCreateUpdateRT(m_tsSpec.pRT_ALD_DEM_MIN_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_SPEC_FIN_ALD_MIN");
		CheckCreateUpdateRT(m_tsSpec.pRT_RGB_DEM_MAX_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_SPEC_FIN_RGB_MAX");
		CheckCreateUpdateRT(m_tsSpec.pRT_ALD_DEM_MAX_0, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_SPEC_FIN_ALD_MAX");
		CheckCreateUpdateRT(m_tsSpec.pRT_RGB_DEM_MIN_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_SPEC_FIN_RGB_MIN");
		CheckCreateUpdateRT(m_tsSpec.pRT_ALD_DEM_MIN_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_SPEC_FIN_ALD_MIN");
		CheckCreateUpdateRT(m_tsSpec.pRT_RGB_DEM_MAX_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_SPEC_FIN_RGB_MAX");
		CheckCreateUpdateRT(m_tsSpec.pRT_ALD_DEM_MAX_1, nInW, nInH, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_SPEC_FIN_ALD_MAX");

		CheckCreateUpdateRT(m_tsSpec.pRT_FIN_OUT_0, nWidth, nHeight, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SVO_FIN_SPEC_OUT");
		CheckCreateUpdateRT(m_tsSpec.pRT_FIN_OUT_1, nWidth, nHeight, eTF_R16G16B16A16F, eTT_2D, FT_STATE_CLAMP, "SV1_FIN_SPEC_OUT");

		// swap ping-pong RT
		std::swap(m_tsDiff.pRT_ALD_0, m_tsDiff.pRT_ALD_1);
		std::swap(m_tsDiff.pRT_RGB_0, m_tsDiff.pRT_RGB_1);
		std::swap(m_tsDiff.pRT_RGB_DEM_MIN_0, m_tsDiff.pRT_RGB_DEM_MIN_1);
		std::swap(m_tsDiff.pRT_ALD_DEM_MIN_0, m_tsDiff.pRT_ALD_DEM_MIN_1);
		std::swap(m_tsDiff.pRT_RGB_DEM_MAX_0, m_tsDiff.pRT_RGB_DEM_MAX_1);
		std::swap(m_tsDiff.pRT_ALD_DEM_MAX_0, m_tsDiff.pRT_ALD_DEM_MAX_1);
		std::swap(m_tsDiff.pRT_FIN_OUT_0, m_tsDiff.pRT_FIN_OUT_1);

		std::swap(m_tsSpec.pRT_ALD_0, m_tsSpec.pRT_ALD_1);
		std::swap(m_tsSpec.pRT_RGB_0, m_tsSpec.pRT_RGB_1);
		std::swap(m_tsSpec.pRT_RGB_DEM_MIN_0, m_tsSpec.pRT_RGB_DEM_MIN_1);
		std::swap(m_tsSpec.pRT_ALD_DEM_MIN_0, m_tsSpec.pRT_ALD_DEM_MIN_1);
		std::swap(m_tsSpec.pRT_RGB_DEM_MAX_0, m_tsSpec.pRT_RGB_DEM_MAX_1);
		std::swap(m_tsSpec.pRT_ALD_DEM_MAX_0, m_tsSpec.pRT_ALD_DEM_MAX_1);
		std::swap(m_tsSpec.pRT_FIN_OUT_0, m_tsSpec.pRT_FIN_OUT_1);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		std::swap(m_pRT_SHAD_FIN_0, m_pRT_SHAD_FIN_1);
	#endif
	}
}

bool CSvoRenderer::IsShaderItemUsedForVoxelization(SShaderItem& rShaderItem, IRenderNode* pRN)
{
	CShader* pS = (CShader*)rShaderItem.m_pShader;
	CShaderResources* pR = (CShaderResources*)rShaderItem.m_pShaderResources;

	// skip some objects marked by level designer
	//	if(pRN && pRN->IsRenderNode() && pRN->GetIntegrationType())
	//	return false;

	// skip transparent meshes except decals
	//	if((pR->Opacity() != 1.f) && !(pS->GetFlags()&EF_DECAL))
	//	return false;

	// skip windows
	if (pS && pS->GetShaderType() == eST_Glass)
		return false;

	// skip water
	if (pS && pS->GetShaderType() == eST_Water)
		return false;

	// skip outdoor vegetations
	//	(fog not working) if(pS->GetShaderType() == eST_Vegetation && pRN->IsRenderNode() && !pRN->GetEntityVisArea())
	//	return false;

	return true;
}

inline Vec3 SVO_StringToVector(const char* str)
{
	Vec3 vTemp(0, 0, 0);
	float x, y, z;
	if (sscanf(str, "%f,%f,%f", &x, &y, &z) == 3)
	{
		vTemp(x, y, z);
	}
	else
	{
		vTemp(0, 0, 0);
	}
	return vTemp;
}

void CSvoRenderer::FillForwardParams(SForwardParams& svogiParams, bool enable) const
{
	if (enable)
	{
		svogiParams.IntegrationMode.x = 1.f - e_svoTI_HighGlossOcclusion;

		float fModeFin = 0;
		int nModeGI = GetIntegratioMode();

		if (nModeGI == 0 && GetUseLightProbes())
		{
			// AO modulates diffuse and specular
			fModeFin = 0;
		}
		else if (nModeGI <= 1)
		{
			// GI replaces diffuse and modulates specular
			fModeFin = 1.f;
		}
		else if (nModeGI == 2)
		{
			// GI replaces diffuse and specular
			fModeFin = 2.f;
		}

		svogiParams.IntegrationMode.y = fModeFin;
		svogiParams.IntegrationMode.z = e_svoDVR ? (float)e_svoDVR : ((m_texInfo.bSvoReady && e_svoTI_NumberOfBounces) ? e_svoTI_SpecularAmplifier : 0);
		svogiParams.IntegrationMode.w = e_svoTI_SkyColorMultiplier;
	}
	else
	{
		// turning off by parameters.
		svogiParams.IntegrationMode = Vec4(0.0f, -1.0f, 0.0f, 0.0f);
	}
}

bool CSvoRenderer::SetShaderParameters(float*& pSrc, uint32 paramType, UFloat4* sData)
{
	bool bRes = true;

	CSvoRenderer* pSR = CSvoRenderer::GetInstance();

	if (!pSR)
	{
		if (paramType == ECGP_PB_SvoParams4)
		{
			// SvoParams4 is used by forward tiled shaders even if GI is disabled, it contains info about GI mode and also is GI active or not
			sData[0].f[0] = 0;
			sData[0].f[1] = -1.f;
			sData[0].f[2] = 0;
			sData[0].f[3] = 0;
			return true;
		}

		return false;
	}

	switch (paramType)
	{
	case ECGP_PB_SvoViewProj0:
	case ECGP_PB_SvoViewProj1:
	case ECGP_PB_SvoViewProj2:
		{
			pSrc = pSR->m_mGpuVoxViewProj[((paramType) - ECGP_PB_SvoViewProj0)].GetData();
			break;
		}

	case ECGP_PB_SvoNodesForUpdate0:
	case ECGP_PB_SvoNodesForUpdate1:
	case ECGP_PB_SvoNodesForUpdate2:
	case ECGP_PB_SvoNodesForUpdate3:
	case ECGP_PB_SvoNodesForUpdate4:
	case ECGP_PB_SvoNodesForUpdate5:
	case ECGP_PB_SvoNodesForUpdate6:
	case ECGP_PB_SvoNodesForUpdate7:
		{
			pSrc = &(pSR->m_arrNodesForUpdate[((paramType) - ECGP_PB_SvoNodesForUpdate0)][0][0]);
			break;
		}

	case ECGP_PB_SvoNodeBoxWS:
		{
			sData[0].f[0] = pSR->m_wsOffset.x;
			sData[0].f[1] = pSR->m_wsOffset.y;
			sData[0].f[2] = pSR->m_wsOffset.z;
			sData[0].f[3] = pSR->m_wsOffset.w;
			break;
		}

	case ECGP_PB_SvoNodeBoxTS:
		{
			sData[0].f[0] = pSR->m_tcOffset.x;
			sData[0].f[1] = pSR->m_tcOffset.y;
			sData[0].f[2] = pSR->m_tcOffset.z;
			sData[0].f[3] = pSR->m_tcOffset.w;
			break;
		}

	case ECGP_PB_SvoTreeSettings0:
		{
			sData[0].f[0] = (float)gEnv->p3DEngine->GetTerrainSize();
			sData[0].f[1] = pSR->e_svoMinNodeSize;
			sData[0].f[2] = (float)pSR->m_texInfo.nBrickSize;
			sData[0].f[3] = (float)pSR->e_svoVoxelPoolResolution;
			break;
		}

	case ECGP_PB_SvoTreeSettings1:
		{
			sData[0].f[0] = pSR->e_svoMaxNodeSize;
			sData[0].f[1] = pSR->e_svoTI_RT_MaxDist;
			sData[0].f[2] = pSR->e_svoTI_Troposphere_CloudGen_FreqStep;
			sData[0].f[3] = pSR->e_svoTI_Troposphere_Density;
			break;
		}

	case ECGP_PB_SvoTreeSettings2:
		{
			sData[0].f[0] = pSR->e_svoTI_Troposphere_Ground_Height;
			sData[0].f[1] = pSR->e_svoTI_Troposphere_Layer0_Height;
			sData[0].f[2] = pSR->e_svoTI_Troposphere_Layer1_Height;
			sData[0].f[3] = pSR->e_svoTI_Troposphere_CloudGen_Height;
			break;
		}

	case ECGP_PB_SvoTreeSettings3:
		{
			sData[0].f[0] = pSR->e_svoTI_Troposphere_CloudGen_Freq;
			sData[0].f[1] = pSR->e_svoTI_Troposphere_CloudGen_Scale;
			sData[0].f[2] = pSR->e_svoTI_Troposphere_CloudGenTurb_Freq;
			sData[0].f[3] = pSR->e_svoTI_Troposphere_CloudGenTurb_Scale;
			break;
		}

	case ECGP_PB_SvoTreeSettings4:
		{
			sData[0].f[0] = pSR->e_svoTI_Troposphere_Layer0_Rand;
			sData[0].f[1] = pSR->e_svoTI_Troposphere_Layer1_Rand;
			sData[0].f[2] = pSR->e_svoTI_Troposphere_Layer0_Dens;
			sData[0].f[3] = pSR->e_svoTI_Troposphere_Layer1_Dens;
			break;
		}

	case ECGP_PB_SvoTreeSettings5:
		{
			sData[0].f[0] = pSR->e_svoTI_PortalsInject;
			sData[0].f[1] = pSR->e_svoTI_PortalsDeform;
			sData[0].f[2] = pSR->m_texInfo.vSkyColorTop.x;
			sData[0].f[3] = pSR->m_texInfo.vSkyColorTop.y;
			break;
		}

	case ECGP_PB_SvoParams0:
		{
			sData[0].f[0] = 1.f / max(pSR->e_svoTI_ShadowsSoftness, 0.01f);
			sData[0].f[1] = pSR->e_svoTI_Diffuse_Spr;
			sData[0].f[2] = pSR->e_svoTI_DiffuseBias;

			float fDepth = 0;
			float nWS = pSR->m_texInfo.vSvoOriginAndSize.w;
			while (nWS > pSR->e_svoMinNodeSize)
			{
				nWS /= 2;
				fDepth++;
			}
			sData[0].f[3] = 0.1f + fDepth;

			break;
		}

	case ECGP_PB_SvoParams1:
		{
			// diffuse
			sData[0].f[0] = 1.f / (pSR->e_svoTI_DiffuseConeWidth + 0.00001f);
			sData[0].f[1] = pSR->e_svoTI_ConeMaxLength;
			sData[0].f[2] = 1.f / max(pSR->e_svoTI_SSDepthTrace, 0.01f);
			sData[0].f[3] = (pSR->m_texInfo.bSvoReady && pSR->e_svoTI_NumberOfBounces) ? pSR->e_svoTI_DiffuseAmplifier : 0;
			break;
		}

	case ECGP_PB_SvoParams2:
		{
			// inject
			sData[0].f[0] = pSR->e_svoTI_InjectionMultiplier;
			sData[0].f[1] = 1.f / max(pSR->e_svoTI_PropagationBooster, 0.001f);
			sData[0].f[2] = pSR->e_svoTI_Saturation;
			sData[0].f[3] = pSR->e_svoTI_TranslucentBrightness;
			break;
		}

	case ECGP_PB_SvoParams3:
		{
			// specular params
			sData[0].f[0] = pSR->e_svoTI_Specular_Sev;
			sData[0].f[1] = pSR->m_texInfo.vSkyColorTop.z;
			sData[0].f[2] = pSR->e_svoTI_Troposphere_Brightness * pSR->e_svoTI_Troposphere_Active;
			sData[0].f[3] = (pSR->m_texInfo.bSvoReady && pSR->e_svoTI_NumberOfBounces) ? pSR->e_svoTI_SpecularAmplifier : 0;
			break;
		}

	case ECGP_PB_SvoParams4:
		{
			// LOD & sky color
			sData[0].f[0] = 1.f - pSR->e_svoTI_HighGlossOcclusion;

			float fModeFin = 0;
			int nModeGI = CSvoRenderer::GetInstance()->GetIntegratioMode();

			if (nModeGI == 0 && CSvoRenderer::GetInstance()->GetUseLightProbes())
			{
				// AO modulates diffuse and specular
				fModeFin = 0;
			}
			else if (nModeGI <= 1)
			{
				// GI replaces diffuse and modulates specular
				fModeFin = 1.f;
			}
			else if (nModeGI == 2)
			{
				// GI replaces diffuse and specular
				fModeFin = 2.f;
			}

			sData[0].f[1] = pSR->IsActive() ? fModeFin : -1.f;
			sData[0].f[2] = pSR->e_svoDVR ? (float)pSR->e_svoDVR : ((pSR->m_texInfo.bSvoReady && pSR->e_svoTI_NumberOfBounces) ? pSR->e_svoTI_SpecularAmplifier : 0);
			sData[0].f[3] = pSR->e_svoTI_SkyColorMultiplier;
			break;
		}

	case ECGP_PB_SvoParams5:
		{
			// sky color
			sData[0].f[0] = pSR->e_svoTI_EmissiveMultiplier;
			sData[0].f[1] = (float)pSR->m_nCurPropagationPassID;
			sData[0].f[2] = pSR->e_svoTI_NumberOfBounces - 1.f;
			sData[0].f[3] = pSR->m_texInfo.fGlobalSpecCM_Mult;
			break;
		}

	case ECGP_PB_SvoParams6:
		{
			sData[0].f[0] = pSR->e_svoTI_PointLightsMultiplier;
			sData[0].f[1] = pSR->m_texInfo.vSvoOriginAndSize.x;
			sData[0].f[2] = pSR->e_svoTI_MinReflectance;
			sData[0].f[3] = pSR->m_texInfo.vSvoOriginAndSize.y;
			break;
		}

	case ECGP_PB_SvoParams7:
		{
			sData[0].f[0] = pSR->e_svoTI_AnalyticalOccludersRange;
			sData[0].f[1] = pSR->e_svoTI_AnalyticalOccludersSoftness;
			sData[0].f[2] = pSR->m_texInfo.vSvoOriginAndSize.z;
			sData[0].f[3] = pSR->m_texInfo.vSvoOriginAndSize.w;
			break;
		}

	case ECGP_PB_SvoParams8:
		{
			sData[0].f[0] = pSR->e_svoTI_VoxelOpacityMultiplier;
			sData[0].f[1] = pSR->e_svoTI_SkyLightBottomMultiplier;
			sData[0].f[2] = pSR->e_svoTI_PointLightsBias;
			sData[0].f[3] = 0;
			break;
		}

	case ECGP_PB_SvoParams9:
		{
			sData[0].f[0] = 0;
			sData[0].f[1] = 0;
			sData[0].f[2] = 0;
			sData[0].f[3] = 0;
			break;
		}

	default:
		bRes = false;
	}

	return bRes;
}

void CSvoRenderer::DebugDrawStats(const RPProfilerStats* pBasicStats, float& ypos, const float ystep, float xposms)
{
	ColorF color = Col_Yellow;
	const EDrawTextFlags txtFlags = (EDrawTextFlags)(eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Monospace);

	#define SVO_Draw2dLabel(labelName)                                                                                                             \
	  IRenderAuxText::Draw2dLabel(60, ypos += ystep, 2, &color.r, false, (const char*)(((const char*)( # labelName)) + 10));                       \
	  if (pBasicStats[labelName].gpuTimeMax > 0.01)                                                                                                \
	    IRenderAuxText::Draw2dLabelEx(xposms, ypos, 2, color, txtFlags, "%5.2f Aver=%5.2f Max=%5.2f",                                              \
	                                  pBasicStats[labelName].gpuTime, pBasicStats[labelName].gpuTimeSmoothed, pBasicStats[labelName].gpuTimeMax);  \
	  else                                                                                                                                         \
	    IRenderAuxText::Draw2dLabelEx(xposms, ypos, 2, color, txtFlags, "%5.2f", pBasicStats[labelName].gpuTime);                                  \

	SVO_Draw2dLabel(eRPPSTATS_TI_INJECT_CLEAR);
	SVO_Draw2dLabel(eRPPSTATS_TI_VOXELIZE);
	SVO_Draw2dLabel(eRPPSTATS_TI_INJECT_AIR);
	SVO_Draw2dLabel(eRPPSTATS_TI_INJECT_LIGHT);
	SVO_Draw2dLabel(eRPPSTATS_TI_INJECT_REFL0);
	SVO_Draw2dLabel(eRPPSTATS_TI_INJECT_REFL1);
	SVO_Draw2dLabel(eRPPSTATS_TI_INJECT_DYNL);
	SVO_Draw2dLabel(eRPPSTATS_TI_NID_DIFF);
	SVO_Draw2dLabel(eRPPSTATS_TI_GEN_DIFF);
	SVO_Draw2dLabel(eRPPSTATS_TI_GEN_SPEC);
	SVO_Draw2dLabel(eRPPSTATS_TI_GEN_AIR);
	SVO_Draw2dLabel(eRPPSTATS_TI_DEMOSAIC_DIFF);
	SVO_Draw2dLabel(eRPPSTATS_TI_DEMOSAIC_SPEC);
	SVO_Draw2dLabel(eRPPSTATS_TI_UPSCALE_DIFF);
	SVO_Draw2dLabel(eRPPSTATS_TI_UPSCALE_SPEC);
}

uint64 CSvoRenderer::GetRunTimeFlags(bool bDiffuseMode, bool bPixelShader)
{
	uint64 rtFlags = 0;

	if (e_svoTI_LowSpecMode > 0) // simplify shaders
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (m_texInfo.pGlobalSpecCM && GetIntegratioMode()) // use global env CM
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	#endif

	if (e_svoTI_Troposphere_Active) // compute air lighting as well
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE2];

	if (e_svoTI_Diffuse_Cache) // use pre-baked lighting
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE3];

	if (bDiffuseMode) // diffuse or specular rendering
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE4];

	if (GetIntegratioMode()) // ignore colors and normals for AO only mode
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE5];

	if ((bPixelShader && e_svoTI_HalfresKernelPrimary) || (!bPixelShader && e_svoTI_HalfresKernelSecondary)) // smaller kernel - less de-mosaic work and faster compute update
		rtFlags |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE];

	if (bPixelShader && !GetIntegratioMode() && e_svoTI_InjectionMultiplier) // read sun light and shadow map during final cone tracing
		rtFlags |= g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ];

	if (bPixelShader && !GetIntegratioMode() && e_svoTI_InjectionMultiplier && m_arrLightsDynamic.Count()) // use point lights and shadow maps during final cone tracing
		rtFlags |= g_HWSR_MaskBit[HWSR_POINT_LIGHT];

	if (bPixelShader && e_svoTI_SSDepthTrace) // SS depth trace
		rtFlags |= g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];

	if (bPixelShader && e_svoTI_DualTracing && (gRenDev->GetActiveGPUCount() >= e_svoTI_DualTracing))
		rtFlags |= g_HWSR_MaskBit[HWSR_MOTION_BLUR];

	if (e_svoTI_AnalyticalGI)
		rtFlags |= g_HWSR_MaskBit[HWSR_LIGHTVOLUME1];

	if (e_svoTI_TraceVoxels)
		rtFlags |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

	if (m_texInfo.arrPortalsPos[0].z)
		rtFlags |= g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];

	if (bPixelShader && (e_svoTI_AnalyticalGI || e_svoTI_AnalyticalOccluders) && m_texInfo.arrAnalyticalOccluders[0][0].radius)
		rtFlags |= g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP];

	if (bPixelShader && e_svoTI_AnalyticalOccluders && m_texInfo.arrAnalyticalOccluders[1][0].radius)
		rtFlags |= g_HWSR_MaskBit[HWSR_SPRITE];

	if (!bPixelShader && e_svoTI_SunRSMInject)
		rtFlags |= g_HWSR_MaskBit[HWSR_AMBIENT_OCCLUSION];

	#if !CRY_PLATFORM_CONSOLE
	if (bPixelShader && e_svoTI_RsmUseColors > 0)
		rtFlags |= g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG];
	#endif

	if (e_svoTI_ShadowsFromSun && bDiffuseMode && bPixelShader)
		rtFlags |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];

	if (e_svoTI_SpecularFromDiffuse && bDiffuseMode && bPixelShader)
		rtFlags |= g_HWSR_MaskBit[HWSR_PROJECTION_MULTI_RES];

	if (e_svoTI_ShadowsFromHeightmap && bPixelShader)
		rtFlags |= g_HWSR_MaskBit[HWSR_QUALITY1];

	return rtFlags;
}

int CSvoRenderer::GetIntegratioMode() const
{
	return e_svoTI_IntegrationMode;
}

int CSvoRenderer::GetIntegratioMode(bool& bSpecTracingInUse) const
{
	bSpecTracingInUse = (e_svoTI_IntegrationMode == 2) || (e_svoTI_SpecularFromDiffuse != 0);
	return e_svoTI_IntegrationMode;
}

bool CSvoRenderer::IsActive()
{
	return CSvoRenderer::s_pInstance && CSvoRenderer::s_pInstance->e_svoTI_Apply;
}

void CSvoRenderer::InitCVarValues()
{
	#define INIT_SVO_CVAR(_type, _var)                              \
	  if (strstr( # _type, "float"))                                \
	    _var = (_type)gEnv->pConsole->GetCVar( # _var)->GetFVal();  \
	  else                                                          \
	    _var = (_type)gEnv->pConsole->GetCVar( # _var)->GetIVal();  \

	INIT_ALL_SVO_CVARS;
	#undef INIT_SVO_CVAR
}

CTexture* CSvoRenderer::GetTroposphereMinRT()
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (IsActive() && e_svoTI_Troposphere_Active && m_pRT_AIR_MIN)
		return m_pRT_AIR_MIN;
	#endif
	return NULL;
}

CTexture* CSvoRenderer::GetTroposphereMaxRT()
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (IsActive() && e_svoTI_Troposphere_Active && m_pRT_AIR_MAX)
		return m_pRT_AIR_MAX;
	#endif
	return NULL;
}

CTexture* CSvoRenderer::GetTroposphereShadRT()
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (IsActive() && e_svoTI_Troposphere_Active && m_pRT_SHAD_MIN_MAX)
		return m_pRT_SHAD_MIN_MAX;
	#endif
	return NULL;
}

CTexture* CSvoRenderer::GetTracedSunShadowsRT()
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (IsActive() && e_svoTI_ShadowsFromSun && m_pRT_SHAD_FIN_0)
		return m_pRT_SHAD_FIN_0;
	#endif
	return NULL;
}

CTexture* CSvoRenderer::GetDiffuseFinRT()
{
	return m_tsDiff.pRT_FIN_OUT_0;
}

CTexture* CSvoRenderer::GetSpecularFinRT()
{
	return m_tsSpec.pRT_FIN_OUT_0;
}

void CSvoRenderer::UpscalePass(SSvoTargetsSet* pTS)
{
	CSvoFullscreenPass& rp = pTS->passUpscale;

	const char* szTechFinalName = "UpScalePass";

	if (!e_svoTI_Active || !e_svoTI_Apply || !e_svoRender || !m_pShader)
		return;

	rp.SetTechnique(m_pShader, szTechFinalName, GetRunTimeFlags(pTS == &m_tsDiff));
	rp.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
	rp.SetState(GS_NODEPTHTEST);

	rp.SetRenderTarget(0, pTS->pRT_FIN_OUT_0);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (pTS == &m_tsDiff && m_pRT_SHAD_FIN_0 && (e_svoTI_ShadowsFromSun || e_svoTI_SpecularFromDiffuse))
		rp.SetRenderTarget(1, m_pRT_SHAD_FIN_0);
	#endif

	// compute specular form results of diffuse tracing
	if (pTS == &m_tsDiff && e_svoTI_SpecularFromDiffuse)
		rp.SetRenderTarget(2, m_tsSpec.pRT_FIN_OUT_0);

	rp.SetRequireWorldPos(true);
	rp.SetRequirePerViewConstantBuffer(true);

	SetupGBufferTextures(rp);

	rp.SetTexture(10, pTS->pRT_ALD_DEM_MIN_0);
	rp.SetTexture(11, pTS->pRT_RGB_DEM_MIN_0);
	rp.SetTexture(12, pTS->pRT_ALD_DEM_MAX_0);
	rp.SetTexture(13, pTS->pRT_RGB_DEM_MAX_0);

	rp.SetTexture(9, pTS->pRT_FIN_OUT_1);

	if (pTS == &m_tsSpec && m_tsDiff.pRT_FIN_OUT_0)
	{
		rp.SetTexture(15, m_tsDiff.pRT_FIN_OUT_0);
	}
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	else if (pTS == &m_tsDiff && m_pRT_SHAD_MIN_MAX && e_svoTI_ShadowsFromSun)
	{
		rp.SetTexture(15, m_pRT_SHAD_MIN_MAX);
	}
	#endif
	else
	{
		rp.SetTexture(15, CRendererResources::s_ptexBlack);
	}

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (pTS == &m_tsDiff && m_pRT_SHAD_FIN_1 && e_svoTI_ShadowsFromSun)
		rp.SetTexture(16, m_pRT_SHAD_FIN_1);
	#endif

	rp.SetTexture(8, GetUtils().GetVelocityObjectRT(RenderView()));

	SetupCommonSamplers(rp);

	rp.BeginConstantUpdate();

	SetupCommonConstants(pTS, rp, pTS->pRT_ALD_0);

	rp.Execute();
}

template<class T>
void CSvoRenderer::SetupRsmSunTextures(T& rp)
{
	const int rsmDepthTexSlot = 29;
	const int rsmColorTexSlot = 30;
	const int rsmNormlTexSlot = 31;

	CTexture* pRsmDepthMap = CRendererResources::s_ptexBlack;
	CTexture* pRsmColorMap = CRendererResources::s_ptexBlack;
	CTexture* pRsmNormalMap = CRendererResources::s_ptexBlack;

	if (ShadowMapFrustum* pRsmFrustum = GetRsmSunFrustum(RenderView()))
	{
		assert(!pRsmFrustum->bUseShadowsPool);

		pRsmDepthMap = pRsmFrustum->pDepthTex;

		if (CTexture* pColorMap = GetRsmColorMap(*pRsmFrustum))
			pRsmColorMap = pColorMap;

		if (CTexture* pNormalMap = GetRsmNormlMap(*pRsmFrustum))
			pRsmNormalMap = pNormalMap;
	}

	rp.SetTexture(rsmDepthTexSlot, pRsmDepthMap);
	rp.SetTexture(rsmColorTexSlot, pRsmColorMap);
	rp.SetTexture(rsmNormlTexSlot, pRsmNormalMap);
}

template<class T>
void CSvoRenderer::SetupRsmSunConstants(T& rp)
{
	static CCryNameR lightProjParamName("SVO_RsmSunShadowProj");
	static CCryNameR rsmSunColparamName("SVO_RsmSunCol");
	static CCryNameR rsmSunDirparamName("SVO_RsmSunDir");
	Matrix44A shadowMat;
	shadowMat.SetIdentity();

	ShadowMapFrustum* pRsmFrustum = GetRsmSunFrustum(RenderView());

	const auto& viewInfo = RenderView()->GetViewInfo(CCamera::eEye_Left);

	if (pRsmFrustum && GetRsmColorMap(*pRsmFrustum))
	{
		CShadowUtils::SShadowsSetupInfo shadowsSetup = gcpRendD3D->ConfigShadowTexgen(RenderView(), pRsmFrustum, 0);

		assert(!pRsmFrustum->bUseShadowsPool);

		// set up shadow matrix
		shadowMat = shadowsSetup.ShadowMat;
		const Vec4 vEye(viewInfo.cameraOrigin, 0.f);
		Vec4 vecTranslation(vEye.Dot((Vec4&)shadowMat.m00), vEye.Dot((Vec4&)shadowMat.m10), vEye.Dot((Vec4&)shadowMat.m20), vEye.Dot((Vec4&)shadowMat.m30));
		shadowMat.m03 += vecTranslation.x;
		shadowMat.m13 += vecTranslation.y;
		shadowMat.m23 += vecTranslation.z;
		shadowMat.m33 += vecTranslation.w;
		(Vec4&)shadowMat.m20 *= shadowsSetup.RecpFarDist;
		rp.SetConstantArray(lightProjParamName, alias_cast<Vec4*>(&shadowMat), 4);

		Vec4 vData(gEnv->p3DEngine->GetSunColor(), e_svoTI_InjectionMultiplier);
		rp.SetConstantArray(rsmSunColparamName, (Vec4*)&vData, 1);
		Vec4 vData2(gEnv->p3DEngine->GetSunDirNormalized(), (float)e_svoTI_SunRSMInject);
		rp.SetConstantArray(rsmSunDirparamName, (Vec4*)&vData2, 1);
	}
	else
	{
		Vec4 vData(0, 0, 0, 0);
		rp.SetConstantArray(rsmSunColparamName, (Vec4*)&vData, 1);
		Vec4 vData2(0, 0, 0, 0);
		rp.SetConstantArray(rsmSunDirparamName, (Vec4*)&vData2, 1);
	}
}

ISvoRenderer* CD3D9Renderer::GetISvoRenderer()
{
	return CSvoRenderer::GetInstance(true);
}

template<class T>
void CSvoRenderer::BindTiledLights(PodArray<I3DEngine::SLightTI>& lightsTI, T& rp)
{
	auto* tiledLights = gcpRendD3D.GetGraphicsPipeline().GetTiledLightVolumesStage();

	rp.SetBuffer(16, tiledLights->GetLightShadeInfoBuffer());
	rp.SetTexture(19, tiledLights->GetProjectedLightAtlas());
	rp.SetTexture(20, CRendererResources::s_ptexRT_ShadowPool);

	CTexture* ptexRsmCol = CRendererResources::s_ptexBlack;
	CTexture* ptexRsmNor = CRendererResources::s_ptexBlack;
	if (CSvoRenderer::GetInstance()->IsActive() && CSvoRenderer::GetInstance()->GetSpecularFinRT())
	{
		if (CTexture* pTexPoolCol = CSvoRenderer::GetInstance()->GetRsmPoolCol())
		{
			if (CTexture::IsTextureExist(pTexPoolCol))
				ptexRsmCol = pTexPoolCol;
		}
		if (CTexture* pTexPoolNor = CSvoRenderer::GetInstance()->GetRsmPoolNor())
		{
			if (CTexture::IsTextureExist(pTexPoolNor))
				ptexRsmNor = pTexPoolNor;
		}
	}

	rp.SetTexture(26, ptexRsmCol);
	rp.SetTexture(27, ptexRsmNor);

	CTiledLightVolumesStage::STiledLightShadeInfo* tiledLightShadeInfo = tiledLights->GetTiledLightShadeInfo();

	const auto& viewInfo = RenderView()->GetViewInfo(CCamera::eEye_Left);

	for (int l = 0; l < lightsTI.Count(); l++)
	{
		I3DEngine::SLightTI& svoLight = lightsTI[l];

		if (!svoLight.vDirF.w)
			continue;

		Vec4 worldViewPos = Vec4(viewInfo.cameraOrigin, 0);

		for (uint32 lightIdx = 0; lightIdx <= 255 && tiledLightShadeInfo[lightIdx].posRad != Vec4(0, 0, 0, 0); ++lightIdx)
		{
			if ((tiledLightShadeInfo[lightIdx].lightType == CTiledLightVolumesStage::tlTypeRegularProjector) && svoLight.vPosR.IsEquivalent(tiledLightShadeInfo[lightIdx].posRad /*+ worldViewPos*/, .5f))
			{
				if (svoLight.vCol.w > 0)
					svoLight.vCol.w = ((float)lightIdx + 100);
				else
					svoLight.vCol.w = -((float)lightIdx + 100);
			}
		}
	}
}

ShadowMapFrustum* CSvoRenderer::GetRsmSunFrustum(const CRenderView* pRenderView) const
{
	for (const auto& pFrustumToRender : pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunDynamic))
	{
		if (pFrustumToRender->pFrustum->nShadowMapLod == e_svoTI_GsmCascadeLod)
		{
			return pFrustumToRender->pFrustum;
		}
	}

	return nullptr;
}

CTexture* CSvoRenderer::GetRsmColorMap(const ShadowMapFrustum& rFr, bool bCheckUpdate)
{
	if (IsActive() && (rFr.nShadowMapLod == CSvoRenderer::GetInstance()->e_svoTI_GsmCascadeLod) && CSvoRenderer::GetInstance()->e_svoTI_InjectionMultiplier && CSvoRenderer::GetInstance()->e_svoTI_RsmUseColors >= 0)
	{
		if (bCheckUpdate)
			CSvoRenderer::GetInstance()->CheckCreateUpdateRT(s_pRsmColorMap, rFr.nShadowMapSize, rFr.nShadowMapSize, eTF_R8G8B8A8, eTT_2D, FT_STATE_CLAMP, "SVO_SUN_RSM_COLOR");

		return CTexture::IsTextureExist(s_pRsmColorMap) ? s_pRsmColorMap.get() : nullptr;
	}

	if (IsActive() && rFr.bUseShadowsPool && CSvoRenderer::GetInstance()->e_svoTI_InjectionMultiplier && CSvoRenderer::GetInstance()->e_svoTI_RsmUseColors >= 0 && rFr.m_Flags & DLF_USE_FOR_SVOGI)
	{
		if (bCheckUpdate)
			CSvoRenderer::GetInstance()->CheckCreateUpdateRT(s_pRsmPoolCol, CRendererResources::s_ptexRT_ShadowPool->GetWidth(), CRendererResources::s_ptexRT_ShadowPool->GetHeight(), eTF_R8G8B8A8, eTT_2D, FT_STATE_CLAMP, "SVO_PRJ_RSM_COLOR");

		return CTexture::IsTextureExist(s_pRsmPoolCol) ? s_pRsmPoolCol.get() : nullptr;
	}

	return NULL;
}

CTexture* CSvoRenderer::GetRsmNormlMap(const ShadowMapFrustum& rFr, bool bCheckUpdate)
{
	if (IsActive() && (rFr.nShadowMapLod == CSvoRenderer::GetInstance()->e_svoTI_GsmCascadeLod) && CSvoRenderer::GetInstance()->e_svoTI_InjectionMultiplier && CSvoRenderer::GetInstance()->e_svoTI_RsmUseColors >= 0)
	{
		if (bCheckUpdate)
			CSvoRenderer::GetInstance()->CheckCreateUpdateRT(s_pRsmNormlMap, rFr.nShadowMapSize, rFr.nShadowMapSize, eTF_R8G8B8A8, eTT_2D, FT_STATE_CLAMP, "SVO_SUN_RSM_NORMAL");

		return CTexture::IsTextureExist(s_pRsmNormlMap) ? s_pRsmNormlMap.get() : nullptr;
	}

	if (IsActive() && rFr.bUseShadowsPool && CSvoRenderer::GetInstance()->e_svoTI_InjectionMultiplier && CSvoRenderer::GetInstance()->e_svoTI_RsmUseColors >= 0 && rFr.m_Flags & DLF_USE_FOR_SVOGI)
	{
		if (bCheckUpdate)
			CSvoRenderer::GetInstance()->CheckCreateUpdateRT(s_pRsmPoolNor, CRendererResources::s_ptexRT_ShadowPool->GetWidth(), CRendererResources::s_ptexRT_ShadowPool->GetHeight(), eTF_R8G8B8A8, eTT_2D, FT_STATE_CLAMP, "SVO_PRJ_RSM_NORMAL");

		return CTexture::IsTextureExist(s_pRsmPoolNor) ? s_pRsmPoolNor.get() : nullptr;
	}

	return NULL;
}

void CSvoRenderer::GetRsmTextures(_smart_ptr<CTexture>& pRsmColorMap, _smart_ptr<CTexture>& pRsmNormlMap, _smart_ptr<CTexture>& pRsmPoolCol, _smart_ptr<CTexture>& pRsmPoolNor)
{
	if (!s_pRsmColorMap)
		s_pRsmColorMap = CTexture::GetOrCreateTextureObjectPtr("SVO_SUN_RSM_COLOR", 0, 0, 1, eTT_2D, FT_STATE_CLAMP, eTF_R8G8B8A8);
	if (!s_pRsmNormlMap)
		s_pRsmNormlMap = CTexture::GetOrCreateTextureObjectPtr("SVO_SUN_RSM_NORMAL", 0, 0, 1, eTT_2D, FT_STATE_CLAMP, eTF_R8G8B8A8);
	if (!s_pRsmPoolCol)
		s_pRsmPoolCol = CTexture::GetOrCreateTextureObjectPtr("SVO_PRJ_RSM_COLOR", 0, 0, 1, eTT_2D, FT_STATE_CLAMP, eTF_R8G8B8A8);
	if (!s_pRsmPoolNor)
		s_pRsmPoolNor = CTexture::GetOrCreateTextureObjectPtr("SVO_PRJ_RSM_NORMAL", 0, 0, 1, eTT_2D, FT_STATE_CLAMP, eTF_R8G8B8A8);

	pRsmColorMap = s_pRsmColorMap;
	pRsmNormlMap = s_pRsmNormlMap;
	pRsmPoolCol = s_pRsmPoolCol;
	pRsmPoolNor = s_pRsmPoolNor;
}

void CSvoRenderer::CheckCreateUpdateRT(_smart_ptr<CTexture>& pTex, int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char* szName)
{
	if (!CTexture::IsTextureExist(pTex) || pTex->GetWidth() != nWidth || pTex->GetHeight() != nHeight || pTex->GetTextureDstFormat() != eTF)
	{
		const bool bNeedsDecRef = !CTexture::IsTextureExist(pTex); // NOTE: SD3DPostEffectsUtils::GetOrCreateRenderTarget adds ref when !CTexture::IsTextureExist only

		CTexture* pTexRaw = pTex;
		if (SD3DPostEffectsUtils::GetOrCreateRenderTarget(szName, pTexRaw, nWidth, nHeight, Clr_Unknown, 0, false, eTF))
		{
			pTex = pTexRaw;
			pTex->DisableMgpuSync();

			if (bNeedsDecRef)
				pTex->Release();
		}
	}
}

	#ifdef FEATURE_SVO_GI_ALLOW_HQ

void CSvoRenderer::SVoxPool::Init(ITexture* _pTex)
{
	pTex = (CTexture*)_pTex;

	if (pTex)
	{
		if (CSvoRenderer::s_pInstance && CSvoRenderer::s_pInstance->GetIntegratioMode())
		{
			if (pTex->GetFlags() & FT_USAGE_UAV_RWTEXTURE)
				pUAV = pTex->GetDevTexture()->LookupUAV(EDefaultResourceViews::UnorderedAccess);
			else
				pSRV = pTex->GetDevTexture()->LookupSRV(EDefaultResourceViews::Default);
		}

		nTexId = pTex->GetTextureID();
	}
}

	#endif

#endif
