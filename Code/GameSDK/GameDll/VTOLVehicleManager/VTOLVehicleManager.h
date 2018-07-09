// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   VTOLVehicleManager.h
//  Version:     v1.00
//  Created:     12/05/2011
//  Compilers:   Visual Studio.NET
//  Description: Predict and manage the future positions of specified entities
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VTOL_VEHICLE_MANAGER_
#define __VTOL_VEHICLE_MANAGER_

#if _MSC_VER > 1000
#pragma once
#endif

#include <CryEntitySystem/IEntitySystem.h>
#include "MPPathFollowingManager.h"

#include "GameRulesModules/IGameRulesModuleRMIListener.h"
#include "GameRulesModules/IGameRulesClientConnectionListener.h"
#include "Audio/GameAudio.h"

//Forward declarations
struct IVehicleSystem; 
struct IAIActor;
class CPlayer;
class CWaypointPath;

class CVTOLVehicleManager : public IEntityEventListener
													,	public IMPPathFollower
													,	public IGameRulesModuleRMIListener
													,	public IGameRulesClientConnectionListener
{

private:
	enum E_PlayerEnteredVtolStatus
	{
		E_PEVS_Entered = 0,
		E_PEVS_StillInside,
		E_PEVS_Exited
	};

	enum E_VTOLState
	{
		EVS_Normal,
		EVS_Invisible,
	};

	enum ERMITypes
	{
		eRMIType_None=0, 

		// single entity RMIs
		eRMITypeSingleEntity_vtol_destroyed,
		eRMITypeSingleEntity_vtol_hidden,				// for late joining clients only
		eRMITypeSingleEntity_vtol_respawned,
	};

	typedef std::vector<EntityId> TPlayerList;
	typedef std::pair<EntityId, E_PlayerEnteredVtolStatus> TPlayerStatus;
	typedef std::vector< TPlayerStatus > TPlayerStatusList;

	struct SVTOLInfo
	{
		QuatT location;
		AABB localBounds;
		TPlayerList playersInside;
		EntityId entityId;
		float stateTime;
		E_VTOLState state;

		SVTOLInfo()
			: location(IDENTITY)
			, localBounds(AABB::RESET)
			, entityId(0)
			, stateTime(0.f)
			, state(EVS_Normal)
		{}

		void Reset();
	};

	typedef std::map<EntityId, SVTOLInfo> TVTOLList;

	//////////////////////////////////////////////////////////////////////////

public:
	CVTOLVehicleManager();
	~CVTOLVehicleManager();

	void Init();
	void InitClient(int channelID);

	void Reset();
	void Update(float frameTime);

	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event );
	// ~IEntityEventListener

	// IMPPathFollower
	virtual void OnAttachRequest(const struct SPathFollowingAttachToPathParameters& params, const CWaypointPath* pPath);
	virtual void OnUpdateSpeedRequest(EntityId entityId, float speed);
	// ~IMPPathFollower

	// IGameRulesModuleRMIListener
	virtual void OnSingleEntityRMI(CGameRules::SModuleRMIEntityParams params);
	virtual void OnDoubleEntityRMI(CGameRules::SModuleRMITwoEntityParams params) {};
	virtual void OnEntityWithTimeRMI(CGameRules::SModuleRMIEntityTimeParams params) {};
	virtual void OnSvClientActionRMI(CGameRules::SModuleRMISvClientActionParams params, EntityId fromEid) {};
	// ~IGameRulesModuleRMIListener

	// IGameRulesClientConnectionListener
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) {};
	virtual void OnClientDisconnect(int channelId, EntityId playerId) {};
	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId) {}
	virtual void OnOwnClientEnteredGame();
	// ~IGameRulesClientConnectionListener

	void RegisterVTOL(EntityId entityId);
	bool IsVTOL(EntityId entityId);

	bool IsPlayerInVTOL(EntityId entityId) const; 
	bool TestEntityInVTOL(const IEntity& entity) const; 

	bool AnyEnemiesInside( EntityId friendlyPlayerId ) const;

private:

	void UpdateEntityInVTOL(SVTOLInfo& info, EntityId entityId);
	bool TestIsInVTOL(const SVTOLInfo& info, const AABB& aabb) const; 
	void UpdateRotationOfInternalPlayers(SVTOLInfo& info, const TPlayerStatusList& playerStatuses, const Quat& rotationDifference);
	void OnPlayerEntered(EntityId playerId);
	void OnPlayerExited(EntityId playerId);
	CPlayer* GetPlayerFromEntityId(EntityId id) const;
	void RespawnVTOL(IVehicle* pVehicle, SVTOLInfo& info);
	void CalculateBounds(IEntity* pVTOLEntity, SVTOLInfo& info);
	void LockSeats(IVehicle* pVehicle, bool lock);
	void CreateExplosion(IParticleEffect *inParticleEffect, const Vec3& pos, const float inEffectScale, TAudioSignalID inAudioSignal=INVALID_AUDIOSIGNAL_ID);
	void DestroyVTOL(IEntity *pVTOL, SVTOLInfo& info);
	void DestructionDamageRatioReached(IVehicle* pVehicle, SVTOLInfo& info, float frameTime);
	void RegisterVTOLWithPathFollower(bool registerVTOL);
	void SetupMovement(EntityId vtolId);
	void ResetVehicle(SVTOLInfo& info);

	TVTOLList	m_vtolList;
	IVehicleSystem* m_pVehicleSystem;
	TAudioSignalID m_explosionSignalId;
	IParticleEffect* m_pExplosionEffect;
	int m_moduleRMIIndex;
	bool m_bRegisteredWithPathFollower;
	uint16 m_classId;

	// STATIC:
public:
	static const char * s_VTOLClassName;
private:
	static const char * s_explosionEffectName;
};

#endif // __VTOL_VEHICLE_MANAGER_
