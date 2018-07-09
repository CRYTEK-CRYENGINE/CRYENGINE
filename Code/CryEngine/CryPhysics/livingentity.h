// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef livingentity_h
#define livingentity_h
#pragma once

const int SZ_ACTIONS = 128;
const int SZ_HISTORY = 128;

struct le_history_item {
	Vec3 pos;
	Quat q;
	Vec3 v;
	int bFlying;
	Vec3 nslope;
	float timeFlying;
	float minFlyTime;
	float timeUseLowCap;
	int idCollider;
	int iColliderPart;
	Vec3 posColl;
	float dt;
};

struct le_contact {
	CPhysicalEntity *pent;
	int ipart;
	Vec3 pt,ptloc;
	Vec3 n;
	float penetration;
	Vec3 center;
	entity_contact *pSolverContact[2];
};

struct CRY_ALIGN(16) le_tmp_contact {
	CPhysicalEntity* pent;
	Vec3 ptcontact;
	Vec3 ncontact;
	int ipart;
	int idmat;
	int iPrim;
	float tmin;
};

struct CRY_ALIGN(16) le_precomp_part { 
	Vec3 BBox[2]; 
	Quat partrot; 
	Vec3 partoff;
	float partscale; 
	int partflags; 
	int ipart; 
	int surface_idx;
	IGeometry* pgeom; 
}; 

struct CRY_ALIGN(16) le_precomp_entity { 
	Vec3 BBox[2]; 
	Vec3 sz; 
	int entflags; 
	float massinv; 
	int bCheckBBox; 
	int entType;
	int nParts;
	int iSimClass;
	int ignoreCollisionsWith;
	int iPartsBegin, iPartsEnd;
	CPhysicalEntity* pent;
}; 

struct le_precomp_data { 
	le_precomp_entity *pents; 
	le_precomp_part *pparts;
	int nPrecompEntsAlloc, nPrecompPartsAlloc; 
}; 

struct SLivingEntityNetSerialize {
	Vec3 pos;
	Vec3 vel;
	Vec3 velRequested;
	void Serialize( TSerialize ser );
};

class CLivingEntity : public CPhysicalEntity {
public:
	explicit CLivingEntity(CPhysicalWorld *pWorld, IGeneralMemoryHeap* pHeap = NULL);
	virtual ~CLivingEntity();
	virtual pe_type GetType() const { return PE_LIVING; }

	virtual int SetParams(pe_params*,int bThreadSafe=1);
	virtual int GetParams(pe_params*) const;
	virtual int GetStatus(pe_status*) const;
	virtual int Action(pe_action*,int bThreadSafe=1);
	virtual void StartStep(float time_interval);
	virtual float GetMaxTimeStep(float time_interval);
	virtual int Step(float time_interval);
	//int StepBackEx(float time_interval, bool bRollbackHistory=true);
	virtual void StepBack(float time_interval) { /*StepBackEx(time_interval);*/ }
	virtual float CalcEnergy(float time_interval);
	virtual int RegisterContacts(float time_interval,int nMaxPlaneContacts);
	virtual int Update(float time_interval, float damping);
	virtual int Awake(int bAwake=1,int iSource=0);
	virtual void AlertNeighbourhoodND(int mode) { ReleaseGroundCollider(); CPhysicalEntity::AlertNeighbourhoodND(mode); }
	virtual void ComputeBBox(Vec3 *BBox, int flags=update_part_bboxes);
	virtual RigidBody *GetRigidBody(int ipart=-1,int bWillModify=0);
	virtual RigidBody *GetRigidBodyData(RigidBody *pbody, int ipart=-1) { 
		pbody->zero(); pbody->M=m_mass; pbody->Minv=m_massinv;
		pbody->v=m_vel; pbody->pos=m_pos; return pbody;
	}
	virtual void OnContactResolved(entity_contact *pContact, int iop, int iGroupId);

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);

	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);

	enum snapver { SNAPSHOT_VERSION = 2 };
	virtual int GetStateSnapshot(class CStream &stm, float time_back=0, int flags=0);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0);

	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	virtual float GetMassInv() { return m_massinv; }
	virtual void GetSpatialContactMatrix(const Vec3 &pt, int ipart, float Ibuf[][6]) {
		Ibuf[3][0]+=m_massinv; Ibuf[4][1]+=m_massinv; Ibuf[5][2]+=m_massinv;
	}
	float ShootRayDown(le_precomp_entity* pents, int nents, le_precomp_part *pparts, const Vec3 &pos,
		Vec3 &nslope, float time_interval=0, bool bUseRotation=false, bool bUpdateGroundCollider=false,bool bIgnoreSmallObjects=true);
	void AddLegsImpulse(const Vec3 &vel, const Vec3 &nslope, bool bInstantChange);
	void ReleaseGroundCollider();
	void SetGroundCollider(CPhysicalEntity *pCollider, int bAcceptStatic=0);
	Vec3 SyncWithGroundCollider(float time_interval);
	void RegisterContact(const Vec3 &posSelf, const Vec3& pt,const Vec3& n, CPhysicalEntity *pCollider, int ipart,int idmat, 
		float imp=0, int bLegsContact=0, int iPrim=-1, int ipartMin=0);
	void RegisterUnprojContact(const le_contact &unproj);
	float UnprojectionNeeded(const Vec3 &pos,const quaternionf &qrot, float hCollider,float hPivot, const Vec3 &newdim,int bCapsule, 
		Vec3 &dirUnproj, int iCaller=get_iCaller()) const;

	//void AllocateExtendedHistory();

	void UpdatePosition(const Vec3 &pos, const Vec3 *BBox, int bGridLocked);
	void Step_HandleFlying(Vec3 &vel, const Vec3& velGround, int bWasFlying, const Vec3& heightAdj, const float kInertia, const float time_interval);
	void Step_HandleWasFlying(Vec3& vel, int& bFlying, const Vec3& axis, const int bGroundContact);
	void Step_HandleLivingEntity();

	Vec3 m_vel,m_velRequested,m_gravity,m_nslope;
	float m_dtRequested;
	float m_kInertia,m_kInertiaAccel,m_kAirControl,m_kAirResistance, m_hCyl,m_hEye,m_hPivot;
	Vec3 m_size;
	float m_dh,m_dhSpeed,m_dhAcc,m_stablehTime,m_hLatest,m_nodSpeed;
	float m_mass,m_massinv;
	int m_surface_idx;
	int m_lastGroundSurfaceIdx,m_lastGroundSurfaceIdxAux;
	int m_lastGroundPrim;
	float m_timeFlying,m_timeForceInertia;
	float m_slopeSlide,m_slopeClimb,m_slopeJump,m_slopeFall;
	float m_maxVelGround;
	float m_groundContactEps;
	float m_timeImpulseRecover;
	CCylinderGeom *m_pCylinderGeom;
	float m_hHead;
	CSphereGeom *m_pHeadGeom;
	phys_geometry m_CylinderGeomPhys;
	float m_timeUseLowCap;
	float m_timeSinceStanceChange;
	float m_timeSinceImpulseContact;
	//float m_dhHist[2],m_timeOnStairs;
	float m_timeStepFull,m_timeStepPerformed;
	int m_iSnapshot;
	int m_iTimeLastSend;
	int m_collTypes;
	CPhysicalEntity *m_pLivingEntToIgnore;

	unsigned int m_bFlying : 1;
	unsigned int m_bJumpRequested : 1;
	unsigned int m_bSwimming : 1;
	unsigned int m_bUseCapsule : 1;
	unsigned int m_bIgnoreCommands : 1;
	unsigned int m_bStateReading : 1;
	unsigned int m_bActive : 1;
	unsigned int m_bActiveEnvironment : 1;
	unsigned int m_bStuck : 1;
	unsigned int m_bReleaseGroundColliderWhenNotActive : 1;
	mutable unsigned int m_bHadCollisions : 1;
	int m_bSquashed;

	CPhysicalEntity *m_pLastGroundCollider;
	int m_iLastGroundColliderPart;
	Vec3 m_posLastGroundColl;
	Vec3 m_velGround;
	Vec3 m_deltaPos,m_posLocal;
	Vec3 m_deltaV;
	Quat m_deltaQRot;
	float m_timeSmooth;
	float m_timeRotChanged;

// 	le_history_item *m_history,m_history_buf[4];
// 	int m_szHistory,m_iHist;
// 	pe_action_move *m_actions,m_actions_buf[16];
// 	int m_szActions,m_iAction;
// 	int m_iCurTime,m_iRequestedTime;

	le_contact *m_pContacts;
	int m_nContacts,m_nContactsAlloc;
	RigidBody *m_pBody;

	//Vec3 m_posLogged;
	//int m_timeLogged;

	mutable volatile int m_lockLiving;
	mutable volatile int m_lockStep;

	bool m_forceFly;

	static le_precomp_data s_precompData[MAX_PHYS_THREADS+1]; 
};

#endif
