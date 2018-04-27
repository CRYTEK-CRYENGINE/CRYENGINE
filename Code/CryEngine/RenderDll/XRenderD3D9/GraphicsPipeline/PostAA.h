// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CPostAAStage : public CGraphicsPipelineStage
{
public:
	// Width and height should be the actual dimensions of the texture to be antialiased, 
	// which might be different than the provided renderview's output resolution (e.g. VR).
	void CalculateJitterOffsets(int targetWidth, int targetHeight, CRenderView* pTargetRenderView);
	void CalculateJitterOffsets(CRenderView* pRenderView)
	{
		const int32_t w = pRenderView->GetRenderResolution()[0];
		const int32_t h = pRenderView->GetRenderResolution()[1];
		CRY_ASSERT(w > 0 && h > 0);
		if (w > 0 && h > 0)
			CalculateJitterOffsets(w, h, pRenderView);
	}

	void Init();
	void Execute();

private:
	void ApplySMAA(CTexture*& pCurrRT);
	void ApplyTemporalAA(CTexture*& pCurrRT, CTexture*& pMgpuRT, uint32 aaMode);
	void DoFinalComposition(CTexture*& pCurrRT, CTexture* pDestRT, uint32 aaMode);

private:
	_smart_ptr<CTexture> m_pTexAreaSMAA;
	_smart_ptr<CTexture> m_pTexSearchSMAA;
	int                  m_lastFrameID;

	CFullscreenPass      m_passSMAAEdgeDetection;
	CFullscreenPass      m_passSMAABlendWeights;
	CFullscreenPass      m_passSMAANeighborhoodBlending;
	CFullscreenPass      m_passTemporalAA;
	CFullscreenPass      m_passComposition;
};
