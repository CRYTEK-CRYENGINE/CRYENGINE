// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef rigidentity_h
#define rigidentity_h
#pragma once

typedef uint64 masktype;
#define getmask(i) ((uint64)1<<(i))
const int NMASKBITS = 64;

enum constr_info_flags { constraint_limited_1axis=1, constraint_limited_2axes=2, constraint_rope=4, constraint_area=8, constraint_broken=0x10000 };

struct constraint_info {
	int id;
	quaternionf qframe_rel[2];
	Vec3 ptloc[2];
	float limits[2];
	unsigned int flags;
	float damping;
	float sensorRadius;
	CPhysicalEntity *pConstraintEnt;
	int bActive;
	float limit;
	float hardness;
};

struct checksum_item {
	int iPhysTime;
	unsigned int checksum;
};
const int NCHECKSUMS = 1;

struct SRigidEntityNetSerialize
{
	Vec3 pos;
	Quat rot;
	Vec3 vel;
	Vec3 angvel;
	bool simclass;
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	uint8 sequenceNumber;
#endif

	void Serialize( TSerialize ser );
};

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION

#define MAX_STATE_HISTORY_SNAPSHOTS 5
#define MAX_SEQUENCE_HISTORY_SNAPSHOTS 20

struct SRigidEntityNetStateHistory {
	SRigidEntityNetStateHistory() 
		: numReceivedStates(0)
		, numReceivedSequences(0)
		, receivedStatesStart(0)
		, receivedSequencesStart(0)
		, paused(0)
		, sequenceDeltaAverage(0.0f)
	{
		ZeroArray(m_receivedStates);
		ZeroArray(m_receivedSequenceDeltas);
	}

	inline int GetNumReceivedStates() { return numReceivedStates; }
	inline int GetNumReceivedSequences() { return numReceivedSequences; }
	inline SRigidEntityNetSerialize& GetReceivedState(int index) { return m_receivedStates[(index + receivedStatesStart) % MAX_STATE_HISTORY_SNAPSHOTS]; }
	inline float GetAverageSequenceDelta() { return sequenceDeltaAverage; }
	void PushReceivedState(const SRigidEntityNetSerialize& item);
	void PushReceivedSequenceDelta(uint8 delta);

	inline void Clear() {
		numReceivedStates = 0;
		numReceivedSequences = 0;
		receivedStatesStart = 0;
		receivedSequencesStart = 0;
		sequenceDeltaAverage = 0.0f;
	}
	inline int8 Paused() { return paused; }
	inline void SetPaused(int8 p) { paused = p; }

private:
	void UpdateSequenceDeltaAverage(uint8 delta, int sampleCount);

	SRigidEntityNetSerialize m_receivedStates[MAX_STATE_HISTORY_SNAPSHOTS];
	uint8 m_receivedSequenceDeltas[MAX_SEQUENCE_HISTORY_SNAPSHOTS];
	int8 numReceivedStates;
	int8 numReceivedSequences;
	int8 receivedStatesStart;
	int8 receivedSequencesStart;
	int8 paused;
	float sequenceDeltaAverage;
};
#endif

class CRigidEntity : public CPhysicalEntity {
 public:
	explicit CRigidEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap = NULL);
	virtual ~CRigidEntity();
	virtual pe_type GetType() const { return PE_RIGID; }

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);
	virtual int SetParams(pe_params *_params,int bThreadSafe=1);
	virtual int GetParams(pe_params *_params) const;
	virtual int GetStatus(pe_status*) const;
	virtual int Action(pe_action*,int bThreadSafe=1);

	virtual int AddCollider(CPhysicalEntity *pCollider);
	virtual int AddColliderNoLock(CPhysicalEntity *pCollider);
	virtual int RemoveCollider(CPhysicalEntity *pCollider, bool bRemoveAlways=true);
	virtual int RemoveColliderNoLock(CPhysicalEntity *pCollider, bool bRemoveAlways=true);
	virtual int RemoveContactPoint(CPhysicalEntity *pCollider, const Vec3 &pt, float mindist2);
	virtual int HasContactsWith(CPhysicalEntity *pent);
	virtual int HasPartContactsWith(CPhysicalEntity *pent, int ipart, int bGreaterOrEqual=0);
	virtual int HasCollisionContactsWith(CPhysicalEntity *pent);
	virtual int HasConstraintContactsWith(const CPhysicalEntity *pent, int flagsIgnore=0) const;
	virtual int Awake(int bAwake=1,int iSource=0);
	virtual int IsAwake(int ipart=-1) const { return m_bAwake; }
	virtual void AlertNeighbourhoodND(int mode);
	virtual void OnContactResolved(entity_contact *pContact, int iop, int iGroupId);

	virtual RigidBody *GetRigidBody(int ipart=-1,int bWillModify=0) { return &m_body; }
	virtual float GetMassInv() { return m_flags & aef_recorded_physics ? 0:m_body.Minv; }

	enum snapver { SNAPSHOT_VERSION = 9 };
	virtual int GetSnapshotVersion() { return SNAPSHOT_VERSION; }
	virtual int GetStateSnapshot(class CStream &stm, float time_back=0, int flags=0);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags);
	virtual int PostSetStateFromSnapshot();
	virtual unsigned int GetStateChecksum();
	virtual void SetNetworkAuthority(int authoritive, int paused);
	int WriteContacts(CStream &stm,int flags);
	int ReadContacts(CStream &stm,int flags);
	int WriteContacts(TSerialize ser);
	int ReadContacts(TSerialize ser);

	virtual void StartStep(float time_interval);
	virtual float GetMaxTimeStep(float time_interval);
	virtual float GetLastTimeStep(float time_interval) { return m_lastTimeStep; }
	virtual int Step(float time_interval);
	virtual void StepBack(float time_interval);
	virtual int GetContactCount(int nMaxPlaneContacts);
	virtual int RegisterContacts(float time_interval,int nMaxPlaneContacts);
	virtual int Update(float time_interval, float damping);
	virtual float CalcEnergy(float time_interval);
	virtual float GetDamping(float time_interval);
	virtual float GetMaxFriction() { return m_maxFriction; }
	virtual void GetSleepSpeedChange(int ipart, Vec3 &v,Vec3 &w) { v=m_vSleep; w=m_wSleep; }

	virtual bool OnSweepHit(geom_contact &cnt, int icnt, float &dt, Vec3 &vel, int &nsweeps) {
		if (m_nColliders)
			m_minFriction = 3.0f;
		return false;
	}
	virtual void CheckAdditionalGeometry(float time_interval) {}
	virtual void AddAdditionalImpulses(float time_interval) {}
	virtual void RecomputeMassDistribution(int ipart=-1,int bMassChanged=1);

	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);
	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	int RegisterConstraint(const Vec3 &pt0,const Vec3 &pt1, int ipart0, CPhysicalEntity *pBuddy,int ipart1, int flags,int flagsInfo=0, CPhysicalEntity* pConstraintEnt=0);
	int RemoveConstraint(int iConstraint);
	virtual void BreakableConstraintsUpdated();
	entity_contact *RegisterContactPoint(int idx, const Vec3 &pt, const geom_contact *pcontacts, int iPrim0,int iFeature0, 
		int iPrim1,int iFeature1, int flags=contact_new, float penetration=0, int iCaller=get_iCaller_int(), const Vec3 &nloc=Vec3(ZERO));
	int CheckForNewContacts(geom_world_data *pgwd0,intersection_params *pip, int &itmax, Vec3 sweep=Vec3(0), int iStartPart=0,int nParts=-1, int *pFlagsAccum=0);
	virtual int GetPotentialColliders(CPhysicalEntity **&pentlist, float dt=0);
	virtual int CheckSelfCollision(int ipart0,int ipart1) { return 0; }
	void UpdatePenaltyContacts(float time_interval);
	int UpdatePenaltyContact(entity_contact *pContact, float time_interval);
	void VerifyExistingContacts(float maxdist);
	int EnforceConstraints(float time_interval);
	void UpdateConstraints(float time_interval);
	void UpdateContactsAfterStepBack(float time_interval);
	void ApplyBuoyancy(float time_interval,const Vec3 &gravity,pe_params_buoyancy *pb,int nBuoys);
	void ArchiveContact(entity_contact *pContact, float imp=0, int bLastInGroup=1, float r=0.0f);
	int CompactContactBlock(entity_contact *pContact,int endFlags, float maxPlaneDist, int nMaxContacts,int &nContacts,
		entity_contact *&pResContact, Vec3 &n,float &maxDist, const Vec3 &ptTest, const Vec3 &dirTest) const;
	void ComputeBBoxRE(coord_block_BBox *partCoord);
	void UpdatePosition(int bGridLocked);
	int PostStepNotify(float time_interval,pe_params_buoyancy *pb,int nMaxBuoys,int iCaller);
	masktype MaskIgnoredColliders(int iCaller, int bScheduleForStep=0);
	void UnmaskIgnoredColliders(masktype constraint_mask, int iCaller);
	void FakeRayCollision(CPhysicalEntity *pent, float dt);
	int ExtractConstraintInfo(int i, masktype constraintMask, pe_action_add_constraint &aac);
	EventPhysJointBroken &ReportConstraintBreak(EventPhysJointBroken &epjb, int i);
	virtual bool IgnoreCollisionsWith(const CPhysicalEntity *pent, int bCheckConstraints=0) const;
	virtual void OnNeighbourSplit(CPhysicalEntity *pentOrig, CPhysicalEntity *pentNew);

	void AttachContact(entity_contact *pContact, int i, CPhysicalEntity *pCollider);
	void DetachContact(entity_contact *pContact, int i=-1,int bCheckIfEmpty=1,int bAcquireContactLock=1);
	void DetachAllContacts();
	void MoveConstrainedObjects(const Vec3 &dpos, const quaternionf &dq);
	virtual void DetachPartContacts(int ipart,int iop0, CPhysicalEntity *pent,int iop1, int bCheckIfEmpty=1);
	void CapBodyVel();
	void CleanupAfterContactsCheck(int iCaller);
	void CheckContactConflicts(geom_contact *pcontacts, int ncontacts, int iCaller);
	void ProcessContactEvents(geom_contact* pcontact, int i, int iCaller);
	virtual void DelayedIntersect(geom_contact *pcontacts, int ncontacts, CPhysicalEntity **pColliders, int (*iCollParts)[2]);
	void ProcessCanopyContact(geom_contact *pcontacts, int i, float time_interval, int iCaller);
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	float GetInterpSequenceNumber();
	bool GetInterpolatedState(float sequenceNumber, SRigidEntityNetSerialize &interpState);
	void UpdateStateFromNetwork();
	uint8 GetLocalSequenceNumber() const;
#endif

	Vec3 m_posNew;			 
	quaternionf m_qNew;
	Vec3 m_BBoxNew[2];
	int m_iVarPart0;

	unsigned int m_bCollisionCulling     : 1;
	unsigned int m_bJustLoaded           : 8;
	unsigned int m_bStable               : 2;
	unsigned int m_bHadSeverePenetration : 1;
	unsigned int m_bSteppedBack          : 1;
	unsigned int m_nStepBackCount        : 4;
	unsigned int m_bCanSweep             : 1;
	unsigned int m_nNonRigidNeighbours   : 8;
	unsigned int m_bFloating             : 1;
	unsigned int m_bDisablePreCG				 : 1;
	unsigned int m_bSmallAndFastForced   : 1;

	unsigned int m_bAwake              : 8;
	unsigned int m_nSleepFrames        : 5;
	unsigned int m_nFutileUnprojFrames : 3;
	unsigned int m_nEvents             : 5;
	unsigned int m_nMaxEvents          : 5;
	unsigned int m_icollMin            : 5;
	unsigned int m_alwaysSweep         : 1;

	entity_contact **m_pColliderContacts;
	masktype *m_pColliderConstraints;
	entity_contact *m_pContactStart,*m_pContactEnd;
	int m_nContacts;
	entity_contact *m_pConstraints;
	constraint_info *m_pConstraintInfos;
	int m_nConstraintsAlloc;
	masktype m_constraintMask;
	unsigned int m_nRestMask;
	int m_nPrevColliders;
	int m_collTypes;

	float m_velFastDir,m_sizeFastDir;

	float m_timeStepFull;
	float m_timeStepPerformed;
	float m_lastTimeStep;
	float m_minAwakeTime;
	float m_nextTimeStep;

	Vec3 m_gravity,m_gravityFreefall;
	float m_Emin;
	float m_maxAllowedStep;
	Vec3 m_vAccum,m_wAccum;
	float m_damping,m_dampingFreefall;
	float m_dampingEx;
	float m_maxw;
	
	float m_minFriction,m_maxFriction;
	Vec3 m_vSleep,m_wSleep;
	entity_contact *m_pStableContact;

	RigidBody m_body;
	Vec3 m_Pext,m_Lext;
	Vec3 m_prevPos,m_prevv,m_prevw;
	quaternionf m_prevq;
	float m_E0,m_Estep;
	float m_timeCanopyFallen;
	unsigned int m_bCanopyContact : 8;
	unsigned int m_nCanopyContactsLost : 15;
	unsigned int m_sequenceOffset : 8;
	unsigned int m_hasAuthority : 1;
	Vec3 m_Psoft,m_Lsoft;
	Vec3 m_forcedMove;
	float m_sweepGap;

	EventPhysCollision **m_pEventsColl;
	int m_iLastLogColl;
	float m_vcollMin;
	int m_iLastLog;
	EventPhysPostStep *m_pEvent;

	float m_waterDamping;
	float m_kwaterDensity,m_kwaterResistance;
	float m_EminWater;
	float m_submergedFraction;

	int m_iLastConstraintIdx;
	volatile int m_lockConstraintIdx;
	volatile int m_lockContacts;
	volatile int m_lockStep;
	union {
		mutable volatile int m_lockNetInterp;
		int m_iLastChecksum;
	};
	union {
		checksum_item m_checksums[NCHECKSUMS];
		struct SRigidEntityNetStateHistory* m_pNetStateHistory;
	};
};

struct REdata {
	CPhysicalEntity *CurColliders[128];
	int CurCollParts[128][2];
	int idx0NoColl;
	int nLastContacts;
};
extern REdata g_REdata[];

#define g_CurColliders  g_REdata[iCaller].CurColliders
#define g_CurCollParts  g_REdata[iCaller].CurCollParts
#define g_idx0NoColl	  g_REdata[iCaller].idx0NoColl
#define g_nLastContacts g_REdata[iCaller].nLastContacts


#endif
