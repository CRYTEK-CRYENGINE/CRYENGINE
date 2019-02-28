// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Fire Mode Plugins

-------------------------------------------------------------------------
History:
- 10:02:2011 : Created by Claire Allan

*************************************************************************/

#pragma once

#ifndef __FIREMODEPLUGIN_H__
#define __FIREMODEPLUGIN_H__

#include "FireModePluginParams.h"
#include "EntityUtility/EntityEffects.h"
#include "GameTypeInfo.h"

class CFireMode;

class IFireModePlugin
{
public:
	CRY_INTERFACE_GTI(IFireModePlugin);

	IFireModePlugin() {}
	virtual ~IFireModePlugin () {}

	virtual bool Update(float frameTime, uint32 frameId) = 0;
	virtual void Activate(bool activate) = 0;
	virtual bool AllowFire() const = 0;
	virtual bool Init(CFireMode* pFiremode, IFireModePluginParams* pPluginParams) = 0;
	virtual void OnShoot() = 0;
	virtual void OnReplayShoot() = 0;
	virtual void AlterFiringDirection(const Vec3& firingPos, Vec3& rFiringDir) const = 0;
};

template <typename paramType> 
class CFireModePlugin : public IFireModePlugin
{
public:
	CFireModePlugin() : m_pOwnerFiremode(NULL), m_pParams(NULL) {}
	virtual ~CFireModePlugin () {}

	bool Init(CFireMode* pFiremode, IFireModePluginParams* pPluginParams)
	{
		CRY_ASSERT_MESSAGE(pFiremode, "NULL firemode passed to plugin, not possible!");
		m_pOwnerFiremode = pFiremode;

		if(pPluginParams && pPluginParams->GetPluginType() == GetRunTimeType())
		{
			m_pParams = static_cast<paramType*>(pPluginParams);
		}
		else
		{
			CRY_ASSERT_MESSAGE(0, "Incorrect or NULL parameters for plugin");
			return false;
		}

		return (pFiremode != NULL);
	}

	virtual bool Update(float frameTime, uint32 frameId) { return false; }
	virtual void Activate(bool activate) {}
	virtual bool AllowFire() const { return true; }
	virtual void OnShoot() {}
	virtual void OnReplayShoot() {}
	virtual void AlterFiringDirection(const Vec3& firingPos, Vec3& rFiringDir) const {}
	

protected:

	CFireMode* m_pOwnerFiremode;
	const paramType* m_pParams;
};

class CFireModePlugin_Overheat : public CFireModePlugin<SFireModePlugin_HeatingParams> 
{
public:
	CRY_DECLARE_GTI(CFireModePlugin_Overheat);

	CFireModePlugin_Overheat();
	virtual ~CFireModePlugin_Overheat() {};

	virtual bool Update(float frameTime, uint32 frameId) override;
	virtual void Activate(bool activate) override;
	virtual bool AllowFire() const override;
	virtual void OnShoot() override;

	float GetHeat() const { return m_heat; }

protected:
	float			m_heat;
	float			m_overheat;
	float			m_nextHeatTime;
	EntityEffects::TAttachedEffectId m_heatEffectId;
	bool			m_firedThisFrame;
	bool			m_isCoolingDown;
};

class CFireModePlugin_Reject : public CFireModePlugin<SFireModePlugin_RejectParams>
{
public:
	CRY_DECLARE_GTI(CFireModePlugin_Reject);

	CFireModePlugin_Reject();
	virtual ~CFireModePlugin_Reject() {};

	virtual bool Update(float frameTime, uint32 frameId) override;
	virtual void Activate(bool activate) override;
	virtual void OnShoot() override;
	virtual void OnReplayShoot() override { OnShoot(); }

protected:
	Vec3	m_lastShellFXPosition;
	Vec3	m_shellHelperVelocity;
	float	m_shellFXSpeed;
};

class CFireModePlugin_RecoilShake : public CFireModePlugin<SFireModePlugin_RecoilShakeParams>
{
public:
	CRY_DECLARE_GTI(CFireModePlugin_RecoilShake);

	virtual void OnShoot() override;
	virtual void OnReplayShoot() override;
};

#endif //__FIREMODEPLUGIN_H__
