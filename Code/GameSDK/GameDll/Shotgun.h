// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _SHOTGUN_H_
#define _SHOTGUN_H_

#include "Single.h"

class CShotgun :
	public CSingle
{
	struct BeginReloadLoop;
	class PartialReloadAction;
	class ReloadEndAction;
	class ScheduleReload;

public:
	CRY_DECLARE_GTI(CShotgun);

	virtual void GetMemoryUsage(ICrySizer * s) const override;
	void GetInternalMemoryUsage(ICrySizer * s) const;
	virtual void Activate(bool activate) override;
	virtual void StartReload(int zoomed) override;
	void ReloadShell(int zoomed);
	virtual void EndReload(int zoomed) override;
	using CSingle::EndReload;
	
	virtual void CancelReload() override;

	virtual bool CanFire(bool considerAmmo) const override;

	virtual bool Shoot(bool resetAnimation, bool autoreload = true , bool isRemote=false ) override;
	virtual void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph) override;

	virtual float GetSpreadForHUD() const override;

	virtual uint8 GetShotIncrementAmount() override
	{
		return (uint8)m_fireParams->shotgunparams.pellets;
	}

private:

	int   m_max_shells;
	uint8 m_shotIndex;

	bool	m_reload_pump;
	bool	m_load_shell_on_end;				
	bool	m_break_reload;
	bool	m_reload_was_broken;

};

#endif
