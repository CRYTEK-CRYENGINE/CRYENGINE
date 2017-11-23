// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "SunShafts.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"

// TODO: Add support for occlusion query to find out if sky is visible at all

struct SSunShaftConstants
{
	Vec4 sunPos;
	Vec4 params;
};

void CSunShaftsStage::Init()
{
	m_passShaftsMask.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);

	m_passShaftsGen0.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
	m_passShaftsGen0.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
	m_passShaftsGen0.SetRequirePerViewConstantBuffer(true);
	m_passShaftsGen0.AllocateTypedConstantBuffer<SSunShaftConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Pixel);

	m_passShaftsGen1.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
	m_passShaftsGen1.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
	m_passShaftsGen1.AllocateTypedConstantBuffer<SSunShaftConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Pixel);
	m_passShaftsGen1.SetRequirePerViewConstantBuffer(true);
}

bool CSunShaftsStage::IsActive()
{
	return CRenderer::CV_r_sunshafts && CRenderer::CV_r_PostProcess;
}

CTexture* CSunShaftsStage::GetFinalOutputRT()
{
	return gRenDev->EF_GetRenderQuality() >= eRQ_High ? CRendererResources::s_ptexBackBufferScaled[0] : CRendererResources::s_ptexBackBufferScaled[1];
}

CTexture* CSunShaftsStage::GetTempOutputRT()
{
	return gRenDev->EF_GetRenderQuality() >= eRQ_High ? CRendererResources::s_ptexBackBufferScaledTemp[0] : CRendererResources::s_ptexBackBufferScaledTemp[1];
}

void CSunShaftsStage::GetCompositionParams(Vec4& params0, Vec4& params1)
{
	CSunShafts* pSunShafts = (CSunShafts*)PostEffectMgr()->GetEffect(EPostEffectID::SunShafts);
	Vec4 params[2];
	pSunShafts->GetSunShaftsParams(params);
	params0 = params[0];
	params1 = params[1];
}

void CSunShaftsStage::Execute()
{
	PROFILE_LABEL_SCOPE("SUNSHAFTS_GEN");

	if (!IsActive())
		return;

	CSunShafts* pSunShafts = (CSunShafts*)PostEffectMgr()->GetEffect(EPostEffectID::SunShafts);
	float rayAttenuation = clamp_tpl<float>(pSunShafts->m_pRaysAttenuation->GetParam(), 0.0f, 10.0f);

	CShader* pShader = CShaderMan::s_shPostSunShafts;
	CTexture* pFinalRT = GetFinalOutputRT();
	CTexture* pTempRT = GetTempOutputRT();

	// Generate mask for sun shafts
	{
		if (m_passShaftsMask.InputChanged())
		{
			static CCryNameTSCRC techMaskGen("SunShaftsMaskGen");
			uint64 rtMask = g_HWSR_MaskBit[HWSR_SAMPLE0];
			m_passShaftsMask.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
			m_passShaftsMask.SetTechnique(pShader, techMaskGen, rtMask);
			m_passShaftsMask.SetRenderTarget(0, pFinalRT);
			m_passShaftsMask.SetState(GS_NODEPTHTEST);

			m_passShaftsMask.SetTextureSamplerPair(0, CRendererResources::s_ptexLinearDepthScaled[0], EDefaultSamplerStates::PointClamp);
			m_passShaftsMask.SetTextureSamplerPair(1, CRendererResources::s_ptexHDRTargetScaled[0], EDefaultSamplerStates::PointClamp);  // TODO
		}

		m_passShaftsMask.BeginConstantUpdate();
		m_passShaftsMask.Execute();
	}

	// Apply local radial blur to mask
	{
		SRenderViewInfo viewInfo[2];
		int viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfo);

		Vec4 sunPosScreen[2];
		Vec3 sunPos = gEnv->p3DEngine->GetSunDir() * 1000.0f;

		for (int i = 0; i < viewInfoCount; ++i)
		{
			sunPosScreen[i] = Vec4(sunPos, 1.0f) * viewInfo[i].cameraProjMatrix;
			sunPosScreen[i].x = (( sunPosScreen[i].x + sunPosScreen[i].w) * 0.5f) / (1e-6f + sunPosScreen[i].w);
			sunPosScreen[i].y = ((-sunPosScreen[i].y + sunPosScreen[i].w) * 0.5f) / (1e-6f + sunPosScreen[i].w);
			sunPosScreen[i].w = gEnv->p3DEngine->GetSunDirNormalized().dot(PostProcessUtils().m_pViewProj.GetRow(2));
		}

		// Pass 1
		{
			if (m_passShaftsGen0.InputChanged())
			{
				static CCryNameTSCRC techShaftsGen("SunShaftsGen");
				uint64 rtMask = g_HWSR_MaskBit[HWSR_SAMPLE0];
				m_passShaftsGen0.SetTechnique(pShader, techShaftsGen, rtMask);
				m_passShaftsGen0.SetRenderTarget(0, pTempRT);
				m_passShaftsGen0.SetState(GS_NODEPTHTEST);
				m_passShaftsGen0.SetTextureSamplerPair(0, pFinalRT, EDefaultSamplerStates::LinearClamp);
			}

			auto constants = m_passShaftsGen0.BeginTypedConstantUpdate<SSunShaftConstants>(eConstantBufferShaderSlot_PerBatch);
			constants->sunPos = sunPosScreen[0];
			constants->params = Vec4(0.1f, rayAttenuation, 0, 0);
			
			if (viewInfoCount > 1)
			{
				constants.BeginStereoOverride();
				constants->sunPos = sunPosScreen[1];
			}

			m_passShaftsGen0.EndTypedConstantUpdate(constants);
			m_passShaftsGen0.Execute();
		}

		// Pass 2
		{
			if (m_passShaftsGen1.InputChanged())
			{
				static CCryNameTSCRC techShaftsGen("SunShaftsGen");
				uint64 rtMask = g_HWSR_MaskBit[HWSR_SAMPLE0];
				m_passShaftsGen1.SetTechnique(pShader, techShaftsGen, rtMask);
				m_passShaftsGen1.SetRenderTarget(0, pFinalRT);
				m_passShaftsGen1.SetState(GS_NODEPTHTEST);
				m_passShaftsGen1.SetTextureSamplerPair(0, pTempRT, EDefaultSamplerStates::LinearClamp);
			}

			auto constants = m_passShaftsGen1.BeginTypedConstantUpdate<SSunShaftConstants>(eConstantBufferShaderSlot_PerBatch);
			constants->sunPos = sunPosScreen[0];
			constants->params = Vec4(0.025f, rayAttenuation, 0, 0);

			if (viewInfoCount > 1)
			{
				constants.BeginStereoOverride();
				constants->sunPos = sunPosScreen[1];
			}

			m_passShaftsGen1.EndTypedConstantUpdate(constants);
			m_passShaftsGen1.Execute();
		}
	}
}
