// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once


#include <CryEntitySystem/IEntity.h>
#include <CryCore/Containers/CryListenerSet.h>

//////////////////////////////////////////////////////////////////////////
// This is the order in which the proxies are serialized
const static EEntityProxy ProxySerializationOrder[] = {
	ENTITY_PROXY_SCRIPT,
	ENTITY_PROXY_AUDIO,
	ENTITY_PROXY_AREA,
	ENTITY_PROXY_BOIDS,
	ENTITY_PROXY_BOID_OBJECT,
	ENTITY_PROXY_CAMERA,
	ENTITY_PROXY_FLOWGRAPH,
	ENTITY_PROXY_SUBSTITUTION,
	ENTITY_PROXY_TRIGGER,
	ENTITY_PROXY_USER,
	ENTITY_PROXY_ROPE,
	ENTITY_PROXY_ENTITYNODE,
	ENTITY_PROXY_CLIPVOLUME,
	ENTITY_PROXY_DYNAMICRESPONSE,
	ENTITY_PROXY_LAST
};

//////////////////////////////////////////////////////////////////////////
// These headers cannot be replaced with forward references.
// They are needed for correct up casting from IEntityComponent to real proxy class.
	#include "RenderProxy.h"
	#include "PhysicsProxy.h"
	#include "ScriptProxy.h"
	#include "SubstitutionProxy.h"
	#include "EntityComponentsVector.h"
//////////////////////////////////////////////////////////////////////////

// forward declarations.
struct IEntitySystem;
struct IEntityArchetype;
class CEntitySystem;
struct AIObjectParams;
struct SGridLocation;
struct SProximityElement;

namespace Schematyc
{
	// Forward declare interfaces.
	struct IObjectProperties;
	// Forward declare shared pointers.
	DECLARE_SHARED_POINTERS(IObjectProperties)
}

// (MATT) This should really live in a minimal AI include, which right now we don't have  {2009/04/08}
	#ifndef INVALID_AIOBJECTID
typedef uint32 tAIObjectID;
		#define INVALID_AIOBJECTID ((tAIObjectID)(0))
	#endif

//////////////////////////////////////////////////////////////////////
class CEntity : public IEntity
{
	// Entity constructor.
	// Should only be called from Entity System.
	CEntity(SEntitySpawnParams& params);

public:
	// Entity destructor.
	// Should only be called from Entity System.
	virtual ~CEntity();

	// Called by entity system to complete initialization of the entity.
	bool Init(SEntitySpawnParams& params);
	// Called by EntitySystem every frame for each pre-physics active entity.
	void PrePhysicsUpdate(SEntityEvent& event);
	// Called by EntitySystem every frame for each active entity.
	void Update(SEntityUpdateContext& ctx);
	// Called by EntitySystem before entity is destroyed.
	void ShutDown();

	//////////////////////////////////////////////////////////////////////////
	// IEntity interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EntityId   GetId() const final   { return m_nID; }
	virtual const EntityGUID& GetGuid() const final { return m_guid; }

	//////////////////////////////////////////////////////////////////////////
	virtual IEntityClass*     GetClass() const final     { return m_pClass; }
	virtual IEntityArchetype* GetArchetype() const final { return m_pArchetype; }

	//////////////////////////////////////////////////////////////////////////
	virtual void   SetFlags(uint32 flags) final;
	virtual uint32 GetFlags() const final                      { return m_flags; }
	virtual void   AddFlags(uint32 flagsToAdd) final           { SetFlags(m_flags | flagsToAdd); }
	virtual void   ClearFlags(uint32 flagsToClear) final       { SetFlags(m_flags & (~flagsToClear)); }
	virtual bool   CheckFlags(uint32 flagsToCheck) const final { return (m_flags & flagsToCheck) == flagsToCheck; }

	virtual void   SetFlagsExtended(uint32 flags) final;
	virtual uint32 GetFlagsExtended() const final                            { return m_flagsExtended; }

	virtual bool   IsGarbage() const final                                   { return m_bGarbage; }
	virtual bool   IsLoadedFromLevelFile() const final                       { return m_bLoadedFromLevelFile; }
	ILINE void     SetLoadedFromLevelFile(const bool bIsLoadedFromLevelFile) { m_bLoadedFromLevelFile = bIsLoadedFromLevelFile; }

	//////////////////////////////////////////////////////////////////////////
	virtual void        SetName(const char* sName) final;
	virtual const char* GetName() const final { return m_szName.c_str(); }
	virtual string      GetEntityTextDescription() const final;

	//////////////////////////////////////////////////////////////////////////
	virtual void     AttachChild(IEntity* pChildEntity, const SChildAttachParams& attachParams) final;
	virtual void     DetachAll(int nDetachFlags = 0) final;
	virtual void     DetachThis(int nDetachFlags = 0, int nWhyFlags = 0) final;
	virtual int      GetChildCount() const final;
	virtual IEntity* GetChild(int nIndex) const final;
	virtual void     EnableInheritXForm(bool bEnable);
	virtual IEntity* GetParent() const final { return m_hierarchy.pParent; }
	virtual IEntity* GetLocalSimParent() const final { return m_hierarchy.parentBindingType == EBindingType::eBT_LocalSim ? m_hierarchy.pParent : nullptr; }
	virtual Matrix34 GetParentAttachPointWorldTM() const final;
	virtual bool     IsParentAttachmentValid() const final;
	virtual IEntity* GetAdam();
																												
	//////////////////////////////////////////////////////////////////////////
	virtual void SetWorldTM(const Matrix34& tm, int nWhyFlags = 0) final;
	virtual void SetLocalTM(const Matrix34& tm, int nWhyFlags = 0) final;

	// Set position rotation and scale at once.
	virtual void            SetPosRotScale(const Vec3& vPos, const Quat& qRotation, const Vec3& vScale, int nWhyFlags = 0) final;

	virtual Matrix34        GetLocalTM() const final;
	virtual const Matrix34& GetWorldTM() const final { return m_worldTM; }

	virtual void            GetWorldBounds(AABB& bbox) const final;
	virtual void            GetLocalBounds(AABB& bbox) const final;
	virtual void            SetLocalBounds(const AABB& bounds, bool bDoNotRecalculate) final;
	virtual void            InvalidateLocalBounds() final;

	//////////////////////////////////////////////////////////////////////////
	virtual void        SetPos(const Vec3& vPos, int nWhyFlags = 0, bool bRecalcPhyBounds = false, bool bForce = false) final;
	virtual const Vec3& GetPos() const final { return m_vPos; }

	virtual void        SetRotation(const Quat& qRotation, int nWhyFlags = 0) final;
	virtual const Quat& GetRotation() const final { return m_qRotation; }

	virtual void        SetScale(const Vec3& vScale, int nWhyFlags = 0) final;
	virtual const Vec3& GetScale() const final    { return m_vScale; }

	virtual Vec3        GetWorldPos() const final { return m_worldTM.GetTranslation(); }
	virtual Ang3        GetWorldAngles() const final;
	virtual Quat        GetWorldRotation() const final;
	//////////////////////////////////////////////////////////////////////////

	virtual void UpdateComponentEventMask(const IEntityComponent* pComponent) final;
	virtual bool IsActivatedForUpdates() const final { return m_bInActiveList; }

	virtual void PrePhysicsActivate(bool bActive) final;
	virtual bool IsPrePhysicsActive() final;

	//////////////////////////////////////////////////////////////////////////
	virtual void Serialize(TSerialize ser, int nFlags) final;

	virtual bool SendEvent(const SEntityEvent& event) final;

	virtual void AddEventListener(EEntityEvent event, IEntityEventListener* pListener) final;
	virtual void RemoveEventListener(EEntityEvent event, IEntityEventListener* pListener) final;

	//////////////////////////////////////////////////////////////////////////

	virtual int SetTimer(int nTimerId, int nMilliSeconds) final;
	virtual void KillTimer(int nTimerId) final;

	virtual void Hide(bool bHide, EEntityHideFlags hideFlags = ENTITY_HIDE_NO_FLAG) final;
	        void SendHideEvent(bool bHide, EEntityHideFlags hideFlags);
	virtual bool IsHidden() const final { return m_bHidden; }
	virtual bool IsInHiddenLayer() const final { return m_bIsInHiddenLayer; }

	virtual void Invisible(bool bInvisible) final;
	virtual bool IsInvisible() const final { return m_bInvisible; }

	virtual uint8 GetComponentChangeState() const final;

	//////////////////////////////////////////////////////////////////////////
	virtual const IAIObject* GetAI() const final            { return m_aiObjectID ? GetAIObject() : nullptr; }
	virtual IAIObject*  GetAI() final                       { return m_aiObjectID ? GetAIObject() : nullptr; }
	virtual bool        HasAI() const final                 { return m_aiObjectID != 0; }
	virtual tAIObjectID GetAIObjectID() const final         { return m_aiObjectID; }
	virtual void        SetAIObjectID(tAIObjectID id) final { m_aiObjectID = id; }
	//////////////////////////////////////////////////////////////////////////

	virtual void                SetUpdatePolicy(EEntityUpdatePolicy eUpdatePolicy) final;
	virtual EEntityUpdatePolicy GetUpdatePolicy() const final { return (EEntityUpdatePolicy)m_eUpdatePolicy; }

	//////////////////////////////////////////////////////////////////////////
	// Entity Proxies Interfaces access functions.
	//////////////////////////////////////////////////////////////////////////
	virtual IEntityComponent* GetProxy(EEntityProxy proxy) const final;
	virtual IEntityComponent* CreateProxy(EEntityProxy proxy) final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	virtual IEntityComponent* CreateComponentByInterfaceID(const CryInterfaceID& interfaceId, IEntityComponent::SInitParams *pInitParams) final;
	virtual bool              AddComponent(std::shared_ptr<IEntityComponent> pComponent, IEntityComponent::SInitParams *pInitParams) final;
	virtual void              RemoveComponent(IEntityComponent* pComponent) final;
	virtual void              RemoveAllComponents() final;
	virtual void              ReplaceComponent(IEntityComponent* pExistingComponent, std::shared_ptr<IEntityComponent> pNewComponent) final;
	virtual IEntityComponent* GetComponentByTypeId(const CryInterfaceID& interfaceID) const final;
	virtual void              GetComponentsByTypeId(const CryInterfaceID& interfaceID, DynArray<IEntityComponent*>& components) const final;
	virtual IEntityComponent* GetComponentByGUID(const CryGUID& guid) const final;
	virtual void              QueryComponentsByInterfaceID(const CryInterfaceID& interfaceID, DynArray<IEntityComponent*> &components) const final;
	virtual IEntityComponent* QueryComponentByInterfaceID(const CryInterfaceID& interfaceID) const final;
	virtual void              CloneComponentsFrom(IEntity& otherEntity) final;
	virtual void              GetComponents( DynArray<IEntityComponent*> &components ) const final;
	virtual uint32            GetComponentsCount() const final;
	virtual void              VisitComponents(const ComponentsVisitor &visitor) final;
	
	//////////////////////////////////////////////////////////////////////////
	// Physics.
	//////////////////////////////////////////////////////////////////////////
	virtual void             Physicalize(SEntityPhysicalizeParams& params) final;
	virtual void             AssignPhysicalEntity(IPhysicalEntity* pPhysEntity, int nSlot = -1) final;
	virtual void             EnablePhysics(bool enable) final;
	virtual bool             IsPhysicsEnabled() const final;
	virtual IPhysicalEntity* GetPhysics() const final;

	virtual int              PhysicalizeSlot(int slot, SEntityPhysicalizeParams& params) final;
	virtual void             UnphysicalizeSlot(int slot) final;
	virtual void             UpdateSlotPhysics(int slot) final;

	virtual void             SetPhysicsState(XmlNodeRef& physicsState) final;
	virtual void             GetPhysicsWorldBounds(AABB& bounds) const final;

	virtual void             AddImpulse(int ipart, const Vec3& pos, const Vec3& impulse, bool bPos, float fAuxScale, float fPushScale = 1.0f) final;

	virtual bool             PhysicalizeFoliage(int iSlot) final;
	virtual void             DephysicalizeFoliage(int iSlot) final;

	//! retrieve starting partid for a given slot.
	virtual int  GetPhysicalEntityPartId0(int islot = 0) final;

	virtual void PhysicsNetSerializeEnable(bool enable) final;
	virtual void PhysicsNetSerializeTyped(TSerialize& ser, int type, int flags) final;
	virtual void PhysicsNetSerialize(TSerialize& ser) final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Custom entity material.
	//////////////////////////////////////////////////////////////////////////
	virtual void       SetMaterial(IMaterial* pMaterial) final;
	virtual IMaterial* GetMaterial() final;

	//////////////////////////////////////////////////////////////////////////
	// Entity Slots interface.
	//////////////////////////////////////////////////////////////////////////
	virtual bool                       IsSlotValid(int nSlot) const final;
	virtual int                        AllocateSlot() final;
	virtual void                       FreeSlot(int nSlot) final;
	virtual int                        GetSlotCount() const final;
	virtual void                       ClearSlots() final;
	virtual bool                       GetSlotInfo(int nSlot, SEntitySlotInfo& slotInfo) const final;
	virtual const Matrix34&            GetSlotWorldTM(int nIndex) const final;
	virtual const Matrix34&            GetSlotLocalTM(int nIndex, bool bRelativeToParent) const final;
	virtual void                       SetSlotLocalTM(int nIndex, const Matrix34& localTM, int nWhyFlags = 0) final;
	virtual void                       SetSlotCameraSpacePos(int nIndex, const Vec3& cameraSpacePos) final;
	virtual void                       GetSlotCameraSpacePos(int nSlot, Vec3& cameraSpacePos) const final;
	virtual bool                       SetParentSlot(int nParentSlot, int nChildSlot) final;
	virtual void                       SetSlotMaterial(int nSlot, IMaterial* pMaterial) final;
	virtual IMaterial*                 GetSlotMaterial(int nSlot) const final;
	virtual IMaterial*                 GetRenderMaterial(int nSlot = -1) const final;
	virtual void                       SetSlotFlags(int nSlot, uint32 nFlags) final;
	virtual uint32                     GetSlotFlags(int nSlot) const final;
	virtual int                        SetSlotRenderNode(int nSlot, IRenderNode* pRenderNode) final;
	virtual IRenderNode*               GetSlotRenderNode(int nSlot) final;
	virtual void                       UpdateSlotForComponent(IEntityComponent *pComponent) final;
	virtual bool                       ShouldUpdateCharacter(int nSlot) const final;
	virtual ICharacterInstance*        GetCharacter(int nSlot) final;
	virtual int                        SetCharacter(ICharacterInstance* pCharacter, int nSlot, bool bUpdatePhysics) final;
	virtual IStatObj*                  GetStatObj(int nSlot) final;
	virtual int                        SetStatObj(IStatObj* pStatObj, int nSlot, bool bUpdatePhysics, float mass = -1.0f) final;
	virtual IParticleEmitter*          GetParticleEmitter(int nSlot) final;
	virtual IGeomCacheRenderNode*      GetGeomCacheRenderNode(int nSlot) final;
	virtual IRenderNode*               GetRenderNode(int nSlot = -1) const final;
	virtual bool                       IsRendered() const final;
	virtual void                       PreviewRender(SEntityPreviewContext &context) final;

	virtual void                       MoveSlot(IEntity* targetIEnt, int nSlot) final;

	virtual int                        LoadGeometry(int nSlot, const char* sFilename, const char* sGeomName = NULL, int nLoadFlags = 0) final;
	virtual int                        LoadCharacter(int nSlot, const char* sFilename, int nLoadFlags = 0) final;
	#if defined(USE_GEOM_CACHES)
	virtual int                        LoadGeomCache(int nSlot, const char* sFilename) final;
	#endif
	virtual int                        SetParticleEmitter(int nSlot, IParticleEmitter* pEmitter, bool bSerialize = false) final;
	virtual int                        LoadParticleEmitter(int nSlot, IParticleEffect* pEffect, SpawnParams const* params = NULL, bool bPrime = false, bool bSerialize = false) final;
	virtual int                        LoadLight(int nSlot, SRenderLight* pLight) final;
	int                                LoadLightImpl(int nSlot, SRenderLight* pLight);

	virtual bool                       UpdateLightClipBounds(SRenderLight& light);
	int                                LoadCloudBlocker(int nSlot, const SCloudBlockerProperties& properties);
	virtual int                        LoadFogVolume(int nSlot, const SFogVolumeProperties& properties) override;

	int                                FadeGlobalDensity(int nSlot, float fadeTime, float newGlobalDensity);

	virtual void                       SetSubObjHideMask(int nSlot, hidemask nSubObjHideMask) final        { GetEntityRender()->SetSubObjHideMask(nSlot, nSubObjHideMask); };
	virtual hidemask                   GetSubObjHideMask(int nSlot) const final                            { return GetEntityRender()->GetSubObjHideMask(nSlot); };

	virtual void                       SetRenderNodeParams(const IEntity::SRenderNodeParams& params) final { GetEntityRender()->SetRenderNodeParams(params); };
	virtual IEntity::SRenderNodeParams GetRenderNodeParams() const final                                   { return GetEntityRender()->GetRenderNodeParams(); };

	virtual void                       InvalidateTM(int nWhyFlags = 0, bool bRecalcPhyBounds = false) final;

	// Inline implementation.
	virtual IScriptTable* GetScriptTable() const final;

	// Load/Save entity parameters in XML node.
	virtual void         SerializeXML(XmlNodeRef& node, bool bLoading, bool bIncludeScriptProxy = true) final;

	virtual IEntityLink* GetEntityLinks() final;
	virtual IEntityLink* AddEntityLink(const char* sLinkName, EntityId entityId, EntityGUID entityGuid) final;
	virtual void         RenameEntityLink(IEntityLink* pLink, const char* sNewLinkName) final;
	virtual void         RemoveEntityLink(IEntityLink* pLink) final;
	virtual void         RemoveAllEntityLinks() final;
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	virtual IEntity* UnmapAttachedChild(int& partId) final;

	virtual void     GetMemoryUsage(ICrySizer* pSizer) const final;

	//////////////////////////////////////////////////////////////////////////
	// Local methods.
	//////////////////////////////////////////////////////////////////////////

	// Returns true if entity was already fully initialized by this point.
	virtual bool IsInitialized() const final { return m_bInitialized; }
	//////////////////////////////////////////////////////////////////////////

	// Get fast access to the slot, only used internally by entity components.
	class CEntitySlot* GetSlot(int nSlot) const;

	// Get render proxy object.
	ILINE const CEntityRender* GetEntityRender() const { return static_cast<const CEntityRender*>(&m_render); }
	ILINE CEntityRender*       GetEntityRender()       { return static_cast<CEntityRender*>(&m_render); }

	// Get physics proxy object.
	ILINE const CEntityPhysics* GetPhysicalProxy() const { return static_cast<const CEntityPhysics*>(&m_physics); }
	ILINE CEntityPhysics*       GetPhysicalProxy()       { return static_cast<CEntityPhysics*>(&m_physics); }

	// Get script proxy object.
	ILINE CEntityComponentLuaScript* GetScriptProxy() const { return (CEntityComponentLuaScript*)CEntity::GetProxy(ENTITY_PROXY_SCRIPT); }
	//////////////////////////////////////////////////////////////////////////

	// For internal use.
	CEntitySystem* GetCEntitySystem() const { return g_pIEntitySystem; }

	//////////////////////////////////////////////////////////////////////////
	// Activates entity only for specified number of frames.
	// numUpdates must be a small number from 0-15.
	void ActivateForNumUpdates(int numUpdates);

	void ActivateEntityIfNecessary();
	bool ShouldActivate() const;

	void PrepareForDeletion();

	//////////////////////////////////////////////////////////////////////////
	// Description:
	//   Invalidates entity and childs cached world transformation.
	void CalcLocalTM(Matrix34& tm) const;

	void Hide(bool bHide, EEntityEvent eEvent1, EEntityEvent eEvent2);

	// for ProximityTriggerSystem
	SProximityElement* GetProximityElement()         { return m_pProximityEntity; }

	virtual void       IncKeepAliveCounter() final   { m_nKeepAliveCounter++; }
	virtual void       DecKeepAliveCounter() final   { assert(m_nKeepAliveCounter > 0); m_nKeepAliveCounter--; }
	virtual void       ResetKeepAliveCounter() final { m_nKeepAliveCounter = 0; }
	virtual bool       IsKeptAlive() const final     { return m_nKeepAliveCounter > 0; }

	// LiveCreate entity interface
	virtual bool  HandleVariableChange(const char* szVarName, const void* pVarData) final;

	virtual void  OnRenderNodeVisibilityChange(bool bBecomeVisible) final;
	virtual float GetLastSeenTime() const final;

	virtual INetEntity* AssignNetEntityLegacy(INetEntity* ptr) final;
	virtual INetEntity* GetNetEntity() final;

	//////////////////////////////////////////////////////////////////////////
	void RemoveAllEventListeners();

	virtual	uint32 GetEditorObjectID() const final override;
	virtual	void   SetObjectID(uint32 ID) final override;
	virtual	void   GetEditorObjectInfo(bool& bSelected, bool& bHighlighted) const final override;
	virtual	void   SetEditorObjectInfo(bool bSelected, bool bHighlighted) final override;

	void SetInHiddenLayer(bool bHiddenLayer);
	virtual	Schematyc::IObject* GetSchematycObject() const final { return m_pSchematycObject; };
	virtual void OnSchematycObjectDestroyed() final { m_pSchematycObject = nullptr; }

	virtual EEntitySimulationMode GetSimulationMode() const final { return m_simulationMode; };
	//~IEntity

	void SetSimulationMode(EEntitySimulationMode mode);
	void OnEditorGameModeChanged(bool bEnterGameMode);

	void ShutDownComponent(IEntityComponent* pComponent);

	CEntityComponentsVector& GetComponentsVector() { return m_components; };

protected:
	//////////////////////////////////////////////////////////////////////////
	// Attachment.
	//////////////////////////////////////////////////////////////////////////
	void OnRellocate(int nWhyFlags);
	void LogEvent(const SEntityEvent& event, CTimeValue dt);
	//////////////////////////////////////////////////////////////////////////

private:
	bool IsScaled(float threshold = 0.0003f) const
	{
		return (fabsf(m_vScale.x - 1.0f) + fabsf(m_vScale.y - 1.0f) + fabsf(m_vScale.z - 1.0f)) >= threshold;
	}
	// Fetch the IA object from the AIObjectID, if any
	IAIObject* GetAIObject() const;

	void CreateSchematycObject(const SEntitySpawnParams& params);

	void AddComponentInternal(std::shared_ptr<IEntityComponent> pComponent, const CryGUID& componentTypeID, IEntityComponent::SInitParams *pInitParams, const CEntityComponentClassDesc* pClassDescription);

	//////////////////////////////////////////////////////////////////////////
	// Component Save/Load
	//////////////////////////////////////////////////////////////////////////
	// Loads a component, but leaves it uninitialized
	void LoadComponent(Serialization::IArchive& archive);
	void SaveComponent(Serialization::IArchive& archive,IEntityComponent &component);
	// Loads a component with the legacy XML format, but leaves it uninitialized
	bool LoadComponentLegacy(XmlNodeRef& entityNode, XmlNodeRef& componentNode);
	void SaveComponentLegacy(CryGUID typeId,XmlNodeRef& entityNode,XmlNodeRef& componentNode,IEntityComponent &component,bool bIncludeScriptProxy);
	//////////////////////////////////////////////////////////////////////////

	void OnComponentMaskChanged(const IEntityComponent& component, uint64 newMask);

private:
	friend class CEntitySystem;
	friend class CPhysicsEventListener; // For faster access to internals.
	friend class CEntitySlot;           // For faster access to internals.
	friend class CEntityRender;         // For faster access to internals.
	friend class CEntityComponentTriggerBounds;
	friend class CEntityPhysics;
	friend class CNetEntity; // CNetEntity iterates all components on serialization.

	enum class EBindingType
	{
		eBT_Pivot,
		eBT_GeomCacheNode,
		eBT_CharacterBone,
		eBT_LocalSim,
	};
	virtual EBindingType GetParentBindingType() const final { return m_hierarchy.parentBindingType; }

	// Childs structure, only allocated when any child entity is attached.
	struct STransformHierarchy
	{
		std::vector<CEntity*> childs;
		CEntity*              pParent = nullptr;
		EBindingType          parentBindingType = EBindingType::eBT_Pivot;
		int                   attachId = -1;
		attachMask            childrenAttachIds = 0; // bitmask of used attachIds of the children
	};

	struct SEventListeners
	{
		struct SListener
		{
			EEntityEvent          event;
			IEntityEventListener* pListener;
			SListener() : event(ENTITY_EVENT_LAST), pListener(nullptr) {}
			SListener(EEntityEvent e, IEntityEventListener* l) : event(e), pListener(l) {}
			SListener(IEntityEventListener* l) : event(ENTITY_EVENT_LAST), pListener(l) {}
			operator bool() const { return pListener != 0; }
			bool operator==(const SListener& l) const { return event == l.event && pListener == l.pListener; }
			bool operator!=(const SListener& l) const { return !(*this == l); }

		};
		SEventListeners(size_t reserve) : m_listeners(reserve), haveListenersMask(0) {}
		// Bit mask of the events where listener was registered
		// Clearing the bit is not required, it will only cause slightly lower performance of the send event
		uint64                  haveListenersMask;
		CListenerSet<SListener> m_listeners;
	};

private:
	//////////////////////////////////////////////////////////////////////////
	// Internal entity flags (must be first member var of CEntity) (Reduce cache misses on access to entity data).
	//////////////////////////////////////////////////////////////////////////
	unsigned int         m_bInActiveList  : 1;      // Added to entity system active list.
	unsigned int         m_bRequiresComponentUpdate  : 1; // Whether or not any components require update callbacks
	unsigned int         m_bInitialized   : 1;      // Set if this entity already Initialized.
	unsigned int         m_bHidden        : 1;      // Set if this entity is hidden.
	unsigned int         m_bIsInHiddenLayer : 1;    // Set if this entity is in a hidden layer.
	unsigned int         m_bGarbage       : 1;      // Set if this entity is garbage and will be removed soon.

	unsigned int         m_bTrigger       : 1;      // Set if entity is proximity trigger itself.
	unsigned int         m_bWasRelocated  : 1;      // Set if entity was relocated at least once.
	unsigned int         m_nUpdateCounter : 4;      // Update counter is used to activate the entity for the limited number of frames.
	                                                // Usually used for Physical Triggers.
	                                                // When update counter is set, and entity is activated, it will automatically
	                                                // deactivate after this counter reaches zero
	unsigned int m_eUpdatePolicy        : 4;        // Update policy defines in which cases to call
	                                                // entity update function every frame.
	unsigned int m_bInvisible           : 1;        // Set if this entity is invisible.
	unsigned int m_bNotInheritXform     : 1;        // Inherit or not transformation from parent.
	unsigned int m_bInShutDown          : 1;        // Entity is being shut down.

	unsigned int m_bLoadedFromLevelFile : 1;    // Entity was loaded from level file
	
	uint8 m_componentChangeState;

	// Name of the entity.
	string m_szName;

	// Pointer to the class that describe this entity.
	IEntityClass* m_pClass;

	// Pointer to the entity archetype.
	IEntityArchetype* m_pArchetype;

	// Pointer to hierarchical binding structure.
	// It contains array of child entities and pointer to the parent entity.
	// Because entity attachments are not used very often most entities do not need it,
	// and space is preserved by keeping it separate structure.
	STransformHierarchy m_hierarchy;

	// Custom entity material.
	_smart_ptr<IMaterial> m_pMaterial;

	// Array of components, linear search in a small array is almost always faster then a more complicated set or map containers.
	CEntityComponentsVector m_components;

	// Entity Links.
	IEntityLink* m_pEntityLinks;

	// For tracking entity in the partition grid.
	SGridLocation*     m_pGridLocation;
	// For tracking entity inside proximity trigger system.
	SProximityElement* m_pProximityEntity;

	//! Schematyc object associated with this entity.
	Schematyc::IObject* m_pSchematycObject = nullptr;
	
	//! Public Properties of the associated Schematyc object
	Schematyc::IObjectPropertiesPtr m_pSchematycProperties = nullptr;

	std::unique_ptr<SEventListeners> m_pEventListeners;
	
	// NetEntity stores the network serialization configuration.
	std::unique_ptr<INetEntity> m_pNetEntity;

	CEntityRender                    m_render;
	CEntityPhysics                   m_physics;

	// Optional entity guid.
	EntityGUID m_guid;

	// Unique ID of the entity.
	EntityId m_nID;

	// The representation of this entity in the AI system.
	tAIObjectID m_aiObjectID;

	// Entity flags. any combination of EEntityFlags values.
	uint32 m_flags;

	// Entity extended flags. any combination of EEntityFlagsExtended values.
	uint32 m_flagsExtended;

	//! Simulation mode of this entity, (ex Preview, In Game, In Editor)
	EEntitySimulationMode m_simulationMode = EEntitySimulationMode::Idle;

	// Position of the entity in local space.
	Vec3             m_vPos;
	// Rotation of the entity in local space.
	Quat             m_qRotation;
	// Scale of the entity in local space.
	Vec3             m_vScale;
	// World transformation matrix of this entity.
	mutable Matrix34 m_worldTM;

	// counter to prevent deletion if entity is processed deferred by for example physics events
	uint32 m_nKeepAliveCounter;

	uint32                           m_objectID;
};
