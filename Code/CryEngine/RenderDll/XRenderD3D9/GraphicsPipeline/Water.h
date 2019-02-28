// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/UtilityPasses.h"
#include "Common/FullscreenPass.h"
#include "Common/PrimitiveRenderPass.h"
#include "Common/SceneRenderPass.h"
#include "SceneGBuffer.h"

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
		ePerInstanceTexture_Foam              = 14,
		ePerInstanceTexture_Displacement      = 15,
		ePerInstanceTexture_RainRipple        = 16,

		ePerInstanceTexture_Count
	};

	enum EPerPassTexture
	{
		ePerPassTexture_VolFogShadow          = 21,
		ePerPassTexture_VolumetricFog         = 22,
		ePerPassTexture_VolFogGlobalEnvProbe0 = 23,
		ePerPassTexture_VolFogGlobalEnvProbe1 = 24,

		ePerPassTexture_PerlinNoiseMap        = 25,
		ePerPassTexture_Jitter                = 26,

		ePerPassTexture_WaterRipple           = 27,
		ePerPassTexture_WaterNormal           = 28,
		ePerPassTexture_WaterGloss            = 29,
		ePerPassTexture_Refraction            = 30,
		ePerPassTexture_Reflection            = 31,

		ePerPassTexture_SceneDepth            = 32,
		
		ePerPassTexture_ShadowMap0            = 33,
		ePerPassTexture_ShadowMap1            = 34,
		ePerPassTexture_ShadowMap2            = 35,
		ePerPassTexture_ShadowMap3            = 36,

		ePerPassTexture_Count
	};
	static_assert(int32(ePerPassTexture_PerlinNoiseMap) == int32(CSceneGBufferStage::ePerPassTexture_PerlinNoiseMap), "Per instance texture count must be same in water stage to ensure using same resource layout.");
	static_assert(int32(ePerPassTexture_SceneDepth) == int32(CSceneGBufferStage::ePerPassTexture_SceneLinearDepth), "Per instance texture count must be same in water stage to ensure using same resource layout.");

	enum EPerPassSampler
	{
		ePerPassSampler_Aniso16xWrap    = 0,
		ePerPassSampler_Aniso16xClamp   = 1,

		ePerPassSampler_PointWrap       = 8,
		ePerPassSampler_PointClamp      = 9,

		ePerPassSampler_LinearClampComp = 10,
		ePerPassSampler_LinearMirror    = 11,

		ePerPassSampler_Count,
	};
	// NOTE: DXOrbis only supports 12 sampler state slots at this time, don't use s12 or higher if DXOrbis support is desired!
	static_assert(ePerPassSampler_Count <= 12, "Too many sampler states for DXOrbis");

	static const uint32 RainRippleTexCount = 24;

public:
	static bool UpdateCausticsGrid(N3DEngineCommon::SCausticInfo& causticInfo, bool& bVertexUpdated, CRenderer* pRenderer);

public:
	CWaterStage();

	void Init() final;
	void Update() final;
	void Prepare();
	void Resize(int renderWidth, int renderHeight) override final;
	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		if (flags & EShaderRenderingFlags::SHDF_FORWARD_MINIMAL)
			return false;

		return true;
	}

	void  ExecuteWaterVolumeCaustics();
	void  ExecuteDeferredWaterVolumeCaustics();
	void  ExecuteDeferredOceanCaustics();
	void  ExecuteWaterFogVolumeBeforeTransparent();
	void  Execute();

	const CDeviceResourceSetDesc& GetDefaultPerInstanceResources()   const { return m_defaultPerInstanceResources; }
	const CDeviceResourceSetPtr&  GetDefaultPerInstanceResourceSet() const { return m_pDefaultPerInstanceResourceSet; }

	bool  CreatePipelineStates(uint32 passMask, DevicePipelineStatesArray& pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);
	bool  CreatePipelineState(CDeviceGraphicsPSOPtr& outPSO, const SGraphicsPipelineStateDescription& desc, EPass passID, std::function<void(CDeviceGraphicsPSODesc& psoDesc)> modifier);

	bool  IsNormalGenActive() const { return m_bWaterNormalGen; }

private:
	CDeviceResourceLayoutPtr CreateScenePassLayout(const CDeviceResourceSetDesc& perPassResources);
	bool  PrepareDefaultPerInstanceResources();
	bool  SetAndBuildPerPassResources(bool bOnInit, EPass passId);
	void  UpdatePerPassResources(EPass passId);

	void  ExecuteWaterNormalGen();
	void  ExecuteOceanMaskGen();
	void  ExecuteWaterVolumeCausticsGen(N3DEngineCommon::SCausticInfo& causticInfo);
	void  ExecuteReflection();

	void  ExecuteSceneRenderPass(CSceneRenderPass& pass, ERenderListID renderList);

	int32 GetCurrentFrameID(const int32 frameID) const;
	int32 GetPreviousFrameID(const int32 frameID) const;

private:
	_smart_ptr<CTexture>                      m_pFoamTex;
	_smart_ptr<CTexture>                      m_pJitterTex;
	_smart_ptr<CTexture>                      m_pWaterGlossTex;
	_smart_ptr<CTexture>                      m_pOceanWavesTex;
	_smart_ptr<CTexture>                      m_pOceanCausticsTex;
	_smart_ptr<CTexture>                      m_pOceanMaskTex = nullptr;

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
	CClearRegionPass                          m_passWaterReflectionClear;
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

	uint64 m_frameIdWaterSim;
	Vec4   m_oceanAnimationParams[2];

	int32 m_aniso16xClampSampler;
	int32 m_aniso16xWrapSampler;
	int32 m_linearCompareClampSampler;
	int32 m_linearMirrorSampler;

	bool              m_bWaterNormalGen;
	bool              m_bOceanMaskGen;

	static constexpr int32 nGridSize = 64;
};
