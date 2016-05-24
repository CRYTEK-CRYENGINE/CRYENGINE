// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _3DENGINE_BRUSH_H_
#define _3DENGINE_BRUSH_H_

#include "ObjMan.h"
#include "DeformableNode.h"

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <CryCore/Platform/platform.h>
#endif

class CBrush
	: public IBrush
	  , public Cry3DEngineBase
{
	friend class COctreeNode;

public:
	CBrush();
	virtual ~CBrush();

	virtual const char*         GetEntityClassName() const final;
	virtual Vec3                GetPos(bool bWorldOnly = true) const final;
	virtual const char*         GetName() const final;
	virtual bool                HasChanged();
	virtual void                Render(const struct SRendParams& EntDrawParams, const SRenderingPassInfo& passInfo) final;
	virtual CLodValue           ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo) final;
	void                        Render(const CLodValue& lodValue, const SRenderingPassInfo& passInfo, SSectorTextureSet* pTerrainTexInfo, PodArray<CDLight*>* pAffectingLights);

	virtual struct IStatObj*    GetEntityStatObj(unsigned int nPartId = 0, unsigned int nSubPartId = 0, Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) final;

	virtual bool                GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const final;

	virtual void                SetEntityStatObj(unsigned int nSlot, IStatObj* pStatObj, const Matrix34A* pMatrix = NULL) final;

	virtual IRenderNode*        Clone() const final;

	virtual void                SetCollisionClassIndex(int tableIndex) final { m_collisionClassIdx = tableIndex; }

	virtual void                SetLayerId(uint16 nLayerId) final            { m_nLayerId = nLayerId; Get3DEngine()->C3DEngine::UpdateObjectsLayerAABB(this); }
	virtual uint16              GetLayerId() final                           { return m_nLayerId; }
	virtual struct IRenderMesh* GetRenderMesh(int nLod) final;

	virtual IPhysicalEntity*    GetPhysics() const final;
	virtual void                SetPhysics(IPhysicalEntity* pPhys) final;
	static bool                 IsMatrixValid(const Matrix34& mat);
	virtual void                Dephysicalize(bool bKeepIfReferenced = false) final;
	virtual void                Physicalize(bool bInstant = false) final;
	void                        PhysicalizeOnHeap(IGeneralMemoryHeap* pHeap, bool bInstant = false);
	virtual bool                PhysicalizeFoliage(bool bPhysicalize = true, int iSource = 0, int nSlot = 0) final;
	virtual IPhysicalEntity*    GetBranchPhys(int idx, int nSlot = 0) final { IFoliage* pFoliage = GetFoliage(nSlot); return pFoliage ? pFoliage->GetBranchPhysics(idx) : 0; }
	virtual struct IFoliage*    GetFoliage(int nSlot = 0) final;

	//! Assign final material to this entity.
	virtual void       SetMaterial(IMaterial* pMat) final;
	virtual IMaterial* GetMaterial(Vec3* pHitPos = NULL) const final;
	virtual IMaterial* GetMaterialOverride() final { return m_pMaterial; };
	virtual void       CheckPhysicalized() final;

	virtual float      GetMaxViewDist() final;

	virtual EERType    GetRenderNodeType() final;

	void               SetStatObj(IStatObj* pStatObj);

	void               SetMatrix(const Matrix34& mat) final;
	const Matrix34& GetMatrix() const final        { return m_Matrix; }
	virtual void    SetDrawLast(bool enable) final { m_bDrawLast = enable; }
	bool            GetDrawLast() const            { return m_bDrawLast; }

	void            Dematerialize() final;
	virtual void    GetMemoryUsage(ICrySizer* pSizer) const final;
	static PodArray<SExportedBrushMaterial> m_lstSExportedBrushMaterials;

	virtual const AABB GetBBox() const final;
	virtual void       SetBBox(const AABB& WSBBox) final { m_WSBBox = WSBBox; }
	virtual void       FillBBox(AABB& aabb) final;
	virtual void       OffsetPosition(const Vec3& delta) final;

	virtual bool       CanExecuteRenderAsJob() final;

	//private:
	void CalcBBox();
	void UpdatePhysicalMaterials(int bThreadSafe = 0);

	void OnRenderNodeBecomeVisible(const SRenderingPassInfo& passInfo) final;

	void UpdateExecuteAsPreProcessJobFlag();

	bool HasDeformableData() const { return m_pDeform != NULL; }

	Matrix34         m_Matrix;
	float            m_fMatrixScale;
	IPhysicalEntity* m_pPhysEnt;

	//! final material.
	_smart_ptr<IMaterial> m_pMaterial;

	uint16                m_collisionClassIdx;
	uint16                m_nLayerId;

	_smart_ptr<IStatObj>  m_pStatObj;
	CDeformableNode*      m_pDeform;

	bool                  m_bVehicleOnlyPhysics;

	bool                  m_bMerged;
	bool                  m_bExecuteAsPreprocessJob;
	bool                  m_bDrawLast;

	AABB                  m_WSBBox;
};

///////////////////////////////////////////////////////////////////////////////
inline const AABB CBrush::GetBBox() const
{
	return m_WSBBox;
}

#endif // _3DENGINE_BRUSH_H_
