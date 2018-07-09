// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: 

Ltag version of single firemode. Requires some extra functionality
for animation, projectile spawning, etc

-------------------------------------------------------------------------
History:
- 15:09:09   Benito Gangoso Rodriguez

*************************************************************************/

#pragma once

#ifndef _LTAG_SINGLE_H_
#define _LTAG_SINGLE_H_

#include "Single.h"
#include "LTAGGrenade.h"
#include "Throw.h"

class CLTagSingle : public CSingle
{
private:
	typedef CSingle Parent;
	struct EndCockingAction;
	struct ScheduleReload;

public:
	CRY_DECLARE_GTI(CLTagSingle);

	CLTagSingle();
	virtual ~CLTagSingle();

	//CSingle
	virtual void Activate(bool activate) override;
	virtual void UpdateFPView(float frameTime) override;

	virtual void OnShoot(EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel) override;

	virtual void SetProjectileLaunchParams(const SProjectileLaunchParams &launchParams) override;
	//~CSingle

	bool NextGrenadeType();
	const ELTAGGrenadeType GetCurrentGrenadeType() const;
	//bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	//uint NetGetGrenadeType() const { return static_cast<uint>(m_grenadeType); }
	//void NetSetGrenadeType(uint type);
	void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle) override;

	void GetMemoryUsage(ICrySizer * s) const override
	{
		s->AddObject(this, sizeof(*this));	
		CSingle::GetInternalMemoryUsage(s);		// collect memory of parent class
	}

protected:
	virtual void SetReloadFragmentTags(CTagState& fragTags, int ammoCount) override;

	void SwitchGrenades ();
	void UpdateGrenadeAttachment(ICharacterInstance* pCharacter, const char* attachmentName, const char* model);

	ELTAGGrenadeType m_grenadeType;
	bool	m_fpModelInitialised;
};



#endif
