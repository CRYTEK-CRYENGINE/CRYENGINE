// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/SceneRenderPass.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CRESky;
class CREHDRSky;

class CSceneForwardStage : public CGraphicsPipelineStage
{
	enum EPerPassTexture
	{
		ePerPassTexture_PerlinNoiseMap = 25,
		ePerPassTexture_TerrainElevMap,
		ePerPassTexture_WindGrid,
		ePerPassTexture_TerrainNormMap,
		ePerPassTexture_TerrainBaseMap,
		ePerPassTexture_NormalsFitting,
		ePerPassTexture_DissolveNoise,

		ePerPassTexture_Count
	};

public:
	enum EPass
	{
		ePass_Forward = 0,
		ePass_ForwardRecursive,
	};

public:
	CSceneForwardStage();

	virtual void Init() override;

	bool         CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);
	bool         CreatePipelineState(const SGraphicsPipelineStateDescription& desc,
	                                 CDeviceGraphicsPSOPtr& outPSO,
	                                 EPass passId = ePass_Forward,
	                                 std::function<void(CDeviceGraphicsPSODesc& psoDesc, const SGraphicsPipelineStateDescription& desc)> customState = nullptr);

	void         Execute_Opaque();
	void         Execute_TransparentBelowWater();
	void         Execute_TransparentAboveWater();
	void         Execute_AfterPostProcess();
	void         Execute_Minimum();

	void         SetSkyRE(CRESky* pSkyRE, CREHDRSky* pHDRSkyRE);

private:
	bool PreparePerPassResources(CRenderView* pRenderView, bool bOnInit, bool bShadowMask = true, bool bFog = true);
	void Execute_Transparent(bool bBelowWater);

	void SetupHDRSkyParameters();
	void Execute_SkyPass();


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
	CSceneRenderPass         m_forwardLDRPass;
	CSceneRenderPass         m_forwardEyeOverlayPass;

	CSceneRenderPass         m_forwardOpaqueRecursivePass;
	CSceneRenderPass         m_forwardOverlayRecursivePass;
	CSceneRenderPass         m_forwardTransparentRecursivePass;

	CStretchRectPass         m_copySceneTargetBWPass;
	CStretchRectPass         m_copySceneTargetAWPass;

	CFullscreenPass          m_skyPass;
	CRenderPrimitive         m_starsPrimitive;
	CPrimitiveRenderPass     m_starsPass;
	CRESky*                  m_pSkyRE = nullptr;
	CREHDRSky*               m_pHDRSkyRE = nullptr;
	Vec4                     m_paramMoonTexGenRight;
	Vec4                     m_paramMoonTexGenUp;
	Vec4                     m_paramMoonDirSize;
};
