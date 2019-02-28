// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: C++ Item Implementation
  
 -------------------------------------------------------------------------
  History:
  - 27:10:2004   11:25 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEM_H__
#define __ITEM_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <CryScriptSystem/IScriptSystem.h>
#include <CryGame/IGameFramework.h>
#include <IItemSystem.h>
#include <IActionMapManager.h>
#include <ICryMannequinEditor.h>
#include <CryAnimation/CryCharAnimationParams.h>
#include <map>
#include <list>


#include "ItemScheduler.h"
#include "ItemString.h"
#include "ItemDefinitions.h"
#include "ItemAnimation.h"
#include <CryCore/Containers/CryFixedArray.h>
#include "EntityUtility/EntityScriptCalls.h"
#include "EntityUtility/EntityEffects.h"

#define ITEM_DESELECT_POSE			"nw"

#define ITEM_OWNER_ACTION_LAYER		WeaponAnimLayerID::Total

#define FP_OFFSET_OVERRIDE_BLEND_TIME (0.4f)

class CItemSharedParams;
class CTagState;
class IActionController;

struct IAttachmentManager;
struct ICharacterInstance;
struct SParams;
struct SMountParams;
struct SMannequinItemParams;

class CTagDefinition;

enum EItemUpdateSlots
{
  eIUS_General = 0,
	eIUS_Zooming = 1,
	eIUS_FireMode = 2,  
	eIUS_Scheduler = 3,  
};

class CPlayer;
class CActor;
DECLARE_SHARED_POINTERS(CActor);

class CItem :
	public CGameObjectExtensionHelper<CItem, IItem, 39>,
	public IGameObjectProfileManager
{
	friend class CScriptBind_Item;
public:
	struct SelectAction;
	struct SwitchHandAction;
	struct ExchangeToNextItem;

	//------------------------------------------------------------------------
	// Typedefs
	//------------------------------------------------------------------------
	enum ePhysicalization
	{
		eIPhys_PhysicalizedRigid,
		eIPhys_PhysicalizedStatic,
		eIPhys_NotPhysicalized,
		eIPhys_Max
	};

	enum ETimer
	{
		eIT_Flying = 0,
		eIT_Last,
	};

	enum ePlayActionFlags
	{
		eIPAF_Default								= 0,
		eIPAF_ConcentratedFire			= 1 << 25,	// used for sound & animation changes. C2 and C2MP specific
	};

	typedef uint32 TItemFlags;
	enum EItemFlags {
		eIF_None											= 0,
		eIF_Modifying									= BIT(2),
		eIF_Transitioning							= BIT(3),
		eIF_Cloaked										= BIT(4),
		eIF_SerializeCloaked					= BIT(5), //used for cloaked serialization
		eIF_AccessoryAmmoAvailable		= BIT(6),
		eIF_IgnoreHeat								= BIT(7),
		eIF_NoDrop										= BIT(9),	//Fix reseting problem in editor
		eIF_SerializeDestroyed				= BIT(10),
		eIF_SerializeRigidPhysics			= BIT(11),
		eIF_PostSerializeSelect				= BIT(12),
		eIF_RegisteredAs1pUser				= BIT(14),
		eIF_RegisteredAs3pUser				= BIT(15),
		eIF_SelectGrabbingWeapon			= BIT(17),
		eIF_Selecting									= BIT(18),
		eIF_Unholstering							= BIT(19),
		eIF_UnholsteringPlaySelect		= BIT(20),
		eIF_InformClientsAboutUse			= BIT(21),
		eIF_PlayFastSelect						= BIT(22),
		eIF_PlayFastSelectAsSpeedBias	= BIT(23),
		eIF_BlockActions							= BIT(24), //Used to block melee, firemode switch and charged fires while fire-over-time actions are active
		eIF_ExchangeItemScheduled			= BIT(25),
		eIF_UseFastSelectTag					= BIT(26),
		eIF_UseAnimActionUnhide				= BIT(27),
	};

	enum eViewMode
	{
		eIVM_FirstPerson = 1,
		eIVM_ThirdPerson = 2,
	};

	enum eItemAttachment
	{
		eIA_None,
		eIA_WeaponEntity,
		eIA_WeaponCharacter,
		eIA_StowPrimary,
		eIA_StowSecondary,
	};

	enum eItemLowerMode
	{
		eILM_Raised,
		eILM_Lower,
		eILM_Cinematic,
	};

	typedef CryFixedStringT<32> TempAGInputName;

	struct SStats
	{
		SStats()
		:	fp(false),
			mounted(false),
			mount_last_aimdir(Vec3(0.0f,0.0f,0.0f)),
			mount_dir(Vec3(0.0f,1.0f,0.0f)),      
			pickable(true),
			selected(false),
			dropped(false),
			detached(false),
			brandnew(true),      
			flying(false),      
			viewmode(0),
      used(false),
			sound_enabled(true),
			hand(eIH_Right),
      health(0.f),
			first_selection(true),
			attachment(eIA_None),
			physicalizedSlot(eIGS_Last)
		{
		};

		void Serialize(TSerialize &ser)
		{
			ser.BeginGroup("ItemStats");
			//int vmode = viewmode;
			//ser.Value("viewmode", vmode);
			bool fly = flying;
			ser.Value("flying", fly);
			bool sel = selected;
			ser.Value("selected", sel);
			bool mount = mounted;
			ser.Value("mounted", mount);
			bool use = used;
			ser.Value("used", use);
			int han = hand;
			ser.Value("hand", han);
			bool drop = dropped;
			ser.Value("dropped", drop);
			bool detach = detached;
			ser.Value("detached", detach);
			//bool firstPerson = fp;
			//ser.Value("firstPerson", firstPerson);
			bool bnew = brandnew;
			ser.Value("brandnew", bnew);      
			float fHealth = health;
			ser.Value("health", fHealth);   
			bool bfirstSelection = first_selection;
			ser.Value("first_selection", bfirstSelection);
			int attach = (int)attachment;
			ser.Value("attachment", attach);
			bool pick = pickable;
			ser.Value("pickable", pick);
			ser.Value("mount_dir", mount_dir);
			ser.Value("mount_last_aimdir", mount_last_aimdir);
			ser.EndGroup();

			if(ser.IsReading())
			{
				//viewmode = vmode;
				flying = fly;
				selected = sel;
				mounted = mount;
				used = use;
				hand = han;
				dropped = drop;
				detached = detach;
				brandnew = bnew;        
				//fp = firstPerson;
				health = fHealth;
				first_selection = bfirstSelection;
				attachment = (eItemAttachment)attach;
				pickable = pick;
			}
		}

		Vec3	mount_dir;
		Vec3	mount_last_aimdir;
		int		hand:3;
		int		viewmode:3;
		float health;
		bool	fp:1;
		bool	mounted:1;
		bool	pickable:1;
		bool	selected:1;
		bool	dropped:1;
		bool	detached:1;
		bool	brandnew:1;    
		bool	flying:1;
		bool	used:1;
		bool	sound_enabled:1;
		bool  first_selection:1;
		eItemAttachment   attachment;
		eGeometrySlot	physicalizedSlot;
	};

	struct SEditorStats
	{
		SEditorStats()
		:	ownerId(0),
			current(false)
		{};

		SEditorStats(EntityId _ownerId, bool _current)
		:	ownerId(_ownerId),
			current(_current)
		{};


		EntityId	ownerId;
		bool			current;
	};

	struct SEntityProperties
	{
		SEntityProperties()
			: mounted_min_pitch(-30.0f)
			, mounted_max_pitch(30.0f)
			, mounted_yaw_range(60.0f)
			, hitpoints(0)
			, pickable(true)
			, mounted(false)
			, physics(eIPhys_NotPhysicalized)
			, usable(false) 
			, specialSelect(false)
		{ 
			initialSetup.resize(0);
		};

		float		mounted_min_pitch;
		float		mounted_max_pitch;
		float		mounted_yaw_range;

		ePhysicalization physics;
		int hitpoints;
		bool pickable;
		bool mounted;
		bool usable;
		bool specialSelect;
		string initialSetup;
		
		void GetMemoryStatistics(ICrySizer * s) const
		{
			s->Add(initialSetup);
		}
	};

	struct SRespawnProperties
	{
		SRespawnProperties(): timer(0.0f), unique(false), respawn(false) {};
		float timer;
		bool	unique;
		bool	respawn;
	};

	struct SAccessoryInfo
	{
		SAccessoryInfo() : pClass(NULL), accessoryId(0) {};
		SAccessoryInfo(IEntityClass* _pClass, EntityId _id) : pClass(_pClass), accessoryId(_id) {}; 
		
		IEntityClass* pClass;
		EntityId			accessoryId;
	};

	typedef CryFixedArray<SAccessoryInfo, ITEM_MAX_NUM_ACCESSORIES>	TAccessoryArray;

public:
	CItem();
	virtual ~CItem();

	// IItem, IGameObjectExtension
	static const char* GetWeaponComponentType();
	virtual const char* GetType() const;
	virtual bool Init( IGameObject * pGameObject );
	virtual void InitClient(int channelId);
	virtual void PostInitClient(int channelId);
	virtual void PostInit( IGameObject * pGameObject );
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual NetworkAspectType GetNetSerializeAspects();
	virtual void PostSerialize();
	virtual void SerializeLTL( TSerialize ser );
	virtual void SerializeSpawnInfo( TSerialize ser ) {};
	virtual ISerializableInfoPtr GetSpawnInfo() { return 0;};
	virtual void Update( SEntityUpdateContext& ctx, int );
	virtual void PostUpdate( float frameTime ) {};
	virtual void PostRemoteSpawn() {};
	virtual void HandleEvent( const SGameObjectEvent& );
	virtual void ProcessEvent(const SEntityEvent& );
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId(uint16 id) {};
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	void GetInternalMemoryUsage(ICrySizer *pSizer) const;
	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void UpdateFPView(float frameTime);
	virtual bool FilterView(struct SViewParams &viewParams);
	virtual void PostFilterView(struct SViewParams &viewParams);

	virtual void GetFPOffset(QuatT &offset) const;
	virtual bool ShouldBindOnInit() const { return true; }

	virtual bool CheckAmmoRestrictions(IInventory * pInventory);

	virtual IWeapon *GetIWeapon() { return 0; };
	virtual const IWeapon *GetIWeapon() const { return 0; }

	virtual bool IsAccessory() { return false; };

	virtual void SetOwnerId(EntityId ownerId);
	virtual EntityId GetOwnerId() const;

	virtual void Reset();
  virtual void ResetOwner();
	virtual bool ResetParams();
	virtual void PreResetParams() {};

	virtual void RemoveEntity(bool force = false);

	virtual void SetParentId(EntityId parentId);
	virtual EntityId GetParentId() const;

	virtual void SetHand(int hand);
	void SwitchToHand(int hand);
	virtual void Use(EntityId userId);
	virtual void Select(bool select);
	virtual void Drop(float impulseScale=1.0f, bool selectNext=true, bool byDeath=false);
	virtual void PickUp(EntityId picker, bool sound, bool select=true, bool keepHistory=true, const char *setup = NULL);
	virtual void Physicalize(bool enable, bool rigid);
	void Unholstering(bool playSelect = true, float biasSelectTime = 1.0f);

	virtual ePhysicalization FindPhysicalisationType(bool enable, bool rigid);
	void DeferPhysicalize(bool enable, bool rigid);

	virtual void Pickalize(bool enable, bool dropped);
	virtual void Impulse(const Vec3 &position, const Vec3 &direction, float impulse);
	void DisableCollisionWithPlayers();

	void LowerItem(eItemLowerMode mode);
	void UnlowerItem();
	ILINE bool IsLowered() const { return m_itemLowerMode != eILM_Raised; };
	eItemLowerMode GetLowerMode() const {return m_itemLowerMode;}

	virtual bool CanPickUp(EntityId userId) const;
	virtual bool CanDrop() const;
	virtual bool CanUse(EntityId userId) const;
	virtual bool IsMountable() const;
	virtual bool IsMounted() const;
	virtual bool IsRippedOff() const { return false; }
	virtual bool CanRipOff() const { return false; }
	virtual bool IsHeavyWeapon() const { return false; }
	virtual bool IsUsed() const;
	virtual bool IsPickable() const { return IsRippedOff() ? !IsUsed() : !IsMounted(); }
	virtual bool ShouldAttachWhenSelected () {return true;}

	virtual bool InitRespawn();
	virtual void TriggerRespawn();

	void CloakEnable(bool enable, bool fade, float cloakBlendSpeedScale = 1.0f, bool bFadeByDistance = false, uint8 colorChannel = 0, bool bIgnoreCloakRefractionColor = false, EntityId syncedFromId = 0 );
	void CloakSync(bool fade);
	
	virtual Vec3 GetMountedAngleLimits() const;
	virtual Vec3 GetMountedDir() const {return GetStats().mount_dir;}
	virtual void SetMountedAngleLimits(float min_pitch, float max_pitch, float yaw_range);

	virtual bool CanSelect() const;
	virtual bool CanDeselect() const;
	virtual bool IsSelecting() const;
	virtual bool IsSelected() const;
	virtual void OnParentSelect(bool select) {};
	virtual void OnParentReloaded() {};
	virtual void SetAccessoryReloadTags(CTagState& fragTags) {}

	virtual void MountAt(const Vec3 &pos);
	virtual void MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles);
	virtual void StartUse(EntityId userId);
	virtual void StopUse(EntityId  userId);
	virtual void ApplyViewLimit(EntityId userId, bool apply);

	virtual void EnableSound(bool enable);
	virtual bool IsSoundEnabled() const;
	virtual bool IsModifying() const { return AreAnyItemFlagsSet(eIF_Modifying | eIF_Transitioning); }

	// ~IItem

	bool CanFireUnderWater() const;

	ILINE bool IsDestroyed() const { return ((m_properties.hitpoints > 0) && (m_stats.health <= 0.f)); }

	virtual void PickUpAmmo(EntityId pickerId) {};
	void PickUpAccessories(CActor *pPickerActor, IInventory* pInventory);
	virtual bool HasSomeAmmoToPickUp(EntityId pickerId) const { return false; }
	virtual ColorF GetSilhouetteColor() const;

	// IGameObjectProfileManager
	virtual bool SetAspectProfile( EEntityAspects aspect, uint8 profile );
	virtual uint8 GetDefaultProfile( EEntityAspects aspect );
	// ~IGameObjectProfileManager

	virtual uint32 StartDeselection(bool fastDeselect);
	virtual void CancelDeselection();
	virtual bool IsDeselecting() const;

	void SetIgnoreHeat(bool ignoreHeat);

	// Events
	virtual void OnStartUsing();
	virtual void OnStopUsing();
	virtual void OnSelect(bool select);
	virtual void OnSelected(bool selected);
	virtual void OnReloaded();
	virtual void OnEnterFirstPerson();
	virtual void OnEnterThirdPerson();
  virtual void OnReset();
	virtual void OnPickedUp(EntityId actorId, bool destroyed);
	virtual void OnDropped(EntityId actorId, bool ownerWasAI);
	virtual void OnBeginCutScene();
	virtual void OnEndCutScene();
	virtual void OnOwnerActivated();
	virtual void OnOwnerDeactivated();
	virtual void OnOwnerStanceChanged( const EStance stance );
	  
	virtual const SStats &GetStats() const { return m_stats; };
	virtual const SParams &GetParams() const;
	virtual const SMountParams* GetMountedParams() const;
	virtual const SEntityProperties &GetProperties() const { return m_properties; };

	// params
	virtual bool ReadItemParams(const IItemParamsNode *root) { return true; }
	virtual void InitItemFromParams();
	virtual void InitGeometry();
	virtual void InitAccessories();
	virtual void InitDamageLevels();
	virtual void ReadProperties(IScriptTable *pScriptTable);

	void ReadMountedProperties(IScriptTable* pScriptTable);

	// accessories
	virtual void RemoveAccessory(const ItemString& name) { CRY_ASSERT_MESSAGE(0, "DEPRECATED: Use RemoveAccessory(const IEntityClass* pClass)"); };
	virtual void RemoveAllAccessories();
	virtual void DetachAllAccessories();
	virtual void AttachAccessory(const ItemString& name, bool attach, bool noanim, bool force = false, bool firstTimeAttached = false, bool initialLoadoutSetup = false);
	virtual void AttachAccessory(IEntityClass* pClass, bool attach, bool noanim, bool force = false, bool firstTimeAttached = false, bool initialLoadoutSetup = false);
	virtual CItem* GetAccessory(const ItemString& name);
	virtual bool IsFirstTimeAccessoryAttached(IEntityClass* pClass) const;
	void AccessoryAttachAction(CItem* pAccessory, const SAccessoryParams* params, bool initialLoadoutSetup);
	void AccessoryDetachAction(CItem* pAccessory, const SAccessoryParams* params);
	void ShowAttachmentHelper(int slot, const char* name, bool show);
	
	virtual const SAccessoryParams* GetAccessoryParams(const IEntityClass* pClass) const;
	bool IsAccessoryHelperFree(const ItemString& helper);
	virtual void InitialSetup();
	virtual void PatchInitialSetup(); 
	virtual void ReAttachAccessories();
	virtual void AccessoriesChanged(bool initialLoadoutSetup);
	virtual void FixAccessories(const SAccessoryParams *newParams, bool attach) {};
	virtual void ResetAccessoriesScreen(IActor* pOwner);
	virtual void RemoveOwnerAttachedAccessories();

	virtual void SwitchAccessory(const ItemString& accessory);
	virtual void DoSwitchAccessory(const ItemString& accessory, bool initialLoadoutSetup = false);

	virtual void DetachAccessory(const ItemString& accessory); 

	const Matrix34 GetWorldTM() const;
	const Vec3 GetWorldPos() const;
	void GetRelativeLocation(QuatT& location) const;

	ILINE bool IsAttachedToBack() const
	{
		return (m_stats.attachment == eIA_StowPrimary) || (m_stats.attachment == eIA_StowSecondary);
	}

	ILINE bool IsAttachedToHand() const
	{
		return (m_stats.attachment == eIA_WeaponCharacter) || (m_stats.attachment == eIA_WeaponEntity);
	}

	virtual void AddAccessoryAmmoToInventory(IEntityClass* pAmmoType, int count, IInventory* pOwnerInventory);
	virtual bool GivesAmmo();
	void ProcessAccessoryAmmo(IInventory* pOwnerInventory, IWeapon* pParentWeapon = NULL);
	void ProcessAccessoryAmmoCapacities(IInventory* pOwnerInventory, bool addCapacity);

	const TAccessoryArray& GetAccessories() const	{ return m_accessories; }
	const TAccessoryParamsVector& GetAccessoriesParamsVector() const;
	virtual const SAccessoryParams* GetSecondaryFiremodeAccessory() const { return NULL; }
	virtual const SAccessoryParams* GetDefaultOverrideAccessory() const	{ return NULL; }

	CItem* AddAccessory(IEntityClass* pClass);
	CItem* GetAccessory(IEntityClass* pClass);
	bool HasAccessory(const ItemString& name);
	bool HasAccessory(IEntityClass* pClass);
	void ReAttachAccessory(IEntityClass* pClass);
	void RemoveAccessory(IEntityClass* pClass);
	void RemoveAccessoryOnCategory(const ItemString& helperName);

	// effects
	EntityEffects::TAttachedEffectId AttachEffect(int slot,  
																								bool attachToAccessory = false, 
																								const char* effectName = NULL, 
																								const char* helper = NULL,
																								const Vec3& offset = Vec3Constants<float>::fVec3_Zero, 
																								const Vec3& dir = Vec3Constants<float>::fVec3_OneY, 
																								float scale = 1.0f, 
																								bool prime = true);

  EntityEffects::TAttachedEffectId AttachLight(int slot, const char *helper, const EntityEffects::SLightAttachParams& lightParams);

	void DetachEffect(EntityEffects::TAttachedEffectId id);
	IParticleEmitter *GetEffectEmitter(EntityEffects::TAttachedEffectId id) const;


	// misc
	bool AttachToHand(bool attach, bool checkAttachment = false);
	bool AttachToBack(bool attach);
	eItemAttachment ChooseAttachmentPoint(bool attach, struct IAttachment **attachmentPt = NULL) const;
	void EnableUpdate(bool enable, int slot=-1);
	void RequireUpdate(int slot=-1);
	void Hide(bool hide, bool remoteUpdate = false);
	void HideItem(bool hide);
	virtual void SetBusy(bool busy) { m_scheduler.SetBusy(busy); };
	virtual bool IsBusy() const { return m_scheduler.IsBusy(); };
	CItemScheduler *GetScheduler() { return &m_scheduler; };
	IItemSystem *GetIItemSystem() { return m_pItemSystem; };

	IEntity *GetOwner() const;
	CActor *GetOwnerActor() const;
	CActor *GetActor(EntityId actorId) const;
	CPlayer *GetOwnerPlayer() const;
  IInventory *GetActorInventory(IActor *pActor) const;
	CItem *GetActorItem(IActor *pActor) const;
	CActor *GetActorByNetChannel(INetChannel *pNetChannel) const;

	const char *GetDisplayName() const;

	virtual void ForcePendingActions(uint8 blockedActions = 0) {}

	bool ShouldNotDrop() const { return AreAnyItemFlagsSet(eIF_NoDrop); }
	virtual bool HasFastSelect(EntityId nextItemId)const;
	virtual bool ShouldPlaySelectAction() const;
	//Special FP weapon render method
	virtual void RegisterFPWeaponForRenderingAlways(bool registerRenderAlways);

	// view
	bool IsOwnerFP();
	virtual void UpdateMounted(float frameTime);  
	void CheckViewChange();
	virtual void SetViewMode(int mode);
	void AttachToShadowHand(bool attach);
	void CopyRenderFlags(IEntity *pOwner);
  virtual void UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis);

	// character attachments
	bool CreateCharacterAttachment(int slot, const char *name, int type, const char *bone);
	void DestroyCharacterAttachment(int slot, const char *name);
	ICharacterInstance* GetAppropriateCharacter(int slot, bool owner);
	void ResetCharacterAttachment(int slot, const char *name, bool owner, EntityId attachedEntId = 0);
	IMaterial* GetCharacterAttachmentMaterial(int slot, const char* name, bool owner = false);
	const char *GetCharacterAttachmentBone(int slot, const char *name);
	void SetCharacterAttachment(int slot, const char *name, IEntity *pEntity, bool owner);
	void SetCharacterAttachment(int slot, const char *name, IStatObj *pObj, bool owner);
	void SetCharacterAttachment(int slot, const char *name, ICharacterInstance *pCharacter, bool owner);
	void SetCharacterAttachment(int slot, const char *name, IEntity *pEntity, int objSlot, bool owner);
	virtual void SetCharacterAttachmentLocalTM(int slot, const char *name, const Matrix34 &tm);
	void SetCharacterAttachmentWorldTM(int slot, const char *name, const Matrix34 &tm);
	Matrix34 GetCharacterAttachmentLocalTM(int slot, const char *name);
	Matrix34 GetCharacterAttachmentWorldTM(int slot, const char *name);
	void HideCharacterAttachment(int slot, const char *name, bool hide);
	void HideCharacterAttachmentMaster(int slot, const char *name, bool hide);

	void CreateAttachmentHelpers(int slot);
	void DestroyAttachmentHelpers(int slot);
	const THelperVector& GetAttachmentHelpers();

  // damage
  virtual void OnHit(float damage, int hitType);
  virtual void OnDestroyed();
  virtual void OnRepaired();
	virtual void DestroyedGeometry(bool use);
	virtual void UpdateDamageLevel();

	// resource
	virtual bool SetGeometry(int slot, const ItemString& name, const ItemString& material=ItemString(), bool useParentMaterial=false, const Vec3& poffset=Vec3(0,0,0), const Ang3& aoffset=Ang3(0,0,0), float scale=1.0f, bool forceReload=false);

	_smart_ptr<IAction> PlayAction(FragmentID action, int layer=0, bool loop=false, uint32 flags = eIPAF_Default, float speedOverride = -1.0f, float animWeigth = 1.0f, float ffeedbackWeight = 1.0f);
	bool PlayFragment(class IAction* pAction, float speedOverride = -1.0f, float timeOverride = -1.0f, float animWeight = 1.0f, float ffeedbackWeight = 1.0f, bool concentratedFire = false);
	int GetFragmentID(const char* actionName, const CTagDefinition** pTagDef = NULL);
	virtual void SetFragmentTags(CTagState& fragTags) {};

	uint32 GetCurrentAnimationTime(int slot);
	void DrawSlot(int slot, bool draw, bool near=false);
	Vec3 GetSlotHelperPos(int slot, const char *helper, bool worldSpace, bool relative=false) const;
	const Matrix33 &GetSlotHelperRotation(int slot, const char *helper, bool worldSpace, bool relative=false);
	void Quiet();

	virtual void OnAttach(bool attach);

	IEntityAudioComponent *GetAudioProxy(bool create=false);
	IEntityRender *GetRenderProxy(bool create=false);
		
	EntityId NetGetOwnerId() const;
	void NetSetOwnerId(EntityId id);

	//JAW/C4 special stuff
	bool IsAutoDroppable() const;
	bool CanPickUpAutomatically() const;

	bool IsIdentical(CItem* pOtherItem) const;

	// Utils
	static void DeselectItem(EntityId itemId);

	struct AccessoryParams
	{
		AccessoryParams() : accessoryClassId(0) {};
		AccessoryParams(uint16 _accessoryClassId): accessoryClassId(_accessoryClassId) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("accessoryClassId", accessoryClassId, 'clas');
		};

		uint16 accessoryClassId;
	};

	struct EmptyParams
	{
		void SerializeWith(const TSerialize& ser) {};
	};

	DECLARE_SERVER_RMI_NOATTACH(SvRequestAttachAccessory, AccessoryParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClAttachAccessory, AccessoryParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClAttachInitialAccessory, AccessoryParams, eNRT_ReliableUnordered); //For accessories that are part of the initial loadout

	DECLARE_SERVER_RMI_NOATTACH(SvRequestDetachAccessory, AccessoryParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClDetachAccessory, AccessoryParams, eNRT_ReliableUnordered);
	
	DECLARE_SERVER_RMI_NOATTACH(SvRequestEnterModify, EmptyParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestLeaveModify, EmptyParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClEnterModify, EmptyParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClLeaveModify, EmptyParams, eNRT_ReliableUnordered);

	// properties
	template<typename T>bool GetEntityProperty(const char *name, T &value) const
	{
		return EntityScripts::GetEntityProperty(GetEntity(), name, value);
	}
	template<typename T>bool GetEntityProperty(const char *table, const char *name, T &value) const
	{
    return EntityScripts::GetEntityProperty(GetEntity(), table, name, value);
	}

  template<typename T>void SetEntityProperty(const char *name, const T &value)
  {
		EntityScripts::SetEntityProperty(GetEntity(), name, value);
  }

	ILINE void SetItemFlags(TItemFlags flags)						{ m_itemFlags |= flags; }
	ILINE void ClearItemFlags(TItemFlags flags)					{ m_itemFlags &= ~flags; }
	ILINE void SetItemFlag(TItemFlags flags, bool bSet) { if(bSet) SetItemFlags(flags); else ClearItemFlags(flags); }
	ILINE bool AreAnyItemFlagsSet(TItemFlags flag)	const			{ return (m_itemFlags & flag) != 0; }

	ILINE bool IsServer() {	return gEnv->bServer; }
	ILINE bool IsClient() {	return gEnv->IsClient(); }

	ILINE bool IsSelectGrabbingWeapon() const		{	return AreAnyItemFlagsSet(eIF_SelectGrabbingWeapon); }
	void DoSelectWeaponGrab();

	void Prepare1pAnimationDbas();
	void Prepare1pChrs();

	const CItemSharedParams* GetSharedItemParams() const {return m_sharedparams;}

	void ScheduleExchangeToNextItem(CActor* pActor, CItem* pNextItem, CItem* pDeselectItem);

	virtual bool IsRippingOrRippedOff() const { return false; }
	virtual void ForceRippingOff(bool ripOff) {}

	ILINE IActionController* GetActionController() const { return m_pCurrentActionController; }
	virtual void UpdateCurrentActionController();
	virtual void SetCurrentActionController( IActionController* pActionController );
	const SMannequinItemParams& GetMannequinItemParams() const {return *m_pCurrentManItemParams;}
	const SMannequinItemParams::FragmentIDs& GetFragmentIds() const {return GetMannequinItemParams().fragmentIDs;}

	virtual void SetSubContextID(int subContext)
	{
		m_subContext = subContext;
	}
	virtual int GetSubContextID()
	{
		return m_subContext;
	}
	
	virtual const string GetAttachedAccessoriesString(const char* separator = ",");
	ILINE void OnItemSelectActionComplete() { m_pSelectAction.reset(); }
	ILINE void ForceSelectActionComplete () { if(m_pSelectAction) { m_pSelectAction->ItemSelectCancelled(); } OnItemSelectActionComplete(); }

protected:
	bool InitActionController(IEntity* pEntity);
	virtual bool ShouldDoPostSerializeReset() const;

	void UnlockDefaultAttachments(CActor* pActor);
	IAttachment * GetCharacterAttachment(ICharacterInstance * pCharacter, const char *name) const;

	void UpdateActionControllerSelection(bool bSelected);

	void DropAfterRaycast(const QueuedRayID& rayID, const RayCastResult& result);
	void DetachItem(IEntity* pThisItemEntity, CActor* pOwnerActor, float impulseScale, Vec3 localDropDirection = ZERO );

	void UpdateLowerItem(CPlayer* pLocalPlayer);

	virtual float GetSelectSpeed(CActor* pOwnerActor) { return 1.f; } 

	void RegisterAsUser();
	void UnRegisterAsUser();

	void RegisterAs1pDbaUser();
	void UnRegisterAs1pDbaUser();

	void RegisterAs1pChrUser();
	void UnRegisterAs1pChrUser();

	void RegisterAs1pAudioCacheUser();
	void UnRegisterAs1pAudioCacheUser();

	void RegisterAs3pAudioCacheUser();
	void UnRegisterAs3pAudioCacheUser();

	void AudioCacheItemAndAccessories(const bool enable, const char* type);
public:
	static void AudioCacheItem(const bool enable, const IEntityClass* pClass, const char* prefix, const char* postfix);

protected:
	bool IsAudioCached(const IEntityClass *pClass, const char* type);

	IAttachmentManager *GetOwnerAttachmentManager() const;

	virtual void UpdateTags(const IActionController *pActionController, CTagState &tagState, bool selected) const;
	virtual void UpdateAccessoryTags(const SMannequinItemParams* pParams, CTagState &tagState, bool selected) const;
	virtual void UpdateMountedTags(const SMannequinItemParams* pParams, CTagState &tagState, bool selected) const;
	void UpdateScopeContexts(IActionController *pActionController, int nCharacterSlot = 0);
	void ClearScopeContexts(IActionController *pActionController);

	void AttachDefaultAttachment(IEntityClass* pAccessoryClass);

	virtual void OnUnlowerItem() {}

	class COwnerInfo
	{
	public:
		COwnerInfo() : m_id(0) {}

		ILINE EntityId GetId() const { return m_id; }
		ILINE const CActorWeakPtr& GetActorWeakPtr() const { return m_pActor; }

		void Set(EntityId id, const CActorWeakPtr& pActor)
		{
			m_id = id;
			m_pActor = pActor;
		}

		void Reset()
		{
			m_id = 0;
			m_pActor.reset();
		}

	private:
		EntityId m_id;
		CActorWeakPtr m_pActor;
	};

	// data
	_smart_ptr<CItemSharedParams>	m_sharedparams;

	TItemFlags					m_itemFlags;

	QueuedRayID					m_dropRayId;
	Vec3								m_dropPosition;
	Vec3								m_dropImpulse;

	SStats							m_stats;
	SEditorStats				m_editorstats;
	TAccessoryArray			m_accessories;
	std::vector<EntityEffects::TAttachedEffectId>	m_damageLevelEffects;
	
	TInitialSetup				m_initialSetup;

	EntityEffects::CEffectsController	m_effectsController;
	CItemScheduler			m_scheduler;
	COwnerInfo					m_owner;
	EntityId						m_parentId;

	uint32							m_animationTime[eIGS_Last];

	SRespawnProperties	m_respawnprops;
	SEntityProperties		m_properties;
	EntityId						m_hostId;
	EntityId						m_postSerializeMountedOwner;
	SAnimationContext  *m_pItemAnimationContext;
	IActionController	 *m_pItemActionController;
	IActionController	 *m_pCurrentActionController;
	const SMannequinItemParams*	m_pCurrentManItemParams;
	static SMannequinItemParams sNullManItemParams;

	static SItemAnimationEvents s_animationEventsTable;

	static IEntitySystem		*m_pEntitySystem;
	static IItemSystem			*m_pItemSystem;
	static IGameFramework		*m_pGameFramework;
	static IGameplayRecorder*m_pGameplayRecorder;

	_smart_ptr<CItemSelectAction> m_pSelectAction;

	ePhysicalization			m_deferPhysicalization;

	ItemString						m_geometry[eIGS_Last];

	eItemLowerMode							m_itemLowerMode;
	
	Vec3									m_serializeActivePhysics;

	float									m_unholsteringSelectBiasTime;
	int										m_delayedUnhideCntr;

	uint16								m_attachedAccessoryHistory;

	TagID									m_subContext;

public:

	static IEntityClass*	sBinocularsClass;
	static IEntityClass*	sDebugGunClass;
	static IEntityClass*	sRefWeaponClass;
	static IEntityClass*	sLTagGrenade;
	static IEntityClass*	sFragHandGrenadesClass;	
	static IEntityClass*	sNoWeaponClass;
	static IEntityClass*	sWeaponMeleeClass;
	static IEntityClass*	sBowClass;
	static IEntityClass*  sSilencerPistolClass;
	static IEntityClass*  sSilencerClass;

	static SItemFragmentTagCRCs sFragmentTagCRCs;
	static SItemActionParamCRCs  sActionParamCRCs;
};


#endif //__ITEM_H__
