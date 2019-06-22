// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ObjMan.h"

struct ICharacterInstance;

class CCharacterRenderNode final : public ICharacterRenderNode, private Cry3DEngineBase
{
	friend class COctreeNode;

public:
	CCharacterRenderNode();
	virtual ~CCharacterRenderNode();

	//////////////////////////////////////////////////////////////////////////
	// Implement ICharacterRenderNode
	//////////////////////////////////////////////////////////////////////////
	virtual EERType     GetRenderNodeType() const override { return eERType_Character; }
	//! Debug info about object.
	virtual const char* GetName() const override;
	virtual const char* GetEntityClassName() const override          { return "CharacterRenderNode"; }
	virtual string      GetDebugString(char type = 0) const override { return GetName(); }

	//! Sets render node transformation matrix.
	virtual void SetMatrix(const Matrix34& transform) override;

	//! Gets local bounds of the render node.
	virtual void       GetLocalBounds(AABB& bbox) const override;

	virtual Vec3       GetPos(bool bWorldOnly = true) const override { return m_matrix.GetTranslation(); }
	virtual const AABB GetBBox() const override;
	virtual void       FillBBox(AABB& aabb) const override           { aabb = GetBBox(); }
	virtual void       SetBBox(const AABB& WSBBox) override          {}
	virtual void       OffsetPosition(const Vec3& delta) override;
	virtual void       Render(const struct SRendParams& EntDrawParams, const SRenderingPassInfo& passInfo) override;

	virtual CLodValue  ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo) override;
	virtual bool       GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const override;

	//! Gives access to object components.
	virtual ICharacterInstance* GetEntityCharacter(Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) override {  if(pMatrix) *pMatrix = m_matrix; return m_pCharacterInstance; }

	//! Get physical entity.
	virtual struct IPhysicalEntity* GetPhysics() const override                 { return m_pPhysicsEntity; }
	virtual void                    SetPhysics(IPhysicalEntity* pPhys) override { m_pPhysicsEntity = pPhys; }
	virtual void                    Physicalize(bool bInstant = false) override;

	//! Set override material for this instance.
	virtual void SetMaterial(IMaterial* pMat) override { m_pMaterial = pMat; }

	//! Queries override material of this instance.
	virtual IMaterial* GetMaterial(Vec3* pHitPos = NULL) const override;
	virtual IMaterial* GetMaterialOverride() const override { return m_pMaterial; }

	virtual float      GetMaxViewDist() const override;

	virtual void       OnRenderNodeVisible( bool bBecomeVisible ) override;
	virtual void       OnRenderNodeBecomeVisibleAsync(SRenderNodeTempData* pTempData, const SRenderingPassInfo& passInfo) override { OnRenderNodeVisible(true); }

	virtual void                              SetCameraSpaceParams(stl::optional<SCameraSpaceParams> cameraSpaceParams) override;
	virtual stl::optional<SCameraSpaceParams> GetCameraSpaceParams() const override;

	virtual void       GetMemoryUsage(ICrySizer* pSizer) const override {}

	virtual void       SetOwnerEntity(IEntity* pEntity) override  { m_pOwnerEntity = pEntity; }
	virtual IEntity*   GetOwnerEntity() const override            { return m_pOwnerEntity; }
	virtual bool       IsAllocatedOutsideOf3DEngineDLL() override { return GetOwnerEntity() != nullptr; }

	virtual void       UpdateStreamingPriority(const SUpdateStreamingPriorityContext& streamingContext) override;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Implement ICharacterRenderNode
	//////////////////////////////////////////////////////////////////////////
	virtual void SetCharacter(ICharacterInstance* pCharacter) final;
	//////////////////////////////////////////////////////////////////////////

	static void PrecacheCharacter(const float fImportance, ICharacterInstance* pCharacter, IMaterial* pSlotMat,
	                              const Matrix34& matParent, const float fEntDistance, const float fScale, int nMaxDepth, bool bFullUpdate, bool bDrawNear, int nLod);

private:
	void        CalcNearestTransform(Matrix34& transformMatrix, const SRenderingPassInfo& passInfo);

	static void PrecacheCharacterCollect(const float fImportance, ICharacterInstance* pCharacter, IMaterial* pSlotMat,
	                                     const Matrix34& matParent, const float fEntDistance, const float fScale, int nMaxDepth, bool bFullUpdate, bool bDrawNear, int nLod,
	                                     const int nRoundId, std::vector<std::pair<IMaterial*, float>>& collectedMaterials);

private:
	_smart_ptr<ICharacterInstance> m_pCharacterInstance;
	IPhysicalEntity*               m_pPhysicsEntity;

	// Override Material used to render node
	_smart_ptr<IMaterial> m_pMaterial;

	// World space transformation
	Matrix34 m_matrix;

	// Cached World space bounding box
	mutable AABB m_cachedBoundsWorld;
	// Cached Local space bounding box
	mutable AABB m_cachedBoundsLocal;

	stl::optional<SCameraSpaceParams> m_cameraSpaceParams = stl::nullopt;

	// When render node is created by the entity, pointer to the owner entity.
	IEntity* m_pOwnerEntity = 0;
};
