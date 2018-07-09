// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "intersectionchecks.h"
#include "unprojectionchecks.h"
#include "bvtree.h"
#include "aabbtree.h"
#include "obbtree.h"
#include "singleboxtree.h"
#include "geometry.h"
#include "trimesh.h"
#include "tetrlattice.h"


SCGFace *CTetrLattice::g_Faces = 0;
SCGTetr *CTetrLattice::g_Tets = 0;
int CTetrLattice::g_nFacesAlloc = 0;
int CTetrLattice::g_nTetsAlloc = 0;


CTetrLattice::CTetrLattice(IPhysicalWorld *pWorld)
	: m_pWorld(pWorld)
	, m_pMesh(nullptr)
	, m_pVtx(nullptr)
	, m_nVtx(0)
	, m_pTetr(nullptr)
	, m_nTetr(0)
	, m_pVtxFlags(nullptr)
	, m_nMaxCracks(4)
	, m_idmat(0)
	, m_maxForcePush(0.01f)
	, m_maxForcePull(0.01f)
	, m_maxForceShift(0.01f)
	, m_maxTorqueTwist(0.01f)
	, m_maxTorqueBend(0.01f)
	, m_crackWeaken(0.4f)
	, m_density(1.0f)
	, m_nRemovedTets(0)
	, m_pVtxRemap(nullptr)
	, m_flags(0)
	, m_maxTension(0.0f)
	, m_imaxTension(LPush)
	, m_lastImpulseTime(-1.0f)
	, m_pGridTet0(nullptr)
	, m_pGrid(nullptr)
{
}

CTetrLattice::CTetrLattice(CTetrLattice *src, int bCopyData)
	: m_pWorld(src->m_pWorld)
	, m_pMesh(nullptr)
	, m_pVtx(nullptr)
	, m_nVtx(0)
	, m_pTetr(nullptr)
	, m_nTetr(0)
	, m_pVtxFlags(nullptr)
	, m_nMaxCracks(src->m_nMaxCracks)
	, m_idmat(src->m_idmat)
	, m_maxForcePush(src->m_maxForcePush)
	, m_maxForcePull(src->m_maxForcePull)
	, m_maxForceShift(src->m_maxForceShift)
	, m_maxTorqueTwist(src->m_maxTorqueTwist)
	, m_maxTorqueBend(src->m_maxTorqueBend)
	, m_crackWeaken(src->m_crackWeaken)
	, m_density(src->m_density)
	, m_nRemovedTets(0)
	, m_pVtxRemap(0)
	, m_flags(src->m_flags)
	, m_maxTension(0)
	, m_imaxTension(LPush)
	, m_lastImpulseTime(-1.0f)
	, m_RGrid(src->m_RGrid)
	, m_posGrid(src->m_posGrid)
	, m_stepGrid(src->m_stepGrid)
	, m_rstepGrid(src->m_rstepGrid)
	, m_szGrid(src->m_szGrid)
	, m_strideGrid(src->m_strideGrid)
	, m_pGridTet0(nullptr)
	, m_pGrid(nullptr)
{
	if (bCopyData) {
		memcpy(m_pVtx = new Vec3[m_nVtx=src->m_nVtx], src->m_pVtx, src->m_nVtx*sizeof(m_pVtx[0]));
		memcpy(m_pVtxFlags = new int[m_nVtx], src->m_pVtxFlags, m_nVtx*sizeof(m_pVtxFlags[0]));
		memcpy(m_pTetr = new STetrahedron[m_nTetr=src->m_nTetr], src->m_pTetr, src->m_nTetr*sizeof(m_pTetr[0]));
		if (src->m_pGrid) {
			memcpy(m_pGridTet0 = new int[m_szGrid.GetVolume()+1], src->m_pGridTet0, (m_szGrid.GetVolume()+1)*sizeof(m_pGridTet0[0]));
			memcpy(m_pGrid = new int[m_pGridTet0[m_szGrid.GetVolume()]], src->m_pGrid, m_pGridTet0[m_szGrid.GetVolume()]*sizeof(m_pGrid[0]));
		}
		m_nRemovedTets = src->m_nRemovedTets;
	}
}


CTetrLattice::~CTetrLattice()
{
	if (m_pVtx) delete[] m_pVtx;
	if (m_pVtxFlags) delete[] m_pVtxFlags;
	if (m_pTetr) delete[] m_pTetr;
	if (m_pGridTet0) delete[] m_pGridTet0;
	if (m_pGrid) delete[] m_pGrid;
	if (m_pVtxRemap) delete[] m_pVtxRemap;
}


CTetrLattice *CTetrLattice::CreateLattice(const Vec3 *pt,int npt, const int *pTets,int nTets)
{
	int i,j,*pTet0,*pVtxTets,pEdgeTets[64],pFaceTets[8];
	Vec3 vtx[4],vsum,n;
	Matrix33 I,mtx;

	memcpy(m_pVtx = new Vec3[m_nVtx=npt], pt, npt*sizeof(m_pVtx[0]));
	memset(m_pVtxFlags = new int[m_nVtx], 0, npt*sizeof(m_pVtxFlags[0]));
	m_pTetr = new STetrahedron[(m_nTetr=nTets)+1];

	for(i=0;i<m_nTetr;i++) {
		m_pTetr[i].flags = 0;
		for(j=0,vsum.zero();j<4;j++)
			vsum += (vtx[j] = pt[m_pTetr[i].ivtx[j] = pTets[i*4+j]]); 
		if ((vtx[1]-vtx[0] ^ vtx[2]-vtx[0])*(vtx[3]-vtx[0])>0) {
			vtx[0] = pt[m_pTetr[i].ivtx[0]=pTets[i*4+1]]; 
			vtx[1] = pt[m_pTetr[i].ivtx[1]=pTets[i*4]];
		}
		for(j=0;j<4;j++) vtx[j] -= vsum*0.25f;
		n = vtx[3]-vtx[0]^vtx[2]-vtx[0];
		m_pTetr[i].M = ((vtx[1]-vtx[0])*n)*(1.0f/6);
		m_pTetr[i].Vinv = m_pTetr[i].Minv = 1/m_pTetr[i].M;
		m_pTetr[i].area = n.len()*0.5f;
		I.SetZero();
		for(j=0;j<4;j++)
			I -= dotproduct_matrix(vtx[j],vtx[j],mtx);
		vsum.Set(-I(0,0),-I(1,1),-I(2,2));
		for(j=0;j<3;j++) I(j,j)=vsum[inc_mod3[j]]+vsum[dec_mod3[j]]; 
		(m_pTetr[i].Iinv = I*(m_pTetr[i].M*(1.0f/20))).Invert();
		for(j=0;j<4;j++) 
			m_pTetr[i].fracFace[j] = (vtx[j+2&3]-vtx[j+1&3]^vtx[j+3&3]-vtx[j+1&3]).len();
		m_pTetr[i].idx = -1;
		m_pTetr[i].Pext.zero(); m_pTetr[i].Lext.zero();
	}

	memset(pTet0 = new int[m_nVtx+1], 0,(m_nVtx+1)*sizeof(int)); // for each used vtx, points to the corresponding tetr list start
	pVtxTets = new int[m_nTetr*4]; // holds tetrahedra lists for each used vtx

	for(i=0;i<m_nTetr;i++) for(j=0;j<4;j++) 
		pTet0[m_pTetr[i].ivtx[j]]++;
	for(i=0;i<m_nVtx;i++) pTet0[i+1] += pTet0[i];
	for(i=m_nTetr-1;i>=0;i--) for(j=0;j<4;j++) 
		pVtxTets[--pTet0[m_pTetr[i].ivtx[j]]] = i;
	for(i=0;i<m_nTetr;i++) for(j=0;j<4;j++) {
		nTets = intersect_lists(pVtxTets+pTet0[m_pTetr[i].ivtx[j+1&3]], pTet0[m_pTetr[i].ivtx[j+1&3]+1]-pTet0[m_pTetr[i].ivtx[j+1&3]],
			pVtxTets+pTet0[m_pTetr[i].ivtx[j+2&3]], pTet0[m_pTetr[i].ivtx[j+2&3]+1]-pTet0[m_pTetr[i].ivtx[j+2&3]], pEdgeTets);
		nTets = intersect_lists(pVtxTets+pTet0[m_pTetr[i].ivtx[j+3&3]], pTet0[m_pTetr[i].ivtx[j+3&3]+1]-pTet0[m_pTetr[i].ivtx[j+3&3]],
			pEdgeTets,nTets, pFaceTets);
		// if (nTets>2) - error in topology
		m_pTetr[i].ibuddy[j] = pFaceTets[iszero(i-pFaceTets[0])] | nTets-2>>31;
	}

	delete[] pVtxTets; delete[] pTet0;
	return this;
}


void CTetrLattice::SetMesh(CTriMesh *pMesh)
{
	m_pMesh = pMesh;

	if (!m_pGrid) {
		box bbox;
		m_pMesh->GetBBox(&bbox);
		SetGrid(bbox);
	}
}


void CTetrLattice::SetGrid(const box &bbox)
{
	int i,j,k,i0,i1,i2,nSlots,*pTetCells=0;
	float rstep,s,e;
	Vec3 sz,n,pt,vtx[4],BBox[2];
	Vec3i iBBox[2],ic;

	m_RGrid = bbox.Basis.T();
	BBox[0] = BBox[1] = m_pVtx[0]*m_RGrid;
	for(i=1;i<m_nVtx;i++) if (!(m_pVtxFlags[i] & lvtx_removed)) {
		BBox[0] = min(BBox[0], pt=m_pVtx[i]*m_RGrid);
		BBox[1] = max(BBox[1], pt);
	}
	m_posGrid = m_RGrid*BBox[0];
	sz = BBox[1]-BBox[0];
	rstep = cubert_tpl(m_nTetr*4/sz.GetVolume());
	for(i=0;i<3;i++) {
		m_szGrid[i] = max(1,float2int(sz[i]*rstep));
		m_stepGrid[i] = sz[i]/m_szGrid[i];
		m_rstepGrid[i] = m_szGrid[i]/sz[i];
	}
	m_strideGrid.Set(m_szGrid.z*m_szGrid.y,m_szGrid.z,1);
	memset(m_pGridTet0 = new int[m_szGrid.GetVolume()+1], 0, (m_szGrid.GetVolume()+1)*sizeof(m_pGridTet0[0]));;

	for(i=nSlots=0;i<m_nTetr;i++) {
		m_pTetr[i].idxface[0] = nSlots;
		BBox[0]=BBox[1]=vtx[0] = (m_pVtx[m_pTetr[i].ivtx[0]]-m_posGrid)*m_RGrid;
		for(j=1;j<4;j++) {
			vtx[j] = (m_pVtx[m_pTetr[i].ivtx[j]]-m_posGrid)*m_RGrid;
			BBox[0] = min(BBox[0], vtx[j]);
			BBox[1] = max(BBox[1], vtx[j]);
		}
		for(j=0;j<3;j++) iBBox[0][j] = min(m_szGrid[j],max(0,float2int(BBox[0][j]*m_rstepGrid[j]-0.5f)));
		for(j=0;j<3;j++) iBBox[1][j] = min(m_szGrid[j],max(0,float2int(BBox[1][j]*m_rstepGrid[j]+0.5f)));
		for(ic.x=iBBox[0].x;ic.x<iBBox[1].x;ic.x++) for(ic.y=iBBox[0].y;ic.y<iBBox[1].y;ic.y++) for(ic.z=iBBox[0].z;ic.z<iBBox[1].z;ic.z++) {
			pt.Set((ic.x+0.5f)*m_stepGrid.x, (ic.y+0.5f)*m_stepGrid.y, (ic.z+0.5f)*m_stepGrid.z);
			for(j=0;j<4;j++) {
				n = vtx[j+2&3]-vtx[j+1&3] ^ vtx[j+3&3]-vtx[j+1&3];
				s = vtx[j+1&3]*n; e = vtx[j]*n;
				if (fabs_tpl(s+e-(pt*n)*2) > fabs_tpl(s-e)+m_stepGrid*n.abs())
					goto separate;
			}
			for(j=0;j<6;j++) for(k=0;k<3;k++) {
				i2=j>>2; i0=(j>>1)+i2; i1=i0+1+((j&1)<<i2)&3;
				n = cross_with_ort(vtx[i0]-vtx[i1],k);
				s = min(min(min(vtx[0]*n,vtx[1]*n),vtx[2]*n),vtx[3]*n);
				e = max(max(max(vtx[0]*n,vtx[1]*n),vtx[2]*n),vtx[3]*n);
				if (fabs_tpl(s+e-(pt*n)*2) > e-s+m_stepGrid*n.abs())
					goto separate;
			}
			m_pGridTet0[j=ic*m_strideGrid]++;
			if ((nSlots & 255)==0)
				ReallocateList(pTetCells, nSlots,nSlots+256);
			pTetCells[nSlots++] = j;
			separate:;
		}
		m_pTetr[i].idxface[1] = nSlots;
	}
	m_pGrid = new int[nSlots];
	for(i=1,j=m_szGrid.GetVolume(); i<=j; i++) m_pGridTet0[i]+=m_pGridTet0[i-1];
	for(i=m_nTetr-1;i>=0;i--)	for(j=m_pTetr[i].idxface[0];j<m_pTetr[i].idxface[1];j++)
		m_pGrid[--m_pGridTet0[pTetCells[j]]] = i;
	delete[] pTetCells;
}


int CTetrLattice::SetParams(pe_params *_params)
{
	if (_params->type==pe_tetrlattice_params::type_id) {
		pe_tetrlattice_params *params = (pe_tetrlattice_params*)_params;
		if (!is_unused(params->density) && params->density!=m_density) {
			float diff=params->density/m_density, rdiff=1/diff;
			for(int i=0;i<m_nTetr;i++) {
				m_pTetr[i].M*=diff; m_pTetr[i].Minv*=rdiff; m_pTetr[i].Iinv*=rdiff;
			}
			m_maxForcePush*=diff; m_maxForcePull*=diff; m_maxForceShift*=diff;
			m_maxTorqueTwist*=diff; m_maxTorqueBend*=diff;
			m_density = params->density;
		}
		if (!is_unused(params->nMaxCracks)) m_nMaxCracks = params->nMaxCracks;
		if (!is_unused(params->maxForcePush)) m_maxForcePush = params->maxForcePush;
		if (!is_unused(params->maxForcePull)) m_maxForcePull = params->maxForcePull;
		if (!is_unused(params->maxForceShift)) m_maxForceShift = params->maxForceShift;
		if (!is_unused(params->maxTorqueTwist)) m_maxTorqueTwist = params->maxTorqueTwist;
		if (!is_unused(params->maxTorqueBend)) m_maxTorqueBend = params->maxTorqueBend;
		if (!is_unused(params->crackWeaken)) m_crackWeaken = params->crackWeaken;

		return 1;
	}
	return 0;
}

int CTetrLattice::GetParams(pe_params *_params)
{
	if (_params->type==pe_tetrlattice_params::type_id) {
		pe_tetrlattice_params *params = (pe_tetrlattice_params*)_params;
		params->density = m_density;
		params->nMaxCracks = m_nMaxCracks;
		params->maxForcePush = m_maxForcePush;
		params->maxForcePull = m_maxForcePull;
		params->maxForceShift = m_maxForceShift;
		params->maxTorqueTwist = m_maxTorqueTwist;
		params->maxTorqueBend = m_maxTorqueBend;
		params->crackWeaken = m_crackWeaken;
		return 1;
	}
	return 0;
}


void CTetrLattice::Subtract(IGeometry *pGeom, const geom_world_data *pgwd1,const geom_world_data *pgwd2)
{
	// only the lattice is updated here, not m_pMesh
	static float g_recip[] = { 0,1,0.5f,1.0f/3,0.25f };
	int i,idx,state0,state1,ivtx0=-1,itet0=-1;
	float frac,rfrac, rscale1=pgwd1->scale==1.0f?1.0f:1.0f/pgwd1->scale, rscale2=pgwd2->scale==1.0f?1.0f:1.0f/pgwd2->scale;
	box bboxLoc;
	Vec3 pos,sz,center,n;
	Vec3i ic,iBBox[2];
	Matrix33 R;
	STetrahedron *ptet;

	pGeom->GetBBox(&bboxLoc);
	bboxLoc.Basis *= pgwd2->R.T()*pgwd1->R;	R = bboxLoc.Basis*m_RGrid;
	bboxLoc.center = ((pgwd2->offset + pgwd2->R*bboxLoc.center*pgwd2->scale - pgwd1->offset)*pgwd1->R)*rscale1;
	bboxLoc.size *= pgwd2->scale*rscale1;
	sz = bboxLoc.size*R.Fabs();
	pos = (bboxLoc.center-m_posGrid)*m_RGrid;
	for(i=0;i<3;i++) {
		iBBox[0][i] = min(m_szGrid[i], max(0, float2int((pos[i]-sz[i])*m_rstepGrid[i]-0.5f)));
		iBBox[1][i] = min(m_szGrid[i], max(0, float2int((pos[i]+sz[i])*m_rstepGrid[i]+0.5f)));
	}
	R = pgwd2->R.T()*pgwd1->R*(pgwd1->scale*rscale2);
	pos = ((pgwd1->offset-pgwd2->offset)*rscale2)*pgwd2->R;
	center = ((pgwd2->R*pGeom->GetCenter()*pgwd2->scale+pgwd2->offset-pgwd1->offset)*pgwd1->R)*rscale1;

	for(ic.x=iBBox[0].x;ic.x<iBBox[1].x;ic.x++) for(ic.y=iBBox[0].y;ic.y<iBBox[1].y;ic.y++) for(ic.z=iBBox[0].z;ic.z<iBBox[1].z;ic.z++)
		for(idx=m_pGridTet0[ic*m_strideGrid]; idx<m_pGridTet0[ic*m_strideGrid+1]; idx++) 
		if (!((ptet=m_pTetr+m_pGrid[idx])->flags & (ltet_removed|ltet_processed))) {
			for(ptet=m_pTetr+m_pGrid[idx],i=0,state0=15; i<4; i++)
				state0 ^= (m_pVtxFlags[ptet->ivtx[i]] & lvtx_removed)<<i;

			for(i=0,state1=state0,frac=-1; i<4; i++) {
				if (!(m_pVtxFlags[ptet->ivtx[i]] & (lvtx_removed | lvtx_processed))) { // check if the vertex is inside pGeom
					sz = bboxLoc.Basis*(m_pVtx[ptet->ivtx[i]]-bboxLoc.center);
					if (max(max(fabs_tpl(sz.x)-bboxLoc.size.x,fabs_tpl(sz.y)-bboxLoc.size.y),fabs_tpl(sz.z)-bboxLoc.size.z)<0)
						m_pVtxFlags[ptet->ivtx[i]] |= lvtx_removed_new & -pGeom->PointInsideStatus(R*m_pVtx[ptet->ivtx[i]]+pos);
					m_pVtxFlags[ptet->ivtx[i]] = m_pVtxFlags[ptet->ivtx[i]] & ~(-1<<lvtx_inext_log2) | ivtx0<<lvtx_inext_log2 | lvtx_processed;
					ivtx0 = ptet->ivtx[i]; // maintain a linked list of processed vertices
				}
				state1 ^= (m_pVtxFlags[ptet->ivtx[i]]>>1 & lvtx_removed)<<i;
				n = (m_pVtx[ptet->ivtx[i+2&3]]-m_pVtx[ptet->ivtx[i+1&3]] ^ m_pVtx[ptet->ivtx[i+3&3]]-m_pVtx[ptet->ivtx[i+1&3]])*((i*2&2)-1);
				frac = max(frac, n*(center-m_pVtx[ptet->ivtx[i+1&3]]));
			}

			if (frac<=0) { // weaken the tetrahedron if pGeom's center is inside it
				frac = 1.0f-min(0.9f,max(0.1f,pGeom->GetVolume()*cube(pgwd2->scale*rscale1)*0.7f*ptet->Vinv)); rfrac = 1.0f/frac;
				ptet->M *= frac; ptet->Minv *= rfrac; ptet->Vinv *= rfrac; ptet->Iinv *= rfrac;
				for(i=0;i<4;i++) {
					ptet->fracFace[i] *= frac;
					if (ptet->ibuddy[i]>=0)
						m_pTetr[ptet->ibuddy[i]].fracFace[GetFaceByBuddy(ptet->ibuddy[i],m_pGrid[idx])] *= frac;
				}
			}

			if (state0!=state1) for(i=0;i<4;i++) // weaken each face by the number of removed vertices 2-66%, 1-33%, 0-0%
				ptet->fracFace[i] *= bitcount(state1 & ~(1<<i))*g_recip[bitcount(state0 & ~(1<<i))];
			if (state1==0) { // if all 4 vertices are removed, remove the entire tetrahedron
				ptet->flags |= ltet_removed;
				for(i=0;i<4;i++) if (ptet->ibuddy[i]>=0)
					m_pTetr[ptet->ibuddy[i]].ibuddy[GetFaceByBuddy(ptet->ibuddy[i],m_pGrid[idx])] = -1;
				m_nRemovedTets++;
			}
			ptet->flags = ptet->flags & ~(-1<<ltet_inext_log2) | itet0<<ltet_inext_log2 | ltet_processed; // link processed tets into a list
			itet0 = m_pGrid[idx];	
		}

	// reset processed flag and set removed<-removed_new for the processed vertices
	for(; ivtx0!=-1; ivtx0=m_pVtxFlags[ivtx0]>>lvtx_inext_log2)
		m_pVtxFlags[ivtx0] = m_pVtxFlags[ivtx0] & ~(lvtx_processed | lvtx_removed_new) | (m_pVtxFlags[ivtx0] & lvtx_removed_new)>>1;
	for(; itet0!=-1; itet0=m_pTetr[itet0].flags>>ltet_inext_log2)
		m_pTetr[itet0].flags &= ~ltet_processed;
}


int CTetrLattice::CheckStructure(float time_interval,const Vec3 &gravity, const plane *pGround,int nPlanes, pe_explosion *pexpl, 
																 int maxIters,int bLogTension)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int i,j,nTets=0,nFaces=0,iter;
	Vec3 pt,dw0,dw1,n;
	real pAp,a,b,r2=0,r2new;
	float e,vmax,t2=sqr(time_interval),area2,Pn,Ln,nlen2;
	Matrix33 rmtx;
	quotientf tens,tensMax;
	SCGTetr *pTet0,*pTet1;
	e = gravity.len()*0.05f;

	for(i=0;i<m_nTetr;i++) {
		if (!(m_pTetr[i].flags & ltet_removed)) {
			if (nTets==g_nTetsAlloc)
				ReallocateList(g_Tets,nTets,g_nTetsAlloc+=32);
			for(j=0,vmax=e;j<4;j++)	for(iter=0;iter<nPlanes;iter++)
				vmax = min(vmax, pGround[iter].n*(m_pVtx[m_pTetr[i].ivtx[j]]-pGround[iter].origin));
			if (vmax>=0) {
				g_Tets[nTets].Minv = m_pTetr[i].Minv;
				g_Tets[nTets].Iinv = m_pTetr[i].Iinv;
				g_Tets[nTets].dP = gravity;
				if (pexpl) { // add velocity from explosion
					n = GetTetrCenter(i)-pexpl->epicenter; nlen2 = n.len();
					g_Tets[nTets].dP += n*(pexpl->impulsivePressureAtR*sqr(pexpl->r)/(max(1E-5f,nlen2)*sqr(max(pexpl->rmin,nlen2)))*m_pTetr[i].area*0.3f);
				}
			} else {
				g_Tets[nTets].Minv = 0;
				g_Tets[nTets].Iinv.SetZero();
				g_Tets[nTets].dP.zero();
			}
			g_Tets[nTets].dL.zero();
			m_pTetr[i].idx = nTets++;
		}
		for(j=0;j<4;j++) m_pTetr[i].idxface[j] = -1;
	}
	for(i=0,vmax=0;i<m_nTetr;i++) if (!(m_pTetr[i].flags & ltet_removed)) for(j=0;j<4;j++) 
		if (m_pTetr[i].ibuddy[j]>i && !(m_pTetr[m_pTetr[i].ibuddy[j]].flags & ltet_removed) && 
				max(g_Tets[m_pTetr[i].idx].Minv,g_Tets[m_pTetr[m_pTetr[i].ibuddy[j]].idx].Minv)>0 && m_pTetr[i].fracFace[j]>0) 
		{ if (nFaces==g_nFacesAlloc)
				ReallocateList(g_Faces,nFaces,g_nFacesAlloc+=32);
			g_Faces[nFaces].itet=i; g_Faces[nFaces].iface=j;
			m_pTetr[i].idxface[j] = m_pTetr[m_pTetr[i].ibuddy[j]].idxface[GetFaceByBuddy(m_pTetr[i].ibuddy[j],i)] = nFaces;
			pTet0=g_Faces[nFaces].pTet[0] = g_Tets+m_pTetr[i].idx; 
			pTet1=g_Faces[nFaces].pTet[1] = g_Tets+m_pTetr[m_pTetr[i].ibuddy[j]].idx;
			pt = (m_pVtx[m_pTetr[i].ivtx[j+1&3]]+m_pVtx[m_pTetr[i].ivtx[j+2&3]]+m_pVtx[m_pTetr[i].ivtx[j+3&3]])*(1.0f/3);
			g_Faces[nFaces].r0 = pt-GetTetrCenter(i); 
			g_Faces[nFaces].r1 = pt-GetTetrCenter(m_pTetr[i].ibuddy[j]);
			Pn = pTet0->Minv+pTet1->Minv;	g_Faces[nFaces].vKinv.SetZero();
			g_Faces[nFaces].vKinv(0,0)=Pn; g_Faces[nFaces].vKinv(1,1)=Pn; g_Faces[nFaces].vKinv(2,2)=Pn;
			crossproduct_matrix(g_Faces[nFaces].r0,rmtx); g_Faces[nFaces].vKinv -= rmtx*pTet0->Iinv*rmtx;
			crossproduct_matrix(g_Faces[nFaces].r1,rmtx); g_Faces[nFaces].vKinv -= rmtx*pTet1->Iinv*rmtx;
			g_Faces[nFaces].vKinv.Invert();
			(g_Faces[nFaces].wKinv = pTet0->Iinv+pTet1->Iinv).Invert();
			g_Faces[nFaces].rv = pTet1->dP+(pTet1->dL^g_Faces[nFaces].r1) - pTet0->dP-(pTet0->dL^g_Faces[nFaces].r0);
			g_Faces[nFaces].rw = g_Faces[nFaces].pTet[1]->dL-g_Faces[nFaces].pTet[0]->dL;
			g_Faces[nFaces].dP = g_Faces[nFaces].vKinv*g_Faces[nFaces].rv; 
			g_Faces[nFaces].dL = g_Faces[nFaces].wKinv*g_Faces[nFaces].rw;
			r2 += g_Faces[nFaces].dP*g_Faces[nFaces].rv + g_Faces[nFaces].dL*g_Faces[nFaces].rw;
			vmax = max(vmax,max(g_Faces[nFaces].rv.len2(),g_Faces[nFaces].rw.len2()*g_Faces[nFaces].r0.len2()));
			g_Faces[nFaces].P.zero(); g_Faces[nFaces].L.zero();
			g_Faces[nFaces++].flags = 0;
		}
	iter = min(maxIters/max(1,nFaces), nFaces*6);

	if (vmax>sqr(e)) do {
		for(i=0;i<nTets;i++) { g_Tets[i].dP.zero(); g_Tets[i].dL.zero(); }
		for(i=0;i<nFaces;i++) { 
			g_Faces[i].pTet[0]->dP += g_Faces[i].dP; g_Faces[i].pTet[0]->dL += (g_Faces[i].r0^g_Faces[i].dP) + g_Faces[i].dL;
			g_Faces[i].pTet[1]->dP -= g_Faces[i].dP; g_Faces[i].pTet[1]->dL -= (g_Faces[i].r1^g_Faces[i].dP) + g_Faces[i].dL;
		}
		for(i=0;i<nFaces;i++) {
			pTet0 = g_Faces[i].pTet[0]; pTet1 = g_Faces[i].pTet[1];
			dw0 = pTet0->Iinv*pTet0->dL; dw1 = pTet1->Iinv*pTet1->dL;
			g_Faces[i].dw = dw0-dw1;
			g_Faces[i].dv  = pTet0->dP*pTet0->Minv + (dw0 ^ g_Faces[i].r0);
			g_Faces[i].dv -= pTet1->dP*pTet1->Minv + (dw1 ^ g_Faces[i].r1);
		}

		pAp = 0;
		for(i=0;i<nFaces;i++)
			pAp += g_Faces[i].dw*g_Faces[i].dL + g_Faces[i].dv*g_Faces[i].dP;

		a = min((real)50.0,r2/max((real)1E-10,pAp)); r2new=0;
		for(i=0;i<nFaces;i++) {
			g_Faces[i].dv = g_Faces[i].vKinv*(g_Faces[i].rv -= g_Faces[i].dv*a);
			g_Faces[i].dw = g_Faces[i].wKinv*(g_Faces[i].rw -= g_Faces[i].dw*a);
			r2new += g_Faces[i].dv*g_Faces[i].rv + g_Faces[i].dw*g_Faces[i].rw;
			g_Faces[i].P += g_Faces[i].dP*a; g_Faces[i].L += g_Faces[i].dL*a;
		}
		b = r2new/r2; r2=r2new;	
		for(i=0,vmax=0; i<nFaces; i++) {
			(g_Faces[i].dP*=b) += g_Faces[i].dv;
			(g_Faces[i].dL*=b) += g_Faces[i].dw;
			vmax = max(vmax,max(g_Faces[i].rv.len2(),g_Faces[i].rw.len2()*g_Faces[i].r0.len2()));
		}
	} while(--iter && vmax>sqr(e));

	for(j=0,i=-1,tensMax.set(1-bLogTension,1); j<nFaces; j++) {
		n = (m_pVtx[m_pTetr[g_Faces[j].itet].ivtx[g_Faces[j].iface+2&3]]-m_pVtx[m_pTetr[g_Faces[j].itet].ivtx[g_Faces[j].iface+1&3]] ^
				 m_pVtx[m_pTetr[g_Faces[j].itet].ivtx[g_Faces[j].iface+3&3]]-m_pVtx[m_pTetr[g_Faces[j].itet].ivtx[g_Faces[j].iface+1&3]])*
				 ((g_Faces[j].iface*2&2)-1);
		Pn = g_Faces[j].P*n; Ln = g_Faces[j].L*n;	area2 = sqr(m_pTetr[g_Faces[j].itet].fracFace[g_Faces[j].iface])*(nlen2=n.len2())*t2;
		tens = max(max(max(max(
			quotientf(sqr_signed(Pn), area2*sqr(m_maxForcePull)), 
			quotientf(sqr_signed(-Pn), area2*sqr(m_maxForcePush))), 
			quotientf((g_Faces[j].P*nlen2-n*Pn).len2(), area2*nlen2*sqr(m_maxForceShift))), 
			quotientf(sqr(Ln), area2*sqr(m_maxTorqueTwist))),
			quotientf((g_Faces[j].L*nlen2-n*Ln).len2(), area2*nlen2*sqr(m_maxTorqueBend)));
		if (tens > tensMax) {
			tensMax = tens; i = j; pt = n;
		}
	}
	for(j=0;j<m_nTetr;j++)
		m_pTetr[j].idx = -1;
	if (bLogTension && i>=0) {
		n = pt; Pn = g_Faces[i].P*n; Ln = g_Faces[i].L*n;	area2 = sqr(m_pTetr[g_Faces[i].itet].fracFace[g_Faces[i].iface])*(nlen2=n.len2())*t2;
		if (quotientf(sqr_signed(Pn), area2*sqr(m_maxForcePull))>=tensMax) {
			m_maxTension = Pn/sqrt_tpl(area2); m_imaxTension = LPull;
		} else if (quotientf(sqr_signed(-Pn), area2*sqr(m_maxForcePush))>=tensMax) {
			m_maxTension = -Pn/sqrt_tpl(area2); m_imaxTension = LPush;
		} else if (quotientf((g_Faces[i].P*nlen2-n*Pn).len2(), area2*nlen2*sqr(m_maxForceShift))>=tensMax) {
			m_maxTension = (g_Faces[i].P*nlen2-n*Pn).len()/sqrt_tpl(area2*nlen2); m_imaxTension = LShift;
		} else if (quotientf(sqr(Ln), area2*sqr(m_maxTorqueTwist))>=tensMax) {
			m_maxTension = Ln/sqrt_tpl(area2); m_imaxTension = LTwist;
		} else {
			m_maxTension = (g_Faces[i].L*nlen2-n*Ln).len()/sqrt_tpl(area2*nlen2); m_imaxTension = LBend;
		}
		if (tensMax<1)
			i = -1;
	}
	if (i<0)
		return 0;

	int itet,itet0,itet1,iface,iface0,iface1,idxsum,idxedge,idxvtx,idxface,ihead,itail,nCracks=0,tqueue[32],fqueue[32];
	geom_world_data gwd[2];
	Vec3 n0,vtx[3];
	IGeometry *pCrack;
	g_Faces[i].flags |= lface_processed;
	tqueue[0] = g_Faces[i].itet; fqueue[0] = g_Faces[i].iface; 
	ihead = -1; itail = 0;

	do {
		// get a face from the queue and generate a crack for it
		ihead = ihead+1 & CRY_ARRAY_COUNT(tqueue)-1;
		itet0 = tqueue[ihead]; iface0 = fqueue[ihead];
		itet1 = m_pTetr[itet0].ibuddy[iface0];

		for(j=0;j<3;j++) vtx[j] = m_pVtx[m_pTetr[itet0].ivtx[iface0+1+j&3]];
		if (pCrack = m_pWorld->GetGeomManager()->GetCrackGeom(vtx,m_idmat,gwd+1)) {
#ifdef _DEBUG
			for(j=0; j<3 && pCrack->PointInsideStatus(((vtx[j]-gwd[1].offset)*gwd[1].R)/gwd[1].scale); j++);
			static CTriMesh *g_pPrevMesh=0;
			static Vec3 g_vtx[3];
			static int g_bRepeatSubtract = 0;
			if (g_bRepeatSubtract) {
				m_pMesh = g_pPrevMesh; pCrack->Release();
				pCrack = m_pWorld->GetGeomManager()->GetCrackGeom(g_vtx,m_idmat,gwd+1);
			} else {
				if (g_pPrevMesh) g_pPrevMesh->Release();
				for(j=0;j<3;j++) g_vtx[j] = vtx[j];
			}
			(g_pPrevMesh = new CTriMesh())->Clone(m_pMesh,0);
			g_pPrevMesh->RebuildBVTree(m_pMesh->m_pTree);
#endif
			if (m_pMesh->Subtract(pCrack, gwd,gwd+1)) {
				//Subtract(pCrack, gwd,gwd+1);
				m_pTetr[itet0].fracFace[iface0] = 0; itet1 = m_pTetr[itet0].ibuddy[iface0];
				m_pTetr[itet1].fracFace[GetFaceByBuddy(itet1,itet0)] = 0;
			}	else {
				m_pTetr[itet0].fracFace[iface0] *= 1.5f; 
				m_pTetr[itet1].fracFace[GetFaceByBuddy(m_pTetr[itet0].ibuddy[iface0],itet0)] *= 1.5f;
				continue;
			}
			pCrack->Release();
		}	else 
			continue;

		n0 = (vtx[1]-vtx[0] ^ vtx[2]-vtx[0])*((iface0*2&2)-1);
		for(j=idxsum=0;j<3;j++) idxsum += m_pTetr[itet0].ivtx[iface0+1+j&3];
		// do not quit now if the crack limit is reached, for we still want to weaken the neighbourhood

		for(j=0;j<3;j++) { // trace fins around the face's edges
			itet = itet0; iface = iface0+1+j&3;
			idxedge = idxsum-m_pTetr[itet0].ivtx[iface];
			do {
				itet1 = itet; iface1 = iface;
				if ((itet = m_pTetr[itet].ibuddy[iface])<0 || m_pTetr[itet].flags & ltet_processed)
					break;
				iface = GetFaceByBuddy(itet,itet1);
				idxface = m_pTetr[itet].idxface[iface];
				if (idxface<0 || g_Faces[idxface].flags & lface_processed)
					break;
				g_Faces[idxface].flags |= lface_processed;
				n = (m_pVtx[m_pTetr[itet].ivtx[iface+2&3]]-m_pVtx[m_pTetr[itet].ivtx[iface+1&3]] ^ 
						 m_pVtx[m_pTetr[itet].ivtx[iface+3&3]]-m_pVtx[m_pTetr[itet].ivtx[iface+1&3]])*((iface*2&2)-1);
				if (sqr_signed(n*n0) > sqr(0.75f)*n.len2()*n0.len2()) {
					m_pTetr[itet].fracFace[iface] *= m_crackWeaken;
					m_pTetr[itet1].fracFace[iface1] *= m_crackWeaken;
					Pn = g_Faces[idxface].P*n; Ln = g_Faces[idxface].L*n; area2 = sqr(m_pTetr[itet].fracFace[iface]);
					if (sqr_signed(Pn)*t2 > area2*sqr(m_maxForcePull) || 
							sqr_signed(-Pn)*t2 > area2*sqr(m_maxForcePush) ||
							(g_Faces[idxface].P-n*Pn).len2()*t2 > area2*sqr(m_maxForcePush) ||
							sqr(Ln)*t2 > area2*sqr(m_maxTorqueTwist) || 
							(g_Faces[idxface].L-n*Ln).len2()*t2 > area2*sqr(m_maxTorqueBend))
					{	// put the to-be-cracked face into the queue
						itail = itail+1 & CRY_ARRAY_COUNT(tqueue)-1;
						tqueue[itail]=itet; fqueue[itail]=iface;
						break;
					}
				}
				idxvtx = m_pTetr[itet].ivtx[iface+1&3] + m_pTetr[itet].ivtx[iface+2&3] + m_pTetr[itet].ivtx[iface+3&3] - idxedge;
				idxvtx = iszero(idxvtx-m_pTetr[itet].ivtx[iface+2&3]) + (2&-iszero(idxvtx-m_pTetr[itet].ivtx[iface+3&3]));
				iface = iface+1+idxvtx & 3;
			} while(true);
		}
	}	while(ihead!=itail && ++nCracks<m_nMaxCracks);

	return 1;
}


void CTetrLattice::Split(CTriMesh **pChunks,int nChunks, CTetrLattice **pLattices)
{
	int ichunk,i,j,ibuddy,idx,idx1,nNewTet,nNewVtx,ivtx0,itet0,itet,nCells;
	Vec3 pos,sz;
	box bboxLoc;
	Matrix33 R;
	Vec3i iBBox[2],ic;
	STetrahedron *ptet;
	CTetrLattice *plat;
	if (!m_pVtxRemap) m_pVtxRemap = new int[m_nVtx];

	for(ichunk=0; ichunk<nChunks; ichunk++) {
		pChunks[ichunk]->GetBBox(&bboxLoc);
		R = bboxLoc.Basis*m_RGrid;
		sz = bboxLoc.size*R.Fabs();
		pos = (bboxLoc.center-m_posGrid)*m_RGrid;
		for(i=0;i<3;i++) {
			iBBox[0][i] = min(m_szGrid[i], max(0, float2int((pos[i]-sz[i])*m_rstepGrid[i]-0.5f)));
			iBBox[1][i] = min(m_szGrid[i], max(0, float2int((pos[i]+sz[i])*m_rstepGrid[i]+0.5f)));
		}
		nNewVtx=nNewTet = 0; itet0=ivtx0 = -1;

		for(ic.x=iBBox[0].x;ic.x<iBBox[1].x;ic.x++) for(ic.y=iBBox[0].y;ic.y<iBBox[1].y;ic.y++) for(ic.z=iBBox[0].z;ic.z<iBBox[1].z;ic.z++) 
			for(idx=m_pGridTet0[ic*m_strideGrid]; idx<m_pGridTet0[ic*m_strideGrid+1]; idx++) 
			if (!((ptet=m_pTetr+m_pGrid[idx])->flags & (ltet_removed|ltet_processed))) {
				sz = bboxLoc.Basis*(GetTetrCenter(m_pGrid[idx])-bboxLoc.center);
				if (max(max(fabs_tpl(sz.x)-bboxLoc.size.x,fabs_tpl(sz.y)-bboxLoc.size.y),fabs_tpl(sz.z)-bboxLoc.size.z)<0 &&
						pChunks[ichunk]->PointInsideStatus(GetTetrCenter(m_pGrid[idx])))
				{	// move the tets that have their centers inside the chunk geom to that chunk's lattice
					ptet->idx = nNewTet++; ptet->flags |= ltet_removed_new;
					for(i=0;i<4;i++) if (!(m_pVtxFlags[ptet->ivtx[i]] & lvtx_processed)) {
						m_pVtxFlags[ptet->ivtx[i]] = m_pVtxFlags[ptet->ivtx[i]] & ~(-1<<lvtx_inext_log2) | ivtx0<<lvtx_inext_log2 | lvtx_processed;
						ivtx0 = ptet->ivtx[i]; // maintain a linked list of processed vertices
						m_pVtxRemap[ptet->ivtx[i]] = nNewVtx++;
					}
				}
				ptet->flags = ptet->flags & ~(-1<<ltet_inext_log2) | itet0<<ltet_inext_log2 | ltet_processed; // link processed tets into a list
				itet0 = m_pGrid[idx];	
			}

		if (nNewTet>1 && nNewVtx) {
			pLattices[ichunk] = plat = new CTetrLattice(this,0);
			plat->m_pMesh = pChunks[ichunk];
			plat->m_pVtx = new Vec3[plat->m_nVtx = nNewVtx];
			plat->m_pVtxFlags = new int[plat->m_nVtx];
			plat->m_pTetr = new STetrahedron[plat->m_nTetr = nNewTet];

			for(i=0,idx=ivtx0; idx!=-1; idx=m_pVtxFlags[idx]>>lvtx_inext_log2,i++) {
				plat->m_pVtx[i] = m_pVtx[idx];
				plat->m_pVtxFlags[i] = m_pVtxFlags[idx] & ~lvtx_processed;
			}
			for(i=0,itet=itet0; itet!=-1; itet=m_pTetr[itet].flags>>ltet_inext_log2) if (m_pTetr[itet].flags & ltet_removed_new) {
				plat->m_pTetr[i].flags = 0; plat->m_pTetr[i].idx = -1;
				plat->m_pTetr[i].M = m_pTetr[itet].M; plat->m_pTetr[i].Minv = m_pTetr[itet].Minv;
				plat->m_pTetr[i].Vinv = m_pTetr[itet].Vinv; plat->m_pTetr[i].Iinv = m_pTetr[itet].Iinv;
				plat->m_pTetr[i].area = m_pTetr[itet].area;
				for(j=0;j<4;j++) {
					plat->m_pTetr[i].fracFace[j] = m_pTetr[itet].fracFace[j];
					plat->m_pTetr[i].ivtx[j] = nNewVtx-1-m_pVtxRemap[m_pTetr[itet].ivtx[j]];
					plat->m_pTetr[i].ibuddy[j] = -1;
					if ((ibuddy=m_pTetr[itet].ibuddy[j])>=0)
						if (m_pTetr[ibuddy].idx>=0)
							plat->m_pTetr[i].ibuddy[j] = nNewTet-1-m_pTetr[ibuddy].idx;
						else
							m_pTetr[ibuddy].ibuddy[GetFaceByBuddy(ibuddy,itet)] = -1;
				}	i++;
			}
			if (i!=nNewTet)
				pLattices[ichunk] = 0;
			else {
				plat->m_szGrid = iBBox[1]-iBBox[0];
				plat->m_strideGrid.Set(plat->m_szGrid.z*plat->m_szGrid.y, plat->m_szGrid.z, 1);
				plat->m_posGrid += m_RGrid*Vec3(iBBox[0].x*m_stepGrid.x, iBBox[0].y*m_stepGrid.y, iBBox[0].z*m_stepGrid.z);
				plat->m_pGridTet0 = new int[plat->m_szGrid.GetVolume()+1];
				idx1 = nCells = 0;
				for(ic.x=iBBox[0].x;ic.x<iBBox[1].x;ic.x++) for(ic.y=iBBox[0].y;ic.y<iBBox[1].y;ic.y++) for(ic.z=iBBox[0].z;ic.z<iBBox[1].z;ic.z++) 
					for(idx=m_pGridTet0[ic*m_strideGrid]; idx<m_pGridTet0[ic*m_strideGrid+1]; idx++)
						nCells += m_pTetr[m_pGrid[idx]].flags>>1 & ltet_removed;
				plat->m_pGrid = new int[nCells];
				for(ic.x=iBBox[0].x;ic.x<iBBox[1].x;ic.x++) for(ic.y=iBBox[0].y;ic.y<iBBox[1].y;ic.y++) for(ic.z=iBBox[0].z;ic.z<iBBox[1].z;ic.z++) 
					for(idx=m_pGridTet0[ic*m_strideGrid], plat->m_pGridTet0[(ic-iBBox[0])*plat->m_strideGrid]=idx1; 
							idx<m_pGridTet0[ic*m_strideGrid+1]; idx++) 
						if (m_pTetr[m_pGrid[idx]].flags & ltet_removed_new)
							plat->m_pGrid[idx1++] = nNewTet-1-m_pTetr[m_pGrid[idx]].idx;
				plat->m_pGridTet0[plat->m_szGrid.GetVolume()]=idx1; 
			}
		} else
			pLattices[ichunk] = 0;

		for(; itet0!=-1; itet0=m_pTetr[itet0].flags>>ltet_inext_log2)	{
			m_pTetr[itet0].flags = m_pTetr[itet0].flags & ~(ltet_processed|ltet_removed_new) | m_pTetr[itet0].flags>>1 & ltet_removed;
			m_pTetr[itet0].idx = -1;
		}
		for(; ivtx0!=-1; ivtx0=m_pVtxFlags[ivtx0]>>lvtx_inext_log2)
			m_pVtxFlags[ivtx0] &= ~lvtx_processed;
	}

	Defragment();
}


int CTetrLattice::AddImpulse(const Vec3 &pt, const Vec3 &impulse,const Vec3 &momentum, const Vec3& gravity,float worldTime)
{
	int i,bCheckStructure=0;
	Vec3 ptloc;
	Vec3i ic;
	float vgravity = gravity.len2();
	STetrahedron *ptet;

	if (worldTime!=m_lastImpulseTime) {
		for(i=0;i<m_nTetr;i++)
			m_pTetr[i].Pext.zero(), m_pTetr[i].Lext.zero();
		m_lastImpulseTime = worldTime;
	}

	ptloc = (pt-m_posGrid)*m_RGrid;
	for(i=0;i<3;i++)
		ic[i] = min(m_szGrid[i]-1, max(0, float2int(ptloc[i]*m_rstepGrid[i]-0.5f)));

	for(i=m_pGridTet0[ic*m_strideGrid]; i<m_pGridTet0[ic*m_strideGrid+1]; i++) 
		if (!((ptet=m_pTetr+m_pGrid[i])->flags & ltet_removed)) {
			ptet->Pext += impulse;
			ptet->Lext += momentum+(pt-GetTetrCenter(m_pGrid[i]) ^ impulse);
			bCheckStructure |= isneg(vgravity - max((ptet->Pext*ptet->Minv).len2(), (ptet->Iinv*ptet->Lext).len2()*ptet->area));
		}

	return bCheckStructure;
}


int CTetrLattice::Defragment()
{
	// removes tetrahedra if m_nRemovedTets > 70% m_nTets (+remap buddy list)
	// don't physically remove vertices - they are cheap, there's no loop over all vertices, but it would require remapping
	if (m_nRemovedTets*10 > m_nTetr*7) {
		int i,j,nTetr0=m_nTetr,ngrid,ngrid0;
		for(i=ngrid=0;i<m_szGrid.GetVolume();i++) {
			for(j=m_pGridTet0[i],ngrid0=ngrid; j<m_pGridTet0[i+1]; j++)
				if (!(m_pTetr[m_pGrid[j]].flags & ltet_removed))
					m_pGrid[ngrid++] = m_pGrid[j];
			m_pGridTet0[i] = ngrid0;
		}
		m_pGridTet0[i] = ngrid;

		for(i=j=0;i<m_nTetr;i++) if (!(m_pTetr[i].flags & ltet_removed))
			m_pTetr[i].idx = j++;
		for(i=0;i<m_nTetr;i++) for(j=0;j<4;j++) if (m_pTetr[i].ibuddy[j]>=0)
			m_pTetr[i].ibuddy[j] = m_pTetr[m_pTetr[i].ibuddy[j]].idx;
		for(i=m_pGridTet0[m_szGrid.GetVolume()]-1;i>=0;i--)
			m_pGrid[i] = m_pTetr[m_pGrid[i]].idx;
		for(i=j=0;i<m_nTetr;i++) if (!(m_pTetr[i].flags & ltet_removed)) {
			if (i!=j) m_pTetr[j] = m_pTetr[i]; j++;
		}
		m_nTetr = j;
		for(i=0;i<m_nTetr;i++)
			m_pTetr[i].idx = -1;

		ReallocateList(m_pTetr,nTetr0,m_nTetr);
		m_nRemovedTets = 0;
		return 1;
	}
	return 0;
}


void CTetrLattice::DrawWireframe(IPhysRenderer *pRenderer, geom_world_data *gwd, int idxColor)
{
	Vec3 pt[5]; int i,j,i0,i1,i2;

	for(i=0;i<m_nTetr;i++) if (!(m_pTetr[i].flags & ltet_removed)) {
		for(j=0;j<4;j++)
			pt[j] = gwd->offset+gwd->R*m_pVtx[m_pTetr[i].ivtx[j]]*gwd->scale;
		for(j=0;j<6;j++) {
			i2=j>>2; i0=(j>>1)+i2; i1=i0+1+((j&1)<<i2)&3;
			pRenderer->DrawLine(pt[i0],pt[i1],idxColor);
		}
		for(j=0;j<4;j++) if (m_pTetr[i].fracFace[j]<=0) {
			pt[4] = (pt[j+1&3]+pt[j+2&3]+pt[j+3&3])*(1.0f/3);
			for(i0=1;i0<4;i0++)
				pRenderer->DrawLine(pt[j+i0&3],pt[4]*0.85f+pt[j+i0&3]*0.15f,idxColor);
		}
	}
}


IGeometry *CTetrLattice::CreateSkinMesh(int nMaxTrisPerBVNode)
{
	int i,j,itri;
	index_t *pIdx=new index_t[m_nTetr*12];
	for(i=itri=0;i<m_nTetr;i++) for(j=0;j<4;j++) if (m_pTetr[i].ibuddy[j]<0)
		itri += GetFaceIdx(i,j, pIdx+itri);

	IGeometry *pGeom = m_pWorld->GetGeomManager()->CreateMesh(m_pVtx,pIdx,0,0,itri/3,
		mesh_AABB|mesh_shared_vtx|mesh_shared_idx|mesh_multicontact2,0.0f,2,nMaxTrisPerBVNode);
	((CTriMesh*)pGeom)->m_flags &= ~mesh_shared_idx;
	((CTriMesh*)pGeom)->m_nVertices = m_nVtx;
	return pGeom;
}


int CTetrLattice::CheckPoint(const Vec3 &pt, int *idx, float *w)
{
	int i,j;
	if (!m_pGrid) {
		Vec3 BBox[2] = { m_pVtx[0],m_pVtx[0] };
		box bbox;
		for(i=1;i<m_nVtx;i++) {
			BBox[0]=min(BBox[0],m_pVtx[i]); BBox[1]=max(BBox[1],m_pVtx[i]);
		}
		bbox.Basis.SetIdentity();	bbox.bOriented=0;
		bbox.center = (BBox[0]+BBox[1])*0.5f;
		bbox.size = (BBox[1]-BBox[0])*0.501f;
		SetGrid(bbox);
	}

	Vec3 ptloc = (pt-m_posGrid)*m_RGrid;
	Vec3i ic;
	STetrahedron *ptet;

	for(i=0;i<3;i++)
		ic[i] = min(m_szGrid[i]-1, max(0, float2int(ptloc[i]*m_rstepGrid[i]-0.5f)));

	for(i=m_pGridTet0[ic*m_strideGrid]; i<m_pGridTet0[ic*m_strideGrid+1]; i++) 
		if (!((ptet=m_pTetr+m_pGrid[i])->flags & ltet_removed)) {
			for(j=0;j<4;j++) {
				int ivtx[3]; GetFaceIdx(m_pGrid[i],j, ivtx);
				w[j] = ((m_pVtx[ivtx[2]]-m_pVtx[ivtx[0]] ^ m_pVtx[ivtx[1]]-m_pVtx[ivtx[0]])*
							 (pt-m_pVtx[ivtx[0]]))*(1.0f/6)*ptet->Vinv;
				idx[j] = ptet->ivtx[j];
			}
			if (min(min(min(w[0],w[1]),w[2]),w[3])>0.0f)
				return m_pGrid[i]+1;
		}	
	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// CBreakableGrid2d ////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////


/*struct jgrid_checker {
	CBreakableGrid2d *pgrd;
	Vec2 org,dir,dirn;
	int iedge[2],iedgeExit0;
	int iprevcell;

	int check_cell(const Vec2i &icell, int &ilastcell) {
		int i,idx,icurcell,mask=2,bhit;
		Vec2 c,step=pgrd->m_coord.step,pt,pt0(pgrd->m_coord.origin.x,pgrd->m_coord.origin.y);

		idx = icell*pgrd->m_coord.stride;
		pgrd->m_pCellDiv[idx] |= 2;	// mark the cell as used
		c.set(step.x*(icell.x+0.5f), step.y*(icell.y+0.5f));
		icurcell = icell.y<<16|icell.x;

		if ((icurcell|iprevcell>>31)!=iprevcell) { // false if this is the first edge cell
			if (iedge[0]!=(iedge[1]|iedge[0]>>31)) for(i=iedge[1]+1&3; i!=iedge[0]; i=i+1&3)
				mask |= 4<<i;
			pgrd->m_pCellDiv[Vec2i(iprevcell&0xFFFF,iprevcell>>16)*pgrd->m_coord.stride] &= mask;
			for(i=0;i<2;i++) { // compute the enter point
				bhit = -inrange((dirn^org-c)*(1-i*2)-fabs_tpl(dirn[i^1])*step[i], -dirn[i]*step[i^1]*1.001f,dirn[i]*step[i^1]*1.001f);
				(iedge[0] &= ~bhit) |= (i^1 | (i^isnonneg(dirn[i]))<<1) & bhit;
			}
			if (icurcell!=ilastcell) {
				for(pt=org+dirn*(dirn*(c-org)),i=0; (fabs_tpl(pt.x-c.x)>step.x*0.5f || fabs_tpl(pt.y-c.y)>step.y*0.5f) && i<4; i++)
					pt = org+dirn*(dirn*(c+Vec2(step.x*(0.5f-(i&1)),0)+Vec2(0,step.y*(0.5f-(i>>1)))-org));
				pgrd->m_pt[idx] = pt+pt0;
			}
		}	else
			pgrd->m_pt[idx] = org+pt0;
		if (icurcell!=ilastcell)	{	// false if this is the last edge cell
			for(i=0;i<2;i++) { // compute the leave point
				bhit = -inrange((dirn^org-c)*(1-i*2)+fabs_tpl(dirn[i^1])*step[i], -dirn[i]*step[i^1]*1.001f,dirn[i]*step[i^1]*1.001f);
				(iedge[1] &= ~bhit) |= (i^1 | (i^isneg(dirn[i]))<<1) & bhit;
			}
			iedgeExit0 += iedge[1]+1 & iedgeExit0>>31;
		}	else
			pgrd->m_pt[idx] = org+dir+pt0;

		iprevcell = icurcell;
		return 0;
	}
};*/


int CBreakableGrid2d::get_neighb(int iTri,int iEdge) 
{ 
	static int offsx[8] = { -1, 0, 1, 0, 0, 1, 0,-1 };
	static int offsy[8] = {  0,-1, 0, 1,-1, 0, 1, 0 };
	static int buddytri[16] = { 1,1,0,0,1,0,0,1, 0,1,1,0,1,1,0,0 };

	if (iEdge==2) 
		return iTri^1;
	int i = m_pCellDiv[iTri>>1]<<2 | (iTri&1)<<1 | iEdge&1;
	int iBuddyCell = (iTri>>1)+offsx[i]+offsy[i]*m_coord.size.x;
	return iBuddyCell*2 + buddytri[m_pCellDiv[iBuddyCell]<<3 | i];
}

void CBreakableGrid2d::get_edge_ends(int iTri,int iEdge, int &iend0,int &iend1)
{
	int i = 3+m_pCellDiv[iTri>>1]+(iTri&1)*2+iEdge & 3;
	iend0 = (iTri>>1) + (i>>1)*m_coord.size.x + (i&1 ^ i>>1);
	i = 3+m_pCellDiv[iTri>>1]+(iTri&1)*2+inc_mod3[iEdge] & 3;
	iend1 = (iTri>>1) + (i>>1)*m_coord.size.x + (i&1 ^ i>>1);
}


void CBreakableGrid2d::Generate(Vec2 *ptsrc,int npt, const Vec2i &nCells, int bStaticBorder, int seed)
{
	int i,j,ix,iy,ncells;
	Vec2 ptmin,ptmax,step,v0,v1;
	Vec2i sz;
	jgrid_checker jgc;
	jgc.ppt = 0; jgc.pnorms = 0;
	jgc.bMarkCenters = 0;

	SScopedRandomSeedChange seedChange;

	if (seed != -1)
		seedChange.Seed((unsigned int)seed);

	sz.set(nCells.x+2,nCells.y+3);
	for(i=1,ptmin=ptmax=ptsrc[0]; i<npt; i++) {
		ptmin = min(ptmin,ptsrc[i]); ptmax = max(ptmax,ptsrc[i]);
	}
	//m_coord.step = step.set((ptmax.x-ptmin.x)*(1+0.02f*rncells.x)*rncells.x, (ptmax.y-ptmin.y)*(1+0.02f*rncells.y)*rncells.y);
	m_coord.step = step.set((ptmax.x-ptmin.x)/(nCells.x-0.02f), (ptmax.y-ptmin.y)/(nCells.y-0.02f));
	m_coord.stepr.set(1.0f/step.x, 1.0f/step.y);
	ptmin.x -= step.x*1.51f;//(ptmax.x-ptmin.x)*rncells.x*0.01f+step.x*1.5f;
	ptmin.y -= step.y*1.51f;//(ptmax.y-ptmin.y)*rncells.y*0.01f+step.y*1.5f;

	m_coord.Basis.SetIdentity();
	m_coord.bOriented = 0;
	*(Vec2*)&m_coord.origin.zero() = ptmin;
	m_coord.size = sz;
	m_coord.stride.set(1,sz.x);

	ncells = sz.x*sz.y;
	m_pt = new Vec2[ncells];
	m_pTris = new int[ncells*2];
	memset(m_pCellDiv = new char[ncells], 28, ncells);
	for(i=0;i<sz.x;i++) m_pCellDiv[i]=m_pCellDiv[sz.x*(sz.y-1)+i] = 2;
	for(i=0;i<sz.y;i++) m_pCellDiv[i*sz.x] = 2;
	jgc.pgrid = &m_coord;
	jgc.pCellMask=m_pCellDiv; jgc.ppt=m_pt;
	jgc.pqueue=m_pCellQueue; jgc.szQueue=m_szCellQueue;

	for(ix=0;ix<sz.x;ix++) for(iy=0;iy<sz.y;iy++)	// generate jittered grid points
		m_pt[ix+iy*sz.x].set((ix+cry_random(0.1f, 0.9f))*step.x+ptmin.x, (iy+cry_random(0.1f, 0.9f))*step.y+ptmin.y);

	jgc.iedge[0]=jgc.iedge[1]=jgc.iprevcell=jgc.iedgeExit0 = -1; j = 2;
	for(i=0;i<npt;i++) {
		jgc.org = ptsrc[i]-ptmin; jgc.dirn = norm(jgc.dir = ptsrc[i+1 & i+1-npt>>31]-ptsrc[i]);
		Vec3 origin(jgc.org.x,jgc.org.y,0);
		Vec3 direction(jgc.dir.x,jgc.dir.y,0);
		DrawRayOnGrid(&m_coord, origin,direction, jgc);
	}
	if (jgc.iedge[0]!=jgc.iedgeExit0) for(i=jgc.iedgeExit0+1&3; i!=jgc.iedge[0]; i=i+1&3)
		j |= 4<<i;
	m_pCellDiv[Vec2i(jgc.iprevcell&0xFFFF,jgc.iprevcell>>16)*m_coord.stride] &= j;
	for(i=0;i<ncells;i++) if (m_pCellDiv[i]&2 && m_pCellDiv[i]&28)
		for(j=0;j<4;j++) if (m_pCellDiv[i] & 4<<j && !(m_pCellDiv[ix = i+Vec2i(1-(j&2) & -(j&1), (j&2)-1 & ~-(j&1))*m_coord.stride] & 2))
			jgc.MarkCellInteriorQueue(ix);
	m_pCellQueue=jgc.pqueue; m_szCellQueue=jgc.szQueue;
	for(i=0;i<sz.x;i++) m_pCellDiv[i]=m_pCellDiv[sz.x*(sz.y-1)+i] = 0;
	for(i=0;i<sz.y;i++) m_pCellDiv[i*sz.x] = 0;

	for(i=0; i<ncells-sz.x-1; i++) { 
		j = (m_pCellDiv[i]>>1&1) + (m_pCellDiv[i+1]>>1&1)*2 + (m_pCellDiv[i+sz.x+1]>>1&1)*4 + (m_pCellDiv[i+sz.x]>>1&1)*8;
		if (j==15) {
			m_pCellDiv[i] = cry_random(0,1);	// randomly choose the way the cell is split
			// check is this triangulation creates valid triangles
			v0 = m_pt[i+1+m_pCellDiv[i]*sz.x]; v1 = m_pt[i+(m_pCellDiv[i]^1)*sz.x]-m_pt[i+m_pCellDiv[i]];
			m_pCellDiv[i] ^= isneg(sqr_signed(v0^v1)-sqr(0.2f)*len2(v0)*len2(v1));
			v0 = m_pt[i+(m_pCellDiv[i]^1)*sz.x]-m_pt[i+(m_pCellDiv[i]^1)+sz.x]; v1 = m_pt[i+1+m_pCellDiv[i]*sz.x]-m_pt[i+(m_pCellDiv[i]^1)+sz.x];
			m_pCellDiv[i] ^= isneg(sqr_signed(v0^v1)-sqr(0.2f)*len2(v0)*len2(v1));
			m_pTris[i*2] = m_pTris[i*2+1] = TRI_AVAILABLE;
		} else if (bitcount(j)>=3) {
			ix = 9-(j&3)-(3&-(j>>2&1))-(j>>1&4);
			m_pCellDiv[i] = ix&1;	
			m_pTris[i*2+(ix>>1^1)] = TRI_AVAILABLE; m_pTris[i*2+(ix>>1)] = TRI_FIXED;
		} else {
			m_pCellDiv[i] = 0;
			m_pTris[i*2] = m_pTris[i*2+1] = TRI_FIXED;
		}
	}
	
	// mark borders
	for(ix=0;ix<sz.x;ix++) m_pTris[ix*2]=m_pTris[ix*2+1]=m_pTris[(ix+sz.x*(sz.y-1))*2]=m_pTris[(ix+sz.x*(sz.y-1))*2+1] = TRI_FIXED;
	for(iy=0;iy<sz.y;iy++) m_pTris[iy*sz.x*2]=m_pTris[iy*sz.x*2+1]=m_pTris[(iy*sz.x+sz.x-1)*2]=m_pTris[(iy*sz.x+sz.x-1)*2+1] = TRI_FIXED;
	for(i=m_nTris=0;i<ncells*2;i++) m_nTris -= -(m_pTris[i] & TRI_AVAILABLE)>>31;

	for(i=0;i<ncells;i++) m_pCellDiv[i] &= 1;
	m_nTris0 = m_nTris;
}


inline int CBreakableGrid2d::CropSpikes(int imin,int imax, int *queue,int szQueue, int flags,int flagsNew, float thresh)
{
	int i,iTri,ivtx[3],ihead=0,itail=0,nCropped=0;
	const int flagsMask = TRI_FIXED|flags;

	for(i=imin; i<=imax; i++) if ((m_pTris[i] & flagsMask)==flags && 
			(m_pTris[get_neighb(i,0)]&flagsMask)+(m_pTris[get_neighb(i,1)]&flagsMask)+(m_pTris[get_neighb(i,2)]&flagsMask)==flags)
	{	queue[ihead] = i; ihead = ihead+1 & szQueue-1; }

	if (ihead) do { 
		iTri = queue[itail]; itail = itail+1 & szQueue-1;
		i = -(-(m_pTris[get_neighb(iTri,1)] & flags)>>31) - (-(m_pTris[get_neighb(iTri,2)] & flags)>>31)*2;
		get_edge_ends(iTri,i, ivtx[1],ivtx[2]);
		get_edge_ends(iTri,inc_mod3[i], ivtx[2],ivtx[0]);
		Vec2 v0=m_pt[ivtx[1]]-m_pt[ivtx[0]], v1=m_pt[ivtx[2]]-m_pt[ivtx[0]];
		if ((m_pTris[iTri] & TRI_FIXED) + sqr_signed(v0^v1) < len2(v0)*len2(v1)*sqr(thresh)) {
			m_pTris[iTri] = flagsNew; nCropped++;
			iTri = get_neighb(iTri,i);
			if ((m_pTris[get_neighb(iTri,0)]&flagsMask)+(m_pTris[get_neighb(iTri,1)]&flagsMask)+(m_pTris[get_neighb(iTri,2)]&flagsMask)==flags)
			{	queue[ihead] = iTri; ihead = ihead+1 & szQueue-1; }
		}
	} while (ihead!=itail);

	return nCropped;
}


int *CBreakableGrid2d::BreakIntoChunks(const Vec2 &pt, float r, Vec2 *&ptout, int maxPatchTris,float jointhresh,int seed, 
																			 float filterAng,float ry)
{
	int i,j,nPatches,nSeedTris,nPatchTris,iTri,iTriNew,nCells,ihead,itail,szQueue,iEdge,
			iCurPatch,nVtx,nStaticTris,nUsedTris,ivtx[3],nTris,bStable;
	//int iMotherPatch,bHasInclusions,iPatch;
//	Vec2i sz = m_coord.size;
	Vec2 c,v0,v1;
	int *queue,*pSeedTris,*pPatchSeeds,*pPatchMothers,*pVtx=0;
	ptout = m_pt;
	if (ry<=0.0f)
		ry = r;

	SScopedRandomSeedChange seedChange;
	if (seed!=-1)
		seedChange.Seed((unsigned int)seed);

	nCells = m_coord.size.x*m_coord.size.y;
	for(szQueue=8; szQueue<(nCells>>2); szQueue<<=1);
	queue = new int[szQueue];
	pSeedTris = new int[nCells*2];
	pPatchSeeds = new int[nCells*2];
	pPatchMothers = new int[nCells*2];

	// mark tris that are outside of the shattering region as static
	j = maxPatchTris>0 ? TRI_AVAILABLE : TRI_EMPTY;
	int imin=nCells*2,imax=0;
	for(i=nStaticTris=0; i<nCells*2; i++)	if ((m_pTris[i] & (TRI_EMPTY|TRI_FIXED))==0) {
		get_edge_ends(i,0, ivtx[0],ivtx[1]);
		get_edge_ends(i,1, ivtx[1],ivtx[2]);
		c = (m_pt[ivtx[0]]+m_pt[ivtx[1]]+m_pt[ivtx[2]])*(1.0f/3);
		if (sqr(c.x-pt.x)*sqr(ry)+sqr(c.y-pt.y)*sqr(r)>=sqr(r*ry)) {
			m_pTris[i] = TRI_STABLE; nStaticTris++;
		}	else {
			m_pTris[i] = j;
			imin=min(imin,i); imax=max(imax,i);
		}
	}

	if (filterAng>0) { 
		imin = max(0, imin-m_coord.stride.y*2);
		imax = min(nCells*2-1, imax+m_coord.stride.y*2);
		float cutoff = sin_tpl(filterAng);
		nStaticTris -= CropSpikes(imin,imax, queue,szQueue, TRI_STABLE,TRI_AVAILABLE, cutoff);
		nStaticTris += CropSpikes(imin,imax, queue,szQueue, TRI_AVAILABLE,TRI_STABLE, cutoff);
	}

	// unite grid triangles into patches by randomly growing them 
	nUsedTris = nPatches = 0;
	if (maxPatchTris>0) for(; nUsedTris+nStaticTris<m_nTris; ) {
		for(i=0;i<nCells*2 && !(m_pTris[i] & TRI_AVAILABLE);i++);
		if (i>=nCells*2)
			break; // should never happen though
		m_pTris[pPatchSeeds[nPatches]=queue[0]=i]=nPatches; itail=0; ihead=1; nUsedTris++;
		nSeedTris = 0;

		do {
			nPatchTris = cry_random(0, maxPatchTris - 1);	// request random number of triangles for this patch
			for(i=0;i<3;i++) if (m_pTris[iTri=get_neighb(queue[itail],i)]==TRI_AVAILABLE)
				m_pTris[pSeedTris[nSeedTris++] = iTri] |= TRI_PROCESSED; // immediately queue neighbours as potential subsequent patch seeds
			if (nPatchTris>0) do { // grow iCurPatch around a seed triangle
				iTri = queue[itail]; itail = itail+1 & szQueue-1;
				if (m_pTris[iTri] & TRI_AVAILABLE) {
					if (cry_random(0.0f, 1.0f)>jointhresh) {
						m_pTris[iTri] = nPatches; nPatchTris--;	nUsedTris++;
						pPatchSeeds[nPatches] = iTri;
					} else if (m_pTris[iTri]==TRI_AVAILABLE)
						m_pTris[pSeedTris[nSeedTris++] = iTri] |= TRI_PROCESSED;
				}
				if (m_pTris[iTri]==nPatches)
					for(i=0;i<3;i++) if (m_pTris[j=get_neighb(iTri,i)] & TRI_AVAILABLE) {
						queue[ihead] = j; ihead = ihead+1 & szQueue-1;
					}
			} while(nPatchTris>0 && ihead!=itail);

			pPatchMothers[nPatches++] = -1;
			for(i=j=0;i<nSeedTris;i++) if (m_pTris[pSeedTris[i]] & TRI_AVAILABLE)
				pSeedTris[j++] = pSeedTris[i];
			for(;ihead!=itail;itail = itail+1 & szQueue-1) if (m_pTris[queue[itail]]==TRI_AVAILABLE)
				m_pTris[pSeedTris[j++] = queue[itail]] |= TRI_PROCESSED;
			if ((nSeedTris=j)==0)	// all triangles are assigned to patches
				break;
			m_pTris[pPatchSeeds[nPatches]=queue[0]=pSeedTris[0]]=nPatches; 
			itail=0; ihead=1; nUsedTris++;
		} while(true);
	}

	// find isolated island in the static part and register them as patches
	for(i=0;i<nCells*2;i++) if (m_pTris[i]==TRI_STABLE) {
		m_pTris[pSeedTris[0] = queue[0] = i] |= TRI_PROCESSED; 
		itail=0; ihead=1;	nTris=1; bStable=0;
		do {
			iTri = queue[itail]; itail = itail+1 & szQueue-1;
			for(j=0;j<3;j++) {
				if (m_pTris[iTriNew=get_neighb(iTri,j)]==TRI_STABLE) {
					m_pTris[pSeedTris[nTris++] = queue[ihead] = iTriNew] |= TRI_PROCESSED; ihead = ihead+1 & szQueue-1;
				} bStable |= m_pTris[iTriNew] & TRI_FIXED;
			}
		} while(ihead!=itail);
		if (!bStable)	{
			for(j=0;j<nTris;j++) m_pTris[pSeedTris[j]] = nPatches;
			pPatchSeeds[nPatches] = i; pPatchMothers[nPatches++] = -2;
			nUsedTris += nTris;
		}
	}

	// iteratively chop off thin ears from the patches
	for(iCurPatch=nVtx=0; iCurPatch<nPatches; iCurPatch++) { 
		m_pTris[queue[0]=pPatchSeeds[iCurPatch]] |= TRI_PROCESSED; itail=0; ihead=1;
		do {
			iTri = queue[itail]; itail = itail+1 & szQueue-1;
			if (m_pTris[iTri] != (iCurPatch|TRI_PROCESSED))
				continue;
			for(i=0;i<3;i++) if (m_pTris[j=get_neighb(iTri,i)]==iCurPatch) {
				queue[ihead] = j; ihead = ihead+1 & szQueue-1; 
				m_pTris[j] |= TRI_PROCESSED;
			}
			do {
				for(i=iEdge=0;i<3;i++)
					iEdge |= iszero((m_pTris[get_neighb(iTri,i)] & ~TRI_PROCESSED)-iCurPatch)<<i;
				if (bitcount(iEdge)==1) {
					i = (iEdge&7)-1-(iEdge>>2&1);
					get_edge_ends(iTri,i, ivtx[0],ivtx[1]);
					get_edge_ends(iTri,inc_mod3[i], ivtx[1],ivtx[2]);
					v0 = m_pt[ivtx[0]]-m_pt[ivtx[2]]; v1 = m_pt[ivtx[1]]-m_pt[ivtx[2]];
					if (sqr_signed(v0^v1) < len2(v0)*len2(v1)*sqr(0.5f)) {
						m_pTris[iTri] = TRI_EMPTY;
						iTri = pPatchSeeds[iCurPatch] = get_neighb(iTri,i);
					}	else break;
				} else break;
			}	while(true);
		} while(ihead!=itail);
	}

	// output all generated patches
	for(iCurPatch=nVtx=0; iCurPatch<nPatches; iCurPatch++) { 
		if (m_pTris[pPatchSeeds[iCurPatch]] != (iCurPatch|TRI_PROCESSED))
			continue;
		m_pTris[queue[0]=pPatchSeeds[iCurPatch]] &= ~TRI_PROCESSED; itail=0; ihead=1;
		do { // grow iCurPatch around a seed triangle
			iTri = queue[itail]; itail = itail+1 & szQueue-1;
			if ((nVtx-1&31)+4>=32) ReallocateList(pVtx, nVtx,(nVtx+3&~31)+32);
			get_edge_ends(iTri,0, pVtx[nVtx],pVtx[nVtx+1]); // output the points
			get_edge_ends(iTri,1, pVtx[nVtx+1],pVtx[nVtx+2]); 
			for(i=iEdge=0;i<3;i++) if ((m_pTris[j=get_neighb(iTri,i)] & ~TRI_PROCESSED)==iCurPatch) {
				iEdge |= 1<<i;
				if (m_pTris[j] & TRI_PROCESSED) {
					queue[ihead] = j; ihead = ihead+1 & szQueue-1; 
					m_pTris[j] &= ~TRI_PROCESSED;
				}
			}	else
				iEdge |= iszero(m_pTris[j]-TRI_EMPTY)<<i+3;
			if (iEdge==0) {
				v0 = m_pt[pVtx[nVtx]]-m_pt[pVtx[nVtx+2]]; 
				v1 = m_pt[pVtx[nVtx+1]]-m_pt[pVtx[nVtx+2]];
				if ((v0^v1) < sqr(m_coord.step.x*0.9f)) {
					m_pTris[iTri] = TRI_EMPTY;
					continue;					
				}
			}
			pVtx[nVtx+3] = iEdge;	nVtx += 4; 
		} while(ihead!=itail);
		if (!(nVtx&31)) ReallocateList(pVtx, nVtx,nVtx+32);
		pVtx[nVtx++] = -1;
	}
	m_nTris -= nUsedTris;

	delete[] pPatchMothers; delete[] pPatchSeeds;
	delete[] pSeedTris; delete[] queue;

	// output the remains of this grid
	for(i=0;i<nCells*2;i++) if (m_pTris[i]==(TRI_STABLE|TRI_PROCESSED)) {
		if ((nVtx-1&31)+4>=32) ReallocateList(pVtx, nVtx,(nVtx+3&~31)+32);
		get_edge_ends(i,0, pVtx[nVtx],pVtx[nVtx+1]); // output the points
		get_edge_ends(i,1, pVtx[nVtx+1],pVtx[nVtx+2]); 
		for(j=iEdge=0;j<3;j++) {
			iTri = m_pTris[get_neighb(i,j)];
			iEdge |= iszero(iTri-(TRI_STABLE|TRI_PROCESSED))*65 << j;
			iEdge |= (iszero(iTri & TRI_FIXED)^1) << j+6;
		}
		pVtx[nVtx+3] = iEdge; nVtx += 4;
	}	else 
		m_pTris[i] = max(m_pTris[i],(int)TRI_EMPTY);
	if (!(nVtx&31)) ReallocateList(pVtx, nVtx,nVtx+32);
	pVtx[nVtx++] = -2;

	return pVtx;
}

	// find patches that border upon only one patch (=are contained inside it) and merge them with this enclosing patch
	/*do {
		for(iCurPatch=bHasInclusions=0; iCurPatch<nPatches; iCurPatch++) if (pPatchMothers[iCurPatch]>=-1) { 
			m_pTris[queue[0]=pPatchSeeds[iCurPatch]] |= 1<<30; itail=0; ihead=1; iMotherPatch=-1;

			do {
				iTri = queue[itail]; itail = itail+1 & szQueue-1;
				for(iEdge=0;iEdge<3;iEdge++) {
					iPatch = m_pTris[iTriNew = get_neighb(iTri,iEdge)] & ~(1<<30);
					if (pPatchMothers[iCurPatch]>=0) // assign tri to outer patch, if it exists
						m_pTris[iTri] = pPatchMothers[iCurPatch] | 1<<30;

					if (iPatch!=iCurPatch && iPatch<nCells*2 && pPatchMothers[iCurPatch]<0) {
						pPatchSeeds[iCurPatch] = iTri;
						if (iMotherPatch<0)
							iMotherPatch = iPatch;
						else if (iPatch!=iMotherPatch)
							break; // we have at least 2 distinct neighbours
					}	else if (m_pTris[iTriNew]==iCurPatch) { // check if this patch tri hasn't been processed yet
						queue[ihead] = iTriNew; ihead = ihead+1 & szQueue-1;
						m_pTris[iTriNew] |= 1<<30;
					}
				}
			} while(itail!=ihead && iEdge==3);

			if (pPatchMothers[iCurPatch]>=0) {
				pPatchMothers[pPatchMothers[iCurPatch]] = -1; // 'needs recheck' state
				pPatchMothers[iCurPatch] = -3; // 'always ignore this patch from now on' state
				bHasInclusions = 1;
			} else {
				if (iEdge==3) {
					pPatchMothers[iCurPatch] = iMotherPatch;
					bHasInclusions = 1;
				}	else
					pPatchMothers[iCurPatch] = -2; // 'checked' state
			}
		}

		for(i=nCells*2-1;i>=0;i--) m_pTris[i]&=~(1<<30);
		if (bHasInclusions) do { // merge nested inclusions
			for(i=j=0;i<nPatches;i++)	if (pPatchMothers[i]>=0 && pPatchMothers[pPatchMothers[i]]>=0) {
				pPatchMothers[i] = pPatchMothers[pPatchMothers[i]]; j++;
			}
		} while(j);
	} while(bHasInclusions);*/


