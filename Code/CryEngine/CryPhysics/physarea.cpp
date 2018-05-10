// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "geoman.h"
#include "bvtree.h"
#include "geometry.h"
#include "overlapchecks.h"
#include "raybv.h"
#include "raygeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "physicalworld.h"
#include "trimesh.h"
#include "heightfieldgeom.h"
#include "waterman.h"

extern IGeometry *PrepGeomExt(IGeometry *pGeom);

#include <CryNetwork/ISerialize.h>
#include <CryMath/GeomQuery.h>

class CPhysicalStub : public IPhysicalEntity {
public:
	CPhysicalStub() {}

	virtual pe_type GetType() const { return PE_STATIC; }
	virtual int AddRef() { return 0; }
	virtual int Release() { return 0; }
	virtual int SetParams(pe_params* params,int bThreadSafe=1) { return 0; }
	virtual int GetParams(pe_params* params) const { return 0; }
	virtual int GetStatus(pe_status* status) const { return 0; }
	virtual int Action(pe_action* action,int bThreadSafe=1) { return 0; }

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1) { return -1; }
	virtual void RemoveGeometry(int id,int bThreadSafe=1) {}
	virtual float GetExtent(EGeomForm eForm) const { return 0.f; }
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const {}

	virtual void *GetForeignData(int itype=0) const { return 0; }
	virtual int GetiForeignData() const { return 0; }

	virtual int GetStateSnapshot(class CStream &stm, float time_back=0, int flags=0) { return 0; }
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0) { return 0; }
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0) { return 0; }
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0) { return 0; }
	virtual int SetStateFromTypedSnapshot(TSerialize ser, int type, int flags/* =0 */) {return 0;}
	virtual int PostSetStateFromSnapshot() { return 0; }
	virtual int GetStateSnapshotTxt(char *txtbuf,int szbuf, float time_back=0) { return 0; }
	virtual void SetStateFromSnapshotTxt(const char *txtbuf,int szbuf) {}
	virtual unsigned int GetStateChecksum() { return 0; }
	virtual void SetNetworkAuthority(int authoritive, int paused) {}

	virtual void StartStep(float time_interval) {}
	virtual int Step(float time_interval) { return 0; }
	virtual int DoStep(float time_interval, int iCaller) { return 0; }
	virtual void StepBack(float time_interval) {}
	virtual IPhysicalWorld *GetWorld() const { return 0; }

	virtual void GetMemoryStatistics(ICrySizer *pSizer) const {}
};
CPhysicalStub g_stub;

static void SignalEventAreaChange( CPhysicalWorld* pWorld, CPhysArea* pArea, Vec3* pBBPrev = 0 )
{
	EventPhysAreaChange epac;
	epac.pEntity = pArea;
	epac.pForeignData=pArea->m_pForeignData; epac.iForeignData=pArea->m_iForeignData;
	epac.boxAffected[0] = pArea->m_BBox[0];
	epac.boxAffected[1] = pArea->m_BBox[1];
	if (pBBPrev) {
		epac.boxAffected[0].CheckMin(pBBPrev[0]);
		epac.boxAffected[1].CheckMax(pBBPrev[1]);
	}
	epac.q = quaternion(pArea->m_R);
	epac.pos = pArea->m_offset;
	epac.qContainer.SetIdentity();
	epac.posContainer.zero();
	if (epac.pContainer = pArea->m_pContainerEnt) {
		epac.qContainer=pArea->m_pContainerEnt->m_qrot; epac.posContainer=pArea->m_pContainerEnt->m_pos;
		epac.pos = (epac.pos-epac.posContainer)*epac.qContainer;
		epac.q = !epac.qContainer*epac.q;
	}
	epac.depth = -pArea->m_zlim[0];
	pWorld->OnEvent(0,&epac);
	if (pWorld->m_numAreaTriggers>0) {
		CPhysicalEntity **pentlist;
		pWorld->GetEntitiesAround(epac.boxAffected[0],epac.boxAffected[1], pentlist, ent_triggers, (CPhysicalEntity*)epac.pEntity);
	}
}

CPhysArea::CPhysArea(CPhysicalWorld *pWorld)
	: m_bDeleted(0)
	, m_next(nullptr)
	, m_nextBig(nullptr)
	, m_nextTyped(nullptr)
	, m_pWorld(pWorld)
	, m_offset(ZERO)
	, m_R(IDENTITY)
	, m_scale(1.0f)
	, m_rscale(1.0f)
	, m_offset0(ZERO)
	, m_R0(IDENTITY)
	, m_pGeom(nullptr)
	, m_pt(nullptr)
	, m_npt(0)
	, m_pFlows(nullptr)
	, m_pMask(nullptr)
	, m_ptSpline(nullptr)
	, m_V(0.0f)
	, m_accuracyV(0.001f)
	, m_pContainer(nullptr)
	, m_pContainerEnt(nullptr)
	, m_idpartContainer(0)
	, m_pContainerParts(nullptr)
	, m_nContainerParts(0)
	, m_bConvexBorder(0)
	, m_pTrigger(nullptr)
	, m_moveAccum(0.0f)
	, m_nSleepFrames(0)
	, m_sleepVel(0.05f)
	, m_borderPad(0.0f)
	, m_szCell(0.0f)
	, m_sizeReserve(0.0f)
	, m_minObjV(0.001f)
	, m_pWaterMan(nullptr)
	, m_pSurface(nullptr)
	, m_nptAlloc(0)
	, m_pMassGeom(nullptr)
	, m_ptLastCheck(1E10f, 1E10f, 1E10f)
	, m_iClosestSeg(0)
	, m_tClosest(0.0f)
	, m_mindist(0.0f)
	, m_rsize(ZERO)
	, m_bUniform(1)
	, m_falloff0(1.0f)
	, m_rfalloff0(0.0f)
	, m_damping(0.0f)
	, m_bUseCallback(0)
	, m_debugGeomHash(0)
	, m_lockRef(0)
{ 	
	m_nextActive = this;
	ZeroArray(m_zlim);
	ZeroArray(m_idxSort);

	MARK_UNUSED m_gravity, m_pb.waterPlane.origin, m_pb.iMedium;	

	//CPhysicalPlaceholder
	m_iSimClass=5; 
	m_iGThunk0 = 0; m_pEntBuddy = 0;
	 
	static_assert(CRY_ARRAY_COUNT(m_BBox) == 2, "Invalid array size!");
	m_BBox[0].zero();
	m_BBox[1].zero();
	m_pWorld->SetGrid(this,&m_pWorld->m_entgrid);
}

CPhysArea::~CPhysArea() 
{ 
	if (m_pt) { 
		delete[] m_pt; delete[] m_idxSort[0]; delete[] m_idxSort[1]; delete[] m_pMask; 
	}
	delete[] m_ptSpline;
	if (m_pGeom)
		m_pGeom->Release();
	if (m_pFlows)
		delete[] m_pFlows;
	if (m_pContainer)
		m_pContainer->Release();
	if (m_pContainerEnt) {
		m_pContainerEnt->Release();	m_pContainerEnt->RemoveGeometry(100);
	}
	if (m_pTrigger)
		m_pWorld->DestroyPhysicalEntity(m_pTrigger,0,1);
	DeleteWaterMan();
	if (m_pSurface)
		m_pSurface->Release();
}

void CPhysArea::DeleteWaterMan()
{
	if (m_pWaterMan) {
		WriteLock lock(m_pWorld->m_lockWaterMan);
		if (m_pWorld->m_pWaterMan==m_pWaterMan) 
			m_pWorld->m_pWaterMan = m_pWaterMan->m_next;
		delete m_pWaterMan;	m_pWaterMan=0;
	}
	if (m_pSurface) {
		m_pSurface->Release(); m_pSurface=0;
	}
}


CPhysicalEntity *CPhysArea::GetEntity() 
{
	return (CPhysicalEntity*)&g_stub; 
}

Vec3& zero_unused(Vec3& v)
{
	if (is_unused(v))
		v.zero();
	return v;
}
float getHeightFunc(float *data,getHeightCallback func, int ix,int iy) { return func(ix,iy); }
float getHeightData(float *data,getHeightCallback func, int ix,int iy) { return data[ix+iy]*0.33f; }

int CPhysArea::ApplyParams(const Vec3& pt, Vec3& gravity, const Vec3 &vel, pe_params_buoyancy *pbdst, int nBuoys, int nMaxBuoys, int &iMedium0, 
													 IPhysicalEntity *pent) const
{
	int bRes=1,bUseBuoy=0,bDiffGrid=0;
	QuatT transG(IDENTITY);
	if (bDiffGrid = pent && m_pWorld->GetGrid(this)!=m_pWorld->GetGrid(pent))
		transG = GridTrans((CPhysicalPlaceholder*)pent,this);
	Vec3 ptloc = ((transG*pt-m_offset)*m_R)*m_rscale;

	if (!is_unused(m_pb.waterPlane.origin) && 
			!(pent && ((CPhysicalEntity*)pent)->m_flags & pef_ignore_ocean && this==m_pWorld->m_pGlobalArea)) 
	{
		if (m_pb.iMedium==iMedium0)
			nBuoys=0,iMedium0=-1,bRes=0;
		if (nBuoys<nMaxBuoys)
			pbdst[nBuoys] = m_pb,bUseBuoy=1;
	}

	if (m_ptSpline && !m_bUniform) {
		Vec3 p0,p1,p2,v0,v1,v2,gcent(ZERO),gpull;
		int iClosestSeg; float tClosest,mindist;
		mindist = FindSplineClosestPt(ptloc, iClosestSeg,tClosest);
		p0 = m_ptSpline[max(0,iClosestSeg-1)]; p1 = m_ptSpline[iClosestSeg]; p2 = m_ptSpline[min(m_npt-1,iClosestSeg+1)];
		v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f;
		if (iClosestSeg+tClosest>0 && iClosestSeg+tClosest<m_npt)
			gcent = ((v2*tClosest+v1)*tClosest+v0-ptloc).normalized();
		gpull = (v2*max(0.01f,min(1.99f,2*tClosest))+v1).normalized();
		if (!m_bUseCallback) {
			float t = sqr(1-sqrt_tpl(mindist)/m_zlim[0]);
			t = t*(1-m_falloff0)+m_falloff0;
			if (!is_unused(m_gravity))
				zero_unused(gravity) += m_R*(gpull*t+gcent*(1-t))*m_gravity.z-vel*m_damping;
			if (bUseBuoy)
				pbdst[nBuoys].waterFlow = m_R*(gpull*t+gcent*(1-t))*m_pb.waterFlow.z;
		} else {
			EventPhysArea epa;
			epa.pt=pt; epa.gravity=m_gravity; epa.pb=m_pb; epa.pent=pent;
			epa.ptref=(v2*tClosest+v1)*tClosest+v0;
			epa.dirref=gpull;
			m_pWorld->OnEvent(0,&epa);
			if (is_unused(epa.gravity))
				zero_unused(gravity) += epa.gravity;
			if (!is_unused(epa.pb.waterPlane.origin) && nBuoys<nMaxBuoys)
				pbdst[nBuoys] = epa.pb;
		}
	}	else {
		box bbox; bbox.center.zero();
		if (m_pGeom)
			m_pGeom->GetBBox(&bbox);
		Vec3 vdist = (ptloc-bbox.center)*Diag33(m_rsize);
		float dist = m_pGeom && m_pGeom->GetType() == GEOM_BOX ? max(max(abs(vdist.x), abs(vdist.y)), abs(vdist.z))
			: vdist.GetLengthFast();
		if (dist>1)
			bRes = 0;
		else if (dist<1) if (!m_bUseCallback) {
			dist = 1-max(0.0f,dist-m_falloff0)*m_rfalloff0;
			if (m_bUniform) {
				if (!is_unused(m_gravity)) {
					if (m_bUniform==2)
						zero_unused(gravity) += m_R*m_gravity*dist;
					else
						zero_unused(gravity) += m_gravity*dist;
				}
			}	else {
				Vec3 dir = (pt-m_offset).normalized();
				if (!is_unused(m_gravity))
					zero_unused(gravity) += dir*(dist*m_gravity.z);	
				if (bUseBuoy)
					pbdst[nBuoys].waterFlow = dir*m_pb.waterFlow.z;
			}
			if (bUseBuoy)	{
				if (m_pGeom && m_pGeom->GetType()==GEOM_HEIGHTFIELD || m_pWaterMan && pent!=m_pContainerEnt) {
					if (pent && m_pb.waterPlane.origin.z>-1000.0f) {
						CPhysicalPlaceholder *ppc = (CPhysicalPlaceholder*)pent;
						int i,j,istep,jstep,npt;
						Vec3 sz,org,n=m_pb.waterPlane.n,offset=m_offset;
						float det,maxdet,maxz=0.0f;
						Matrix33 Rabs,C,tmp;
						Vec2i ibbox[2];
						org = transG*((ppc->m_BBox[0]+ppc->m_BBox[1])*0.5f);
						sz = (ppc->m_BBox[1]-ppc->m_BBox[0])*0.5f;
						if (true) {//fabs_tpl((org-m_pb.waterPlane.origin)*n)-sz*n.abs() < 1.5f) {
							sz = (Rabs=m_R*Matrix33(transG.q)).Fabs()*sz;
							heightfield hf, *phf=&hf;
							int iyscale=1,vmask=0; float *pdata; Vec3 vel(ZERO),*pvel=&vel;
							float (*getHeight)(float *data,getHeightCallback func, int ix,int iy);
							if (m_pWaterMan) {
								hf.size.set(m_pWaterMan->m_nCells,m_pWaterMan->m_nCells);
								hf.step.set(m_pWaterMan->m_cellSz,m_pWaterMan->m_cellSz);
								hf.stepr.set(m_pWaterMan->m_rcellSz,m_pWaterMan->m_rcellSz);
								offset = m_pb.waterPlane.origin + m_R*Vec3(hf.step)*0.5f*m_scale;
								getHeight = getHeightData; pdata=m_pWaterMan->m_pTiles[0]->ph; iyscale=hf.size.x;
								pvel=m_pWaterMan->m_pTiles[0]->pvel; vmask=-1;
								m_pWaterMan->OnWaterInteraction((CPhysicalEntity*)pent);
								m_pWorld->ActivateArea(const_cast<CPhysArea*>(this));
							} else {
								phf = (heightfield*)m_pGeom->GetData();
								getHeight = getHeightFunc;
							}
							org = ((org-offset)*m_R)*m_rscale;
							ibbox[0].x = max(0, min(phf->size.x-1, float2int((org.x-sz.x)*phf->stepr.x-0.5f)));
							ibbox[0].y = max(0, min(phf->size.y-1, float2int((org.y-sz.y)*phf->stepr.y-0.5f)));
							ibbox[1].x = max(0, min(phf->size.x-1, float2int((org.x+sz.x)*phf->stepr.x+0.5f)));
							ibbox[1].y = max(0, min(phf->size.y-1, float2int((org.y+sz.y)*phf->stepr.y+0.5f)));
							org.zero(); C.SetZero();
							istep=(ibbox[1].x-ibbox[0].x>>2)+1; jstep=(ibbox[1].y-ibbox[0].y>>2)+1;
							for(i=ibbox[0].x,npt=0;i<=ibbox[1].x;i+=istep) for(j=ibbox[0].y;j<=ibbox[1].y;j+=jstep,npt++) {
								sz.Set((i-ibbox[0].x)*phf->step.x, (j-ibbox[0].y)*phf->step.y, getHeight(pdata,phf->fpGetHeightCallback,i,j*iyscale));
								C += dotproduct_matrix(sz,sz,tmp); org += sz;
								vel += Vec3(*(Vec2*)(pvel+(i+j*iyscale & vmask)));
								maxz = max(maxz, fabs_tpl(sz.z));
							}
							float rnpt=1.0f/npt; org*=rnpt;	vel*=rnpt;
							C -= dotproduct_matrix(org,org,tmp)*(float)npt;
							org += Vec3(ibbox[0].x*phf->step.x, ibbox[0].y*phf->step.y, 0);

							for(i=0,j=-1,maxdet=0;i<3;i++) {
								det = C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],dec_mod3[i]) - C(dec_mod3[i],inc_mod3[i])*C(inc_mod3[i],dec_mod3[i]);
								if (fabs(det)>fabs(maxdet)) 
									maxdet=det, j=i;
							}
							if (fabs(maxdet)>=FLT_EPSILON && maxz<10000.0f) {
								det = 1.0/maxdet;
								n[j] = 1;
								n[inc_mod3[j]] = -(C(inc_mod3[j],j)*C(dec_mod3[j],dec_mod3[j]) - C(dec_mod3[j],j)*C(inc_mod3[j],dec_mod3[j]))*det;
								n[dec_mod3[j]] = -(C(inc_mod3[j],inc_mod3[j])*C(dec_mod3[j],j) - C(dec_mod3[j],inc_mod3[j])*C(inc_mod3[j],j))*det;
								n = m_R*n.normalized();

								pbdst[nBuoys].waterPlane.origin = m_R*org*m_scale+offset;
								pbdst[nBuoys].waterPlane.n = n*sgnnz(n*pbdst[nBuoys].waterPlane.n);
								pbdst[nBuoys].waterFlow += vel;
							}
						}
					}
				}	else if (m_pt && m_pGeom && !m_debugGeomHash) {
					pbdst[nBuoys].waterPlane.origin = m_R*m_trihit.pt[0]+m_offset;
					pbdst[nBuoys].waterPlane.n = m_R*m_trihit.n;
					if (m_pFlows) {
						CTriMesh *pMesh = (CTriMesh*)m_pGeom;
						float rarea = isqrt_safe_tpl( (pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3+1]]-pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3]] ^ 
																					 pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3+2]]-pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3]]).len2() );
						pbdst[nBuoys].waterFlow.zero();
						for(int i=0;i<3;i++)
							pbdst[nBuoys].waterFlow += m_pFlows[pMesh->m_pIndices[m_trihit.idx*3+i]]*(rarea*
								((pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3+i]]-ptloc ^ 
								  pMesh->m_pVertices[pMesh->m_pIndices[m_trihit.idx*3+inc_mod3[i]]]-ptloc)*m_trihit.n));
						pbdst[nBuoys].waterFlow = m_R*pbdst[nBuoys].waterFlow;
					}
				}
				if (m_bUniform==2)
					pbdst[nBuoys].waterFlow = m_R*pbdst[nBuoys].waterFlow*dist;
				else
					pbdst[nBuoys].waterFlow *= dist;
			}	else if (m_damping>0 && nBuoys<nMaxBuoys)	{
				pbdst[nBuoys].iMedium = 1;
				pbdst[nBuoys].waterPlane.origin.Set(0,0,1E8f);
				pbdst[nBuoys].waterPlane.n.Set(0,0,1);
				pbdst[nBuoys].waterDamping = m_damping;
				pbdst[nBuoys].waterDensity = 0;
				pbdst[nBuoys].waterResistance = 0;
				pbdst[nBuoys].waterFlow.zero();
				bUseBuoy = 1;
			}
		}	else {
			EventPhysArea epa;
			epa.pt=pt; epa.gravity=m_gravity; epa.pb=m_pb; epa.pent=pent;
			epa.ptref=m_offset; epa.dirref.zero();
			m_pWorld->OnEvent(0,&epa);
			if (is_unused(epa.gravity))
				zero_unused(gravity) += epa.gravity;
			if (!is_unused(epa.pb.waterPlane.origin) && nBuoys<nMaxBuoys)
				pbdst[nBuoys] = epa.pb;
		}
	}

	if (bUseBuoy & bDiffGrid) {
		pbdst[nBuoys].waterPlane.origin = transG.GetInverted()*pbdst[nBuoys].waterPlane.origin;
		pbdst[nBuoys].waterPlane.n = !transG.q*pbdst[nBuoys].waterPlane.n;
		pbdst[nBuoys].waterFlow = !transG.q*pbdst[nBuoys].waterFlow;
	}

	return bRes&bUseBuoy;
}

int CPhysArea::CheckPoint(const Vec3& pttest, float radius) const
{
	Vec3 ptloc = ((pttest-m_offset)*m_rscale)*m_R;
	int iClosestSeg; float tClosest;
	if (!is_unused(m_gravity))
		radius = 0.0f;

	if (m_pGeom && !m_pt)
		return radius>0 ? ((CGeometry*)m_pGeom)->SphereCheck(ptloc,radius) : m_pGeom->PointInsideStatus(ptloc);
	else if (m_pt) {
		unsigned int maskbuf[64],*pMask;
		int bReadLock=1;
		if (m_npt < sizeof(maskbuf)*8)
			memset(pMask=maskbuf, 0, (m_npt-1>>3)+1);
		else {
			int iCaller=get_iCaller();
			pMask = m_pMask+iCaller*((m_npt-1>>5)+1);
			bReadLock = isneg(iCaller-MAX_PHYS_THREADS);
		}
		ReadLockCond lockr(m_lockUpdate, bReadLock);
		WriteLockCond lockw(m_lockUpdate, bReadLock^1);
		int i,iend,i1,istart[2],ilim[2],bInside=0;
		float fend;
		quotientf y,ymax;
		Vec2 dp;
		if (!inrange(ptloc.z, m_zlim[0]-radius,m_zlim[1]+radius))
			return 0;
		for(fend=0,iend=0; iend<2; iend++,fend++) {
			ilim[0]=-1; ilim[1]=m_npt;
			do {
				i = ilim[0]+ilim[1]>>1;
				i1 = m_idxSort[iend][i]+1; i1 &= i1-m_npt>>31;
				ilim[isneg(ptloc.x-minmax(m_pt[m_idxSort[iend][i]].x,m_pt[i1].x,fend))] = i;
			}	while(ilim[1] > ilim[0]+1);
			istart[iend] = ilim[1];
		}
		if (istart[0]-1>>31 | m_npt-1-istart[1]>>31)
			return 0;

		for(i=0; i<istart[0]; i++) 
			pMask[m_idxSort[0][i]>>5] |= 1<<(m_idxSort[0][i]&31);
		for(i=istart[1],ymax=1E10f; i<m_npt; i++) if (pMask[m_idxSort[1][i]>>5] & 1<<(m_idxSort[1][i]&31)) {
			dp = m_pt[m_idxSort[1][i]+1 & m_idxSort[1][i]-m_npt+1>>31]-m_pt[m_idxSort[1][i]];
			y.set(((ptloc.x-m_pt[m_idxSort[1][i]].x)*dp.y+m_pt[m_idxSort[1][i]].y*dp.x)*dp.x, sqr(dp.x));
			if (y>ptloc.y && y<ymax) {
				ymax = y; bInside = isneg(dp.x);
			}
		}
		for(i=0;i<istart[0];i++) 
			pMask[m_idxSort[0][i]>>5] ^= 1<<(m_idxSort[0][i]&31);
		if (bInside && m_pGeom && !m_debugGeomHash) {
			CTriMesh *pMesh = (CTriMesh*)m_pGeom;
			if (!(bInside &= pMesh->PointInsideStatusMesh(ptloc,&m_trihit)))
				if (radius>0 && (i=pMesh->SphereCheck(ptloc,radius))) {
					m_trihit.idx = --i;	bInside = 1;
					m_trihit.n = pMesh->m_pNormals[i];
					for(i1=0;i1<3;i1++) m_trihit.pt[i1] = pMesh->m_pVertices[pMesh->m_pIndices[i*3+i1]];
				}
		}

		return bInside;
	} else if (m_ptSpline)
		return isneg(FindSplineClosestPt(ptloc,iClosestSeg,tClosest)-sqr(m_zlim[0]));

	return 0;
}

float CPhysArea::FindSplineClosestPt(const Vec3 &ptloc, int &iClosestSegRet,float &tClosestRet) const
{
	int i,j,iClosestSeg;
	Vec3 p0,p1,p2,v0,v1,v2,BBox[2];
	float dist,mindist,tClosest;
	real t[4];
	{ ReadLock lock(m_lockUpdate); 
		if ((ptloc-m_ptLastCheck).len2()==0) {
			iClosestSegRet = m_iClosestSeg; tClosestRet = m_tClosest;
			return m_mindist;
		}
	}
	mindist = (ptloc-m_ptSpline[0]).len2(); iClosestSeg=0; tClosest=0;
	if ((dist=(ptloc-m_ptSpline[m_npt-1]).len2())<mindist)
		mindist=dist, iClosestSeg=m_npt-1, tClosest=1;

	for(i=0;i<m_npt;i++) {
		p0 = m_ptSpline[max(0,i-1)]; p1 = m_ptSpline[i]; p2 = m_ptSpline[min(m_npt-1,i+1)];
		BBox[0] = min(min(p0,p1),p2);	BBox[0] -= Vec3(1,1,1)*m_zlim[0];
		BBox[1] = max(max(p0,p1),p2);	BBox[1] += Vec3(1,1,1)*m_zlim[0];
		if (PtInAABB(BBox,ptloc)) {
			v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f-ptloc;
			for(j=(P3((v2*v2)*2)+P2((v2*v1)*3)+P1(v1*v1+(v2*v0)*2)+real(v1*v0)).findroots(0,1,t)-1;j>=0;j--)
				if ((dist=((v2*t[j]+v1)*t[j]+v0).len2())<mindist)
					mindist=dist, iClosestSeg=i, tClosest=t[j];
		}
	}

	{ WriteLock lock(m_lockUpdate); 
		m_ptLastCheck=ptloc; 
		m_mindist=mindist; m_iClosestSeg=iClosestSeg; m_tClosest=tClosest;
		iClosestSegRet=m_iClosestSeg; tClosestRet=m_tClosest;
	}
	
	return mindist;
}

int CPhysArea::FindSplineClosestPt(const Vec3 &org, const Vec3 &dir, Vec3 &ptray, Vec3 &ptspline) const
{
	int i,j;
	Vec3 p0,p1,p2,v0,v1,v2,v0d,v1d,v2d,pt,ptmin,ptmax;
	float dist,mindist,h,dlen2=dir.len2();
	box bbox;
	ray aray;
	real t[4];
	mindist=(m_ptSpline[0]-org).len2(); ptray=org; ptspline=m_ptSpline[0];
	if ((dist=(m_ptSpline[0]-org-dir).len2()) < mindist)
		mindist=dist, ptray=org+dir;
	if ((dist=(m_ptSpline[m_npt-1]-org).len2()) < mindist)
		mindist=dist, ptray=org, ptspline=m_ptSpline[m_npt-1];
	if ((dist=(m_ptSpline[m_npt-1]-org-dir).len2()) < mindist)
		mindist=dist, ptray=org+dir, ptspline=m_ptSpline[m_npt-1];
	mindist *= dlen2;	ptray *= dlen2;
	bbox.Basis.SetIdentity();
	bbox.bOriented = 0;
	aray.dir = dir;
	aray.origin = org;

	for(i=0;i<m_npt;i++) {
		p0 = m_ptSpline[max(0,i-1)]; p1 = m_ptSpline[i]; p2 = m_ptSpline[min(m_npt-1,i+1)];
		ptmin = min(min(p0,p1),p2);	ptmax = max(max(p0,p1),p2);	
		bbox.center = (ptmin+ptmax)*0.5f;
		bbox.size = (ptmax-ptmin)*0.5f+Vec3(1,1,1)*m_zlim[0];
		if (box_ray_overlap_check(&bbox,&aray)) {
			v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f-org-dir;
			for(j=(P3((v2*v2)*2)+P2((v2*v1)*3)+P1(v1*v1+(v2*v0)*2)+real(v1*v0)).findroots(0,1,t)-1;j>=0;j--)
				if ((dist=(pt=(v2*t[j]+v1)*t[j]+v0).len2()*dlen2)<mindist)
					mindist=dist, ptspline=pt+org+dir, ptray=(org+dir)*dlen2;
			v0 += dir;
			for(j=(P3((v2*v2)*2)+P2((v2*v1)*3)+P1(v1*v1+(v2*v0)*2)+real(v1*v0)).findroots(0,1,t)-1;j>=0;j--)
				if ((dist=(pt=(v2*t[j]+v1)*t[j]+v0).len2()*dlen2)<mindist)
					mindist=dist, ptspline=pt+org, ptray=org*dlen2;
			v2d = v2^dir; v1d = v1^dir; v0d = v0^dir;
			for(j=(P3((v2d*v2d)*2)+P2((v2d*v1d)*3)+P1(v1d*v1d+(v2d*v0d)*2)+real(v1d*v0d)).findroots(0,1,t)-1;j>=0;j--) 
				if (inrange(h=(pt=(v2*t[j]+v1)*t[j]+v0)*dir,0.0f,dlen2) && (dist=(pt^dir).len2())<mindist)
					mindist=dist, ptspline=pt+org, ptray=org*dlen2+dir*h;
		}
	}

	if (mindist < sqr(m_zlim[0])*dlen2) {
		ptray /= dlen2; return 1;
	}
	return 0;
}

int CPhysArea::RayTraceInternal(const Vec3 &org, const Vec3 &dir, ray_hit *pHit, pe_params_pos *pp)
{
	geom_world_data gwd;
	geom_contact *pcontacts;
	intersection_params ip;
	int ncont,iClosestSeg;
	float tClosest;
	if (pp) {
		quaternionf qrot;
		gwd.offset = pp->pos; qrot = pp->q; 
		if (!is_unused(pp->scale)) gwd.scale = pp->scale;
		get_xqs_from_matrices(pp->pMtx3x4,pp->pMtx3x3, gwd.offset,qrot,gwd.scale);
		gwd.R = Matrix33(qrot);
	} else {
		gwd.R = m_R;
		gwd.offset = m_offset;
		gwd.scale = m_scale;
	}
	pHit->pCollider = this; 
	pHit->partid=pHit->surface_idx=pHit->idmatOrg=pHit->foreignIdx = 0;

	if (m_pGeom && m_pGeom->GetType()==GEOM_TRIMESH && !m_debugGeomHash) {
		CRayGeom aray(org,dir);
		CTriMesh* pTrimesh = (CTriMesh*)PrepGeomExt(m_pGeom);
		ncont = pTrimesh->CTriMesh::Intersect(&aray,&gwd,(geom_world_data*)0,&ip,pcontacts);
		for(; ncont>0 && pcontacts[ncont-1].n*dir>0; ncont--);
		if (ncont>0) {
			pHit->dist = pcontacts[ncont-1].t;
			pHit->pt = pcontacts[ncont-1].pt; 
			pHit->n = pcontacts[ncont-1].n;
			return 1;
		}
	}	else if (m_npt>0 && m_ptSpline && m_pb.iMedium!=0) {
		Vec3 ptray,ptspline;
		float rscale = gwd.scale==1.0f ? 1.0f:1.0f/gwd.scale;
		if (FindSplineClosestPt(((org-gwd.offset)*rscale)*gwd.R, dir*gwd.R, ptray,ptspline)) {
			pHit->pt = gwd.R*ptray*gwd.scale+gwd.offset;
			pHit->n = gwd.R*(ptray-ptspline).normalized();
			pHit->dist = (pHit->pt-org)*dir.normalized();
			return 1;
		}
	}	else if (m_pb.iMedium==0) {
		quotientf t((m_pb.waterPlane.origin-org)*m_pb.waterPlane.n, dir*m_pb.waterPlane.n);
		if (inrange(t.x, 0.0f,t.y)) {
			pHit->dist = (t.x/=t.y)*dir.len();
			pHit->pt = org+dir*t.x;
			pHit->n = m_pb.waterPlane.n;
			if (m_pGeom && !m_debugGeomHash) {
				if (m_pGeom->GetType()!=GEOM_HEIGHTFIELD)
					return m_pGeom->PointInsideStatus(((pHit->pt-gwd.offset)*gwd.R)*(gwd.scale==1.0f?1.0f:1.0f/gwd.scale));
				else {
					heightfield *phf = (heightfield*)m_pGeom->GetData();
					CRayGeom aray(org,dir);
					if (dir.len2()>sqr((phf->step.x+phf->step.y)*3*m_scale)) {
						aray.m_ray.dir = aray.m_dirn*((phf->step.x+phf->step.y)*m_scale*3);
						aray.m_ray.origin = pHit->pt-aray.m_ray.dir*0.5f;
					}
					IGeometry *pHFGeom = PrepGeomExt(m_pGeom);
					if (ncont = ((CHeightfield*)pHFGeom)->CHeightfield::Intersect(&aray,&gwd,(geom_world_data*)0,&ip,pcontacts)) {
						pHit->pt = pcontacts[ncont-1].pt;
						pHit->dist = (pHit->pt-org)*aray.m_dirn;
						pHit->n = pcontacts[ncont-1].n;
					}
					return 1;
				}
			}	else if (m_npt>0 && m_ptSpline)
				return FindSplineClosestPt(((pHit->pt-gwd.offset)*gwd.R)/gwd.scale,iClosestSeg,tClosest) < m_zlim[0];
			else if (m_pt)
				return CheckPoint(pHit->pt);
			else
				return 1;
		}
	}
	return 0;
}

float CPhysArea::GetExtent(EGeomForm eForm) const
{
	float extent = 0.f;
	if (m_pGeom && !m_debugGeomHash) {
		extent = m_pGeom->GetExtent(eForm);
	} else if (m_ptSpline) {
		CGeomExtent& ext = m_Extents.Make(GeomForm_Edges);
		if (!ext) {
			// approximate segment lengths with point distance
			ext.ReserveParts(m_npt-1);
			for (int i = 0; i < m_npt-1; i++)
				ext.AddPart((m_ptSpline[i] - m_ptSpline[i+1]).GetLength());
			float cross = CircleExtent(EGeomForm(eForm-1), m_zlim[0]);
			extent = ext.TotalExtent() * cross;
		}
	}

	return extent * ScaleExtent(eForm, m_scale);
}

void CPhysArea::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm)	const
{
	if (m_pGeom && !m_debugGeomHash) {
		m_pGeom->GetRandomPoints(points, seed, eForm);
	}
	else if (m_ptSpline && m_Extents[GeomForm_Edges].NumParts()) {
		for (auto& ran : points) {
			// choose random segment, and fractional distance therein
			int i = m_Extents[GeomForm_Edges].RandomPart(seed);
			float t = seed.GetRandom(0.0f, 1.0f);
			if (t <= 0.5f)
				i++;

			Vec3 tang;
			if (i == 0)
			{
				tang = m_ptSpline[1] - m_ptSpline[0];
				ran.vPos = m_ptSpline[0] + tang * (t - 0.5f);
			}
			else if (i == m_npt - 1)
			{
				tang = m_ptSpline[m_npt - 1] - m_ptSpline[m_npt - 2];
				ran.vPos = m_ptSpline[m_npt - 2] + tang * (t + 0.5f);
			}
			else
			{
				Vec3 const& p0 = m_ptSpline[i - 1];
				Vec3 const& p1 = m_ptSpline[i];
				Vec3 const& p2 = m_ptSpline[i + 1];
				Vec3 v2 = (p0 + p2)*0.5f - p1,
					v1 = p1 - p0,
					v0 = (p0 + p1)*0.5f;
				ran.vPos = (v2*t + v1)*t + v0;
				tang = v2*(2.f*t) + v1;
			}

			// compute radial axes from tangent
			Vec3 x(max(abs(tang.y), abs(tang.z)), max(abs(tang.z), abs(tang.x)), max(abs(tang.x), abs(tang.y)));
			Vec3 y = tang ^ x;
			x.Normalize();
			y.Normalize();

			// generate random displacement from spline.
			Vec2 xy = CircleRandomPoint(seed, EGeomForm(eForm - 1), m_zlim[0]);
			Vec3 dis = x * xy.x + y * xy.y;

			ran.vPos += dis;
			ran.vNorm = dis.normalized();
		}
	}
	else
		return points.fill(ZERO);

	for (auto& ran : points) {
		ran.vPos = m_R*ran.vPos * m_scale + m_offset;
		ran.vNorm = m_R*ran.vNorm;
	}
}


int IsAreaNonWater(CPhysArea *pArea)
{
	return pArea!=pArea->m_pWorld->m_pGlobalArea && (!is_unused(pArea->m_gravity) || !is_unused(pArea->m_pb.waterPlane.origin) && 
		pArea->m_pb.iMedium>0 && pArea->m_pb.waterDamping+pArea->m_pb.waterDensity+pArea->m_pb.waterResistance>0);
}
int IsAreaGravity(CPhysArea *pArea)
{
	return pArea!=pArea->m_pWorld->m_pGlobalArea && !is_unused(pArea->m_gravity);
}

int CPhysArea::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued())
		return 1;

	if (_params->type==pe_params_pos::type_id) {
		pe_params_pos *params = (pe_params_pos*)_params;
		WriteLock lock(m_pWorld->m_lockAreas);
		Vec3 boxPrev[2] = { m_BBox[0], m_BBox[1] };
		get_xqs_from_matrices(params->pMtx3x4,params->pMtx3x3, params->pos,params->q,params->scale);
		Vec3 pos,sz;
		Vec3 lastPos = m_offset - m_offset0;
		if (!is_unused(params->pos)) m_offset = m_offset0+params->pos;
		if (!is_unused(params->q)) m_R = Matrix33(params->q)*m_R0;
		if (!is_unused(params->scale)) m_rscale = 1/(m_scale = params->scale);
		if (params->bRecalcBounds & 2 && !is_unused(params->pos))	{
			m_BBox[0] += params->pos - lastPos;
			m_BBox[1] += params->pos - lastPos;
		}	else if (m_pGeom && !m_debugGeomHash) {
			box abox;
			m_pGeom->GetBBox(&abox);
			abox.Basis *= m_R.T();
			sz = (abox.size*abox.Basis.Fabs())*m_scale;
			pos = m_offset + m_R*abox.center*m_scale;
			m_BBox[0] = pos-sz; m_BBox[1] = pos+sz;
		} else if (m_pt) {
			sz = Matrix33(m_R).Fabs()*m_size0*m_scale;
			pos = (m_offset-m_offset0) + (m_R*m_R0.T())*m_offset0*m_scale;
			m_BBox[0] = pos-sz; m_BBox[1] = pos+sz;
		}	else if (m_ptSpline) {
			m_BBox[0]=m_BBox[1] = m_R*m_ptSpline[0]*m_scale+m_offset;
			for(int i=0;i<m_npt;i++) {
				Vec3 ptw = m_R*m_ptSpline[i]*m_scale+m_offset;
				m_BBox[0] = min(m_BBox[0],ptw); m_BBox[1] = max(m_BBox[1],ptw);
			}
			m_BBox[0] -= Vec3(m_zlim[0],m_zlim[0],m_zlim[0]);
			m_BBox[1] += Vec3(m_zlim[0],m_zlim[0],m_zlim[0]);
		}
		m_pWorld->RepositionArea(this, boxPrev);
		return 1;
	}

	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		m_pWorld->m_numNonWaterAreas -= IsAreaNonWater(this);
		m_pWorld->m_numGravityAreas -= IsAreaGravity(this);
		if (!is_unused(params->gravity)) {
			m_gravity = params->gravity;
			if (m_pWorld->m_pGlobalArea==this)
				m_pWorld->m_vars.gravity = m_gravity;
		}
		if (!is_unused(params->damping)) m_damping = params->damping;
		m_pWorld->m_numNonWaterAreas += IsAreaNonWater(this);
		m_pWorld->m_numGravityAreas += IsAreaGravity(this);
		SignalEventAreaChange(m_pWorld, this);
		return 1;
	}

	if (_params->type==pe_params_area::type_id) {
		pe_params_area *params = (pe_params_area*)_params;
		m_pWorld->m_numNonWaterAreas -= IsAreaNonWater(this);
		m_pWorld->m_numGravityAreas -= IsAreaGravity(this);
		if (!is_unused(params->gravity)) {
			m_gravity = params->gravity;
			if (m_pWorld->m_pGlobalArea==this)
				m_pWorld->m_vars.gravity = m_gravity;
		}
		if (!is_unused(params->bUniform)) m_bUniform = params->bUniform;
		if (!is_unused(params->falloff0) && params->falloff0 >= 0) 
			m_rfalloff0 = (m_falloff0=params->falloff0)<1.0f ? 1.0f/(1-params->falloff0):0;
		if (!is_unused(params->damping)) m_damping = params->damping;
		if (!is_unused(params->bUseCallback)) m_bUseCallback = params->bUseCallback;
		if (!is_unused(params->pGeom) && params->pGeom!=m_pGeom) {
			if (m_pGeom) m_pGeom->Release();
			m_pGeom = params->pGeom; m_debugGeomHash = 0;
		}
		if (!is_unused(params->objectVolumeThreshold)) m_minObjV = params->objectVolumeThreshold;
		if (!is_unused(params->growthReserve) && params->growthReserve!=m_sizeReserve) { DeleteWaterMan(); m_sizeReserve=params->growthReserve; m_pWorld->ActivateArea(this); }
		if (!is_unused(params->borderPad) && params->borderPad!=m_borderPad) { m_borderPad = params->borderPad; m_pWorld->ActivateArea(this); }
		if (!is_unused(params->bConvexBorder) && params->bConvexBorder!=m_bConvexBorder) { m_bConvexBorder = params->bConvexBorder; m_pWorld->ActivateArea(this); }
		if (!is_unused(params->volume) && params->volume!=m_V) { m_V = params->volume; m_pWorld->ActivateArea(this); }
		if (!is_unused(params->cellSize) && m_szCell!=params->cellSize) { 
			if (m_pContainer && m_qtsContainer.s!=1.0f) {
				m_pContainer->Release(); m_pContainer=0;
			}
			DeleteWaterMan(); m_szCell=params->cellSize; m_pWorld->ActivateArea(this); 
		}
		if (!is_unused(params->volumeAccuracy)) m_accuracyV = params->volumeAccuracy;
		pe_params_waterman pwm; bool bSetWaterman=false;
		for(int i=0;i<sizeof(m_waveSim)/sizeof(float);i++) if (!is_unused(((float*)&params->waveSim)[i])) {
			((float*)&m_waveSim)[i]=((float*)&params->waveSim)[i]; bSetWaterman=true; 
		}
		if (m_pWaterMan && bSetWaterman) {
			memcpy(&(params_wavesim&)pwm, &params->waveSim, sizeof(params_wavesim));
			m_pWaterMan->SetParams(&pwm);
		}
		m_pWorld->m_numNonWaterAreas += IsAreaNonWater(this);
		m_pWorld->m_numGravityAreas += IsAreaGravity(this);
		SignalEventAreaChange(m_pWorld, this);
		return 1;
	}

	if (_params->type==pe_params_buoyancy::type_id) {
		pe_params_buoyancy *params = (pe_params_buoyancy*)_params;
		m_pWorld->m_numNonWaterAreas -= IsAreaNonWater(this);
		if (!is_unused(params->waterPlane.origin) && is_unused(m_pb.waterPlane.origin)) {
			m_pb.waterDensity = 1000.0f; m_pb.waterDamping = 0.0f;
			m_pb.waterPlane.n.Set(0,0,1);	m_pb.waterPlane.origin.zero();
			m_pb.waterFlow.zero(); m_pb.waterResistance = 1000.0f;
		}
		if (!is_unused(params->iMedium)) {
			WriteLock lock(m_pWorld->m_lockAreas);
			if ((unsigned int)m_pb.iMedium<16) {
				CPhysArea *pArea;
				if (m_pWorld->m_pTypedAreas[m_pb.iMedium]==this)
					m_pWorld->m_pTypedAreas[m_pb.iMedium] = m_nextTyped;
				else {
					for(pArea=m_pWorld->m_pTypedAreas[m_pb.iMedium]; pArea && pArea->m_nextTyped!=this; pArea=pArea->m_nextTyped);
					if (pArea)
						pArea->m_nextTyped = m_nextTyped;
				}
				m_nextTyped = 0;
			}
			if ((unsigned int)(m_pb.iMedium=params->iMedium)<16) {
				m_nextTyped = m_pWorld->m_pTypedAreas[m_pb.iMedium];
				m_pWorld->m_pTypedAreas[m_pb.iMedium] = this;
			}
		}
		if (!is_unused(params->waterDensity)) m_pb.waterDensity = params->waterDensity;
		if (!is_unused(params->waterDamping)) m_pb.waterDamping = params->waterDamping;
		if (!is_unused(params->waterPlane.n)) m_pb.waterPlane.n = params->waterPlane.n;
		if (!is_unused(params->waterPlane.origin) && m_V<=0) m_pb.waterPlane.origin = params->waterPlane.origin;
		if (!is_unused(params->waterFlow)) m_pb.waterFlow = params->waterFlow;
		if (!is_unused(params->waterResistance)) m_pb.waterResistance = params->waterResistance;
		if (m_pWorld->m_pGlobalArea==this)
			m_pWorld->m_bCheckWaterHits = m_pWorld->m_matWater>=0 && !is_unused(m_pb.waterPlane.origin) ? ent_water:0;
		m_pWorld->m_numNonWaterAreas += IsAreaNonWater(this);
		SignalEventAreaChange(m_pWorld, this);
		return 1;
	}

	return CPhysicalPlaceholder::SetParams(_params,bThreadSafe);
}

int CPhysArea::GetParams(pe_params *_params) const
{
	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		params->gravity = m_gravity;
		params->damping = m_damping;
		return 1;
	}

	if (_params->type==pe_params_area::type_id) {
		pe_params_area *params = (pe_params_area*)_params;
		params->gravity = m_gravity;
		params->bUniform = m_bUniform;
		params->damping = m_damping;
		params->falloff0 = m_falloff0;
		params->bUseCallback = m_bUseCallback;
		params->pGeom = m_pGeom;
		params->objectVolumeThreshold = m_minObjV;
		params->growthReserve = m_sizeReserve;
		params->borderPad = m_borderPad;
		params->volume = m_V;
		params->cellSize = m_szCell;
		params->volumeAccuracy = m_accuracyV;
		if (m_pWaterMan) {
			pe_params_waterman pwm; m_pWaterMan->GetParams(&pwm);
			memcpy(&params->waveSim, &(params_wavesim&)pwm, sizeof(params->waveSim));
		} else
			memcpy(&params->waveSim, &m_waveSim, sizeof(m_waveSim));
		params->bConvexBorder = m_bConvexBorder;
		return 1;
	}

	if (_params->type==pe_params_buoyancy::type_id) {
		*(pe_params_buoyancy*)_params = m_pb;
		return 1;
	}

	return CPhysicalPlaceholder::GetParams(_params);
}

int CPhysArea::Action(pe_action *_action,int bThreadSafe) 
{
	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())
		return 1;

	if (_action->type==pe_action_reset::type_id) {
		if (m_pWaterMan)
			m_pWaterMan->Reset();
		if (m_szCell>0 || m_V>0)
			m_pWorld->ActivateArea(this);
		return 1;
	}
	return 0;
}

int CPhysArea::GetStatus(pe_status *_status) const
{
	if (_status->type==pe_status_pos::type_id) {
		pe_status_pos *status = (pe_status_pos*)_status;
		status->partid=status->ipart = 0;
		status->flags=status->flagsOR=status->flagsAND = 0;
		status->pos = m_offset;
		status->BBox[0] = m_BBox[0]-m_offset; 
		status->BBox[1]=  m_BBox[1]-m_offset;
		status->q = quaternionf(m_R);
		status->scale = m_scale;
		status->iSimClass = m_iSimClass;
		if (status->pMtx3x4)
			(*status->pMtx3x4 = m_R*m_scale).SetTranslation(m_offset);
		if (status->pMtx3x3)
			(Matrix33&)*status->pMtx3x3 = m_R*m_scale;
		status->pGeom=status->pGeomProxy = m_pGeom;
		return 1;
	}

	if (_status->type==pe_status_contains_point::type_id)
	{
		if (this == m_pWorld->m_pGlobalArea)
			return 1;
		return CheckPoint(((pe_status_contains_point*)_status)->pt);
	}

	if (_status->type==pe_status_area::type_id)	{
		pe_status_area *status = (pe_status_area*)_status;
		status->pLockUpdate = &m_lockRef;
		status->pSurface = m_pSurface;

		if (!is_unused(status->ptClosest)) {
			Vec3 ptloc = ((status->ptClosest-m_offset)*m_R)*m_rscale, ptres[2]={ ptloc,ortx };
			int i;
			if (m_pGeom) {
				geom_world_data gwd; 
				m_pGeom->FindClosestPoint(&gwd,i,i,ptloc,ptloc,ptres);
				contact cnt;
				box bbox; m_pGeom->GetBBox(&bbox);
				float r = (bbox.size.x+bbox.size.y+bbox.size.z)*0.03f;
				((CGeometry*)m_pGeom)->UnprojectSphere(ptres[0],r,r,&cnt);
				ptres[1] = cnt.n;
			}	else if (m_ptSpline) {
				Vec3 p0,p1,p2,v0,v1,v2;
				float tClosest,mindist = FindSplineClosestPt(ptloc, i,tClosest);
				p0 = m_ptSpline[max(0,i-1)]; p1 = m_ptSpline[i]; p2 = m_ptSpline[min(m_npt-1,i+1)];
				v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f;
				ptres[0] = (v2*tClosest+v1)*tClosest+v0;
				ptres[1] = (v2*max(0.01f,min(1.99f,2*tClosest))+v1).normalized()*(2-inrange(i+tClosest,0.001f,m_npt-0.001f));
			}
			status->ptClosest = m_R*ptres[0]*m_scale+m_offset;
			status->dirClosest = m_R*ptres[1];
			return 1;
		}

		bool bUniformForce = m_bUniform && (m_rsize.IsZero() || m_rfalloff0 == 0.f);
		bool bContained = m_pWorld->m_pGlobalArea == this || (!m_pGeom && !m_npt) || is_unused(status->ctr);
		Vec3 ptClosest = is_unused(status->ctr) ? Vec3(ZERO) : status->ctr;

		if (status->size.len2()>0.0f) {
			Vec3 box[2] = { status->ctr - status->size, status->ctr + status->size };
			if (m_pb.iMedium == 0 && !is_unused(m_pb.waterPlane.origin)) {
				if (!bContained) {
					if (box[0].x > m_BBox[1].x || m_BBox[0].x > box[1].x || 
							box[0].y > m_BBox[1].y || m_BBox[0].y > box[1].y)
						return 0;
				}
			}
			else if (!bContained && !AABB_overlap(box,m_BBox))	// non-global area, check box intersection.
				return 0;

			if (status->bUniformOnly) {
				// if non-uniform in query volume, return -1
				if (!bUniformForce)
					return -1;

				if (!bContained) {
					if (m_ptSpline)
						return -1;

					if (m_pt && m_npt) {
						{ ReadLock lock(m_lockUpdate);
							// check for no points intersecting box
							Vec2 box2[2] = { Vec2(box[0]), Vec2(box[1]) };
							Vec2 pt0( m_R * Vec3(m_pt[0].x, m_pt[0].y, ptClosest.z) * m_scale + m_offset );
							for (int i = m_npt-1; i >= 0; i--)
							{
								if (pt0.x >= box[0].x && pt0.x <= box[1].x && pt0.y >= box[0].y && pt0.y <= box[1].y)
									return -1;
								Vec2 pt1( m_R * Vec3(m_pt[i].x, m_pt[i].y, ptClosest.z) * m_scale + m_offset );
								if (box_segment_intersect(box2, pt0, pt1))
									return -1;
								pt0 = pt1;
							}
						}

						// no intersections, check whether inside or outside
						if (!CheckPoint(ptClosest, status->size.z))
							return 0;
					}
					if (m_pGeom && !m_debugGeomHash) {
						if (m_pGeom->IsConvex(0.f)) {
							// See if it contains all bounding box points.
							for (int i = 0; i < 8; i++) {
								Vec3 pt = ptClosest;
								pt.x += status->size.x * (i&1 ? 1.f : -1.f);
								pt.y += status->size.y * (i&2 ? 1.f : -1.f);
								pt.z += status->size.z * (i&4 ? 1.f : -1.f);
								if (!m_pGeom->PointInsideStatus(pt))
									return -1;
							}
						}
						else
							return -1;
					}
					bContained = true;
				}
			}
			Vec3 sz = m_BBox[1]-m_BBox[0];
			if (inrange(fabsf(sz.x)+fabsf(sz.y)+fabsf(sz.z),1e-5f,1e5f))
				ptClosest = min(max((m_BBox[0]+m_BBox[1])*0.5f,box[0]),box[1]);	// closest point in the query box to the area's center
		}

		// quick test for containment.
		else if (!bContained && !CheckPoint(status->ctr))
			return 0;

		if (bUniformForce) {
			status->gravity = m_gravity;
			status->pb = m_pb;

			if (m_bUniform==2) {
				if (!is_unused(status->gravity))
					status->gravity = m_R*status->gravity;
				if (!is_unused(status->pb.waterFlow))
					status->pb.waterFlow = m_R*status->pb.waterFlow;
			}
			return bContained ? 1 : -1;
		}

		// general force query
		if (status->bUniformOnly || is_unused(status->ctr))
			return -1;

		int iMedium = -1;
		return ApplyParams(ptClosest, status->gravity, status->vel, &status->pb, 0, 1, iMedium, 0) || !is_unused(status->gravity);
	}

	if (_status->type==pe_status_extent::type_id) {
		pe_status_extent *status = (pe_status_extent*)_status;
		status->extent = GetExtent(status->eForm);
		return 1;
	}

	if (_status->type==pe_status_random::type_id) {
		pe_status_random *status = (pe_status_random*)_status;
		GetRandomPoints(status->points, status->seed, status->eForm);
		return 1;
	}

	return CPhysicalPlaceholder::GetStatus(_status);
}

int CPhysArea::OnBBoxOverlap(const EventPhysBBoxOverlap *pEvent)
{
	if (pEvent->iForeignData[1]==PHYS_FOREIGN_ID_PHYS_AREA)	{
		CPhysicalEntity *pent = (CPhysicalEntity*)pEvent->pEntity[0];
		CPhysArea *pArea = ((CPhysArea*)pEvent->pForeignData[1]);
		float V=0; for(int i=0;i<pent->m_nParts;i++) 
			V += pent->m_parts[i].pPhysGeom->V*cube(pent->m_parts[i].scale)*isneg(-((int)pent->m_parts[i].flags & geom_floats));
		if (V>pArea->m_V*pArea->m_minObjV)
			pArea->m_pWorld->ActivateArea((CPhysArea*)pEvent->pForeignData[1]);
	}
	return 0;
}

void CPhysArea::Update(float dt)
{
	int iCaller = get_iCaller();
	Release(); // since it was addreffed by an update request
	if (m_bDeleted)
		return;
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	PHYS_AREA_PROFILER(this)
	int bAwakeEnv = 0;

	Vec3 boxPrev[2] = { m_BBox[0], m_BBox[1] };

	if (m_V>0 && !m_pContainer) {
		ray_hit rhit;
		CTriMesh *pContainer = 0;
		if (m_pWorld->RayWorldIntersection(m_offset0,m_pWorld->m_pGlobalArea->m_gravity,ent_static|ent_sleeping_rigid|ent_rigid,rwi_pierceability(15),&rhit,1) && rhit.pCollider) 
			if (CGeometry *pGeom = (CGeometry*)((CPhysicalEntity*)rhit.pCollider)->m_parts[rhit.ipart].pPhysGeom->pGeom)
				if (pContainer = (CTriMesh*)pGeom->GetTriMesh()) {
					((CPhysicalEntity*)rhit.pCollider)->GetLocTransform(rhit.ipart,m_qtsContainer.t,m_qtsContainer.q,m_qtsContainer.s,this);
					if (!((CPhysicalPlaceholder*)rhit.pCollider)->m_iSimClass) {
						CPhysicalEntity **pents;
						geom_world_data gwd[2];
						intersection_params ip;	geom_contact *pcont;
						const int MAX_GEOMS = 64;
						Vec3 bboxes[MAX_GEOMS][2];
						int igeom=0,ngeoms=0,nNewParts,listBuf[MAX_GEOMS*3],*list0=listBuf,*list1=listBuf+MAX_GEOMS; 
						m_nContainerParts=nNewParts=1; m_pContainerParts = new int[MAX_GEOMS];
						m_pContainerParts[0] = m_pWorld->GetPhysicalEntityId(rhit.pCollider)<<8 | rhit.ipart;
						memcpy(bboxes[0],((CPhysicalEntity*)rhit.pCollider)->m_parts[rhit.ipart].BBox,sizeof(Vec3)*2);
						gwd[0].offset=m_qtsContainer.t; gwd[0].R=Matrix33(m_qtsContainer.q); gwd[0].scale=m_qtsContainer.s;
						ip.bStopAtFirstTri=true; ip.bNoAreaContacts=true;
						extern int g_bBruteforceTriangulation;
						int bft=g_bBruteforceTriangulation; g_bBruteforceTriangulation=1;

						do {
							int i,j,i1,j1,ngeomsNew=0,*listNew=listBuf+MAX_GEOMS*2;
							if (nNewParts) {
								int nents = m_pWorld->GetEntitiesAround(bboxes[m_nContainerParts-1][0],bboxes[m_nContainerParts-1][1],pents,ent_static,(CPhysicalEntity*)this,0,iCaller);
								for(i=0;i<nents;i++) for(j=0;j<pents[i]->GetUsedPartsCount(iCaller);j++) 
									if (pents[i]->m_parts[j1=pents[i]->GetUsedPart(iCaller,j)].flags & geom_floats)	{
										Vec3 partBBox[2], *pBBox=pents[i]->GetPartBBox(j1, partBBox, this);
										for(i1=0; i1<m_nContainerParts && !AABB_overlap(bboxes[i1],pBBox); i1++);
										if (i1<m_nContainerParts) {
											for(i1=0,j1=pents[i]->m_id<<8|j1; i1<m_nContainerParts && m_pContainerParts[i1]!=j1; i1++);
											if (i1==m_nContainerParts && ngeomsNew<MAX_GEOMS)	{
												for(i1=ngeomsNew++-1; i1>=0 &&	listNew[i1]>j1; i1--)
													listNew[i1+1]=listNew[i1];
												listNew[i1+1] = j1; 
											}
										}
									}
								nNewParts=0;
							}
							if (ngeomsNew) {
								ngeoms = unite_lists(list0+igeom,ngeoms-igeom, listNew,ngeomsNew, list1,MAX_GEOMS);
								int *t=list0; list0=list1; list1=t; igeom=0; ngeomsNew=0;
							}
							for(;igeom<ngeoms && !nNewParts;igeom++) if (CPhysicalEntity *pent = (CPhysicalEntity*)m_pWorld->GetPhysicalEntityById(list0[igeom]>>8)) {
								Quat q; pent->GetLocTransform(j1=list0[igeom]&0xFF,gwd[1].offset,q,gwd[1].scale,this); gwd[1].R=Matrix33(q);
								if (pContainer->Intersect(pent->m_parts[j1].pPhysGeomProxy->pGeom,gwd,gwd+1,&ip,pcont))	{
									pContainer->Flip();
									IGeometry *pMesh = ((CGeometry*)pent->m_parts[j1].pPhysGeomProxy->pGeom)->GetTriMesh(0);
									if (pContainer->Subtract(pMesh,gwd,gwd+1,0)) {
										memcpy(bboxes+m_nContainerParts, pent->m_parts[j1].BBox, sizeof(Vec3)*2);
										m_pContainerParts[m_nContainerParts++] = pent->m_id<<8|j1; nNewParts++; 
									}
									if (pMesh && pMesh!=pent->m_parts[j1].pPhysGeomProxy->pGeom)
										pMesh->Release();
									pContainer->Flip();
								}
							}
						} while(igeom<ngeoms && m_nContainerParts<MAX_GEOMS);
						g_bBruteforceTriangulation = bft;
					}
				}
		if (pContainer) { 
			if (m_szCell>0 && m_qtsContainer.s!=1.0f) {
				for(int i=0;i<pContainer->m_nVertices;i++) pContainer->m_pVertices[i]*=m_qtsContainer.s;
				pContainer->RebuildBVTree();
				m_qtsContainer.s = 1.0f;
			}
			WriteLock lock(m_lockUpdate);
			m_pContainer = pContainer;
			if (((CPhysicalPlaceholder*)rhit.pCollider)->m_iSimClass)
				(m_pContainerEnt = (CPhysicalEntity*)rhit.pCollider)->AddRef();
			m_idpartContainer = rhit.partid;
		}
	}

	if (m_pContainer) {
		QuatTS qtParent = m_qtsContainer.GetInverted();
		Vec3 offs0 = qtParent*m_offset0, org0 = qtParent*m_pb.waterPlane.origin;
		if (m_pContainerEnt) {
			for(int i=m_pContainerEnt->m_nParts-1;i>0 && m_pContainerEnt->m_parts[i].id!=m_idpartContainer;i--);
			m_pContainerEnt->GetLocTransform(0,m_qtsContainer.t,m_qtsContainer.q,m_qtsContainer.s,this);
			m_offset0 = m_qtsContainer*offs0;	m_pb.waterPlane.origin = m_qtsContainer*org0;
		}
		Vec3 g = is_unused(m_pb.waterPlane.origin) ? m_pWorld->m_pGlobalArea->m_gravity.normalized() : -m_pb.waterPlane.n;
		QuatTS loc = m_qtsContainer.GetInverted();
		QuatT qtBorder;
		Vec3 partBBox[2];
		float depth,V=m_V*cube(loc.s),V0=V;
		static IGeometry **g_pGeoms;
		static int g_nGeomsAlloc=0;
		static QuatTS *g_qtsGeoms;
		CPhysicalEntity **pents;
		int ngeoms, nents=m_pWorld->GetEntitiesAround(m_BBox[0], m_BBox[1], pents, ent_static|ent_sleeping_rigid|ent_rigid, (CPhysicalEntity*)this, 0,iCaller);
		for(int i=ngeoms=0;i<nents;i++) {
			float Vent=0;	int j1;
			for(int j=0;j<pents[i]->GetUsedPartsCount(iCaller);j++) {
				j1 = pents[i]->GetUsedPart(iCaller,j); 
				Vent += pents[i]->m_parts[j1].pPhysGeom->V*cube(pents[i]->m_parts[j1].scale);
			}
			if (Vent>m_V*0.001f) 
				for(int j=0;j<pents[i]->GetUsedPartsCount(iCaller);j++) 
					if (pents[i]->m_parts[j1=pents[i]->GetUsedPart(iCaller,j)].flags & geom_floats && AABB_overlap(m_BBox,pents[i]->GetPartBBox(j1,partBBox,this))) {
						int i1,id=pents[i]->m_id<<8|j1; 
						for(i1=0; i1<m_nContainerParts && m_pContainerParts[i1]!=id; i1++);
						if (i1<m_nContainerParts || pents[i]==m_pContainerEnt && pents[i]->m_parts[j1].id==m_idpartContainer)
							continue;
						QuatTS loc1; pents[i]->GetLocTransform(j1,loc1.t,loc1.q,loc1.s,this);
						if (ngeoms>=g_nGeomsAlloc) {
							ReallocateList(g_pGeoms,ngeoms,g_nGeomsAlloc+64);
							ReallocateList(g_qtsGeoms,ngeoms,g_nGeomsAlloc+=64);
						}
						g_pGeoms[ngeoms] = pents[i]->m_parts[j1].pPhysGeomProxy->pGeom;
						g_qtsGeoms[ngeoms++] = loc*loc1;
					}
		}

		Vec2 *ptnew;
		float Vsubmerged; Vec3 comSubmerged;
		int npt = ((CTriMesh*)m_pContainer)->FloodFill(loc*m_offset0,V,loc.q*g,V*m_accuracyV, ptnew,qtBorder,depth,m_bConvexBorder, g_pGeoms,g_qtsGeoms,ngeoms, Vsubmerged,comSubmerged);
		if (!npt)
			return;
		float e = sqr(max(m_pWorld->m_vars.maxContactGap,m_borderPad*0.3f));
		int j=1; for(int i=1;i<npt;j+=isneg(e-(ptnew[i++]-ptnew[j-1]).GetLength2())) ptnew[j]=ptnew[i];
		npt = j-isneg((ptnew[0]-ptnew[j-1]).GetLength2()-e);
		for(int i=j=1;i<npt;i++) {
			Vec2 dir0=ptnew[i]-ptnew[j-1], dir1=ptnew[i+1&i+1-npt>>31]-ptnew[i];
			ptnew[j]=ptnew[i]; j+=isneg(sqr_signed(dir0*dir1)-(dir0*dir0)*(dir1*dir1)*sqr(0.996f));
		}	
		if ((npt=j)<3)
			return;
		if (m_borderPad) {
			Vec2 pt0=ptnew[0],ptprev=ptnew[npt-1],ptcur,n; 
			for(int i=0; i<npt-1; i++,ptprev=ptcur) {
				ptcur=ptnew[i]; ptnew[i] += ((ptnew[i]-ptprev).GetNormalized()+(ptnew[i]-ptnew[i+1]).GetNormalized()).GetNormalized()*m_borderPad;
			}	ptnew[npt-1] += ((ptnew[npt-1]-ptprev).GetNormalized()+(ptnew[npt-1]-pt0).GetNormalized()).GetNormalized()*m_borderPad;
		}
		{ WriteLock lock(m_lockUpdate);
			if (m_pt) delete[] m_pt; m_pt=ptnew;
			m_npt = npt;
			if (V!=V0)
				m_V = V*cube(m_qtsContainer.s);
			ProcessBorder();
			loc = m_qtsContainer*qtBorder;
			m_R=Matrix33(loc.q); m_offset=loc.t; m_rscale=1.0f/(m_scale=loc.s);
			if (!is_unused(m_pb.waterPlane.origin))
				m_pb.waterPlane.n = -g;
			m_BBox[0]=m_BBox[1] = loc*Vec3(m_pt[0]);
			float diff = m_pb.waterPlane.n*(m_BBox[0]-m_pb.waterPlane.origin);
			if (diff>m_sleepVel*dt || (m_moveAccum+=diff)>m_sleepVel*dt) {
				m_moveAccum=0; m_nSleepFrames=0;
			}	else if ((++m_nSleepFrames&3)==3)
				m_moveAccum = 0; 
			bAwakeEnv = isneg(m_nSleepFrames-4);
			m_pb.waterPlane.origin += m_pb.waterPlane.n*diff;
			m_zlim[0] = -depth; m_zlim[1] = m_szCell*2*m_rscale;
			for(int i=0;i<m_npt;i++) {
				Vec3 pt[] = { loc*(Vec3(m_pt[i])+Vec3(0,0,m_zlim[0])), loc*(Vec3(m_pt[i])+Vec3(0,0,m_zlim[1])) };
				m_BBox[0]=min(min(m_BBox[0],pt[0]),pt[1]); m_BBox[1]=max(max(m_BBox[1],pt[0]),pt[1]);
			}
			m_debugGeomHash = -1;
			pe_params_bbox pbb; pbb.BBox[0]=m_BBox[0]; pbb.BBox[1]=m_BBox[1];
			if (!m_pTrigger) {
				pe_params_pos pp; pp.iSimClass = 6;
				pe_params_foreign_data pfd;	pfd.iForeignData=PHYS_FOREIGN_ID_PHYS_AREA; pfd.pForeignData=this; pfd.iForeignFlags=ent_rigid;
				m_pTrigger = m_pWorld->CreatePhysicalPlaceholder(PE_STATIC,&pbb);
				m_pTrigger->SetParams(&pp); m_pTrigger->SetParams(&pfd);
				m_pWorld->AddEventClient(EventPhysBBoxOverlap::id,(int(*)(const EventPhys*))OnBBoxOverlap,0,1000.0f);
			}	else
				m_pTrigger->SetParams(&pbb);
		}

		if (m_pContainerEnt && !m_pMassGeom) {
			sphere sph; sph.r = (m_BBox[1]-m_BBox[0]).len()*0.25f;
			sph.center = (m_qtsContainer*comSubmerged-m_pContainerEnt->m_pos)*m_pContainerEnt->m_qrot; 
			IGeometry *pSph = m_pWorld->CreatePrimitive(sphere::type,&sph);
			m_pMassGeom = m_pWorld->RegisterGeometry(pSph); pSph->Release();
			pe_geomparams gp;	gp.flags=gp.flagsCollider=0;
			gp.mass=(m_pMassGeom->V=Vsubmerged)*m_pb.waterDensity;
			m_pContainerEnt->AddGeometry(m_pMassGeom,&gp,100); m_pMassGeom->nRefCount--;
		}	else if (m_pContainerEnt && m_pMassGeom) {
			pe_params_part pp; pp.partid=100;
			pp.mass = (m_pMassGeom->V=Vsubmerged)*m_pb.waterDensity;
			m_pMassGeom->origin = (m_qtsContainer*comSubmerged-m_pContainerEnt->m_pos)*m_pContainerEnt->m_qrot; 
			m_pContainerEnt->SetParams(&pp);
		}

		if (m_szCell<=0) {
			Vec3 *pt; int *pIdx;
			if (m_pSurface) {
				if (m_npt>m_nptAlloc) {
					ReallocateList(m_pSurface->m_pIndices,0,m_npt*6*(sizeof(int)/sizeof(index_t)));
					ReallocateList(m_pSurface->m_pNormals,0,m_npt*2);
					ReallocateList(m_pSurface->m_pVertices.data,0,m_nptAlloc=m_npt);
				}
				pIdx=(int*)m_pSurface->m_pIndices; pt=m_pSurface->m_pVertices.data;
			} else {
				pIdx=new int[m_npt*6]; pt=new Vec3[m_nptAlloc=m_npt];
			}
			for(int i=0;i<m_npt;i++) pt[i]=Vec3(m_pt[i]);
			MARK_UNUSED m_pt[m_npt].x;
			int nTris = TriangulatePoly(m_pt,m_npt+1,pIdx,m_npt*2);
			if (!nTris)	{
				delete[] pt; delete[] pIdx; return;
			}
			if (!m_pSurface) {
				m_pSurface = (CTriMesh*)m_pWorld->CreateMesh(pt,pIdx,0,0,nTris,mesh_SingleBB|mesh_shared_vtx|mesh_shared_idx|mesh_no_vtx_merge|mesh_no_filter,0);
				m_pSurface->m_flags &= ~(mesh_shared_vtx|mesh_shared_idx);
			}	else {
				if (sizeof(int)!=sizeof(index_t))
					for(int i=0;i<nTris*3;i++) m_pSurface->m_pIndices[i]=pIdx[i];
				m_pSurface->m_nVertices=m_npt; m_pSurface->m_nTris=nTris;
				if ((m_pWorld->m_vars.iDrawHelpers & (ent_areas<<8|pe_helper_geometry))==(ent_areas<<8|pe_helper_geometry))
					for(int i=0;i<nTris;i++) m_pSurface->RecalcTriNormal(i);
			}
		}

		if (m_pWaterMan) {
			if (m_pContainerEnt) {
				m_pWaterMan->SetViewerPos((m_BBox[0]+m_BBox[1])*0.5f);
				//m_pWaterMan->OffsetWaterLevel((m_qtsContainer*org0-m_pb.waterPlane.origin)*m_pb.waterPlane.n,dt);
			} else
				m_pWaterMan->UpdateWaterLevel(m_pb.waterPlane, dt);
		}
		m_pWorld->RepositionArea(this, boxPrev);
	}

	if (m_szCell>0 && m_pt && !m_pWaterMan) {
		Vec2 BBox[] = { m_pt[0],m_pt[0] };
		for(int i=0;i<m_npt;i++) {
			BBox[0]=min(BBox[0],m_pt[i]); BBox[1]=max(BBox[1],m_pt[i]);
		}
		float dim = max(BBox[1].x-BBox[0].x,BBox[1].y-BBox[0].y)*m_scale;
		int nCells = float2int(dim*(1.0f+m_sizeReserve)/m_szCell)+1|1;
		if (nCells<=1)
			return;
		WriteLock lock(m_pWorld->m_lockWaterMan);
		m_pWaterMan = new CWaterMan(m_pWorld,this);
		m_pWaterMan->m_prev = m_pWorld->m_pWaterMan;
		if (m_pWorld->m_pWaterMan) {
			if (m_pWaterMan->m_next = m_pWorld->m_pWaterMan->m_next)
				m_pWaterMan->m_next->m_prev = m_pWaterMan;
			m_pWorld->m_pWaterMan->m_next = m_pWaterMan;
		} else
			m_pWorld->m_pWaterMan = m_pWaterMan;
		pe_params_waterman pwm;
		pwm.nExtraTiles = 0;
		pwm.tileSize = (pwm.nCells=nCells)*m_szCell;
		Vec3 org = m_R*Vec3(BBox[0]+BBox[1]-Vec2(pwm.tileSize,pwm.tileSize))*0.5f*m_scale+m_offset;
		m_pb.waterPlane.origin = org+m_pb.waterPlane.n*(m_pb.waterPlane.n*(m_pb.waterPlane.origin-org));
		pwm.posViewer = m_R*Vec3(BBox[0]+BBox[1])*0.5f*m_scale+m_offset;
		memcpy(&(params_wavesim&)pwm, &m_waveSim, sizeof(m_waveSim));
		m_pWaterMan->SetParams(&pwm);
	}

	if (m_pWaterMan) {
		pe_status_waterman swm;	m_pWaterMan->GetStatus(&swm);
		int nvtx = sqr(swm.nCells)*sqr(swm.nTiles*2+1);
		if (!m_pSurface) {
			Vec3 *pvtx = new Vec3[nvtx+(nvtx*2)/3+1];	memset(pvtx,0,nvtx*sizeof(Vec3));
			Vec3_tpl<index_t> *pTris = new Vec3_tpl<index_t>[sqr(swm.nCells-1)*2];
			for(int i=0;i<swm.nCells-1;i++) for(int j=0;j<swm.nCells-1;j++)	{
				pTris[(i*(swm.nCells-1)+j)*2  ].Set(i*swm.nCells+j,i*swm.nCells+j+1,(i+1)*swm.nCells+j); 
				pTris[(i*(swm.nCells-1)+j)*2+1].Set(i*swm.nCells+j+1,(i+1)*swm.nCells+j+1,(i+1)*swm.nCells+j); 
			}
			m_pSurface = (CTriMesh*)m_pWorld->CreateMesh(pvtx,(index_t*)pTris,0,0,sqr(swm.nCells-1)*2,mesh_SingleBB|mesh_shared_vtx|mesh_shared_idx|mesh_no_vtx_merge|mesh_no_filter,0);
			m_pSurface->m_flags &= ~(mesh_shared_vtx|mesh_shared_idx);
		}
		m_pWaterMan->GenerateSurface(0,m_pSurface->m_pVertices.data,(Vec3_tpl<index_t>*)m_pSurface->m_pIndices,m_pt,m_npt,(swm.origin-m_offset)*m_R,(Vec2*)(m_pSurface->m_pVertices.data+nvtx));
		bAwakeEnv |= m_pWaterMan->IsActive();
	}

	if (bAwakeEnv && (m_pWorld->m_idStep&3)==3)	{
		CPhysicalEntity **pents;
		for(int i=m_pWorld->GetEntitiesAround(m_BBox[0],m_BBox[1],pents,ent_sleeping_rigid,0,0,iCaller)-1;i>=0;i--)
			for(int j=0;j<pents[i]->m_nParts;j++) if (pents[i]->m_parts[j].flags & geom_floats) {
				pents[i]->Awake(); break;
			}
	}

	SignalEventAreaChange(m_pWorld, this, boxPrev);
}


void CPhysArea::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{	
	if (m_bDeleted)
		return;
	//ReadLock lock(m_lockUpdate);
	if (flags & pe_helper_bbox) {
		int i,j;
		Vec3 sz,center,pt[8];
		center = (m_BBox[1]+m_BBox[0])*0.5f;
		sz = (m_BBox[1]-m_BBox[0])*0.5f;
		for(i=0;i<8;i++)
			pt[i] = Vec3(sz.x*((i<<1&2)-1),sz.y*((i&2)-1),sz.z*((i>>1&2)-1))+center;
		for(i=0;i<8;i++) for(j=0;j<3;j++) if (i&1<<j)
			pRenderer->DrawLine(pt[i],pt[i^1<<j],m_iSimClass);
	}

	if (flags & pe_helper_geometry)	{
		if (m_ptSpline) {
			int i,j; float t,f,x,y,dist=0;
			const float cos15=0.96592582f,sin15=0.25881904f;
			Vec3 p0,p1,p2,v0,v1,v2,pt,pt0,ptc0,ptc1;
			Vec3_tpl<Vec3> axes;
			for(i=0; i<m_npt; i++) {
				p0 = m_R*m_ptSpline[max(0,i-1)]*m_scale+m_offset; 
				p1 = m_R*m_ptSpline[i]*m_scale+m_offset; 
				p2 = m_R*m_ptSpline[min(m_npt-1,i+1)]*m_scale+m_offset;
				v2 = (p0+p2)*0.5f-p1; v1 = p1-p0; v0 = (p0+p1)*0.5f;
				for(t=0.1f,pt0=(p0+p1)*0.5f; t<=1.01f; t+=0.1f,pt0=pt) {
					pt = (v2*t+v1)*t+v0;
					pRenderer->DrawLine(pt0,pt,m_iSimClass);
					if ((dist += (pt-pt0).len())>0.2f) {
						dist -= 0.2f; axes.z = (v2*(t*2)+v1).normalized();
						axes.x = axes.z.GetOrthogonal().normalized(); axes.y = axes.z^axes.x;
						for(j=0,x=cos15*m_zlim[0],y=sin15*m_zlim[0],ptc0=pt+axes.x*m_zlim[0]; j<24; j++,ptc0=ptc1) {
							ptc1 = pt+axes.x*x+axes.y*y;
							pRenderer->DrawLine(ptc0,ptc1,m_iSimClass);
							f=x; x=x*cos15-y*sin15; y=y*cos15+f*sin15;
						}
					}
				}
			}
		}
		if (m_pt) {
			int i0,i1,j,hash=0;
			for(j=0;j<2;j++) for(i0=0;i0<m_npt;i0++) {
				i1 = i0+1 & i0-m_npt+1>>31;
				pRenderer->DrawLine(m_R*Vec3(m_pt[i0].x,m_pt[i0].y,m_zlim[j])*m_scale+m_offset,
														m_R*Vec3(m_pt[i1].x,m_pt[i1].y,m_zlim[j])*m_scale+m_offset, m_iSimClass);
				((hash ^= ((Vec3i*)m_pt)[i0].x*3) ^= ((Vec3i*)m_pt)[i0].y*5) ^= ((Vec3i*)m_pt)[i0].z*7;
			}
			hash+=iszero(hash+1);	hash+=iszero(hash);
			PREFAST_SUPPRESS_WARNING(6240);
			if (flags & pe_helper_collisions && (!m_pGeom || m_debugGeomHash!=hash) && sizeof(int)==sizeof(index_t)) {
				WriteLock lock(m_lockUpdate);
				if (m_pGeom) m_pGeom->Release();
				Vec3 *pt = new Vec3[m_npt*2];
				Vec3i *pTris = new Vec3i[m_npt*6];
				MARK_UNUSED m_pt[m_npt].x;
				int nTris = TriangulatePoly(m_pt,m_npt+1,(int*)pTris,m_npt*2);
				float zup = !is_unused(m_pb.waterPlane.origin) && (m_pb.waterPlane.n*m_R).z>0.998f ? ((m_pb.waterPlane.origin-m_offset)*m_R).z*m_rscale : m_zlim[1];
				for(j=0;j<m_npt;j++) 
					pt[j](m_pt[j].x,m_pt[j].y,m_zlim[0]), pt[j+m_npt](m_pt[j].x,m_pt[j].y,zup);
				for(j=0;j<nTris;j++)
					pTris[nTris+j]=Vec3i(pTris[j].y,pTris[j].x,pTris[j].z), pTris[j]+=Vec3i(m_npt);
				for(j=0;j<m_npt-1;j++)
					pTris[nTris+j<<1](j,j+1,m_npt+j), pTris[(nTris+j)*2+1](m_npt+j+1,m_npt+j,j+1);
				pTris[nTris+j<<1](j,0,m_npt+j); pTris[(nTris+j)*2+1](m_npt,m_npt+j,0);
				m_pGeom = m_pWorld->CreateMesh(pt,strided_pointer<ushort>((ushort*)pTris,sizeof(int)),0,0,(m_npt+nTris)*2,mesh_SingleBB|mesh_shared_vtx|mesh_shared_idx|mesh_no_vtx_merge|mesh_no_filter,0);
				mesh_data *pmd = (mesh_data*)m_pGeom->GetData();
				pmd->flags &= ~(mesh_shared_vtx|mesh_shared_idx);
				m_debugGeomHash = hash;
			}
		}
		geom_world_data gwd;
		gwd.R = m_R;
		gwd.offset = m_offset;
		gwd.scale = m_scale;
		if (m_pGeom)
			m_pGeom->DrawWireframe(pRenderer,&gwd, flags>>16, m_iSimClass);
		if (m_pSurface)
			m_pSurface->DrawWireframe(pRenderer,&gwd, flags>>16, m_iSimClass);
	}

	if (flags & 16 && m_pWaterMan)
		m_pWaterMan->DrawHelpers(pRenderer);

	if (flags & pe_helper_lattice && m_pContainer) {
		geom_world_data gwd;
		gwd.R = Matrix33(m_qtsContainer.q);
		gwd.offset = m_qtsContainer.t;
		gwd.scale = m_qtsContainer.s;
		m_pContainer->DrawWireframe(pRenderer,&gwd, flags>>16, m_iSimClass);	
	}
}


void CPhysArea::GetMemoryStatistics(ICrySizer *pSizer) const
{
	pSizer->AddObject(this, sizeof(CPhysArea));
	if (m_pt) {
		pSizer->AddObject(m_pt, sizeof(m_pt[0])*(m_npt+1));
		pSizer->AddObject(m_idxSort[0], sizeof(m_idxSort[0][0])*m_npt);
		pSizer->AddObject(m_idxSort[1], sizeof(m_idxSort[1][0])*m_npt);
		pSizer->AddObject(m_pMask, sizeof(m_pMask[0])*((m_npt-1>>5)+1)*(MAX_PHYS_THREADS+1));
	}
	if (m_ptSpline)
		pSizer->AddObject(m_ptSpline, sizeof(m_ptSpline[0])*m_npt);
	if (m_pGeom) {
		if (m_pGeom->AddRef()==2)
			m_pGeom->GetMemoryStatistics(pSizer);
		m_pGeom->Release();
	}
	if (m_pFlows)
		pSizer->AddObject(m_pFlows, sizeof(m_pFlows[0])*m_npt);
}


static inline void swap(float *pval,int *pidx, int i1,int i2) {	
	float x=pval[i1]; pval[i1]=pval[i2]; pval[i2]=x;
	int i=pidx[i1]; pidx[i1]=pidx[i2]; pidx[i2]=i;
}
static void qsort(float *pval,int *pidx, int ileft,int iright)
{
	if (ileft>=iright) return;
	int i,ilast; 
	swap(pval,pidx, ileft,ileft+iright>>1);
	for(ilast=ileft,i=ileft+1; i<=iright; i++)
	if (pval[i] < pval[ileft])
		swap(pval,pidx, ++ilast,i);
	swap(pval,pidx, ileft,ilast);

	qsort(pval,pidx, ileft,ilast-1);
	qsort(pval,pidx, ilast+1,iright);
}

void CPhysArea::ProcessBorder()
{
	if (m_pt) {
		if (m_idxSort[0]) delete[] m_idxSort[0];
		if (m_idxSort[1]) delete[] m_idxSort[1];
		if (m_pMask) delete[] m_pMask;
		m_idxSort[0] = new int[m_npt];
		m_idxSort[1] = new int[m_npt];
		memset(m_pMask = new unsigned int[((m_npt-1>>5)+1)*(MAX_PHYS_THREADS+1)],0,((m_npt-1>>5)+1)*(MAX_PHYS_THREADS+1)*sizeof(int));
		float *xstart = new float[m_npt];

		for(int iend=0; iend<2; iend++)	{
			for(int i=0;i<m_npt;i++) {
				m_idxSort[iend][i] = i; 
				int j = i+1 & i-m_npt+1>>31;
				int k = isneg(m_pt[i].x-m_pt[j].x)^iend;
				xstart[i] = m_pt[i&-k | j&~-k].x;
			}	
			qsort(xstart,m_idxSort[iend], 0,m_npt-1);
		}
		delete[] xstart;
	}
}

IPhysicalEntity *CPhysicalWorld::AddArea(Vec3 *pt,int npt, float zmin,float zmax, const Vec3 &pos,const quaternionf &q,float scale, 
																				 const Vec3 &normal, int *pTessIdx,int nTessTris,Vec3 *pFlows)
{
	if (npt<=0)
		return 0;
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea)	{
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea *pArea = new CPhysArea(this);
	int i,j,k;
	float seglen,len,maxdist;
	Vec3 n,p0,p1,BBox[2],sz,center,nscrew,axisx;
	Matrix33 C;
	m_nAreas++; m_nTypeEnts[PE_AREA]++;

	len=seglen = (pt[0]-pt[npt-1]).len();
	pArea->m_offset0 = (pt[0]+pt[npt-1])*seglen;
	for(i=0; i<npt-1; i++) {
		len += seglen=(pt[i+1]-pt[i]).len();
		pArea->m_offset0 += (pt[i+1]+pt[i])*seglen;
	}
	if (len<1e-8f) {
		delete pArea; return 0;
	}
	pArea->m_offset0 /= len*2;
	nscrew = pt[npt-1]-pArea->m_offset0 ^ pt[0]-pArea->m_offset0;
	for(i=0,C.SetZero(); i<npt-1; i++) { 
		p0 = pt[i]-pArea->m_offset0; p1 = pt[i+1]-pArea->m_offset0;	seglen = (p1-p0).len();
		nscrew += p0^p1;
		for(j=0;j<3;j++) for(k=0;k<3;k++)
			C(j,k) += seglen*(2*(p0[j]*p0[k]+p1[j]*p1[k]) + p0[j]*p1[k]+p0[k]*p1[j]);
	}

	real eval[3];
	Vec3r Cbuf[3],eigenAxes[3];
	for(i=0;i<3;i++) Cbuf[i]=C.GetRow(i);
	matrix Cmtx(3,3,mtx_symmetric,Cbuf[0]),eigenBasis(3,3,0,eigenAxes[0]); 
	Cmtx.jacobi_transformation(eigenBasis,eval,0);
	if (normal.len2()==0) {
		n = eigenAxes[idxmin3(eval)]; n *= (i=sgnnz(n*nscrew));
		axisx = eigenAxes[idxmax3(eval)]*i;
	}	else {
		n = normal; axisx = eigenAxes[idxmax3(eval)];
		(axisx -= n*(axisx*n)).normalize();
	}

	/*for(i=0,maxdet=0;i<3;i++) {
		det = C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],dec_mod3[i]) - 
					C(dec_mod3[i],inc_mod3[i])*C(inc_mod3[i],dec_mod3[i]);
		if (fabs_tpl(det)>fabs_tpl(maxdet)) { 
			maxdet=det; j=i; 
		}
	}
	det = 1/maxdet; n[j] = 1;
	n[inc_mod3[j]] = -(C(inc_mod3[j],j)*C(dec_mod3[j],dec_mod3[j]) - C(dec_mod3[j],j)*C(inc_mod3[j],dec_mod3[j]))*det;
	n[dec_mod3[j]] = -(C(inc_mod3[j],inc_mod3[j])*C(dec_mod3[j],j) - C(dec_mod3[j],inc_mod3[j])*C(inc_mod3[j],j))*det;
	n.normalize(); n *= sgnnz(n*nscrew);*/

	pArea->m_R0.SetColumn(2,n);
	pArea->m_R0.SetColumn(0,axisx);
	pArea->m_R0.SetColumn(1,n^axisx);
	pArea->m_pt = new Vec2[(pArea->m_npt=npt)+1];
	pArea->m_scale = scale;
	pArea->m_rscale = 1/pArea->m_scale;
	BBox[0] = Vec3(VMAX); BBox[1] = Vec3(VMIN);
	for(i=0,maxdist=0;i<npt;i++) {
		p0 = (pt[i]-pArea->m_offset0)*pArea->m_R0;
		pArea->m_pt[i] = Vec2(p0); maxdist = max(maxdist,fabs_tpl(p0.z));
		BBox[0].x=min_safe(BBox[0].x,pArea->m_pt[i].x); BBox[0].y=min_safe(BBox[0].y,pArea->m_pt[i].y);
		BBox[1].x=max_safe(BBox[1].x,pArea->m_pt[i].x);	BBox[1].y=max_safe(BBox[1].y,pArea->m_pt[i].y);
	}
	BBox[0].z=pArea->m_zlim[0] = zmin; BBox[1].z=pArea->m_zlim[1] = zmax;
	pArea->m_size0 = (BBox[1]-BBox[0])*0.5f;
	pArea->ProcessBorder();

	pArea->m_offset = pArea->m_offset0+pos;
	pArea->m_R = Matrix33(q)*pArea->m_R0;
	memset(pArea->m_pMask = new unsigned int[((npt-1>>5)+1)*(MAX_PHYS_THREADS+1)],0,((npt-1>>5)+1)*(MAX_PHYS_THREADS+1)*sizeof(int));

	sz = Matrix33(pArea->m_R).Fabs()*pArea->m_size0*scale;
	center = pArea->m_offset+pArea->m_R*(BBox[0]+BBox[1])*(pArea->m_scale*0.5f);
	pArea->m_BBox[0] = center-sz; 
	pArea->m_BBox[1] = center+sz;
	pArea->m_debugGeomHash = -1;

	if (maxdist>0.05f || pFlows) {
		int nTris, *pidx=new int[npt*3];
		Vec3 *pvtx=new Vec3[npt];
		MARK_UNUSED pArea->m_pt[npt].x;
		if (pTessIdx && nTessTris)
			memcpy(pidx,pTessIdx,(nTris=nTessTris)*3*sizeof(int));
		else
			nTris = TriangulatePoly(pArea->m_pt,npt+1,pidx,npt*3);
		if (pFlows)
			pArea->m_pFlows = new Vec3[npt];
		for(i=0;i<npt;i++) {
			pvtx[i] = (pt[i]-pArea->m_offset0)*pArea->m_R0;
			if (pFlows)
				pArea->m_pFlows[i] = pFlows[i]*pArea->m_R0;
		}
		if (pArea->m_pGeom = CreateMesh(pvtx,pidx,0,0,nTris, mesh_SingleBB|mesh_shared_vtx)) {
			((CTriMesh*)pArea->m_pGeom)->m_flags &= ~(mesh_shared_vtx|mesh_shared_idx);
			pArea->m_pGeom->PrepareForRayTest(0);	pArea->m_debugGeomHash = 0;
		}
		delete[] pidx;
	} else
		pArea->m_zlim[1] += maxdist+0.01f;

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}


IPhysicalEntity *CPhysicalWorld::AddArea(IGeometry *pGeom, const Vec3& pos,const quaternionf &q,float scale)
{
	assert(pGeom);
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea) {
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea *pArea = new CPhysArea(this);
	m_nAreas++;	m_nTypeEnts[PE_AREA]++;
	pArea->m_pGeom = pGeom;
	pArea->m_offset = pos;
	pArea->m_R = Matrix33(q);
	pArea->m_rscale = 1.0f/(pArea->m_scale=scale);

	pArea->m_offset0.zero();
	pArea->m_R0.SetIdentity();

	box abox;
	Vec3 sz,center;
	pGeom->GetBBox(&abox);
	abox.Basis *= pArea->m_R.T();
	sz = (abox.size*abox.Basis.Fabs())*scale;
	center = pos + q*abox.center*scale;
	pArea->m_BBox[0] = center-sz;
	pArea->m_BBox[1] = center+sz;
	pGeom->PrepareForRayTest(0);

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}


IPhysicalEntity *CPhysicalWorld::AddArea(Vec3 *pt,int npt, float r, const Vec3 &pos,const quaternionf &q,float scale)
{
	if (npt<=0)
		return 0;
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea) {
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
	}

	CPhysArea *pArea = new CPhysArea(this);
	m_nAreas++;	m_nTypeEnts[PE_AREA]++;
	pArea->m_offset = pos;
	pArea->m_R = Matrix33(q);
	pArea->m_rscale = 1.0f/(pArea->m_scale=scale);
	pArea->m_offset0.zero();
	pArea->m_R0.SetIdentity();
	pArea->m_zlim[0] = r;

	pArea->m_ptSpline = new Vec3[pArea->m_npt = npt];
	pArea->m_BBox[0]=pArea->m_BBox[1] = q*pt[0]*scale+pos;
	for(int i=0;i<npt;i++) {
		pArea->m_ptSpline[i] = pt[i];
		Vec3 ptw = q*pt[i]*scale+pos;
		pArea->m_BBox[0] = min(pArea->m_BBox[0],ptw); 
		pArea->m_BBox[1] = max(pArea->m_BBox[1],ptw);
	}
	pArea->m_BBox[0] -= Vec3(r,r,r);
	pArea->m_BBox[1] += Vec3(r,r,r);
	pArea->m_damping = 1.0f;
	pArea->m_falloff0 = 1.8f;
	pArea->m_rfalloff0 = 0.0f;

	pArea->m_next = m_pGlobalArea->m_next;
	m_pGlobalArea->m_next = pArea;
	RepositionArea(pArea);

	return pArea;
}


IPhysicalEntity *CPhysicalWorld::AddGlobalArea()
{
	WriteLock lock(m_lockAreas);
	if (!m_pGlobalArea)	{
		m_pGlobalArea = new CPhysArea(this);
		m_pGlobalArea->m_gravity = m_vars.gravity;
		m_pGlobalArea->m_BBox[0] = Vec3(-1e10f);
		m_pGlobalArea->m_BBox[1] = Vec3(1e10f);
		SignalEventAreaChange(this, m_pGlobalArea);
	}
	return m_pGlobalArea;
}


void CPhysicalWorld::RepositionArea(CPhysArea *pArea, Vec3* pBoxPrev)
{
	int res;
	CPhysArea *pPrevArea;
	for(pPrevArea=m_pGlobalArea; pPrevArea && pPrevArea->m_nextBig!=pArea; pPrevArea=pPrevArea->m_nextBig);

	if (!pArea->m_npt) {
		// set rsize for falloff distance test; if no falloff, extend beyond geom bounds so it's full strength throughout
		Vec3 size = pArea->m_BBox[1] - pArea->m_BBox[0];
		pArea->m_rsize.Set(size.x>0 ? 2.f/size.x:0, size.y>0 ? 2.f/size.y:0, size.z>0 ? 2.f/size.z:0);
	} else
		pArea->m_rsize.zero();

	if ((res = RepositionEntity(pArea,1|8))!=0) {
		if (res!=-1) {
			if (pPrevArea) {
				pPrevArea->m_nextBig = pArea->m_nextBig;
				m_nBigAreas--;
			}
			pArea->m_nextBig = 0;
		} else if (!pPrevArea) {
			pArea->m_nextBig = m_pGlobalArea->m_nextBig;
			m_pGlobalArea->m_nextBig = pArea;
			m_nBigAreas++;
		}
	}

	SignalEventAreaChange(this, pArea, pBoxPrev);
}


void CPhysicalWorld::RemoveArea(IPhysicalEntity *_pArea)
{
	CPhysArea *pPrevArea,*pArea=(CPhysArea*)_pArea;
	WriteLock lock1(m_lockAreas),lock(m_lockGrid);
	pArea->m_bDeleted = 2;
	for(pPrevArea=m_pGlobalArea; pPrevArea && pPrevArea->m_next!=pArea; pPrevArea=pPrevArea->m_next);
	if (pPrevArea) pPrevArea->m_next = pArea->m_next;
	for(pPrevArea=m_pGlobalArea; pPrevArea && pPrevArea->m_nextBig!=pArea; pPrevArea=pPrevArea->m_nextBig);
	if (pPrevArea) { 
		pPrevArea->m_nextBig = pArea->m_nextBig; m_nBigAreas--;
	}
	if ((unsigned int)pArea->m_pb.iMedium<16) {
		if (m_pTypedAreas[pArea->m_pb.iMedium]==pArea)
			m_pTypedAreas[pArea->m_pb.iMedium] = pArea->m_nextTyped;
		else {
			for(pPrevArea=m_pTypedAreas[pArea->m_pb.iMedium]; pPrevArea && pPrevArea->m_nextTyped!=pArea; pPrevArea=pPrevArea->m_nextTyped);
			if (pPrevArea)
				pPrevArea->m_nextTyped = pArea->m_nextTyped;
		}
		pArea->m_nextTyped = 0;
	}
	m_numNonWaterAreas -= IsAreaNonWater(pArea);
	m_numGravityAreas -= IsAreaGravity(pArea);

	SignalEventAreaChange(this, pArea);
	DetachEntityGridThunks(pArea);
	m_nAreas--;	m_nTypeEnts[PE_AREA]--;
	pArea->m_next=m_pDeletedAreas; m_pDeletedAreas=pArea;
	for(int i=0;i<=MAX_PHYS_THREADS;i++)
		m_prevGEAobjtypes[i] = -1;
}

int CPhysicalWorld::CheckAreas(const Vec3 &ptc, Vec3 &gravity, pe_params_buoyancy *pb, int nMaxBuoys, int iMedium, const Vec3 &vel, IPhysicalEntity *pent, int iCaller)
{
	if (!m_pGlobalArea)
		return 0;
	CPhysArea *pArea;
	CPhysicalEntity **pEnts;
	Vec3 gravityGlobal,bbox[2] = { ptc, ptc };
	MARK_UNUSED gravity,gravityGlobal;
	int nBuoys,iMedium0=-1;
	float radius = 0.0f;
	iMedium -= iMedium>>31 & iCaller-MAX_PHYS_THREADS>>31 & m_numNonWaterAreas-1>>31;
	if (pent) {
		bbox[0] = ((CPhysicalPlaceholder*)pent)->m_BBox[0]; 
		bbox[1] = ((CPhysicalPlaceholder*)pent)->m_BBox[1];
		Vec3 sz = max((bbox[1]-ptc).abs(), (bbox[0]-ptc).abs());
		radius = max(max(sz.x,sz.y),sz.z);
	}
	if (nBuoys=m_pGlobalArea->ApplyParams(ptc, gravityGlobal,vel, pb,0,nMaxBuoys,iMedium0, pent))
		iMedium0 = pb->iMedium;

	if ((unsigned int)iMedium<16) {
		if (nMaxBuoys) for(pArea=m_pTypedAreas[iMedium]; pArea; pArea=pArea->m_nextTyped)
			if (!pArea->m_bDeleted && AABB_overlap(pArea->m_BBox,bbox) && pArea->CheckPoint(ptc,radius))
				nBuoys += pArea->ApplyParams(ptc, gravity,vel, pb,nBuoys,nMaxBuoys,iMedium0, pent);
		if (is_unused(gravity))
			gravity = gravityGlobal;
		return nBuoys | nBuoys-1>>31;
	}

	if (m_numGravityAreas+nMaxBuoys==0) {
		gravity = gravityGlobal;
		return 0;
	}

	if (m_vars.bMultithreaded+iCaller>MAX_PHYS_THREADS)
		iCaller = get_iCaller();
	WriteLock lock0(m_lockCaller[iCaller]);
	ReadLock lock1(m_lockAreas);

	if (m_nBigAreas==m_nAreas) {
		for(pArea=m_pGlobalArea->m_nextBig; pArea; pArea=pArea->m_nextBig)
			if (!pArea->m_bDeleted && (!pent || GetGrid(pent)==GetGrid(pArea) || (pArea->m_BBox[1]-pArea->m_BBox[0]).len2()>1e12f) && AABB_overlap(pArea->m_BBox,bbox) && pArea->CheckPoint(ptc))
				nBuoys += pArea->ApplyParams(ptc, gravity,vel, pb,nBuoys,nMaxBuoys,iMedium0, pent);
	} else {
		for(int i=GetEntitiesAround(bbox[0],bbox[1],pEnts,ent_areas,(CPhysicalEntity*)pent,0,iCaller)-1; i>=0; i--)
			if ((pArea=(CPhysArea*)pEnts[i])->CheckPoint(ptc,radius))
				nBuoys += pArea->ApplyParams(ptc, gravity,vel, pb,nBuoys,nMaxBuoys,iMedium0, pent);
	}
	if (is_unused(gravity))
		gravity = gravityGlobal;

	return nBuoys | nBuoys-1>>31;
}

void CPhysicalWorld::SetWaterMat(int imat) 
{ 
	m_matWater=imat; 
	m_bCheckWaterHits = m_matWater>=0 && m_pGlobalArea && !is_unused(m_pGlobalArea->m_pb.waterPlane.origin) ? ent_water : 0;
}

IPhysicalEntity *CPhysicalWorld::GetNextArea(IPhysicalEntity *pPrevArea)
{
	if (!pPrevArea)	{
		if (!m_pGlobalArea)
			return 0;
		ReadLockCond lock(m_lockAreas, 1); lock.SetActive(0);
		return m_pGlobalArea;
	} else {
		CPhysArea *pNextArea;
		for(pNextArea=((CPhysArea*)pPrevArea)->m_next; pNextArea && pNextArea->m_bDeleted; pNextArea=(CPhysArea*)pNextArea->m_next);
		if (!pNextArea) {
			ReadLockCond lock(m_lockAreas, 0); lock.SetActive(1);
			return 0;
		}	else 
			return (IPhysicalEntity*)pNextArea;
	}
}

void CPhysicalWorld::ActivateArea(CPhysArea *pArea)
{
	if (pArea->m_nextActive==pArea) {
		WriteLock lock(m_lockActiveAreas);
		if (pArea->m_nextActive==pArea) {	// check again after locking
			pArea->m_nextActive = m_pActiveArea;
			m_pActiveArea = pArea;
			pArea->AddRef();
		}
	}
}
