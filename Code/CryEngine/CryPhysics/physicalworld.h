// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef physicalworld_h
#define physicalworld_h
#pragma once

#include "rigidbody.h"
#include "physicalentity.h"
#include "geoman.h"
#include <CryThreading/IThreadManager.h>

const int NSURFACETYPES = 512;
const int PLACEHOLDER_CHUNK_SZLG2 = 8;
const int PLACEHOLDER_CHUNK_SZ = 1<<PLACEHOLDER_CHUNK_SZLG2;
const int QUEUE_SLOT_SZ = 8192;

const int PENT_SETPOSED = 1<<16;
const int PENT_QUEUED_BIT = 17;
const int PENT_QUEUED = 1<<PENT_QUEUED_BIT;

class CPhysicalPlaceholder;
class CPhysicalEntity;
class CPhysArea;
struct pe_gridthunk;
struct le_precomp_entity;
struct le_precomp_part;
struct le_tmp_contact;
enum { pef_step_requested = 0x40000000 };

template <class T> T *CONTACT_END(T *const &pnext) { return (T*)((INT_PTR)&pnext-(INT_PTR)&((T*)0)->next); }

#if defined(ENTITY_PROFILER_ENABLED)
#define PHYS_ENTITY_PROFILER CPhysEntityProfiler ent_profiler(this);
#define PHYS_AREA_PROFILER(pArea) CPhysAreaProfiler ent_profiler(pArea);
#else
#define PHYS_ENTITY_PROFILER
#define PHYS_AREA_PROFILER(pArea)
#endif

#if defined(_RELEASE)
# define PHYS_FUNC_PROFILER_DISABLED 1
#endif

#ifndef PHYS_FUNC_PROFILER_DISABLED
# define PHYS_FUNC_PROFILER(name) CPhysFuncProfiler func_profiler((name),this);
# define BLOCK_PROFILER(count) CBlockProfiler block_profiler(count);
#else
# define PHYS_FUNC_PROFILER(name)
# define BLOCK_PROFILER(count)
#endif

struct EventClient {
	int (*OnEvent)(const EventPhys*);
	EventClient *next;
	float priority;
#ifndef PHYS_FUNC_PROFILER_DISABLED
	char tag[32];
	int64 ticks;
#endif
};

const int EVENT_CHUNK_SZ = 8192-sizeof(void*);
struct EventChunk {
	EventChunk *next;
};

struct SExplosionShape {
	int id;
	IGeometry *pGeom;
	float size,rsize;
	int idmat;
	float probability;
	int iFirstByMat,nSameMat;
	int bCreateConstraint;
};

struct SRwiRequest : IPhysicalWorld::SRWIParams {
	int iCaller;
	int idSkipEnts[5];
};

#define max_def(a,b) (a) + ((b)-(a) & (a)-(b)>>31)
struct SPwiRequest : IPhysicalWorld::SPWIParams {
	char primbuf[max_def(max_def(max_def(max_def(sizeof(box),sizeof(cylinder)),sizeof(capsule)),sizeof(sphere)),sizeof(triangle))];
	int idSkipEnts[4];
};

struct SBreakRequest {
	pe_explosion expl;
	geom_world_data gwd[2];
	CPhysicalEntity *pent;
	int partid;
	Vec3 gravity;
};

struct pe_PODcell {
	int inextActive;
	float lifeTime;
	int nObjects;
	Vec2 zlim;
};

struct CRY_ALIGN(128) SPrecompPartBV {
	sphere partsph;
	box partbox;
	quaternionf qpart;
	Vec3 pospart;
	float scale;
	CPhysicalEntity* pent; geom *ppart;
	int ient; int ipart; int iparttype;
};

struct SThreadData {
	CPhysicalEntity **pTmpEntList;
	int szList;

	SPrecompPartBV *pTmpPartBVList;
	int szTmpPartBVList, nTmpPartBVs;
	CPhysicalEntity* pTmpPartBVListOwner;

	le_precomp_entity *pTmpPrecompEntsLE;
	le_precomp_part *pTmpPrecompPartsLE;
	le_tmp_contact *pTmpNoResponseContactLE;
	int nPrecompEntsAllocLE, nPrecompPartsAllocLE, nNoResponseAllocLE;

	int bGroupInvisible;
	float groupMass;
	float maxGroupFriction;
};

struct SThreadTaskRequest {
	float time_interval;
	int bSkipFlagged;
	int ipass;
	int iter;
	int *pbAllGroupsFinished;
};

#ifdef ENTGRID_2LEVEL
struct pe_entgrid {
	pe_entgrid &operator=(const pe_entgrid &src) {
		log2Stride=src.log2Stride;
		log2StrideXMask=src.log2StrideXMask;
		log2StrideYMask=src.log2StrideYMask;
		gridlod1=src.gridlod1;
		return *this;
	}
	pe_entgrid &operator=(int) { gridlod1=0; return *this; }
	operator bool() const { return gridlod1!=0; }

	int operator[](int i) {
		int iy0=i>>log2Stride, ix0=i & log2StrideXMask;
		int *grid = gridlod1[ ((i & ~log2StrideYMask) >> 6) | ix0>>3], dummy=0;
		INT_PTR mask = -iszero((INT_PTR)grid);
		return ((int*)((INT_PTR)grid+((INT_PTR)&dummy-(INT_PTR)grid & mask)))[(iy0 & 7)*8+(ix0 & 7) & ~(int)mask];
	}
	int &operator[](unsigned int i) {
		int iy0=i>>log2Stride, ix0=i & log2StrideXMask;
		int *&grid = gridlod1[((i & ~log2StrideYMask) >> 6) | ix0>>3];
		if (!grid)
			memset(grid=new int[64], 0, 64*sizeof(int));
		// cppcheck-suppress memleak
		return grid[(iy0 & 7)*8+(ix0 & 7)];
	}

	int log2Stride;
	int log2StrideXMask;
	int log2StrideYMask;
	int **gridlod1;
};

inline void AllocateGrid(pe_entgrid &grid, const Vec2i &size)
{
	memset(grid.gridlod1 = new int*[(size.x*size.y>>6)+1], 0, ((size.x*size.y>>6)+1)*sizeof(int*));
	int bitCount = bitcount(size.x-1);
	grid.log2Stride				= bitCount;
	grid.log2StrideXMask	= (1 << bitCount) - 1;
	grid.log2StrideYMask	= (1 << (bitCount + 3)) - 1;
}
inline void DeallocateGrid(pe_entgrid &grid, const Vec2i &size)
{
	for(int i=size.x*size.y>>6;i>=0;i--) if (grid.gridlod1[i])
		delete[] grid.gridlod1[i];
	delete[] grid.gridlod1; grid.gridlod1=0;
}
inline int GetGridSize(pe_entgrid &grid, const Vec2i& size)
{
	if (!grid)
		return 0;
	int i,sz=0;
	for(i=size.x*size.y>>6; i>=0; i--)
		sz += iszero((INT_PTR)grid.gridlod1[i])^1;
	return sz*64*sizeof(int)+((size.x*size.y>>6)+1)*sizeof(int*);
}
#else
#define pe_entgrid int*
inline void AllocateGrid(int *&grid, const Vec2i &size) { memset(grid = new int[size.x*size.y+1], 0, (size.x*size.y+1)*sizeof(int)); }
inline void DeallocateGrid(int *&grid, const Vec2i &size) { delete[] grid; }
inline int GetGridSize(int *&grid, const Vec2i& size) { return (size.x*size.y+1)*sizeof(int); }
#endif

const int TRIGGER_PORTAL = 1<<9;
const int TRIGGER_PORTAL_INV = 1<<10;
inline int IsPortal(const CPhysicalPlaceholder *pobj) { return iszero(pobj->m_iSimClass-6) & pobj->m_iForeignFlags>>9; }
inline int IsPortal(const pe_gridthunk &thunk) { return thunk.iSimClass==6 && thunk.pent->m_iForeignFlags & TRIGGER_PORTAL; }

inline Vec3* transformBBox(const Vec3& ptmin, const Vec3& ptmax, Vec3* dstBBox, const QuatT& trans) {
	Matrix33 R = Matrix33(trans.q);
	Vec3 c = trans*((ptmin+ptmax)*0.5f), sz = R.Fabs()*(ptmax-ptmin)*0.5f;
	dstBBox[0] = c-sz; dstBBox[1] = c+sz;
	return dstBBox;
}


struct SEntityGrid : public CPhysicalPlaceholder, grid {
	int iup;
	float zGran,rzGran;
	pe_entgrid cells;
	pe_PODcell **pPODcells,dummyPODcell,*pDummyPODcell;
	Vec2i PODstride;
	int log2PODscale;
	int bHasPODGrid,iActivePODCell0;

	SEntityGrid *m_next,*m_prev;
	QuatT m_transInHost,m_transW;
	Vec3 m_velW;
	Vec3 m_v, m_w, m_com;
	float m_dvSleep2;
	CPhysicalEntity *m_host;
	QuatT GetUpdatedTransW();
	Vec3 GetUpdatedVelW();

	class CPhysicalWorld *m_pWorld = nullptr;
	volatile mutable int m_refCount = 0;
	virtual int AddRef() { return CryInterlockedIncrement(&m_refCount); }
	virtual int Release();
	virtual int SetParams(pe_params*, int bThreadSafe=1);
	virtual int GetParams(pe_params*) const;

	virtual pe_type GetType() const { return PE_GRID; }
	void Init();
	void Free() { if (cells) DeallocateGrid(cells,size); cells=0; if (m_host) m_host->Release(); m_host=0; }
	void Setup(int axisz, Vec3 org, int nx,int ny, float stepx,float stepy, int log2PODscale, int bCyclic);
	void DeactivateOnDemand();
	void GetPODGridCellBBox(int ix,int iy, Vec3 &center,Vec3 &size) const;
	void RegisterBBoxInPODGrid(const Vec3 *BBox, IPhysicsStreamer *pStreamer);
	void UnregisterBBoxInPODGrid(const Vec3 *BBox);
	int AddRefEntInPODGrid(IPhysicalEntity *_pent, const Vec3 *BBox);

	pe_PODcell *getPODcell(int ix,int iy)	const {
		int i, imask = ~negmask(ix) & ~negmask(iy) & negmask(ix - size.x) & negmask(iy - size.y);   // (x>=0 && x<m_entgrid.size.x) ? 0xffffffff : 0;
		ix >>= log2PODscale; iy >>= log2PODscale;
		i = (ix>>3)*PODstride.x + (iy>>3)*PODstride.y;
		i = i + ((-1-i) & ~imask) & -bHasPODGrid;
		INT_PTR pmask = -iszero((INT_PTR)pPODcells[i]);
		pe_PODcell *pcell0 = (pe_PODcell*)(((INT_PTR)pPODcells[i] & ~pmask) + ((INT_PTR)pDummyPODcell & pmask));
		imask &= -bHasPODGrid & ~pmask;
		return pcell0 + ((ix&7)+((iy&7)<<3) & imask);
	}
	Vec3 vecToGrid(const Vec3& src) const { return src.GetPermutated(iup); }
	Vec3 vecFromGrid(const Vec3& src) const { return src.GetPermutated(iup^1^iup>>1); }
	void BBoxToGrid(const Vec3& ptmin, const Vec3& ptmax, Vec3* dst) const {	dst[0]=vecToGrid(ptmin-origin); dst[1]=vecToGrid(ptmax-origin); }
	void BBoxFromGrid(const Vec3& ptmin, const Vec3& ptmax, Vec3* dst) const {	dst[0]=vecFromGrid(ptmin)+origin; dst[1]=vecFromGrid(ptmax)+origin; }
};


struct SPhysTask : public IThread {
	SPhysTask(CPhysicalWorld *pWorld,int idx) { m_pWorld=pWorld; m_idx=idx; bStop=0; }
	virtual void ThreadEntry();
	void SignalStopWork();
	void Wait();
	
	class CPhysicalWorld *m_pWorld;
	int m_idx;
	volatile int bStop;
};

#ifndef MAIN_THREAD_NONWORKER
#define THREAD_TASK(a,b) \
	m_rq.ipass=a; \
	for(int ithread=0;ithread<m_nWorkerThreads;ithread++)	m_threadStart[ithread].Set();	\
	b; \
	for(int ithread=0;ithread<m_nWorkerThreads;ithread++)	m_threadDone[ithread].Wait();
#else
#define THREAD_TASK(a,b) \
	m_rq.ipass=a; \
	for(int ithread=0;ithread<m_nWorkerThreads;ithread++)	m_threadStart[ithread].Set();	\
	for(int ithread=0;ithread<m_nWorkerThreads;ithread++)	m_threadDone[ithread].Wait();
#endif

class CPhysicalWorld : public IPhysicalWorld, public IPhysUtils, public CGeomManager {

  class CPhysicalEntityIt : public IPhysicalEntityIt {
  public:
    CPhysicalEntityIt(CPhysicalWorld* pWorld);

	  virtual bool IsEnd();
	  virtual IPhysicalEntity* Next();
	  virtual IPhysicalEntity* This();
	  virtual void MoveFirst();

	  virtual void AddRef()	{	++m_refs;	}
		virtual void Release() { if (--m_refs<=0)	delete this; }
	private:
		CPhysicalWorld* m_pWorld;
		CPhysicalEntity *m_pCurEntity;
		int m_refs;
	};

public:
	CPhysicalWorld(ILog *pLog);
	~CPhysicalWorld();

	virtual void Init();
	virtual void Shutdown(int bDeleteEntities = 1);
	virtual void Release() { delete this; }
	virtual IGeomManager* GetGeomManager() { return this; }
	virtual IPhysUtils* GetPhysUtils() { return this; }

	virtual IPhysicalEntity* SetupEntityGrid(int axisz, Vec3 org, int nx,int ny, float stepx,float stepy, int log2PODscale=0, int bCyclic=0, 
		IPhysicalEntity* pHost=nullptr, const QuatT& posInHost=QuatT(IDENTITY));
	virtual void Cleanup();
	virtual void DeactivateOnDemandGrid();
	virtual void RegisterBBoxInPODGrid(const Vec3 *BBox);
	virtual void UnregisterBBoxInPODGrid(const Vec3 *BBox);
	virtual int AddRefEntInPODGrid(IPhysicalEntity *pent, const Vec3 *BBox=0);
	virtual IPhysicalEntity *SetHeightfieldData(const heightfield *phf,int *pMatMapping=0,int nMats=0);
	virtual IPhysicalEntity *GetHeightfieldData(heightfield *phf);
	virtual void SetHeightfieldMatMapping(int *pMatMapping, int nMats);
	virtual int SetSurfaceParameters(int surface_idx, float bounciness,float friction, unsigned int flags=0);
	virtual int GetSurfaceParameters(int surface_idx, float &bounciness,float &friction, unsigned int &flags);
	virtual int SetSurfaceParameters(int surface_idx, float bounciness,float friction,
		float damage_reduction, float ric_angle, float ric_dam_reduction,
		float ric_vel_reduction, unsigned int flags=0);
	virtual int GetSurfaceParameters(int surface_idx, float &bounciness,float &friction,
		float &damage_reduction, float &ric_angle, float &ric_dam_reduction,
		float &ric_vel_reduction, unsigned int &flags);
	virtual PhysicsVars *GetPhysVars() { return &m_vars; }

	void InitGThunksPool();
	void AllocGThunksPool( int nNewSize );
	void DeallocGThunksPool();
	void FlushOldThunks();
	void SortThunks();
	int GetFreeThunk();

	virtual IPhysicalEntity* CreatePhysicalEntity(pe_type type, pe_params* params=0, void *pForeignData=0,int iForeignData=0, int id=-1, IGeneralMemoryHeap* pHeap = NULL)
	{ return CreatePhysicalEntity(type,0.0f,params,pForeignData,iForeignData,id, NULL, pHeap); }
	virtual IPhysicalEntity* CreatePhysicalEntity(pe_type type, float lifeTime, pe_params* params=0, void *pForeignData=0,int iForeignData=0,
		int id=-1,IPhysicalEntity *pHostPlaceholder=0, IGeneralMemoryHeap* pHeap = NULL);
	virtual IPhysicalEntity *CreatePhysicalPlaceholder(pe_type type, pe_params* params=0, void *pForeignData=0,int iForeignData=0, int id=-1);
	virtual int DestroyPhysicalEntity(IPhysicalEntity *pent, int mode=0, int bThreadSafe=0);
	virtual int SetPhysicalEntityId(IPhysicalEntity *pent, int id, int bReplace=1, int bThreadSafe=0);
	virtual int GetPhysicalEntityId(IPhysicalEntity *pent);
	int GetFreeEntId();
	virtual IPhysicalEntity* GetPhysicalEntityById(int id);
	int IsPlaceholder(const CPhysicalPlaceholder *pent) {
		if (!pent) return 0;
		int iChunk; for(iChunk=0; iChunk<m_nPlaceholderChunks && (unsigned int)(pent-m_pPlaceholders[iChunk])>=(unsigned int)PLACEHOLDER_CHUNK_SZ; iChunk++);
		return iChunk<m_nPlaceholderChunks ? (iChunk<<PLACEHOLDER_CHUNK_SZLG2 | (int)(pent-m_pPlaceholders[iChunk]))+1 : 0;
	}

	virtual void TimeStep(float time_interval, int flags=ent_all|ent_deleted);
	virtual float GetPhysicsTime() { return m_timePhysics; }
	virtual int GetiPhysicsTime() { return m_iTimePhysics; }
	virtual void SetPhysicsTime(float time) {
		m_timePhysics = time;
		if (m_vars.timeGranularity>0)
			m_iTimePhysics = (int)(m_timePhysics/m_vars.timeGranularity+0.5f);
	}
	virtual void SetiPhysicsTime(int itime) { m_timePhysics = (m_iTimePhysics=itime)*m_vars.timeGranularity; }
	virtual void SetSnapshotTime(float time_snapshot,int iType=0) {
		m_timeSnapshot[iType] = time_snapshot;
		if (m_vars.timeGranularity>0)
			m_iTimeSnapshot[iType] = (int)(time_snapshot/m_vars.timeGranularity+0.5f);
	}
	virtual void SetiSnapshotTime(int itime_snapshot,int iType=0) {
		m_iTimeSnapshot[iType] = itime_snapshot; m_timeSnapshot[iType] = itime_snapshot*m_vars.timeGranularity;
	}

	// *important* if request RWIs queued iForeignData should be a EPhysicsForeignIds
	virtual int RayWorldIntersection(const Vec3& org,const Vec3& dir, int objtypes, unsigned int flags, ray_hit *hits,int nmaxhits,
		IPhysicalEntity **pSkipEnts=0,int nSkipEnts=0, void *pForeignData=0,int iForeignData=0,
		const char *pNameTag="RayWorldIntersection(Physics)", ray_hit_cached *phitLast=0, int iCaller=get_iCaller_int())
	{
		SRWIParams rp; rp.org=org; rp.dir=dir; rp.objtypes=objtypes; rp.flags=flags; rp.hits=hits; rp.nMaxHits=nmaxhits;
		rp.pForeignData=pForeignData; rp.iForeignData=iForeignData;	rp.phitLast=phitLast; rp.pSkipEnts=pSkipEnts; rp.nSkipEnts=nSkipEnts;
		return RayWorldIntersection(rp, pNameTag, iCaller);
	}
	virtual int RayWorldIntersection(const SRWIParams &rp, const char *pNameTag="RayWorldIntersection(Physics)", int iCaller=get_iCaller_int());
	virtual int TracePendingRays(int bDoTracing=1);

	void RayHeightfield(const Vec3 &org,Vec3 &dir, ray_hit *hits, int flags, int iCaller);
	void RayWater(const Vec3 &org,const Vec3 &dir, struct entity_grid_checker &egc, int flags,int nMaxHits, ray_hit *hits);

	virtual void SimulateExplosion(pe_explosion *pexpl, IPhysicalEntity **pSkipEnts=0,int nSkipEnts=0,
		int iTypes=ent_rigid|ent_sleeping_rigid|ent_living|ent_independent, int iCaller=get_iCaller_int());
	virtual float IsAffectedByExplosion(IPhysicalEntity *pent, Vec3 *impulse=0);
	virtual float CalculateExplosionExposure(pe_explosion *pexpl, IPhysicalEntity *pient);
	virtual void ResetDynamicEntities();
	virtual void DestroyDynamicEntities();
	virtual void PurgeDeletedEntities();
	virtual int DeformPhysicalEntity(IPhysicalEntity *pent, const Vec3 &ptHit,const Vec3 &dirHit,float r, int flags=0);
	virtual void UpdateDeformingEntities(float time_interval);
	virtual int GetEntityCount(int iEntType) { return m_nTypeEnts[iEntType]; }
	virtual int ReserveEntityCount(int nNewEnts);

  virtual IPhysicalEntityIt* GetEntitiesIterator();

	virtual void DrawPhysicsHelperInformation(IPhysRenderer *pRenderer,int iCaller=0);
	virtual void DrawEntityHelperInformation(IPhysRenderer *pRenderer,int iEntityId,int iDrawHelpers);

	virtual void GetMemoryStatistics(ICrySizer *pSizer);

	virtual int CollideEntityWithBeam(IPhysicalEntity *_pent, Vec3 org,Vec3 dir,float r, ray_hit *phit);
	virtual int RayTraceEntity(IPhysicalEntity *pient, Vec3 origin,Vec3 dir, ray_hit *pHit, pe_params_pos *pp=0,
		unsigned int geomFlagsAny = geom_colltype0|geom_colltype_player);
	virtual int CollideEntityWithPrimitive(IPhysicalEntity *_pent, int itype, primitive *pprim, Vec3 dir, ray_hit *phit, intersection_params* pip=0);

	virtual float PrimitiveWorldIntersection(const SPWIParams &pp, WriteLockCond *pLockContacts=0, const char *pNameTag="PrimitiveWorldIntersection");
	virtual void RasterizeEntities(const grid3d& grid, uchar *rbuf, int objtypes, float massThreshold, const Vec3& offsBBox, const Vec3& sizeBBox, int flags);

	virtual int GetEntitiesInBox(Vec3 ptmin,Vec3 ptmax, IPhysicalEntity **&pList, int objtypes, int szListPrealloc) {
		WriteLock lock(m_lockCaller[MAX_PHYS_THREADS]);
		return GetEntitiesAround(ptmin,ptmax, (CPhysicalEntity**&)pList, objtypes, 0, szListPrealloc, MAX_PHYS_THREADS);
	}
	int GetEntitiesAround(const Vec3 &ptmin,const Vec3 &ptmax, CPhysicalEntity **&pList, int objtypes, CPhysicalEntity *pPetitioner=0,
		int szListPrealoc=0, int iCaller=get_iCaller());
	int ChangeEntitySimClass(CPhysicalEntity *pent, int bGridLocked);
	int RepositionEntity(CPhysicalPlaceholder *pobj, int flags=3, Vec3 *BBox=0, int bQueued=0);
	void DetachEntityGridThunks(CPhysicalPlaceholder *pobj);
	void ScheduleForStep(CPhysicalEntity *pent, float time_interval);
	CPhysicalEntity *CheckColliderListsIntegrity();

	virtual int CoverPolygonWithCircles(strided_pointer<Vec2> pt,int npt,bool bConsecutive, const Vec2 &center,
		Vec2 *&centers,float *&radii, float minCircleRadius)
	{ return ::CoverPolygonWithCircles(pt,npt,bConsecutive, center, centers,radii, minCircleRadius); }
	virtual void DeletePointer(void *pdata) { if (pdata) delete[] (char*)pdata; }
	virtual int qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris, qhullmalloc qmalloc = 0) { return ::qhull(pts,npts,pTris, qmalloc); }
	virtual int qhull2d(ptitem2d *pts,int nVtx, edgeitem *edges, int nMaxEdges=0) { return ::qhull2d(pts,nVtx,edges,nMaxEdges); }
	virtual int TriangulatePoly(Vec2 *pVtx, int nVtx, int *pTris,int szTriBuf)
	{ return ::TriangulatePoly(pVtx, nVtx, pTris, szTriBuf); }
	virtual void SetPhysicsStreamer(IPhysicsStreamer *pStreamer) { m_pPhysicsStreamer=pStreamer; }
	virtual void SetPhysicsEventClient(IPhysicsEventClient *pEventClient) { m_pEventClient=pEventClient; }
	virtual float GetLastEntityUpdateTime(IPhysicalEntity *pent) { return m_updateTimes[((CPhysicalPlaceholder*)pent)->m_iSimClass & 7]; }
	virtual volatile int *GetInternalLock(int idx) {
		switch (idx) {
			case PLOCK_WORLD_STEP: return &m_lockStep;
			case PLOCK_QUEUE: return &m_lockQueue;
			case PLOCK_AREAS: return &m_lockAreas;
			case PLOCK_TRACE_PENDING_RAYS: return &m_lockTPR;
			default:
				if ((unsigned int)(idx-PLOCK_CALLER0)<=(unsigned int)MAX_PHYS_THREADS)
					return m_lockCaller+(idx-PLOCK_CALLER0);
		}
		return 0;
	}

	void AddEntityProfileInfo(CPhysicalEntity *pent,int nTicks);
	virtual int GetEntityProfileInfo(phys_profile_info *&pList)	{	pList=m_pEntProfileData; return m_nProfiledEnts; }
	void AddFuncProfileInfo(const char *name,int nTicks);
	virtual int GetFuncProfileInfo(phys_profile_info *&pList)	{	pList=m_pFuncProfileData; return m_nProfileFunx; }
	virtual int GetGroupProfileInfo(phys_profile_info *&pList) { pList=m_grpProfileData; return CRY_ARRAY_COUNT(m_grpProfileData); }
	virtual int GetJobProfileInfo(phys_job_info *&pList) { pList = m_JobProfileInfo; return CRY_ARRAY_COUNT(m_JobProfileInfo); }
	phys_job_info& GetJobProfileInst(int ijob) { return m_JobProfileInfo[ijob]; }

	// *important* if provided callback function return 0, other registered listeners are not called anymore.
	virtual void AddEventClient(int type, int (*func)(const EventPhys*), int bLogged, float priority=1.0f);
	virtual int RemoveEventClient(int type, int (*func)(const EventPhys*), int bLogged);
	virtual void PumpLoggedEvents();
	virtual uint32 GetPumpLoggedEventsTicks();
	virtual void ClearLoggedEvents();
	void CleanseEventsQueue();
	void PatchEventsQueue(IPhysicalEntity* pEntity, void* pForeignData, int iForeignData);
	EventPhys *AllocEvent(int id,int sz);
	template<class Etype> int OnEvent(unsigned int flags, Etype *pEvent, Etype **pEventLogged=0) {
		int res = 0;
		if ((flags & Etype::flagsCall)==Etype::flagsCall)
			res = SignalEvent(pEvent, 0);
		if ((flags & Etype::flagsLog)==Etype::flagsLog) {
			WriteLock lock(m_lockEventsQueue);
			Etype *pDst = (Etype*)AllocEvent(Etype::id,sizeof(Etype));
			memcpy(pDst,pEvent,sizeof(*pDst)); pDst->next = 0;
			(m_pEventLast ? m_pEventLast->next : m_pEventFirst) = pDst;
			m_pEventLast = pDst;
			if (pEventLogged)
				*pEventLogged = pDst;
			if (Etype::id==(const int)EventPhysPostStep::id) {
				CPhysicalPlaceholder *ppc = (CPhysicalPlaceholder*)((EventPhysPostStep*)pEvent)->pEntity;
				if (ppc->m_bProcessed & PENT_SETPOSED)
					AtomicAdd(&ppc->m_bProcessed, -PENT_SETPOSED);
			}
		}
		return res;
	}
	int SignalEvent(EventPhys *pEvent, int bLogged) {
		int nres = 0;
		EventClient *pClient;
		ReadLock lock(m_lockEventClients);
		for(pClient = m_pEventClients[pEvent->idval][bLogged]; pClient; pClient=pClient->next)
			nres += pClient->OnEvent(pEvent);
		return nres;
	}
	virtual int NotifyEventClients(EventPhys *pEvent, int bLogged) { return SignalEvent(pEvent,bLogged); }

	virtual int SerializeWorld(const char *fname, int bSave);
	virtual int SerializeGeometries(const char *fname, int bSave);

	virtual IPhysicalEntity *AddGlobalArea();
	virtual IPhysicalEntity *AddArea(Vec3 *pt,int npt, float zmin,float zmax, const Vec3 &pos=Vec3(0,0,0), const quaternionf &q=quaternionf(),
		float scale=1.0f, const Vec3& normal=Vec3(ZERO), int *pTessIdx=0,int nTessTris=0,Vec3 *pFlows=0);
	virtual IPhysicalEntity *AddArea(IGeometry *pGeom, const Vec3& pos,const quaternionf &q,float scale);
	virtual IPhysicalEntity *AddArea(Vec3 *pt,int npt, float r, const Vec3 &pos,const quaternionf &q,float scale);
	virtual void RemoveArea(IPhysicalEntity *pArea);
	virtual int CheckAreas(const Vec3 &ptc, Vec3 &gravity, pe_params_buoyancy *pb, int nMaxBuoys=1, int iMedium=-1, const Vec3 &vel=Vec3(ZERO),
		IPhysicalEntity *pent=0, int iCaller=get_iCaller_int());
	int CheckAreas(CPhysicalEntity *pent, Vec3 &gravity, pe_params_buoyancy *pb,int nMaxBuoys=1, const Vec3 &vel=Vec3(ZERO), int iCaller=get_iCaller_int()) {
		if (!m_pGlobalArea || pent->m_flags & pef_ignore_areas)
			return 0;
		return CheckAreas((pent->m_BBox[0]+pent->m_BBox[1])*0.5f, gravity, pb, nMaxBuoys, -1, vel, pent, iCaller);
	}
	void RepositionArea(CPhysArea *pArea, Vec3* pBoxPrev = 0);
	void ActivateArea(CPhysArea *pArea);
	virtual IPhysicalEntity *GetNextArea(IPhysicalEntity *pPrevArea=0);

	virtual void SetWaterMat(int imat);
	virtual int GetWaterMat() { return m_matWater; }
	virtual int SetWaterManagerParams(pe_params *params);
	virtual int GetWaterManagerParams(pe_params *params);
	virtual int GetWatermanStatus(pe_status *status);
	virtual void DestroyWaterManager();

	virtual int AddExplosionShape(IGeometry *pGeom, float size,int idmat, float probability=1.0f);
	virtual void RemoveExplosionShape(int id);
	virtual void RemoveAllExplosionShapes(void (*OnRemoveGeom)(IGeometry *pGeom)=0) {
		for(int i=0;i<m_nExpl;i++) {
			if (OnRemoveGeom) OnRemoveGeom(m_pExpl[i].pGeom);
			m_pExpl[i].pGeom->Release();
		}	m_nExpl=m_idExpl = 0;
	}
	IGeometry *GetExplosionShape(float size,int idmat, float &scale, int &bCreateConstraint);
	int DeformEntityPart(CPhysicalEntity *pent,int i, pe_explosion *pexpl, geom_world_data *gwd,geom_world_data *gwd1, int iSource=0);
	void MarkEntityAsDeforming(CPhysicalEntity *pent);
	void UnmarkEntityAsDeforming(CPhysicalEntity *pent);
	void ClonePhysGeomInEntity(CPhysicalEntity *pent,int i,IGeometry *pNewGeom);

	void AllocRequestsQueue(int sz) {
		if (m_nQueueSlotSize+sz+1 > QUEUE_SLOT_SZ) {
			if (m_nQueueSlots==m_nQueueSlotsAlloc)
				ReallocateList(m_pQueueSlots, m_nQueueSlots,m_nQueueSlotsAlloc+=8, true);
			if (!m_pQueueSlots[m_nQueueSlots])
				m_pQueueSlots[m_nQueueSlots] = new char[max(sz+1,QUEUE_SLOT_SZ)];
			++m_nQueueSlots; m_nQueueSlotSize = 0;
		}
	}
	void *QueueData(void *ptr, int sz) {
		void *storage = m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize;
		memcpy(storage, ptr, sz);	m_nQueueSlotSize += sz;
		*(int*)(m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize) = -1;
		return storage;
	}
	template<class T> T* QueueData(const T& data) {
		T &storage = *(T*)(m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize);
		storage = data;
		m_nQueueSlotSize += sizeof(data);
		*(int*)(m_pQueueSlots[m_nQueueSlots-1]+m_nQueueSlotSize) = -1;
		return &storage;
	}

	float GetFriction(int imat0,int imat1,int bDynamic=0) {
		float *pTable = (float*)((intptr_t)m_FrictionTable&~(intptr_t)-bDynamic | (intptr_t)m_DynFrictionTable&(intptr_t)-bDynamic);
		return max(0.0f,pTable[imat0 & NSURFACETYPES-1]+pTable[imat1 & NSURFACETYPES-1])*0.5f;
	}
	float GetBounciness(int imat0,int imat1) {
		return (m_BouncinessTable[imat0 & NSURFACETYPES-1]+m_BouncinessTable[imat1 & NSURFACETYPES-1])*0.5f;
	}

	virtual void SavePhysicalEntityPtr(TSerialize ser, IPhysicalEntity* pIEnt);
	virtual IPhysicalEntity* LoadPhysicalEntityPtr(TSerialize ser);
    virtual void GetEntityMassAndCom(IPhysicalEntity* pIEnt, float& mass, Vec3& com);
	virtual void* GetInternalImplementation(int type, void* object = nullptr) { return nullptr; }

	
	IPhysicalWorld *GetIWorld() { return this; }

	virtual void SerializeGarbageTypedSnapshot( TSerialize ser, int iSnapshotType, int flags );

	int GetTmpEntList(CPhysicalEntity **&pEntList, int iCaller)	{
		INT_PTR plist = (INT_PTR)m_threadData[iCaller].pTmpEntList;
		int is0=iCaller-1>>31, isN=MAX_PHYS_THREADS-iCaller-1>>31;
		plist += (INT_PTR)m_pTmpEntList -plist & is0;
		plist += (INT_PTR)m_pTmpEntList2-plist & isN;
		m_threadData[iCaller].pTmpEntList = (CPhysicalEntity**)plist;
		m_threadData[iCaller].szList += m_nEntsAlloc-m_threadData[iCaller].szList & (is0|isN);
		pEntList = m_threadData[iCaller].pTmpEntList;
		return m_threadData[iCaller].szList;
	}
	int ReallocTmpEntList(CPhysicalEntity **&pEntList, int iCaller, int szNew);

	void ProcessNextEntityIsland(float time_interval, int ipass, int iter, int &bAllGroupsFinished, int iCaller);
	void ProcessIslandSolverResults(int i, int iter, float groupTimeStep,float Ebefore, int nEnts,float fixedDamping, int &bAllGroupsFinished,
		entity_contact **pContacts,int nContacts,int nBodies, int iCaller,int64 iticks0);
	int ReadDelayedSolverResults(CMemStream &stm, float &dt,float &Ebefore,int &nEnts,float &fixedDamping, entity_contact **pContacts,RigidBody **pBodies);
	void ProcessNextEngagedIndependentEntity(int iCaller);
	void ProcessNextLivingEntity(float time_interval, int bSkipFlagged, int iCaller);
	void ProcessNextIndependentEntity(float time_interval, int bSkipFlagged, int iCaller);
	void ProcessBreakingEntities(float time_interval);
	void ThreadProc(int ithread, SPhysTask *pTask);

	template<class T> void ReallocQueue(T *&pqueue, int sz,int &szAlloc, int &head,int &tail, int nGrow) {
		if (sz==szAlloc) {
			T *pqueueOld = pqueue;
			pqueue = new T[szAlloc+nGrow];
			memcpy(pqueue, pqueueOld, (head+1)*sizeof(T));
			memcpy(pqueue+head+nGrow+1, pqueueOld+head+1, (sz-head-1)*sizeof(T));
			if (tail) tail += nGrow;
			szAlloc += nGrow;
			delete[] pqueueOld;
		}
		head = head+1 - (szAlloc & szAlloc-2-head>>31);
	}

	virtual EventPhys *AddDeferredEvent( int type, EventPhys *event )
	{
		switch (type) {
			case EventPhysBBoxOverlap::id       : { EventPhysBBoxOverlap *pLogged; OnEvent(EventPhysBBoxOverlap::flagsLog,(EventPhysBBoxOverlap*)event,&pLogged); return pLogged; }
			case EventPhysCollision::id         : { EventPhysCollision *pLogged; OnEvent(EventPhysCollision::flagsLog,(EventPhysCollision*)event,&pLogged); return pLogged; }
			case EventPhysStateChange::id       : { EventPhysStateChange *pLogged; OnEvent(EventPhysStateChange::flagsLog,(EventPhysStateChange*)event,&pLogged); return pLogged; }
			case EventPhysEnvChange::id         : { EventPhysEnvChange *pLogged; OnEvent(EventPhysEnvChange::flagsLog,(EventPhysEnvChange*)event,&pLogged); return pLogged; }
			case EventPhysPostStep::id          : { EventPhysPostStep *pLogged; OnEvent(EventPhysPostStep::flagsLog,(EventPhysPostStep*)event,&pLogged); return pLogged; }
			case EventPhysUpdateMesh::id        : { EventPhysUpdateMesh *pLogged; OnEvent(EventPhysUpdateMesh::flagsLog,(EventPhysUpdateMesh*)event,&pLogged); return pLogged; }
			case EventPhysCreateEntityPart::id  : { EventPhysCreateEntityPart *pLogged; OnEvent(EventPhysCreateEntityPart::flagsLog,(EventPhysCreateEntityPart*)event,&pLogged); return pLogged; }
			case EventPhysRemoveEntityParts::id : { EventPhysRemoveEntityParts *pLogged; OnEvent(EventPhysRemoveEntityParts::flagsLog,(EventPhysRemoveEntityParts*)event,&pLogged); return pLogged; }
			case EventPhysRevealEntityPart::id  : { EventPhysRevealEntityPart *pLogged; OnEvent(EventPhysRevealEntityPart::flagsLog,(EventPhysRevealEntityPart*)event,&pLogged); return pLogged; }
			case EventPhysJointBroken::id       : { EventPhysJointBroken *pLogged; OnEvent(EventPhysJointBroken::flagsLog,(EventPhysJointBroken*)event,&pLogged); return pLogged; }
			case EventPhysRWIResult::id         : { EventPhysRWIResult *pLogged; OnEvent(EventPhysRWIResult::flagsLog,(EventPhysRWIResult*)event,&pLogged); return pLogged; }
			case EventPhysPWIResult::id         : { EventPhysPWIResult *pLogged; OnEvent(EventPhysPWIResult::flagsLog,(EventPhysPWIResult*)event,&pLogged); return pLogged; }
			case EventPhysArea::id              : { EventPhysArea *pLogged; OnEvent(EventPhysArea::flagsLog,(EventPhysArea*)event,&pLogged); return pLogged; }
			case EventPhysAreaChange::id        : { EventPhysAreaChange *pLogged; OnEvent(EventPhysAreaChange::flagsLog,(EventPhysAreaChange*)event,&pLogged); return pLogged; }
			case EventPhysEntityDeleted::id     : { EventPhysEntityDeleted *pLogged; OnEvent(EventPhysEntityDeleted::flagsLog,(EventPhysEntityDeleted*)event,&pLogged); return pLogged; }
		}
		return 0;
	}

	template <class T> T *AllocPooledObj(T *&pFirstFree, int &countFree,int &countAlloc, volatile int &ilock)
	{
		WriteLock lock(ilock);
		if (pFirstFree->next==pFirstFree) {
			T *pChunk = new T[64];
			memset(pChunk, 0, sizeof(T)*64);
			for(int i=0;i<64;i++) {
				pChunk[i].next = pChunk+i+1; pChunk[i].prev = pChunk+i-1;
				pChunk[i].bChunkStart = 0;
			}
			pChunk[0].prev = pChunk[63].next = CONTACT_END(pFirstFree);
			(pFirstFree = pChunk)->bChunkStart = 1;
			countFree += 64; countAlloc += 64;
		}
		T *pObj = pFirstFree;
		pFirstFree->next->prev = pFirstFree->prev;
		pFirstFree->prev->next = pFirstFree->next;
		pObj->next=pObj->prev = pObj;
		countFree--;
		return pObj;
	}
	template <class T> void FreePooledObj(T *pObj, T *&pFirstFree, int &countFree, volatile int &ilock)
	{
		WriteLock lock(ilock);
		pObj->prev = pFirstFree->prev; pObj->next = pFirstFree;
		pFirstFree->prev = pObj; pFirstFree = pObj;
		countFree++;
	}
	template <class T> void FreeObjPool(T *&pFirstFree, int countFree,int &countAlloc)
	{
		T *pObj,*pObjNext;
		for(pObj=pFirstFree; pObj!=CONTACT_END(pFirstFree); pObj=pObj->next) if (!pObj->bChunkStart) {
			pObj->prev->next=pObj->next; pObj->next->prev=pObj->prev;
		}

		// workaround for a aliasing problem with pObj->next pointing into CPhysicalWorld;
		// the compiler assumes that pFristFree doesn't changein the first loop, thus it spares
		// a second load in the second loop, which is wrong.
		// the MEMORY_RW_REORDERING_BARRIER ensures a reload of pFirstFree
		MEMORY_RW_REORDERING_BARRIER;

		for(pObj=pFirstFree; pObj!=CONTACT_END(pFirstFree); pObj=pObjNext) {
			pObjNext = pObj->next; delete[] pObj;
		}
		pFirstFree = CONTACT_END(pFirstFree); pFirstFree->prev = pFirstFree;
		countAlloc = countFree = 0;
	}

	entity_contact *AllocContact() { return AllocPooledObj(m_pFreeContact,m_nFreeContacts,m_nContactsAlloc,m_lockContacts); }
	void FreeContact(entity_contact *pContact) { FreePooledObj(pContact, m_pFreeContact,m_nFreeContacts,m_lockContacts); }

	geom *AllocEntityPart() { return AllocPooledObj(m_pFreeEntPart,m_nFreeEntParts,m_nEntPartsAlloc,m_lockEntParts); }
	void FreeEntityPart(geom *pPart) { FreePooledObj(pPart, m_pFreeEntPart,m_nFreeEntParts,m_lockEntParts); }

	geom *AllocEntityParts(int count) { return count==1 ? AllocEntityPart() : new geom[count]; }
	void FreeEntityParts(geom *pParts, int count) {
		if (pParts!=&CPhysicalEntity::m_defpart)
			if (count==1)
				FreeEntityPart(pParts);
			else
				delete[] pParts;
	}

	PhysicsVars m_vars;
	ILog *m_pLog;
	IPhysicsStreamer *m_pPhysicsStreamer;
	IPhysicsEventClient *m_pEventClient;
	IPhysRenderer *m_pRenderer;

	CPhysicalEntity *m_pTypedEnts[8],*m_pTypedEntsPerm[8];
	CPhysicalEntity **m_pTmpEntList,**m_pTmpEntList1,**m_pTmpEntList2;
	CPhysicalEntity *m_pHiddenEnts;
	float *m_pGroupMass,*m_pMassList;
	int *m_pGroupIds,*m_pGroupNums;

	SEntityGrid m_entgrid;
	SEntityGrid *m_pDeletedGrids = nullptr;
#ifdef MULTI_GRID
	SEntityGrid* GetGrid(const CPhysicalPlaceholder *pobj) { return pobj->m_pGrid; }
	SEntityGrid* GetGrid(const IPhysicalEntity *pIEnt) { return pIEnt!=WORLD_ENTITY ? GetGrid((CPhysicalPlaceholder*)pIEnt) : &m_entgrid; }
	SEntityGrid* SetGrid(CPhysicalPlaceholder *pobj, SEntityGrid *pgrid) { SEntityGrid *pgridPrev=GetGrid(pobj); pobj->m_pGrid=pgrid; return pgridPrev; }
	SEntityGrid* GetHostedGrid(const CPhysicalEntity *pent) { 
		return pent->m_pOuterEntity && pent->m_pOuterEntity->GetType()==PE_GRID ? static_cast<SEntityGrid*>((IPhysicalEntity*)pent->m_pOuterEntity) : nullptr;
	}
	template<typename Rot,typename Tdst> QuatT TransformToGrid(const CPhysicalPlaceholder *src, const Tdst *dst, Vec3 &offs, Rot &rot) {
		if (GetGrid(src) != GetGrid(dst)) {
			QuatT diff = GetGrid(dst)->m_transW.GetInverted()*GetGrid(src)->m_transW;
			rot = Rot(diff.q)*rot;
			offs = diff*offs;
			return diff;
		}
		return QuatT(IDENTITY);
	}
#else
	template<typename T> SEntityGrid* GetGrid(const T *pobj) { return &m_entgrid; }
	SEntityGrid* SetGrid(CPhysicalPlaceholder *pobj, SEntityGrid *pgrid) { return &m_entgrid; }
	SEntityGrid* GetHostedGrid(const CPhysicalEntity *pent) { return nullptr; }
	template<typename Rot,typename Tdst> QuatT TransformToGrid(const CPhysicalPlaceholder *src, const Tdst *dst, Vec3 &offs, Rot &rot) { return QuatT(IDENTITY); }
#endif
	void UnlockGrid(CPhysicalPlaceholder *pobj, int val=-WRITE_LOCK_VAL) { AtomicAdd(&m_lockGrid, val); }
	void SyncPortal(CPhysicalPlaceholder *portal);
	SEntityGrid* DestroyGrid(SEntityGrid *pgrid);	// returns pgrid->m_next

	pe_gridthunk *m_gthunks;
	int m_thunkPoolSz,m_iFreeGThunk0;
	pe_gridthunk *m_oldThunks;
	volatile int m_lockOldThunks;
	int m_nEnts,m_nEntsAlloc;
	int m_nDynamicEntitiesDeleted;
	CPhysicalPlaceholder **m_pEntsById;
	int m_nIdsAlloc, m_iNextId;
	int m_iNextIdDown,m_lastExtId,m_nExtIds;
	int m_bGridThunksChanged;
	int m_bUpdateOnlyFlagged;
	int m_nTypeEnts[10];
	int m_bEntityCountReserved;
#ifndef _RELEASE
	volatile int m_nGEA[MAX_PHYS_THREADS+1];
#endif
	int m_nEntListAllocs;
	int m_nOnDemandListFailures;
	int m_iLastPODUpdate;
	Vec3 m_prevGEABBox[MAX_PHYS_THREADS+1][2];
	int m_prevGEAobjtypes[MAX_PHYS_THREADS+1];
	int m_nprevGEAEnts[MAX_PHYS_THREADS+1];

	int m_nPlaceholders,m_nPlaceholderChunks,m_iLastPlaceholder;
	CPhysicalPlaceholder **m_pPlaceholders;
	int *m_pPlaceholderMap;
	CPhysicalEntity *m_pEntBeingDeleted;

	SOcclusionCubeMap m_cubeMapStatic;
	SOcclusionCubeMap m_cubeMapDynamic;
	Vec3 m_lastEpicenter,m_lastEpicenterImp,m_lastExplDir;
	CPhysicalEntity **m_pExplVictims;
	float *m_pExplVictimsFrac;
	Vec3 *m_pExplVictimsImp;
	int m_nExplVictims,m_nExplVictimsAlloc;

	CPhysicalEntity *m_pHeightfield[MAX_PHYS_THREADS+2];
	Matrix33 m_HeightfieldBasis;
	Vec3 m_HeightfieldOrigin;

	float m_timePhysics,m_timeSurplus,m_timeSnapshot[4];
	int m_iTimePhysics,m_iTimeSnapshot[4];
	float m_updateTimes[8];
	int m_iSubstep,m_bWorldStep;
	float m_curGroupMass;
	CPhysicalEntity *m_pAuxStepEnt;
	phys_profile_info m_pEntProfileData[64];
	int m_nProfiledEnts;
	phys_profile_info *m_pFuncProfileData;
	int m_nProfileFunx,m_nProfileFunxAlloc;
	volatile int m_lockEntProfiler,m_lockFuncProfiler;
	phys_profile_info m_grpProfileData[16];
	phys_job_info m_JobProfileInfo[6];
	float m_lastTimeInterval;
	int m_nSlowFrames;
	uint32 m_nPumpLoggedEventsHits;
	volatile threadID m_idThread;
	volatile threadID m_idPODThread;
	int m_bMassDestruction;

	float m_BouncinessTable[NSURFACETYPES];
	float m_FrictionTable[NSURFACETYPES];
	float m_DynFrictionTable[NSURFACETYPES];
	float m_DamageReductionTable[NSURFACETYPES];
	float m_RicochetAngleTable[NSURFACETYPES];
	float m_RicDamReductionTable[NSURFACETYPES];
	float m_RicVelReductionTable[NSURFACETYPES];
	unsigned int m_SurfaceFlagsTable[NSURFACETYPES];
	int m_matWater,m_bCheckWaterHits;
	class CWaterMan *m_pWaterMan;
	Vec3 m_posViewer;

	char **m_pQueueSlots,**m_pQueueSlotsAux;
	int m_nQueueSlots,m_nQueueSlotsAlloc;
	int m_nQueueSlotsAux,m_nQueueSlotsAllocAux;
	int m_nQueueSlotSize,m_nQueueSlotSizeAux;

	CPhysArea *m_pGlobalArea;
	int m_nAreas,m_nBigAreas;
	CPhysArea *m_pDeletedAreas;
	CPhysArea *m_pTypedAreas[16];
	CPhysArea *m_pActiveArea;
	int m_numNonWaterAreas;
	int m_numGravityAreas;
	int m_numAreaTriggers;

	EventPhys *m_pEventFirst,*m_pEventLast,*m_pFreeEvents[EVENT_TYPES_NUM];
	EventClient *m_pEventClients[EVENT_TYPES_NUM][2];
	EventChunk *m_pFirstEventChunk,*m_pCurEventChunk;
	int m_szCurEventChunk;
	int m_nEvents[EVENT_TYPES_NUM];
	volatile int m_idStep;

	entity_contact *m_pFreeContact,*m_pLastFreeContact;
	int m_nFreeContacts;
	int m_nContactsAlloc;

	geom *m_pFreeEntPart,*m_pLastFreeEntPart;
	geom *m_oldEntParts;
	int m_nFreeEntParts;
	int m_nEntPartsAlloc;

	SExplosionShape *m_pExpl;
	int m_nExpl,m_nExplAlloc,m_idExpl;
	CPhysicalEntity **m_pDeformingEnts;
	int m_nDeformingEnts,m_nDeformingEntsAlloc;
	SBreakRequest *m_breakQueue;
	int m_breakQueueHead,m_breakQueueTail;
	int m_breakQueueSz,m_breakQueueAlloc;

	SRwiRequest *m_rwiQueue;
	int m_rwiQueueHead,m_rwiQueueTail;
	int m_rwiQueueSz,m_rwiQueueAlloc;
	ray_hit *m_pRwiHitsHead,*m_pRwiHitsTail, *m_pRwiHitsPool;
	int m_rwiHitsPoolSize;
	int m_rwiPoolEmpty;

	SPwiRequest *m_pwiQueue;
	int m_pwiQueueHead,m_pwiQueueTail;
	int m_pwiQueueSz,m_pwiQueueAlloc;

	SThreadTaskRequest m_rq;
	CryEvent m_threadStart[MAX_PHYS_THREADS],m_threadDone[MAX_PHYS_THREADS];
	SThreadData m_threadData[MAX_PHYS_THREADS+1];
	SPhysTask *m_threads[MAX_PHYS_THREADS];
	Vec3 m_BBoxPlayerGroup[MAX_PHYS_THREADS+1][2];
	int m_nGroups;
	float m_maxGroupMass;
	volatile int m_nWorkerThreads;
	volatile int m_iCurGroup;
	volatile CPhysicalEntity *m_pCurEnt;
	volatile CPhysicalEntity *m_pMovedEnts;
	volatile int m_lockNextEntityGroup;
	volatile int m_lockMovedEntsList;
	volatile int m_lockPlayerGroups;

	volatile int m_lockGrid;
	volatile int m_lockPODGrid;
	volatile int m_lockEntIdList;
	volatile int m_lockStep, m_lockCaller[MAX_PHYS_THREADS+1],m_lockQueue,m_lockList;
	volatile int m_lockAreas;
	volatile int m_lockActiveAreas;
	volatile int m_lockEventsQueue,m_iLastLogPump, m_lockEventClients;
	volatile int m_lockDeformingEntsList;
	volatile int m_lockRwiQueue;
	volatile int m_lockRwiHitsPool;
	volatile int m_lockTPR;
	volatile int m_lockPwiQueue;
	volatile int m_lockContacts;
	volatile int m_lockEntParts;
	volatile int m_lockBreakQueue;
	volatile int m_lockAuxStepEnt;
	volatile int m_lockWaterMan;
};

#ifdef MULTI_GRID
template<typename Tdst> inline QuatT GridTrans(const CPhysicalPlaceholder *pentSrc, const Tdst *pentDst) {
	return pentDst->m_pWorld->GetGrid(pentDst)->m_transW.GetInverted() * pentDst->m_pWorld->GetGrid(pentSrc)->m_transW;
}
#else
template<typename Tdst> inline QuatT GridTrans(const CPhysicalPlaceholder *pentSrc, const Tdst *pentDst) { return QuatT(IDENTITY); }
#endif

template<typename Rot> inline void CPhysicalEntity::GetPartTransform(int ipart, Vec3 &offs, Rot &R, float &scale, const CPhysicalPlaceholder *trg) const 
{
	R = Rot(m_pNewCoords->q*m_parts[ipart].pNewCoords->q);
	offs = m_pNewCoords->q*m_parts[ipart].pNewCoords->pos + m_pNewCoords->pos;
	scale = m_parts[ipart].scale;
	m_pWorld->TransformToGrid(this,trg, offs,R);
}

template<typename CTrg> inline Vec3* CPhysicalEntity::GetPartBBox(int ipart, Vec3* BBox, const CTrg *trg) const 
{	
	if (m_pWorld->GetGrid(this)==m_pWorld->GetGrid(trg))
		return m_parts[ipart].BBox; 
	QuatT trans = GridTrans(this,trg);
	return transformBBox(m_parts[ipart].BBox[0], m_parts[ipart].BBox[1], BBox, trans);
}

inline RigidBody *CPhysicalEntity::GetRigidBodyTrans(RigidBody *pbody, int ipart, CPhysicalEntity *trg, int type, bool needIinv)
{
	RigidBody *pbodySrc = type==2 ? GetRigidBodyData(pbody,ipart) : GetRigidBody(ipart,type&1);
	if (m_pWorld->GetGrid(this)==m_pWorld->GetGrid(trg))
		return pbodySrc;
	QuatT trans = GridTrans(this,trg);
	pbody->v = trans.q*pbodySrc->v;
	pbody->w = trans.q*pbodySrc->w;
	pbody->pos = trans*pbodySrc->pos;
	pbody->M=pbodySrc->M; pbody->Minv=pbodySrc->Minv;
	if (needIinv)	{
		pbody->flags = 0;
		pbody->Iinv = Matrix33(trans.q)*pbodySrc->Iinv*Matrix33(!trans.q);
	}
	return pbody;
}


const int PHYS_FOREIGN_ID_PHYS_AREA = 12;

class CRY_ALIGN(128) CPhysArea : public CPhysicalPlaceholder {
public:
	CPhysArea(CPhysicalWorld *pWorld);
	~CPhysArea();
	virtual pe_type GetType() const { return PE_AREA; }
	virtual bool IsPlaceholder() const { return false; }
	virtual CPhysicalEntity *GetEntity();
	virtual CPhysicalEntity *GetEntityFast() { return GetEntity(); }
	virtual int AddRef() { return CryInterlockedIncrement(&m_lockRef); }
	virtual int Release() { return CryInterlockedDecrement(&m_lockRef); }

	virtual int SetParams(pe_params* params,int bThreadSafe=1);
	virtual int Action(pe_action* action,int bThreadSafe=1);
	virtual int GetParams(pe_params* params) const;
	virtual int GetStatus(pe_status* status) const;
	virtual IPhysicalWorld *GetWorld() const { return (IPhysicalWorld*)m_pWorld; }
	int CheckPoint(const Vec3& pttest, float radius=0.0f) const;
	int ApplyParams(const Vec3& pt, Vec3& gravity,const Vec3 &vel, pe_params_buoyancy *pbdst,int nBuoys,int nMaxBuoys,int &iMedium0, IPhysicalEntity *pent) const;
	float FindSplineClosestPt(const Vec3 &ptloc, int &iClosestSeg,float &tClosest) const;
	int FindSplineClosestPt(const Vec3 &org, const Vec3 &dir, Vec3 &ptray, Vec3 &ptspline) const;
	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);
	int RayTraceInternal(const Vec3 &org, const Vec3 &dir, ray_hit *pHit, pe_params_pos *pp);
	int RayTrace(const Vec3 &org, const Vec3 &dir, ray_hit *pHit, pe_params_pos *pp=(pe_params_pos*)0)
	{
		return RayTraceInternal(org, dir, pHit, pp);
	}
	void Update(float dt);
	void ProcessBorder();
	void DeleteWaterMan();
	static int OnBBoxOverlap(const EventPhysBBoxOverlap*);

	float GetExtent(EGeomForm eForm) const;
	void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;

	virtual void GetMemoryStatistics(ICrySizer *pSizer) const;

	int m_bDeleted;
	CPhysArea *m_next,*m_nextBig,*m_nextTyped,*m_nextActive;
	CPhysicalWorld *m_pWorld;

	Vec3 m_offset;
	Matrix33 m_R;
	float m_scale,m_rscale;

	Vec3 m_offset0;
	Matrix33 m_R0;

	IGeometry *m_pGeom;
	Vec2 *m_pt;
	int m_npt;
	float m_zlim[2];
	int *m_idxSort[2];
	Vec3 *m_pFlows;
	unsigned int *m_pMask;
	Vec3 m_size0;
	Vec3 *m_ptSpline;
	float m_V;
	float m_accuracyV;
	class CTriMesh *m_pContainer;
	CPhysicalEntity *m_pContainerEnt;
	int m_idpartContainer;
	QuatTS m_qtsContainer;
	int *m_pContainerParts;
	int m_nContainerParts;
	int m_bConvexBorder;
	IPhysicalEntity *m_pTrigger;
	float m_moveAccum;
	int m_nSleepFrames;
	float m_sleepVel;
	float m_borderPad;
	float m_szCell;
	params_wavesim m_waveSim;
	float m_sizeReserve;
	float m_minObjV;
	class CWaterMan *m_pWaterMan;
	class CTriMesh *m_pSurface;
	int m_nptAlloc;
	phys_geometry *m_pMassGeom;
	mutable CGeomExtents m_Extents;
	mutable Vec3 m_ptLastCheck;
	mutable int m_iClosestSeg;
	mutable float m_tClosest,m_mindist;
	mutable indexed_triangle m_trihit;

	pe_params_buoyancy m_pb;
	Vec3 m_gravity;
	Vec3 m_rsize;
	int m_bUniform;
	float m_falloff0,m_rfalloff0;
	float m_damping;
	int m_bUseCallback;
	int m_debugGeomHash;
	mutable volatile int m_lockRef;
};

extern int g_nPhysWorlds;
extern CPhysicalWorld *g_pPhysWorlds[];

inline int IsPODThread(CPhysicalWorld *pWorld) { return iszero((int32)(CryGetCurrentThreadId()-(pWorld->m_idPODThread))); }
inline void MarkAsPODThread(CPhysicalWorld *pWorld) { pWorld->m_idPODThread = CryGetCurrentThreadId(); }
inline void UnmarkAsPODThread(CPhysicalWorld *pWorld) { pWorld->m_idPODThread = THREADID_NULL; }
extern int g_idxThisIsPODThread;

struct CPhysEntityProfilerBase {
	volatile int64 m_iStartTime;
	CPhysEntityProfilerBase() {
		m_iStartTime = CryGetTicks();
	}
};
struct CPhysEntityProfiler : CPhysEntityProfilerBase {
	CPhysicalEntity *m_pEntity;
	CPhysEntityProfiler(CPhysicalEntity *pent) { m_pEntity=pent; }
	~CPhysEntityProfiler() {
		if (m_pEntity->m_pWorld->m_vars.bProfileEntities)
			m_pEntity->m_pWorld->AddEntityProfileInfo(m_pEntity,(int)((CryGetTicks()-m_iStartTime)));
	}
};
struct CPhysAreaProfiler : CPhysEntityProfilerBase {
	CPhysArea *m_pArea;
	CPhysAreaProfiler(CPhysArea *pArea) { m_pArea=pArea; }
	~CPhysAreaProfiler() {
		if (m_pArea && m_pArea->m_pWorld->m_vars.bProfileEntities)
			m_pArea->m_pWorld->AddEntityProfileInfo((CPhysicalEntity*)m_pArea,(int)((CryGetTicks()-m_iStartTime)));
	}
};

struct CPhysFuncProfiler {
	volatile int64 m_iStartTime;
	const char *m_name;
	CPhysicalWorld *m_pWorld;

	CPhysFuncProfiler(const char *name, CPhysicalWorld *pWorld) {
		m_name = name; m_pWorld = pWorld;
		m_iStartTime = CryGetTicks();
	}
	~CPhysFuncProfiler() {
		if (m_pWorld->m_vars.bProfileFunx)
			m_pWorld->AddFuncProfileInfo(m_name, (int)(CryGetTicks()-m_iStartTime));
	}
};

struct CBlockProfiler {
	volatile int64 &m_time;
	CBlockProfiler(int64 &time) : m_time(time) { m_time=CryGetTicks(); }
	~CBlockProfiler() { m_time = CryGetTicks()-m_time; }
};

template<class T> inline QuatT InputGridTrans(const T* input, const CPhysicalEntity *trg) {
	return !is_unused(input->pGridRefEnt) && input->pGridRefEnt && trg->m_pWorld->GetGrid(trg)!=trg->m_pWorld->GetGrid(input->pGridRefEnt) ?
		trg->m_pWorld->GetGrid(trg)->m_transW.GetInverted() * trg->m_pWorld->GetGrid(input->pGridRefEnt)->m_transW :
		QuatT(IDENTITY);
}

template<class T> inline bool StructChangesPos(T*) { return false; }
inline bool StructChangesPos(pe_params *params) {
	pe_params_pos *pp = (pe_params_pos*)params;
	return params->type==pe_params_pos::type_id && (!is_unused(pp->pos) || !is_unused(pp->pMtx3x4) && pp->pMtx3x4) && !(pp->bRecalcBounds & 16);
}

template<class T> inline void OnStructQueued(T *params, CPhysicalWorld *pWorld, void *ptrAux,int iAux) {}
inline void OnStructQueued(pe_geomparams *params, CPhysicalWorld *pWorld, void *ptrAux,int iAux) { pWorld->AddRefGeometry((phys_geometry*)ptrAux); }

template<class T> struct ChangeRequest {
	CPhysicalWorld *m_pWorld;
	int m_bQueued,m_bLocked,m_bLockedCaller;
	T *m_pQueued;

	ChangeRequest(CPhysicalPlaceholder *pent, CPhysicalWorld *pWorld, T *params, int bInactive, void *ptrAux=0,int iAux=0) :
	m_pWorld(pWorld),m_bQueued(0),m_bLocked(0),m_bLockedCaller(0)
	{
		if (pent->m_iSimClass==-1 && ((CPhysicalEntity*)pent)->m_iDeletionTime==3)
			bInactive |= bInactive-1>>31;
		if ((unsigned int)pent->m_iSimClass>=7u && bInactive>=0 && !AllowChangesOnDeleted(params)) {
			extern int g_dummyBuf[16];
			m_bQueued = 1; m_pQueued = (T*)(g_dummyBuf+1); return;
		}
		if (bInactive<=0) {
			int isPODthread;
			int isProcessed = 0;
			while (!isProcessed) {
				if (m_pWorld->m_lockStep || m_pWorld->m_lockTPR || pent->m_bProcessed>=PENT_QUEUED || bInactive<0 ||
						!(isPODthread=IsPODThread(m_pWorld)) && m_pWorld->m_lockCaller[MAX_PHYS_THREADS])
				{
					subref *psubref;
					int szSubref,szTot;
					WriteLock lock(m_pWorld->m_lockQueue);
					AtomicAdd(&pent->m_bProcessed, PENT_QUEUED);
					for(psubref=GetSubref(params),szSubref=0; psubref; psubref=psubref->next)
						if (*(char**)((char*)params+psubref->iPtrOffs) && !is_unused(*(char**)((char*)params+psubref->iPtrOffs)))
							szSubref += ((max(0,*(int*)((char*)params+max(0,psubref->iSzOffs))) & -psubref->iSzOffs>>31) + psubref->iSzFixed)*psubref->iSzUnit;
					szTot = sizeof(int)*2+sizeof(void*)+GetStructSize(params)+szSubref;
					if (StructUsesAuxData(params))
						szTot += sizeof(void*)+sizeof(int);
					m_pWorld->AllocRequestsQueue(szTot);
					m_pWorld->QueueData(GetStructId(params));
					m_pWorld->QueueData(szTot);
					m_pWorld->QueueData(pent);
					if (StructUsesAuxData(params)) {
						m_pWorld->QueueData(ptrAux);
						m_pWorld->QueueData(iAux);
					}
					m_pQueued = (T*)m_pWorld->QueueData(params, GetStructSize(params));
					for(psubref=GetSubref(params); psubref; psubref=psubref->next) {
						szSubref = ((*(int*)((char*)params+max(0,psubref->iSzOffs)) & -psubref->iSzOffs>>31) + psubref->iSzFixed)*psubref->iSzUnit;
						char *pParams = *(char**)((char*)params+psubref->iPtrOffs);
						if (pParams && !is_unused(pParams) && szSubref>=0)
							*(void**)((char*)m_pQueued+psubref->iPtrOffs) = m_pWorld->QueueData(pParams, szSubref);
					}
					OnStructQueued(params,pWorld,ptrAux,iAux);
					m_bQueued = 1;
					isProcessed = 1;
				}	else {
					if (!isPODthread) {
						if (AtomicCAS(&m_pWorld->m_lockCaller[MAX_PHYS_THREADS], WRITE_LOCK_VAL, 0)) {
							continue;
						}
						m_bLockedCaller = WRITE_LOCK_VAL;
					}
					if (AtomicCAS(&m_pWorld->m_lockStep, WRITE_LOCK_VAL, 0)) {
						if (!isPODthread) {
							AtomicAdd(&m_pWorld->m_lockCaller[MAX_PHYS_THREADS], -m_bLockedCaller);
							m_bLockedCaller=0;
						}
						continue;
					}
					m_bLocked = WRITE_LOCK_VAL;
					isProcessed = 1;
				}
			}
		}
		if (StructChangesPos(params) && !(pent->m_bProcessed & PENT_SETPOSED))
			AtomicAdd(&pent->m_bProcessed, PENT_SETPOSED);
	}
	~ChangeRequest() { AtomicAdd(&m_pWorld->m_lockStep,-m_bLocked);AtomicAdd(&m_pWorld->m_lockCaller[MAX_PHYS_THREADS],-m_bLockedCaller); }
	int IsQueued() { return m_bQueued; }
	T *GetQueuedStruct() { return m_pQueued; }
};

inline void InitEventBase(EventPhysMono *ev, CPhysicalEntity *pent)	{
	ev->pEntity = pent; ev->pForeignData = pent->m_pForeignData; ev->iForeignData = pent->m_iForeignData;
}
inline void InitEvent(EventPhysPostStep *ev, CPhysicalEntity *pent, int iCaller)	{
	InitEventBase((EventPhysMono*)ev, pent);
	ev->pGrid = pent->m_pWorld->GetGrid(pent);
	ev->iCaller = iCaller;
}

class CRayGeom;
struct geom_contact;

//do not process lock
struct SRayTraceRes
{
	const CRayGeom *pRay;
	geom_contact *pcontacts;
	ILINE SRayTraceRes(const CRayGeom *const ray,geom_contact *contacts)
		: pRay(ray),pcontacts(contacts){}
	ILINE void SetLock(volatile int*){}
};
#endif
