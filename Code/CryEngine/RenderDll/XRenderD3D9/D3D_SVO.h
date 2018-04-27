// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================

   Revision history:
* Created by Vladimir Kajalin

   =============================================================================*/

#ifndef __D3D_SVO__H__
#define __D3D_SVO__H__

#if defined(FEATURE_SVO_GI)

	#include "GraphicsPipeline/Common/ComputeRenderPass.h"
	#include "GraphicsPipeline/Common/FullscreenPass.h"

class CSvoComputePass : public CComputeRenderPass
{
public:

	CSvoComputePass()
		: CComputeRenderPass::CComputeRenderPass(CSvoComputePass::eFlags_ReflectConstantBuffersFromShader)
	{
	}

	int nPrevTargetSize = 0;
};

class CSvoFullscreenPass : public CFullscreenPass
{
public:

	int nPrevTargetSize = 0;
};

struct SSvoTargetsSet
{
	// tracing targets
	_smart_ptr<CTexture> pRT_RGB_0, pRT_ALD_0, pRT_RGB_1, pRT_ALD_1;

	// de-mosaic targets
	_smart_ptr<CTexture> pRT_RGB_DEM_MIN_0, pRT_ALD_DEM_MIN_0, pRT_RGB_DEM_MAX_0, pRT_ALD_DEM_MAX_0;
	_smart_ptr<CTexture> pRT_RGB_DEM_MIN_1, pRT_ALD_DEM_MIN_1, pRT_RGB_DEM_MAX_1, pRT_ALD_DEM_MAX_1;

	// output
	_smart_ptr<CTexture> pRT_FIN_OUT_0, pRT_FIN_OUT_1;

	CSvoFullscreenPass   passConeTrace;
	CSvoFullscreenPass   passDemosaic;
	CSvoFullscreenPass   passUpscale;
};

class CSvoRenderer : public ISvoRenderer
{
public:
	struct SForwardParams
	{
		Vec4 IntegrationMode;
	};

	// ISvoRenderer
	void                 Release() final;
	void                 InitCVarValues() final;

	static CSvoRenderer* GetInstance(bool bCheckAlloce = false);
	static bool          IsActive();
	void                 UpdateCompute(CRenderView* pRenderView);
	void                 UpdateRender(CRenderView* pRenderView);
	int                  GetIntegratioMode() const;
	int                  GetIntegratioMode(bool& bSpecTracingInUse) const;
	bool                 GetUseLightProbes() const { return e_svoTI_SkyColorMultiplier >= 0; }
	void                 DebugDrawStats(const RPProfilerStats* pBasicStats, float& ypos, const float ystep, float xposms);

	static bool          SetShaderParameters(float*& pSrc, uint32 paramType, UFloat4* sData);
	static CTexture*     GetRsmColorMap(const ShadowMapFrustum& rFr, bool bCheckUpdate = false);
	static CTexture*     GetRsmNormlMap(const ShadowMapFrustum& rFr, bool bCheckUpdate = false);
	ShadowMapFrustum*    GetRsmSunFrustum(const CRenderView* pRenderView) const;
	CTexture*            GetTroposphereMinRT();
	CTexture*            GetTroposphereMaxRT();
	CTexture*            GetTroposphereShadRT();
	CTexture*            GetTracedSunShadowsRT();
	CTexture*            GetDiffuseFinRT();
	CTexture*            GetSpecularFinRT();
	float                GetSsaoAmount()           { return IsActive() ? e_svoTI_SSAOAmount : 1.f; }
	float                GetVegetationMaxOpacity() { return e_svoTI_VegetationMaxOpacity; }
	CTexture*            GetRsmPoolCol()           { return IsActive() ? s_pRsmPoolCol.get() : NULL; }
	CTexture*            GetRsmPoolNor()           { return IsActive() ? s_pRsmPoolNor.get() : NULL; }
	static void          GetRsmTextures(_smart_ptr<CTexture>& pRsmColorMap, _smart_ptr<CTexture>& pRsmNormlMap, _smart_ptr<CTexture>& pRsmPoolCol, _smart_ptr<CTexture>& pRsmPoolNor);
	ColorF               GetSkyColor()             { return m_texInfo.vSkyColorTop; }

	void                 FillForwardParams(SForwardParams& svogiParams, bool enable = true) const;

protected:

	CSvoRenderer();
	virtual ~CSvoRenderer() {}

	// ISvoRenderer
	void                   SetEditingHelper(const Sphere& sp) final;
	bool                   IsShaderItemUsedForVoxelization(SShaderItem& rShaderItem, IRenderNode* pRN) final;

	static CTexture*       GetGBuffer(int nId);
	void                   UpscalePass(SSvoTargetsSet* pTS);
	void                   DemosaicPass(SSvoTargetsSet* pTS);
	void                   ConeTracePass(SSvoTargetsSet* pTS);

	template<class T> void SetupCommonSamplers(T& rp);
	template<class T> void SetupCommonConstants(SSvoTargetsSet* pTS, T& rp, CTexture* pRT);
	template<class T> void SetupRsmSunTextures(T& rp);
	template<class T> void SetupRsmSunConstants(T& rp);

	void                   TropospherePass();
	void                   TraceSunShadowsPass();

	void                   SetupGBufferTextures(CSvoFullscreenPass& rp);

	void                   CheckCreateUpdateRT(_smart_ptr<CTexture>& pTex, int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char* szName);

	void                   UpdateGpuVoxParams(I3DEngine::SSvoNodeInfo& nodeInfo);
	void                   ExecuteComputeShader(const char* szTechFinalName, CSvoComputePass& rp, int* nNodesForUpdateStartIndex, int nObjPassId, PodArray<I3DEngine::SSvoNodeInfo>& arrNodesForUpdate);
	uint64                 GetRunTimeFlags(bool bDiffuseMode = true, bool bPixelShader = true);
	template<class T> void SetupSvoTexturesForRead(I3DEngine::SSvoStaticTexInfo& texInfo, T& rp, int nStage, int nStageOpa = 0, int nStageNorm = 0);
	void                   SetupNodesForUpdate(int& nNodesForUpdateStartIndex, PodArray<I3DEngine::SSvoNodeInfo>& arrNodesForUpdate, CSvoComputePass& rp);
	void                   CheckAllocateRT(bool bSpecPass);
	template<class T> void SetupLightSources(PodArray<I3DEngine::SLightTI>& lightsTI, T& rp);
	template<class T> void BindTiledLights(PodArray<I3DEngine::SLightTI>& lightsTI, T& rp);
	void                   DrawPonts(PodArray<SVF_P3F_C4B_T2F>& arrVerts);
	void                   VoxelizeRE();
	bool                   VoxelizeMeshes(CShader* ef, SShaderPass* sfm);

	CRenderView*           RenderView() const { return m_pRenderView; };

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	_smart_ptr<CTexture> m_pRT_NID_0;
	_smart_ptr<CTexture> m_pRT_AIR_MIN;
	_smart_ptr<CTexture> m_pRT_AIR_MAX;
	_smart_ptr<CTexture> m_pRT_SHAD_MIN_MAX;
	_smart_ptr<CTexture> m_pRT_SHAD_FIN_0, m_pRT_SHAD_FIN_1;
	#endif

	Matrix44A                   m_matReproj;
	Matrix44A                   m_matViewProjPrev;
	CShader*                    m_pShader;
	_smart_ptr<CTexture>        m_pNoiseTex;
	_smart_ptr<CTexture>        m_pCloudShadowTex;
	static _smart_ptr<CTexture> s_pRsmColorMap;
	static _smart_ptr<CTexture> s_pRsmNormlMap;
	static _smart_ptr<CTexture> s_pRsmPoolCol;
	static _smart_ptr<CTexture> s_pRsmPoolNor;

	// Currently used render view
	CRenderView* m_pRenderView;

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	struct SVoxPool
	{
		SVoxPool() { nTexId = 0; pUAV = 0; pSRV = 0; }
		void Init(ITexture* pTex);
		int                nTexId;
		D3DUAV*            pUAV;
		D3DShaderResource* pSRV;
		CTexture*          pTex;
	};

	SVoxPool             vp_OPAC;
	SVoxPool             vp_RGB0;
	SVoxPool             vp_RGB1;
	SVoxPool             vp_DYNL;
	SVoxPool             vp_RGB2;
	SVoxPool             vp_RGB3;
	SVoxPool             vp_RGB4;
	SVoxPool             vp_NORM;
	SVoxPool             vp_ALDI;

	_smart_ptr<CTexture> m_pTexTexA;
	_smart_ptr<CTexture> m_pTexTriA;
	_smart_ptr<CTexture> m_pTexIndA;
	_smart_ptr<CTexture> m_pTexTree;
	_smart_ptr<CTexture> m_pTexOpac;
	_smart_ptr<CTexture> m_pTexTris;
	_smart_ptr<CTexture> m_pGlobalSpecCM;
	#endif

	PodArray<I3DEngine::SSvoNodeInfo> m_arrNodesForUpdateIncr;
	PodArray<I3DEngine::SSvoNodeInfo> m_arrNodesForUpdateNear;
	PodArray<I3DEngine::SLightTI>     m_arrLightsStatic;
	PodArray<I3DEngine::SLightTI>     m_arrLightsDynamic;
	PodArray<SVF_P3F_C4B_T2F>         m_arrVerts;
	Matrix44A                         m_mGpuVoxViewProj[3];
	Vec4                              m_wsOffset, m_tcOffset;
	static const int                  SVO_MAX_NODE_GROUPS = 4;
	float                             m_arrNodesForUpdate[SVO_MAX_NODE_GROUPS][4][4];
	int                               m_nCurPropagationPassID;
	I3DEngine::SSvoStaticTexInfo      m_texInfo;
	static CSvoRenderer*              s_pInstance;
	PodArray<I3DEngine::SSvoNodeInfo> m_arrNodeInfo;

	SSvoTargetsSet                    m_tsDiff, m_tsSpec;

	// passes
	CSvoComputePass    m_passClearBricks;
	CSvoComputePass    m_passInjectDynamicLights;
	CSvoComputePass    m_passInjectStaticLights;
	CSvoComputePass    m_passInjectAirOpacity;
	CSvoComputePass    m_passPropagateLighting_1to2;
	CSvoComputePass    m_passPropagateLighting_2to3;
	CSvoFullscreenPass m_passTroposphere;

	// cvar values
	#define INIT_ALL_SVO_CVARS                                      \
	  INIT_SVO_CVAR(int, e_svoDVR);                                 \
	  INIT_SVO_CVAR(int, e_svoEnabled);                             \
	  INIT_SVO_CVAR(int, e_svoRender);                              \
	  INIT_SVO_CVAR(int, e_svoTI_ResScaleBase);                     \
	  INIT_SVO_CVAR(int, e_svoTI_ResScaleAir);                      \
	  INIT_SVO_CVAR(int, e_svoTI_Active);                           \
	  INIT_SVO_CVAR(int, e_svoTI_IntegrationMode);                  \
	  INIT_SVO_CVAR(float, e_svoTI_InjectionMultiplier);            \
	  INIT_SVO_CVAR(float, e_svoTI_SkyColorMultiplier);             \
	  INIT_SVO_CVAR(float, e_svoTI_DiffuseAmplifier);               \
	  INIT_SVO_CVAR(float, e_svoTI_TranslucentBrightness);          \
	  INIT_SVO_CVAR(int, e_svoTI_NumberOfBounces);                  \
	  INIT_SVO_CVAR(float, e_svoTI_Saturation);                     \
	  INIT_SVO_CVAR(float, e_svoTI_PropagationBooster);             \
	  INIT_SVO_CVAR(float, e_svoTI_DiffuseBias);                    \
	  INIT_SVO_CVAR(float, e_svoTI_DiffuseConeWidth);               \
	  INIT_SVO_CVAR(float, e_svoTI_ConeMaxLength);                  \
	  INIT_SVO_CVAR(float, e_svoTI_SpecularAmplifier);              \
	  INIT_SVO_CVAR(float, e_svoMinNodeSize);                       \
	  INIT_SVO_CVAR(float, e_svoMaxNodeSize);                       \
	  INIT_SVO_CVAR(int, e_svoTI_LowSpecMode);                      \
	  INIT_SVO_CVAR(int, e_svoTI_HalfresKernelPrimary);             \
	  INIT_SVO_CVAR(int, e_svoTI_HalfresKernelSecondary);           \
	  INIT_SVO_CVAR(int, e_svoVoxelPoolResolution);                 \
	  INIT_SVO_CVAR(int, e_svoTI_Apply);                            \
	  INIT_SVO_CVAR(float, e_svoTI_Diffuse_Spr);                    \
	  INIT_SVO_CVAR(int, e_svoTI_Diffuse_Cache);                    \
	  INIT_SVO_CVAR(int, e_svoTI_SpecularFromDiffuse);              \
	  INIT_SVO_CVAR(int, e_svoTI_DynLights);                        \
	  INIT_SVO_CVAR(int, e_svoTI_ShadowsFromSun);                   \
	  INIT_SVO_CVAR(int, e_svoTI_ShadowsFromHeightmap);             \
	  INIT_SVO_CVAR(int, e_svoTI_Troposphere_Active);               \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Brightness);         \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Ground_Height);      \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Layer0_Height);      \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Layer1_Height);      \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Snow_Height);        \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Layer0_Rand);        \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Layer1_Rand);        \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Layer0_Dens);        \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Layer1_Dens);        \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_CloudGen_Height);    \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_CloudGen_Freq);      \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_CloudGen_FreqStep);  \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_CloudGen_Scale);     \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_CloudGenTurb_Freq);  \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_CloudGenTurb_Scale); \
	  INIT_SVO_CVAR(float, e_svoTI_Troposphere_Density);            \
	  INIT_SVO_CVAR(float, e_svoTI_RT_MaxDist);                     \
	  INIT_SVO_CVAR(float, e_svoTI_ShadowsSoftness);                \
	  INIT_SVO_CVAR(float, e_svoTI_Specular_Sev);                   \
	  INIT_SVO_CVAR(float, e_svoTI_SSAOAmount);                     \
	  INIT_SVO_CVAR(float, e_svoTI_PortalsDeform);                  \
	  INIT_SVO_CVAR(float, e_svoTI_PortalsInject);                  \
	  INIT_SVO_CVAR(int, e_svoTI_SunRSMInject);                     \
	  INIT_SVO_CVAR(int, e_svoDispatchX);                           \
	  INIT_SVO_CVAR(int, e_svoDispatchY);                           \
	  INIT_SVO_CVAR(int, e_svoDebug);                               \
	  INIT_SVO_CVAR(int, e_svoVoxGenRes);                           \
	  INIT_SVO_CVAR(float, e_svoDVR_DistRatio);                     \
	  INIT_SVO_CVAR(int, e_svoTI_GsmCascadeLod);                    \
	  INIT_SVO_CVAR(float, e_svoTI_SSDepthTrace);                   \
	  INIT_SVO_CVAR(float, e_svoTI_EmissiveMultiplier);             \
	  INIT_SVO_CVAR(float, e_svoTI_PointLightsMultiplier);          \
	  INIT_SVO_CVAR(float, e_svoTI_TemporalFilteringBase);          \
	  INIT_SVO_CVAR(float, e_svoTI_HighGlossOcclusion);             \
	  INIT_SVO_CVAR(float, e_svoTI_VegetationMaxOpacity);           \
	  INIT_SVO_CVAR(float, e_svoTI_MinReflectance);                 \
	  INIT_SVO_CVAR(int, e_svoTI_DualTracing);                      \
	  INIT_SVO_CVAR(int, e_svoTI_AnalyticalOccluders);              \
	  INIT_SVO_CVAR(int, e_svoTI_RsmUseColors);                     \
	  INIT_SVO_CVAR(float, e_svoTI_AnalyticalOccludersRange);       \
	  INIT_SVO_CVAR(float, e_svoTI_AnalyticalOccludersSoftness);    \
	  INIT_SVO_CVAR(int, e_svoTI_AnalyticalGI);                     \
	  INIT_SVO_CVAR(int, e_svoTI_TraceVoxels);                      \
	  INIT_SVO_CVAR(int, e_svoTI_AsyncCompute);                     \
	  INIT_SVO_CVAR(float, e_svoTI_SkyLightBottomMultiplier);       \
	  INIT_SVO_CVAR(float, e_svoTI_VoxelOpacityMultiplier);         \
	  INIT_SVO_CVAR(float, e_svoTI_PointLightsBias);                \
	  // INIT_ALL_SVO_CVARS

	#define INIT_SVO_CVAR(_type, _var) _type _var = 0;
	INIT_ALL_SVO_CVARS;
	#undef INIT_SVO_CVAR
};

#endif
#endif
