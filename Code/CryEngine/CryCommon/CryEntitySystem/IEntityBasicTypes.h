// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Utils/EnumFlags.h>

//! Unique identifier for each entity instance.
//! Entity identifiers are unique for the session, but cannot be guaranteed over the network, separate instances of the application and serialization / save games.
//! Note that the type cannot be changed to increase entity count, since the entity system has specialized salting of entity identifiers
using EntityId = uint32;
//! Salt used in entity id in order to facilitate re-use of entity identifiers
using EntitySalt = uint16;
//! Index of an entity in the internal array, must be exactly half the size of EntityId!
using EntityIndex = EntitySalt;

static_assert(std::numeric_limits<EntityIndex>::digits == std::numeric_limits<EntityId>::digits / 2, "Entity Index must be exactly half the size of EntityId, identifier consists of both index and salt");

//! The entity identifier '0' is always invalid, the minimum valid id is 1.
constexpr EntityId INVALID_ENTITYID = 0;

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
enum class EEvent
{
	//! Sent when the entity local or world transformation matrix change (position/rotation/scale).
	//! nParam[0] = combination of the EEntityXFormFlags.
	TransformChanged,

	//! Called when the entity is moved/scaled/rotated in the editor. Only send on mouseButtonUp (hence finished).
	//! This is useful since the TransformChanged event will be continuously fired as the user is dragging the entity around.
	TransformChangeFinishedInEditor,

	//! Sent when the specified entity timer (added via IEntity::SetTimer) expired
	//! Entity timers are processed once a frame and expired timers are notified in the order of registration
	//! The timer is automatically removed from the timer map before this event is sent.
	//! nParam[0] = TimerId
	//! nParam[1] = Initial duration in milliseconds
	//! nParam[2] = EntityId
	TimerExpired,

	//! Sent at the very end of CEntity::Init, called by IEntitySystem::InitEntity
	//! This indicates that the entity has finished initializing, and that custom logic can start
	//! Note that this only applies for components that are part of the entity before initialization occurs.
	//! Additionally, this event is also sent to unremovable entities (ENTITY_FLAG_UNREMOVABLE) to reset their state on entity system reset or deserialize from save game.
	Initialize,

	//! Sent before entity is removed.
	Remove,

	//! Sent to reset the state of the entity (used from Editor).
	//! nParam[0] is 1 if entering game mode, 0 if exiting
	Reset,

	//! Sent to parent entity after child entity have been attached.
	//! nParam[0] contains ID of child entity.
	ChildAttached,

	//! Sent to child entity after it has been attached to the parent.
	//! nParam[0] contains ID of parent entity.
	AttachedToParent,

	//! Sent to parent entity after child entity have been detached.
	//! nParam[0] contains ID of child entity.
	ChildDetached,

	//! Sent to child entity after it has been detached from the parent.
	//! nParam[0] contains ID of parent entity.
	DetachedFromParent,

	//! Sent to an entity when a new named link was added to it (normally through the Editor)
	//! \see IEntity::GetEntityLinks
	//! \see IEntity::AddEntityLink
	//! nParam[0] contains IEntityLink ptr.
	LinkAdded,

	//! Sent to an entity when a named link was removed from it (normally through the Editor)
	//! \see IEntity::RemoveEntityLink
	//! nParam[0] contains IEntityLink ptr.
	LinkRemoved,

	//! Sent after the entity was hidden
	//! \see IEntity::Hide
	Hidden,

	//! Sent after the entity was unhidden
	//! \see IEntity::Hide
	Unhidden,

	//! Sent after the layer the entity resides in was hidden
	LayerHidden,

	//! Sent after the layer the entity resides in was unhidden
	LayerUnhidden,

	//! Sent when a physics processing for the entity must be enabled/disabled.
	//! nParam[0] == 1 physics must be enabled if 0 physics must be disabled.
	PhysicsToggled,

	//! Sent when a physics in an entity changes state.
	//! nParam[0] == 1 physics entity awakes, 0 physics entity get to a sleep state.
	PhysicsStateChanged,

	//! Sent when script is broadcasting its events.
	//! nParam[0] = Pointer to the ASCIIZ string with the name of the script event.
	//! nParam[1] = Type of the event value from IEntityClass::EventValueType.
	//! nParam[2] = Pointer to the event value depending on the type.
	LuaScriptEvent,

	//! Sent when another entity enters an area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	EntityEnteredThisArea,

	//! Sent when another entity leaves an area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	EntityLeftThisArea,

	//! Sent when another entity becomes close to the area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	//! fParam[0] = Distance to our area
	EntityEnteredNearThisArea,


	//! Sent when another entity is no longer close to the area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	EntityLeftNearThisArea,

	//! Sent when another entity moves inside the area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	EntityMoveInsideThisArea,

	//! Sent when another entity moves inside the near area linked to this entity
	//! nParam[0] = EntityId of the entity in our area
	//! nParam[1] = Area identifier
	//! nParam[2] = Entity identifier assigned to the area (normally ours)
	//! fParam[0] = Fade ratio (0 to 1)
	EntityMoveNearThisArea,

	//! Sent when an entity with pef_monitor_poststep receives a poststep notification (the handler should be thread safe!)
	//! fParam[0] = time interval
	PhysicsThreadPostStep,

	//! Sent when Breakable object is broken in physics.
	PhysicalObjectBroken,

	//! Sent when a logged physical collision is processed on the Main thread. The collision has already occurred since this event is logged, but can be handled on the main thread without considering threading.
	//! nParam[0] = const EventPhysCollision *, contains collision info and can be obtained as: reinterpret_cast<const EventPhysCollision*>(event.nParam[0])
	//! nParam[1] 0 if we are the source of the collision, otherwise 1.
	//! \par Example
	//! \include CryEntitySystem/Examples/MonitorCollision.cpp
	PhysicsCollision,

	//! Sent when a physical collision is reported just as it occurred, most likely on the physics thread. This callback has to be thread-safe!
	//! nParam[0] = const EventPhysCollision *, contains collision info and can be obtained as: reinterpret_cast<const EventPhysCollision*>(event.nParam[0])
	//! nParam[1] 0 if we are the source of the collision, otherwise 1.
	PhysicsThreadCollision,

	//! Sent only if ENTITY_FLAG_SEND_RENDER_EVENT is set
	//! Called when entity is first rendered (When any of the entity render nodes are considered by 3D engine for rendering this frame)
	//! Or called when entity is not being rendered for at least several frames
	//! nParam[0] == 0 if rendeing Stops.
	//! nParam[0] == 1 if rendeing Starts.
	RenderVisibilityChanged,

	//! Called when the level loading is complete.
	LevelLoaded,

	//! Called when the level is started.
	LevelStarted,

	//! Called when the game is started (games may start multiple times).
	GameplayStarted,

	//! Called when the entity enters a script state.
	EnterLuaScriptState,

	//! Called when the entity leaves a script state.
	LeaveLuaScriptState,

	//! Called before we serialized the game from file.
	DeserializeSaveGameStart,

	//! Called after we serialized the game from file.
	DeserializeSaveGameDone,

	//! Called when the entity becomes invisible.
	//! nParam[0] = if 1 physics will ignore this event
	//! \see IEntity::Invisible
	BecomeInvisible,

	//! Called when the entity gets out of invisibility.
	//! nParam[0] = if 1 physics will ignore this event
	//! \see IEntity::Invisible
	BecomeVisible,

	//! Called when the entity material change.
	//! nParam[0] = pointer to the new IMaterial.
	OverrideMaterialChanged,

	//! Called when the entitys material layer mask changes.
	MaterialLayerChanged,

	//! Called when an animation event (placed on animations in editor) is encountered.
	//! nParam[0] = AnimEventInstance* pEventParameters.
	//! nParam[1] = ICharacterInstance* that this event occurred on
	AnimationEvent,

	//! Called from ScriptBind_Entity when script requests to set collidermode.
	//! nParam[0] = ColliderMode
	LuaScriptSetColliderMode,

	//! Called to activate some output in a flow node connected to the entity
	//! nParam[0] = Output port index
	//! nParam[1] = TFlowInputData* to send to output
	ActivateFlowNodeOutput,

	//! Called in the editor when a property of the selected entity changes. This is *not* sent when using IEntityPropertyGroup
	//! nParam[0] = IEntityComponent pointer or nullptr
	//! nParam[1] = Member id of the changed property, (@see IEntityComponent::GetClassDesc() FindMemberById(nParam[1]))
	EditorPropertyChanged,

	//! Called when a script reloading is requested and done in the editor.
	ReloadLuaScript,

	//! Called when the entity is added to the list of entities that are updated.
	//! Note that this event is deprecated, and should not be used
	SubscribedForUpdates,

	//! Called when the entity is removed from the list of entities that are updated.
	//! Note that this event is deprecated, and should not be used
	UnsubscribedFromUpdates,

	//! Called when the entity's network authority changes. Only the server is able
	//! to delegate/revoke the authority to/from the client.
	//! nParam[0] stores the new authority value.
	NetworkAuthorityChanged,

	//! Sent once to the to the client when the special player entity has spawned.
	//! \see ENTITY_FLAG_LOCAL_PLAYER
	BecomeLocalPlayer,

	//! Called when the entity should be added to the radar.
	AddToRadar,

	//! Called when the entity should be removed from the radar.
	RemoveFromRadar,

	//! Called when the entity's name is set.
	NameChanged,

	//! Called when an event related to an audio trigger occurred.
	//! REMARK:: Only sent for triggers that have their ERequestFlags set to receive Callbacks via (CallbackOnExternalOrCallingThread | DoneCallbackOnExternalThread) from the main-thread
	//! nParam[0] stores a const CryAudio::SRequestInfo* const
	AudioTriggerStarted,
	//! Remark: Will also be sent, if the trigger failed to start
	AudioTriggerEnded, 

	//! Sent when an entity slot changes, i.e. geometry was added
	//! nParam[0] stores the slot index
	SlotChanged,

	//! Sent when the physical type of an entity changed, i.e. physicalized or dephysicalized.
	PhysicalTypeChanged,

	//! Entity was just spawned on this machine as requested by the server
	NetworkReplicatedFromServer,

	//! Called when the pre-physics update is done; fParam[0] is the frame time.
	PrePhysicsUpdate,

	//! Sent when the entity is updating every frame.
	//! nParam[0] = pointer to SEntityUpdateContext structure.
	//! fParam[0] = frame time
	Update,

	//! Last entity event in list.
	Last,
};

using EntityEventMask = uint64;
constexpr EntityEventMask EventToMask(EEvent event) { return 1ULL << static_cast<EntityEventMask>(event); }

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
constexpr EEntityEvent ENTITY_EVENT_MATERIAL_LAYER = EEntityEvent::MaterialLayerChanged;
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
constexpr EEntityEvent ENTITY_EVENT_ADD_TO_RADAR = EEntityEvent::AddToRadar;
constexpr EEntityEvent ENTITY_EVENT_REMOVE_FROM_RADAR = EEntityEvent::RemoveFromRadar;
constexpr EEntityEvent ENTITY_EVENT_SET_NAME = EEntityEvent::NameChanged;
constexpr EEntityEvent ENTITY_EVENT_AUDIO_TRIGGER_STARTED = EEntityEvent::AudioTriggerStarted;
constexpr EEntityEvent ENTITY_EVENT_AUDIO_TRIGGER_ENDED = EEntityEvent::AudioTriggerEnded;
constexpr EEntityEvent ENTITY_EVENT_SLOT_CHANGED = EEntityEvent::SlotChanged;
constexpr EEntityEvent ENTITY_EVENT_PHYSICAL_TYPE_CHANGED = EEntityEvent::PhysicalTypeChanged;
constexpr EEntityEvent ENTITY_EVENT_SPAWNED_REMOTELY = EEntityEvent::NetworkReplicatedFromServer;
constexpr EEntityEvent ENTITY_EVENT_PREPHYSICSUPDATE = EEntityEvent::PrePhysicsUpdate;
constexpr EEntityEvent ENTITY_EVENT_UPDATE = EEntityEvent::Update;
constexpr EEntityEvent ENTITY_EVENT_LAST = EEntityEvent::Last;

using EntityEventMask = Cry::Entity::EntityEventMask;
constexpr EntityEventMask ENTITY_EVENT_BIT(EEntityEvent event) { return EventToMask(event); }
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
