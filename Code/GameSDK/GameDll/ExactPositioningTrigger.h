// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EXACTPOSITIONINGTRIGGER_H__
#define __EXACTPOSITIONINGTRIGGER_H__

#pragma once


class CExactPositioningTrigger
{
public:
	CExactPositioningTrigger();
	CExactPositioningTrigger( const Vec3& pos, float width, const Vec3& triggerSize, const Quat& orient, float orientTolerance, float animMovementLength );

	void Update( float frameTime, Vec3 userPos, Quat userOrient, bool allowTriggering );
	//void DebugDraw();

	bool IsReached() const { return m_state >= eS_Optimizing; }
	bool IsTriggered() const { return m_state >= eS_Triggered; }

	void ResetRadius( const Vec3& triggerSize, float orientTolerance );
	void SetDistanceErrorFactor( float factor ) { m_distanceErrorFactor = factor; }

private:
	enum EState
	{
		eS_Invalid,
		eS_Initializing,
		eS_Before,
		eS_Optimizing,
		eS_Triggered
	};

	Vec3 m_pos;
	float m_width;
	Vec3 m_posSize;
	Quat m_orient;
	float m_cosOrientTolerance;
	float m_sideTime;
	float m_distanceErrorFactor;

	float m_animMovementLength;
	float m_distanceError;
	float m_orientError;
	float m_oldFwdDir;

	EState m_state;

	Vec3 m_userPos;
	Quat m_userOrient;
};

#endif // __EXACTPOSITIONINGTRIGGER_H__
