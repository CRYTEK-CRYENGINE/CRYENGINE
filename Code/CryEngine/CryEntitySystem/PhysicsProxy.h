// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

// forward declarations.
struct SEntityEvent;
struct IPhysicalEntity;
struct IPhysicalWorld;

#if 0 // uncomment if more than 64 parts are needed and the mask can no longer fit into an int64
	#include <CryCore/BitMask.h>
typedef bitmask_t<bitmaskPtr, 4>    attachMask;
typedef bitmask_t<bitmaskBuf<4>, 4> attachMaskLoc;
	#define attachMask1 (bitmask_t<bitmaskOneBit, 4>(1))
#else
typedef uint32 attachMask, attachMaskLoc;
	#define attachMask1 1
#endif

// Implements physical behavior of the entity.
class CEntityPhysics
{
public:
	enum EFlags
	{
		CHARACTER_SLOT_MASK = 0x000F,     // Slot Id, of physicalized character.
		// When set Physical proxy will ignore incoming xform events from the entity.
		// Needed to prevent cycle change, when physical entity change entity xform and recieve back an event about entity xform.
		FLAG_IGNORE_XFORM_EVENT        = 1 << 4,
		FLAG_IGNORE_BUOYANCY           = 1 << 5,
		FLAG_PHYSICS_DISABLED          = 1 << 6,
		FLAG_SYNC_CHARACTER            = 1 << 7,
		FLAG_WAS_HIDDEN                = 1 << 8,
		FLAG_PHYS_CHARACTER            = 1 << 9,
		FLAG_PHYS_AWAKE_WHEN_VISIBLE   = 1 << 10,
		FLAG_ATTACH_CLOTH_WHEN_VISIBLE = 1 << 11,
		FLAG_POS_EXTRAPOLATED          = 1 << 12,
		FLAG_DISABLE_ENT_SERIALIZATION = 1 << 13,
		FLAG_PHYSICS_REMOVED           = 1 << 14,
		// Whether or not the physics proxy currently requires that the entity is active / updated every frame.
		FLAG_ACTIVE                    = 1 << 15,
	};

	CEntityPhysics();
	~CEntityPhysics();
	;
	CEntity* GetEntity() const { return m_pEntity; };

	void     ProcessEvent(const SEntityEvent& event);

	void     SerializeXML(XmlNodeRef& entityNode, bool bLoading);
	bool     NeedNetworkSerialize();
	void     SerializeTyped(TSerialize ser, int type, int flags);
	void     EnableNetworkSerialization(bool enable);
	//////////////////////////////////////////////////////////////////////////

	void             Serialize(TSerialize ser);

	void             GetLocalBounds(AABB& bbox) const;
	void             GetWorldBounds(AABB& bbox) const;

	void             Physicalize(SEntityPhysicalizeParams& params);
	IPhysicalEntity* GetPhysicalEntity() const { return m_pPhysicalEntity; }
	void             EnablePhysics(bool bEnable);
	bool             IsPhysicsEnabled() const;
	void             AddImpulse(int ipart, const Vec3& pos, const Vec3& impulse, bool bPos, float fAuxScale, float fPushScale = 1.0f);

	int              GetPartId0(int nSlot = 0);
	int              GetPhysAttachId();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Called from physics events.
	//////////////////////////////////////////////////////////////////////////
	// Called back by physics PostStep event for the owned physical entity.
	void OnPhysicsPostStep(EventPhysPostStep* pEvent = 0);
	void AttachToPhysicalEntity(IPhysicalEntity* pPhysEntity);
	void CreateRenderGeometry(int nSlot, IGeometry* pFromGeom, bop_meshupdate* pLastUpdate = 0);
	void OnContactWithEntity(CEntity* pEntity);
	void OnCollision(CEntity* pTarget, int matId, const Vec3& pt, const Vec3& n, const Vec3& vel, const Vec3& targetVel, int partId, float mass);
	//////////////////////////////////////////////////////////////////////////

	void              SetFlags(int nFlags)            { m_nFlags = nFlags; };
	uint32            GetFlags() const                { return m_nFlags; };
	bool              CheckFlags(uint32 nFlags) const { return (m_nFlags & nFlags) == nFlags; }
	void              UpdateSlotGeometry(int nSlot, IStatObj* pStatObjNew = 0, float mass = -1.0f, int bNoSubslots = 1);
	void              AssignPhysicalEntity(IPhysicalEntity* pPhysEntity, int nSlot = -1);

	virtual bool      PhysicalizeFoliage(int iSlot);
	virtual void      DephysicalizeFoliage(int iSlot);
	virtual IFoliage* GetFoliage(int iSlot);

	int               AddSlotGeometry(int nSlot, SEntityPhysicalizeParams& params, int bNoSubslots = 1);
	void              RemoveSlotGeometry(int nSlot);

	void              MovePhysics(CEntityPhysics* dstPhysics);

	virtual void      GetMemoryUsage(ICrySizer* pSizer) const;
	void              ReattachSoftEntityVtx(IPhysicalEntity* pAttachToEntity, int nAttachToPart);

#if !defined(_RELEASE)
	static void EnableValidation();
	static void DisableValidation();
#endif

	void               SetActive(bool bActive);

private:
	IPhysicalWorld*  PhysicalWorld() const { return gEnv->pPhysicalWorld; }
	void             OnEntityXForm(const SEntityEvent& event);
	void             OnChangedPhysics(bool bEnabled);
	void             DestroyPhysicalEntity(bool bDestroyCharacters = true, int iMode = 0);

	void             PhysicalizeSimple(SEntityPhysicalizeParams& params);
	void             PhysicalizeLiving(SEntityPhysicalizeParams& params);
	void             PhysicalizeParticle(SEntityPhysicalizeParams& params);
	void             PhysicalizeSoft(SEntityPhysicalizeParams& params);
	void             AttachSoftVtx(IRenderMesh* pRM, IPhysicalEntity* pAttachToEntity, int nAttachToPart);
	void             PhysicalizeArea(SEntityPhysicalizeParams& params);
	bool             PhysicalizeGeomCache(SEntityPhysicalizeParams& params);
	bool             PhysicalizeCharacter(SEntityPhysicalizeParams& params);
	bool             ConvertCharacterToRagdoll(SEntityPhysicalizeParams& params, const Vec3& velInitial);

	void             CreatePhysicalEntity(SEntityPhysicalizeParams& params);
	phys_geometry*   GetSlotGeometry(int nSlot);
	void             SyncCharacterWithPhysics();

	void             MoveChildPhysicsParts(IPhysicalEntity* pSrcAdam, CEntity* pChild, pe_action_move_parts& amp, uint64 usedRanges);

	IPhysicalEntity* QueryPhyscalEntity(IEntity* pEntity) const;
	CEntity*         GetCEntity(IPhysicalEntity* pPhysEntity);

	void             ReleasePhysicalEntity();

	void             RemapChildAttachIds(CEntity* pent, attachMask& idmaskSrc, attachMask& idmaskDst, int* idmap);

	bool             TriggerEventIfStateChanged(IPhysicalEntity* pPhysEntity, const pe_status_pos* pPrevStatus) const;
	// Figures out render material at slot nSlot, and fills necessary data into the ppart output structure
	void             UpdateParamsFromRenderMaterial(int nSlot, IPhysicalEntity* pPhysEntity);

	void             AwakeOnRender(bool vRender);

private:
	friend class CEntity;

	uint32           m_nFlags = 0;
	CEntity*         m_pEntity = nullptr;
	// Pointer to physical object.
	IPhysicalEntity* m_pPhysicalEntity = nullptr;
};