// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MotionBlur.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

float CMotionBlurStage::ComputeMotionScale()
{
	// The length of the generated motion vectors is proportional to the current time step, so we need
	// to rescale the motion vectors to simulate a constant camera exposure time

	float exposureTime = 1.0f / std::max(CRenderer::CV_r_MotionBlurShutterSpeed, 1e-6f);
	float timeStep = std::max(gEnv->pTimer->GetFrameTime(), 1e-6f);

	exposureTime *= gEnv->pTimer->GetTimeScale();

	return exposureTime / timeStep;
}

void CMotionBlurStage::Execute()
{
	if (!CRenderer::CV_r_MotionBlur || gRenDev->m_nDisableTemporalEffects)
		return;

	PROFILE_LABEL_SCOPE("MB");

	CD3D9Renderer* rd = gcpRendD3D;
	CShader* pShader = CShaderMan::s_shPostMotionBlur;

	SRenderViewport viewport = RenderView()->GetViewport();

	int vpX = viewport.x, vpY=viewport.y, vpWidth=viewport.width, vpHeight=viewport.height;

	// Check if DOF is enabled
	CDepthOfField* pDofRenderTech = (CDepthOfField*)PostEffectMgr()->GetEffect(EPostEffectID::DepthOfField);
	SDepthOfFieldParams dofParams = pDofRenderTech->GetParams();
	dofParams.vFocus.w *= 2.0f;
	const bool bGatherDofEnabled = CRenderer::CV_r_dof > 0 && CRenderer::CV_r_DofMode == 1 && dofParams.vFocus.w > 0.0001f;

	SRenderViewInfo viewInfo[2];
	size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfo);

	Matrix44A mViewProjPrev =  viewInfo[0].prevCameraMatrix;
	Matrix44 mViewProj = GetUtils().m_pView;
	mViewProjPrev = mViewProjPrev * GetUtils().m_pProj * GetUtils().m_pScaleBias;
	mViewProjPrev.Transpose();

	float tileCountX = (float)CRendererResources::s_ptexVelocityTiles[1]->GetWidth();
	float tileCountY = (float)CRendererResources::s_ptexVelocityTiles[1]->GetHeight();

	static CCryNameR motionBlurParamName("vMotionBlurParams");

	{
		PROFILE_LABEL_SCOPE("PACK VELOCITY");

		CMotionBlur* pMB = (CMotionBlur*)PostEffectMgr()->GetEffect(EPostEffectID::MotionBlur);
		const float maxRange = 32.0f;
		const float amount = clamp_tpl<float>(pMB->m_pRadBlurAmount->GetParam() / maxRange, 0.0f, 1.0f);
		const float radius = 1.0f / clamp_tpl<float>(pMB->m_pRadBlurRadius->GetParam(), 1e-6f, 2.0f);
		const Vec4 blurDir = pMB->m_pDirectionalBlurVec->GetParamVec4();
		const Vec4 dirBlurParam = Vec4(blurDir.x * (maxRange / (float)vpWidth), blurDir.y * (maxRange / (float)vpHeight), (float)vpWidth / (float)vpHeight, 1.0f);
		const Vec4 radBlurParam = Vec4(pMB->m_pRadBlurScreenPosX->GetParam() * dirBlurParam.z, pMB->m_pRadBlurScreenPosY->GetParam(), radius * amount, amount);

		const bool bRadialBlur = amount + (blurDir.x * blurDir.x) + (blurDir.y * blurDir.y) > 1.0f / (float)vpWidth;

		if (m_passPacking.IsDirty(bRadialBlur, GetUtils().GetVelocityObjectRT(RenderView())->GetTextureID()))
		{
			static CCryNameTSCRC techPackVelocities("PackVelocities");
			m_passPacking.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passPacking.SetTechnique(pShader, techPackVelocities, bRadialBlur ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0);
			m_passPacking.SetRenderTarget(0, CRendererResources::s_ptexVelocity);
			m_passPacking.SetState(GS_NODEPTHTEST);
			m_passPacking.SetTexture(0, CRendererResources::s_ptexLinearDepth);
			m_passPacking.SetTexture(1, GetUtils().GetVelocityObjectRT(RenderView()));
			m_passPacking.SetSampler(0, EDefaultSamplerStates::PointClamp);
			m_passPacking.SetRequireWorldPos(true);
		}

		static CCryNameR viewProjPrevName("mViewProjPrev");
		static CCryNameR dirBlurName("vDirectionalBlur");
		static CCryNameR radBlurName("vRadBlurParam");

		m_passPacking.BeginConstantUpdate();

		mViewProjPrev.Transpose();
		m_passPacking.SetConstantArray(viewProjPrevName, (Vec4*)mViewProjPrev.GetData(), 4, eHWSC_Pixel);
		m_passPacking.SetConstant(dirBlurName, dirBlurParam, eHWSC_Pixel);
		m_passPacking.SetConstant(radBlurName, radBlurParam, eHWSC_Pixel);
		m_passPacking.SetConstant(motionBlurParamName,
			Vec4(ComputeMotionScale(), 1.0f / tileCountX, 1.0f / tileCountX * CRenderer::CV_r_MotionBlurCameraMotionScale, 0), eHWSC_Pixel);

		m_passPacking.Execute();
	}

	{
		PROFILE_LABEL_SCOPE("VELOCITY TILES");

		static CCryNameTSCRC techVelocityTileGen("VelocityTileGen");
		static CCryNameTSCRC techTileNeighborhood("VelocityTileNeighborhood");

		// Tile generation first pass
		{
			if (m_passTileGen1.IsDirty())
			{
				m_passTileGen1.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				m_passTileGen1.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				m_passTileGen1.SetTechnique(pShader, techVelocityTileGen, 0);
				m_passTileGen1.SetRenderTarget(0, CRendererResources::s_ptexVelocityTiles[0]);
				m_passTileGen1.SetState(GS_NODEPTHTEST);
				m_passTileGen1.SetTextureSamplerPair(0, CRendererResources::s_ptexVelocity, EDefaultSamplerStates::PointClamp);
			}

			m_passTileGen1.BeginConstantUpdate();

			Vec4 params = Vec4(
				      (float)CRendererResources::s_ptexVelocity->GetWidth(),
				      (float)CRendererResources::s_ptexVelocity->GetHeight(),
				ceilf((float)CRendererResources::s_renderWidth / tileCountX), 0);
			m_passTileGen1.SetConstant(motionBlurParamName, params, eHWSC_Pixel);

			m_passTileGen1.Execute();
		}

		// Tile generation second pass
		{
			if (m_passTileGen2.IsDirty())
			{
				m_passTileGen2.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				m_passTileGen2.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				m_passTileGen2.SetTechnique(pShader, techVelocityTileGen, 0);
				m_passTileGen2.SetRenderTarget(0, CRendererResources::s_ptexVelocityTiles[1]);
				m_passTileGen2.SetState(GS_NODEPTHTEST);
				m_passTileGen2.SetTextureSamplerPair(0, CRendererResources::s_ptexVelocityTiles[0], EDefaultSamplerStates::PointClamp);
			}

			m_passTileGen2.BeginConstantUpdate();

			Vec4 params = Vec4(
				      (float)CRendererResources::s_ptexVelocityTiles[0]->GetWidth(),
				      (float)CRendererResources::s_ptexVelocityTiles[0]->GetHeight(),
				ceilf((float)CRendererResources::s_renderHeight / tileCountY), 1);

			m_passTileGen2.SetConstant(motionBlurParamName, params, eHWSC_Pixel);

			m_passTileGen2.Execute();
		}

		// Neighborhood max
		{
			if (m_passNeighborMax.IsDirty())
			{
				m_passNeighborMax.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				m_passNeighborMax.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				m_passNeighborMax.SetTechnique(pShader, techTileNeighborhood, 0);
				m_passNeighborMax.SetRenderTarget(0, CRendererResources::s_ptexVelocityTiles[2]);
				m_passNeighborMax.SetState(GS_NODEPTHTEST);
				m_passNeighborMax.SetTextureSamplerPair(0, CRendererResources::s_ptexVelocityTiles[1], EDefaultSamplerStates::PointClamp);
			}

			m_passNeighborMax.BeginConstantUpdate();

			Vec4 params = Vec4(1.0f / tileCountX, 1.0f / tileCountY, 0, 0);
			m_passNeighborMax.SetConstant(motionBlurParamName, params, eHWSC_Pixel);

			m_passNeighborMax.Execute();
		}
	}

	{
		PROFILE_LABEL_SCOPE("MOTION VECTOR APPLY");

		if (bGatherDofEnabled)
		{
			m_passCopy.Execute(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexSceneTargetR11G11B10F[0]);
		}

		if (m_passMotionBlur.IsDirty(CRenderer::CV_r_MotionBlurQuality, bGatherDofEnabled))
		{
			uint64 rtMask = 0;
			rtMask |= (CRenderer::CV_r_MotionBlurQuality >= 2) ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;
			rtMask |= (CRenderer::CV_r_MotionBlurQuality == 1) ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;

			static CCryNameTSCRC techMotionBlur("MotionBlur");
			m_passMotionBlur.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passMotionBlur.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passMotionBlur.SetTechnique(pShader, techMotionBlur, rtMask);
			m_passMotionBlur.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
			m_passMotionBlur.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);
			m_passMotionBlur.SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA);
			m_passMotionBlur.SetTexture(0, bGatherDofEnabled ? CRendererResources::s_ptexSceneTargetR11G11B10F[0] : CRendererResources::s_ptexHDRTargetPrev);
			m_passMotionBlur.SetTexture(1, CRendererResources::s_ptexVelocity);
			m_passMotionBlur.SetTexture(2, CRendererResources::s_ptexVelocityTiles[2]);
			m_passMotionBlur.SetSampler(0, EDefaultSamplerStates::LinearClamp);
			m_passMotionBlur.SetSampler(1, EDefaultSamplerStates::PointClamp);
		}

		m_passMotionBlur.BeginConstantUpdate();

		Vec4 params = Vec4(1.0f / tileCountX, 1.0f / tileCountY, 0, 0);
		m_passMotionBlur.SetConstant(motionBlurParamName, params, eHWSC_Pixel);

		m_passMotionBlur.Execute();
	}
}
