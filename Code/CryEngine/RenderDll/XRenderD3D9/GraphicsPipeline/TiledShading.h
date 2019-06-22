// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/ComputeRenderPass.h"
#include "Common/FullscreenPass.h"

class CTiledShadingStage : public CGraphicsPipelineStage
{
public:
	enum EExecutionMode
	{
		eDeferredMode_Off      = 0,
		eDeferredMode_Enabled  = 2,
		eDeferredMode_1Pass    = 2,
		eDeferredMode_2Pass    = 3,
		eDeferredMode_Disabled = 4
	};
	static const EGraphicsPipelineStage StageID = eStage_TiledShading;

	CTiledShadingStage(CGraphicsPipeline& graphicsPipeline);
	~CTiledShadingStage();

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRendererCVars::CV_r_DeferredShadingTiled >= CTiledShadingStage::eDeferredMode_1Pass;
	}

	void Init() final;
	void Execute();

private:
	enum EVolumeTypes
	{
		eVolumeType_Box,
		eVolumeType_Sphere,
		eVolumeType_Cone,

		eVolumeType_Count
	};

	struct SVolumeGeometry
	{
		CGpuBuffer       vertexDataBuf;
		buffer_handle_t  vertexBuffer;
		buffer_handle_t  indexBuffer;
		uint32           numVertices;
		uint32           numIndices;
	};

private:
	CGpuBuffer            m_lightVolumeInfoBuf;

	_smart_ptr<CTexture>  m_pTexGiDiff;
	_smart_ptr<CTexture>  m_pTexGiSpec;

	SVolumeGeometry       m_volumeMeshes[eVolumeType_Count];

	CConstantBufferPtr    m_pPerViewConstantBuffer = nullptr;
	CComputeRenderPass    m_passCullingShading;
	
	CFullscreenPass       m_passCopyDepth;
	CPrimitiveRenderPass  m_passLightVolumes;
	CRenderPrimitive      m_volumePasses[eVolumeType_Count * 2];  // Inside and outside of volume for each type
	uint32                m_numVolumesPerPass[eVolumeType_Count * 2];
};
