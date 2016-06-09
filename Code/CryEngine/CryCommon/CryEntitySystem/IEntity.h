// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IEntity.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryEntitySystem/IComponent.h>

// Forward declarations.
struct IPhysicalEntity;
struct IEntityClass;
struct IEntityArchetype;
class CStream;
class CCamera;
struct EntityCloneState;
struct IEntity;
struct SEntityUpdateContext;
struct IEntitySerializationContext;
struct IScriptTable;
struct AABB;
class XmlNodeRef;
class CDLight;
struct AIObjectParams;
struct IParticleEffect;
struct SpawnParams;
struct IComponentEventDistributer;
struct IEntityProxy;
struct IGeomCacheRenderNode;
struct ICharacterInstance;
struct IParticleEmitter;
struct IStatObj;

//////////////////////////////////////////////////////////////////////////
struct IGameObject;
struct IAIObject;
struct IMaterial;
//////////////////////////////////////////////////////////////////////////

#define FORWARD_DIRECTION Vec3(0, 1, 0)

typedef unsigned int EntityId;          //!< Unique identifier for each entity instance. Don't change the type!
#define INVALID_ENTITYID ((EntityId)(0))

//! Unique Entity identifier, only used for saved entities.
typedef uint64 EntityGUID;

#if !defined(SWIG)
enum EPartIds
{
	PARTID_MAX_SLOTS_log2  = 9,
	PARTID_MAX_SLOTS       = 1 << PARTID_MAX_SLOTS_log2, //!< The maximum amount of slots in entity/skeleton/compound statobj.
	PARTID_MAX_ATTACHMENTS = 32,                         //!< The maximum amount of physicalized entity attachments(must be a power of 2 and <32).
	PARTID_LINKED          = 1 << 30,                    //!< Marks partids that belong to linked entities.
};
#endif

//! Phys part id format:.
//! Bit 31 = 0 (since partids can't be negative).
//! Bit 30 - 1 if part belongs to an attached entity.
//! Bits 29,28 - count of nested levels (numLevels) - 1, i.e. compound statobj within a cga within an entity.
//! Lowest PARTID_MAX_SLOTS_log2*numLevels bits - indices of slots at the corresponding level.
//! Bits directly above those - attachment id if bit 30 is set.
inline int ParsePartId(int partId, int& nLevels, int& nSlotBits)
{
	nSlotBits = (nLevels = (partId >> 28 & 3) + 1) * PARTID_MAX_SLOTS_log2;
	return partId >> 30 & 1;
}

//! Allocate next level of slots.
inline int AllocPartIdRange(int partId, int nSlots)
{
	int nLevels, nBits;
	ParsePartId(partId, nLevels, nBits);
	return partId & 1 << 30 | nLevels << 28 | (partId & (1 << 28) - 1) << PARTID_MAX_SLOTS_log2;
}

//! Extract slot index from partid; 0 = top level.
inline int GetSlotIdx(int partId, int level = 0)
{
	int nLevels, nBits;
	ParsePartId(partId, nLevels, nBits);
	return partId >> (nLevels - 1 - level) * PARTID_MAX_SLOTS_log2 & PARTID_MAX_SLOTS - 1;
}

// (MATT) This should really live in a minimal AI include, which right now we don't have  {2009/04/08}
#ifndef INVALID_AIOBJECTID
typedef uint32 tAIObjectID;
	#define INVALID_AIOBJECTID ((tAIObjectID)(0))
#endif

//////////////////////////////////////////////////////////////////////////
#include <CryEntitySystem/IEntityProxy.h>
#include <CryNetwork/SerializeFwd.h>
//////////////////////////////////////////////////////////////////////////

struct SEntitySpawnParams
{
	//! The Entity unique identifier (EntityId).
	//! If 0 then an ID will be generated automatically (based on the bStaticEntityId parameter).
	EntityId   id;
	EntityId   prevId;          //!< Previously used entityId, in the case of reloading.

	EntityGUID guid;            //!< Optional entity guid.
	EntityGUID prevGuid;        //!< Previously used entityGuid, in the case of reloading.

	//! Class of entity.
	IEntityClass* pClass;

	//! Entity archetype.
	IEntityArchetype* pArchetype;

	//! The name of the layer the entity resides in, when in the Editor.
	const char* sLayerName;

	//! Reference to entity's xml node in level data.
	XmlNodeRef entityNode;

	//////////////////////////////////////////////////////////////////////////
	// Initial Entity parameters.
	const char* sName;          //!< \note The name of the entity does not need to be unique.
	uint32      nFlags;         //!< Entity flags, e.g. ENTITY_FLAG_CASTSHADOW.
	uint32      nFlagsExtended; //!< EEntityFlagsExtended flags.
	bool        bIgnoreLock;    //!< Spawn lock.

	//! To support save games compatible with patched levels (patched levels might use.
	//! More EntityIDs and save game might conflict with dynamic ones).
	bool  bStaticEntityId;

	Vec3  vPosition;                  //!< Initial entity position (Local space).
	Quat  qRotation;                  //!< Initial entity rotation (Local space).
	Vec3  vScale;                     //!< Initial entity scale (Local space).
	void* pUserData;                  //!< Any user defined data. It will be available for container when it will be created.
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Optional properties table.
	IScriptTable* pPropertiesTable;
	IScriptTable* pPropertiesInstanceTable;
	//////////////////////////////////////////////////////////////////////////

	SEntitySpawnParams()
		: id(0)
		, prevId(0)
		, guid(0)
		, prevGuid(0)
		, nFlags(0)
		, nFlagsExtended(0)
		, bIgnoreLock(false)
		, bStaticEntityId(false)
		, pClass(NULL)
		, sName("")
		, sLayerName("")
		, pArchetype(NULL)
		, vPosition(ZERO)
		, vScale(Vec3Constants<float>::fVec3_One)
		, pUserData(0)
		, pPropertiesTable(0)
		, pPropertiesInstanceTable(0)
	{
		qRotation.SetIdentity();
	}
};

//! Entity update context structure.
struct SEntityUpdateContext
{
	int      nFrameID;            //!< Current rendering frame id.
	CCamera* pCamera;             //!< Current camera.
	float    fCurrTime;           //!< Current system time.
	float    fFrameTime;          //!< Delta frame time (of last frame).
	bool     bProfileToLog;       //!< Indicates if a profile entity must update the log.
	int      numUpdatedEntities;  //!< Number of updated entities.
	int      numVisibleEntities;  //!< Number of visible and updated entities.
	float    fMaxViewDist;        //!< Maximal view distance.
	float    fMaxViewDistSquared; //!< Maximal view distance squared.
	Vec3     vCameraPos;          //!< Camera source position.

	SEntityUpdateContext() : nFrameID(0), pCamera(0), fCurrTime(0),
		bProfileToLog(false), numVisibleEntities(0), numUpdatedEntities(0), fMaxViewDist(1e+8), fFrameTime(0) {};
};

enum EEntityXFormFlags
{
	ENTITY_XFORM_POS                      = BIT(1),
	ENTITY_XFORM_ROT                      = BIT(2),
	ENTITY_XFORM_SCL                      = BIT(3),
	ENTITY_XFORM_NO_PROPOGATE             = BIT(4),
	ENTITY_XFORM_FROM_PARENT              = BIT(5),   //!< When parent changes his transformation.
	ENTITY_XFORM_PHYSICS_STEP             = BIT(13),
	ENTITY_XFORM_EDITOR                   = BIT(14),
	ENTITY_XFORM_TRACKVIEW                = BIT(15),
	ENTITY_XFORM_TIMEDEMO                 = BIT(16),
	ENTITY_XFORM_NOT_REREGISTER           = BIT(17),  //!< Optimization flag, when set object will not be re-registered in 3D engine.
	ENTITY_XFORM_NO_EVENT                 = BIT(18),  //!< Suppresses ENTITY_EVENT_XFORM event.
	ENTITY_XFORM_NO_SEND_TO_ENTITY_SYSTEM = BIT(19),
	ENTITY_XFORM_USER                     = 0x1000000,
};

//! EEntityEvent defines all events that can be sent to an entity.
enum EEntityEvent
{
	//! Sent when the entity local or world transformation matrix change (position/rotation/scale).
	//! nParam[0] = combination of the EEntityXFormFlags.
	ENTITY_EVENT_XFORM = 0,

	//! Called when the entity is moved/scaled/rotated in the editor. Only send on mouseButtonUp (hence finished).
	ENTITY_EVENT_XFORM_FINISHED_EDITOR,

	//! Sent when the entity timer expire.
	//! nParam[0] = TimerId, nParam[1] = milliseconds.
	ENTITY_EVENT_TIMER,

	//! Sent for unremovable entities when they are respawn.
	ENTITY_EVENT_INIT,

	//! Sent before entity is removed.
	ENTITY_EVENT_DONE,

	//! Sent when the entity becomes visible or invisible.
	//! nParam[0] is 1 if the entity becomes visible or 0 if the entity becomes invisible.
	ENTITY_EVENT_VISIBLITY,

	//! Sent to reset the state of the entity (used from Editor).
	//! nParam[0] is 1 if entering gamemode, 0 if exiting
	ENTITY_EVENT_RESET,

	//! Sent to parent entity after child entity have been attached.
	//! nParam[0] contains ID of child entity.
	ENTITY_EVENT_ATTACH,

	//! Sent to child entity after it has been attached to the parent.
	//! nParam[0] contains ID of parent entity.
	ENTITY_EVENT_ATTACH_THIS,

	//! Sent to parent entity after child entity have been detached.
	//! nParam[0] contains ID of child entity.
	ENTITY_EVENT_DETACH,

	//! Sent to child entity after it has been detached from the parent.
	//! nParam[0] contains ID of parent entity.
	ENTITY_EVENT_DETACH_THIS,

	//! Sent to parent entity after child entity have been linked.
	//! nParam[0] contains IEntityLink ptr.
	ENTITY_EVENT_LINK,

	//! Sent to parent entity before child entity have been delinked.
	//! nParam[0] contains IEntityLink ptr.
	ENTITY_EVENT_DELINK,

	//! Sent when the entity must be hidden.
	ENTITY_EVENT_HIDE,

	//! Sent when the entity must become not hidden.
	ENTITY_EVENT_UNHIDE,

	//! Sent when a physics processing for the entity must be enabled/disabled.
	//! nParam[0] == 1 physics must be enabled if 0 physics must be disabled.
	ENTITY_EVENT_ENABLE_PHYSICS,

	//! Sent when a physics in an entity changes state.
	//! nParam[0] == 1 physics entity awakes, 0 physics entity get to a sleep state.
	ENTITY_EVENT_PHYSICS_CHANGE_STATE,

	//! Sent when script is broadcasting its events.
	//! nParam[0] = Pointer to the ASCIIZ string with the name of the script event.
	//! nParam[1] = Type of the event value from IEntityClass::EventValueType.
	//! nParam[2] = Pointer to the event value depending on the type.
	ENTITY_EVENT_SCRIPT_EVENT,

	//! Sent when triggering entity enters to the area proximity, this event sent to all target entities of the area.
	//! nParam[0] = TriggerEntityId, nParam[1] = AreaId, nParam[2] = EntityId of Area
	ENTITY_EVENT_ENTERAREA,

	//! Sent when triggering entity leaves the area proximity, this event sent to all target entities of the area.
	//! nParam[0] = TriggerEntityId, nParam[1] = AreaId, nParam[2] = EntityId of Area
	ENTITY_EVENT_LEAVEAREA,

	//! Sent when triggering entity is near to the area proximity, this event sent to all target entities of the area.
	//! nParam[0] = TriggerEntityId, nParam[1] = AreaId, nParam[2] = EntityId of Area
	ENTITY_EVENT_ENTERNEARAREA,

	//! Sent when triggering entity leaves the near area within proximity region of the outside area border.
	//! nParam[0] = TriggerEntityId, nParam[1] = AreaId, nParam[2] = EntityId of Area
	ENTITY_EVENT_LEAVENEARAREA,

	//! Sent when triggering entity moves inside the area within proximity region of the outside area border.
	//! nParam[0] = TriggerEntityId, nParam[1] = AreaId, nParam[2] = EntityId of Area
	ENTITY_EVENT_MOVEINSIDEAREA,

	// Sent when triggering entity moves inside an area of higher priority then the area this entity is linked to.
	// nParam[0] = TriggerEntityId, nParam[1] = AreaId, nParam[2] = EntityId of Area with low prio, nParam[3] = EntityId of Area with high prio
	//ENTITY_EVENT_EXCLUSIVEMOVEINSIDEAREA,

	//! Sent when triggering entity moves inside the area within the near region of the outside area border.
	//! nParam[0] = TriggerEntityId, nParam[1] = AreaId, nParam[2] = EntityId of Area, fParam[0] = FadeRatio (0-1)
	ENTITY_EVENT_MOVENEARAREA,

	//! Sent when triggering entity enters or leaves an area so all active areas of same group get notified. This event is sent to all target entities of the area.
	ENTITY_EVENT_CROSS_AREA,

	//! Sent when an entity with pef_monitor_poststep receives a poststep notification (the hamdler should be thread safe!)
	//! fParam[0] = time interval
	ENTITY_EVENT_PHYS_POSTSTEP,

	//! Sent when Breakable object is broken in physics.
	ENTITY_EVENT_PHYS_BREAK,

	//! Sent when AI object of the entity finished executing current order/action.
	ENTITY_EVENT_AI_DONE,

	//! Sent when a sound finished or was stopped playing.
	ENTITY_EVENT_SOUND_DONE,

	//! Sent when an entity has not been rendered for a while (the time specified via cvar "es_not_seen_timeout")
	//! \note This is sent only if ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT is set.
	ENTITY_EVENT_NOT_SEEN_TIMEOUT,

	//! Physical collision.
	ENTITY_EVENT_COLLISION,

	//! Called when entity is rendered (Only if ENTITY_FLAG_SEND_RENDER_EVENT is set).
	//! nParam[0] is a pointer to the current rendering SRenderParams structure.
	ENTITY_EVENT_RENDER,

	//! Called when the pre-physics update is done; fParam[0] is the frame time.
	ENTITY_EVENT_PREPHYSICSUPDATE,

	//! Called when the level loading is complete.
	ENTITY_EVENT_LEVEL_LOADED,

	//! Called when the level is started.
	ENTITY_EVENT_START_LEVEL,

	//! Called when the game is started (games may start multiple times).
	ENTITY_EVENT_START_GAME,

	//! Called when the entity enters a script state.
	ENTITY_EVENT_ENTER_SCRIPT_STATE,

	//! Called when the entity leaves a script state.
	ENTITY_EVENT_LEAVE_SCRIPT_STATE,

	//! Called before we serialized the game from file.
	ENTITY_EVENT_PRE_SERIALIZE,

	//! Called after we serialized the game from file.
	ENTITY_EVENT_POST_SERIALIZE,

	//! Called when the entity becomes invisible.
	ENTITY_EVENT_INVISIBLE,

	//! Called when the entity gets out of invisibility.
	ENTITY_EVENT_VISIBLE,

	//! Called when the entity material change.
	//! nParam[0] = pointer to the new IMaterial.
	ENTITY_EVENT_MATERIAL,

	//! Called when the entitys material layer mask changes.
	ENTITY_EVENT_MATERIAL_LAYER,

	//! Called when the entity gets hits by a weapon.
	ENTITY_EVENT_ONHIT,

	//! Called when an animation event (placed on animations in editor) is encountered.
	//! nParam[0] = AnimEventInstance* pEventParameters.
	ENTITY_EVENT_ANIM_EVENT,

	//! Called from ScriptBind_Entity when script requests to set collidermode.
	//! nParam[0] = ColliderMode
	ENTITY_EVENT_SCRIPT_REQUEST_COLLIDERMODE,

	//! Called to activate some output in a flow node connected to the entity
	//! nParam[0] = Output port index
	//! nParam[1] = TFlowInputData* to send to output
	ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT,

	//! Called in the editor when some property of the current selected entity changes.
	ENTITY_EVENT_EDITOR_PROPERTY_CHANGED,

	//! Called when a script reloading is requested and done in the editor.
	ENTITY_EVENT_RELOAD_SCRIPT,

	//! Called when the entity is added to the list of entities that are updated.
	ENTITY_EVENT_ACTIVATED,

	//! Called when the entity is removed from the list of entities that are updated.
	ENTITY_EVENT_DEACTIVATED,

	//! Called when the entity should be added to the radar.
	ENTITY_EVENT_ADD_TO_RADAR,

	//! Called when the entity should be removed from the radar.
	ENTITY_EVENT_REMOVE_FROM_RADAR,

	//! Last entity event in list.
	ENTITY_EVENT_LAST,
};

//! Variant of default BIT macro to safely handle 64-bit numbers.
#define ENTITY_EVENT_BIT(x) (1ULL << (x))

//! SEntityEvent structure describe event id and parameters that can be sent to an entity.
struct SEntityEvent
{
	SEntityEvent(
	  const int n0,
	  const int n1,
	  const int n2,
	  const int n3,
	  const float f0,
	  const float f1,
	  const float f2,
	  Vec3 const& _vec)
		: vec(_vec)
	{
		nParam[0] = n0;
		nParam[1] = n1;
		nParam[2] = n2;
		nParam[3] = n3;
		fParam[0] = f0;
		fParam[1] = f1;
		fParam[2] = f2;
	}

	SEntityEvent()
		: event(ENTITY_EVENT_LAST)
		, vec(ZERO)
	{
		nParam[0] = nParam[1] = nParam[2] = nParam[3] = 0;
		fParam[0] = fParam[1] = fParam[2] = 0.0f;
	}

	SEntityEvent(EEntityEvent _event)
		: event(_event)
		, vec(ZERO)
	{
		nParam[0] = nParam[1] = nParam[2] = nParam[3] = 0;
		fParam[0] = fParam[1] = fParam[2] = 0.0f;
	}

	EEntityEvent event;     //!< Any event from EEntityEvent enum.
	INT_PTR      nParam[4]; //!< Event parameters.
	float        fParam[3];
	Vec3         vec;
};

//! Updates policy defines in which cases to call entity update function every frame.
enum EEntityUpdatePolicy
{
	ENTITY_UPDATE_NEVER,           //!< Never update entity every frame.
	ENTITY_UPDATE_IN_RANGE,        //!< Only update entity if it is in specified range from active camera.
	ENTITY_UPDATE_POT_VISIBLE,     //!< Only update entity if it is potentially visible.
	ENTITY_UPDATE_VISIBLE,         //!< Only update entity if it is visible.
	ENTITY_UPDATE_PHYSICS,         //!< Only update entity if it is need to be updated due to physics.
	ENTITY_UPDATE_PHYSICS_VISIBLE, //!< Only update entity if it is need to be updated due to physics or if it is visible.
	ENTITY_UPDATE_ALWAYS,          //!< Always update entity every frame.
};

//! Flags can be set on entity with SetFlags/GetFlags method.
enum EEntityFlags
{
	//////////////////////////////////////////////////////////////////////////
	// Persistent flags (can be set from the editor).
	ENTITY_FLAG_CASTSHADOW          = BIT(1),
	ENTITY_FLAG_UNREMOVABLE         = BIT(2),        //!< This entity cannot be removed using IEntitySystem::RemoveEntity until this flag is cleared.
	ENTITY_FLAG_GOOD_OCCLUDER       = BIT(3),
	ENTITY_FLAG_NO_DECALNODE_DECALS = BIT(4),
	//////////////////////////////////////////////////////////////////////////

	ENTITY_FLAG_WRITE_ONLY              = BIT(5),
	ENTITY_FLAG_NOT_REGISTER_IN_SECTORS = BIT(6),
	ENTITY_FLAG_CALC_PHYSICS            = BIT(7),
	ENTITY_FLAG_CLIENT_ONLY             = BIT(8),
	ENTITY_FLAG_SERVER_ONLY             = BIT(9),
	ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO   = BIT(10),  //!< This entity have special custom view distance ratio (AI/Vehicles must have it).
	ENTITY_FLAG_CALCBBOX_USEALL         = BIT(11),  //!< use character and objects in BBOx calculations.
	ENTITY_FLAG_VOLUME_SOUND            = BIT(12),  //!< Entity is a volume sound (will get moved around by the sound proxy).
	ENTITY_FLAG_HAS_AI                  = BIT(13),  //!< Entity has an AI object.
	ENTITY_FLAG_TRIGGER_AREAS           = BIT(14),  //!< This entity will trigger areas when it enters them.
	ENTITY_FLAG_NO_SAVE                 = BIT(15),  //!< This entity will not be saved.
	ENTITY_FLAG_CAMERA_SOURCE           = BIT(16),  //!< This entity is a camera source.
	ENTITY_FLAG_CLIENTSIDE_STATE        = BIT(17),  //!< Prevents error when state changes on the client and does not sync state changes to the client.
	ENTITY_FLAG_SEND_RENDER_EVENT       = BIT(18),  //!< When set entity will send ENTITY_EVENT_RENDER every time its rendered.
	ENTITY_FLAG_NO_PROXIMITY            = BIT(19),  //!< Entity will not be registered in the partition grid and can not be found by proximity queries.
	ENTITY_FLAG_PROCEDURAL              = BIT(20),  //!< Entity has been generated at runtime.
	ENTITY_FLAG_UPDATE_HIDDEN           = BIT(21),  //!< Entity will be update even when hidden.
	ENTITY_FLAG_NEVER_NETWORK_STATIC    = BIT(22),  //!< Entity should never be considered a static entity by the network system.
	ENTITY_FLAG_IGNORE_PHYSICS_UPDATE   = BIT(23),  //!< Used by Editor only, (don't set).
	ENTITY_FLAG_SPAWNED                 = BIT(24),  //!< Entity was spawned dynamically without a class.
	ENTITY_FLAG_SLOTS_CHANGED           = BIT(25),  //!< Entity's slots were changed dynamically.
	ENTITY_FLAG_MODIFIED_BY_PHYSICS     = BIT(26),  //!< Entity was procedurally modified by physics.
	ENTITY_FLAG_OUTDOORONLY             = BIT(27),  //!< Same as Brush->Outdoor only.
	ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT   = BIT(28),  //!< Entity will be sent ENTITY_EVENT_NOT_SEEN_TIMEOUT if it is not rendered for 30 seconds.
	ENTITY_FLAG_RECVWIND                = BIT(29),  //!< Receives wind.
	ENTITY_FLAG_LOCAL_PLAYER            = BIT(30),
	ENTITY_FLAG_AI_HIDEABLE             = BIT(31),  //!< AI can use the object to calculate automatic hide points.
};

enum EEntityFlagsExtended
{
	ENTITY_FLAG_EXTENDED_AUDIO_LISTENER                 = BIT(0),
	ENTITY_FLAG_EXTENDED_AUDIO_DISABLED                 = BIT(1),
	ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE               = BIT(2),
	ENTITY_FLAG_EXTENDED_CAN_COLLIDE_WITH_MERGED_MESHES = BIT(3),
	ENTITY_FLAG_EXTENDED_DYNAMIC_DISTANCE_SHADOWS       = BIT(4),
};

//! Flags can be passed to IEntity::Serialize().
enum EEntitySerializeFlags
{

	ENTITY_SERIALIZE_PROXIES    = BIT(1),    //!< Serialize proxies.
	ENTITY_SERIALIZE_POSITION   = BIT(2),    //!< Serialize properties common to all entities (position, rotation, scale).
	ENTITY_SERIALIZE_ROTATION   = BIT(3),
	ENTITY_SERIALIZE_SCALE      = BIT(4),
	ENTITY_SERIALIZE_GEOMETRIES = BIT(5),
	ENTITY_SERIALIZE_PROPERTIES = BIT(6),
};

enum EEntityGetSetSlotFlags
{
	ENTITY_SLOT_ACTUAL = 1 << 31
};

#define ENTITY_LINK_NAME_MAX_LENGTH 31

struct IEntityLink
{
	char         name[ENTITY_LINK_NAME_MAX_LENGTH + 1]; //!< Name of the link.
	EntityId     entityId;                              //!< Entity targeted by the link.
	EntityGUID   entityGuid;                            //!< Entity targeted by the link.
	IEntityLink* next;                                  //!< Pointer to the next link, or NULL if last link.
};

struct SChildAttachParams
{
	SChildAttachParams()
		: m_nAttachFlags(0)
		, m_target(NULL)
	{}

	SChildAttachParams(const int nAttachFlags)
		: m_nAttachFlags(nAttachFlags)
		, m_target(NULL)
	{}

	SChildAttachParams(const int nAttachFlags, const char* target)
		: m_nAttachFlags(nAttachFlags)
		, m_target(target)
	{}

	int         m_nAttachFlags;
	const char* m_target;
};

//! Interface to entity object.
struct IEntity
{
	enum EEntityLoadFlags
	{
		EF_AUTO_PHYSICALIZE = 0x0001,
		EF_NO_STREAMING     = 0x0002,
	};
	enum EAttachmentFlags
	{
		ATTACHMENT_KEEP_TRANSFORMATION = BIT(0),    //!< Keeps world transformation of entity when attaching or detaching it.
		ATTACHMENT_GEOMCACHENODE       = BIT(1),    //!< Attach to geom cache node.
		ATTACHMENT_CHARACTERBONE       = BIT(2),    //!< Attached to character bone.
	};

#ifdef SEG_WORLD
	enum ESWObjFlag
	{
		ESWOF_Normal   = 0x00,
		ESWOF_CrossSeg = 0x01,
		ESWOF_Global   = 0x02
	};

	virtual void SetLocalSeg(bool bL) = 0;
	virtual bool IsLocalSeg() const = 0;
#endif //SEG_WORLD

	// <interfuscator:shuffle>
	virtual ~IEntity(){}

	//! Retrieves the runtime unique identifier of this entity assigned to it by the Entity System.
	//! EntityId may not be the same when saving/loading entity.
	//! EntityId is mostly used in runtime for fast and unique identification of entities..
	//! \return The entity ID.
	virtual EntityId GetId() const = 0;

	//! Retrieves the globally unique identifier of this entity assigned to it by the Entity System.
	//! EntityGuid is guaranteed to be the same when saving/loading entity, it is also same in the editor and in the game.
	//! \return The entity globally unique identifier.
	virtual EntityGUID GetGuid() const = 0;

	//! Retrieves the entity class pointer.
	//! Entity class defines entity type, what script it will use, etc...
	//! \return Pointer to the entity class interface.
	virtual IEntityClass* GetClass() const = 0;

	//! Retrieves the entity archetype.
	//! Entity archetype contain definition for entity script properties.
	//! \return Pointer to the entity archetype interface.
	virtual IEntityArchetype* GetArchetype() const = 0;

	//! Sets entity flags, completely replaces all flags which are already set in the entity.
	//! \param flags Flag values which are defined in EEntityFlags.
	virtual void SetFlags(uint32 flags) = 0;

	//! Gets current entity flags.
	virtual uint32 GetFlags() const = 0;

	//! Adds flag/s to the current set of entity flags (logical OR).
	//! \param flagsToAdd Combination of bit flags to add.
	virtual void AddFlags(uint32 flagsToAdd) = 0;

	//! Removes flag/s from the current set of entity flags (logical AND NOT).
	//! \param flagsToClear Combination of bit flags to remove.
	virtual void ClearFlags(uint32 flagsToClear) = 0;

	//! Checks if the specified entity flag is set.
	//! \param flagsToCheck Combination of bit flags to check.
	virtual bool CheckFlags(uint32 flagsToCheck) const = 0;

	//! Sets entity flags, completely replaces all flags which are already set in the entity.
	//! \param flags - Extended flag values which are defined in EEntityFlagsExtended.
	virtual void SetFlagsExtended(uint32 flags) = 0;

	//! Gets current entity EEntityFlagsExtended flags.
	virtual uint32 GetFlagsExtended() const = 0;

	//! Checks if this entity was marked for deletion.
	//! If this function returns true, it will be deleted on next frame, and it is pointless to perform any operations on such entity.
	//! \return True if entity marked for deletion, false otherwise.
	virtual bool IsGarbage() const = 0;

	//! Changes the entity name.
	//! Entity name does not have to be unique, but for the sake of easier finding entities by name it is better to not
	//! assign the same name to different entities.
	//! \param sName New name for the entity.
	virtual void SetName(const char* sName) = 0;

	//! Gets entity name.
	//! \return Name of the entity.
	virtual const char* GetName() const = 0;

	//! Returns textual description of entity for logging.
	virtual string GetEntityTextDescription() const = 0;

	//! Serializes entity parameters to/from XML.
	virtual void SerializeXML(XmlNodeRef& entityNode, bool bLoading) = 0;

	//! \retval true if this entity was loaded from level file.
	//! \retval false for entities created dynamically.
	virtual bool IsLoadedFromLevelFile() const = 0;

	// Entity Hierarchy.

	//! Attaches the child entity to this entity.
	//! The child entity will inherit all the transformation of the parent entity.
	//! \param pChildEntity Child entity to attach.
	//! \param attachParams Attachment parameters.
	virtual void AttachChild(IEntity* pChildEntity, const SChildAttachParams& attachParams = SChildAttachParams()) = 0;

	//! Detaches all the child entities attached to this entity.
	//! \param nDetachFlags Combination of the EAttachmentFlags flags.
	virtual void DetachAll(int nDetachFlags = 0) = 0;

	//! Detach this entity from the parent entity (assumes that this entity is the child entity).
	//! \param nDetachFlags Combination of the EAttachmentFlags flags.
	virtual void DetachThis(int nDetachFlags = 0, int nWhyFlags = 0) = 0;

	//! Retrieves the number of attached child entities.
	virtual int GetChildCount() const = 0;

	//! Retrieves the attached child entity by index.
	//! \param nIndex Index of the child entity, must be 0 <= nIndex < GetChildCount().
	//! \return Pointer to the child entity interface.
	virtual IEntity* GetChild(int nIndex) const = 0;

	//! Retrieves the parent of this entity.
	//! \return Pointer to the parent entity interface, or NULL if this entity does not have a parent.
	virtual IEntity* GetParent() const = 0;

	//! Retrieves the TM of the point this entity is attached to if it has a parent.
	//! \note This can be different from GetParent()->GetWorldTM() when the attachment point is not the pivot.
	//! \return World space matrix of the parent attachment transform
	virtual Matrix34 GetParentAttachPointWorldTM() const = 0;

	//! \return true if the matrix returned by GetParentAttachPointWorldTM is valid, otherwise false.
	virtual bool IsParentAttachmentValid() const = 0;

	// Entity transformation.

	//! Sets entity transformation matrix in the world space.
	//! \param tm World space transformation matrix (Non unifrorm scale is not supported).
	virtual void SetWorldTM(const Matrix34& tm, int nWhyFlags = 0) = 0;

	//! Sets entity transformation matrix in the local entity space.
	//! \param tm Local space transformation matrix (Non unifrorm scale is not supported).
	virtual void SetLocalTM(const Matrix34& tm, int nWhyFlags = 0) = 0;

	//! Retrieves the entity transformation matrix in the world space.
	//! \return World space transformation matrix (Include transformations of all parent entities).
	virtual const Matrix34& GetWorldTM() const = 0;

	//! Retrieves the entity transformation matrix in the local entity space.
	//! \return Local space transformation matrix.
	virtual Matrix34 GetLocalTM() const = 0;

	//! Retrieves the entity axis aligned bounding box in the world space.
	//! \param bbox Output parameter for the bounding box.
	virtual void GetWorldBounds(AABB& bbox) const = 0;

	//! Retrieves the entity axis aligned bounding box in the local entity space.
	//! \param bbox Output parameter for the bounding box.
	virtual void GetLocalBounds(AABB& bbox) const = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Set the entity local space position.
	virtual void SetPos(const Vec3& vPos, int nWhyFlags = 0, bool bRecalcPhyBounds = false, bool bForce = false) = 0;

	//! Retrieve the entity local space position.
	virtual const Vec3& GetPos() const = 0;

	//! Sets the entity local space rotation quaternion.
	virtual void SetRotation(const Quat& qRotation, int nWhyFlags = 0) = 0;

	//! Retrieves the entity local space rotation quaternion.
	virtual const Quat& GetRotation() const = 0;

	//! Sets the entity local space scale.
	virtual void SetScale(const Vec3& vScale, int nWhyFlags = 0) = 0;

	//! Retrieves the entity local space scale.
	virtual const Vec3& GetScale() const = 0;

	//! Sets the entity position,rotation and scale at once.
	virtual void SetPosRotScale(const Vec3& vPos, const Quat& qRotation, const Vec3& vScale, int nWhyFlags = 0) = 0;

	//! Helper function to retrieve world space entity position.
	virtual Vec3 GetWorldPos() const = 0;

	//! Helper function to retrieve world space entity orientation angles.
	virtual Ang3 GetWorldAngles() const = 0;

	//! Helper function to retrieve world space entity orientation quaternion
	virtual Quat GetWorldRotation() const = 0;

	//! Helper function to retrieve world space forward dir
	virtual const Vec3& GetForwardDir() const = 0;

	//////////////////////////////////////////////////////////////////////////

	//! Activates entity, if entity is active it will be updated every frame.
	virtual void Activate(bool bActive) = 0;

	//! Check if the entity is active now.
	virtual bool IsActive() const = 0;

	//! Activates entity, if entity is active it will be updated every frame.
	virtual void PrePhysicsActivate(bool bActive) = 0;

	//! Check if the entity is active now.
	virtual bool IsPrePhysicsActive() = 0;

	//////////////////////////////////////////////////////////////////////////

	//! Saves or loads entity parameters to/from stream using serialization context class.
	//! \param serializer Serialization context class, provides all the information needed to properly serialize entity contents.
	//! \param nFlags Additional custom serialization flags.
	virtual void Serialize(TSerialize serializer, int nFlags = 0) = 0;

	//////////////////////////////////////////////////////////////////////////

	//! Sends event to the entity.
	//! \param event Event description (event id, parameters).
	virtual bool SendEvent(SEntityEvent& event) = 0;

	//////////////////////////////////////////////////////////////////////////

	//! Starts the entity timer.
	//! Entity timers are owned by entity, every entity can have it`s own independent timers,
	//! so TimerId must not be unique across multiple entities.
	//! When timer finishes entity system will signal to the entity *once* with an event ENTITY_EVENT_TIME.
	//! Multiple timers can be started simultaneously with different timer ids.
	//! If some timer is not yet finished and another timer with the same timerID is set, the new one
	//! will override old timer, and old timer will not send finish event.
	//! \param nTimerId Timer ID, multiple timers with different IDs are possible.
	//! \param nMilliSeconds Timer timeout time in milliseconds.
	virtual void SetTimer(int nTimerId, int nMilliSeconds) = 0;

	//! Stops already started entity timer with this id.
	//! \param nTimerId Timer ID of the timer started for this entity.
	virtual void KillTimer(int nTimerId) = 0;

	//! Hides this entity, makes it invisible and disable its physics.
	//! \param bHide If true hide the entity, is false unhides it.
	virtual void Hide(bool bHide) = 0;

	//! Checks if the entity is hidden.
	virtual bool IsHidden() const = 0;

	//! Makes the entity invisible and disable its physics.
	//! Different from hide in that the entity is still updated.
	virtual void Invisible(bool bInvisible) = 0;

	//! Checks if the entity is invisible.
	virtual bool IsInvisible() const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual IAIObject*  GetAI() = 0;
	virtual bool        HasAI() const = 0;
	virtual tAIObjectID GetAIObjectID() const = 0;
	virtual void        SetAIObjectID(tAIObjectID id) = 0;
	//////////////////////////////////////////////////////////////////////////
	virtual bool        RegisterInAISystem(const AIObjectParams& params) = 0;
	//////////////////////////////////////////////////////////////////////////

	//! Changes the entity update policy.
	//! Update policy of entity defines the automatic activation rules for the entity.
	//! For example. when a physical object awakens it will activate the entity, and when will go to sleep
	//! again will deactivate it.
	//! Or entity can become active every time that it becomes visible, and deactivated when goes out of view.
	//! There are multiple such predefined update policies exist, consider EEntityUpdatePolicy enum.
	//! \param eUpdatePolicy - Update policy, one of EEntityUpdatePolicy enums.
	virtual void SetUpdatePolicy(EEntityUpdatePolicy eUpdatePolicy) = 0;

	//! Retrieves the entity update policy.
	virtual EEntityUpdatePolicy GetUpdatePolicy() const = 0;

	// Entity Proxies Interfaces access functions.

	//! Retrieves a pointer to the specified proxy interface in the entity.
	//! \param proxy Proxy interface identifier.
	//! \return Valid pointer to the requested entity proxy object, or NULL if such proxy not exist.
	virtual IEntityProxy* GetProxy(EEntityProxy proxy) const = 0;

	//! Replaces the specified proxy interface in the entity.
	//! \param proxy Proxy interface identifier.
	//! \param pProxy Pointer to the Proxy interface.
	virtual void SetProxy(EEntityProxy proxy, IEntityProxyPtr pProxy) = 0;

	//! Creates a specified proxy in the entity.
	//! \param proxy Proxy interface identifier.
	//! \return Valid pointer to the created entity proxy object, or NULL if creation failed.
	virtual IEntityProxyPtr CreateProxy(EEntityProxy proxy) = 0;

	//! Register or unregisters a component with the entity.
	//! \param pComponent The target component.
	//! \param flags IComponent contains the relevent flags to control registration behaviour.
	virtual void RegisterComponent(IComponentPtr pComponent, const int flags) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Physics.
	//////////////////////////////////////////////////////////////////////////
	virtual void             Physicalize(SEntityPhysicalizeParams& params) = 0;
	//! \return A physical entity assigned to an entity.
	virtual IPhysicalEntity* GetPhysics() const = 0;

	virtual int              PhysicalizeSlot(int slot, SEntityPhysicalizeParams& params) = 0;
	virtual void             UnphysicalizeSlot(int slot) = 0;
	virtual void             UpdateSlotPhysics(int slot) = 0;

	virtual void             SetPhysicsState(XmlNodeRef& physicsState) = 0;

	// Custom entity material.

	//! Assigns a custom material to the entity.
	//! \param pMaterial Pointer to custom material interface.
	virtual void SetMaterial(IMaterial* pMaterial) = 0;

	//! Retrieves a custom material set to the entity.
	//! \return Pointer to custom material interface or NULL if custom material not set.
	virtual IMaterial* GetMaterial() = 0;

	// Working with Slots.

	//! Check if the slot with specified index exist.
	//! \param nIndex Index of required slot.
	virtual bool IsSlotValid(int nIndex) const = 0;

	//! Frees slot at specified index.
	virtual void FreeSlot(int nIndex) = 0;

	//! Gets number of allocated object slots in the entity.
	virtual int GetSlotCount() const = 0;

	//! Gets object slot information.
	//! \param nIndex Index of required slot.
	//! \param slotInfo Output variable where description of the slot will be written.
	virtual bool GetSlotInfo(int nIndex, SEntitySlotInfo& slotInfo) const = 0;

	//! \return World transformation matrix of the object slot.
	virtual const Matrix34& GetSlotWorldTM(int nSlot) const = 0;

	//! \return Local transformation matrix relative to host entity transformation matrix of the object slot.
	//! \param nSlot - Index of required slot.
	//! \param bRelativeToParent - flag specifying whether the local transformation matrix is relative to the parent slot or the entity.
	virtual const Matrix34& GetSlotLocalTM(int nSlot, bool bRelativeToParent) const = 0;

	//! Sets local transformation matrix of the object slot.
	virtual void SetSlotLocalTM(int nSlot, const Matrix34& localTM, int nWhyFlags = 0) = 0;

	//! Sets camera space position of the object slot.
	virtual void SetSlotCameraSpacePos(int nSlot, const Vec3& cameraSpacePos) = 0;

	//! Gets camera space position of the object slot.
	virtual void GetSlotCameraSpacePos(int nSlot, Vec3& cameraSpacePos) const = 0;

	//! Attaches child slot to the parent slot.
	//! This will form hierarchical transformation relationship between object slots.
	//! \param nParentIndex Index of the parent slot (Child slot will be attached to this one).
	//! \param nChildIndex Index of the child slot.
	virtual bool SetParentSlot(int nParentIndex, int nChildIndex) = 0;

	//! Assigns a custom material to the specified object slot.
	//! \param nSlot Index of the slot, if -1 assign this material to all existing slots.
	//! \param pMaterial Pointer to custom material interface.
	virtual void SetSlotMaterial(int nSlot, IMaterial* pMaterial) = 0;

	//! Sets the flags of the specified slot.
	//! \param nSlot Index of the slot, if -1 apply to all existing slots.
	//! \param nFlags Flags to set.
	virtual void SetSlotFlags(int nSlot, uint32 nFlags) = 0;

	//! Retrieves the flags of the specified slot.
	//! \param nSlot Index of the slot.
	//! \return Slot flags, or 0 if specified slot is not valid.
	virtual uint32 GetSlotFlags(int nSlot) const = 0;

	//! \param nSlot - Index of the slot.
	//! \return true if character is to be updated, otherwise false.
	virtual bool ShouldUpdateCharacter(int nSlot) const = 0;

	//! Fast method to get the character at the specified slot.
	//! \param nSlot Index of the slot.
	//! \return Character pointer or NULL if character with this slot does not exist.
	virtual ICharacterInstance* GetCharacter(int nSlot) = 0;

	//! Sets character instance of a slot, and creates slot if necessary.
	//! \param nSlot		Index of a slot, or -1 if a new slot need to be allocated.
	//! \param pCharacter	A pointer to character instance.
	//! \return An integer which refers to the slot index which used.
	virtual int SetCharacter(ICharacterInstance* pCharacter, int nSlot) = 0;

	//! Fast method to get the static object at the specified slot.
	//! \param nSlot Index of the slot; | with ENTITY_SLOT_ACTUAL to disable compound statobj handling.
	//! \return StatObj pointer or NULL if stat object with this slot does not exist.
	virtual IStatObj* GetStatObj(int nSlot) = 0;

	//! Fast method to get the particle em at the specified slot.
	//! \param nSlot Index of the slot.
	//! \return IParticleEmitter pointer or NULL if stat object with this slot does not exist.
	virtual IParticleEmitter* GetParticleEmitter(int nSlot) = 0;

	//! Fast method to get the geom cache render cache at the specified slot.
	//! \param nSlot Index of the slot.
	//! \return IGeomCacheRenderNode pointer or NULL if stat object with this slot does not exist.
	virtual IGeomCacheRenderNode* GetGeomCacheRenderNode(int nSlot) = 0;

	//! Moves the contents of a render slot from one entity to another, will also move any associated physics.
	//! \param targetIEnt Entity to receive the new slot info.
	//! \param nSlot Index of the slot.
	virtual void MoveSlot(IEntity* targetIEnt, int nSlot) = 0;

	//! Sets static object of a slot, creates slot if necessary
	//! \param nSlot Index of a slot, or -1 if a new slot need to be allocated; | with ENTITY_SLOT_ACTUAL to disable compound statobj handling.
	//! \param pStatObj Pointer to the new static object.
	//! \param mass New mass of the slot, negative value to keep the current.
	//! \return Integer which refer to the slot index which used
	virtual int SetStatObj(IStatObj* pStatObj, int nSlot, bool bUpdatePhysics, float mass = -1.0f) = 0;

	//! Loads static geometry to the specified slot, or to next available slot.
	//! If same object is already loaded in this slot, operation is ignored.
	//! If this slot number is occupied by different kind of object it is overwritten with static object.
	//! \param nLoadFlags See ELoadFlags
	//! \see ELoadFlags
	//! \return Slot id where the object was loaded, or -1 if loading failed.
	virtual int LoadGeometry(int nSlot, const char* sFilename, const char* sGeomName = NULL, int nLoadFlags = 0) = 0;

	//! Loads character to the specified slot, or to next available slot.
	//! If same character is already loaded in this slot, operation is ignored.
	//! If this slot number is occupied by different kind of object it is overwritten.
	//! \return Slot id where the object was loaded, or -1 if loading failed.
	virtual int LoadCharacter(int nSlot, const char* sFilename, int nLoadFlags = 0) = 0;

#if defined(USE_GEOM_CACHES)
	//! Loads geometry cache to the specified slot, or to next available slot.
	//! If same geometry cache is already loaded in this slot, operation is ignored.
	//! If this slot number is occupied by different kind of object it is overwritten.
	//! \return Slot id where the object was loaded, or -1 if loading failed.
	virtual int LoadGeomCache(int nSlot, const char* sFilename) = 0;
#endif

	//! Loads a new particle emitter to the specified slot, or to next available slot.
	//! If same character is already loaded in this slot, operation is ignored.
	//! If this slot number is occupied by different kind of object it is overwritten.
	//! \return Slot id where the particle emitter was loaded, or -1 if loading failed.
	virtual int LoadParticleEmitter(int nSlot, IParticleEffect* pEffect, SpawnParams const* params = NULL, bool bPrime = false, bool bSerialize = false) = 0;
	virtual int SetParticleEmitter(int nSlot, IParticleEmitter* pEmitter, bool bSerialize = false) = 0;

	//! Loads a light source to the specified slot, or to next available slot.
	//! \return Slot id where the light source was loaded, or -1 if loading failed.
	virtual int LoadLight(int nSlot, CDLight* pLight) = 0;

	//! Invalidates the entity's and all its children's transformation matrices!
	virtual void InvalidateTM(int nWhyFlags = 0, bool bRecalcPhyBounds = false) = 0;

	//! Easy Script table access.
	IScriptTable* GetScriptTable() const;

	//! Enable/Disable physics by flag.
	virtual void EnablePhysics(bool enable) = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Entity links.
	//////////////////////////////////////////////////////////////////////////

	//! Gets pointer to the first entity link.
	virtual IEntityLink* GetEntityLinks() = 0;
	virtual IEntityLink* AddEntityLink(const char* sLinkName, EntityId entityId, EntityGUID entityGuid = 0) = 0;
	virtual void         RemoveEntityLink(IEntityLink* pLink) = 0;
	virtual void         RemoveAllEntityLinks() = 0;
	//////////////////////////////////////////////////////////////////////////

	//! \param[out] partId Set to the child's partId.
	//! \return An attached child entity that corresponds to the physical part partId.
	virtual IEntity* UnmapAttachedChild(int& partId) = 0;

	//! \return true if entity is completely initialized.
	virtual bool IsInitialized() const = 0;

	//! Draw a debug view of this entity geometry.
	virtual void DebugDraw(const struct SGeometryDebugDrawInfo& info) = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! Increase/or decrease KeepAliveCounter
	//! Used as a refcount to prevent deletion when deferring some events
	//! (like physics collisions which can refer these entities).
	virtual void IncKeepAliveCounter() = 0;
	virtual void DecKeepAliveCounter() = 0;
	virtual void ResetKeepAliveCounter() = 0;
	virtual bool IsKeptAlive() const = 0;

	//! LiveCreate entity manipulation.
	//! Process the LiveCreate message with a variable change.
	//! This function is used as low-latency update vs updating the whole entity.
	//! \return true if handled, false otherwise
	virtual bool HandleVariableChange(const char* szVarName, const void* pVarData) = 0;

#ifdef SEG_WORLD
	//! Draw a debug view of this entity geometry
	virtual unsigned int GetSwObjDebugFlag() const = 0;
	virtual void         SetSwObjDebugFlag(unsigned int uiVal) = 0;
#endif //SEG_WORLD

	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////
// CryComponent helpers: has to be here due to circular dependancy.

template<typename TYPE>
struct DeleteWithRelease
{
	void operator()(TYPE* p) { p->Release(); }
};

template<typename TYPE>
std::shared_ptr<TYPE> ComponentCreate()
{
	std::shared_ptr<TYPE> p(new TYPE);
	return p;
}

template<typename TYPE>
std::shared_ptr<TYPE> ComponentCreate_DeleteWithRelease()
{
	std::shared_ptr<TYPE> p(new TYPE, DeleteWithRelease<TYPE>());
	return p;
}

template<typename TYPE>
std::shared_ptr<TYPE> ComponentCreateAndRegister(const IComponent::SComponentInitializer& componentInitializer, const int flags = 0)
{
	CRY_ASSERT(componentInitializer.m_pEntity);
	std::shared_ptr<TYPE> pComponent;
	pComponent = ComponentCreate<TYPE>();
	componentInitializer.m_pEntity->RegisterComponent(pComponent, flags | IComponent::EComponentFlags_Enable);
	pComponent->Initialize(componentInitializer);
	return pComponent;
}

template<typename TYPE>
std::shared_ptr<TYPE> ComponentCreateAndRegister_DeleteWithRelease(const IComponent::SComponentInitializer& componentInitializer, const int flags = 0)
{
	CRY_ASSERT(componentInitializer.m_pEntity);
	std::shared_ptr<TYPE> pComponent;
	pComponent = ComponentCreate_DeleteWithRelease<TYPE>();
	componentInitializer.m_pEntity->RegisterComponent(pComponent, flags | IComponent::EComponentFlags_Enable);
	pComponent->Initialize(componentInitializer);
	return pComponent;
}

template<typename DST, typename SRC>
DST crycomponent_cast(SRC pComponent) { return std::static_pointer_cast<typename DST::element_type>(pComponent); }

// Inline implementation.
inline IScriptTable* IEntity::GetScriptTable() const
{
	IEntityScriptProxy* pScriptProxy = (IEntityScriptProxy*)GetProxy(ENTITY_PROXY_SCRIPT);
	if (pScriptProxy)
		return pScriptProxy->GetScriptTable();
	return NULL;
}
