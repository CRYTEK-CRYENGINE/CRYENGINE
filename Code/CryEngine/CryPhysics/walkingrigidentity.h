// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef walkingrigidentity_h
#define walkingrigidentity_h
#pragma once

#include "rigidentity.h"

class CWalkingRigidEntity : public CRigidEntity {
public:
	explicit CWalkingRigidEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap = NULL);
	virtual ~CWalkingRigidEntity() { delete[] m_legs;	}
	virtual pe_type GetType() const { return PE_WALKINGRIGID; }

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id, int bThreadSafe=1);
	virtual int SetParams(pe_params *_params,int bThreadSafe=1);
	virtual int GetParams(pe_params *_params) const;
	virtual int GetStatus(pe_status*) const;

	virtual void RecomputeMassDistribution(int ipart=-1,int bMassChanged=1);
	virtual int Step(float dt);
	virtual float CalcEnergy(float dt) { return m_Eaux + CRigidEntity::CalcEnergy(dt); }
	virtual int GetPotentialColliders(CPhysicalEntity **&pentlist, float dt=0);
	virtual bool OnSweepHit(geom_contact &cnt, int icnt, float &dt, Vec3 &vel, int &nsweeps);
	virtual void CheckAdditionalGeometry(float dt);
	virtual int RegisterContacts(float dt,int nMaxPlaneContacts);
	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	Vec3 *m_legs = nullptr;
	int m_nLegs = 0;
	int m_colltypeLegs = geom_colltype_player;
	float m_minLegsCollMass = 1.0f;
	float m_velStick = 0.5f;
	float m_friction = 1.0f;
	float m_unprojScale = 10.0f;

	int m_ilegHit = -1;
	float m_distHit;
	CPhysicalEntity *m_pentGround = nullptr;
	int m_ipartGround;
	Vec3 m_ptlocGround, m_nlocGround;
	int m_matidGround;

	int m_nCollEnts = -1;
	CPhysicalEntity **m_pCollEnts;
	float m_moveDist = 0;
	float m_Eaux = 0;

	float m_massLegacy = 80;
	int m_matidLegacy = 0;
};

#endif
