// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef articulatedentity_h
#define articulatedentity_h
#pragma once

struct featherstone_data {
	Vec3i qidx2axidx,axidx2qidx;
	Vec3 Ya_vec[2];
	Vec3 dv_vec[2];
	Vec3 s_vec[3][2];
	Vec3 Q;
	float qinv[9];
	float qinv_down[9];
	float s[18];
	CRY_ALIGN(16) float Ia[6][6];
	float Ia_s[18];
	CRY_ALIGN(16) float Ia_s_qinv_sT[6][6];
	CRY_ALIGN(16) float s_qinv_sT[6][6];
	CRY_ALIGN(16) float s_qinv_sT_Ia[6][6];
	float qinv_sT[3][6];
	float qinv_sT_Ia[3][6];
	CRY_ALIGN(16) float Iinv[6][6];
	int useTree;
	int iparent;
	int jointSize;
	Matrix33 *pM0inv;
};

enum joint_flags_aux { joint_rotate_pivot=010000000 };

struct ae_joint : ArticulatedBody {
	ae_joint() {
		memset(this,0,sizeof(*this));
		iParent=-2; idbody=-1;
		MARK_UNUSED dq_req.x, dq_req.y, dq_req.z;
		limits[0].Set(-1E10,-1E10,-1E10);
		limits[1].Set(1E10,1E10,1E10);
		flags=all_angles_locked; 
		quat0.SetIdentity();
		prev_qrot.SetIdentity();
		quat.SetIdentity();
		body.q.SetIdentity(); body.qfb.SetIdentity();
	}
	~ae_joint() {
		if (fsbuf) delete[] fsbuf;
	}

	Ang3 q;
	Ang3 qext;
	Vec3 dq;
	Vec3 dqext;
	Vec3 dq_req;
	Vec3 dq_limit;
	Vec3 ddq;
	quaternionf quat;

	Ang3 prev_q;
	Vec3 prev_dq;
	Vec3 prev_pos,prev_v,prev_w;
	quaternionf prev_qrot;
	Ang3 q0;
	Vec3 Fcollision,Tcollision;
	Vec3 vSleep,wSleep;

	unsigned int flags;
	quaternionf quat0;
	Vec3 limits[2];
	Vec3 bounciness;
	Vec3 ks,kd;
	Vec3 qdashpot,kdashpot;
	Vec3 pivot[2];

	int iStartPart,nParts;
	int iLevel;
	masktype selfCollMask;
	struct entity_contact *pContact;
	int bAwake;
	int bQuat0Changed;
	int bHasExtContacts;
	
	int idbody;
	Vec3 dv_body,dw_body;
	Vec3 Pimpact,Limpact;
	int nActiveAngles;
	Vec3 rotaxes[3];
	Matrix33 I;

	char *fsbuf;
};

struct ae_part_info {
	Vec3 pos;
	quaternionf q;
	float scale;
	Vec3 BBox[2];
	quaternionf q0;
	Vec3 pos0;
	int iJoint;
	int idbody;
	Vec3 posHist[2];
	quaternionf qHist[2];
};


class CArticulatedEntity : public CRigidEntity {
 public:
	explicit CArticulatedEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap = NULL);
	virtual ~CArticulatedEntity();
	virtual pe_type GetType() const { return PE_ARTICULATED; }
	virtual void AlertNeighbourhoodND(int mode);

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);
	virtual int SetParams(pe_params *_params,int bThreadSafe=1);
	virtual int GetParams(pe_params *_params) const;
	virtual int GetStatus(pe_status *_status) const;
	virtual int Action(pe_action*,int bThreadSafe=1);

	virtual RigidBody *GetRigidBody(int ipart=-1,int bWillModify=0);
	virtual RigidBody *GetRigidBodyData(RigidBody *pbody, int ipart=-1);
	virtual void GetLocTransformLerped(int ipart, Vec3 &offs, quaternionf &q, float &scale, float timeBack, const CPhysicalPlaceholder *trg) const;
	virtual void OnContactResolved(entity_contact *pcontact, int iop, int iGroupId);

	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	enum snapver { SNAPSHOT_VERSION = 6 };
	virtual int GetStateSnapshot(CStream &stm, float time_back=0,int flags=0);
	virtual int SetStateFromSnapshot(CStream &stm, int flags);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags);

	virtual float GetMaxTimeStep(float time_interval);
	virtual int Step(float time_interval);
	virtual void StepBack(float time_interval);
	virtual int RegisterContacts(float time_interval,int nMaxPlaneContacts);
	virtual int Update(float time_interval, float damping);
	virtual float CalcEnergy(float time_interval);
	virtual float GetDamping(float time_interval);
	virtual void GetSleepSpeedChange(int ipart, Vec3 &v,Vec3 &w) { int i=m_infos[ipart].iJoint; v=m_joints[i].vSleep; w=m_joints[i].wSleep; }

	virtual int GetPotentialColliders(CPhysicalEntity **&pentlist, float dt=0);
	virtual int CheckSelfCollision(int ipart0,int ipart1);
	virtual int IsAwake(int ipart=-1) const;
	virtual void RecomputeMassDistribution(int ipart=-1,int bMassChanged=1);
	virtual void BreakableConstraintsUpdated();
	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);

	int SyncWithHost(int bRecalcJoints,float time_interval);
	void SyncBodyWithJoint(int idx, int flags=3);
	void SyncJointWithBody(int idx, int flags=1);
	void UpdateJointRotationAxes(int idx);
	void CheckForGimbalLock(int idx);
	int GetUnprojAxis(int idx, Vec3 &axis);

	int StepJoint(int idx, float time_interval,int &bBounced, int bFlying, int iCaller);
	void JointListUpdated();
	void StepFeatherstone(float time_interval, int bBounced, Matrix33 &M0host);
	int CalcBodyZa(int idx, float time_interval, vectornf &Za_change);
	int CalcBodyIa(int idx, matrixf& Ia_change, int bIncludeLimits=1);
	void CalcBodiesIinv(int bLockLimits);
	int CollectPendingImpulses(int idx,int &bNotZero,int bBounce=1);
	void PropagateImpulses(const Vec3 &dv,int bLockLimits=0,int bApplyVel=1,const Vec3 &dw=Vec3(ZERO));
	void CalcVelocityChanges(float time_interval, const Vec3 &dv,const Vec3 &dw);
	void GetJointTorqueResponseMatrix(int idx, Matrix33 &K);
	void UpdatePosition(int bGridLocked);
	void UpdateJointDyn();
	void AssignContactsToJoints();
	int UpdateHistory(int bStepDone) {
		if (bStepDone) {
			m_posHist[0]=m_posHist[1];m_posHist[1]=m_pos; m_qHist[0]=m_qHist[1];m_qHist[1]=m_qrot;
			for(int i=0;i<m_nParts;i++) {
				m_infos[i].posHist[0] = m_infos[i].posHist[1]; m_infos[i].posHist[1] = m_infos[i].pos;
				m_infos[i].qHist[0] = m_infos[i].qHist[1]; m_infos[i].qHist[1] = m_infos[i].q;
			}
			float rhistTime0 = m_rhistTime;
			m_rhistTime = 1.0f/max(m_timeStepFull,0.0001f);
			if (rhistTime0==0)
				UpdateHistory(1);
		}
		return bStepDone;
	}

	int IsChildOf(int idx, int iParent) { return isnonneg(iParent) & isneg(iParent-idx) & isneg(idx-iParent-m_joints[iParent].nChildrenTree-1); }
	entity_contact *CreateConstraintContact(int idx);
	void AllocFSData();

	ae_part_info *m_infos;
	ae_joint *m_joints;
	int m_nJoints, m_nJointsAlloc;
	Vec3 m_posPivot, m_offsPivot;
	Vec3 m_acc,m_wacc;
	Matrix33 m_M0inv;
	Vec3 m_Ya_vec[2];
	float m_simTime,m_simTimeAux;
	float m_scaleBounceResponse;
	int m_bGrounded;
	int m_nRoots;
	int m_bInheritVel;
	CPhysicalEntity *m_pHost;
	Vec3 m_posHostPivot;
	quaternionf m_qHostPivot;
	Vec3 m_velHost;
	Vec3 m_rootImpulse;
	int m_bCheckCollisions;
	int m_bFeatherstone;
	int m_bExertImpulse;
	int m_iSimType,m_iSimTypeLyingMode;
	int m_iSimTypeCur;
	int m_iSimTypeOverride;
	int m_bIaReady;
	int m_bPartPosForced;
	int m_bFastLimbs;
	float m_maxPenetrationCur;
	int m_bUsingUnproj;
	Vec3 m_prev_pos,m_prev_vel;
	int m_bUpdateBodies;
	int m_nDynContacts,m_bInGroup;
	int m_bIgnoreCommands;
	float m_Ejoints;

	int m_nCollLyingMode;
	Vec3 m_gravityLyingMode;
	float m_dampingLyingMode;
	float m_EminLyingMode;
	int m_nBodyContacts;

	Vec3 m_posHist[2];
	quaternionf m_qHist[2];
	float m_rhistTime;
	int m_bContactsAssigned;

	mutable volatile int m_lockJoints;

	CPhysicalEntity **m_pCollEntList;
	int m_nCollEnts;
};

#endif
