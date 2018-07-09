// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Laser accessory (re-factored from Crysis L.A.M.)

-------------------------------------------------------------------------
History:
- 11-6-2008   Created by Benito Gangoso Rodriguez

*************************************************************************/

#ifndef LASER_H
#define LASER_H

#include <IItemSystem.h>
#include "Accessory.h"

struct SLaserParams;
class CWeapon;

class CLaserBeam
{
public:
	CLaserBeam();
	~CLaserBeam();

	struct SLaserUpdateDesc
	{
		SLaserUpdateDesc(const Vec3& laserPos, const Vec3& laserDir, float frameTime, bool bOwnerHidden);

		Vec3 m_laserPos;
		Vec3 m_laserDir;
		float m_frameTime;
		bool m_ownerCloaked;
		bool m_weaponZoomed;
		bool m_bOwnerHidden;
	};

	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);;

	void Initialize(EntityId owneEntityId, const SLaserParams* pParams, eGeometrySlot geometrySlot);
	void TurnOnLaser();
	void TurnOffLaser();
	bool IsLaserActivated() const { return m_laserOn; }
	void UpdateLaser(const SLaserUpdateDesc& laserUpdateDesc);
	Vec3 GetLastHit() const {return m_lastHit;}
	void SetGeometrySlot(eGeometrySlot geometrySlot);
	ILINE void SetParams(SLaserParams* pParams) { m_pLaserParams = pParams; }

	EntityId GetLaserEntityId() const { return m_laserEntityId; }
private:

	IEntity* CreateLaserEntity();
	IEntity* GetLaserEntity();
	void DestroyLaserEntity();
	void SetLaserEntitySlots(bool freeSlots);
	void UpdateLaserGeometry(IEntity& laserEntity);
	int GetIndexFromGeometrySlot();
	IAttachmentManager* GetLaserCharacterAttachmentManager();
	void FixAttachment(IEntity* pLaserEntity);

private:
	const SLaserParams* m_pLaserParams;

	Vec3		m_lastHit;
	Vec3		m_lastLaserUpdatePosition;
	Vec3		m_lastLaserUpdateDirection;
	float		m_laserUpdateTimer;

	EntityId			m_ownerEntityId;
	EntityId			m_laserEntityId;
	QueuedRayID		m_queuedRayId;
	eGeometrySlot	m_geometrySlot;
	int						m_laserDotSlot;
	int						m_laserGeometrySlot;

	bool		m_laserOn;
	bool		m_hasHitData;
	bool		m_hitSolid;
	bool		m_usingEntityAttachment;
};


class CLaser : public CAccessory, IWeaponFiringLocator
{
protected: 
	typedef CAccessory BaseClass;

public:

	CLaser();
	virtual			~CLaser();

	//IItem
	virtual bool Init(IGameObject * pGameObject );
	virtual void Reset();

	virtual void OnAttach(bool attach);
	virtual void OnParentSelect(bool select);
	virtual void Update(SEntityUpdateContext& ctx, int slot);

	virtual bool ResetParams();
	//~IItem

	//Item Events
	virtual void OnEnterFirstPerson();
	virtual void OnEnterThirdPerson();

	//For AI control (or outside)
	bool IsLaserActivated() const { return m_laserBeam.IsLaserActivated(); }
	void	ActivateLaser(bool activate);

	void TurnOnLaser(bool manual = false);
	void TurnOffLaser(bool manual = false);

private:

	CWeapon* GetWeapon() const;

	void GetLaserPositionAndDirection(CWeapon* pParentWeapon, Vec3& pos, Vec3& dir);

	virtual bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit);
	virtual bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos);
	virtual bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
	virtual bool GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
	virtual bool GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir);
	virtual void WeaponReleased();

	CLaserBeam m_laserBeam;
	ItemString m_laserHelperFP;
};

#endif
