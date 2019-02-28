// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScreenSpaceSSS.h"
#include "DriverD3D.h"

void CScreenSpaceSSSStage::Execute(CTexture* pIrradianceTex)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	PROFILE_LABEL_SCOPE("SSSSS");

	CShader* pShader = CShaderMan::s_shDeferredShading;

	const auto& projMatrix = GetCurrentViewInfo().projMatrix;

	Vec4 viewSpaceParams(2.0f / projMatrix.m00, 2.0f / projMatrix.m11, -1.0f / projMatrix.m00, -1.0f / projMatrix.m11);
	float fProjScaleX = 0.5f * projMatrix.m00;
	float fProjScaleY = 0.5f * projMatrix.m11;

	static CCryNameTSCRC techBlur("SSSSS_Blur");
	static CCryNameR viewSpaceParamsName("ViewSpaceParams");
	static CCryNameR blurDirName("SSSBlurDir");

	// Horizontal pass
	{
		if (m_passH.IsDirty(pIrradianceTex->GetTextureID()))
		{
			m_passH.SetTechnique(pShader, techBlur, 0);
			m_passH.SetRenderTarget(0, CRendererResources::s_ptexSceneTargetR11G11B10F[1]);
			m_passH.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passH.SetState(GS_NODEPTHTEST);

			m_passH.SetTexture(0, pIrradianceTex);
			m_passH.SetTexture(1, CRendererResources::s_ptexLinearDepth);
			m_passH.SetTexture(2, CRendererResources::s_ptexSceneNormalsMap);
			m_passH.SetTexture(3, CRendererResources::s_ptexSceneDiffuse);
			m_passH.SetTexture(4, CRendererResources::s_ptexSceneSpecular);
			m_passH.SetSampler(0, EDefaultSamplerStates::PointClamp);
		}

		m_passH.BeginConstantUpdate();

		m_passH.SetConstant(viewSpaceParamsName, viewSpaceParams, eHWSC_Pixel);
		m_passH.SetConstant(blurDirName, Vec4(fProjScaleX, 0, 0, 0), eHWSC_Pixel);

		m_passH.Execute();
	}

	// Vertical pass
	{
		if (m_passV.IsDirty(pIrradianceTex->GetTextureID()))
		{
			m_passV.SetTechnique(pShader, techBlur, g_HWSR_MaskBit[HWSR_SAMPLE0]);
			m_passV.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);
			m_passV.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passV.SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);

			m_passV.SetTexture(0, CRendererResources::s_ptexSceneTargetR11G11B10F[1]);
			m_passV.SetTexture(1, CRendererResources::s_ptexLinearDepth);
			m_passV.SetTexture(2, CRendererResources::s_ptexSceneNormalsMap);
			m_passV.SetTexture(3, CRendererResources::s_ptexSceneDiffuse);
			m_passV.SetTexture(4, CRendererResources::s_ptexSceneSpecular);
			m_passV.SetTexture(5, pIrradianceTex);
			m_passV.SetSampler(0, EDefaultSamplerStates::PointClamp);
		}

		m_passV.BeginConstantUpdate();

		m_passV.SetConstant(viewSpaceParamsName, viewSpaceParams, eHWSC_Pixel);
		m_passV.SetConstant(blurDirName, Vec4(0, fProjScaleY, 0, 0), eHWSC_Pixel);

		m_passV.Execute();
	}
}
