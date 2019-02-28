// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef particleentity_h
#define particleentity_h
#pragma once

class CParticleEntity : public CPhysicalEntity {
 public:
	explicit CParticleEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap = NULL);
	virtual ~CParticleEntity();
	virtual pe_type GetType() const { return PE_PARTICLE; }

	virtual int SetParams(pe_params*,int bThreadSafe=1);
	virtual int GetParams(pe_params*) const;
	virtual int GetStatus(pe_status*) const;
	virtual int Action(pe_action*,int bThreadSafe=1);
	virtual int Awake(int bAwake=1,int iSource=0);
	virtual int IsAwake(int ipart=-1) const;
	virtual int RayTrace(SRayTraceRes&);
	virtual void ComputeBBox(Vec3 *BBox, int flags=update_part_bboxes) { 
		Vec3 sz(m_dim,m_dim,m_dim); BBox[0]=m_pos-sz; BBox[1]=m_pos+sz; 
	}
	virtual float GetMass(int ipart) { return m_mass; }
	virtual void AlertNeighbourhoodND(int mode) { if (m_pColliderToIgnore) m_pColliderToIgnore->Release(); m_pColliderToIgnore=0; }

	enum snapver { SNAPSHOT_VERSION = 3 };
	virtual int GetStateSnapshot(class CStream &stm,float time_back=0,int flags=0);
	virtual int SetStateFromSnapshot(class CStream &stm, int flags);

	virtual int GetStateSnapshot(TSerialize ser, float time_back, int flags);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags);

	virtual void StartStep(float time_interval);
	virtual float GetMaxTimeStep(float time_interval);
	virtual int Step(float time_interval) { return DoStep(time_interval); }
	virtual int DoStep(float time_interval,int iCaller=get_iCaller_int());

	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);
	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	float m_mass,m_dim,m_rdim,m_dimLying;
	float m_kAirResistance,m_kWaterResistance, m_accThrust, m_kAccLift;
	Vec3 m_gravity,m_gravity0,m_waterGravity,m_rollax,m_normal;
	Vec3 m_heading,m_vel,m_wspin;
	quaternionf m_qspin;
	float m_minBounceVel;
	float m_minVel;
	int m_surface_idx;
	CPhysicalEntity *m_pColliderToIgnore;
	int m_collTypes;
	Vec3 m_slide_normal;
	float m_timeSurplus;
	float m_depth;
	Vec3 m_velMedium;
	
	float m_timeStepPerformed,m_timeStepFull;
	float m_timeForceAwake;
	float m_sleepTime;

	unsigned int m_areaCheckPeriod : 8;
	unsigned int m_nStepCount : 8;
	mutable unsigned int m_bHadCollisions : 1;
	unsigned int m_bRecentCollisions : 3;
	unsigned int m_bForceAwake : 2;
	unsigned int m_iPierceability : 5;
	unsigned int m_bSliding : 1;
	unsigned int m_bDontPlayHitEffect : 1;

	mutable volatile int m_lockParticle;
	static CParticleEntity *g_pCurParticle[MAX_PHYS_THREADS+1];
};

#endif