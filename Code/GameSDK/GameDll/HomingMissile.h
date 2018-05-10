// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: HomingMissile

-------------------------------------------------------------------------
History:
- 12:10:2005   11:15 : Created by Márcio Martins

*************************************************************************/
#ifndef __HomingMissile_H__
#define __HomingMissile_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Rocket.h"

#define ASPECT_DESTINATION eEA_GameClientDynamic

class CHomingMissile : public CRocket
{
public:
	CHomingMissile();
	virtual ~CHomingMissile();
  
	// CRocket	
	virtual bool Init(IGameObject *pGameObject);
	virtual void Update(SEntityUpdateContext &ctx, int updateSlot);
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale);
	virtual void SetDestination(const Vec3& pos);

	virtual void SetDestination(EntityId targetId)
	{
		m_targetId = targetId;
	}

	virtual void Deflected(const Vec3& dir);

	virtual void FullSerialize(TSerialize ser);
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual NetworkAspectType GetNetSerializeAspects();
	// ~CRocket

	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);

protected:

	void SetViewMode(CItem::eViewMode viewMode);

	virtual void UpdateControlledMissile(float frameTime);   //LAW missiles
	virtual void UpdateCruiseMissile(float frameTime);       //Vehicle missiles

	void SerializeDestination( TSerialize ser );

	void UpdateHomingGuide();

	ILINE bool HasTarget() const { return !m_destination.IsZeroFast(0.3f); }; //Net serialize inaccuracy means checking for actual zero is not sufficient

	Vec3 m_homingGuidePosition;
	Vec3 m_homingGuideDirection;

	Vec3 m_destination;
  
	EntityId	m_targetId;

	float m_lockedTimer;
	float m_controlLostTimer;

	QueuedRayID m_destinationRayId;

	// status
	bool m_isCruising;
	bool m_isDescending;
	bool m_trailEnabled;
};


#endif // __HomingMissile_H__
