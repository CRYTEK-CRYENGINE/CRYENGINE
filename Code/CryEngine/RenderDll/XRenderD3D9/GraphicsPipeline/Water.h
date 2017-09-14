// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/UtilityPasses.h"
#include "Common/FullscreenPass.h"
#include "Common/PrimitiveRenderPass.h"
#include "Common/SceneRenderPass.h"

class CWaterStage : public CGraphicsPipelineStage
{
public:
	enum EPass
	{
		ePass_ReflectionGen = 0,
		ePass_FogVolume,
		ePass_WaterSurface,
		ePass_CausticsGen,
		ePass_OceanMaskGen,

		ePass_Count,
	};
	static_assert(ePass_Count <= MAX_PIPELINE_SCENE_STAGE_PASSES, "Too many passes in one graphics pipeline stage");

	enum EPassMask
	{
		ePassMask_ReflectionGen = BIT(ePass_ReflectionGen),
		ePassMask_FogVolume     = BIT(ePass_FogVolume),
		ePassMask_WaterSurface  = BIT(ePass_WaterSurface),
		ePassMask_CausticsGen   = BIT(ePass_CausticsGen),
		ePassMask_OceanMaskGen  = BIT(ePass_OceanMaskGen),
	};

	// default for water volume and ocean
	enum EPerInstanceTexture
	{
		ePerInstanceTexture_PerlinNoise = 14,
		ePerInstanceTexture_Jitter      = 15,

		ePerInstanceTexture_Count
	};

	enum EPerPassTexture
	{
		ePerPassTexture_WaterGloss            = 16,
		ePerPassTexture_Foam                  = 17,
		ePerPassTexture_RainRipple            = 18,

		ePerPassTexture_ShadowMap0            = 19,
		ePerPassTexture_ShadowMap1            = 20,
		ePerPassTexture_ShadowMap2            = 21,
		ePerPassTexture_ShadowMap3            = 22,

		ePerPassTexture_VolFogShadow          = 23,
		ePerPassTexture_VolumetricFog         = 24,
		ePerPassTexture_VolFogGlobalEnvProbe0 = 25,
		ePerPassTexture_VolFogGlobalEnvProbe1 = 26,

		ePerPassTexture_WaterRipple           = 27,
		ePerPassTexture_WaterNormal           = 28,
		ePerPassTexture_SceneDepth            = 29,
		ePerPassTexture_Refraction            = 30,
		ePerPassTexture_Reflection            = 31,

		ePerPassTexture_Count
	};
	// NOTE: DXOrbis only supports 32 shader slots at this time, don't use t32 or higher if DXOrbis support is desired!
	static_assert(ePerPassTexture_Count <= 32, "Bind slot too high for DXOrbis");

	enum EPerPassSampler
	{
		ePerPassSampler_Aniso16xWrap = 1,

		ePerPassSampler_PointWrap    = EFSS_MAX,
		ePerPassSampler_PointClamp,

		ePerPassSampler_Aniso16xClamp,
		ePerPassSampler_LinearClampComp,

		ePerPassSampler_Count,
	};
	// NOTE: DXOrbis only supports 12 sampler state slots at this time, don't use s12 or higher if DXOrbis support is desired!
	static_assert(ePerPassSampler_Count <= 12, "Too many sampler states for DXOrbis");

	static const uint32 RainRippleTexCount = 24;

public:
	static bool UpdateCausticsGrid(N3DEngineCommon::SCausticInfo& causticInfo, bool& bVertexUpdated, CRenderer* pRenderer);

public:
	CWaterStage();

	void                         Init() override;
	void                         Prepare(CRenderView* pRenderView) override;

	void                         ExecuteWaterVolumeCaustics();
	void                         ExecuteDeferredWaterVolumeCaustics(bool bTiledDeferredShading);
	void                         ExecuteDeferredOceanCaustics();
	void                         ExecuteWaterFogVolumeBeforeTransparent();
	void                         Execute();

	const CDeviceResourceSetDesc& GetDefaultPerInstanceResources()   const { return m_defaultPerInstanceResources; }
	const CDeviceResourceSetPtr&  GetDefaultPerInstanceResourceSet() const { return m_pDefaultPerInstanceResourceSet; }

	bool                         CreatePipelineStates(uint32 passMask, DevicePipelineStatesArray& pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);
	bool                         CreatePipelineState(CDeviceGraphicsPSOPtr& outPSO, const SGraphicsPipelineStateDescription& desc, EPass passID, std::function<void(CDeviceGraphicsPSODesc& psoDesc)> modifier);

	bool                         IsNormalGenActive() const { return m_bWaterNormalGen; }

private:
	bool  PrepareResourceLayout();
	bool  PrepareDefaultPerInstanceResources();
	bool  SetAndBuildPerPassResources(CRenderView* RESTRICT_POINTER pRenderView, bool bOnInit, EPass passId);
	void  UpdatePerPassResources(CRenderView& renderView, EPass passId);

	void  ExecuteWaterNormalGen();
	void  ExecuteOceanMaskGen(CRenderView* pRenderView);
	void  ExecuteWaterVolumeCausticsGen(N3DEngineCommon::SCausticInfo& causticInfo, CRenderView* pRenderView);
	void  ExecuteReflection(CRenderView* pRenderView);

	void  ExecuteSceneRenderPass(CRenderView* pRenderView, CSceneRenderPass& pass, ERenderListID renderList);

	int32 GetCurrentFrameID(const int32 frameID) const;
	int32 GetPreviousFrameID(const int32 frameID) const;

private:
	_smart_ptr<CTexture>                      m_pFoamTex;
	_smart_ptr<CTexture>                      m_pPerlinNoiseTex;
	_smart_ptr<CTexture>                      m_pJitterTex;
	_smart_ptr<CTexture>                      m_pWaterGlossTex;
	_smart_ptr<CTexture>                      m_pOceanWavesTex;
	_smart_ptr<CTexture>                      m_pOceanCausticsTex;
	_smart_ptr<CTexture>                      m_pOceanMaskTex;

	std::array<_smart_ptr<CTexture>, RainRippleTexCount> m_pRainRippleTex;
	uint32                                               m_rainRippleTexIndex;

	CFullscreenPass                           m_passWaterNormalGen;
	CMipmapGenPass                            m_passWaterNormalMipmapGen;
	CSceneRenderPass                          m_passOceanMaskGen;
	CSceneRenderPass                          m_passWaterCausticsSrcGen;
	CFullscreenPass                           m_passWaterCausticsDilation;
	CGaussianBlurPass                         m_passBlurWaterCausticsGen0;
	CPrimitiveRenderPass                      m_passRenderCausticsGrid;
	CGaussianBlurPass                         m_passBlurWaterCausticsGen1;
	CFullscreenPass                           m_passDeferredWaterVolumeCaustics;
	CPrimitiveRenderPass                      m_passDeferredOceanCausticsStencil;
	CFullscreenPass                           m_passDeferredOceanCaustics;
	CSceneRenderPass                          m_passWaterFogVolumeBeforeWater;
	CStretchRectPass                          m_passCopySceneTargetReflection;
	CSceneRenderPass                          m_passWaterReflectionGen;
	CMipmapGenPass                            m_passWaterReflectionMipmapGen;
	CStretchRectPass                          m_passCopySceneTarget;
	CSceneRenderPass                          m_passWaterSurface;
	CSceneRenderPass                          m_passWaterFogVolumeAfterWater;

	CDeviceResourceLayoutPtr                  m_pResourceLayout;
	CDeviceResourceSetDesc                    m_defaultPerInstanceResources;
	CDeviceResourceSetPtr                     m_pDefaultPerInstanceResourceSet;
	CDeviceResourceSetDesc                    m_perPassResources[ePass_Count];
	CDeviceResourceSetPtr                     m_pPerPassResourceSets[ePass_Count];
	CConstantBufferPtr                        m_pPerPassCB[ePass_Count];

	CRenderPrimitive                          m_causticsGridPrimitive;
	CRenderPrimitive                          m_deferredOceanStencilPrimitive[2];

	int32  m_frameIdWaterSim;
	Vec4   m_oceanAnimationParams[2];

	bool              m_bWaterNormalGen;
};
