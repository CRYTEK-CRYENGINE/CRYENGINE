// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Iron Sight

-------------------------------------------------------------------------
History:
- 28:10:2005   16:00 : Created by Márcio Martins

*************************************************************************/
#pragma once

#ifndef __IRONSIGHT_H__
#define __IRONSIGHT_H__

#include <IViewSystem.h>
#include "Weapon.h"
#include "GameParameters.h"
#include "GameTypeInfo.h"
#include "Recoil.h"
#include "Utility/Wiggle.h"
#include "WeaponSharedParams.h"
#include "Audio/AudioSignalPlayer.h"

class CIronSight : public IZoomMode
{
private:
	struct EnterZoomAction;
	struct DisableTurnOffAction;
	struct EnableTurnOffAction;

public:
	CRY_DECLARE_GTI_BASE(CIronSight);

	CIronSight();
	virtual ~CIronSight();
	virtual void GetMemoryUsage(ICrySizer * s) const;

	// IZoomMode
	virtual void InitZoomMode(IWeapon *pWeapon, const SParentZoomModeParams* pParams, uint32 id);

	const SZoomModeParams* GetShared() const {return m_zoomParams;}
	const SParentZoomModeParams* GetParentShared() { return m_parentZoomParams; }
	void ResetSharedParams(const SZoomModeParams* pParams) { m_zoomParams = pParams; }

	virtual void Update(float frameTime, uint32 frameId);
	virtual void Release();

	virtual void Activate(bool activate);

	virtual bool CanZoom() const;
	virtual bool StartZoom(bool stayZoomed = false, bool fullZoomout = true, int zoomStep = 1);
	virtual void StopZoom();
	virtual void ExitZoom(bool force);

	virtual int GetCurrentStep() const;
	virtual float GetZoomFoVScale(int step) const;

	virtual void ZoomIn();
	virtual bool ZoomOut();

	virtual bool IsZoomed() const;
	virtual bool IsZoomingIn() const;
	virtual bool IsZoomingInOrOut() const;
	bool IsZoomingOutScheduled() const;
	void CancelZoomOutSchedule();
	virtual EZoomState GetZoomState() const;
	virtual float GetZoomInTime() const;
	virtual float GetZoomTransition() const;

	virtual bool AllowsZoomSnap() const;

	virtual void Enable(bool enable);
	virtual bool IsEnabled() const;

	virtual void Serialize(TSerialize ser);

	virtual void GetFPOffset(QuatT &offset) const;

	virtual void UpdateFPView(float frameTime){}

	virtual int  GetMaxZoomSteps() const;

	virtual void ApplyZoomMod(IFireMode* pFM, float modMultiplier);
	virtual void ResetZoomMod(IFireMode* pFM);

	virtual bool IsToggle();

	virtual void FilterView(SViewParams &viewparams);
	virtual void PostFilterView(SViewParams & viewparams);
	// ~IZoomMode

	virtual void ResetTurnOff();
	virtual void TurnOff(bool enable, bool smooth=true, bool anim=true);

	virtual bool IsScope() const { return false; }

	virtual void StartStabilize();
	virtual void EndStabilize();
	virtual bool IsStable();

	float GetStageMovementModifier() const;
	float GetStageRotationModifier() const;

	void ClearDoF();

protected:
	virtual void EnterZoom(float time, bool smooth=true, int zoomStep = 1);
	virtual void ScheduleLeaveZoom();
	virtual void LeaveZoom();

	virtual void ZoomIn(float time, float from, float to, bool smooth, int nextStep);
	virtual void ZoomOut(float time, float from, float to, bool smooth, int nextStep);

	virtual void OnEnterZoom();
	virtual void OnZoomedIn();

	virtual void OnLeaveZoom();
	virtual void OnZoomedOut();

	virtual void OnZoom(float time, int nextStep);

	virtual void OnZoomStep(bool zoomingIn, float t);

	virtual void SetActorFoVScale(const float fov);
	virtual float GetActorFoVScale() const;

	void ClearBlur();

	void SetIsZoomed(bool isZoomed);
	void AdjustNearFov(float time, bool zoomIn);
	void ResetNearFov();

	Ang3 ZoomSway(CPlayer& clientPlayer, float time);
	float GetModifiedZoomInTime();

	void UpdateBonusZoomScale(float frameTime);

	float GetZoomFovScale() const;
	float GetZoomBonusScale() const;

	IMaterial* GetCrossHairMaterial() const;

	CWeapon				*m_pWeapon;

	CAudioSignalPlayer				m_stabilizeAudioLoop;

	const SParentZoomModeParams*	m_parentZoomParams;
	const SZoomModeParams*				m_zoomParams;  
	uint32									m_zmIdx;

	//Beni - TODO: Many of the var members below could be static
	float					m_savedFoVScale;

	float					m_zoomTimer;
	float					m_zoomTime;
	float					m_averageDoF;

	float					m_leaveZoomTimeDelay;

	float					m_startZoomFoVScale;
	float					m_endZoomFoVScale;
	float					m_currentZoomFoVScale;
	float					m_currentZoomBonusScale;
	int						m_currentStep;

	float					m_initialNearFov;

	float         m_swayTime;
	CWiggle				m_swayX;
	CWiggle				m_swayY;
	//float					m_antiSniperEffect;

	float         m_lastRecoil;

	float			m_stableTime;

	bool			m_enabled;
	bool			m_smooth;
	bool			m_zoomOutScheduled;
	bool					m_zoomed;
	bool					m_zoomingIn;

	bool			m_stable;
	bool			m_leavingstability;
	bool			m_blendNearFoV;
	bool			m_activated;
};

#endif // __IRONSIGHT_H__
