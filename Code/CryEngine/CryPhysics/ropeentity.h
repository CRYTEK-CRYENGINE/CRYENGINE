// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ropeentity_h
#define ropeentity_h

struct rope_vtx {
	Vec3 pt,pt0;
	Vec3 vel;
	Vec3 dir;
	Vec3 ncontact;
	Vec3 vcontact;
	float dP;
	CPhysicalEntity *pContactEnt;
	unsigned int kdP : 1;
	unsigned int iContactPart : 31;
};

struct rope_segment : rope_vtx {
	//~rope_segment() { if (pContactEnt) pContactEnt->Release(); }
	Vec3 vel_ext;
	Vec3 ptdst;
	int bRecheckContact : 1;
	int bRecalcDir : 1;
	int iCheckPart : 30;
	float tcontact;
	float vreq;
	int iPrim,iFeature;
	int iVtx0;
};

struct rope_vtx_attach {
	~rope_vtx_attach() { pent->Release(); }
	int ivtx;
	CPhysicalEntity *pent;
	int ipart;
	Vec3 ptloc;
	Vec3 pt,v;
};

struct SRopeCheckPart {
	Vec3 offset;
	Matrix33 R;
	float scale,rscale;
	box bbox;
	CPhysicalEntity *pent;
	int ipart;
	Vec3 pos0;
	quaternionf q0;
	CGeometry *pGeom;
	int bProcess;
	int bVtxUnproj;
	Vec3 v,w;
};

#ifdef DEBUG_ROPES
template<class T> struct safe_array {
	safe_array(int *_psizeDyn, int _sizeConst=0) { data=0; psizeDyn=_psizeDyn; sizeConst=_sizeConst; }
	safe_array& operator=(T* pdata) { data=pdata; return *this; }
	// cppcheck-suppress operatorEqVarError
	safe_array& operator=(const safe_array<T> &op) { data=op.data; return *this; }
	T& operator[](int idx) { 
		if (idx<0 || idx>=*psizeDyn+sizeConst)
			__asm int 3;
		return data[idx];
	}
	const T& operator[](int idx) const { return data[idx]; }
	operator T*() { return data; }
	T* data;
	int *psizeDyn,sizeConst;
};
#define ROPE_SAFE_ARRAY(T) safe_array<T>
#else
#define ROPE_SAFE_ARRAY(T) T*
#endif



class CRopeEntity : public CPhysicalEntity {
 public:
	explicit CRopeEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap = NULL);
	virtual ~CRopeEntity();
	virtual pe_type GetType() const { return PE_ROPE; }

	virtual int SetParams(pe_params*,int bThreadSafe=1);
	virtual int GetParams(pe_params*) const;
	virtual int GetStatus(pe_status*) const;
	virtual int Action(pe_action*,int bThreadSafe=1);

	virtual void StartStep(float time_interval);
	virtual float GetMaxTimeStep(float time_interval);
	virtual int Step(float time_interval);
	virtual int Awake(int bAwake=1,int iSource=0);
	virtual int IsAwake(int ipart=-1) const { return m_bPermanent; }
	virtual void AlertNeighbourhoodND(int mode);
	virtual int RayTrace(SRayTraceRes& rtr);
	virtual float GetMass(int ipart) { return m_mass/m_nSegs; }
	virtual float GetMassInv() { return 1E26f; }
	virtual RigidBody *GetRigidBodyData(RigidBody *pbody, int ipart=-1);
	virtual void GetLocTransform(int ipart, Vec3 &offs, quaternionf &q, float &scale, const CPhysicalPlaceholder *trg) const;
	void EnforceConstraints(float seglen, const quaternionf& qtv,const Vec3& offstv,float scaletv, int bTargetPoseActive, float dt=0);
	virtual void OnNeighbourSplit(CPhysicalEntity *pentOrig, CPhysicalEntity *pentNew);
	virtual int RegisterContacts(float time_interval,int nMaxPlaneContacts);
	virtual int Update(float time_interval, float damping);
	virtual float GetDamping(float time_interval) { return max(0.0f,1.0f-m_damping*time_interval); }
	virtual float CalcEnergy(float time_interval) { return time_interval>0 ? m_energy:0.0f; }
	virtual float GetLastTimeStep(float time_interval) { return m_lastTimeStep; }
	virtual void ApplyVolumetricPressure(const Vec3 &epicenter, float kr, float rmin);
	void RecalcBBox();

	void CheckCollisions(int iDir, SRopeCheckPart *checkParts,int nCheckParts, float seglen,float rseglen, const Vec3 &hingeAxis);
	void StepSubdivided(float time_interval, SRopeCheckPart *checkParts,int nCheckParts, float seglen);
	void ZeroLengthStraighten(float time_interval);
	float Solver(float time_interval, float seglen);
	void ApplyStiffness(float time_interval, int bTargetPoseActive, const quaternionf &qtv,const Vec3 &offstv,float scaletv);

	enum snapver { SNAPSHOT_VERSION = 8 };
	virtual int GetStateSnapshot(CStream &stm, float time_back=0,int flags=0);
	virtual int SetStateFromSnapshot(CStream &stm, int flags);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0,int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags);

	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);
	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	int GetVertices(strided_pointer<Vec3>& verts) const;
	virtual float GetExtent(EGeomForm eForm) const;
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;

	Vec3 m_gravity,m_gravity0;
	float m_damping;
	float m_maxAllowedStep;
	float m_Emin;
	float m_timeStepPerformed,m_timeStepFull;
	int m_nSlowFrames;
	float m_lastTimeStep;
	
	int m_nSleepingNeighboursFrames;
	int m_bHasContacts;
	int m_bContactsRegistered;
	mutable volatile int m_lockVtx;
	volatile int m_lockAwake;

	float m_length;
	
	//Grouping these values as they are all used by CRopeEntity::GetStatus(). This avoids L2 cache line pollution
	int m_nSegs;
	ROPE_SAFE_ARRAY(rope_segment) m_segs;
	float m_timeLastActive;
	int m_bTargetPoseActive;
	float m_stiffnessAnim;
	Vec3 m_lastposHost;
	quaternionf m_lastqHost;

	float m_mass;
	float m_collDist;
	int m_surface_idx;
	float m_friction;
	float m_stiffness;
	float m_dampingAnim,m_stiffnessDecayAnim;
	Vec3 m_wind,m_wind0,m_wind1;
  float m_airResistance,m_windVariance,m_windTimer;
	float m_waterResistance,m_rdensity;
	float m_jointLimit,m_jointLimitDecay;
	float m_szSensor;
	float m_maxForce;
	int m_flagsCollider;
	int m_collTypes;
	float m_penaltyScale;
	int m_maxIters;
	float m_attachmentZone;
	float m_minSegLen;
	float m_unprojLimit;
	float m_noCollDist;	

	CPhysicalEntity *m_pTiedTo[2];
	Vec3 m_ptTiedLoc[2];
	int m_iTiedPart[2];
	int m_idConstraint;
	int m_iConstraintClient;
	Vec3 m_posBody[2][2];
	quaternionf m_qBody[2][2];
	Vec3 m_dir0dst;
	Vec3 m_collBBox[2];

	ROPE_SAFE_ARRAY(rope_vtx) m_vtx;
	ROPE_SAFE_ARRAY(rope_vtx) m_vtx1;
	ROPE_SAFE_ARRAY(rope_solver_vtx) m_vtxSolver;
	int m_nVtx,m_nVtxAlloc,m_nVtx0;
	int m_nFragments;
	//class CTriMesh *m_pMesh;
	//Vec3 m_lastMeshOffs;
	int m_nMaxSubVtx;
	ROPE_SAFE_ARRAY(int) m_idx;
	int m_bStrained;
	float m_frictionPull;
	float m_energy;
	entity_contact *m_pContact;

	rope_vtx_attach *m_attach;
	int m_nAttach;

	void MeshVtxUpdated();
	void AllocSubVtx();
	void FillVtxContactData(rope_vtx *pvtx,int iseg, SRopeCheckPart &cp, geom_contact *pcontact);
};

#endif
