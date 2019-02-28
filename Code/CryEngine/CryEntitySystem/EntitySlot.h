// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// forward declaration.
class CEntity;
struct IStatObj;
struct ICharacterInstance;
struct IParticleEmitter;
struct SRendParams;
struct IMaterial;
struct SEntityUpdateContext;
struct IRenderNode;
struct IGeomCacheRenderNode;

typedef Matrix34 CWorldTransform;
typedef Vec3     CWorldPosition;

//////////////////////////////////////////////////////////////////////////
// Description:
//    CEntityObject defines a particular geometry object or character inside the entity.
//    Entity objects are organized in numbered slots, entity can have any number of slots,
//    where each slot is an instance of CEntityObject.
//////////////////////////////////////////////////////////////////////////
class CEntitySlot
{
public:
	explicit CEntitySlot(CEntity* pEntity);
	~CEntitySlot();

	// Called post entity initialization.
	void PostInit();

	// Set local transformation of the slot object.
	void SetLocalTM(const Matrix34& localTM);

	// Set camera space position of the object slot
	void SetCameraSpacePos(const Vec3& cameraSpacePos);

	// Get camera space position of the object slot
	void GetCameraSpacePos(Vec3& cameraSpacePos);

	// Add local object bounds to the one specified in parameter.
	bool GetLocalBounds(AABB& bounds);

	// Get local transformation matrix of entity object.
	const Matrix34& GetLocalTM() const { return m_localTM; };
	// Get world transformation matrix of entity object.
	const Matrix34& GetWorldTM() const { return m_worldTM; };

	// Clear all contents of the slot.
	void Clear();

	// Release all content objects in this slot.
	void ReleaseObjects();

	// Return true if slot bounds changed and bbox of entity needs to be recomputed
	bool GetBoundsChanged() const;

	// XForm slot.
	void OnXForm(int nWhyFlags);

	//////////////////////////////////////////////////////////////////////////
	// @see EEntitySlotFlags
	void       SetFlags(uint32 nFlags);
	// Flags are a combination of EEntitySlotFlags
	uint32     GetFlags() const { return m_flags; }
	void       SetRenderFlag(bool bEnable);

	void       SetHidemask(hidemask& mask);
	hidemask   GetHidemask() const { return m_nSubObjHideMask; };

	void       SetMaterial(IMaterial* pMtl);
	IMaterial* GetMaterial() const { return m_pMaterial; };

	//! Fill SEntitySlotInfo structure with contents of the Entity Slot
	void GetSlotInfo(SEntitySlotInfo& slotInfo) const;

	//////////////////////////////////////////////////////////////////////////
	void                  UpdateRenderNode(bool bForceRecreateNode = false);

	void                  SetAsLight(const SRenderLight& lightData, uint16 layerId = 0);

	IRenderNode*          GetRenderNode() const { return m_pRenderNode; }
	void                  SetRenderNode(IRenderNode* pRenderNode);

	EERType               GetRenderNodeType() const { return m_renderNodeType; }

	void                  DebugDraw(const SGeometryDebugDrawInfo& info);

	void                  ShallowCopyFrom(CEntitySlot* pSrcSlot);

	ICharacterInstance*   GetCharacter() const { return m_pCharacter; }
	void                  SetCharacter(ICharacterInstance* val);

	IStatObj*             GetStatObj() const { return m_pStatObj; }
	void                  SetStatObj(IStatObj* val);

	IParticleEmitter*     GetParticleEmitter() const;

	IGeomCacheRenderNode* GetGeomCacheRenderNode() const
	{
#if defined(USE_GEOM_CACHES)
		if (m_renderNodeType == eERType_GeomCache)
			return static_cast<IGeomCacheRenderNode*>(m_pRenderNode);
		else
#endif
		return nullptr;
	}

	//! Assign parent slot index
	void SetParent(int parent) { m_parent = parent; };
	//! Get parent slot index
	int  GetParent() const     { return m_parent; }

	void InvalidateParticleEmitter();

	//! Return True if this slot is being rendered now.
	bool IsRendered() const;

	//! Render this slot fo previewing in Editor.
	void PreviewRender(SEntityPreviewContext& context);

	void SetNeedSerialize(bool bNeedSerialize) { m_bNeedSerialize = bNeedSerialize; };
	bool NeedSerialize() const                 { return m_bNeedSerialize; }

private:
	void ComputeWorldTransform();

public:
	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete.
	//////////////////////////////////////////////////////////////////////////
	void* operator new(size_t nSize);
	void  operator delete(void* ptr);

private:
	// Owner entity.
	CEntity* m_pEntity = nullptr;
	// sub object hide mask, store previous one to render correct object while the new rendermesh is created in a thread
	hidemask m_nSubObjHideMask;

	Vec3     m_cameraSpacePos;

	//! Override material for this specific slot.
	_smart_ptr<IMaterial> m_pMaterial;

	//! Id of the parent slot.
	int m_parent = -1;

	//! Computed entity slot transformation in the world space.
	Matrix34 m_worldTM;
	//! Entity slot transformation in the local space relative to the entity and to the parent entity slot.
	Matrix34 m_localTM;

	// Internal usage flags.
	uint8 m_bNeedSerialize        : 1; //! This slot needs to be serialized on save/load.
	uint8 m_bCameraSpacePos       : 1; //! Slot render in camera space (m_cameraSpacePos member variable must be set)
	uint8 m_bRegisteredRenderNode : 1; //! RenderNode was registered to the 3D engine.
	uint8 m_bPostInit             : 1; //! Post Init was already called.

	std::underlying_type<EEntitySlotFlags>::type m_flags = 0;

	ICharacterInstance*                          m_pCharacter = nullptr;

	IStatObj*    m_pStatObj = nullptr;

	IRenderNode* m_pRenderNode = nullptr;
	EERType      m_renderNodeType;
};

//////////////////////////////////////////////////////////////////////////
extern stl::PoolAllocatorNoMT<sizeof(CEntitySlot), 16>* g_Alloc_EntitySlot;

//////////////////////////////////////////////////////////////////////////
// Custom new/delete.
//////////////////////////////////////////////////////////////////////////
inline void* CEntitySlot::operator new(size_t nSize)
{
	void* ptr = g_Alloc_EntitySlot->Allocate();
	if (ptr)
		memset(ptr, 0, nSize); // Clear objects memory.
	return ptr;
}

//////////////////////////////////////////////////////////////////////////
inline void CEntitySlot::operator delete(void* ptr)
{
	if (ptr)
		g_Alloc_EntitySlot->Deallocate(ptr);
}
