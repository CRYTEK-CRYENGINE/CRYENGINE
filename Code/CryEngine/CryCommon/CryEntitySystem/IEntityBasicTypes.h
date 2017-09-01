// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//!< Unique identifier for each entity instance. Don't change the type!
typedef uint32 EntityId;

constexpr EntityId INVALID_ENTITYID = 0;

typedef CryGUID EntityGUID;

enum class EEntitySimulationMode
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

	//! Sent when the entity is updating every frame.
	//! nParam[0] = pointer to SEntityUpdateContext structure.
	ENTITY_EVENT_UPDATE,

	//! Called when the entity is moved/scaled/rotated in the editor. Only send on mouseButtonUp (hence finished).
	ENTITY_EVENT_XFORM_FINISHED_EDITOR,

	//! Sent when the entity timer expire.
	//! nParam[0] = TimerId, nParam[1] = milliseconds.
	ENTITY_EVENT_TIMER,

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

	//! Physical collision.
	ENTITY_EVENT_COLLISION,

	//! Sent only if ENTITY_FLAG_SEND_RENDER_EVENT is set
	//! Called when entity is first rendered (When any of the entity render nodes are considered by 3D engine for rendering this frame)
	//! Or called when entity is not being rendered for at least several frames
	//! nParam[0] == 0 if rendeing Stops.
	//! nParam[0] == 1 if rendeing Starts.
	ENTITY_EVENT_RENDER_VISIBILITY_CHANGE,

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
	//! nParam[0] = if 1 physics will ignore this event
	ENTITY_EVENT_INVISIBLE,

	//! Called when the entity gets out of invisibility.
	//! nParam[0] = if 1 physics will ignore this event
	ENTITY_EVENT_VISIBLE,

	//! Called when the entity material change.
	//! nParam[0] = pointer to the new IMaterial.
	ENTITY_EVENT_MATERIAL,

	//! Called when the entitys material layer mask changes.
	ENTITY_EVENT_MATERIAL_LAYER,

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

	//! Called when the entity should be added to the radar.
	ENTITY_EVENT_ADD_TO_RADAR,

	//! Called when the entity should be removed from the radar.
	ENTITY_EVENT_REMOVE_FROM_RADAR,

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

	//! Last entity event in list.
	ENTITY_EVENT_LAST,
};

#define ENTITY_PERFORMANCE_EXPENSIVE_EVENTS_MASK (BIT64(ENTITY_EVENT_RENDER_VISIBILITY_CHANGE) | BIT64(ENTITY_EVENT_PREPHYSICSUPDATE) | BIT64(ENTITY_EVENT_UPDATE))

//! Variant of default BIT macro to safely handle 64-bit numbers.
#define ENTITY_EVENT_BIT(x) BIT64((x))

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
