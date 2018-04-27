// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

12/03/2012 Jonathan Bunner
01/06/2012 Pete Bottomley - Changed to CFireModePlugin_AutoAim and swapped to
						be a CFireModePlugin. Removed duplicated params struct.
*************************************************************************/

#ifndef __PROJECTILE_AUTOAIM_HELPER_H__
#define __PROJECTILE_AUTOAIM_HELPER_H__

#if !defined(_RELEASE)
#define ALLOW_PROJECTILEHELPER_DEBUGGING 1
#else
#define ALLOW_PROJECTILEHELPER_DEBUGGING 0
#endif

#include "FireModePlugin.h"

class CFireModePlugin_AutoAim : public CFireModePlugin<SFireModePlugin_AutoAimParams>
{
private:
	typedef CFireModePlugin<SFireModePlugin_AutoAimParams> Parent;
	typedef SFireModePlugin_AutoAimParams::SAutoAimModeParams ConeParams;

	struct STargetSelectionParams
	{
		STargetSelectionParams()
		{
			// Some sensible defaults
			m_distanceHeuristicWeighting	= 1.0f;
			m_angleHeuristicWeighting		= 2.0f;
			m_bDisableAutoAimIfPlayerAlreadyHasTarget = true;
			m_bDisableAutoAimIfPlayerIsZoomed = false;
		}

		// Settings for prioritizing target candidates based on distance (e.g. closer better) and/or how central the target is to the aiming reticule.
		float m_distanceHeuristicWeighting;
		float m_angleHeuristicWeighting;

		// If the player already has a target for themselves, do we still attempt to adjust projectile paths?
		bool  m_bDisableAutoAimIfPlayerAlreadyHasTarget;
		bool  m_bDisableAutoAimIfPlayerIsZoomed;
	};

public:
	CRY_DECLARE_GTI(CFireModePlugin_AutoAim);

public:
	CFireModePlugin_AutoAim();
	virtual ~CFireModePlugin_AutoAim();

	// CFireModePlugin
	virtual bool Init(CFireMode* pFiremode, IFireModePluginParams* pPluginParams) override;
	virtual void Activate(bool activate) override;
	virtual bool Update(float frameTime, uint32 frameId) override;
	virtual void AlterFiringDirection(const Vec3& firingPos, Vec3& rFiringDir) const override;
	//~CFireModePlugin

	ILINE void SetEnabled(const bool bEnabled) {m_bEnabled = bEnabled;}
	ILINE bool GetEnabled() const {return m_bEnabled;}

private:
	// Adjust the given firing direction to potential targets based on current params
	void AdjustFiringDirection(const Vec3& attackerPos, Vec3& firingDirToAdjust, const bool bCurrentlyZoomed, const EntityId ownerId ) const;

	// Helpers
	IEntity* CalculateBestProjectileAutoAimTarget(const Vec3& attackerPos, const Vec3& attackerDir, const bool bCurrentlyZoomed, const EntityId ownerId) const;
	bool	 AllowedToTargetPlayer(const EntityId attackerId, const EntityId victimEntityId) const;
	bool	 TargetPositionWithinFrontalCone(const Vec3& attackerLocation,const Vec3& victimLocation,const Vec3& attackerFacingdir, const float targetConeRads, float& theta) const;

	ILINE const ConeParams& GetAimConeSettings(const bool bIsZoomed) const { return bIsZoomed ? m_pParams->m_zoomedParams : m_pParams->m_normalParams;}

	// Data
	STargetSelectionParams m_targetSelectionParams;
	bool m_bEnabled;

#if ALLOW_PROJECTILEHELPER_DEBUGGING
	mutable CryFixedStringT<128> m_lastShotAutoAimedStatus; 
	mutable CryFixedStringT<32>  m_lastTargetRejectionReason;
#endif //#if ALLOW_PROJECTILEHELPER_DEBUGGING
};

#endif //__PROJECTILE_AUTOAIM_HELPER_H__