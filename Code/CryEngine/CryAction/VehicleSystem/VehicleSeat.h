// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a standard seat for vehicles

   -------------------------------------------------------------------------
   History:
   - 14:04:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLESEAT_H__
#define __VEHICLESEAT_H__

#include "IVehicleSystem.h"
#include "IMovementController.h"
#include "AIProxy.h"
#include "AnimationGraph/AnimatedCharacter.h"

class CGameChannel;
class CVehicle;
class CVehicleSeatSerializer;
class CVehicleSeatGroup;
class CScriptBind_Vehicle;
class CScriptBind_VehicleSeat;
class CCryAction;
class CVehicleSeatActionWeapons;
class CVehicleSeatActionRotateTurret;
class CVehicleViewThirdPerson;

struct SSeatActionData
{
	IVehicleSeatAction* pSeatAction;
	bool                isAvailableRemotely;
	bool                isEnabled;

	SSeatActionData()
	{
		pSeatAction = NULL;
		isAvailableRemotely = false;
		isEnabled = true;
	}

	void Reset()
	{
		if (pSeatAction)
			pSeatAction->Reset();

		isEnabled = true;
	}

	void Serialize(TSerialize ser, EEntityAspects aspects)
	{
		if (ser.GetSerializationTarget() != eST_Network)
		{
			ser.Value("isEnabledAction", isEnabled);
		}

		if (pSeatAction)
			pSeatAction->Serialize(ser, aspects);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(pSeatAction);
	}
};

typedef std::vector<SSeatActionData> TVehicleSeatActionVector;
typedef std::vector<IVehicleView*>   TVehicleViewVector;

class CVehicleSeat
	:
	  public IVehicleSeat,
	  public IMovementController,
	  public IFireController,
	  public IAnimatedCharacterListener
{
public:
	CVehicleSeat();
	~CVehicleSeat();

	virtual bool         Init(IVehicle* pVehicle, TVehicleSeatId seatId, const CVehicleParams& seatTable);
	virtual void         PostInit(IVehicle* pVehicle);
	virtual void         Reset();
	virtual void         Release() { delete this; }

	void                 GetMemoryUsage(ICrySizer* s) const;

	const char*          GetSeatName() const     { return m_name.c_str(); }
	IMovementController* GetMovementController() { return this; }

	void                 SerializeActions(TSerialize ser, EEntityAspects aspects);
	void                 OnSpawnComplete();

	// IVehicleSeat
	virtual bool                      Enter(EntityId actorId, bool isTransitionEnabled = true);
	virtual bool                      Exit(bool isTransitionEnabled, bool force = false, Vec3 exitPos = ZERO);

	virtual void                      SetLocked(EVehicleSeatLockStatus lock) { m_lockStatus = lock; }
	virtual EVehicleSeatLockStatus    GetLockedStatus() const                { return m_lockStatus; }

	virtual bool                      IsDriver() const                       { return m_isDriver; }
	virtual bool                      IsGunner() const;
	virtual bool                      IsRemoteControlled() const             { return IsRemoteControlledInternal(); }
	virtual bool                      IsLocked(IActor* pActor) const;
	bool                              IsRotatable() const                    { return m_pTurretAction != NULL; }

	virtual bool                      IsPassengerHidden() const              { return m_isPassengerHidden; }
	virtual bool                      IsPassengerExposed() const             { return m_isPassengerExposed; }
	virtual const SSeatSoundParams&   GetSoundParams() const                 { return m_soundParams; }
	SSeatSoundParams&                 GetSoundParams()                       { return m_soundParams; }

	virtual bool                      ProcessPassengerMovementRequest(const CMovementRequest& movementRequest);

	virtual IVehicleView*             GetView(TVehicleViewId viewId) const;
	virtual bool                      SetView(TVehicleViewId viewId);
	virtual TVehicleViewId            GetCurrentViewId() const       { return m_currentView; }
	virtual IVehicleView*             GetCurrentView() const         { return GetView(m_currentView); }
	virtual TVehicleViewId            GetNextViewId(bool bExcludeDebug, bool bExcludeDisabled) const;
	virtual TVehicleViewId            GetNextAvailableViewId() const { return GetNextViewId(false, false); }
	virtual int                       GetViewCount() const           { return m_views.size(); };

	virtual const char*               GetActionMap() const           { return m_actionMap.c_str(); }

	virtual bool                      IsAutoAimEnabled();

	virtual IVehicleSeatAction*       GetISeatActionWeapons();
	virtual const IVehicleSeatAction* GetISeatActionWeapons() const;

	virtual void                      PrePhysUpdate(const float dt);

	virtual void                      OffsetPosition(const Vec3& delta);
	// ~IVehicleSeat

	bool     EnterRemotely(EntityId actorId);
	bool     ExitRemotely();

	bool     SitDown();
	bool     StandUp();

	bool     ShouldEnterInFirstPerson() { return m_shouldEnterInFirstPerson; }
	bool     ShouldExitInFirstPerson()  { return m_shouldExitInFirstPerson; }

	void     GivesActorSeatFeatures(bool enabled = true, bool isRemote = false);

	void     SetPasssenger(EntityId passengerId) { m_passengerId = passengerId; }
	EntityId GetPassenger(bool remoteUser = false) const;
	IActor*  GetPassengerActor(bool remoteUser = false) const;
	bool     IsFree(IActor* pActor);

	void     OnAction(const TVehicleActionId actionId, int activationMode, float value);
	void     Update(float deltaTime);
	void     UpdateView(SViewParams& viewParams);
	void     UpdateRemote(IActor* pActor, float deltaTime);

	void     UpdateSounds(float deltaTime);
	void     StopSounds();

	void Serialize(TSerialize ser, EEntityAspects);
	void                    PostSerialize();
	CVehicleSeatSerializer* GetSerializer();
	void                    SetSerializer(CVehicleSeatSerializer* pSerializer);

	virtual IVehicle*       GetVehicle() const { return (IVehicle*)m_pVehicle; }
	virtual IVehiclePart*   GetAimPart() const { return (IVehiclePart*)m_pAimPart; }

	float                   ProcessPassengerDamage(float actorHealth, float damage, int hitTypeId, bool explosion);

	virtual void            OnPassengerDeath();
	virtual void            UnlinkPassenger(bool ragdoll);
	void                    OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
	virtual void            OnCameraShake(float& angleAmount, float& shiftAmount, const Vec3& pos, const char* source) const;

	// network
	EntityId NetGetPassengerId() const;
	void     NetSetPassengerId(EntityId passengerId);

	// IMovementController
	virtual bool RequestMovement(CMovementRequest& request);
	virtual void GetMovementState(SMovementState& state);
	virtual bool GetStanceState(const SStanceStateQuery& query, SStanceState& state);
	// ~IMovementController

	// IFireController
	virtual bool RequestFire(bool bFire);
	virtual void UpdateTargetPosAI(const Vec3& pos);
	// ~IFireController

	virtual void                     ForceFinishExiting()   {}

	const TVehicleSeatActionVector&  GetSeatActions() const { return m_seatActions; }
	TVehicleSeatActionVector&        GetSeatActions()       { return m_seatActions; }

	bool                             IsPassengerClientActor() const;

	int                              GetSeatActionId(IVehicleSeatAction* pAction) const;
	const IVehicleSeatAction*        GetSeatActionById(int id) const;
	virtual void                     ChangedNetworkState(NetworkAspectType aspects);

	const string&                    GetName() const   { return m_name; }
	TVehicleSeatId                   GetSeatId() const { return m_seatId; }

	CGameChannel*                    GetGameChannel(EntityId actorId);

	virtual IVehicleHelper*          GetSitHelper() const                        { return m_pSitHelper; }
	virtual IVehicleHelper*          GetAIVisionHelper() const                   { return m_pAIVisionHelper; }
	IVehicleHelper*                  GetEnterHelper() const                      { return m_pEnterHelper; }
	IVehicleHelper*                  GetExitHelper() const                       { return m_pExitHelper ? m_pExitHelper : m_pEnterHelper; }

	virtual int                      GetCurrentTransition() const                { return m_transitionType; }

	void                             SetSeatGroup(CVehicleSeatGroup* pSeatGroup) { m_pSeatGroup = pSeatGroup; }
	const CVehicleSeatGroup*         GetSeatGroup() const                        { return m_pSeatGroup; }

	CVehicleSeatActionWeapons*       GetSeatActionWeapons()                      { return m_pWeaponAction; }
	const CVehicleSeatActionWeapons* GetSeatActionWeapons() const                { return m_pWeaponAction; }

	int                              GetSeatGroupIndex() const                   { return m_seatGroupIndex; }
	void                             SetAIWeaponId(EntityId aiWeaponId)          { m_aiWeaponId = aiWeaponId; }

	// vehicle exit code.
	Matrix34      GetExitTM(IActor* pActor, bool opposite = false);                           // get this seat's current exit TM
	bool          TestExitPosition(IActor* pActor, Vec3& testPos, EntityId* pBlockingEntity); // test a specific position to see if the actor can be placed there.

	void          OnSeatFreed(TVehicleSeatId seatId, EntityId passengerId);
	CVehicleSeat* GetSeatUsedRemotely(bool onlyIfBeingUsed) const;

	bool          IsPassengerShielded() const { return m_isPassengerShielded; };

	bool          ShouldEnslaveActorAnimations() const;

	void          OnVehicleActionControllerDeleted();

	// from IAnimCharListener
	virtual void OnCharacterChange();

protected:

	bool            InitSeatActions(const CVehicleParams& seatTable);

	TVehicleSeatId  GetNextSeatId(const TVehicleSeatId currentSeatId) const;

	IVehicleClient* GetVehicleClient() const;

	void            UpdateHiddenState(bool hide);

	void            UpdatePassengerLocalTM(IActor* pActor);
	bool            QueueTransition();

	inline bool     IsRemoteControlledInternal() const { return m_isRemoteControlled; } //avoid the virtual cost on when used internally

	static CCryAction*              m_pGameFramework;

	TVehicleSeatActionVector        m_seatActions;
	TVehicleViewVector              m_views;

	string                          m_seatNameToUseForEntering;
	string                          m_useActionsRemotelyOfSeat; // only for init

	string                          m_actionMap;
	string                          m_name;

	SSeatSoundParams                m_soundParams;

	Vec3                            m_exitOffsetPlayer;
	Vec3                            m_adjustedExitPos;
	Vec3                            m_exitWorldPos; // set when player requests exit (to avoid repeated raycasts)

	CVehicle*                       m_pVehicle;
	CVehicleSeatSerializer*         m_pSerializer;

	CVehicleSeatActionWeapons*      m_pWeaponAction;
	CVehicleSeatActionRotateTurret* m_pTurretAction;
	CVehicleSeatGroup*              m_pSeatGroup;
	CVehicleSeat*                   m_pRemoteSeat;

	IVehiclePart*                   m_pAimPart;

	IVehicleHelper*                 m_pEnterHelper;
	IVehicleHelper*                 m_pExitHelper;
	IVehicleHelper*                 m_pSitHelper;
	IVehicleHelper*                 m_pAIVisionHelper;

	EntityId                        m_serializerId;
	EntityId                        m_passengerId;
	EntityId                        m_aiWeaponId;

	TVehicleSeatId                  m_seatId;

	TVehicleViewId                  m_currentView;

	EVehicleSeatLockStatus          m_lockStatus;
	EVehicleSeatLockStatus          m_lockDefault;

	IActionPtr                      m_pIdleAction;

	// Mannequin
	enum EMannequinSeatActions
	{
		eMannequinSeatAction_Idle = 0,
		eMannequinSeatAction_Exit,
		eMannequinSeatAction_Count
	};

	FragmentID m_seatFragmentIDs[eMannequinSeatAction_Count];

	// ugly variables for PostSerialize, needed to support recent changes in entity serialization
	EntityId m_postEnterId;
	EntityId m_postViewId;
	// ~ugly

	float m_explosionShakeMult;

	int   m_seatGroupIndex;

	uint8 m_transitionType;
	uint8 m_prevTransitionType;
	uint8 m_queuedTransition;

	bool  m_isDriver                 : 1;
	bool  m_isPassengerShielded      : 1;
	bool  m_isPassengerHidden        : 1;
	bool  m_isPassengerExposed       : 1;

	bool  m_isPlayerViewBlending     : 1;
	bool  m_stopAllAnimationsOnEnter : 1;
	bool  m_skipViewBlending         : 1;
	bool  m_updatePassengerTM        : 1;

	bool  m_isRemoteControlled       : 1;
	bool  m_isRagdollingOnDeath      : 1;
	bool  m_movePassengerOnExit      : 1;

	bool  m_autoAimEnabled           : 1;
	bool  m_transformViewOnExit      : 1; //When sat in the seat you are already in a world space position - no need to teleport when exiting

	bool  m_shouldEnterInFirstPerson : 1;
	bool  m_shouldExitInFirstPerson  : 1;
};

#endif
