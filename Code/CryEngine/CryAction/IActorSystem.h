// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Actor System interfaces.

   -------------------------------------------------------------------------
   History:
   - 26:8:2004   14:55 : Created by Márcio Martins

*************************************************************************/
#ifndef __IACTORSYSTEM_H__
#define __IACTORSYSTEM_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryScriptSystem/IScriptSystem.h>
#include "IGameObjectSystem.h"
#include "IGameObject.h"

enum EActorPhysicalization
{
	eAP_NotPhysicalized,
	eAP_Alive,
	eAP_Ragdoll,
	eAP_Sleep,
	eAP_Frozen,
	eAP_Linked,
	eAP_Spectator,
};

struct IActor;
struct IActionListener;
struct IAnimationGraphState;
struct SViewParams;
class CGameObject;
struct IGameObjectExtension;
struct IInventory;
struct IAnimatedCharacter;
struct ICharacterInstance;
struct AnimEventInstance;
struct IAIAction;
struct pe_params_rope;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

typedef int ActorClass;

struct IActor : public IGameObjectExtension
{
	virtual ~IActor(){}
	virtual void                  SetHealth(float health) = 0;
	virtual float                 GetHealth() const = 0;
	virtual int                   GetHealthAsRoundedPercentage() const = 0;
	virtual void                  SetMaxHealth(float maxHealth) = 0;
	virtual float                 GetMaxHealth() const = 0;
	virtual int                   GetArmor() const = 0;
	virtual int                   GetMaxArmor() const = 0;
	virtual int                   GetTeamId() const = 0;

	virtual bool                  IsFallen() const = 0;
	virtual bool                  IsDead() const = 0;
	virtual int                   IsGod() = 0;
	virtual void                  Fall(Vec3 hitPos = Vec3(0, 0, 0)) = 0;
	virtual bool                  AllowLandingBob() = 0;

	virtual void                  PlayAction(const char* action, const char* extension, bool looping = false) = 0;
	virtual IAnimationGraphState* GetAnimationGraphState() = 0;
	virtual void                  ResetAnimationState() = 0;

	virtual void                  CreateScriptEvent(const char* event, float value, const char* str = NULL) = 0;
	virtual bool                  BecomeAggressiveToAgent(EntityId entityID) = 0;

	virtual void                  SetFacialAlertnessLevel(int alertness) = 0;
	virtual void                  RequestFacialExpression(const char* pExpressionName = NULL, f32* sequenceLength = NULL) = 0;
	virtual void                  PrecacheFacialExpression(const char* pExpressionName) = 0;

	virtual EntityId              GetGrabbedEntityId() const = 0;

	virtual void                  HideAllAttachments(bool isHiding) = 0;

	virtual void                  SetIKPos(const char* pLimbName, const Vec3& goalPos, int priority) = 0;

	virtual void                  SetViewInVehicle(Quat viewRotation) = 0;
	virtual void                  SetViewRotation(const Quat& rotation) = 0;
	virtual Quat                  GetViewRotation() const = 0;

	virtual bool                  IsFriendlyEntity(EntityId entityId, bool bUsingAIIgnorePlayer = true) const = 0;

	//virtual Vec3 GetViewAngleOffset() = 0;
	virtual Vec3                 GetLocalEyePos() const = 0;

	virtual void                 CameraShake(float angle, float shift, float duration, float frequency, Vec3 pos, int ID, const char* source = "") = 0;

	virtual IItem*               GetHolsteredItem() const = 0;
	virtual void                 HolsterItem(bool holster, bool playSelect = true, float selectSpeedBias = 1.0f, bool hideLeftHandObject = true) = 0;
	//virtual IItem *GetCurrentItem() const = 0;
	virtual IItem*               GetCurrentItem(bool includeVehicle = false) const = 0;
	virtual bool                 DropItem(EntityId itemId, float impulseScale = 1.0f, bool selectNext = true, bool byDeath = false) = 0;
	virtual IInventory*          GetInventory() const = 0;
	virtual void                 NotifyCurrentItemChanged(IItem* newItem) = 0;

	virtual IMovementController* GetMovementController() const = 0;

	// get currently linked vehicle, or NULL
	virtual IEntity* LinkToVehicle(EntityId vehicleId) = 0;

	virtual IEntity* GetLinkedEntity() const = 0;

	virtual uint8    GetSpectatorMode() const = 0;

	virtual bool     IsThirdPerson() const = 0;
	virtual void     ToggleThirdPerson() = 0;

	virtual bool     IsStillWaitingOnServerUseResponse() const        { return false; }
	virtual void     SetStillWaitingOnServerUseResponse(bool waiting) {}

	virtual void     SetFlyMode(uint8 flyMode)                        {};
	virtual uint8    GetFlyMode() const                               { return 0; };

	//virtual void SendRevive(const Vec3& position, const Quat& orientation, int team, bool clearInventory) = 0;

	virtual void                      Release() = 0;

	virtual bool                      IsPlayer() const = 0;
	virtual bool                      IsClient() const = 0;
	virtual bool                      IsMigrating() const = 0;
	virtual void                      SetMigrating(bool isMigrating) = 0;

	virtual void                      InitLocalPlayer() = 0;

	virtual const char*               GetActorClassName() const = 0;
	virtual ActorClass                GetActorClass() const = 0;

	virtual const char*               GetEntityClassName() const = 0;

	virtual void                      SerializeLevelToLevel(TSerialize& ser) = 0;
	virtual void                      ProcessEvent(const SEntityEvent& event) = 0;

	virtual IAnimatedCharacter*       GetAnimatedCharacter() = 0;
	virtual const IAnimatedCharacter* GetAnimatedCharacter() const = 0;
	virtual void                      PlayExactPositioningAnimation(const char* sAnimationName, bool bSignal, const Vec3& vPosition, const Vec3& vDirection, float startWidth, float startArcAngle, float directionTolerance) = 0;
	virtual void                      CancelExactPositioningAnimation() = 0;
	virtual void                      PlayAnimation(const char* sAnimationName, bool bSignal) = 0;

	// Respawn the actor to some intiial data (used by CheckpointSystem)
	virtual bool Respawn()
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Use of IActor::Respawn when not implemented!");
		return false;
	}

	// Reset the actor to its initial location (used by CheckpointSystem)
	virtual void ResetToSpawnLocation()
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Use of IActor::ResetToSpawnLocation when not implemented!");
	}

	// Can the actor currently break through glass (used by ActionGame)
	virtual bool CanBreakGlass() const
	{
		return false;
	}

	// Forces the actor break through glass if allowed (used by ActionGame)
	virtual bool MustBreakGlass() const
	{
		return false;
	}

	// Enables/Disables time demo mode.
	// Some movement/view direction things work differently when running under time demo.
	virtual void EnableTimeDemo(bool bTimeDemo) = 0;

	uint16       GetChannelId() const
	{
		return GetGameObject()->GetChannelId();
	}
	void SetChannelId(uint16 id)
	{
		GetGameObject()->SetChannelId(id);
	}

	virtual void SwitchDemoModeSpectator(bool activate) = 0;

	//virtual void NotifyLeaveFallAndPlay() = 0;
	//virtual void NotifyInventoryAmmoChange(IEntityClass* pAmmoClass, int amount) = 0;

	virtual void SetCustomHead(const char* customHead) {};// = 0;

	// IVehicle
	virtual IVehicle* GetLinkedVehicle() const = 0;

	virtual void      OnAIProxyEnabled(bool enabled) = 0;
	virtual void      OnReturnedToPool() = 0;
	virtual void      OnPreparedFromPool() = 0;
	virtual void      OnShiftWorld()                        {};

	virtual void      MountedGunControllerEnabled(bool val) {};
	virtual bool      MountedGunControllerEnabled() const   { return false; }

	virtual bool      ShouldMuteWeaponSoundStimulus() const = 0;

	// Populates list of physics entities to skip for IK raycasting. returns num Entities to skip
	virtual int  GetPhysicalSkipEntities(IPhysicalEntity** pSkipList, const int maxSkipSize) const { return 0; }

	virtual void OnReused(IEntity* pEntity, SEntitySpawnParams& params) = 0;

	virtual bool IsInteracting() const = 0;
};

struct IActorIterator
{
	virtual ~IActorIterator(){}
	virtual size_t  Count() = 0;
	virtual IActor* Next() = 0;
	virtual void    AddRef() = 0;
	virtual void    Release() = 0;
};
typedef _smart_ptr<IActorIterator> IActorIteratorPtr;

struct IItemParamsNode;

struct IActorSystem
{
	virtual ~IActorSystem(){}
	virtual void                   Reset() = 0;
	virtual void                   Reload() = 0;
	virtual IActor*                GetActor(EntityId entityId) = 0;
	virtual IActor*                GetActorByChannelId(uint16 channelId) = 0;
	virtual IActor*                CreateActor(uint16 channelId, const char* name, const char* actorClass, const Vec3& pos, const Quat& rot, const Vec3& scale, EntityId id = 0) = 0;

	virtual int                    GetActorCount() const = 0;
	virtual IActorIteratorPtr      CreateActorIterator() = 0;

	virtual void                   SetDemoPlaybackMappedOriginalServerPlayer(EntityId id) = 0;
	virtual EntityId               GetDemoPlaybackMappedOriginalServerPlayer() const = 0;
	virtual void                   SwitchDemoSpectator(EntityId id = 0) = 0;
	virtual IActor*                GetCurrentDemoSpectator() = 0;
	virtual IActor*                GetOriginalDemoSpectator() = 0;

	virtual void                   AddActor(EntityId entityId, IActor* pActor) = 0;
	virtual void                   RemoveActor(EntityId entityId) = 0;

	virtual void                   Scan(const char* folderName) = 0;
	virtual bool                   ScanXML(const XmlNodeRef& root, const char* xmlFile) = 0;
	virtual const IItemParamsNode* GetActorParams(const char* actorClass) const = 0;

	virtual bool                   IsActorClass(IEntityClass* pClass) const = 0;
};

#endif //__IACTORSYSTEM_H__
