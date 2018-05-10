// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef wheeledvehicleentity_h
#define wheeledvehicleentity_h
#pragma once

struct suspension_point {
	Vec3 pos;
	quaternionf q;
	float scale;
	Vec3 BBox[2];

	float Tscale;
	Vec3 pt; // the uppermost suspension point in car frame
	float fullen; // unconstrained length
	float kStiffness, kStiffnessWeight; // stiffness coefficient
	float kDamping,kDamping0; // damping coefficient
	float len0; // initial length in model
	float Mpt; // hull "mass" at suspension upper point along suspension direction
	quaternionf q0;	// used to calculate geometry transformation from wheel transformation
	Vec3 pos0,ptc0; // ...
	float Iinv;
	float minFriction,maxFriction;
  float kLatFriction;
	int flags0,flagsCollider0;
	unsigned int bDriving    : 1; // if the corresponding wheel a driving wheel
	unsigned int bCanBrake   : 1;
	unsigned int bBlocked    : 1;
	unsigned int bRayCast    : 1;
	unsigned int bSlip       : 1;
	unsigned int bSlipPull   : 1;
	unsigned int bContact    : 1;
	unsigned int bCanSteer   : 1;
	int8 iAxle;
	int8 iBuddy;
	int16 ipart;
	float r,rinv; // wheel radius, 1.0/radius
	float width;

	float curlen; // current length
	float steer; // steering angle
	float rot; // current wheel rotation angle
	float w; // current rotation speed
	float wa; // current angular acceleration
	float T; // wheel's net torque
	float prevTdt;
	float prevw;
	EventPhysCollision *pCollEvent;

	Vec3 ncontact,ptcontact; // filled in RegisterPendingCollisions
	int16 surface_idx[2];
	Vec3 vrel;
	Vec3 rworld;
	float vworld;
	float PN;
	RigidBody *pbody;
	CPhysicalEntity *pent;
	float unproj;
	entity_contact contact;
};

struct SWheeledVehicleEntityNetSerialize
{
	strided_pointer<suspension_point> pSusp;
	float pedal;
	float steer;
	float clutch;
	float wengine;
	bool handBrake;
	int32 curGear;

	void Serialize( TSerialize ser, int nSusp );
};

class CWheeledVehicleEntity : public CRigidEntity {
 public:
	explicit CWheeledVehicleEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap = NULL);
	virtual ~CWheeledVehicleEntity() { delete[] m_susp; }
	virtual pe_type GetType() const { return PE_WHEELEDVEHICLE; }

	virtual int SetParams(pe_params*,int bThreadSafe=1);
	virtual int GetParams(pe_params*) const;
	virtual int Action(pe_action*,int bThreadSafe=1);
	virtual int GetStatus(pe_status*) const;

	enum snapver { SNAPSHOT_VERSION = 1 };
	virtual int GetSnapshotVersion() { return SNAPSHOT_VERSION; }
	virtual int GetStateSnapshot(class CStream &stm, float time_back=0, int flags=0);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0);

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);

	virtual float GetMaxTimeStep(float time_interval);
	virtual float GetDamping(float time_interval);
	virtual float CalcEnergy(float time_interval);
	virtual void CheckAdditionalGeometry(float time_interval);
	virtual int RegisterContacts(float time_interval,int nMaxPlaneContacts);
	virtual int RemoveCollider(CPhysicalEntity *pCollider, bool bRemoveAlways=true);
	virtual int HasContactsWith(CPhysicalEntity *pent);
	virtual int HasPartContactsWith(CPhysicalEntity *pent, int ipart, int bGreaterOrEqual=0);
	virtual void AddAdditionalImpulses(float time_interval);
	virtual void AlertNeighbourhoodND(int mode);
	virtual int Update(float time_interval, float damping);
	virtual void ComputeBBox(Vec3 *BBox, int flags);
	//virtual RigidBody *GetRigidBody(int ipart=-1) { return &m_bodyStatic; }
	virtual void OnContactResolved(entity_contact *pcontact, int iop, int iGroupId);
	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);

	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	void RecalcSuspStiffness();
	float ComputeDrivingTorque(float time_interval);

	suspension_point *m_susp = nullptr;
	int m_suspAlloc = 0;
	float m_enginePower,m_maxSteer;
	float m_engineMaxw,m_engineMinw,m_engineIdlew,m_engineShiftUpw,m_engineShiftDownw,m_gearDirSwitchw,m_engineStartw;
	float m_axleFriction,m_brakeTorque,m_clutchSpeed,m_minBrakingFriction,m_maxBrakingFriction,m_kDynFriction,m_slipThreshold;
	float m_kStabilizer;
	float m_enginePedal,m_steer,m_ackermanOffset,m_clutch,m_wengine;
	float m_gears[12];
	int8 m_nGears,m_iCurGear;
	int8 m_maxGear,m_minGear;
	int8 m_bHandBrake;
	int8 m_nHullParts;
	int8 m_bHasContacts;
	// int8 m_iIntegrationType;
	int8 m_bKeepTractionWhenTilted;
	float m_kSteerToTrack;
	float m_EminRigid,m_EminVehicle;
	float m_maxAllowedStepVehicle,m_maxAllowedStepRigid;
	float m_dampingVehicle;
	//Vec3 m_Ffriction,m_Tfriction;
	float m_timeNoContacts;
	mutable volatile int m_lockVehicle;
  float m_pullTilt;
  float m_drivingTorque;
	float m_maxTilt;
	float m_wheelMassScale;
};

#endif
