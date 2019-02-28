// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/SceneRenderPass.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"
#include "SceneGBuffer.h"

class CRESky;
class CREHDRSky;

class CSceneForwardStage : public CGraphicsPipelineStage
{
public:
	struct SCloudShadingParams
	{
		Vec4 CloudShadingColorSun;
		Vec4 CloudShadingColorSky;
	};

	enum EPerPassTexture
	{
		ePerPassTexture_PerlinNoiseMap   = CSceneGBufferStage::ePerPassTexture_PerlinNoiseMap,
		ePerPassTexture_TerrainElevMap   = CSceneGBufferStage::ePerPassTexture_TerrainElevMap,
		ePerPassTexture_WindGrid         = CSceneGBufferStage::ePerPassTexture_WindGrid,
		ePerPassTexture_TerrainNormMap   = CSceneGBufferStage::ePerPassTexture_TerrainNormMap,
		ePerPassTexture_TerrainBaseMap   = CSceneGBufferStage::ePerPassTexture_TerrainBaseMap,
		ePerPassTexture_NormalsFitting   = CSceneGBufferStage::ePerPassTexture_NormalsFitting,
		ePerPassTexture_DissolveNoise    = CSceneGBufferStage::ePerPassTexture_DissolveNoise,
		ePerPassTexture_SceneLinearDepth = CSceneGBufferStage::ePerPassTexture_SceneLinearDepth,
	};

public:
	enum EPass
	{
		ePass_Forward = 0,
		ePass_ForwardRecursive,
	};

public:
	CSceneForwardStage();

	void Init() final;
	void Update() final;

	bool         CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);
	bool         CreatePipelineState(const SGraphicsPipelineStateDescription& desc,
	                                 CDeviceGraphicsPSOPtr& outPSO,
	                                 EPass passId = ePass_Forward,
	                                 std::function<void(CDeviceGraphicsPSODesc& psoDesc, const SGraphicsPipelineStateDescription& desc)> customState = nullptr);

	void         ExecuteOpaque();
	void         ExecuteTransparentBelowWater();
	void         ExecuteTransparentAboveWater();
	void         ExecuteTransparentDepthFixup();
	void         ExecuteTransparentLoRes(int subRes);
	void         ExecuteAfterPostProcessHDR();
	void         ExecuteAfterPostProcessLDR();
	void         ExecuteMinimum(CTexture* pColorTex, CTexture* pDepthTex);

	void         SetSkyRE(CRESky* pSkyRE, CREHDRSky* pHDRSkyRE);

	void FillCloudShadingParams(SCloudShadingParams& cloudParams, bool enable = true) const;

private:
	bool PreparePerPassResources(bool bOnInit, bool bShadowMask = true, bool bFog = true);
	void ExecuteTransparent(bool bBelowWater);

	void SetHDRSkyParameters();
	void ExecuteSky(CTexture* pColorTex, CTexture* pDepthTex);


private:
	_smart_ptr<CTexture> m_pSkyDomeTextureMie;
	_smart_ptr<CTexture> m_pSkyDomeTextureRayleigh;
	_smart_ptr<CTexture> m_pSkyMoonTex;

	CDeviceResourceLayoutPtr m_pOpaqueResourceLayout;
	CDeviceResourceLayoutPtr m_pTransparentResourceLayout;
	CDeviceResourceLayoutPtr m_pEyeOverlayResourceLayout;

	CDeviceResourceSetDesc   m_opaquePassResources;
	CDeviceResourceSetPtr    m_pOpaquePassResourceSet;
	CDeviceResourceSetDesc   m_transparentPassResources;
	CDeviceResourceSetPtr    m_pTransparentPassResourceSet;
	CDeviceResourceSetDesc   m_eyeOverlayPassResources;
	CDeviceResourceSetPtr    m_pEyeOverlayPassResourceSet;
	CConstantBufferPtr       m_pPerPassCB;

	CSceneRenderPass         m_forwardOpaquePass;
	CSceneRenderPass         m_forwardOverlayPass;
	CSceneRenderPass         m_forwardTransparentBWPass;
	CSceneRenderPass         m_forwardTransparentAWPass;
	CSceneRenderPass         m_forwardTransparentLoResPass;
	CSceneRenderPass         m_forwardHDRPass;
	CSceneRenderPass         m_forwardLDRPass;
	CSceneRenderPass         m_forwardEyeOverlayPass;

	CSceneRenderPass         m_forwardOpaqueRecursivePass;
	CSceneRenderPass         m_forwardOverlayRecursivePass;
	CSceneRenderPass         m_forwardTransparentRecursivePass;

	CStretchRectPass         m_copySceneTargetBWPass;
	CStretchRectPass         m_copySceneTargetAWPass;

	CFullscreenPass           m_depthFixupPass;
	CFullscreenPass           m_depthCopyPass;
	CNearestDepthUpsamplePass m_depthUpscalePass;
	CFullscreenPass           m_skyPass;
	CRenderPrimitive          m_starsPrimitive;
	CPrimitiveRenderPass      m_starsPass;
	CRESky*                   m_pSkyRE = nullptr;
	CREHDRSky*                m_pHDRSkyRE = nullptr;
	Vec4                      m_paramMoonTexGenRight;
	Vec4                      m_paramMoonTexGenUp;
	Vec4                      m_paramMoonDirSize;
};
