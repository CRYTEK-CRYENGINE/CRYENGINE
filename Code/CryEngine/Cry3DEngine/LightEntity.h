// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef LIGHT_ENTITY_H
#define LIGHT_ENTITY_H

const float LIGHT_PROJECTOR_MAX_FOV = 175.f;

struct CLightEntity : public ILightSource, public Cry3DEngineBase
{
	static void StaticReset();

public:
	virtual EERType                  GetRenderNodeType();
	virtual const char*              GetEntityClassName(void) const { return "LightEntityClass"; }
	virtual const char*              GetName(void) const;
	virtual void                     GetLocalBounds(AABB& bbox);
	virtual Vec3                     GetPos(bool) const;
	virtual void                     Render(const SRendParams&, const SRenderingPassInfo& passInfo);
	virtual void                     Hide(bool bHide);
	virtual IPhysicalEntity*         GetPhysics(void) const                  { return 0; }
	virtual void                     SetPhysics(IPhysicalEntity*)            {}
	virtual void                     SetMaterial(IMaterial* pMat)            { m_pMaterial = pMat; }
	virtual IMaterial*               GetMaterial(Vec3* pHitPos = NULL) const { return m_pMaterial; }
	virtual IMaterial*               GetMaterialOverride()                   { return m_pMaterial; }
	virtual float                    GetMaxViewDist();
	virtual void                     SetLightProperties(const CDLight& light);
	virtual CDLight&                 GetLightProperties() { return m_light; };
	virtual void                     Release(bool);
	virtual void                     SetMatrix(const Matrix34& mat);
	virtual const Matrix34&          GetMatrix() { return m_Matrix; }
	virtual struct ShadowMapFrustum* GetShadowFrustum(int nId = 0);
	virtual void                     GetMemoryUsage(ICrySizer* pSizer) const;
	virtual const AABB               GetBBox() const             { return m_WSBBox; }
	virtual void                     SetBBox(const AABB& WSBBox) { m_WSBBox = WSBBox; }
	virtual void                     FillBBox(AABB& aabb);
	virtual void                     OffsetPosition(const Vec3& delta);
	virtual void                     SetCastingException(IRenderNode* pNotCaster) { m_pNotCaster = pNotCaster; }
	virtual bool                     IsLightAreasVisible();
	const PodArray<SPlaneObject>&    GetCastersHull() const { return s_lstTmpCastersHull; }

	virtual void             SetViewDistRatio(int nViewDistRatio);
#if defined(FEATURE_SVO_GI)
	virtual EGIMode          GetGIMode() const;
#endif
	virtual void             SetOwnerEntity( IEntity *pEntity );
	virtual IEntity*         GetOwnerEntity() const final      { return m_pOwnerEntity; }
	virtual bool             IsAllocatedOutsideOf3DEngineDLL() { return GetOwnerEntity() != nullptr; }
	void                     InitEntityShadowMapInfoStructure();
	void                     UpdateGSMLightSourceShadowFrustum(const SRenderingPassInfo& passInfo);
	int                      UpdateGSMLightSourceDynamicShadowFrustum(int nDynamicLodCount, int nDistanceLodCount, float& fDistanceFromViewLastDynamicLod, float& fGSMBoxSizeLastDynamicLod, bool bFadeLastCascade, const SRenderingPassInfo& passInfo);
	int                      UpdateGSMLightSourceCachedShadowFrustum(int nFirstLod, int nLodCount, float& fDistFromViewDynamicLod, float fRadiusDynamicLod, const SRenderingPassInfo& passInfo);
	int                      UpdateGSMLightSourceNearestShadowFrustum(int nFrustumIndex, const SRenderingPassInfo& passInfo);
	bool                     ProcessFrustum(int nLod, float fCamBoxSize, float fDistanceFromView, PodArray<struct SPlaneObject>& lstCastersHull, const SRenderingPassInfo& passInfo);
	static void              ProcessPerObjectFrustum(ShadowMapFrustum* pFr, struct SPerObjectShadow* pPerObjectShadow, ILightSource* pLightSource, const SRenderingPassInfo& passInfo);
	void                     InitShadowFrustum_SUN_Conserv(ShadowMapFrustum* pFr, int dwAllowedTypes, float fGSMBoxSize, float fDistance, int nLod, const SRenderingPassInfo& passInfo);
	void                     InitShadowFrustum_PROJECTOR(ShadowMapFrustum* pFr, int dwAllowedTypes, const SRenderingPassInfo& passInfo);
	void                     InitShadowFrustum_OMNI(ShadowMapFrustum* pFr, int dwAllowedTypes, const SRenderingPassInfo& passInfo);
	void                     FillFrustumCastersList_SUN(ShadowMapFrustum* pFr, int dwAllowedTypes, int nRenderNodeFlags, PodArray<struct SPlaneObject>& lstCastersHull, int nLod, const SRenderingPassInfo& passInfo);
	void                     FillFrustumCastersList_PROJECTOR(ShadowMapFrustum* pFr, int dwAllowedTypes, const SRenderingPassInfo& passInfo);
	void                     FillFrustumCastersList_OMNI(ShadowMapFrustum* pFr, int dwAllowedTypes, const SRenderingPassInfo& passInfo);
	void                     CheckValidFrustums_OMNI(ShadowMapFrustum* pFr, const SRenderingPassInfo& passInfo);
	bool                     CheckFrustumsIntersect(CLightEntity* lightEnt);
	bool                     GetGsmFrustumBounds(const CCamera& viewFrustum, ShadowMapFrustum* pShadowFrustum);
	void                     DetectCastersListChanges(ShadowMapFrustum* pFr, const SRenderingPassInfo& passInfo);
	void                     OnCasterDeleted(IShadowCaster* pCaster);
	int                      MakeShadowCastersHull(PodArray<SPlaneObject>& lstCastersHull, const SRenderingPassInfo& passInfo);
	static Vec3              GSM_GetNextScreenEdge(float fPrevRadius, float fPrevDistanceFromView, const SRenderingPassInfo& passInfo);
	static float             GSM_GetLODProjectionCenter(const Vec3& vEdgeScreen, float fRadius);
	static bool              FrustumIntersection(const CCamera& viewFrustum, const CCamera& shadowFrustum);
	void                     UpdateCastShadowFlag(float fDistance, const SRenderingPassInfo& passInfo);
	void                     CalculateShadowBias(ShadowMapFrustum* pFr, int nLod, float fGSMBoxSize) const;

	CLightEntity();
	~CLightEntity();

	CDLight               m_light;
	bool                  m_bShadowCaster : 1;

	_smart_ptr<IMaterial> m_pMaterial;
	Matrix34              m_Matrix;
	IRenderNode*          m_pNotCaster;

	// used for shadow maps
	struct ShadowMapInfo
	{
		void Release(struct IRenderer* pRenderer);
		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(this, sizeof(*this));
		}
		ShadowMapFrustumPtr pGSM[MAX_GSM_LODS_NUM];
	}*   m_pShadowMapInfo;

	AABB m_WSBBox;

	void   SetLayerId(uint16 nLayerId);
	uint16 GetLayerId()                { return m_layerId; }

private:
	static PodArray<SPlaneObject> s_lstTmpCastersHull;
	uint16    m_layerId;
	IEntity*  m_pOwnerEntity = 0;
};
#endif
