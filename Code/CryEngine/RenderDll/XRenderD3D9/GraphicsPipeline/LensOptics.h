// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/PrimitiveRenderPass.h"
#include "Common/RendElements/FlareSoftOcclusionQuery.h"

class CRenderView;

class CLensOpticsStage : public CGraphicsPipelineStage
{
public:
	void      Init();
	void      Execute();

	bool      HasContent() const { return m_primitivesRendered>0; }

private:
	void      UpdateOcclusionQueries(SRenderViewInfo* pViewInfo, int viewInfoCount);

	CPrimitiveRenderPass  m_passLensOptics;
	CSoftOcclusionManager m_softOcclusionManager;

	int             m_occlusionUpdateFrame = -1;
	int             m_primitivesRendered   =  0;
};
