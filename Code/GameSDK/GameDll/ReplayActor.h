// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id: CReplayActor.h$
$DateTime$
Description: A replay actor spawned during KillCam replay

-------------------------------------------------------------------------
History:
- 03/19/2010 09:15:00: Created by Martin Sherburn

*************************************************************************/

#ifndef __REPLAYACTOR_H__
#define __REPLAYACTOR_H__

#include "ReplayObject.h"
#include "GameObjects/GameObject.h"
#include "DualCharacterProxy.h"
#include "Network/Lobby/SessionNames.h"

enum EReplayActorFlags
{
	eRAF_Dead																	= BIT(0),
	eRAF_FirstPerson													= BIT(1),
	eRAF_IsSquadMember												= BIT(2),
	eRAF_Invisible														= BIT(3),
	eRAF_LocalClient													= BIT(4),
	eRAF_HaveSpawnedMyCorpse									= BIT(5),
	eRAF_StealthKilling												= BIT(6),
};

struct SBasicReplayMovementParams
{
	SBasicReplayMovementParams()
		: m_desiredStrafeSmoothQTX(ZERO)
		, m_desiredStrafeSmoothRateQTX(ZERO)
		, m_fDesiredMoveSpeedSmoothQTX(0.f)
		, m_fDesiredMoveSpeedSmoothRateQTX(0.f)
		, m_fDesiredTurnSpeedSmoothQTX(0.f)
		, m_fDesiredTurnSpeedSmoothRateQTX(0.f)
	{
	}

	void SetDesiredLocalLocation2( ISkeletonAnim* pSkeletonAnim, const QuatT& desiredLocalLocation, float lookaheadTime, float fDeltaTime, float turnSpeedMultiplier);

	Vec2 m_desiredStrafeSmoothQTX;
	Vec2 m_desiredStrafeSmoothRateQTX;
	f32 m_fDesiredMoveSpeedSmoothQTX;
	f32 m_fDesiredMoveSpeedSmoothRateQTX;
	f32 m_fDesiredTurnSpeedSmoothQTX;
	f32 m_fDesiredTurnSpeedSmoothRateQTX;
};

class CReplayActor : public CReplayObject
{
public:

	typedef CryFixedStringT<DISPLAY_NAME_LENGTH> TPlayerDisplayName;
	static const int ShadowCharacterSlot = 5;

	CReplayActor();
	virtual ~CReplayActor();

	// IEntityComponent
	virtual	void ProcessEvent( const SEntityEvent &event );
	virtual uint64 GetEventMask() const final;
	// ~IEntityComponent

	virtual void PostInit(IGameObject *pGameObject);
	virtual void Release();

	void OnRemove();

	void SetupActionController(const char *controllerDef, const char *animDB1P, const char *animDB3P);
	class IActionController *GetActionController() 
	{
		return m_pActionController;
	}

	EntityId GetGunId() const { return m_gunId; }
	void SetGunId(EntityId gunId);
	void AddItem( const EntityId itemId );

	bool IsOnGround() const { return m_onGround; }
	void SetOnGround(bool onGround) { m_onGround = onGround; }

	Vec3 GetVelocity() const { return m_velocity; }
	void SetVelocity(const Vec3& velocity) {m_velocity = velocity;}

	int GetHealthPercentage() const { return m_healthPercentage; }
	void SetHealthPercentage(int health) { m_healthPercentage = health; }

	int GetRank() const { return m_rank; }
	void SetRank(int rank) { m_rank = rank; }

	int GetReincarnations() const { return m_reincarnations; }
	void SetReincarnations(int reincarnations) { m_reincarnations = reincarnations; }

	int8 GetTeam() const { return m_team; }
	void SetTeam(int8 team, const bool isFriendly);
	ILINE bool IsFriendly ( ) const { return m_isFriendly; }

	const TPlayerDisplayName& GetName() const { return m_name; }
	void SetName(const TPlayerDisplayName& name) { m_name = name; }

	void SetFirstPerson(bool firstperson);
	bool IsThirdPerson() { return (m_flags & eRAF_FirstPerson) == 0; }

	bool GetIsOnClientsSquad() const { return ((m_flags&eRAF_IsSquadMember)!=0); }
	void SetIsOnClientsSquad(const bool isOnClientsSquad ) { isOnClientsSquad ? m_flags |= eRAF_IsSquadMember : m_flags &= (~eRAF_IsSquadMember); }

	bool IsClient() const { return ((m_flags&eRAF_LocalClient)!=0); }
	void SetIsClient(const bool isClient ) { isClient ? m_flags |= eRAF_LocalClient : m_flags &= (~eRAF_LocalClient); }

	const QuatT& GetHeadPos() const { return m_headPos; }
	const QuatT& GetCameraTran() const { return m_cameraPos; }

	void LoadCharacter(const char *filename);

	void PlayAnimation(int animId, const CryCharAnimationParams& Params, float speedMultiplier, float animTime);
	void PlayUpperAnimation(int animId, const CryCharAnimationParams& Params, float speedMultiplier, float animTime);

	void ApplyMannequinEvent(const struct SMannHistoryItem &mannEvent, float animTime);

	void Ragdollize();
	void ApplyRagdollImpulse( pe_action_impulse& impulse );
	void TransitionToCorpse(IEntity& corpse);

	ICharacterInstance *GetShadowCharacter() const { return GetEntity()->GetCharacter(ShadowCharacterSlot); }

	ILINE SBasicReplayMovementParams& GetBasicMovement() { return m_basicMovement; }

	static CReplayActor *GetReplayActor(IEntity* pEntity)
	{
		if (pEntity)
		{
			if (CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER))
			{
				return (CReplayActor*)pGameObject->QueryExtension("ReplayActor");
			}
		}
		return NULL;
	}

	uint16 m_flags;		// EReplayActorFlags

	Vec3 m_trackerPreviousPos;
	float m_trackerUpdateTime;

	// These are used for HUD player name tags
	Vec3 m_drawPos;
	float m_size;

private:
	bool GetAdjustedLayerTime(ICharacterInstance* pCharacterInstance, int animId, const CryCharAnimationParams& params, float speedMultiplier, float animTime, float &adjustedLayerTime);
	void UpdateCharacter();
	void UpdateScopeContexts();
	void RemoveGun( const bool bRemoveEntity );
	void OnRagdollized();
	void Physicalize();

	CAnimationProxyDualCharacter m_animationProxy;
	CAnimationProxyDualCharacterUpper m_animationProxyUpper;
	CReplayItemList m_itemList;

	TPlayerDisplayName m_name;
	
	pe_action_impulse m_ragdollImpulse;

	QuatT m_headPos;
	QuatT m_cameraPos;

	Vec3 m_velocity;

	EntityId m_gunId;

	struct SAnimationContext *m_pAnimContext;
	class IActionController *m_pActionController;
	const class IAnimationDatabase *m_pAnimDatabase1P;
	const class IAnimationDatabase *m_pAnimDatabase3P;

	int m_lastFrameUpdated;
	int m_healthPercentage;
	int m_rank;
	int m_reincarnations;
	int16 m_headBoneID;
	int16 m_cameraBoneID;
	int8 m_team;
	bool m_onGround;
	bool m_isFriendly;

	SBasicReplayMovementParams m_basicMovement;

	struct GunRemovalListener : public IEntityEventListener
	{
		GunRemovalListener() : pReplayActor(NULL) {}
		virtual ~GunRemovalListener(){}
		virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event );

		CReplayActor * pReplayActor;
	} m_GunRemovalListener;
};

#endif //!__REPLAYACTOR_H__
