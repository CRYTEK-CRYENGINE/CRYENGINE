// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================

   Revision history:
* Created by Vladimir Kajalin

   =============================================================================*/

#ifndef __D3D_SVO__H__
#define __D3D_SVO__H__

#if defined(FEATURE_SVO_GI)

struct SSvoTargetsSet
{
	SSvoTargetsSet() { ZeroStruct(*this); }
	void Release();

	CTexture
	// tracing targets
	* pRT_RGB_0, * pRT_ALD_0,
	* pRT_RGB_1, * pRT_ALD_1,
	// de-mosaic targets
	* pRT_RGB_DEM_MIN_0, * pRT_ALD_DEM_MIN_0,
	* pRT_RGB_DEM_MAX_0, * pRT_ALD_DEM_MAX_0,
	* pRT_RGB_DEM_MIN_1, * pRT_ALD_DEM_MIN_1,
	* pRT_RGB_DEM_MAX_1, * pRT_ALD_DEM_MAX_1,
	// output
	* pRT_FIN_OUT_0,
	* pRT_FIN_OUT_1;
};

class CSvoRenderer : public ISvoRenderer
{
public:

	static CSvoRenderer* GetInstance(bool bCheckAlloce = false);
	void                 Release();
	static bool          IsActive();
	void                 UpdateCompute();
	void                 UpdateRender();
	int                  GetIntegratioMode();
	int                  GetIntegratioMode(bool& bSpecTracingInUse);
	bool                 GetUseLightProbes() { return e_svoTI_SkyColorMultiplier >= 0; }
	void                 DebugDrawStats(const RPProfilerStats* pBasicStats, float& ypos, const float ystep, float xposms);
	static bool          SetSamplers(int nCustomID, EHWShaderClass eSHClass, int nTUnit, int nTState, int nTexMaterialSlot, int nSUnit);
	static bool          SetShaderParameters(float*& pSrc, uint32 paramType, UFloat4* sData);
	static CTexture*     GetRsmColorMap(const ShadowMapFrustum& rFr, bool bCheckUpdate = false);
	static CTexture*     GetRsmNormlMap(const ShadowMapFrustum& rFr, bool bCheckUpdate = false);
	CTexture*            GetTroposphereMinRT();
	CTexture*            GetTroposphereMaxRT();
	CTexture*            GetTroposphereShadRT();
	CTexture*            GetDiffuseFinRT();
	CTexture*            GetSpecularFinRT();
	float                GetSsaoAmount()           { return IsActive() ? e_svoTI_SSAOAmount : 1.f; }
	float                GetVegetationMaxOpacity() { return e_svoTI_VegetationMaxOpacity; }
	CTexture*            GetRsmPoolCol()           { return IsActive() ? m_pRsmPoolCol : NULL; }
	CTexture*            GetRsmPoolNor()           { return IsActive() ? m_pRsmPoolNor : NULL; }

protected:

	CSvoRenderer();
	virtual ~CSvoRenderer();
	void             SetEditingHelper(const Sphere& sp);
	bool             IsShaderItemUsedForVoxelization(SShaderItem& rShaderItem, IRenderNode* pRN);
	static CTexture* GetGBuffer(int nId);
	void             UpScalePass(SSvoTargetsSet* pTS);
	void             DemosaicPass(SSvoTargetsSet* pTS);
	void             ConeTracePass(SSvoTargetsSet* pTS);
	void             SetupRsmSun(const EHWShaderClass eShClass);
	void             TropospherePass();
	void             SetShaderFloat(const EHWShaderClass eShClass, const CCryNameR& NameParam, const Vec4* fParams, int nParams);
	void             CheckCreateUpdateRT(CTexture*& pTex, int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char* szName);

	enum EComputeStages
	{
		eCS_ClearBricks,
		eCS_InjectDynamicLights,
		eCS_InjectStaticLights,
		eCS_InjectAirOpacity,
		eCS_PropagateLighting_1to2,
		eCS_PropagateLighting_2to3,
	};

	void UpdateGpuVoxParams(I3DEngine::SSvoNodeInfo& nodeInfo);
	void ExecuteComputeShader(CShader* pSH, const char* szTechFinalName, EComputeStages etiStage, int* nNodesForUpdateStartIndex, int nObjPassId, PodArray<I3DEngine::SSvoNodeInfo>& arrNodesForUpdate);
	void SetShaderFlags(bool bDiffuseMode = true, bool bPixelShader = true);
	void SetupSvoTexturesForRead(I3DEngine::SSvoStaticTexInfo& texInfo, EHWShaderClass eShaderClass, int nStage, int nStageOpa = 0, int nStageNorm = 0);
	void SetupNodesForUpdate(int& nNodesForUpdateStartIndex, PodArray<I3DEngine::SSvoNodeInfo>& arrNodesForUpdate);
	void CheckAllocateRT(bool bSpecPass);
	void SetupLightSources(PodArray<I3DEngine::SLightTI>& lightsTI, CShader* pShader, bool bPS);
	void BindTiledLights(PodArray<I3DEngine::SLightTI>& lightsTI, CDeviceManager::SHADER_TYPE shaderType);
	void DrawPonts(PodArray<SVF_P3F_C4B_T2F>& arrVerts);
	void InitCVarValues();
	void VoxelizeRE();
	bool VoxelizeMeshes(CShader* ef, SShaderPass* sfm);

	SSvoTargetsSet m_tsDiff, m_tsSpec;

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	CTexture
	* m_pRT_NID_0,
	* m_pRT_AIR_MIN,
	* m_pRT_AIR_MAX,
	* m_pRT_AIR_SHAD;
	#endif

	int       m_nTexStateTrilinear, m_nTexStateLinear, m_nTexStatePoint, m_nTexStateLinearWrap;
	Matrix44A m_matReproj;
	Matrix44A m_matViewProjPrev;
	CShader*  m_pShader;
	CTexture* m_pNoiseTex;
	CTexture* m_pRsmColorMap;
	CTexture* m_pRsmNormlMap;
	CTexture* m_pRsmPoolCol;
	CTexture* m_pRsmPoolNor;

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	struct SVoxPool
	{
		SVoxPool() { nTexId = 0; pUAV = 0; pSRV = 0; }
		void Init(ITexture* pTex);
		int     nTexId;
		D3DUAV* pUAV;
		D3DShaderResource* pSRV;
	};

	SVoxPool vp_OPAC;
	SVoxPool vp_RGB0;
	SVoxPool vp_RGB1;
	SVoxPool vp_DYNL;
	SVoxPool vp_RGB2;
	SVoxPool vp_RGB3;
	SVoxPool vp_RGB4;
	SVoxPool vp_NORM;
	SVoxPool vp_ALDI;
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
	  INIT_SVO_CVAR(int, e_svoTI_HalfresKernel);                    \
	  INIT_SVO_CVAR(int, e_svoVoxelPoolResolution);                 \
	  INIT_SVO_CVAR(int, e_svoTI_Apply);                            \
	  INIT_SVO_CVAR(float, e_svoTI_Diffuse_Spr);                    \
	  INIT_SVO_CVAR(int, e_svoTI_Diffuse_Cache);                    \
	  INIT_SVO_CVAR(int, e_svoTI_Specular_Reproj);                  \
	  INIT_SVO_CVAR(int, e_svoTI_Specular_FromDiff);                \
	  INIT_SVO_CVAR(int, e_svoTI_DynLights);                        \
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
	  INIT_SVO_CVAR(float, e_svoTI_Shadow_Sev);                     \
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
		INIT_SVO_CVAR(int, e_svoTI_RsmUseColors);											\
		INIT_SVO_CVAR(float, e_svoTI_AnalyticalOccludersRange);				\
		INIT_SVO_CVAR(float, e_svoTI_AnalyticalOccludersSoftness);		\

	  // INIT_ALL_SVO_CVARS

	#define INIT_SVO_CVAR(_type, _var) _type _var;
	INIT_ALL_SVO_CVARS;
	#undef INIT_SVO_CVAR
};

#endif
#endif
