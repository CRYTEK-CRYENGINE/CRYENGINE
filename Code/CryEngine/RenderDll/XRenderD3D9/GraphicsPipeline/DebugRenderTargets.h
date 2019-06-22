// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/PrimitiveRenderPass.h"

class CDebugRenderTargetsStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_DebugRenderTargets;
	static const char*                  showRenderTargetHelp;

	CDebugRenderTargetsStage(CGraphicsPipeline& graphicsPipeline) : CGraphicsPipelineStage(graphicsPipeline) {}

	void Execute();

	void OnShowRenderTargetsCmd(SDebugRenderTargetInfo& debugInfo);

private:

	struct SRenderTargetInfo
	{
		CTexture* pTexture      = nullptr;
		Vec4      channelWeight = Vec4(1.0f);
		bool      bFiltered     = false;
		bool      bRGBKEncoded  = false;
		bool      bAliased      = false;
		int       slice         = -1;
		int       mip           = -1;
	};

	void ResetRenderTargetList();
	void ExecuteShowTargets();

private:
	CPrimitiveRenderPass           m_debugPass;
	std::vector<CRenderPrimitive>  m_debugPrimitives;
	int                            m_primitiveCount = 0;

	std::vector<SRenderTargetInfo> m_renderTargetList;
	bool                           m_bShowList;
	int                            m_columnCount;
};