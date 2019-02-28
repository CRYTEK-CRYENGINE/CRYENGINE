// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
class CComputeParticlesStage;
class CTiledLightVolumesStage;
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
class CDebugRenderTargetsStage;
class CCamera;

struct SRenderViewInfo;

enum EStandardGraphicsPipelineStage
{
	// Scene stages
	eStage_ShadowMap,
	eStage_SceneGBuffer,
	eStage_SceneForward,
	eStage_SceneCustom,
	eStage_SCENE_NUM,

	// Regular stages supporting async compute
	eStage_FIRST_ASYNC_COMPUTE = eStage_SCENE_NUM,
	eStage_ComputeSkinning = eStage_FIRST_ASYNC_COMPUTE,
	eStage_ComputeParticles,
	eStage_TiledLightVolumes,
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
	eStage_DebugRenderTargets,

	eStage_Count
};

class CStandardGraphicsPipeline : public CGraphicsPipeline
{
public:
	CStandardGraphicsPipeline();

	bool IsInitialized() const { return m_bInitialized; }

	void Init() final;
	void Resize(int renderWidth, int renderHeight) final;
	void Update(CRenderView* pRenderView, EShaderRenderingFlags renderingFlags) final;
	void Execute() final;
	void ShutDown() final;

	void ExecuteDebugger();
	void ExecuteBillboards();
	void ExecuteMinimumForwardShading();

	bool CreatePipelineStates(DevicePipelineStatesArray* pStateArray,
	                          SGraphicsPipelineStateDescription stateDesc,
	                          CGraphicsPipelineStateLocalCache* pStateCache);
	void GenerateMainViewConstantBuffer();
	void GeneratePerViewConstantBuffer(const SRenderViewInfo* pViewInfo, int viewInfoCount, CConstantBufferPtr pPerViewBuffer,const SRenderViewport* pCustomViewport = nullptr);
	bool FillCommonScenePassStates(const SGraphicsPipelineStateDescription& inputDesc, CDeviceGraphicsPSODesc& psoDesc);
	CDeviceResourceLayoutPtr CreateScenePassLayout(const CDeviceResourceSetDesc& perPassResources);

	// Partial pipeline functions, will be removed once the entire pipeline is implemented in Execute()
	void   ExecutePostAA();
	void   ExecuteAnisotropicVerticalBlur(CTexture* pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly);

	uint32 IncrementNumInvalidDrawcalls(int count) { return CryInterlockedAdd((volatile int*)&m_numInvalidDrawcalls, count); }
	uint32 GetNumInvalidDrawcalls() const          { return m_numInvalidDrawcalls;   }

	size_t GetViewInfoCount() const final;
	size_t GenerateViewInfo(SRenderViewInfo viewInfo[2]) final;

	const SRenderViewInfo& GetCurrentViewInfo(CCamera::EEye eye) const;

	uint32 GetRenderFlags() const { return m_renderingFlags; }

	CConstantBufferPtr                       GetMainViewConstantBuffer()                { return m_mainViewConstantBuffer.GetDeviceConstantBuffer(); }
	const CDeviceResourceSetDesc&            GetDefaultMaterialBindPoints()       const { return m_defaultMaterialBindPoints; }
	std::array<SamplerStateHandle, EFSS_MAX> GetDefaultMaterialSamplers()         const;
	const CDeviceResourceSetDesc&            GetDefaultDrawExtraResourceLayout()  const { return m_defaultDrawExtraRL; }
	CDeviceResourceSetPtr                    GetDefaulDrawExtraResourceSet()      const { return m_pDefaultDrawExtraRS; }

	CPostAAStage*             GetPostAAStage()                   const { return m_pPostAAStage; }
	CSceneGBufferStage*       GetGBufferStage()                        { return m_pSceneGBufferStage; }
	CShadowMapStage*          GetShadowStage()                   const { return m_pShadowMapStage; }
	CSceneForwardStage*       GetSceneForwardStage()             const { return m_pSceneForwardStage; }
	CComputeSkinningStage*    GetComputeSkinningStage()          const { return m_pComputeSkinningStage; }
	CComputeParticlesStage*   GetComputeParticlesStage()         const { return m_pComputeParticlesStage; }
	CDeferredDecalsStage*     GetDeferredDecalsStage()           const { return m_pDeferredDecalsStage; }
	CTiledLightVolumesStage*  GetTiledLightVolumesStage()        const { return m_pTiledLightVolumesStage; }
	CClipVolumesStage*        GetClipVolumesStage()              const { return m_pClipVolumesStage; }
	CHeightMapAOStage*        GetHeightMapAOStage()              const { return m_pHeightMapAOStage; }
	CShadowMaskStage*         GetShadowMaskStage()               const { return m_pShadowMaskStage; }
	CSceneCustomStage*        GetSceneCustomStage()              const { return m_pSceneCustomStage; }
	CFogStage*                GetFogStage()                      const { return m_pFogStage; }
	CVolumetricFogStage*      GetVolumetricFogStage()            const { return m_pVolumetricFogStage; }
	CWaterRipplesStage*       GetWaterRipplesStage()             const { return m_pWaterRipplesStage; }
	CWaterStage*              GetWaterStage()                    const { return m_pWaterStage; }
	CLensOpticsStage*         GetLensOpticsStage()               const { return m_pLensOpticsStage; }
	CDepthReadbackStage*      GetDepthReadbackStage()            const { return m_pDepthReadbackStage; }
	COmniCameraStage*         GetOmniCameraStage()               const { return m_pOmniCameraStage; }
	CDebugRenderTargetsStage* GetDebugRenderTargetsStage()       const { return m_pDebugRenderTargetsStage; }

	CLightVolumeBuffer&       GetLightVolumeBuffer()                   { return m_lightVolumeBuffer; }
	CParticleBufferSet&       GetParticleBufferSet()                   { return m_particleBuffer; }

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
	CComputeParticlesStage*       m_pComputeParticlesStage = nullptr;
	CDeferredDecalsStage*         m_pDeferredDecalsStage = nullptr;
	CClipVolumesStage*            m_pClipVolumesStage = nullptr;
	CShadowMaskStage*             m_pShadowMaskStage = nullptr;
	CTiledLightVolumesStage*      m_pTiledLightVolumesStage = nullptr;
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
	CDebugRenderTargetsStage*     m_pDebugRenderTargetsStage = nullptr;

	CTypedConstantBuffer<HLSL_PerViewGlobalConstantBuffer, 256> m_mainViewConstantBuffer;
	CDeviceResourceSetDesc                                      m_defaultMaterialBindPoints;
	CDeviceResourceSetDesc                                      m_defaultDrawExtraRL;
	CDeviceResourceSetPtr                                       m_pDefaultDrawExtraRS;

	EShaderRenderingFlags         m_renderingFlags = EShaderRenderingFlags(0);

	bool                          m_bInitialized = false;

	CCVarUpdateRecorder           m_changedCVars;

	// light volume data
	CLightVolumeBuffer            m_lightVolumeBuffer;

	// particle data for writing directly to VMEM
	CParticleBufferSet            m_particleBuffer;

public:
	std::unique_ptr<CDownsamplePass        > m_DownscalePass;
	std::unique_ptr<CSharpeningUpsamplePass> m_UpscalePass;

private:
	void ExecuteHDRPostProcessing();
	void ExecuteMobilePipeline();

	int m_numInvalidDrawcalls = 0;
};

