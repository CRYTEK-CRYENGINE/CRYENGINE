// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/UtilityPasses.h"
#include "Common/FullscreenPass.h"
#include "Common/PrimitiveRenderPass.h"

class CRainStage : public CGraphicsPipelineStage
{
public:
	CRainStage();
	virtual ~CRainStage();

	void Init() final;
	void Update() final;
	void Destroy();

	void ExecuteRainOcclusion();
	void ExecuteDeferredRainGBuffer();
	void Execute();

private:
	static const int32  m_slices = 12;
	static const int32  m_maxRainLayers = 3;
	static const uint32 RainRippleTexCount = 24;

private:
	void ExecuteRainOcclusionGen();

private:
	_smart_ptr<CTexture> m_pSurfaceFlowTex;
	_smart_ptr<CTexture> m_pRainSpatterTex;
	_smart_ptr<CTexture> m_pPuddleMaskTex;
	_smart_ptr<CTexture> m_pHighFreqNoiseTex;
	_smart_ptr<CTexture> m_pRainfallTex;
	_smart_ptr<CTexture> m_pRainfallNormalTex;

	std::array<_smart_ptr<CTexture>, RainRippleTexCount> m_pRainRippleTex;
	uint32                                         m_rainRippleTexIndex = 0;

	CPrimitiveRenderPass                           m_passRainOcclusionGen;
	CStretchRectPass                               m_passCopyGBufferNormal;
	CStretchRectPass                               m_passCopyGBufferSpecular;
	CStretchRectPass                               m_passCopyGBufferDiffuse;
	CFullscreenPass                                m_passDeferredRainGBuffer;
	CFullscreenPass                                m_passRainOcclusionAccumulation;
	CGaussianBlurPass                              m_passRainOcclusionBlur;
	CPrimitiveRenderPass                           m_passRain;

	std::vector<std::unique_ptr<CRenderPrimitive>> m_rainOccluderPrimitives;

	CRenderPrimitive                               m_rainPrimitives[m_maxRainLayers];
	buffer_handle_t                                m_rainVertexBuffer = ~0u;

	SRainParams                                    m_RainVolParams;
};
