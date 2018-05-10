// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/ComputeRenderPass.h"
#include "Common/FullscreenPass.h"
#include "Common/SceneRenderPass.h"

class CVolumetricFogStage : public CGraphicsPipelineStage
{
public:
	struct SForwardParams
	{
		Vec4 vfSamplingParams;
		Vec4 vfDistributionParams;
		Vec4 vfScatteringParams;
		Vec4 vfScatteringBlendParams;
		Vec4 vfScatteringColor;
		Vec4 vfScatteringSecondaryColor;
		Vec4 vfHeightDensityParams;
		Vec4 vfHeightDensityRampParams;
		Vec4 vfDistanceParams;
		Vec4 vfGlobalEnvProbeParams0;
		Vec4 vfGlobalEnvProbeParams1;
	};

public:
	static const int32 ShadowCascadeNum = 3;

	static bool  IsEnabledInFrame();
	static int32 GetVolumeTextureDepthSize();
	static int32 GetVolumeTextureSize(int32 size, int32 scale);
	static float GetDepthTexcoordFromLinearDepthScaled(float linearDepthScaled, float raymarchStart, float invRaymarchDistance, float depthSlicesNum);

public:
	CVolumetricFogStage();
	virtual ~CVolumetricFogStage();

	void Init() final;
	void Update() final;

	void Execute();

	bool CreatePipelineState(const SGraphicsPipelineStateDescription& desc, CDeviceGraphicsPSOPtr& outPSO) const;

	void FillForwardParams(SForwardParams& volFogParams, bool enable = true) const;

	template<class RenderPassType>
	void        BindVolumetricFogResources(RenderPassType& pass, int32 startTexSlot, int32 trilinearClampSamplerSlot);
	const Vec4& GetGlobalEnvProbeShaderParam0() const { return m_globalEnvProbeParam0; }
	const Vec4& GetGlobalEnvProbeShaderParam1() const { return m_globalEnvProbeParam1; }
	CTexture*   GetVolumetricFogTex() const;
	CTexture*   GetGlobalEnvProbeTex0() const;
	CTexture*   GetGlobalEnvProbeTex1() const;
	void        ResetFrame();

private:
	static const int32 MaxFrameNum = 4;

private:
	bool      PreparePerPassResources(bool bOnInit);

	void      ExecuteInjectParticipatingMedia(const SScopedComputeCommandList& commandList);
	void      ExecuteVolumetricFog(const SScopedComputeCommandList& commandList);

	uint32    GetTemporalBufferId() const;
	CTexture* GetInscatterTex() const;
	CTexture* GetPrevInscatterTex() const;
	CTexture* GetDensityTex() const;
	CTexture* GetPrevDensityTex() const;

	void      GenerateLightList();
	void      GenerateFogVolumeList();

	bool      ReplaceShadowMapWithStaticShadowMap(CShadowUtils::SShadowCascades& shadowCascades, uint32 shadowCascadeSlot) const;

	bool      IsVisible() const;
	bool      IsTexturesValid() const;
	void      UpdateFrame();
	void      ExecuteDownscaleShadowmap();
	void      ExecuteBuildLightListGrid(const SScopedComputeCommandList& commandList);
	void      ExecuteDownscaledDepth(const SScopedComputeCommandList& commandList);
	void      ExecuteInjectFogDensity(const SScopedComputeCommandList& commandList);
	void      ExecuteInjectInscatteringLight(const SScopedComputeCommandList& commandList);
	void      ExecuteBlurDensityVolume(const SScopedComputeCommandList& commandList);
	void      ExecuteBlurInscatterVolume(const SScopedComputeCommandList& commandList);
	void      ExecuteTemporalReprojection(const SScopedComputeCommandList& commandList);
	void      ExecuteRaymarchVolumetricFog(const SScopedComputeCommandList& commandList);

private:
	_smart_ptr<CTexture>     m_pVolFogBufDensityColor;
	_smart_ptr<CTexture>     m_pVolFogBufDensity;
	_smart_ptr<CTexture>     m_pVolFogBufEmissive;
	_smart_ptr<CTexture>     m_pInscatteringVolume;
	_smart_ptr<CTexture>     m_pFogInscatteringVolume[2];
	_smart_ptr<CTexture>     m_pFogDensityVolume[2];
	_smart_ptr<CTexture>     m_pMaxDepth;
	_smart_ptr<CTexture>     m_pMaxDepthTemp;
	_smart_ptr<CTexture>     m_pNoiseTexture;
	_smart_ptr<CTexture>     m_pDownscaledShadow[ShadowCascadeNum];
	_smart_ptr<CTexture>     m_pDownscaledShadowTemp;
	_smart_ptr<CTexture>     m_pCloudShadowTex;
	_smart_ptr<CTexture>     m_globalEnvProveTex0;
	_smart_ptr<CTexture>     m_globalEnvProveTex1;

	CGpuBuffer               m_lightCullInfoBuf;
	CGpuBuffer               m_LightShadeInfoBuf;
	CGpuBuffer               m_lightGridBuf;
	CGpuBuffer               m_lightCountBuf;
	CGpuBuffer               m_fogVolumeCullInfoBuf;
	CGpuBuffer               m_fogVolumeInjectInfoBuf;

	CFullscreenPass          m_passDownscaleShadowmap[ShadowCascadeNum];
	CFullscreenPass          m_passDownscaleShadowmap2[ShadowCascadeNum];
	CComputeRenderPass       m_passBuildLightListGrid;
	CComputeRenderPass       m_passDownscaleDepthHorizontal;
	CComputeRenderPass       m_passDownscaleDepthVertical;
	CComputeRenderPass       m_passInjectFogDensity;
	CSceneRenderPass         m_passInjectParticleDensity;
	CComputeRenderPass       m_passInjectInscattering;
	CComputeRenderPass       m_passBlurDensityHorizontal[2];
	CComputeRenderPass       m_passBlurDensityVertical[2];
	CComputeRenderPass       m_passBlurInscatteringHorizontal[2];
	CComputeRenderPass       m_passBlurInscatteringVertical[2];
	CComputeRenderPass       m_passTemporalReprojection[2];
	CComputeRenderPass       m_passRaymarch[2];

	Vec4                     m_globalEnvProbeParam0;
	Vec4                     m_globalEnvProbeParam1;

	CDeviceResourceLayoutPtr m_pSceneRenderResourceLayout;
	CDeviceResourceSetDesc   m_sceneRenderPassResources;
	CDeviceResourceSetPtr    m_pSceneRenderPassResourceSet;
	CConstantBufferPtr       m_pSceneRenderPassCB;

	Matrix44A                m_viewProj[MaxFrameNum];
	int32                    m_cleared;
	uint32                   m_numTileLights;
	uint32                   m_numFogVolumes;
	int64                    m_frameID;
	int32                    m_tick;
	int32                    m_resourceFrameID;
};
