// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 20:10:2004   10:30 : Created by Craig Tiller

*************************************************************************/
#ifndef __IGAMERULES_H__
#define __IGAMERULES_H__

#pragma once

#include "IGameObject.h"
#include <CryParticleSystem/IParticles.h>

// Summary
//   Types for the different kind of messages.
enum ETextMessageType
{
	eTextMessageCenter = 0,
	eTextMessageConsole, // Console message.
	eTextMessageError,   // Error message.
	eTextMessageInfo,    // Info message.
	eTextMessageServer,
	eTextMessageAnnouncement
};

// Summary
//   Types for the different kind of chat messages.
enum EChatMessageType
{
	eChatToTarget = 0, // The message is to be sent to the target entity.
	eChatToTeam,       // The message is to be sent to the team of the sender.
	eChatToAll         // The message is to be sent to all client connected.
};

// Summary
//	Friendly fire types for explosions
enum EFriendyFireType
{
	eFriendyFireNone = 0,
	eFriendyFireSelf,   //doesn't hurt shooter
	eFriendyFireTeam,   //doesn't hurt shooter or shooter's teammates
};

// Summary
//	Types of resources to cache
enum EGameResourceType
{
	eGameResourceType_Loadout = 0,
	eGameResourceType_Item,
};

// Summary
//  Structure containing name of hit and associated flags.
struct HitTypeInfo
{
	HitTypeInfo()
		: m_flags(0) {}
	HitTypeInfo(const char* pName, int flags = 0)
		: m_name(pName)
		, m_flags(flags) {}

	string m_name;
	int    m_flags;
};

// Summary
//   Structure to describe an hit
// Description
//   This structure is used with the GameRules to create an hit.
// See Also
//   ExplosionInfo
struct HitInfo
{
	EntityId shooterId;    // EntityId of the shooter
	EntityId targetId;     // EntityId of the target which got shot
	EntityId weaponId;     // EntityId of the weapon
	EntityId projectileId; // 0 if hit was not caused by a projectile

	float    damage;       // damage count of the hit
	float    impulseScale; // If this is non zero, this impulse will be applied to the partId set below.
	float    radius;       // radius of the hit
	float    angle;
	int      material;   // material id of the surface which got hit
	int      type;       // type id of the hit, see IGameRules::GetHitTypeId for more information
	int      bulletType; //type of bullet, if hit was of type bullet

	float    damageMin;

	int      partId;

	Vec3     pos; // position of the hit
	Vec3     dir; // direction of the hit
	Vec3     normal;

	uint16   projectileClassId;
	uint16   weaponClassId;
#if USE_LAGOMETER
	uint8    lagOMeterHitId;
#endif

	bool remote;
	bool aimed;          // set to true if shot was aimed - i.e. first bullet, zoomed in etc.
	bool knocksDown;     // true if the hit should knockdown
	bool knocksDownLeg;  // true if the hit should knockdown when hit in a leg
	bool hitViaProxy;    // true if the 'shooter' didn't actually shoot, ie. a weapon acting on their behalf did (team perks)
	bool explosion;      // true if this hit directly results from an explosion
	bool forceLocalKill; // forces the hit to kill the victim + start hitdeathreaction/ragdoll NOT NET SERIALISED

	int  penetrationCount;    // number of surfaces the bullet has penetrated

	HitInfo()
		: shooterId(0),
		targetId(0),
		weaponId(0),
		projectileId(0),
		damage(0),
		impulseScale(0),
		damageMin(0),
		radius(0),
		angle(0),
		material(-1),
		partId(-1),
		type(0),
		pos(ZERO),
		dir(FORWARD_DIRECTION),
		normal(FORWARD_DIRECTION),
#if USE_LAGOMETER
		lagOMeterHitId(0),
#endif
		remote(false),
		bulletType(-1),
		aimed(false),
		knocksDown(false),
		knocksDownLeg(false),
		hitViaProxy(false),
		explosion(false),
		projectileClassId(~uint16(0)),
		weaponClassId(~uint16(0)),
		penetrationCount(0),
		forceLocalKill(false)
	{}

	HitInfo(EntityId shtId, EntityId trgId, EntityId wpnId, float dmg, float rd, int mat, int part, int typ)
		: shooterId(shtId),
		targetId(trgId),
		weaponId(wpnId),
		projectileId(0),
		damage(dmg),
		impulseScale(0),
		radius(rd),
		angle(0),
		material(mat),
		partId(part),
		type(typ),
		pos(ZERO),
		dir(FORWARD_DIRECTION),
		normal(FORWARD_DIRECTION),
#if USE_LAGOMETER
		lagOMeterHitId(0),
#endif
		remote(false),
		bulletType(-1),
		aimed(false),
		damageMin(0),
		knocksDown(false),
		knocksDownLeg(false),
		hitViaProxy(false),
		explosion(false),
		projectileClassId(~uint16(0)),
		weaponClassId(~uint16(0)),
		penetrationCount(0),
		forceLocalKill(false)
	{}

	HitInfo(EntityId shtId, EntityId trgId, EntityId wpnId, float dmg, float rd, int mat, int part, int typ, const Vec3& p, const Vec3& d, const Vec3& n)
		: shooterId(shtId),
		targetId(trgId),
		weaponId(wpnId),
		projectileId(0),
		damage(dmg),
		impulseScale(0),
		radius(rd),
		angle(0),
		material(mat),
		partId(part),
		type(typ),
		pos(p),
		dir(d),
		normal(n),
#if USE_LAGOMETER
		lagOMeterHitId(0),
#endif
		remote(false),
		bulletType(-1),
		aimed(false),
		damageMin(0),
		knocksDown(false),
		knocksDownLeg(false),
		hitViaProxy(false),
		explosion(false),
		projectileClassId(~uint16(0)),
		weaponClassId(~uint16(0)),
		penetrationCount(0),
		forceLocalKill(false)
	{}

	// not all fields are serialized because this is used only for sending data over the net
	void SerializeWith(TSerialize ser)
	{
		ser.Value("shooterId", shooterId, 'eid');
		ser.Value("targetId", targetId, 'eid');
		ser.Value("weaponId", weaponId, 'eid');
		ser.Value("projectileId", projectileId, 'ui32'); //'eid');
		ser.Value("damage", damage, 'dmg');
		ser.Value("radius", radius, 'hRad');
		ser.Value("material", material, 'mat');
#ifndef _RELEASE
		if (IsPartIDInvalid())
		{
			CryFatalError("HitInfo::SerializeWith - partId %i is out of range.", partId);
		}
#endif
		ser.Value("partId", partId, 'part');
		ser.Value("type", type, 'hTyp');
		ser.Value("pos", pos, 'wrld');
		ser.Value("dir", dir, 'dir1');
		ser.Value("normal", normal, 'dir1');
#if USE_LAGOMETER
		ser.Value("lagOMeterHitId", lagOMeterHitId, 'ui4');
#endif
		ser.Value("bulletType", bulletType);
		//ser.Value("aimed", aimed);
		ser.Value("damageMin", damageMin);
		ser.Value("projectileClass", projectileClassId, 'ui16');
		ser.Value("weaponClass", weaponClassId, 'ui16');
		ser.Value("penetrationCount", penetrationCount, 'ui8');
		ser.Value("impulseScale", impulseScale, 'impS');
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

#ifndef _RELEASE
	bool IsPartIDInvalid();
#endif
};

// Summary
//   Structure to describe an explosion
// Description
//   This structure is used with the GameRules to create an explosion.
// See Also
//   HitInfo
struct ExplosionInfo
{
	EntityId         shooterId; // Id of the shooter who triggered the explosion
	EntityId         weaponId;  // Id of the weapon used to create the explosion
	EntityId         projectileId;//Id of the "bullet" (grenade, claymore...) to create the explosion
	uint16           projectileClassId;

	float            damage; // damage created by the explosion

	Vec3             pos;       // position of the explosion
	Vec3             dir;       // direction of the explosion
	float            minRadius; // min radius of the explosion
	float            radius;    // max radius of the explosion
	float            soundRadius;
	float            minPhysRadius;
	float            physRadius;
	float            angle;
	float            pressure; // pressure created by the explosion
	float            hole_size;
	IParticleEffect* pParticleEffect;
	string           effect_name; // this is needed because we don't load particle effects on the dedicated server,
	                              // so we need to store the name if we are going to send it
	string           effect_class;
	float            effect_scale;
	int              type; // type id of the hit, see IGameRules::GetHitTypeId for more information

	bool             impact;
	bool             propogate;
	bool             explosionViaProxy; // true if the 'shooter' didn't actually shoot, ie. a weapon acting on their behalf did (team perks)

	Vec3             impact_normal;
	Vec3             impact_velocity;
	EntityId         impact_targetId;
	float            maxblurdistance;
	int              friendlyfire;

	//Flashbang params
	float blindAmount;
	float flashbangScale;

	int   firstPassPhysicsEntities; // Specify which physics types to hit on the first pass. defaults to 'ent_living', set to zero to disable

	ExplosionInfo()
		: shooterId(0),
		weaponId(0),
		projectileId(0),
		projectileClassId(~uint16(0)),
		damage(0),
		pos(ZERO),
		dir(FORWARD_DIRECTION),
		minRadius(2.5f),
		radius(5.0f),
		soundRadius(0.0f),
		minPhysRadius(2.5f),
		physRadius(5.0f),
		angle(0.0f),
		pressure(200.0f),
		hole_size(5.0f),
		pParticleEffect(0),
		type(0),
		effect_scale(1.0f),
		maxblurdistance(0.0f),
		friendlyfire(eFriendyFireNone),
		impact(false),
		impact_normal(FORWARD_DIRECTION),
		impact_velocity(ZERO),
		impact_targetId(0),
		blindAmount(0.0f),
		flashbangScale(1.0f),
		propogate(true),
		firstPassPhysicsEntities(ent_living),
		explosionViaProxy(false)
	{
	}

	ExplosionInfo(EntityId shtId, EntityId wpnId, EntityId prjId, float dmg, const Vec3& p, const Vec3& d, float minR, float r,
	              float minPhysR, float physR, float a, float press, float holesize, int typ)
		: shooterId(shtId),
		weaponId(wpnId),
		projectileId(prjId),
		projectileClassId(~uint16(0)),
		damage(dmg),
		pos(p),
		dir(d),
		minRadius(minR),
		radius(r),
		soundRadius(0.0f),
		minPhysRadius(minPhysR),
		physRadius(physR),
		angle(a),
		pressure(press),
		hole_size(holesize),
		pParticleEffect(0),
		type(typ),
		effect_scale(1.0f),
		maxblurdistance(0.0f),
		friendlyfire(eFriendyFireNone),
		impact(false),
		impact_normal(FORWARD_DIRECTION),
		impact_velocity(ZERO),
		impact_targetId(0),
		blindAmount(0.0f),
		flashbangScale(1.0f),
		propogate(true),
		firstPassPhysicsEntities(ent_living),
		explosionViaProxy(false)
	{

	}

	void SetImpact(const Vec3& normal, const Vec3& velocity, EntityId targetId)
	{
		impact = true;
		impact_normal = normal;
		impact_velocity = velocity;
		impact_targetId = targetId;
	}

	void SetEffect(const char* effectName, float scale, float maxblurdistance, float blindAmount = 0.0f, float flashbangScale = 1.0f)
	{
		effect_name = effectName;
		effect_scale = scale;
		if (!effect_name.empty())
			pParticleEffect = gEnv->pParticleManager->FindEffect(effect_name.c_str());
		this->maxblurdistance = maxblurdistance;
		this->blindAmount = blindAmount;
		this->flashbangScale = flashbangScale;
	}

	void SetEffectName(const char* effectName)
	{
		this->effect_name = effectName;
	}

	void SetEffectClass(const char* cls)
	{
		effect_class = cls;
	}

	void SetFriendlyFire(EFriendyFireType friendlyFireType)
	{
		friendlyfire = friendlyFireType;
	}

	void SerializeWith(TSerialize ser)
	{
		const bool bNetwork = (ser.GetSerializationTarget() == eST_Network);
		ser.Value("shooterId", shooterId, 'eid');
		ser.Value("weaponId", weaponId, 'eid');
		ser.Value("projectileId", projectileId, 'eid');
		ser.Value("damage", damage, 'dmg');
		ser.Value("pos", pos, 'wrld');
		ser.Value("dir", dir, 'dir1');
		ser.Value("minRadius", minRadius, 'hRad');
		ser.Value("radius", radius, 'hRad');
		ser.Value("soundRadius", soundRadius, 'hRad');
		ser.Value("minPhysRadius", minPhysRadius, 'hRad');
		ser.Value("physRadius", physRadius, 'hRad');

		if (!bNetwork)
		{
			ser.Value("angle", angle, 'hAng');
		}
		else
		{
			CRY_ASSERT_MESSAGE(angle == 0.0f, "Non zero explosion angles are not supported in multiplayer");
		}

		ser.Value("pressure", pressure, 'hPrs');
		ser.Value("hole_size", hole_size, 'hHSz');
		ser.Value("type", type, 'hTyp');

		ser.Value("effect_class", effect_class);
		ser.Value("friendlyfire", friendlyfire, 'spec');

		//UK R&D working on CRCs / indicies for effects & materials
		if (ser.BeginOptionalGroup("effect", !effect_name.empty()))
		{
			if (ser.IsWriting())
			{
				ser.Value("effect_name", effect_name);
			}
			else
			{
				ser.Value("effect_name", effect_name);
				pParticleEffect = gEnv->pParticleManager->FindEffect(effect_name.c_str());
			}
			ser.Value("effect_scale", effect_scale, 'hESc');
			ser.Value("maxblurdistance", maxblurdistance, 'iii');
			ser.EndGroup();
		}

		if (ser.BeginOptionalGroup("flashbang", blindAmount != 0.0f))
		{
			ser.Value("blindAmount", blindAmount, 'hESc');
			ser.Value("flashbangScale", flashbangScale, 'hESc');
			ser.EndGroup();
		}

		if (ser.BeginOptionalGroup("impact", impact))
		{
			if (ser.IsReading())
				impact = true;
			ser.Value("impact_normal", impact_normal, 'dir1');
			ser.Value("impact_velocity", impact_velocity, 'pPVl');
			ser.Value("impact_targetId", impact_targetId, 'eid');
			ser.EndGroup();
		}
	};
};

// Summary
//   Interface to implement an hit listener
// See Also
//   IGameRules, IGameRules::AddHitListener, IGameRules::RemoveHitListener
struct IHitListener
{
	virtual ~IHitListener(){}
	// Summary
	//   Function called when the GameRules process an hit
	// See Also
	//   IGameRules
	virtual void OnHit(const HitInfo&) = 0;

	// Summary
	//   Function called when the GameRules process an explosion (client side)
	// See Also
	//   IGameRules
	virtual void OnExplosion(const ExplosionInfo&) = 0;

	// Summary
	//	Function called when the GameRules process an explosion (server side)
	// See Also
	//	IGameRules
	virtual void OnServerExplosion(const ExplosionInfo&) = 0;
};

// Summary
//   Interface used to implement the game rules
struct IGameRules : public IGameObjectExtension
{
	struct SGameCollision
	{
		const EventPhysCollision* pCollision;
		IGameObject*              pSrc;
		IGameObject*              pTrg;
		IEntity*                  pSrcEntity;
		IEntity*                  pTrgEntity;
	};
	// Summary
	//   Returns wether the disconnecting client should be kept for a few more moments or not.
	virtual bool ShouldKeepClient(int channelId, EDisconnectionCause cause, const char* desc) const = 0;

	// Summary
	//   Called after level loading, to precache anything needed.
	virtual void PrecacheLevel() = 0;

	// Summary
	//   Called from different subsystem to pre-cache needed game resources for the level
	virtual void PrecacheLevelResource(const char* resourceName, EGameResourceType resourceType) = 0;

	// Summary
	//	 Checks to see whether the xml node ref exists in the precache map, keyed by filename. If it does, it returns it. If it doesn't, it returns a NULL ref
	virtual XmlNodeRef FindPrecachedXmlFile(const char* sFilename) = 0;

	// client notification
	virtual void OnDisconnect(EDisconnectionCause cause, const char* desc) = 0; // notification to the client that he has been disconnected

	// Summary
	//   Notifies the server when a client is connecting
	virtual bool OnClientConnect(int channelId, bool isReset) = 0;

	// Summary
	//   Notifies the server when a client is disconnecting
	virtual void OnClientDisconnect(int channelId, EDisconnectionCause cause, const char* desc, bool keepClient) = 0;

	// Summary
	//   Notifies the server when a client has entered the current game
	virtual bool OnClientEnteredGame(int channelId, bool isReset) = 0;

	// Summary
	//   Notifies when an entity has spawn
	virtual void OnEntitySpawn(IEntity* pEntity) = 0;

	// Summary
	//   Notifies when an entity has been removed
	virtual void OnEntityRemoved(IEntity* pEntity) = 0;

	// Summary
	//   Notifies when an entity has been reused
	virtual void OnEntityReused(IEntity* pEntity, SEntitySpawnParams& params, EntityId prevId) = 0;

	// Summary
	//   Broadcasts a message to the clients in the game
	// Parameters
	//   type - indicated the message type
	//   msg - the message to be sent
	virtual void SendTextMessage(ETextMessageType type, const char* msg, uint32 to = eRMI_ToAllClients, int channelId = -1,
	                             const char* p0 = 0, const char* p1 = 0, const char* p2 = 0, const char* p3 = 0) = 0;

	// Summary
	//   Broadcasts a chat message to the clients in the game which are part of the target
	// Parameters
	//   type - indicated the message type
	//   sourceId - EntityId of the sender of this message
	//   targetId - EntityId of the target, used in case type is set to eChatToTarget
	//   msg - the message to be sent
	virtual void SendChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId, const char* msg) = 0;

	// Summary
	//   Performs client tasks needed for an hit
	// Parameters
	//   hitInfo - structure holding all the information about the hit
	// See Also
	//   ServerHit, ServerSimpleHit
	virtual void ClientHit(const HitInfo& hitInfo) = 0;

	// Summary
	//   Performs server tasks needed for an hit
	// Parameters
	//   hitInfo - structure holding all the information about the hit
	// See Also
	//   ClientHit, ServerSimpleHit
	virtual void ServerHit(const HitInfo& hitInfo) = 0;

	// Summary
	//   Returns the Id of an HitType from its name
	// Parameters
	//   type - name of the HitType
	// Returns
	//   Id of the HitType
	// See Also
	//   HitInfo, GetHitType
	virtual int GetHitTypeId(const uint32 crc) const = 0;
	virtual int GetHitTypeId(const char* type) const = 0;

	// Summary
	//   Returns the name of an HitType from its id
	// Parameters
	//   id - Id of the HitType
	// Returns
	//   Name of the HitType
	// See Also
	//   HitInfo, GetHitTypeId
	virtual const char* GetHitType(int id) const = 0;

	// Summary
	//   Notifies that a vehicle got destroyed
	virtual void OnVehicleDestroyed(EntityId id) = 0;

	// Summary
	//   Notifies that a vehicle got submerged
	virtual void OnVehicleSubmerged(EntityId id, float ratio) = 0;

	// Summary
	//   Check if a player should be able to enter a vehicle
	// Parameters
	//   playerId - Id of the player attempting to enter a vehicle
	virtual bool CanEnterVehicle(EntityId playerId) = 0;

	// Summary
	//   Prepares an entity to be allowed to respawn
	// Parameters
	//   entityId - Id of the entity which needs to respawn
	// See Also
	//   HasEntityRespawnData, ScheduleEntityRespawn, AbortEntityRespawn
	virtual void CreateEntityRespawnData(EntityId entityId) = 0;

	// Summary
	//   Indicates if an entity has respawn data
	// Description
	//   Respawn data can be created used the function CreateEntityRespawnData.
	// Parameters
	//   entityId - Id of the entity which needs to respawn
	// See Also
	//   CreateEntityRespawnData, ScheduleEntityRespawn, AbortEntityRespawn
	virtual bool HasEntityRespawnData(EntityId entityId) const = 0;

	// Summary
	//   Schedules an entity to respawn
	// Parameters
	//   entityId - Id of the entity which needs to respawn
	//   unique - if set to true, this will make sure the entity isn't spawn a
	//            second time in case it currently exist
	//   timer - time in second until the entity is respawned
	// See Also
	//   CreateEntityRespawnData, HasEntityRespawnData, AbortEntityRespawn
	virtual void ScheduleEntityRespawn(EntityId entityId, bool unique, float timer) = 0;

	// Summary
	//   Aborts a scheduled respawn of an entity
	// Parameters
	//   entityId - Id of the entity which needed to respawn
	//   destroyData - will force the respawn data set by CreateEntityRespawnData
	//                 to be removed
	// See Also
	//   CreateEntityRespawnData, CreateEntityRespawnData, ScheduleEntityRespawn
	virtual void AbortEntityRespawn(EntityId entityId, bool destroyData) = 0;

	// Summary
	//   Schedules an entity to be removed from the level
	// Parameters
	//   entityId - EntityId of the entity to be removed
	//   timer - time in seconds until which the entity should be removed
	//   visibility - performs a visibility check before removing the entity
	// Remarks
	//   If the visibility check has been requested, the timer will be restarted
	//   in case the entity is visible at the time the entity should have been
	//   removed.
	// See Also
	//   AbortEntityRemoval
	virtual void ScheduleEntityRemoval(EntityId entityId, float timer, bool visibility) = 0;

	// Summary
	//   Cancel the scheduled removal of an entity
	// Parameters
	//   entityId - EntityId of the entity which was scheduled to be removed
	virtual void AbortEntityRemoval(EntityId entityId) = 0;

	// Summary
	//   Registers a listener which will be called every time for every hit.
	// Parameters
	//   pHitListener - a pointer to the hit listener
	// See Also
	//   RemoveHitListener, IHitListener
	virtual void AddHitListener(IHitListener* pHitListener) = 0;

	// Summary
	//   Removes a registered hit listener
	// Parameters
	//   pHitListener - a pointer to the hit listener
	// See Also
	//   AddHitListener
	virtual void RemoveHitListener(IHitListener* pHitListener) = 0;

	// Summary
	//		Gets called when two entities collide, gamerules should dispatch this
	//		call also to Script functions
	// Parameters
	//  pEvent - physics event containing the necessary info
	virtual bool OnCollision(const SGameCollision& event) = 0;

	// Summary
	//		Gets called when two entities collide, and determines if AI should receive stiulus
	// Parameters
	//  pEvent - physics event containing the necessary info
	virtual void OnCollision_NotifyAI(const EventPhys* pEvent) = 0;

	// allows gamerules to extend the 'status' command
	virtual void ShowStatus() = 0;

	// Summary
	//    Checks if game time is limited
	virtual bool IsTimeLimited() const = 0;

	// Summary
	//    Gets remaining game time
	virtual float GetRemainingGameTime() const = 0;

	// Summary
	//    Sets remaining game time
	virtual void SetRemainingGameTime(float seconds) = 0;

	// Clear all migrating players' details
	virtual void ClearAllMigratingPlayers(void) = 0;

	// Summary
	// Set the game channel for a migrating player name, returns the entityId for that player
	virtual EntityId SetChannelForMigratingPlayer(const char* name, uint16 channelID) = 0;

	// Summary
	// Store the details of a migrating player
	virtual void StoreMigratingPlayer(IActor* pActor) = 0;

	// Summary
	// Get the name of the team with the specified ID, or NULL if no team with that ID is known
	virtual const char* GetTeamName(int teamId) const = 0;
};

// Summary:
//   Vehicle System interface
// Description:
//   Interface used to implement the vehicle system.
struct IGameRulesSystem
{
	virtual ~IGameRulesSystem(){}
	// Summary
	//   Registers a new GameRules
	// Parameters
	//   pRulesName - The name of the GameRules, which should also be the name of the GameRules script
	//   pExtensionName - The name of the IGameRules implementation which should be used by the GameRules
	//   bUseScript - Whether or not to load and use a Lua script
	// Returns
	//   The value true will be returned if the GameRules could have been registered.
	virtual bool RegisterGameRules(const char* pRulesName, const char* pExtensionName, bool bUseScript = true) = 0;

	// Summary
	//   Creates a new instance for a GameRules
	// Description
	//   The EntitySystem will be requested to spawn a new entity representing the GameRules.
	// Parameters
	//   pRulesName - Name of the GameRules
	// Returns
	//   If it succeeds, true will be returned.
	virtual bool CreateGameRules(const char* pRulesName) = 0;

	// Summary
	//   Removes the currently active game from memory
	// Descriptions
	//   This function will request the EntitySystem to release the GameRules
	//   entity. Additionally, the global g_gameRules script table will be freed.
	// Returns
	//   The value true will be returned upon completion.
	virtual bool DestroyGameRules() = 0;

	// Summary
	//   Adds an alias name for the specified game rules
	virtual void AddGameRulesAlias(const char* gamerules, const char* alias) = 0;

	// Summary
	//   Adds a default level location for the specified game rules. Level system will look up levels here.
	virtual void AddGameRulesLevelLocation(const char* gamerules, const char* mapLocation) = 0;

	// Summary
	//	 Returns the ith map location for the specified game rules
	virtual const char* GetGameRulesLevelLocation(const char* gamerules, int i) = 0;

	// Sumarry
	//	 Returns the correct gamerules name from an alias
	virtual const char* GetGameRulesName(const char* alias) const = 0;

	// Summary
	//   Determines if the specified GameRules has been registered
	// Parameters
	//   pRulesName - Name of the GameRules
	virtual bool HaveGameRules(const char* pRulesName) = 0;

	// Summary
	//   Sets one GameRules instance as the one which should be currently be used
	// Parameters
	//   pGameRules - a pointer to a GameRules instance
	// Remarks
	//   Be warned that this function won't set the script GameRules table. The
	//   CreateGameRules will have to be called to do this.
	virtual void SetCurrentGameRules(IGameRules* pGameRules) = 0;

	// Summary
	//   Gets the currently used GameRules
	// Returns
	//   A pointer to the GameRules instance which is currently being used.
	virtual IGameRules* GetCurrentGameRules() const = 0;

	ILINE IEntity*      GetCurrentGameRulesEntity()
	{
		IGameRules* pGameRules = GetCurrentGameRules();

		if (pGameRules)
			return pGameRules->GetEntity();

		return 0;
	}
};

#endif
