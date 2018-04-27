// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYERROTATION_H__
#define __PLAYERROTATION_H__

#pragma once

struct IItem;
struct SActorFrameMovementParams;
struct SAimAccelerationParams;
struct SActorParams;
struct SPlayerStats;
struct SAutoaimTarget;
class CWeapon;
class CPlayer;
struct SLinkStats;

class CPlayerRotation
{
public:
	CPlayerRotation( const CPlayer& player );
	void Process(	IItem* pCurrentItem,
					const SActorFrameMovementParams& movement,
					const SAimAccelerationParams& verticalAcceleration,
					float frameTime);

	void FullSerialize( TSerialize ser );

	ILINE				 void SetBaseQuat(const Quat &baseQuat) { m_baseQuat = baseQuat; }

	ILINE const Quat& GetBaseQuat() const { return m_baseQuat; }
	ILINE const Quat& GetViewQuat() const { return m_viewQuat; }
	ILINE const Quat& GetViewQuatFinal() const { return m_viewQuatFinal; }
	ILINE const Ang3& GetViewAngles() const { return m_viewAngles; }
	ILINE       float GetLeanAmount() const { return m_leanAmount; }
	ILINE       float GetLeanPeekOverAmount() const { return m_peekOverAmount; }

	void SetViewRotation( const Quat &rotation );
	void SetViewRotationAndKeepBaseOrientation( const Quat &rotation );
	void SetViewRotationOnRevive( const Quat &rotation );
	void AddAngularImpulse( const Ang3 &angular,float deceleration,float duration );
	void ResetLinkedRotation( const Quat& rotation );
	void AddViewAngles(const Ang3 &angles);
	
	ILINE void AddViewAngleOffsetForFrame(const Ang3 &offset)
	{
		m_frameViewAnglesOffset += offset;
	}

	ILINE void SetForceLookAt(const Vec3& lookAtDirection, const bool bForcedLookAtBlendingEnabled = true)
	{
		m_forceLookVector = lookAtDirection;
		m_bForcedLookAtBlendingEnabled = bForcedLookAtBlendingEnabled; 
	}

private:

	void GetStanceAngleLimits(const SAimAccelerationParams& verticalAcceleration, IItem* pCurrentPlayerItem, float & minAngle,float & maxAngle);
	ILINE float GetLocalPitch()
	{
		const Quat localQuat = m_baseQuat.GetInverted() * m_viewQuat;
		return atan2(
			2*(localQuat.v.y*localQuat.v.z + localQuat.w*localQuat.v.x),
			localQuat.w*localQuat.w - localQuat.v.x*localQuat.v.x - localQuat.v.y*localQuat.v.y + localQuat.v.z*localQuat.v.z);
	}
	
	void ProcessForcedLookDirection( const Quat& lastViewQuat, float frameTime );
	void ProcessNormalRoll( float frameTime );
	void ProcessAngularImpulses( float frameTime );
	void ProcessNormal( float frameTime );
	void ProcessLeanAndPeek( const SActorFrameMovementParams& movement );
	void ProcessLinkedState( SLinkStats& linkStats, const Quat& lastBaseQuat, const Quat& lastViewQuat );
	void ProcessFinalViewEffects( float minAngle, float maxAngle );

	IItem* GetCurrentItem(bool includeVehicle);
	bool ShouldProcessTargetAssistance() const;
	void ProcessTargetAssistance( IItem* pCurrentPlayerItem );
	void TargetAimAssistance(CWeapon* pWeapon, float& followH, float& followV, float& scale, float& _fZoomAmount, const Vec3 playerView[4]);

	void ClampAngles( float minAngle, float maxAngle );

	Vec3 GetCloseCombatSnapTargetDirection(EntityId snapTargetId) const;
	Vec3 GetCloseCombatSnapTargetDirection(const Vec3& aimPos, const Vec3& aimDirection, const SAutoaimTarget& aaTarget) const;
	Vec3 PickBestSnapDirection(const Vec3& aimPos, const Vec3& aimDirection, const SAutoaimTarget& aaTarget) const;
	void UpdateCurrentSnapTarget(const EntityId snapTargetId, const Vec3& snapDirection);

	CWeapon* GetCurrentWeapon( IItem* pCurrentPlayerItem ) const;

	void NormalizeQuats();

	struct SLeanAndPeekInfo
	{
		SLeanAndPeekInfo()
			: m_queuedRayID(0)
			, m_lastLimit(0.0f, 0.0f, 0.0f)
		{

		}

		~SLeanAndPeekInfo()
		{
			CancelPendingRay();
		}

		void Reset();
		const Vec3& GetLeanLimit(const Vec3 &startPos, const Vec3 &dir, int objTypes, int flags, IPhysicalEntity **pSkipEnts = NULL, int nSkipEnts = 0);
		void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);

	private:

		void CancelPendingRay();

		QueuedRayID m_queuedRayID;
		Vec3		m_lastLimit;
	} m_leanAndPeekInfo;

	const CPlayer& m_player;

	Ang3 m_viewAngles;
	Quat m_viewQuat;
	Quat m_viewQuatFinal; //the final exact view quaternion (with recoil added)
	Quat m_baseQuat;
	Quat m_baseQuatLinked;
	Quat m_viewQuatLinked;

	Ang3 		m_frameViewAnglesOffset;		//Used for recoil/zoom sway
	
	Vec3		m_forceLookVector;

	Vec3     m_snap_target_dir;
	Vec3	 m_follow_target_dir;
	EntityId m_follow_target_id;
	EntityId m_snap_target_id;

	float	m_viewRoll;
	float	m_leanAmount;	
	float	m_peekOverAmount;

	// angular impulses
	Ang3  m_angularImpulse;
	Ang3  m_angularImpulseDelta;
	float m_angularImpulseTime;
	float m_angularImpulseDeceleration;
	float m_angularImpulseTimeMax;

	// This appears to be used purely internally.
	Ang3 m_deltaAngles;

	// Additional rotation requested by an external system
	Ang3 m_externalAngles;

	bool m_currently_snapping;
	bool m_bForcedLookAtBlendingEnabled; 

	float m_minAngle;
	float m_maxAngle;
	float m_minAngleTarget;
	float m_maxAngleTarget;
	float m_minAngleRate;
	float m_maxAngleRate;
	CTimeValue m_angleTransitionTimer;
};

#endif
