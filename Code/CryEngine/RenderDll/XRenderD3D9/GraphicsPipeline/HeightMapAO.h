// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CHeightMapAOStage : public CGraphicsPipelineStage
{
public:
	void Update() final;
	void Execute();

	bool IsValid() const { return m_bHeightMapAOExecuted; }

	const ShadowMapFrustum* GetHeightMapFrustum   () const { CRY_ASSERT(m_bHeightMapAOExecuted); return m_pHeightMapFrustum; }
	CTexture*         GetHeightMapAOScreenDepthTex() const { CRY_ASSERT(m_bHeightMapAOExecuted); return m_pHeightMapAOScreenDepthTex; }
	CTexture*         GetHeightMapAOTex           () const { CRY_ASSERT(m_bHeightMapAOExecuted); return m_pHeightMapAOTex; }

private:
	CFullscreenPass m_passSampling;
	CFullscreenPass m_passSmoothing;
	CMipmapGenPass  m_passMipmapGen;

	bool                    m_bHeightMapAOExecuted = false;
	const ShadowMapFrustum* m_pHeightMapFrustum;
	CTexture*               m_pHeightMapAOScreenDepthTex;
	CTexture*               m_pHeightMapAOTex;
};
