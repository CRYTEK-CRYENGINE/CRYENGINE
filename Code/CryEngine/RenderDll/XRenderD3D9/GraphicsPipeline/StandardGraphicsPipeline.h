// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipeline.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "Common/TypedConstantBuffer.h"

class CShadowMapStage;
class CSceneGBufferStage;
class CSceneForwardStage;
class CAutoExposureStage;
class CBloomStage;
class CHeightMapAOStage;
class CScreenSpaceObscuranceStage;
class CScreenSpaceReflectionsStage;
class CScreenSpaceSSSStage;
class CVolumetricFogStage;
class CFogStage;
class CVolumetricCloudsStage;
class CWaterStage;
class CWaterRipplesStage;
class CMotionBlurStage;
class CDepthOfFieldStage;
class CToneMappingStage;
class CSunShaftsStage;
class CPostAAStage;
class CClipVolumesStage;
class CDeferredDecalsStage;
class CShadowMaskStage;
class CComputeSkinningStage;
class CGpuParticlesStage;
class CTiledShadingStage;
class CColorGradingStage;
class CSceneCustomStage;
class CLensOpticsStage;
class CPostEffectStage;
class CRainStage;
class CSnowStage;
class COmniCameraStage;
class CDepthReadbackStage;
class CMobileCompositionStage;
class CRenderCamera;
class CCamera;

enum EStandardGraphicsPipelineStage
{
	// Scene stages
	eStage_ShadowMap,
	eStage_SceneGBuffer,
	eStage_SceneForward,
	eStage_SceneCustom,

	// Regular stages supporting async compute
	eStage_FIRST_ASYNC_COMPUTE,
	eStage_ComputeSkinning = eStage_FIRST_ASYNC_COMPUTE,
	eStage_GpuParticles,
	eStage_TiledShading,
	eStage_VolumetricClouds,

	// Regular stages
	eStage_HeightMapAO,
	eStage_ScreenSpaceObscurance,
	eStage_ScreenSpaceReflections,
	eStage_ScreenSpaceSSS,
	eStage_VolumetricFog,
	eStage_Fog,
	eStage_WaterRipples,
	eStage_Water,
	eStage_MotionBlur,
	eStage_DepthOfField,
	eStage_AutoExposure,
	eStage_Bloom,
	eStage_ColorGrading,
	eStage_ToneMapping,
	eStage_Sunshafts,
	eStage_PostAA,
	eStage_ClipVolumes,
	eStage_DeferredDecals,
	eStage_ShadowMask,
	eStage_LensOptics,
	eStage_PostEffet,
	eStage_Rain,
	eStage_Snow,
	eStage_DepthReadback,
	eStage_MobileComposition,
	eStage_OmniCamera,

	eStage_Count
};

class CStandardGraphicsPipeline : public CGraphicsPipeline
{
public:
	struct SViewInfo
	{
		enum eFlags
		{
			eFlags_ReverseDepth           = BIT(0),
			eFlags_MirrorCull             = BIT(1),
			eFlags_SubpixelShift          = BIT(2),
			eFlags_MirrorCamera           = BIT(3),
			eFlags_ObliqueFrustumClipping = BIT(4),

			eFlags_None                   = 0
		};

		const CRenderCamera* pRenderCamera;
		const CCamera*       pCamera;
		const Plane*         pFrustumPlanes;

		Matrix44A            cameraProjZeroMatrix;
		Matrix44A            cameraProjMatrix;
		Matrix44A            cameraProjNearestMatrix;
		Matrix44A            projMatrix;
		Matrix44A            viewMatrix;
		Matrix44A            invCameraProjMatrix;
		Matrix44A            invViewMatrix;
		Matrix44A            prevCameraMatrix;
		Matrix44A            prevCameraProjMatrix;
		Matrix44A            prevCameraProjNearestMatrix;

		SViewport            viewport;
		Vec4                 downscaleFactor;

		eFlags               flags;

		SViewInfo();
		void     SetCamera(const CRenderCamera& renderCam, const CCamera& cam, const CCamera& previousCam, Vec2 subpixelShift, float drawNearestFov, float drawNearestFarPlane, Plane obliqueClippingPlane = Plane());
		void     SetLegacyCamera(CD3D9Renderer* pRenderer, const Matrix44& previousFrameCameraMatrix);

		Matrix44 GetNearestProjection(float nearestFOV, float farPlane, Vec2 subpixelShift);
		void     ExtractViewMatrices(const CCamera& cam, Matrix44& view, Matrix44& viewZero, Matrix44& invView) const;
	};

	CStandardGraphicsPipeline();

	virtual void Init() override;
	virtual void Prepare(CRenderView* pRenderView, EShaderRenderingFlags renderingFlags) override;
	virtual void Execute() override;
	virtual void ExecuteBillboards();
	void         ExecuteMinimumForwardShading();

	bool         CreatePipelineStates(DevicePipelineStatesArray* pStateArray,
	                                  SGraphicsPipelineStateDescription stateDesc,
	                                  CGraphicsPipelineStateLocalCache* pStateCache);
	void UpdateMainViewConstantBuffer();
	void UpdatePerViewConstantBuffer(const SViewInfo* pViewInfo, int viewInfoCount, CConstantBufferPtr pPerViewBuffer);
	bool FillCommonScenePassStates(const SGraphicsPipelineStateDescription& inputDesc, CDeviceGraphicsPSODesc& psoDesc);
	CDeviceResourceLayoutPtr CreateScenePassLayout(const CDeviceResourceSetDesc& perPassResources);

	// Partial pipeline functions, will be removed once the entire pipeline is implemented in Execute()
	void   RenderTiledShading();
	void   RenderScreenSpaceSSS(CTexture* pIrradianceTex);
	void   RenderPostAA();
	void   ExecuteAnisotropicVerticalBlur(CTexture* pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly);

	static void   SwitchToLegacyPipeline();
	static void   SwitchFromLegacyPipeline();

	uint32 IncrementNumInvalidDrawcalls(int count) { return CryInterlockedAdd((volatile int*)&m_numInvalidDrawcalls, count); }
	uint32 GetNumInvalidDrawcalls() const          { return m_numInvalidDrawcalls;   }

	int GetViewInfoCount() const;
	int GetViewInfo(SViewInfo viewInfo[2], const D3DViewPort * pCustomViewport = NULL);
	uint32 GetRenderFlags() const { return m_renderingFlags; }

	CConstantBufferPtr                       GetMainViewConstantBuffer()                { return m_mainViewConstantBuffer.GetDeviceConstantBuffer(); }
	const CDeviceResourceSetDesc&            GetDefaultMaterialBindPoints()       const { return m_defaultMaterialBindPoints; }
	std::array<SamplerStateHandle, EFSS_MAX> GetDefaultMaterialSamplers()         const;
	const CDeviceResourceSetDesc&            GetDefaultInstanceExtraResources()   const { return m_defaultInstanceExtraResources; }
	CDeviceResourceSetPtr                    GetDefaultInstanceExtraResourceSet() const { return m_pDefaultInstanceExtraResourceSet; }

	CRenderView*              GetCurrentRenderView()             const { return m_pCurrentRenderView; };
	CSceneGBufferStage*       GetGBufferStage()                        { return m_pSceneGBufferStage; }
	CShadowMapStage*          GetShadowStage()                   const { return m_pShadowMapStage; }
	CSceneForwardStage*       GetSceneForwardStage()             const { return m_pSceneForwardStage; }
	CComputeSkinningStage*    GetComputeSkinningStage()          const { return m_pComputeSkinningStage; }
	CGpuParticlesStage*       GetGpuParticlesStage()             const { return m_pGpuParticlesStage; }
	CDeferredDecalsStage*     GetDeferredDecalsStage()           const { return m_pDeferredDecalsStage; }
	CClipVolumesStage*        GetClipVolumesStage()              const { return m_pClipVolumesStage; }
	CShadowMaskStage*         GetShadowMaskStage()               const { return m_pShadowMaskStage; }
	CSceneCustomStage*        GetSceneCustomStage()              const { return m_pSceneCustomStage; }
	CFogStage*                GetFogStage()                      const { return m_pFogStage; }
	CVolumetricFogStage*      GetVolumetricFogStage()            const { return m_pVolumetricFogStage; }
	CWaterRipplesStage*       GetWaterRipplesStage()             const { return m_pWaterRipplesStage; }
	CWaterStage*              GetWaterStage()                    const { return m_pWaterStage; }
	CLensOpticsStage*         GetLensOpticsStage()               const { return m_pLensOpticsStage; }
	CDepthReadbackStage*      GetDepthReadbackStage()            const { return m_pDepthReadbackStage; }
	COmniCameraStage*         GetOmniCameraStage()               const { return m_pOmniCameraStage; }

public:
	static void ApplyShaderQuality(CDeviceGraphicsPSODesc& psoDesc, const SShaderProfile& shaderProfile);

private:
	CShadowMapStage*              m_pShadowMapStage = nullptr;
	CSceneGBufferStage*           m_pSceneGBufferStage = nullptr;
	CSceneForwardStage*           m_pSceneForwardStage = nullptr;
	CAutoExposureStage*           m_pAutoExposureStage = nullptr;
	CBloomStage*                  m_pBloomStage = nullptr;
	CHeightMapAOStage*            m_pHeightMapAOStage = nullptr;
	CScreenSpaceObscuranceStage*  m_pScreenSpaceObscuranceStage = nullptr;
	CScreenSpaceReflectionsStage* m_pScreenSpaceReflectionsStage = nullptr;
	CScreenSpaceSSSStage*         m_pScreenSpaceSSSStage = nullptr;
	CVolumetricFogStage*          m_pVolumetricFogStage = nullptr;
	CFogStage*                    m_pFogStage = nullptr;
	CVolumetricCloudsStage*       m_pVolumetricCloudsStage = nullptr;
	CWaterStage*                  m_pWaterStage = nullptr;
	CWaterRipplesStage*           m_pWaterRipplesStage = nullptr;
	CMotionBlurStage*             m_pMotionBlurStage = nullptr;
	CDepthOfFieldStage*           m_pDepthOfFieldStage = nullptr;
	CSunShaftsStage*              m_pSunShaftsStage = nullptr;
	CToneMappingStage*            m_pToneMappingStage = nullptr;
	CPostAAStage*                 m_pPostAAStage = nullptr;
	CComputeSkinningStage*        m_pComputeSkinningStage = nullptr;
	CGpuParticlesStage*           m_pGpuParticlesStage = nullptr;
	CDeferredDecalsStage*         m_pDeferredDecalsStage = nullptr;
	CClipVolumesStage*            m_pClipVolumesStage = nullptr;
	CShadowMaskStage*             m_pShadowMaskStage = nullptr;
	CTiledShadingStage*           m_pTiledShadingStage = nullptr;
	CColorGradingStage*           m_pColorGradingStage = nullptr;
	CSceneCustomStage*            m_pSceneCustomStage = nullptr;
	CLensOpticsStage*             m_pLensOpticsStage = nullptr;
	CPostEffectStage*             m_pPostEffectStage = nullptr;
	CRainStage*                   m_pRainStage = nullptr;
	CSnowStage*                   m_pSnowStage = nullptr;
	CDepthReadbackStage*          m_pDepthReadbackStage = nullptr;
	CMobileCompositionStage*      m_pMobileCompositionStage = nullptr;
	COmniCameraStage*             m_pOmniCameraStage = nullptr;

	CTypedConstantBuffer<HLSL_PerViewGlobalConstantBuffer> m_mainViewConstantBuffer;
	CDeviceResourceSetDesc                                 m_defaultMaterialBindPoints;
	CDeviceResourceSetDesc                                 m_defaultInstanceExtraResources;
	CDeviceResourceSetPtr                                  m_pDefaultInstanceExtraResourceSet;

	CRenderView*                  m_pCurrentRenderView = nullptr;
	EShaderRenderingFlags         m_renderingFlags = EShaderRenderingFlags(0);

	bool                          m_bInitialized = false;

	CCVarUpdateRecorder           m_changedCVars;

public:
	std::unique_ptr<CDownsamplePass        > m_DownscalePass;
	std::unique_ptr<CSharpeningUpsamplePass> m_UpscalePass;

private:
	void ExecuteHDRPostProcessing();
	void ExecuteMobilePipeline();

	int m_numInvalidDrawcalls = 0;
};

DEFINE_ENUM_FLAG_OPERATORS(CStandardGraphicsPipeline::SViewInfo::eFlags);
