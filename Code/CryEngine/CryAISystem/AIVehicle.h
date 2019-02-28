// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _AI_VEHICLE_
#define _AI_VEHICLE_

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Puppet.h"

class CAIVehicle
	: public CPuppet
{
public:
	CAIVehicle();
	~CAIVehicle(void);

	virtual void     Update(EUpdateType type) override;
	virtual void     UpdateDisabled(EUpdateType type) override;
	void             Navigate(CAIObject* pTarget);
	virtual void     Event(unsigned short eType, SAIEVENT* pEvent) override;

	virtual void     Reset(EObjectResetType type) override;
	virtual void     ParseParameters(const AIObjectParams& params, bool bParseMovementParams = true) override;
	virtual EntityId GetPerceivedEntityID() const override;

	void             AlertPuppets(void);
	virtual void     Serialize(TSerialize ser) override;

	bool             HandleVerticalMovement(const Vec3& targetPos);

	bool             IsDriverInside() const;
	bool             IsPlayerInside();

	EntityId         GetDriverEntity() const;
	CAIActor*        GetDriver() const;

	virtual bool     IsTargetable() const override { return IsActive(); }
	virtual bool     IsActive() const override     { return m_bEnabled || IsDriverInside(); }

	virtual void     GetPathFollowerParams(struct PathFollowerParams& outParams) const override;

protected:
	void FireCommand(void);
	bool GetEnemyTarget(int objectType, Vec3& hitPosition, float fDamageRadius2, CAIObject** pTarget);
	void OnDriverChanged(bool bEntered);

private:

	// local functions for firecommand()
	Vec3  PredictMovingTarget(const CAIObject* pTarget, const Vec3& vTargetPos, const Vec3& vFirePos, const float duration, const float distpred);
	bool  CheckExplosion(const Vec3& vTargetPos, const Vec3& vFirePos, const Vec3& vActuallFireDir, const float fDamageRadius);
	Vec3  GetError(const Vec3& vTargetPos, const Vec3& vFirePos, const float fAccuracy);
	float RecalculateAccuracy(void);

	bool        m_bPoweredUp;

	CTimeValue  m_fNextFiringTime;      //To put an interval for the next firing.
	CTimeValue  m_fFiringPauseTime;     //Put a pause time after the trurret rotation has completed.
	CTimeValue  m_fFiringStartTime;     //the time that the firecommand started.

	Vec3        m_vDeltaTarget;   //To add a vector to the target position for errors and moving prediction

	int         m_ShootPhase;
	mutable int m_driverInsideCheck;
	int         m_playerInsideCheck;
	bool        m_bDriverInside;
};

ILINE const CAIVehicle* CastToCAIVehicleSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCAIVehicle() : 0; }
ILINE CAIVehicle*       CastToCAIVehicleSafe(IAIObject* pAI)       { return pAI ? pAI->CastToCAIVehicle() : 0; }

#endif
