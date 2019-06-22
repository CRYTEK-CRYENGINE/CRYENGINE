// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Utils/EnumFlags.h>
#include <limits>
#include <CryCore/CryEnumMacro.h>
#include <CryExtension/CryGUID.h>
#include <CryMath/Cry_Math.h>

//! Unique identifier for each entity instance.
//! Entity identifiers are unique for the session, but cannot be guaranteed over the network, separate instances of the application and serialization / save games.
//! Note that the type cannot be changed to increase entity count, since the entity system has specialized salting of entity identifiers
using EntityId = uint32;
//! Entity salt signifies the number of times an entity index has been re-used, incremented once per use (initially 0)
//! The EntitySalt can at most take up half the bits of an entity id (32 bits)
using EntitySalt = uint16;
static_assert(std::numeric_limits<EntitySalt>::digits == std::numeric_limits<EntityId>::digits / 2, "Entity Salt must be exactly half the size of EntityId, identifier consists of both index and salt");

//! Number of bits to use for the entity index, determining how many entities can be in the scene
constexpr size_t EntityIndexBitCount = 16;
static_assert(EntityIndexBitCount < (std::numeric_limits<EntityId>::digits - 1), "Entity Index must leave at least two bits for the salt (allowing one index re-use)!");

//! Number of reserved entity ids, currently 0 and the last two identifiers
//! These are only used internally and cannot be given to a spawned entity
constexpr size_t InternalEntityIndexCount = 3;
//! Maximum number of entities that can be represented by an entity index
//! Note that this is always higher than the maximum number of logical entities
//! See MaximumEntityCount for the technical limit
constexpr size_t MaximumEntityDigits = 1ULL << EntityIndexBitCount;
//! Maximum number of entities that can be spawned by the game, either dynamically or through exported entities in a level
//! This does not include INVALID_ENTITYID
constexpr size_t MaximumEntityCount = MaximumEntityDigits - InternalEntityIndexCount;
//! Number of entities that can be stored in the entity array
//! Note that this includes [0] being INVALID_ENTITYID!
constexpr size_t EntityArraySize = MaximumEntityCount + 1;
//! Number of bits to use for the entity salt, dynamically calculated based on the EntityIndexBitCount value
constexpr size_t EntitySaltBitCount = std::numeric_limits<EntityId>::digits - EntityIndexBitCount;

//! Entity index signifies the position of an entity in the internal array
//! Same size as EntitySalt when bit count equals the index count, otherwise EntityId is used (2x EntitySalt) as index requires more than half of the bits.
using EntityIndex = typename std::conditional<EntitySaltBitCount == EntityIndexBitCount, EntitySalt, EntityId>::type;

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
	constexpr operator bool() const { return id != INVALID_ENTITYID; }

	//! Extracts the index from an entity identifier, giving the position of the entity in the internal array
	constexpr EntityId GetIndex() const { return id & (MaximumEntityDigits - 1U); }
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

namespace Cry
{
//! Main namespace for the entity system
//! Note that moving entity types under this namespace is still early in the works!
namespace Entity
{
//! EEvent defines all events that can be sent to an entity.
enum class EEvent : uint64
{
	//! Sent when the entity local or world transformation matrix change (position/rotation/scale).
	//! nParam[0] = combination of the EEntityXFormFlags.
	TransformChanged = BIT64(0),

	//! Called when the entity is moved/scaled/rotated in the editor. Only send on mouseButtonUp (hence finished).
	//! This is useful since the TransformChanged event will be continuously fired as the user is dragging the entity around.
	TransformChangeFinishedInEditor = BIT64(1),

	//! Sent at the very end of CEntity::Init, called by IEntitySystem::InitEntity
	//! This indicates that the entity has finished initializing, and that custom logic can start
	//! Note that this only applies for components that are part of the entity before initialization occurs.
	//! Additionally, this event is also sent to unremovable entities (ENTITY_FLAG_UNREMOVABLE) to reset their state on entity system reset or deserialize from save game.
	Initialize = BIT64(2),

	//! Sent before entity is removed.
	Remove = BIT64(3),

	//! Sent to reset the state of the entity (used from Editor).
	//! nParam[0] is 1 if entering game mode, 0 if exiting
	Reset = BIT64(4),

	//! Sent to parent entity after child entity have been attached.
	//! nParam[0] contains ID of child entity.
	ChildAttached = BIT64(5),

	//! Sent to child entity after it has been attached to the parent.
	//! nParam[0] contains ID of parent entity.
	AttachedToParent = BIT64(6),

	//! Sent to parent entity after child entity have been detached.
	//! nParam[0] contains ID of child entity.
	ChildDetached = BIT64(7),

	//! Sent to child entity after it has been detached from the parent.
	//! nParam[0] contains ID of parent entity.
	DetachedFromParent = BIT64(8),

	//! Sent to an entity when a new named link was added to it (normally through the Editor)
	//! \see IEntity::GetEntityLinks
	//! \see IEntity::AddEntityLink
	//! nParam[0] contains IEntityLink ptr.
	LinkAdded = BIT64(9),

	//! Sent to an entity when a named link was removed from it (normally through the Editor)
	//! \see IEntity::RemoveEntityLink
	//! nParam[0] contains IEntityLink ptr.
	LinkRemoved = BIT64(10),

	//! Sent after the entity was hidden
	//! \see IEntity::Hide
	Hidden = BIT64(11),

	//! Sent after the entity was unhidden
	//! \see IEntity::Hide
	Unhidden = BIT64(12),

	//! Sent after the layer the entity resides in was hidden
	LayerHidden = BIT64(13),

	//! Sent after the layer the entity resides in was unhidden
	LayerUnhidden = BIT64(14),

	//! Sent when a physics processing for the entity must be enabled/disabled.
	//! nParam[0] == 1 physics must be enabled if 0 physics must be disabled.
	PhysicsToggled = BIT64(15),

	//! Sent when a physics in an entity changes state.
	//! nParam[0] == 1 physics entity awakes), 0 physics entity get to a sleep state.
	PhysicsStateChanged = BIT64(16),

	//! Sent when script is broadcasting its events.
	//! nParam[0] = Pointer to the ASCIIZ string with the name of the script event.
	//! nParam[1] = Type of the event value from IEntityClass::EventValueType.
	//! nParam[2] = Pointer to the event value depending on the type.
	LuaScriptEvent = BIT64(17),

	//! Sent when another entity enters an area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	EntityEnteredThisArea = BIT64(18),

	//! Sent when another entity leaves an area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	EntityLeftThisArea = BIT64(19),

	//! Sent when another entity becomes close to the area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	//! fParam[0] = Distance to our area
	EntityEnteredNearThisArea = BIT64(20),


	//! Sent when another entity is no longer close to the area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	EntityLeftNearThisArea = BIT64(21),

	//! Sent when another entity moves inside the area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	EntityMoveInsideThisArea = BIT64(22),

	//! Sent when another entity moves inside the near area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	//! fParam[0] = Fade ratio (0 to 1)
	EntityMoveNearThisArea = BIT64(23),

	//! Sent when an entity with pef_monitor_poststep receives a poststep notification (the handler should be thread safe!)
	//! fParam[0] = time interval
	PhysicsThreadPostStep = BIT64(24),

	//! Sent when Breakable object is broken in physics.
	PhysicalObjectBroken = BIT64(25),

	//! Sent when a logged physical collision is processed on the Main thread. The collision has already occurred since this event is logged), but can be handled on the main thread without considering threading.
	//! nParam[0] = const EventPhysCollision *), contains collision info and can be obtained as: reinterpret_cast<const EventPhysCollision*>(event.nParam[0])
	//! nParam[1] 0 if we are the source of the collision), otherwise 1.
	//! \par Example
	//! \include CryEntitySystem/Examples/MonitorCollision.cpp
	PhysicsCollision = BIT64(26),

	//! Sent when a physical collision is reported just as it occurred), most likely on the physics thread. This callback has to be thread-safe!
	//! nParam[0] = const EventPhysCollision *), contains collision info and can be obtained as: reinterpret_cast<const EventPhysCollision*>(event.nParam[0])
	//! nParam[1] 0 if we are the source of the collision), otherwise 1.
	PhysicsThreadCollision = BIT64(27),

	//! Sent only if ENTITY_FLAG_SEND_RENDER_EVENT is set
	//! Called when entity is first rendered (When any of the entity render nodes are considered by 3D engine for rendering this frame)
	//! Or called when entity is not being rendered for at least several frames
	//! nParam[0] == 0 if rendeing Stops.
	//! nParam[0] == 1 if rendeing Starts.
	RenderVisibilityChanged = BIT64(28),

	//! Called when the level loading is complete.
	LevelLoaded = BIT64(29),

	//! Called when the level is started.
	LevelStarted = BIT64(30),

	//! Called when the game is started (games may start multiple times).
	GameplayStarted = BIT64(31),

	//! Called when the entity enters a script state.
	EnterLuaScriptState = BIT64(32),

	//! Called when the entity leaves a script state.
	LeaveLuaScriptState = BIT64(33),

	//! Called before we serialized the game from file.
	DeserializeSaveGameStart = BIT64(34),

	//! Called after we serialized the game from file.
	DeserializeSaveGameDone = BIT64(35),

	//! Called when the entity becomes invisible.
	//! \see IEntity::Invisible
	BecomeInvisible = BIT64(36),

	//! Called when the entity gets out of invisibility.
	//! nParam[0] = if 1 physics will ignore this event
	//! \see IEntity::Invisible
	BecomeVisible = BIT64(37),

	//! Called when the entity material change.
	//! nParam[0] = pointer to the new IMaterial.
	OverrideMaterialChanged = BIT64(38),

	//! Called when an animation event (placed on animations in editor) is encountered.
	//! nParam[0] = AnimEventInstance* pEventParameters.
	//! nParam[1] = ICharacterInstance* that this event occurred on
	AnimationEvent = BIT64(39),

	//! Called from ScriptBind_Entity when script requests to set collidermode.
	//! nParam[0] = ColliderMode
	LuaScriptSetColliderMode = BIT64(40),

	//! Called to activate some output in a flow node connected to the entity
	//! nParam[0] = Output port index
	//! nParam[1] = TFlowInputData* to send to output
	ActivateFlowNodeOutput = BIT64(41),

	//! Called in the editor when a property of the selected entity changes. This is *not* sent when using IEntityPropertyGroup
	//! nParam[0] = IEntityComponent pointer or nullptr
	//! nParam[1] = Member id of the changed property), (@see IEntityComponent::GetClassDesc() FindMemberById(nParam[1]))
	EditorPropertyChanged = BIT64(42),

	//! Called when a script reloading is requested and done in the editor.
	ReloadLuaScript = BIT64(43),

	//! Called when the entity is added to the list of entities that are updated.
	//! Note that this event is deprecated), and should not be used
	SubscribedForUpdates = BIT64(44),

	//! Called when the entity is removed from the list of entities that are updated.
	//! Note that this event is deprecated), and should not be used
	UnsubscribedFromUpdates = BIT64(45),

	//! Called when the entity's network authority changes. Only the server is able
	//! to delegate/revoke the authority to/from the client.
	//! nParam[0] stores the new authority value.
	NetworkAuthorityChanged = BIT64(46),

	//! Sent once to the to the client when the special player entity has spawned.
	//! \see ENTITY_FLAG_LOCAL_PLAYER
	BecomeLocalPlayer = BIT64(47),

	//! Called when the entity's name is set.
	NameChanged = BIT64(48),

	//! Called when an event related to an audio trigger occurred.
	//! REMARK:: Only sent for triggers that have their ERequestFlags set to receive Callbacks via (CallbackOnExternalOrCallingThread | DoneCallbackOnExternalThread) from the main-thread
	//! nParam[0] stores a const CryAudio::SRequestInfo* const
	AudioTriggerStarted = BIT64(49),
	//! Remark: Will also be sent), if the trigger failed to start
	AudioTriggerEnded = BIT64(50),

	//! Sent when an entity slot changes), i.e. geometry was added
	//! nParam[0] stores the slot index
	SlotChanged = BIT64(51),

	//! Sent when the physical type of an entity changed), i.e. physicalized or dephysicalized.
	PhysicalTypeChanged = BIT64(52),

	//! Entity was just spawned on this machine as requested by the server
	NetworkReplicatedFromServer = BIT64(53),

	//! Not an entity event), but signifies the last event that is sent via CEntity::SendEvent
	//! Others are grouped in the entity system due to being sent by batch every frame.
	LastNonPerformanceCritical = NetworkReplicatedFromServer,

	//! Called when the pre-physics update is done; fParam[0] is the frame time.
	PrePhysicsUpdate = BIT64(54),

	//! Sent when the entity is updating every frame.
	//! nParam[0] = pointer to SEntityUpdateContext structure.
	//! fParam[0] = frame time
	Update = BIT64(55),

	//! Sent when the entity timer expire.
	//! nParam[0] = TimerId), nParam[1] = milliseconds.
	TimerExpired = BIT64(56),

	//! Last entity event in list.
	Last = BIT64(57),
	Count = 57,
};

using EventFlags = CEnumFlags<EEvent>;
CRY_CREATE_ENUM_FLAG_OPERATORS(EEvent);

namespace Deprecated_With_5_5
{
using EEntityEvent = Cry::Entity::EEvent;

constexpr EEntityEvent ENTITY_EVENT_XFORM = EEntityEvent::TransformChanged;
constexpr EEntityEvent ENTITY_EVENT_XFORM_FINISHED_EDITOR = EEntityEvent::TransformChangeFinishedInEditor;
constexpr EEntityEvent ENTITY_EVENT_TIMER = EEntityEvent::TimerExpired;
constexpr EEntityEvent ENTITY_EVENT_INIT = EEntityEvent::Initialize;
constexpr EEntityEvent ENTITY_EVENT_DONE = EEntityEvent::Remove;
constexpr EEntityEvent ENTITY_EVENT_RESET = EEntityEvent::Reset;
constexpr EEntityEvent ENTITY_EVENT_ATTACH = EEntityEvent::ChildAttached;
constexpr EEntityEvent ENTITY_EVENT_ATTACH_THIS = EEntityEvent::AttachedToParent;
constexpr EEntityEvent ENTITY_EVENT_DETACH = EEntityEvent::ChildDetached;
constexpr EEntityEvent ENTITY_EVENT_DETACH_THIS = EEntityEvent::DetachedFromParent;
constexpr EEntityEvent ENTITY_EVENT_LINK = EEntityEvent::LinkAdded;
constexpr EEntityEvent ENTITY_EVENT_DELINK = EEntityEvent::LinkRemoved;
constexpr EEntityEvent ENTITY_EVENT_HIDE = EEntityEvent::Hidden;
constexpr EEntityEvent ENTITY_EVENT_UNHIDE = EEntityEvent::Unhidden;
constexpr EEntityEvent ENTITY_EVENT_LAYER_HIDE = EEntityEvent::LayerHidden;
constexpr EEntityEvent ENTITY_EVENT_LAYER_UNHIDE = EEntityEvent::LayerUnhidden;
constexpr EEntityEvent ENTITY_EVENT_ENABLE_PHYSICS = EEntityEvent::PhysicsToggled;
constexpr EEntityEvent ENTITY_EVENT_PHYSICS_CHANGE_STATE = EEntityEvent::PhysicsStateChanged;
constexpr EEntityEvent ENTITY_EVENT_SCRIPT_EVENT = EEntityEvent::LuaScriptEvent;
constexpr EEntityEvent ENTITY_EVENT_ENTERAREA = EEntityEvent::EntityEnteredThisArea;
constexpr EEntityEvent ENTITY_EVENT_LEAVEAREA = EEntityEvent::EntityLeftThisArea;
constexpr EEntityEvent ENTITY_EVENT_ENTERNEARAREA = EEntityEvent::EntityEnteredNearThisArea;
constexpr EEntityEvent ENTITY_EVENT_LEAVENEARAREA = EEntityEvent::EntityLeftNearThisArea;
constexpr EEntityEvent ENTITY_EVENT_MOVEINSIDEAREA = EEntityEvent::EntityMoveInsideThisArea;
constexpr EEntityEvent ENTITY_EVENT_MOVENEARAREA = EEntityEvent::EntityMoveNearThisArea;
constexpr EEntityEvent ENTITY_EVENT_PHYS_POSTSTEP = EEntityEvent::PhysicsThreadPostStep;
constexpr EEntityEvent ENTITY_EVENT_PHYS_BREAK = EEntityEvent::PhysicalObjectBroken;
constexpr EEntityEvent ENTITY_EVENT_COLLISION = EEntityEvent::PhysicsCollision;
constexpr EEntityEvent ENTITY_EVENT_COLLISION_IMMEDIATE = EEntityEvent::PhysicsThreadCollision;
constexpr EEntityEvent ENTITY_EVENT_RENDER_VISIBILITY_CHANGE = EEntityEvent::RenderVisibilityChanged;
constexpr EEntityEvent ENTITY_EVENT_LEVEL_LOADED = EEntityEvent::LevelLoaded;
constexpr EEntityEvent ENTITY_EVENT_START_LEVEL = EEntityEvent::LevelStarted;
constexpr EEntityEvent ENTITY_EVENT_START_GAME = EEntityEvent::GameplayStarted;
constexpr EEntityEvent ENTITY_EVENT_ENTER_SCRIPT_STATE = EEntityEvent::EnterLuaScriptState;
constexpr EEntityEvent ENTITY_EVENT_LEAVE_SCRIPT_STATE = EEntityEvent::LeaveLuaScriptState;
constexpr EEntityEvent ENTITY_EVENT_PRE_SERIALIZE = EEntityEvent::DeserializeSaveGameStart;
constexpr EEntityEvent ENTITY_EVENT_POST_SERIALIZE = EEntityEvent::DeserializeSaveGameDone;
constexpr EEntityEvent ENTITY_EVENT_INVISIBLE = EEntityEvent::BecomeInvisible;
constexpr EEntityEvent ENTITY_EVENT_VISIBLE = EEntityEvent::BecomeVisible;
constexpr EEntityEvent ENTITY_EVENT_MATERIAL = EEntityEvent::OverrideMaterialChanged;
constexpr EEntityEvent ENTITY_EVENT_ANIM_EVENT = EEntityEvent::AnimationEvent;
constexpr EEntityEvent ENTITY_EVENT_SCRIPT_REQUEST_COLLIDERMODE = EEntityEvent::LuaScriptSetColliderMode;
constexpr EEntityEvent ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT = EEntityEvent::ActivateFlowNodeOutput;
constexpr EEntityEvent ENTITY_EVENT_EDITOR_PROPERTY_CHANGED = EEntityEvent::EditorPropertyChanged;
constexpr EEntityEvent ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED = EEntityEvent::EditorPropertyChanged;
constexpr EEntityEvent ENTITY_EVENT_RELOAD_SCRIPT = EEntityEvent::ReloadLuaScript;
constexpr EEntityEvent ENTITY_EVENT_ACTIVATED = EEntityEvent::SubscribedForUpdates;
constexpr EEntityEvent ENTITY_EVENT_DEACTIVATED = EEntityEvent::UnsubscribedFromUpdates;
constexpr EEntityEvent ENTITY_EVENT_SET_AUTHORITY = EEntityEvent::NetworkAuthorityChanged;
constexpr EEntityEvent ENTITY_EVENT_NET_BECOME_LOCAL_PLAYER = EEntityEvent::BecomeLocalPlayer;
constexpr EEntityEvent ENTITY_EVENT_SET_NAME = EEntityEvent::NameChanged;
constexpr EEntityEvent ENTITY_EVENT_AUDIO_TRIGGER_STARTED = EEntityEvent::AudioTriggerStarted;
constexpr EEntityEvent ENTITY_EVENT_AUDIO_TRIGGER_ENDED = EEntityEvent::AudioTriggerEnded;
constexpr EEntityEvent ENTITY_EVENT_SLOT_CHANGED = EEntityEvent::SlotChanged;
constexpr EEntityEvent ENTITY_EVENT_PHYSICAL_TYPE_CHANGED = EEntityEvent::PhysicalTypeChanged;
constexpr EEntityEvent ENTITY_EVENT_SPAWNED_REMOTELY = EEntityEvent::NetworkReplicatedFromServer;
constexpr EEntityEvent ENTITY_EVENT_PREPHYSICSUPDATE = EEntityEvent::PrePhysicsUpdate;
constexpr EEntityEvent ENTITY_EVENT_UPDATE = EEntityEvent::Update;
constexpr EEntityEvent ENTITY_EVENT_LAST = EEntityEvent::Last;

using EntityEventMask = Cry::Entity::EventFlags;
constexpr EEntityEvent ENTITY_EVENT_BIT(EEntityEvent event) { return event; }
}
}
}

using namespace Cry::Entity::Deprecated_With_5_5;

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

enum EEntityXFormFlags : uint32
{
	ENTITY_XFORM_POS            = BIT32(1),
	ENTITY_XFORM_ROT            = BIT32(2),
	ENTITY_XFORM_SCL            = BIT32(3),
	ENTITY_XFORM_NO_PROPOGATE   = BIT32(4),
	ENTITY_XFORM_FROM_PARENT    = BIT32(5), //!< When parent changes his transformation.
	ENTITY_XFORM_PHYSICS_STEP   = BIT32(6),
	ENTITY_XFORM_EDITOR         = BIT32(7),
	ENTITY_XFORM_TRACKVIEW      = BIT32(8),
	ENTITY_XFORM_TIMEDEMO       = BIT32(9),
	ENTITY_XFORM_NOT_REREGISTER = BIT32(10), //!< Optimization flag, when set object will not be re-registered in 3D engine.
	ENTITY_XFORM_NO_EVENT       = BIT32(11), //!< Suppresses ENTITY_EVENT_XFORM event.
	ENTITY_XFORM_IGNORE_PHYSICS = BIT32(12), //!< When set physics ignore xform event handling.

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
	ENTITY_SLOT_RENDER                      = BIT16(0), //!< Draw this slot.
	ENTITY_SLOT_RENDER_NEAREST              = BIT16(1), //!< Draw this slot as nearest. [Rendered in camera space].
	ENTITY_SLOT_RENDER_WITH_CUSTOM_CAMERA   = BIT16(2), //!< Draw this slot using custom camera passed as a Public ShaderParameter to the entity.
	ENTITY_SLOT_IGNORE_PHYSICS              = BIT16(3), //!< This slot will ignore physics events sent to it.
	ENTITY_SLOT_BREAK_AS_ENTITY             = BIT16(4),
	ENTITY_SLOT_RENDER_AFTER_POSTPROCESSING = BIT16(5),
	ENTITY_SLOT_BREAK_AS_ENTITY_MP          = BIT16(6), //!< In MP this an entity that shouldn't fade or participate in network breakage.
	ENTITY_SLOT_CAST_SHADOW                 = BIT16(7),
	ENTITY_SLOT_IGNORE_VISAREAS             = BIT16(8),
	ENTITY_SLOT_GI_MODE_BIT0                = BIT16(9),
	ENTITY_SLOT_GI_MODE_BIT1                = BIT16(10),
	ENTITY_SLOT_GI_MODE_BIT2                = BIT16(11),
	ENTITY_SLOT_ALLOW_TERRAIN_LAYER_BLEND   = BIT16(12),
	ENTITY_SLOT_ALLOW_DECAL_BLEND           = BIT16(13),
	ENTITY_SLOT_CUSTOM_VIEWDIST_RATIO       = BIT16(14), //!< This slot will use a custom view dist ratio and ignores the view dist ratio set for the entity.
};

struct ISimpleEntityEventListener
{
	virtual void ProcessEvent(const SEntityEvent& event) = 0;

protected:
	virtual ~ISimpleEntityEventListener() {}
};
