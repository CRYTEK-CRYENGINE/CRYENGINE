// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CDeformableNode;

#define VEGETATION_CONV_FACTOR 64.f

template<class T>
class PodArrayAABB : public PodArray<T>
{
public:
	AABB m_aabbBox;
};

// Warning: Average outdoor level has about 200.000 objects of this class allocated - so keep it small
class CVegetation final : public IVegetation, public Cry3DEngineBase
{
public:

	enum EAllocatorId
	{
		eAllocator_Default    = 0,
		eAllocator_Procedural = 1,
		eAllocator_Count      = 2,
	};

	Vec3                                        m_vPos;
	IPhysicalEntity*                            m_pPhysEnt;
	SVegetationSpriteInfo*                      m_pSpriteInfo;
	CDeformableNode*                            m_pDeformable;
	PodArrayAABB<CRenderObject::SInstanceInfo>* m_pInstancingInfo;

	int  m_nObjectTypeIndex;
	byte m_ucAngle;
	byte m_ucScale;
	byte m_boxExtends[6];
	byte m_ucRadius;
	byte m_ucAngleX;
	byte m_ucAngleY;
	byte m_bApplyPhys;

	static CRY_ALIGN(128) float g_scBoxDecomprTable[256];

	CVegetation();
	virtual ~CVegetation();
	void                 SetStatObjGroupIndex(int nVegetationGroupIndex) override;

	virtual void         Instance(bool bInstance) override;
	void                 CheckCreateDeformable();
	virtual bool         CanExecuteRenderAsJob() const override;
	int                  GetStatObjGroupId() const override      { return m_nObjectTypeIndex; }
	const char*          GetEntityClassName(void) const override { return "Vegetation"; }
	Vec3                 GetPos(bool bWorldOnly = true) const override;
	virtual float        GetScale(void) const override           { return (1.f / VEGETATION_CONV_FACTOR) * m_ucScale; }
	void                 SetScale(float fScale)               { m_ucScale = (uint8)SATURATEB(fScale * VEGETATION_CONV_FACTOR); }
	const char*          GetName(void) const override;
	virtual CLodValue    ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo) override;
	virtual void         Render(const SRendParams& RendParams, const SRenderingPassInfo& passInfo) override { assert(0); }
	void                 Render(const SRenderingPassInfo& passInfo, const CLodValue& lodValue, SSectorTextureSet* pTerrainTexInfo) const;
	IPhysicalEntity*     GetPhysics(void) const override                                                    { return m_pPhysEnt; }
	IRenderMesh*         GetRenderMesh(int nLod) const override;
	void                 SetPhysics(IPhysicalEntity* pPhysEnt) override                                     { m_pPhysEnt = pPhysEnt; }
	void                 SetMaterial(IMaterial*) override                                                   {}
	IMaterial*           GetMaterial(Vec3* pHitPos = NULL) const override;
	IMaterial*           GetMaterialOverride() const override;
	void                 SetMatrix(const Matrix34& mat) override;
	virtual void         Physicalize(bool bInstant = false) override;
	bool                 PhysicalizeFoliage(bool bPhysicalize = true, int iSource = 0, int nSlot = 0) override;
	virtual IRenderNode* Clone() const override;
	IPhysicalEntity*     GetBranchPhys(int idx, int nSlot = 0) override;
	IFoliage*            GetFoliage(int nSlot = 0) override;
	float                GetSpriteSwitchDist() const;
	bool                 IsBreakable() const { pe_params_part pp; pp.ipart = 0; return m_pPhysEnt && m_pPhysEnt->GetParams(&pp) && pp.idmatBreakable >= 0; }
	bool                 IsBending() const;
	virtual float        GetMaxViewDist() const override;
	IStatObj*            GetEntityStatObj(unsigned int nSubPartId = 0, Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) override;
	virtual EERType      GetRenderNodeType() const override { return eERType_Vegetation; }
	virtual void         Dephysicalize(bool bKeepIfReferenced = false) override;
	void                 Dematerialize() override;
	virtual void         GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual const AABB   GetBBox() const override { AABB aabb; FillBBoxFromExtends(aabb, m_boxExtends, m_vPos); return aabb; }
	virtual void         FillBBox(AABB& aabb) const override { aabb = GetBBox(); }
	virtual void         SetBBox(const AABB& WSBBox) override;
	virtual void         OffsetPosition(const Vec3& delta) override;
	const float          GetRadius() const;
	static void          StaticReset();
	void                 UpdateRndFlags();
	ILINE int            GetStatObjGroupSize() const
	{
		return GetObjManager()->m_lstStaticTypes.Count();
	}
	ILINE StatInstGroup& GetStatObjGroup() const
	{
		return GetObjManager()->m_lstStaticTypes[m_nObjectTypeIndex];
	}
	ILINE CStatObj* GetStatObj() const
	{
		return GetObjManager()->m_lstStaticTypes[m_nObjectTypeIndex].GetStatObj();
	}
	float         GetZAngle() const;
	AABB          CalcBBox();
	void          CalcMatrix(Matrix34A& tm, int* pTransFags = NULL);
	virtual uint8 GetMaterialLayers() const override
	{
		return GetObjManager()->m_lstStaticTypes[m_nObjectTypeIndex].nMaterialLayers;
	}
	//	float GetLodForDistance(float fDistance);
	void         Init();
	void         ShutDown();
	void         OnRenderNodeBecomeVisibleAsync(SRenderNodeTempData* pTempData, const SRenderingPassInfo& passInfo) override;
	void         UpdateSpriteInfo(SVegetationSpriteInfo& properties, float fSpriteAmount, SSectorTextureSet* pTerrainTexInfo, const SRenderingPassInfo& passInfo) const;
	static void  InitVegDecomprTable();
	virtual bool GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const override;

	ILINE void   SetBoxExtends(byte* pBoxExtends, byte* pRadius, const AABB& RESTRICT_REFERENCE aabb, const Vec3& RESTRICT_REFERENCE vPos)
	{
		const float fRatio = (255.f / VEGETATION_CONV_FACTOR);
		Vec3 v0 = aabb.max - vPos;
		pBoxExtends[0] = (byte)SATURATEB(v0.x * fRatio + 1.f);
		pBoxExtends[1] = (byte)SATURATEB(v0.y * fRatio + 1.f);
		pBoxExtends[2] = (byte)SATURATEB(v0.z * fRatio + 1.f);
		Vec3 v1 = vPos - aabb.min;
		pBoxExtends[3] = (byte)SATURATEB(v1.x * fRatio + 1.f);
		pBoxExtends[4] = (byte)SATURATEB(v1.y * fRatio + 1.f);
		pBoxExtends[5] = (byte)SATURATEB(v1.z * fRatio + 1.f);
		*pRadius = (byte)SATURATEB(max(v0.GetLength(), v1.GetLength()) * fRatio + 1.f);
	}

	ILINE void FillBBoxFromExtends(AABB& aabb, const byte* const __restrict pBoxExtends, const Vec3& RESTRICT_REFERENCE vPos) const
	{
		if (m_pInstancingInfo)
		{
			aabb = m_pInstancingInfo->m_aabbBox;
			return;
		}

		const float* const __restrict cpDecompTable = g_scBoxDecomprTable;
		const float cData0 = cpDecompTable[pBoxExtends[0]];
		const float cData1 = cpDecompTable[pBoxExtends[1]];
		const float cData2 = cpDecompTable[pBoxExtends[2]];
		const float cData3 = cpDecompTable[pBoxExtends[3]];
		const float cData4 = cpDecompTable[pBoxExtends[4]];
		const float cData5 = cpDecompTable[pBoxExtends[5]];
		aabb.max.x = vPos.x + cData0;
		aabb.max.y = vPos.y + cData1;
		aabb.max.z = vPos.z + cData2;
		aabb.min.x = vPos.x - cData3;
		aabb.min.y = vPos.y - cData4;
		aabb.min.z = vPos.z - cData5;
	}

	ILINE void FillBBox_NonVirtual(AABB& aabb) const
	{
		FillBBoxFromExtends(aabb, m_boxExtends, m_vPos);
	}

	// Apply bending parameters to the CRenderObject
	void FillBendingData(CRenderObject* pObj) const;

	// Custom pool allocator for vegetation
	static void* operator new(size_t size, EAllocatorId allocatorId = eAllocator_Default);
	// Need to have a matching placement delete operator for durango compilation
	static void  operator delete(void* pToFree, EAllocatorId unused);
	static void  operator delete(void* pToFree);
	static void  GetStaticMemoryUsage(ICrySizer* pSizer);
};
