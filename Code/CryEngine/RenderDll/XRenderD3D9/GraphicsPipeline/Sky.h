// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"

class CSkyStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_Sky;

	CSkyStage(CGraphicsPipeline& graphicsPipeline);

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		if (!(flags & SHDF_ALLOW_SKY))
			return false;

		return gcpRendD3D->m_p3DEngineCommon[gRenDev->GetRenderThreadID()].m_SkyInfo.m_bIsVisible;
	}

	void Init() final;
	void Update() final;

	void Execute(CTexture* pColorTex, CTexture* pDepthTex);
	void ExecuteMinimum(CTexture* pColorTex, CTexture* pDepthTex);

private:
	void CreateSkyDomeTextures(int32 width, int32 height);
	bool LoadStarsData();
	void SetSkyParameters();
	void SetHDRSkyParameters();

private:
	int                     m_skyDomeTextureLastTimeStamp;
	CTexture*               m_pSkyDomeTextureMie;
	CTexture*               m_pSkyDomeTextureRayleigh;
	uint32                  m_numStars;
	_smart_ptr<IRenderMesh> m_pStarMesh;

	CFullscreenPass         m_skyPass;
	Vec4                    m_paramMoonTexGenRight;
	Vec4                    m_paramMoonTexGenUp;
	Vec4                    m_paramMoonDirSize;
	CRenderPrimitive        m_starsPrimitive;
	CPrimitiveRenderPass    m_starsPass;
};