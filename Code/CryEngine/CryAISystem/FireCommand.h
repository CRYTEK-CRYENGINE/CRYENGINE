// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   FireCommand.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 2 Mar 2009			 : Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/
#ifndef __FireCommand_H__
#define __FireCommand_H__

#include <CryAISystem/IAgent.h>

class CAIActor;
class CPuppet;

//====================================================================
// CFireCommandInstant
// Fire command handler for assault rifles and alike.
// When starting to fire the fire is drawn along a line towards the target.
//====================================================================
class CFireCommandInstant : public IFireCommandHandler
{
	friend class CAISystem; // need this for DebugDraw
public:
	CFireCommandInstant(IAIActor* pShooter);
	virtual const char*  GetName() { return "instant"; }

	virtual void         Reset();
	virtual EAIFireState Update(IAIObject* pITarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual void         Release()                                                   { delete this; }
	virtual bool         ValidateFireDirection(const Vec3& fireVector, bool mustHit) { return true; }
	virtual void         DebugDraw();
	virtual bool         UseDefaultEffectFor(EFireMode fireMode) const               { return true; }
	virtual void         OnReload()                                                  {}
	virtual void         OnShoot()                                                   { ++m_ShotsCount; }
	virtual int          GetShotCount() const                                        { return m_ShotsCount; }
	virtual float        GetTimeToNextShot() const;

protected:
	CPuppet* m_pShooter;

	// For new damage control
	enum EBurstState
	{
		BURST_NONE,
		BURST_FIRE,
		BURST_PAUSE,
	};

	EBurstState m_weaponBurstState;
	float       m_weaponBurstTime;
	float       m_weaponBurstTimeScale;
	float       m_weaponBurstTimeWithoutShot;
	int         m_weaponBurstBulletCount;
	int         m_curBurstBulletCount;
	float       m_curBurstPauseTime;
	float       m_weaponTriggerTime;
	bool        m_weaponTriggerState;
	float       m_coverFireTime;
	int         m_ShotsCount;

	class DrawFireEffect
	{
	public:
		enum EState
		{
			Running = 0,
			Finished,
		};

		EState GetState() const;
		bool   Update(float updateTime, CPuppet* pShooter, IAIObject* pTarget, const AIWeaponDescriptor& descriptor,
		              Vec3& aimTarget, bool canFire);

		void Reset();

	private:
		struct State
		{
			State()
				: time(0.0f)
				, idleTime(0.0f)
				, startSeed(cry_random(0.0f, 1337.0f))
				, state(Running)
			{
			}

			float  time;
			float  idleTime;
			float  startSeed;
			EState state;

		} m_state;
	};

	DrawFireEffect m_drawFire;
};

//====================================================================
// CFireCommandInstantSinle
// Fire command handler for pistols/shotguns/etc.
// When starting to fire the fire is drawn along a line towards the target.
//====================================================================
class CFireCommandInstantSingle : public CFireCommandInstant
{
public:
	CFireCommandInstantSingle(IAIActor* pShooter) :
		CFireCommandInstant(pShooter) {}

	virtual int  SelectBurstLength() { return 0; }
	virtual void CheckIfNeedsToFade(CAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
	{ return; }
	virtual bool IsStartingNow() const                         { return false; }
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }

};

//====================================================================
// CFireCommandLob
// Fire command handler for lobbing projectiles (grenades, grenade launchers, etc.)
// Calculates the lob trajectory - finds the best one; makes sure not to hit friends
// Note: Cannot be directly used. Helper for other fire command handlers.
//====================================================================
class CFireCommandLob : public IFireCommandHandler
{
public:
	CFireCommandLob(IAIActor* pShooter);
	~CFireCommandLob();
	virtual void         Reset();
	virtual EAIFireState Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual void         DebugDraw();

	virtual bool         ValidateFireDirection(const Vec3& fireVector, bool mustHit) { return true; }
	virtual void         Release()                                                   { delete this; }
	virtual bool         UseDefaultEffectFor(EFireMode fireMode) const               { return true; }
	virtual void         OnReload()                                                  {}
	virtual void         OnShoot()                                                   { m_lastStep = 0; }
	virtual int          GetShotCount() const                                        { return 0; }
	virtual void         GetProjectileInfo(SFireCommandProjectileInfo& info) const   { info.fSpeedScale = m_projectileSpeedScale; }
	virtual float        GetTimeToNextShot() const;

	// Must be declared in parent fire command handler
	virtual bool IsUsingPrimaryWeapon() const = 0;
	virtual bool CanHaveZeroTargetPos() const = 0;

	virtual bool GetOverrideAimingPosition(Vec3& overrideAimingPosition) const;

protected:
	struct SDebugThrowEval
	{
		bool  fake;
		bool  best;
		float score;
		Vec3  pos;

		typedef std::vector<Vec3> TTrajectory;
		TTrajectory trajectory;

		SDebugThrowEval() : fake(false), best(false), score(0.0f), pos(ZERO) {}
	};

	EAIFireState GetBestLob(IAIObject* pTarget, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);

	struct Trajectory
	{
		enum
		{
			MaxSamples = 64
		};

		struct Sample
		{
			Vec3 position;
			Vec3 velocity;
		};

		size_t GetSampleCount() const
		{
			return sampleCount;
		}

		Sample GetSample(const size_t index) const
		{
			assert(index < sampleCount);
			assert(index < MaxSamples);

			Sample s;

			if (index < sampleCount && index < MaxSamples)
			{
				s.position = positions[index];
				s.velocity = velocities[index];
			}
			else
			{
				s.position.zero();
				s.velocity.zero();
			}

			return s;
		}

		Vec3   positions[MaxSamples];
		Vec3   velocities[MaxSamples];
		uint32 sampleCount;
	};

	float EvaluateThrow(const Vec3& targetPos, const Vec3& targetViewDir, const Vec3& dir, float vel,
	                    Vec3& outThrowHitPos, Vec3& outThrowHitDir, float& outSpeed, Trajectory& trajectory,
	                    const float maxAllowedDistanceFromTarget, const float minimumDistanceToFriendsSq) const;
	bool IsValidDestination(ERequestedGrenadeType eReqGrenadeType, const Vec3& throwHitPos,
	                        const float minimumDistanceToFriendsSq) const;
	void ClearDebugEvals();

protected:
	Trajectory m_bestTrajectory;
	CPuppet*   m_pShooter;
	Vec3       m_targetPos;
	Vec3       m_throwDir;
	CTimeValue m_nextFireTime;
	float      m_preferredHeight;
	float      m_projectileSpeedScale;
	float      m_bestScore;
	int        m_lastStep;

#ifdef CRYAISYSTEM_DEBUG
	typedef std::vector<SDebugThrowEval> TDebugEvals;
	mutable TDebugEvals m_DEBUG_evals;
	float               m_debugBest;
	Vec3                m_Apex;
#endif //CRYAISYSTEM_DEBUG
};

//====================================================================
// CFireCommandProjectileSlow
// Fire command handler for slow projectiles such as hand grenades.
//====================================================================
class CFireCommandProjectileSlow : public CFireCommandLob
{
public:
	CFireCommandProjectileSlow(IAIActor* pShooter);
	virtual const char* GetName() { return "projectile_slow"; }

	// CFireCommandLob
	virtual bool IsUsingPrimaryWeapon() const { return true; }
	virtual bool CanHaveZeroTargetPos() const { return false; }
	//~CFireCommandLob
};

//====================================================================
// CFireCommandProjectileFast
// Fire command handler for fast projectiles such as RPG.
//====================================================================
class CFireCommandProjectileFast : public IFireCommandHandler
{
public:
	CFireCommandProjectileFast(IAIActor* pShooter);
	virtual const char*  GetName() { return "projectile_fast"; }
	virtual void         Reset();
	virtual EAIFireState Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool         ValidateFireDirection(const Vec3& fireVector, bool mustHit) { return true; }
	virtual void         Release()                                                   { delete this; }
	virtual void         DebugDraw();
	virtual bool         UseDefaultEffectFor(EFireMode fireMode) const               { return true; }
	virtual void         OnReload();
	virtual void         OnShoot()                                                   {}
	virtual int          GetShotCount() const                                        { return 0; }
	virtual void         GetProjectileInfo(SFireCommandProjectileInfo& info) const   { info.trackingId = m_trackingId; }
	virtual float        GetTimeToNextShot() const;

private:
	enum EAimMode
	{
		AIM_INFRONT,
		AIM_SIDES,
		AIM_BACK,
		AIM_DIRECTHIT,
	};

	bool ChooseShootPoint(Vec3& outShootPoint, IAIObject* pTarget, float explRadius, float missRatio, EAimMode aimMode);
	bool IsValidShootPoint(const Vec3& firePos, const Vec3& shootPoint, float explRadius) const;
	bool NoFriendNearAimTarget(float explRadius, const Vec3& shootPoint) const;

	CPuppet* m_pShooter;
	Vec3     m_aimPos;
	EntityId m_trackingId;
};

//====================================================================
// CFireCommandGrenade
// Lobs the secondary weapon given the grenade type
//====================================================================
class CFireCommandGrenade : public CFireCommandLob
{
public:
	CFireCommandGrenade(IAIActor* pShooter)
		: CFireCommandLob(pShooter)
	{
	}
	virtual const char* GetName() { return "grenade"; }

	// CFireCommandLob
	virtual bool IsUsingPrimaryWeapon() const { return false; }
	virtual bool CanHaveZeroTargetPos() const { return true; }
	//~CFireCommandLob
};

#endif
