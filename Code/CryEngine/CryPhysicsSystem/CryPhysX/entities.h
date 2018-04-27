// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef entities_h
#define entities_h
#pragma once

enum { pef_auto_id = 0x40000000 };

struct phys_geometry_refcnt : phys_geometry {
	~phys_geometry_refcnt() { delete[] pMatMapping; pGeom->Release(); }
	int AddRef() { return CryInterlockedIncrement((volatile int*)&nRefCount); }
	int Release();
};

template<class PxClass> struct comp_userDataPtr {
	bool operator() (const PxClass* p0, const PxClass* p1) const { return (int)(UINT_PTR)p0->userData < (int)(UINT_PTR)p1->userData; }
	static PxClass *ptrFromKey(int &key) { return (PxClass*)((INT_PTR)&key-(INT_PTR)&((PxClass*)0)->userData); }
};

template<class PxClass> inline void AwakeEnt(PxClass *ent, bool awake=true, float minTime=0) 
{
	if (ent && ent->getScene()) {
		if (awake) {
			ent->wakeUp();
			ent->setWakeCounter(max(minTime, ent->getWakeCounter()));
		} else
			ent->putToSleep();
	}
}

class PhysXEnt : public IPhysicalEntity
{
public:
	PhysXEnt() {}
	PhysXEnt(pe_type type, int id, struct pe_params *params=0);
	virtual ~PhysXEnt();
	virtual pe_type GetType() const { return m_type; }
	virtual int AddRef() { return CryInterlockedIncrement(&m_nRefCount); }
	virtual int Release() { return CryInterlockedDecrement(&m_nRefCount); }
	virtual int SetParams(pe_params* params, int bThreadSafe = 1);
	virtual int GetParams(pe_params* params) const;
	virtual int GetStatus(pe_status* status) const;
	virtual int Action(pe_action*, int bThreadSafe = 1);
	virtual int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1);
	virtual void RemoveGeometry(int id, int bThreadSafe = 1);
	virtual void* GetForeignData(int itype = 0) const { return itype==m_iForeignData ? m_pForeignData : nullptr; }
	virtual int GetiForeignData() const { return m_iForeignData; }
	virtual int GetStateSnapshot(class CStream& stm, float time_back = 0, int flags = 0)  { return 0; }
	virtual int SetStateFromSnapshot(class CStream& stm, int flags = 0) { return 0; }
	virtual int PostSetStateFromSnapshot() { return 0; }
	virtual unsigned int GetStateChecksum() { return 0; }
	virtual void SetNetworkAuthority(int authoritive = -1, int paused = -1) {}
	virtual int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags = 0);
	virtual int SetStateFromTypedSnapshot(TSerialize ser, int type, int flags = 0);
	virtual int GetStateSnapshotTxt(char* txtbuf, int szbuf, float time_back = 0) { return 0; }
	virtual void SetStateFromSnapshotTxt(const char* txtbuf, int szbuf) {}
	virtual int DoStep(float time_interval, int iCaller) { return 0; }
	virtual void StartStep(float time_interval) {}
	virtual void StepBack(float time_interval) {}
	virtual IPhysicalWorld* GetWorld() const;
	virtual void GetMemoryStatistics(ICrySizer* pSizer) const {}

	virtual QuatT getGlobalPose() const { return cpx::Helper::T(m_actor->getGlobalPose()); }
	virtual void setGlobalPose(const QuatT& pose, int components=3) { m_actor->setGlobalPose(cpx::Helper::T(pose)); }
	virtual void getBBox(Vec3 *BBox) const { if (m_actor) { PxBounds3 bbox = m_actor->getWorldBounds(); BBox[0]=cpx::Helper::V(bbox.minimum); BBox[1]=cpx::Helper::V(bbox.maximum); } }
	virtual QuatT getLocalPose(int idxPart) const;
	virtual void setLocalPose(int idxPart, const QuatT& pose);
	virtual PxRigidBody *getRigidBody(int ipart) const { return m_actor && m_actor->getScene() ? m_actor->isRigidBody() : nullptr;	}
	int idxPart(int id) const { int i; for(i=m_parts.size()-1;i>=0 && cpx::Helper::PartId(m_parts[i].shape)!=id;i--); return i; }
	virtual void Enable(bool enable=true);
	virtual void Awake(bool awake=true, float minTime=0);
	virtual bool IsAwake() const { return m_actor->isRigidDynamic() ? !m_actor->isRigidDynamic()->isSleeping() : false; }
	virtual void SetSimClass(int iSimClass);
	virtual void PostStep(float dt, int immediate=1);
	virtual int RefineRayHit(const Vec3& rayOrg, const Vec3& rayDir, Vec3 &pt, Vec3 &n, int partid) const { return partid; }
	void MarkForAdd();
	PhysXEnt* AddToScene(bool fromList=false);

	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int iDrawHelpers);

	pe_type m_type;
	volatile mutable int m_nRefCount = 0;
	int m_id : 24;
	int m_iSimClass : 8;
	PhysXEnt *m_list[2] = { this,this };

	void* m_pForeignData = 0;
	int m_iForeignData  : 16;
	int m_iForeignFlags : 16;

	struct part {
		_smart_ptr<phys_geometry_refcnt> geom;
		PxShape *shape;
		Diag33 scale;
		float density;
	};
	std::vector<part> m_parts;
	uint m_flags = 0;
	PxRigidActor *m_actor = nullptr;
	// reserved bits: 0 - awake request; 1 - sleep request; 2 - added to scene; 3 - deletion request; 4 - deleted; 5 - hide requested; 31 - queued add to scene
	volatile uint m_mask = 0;	
	std::set<PxD6Joint*,comp_userDataPtr<PxD6Joint>> m_constraints;
	static class PhysXWorld *g_pPhysWorld;
};


class PhysXLiving : public PhysXEnt
{
public:
	PhysXLiving(int id, pe_params *params) : PhysXEnt(PE_LIVING, id, params) {}
	int SetParams(pe_params* params, int bThreadSafe = 1) override;
	int GetStatus(pe_status* status) const override;
	int Action(pe_action*, int bThreadSafe = 1) override;
	void setGlobalPose(const QuatT& pose, int components) override { 
		if (components & 2) m_qRot = pose.q;
		if (components & 1) m_actor->setGlobalPose(cpx::Helper::T(QuatT(m_qRot,pose.t)));
	}
	QuatT getGlobalPose() const override { return QuatT(m_qRot,cpx::Helper::V(m_actor->getGlobalPose().p)); }
	void PostStep(float dt, int immediate=1) override;
	Quat m_qRot = Quat(IDENTITY);
};


class PhysXProjectile : public PhysXEnt
{
public:
	PhysXProjectile(int id,pe_params *params);
	int SetParams(pe_params* params, int bThreadSafe = 1) override;
	int GetParams(pe_params* params) const override;
	int GetStatus(pe_status* status) const override;
	int Action(pe_action*, int bThreadSafe = 1) override;
	int DoStep(float time_interval, int iCaller) override;

	QuatT getGlobalPose() const override;
	void Enable(bool) override;
	void getBBox(Vec3 *BBox) const override;
	
	Vec3 m_pos = Vec3(ZERO);
	Vec3 m_vel = Vec3(ZERO);
	Vec3 m_gravity = Vec3(ZERO);
	float m_mass = 0.1f;
	float m_size = 0.05f;
	int m_pierceability = sf_max_pierceable;
	int m_surfaceIdx = 0;
	float m_minBounceVel = 1.5f;
	_smart_ptr<PhysXEnt> m_pIgnoredEnt;
	volatile int m_stepped = 0;
};


class PhysXArticulation : public PhysXEnt
{
public:
	PhysXArticulation(int id,pe_params*);
	virtual ~PhysXArticulation() { if (m_hostAttach) m_hostAttach->release(); m_artic->release(); }
	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id=-1, int bThreadSafe=1) override;
	int SetParams(pe_params* params, int bThreadSafe = 1) override;
	int GetStatus(pe_status* status) const override;
	int Action(pe_action*, int bThreadSafe = 1) override;

	template<typename Func> void IterateJoints(Func func) const {
		int n = m_artic->getNbLinks();
		PxArticulationLink **links = (PxArticulationLink**)alloca(n*sizeof(void*));
		m_artic->getLinks(links,n);
		for(int i=0; i<n; i++)
			func(links[i]);
	}
	PxArticulationLink *findJoint(int id) const;
	QuatT getGlobalPose() const override;
	void setGlobalPose(const QuatT& pose, int components=3) override;
	void getBBox(Vec3 *BBox) const override;
	QuatT getLocalPose(const PxRigidBody *link, int idxPart) const;
	QuatT getLocalPose(int idxPart) const override;
	PxRigidBody* getRigidBody(int ipart) const override;
	void Enable(bool enable=true) override;
	void Awake(bool awake=true, float minTime=0) override;
	bool IsAwake() const override;
	void SetSimClass(int iSimClass) override;
	bool IsRagdoll() const { return (uint)m_iSimClass-1u < 2u; }

	PxArticulation *m_artic = nullptr;
	PxArticulationLink *m_link0 = nullptr;
	Vec3 m_offsPivot = Vec3(ZERO);
	Quat m_rotExt = Quat(IDENTITY);
	PxFixedJoint *m_hostAttach = nullptr;
};


class PhysXRope : public PhysXEnt
{
public:
	PhysXRope(int id,pe_params* params);
	virtual ~PhysXRope() { if (m_joint) m_joint->release(); }
	int SetParams(pe_params* params, int bThreadSafe = 1) override;
	int GetParams(pe_params* params) const override;
	int GetStatus(pe_status* status) const override;
	int Action(pe_action*, int bThreadSafe = 1) override;

	QuatT getGlobalPose() const override { return QuatT(m_pos,Quat(IDENTITY)); }
	void setGlobalPose(const QuatT& pose, int components=3) override;
	void getBBox(Vec3 *BBox) const override;
	void Awake(bool awake=true, float minTime=0) override {}
	bool IsAwake() const override;
	void Enable(bool) override {}
	bool UpdateEnds();

	Vec3 m_pos = Vec3(0);
	PxDistanceJoint *m_joint = nullptr;
	std::vector<Vec3> m_points;
	float m_length = 0;
	mutable int m_idStepLastAwake = 0;
};


class PhysXVehicle : public PhysXEnt
{
public:
	PhysXVehicle(int id,pe_params* params) : PhysXEnt(PE_WHEELEDVEHICLE,id,params) {}
	virtual ~PhysXVehicle() { 
		if (m_rayCaster) m_rayCaster->release();
		if (m_vehicle) m_vehicle->release();
		if (m_fric) m_fric->release();
	}
	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id=-1, int bThreadSafe=1) override;
	int SetParams(pe_params* params, int bThreadSafe = 1) override;
	int GetStatus(pe_status* status) const override;
	int Action(pe_action*, int bThreadSafe = 1) override;

	bool SetupPxVehicle();
	void PostStep(float dt, int immediate=1) override;

	struct Wheel {
		int idx;
		int partid;
		bool driving;
		bool canBrake;
		bool canSteer;
		float suspLenRest,suspLenMax;
		Vec3 pivot;
		Vec3 center;
		float mass;
		float r,width;
		float kd;
		float friction;
	};
	std::vector<Wheel> m_wheels;
	float m_enginePower = 10000;
	float m_engineMaxRPM = 6000;
	float m_maxSteer = gf_PI/4;
	std::vector<float> m_gears;

	std::vector<PxRaycastQueryResult> m_rayHits;
	PxBatchQuery *m_rayCaster = nullptr;
	std::vector<PxWheelQueryResult> m_wheelsQuery;
	PxVehicleDrivableSurfaceToTireFrictionPairs *m_fric = nullptr;
	PxVehicleDrive4W *m_vehicle = nullptr;
};


class PhysXCloth : public PhysXEnt
{
public:
	PhysXCloth(int id, pe_params* params);
	virtual ~PhysXCloth() { if (m_cloth) m_cloth->release(); }
	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id=-1, int bThreadSafe=1) override;
	int SetParams(pe_params* params, int bThreadSafe = 1) override;
	int Action(pe_action*, int bThreadSafe = 1) override;
	int GetStatus(pe_status* status) const override;

	void getBBox(Vec3 *BBox) const override;
	QuatT getGlobalPose() const override { return cpx::Helper::T(m_cloth ? m_cloth->getGlobalPose() : m_actor->getGlobalPose()); }
	void setGlobalPose(const QuatT& pose, int components=3) override;
	void Enable(bool enable=true) override;
	void Awake(bool awake=true, float minTime=0) override { AwakeEnt(m_cloth,awake); }
	bool IsAwake() const override { return m_cloth ? !m_cloth->isSleeping() : false; };
	void PostStep(float dt, int immediate=1) override;
	int RefineRayHit(const Vec3& rayOrg, const Vec3& rayDir, Vec3 &pt, Vec3 &n, int partid) const override;
	void ApplyParams();
	void Attach(int *piVtx,int nVtx);
	void DeferredAttachAndCreate();

	PxCloth *m_cloth = nullptr;
	float m_mass = 0;
	Vec3 m_normal = Vec3(0,0,1);
	mutable std::vector<Vec3> m_vtx;
	PhysXEnt *m_host = nullptr;
	QuatT m_poseInHost = QuatT(IDENTITY);
	QuatT m_lastHostPose = QuatT(IDENTITY);
	int m_idpartHost;
	struct SPtAttach {
		int idx;
		PhysXEnt *ent;
		Vec3 ptrel;
	};
	std::vector<SPtAttach> m_attach;
	volatile int m_lockAttach = 0;
	std::vector<PxClothParticleMotionConstraint>	m_constr;
	std::vector<int> m_ivtxAttachAccum;
	float m_friction = 0.2f;
	float m_sleepSpeed = 0.04f;
	float m_maxTimeStep = 0.02f;
	float m_thickness = 0.01f;
	float m_damping = 0.0f;
	float m_hostSpaceSim = 0.0f;
	int m_collTypes = ent_rigid | ent_static;
	Vec3 m_wind[3] = { Vec3(0),Vec3(0),Vec3(0) };
	float m_windVariance = 0;
	float m_windTimer = 0;
};


namespace cpx { 
	namespace Helper {
		inline int JointId(const physx::PxShape *shape) { return (int)((INT_PTR)shape->userData>>16); }
		inline int JointId(const physx::PxArticulationLink *link) {
			PxShape *shape;
			int n = link->getShapes(&shape,1);
			return n ? JointId(shape) : -1; 
		}
	}
}

template<class Tparams> int ExtractTransform(const Tparams* params, QuatT& trans, Diag33& scale)
{
	int changed = 0; 
	if (!is_unused(params->pos)) { trans.t = params->pos; changed |= 1; }
	if (!is_unused(params->q)) { trans.q = params->q; changed |= 2; }
	if (!is_unused(params->scale)) { scale = Diag33(params->scale); changed |= 4; }
	Matrix33 *pMtx33=params->pMtx3x3, mtx33;
	if (params->pMtx3x4) {
		trans.t = params->pMtx3x4->GetTranslation(); changed |= 1;
		pMtx33 = &(mtx33 = Matrix33(*params->pMtx3x4));
	} 
	if (pMtx33) {
		scale = Diag33(pMtx33->GetColumn(0).len(),pMtx33->GetColumn(1).len(),pMtx33->GetColumn(2).len());
		trans.q = Quat(*pMtx33 * Diag33(scale).invert());	changed |= 6;
	}
	return changed;
}

extern volatile int g_lockEntList;
inline void RemoveFromList(PhysXEnt *pent) 
{
	pent->m_list[0]->m_list[1] = pent->m_list[1];
	pent->m_list[1]->m_list[0] = pent->m_list[0];
	pent->m_list[0] = pent->m_list[1] = pent;
}

inline void Insert(PhysXEnt* pent, PhysXEnt* pentRef, int dir, bool allowDeleted=false)
{
	WriteLock lock(g_lockEntList);
	if (pent!=pentRef && (allowDeleted || !(pent->m_mask & 0x18))) {
		RemoveFromList(pent);
		pent->m_list[dir] = pentRef->m_list[dir];	pent->m_list[dir^1] = pentRef;
		pentRef->m_list[dir]->m_list[dir^1] = pent;	pentRef->m_list[dir] = pent;
	}
}

inline void InsertBefore(PhysXEnt *pent, PhysXEnt *pentBeforeWhich, bool allowDeleted=false) { Insert(pent,pentBeforeWhich,0,allowDeleted); }
inline void InsertAfter(PhysXEnt *pent, PhysXEnt *pentAfterWhich, bool allowDeleted=false) { Insert(pent,pentAfterWhich,1,allowDeleted); }
template<class T> inline T* ListStart(T **pLinks) { return (T*)((INT_PTR)pLinks-(INT_PTR)((T*)0)->m_list); }

namespace cpx { 
	namespace Helper {
		inline PhysXEnt* Ent(const physx::PxActor *actor) { return (PhysXEnt*)(actor->userData); }
	}
}

#endif
