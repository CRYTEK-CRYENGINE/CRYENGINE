// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef ShootOp_h
	#define ShootOp_h

	#include "EnterLeaveUpdateGoalOp.h"
	#include <CrySystem/TimeValue.h>

// Shoot at the attention target or at a point in the actor's local-space.
// The shooting will go on for a specified amount of time, and after that
// the fire mode will be set to 'off' and the goal op will finish.
class ShootOp : public EnterLeaveUpdateGoalOp
{
public:
	ShootOp();
	ShootOp(const XmlNodeRef& node);
	ShootOp(const ShootOp& rhs);

	virtual void Enter(CPipeUser& pipeUser);
	virtual void Leave(CPipeUser& pipeUser);
	virtual void Update(CPipeUser& pipeUser);
	virtual void Serialize(TSerialize serializer);

	enum ShootAt
	{
		AttentionTarget,
		ReferencePoint,
		LocalSpacePosition,

		ShootAtSerializationHelperLast,
		ShootAtSerializationHelperFirst = AttentionTarget
	};

private:
	void SetupFireTargetForLocalPosition(CPipeUser& pipeUser);
	void SetupFireTargetForReferencePoint(CPipeUser& pipeUser);

	Vec3                  m_position;
	CStrongRef<CAIObject> m_dummyTarget;
	CTimeValue            m_timeWhenShootingShouldEnd;
	float                 m_duration;
	ShootAt               m_shootAt;
	EFireMode             m_fireMode;
	EStance               m_stance;
};

#endif // ShootOp_h
