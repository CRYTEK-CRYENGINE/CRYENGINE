// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RECORDINGSYSTEMPACKETS_H__
#define __RECORDINGSYSTEMPACKETS_H__

#include <CryAnimation/CryCharAnimationParams.h>
#include "Battlechatter.h"
#include "Network/Lobby/SessionNames.h"

#include "ICryMannequin.h"

#define MAX_WEAPON_ACCESSORIES 3
#define RECORDING_SYSTEM_MAX_SLOTS 5

#define ThirdPersonPacketList(f)         \
	f(eTPP_TPChar = eRBPT_Custom)          \
	f(eTPP_SpawnCustomParticle)            \
	f(eTPP_ParticleCreated)                \
	f(eTPP_ParticleDeleted)                \
	f(eTPP_ParticleLocation)               \
	f(eTPP_ParticleTarget)                 \
	f(eTPP_WeaponAccessories)              \
	f(eTPP_WeaponSelect)                   \
	f(eTPP_FiremodeChanged)                \
	f(eTPP_MannEvent)											 \
	f(eTPP_MannSetSlaveController)				 \
	f(eTPP_MannSetParam)									 \
	f(eTPP_MannSetParamFloat)							 \
	f(eTPP_MountedGunAnimation)            \
	f(eTPP_MountedGunRotate)			         \
	f(eTPP_MountedGunEnter)                \
	f(eTPP_MountedGunLeave)                \
	f(eTPP_OnShoot)                        \
	f(eTPP_EntitySpawn)                    \
	f(eTPP_EntityRemoved)                  \
	f(eTPP_EntityLocation)                 \
	f(eTPP_EntityHide)                     \
	f(eTPP_PlaySound)                      \
	f(eTPP_StopSound)                      \
	f(eTPP_SoundParameter)                 \
	f(eTPP_BulletTrail)                    \
	f(eTPP_ProceduralBreakHappened)        \
	f(eTPP_DrawSlotChange)                 \
	f(eTPP_StatObjChange)                  \
	f(eTPP_SubObjHideMask)                 \
	f(eTPP_TeamChange)                     \
	f(eTPP_ItemSwitchHand)                 \
	f(eTPP_EntityAttached)                 \
	f(eTPP_EntityDetached)                 \
	f(eTPP_PlayerJoined)                   \
	f(eTPP_PlayerLeft)                     \
	f(eTPP_PlayerChangedModel)             \
	f(eTPP_CorpseSpawned)                  \
	f(eTPP_CorpseRemoved)                  \
	f(eTPP_AnimObjectUpdated)              \
	f(eTPP_ObjectCloakSync)								 \
	f(eTPP_PickAndThrowUsed)               \
	f(eTPP_ForcedRagdollAndImpulse)        \
	f(eTPP_RagdollImpulse)								 \
	f(eTPP_InteractiveObjectFinishedUse)   \

AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(EThirdPersonPacket, ThirdPersonPacketList, eTPP_Last);

enum EFirstPersonPacket
{
	eFPP_FPChar = eRBPT_Custom,
	eFPP_Flashed,
	eFPP_VictimPosition,
	eFPP_KillHitPosition,
	eFPP_BattleChatter,
	eFPP_RenderNearest,
	eFPP_PlayerHealthEffect,
	eFPP_PlaybackTimeOffset,
	eFPP_Max
};

struct SRecording_FPChar : SRecording_Packet
{
	SRecording_FPChar()
		: camlocation(IDENTITY)
		, relativePosition(IDENTITY)
		, fov(0)
		, frametime(0)
		, playerFlags(0)
	{
		size = sizeof(SRecording_FPChar);
		type = eFPP_FPChar;
	}

	void Serialise(CBufferUtil &buffer);

	QuatT camlocation;
	QuatT relativePosition;
	float fov;
	float frametime;
	uint8 playerFlags;	// EFirstPersonFlags
};

struct SRecording_Flashed : SRecording_Packet
{
	SRecording_Flashed()
		: frametime(0)
		, duration(0)
		, blindAmount(0)
	{
		size = sizeof(SRecording_Flashed);
		type = eFPP_Flashed;
	}

	void Serialise(CBufferUtil &buffer);

	float frametime;
	float duration;
	float blindAmount;
};

struct SRecording_RenderNearest : SRecording_Packet
{
	SRecording_RenderNearest()
		: frametime(0.f)
		, renderNearest(false)
	{
		size = sizeof(SRecording_RenderNearest);
		type = eFPP_RenderNearest;
	}

	void Serialise(CBufferUtil &buffer);

	float frametime;
	bool renderNearest;
};

struct SRecording_VictimPosition : SRecording_Packet
{
	SRecording_VictimPosition()
		: frametime(0)
		, victimPosition(ZERO)
	{
		size = sizeof(SRecording_VictimPosition);
		type = eFPP_VictimPosition;
	}

	void Serialise(CBufferUtil &buffer);

	float frametime;
	Vec3 victimPosition;
};

struct SRecording_KillHitPosition : SRecording_Packet
{
	SRecording_KillHitPosition()
		: hitRelativePos(ZERO)
		, victimId(0)
		, fRemoteKillTime(FLT_MAX)
	{
		size = sizeof(SRecording_KillHitPosition);
		type = eFPP_KillHitPosition;
	}

	void Serialise(CBufferUtil &buffer);

	Vec3			hitRelativePos;
	float			fRemoteKillTime;
	EntityId	victimId;	// Note: the victimId is to be used locally only, once sent across the network it will be meaningless
};

struct SRecording_OnShoot : SRecording_Packet
{
	SRecording_OnShoot()
		: weaponId(0)
	{
		size = sizeof(SRecording_OnShoot);
		type = eTPP_OnShoot;
	}

	EntityId weaponId;
};

struct SRecording_TPChar : SRecording_Packet
{
	SRecording_TPChar()
		: entitylocation(IDENTITY)
		, velocity(ZERO)
		, aimdir(ZERO)
		, eid(0)
		, layerEffectParams(0)
		, playerFlags(0)
		, healthPercentage(100)
	{
		size = sizeof(SRecording_TPChar);
		type = eTPP_TPChar;
	}

	QuatT entitylocation;
	Vec3 velocity;
	Vec3 aimdir;
	EntityId eid;
	uint32 layerEffectParams;	// Used for suit mode effects
	uint16 playerFlags;		// EThirdPersonFlags
	uint8 healthPercentage;
};

struct SRecording_MannEvent : SRecording_Packet
{
	SRecording_MannEvent()
		: eid(0)
	{
		size = sizeof(SRecording_MannEvent);
		type = eTPP_MannEvent;
	}

	EntityId eid;
	SMannHistoryItem historyItem;
};

struct SRecording_MannSetSlaveController : SRecording_Packet
{
	SRecording_MannSetSlaveController():
		masterActorId(0),
		slaveActorId(0),
		targetContext(0),
		enslave(false),
		optionalDatabaseFilenameCRC(0)
	{
		size = sizeof(SRecording_MannSetSlaveController);
		type = eTPP_MannSetSlaveController;
	}

	EntityId masterActorId;
	EntityId slaveActorId;
	uint32 optionalDatabaseFilenameCRC;
	uint8 targetContext;
	bool enslave;
};

struct SRecording_MannSetParam : SRecording_Packet
{
	SRecording_MannSetParam():
		entityId(0),
		paramNameCRC(0),
		quat(IDENTITY)
	{
		size = sizeof(SRecording_MannSetParam);
		type = eTPP_MannSetParam;
	}

	EntityId entityId;
	uint32 paramNameCRC;
	QuatT quat;
};

struct SRecording_MannSetParamFloat : SRecording_Packet
{
	SRecording_MannSetParamFloat()
		: entityId(0)
		, paramNameCRC(0)
		, param(0)
	{
		size = sizeof(SRecording_MannSetParamFloat);
		type = eTPP_MannSetParamFloat;
	}

	EntityId entityId;
	uint32 paramNameCRC;
	float param;
};

struct SRecording_MountedGunAnimation : SRecording_Packet
{
	SRecording_MountedGunAnimation()
		: ownerId(0)
		, aimrad(0)
		, aimUp(1.0f)
		, aimDown(1.0f)
	{
		size = sizeof(SRecording_MountedGunAnimation);
		type = eTPP_MountedGunAnimation;
	}

	EntityId ownerId;
	float aimrad;
	float aimUp;
	float aimDown;
};

struct SRecording_MountedGunRotate : SRecording_Packet
{
	SRecording_MountedGunRotate()
		: gunId(0)
	{
		rotation = Quat::CreateIdentity();
		size = sizeof(SRecording_MountedGunRotate);
		type = eTPP_MountedGunRotate;
	}

	EntityId gunId;
	Quat rotation;
};

struct SRecording_MountedGunEnter : SRecording_Packet
{
	SRecording_MountedGunEnter()
		: ownerId(0)
		, upAnimId(0)
		, downAnimId(0)
	{
		size = sizeof(SRecording_MountedGunEnter);
		type = eTPP_MountedGunEnter;
	}

	EntityId ownerId;
	int upAnimId;
	int downAnimId;
};

struct SRecording_MountedGunLeave : SRecording_Packet
{
	SRecording_MountedGunLeave()
		: ownerId(0)
	{
		size = sizeof(SRecording_MountedGunLeave);
		type = eTPP_MountedGunLeave;
	}

	EntityId ownerId;
};

struct SRecording_WeaponAccessories : SRecording_Packet
{
	SRecording_WeaponAccessories()
		: weaponId(0)
	{
		size = sizeof(SRecording_WeaponAccessories);
		type = eTPP_WeaponAccessories;
		for (int i=0; i<MAX_WEAPON_ACCESSORIES; i++)
		{
			pAccessoryClasses[i] = NULL;
		}
	}

	EntityId weaponId;
	IEntityClass* pAccessoryClasses[MAX_WEAPON_ACCESSORIES];
};

struct SRecording_WeaponSelect : SRecording_Packet
{
	SRecording_WeaponSelect()
		: ownerId(0)
		, weaponId(0)
		, isRippedOff(false)
		, isSelected(false)
	{
		size = sizeof(SRecording_WeaponSelect);
		type = eTPP_WeaponSelect;
	}

	EntityId ownerId;
	EntityId weaponId;
	bool isRippedOff;
	bool isSelected;
};

struct SRecording_FiremodeChanged : SRecording_Packet
{
	SRecording_FiremodeChanged()
		:	ownerId(0)
		,	weaponId(0)
		,	firemode(0)
	{
		size = sizeof(SRecording_FiremodeChanged);
		type = eTPP_FiremodeChanged;
	}

	EntityId ownerId;
	EntityId weaponId;
	int firemode;
};

struct SRecording_SpawnCustomParticle : SRecording_Packet
{
	SRecording_SpawnCustomParticle()
		: pParticleEffect(NULL)
		, location(IDENTITY)
	{
		size = sizeof(SRecording_SpawnCustomParticle);
		type = eTPP_SpawnCustomParticle;
	}

	IParticleEffect *pParticleEffect;
	Matrix34 location;
};

struct SRecording_ParticleCreated : SRecording_Packet
{
	SRecording_ParticleCreated()
		: pParticleEmitter(NULL)
		, pParticleEffect(NULL)
		, location(IDENTITY)
		, entityId(0)
		, entitySlot(0)
	{
		size = sizeof(SRecording_ParticleCreated);
		type = eTPP_ParticleCreated;
	}

	IParticleEmitter *pParticleEmitter;
	IParticleEffect *pParticleEffect;
	QuatTS location;
	EntityId entityId;
	int entitySlot;
};

struct SRecording_ParticleDeleted : SRecording_Packet
{
	SRecording_ParticleDeleted()
		: pParticleEmitter(NULL)
	{
		size = sizeof(SRecording_ParticleDeleted);
		type = eTPP_ParticleDeleted;
	}

	IParticleEmitter *pParticleEmitter;
};

struct SRecording_ParticleLocation : SRecording_Packet
{
	SRecording_ParticleLocation()
		: pParticleEmitter(NULL)
		, location(IDENTITY)
	{
		size = sizeof(SRecording_ParticleLocation);
		type = eTPP_ParticleLocation;
	}

	IParticleEmitter *pParticleEmitter;
	Matrix34 location;
};

struct SRecording_ParticleTarget : SRecording_Packet
{
	SRecording_ParticleTarget()
		: pParticleEmitter(NULL)
		, targetPos(ZERO)
		, velocity(ZERO)
		, radius(0.f)
		, target(false)
		, priority(false)
	{
		size = sizeof(SRecording_ParticleTarget);
		type = eTPP_ParticleTarget;
	}

	IParticleEmitter *pParticleEmitter;
	Vec3 targetPos;
	Vec3 velocity;
	float radius;
	bool target;
	bool priority;

};

struct SRecording_EntitySpawn : SRecording_Packet
{
	SRecording_EntitySpawn()
		: pClass(NULL)
		, entityFlags(0)
		, entitylocation(IDENTITY)
		, entityScale(IDENTITY)
		, entityId(0)
		, pScriptTable(NULL)
		, pMaterial(NULL)
		, hidden(false)
		, useOriginalClass(false)
		, subObjHideMask(0)
	{
		size = sizeof(SRecording_EntitySpawn);
		type = eTPP_EntitySpawn;

		for(int i = 0; i < RECORDING_SYSTEM_MAX_SLOTS; i++)
		{
			szCharacterSlot[i]	= NULL;
			pStatObj[i]					= NULL;
			slotFlags[i]				= 0;
			slotOffset[i]				= ZERO;
		}
	}

	IEntityClass* pClass;
	const char* szCharacterSlot[RECORDING_SYSTEM_MAX_SLOTS];
	IStatObj * pStatObj[RECORDING_SYSTEM_MAX_SLOTS];
	int slotFlags[RECORDING_SYSTEM_MAX_SLOTS];
	Vec3 slotOffset[RECORDING_SYSTEM_MAX_SLOTS];
	uint32 entityFlags;
	QuatT entitylocation;
	Vec3 entityScale;
	EntityId entityId;
	hidemask subObjHideMask;
	IScriptTable * pScriptTable;
	IMaterial* pMaterial;
	bool hidden : 1;
	bool useOriginalClass : 1;
};

struct SRecording_EntityRemoved : SRecording_Packet
{
	SRecording_EntityRemoved()
		: entityId(0)
	{
		size = sizeof(SRecording_EntityRemoved);
		type = eTPP_EntityRemoved;
	}

	EntityId entityId;
};

struct SRecording_EntityLocation : SRecording_Packet
{
	SRecording_EntityLocation()
		: entityId(0)
		, entitylocation(IDENTITY)
	{
		size = sizeof(SRecording_EntityLocation);
		type = eTPP_EntityLocation;
	}

	EntityId entityId;
	QuatT entitylocation;
};

struct SRecording_EntityHide : SRecording_Packet
{
	SRecording_EntityHide()
		: entityId(0)
		, hide(false)
	{
		size = sizeof(SRecording_EntityHide);
		type = eTPP_EntityHide;
	}

	EntityId entityId;
	bool hide;
};

struct SRecording_PlaySound : SRecording_Packet
{
	SRecording_PlaySound()
		: position(ZERO)
		//, soundId(INVALID_SOUNDID)
		, szName(0)
		, flags(0)
		//, semantic(eSoundSemantic_None)
		, entityId(0)
	{
		size = sizeof(SRecording_PlaySound);
		type = eTPP_PlaySound;
	}

	Vec3 position;
	//tSoundID soundId;
	const char* szName;
	uint32 flags;
	//ESoundSemantic semantic;
	EntityId entityId;
};

struct SRecording_StopSound : SRecording_Packet
{
	SRecording_StopSound()
		//: soundId(INVALID_SOUNDID)
	{
		size = sizeof(SRecording_StopSound);
		type = eTPP_StopSound;
	}

	//tSoundID soundId;
};

struct SRecording_SoundParameter : SRecording_Packet
{
	SRecording_SoundParameter()
		: /*soundId(INVALID_SOUNDID)
		, */index(0)
		, value(0.0f)
	{
		size = sizeof(SRecording_SoundParameter);
		type = eTPP_SoundParameter;
	}

	//tSoundID soundId;
	uint8 index;
	float value;
};

struct SRecording_BattleChatter : SRecording_Packet
{
	SRecording_BattleChatter()
		: frametime(0)
		, entityNetId(0)
		, chatterType(BC_Null)
		, chatterVariation(0)
	{
		size = sizeof(SRecording_BattleChatter);
		type = eFPP_BattleChatter;
	}

	void Serialise(CBufferUtil &buffer);

	float frametime;
	uint16 entityNetId;
	uint8 chatterType;
	uint8 chatterVariation;		//0 is no specific variation, 1-255 is variation 0-254.
};

struct SRecording_BulletTrail : SRecording_Packet
{
	SRecording_BulletTrail()
		: start(ZERO)
		, end(ZERO)
		, friendly(false)
	{
		size = sizeof(SRecording_BulletTrail);
		type = eTPP_BulletTrail;
	}

	Vec3 start;
	Vec3 end;
	bool friendly;
};

struct SRecording_ProceduralBreakHappened : SRecording_Packet
{
	SRecording_ProceduralBreakHappened()
		: uBreakEventIndex(0)
	{
		size = sizeof(SRecording_ProceduralBreakHappened);
		type = eTPP_ProceduralBreakHappened;
		static_assert(alignof(SRecording_ProceduralBreakHappened) == 4, "Invalid type alignment!");
	}

	uint16 uBreakEventIndex;
};

struct SRecording_DrawSlotChange : SRecording_Packet
{
	SRecording_DrawSlotChange()
		: entityId(0)
		, iSlot(0)
		, flags(0)
	{
		size = sizeof(SRecording_DrawSlotChange);
		type = eTPP_DrawSlotChange;
	}

	EntityId entityId;
	int iSlot;
	int flags;
};

struct SRecording_StatObjChange : SRecording_Packet
{
	SRecording_StatObjChange()
		: entityId(0)
		,	iSlot(0)
		, pNewStatObj(NULL)
	{
		size = sizeof(SRecording_StatObjChange);
		type = eTPP_StatObjChange;
	}

	EntityId	entityId;
	int32			iSlot;
	IStatObj *pNewStatObj;
};

struct SRecording_SubObjHideMask : SRecording_Packet
{
	SRecording_SubObjHideMask()
		: entityId(0)
		,	slot(0)
		, subObjHideMask(0)
	{
		size = sizeof(SRecording_SubObjHideMask);
		type = eTPP_SubObjHideMask;
	}

	EntityId entityId;
	int32 slot;
	hidemask subObjHideMask;
};

struct SRecording_TeamChange : SRecording_Packet
{
	SRecording_TeamChange()
		: entityId(0)
		,	teamId(0)
		, isFriendly(false)
	{
		size = sizeof(SRecording_TeamChange);
		type = eTPP_TeamChange;
	}

	EntityId entityId;
	int8 teamId;
	bool isFriendly;
};

struct SRecording_ItemSwitchHand : SRecording_Packet
{
	SRecording_ItemSwitchHand()
		: entityId(0)
		,	hand(0)
	{
		size = sizeof(SRecording_ItemSwitchHand);
		type = eTPP_ItemSwitchHand;
	}

	EntityId entityId;
	int hand;
};

struct SRecording_EntityAttached : SRecording_Packet
{
	SRecording_EntityAttached()
		: entityId(0)
		, actorId(0)
	{
		size = sizeof(SRecording_EntityAttached);
		type = eTPP_EntityAttached;
	}

	EntityId entityId;
	EntityId actorId;
};

struct SRecording_EntityDetached : SRecording_Packet
{
	SRecording_EntityDetached()
		: entityId(0)
		, actorId(0)
	{
		size = sizeof(SRecording_EntityDetached);
		type = eTPP_EntityDetached;
	}

	EntityId entityId;
	EntityId actorId;
};

struct SRecording_PlayerJoined : SRecording_Packet
{
	SRecording_PlayerJoined()
		: playerId(0)
		, pControllerDef(NULL)
		, pAnimDB1P(NULL)
		, pAnimDB3P(NULL)
		, reincarnations(0)
		, rank(0)
		, bIsClient(false)
		, bSameSquad(false)
		, bIsPlayer(false)
	{
		size = sizeof(SRecording_PlayerJoined);
		type = eTPP_PlayerJoined;
		displayName[0] = '\0';
	}

	EntityId playerId;
	char displayName[DISPLAY_NAME_LENGTH];
	const char *pControllerDef;
	const char *pAnimDB1P;
	const char *pAnimDB3P;
	uint8 reincarnations;
	uint8 rank;
	bool bIsClient;			// Is a local client actor
	bool bSameSquad;
	bool bIsPlayer;
};

struct SRecording_PlayerLeft : SRecording_Packet
{
	SRecording_PlayerLeft()
		: playerId(0)
	{
		size = sizeof(SRecording_PlayerLeft);
		type = eTPP_PlayerLeft;
	}

	EntityId playerId;
};

struct SRecording_PlayerChangedModel : SRecording_Packet
{
	SRecording_PlayerChangedModel()
		: playerId(0)
		, pModelName(NULL)
		, pShadowName(NULL)
	{
		size = sizeof(SRecording_PlayerChangedModel);
		type = eTPP_PlayerChangedModel;
	}

	EntityId playerId;
	const char *pModelName;
	const char *pShadowName;
};

struct SRecording_CorpseSpawned : SRecording_Packet
{
	SRecording_CorpseSpawned()
		: corpseId(0)
		, playerId(0)
	{
		size = sizeof(SRecording_CorpseSpawned);
		type = eTPP_CorpseSpawned;
	}

	EntityId corpseId;
	EntityId playerId;
};

struct SRecording_CorpseRemoved : SRecording_Packet
{
	SRecording_CorpseRemoved()
		: corpseId(0)
	{
		size = sizeof(SRecording_CorpseRemoved);
		type = eTPP_CorpseRemoved;
	}

	EntityId corpseId;
};

struct SRecording_AnimObjectUpdated : SRecording_Packet
{
	SRecording_AnimObjectUpdated()
		: entityId(0)
		, fTime(0.f)
		, animId(-1)
	{
		size = sizeof(SRecording_AnimObjectUpdated);
		type = eTPP_AnimObjectUpdated;
	}

	EntityId entityId;
	float fTime;
	int16 animId;
};

struct SRecording_ObjectCloakSync : SRecording_Packet
{
	SRecording_ObjectCloakSync():
		cloakObjectId(0),
		cloakPlayerId(0),
		fadeToDesiredCloakTarget(true),
		cloakSlave(false)

	{
		size = sizeof(SRecording_ObjectCloakSync);
		type = eTPP_ObjectCloakSync;
	}

	void Reset()
	{
		cloakObjectId  = 0; 
		fadeToDesiredCloakTarget = true;
		cloakSlave = false; 
	}

	EntityId cloakPlayerId;
	EntityId cloakObjectId; 
	bool fadeToDesiredCloakTarget;
	bool cloakSlave; 
};

struct SRecording_PickAndThrowUsed : SRecording_Packet
{
	SRecording_PickAndThrowUsed()
		: playerId(0)
		, environmentEntityId(0)
		, pickAndThrowId(0)
		, bPickedUp(false)
	{
		size = sizeof(SRecording_PickAndThrowUsed);
		type = eTPP_PickAndThrowUsed;
	}

	EntityId playerId;
	EntityId environmentEntityId;
	EntityId pickAndThrowId;
	bool bPickedUp;
};

struct SRecording_ForcedRagdollAndImpulse : SRecording_Packet
{
	SRecording_ForcedRagdollAndImpulse()
		: playerEntityId(0)
		, vecImpulse(ZERO)
		, pos(ZERO)
		, partid(0)
		, iApplyTime(0)
	{
		size = sizeof(SRecording_ForcedRagdollAndImpulse);
		type = eTPP_ForcedRagdollAndImpulse;
	}

	EntityId playerEntityId;
	Vec3 vecImpulse;
	Vec3 pos; 
	int partid;	
	int iApplyTime; 

};

struct SRecording_RagdollImpulse : SRecording_Packet
{
	SRecording_RagdollImpulse()
		: playerId(0)
		, impulse(ZERO)
		, point(ZERO)
		, partid(0)
	{
		size = sizeof(SRecording_RagdollImpulse);
		type = eTPP_RagdollImpulse;
	}

	EntityId playerId;
	Vec3 impulse;
	Vec3 point;
	int partid;
};

struct SRecording_PlayerHealthEffect : SRecording_Packet
{
	SRecording_PlayerHealthEffect()
		: frametime(0)
		, hitStrength(0.0f)
		, hitSpeed(0.0f)
	{
		size = sizeof(SRecording_PlayerHealthEffect);
		type = eFPP_PlayerHealthEffect;
	}

	void Serialise(CBufferUtil &buffer);

	Vec3	hitDirection;
	float frametime;
	float	hitStrength;
	float	hitSpeed;
};

struct SRecording_PlaybackTimeOffset : SRecording_Packet
{
	SRecording_PlaybackTimeOffset()
		: timeOffset(0)
	{
		size = sizeof(SRecording_PlaybackTimeOffset);
		type = eFPP_PlaybackTimeOffset;
	}

	void Serialise(CBufferUtil &buffer);

	float timeOffset;
};

struct SRecording_InteractiveObjectFinishedUse : SRecording_Packet
{
	SRecording_InteractiveObjectFinishedUse()
		: objectId(0)
		, interactionTypeTag(TAG_ID_INVALID)
		, interactionIndex(0)
	{
		size = sizeof(SRecording_InteractiveObjectFinishedUse);
		type = eTPP_InteractiveObjectFinishedUse;
	}

	EntityId objectId;
	TagID interactionTypeTag;
	uint8 interactionIndex;
};

//////////////////////////////////////////////////////////////////////////

struct SPlayerInitialState
{
	static const uint32 MAX_FRAGMENT_TRIGGERS = 16;

	SPlayerInitialState()
	{
		Reset();
	}

	void Reset()
	{
		playerId = 0;
		animationStartTime = 0;
		upperAnimationStartTime = 0;
		weaponAnimationStartTime = 0;
		// Set the entityIds to 0 to indicate the state has not been set
		weapon.ownerId = 0;
		firemode.ownerId = 0;
		mountedGunEnter.ownerId = 0;
		changedModel.playerId = 0;
		teamChange.entityId = 0;
		playerJoined.playerId = 0;
		pickAndThrowUsed.playerId = 0;
		objCloakParams.Reset();
	}

	EntityId playerId;
	float animationStartTime;
	float upperAnimationStartTime;
	float weaponAnimationStartTime;
	SRecording_WeaponSelect weapon;
	SRecording_FiremodeChanged firemode;
	SRecording_MountedGunEnter mountedGunEnter;
	SRecording_PlayerChangedModel changedModel;
	SRecording_TeamChange teamChange;
	SRecording_PlayerJoined playerJoined;
	SRecording_PickAndThrowUsed pickAndThrowUsed;
	SRecording_ObjectCloakSync objCloakParams; 
	SRecording_MannEvent tagSetting;
	SRecording_MannEvent fragmentTriggers[MAX_FRAGMENT_TRIGGERS];
	SRecording_MannSetSlaveController mannSetSlaveController;
};
//////////////////////////////////////////////////////////////////////////

typedef std::map<EntityId, std::pair<SRecording_EntitySpawn, float> > TEntitySpawnMap;
typedef std::map<EntityId, SRecording_WeaponAccessories> TWeaponAccessoryMap;
typedef std::map<IParticleEmitter*, std::pair<SRecording_ParticleCreated, float> > TParticleCreatedMap;
//typedef std::map<tSoundID, std::pair<SRecording_PlaySound, float> > TPlaySoundMap;
typedef std::vector<SRecording_EntityAttached> TEntityAttachedVec;


#endif // __RECORDINGSYSTEMPACKETS_H__
