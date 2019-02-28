// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef physicalworld_h
#define physicalworld_h
#pragma once

#include "../../CryPhysics/geoman.h"
#include <CryNetwork/ISerialize.h>


class PhysXEnt;
class PhysXProjectile;

const int NSURFACETYPES = 512;

class PhysXWorld :public IPhysicalWorld, public IPhysUtils, public CGeomManager, public PxSimulationEventCallback
{

public:

	PhysXWorld(ILog* pLog);
	virtual ~PhysXWorld() {}

	virtual int CoverPolygonWithCircles(strided_pointer<Vec2> pt,int npt,bool bConsecutive, const Vec2 &center, Vec2 *&centers,float *&radii, float minCircleRadius)
	{ return ::CoverPolygonWithCircles(pt,npt,bConsecutive, center, centers,radii, minCircleRadius); }
	virtual void DeletePointer(void *pdata) { if (pdata) delete[] (char*)pdata; }
	virtual int qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris, qhullmalloc qmalloc = 0) { return ::qhull(pts,npts,pTris, qmalloc); }
	virtual int qhull2d(ptitem2d *pts, int nVtx, edgeitem *edges, int nMaxEdges = 0) { return ::qhull2d(pts, nVtx, edges, nMaxEdges); }
	virtual int TriangulatePoly(Vec2 *pVtx, int nVtx, int *pTris,int szTriBuf) { return ::TriangulatePoly(pVtx, nVtx, pTris, szTriBuf); }

	virtual void          Init() {}
	virtual void          Shutdown(int bDeleteGeometries = 1) {}
	virtual void          Release();

	virtual IGeomManager* GetGeomManager() { return this; }
	virtual IPhysUtils*   GetPhysUtils() { return this; }

	virtual IPhysicalEntity* SetupEntityGrid(int axisz, Vec3 org, int nx,int ny, float stepx,float stepy, int log2PODscale=0, int bCyclic=0, 
		IPhysicalEntity* pHost=nullptr, const QuatT& posInHost=QuatT(IDENTITY));
	virtual void Cleanup() {}
	virtual void RegisterBBoxInPODGrid(const Vec3* BBox) {}
	virtual void UnregisterBBoxInPODGrid(const Vec3* BBox) {}
	virtual void DeactivateOnDemandGrid() {}
	virtual int AddRefEntInPODGrid(IPhysicalEntity* pent, const Vec3* BBox = 0) { return 0; }

	virtual IPhysicalEntity* SetHeightfieldData(const primitives::heightfield* phf, int* pMatMapping = 0, int nMats = 0);
	virtual IPhysicalEntity* GetHeightfieldData(primitives::heightfield* phf) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }
	virtual void             SetHeightfieldMatMapping(int* pMatMapping, int nMats) { CRY_PHYSX_LOG_FUNCTION; }
	virtual PhysicsVars*     GetPhysVars() { return &m_vars; }

	virtual IPhysicalEntity* CreatePhysicalEntity(pe_type type, pe_params* params = 0, void* pforeigndata = 0, int iforeigndata = 0, int id = -1, IGeneralMemoryHeap* pHeap = NULL)
	{
		return CreatePhysicalEntity(type, 0.0f, params, pforeigndata, iforeigndata, id, NULL, pHeap);
	}
	virtual IPhysicalEntity* CreatePhysicalEntity(pe_type type, float lifeTime, pe_params* params = 0, void* pForeignData = 0, int iForeignData = 0, int id = -1, IPhysicalEntity* pHostPlaceholder = 0, IGeneralMemoryHeap* pHeap = NULL);
	virtual IPhysicalEntity* CreatePhysicalPlaceholder(pe_type type, pe_params* params = 0, void* pForeignData = 0, int iForeignData = 0, int id = -1);
	virtual int              DestroyPhysicalEntity(IPhysicalEntity* pent, int mode = 0, int bThreadSafe = 0);

	virtual int              SetPhysicalEntityId(IPhysicalEntity* pent, int id, int bReplace = 1, int bThreadSafe = 0) { return 0; }
	virtual int              GetPhysicalEntityId(IPhysicalEntity* pent);
	virtual IPhysicalEntity* GetPhysicalEntityById(int id);
	int                      GetFreeEntId();

	virtual int SetSurfaceParameters(int surface_idx, float bounciness, float friction, unsigned int flags = 0);
	virtual int GetSurfaceParameters(int surface_idx, float& bounciness, float& friction, unsigned int& flags);
	virtual int SetSurfaceParameters(int surface_idx, float bounciness, float friction, float damage_reduction, float ric_angle, float ric_dam_reduction, float ric_vel_reduction, unsigned int flags = 0) 
	{ return SetSurfaceParameters(surface_idx,bounciness,friction,flags); }
	virtual int GetSurfaceParameters(int surface_idx, float& bounciness, float& friction, float& damage_reduction, float& ric_angle, float& ric_dam_reduction, float& ric_vel_reduction, unsigned int& flags) 
	{ return GetSurfaceParameters(surface_idx,bounciness,friction,flags); }

	virtual void  TimeStep(float time_interval, int flags = ent_all | ent_deleted);

	virtual float GetPhysicsTime() { return m_time; }
	virtual int   GetiPhysicsTime() { return float2int(m_time/m_vars.timeGranularity); }
	virtual void  SetPhysicsTime(float time) { m_time=time; }
	virtual void  SetiPhysicsTime(int itime) { CRY_PHYSX_LOG_FUNCTION; }

	virtual void  SetSnapshotTime(float time_snapshot, int iType = 0) { CRY_PHYSX_LOG_FUNCTION; }
	virtual void  SetiSnapshotTime(int itime_snapshot, int iType = 0) { CRY_PHYSX_LOG_FUNCTION; }

	virtual int GetEntitiesInBox(Vec3 ptmin, Vec3 ptmax, IPhysicalEntity**& pList, int objtypes, int szListPrealloc = 0);

	virtual int RayWorldIntersection(const SRWIParams& rp, const char* pNameTag = RWI_NAME_TAG, int iCaller = MAX_PHYS_THREADS);
	virtual int  TracePendingRays(int bDoActualTracing = 1);

	virtual void ResetDynamicEntities();
	virtual void               DestroyDynamicEntities() { CRY_PHYSX_LOG_FUNCTION; }

	virtual void               PurgeDeletedEntities() { CRY_PHYSX_LOG_FUNCTION; } //!< Forces immediate physical deletion of all entities marked as deleted
	virtual int                GetEntityCount(int iEntType) { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; } //!< iEntType is of pe_type
	virtual int                ReserveEntityCount(int nExtraEnts) { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; } //!< can prevent excessive internal lists re-allocations

	virtual IPhysicalEntityIt* GetEntitiesIterator() { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }

	virtual void               SimulateExplosion(pe_explosion* pexpl, IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0, int iTypes = ent_rigid | ent_sleeping_rigid | ent_living | ent_independent, int iCaller = MAX_PHYS_THREADS);

	virtual void RasterizeEntities(const primitives::grid3d& grid, uchar* rbuf, int objtypes, float massThreshold, const Vec3& offsBBox, const Vec3& sizeBBox, int flags) { CRY_PHYSX_LOG_FUNCTION; }

	virtual int   DeformPhysicalEntity(IPhysicalEntity* pent, const Vec3& ptHit, const Vec3& dirHit, float r, int flags = 0) { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; }
	virtual void  UpdateDeformingEntities(float time_interval = 0.01f) { CRY_PHYSX_LOG_FUNCTION; } //!< normally this happens during TimeStep
	virtual float CalculateExplosionExposure(pe_explosion* pexpl, IPhysicalEntity* pient) { CRY_PHYSX_LOG_FUNCTION; _RETURN_FLOAT_DUMMY_; }

	virtual float IsAffectedByExplosion(IPhysicalEntity* pent, Vec3* impulse = 0) { CRY_PHYSX_LOG_FUNCTION; _RETURN_FLOAT_DUMMY_; }

	virtual int  AddExplosionShape(IGeometry* pGeom, float size, int idmat, float probability = 1.0f) { return 0; }
	virtual void RemoveExplosionShape(int id) { CRY_PHYSX_LOG_FUNCTION; }
	virtual void RemoveAllExplosionShapes(void(*OnRemoveGeom)(IGeometry* pGeom) = 0) { CRY_PHYSX_LOG_FUNCTION; }

	virtual void DrawPhysicsHelperInformation(IPhysRenderer* pRenderer, int iCaller = MAX_PHYS_THREADS);
	virtual void DrawEntityHelperInformation(IPhysRenderer* pRenderer, int iEntityId, int iDrawHelpers);

	virtual int   CollideEntityWithBeam(IPhysicalEntity* _pent, Vec3 org, Vec3 dir, float r, ray_hit* phit) { 
		sphere sph; sph.center=org; sph.r=r;
		return CollideEntityWithPrimitive(_pent, sphere::type,&sph, dir, phit);
	}
	virtual int   CollideEntityWithPrimitive(IPhysicalEntity* _pent, int itype, primitives::primitive* pprim, Vec3 dir, ray_hit* phit, intersection_params* pip = 0);
	virtual int   RayTraceEntity(IPhysicalEntity* pient, Vec3 origin, Vec3 dir, ray_hit* pHit, pe_params_pos* pp = 0, unsigned int geomFlagsAny = geom_colltype0 | geom_colltype_player);

	virtual float PrimitiveWorldIntersection(const SPWIParams& pp, WriteLockCond* pLockContacts = 0, const char* pNameTag = PWI_NAME_TAG);
	virtual void  GetMemoryStatistics(ICrySizer* pSizer) { CRY_PHYSX_LOG_FUNCTION; }

	virtual void  SetPhysicsStreamer(IPhysicsStreamer* pStreamer) { CRY_PHYSX_LOG_FUNCTION; }
	virtual void  SetPhysicsEventClient(IPhysicsEventClient* pEventClient) { CRY_PHYSX_LOG_FUNCTION; }
	virtual float GetLastEntityUpdateTime(IPhysicalEntity* pent) { CRY_PHYSX_LOG_FUNCTION; _RETURN_FLOAT_DUMMY_; } //!< simulation class-based, not actually per-entity
	virtual int   GetEntityProfileInfo(phys_profile_info*& pList) { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; }
	virtual int   GetFuncProfileInfo(phys_profile_info*& pList) { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; }
	virtual int   GetGroupProfileInfo(phys_profile_info*& pList) { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; }
	virtual int   GetJobProfileInfo(phys_job_info*& pList) { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; }

	virtual void             AddEventClient(int type, int(*func)(const EventPhys*), int bLogged, float priority = 1.0f);
	virtual int              RemoveEventClient(int type, int(*func)(const EventPhys*), int bLogged);
	virtual int              NotifyEventClients(EventPhys* pEvent, int bLogged) { SendEvent(*pEvent,bLogged); return 1; }
	virtual void             PumpLoggedEvents(); 
	virtual uint32           GetPumpLoggedEventsTicks() { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; }
	virtual void             ClearLoggedEvents();

	virtual IPhysicalEntity* AddGlobalArea();
	virtual IPhysicalEntity* AddArea(Vec3* pt, int npt, float zmin, float zmax, const Vec3& pos = Vec3(0, 0, 0), const quaternionf& q = quaternionf(IDENTITY), float scale = 1.0f, const Vec3& normal = Vec3(ZERO), int* pTessIdx = 0, int nTessTris = 0, Vec3* pFlows = 0) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }
	virtual IPhysicalEntity* AddArea(IGeometry* pGeom, const Vec3& pos, const quaternionf& q, float scale);
	virtual IPhysicalEntity* AddArea(Vec3* pt, int npt, float r, const Vec3& pos = Vec3(0, 0, 0), const quaternionf& q = quaternionf(IDENTITY), float scale = 1) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }
	virtual IPhysicalEntity* GetNextArea(IPhysicalEntity* pPrevArea = 0);
	virtual int              CheckAreas(const Vec3& ptc, Vec3& gravity, pe_params_buoyancy* pb, int nMaxBuoys = 1, int iMedium = -1, const Vec3& vec = Vec3(ZERO), IPhysicalEntity* pent = 0, int iCaller = MAX_PHYS_THREADS);

	virtual void             SetWaterMat(int imat) {} 
	virtual int              GetWaterMat() { return 0; } 
	virtual int              SetWaterManagerParams(pe_params* params) { return 0; }
	virtual int              GetWaterManagerParams(pe_params* params) { return 0; }
	virtual int              GetWatermanStatus(pe_status* status) { return 0; }
	virtual void             DestroyWaterManager() {}

	virtual volatile int*    GetInternalLock(int idx) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; } //!< returns one of phys_lock locks

	virtual int              SerializeWorld(const char* fname, int bSave) { return 0; }
	virtual int              SerializeGeometries(const char* fname, int bSave) { return 0; }

	virtual void             SerializeGarbageTypedSnapshot(TSerialize ser, int iSnapshotType, int flags) { CRY_PHYSX_LOG_FUNCTION; }

	virtual void             SavePhysicalEntityPtr(TSerialize ser, IPhysicalEntity* pent) { CRY_PHYSX_LOG_FUNCTION; }
	virtual IPhysicalEntity* LoadPhysicalEntityPtr(TSerialize ser) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }
	virtual void             GetEntityMassAndCom(IPhysicalEntity* pIEnt, float& mass, Vec3& com) { CRY_PHYSX_LOG_FUNCTION; }

	virtual EventPhys*       AddDeferredEvent(int type, EventPhys* event);

	virtual void*			 GetInternalImplementation(int type, void* object = nullptr); // type: EPE_InternalImplementation

	// private:

	PhysicsVars m_vars;
	ILog* m_pLog;
	IPhysRenderer *m_pRenderer;

	/////////////////////////// PhysX //////////////////////////////////
	PhysXEnt *m_phf = nullptr;
	PhysXEnt *m_pentStatic = nullptr;
	std::vector<PhysXEnt*> m_entsById;
	int m_idFree = -1;
	uint m_idWorldRgn = 0;
	PxMaterial *m_mats[NSURFACETYPES];
	volatile int m_lockMask=0, m_lockCollEvents=0, m_lockEntDelete=0, m_lockEntList=0;
	volatile uint m_maskUsed = 0x1f; // lower mask bits are reserved
	std::vector<PhysXEnt*> m_entList;
	PhysXEnt *m_activeEntList[2], *m_prevActiveEntList[2];
	PhysXEnt *m_addEntList[2], *m_delEntList[2];
	PhysXProjectile *m_projectilesList[2];
	PhysXEnt *m_auxEntList[2];
	pe_params_buoyancy m_pbGlob;
	Vec3 m_wind = Vec3(0);
	float m_dt=0, m_dtSurplus=0;
	int m_idStep = 0;
	volatile int m_updated=0;
	double m_time = 0;
	std::vector<char> m_scratchBuf;

	PxBatchQuery *m_batchQuery[2];
	int m_nqRWItouches=0;
	std::vector<PxRaycastQueryResult> m_rwiResPx;
	std::vector<PxRaycastHit> m_rwiTouchesPx;
	std::vector<EventPhysRWIResult> m_rwiRes[3];
	std::vector<ray_hit> m_rwiHits;
	volatile int m_lockRWIqueue=0, m_lockRWIres=0;

	struct SEventClient {
		int (*OnEvent)(const EventPhys*);
		float priority;
		bool operator<(const SEventClient& op) const { return priority > op.priority; }
	};
	std::multiset<SEventClient> m_eventClients[EVENT_TYPES_NUM][2];
	int m_nCollEvents = 0;
	std::vector<EventPhysCollision> m_collEvents[2];

	void SendEvent(EventPhys &evt, int bLogged) { 
		auto &list = m_eventClients[evt.idval][bLogged];
		std::find_if(list.begin(),list.end(), [&](auto client)->bool { return !client.OnEvent(&evt); });
	}
	EventPhys *AllocEvent(int id);

	virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) {}
	virtual void onWake(PxActor** actors, PxU32 count);
	virtual void onSleep(PxActor** actors, PxU32 count);
	virtual void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs);
	virtual void onTrigger(PxTriggerPair* pairs, PxU32 count) {}
	virtual void onAdvance(const PxRigidBody*const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) {}

	PxMaterial *GetSurfaceType(int i) { return m_mats[ (uint)i<(uint)NSURFACETYPES && m_mats[i] ? i : 0 ]; }
	void UpdateProjectileState(PhysXProjectile *pent);

	private:

	bool m_debugDraw;
	IPhysRenderer *m_renderer;
	std::vector<Vec3> m_debugDrawTriangles;
	std::vector<ColorB> m_debugDrawTrianglesColors;
	std::vector<Vec3> m_debugDrawLines;
	std::vector<ColorB> m_debugDrawLinesColors;

};

namespace cpx { 
	namespace Helper {
		inline int MatId(const physx::PxMaterial *mtl) { return mtl ? ((int)(INT_PTR)mtl->userData & 0xffff) : 0; }
	}
}

#endif
