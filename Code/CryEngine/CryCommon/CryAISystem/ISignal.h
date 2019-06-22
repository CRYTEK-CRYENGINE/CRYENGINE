// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryAISystem/IAISystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/XML/XMLAttrReader.h>

#include <memory>

#ifndef _RELEASE
#define SIGNAL_MANAGER_DEBUG
#endif // _RELEASE

// Signal ids
#define AISIGNAL_INCLUDE_DISABLED     0
#define AISIGNAL_DEFAULT              1
#define AISIGNAL_PROCESS_NEXT_UPDATE  3
#define AISIGNAL_NOTIFY_ONLY          9
#define AISIGNAL_ALLOW_DUPLICATES     10
#define AISIGNAL_RECEIVED_PREV_UPDATE 30  //!< Internal AI system use only, like AISIGNAL_DEFAULT but used for logging/recording.
//!< A signal sent in the previous update and processed in the current (AISIGNAL_PROCESS_NEXT_UPDATE).
#define AISIGNAL_INITIALIZATION       -100

namespace AISignals
{
	class CSignalManager;

	enum ESignalFilter
	{
		SIGNALFILTER_SENDER = 0,
		SIGNALFILTER_LASTOP,
		SIGNALFILTER_GROUPONLY,
		SIGNALFILTER_FACTIONONLY,
		SIGNALFILTER_ANYONEINCOMM,
		SIGNALFILTER_TARGET,
		SIGNALFILTER_SUPERGROUP,
		SIGNALFILTER_SUPERFACTION,
		SIGNALFILTER_SUPERTARGET,
		SIGNALFILTER_NEARESTGROUP,
		SIGNALFILTER_NEARESTSPECIES,
		SIGNALFILTER_NEARESTINCOMM,
		SIGNALFILTER_HALFOFGROUP,
		SIGNALFILTER_LEADER,
		SIGNALFILTER_GROUPONLY_EXCEPT,
		SIGNALFILTER_ANYONEINCOMM_EXCEPT,
		SIGNALFILTER_LEADERENTITY,
		SIGNALFILTER_NEARESTINCOMM_FACTION,
		SIGNALFILTER_NEARESTINCOMM_LOOKING,
		SIGNALFILTER_FORMATION,
		SIGNALFILTER_FORMATION_EXCEPT,
		SIGNALFILTER_READABILITY = 100,
		SIGNALFILTER_READABILITYAT,         //!< Readability anticipation.
		SIGNALFILTER_READABILITYRESPONSE,   //!< Readability response.
	};

	enum class ESignalTag
	{
		NONE = 0,
		CRY_ENGINE,
		GAME_SDK,
		DEPRECATED,
		GAME,

		COUNT
	};

	struct IAISignalExtraData
	{
		// <interfuscator:shuffle>
		virtual void        Serialize(TSerialize ser) = 0;
		virtual const char* GetObjectName() const = 0;
		virtual void        SetObjectName(const char* szObjectName) = 0;

		virtual void        ToScriptTable(SmartScriptTable& table) const = 0;
		virtual void        FromScriptTable(const SmartScriptTable& table) = 0;
		// </interfuscator:shuffle>

		IAISignalExtraData() : string1(NULL), string2(NULL) {}

		Vec3         point;
		Vec3         point2;
		ScriptHandle nID;
		float        fValue;
		int          iValue;
		int          iValue2;
		const char*  string1;
		const char*  string2;
	protected:
		~IAISignalExtraData() {}
	};

	class ISignalDescription
	{
	public:
		virtual const char* GetName() const = 0;
		virtual uint32 GetCrc() const = 0;
		virtual ESignalTag GetTag() const = 0;

		virtual bool IsNone() const = 0;

		bool operator ==(const ISignalDescription& rhs) const
		{
			return GetCrc() == rhs.GetCrc();
		}
	protected:
		~ISignalDescription() {}
	};

	// Provides access to built in signals descriptions
	// Initialized from the Signal Manager
	struct SBuiltInSignalsDescriptions
	{
		// ESignalTag::NONE
		const AISignals::ISignalDescription& GetNone() const { return *pNone; }
		void SetNone(const AISignals::ISignalDescription* val) { pNone = val; }

		// ESignalTag::GAME_SDK
		const AISignals::ISignalDescription& GetOnActJoinFormation() const { return *pOnActJoinFormation; }
		const AISignals::ISignalDescription& GetOnAloneSignal() const { return *pOnAloneSignal; }
		const AISignals::ISignalDescription& GetOnAssignedSearchSpotSeen() const { return *pOnAssignedSearchSpotSeen; }
		const AISignals::ISignalDescription& GetOnAssignedSearchSpotSeenBySomeoneElse() const { return *pOnAssignedSearchSpotSeenBySomeoneElse; }
		const AISignals::ISignalDescription& GetOnEnableFire() const { return *pOnEnableFire; }
		const AISignals::ISignalDescription& GetOnEnterSignal() const { return *pOnEnterSignal; }
		const AISignals::ISignalDescription& GetOnFallAndPlay() const { return *pOnFallAndPlay; }
		const AISignals::ISignalDescription& GetOnFinishedHitDeathReaction() const { return *pOnFinishedHitDeathReaction; }
		const AISignals::ISignalDescription& GetOnGrabbedByPlayer() const { return *pOnGrabbedByPlayer; }
		const AISignals::ISignalDescription& GetOnGroupMemberDied() const { return *pOnGroupMemberDied; }
		const AISignals::ISignalDescription& GetOnGroupMemberEnteredCombat() const { return *pOnGroupMemberEnteredCombat; }
		const AISignals::ISignalDescription& GetOnGroupMemberGrabbedByPlayer() const { return *pOnGroupMemberGrabbedByPlayer; }
		const AISignals::ISignalDescription& GetOnHitDeathReactionInterrupted() const { return *pOnHitDeathReactionInterrupted; }
		const AISignals::ISignalDescription& GetOnHitMountedGun() const { return *pOnHitMountedGun; }
		const AISignals::ISignalDescription& GetOnIncomingProjectile() const { return *pOnIncomingProjectile; }
		const AISignals::ISignalDescription& GetOnInTargetFov() const { return *pOnInTargetFov; }
		const AISignals::ISignalDescription& GetOnLeaveSignal() const { return *pOnLeaveSignal; }
		const AISignals::ISignalDescription& GetOnMeleePerformed() const { return *pOnMeleePerformed; }
		const AISignals::ISignalDescription& GetOnMeleeKnockedDownTarget() const { return *pOnMeleeKnockedDownTarget; }		
		const AISignals::ISignalDescription& GetOnNotAloneSignal() const { return *pOnNotAloneSignal; }
		const AISignals::ISignalDescription& GetOnNotInTargetFov() const { return *pOnNotInTargetFov; }
		const AISignals::ISignalDescription& GetOnNotVisibleFromTarget() const { return *pOnNotVisibleFromTarget; }
		const AISignals::ISignalDescription& GetOnReload() const { return *pOnReload; }
		const AISignals::ISignalDescription& GetOnReloadDone() const { return *pOnReloadDone; }
		const AISignals::ISignalDescription& GetOnSawObjectMove() const { return *pOnSawObjectMove; }
		const AISignals::ISignalDescription& GetOnSpottedDeadGroupMember() const { return *pOnSpottedDeadGroupMember; }
		const AISignals::ISignalDescription& GetOnTargetSearchSpotSeen() const { return *pOnTargetSearchSpotSeen; }
		const AISignals::ISignalDescription& GetOnTargetedByTurret() const { return *pOnTargetedByTurret; }
		const AISignals::ISignalDescription& GetOnThrownObjectSeen() const { return *pOnThrownObjectSeen; }
		const AISignals::ISignalDescription& GetOnTurretHasBeenDestroyed() const { return *pOnTurretHasBeenDestroyed; }
		const AISignals::ISignalDescription& GetOnTurretHasBeenDestroyedByThePlayer() const { return *pOnTurretHasBeenDestroyedByThePlayer; }
		const AISignals::ISignalDescription& GetOnVisibleFromTarget() const { return *pOnVisibleFromTarget; }
		const AISignals::ISignalDescription& GetOnWaterRippleSeen() const { return *pOnWaterRippleSeen; }
		const AISignals::ISignalDescription& GetOnWatchMeDie() const { return *pOnWatchMeDie; }

		void SetOnActJoinFormation(const AISignals::ISignalDescription* val) { pOnActJoinFormation = val; }
		void SetOnAloneSignal(const AISignals::ISignalDescription* val) { pOnAloneSignal = val; }
		void SetOnAssignedSearchSpotSeen(const AISignals::ISignalDescription* val) { pOnAssignedSearchSpotSeen = val; }
		void SetOnAssignedSearchSpotSeenBySomeoneElse(const AISignals::ISignalDescription* val) { pOnAssignedSearchSpotSeenBySomeoneElse = val; }
		void SetOnEnableFire(const AISignals::ISignalDescription* val) { pOnEnableFire = val; }
		void SetOnEnterSignal(const AISignals::ISignalDescription* val) { pOnEnterSignal = val; }
		void SetOnFallAndPlay(const AISignals::ISignalDescription* val) { pOnFallAndPlay = val; }
		void SetOnFinishedHitDeathReaction(const AISignals::ISignalDescription* val) { pOnFinishedHitDeathReaction = val; }
		void SetOnGrabbedByPlayer(const AISignals::ISignalDescription* val) { pOnGrabbedByPlayer = val; }
		void SetOnGroupMemberDied(const AISignals::ISignalDescription* val) { pOnGroupMemberDied = val; }
		void SetOnGroupMemberEnteredCombat(const AISignals::ISignalDescription* val) { pOnGroupMemberEnteredCombat = val; }
		void SetOnGroupMemberGrabbedByPlayer(const AISignals::ISignalDescription* val) { pOnGroupMemberGrabbedByPlayer = val; }
		void SetOnHitDeathReactionInterrupted(const AISignals::ISignalDescription* val) { pOnHitDeathReactionInterrupted = val; }
		void SetOnHitMountedGun(const AISignals::ISignalDescription* val) { pOnHitMountedGun = val; }
		void SetOnIncomingProjectile(const AISignals::ISignalDescription* val) { pOnIncomingProjectile = val; }
		void SetOnInTargetFov(const AISignals::ISignalDescription* val) { pOnInTargetFov = val; }
		void SetOnLeaveSignal(const AISignals::ISignalDescription* val) { pOnLeaveSignal = val; }
		void SetOnMeleePerformed(const AISignals::ISignalDescription* val) { pOnMeleePerformed = val; }
		void SetOnMeleeKnockedDownTarget(const AISignals::ISignalDescription* val) { pOnMeleeKnockedDownTarget = val; }
		void SetOnNotAloneSignal(const AISignals::ISignalDescription* val) { pOnNotAloneSignal = val; }
		void SetOnNotInTargetFov(const AISignals::ISignalDescription* val) { pOnNotInTargetFov = val; }
		void SetOnNotVisibleFromTarget(const AISignals::ISignalDescription* val) { pOnNotVisibleFromTarget = val; }
		void SetOnReload(const AISignals::ISignalDescription* val) { pOnReload = val; }
		void SetOnReloadDone(const AISignals::ISignalDescription* val) { pOnReloadDone = val; }
		void SetOnSawObjectMove(const AISignals::ISignalDescription* val) { pOnSawObjectMove = val; }
		void SetOnSpottedDeadGroupMember(const AISignals::ISignalDescription* val) { pOnSpottedDeadGroupMember = val; }
		void SetOnTargetSearchSpotSeen(const AISignals::ISignalDescription* val) { pOnTargetSearchSpotSeen = val; }
		void SetOnTargetedByTurret(const AISignals::ISignalDescription* val) { pOnTargetedByTurret = val; }
		void SetOnThrownObjectSeen(const AISignals::ISignalDescription* val) { pOnThrownObjectSeen = val; }
		void SetOnTurretHasBeenDestroyed(const AISignals::ISignalDescription* val) { pOnTurretHasBeenDestroyed = val; }
		void SetOnTurretHasBeenDestroyedByThePlayer(const AISignals::ISignalDescription* val) { pOnTurretHasBeenDestroyedByThePlayer = val; }
		void SetOnVisibleFromTarget(const AISignals::ISignalDescription* val) { pOnVisibleFromTarget = val; }
		void SetOnWaterRippleSeen(const AISignals::ISignalDescription* val) { pOnWaterRippleSeen = val; }
		void SetOnWatchMeDie(const AISignals::ISignalDescription* val) { pOnWatchMeDie = val; }

		//ESignalTag::CRY_ENGINE
		const AISignals::ISignalDescription& GetOnActionCreated() const { return *pOnActionCreated; }
		const AISignals::ISignalDescription& GetOnActionDone() const { return *pOnActionDone; }
		const AISignals::ISignalDescription& GetOnActionEnd() const { return *pOnActionEnd; }
		const AISignals::ISignalDescription& GetOnActionStart() const { return *pOnActionStart; }
		const AISignals::ISignalDescription& GetOnAttentionTargetThreatChanged() const { return *pOnAttentionTargetThreatChanged; }
		const AISignals::ISignalDescription& GetOnBulletRain() const { return *pOnBulletRain; }
		const AISignals::ISignalDescription& GetOnCloakedTargetSeen() const { return *pOnCloakedTargetSeen; }
		const AISignals::ISignalDescription& GetOnCloseCollision() const { return *pOnCloseCollision; }
		const AISignals::ISignalDescription& GetOnCloseContact() const { return *pOnCloseContact; }
		const AISignals::ISignalDescription& GetOnCoverCompromised() const { return *pOnCoverCompromised; }
		const AISignals::ISignalDescription& GetOnEMPGrenadeThrown() const { return *pOnEMPGrenadeThrown; }
		const AISignals::ISignalDescription& GetOnEnemyHeard() const { return *pOnEnemyHeard; }
		const AISignals::ISignalDescription& GetOnEnemyMemory() const { return *pOnEnemyMemory; }
		const AISignals::ISignalDescription& GetOnEnemySeen() const { return *pOnEnemySeen; }
		const AISignals::ISignalDescription& GetOnEnterCover() const { return *pOnEnterCover; }
		const AISignals::ISignalDescription& GetOnEnterNavSO() const { return *pOnEnterNavSO; }
		const AISignals::ISignalDescription& GetOnExposedToExplosion() const { return *pOnExposedToExplosion; }
		const AISignals::ISignalDescription& GetOnExposedToFlashBang() const { return *pOnExposedToFlashBang; }
		const AISignals::ISignalDescription& GetOnExposedToSmoke() const { return *pOnExposedToSmoke; }
		const AISignals::ISignalDescription& GetOnFlashbangGrenadeThrown() const { return *pOnFlashbangGrenadeThrown; }
		const AISignals::ISignalDescription& GetOnFragGrenadeThrown() const { return *pOnFragGrenadeThrown; }
		const AISignals::ISignalDescription& GetOnGrenadeDanger() const { return *pOnGrenadeDanger; }
		const AISignals::ISignalDescription& GetOnGrenadeThrown() const { return *pOnGrenadeThrown; }
		const AISignals::ISignalDescription& GetOnGroupChanged() const { return *pOnGroupChanged; }
		const AISignals::ISignalDescription& GetOnGroupMemberMutilated() const { return *pOnGroupMemberMutilated; }
		const AISignals::ISignalDescription& GetOnGroupTarget() const { return *pOnGroupTarget; }
		const AISignals::ISignalDescription& GetOnGroupTargetMemory() const { return *pOnGroupTargetMemory; }
		const AISignals::ISignalDescription& GetOnGroupTargetNone() const { return *pOnGroupTargetNone; }
		const AISignals::ISignalDescription& GetOnGroupTargetSound() const { return *pOnGroupTargetSound; }
		const AISignals::ISignalDescription& GetOnGroupTargetVisual() const { return *pOnGroupTargetVisual; }
		const AISignals::ISignalDescription& GetOnInterestingSoundHeard() const { return *pOnInterestingSoundHeard; }
		const AISignals::ISignalDescription& GetOnLeaveCover() const { return *pOnLeaveCover; }
		const AISignals::ISignalDescription& GetOnLeaveNavSO() const { return *pOnLeaveNavSO; }
		const AISignals::ISignalDescription& GetOnLostSightOfTarget() const { return *pOnLostSightOfTarget; }
		const AISignals::ISignalDescription& GetOnLowAmmo() const { return *pOnLowAmmo; }
		const AISignals::ISignalDescription& GetOnMeleeExecuted() const { return *pOnMeleeExecuted; }
		const AISignals::ISignalDescription& GetOnMemoryMoved() const { return *pOnMemoryMoved; }
		const AISignals::ISignalDescription& GetOnMovementPlanProduced() const { return *pOnMovementPlanProduced; }
		const AISignals::ISignalDescription& GetOnMovingInCover() const { return *pOnMovingInCover; }
		const AISignals::ISignalDescription& GetOnMovingToCover() const { return *pOnMovingToCover; }
		const AISignals::ISignalDescription& GetOnNewAttentionTarget() const { return *pOnNewAttentionTarget; }
		const AISignals::ISignalDescription& GetOnNoTarget() const { return *pOnNoTarget; }
		const AISignals::ISignalDescription& GetOnNoTargetAwareness() const { return *pOnNoTargetAwareness; }
		const AISignals::ISignalDescription& GetOnNoTargetVisible() const { return *pOnNoTargetVisible; }
		const AISignals::ISignalDescription& GetOnObjectSeen() const { return *pOnObjectSeen; }
		const AISignals::ISignalDescription& GetOnOutOfAmmo() const { return *pOnOutOfAmmo; }
		const AISignals::ISignalDescription& GetOnPlayerGoingAway() const { return *pOnPlayerGoingAway; }
		const AISignals::ISignalDescription& GetOnPlayerLooking() const { return *pOnPlayerLooking; }
		const AISignals::ISignalDescription& GetOnPlayerLookingAway() const { return *pOnPlayerLookingAway; }
		const AISignals::ISignalDescription& GetOnPlayerSticking() const { return *pOnPlayerSticking; }
		const AISignals::ISignalDescription& GetOnReloaded() const { return *pOnReloaded; }
		const AISignals::ISignalDescription& GetOnSeenByEnemy() const { return *pOnSeenByEnemy; }
		const AISignals::ISignalDescription& GetOnShapeDisabled() const { return *pOnShapeDisabled; }
		const AISignals::ISignalDescription& GetOnShapeEnabled() const { return *pOnShapeEnabled; }
		const AISignals::ISignalDescription& GetOnSmokeGrenadeThrown() const { return *pOnSmokeGrenadeThrown; }
		const AISignals::ISignalDescription& GetOnSomethingSeen() const { return *pOnSomethingSeen; }
		const AISignals::ISignalDescription& GetOnSuspectedSeen() const { return *pOnSuspectedSeen; }
		const AISignals::ISignalDescription& GetOnSuspectedSoundHeard() const { return *pOnSuspectedSoundHeard; }
		const AISignals::ISignalDescription& GetOnTargetApproaching() const { return *pOnTargetApproaching; }
		const AISignals::ISignalDescription& GetOnTargetArmoredHit() const { return *pOnTargetArmoredHit; }
		const AISignals::ISignalDescription& GetOnTargetCloaked() const { return *pOnTargetCloaked; }
		const AISignals::ISignalDescription& GetOnTargetDead() const { return *pOnTargetDead; }
		const AISignals::ISignalDescription& GetOnTargetFleeing() const { return *pOnTargetFleeing; }
		const AISignals::ISignalDescription& GetOnTargetInTerritory() const { return *pOnTargetInTerritory; }
		const AISignals::ISignalDescription& GetOnTargetOutOfTerritory() const { return *pOnTargetOutOfTerritory; }
		const AISignals::ISignalDescription& GetOnTargetStampMelee() const { return *pOnTargetStampMelee; }
		const AISignals::ISignalDescription& GetOnTargetTooClose() const { return *pOnTargetTooClose; }
		const AISignals::ISignalDescription& GetOnTargetTooFar() const { return *pOnTargetTooFar; }
		const AISignals::ISignalDescription& GetOnTargetUncloaked() const { return *pOnTargetUncloaked; }
		const AISignals::ISignalDescription& GetOnThreateningSeen() const { return *pOnThreateningSeen; }
		const AISignals::ISignalDescription& GetOnThreateningSoundHeard() const { return *pOnThreateningSoundHeard; }
		const AISignals::ISignalDescription& GetOnUpdateItems() const { return *pOnUpdateItems; }
		const AISignals::ISignalDescription& GetOnUseSmartObject() const { return *pOnUseSmartObject; }
		const AISignals::ISignalDescription& GetOnWeaponEndBurst() const { return *pOnWeaponEndBurst; }
		const AISignals::ISignalDescription& GetOnWeaponMelee() const { return *pOnWeaponMelee; }
		const AISignals::ISignalDescription& GetOnWeaponShoot() const { return *pOnWeaponShoot; }

		void SetOnActionCreated(const AISignals::ISignalDescription* val) { pOnActionCreated = val; }
		void SetOnActionDone(const AISignals::ISignalDescription* val) { pOnActionDone = val; }
		void SetOnActionEnd(const AISignals::ISignalDescription* val) { pOnActionEnd = val; }
		void SetOnActionStart(const AISignals::ISignalDescription* val) { pOnActionStart = val; }
		void SetOnAttentionTargetThreatChanged(const AISignals::ISignalDescription* val) { pOnAttentionTargetThreatChanged = val; }
		void SetOnBulletRain(const AISignals::ISignalDescription* val) { pOnBulletRain = val; }
		void SetOnCloakedTargetSeen(const AISignals::ISignalDescription* val) { pOnCloakedTargetSeen = val; }
		void SetOnCloseCollision(const AISignals::ISignalDescription* val) { pOnCloseCollision = val; }
		void SetOnCloseContact(const AISignals::ISignalDescription* val) { pOnCloseContact = val; }
		void SetOnCoverCompromised(const AISignals::ISignalDescription* val) { pOnCoverCompromised = val; }
		void SetOnEMPGrenadeThrown(const AISignals::ISignalDescription* val) { pOnEMPGrenadeThrown = val; }
		void SetOnEnemyHeard(const AISignals::ISignalDescription* val) { pOnEnemyHeard = val; }
		void SetOnEnemyMemory(const AISignals::ISignalDescription* val) { pOnEnemyMemory = val; }
		void SetOnEnemySeen(const AISignals::ISignalDescription* val) { pOnEnemySeen = val; }
		void SetOnEnterCover(const AISignals::ISignalDescription* val) { pOnEnterCover = val; }
		void SetOnEnterNavSO(const AISignals::ISignalDescription* val) { pOnEnterNavSO = val; }
		void SetOnExposedToExplosion(const AISignals::ISignalDescription* val) { pOnExposedToExplosion = val; }
		void SetOnExposedToFlashBang(const AISignals::ISignalDescription* val) { pOnExposedToFlashBang = val; }
		void SetOnExposedToSmoke(const AISignals::ISignalDescription* val) { pOnExposedToSmoke = val; }
		void SetOnFlashbangGrenadeThrown(const AISignals::ISignalDescription* val) { pOnFlashbangGrenadeThrown = val; }
		void SetOnFragGrenadeThrown(const AISignals::ISignalDescription* val) { pOnFragGrenadeThrown = val; }
		void SetOnGrenadeDanger(const AISignals::ISignalDescription* val) { pOnGrenadeDanger = val; }
		void SetOnGrenadeThrown(const AISignals::ISignalDescription* val) { pOnGrenadeThrown = val; }
		void SetOnGroupChanged(const AISignals::ISignalDescription* val) { pOnGroupChanged = val; }
		void SetOnGroupMemberMutilated(const AISignals::ISignalDescription* val) { pOnGroupMemberMutilated = val; }
		void SetOnGroupTarget(const AISignals::ISignalDescription* val) { pOnGroupTarget = val; }
		void SetOnGroupTargetMemory(const AISignals::ISignalDescription* val) { pOnGroupTargetMemory = val; }
		void SetOnGroupTargetNone(const AISignals::ISignalDescription* val) { pOnGroupTargetNone = val; }
		void SetOnGroupTargetSound(const AISignals::ISignalDescription* val) { pOnGroupTargetSound = val; }
		void SetOnGroupTargetVisual(const AISignals::ISignalDescription* val) { pOnGroupTargetVisual = val; }
		void SetOnInterestingSoundHeard(const AISignals::ISignalDescription* val) { pOnInterestingSoundHeard = val; }
		void SetOnLeaveCover(const AISignals::ISignalDescription* val) { pOnLeaveCover = val; }
		void SetOnLeaveNavSO(const AISignals::ISignalDescription* val) { pOnLeaveNavSO = val; }
		void SetOnLostSightOfTarget(const AISignals::ISignalDescription* val) { pOnLostSightOfTarget = val; }
		void SetOnLowAmmo(const AISignals::ISignalDescription* val) { pOnLowAmmo = val; }
		void SetOnMeleeExecuted(const AISignals::ISignalDescription* val) { pOnMeleeExecuted = val; }
		void SetOnMemoryMoved(const AISignals::ISignalDescription* val) { pOnMemoryMoved = val; }
		void SetOnMovementPlanProduced(const AISignals::ISignalDescription* val) { pOnMovementPlanProduced = val; }
		void SetOnMovingInCover(const AISignals::ISignalDescription* val) { pOnMovingInCover = val; }
		void SetOnMovingToCover(const AISignals::ISignalDescription* val) { pOnMovingToCover = val; }
		void SetOnNewAttentionTarget(const AISignals::ISignalDescription* val) { pOnNewAttentionTarget = val; }
		void SetOnNoTarget(const AISignals::ISignalDescription* val) { pOnNoTarget = val; }
		void SetOnNoTargetAwareness(const AISignals::ISignalDescription* val) { pOnNoTargetAwareness = val; }
		void SetOnNoTargetVisible(const AISignals::ISignalDescription* val) { pOnNoTargetVisible = val; }
		void SetOnObjectSeen(const AISignals::ISignalDescription* val) { pOnObjectSeen = val; }
		void SetOnOutOfAmmo(const AISignals::ISignalDescription* val) { pOnOutOfAmmo = val; }
		void SetOnPlayerGoingAway(const AISignals::ISignalDescription* val) { pOnPlayerGoingAway = val; }
		void SetOnPlayerLooking(const AISignals::ISignalDescription* val) { pOnPlayerLooking = val; }
		void SetOnPlayerLookingAway(const AISignals::ISignalDescription* val) { pOnPlayerLookingAway = val; }
		void SetOnPlayerSticking(const AISignals::ISignalDescription* val) { pOnPlayerSticking = val; }
		void SetOnReloaded(const AISignals::ISignalDescription* val) { pOnReloaded = val; }
		void SetOnSeenByEnemy(const AISignals::ISignalDescription* val) { pOnSeenByEnemy = val; }
		void SetOnShapeDisabled(const AISignals::ISignalDescription* val) { pOnShapeDisabled = val; }
		void SetOnShapeEnabled(const AISignals::ISignalDescription* val) { pOnShapeEnabled = val; }
		void SetOnSmokeGrenadeThrown(const AISignals::ISignalDescription* val) { pOnSmokeGrenadeThrown = val; }
		void SetOnSomethingSeen(const AISignals::ISignalDescription* val) { pOnSomethingSeen = val; }
		void SetOnSuspectedSeen(const AISignals::ISignalDescription* val) { pOnSuspectedSeen = val; }
		void SetOnSuspectedSoundHeard(const AISignals::ISignalDescription* val) { pOnSuspectedSoundHeard = val; }
		void SetOnTargetApproaching(const AISignals::ISignalDescription* val) { pOnTargetApproaching = val; }
		void SetOnTargetArmoredHit(const AISignals::ISignalDescription* val) { pOnTargetArmoredHit = val; }
		void SetOnTargetCloaked(const AISignals::ISignalDescription* val) { pOnTargetCloaked = val; }
		void SetOnTargetDead(const AISignals::ISignalDescription* val) { pOnTargetDead = val; }
		void SetOnTargetFleeing(const AISignals::ISignalDescription* val) { pOnTargetFleeing = val; }
		void SetOnTargetInTerritory(const AISignals::ISignalDescription* val) { pOnTargetInTerritory = val; }
		void SetOnTargetOutOfTerritory(const AISignals::ISignalDescription* val) { pOnTargetOutOfTerritory = val; }
		void SetOnTargetStampMelee(const AISignals::ISignalDescription* val) { pOnTargetStampMelee = val; }
		void SetOnTargetTooClose(const AISignals::ISignalDescription* val) { pOnTargetTooClose = val; }
		void SetOnTargetTooFar(const AISignals::ISignalDescription* val) { pOnTargetTooFar = val; }
		void SetOnTargetUncloaked(const AISignals::ISignalDescription* val) { pOnTargetUncloaked = val; }
		void SetOnThreateningSeen(const AISignals::ISignalDescription* val) { pOnThreateningSeen = val; }
		void SetOnThreateningSoundHeard(const AISignals::ISignalDescription* val) { pOnThreateningSoundHeard = val; }
		void SetOnUpdateItems(const AISignals::ISignalDescription* val) { pOnUpdateItems = val; }
		void SetOnUseSmartObject(const AISignals::ISignalDescription* val) { pOnUseSmartObject = val; }
		void SetOnWeaponEndBurst(const AISignals::ISignalDescription* val) { pOnWeaponEndBurst = val; }
		void SetOnWeaponMelee(const AISignals::ISignalDescription* val) { pOnWeaponMelee = val; }
		void SetOnWeaponShoot(const AISignals::ISignalDescription* val) { pOnWeaponShoot = val; }

		// ESignalTag::DEPRECATED
		const AISignals::ISignalDescription& GetOnAbortAction_DEPRECATED() const { return *pOnAbortAction_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActAlerted_DEPRECATED() const { return *pOnActAlerted_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActAnim_DEPRECATED() const { return *pOnActAnim_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActAnimex_DEPRECATED() const { return *pOnActAnimex_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActChaseTarget_DEPRECATED() const { return *pOnActChaseTarget_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActDialog_DEPRECATED() const { return *pOnActDialog_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActDialogOver_DEPRECATED() const { return *pOnActDialogOver_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActDropObject_DEPRECATED() const { return *pOnActDropObject_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActDummy_DEPRECATED() const { return *pOnActDummy_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActEnterVehicle_DEPRECATED() const { return *pOnActEnterVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActExecute_DEPRECATED() const { return *pOnActExecute_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActExitVehicle_DEPRECATED() const { return *pOnActExitVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActFollowPath_DEPRECATED() const { return *pOnActFollowPath_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActUseObject_DEPRECATED() const { return *pOnActUseObject_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnActVehicleStickPath_DEPRECATED() const { return *pOnActVehicleStickPath_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnAddDangerPoint_DEPRECATED() const { return *pOnAddDangerPoint_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnAttackShootSpot_DEPRECATED() const { return *pOnAttackShootSpot_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnAttackSwitchPosition_DEPRECATED() const { return *pOnAttackSwitchPosition_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnAvoidDanger_DEPRECATED() const { return *pOnAvoidDanger_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnBackOffFailed_DEPRECATED() const { return *pOnBackOffFailed_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnBehind_DEPRECATED() const { return *pOnBehind_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnCallReinforcements_DEPRECATED() const { return *pOnCallReinforcements_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnChangeStance_DEPRECATED() const { return *pOnChangeStance_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnChargeBailOut_DEPRECATED() const { return *pOnChargeBailOut_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnChargeHit_DEPRECATED() const { return *pOnChargeHit_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnChargeMiss_DEPRECATED() const { return *pOnChargeMiss_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnChargeStart_DEPRECATED() const { return *pOnChargeStart_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnCheckDeadBody_DEPRECATED() const { return *pOnCheckDeadBody_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnCheckDeadTarget_DEPRECATED() const { return *pOnCheckDeadTarget_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnClearSpotList_DEPRECATED() const { return *pOnClearSpotList_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnCombatTargetDisabled_DEPRECATED() const { return *pOnCombatTargetDisabled_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnCombatTargetEnabled_DEPRECATED() const { return *pOnCombatTargetEnabled_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnCoverReached_DEPRECATED() const { return *pOnCoverReached_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEndPathOffset_DEPRECATED() const { return *pOnEndPathOffset_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEndVehicleDanger_DEPRECATED() const { return *pOnEndVehicleDanger_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnExecutingSpecialAction_DEPRECATED() const { return *pOnExecutingSpecialAction_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFireDisabled_DEPRECATED() const { return *pOnFireDisabled_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFiringAllowed_DEPRECATED() const { return *pOnFiringAllowed_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFiringNotAllowed_DEPRECATED() const { return *pOnFiringNotAllowed_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnForcedExecute_DEPRECATED() const { return *pOnForcedExecute_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnForcedExecuteComplete_DEPRECATED() const { return *pOnForcedExecuteComplete_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFormationPointReached_DEPRECATED() const { return *pOnFormationPointReached_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnKeepEnabled_DEPRECATED() const { return *pOnKeepEnabled_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLeaderActionCompleted_DEPRECATED() const { return *pOnLeaderActionCompleted_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLeaderActionFailed_DEPRECATED() const { return *pOnLeaderActionFailed_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLeaderAssigned_DEPRECATED() const { return *pOnLeaderAssigned_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLeaderDeassigned_DEPRECATED() const { return *pOnLeaderDeassigned_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLeaderDied_DEPRECATED() const { return *pOnLeaderDied_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLeaderTooFar_DEPRECATED() const { return *pOnLeaderTooFar_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLeftLean_DEPRECATED() const { return *pOnLeftLean_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLowHideSpot_DEPRECATED() const { return *pOnLowHideSpot_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNavTypeChanged_DEPRECATED() const { return *pOnNavTypeChanged_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNoAimPosture_DEPRECATED() const { return *pOnNoAimPosture_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNoFormationPoint_DEPRECATED() const { return *pOnNoFormationPoint_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNoGroupTarget_DEPRECATED() const { return *pOnNoGroupTarget_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNoPathFound_DEPRECATED() const { return *pOnNoPathFound_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNoPeekPosture_DEPRECATED() const { return *pOnNoPeekPosture_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNotBehind_DEPRECATED() const { return *pOnNotBehind_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrdSearch_DEPRECATED() const { return *pOnOrdSearch_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderAcquireTarget_DEPRECATED() const { return *pOnOrderAcquireTarget_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderAttack_DEPRECATED() const { return *pOnOrderAttack_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderDone_DEPRECATED() const { return *pOnOrderDone_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderFail_DEPRECATED() const { return *pOnOrderFail_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderSearch_DEPRECATED() const { return *pOnOrderSearch_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnPathFindAtStart_DEPRECATED() const { return *pOnPathFindAtStart_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnPathFound_DEPRECATED() const { return *pOnPathFound_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRPTLeaderDead_DEPRECATED() const { return *pOnRPTLeaderDead_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRequestReinforcementTriggered_DEPRECATED() const { return *pOnRequestReinforcementTriggered_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRequestUpdate_DEPRECATED() const { return *pOnRequestUpdate_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRequestUpdateAlternative_DEPRECATED() const { return *pOnRequestUpdateAlternative_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRequestUpdateTowards_DEPRECATED() const { return *pOnRequestUpdateTowards_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRightLean_DEPRECATED() const { return *pOnRightLean_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSameHidespotAgain_DEPRECATED() const { return *pOnSameHidespotAgain_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnScaleFormation_DEPRECATED() const { return *pOnScaleFormation_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSetMinDistanceToTarget_DEPRECATED() const { return *pOnSetMinDistanceToTarget_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSetUnitProperties_DEPRECATED() const { return *pOnSetUnitProperties_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSpecialAction_DEPRECATED() const { return *pOnSpecialAction_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSpecialActionDone_DEPRECATED() const { return *pOnSpecialActionDone_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnStartFollowPath_DEPRECATED() const { return *pOnStartFollowPath_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnStartForceFire_DEPRECATED() const { return *pOnStartForceFire_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnStopFollowPath_DEPRECATED() const { return *pOnStopFollowPath_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnStopForceFire_DEPRECATED() const { return *pOnStopForceFire_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSurpriseAction_DEPRECATED() const { return *pOnSurpriseAction_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnTPSDestFound_DEPRECATED() const { return *pOnTPSDestFound_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnTPSDestNotFound_DEPRECATED() const { return *pOnTPSDestNotFound_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnTPSDestReached_DEPRECATED() const { return *pOnTPSDestReached_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnTargetNavTypeChanged_DEPRECATED() const { return *pOnTargetNavTypeChanged_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUnitBusy_DEPRECATED() const { return *pOnUnitBusy_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUnitDamaged_DEPRECATED() const { return *pOnUnitDamaged_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUnitDied_DEPRECATED() const { return *pOnUnitDied_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUnitMoving_DEPRECATED() const { return *pOnUnitMoving_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUnitResumed_DEPRECATED() const { return *pOnUnitResumed_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUnitStop_DEPRECATED() const { return *pOnUnitStop_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUnitSuspended_DEPRECATED() const { return *pOnUnitSuspended_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnVehicleDanger_DEPRECATED() const { return *pOnVehicleDanger_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnAIAgressive_DEPRECATED() const { return *pOnAIAgressive_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnAlertStatus_DEPRECATED() const { return *pOnAlertStatus_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnAtCloseRange_DEPRECATED() const { return *pOnAtCloseRange_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnAtOptimalRange_DEPRECATED() const { return *pOnAtOptimalRange_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnBodyFallSound_DEPRECATED() const { return *pOnBodyFallSound_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnChangeSetEnd_DEPRECATED() const { return *pOnChangeSetEnd_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnCheckNextWeaponAccessory_DEPRECATED() const { return *pOnCheckNextWeaponAccessory_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnCommandTold_DEPRECATED() const { return *pOnCommandTold_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnControllVehicle_DEPRECATED() const { return *pOnControllVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnDamage_DEPRECATED() const { return *pOnDamage_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnDeadMemberSpottedBySomeoneElse_DEPRECATED() const { return *pOnDeadMemberSpottedBySomeoneElse_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnDisableFire_DEPRECATED() const { return *pOnDisableFire; }
		const AISignals::ISignalDescription& GetOnDoExitVehicle_DEPRECATED() const { return *pOnDoExitVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnDriverEntered_DEPRECATED() const { return *pOnDriverEntered_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnDriverOut_DEPRECATED() const { return *pOnDriverOut_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnDroneSeekCommand_DEPRECATED() const { return *pOnDroneSeekCommand_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnDropped_DEPRECATED() const { return *pOnDropped_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEMPGrenadeThrownInGroup_DEPRECATED() const { return *pOnEMPGrenadeThrownInGroup_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEnableAlertStatus_DEPRECATED() const { return *pOnEnableAlertStatus_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEnemyDamage_DEPRECATED() const { return *pOnEnemyDamage_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEnemyDied_DEPRECATED() const { return *pOnEnemyDied_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEnteredVehicleGunner_DEPRECATED() const { return *pOnEnteredVehicleGunner_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEnteredVehicle_DEPRECATED() const { return *pOnEnteredVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEnteringEnd_DEPRECATED() const { return *pOnEnteringEnd_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnEnteringVehicle_DEPRECATED() const { return *pOnEnteringVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnExitVehicleStand_DEPRECATED() const { return *pOnExitVehicleStand_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnExitedVehicle_DEPRECATED() const { return *pOnExitedVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnExitingEnd_DEPRECATED() const { return *pOnExitingEnd_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFlashbangGrenadeThrownInGroup_DEPRECATED() const { return *pOnFlashGrenadeThrownInGroup_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFollowLeader_DEPRECATED() const { return *pOnFollowLeader_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFragGrenadeThrownInGroup_DEPRECATED() const { return *pOnFragGrenadeThrownInGroup_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFriendlyDamageByPlayer_DEPRECATED() const { return *pOnFriendlyDamageByPlayer_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnFriendlyDamage_DEPRECATED() const { return *pOnFriendlyDamage_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnGoToGrabbed_DEPRECATED() const { return *pOnGoToGrabbed_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnGoToSeek_DEPRECATED() const { return *pOnGoToSeek_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnGroupMemberDiedOnAGL_DEPRECATED() const { return *pOnGroupMemberDiedOnAGL_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnGroupMemberDiedOnHMG_DEPRECATED() const { return *pOnGroupMemberDiedOnHMG_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnGroupMemberDiedOnMountedWeapon_DEPRECATED() const { return *pOnGroupMemberDiedOnMountedWeapon_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnGunnerLostTarget_DEPRECATED() const { return *pOnGunnerLostTarget_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnInVehicleFoundTarget_DEPRECATED() const { return *pOnInVehicleFoundTarget_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnInVehicleRequestStartFire_DEPRECATED() const { return *pOnInVehicleRequestStartFire_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnInVehicleRequestStopFire_DEPRECATED() const { return *pOnInVehicleRequestStopFire_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnIncomingFire_DEPRECATED() const { return *pOnIncomingFire_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnJoinTeam_DEPRECATED() const { return *pOnJoinTeam_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnJustConstructed_DEPRECATED() const { return *pOnJustConstructed_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLeaveMountedWeapon_DEPRECATED() const { return *pOnLeaveMountedWeapon_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLowAmmoFinnished_DEPRECATED() const { return *pOnLowAmmoFinished_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnLowAmmoStart_DEPRECATED() const { return *pOnLowAmmoStart_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNearbyWaterRippleSeen_DEPRECATED() const { return *pOnNearbyWaterRippleSeen_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNewSpawn_DEPRECATED() const { return *pOnNewSpawn_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnNoSearchSpotsAvailable_DEPRECATED() const { return *pOnNoSearchSpotsAvailable_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderExitVehicle_DEPRECATED() const { return *pOnOrderExitVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderFollow_DEPRECATED() const { return *pOnOrderFollow_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderLeaveVehicle_DEPRECATED() const { return *pOnOrderLeaveVehicle_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderMove_DEPRECATED() const { return *pOnOrderMove_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnOrderUse_DEPRECATED() const { return *pOnOrderUse_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnPlayerDied_DEPRECATED() const { return *pOnPlayerDied_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnPlayerHit_DEPRECATED() const { return *pOnPlayerHit_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnPlayerNiceShot_DEPRECATED() const { return *pOnPlayerNiceShot_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnPlayerTeamKill_DEPRECATED() const { return *pOnPlayerTeamKill_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnPrepareForMountedWeaponUse_DEPRECATED() const { return *pOnPrepareForMountedWeaponUse_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRefPointReached_DEPRECATED() const { return *pOnRefPointReached_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRelocate_DEPRECATED() const { return *pOnRelocate_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnRequestJoinTeam_DEPRECATED() const { return *pOnRequestJoinTeam_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnResetAssignment_DEPRECATED() const { return *pOnResetAssignment_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSharedUseThisMountedWeapon_DEPRECATED() const { return *pOnSharedUseThisMountedWeapon_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSmokeGrenadeThrownInGroup_DEPRECATED() const { return *pOnSmokeGrenadeThrownInGroup_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSomebodyDied_DEPRECATED() const { return *pOnSomebodyDied_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSpawn_DEPRECATED() const { return *pOnSpawn_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSquadmateDied_DEPRECATED() const { return *pOnSquadmateDied_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnStopAndExit_DEPRECATED() const { return *pOnStopAndExit_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnSuspiciousActivity_DEPRECATED() const { return *pOnSuspiciousActivity_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnTargetLost_DEPRECATED() const { return *pOnTargetLost_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnTargetNotVisible_DEPRECATED() const { return *pOnTargetNotVisible_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnTooFarFromWeapon_DEPRECATED() const { return *pOnTooFarFromWeapon_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUnloadDone_DEPRECATED() const { return *pOnUnloadDone_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUseMountedWeapon2_DEPRECATED() const { return *pOnUseMountedWeapon2_DEPRECATED; }
		const AISignals::ISignalDescription& GetOnUseMountedWeapon_DEPRECATED() const { return *pOnUseMountedWeapon_DEPRECATED; }

		void SetOnAbortAction_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAbortAction_DEPRECATED = val; }
		void SetOnActAlerted_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActAlerted_DEPRECATED = val; }
		void SetOnActAnim_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActAnim_DEPRECATED = val; }
		void SetOnActAnimex_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActAnimex_DEPRECATED = val; }
		void SetOnActChaseTarget_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActChaseTarget_DEPRECATED = val; }
		void SetOnActDialog_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActDialog_DEPRECATED = val; }
		void SetOnActDialogOver_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActDialogOver_DEPRECATED = val; }
		void SetOnActDropObject_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActDropObject_DEPRECATED = val; }
		void SetOnActDummy_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActDummy_DEPRECATED = val; }
		void SetOnActEnterVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActEnterVehicle_DEPRECATED = val; }
		void SetOnActExecute_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActExecute_DEPRECATED = val; }
		void SetOnActExitVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActExitVehicle_DEPRECATED = val; }
		void SetOnActFollowPath_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActFollowPath_DEPRECATED = val; }
		void SetOnActUseObject_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActUseObject_DEPRECATED = val; }
		void SetOnActVehicleStickPath_DEPRECATED(const AISignals::ISignalDescription* val) { pOnActVehicleStickPath_DEPRECATED = val; }
		void SetOnAddDangerPoint_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAddDangerPoint_DEPRECATED = val; }
		void SetOnAttackShootSpot_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAttackShootSpot_DEPRECATED = val; }
		void SetOnAttackSwitchPosition_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAttackSwitchPosition_DEPRECATED = val; }
		void SetOnAvoidDanger_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAvoidDanger_DEPRECATED = val; }
		void SetOnBackOffFailed_DEPRECATED(const AISignals::ISignalDescription* val) { pOnBackOffFailed_DEPRECATED = val; }
		void SetOnBehind_DEPRECATED(const AISignals::ISignalDescription* val) { pOnBehind_DEPRECATED = val; }
		void SetOnCallReinforcements_DEPRECATED(const AISignals::ISignalDescription* val) { pOnCallReinforcements_DEPRECATED = val; }
		void SetOnChangeStance_DEPRECATED(const AISignals::ISignalDescription* val) { pOnChangeStance_DEPRECATED = val; }
		void SetOnChargeBailOut_DEPRECATED(const AISignals::ISignalDescription* val) { pOnChargeBailOut_DEPRECATED = val; }
		void SetOnChargeHit_DEPRECATED(const AISignals::ISignalDescription* val) { pOnChargeHit_DEPRECATED = val; }
		void SetOnChargeMiss_DEPRECATED(const AISignals::ISignalDescription* val) { pOnChargeMiss_DEPRECATED = val; }
		void SetOnChargeStart_DEPRECATED(const AISignals::ISignalDescription* val) { pOnChargeStart_DEPRECATED = val; }
		void SetOnCheckDeadBody_DEPRECATED(const AISignals::ISignalDescription* val) { pOnCheckDeadBody_DEPRECATED = val; }
		void SetOnCheckDeadTarget_DEPRECATED(const AISignals::ISignalDescription* val) { pOnCheckDeadTarget_DEPRECATED = val; }
		void SetOnClearSpotList_DEPRECATED(const AISignals::ISignalDescription* val) { pOnClearSpotList_DEPRECATED = val; }
		void SetOnCombatTargetDisabled_DEPRECATED(const AISignals::ISignalDescription* val) { pOnCombatTargetDisabled_DEPRECATED = val; }
		void SetOnCombatTargetEnabled_DEPRECATED(const AISignals::ISignalDescription* val) { pOnCombatTargetEnabled_DEPRECATED = val; }
		void SetOnCoverReached_DEPRECATED(const AISignals::ISignalDescription* val) { pOnCoverReached_DEPRECATED = val; }
		void SetOnEndPathOffset_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEndPathOffset_DEPRECATED = val; }
		void SetOnEndVehicleDanger_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEndVehicleDanger_DEPRECATED = val; }
		void SetOnExecutingSpecialAction_DEPRECATED(const AISignals::ISignalDescription* val) { pOnExecutingSpecialAction_DEPRECATED = val; }
		void SetOnFireDisabled_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFireDisabled_DEPRECATED = val; }
		void SetOnFiringAllowed_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFiringAllowed_DEPRECATED = val; }
		void SetOnFiringNotAllowed_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFiringNotAllowed_DEPRECATED = val; }
		void SetOnForcedExecute_DEPRECATED(const AISignals::ISignalDescription* val) { pOnForcedExecute_DEPRECATED = val; }
		void SetOnForcedExecuteComplete_DEPRECATED(const AISignals::ISignalDescription* val) { pOnForcedExecuteComplete_DEPRECATED = val; }
		void SetOnFormationPointReached_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFormationPointReached_DEPRECATED = val; }
		void SetOnKeepEnabled_DEPRECATED(const AISignals::ISignalDescription* val) { pOnKeepEnabled_DEPRECATED = val; }
		void SetOnLeaderActionCompleted_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLeaderActionCompleted_DEPRECATED = val; }
		void SetOnLeaderActionFailed_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLeaderActionFailed_DEPRECATED = val; }
		void SetOnLeaderAssigned_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLeaderAssigned_DEPRECATED = val; }
		void SetOnLeaderDeassigned_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLeaderDeassigned_DEPRECATED = val; }
		void SetOnLeaderDied_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLeaderDied_DEPRECATED = val; }
		void SetOnLeaderTooFar_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLeaderTooFar_DEPRECATED = val; }
		void SetOnLeftLean_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLeftLean_DEPRECATED = val; }
		void SetOnLowHideSpot_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLowHideSpot_DEPRECATED = val; }
		void SetOnNavTypeChanged_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNavTypeChanged_DEPRECATED = val; }
		void SetOnNoAimPosture_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNoAimPosture_DEPRECATED = val; }
		void SetOnNoFormationPoint_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNoFormationPoint_DEPRECATED = val; }
		void SetOnNoGroupTarget_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNoGroupTarget_DEPRECATED = val; }
		void SetOnNoPathFound_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNoPathFound_DEPRECATED = val; }
		void SetOnNoPeekPosture_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNoPeekPosture_DEPRECATED = val; }
		void SetOnNotBehind_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNotBehind_DEPRECATED = val; }
		void SetOnOrdSearch_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrdSearch_DEPRECATED = val; }
		void SetOnOrderAcquireTarget_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderAcquireTarget_DEPRECATED = val; }
		void SetOnOrderAttack_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderAttack_DEPRECATED = val; }
		void SetOnOrderDone_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderDone_DEPRECATED = val; }
		void SetOnOrderFail_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderFail_DEPRECATED = val; }
		void SetOnOrderSearch_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderSearch_DEPRECATED = val; }
		void SetOnPathFindAtStart_DEPRECATED(const AISignals::ISignalDescription* val) { pOnPathFindAtStart_DEPRECATED = val; }
		void SetOnPathFound_DEPRECATED(const AISignals::ISignalDescription* val) { pOnPathFound_DEPRECATED = val; }
		void SetOnRPTLeaderDead_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRPTLeaderDead_DEPRECATED = val; }
		void SetOnRequestReinforcementTriggered_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRequestReinforcementTriggered_DEPRECATED = val; }
		void SetOnRequestUpdate_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRequestUpdate_DEPRECATED = val; }
		void SetOnRequestUpdateAlternative_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRequestUpdateAlternative_DEPRECATED = val; }
		void SetOnRequestUpdateTowards_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRequestUpdateTowards_DEPRECATED = val; }
		void SetOnRightLean_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRightLean_DEPRECATED = val; }
		void SetOnSameHidespotAgain_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSameHidespotAgain_DEPRECATED = val; }
		void SetOnScaleFormation_DEPRECATED(const AISignals::ISignalDescription* val) { pOnScaleFormation_DEPRECATED = val; }
		void SetOnSetMinDistanceToTarget_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSetMinDistanceToTarget_DEPRECATED = val; }
		void SetOnSetUnitProperties_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSetUnitProperties_DEPRECATED = val; }
		void SetOnSpecialAction_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSpecialAction_DEPRECATED = val; }
		void SetOnSpecialActionDone_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSpecialActionDone_DEPRECATED = val; }
		void SetOnStartFollowPath_DEPRECATED(const AISignals::ISignalDescription* val) { pOnStartFollowPath_DEPRECATED = val; }
		void SetOnStartForceFire_DEPRECATED(const AISignals::ISignalDescription* val) { pOnStartForceFire_DEPRECATED = val; }
		void SetOnStopFollowPath_DEPRECATED(const AISignals::ISignalDescription* val) { pOnStopFollowPath_DEPRECATED = val; }
		void SetOnStopForceFire_DEPRECATED(const AISignals::ISignalDescription* val) { pOnStopForceFire_DEPRECATED = val; }
		void SetOnSurpriseAction_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSurpriseAction_DEPRECATED = val; }
		void SetOnTPSDestFound_DEPRECATED(const AISignals::ISignalDescription* val) { pOnTPSDestFound_DEPRECATED = val; }
		void SetOnTPSDestNotFound_DEPRECATED(const AISignals::ISignalDescription* val) { pOnTPSDestNotFound_DEPRECATED = val; }
		void SetOnTPSDestReached_DEPRECATED(const AISignals::ISignalDescription* val) { pOnTPSDestReached_DEPRECATED = val; }
		void SetOnTargetNavTypeChanged_DEPRECATED(const AISignals::ISignalDescription* val) { pOnTargetNavTypeChanged_DEPRECATED = val; }
		void SetOnUnitBusy_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUnitBusy_DEPRECATED = val; }
		void SetOnUnitDamaged_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUnitDamaged_DEPRECATED = val; }
		void SetOnUnitDied_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUnitDied_DEPRECATED = val; }
		void SetOnUnitMoving_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUnitMoving_DEPRECATED = val; }
		void SetOnUnitResumed_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUnitResumed_DEPRECATED = val; }
		void SetOnUnitStop_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUnitStop_DEPRECATED = val; }
		void SetOnUnitSuspended_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUnitSuspended_DEPRECATED = val; }
		void SetOnVehicleDanger_DEPRECATED(const AISignals::ISignalDescription* val) { pOnVehicleDanger_DEPRECATED = val; }
		void SetOnAIAgressive_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAIAgressive_DEPRECATED = val; }
		void SetOnAlertStatus_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAlertStatus_DEPRECATED = val; }
		void SetOnAtCloseRange_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAtCloseRange_DEPRECATED = val; }
		void SetOnAtOptimalRange_DEPRECATED(const AISignals::ISignalDescription* val) { pOnAtOptimalRange_DEPRECATED = val; }
		void SetOnBodyFallSound_DEPRECATED(const AISignals::ISignalDescription* val) { pOnBodyFallSound_DEPRECATED = val; }
		void SetOnChangeSetEnd_DEPRECATED(const AISignals::ISignalDescription* val) { pOnChangeSetEnd_DEPRECATED = val; }
		void SetOnCheckNextWeaponAccessory_DEPRECATED(const AISignals::ISignalDescription* val) { pOnCheckNextWeaponAccessory_DEPRECATED = val; }
		void SetOnCommandTold_DEPRECATED(const AISignals::ISignalDescription* val) { pOnCommandTold_DEPRECATED = val; }
		void SetOnControllVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnControllVehicle_DEPRECATED = val; }
		void SetOnDamage_DEPRECATED(const AISignals::ISignalDescription* val) { pOnDamage_DEPRECATED = val; }
		void SetOnDeadMemberSpottedBySomeoneElse_DEPRECATED(const AISignals::ISignalDescription* val) { pOnDeadMemberSpottedBySomeoneElse_DEPRECATED = val; }
		void SetOnDisableFire_DEPRECATED(const AISignals::ISignalDescription* val) { pOnDisableFire = val; }
		void SetOnDoExitVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnDoExitVehicle_DEPRECATED = val; }
		void SetOnDriverEntered_DEPRECATED(const AISignals::ISignalDescription* val) { pOnDriverEntered_DEPRECATED = val; }
		void SetOnDriverOut_DEPRECATED(const AISignals::ISignalDescription* val) { pOnDriverOut_DEPRECATED = val; }
		void SetOnDroneSeekCommand_DEPRECATED(const AISignals::ISignalDescription* val) { pOnDroneSeekCommand_DEPRECATED = val; }
		void SetOnDropped_DEPRECATED(const AISignals::ISignalDescription* val) { pOnDropped_DEPRECATED = val; }
		void SetOnEMPGrenadeThrownInGroup_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEMPGrenadeThrownInGroup_DEPRECATED = val; }
		void SetOnEnableAlertStatus_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEnableAlertStatus_DEPRECATED = val; }
		void SetOnEnemyDamage_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEnemyDamage_DEPRECATED = val; }
		void SetOnEnemyDied_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEnemyDied_DEPRECATED = val; }
		void SetOnEnteredVehicleGunner_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEnteredVehicleGunner_DEPRECATED = val; }
		void SetOnEnteredVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEnteredVehicle_DEPRECATED = val; }
		void SetOnEnteringEnd_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEnteringEnd_DEPRECATED = val; }
		void SetOnEnteringVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnEnteringVehicle_DEPRECATED = val; }
		void SetOnExitVehicleStand_DEPRECATED(const AISignals::ISignalDescription* val) { pOnExitVehicleStand_DEPRECATED = val; }
		void SetOnExitedVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnExitedVehicle_DEPRECATED = val; }
		void SetOnExitingEnd_DEPRECATED(const AISignals::ISignalDescription* val) { pOnExitingEnd_DEPRECATED = val; }
		void SetOnFlashGrenadeThrownInGroup_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFlashGrenadeThrownInGroup_DEPRECATED = val; }
		void SetOnFollowLeader_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFollowLeader_DEPRECATED = val; }
		void SetOnFragGrenadeThrownInGroup_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFragGrenadeThrownInGroup_DEPRECATED = val; }
		void SetOnFriendlyDamageByPlayer_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFriendlyDamageByPlayer_DEPRECATED = val; }
		void SetOnFriendlyDamage_DEPRECATED(const AISignals::ISignalDescription* val) { pOnFriendlyDamage_DEPRECATED = val; }
		void SetOnGoToGrabbed_DEPRECATED(const AISignals::ISignalDescription* val) { pOnGoToGrabbed_DEPRECATED = val; }
		void SetOnGoToSeek_DEPRECATED(const AISignals::ISignalDescription* val) { pOnGoToSeek_DEPRECATED = val; }
		void SetOnGroupMemberDiedOnAGL_DEPRECATED(const AISignals::ISignalDescription* val) { pOnGroupMemberDiedOnAGL_DEPRECATED = val; }
		void SetOnGroupMemberDiedOnHMG_DEPRECATED(const AISignals::ISignalDescription* val) { pOnGroupMemberDiedOnHMG_DEPRECATED = val; }
		void SetOnGroupMemberDiedOnMountedWeapon_DEPRECATED(const AISignals::ISignalDescription* val) { pOnGroupMemberDiedOnMountedWeapon_DEPRECATED = val; }
		void SetOnGunnerLostTarget_DEPRECATED(const AISignals::ISignalDescription* val) { pOnGunnerLostTarget_DEPRECATED = val; }
		void SetOnInVehicleFoundTarget_DEPRECATED(const AISignals::ISignalDescription* val) { pOnInVehicleFoundTarget_DEPRECATED = val; }
		void SetOnInVehicleRequestStartFire_DEPRECATED(const AISignals::ISignalDescription* val) { pOnInVehicleRequestStartFire_DEPRECATED = val; }
		void SetOnInVehicleRequestStopFire_DEPRECATED(const AISignals::ISignalDescription* val) { pOnInVehicleRequestStopFire_DEPRECATED = val; }
		void SetOnIncomingFire_DEPRECATED(const AISignals::ISignalDescription* val) { pOnIncomingFire_DEPRECATED = val; }
		void SetOnJoinTeam_DEPRECATED(const AISignals::ISignalDescription* val) { pOnJoinTeam_DEPRECATED = val; }
		void SetOnJustConstructed_DEPRECATED(const AISignals::ISignalDescription* val) { pOnJustConstructed_DEPRECATED = val; }
		void SetOnLeaveMountedWeapon_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLeaveMountedWeapon_DEPRECATED = val; }
		void SetOnLowAmmoFinished_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLowAmmoFinished_DEPRECATED = val; }
		void SetOnLowAmmoStart_DEPRECATED(const AISignals::ISignalDescription* val) { pOnLowAmmoStart_DEPRECATED = val; }
		void SetOnNearbyWaterRippleSeen_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNearbyWaterRippleSeen_DEPRECATED = val; }
		void SetOnNewSpawn_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNewSpawn_DEPRECATED = val; }
		void SetOnNoSearchSpotsAvailable_DEPRECATED(const AISignals::ISignalDescription* val) { pOnNoSearchSpotsAvailable_DEPRECATED = val; }
		void SetOnOrderExitVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderExitVehicle_DEPRECATED = val; }
		void SetOnOrderFollow_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderFollow_DEPRECATED = val; }
		void SetOnOrderLeaveVehicle_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderLeaveVehicle_DEPRECATED = val; }
		void SetOnOrderMove_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderMove_DEPRECATED = val; }
		void SetOnOrderUse_DEPRECATED(const AISignals::ISignalDescription* val) { pOnOrderUse_DEPRECATED = val; }
		void SetOnPlayerDied_DEPRECATED(const AISignals::ISignalDescription* val) { pOnPlayerDied_DEPRECATED = val; }
		void SetOnPlayerHit_DEPRECATED(const AISignals::ISignalDescription* val) { pOnPlayerHit_DEPRECATED = val; }
		void SetOnPlayerNiceShot_DEPRECATED(const AISignals::ISignalDescription* val) { pOnPlayerNiceShot_DEPRECATED = val; }
		void SetOnPlayerTeamKill_DEPRECATED(const AISignals::ISignalDescription* val) { pOnPlayerTeamKill_DEPRECATED = val; }
		void SetOnPrepareForMountedWeaponUse_DEPRECATED(const AISignals::ISignalDescription* val) { pOnPrepareForMountedWeaponUse_DEPRECATED = val; }
		void SetOnRefPointReached_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRefPointReached_DEPRECATED = val; }
		void SetOnRelocate_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRelocate_DEPRECATED = val; }
		void SetOnRequestJoinTeam_DEPRECATED(const AISignals::ISignalDescription* val) { pOnRequestJoinTeam_DEPRECATED = val; }
		void SetOnResetAssignment_DEPRECATED(const AISignals::ISignalDescription* val) { pOnResetAssignment_DEPRECATED = val; }
		void SetOnSharedUseThisMountedWeapon_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSharedUseThisMountedWeapon_DEPRECATED = val; }
		void SetOnSmokeGrenadeThrownInGroup_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSmokeGrenadeThrownInGroup_DEPRECATED = val; }
		void SetOnSomebodyDied_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSomebodyDied_DEPRECATED = val; }
		void SetOnSpawn_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSpawn_DEPRECATED = val; }
		void SetOnSquadmateDied_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSquadmateDied_DEPRECATED = val; }
		void SetOnStopAndExit_DEPRECATED(const AISignals::ISignalDescription* val) { pOnStopAndExit_DEPRECATED = val; }
		void SetOnSuspiciousActivity_DEPRECATED(const AISignals::ISignalDescription* val) { pOnSuspiciousActivity_DEPRECATED = val; }
		void SetOnTargetLost_DEPRECATED(const AISignals::ISignalDescription* val) { pOnTargetLost_DEPRECATED = val; }
		void SetOnTargetNotVisible_DEPRECATED(const AISignals::ISignalDescription* val) { pOnTargetNotVisible_DEPRECATED = val; }
		void SetOnTooFarFromWeapon_DEPRECATED(const AISignals::ISignalDescription* val) { pOnTooFarFromWeapon_DEPRECATED = val; }
		void SetOnUnloadDone_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUnloadDone_DEPRECATED = val; }
		void SetOnUseMountedWeapon2_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUseMountedWeapon2_DEPRECATED = val; }
		void SetOnUseMountedWeapon_DEPRECATED(const AISignals::ISignalDescription* val) { pOnUseMountedWeapon_DEPRECATED = val; }

private:
	// ESignalTag::NONE
	const ISignalDescription* pNone;

	// ESignalTag::GAME_SDK
	const ISignalDescription* pOnActJoinFormation;
	const ISignalDescription* pOnAloneSignal;
	const ISignalDescription* pOnAssignedSearchSpotSeen;
	const ISignalDescription* pOnAssignedSearchSpotSeenBySomeoneElse;
	const ISignalDescription* pOnEnableFire;
	const ISignalDescription* pOnEnterSignal;
	const ISignalDescription* pOnFallAndPlay;
	const ISignalDescription* pOnFinishedHitDeathReaction;
	const ISignalDescription* pOnGrabbedByPlayer;
	const ISignalDescription* pOnGroupMemberDied;
	const ISignalDescription* pOnGroupMemberEnteredCombat;
	const ISignalDescription* pOnGroupMemberGrabbedByPlayer;
	const ISignalDescription* pOnHitDeathReactionInterrupted;
	const ISignalDescription* pOnHitMountedGun;
	const ISignalDescription* pOnIncomingProjectile;
	const ISignalDescription* pOnInTargetFov;
	const ISignalDescription* pOnLeaveSignal;
	const ISignalDescription* pOnMeleePerformed;
	const ISignalDescription* pOnMeleeKnockedDownTarget;
	const ISignalDescription* pOnNotAloneSignal;
	const ISignalDescription* pOnNotInTargetFov;
	const ISignalDescription* pOnNotVisibleFromTarget;
	const ISignalDescription* pOnReload;
	const ISignalDescription* pOnReloadDone;
	const ISignalDescription* pOnSawObjectMove;
	const ISignalDescription* pOnSpottedDeadGroupMember;
	const ISignalDescription* pOnTargetSearchSpotSeen;
	const ISignalDescription* pOnTargetedByTurret;
	const ISignalDescription* pOnThrownObjectSeen;
	const ISignalDescription* pOnTurretHasBeenDestroyed;
	const ISignalDescription* pOnTurretHasBeenDestroyedByThePlayer;
	const ISignalDescription* pOnVisibleFromTarget;
	const ISignalDescription* pOnWaterRippleSeen;
	const ISignalDescription* pOnWatchMeDie;

	//ESignalTag::CRY_ENGINE
	const ISignalDescription* pOnActionCreated;
	const ISignalDescription* pOnActionDone;
	const ISignalDescription* pOnActionEnd;
	const ISignalDescription* pOnActionStart;
	const ISignalDescription* pOnAttentionTargetThreatChanged;
	const ISignalDescription* pOnBulletRain;
	const ISignalDescription* pOnCloakedTargetSeen;
	const ISignalDescription* pOnCloseCollision;
	const ISignalDescription* pOnCloseContact;
	const ISignalDescription* pOnCoverCompromised;
	const ISignalDescription* pOnEMPGrenadeThrown;
	const ISignalDescription* pOnEnemyHeard;
	const ISignalDescription* pOnEnemyMemory;
	const ISignalDescription* pOnEnemySeen;
	const ISignalDescription* pOnEnterCover;
	const ISignalDescription* pOnEnterNavSO;
	const ISignalDescription* pOnExposedToExplosion;
	const ISignalDescription* pOnExposedToFlashBang;
	const ISignalDescription* pOnExposedToSmoke;
	const ISignalDescription* pOnFlashbangGrenadeThrown;
	const ISignalDescription* pOnFragGrenadeThrown;
	const ISignalDescription* pOnGrenadeDanger;
	const ISignalDescription* pOnGrenadeThrown;
	const ISignalDescription* pOnGroupChanged;
	const ISignalDescription* pOnGroupMemberMutilated;
	const ISignalDescription* pOnGroupTarget;
	const ISignalDescription* pOnGroupTargetMemory;
	const ISignalDescription* pOnGroupTargetNone;
	const ISignalDescription* pOnGroupTargetSound;
	const ISignalDescription* pOnGroupTargetVisual;
	const ISignalDescription* pOnInterestingSoundHeard;
	const ISignalDescription* pOnLeaveCover;
	const ISignalDescription* pOnLeaveNavSO;
	const ISignalDescription* pOnLostSightOfTarget;
	const ISignalDescription* pOnLowAmmo;
	const ISignalDescription* pOnMeleeExecuted;
	const ISignalDescription* pOnMemoryMoved;
	const ISignalDescription* pOnMovementPlanProduced;
	const ISignalDescription* pOnMovingInCover;
	const ISignalDescription* pOnMovingToCover;
	const ISignalDescription* pOnNewAttentionTarget;
	const ISignalDescription* pOnNoTarget;
	const ISignalDescription* pOnNoTargetAwareness;
	const ISignalDescription* pOnNoTargetVisible;
	const ISignalDescription* pOnObjectSeen;
	const ISignalDescription* pOnOutOfAmmo;
	const ISignalDescription* pOnPlayerGoingAway;
	const ISignalDescription* pOnPlayerLooking;
	const ISignalDescription* pOnPlayerLookingAway;
	const ISignalDescription* pOnPlayerSticking;
	const ISignalDescription* pOnReloaded;
	const ISignalDescription* pOnSeenByEnemy;
	const ISignalDescription* pOnShapeDisabled;
	const ISignalDescription* pOnShapeEnabled;
	const ISignalDescription* pOnSmokeGrenadeThrown;
	const ISignalDescription* pOnSomethingSeen;
	const ISignalDescription* pOnSuspectedSeen;
	const ISignalDescription* pOnSuspectedSoundHeard;
	const ISignalDescription* pOnTargetApproaching;
	const ISignalDescription* pOnTargetArmoredHit;
	const ISignalDescription* pOnTargetCloaked;
	const ISignalDescription* pOnTargetDead;
	const ISignalDescription* pOnTargetFleeing;
	const ISignalDescription* pOnTargetInTerritory;
	const ISignalDescription* pOnTargetOutOfTerritory;
	const ISignalDescription* pOnTargetStampMelee;
	const ISignalDescription* pOnTargetTooClose;
	const ISignalDescription* pOnTargetTooFar;
	const ISignalDescription* pOnTargetUncloaked;
	const ISignalDescription* pOnThreateningSeen;
	const ISignalDescription* pOnThreateningSoundHeard;
	const ISignalDescription* pOnUpdateItems;
	const ISignalDescription* pOnUseSmartObject;
	const ISignalDescription* pOnWeaponEndBurst;
	const ISignalDescription* pOnWeaponMelee;
	const ISignalDescription* pOnWeaponShoot;

	// ESignalTag::DEPRECATED
	const ISignalDescription* pOnAbortAction_DEPRECATED;
	const ISignalDescription* pOnActAlerted_DEPRECATED;
	const ISignalDescription* pOnActAnim_DEPRECATED;
	const ISignalDescription* pOnActAnimex_DEPRECATED;
	const ISignalDescription* pOnActChaseTarget_DEPRECATED;
	const ISignalDescription* pOnActDialog_DEPRECATED;
	const ISignalDescription* pOnActDialogOver_DEPRECATED;
	const ISignalDescription* pOnActDropObject_DEPRECATED;
	const ISignalDescription* pOnActDummy_DEPRECATED;
	const ISignalDescription* pOnActEnterVehicle_DEPRECATED;
	const ISignalDescription* pOnActExecute_DEPRECATED;
	const ISignalDescription* pOnActExitVehicle_DEPRECATED;
	const ISignalDescription* pOnActFollowPath_DEPRECATED;
	const ISignalDescription* pOnActUseObject_DEPRECATED;
	const ISignalDescription* pOnActVehicleStickPath_DEPRECATED;
	const ISignalDescription* pOnAddDangerPoint_DEPRECATED;
	const ISignalDescription* pOnAttackShootSpot_DEPRECATED;
	const ISignalDescription* pOnAttackSwitchPosition_DEPRECATED;
	const ISignalDescription* pOnAvoidDanger_DEPRECATED;
	const ISignalDescription* pOnBackOffFailed_DEPRECATED;
	const ISignalDescription* pOnBehind_DEPRECATED;
	const ISignalDescription* pOnCallReinforcements_DEPRECATED;
	const ISignalDescription* pOnChangeStance_DEPRECATED;
	const ISignalDescription* pOnChargeBailOut_DEPRECATED;
	const ISignalDescription* pOnChargeHit_DEPRECATED;
	const ISignalDescription* pOnChargeMiss_DEPRECATED;
	const ISignalDescription* pOnChargeStart_DEPRECATED;
	const ISignalDescription* pOnCheckDeadBody_DEPRECATED;
	const ISignalDescription* pOnCheckDeadTarget_DEPRECATED;
	const ISignalDescription* pOnClearSpotList_DEPRECATED;
	const ISignalDescription* pOnCombatTargetDisabled_DEPRECATED;
	const ISignalDescription* pOnCombatTargetEnabled_DEPRECATED;
	const ISignalDescription* pOnCoverReached_DEPRECATED;
	const ISignalDescription* pOnEndPathOffset_DEPRECATED;
	const ISignalDescription* pOnEndVehicleDanger_DEPRECATED;
	const ISignalDescription* pOnExecutingSpecialAction_DEPRECATED;
	const ISignalDescription* pOnFireDisabled_DEPRECATED;
	const ISignalDescription* pOnFiringAllowed_DEPRECATED;
	const ISignalDescription* pOnFiringNotAllowed_DEPRECATED;
	const ISignalDescription* pOnForcedExecute_DEPRECATED;
	const ISignalDescription* pOnForcedExecuteComplete_DEPRECATED;
	const ISignalDescription* pOnFormationPointReached_DEPRECATED;
	const ISignalDescription* pOnKeepEnabled_DEPRECATED;
	const ISignalDescription* pOnLeaderActionCompleted_DEPRECATED;
	const ISignalDescription* pOnLeaderActionFailed_DEPRECATED;
	const ISignalDescription* pOnLeaderAssigned_DEPRECATED;
	const ISignalDescription* pOnLeaderDeassigned_DEPRECATED;
	const ISignalDescription* pOnLeaderDied_DEPRECATED;
	const ISignalDescription* pOnLeaderTooFar_DEPRECATED;
	const ISignalDescription* pOnLeftLean_DEPRECATED;
	const ISignalDescription* pOnLowHideSpot_DEPRECATED;
	const ISignalDescription* pOnNavTypeChanged_DEPRECATED;
	const ISignalDescription* pOnNoAimPosture_DEPRECATED;
	const ISignalDescription* pOnNoFormationPoint_DEPRECATED;
	const ISignalDescription* pOnNoGroupTarget_DEPRECATED;
	const ISignalDescription* pOnNoPathFound_DEPRECATED;
	const ISignalDescription* pOnNoPeekPosture_DEPRECATED;
	const ISignalDescription* pOnNotBehind_DEPRECATED;
	const ISignalDescription* pOnOrdSearch_DEPRECATED;
	const ISignalDescription* pOnOrderAcquireTarget_DEPRECATED;
	const ISignalDescription* pOnOrderAttack_DEPRECATED;
	const ISignalDescription* pOnOrderDone_DEPRECATED;
	const ISignalDescription* pOnOrderFail_DEPRECATED;
	const ISignalDescription* pOnOrderSearch_DEPRECATED;
	const ISignalDescription* pOnPathFindAtStart_DEPRECATED;
	const ISignalDescription* pOnPathFound_DEPRECATED;
	const ISignalDescription* pOnRPTLeaderDead_DEPRECATED;
	const ISignalDescription* pOnRequestReinforcementTriggered_DEPRECATED;
	const ISignalDescription* pOnRequestUpdate_DEPRECATED;
	const ISignalDescription* pOnRequestUpdateAlternative_DEPRECATED;
	const ISignalDescription* pOnRequestUpdateTowards_DEPRECATED;
	const ISignalDescription* pOnRightLean_DEPRECATED;
	const ISignalDescription* pOnSameHidespotAgain_DEPRECATED;
	const ISignalDescription* pOnScaleFormation_DEPRECATED;
	const ISignalDescription* pOnSetMinDistanceToTarget_DEPRECATED;
	const ISignalDescription* pOnSetUnitProperties_DEPRECATED;
	const ISignalDescription* pOnSpecialAction_DEPRECATED;
	const ISignalDescription* pOnSpecialActionDone_DEPRECATED;
	const ISignalDescription* pOnStartFollowPath_DEPRECATED;
	const ISignalDescription* pOnStartForceFire_DEPRECATED;
	const ISignalDescription* pOnStopFollowPath_DEPRECATED;
	const ISignalDescription* pOnStopForceFire_DEPRECATED;
	const ISignalDescription* pOnSurpriseAction_DEPRECATED;
	const ISignalDescription* pOnTPSDestFound_DEPRECATED;
	const ISignalDescription* pOnTPSDestNotFound_DEPRECATED;
	const ISignalDescription* pOnTPSDestReached_DEPRECATED;
	const ISignalDescription* pOnTargetNavTypeChanged_DEPRECATED;
	const ISignalDescription* pOnUnitBusy_DEPRECATED;
	const ISignalDescription* pOnUnitDamaged_DEPRECATED;
	const ISignalDescription* pOnUnitDied_DEPRECATED;
	const ISignalDescription* pOnUnitMoving_DEPRECATED;
	const ISignalDescription* pOnUnitResumed_DEPRECATED;
	const ISignalDescription* pOnUnitStop_DEPRECATED;
	const ISignalDescription* pOnUnitSuspended_DEPRECATED;
	const ISignalDescription* pOnVehicleDanger_DEPRECATED;

		// Used in Lua scripts
	const ISignalDescription* pOnAIAgressive_DEPRECATED;
	const ISignalDescription* pOnAlertStatus_DEPRECATED;
	const ISignalDescription* pOnAtCloseRange_DEPRECATED;
	const ISignalDescription* pOnAtOptimalRange_DEPRECATED;
	const ISignalDescription* pOnBodyFallSound_DEPRECATED;
	const ISignalDescription* pOnChangeSetEnd_DEPRECATED;
	const ISignalDescription* pOnCheckNextWeaponAccessory_DEPRECATED;
	const ISignalDescription* pOnCommandTold_DEPRECATED;
	const ISignalDescription* pOnControllVehicle_DEPRECATED;
	const ISignalDescription* pOnDamage_DEPRECATED;
	const ISignalDescription* pOnDeadMemberSpottedBySomeoneElse_DEPRECATED;
	const ISignalDescription* pOnDisableFire;
	const ISignalDescription* pOnDoExitVehicle_DEPRECATED;
	const ISignalDescription* pOnDriverEntered_DEPRECATED;
	const ISignalDescription* pOnDriverOut_DEPRECATED;
	const ISignalDescription* pOnDroneSeekCommand_DEPRECATED;
	const ISignalDescription* pOnDropped_DEPRECATED;
	const ISignalDescription* pOnEMPGrenadeThrownInGroup_DEPRECATED;
	const ISignalDescription* pOnEnableAlertStatus_DEPRECATED;
	const ISignalDescription* pOnEnemyDamage_DEPRECATED;
	const ISignalDescription* pOnEnemyDied_DEPRECATED;
	const ISignalDescription* pOnEnteredVehicleGunner_DEPRECATED;
	const ISignalDescription* pOnEnteredVehicle_DEPRECATED;
	const ISignalDescription* pOnEnteringEnd_DEPRECATED;
	const ISignalDescription* pOnEnteringVehicle_DEPRECATED;
	const ISignalDescription* pOnExitVehicleStand_DEPRECATED;
	const ISignalDescription* pOnExitedVehicle_DEPRECATED;
	const ISignalDescription* pOnExitingEnd_DEPRECATED;
	const ISignalDescription* pOnFlashGrenadeThrownInGroup_DEPRECATED;
	const ISignalDescription* pOnFollowLeader_DEPRECATED;
	const ISignalDescription* pOnFragGrenadeThrownInGroup_DEPRECATED;
	const ISignalDescription* pOnFriendlyDamageByPlayer_DEPRECATED;
	const ISignalDescription* pOnFriendlyDamage_DEPRECATED;
	const ISignalDescription* pOnGoToGrabbed_DEPRECATED;
	const ISignalDescription* pOnGoToSeek_DEPRECATED;
	const ISignalDescription* pOnGroupMemberDiedOnAGL_DEPRECATED;
	const ISignalDescription* pOnGroupMemberDiedOnHMG_DEPRECATED;
	const ISignalDescription* pOnGroupMemberDiedOnMountedWeapon_DEPRECATED;
	const ISignalDescription* pOnGunnerLostTarget_DEPRECATED;
	const ISignalDescription* pOnInVehicleFoundTarget_DEPRECATED;
	const ISignalDescription* pOnInVehicleRequestStartFire_DEPRECATED;
	const ISignalDescription* pOnInVehicleRequestStopFire_DEPRECATED;
	const ISignalDescription* pOnIncomingFire_DEPRECATED;
	const ISignalDescription* pOnJoinTeam_DEPRECATED;
	const ISignalDescription* pOnJustConstructed_DEPRECATED;
	const ISignalDescription* pOnLeaveMountedWeapon_DEPRECATED;
	const ISignalDescription* pOnLowAmmoFinished_DEPRECATED;
	const ISignalDescription* pOnLowAmmoStart_DEPRECATED;
	const ISignalDescription* pOnNearbyWaterRippleSeen_DEPRECATED;
	const ISignalDescription* pOnNewSpawn_DEPRECATED;
	const ISignalDescription* pOnNoSearchSpotsAvailable_DEPRECATED;
	const ISignalDescription* pOnOrderExitVehicle_DEPRECATED;
	const ISignalDescription* pOnOrderFollow_DEPRECATED;
	const ISignalDescription* pOnOrderLeaveVehicle_DEPRECATED;
	const ISignalDescription* pOnOrderMove_DEPRECATED;
	const ISignalDescription* pOnOrderUse_DEPRECATED;
	const ISignalDescription* pOnPlayerDied_DEPRECATED;
	const ISignalDescription* pOnPlayerHit_DEPRECATED;
	const ISignalDescription* pOnPlayerNiceShot_DEPRECATED;
	const ISignalDescription* pOnPlayerTeamKill_DEPRECATED;
	const ISignalDescription* pOnPrepareForMountedWeaponUse_DEPRECATED;
	const ISignalDescription* pOnRefPointReached_DEPRECATED;
	const ISignalDescription* pOnRelocate_DEPRECATED;
	const ISignalDescription* pOnRequestJoinTeam_DEPRECATED;
	const ISignalDescription* pOnResetAssignment_DEPRECATED;
	const ISignalDescription* pOnSharedUseThisMountedWeapon_DEPRECATED;
	const ISignalDescription* pOnSmokeGrenadeThrownInGroup_DEPRECATED;
	const ISignalDescription* pOnSomebodyDied_DEPRECATED;
	const ISignalDescription* pOnSpawn_DEPRECATED;
	const ISignalDescription* pOnSquadmateDied_DEPRECATED;
	const ISignalDescription* pOnStopAndExit_DEPRECATED;
	const ISignalDescription* pOnSuspiciousActivity_DEPRECATED;
	const ISignalDescription* pOnTargetLost_DEPRECATED;
	const ISignalDescription* pOnTargetNotVisible_DEPRECATED;
	const ISignalDescription* pOnTooFarFromWeapon_DEPRECATED;
	const ISignalDescription* pOnUnloadDone_DEPRECATED;
	const ISignalDescription* pOnUseMountedWeapon2_DEPRECATED;
	const ISignalDescription* pOnUseMountedWeapon_DEPRECATED;
	};

	class ISignal
	{
	public:
		virtual const int GetNSignal() const = 0;
		virtual const ISignalDescription& GetSignalDescription() const = 0;
		virtual const EntityId GetSenderID() const = 0;
		virtual IAISignalExtraData* GetExtraData() = 0;

		virtual void SetExtraData(IAISignalExtraData* val) = 0;
		virtual bool HasSameDescription(const ISignal* other) const = 0;
		virtual void Serialize(TSerialize ser) = 0;
	protected:
		~ISignal() {}
	};

	typedef std::shared_ptr<ISignal> SignalSharedPtr;

	// Manages the creation of signals using built-in or game signals descriptions
	// - All built-in signals descriptions are initialized upon construction
	// - Game signals descriptions can be registered by calling RegisterGameSignalDescription
	// Signal Manager should only be created once from the AI system and accessed through gEnv/gAIEnv
	// This class doesn't have a member GetSignalDescription(const char* szSignalDescName) so that we force the user
	// to keep track of the Signal Descriptions that he/she creates and can retrieve them using the crc
	class ISignalManager
	{
	public:
		virtual const SBuiltInSignalsDescriptions& GetBuiltInSignalDescriptions() const = 0;

		virtual size_t                             GetSignalDescriptionsCount() const = 0;
		virtual const ISignalDescription&          GetSignalDescription(const size_t index) const = 0;
		virtual const ISignalDescription&          GetSignalDescription(const char* szSignalDescName) const = 0;

		virtual const ISignalDescription&          RegisterGameSignalDescription(const char* szSignalName) = 0;
		virtual void                               DeregisterGameSignalDescription(const ISignalDescription& signalDescription) = 0;

#ifdef SIGNAL_MANAGER_DEBUG
		virtual SignalSharedPtr                    CreateSignal(const int nSignal, const ISignalDescription& signalDescription, const EntityId senderID = INVALID_ENTITYID, IAISignalExtraData* pEData = nullptr) = 0;
#else
		virtual SignalSharedPtr                    CreateSignal(const int nSignal, const ISignalDescription& signalDescription, const EntityId senderID = INVALID_ENTITYID, IAISignalExtraData* pEData = nullptr) const = 0;
#endif //SIGNAL_MANAGER_DEBUG

		virtual SignalSharedPtr                    CreateSignal_DEPRECATED(const int nSignal, const char* szCustomSignalTypeName, const EntityId senderID = INVALID_ENTITYID, IAISignalExtraData* pEData = nullptr) = 0;

#ifdef SIGNAL_MANAGER_DEBUG
		virtual void                               DebugDrawSignalsHistory() const = 0;
#endif //SIGNAL_MANAGER_DEBUG

	protected:
		~ISignalManager() {}
	};
}