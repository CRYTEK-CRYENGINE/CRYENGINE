// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Snow.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "Common/ReverseDepth.h"

//////////////////////////////////////////////////////////////////////////

namespace
{
struct SSnowClusterCB
{
	Vec4     vWaterLevel;
	Vec4     vSnowFlakeParams;
	Vec4     vSnowClusterPos;
	Vec4     vSnowClusterPosPrev;
	Matrix34 mSnowOccMatr;
};
}

CSnowStage::CSnowStage()
{

}

CSnowStage::~CSnowStage()
{
	if (m_snowFlakeVertexBuffer != ~0u)
	{
		gRenDev->m_DevBufMan.Destroy(m_snowFlakeVertexBuffer);
	}
}

void CSnowStage::Init()
{
	CRY_ASSERT(m_pSnowFlakesTex == nullptr);
	m_pSnowFlakesTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/snowflakes.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pSnowDerivativesTex == nullptr);
	m_pSnowDerivativesTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/perlinNoiseDerivatives.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pSnowSpatterTex == nullptr);
	m_pSnowSpatterTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/Frozen/snow_spatter.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pFrostBubblesBumpTex == nullptr);
	m_pFrostBubblesBumpTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/Frozen/frost_noise4.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pSnowFrostBumpTex == nullptr);
	m_pSnowFrostBumpTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/Frozen/frost_noise3.dds", FT_DONT_STREAM, eTF_Unknown);
}

void CSnowStage::Update()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const CRenderView* pRenderView = RenderView();

	if (pRenderView->GetCurrentEye() != CCamera::eEye_Right)
	{
		// TODO: remove implicit dependency to ExecuteRainPreprocess() due to SRainParams::matOccTransRender updated in ExecuteRainPreprocess().
		// TODO: m_RainInfo and m_SnowInfo need to be unique for each view-port if the engine supports multi view-port rendering.
		SRainParams& rainVolParams = rd->m_p3DEngineCommon.m_RainInfo;
		SSnowParams& snowVolParams = rd->m_p3DEngineCommon.m_SnowInfo;

		m_RainVolParams = rainVolParams;
		m_SnowVolParams = snowVolParams;
	}
}

void CSnowStage::ResizeResource(int resourceWidth, int resourceHeight)
{
	const uint32 flags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;

	m_pSnowDisplacementTex = CTexture::GetOrCreateTextureObjectPtr("$SnowDisplacement", resourceWidth, resourceHeight, 1, eTT_2D, flags, eTF_R8);
	if (m_pSnowDisplacementTex)
	{
		const bool shouldApplyDisplacement = (CRenderer::CV_r_snow > 0) && (CRenderer::CV_r_snow_displacement > 0);

		// Create/release the displacement texture on demand
		if (!shouldApplyDisplacement && CTexture::IsTextureExist(m_pSnowDisplacementTex))
			m_pSnowDisplacementTex->ReleaseDeviceTexture(false);
		else if (shouldApplyDisplacement && (m_pSnowDisplacementTex->Invalidate(resourceWidth, resourceHeight, eTF_R8G8B8A8) || !CTexture::IsTextureExist(m_pSnowDisplacementTex)))
			m_pSnowDisplacementTex->CreateRenderTarget(eTF_R8G8B8A8, Clr_Transparent);
	}
}

void CSnowStage::Resize(int renderWidth, int renderHeight)
{
#if defined(VOLUMETRIC_FOG_SHADOWS)
	ResizeResource(renderWidth, renderHeight);
#endif
}

void CSnowStage::OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater)
{
#if defined(VOLUMETRIC_FOG_SHADOWS)
	auto pVar1 = cvarUpdater.GetCVar("r_snow");
	auto pVar2 = cvarUpdater.GetCVar("r_snow_displacement");
	if (pVar1 || pVar2)
	{
		const CRenderView* pRenderView = RenderView();
		const int32 renderWidth  = pRenderView->GetRenderResolution()[0];
		const int32 renderHeight = pRenderView->GetRenderResolution()[1];

		ResizeResource(renderWidth, renderHeight);
	}
#endif
}

void CSnowStage::ExecuteDeferredSnowGBuffer()
{
	const SSnowParams& snowVolParams = m_SnowVolParams;
	const SRainParams& rainVolParams = m_RainVolParams;

	if ((CRenderer::CV_r_snow < 1)
	    || (snowVolParams.m_fSnowAmount < 0.05f
	        && snowVolParams.m_fFrostAmount < 0.05f
	        && snowVolParams.m_fSurfaceFreezing < 0.05f)
	    || snowVolParams.m_fRadius < 0.05f)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("DEFERRED_SNOW_GBUFFER");

	CTexture* CRendererResources__s_ptexSceneSpecular = CRendererResources::s_ptexSceneSpecular;
#if defined(DURANGO_USE_ESRAM)
	CRendererResources__s_ptexSceneSpecular = CRendererResources::s_ptexSceneSpecularESRAM;
#endif

	// TODO: Try avoiding the copy by directly accessing UAVs
	m_passCopyGBufferSpecular.Execute(CRendererResources::s_ptexSceneNormalsMap, CRendererResources::s_ptexSceneNormalsBent);
	m_passCopyGBufferDiffuse.Execute(CRendererResources__s_ptexSceneSpecular, CRendererResources::s_ptexSceneSpecularTmp);
	m_passCopyGBufferNormal.Execute(CRendererResources::s_ptexSceneDiffuse, CRendererResources::s_ptexSceneDiffuseTmp);

	if (CRenderer::CV_r_snow_displacement && CTexture::IsTextureExist(m_pSnowDisplacementTex))
	{
		CClearSurfacePass::Execute(m_pSnowDisplacementTex, Clr_Transparent);
	}

	auto* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);
	CRenderView& rv = *pRenderView;

	uint64 rtMask = 0;
	rtMask |= (rainVolParams.bApplyOcclusion) ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;

	CTexture* pOcclusionTex = (rainVolParams.bApplyOcclusion) ? CRendererResources::s_ptexRainOcclusion : CRendererResources::s_ptexBlack;
	CTexture* zTarget = pRenderView->GetDepthTarget();

	auto& pass = m_passDeferredSnowGBuffer;

	if (pass.IsDirty(rtMask, CRenderer::CV_r_snow_displacement, pOcclusionTex->GetID()))
	{
		static CCryNameTSCRC techName("Snow");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		pass.SetTechnique(CShaderMan::s_ShaderDeferredSnow, techName, rtMask);

		const int32 stencilState = STENC_FUNC(FSS_STENCFUNC_EQUAL) |
			STENCOP_FAIL(FSS_STENCOP_KEEP) |
			STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			STENCOP_PASS(FSS_STENCOP_KEEP);
		const uint8 stencilRef = BIT_STENCIL_RESERVED;
		const uint8 stencilReadMask = BIT_STENCIL_RESERVED;
		pass.SetStencilState(stencilState, stencilRef, stencilReadMask, 0xFF);
		pass.SetState(GS_NODEPTHTEST | GS_STENCIL);

		pass.SetRenderTarget(0, CRendererResources::s_ptexSceneNormalsMap);
		pass.SetRenderTarget(1, CRendererResources::s_ptexSceneDiffuse);
		pass.SetRenderTarget(2, CRendererResources__s_ptexSceneSpecular);
		pass.SetDepthTarget(zTarget);

		if (CRenderer::CV_r_snow_displacement)
		{
			pass.SetRenderTarget(3, m_pSnowDisplacementTex);
		}
		else
		{
			pass.SetRenderTarget(3, nullptr);
		}

		pass.SetTexture(0, CRendererResources::s_ptexSceneDiffuseTmp);
		pass.SetTexture(1, CRendererResources::s_ptexSceneNormalsBent);
		pass.SetTexture(2, CRendererResources::s_ptexSceneSpecularTmp);
		pass.SetTexture(3, CRendererResources::s_ptexLinearDepth);
		pass.SetTexture(4, m_pSnowDerivativesTex);
		pass.SetTexture(5, m_pSnowSpatterTex);
		pass.SetTexture(6, m_pFrostBubblesBumpTex);
		pass.SetTexture(7, m_pSnowFrostBumpTex);
		pass.SetTexture(8, CRendererResources::s_ptexNoise3D);
		pass.SetTexture(9, pOcclusionTex);

		pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
		pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);
		pass.SetSampler(2, EDefaultSamplerStates::PointBorder_White);
		pass.SetSampler(3, EDefaultSamplerStates::BilinearWrap);

		// Those texture and sampler are used in EncodeGBuffer().
		pass.SetTexture(30, CRendererResources::s_ptexNormalsFitting);
		pass.SetSampler(9, EDefaultSamplerStates::PointClamp);

		pass.SetRequirePerViewConstantBuffer(true);
	}

	int32 sX;
	int32 sY;
	int32 sWidth;
	int32 sHeight;
	GetScissorRegion(GetCurrentViewInfo().cameraOrigin, snowVolParams.m_vWorldPos, snowVolParams.m_fRadius, sX, sY, sWidth, sHeight);

	D3D11_RECT scissor;
	scissor.left = sX;
	scissor.top = sY;
	scissor.right = sX + sWidth;
	scissor.bottom = sY + sHeight;

	pass.SetScissor(true, scissor);

	pass.BeginConstantUpdate();

	static CCryNameR paramName("g_SnowVolumeParams");
	const Vec4 vSnowPosCS = Vec4(snowVolParams.m_vWorldPos, 1.0f / max(snowVolParams.m_fRadius, 1e-3f));
	pass.SetConstant(paramName, vSnowPosCS);

	static CCryNameR param1Name("g_SnowMultipliers");
	const float fSnowAmount = snowVolParams.m_fSnowAmount;
	const float fFrostAmount = snowVolParams.m_fFrostAmount;
	const float fSurfaceFreezing = snowVolParams.m_fSurfaceFreezing;
	const Vec4 vSnowMultipliers(fSnowAmount, fFrostAmount, clamp_tpl(fSurfaceFreezing, 0.0f, 1.0f), 0);
	pass.SetConstant(param1Name, vSnowMultipliers);

	// Sample wind at camera position
	const AABB box(GetCurrentViewInfo().cameraOrigin);
	const Vec3 windVec = gEnv->p3DEngine->GetWind(box, false);

	Vec3 windVecOcc = gEnv->p3DEngine->GetGlobalWind(false);

	if (rainVolParams.bApplyOcclusion)
	{
		static CCryNameR param3Name("g_SnowOcc_TransMat");
		pass.SetConstantArray(param3Name, (Vec4*)rainVolParams.matOccTransRender.GetData(), 3);

		// Pre-calculate wind-driven occlusion sample offset
		const float windOffsetScale = 15.0f / (float)RAIN_OCC_MAP_SIZE;
		windVecOcc = rainVolParams.matOccTransRender.TransformVector(windVec);
		windVecOcc.x *= windOffsetScale;
		windVecOcc.y *= windOffsetScale;

		static CCryNameR param4Name("g_SnowOcc_WindOffs");
		const Vec4 pWindParamsOcc(windVecOcc.x, windVecOcc.y, 0, 0);
		pass.SetConstant(param4Name, pWindParamsOcc);
	}

	static CCryNameR param2Name("g_WindDirection");
	const Vec4 pWindParams(windVec.x, windVec.y, windVecOcc.x, windVecOcc.y);
	pass.SetConstant(param2Name, pWindParams);

	pass.Execute();
}

void CSnowStage::ExecuteDeferredSnowDisplacement()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	const SSnowParams& snowVolParams = m_SnowVolParams;
	const SRainParams& rainVolParams = m_RainVolParams;

	if ((CRenderer::CV_r_snow < 1 || CRenderer::CV_r_snow_displacement < 1)
	    || snowVolParams.m_fSnowAmount < 0.05f
	    || snowVolParams.m_fRadius < 0.05f)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("DEFERRED_SNOW_DISPLACEMENT");

	static CCryNameR param5Name("g_CameraMatrix");
	Matrix44A matView = GetCurrentViewInfo().viewMatrix;

	// Adjust the camera matrix so that the camera space will be: +y = down, +z - towards, +x - right
	Vec3 zAxis = matView.GetRow(1);
	matView.SetRow(1, -matView.GetRow(2));
	matView.SetRow(2, zAxis);
	float z = matView.m13;
	matView.m13 = -matView.m23;
	matView.m23 = z;

	auto* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);
	CRenderView& rv = *pRenderView;

	int32 sX;
	int32 sY;
	int32 sWidth;
	int32 sHeight;
	GetScissorRegion(GetCurrentViewInfo().cameraOrigin, snowVolParams.m_vWorldPos, snowVolParams.m_fRadius, sX, sY, sWidth, sHeight);

	D3D11_RECT scissor;
	scissor.left = sX;
	scissor.top = sY;
	scissor.right = sX + sWidth;
	scissor.bottom = sY + sHeight;

	{
		PROFILE_LABEL_SCOPE("GENERATE_HEIGHT_MAP");

		auto& pass = m_passParallaxSnowHeightMapGen;

		if (pass.IsDirty())
		{
			static CCryNameTSCRC techName = "ParallaxMapPrepass";
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_ShaderDeferredSnow, techName, 0);

			pass.SetState(GS_NODEPTHTEST);

			pass.SetRenderTarget(0, CRendererResources::s_ptexBackBuffer);

			pass.SetTexture(0, m_pSnowDisplacementTex);
			pass.SetTexture(1, CRendererResources::s_ptexLinearDepth);
			pass.SetTexture(2, CRendererResources::s_ptexSceneNormalsMap);

			pass.SetSampler(0, EDefaultSamplerStates::PointClamp);

			pass.SetRequirePerViewConstantBuffer(true);
		}

		pass.SetScissor(true, scissor);

		pass.BeginConstantUpdate();

		pass.SetConstantArray(param5Name, (Vec4*)matView.GetData(), 3);

		pass.Execute();
	}

	{
		auto& pass = m_passParallaxSnowMin;

		if (pass.IsDirty())
		{
			static CCryNameTSCRC techName = "ParallaxMapMin";
			pass.SetTechnique(CShaderMan::s_ShaderDeferredSnow, techName, 0);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);

			pass.SetState(GS_NODEPTHTEST);

			pass.SetRenderTarget(0, CRendererResources::s_ptexSceneDiffuseTmp);

			pass.SetTexture(0, CRendererResources::s_ptexBackBuffer);
			pass.SetTexture(1, CRendererResources::s_ptexLinearDepth);

			pass.SetSampler(0, EDefaultSamplerStates::PointClamp);

			pass.SetRequirePerViewConstantBuffer(true);
		}

		pass.SetScissor(true, scissor);

		pass.BeginConstantUpdate();

		pass.Execute();
	}

	// Iteratively apply displacement to maximize quality and minimize sample count.
	{
		PROFILE_LABEL_SCOPE("APPLY_DISPLACEMENT");

		m_passCopySceneToParallaxSnowSrc.Execute(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexSceneTarget);

		static CCryNameTSCRC techName = "ParallaxMapApply";
		static CCryNameR paramsName("g_DisplacementParams");
		Vec4 passParams(0.0f);

		// First pass.
		{
			auto& pass = m_passParallaxSnow[0];

			if (pass.IsDirty())
			{
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass.SetTechnique(CShaderMan::s_ShaderDeferredSnow, techName, 0);

				pass.SetState(GS_NODEPTHTEST);

				pass.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);
				pass.SetRenderTarget(1, CRendererResources::s_ptexSceneSpecularTmp);

				pass.SetTexture(0, CRendererResources::s_ptexSceneTarget);
				pass.SetTexture(1, CRendererResources::s_ptexSceneDiffuseTmp);

				pass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

				pass.SetRequirePerViewConstantBuffer(true);
			}

			pass.SetScissor(true, scissor);

			pass.BeginConstantUpdate();

			passParams.x = 1.0f;
			pass.SetConstant(paramsName, passParams);

			pass.Execute();
		}

		// Second pass.
		{
			auto& pass = m_passParallaxSnow[1];

			if (pass.IsDirty())
			{
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass.SetTechnique(CShaderMan::s_ShaderDeferredSnow, techName, 0);

				pass.SetState(GS_NODEPTHTEST);

				pass.SetRenderTarget(0, CRendererResources::s_ptexSceneTarget);
				pass.SetRenderTarget(1, CRendererResources::s_ptexSceneDiffuseTmp);

				pass.SetTexture(0, CRendererResources::s_ptexHDRTarget);
				pass.SetTexture(1, CRendererResources::s_ptexSceneSpecularTmp);

				pass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

				pass.SetRequirePerViewConstantBuffer(true);
			}

			pass.SetScissor(true, scissor);

			pass.BeginConstantUpdate();

			passParams.x = 0.5f;
			pass.SetConstant(paramsName, passParams);

			pass.Execute();
		}

		// Third pass.
		{
			auto& pass = m_passParallaxSnow[2];

			if (pass.IsDirty())
			{
				uint64 rtMask = g_HWSR_MaskBit[HWSR_SAMPLE0];
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass.SetTechnique(CShaderMan::s_ShaderDeferredSnow, techName, rtMask);

				pass.SetState(GS_NODEPTHTEST);

				pass.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);
				pass.SetRenderTarget(1, CRendererResources::s_ptexLinearDepth);

				pass.SetTexture(0, CRendererResources::s_ptexSceneTarget);
				pass.SetTexture(1, CRendererResources::s_ptexSceneDiffuseTmp);

				pass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

				pass.SetRequirePerViewConstantBuffer(true);
			}

			pass.SetScissor(true, scissor);

			pass.BeginConstantUpdate();

			passParams.x = 0.25f;
			pass.SetConstant(paramsName, passParams);

			pass.Execute();
		}
	}
}

void CSnowStage::Execute()
{
	if ((CRenderer::CV_r_snow < 1) || !CRenderer::CV_r_PostProcess || !CRendererResources::s_ptexBackBuffer || !CRendererResources::s_ptexSceneTarget)
	{
		return;
	}

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const SRainParams& rainVolParams = m_RainVolParams;

	if (rd->m_bDeferredRainOcclusionEnabled)
	{
		if (rainVolParams.areaAABB.IsReset())
		{
			return;
		}

		if (!CTexture::IsTextureExist(CRendererResources::s_ptexRainOcclusion))
		{
			return;
		}
	}

	const SSnowParams snowVolParams = m_SnowVolParams;
	const bool bActive = (rd->m_bDeferredSnowEnabled && snowVolParams.m_fSnowFallBrightness > 0.005f && snowVolParams.m_nSnowFlakeCount > 0)
	                     || m_nAliveClusters;

	if (!bActive)
	{
		return;
	}

	if (!GenerateSnowClusterVertex())
	{
		return;
	}

	if (m_nFlakesPerCluster < 1)
	{
		return;
	}

	if (!PostProcessUtils().m_pTimer)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("SCENE_SNOW_FLAKES");

	// copy scene target texture
	m_passCopySceneTargetTexture.Execute(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexSceneTarget);

	CreateSnowClusters();
	UpdateSnowClusters();
	RenderSnowClusters();

	if (CRenderer::CV_r_snow_halfres)
	{
		ExecuteHalfResComposite();
	}
}

bool CSnowStage::GenerateSnowClusterVertex()
{
	const SSnowParams snowVolParams = m_SnowVolParams;

	int32 iRTWidth  = CRendererResources::s_renderWidth;
	int32 iRTHeight = CRendererResources::s_renderHeight;
	float fAspect = float(iRTWidth) / iRTHeight;

	// Vertex offsets for sprite expansion.
	static const Vec2 vVertOffsets[6] =
	{
		Vec2(1,  1),
		Vec2(-1, 1),
		Vec2(1,  -1),
		Vec2(1,  -1),
		Vec2(-1, 1),
		Vec2(-1, -1)
	};

	// Create vertex buffer if there isn't one or if the count has changed.
	if (m_snowFlakeVertexBuffer == ~0u
	    || ((m_nFlakesPerCluster != snowVolParams.m_nSnowFlakeCount) && 0 < snowVolParams.m_nSnowFlakeCount))
	{
		// release vertex buffer.
		if (m_snowFlakeVertexBuffer != ~0u)
		{
			gRenDev->m_DevBufMan.Destroy(m_snowFlakeVertexBuffer);
		}
		m_snowFlakeVertexBuffer = ~0u;

		m_nFlakesPerCluster = snowVolParams.m_nSnowFlakeCount;
		m_nSnowFlakeVertCount = m_nFlakesPerCluster * 6;

		std::unique_ptr<SVF_P3F_T2F_T3F[]> pSnowFlakes(new SVF_P3F_T2F_T3F[m_nSnowFlakeVertCount]);

		float fUserSize = 0.0075f;
		float fUserSizeVar = 0.0025f;

		// Loop through number of flake sprites (6 verts per sprite).
		for (int32 p = 0; p < m_nSnowFlakeVertCount; p += 6)
		{
			Vec3 vPosition = Vec3(cry_random(-10.0f, 10.0f), cry_random(-10.0f, 10.0f), cry_random(-10.0f, 10.0f));
			float fSize = fUserSize + fUserSizeVar * cry_random(-1.0f, 1.0f);
			float fRandPhase = cry_random(0.0f, 10.0f);

			// Triangle strip, each vertex gets it's own offset for sprite expansion.
			for (int32 i = 0; i < 6; ++i)
			{
				int32 nIdx = p + i;
				pSnowFlakes[nIdx].p = vPosition;
				pSnowFlakes[nIdx].st0 = Vec2(vVertOffsets[i].x, vVertOffsets[i].y);
				pSnowFlakes[nIdx].st1 = Vec3(fSize, fSize * fAspect, fRandPhase);
			}
		}

		const auto bufferSize = m_nSnowFlakeVertCount * sizeof(SVF_P3F_T2F_T3F);

		m_snowFlakeVertexBuffer = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, bufferSize);
		CRY_ASSERT(m_snowFlakeVertexBuffer != ~0u);

		gRenDev->m_DevBufMan.UpdateBuffer(m_snowFlakeVertexBuffer, pSnowFlakes.get(), bufferSize);
	}

	return (m_snowFlakeVertexBuffer != ~0u);
}

void CSnowStage::CreateSnowClusters()
{
	auto numClusters = max<int32>(1, CRenderer::CV_r_snowFlakeClusters);
	if (numClusters != m_nNumClusters)
	{
		CRY_ASSERT(m_nNumClusters == m_snowClusterArray.size());

		m_snowClusterArray.resize(numClusters);

		// create and initialize render primitives.
		for (int32 index = m_nNumClusters; index < numClusters; ++index)
		{
			auto& cluster = m_snowClusterArray[index];

			cluster.m_pPrimitive = stl::make_unique<CRenderPrimitive>();

			cluster.m_pPrimitive->AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(SSnowClusterCB), EShaderStage_Vertex | EShaderStage_Pixel);
		}

		m_nNumClusters = numClusters;
	}
}

void CSnowStage::UpdateSnowClusters()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const SSnowParams snowVolParams = m_SnowVolParams;

	bool bAllowSpawn(rd->m_bDeferredSnowEnabled && snowVolParams.m_fSnowFallBrightness > 0.005f);

	const float fCurrFrameTime = gEnv->pTimer->GetFrameTime();
	const float fGravity = fCurrFrameTime * snowVolParams.m_fSnowFallGravityScale;
	const float fWind = snowVolParams.m_fSnowFallWindScale;

	m_nAliveClusters = 0;

	const Vec3 vCameraPos = GetCurrentViewInfo().cameraOrigin;

	for (auto& cluster : m_snowClusterArray)
	{
		float fCurrLifeTime = cluster.m_fLifeTime > 0.0f ? (PostProcessUtils().m_pTimer->GetCurrTime() - cluster.m_fSpawnTime) / cluster.m_fLifeTime : 0.0f;
		float fClusterDist = cluster.m_pPos.GetDistance(vCameraPos);

		// Cluster is too small, died or is out of range, respawn.
		if (fabs(fCurrLifeTime) > 1.0f || cluster.m_fLifeTime < 0 || fClusterDist > 30.0f)
		{
			if (bAllowSpawn)
			{
				static SSnowCluster pNewDrop;

				cluster.m_pPos = vCameraPos;

				cluster.m_pPos.x += cry_random(-1.0f, 1.0f) * 15.0f;
				cluster.m_pPos.y += cry_random(-1.0f, 1.0f) * 15.0f;
				cluster.m_pPos.z += cry_random(-1.0f, 1.0f) * 5.0f + 4.0f;

				cluster.m_pPosPrev = cluster.m_pPos;
				cluster.m_fLifeTime = (pNewDrop.m_fLifeTime / std::max(1.0f, m_SnowVolParams.m_fSnowFallGravityScale) + pNewDrop.m_fLifeTimeVar * cry_random(-1.0f, 1.0f));

				// Increase/decrease weight randomly
				cluster.m_fWeight = clamp_tpl<float>(pNewDrop.m_fWeight + pNewDrop.m_fWeightVar * cry_random(-1.0f, 1.0f), 0.1f, 1.0f);

				cluster.m_fSpawnTime = PostProcessUtils().m_pTimer->GetCurrTime();
			}
			else
			{
				cluster.m_fLifeTime = -1.0f;
				continue;
			}
		}

		// Previous position (for blur).
		cluster.m_pPosPrev = cluster.m_pPos;

		// Apply gravity force.
		if (snowVolParams.m_fSnowFallGravityScale)
		{
			Vec3 vGravity(0.0f, 0.0f, -9.8f);
			if (gEnv->pPhysicalWorld)
			{
				pe_params_buoyancy pb;
				gEnv->pPhysicalWorld->CheckAreas(cluster.m_pPos, vGravity, &pb);
			}
			cluster.m_pPos += vGravity * fGravity * cluster.m_fWeight;
		}

		// Apply wind force.
		if (snowVolParams.m_fSnowFallWindScale)
		{
			Vec3 vWindVec = gEnv->p3DEngine->GetWind(AABB(cluster.m_pPos - Vec3(10, 10, 10), cluster.m_pPos + Vec3(10, 10, 10)), false);
			cluster.m_pPos += vWindVec * cluster.m_fWeight * fWind;
		}

		// Increment count.
		m_nAliveClusters++;
	}
}

void CSnowStage::RenderSnowClusters()
{
	SRenderViewShaderConstants& PF = RenderView()->GetShaderConstants();

	CTexture* pSceneSrc = CRendererResources::s_ptexHDRTarget;
	CTexture* pVelocitySrc = CRendererResources::s_ptexVelocity;
	if (CRenderer::CV_r_snow_halfres)
	{
		pSceneSrc = CRendererResources::s_ptexHDRTargetScaledTmp[0];
		pVelocitySrc = CRendererResources::s_ptexBackBufferScaled[0];

		// Clear the buffers
		CClearSurfacePass::Execute(pSceneSrc, Clr_Transparent);
		CClearSurfacePass::Execute(pVelocitySrc, Clr_Static);
	}

	const SRainParams& rainVolParams = m_RainVolParams;
	const SSnowParams snowVolParams = m_SnowVolParams;

	auto& pass = m_passSnow;

	D3DViewPort viewport;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(pSceneSrc->GetWidth());
	viewport.Height = static_cast<float>(pSceneSrc->GetHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// Render to HDR and velocity.
	pass.SetRenderTarget(0, pSceneSrc);
	pass.SetRenderTarget(1, pVelocitySrc);
	pass.SetDepthTarget(CRenderer::CV_r_snow_halfres ? nullptr : RenderView()->GetDepthTarget());
	pass.SetViewport(viewport);
	pass.BeginAddingPrimitives();

	auto pPerViewCB = GetStdGraphicsPipeline().GetMainViewConstantBuffer();

	CTexture* pOcclusionTex = (rainVolParams.bApplyOcclusion) ? CRendererResources::s_ptexRainOcclusion : CRendererResources::s_ptexBlack;

	uint64 rtMask = 0;
	if (rainVolParams.bApplyOcclusion)
	{
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	}

	uint32 renderState = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_DEPTHFUNC_LEQUAL;
	const bool bReverseDepth = true;
	if (bReverseDepth)
	{
		renderState = ReverseDepthHelper::ConvertDepthFunc(renderState);
	}

	static CCryNameTSCRC tech0Name("SceneSnow");
	const Vec4 vSnowFlakeParams = Vec4(snowVolParams.m_fSnowFallBrightness, min(10.0f, snowVolParams.m_fSnowFlakeSize), snowVolParams.m_fSnowFallTurbulence, snowVolParams.m_fSnowFallTurbulenceFreq);
	const Vec4 vWaterLevel = Vec4(PF.vWaterLevel, 0.0f);

	for (auto& cluster : m_snowClusterArray)
	{
		if (cluster.m_fLifeTime < 0.0f)
		{
			continue;
		}

		// Don't render if indoors or under water.
		if (gEnv->p3DEngine->GetVisAreaFromPos(cluster.m_pPos)
		    || cluster.m_pPos.z < gEnv->p3DEngine->GetWaterLevel(&(cluster.m_pPos)))
		{
			continue;
		}

		CRenderPrimitive* pPrim = cluster.m_pPrimitive.get();

		pPrim->SetCustomVertexStream(m_snowFlakeVertexBuffer, EDefaultInputLayouts::P3F_T2F_T3F, sizeof(SVF_P3F_T2F_T3F));
		pPrim->SetCustomIndexStream(~0u, RenderIndexType(0));
		pPrim->SetDrawInfo(eptTriangleList, 0, 0, m_nSnowFlakeVertCount);

		pPrim->SetTechnique(CShaderMan::s_shPostEffectsGame, tech0Name, rtMask);
		pPrim->SetCullMode(eCULL_None);
		pPrim->SetRenderState(renderState);

		pPrim->SetTexture(0, CRendererResources::s_ptexSceneTarget);
		pPrim->SetTexture(1, CRendererResources::s_ptexLinearDepth);
		pPrim->SetTexture(2, pOcclusionTex);
		pPrim->SetTexture(3, m_pSnowFlakesTex);

		pPrim->SetSampler(0, EDefaultSamplerStates::PointClamp);
		pPrim->SetSampler(1, EDefaultSamplerStates::TrilinearWrap);

		pPrim->SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB.get(), EShaderStage_Vertex | EShaderStage_Pixel);
		pPrim->Compile(pass);

		// update constant buffer
		{

			auto constants = pPrim->GetConstantManager().BeginTypedConstantUpdate<SSnowClusterCB>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

			constants->vWaterLevel = vWaterLevel;
			constants->vSnowFlakeParams = vSnowFlakeParams;
			constants->vSnowClusterPos = Vec4(cluster.m_pPos, 1.0f);
			constants->vSnowClusterPosPrev = Vec4(cluster.m_pPosPrev, 1.0f);
			if (m_RainVolParams.bApplyOcclusion)
			{
				constants->mSnowOccMatr = Matrix34(m_RainVolParams.matOccTransRender);
			}

			pPrim->GetConstantManager().EndTypedConstantUpdate(constants);
		}

		pass.AddPrimitive(pPrim);
	}

	pass.Execute();
}

void CSnowStage::ExecuteHalfResComposite()
{
	PROFILE_LABEL_SCOPE("SCENE_SNOW_FLAKES_HALFRES_COMPOSITE");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	auto& pass = m_passSnowHalfResCompisite;

	if (pass.IsDirty())
	{
		static CCryNameTSCRC techName = "SnowHalfResComposite";
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, 0);
		pass.SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

		pass.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);
		pass.SetRenderTarget(1, CRendererResources::s_ptexVelocity);

		pass.SetTexture(0, CRendererResources::s_ptexHDRTargetScaledTmp[0]);
		pass.SetTexture(1, CRendererResources::s_ptexBackBufferScaled[0]);

		pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
		pass.SetSampler(1, EDefaultSamplerStates::PointClamp);

		pass.SetRequirePerViewConstantBuffer(true);
	}

	pass.BeginConstantUpdate();

	pass.Execute();
}

void CSnowStage::GetScissorRegion(const Vec3& cameraOrigin, const Vec3& vCenter, float fRadius, int32& sX, int32& sY, int32& sWidth, int32& sHeight) const
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	Vec3 vViewVec = vCenter - cameraOrigin;
	float fDistToLS = vViewVec.GetLength();

	bool bInsideLightVolume = fDistToLS <= fRadius;
	if (bInsideLightVolume)
	{
		sX = sY = 0;
		sWidth  = CRendererResources::s_renderWidth;
		sHeight = CRendererResources::s_renderHeight;
		return;
	}

	const auto& viewInfo = GetStdGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left);
	Matrix44 mView = viewInfo.viewMatrix;
	Matrix44 mProj = viewInfo.projMatrix;

	Vec3 pBRectVertices[4];
	Vec4 vCenterVS = Vec4(vCenter, 1) * mView;

	{
		// Compute tangent planes
		float r = fRadius;
		float sq_r = r * r;

		Vec3 vLPosVS = Vec3(vCenterVS.x, vCenterVS.y, vCenterVS.z);
		float lx = vLPosVS.x;
		float ly = vLPosVS.y;
		float lz = vLPosVS.z;
		float sq_lx = lx * lx;
		float sq_ly = ly * ly;
		float sq_lz = lz * lz;

		// Compute left and right tangent planes to light sphere
		float sqrt_d = sqrt_tpl(max(sq_r * sq_lx - (sq_lx + sq_lz) * (sq_r - sq_lz), 0.0f));
		float nx = (r * lx + sqrt_d) / (sq_lx + sq_lz);
		float nz = iszero(lz) ? 1.0f : (r - nx * lx) / lz;

		Vec3 vTanLeft = Vec3(nx, 0, nz).normalized();

		nx = (r * lx - sqrt_d) / (sq_lx + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - nx * lx) / lz;
		Vec3 vTanRight = Vec3(nx, 0, nz).normalized();

		pBRectVertices[0] = vLPosVS - r * vTanLeft;
		pBRectVertices[1] = vLPosVS - r * vTanRight;

		// Compute top and bottom tangent planes to light sphere
		sqrt_d = sqrt_tpl(max(sq_r * sq_ly - (sq_ly + sq_lz) * (sq_r - sq_lz), 0.0f));
		float ny = (r * ly - sqrt_d) / (sq_ly + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - ny * ly) / lz;
		Vec3 vTanBottom = Vec3(0, ny, nz).normalized();

		ny = (r * ly + sqrt_d) / (sq_ly + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - ny * ly) / lz;
		Vec3 vTanTop = Vec3(0, ny, nz).normalized();

		pBRectVertices[2] = vLPosVS - r * vTanTop;
		pBRectVertices[3] = vLPosVS - r * vTanBottom;
	}

	Vec2 vPMin = Vec2(1, 1);
	Vec2 vPMax = Vec2(0, 0);
	Vec2 vMin = Vec2(1, 1);
	Vec2 vMax = Vec2(0, 0);

	// Project all vertices
	for (int i = 0; i < 4; i++)
	{
		Vec4 vScreenPoint = Vec4(pBRectVertices[i], 1.0) * mProj;

		//projection space clamping
		vScreenPoint.w = max(vScreenPoint.w, 0.00000000000001f);
		vScreenPoint.x = max(vScreenPoint.x, -(vScreenPoint.w));
		vScreenPoint.x = min(vScreenPoint.x, vScreenPoint.w);
		vScreenPoint.y = max(vScreenPoint.y, -(vScreenPoint.w));
		vScreenPoint.y = min(vScreenPoint.y, vScreenPoint.w);

		//NDC
		vScreenPoint /= vScreenPoint.w;

		//output coords
		//generate viewport (x=0,y=0,height=1,width=1)
		Vec2 vWin;
		vWin.x = (1.0f + vScreenPoint.x) * 0.5f;
		vWin.y = (1.0f + vScreenPoint.y) * 0.5f;   //flip coords for y axis

		assert(vWin.x >= 0.0f && vWin.x <= 1.0f);
		assert(vWin.y >= 0.0f && vWin.y <= 1.0f);

		// Get light sphere screen bounds
		vMin.x = min(vMin.x, vWin.x);
		vMin.y = min(vMin.y, vWin.y);
		vMax.x = max(vMax.x, vWin.x);
		vMax.y = max(vMax.y, vWin.y);
	}

	float fWidth = (float)CRendererResources::s_renderWidth;
	float fHeight = (float)CRendererResources::s_renderHeight;

	sX = (int32)(vMin.x * fWidth);
	sY = (int32)((1.0f - vMax.y) * fHeight);
	sWidth = (int32)ceilf((vMax.x - vMin.x) * fWidth);
	sHeight = (int32)ceilf((vMax.y - vMin.y) * fHeight);
}
