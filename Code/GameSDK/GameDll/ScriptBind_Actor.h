// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Exposes actor functionality to LUA
  
 -------------------------------------------------------------------------
  History:
  - 7:10:2004   14:19 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_ACTOR_H__
#define __SCRIPTBIND_ACTOR_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>


struct IGameFramework;
struct IActor;


// <title Actor>
// Syntax: Actor
class CScriptBind_Actor :
	public CScriptableBase
{
public:
	CScriptBind_Actor(ISystem *pSystem);
	virtual ~CScriptBind_Actor();

	void AttachTo(IActor *pActor);

	//------------------------------------------------------------------------
	int DumpActorInfo(IFunctionHandler *pH);

	int Revive(IFunctionHandler *pH);
	int Kill(IFunctionHandler *pH);
	int ShutDown(IFunctionHandler *pH);
	int SetParams(IFunctionHandler *pH);
	int GetHeadDir(IFunctionHandler *pH);
	int GetAimDir(IFunctionHandler *pH);

	int PostPhysicalize(IFunctionHandler *pH);
	int GetChannel(IFunctionHandler *pH);
	int IsPlayer(IFunctionHandler *pH);
	int IsLocalClient(IFunctionHandler *pH);
	int GetLinkedVehicleId(IFunctionHandler *pH);

	int LinkToEntity(IFunctionHandler *pH);
	int SetAngles(IFunctionHandler *pH,Ang3 vAngles );
	int PlayerSetViewAngles( IFunctionHandler *pH,Ang3 vAngles );
	int GetAngles(IFunctionHandler *pH);

	int SetMovementTarget(IFunctionHandler *pH, Vec3 pos, Vec3 target, Vec3 up, float speed);
	int CameraShake(IFunctionHandler *pH,float amount,float duration,float frequency,Vec3 pos);
	int SetViewShake(IFunctionHandler *pH, Ang3 shakeAngle, Vec3 shakeShift, float duration, float frequency, float randomness);

	int SetExtensionParams(IFunctionHandler* pH, const char *extension, SmartScriptTable params);

	int SvRefillAllAmmo(IFunctionHandler* pH, const char* refillType, bool refillAll, int grenadeCount, bool bRefillCurrentGrenadeType);
	int ClRefillAmmoResult(IFunctionHandler* pH, bool ammoRefilled);

	int SvGiveAmmoClips(IFunctionHandler* pH, int numClips);

	int IsImmuneToForbiddenArea(IFunctionHandler *pH);

	int SetHealth(IFunctionHandler *pH, float health);
	int SetMaxHealth(IFunctionHandler *pH, float health);
	int GetHealth(IFunctionHandler *pH);
	int GetMaxHealth(IFunctionHandler *pH);
	int DamageInfo(IFunctionHandler *pH, ScriptHandle shooter, ScriptHandle target, ScriptHandle weapon, ScriptHandle projectile, float damage, int damageType, Vec3 hitDirection);
	int GetLowHealthThreshold(IFunctionHandler *pH);

	int SetPhysicalizationProfile(IFunctionHandler *pH, const char *profile);
	int GetPhysicalizationProfile(IFunctionHandler *pH);

	int QueueAnimationState(IFunctionHandler *pH, const char *animationState);

	int CreateCodeEvent(IFunctionHandler *pH,SmartScriptTable params);

	int PauseAnimationGraph(IFunctionHandler* pH);
	int ResumeAnimationGraph(IFunctionHandler* pH);
	int HurryAnimationGraph(IFunctionHandler* pH);
	int SetTurnAnimationParams(IFunctionHandler* pH, const float turnThresholdAngle, const float turnThresholdTime);

	int SetSpectatorMode(IFunctionHandler *pH, int mode, ScriptHandle targetId);
	int GetSpectatorMode(IFunctionHandler *pH);
	int GetSpectatorState(IFunctionHandler *pH);
	int GetSpectatorTarget(IFunctionHandler* pH);

	int Fall(IFunctionHandler *pH, Vec3 hitPos);

	int StandUp(IFunctionHandler *pH);

	int GetExtraHitLocationInfo(IFunctionHandler *pH, int slot, int partId);

	int SetForcedLookDir(IFunctionHandler *pH, CScriptVector dir);
	int ClearForcedLookDir(IFunctionHandler *pH);
	int SetForcedLookObjectId(IFunctionHandler *pH, ScriptHandle objectId);
	int ClearForcedLookObjectId(IFunctionHandler *pH);

	int CanSpectacularKillOn(IFunctionHandler *pH, ScriptHandle targetId);
	int StartSpectacularKill(IFunctionHandler *pH, ScriptHandle targetId);
	int RegisterInAutoAimManager(IFunctionHandler *pH);
	int CheckBodyDamagePartFlags(IFunctionHandler *pH, int partID, int materialID, uint32 bodyPartFlagsMask);
	int GetBodyDamageProfileID(IFunctionHandler* pH, const char* bodyDamageFileName, const char* bodyDamagePartsFileName);
	int OverrideBodyDamageProfileID(IFunctionHandler* pH, const int bodyDamageProfileID);
	int IsGod(IFunctionHandler* pH);

	//------------------------------------------------------------------------
	// ITEM STUFF
	//------------------------------------------------------------------------
	int HolsterItem(IFunctionHandler *pH, bool holster);
	int DropItem(IFunctionHandler *pH, ScriptHandle itemId);
	int PickUpItem(IFunctionHandler *pH, ScriptHandle itemId);
	int IsCurrentItemHeavy( IFunctionHandler* pH );
	int PickUpPickableAmmo(IFunctionHandler *pH, const char *ammoName, int count);

	int SelectItemByName(IFunctionHandler *pH, const char *name);
	int SelectItem(IFunctionHandler *pH, ScriptHandle itemId, bool forceSelect);
	int SelectLastItem(IFunctionHandler *pH);
	int SelectNextItem(IFunctionHandler *pH, int direction, bool keepHistory, const char *category);
	int SimpleFindItemIdInCategory(IFunctionHandler *pH, const char *category);

	int DisableHitReaction(IFunctionHandler *pH);
	int EnableHitReaction(IFunctionHandler *pH);

	int CreateIKLimb( IFunctionHandler *pH, int slot, const char *limbName, const char *rootBone, const char *midBone, const char *endBone, int flags);

	int PlayAction(IFunctionHandler *pH, const char* action);

	//misc
	int RefreshPickAndThrowObjectPhysics( IFunctionHandler *pH );

	int AcquireOrReleaseLipSyncExtension(IFunctionHandler *pH);

protected:
	class CActor *GetActor(IFunctionHandler *pH);

	bool IsGrenadeClass(const IEntityClass* pEntityClass) const;
	bool RefillOrGiveGrenades(CActor& actor, IInventory& inventory, IEntityClass* pGrenadeClass, int grenadeCount);

	ISystem					*m_pSystem;
	IGameFramework	*m_pGameFW;
};

#endif //__SCRIPTBIND_ACTOR_H__
