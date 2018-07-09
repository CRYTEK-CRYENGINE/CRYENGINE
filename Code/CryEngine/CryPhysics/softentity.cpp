// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "overlapchecks.h"
#include "intersectionchecks.h"
#include "raybv.h"
#include "raygeom.h"
#include "singleboxtree.h"
#include "boxgeom.h"
#include "spheregeom.h"
#include "trimesh.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "rigidentity.h"
#include "softentity.h"
#include "tetrlattice.h"
#define m_bAwake m_bPermanent

static Vec3 g_lastposHost,g_vHost,g_wHost;
static quaternionf g_lastqHost(0,0,0,0);
volatile int CPhysicalEntity::g_lockProcessedAux = 0;


CSoftEntity::CSoftEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap) 
	: CPhysicalEntity(pworld, pHeap)
	, m_vtx(nullptr)
	, m_edges(nullptr)
	, m_pVtxEdges(nullptr)
	, m_nVtx(0)
	, m_nEdges(0)
	, m_nConnectedVtx(0)
	, m_nAttachedVtx(0)
	, m_offs0(ZERO)
	, m_qrot0(IDENTITY)
	, m_bMeshUpdated(0)
	, m_lastposHost(ZERO)
	, m_lastqHost(IDENTITY)
	, m_pTetrEdges(nullptr)
	, m_pTetrQueue(nullptr)
	, m_lastPos(ZERO)
	, m_timeStepFull(0.0f)
	, m_timeStepPerformed(0.0f)
	, m_timeStepSurplus(0.0f)
	, m_gravity(0.0f, 0.0f, -9.8f)
	, m_Emin(sqr(0.01f))
	, m_maxAllowedStep(0.1f)
	, m_nSlowFrames(0)
	, m_damping(0.0f)
	, m_accuracy(0.01f)
	, m_nMaxIters(20)
	, m_prevTimeInterval(0.0f)
	, m_bSkinReady(0)
	, m_maxMove(0.0f)
	, m_maxAllowedDist(1e10f)
	, m_thickness(0.04f)
	, m_ks(10.0f)
	, m_maxSafeStep(0.2f)
	, m_density(0.0f)
	, m_coverage(0.0f)
	, m_friction(0.0f)
	, m_impulseScale(0.05f)
	, m_explosionScale(0.001f)
	, m_collImpulseScale(1.0f)
	, m_maxCollImpulse(3000.0f)
	, m_collTypes(ent_static | ent_sleeping_rigid | ent_rigid | ent_living)
	, m_massDecay(0.0f)
	, m_kShapeStiffnessNorm(0.0f)
	, m_kShapeStiffnessTang(0.0f)
	, m_vtxvol(0.0f)
	, m_stiffnessAnim(0.0f)
	, m_stiffnessDecayAnim(0.0f)
	, m_dampingAnim(0.0f)
	, m_maxDistAnim(0.0f)
	, m_kRigid(0.0f)
	, m_maxLevelDenom(1.0f)
	, m_waterResistance(0.0f)
	, m_airResistance(0)
	, m_wind(ZERO)
	, m_wind0(ZERO)
	, m_wind1(ZERO)
	, m_windTimer(0.0f)
	, m_windVariance(0.2f)
	, m_pCore(nullptr)
	, m_pos0core(ZERO)
	, m_corevtx(nullptr)
	, m_nCoreVtx(0)
	, m_iLastLog(0)
	, m_pEvent(nullptr)
	, m_lockSoftBody(0)
{
	//CPhysicalPlaceholder
	m_iSimClass = 4;

	//CPhysicalEntity
	m_bAwake = 1;
	m_flags |= se_skip_longest_edges;
	m_collisionClass.type |= collision_class_soft;
}

CSoftEntity::~CSoftEntity()
{
	if (m_vtx) { 
		if (m_pWorld->m_bMassDestruction)
			for(int i=0;i<m_nVtx;i++) m_vtx[i].pContactEnt=0;
		delete[] m_vtx; m_vtx=0; 
	}
	if (m_edges) { delete[] m_edges; m_edges=0; }
	if (m_pVtxEdges) { delete[] m_pVtxEdges; m_pVtxEdges=0; }
	if (m_pTetrEdges) { delete[] m_pTetrEdges; m_pTetrEdges=0; }
	if (m_pTetrQueue) { delete[] m_pTetrQueue; m_pTetrQueue=0; }
	RemoveCore();
	m_nVtx=m_nEdges = 0; 
}

void CSoftEntity::RemoveCore() 
{
	if (m_corevtx) { delete[] m_corevtx; m_corevtx=0; }
	if (m_pCore && !m_pWorld->m_bMassDestruction) {
		m_pWorld->DestroyPhysicalEntity(m_pCore,0,1);	m_pCore = 0;
	}
}


void CSoftEntity::AlertNeighbourhoodND(int mode)
{
	for(int i=0;i<m_nVtx;i++) if (m_vtx[i].pContactEnt && (!m_vtx[i].bAttached || mode==0)) {
		m_vtx[i].pContactEnt->Release(); m_vtx[i].pContactEnt=0; m_vtx[i].bAttached=0;
	}
	RemoveCore();
}

int CSoftEntity::Awake(int bAwake, int iSource) 
{ 
	if ((m_bPermanent=bAwake) && !m_pCore) 
		m_nSlowFrames=0; 
	m_pWorld->RepositionEntity(this,2);
	return 1; 
}

int CSoftEntity::AddGeometry(phys_geometry *pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	if (!pgeom || pgeom->pGeom->GetType()!=GEOM_TRIMESH && !params->pLattice || m_nParts>0)
		return -1;

	ChangeRequest<pe_geomparams> req(this,m_pWorld,params,bThreadSafe,pgeom,id);
	if (req.IsQueued()) {
		WriteLock lock(m_lockPartIdx);
		if (id<0)
			*((int*)req.GetQueuedStruct()-1) = id = m_iLastIdx++;
		else 
			m_iLastIdx = max(m_iLastIdx,id+1);
		return id;
	}
	if (params->mass==0 && params->density==0)
		params->mass = 1;

	int res = CPhysicalEntity::AddGeometry(pgeom,params,id,1);
	WriteLock lock(m_lockUpdate);
	int i,j,i0,i1,bDegen,iedge,itri,itri0,ivtx,itrinew,nVtxEdges,(*pInfo)[3];
	float rvtxmass,len[3],vtxmass;
	RemoveCore();
	pgeom->pGeom->Lock(); pgeom->pGeom->Unlock();

	if (params->pLattice) {
		phys_geometry *pgeom0 = pgeom;
		CTetrLattice *pLattice = new CTetrLattice((CTetrLattice*)params->pLattice,1);
		pLattice->SetMesh((CTriMesh*)pgeom->pGeom);
		m_parts[m_nParts-1].pMatMapping = 0;
		m_parts[m_nParts-1].pLattice = pLattice;
		m_parts[m_nParts-1].flags |= geom_can_modify;
		int *pIdx = new int[pLattice->m_nTetr*12];

		for(i=itri=0;i<pLattice->m_nTetr;i++) for(j=0;j<4;j++) if (pLattice->m_pTetr[i].ibuddy[j]<i) 
			for(ivtx=3;ivtx>0;ivtx--) pIdx[itri++] = pLattice->m_pTetr[i].ivtx[j+5*(j&1)+(ivtx^-(j&1)) & 3];
		IGeometry *pGeom = m_pWorld->CreateMesh(pLattice->m_pVtx, pIdx,0,0,itri/3, mesh_SingleBB|mesh_shared_vtx,0.0f);
		m_parts[m_nParts-1].pPhysGeomProxy=pgeom = m_pWorld->RegisterGeometry(pGeom);
		pGeom->Release();
		delete[] pIdx;

		pGeom = pLattice->CreateSkinMesh(32);
		m_parts[m_nParts-1].pPhysGeom = m_pWorld->RegisterGeometry(pGeom);
		pGeom->Release();

		if (m_flags & se_rigid_core) {
			m_corevtx = new int[pLattice->m_nVtx];
			for(i=j=0,m_pos0core.zero();i<pLattice->m_nVtx;i++) if (pgeom->pGeom->PointInsideStatus(pLattice->m_pVtx[i]))
				m_pos0core += pLattice->m_pVtx[m_corevtx[j++] = i];
			if (m_nCoreVtx=j) {
				pe_params_pos pp;
				pp.pos = m_pos+m_qrot*((m_pos0core/=j)+params->pos); pp.q = m_qrot*params->q;
				m_pCore = (CRigidEntity*)m_pWorld->CreatePhysicalEntity(PE_RIGID,0.0f,&pp,m_pForeignData,0x5AFE);
				m_pCore->m_iForeignData = m_iForeignData;
				pe_geomparams gp;
				gp.pos = m_pos + m_qrot*params->pos;
				gp.pos = (gp.pos-pp.pos)*pp.q;
				gp.scale = params->scale;
				gp.flags &= ~geom_colltype_ray;
				gp.mass = j*params->mass/pLattice->m_nVtx;
				m_pCore->AddGeometry(pgeom0, &gp);
				m_pCore->m_flags |= pef_ignore_areas;
				pe_simulation_params sp; 
				sp.gravity.zero(); 
				sp.damping = m_damping;
				sp.minEnergy = 0.0f;
				m_pCore->SetParams(&sp);
				m_pCore->m_kwaterDensity = 0.0f;
			} else {
				delete[] m_corevtx; m_corevtx = 0;
			}
		}
		m_pWorld->UnregisterGeometry(pgeom0);
		
		m_flags &= ~se_skip_longest_edges;
	}
	
	CTriMesh *pMesh = (CTriMesh*)pgeom->pGeom;
	vtxmass = params->mass/pMesh->m_nVertices;
	rvtxmass = pMesh->m_nVertices/max(params->mass,0.0001f);
	m_vtxvol = 1.0f/(rvtxmass*params->density);
	m_density = params->density;

	m_vtx = new se_vertex[m_nVtx = pMesh->m_nVertices];
	memset(m_vtx, 0, m_nVtx*sizeof(se_vertex));
	for(i=0;i<m_nVtx;i++) {
		m_vtx[i].posorg = pMesh->m_pVertices[i];
		m_vtx[i].pos = m_qrot*(params->q*pMesh->m_pVertices[i]*params->scale+params->pos);
		m_vtx[i].massinv = rvtxmass; m_vtx[i].mass = vtxmass;
		m_vtx[i].vel.zero(); m_vtx[i].iSorted=m_vtx[i].bAttached = 0;
		m_vtx[i].pContactEnt = 0;	m_vtx[i].iContactNode = 0; m_vtx[i].ncontact.zero(); m_vtx[i].n.zero();	m_vtx[i].nmesh.zero();
		m_vtx[i].iStartEdge=0; m_vtx[i].iEndEdge=-1; m_vtx[i].idx=m_vtx[i].idx0=i;
		m_vtx[i].bFullFan = 0; m_vtx[i].area = 0;
	}
	m_offs0 = m_vtx[0].pos;
	for(i=1;i<m_nVtx;i++) m_vtx[i].pos -= m_vtx[0].pos;
	m_vtx[0].pos.zero();
	/*float rscale = 1.0f/m_parts[0].scale;
	for(i=0;i<m_nVtx;i++) 
		pMesh->m_pVertices[i] = ((m_vtx[i].pos+m_offs0-m_parts[0].pos)*rscale)*m_parts[0].q;*/
	
	if (params->pLattice) {
		CTetrLattice *pLattice = (CTetrLattice*)params->pLattice;
		int *pEdgeMask = new int[iedge = m_nVtx*(m_nVtx+1)+63>>6];
		memset(pEdgeMask, 0, iedge*sizeof(int));

		for(i=m_nEdges=0;i<pMesh->m_nTris;i++) for(j=0;j<3;j++) {
			i0 = pMesh->m_pIndices[i*3+j]; i1 = pMesh->m_pIndices[i*3+inc_mod3[j]];
			int mask=i1-i0>>31; i0^=i1&mask; i1^=i0&mask; i0^=i1&mask;
			i0 = (i1*(i1+1)>>1)+i0;
			m_nEdges += pEdgeMask[i0>>5]>>(i0&31) & 1 ^ 1;
			pEdgeMask[i0>>5] |= 1<<(i0&31);
		}

		memset(pEdgeMask, 0, iedge*sizeof(int));
		memset(m_edges = new se_edge[m_nEdges+1], 0, m_nEdges*sizeof(se_edge));
		for(i=m_nEdges=0;i<pMesh->m_nTris;i++) for(j=0;j<3;j++) {
			i0 = pMesh->m_pIndices[i*3+j]; i1 = pMesh->m_pIndices[i*3+inc_mod3[j]];
			int mask=i1-i0>>31; i0^=i1&mask; i1^=i0&mask; i0^=i1&mask;
			m_edges[m_nEdges].ivtx[0] = i0; m_edges[m_nEdges].ivtx[1] = i1;
			m_edges[m_nEdges].len0 = (pMesh->m_pVertices[i1]-pMesh->m_pVertices[i0]).len();
			i0 = (i1*(i1+1)>>1)+i0;
			m_nEdges += pEdgeMask[i0>>5]>>(i0&31) & 1 ^ 1;
			pEdgeMask[i0>>5] |= 1<<(i0&31);
		}
		delete[] pEdgeMask;

		m_pTetrEdges = new int[pLattice->m_nTetr*6];
		m_pVtxEdges = new int[m_nEdges*2];
		for(i=0;i<m_nEdges;i++) for(j=0;j<2;j++)
			m_vtx[m_edges[i].ivtx[j]].iStartEdge++;
		for(i=1,m_vtx[0].iEndEdge=0;i<m_nVtx;i++)
			m_vtx[i].iEndEdge = m_vtx[i-1].iEndEdge+m_vtx[i-1].iStartEdge;
		for(i=0;i<m_nVtx;i++) 
			m_vtx[i].iStartEdge = m_vtx[i].iEndEdge--;
		for(i=0;i<m_nEdges;i++) for(j=0;j<2;j++)
			m_pVtxEdges[++m_vtx[m_edges[i].ivtx[j]].iEndEdge] = i;
		for(i=iedge=0;i<pLattice->m_nTetr;i++) for(int ivtx0=0;ivtx0<3;ivtx0++) for(int ivtx1=ivtx0+1;ivtx1<4;ivtx1++,iedge++) {
			i0 = pLattice->m_pTetr[i].ivtx[ivtx0]; i1 = pLattice->m_pTetr[i].ivtx[ivtx1]; 
			for(j=m_vtx[i0].iStartEdge; 
					j<=m_vtx[i0].iEndEdge && m_edges[m_pVtxEdges[j]].ivtx[0]+m_edges[m_pVtxEdges[j]].ivtx[1]-i0!=i1; j++);
			m_pTetrEdges[iedge] = m_pVtxEdges[j];
		}
		m_pTetrQueue = new int[pLattice->m_nTetr];
		for(i=0,m_BBox[0]=m_BBox[1]=m_pos; i<m_nVtx; i++) {
			m_BBox[0] = min(m_BBox[0], m_vtx[i].pos+m_pos+m_offs0);
			m_BBox[1] = max(m_BBox[1], m_vtx[i].pos+m_pos+m_offs0);
		}

		m_bMeshUpdated=m_bSkinReady = 0;
		m_nConnectedVtx=m_nVtx; m_nAttachedVtx=0; m_bAwake=1;
		m_coverage = 0.0f;
		m_flags |= pef_use_geom_callbacks|sef_volumetric;
		return res;
	}

	memset(pInfo=new int[pMesh->m_nTris][3],-1,pMesh->m_nTris*sizeof(pInfo[0]));
	// count the number of edges - for each tri, mark each edge, unless buddy already did it
	for(i=m_nEdges=0;i<pMesh->m_nTris;i++) {
		for(j=bDegen=0;j<3;j++) {
			len[j] = (pMesh->m_pVertices[pMesh->m_pIndices[i*3+j]]-pMesh->m_pVertices[pMesh->m_pIndices[i*3+inc_mod3[j]]]).len2();
			bDegen |= iszero(len[j]);
			m_vtx[pMesh->m_pIndices[i*3+j]].nmesh += pMesh->m_pNormals[i];
		}
		iedge = idxmax3(len); j = iedge&-bDegen;
		do {
			if (pInfo[i][j]<0 && !(m_flags&se_skip_longest_edges && j==iedge && !bDegen)) {
				if (pMesh->m_pTopology[i].ibuddy[j]>=0)
					pInfo[ pMesh->m_pTopology[i].ibuddy[j] ][ pMesh->GetEdgeByBuddy(pMesh->m_pTopology[i].ibuddy[j],i) ] = m_nEdges;
				pInfo[i][j] = m_nEdges++;
			}
		} while(++j<3 && !bDegen);
	}
	m_edges = new se_edge[m_nEdges];
	m_pVtxEdges = new int[m_nEdges*2];
	for(i=0;i<pMesh->m_nTris;i++) for(j=0;j<3;j++) if ((iedge=pInfo[i][j])>=0) {
		i0 = m_edges[iedge].ivtx[0] = pMesh->m_pIndices[i*3+j];
		i1 = m_edges[iedge].ivtx[1] = pMesh->m_pIndices[i*3+inc_mod3[j]];
		m_edges[iedge].len=m_edges[iedge].len0 = (pMesh->m_pVertices[i0]-pMesh->m_pVertices[i1]).len()*params->scale;
		m_edges[iedge].len = 0;
	}

	// for each vertex, trace ccw fan around it and store in m_pVtxEdges
	m_BBox[0].zero(); m_BBox[1].zero();
	for(i=nVtxEdges=0; i<pMesh->m_nTris; i++) 
	for(j=0;j<3;j++) if (!m_vtx[ivtx=pMesh->m_pIndices[i*3+j]].iSorted) {
		itri=i; iedge=j;
		m_vtx[ivtx].iStartEdge = nVtxEdges;	m_vtx[ivtx].bFullFan = 1;
		int loop=0;	do { // first, trace cw fan until we find an open edge (if any)
			if ((itrinew = pMesh->m_pTopology[itri].ibuddy[iedge])<=0)
				break;
			iedge = inc_mod3[pMesh->GetEdgeByBuddy(itrinew,itri)];
		}	while((itri=itrinew)!=i && ++loop<40);
		itri0 = itri;
		do { // now trace ccw fan
			assert(itri < pMesh->m_nTris);
			if (pInfo[itri][iedge]>=0)
				if (m_edges[pInfo[itri][iedge]].len<1.9f)
					m_edges[m_pVtxEdges[nVtxEdges++] = pInfo[itri][iedge]].len++;
				else
					break; // topological error
			if ((itrinew = pMesh->m_pTopology[itri].ibuddy[dec_mod3[iedge]])<0) {
				if (pInfo[itri][dec_mod3[iedge]]>=0)
					m_pVtxEdges[nVtxEdges++] = pInfo[itri][dec_mod3[iedge]];
				m_vtx[ivtx].bFullFan = 0; break;
			}
			iedge = pMesh->GetEdgeByBuddy(itrinew,itri);
		} while ((itri=itrinew)!=itri0);
		m_vtx[ivtx].iEndEdge = nVtxEdges-1;
		m_vtx[ivtx].rnEdges = 1.0f/max(1,nVtxEdges-m_vtx[ivtx].iStartEdge);
		m_vtx[ivtx].iSorted = 1;
		m_vtx[ivtx].surface_idx[0] = pMesh->m_pIds ? pMesh->m_pIds[i]:-1;
		m_vtx[ivtx].n = m_qrot*params->q*m_vtx[ivtx].nmesh.normalize();

		m_BBox[0] = min(m_BBox[0], m_vtx[ivtx].pos);
		m_BBox[1] = max(m_BBox[1], m_vtx[ivtx].pos);
	}
	delete[] pInfo;
	m_coverage = m_flags&se_skip_longest_edges ? 0.5f: 1.0f/3;
	m_BBox[0] += m_pos+m_offs0; m_BBox[1] += m_pos+m_offs0;

	BakeCurrentPose();

	box bbox;
	bbox.Basis.SetIdentity();
	bbox.bOriented = 0;
	bbox.center = (m_BBox[0]+m_BBox[1])*0.5f-m_pos-m_parts[0].pos;
	bbox.size = (m_BBox[1]-m_BBox[0])*(0.5f/params->scale);
	if (pMesh->m_pTree)
		delete pMesh->m_pTree;
	CSingleBoxTree *pTree = new CSingleBoxTree;
	pTree->SetBox(&bbox);
	pTree->Build(pMesh);
	pTree->m_nPrims = pMesh->m_nTris;
	pMesh->m_pTree = pTree;
	(pMesh->m_flags &= ~(mesh_OBB|mesh_AABB|mesh_AABB_rotated|mesh_force_AABB)) |= mesh_SingleBB;
	m_flags |= pef_use_geom_callbacks;
	m_bMeshUpdated = 0;
	m_nConnectedVtx=m_nAttachedVtx = 0;
	m_maxAllowedDist = max(max(bbox.size.x,bbox.size.y),bbox.size.z)*params->scale*10.0f;

	return res;
}

void CSoftEntity::BakeCurrentPose()
{
	int i,j;
	Vec3 edge0,edge1,l,n;
	int sg;

	for(i=0; i<m_nEdges; i++)
		m_edges[i].len0 = (m_vtx[m_edges[i].ivtx[0]].pos-m_vtx[m_edges[i].ivtx[1]].pos).len();

	for(i=0; i<m_nVtx; i++) {
		for(j=m_vtx[i].iStartEdge,n.zero(); j<=m_vtx[i].iEndEdge; j++)	{
			int j1 = j+1 + (m_vtx[i].iStartEdge-j-1 & ~(j-m_vtx[i].iEndEdge>>31));
			edge0 = m_vtx[m_edges[m_pVtxEdges[j ]].ivtx[1]+m_edges[m_pVtxEdges[j ]].ivtx[0]-i].pos-m_vtx[i].pos;
			edge1 = m_vtx[m_edges[m_pVtxEdges[j1]].ivtx[1]+m_edges[m_pVtxEdges[j1]].ivtx[0]-i].pos-m_vtx[i].pos;
			n += (edge0^edge1)*isneg(j-m_vtx[i].iEndEdge-m_vtx[i].bFullFan);
			l = edge0.normalize()^edge1.normalize(); sg = sgnnz(edge1*edge0);
			m_edges[m_pVtxEdges[j]].angle0[iszero(m_edges[m_pVtxEdges[j]].ivtx[1]-i)] = l.len()*sg+1-sg;
		}
		for(j=m_vtx[i].iStartEdge,n.normalize(),m_vtx[i].angle0=0; j<=m_vtx[i].iEndEdge; j++)	{
			edge0 = (m_vtx[m_edges[m_pVtxEdges[j ]].ivtx[1]+m_edges[m_pVtxEdges[j ]].ivtx[0]-i].pos-m_vtx[i].pos).normalized();
			m_vtx[i].angle0 += 1-n*edge0;
		}
	}
}

void CSoftEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this,m_pWorld,0,bThreadSafe,0,id);
	if (req.IsQueued())
		return;

	int i;
	for(i=0;i<m_nParts && m_parts[i].id!=id;i++);
	if (i==m_nParts) return;

	if (m_vtx) { delete[] m_vtx; m_vtx=0; }
	if (m_edges) { delete[] m_edges; m_edges=0; }
	if (m_pVtxEdges) { delete[] m_pVtxEdges; m_pVtxEdges=0; }
	m_nVtx=m_nEdges = 0; 

	CPhysicalEntity::RemoveGeometry(id,1);
}

int CSoftEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued())
		return 1;

	int res = CPhysicalEntity::SetParams(_params,1);
	if (res) {
		if (_params->type==pe_params_pos::type_id && (!is_unused(((pe_params_pos*)_params)->q) || !is_unused(((pe_params_pos*)_params)->pos))) {
			WriteLock lock(m_lockSoftBody);
			if (m_nVtx>0) {
				CTriMesh *pMesh = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
				int i,j;
				for(i=0;i<m_nVtx;i++)	{
					m_vtx[i].pos = m_qrot*(m_parts[0].q*pMesh->m_pVertices[i]*m_parts[0].scale+m_parts[0].pos);
					m_vtx[i].vel = m_qrot*m_vtx[i].vel;
				}
				/*if (!(m_flags & sef_skeleton))*/ for(j=0;j<m_nAttachedVtx;j++) {
					Vec3 offs; quaternionf q; float scale; i=m_vtx[j].idx;
					m_vtx[i].pContactEnt->GetLocTransform(m_vtx[i].iContactPart, offs,q,scale, this);
					m_vtx[i].ptAttach = (m_vtx[i].pos+m_pos-offs)*q;
				}
				m_offs0 = m_vtx[m_vtx[0].idx].pos;
				for(i=0;i<m_nVtx;i++) m_vtx[i].pos -= m_offs0;
			}
		}
		return res;
	}

	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		if (!is_unused(params->gravity)) m_gravity = params->gravity;
		if (!is_unused(params->maxTimeStep)) m_maxAllowedStep = params->maxTimeStep;
		if (!is_unused(params->minEnergy)) m_Emin = params->minEnergy;
		if (!is_unused(params->damping)) m_damping = params->damping;
		if (!is_unused(params->density) && params->density>=0 && m_nParts>0) {
			m_vtxvol = m_parts[0].mass/(m_nVtx*params->density);
			m_density = params->density;
		}
		if (!is_unused(params->mass) && params->mass>0 && m_nParts>0) {
			int i; float dmass=m_parts[0].mass/params->mass, rdmass=params->mass/m_parts[0].mass;
			for(i=0;i<m_nVtx;i++) 
				m_vtx[i].massinv*=dmass, m_vtx[i].mass*=rdmass;
			for(i=0;i<m_nEdges;i++)
				m_edges[i].kmass = 1.0f/max(1E-10f,m_vtx[m_edges[i].ivtx[0]].massinv+m_vtx[m_edges[i].ivtx[1]].massinv);
			m_parts[0].mass = params->mass;
		}
		if (!is_unused(params->iSimClass))	{
			m_iSimClass = params->iSimClass;
			m_pWorld->RepositionEntity(this,2);
		}
		return 1;
	}

	if (_params->type==pe_params_softbody::type_id) {
		pe_params_softbody *params = (pe_params_softbody*)_params;
		WriteLock lock(m_lockSoftBody);
		if (!is_unused(params->thickness)) m_thickness = params->thickness;
		if (!is_unused(params->friction)) m_friction = params->friction;
		if (!is_unused(params->ks) && (m_ks = params->ks)<0)
			m_ks *= -m_parts[0].mass/(m_nVtx*sqr(m_maxAllowedStep));
		if (!is_unused(params->airResistance)) m_airResistance = params->airResistance;
		if (!is_unused(params->waterResistance)) m_waterResistance = params->waterResistance;
		if (!is_unused(params->wind)) if ((m_wind0=m_wind1=m_wind = params->wind).len2()>0)
			Awake();
		if (!is_unused(params->windVariance)) m_windVariance = params->windVariance;
		if (!is_unused(params->accuracy)) m_accuracy = params->accuracy;
		if (!is_unused(params->nMaxIters)) m_nMaxIters = params->nMaxIters;
		if (!is_unused(params->maxSafeStep)) m_maxSafeStep = params->maxSafeStep;
		if (!is_unused(params->impulseScale)) m_impulseScale = params->impulseScale;
		if (!is_unused(params->explosionScale)) m_explosionScale = params->explosionScale;
		if (!is_unused(params->collisionImpulseScale)) m_collImpulseScale = params->collisionImpulseScale;
		if (!is_unused(params->maxCollisionImpulse)) m_maxCollImpulse = params->maxCollisionImpulse;
		if (!is_unused(params->collTypes)) m_collTypes = params->collTypes;
		if (!is_unused(params->massDecay) && m_massDecay!=params->massDecay) {
			m_massDecay = params->massDecay;
			int i,imaxLvl=0;
			for(i=0;i<m_nVtx;i++) imaxLvl = max(imaxLvl,m_vtx[i].iSorted);
			float rvtxmass = m_nVtx/m_parts[0].mass;
			float kmass = (m_massDecay+1)/(float)(imaxLvl+1);
			for(i=0;i<m_nVtx;i++) if (m_vtx[i].massinv>0.0f)
				m_vtx[i].mass = 1.0f/(m_vtx[i].massinv = rvtxmass*(1.0f+m_vtx[i].iSorted*kmass));
			for(i=0;i<m_nEdges;i++)
				m_edges[i].kmass = 1.0f/max(1E-10f,m_vtx[m_edges[i].ivtx[0]].massinv+m_vtx[m_edges[i].ivtx[1]].massinv);
		}
		if (!is_unused(params->shapeStiffnessNorm)) m_kShapeStiffnessNorm = params->shapeStiffnessNorm;
		if (!is_unused(params->shapeStiffnessTang)) m_kShapeStiffnessTang = params->shapeStiffnessTang;
		if (!is_unused(params->stiffnessAnim)) m_stiffnessAnim = params->stiffnessAnim;
		if (!is_unused(params->stiffnessDecayAnim)) m_stiffnessDecayAnim = params->stiffnessDecayAnim;
		if (!is_unused(params->dampingAnim)) m_dampingAnim = params->dampingAnim;
		if (!is_unused(params->maxDistAnim)) m_maxDistAnim = params->maxDistAnim;
		if (!is_unused(params->hostSpaceSim)) m_kRigid = params->hostSpaceSim;
		return 1;
	}

	return 0;
}


int CSoftEntity::GetParams(pe_params *_params) const
{
	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		params->gravity=params->gravityFreefall = m_gravity;
		params->maxTimeStep = m_maxAllowedStep;
		params->minEnergy = m_Emin;
		params->damping=params->dampingFreefall = m_damping;
		params->mass = m_parts[0].mass;
		params->density = m_density;
		params->iSimClass = m_iSimClass;
		return 1;
	}

	if (_params->type==pe_params_softbody::type_id) {
		pe_params_softbody *params = (pe_params_softbody*)_params;
		ReadLock lock(m_lockSoftBody);
		params->thickness = m_thickness;
		params->friction = m_friction;
		params->ks = m_ks;
		params->airResistance = m_airResistance;
		params->waterResistance = m_waterResistance;
		params->wind = m_wind;
		params->windVariance = m_windVariance;
		params->accuracy = m_accuracy;
		params->nMaxIters = m_nMaxIters;
		params->maxSafeStep = m_maxSafeStep;
		params->impulseScale = m_impulseScale;
		params->explosionScale = m_explosionScale;
		params->collisionImpulseScale = m_collImpulseScale;
		params->maxCollisionImpulse = m_maxCollImpulse;
		params->collTypes = m_collTypes;
		params->massDecay = m_massDecay;
		params->shapeStiffnessNorm = m_kShapeStiffnessNorm;
		params->shapeStiffnessTang = m_kShapeStiffnessTang;
		params->stiffnessAnim = m_stiffnessAnim;
		params->stiffnessDecayAnim = m_stiffnessDecayAnim;
		params->dampingAnim = m_dampingAnim;
		params->maxDistAnim = m_maxDistAnim;
		params->hostSpaceSim = m_kRigid;
		return 1;
	}

	return CPhysicalEntity::GetParams(_params);
}

int CSoftEntity::GetStatus(pe_status *_status) const
{
	int res;
	if (res = CPhysicalEntity::GetStatus(_status)) {
		if (_status->type==pe_status_caps::type_id) {
			pe_status_caps *status = (pe_status_caps*)_status;
			status->bCanAlterOrientation = 1;
		}
		return res;
	}

	if (_status->type==pe_status_softvtx::type_id) {
		if (m_nVtx<=0)
			return 0;
		pe_status_softvtx *status = (pe_status_softvtx*)_status;
		if (status->flags & eSSV_LockPos) {
			++m_lockUpdate; for(;*((char*)&m_lockUpdate+2););
		} 
		if (status->flags & eSSV_UnlockPos)
			--m_lockUpdate;
		status->nVtx = m_nVtx;
		status->pVtx = ((CTriMesh*)m_parts[0].pPhysGeomProxy->pGeom)->m_pVertices;
		status->pNormals = strided_pointer<Vec3>(&m_vtx[0].nmesh, sizeof(m_vtx[0]));
		status->pVtxMap = ((CTriMesh*)m_parts[0].pPhysGeom->pGeom)->m_pVtxMap;
		status->pMesh = m_parts[0].pPhysGeomProxy->pGeom;
		status->posHost = m_lastposHost;
		status->qHost = m_lastqHost;
		status->pos = m_bMeshUpdated ? m_lastPos : m_pos;
		status->q = m_qrot;
		return 1;
	}

	if (_status->type==pe_status_collisions::type_id) {
		pe_status_collisions *status = (pe_status_collisions*)_status;
		int i,j,n;
		if (status->len<=0)
			return 0;

		for(i=n=0;i<m_nVtx;i++) if (!m_vtx[i].bAttached && m_vtx[i].pContactEnt) {
			if (n==status->len) {
				for(n=1,j=0; n<status->len; n++) 
					if ((status->pHistory[n].v[0]-status->pHistory[n].v[1]).len2() < (status->pHistory[j].v[0]-status->pHistory[j].v[1]).len2())
						j = n;
				if ((status->pHistory[j].v[0]-status->pHistory[j].v[1]).len2() > (m_vtx[i].vel-m_vtx[i].vcontact).len2())
					continue;
			} else j=n++;
			status->pHistory[j].pt = m_vtx[i].pos;
			status->pHistory[j].n = m_vtx[i].ncontact;
			status->pHistory[j].v[0] = m_vtx[i].vel;
			status->pHistory[j].v[1] = m_vtx[i].vcontact;
			status->pHistory[j].mass[0] = m_parts[0].mass;
			status->pHistory[j].mass[1] = m_vtx[i].pContactEnt->GetMassInv();
			if (status->pHistory[j].mass[1]>0)
				status->pHistory[j].mass[1] = 1.0f/status->pHistory[j].mass[1];
			status->pHistory[j].age = 0;
			status->pHistory[j].idCollider = m_pWorld->GetPhysicalEntityId(m_vtx[i].pContactEnt);
			status->pHistory[j].partid[0] = 0;
			status->pHistory[j].partid[1] = m_vtx[i].iContactPart;
			status->pHistory[j].idmat[0] = m_vtx[i].surface_idx[0];
			status->pHistory[j].idmat[1] = m_vtx[i].surface_idx[1];
		}

		return status->len = n;
	}

	if (_status->type==pe_status_dynamics::type_id) {
		pe_status_dynamics *status = (pe_status_dynamics*)_status;
		status->mass = m_nParts ? m_parts[0].mass : 0.0f;
		if (m_nVtx) {
			status->v = m_vtx[m_vtx[0].idx].vel;
			status->centerOfMass.zero();
			for(int i=0; i<m_nVtx; status->centerOfMass+=m_vtx[i++].pos);
			status->centerOfMass/=m_nVtx;
		}
	}

	return 0;
}


void CSoftEntity::AttachPoints(pe_action_attach_points *action, CPhysicalEntity *pent,int ipart, float rvtxmass,float vtxmass, int bAttached, const Vec3 &_offs,const quaternionf &_q)
{
	pe_params_rope pr;
	int bRope = pent->GetParams(&pr);
	Vec3 offs=_offs; Quat q=_q;
	for(int i=0;i<action->nPoints;i++) if (action->piVtx[i]<m_nVtx) {
		if (m_vtx[action->piVtx[i]].pContactEnt) m_vtx[action->piVtx[i]].pContactEnt->Release();
		if (m_vtx[action->piVtx[i]].pContactEnt = pent)
			pent->AddRef();
		m_vtx[action->piVtx[i]].massinv = rvtxmass;
		m_vtx[action->piVtx[i]].mass = vtxmass;
		if (bAttached & -bRope) {
			quotientf mindist(1,0),dist;
			Vec3 vtx = m_vtx[action->piVtx[i]].pos+m_pos+m_offs0;
			for(int j=0; j<pr.nSegments; j++) {
				Vec3 seg = pr.pPoints[j+1]-pr.pPoints[j]; dist.y = seg.len2(); 
				dist.x = min(min((pr.pPoints[j]-vtx).len2()*dist.y,(pr.pPoints[j+1]-vtx).len2()*dist.y), (vtx-pr.pPoints[j]^seg).len2());	
				int ismin = -isneg(dist.x*mindist.y-mindist.x*dist.y);
				ipart += j-ipart & ismin; mindist.x -= (dist.x-mindist.x)*ismin; mindist.y -= (dist.y-mindist.y)*ismin;
			}
			pent->GetLocTransform(ipart,offs,q,dist.x,this);
		}
		m_vtx[action->piVtx[i]].iContactPart = ipart;
		if (m_vtx[action->piVtx[i]].bAttached = bAttached) {
			if (!is_unused(action->points))
				m_vtx[action->piVtx[i]].ptAttach = action->points[i];
			else
				m_vtx[action->piVtx[i]].ptAttach = m_vtx[action->piVtx[i]].pos+m_pos+m_offs0;
			if (!action->bLocal)
				m_vtx[action->piVtx[i]].ptAttach = (m_vtx[action->piVtx[i]].ptAttach-offs)*q;
		}
	}
}


int CSoftEntity::Action(pe_action *_action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())
		return 1;

	if (_action->type==pe_action_impulse::type_id) {
		pe_action_impulse *action = (pe_action_impulse*)_action;
		ENTITY_VALIDATE("CSoftEntity:Action(action_impulse)",action);

		if (m_nVtx>0 && !is_unused(action->impulse)) {
			CTriMesh *pMesh = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
			Vec3 pt=!is_unused(action->point) ? action->point:(m_BBox[0]+m_BBox[1])*0.5f, impulse=action->impulse*m_impulseScale;
			int i,j;
			if (!is_unused(action->ipart)) i = action->ipart;
			else i = action->partid;
			pt -= m_pos+m_offs0;
			if (!(m_flags & sef_skeleton)) {
				m_bAwake = 1;
				m_pWorld->RepositionEntity(this,2);
			}

			if ((unsigned int)i < (unsigned int)pMesh->m_nTris) {
				float rarea,k; int idx[3];
				for(j=0;j<3;j++) idx[j] = pMesh->m_pIndices[i*3+j];
				rarea = (m_vtx[idx[1]].pos-m_vtx[idx[0]].pos ^ m_vtx[idx[2]].pos-m_vtx[idx[0]].pos).len();
				if (rarea>1E-4f) {
					rarea = 1.0f/rarea;
					for(j=0;j<3;j++) {
						k = (m_vtx[idx[inc_mod3[j]]].pos-pt ^ m_vtx[idx[j]].pos-pt).len()*rarea;
						m_vtx[idx[dec_mod3[j]]].vel += impulse*(m_vtx[idx[dec_mod3[j]]].massinv*k);
					}
				} else
					m_vtx[idx[0]].vel += impulse*m_vtx[idx[0]].massinv;
			} else {
				quotientf dist,mindist(1.0f,0.0f);
				int itri; Vec3 w; float wdenom;
				for(i=0; i<pMesh->m_nTris; i++) {
					Vec3 vtx[3],n; for(j=0;j<3;j++) vtx[j]=m_vtx[pMesh->m_pIndices[i*3+j]].pos;
					n = vtx[1]-vtx[0]^vtx[2]-vtx[0];
					int bInside = isneg(max(max((pt-vtx[0]^vtx[1]-vtx[0])*n,(pt-vtx[1]^vtx[2]-vtx[1])*n),(pt-vtx[2]^vtx[0]-vtx[2])*n));
					if (dist.set(sqr((pt-vtx[0])*n), n.len2()) < mindist*bInside) {
						itri=i;mindist=dist; wdenom=n.len2(); for(j=0;j<3;j++) w[j] = n*(vtx[inc_mod3[j]]-pt^vtx[dec_mod3[j]]-pt); 
					}
					for(j=0;j<3;j++) {
						n = vtx[inc_mod3[j]]-vtx[j]; float t = (pt-vtx[j])*n;
						if (dist.set((pt-vtx[j]^n).len2(),n.len2()) < mindist*inrange(t,0.0f,n.len2())) {
							itri=i;mindist=dist; w.zero(); w[j]=(wdenom=n.len2())-t; w[inc_mod3[j]]=t; 
						}
					}
					for(j=0;j<3;j++) if (dist.set((pt-vtx[j]).len2(),1.0f)<mindist) {
						itri=i;mindist=dist; w.zero(); w[j]=wdenom=1.0f;
					}
				}
				wdenom = 1.0f/wdenom;
				for(j=0;j<3;j++)
					i=pMesh->m_pIndices[itri*3+j], m_vtx[i].vel+=impulse*(m_vtx[i].massinv*wdenom*w[j]);
			}
		}
		return 1;
	}

	if (_action->type==pe_action_attach_points::type_id) {
		pe_action_attach_points *action = (pe_action_attach_points*)_action;
		if (!m_nVtx)
			return 0;
		WriteLock lock(m_lockSoftBody);
		CPhysicalEntity* pent = action->pEntity==WORLD_ENTITY ? &g_StaticPhysicalEntity : 
			action->pEntity ? ((CPhysicalPlaceholder*)action->pEntity)->GetEntity() : 0;
		int i,ipart=0, bAttached=iszero((intptr_t)pent)^1;
		if (bAttached && is_unused(action->points))
			bAttached = 2;
		float rvtxmass=0,vtxmass=0;
		Vec3 offs; quaternionf q; float scale;
		if (!pent || action->nPoints==0)
			vtxmass = 1.0f/(rvtxmass = m_nVtx/m_parts[0].mass);

		if (is_unused(action->nPoints) && pent) {
			for(i=0;i<m_nVtx;i++) if (m_vtx[i].bAttached && m_vtx[i].pContactEnt) {
				m_vtx[i].pContactEnt->Release();
				(m_vtx[i].pContactEnt = pent)->AddRef();
			}
		}	else if (action->nPoints<0) {
			CPhysicalEntity **pents = &pent;
			int iCaller=get_iCaller(), nents=pent && action->pEntity!=WORLD_ENTITY ? 1 :
				m_pWorld->GetEntitiesAround(m_BBox[0],m_BBox[1],pents,ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid,0,0,iCaller), npt=0;
			action->piVtx = new int[m_nVtx];
			for(int ient=0;ient<nents;ient++) for(int j=0;j<pents[ient]->GetUsedPartsCount(iCaller);j++) {
				ipart = pents[ient]->GetUsedPart(iCaller,j);
				pents[ient]->GetLocTransform(ipart, offs,q,scale, this);
				float rscale = 1.0f/scale;
				for(i=action->nPoints=0;i<m_nVtx;i++) if (!m_vtx[i].bAttached) {
					action->piVtx[action->nPoints] = i;
					action->nPoints += pents[ient]->m_parts[ipart].pPhysGeomProxy->pGeom->PointInsideStatus((m_vtx[i].pos+m_pos+m_offs0-offs)*q*rscale);
				}
				
				if (action->nPoints>0)
					AttachPoints(action,pents[ient],ipart, rvtxmass,vtxmass, bAttached, offs,q);
				npt += action->nPoints;
			}
			delete[] action->piVtx; action->piVtx=0; action->nPoints = -1;
			if (!npt)
				return 1;
		} else {
			if (!pent && (is_unused(action->piVtx) || !action->piVtx)) {
				for(i=0;i<m_nVtx;i++) if (m_vtx[i].bAttached && m_vtx[i].pContactEnt) {
					m_vtx[i].pContactEnt->Release(); m_vtx[i].pContactEnt=0; m_vtx[i].bAttached=0;
				}
				m_nAttachedVtx = 0;
				return 1;
			}

			if (pent && !is_unused(action->partid)) {
				for(ipart=0; ipart<pent->m_nParts && pent->m_parts[ipart].id!=action->partid; ipart++);
				if (ipart>=pent->m_nParts)
					return 0;
			}
			if (pent && pent->m_iSimClass==4 && pent->GetType()!=PE_ROPE)
				(m_collTypes &= ~ent_living) |= ent_independent;
			if (bAttached)
				pent->GetLocTransform(ipart, offs,q,scale, this);

			if (pent && action->nPoints==0)	{
				for(i=0;i<m_nVtx;i++) if (m_vtx[i].bAttached && m_vtx[i].pContactEnt==pent) {
					m_vtx[i].pContactEnt->Release(); m_vtx[i].pContactEnt=0; m_vtx[i].bAttached=0;
					m_vtx[i].mass=vtxmass; m_vtx[i].massinv=rvtxmass;
				}
			} else
				AttachPoints(action,pent,ipart, rvtxmass,vtxmass, bAttached, offs,q);
		}

		int ihead,itail,j,i1,imaxLvl=0;
		for(ihead=0; ihead<m_nAttachedVtx && m_vtx[m_vtx[ihead].idx].bAttached; m_vtx[ihead++].iSorted=0);
		for(i=ihead;i<m_nVtx;i++)
			if (m_vtx[m_vtx[i].idx].bAttached)
				m_vtx[ihead++].idx=m_vtx[i].idx,m_vtx[m_vtx[i].idx].iSorted=0;
			else
				m_vtx[m_vtx[i].idx].iSorted = -1;
		for(i=0,offs.zero();i<m_nVtx;i++)
			offs += m_vtx[i].pos;
		for(i=m_nAttachedVtx,offs/=m_nVtx;i<ihead-1;i++) for(j=ihead-1;j>i;j--) 
			if ((m_vtx[m_vtx[j].idx].pos-offs).len2()<(m_vtx[m_vtx[j-1].idx].pos-offs).len2()) {
				i1=m_vtx[j].idx; m_vtx[j].idx=m_vtx[j-1].idx; m_vtx[j-1].idx=i1;
			}
		m_nAttachedVtx = ihead;

		for(itail=0;itail!=ihead;)	for(j=m_vtx[i=m_vtx[itail++].idx].iStartEdge; j<=m_vtx[i].iEndEdge; j++)
			if (m_vtx[i1 = m_edges[m_pVtxEdges[j]].ivtx[0]+m_edges[m_pVtxEdges[j]].ivtx[1]-i].iSorted<0)
				m_vtx[ihead++].idx = i1, imaxLvl = max(imaxLvl, m_vtx[i1].iSorted = m_vtx[i].iSorted+1);
		for(i=0,i1=ihead; i<m_nVtx; i++) {
			if (m_vtx[i].iSorted<0) {
				m_vtx[i1++].idx=i; m_vtx[i].iSorted=0;
			}
			m_vtx[m_vtx[i].idx].idx0 = i;
		}
		m_nConnectedVtx = ihead;
		if (!ihead && m_flags & sef_volumetric)
			m_nConnectedVtx = m_nVtx;
		m_maxLevelDenom = 1.0f/max(1,imaxLvl);

		Vec3 pos0 = m_vtx[m_vtx[0].idx].pos;
		m_offs0 += pos0;
		for(i=0;i<m_nVtx;i++) m_vtx[i].pos -= pos0;

		if (m_massDecay>0) {
			rvtxmass = m_nVtx/m_parts[0].mass;
			float kmass = (m_massDecay+1)/(float)(imaxLvl+1);
			for(i=0;i<m_nVtx;i++) if (m_vtx[i].massinv>0.0f)
				m_vtx[i].mass = 1.0f/(m_vtx[i].massinv = rvtxmass*(1.0f+m_vtx[i].iSorted*kmass));
		}

		for(i=0;i<m_nEdges;i++)
			m_edges[i].kmass = 1.0f/max(1E-10f,m_vtx[m_edges[i].ivtx[0]].massinv+m_vtx[m_edges[i].ivtx[1]].massinv);
		m_lastposHost.zero();

		return 1;
	}

	if (_action->type==pe_action_target_vtx::type_id) {
		pe_action_target_vtx *action = (pe_action_target_vtx*)_action;
		if (is_unused(action->points) || !action->points)	{
			float scale=1.0f;
			if (is_unused(action->posHost) && m_nVtx && m_vtx[m_vtx[0].idx].pContactEnt)
				m_vtx[m_vtx[0].idx].pContactEnt->GetLocTransform(m_vtx[m_vtx[0].idx].iContactPart,action->posHost,action->qHost,scale,this);
			for(int i=0; i<m_nVtx; i++) if (!m_vtx[i].bAttached) 
				m_vtx[i].ptAttach = (m_vtx[i].pos-action->posHost)*action->qHost;
		} else {
			for(int i=0; i<action->nPoints; i++) //if (!m_vtx[i].bAttached) 
				m_vtx[i].ptAttach = (action->points[i]-action->posHost)*action->qHost;
			if (m_maxSafeStep>0) for(int i=0;i<m_nEdges;i++) {
				float len2=sqr(m_edges[i].len0), lenNew2=(m_vtx[m_edges[i].ivtx[0]].ptAttach-m_vtx[m_edges[i].ivtx[1]].ptAttach).len2(), e2=sqr(m_edges[i].len0*m_maxSafeStep*0.5f);
				if (min(sqr(len2+lenNew2-e2)-len2*lenNew2*4, len2+lenNew2-e2)>0)
					m_edges[i].len0 = sqrt_tpl(lenNew2);
			}
		}
		return 1;
	}

	if (_action->type==pe_action_reset::type_id) {
		if (m_nVtx>0) {
			CTriMesh *pMesh = (CTriMesh*)m_parts[0].pPhysGeomProxy->pGeom;
			int i;
			pe_action_reset *action = (pe_action_reset*)_action;
			for(i=0;i<m_nVtx;i++) 
				m_vtx[i].vel.zero();
			if (action->bClearContacts==1 && !(m_flags & sef_skeleton)) {
				for(i=0;i<m_nVtx;i++) {
					if (m_vtx[i].pContactEnt)	m_vtx[i].pContactEnt->Release();
					m_vtx[i].pContactEnt=0;	m_vtx[i].bAttached=0;
					m_vtx[i].vel.zero();
					pMesh->m_pVertices[i] = m_vtx[i].posorg;
				}
				m_nAttachedVtx = 0;
				for(i=0;i<pMesh->m_nTris;i++)	pMesh->RecalcTriNormal(i);
			} else if (action->bClearContacts==2) {
				for(i=0;i<m_nVtx;i++) if (!m_vtx[i].bAttached && m_vtx[i].pContactEnt) {
					m_vtx[i].pContactEnt->Release();
					m_vtx[i].pContactEnt=0;	
				}
				if (!(m_flags & sef_skeleton)) for(i=0;i<m_nVtx;i++)
					pMesh->m_pVertices[i] = m_vtx[i].posorg;
				//BakeCurrentPose();
			}	else if (action->bClearContacts==3) {
				CTriMesh *pMesh2 = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
				for(i=0;i<m_nVtx;i++)
					m_vtx[i].pos = m_qrot*(m_parts[0].q*(pMesh2->m_pVertices[i]=m_vtx[i].posorg)*m_parts[0].scale+m_parts[0].pos);
				for(i=m_nVtx-1;i>=0;i--) m_vtx[i].pos -= m_vtx[0].pos;
				for(i=0;i<pMesh2->m_nTris;i++) pMesh2->RecalcTriNormal(i);
				BakeCurrentPose();
				m_bMeshUpdated = 0;
			}
			m_windTimer = 0;
		}
		return 1;
	}

	if (_action->type==pe_action_slice::type_id) {
		pe_action_slice *action = (pe_action_slice*)_action;
		int ipart=0;
		if (!is_unused(action->partid))
			for(ipart=0;ipart<m_nParts && m_parts[ipart].id!=action->partid;ipart++);
		else if (!is_unused(action->ipart))
			ipart = action->ipart;
		if (ipart>=m_nParts || action->npt!=3)
			return 0;
		int id = m_parts[ipart].id;
		phys_geometry *pGeom = m_parts[ipart].pPhysGeomProxy;
		pe_geomparams gp; 
		gp.flags=m_parts[ipart].flags; gp.flagsCollider=m_parts[ipart].flagsCollider;
		gp.pos=m_parts[ipart].pos; gp.q=m_parts[ipart].q; gp.scale=m_parts[ipart].scale; gp.mass=m_parts[ipart].mass;
		CTriMesh *pMesh = (CTriMesh*)m_parts[ipart].pPhysGeomProxy->pGeom;
		for(int i=0;i<pMesh->m_nTris;i++) pMesh->RecalcTriNormal(i);
		
		primitives::triangle triCut; 
		for(int i=0;i<3;i++) triCut.pt[i] = (((action->pt[i]-m_pos)*m_qrot-gp.pos)*gp.q)/gp.scale;
		triCut.n = (triCut.pt[1]-triCut.pt[0] ^ triCut.pt[2]-triCut.pt[0]).normalized();

		if (pMesh->Slice(&triCut,0.01f,0.05f)) {
			pe_action_attach_points asp; asp.nPoints = m_nAttachedVtx;
			asp.piVtx = new int[m_nAttachedVtx];
			for(int i=0;i<m_nAttachedVtx;i++) asp.piVtx[i] = m_vtx[i].idx;
			if ((asp.pEntity=m_vtx[m_vtx[0].idx].pContactEnt) && ((CPhysicalEntity*)asp.pEntity)->m_nParts>0)
				asp.partid = ((CPhysicalEntity*)asp.pEntity)->m_parts[m_vtx[m_vtx[0].idx].iContactPart].id;
			int nVtx = m_nVtx;
			se_vertex *vtx=m_vtx; m_vtx=0;
			se_edge *edges=m_edges; m_edges=0;
			int *pVtxEdges=m_pVtxEdges; m_pVtxEdges=0;
			++pGeom->nRefCount;	RemoveGeometry(m_parts[ipart].id);
			float maxAllowedDist = m_maxAllowedDist;
			AddGeometry(pGeom,&gp,id); --pGeom->nRefCount;
			m_maxAllowedDist = maxAllowedDist;
			Action(&asp);
			int nSimVtx = pMesh->m_nVertices;
			for(int i=nSimVtx-1; i>=m_nConnectedVtx; i--)
				if (m_vtx[m_vtx[i].idx].iStartEdge > m_vtx[m_vtx[i].idx].iEndEdge)
					m_vtx[i].idx = m_vtx[--nSimVtx].idx;
			m_nConnectedVtx = nSimVtx;
			for(int i=0;i<nVtx;i++) {
				m_vtx[i].vel = vtx[i].vel;
				if (!vtx[i].bAttached && vtx[i].iStartEdge<=m_vtx[i].iEndEdge) {
					if (m_vtx[i].pContactEnt = vtx[i].pContactEnt)
						m_vtx[i].pContactEnt->AddRef();
					m_vtx[i].iContactPart = vtx[i].iContactPart;
					m_vtx[i].ncontact = vtx[i].ncontact;
				}
				for(int j0=vtx[i].iStartEdge;j0<=vtx[i].iEndEdge;j0++) if (edges[pVtxEdges[j0]].ivtx[0]==i)
					for(int j1=m_vtx[i].iStartEdge;j1<=m_vtx[i].iEndEdge;j1++) if (edges[pVtxEdges[j0]].ivtx[1]==m_edges[m_pVtxEdges[j1]].ivtx[1])
						m_edges[m_pVtxEdges[j1]].len0 = edges[pVtxEdges[j0]].len0;
			}
			delete[] vtx; delete[] edges; delete[] pVtxEdges;

			if (pMesh->m_pMeshUpdate) {
				EventPhysUpdateMesh epum;
				epum.pEntity=this; epum.pForeignData=m_pForeignData; epum.iForeignData=m_iForeignData;
				epum.bInvalid=0; epum.partid=id;
				epum.iReason = EventPhysUpdateMesh::ReasonFracture;
				epum.pMesh = pMesh;
				for(epum.pLastUpdate=pMesh->m_pMeshUpdate; epum.pLastUpdate->next; epum.pLastUpdate=epum.pLastUpdate->next);
				m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges+1,&epum);
			}
			return 1;
		}
		return 0;
	}

	return CPhysicalEntity::Action(_action,1);
}


void CSoftEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval+m_timeStepSurplus;
	m_timeStepSurplus = 0;
}


float CSoftEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull-0.001f)
		return time_interval;
	return min(min(m_timeStepFull-m_timeStepPerformed,m_maxAllowedStep),time_interval);
}


void CSoftEntity::StepInner(float time_interval, int bCollMode, check_part *checkParts,int nCheckParts, 
														const plane &waterPlane,const Vec3 &waterFlow,float waterDensity,
														const Vec3 &lastposHost, const quaternionf &lastqHost,
                            se_vertex *pvtx)
{
	#define m_vtx pvtx
	int i,j,i0,i1,j1,iter,imask;
	float l0,rsep,area,kr,windage,rmax,vreq,rthickness=1.0f/max(1e-8f,m_thickness);
	Vec3 l,v,w,F,edge0,edge1;
	geom_world_data gwd;
	CPhysicalEntity *pent;
	CRayGeom aray;
	intersection_params ip;
	geom_contact *pcontacts;
	INT_PTR mask = ~-((INT_PTR)bCollMode>>2);
	ip.bStopAtFirstTri = true;

	if (m_pCore && m_pCore->m_nColliders) {
		m_pCore->m_maxAllowedStep = m_maxAllowedStep;
		pe_action_impulse ai;
		ai.impulse.zero(); ai.angImpulse.zero();
		ai.iApplyTime = 0;
		for(j=0; j<m_nCoreVtx; j++) {
			i = m_corevtx[j];
			Vec3 posCore = m_pCore->m_qrot*(m_vtx[i].posorg-m_pos0core)+m_pCore->m_pos;
			Vec3 vdst = (posCore-m_vtx[i].pos-m_pos-m_offs0)*5.0f, dir = vdst.normalized();
			Vec3 dv = dir*max(0.0f, dir*(vdst-m_vtx[i].vel));
			m_vtx[i].vel += dv; dv *= m_vtx[i].mass;
			ai.impulse -= dv;
			ai.angImpulse -= posCore-m_pCore->m_body.pos ^ dv;
		}
		m_pCore->Action(&ai);
	}

	if (!(bCollMode&2) && m_flags & sef_volumetric) {
		CTetrLattice *pLat = m_parts[0].pLattice;
		for(iter=0,i1=1; iter<m_nMaxIters && i1; iter++) {
			for(i=0;i<pLat->m_nTetr;i++) pLat->m_pTetr[i].flags &= ~64;
			for(i0=-pLat->m_nTetr,i1=0; i0<i1; i0++) {
				i = i0+pLat->m_nTetr & i0>>31 | m_pTetrQueue[max(0,i0)] & ~(i0>>31); // i0<0 ? i0+m_nTets : m_pTetrQueue[i0]; 
				Vec3 vtx[4],com,P,L; int idx[4]; 
				for(j=0;j<4;j++) vtx[j] = m_vtx[idx[j]=pLat->m_pTetr[i].ivtx[j]].pos;
				kr = (vtx[1]-vtx[0] ^ vtx[2]-vtx[0])*(vtx[3]-vtx[0]);
				for(j=0;j<4;j++) vtx[j] += m_vtx[idx[j]].vel*time_interval;
				if (min(-kr,(vtx[1]-vtx[0] ^ vtx[2]-vtx[0])*(vtx[3]-vtx[0]))>0.0f) {
					Matrix33 I,Ivtx; float mass;
					for(com.zero(),mass=0.0f,j=0;j<4;j++)
						mass += m_vtx[idx[j]].mass, com += m_vtx[idx[j]].pos*m_vtx[idx[j]].mass; 
					if (mass<=0)
						continue;
					for(com/=mass,I.SetZero(),j=0;j<4;j++)
						Ivtx.SetZero(), I += OffsetInertiaTensor(Ivtx,m_vtx[idx[j]].pos-com,m_vtx[idx[j]].mass);
					for(j=0,P.zero(),L.zero();j<4;j++) 
						P += (F=m_vtx[idx[j]].vel*m_vtx[idx[j]].mass), L += m_vtx[idx[j]].pos-com ^ F;
					for(j=0,v=P/mass,w=I.GetInverted()*L; j<4; j++) 
						m_vtx[idx[j]].vel = v + (w^m_vtx[idx[j]].pos-com);
					for(j=0;j<4;j++) if ((j1=pLat->m_pTetr[i].ibuddy[j])>=0 && !(pLat->m_pTetr[j1].flags & 64))
						m_pTetrQueue[i1++]=j1, pLat->m_pTetr[j1].flags|=64;
				}
			}
		}
	}

	if (m_stiffnessAnim>0.0f) { // apply simple animation target pull
		float maxDiff0 = m_maxDistAnim>0.0f ? m_maxDistAnim : 1e5f;
		for(j=m_nAttachedVtx; j<m_nConnectedVtx; j++) {
			i = m_vtx[j].idx;
			Vec3 diff = lastqHost*m_vtx[i].ptAttach+lastposHost-m_vtx[i].pos-m_pos-m_offs0;
			float maxDiff = maxDiff0*(1.0f-m_stiffnessDecayAnim*(1.0f-m_vtx[i].iSorted*m_maxLevelDenom));
			if (diff.len2() > sqr(maxDiff))	{
				m_vtx[i].pos += diff;	diff.NormalizeFast(); m_vtx[i].pos -= diff*maxDiff;
			}
			if (diff*(diff+m_vtx[i].vel*time_interval) < 0.0f)
				m_vtx[i].pos += m_vtx[i].vel*((m_vtx[i].vel*diff)/m_vtx[i].vel.len2() - time_interval);
		}
	}

	m_maxMove = 0.0f;
	if (!(bCollMode&2)) for(i0=m_nAttachedVtx; i0<m_nConnectedVtx; i0++) {
		i = m_vtx[i0].idx;
		Vec3 pos0 = m_vtx[i].pos;
		m_vtx[i].pos += m_vtx[i].vel*time_interval;

		float siblingCheck = m_massDecay>0.0f ? 0.0f:1.0f;
		if (m_maxSafeStep>0) 
			for(j=m_vtx[i].iStartEdge; j<=m_vtx[i].iEndEdge; j++) if (m_edges[m_pVtxEdges[j]].len>=0) {
				i1 = m_edges[m_pVtxEdges[j]].ivtx[0]+m_edges[m_pVtxEdges[j]].ivtx[1]-i;
				if (m_vtx[i1].idx0<i0 && min(fabs_tpl(m_vtx[i1].mass-m_vtx[i].mass)+siblingCheck, (m_vtx[i1].pos-m_vtx[i].pos).len2()-sqr(l.x=m_edges[m_pVtxEdges[j]].len0*(1.0f+m_maxSafeStep)))>1e-8f) {
					m_vtx[i].pos = m_vtx[i1].pos + (m_vtx[i].pos-m_vtx[i1].pos).normalized()*l.x;
					m_edges[m_pVtxEdges[j]].len = -1;
				}
			}
		if (m_vtx[i].pos.len2()>sqr(m_maxAllowedDist)) {
			m_vtx[i].pos =  m_pos.normalized()*m_maxAllowedDist;
			m_vtx[i].vel.zero();
		}

		rsep = m_thickness;
		m_vtx[i].vreq = 0;
		if (pent = m_vtx[i].pContactEnt) {
			i1 = m_vtx[i].iCheckPart; 
			m_vtx[i].vcontact = checkParts[i1].vbody+(checkParts[i1].wbody^m_vtx[i].pos+m_pos+m_offs0);
			if ((m_vtx[i].vel-m_vtx[i].vcontact)*m_vtx[i].ncontact<0.1f) {
				if (checkParts[i1].bPrimitive) {
					m_vtx[i].pContactEnt = 0;
				}	else {
					j = m_vtx[i].iContactPart;
					gwd.offset = checkParts[i1].offset;
					gwd.R = checkParts[i1].R;
					gwd.scale = checkParts[i1].scale;
					aray.m_dirn = -m_vtx[i].ncontact;
					aray.m_ray.origin = m_vtx[i].pos;
					aray.m_ray.dir = aray.m_dirn*(m_thickness*1.5f);
					gwd.iStartNode = m_vtx[i].iContactNode;
					if (checkParts[i1].pGeom->Intersect(&aray,&gwd,0,&ip,pcontacts)) {
						m_vtx[i].pos = pcontacts->pt+pcontacts->n*m_thickness;
						m_vtx[i].ncontact = pcontacts->n;
						m_vtx[i].iContactNode = pcontacts->iNode[0];
					} else
						m_vtx[i].pContactEnt=0;
				}
			}	else
					m_vtx[i].pContactEnt=0;
		}	else
			m_vtx[i].pContactEnt=0, m_vtx[i].vcontact.zero();

		for(j=0; j<nCheckParts; j++) {
			v = checkParts[j].vbody+(checkParts[j].wbody^m_vtx[i].pos+m_pos+m_offs0);
			if (checkParts[j].bPrimitive) {
				contact acontact;	acontact.iFeature[0]=m_vtx[i].iContactNode;
				if (checkParts[j].pGeom->UnprojectSphere(((m_vtx[i].pos-checkParts[j].offset)*checkParts[j].R)*checkParts[j].rscale, 
					m_thickness*checkParts[j].rscale,rsep*checkParts[j].rscale, &acontact)) 
				{
					Vec3 posnew = checkParts[j].R*(acontact.pt*checkParts[j].scale)+checkParts[j].offset;
					Vec3 nnew = checkParts[j].R*acontact.n;
					if ((posnew-m_vtx[i].pos)*nnew>0)
						m_vtx[i].pos = posnew;
					m_vtx[i].vreq = max(0.0f, m_thickness-nnew*(m_vtx[i].pos-posnew))*10.0f;
					if (m_vtx[i].pContactEnt && (m_vtx[i].ncontact-nnew)*m_gravity<0)
						continue;
					m_vtx[i].ncontact = nnew;
					m_vtx[i].pContactEnt = checkParts[j].pent;
					m_vtx[i].iContactPart = checkParts[j].ipart;
					m_vtx[i].iCheckPart = j;
					m_vtx[i].iContactNode = acontact.iFeature[0];
					m_vtx[i].vcontact = v;					
					m_vtx[i].surface_idx[1] = checkParts[j].surface_idx;
				}
			} else {
				aray.m_ray.origin = (m_vtx[i].pos-checkParts[j].offset)*checkParts[j].R;
				for(i1=0,j1=1; j1<checkParts[j].nCont; j1++) 
					i1 += j1-i1 & -isneg(fabs_tpl((aray.m_ray.origin-checkParts[j].contPlane[j1].origin)*checkParts[j].contPlane[j1].n)-
															 fabs_tpl((aray.m_ray.origin-checkParts[j].contPlane[i1].origin)*checkParts[j].contPlane[i1].n));
				if ((aray.m_ray.origin-checkParts[j].contPlane[i1].origin).len2() > checkParts[j].contRadius[i1]*1.2f) {
					aray.m_ray.dir = ((v-m_vtx[i].vel)*checkParts[j].R)*m_prevTimeInterval;
					aray.m_dirn = aray.m_ray.dir.normalized();
					aray.m_ray.origin -= aray.m_dirn*m_thickness;
					aray.m_ray.dir += aray.m_dirn*m_thickness;
				}	else
					aray.m_ray.dir = (aray.m_dirn=checkParts[j].contPlane[i1].n)*(checkParts[j].contDepth[i1]+m_thickness*2.0f);
				if (box_ray_overlap_check(&checkParts[j].bbox,&aray.m_ray)) {
					gwd.scale = checkParts[j].scale; 
					gwd.offset.zero(); gwd.R.SetIdentity();
					if (checkParts[j].pGeom->Intersect(&aray,&gwd,0,&ip,pcontacts)) {
						if (pcontacts->n*aray.m_dirn > 0) {
							m_vtx[i].pos = checkParts[j].R*(pcontacts->pt+aray.m_dirn*m_thickness)+checkParts[j].offset;
							m_vtx[i].ncontact = checkParts[j].R*pcontacts->n;
							m_vtx[i].pContactEnt = checkParts[j].pent;
							m_vtx[i].iContactPart = checkParts[j].ipart;
							m_vtx[i].iCheckPart = j;
							m_vtx[i].iContactNode = pcontacts->iNode[0];
							m_vtx[i].vcontact = v;
							imask = pcontacts->id[0]>>31;
							m_vtx[i].surface_idx[1] = checkParts[j].surface_idx&imask | pcontacts->id[0]&~imask;
						}
					}
				}
			}
		}

		if (((INT_PTR)pent & mask)!=((INT_PTR)m_vtx[i].pContactEnt & mask)) {
			if (pent) pent->Release();
			if (m_vtx[i].pContactEnt) m_vtx[i].pContactEnt->AddRef();
		} 
		m_maxMove = max(m_maxMove, (pos0-m_vtx[i].pos).len2());
	}

	if (bCollMode&1)
		return;

	if (!(m_flags & sef_volumetric)) 
		for(i=0; i<m_nEdges; i++) {	// calculate edge lengths
			l = m_vtx[m_edges[i].ivtx[0]].pos - m_vtx[m_edges[i].ivtx[1]].pos;
			m_edges[i].rlen = 1.0f/max(1E-4f,m_edges[i].len = sqrt_tpl(l.len2()));
		}
	else if (m_waterResistance+m_airResistance+m_kShapeStiffnessTang+m_kShapeStiffnessNorm>0) 
		for(i=0; i<m_nEdges; i++) {	// calculate edge lengths
			float len = (m_vtx[m_edges[i].ivtx[0]].pos - m_vtx[m_edges[i].ivtx[1]].pos).len2();
			m_edges[i].len = sqrt_fast_tpl(len);
			m_edges[i].rlen = isqrt_fast_tpl(len);
		}

	for(i=0,area=0; i<m_nVtx; i++) {
		// calculate normal
		for(j=m_vtx[i].iStartEdge,m_vtx[i].n.zero(); j<m_vtx[i].iEndEdge+m_vtx[i].bFullFan; j++) {
			imask = j-m_vtx[i].iEndEdge>>31; j1 = j+1&imask | m_vtx[i].iStartEdge&~imask;
			m_vtx[i].n += 
				m_vtx[m_edges[m_pVtxEdges[j ]].ivtx[1]+m_edges[m_pVtxEdges[j ]].ivtx[0]-i].pos-m_vtx[i].pos ^
				m_vtx[m_edges[m_pVtxEdges[j1]].ivtx[1]+m_edges[m_pVtxEdges[j1]].ivtx[0]-i].pos-m_vtx[i].pos;
		}
		if ((m_vtx[i].area = m_vtx[i].n.len2())>0) {
			m_vtx[i].area = sqrt_tpl(m_vtx[i].area);
			m_vtx[i].n /= m_vtx[i].area; m_vtx[i].area *= m_coverage*0.5f;
			area += m_vtx[i].area;
		}
	}
	if (area>0)
		area = m_nVtx/area;

	for(i=0; i<m_nVtx; i++) {	// apply gravity, buoyancy, wind (or water resistance)
		if (!m_vtx[i].bAttached) {
			if ((l0=(m_vtx[i].pos+m_pos-waterPlane.origin)*waterPlane.n)<0) {
				m_vtx[i].vel -= m_gravity*(waterDensity*m_vtxvol*time_interval*min(1.0f,-l0*rthickness));
				kr = m_waterResistance;	w = waterFlow;
			}	else {
				kr = m_airResistance;	w = m_wind0*m_windTimer+m_wind1*(1.0f-m_windTimer);
			}
			for(j=m_vtx[i].iStartEdge,windage=0; j<=m_vtx[i].iEndEdge; j++)
				windage += ((m_vtx[m_edges[m_pVtxEdges[j]].ivtx[1]].pos-m_vtx[m_edges[m_pVtxEdges[j]].ivtx[0]].pos)*m_vtx[i].n)*
					(iszero(i^m_edges[m_pVtxEdges[j]].ivtx[0])*2-1)*m_edges[m_pVtxEdges[j]].rlen;
			m_vtx[i].vel += m_vtx[i].n*((m_vtx[i].n*(w-m_vtx[i].vel))*m_vtx[i].area*area*(windage*m_vtx[i].rnEdges+1)*kr*time_interval);
			m_vtx[i].vel += m_gravity*time_interval;
		}
	}

	if (m_kShapeStiffnessTang+m_kShapeStiffnessNorm>0) { // apply shape-preserving forces
		int sg;
		float angle;
		for(i=0; i<m_nVtx; i++) if (!m_vtx[i].bAttached) {
			for(j=m_vtx[i].iStartEdge,angle=0; j<=m_vtx[i].iEndEdge; j++)	{
				j1 = j+1 + (m_vtx[i].iStartEdge-j-1 & ~(j-m_vtx[i].iEndEdge>>31));
				edge0 = m_vtx[m_edges[m_pVtxEdges[j ]].ivtx[1]+m_edges[m_pVtxEdges[j ]].ivtx[0]-i].pos-m_vtx[i].pos;
				edge1 = m_vtx[m_edges[m_pVtxEdges[j1]].ivtx[1]+m_edges[m_pVtxEdges[j1]].ivtx[0]-i].pos-m_vtx[i].pos;
				l = edge0^edge1; sg = sgnnz(edge1*edge0);
				angle += 1-m_vtx[i].n*edge0*m_edges[m_pVtxEdges[j]].rlen;
				if (l.len2()>0) {
					float angle0 = m_edges[m_pVtxEdges[j]].angle0[iszero(i-m_edges[m_pVtxEdges[j]].ivtx[1])];
					l *= ((angle0-1+sg)/l.len()-m_edges[m_pVtxEdges[j]].rlen*m_edges[m_pVtxEdges[j1]].rlen*sg);
					l *= time_interval*m_kShapeStiffnessTang;
					m_vtx[m_edges[m_pVtxEdges[j ]].ivtx[1]+m_edges[m_pVtxEdges[j ]].ivtx[0]-i].vel -= l^edge0;
					m_vtx[m_edges[m_pVtxEdges[j1]].ivtx[1]+m_edges[m_pVtxEdges[j1]].ivtx[0]-i].vel += l^edge1;
				}
			}
			for(j=m_vtx[i].iStartEdge; j<=m_vtx[i].iEndEdge; j++)	{
				edge0 = m_vtx[m_edges[m_pVtxEdges[j ]].ivtx[1]+m_edges[m_pVtxEdges[j ]].ivtx[0]-i].pos-m_vtx[i].pos;
				l	= (m_vtx[i].n*edge0.len2()-edge0*(m_vtx[i].n*edge0))*m_edges[m_pVtxEdges[j]].rlen;
				l *= (angle-m_vtx[i].angle0)*time_interval*m_kShapeStiffnessNorm;
				m_vtx[m_edges[m_pVtxEdges[j ]].ivtx[1]+m_edges[m_pVtxEdges[j ]].ivtx[0]-i].vel += l;
				m_vtx[i].vel -= l;
			}
		}
	}

	if (m_stiffnessAnim) { // apply simple animation target pull
		float denom = m_maxLevelDenom;
		for(j=m_nAttachedVtx; j<m_nConnectedVtx; j++) {
			i = m_vtx[j].idx;
			v = (lastqHost*m_vtx[i].ptAttach+lastposHost-m_vtx[i].pos-m_pos-m_offs0)*m_stiffnessAnim*(1-m_stiffnessDecayAnim*m_vtx[i].iSorted*denom);
			float t = max(0.0f, 1.0f-m_dampingAnim*time_interval);
			m_vtx[i].vel = m_vtx[i].vel*t + v*(1.0f-t);
		}
	}

	if (!(m_flags & sef_volumetric)) {
		iter=0; do {
			for(i=0,rmax=0; i<m_nConnectedVtx; i++) {
				i0 = m_vtx[i].idx;
				for(j=m_vtx[i0].iStartEdge; j<=m_vtx[i0].iEndEdge; j++)	
					if ((vreq = (m_edges[m_pVtxEdges[j]].len-m_edges[m_pVtxEdges[j]].len0)*m_ks)>-1e10f) {
						i1 = m_edges[m_pVtxEdges[j]].ivtx[0]+m_edges[m_pVtxEdges[j]].ivtx[1]-i0;
						if (m_vtx[i1].idx0<i)
							continue;
						l = (m_vtx[i1].pos-m_vtx[i0].pos)*m_edges[m_pVtxEdges[j]].rlen;
						F = l*-min(100.0f, vreq=(m_vtx[i0].vel-m_vtx[i1].vel)*l-vreq);
						m_vtx[i0].vel += F*(m_vtx[i0].massinv*m_edges[m_pVtxEdges[j]].kmass);
						m_vtx[i1].vel -= F*(m_vtx[i1].massinv*m_edges[m_pVtxEdges[j]].kmass);
						rmax = max(rmax,-vreq);
					}
				if (m_vtx[i0].pContactEnt && !m_vtx[i0].bAttached) {
					m_vtx[i0].vel -= m_vtx[i0].ncontact*min(0.0f, vreq = m_vtx[i0].ncontact*(m_vtx[i0].vel-m_vtx[i0].vcontact)-m_vtx[i0].vreq);
					rmax = max(rmax,-vreq);
					if (m_friction*vreq<0) {
						v = m_vtx[i0].vel-m_vtx[i0].vcontact;
						v -= m_vtx[i0].ncontact*(m_vtx[i0].ncontact*v);
						if (v.len2()>sqr(m_friction*vreq))
							m_vtx[i0].vel += v.normalized()*(m_friction*vreq);
						else
							m_vtx[i0].vel = m_vtx[i0].vcontact;
					}
				}
			}
		} while(rmax>m_accuracy && ++iter<m_nMaxIters);
	} else {
		for(i=0;i<m_nVtx;i++) m_vtx[i].pos0=m_vtx[i].pos-(m_vtx[i].vel0=m_vtx[i].vel)*(time_interval*0.5f);
		for(i=0; i<m_nEdges; i++) {	// calculate edge lengths
			l = m_vtx[m_edges[i].ivtx[0]].pos0 - m_vtx[m_edges[i].ivtx[1]].pos0;
			m_edges[i].rlen = 1.0f/max(1E-4f,m_edges[i].len = sqrt_tpl(l.len2()));
		}
		float ks = m_ks*m_parts[0].mass/m_nVtx, kmax = m_maxSafeStep>0 ? m_maxSafeStep:10.0f;
		for(i=0; i<m_nEdges; i++) {
			i0=m_edges[i].ivtx[0]; i1=m_edges[i].ivtx[1];
			l = m_vtx[i1].pos0-m_vtx[i0].pos0;
			F  = l*(ks*min(kmax,m_edges[i].rlen*(m_edges[i].len-m_edges[i].len0))*time_interval);
			F += l*(((m_vtx[i1].vel0-m_vtx[i0].vel0)*l)*ks*sqr(time_interval)*0.5f*sqr(m_edges[i].rlen));
			m_vtx[i0].vel += F*m_vtx[i0].massinv;	m_vtx[i1].vel -= F*m_vtx[i1].massinv;
		}
		for(v.zero(),i=0; i<m_nVtx; v+=m_vtx[i++].vel);
		float kd = 2*sqrt_tpl(m_ks)*time_interval;
		const float v0=3, kv=0.1f;
		for(v/=max(1,m_nVtx),i=0; i<m_nVtx; i++)
			m_vtx[i].vel = (m_vtx[i].vel-v)*max(0.5f,1-kd*max(m_damping,(sqrt_fast_tpl((m_vtx[i].vel-v).len2())-v0)*kv))+v;

		for(i=0; i<nCheckParts; i++) 
			checkParts[i].P.zero(), checkParts[i].L.zero(); 
		for(i0=0; i0<m_nVtx; i0++) if (m_vtx[i0].pContactEnt && !m_vtx[i0].bAttached) {
			v = m_vtx[i0].vel;
			m_vtx[i0].vel -= m_vtx[i0].ncontact*min(0.0f, vreq = m_vtx[i0].ncontact*(m_vtx[i0].vel-m_vtx[i0].vcontact)-m_vtx[i0].vreq);
			if (m_friction*vreq<0) {
				v = m_vtx[i0].vel-m_vtx[i0].vcontact;
				v -= m_vtx[i0].ncontact*(m_vtx[i0].ncontact*v);
				if (v.len2()>sqr(m_friction*vreq))
					m_vtx[i0].vel += v.normalized()*(m_friction*vreq);
				else
					m_vtx[i0].vel = m_vtx[i0].vcontact;
			}
			F = (v-m_vtx[i0].vel)*m_vtx[i0].mass;
			i = m_vtx[i0].iCheckPart;
			checkParts[i].P += F;
			checkParts[i].L += checkParts[i].posBody-(m_vtx[i0].pos+m_pos+m_offs0) ^ F;
		}

		pe_action_impulse ai;
		for(i=0; i<nCheckParts; i++) {
			ai.ipart = checkParts[i].ipart;
			ai.impulse = checkParts[i].P;
			ai.angImpulse = checkParts[i].L;
			checkParts[i].pent->Action(&ai);
		}

		if (m_pCore && !m_pCore->m_nColliders) {
			Vec3 center,axis;
			float mass,sint,cost;
			for(j=0,center.zero(),mass=0.0f; j<m_nCoreVtx; j++) {
				i = m_corevtx[j];
				center += m_vtx[i].pos*m_vtx[i].mass;
				mass += m_vtx[i].mass;
			}
			for(j=0,center/=mass,axis.zero(); j<m_nCoreVtx; j++) {
				i = m_corevtx[j];
				axis += (m_pCore->m_qrot*(m_vtx[i].posorg-m_pos0core) ^ m_vtx[i].pos-center)*m_vtx[i].mass;
			}
			for(j=0,axis.normalize(),sint=cost=0.0f; j<m_nCoreVtx; j++) {
				i = m_corevtx[j];
				Vec3 posCore = m_pCore->m_qrot*(m_vtx[i].posorg-m_pos0core), axisz = axis*(axis*posCore);
				sint += (axis^posCore)*(m_vtx[i].pos-center-axisz);
				cost += (posCore-axisz)*(m_vtx[i].pos-center-axisz);
			}
			m_pCore->Awake();
			pe_params_pos pp; 
			(pp.q = Quat::CreateRotationAA(atan2_tpl(sint,cost), axis)*m_pCore->m_qrot).Normalize();
			m_pCore->SetParams(&pp);
			m_pCore->m_forcedMove = center+m_pos+m_offs0-m_pCore->m_pos;
		}

		/*for(iter=0,j=1;iter<m_nMaxIters && j;iter++) for(i=j=0;i<0*m_nEdges;i++) 
			if (fabs_tpl(m_edges[i].len-m_edges[i].len0)>m_edges[i].len0*0.1f) {
				i0=m_edges[i].ivtx[0]; i1=m_edges[i].ivtx[1];
				Vec3 dir=m_vtx[i1].pos-m_vtx[i0].pos, dv=m_vtx[i1].vel-m_vtx[i0].vel;
				l = dir+dv*time_interval;
				float diff0=dir.len2()-sqr(m_edges[i].len0), diff1=l.len2()-sqr(m_edges[i].len0);
				if (diff0*diff1<0 && fabs_tpl(diff1)>fabs_tpl(diff0)) {
					F = dir*((m_vtx[i0].massinv+m_vtx[i1].massinv)*time_interval);
					float ka=F.len2(), kb=l*F, kc=l.len2()-sqr(m_edges[i].len0), kd=kb*kb-ka*kc;
					if (kd>0) {
						F = dir*((kb-sgnnz(dv*dir)*sqrt_tpl(kd))/ka);
						m_vtx[i0].vel += F*m_vtx[i0].massinv;	
						m_vtx[i1].vel -= F*m_vtx[i1].massinv;
						j++;
					}
				}
			}*/

		for(i=0;i<m_nVtx;i++) if (m_vtx[i].vel.len2()>sqr(10.0f))
			m_vtx[i].vel.normalize() *= 10.0f;
	}
#ifdef m_vtx
#undef m_vtx
#endif
}

int CSoftEntity::Step(float time_interval)
{
	if (m_nVtx<=0 || !m_bAwake || !m_nConnectedVtx)
		return 1;

	int iCaller = get_iCaller_int();
	if (m_flags & (pef_invisible|pef_disabled)) {
	report_step0:
		EventPhysPostStep event; InitEvent(&event,this,iCaller);
		event.dt=time_interval; event.pos=m_pos; event.q=m_qrot; event.idStep=m_pWorld->m_idStep;
		m_pWorld->OnEvent(m_flags,&event);
		return 1;
	}

	int i,j,nEnts=0,i0,i1,nCheckParts=0,bGridLocked=0,nhostPt=0,ijob=-2;
	Vec3 v,w,d,center,BBox0[2],BBox[2],lastposHost(ZERO),vHost(ZERO),wHost(ZERO),pos0;
	quaternionf lastqHost(IDENTITY);
	RigidBody rbody;
	float rmax,kr;
	box boxent;
	check_part checkParts[20];
	geom_world_data gwd,gwd1;
	CPhysicalEntity **pentlist,*pentHost;
	RigidBody *pbody;
	COverlapChecker Overlapper;
	pe_params_buoyancy pb[4];
	plane waterPlane; 
	Vec3 waterFlow(ZERO);
	float waterDensity=0,ktimeBack;
	{ ReadLock lock(m_lockSoftBody);

	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	PHYS_ENTITY_PROFILER

	if ((g_lastqHost|g_lastqHost)>0) {
		lastposHost=g_lastposHost; lastqHost=g_lastqHost;
		vHost=g_vHost; wHost=g_wHost;
		g_lastqHost.v.zero(); g_lastqHost.w=0;
		goto stepdone;
	}

	Overlapper.Init();
	boxent.size = (m_BBox[1]-m_BBox[0])*0.5f+Vec3(m_thickness,m_thickness,m_thickness)*2;
	center = (m_BBox[0]+m_BBox[1])*0.5f;
	boxent.bOriented = 1;

	for(i0=0;i0<m_nVtx;i0++) {
		i=m_vtx[i0].idx;
		if (m_vtx[i].pContactEnt && m_vtx[i].pContactEnt->m_iSimClass==7) {
			if (m_vtx[i].bAttached)	{
				m_vtx[i0].idx = m_vtx[m_nAttachedVtx-1].idx; m_vtx[--m_nAttachedVtx].idx = i;
				--i0;
			}
			m_vtx[i].pContactEnt->Release(); m_vtx[i].pContactEnt = 0; m_vtx[i].bAttached = 0;
		}
	}
	if (time_interval+m_windTimer==0.0f) {
		m_windTimer=0.001f; goto report_step0;
	}
	if (m_flags & sef_volumetric && time_interval<m_maxAllowedStep*0.95f && m_timeStepFull>m_maxAllowedStep)
		if ((m_timeStepSurplus+=time_interval) > m_maxAllowedStep) {
			m_timeStepSurplus-=m_maxAllowedStep; time_interval=m_maxAllowedStep;
		}	else
			return 1;
	if (m_timeStepPerformed>m_timeStepFull-0.001f)
		return 1;
	m_timeStepPerformed += time_interval;
	ktimeBack = (m_timeStepFull-m_timeStepPerformed)/m_timeStepFull*(iszero(m_pWorld->m_bWorldStep-2)^1);

	waterPlane.origin.Set(0,0,-1000); waterPlane.n.Set(0,0,1);
	if ((nEnts=m_pWorld->CheckAreas(this,w,pb,4)) && !is_unused(w) && (w-m_pWorld->m_vars.gravity).len2()>w.len2()*1E-4f)
		m_gravity = w;
	for(i=0,w=m_wind;i<nEnts;i++) if (pb[i].iMedium)
		w += pb[i].waterFlow;
	else {
		waterPlane.origin = pb[i].waterPlane.origin; waterPlane.n = pb[i].waterPlane.n;
		waterFlow = pb[i].waterFlow; waterDensity=pb[i].waterDensity;
	}
	if (!m_wind0.len2())
		m_wind0 = m_wind1 = w;

	if ((m_windTimer+=time_interval*4)>1.0f) {
		m_windTimer = 0; m_wind0 = m_wind1;
		float windage = m_windVariance*(fabs_tpl(w.x)+fabs_tpl(w.y)+fabs_tpl(w.z));
		m_wind1 = w+Vec3(cry_random(0.0f, windage),cry_random(0.0f, windage),cry_random(0.0f, windage));
	}

	if (m_thickness>0) {
		if (!m_collTypes && m_nAttachedVtx && (pentHost=m_vtx[m_vtx[0].idx].pContactEnt)) {
			pentlist=&pentHost; nEnts=1; 
			if (pentHost->m_flags & pef_parts_traceable)
				pentHost->m_nUsedParts |= 15<<iCaller*4;
		}	else
			nEnts = m_pWorld->GetEntitiesAround(m_BBox[0]-Vec3(m_thickness,m_thickness,m_thickness)*2,
				m_BBox[1]+Vec3(m_thickness,m_thickness,m_thickness)*2, pentlist, 
					m_collTypes|ent_sort_by_mass|ent_ignore_noncolliding|ent_triggers, this, 0,iCaller);
	}	else
		nEnts = 0;
	gwd.offset = m_qrot*m_parts[0].pos-m_offs0;
	gwd.R = Matrix33(m_qrot*m_parts[0].q);
	gwd.scale = m_parts[0].scale;

	{
	WriteLock lockEL(g_lockProcessedAux);
	for(j=nCheckParts=0,i=nEnts-1;i>=0;i--) if (pentlist[i]!=this && pentlist[i]!=m_pCore && !IgnoreCollision(m_collisionClass, pentlist[i]->m_collisionClass))
	for(i1=0,pentlist[i]->m_bProcessed_aux=nCheckParts<<24; i1<pentlist[i]->GetUsedPartsCount(iCaller); i1++) 
	if (pentlist[i]->m_parts[j=pentlist[i]->GetUsedPart(iCaller,i1)].flags & m_parts[0].flagsCollider &&
			!(pentlist[i]->m_parts[j].flags & geom_no_coll_response)) 
	{	pentlist[i]->GetLocTransformLerped(j, checkParts[nCheckParts].offset,lastqHost,kr,ktimeBack,this);
		boxent.Basis = Matrix33(lastqHost);
		boxent.center = (center-checkParts[nCheckParts].offset)*boxent.Basis;
		if (pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetType()!=GEOM_HEIGHTFIELD) {
			pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetBBox(&checkParts[nCheckParts].bbox);
			checkParts[nCheckParts].bbox.center *= pentlist[i]->m_parts[j].scale;
			checkParts[nCheckParts].bbox.size *= pentlist[i]->m_parts[j].scale;
			checkParts[nCheckParts].bPrimitive = pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->IsConvex(0.02f);
		} else {
			checkParts[nCheckParts].bbox = boxent;
			checkParts[nCheckParts].bPrimitive = 0;
		}
		boxent.bOriented++;
		if (box_box_overlap_check(&boxent,&checkParts[nCheckParts].bbox,&Overlapper)) {
			checkParts[nCheckParts].pent = pentlist[i];
			checkParts[nCheckParts].ipart = j;
			checkParts[nCheckParts].R = boxent.Basis;
			checkParts[nCheckParts].pGeom = (CGeometry*)pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom;
			checkParts[nCheckParts].scale = pentlist[i]->m_parts[j].scale;
			checkParts[nCheckParts].rscale = checkParts[nCheckParts].scale==1.0f ? 1.0f : 
				1.0f/checkParts[nCheckParts].scale;
			checkParts[nCheckParts].pGeom->PrepareForRayTest(m_thickness*2*checkParts[nCheckParts].rscale);
			checkParts[nCheckParts].offset -= m_pos+m_offs0;
			pbody = pentlist[i]->GetRigidBodyTrans(&rbody,j,this,2);
			checkParts[nCheckParts].vbody = pbody->v-(pbody->w^pbody->pos);
			checkParts[nCheckParts].wbody = pbody->w;
			checkParts[nCheckParts].posBody = pbody->pos;
			//checkParts[nCheckParts].bPrimitive = isneg(checkParts[nCheckParts].pGeom->GetPrimitiveCount()-2);
			checkParts[nCheckParts].surface_idx = pentlist[i]->m_parts[j].surface_idx;
			if (pentlist[i]->m_nParts<=24)
				pentlist[i]->m_bProcessed_aux |= 1u<<j;
			else
				pentlist[i]->m_parts[j].flags |= geom_removed;
			checkParts[nCheckParts].nCont = 0;
			checkParts[nCheckParts].contPlane[0].origin.zero();
			checkParts[nCheckParts].contRadius[0] = 0.0f;
			if (m_parts[0].pPhysGeom!=m_parts[0].pPhysGeomProxy && !checkParts[nCheckParts].bPrimitive) {
				if (!m_bSkinReady) {
					for(i0=0; i0<((CTriMesh*)m_parts[0].pPhysGeom->pGeom)->m_nTris; i0++)
						((CTriMesh*)m_parts[0].pPhysGeom->pGeom)->RecalcTriNormal(i0);
					((CTriMesh*)m_parts[0].pPhysGeom->pGeom)->RebuildBVTree();
					m_bSkinReady = 1;
				}
				geom_contact *pcontacts;
				gwd1.offset = checkParts[nCheckParts].offset;
				gwd1.R = checkParts[nCheckParts].R;
				gwd1.scale = checkParts[nCheckParts].scale;
				checkParts[nCheckParts].nCont = m_parts[0].pPhysGeom->pGeom->Intersect(checkParts[nCheckParts].pGeom, &gwd,&gwd1, 0, pcontacts);
				checkParts[nCheckParts].nCont = min(checkParts[nCheckParts].nCont, (int)(sizeof(checkParts[0].contPlane)/sizeof(checkParts[0].contPlane[0])));
				for(i0=0; i0<checkParts[nCheckParts].nCont; i0++) {
					checkParts[nCheckParts].contPlane[i0].origin = (pcontacts[i0].center-checkParts[nCheckParts].offset)*checkParts[nCheckParts].R;
					checkParts[nCheckParts].contPlane[i0].n = pcontacts[i0].dir*checkParts[nCheckParts].R;
					for(rmax=0,i1=0; i1<pcontacts[i0].nborderpt; i1++)
						rmax = max(rmax, (pcontacts[i0].ptborder[i1]-pcontacts[i0].center).len2());
					checkParts[nCheckParts].contRadius[i0] = rmax;
					checkParts[nCheckParts].contDepth[i0] = pcontacts[i0].t;
				}
			}
			AtomicAdd(&pentlist[i]->m_bProcessed, ~pentlist[i]->m_bProcessed&1<<iCaller);
			if (++nCheckParts==CRY_ARRAY_COUNT(checkParts))
				goto enoughgeoms;
		}
	} enoughgeoms:

	for(i0=m_nAttachedVtx; i0<m_nConnectedVtx; i0++) if (m_vtx[i=m_vtx[i0].idx].pContactEnt) 
		if (!(m_vtx[i].pContactEnt->m_bProcessed & 1<<iCaller) ||
				 (unsigned int)(j = m_vtx[i].pContactEnt->GetCheckPart(m_vtx[i].iContactPart))>=(unsigned int)nCheckParts)
		{
			m_vtx[i].pContactEnt->Release(); m_vtx[i].pContactEnt=0; 
		}	else
			m_vtx[i].iCheckPart = j;
	for(i=0; i<nCheckParts; i++) if (checkParts[i].pent->m_bProcessed & 1<<iCaller) {
		AtomicAdd(&checkParts[i].pent->m_bProcessed, -(1<<iCaller));
		if (checkParts[i].pent->m_nParts>24) for(j=0;j<checkParts[i].pent->m_nParts;j++)
			checkParts[i].pent->m_parts[j].flags &= ~geom_removed;
	}
	}	// g_lockProcessedAux

	for(i0=i1=0,center.zero(); i0<m_nAttachedVtx; i0++) {
		i = m_vtx[i0].idx;
		if (m_vtx[i1].pContactEnt!=m_vtx[i].pContactEnt || m_vtx[i1].iContactPart!=(m_vtx[i].iContactPart|nhostPt-1>>31)) {
			pbody = m_vtx[i].pContactEnt->GetRigidBodyTrans(&rbody,m_vtx[i].iContactPart,this,2);
			m_vtx[i].pContactEnt->GetLocTransformLerped(m_vtx[i].iContactPart, lastposHost,lastqHost,kr,ktimeBack, this);
			vHost+=pbody->v; wHost+=pbody->w;	center+=pbody->pos; i1=i; nhostPt++;
		}
		m_vtx[i].pos = lastposHost+lastqHost*m_vtx[i].ptAttach;
		m_vtx[i].vcontact = m_vtx[i].vel = (pbody->v+(pbody->w^m_vtx[i].pos-pbody->pos))*(1-m_kRigid);
		m_vtx[i].pos -= m_pos+m_offs0;

		if (min(m_maxSafeStep,m_timeStepPerformed+0.001f-m_timeStepFull)>0) 
			for(j=m_vtx[i].iStartEdge; j<=m_vtx[i].iEndEdge; j++) if (m_edges[m_pVtxEdges[j]].len>=0) {
				int i2 = m_edges[m_pVtxEdges[j]].ivtx[0]+m_edges[m_pVtxEdges[j]].ivtx[1]-i;
				if (m_vtx[i2].idx0>i0 && (m_vtx[i2].pos-m_vtx[i].pos).len2() > sqr(m_edges[m_pVtxEdges[j]].len0*(1.0f+m_maxSafeStep))) {
					m_vtx[i2].pos = m_vtx[i].pos + (m_vtx[i2].pos-m_vtx[i].pos).normalized()*(m_edges[m_pVtxEdges[j]].len0*(1.0f+m_maxSafeStep));
					m_edges[m_pVtxEdges[j]].len = -1;
				}
			}
	}
	if (nhostPt>0) {
		kr = 1.0f/nhostPt;
		vHost*=kr; wHost*=kr;	center*=kr;
	}
	if (m_kRigid>0) {
		for(; i0<m_nConnectedVtx; i0++)	{
			Vec3 vtxRigid = lastposHost + lastqHost*((m_vtx[m_vtx[i0].idx].pos+m_pos+m_offs0-m_lastposHost-m_lastPos)*m_lastqHost) - m_pos-m_offs0;
			(m_vtx[m_vtx[i0].idx].pos *= 1-m_kRigid) += vtxRigid*m_kRigid;
		}
		for(i=0;i<nCheckParts;i++) {
			checkParts[i].vbody -= (vHost-(wHost^center))*m_kRigid;
			checkParts[i].wbody -= wHost*m_kRigid;
		}
	}

	StepInner(time_interval,0, checkParts,nCheckParts, waterPlane,waterFlow,waterDensity, lastposHost,lastqHost, m_vtx);
	stepdone:

	pos0 = m_vtx[m_vtx[0].idx].pos;
	rmax = m_maxMove/sqr(max(0.0001f,time_interval));
	BBox[0].zero(); BBox[1].zero();	
	for(i=0; i<m_nVtx; i++) {
		v = vHost + (wHost ^ m_vtx[i].pos+m_pos+m_offs0-lastposHost);
		kr = min(1.0f,m_damping*time_interval)*(1+(-m_vtx[i].bAttached>>31));
		m_vtx[i].vel = m_vtx[i].vel*(1.0f-kr) + v*kr;
		m_vtx[i].pos -= pos0;
		rmax = max(rmax, m_vtx[i].vel.len2()*m_vtx[i].bAttached);
		BBox[0] = min(BBox[0], m_vtx[i].pos);
		BBox[1] = max(BBox[1], m_vtx[i].pos);
	}
	if ((m_wind0+m_wind1).len2()*m_airResistance>0 || rmax>m_Emin) {
		m_nSlowFrames = 0; m_bAwake = 1;
	} else if (++m_nSlowFrames>=3) {
		m_bAwake = 0;
		if (m_pCore) {
			pe_params_pos pp; pp.iSimClass=1;
			m_pCore->SetParams(&pp);
		}
	}
	
	BBox[0] += m_pos+pos0+m_offs0; BBox[1] += m_pos+pos0+m_offs0;
	} // m_lockSoftBody
	bGridLocked = m_pWorld->RepositionEntity(this,3-iszero((int)m_flags & pef_traceable),BBox);

	{ WriteLock locku(m_lockUpdate);
		m_BBox[0] = BBox[0]; m_BBox[1] = BBox[1];
		m_pos += pos0;
		m_prevTimeInterval = time_interval;

		if (!(m_parts[0].flags & geom_can_modify))
			m_pWorld->ClonePhysGeomInEntity(this, 0, m_pWorld->CloneGeometry(m_parts[0].pPhysGeom->pGeom));

		float rscale = m_parts[0].scale==1.0f ? 1.0f:1.0f/m_parts[0].scale;
		box bbox;
		bbox.Basis = Matrix33(!m_parts[0].q);
		bbox.center = (m_BBox[0]+m_BBox[1])*0.5f-m_pos-m_parts[0].pos;
		bbox.size = (m_BBox[1]-m_BBox[0])*(0.5f*rscale);
		CTriMesh *pMesh = (CTriMesh*)m_parts[0].pPhysGeomProxy->pGeom;
		if (pMesh->m_pTree->GetType()==BVT_SINGLEBOX)
			((CSingleBoxTree*)pMesh->m_pTree)->SetBox(&bbox);
		if (m_parts[0].scale==1.0f && m_parts[0].q.w==1.0f)	for(i=0;i<m_nVtx;i++) {
			pMesh->m_pVertices[i] = (m_vtx[i].pos + m_offs0) * m_qrot - m_parts[0].pos;
			m_vtx[i].nmesh = m_vtx[i].n*m_qrot;
		} else for(i=0;i<m_nVtx;i++) {
			pMesh->m_pVertices[i] = ((m_vtx[i].pos + m_offs0) * m_qrot - m_parts[0].pos) * rscale * m_parts[0].q ;
			m_vtx[i].nmesh = m_vtx[i].n*m_qrot;
		}
		m_bMeshUpdated = 1;
		m_bSkinReady = 0;
		m_pWorld->UnlockGrid(this,-bGridLocked);
		m_lastposHost=lastposHost-m_pos; m_lastqHost=lastqHost;
		m_lastPos = m_pos;
	}
	//for(i=0;i<pMesh->m_nTris;i++)
	//	MARK_UNUSED pMesh->m_pNormals[i];

	EventPhysPostStep epps;	InitEvent(&epps,this,iCaller);
	epps.dt=time_interval; epps.pos=m_pos; epps.q=m_qrot; epps.idStep=m_pWorld->m_idStep;
	m_pWorld->OnEvent(m_flags&pef_monitor_poststep, &epps);
	if (m_pWorld->m_iLastLogPump==m_iLastLog && m_pEvent) {
		WriteLock lockq(m_pWorld->m_lockEventsQueue);
		m_pEvent->dt += time_interval;
		m_pEvent->idStep = m_pWorld->m_idStep;
		m_pEvent->pos = m_pos; m_pEvent->q = m_qrot;	
	}	else {
		m_pEvent = 0;
		m_pWorld->OnEvent(m_flags&pef_log_poststep,&epps,&m_pEvent);
		m_iLastLog = m_pWorld->m_iLastLogPump;
	}

	return isneg(m_timeStepFull-m_timeStepPerformed-0.001f);
}


void CSoftEntity::ApplyVolumetricPressure(const Vec3 &epicenter, float kr, float rmin)
{
	if (m_nVtx>0) {
		WriteLock lock(m_lockSoftBody);
		CTriMesh *pMesh = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
		Vec3 ptc,r,n,pt[3],dP;
		int i,j,idx[3],ipass;
		float r2,rmin2=sqr(rmin),scale,P;
		kr *= m_explosionScale;
		
		for(ipass=0,scale=m_maxCollImpulse>0.0f ? 0.0f:1.0f,P=0.0f; ipass<2; ipass++) {
			for(i=0; i<pMesh->m_nTris; i++) {
				for(j=0;j<3;j++) pt[j]=m_vtx[idx[j]=pMesh->m_pIndices[i*3+j]].pos+m_pos+m_offs0;
				ptc = (pt[0]+pt[1]+pt[2])*(1.0f/3); r = ptc-epicenter; r2 = r.len2();
				n = pt[2]-pt[0]^pt[1]-pt[0]; 
				if (n*r<0.0f)
					continue;
				dP = n*((n*r)*0.5f*kr/(sqrt_tpl(n.len2()*r2)*max(rmin2,r2)));
				for(j=0;j<3;j++) 
					m_vtx[idx[j]].vel += dP*(m_vtx[idx[j]].massinv*scale);
				P += dP.len()*3.0f;
			}
			if (scale)
				break;
			scale = P>m_maxCollImpulse ? m_maxCollImpulse/P : 1.0f;
		}
		if (!(m_flags & sef_skeleton)) {
			pe_action_awake pa;
			Action(&pa, -(get_iCaller()-MAX_PHYS_THREADS>>31));
		}
	}
}

static geom_contact g_SoftContact[MAX_PHYS_THREADS+1];

int CSoftEntity::RayTrace(SRayTraceRes& rtr)
{
	if (m_nVtx>0) {
		CTriMesh *pMesh = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
		prim_inters inters;
		triangle atri;
		int i,j;
		float mindist=1e10f, dist;

		for(i=0;i<pMesh->m_nTris;i++) {
			for(j=0;j<3;j++) atri.pt[j] = m_vtx[pMesh->m_pIndices[i*3+j]].pos+m_pos+m_offs0;
			atri.n = atri.pt[1]-atri.pt[0] ^ atri.pt[2]-atri.pt[0];
			if (ray_tri_intersection(&rtr.pRay->m_ray,&atri,&inters) && (dist = (inters.pt[0]-rtr.pRay->m_ray.origin)*rtr.pRay->m_dirn) < mindist) {
				rtr.pcontacts = &g_SoftContact[get_iCaller()];
				rtr.pcontacts->pt = inters.pt[0];
				rtr.pcontacts->t = mindist = dist;
				rtr.pcontacts->id[0] = pMesh->m_pIds ? pMesh->m_pIds[i] : m_parts[0].surface_idx;
				rtr.pcontacts->iNode[0] = i;
				rtr.pcontacts->n = atri.n.normalized()*-sgnnz(atri.n*rtr.pRay->m_dirn);
			}
		}
		return mindist < 1e10f;
	}

	return 0;
}


int CSoftEntity::GetStateSnapshot(CStream &stm,float time_back,int flags)
{
	stm.WriteNumberInBits(SNAPSHOT_VERSION,4);
	stm.WriteBits((uint8*)&m_qrot0,sizeof(m_qrot0)*8);
	stm.Write(m_bAwake!=0);
	return 1;
}

int CSoftEntity::SetStateFromSnapshot(CStream &stm, int flags)
{
	int ver=0; 
	bool bnz;

	stm.ReadNumberInBits(ver,4);
	if (ver!=SNAPSHOT_VERSION)
		return 0;

	if (!(flags & ssf_no_update)) {
		pe_params_pos pp;
		stm.ReadBits((uint8*)&pp.q,sizeof(pp.q)*8);
		SetParams(&pp);
		stm.Read(bnz);
		m_bAwake = bnz? 1:0;
	} else
		stm.Seek(stm.GetReadPos()+sizeof(quaternionf)*8+1);

	return 1;
}

int CSoftEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	if (m_flags & sef_skeleton || flags & 16)
		if (ser.BeginOptionalGroup("updated", m_bMeshUpdated != 0)) {
			ser.Value("pos", m_pos);
			ser.Value("q", m_qrot);
			ser.Value("q0", m_qrot0);
			bool awake = m_bAwake!=0;
			ser.Value("awake", awake);
			for(int i=0; i<m_nVtx; i++) {
				ser.BeginGroup("vtx");
				ser.Value("pos",m_vtx[i].pos);
				if (m_bAwake)
					ser.Value("vel",m_vtx[i].vel);
				ser.EndGroup();
			}
			ser.EndGroup();
		}
	return 1;
}

int CSoftEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	if (m_flags & sef_skeleton || flags & 16) {
		if (m_bMeshUpdated = ser.BeginOptionalGroup("updated", true)) {
			ser.Value("pos", m_pos);
			ser.Value("q", m_qrot);
			ser.Value("q0", m_qrot0);
			bool awake; ser.Value("awake", awake);
			m_bAwake = awake ? 1:0;
			int i; for(i=0; i<m_nVtx; i++) {
				ser.BeginGroup("vtx");
				ser.Value("pos",m_vtx[i].pos);
				if (m_bAwake)
					ser.Value("vel",m_vtx[i].vel);
				ser.EndGroup();
			}
			CTriMesh *pMesh = (CTriMesh*)m_parts[0].pPhysGeomProxy->pGeom;
			Vec3 d = m_offs0-m_parts[0].pos;
			float rscale = 1.0f/m_parts[0].scale;
			Vec3 BBox[2];
			BBox[0]=BBox[1] = ((m_vtx[0].pos+d)*rscale)*m_parts[0].q;
			for(i=0;i<m_nVtx;i++) {
				Vec3 vtx = pMesh->m_pVertices[i] = ((m_vtx[i].pos+d)*rscale)*m_parts[0].q;
				BBox[0]=min(BBox[0],vtx); BBox[1]=max(BBox[1],vtx); 
			}
			box bbox; bbox.bOriented=0;	bbox.Basis.SetIdentity();
			bbox.center=(BBox[0]+BBox[1])*0.5f; bbox.size=(BBox[1]-BBox[0])*0.5f;
			if (pMesh->m_pTree->GetType()==BVT_SINGLEBOX)
				((CSingleBoxTree*)pMesh->m_pTree)->SetBox(&bbox);
			ser.EndGroup();
		}
	}
	return 1;
}

void CSoftEntity::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{
	CPhysicalEntity::DrawHelperInformation(pRenderer,flags);

	int i;
	Vec3 offs = m_pos+m_offs0;
	float thickness = m_thickness;
	if (flags & pe_helper_collisions) {
		sphere sph; sph.r=m_thickness; sph.center.zero(); 
		CSphereGeom gsph; gsph.CreateSphere(&sph);
		geom_world_data gwd; gwd.iStartNode = -10;
		for(i=0;i<m_nVtx;i++) {
			gwd.offset = m_vtx[i].pos+m_pos+m_offs0;
			pRenderer->DrawGeometry(&gsph,&gwd);
		}
		thickness *= 2.5f;
	}
	if (flags & pe_helper_geometry) for(i=0;i<m_nEdges;i++)
		pRenderer->DrawLine(m_vtx[m_edges[i].ivtx[0]].pos+offs, m_vtx[m_edges[i].ivtx[1]].pos+offs, m_iSimClass);
	if (flags & pe_helper_collisions) for(i=0;i<m_nVtx;i++) if (m_vtx[i].pContactEnt)
		pRenderer->DrawLine(m_vtx[i].pos+offs+m_vtx[i].ncontact*thickness, m_vtx[i].pos+offs, m_iSimClass);
}


void CSoftEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	if (GetType()==PE_SOFT)
		pSizer->AddObject((CSoftEntity*)this, sizeof(CSoftEntity));
	CPhysicalEntity::GetMemoryStatistics(pSizer);
	pSizer->AddObject(m_vtx, m_nVtx*sizeof(m_vtx[0]));
	pSizer->AddObject(m_edges, m_nEdges*sizeof(m_edges[0]));
	pSizer->AddObject(m_pVtxEdges, m_nEdges*2*sizeof(m_pVtxEdges[0]));
	if (m_parts[0].pLattice) {
		pSizer->AddObject(m_pTetrEdges, m_parts[0].pLattice->m_nTetr*6*sizeof(m_pTetrEdges[0]));
		pSizer->AddObject(m_pTetrQueue, m_parts[0].pLattice->m_nTetr*sizeof(m_pTetrQueue[0]));
	}
}

#undef CMemStream
#undef se_vertex
#undef m_bAwake
