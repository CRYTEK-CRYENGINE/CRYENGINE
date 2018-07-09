// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Camera mode implementations

-------------------------------------------------------------------------
History:
- 16:10:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef _CAMERA_MODES_H_
#define _CAMERA_MODES_H_

#include "ICameraMode.h"

struct SPlayerStats;
struct SDeathCamSPParams;
struct IItem;



struct CCameraPose
{
public:
	CCameraPose()
		:	m_position(ZERO)
		,	m_rotation(IDENTITY) {}
	CCameraPose(Vec3 positionOffset, Quat rotationOffset)
		:	m_position(positionOffset)
		,	m_rotation(rotationOffset) {}
	CCameraPose(const CCameraPose& copy)
		:	m_position(copy.m_position)
		,	m_rotation(copy.m_rotation) {}

	static CCameraPose Compose(const CCameraPose& lhv, const CCameraPose& rhv)
	{
		return CCameraPose(
			lhv.GetPosition() + rhv.GetPosition(),
			lhv.GetRotation() * rhv.GetRotation());
	}

	static CCameraPose Scale(const CCameraPose& lhv, float rhv)
	{
		if (rhv == 1.0f)
			return lhv;
		else
			return CCameraPose(
				lhv.GetPosition() * rhv,
				Quat::CreateSlerp(IDENTITY, lhv.GetRotation(), rhv));
	}

	static CCameraPose Lerp(const CCameraPose& from, const CCameraPose& to, float stride)
	{
		return CCameraPose(
			LERP(from.GetPosition(), to.GetPosition(), stride),
			Quat::CreateSlerp(from.GetRotation(), to.GetRotation(), stride));
	}

	Vec3 GetPosition() const {return m_position;}
	Quat GetRotation() const {return m_rotation;}

private:
	Vec3 m_position;
	Quat m_rotation;
};


class CDefaultCameraMode : public ICameraMode
{
private:

public:
	CDefaultCameraMode();
	virtual ~CDefaultCameraMode();

	virtual bool UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime);
	virtual bool CanTransition() { return true; }
	
	virtual void GetCameraAnimationFactor(float &pos, float &rot)
	{
		pos = rot = 1.0f;
	}

private:
	CCameraPose FirstPersonCameraPose(const CPlayer& clientPlayer, IItem* pCurrentItem, float frameTime);
	CCameraPose ThirdPersonCameraPose(const CPlayer& clientPlayer, const SViewParams& viewParams);
	CCameraPose ViewBobing(const CPlayer& clientPlayer, IItem* pCurrentItem, float frameTime);

	bool ApplyItemFPViewFilter(const CPlayer& clientPlayer, IItem* pCurrentItem, SViewParams& viewParams);

	float m_lastTpvDistance;
	float m_bobCycle;
	Vec3 m_bobOffset;
	float m_minAngle;
	float m_minAngleTarget;
	float m_minAngleRate;
	CTimeValue m_angleTransitionTimer;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CSpectatorFollowCameraMode : public ICameraMode
{
public:
	CSpectatorFollowCameraMode();
	virtual ~CSpectatorFollowCameraMode();
	virtual bool UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime);

	virtual void Activate(const CPlayer & clientPlayer);
	virtual void Deactivate(const CPlayer & clientPlayer);

private:
	void ApplyEffects ( const CPlayer& clientPlayer, const SViewParams& viewParams, float focalDist, float minFocalRange );
	void RevertEffects () const;

private:

	EntityId m_lastSpectatorTarget;
	int	 m_lastFrameId;
	Vec3 m_viewOffset; // In "camera view" space
	Vec3 m_entityPos;
	Vec3 m_entityExtra;
	float m_orbitX;
	float m_orbitZ;
	uint8 m_postEffectsForSpecMode;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CSpectatorFixedCameraMode : public ICameraMode
{
public:
	CSpectatorFixedCameraMode();

	virtual bool UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime);

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CAnimationControlledCameraMode : public ICameraMode
{
public:
	virtual bool UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime);
	virtual bool CanTransition() { return true; }

	virtual void GetCameraAnimationFactor(float &pos, float &rot) 
	{
		pos = 1.0f; rot = 1.0f;
	}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CDeathCameraMode : public ICameraMode
{
public:
	CDeathCameraMode();
	virtual ~CDeathCameraMode();

	virtual bool UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime);
	virtual void Activate(const CPlayer& clientPlayer);

private:
	void  Init(const CPlayer* subject, const EntityId killerEid, const int hitType);

	void PreventVerticalGimbalIssues( Vec3& focusPos, Vec3& focusDir, const Vec3& camPos ) const;


	void UpdateInternalPositions( const Vec3& eye, const Vec3& lookAt, const float lerpSpeed, const float frameTime );
	ILINE void SetInternalPositions ( const Vec3& eye, const Vec3& lookAt ) { m_cameraEye = eye; m_cameraLookAt = lookAt; }
	void ApplyResults( SViewParams& viewParams );

private:
	Matrix34  m_prevSubWorldTM0;
	Matrix34  m_prevSubWorldTM1;
	Vec3  m_firstFocusPos;
	Vec3  m_prevFocusDir;
	Vec3  m_prevLookPos;
	Vec3  m_cameraEye;
	Vec3  m_cameraLookAt;
	float  m_prevFocus2camClearDist;
	float  m_prevBumpUp;
	float m_updateTime;
	float m_turnAngle;
	float m_timeOut;
	float m_invTimeMulti;
	EntityId  m_subjectEid;
	EntityId  m_killerEid;
	int m_hitType;
	bool  m_firstUpdate;
	bool m_gotoKillerCam;
};

//////////////////////////////////////////////////////////////////////////
/// Special camera mode for single player local player deaths. On its current
/// implementation is just an animation-controlled camera with some depth
/// of field effects to enphasize the killer's position
//////////////////////////////////////////////////////////////////////////
class CDeathCameraModeSinglePlayer : public CAnimationControlledCameraMode
{
public:
	CDeathCameraModeSinglePlayer();
	virtual ~CDeathCameraModeSinglePlayer();
	virtual bool  UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime);
	virtual void  Activate(const CPlayer& clientPlayer);
	virtual void  Deactivate(const CPlayer& clientPlayer);

private:
	typedef CAnimationControlledCameraMode ParentMode;

	bool	IsEntityInFrustrum(EntityId entityId, const CPlayer& clientPlayer, const SViewParams& viewParams) const;

	void  Init(const CPlayer* subject, const EntityId killerEid);

private:
	EntityId  								m_subjectEid;
	EntityId  								m_killerEid;
	float											m_fUpdateCounter;
	float											m_fKillerHeightOffset;
	bool											m_bIsKillerInFrustrum;
	float											m_fFocusDistance;
	float											m_fFocusRange;

	const SDeathCamSPParams&	m_params;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CVehicleCameraMode : public ICameraMode
{
public:
	virtual bool UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CPartialAnimationControlledCameraMode : public ICameraMode
{
public:

	CPartialAnimationControlledCameraMode();
	virtual ~CPartialAnimationControlledCameraMode();

	virtual void Activate(const CPlayer& clientPlayer);
	virtual bool UpdateView(const CPlayer& clientPlayer, SViewParams& viewParams, float frameTime);
	virtual bool CanTransition() { return true; }

	virtual void SetCameraAnimationFactor( const ICameraMode::AnimationSettings& animationSettings )
	{
		m_animationSettings.positionFactor = animationSettings.positionFactor;
		m_animationSettings.rotationFactor = animationSettings.rotationFactor;
		m_animationSettings.applyResult = animationSettings.applyResult;
		m_animationSettings.stableBlendOff = animationSettings.stableBlendOff;
	}

	virtual void GetCameraAnimationFactor(float &pos, float &rot) 
	{
		if(IsBlendingOff() && !m_animationSettings.stableBlendOff)
		{
			//CLAIRE: Stable blend off is not compatible with blending back into camera animation as the stored position/rotation on last frame 
			//would have been missing last frames camera animation and therefore we end up with a pop in the opposite direction
			pos = 0.f;
			rot = 0.f;
		}
		else
		{
			pos = m_currentPositionFactor; 
			rot = m_currentOrientationFactor;
		}
	}

private:

	ILINE float GetBlendOrientationFactor() const { return m_animationSettings.rotationFactor; }
	ILINE float GetBlendPositionFactor() const { return m_animationSettings.positionFactor; }

	float m_currentOrientationFactor;
	float m_currentPositionFactor;

	ICameraMode::AnimationSettings m_animationSettings;

	QuatT m_lastQuatBeforeDeactivation;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#endif
