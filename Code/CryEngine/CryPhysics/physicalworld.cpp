// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "overlapchecks.h"
#include "intersectionchecks.h"
#include "raybv.h"
#include "raygeom.h"
#include "geoman.h"
#include "singleboxtree.h"
#include "boxgeom.h"
#include "cylindergeom.h"
#include "capsulegeom.h"
#include "spheregeom.h"
#include "trimesh.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "rigidentity.h"
#include "walkingrigidentity.h"
#include "particleentity.h"
#include "livingentity.h"
#include "wheeledvehicleentity.h"
#include "articulatedentity.h"
#include "ropeentity.h"
#include "softentity.h"
#include "tetrlattice.h"
#include "physicalworld.h"
#include "waterman.h"
#include <CryCore/Platform/CryWindows.h>

int g_dummyBuf[16];

// forward declarations of memory functions to access static data
namespace TriMeshStaticData{ void GetMemoryUsage( ICrySizer *pSizer ); }

#ifdef MULTI_GRID
SEntityGrid *GetGridFromEntity(const CPhysicalEntity* pent) { return pent->m_pWorld->GetGrid(pent); }
QuatT GetGridTransform(const SEntityGrid *src, const SEntityGrid *dst) { return dst->m_transW.GetInverted() * src->m_transW; }
Vec3 GetGridRelVel(const SEntityGrid *src, const SEntityGrid *dst) { return src->m_velW - dst->m_velW; }
#endif

CPhysicalWorld::CPhysicalEntityIt::CPhysicalEntityIt(CPhysicalWorld* pWorld)
{
  m_pWorld = pWorld;
  m_refs = 0;

  MoveFirst();
}

bool CPhysicalWorld::CPhysicalEntityIt::IsEnd()
{
  return !m_pCurEntity;
}

IPhysicalEntity* CPhysicalWorld::CPhysicalEntityIt::Next()
{
  if (IsEnd())
    return NULL;

	if (m_pCurEntity->m_next)
		m_pCurEntity = m_pCurEntity->m_next;
	else if (m_pCurEntity->m_iSimClass<0)
		m_pCurEntity = 0;
	else {
		int i; for(i=m_pCurEntity->m_iSimClass+1; i<8 && !m_pWorld->m_pTypedEnts[i]; i++);
		m_pCurEntity = i<8 ? m_pWorld->m_pTypedEnts[i] : m_pWorld->m_pHiddenEnts;
	}

  return m_pCurEntity;
}

IPhysicalEntity* CPhysicalWorld::CPhysicalEntityIt::This()
{
  if (IsEnd())
    return NULL;

  return m_pCurEntity;
}

void CPhysicalWorld::CPhysicalEntityIt::MoveFirst()
{
	int i; for(i=0;i<8 && !m_pWorld->m_pTypedEnts[i];i++);
	m_pCurEntity = i<8 ? m_pWorld->m_pTypedEnts[i] : m_pWorld->m_pHiddenEnts;
}

CPhysicalWorld *g_pPhysWorlds[64];
int g_nPhysWorlds;

#if MAX_PHYS_THREADS<=1
threadID g_physThreadId = THREADID_NULL;
#else
TLS_DEFINE(int*, g_pidxPhysThread);

void MarkAsPhysThread() {
	static int g_ibufPhysThread[2] = { 1,0 };
	static int *g_lastPtr = g_ibufPhysThread;
	int *ptr = TLS_GET(int*, g_pidxPhysThread);
	if (ptr!=g_lastPtr) {
		ptr = g_ibufPhysThread+(g_lastPtr-g_ibufPhysThread^1);
		*g_lastPtr = MAX_PHYS_THREADS; *ptr = 0;
		TLS_SET(g_pidxPhysThread, g_lastPtr=ptr);
	}
}
void MarkAsPhysWorkerThread(int *pidx) {
	TLS_SET(g_pidxPhysThread, pidx);
}
#endif


int GetEntityProfileType(CPhysicalEntity *pent)
{
	int i = pent->GetType();
	if (iszero(i-PE_RIGID)|iszero(i-PE_ARTICULATED)|iszero(i-PE_WHEELEDVEHICLE) && pent->GetMassInv()==0.0f)
		return 10;
	if (i==PE_ROPE && pent->m_pOuterEntity)
		return 7+iszero(pent->m_pOuterEntity->GetType()-PE_ARTICULATED);
	else if (i==PE_ARTICULATED)
		i += 9+2-i & -iszero(pent->m_iSimClass-4);
	else if (i==PE_AREA)
		return 11;
	return i-2;
}


CPhysicalWorld::CPhysicalWorld(ILog *pLog) : m_nWorkerThreads(0)
{
	m_pLog = pLog;
	g_pPhysWorlds[g_nPhysWorlds] = this;
	g_nPhysWorlds = min(g_nPhysWorlds+1,(int)(CRY_ARRAY_COUNT(g_pPhysWorlds)));
	m_pEventClient = 0;
	if (g_nPhysWorlds==1)
		g_pLockIntersect = &m_lockCaller[MAX_PHYS_THREADS];

	//////////////////////////////////////////////////////////////////////////
	// Initialize physics event listeners
	//////////////////////////////////////////////////////////////////////////
	m_pPhysicsStreamer = 0;
	int i; for(i=0; i<EVENT_TYPES_NUM; i++)
		m_pEventClients[i][0]=m_pEventClients[i][1] = 0;

	m_vars.nMaxSubsteps = 10;
	for(i=0;i<NSURFACETYPES;i++) {
		m_BouncinessTable[i] = 0;
		m_FrictionTable[i] = 1.2f;
		m_DynFrictionTable[i] = 1.2f/1.5f;
		m_SurfaceFlagsTable[i] = 0;
	}
	m_vars.nMaxStackSizeMC = 8;
	m_vars.maxMassRatioMC = 50.0f;
	m_vars.nMaxMCiters = 6000;
	m_vars.nMinMCiters = 4;
	m_vars.nMaxMCitersHopeless = 6000;
	m_vars.accuracyMC = 0.002f;
	m_vars.accuracyLCPCG = 0.005f;
	m_vars.nMaxContacts = 150;
	m_vars.nMaxPlaneContacts = 8;
	m_vars.nMaxPlaneContactsDistress = 4;
	m_vars.nMaxLCPCGsubiters = 120;
	m_vars.nMaxLCPCGsubitersFinal = 250;
	m_vars.nMaxLCPCGmicroiters = 12000;
	m_vars.nMaxLCPCGmicroitersFinal = 25000;
	m_vars.nMaxLCPCGiters = 5;
	m_vars.minLCPCGimprovement = 0.05f;
	m_vars.nMaxLCPCGFruitlessIters = 4;
	m_vars.accuracyLCPCGnoimprovement = 0.05f;
	m_vars.minSeparationSpeed = 0.02f;
	m_vars.maxwCG = 500.0f;
	m_vars.maxvCG = 500.0f;
	m_vars.maxvUnproj = 10.0f;
	m_vars.maxMCMassRatio = 100.0f;
	m_vars.maxMCVel = 15.0f;
	m_vars.maxLCPCGContacts = 100;
	m_vars.bFlyMode = 0;
	m_vars.iCollisionMode = 0;
	m_vars.bSingleStepMode = 0;
	m_vars.bDoStep = 0;
	m_vars.fixedTimestep = 0;
	m_vars.timeGranularity = 0.0001f;
	m_vars.maxWorldStep = 0.2f;
	m_vars.iDrawHelpers = 0;
	m_vars.drawHelpersOpacity = 1.0f;
	m_vars.iOutOfBounds = raycast_out_of_bounds|get_entities_out_of_bounds;
#if CRY_PLATFORM_MOBILE
	m_vars.nMaxSubsteps = 2;
#else
	m_vars.nMaxSubsteps = 5;
#endif
	m_vars.nMaxSurfaces = NSURFACETYPES;
	m_vars.maxContactGap = 0.01f;
	m_vars.maxContactGapPlayer = 0.01f;
	m_vars.bProhibitUnprojection = 1;//2;
	m_vars.bUseDistanceContacts = 0;
	m_vars.unprojVelScale = 10.0f;
	m_vars.maxUnprojVel = 2.5f;
	m_vars.maxUnprojVelRope = 10.0f;
	m_vars.gravity.Set(0,0,-9.8f);
	m_vars.nGroupDamping = 8;
	m_vars.groupDamping = 0.5f;
	m_vars.nMaxSubstepsLargeGroup = 5;
	m_vars.nBodiesLargeGroup = 30;
	m_vars.bEnforceContacts = 1;//1;
	m_vars.bBreakOnValidation = 0;
	m_vars.bLogActiveObjects = 0;
	m_vars.bMultiplayer = 0;
	m_vars.bProfileEntities = 0;
	m_vars.bProfileFunx = 0;
	m_vars.bProfileGroups = 0;
	m_vars.minBounceSpeed = 1;
	m_vars.nGEBMaxCells = 1024;
	m_vars.maxVel = 100.0f;
	m_vars.maxVelPlayers = 150.0f;
	m_vars.maxVelBones = 10.0f;
	m_vars.bSkipRedundantColldet = 1;
	m_vars.penaltyScale = 0.3f;
	m_vars.maxContactGapSimple = 0.03f;
	m_vars.bLimitSimpleSolverEnergy = 1;
	m_vars.nMaxEntityCells = 300000;
	m_vars.nMaxAreaCells = 128;
	m_vars.nMaxEntityContacts = 256;
	m_vars.tickBreakable = 0.1f;
	m_vars.approxCapsLen = 1.2f;
	m_vars.nMaxApproxCaps = 7;
	m_vars.bCGUnprojVel = 0;
	m_vars.bLogLatticeTension = 0;
	m_vars.nMaxLatticeIters = 100000;
	m_vars.bLogStructureChanges = 1;
	m_vars.bPlayersCanBreak = 0;
	m_vars.bMultithreaded = 0;
	m_vars.breakImpulseScale = 1.0f;
	m_vars.jointGravityStep = 1.0f;
	m_vars.jointDmgAccum = 2.0f;
	m_vars.jointDmgAccumThresh = 0.2f;
	m_vars.massLimitDebris = 1E10f;
	m_vars.maxSplashesPerObj = 0;
	m_vars.splashDist0 = 7.0f; m_vars.minSplashForce0 = 15000.0f;	m_vars.minSplashVel0 = 4.5f;
	m_vars.splashDist1 = 30.0f; m_vars.minSplashForce1 = 150000.0f; m_vars.minSplashVel1 = 10.0f;
	m_vars.lastTimeStep = 0;
	m_vars.numThreads = 2;
	m_vars.physCPU = 4;
	m_vars.physWorkerCPU = 1;
	m_vars.helperOffset.zero();
	m_vars.timeScalePlayers = 1.0f;
	MARK_UNUSED m_vars.flagsColliderDebris;
	m_vars.flagsANDDebris = -1;
	m_vars.bDebugExplosions = 0;
	m_vars.ticksPerSecond = 3000000000U;
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	m_vars.netInterpTime = 0.1f;
	m_vars.netExtrapMaxTime = 0.5f;
	m_vars.netSequenceFrequency = 256;
	m_vars.netDebugDraw = 0;
#else
	m_vars.netMinSnapDist = 0.1f;
	m_vars.netVelSnapMul = 0.1f;
	m_vars.netMinSnapDot = 0.99f;
	m_vars.netAngSnapMul = 0.01f;
	m_vars.netSmoothTime = 5.0f;
#endif
	m_vars.bEntGridUseOBB = 1;
	m_vars.nStartupOverloadChecks = 20;
	m_vars.maxRopeColliderSize =
#if CRY_PLATFORM_MOBILE
	100;
#else
	0;
#endif
	m_vars.breakageMinAxisInertia = 0.01f;
	m_vars.idEntBreakOnAwake = -1000;

	memset(m_grpProfileData, 0, sizeof(m_grpProfileData));
	m_grpProfileData[ 0].pName = "Rigid bodies";
	m_grpProfileData[ 1].pName = "Wheeled vehicles";
	m_grpProfileData[ 2].pName = "Actors";
	m_grpProfileData[ 3].pName = "Physicalized particles";
	m_grpProfileData[ 4].pName = "Ragdolls";
	m_grpProfileData[ 5].pName = "Ropes (standalone)";
	m_grpProfileData[ 6].pName = "Cloth";
	m_grpProfileData[ 7].pName = "Ropes (vegetation)";
	m_grpProfileData[ 8].pName = "Ropes (character)";
	m_grpProfileData[ 9].pName = "Character skeletons";
	m_grpProfileData[10].pName = "Kinematic objects";
	m_grpProfileData[11].pName = "Areas";
	m_grpProfileData[12].pName = "Entities total";   m_grpProfileData[12].nCallsLast=1;
	m_grpProfileData[13].pName = "of which the Solver"; m_grpProfileData[13].nCallsLast=1;
	m_grpProfileData[14].pName = "Queued requests";	 m_grpProfileData[14].nCallsLast=1;
	m_grpProfileData[15].pName = "World step total"; m_grpProfileData[15].nCallsLast=1;
	memset(m_pEntProfileData, 0, sizeof(m_pEntProfileData));

	memset(m_JobProfileInfo, 0, sizeof(m_JobProfileInfo));
	m_JobProfileInfo[0].pName = "rb_geomintersect";
	m_JobProfileInfo[1].pName = "solver      ";
	m_JobProfileInfo[2].pName = "cloth       ";
	m_JobProfileInfo[3].pName = "rope        ";
	m_JobProfileInfo[4].pName = "featherstone";
	m_JobProfileInfo[5].pName = "rb_primintersect";

	m_pFirstEventChunk = m_pCurEventChunk = (EventChunk*)(new char[sizeof(EventChunk)+EVENT_CHUNK_SZ]);
	m_pRwiHitsTail = (m_pRwiHitsHead = (m_pRwiHitsPool=new ray_hit[257])+1)+255;

	Init();
}

CPhysicalWorld::~CPhysicalWorld()
{
	Shutdown();

	//////////////////////////////////////////////////////////////////////////
	// Deinitialize physics event listeners
	//////////////////////////////////////////////////////////////////////////
	for(int i=0; i<EVENT_TYPES_NUM; i++) {
		EventClient *pClient,*pNextClient;
		for (int bLogged=0;bLogged<2;bLogged++) {
			for(pClient=m_pEventClients[i][bLogged]; pClient; pClient=pNextClient) {
				pNextClient=pClient->next; delete pClient;
			}
			m_pEventClients[i][bLogged] = 0;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if (g_StaticPhysicalEntity.m_pWorld==this)
		g_StaticPhysicalEntity.m_pWorld = 0;		
	memset(g_StaticPhysicalEntity.m_pUsedParts = new int[MAX_PHYS_THREADS+1][16], 0, sizeof(*g_StaticPhysicalEntity.m_pUsedParts));
	int i;
	for(i=0; i<g_nPhysWorlds && g_pPhysWorlds[i]!=this; i++);
	if (i<g_nPhysWorlds)
		g_nPhysWorlds--;
	for(; i<g_nPhysWorlds; i++) g_pPhysWorlds[i] = g_pPhysWorlds[i+1];
	if (!g_nPhysWorlds)
		g_pPhysWorlds[0] = 0;
	m_nPumpLoggedEventsHits = 0;

	delete[] m_pRwiHitsPool;
	delete[] ((char*)m_pFirstEventChunk);
}

QuatT SEntityGrid::GetUpdatedTransW() 
{
	if (!m_host)
		m_transW = m_transInHost;
	else {
		QuatT transHost(m_host->m_pNewCoords->q, m_host->m_pNewCoords->pos);
		m_transW = m_host->m_pWorld->GetGrid(m_host)->GetUpdatedTransW() * transHost * m_transInHost;
	}
	return m_transW;
}

Vec3 SEntityGrid::GetUpdatedVelW()
{
	if (m_host) {
		pe_status_dynamics sd;
		m_host->GetStatus(&sd);
		SEntityGrid *parent = m_host->m_pWorld->GetGrid(m_host);
		m_velW = parent->GetUpdatedVelW() + parent->m_transW.q*sd.v;
		m_v = sd.v; m_w = sd.w;	
		m_com = sd.centerOfMass-m_host->m_pos;
	} else
		m_velW.zero();
	return m_velW;
}

void SEntityGrid::Init()
{
	m_next=m_prev = nullptr;
	cells = 0; 
	zGran = 1.0f/16; rzGran = 16;
	pPODcells = &(pDummyPODcell=&dummyPODcell); dummyPODcell.lifeTime = 1E10f;
	dummyPODcell.zlim.set(1E10f,-1E10f);
	log2PODscale = 0;
	iActivePODCell0 = -1;	bHasPODGrid = 0;
	dummyPODcell.zlim[0]=1E10f; dummyPODcell.zlim[1]=1E10f;
	m_host = nullptr;
	m_transInHost.SetIdentity();
	m_velW.zero();
	m_v.zero(); m_w.zero(); m_com.zero();
	m_dvSleep2 = sqr(3);
}

void CPhysicalWorld::Init()
{
	InitGeoman();
	m_pTmpEntList=0; m_pTmpEntList1=0; m_pTmpEntList2=0; m_pGroupMass=0; m_pMassList = 0; m_pGroupIds = 0; m_pGroupNums = 0;
	m_nEnts = 0; m_nEntsAlloc = 0; m_bEntityCountReserved = 0;
	m_entgrid.Init();
	m_gthunks = 0;
	m_thunkPoolSz = 0;
	m_timePhysics = m_timeSurplus = 0;
	m_timeSnapshot[0]=m_timeSnapshot[1]=m_timeSnapshot[2]=m_timeSnapshot[3] = 0;
	m_iTimeSnapshot[0]=m_iTimeSnapshot[1]=m_iTimeSnapshot[2]=m_iTimeSnapshot[3] = 0;
	m_iTimePhysics = 0;
	int i; for(i=0;i<8;i++) { m_pTypedEnts[i]=m_pTypedEntsPerm[i]=0; m_updateTimes[i]=0; }
	m_pHiddenEnts = 0;
	for(i=0;i<10;i++) m_nTypeEnts[i]=0;
	for(i=0;i<=MAX_PHYS_THREADS;i++) {
		m_pHeightfield[i] = 0;
		m_lockCaller[i] = 0;
	}
	memset(m_threadData, 0, sizeof(m_threadData));

	m_iNextId = 1;
	m_iNextIdDown=m_lastExtId=m_nExtIds = 0;
	m_pEntsById = 0;
	m_nIdsAlloc = 0;
	m_nExplVictims = m_nExplVictimsAlloc = 0;
	m_pPlaceholders = 0; m_pPlaceholderMap = 0;
	m_nPlaceholders = m_nPlaceholderChunks = 0;
	m_iLastPlaceholder = -1;
	m_nProfiledEnts = 0;
	m_iSubstep = 0;
	m_bWorldStep = 0;
	m_nDynamicEntitiesDeleted = 0;
	m_nQueueSlots = m_nQueueSlotsAlloc = 0;
	m_nQueueSlotsAux = m_nQueueSlotsAllocAux = 0;
	m_nQueueSlotSize = m_nQueueSlotSizeAux = QUEUE_SLOT_SZ;
	m_pQueueSlots = 0; m_pQueueSlotsAux = 0;

	m_pGlobalArea = 0; m_nAreas=m_nBigAreas = 0; m_pDeletedAreas = 0;
	memset(m_pTypedAreas, 0, sizeof(m_pTypedAreas));
	m_numNonWaterAreas = m_numGravityAreas = m_numAreaTriggers = 0;
	m_pActiveArea = 0;
	m_iLastLogPump = 0;
	m_pFreeContact = m_pLastFreeContact = CONTACT_END(m_pFreeContact);
	m_nFreeContacts = m_nContactsAlloc = 0;
	m_pFreeEntPart = m_pLastFreeEntPart = CONTACT_END(m_pFreeEntPart);
	m_nFreeEntParts = m_nEntPartsAlloc = 0;
	m_pExpl = 0;
	m_nExpl = m_nExplAlloc = 0; m_idExpl = 0;
	m_pDeformingEnts = 0; m_nDeformingEnts = m_nDeformingEntsAlloc = 0;
	m_pRenderer = 0;
	m_lockDeformingEntsList = 0;
	m_lockAreas = 0; m_lockActiveAreas = 0;
	m_matWater = -1; m_bCheckWaterHits = 0;
	if (!g_StaticPhysicalEntity.m_pWorld)
		g_StaticPhysicalEntity.m_pWorld = this;
	g_StaticPhysicalEntity.m_id = -2;
	SetGrid(&g_StaticPhysicalEntity, &m_entgrid);
	memset(&CPhysicalEntity::m_defpart,0,sizeof(geom));
	CPhysicalEntity::m_defpart.q.SetIdentity();
	CPhysicalEntity::m_defpart.scale = 1.0f;
	CPhysicalEntity::m_defpart.pNewCoords = (coord_block_BBox*)&CPhysicalEntity::m_defpart.pos;
	m_rwiQueueHead=-1; m_rwiQueueTail=-64; m_rwiQueueSz=m_rwiQueueAlloc = 0;
	m_rwiQueue = 0; m_lockRwiQueue = 0;
	m_pRwiHitsTail = (m_pRwiHitsHead = m_pRwiHitsPool+1)+255;
	memset(m_pRwiHitsPool, 0, 257*sizeof(ray_hit));
	for(i=0;i<255;i++) m_pRwiHitsHead[i].next = m_pRwiHitsHead+i+1;
	m_pRwiHitsHead[i].next = m_pRwiHitsHead; m_rwiPoolEmpty = 1;
	m_rwiHitsPoolSize = 256; m_lockRwiHitsPool = 0; m_lockTPR = 0;
	m_pwiQueueHead=-1; m_pwiQueueTail=0; m_pwiQueueSz=m_pwiQueueAlloc = 0;
	m_pwiQueue = 0; m_lockPwiQueue = 0;
	m_breakQueueHead=-1; m_breakQueueTail=0; m_breakQueueSz=m_breakQueueAlloc = 0;
	m_breakQueue = 0; m_lockBreakQueue = 0;
	m_pWaterMan = 0;
	m_idStep = 0;
	m_nEntListAllocs = 0;
	m_curGroupMass = 0;
	m_iLastPODUpdate = 1;
	m_nProfileFunx=m_nProfileFunxAlloc = 0; m_pFuncProfileData = 0;
	m_posViewer.zero();

	for(i=0;i<=MAX_PHYS_THREADS;i++) {
#ifndef _RELEASE
		m_nGEA[i] = 0;
#endif
		m_prevGEABBox[i][0].Set(1E10f,1E10f,1E10f); m_prevGEABBox[i][1].zero();
		m_prevGEAobjtypes[i]=m_nprevGEAEnts[i] = 0;
		m_BBoxPlayerGroup[i][0]=m_BBoxPlayerGroup[i][1] = Vec3(1e10f);
	}

	for(i=0; i<EVENT_TYPES_NUM; i++) {
		m_pFreeEvents[i] = 0;
		m_nEvents[i] = 0;
	}
	m_pEventFirst = m_pEventLast = 0;
	m_pCurEventChunk = m_pFirstEventChunk;
	m_szCurEventChunk = 0;
	m_pFirstEventChunk->next = 0;

	m_pEntBeingDeleted = 0;
	m_bGridThunksChanged = 0;
	m_bUpdateOnlyFlagged = 0;
	m_lockStep=0;	m_lockQueue=0; m_lockList=0; m_lockEventsQueue=0;	m_lockEntIdList=0; m_lockEventClients=0;
	m_lockGrid = 0; m_lockPODGrid = 0; m_lockFuncProfiler = 0; m_lockEntProfiler = 0;
	m_lockContacts = 0;	m_lockEntParts = 0;
	m_idThread = m_idPODThread = threadID(THREADID_NULL);
	m_nOnDemandListFailures = 0; m_iLastPODUpdate = 1;
	m_lockNextEntityGroup = 0; m_lockMovedEntsList = 0;
	m_lockPlayerGroups = 0;
	m_lockAuxStepEnt = 0;
	m_lockWaterMan = 0;
	
	m_bMassDestruction = 0;
	m_nSlowFrames = 0;
	m_oldThunks = 0; m_lockOldThunks = 0;
	m_oldEntParts = 0;
}

void CPhysicalWorld::InitGThunksPool()
{
	// Round up to multiples of 64k
	const int bytes = 32768*sizeof(pe_gridthunk);
	const int numPages = (bytes+(64*1024-1)) >> 16;
	const int nStartingThunks = (numPages<<16)/sizeof(pe_gridthunk);
	AllocGThunksPool(nStartingThunks);
}

static pe_gridthunk* AllocateThunks(int& nNewSize)
{
	// Round up to multiples of 64k
	size_t bytes = sizeof(pe_gridthunk)*nNewSize;
	size_t numPages = (bytes + 64*1024-1) >> 16;
	nNewSize = (numPages<<16)/sizeof(pe_gridthunk);
	void* memory = malloc(numPages<<16);
	return new (memory) pe_gridthunk[nNewSize];
}

static void FreeThunks(pe_gridthunk* thunks)
{
// NB: Dont care about the destructors
	free(thunks);
}

//////////////////////////////////////////////////////////////////////////
void CPhysicalWorld::AllocGThunksPool( int nNewSize )
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Physics, 0, "Physical Grid Pool");

	if (nNewSize==m_thunkPoolSz)
		return;
	if (nNewSize<m_thunkPoolSz)
		DeallocGThunksPool();

	pe_gridthunk *prevthunks = m_gthunks, *gthunks;
	gthunks = AllocateThunks(nNewSize);

	if (prevthunks)
	{
		// Reallocate
		NO_BUFFER_OVERRUN
		memcpy(gthunks, prevthunks, m_thunkPoolSz*sizeof(pe_gridthunk));
		memset(gthunks+m_thunkPoolSz, 0, (nNewSize-m_thunkPoolSz)*sizeof(pe_gridthunk));
		int ithunk;
		for(ithunk=m_thunkPoolSz; ithunk<nNewSize-1; ithunk++)
			gthunks[ithunk].inextOwned = ithunk+1;
		gthunks[ithunk].inextOwned = 0;
		m_iFreeGThunk0=m_thunkPoolSz;
		m_thunkPoolSz=nNewSize;
		m_gthunks = gthunks;
		if (m_bWorldStep) {
			WriteLock lock(m_lockOldThunks);
			*(pe_gridthunk**)prevthunks = m_oldThunks;
			m_oldThunks = prevthunks;
		}	else
			FreeThunks(prevthunks);
	}
	else
	{
		// First Time
		m_thunkPoolSz = nNewSize;
		memset(gthunks, 0, m_thunkPoolSz*sizeof(pe_gridthunk));
		int i; for(i=0;i<m_thunkPoolSz-1;i++) gthunks[i].inextOwned = i+1;
		gthunks[i].inextOwned = 0;
		gthunks[0].inext=gthunks[0].iprev = 0;
		m_iFreeGThunk0=1;
		m_gthunks = gthunks;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhysicalWorld::DeallocGThunksPool()
{
	if (m_gthunks)
		FreeThunks(m_gthunks);
	m_gthunks=0;
	m_thunkPoolSz=m_iFreeGThunk0=0;
}

//////////////////////////////////////////////////////////////////////////
void CPhysicalWorld::Cleanup()
{
	Shutdown(1);
	Init();
}


void CPhysicalWorld::Shutdown(int bDeleteGeometries)
{
	WriteLock lock(m_lockStep);

	for(int i=m_nWorkerThreads-1;i>=0;i--) {
		m_threads[i]->bStop=1,m_threadStart[i].Set(),m_threadDone[i].Wait();
		delete m_threads[i]; m_threads[i] = NULL;
	}
	int i; CPhysicalEntity *pent,*pent_next;
	m_bMassDestruction = 1;
	for(i=0;i<8;i++) {
		for(pent=m_pTypedEnts[i]; pent; pent=pent_next)
		{ pent_next=pent->m_next; pent->Delete(); }
		m_pTypedEnts[i] = 0; m_pTypedEntsPerm[i] = 0;
	}
	for(pent=m_pHiddenEnts; pent; pent=pent_next)
	{ pent_next=pent->m_next; pent->Delete(); }
	m_pHiddenEnts = 0;
	CPhysArea *pArea,*pNextArea;
	for(pArea=m_pDeletedAreas; pArea; pArea=pNextArea) {
		pNextArea=pArea->m_next; delete pArea;
	}
	m_pDeletedAreas = 0;
	delete[] m_pDeformingEnts; m_pDeformingEnts = 0;
	m_nEnts = m_nEntsAlloc = 0; m_bEntityCountReserved = 0;
	for(i=0;i<m_nPlaceholderChunks;i++) if (m_pPlaceholders[i])
		delete[] m_pPlaceholders[i];
	if (m_pPlaceholders) delete[] m_pPlaceholders; m_pPlaceholders = 0;
	if (m_pPlaceholderMap) delete[] m_pPlaceholderMap; m_pPlaceholderMap = 0;
	m_nPlaceholderChunks = m_nPlaceholders = 0;
	m_iLastPlaceholder = -1;
	m_entgrid.Free();
	DeallocGThunksPool();
	if (m_pTmpEntList) delete[] m_pTmpEntList; m_pTmpEntList = 0;
	if (m_pTmpEntList1) delete[] m_pTmpEntList1; m_pTmpEntList1 = 0;
	if (m_pTmpEntList2) delete[] m_pTmpEntList2; m_pTmpEntList2 = 0;
	if (m_pGroupMass) delete[] m_pGroupMass; m_pGroupMass = 0;
	if (m_pMassList) delete[] m_pMassList; m_pMassList = 0;
	if (m_pGroupIds) delete[] m_pGroupIds; m_pGroupIds = 0;
	if (m_pGroupNums) delete[] m_pGroupNums; m_pGroupNums = 0;
	if (m_pEntsById) delete[] m_pEntsById; m_pEntsById = 0;	m_nIdsAlloc = 0;
	m_cubeMapStatic.Free();
	m_cubeMapDynamic.Free();
	if (m_nExplVictimsAlloc) {
		delete[] m_pExplVictims; m_pExplVictims = 0;
		delete[] m_pExplVictimsFrac; m_pExplVictimsFrac = 0;
		delete[] m_pExplVictimsImp; m_pExplVictimsImp = 0;
	}
	for(i=1;i<MAX_PHYS_THREADS;i++)
		delete[] m_threadData[i].pTmpEntList;
	for(i=0;i<=MAX_PHYS_THREADS;i++) {
		m_threadData[i].szList=0; m_threadData[i].pTmpEntList=0;
	}

	m_pEventFirst = m_pEventLast = 0;
	EventChunk *pChunk,*pChunkNext;
	for(pChunk=m_pFirstEventChunk->next;pChunk;pChunk=pChunkNext) {
		pChunkNext=pChunk->next;
		delete[] ((char*)pChunk);
	}
	m_szCurEventChunk = 0; m_pCurEventChunk = m_pFirstEventChunk;
	memset(m_pFreeEvents, 0, sizeof(m_pFreeEvents));
	memset(m_nEvents, 0, sizeof(m_nEvents));

	for(i=0; i<m_nQueueSlotsAlloc; i++)
		delete[] m_pQueueSlots[i];
	delete [] m_pQueueSlots; m_pQueueSlots = 0;
	m_nQueueSlots = m_nQueueSlotsAlloc = 0;
	m_nQueueSlotSize = m_nQueueSlotSizeAux = QUEUE_SLOT_SZ;
	m_pQueueSlots = 0;
	for(i=0; i<m_nQueueSlotsAllocAux; i++)
		delete[] m_pQueueSlotsAux[i];
	delete [] m_pQueueSlotsAux;
	m_nQueueSlotsAux = m_nQueueSlotsAllocAux = 0;
	m_pQueueSlotsAux = 0;

	if (m_pExpl) {
		for(i=0;i<m_nExpl;i++) m_pExpl[i].pGeom->Release();
		delete[] m_pExpl; m_pExpl = 0;
	}
	m_nExpl = m_nExplAlloc = 0; m_idExpl = 0;
	if (m_rwiQueue)	{
		delete[] m_rwiQueue; m_rwiQueue = 0;
	}
	m_rwiQueueHead=-1; m_rwiQueueTail=-64; m_rwiQueueSz=m_rwiQueueAlloc = 0;
	m_rwiQueue = 0; m_lockRwiQueue = 0;
	if (m_pRwiHitsHead) {
		ray_hit *phit,*pchunk;
		for(phit=(pchunk=m_pRwiHitsPool)+1; phit->next!=m_pRwiHitsPool+1; phit=phit->next)
			if (!phit->next[-1].next) { // means phit->next[-1] is a chunk header
				pchunk->next = phit->next-1; pchunk = phit->next-1;
			}
		for(pchunk=m_pRwiHitsPool->next; pchunk; pchunk=phit) {
			phit=pchunk->next; delete[] pchunk;
		}
		m_pRwiHitsTail = m_pRwiHitsHead;
		m_rwiHitsPoolSize = 0;
	}
	if (m_pwiQueue)	{
		delete[] m_pwiQueue; m_pwiQueue = 0;
	}
	m_pwiQueueHead=-1; m_pwiQueueTail=0; m_pwiQueueSz=m_pwiQueueAlloc = 0;
	m_pwiQueue = 0; m_lockPwiQueue = 0;
	if (m_breakQueue)	{
		delete[] m_breakQueue; m_breakQueue = 0;
	}
	m_breakQueueHead=-1; m_breakQueueTail=0; m_breakQueueSz=m_breakQueueAlloc = 0;
	m_breakQueue = 0; m_lockBreakQueue = 0;
	DestroyWaterManager();

	if (bDeleteGeometries) {
		SetHeightfieldData(0);
		ShutDownGeoman();

		delete m_pGlobalArea;
		m_pGlobalArea = NULL;
	}
	m_bMassDestruction = 0;

	for(i=0;i<=MAX_PHYS_THREADS;i++) {
		delete[] m_threadData[i].pTmpPartBVList, m_threadData[i].pTmpPartBVList=0;
		m_threadData[i].szTmpPartBVList=m_threadData[i].nTmpPartBVs=0;
		m_threadData[i].pTmpPartBVListOwner=0;
		delete[] m_threadData[i].pTmpPrecompEntsLE, m_threadData[i].pTmpPrecompEntsLE=0;
		delete[] m_threadData[i].pTmpPrecompPartsLE, m_threadData[i].pTmpPrecompPartsLE=0;
		delete[] m_threadData[i].pTmpNoResponseContactLE, m_threadData[i].pTmpNoResponseContactLE=0;
		m_threadData[i].nPrecompPartsAllocLE=m_threadData[i].nPrecompEntsAllocLE=0;
		m_threadData[i].nNoResponseAllocLE=0;
	}

	FreeObjPool(m_pFreeContact,m_nFreeContacts,m_nContactsAlloc);
	FreeObjPool(m_pFreeEntPart,m_nFreeEntParts,m_nEntPartsAlloc);

	CleanupContactSolvers();
	FlushOldThunks();

	delete [] m_pFuncProfileData;
	m_pFuncProfileData = NULL;
	m_nProfileFunx=m_nProfileFunxAlloc = 0;

	CTriMesh::CleanupGlobalLoadState();
	CPhysicalEntity::CleanupGlobalState();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////


IPhysicalEntity *CPhysicalWorld::SetHeightfieldData(const heightfield *phf, int *pMatMapping,int nMats)
{
	int iCaller;
	CPhysicalEntity *pHF;
	if (!phf) {
		for(iCaller=0;iCaller<=MAX_PHYS_THREADS;iCaller++)
			if (pHF = m_pHeightfield[iCaller]) {
				m_pHeightfield[iCaller] = 0;
				assert(pHF->m_parts[0].pPhysGeom); // analyzer:(
				pHF->m_parts[0].pPhysGeom->pGeom->Release();
				delete pHF->m_parts[0].pPhysGeom;
				pHF->m_parts[0].pPhysGeom = 0;
				if (pHF->m_parts[0].pMatMapping)
					delete[] pHF->m_parts[0].pMatMapping;
				pHF->Delete();
			}
		return 0;
	}
	for(iCaller=0; iCaller<=MAX_PHYS_THREADS; iCaller++) {
		CGeometry *pGeom = (CGeometry*)CreatePrimitive(heightfield::type, phf);
		pHF=m_pHeightfield[iCaller]; m_pHeightfield[iCaller]=0;
		if (pHF)	{
			pHF->m_parts[0].pPhysGeom->pGeom->Release();
			if (pHF->m_parts[0].pMatMapping)
				delete[] pHF->m_parts[0].pMatMapping;
		} else {
			pHF = CPhysicalEntity::Create<CPhysicalEntity>(this, NULL);
			SetGrid(pHF, &m_entgrid);
			pHF->m_parts = AllocEntityPart(); pHF->m_nPartsAlloc = 1;
			pHF->m_parts[0].pPhysGeom = pHF->m_parts[0].pPhysGeomProxy = new phys_geometry;
			memset(pHF->m_parts[0].pPhysGeom, 0, sizeof(phys_geometry));
			pHF->m_parts[0].id = 0;
			pHF->m_parts[0].scale = 1.0;
			pHF->m_parts[0].mass = 0;
			pHF->m_parts[0].flags = geom_collides;
			pHF->m_parts[0].flagsCollider = geom_colltype0;
			pHF->m_parts[0].minContactDist = phf->step.x;
			pHF->m_parts[0].idmatBreakable = -1;
			pHF->m_parts[0].pLattice = 0;
			pHF->m_parts[0].nMats = 0;
			pHF->m_nParts = 1;
			pHF->m_id = -1;
			pHF->m_collisionClass.type |= collision_class_terrain;
		}
		if (pMatMapping)
			memcpy(pHF->m_parts[0].pMatMapping = new int[nMats], pMatMapping,
				(pHF->m_parts[0].nMats=nMats)*sizeof(int));
		else
			pHF->m_parts[0].pMatMapping = 0;
		m_HeightfieldBasis = phf->Basis;
		m_HeightfieldOrigin = phf->origin;
		pHF->m_parts[0].pPhysGeom->pGeom = pGeom;
		pHF->m_parts[0].pos = phf->origin;
		pHF->m_parts[0].q = !quaternionf(phf->Basis);
		pHF->m_parts[0].BBox[0].zero();
		pHF->m_parts[0].BBox[1].zero();
		m_pHeightfield[iCaller] = pHF;
	}
	if (!m_nEnts)
		ReserveEntityCount(1);
	return m_pHeightfield[0];
}

IPhysicalEntity *CPhysicalWorld::GetHeightfieldData(heightfield *phf)
{
	if (m_pHeightfield[0]) {
		m_pHeightfield[0]->m_parts[0].pPhysGeom->pGeom->GetPrimitive(0,phf);
		phf->Basis = m_HeightfieldBasis;
		phf->origin = m_HeightfieldOrigin;
	}
	return m_pHeightfield[0];
}

void CPhysicalWorld::SetHeightfieldMatMapping(int *pMatMapping, int nMats)
{
	if(!pMatMapping)
		return;
	
	for(int iCaller=0; iCaller<=MAX_PHYS_THREADS; iCaller++)
	{
		CPhysicalEntity *pHF = m_pHeightfield[iCaller];
		if(pHF && pHF->m_parts && pHF->m_parts[0].pMatMapping)
		{
			memcpy(pHF->m_parts[0].pMatMapping, pMatMapping, nMats*sizeof(int));
			pHF->m_parts[0].nMats = nMats;
		}
	}
}

void SEntityGrid::Setup(int axisz, Vec3 org, int nx,int ny, float stepx,float stepy, int _log2PODscale, int _bCyclic)
{
	nx = min(1024,nx);
	ny = min(1024,ny);
	iup = axisz;

	size.set(nx,ny);
	stride.set(1,nx);
	step.set(stepx,stepy);
	stepr.set(1.0f/stepx,1.0f/stepy);
	origin = org;
	Basis.SetIdentity();
	AllocateGrid(cells, size);
	log2PODscale = _log2PODscale;
	PODstride.set(1,ny>>3+log2PODscale);
	bCyclic = _bCyclic;
}

IPhysicalEntity* CPhysicalWorld::SetupEntityGrid(int axisz, Vec3 org, int nx,int ny, float stepx,float stepy, int log2PODscale, int bCyclic, IPhysicalEntity* pHost, const QuatT& posInHost)
{
	WriteLock lockGrid(m_lockGrid);
	SEntityGrid *pgrid = &m_entgrid;
	if (pHost && !(pgrid = GetHostedGrid((CPhysicalEntity*)pHost))) {
		pgrid = new SEntityGrid;
		pgrid->Init();
		memset(m_gthunks+(pgrid->m_iGThunk0 = GetFreeThunk()), 0, sizeof(pe_gridthunk));
		if (pgrid->m_next = m_entgrid.m_next) m_entgrid.m_next->m_prev = pgrid; 
		pgrid->m_prev = &m_entgrid;	m_entgrid.m_next = pgrid;
		pe_params_outer_entity poe; poe.pOuterEntity = pgrid;
		pHost->SetParams(&poe);
		(pgrid->m_host = (CPhysicalEntity*)pHost)->AddRef();
		pe_params_foreign_data pfd; pHost->GetParams(&pfd);
		pgrid->m_pForeignData = pfd.pForeignData;
		pgrid->m_iForeignData = pfd.iForeignData;
		pgrid->m_iForeignFlags = pfd.iForeignFlags;
	}
	SetGrid(pgrid,pgrid);
	pgrid->m_pWorld = this;
	pgrid->m_transInHost = posInHost;
	pgrid->GetUpdatedTransW();
	pgrid->GetUpdatedVelW();
	
	if (pgrid->cells) {
		int i;
		if (m_pEntsById) for(i=0;i<m_iNextId;i++) if (m_pEntsById[i] && GetGrid(m_pEntsById[i])==pgrid) {
			DetachEntityGridThunks(m_pEntsById[i]);
			if (!m_pEntsById[i]->m_pEntBuddy || m_pEntsById[i]->m_pEntBuddy==m_pEntsById[i]) {
				CPhysicalEntity *pent = (CPhysicalEntity*)m_pEntsById[i];
				for(int j=0;j<pent->m_nParts;j++) if (pent->m_parts[j].pPlaceholder)
					DetachEntityGridThunks(pent->m_parts[j].pPlaceholder);
			}
		}
		for(CPhysArea *pArea=m_pGlobalArea; pArea; pArea=pArea->m_next)	if (GetGrid(pArea)==pgrid)
			DetachEntityGridThunks(pArea);
		for	(i=pgrid->size.x*pgrid->size.y;i>=0;i--) if (pgrid->cells[i])
			m_gthunks[pgrid->cells[i]].iprev = 0;
		DeallocateGrid(pgrid->cells, pgrid->size);
	}

	if (!pHost) {
		InitGThunksPool();

		//Before changing m_entpgrid->size, delete the on demand grid so
		//we don't treat the old grid as valid and write/delete out of bounds.
		DeactivateOnDemandGrid();
	}

	pgrid->Setup(axisz,org,nx,ny,stepx,stepy,log2PODscale,bCyclic);
	if (bCyclic)
		m_vars.iOutOfBounds = raycast_out_of_bounds|get_entities_out_of_bounds;
	return pgrid;
}

int SEntityGrid::SetParams(pe_params* _params, int bThreadSafe)
{
	if (_params->type==pe_params_outer_entity::type_id) {
		pe_params_outer_entity *params = (pe_params_outer_entity*)_params;
		if (!is_unused(params->pOuterEntity) && params->pOuterEntity!=(IPhysicalEntity*)m_host) {
			if (m_host) m_host->Release();
			if (m_host=(CPhysicalEntity*)params->pOuterEntity) m_host->AddRef();
			GetUpdatedTransW();
			GetUpdatedVelW();
		}
		return 1;
	}
	if (_params->type==pe_params_pos::type_id) {
		pe_params_pos *params = (pe_params_pos*)_params;
		QuatT trans = m_transInHost;
		if (!is_unused(params->pos)) trans.t = params->pos;
		if (!is_unused(params->q)) trans.q = params->q;
		if (!is_unused(params->pMtx3x3) && params->pMtx3x3) trans.q = Quat(*params->pMtx3x3);
		if (!is_unused(params->pMtx3x4) && params->pMtx3x4) {
			Matrix33 mtx = Matrix33(*params->pMtx3x4);
			trans.q = Quat(mtx * Diag33(mtx.GetColumn0().len(),mtx.GetColumn1().len(),mtx.GetColumn2().len()).invert());
			trans.t = params->pMtx3x4->GetTranslation();
		}
		m_transInHost = trans;
		GetUpdatedTransW();
		return 1;
	}
	if (_params->type==pe_params_foreign_data::type_id) {
		pe_params_foreign_data *params = (pe_params_foreign_data*)_params;
		if (!is_unused(params->pForeignData)) m_pForeignData = params->pForeignData;
		if (!is_unused(params->iForeignData)) m_iForeignData = params->iForeignData;
		if (!is_unused(params->iForeignFlags)) m_iForeignFlags = params->iForeignFlags;
		return 1;
	}
	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		if (!is_unused(params->minEnergy)) m_dvSleep2 = params->minEnergy;
		return 1;
	}
	return 0;
}

int SEntityGrid::GetParams(pe_params* _params) const
{
	if (_params->type==pe_params_outer_entity::type_id) {
		((pe_params_outer_entity*)_params)->pOuterEntity = m_host;
		return 1;
	}
	if (_params->type==pe_params_pos::type_id) {
		pe_params_pos *params = (pe_params_pos*)_params;
		params->pos = m_transInHost.t;
		params->q = m_transInHost.q;
		params->scale = 1.0f;
		params->iSimClass = m_host ? m_host->m_iSimClass : m_iSimClass;
		return 1;
	}
	if (_params->type==pe_params_foreign_data::type_id) {
		pe_params_foreign_data *params = (pe_params_foreign_data*)_params;
		params->pForeignData = m_pForeignData;
		params->iForeignData = m_iForeignData;
		params->iForeignFlags = m_iForeignFlags;
		return 1;
	}
	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		params->minEnergy = m_dvSleep2;
		return 1;
	}
	return 0;
}


int SEntityGrid::Release() 
{ 
	int res = CryInterlockedDecrement(&m_refCount); 
	if (res<=0 && m_pWorld)
		m_pWorld->DestroyPhysicalEntity(this);
	return res;
}

SEntityGrid* CPhysicalWorld::DestroyGrid(SEntityGrid *pgrid)
{
	if (pgrid==&m_entgrid)
		return nullptr;
	WriteLock lock(m_lockGrid);
	SEntityGrid *next = pgrid->m_next;
	for(int icell=0; icell<=pgrid->size.x*pgrid->size.y; icell++)
		while (int ithunk=pgrid->cells[icell]) {
			CPhysicalPlaceholder *pent = m_gthunks[ithunk].pent;
			DetachEntityGridThunks(pent);
			pent->m_ig[0].x=pent->m_ig[1].x=pent->m_ig[0].y=pent->m_ig[1].y = GRID_REG_PENDING;
			SetGrid(pent, &m_entgrid);
		}
	m_gthunks[pgrid->m_iGThunk0].inextOwned = m_iFreeGThunk0;
	m_iFreeGThunk0 = pgrid->m_iGThunk0;
	pgrid->Free();
	delete pgrid;
	return next;
}


void CPhysicalWorld::DeactivateOnDemandGrid()
{
	SEntityGrid &grid = m_entgrid;
	grid.DeactivateOnDemand();
	for(int i=0;i<8;i++) for(CPhysicalEntity *pent=m_pTypedEnts[i]; pent; pent=pent->m_next) if (GetGrid(pent)==&grid) {
		pent->m_nRefCount-=pent->m_nRefCountPOD; pent->m_nRefCountPOD=0;
	}
}

void SEntityGrid::DeactivateOnDemand()
{
	if (bHasPODGrid) {
		int i;
		pPODcells--;
		for(i=size.x*size.y>>(3+log2PODscale)*2;i>=0;i--) if (pPODcells[i])
			delete[] pPODcells[i];
		delete[] pPODcells;
		pPODcells = &pDummyPODcell; dummyPODcell.lifeTime = 1E10f;
		iActivePODCell0 = -1;	bHasPODGrid = 0;
	}
}

void CPhysicalWorld::RegisterBBoxInPODGrid(const Vec3 *BBox)
{
	WriteLock lock(m_lockPODGrid);
	m_entgrid.RegisterBBoxInPODGrid(BBox, m_pPhysicsStreamer);
}

void SEntityGrid::RegisterBBoxInPODGrid(const Vec3 *BBox, IPhysicsStreamer *pStreamer)
{
	int i,ix,iy,igx[2],igy[2],imask;
	
	if (!bHasPODGrid) {
		if ((size.x|size.y) & 7)
			return;
		i = (size.x*size.y>>(3+log2PODscale)*2)+1;
		memset(pPODcells = new pe_PODcell*[i], 0, i*sizeof(pPODcells[0]));
		pPODcells++;
		iActivePODCell0 = -1;	bHasPODGrid = 1;
	}

	Vec3 gBBox[2]; BBoxToGrid(BBox[0],BBox[1],gBBox);
	for(i=0;i<2;i++) {
		igx[i] = max(-1,min(size.x,float2int(gBBox[i].x*stepr.x-0.5f)));
		igy[i] = max(-1,min(size.y,float2int(gBBox[i].y*stepr.y-0.5f)));
		igx[i] >>= log2PODscale; igy[i] >>= log2PODscale;
	}
	for(ix=igx[0];ix<=igx[1];ix++) for(iy=igy[0];iy<=igy[1];iy++) {
		imask = ~negmask(ix) & ~negmask(iy) & negmask(ix - (size.x>>log2PODscale)) & negmask(iy - (size.y>>log2PODscale));   // (x>=0 && x<m_entgrid.size.x) ? 0xffffffff : 0;
		i = (ix>>3)*PODstride.x + (iy>>3)*PODstride.y;
		i = i + ((-1-i) & ~imask);
		if (!pPODcells[i]) {
			memset(pPODcells[i] = new pe_PODcell[64], 0, sizeof(pe_PODcell)*64);
			for(int j=0;j<64;j++) pPODcells[i][j].zlim.set(1000.0f,-1000.0f);
			if (!imask) pPODcells[i][0].lifeTime = 1e10f;
		}
		pe_PODcell *pPODcell = pPODcells[i] + ((ix&7)+(iy&7)*8 & imask);
		pPODcell->zlim[0] = min(pPODcell->zlim[0], gBBox[0].z);
		pPODcell->zlim[1] = max(pPODcell->zlim[1], gBBox[1].z);
		pPODcell->nObjects++;
		if (pPODcell->lifeTime>0 && pPODcell->lifeTime<1e10f) {
			MarkAsPODThread(m_pWorld);
			++m_pWorld->m_iLastPODUpdate;
			Vec3 center,sz; GetPODGridCellBBox(ix<<log2PODscale,iy<<log2PODscale, center,sz);
			pStreamer->DestroyPhysicalEntitiesInBox(center-sz,center+sz);
			UnmarkAsPODThread(m_pWorld);
			pPODcell->lifeTime = -1;
			int *picellNext;
			pe_PODcell *pPODcell1;
			for(i=iActivePODCell0,picellNext=&iActivePODCell0; i>=0; i=pPODcell1->inextActive)
				if (pPODcell==(pPODcell1=getPODcell(i&0xFFFF,i>>16))) {
					*picellNext = pPODcell->inextActive; break;
				} else
					picellNext = &pPODcell1->inextActive;
    }
	}
}

void CPhysicalWorld::UnregisterBBoxInPODGrid(const Vec3 *BBox)
{
	m_entgrid.UnregisterBBoxInPODGrid(BBox);
}
void SEntityGrid::UnregisterBBoxInPODGrid(const Vec3 *BBox)
{
	if (!bHasPODGrid)
		return;

	int i,ix,iy,igx[2],igy[2],imask;
	pe_PODcell *pPODcell;
	Vec3 gBBox[2]; BBoxToGrid(BBox[0],BBox[1],gBBox);
	for(i=0;i<2;i++) {
		igx[i] = max(-1,min(size.x,float2int(gBBox[i].x*stepr.x-0.5f)));
		igy[i] = max(-1,min(size.y,float2int(gBBox[i].y*stepr.y-0.5f)));
		igx[i] >>= log2PODscale; igy[i] >>= log2PODscale;
	}
	for(ix=igx[0];ix<=igx[1];ix++) for(iy=igy[0];iy<=igy[1];iy++) {
		imask = ~negmask(ix) & ~negmask(iy) & negmask(ix - (size.x>>log2PODscale)) & negmask(iy - (size.y>>log2PODscale));   // (x>=0 && x<m_entgrid.size.x) ? 0xffffffff : 0;
		i = (ix>>3)*PODstride.x + (iy>>3)*PODstride.y;
		i = i + ((-1-i) & ~imask);
		if(!pPODcells[i])
			continue;
		pPODcell = pPODcells[i] + ((ix&7)+(iy&7)*8 & imask);
		if (--pPODcell->nObjects<=0)
			pPODcell->zlim.set(1000.0f,-1000.0f);
	}
}

int CPhysicalWorld::AddRefEntInPODGrid(IPhysicalEntity *_pent, const Vec3 *BBox)
{
	WriteLock lock(m_lockPODGrid);
	return GetGrid(_pent)->AddRefEntInPODGrid(_pent,BBox);
}
int SEntityGrid::AddRefEntInPODGrid(IPhysicalEntity *_pent, const Vec3 *BBox)
{
	CPhysicalEntity *pent = (CPhysicalEntity*)_pent;
	int i,ix,iy,nCells=0;
	Vec2i ig[2];
	const Vec3 *pBBox = BBox ? BBox:pent->m_BBox;
	Vec3 gBBox[2]; BBoxToGrid(pBBox[0],pBBox[1], gBBox);
	for(i=0;i<2;i++) {
		ig[i].x = max(-1,min(size.x,float2int(gBBox[i].x*stepr.x-0.5f)));
		ig[i].y = max(-1,min(size.y,float2int(gBBox[i].y*stepr.y-0.5f)));
		ig[i].x >>= log2PODscale; ig[i].y >>= log2PODscale;
	}
	for(ix=ig[0].x;ix<=ig[1].x;ix++) for(iy=ig[0].y;iy<=ig[1].y;iy++)
		if (getPODcell(ix<<log2PODscale,iy<<log2PODscale)->lifeTime>0)
			++pent->m_nRefCount,++pent->m_nRefCountPOD,++nCells;
	return nCells;
}


void SEntityGrid::GetPODGridCellBBox(int ix,int iy, Vec3 &center,Vec3 &size) const
{
	Vec2 stepPOD = step*(1<<log2PODscale);
	pe_PODcell *pPODcell = getPODcell(ix,iy);
	center = vecFromGrid(Vec3(((ix>>log2PODscale)+0.5f)*stepPOD.x, ((iy>>log2PODscale)+0.5f)*stepPOD.y, (pPODcell->zlim[1]+pPODcell->zlim[0])*0.5f));
	size = vecFromGrid(Vec3(stepPOD.x*0.5f, stepPOD.y*0.5f, (pPODcell->zlim[1]-pPODcell->zlim[0])*0.5f));
}


int CPhysicalWorld::SetSurfaceParameters(int surface_idx, float bounciness,float friction, unsigned int flags)
{
	if ((unsigned int)surface_idx>=(unsigned int)NSURFACETYPES)
		return 0;
	m_BouncinessTable[surface_idx] = bounciness;
	m_FrictionTable[surface_idx] = friction;
	m_DynFrictionTable[surface_idx] = friction*(1.0/1.5);
	m_SurfaceFlagsTable[surface_idx] = flags;
	m_DamageReductionTable[surface_idx] = 0;
	m_RicochetAngleTable[surface_idx] = 0;
	m_RicDamReductionTable[surface_idx] = 0;
	m_RicVelReductionTable[surface_idx] = 0;
	return 1;
}
int CPhysicalWorld::GetSurfaceParameters(int surface_idx, float &bounciness,float &friction, unsigned int &flags)
{
	if ((unsigned int)surface_idx>=(unsigned int)NSURFACETYPES)
		return 0;
	bounciness = m_BouncinessTable[surface_idx];
	friction = m_FrictionTable[surface_idx];
	flags = m_SurfaceFlagsTable[surface_idx];
	return 1;
}

int CPhysicalWorld::SetSurfaceParameters(int surface_idx, float bounciness, float friction,
																				 float damage_reduction, float ric_angle, float ric_dam_reduction,
																				 float ric_vel_reduction, unsigned int flags)
{
	if ((unsigned int)surface_idx>=(unsigned int)NSURFACETYPES)
		return 0;
	SetSurfaceParameters(surface_idx, bounciness, friction, flags);
	m_DamageReductionTable[surface_idx] = damage_reduction;
	m_RicochetAngleTable[surface_idx] = ric_angle;
	m_RicDamReductionTable[surface_idx] = ric_dam_reduction;
	m_RicVelReductionTable[surface_idx] = ric_vel_reduction;
	return 1;
}

int CPhysicalWorld::GetSurfaceParameters(int surface_idx, float &bounciness,float &friction,
																				 float &damage_reduction, float &ric_angle, float &ric_dam_reduction,
																				 float &ric_vel_reduction, unsigned int &flags)
{
	if ((unsigned int)surface_idx>=(unsigned int)NSURFACETYPES)
		return 0;
	GetSurfaceParameters(surface_idx, bounciness, friction, flags);
	damage_reduction = m_DamageReductionTable[surface_idx];
	ric_angle = m_RicochetAngleTable[surface_idx];
	ric_dam_reduction = m_RicDamReductionTable[surface_idx];
	ric_vel_reduction = m_RicVelReductionTable[surface_idx];
	return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

IPhysicalEntity* CPhysicalWorld::CreatePhysicalEntity(pe_type type, float lifeTime, pe_params* params, void *pForeignData,int iForeignData,
																											int id, IPhysicalEntity *pHostPlaceholder, IGeneralMemoryHeap* pHeap)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other,0,"CreatePhysicalEntity");

	CPhysicalEntity *res=0;
	CPhysicalPlaceholder *pEntityHost = (CPhysicalPlaceholder*)pHostPlaceholder;
//#ifdef _DEBUG
//	{ WriteLockCond lock(m_lockStep, pForeignData!=0 || iForeignData!=0x5AFE);	// since in debug memory manager is not thread-safe (but dll-specific)
//#endif

	switch (type) {
		case PE_STATIC: res = CPhysicalEntity::Create<CPhysicalEntity>(this, pHeap); break;
		case PE_RIGID : res = CPhysicalEntity::Create<CRigidEntity>(this, pHeap); break;
		case PE_LIVING: res = CPhysicalEntity::Create<CLivingEntity>(this, pHeap); break;
		case PE_WHEELEDVEHICLE: res = CPhysicalEntity::Create<CWheeledVehicleEntity>(this, pHeap); break;
		case PE_WALKINGRIGID: res = CPhysicalEntity::Create<CWalkingRigidEntity>(this, pHeap); break;
		case PE_PARTICLE: res = CPhysicalEntity::Create<CParticleEntity>(this, pHeap); break;
		case PE_ARTICULATED: res = CPhysicalEntity::Create<CArticulatedEntity>(this, pHeap); break;
		case PE_ROPE: res = CPhysicalEntity::Create<CRopeEntity>(this, pHeap); break;
		case PE_SOFT: res = CPhysicalEntity::Create<CSoftEntity>(this, pHeap); break;
	}
	m_nTypeEnts[type]++;
	if (!res)
		return 0;
	SetGrid(res, &m_entgrid);

//#ifdef _DEBUG
//	}
//#endif

	if (type!=PE_STATIC)
		m_nDynamicEntitiesDeleted = 0;
	if (pEntityHost && lifeTime>0) {
		res->m_pForeignData = pEntityHost->m_pForeignData;
		res->m_iForeignData = pEntityHost->m_iForeignData;
		res->m_iForeignFlags = pEntityHost->m_iForeignFlags;
		res->m_id = pEntityHost->m_id;
		res->m_pEntBuddy = pEntityHost;
		pEntityHost->m_pEntBuddy = res;
		res->m_maxTimeIdle = lifeTime;
		res->m_bPrevPermanent = res->m_bPermanent = 0;
		res->m_iGThunk0 = pEntityHost->m_iGThunk0;
		res->m_ig[0].x=pEntityHost->m_ig[0].x; res->m_ig[1].x=pEntityHost->m_ig[1].x;
		res->m_ig[0].y=pEntityHost->m_ig[0].y; res->m_ig[1].y=pEntityHost->m_ig[1].y;
	} else {
		res->m_bPrevPermanent = res->m_bPermanent = 1;
		res->m_pForeignData = pForeignData;
		res->m_iForeignData = iForeignData;
		m_lastExtId = max(m_lastExtId, id);
		m_nExtIds += 1+(id>>31);
		SetPhysicalEntityId(res, id>=0 ? id:GetFreeEntId());
	}
	res->m_flags |= 0x80000000u;
	if (params)
		res->SetParams(params, iForeignData==0x5AFE || get_iCaller()<MAX_PHYS_THREADS);

	int isPOD = IsPODThread(this);
	if (!m_lockStep && (isPOD || !m_lockTPR) && lifeTime==0) {
		WriteLockCond lock1(m_lockCaller[MAX_PHYS_THREADS], !isPOD && m_nEnts+1>m_nEntsAlloc-1);
		WriteLockCond lock2(m_lockTPR, !isPOD && m_nEnts+1>m_nEntsAlloc-1);
		WriteLock lock(m_lockStep);
		res->m_flags &= ~0x80000000u;
		RepositionEntity(res,2);
		if (++m_nEnts > m_nEntsAlloc-1) {
			int nEntsAllocNew = m_nEntsAlloc+4096;
			m_nEntListAllocs++; m_bEntityCountReserved = 0;
			ReallocateList(m_pTmpEntList,m_nEnts-1,nEntsAllocNew);
			ReallocateList(m_pTmpEntList1,m_nEnts-1,nEntsAllocNew);
			if (m_threadData[MAX_PHYS_THREADS].szList<nEntsAllocNew)
				ReallocateList(m_pTmpEntList2,m_threadData[MAX_PHYS_THREADS].szList,nEntsAllocNew);
			ReallocateList(m_pGroupMass,m_nEnts-1,nEntsAllocNew);
			ReallocateList(m_pMassList,m_nEnts-1,nEntsAllocNew);
			ReallocateList(m_pGroupIds,m_nEnts-1,nEntsAllocNew);
			ReallocateList(m_pGroupNums,m_nEnts-1,nEntsAllocNew);
			m_nEntsAlloc = nEntsAllocNew;
		}
	} else if (!m_lockQueue || get_iCaller()>=MAX_PHYS_THREADS && iForeignData!=0x5AFE) {
		WriteLock lock(m_lockQueue);
		AllocRequestsQueue(sizeof(int)*3+sizeof(void*));
		QueueData(4);	// RepositionEntity opcode
		QueueData((int)(sizeof(int)*3+sizeof(void*)));	// size
		QueueData(res);
		QueueData(2);	// flags
	} else {
		res->m_timeIdle = -10.0f;
		m_nOnDemandListFailures++;
	}

	return res;
}


IPhysicalEntity *CPhysicalWorld::CreatePhysicalPlaceholder(pe_type type, pe_params* params, void *pForeignData,int iForeignData, int id)
{
	int i,j,iChunk;
	if (m_nPlaceholders*10<m_iLastPlaceholder*7) {
		for(i=m_iLastPlaceholder>>5; i>=0 && m_pPlaceholderMap[i]==-1; i--);
		if (i>=0) {
			for(j=0;j<32 && m_pPlaceholderMap[i]&1<<j;j++);
			i = i<<5|j;
		}
		i = i-(i>>31) | m_iLastPlaceholder+1&i>>31;
	} else
		i = m_iLastPlaceholder+1;

	iChunk = i>>PLACEHOLDER_CHUNK_SZLG2;
	if (iChunk==m_nPlaceholderChunks) {
		m_nPlaceholderChunks++;
		ReallocateList(m_pPlaceholders, m_nPlaceholderChunks-1,m_nPlaceholderChunks,true);
		ReallocateList(m_pPlaceholderMap, (m_iLastPlaceholder>>5)+1,m_nPlaceholderChunks<<PLACEHOLDER_CHUNK_SZLG2-5,true);
	}
	if (!m_pPlaceholders[iChunk])
		m_pPlaceholders[iChunk] = new CPhysicalPlaceholder[PLACEHOLDER_CHUNK_SZ];
	CPhysicalPlaceholder *res = m_pPlaceholders[iChunk]+(i & PLACEHOLDER_CHUNK_SZ-1);

	SetGrid(res, &m_entgrid);
	res->m_pForeignData = pForeignData;
	res->m_iForeignData = iForeignData;
	res->m_iForeignFlags = 0;
	res->m_iGThunk0 = 0;
	res->m_ig[0].x=res->m_ig[0].y=res->m_ig[1].x=res->m_ig[1].y = GRID_REG_PENDING;
	res->m_pEntBuddy = 0;
	res->m_id = -1;
	res->m_bProcessed = 0;
	switch (type) {
		case PE_STATIC: res->m_iSimClass = 0; break;
		case PE_RIGID: case PE_WHEELEDVEHICLE: res->m_iSimClass = 1; break;
		case PE_LIVING: res->m_iSimClass = 3; break;
		case PE_PARTICLE: case PE_ROPE: case PE_ARTICULATED: case PE_SOFT: res->m_iSimClass = 4;
	}
	m_pPlaceholderMap[i>>5] |= 1<<(i&31);

	if (id>-2)
		SetPhysicalEntityId(res, id>=0 ? id:GetFreeEntId());
	else
		res->m_id = id;
	if (params)
		res->SetParams(params);
	m_nPlaceholders++;
	m_iLastPlaceholder = max(m_iLastPlaceholder,i);

	return res;
}


int CPhysicalWorld::DestroyPhysicalEntity(IPhysicalEntity* _pent,int mode,int bThreadSafe)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int idx;
	CPhysicalPlaceholder *ppc = (CPhysicalPlaceholder*)_pent;
	if (ppc->m_pEntBuddy && ppc->m_pEntBuddy->IsPlaceholder() && mode!=0 || m_nDynamicEntitiesDeleted && ppc->m_iSimClass>0)
		return 0;
	if (!(idx=IsPlaceholder(ppc)))
		if (ppc->m_iSimClass!=5) {
			if (mode & 4 && ((CPhysicalEntity*)ppc)->Release()>0)
				return 0;
			if (!bThreadSafe && m_vars.lastTimeStep>0.0f) {
				EventPhysEntityDeleted eped;
				eped.pEntity=ppc; eped.mode=mode;
				eped.pForeignData=ppc->m_pForeignData; eped.iForeignData=ppc->m_iForeignData;
				eped.isFromPOD = IsPODThread(this);
				SignalEvent(&eped,0);
			}
			((CPhysicalEntity*)ppc)->m_iDeletionTime = mode+1;
			if (mode==0) {
				((CPhysicalEntity*)ppc)->m_pForeignData = 0;
				((CPhysicalEntity*)ppc)->m_iForeignData = -1;
			}
		} else
			((CPhysArea*)ppc)->m_bDeleted = mode!=2;
	mode &= 3;

	//if (m_lockStep & (bThreadSafe^1))
	if (m_vars.bMultithreaded & (bThreadSafe^1) && (m_lockStep || m_lockTPR || m_vars.lastTimeStep>0 || mode==1 && ppc->m_bProcessed>=PENT_QUEUED)) {
		WriteLock lock(m_lockQueue);
		AtomicAdd(&ppc->m_bProcessed, PENT_QUEUED);
		AllocRequestsQueue(sizeof(int)*3+sizeof(void*));
		QueueData(5);	// DestroyPhysicalEntity opcode
		QueueData((int)(sizeof(int)*3+sizeof(void*)));	// size
		QueueData(_pent);
		QueueData(mode);
		return 1;
	}
	WriteLockCond lock0(m_lockCaller[MAX_PHYS_THREADS], m_vars.bMultithreaded && !bThreadSafe && !IsPODThread(this));
	WriteLockCond lock(m_lockStep, m_vars.bMultithreaded && !bThreadSafe && !IsPODThread(this));

	if (ppc->m_iSimClass==5) {
		if (mode==0)
			RemoveArea(_pent);
		return 1;
	}

	if (_pent->GetType()==PE_GRID) {
		SEntityGrid *pgrid = static_cast<SEntityGrid*>(_pent);
		if (pgrid!=&m_entgrid && mode==0 && CryInterlockedDecrement(&pgrid->m_refCount)<=0) {
			if (pgrid->m_next) pgrid->m_next->m_prev = pgrid->m_prev;
			if (pgrid->m_prev) pgrid->m_prev->m_next = pgrid->m_next;
			if (pgrid==m_pDeletedGrids) m_pDeletedGrids = nullptr;
			pgrid->m_prev = nullptr; pgrid->m_next = m_pDeletedGrids; 
			if (m_pDeletedGrids) m_pDeletedGrids->m_prev = pgrid;
			m_pDeletedGrids = pgrid;
			pgrid->m_iSimClass = 7;
		}
		return 1;
	}

	if (idx) {
		if (mode!=0)
			return 0;
		if (ppc->m_pEntBuddy && ppc->m_pEntBuddy->m_pEntBuddy==ppc)
			DestroyPhysicalEntity(ppc->m_pEntBuddy,mode,1);
		SetPhysicalEntityId(ppc,-1);
		{ WriteLock lockGrid(m_lockGrid);
			DetachEntityGridThunks(ppc);
		}
		--idx;
		m_pPlaceholderMap[idx>>5] &= ~(1<<(idx&31));
		m_nPlaceholders--;

		int i,j,iChunk = idx>>PLACEHOLDER_CHUNK_SZLG2;
		// if entire iChunk is empty, deallocate it
		for(i=j=0;i<(PLACEHOLDER_CHUNK_SZ>>5);i++) j |= m_pPlaceholderMap[(iChunk<<PLACEHOLDER_CHUNK_SZLG2-5)+i];
		if (!j) {
			delete[] m_pPlaceholders[iChunk]; m_pPlaceholders[iChunk] = 0;
		}
		j = m_nPlaceholderChunks;
		// make sure that m_iLastPlaceholder points to the last used placeholder slot
		for(; m_iLastPlaceholder>=0 && !(m_pPlaceholderMap[m_iLastPlaceholder>>5] & 1<<(m_iLastPlaceholder&31)); m_iLastPlaceholder--)
		if ((m_iLastPlaceholder^m_iLastPlaceholder-1)+1>>1 == PLACEHOLDER_CHUNK_SZ) {
			// if m_iLastPlaceholder points to the 1st chunk element, entire chunk is free and can be deallocated
			iChunk = m_iLastPlaceholder>>PLACEHOLDER_CHUNK_SZLG2;
			if (m_pPlaceholders[iChunk]) {
				delete[] m_pPlaceholders[iChunk]; m_pPlaceholders[iChunk] = 0;
			}
			m_nPlaceholderChunks = iChunk;
		}
		if (m_nPlaceholderChunks<j)
			ReallocateList(m_pPlaceholderMap,j<<PLACEHOLDER_CHUNK_SZLG2-5,m_nPlaceholderChunks<<PLACEHOLDER_CHUNK_SZLG2-5,true);

		return 1;
	}

	CPhysicalEntity *pent = (CPhysicalEntity*)_pent;
	if (pent->m_iSimClass==7)
		return 0;
	pent->m_iDeletionTime = max(4,m_iLastLogPump+2);
	for(idx=m_nProfiledEnts-1;idx>=0;idx--) if (m_pEntProfileData[idx].pEntity==pent)
		memmove(m_pEntProfileData+idx, m_pEntProfileData+idx+1, (--m_nProfiledEnts-idx)*sizeof(m_pEntProfileData[0]));
	for(idx=0; idx<=MAX_PHYS_THREADS; idx++)
		m_prevGEAobjtypes[idx] = -1;

	if (pent->m_iSimClass<0) {
		if (pent->m_next) pent->m_next->m_prev = pent->m_prev;
		if (pent->m_prev) pent->m_prev->m_next = pent->m_next;
		if (pent==m_pHiddenEnts) m_pHiddenEnts = pent->m_next;
		pent->m_next = pent->m_prev = 0;
	}
	if (mode==2) {
		if (pent->m_iSimClass==-1 && pent->m_iPrevSimClass>=0) {
			pent->m_ig[0].x=pent->m_ig[1].x=pent->m_ig[0].y=pent->m_ig[1].y = GRID_REG_PENDING;
			pent->m_iSimClass = pent->m_iPrevSimClass & 0x0F; pent->m_iPrevSimClass=-1;
			int locked = RepositionEntity(pent);
			AtomicAdd(&m_lockGrid, -locked);
			if (pent->m_pStructure && pent->m_pStructure->defparts)
				for(int i=0;i<pent->m_nParts;i++) if (pent->m_pStructure->defparts[i].pSkinInfo && pent->m_pStructure->defparts[i].pSkelEnt)
					DestroyPhysicalEntity(pent->m_pStructure->defparts[i].pSkelEnt,2,1);
		}
		pent->m_iDeletionTime = 0;
		return 1;
	}

	if (m_pEntBeingDeleted==pent)
		return 1;
	m_pEntBeingDeleted = pent;
	if (mode==0 && !pent->m_bPermanent && m_pPhysicsStreamer)
		m_pPhysicsStreamer->DestroyPhysicalEntity(pent);
	m_pEntBeingDeleted = 0;

	pent->AlertNeighbourhoodND(mode);
	if ((unsigned int)pent->m_iPrevSimClass<8u && pent->m_iSimClass>=0) {
		WriteLock lockl(m_lockList);
		if (pent->m_next) pent->m_next->m_prev = pent->m_prev;
		(pent->m_prev ? pent->m_prev->m_next : m_pTypedEnts[pent->m_iPrevSimClass]) = pent->m_next;
		if (pent==m_pTypedEntsPerm[pent->m_iPrevSimClass])
			m_pTypedEntsPerm[pent->m_iPrevSimClass] = pent->m_next;
	}
	pent->m_next=pent->m_prev = 0;
/*#ifdef _DEBUG
CPhysicalEntity *ptmp = m_pTypedEnts[1];
for(;ptmp && ptmp!=m_pTypedEntsPerm[1]; ptmp=ptmp->m_next);
if (ptmp!=m_pTypedEntsPerm[1])
DEBUG_BREAK;
#endif*/

	if (!pent->m_pEntBuddy || IsPortal(pent))	{
		{ WriteLock lockGrid(m_lockGrid);
			DetachEntityGridThunks(pent);
		}
		for(int j=0;j<pent->m_nParts;j++) if (pent->m_parts[j].pPlaceholder)
			DestroyPhysicalEntity(pent->ReleasePartPlaceholder(j),0,1);
    if (pent->m_flags & pef_parts_traceable)
		  (pent->m_flags &= ~pef_parts_traceable) |= pef_traceable;
		pent->m_nUsedParts = 0;
	}
	pent->m_iGThunk0 = 0;

	if (mode==0) {
		int bWasRegistered = !(pent->m_flags & 0x80000000u);
		pent->m_iPrevSimClass = -1; 
		pent->m_pForeignData = 0;
		pent->m_iForeignData = -1;
		if (pent->m_next = m_pTypedEnts[7])
			pent->m_next->m_prev=pent;
		if (pent->m_pEntBuddy) {
			pent->m_pEntBuddy->m_pEntBuddy = 0;
			if (IsPortal(pent) && !pent->m_pEntBuddy->m_pForeignData)	{
				DestroyPhysicalEntity(pent->m_pEntBuddy,0,1);
				goto freeid;
			}
		} else {
			freeid:
			if (pent->m_id<=m_lastExtId)
				--m_nExtIds;
			SetPhysicalEntityId(pent,-1);	pent->m_id=-1;
		}
		pent->m_iSimClass = 7;
		m_pTypedEnts[7] = pent;
		if (bWasRegistered)
			m_nTypeEnts[pent->GetType()]--;
		if (bWasRegistered && --m_nEnts < m_nEntsAlloc-8192 && !m_bEntityCountReserved) {
			WriteLockCond lock1(m_lockCaller[MAX_PHYS_THREADS], m_vars.bMultithreaded && !bThreadSafe && IsPODThread(this));
			m_nEntsAlloc -= 8192; m_nEntListAllocs++;
			ReallocateList(m_pTmpEntList,m_nEntsAlloc+8192,m_nEntsAlloc);
			ReallocateList(m_pTmpEntList1,m_nEntsAlloc+8192,m_nEntsAlloc);
			ReallocateList(m_pTmpEntList2,m_nEntsAlloc+8192,m_nEntsAlloc);
			ReallocateList(m_pGroupMass,0,m_nEntsAlloc);
			ReallocateList(m_pMassList,0,m_nEntsAlloc);
			ReallocateList(m_pGroupIds,0,m_nEntsAlloc);
			ReallocateList(m_pGroupNums,0,m_nEntsAlloc);
			m_threadData[0].szList=m_threadData[MAX_PHYS_THREADS].szList = m_nEntsAlloc;
		}
	} else if (pent->m_iSimClass>=0) {
		pe_action_reset reset;
		pent->Action(&reset);
		pent->m_iPrevSimClass = pent->m_iSimClass | 0x100;
		pent->m_iSimClass = -1;
	}
	if (mode==1) {
		if (m_pHiddenEnts) m_pHiddenEnts->m_prev = pent;
		pent->m_next = m_pHiddenEnts; m_pHiddenEnts = pent;
		pent->m_prev = 0;
	}

	return 1;
}


int CPhysicalWorld::ReserveEntityCount(int nNewEnts)
{
	if (m_nEnts+nNewEnts > m_nEntsAlloc-1) {
		m_nEntsAlloc = (m_nEnts+nNewEnts & ~4095) + 4096;
		m_nEntListAllocs++; m_bEntityCountReserved = 1;
		ReallocateList(m_pTmpEntList,m_nEnts,m_nEntsAlloc);
		ReallocateList(m_pTmpEntList1,m_nEnts,m_nEntsAlloc);
		ReallocateList(m_pTmpEntList2,m_nEnts,m_nEntsAlloc);
		ReallocateList(m_pGroupMass,m_nEnts,m_nEntsAlloc);
		ReallocateList(m_pMassList,m_nEnts,m_nEntsAlloc);
		ReallocateList(m_pGroupIds,m_nEnts,m_nEntsAlloc);
		ReallocateList(m_pGroupNums,m_nEnts,m_nEntsAlloc);
	}
	return m_nEntsAlloc;
}


void CPhysicalWorld::CleanseEventsQueue()
{
	WriteLock lock(m_lockEventsQueue);
	EventPhys *pEvent,**ppPrevNext;
	for(pEvent=m_pEventFirst,ppPrevNext=&m_pEventFirst,m_pEventLast=0; pEvent; pEvent=*ppPrevNext) {
		if (pEvent->idval<=EventPhysCollision::id &&
				(((CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[0])->m_iDeletionTime ||
					((CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[1])->m_iDeletionTime) ||
				pEvent->idval>EventPhysCollision::id && ((CPhysicalEntity*)((EventPhysMono*)pEvent)->pEntity)->m_iDeletionTime)
		{
			if (pEvent->idval==EventPhysCreateEntityPart::id)
				DestroyPhysicalEntity(((EventPhysCreateEntityPart*)pEvent)->pEntNew,0,1);
			*ppPrevNext = pEvent->next;
			pEvent->next = m_pFreeEvents[pEvent->idval]; m_pFreeEvents[pEvent->idval] = pEvent;
		} else {
			ppPrevNext = &pEvent->next;
			m_pEventLast = pEvent;
		}
	}
}

void CPhysicalWorld::PatchEventsQueue(IPhysicalEntity* pEntity, void* pForeignData, int iForeignData)
{
	WriteLock lock(m_lockEventsQueue);
	EventPhys *pEvent;
	EventPhysMono* pMono;
	EventPhysStereo* pStereo;
	int i;
	for(pEvent=m_pEventFirst; pEvent; pEvent=pEvent->next) {
		if (pEvent->idval>=0 && pEvent->idval <= EventPhysCollision::id) {
			pStereo = (EventPhysStereo*)pEvent;
			for (i=0; i<2; ++i) if (pStereo->pEntity[i] == pEntity)	{
				pStereo->pForeignData[i] = pForeignData;
				pStereo->iForeignData[i] = iForeignData;
			}
		} else if (pEvent->idval==EventPhysRWIResult::id || pEvent->idval==EventPhysPWIResult::id) {
			// Do nothing, the foreign data should not be changed
		} else if (pEvent->idval<EVENT_TYPES_NUM) {
			pMono = (EventPhysMono*)pEvent;
			if (pMono->pEntity == pEntity) {
				pMono->pForeignData = pForeignData;
				pMono->iForeignData = iForeignData;
			}
		} else {
#     if !defined(_RELEASE)
			// intentionally crashing here - please update the code if new events have been created
            __debugbreak();
#     endif
		}
	}
}

int CPhysicalWorld::GetFreeEntId()
{
	int nPhysEnts=m_nEnts-m_nExtIds, nPhysSlots=m_iNextId-m_lastExtId;
	if (nPhysEnts*2 > nPhysSlots)
		return m_iNextId++;
	int nTries;
	for(nTries=100; nTries>0 && m_iNextIdDown>m_lastExtId && m_pEntsById[m_iNextIdDown]; m_iNextIdDown--,nTries--);
	if (nTries<=0)
		return m_iNextId++;
	if (m_iNextIdDown<=m_lastExtId)
		for(m_iNextIdDown=m_iNextId-2,nTries=100; nTries>0 && m_iNextIdDown>m_lastExtId && m_pEntsById[m_iNextIdDown]; m_iNextIdDown--,nTries--);
	if (nTries<=0 || m_iNextIdDown<=m_lastExtId)
		return m_iNextId++;
	return m_iNextIdDown--;
}


int CPhysicalWorld::SetPhysicalEntityId(IPhysicalEntity *_pent, int id, int bReplace, int bThreadSafe)
{
	WriteLockCond lock(m_lockEntIdList,bThreadSafe^1);
	CPhysicalPlaceholder *pent = (CPhysicalPlaceholder*)_pent;
	unsigned int previd = (unsigned int)pent->m_id;
	if (previd<(unsigned int)m_nIdsAlloc) {
		m_pEntsById[previd] = 0;
		if (previd==m_iNextId-1)
			for(;m_iNextId>0 && m_pEntsById[m_iNextId-1]==0;m_iNextId--);
		if (previd==m_lastExtId)
			for(--m_lastExtId; m_lastExtId>0 && !m_pEntsById[m_lastExtId]; m_lastExtId--);
	}
	m_iNextId = max(m_iNextId,id+1);

	if (id>=0) {
		if (id>=m_nIdsAlloc) {
			int nAllocPrev = m_nIdsAlloc;
			ReallocateList(m_pEntsById, nAllocPrev,m_nIdsAlloc=(id&~32767)+32768, true);
		}
		if (m_pEntsById[id]) {
			if (bReplace)
				SetPhysicalEntityId(m_pEntsById[id],GetFreeEntId(),1,1);
			else
				return 0;
		}
		if (pent->m_pEntBuddy && pent->m_pEntBuddy->IsPlaceholder())
			pent = pent->m_pEntBuddy;
		(m_pEntsById[id] = pent)->m_id = id;
		if (pent->m_pEntBuddy && !IsPortal(pent))
			pent->m_pEntBuddy->m_id = id;
		return 1;
	}
	return 0;
}

int CPhysicalWorld::GetPhysicalEntityId(IPhysicalEntity *pent)
{
	return pent==WORLD_ENTITY ? -2 : (pent ? ((CPhysicalEntity*)pent)->m_id : -1);
}

IPhysicalEntity* CPhysicalWorld::GetPhysicalEntityById(int id)
{
	ReadLock lock(m_lockEntIdList);
	if (id==-1)
		return m_pHeightfield[0];
	else if (id==-2)
		return &g_StaticPhysicalEntity;
	else {
		int bNoExpand = id>>30; id &= ~(1<<30);
		if ((unsigned int)id<(unsigned int)m_nIdsAlloc)
			return m_pEntsById[id] ? (!bNoExpand ? m_pEntsById[id]->GetEntity():m_pEntsById[id]->GetEntityFast()) : 0;
	}
	return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////


static inline void swap(CPhysicalEntity **pentlist,float *pmass,int *pids, int i1,int i2) {
	CPhysicalEntity *pent = pentlist[i1]; pentlist[i1] = pentlist[i2]; pentlist[i2] = pent;
	float m = pmass[i1]; pmass[i1] = pmass[i2]; pmass[i2] = m;
	if (pids) {
		int id = pids[i1]; pids[i1] = pids[i2]; pids[i2] = id;
	}
}
static void qsort(CPhysicalEntity **pentlist,float *pmass,int *pids, int ileft,int iright)
{
	if (ileft>=iright) return;
	int i,ilast;
	float diff = 0.0f;
	swap(pentlist,pmass,pids, ileft,ileft+iright>>1);
	for(ilast=ileft,i=ileft+1; i<=iright; i++) {
		diff += fabs_tpl(pmass[i]-pmass[ileft]);
		if (pmass[i] > pmass[ileft])
			swap(pentlist,pmass,pids, ++ilast,i);
	}
	swap(pentlist,pmass,pids, ileft,ilast);

	if (diff>0) {
		qsort(pentlist,pmass,pids, ileft,ilast-1);
		qsort(pentlist,pmass,pids, ilast+1,iright);
	}
}

CPhysicalEntity* GroupByOuterEntity(CPhysicalEntity *pent0)
{
#define next m_next_coll2
#define prev m_next_coll1
	if (!pent0)
		return 0;
	CPhysicalEntity *pent,*pent1=0,*pentLast;
	for(pent=pent0; pent; pentLast=pent,pent=pent->next) pent->m_bProcessed_aux=1<<31;
	for(pent=pent0,pent1=0; pent; pent=pent->next) if (pent->m_pOuterEntity && pent->m_pOuterEntity->m_bProcessed_aux>=0) {
		// temporarily append external outer ents to the list	(in m_bProcessed_aux mark bit 31 = in list, bit 30 = is external)
		pentLast->next = pent->m_pOuterEntity; pentLast = pent->m_pOuterEntity;
		pentLast->m_next_coll = pentLast->next; // store previous value of coll_next2
		pentLast->next = 0; pentLast->m_bProcessed_aux = 3<<30;
	}
	for(pent=pent0,pent1=0; pent; pent->prev=pent1,pent1=pent,pent=pent->next);	// double-link the list
	for(pent=pentLast=pent0; pent; pentLast=pent,pent=pent1) {
		pent1 = pent->next;	// re-insert pent after its outer entity, if it has any
		if (pent->m_pOuterEntity && pent!=pent->m_pOuterEntity && pent->prev!=pent->m_pOuterEntity) {
			if (pent->next) pent->next->prev = pent->prev; 
			if (pent->prev) pent->prev->next = pent->next;
			pent->next = pent->m_pOuterEntity->next; pent->prev = pent->m_pOuterEntity;
			if (pent->m_pOuterEntity->next) pent->m_pOuterEntity->next->prev = pent;
			pent->m_pOuterEntity->next = pent;
		}
	}
	for(; pent0->prev; pent0=pent0->prev);
	pent = pent0;	pentLast = 0;
	for(int iGroup=-1; pent; pent=pent1) {
		pent1 = pent->next;
		if (!pent->m_pOuterEntity || pent->m_pOuterEntity==pent)
			++iGroup;
		if (pent->m_bProcessed_aux & 1<<30)	{
			(pentLast ? pentLast->next : pent0) = pent->next;	 // remove external ents from the list
			pent->m_next_coll2 = pent->m_next_coll;
		} else
			(pentLast=pent)->m_iGroup = iGroup;
		pent->m_bProcessed_aux = 0;
	}
	return pent0;
#undef prev
#undef next
}

int CPhysicalWorld::ReallocTmpEntList(CPhysicalEntity **&pEntList, int iCaller, int szNew)
{
	assert(iCaller<=MAX_PHYS_THREADS);
	ReallocateList(m_threadData[iCaller].pTmpEntList, m_threadData[iCaller].szList, szNew);
	pEntList = m_threadData[iCaller].pTmpEntList;
	if (iCaller==0)
		m_pTmpEntList = m_threadData[iCaller].pTmpEntList;
	else if (iCaller==MAX_PHYS_THREADS)
		m_pTmpEntList2 = m_threadData[iCaller].pTmpEntList;
	return m_threadData[iCaller].szList = szNew;
}


inline bool AABB_overlap2d(const Vec2& min0,const Vec2& max0, const Vec2 &min1,const Vec2& max1) {
	return max(fabs_tpl(min0.x+max0.x-min1.x-max1.x) - (max0.x-min0.x)-(max1.x-min1.x),
						 fabs_tpl(min0.y+max0.y-min1.y-max1.y) - (max0.y-min0.y)-(max1.y-min1.y))<0;
}

static ILINE int getcell_safe(grid& g, int ix, int iy, int iOutOfBounds)
{
	return (iOutOfBounds & get_entities_out_of_bounds)==0 ? (iy*g.stride.y + ix*g.stride.x) : g.getcell_safe(ix, iy);
}

const int ent_GEA_recursive = 1<<30;

int CPhysicalWorld::GetEntitiesAround(const Vec3 &ptmin,const Vec3 &ptmax, CPhysicalEntity **&pList, int objtypes,
																			CPhysicalEntity *pPetitioner, int szListPrealloc, int iCaller)
{
	SEntityGrid &grid = pPetitioner ? *GetGrid(pPetitioner) : m_entgrid;
	IF (!grid.cells || !m_pTmpEntList, 0) return 0;
	CPhysicalEntity **pTmpEntList;
	int nout=0,nout0=0,itypePetitioner;

	int maskCaller = 1<<iCaller, maskCaller4 = sqr(sqr(maskCaller));
	EventPhysBBoxOverlap event;
	int szList = GetTmpEntList(pTmpEntList, iCaller);
	IF ((szListPrealloc | (objtypes & ent_allocate_list))==0, 1) {
		pList = pTmpEntList;
	}

	if (pPetitioner) {
		itypePetitioner = 1<<pPetitioner->m_iSimClass & -iszero((int)pPetitioner->m_flags&pef_never_affect_triggers);
		event.pEntity[0]=pPetitioner; event.pForeignData[0]=pPetitioner->m_pForeignData; event.iForeignData[0]=pPetitioner->m_iForeignData;
		if (objtypes & ent_GEA_recursive)
			nout=nout0 = szListPrealloc;
	} else {
		itypePetitioner = 0;
	}

	const int itype = itypePetitioner;
	const int bAreasOnly = iszero(objtypes-ent_areas);
#ifndef _RELEASE
	m_nGEA[iCaller]++;
#endif
	if ((ptmin-m_prevGEABBox[iCaller][0]).len2()+(ptmax-m_prevGEABBox[iCaller][1]).len2()+(objtypes & ent_GEA_recursive) == 0.0f &&
		sqr(objtypes-m_prevGEAobjtypes[iCaller])+ 1+(iCaller-MAX_PHYS_THREADS>>31) == 0) {
		pList = pTmpEntList; return m_nprevGEAEnts[iCaller];
	}

	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
#ifndef PHYS_FUNC_PROFILER_DISABLED
	INT_PTR mask = (INT_PTR)pPetitioner;
	mask = mask>>sizeof(mask)*8-1 ^ (mask-1)>>sizeof(mask)*8-1;
	PHYS_FUNC_PROFILER((const char*)((INT_PTR)"GetEntitiesAround(Physics)"&~mask | (INT_PTR)"GetEntitiesAround(External)"&mask));
#endif

	Vec3 gBBox[2]; grid.BBoxToGrid(ptmin,ptmax, gBBox);
	float zdiff = gBBox[1].z-gBBox[0].z, zsum = gBBox[1].z+gBBox[0].z;
	for(int i=0;i<2;i++) gBBox[i] = gBBox[i]*Diag33(grid.stepr.x, grid.stepr.y, grid.rzGran);
	int igx[2] = { float2int(gBBox[0].x-0.5f), float2int(gBBox[1].x-0.5f) };
	int igy[2] = { float2int(gBBox[0].y-0.5f), float2int(gBBox[1].y-0.5f) };

	if (m_entgrid.iscyclic()) {
	}	else
	IF ((m_vars.iOutOfBounds & get_entities_out_of_bounds)==0, 1) {
		int mask1, mask2, mask3, mask4, mask5, mask6, mask7, mask8;
		igx[0] = apply_clamp(igx[0], 0, grid.size.x-1, mask1, mask2);
		igx[1] = apply_clamp(igx[1], 0, grid.size.x-1, mask3, mask4);
		igy[0] = apply_clamp(igy[0], 0, grid.size.y-1, mask5, mask6);
		igy[1] = apply_clamp(igy[1], 0, grid.size.y-1, mask7, mask8);
		if ( (mask1&mask3) | (mask5&mask7) | (mask2&mask4) | (mask6&mask8) )
			return 0;	// both x<0 or both y<0 or both x>=maxx or both y>=maxy
	} else {
		igx[0] = max(-1,min(grid.size.x, igx[0]));
		igx[1] = max(-1,min(grid.size.x, igx[1]));
		igy[0] = max(-1,min(grid.size.y, igy[0]));
		igy[1] = max(-1,min(grid.size.y, igy[1]));
	}

	const int igx0 = igx[0];
	const int igx1 = igx[1];
	const int igy0 = igy[0];
	const int igy1 = igy[1];

	IF ((igx1-igx0+1)*(igy1-igy0+1)>m_vars.nGEBMaxCells, 0) {
		if (m_pLog)
			m_pLog->Log("GetEntitiesInBox: too many cells requested by %s (%d, (%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f))",
			pPetitioner && m_pRenderer ?
			m_pRenderer->GetForeignName(pPetitioner->m_pForeignData,pPetitioner->m_iForeignData,pPetitioner->m_iForeignFlags):"Game",
			(igx1-igx0+1)*(igy1-igy0+1), ptmin.x,ptmin.y,ptmin.z, ptmax.x,ptmax.y,ptmax.z);
		if (m_vars.bBreakOnValidation) DoBreak;
	}

	{ ReadLock lock0(m_lockGrid);
		for(int ix=igx0;ix<=igx1;ix++) {
			for(int iy=igy0;iy<=igy1;iy++) {
				if ((objtypes & (ent_static|ent_no_ondemand_activation))==ent_static)	{
					pe_PODcell *pPODcell = grid.getPODcell(ix,iy);
					const float zrange = pPODcell->zlim[1]-pPODcell->zlim[0];
					if (fabs_tpl((zsum-pPODcell->zlim[1]-pPODcell->zlim[0])*zrange) < (zdiff+zrange)*zrange)	{
						CryInterlockedAdd(&m_lockGrid,-1);
						ReadLock lockPOD(m_lockPODGrid);
						if (pPODcell->lifeTime<=0) {
							CryInterlockedAdd(&m_lockPODGrid,-1);
							{	WriteLock lockPODw(m_lockPODGrid);
								if (pPODcell->lifeTime<=0) {
									MarkAsPODThread(this);
									Vec3 center,size;
									grid.GetPODGridCellBBox(ix,iy,center,size);
									m_nOnDemandListFailures=0; ++m_iLastPODUpdate;
									if (m_pPhysicsStreamer->CreatePhysicalEntitiesInBox(center-size,center+size))	{
										pPODcell->lifeTime = m_nOnDemandListFailures ? 0.001f:8.0f;
										pPODcell->inextActive = grid.iActivePODCell0;
										grid.iActivePODCell0 = iy<<16|ix;
										szList = max(szList, GetTmpEntList(pTmpEntList, iCaller));
									}
								}
							}
							ReadLockCond lockPODr(m_lockPODGrid,1); lockPODr.SetActive(0);
							m_nOnDemandListFailures=0;
						}	else
							pPODcell->lifeTime = max(8.0f,pPODcell->lifeTime);
						UnmarkAsPODThread(this);
						ReadLockCond relock(m_lockGrid,1); relock.SetActive(0);
					}
				}

				for(int ithunk = grid.cells[getcell_safe(grid, ix, iy, m_vars.iOutOfBounds)], iObjTypesValid = objtypes & 1<<m_gthunks[ithunk].iSimClass, ithunk_next = 0; ithunk && iObjTypesValid|bAreasOnly^1;
					ithunk=ithunk_next, iObjTypesValid = objtypes & 1<<m_gthunks[ithunk].iSimClass)
				{
					ithunk_next = m_gthunks[ithunk].inext;
					int iz0 = m_gthunks[ithunk].BBoxZ0, iz1 = m_gthunks[ithunk].BBoxZ1;
					iz1 += isneg(iz1-iz0)<<16;
					int diz = float2int((gBBox[0].z+gBBox[1].z-iz0-iz1)*(0.5f/65536))*65536;
					if (iObjTypesValid && 
							(!m_entgrid.inrange(ix,iy) || AABB_overlap(gBBox[0], gBBox[1],
								Vec3(ix+m_gthunks[ithunk].BBox[0]*(1.0f/256), iy+m_gthunks[ithunk].BBox[1]*(1.0f/256), iz0+diz),
								Vec3(ix+(m_gthunks[ithunk].BBox[2]+1)*(1.0f/256), iy+(m_gthunks[ithunk].BBox[3]+1)*(1.0f/256), iz1+diz))) &&
						!(m_gthunks[ithunk].pent->m_bProcessed & maskCaller))
					{
						CPhysicalPlaceholder *pGridEnt = m_gthunks[ithunk].pent;
						int bContact;
						{ ReadLock lock1(pGridEnt->m_lockUpdate);
							bContact = AABB_overlap(ptmin,ptmax,pGridEnt->m_BBox[0],pGridEnt->m_BBox[1]);
						}

						if (bContact) {
							if (nout>=szList)
								szList = ReallocTmpEntList(pTmpEntList,iCaller,szList+1024);
							if ((unsigned int)(pGridEnt->m_iSimClass-5)>1u) {
								m_bGridThunksChanged = 0;
								CPhysicalEntity *pent = pGridEnt->GetEntity();
								if (m_bGridThunksChanged)
									ithunk_next = grid.cells[getcell_safe(grid,ix,iy, m_vars.iOutOfBounds)];
								m_bGridThunksChanged = 0;
								int bProcessed;
								if (!pGridEnt->m_pEntBuddy || pent->m_pEntBuddy==pGridEnt) {
									if (pGridEnt->m_id<=-2)
										continue;	// Placeholders shouldn't be here
									if (objtypes & ent_ignore_noncolliding) {
										int i=0;
										for(;i<pent->m_nParts && !(pent->m_parts[i].flags & geom_colltype_solid);i++);
										if (i==pent->m_nParts) continue;
									}
									pTmpEntList[nout] = pent;
									bProcessed = iszero(m_bUpdateOnlyFlagged & ((int)pent->m_flags^pef_update)) | iszero(pent->m_iSimClass);
									bProcessed &= iszero(((CPhysicalEntity*)pGridEnt)->m_iDeletionTime);
									nout += bProcessed; AtomicAdd(&pGridEnt->m_bProcessed, maskCaller & -bProcessed);
								} else if ((m_bUpdateOnlyFlagged & ((int)pent->m_flags^pef_update) & -pent->m_iSimClass>>31)==0) {
									bProcessed = isneg(-(int)(pent->m_bProcessed & maskCaller));
									AtomicAdd(&pent->m_lockUpdate, bProcessed^1);
									volatile char *pw=(volatile char*)&pent->m_lockUpdate+(1+eBigEndian); for(;*pw;); // ReadLock(m_lockUpdate)
									AtomicAdd(&pent->m_bProcessed, maskCaller & ~-bProcessed);
									AtomicAdd(&pent->m_nUsedParts, (pent->m_nUsedParts & 15*maskCaller4)*(bProcessed-1u));
									int nUsedParts = pent->m_nUsedParts>>iCaller*4 & 15;
									int notFull = nUsedParts+1>>4 ^ 1;
									notFull &= 1-iszero((INT_PTR)pGridEnt->m_pEntBuddy);
									nUsedParts += notFull;
									AtomicAdd(&pent->m_nUsedParts, maskCaller4 & -notFull);
									pent->m_pUsedParts[iCaller][nUsedParts-1] = -2-pGridEnt->m_id;
									if (!bProcessed)
										pTmpEntList[nout++] = pent;
									AtomicAdd(&pGridEnt->m_bProcessed, maskCaller);
								}
							} else if (pGridEnt->m_iSimClass==5) {
								if (!((CPhysArea*)pGridEnt)->m_bDeleted) {
									pTmpEntList[nout++] = (CPhysicalEntity*)pGridEnt;
								AtomicAdd(&pGridEnt->m_bProcessed, maskCaller);
								}
							} else if (pGridEnt->m_iForeignFlags & itype) {
								event.pEntity[1] = pGridEnt->m_iForeignData==PHYS_FOREIGN_ID_PHYS_AREA ? (IPhysicalEntity*)pGridEnt->m_pForeignData : pGridEnt; 
								event.pForeignData[1]=pGridEnt->m_pForeignData; event.iForeignData[1]=pGridEnt->m_iForeignData;
								OnEvent(pPetitioner->m_flags, &event);
								// handle portals to other grids
								if (pGridEnt->m_iForeignFlags & TRIGGER_PORTAL && GetHostedGrid(pPetitioner)!=GetGrid(pGridEnt->m_pEntBuddy)) {
									CPhysicalEntity *pentTrg = (CPhysicalEntity*)pGridEnt;
									if (!pGridEnt->IsPlaceholder() && pentTrg->m_nParts) {
										COverlapChecker Overlapper;
										box bbox[2]; 
										primitive *pgeom = bbox+1;
										bbox[0].Basis.SetIdentity(); bbox[0].bOriented = 0; 
										bbox[0].center = (ptmin+ptmax)*0.5f; bbox[0].size = (ptmax-ptmin)*0.5f;
										const geom& part = pentTrg->m_parts[0];
										part.pPhysGeom->pGeom->GetBBox(bbox+1);
										bbox[1].center = pentTrg->m_qrot*(part.q*bbox[1].center*part.scale + part.pos) + pentTrg->m_pos;
										bbox[1].Basis *= Matrix33(!part.q*!pentTrg->m_qrot);
										bbox[1].size *= part.scale;
										if (!Overlapper.Check(box::type,box::type, bbox,bbox+1))
											continue;
									}
									Vec3 BBox1[2]; transformBBox(ptmin,ptmax, BBox1, GetGrid(pGridEnt->m_pEntBuddy)->m_transW.GetInverted()*GetGrid(pGridEnt)->m_transW);
									CPhysicalEntity **plist = pTmpEntList;
									nout = GetEntitiesAround(BBox1[0],BBox1[1], plist, objtypes & ent_all & ~ent_terrain | ent_GEA_recursive, (CPhysicalEntity*)pGridEnt->m_pEntBuddy, nout, iCaller);
									szList = max(szList, GetTmpEntList(pTmpEntList, iCaller));
									pPetitioner->SetLastPortal(pGridEnt);
								}
	} } } } } } }

	for(int i=(objtypes & ent_GEA_recursive);i<nout;i++)	{
		AtomicAdd(&(pTmpEntList[i]->m_pEntBuddy ? pTmpEntList[i]->m_pEntBuddy:pTmpEntList[i])->m_bProcessed,-maskCaller);
		int j,nUsedParts = pTmpEntList[i]->m_nUsedParts>>iCaller*4 & 15;
		nUsedParts &= ~-iszero(pTmpEntList[i]->m_iSimClass-5);
		if (nUsedParts==15)	for(j=0;j<pTmpEntList[i]->m_nParts;j++)
			AtomicAdd(&pTmpEntList[i]->m_parts[j].pPlaceholder->m_bProcessed,-(int)(pTmpEntList[i]->m_parts[j].pPlaceholder->m_bProcessed & maskCaller));
		else for(j=0;j<nUsedParts;j++)
			AtomicAdd(&pTmpEntList[i]->m_parts[pTmpEntList[i]->m_pUsedParts[iCaller][j]].pPlaceholder->m_bProcessed,-maskCaller);
		AtomicAdd(&pTmpEntList[i]->m_lockUpdate, -nUsedParts>>31);
	}

	if (m_pHeightfield[iCaller] && objtypes & ent_terrain && &grid==&m_entgrid) {
		if (nout>=szList)
			szList = ReallocTmpEntList(pTmpEntList,iCaller,szList+1024);
		pTmpEntList[nout++] = m_pHeightfield[iCaller];
	}

	if (objtypes & ent_sort_by_mass) {
		for(int i=0;i<nout;i++) m_pMassList[i] = pTmpEntList[i]->GetMassInv();
		// manually put all static (0-massinv) object to the end of the list, since qsort doesn't
		// perform very well on lists of same numbers
		int ilast;
		for(int i=ilast=nout-1; i>0; i--) if (m_pMassList[i]==0) {
			if (i!=ilast)
				swap(pTmpEntList,m_pMassList,0, i,ilast);
			--ilast;
		}
		qsort(pTmpEntList,m_pMassList,0, 0,ilast);
	}

	if (objtypes&1<<5) {
		ReadLock lock1(m_lockAreas);
		if (m_pGlobalArea) for(CPhysArea* pArea=m_pGlobalArea->m_nextBig; pArea; pArea=pArea->m_nextBig)
			if (!pArea->m_bDeleted && AABB_overlap(ptmin,ptmax,pArea->m_BBox[0],pArea->m_BBox[1]) && nout<szList)
				pTmpEntList[nout++] = (CPhysicalEntity*)pArea;
	}

	if (szListPrealloc<nout) {
		if (!(objtypes & ent_allocate_list))
			pList = pTmpEntList;
		else if (nout>0) {	//  don't allocate 0-elements arrays
			pList = new CPhysicalEntity*[nout];
			for(int i=0;i<nout;i++) pList[i] = pTmpEntList[i];
		}
	}	else
		for(int i=nout0;i<nout;i++) pList[i] = pTmpEntList[i];

	if (objtypes & ent_addref_results)
		for(int i=0;i<nout;i++) pList[i]->AddRef();

	if (!(objtypes & ent_GEA_recursive)) {
		m_prevGEABBox[iCaller][0]=ptmin; m_prevGEABBox[iCaller][1]=ptmax;
		m_prevGEAobjtypes[iCaller]=objtypes; m_nprevGEAEnts[iCaller]=nout;
	}
	return nout;
}


void CPhysicalWorld::ScheduleForStep(CPhysicalEntity *pent, float time_interval)
{
	WriteLock lock(m_lockAuxStepEnt);
	if (!(pent->m_flags & pef_step_requested)) {
		pent->m_flags |= pef_step_requested;
		pent->m_next_coll2 = m_pAuxStepEnt;
		pent->m_timeIdle = time_interval;
		m_pAuxStepEnt = pent;
	}	else
		pent->m_timeIdle = min(pent->m_timeIdle, time_interval);
}


void CPhysicalWorld::UpdateDeformingEntities(float time_interval)
{
	WriteLock lock3(m_lockDeformingEntsList);
	int i,j;
	int iCaller = get_iCaller_int();
	if (time_interval>=0) {
		for(i=j=0; i<m_nDeformingEnts; i++) if (m_pDeformingEnts[i]->m_iSimClass!=7 && m_pDeformingEnts[i]->UpdateStructure(time_interval,0,iCaller))
			m_pDeformingEnts[j++] = m_pDeformingEnts[i];
		else
			m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
		m_nDeformingEnts = j;
	} else {
		for(i=0;i<m_nDeformingEnts;i++)
			m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
		m_nDeformingEnts = 0;
	}
}


void SPhysTask::ThreadEntry() { m_pWorld->ThreadProc(m_idx,this); }
void SPhysTask::SignalStopWork() { bStop=1; m_pWorld->m_threadStart[m_idx].Set(); }
void SPhysTask::Wait() { m_pWorld->m_threadDone[m_idx].Wait(); }
int __cursubstep=10;

void CPhysicalWorld::ProcessIslandSolverResults(int i, int iter, float groupTimeStep,float Ebefore, int nEnts,float fixedDamping, int &bAllGroupsFinished,
																								entity_contact **pContacts,int nContacts,int nBodies, int iCaller,int64 iticks0)
{
	int i1,j,bGroupFinished,nBrokenParts,nParts0,idCurGroup=m_pGroupIds[i];
	float Eafter,damping;
	CPhysicalEntity *pent;
	iter += m_rq.iter-iter | iter>>31;

	for(j=0;j<nContacts;j++) if ((pContacts[j]->ipart[0]|pContacts[j]->ipart[1])>=0) {
		if (pContacts[j]->pent[0]->m_parts[pContacts[j]->ipart[0]].flags & geom_monitor_contacts)
			pContacts[j]->pent[0]->OnContactResolved(pContacts[j],0,idCurGroup);
		if (pContacts[j]->pent[1]->m_parts[pContacts[j]->ipart[1]].flags & geom_monitor_contacts)
			pContacts[j]->pent[1]->OnContactResolved(pContacts[j],1,idCurGroup);
	}

	#ifdef ENTITY_PROFILER_ENABLED

	i1 = CryGetTicks()-iticks0;
	if (m_vars.bProfileGroups && iticks0>0)
		m_grpProfileData[13].nTicks += i1;
	if (m_vars.bProfileEntities>0 && iticks0>0) {
		for(pent=m_pTmpEntList1[i],j=0; pent; pent=pent->m_next_coll)
			j += -pent->m_iSimClass>>31 & 1;
		i1 /= max(1,j);
		for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll) if (pent->m_iSimClass>0)
			AddEntityProfileInfo(pent,i1);
	}
	#endif

	damping = 1.0f-groupTimeStep*m_vars.groupDamping*isneg(m_vars.nGroupDamping-1-nEnts);
	for(pent=m_pTmpEntList1[i],bGroupFinished=1,Eafter=0.0f; pent; pent=pent->m_next_coll) {
		Eafter += pent->CalcEnergy(0);
		if (!(pent->m_flags & pef_fixed_damping))
			damping = min(damping,pent->GetDamping(groupTimeStep));
		else {
			damping = pent->GetDamping(groupTimeStep);
			break;
		}
	}
	//Ebefore *= isneg(-nAnimatedObjects)+1; // increase energy growth limit if we have animated bodies involved
	if (Eafter>Ebefore*(1.0f+0.1f*isneg(nBodies-15)))
		damping = min(damping, sqrt_tpl(Ebefore/Eafter));
	if (fixedDamping>-0.5f)
		damping = fixedDamping;
	for(pent=m_pTmpEntList1[i],bGroupFinished=1; pent; pent=pent->m_next_coll)
		bGroupFinished &= pent->Update(groupTimeStep, damping);
	bGroupFinished |= isneg(m_vars.nMaxSubstepsLargeGroup-iter-2 & m_vars.nBodiesLargeGroup-nBodies-1);
	bGroupFinished |= isneg(m_vars.nMaxSubsteps-iter-2);
	if (!bGroupFinished) for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll)
		pent->m_bMoved = 0;
	else {
		WriteLock lock1(m_lockMovedEntsList);
		for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll) {
			pent->m_bMoved = 3;
			if (pent->m_iSimClass<3 && !pent->m_next_coll2) {
				pent->m_next_coll2=(CPhysicalEntity*)m_pMovedEnts; m_pMovedEnts=pent;
			}
		}
	}
	bAllGroupsFinished &= bGroupFinished;

	// process deforming (breaking) enities of this group
	{
		WriteLock lock3(m_lockDeformingEntsList);
		for(i1=j=nBrokenParts=0; i1<m_nDeformingEnts; i1++) if (m_pDeformingEnts[i1]->m_iGroup==idCurGroup) {
			if ((nParts0=m_pDeformingEnts[i1]->m_nParts) && m_pDeformingEnts[i1]->m_iSimClass!=7) {
				if (m_pDeformingEnts[i1]->UpdateStructure(max(groupTimeStep,0.01f),0,iCaller))
					m_pDeformingEnts[j++] = m_pDeformingEnts[i1];
				else
					m_pDeformingEnts[i1]->m_flags &= ~pef_deforming;
				nBrokenParts += -iszero((int)m_pDeformingEnts[i1]->m_flags & aef_recorded_physics) & nParts0-m_pDeformingEnts[i1]->m_nParts;
			} else
				m_pDeformingEnts[i1]->m_flags &= ~pef_deforming;
			m_pDeformingEnts[i1]->m_iGroup = -1;
		}	else
			m_pDeformingEnts[j++] = m_pDeformingEnts[i1];
		if (m_nDeformingEnts) m_threadData[iCaller].pTmpPartBVListOwner=0; // Something broke: invalidate the precomputed bv data for ropes
		m_nDeformingEnts = j;
	}
	// if some entities broke, step back the velocities, but don't re-execute the step immediately
	if (nBrokenParts) for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll)
		pent->StepBack(0);
}

int CPhysicalWorld::ReadDelayedSolverResults(CMemStream &stm, float &dt,float &Ebefore,int &nEnts,float &fixedDamping,
																						 entity_contact **pContacts,RigidBody **pBodies)
{
	int iCaller = get_iCaller_int();
	int iGroup,nContacts,nBodies;
	stm.Read(iGroup); stm.Read(dt); stm.Read(Ebefore); stm.Read(nEnts);	stm.Read(fixedDamping);
	stm.Read(m_threadData[iCaller].bGroupInvisible);
	stm.Read(nContacts); stm.ReadRaw(pContacts, sizeof(void*)*nContacts);
	stm.Read(nBodies); stm.ReadRaw(pBodies, sizeof(void*)*nBodies);
	return iGroup;
}

void CPhysicalWorld::ProcessNextEntityIsland(float time_interval, int ipass, int iter, int &bAllGroupsFinished, int iCaller)
{
	int i,j,n,nEnts,nAnimatedObjects,nBodies,bStepValid,bGroupInvisible;
	float Ebefore,groupTimeStep,fixedDamping,maxGroupFriction;
	CPhysicalEntity *pent,*pent_next,*phead,**pentlist;

	do {
		{ WriteLock lock(m_lockNextEntityGroup);
			if (m_iCurGroup>=m_nGroups)
				break;
			i = m_iCurGroup++;
		}
		m_threadData[iCaller].groupMass=m_curGroupMass = m_pGroupMass[i]-m_maxGroupMass*isneg(m_maxGroupMass-m_pGroupMass[i]);
		m_threadData[iCaller].bGroupInvisible = 0;
		groupTimeStep = time_interval*(ipass^1); nAnimatedObjects = 0; fixedDamping = -1.0f;
		Ebefore = 0.0f;
		for(phead=m_pTmpEntList1[i],bGroupInvisible=pef_invisible,maxGroupFriction=100.0f; phead; phead=phead->m_next_coll)	{
			ReadLock lockcol(phead->m_lockColliders);
			bGroupInvisible &= phead->m_flags;
			for(j=0,n=phead->GetColliders(pentlist); j<n; j++) if (pentlist[j]->m_iSimClass>1 && pentlist[j]->GetMassInv()<=0) {
				if (ipass) {
					if (pentlist[j]->m_flags & pef_fixed_damping)
						fixedDamping = max(fixedDamping, pentlist[j]->GetDamping(pentlist[j]->GetMaxTimeStep(time_interval)));
					groupTimeStep = max(groupTimeStep, pentlist[j]->GetLastTimeStep(time_interval));
					maxGroupFriction = min(maxGroupFriction, pentlist[j]->GetMaxFriction());
					RigidBody *pbody = pentlist[j]->GetRigidBody();
					Vec3 sz = pentlist[j]->m_BBox[1]-pentlist[j]->m_BBox[0], v(pbody->v),w(pbody->w);
					if (pentlist[j]->GetType()==PE_ARTICULATED)	for(int ibody=0; ibody<pentlist[j]->m_nParts; ibody++) {
						pbody = pentlist[j]->GetRigidBody(ibody);
						v += (pbody->v-v)*isneg(v.len2()-pbody->v.len2());
						w += (pbody->w-w)*isneg(w.len2()-pbody->w.len2());
					}
					Ebefore += m_curGroupMass*(v.len2()+w.len2()*sqr(max(max(sz.x,sz.y),sz.z)));
				} else
					groupTimeStep = min(groupTimeStep, pentlist[j]->GetMaxTimeStep(time_interval));
				nAnimatedObjects++;
				bGroupInvisible &= pentlist[j]->m_flags | ~-iszero(pentlist[j]->m_iSimClass-2);
			}
		}
		m_threadData[iCaller].bGroupInvisible = -(-bGroupInvisible>>31);
		m_threadData[iCaller].maxGroupFriction = maxGroupFriction;
		m_threadData[MAX_PHYS_THREADS].maxGroupFriction = maxGroupFriction;

		if (ipass==0) {
			for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll)
				groupTimeStep = min(groupTimeStep, pent->GetMaxTimeStep(time_interval));
			for(pent=m_pTmpEntList1[i],bStepValid=1,phead=0; pent; pent=pent_next) {
				pent_next=pent->m_next_coll;
				if (pent->m_iSimClass<3 || pent->GetType()==PE_ARTICULATED)
					bStepValid &= (phead=pent)->Step(groupTimeStep);
				pent->m_bMoved = 1;
			}
			if (!bStepValid) {
				for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll)
					pent->StepBack(groupTimeStep);
			}
			for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll) pent->m_bMoved = 2;
		} else if (time_interval>0) {
			for(pent=m_pTmpEntList1[i],groupTimeStep=0; pent; pent=pent->m_next_coll) if (pent->m_iSimClass>1)
				groupTimeStep = max(groupTimeStep, pent->GetLastTimeStep(time_interval));
			if (groupTimeStep==0)
				groupTimeStep = time_interval;
			InitContactSolver(groupTimeStep);

			if (m_vars.nMaxPlaneContactsDistress!=m_vars.nMaxPlaneContacts) {
				for(pent=m_pTmpEntList1[i],j=nEnts=0; pent; pent=pent->m_next_coll,nEnts++)
					j += pent->GetContactCount(m_vars.nMaxPlaneContacts);
				n = j>m_vars.nMaxContacts ? m_vars.nMaxPlaneContactsDistress : m_vars.nMaxPlaneContacts;
				for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll)	{
					pent->RegisterContacts(groupTimeStep,n);
					Ebefore += pent->CalcEnergy(groupTimeStep);
				}
			} else for(pent=m_pTmpEntList1[i],nEnts=0; pent; pent=pent->m_next_coll,nEnts++) {
				pent->RegisterContacts(groupTimeStep, m_vars.nMaxPlaneContacts);
				Ebefore += pent->CalcEnergy(groupTimeStep);
			}

			Ebefore = max(m_pGroupMass[i]*sqr(0.005f),Ebefore);

			int64 iticks0=0;
			#ifdef ENTITY_PROFILER_ENABLED
			iticks0 = CryGetTicks();
			#endif

			entity_contact **pContacts;
			int nContacts=0;
			nBodies = InvokeContactSolver(groupTimeStep, &m_vars, Ebefore, pContacts,nContacts);
			ProcessIslandSolverResults(i,iter,groupTimeStep,Ebefore,nEnts,fixedDamping,bAllGroupsFinished,pContacts,nContacts,nBodies,iCaller,iticks0);
		}
	} while(true);
}

void CPhysicalWorld::ProcessNextEngagedIndependentEntity(int iCaller)
{
	CPhysicalEntity *pent,*pentEnd,*pentNext;
	do {
		{ WriteLock lock(m_lockNextEntityGroup);
			if (!m_pCurEnt)
				break;
			for(pent=pentEnd=(CPhysicalEntity*)m_pCurEnt; pentEnd->m_next_coll2 && pentEnd->m_next_coll2->m_iGroup==pent->m_iGroup; pentEnd=pentEnd->m_next_coll2); 
			m_pCurEnt = pentEnd->m_next_coll2;
		}
		do {
			pentNext = pent->m_next_coll2; pent->m_next_coll2 = 0;
			m_threadData[iCaller].bGroupInvisible = isneg(-((int)pent->m_flags & pef_invisible));
			pent->m_flags &= ~pef_step_requested;
			float dtFull=pent->m_timeIdle, dt;
			for(int iter=0; !pent->Step(dt=pent->GetMaxTimeStep(dtFull)) && ++iter<m_vars.nMaxSubsteps && dtFull>0.001f; )
				dtFull -= dt;
			pent->m_bMoved = 0;
			if (pent==pentEnd)
				break;
		} while(pent=pentNext);
	} while(true);
}

void CPhysicalWorld::ProcessNextLivingEntity(float time_interval, int bSkipFlagged, int iCaller)
{
	CPhysicalEntity *pent,*pentEnd;
	Vec3 BBox[2],BBoxNew[2],velAbs;
	do {
		{ WriteLock lock(m_lockNextEntityGroup);
			if (!m_pCurEnt)
				break;
			pent=pentEnd = (CPhysicalEntity*)m_pCurEnt;
			velAbs =((CLivingEntity*)pent)->m_vel.abs();
			BBox[0] = pent->m_BBox[0]-velAbs; BBox[1] = pent->m_BBox[1]+velAbs;
			while(pentEnd->m_next) {
				velAbs = ((CLivingEntity*)pentEnd->m_next)->m_vel.abs();
				BBoxNew[0] = pentEnd->m_next->m_BBox[0]-velAbs; BBoxNew[1] = pentEnd->m_next->m_BBox[1]+velAbs;
				if (AABB_overlap(BBox,BBoxNew)) {
					BBox[0] = min(BBox[0], BBoxNew[0]);
					BBox[1] = max(BBox[1], BBoxNew[1]);
					pentEnd = pentEnd->m_next;
				}	else
					break;
			}
			m_pCurEnt = pentEnd->m_next;
		}
		if (m_nWorkerThreads>0) {
			assert(m_nWorkerThreads+FIRST_WORKER_THREAD<=MAX_PHYS_THREADS);
			int i;
			do {
				do {
					ReadLock lockr(m_lockPlayerGroups);
					for(i=0; i<m_nWorkerThreads+FIRST_WORKER_THREAD && (i==iCaller || !AABB_overlap(m_BBoxPlayerGroup[i],BBox)); i++);
					if (i>=m_nWorkerThreads+FIRST_WORKER_THREAD)
						break;
				} while(true);
				{ WriteLock lockw(m_lockPlayerGroups);
					for(i=0; i<m_nWorkerThreads+FIRST_WORKER_THREAD && (i==iCaller || !AABB_overlap(m_BBoxPlayerGroup[i],BBox)); i++);
					if (i>=m_nWorkerThreads+FIRST_WORKER_THREAD) {
						m_BBoxPlayerGroup[iCaller][0] = BBox[0];
						m_BBoxPlayerGroup[iCaller][1] = BBox[1];
						break;
					}
				}
			} while(true);
		}
		do {
			if (!(m_bUpdateOnlyFlagged&(pent->m_flags^pef_update) | bSkipFlagged&pent->m_flags))
				pent->Step(pent->GetMaxTimeStep(time_interval*m_vars.timeScalePlayers));
			if (pent==pentEnd)
				break;
		}	while (pent=pent->m_next);
		if (m_nWorkerThreads>0) {
			WriteLock lock(m_lockPlayerGroups);
			m_BBoxPlayerGroup[iCaller][0]=m_BBoxPlayerGroup[iCaller][1] = Vec3(1e10f);
		}
	} while(true);
}

void CPhysicalWorld::ProcessNextIndependentEntity(float time_interval, int bSkipFlagged, int iCaller)
{
	CPhysicalEntity *pent,*pentEnd;
	int iter;
	do {
		{ WriteLock lock(m_lockNextEntityGroup);
			if (!m_pCurEnt)
				break;
			for(pent=pentEnd=(CPhysicalEntity*)m_pCurEnt; pentEnd->m_next_coll2 && pentEnd->m_next_coll2->m_iGroup==pent->m_iGroup; pentEnd=pentEnd->m_next_coll2); 
			m_pCurEnt = pentEnd->m_next_coll2;
		}
		do {
			if (!(m_bUpdateOnlyFlagged&(pent->m_flags^pef_update) | bSkipFlagged&pent->m_flags)) {
				m_threadData[iCaller].bGroupInvisible = isneg(-((int)pent->m_flags & pef_invisible));
				for(iter=0; !pent->Step(pent->GetMaxTimeStep(time_interval)) && ++iter<m_vars.nMaxSubsteps; );
			}
			if (pent==pentEnd)
				break;
		} while(pent=pent->m_next_coll2);
	} while(true);
}

void CPhysicalWorld::ProcessBreakingEntities(float time_interval)
{
	WriteLock lock3(m_lockDeformingEntsList);
	int i,j;
	int iCaller = get_iCaller_int();
	for(i=j=0; i<m_nDeformingEnts; i++) if (m_pDeformingEnts[i]->m_iSimClass!=7 && m_pDeformingEnts[i]->UpdateStructure(time_interval,0,iCaller))
		m_pDeformingEnts[j++] = m_pDeformingEnts[i];
	else
		m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
	m_nDeformingEnts = j;
}

void CPhysicalWorld::ThreadProc(int ithread, SPhysTask *pTask)
{
	if (pTask->bStop)
		return;
	AtomicAdd(&m_nWorkerThreads,1);
	MarkAsPhysWorkerThread(&ithread);
	while(true) {
		m_threadStart[ithread-FIRST_WORKER_THREAD].Wait();
		if (pTask->bStop)	{
			m_threadDone[ithread-FIRST_WORKER_THREAD].Set();
			break;
		}
		switch(m_rq.ipass) {
			case 0:
			case 1: ProcessNextEntityIsland(m_rq.time_interval, m_rq.ipass, m_rq.iter, *m_rq.pbAllGroupsFinished,ithread); break;
			case 2: ProcessNextEngagedIndependentEntity(ithread); break;
			case 3: ProcessNextLivingEntity(m_rq.time_interval, m_rq.bSkipFlagged, ithread); break;
			case 4: ProcessNextIndependentEntity(m_rq.time_interval, m_rq.bSkipFlagged, ithread); break;
			case 5: ProcessBreakingEntities(m_rq.time_interval); break;
		}
		m_threadDone[ithread-FIRST_WORKER_THREAD].Set();
	}
	AtomicAdd(&m_nWorkerThreads,-1);
}

int __curstep = 0; // debug

void CPhysicalWorld::TimeStep(float time_interval, int flags)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	float m,/*m_groupTimeStep,*/time_interval_org = time_interval;
	CPhysicalEntity *pent,*phead,*ptail,**pentlist,*pent_next,*pent1,*pentmax;
	int i,i1,j,n,iter,ipass,nGroups,bHeadAdded,bAllGroupsFinished,bSkipFlagged;

	if (time_interval<0)
		return;
	//if (m_vars.bMultithreaded)
	//	m_pLog = 0;

	m_vars.numThreads = min(m_vars.numThreads,MAX_PHYS_THREADS);
	if (m_vars.numThreads!=m_nWorkerThreads+FIRST_WORKER_THREAD) {
		for(i=m_nWorkerThreads-1;i>=0;i--) {
			m_threads[i]->bStop=1,m_threadStart[i].Set(), m_threadDone[i].Wait();
			delete m_threads[i]; m_threads[i] = NULL;
		}

		for(i=0;i<m_vars.numThreads-FIRST_WORKER_THREAD;i++) {
			m_threads[i] = new SPhysTask(this,i+FIRST_WORKER_THREAD);
			if (!gEnv->pThreadManager->SpawnThread(m_threads[i], "PhysicsWorkerThread_%u", i)) {
				CryFatalError("Error spawning \"PhysicsWorkerThread_%u\" thread.", i);
			}
		}
		for(; m_nWorkerThreads!=m_vars.numThreads-FIRST_WORKER_THREAD; )
			CrySleep(1);
	}

	{
    WriteLock lock1(m_lockCaller[MAX_PHYS_THREADS]),lock2(m_lockStep);
		char **pQueueSlots;
		int nQueueSlots;
		volatile int64 timer = CryGetTicks();
		{ WriteLock lock3(m_lockQueue);
			pQueueSlots=m_pQueueSlots; m_pQueueSlots=m_pQueueSlotsAux; m_pQueueSlotsAux=pQueueSlots;
			nQueueSlots=m_nQueueSlots; m_nQueueSlots=m_nQueueSlotsAux; m_nQueueSlotsAux=nQueueSlots;
			i=m_nQueueSlotsAlloc; m_nQueueSlotsAlloc=m_nQueueSlotsAllocAux; m_nQueueSlotsAllocAux=i;
			i=m_nQueueSlotSize; m_nQueueSlotSize=m_nQueueSlotSizeAux; m_nQueueSlotSizeAux=i;
		}
		if (time_interval>0 && !(flags & ent_flagged_only))
			MarkAsPhysThread();
		phys_geometry *pgeom;
		for(i=0; i<nQueueSlots; i++) for(j=0; (iter=*(int*)(pQueueSlots[i]+j))!=-1; j+=*(int*)(pQueueSlots[i]+j+sizeof(int))) {
			if (iter<0)
				continue;
			pent = *(CPhysicalEntity**)(pQueueSlots[i]+j+sizeof(int)*2);
			if (!(pent->m_iSimClass==7 || pent->m_iSimClass==5 && pent->m_iDeletionTime==2))	switch (iter) {
				case 0: pent->SetParams((pe_params*)(pQueueSlots[i]+j+sizeof(int)*2+sizeof(void*)),1); break;
				case 1: pent->Action((pe_action*)(pQueueSlots[i]+j+sizeof(int)*2+sizeof(void*)),1); break;
				case 2: pent->AddGeometry(pgeom=*(phys_geometry**)(pQueueSlots[i]+j+sizeof(int)*2+sizeof(void*)),
																	(pe_geomparams*)(pQueueSlots[i]+j+sizeof(int)*3+sizeof(void*)*2),
																	*(int*)(pQueueSlots[i]+j+sizeof(int)*2+sizeof(void*)*2),1);
								AtomicAdd(&pgeom->nRefCount, -1);	break;
				case 3: pent->RemoveGeometry(*(int*)(pQueueSlots[i]+j+sizeof(int)*2+sizeof(void*)*2),1); break;
				case 4:
					pent->m_flags &= ~0x80000000u;
					i1 = RepositionEntity(pent,*(int*)(pQueueSlots[i]+j+sizeof(int)*2+sizeof(void*)),0,1);
					AtomicAdd(&m_lockGrid, -i1);
					if (++m_nEnts > m_nEntsAlloc-1) {
						m_nEntsAlloc += 4096; m_nEntListAllocs++; m_bEntityCountReserved = 0;
						ReallocateList(m_pTmpEntList,m_nEnts-1,m_nEntsAlloc);
						ReallocateList(m_pTmpEntList1,m_nEnts-1,m_nEntsAlloc);
						ReallocateList(m_pTmpEntList2,m_nEnts-1,m_nEntsAlloc);
						ReallocateList(m_pGroupMass,m_nEnts-1,m_nEntsAlloc);
						ReallocateList(m_pMassList,m_nEnts-1,m_nEntsAlloc);
						ReallocateList(m_pGroupIds,m_nEnts-1,m_nEntsAlloc);
						ReallocateList(m_pGroupNums,m_nEnts-1,m_nEntsAlloc);
					}	break;
				case 5:
					DestroyPhysicalEntity((IPhysicalEntity*)pent, *(int*)(pQueueSlots[i]+j+sizeof(int)*2+sizeof(void*)),1);
			}
			if (iter!=4)
				AtomicAdd(&pent->m_bProcessed,-PENT_QUEUED);
		}
		if (nQueueSlots) {
			m_nQueueSlotsAux=1; m_nQueueSlotSizeAux=0; *(int*)m_pQueueSlotsAux[0]=-1;
		}
		m_grpProfileData[14].nTicksLast = CryGetTicks()-timer;
	}
	{ 
	WriteLockCond lock1(m_lockCaller[MAX_PHYS_THREADS], flags & ent_flagged_only);
	WriteLock lock(m_lockStep);
	if (time_interval>0 && !(flags & ent_flagged_only))
		MarkAsPhysThread();
	volatile int64 timer=CryGetTicks();

	if (time_interval > m_vars.maxWorldStep)
		time_interval = time_interval_org = m_vars.maxWorldStep;

	if (m_vars.timeGranularity>0) {
		i = float2int(time_interval_org*(m_vars.rtimeGranularity=1.0f/m_vars.timeGranularity));
		time_interval_org = time_interval = i*m_vars.timeGranularity;
		m_iTimePhysics += i;
		m_timePhysics = m_iTimePhysics*m_vars.timeGranularity;
	}	else
		m_timePhysics += time_interval;
	if (m_vars.fixedTimestep>0 && time_interval>0)
		time_interval = m_vars.fixedTimestep;
	m_bUpdateOnlyFlagged = flags & ent_flagged_only;
	bSkipFlagged = flags>>1 & pef_update;
	m_bWorldStep = 1;
	m_vars.bMultiplayer = gEnv->bMultiplayer;
	m_vars.bUseDistanceContacts &= m_vars.bMultiplayer^1;
	m_lastTimeInterval = time_interval;
	if (!m_bUpdateOnlyFlagged)
		m_vars.lastTimeStep = time_interval;
	if (m_pGlobalArea && !is_unused(m_pGlobalArea->m_gravity))
		m_pGlobalArea->m_gravity = m_vars.gravity;
	m_rq.time_interval=time_interval; m_rq.bSkipFlagged=bSkipFlagged;	m_rq.pbAllGroupsFinished=&bAllGroupsFinished;
	m_pMovedEnts = 0;

	if (flags & ent_living) {
		for(pent=m_pTypedEnts[3]; pent; pent=pent->m_next) if (!(m_bUpdateOnlyFlagged&(pent->m_flags^pef_update) | bSkipFlagged&pent->m_flags))
			pent->StartStep(time_interval_org*m_vars.timeScalePlayers); // prepare to advance living entities
	}

	if (!m_vars.bSingleStepMode || m_vars.bDoStep) {
		i = m_vars.timeScalePlayers!=1.0f && time_interval>0.0f;
		if ((m_nSlowFrames = (m_nSlowFrames & -i)+i)>4 && m_iLastLogPump<m_vars.nStartupOverloadChecks) {
			// force-freeze dynamic ents in cases of massive slowdowns
			pe_status_dynamics sd;
			for(pent=m_pTypedEnts[2]; pent; pent=pent->m_next) if (pent->GetStatus(&sd) && sd.v.len2()<sqr(5.0f))
				pent->Awake(0);
			for(pent=m_pTypedEnts[4]; pent; pent=pent->m_next)
				pent->Awake(0);
		}

		{ SBreakRequest curreq;
			do {
				{ ReadLock lockbq(m_lockBreakQueue);
					if (m_breakQueueSz==0)
						break;
					curreq = m_breakQueue[m_breakQueueTail];
					m_breakQueueTail = m_breakQueueTail+1 - (m_breakQueueAlloc & m_breakQueueAlloc-2-m_breakQueueTail>>31);
					m_breakQueueSz--;
				}
				if (curreq.pent->m_iSimClass!=7) {
					int ipart;
					for(ipart=curreq.pent->m_nParts-1; ipart>=0 && curreq.pent->m_parts[ipart].id!=curreq.partid; ipart--);
					if (ipart>=0 && DeformEntityPart(curreq.pent,ipart, &curreq.expl, curreq.gwd,curreq.gwd+1) &&
							curreq.pent->UpdateStructure(0.01f,&curreq.expl,-1,curreq.gravity))
						MarkEntityAsDeforming(curreq.pent);
				}
				curreq.pent->Release();
			} while(true);
		}

		iter = 0;	__curstep++;
		if (!(__curstep & 7))
			memset(g_idata[0].UsedNodesMap, 0, sizeof(g_idata[0].UsedNodesMap));
		if (flags & ent_independent) {
			for(pent=m_pTypedEnts[4]; pent; pent=pent->m_next) if (!(m_bUpdateOnlyFlagged&(pent->m_flags^pef_update) | bSkipFlagged&pent->m_flags))
				pent->StartStep(time_interval);
		}
		m_grpProfileData[13].nCallsLast = 0;

		if (flags & ent_rigid && time_interval>0) {
			if (m_pTypedEnts[2]) do { // make as many substeps as required
				bAllGroupsFinished = 1;	m_pAuxStepEnt = 0;
				m_pGroupNums[m_nEntsAlloc-1] = -1; // special group for rigid bodies w/ infinite mass
				m_threadData[0].bGroupInvisible = 0;
				m_iSubstep++;

				for(ipass=0; ipass<2; ipass++) {
					// build lists of intercolliding groups of entities
					for(pent=m_pTypedEnts[2],nGroups=0,m_threadData[0].groupMass=1.0f; pent; pent=pent_next) {
						pent_next = pent->m_next;
						if (!(pent->m_bMoved | m_bUpdateOnlyFlagged&(pent->m_flags^pef_update) | bSkipFlagged&pent->m_flags)) {
							if (pent->GetMassInv()<=0) {
								if ((iter|ipass)==0) { // just make isolated step for rigids with infinite mass
									pent->StartStep(time_interval);
									pent->m_iGroup = -1;//m_nEntsAlloc-1;
								}
								if (ipass==0)	{
									pent->Step(/*m_groupTimeStep = */pent->GetMaxTimeStep(time_interval));
									bAllGroupsFinished &= pent->Update(time_interval,1);
								}
							} else {
								pent->m_iGroup = nGroups; pent->m_bMoved = 1;	m_pGroupIds[nGroups] = 0;
								m_pGroupMass[nGroups] = 1.0f/pent->GetMassInv();
								if ((iter | ipass)==0) pent->StartStep(time_interval);
								pent->m_next_coll1 = pent->m_next_coll = 0;
								m_pTmpEntList1[nGroups] = 0;
								// initially m_pTmpEntList1 points to group entities that collide with statics (sorted by mass) - linked via m_next_coll
								// m_next_coll1 maintains a queue of current intercolliding objects

								for(phead=ptail=pentmax=pent; phead; phead=phead->m_next_coll1) {
									for(i=bHeadAdded=0,n=phead->GetColliders(pentlist); i<n; i++) if (pentlist[i]->GetMassInv()<=0) {
										if (!bHeadAdded) {
											for(pent1=m_pTmpEntList1[nGroups]; pent1 && pent1->m_next_coll && pent1->m_next_coll->GetMassInv()<=phead->GetMassInv();
													pent1=pent1->m_next_coll);
											if (!pent1 || pent1->GetMassInv()>phead->GetMassInv()) {
												phead->m_next_coll = pent1; m_pTmpEntList1[nGroups] = phead;
											} else {
												phead->m_next_coll = pent1->m_next_coll; pent1->m_next_coll = phead;
											}
											bHeadAdded = 1;
										}
										m_pGroupIds[nGroups] = 1; // tells that group has static entities
									} else if (!(pentlist[i]->m_bMoved | m_bUpdateOnlyFlagged & (pentlist[i]->m_flags^pef_update))) {
										pentlist[i]->m_flags &= ~bSkipFlagged;
										ptail->m_next_coll1 = pentlist[i]; ptail = pentlist[i]; ptail->m_next_coll1 = 0;
										ptail->m_next_coll = 0;
										if ((iter | ipass)==0) ptail->StartStep(time_interval);
										ptail->m_iGroup = nGroups; ptail->m_bMoved = 1;
										m_pGroupMass[nGroups] += 1.0f/(m=ptail->GetMassInv());
										if (pentmax->GetMassInv()>m)
											pentmax = ptail;
									}
								}
								if (!m_pTmpEntList1[nGroups])
									m_pTmpEntList1[nGroups] = pentmax;
								nGroups++;
							}
						}
					}

					// add maximum group mass to all groups that contain static entities
					for(i=1,m=m_pGroupMass[0]; i<nGroups; i++) m = max(m,m_pGroupMass[i]);
					for(m*=1.01f,i=0; i<nGroups; i++) m_pGroupMass[i] += m*m_pGroupIds[i];
					for(i=0;i<nGroups;i++) m_pGroupIds[i] = i;

					// sort groups by decsending group mass
          qsort(m_pTmpEntList1,m_pGroupMass,m_pGroupIds, 0,nGroups-1);
					for(i=0;i<nGroups;i++) m_pGroupNums[m_pGroupIds[i]] = i;

					for(i=0;i<nGroups;i++) {
						for(ptail=m_pTmpEntList1[i]; ptail->m_next_coll; ptail=ptail->m_next_coll)
							ptail->m_bMoved=0; ptail->m_bMoved=0;
						for(phead=m_pTmpEntList1[i]; phead; phead=phead->m_next_coll)
							for(j=0,n=phead->GetColliders(pentlist); j<n; j++) if (pentlist[j]->GetMassInv()>0) {
								if (pentlist[j]->m_bMoved==1 && !(m_bUpdateOnlyFlagged & (pentlist[j]->m_flags^pef_update))) {
									ptail->m_next_coll = pentlist[j]; ptail = pentlist[j]; ptail->m_next_coll = 0;
									ptail->m_bMoved = 0;
								}
							}
					}

					m_nGroups=nGroups; m_maxGroupMass=m;
					m_rq.iter=iter;	m_iCurGroup=0;
					THREAD_TASK(ipass, ProcessNextEntityIsland(time_interval,ipass,iter,bAllGroupsFinished,0));

					if (ipass==0) {
						for(i=0;i<m_nGroups;i++)
							for(pent=m_pTmpEntList1[i]; pent; pent=pent->m_next_coll)
								pent->m_bMoved=0, pent->m_iGroup=-1;
						m_bWorldStep = 2;
						for(i=0,pent=m_pAuxStepEnt; pent; pent=pent->m_next_coll2,i++) if (pent->GetType()==PE_LIVING)
							((CLivingEntity*)pent)->SyncWithGroundCollider(pent->m_timeIdle);
						m_pCurEnt = GroupByOuterEntity(m_pAuxStepEnt); m_pAuxStepEnt = 0;
						THREAD_TASK(2, ProcessNextEngagedIndependentEntity(0))
						m_bWorldStep = 1;
					}	else
						m_grpProfileData[13].nCallsLast = max(m_grpProfileData[13].nCallsLast,m_nGroups);
				}
			} while (!bAllGroupsFinished && ++iter<m_vars.nMaxSubsteps);

			for(pent=(CPhysicalEntity*)m_pMovedEnts; pent; pent=pent_next) {
				pent_next = pent->m_next_coll2;
				pent->m_bMoved=0, pent->m_iGroup=-1; pent->m_next_coll2=0;
			}
			for(pent=m_pTypedEnts[4]; pent; pent=pent->m_next) pent->m_bMoved=0,pent->m_iGroup=-1;
			m_updateTimes[1] = m_updateTimes[2] = m_timePhysics;
		}

		{ ReadLock lockwm(m_lockWaterMan);
			for(CWaterMan *pWaterMan=m_pWaterMan; pWaterMan; pWaterMan=pWaterMan->m_next)
				pWaterMan->TimeStep(time_interval);
		}
		for(i=0;i<m_nProfiledEnts;i++) {
			m_pEntProfileData[i].nTicksStep &= -m_pEntProfileData[i].nTicks>>31;
			m_pEntProfileData[i].nTicksLast=m_pEntProfileData[i].nTicks; m_pEntProfileData[i].nTicks=0;
			m_pEntProfileData[i].nCallsLast=m_pEntProfileData[i].nCalls; m_pEntProfileData[i].nCalls=0;
		}
	}
	m_iSubstep++;

	if (flags & ent_living) {
		m_pCurEnt = m_pTypedEnts[3];
		THREAD_TASK(3, ProcessNextLivingEntity(time_interval,bSkipFlagged,0))
		/*for(pent=m_pTypedEnts[3]; pent; pent=pent_next) {
			pent_next = pent->m_next;
			if (!(m_bUpdateOnlyFlagged&(pent->m_flags^pef_update) | bSkipFlagged&pent->m_flags))
				pent->Step(pent->GetMaxTimeStep(time_interval_org*m_vars.timeScalePlayers)); // advance living entities
		}*/
		m_updateTimes[3] = m_timePhysics;
	}

	m_bWorldStep = 3;
	if (!m_vars.bSingleStepMode || m_vars.bDoStep) {
		if (flags & ent_independent) {
			for(pent=m_pTypedEntsPerm[4]; pent; pent=pent->m_next)
				pent->m_next_coll2 = pent->m_next;
			m_pCurEnt = GroupByOuterEntity(m_pTypedEntsPerm[4]);
			THREAD_TASK(4, ProcessNextIndependentEntity(time_interval,bSkipFlagged,0));
			m_updateTimes[4] = m_timePhysics;
		}
	}

	if (flags & ent_deleted) {
		if (!m_vars.bSingleStepMode || m_vars.bDoStep) {
			// process deforming (breaking) enities
			{
				WriteLock lock3(m_lockDeformingEntsList);
				for(i=j=0; i<m_nDeformingEnts; i++) if (m_pDeformingEnts[i]->m_iSimClass!=7 && m_pDeformingEnts[i]->UpdateStructure(time_interval,0))
					m_pDeformingEnts[j++] = m_pDeformingEnts[i];
				else
					m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
				m_nDeformingEnts = j;
				m_updateTimes[0] = m_timePhysics;
			}

			CleanseEventsQueue(); // remove events that reference deleted entities

			for(pent=m_pTypedEnts[7]; pent; pent=pent_next) { // purge deletion requests
				pent_next = pent->m_next;
				if (m_iLastLogPump>=pent->m_iDeletionTime && pent->m_nRefCount<=0)	{
					if (pent->m_next) pent->m_next->m_prev = pent->m_prev;
					(pent->m_prev ? pent->m_prev->m_next : m_pTypedEnts[7]) = pent->m_next;
					pent->Delete();
				}
			}
			//m_pTypedEnts[7] = 0;
		}

		// flush timeouted sectors for cell-based physics-on-demand
		{ WriteLock lockPOD(m_lockPODGrid);
			MarkAsPODThread(this);
			for(SEntityGrid *pGrid=&m_entgrid; pGrid; pGrid=pGrid->m_next) { 
				pe_PODcell *pPODcell;
				int *picellNext;
				for(i=pGrid->iActivePODCell0,picellNext=&pGrid->iActivePODCell0; i>=0; i=pPODcell->inextActive)
					if (((pPODcell=pGrid->getPODcell(i&0xFFFF,i>>16))->lifeTime-=time_interval_org)<=0 || pPODcell->lifeTime>1E9f) {
						Vec3 center,sz;	++m_iLastPODUpdate;
						pGrid->GetPODGridCellBBox(i&0xFFFF,i>>16, center,sz);
						m_pPhysicsStreamer->DestroyPhysicalEntitiesInBox(center-sz,center+sz);
						*picellNext = pPODcell->inextActive;
					} else picellNext = &pPODcell->inextActive;
			}
			UnmarkAsPODThread(this);
		}

		// flush static and sleeping physical objects that have timeouted
		for(i=0;i<2;i++) for(pent=m_pTypedEnts[i]; pent && pent!=m_pTypedEntsPerm[i]; pent=pent_next) {
			pent_next = pent->m_next;
			{ WriteLock lockEnt(pent->m_lockUpdate);
				for(j=0;j<pent->m_nParts && !(pent->m_parts[j].flags & geom_can_modify);j++);
				if (j<pent->m_nParts || pent->m_pStructure && pent->m_pStructure->bModified)
					j=-1;
			}
			if (j==-1) {
				j -= pent_next==m_pTypedEntsPerm[i];
				pent->m_bPermanent = 1;
				ChangeEntitySimClass(pent, 0);
				if (pent->m_pEntBuddy) {
					CPhysicalPlaceholder *ppc = pent->m_pEntBuddy;
					ppc->m_pEntBuddy=0;
					ppc->m_iGThunk0=0;
					SetPhysicalEntityId(ppc,-1,1,1); ppc->m_id=-1; pent->m_pEntBuddy = 0;
					SetPhysicalEntityId(pent,pent->m_id,1,1);
					for(i1=pent->m_iGThunk0;i1;i1=m_gthunks[i1].inextOwned) m_gthunks[i1].pent=pent;
					DestroyPhysicalEntity(ppc,0,1);
				}
				if (j==-2)
					break;
			}	else if (pent->m_nRefCount==0 && ((pent->m_timeIdle+=time_interval_org)>pent->m_maxTimeIdle || pent->m_timeIdle<0))
				DestroyPhysicalEntity(pent,0,1);
		}

		for(pent=m_pTypedEnts[2]; pent!=m_pTypedEntsPerm[2]; pent=pent->m_next)
			pent->m_timeIdle = 0;	// reset idle count for active physical entities

		/*for(pent=m_pTypedEnts[4]; pent!=m_pTypedEntsPerm[4]; pent=pent_next) {
			assert(pent);
			pent_next = pent->m_next;
			if (pent->IsAwake())
				pent->m_timeIdle = 0;	// reset idle count for active detached entities
			else if (pent->m_nRefCount==0 && (pent->m_timeIdle+=time_interval_org)>pent->m_maxTimeIdle)
				DestroyPhysicalEntity(pent);
		}*/

		{ // update active areas
			CPhysArea *pArea,*pNextArea;
			for(pArea=m_pActiveArea,m_pActiveArea=0; pArea; pArea=pNextArea) {
				pArea->Update(time_interval); pNextArea=pArea->m_nextActive; pArea->m_nextActive=pArea;
			}
		}
		// flush deleted areas
		{ WriteLock lockAreas(m_lockAreas);
			CPhysArea *pArea,**ppNextArea=&m_pDeletedAreas;
			for(pArea=m_pDeletedAreas; pArea; pArea=*ppNextArea) if (pArea->m_lockRef==0) {
				*ppNextArea=pArea->m_next; delete pArea;
			}	else
				ppNextArea = &pArea->m_next;
		}
		for(SEntityGrid *pgrid=m_pDeletedGrids; pgrid; pgrid=DestroyGrid(pgrid));
		m_pDeletedGrids = nullptr;

		// invalidate the precomputed bv data for ropes
		for(i=0;i<=MAX_PHYS_THREADS;i++) m_threadData[i].pTmpPartBVListOwner=0;

		m_updateTimes[7] = m_timePhysics;
		if (m_vars.bDoStep==2) {
			m_vars.bDoStep = 0;
			SerializeWorld("worldents.txt",1);
			SerializeGeometries("worldgeoms.txt",1);
		}	else if (m_vars.bDoStep==3) {
			m_vars.bDoStep = 0;
			SerializeWorld("world.phump",3);
		}
		m_vars.bDoStep = 0;
	}
	m_bUpdateOnlyFlagged = 0;
	m_bWorldStep = 0;
	if (time_interval>0)
		++m_idStep;

	if (m_vars.bProfileGroups) {
		for(i=0,m_grpProfileData[12].nTicksLast=0; i<=11; i++) {
			m_grpProfileData[12].nTicksLast += (m_grpProfileData[i].nTicksLast=m_grpProfileData[i].nTicks);
			m_grpProfileData[i].nTicks=0; m_grpProfileData[i].nCallsLast=0;
		}
		for(pent=m_pTypedEnts[2]; pent; pent=pent->m_next)
			m_grpProfileData[GetEntityProfileType(pent)].nCallsLast++;
		for(pent=m_pTypedEnts[3]; pent; pent=pent->m_next)
			m_grpProfileData[PE_LIVING-2].nCallsLast++;
		for(pent=m_pTypedEntsPerm[4]; pent; pent=pent->m_next)
			m_grpProfileData[GetEntityProfileType(pent)].nCallsLast++;
		m_grpProfileData[13].nTicksLast = m_grpProfileData[13].nTicks;
		m_grpProfileData[13].nTicks = 0;
		m_grpProfileData[15].nTicksLast = CryGetTicks()-timer;
	}
	} // m_lockStep
	
	FlushOldThunks();
}

void CPhysicalWorld::FlushOldThunks()
{
	WriteLock lock(m_lockOldThunks);
	pe_gridthunk *pthunks=m_oldThunks,*pthunksNext;
	for(; pthunks; pthunks=pthunksNext) {
		pthunksNext = *(pe_gridthunk**)pthunks;
		delete[] pthunks;
	}
	m_oldThunks = 0;

	geom *pparts=m_oldEntParts,*ppartsNext;
	for(m_oldEntParts=0; pparts; pparts=ppartsNext) {
		ppartsNext = (geom*)pparts->pLattice;
		delete[] pparts;
	}

	FlushOldGeoms();
}

#if 0 && CAPTURE_REPLAY_LOG
static int s_numThunksAllocated = 0;

NO_INLINE void TrackThunkUsageAlloc(int thunk)
{
	s_numThunksAllocated++;
	if (CryGetIMemReplay()->MemStatEnterAlloc())
		CryGetIMemReplay()->MemStatLeaveAlloc((void*)(thunk + 0xf0000000), sizeof(pe_gridthunk));
}

NO_INLINE void TrackThunkUsageFree(int thunk)
{
	s_numThunksAllocated--;
	if (CryGetIMemReplay()->MemStatEnterFree())
		CryGetIMemReplay()->MemStatLeaveFree((void*)(thunk + 0xf0000000));
}
#else // CAPTURE_REPLAY_LOG
#define TrackThunkUsageAlloc(thunk)
#define TrackThunkUsageFree(thunk)
#endif // CAPTURE_REPLAY_LOG

#if defined(__GNUC__)
#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
	#pragma GCC diagnostic ignored "-Woverflow"
#else
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Woverflow"
#endif
#endif
void CPhysicalWorld::DetachEntityGridThunks(CPhysicalPlaceholder *pobj)
{
	if (pobj->m_iGThunk0) {
		SEntityGrid &grid = *GetGrid(pobj);
		int ithunk,ithunk_next,ithunk_last,icell,iprev,inext;
		for(ithunk=pobj->m_iGThunk0; ithunk; ithunk=ithunk_next) {
			TrackThunkUsageFree(ithunk);
			ithunk_next = m_gthunks[ithunk].inextOwned;
			iprev=m_gthunks[ithunk].iprev; inext=m_gthunks[ithunk].inext;
			m_gthunks[ithunk].pent = 0;
#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wconstant-conversion"
#endif
			m_gthunks[ithunk].inext=m_gthunks[ithunk].iprev = -1;
#if defined(__clang__)
	#pragma clang diagnostic pop
#endif
			m_gthunks[inext].iprev = iprev & -(int)inext>>31;
			m_gthunks[inext].bFirstInCell = m_gthunks[ithunk].bFirstInCell;
			if (m_gthunks[ithunk].bFirstInCell) {
				icell = grid.size.x*grid.size.y;
				if (grid.cells[icell]!=ithunk)
					icell = Vec2i(iprev&1023,iprev>>10&1023)*grid.stride;
				grid.cells[(unsigned int)icell] = inext;
			}	else
				m_gthunks[iprev].inext = inext;
			ithunk_last = ithunk;
		}
		m_gthunks[ithunk_last].inextOwned = m_iFreeGThunk0;
		m_iFreeGThunk0 = pobj->m_iGThunk0;
		pobj->m_iGThunk0 = 0;
	}
}
#if defined(__GNUC__)
#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
	#pragma GCC diagnostic error "-Woverflow"
#else
	#pragma GCC diagnostic pop
#endif
#endif

void CPhysicalWorld::SortThunks()
{
	if (m_thunkPoolSz == 0)
		return;

	int i,j,icell,nthunks=1;
	int *new2old=new int[m_thunkPoolSz], *old2new=new int[m_thunkPoolSz];
	SEntityGrid *pgrid;
	WriteLockCond lock(m_lockGrid);

	for(pgrid=&m_entgrid; pgrid; pgrid=pgrid->m_next)
		for(icell=0; icell<=pgrid->size.x*pgrid->size.y; icell++)
			for(i=pgrid->cells[icell]; i; i=m_gthunks[i].inext)
				new2old[nthunks++] = i;
	for(i=m_iFreeGThunk0; i; i=m_gthunks[i].inextOwned) {
		PREFAST_SUPPRESS_WARNING(6386)
		new2old[nthunks++] = i;
	}
	assert(nthunks==m_thunkPoolSz);
	PREFAST_ASSUME(nthunks==m_thunkPoolSz);
	for(i=1,old2new[0]=0; i<nthunks; i++) old2new[new2old[i]] = i;
	for(i=0; i<nthunks; i++) {
		if (m_gthunks[i].pent) {
			m_gthunks[i].inext = old2new[m_gthunks[i].inext];
			if (m_gthunks[i].bFirstInCell) {
				pgrid = GetGrid(m_gthunks[i].pent);
				icell = Vec2i(max(0,min(pgrid->size.x-1,(int)m_gthunks[i].iprev&1023)), max(0,min(pgrid->size.y-1,(int)m_gthunks[i].iprev>>10&1023)))*pgrid->stride;
				if (pgrid->cells[(unsigned int)icell]!=i)
					icell = pgrid->size.x*pgrid->size.y;
				pgrid->cells[(unsigned int)icell] = old2new[i];
			}	else
				m_gthunks[i].iprev = old2new[m_gthunks[i].iprev];
		}
		m_gthunks[i].inextOwned = old2new[m_gthunks[i].inextOwned];
	}
	for(i=1; i<nthunks; i++) if (m_gthunks[i].pent && !(m_gthunks[i].pent->m_bProcessed & 1<<31)) {
		int idxNew = old2new[m_gthunks[i].pent->m_iGThunk0];
		if (m_gthunks[i].pent->m_pEntBuddy && m_gthunks[i].pent->m_pEntBuddy->m_iGThunk0==m_gthunks[i].pent->m_iGThunk0)
			m_gthunks[i].pent->m_pEntBuddy->m_iGThunk0 = idxNew;
		m_gthunks[i].pent->m_iGThunk0 = idxNew;
		m_gthunks[i].pent->m_bProcessed |= 1<<31;
	}
	for(i=1; i<nthunks; i++) if (m_gthunks[i].pent)
		m_gthunks[i].pent->m_bProcessed &= ~(1<<31);
	m_iFreeGThunk0 = old2new[m_iFreeGThunk0];

	for(i=1; i<nthunks; i++) if (old2new[i]!=i) {
		pe_gridthunk thunk = m_gthunks[i]; j = old2new[i];
		do {
			pe_gridthunk thunkNext = m_gthunks[j];
			int jnext = old2new[j];
			m_gthunks[j] = thunk; old2new[j] = j;
			if (j==i)
				break;
			thunk = thunkNext; j = jnext;
		}	while(true);
	}
	//for(i=0;i<nthunks && old2new[i]==i;i++);
	//assert(i==nthunks);
	delete[] old2new; delete[] new2old;
}


int CPhysicalWorld::ChangeEntitySimClass(CPhysicalEntity *pent, int bGridLocked)
{
	// This function should NEVER be called from a non-physics thread
	//
	// Uncomment this code here to force a crash to catch incorrect accesses
	//if (get_iCaller()==MAX_PHYS_THREADS && m_bWorldStep>0 && m_vars.lastTimeStep>0)
	//{
	//	volatile int *p = NULL;
	//	*p = 10;
	//}

	if ((unsigned int)(pent->m_iPrevSimClass-0x100)<8u) {
		VALIDATOR_LOG(m_pLog, "Error: *major* problem - trying to change iSimClass of a hidden entity!");
		if (m_vars.iDrawHelpers & 0x100<<ent_rigid) DoBreak
		return 0;
	}
	{
		WriteLock lock(m_lockList);

		const unsigned int iSimClass=pent->m_iSimClass, iPrevSimClass=pent->m_iPrevSimClass;
		CPhysicalEntity *pent0=pent,*pent1=pent;
		int bPermanent = pent->m_bPermanent;
		if (iSimClass==4 && iPrevSimClass==4 && pent->m_pOuterEntity) {
			for(; pent0->m_prev && pent0->m_prev->m_pOuterEntity==pent->m_pOuterEntity; bPermanent|=(pent0=pent0->m_prev)->m_bPermanent);
			for(; pent1->m_next && pent1->m_next->m_pOuterEntity==pent->m_pOuterEntity; bPermanent|=(pent1=pent1->m_next)->m_bPermanent);
		}
		if (pent->m_iPrevSimClass==iSimClass && pent->m_bPrevPermanent==bPermanent)
			return 0;

		if (iPrevSimClass<8u) {
			if (pent1->m_next) pent1->m_next->m_prev = pent0->m_prev;
			(pent0->m_prev ? pent0->m_prev->m_next : m_pTypedEnts[iPrevSimClass]) = pent1->m_next;
			for(CPhysicalEntity *penti=pent0; penti!=pent1->m_next; penti=penti->m_next)
				if (penti==m_pTypedEntsPerm[iPrevSimClass])	{
					m_pTypedEntsPerm[iPrevSimClass] = pent1->m_next; break;
				}
		}

		PREFAST_ASSUME(pent0);
		if (!bPermanent) {
			pent1->m_next = m_pTypedEnts[pent->m_iSimClass];
			pent0->m_prev = 0;
			if (pent1->m_next) pent1->m_next->m_prev = pent1;
			m_pTypedEnts[iSimClass] = pent0;
		} else {
			pent1->m_next = m_pTypedEntsPerm[iSimClass];
			if (m_pTypedEntsPerm[iSimClass]) {
				if (pent0->m_prev = m_pTypedEntsPerm[iSimClass]->m_prev)
					pent0->m_prev->m_next = pent0;
				pent1->m_next->m_prev = pent1;
			} else if (m_pTypedEnts[iSimClass]) {
				for(pent0->m_prev=m_pTypedEnts[iSimClass]; pent0->m_prev->m_next; pent0->m_prev=pent0->m_prev->m_next);
				pent0->m_prev->m_next = pent0;
			} else
				pent0->m_prev = 0;
			if (m_pTypedEntsPerm[iSimClass]==m_pTypedEnts[iSimClass])
				m_pTypedEnts[iSimClass] = pent0;
			m_pTypedEntsPerm[iSimClass] = pent0;
		}
		for(; pent0!=pent1; pent0=pent0->m_next) {
			pent0->m_bPrevPermanent = bPermanent;
			pent0->m_iPrevSimClass = iSimClass;
		}
		pent1->m_bPrevPermanent = bPermanent;
		pent1->m_iPrevSimClass = iSimClass;
	}

	{
		ReadLockCond gridLock(m_lockGrid, !bGridLocked);
		for(int ithunk=pent->m_iGThunk0; ithunk; ithunk=m_gthunks[ithunk].inextOwned)
			m_gthunks[ithunk].iSimClass = pent->m_iSimClass;
	}
	return 1;
}


int CPhysicalWorld::GetFreeThunk()
{
	if (!m_iFreeGThunk0) {
		if (m_thunkPoolSz>=1<<20) {
			static bool g_bSpammed = false;
			if (!g_bSpammed)
				VALIDATOR_LOG(m_pLog, "Error: too many entity grid thunks created, further repositions ignored");
			g_bSpammed = true;
			return 0;
		}
		const int increaseThunks = (64*1024)/sizeof(pe_gridthunk);	// Increase in multiples of 64K to avoid fragmentation
		AllocGThunksPool(m_thunkPoolSz+increaseThunks);
	}
	int ithunk = m_iFreeGThunk0; 
	m_iFreeGThunk0 = m_gthunks[m_iFreeGThunk0].inextOwned;
	return ithunk;
}


int CPhysicalWorld::RepositionEntity(CPhysicalPlaceholder *pobj, int flags, Vec3 *BBox, int bQueued)
{
	SEntityGrid *pgrid = GetGrid(pobj);
	int i,j,igx[2],igy[2],igxInner[2],igyInner[2],igz[2],ix,iy,ithunk,ithunk0;
	unsigned int n;
	if ((unsigned int)pobj->m_iSimClass>=7u) return 0; // entity is frozen
	int bGridLocked = 0;
	int bBBoxUpdated = 0;
	EventPhysStateChange event;

	if (flags&1 && pgrid->cells) {
		i = -iszero((INT_PTR)BBox);
		Vec3 *pBBox = (Vec3*)((INT_PTR)pobj->m_BBox & (INT_PTR)i | (INT_PTR)BBox & ~(INT_PTR)i);
		Vec3 gBBox[2]; pgrid->BBoxToGrid(pBBox[0],pBBox[1], gBBox);
		for(i=0;i<2;i++) {
			float x = gBBox[i].x*pgrid->stepr.x;
			igx[i] = pgrid->crop(float2int(x-0.5f),0);
			igxInner[i] = max(0,min(255,float2int((x-igx[i])*256.0f-0.5f)));
			x = gBBox[i].y*pgrid->stepr.y;
			igy[i] = pgrid->crop(float2int(x-0.5f),1);
			igyInner[i] = max(0,min(255,float2int((x-igy[i])*256.0f-0.5f)));
			igz[i] = (int)(gBBox[i].z*pgrid->rzGran)+i;
		}
		if (pobj->m_ig[0].x!=NO_GRID_REG) // if m_igx[0] is NO_GRID_REG, the entity should not be registered in grid at all
			if (igx[0]-pobj->m_ig[0].x | igy[0]-pobj->m_ig[0].y | igx[1]-pobj->m_ig[1].x | igy[1]-pobj->m_ig[1].y | flags&4 | flags>>2&1 ^ pobj->m_bOBBThunks) {
				CPhysicalPlaceholder *pcurobj = pobj;
				int moveToEnd = -iszero(pobj->m_ig[0].x-GRID_REG_LAST);
				WriteLockCond lockGrid(m_lockGrid); 
				if (pobj->m_pEntBuddy && pobj->m_pEntBuddy->IsPlaceholder())	{
					flags|=8; goto skiprepos; //pcurobj = pobj->m_pEntBuddy;
				}
				m_bGridThunksChanged = 1;
				DetachEntityGridThunks(pobj);
				n = (igx[1]-igx[0]+1)*(igy[1]-igy[0]+1);
				if (pobj->m_iSimClass!=5) {
					if (n==0 || n>(unsigned int)m_vars.nMaxEntityCells) {
						Vec3 pos = (pcurobj->m_BBox[0]+pcurobj->m_BBox[1])*0.5f;
						char buf[256];
						cry_sprintf(buf,"Error: %s @ %.1f,%.1f,%.1f is too large or invalid", !m_pRenderer ? "entity" :
							m_pRenderer->GetForeignName(pcurobj->m_pForeignData,pcurobj->m_iForeignData,pcurobj->m_iForeignFlags), pos.x,pos.y,pos.z);
						VALIDATOR_LOG(m_pLog,buf);
						if (m_vars.bBreakOnValidation) DoBreak
						pobj->m_ig[0].x=pobj->m_ig[1].x=pobj->m_ig[0].y=pobj->m_ig[1].y = GRID_REG_PENDING;
						flags|=8; goto skiprepos;
					}
				} else if (n>(unsigned int)m_vars.nMaxAreaCells)
					return -1;

				if (pgrid!=&m_entgrid && pcurobj->m_iSimClass>0 && (ithunk=GetFreeThunk())) {
					pe_gridthunk &thunk = m_gthunks[ithunk], &thunk0 = m_gthunks[pgrid->m_iGThunk0];
					thunk.inextOwned = pcurobj->m_iGThunk0;
					pcurobj->m_iGThunk0 = ithunk;
					thunk.iSimClass = pcurobj->m_iSimClass;
					thunk.pent = pcurobj;
					thunk.bFirstInCell = 0;
					if (thunk.inext = thunk0.inext)
						m_gthunks[thunk0.inext].iprev = ithunk; 
					thunk.iprev = pgrid->m_iGThunk0;
					thunk0.inext = ithunk;
				}

				for(ix=igx[0];ix<=igx[1];ix++) for(iy=igy[0];iy<=igy[1];iy++) {
					if ((flags&4) && (ix|iy|pgrid->size.x-1-ix|pgrid->size.y-1-iy)>=0) {
						Diag33 gscale(pgrid->step.x, pgrid->step.y, pgrid->zGran);
						AABB bbox; pgrid->BBoxFromGrid(gscale*Vec3(ix,iy,igz[0]), gscale*Vec3(ix+1,iy+1,igz[1]), (Vec3*)&bbox);
						if (!((CPhysicalEntity*)pobj)->OccupiesEntityGridSquare(bbox))
							continue;
					}
					j = pgrid->getcell_safe(ix,iy);
					if (!(ithunk = GetFreeThunk())) {
						flags|=8; goto skiprepos;
					}
					TrackThunkUsageAlloc(ithunk);
					pe_gridthunk * __restrict pNewGThunk = &m_gthunks[ithunk];

					pNewGThunk->inextOwned = pcurobj->m_iGThunk0;
					pcurobj->m_iGThunk0 = ithunk;
					pcurobj->m_bOBBThunks = flags>>2 & 1;

					int ithunkGrid = pgrid->cells[j];

					if (!ithunkGrid || m_gthunks[ithunkGrid].iSimClass-5 & ~moveToEnd) {
						int& entGridEntry = pgrid->cells[(unsigned int)j];
						pNewGThunk->bFirstInCell = 1;
						pNewGThunk->iprev = ((iy & pgrid->size.y-1)<<10|ix & pgrid->size.x-1);
						pNewGThunk->inext = entGridEntry;
						m_gthunks[ithunkGrid].iprev = ithunk & -ithunkGrid>>31;
						m_gthunks[ithunkGrid].bFirstInCell = 0;
						entGridEntry = ithunk;
					}	else {
						for(ithunk0=ithunkGrid; iszero((int)m_gthunks[m_gthunks[ithunk0].inext].iSimClass-5) | moveToEnd & m_gthunks[ithunk0].inext; ithunk0=m_gthunks[ithunk0].inext);
						pNewGThunk->bFirstInCell = 0;
						pNewGThunk->inext = m_gthunks[ithunk0].inext;
						pNewGThunk->iprev = ithunk0;
						m_gthunks[m_gthunks[ithunk0].inext].iprev = ithunk & -(int)m_gthunks[ithunk0].inext>>31;
						m_gthunks[ithunk0].inext = ithunk;
					}

					pNewGThunk->iSimClass = pcurobj->m_iSimClass;
					pNewGThunk->BBox[0] = igxInner[0] & ~(igx[0]-ix>>31);
					pNewGThunk->BBox[1] = igyInner[0] & ~(igy[0]-iy>>31);
					pNewGThunk->BBox[2] = igxInner[1] + (255-igxInner[1] & ix-igx[1]>>31);
					pNewGThunk->BBox[3] = igyInner[1] + (255-igyInner[1] & iy-igy[1]>>31);
					pNewGThunk->BBoxZ0  = igz[0];
					pNewGThunk->BBoxZ1  = igz[1];
					pNewGThunk->pent = pcurobj;
				}
				pcurobj->m_ig[0].x=igx[0]; pcurobj->m_ig[1].x=igx[1];
				pcurobj->m_ig[0].y=igy[0]; pcurobj->m_ig[1].y=igy[1];
				if (pcurobj->m_pEntBuddy && pcurobj->m_pEntBuddy->m_pEntBuddy==pcurobj && !IsPortal(pcurobj)) {
					pcurobj->m_pEntBuddy->m_iGThunk0 = pcurobj->m_iGThunk0;
					pcurobj->m_pEntBuddy->m_ig[0].x=igx[0]; pcurobj->m_pEntBuddy->m_ig[1].x=igx[1];
					pcurobj->m_pEntBuddy->m_ig[0].y=igy[0]; pcurobj->m_pEntBuddy->m_ig[1].y=igy[1];
				}
			skiprepos:;
				bGridLocked = lockGrid.iActive & ~-(flags>>3&1);
				lockGrid.SetActive(flags>>3 & 1);
			} else if (pobj->m_iGThunk0) for(ix=igx[1],ithunk=pobj->m_iGThunk0;ix>=igx[0];ix--) for(iy=igy[1];iy>=igy[0];iy--,ithunk=m_gthunks[ithunk].inextOwned) {
				m_gthunks[ithunk].BBox[0] = igxInner[0] & ~(igx[0]-ix>>31);
				m_gthunks[ithunk].BBox[1] = igyInner[0] & ~(igy[0]-iy>>31);
				m_gthunks[ithunk].BBox[2] = igxInner[1] + (255-igxInner[1] & ix-igx[1]>>31);
				m_gthunks[ithunk].BBox[3] = igyInner[1] + (255-igyInner[1] & iy-igy[1]>>31);
				m_gthunks[ithunk].BBoxZ0  = igz[0];
				m_gthunks[ithunk].BBoxZ1  = igz[1];
			}

		if (pobj->m_iSimClass)
			if (pobj->m_iSimClass > 4) {
				if (IsPortal(pobj) && !(flags & 16))	{
					UnlockGrid(pobj, -bGridLocked);	bGridLocked = 0;
					SyncPortal(pobj);
				}
			} else {
				CPhysicalEntity *pent = (CPhysicalEntity*)pobj;
				if (CPhysicalPlaceholder *portal = pent->GetLastPortal()) {
					((CPhysicalEntity*)pobj)->SetLastPortal(nullptr);
					pe_status_contains_point scp;
					scp.pt = (pBBox[0]+pBBox[1])*0.5f;
					pe_status_dynamics sd;
					if (pobj->GetStatus(&sd))
						scp.pt = sd.centerOfMass;
					pe_params_outer_entity poe;
					IPhysicalEntity *insideWhat = portal->GetParams(&poe) && poe.pOuterEntity ? poe.pOuterEntity : portal;
					scp.pt = GetGrid(insideWhat)->m_transW.GetInverted() * (GetGrid(pobj)->m_transW * scp.pt);
					if (!(insideWhat->GetStatus(&scp) ^ -(portal->m_iForeignFlags & TRIGGER_PORTAL_INV)>>31 & 1)) {
						UnlockGrid(pobj, -bGridLocked);	bGridLocked = 0;
						pe_params_pos pp;
						pp.bRecalcBounds |= 32;
						pent->m_pos = pent->m_pNewCoords->pos;
						pent->m_qrot = pent->m_pNewCoords->q;
						pp.pGridRefEnt = portal->m_pEntBuddy;
						pobj->SetParams(&pp);
						pBBox[0] = pent->m_BBox[0]; pBBox[1] = pent->m_BBox[1];
					}
				}
				if (SEntityGrid *pgrid1 = GetHostedGrid(pent)) {
					Vec3 v0 = pgrid1->m_v, w0 = pgrid1->m_w;
					pgrid1->GetUpdatedTransW();
					pgrid1->GetUpdatedVelW();
					UnlockGrid(pobj, -bGridLocked);	bGridLocked = 0;
					Quat qloc = !(pent->m_qrot*pgrid1->m_transInHost.q);
					Vec3 dv = qloc*(pgrid1->m_v-v0), dw = qloc*(pgrid1->m_w-w0);
					Vec3 com = pgrid1->m_transInHost.GetInverted()*pgrid1->m_com*pent->m_qrot;
					pe_action_impulse ai;
					ai.iSource = 5;
					for(ithunk=m_gthunks[pgrid1->m_iGThunk0].inext; ithunk; ithunk=m_gthunks[ithunk].inext)
						if (IsPortal(m_gthunks[ithunk]))
							SyncPortal(m_gthunks[ithunk].pent);
						else if ((unsigned int)m_gthunks[ithunk].pent->m_iSimClass-1u < 2u) {
							CPhysicalEntity *pent1 = (CPhysicalEntity*)m_gthunks[ithunk].pent;
							int nbodies = pent1->GetType()!=PE_ARTICULATED ? 1:pent1->m_nParts-1, nimp=0;
							RigidBody *pbodyPrev=nullptr, *pbody;
							CPhysicalEntity **pcoll;
							for(i=pent1->GetColliders(pcoll)-1,j=-1; i>=0; i--)
								if (GetGrid(pcoll[i])!=pgrid1)
									pcoll[j=i]->Awake();
							for(ai.ipart=0; ai.ipart<nbodies; ai.ipart++,pbody=pbodyPrev) {
								if ((pbody = pent1->GetRigidBody(ai.ipart))==pbodyPrev)
									continue;
								Vec3 dvLoc = dv + (dw ^ pbody->pos-com);
								if (j==-1 && dvLoc.len2()<pgrid1->m_dvSleep2*sqr(pent->GetLastTimeStep(0)))
									continue;
								ai.impulse = -dvLoc*pbody->M;
								ai.angImpulse = pbody->q*(pbody->Ibody*(!pbody->q*-dw));
								pent1->Action(&ai); nimp++;
							}
							if (min(nbodies-1,nimp)>0) {
								pe_action_awake aa;
								aa.minAwakeTime = 0.01f;
								pent1->Action(&aa);
							}
						}
				}
			}

		if (bBBoxUpdated = BBox && (((pobj->m_BBox[0]-BBox[0]).len2()+(pobj->m_BBox[1]-BBox[1]).len2())>0)) {
			event.BBoxNew[0] = BBox[0];
			event.BBoxNew[1] = BBox[1];
		}	else {
			event.BBoxNew[0] = pobj->m_BBox[0];
			event.BBoxNew[1] = pobj->m_BBox[1];
		}
	} else {
		event.BBoxNew[0] = pobj->m_BBox[0];
		event.BBoxNew[1] = pobj->m_BBox[1];
	}

	int bSimClassUpdated = 0;
	if (flags&2) {
		CPhysicalEntity *pent = (CPhysicalEntity*)pobj;
		if (pent->m_iPrevSimClass!=pent->m_iSimClass || pent->m_bPermanent!=pent->m_bPrevPermanent) {
			bSimClassUpdated = 1;
			i = pent->m_iPrevSimClass;
			ChangeEntitySimClass(pent, bGridLocked);
/*#ifdef _DEBUG
CPhysicalEntity *ptmp = m_pTypedEnts[1];
for(;ptmp && ptmp!=m_pTypedEntsPerm[1]; ptmp=ptmp->m_next);
if (ptmp!=m_pTypedEntsPerm[1])
DEBUG_BREAK;
#endif*/
		}
	}

	if (bBBoxUpdated | bSimClassUpdated) {
		CPhysicalEntity *pent = (CPhysicalEntity*)pobj;
		if (pent->m_flags & (pef_monitor_state_changes | pef_log_state_changes) &&
			(1 - bSimClassUpdated)*(1 - bBBoxUpdated + max(0.0f, pent->GetMassInv())) == 0) { // only send BBox updates for kinematic (0-mass) entities
			event.pEntity=pent; event.pForeignData=pent->m_pForeignData; event.iForeignData=pent->m_iForeignData;
			event.BBoxOld[0] = pent->m_BBox[0];
			event.BBoxOld[1] = pent->m_BBox[1];
			event.iSimClass[0] = bSimClassUpdated ? i : pent->m_iSimClass;
			event.iSimClass[1] = pent->m_iSimClass;
			event.timeIdle = pent->m_timeIdle;
			OnEvent(pent->m_flags, &event);
		}
	}

	return bGridLocked;
}


void CPhysicalWorld::SyncPortal(CPhysicalPlaceholder *portal)
{
	if (portal->IsPlaceholder() || portal->m_pEntBuddy->IsPlaceholder()) {
		QuatT transG = GetGrid(portal->m_pEntBuddy)->m_transW.GetInverted() * GetGrid(portal)->m_transW;
		pe_params_bbox pbbox;
		transformBBox(portal->m_BBox[0],portal->m_BBox[1], pbbox.BBox, transG);
		portal->m_pEntBuddy->SetParams(&pbbox);
	} else {
		pe_status_pos sp; sp.pGridRefEnt = portal->m_pEntBuddy;
		portal->GetStatus(&sp);
		pe_params_pos pp;
		pp.bRecalcBounds |= 128;
		pp.pos = sp.pos; pp.q = sp.q;
		portal->m_pEntBuddy->SetParams(&pp);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////


float CPhysicalWorld::IsAffectedByExplosion(IPhysicalEntity *pobj, Vec3 *impulse)
{
	int i;
	CPhysicalEntity *pent = ((CPhysicalPlaceholder*)pobj)->GetEntityFast();
	for(i=0;i<m_nExplVictims && m_pExplVictims[i]!=pent;i++);
	if (i<m_nExplVictims) {
		if (impulse)
			*impulse = m_pExplVictimsImp[i];
		return m_pExplVictimsFrac[i];
	}
	if (impulse)
		impulse->zero();
	return 0.0f;
}

int CPhysicalWorld::DeformPhysicalEntity(IPhysicalEntity *pient, const Vec3 &ptHit,const Vec3 &dirHit,float r, int flags)
{
	// craig - experimental fix: i think the random number in GetExplosionShape() is upsetting things in MP
	SScopedRandomSeedChange randomSeedChange;
	if (m_vars.bMultiplayer)
		randomSeedChange.Seed(1234567);

	int i,bEntChanged,bPartChanged;
	CPhysicalEntity *pent = (CPhysicalEntity*)pient;
	pe_explosion expl;
	geom_world_data gwd,gwd1;
	box bbox;
	Vec3 zaxWorld(0,0,1),zaxObj,zax;
	gwd1.offset = ptHit;
	(zaxWorld -= dirHit*(zaxWorld*dirHit)).normalize();
	expl.epicenter=expl.epicenterImp = ptHit;
	expl.impulsivePressureAtR = 0;
	expl.r=expl.rmin=expl.holeSize = r;
	if (flags & 2) { // special values for explosion
		expl.explDir = dirHit;
		expl.impulsivePressureAtR = -1;
	}
	expl.iholeType = 0;

	{ WriteLockCond lockc(m_lockCaller[get_iCaller()], m_vars.bLogStructureChanges);
		WriteLockCond lock(pent->m_lockUpdate, m_vars.bLogStructureChanges);
		for(i=bEntChanged=0; i<pent->m_nParts; i++)
		if ((pent->m_parts[i].flags & (geom_colltype_explosion|geom_removed))==geom_colltype_explosion && pent->m_parts[i].idmatBreakable>=0) {
			pent->m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
			zaxObj = pent->m_qrot*(pent->m_parts[i].q*bbox.Basis.GetRow(idxmax3(bbox.size)));
			if (pent->m_iSimClass>0 || fabs_tpl(zaxObj*zaxWorld)<0.7f)
				(zax = zaxObj-dirHit*(zaxObj*dirHit)).normalize();
			else zax = zaxWorld;
			gwd1.R.SetColumn(0,dirHit^zax); gwd1.R.SetColumn(1,dirHit); gwd1.R.SetColumn(2,zax);
			gwd.R = Matrix33(pent->m_qrot*pent->m_parts[i].q);
			gwd.offset = pent->m_pos + pent->m_qrot*pent->m_parts[i].pos;
			gwd.scale = pent->m_parts[i].scale;
			bEntChanged += (bPartChanged=DeformEntityPart(pent,i, &expl, &gwd,&gwd1, 1));
			if (bPartChanged)
				pent->m_parts[i].flags &= ~(flags>>16 & 0xFFFF);
		}
	}
	if (bEntChanged && pent->UpdateStructure(0.01f,&expl,MAX_PHYS_THREADS))
		MarkEntityAsDeforming(pent);

	return bEntChanged;
}


void CPhysicalWorld::ClonePhysGeomInEntity(CPhysicalEntity *pent,int i,IGeometry *pNewGeom)
{
	phys_geometry *pgeom;
	if (pNewGeom->GetType()==GEOM_TRIMESH && pent->m_parts[i].pLattice)
		(pent->m_parts[i].pLattice = new CTetrLattice(pent->m_parts[i].pLattice,1))->SetMesh((CTriMesh*)pNewGeom);
	{ WriteLock lock(m_lockGeoman);
		*(pgeom = GetFreeGeomSlot()) = *pent->m_parts[i].pPhysGeomProxy;
		pgeom->pGeom = pNewGeom;
		pgeom->nRefCount = 2;
		pgeom->surface_idx = 0;
		if (pgeom->pMatMapping)
			memcpy(pgeom->pMatMapping=new int[pgeom->nMats], pent->m_parts[i].pPhysGeomProxy->pMatMapping, pgeom->nMats*sizeof(int));
	}
	if (pent->m_parts[i].pPhysGeom->pMatMapping==pent->m_parts[i].pMatMapping)
		pent->m_parts[i].pMatMapping = pgeom->pMatMapping;
	UnregisterGeometry(pent->m_parts[i].pPhysGeom);
	UnregisterGeometry(pent->m_parts[i].pPhysGeomProxy);
	pent->m_parts[i].pPhysGeomProxy = pent->m_parts[i].pPhysGeom = pgeom;
	pent->m_parts[i].flags |= geom_can_modify;
}


int CPhysicalWorld::DeformEntityPart(CPhysicalEntity *pent,int i, pe_explosion *pexpl, geom_world_data *gwd,geom_world_data *gwd1, int iSource)
{
	IGeometry *pGeom,*pHole;
	CTriMesh *pNewGeom=0;
	EventPhysUpdateMesh epum;
	int bCreateConstraint=0;
	epum.pEntity = pent;
	epum.pForeignData = pent->m_pForeignData;
	epum.iForeignData = pent->m_iForeignData;
	epum.partid = pent->m_parts[i].id;
	epum.iReason = iSource ? EventPhysUpdateMesh::ReasonRequest : EventPhysUpdateMesh::ReasonExplosion;
	epum.bInvalid = 0;
	if ((pent->m_parts[i].idmatBreakable>>7 | pexpl->iholeType)!=0 && !(pent->m_parts[i].idmatBreakable & 128<<pexpl->iholeType) 
			|| pent->m_parts[i].pPhysGeomProxy->pGeom->GetiForeignData()==DATA_UNSCALED_GEOM)
		return 0;
	for(int j=0;j<min(pent->m_nParts,5);j++)
		if (i!=j && pent->m_parts[j].flags & geom_log_interactions && pent->m_parts[j].pPhysGeom->pGeom->PointInsideStatus(
			((pexpl->epicenter-pent->m_pos)*pent->m_qrot-pent->m_parts[j].pos)*pent->m_parts[j].q*(
			pent->m_parts[j].scale==1.0f ? 1.0f:1.0f/pent->m_parts[j].scale)))
			return 0;

	if (pHole = GetExplosionShape(pexpl->holeSize,pent->m_parts[i].idmatBreakable,gwd1->scale,bCreateConstraint)) {
		pGeom = pent->m_parts[i].pPhysGeomProxy->pGeom;
		if (!(pent->m_parts[i].flags & geom_can_modify)) {
			if (!(pNewGeom = (CTriMesh*)((CGeometry*)pGeom)->GetTriMesh()))
				return 0;
			if (pent->m_parts[i].flags & geom_break_approximation)
				pNewGeom->m_flags |= mesh_force_AABB;
			pGeom = pNewGeom;
		}	else {
			box bbox,bbox1; pGeom->GetBBox(&bbox); pHole->GetBBox(&bbox1);
			float szmin0 = min(min(bbox.size.x,bbox.size.y),bbox.size.z);
			if (szmin0 > max(max(bbox.size.x,bbox.size.y),bbox.size.z)*0.4f &&
					szmin0 < max(max(bbox1.size.x,bbox1.size.y),bbox1.size.z)*1.5f &&
					++((CTriMesh*)pGeom)->m_nMessyCutCount>=5)
				return 0;
		}
#ifdef _DEBUG1
		static CTriMesh *g_pPrevGeom = 0;
		if (m_vars.iDrawHelpers & 0x4000)
			pent->m_parts[i].pPhysGeomProxy->pGeom = pGeom = g_pPrevGeom;
		else if (g_pPrevGeom)
			g_pPrevGeom->Release();
		(g_pPrevGeom = new CTriMesh())->Clone((CTriMesh*)pGeom,0);
		g_pPrevGeom->RebuildBVTree(((CTriMesh*)pGeom)->m_pTree);
#endif
		if (pGeom->Subtract(pHole, gwd,gwd1)) {
			if (pNewGeom)
				ClonePhysGeomInEntity(pent,i,pNewGeom);
			if (pent->m_parts[i].pLattice)
				pent->m_parts[i].pLattice->Subtract(pHole, gwd,gwd1);
			(epum.pMesh = pGeom)->Lock(0);
			epum.pLastUpdate = (bop_meshupdate*)epum.pMesh->GetForeignData(DATA_MESHUPDATE);
			for(; epum.pLastUpdate && epum.pLastUpdate->next; epum.pLastUpdate=epum.pLastUpdate->next);
			epum.pMesh->Unlock(0);
			OnEvent(m_vars.bLogStructureChanges+1,&epum);
			pent->m_parts[i].flags |= geom_structure_changes|(geom_constraint_on_break & -bCreateConstraint);
			return 1;
		}	else if (pNewGeom)
			pNewGeom->Release();
	}
	return 0;
}


void CPhysicalWorld::MarkEntityAsDeforming(CPhysicalEntity *pent)
{
	if (!(pent->m_flags & pef_deforming)) {
		WriteLock lock(m_lockDeformingEntsList);
		pent->m_flags |= pef_deforming;
		if (m_nDeformingEnts==m_nDeformingEntsAlloc)
			ReallocateList(m_pDeformingEnts, m_nDeformingEnts,m_nDeformingEntsAlloc+=16);
		m_pDeformingEnts[m_nDeformingEnts++] = pent;
	}
}

void CPhysicalWorld::UnmarkEntityAsDeforming(CPhysicalEntity *pent)
{
	if (pent->m_flags & pef_deforming) {
		WriteLock lock(m_lockDeformingEntsList);
		pent->m_flags &= ~pef_deforming;
		int i; for(i=m_nDeformingEnts-1; i>=0 && m_pDeformingEnts[i]!=pent;i--);
		if (i>=0)
			m_pDeformingEnts[i] = m_pDeformingEnts[--m_nDeformingEnts];
	}
}


void CPhysicalWorld::SimulateExplosion(pe_explosion *pexpl, IPhysicalEntity **pSkipEnts,int nSkipEnts, int iTypes, int iCaller)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	CPhysicalEntity **pents;
	int nents,nents1,i,j,i1,bBreak,bEntChanged;
	RigidBody *pbody;
	float kr=pexpl->impulsivePressureAtR*sqr(pexpl->r),maxspeed=15,E,frac=1.0f,sumFrac,sumV,Minv;
	Vec3 gravity;
	pe_params_buoyancy pb;
	geom_world_data gwd,gwd1;
	box bboxPart,bbox;
	sphere sphExpl;
	CPhysicalPlaceholder **pSkipPcs = (CPhysicalPlaceholder**)pSkipEnts;
	CPhysicalEntity *pGridRef = &g_StaticPhysicalEntity;
	pe_explosion explLoc = *pexpl;
	for(i=0;i<nSkipEnts;i++) if (pSkipEnts[i] && GetGrid(pSkipEnts[i])!=GetGrid(pGridRef))
		pGridRef = (CPhysicalEntity*)pSkipEnts[i];
	pe_action_impulse shockwave;
	shockwave.iApplyTime = 2;
	shockwave.iSource = 2;
	shockwave.pGridRefEnt = pGridRef;
	EventPhysCollision epc;
	epc.pEntity[0]=&g_StaticPhysicalEntity; epc.pForeignData[0]=0; epc.iForeignData[0]=0;
	epc.vloc[1].zero(); epc.mass[0] = 1E10f;
	epc.partid[0]=0; epc.idmat[0]=0; epc.penetration=epc.radius=0;
	if (!CheckAreas(pexpl->epicenter,gravity,&pb,1,-1,Vec3(ZERO),pGridRef,iCaller) || is_unused(gravity))
		gravity = m_vars.gravity;
	WriteLock lock(m_lockCaller[iCaller]);
	bboxPart.bOriented = 0;
	bboxPart.Basis.SetIdentity();
	sphExpl.center = pexpl->epicenter;
	sphExpl.r = pexpl->rmax;
	if(pexpl->rmin < FLT_EPSILON)
		pexpl->rmin = 0.1f;
	if (m_vars.bDebugExplosions)
		m_vars.bSingleStepMode = 1;

	CPhysicalEntity **pSkipPhysEnts = (CPhysicalEntity**)pSkipEnts;
	for(i=0;i<nSkipEnts;i++)
		if(pSkipPhysEnts[i]->m_flags & pef_traceable)
			AtomicAdd(&((!pSkipPhysEnts[i]->m_pEntBuddy || pSkipPhysEnts[i]->IsPlaceholder()) ? pSkipPhysEnts[i] : pSkipPhysEnts[i]->m_pEntBuddy)->m_bProcessed,1<<iCaller);
		else {
			CPhysicalPlaceholder* pPartPlaceholder;
			int numParts = pSkipPhysEnts[i]->m_nParts;
			for(int p = 0; p < numParts; p++)
				if(pPartPlaceholder = pSkipPhysEnts[i]->m_parts[p].pPlaceholder)
					AtomicAdd(&pPartPlaceholder->m_bProcessed, 1<<iCaller);
		}

#ifdef _DEBUG
	if (m_vars.iDrawHelpers & 0x4000)	{
pexpl->epicenter = m_lastEpicenter;
pexpl->epicenterImp = m_lastEpicenterImp;
pexpl->explDir = m_lastExplDir;	}
#endif
	if (pexpl->holeSize>0) {
		gwd1.R.SetRotationV0V1(Vec3(0,1,0),pexpl->explDir);
		gwd1.offset = pexpl->epicenter;
	}

	if (pexpl->nOccRes>0) {
		m_cubeMapStatic.Init(pexpl->nOccRes, pexpl->rminOcc, pexpl->rmax);
		m_cubeMapStatic.Reset();
		m_cubeMapDynamic.Init(pexpl->nOccRes, pexpl->rminOcc, pexpl->rmax);
		m_lastEpicenter = pexpl->epicenter;
		m_lastEpicenterImp = pexpl->epicenterImp;
		m_lastExplDir = pexpl->explDir;
	}

	if (pexpl->nOccRes>0 || pexpl->holeSize>0)
		for(nents=GetEntitiesAround(pexpl->epicenter-Vec3(1,1,1)*pexpl->rmax,pexpl->epicenter+Vec3(1,1,1)*pexpl->rmax,pents,
				ent_terrain|ent_static|ent_rigid,pGridRef,0,iCaller)-1; nents>=0; nents--)
		if (pents[nents]->m_iSimClass<1 || pents[nents]->GetMassInv()<=0)
		{ int bMarkDeforming = 0;
			QuatT trans = GridTrans(pGridRef, pents[nents]);
			{ WriteLock lock0(pents[nents]->m_lockUpdate);
				for(i1=bEntChanged=0; i1<pents[nents]->GetUsedPartsCount(iCaller); i1++)
				if (pents[nents]->m_parts[i=pents[nents]->GetUsedPart(iCaller,i1)].flags & geom_colltype_explosion &&
						(pents[nents]->m_nParts<=1 ||	GetGrid(pents[nents])!=GetGrid(pGridRef) ||
						 (bboxPart.center=(pents[nents]->m_parts[i].BBox[1]+pents[nents]->m_parts[i].BBox[0])*0.5f,
						  bboxPart.size  =(pents[nents]->m_parts[i].BBox[1]-pents[nents]->m_parts[i].BBox[0])*0.5f,
						  box_sphere_overlap_check(&bboxPart,&sphExpl))))
				{
					bBreak = (m_vars.breakImpulseScale || pents[nents]->m_flags & pef_override_impulse_scale || pexpl->forceDeformEntities)
						&& iTypes & 1<<pents[nents]->m_iSimClass &&
						pexpl->holeSize>0 && pents[nents]->m_parts[i].idmatBreakable>=0 && !(pents[nents]->m_parts[i].flags & geom_manually_breakable);
					if (pexpl->nOccRes<=0 && !bBreak)
						continue;
					pents[nents]->GetPartTransform(i, gwd.offset,gwd.R,gwd.scale, pGridRef);
					gwd.offset -= pexpl->epicenter;
					if (pexpl->nOccRes>0 &&
							(!(pents[nents]->m_parts[i].flags & geom_manually_breakable) ||
							 (pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox), min(min(bbox.size.x,bbox.size.y),bbox.size.z)>pexpl->rminOcc)))
						pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->BuildOcclusionCubemap(&gwd, 0, &m_cubeMapStatic, &m_cubeMapDynamic, pexpl->nGrow);
					if (bBreak) {
						gwd.offset += pexpl->epicenter;
						if (!(iTypes & ent_delayed_deformations))
							bEntChanged += DeformEntityPart(pents[nents],i, pexpl, &gwd,&gwd1);
						else {
							WriteLock lockbq(m_lockBreakQueue);
							ReallocQueue(m_breakQueue, m_breakQueueSz,m_breakQueueAlloc, m_breakQueueHead,m_breakQueueTail, 4);
							(m_breakQueue[m_breakQueueHead].pent = pents[nents])->AddRef();
							m_breakQueue[m_breakQueueHead].partid = pents[nents]->m_parts[i].id;
							m_breakQueue[m_breakQueueHead].expl = *pexpl;
							m_breakQueue[m_breakQueueHead].gwd[0] = gwd;
							m_breakQueue[m_breakQueueHead].gwd[1] = gwd1;
							m_breakQueue[m_breakQueueHead].gravity = gravity;
							m_breakQueueSz++;
						}
					}
					if (pents[nents]->m_pStructure && pents[nents]->m_pStructure->defparts && pents[nents]->m_pStructure->defparts[i].pSkelEnt) {
						Vec3 pt = pents[nents]->m_pStructure->defparts[i].lastUpdateq*(!pents[nents]->m_qrot*(trans*pexpl->epicenterImp-pents[nents]->m_pos)) +
											pents[nents]->m_pStructure->defparts[i].lastUpdatePos;
						pents[nents]->m_pStructure->defparts[i].pSkelEnt->ApplyVolumetricPressure(pt,kr,pexpl->rmin);
						bMarkDeforming = 1;
					}
				}
			}
			explLoc.epicenterImp = trans*pexpl->epicenterImp;
			explLoc.explDir = trans.q*pexpl->explDir;
			if (bEntChanged && pents[nents]->UpdateStructure(0.01f,&explLoc,-1,gravity) || bMarkDeforming)
				MarkEntityAsDeforming(pents[nents]);
		}
	nents = GetEntitiesAround(pexpl->epicenter-Vec3(1,1,1)*pexpl->rmax,pexpl->epicenter+Vec3(1,1,1)*pexpl->rmax,pents, iTypes, pGridRef,0,iCaller);
	if (pexpl->nOccRes<0 && m_cubeMapStatic.N>=0) {
		// special case: reuse the previous cubeMapStatic and process only entities that were not affected by the previous call
		for(i=nents1=0;i<nents;i++) {
			for(j=0;j<m_nExplVictims && m_pExplVictims[j]!=pents[i];j++);
			if (j==m_nExplVictims)
				pents[nents1++] = pents[i];
		}
		pexpl->nOccRes=m_cubeMapStatic.N; nents=nents1;
	}
	if (m_nExplVictimsAlloc<nents) {
		if (m_nExplVictimsAlloc) {
			delete[] m_pExplVictims; delete[] m_pExplVictimsFrac; delete[] m_pExplVictimsImp;
		}
		m_pExplVictims = new CPhysicalEntity*[m_nExplVictimsAlloc=nents];
		m_pExplVictimsFrac = new float[m_nExplVictimsAlloc];
		m_pExplVictimsImp = new Vec3[m_nExplVictimsAlloc];
	}

	for(nents--,m_nExplVictims=0; nents>=0; nents--) {
		int bMarkDeforming = 0;
		QuatT trans = GridTrans(pGridRef, pents[nents]);
		explLoc.epicenter = trans*pexpl->epicenter;
		explLoc.epicenterImp = trans*pexpl->epicenterImp;
		{ ReadLock lock0(pents[nents]->m_lockUpdate);
			m_pExplVictimsImp[m_nExplVictims].zero();
			for(i1=bEntChanged=0,sumFrac=sumV=0.0f; i1<pents[nents]->GetUsedPartsCount(iCaller); i1++)
			if ((pents[nents]->m_parts[i=pents[nents]->GetUsedPart(iCaller,i1)].flags & geom_colltype_explosion || pents[nents]->m_flags & pef_use_geom_callbacks) &&
					(pents[nents]->m_nParts<=1 ||	GetGrid(pents[nents])!=GetGrid(pGridRef) ||
					 (bboxPart.center=(pents[nents]->m_parts[i].BBox[1]+pents[nents]->m_parts[i].BBox[0])*0.5f,
					  bboxPart.size  =(pents[nents]->m_parts[i].BBox[1]-pents[nents]->m_parts[i].BBox[0])*0.5f,
					  box_sphere_overlap_check(&bboxPart,&sphExpl))))
			{
				bBreak = pents[nents]->GetMassInv()>0 && !(pents[nents]->m_parts[i].flags & geom_manually_breakable);

				if (bBreak || pents[nents]->m_parts[i].flags & (geom_monitor_contacts|geom_manually_breakable)) {
					pents[nents]->GetPartTransform(i, gwd.offset,gwd.R,gwd.scale, pGridRef);
					IGeometry *pGeom = pents[nents]->m_parts[i].pPhysGeomProxy->pGeom;
					if (pexpl->nOccRes>0) {
						gwd.offset -= pexpl->epicenter;
						frac = static_cast<CGeometry*>(pGeom)->BuildOcclusionCubemap(&gwd, 1, &m_cubeMapStatic, &m_cubeMapDynamic, pexpl->nGrow);
						gwd.offset += pexpl->epicenter;
						float Vsafe = max(0.0001f,pents[nents]->m_parts[i].pPhysGeomProxy->V);
						sumFrac += Vsafe*frac;
						sumV += Vsafe;
					}

					if (bBreak)
						if (pexpl->holeSize>0 && pents[nents]->m_parts[i].idmatBreakable>=0)
							bEntChanged += DeformEntityPart(pents[nents],i, &explLoc, &gwd,&gwd1);

					if (kr>0) {
						if (!(pents[nents]->m_flags & pef_use_geom_callbacks)) {
							shockwave.impulse.zero(); shockwave.angImpulse.zero();
							pbody = pents[nents]->GetRigidBody(i);
							Minv = pents[nents]->GetMassInv();
							Vec3 com = pents[nents]->GetType()==PE_STATIC ? (pents[nents]->m_BBox[0]+pents[nents]->m_BBox[1])*0.5f : pbody->pos;
							pGeom->CalcVolumetricPressure(&gwd, pexpl->epicenterImp,kr,pexpl->rmin, com,
								shockwave.impulse,shockwave.angImpulse);
							shockwave.impulse *= frac; shockwave.angImpulse *= frac;
							shockwave.ipart = i;
							if ((E=shockwave.impulse.len2()*sqr(Minv))>sqr(maxspeed))
								shockwave.impulse *= sqrt_tpl(sqr(maxspeed)/E);
							if ((E=shockwave.angImpulse*(pbody->Iinv*shockwave.angImpulse)*Minv)>sqr(maxspeed))
								shockwave.angImpulse *= sqrt_tpl(sqr(maxspeed)/E);
							if (pents[nents]->m_pStructure && pents[nents]->m_pStructure->defparts && pents[nents]->m_pStructure->defparts[i].pSkelEnt) {
								Vec3 pt = pents[nents]->m_pStructure->defparts[i].lastUpdateq*(!pents[nents]->m_qrot*(explLoc.epicenterImp-pents[nents]->m_pos)) +
													pents[nents]->m_pStructure->defparts[i].lastUpdatePos;
								pents[nents]->m_pStructure->defparts[i].pSkelEnt->ApplyVolumetricPressure(pt,kr*frac,pexpl->rmin);
								bMarkDeforming = 1;
							}
							pents[nents]->Action(&shockwave, -(iCaller-MAX_PHYS_THREADS>>31));
							m_pExplVictimsImp[m_nExplVictims] += shockwave.impulse;
						} else
							pents[nents]->ApplyVolumetricPressure(explLoc.epicenterImp,kr*frac,pexpl->rmin);
					}
				} else if (pents[nents]->m_flags & pef_use_geom_callbacks)
					pents[nents]->ApplyVolumetricPressure(explLoc.epicenterImp,kr*frac,pexpl->rmin);

				if ((pents[nents]->m_parts[i].flags & (geom_manually_breakable|geom_structure_changes))==geom_manually_breakable &&
						(pexpl->nOccRes==0 || sumFrac>0))
				{
					int iprim=0,ifeat,ncont,bMultipart;
					Vec3 ptdst[2];
					CBoxGeom boxGeom;
					intersection_params ip;
					geom_contact *pcontacts;
					float rscale;
					mesh_data *pmd=0;
					pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);

					if (pents[nents]->m_parts[i].idmatBreakable>=0 || pents[nents]->m_parts[i].flags & geom_break_approximation ||
							pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->GetPrimitiveCount()<=1 ||
							!(pmd=(mesh_data*)pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->GetData()) || pmd->nIslands<=1)
					{
						if (!pmd || !pmd->pMats)
							epc.idmat[1] = pents[nents]->GetMatId(-1,i);
						else {
							for(iprim=pmd->nTris-1; iprim>=0 &&
								!(m_SurfaceFlagsTable[epc.idmat[1]=pents[nents]->GetMatId(pmd->pMats[iprim],i)] & sf_manually_breakable); iprim--);
							epc.iPrim[1] = iprim;
						}
						epc.n.zero(); bMultipart=0;
						goto single_island;
					} else {
						for(j=0;j<pmd->nIslands;j++) if (((ptdst[0]=gwd.R*pmd->pIslands[j].center*gwd.scale+gwd.offset)-sphExpl.center).len2()<sqr(sphExpl.r*2)) {
							epc.n.zero(); bMultipart=1;
							if (!pmd->pMats)
								epc.idmat[1] = pents[nents]->GetMatId(-1,i);
							else {
								Vec3 BBox[2] = { pmd->pIslands[j].center, pmd->pIslands[j].center };
								for(iprim=pmd->pIslands[j].itri,epc.idmat[1]=-1,epc.iPrim[1]=-1; iprim<pmd->nTris && pmd->pMats[iprim]>=0; iprim=pmd->pTri2Island[iprim].inext) {
									int idmat = pents[nents]->GetMatId(pmd->pMats[iprim],i);
									int bBreakable = -((int)m_SurfaceFlagsTable[idmat] & sf_manually_breakable)>>31;
									epc.idmat[1] += idmat-epc.idmat[1] & bBreakable;
									epc.iPrim[1] += iprim-epc.iPrim[1] & bBreakable;
									for(int ivtx=0;ivtx<3;ivtx++) {
										Vec3 vtx = pmd->pVertices[pmd->pIndices[iprim*3+ivtx]];
										BBox[0]=min(BBox[0],vtx); BBox[1]=max(BBox[1],vtx);
									}
								}
								if (iprim<pmd->nTris || epc.idmat[1]<0)
									continue;
								epc.n[idxmin3(bbox.size = BBox[1]-BBox[0])] = 1.0f;
								epc.n = gwd.R*epc.n;
								epc.vloc[0] = -(epc.n *= sgnnz(epc.n*(sphExpl.center-ptdst[0])));
							}
							goto post_event;

					single_island:
					boxGeom.CreateBox(&bbox);
					if (boxGeom.FindClosestPoint(&gwd, iprim,ifeat, pexpl->epicenter,pexpl->epicenter, ptdst, 1)<0 ||
							(pexpl->epicenter-ptdst[0])*(ptdst[0]-gwd.offset-gwd.R*bbox.center*gwd.scale)<0)
						ptdst[0] = pexpl->epicenter;
					if (pents[nents]->m_parts[i].idmatBreakable>=0) {
						if ((ptdst[0]-ptdst[1]).len2() > sqr(pexpl->rmin*0.7f+pexpl->rmax*0.3f))
							goto next_part;
						j = idxmax3(bbox.size);
						rscale = gwd.scale==1.0f ? 1.0f:1.0f/gwd.scale;
						ptdst[1].z = (bbox.Basis.GetRow(j)*((ptdst[0]-gwd.offset)*gwd.R-bbox.center))*rscale;
						ptdst[1].z = max(-bbox.size[j]*0.8f, min(bbox.size[j]*0.8f, ptdst[1].z));
						bbox.center += bbox.Basis.GetRow(j)*ptdst[1].z;
						bbox.size[inc_mod3[j]]*=1.01f; bbox.size[dec_mod3[j]]*=1.01f; bbox.size[j]*=0.002f;
						boxGeom.CreateBox(&bbox);
						if (ncont=pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->Intersect(&boxGeom, 0,0,&ip, pcontacts) && pcontacts->idxborder) {
							ptdst[0].Set(1E10f,1E10f,1E10f);
							ptdst[1] = ((pexpl->epicenter-gwd.offset)*gwd.R)*rscale;
							float dist,mindist=1E10f;
							for(ncont--;ncont>=0;ncont--) for(j=0;j<pcontacts[ncont].nborderpt;j++) {
								Vec3 vtx0 = pcontacts[ncont].ptborder[j],
										 vtx1 = pcontacts[ncont].ptborder[j+1-(pcontacts[ncont].nborderpt & pcontacts[ncont].nborderpt-j-2<<31)];
								if ((dist=(vtx0-ptdst[1]).len2()) < mindist)
									epc.n = pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->GetNormal(pcontacts[ncont].idxborder[j][0] & IDXMASK,
									ptdst[0]=vtx0), mindist=dist;
								float proj=(ptdst[1]-vtx0)*(vtx1-vtx0), edgelen=(vtx1-vtx0).len2();
								if (inrange(proj,0.0f,edgelen) && (dist=(ptdst[1]-vtx0^vtx1-vtx0).len2())<mindist*edgelen)	{
									mindist = dist*(edgelen = 1.0f/edgelen);
									ptdst[0] = vtx0+(vtx1-vtx0)*(proj*edgelen);
									epc.n = pents[nents]->m_parts[i].pPhysGeomProxy->pGeom->GetNormal(pcontacts[ncont].idxborder[j][0] & IDXMASK,	ptdst[0]);
								}
							}
							ptdst[0] = gwd.R*ptdst[0]*gwd.scale+gwd.offset;
							epc.vloc[0] = -(epc.n = gwd.R*epc.n);
						}
					}
					post_event:
					epc.pt = ptdst[0];
					epc.pEntity[1]=pents[nents]; epc.pForeignData[1]=pents[nents]->m_pForeignData; epc.iForeignData[1]=pents[nents]->m_iForeignData;
					if ((epc.pt-pexpl->epicenter).len2()<sqr(pexpl->holeSize*0.2f)*(1-bMultipart)) {
						epc.pt = pexpl->epicenter;
						if (!epc.n.len2())
							epc.n = -(epc.vloc[0] = pexpl->explDir);
					}	else if (!epc.n.len2())
						epc.vloc[0] = -(epc.n = (pexpl->epicenter-epc.pt).normalized());
					epc.mass[0] = bbox.size.x*bbox.size.y+bbox.size.x*bbox.size.z+bbox.size.y*bbox.size.z;
					epc.vloc[0] *= kr/max(sqr(pexpl->rmin), (pexpl->epicenter-epc.pt).len2());
					epc.vloc[0] *= epc.mass[0]*100.0f;
					epc.mass[0] = 0.01f;
					epc.mass[1] = pents[nents]->GetMass(i);
					epc.partid[1] = pents[nents]->m_parts[i].id;
					epc.idmat[0] = -1;
					epc.normImpulse=epc.penetration = 0;
					epc.radius = pexpl->rmax;
					//pents[nents]->m_parts[i].flags |= geom_will_be_destroyed;

					OnEvent(pef_log_collisions, &epc);
					if (!bMultipart)
						break;
					}} // multi-island loop end
					next_part:;
				}
			}
		}
		if (pents[nents]->m_nParts==0)
			pents[nents]->ApplyVolumetricPressure(explLoc.epicenterImp,kr,pexpl->rmin);

		m_pExplVictims[m_nExplVictims] = pents[nents];
		m_pExplVictimsFrac[m_nExplVictims++] = sumV>0 ? sumFrac/sumV : 1.0f;
		if (bEntChanged && pents[nents]->UpdateStructure(0.01f,pexpl,-1,gravity) || bMarkDeforming)
			MarkEntityAsDeforming(pents[nents]);
	}
	pexpl->pAffectedEnts = (IPhysicalEntity**)m_pExplVictims;
	pexpl->pAffectedEntsExposure = m_pExplVictimsFrac;
	pexpl->nAffectedEnts = m_nExplVictims;

	for(i=0;i<nSkipEnts;i++)
		if(pSkipPhysEnts[i]->m_flags & pef_traceable)
			AtomicAdd(&((!pSkipPhysEnts[i]->m_pEntBuddy || pSkipPhysEnts[i]->IsPlaceholder()) ? pSkipPhysEnts[i] : pSkipPhysEnts[i]->m_pEntBuddy)->m_bProcessed,-(1<<iCaller));
		else {
			CPhysicalPlaceholder* pPartPlaceholder;
			int numParts = pSkipPhysEnts[i]->m_nParts;
			for(int p = 0; p < numParts; p++)
				if(pPartPlaceholder = pSkipPhysEnts[i]->m_parts[p].pPlaceholder)
					AtomicAdd(&pPartPlaceholder->m_bProcessed, -(1<<iCaller));
		}
}


float CPhysicalWorld::CalculateExplosionExposure(pe_explosion *pexpl, IPhysicalEntity *pient)
{
	if (pexpl->nOccRes<=0)
		return 1.0f;
	if (pient->GetType()==PE_AREA)
		return 0.0f;

	CPhysicalEntity *pent = (CPhysicalEntity*)pient;
	WriteLock lockc(m_lockCaller[get_iCaller()]);
	ReadLock lock(pent->m_lockUpdate);
	int i;
	float sumV,sumFrac,frac;
	geom_world_data gwd;

	for(i=0,sumFrac=sumV=0.0f; i<pent->m_nParts; i++) if (pent->m_parts[i].flags & geom_colltype_explosion) {
		gwd.R = Matrix33(pent->m_qrot*pent->m_parts[i].q);
		gwd.offset = pent->m_pos + pent->m_qrot*pent->m_parts[i].pos - pexpl->epicenter;
		gwd.scale = pent->m_parts[i].scale;
		frac = pent->m_parts[i].pPhysGeomProxy->pGeom->BuildOcclusionCubemap(&gwd, 1, &m_cubeMapStatic, &m_cubeMapDynamic, pexpl->nGrow);
		sumFrac += pent->m_parts[i].pPhysGeomProxy->V*frac;
		sumV += pent->m_parts[i].pPhysGeomProxy->V;
	}

	return sumV>0 ? sumFrac/sumV : 1.0f;
}


void CPhysicalWorld::ResetDynamicEntities()
{
	int i; CPhysicalEntity *pent;
	pe_action_reset reset; reset.bClearContacts=2;
	{ WriteLock lock(m_lockStep);
		for(i=1;i<=4;i++) for(pent=m_pTypedEnts[i]; pent; pent=pent->m_next)
			pent->Action(&reset);
	}
	{ WriteLock lock(m_lockAreas);
		for(CPhysArea *pArea=m_pGlobalArea;pArea;pArea=pArea->m_next)
			pArea->Action(&reset);
	}
}


void CPhysicalWorld::DestroyDynamicEntities()
{
	int i; CPhysicalEntity *pent,*pent_next;

	m_nDynamicEntitiesDeleted = 0;
	for(i=1;i<=4;i++) {
		for(pent=m_pTypedEnts[i]; pent; pent=pent_next) {
			pent_next = pent->m_next;
			if (pent->m_pEntBuddy) {
				pent->m_pEntBuddy->m_pEntBuddy = 0;
				DestroyPhysicalEntity(pent->m_pEntBuddy);
			}	else
				SetPhysicalEntityId(pent,-1);
			DetachEntityGridThunks(pent);
			for(int j=0;j<pent->m_nParts;j++) if (pent->m_parts[j].pPlaceholder)
				DetachEntityGridThunks(pent->m_parts[j].pPlaceholder);
			if (pent->m_next = m_pTypedEnts[7])
				pent->m_next->m_prev = pent;
			m_pTypedEnts[7] = pent;
			pent->m_iPrevSimClass = -1; pent->m_iSimClass = 7;
			m_nDynamicEntitiesDeleted++;
		}
		m_pTypedEnts[i] = m_pTypedEntsPerm[i] = 0;
	}

	m_nEnts -= m_nDynamicEntitiesDeleted;
	if (m_nEnts < m_nEntsAlloc-8192 && !m_bEntityCountReserved) {
		int nEntsAlloc = m_nEntsAlloc;
		m_nEntsAlloc = (m_nEnts-1&~8191)+8192; m_nEntListAllocs++;
		ReallocateList(m_pTmpEntList,nEntsAlloc,m_nEntsAlloc);
		ReallocateList(m_pTmpEntList1,nEntsAlloc,m_nEntsAlloc);
		ReallocateList(m_pTmpEntList2,nEntsAlloc,m_nEntsAlloc);
		ReallocateList(m_pGroupMass,0,m_nEntsAlloc);
		ReallocateList(m_pMassList,0,m_nEntsAlloc);
		ReallocateList(m_pGroupIds,0,m_nEntsAlloc);
		ReallocateList(m_pGroupNums,0,m_nEntsAlloc);
	}
}

void CPhysicalWorld::PurgeDeletedEntities()
{
	auto IterateQueue = [this](std::function<void(char *slot,CPhysicalWorld*)> func)	{
		for(int i=0; i<m_nQueueSlots; i++)
			for(int j=0; *(int*)(m_pQueueSlots[i]+j)!=-1; j+=*(int*)(m_pQueueSlots[i]+j+sizeof(int)))
				func(m_pQueueSlots[i]+j,this);
	};
	{ WriteLock lock1(m_lockQueue);
		IterateQueue([](char *slot,CPhysicalWorld *pWorld) { 
			CPhysicalEntity *pent = *(CPhysicalEntity**)(slot+sizeof(int)*2);
			if (*slot==5) 
				pWorld->DestroyPhysicalEntity(pent, *(int*)(slot+sizeof(int)*2+sizeof(void*)), 1); 
		});
		IterateQueue([](char *slot,CPhysicalWorld*) { if ((*(CPhysicalEntity**)(slot+sizeof(int)*2))->m_iSimClass==7) *(int*)slot = -2; });
	}
	for(SEntityGrid *pgrid=m_pDeletedGrids; pgrid; pgrid=DestroyGrid(pgrid));
	m_pDeletedGrids = nullptr;

	{ WriteLock lock3(m_lockDeformingEntsList);
		int i,j;
		for(i=j=0; i<m_nDeformingEnts; i++) if (m_pDeformingEnts[i]->m_iSimClass!=7)
			m_pDeformingEnts[j++] = m_pDeformingEnts[i];
		else
			m_pDeformingEnts[i]->m_flags &= ~pef_deforming;
		m_nDeformingEnts = j;
	}

	TracePendingRays(0);

	WriteLock lock(m_lockStep);
	CleanseEventsQueue();
	CPhysicalEntity *pent,*pent_next;
	/*for(pent=m_pTypedEnts[7]; pent; pent=pent_next) { // purge deletion requests
		pent_next = pent->m_next; delete pent;
	}
	m_pTypedEnts[7] = 0;*/
	for(pent=m_pTypedEnts[7]; pent; pent=pent_next) { // purge deletion requests
		pent_next = pent->m_next;
		if (pent->m_nRefCount<=0)	{
			if (pent->m_next) pent->m_next->m_prev = pent->m_prev;
			(pent->m_prev ? pent->m_prev->m_next : m_pTypedEnts[7]) = pent->m_next;
			pent->Delete();
		}
	}
}


void CPhysicalWorld::DrawPhysicsHelperInformation(IPhysRenderer *pRenderer, int iCaller)
{
#ifndef _RELEASE
	int entype; CPhysicalEntity *pent=0;
	QuatT offs0 = (m_pRenderer = pRenderer)->SetOffset(m_vars.helperOffset);

	if (m_vars.iDrawHelpers) {
		assert(iCaller<=MAX_PHYS_THREADS);
		int i,n=0,nEntListAllocs,nGEA;
		CPhysicalEntity **pEntList;
		{ WriteLock lock0(m_lockCaller[iCaller]);
			if (m_pHeightfield[iCaller] && m_vars.iDrawHelpers & 128)
				pRenderer->DrawGeometry(m_pHeightfield[iCaller]->m_parts[0].pPhysGeom->pGeom,0,0);
			nEntListAllocs=m_nEntListAllocs;
			nGEA=m_nGEA[iCaller];

			pEntList = iCaller ? m_pTmpEntList2:m_pTmpEntList;
			{ ReadLock lock(m_lockList);
				for(entype=0;entype<=6;entype++) if (m_vars.iDrawHelpers & 0x100<<entype)
				for(pent=m_pTypedEnts[entype]; pent && nEntListAllocs==m_nEntListAllocs && nGEA==m_nGEA[iCaller]; pent=pent->m_next)
					pEntList[n++] = (CPhysicalEntity*)(EXPAND_PTR)pent->m_id;
				if (pent)
					return;
			}
		}
		for(i=0;i<n;i++) {
			int id = *(int*)(pEntList+i);
			if (nEntListAllocs!=m_nEntListAllocs || nGEA!=m_nGEA[iCaller])
				break;
			if ((pent=(CPhysicalEntity*)GetPhysicalEntityById(id|1<<30)) && m_vars.iDrawHelpers & 0x80<<pent->m_iSimClass+1) {
				QuatT trans = GetGrid(pent)->m_transW;
				pRenderer->SetOffset(trans.t+m_vars.helperOffset, trans.q);
				pent->DrawHelperInformation(pRenderer, m_vars.iDrawHelpers);
			}
		}
	}
	if (m_vars.iDrawHelpers & 32)	{
		ReadLock lock(m_lockAreas);
		for(CPhysArea *pArea=m_pGlobalArea; pArea; pArea=pArea->m_next)	{
			QuatT trans = GetGrid(pArea)->m_transW;
			pRenderer->SetOffset(trans.t+m_vars.helperOffset, trans.q);
			pArea->DrawHelperInformation(pRenderer, m_vars.iDrawHelpers);
		}
	}
	pRenderer->SetOffset(offs0.t, offs0.q);

	if (m_vars.iDrawHelpers & 8192 && m_cubeMapStatic.N) {
 		float xscale,/*xoffs,*/z, maxlength=0.7f, length;
 		int i,ix,iy,cx,cy,cz,nOccRes=m_cubeMapStatic.N;
 		Vec3 pt0,pt1,dir;
 		xscale = 2.0f/nOccRes;
 //		xoffs = 1.0f-xscale;
 		for(i=0;i<6;i++) {
 			cz=i>>1; cx=inc_mod3[cz]; cy=dec_mod3[cz];
 			for(iy=0;iy<nOccRes;iy++) for(ix=0;ix<nOccRes;ix++) {
				z = m_cubeMapStatic.ConvertToDistance(m_cubeMapStatic.grid[i][iy*nOccRes+ix]);
				if (z<m_cubeMapStatic.rmax) {
					pt0[cz] = z*((i&1)*2-1);
					pt0[cx] = ((ix+0.5f)*xscale-1.0f)*z;
					pt0[cy] = ((iy+0.5f)*xscale-1.0f)*z;
					dir = pt0; length = pt0.len();
					if (length>maxlength) dir = pt0*(maxlength/length);
					pt0 += m_lastEpicenter;
					pRenderer->DrawLine(pt0-dir,pt0,7);
					pt0[cx] -= z*xscale*0.5f; pt0[cy] -= z*xscale*0.5f;
					pt1=pt0; pt1[cx] += z*xscale; pt1[cy] += z*xscale;
					pRenderer->DrawLine(pt0,pt1,7);
					pt0[cy] += z*xscale; pt1[cy] -= z*xscale;
					pRenderer->DrawLine(pt0,pt1,7);
				}
 			}
 		}
		m_cubeMapStatic.DebugDrawToScreen(60.f, 200.f, 120.f);
	}

	if (m_vars.iDrawHelpers & 32768 && m_pWaterMan && !m_pWaterMan->m_pArea)
		m_pWaterMan->DrawHelpers(pRenderer);

	if (m_vars.iDrawHelpers & 0x100<<6)
		for(SEntityGrid *pgrid=m_entgrid.m_next; pgrid; pgrid=pgrid->m_next)
			for(int i=0;i<2;i++) for(int ix=0;ix<=pgrid->size[i];ix++) {
				Vec3 pt0((i^1)*ix*pgrid->step[i],i*ix*pgrid->step[i],0), pt1=pt0+Vec3(i*pgrid->size.x*pgrid->step.x,(i^1)*pgrid->size.y*pgrid->step.y,0);
				pRenderer->DrawLine(pgrid->m_transW*(pgrid->origin+pgrid->vecFromGrid(pt0)), pgrid->m_transW*(pgrid->origin+pgrid->vecFromGrid(pt1)), 6);
			}

	if (m_vars.bLogActiveObjects) {
		ReadLock lock(m_lockList);
		int i,nPrims,nCount=0;
		if (m_vars.bLogActiveObjects & 1)	{
			RigidBody *pbody;
			for(pent=m_pTypedEnts[2]; pent; pent=pent->m_next) if (pent->GetMassInv()>0) {
				for(i=nPrims=0;i<pent->m_nParts;i++) if (pent->m_parts[i].flags & geom_colltype0)
					nPrims += ((CGeometry*)pent->m_parts[i].pPhysGeomProxy->pGeom)->GetPrimitiveCount();
				pbody = pent->GetRigidBody();	++nCount;
				CryLogAlways("%s @ %7.2f,%7.2f,%7.2f, mass %.2f, v %.1f, w %.1f, #polies %d, id %d",
					m_pRenderer ? m_pRenderer->GetForeignName(pent->m_pForeignData,pent->m_iForeignData,pent->m_iForeignFlags):"",
					pent->m_pos.x,pent->m_pos.y,pent->m_pos.z, pbody->M,pbody->v.len(),pbody->w.len(),nPrims,pent->m_id);
			}
		}
		if (m_vars.bLogActiveObjects & 2)
			for(pent=m_pTypedEntsPerm[4]; pent; pent=pent->m_next) if (!(pent->m_flags & pef_disabled) && pent->IsAwake()) {
				++nCount;
				const char *type = "*wrong type*";
				switch (pent->GetType()) {
					case PE_ROPE: type = "Rope"; break;
					case PE_SOFT: type = "Cloth"; break;
					case PE_PARTICLE: type = "Particle"; break;
					case PE_ARTICULATED: type = "Character"; break;
				}
				CryLogAlways("%s @ %7.2f,%7.2f,%7.2f, %s, id %d",
					m_pRenderer ? m_pRenderer->GetForeignName(pent->m_pForeignData,pent->m_iForeignData,pent->m_iForeignFlags):"",
					pent->m_pos.x,pent->m_pos.y,pent->m_pos.z, type, pent->m_id);
			}
		CryLogAlways("%d active object(s)",nCount);
		m_vars.bLogActiveObjects = 0;
	}
#endif//_RELEASE
}

void CPhysicalWorld::DrawEntityHelperInformation(IPhysRenderer *pRenderer, int entityId, int iDrawHelpers)
{
#ifndef _RELEASE
	CPhysicalEntity* pent = (CPhysicalEntity*)GetPhysicalEntityById(entityId);
	if (pent)	{
		QuatT trans = GetGrid(pent)->m_transW;
		pRenderer->SetOffset(trans.t+m_vars.helperOffset, trans.q);
		pent->DrawHelperInformation(pRenderer, iDrawHelpers);
	}
#endif//_RELEASE
}


int CPhysicalWorld::CollideEntityWithBeam(IPhysicalEntity *_pent, Vec3 org,Vec3 dir,float r, ray_hit *phit)
{
	if (!_pent)
		return 0;
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	CPhysicalEntity *pent = (CPhysicalEntity*)_pent;
	WriteLock lockc(m_lockCaller[get_iCaller()]);
	ReadLock lock(pent->m_lockUpdate);
	CSphereGeom SweptSph;
	geom_contact *pcontacts;
	geom_world_data gwd[2];
	sphere asph;
	asph.r = r;
	asph.center.zero();
	SweptSph.CreateSphere(&asph);
	intersection_params ip;
	ip.bSweepTest = dir.len2()>0;
	gwd[0].R.SetIdentity();
	gwd[0].offset = org;
	gwd[0].v = dir;
	ip.time_interval = 1.0f;
	phit->dist = 1E10;

	for(int i=0;i<pent->m_nParts;i++) if (pent->m_parts[i].flags & geom_collides) {
		gwd[1].offset = pent->m_pos + pent->m_qrot*pent->m_parts[i].pos;
		gwd[1].R = Matrix33(pent->m_qrot*pent->m_parts[i].q);
		gwd[1].scale = pent->m_parts[i].scale;
		if (SweptSph.Intersect(pent->m_parts[i].pPhysGeom->pGeom, gwd,gwd+1, &ip, pcontacts)) {
			if (pcontacts->t<phit->dist) {
				phit->dist = pcontacts->t;
				phit->pCollider = pent;
				phit->partid = pent->m_parts[phit->ipart=i].id;
				phit->surface_idx = pent->GetMatId(pcontacts->id[1],i);
				phit->idmatOrg = pcontacts->id[1] + (pent->m_parts[i].surface_idx+1 & pcontacts->id[1]>>31);
				phit->foreignIdx = pent->m_parts[i].pPhysGeom->pGeom->GetForeignIdx(pcontacts->iPrim[1]);
				phit->pt = pcontacts->pt;
				phit->n = -pcontacts->n;
			}
		}
	}

	return isneg(phit->dist-1E9f);
}

int CPhysicalWorld::CollideEntityWithPrimitive(IPhysicalEntity *_pent, int itype, primitive *pprim, Vec3 dir, ray_hit *phit, intersection_params* pip)
{
	if (!_pent || ((CPhysicalPlaceholder*)_pent)->m_iSimClass==5)
		return 0;

	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	CPhysicalEntity *pent = (CPhysicalEntity*)_pent;

	int j;
	int iCaller = get_iCaller();
	geom_contact *pcontacts;
	geom_world_data gwd[2];
	CBoxGeom gbox;
	CCylinderGeom gcyl;
	CCapsuleGeom gcaps;
	CSphereGeom gsph;
	CGeometry *pgeom;
	gwd[0].R.SetIdentity();
	gwd[0].offset.zero();
	gwd[0].v = dir;
	phit->dist = 1E10;

	switch (itype) {
		case box::type:			 gwd[0].offset=((box*)pprim)->center; ((box*)pprim)->center.zero(); gbox.CreateBox((box*)pprim);
												 pgeom=&gbox; ((box*)pprim)->center=gwd[0].offset; break;
		case cylinder::type: gwd[0].offset=((cylinder*)pprim)->center; ((cylinder*)pprim)->center.zero(); gcyl.CreateCylinder((cylinder*)pprim);
												 pgeom=&gcyl; ((cylinder*)pprim)->center=gwd[0].offset; break;
		case capsule::type:  gwd[0].offset=((capsule*)pprim)->center; ((capsule*)pprim)->center.zero(); gcaps.CreateCapsule((capsule*)pprim);
												 pgeom=&gcaps; ((capsule*)pprim)->center=gwd[0].offset; break;
		case sphere::type:	 gwd[0].offset=((sphere*)pprim)->center; ((sphere*)pprim)->center.zero(); gsph.CreateSphere((sphere*)pprim);
												 pgeom=&gsph; ((sphere*)pprim)->center=gwd[0].offset; break;
		default: return 0;
	}

	intersection_params ip;
	if (!pip)
	{
		ip.bSweepTest = dir.len2()>0;
		ip.time_interval = 1.0f;
		pip = &ip;
	}

	Vec3 BBox[2],sz;
	box bbox;
	pgeom->GetBBox(&bbox);

	sz = bbox.size*bbox.Basis.Fabs();
	BBox[0] = gwd[0].offset+bbox.center-sz; BBox[1] = gwd[0].offset+bbox.center+sz;
	for(int i=0;i<3;i++)
		BBox[0][i]+=min(0.0f,dir[i]), BBox[1][i]+=max(0.0f, dir[i]);

	if ((pent->m_BBox[1]-pent->m_BBox[0]).len2() >= 0.0f){
		assert(iCaller<=MAX_PHYS_THREADS); // sca
		WriteLockCond lockc(m_lockCaller[iCaller], iCaller==MAX_PHYS_THREADS);
		ReadLock lockEnt(pent->m_lockUpdate);

		for(j=0;j<pent->m_nParts;++j)
			if ((pent->m_parts[j].flags & geom_collides) &&
			((pent->m_parts[j].BBox[1]-pent->m_parts[j].BBox[0]).len2()==0 || AABB_overlap(pent->m_parts[j].BBox,BBox))) {
				gwd[1].offset = pent->m_pos + pent->m_qrot*pent->m_parts[j].pos;
				gwd[1].R = Matrix33(pent->m_qrot*pent->m_parts[j].q);
				gwd[1].scale = pent->m_parts[j].scale;
				if (pgeom->Intersect(pent->m_parts[j].pPhysGeom->pGeom, gwd,gwd+1, pip, pcontacts)) {
					if (pcontacts->t<phit->dist) {
						phit->dist = pcontacts->t;
						phit->pCollider = pent;
						phit->partid = pent->m_parts[phit->ipart=j].id;
						phit->surface_idx = pent->GetMatId(pcontacts->id[1],j);
						phit->idmatOrg = pcontacts->id[1] + (pent->m_parts[j].surface_idx+1 & pcontacts->id[1]>>31);
						phit->foreignIdx = pent->m_parts[j].pPhysGeom->pGeom->GetForeignIdx(pcontacts->iPrim[1]);
						phit->pt = pcontacts->pt;
						phit->n = -pcontacts->n;
					}
				}
			}
	}

	return isneg(phit->dist-1E9f);
}


static inline void swap(CPhysicalEntity **pentlist, int i1,int i2) {
	CPhysicalEntity *pent = pentlist[i1]; pentlist[i1] = pentlist[i2]; pentlist[i2] = pent;
}
static void qsort(CPhysicalEntity **pentlist, const Vec3 &mask0,const Vec3 &mask1, int ileft,int iright)
{
	if (ileft>=iright) return;
	int i,ilast;
	swap(pentlist, ileft,ileft+iright>>1);
	for(ilast=ileft,i=ileft+1; i<=iright; i++)
		if (pentlist[i]->m_BBox[0]*mask0+pentlist[i]->m_BBox[1]*mask1 < pentlist[ileft]->m_BBox[0]*mask0+pentlist[ileft]->m_BBox[1]*mask1)
			swap(pentlist, ++ilast,i);
	swap(pentlist, ileft,ilast);
	qsort(pentlist, mask0,mask1, ileft,ilast-1);
	qsort(pentlist, mask0,mask1, ilast+1,iright);
}

// Wrapper to create a tri-mesh from one triangle
static void CreateTriMesh(CTriMesh& gtrimesh, primitives::triangle* tri)
{
	static const unsigned short indices[3]={0,1,2};
	strided_pointer<unsigned short> pIndices(const_cast<unsigned short*>(&indices[0]));
	strided_pointer<Vec3> pVerts(&tri->pt[0]);
	gtrimesh.CreateTriMesh(pVerts, pIndices, NULL, 0, 1, mesh_SingleBB|mesh_shared_vtx|/*mesh_shared_idx|*/mesh_no_vtx_merge);
}

float CPhysicalWorld::PrimitiveWorldIntersection(const SPWIParams &pp, WriteLockCond *pLockContactsExp, const char *pNameTag)
{
	int i,j,j1,ncont,nents,iActive=0;
	int iCaller = get_iCaller();
	Vec3 BBox[2],sz,mask[2]={ Vec3(ZERO),Vec3(ZERO) };
	box bbox;
	CPhysicalEntity **pents;
	CBoxGeom gbox;
	CCylinderGeom gcyl;
	CCapsuleGeom gcaps;
	CSphereGeom gsph;
	CTriMesh gtri;
	CGeometry *pgeom;
	intersection_params ip;
	geom_world_data gwd[2];
	geom_contact *pcontacts;
	static geom_contact contactsBest[MAX_PHYS_THREADS+1];
	geom_contact &contactBest = contactsBest[iCaller];
	contactBest.t = 0; contactBest.pt.zero();

	if (pp.entTypes & rwi_queue) {
		WriteLock lockQ(m_lockPwiQueue);
		if (pp.ppcontact || pp.pip)
			return 0;

		ReallocQueue(m_pwiQueue, m_pwiQueueSz,m_pwiQueueAlloc, m_pwiQueueHead,m_pwiQueueTail, 64);
		m_pwiQueue[m_pwiQueueHead].pprim = (primitives::primitive*)m_pwiQueue[m_pwiQueueHead].primbuf;
		switch (m_pwiQueue[m_pwiQueueHead].itype = pp.itype) {
			case box::type: *(box*)m_pwiQueue[m_pwiQueueHead].primbuf = *(box*)pp.pprim; break;
			case cylinder::type: *(cylinder*)m_pwiQueue[m_pwiQueueHead].primbuf = *(cylinder*)pp.pprim; break;
			case capsule::type: *(capsule*)m_pwiQueue[m_pwiQueueHead].primbuf = *(capsule*)pp.pprim; break;
			case sphere::type: *(sphere*)m_pwiQueue[m_pwiQueueHead].primbuf = *(sphere*)pp.pprim; break;
			case triangle::type: *(triangle*)m_pwiQueue[m_pwiQueueHead].primbuf = *(triangle*)pp.pprim; break;
			default: return 0;
		}
		m_pwiQueue[m_pwiQueueHead].sweepDir = pp.sweepDir;
		m_pwiQueue[m_pwiQueueHead].entTypes = pp.entTypes & ~rwi_queue;
		m_pwiQueue[m_pwiQueueHead].geomFlagsAll = pp.geomFlagsAll;
		m_pwiQueue[m_pwiQueueHead].geomFlagsAny = pp.geomFlagsAny;
		m_pwiQueue[m_pwiQueueHead].pForeignData = pp.pForeignData;
		m_pwiQueue[m_pwiQueueHead].iForeignData = pp.iForeignData;
		m_pwiQueue[m_pwiQueueHead].OnEvent = pp.OnEvent;
		m_pwiQueue[m_pwiQueueHead].nSkipEnts = min((int)(sizeof(m_pwiQueue[0].idSkipEnts)/sizeof(m_pwiQueue[0].idSkipEnts[0])),pp.nSkipEnts);
		for(i=0;i<m_pwiQueue[m_pwiQueueHead].nSkipEnts;i++)
			m_pwiQueue[m_pwiQueueHead].idSkipEnts[i] = pp.pSkipEnts[i] ? GetPhysicalEntityId(pp.pSkipEnts[i]):-3;
		m_pwiQueueSz++;
		return 1;
	}

	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	PHYS_FUNC_PROFILER( pNameTag );
	WriteLockCond lock(m_lockCaller[iCaller]), &lockContacts = pLockContactsExp ? *pLockContactsExp : const_cast<SPWIParams&>(pp).lockContacts;

	if (pp.pip) {
		ip = *pp.pip;
		lockContacts.prw = &m_lockCaller[iCaller];
		gwd[0].v = pp.sweepDir;
	} else if (pp.sweepDir.len2()>0)	{
		ip.bSweepTest = true;
		gwd[0].v = pp.sweepDir;
		ip.time_interval = 1.0f;
		contactBest.t = 1E10f;
	} else {
		ip.bStopAtFirstTri = true;
		ip.bNoBorder = true;
		ip.bNoAreaContacts = true;
	}
	if (pp.ppcontact)
		*pp.ppcontact = 0;

	switch (pp.itype) {
		case box::type:			 gwd[0].offset=((box*)pp.pprim)->center; ((box*)pp.pprim)->center.zero(); gbox.CreateBox((box*)pp.pprim);
												 pgeom=&gbox; ((box*)pp.pprim)->center=gwd[0].offset; break;
		case cylinder::type: gwd[0].offset=((cylinder*)pp.pprim)->center; ((cylinder*)pp.pprim)->center.zero(); gcyl.CreateCylinder((cylinder*)pp.pprim);
												 pgeom=&gcyl; ((cylinder*)pp.pprim)->center=gwd[0].offset; break;
		case capsule::type:  gwd[0].offset=((capsule*)pp.pprim)->center; ((capsule*)pp.pprim)->center.zero(); gcaps.CreateCapsule((capsule*)pp.pprim);
												 pgeom=&gcaps; ((capsule*)pp.pprim)->center=gwd[0].offset; break;
		case sphere::type:	 gwd[0].offset=((sphere*)pp.pprim)->center; ((sphere*)pp.pprim)->center.zero(); gsph.CreateSphere((sphere*)pp.pprim);
												 pgeom=&gsph; ((sphere*)pp.pprim)->center=gwd[0].offset; break;
		case triangle::type: CreateTriMesh(gtri, (triangle*)pp.pprim); pgeom=&gtri; break;
		default: return 0;
	}
	pgeom->GetBBox(&bbox);
	sz = bbox.size*bbox.Basis.Fabs();
	BBox[0] = gwd[0].offset+bbox.center-sz; BBox[1] = gwd[0].offset+bbox.center+sz;
	for(i=0;i<3;i++)
		BBox[0][i]+=min(0.0f,pp.sweepDir[i]), BBox[1][i]+=max(0.0f,pp.sweepDir[i]);

	if (pp.nSkipEnts>=0) {
		for(i=0;i<pp.nSkipEnts;i++) if (pp.pSkipEnts[i]) {
			if (!(((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_bProcessed>>iCaller & 1))
				AtomicAdd(&((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_bProcessed,1<<iCaller);
			if (((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_pEntBuddy && !(((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_pEntBuddy->m_bProcessed>>iCaller&1))
				AtomicAdd(&((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_pEntBuddy->m_bProcessed,1<<iCaller);
		}
		nents = GetEntitiesAround(BBox[0],BBox[1],pents,pp.entTypes,0,0,iCaller);
	}	else {
		pents = (CPhysicalEntity**)pp.pSkipEnts;
		nents = -pp.nSkipEnts;
		for(i=0;i<nents;i++) if (pents[i]->m_flags & pef_parts_traceable)
			AtomicAdd(&pents[i]->m_nUsedParts, (15<<iCaller*4) - (pents[i]->m_nUsedParts & 15<<iCaller*4));
	}

	if (ip.bSweepTest && nents>0) {
		i = idxmax3(pp.sweepDir.abs());
		j = isneg(pp.sweepDir[i]);
		mask[j][i] = 1-j*2;
		qsort(pents,mask[0],mask[1],0,nents-1);
		contactBest.pt = pents[nents-1]->m_BBox[j];
	}
	for(i=0;i<nents;i++)
		if (!IgnoreCollision(pents[i]->m_collisionClass, pp.collclass))
		if (sz.x=(pents[i]->m_BBox[1]-pents[i]->m_BBox[0]).len2(),
				sz.x*(pents[i]->m_BBox[0]*mask[0]+pents[i]->m_BBox[1]*mask[1]) <= sz.x*(contactBest.pt*(mask[0]+mask[1])))
	{	ReadLock lockEnt(pents[i]->m_lockUpdate);
		for(j1=0;j1<pents[i]->GetUsedPartsCount(iCaller);j1++)
			if ((pents[i]->m_parts[j=pents[i]->GetUsedPart(iCaller,j1)].flags & pp.geomFlagsAll)==pp.geomFlagsAll &&
					(pents[i]->m_parts[j].flags & pp.geomFlagsAny) &&
					((pents[i]->m_parts[j].BBox[1]-pents[i]->m_parts[j].BBox[0]).len2()==0 || AABB_overlap(pents[i]->m_parts[j].BBox,BBox)))
			{
				gwd[1].offset = pents[i]->m_pos + pents[i]->m_qrot*pents[i]->m_parts[j].pos;
				gwd[1].R = Matrix33(pents[i]->m_qrot*pents[i]->m_parts[j].q);
				gwd[1].scale = pents[i]->m_parts[j].scale;
				if (ncont = pgeom->Intersect(pents[i]->m_parts[j].pPhysGeom->pGeom,gwd,gwd+1,&ip,pcontacts)) {
					for(int ic=0;ic<ncont;ic++) {
						pcontacts[ic].iNode[0]=pcontacts[ic].iPrim[1];
						pcontacts[ic].iNode[1]=j;
						pcontacts[ic].iPrim[0]=pents[i]->m_id; pcontacts[ic].iPrim[1]=pents[i]->m_parts[j].id;
						pcontacts[ic].id[1] = pents[i]->GetMatId(pcontacts[ic].id[1], j);
					}
					if (ip.bStopAtFirstTri) {
						if (pp.ppcontact) *pp.ppcontact = ip.pGlobalContacts;
						contactBest.t = 1.f;
						goto Finished;
					}
					if (ip.bSweepTest) {
						if (pcontacts[0].t<contactBest.t)
							contactBest = pcontacts[0];
					} else
						ip.bKeepPrevContacts=true, contactBest.t+=ncont;
				}
			}
	}

	if (m_vars.iDrawHelpers & 64 && m_pRenderer) {
		if (!ip.bSweepTest)
			m_pRenderer->DrawGeometry(pgeom,gwd,7,1);
		else if (contactBest.t<1E10f)	{
			m_pRenderer->DrawGeometry(pgeom,gwd,7,1,sz=gwd[0].v.normalized()*contactBest.t);
			gwd[0].offset+=sz; m_pRenderer->DrawGeometry(pgeom,gwd,7,1,gwd[0].v-sz);
		}	else
			m_pRenderer->DrawGeometry(pgeom,gwd,7,1,gwd[0].v);
	}

	if (pp.ppcontact)
		*pp.ppcontact = ip.bSweepTest ? &contactBest : ip.pGlobalContacts;

 Finished:
	if (pp.pip && !pp.pip->bThreadSafe && contactBest.t>0.0f && contactBest.t<1E10f) {
		lock.SetActive(0); lockContacts.SetActive(1);
	}

	for(i=0;i<pp.nSkipEnts;i++) if (pp.pSkipEnts[i]) {
		if (((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_bProcessed>>iCaller & 1)
			AtomicAdd(&((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_bProcessed,-(1<<iCaller));
		if (((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_pEntBuddy && (((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_pEntBuddy->m_bProcessed>>iCaller&1))
			AtomicAdd(&((CPhysicalPlaceholder**)pp.pSkipEnts)[i]->m_pEntBuddy->m_bProcessed,-(1<<iCaller));
	}

	return contactBest.t<1E10f ? contactBest.t:0;
}


int CPhysicalWorld::RayTraceEntity(IPhysicalEntity *pient, Vec3 origin,Vec3 dir, ray_hit *pHit, pe_params_pos *pp,
								   unsigned int geomFlagsAny /*=geom_colltype0|geom_colltype_player*/)
{
	if (!(dir.len2()>0 && origin.len2()>=0))
		return 0;

	int i,ncont;
	Vec3 BBox[2],sz;
	box bbox;
	WriteLock lock(m_lockCaller[get_iCaller()]);
	Vec3 pos;
	quaternionf qrot;
	float scale = 1.0f;
	CRayGeom aray(origin,dir);
	ray bray;
	geom_world_data gwd;
	geom_contact *pcontacts;
	intersection_params ip;
	pHit->dist = 1E10;

	if (((CPhysicalPlaceholder*)pient)->m_iSimClass!=5) {
		CPhysicalEntity *pent = ((CPhysicalPlaceholder*)pient)->GetEntity();
		auto FillHit = [pHit,pent](const geom_contact& cont, int ipart) {
			pHit->dist = cont.t;
			pHit->pCollider = pent; 
			pHit->surface_idx = pent->GetMatId(cont.id[0], pHit->ipart=ipart);
			pHit->pt = cont.pt;
			pHit->n = cont.n;
		};

		if (pent->m_flags & pef_use_geom_callbacks || !pent->m_nParts) {
			SRayTraceRes rtr(&aray, nullptr);
			if (ncont = pent->RayTrace(rtr)) {
				FillHit(rtr.pcontacts[ncont-1], 0);
				pHit->partid = rtr.pcontacts[ncont-1].iNode[0];
				return 1;
			}
			return 0;
		}

		if (pp) {
			pos = pp->pos; qrot = pp->q;
			if (!is_unused(pp->scale)) scale = pp->scale;
			get_xqs_from_matrices(pp->pMtx3x4,pp->pMtx3x3, pos,qrot,scale);
		}	else {
			pos = pent->m_pos; qrot = pent->m_qrot;
		}

		bray.dir = dir;
		bray.origin = origin;

		bbox.Basis.SetIdentity();
		bbox.bOriented = 0;

		for(i=0;i<pent->m_nParts;++i)	{
			const geom& part = pent->m_parts[i];

			if (part.flags & geomFlagsAny) {
				const Vec3& ptmin = part.BBox[0];
				const Vec3& ptmax = part.BBox[1];

				if ((ptmax - ptmin).len2() != 0.0f) {
					bbox.center = (ptmin + ptmax) * 0.5f;
					bbox.size = (ptmax - ptmin) * 0.5f;

					if (!box_ray_overlap_check(&bbox, &bray))
						continue;
				}

				gwd.R = Matrix33(qrot*part.q);
				gwd.offset = pos + qrot*part.pos;
				gwd.scale = scale*part.scale;
				ncont = part.pPhysGeom->pGeom->Intersect(&aray,&gwd,0,&ip,pcontacts);
				for(; ncont>0 && (pcontacts[ncont-1].t>pHit->dist || pcontacts[ncont-1].n*dir>0); ncont--);
				if (ncont>0) {
					FillHit(pcontacts[ncont-1], i);
					pHit->partid = pent->m_parts[i].id;
					pHit->idmatOrg = pcontacts[ncont-1].id[0] + (part.surface_idx+1 & pcontacts[ncont-1].id[0]>>31);
					pHit->foreignIdx = part.pPhysGeom->pGeom->GetForeignIdx(pcontacts[ncont-1].iPrim[0]);
				}
			}
		}
	}	else
		return ((CPhysArea*)pient)->RayTrace(origin,dir,pHit,pp);

	return isneg(pHit->dist-1E9);
}


CPhysicalEntity *CPhysicalWorld::CheckColliderListsIntegrity()
{
	int i,j,k;
	CPhysicalEntity *pent;
	for(i=1;i<=2;i++) for(pent=m_pTypedEnts[i];pent;pent=pent->m_next)
		for(j=0;j<pent->m_nColliders;j++) if (pent->m_pColliders[j]->m_iSimClass>0) {
			for(k=0;k<pent->m_pColliders[j]->m_nColliders && pent->m_pColliders[j]->m_pColliders[k]!=pent;k++);
			if (k==pent->m_pColliders[j]->m_nColliders)
				return pent;
		}
	return 0;
}


void CPhysicalWorld::GetMemoryStatistics(ICrySizer *pSizer)
{
	static char *entnames[] = { "static entities", "physical entities", "physical entities", "living entities", "detached entities",
		"areas","triggers", "deleted entities" };
	int i,j,n;
	CPhysicalEntity *pent;
	ReadLock lockStep(m_lockStep);

#ifndef _LIB // Only when compiling as dynamic library
	{
		//SIZER_COMPONENT_NAME(pSizer,"Strings");
		//pSizer->AddObject( (this+1),string::_usedMemory(0) );
	}
	{
#ifndef NOT_USE_CRY_MEMORY_MANAGER
		SIZER_COMPONENT_NAME(pSizer,"STL Allocator Waste");
		CryModuleMemoryInfo meminfo;
		ZeroStruct(meminfo);
		CryGetMemoryInfoForModule( &meminfo );
		pSizer->AddObject( (this+2),meminfo.STL_wasted );
#endif
	}
#endif

/*#if CRY_PLATFORM_WINDOWS
	static char *sec_ids[] = { ".text",".textbss",".data",".idata" };
	static char *sec_names[] = { "code section","code section","data section","data section" };
	_IMAGE_DOS_HEADER *pMZ = (_IMAGE_DOS_HEADER*)GetModuleHandle("CryPhysics.dll");
	_IMAGE_NT_HEADERS *pPE = (_IMAGE_NT_HEADERS*)((char*)pMZ+pMZ->e_lfanew);
	IMAGE_SECTION_HEADER *sections = IMAGE_FIRST_SECTION(pPE);
	for(i=0;i<pPE->FileHeader.NumberOfSections;i++) for(j=0;j<CRY_ARRAY_COUNT(sec_ids);j++)
	if (!strncmp((char*)sections[i].Name, sec_ids[j], min(8,strlen(sec_ids[j])+1))) {
		SIZER_COMPONENT_NAME(pSizer, sec_names[j]);
		pSizer->AddObject((void*)sections[i].VirtualAddress, sections[i].Misc.VirtualSize);
	}
#endif*/

	{ SIZER_COMPONENT_NAME(pSizer,"world structures");
		ReadLock lockGeo(m_lockGeoman);
		pSizer->AddObject(this, sizeof(CPhysicalWorld));
		pSizer->AddObject(m_pTmpEntList, m_nEntsAlloc*sizeof(m_pTmpEntList[0]));
		pSizer->AddObject(m_pTmpEntList1, m_nEntsAlloc*sizeof(m_pTmpEntList1[0]));
		pSizer->AddObject(m_pTmpEntList2, m_nEntsAlloc*sizeof(m_pTmpEntList2[0]));
		pSizer->AddObject(m_pGroupMass, m_nEntsAlloc*sizeof(m_pGroupMass[0]));
		pSizer->AddObject(m_pMassList, m_nEntsAlloc*sizeof(m_pMassList[0]));
		pSizer->AddObject(m_pGroupIds, m_nEntsAlloc*sizeof(m_pGroupIds[0]));
		pSizer->AddObject(m_pGroupNums, m_nEntsAlloc*sizeof(m_pGroupNums[0]));
		pSizer->AddObject(m_pEntsById, m_nIdsAlloc*sizeof(m_pEntsById[0]));
		pSizer->AddObject(&m_entgrid.cells, GetGridSize(m_entgrid.cells, m_entgrid.size));
		pSizer->AddObject(m_gthunks, m_thunkPoolSz*sizeof(m_gthunks[0]));

		m_cubeMapStatic.GetMemoryStatistics(pSizer);
		m_cubeMapDynamic.GetMemoryStatistics(pSizer);

		pSizer->AddObject(m_pExplVictims, m_nExplVictimsAlloc*(sizeof(m_pExplVictims[0])+sizeof(m_pExplVictimsFrac[0])+sizeof(m_pExplVictimsImp[0])));
		pSizer->AddObject(m_pFreeContact, m_nContactsAlloc*sizeof(m_pFreeContact[0]));
		pSizer->AddObject(m_pFreeEntPart, m_nEntPartsAlloc*sizeof(m_pFreeEntPart[0]));
		if (m_entgrid.bHasPODGrid) {
			for(i=j=0; i<(m_entgrid.size.x*m_entgrid.size.y>>(3+m_entgrid.log2PODscale)*2); j+=m_entgrid.pPODcells[i++]!=0);
			pSizer->AddObject(m_entgrid.pPODcells, j*sizeof(pe_PODcell)*64+(m_entgrid.size.x*m_entgrid.size.y*sizeof(m_entgrid.pPODcells[0])>>(3+m_entgrid.log2PODscale)*2));
		}
	}

	{ SIZER_COMPONENT_NAME(pSizer,"world queues");
		EventChunk *pChunk;
		for(n=0,pChunk=m_pFirstEventChunk;pChunk;pChunk=pChunk->next,n++);
		pSizer->AddObject(pChunk, n*EVENT_CHUNK_SZ);
		pSizer->AddObject(m_pQueueSlots, m_nQueueSlots*(sizeof(int*)+QUEUE_SLOT_SZ));
		pSizer->AddObject(m_pQueueSlotsAux, m_nQueueSlotsAux*(sizeof(int*)+QUEUE_SLOT_SZ));
		pSizer->AddObject(m_rwiQueue, m_rwiQueueAlloc*sizeof(m_rwiQueue[0]));
		pSizer->AddObject(m_pwiQueue, m_pwiQueueAlloc*sizeof(m_pwiQueue[0]));
		pSizer->AddObject(m_pRwiHitsHead, m_rwiHitsPoolSize*sizeof(ray_hit));
	}

	void GetRBMemStats(ICrySizer *pSizer);
	GetRBMemStats(pSizer);

	{	SIZER_COMPONENT_NAME(pSizer,"placeholders");
		pSizer->AddObject(m_pPlaceholders, m_nPlaceholderChunks*(sizeof(CPhysicalPlaceholder)*PLACEHOLDER_CHUNK_SZ+sizeof(CPhysicalPlaceholder*)));
		pSizer->AddObject(m_pPlaceholderMap, ((size_t)m_nPlaceholderChunks<<PLACEHOLDER_CHUNK_SZLG2-5)*sizeof(int));
	}

	{ SIZER_COMPONENT_NAME(pSizer,"entities");
		for(i=0;i<=7;i++) {
			SIZER_COMPONENT_NAME(pSizer,entnames[i]);
			for(pent=m_pTypedEnts[i]; pent; pent=pent->m_next)
				pent->GetMemoryStatistics(pSizer);
		}
		{	SIZER_COMPONENT_NAME(pSizer,"hidden entities");
			for(pent=m_pHiddenEnts; pent; pent=pent->m_next)
				pent->GetMemoryStatistics(pSizer);
		}
	}
	{	SIZER_COMPONENT_NAME(pSizer,"areas");
		for(CPhysArea *pArea=m_pGlobalArea; pArea; pArea=pArea->m_next)
			pArea->GetMemoryStatistics(pSizer);
	}

	{ SIZER_COMPONENT_NAME(pSizer,"geometries");
		if (m_pHeightfield[0]) m_pHeightfield[0]->m_parts[0].pPhysGeom->pGeom->GetMemoryStatistics(pSizer);
		if (m_pHeightfield[1]) m_pHeightfield[1]->m_parts[0].pPhysGeom->pGeom->GetMemoryStatistics(pSizer);
		pSizer->AddObject(m_pGeoms, m_nGeomChunks*sizeof(m_pGeoms[0]));
		for(i=0;i<m_nGeomChunks;i++) {
			n = GEOM_CHUNK_SZ&i-m_nGeomChunks+1>>31 | m_nGeomsInLastChunk&m_nGeomChunks-2-i>>31;
			pSizer->AddObject(m_pGeoms[i], n*sizeof(m_pGeoms[i][0]));
			for(j=0;j<n;j++) if (m_pGeoms[i][j].pGeom)
				m_pGeoms[i][j].pGeom->GetMemoryStatistics(pSizer);
		}
	}

	{ SIZER_COMPONENT_NAME(pSizer,"external geometries");
		pSizer->AddObject(&m_sizeExtGeoms, m_sizeExtGeoms);
	}

	{ SIZER_COMPONENT_NAME(pSizer,"Static TriMesh Data");
		TriMeshStaticData::GetMemoryUsage(pSizer);
	}
}


void CPhysicalWorld::AddEntityProfileInfo(CPhysicalEntity *pent,int nTicks)
{
	if (m_vars.bProfileGroups)
		m_grpProfileData[GetEntityProfileType(pent)].nTicks += nTicks;

	if (m_nProfiledEnts==CRY_ARRAY_COUNT(m_pEntProfileData) &&
			nTicks<=m_pEntProfileData[m_nProfiledEnts-1].nTicksStep || m_vars.bSingleStepMode)
		return;

	int i;
	WriteLock lock(m_lockEntProfiler);
	phys_profile_info ppi;
	for(i=0;i<m_nProfiledEnts && m_pEntProfileData[i].pEntity!=pent;i++);
	if (i==m_nProfiledEnts) {
		ppi.pEntity = pent;
		ppi.nTicks=ppi.nTicksStep=nTicks; ppi.nCalls=1;
		ppi.nTicksPeak=ppi.nCallsPeak=ppi.nTicksAvg=ppi.peakAge=ppi.nTicksLast=0; ppi.nCallsAvg=ppi.nCallsLast=0;
		ppi.id = pent->m_id;
		ppi.pName = m_pRenderer ? m_pRenderer->GetForeignName(pent->m_pForeignData,
				pent->m_iForeignData,pent->m_iForeignFlags) : "noname";
	}	else {
		ppi = m_pEntProfileData[i];
		ppi.nTicksStep &= -ppi.nTicks>>31;
		ppi.nTicks += nTicks; ppi.nCalls++;
		nTicks = (ppi.nTicksStep = max(ppi.nTicksStep,nTicks));
		memmove(m_pEntProfileData+i,m_pEntProfileData+i+1, (--m_nProfiledEnts-i)*sizeof(m_pEntProfileData[0]));
	}

	int iBound[2]={ -1,m_nProfiledEnts };
	do {
		i = iBound[0]+iBound[1]>>1;
		PREFAST_ASSUME(i>0 && i<CRY_ARRAY_COUNT(m_pEntProfileData));
		iBound[isneg(m_pEntProfileData[i].nTicksStep-nTicks)] = i;
	} while(iBound[1]>iBound[0]+1);
	m_nProfiledEnts = min(m_nProfiledEnts+1, (int)(CRY_ARRAY_COUNT(m_pEntProfileData)));
	if ((i=iBound[0]+1)<m_nProfiledEnts) {
		memmove(m_pEntProfileData+i+1,m_pEntProfileData+i, (m_nProfiledEnts-1-i)*sizeof(m_pEntProfileData[0]));
		m_pEntProfileData[i] = ppi;
	}
}


void CPhysicalWorld::AddFuncProfileInfo(const char *name, int nTicks)
{
	WriteLock lock(m_lockFuncProfiler);
	int i,iBound[2]={ -1,m_nProfileFunx };
	if (m_nProfileFunx) do {
		i = iBound[0]+iBound[1]>>1;
		iBound[isneg((int)(name-m_pFuncProfileData[i].pName))] = i;
	}	while(iBound[1]>iBound[0]+1);
	if ((i=iBound[0])<0 || m_pFuncProfileData[i].pName!=name) {
		++i;
		if (m_nProfileFunx==m_nProfileFunxAlloc) {
			if (m_nProfileFunx>=64)
				return;
			ReallocateList(m_pFuncProfileData, m_nProfileFunx,m_nProfileFunxAlloc+=16);
		}
		memmove(m_pFuncProfileData+i+1, m_pFuncProfileData+i, sizeof(phys_profile_info)*(m_nProfileFunx-i));
		m_pFuncProfileData[i].nTicks=m_pFuncProfileData[i].nTicksPeak=m_pFuncProfileData[i].peakAge = 0;
		m_pFuncProfileData[i].nCalls=m_pFuncProfileData[i].nCallsPeak = 0;
		m_pFuncProfileData[i].pName = name;
		m_nProfileFunx++;
	}
	m_pFuncProfileData[i].nTicks += nTicks;
	m_pFuncProfileData[i].nCalls++;
	m_pFuncProfileData[i].id = 0;
}


void CPhysicalWorld::AddEventClient(int type, int (*func)(const EventPhys*), int bLogged, float priority)
{
	RemoveEventClient(type,func,bLogged);
	WriteLock lock(m_lockEventClients);
	EventClient *pSlot=new EventClient, *pCurSlot, *pSlot0=m_pEventClients[type][bLogged];
	memset(pSlot,0,sizeof(EventClient));
	pSlot->priority = priority;
	pSlot->OnEvent = func;
	if (pSlot0 && pSlot0->priority>priority) {
		for(pCurSlot=m_pEventClients[type][bLogged]; pCurSlot->next && pCurSlot->next->priority>priority; pCurSlot=pCurSlot->next);
		pSlot->next = pCurSlot->next; pCurSlot->next = pSlot;
	} else
		(m_pEventClients[type][bLogged] = pSlot)->next = pSlot0;
}

int CPhysicalWorld::RemoveEventClient(int type, int (*func)(const EventPhys*), int bLogged)
{
	WriteLock lock(m_lockEventClients);
	EventClient *pSlot=m_pEventClients[type][bLogged];
	if (!pSlot)
		return 0;
	if (pSlot->OnEvent==func) {
		m_pEventClients[type][bLogged] = pSlot->next;
		delete pSlot;	return 1;
	}
	for(; pSlot->next && pSlot->next->OnEvent!=func; pSlot=pSlot->next);
	if (pSlot->next) {
		EventClient *pDelSlot = pSlot->next;
		pSlot->next = pSlot->next->next;
		delete pDelSlot; return 1;
	}
	return 0;
}


EventPhys *CPhysicalWorld::AllocEvent(int id,int sz)
{
	if (m_pFreeEvents[id]==0) {
		if (m_szCurEventChunk+sz > EVENT_CHUNK_SZ) {
			EventChunk *pNewChunk = (EventChunk*)(new char[sizeof(EventChunk)+max(sz,EVENT_CHUNK_SZ)]);
			pNewChunk->next = 0; m_pCurEventChunk->next = pNewChunk;
			m_pCurEventChunk = pNewChunk;
			m_szCurEventChunk = 0;
		}
		m_pFreeEvents[id] = (EventPhys*)((char*)(m_pCurEventChunk+1)+m_szCurEventChunk);
		m_szCurEventChunk += sz;
		m_pFreeEvents[id]->idval = id;
		m_pFreeEvents[id]->next = 0;
	}
	EventPhys *pSlot = m_pFreeEvents[id];
	m_pFreeEvents[id] = m_pFreeEvents[id]->next;
	m_nEvents[id]++;
	return pSlot;
}

uint32 CPhysicalWorld::GetPumpLoggedEventsTicks()
{
	uint32 ticks = m_nPumpLoggedEventsHits;
	m_nPumpLoggedEventsHits = 0;
	return ticks;
}

void CPhysicalWorld::PumpLoggedEvents()
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
#ifdef ENABLE_LW_PROFILERS
	//simple timer shown in r_DisplayInfo=3
	LARGE_INTEGER pumpStart, pumpEnd;
	QueryPerformanceCounter(&pumpStart);
	uint64 startYields = 0ULL;
#endif
	EventPhys *pEventFirst,*pEvent,*pEventLast,*pEvent_next;
	ray_hit *pLastPoolHit=0;
	int lastPoolHitBlockSize;
	{ WriteLock lock(m_lockEventsQueue);
		pEventFirst = m_pEventFirst;
		m_pEventFirst = m_pEventLast = 0;
		for(int i=0; i<EVENT_TYPES_NUM; i++) m_nEvents[i] = 0;
		m_iLastLogPump++;
	}

	unsigned int bNotZeroStep = m_vars.lastTimeStep==0.f ? 0 : (unsigned int)-1;
	EventClient *pClient,*pClientNext;
	for(pEvent=pEventFirst; pEvent; pEvent=pEvent->next)
		if (!(pEvent->idval<=EventPhysCollision::id && (((CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[0])->m_iDeletionTime |
																									((CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[1])->m_iDeletionTime) ||
					pEvent->idval>EventPhysCollision::id && ((CPhysicalEntity*)((EventPhysMono*)pEvent)->pEntity)->m_iDeletionTime))
		{
			if (pEvent->idval==EventPhysPostStep::id) {
				EventPhysPostStep* pepps = (EventPhysPostStep*)pEvent;
				if (iszero(pepps->idStep-m_idStep)&bNotZeroStep)	// Dont emit logs past the current frame, unless timestep is zero
					break;
				else if (pepps->idStep<m_idStep-1 && ((CPhysicalPlaceholder*)pepps->pEntity)->m_iSimClass>1+m_vars.bSingleStepMode*8 ||
								((CPhysicalPlaceholder*)pepps->pEntity)->m_bProcessed & PENT_SETPOSED ||
								((CPhysicalEntity*)pepps->pEntity)->m_iDeletionTime)
					continue;
			}
			int bRWIorPWI = iszero(pEvent->idval-EventPhysRWIResult::id)+iszero(pEvent->idval-EventPhysPWIResult::id);
			if (bRWIorPWI && ((EventPhysRWIResult*)pEvent)->OnEvent)
				((EventPhysRWIResult*)pEvent)->OnEvent((EventPhysRWIResult*)pEvent);
			else for(pClient=m_pEventClients[pEvent->idval][1]; pClient; pClient=pClientNext) {
				pClientNext = pClient->next;
				BLOCK_PROFILER(pClient->ticks);
				int bContinue = pClient->OnEvent(pEvent);
				bContinue += bRWIorPWI;
				if (!bContinue)
					break;
			}

			if (pEvent->idval==EventPhysRWIResult::id && ((EventPhysRWIResult*)pEvent)->bHitsFromPool)
				pLastPoolHit = ((EventPhysRWIResult*)pEvent)->pHits+(lastPoolHitBlockSize=((EventPhysRWIResult*)pEvent)->nMaxHits)-1;
		}
	pEventLast = pEvent;

	for(pClient=m_pEventClients[EventPhysPostPump::id][1]; pClient; pClient=pClient->next) {
		BLOCK_PROFILER(pClient->ticks);
		int bContinue = pClient->OnEvent(NULL);
	}

#ifndef PHYS_FUNC_PROFILER_DISABLED
	int iEvent,iClient;
	for(iEvent=0;iEvent<EVENT_TYPES_NUM;iEvent++)
		for(pClient=m_pEventClients[iEvent][1],iClient=0; pClient; pClient=pClient->next,iClient++) {
			if (pClient->ticks*300 > m_vars.ticksPerSecond) {
				cry_sprintf(pClient->tag, "PhysEventHandler(%d,%d)", iEvent,iClient);
				AddFuncProfileInfo(pClient->tag,pClient->ticks);
			}
			pClient->ticks = 0;
		}
#endif

	{ WriteLock lock(m_lockEventsQueue);
		for(pEvent=pEventFirst; pEvent!=pEventLast; pEvent=pEvent_next) {
			pEvent_next = pEvent->next;
			pEvent->next = m_pFreeEvents[pEvent->idval]; m_pFreeEvents[pEvent->idval] = pEvent;
		}
		if (pEventLast) {
			CPhysicalEntity *pent;
			for(pEvent=pEventLast; pEvent; pEvent=pEvent->next)
				if (pEvent->idval==EventPhysRWIResult::id) {
					EventPhysRWIResult *pEventRWI = (EventPhysRWIResult*)pEvent;
					int i,bNoSolidHits = isneg(pEventRWI->pHits[0].dist);
					for(i=0;i<pEventRWI->nHits;i++) if (pent = (CPhysicalEntity*)pEventRWI->pHits[i+bNoSolidHits].pCollider)
						pent->m_iDeletionTime += isneg(3-pent->m_iDeletionTime);
				}	else if (pEvent->idval>EventPhysCollision::id) {
					pent = (CPhysicalEntity*)((EventPhysMono*)pEvent)->pEntity;
					pent->m_iDeletionTime += isneg(3-pent->m_iDeletionTime); // only increase for "real" deletion times (>3)
				}	else {
					pent = (CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[0];
					pent->m_iDeletionTime += isneg(3-pent->m_iDeletionTime);
					pent = (CPhysicalEntity*)((EventPhysStereo*)pEvent)->pEntity[1];
					pent->m_iDeletionTime += isneg(3-pent->m_iDeletionTime);
				}
			for(pEvent=pEventLast; pEvent->next; pEvent=pEvent->next);
			pEvent->next = m_pEventFirst;
			m_pEventFirst = pEventLast;
			if (!m_pEventLast)
				m_pEventLast = pEvent;
			//(m_pEventLast ? m_pEventLast->next : m_pEventFirst) = pEventLast;
			//for(m_pEventLast=pEventLast; m_pEventLast->next; m_pEventLast=m_pEventLast->next);
		}
	}
	if (pLastPoolHit) {
		WriteLock lockH(m_lockRwiHitsPool);
		int i; for(i=1; i<lastPoolHitBlockSize && pLastPoolHit[-i].next==pLastPoolHit-i+1; i++);
		if (i<lastPoolHitBlockSize)// || pLastPoolHit->next->next!=pLastPoolHit->next+1)
			CryLog("Error: queued RWI hits pool corrupted");
		else {
			m_pRwiHitsHead = pLastPoolHit->next;
			m_rwiPoolEmpty = m_pRwiHitsTail->next==m_pRwiHitsHead;
		}
	}

	{ WriteLock lockfp(m_lockFuncProfiler);
		int i,j;
		for(i=j=0;i<m_nProfileFunx;i++) if (++m_pFuncProfileData[i].id<20) {
			if (i!=j)
				m_pFuncProfileData[j] = m_pFuncProfileData[i];
			++j;
		}
		m_nProfileFunx = j;
	}
#ifdef ENABLE_LW_PROFILERS
	QueryPerformanceCounter(&pumpEnd);
	uint64 endYields = 0ULL;
	m_nPumpLoggedEventsHits += (uint32)(pumpEnd.QuadPart-pumpStart.QuadPart) - (uint32)(endYields - startYields);
#endif
}

void CPhysicalWorld::ClearLoggedEvents()
{
	EventPhys *pEvent,*pEvent_next;
	WriteLock lock(m_lockEventsQueue);
	for(pEvent=m_pEventFirst; pEvent; pEvent=pEvent_next) {
		pEvent_next = pEvent->next;
		pEvent->next = m_pFreeEvents[pEvent->idval]; m_pFreeEvents[pEvent->idval] = pEvent;
	}
	m_pEventFirst = m_pEventLast = 0;
	m_iLastLogPump = -1;
	for(CPhysicalEntity *pent=m_pTypedEnts[7]; pent; pent=pent->m_next)
		pent->m_iDeletionTime = min(4, pent->m_iDeletionTime);
	m_rwiPoolEmpty = 1; m_pRwiHitsHead = m_pRwiHitsTail->next;
}


#ifdef PHYSWORLD_SERIALIZATION
void SerializeGeometries(CPhysicalWorld *pWorld, const char *fname,int bSave);
void SerializeWorld(CPhysicalWorld *pWorld, const char *fname,int bSave);

int CPhysicalWorld::SerializeWorld(const char *fname, int bSave)
{
	::SerializeWorld(this,fname,bSave);
	return 1;
}
int CPhysicalWorld::SerializeGeometries(const char *fname, int bSave)
{
	::SerializeGeometries(this,fname,bSave);
	return 1;
}
#else
int CPhysicalWorld::SerializeWorld(const char *fname, int bSave) { return 0; }
int CPhysicalWorld::SerializeGeometries(const char *fname, int bSave) { return 0; }
#endif


int CPhysicalWorld::AddExplosionShape(IGeometry *pGeom, float size,int idmat, float probability)
{
	int i,j,bCreateConstraint=idmat>>16 & 1;
	idmat &= 0xFFFF;
	for(i=0;i<m_nExpl;i++) if (m_pExpl[i].pGeom==pGeom && m_pExpl[i].idmat==idmat)
		return -1;
	if (m_nExpl==m_nExplAlloc)
		ReallocateList(m_pExpl, m_nExpl,m_nExplAlloc+=16);

	for(i=0;i<m_nExpl && m_pExpl[i].idmat<=idmat;i++);
	memmove(m_pExpl+i+1, m_pExpl+i, (m_nExpl-i)*sizeof(m_pExpl[0]));
	for(int explIdx = i + 1; explIdx <= m_nExpl; ++ explIdx)
		++ m_pExpl[explIdx].iFirstByMat;
	if (i>0 && m_pExpl[i-1].idmat==idmat) {
		m_pExpl[i].iFirstByMat = m_pExpl[i-1].iFirstByMat;
		m_pExpl[i].nSameMat = m_pExpl[i-1].nSameMat+1;
		int jc = m_pExpl[i].iFirstByMat+m_pExpl[i-1].nSameMat;
		for(j=m_pExpl[i].iFirstByMat; j < jc; j++)
			m_pExpl[j].nSameMat++;
	}	else {
		m_pExpl[i].iFirstByMat = i;
		m_pExpl[i].nSameMat = 1;
	}
	if (pGeom->GetType()==GEOM_TRIMESH) {
		mesh_data *pmd = (mesh_data*)pGeom->GetData();
		memset(pmd->pMats, 100, pmd->nTris);
	}

	m_pExpl[i].id = m_idExpl++;
	(m_pExpl[i].pGeom = pGeom)->AddRef();
	m_pExpl[i].size = size;
	m_pExpl[i].rsize = 1/size;
	m_pExpl[i].idmat = idmat;
	m_pExpl[i].probability = probability;
	m_pExpl[i].bCreateConstraint = bCreateConstraint;
	m_nExpl++;

	return m_pExpl[i].id;
}

void CPhysicalWorld::RemoveExplosionShape(int id)
{
	int i,j;
	for(i=0;i<m_nExpl && m_pExpl[i].id!=id;i++);
	if (i==m_nExpl)
		return;
	m_pExpl[i].pGeom->Release();
	for(j=m_pExpl[i].iFirstByMat; j<m_pExpl[i].iFirstByMat+m_pExpl[i].nSameMat; j++)
		m_pExpl[j].nSameMat--;
	for(; j<m_nExpl; j++)
		m_pExpl[j].iFirstByMat--;
	memmove(m_pExpl+i, m_pExpl+i+1, (m_nExpl-1-i)*sizeof(m_pExpl[0]));
	m_nExpl--;
}

// disable overflow warning

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winteger-overflow"
#endif

#if defined(__GNUC__)
#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
        #pragma GCC diagnostic ignored "-Woverflow"
#else
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Woverflow"
#endif
#endif
IGeometry *CPhysicalWorld::GetExplosionShape(float size,int idmat, float &scale, int &bCreateConstraint)
{
	int i,j,mask,ibound[2]={ -1,m_nExpl };
	float sum,probabilitySum,f;

	if (!m_nExpl || size<=0)
		return 0;
	idmat &= 127;
	do {
		i = ibound[0]+ibound[1]>>1;
		ibound[isneg(idmat-m_pExpl[i].idmat)] = i;
	} while(ibound[1] > ibound[0]+1);
	if (ibound[0]<0 || m_pExpl[ibound[0]].idmat!=idmat)
		return 0;

	ibound[1] = m_pExpl[ibound[0]].iFirstByMat+m_pExpl[ibound[0]].nSameMat;
	j = ibound[0] = m_pExpl[ibound[0]].iFirstByMat;
	for(i=j+1; i<ibound[1]; i++)	{
		mask = -isneg(fabs_tpl(m_pExpl[i].size-size) - fabs_tpl(m_pExpl[j].size-size));
		j = i&mask | j&~mask;
	}

	for(i=ibound[0],probabilitySum=0.0f; i<ibound[1]; i++)
		probabilitySum += m_pExpl[i].probability*iszero(m_pExpl[i].size-m_pExpl[j].size);
	f = cry_random(0.0f, probabilitySum);
	for(i=ibound[0],sum=0; i<ibound[1] && sum<f; i++)
		sum += m_pExpl[i].probability*iszero(m_pExpl[i].size-m_pExpl[j].size);
	scale = size*m_pExpl[i-1].rsize;
	bCreateConstraint = m_pExpl[i-1].bCreateConstraint;
	return m_pExpl[i-1].pGeom;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(__GNUC__)
#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
        #pragma GCC diagnostic error "-Woverflow"
#else
	#pragma GCC diagnostic pop
#endif
#endif

int CPhysicalWorld::SetWaterManagerParams(pe_params *params)
{
	if (params->type==pe_params_waterman::type_id && !is_unused(((pe_params_waterman*)params)->posViewer))
		m_posViewer = ((pe_params_waterman*)params)->posViewer;
	WriteLock lock(m_lockWaterMan);
	if (!m_pWaterMan || m_pWaterMan->m_pArea) {
		if ((params->type!=pe_params_waterman::type_id) || is_unused(((pe_params_waterman*)params)->nExtraTiles))
			return 0;
		CWaterMan *pWaterMan = m_pWaterMan;
		(m_pWaterMan = new CWaterMan(this))->m_next = pWaterMan;
		if (pWaterMan) 
			pWaterMan->m_prev = m_pWaterMan;
	}
	return m_pWaterMan->SetParams(params);
}
int CPhysicalWorld::GetWaterManagerParams(pe_params *params)
{
	ReadLock lock(m_lockWaterMan);
	return m_pWaterMan && !m_pWaterMan->m_pArea ? m_pWaterMan->GetParams(params):0;
}
int CPhysicalWorld::GetWatermanStatus(pe_status *status)
{
	ReadLock lock(m_lockWaterMan);
	return m_pWaterMan && !m_pWaterMan->m_pArea ? m_pWaterMan->GetStatus(status):0;
}
void CPhysicalWorld::DestroyWaterManager()
{
	WriteLock lock(m_lockWaterMan);
	if (CWaterMan *pWaterMan = m_pWaterMan) {
		m_pWaterMan = pWaterMan->m_next; delete pWaterMan;
	}
}


void CPhysicalWorld::GetEntityMassAndCom(IPhysicalEntity *pIEnt, float &mass, Vec3 &com)
{
  CPhysicalEntity* pent = (CPhysicalEntity*) pIEnt; // CRAP: need to check this is CPhysicalEntity

	int i,j;
	for(j=i=0,mass=0; i<pent->m_nParts; i++) {
		j += i-j & -isneg(pent->m_parts[i].mass-pent->m_parts[j].mass);
		mass += pent->m_parts[i].mass;
	}
	//if (pent->m_nParts)
	//	com = pent->m_pos+pent->m_qrot*(pent->m_parts[j].pos+pent->m_parts[j].q*pent->m_parts[j].pPhysGeomProxy->origin*pent->m_parts[j].scale);
	//else com.zero();
  com = pent->m_BBox[0] + (pent->m_BBox[1] - pent->m_BBox[0]) / 2.0f;
}

void CPhysicalWorld::SavePhysicalEntityPtr(TSerialize ser, IPhysicalEntity* pIEnt)
{
  CPhysicalEntity* pent = (CPhysicalEntity*) pIEnt; // CRAP: need to check this is CPhysicalEntity

	ser.BeginGroup("entity_ptr");
	int i;
	float mass; Vec3 com;
	ser.Value("id", i=pent->m_id);
	ser.Value("simclass", i=pent->m_iSimClass);
	GetEntityMassAndCom(pent, mass,com);
	ser.Value("com", com);
	ser.Value("mass", mass);
	ser.EndGroup();
}

IPhysicalEntity* CPhysicalWorld::LoadPhysicalEntityPtr(TSerialize ser)
{
	int i,iSimClass,iSimClass1;
	CPhysicalEntity *pent,**pents;
	float mass,mass1;
	Vec3 com,com1;
	ser.BeginGroup("entity_ptr");
	ser.Value("id", i);
	ser.Value("simclass", iSimClass);
	ser.Value("com", com);
	ser.Value("mass", mass);
	ser.EndGroup();
	iSimClass1 = (unsigned int)(iSimClass-1)<2u ? (iSimClass^3):iSimClass;

	if (pent = (CPhysicalEntity*)GetPhysicalEntityById(i)) {
		GetEntityMassAndCom(pent, mass1,com1);
		if ((pent->m_iSimClass==iSimClass || pent->m_iSimClass==iSimClass1 || iSimClass==-1) &&
				fabs_tpl(mass-mass1)<=fabs_tpl(min(mass,mass1))*0.01f && (com-com1).len2()<sqr(0.01f))
			return pent;
	}

	for(i=GetEntitiesAround(com-Vec3(0.1f),com+Vec3(0.1f),pents,1<<iSimClass|1<<iSimClass1)-1;i>=0;i--) {
		GetEntityMassAndCom(pents[i],mass1,com1);
		if (fabs_tpl(mass-mass1)<=fabs_tpl(min(mass,mass1))*0.01f && (com-com1).len2()<sqr(0.01f))
			return pents[i];
	}
	return 0;
}


int CPhysicalEntity::SetStateFromTypedSnapshot(TSerialize ser, int iSnapshotType, int flags)
{
	static CPhysicalEntity g_entStatic(0);
	static CRigidEntity g_entRigid(0);
	static CWheeledVehicleEntity g_entWheeled(0);
	static CLivingEntity g_entLiving(0);
	static CParticleEntity g_entParticle(0);
	static CArticulatedEntity g_entArticulated(0);
	static CRopeEntity g_entRope(0);
	static CSoftEntity g_entSoft(0);
	static CPhysicalEntity *g_pTypedEnts[] = { &g_entStatic,&g_entStatic,&g_entRigid,&g_entWheeled,
		&g_entLiving,&g_entParticle,&g_entArticulated,&g_entRope,&g_entSoft };

	if (GetType()==iSnapshotType)
		return SetStateFromSnapshot(ser,flags);
	return g_pTypedEnts[min((int)(CRY_ARRAY_COUNT(g_pTypedEnts)),max(0,iSnapshotType))]->
		SetStateFromSnapshot(ser,flags);
}

void CPhysicalWorld::SerializeGarbageTypedSnapshot( TSerialize ser, int iSnapshotType, int flags )
{
	static CPhysicalEntity g_entStatic(0);
	static CRigidEntity g_entRigid(0);
	static CWheeledVehicleEntity g_entWheeled(0);
	static CLivingEntity g_entLiving(0);
	static CParticleEntity g_entParticle(0);
	static CArticulatedEntity g_entArticulated(0);
	static CRopeEntity g_entRope(0);
	static CSoftEntity g_entSoft(0);
	static CPhysicalEntity *g_pTypedEnts[] = { &g_entStatic,&g_entStatic,&g_entRigid,&g_entWheeled,
		&g_entLiving,&g_entParticle,&g_entArticulated,&g_entRope,&g_entSoft };

	g_pTypedEnts[min((int)(CRY_ARRAY_COUNT(g_pTypedEnts)),max(0,iSnapshotType))]->
		GetStateSnapshot(ser,flags);
}

IPhysicalEntityIt* CPhysicalWorld::GetEntitiesIterator()
{
  return new CPhysicalEntityIt(this);
}

phys_job_info& GetJobProfileInst(int ijob) { return g_pPhysWorlds[0]->GetJobProfileInst(ijob); }

struct linkedpt {
	linkedpt *next[2];
	Vec2 pt;
};
inline linkedpt* unlink(linkedpt* p) { p->next[0]->next[1]=p->next[1]; p->next[1]->next[0]=p->next[0]; return p; }
inline linkedpt* link_after(linkedpt* plist,linkedpt *p) {
	p->next[0] = plist; p->next[1] = plist->next[1]; 
	plist->next[1]->next[0] = p; plist->next[1] = p; return p;
}

linkedpt* crop_poly2d(linkedpt* p0, const Vec2* bounds, linkedpt* pbuf)
{
	linkedpt *p=p0; do { 
		if (inrange(p->pt.x,bounds[0].x,bounds[1].x)+inrange(p->pt.y,bounds[0].y,bounds[1].y)<2) 
			goto crop_needed; 
	} while ((p=p->next[1])!=p0);
	return p0;
	crop_needed:

	linkedpt pstart,*plast,*pnew,*pnext; 
	for(int i=0;i<4;i++) {
		pstart.next[1]=pstart.next[0]=plast = &pstart;
		float b = bounds[i>>1][i&1], sg = (i&2)-1;
		int inside = (p=p0)->pt[i&1]*sg < b*sg;
		do { pnext = p->next[1];
			link_after(inside ? pstart.next[0]:pbuf->next[0], p);
			if ((p->pt[i&1]-b)*(pnext->pt[i&1]-b) < 0) {
				Vec2 dp = pnext->pt-p->pt;
				(pnew = unlink(pbuf->next[1]))->pt[i&1] = b;
				pnew->pt[i&1^1] = p->pt[i&1^1] + dp[i&1^1]*(b-p->pt[i&1])/dp[i&1];
				link_after(pstart.next[0], pnew); inside ^= 1;
			}
		} while((p=pnext)!=p0);
		p0=pstart.next[1]; unlink(&pstart);
	}
	return p0;
}

void CPhysicalWorld::RasterizeEntities(const grid3d& grid, uchar *rbuf, int objtypes, float massThreshold, const Vec3& offsBBox, const Vec3& sizeBBox, int flags)
{
	linkedpt pbuf[10];
	Vec3 bounds = grid.size*Diag33(grid.step), wbounds=Matrix33(grid.Basis.T()).Fabs()*bounds, n,pt;
	CPhysicalEntity **pents;
	Quat qBasis = Quat(grid.Basis);
	int iCaller=get_iCaller(), flagsAll=0, flagsAny=-1, ystride=grid.size.x;
	(flags & rwi_colltype_any ? flagsAny : flagsAll) = flags>>rwi_colltype_bit;

	Vec2 center2d=Vec2(grid.Basis*offsBBox), size2d=Vec2(Matrix33(grid.Basis).Fabs()*sizeBBox), bounds2d[2]={ center2d-size2d,center2d+size2d };
	Vec2i ibbox[2];	int i,j,ibuf; 
	for(i=0;i<2;i++) for(j=0;j<2;j++) ibbox[i][j] = max(0,min(grid.size[j]-1, float2int(bounds2d[i][j]*grid.stepr[j]-0.5f)));
	for(i=0;i<2;i++) for(j=0;j<2;j++) bounds2d[i][j] = (ibbox[i][j]+i)*grid.step[j];
	for(j=ibbox[0].y;j<=ibbox[1].y;j++) memset(rbuf+j*ystride+ibbox[0].x, 0, (ibbox[1].x-ibbox[0].x+1));

	for(int ient=GetEntitiesAround(grid.origin+max(Vec3(ZERO),offsBBox-sizeBBox),grid.origin+min(wbounds,offsBBox+sizeBBox),pents,objtypes)-1; ient>=0; ient--) 
		if (pents[ient]->GetMassInv()*massThreshold<=1.0f) for(int ipart=pents[ient]->GetUsedPartsCount(iCaller)-1; ipart>=0; ipart--) {
			const geom& part = pents[ient]->m_parts[pents[ient]->GetUsedPart(iCaller,ipart)];
			if (!(part.flags & flagsAny) || (part.flags & flagsAll)!=flagsAll)
				continue;
			QuatTS qPart2Grid(qBasis*pents[ient]->m_qrot*part.q, qBasis*(pents[ient]->m_qrot*part.pos+pents[ient]->m_pos-grid.origin), part.scale);
			int igeom = part.pPhysGeomProxy->pGeom->GetType();

			if (igeom==GEOM_TRIMESH || igeom==GEOM_BOX) {
				const mesh_data *md = (const mesh_data*)part.pPhysGeomProxy->pGeom->GetData();
				int nvtx = 3;
				if (igeom==GEOM_BOX) {
					static int boxidx[] = { 0,2,3,1, 0,4,6,2, 2,6,7,3, 3,7,5,1, 1,5,4,0, 4,5,7,6 };
					static Vec3 boxvtx[8], boxnorm[6] = { -ortz,-ortx,orty,ortx,-orty,ortz };
					static mesh_data boxmd; boxmd.nTris=6; boxmd.pVertices=strided_pointer<Vec3>(boxvtx); boxmd.pIndices=boxidx; boxmd.pNormals=boxnorm;
					const box *pbox = (const box*)md;
					for(i=0;i<8;i++) boxmd.pVertices[i].Set(pbox->size.x*((i*2&2)-1), pbox->size.y*((i&2)-1), pbox->size.z*((i>>1&2)-1));
					qPart2Grid = qPart2Grid*QuatT(!Quat(pbox->Basis),pbox->center);
					md = &boxmd; nvtx = 4;
				}
				for(int itri=0; itri<md->nTris; itri++) if ((n=qPart2Grid.q*md->pNormals[itri]).z>0) {
					float zmin=bounds.z*2, zmax=-bounds.z*2;
					for(j=0;j<nvtx;j++) {
						pt = qPart2Grid*md->pVertices[md->pIndices[itri*nvtx+j]];
						pbuf[j].pt = Vec2(pt); zmin=min(zmin,pt.z); zmax=max(zmax,pt.z);
					}
					if (min(bounds.z-zmin,zmax)<0)
						continue;
					float rnz=1/n.z, rx[2],dy[2]={1e10f,1e10f},rdy[2];
					for(i=0;i<(nvtx+1)*2;i++) pbuf[i].next[1]=pbuf+i+1,pbuf[i].next[0]=pbuf+i-1;
					pbuf[0].next[0] = pbuf+nvtx-1; pbuf[nvtx-1].next[1] = pbuf;
					pbuf[nvtx].next[0] = pbuf+(nvtx+1)*2-1; pbuf[(nvtx+1)*2-1].next[1] = pbuf+nvtx;
					linkedpt *p0 = crop_poly2d(pbuf,bounds2d,pbuf+nvtx), *p[2];

					p[0]=p[1]=p0; do { if (p[1]->pt.y<p[0]->pt.y)	p[0]=p[1]; } while ((p[1]=p[1]->next[1])!=p0);
					int iy = float2int(p[0]->pt.y*grid.stepr.y);
					for(p[1]=p[0]; ;iy++) {
						for(i=0; i<2; i++) { 
							for(rx[i]=grid.size.x*grid.step.x*(1-i); p[i]->next[i]->pt.y<(iy+0.5f)*grid.step.y; )
								if ((p[i]=p[i]->next[i])==p[i^1])
									goto finished;
							Vec2 dp = p[i]->next[i]->pt-p[i]->pt;
							if (dp.y!=dy[i])
								rdy[i] = 1/(dy[i]=dp.y);
							rx[i] = minmax(rx[i], p[i]->pt.x+((iy+0.5f)*grid.step.y-p[i]->pt.y)*dp.x*rdy[i], i);
						}
						int ix[2] = { float2int(rx[0]*grid.stepr.x), float2int(rx[1]*grid.stepr.x) };
						float zh = (pt*n-(ix[0]+0.5f)*grid.step.x*n.x-(iy+0.5f)*grid.step.y*n.y)*rnz;
						for(ibuf=(i=ix[0])+iy*ystride; i<ix[1]; i++,ibuf++,zh-=grid.step.x*n.x*rnz)
							rbuf[ibuf] = max((int)rbuf[ibuf], max(0,min(255,float2int(zh*grid.stepr.z-0.5f))));
					}
					finished:;
				}
			} else {
				box bbox; part.pPhysGeomProxy->pGeom->GetBBox(&bbox);
				Vec3 center=qPart2Grid*bbox.center, size=(Matrix33(qPart2Grid.q)*bbox.Basis.T()).Fabs()*bbox.size*qPart2Grid.s;
				Vec2 bbox2d[2] = { max(bounds2d[0],Vec2(center-size)), min(bounds2d[1],Vec2(center+size)) };
				const primitive *pprim = (const primitive*)part.pPhysGeomProxy->pGeom->GetData();
				QuatTS qGrid2Part = qPart2Grid.GetInverted();
				Vec3 org(0,0,grid.size.z*grid.step.z*2),dirn=qGrid2Part.q*-ortz*qPart2Grid.s;
				ray aray;	aray.dir = qGrid2Part.q*-org*qGrid2Part.s;
				prim_inters inters;	 
				for(i=0;i<2;i++) for(j=0;j<2;j++) ibbox[i][j] = float2int(bbox2d[i][j]*grid.stepr[j]-0.5f);
				for(org.y=((j=ibbox[0].y)+0.5f)*grid.step.y; j<=ibbox[1].y; j++,org.y+=grid.step.y) 
					for(ibuf=j*ystride+(i=ibbox[0].x),org.x=(ibbox[0].x+0.5f)*grid.step.x; i<=ibbox[1].x; i++,ibuf++,org.x+=grid.step.x)
						if ((aray.origin=qGrid2Part*org, g_Intersector.Check(igeom,ray::type, pprim,&aray, &inters)) && inters.n.z>0)
							rbuf[ibuf] = max((int)rbuf[ibuf], max(0,min(255,float2int((org.z-((inters.pt[0]-aray.origin)*dirn))*grid.stepr.z-0.5f))));
			}
		}
}
