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
	virtual EERType     GetRenderNodeType() final { return eERType_Character; }
	//! Debug info about object.
	virtual const char* GetName() const final;
	virtual const char* GetEntityClassName() const final          { return "CharacterRenderNode"; };
	virtual string      GetDebugString(char type = 0) const final { return GetName(); }

	//! Sets render node transformation matrix.
	virtual void SetMatrix(const Matrix34& transform) final;

	//! Gets local bounds of the render node.
	virtual void       GetLocalBounds(AABB& bbox) final;

	virtual Vec3       GetPos(bool bWorldOnly = true) const final { return m_matrix.GetTranslation(); };
	virtual const AABB GetBBox() const final;
	virtual void       FillBBox(AABB& aabb) final                 { aabb = GetBBox(); }
	virtual void       SetBBox(const AABB& WSBBox) final          {}
	virtual void       OffsetPosition(const Vec3& delta) final;
	virtual void       Render(const struct SRendParams& EntDrawParams, const SRenderingPassInfo& passInfo) final;

	virtual CLodValue  ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo) final;
	virtual bool       GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const final;

	//! Gives access to object components.
	virtual ICharacterInstance* GetEntityCharacter(Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) final {  if(pMatrix) *pMatrix = m_matrix; return m_pCharacterInstance; }

	//! Get physical entity.
	virtual struct IPhysicalEntity* GetPhysics() const final                 { return m_pPhysicsEntity; };
	virtual void                    SetPhysics(IPhysicalEntity* pPhys) final { m_pPhysicsEntity = pPhys; };
	virtual void                    Physicalize(bool bInstant = false) final;

	//! Set override material for this instance.
	virtual void SetMaterial(IMaterial* pMat) final { m_pMaterial = pMat; };

	//! Queries override material of this instance.
	virtual IMaterial* GetMaterial(Vec3* pHitPos = NULL) const final { return nullptr; };
	virtual IMaterial* GetMaterialOverride() final                   { return m_pMaterial; };

	virtual float      GetMaxViewDist() final;

	virtual void       OnRenderNodeVisible( bool bBecomeVisible ) final;

	virtual void       SetCameraSpacePos(Vec3* pCameraSpacePos) final;

	virtual void       GetMemoryUsage(ICrySizer* pSizer) const final {};

	virtual void       SetOwnerEntity(IEntity* pEntity) final { m_pOwnerEntity = pEntity; }
	virtual IEntity*   GetOwnerEntity() const final           { return m_pOwnerEntity; }
	virtual bool       IsAllocatedOutsideOf3DEngineDLL()      { return GetOwnerEntity() != nullptr; }

	virtual void       UpdateStreamingPriority(const SUpdateStreamingPriorityContext& streamingContext) final;
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

	Vec3*    m_pCameraSpacePos = nullptr;

	// When render node is created by the entity, pointer to the owner entity.
	IEntity* m_pOwnerEntity = 0;
};
