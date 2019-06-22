// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/PrimitiveRenderPass.h"

struct SColorGradingMergeParams;

// Produces final color chart texture: merges (if any) layers + uses params from EPostEffectID::ColorGrading
// Final result can be nullptr
class CColorGradingStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_ColorGrading;

	CColorGradingStage(CGraphicsPipeline& graphicsPipeline);

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRenderer::CV_r_colorgrading > 0 && CRenderer::CV_r_colorgrading_charts;
	}

	virtual void Init() final;
	virtual void Execute();

	// Result texture of this pass for further usage
	CTexture* GetColorChart() const { return m_pChartToUse; }

private:
	void PreparePrimitives(const SColorGradingMergeParams& mergeParams);
	bool AreRenderPassesDirty() const;

	_smart_ptr<CTexture> m_pChartIdentity;

	// Merge all layers to one
	CPrimitiveRenderPass          m_mergeChartsPass;
	std::vector<CRenderPrimitive> m_mergeChartsPrimitives;
	_smart_ptr<CTexture>          m_pMergedLayer;

	// Merge m_pMergedLayer (or m_pChartIdentity) with constants
	CPrimitiveRenderPass m_colorGradingPass;
	buffer_handle_t      m_slicesVertexBuffer;
	CRenderPrimitive     m_colorGradingPrimitive;
	_smart_ptr<CTexture> m_pFinalMergedTexture;

	// Result texture, for use in CPostAAStage
	_smart_ptr<CTexture> m_pChartToUse;
};
