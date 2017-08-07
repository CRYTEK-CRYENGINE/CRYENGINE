// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/PrimitiveRenderPass.h"


class CDeferredDecalsStage : public CGraphicsPipelineStage
{
public:
	enum { kMaxDeferredDecals = 256 };
	
public:
	CDeferredDecalsStage();
	virtual ~CDeferredDecalsStage();

	void Init();
	void Prepare(CRenderView* pRenderView) final;
	void Execute();

private:
	void SetupDecalPrimitive(const SDeferredDecal& decal, CRenderPrimitive& primitive, _smart_ptr<IRenderShaderResources>& pShaderResources);

private:
	_smart_ptr<IRenderShaderResources> m_decalShaderResources[kMaxDeferredDecals];
	CRenderPrimitive                   m_decalPrimitives[kMaxDeferredDecals];
	CPrimitiveRenderPass               m_decalPass;
};
