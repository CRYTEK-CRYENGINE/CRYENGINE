// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Utils/EnumFlags.h>
#include <limits>

//! Unique identifier for each entity instance.
//! Entity identifiers are unique for the session, but cannot be guaranteed over the network, separate instances of the application and serialization / save games.
//! Note that the type cannot be changed to increase entity count, since the entity system has specialized salting of entity identifiers
using EntityId = uint32;
//! Entity salt signifies the number of times an entity index has been re-used, incremented once per use (initially 0)
//! The EntitySalt can at most take up half the bits of an entity id (32 bits)
using EntitySalt = uint16;
//! Entity index signifies the position of an entity in the internal array
//! Must be the same size as EntityId as the number of bits signifying index in an id might exceed half of EntityId.
using EntityIndex = EntityId;

static_assert(std::numeric_limits<EntitySalt>::digits == std::numeric_limits<EntityId>::digits / 2, "Entity Salt must be exactly half the size of EntityId, identifier consists of both index and salt");

//! Number of bits to use for the entity index, determining how many entities can be in the scene
constexpr size_t EntityIndexBitCount = 16;
static_assert(EntityIndexBitCount < (std::numeric_limits<EntityId>::digits - 1), "Entity Index must leave at least two bits for the salt (allowing one index re-use)!");

//! Number of reserved entity ids, currently 0 and the last two identifiers
//! These are only used internally and cannot be given to a spawned entity
constexpr size_t InternalEntityIndexCount = 3;
//! Number of entities that can be stored in the entity array
constexpr size_t EntityArraySize = 1ULL << EntityIndexBitCount;
//! Maximum number of entities that can be spawned by the game, either dynamically or through exported entities in a level
constexpr size_t MaximumEntityCount = EntityArraySize - InternalEntityIndexCount;
//! Number of bits to use for the entity salt, dynamically calculated based on the EntityIndexBitCount value
constexpr size_t EntitySaltBitCount = std::numeric_limits<EntityId>::digits - EntityIndexBitCount;

//! The entity identifier '0' is always invalid, the minimum valid id is 1.
constexpr EntityId INVALID_ENTITYID = 0;

//! Entity construct to manage the salt and index contained inside an entity identifier
struct SEntityIdentifier
{
	constexpr SEntityIdentifier() = default;
	constexpr SEntityIdentifier(EntityId _id) : id(_id) {}
	constexpr SEntityIdentifier(EntitySalt salt, EntityId index) : id(static_cast<EntityId>(salt) << EntitySaltBitCount | static_cast<uint32>(index)) {}

	constexpr bool operator==(const SEntityIdentifier& rhs) const { return id == rhs.id; }
	constexpr bool operator!=(const SEntityIdentifier& rhs) const { return !(*this == rhs); }
	constexpr operator bool() const { return id != 0; }

	//! Extracts the index from an entity identifier, givng the position of the entity in the internal array
	constexpr EntityId GetIndex() const { return id & (EntityArraySize - 1U); }
	//! Extracts the salt from an entity salt handle,
	constexpr EntityIndex GetSalt() const { return id >> EntitySaltBitCount; }
	constexpr EntityId GetId() const { return id; }

	static constexpr SEntityIdentifier GetHandleFromId(EntityId id) { return SEntityIdentifier(id); }

	EntityId id = INVALID_ENTITYID;
};

typedef CryGUID EntityGUID;

enum class EEntitySimulationMode : uint8
{
	Idle,   // Not running.
	Game,   // Running in game mode.
	Editor, // Running in editor mode.
	Preview // Running in preview window.
};

#define FORWARD_DIRECTION (Vec3(0.f, 1.f, 0.f))

// (MATT) This should really live in a minimal AI include, which right now we don't have  {2009/04/08}
#ifndef INVALID_AIOBJECTID
typedef uint32 tAIObjectID;
	#define INVALID_AIOBJECTID ((tAIObjectID)(0))
#endif

//! EEntityEvent defines all events that can be sent to an entity.
enum EEntityEvent
{
	//! Sent when the entity local or world transformation matrix change (position/rotation/scale).
	//! nParam[0] = combination of the EEntityXFormFlags.
	ENTITY_EVENT_XFORM = 0,

	//! Called when the entity is moved/scaled/rotated in the editor. Only send on mouseButtonUp (hence finished).
	ENTITY_EVENT_XFORM_FINISHED_EDITOR,

	//! Sent for unremovable entities when they are respawn.
	ENTITY_EVENT_INIT,

	//! Sent before entity is removed.
	ENTITY_EVENT_DONE,

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

	//! Sent when the entity must be hidden.
	ENTITY_EVENT_LAYER_HIDE,

	//! Sent when the entity must become not hidden.
	ENTITY_EVENT_LAYER_UNHIDE,

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
	//! fParam[0] = distance
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

	//! Sent when an entity with pef_monitor_poststep receives a poststep notification (the hamdler should be thread safe!)
	//! fParam[0] = time interval
	ENTITY_EVENT_PHYS_POSTSTEP,

	//! Sent when Breakable object is broken in physics.
	ENTITY_EVENT_PHYS_BREAK,

	//! Sent when a logged physical collision is processed on the Main thread. The collision has already occurred since this event is logged, but can be handled on the main thread without considering threading.
	//! nParam[0] = const EventPhysCollision *, contains collision info and can be obtained as: reinterpret_cast<const EventPhysCollision*>(event.nParam[0])
	//! nParam[1] 0 if we are the source of the collision, otherwise 1.
	ENTITY_EVENT_COLLISION,

	//! Sent when a physical collision is reported just as it occurred, most likely on the physics thread. This callback has to be thread-safe!
	//! nParam[0] = const EventPhysCollision *, contains collision info and can be obtained as: reinterpret_cast<const EventPhysCollision*>(event.nParam[0])
	//! nParam[1] 0 if we are the source of the collision, otherwise 1.
	ENTITY_EVENT_COLLISION_IMMEDIATE,

	//! Sent only if ENTITY_FLAG_SEND_RENDER_EVENT is set
	//! Called when entity is first rendered (When any of the entity render nodes are considered by 3D engine for rendering this frame)
	//! Or called when entity is not being rendered for at least several frames
	//! nParam[0] == 0 if rendeing Stops.
	//! nParam[0] == 1 if rendeing Starts.
	ENTITY_EVENT_RENDER_VISIBILITY_CHANGE,

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
	//! nParam[0] = if 1 physics will ignore this event
	ENTITY_EVENT_VISIBLE,

	//! Called when the entity material change.
	//! nParam[0] = pointer to the new IMaterial.
	ENTITY_EVENT_MATERIAL,

	//! Called when an animation event (placed on animations in editor) is encountered.
	//! nParam[0] = AnimEventInstance* pEventParameters.
	//! nParam[1] = ICharacterInstance* that this event occurred on
	ENTITY_EVENT_ANIM_EVENT,

	//! Called from ScriptBind_Entity when script requests to set collidermode.
	//! nParam[0] = ColliderMode
	ENTITY_EVENT_SCRIPT_REQUEST_COLLIDERMODE,

	//! Called to activate some output in a flow node connected to the entity
	//! nParam[0] = Output port index
	//! nParam[1] = TFlowInputData* to send to output
	ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT,

	//! Called in the editor when a Lua property of the selected entity changes. This is *not* sent when using IEntityPropertyGroup
	ENTITY_EVENT_EDITOR_PROPERTY_CHANGED,

	//! Sent when property of the component changes
	//! nParam[0] = IEntityComponent pointer or nullptr
	//! nParam[1] = Member id of the changed property, (@see IEntityComponent::GetClassDesc() FindMemberById(nParam[1]))
	ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED = ENTITY_EVENT_EDITOR_PROPERTY_CHANGED,

	//! Called when a script reloading is requested and done in the editor.
	ENTITY_EVENT_RELOAD_SCRIPT,

	//! Called when the entity is added to the list of entities that are updated.
	ENTITY_EVENT_ACTIVATED,

	//! Called when the entity is removed from the list of entities that are updated.
	ENTITY_EVENT_DEACTIVATED,

	//! Called when the entity's network authority changes. Only the server is able
	//! to delegate/revoke the authority to/from the client.
	//! nParam[0] stores the new authority value.
	ENTITY_EVENT_SET_AUTHORITY,

	//! Sent once to the to the client when the special player entity has spawned.
	//! \see ENTITY_FLAG_LOCAL_PLAYER
	ENTITY_EVENT_NET_BECOME_LOCAL_PLAYER,

	//! Called when the entity's name is set.
	ENTITY_EVENT_SET_NAME,

	//! Called when an event related to an audio trigger occurred.
	//! REMARK:: Only sent for triggers that have their ERequestFlags set to receive Callbacks via (CallbackOnExternalOrCallingThread | DoneCallbackOnExternalThread) from the main-thread
	//! nParam[0] stores a const CryAudio::SRequestInfo* const
	ENTITY_EVENT_AUDIO_TRIGGER_STARTED,
	ENTITY_EVENT_AUDIO_TRIGGER_ENDED,   //Remark: Will also be sent, if the trigger failed to start

	//! Sent when an entity slot changes, i.e. geometry was added
	//! nParam[0] stores the slot index
	ENTITY_EVENT_SLOT_CHANGED,

	//! Sent when the physical type of an entity changed, i.e. physicalized or dephysicalized.
	ENTITY_EVENT_PHYSICAL_TYPE_CHANGED,

	//! Entity was just spawned on this machine as requested by the server
	ENTITY_EVENT_SPAWNED_REMOTELY,

	//! Not an entity event, but signifies the last event that is sent via CEntity::SendEvent
	//! Others are grouped in the entity system due to being sent by batch every frame.
	ENTITY_EVENT_LAST_NON_PERFORMANCE_CRITICAL = ENTITY_EVENT_SPAWNED_REMOTELY,

	//! Called when the pre-physics update is done; fParam[0] is the frame time.
	ENTITY_EVENT_PREPHYSICSUPDATE,

	//! Sent when the entity is updating every frame.
	//! nParam[0] = pointer to SEntityUpdateContext structure.
	//! fParam[0] = frame time
	ENTITY_EVENT_UPDATE,

	//! Sent when the entity timer expire.
	//! nParam[0] = TimerId, nParam[1] = milliseconds.
	ENTITY_EVENT_TIMER,

	//! Last entity event in list.
	ENTITY_EVENT_LAST,
};

#define ENTITY_PERFORMANCE_EXPENSIVE_EVENTS_MASK (BIT64(ENTITY_EVENT_RENDER_VISIBILITY_CHANGE) | BIT64(ENTITY_EVENT_PREPHYSICSUPDATE) | BIT64(ENTITY_EVENT_UPDATE))

//! Variant of default BIT macro to safely handle 64-bit numbers.
#define ENTITY_EVENT_BIT(x) BIT64((x))

using EntityEventMask = uint64;

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

	explicit SEntityEvent(EEntityEvent const _event)
		: event(_event)
		, vec(ZERO)
	{
		nParam[0] = nParam[1] = nParam[2] = nParam[3] = 0;
		fParam[0] = fParam[1] = fParam[2] = 0.0f;
	}

	EEntityEvent event;     //!< Any event from EEntityEvent enum.
	intptr_t     nParam[4]; //!< Event parameters.
	float        fParam[3];
	Vec3         vec;
};

enum EEntityXFormFlags
{
	ENTITY_XFORM_POS            = BIT(1),
	ENTITY_XFORM_ROT            = BIT(2),
	ENTITY_XFORM_SCL            = BIT(3),
	ENTITY_XFORM_NO_PROPOGATE   = BIT(4),
	ENTITY_XFORM_FROM_PARENT    = BIT(5), //!< When parent changes his transformation.
	ENTITY_XFORM_PHYSICS_STEP   = BIT(6),
	ENTITY_XFORM_EDITOR         = BIT(7),
	ENTITY_XFORM_TRACKVIEW      = BIT(8),
	ENTITY_XFORM_TIMEDEMO       = BIT(9),
	ENTITY_XFORM_NOT_REREGISTER = BIT(10), //!< Optimization flag, when set object will not be re-registered in 3D engine.
	ENTITY_XFORM_NO_EVENT       = BIT(11), //!< Suppresses ENTITY_EVENT_XFORM event.
	ENTITY_XFORM_IGNORE_PHYSICS = BIT(12), //!< When set physics ignore xform event handling.

	ENTITY_XFORM_EVENT_COUNT    = 13,

	ENTITY_XFORM_USER           = 0x1000000,
};

using EntityTransformationFlagsMask = CEnumFlags<EEntityXFormFlags>;
using EntityTransformationFlagsType = uint32;

//! Entity proxies that can be hosted by the entity.
enum EEntityProxy
{
	ENTITY_PROXY_AUDIO,
	ENTITY_PROXY_AREA,
	ENTITY_PROXY_BOIDS,
	ENTITY_PROXY_BOID_OBJECT,
	ENTITY_PROXY_CAMERA,
	ENTITY_PROXY_FLOWGRAPH,
	ENTITY_PROXY_SUBSTITUTION,
	ENTITY_PROXY_TRIGGER,
	ENTITY_PROXY_ROPE,
	ENTITY_PROXY_ENTITYNODE,
	ENTITY_PROXY_CLIPVOLUME,
	ENTITY_PROXY_DYNAMICRESPONSE,
	ENTITY_PROXY_SCRIPT,

	ENTITY_PROXY_USER,

	//! Always the last entry of the enum.
	ENTITY_PROXY_LAST
};

//! Flags the can be set on each of the entity object slots.
enum EEntitySlotFlags : uint16
{
	ENTITY_SLOT_RENDER                      = BIT(0), //!< Draw this slot.
	ENTITY_SLOT_RENDER_NEAREST              = BIT(1), //!< Draw this slot as nearest. [Rendered in camera space].
	ENTITY_SLOT_RENDER_WITH_CUSTOM_CAMERA   = BIT(2), //!< Draw this slot using custom camera passed as a Public ShaderParameter to the entity.
	ENTITY_SLOT_IGNORE_PHYSICS              = BIT(3), //!< This slot will ignore physics events sent to it.
	ENTITY_SLOT_BREAK_AS_ENTITY             = BIT(4),
	ENTITY_SLOT_RENDER_AFTER_POSTPROCESSING = BIT(5),
	ENTITY_SLOT_BREAK_AS_ENTITY_MP          = BIT(6), //!< In MP this an entity that shouldn't fade or participate in network breakage.
	ENTITY_SLOT_CAST_SHADOW                 = BIT(7),
	ENTITY_SLOT_IGNORE_VISAREAS             = BIT(8),
	ENTITY_SLOT_GI_MODE_BIT0                = BIT(9),
	ENTITY_SLOT_GI_MODE_BIT1                = BIT(10),
	ENTITY_SLOT_GI_MODE_BIT2                = BIT(11)
};

struct ISimpleEntityEventListener
{
	virtual void ProcessEvent(const SEntityEvent& event) = 0;

protected:
	virtual ~ISimpleEntityEventListener() {}
};
