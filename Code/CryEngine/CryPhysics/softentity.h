// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef softentity_h
#define softentity_h
#pragma once

enum sentity_flags_int { 
	sef_volumetric = 0x08
};

struct se_vertex_base {
	Vec3 pos,vel;
	Vec3 pos0,vel0;
	float massinv,mass;
	Vec3 n,ncontact;
	int idx,idx0;
	float area;
	int iStartEdge : 16;
	int iEndEdge : 16;
	int bAttached : 8;
	int bFullFan : 2;
	int iCheckPart : 6;
	int iContactPart : 16;
	float rnEdges;
	CPhysicalEntity *pContactEnt;
	int iContactNode;
	Vec3 vcontact;
	float vreq;
	two_ints_in_one surface_idx;
	float angle0;
	Vec3 nmesh;
};

struct se_vertex : se_vertex_base {
	~se_vertex() { if (pContactEnt) pContactEnt->Release(); }
	Vec3 ptAttach;
	Vec3 posorg;
	int iSorted;
	//Vec3 P,dv,r,d;
};

struct se_edge {
	int ivtx[2];
	float len0;
	float len,rlen;
	float kmass;
	float angle0[2];
};

struct check_part {
	Vec3 offset;
	Matrix33 R;
	float scale,rscale;
	box bbox;
	CPhysicalEntity *pent;
	int ipart;
	Vec3 vbody,wbody;
	CGeometry *pGeom;
	int bPrimitive;
	int surface_idx;
	Vec3 P,L;
	Vec3 posBody;
	plane contPlane[8];
	float contRadius[8];
	float contDepth[8];
	int nCont;
};


class CSoftEntity : public CPhysicalEntity {
 public:
	explicit CSoftEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap = NULL);
	virtual ~CSoftEntity();
  virtual pe_type GetType() const { return PE_SOFT; }

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);
	virtual int SetParams(pe_params *_params,int bThreadSafe=1);
	virtual int GetParams(pe_params *_params) const;
	virtual int Action(pe_action*,int bThreadSafe=1);
	virtual int GetStatus(pe_status*) const;

	virtual int Awake(int bAwake=1,int iSource=0);
	virtual int IsAwake(int ipart=-1) const { return m_bPermanent; }
	virtual void AlertNeighbourhoodND(int mode);

	virtual void StartStep(float time_interval);
	virtual float GetMaxTimeStep(float time_interval);
	virtual int Step(float time_interval);
	virtual int RayTrace(SRayTraceRes&);
	virtual void ApplyVolumetricPressure(const Vec3 &epicenter, float kr, float rmin);
	virtual float GetMass(int ipart) { return m_parts[0].mass/m_nVtx; }
	void StepInner(float time_interval, int bCollMode, check_part *checkParts,int nCheckParts, 
		const plane &waterPlane,const Vec3 &waterFlow,float waterDensity, const Vec3 &lastposHost, const quaternionf &lastqHost, se_vertex *pvtx);

	void BakeCurrentPose();
	void AttachPoints(pe_action_attach_points *action, CPhysicalEntity *pent,int ipart, float rvtxmass,float vtxmass, int bAttached, const Vec3 &offs,const quaternionf &q);

	enum snapver { SNAPSHOT_VERSION = 10 };
	virtual int GetStateSnapshot(CStream &stm, float time_back=0,int flags=0);
	virtual int SetStateFromSnapshot(CStream &stm, int flags);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0);

	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);
	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	void RemoveCore();

	se_vertex *m_vtx;
	se_edge *m_edges;
	int *m_pVtxEdges;
	int m_nVtx,m_nEdges;
	int m_nConnectedVtx;
	int m_nAttachedVtx;
	Vec3 m_offs0;
	quaternionf m_qrot0;
	int m_bMeshUpdated;
	Vec3 m_lastposHost;
	quaternionf m_lastqHost;
	int *m_pTetrEdges;
	int *m_pTetrQueue;
	Vec3 m_lastPos;

	float m_timeStepFull;
	float m_timeStepPerformed;
	float m_timeStepSurplus;

	Vec3 m_gravity;
	float m_Emin;
	float m_maxAllowedStep;
	int m_nSlowFrames;
	float m_damping;
	float m_accuracy;
	int m_nMaxIters;
	float m_prevTimeInterval;
	int m_bSkinReady;
	float m_maxMove;
	float m_maxAllowedDist;

	float m_thickness;
	float m_ks;
	float m_maxSafeStep;
	float m_density;
	float m_coverage;
	float m_friction;
	float m_impulseScale;
	float m_explosionScale;
	float m_collImpulseScale;
	float m_maxCollImpulse;
	int m_collTypes;
	float m_massDecay;
	float m_kShapeStiffnessNorm,m_kShapeStiffnessTang;
	float m_vtxvol;
	float m_stiffnessAnim,m_stiffnessDecayAnim,m_dampingAnim,m_maxDistAnim;
	float m_kRigid;
	float m_maxLevelDenom;

	float m_waterResistance;
	float m_airResistance;
	Vec3 m_wind;
	Vec3 m_wind0,m_wind1;
	float m_windTimer;
	float m_windVariance;

	class CRigidEntity *m_pCore;
	Vec3 m_pos0core;
	int *m_corevtx,m_nCoreVtx;

	int m_iLastLog;
	EventPhysPostStep *m_pEvent;

	mutable volatile int m_lockSoftBody;
};

#endif
