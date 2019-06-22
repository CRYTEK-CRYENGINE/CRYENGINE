// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/PrimitiveRenderPass.h"


class CDeferredDecalsStage : public CGraphicsPipelineStage
{
public:
	enum { MaxPersistentDecals = 1024 };
	static const EGraphicsPipelineStage StageID = eStage_DeferredDecals;

public:
	CDeferredDecalsStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline) {}

	virtual ~CDeferredDecalsStage() {};

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRenderer::CV_r_deferredDecals > 0;
	}

	void Init() final;
	void Execute();

private:
	void ResizeDecalBuffers(size_t requiredDecalCount);
	void SetupDecalPrimitive(const SDeferredDecal& decal, CRenderPrimitive& primitive, _smart_ptr<IRenderShaderResources>& pShaderResources);

private:
	std::vector<_smart_ptr<IRenderShaderResources>> m_decalShaderResources;
	std::vector<CRenderPrimitive>                   m_decalPrimitives;
	CPrimitiveRenderPass                            m_decalPass;
};
