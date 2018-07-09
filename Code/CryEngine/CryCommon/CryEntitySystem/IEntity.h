// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityBasicTypes.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <type_traits>

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
struct AIObjectParams;
struct IParticleEffect;
struct SpawnParams;
struct IGeomCacheRenderNode;
struct ICharacterInstance;
struct IParticleEmitter;
struct IStatObj;
struct INetEntity;
struct SGeometryDebugDrawInfo;
struct SFogVolumeProperties;
struct SRenderLight;

//////////////////////////////////////////////////////////////////////////
struct IGameObject;
struct IAIObject;
struct IMaterial;
//////////////////////////////////////////////////////////////////////////

namespace Schematyc
{
// Forward declarations.
struct IObject;
struct IEnvRegistrar;
}

//////////////////////////////////////////////////////////////////////////

#include <CryNetwork/SerializeFwd.h>
//////////////////////////////////////////////////////////////////////////

//! Parameters used when attaching one entity to another, creating a parent-child hierarchy
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

	//! See IEntity::EAttachmentFlags
	int         m_nAttachFlags;
	const char* m_target;
};

#if (eCryModule != eCryM_LegacyGameDLL && eCryModule != eCryM_ScriptSystem && eCryModule != eCryM_AISystem && eCryModule != eCryM_FlowGraph \
  && eCryModule != eCryM_GameFramework && eCryModule != eCryM_Legacy && eCryModule != eCryM_Editor && eCryModule != eCryM_EntitySystem \
  && eCryModule != eCryM_MonoBridge)
	#define CRY_DEPRECATED_ENTTIY_ARCHETYPE CRY_DEPRECATED("(v5.4) Entity archetypes are deprecated in favour of Schematyc usage")
#else
	#define CRY_DEPRECATED_ENTTIY_ARCHETYPE
#endif

#if (eCryModule != eCryM_LegacyGameDLL && eCryModule != eCryM_System && eCryModule != eCryM_ScriptSystem && eCryModule != eCryM_Movie \
  && eCryModule != eCryM_FlowGraph && eCryModule != eCryM_GameFramework && eCryModule != eCryM_Legacy && eCryModule != eCryM_Editor   \
  && eCryModule != eCryM_EntitySystem && eCryModule != eCryM_AISystem && eCryModule != eCryM_MonoBridge)
	#define CRY_DEPRECATED_ENTTIY_LUA CRY_DEPRECATED("(v5.5) Lua has been deprecated in favor of C# and Schematyc")
#else
	#define CRY_DEPRECATED_ENTTIY_LUA
#endif

//! Structure used when spawning new entities to define their initial state
struct SEntitySpawnParams
{
	SEntitySpawnParams() = default;

	//! Optional: Initial position of the new entity instance in world space if pParent is not specified, otherwise local
	Vec3 vPosition = ZERO;
	//! Optional: Initial rotation of the new entity instance in world space if pParent is not specified, otherwise local
	Quat qRotation = IDENTITY;
	//! Optional: Initial scale of the new entity instance in world space if pParent is not specified, otherwise local
	Vec3 vScale = Vec3(1.f);

	//! Optional: Name of the new entity instance
	//! Does *not* have to be unique!
	const char* sName = "";          //!< \note The name of the entity does not need to be unique.
	//! Optional: Default entity flags to assign to the new instance
	//! See EEntityFlags
	uint32 nFlags = 0;
	//! Optional: Default extended entity flags to assign to the new instance
	//! See EEntityFlagsExtended
	uint32 nFlagsExtended = 0;

	//! Optional: Specify the parent of the new entity instance, ensuring that the new instance's coordinates are in relation to its parent
	//! Note that setting this will indicate that vPosition, qRotation and vScale should be provided in local space
	IEntity*           pParent = nullptr;
	//! Used when pParent is specified, same behavior as pParent->AttachChild(this, attachmentParams).
	SChildAttachParams attachmentParams;

	//! Optional: Entity class to use, if null we use the default class
	IEntityClass* pClass = nullptr;

	// Optional: Unique entity identifier to use for the new instance, generates automatically if not provided
	EntityId id = INVALID_ENTITYID;
	//! Optional: Unique entity identifier used for the previous instance if we are reloading
	EntityId prevId = INVALID_ENTITYID;
	//! Optional: Used when id is set to automatically generate (id = INVALID_ENTITYID) to pick identifiers from the static pool instead of being treated as a dynamically spawned entity
	//! This is useful for supporting save games compatible with patched levels
	bool bStaticEntityId = false;

	//! Optional: Globally unique identifier for this entity instance, this is guaranteed to be unique across sessions and save games assuming that the entity is saved by the entity system
	//! If one is not provided, it will be generated by the entity system
	EntityGUID guid;
	//! Optional: Globally unique identifier used for the previous instance if we are reloading
	EntityGUID prevGuid;

	//! Optional: The name of the layer the entity resides in, used in the Editor
	const char* sLayerName = "";

	//! Optional: Reference to entity's xml data, used when loading entities from disk
	XmlNodeRef entityNode;

	//! Optional: Whether the spawning of this entity should ignore whether or not IEntitySystem::LockSpawning was called
	//! If not provided, the spawning will fail if a lock was put in place
	bool bIgnoreLock = false;

	//! Optional: User data that can be read from IEntitySystemSink to interact with the spawning of entities
	void* pUserData = nullptr;

	//! Set by networking when an entity is being spawned on a client as requested by the server
	//! This can be used by custom entity classes to deserialize data specified by the host via IEntity::GetSerializableNetworkSpawnInfo.
	//! Used internally to automatically call IEntityComponent::NetReplicateSerialize.
	TSerialize* pSpawnSerializer = nullptr;

	/*CRY_DEPRECATED_ENTTIY_LUA*/ IScriptTable*           pPropertiesTable = nullptr;
	/*CRY_DEPRECATED_ENTTIY_LUA*/ IScriptTable*           pPropertiesInstanceTable = nullptr;
	/*CRY_DEPRECATED_ENTTIY_ARCHETYPE*/ IEntityArchetype* pArchetype = nullptr;
};

//! Update context of an entity, passed along to components with useful data such as frame time and frame identifier
struct SEntityUpdateContext
{
	//! Delta time for this frame
	float  fFrameTime;
	//! Current time at the start of the entity update - see ITimer::GetCurTime
	float  fCurrTime;
	//! The current frame identifier, see gEnv->nMainFrameID
	uint32 frameID;
};

enum EEntityHideFlags
{
	ENTITY_HIDE_NO_FLAG = 0,

	ENTITY_HIDE_LAYER   = BIT(0),
	ENTITY_HIDE_PARENT  = BIT(1),
};

//! Each entity instance holds flags that indicate special behavior within the system
//! These can be set with IEntity::SetFlags, and retrieved with IEntity::GetFlags
enum EEntityFlags : uint32
{
	//////////////////////////////////////////////////////////////////////////
	// Persistent flags (can be set from the editor).
	ENTITY_FLAG_CASTSHADOW          = BIT(0),
	ENTITY_FLAG_UNREMOVABLE         = BIT(1), //!< This entity cannot be removed using IEntitySystem::RemoveEntity until this flag is cleared.
	ENTITY_FLAG_GOOD_OCCLUDER       = BIT(2),
	ENTITY_FLAG_NO_DECALNODE_DECALS = BIT(3),
	//////////////////////////////////////////////////////////////////////////

	ENTITY_FLAG_WRITE_ONLY              = BIT(4),
	ENTITY_FLAG_NOT_REGISTER_IN_SECTORS = BIT(5),
	ENTITY_FLAG_CALC_PHYSICS            = BIT(6),
	//! Entity should only be present on the client, and not server
	ENTITY_FLAG_CLIENT_ONLY             = BIT(7),
	//! Entity should only be present on the server, and not clients
	ENTITY_FLAG_SERVER_ONLY             = BIT(8),
	ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO   = BIT(9), //!< This entity have special custom view distance ratio (AI/Vehicles must have it).
	ENTITY_FLAG_CALCBBOX_USEALL         = BIT(10), //!< use character and objects in BBOx calculations.
	ENTITY_FLAG_VOLUME_SOUND            = BIT(11), //!< Entity is a volume sound (will get moved around by the sound proxy).
	ENTITY_FLAG_HAS_AI                  = BIT(12), //!< Entity has an AI object.
	ENTITY_FLAG_TRIGGER_AREAS           = BIT(13), //!< This entity will trigger areas when it enters them.
	ENTITY_FLAG_NO_SAVE                 = BIT(14), //!< This entity will not be saved.
	ENTITY_FLAG_CAMERA_SOURCE           = BIT(15), //!< This entity is a camera source.
	ENTITY_FLAG_CLIENTSIDE_STATE        = BIT(16), //!< Prevents error when state changes on the client and does not sync state changes to the client.
	ENTITY_FLAG_SEND_RENDER_EVENT       = BIT(17), //!< When set entity will send ENTITY_EVENT_RENDER_VISIBILITY_CHANGE when starts or stop actual rendering.
	ENTITY_FLAG_NO_PROXIMITY            = BIT(18), //!< Entity will not be registered in the partition grid and can not be found by proximity queries.
	ENTITY_FLAG_PROCEDURAL              = BIT(19), //!< Entity has been generated at runtime.
	ENTITY_FLAG_UPDATE_HIDDEN           = BIT(20), //!< Whether update of game logic should be skipped when the entity is hidden. This does *not* disable update of components unless they specifically request it.
	ENTITY_FLAG_NEVER_NETWORK_STATIC    = BIT(21), //!< Entity should never be considered a static entity by the network system.
	ENTITY_FLAG_IGNORE_PHYSICS_UPDATE   = BIT(22), //!< Used by Editor only, (don't set).
	ENTITY_FLAG_SPAWNED                 = BIT(23), //!< Entity was spawned dynamically without a class.
	ENTITY_FLAG_SLOTS_CHANGED           = BIT(24), //!< Entity's slots were changed dynamically.
	ENTITY_FLAG_MODIFIED_BY_PHYSICS     = BIT(25), //!< Entity was procedurally modified by physics.
	ENTITY_FLAG_OUTDOORONLY             = BIT(26), //!< Same as Brush->Outdoor only.

	ENTITY_FLAG_RECVWIND                = BIT(27), //!< Receives wind.
	ENTITY_FLAG_LOCAL_PLAYER            = BIT(28),
	ENTITY_FLAG_AI_HIDEABLE             = BIT(29), //!< AI can use the object to calculate automatic hide points.

	ENTITY_FLAG_PREALLOCATED_COMPONENTS = BIT(30)   //! Components known at load time were allocated in one contiguous chunk of memory along with the entity
};

enum EEntityFlagsExtended : uint16
{
	ENTITY_FLAG_EXTENDED_AUDIO_LISTENER                 = BIT(0),
	ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE               = BIT(1),
	ENTITY_FLAG_EXTENDED_CAN_COLLIDE_WITH_MERGED_MESHES = BIT(2),
	ENTITY_FLAG_EXTENDED_DYNAMIC_DISTANCE_SHADOWS       = BIT(3),
	ENTITY_FLAG_EXTENDED_GI_MODE_BIT0                   = BIT(4), // Bit0 of entity GI mode, see IRenderNode::EGIMode
	ENTITY_FLAG_EXTENDED_GI_MODE_BIT1                   = BIT(5), // Bit1 of entity GI mode, see IRenderNode::EGIMode
	ENTITY_FLAG_EXTENDED_GI_MODE_BIT2                   = BIT(6), // Bit2 of entity GI mode, see IRenderNode::EGIMode
	ENTITY_FLAG_EXTENDED_PREVIEW                        = BIT(7), //!< Entity is spawn for the previewing
	ENTITY_FLAG_EXTENDED_CLONED                         = BIT(8), //!< Entity was cloned from another
	ENTITY_FLAG_EXTENDED_IGNORED_IN_NAVMESH_GENERATION  = BIT(9), //!< Entity's geometry doesn't contribute to NavMesh generation
};

#define ENTITY_FLAG_EXTENDED_GI_MODE_BIT_OFFSET 4                                                                                                           // Bit offset of entity GI mode in EEntityFlagsExtended. Must be equal to the bit id of ENTITY_FLAG_EXTENDED_GI_MODE_BIT0
#define ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK   (ENTITY_FLAG_EXTENDED_GI_MODE_BIT0 | ENTITY_FLAG_EXTENDED_GI_MODE_BIT1 | ENTITY_FLAG_EXTENDED_GI_MODE_BIT2) // Bit mask of entity GI mode.

//! Flags can be passed to IEntity::Serialize().
enum EEntitySerializeFlags
{
	ENTITY_SERIALIZE_PROXIES    = BIT(1), //!< Serialize proxies.
	ENTITY_SERIALIZE_POSITION   = BIT(2), //!< Serialize properties common to all entities (position, rotation, scale).
	ENTITY_SERIALIZE_ROTATION   = BIT(3),
	ENTITY_SERIALIZE_SCALE      = BIT(4),
	ENTITY_SERIALIZE_GEOMETRIES = BIT(5),
	ENTITY_SERIALIZE_PROPERTIES = BIT(6),
};

enum EEntityGetSetSlotFlags
{
	ENTITY_SLOT_ACTUAL = 1 << 31
};

//! Representation of a link connecting one entity to another
//! Links can be added by designers in the Editor to indicate specific behavior between two entities
//! Each link can be given a name, allowing code to identify a certain type of connection.
struct IEntityLink
{
	string       name;
	EntityId     entityId;                              //!< Entity targeted by the link.
	EntityGUID   entityGuid;                            //!< Entity targeted by the link.
	IEntityLink* next;                                  //!< Pointer to the next link, or NULL if last link.
};

//! Parameters used to determine how an entity should be physicalized when IEntity::Physicalize is called
//! \see IEntity::Physicalize
struct SEntityPhysicalizeParams
{
	//////////////////////////////////////////////////////////////////////////
	SEntityPhysicalizeParams() : type(0), density(-1), mass(-1), nSlot(-1), nFlagsOR(0), nFlagsAND(UINT_MAX),
		pAttachToEntity(NULL), nAttachToPart(-1), fStiffnessScale(0), bCopyJointVelocities(false),
		pParticle(NULL), pBuoyancy(NULL), pPlayerDimensions(NULL), pPlayerDynamics(NULL), pCar(NULL), pAreaDef(NULL), nLod(0), szPropsOverride(0) {};
	//////////////////////////////////////////////////////////////////////////

	//! Physicalization type must be one of pe_type enums.
	int type;

	//! Index of object slot. -1 if all slots should be used.
	int nSlot;

	//! Only one either density or mass must be set, parameter set to a negative value will be ignored.
	float density;
	float mass;

	//! Optional physical flag.
	int nFlagsAND;

	//! Optional physical flag.
	int nFlagsOR;

	//! When physicalizing geometry can specify to use physics from different LOD.
	//! Used for characters that have ragdoll physics in Lod1.
	int nLod;

	//! Physical entity to attach this physics object (Only for Soft physical entity).
	IPhysicalEntity* pAttachToEntity;

	//! Part ID in entity to attach to (Only for Soft physical entity).
	int nAttachToPart;

	//! Used for character physicalization (Scale of force in character joint's springs).
	float fStiffnessScale;

	//! Copy joints velocities when converting a character to ragdoll.
	bool                         bCopyJointVelocities;

	struct pe_params_particle*   pParticle;
	struct pe_params_buoyancy*   pBuoyancy;
	struct pe_player_dimensions* pPlayerDimensions;
	struct pe_player_dynamics*   pPlayerDynamics;
	struct pe_params_car*        pCar;

	//! This parameters are only used when type == PE_AREA.
	struct AreaDefinition
	{
		enum EAreaType
		{
			AREA_SPHERE,            //!< Physical area will be sphere.
			AREA_BOX,               //!< Physical area will be box.
			AREA_GEOMETRY,          //!< Physical area will use geometry from the specified slot.
			AREA_SHAPE,             //!< Physical area will points to specify 2D shape.
			AREA_CYLINDER,          //!< Physical area will be a cylinder.
			AREA_SPLINE,            //!< Physical area will be a spline-tube.
		};

		EAreaType areaType;
		float     fRadius;        //!< Must be set when using AREA_SPHERE or AREA_CYLINDER area type or an AREA_SPLINE.
		Vec3      boxmin, boxmax; //!< Min,Max of bounding box, must be set when using AREA_BOX area type.
		Vec3*     pPoints;        //!< Must be set when using AREA_SHAPE area type or an AREA_SPLINE.
		int       nNumPoints;     //!< Number of points in pPoints array.
		float     zmin, zmax;     //!< Min/Max of points.
		Vec3      center;
		Vec3      axis;

		//! pGravityParams must be a valid pointer to the area gravity params structure.
		struct pe_params_area* pGravityParams;

		AreaDefinition() : areaType(AREA_SPHERE), fRadius(0), boxmin(0, 0, 0), boxmax(0, 0, 0),
			pPoints(NULL), nNumPoints(0), pGravityParams(NULL), zmin(0), zmax(0), center(0, 0, 0), axis(0, 0, 0) {}
	};

	//! When physicalizing with type == PE_AREA this must be a valid pointer to the AreaDefinition structure.
	AreaDefinition* pAreaDef;

	//! An optional string with text properties overrides for CGF nodes.
	const char* szPropsOverride;
};

//! Description of the contents of the entity slot.
struct SEntitySlotInfo
{
	//! Slot flags.
	int nFlags;

	//! Index of parent slot, (-1 if no parent).
	int nParentSlot;

	//! Hide mask used by breakable object to indicate what index of the CStatObj sub-object is hidden.
	hidemask nSubObjHideMask;

	//! Slot local transformation matrix.
	const Matrix34* pLocalTM;

	//! Slot world transformation matrix.
	const Matrix34* pWorldTM;

	// Objects that can bound to the slot.
	EntityId                     entityId;
	struct IStatObj*             pStatObj;
	struct ICharacterInstance*   pCharacter;
	struct IParticleEmitter*     pParticleEmitter;
	struct ILightSource*         pLight;
	struct IRenderNode*          pChildRenderNode;
#if defined(USE_GEOM_CACHES)
	struct IGeomCacheRenderNode* pGeomCacheRenderNode;
#endif
	//! Custom Material used for the slot.
	IMaterial* pMaterial;
};

//! Parameters passed to the preview render function
//! \see IEntity::PreviewRender
struct SEntityPreviewContext
{
	SEntityPreviewContext(SGeometryDebugDrawInfo& debugDrawInfo_) : debugDrawInfo(debugDrawInfo_) {}

	//! Defines if the entity has any render nodes
	bool                             bNoRenderNodes = false;
	//! Is the entity currently selected in the sandbox
	bool                             bSelected = false;
	//! Defines if the render slots should get previewed too
	bool                             bRenderSlots = true;

	//! \see I3DEngine::SGeometryDebugDrawInfo
	SGeometryDebugDrawInfo&          debugDrawInfo;
	//! \see IRenderer::SRendParams
	const struct SRendParams*        pRenderParams = nullptr;
	//! \see IRenderer::SRenderingPassInfo
	const struct SRenderingPassInfo* pPassInfo = nullptr;
};

struct IEntityEventListener
{
	// <interfuscator:shuffle>
	virtual ~IEntityEventListener() = default;
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) = 0;
	// </interfuscator:shuffle>
};

//! Entities represent dynamic objects that can manipulate themselves or the scene they reside in during runtime.
//! Examples include light sources, physicalized objects (such as bullets) and players.
//! Entities themselves can not contain custom logic, instead they can contain Components (see IEntityComponent).
struct IEntity
{
#ifndef SWIG
	//! This is a GUID of the Schematyc environment scope where all entity components must be registered under to be found in UI.
	static CryGUID GetEntityScopeGUID()
	{
		static CryGUID guid = "be845278-0dd2-409f-b8be-97895607c256"_cry_guid;
		return guid;
	}
#endif

	enum EEntityLoadFlags
	{
		EF_AUTO_PHYSICALIZE = 0x0001,
		EF_NO_STREAMING     = 0x0002,
	};
	enum EAttachmentFlags
	{
		ATTACHMENT_KEEP_TRANSFORMATION = BIT(0), //!< Keeps world transformation of entity when attaching or detaching it.
		ATTACHMENT_GEOMCACHENODE       = BIT(1), //!< Attach to geom cache node.
		ATTACHMENT_CHARACTERBONE       = BIT(2), //!< Attached to character bone.
		ATTACHMENT_LOCAL_SIM           = BIT(3), //!< Simulated inside the parent by the physics
		ATTACHMENT_SUPPRESS_UPDATE     = BIT(4), //!< Suppresses attachment event and matrix update

		ATTACHMENT_COUNT               = 5,
	};

	using AttachmentFlagsMask = CEnumFlags<EAttachmentFlags>;
	using AttachmentFlagsType = uint32;

	static constexpr int CREATE_NEW_UNIQUE_TIMER_ID = -666;
	static constexpr int KILL_ALL_TIMER = -1;

	struct SRenderNodeParams
	{
		uint8             lodRatio = 100;                  //!< LOD ratio
		uint8             viewDistRatio = 100;             //!< View Distance Ratio
		ESystemConfigSpec minSpec = END_CONFIG_SPEC_ENUM;  //!< Minimal spec where this node is rendered
		float             opacity = 1.0f;                  //!< Opacity of the render node.
		uint32            additionalRenderNodeFlags = 0;   //!< Additional flags to add on the render node (see ERenderNodeFlags)

		// Helper methods.
		void SetRndFlags(uint64 flags, bool enable) { if (enable) additionalRenderNodeFlags |= flags; else additionalRenderNodeFlags &= ~flags; }
		void SetLodRatio(int nLodRatio)             { lodRatio = (uint8)std::min(255, std::max(0, nLodRatio)); }
		void SetViewDistRatio(int nViewDistRatio)   { viewDistRatio = (uint8)std::min(255, std::max(0, nViewDistRatio)); }
		void SetMinSpec(ESystemConfigSpec spec)     { minSpec = spec; }
	};

	//! Callback used for visiting components
	//! @see IEntity::VisitComponents method
	typedef std::function<void (IEntityComponent*)> ComponentsVisitor;

public:
	// <interfuscator:shuffle>
	virtual ~IEntity() {}

	//! Retrieves the runtime unique identifier of this entity assigned to it by the Entity System.
	//! EntityId may not be the same when saving/loading entity
	//! EntityId is not unique across the network, but can be serialized via the 'eid' compression policy
	//! EntityId is mostly used in runtime for fast and unique identification of entities, as the system allows for an O(1) retrieval
	//! \return The entity instance's identifier
	virtual EntityId GetId() const = 0;

	//! Retrieves the globally unique identifier of this entity assigned to it by the Entity System.
	//! EntityGuid is guaranteed to be the same when saving / loading entity, it is also same in the editor and in the game.
	//! \return The entity globally unique identifier.
	virtual const EntityGUID& GetGuid() const = 0;

	//! Retrieves the entity class pointer.
	//! Entity class defines entity type, what script it will use, etc...
	//! \return Pointer to the entity class interface.
	virtual IEntityClass* GetClass() const = 0;

	//! Retrieves the entity archetype.
	//! Entity archetype contain definition for entity script properties.
	//! \return Pointer to the entity archetype interface.
	CRY_DEPRECATED_ENTTIY_ARCHETYPE virtual IEntityArchetype* GetArchetype() const = 0;

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

	//! \return true if entity is completely initialized.
	virtual bool IsInitialized() const = 0;

	//! Checks if this entity was marked for deletion.
	//! If this function returns true, it will be deleted on next frame, and it is pointless to perform any operations on such entity.
	//! \return True if entity marked for deletion, false otherwise.
	virtual bool IsGarbage() const = 0;

	//! Returns a number which is incremeting every time the entity components get changed
	//! Can be checked with an older one to determine if the entity components have changed
	virtual uint8 GetComponentChangeState() const = 0;

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
	virtual void SerializeXML(XmlNodeRef& entityNode, bool bLoading, bool bIncludeScriptProxy = true, bool bExcludeSchematycProperties = false) = 0;

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
	virtual void DetachAll(AttachmentFlagsType attachmentFlags = 0) = 0;

	//! Detach this entity from the parent entity (assumes that this entity is the child entity).
	virtual void DetachThis(AttachmentFlagsType attachmentFlags = 0, EntityTransformationFlagsType transformReasons = 0) = 0;

	//! Retrieves the number of attached child entities.
	virtual int GetChildCount() const = 0;

	//! Retrieves the attached child entity by index.
	//! \param nIndex Index of the child entity, must be 0 <= nIndex < GetChildCount().
	//! \return Pointer to the child entity interface.
	virtual IEntity* GetChild(int nIndex) const = 0;

	//! Retrieves the parent of this entity.
	//! \return Pointer to the parent entity interface, or NULL if this entity does not have a parent.
	virtual IEntity* GetParent() const = 0;

	//! Retrieves the parent of this entity if the parent is a local simulation grid
	//! \return Pointer to the parent entity interface, or NULL if this entity does not have a parent or the parent is not a local simulation grid.
	virtual IEntity* GetLocalSimParent() const = 0;

	//! Retrieves the TM of the point this entity is attached to if it has a parent.
	//! \note This can be different from GetParent()->GetWorldTM() when the attachment point is not the pivot.
	//! \return World space matrix of the parent attachment transform
	virtual Matrix34 GetParentAttachPointWorldTM() const = 0;

	//! \return true if the matrix returned by GetParentAttachPointWorldTM is valid, otherwise false.
	virtual bool IsParentAttachmentValid() const = 0;

	// Entity transformation.

	//! Sets entity transformation matrix in the world space.
	//! \param tm World space transformation matrix (Non unifrorm scale is not supported).
	virtual void SetWorldTM(const Matrix34& tm, EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask()) = 0;

	//! Sets entity transformation matrix in the local entity space.
	//! \param tm Local space transformation matrix (Non unifrorm scale is not supported).
	virtual void SetLocalTM(const Matrix34& tm, EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask()) = 0;

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

	//! Force local bounds.
	//! \param[out] bounds Bounding box in local space.
	//! \param bDoNotRecalculate When set to true entity will never try to recalculate local bounding box set by this call.
	virtual void SetLocalBounds(const AABB& bounds, bool bDoNotRecalculate) = 0;

	//! Invalidates local or world space bounding box.
	virtual void InvalidateLocalBounds() = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Set the entity local space position.
	virtual void SetPos(const Vec3& vPos, EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask(), bool bRecalcPhyBounds = false, bool bForce = false) = 0;

	//! Retrieve the entity local space position.
	virtual const Vec3& GetPos() const = 0;

	//! Sets the entity local space rotation quaternion.
	virtual void SetRotation(const Quat& qRotation, EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask()) = 0;

	//! Retrieves the entity local space rotation quaternion.
	virtual const Quat& GetRotation() const = 0;

	//! Sets the entity local space scale.
	virtual void SetScale(const Vec3& vScale, EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask()) = 0;

	//! Retrieves the entity local space scale.
	virtual const Vec3& GetScale() const = 0;

	//! Sets the entity position,rotation and scale at once.
	virtual void SetPosRotScale(const Vec3& vPos, const Quat& qRotation, const Vec3& vScale, EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask()) = 0;

	//! Helper function to retrieve world space entity position.
	virtual Vec3 GetWorldPos() const = 0;

	//! Helper function to retrieve world space entity orientation angles.
	virtual Ang3 GetWorldAngles() const = 0;

	//! Helper function to retrieve world space entity orientation quaternion
	virtual Quat GetWorldRotation() const = 0;

	//! Helper function to retrieve world space entity scale
	virtual Vec3 GetWorldScale() const = 0;
	//////////////////////////////////////////////////////////////////////////

	//! Used to update the stored mask for a certain component, from IEntityComponent::GetEventMask
	//! For example, changing IEntityComponent::GetEventMask to return ENTITY_EVENT_UPDATE and then calling GetEventMask will result in the component and its entity being updated the next frame.
	//! \par Example
	//! \include CryEntitySystem/Examples/ComponentEvents.cpp
	virtual void UpdateComponentEventMask(const IEntityComponent* pComponent) = 0;

	//! Checks whether the entity is set to be updated this / next frame.
	virtual bool IsActivatedForUpdates() const = 0;

	//////////////////////////////////////////////////////////////////////////

	//! Saves or loads entity parameters to/from stream using serialization context class.
	//! \param serializer Serialization context class, provides all the information needed to properly serialize entity contents.
	//! \param nFlags Additional custom serialization flags.
	virtual void Serialize(TSerialize serializer, int nFlags = 0) = 0;

	//////////////////////////////////////////////////////////////////////////

	//! Sends event to the entity.
	//! \param event Event description (event id, parameters).
	virtual bool SendEvent(const SEntityEvent& event) = 0;

	//! Adds a listener that can receive the specified event from this entity
	virtual void AddEventListener(EEntityEvent event, IEntityEventListener* pListener) = 0;
	//! Removes an existing event listener from the entity
	virtual void RemoveEventListener(EEntityEvent event, IEntityEventListener* pListener) = 0;

	//////////////////////////////////////////////////////////////////////////

	//! Starts the entity timer.
	//! Entity timers are owned by entity, every entity can have it`s own independent timers,
	//! so TimerId must not be unique across multiple entities.
	//! When timer finishes entity system will signal to the entity *once* with an event ENTITY_EVENT_TIME.
	//! Multiple timers can be started simultaneously with different timer ids.
	//! If some timer is not yet finished and another timer with the same timerID is set, the new one
	//! will override old timer, and old timer will not send finish event.
	//! \param nTimerId Timer ID, multiple timers with different IDs are possible. (specify IEntity::CREATE_NEW_UNIQUE_TIMER_ID to let the system generate a new and unique Id)
	//! \param nMilliSeconds Timer timeout time in milliseconds.
	//! \return the (generated) timerId
	virtual int SetTimer(int nTimerId, int nMilliSeconds) = 0;

	//! Stops already started entity timer with this id.
	//! \param nTimerId Timer ID of the timer started for this entity. (specify IEntity::KILL_ALL_TIMER to stop all timer on this entity)
	virtual void KillTimer(int nTimerId) = 0;

	//! Hides this entity, makes it invisible and disable its physics.
	//! Note that the difference between Invisible and Hide is that hiding will also disable certain update logic, such as physics.
	//! \param bHide If true hide the entity, is false unhides it.
	virtual void Hide(bool bHide, EEntityHideFlags hideFlags = ENTITY_HIDE_NO_FLAG) = 0;

	//! Checks if the entity is hidden.
	virtual bool IsHidden() const = 0;

	//! Makes the entity invisible and disable its physics.
	//! Different from hide in that the entity is still updated, and physics is still active
	virtual void Invisible(bool bInvisible) = 0;

	//! Checks if the entity is invisible.
	virtual bool IsInvisible() const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual const IAIObject* GetAI() const = 0;
	virtual IAIObject*       GetAI() = 0;
	virtual bool             HasAI() const = 0;
	virtual tAIObjectID      GetAIObjectID() const = 0;
	virtual void             SetAIObjectID(tAIObjectID id) = 0;
	//////////////////////////////////////////////////////////////////////////

	//! Retrieve access to the rendering functionality of the Entity.
	IEntity* GetRenderInterface() { return this; };

	//! Called by networking on the server to serialize entity information in order to replicate the entity (and its components) on a remote host.
	//! The data is then deserialized on the remote host via SEntitySpawnParams::pSpawnSerializer.
	//! This function will collect information from components by calling IEntityComponent::NetReplicateSerialize.
	virtual ISerializableInfoPtr GetSerializableNetworkSpawnInfo() const = 0;

	// Entity Proxies Interfaces access functions.

	//! Retrieves a pointer to the specified proxy interface in the entity.
	//! \param proxy Proxy interface identifier.
	//! \return Valid pointer to the requested entity proxy object, or NULL if such proxy not exist.
	virtual IEntityComponent* GetProxy(EEntityProxy proxy) const = 0;

	//! Creates a specified proxy in the entity.
	//! \param proxy Proxy interface identifier.
	//! \return Valid pointer to the created entity proxy object, or NULL if creation failed.
	virtual IEntityComponent* CreateProxy(EEntityProxy proxy) = 0;

	//////////////////////////////////////////////////////////////////////////
	// EntityComponents
	//////////////////////////////////////////////////////////////////////////

	//! Queries an entity component type by interface, and then creates the component inside this entity
	//! \param interfaceId The GUID of the component we want to add
	//! \param pInitParams Optional initialization parameters to determine the initial state of the component
	//! \return The newly created entity component pointer (owned by the entity), or null if creation failed.
	virtual IEntityComponent* CreateComponentByInterfaceID(const CryInterfaceID& interfaceId, IEntityComponent::SInitParams* pInitParams = nullptr) = 0;

	//! Adds an already created component to the entity
	//! \param pComponent A shared pointer to the component we want to add, can not be null!
	//! \param pInitParams Optional initialization parameters to determine the initial state of the component
	//! \return True if the component was added, otherwise false.
	virtual bool AddComponent(std::shared_ptr<IEntityComponent> pComponent, IEntityComponent::SInitParams* pInitParams = nullptr) = 0;

	//! Remove previously created component from the Entity
	//! \param pComponent Component pointer to remove from the Entity
	virtual void RemoveComponent(IEntityComponent* pComponent) = 0;

	//! Removes and shut downs all components that entity contains.
	virtual void RemoveAllComponents() = 0;

	//! Replaces an existing component in the entity with another, without shutting down the existing component
	//! Used for reallocating components at run-time while keeping internal state intact
	//! Note that this function does *not* copy any data between the components, that is up to the caller.
	virtual void ReplaceComponent(IEntityComponent* pExistingComponent, std::shared_ptr<IEntityComponent> pNewComponent) = 0;

	//! Return first component of the entity with the specified class ID.
	//! \param interfaceID Identifier for the component implementation.
	virtual IEntityComponent* GetComponentByTypeId(const CryInterfaceID& interfaceID) const = 0;
	//! Return all components with the specified class ID contained in the entity.
	//! \param interfaceID Identifier for the component implementation.
	virtual void GetComponentsByTypeId(const CryInterfaceID& interfaceID, DynArray<IEntityComponent*>& components) const = 0;

	//! Return component with the unique GUID.
	//! \param guid Identifier for the component.
	virtual IEntityComponent* GetComponentByGUID(const CryGUID& guid) const = 0;

	//! Queries an array of entity components implementing the specified component interface
	virtual void              QueryComponentsByInterfaceID(const CryInterfaceID& interfaceID, DynArray<IEntityComponent*>& components) const = 0;
	//! Queries an entity component implementing the specified component interface
	virtual IEntityComponent* QueryComponentByInterfaceID(const CryInterfaceID& interfaceID) const = 0;

#if !defined(SWIG)
	// Helper to check for reflection in std::enable_if, since this can't be done with a constexpr function
	template<class ComponentType>
	struct IsComponentReflected { static constexpr bool value = Schematyc::IsReflectedType<ComponentType>(); };

	// Incorrectly implemented component, either doesn't reflect Schematyc properties or is an incorrectly configured legacy component!
	template<class ComponentType, typename = void>
	struct SComponentType
	{
		enum { kFalse = false };
		static_assert(kFalse, "Component was not reflected!");
	};

	// New Schematyc component type
	template<class ComponentType>
	struct SComponentType<ComponentType, typename std::enable_if<IsComponentReflected<ComponentType>::value&& std::is_convertible<ComponentType, IEntityComponent>::value>::type>
	{
		static const CryGUID& GetGUID()
		{
			static const CryGUID s_guid = Schematyc::GetTypeGUID<ComponentType>();
			return s_guid;
		}

		template<typename ... TArgs>
		static ComponentType* CreateInEntity(IEntity* pEntity, TArgs&& ... args)
		{
			std::shared_ptr<ComponentType> pComponent = std::make_shared<ComponentType>(std::forward<TArgs>(args) ...);
			IEntityComponent::SInitParams initParams = IEntityComponent::SInitParams { pEntity, CryGUID::Create(), "", &Schematyc::GetTypeDesc<ComponentType>(), EEntityComponentFlags::None, nullptr, nullptr };
			if (pEntity->AddComponent(pComponent, &initParams))
			{
				static_cast<IEntityComponent*>(pComponent.get())->Initialize();

				return pComponent.get();
			}

			return nullptr;
		}
	};

	// Legacy entity components
	template<class ComponentType>
	struct SComponentType<ComponentType, typename std::enable_if<!IsComponentReflected<ComponentType>::value&& std::is_convertible<ComponentType, IEntityComponent>::value>::type>
	{
		static const CryGUID& GetGUID()
		{
			return cryiidof<ComponentType>();
		}

		template<typename ... TArgs>
		static ComponentType* CreateInEntity(IEntity* pEntity, TArgs&& ... args)
		{
			std::shared_ptr<ComponentType> pComponent = std::make_shared<ComponentType>(std::forward<TArgs>(args) ...);
			if (pEntity->AddComponent(pComponent))
			{
				static_cast<IEntityComponent*>(pComponent.get())->Initialize();

				return pComponent.get();
			}

			return nullptr;
		}
	};

	//! Creates a new entity component of type ComponentType from a registered factory and adds it to the entity.
	//! ComponentType must be a valid IEntityComponent implementation.
	//! \returns A pointer to the newly created component, otherwise nullptr (and likely an assertion).
	template<typename ComponentType>
	ComponentType* CreateComponent()
	{
		return static_cast<ComponentType*>(CreateComponentByInterfaceID(SComponentType<ComponentType>::GetGUID()));
	}

	//! Get existing or Create a new initialized component inside the entity.
	//! \param bCheckHierarchy Whether or not we should traverse the inheritance hierarchy for all components, otherwise performs a (much faster) equality check to see if the implementation is ComponentType
	template<typename ComponentType>
	ComponentType* GetOrCreateComponent(bool bCheckHierarchy = true)
	{
		if (ComponentType* pComponent = GetComponent<ComponentType>(bCheckHierarchy))
		{
			return pComponent;
		}
		else
		{
			return CreateComponent<ComponentType>();
		}
	}

	//! Helper template function to simplify finding the first component implementing the specified component type
	//! This will traverse the inheritance tree, and is therefore slower than GetComponentByImplementation which simply does an equality check.
	//! ex: auto pScriptProxy = pEntity->GetComponent<IEntityScriptComponent>();
	//! \param bCheckHierarchy Whether or not we should traverse the inheritance hierarchy for all components, otherwise performs a (much faster) equality check to see if the implementation is ComponentType
	template<typename ComponentType>
	ComponentType* GetComponent(bool bCheckHierarchy = true) const
	{
		if (bCheckHierarchy)
		{
			return static_cast<ComponentType*>(QueryComponentByInterfaceID(SComponentType<ComponentType>::GetGUID()));
		}
		else
		{
			return static_cast<ComponentType*>(GetComponentByTypeId(SComponentType<ComponentType>::GetGUID()));
		}

		return nullptr;
	}

	//! Helper template function to simplify finding all components implementing the specified component type
	//! \param components DynArray into which any found components will be stored
	//! \param bCheckHierarchy Whether or not we should traverse the inheritance hierarchy for all components, otherwise performs a (much faster) equality check to see if the implementation is ComponentType
	template<typename ComponentType>
	void GetAllComponents(DynArray<ComponentType*>& components, bool bCheckHierarchy = true) const
	{
		// Hack to avoid copy of vectors, seeing as the interface querying guarantees that the pointers inside are compatible
		DynArray<IEntityComponent*>* pRawComponents = (DynArray<IEntityComponent*>*)(void*) & components;
		if (bCheckHierarchy)
		{
			QueryComponentsByInterfaceID(SComponentType<ComponentType>::GetGUID(), *pRawComponents);
		}
		else
		{
			GetComponentsByTypeId(SComponentType<ComponentType>::GetGUID(), *pRawComponents);
		}

		// Static cast all types from IEntityComponent* to ComponentType*
		for (auto it = pRawComponents->begin(); it != pRawComponents->end(); ++it)
		{
			*it = static_cast<ComponentType*>((IEntityComponent*)(void*)*it);
		}
	}

	//! Create a new initialized component using a new operator of the class type.
	template<class ComponentType, typename ... TArgs>
	ComponentType* CreateComponentClass(TArgs&& ... args)
	{
		return SComponentType<ComponentType>::CreateInEntity(this, std::forward<TArgs>(args) ...);
	}

	//! Get existing or Create a new initialized component using the new operator of the class type, typeid of the component is null guid.
	template<class ComponentType, typename ... TArgs>
	ComponentType* GetOrCreateComponentClass(TArgs&& ... args)
	{
		if (ComponentType* pComponent = GetComponent<ComponentType>())
		{
			return pComponent;
		}
		else
		{
			return CreateComponentClass<ComponentType>(std::forward<TArgs>(args) ...);
		}
	}

	//! Removes the first instance of the specified component type found in the entity
	//! \param checkHierarchy Whether or not we should traverse the inheritance hierarchy for all components, otherwise performs a (much faster) equality check to see if the implementation is ComponentType
	template<typename ComponentType>
	void RemoveComponent(bool checkHierarchy = true)
	{
		if (ComponentType* pComponent = GetComponent<ComponentType>(checkHierarchy))
		{
			RemoveComponent(pComponent);
		}
	}

	//! Removes all instances of the specified component type found in the entity
	//! \param checkHierarchy Whether or not we should traverse the inheritance hierarchy for all components, otherwise performs a (much faster) equality check to see if the implementation is ComponentType
	template<typename ComponentType>
	void RemoveAllComponents(bool checkHierarchy = true)
	{
		DynArray<ComponentType*> components;
		GetAllComponents(components, checkHierarchy);

		for(ComponentType* pComponent : components)
		{
			RemoveComponent(pComponent);
		}
	}
#endif // !defined(SWIG)

	//! Retrieve array of entity components
	virtual void GetComponents(DynArray<IEntityComponent*>& components) const = 0;

	//! Retrieve nu number of components in the entity.
	virtual uint32 GetComponentsCount() const = 0;

	//! Calls a callback on all of the components in the entity
	//! This is a more efficient way to operate on components then using GetComponents method as it is not require temporary allocations
	virtual void VisitComponents(const ComponentsVisitor& visitor) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Physics.
	//////////////////////////////////////////////////////////////////////////
	//! Physicalize entity by creating a PhysicalEntity based on a specified physical parameters
	//! \see IEntity::PhysicalizeSlot
	//! \par Example
	//! \include CryEntitySystem/Examples/PhysicalizeEntity.cpp
	virtual void Physicalize(SEntityPhysicalizeParams& params) = 0;

	//! Enable/Disable physics by flag.
	virtual void EnablePhysics(bool enable) = 0;
	//! Is physical simulation enabled?
	virtual bool IsPhysicsEnabled() const = 0;

	//! Assign a pre-created physical entity to this proxy.
	//! \param pPhysEntity The pre-created physical entity.
	//! \param nSlot Slot Index to which the new position will be taken from.
	virtual void AssignPhysicalEntity(IPhysicalEntity* pPhysEntity, int nSlot = -1) = 0;

	//! \return A physical entity assigned to an entity.
	virtual IPhysicalEntity* GetPhysics() const = 0;
	IPhysicalEntity*         GetPhysicalEntity() const { return GetPhysics(); };

	//! Adds physical geometry to the physical entity tied to this entity, based on the slot (if present)
	//! Entity has to have been physicalized with IEntity::Physicalize first.
	//! \see IEntity::Physicalize
	//! \par Example
	//! \include CryEntitySystem/Examples/PhysicalizeEntitySlot.cpp
	virtual int              PhysicalizeSlot(int slot, SEntityPhysicalizeParams& params) = 0;
	virtual void             UnphysicalizeSlot(int slot) = 0;
	virtual void             UpdateSlotPhysics(int slot) = 0;

	virtual void             SetPhysicsState(XmlNodeRef& physicsState) = 0;

	//! Get world bounds of physical object.
	//! \param[out] bounds Bounding box in world space.
	virtual void GetPhysicsWorldBounds(AABB& bounds) const = 0;

	//! Add impulse to physical entity.
	virtual void AddImpulse(int ipart, const Vec3& pos, const Vec3& impulse, bool bPos, float fAuxScale, float fPushScale = 1.0f) = 0;

	//! Physicalizes the foliage of StatObj in slot iSlot.
	virtual bool PhysicalizeFoliage(int iSlot) = 0;
	//! Dephysicalizes the foliage in slot iSlot.
	virtual void DephysicalizeFoliage(int iSlot) = 0;

	//! retrieve starting partid for a given slot.
	virtual int GetPhysicalEntityPartId0(int islot = 0) = 0;

	//! Enable/disable network serialization of the physics aspect.
	virtual void PhysicsNetSerializeEnable(bool enable) = 0;
	virtual void PhysicsNetSerializeTyped(TSerialize& ser, int type, int flags) = 0;
	virtual void PhysicsNetSerialize(TSerialize& ser) = 0;
	//////////////////////////////////////////////////////////////////////////

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

	//! Allocates index for an entity slot.
	virtual int AllocateSlot() = 0;

	//! Frees slot at specified index.
	virtual void FreeSlot(int nIndex) = 0;

	//! Gets number of allocated object slots in the entity.
	virtual int GetSlotCount() const = 0;

	//! Removes all object slots from the entity.
	virtual void ClearSlots() = 0;

	//! Gets object slot information.
	//! \param nIndex Index of required slot.
	//! \param slotInfo Output variable where description of the slot will be written.
	virtual bool GetSlotInfo(int nIndex, SEntitySlotInfo& slotInfo) const = 0;

	//! \return World transformation matrix of the object slot.
	virtual Matrix34 GetSlotWorldTM(int nSlot) const = 0;

	//! \return Local transformation matrix relative to host entity transformation matrix of the object slot.
	//! \param nSlot - Index of required slot.
	//! \param bRelativeToParent - flag specifying whether the local transformation matrix is relative to the parent slot or the entity.
	virtual Matrix34 GetSlotLocalTM(int nSlot, bool bRelativeToParent) const = 0;

	//! Sets local transformation matrix of the object slot.
	virtual void SetSlotLocalTM(int nSlot, const Matrix34& localTM, EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask()) = 0;

	//! Sets camera space position of the object slot.
	virtual void SetSlotCameraSpacePos(int nSlot, const Vec3& cameraSpacePos) = 0;

	//! Gets camera space position of the object slot.
	virtual void GetSlotCameraSpacePos(int nSlot, Vec3& cameraSpacePos) const = 0;

	//! Attaches child slot to the parent slot.
	//! This will form hierarchical transformation relationship between object slots.
	//! \param nParentIndex Index of the parent slot (Child slot will be attached to this one).
	//! \param nChildIndex Index of the child slot.
	virtual bool SetParentSlot(int nParentIndex, int nChildIndex) = 0;

	//! Prepare and update an entity slot to be used with component
	virtual void UpdateSlotForComponent(IEntityComponent* pComponent, bool callOnTransformChanged = true) = 0;

	//! Assigns a custom material to the specified object slot.
	//! \param nSlot Index of the slot, if -1 assign this material to all existing slots.
	//! \param pMaterial Pointer to custom material interface.
	virtual void SetSlotMaterial(int nSlot, IMaterial* pMaterial) = 0;

	//! Retrieve slot's custom material (This material Must have been applied before with the SetSlotMaterial).
	//! \param nSlot Slot to query custom material from.
	//! \return Custom material applied on the slot.
	virtual IMaterial* GetSlotMaterial(int nSlot) const = 0;

	//! Retrieve an actual material used for rendering specified slot.
	//! Will return custom applied material or if custom material not set will return an actual material assigned to the slot geometry.
	//! \param nSlot Slot to query used material from, if -1 material will be taken from the first renderable slot.
	//! \return Material used for rendering, or NULL if slot is not rendered.
	virtual IMaterial* GetRenderMaterial(int nSlot = -1) const = 0;

	//! Sets the flags of the specified slot.
	//! \param nSlot Index of the slot, if -1 apply to all existing slots.
	//! \param nFlags Flags to set.
	virtual void SetSlotFlags(int nSlot, uint32 nFlags) = 0;

	//! Retrieves the flags of the specified slot.
	//! \param nSlot Index of the slot.
	//! \return Slot flags, or 0 if specified slot is not valid.
	virtual uint32 GetSlotFlags(int nSlot) const = 0;

	//! Assign sub-object hide mask to slot.
	//! \param nSlot Slot to apply hide mask to.
	virtual void SetSubObjHideMask(int nSlot, hidemask nSubObjHideMask) = 0;

	//! Retrieve sub-object hide mask from slot.
	//! \param nSlot Slot to apply hide mask to.
	virtual hidemask GetSubObjHideMask(int nSlot) const = 0;

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
	virtual int SetCharacter(ICharacterInstance* pCharacter, int nSlot, bool bUpdatePhysics = true) = 0;

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
	//! \par Example
	//! \include CryEntitySystem/Examples/LoadGeometry.cpp
	virtual int LoadGeometry(int nSlot, const char* sFilename, const char* sGeomName = NULL, int nLoadFlags = 0) = 0;

	//! Loads character to the specified slot, or to next available slot.
	//! If same character is already loaded in this slot, operation is ignored.
	//! If this slot number is occupied by different kind of object it is overwritten.
	//! \return Slot id where the object was loaded, or -1 if loading failed.
	//! \par Example
	//! \include CryEntitySystem/Examples/LoadCharacter.cpp
	virtual int LoadCharacter(int nSlot, const char* sFilename, int nLoadFlags = 0) = 0;

#if defined(USE_GEOM_CACHES)
	//! Loads geometry cache to the specified slot, or to next available slot.
	//! If same geometry cache is already loaded in this slot, operation is ignored.
	//! If this slot number is occupied by different kind of object it is overwritten.
	//! \return Slot id where the object was loaded, or -1 if loading failed.
	//! \par Example
	//! \include CryEntitySystem/Examples/LoadGeomCache.cpp
	virtual int LoadGeomCache(int nSlot, const char* sFilename) = 0;
#endif

	//! Assigns a 3DEngine render node to the Entity Slot.
	virtual int SetSlotRenderNode(int nSlot, IRenderNode* pRenderNode) = 0;

	//! Returns a 3DEngine render node assigned to an Entity Slot.
	virtual IRenderNode* GetSlotRenderNode(int nSlot) = 0;

	//! Returns the network proxy associated with the entity.
	//! Use the proxy to modify the entity's network behavior.
	virtual INetEntity* GetNetEntity() = 0;

	//! \details Do not use. Provides a way for CGameObject to replace NetEntity with itself.
	virtual INetEntity* AssignNetEntityLegacy(INetEntity* ptr) = 0;

	//! Loads a new particle emitter to the specified slot, or to next available slot.
	//! If this slot number is occupied by different kind of object it is overwritten.
	//! \return Slot id where the particle emitter was loaded, or -1 if loading failed.
	//! \par Example
	//! \include CryEntitySystem/Examples/LoadParticleEmitter.cpp
	virtual int LoadParticleEmitter(int nSlot, IParticleEffect* pEffect, SpawnParams const* params = NULL, bool bPrime = false, bool bSerialize = false) = 0;

	//! Sets an existing particle emitter to the specified slot, or the next available slot.
	//! If this slot number is occupied by different kind of object it is overwritten.
	//! \return Slot id where the particle emitter was loaded, or -1 if loading failed.
	virtual int SetParticleEmitter(int nSlot, IParticleEmitter* pEmitter, bool bSerialize = false) = 0;

	//! Loads a light source to the specified slot, or to next available slot.
	//! \return Slot id where the light source was loaded, or -1 if loading failed.
	//! \par Example
	//! \include CryEntitySystem/Examples/LoadLight.cpp
	virtual int LoadLight(int nSlot, SRenderLight* pLight) = 0;

	//! Updates the clip bounds of the light. Doesn't update any entity slots.
	//! \param Light which should update the clip volumes.
	//! \return Returns true if the light box was updated.
	virtual bool UpdateLightClipBounds(SRenderLight& light) = 0;

	//! Loads a fog volume to the specified slot, or to the next available slot.
	//! \return Slot id where the fog volume was loaded, or -1 if loading failed.
	//! \par Example
	//! \include CryEntitySystem/Examples/LoadFogVolume.cpp
	virtual int LoadFogVolume(int nSlot, const SFogVolumeProperties& properties) = 0;
	
	//! Invalidates the entity's and all its children's transformation matrices!
	virtual void InvalidateTM(EntityTransformationFlagsMask transformReasons = EntityTransformationFlagsMask(), bool bRecalcPhyBounds = false) = 0;

	//! Easy Script table access.
	CRY_DEPRECATED_ENTTIY_LUA virtual IScriptTable* GetScriptTable() const = 0;

	//! Retrieve 3D Engine render node, used to render this entity for a given slot.
	//! If nSlot is -1 return first existing render node in slots.
	virtual IRenderNode* GetRenderNode(int nSlot = -1) const = 0;

	//! Check if Entity is being Rendered by 3dengine.
	//! It doesn't necessary mean that it will be visible on screen but only that 3d engine considers it for a rendering now
	virtual bool IsRendered() const = 0;

	//! Render a preview of the Entity
	//! This method is not used when entity is normally rendered
	//! But only used for previewing the entity in the Sandbox Editor
	virtual void PreviewRender(SEntityPreviewContext& context) = 0;

	//! Sets common parameters that are applied on all render nodes for this render proxy.
	virtual void                       SetRenderNodeParams(const IEntity::SRenderNodeParams& params) = 0;
	virtual IEntity::SRenderNodeParams GetRenderNodeParams() const = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Entity links.
	//////////////////////////////////////////////////////////////////////////

	//! Gets pointer to the first entity link.
	virtual IEntityLink* GetEntityLinks() = 0;
	virtual IEntityLink* AddEntityLink(const char* sLinkName, EntityId entityId, EntityGUID entityGuid) = 0;
	virtual void         RenameEntityLink(IEntityLink* pLink, const char* sNewLinkName) = 0;
	virtual void         RemoveEntityLink(IEntityLink* pLink) = 0;
	virtual void         RemoveAllEntityLinks() = 0;
	//////////////////////////////////////////////////////////////////////////

	//! \param[out] partId Set to the child's partId.
	//! \return An attached child entity that corresponds to the physical part partId.
	virtual IEntity* UnmapAttachedChild(int& partId) = 0;

	virtual void     GetMemoryUsage(ICrySizer* pSizer) const = 0;

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

	//! Called by the 3DEngine when RenderNode created by this entity start or stop being rendered.
	virtual void OnRenderNodeVisibilityChange(bool bBecomeVisible) = 0;

	//! \return the last time (as set by the system timer) when the entity was last seen.
	virtual float GetLastSeenTime() const = 0;

	//! ObjectID that corresponds to editor base objects. This is used for selection and highlighting
	//! so it should be set by editor and have a 1-1 correspondence with a baseobject. This is intended as a
	//! runtime ID and does not need to be serialized
	virtual uint32 GetEditorObjectID() const = 0;
	virtual void   GetEditorObjectInfo(bool& bSelected, bool& bHighlighted) const = 0;
	virtual void   SetEditorObjectInfo(bool bSelected, bool bHighlighted) = 0;

	//! Retrieve current simulation mode of the Entity
	virtual EEntitySimulationMode GetSimulationMode() const = 0;

	// </interfuscator:shuffle>

	//! Schematyc Object associated with this entity
	virtual Schematyc::IObject* GetSchematycObject() const = 0;
	virtual void                OnSchematycObjectDestroyed() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Helper methods
	//! Set the viewDistRatio on the render node.
	void  SetViewDistRatio(int nViewDistRatio);
	//! Sets the LodRatio on the render node.
	void  SetLodRatio(int nLodRatio);
	void  SetAdditionalRenderNodeFlags(uint64 flags, bool bEnable);
	void  SetOpacity(float fAmount);
	float GetOpacity() const { return GetRenderNodeParams().opacity; };
	//////////////////////////////////////////////////////////////////////////

	//! Returns the forward facing direction of the entity
	Vec3 GetForwardDir() const
	{
		if (!GetScale().IsUnit())
		{
			Matrix34 auxTM = GetWorldTM();
			auxTM.OrthonormalizeFast();
			return auxTM.GetColumn1();
		}

		return GetWorldRotation().GetColumn1();
	}
	//! Returns the right facing direction of the entity
	Vec3 GetRightDir() const
	{
		if (!GetScale().IsUnit())
		{
			Matrix34 auxTM = GetWorldTM();
			auxTM.OrthonormalizeFast();
			return auxTM.GetColumn0();
		}

		return GetWorldRotation().GetColumn0();
	}
	//! Returns the up facing direction of the entity
	Vec3 GetUpDir() const
	{
		if (!GetScale().IsUnit())
		{
			Matrix34 auxTM = GetWorldTM();
			auxTM.OrthonormalizeFast();
			return auxTM.GetColumn2();
		}

		return GetWorldRotation().GetColumn2();
	}
};

typedef IEntity IEntityRender;
typedef IEntity IEntityPhysicalProxy;

template<typename DST, typename SRC>
DST crycomponent_cast(SRC pComponent) { return static_cast<DST>(pComponent); }

// Helper methods
//! Set the viewDistRatio on the render node.
inline void IEntity::SetViewDistRatio(int nViewDistRatio)
{
	auto params = GetRenderNodeParams();
	params.SetViewDistRatio(nViewDistRatio);
	SetRenderNodeParams(params);
}

//! Sets the LodRatio on the render node.
inline void IEntity::SetLodRatio(int nLodRatio)
{
	auto params = GetRenderNodeParams();
	params.SetLodRatio(nLodRatio);
	SetRenderNodeParams(params);
}
inline void IEntity::SetAdditionalRenderNodeFlags(uint64 flags, bool bEnable)
{
	auto params = GetRenderNodeParams();
	params.SetRndFlags(flags, bEnable);
	SetRenderNodeParams(params);
}
inline void IEntity::SetOpacity(float fAmount)
{
	auto params = GetRenderNodeParams();
	params.opacity = fAmount;
	SetRenderNodeParams(params);
}

//////////////////////////////////////////////////////////////////////////
// IEntityComponent methods that depend on the IEntity interface
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
ILINE EntityId IEntityComponent::GetEntityId() const
{
	return m_pEntity->GetId();
}

//////////////////////////////////////////////////////////////////////////
ILINE int IEntityComponent::GetOrMakeEntitySlotId()
{
	if (m_entitySlotId == EmptySlotId)
	{
		GetEntity()->UpdateSlotForComponent(this);
	}
	return m_entitySlotId;
}

//////////////////////////////////////////////////////////////////////////
ILINE void IEntityComponent::NetMarkAspectsDirty(const NetworkAspectType aspects)
{
	if (INetEntity* pNetEntity = GetEntity()->GetNetEntity())
	{
		pNetEntity->MarkAspectsDirty(aspects);
	}
}
//////////////////////////////////////////////////////////////////////////
ILINE void IEntityComponent::FreeEntitySlot()
{
	if (m_entitySlotId != EmptySlotId)
	{
		GetEntity()->FreeSlot(m_entitySlotId);
		m_entitySlotId = EmptySlotId;
	}
}

//////////////////////////////////////////////////////////////////////////
inline EEntitySimulationMode IEntityComponent::GetEntitySimulationMode() const
{
	return GetEntity()->GetSimulationMode();
}

//////////////////////////////////////////////////////////////////////////
inline void IEntityComponent::SetTransformMatrix(const Matrix34& transform)
{
	if (m_pTransform == nullptr)
	{
		m_componentFlags.Add(EEntityComponentFlags::Transform);
		m_pTransform = std::make_shared<CryTransform::CTransform>();
	}

	*m_pTransform = CryTransform::CTransform(transform);
	GetEntity()->UpdateSlotForComponent(this);
}

inline void IEntityComponent::SetTransformMatrix(const CryTransform::CTransformPtr& transform)
{
	if (m_pTransform == nullptr)
	{
		m_componentFlags.Add(EEntityComponentFlags::Transform);
		m_pTransform = std::make_shared<CryTransform::CTransform>();
	}

	*m_pTransform = *transform;
	GetEntity()->UpdateSlotForComponent(this);
}

//////////////////////////////////////////////////////////////////////////
inline Matrix34 IEntityComponent::GetWorldTransformMatrix() const
{
	return m_pEntity->GetWorldTM() * GetTransformMatrix();
}

//////////////////////////////////////////////////////////////////////////
//! Utility namespace used by physics.
namespace EntityPhysicsUtils
{
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
}
