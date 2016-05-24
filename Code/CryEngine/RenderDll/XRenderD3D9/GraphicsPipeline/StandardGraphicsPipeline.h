// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipeline.h"
#include "Common/GraphicsPipelineStateSet.h"

class CShadowMapStage;
class CSceneGBufferStage;
class CSceneForwardStage;
class CAutoExposureStage;
class CBloomStage;
class CHeightMapAOStage;
class CScreenSpaceObscuranceStage;
class CScreenSpaceReflectionsStage;
class CScreenSpaceSSSStage;
class CMotionBlurStage;
class CDepthOfFieldStage;
class CToneMappingStage;
class CSunShaftsStage;
class CPostAAStage;
class CRenderCamera;
class CCamera;
class CComputeSkinningStage;

enum EStandardGraphicsPipelineStage
{
	// Scene stages
	eStage_ShadowMap,
	eStage_SceneGBuffer,
	eStage_SceneForward,
	// Regular stages
	eStage_HeightMapAO,
	eStage_ScreenSpaceObscurance,
	eStage_ScreenSpaceReflections,
	eStage_ScreenSpaceSSS,
	eStage_MotionBlur,
	eStage_DepthOfField,
	eStage_AutoExposure,
	eStage_Bloom,
	eStage_ToneMapping,
	eStage_Sunshafts,
	eStage_PostAA,
	eStage_ComputeSkinning,
};

class CStandardGraphicsPipeline : public CGraphicsPipeline
{
public:
	struct SViewInfo
	{
		SViewInfo(const CRenderCamera& renderCam, const CCamera& ccamera)
			: renderCamera(renderCam)
			, camera(ccamera)
			, m_CameraProjZeroMatrix(IDENTITY)
			, m_CameraProjMatrix(IDENTITY)
			, m_CameraProjNearestMatrix(IDENTITY)
			, m_ProjMatrix(IDENTITY)
			, m_InvCameraProjMatrix(IDENTITY)
			, m_PrevCameraProjMatrix(IDENTITY)
			, m_PrevCameraProjNearestMatrix(IDENTITY)
		{}

		const CRenderCamera& renderCamera;
		const CCamera&       camera;
		const Plane*         pFrustumPlanes;

		Matrix44A            m_CameraProjZeroMatrix;
		Matrix44A            m_CameraProjMatrix;
		Matrix44A            m_CameraProjNearestMatrix;
		Matrix44A            m_ProjMatrix;
		Matrix44A            m_InvCameraProjMatrix;
		Matrix44A            m_PrevCameraProjMatrix;
		Matrix44A            m_PrevCameraProjNearestMatrix;

		D3D11_VIEWPORT       viewport;
		Vec4                 downscaleFactor;

		bool                 bReverseDepth : 1;
		bool                 bMirrorCull   : 1;
	};

	CStandardGraphicsPipeline()
		: m_bInitialized(false)
		, m_bUtilityPassesInitialized(false)
		, m_numInvalidDrawcalls(0)
	{}

	virtual void Init() override;
	virtual void ReleaseBuffers() override;
	virtual void Prepare(CRenderView* pRenderView) override;
	virtual void Execute() override;

	bool         CreatePipelineStates(DevicePipelineStatesArray* pStateArray,
	                                  SGraphicsPipelineStateDescription stateDesc,
	                                  CGraphicsPipelineStateLocalCache* pStateCache);
	void UpdatePerViewConstantBuffer(const RECT* pCustomViewport = NULL);
	void UpdatePerViewConstantBuffer(const SViewInfo& viewInfo, CConstantBufferPtr& pPerViewBuffer);

	// Partial pipeline functions, will be removed once the entire pipeline is implemented in Execute()
	void                      RenderScreenSpaceSSS(CTexture* pIrradianceTex);
	void                      RenderPostAA();

	void                      SwitchToLegacyPipeline();
	void                      SwitchFromLegacyPipeline();

	uint32                    IncrementNumInvalidDrawcalls()           { return ++m_numInvalidDrawcalls; }
	uint32                    GetNumInvalidDrawcalls() const           { return m_numInvalidDrawcalls;   }

	CConstantBufferPtr        GetPerViewConstantBuffer()         const { return m_pPerViewConstantBuffer; }
	CDeviceResourceSetPtr     GetDefaultMaterialResources()      const { return m_pDefaultMaterialResources; }
	std::array<int, EFSS_MAX> GetDefaultMaterialSamplers()       const;
	CDeviceResourceSetPtr     GetDefaultInstanceExtraResources() const { return m_pDefaultInstanceExtraResources; }

	CRenderView*              GetCurrentRenderView() const             { return m_pCurrentRenderView; };
	CShadowMapStage*          GetShadowStage() const                   { return m_pShadowMapStage; }
	CComputeSkinningStage*    GetComputeSkinningStage() const          { return m_pComputeSkinningStage;  }

public:
	static void ApplyShaderQuality(CDeviceGraphicsPSODesc& psoDesc, const SShaderProfile& shaderProfile);

private:
	CShadowMapStage*              m_pShadowMapStage;
	CSceneGBufferStage*           m_pSceneGBufferStage;
	CSceneForwardStage*           m_pSceneForwardStage;
	CAutoExposureStage*           m_pAutoExposureStage;
	CBloomStage*                  m_pBloomStage;
	CHeightMapAOStage*            m_pHeightMapAOStage;
	CScreenSpaceObscuranceStage*  m_pScreenSpaceObscuranceStage;
	CScreenSpaceReflectionsStage* m_pScreenSpaceReflectionsStage;
	CScreenSpaceSSSStage*         m_pScreenSpaceSSSStage;
	CMotionBlurStage*             m_pMotionBlurStage;
	CDepthOfFieldStage*           m_pDepthOfFieldStage;
	CSunShaftsStage*              m_pSunShaftsStage;
	CToneMappingStage*            m_pToneMappingStage;
	CPostAAStage*                 m_pPostAAStage;
	CComputeSkinningStage*        m_pComputeSkinningStage;

	CConstantBufferPtr            m_pPerViewConstantBuffer;
	CDeviceResourceSetPtr         m_pDefaultMaterialResources;
	CDeviceResourceSetPtr         m_pDefaultInstanceExtraResources;

	CRenderView*                  m_pCurrentRenderView;

	bool                          m_bInitialized;
	bool                          m_bUtilityPassesInitialized;

private:
	void ExecuteHDRPostProcessing();

	uint32 m_numInvalidDrawcalls;
};
