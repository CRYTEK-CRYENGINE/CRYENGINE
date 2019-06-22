// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

const float LIGHT_PROJECTOR_MAX_FOV = 175.f;

struct ShadowMapInfo
{
	int GetLodCount() const { return dynamicLodCount + cachedLodCount; }

	ShadowMapFrustumPtr pGSM[MAX_GSM_LODS_NUM];

	int                 dynamicLodCount = 0;
	int                 cachedLodCount = 0;
};

struct SShadowFrustumMGPUCache
{
	SShadowFrustumMGPUCache();
	~SShadowFrustumMGPUCache();

	std::array<ShadowMapFrustumPtr, MAX_GSM_LODS_NUM> m_staticShadowMapFrustums;
	ShadowMapFrustumPtr                               m_pHeightMapAOFrustum;
};

class CLightEntity final : public ILightSource, public Cry3DEngineBase
{
public:
	CLightEntity();
	~CLightEntity();
	static void                          StaticReset();

	virtual EERType                      GetRenderNodeType() const override { return eERType_Light; }
	virtual const char*                  GetEntityClassName(void) const override { return "LightEntityClass"; }
	virtual const char*                  GetName(void) const override;
	virtual void                         GetLocalBounds(AABB& bbox) const override;
	virtual Vec3                         GetPos(bool) const override;
	virtual void                         Render(const SRendParams&, const SRenderingPassInfo& passInfo) override;
	virtual void                         Hide(bool bHide) override;
	virtual IPhysicalEntity*             GetPhysics(void) const                  override { return 0; }
	virtual void                         SetPhysics(IPhysicalEntity*)            override {}
	virtual void                         SetMaterial(IMaterial* pMat)            override { m_pMaterial = pMat; }
	virtual IMaterial*                   GetMaterial(Vec3* pHitPos = NULL) const override { return m_pMaterial; }
	virtual IMaterial*                   GetMaterialOverride()             const override { return m_pMaterial; }
	virtual float                        GetMaxViewDist() const override;
	virtual void                         SetLightProperties(const SRenderLight& light) override;
	virtual SRenderLight&                GetLightProperties()       override { return m_light; }
	virtual const SRenderLight&          GetLightProperties() const override { return m_light; }
	virtual void                         SetMatrix(const Matrix34& mat) override;
	virtual const Matrix34&              GetMatrix() const override { return m_Matrix; }
	virtual struct ShadowMapFrustum*     GetShadowFrustum(int nId = 0) const override;
	virtual void                         GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual const AABB                   GetBBox() const override { return m_WSBBox; }
	virtual void                         SetBBox(const AABB& WSBBox) override { m_WSBBox = WSBBox; }
	virtual void                         FillBBox(AABB& aabb) const override { aabb = GetBBox(); }
	virtual void                         OffsetPosition(const Vec3& delta) override;
	virtual void                         SetCastingException(IRenderNode* pNotCaster) override { m_pNotCaster = pNotCaster; }
	IRenderNode*                         GetCastingException()                                 { return m_pNotCaster; }
	virtual bool                         IsLightAreasVisible() const override;
	static const PodArray<SPlaneObject>& GetCastersHull() { return s_lstTmpCastersHull; }
	virtual void                         SetViewDistRatio(int nViewDistRatio) override;
#if defined(FEATURE_SVO_GI)
	virtual EGIMode                      GetGIMode() const override;
#endif
	virtual void                         SetOwnerEntity(IEntity* pEntity) override;
	virtual IEntity*                     GetOwnerEntity() const            override { return m_pOwnerEntity; }
	virtual bool                         IsAllocatedOutsideOf3DEngineDLL() override { return GetOwnerEntity() != nullptr; }
	void                                 InitEntityShadowMapInfoStructure(int dynamicLods, int cachedLods);
	void                                 UpdateGSMLightSourceShadowFrustum(const SRenderingPassInfo& passInfo);
	int                                  UpdateGSMLightSourceDynamicShadowFrustum(int nDynamicLodCount, int nDistanceLodCount, float& fDistanceFromViewLastDynamicLod, float& fGSMBoxSizeLastDynamicLod, bool bFadeLastCascade, const SRenderingPassInfo& passInfo);
	int                                  UpdateGSMLightSourceCachedShadowFrustum(int nFirstLod, int nLodCount, bool isHeightMapAOEnabled, float& fDistFromViewDynamicLod, float fRadiusDynamicLod, const SRenderingPassInfo& passInfo);
	int                                  UpdateGSMLightSourceNearestShadowFrustum(int nFrustumIndex, const SRenderingPassInfo& passInfo);
	bool                                 ProcessFrustum(int nLod, float fCamBoxSize, float fDistanceFromView, PodArray<struct SPlaneObject>& lstCastersHull, const SRenderingPassInfo& passInfo);
	void                                 CollectShadowCascadeForOnePassTraversal(ShadowMapFrustum* pFr);
	static bool                          IsOnePassTraversalFrustum(const ShadowMapFrustum* pFr);
	static void                          SetShadowFrustumsCollector(std::vector<std::pair<ShadowMapFrustum*, const CLightEntity*>>* p) { s_pShadowFrustumsCollector = p; }
	static void                          ProcessPerObjectFrustum(ShadowMapFrustum* pFr, struct SPerObjectShadow* pPerObjectShadow, ILightSource* pLightSource, const SRenderingPassInfo& passInfo);
	void                                 InitShadowFrustum_SUN_Conserv(ShadowMapFrustum* pFr, int dwAllowedTypes, float fGSMBoxSize, float fDistance, int nLod, const SRenderingPassInfo& passInfo);
	void                                 InitShadowFrustum_PROJECTOR(ShadowMapFrustum* pFr, int dwAllowedTypes, const SRenderingPassInfo& passInfo);
	void                                 InitShadowFrustum_OMNI(ShadowMapFrustum* pFr, int dwAllowedTypes, const SRenderingPassInfo& passInfo);
	void                                 SetupShadowFrustumCamera_SUN(ShadowMapFrustum* pFr, int dwAllowedTypes, int nRenderNodeFlags, PodArray<struct SPlaneObject>& lstCastersHull, int nLod, const SRenderingPassInfo& passInfo);
	void                                 SetupShadowFrustumCamera_PROJECTOR(ShadowMapFrustum* pFr, int dwAllowedTypes, const SRenderingPassInfo& passInfo);
	void                                 SetupShadowFrustumCamera_OMNI(ShadowMapFrustum* pFr, int dwAllowedTypes, const SRenderingPassInfo& passInfo);
	void                                 CheckValidFrustums_OMNI(ShadowMapFrustum* pFr, const SRenderingPassInfo& passInfo);
	bool                                 CheckFrustumsIntersect(CLightEntity* lightEnt);
	bool                                 GetGsmFrustumBounds(const CCamera& viewFrustum, ShadowMapFrustum* pShadowFrustum);
	void                                 OnCasterDeleted(IShadowCaster* pCaster);
	int                                  MakeShadowCastersHull(PodArray<SPlaneObject>& lstCastersHull, const SRenderingPassInfo& passInfo);
	static Vec3                          GSM_GetNextScreenEdge(float fPrevRadius, float fPrevDistanceFromView, const SRenderingPassInfo& passInfo);
	static float                         GSM_GetLODProjectionCenter(const Vec3& vEdgeScreen, float fRadius);
	static bool                          FrustumIntersection(const CCamera& viewFrustum, const CCamera& shadowFrustum);
	void                                 UpdateCastShadowFlag(float fDistance, const SRenderingPassInfo& passInfo);
	void                                 CalculateShadowBias(ShadowMapFrustum* pFr, int nLod, float fGSMBoxSize) const;
	virtual void                         SetLayerId(uint16 nLayerId) override;
	virtual uint16                       GetLayerId() const override { return m_layerId; }
	ShadowMapInfo*                       GetShadowMapInfo() const { return m_pShadowMapInfo.get(); }

	virtual void                         Release(bool);

private:
	static PodArray<SPlaneObject>  s_lstTmpCastersHull;
	static SShadowFrustumMGPUCache s_shadowFrustumCache;
	IEntity*                       m_pOwnerEntity = 0;
	_smart_ptr<IMaterial>          m_pMaterial;
	Matrix34                       m_Matrix;
	std::shared_ptr<ShadowMapInfo> m_pShadowMapInfo;
	SRenderLight                   m_light;
	IRenderNode*                   m_pNotCaster;
	AABB                           m_WSBBox;
	uint16                         m_layerId;
	bool                           m_bShadowCaster : 1;

	static std::vector<std::pair<ShadowMapFrustum*, const CLightEntity*>>* s_pShadowFrustumsCollector;
};
