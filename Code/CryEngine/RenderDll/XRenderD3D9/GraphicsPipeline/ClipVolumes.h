// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/PrimitiveRenderPass.h"
#include "Common/FullscreenPass.h"
//#include "Common/UtilityPasses.h"

class CClipVolumesStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_ClipVolumes;
	static const uint32                 MaxDeferredClipVolumes = BIT_STENCIL_INSIDE_CLIPVOLUME - 1; // Keep in sync with MAX_CLIPVOLUMES in DeferredShading.cfx / ShadowBlur.cfx

public:
	CClipVolumesStage(CGraphicsPipeline& graphicsPipeline);
	virtual ~CClipVolumesStage();

	CGpuBuffer* GetClipVolumeInfoBuffer()                 { return &m_clipVolumeInfoBuf; }
	CTexture*   GetClipVolumeStencilVolumeTexture() const { return m_pClipVolumeStencilVolumeTex; }
	uint32      GetClipVolumeShaderParams(const Vec4*& shaderParams) const;

	bool        IsOutdoorVisible() const                  { CRY_ASSERT(m_bClipVolumesValid); return m_bOutdoorVisible; }

	void        Init();
	void        Resize(int renderWidth, int renderHeight) final;
	void        OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater) final;
	void        Update() final;

	bool        IsStageActive(EShaderRenderingFlags flags) const final
	{
		if (flags & EShaderRenderingFlags::SHDF_FORWARD_MINIMAL)
			return false;

		return true;
	}

	void Prepare();
	void Execute();

public:
	struct SClipVolumeInfo
	{
		float data;
	};

	void GenerateClipVolumeInfo();

private:
	void Rescale(int resolutionScale, int depthScale);
	void ResizeResource(int volumeWidth, int volumeHeight, int volumeDepth);
	void FillVolumetricFogDepth();
	void ExecuteVolumetricFog();

private:
	CPrimitiveRenderPass                               m_stencilPass;
	CPrimitiveRenderPass                               m_blendValuesPass;
	CFullscreenPass                                    m_stencilResolvePass;

	CPrimitiveRenderPass                               m_volumetricStencilPass;
	std::vector<std::unique_ptr<CFullscreenPass>>      m_jitteredDepthPassArray;

	CRenderPrimitive                                   m_stencilPrimitives[MaxDeferredClipVolumes * 2];
	CRenderPrimitive                                   m_blendPrimitives[MaxDeferredClipVolumes];
	CRenderPrimitive                                   m_stencilPrimitivesVolFog[MaxDeferredClipVolumes * 2];
	
	CRY_ALIGN(16) Vec4                                 m_clipVolumeShaderParams[MaxDeferredClipVolumes];
	uint32                                             m_nShaderParamCount;

	CGpuBuffer                                         m_clipVolumeInfoBuf;

	CTexture*                                          m_pBlendValuesRT;
	CTexture*                                          m_pDepthTarget;

	CTexture*                                          m_pClipVolumeStencilVolumeTex;
	CTexture*                                          m_depthTargetVolFog;
	std::vector<ResourceViewHandle>                    m_depthTargetArrayVolFog;

	int32                                              m_filledVolumetricFogDepth;
	float                                              m_raymarchDistance;

	bool                                               m_bClipVolumesValid;
	bool                                               m_clearedBlendValuesRT;
	bool                                               m_bOutdoorVisible;
};
