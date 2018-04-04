// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CPostAAStage : public CGraphicsPipelineStage
{
public:
	void Init();
	void Execute();

private:
	void ApplySMAA(CTexture*& pCurrRT);
	void ApplyTemporalAA(CTexture*& pCurrRT, CTexture*& pMgpuRT, uint32 aaMode);
	void DoFinalComposition(CTexture*& pCurrRT, uint32 aaMode);

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
