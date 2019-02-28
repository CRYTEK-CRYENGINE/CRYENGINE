// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Rain.h"

#include "DriverD3D.h"

//////////////////////////////////////////////////////////////////////////

namespace
{
struct SRainOccluderCB
{
	Matrix44 matRainOccluder;
};

struct SSceneRainCB
{
	Matrix44 sceneRainMtx;
	Vec4     unscaledFactor;
	Vec4     sceneRainParams0;
	Vec4     sceneRainParams1;
};
}

CRainStage::CRainStage()
{
	std::fill(std::begin(m_pRainRippleTex), std::end(m_pRainRippleTex), nullptr);
}

CRainStage::~CRainStage()
{
}

void CRainStage::Init()
{
	CRY_ASSERT(m_pSurfaceFlowTex == nullptr);
	m_pSurfaceFlowTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/Rain/surface_flow_ddn.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pRainSpatterTex == nullptr);
	m_pRainSpatterTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/Rain/rain_spatter.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pPuddleMaskTex == nullptr);
	m_pPuddleMaskTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/Rain/puddle_mask.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pHighFreqNoiseTex == nullptr);
	m_pHighFreqNoiseTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/JumpNoiseHighFrequency_x27y19.dds", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pRainfallTex == nullptr);
	m_pRainfallTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/Rain/rainfall.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pRainfallNormalTex == nullptr);
	m_pRainfallNormalTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/Rain/rainfall_ddn.tif", FT_DONT_STREAM, eTF_Unknown);

	const string fileNamePath = "%ENGINE%/EngineAssets/Textures/Rain/Ripple/ripple";
	const string fileNameExt = "_ddn.tif";
	uint32 index = 1;
	for (auto& pTex : m_pRainRippleTex)
	{
		CRY_ASSERT(pTex == nullptr);
		string fileName = fileNamePath + string().Format("%d", index) + fileNameExt;
		pTex = CTexture::ForNamePtr(fileName.c_str(), FT_DONT_STREAM, eTF_Unknown);
		++index;
	}

	{
		const int32 vertexStride = sizeof(SVF_P3F_C4B_T2F);
		const int32 totalVertexCount = (2 * (m_slices + 1)) + (2 * (m_slices + 1)) + (2 * (m_slices + 1));

		m_rainVertexBuffer = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, totalVertexCount * vertexStride);
		CRY_ASSERT(m_rainVertexBuffer != ~0u);

		for (auto& prim : m_rainPrimitives)
		{
			prim.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants);
			prim.SetCullMode(eCULL_None);
			prim.SetCustomVertexStream(m_rainVertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, vertexStride);
			prim.SetCustomIndexStream(~0u, RenderIndexType(0));
			prim.SetDrawInfo(eptTriangleStrip, 0, 0, totalVertexCount);
			prim.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

			prim.AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(SSceneRainCB), EShaderStage_Vertex | EShaderStage_Pixel);
		}

		// update vertex buffer
		{
			const int32 nSlices = m_slices;

			const float nSliceStep(DEG2RAD(360.0f / (float)nSlices));

			std::vector<SVF_P3F_C4B_T2F> pVB;
			SVF_P3F_C4B_T2F vVertex;
			vVertex.color.dcolor = ~0;
			vVertex.st = Vec2(0.0f, 0.0f);

			// Generate top cone vertices
			for (int32 h = 0; h < nSlices + 1; ++h)
			{
				float x = cosf(((float)h) * nSliceStep);
				float y = sinf(((float)h) * nSliceStep);

				vVertex.xyz = Vec3(x * 0.01f, y * 0.01f, 1);
				pVB.push_back(vVertex);

				vVertex.xyz = Vec3(x, y, 0.33f);
				pVB.push_back(vVertex);
			}

			// Generate cylinder vertices
			for (int32 h = 0; h < nSlices + 1; ++h)
			{
				float x = cosf(((float)h) * nSliceStep);
				float y = sinf(((float)h) * nSliceStep);

				vVertex.xyz = Vec3(x, y, 0.33f);
				pVB.push_back(vVertex);

				vVertex.xyz = Vec3(x, y, -0.33f);
				pVB.push_back(vVertex);
			}

			// Generate bottom cone vertices
			for (int32 h = 0; h < nSlices + 1; ++h)
			{
				float x = cosf(((float)h) * nSliceStep);
				float y = sinf(((float)h) * nSliceStep);

				vVertex.xyz = Vec3(x, y, -0.33f);
				pVB.push_back(vVertex);

				vVertex.xyz = Vec3(x * 0.01f, y * 0.01f, -1);
				pVB.push_back(vVertex);
			}

			gRenDev->m_DevBufMan.UpdateBuffer(m_rainVertexBuffer, pVB.data(), totalVertexCount * vertexStride);
		}
	}
}

void CRainStage::Destroy()
{
	if (m_rainVertexBuffer != ~0u)
	{
		gRenDev->m_DevBufMan.Destroy(m_rainVertexBuffer);
	}
}

void CRainStage::Update()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const auto shouldApplyOcclusion = rd->m_bDeferredRainOcclusionEnabled;

	// Create/release the occlusion texture on demand
	if (!shouldApplyOcclusion && CTexture::IsTextureExist(CRendererResources::s_ptexRainOcclusion))
		CRendererResources::s_ptexRainOcclusion->ReleaseDeviceTexture(false);
	else if (shouldApplyOcclusion && !CTexture::IsTextureExist(CRendererResources::s_ptexRainOcclusion))
		CRendererResources::s_ptexRainOcclusion->CreateRenderTarget(eTF_R8G8B8A8, Clr_Neutral);

	if (RenderView()->GetCurrentEye() != CCamera::eEye_Right)
	{
		// TODO: improve this dependency and make it double-buffer.
		// copy rain info to member variable every frame.
		m_RainVolParams = rd->m_p3DEngineCommon.m_RainInfo;
	}
}

void CRainStage::ExecuteDeferredRainGBuffer()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	if (!rd->m_bDeferredRainEnabled)
		return;

	PROFILE_LABEL_SCOPE("DEFERRED_RAIN_GBUFFER");

	CTexture* CRendererResources__s_ptexSceneSpecular = CRendererResources::s_ptexSceneSpecular;
#if defined(DURANGO_USE_ESRAM)
	CRendererResources__s_ptexSceneSpecular = CRendererResources::s_ptexSceneSpecularESRAM;
#endif

	// TODO: Try avoiding the copy by directly accessing UAVs
	m_passCopyGBufferNormal.Execute(CRendererResources::s_ptexSceneNormalsMap, CRendererResources::s_ptexSceneNormalsBent);
	m_passCopyGBufferSpecular.Execute(CRendererResources__s_ptexSceneSpecular, CRendererResources::s_ptexSceneSpecularTmp);
	m_passCopyGBufferDiffuse.Execute(CRendererResources::s_ptexSceneDiffuse, CRendererResources::s_ptexSceneDiffuseTmp);

	const auto& viewInfo = GetCurrentViewInfo();

	const auto& rainVolParams = m_RainVolParams;
	const auto shouldApplyOcclusion = rd->m_bDeferredRainOcclusionEnabled;

	CTexture* pDepthStencilTex = RenderView()->GetDepthTarget();
	CTexture* pOcclusionTex = shouldApplyOcclusion ? CRendererResources::s_ptexRainOcclusion : CRendererResources::s_ptexBlack;

	uint64 rtMask = 0;
	if (shouldApplyOcclusion)
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE0]; // Occlusion
	if (rainVolParams.fSplashesAmount > 0.001f && rainVolParams.fRainDropsAmount > 0.001f)
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE1]; // Splashes

	auto& pass = m_passDeferredRainGBuffer;

	if (pass.IsDirty(rtMask, pDepthStencilTex->GetID(), pOcclusionTex->GetID()))
	{
		static CCryNameTSCRC techName("DeferredRainGBuffer");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		pass.SetTechnique(CShaderMan::s_ShaderDeferredRain, techName, rtMask);

		pass.SetState(GS_NODEPTHTEST);

		pass.SetRenderTarget(0, CRendererResources::s_ptexSceneNormalsMap);
		pass.SetRenderTarget(1, CRendererResources__s_ptexSceneSpecular);
		pass.SetRenderTarget(2, CRendererResources::s_ptexSceneDiffuse);

		pass.SetTexture(0, CRendererResources::s_ptexLinearDepth);
		pass.SetTexture(1, CRendererResources::s_ptexSceneNormalsBent);
		pass.SetTexture(2, CRendererResources::s_ptexSceneSpecularTmp);
		pass.SetTexture(3, CRendererResources::s_ptexSceneDiffuseTmp);
		pass.SetTexture(4, m_pSurfaceFlowTex);
		pass.SetTexture(5, m_pRainSpatterTex);
		pass.SetTexture(6, m_pPuddleMaskTex);
		pass.SetTexture(8, pOcclusionTex);
		pass.SetTexture(9, pDepthStencilTex, SResourceView::eSRV_StencilOnly);

		pass.SetSampler(0, EDefaultSamplerStates::PointWrap);
		pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);
		pass.SetSampler(2, EDefaultSamplerStates::PointBorder_White);
		pass.SetSampler(3, EDefaultSamplerStates::BilinearWrap);

		// Those texture and sampler are used in EncodeGBuffer().
		pass.SetTexture(30, CRendererResources::s_ptexNormalsFitting);
		pass.SetSampler(9, EDefaultSamplerStates::PointClamp);

		pass.SetRequirePerViewConstantBuffer(true);
	}

	// TODO: use Texture2DArray instead of Texture2D for rain ripples texture.
	if (!rd->m_bPauseTimer)
	{
		// flip rain ripple texture
		const float elapsedTime = GetGraphicsPipeline().GetAnimationTime().GetSeconds();
		CRY_ASSERT(elapsedTime >= 0.0f);
		const float AnimTexFlipTime = 0.05f;
		m_rainRippleTexIndex = (uint32)(elapsedTime / AnimTexFlipTime) % m_pRainRippleTex.size();
	}
	CRY_ASSERT(m_rainRippleTexIndex < m_pRainRippleTex.size());
	pass.SetTexture(7, m_pRainRippleTex[m_rainRippleTexIndex]);

	pass.BeginConstantUpdate();

	float fMaxZ = -1.f;
	if (CRenderer::CV_r_rain_maxviewdist_deferred > viewInfo.nearClipPlane)
	{
		fMaxZ = (viewInfo.farClipPlane - (viewInfo.nearClipPlane * viewInfo.farClipPlane) / CRenderer::CV_r_rain_maxviewdist_deferred) / (viewInfo.farClipPlane - viewInfo.nearClipPlane);
	}

	// Global wind params
	Vec3 windVec = gEnv->p3DEngine->GetGlobalWind(false);

	// Animated puddles
	const float fTime = GetGraphicsPipeline().GetAnimationTime().GetSeconds() * 0.333f;
	const float puddleWindScale = -0.15f;
	const float puddleOffsX = fTime * puddleWindScale * windVec.x;
	const float puddleOffsY = fTime * puddleWindScale * windVec.y;

	static CCryNameR puddleParamName0("g_RainPuddleParams0");
	const Vec4 vPuddleParams0 = Vec4(puddleOffsX, puddleOffsY, rainVolParams.fPuddlesAmount * rainVolParams.fCurrentAmount, rainVolParams.fDiffuseDarkening);
	pass.SetConstant(puddleParamName0, vPuddleParams0);

	static CCryNameR puddleParamName1("g_RainPuddleParams1");
	const float invPuddleMask = clamp_tpl(1.0f - rainVolParams.fPuddlesMaskAmount, 0.0f, 1.0f);
	const Vec4 vPuddleParams1 = Vec4(invPuddleMask, rainVolParams.fPuddlesRippleAmount, rainVolParams.fSplashesAmount, 0.0f);
	pass.SetConstant(puddleParamName1, vPuddleParams1);

	// Volume
	static CCryNameR volumeParamName("g_RainVolumeParams");
	const Vec4 vRainPosCS = Vec4(rainVolParams.vWorldPos, 1.f / max(rainVolParams.fRadius, 1e-3f));
	pass.SetConstant(volumeParamName, vRainPosCS);

	// Global colour multiplier
	static CCryNameR colorMulParamName("g_RainColorMultipliers");
	const float fAmount = rainVolParams.fCurrentAmount * CRenderer::CV_r_rainamount;
	Vec4 vRainColorMultipliers = Vec4(rainVolParams.vColor, 1.0f) * fAmount;
	vRainColorMultipliers.w = fMaxZ > 0.0f ? CRenderer::CV_r_rain_maxviewdist_deferred / viewInfo.farClipPlane : 1.0f;
	vRainColorMultipliers.w = -10.0f / vRainColorMultipliers.w;
	pass.SetConstant(colorMulParamName, vRainColorMultipliers);

	if (shouldApplyOcclusion)
	{
		// Occlusion buffer matrix
		static CCryNameR occTransMatParamName("g_RainOcc_TransMat");
		pass.SetConstantArray(occTransMatParamName, (Vec4*)rainVolParams.matOccTransRender.GetData(), 4);

		// Pre-calculate wind-driven occlusion sample offset
		const float windOffsetScale = 15.0f / (float)RAIN_OCC_MAP_SIZE;
		windVec = rainVolParams.matOccTransRender.TransformVector(windVec);
		windVec.x *= windOffsetScale;
		windVec.y *= windOffsetScale;

		static CCryNameR windParamName("g_RainOcc_WindOffs");
		Vec4 pWindParams(windVec.x, windVec.y, 0.f, 0.f);
		pass.SetConstant(windParamName, pWindParams);
	}

	pass.Execute();
}

void CRainStage::Execute()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	if ((CRenderer::CV_r_rain < 1) || !CRenderer::CV_r_PostProcess || !CRendererResources::s_ptexBackBuffer || !CRendererResources::s_ptexSceneTarget)
		return;

	const auto& rainVolParams = m_RainVolParams;
	const auto shouldApplyOcclusion = rd->m_bDeferredRainOcclusionEnabled;

	// this emulates FX_DeferredRainOcclusion()'s return value condition.
	if (shouldApplyOcclusion)
	{
		if (rainVolParams.areaAABB.IsReset() ||
			!CTexture::IsTextureExist(CRendererResources::s_ptexRainOcclusion))
			return;

		if (!(CTexture::IsTextureExist(CRendererResources::s_ptexRainSSOcclusion[0]) && CTexture::IsTextureExist(CRendererResources::s_ptexRainSSOcclusion[1])))
		{
			// Render targets not generated yet
			// - Better to skip and have no rain than it render over everything
			return;
		}
	}

	const bool bActive = rd->m_bDeferredRainEnabled && rainVolParams.fRainDropsAmount > 0.01f;
	if (!bActive)
		return;

	PROFILE_LABEL_SCOPE("RAIN");

	// Generate screen-space rain occlusion
	if (shouldApplyOcclusion)
	{
		PROFILE_LABEL_SCOPE("RAIN_DISTANT_OCCLUSION");

		{
			PROFILE_LABEL_SCOPE("ACCUMULATE");

			auto& pass = m_passRainOcclusionAccumulation;

			CTexture* pOcclusionTex = CRendererResources::s_ptexRainOcclusion;

			if (pass.IsDirty())
			{
				static CCryNameTSCRC pSceneRainOccAccTechName("SceneRainOccAccumulate");
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
				pass.SetTechnique(CShaderMan::s_shPostEffectsGame, pSceneRainOccAccTechName, 0);
				pass.SetRenderTarget(0, CRendererResources::s_ptexRainSSOcclusion[0]);

				pass.SetState(GS_NODEPTHTEST);

				pass.SetRequirePerViewConstantBuffer(true);
				pass.SetRequireWorldPos(true);

				pass.SetTexture(0, CRendererResources::s_ptexLinearDepthScaled[2]);
				pass.SetTexture(1, pOcclusionTex);
				pass.SetTexture(2, m_pHighFreqNoiseTex);

				pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
				pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);

				pass.SetRequirePerViewConstantBuffer(true);
			}

			pass.BeginConstantUpdate();

			static CCryNameR pRainOccParamName("sceneRainOccMtx");
			Vec4* pParam = (Vec4*)rainVolParams.matOccTransRender.GetData();
			pass.SetConstantArray(pRainOccParamName, pParam, 4, eHWSC_Vertex);

			pass.Execute();
		}

		{
			PROFILE_LABEL_SCOPE("BLUR");

			const float fDist = 8.0f;
			m_passRainOcclusionBlur.Execute(CRendererResources::s_ptexRainSSOcclusion[0], CRendererResources::s_ptexRainSSOcclusion[1], 1.0f, fDist);
		}
	}

	auto* pRenderTarget = CRendererResources::s_ptexHDRTarget;

	D3DViewPort viewport;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(pRenderTarget->GetWidth());
	viewport.Height = static_cast<float>(pRenderTarget->GetHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	auto& pass = m_passRain;
	pass.SetRenderTarget(0, pRenderTarget);
	pass.SetViewport(viewport);
	pass.BeginAddingPrimitives();

	static CCryNameTSCRC pSceneRainTechName("SceneRain");

	const float fAmount = rainVolParams.fCurrentAmount * CRenderer::CV_r_rainamount;
	const int32 nLayers = clamp_tpl(int32(fAmount + 0.0001f), 1, m_maxRainLayers);

	// Get lightning color - will just use for overbright rain
	Vec3 v;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_COLOR, v);
	const float fHighlight = 2.f * v.len();
	const float fSizeMult = max(CRenderer::CV_r_rainDistMultiplier, 1e-3f) * 0.5f;

	uint64 rtMask = 0;
	rtMask |= shouldApplyOcclusion ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;

	auto pPerViewCB = rd->GetGraphicsPipeline().GetMainViewConstantBuffer();

	for (int32 nCurLayer = 0; nCurLayer < nLayers; ++nCurLayer)
	{
		auto& prim = m_rainPrimitives[nCurLayer];

		prim.SetFlags(CRenderPrimitive::eFlags_None);
		prim.SetTechnique(CShaderMan::s_shPostEffectsGame, pSceneRainTechName, rtMask);

		prim.SetTexture(0, CRendererResources::s_ptexLinearDepth);
		prim.SetTexture(1, m_pRainfallTex);
		prim.SetTexture(2, m_pRainfallNormalTex);
		prim.SetTexture(3, CRendererResources::s_ptexHDRFinalBloom);

		auto* pSSOcclusionTex = shouldApplyOcclusion ? CRendererResources::s_ptexRainSSOcclusion[0] : CRendererResources::s_ptexBlack;
		prim.SetTexture(4, pSSOcclusionTex);

		// Bind average luminance
		prim.SetTexture(5, CRendererResources::s_ptexHDRToneMaps[0]);

		prim.SetSampler(0, EDefaultSamplerStates::TrilinearWrap);
		prim.SetSampler(1, EDefaultSamplerStates::TrilinearClamp);
		prim.SetSampler(2, EDefaultSamplerStates::PointClamp);

		prim.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB.get(), EShaderStage_Vertex | EShaderStage_Pixel);
		prim.Compile(pass);

		// update constant buffer
		{
			auto constants = prim.GetConstantManager().BeginTypedConstantUpdate<SSceneRainCB>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

			constants->sceneRainMtx = Matrix44(Matrix33(rainVolParams.qRainRotation));

			const Vec2 unscaledFactor(1.0f, 1.0f);
			constants->unscaledFactor = Vec4(unscaledFactor.x, unscaledFactor.y, 1.0f, 1.0f);

			Vec4 pParams;
			pParams.x = rainVolParams.fRainDropsSpeed;
			pParams.z = float(nCurLayer) / max(nLayers - 1, 1);
			pParams.y = fHighlight * (1.f - pParams.z);
			pParams.w = (nCurLayer + 1) * fSizeMult;
			pParams.w = powf(pParams.w, 1.5f);
			constants->sceneRainParams0 = pParams;

			constants->sceneRainParams1 = Vec4(rainVolParams.fRainDropsLighting, fAmount * rainVolParams.fRainDropsAmount, 0.0f, 0.0f);

			prim.GetConstantManager().EndTypedConstantUpdate(constants);
		}

		pass.AddPrimitive(&prim);
	}

	pass.Execute();
}

void CRainStage::ExecuteRainOcclusion()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	if (!rd->m_bDeferredRainOcclusionEnabled)
		return;

	// TODO: m_RainInfo needs to be unique for each view-port if the engine supports multi view-port rendering.
	SRainParams& rainVolParams = rd->m_p3DEngineCommon.m_RainInfo;
	const auto gpuId = rd->RT_GetCurrGpuID();

	if (rainVolParams.areaAABB.IsReset() ||
		rd->m_p3DEngineCommon.m_RainOccluders.m_bProcessed[gpuId])
		return;

	const auto& arrOccluders = rd->m_p3DEngineCommon.m_RainOccluders.m_arrCurrOccluders[gRenDev->GetRenderThreadID()];
	if (arrOccluders.empty()) 
		return;

	ExecuteRainOcclusionGen();

	rd->m_p3DEngineCommon.m_RainOccluders.m_bProcessed[gpuId] = true;

	// store occlusion transformation matrix when occlusion map is updated.
	// TODO: make this variable class member variable after porting all rain features to new graphics pipeline.
	rainVolParams.matOccTransRender = rainVolParams.matOccTrans;
	m_RainVolParams.matOccTransRender = m_RainVolParams.matOccTrans;
}

void CRainStage::ExecuteRainOcclusionGen()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	SRainParams& rainVolParams = m_RainVolParams;

	// Get temp depth buffer
	CTexture* pTmpDepthSurface = rd->CreateDepthTarget(RAIN_OCC_MAP_SIZE, RAIN_OCC_MAP_SIZE, Clr_FarPlane, eTF_Unknown);

	// clear buffers
	CClearSurfacePass::Execute(CRendererResources::s_ptexRainOcclusion, Clr_Neutral);
	CClearSurfacePass::Execute(pTmpDepthSurface, CLEAR_ZBUFFER, Clr_FarPlane.r, Val_Unused);

	// render occluders to rain occlusion texture
	{
		auto& pass = m_passRainOcclusionGen;

		D3DViewPort viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(CRendererResources::s_ptexRainOcclusion->GetWidth());
		viewport.Height = static_cast<float>(CRendererResources::s_ptexRainOcclusion->GetHeight());
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		pass.SetRenderTarget(0, CRendererResources::s_ptexRainOcclusion);
		pass.SetDepthTarget(pTmpDepthSurface);
		pass.SetViewport(viewport);
		pass.BeginAddingPrimitives();

		static const Matrix44 matSs2Ps(2.f, 0.f, 0.f, -1.f,
		                               0.f, 2.f, 0.f, -1.f,
		                               0.f, 0.f, 1.f, 0.f,
		                               0.f, 0.f, 0.f, 1.f);
		const Matrix44& matOccTrans = rainVolParams.matOccTrans;
		const Matrix44 matTrans = matSs2Ps * matOccTrans;

		static CCryNameTSCRC techName("RainOcclusion");

		const auto& arrOccluders = rd->m_p3DEngineCommon.m_RainOccluders.m_arrCurrOccluders[gRenDev->GetRenderThreadID()];
		int32 index = 0;
		auto countPrimitives = m_rainOccluderPrimitives.size();
		for (auto& it : arrOccluders)
		{
			IRenderMesh* pRndMesh = const_cast<IRenderMesh*>(it.m_RndMesh.get());
			CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(pRndMesh);

			if (pRenderMesh)
			{
				pRenderMesh->RT_CheckUpdate(pRenderMesh->_GetVertexContainer(),pRenderMesh->_GetVertexFormat(), 0);
				buffer_handle_t hVertexStream = pRenderMesh->_GetVBStream(VSF_GENERAL);
				buffer_handle_t hIndexStream = pRenderMesh->_GetIBStream();

				if (hVertexStream != ~0u && hIndexStream != ~0u)
				{
					if (index >= countPrimitives)
					{
						m_rainOccluderPrimitives.emplace_back(stl::make_unique<CRenderPrimitive>());

						m_rainOccluderPrimitives[index].get()->AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(SRainOccluderCB), EShaderStage_Vertex);
					}

					CRY_ASSERT(m_rainOccluderPrimitives[index]);
					CRenderPrimitive& prim = *(m_rainOccluderPrimitives[index].get());
					++index;

					const auto primType = pRenderMesh->_GetPrimitiveType();
					const auto numIndex = pRenderMesh->_GetNumInds();
					prim.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
					prim.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
					prim.SetDrawInfo(primType, 0, 0, numIndex);

					prim.SetFlags(CRenderPrimitive::eFlags_None);
					prim.SetTechnique(CShaderMan::s_ShaderDeferredRain, techName, 0);
					prim.SetCullMode(eCULL_None);
					prim.SetRenderState(GS_DEPTHFUNC_LEQUAL | GS_DEPTHWRITE);
					prim.Compile(pass);

					// update constant buffer
					{
						auto constants = prim.GetConstantManager().BeginTypedConstantUpdate<SRainOccluderCB>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex);

						Matrix44A matWorld(it.m_WorldMat);
						constants->matRainOccluder = matTrans * matWorld;

						prim.GetConstantManager().EndTypedConstantUpdate(constants);
					}

					pass.AddPrimitive(&prim);
				}
			}
		}

		pass.Execute();
	}

	SAFE_RELEASE(pTmpDepthSurface);
}
