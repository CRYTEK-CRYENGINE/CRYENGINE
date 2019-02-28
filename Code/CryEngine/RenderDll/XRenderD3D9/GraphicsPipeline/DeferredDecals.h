// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/PrimitiveRenderPass.h"


class CDeferredDecalsStage : public CGraphicsPipelineStage
{
public:
	enum { MaxPersistentDecals = 512 };
	
public:
	CDeferredDecalsStage();
	virtual ~CDeferredDecalsStage();

	void Init() final;
	void Execute();

private:
	void ResizeDecalBuffers(size_t requiredDecalCount);
	void SetupDecalPrimitive(const SDeferredDecal& decal, CRenderPrimitive& primitive, _smart_ptr<IRenderShaderResources>& pShaderResources);

private:
	std::vector<_smart_ptr<IRenderShaderResources> > m_decalShaderResources;
	std::vector<CRenderPrimitive>                    m_decalPrimitives;
	CPrimitiveRenderPass                             m_decalPass;
};
