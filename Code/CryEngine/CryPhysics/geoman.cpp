// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "geometries.h"
#include "geoman.h"
#include "tetrlattice.h"

void CGeomManager::InitGeoman()
{
	m_nGeomChunks=0; m_nGeomsInLastChunk=GEOM_CHUNK_SZ;
	m_lockGeoman = 0;
	m_pCracks = 0; m_nCracks = 0; m_idCrack = 0;
	m_kCrackScale = m_kCrackSkew = 1.0f;
	m_sizeExtGeoms = 0;
	m_pFreeGeom = 0; m_pGeoms = 0;
	m_bReleaseGeomsImmediately = false;
	m_oldGeoms =0; m_lockOldGeoms = 0;
}

void CGeomManager::ShutDownGeoman()
{
	int i,j;
	for(i=0;i<m_nGeomChunks;i++) {
		for(j=0;j<GEOM_CHUNK_SZ;j++) {
			if (m_pGeoms[i][j].pGeom) m_pGeoms[i][j].pGeom->Release();
			if (m_pGeoms[i][j].pMatMapping) delete [] m_pGeoms[i][j].pMatMapping;
		}
		delete[] m_pGeoms[i];
	}
	if (m_nGeomChunks)
		delete[] m_pGeoms;
	m_nGeomChunks=0; m_nGeomsInLastChunk=0;	
	if (m_pCracks) {
		for(i=0;i<m_nCracks;i++) m_pCracks[i].pGeom->Release();
		delete[] m_pCracks;
	}
	m_pCracks=0; m_nCracks=0;
	m_pFreeGeom = 0;
	extern CTriMesh *g_pSliceMesh;
	if (g_pSliceMesh) {
		g_pSliceMesh->Release();
		g_pSliceMesh = 0;
	}
}


IGeometry *CGeomManager::CreateMesh(strided_pointer<const Vec3> pVertices,strided_pointer<unsigned short> pIndices,char *pIds,int *pForeignIdx,int nTris, 
																		int flags, float approx_tolerance, SMeshBVParams *pParams)
{
	Vec3r axes[3],center;
	primitive *pprim;
	int i,itype = triangle::type;

	if (nTris<=0)
		return 0;
	if (pIds)	{
		for(i=1;i<nTris && pIds[i]==pIds[0];i++);
	} else i = nTris;
	if ((i==nTris || approx_tolerance>0.5f) && approx_tolerance>0) { // only try approximation if the mesh has one material
		ComputeMeshEigenBasis(pVertices,pIndices,nTris, axes,center);
		itype = ChoosePrimitiveForMesh(pVertices,pIndices,nTris, axes,center, flags,approx_tolerance, pprim);
	}

	if (itype==triangle::type) {
		if (flags & mesh_VoxelGrid) {
			CVoxelGeom *pVox = new CVoxelGeom;
			SVoxGridParams *params = (SVoxGridParams*)pParams;
			grid3d grd;
			pVox->CreateTriMesh(pVertices,pIndices,pIds,pForeignIdx,nTris, flags);
			grd.Basis.SetIdentity(); grd.bOriented = 0;
			grd.origin = params->origin; 
			grd.size = params->size; 
			grd.step = params->step;
			pVox->CreateVoxelGrid(&grd);
			return pVox;
		}	else {
			SBVTreeParams *params = (SBVTreeParams*)pParams;
			CTriMesh *pMesh = new CTriMesh;
			pMesh->CreateTriMesh(pVertices,pIndices,pIds,pForeignIdx,nTris, flags, 
				params->nMinTrisPerNode,params->nMaxTrisPerNode, params->favorAABB);
			return pMesh;
		}
	} else
		return CreatePrimitive(itype,pprim);
}


IGeometry *CGeomManager::CreatePrimitive(int type, const primitive *pprim)
{
	IGeometry *pRes = 0;
	switch (type) {
		case cylinder::type: pRes = (new CCylinderGeom)->CreateCylinder((cylinder*)pprim); break;
		case capsule::type: pRes = (new CCapsuleGeom)->CreateCapsule((capsule*)pprim); break;
		case sphere::type: pRes = (new CSphereGeom)->CreateSphere((sphere*)pprim); break;
		case box::type: pRes = (new CBoxGeom)->CreateBox((box*)pprim); break;
		case heightfield::type: pRes = (new CHeightfield)->CreateHeightfield((heightfield*)pprim); break;
		case ray::type: pRes = new CRayGeom((ray*)pprim);	break;
		default: return 0;
	}
	m_sizeExtGeoms += ((CGeometry*)pRes)->GetSizeFast();
	return pRes;
}

void CGeomManager::DestroyGeometry(IGeometry *pGeom)
{
	pGeom->Release();
}


int CGeomManager::AddRefGeometry(phys_geometry *pgeom)
{
	AtomicAdd(&pgeom->nRefCount, 1);
	return pgeom->nRefCount;
}

int CGeomManager::UnregisterGeometry(phys_geometry *pgeom)
{
	WriteLock lock(m_lockGeoman);
	AtomicAdd(&pgeom->nRefCount, -1);
	if (pgeom->nRefCount!=0) 
		return pgeom->nRefCount;
	if (((CGeometry*)pgeom->pGeom)->m_nRefCount<=1)
		m_sizeExtGeoms += ((CGeometry*)pgeom->pGeom)->GetSizeFast();
	if (bop_meshupdate *pmu = (bop_meshupdate*)pgeom->pGeom->GetForeignData(DATA_MESHUPDATE)) {
		delete pmu; pgeom->pGeom->SetForeignData(0,DATA_MESHUPDATE);
	}
	pgeom->pGeom->Release();
	pgeom->pGeom = 0;
	if (pgeom->pMatMapping) delete[] pgeom->pMatMapping;
	pgeom->pMatMapping = 0;
	//if (pgeom-m_pGeoms[m_nGeomChunks-1]==m_nGeomsInLastChunk-1) 
	//	m_nGeomsInLastChunk--;
	pgeom->pForeignData = m_pFreeGeom;
	m_pFreeGeom = pgeom;
	return 0;
}

extern class CPhysicalWorld *g_pPhysWorlds[];
void CGeometry::Release() 
{ 
	if (CryInterlockedDecrement(&m_nRefCount)<=0){ 
		if(g_pPhysWorlds[0]) {
			CGeomManager *pGeoman = (CGeomManager*)((IPhysicalWorld*)g_pPhysWorlds[0])->GetGeomManager();
			pGeoman->m_sizeExtGeoms-=GetSizeFast(); 
		}	delete this;
	} 
}

void CGeomManager::FlushOldGeoms()
{
	WriteLock lock(m_lockOldGeoms); 
	CGeometry *pgeom=m_oldGeoms,*pgeomNext;
	for(m_oldGeoms=0; pgeom; pgeom=pgeomNext) {
		pgeomNext = (CGeometry*)pgeom->m_pForeignData;
		delete pgeom;
	}
}


phys_geometry *CGeomManager::GetFreeGeomSlot()
{
	int i,j;
	/*for(i=0;i<m_nGeomChunks-1;i++) {
		for(j=0;j<GEOM_CHUNK_SZ && m_pGeoms[i][j].pGeom;j++);
		if (j<GEOM_CHUNK_SZ) break;
	}
	if (i>=m_nGeomChunks-1) 
		for(j=0;j<m_nGeomsInLastChunk && m_pGeoms[i][j].pGeom;j++);
	if (m_nGeomChunks==0 || j==GEOM_CHUNK_SZ) {*/
	if (!m_pFreeGeom) {
		phys_geometry **t = m_pGeoms;
		m_pGeoms = new phys_geometry*[m_nGeomChunks+1];
		if (m_nGeomChunks) {
			memcpy(m_pGeoms, t, sizeof(phys_geometry*)*m_nGeomChunks);
			delete[] t;
		}
		i=m_nGeomChunks++; 
		memset(m_pGeoms[i] = new phys_geometry[GEOM_CHUNK_SZ], 0, sizeof(phys_geometry)*GEOM_CHUNK_SZ);
		NO_BUFFER_OVERRUN
		for(j=0;j<GEOM_CHUNK_SZ-1;j++) m_pGeoms[i][j].pForeignData = m_pGeoms[i]+j+1;
		m_pFreeGeom = m_pGeoms[i];
	}
	phys_geometry *pgeom = m_pFreeGeom;
	m_pFreeGeom = (phys_geometry*)m_pFreeGeom->pForeignData;
	pgeom->pForeignData = 0;
	return pgeom;
	/*if (i==m_nGeomChunks-1 && j==m_nGeomsInLastChunk)
		m_nGeomsInLastChunk++;

	return m_pGeoms[i]+j;*/
}

phys_geometry *CGeomManager::RegisterGeometry(IGeometry *pGeom,int defSurfaceIdx, int *pMatMapping,int nMats)
{
	WriteLock lock(m_lockGeoman);
	phys_geometry *pgeom = GetFreeGeomSlot();
	if (pGeom) {
		pGeom->CalcPhysicalProperties(pgeom);
		pGeom->AddRef();
		m_sizeExtGeoms -= ((CGeometry*)pGeom)->GetSizeFast();
	}
	float imin = max(max(pgeom->Ibody.x,pgeom->Ibody.y),pgeom->Ibody.z)*0.01f;
	pgeom->Ibody.x = max(pgeom->Ibody.x,imin); 
	pgeom->Ibody.y = max(pgeom->Ibody.y,imin); 
	pgeom->Ibody.z = max(pgeom->Ibody.z,imin); 
	pgeom->nRefCount = 1;
	pgeom->surface_idx = defSurfaceIdx;
	if (pMatMapping)
		memcpy(pgeom->pMatMapping=new int[nMats], pMatMapping, nMats*sizeof(int));
	else pgeom->pMatMapping = 0;
	pgeom->nMats = nMats;
	return pgeom;
}


void CGeomManager::SetGeomMatMapping(phys_geometry *pgeom, int *pMatMapping, int nMats)
{
	if (pgeom->pMatMapping)
		delete[] pgeom->pMatMapping;
	if (pMatMapping)
		memcpy(pgeom->pMatMapping=new int[nMats], pMatMapping, nMats*sizeof(int));
	else 
		pgeom->pMatMapping = 0;
	pgeom->nMats = nMats;
}


void CGeomManager::SaveGeometry(CMemStream &stm, IGeometry *pGeom)
{
	stm.Write(pGeom->GetType());
	pGeom->Save(stm);
}

IGeometry *CGeomManager::LoadGeometry(CMemStream &stm, strided_pointer<const Vec3> pVertices, 
																			strided_pointer<unsigned short> pIndices, char *pIds)
{
	int itype; stm.Read(itype);
	IGeometry *pGeom;
	switch (itype) {
		case GEOM_TRIMESH: pGeom = new CTriMesh; break;
		case GEOM_CYLINDER: pGeom = new CCylinderGeom; break;
		case GEOM_CAPSULE: pGeom = new CCapsuleGeom; break;
		case GEOM_SPHERE: pGeom = new CSphereGeom; break;
		case GEOM_BOX: pGeom = new CBoxGeom; break;
		default: return 0;
	}
	pGeom->Load(stm, pVertices,pIndices,pIds);
	return pGeom;
}

void CGeomManager::SavePhysGeometry(CMemStream &stm, phys_geometry *pgeom)
{
	stm.Write(PHYS_GEOM_VER);
	phys_geometry_serialize pgs;
	pgs.dummy0 = 0;
	pgs.Ibody = pgeom->Ibody;
	pgs.q = pgeom->q;
	pgs.origin = pgeom->origin;
	pgs.V = pgeom->V;
	pgs.nRefCount = pgeom->nRefCount;
	pgs.surface_idx = pgeom->surface_idx;
	pgs.dummy1 = 0;
	pgs.nMats = 0;
	stm.Write(pgs);
	stm.Write(pgeom->pGeom->GetType());
	pgeom->pGeom->Save(stm);
}

phys_geometry *CGeomManager::LoadPhysGeometry(CMemStream &stm, strided_pointer<const Vec3> pVertices, 
																							strided_pointer<unsigned short> pIndices, char *pIds)
{
	int ver; stm.Read(ver);
	if (ver!=PHYS_GEOM_VER)
		return 0;
	WriteLock lock(m_lockGeoman);
	phys_geometry *pgeom = GetFreeGeomSlot();
	phys_geometry_serialize pgs;
/*#if SIZEOF_PTR != 4
	StructUnpack(
		pgeom,
		functor(
		  stm,
		  reinterpret_cast<void (CMemStream::*)(void*, uint32)>(
			  &CMemStream::ReadRaw)
		),
		offsetof(phys_geometry, pForeignData));
#else
	stm.ReadRaw(pgeom, (char*)&pgeom->pForeignData-(char*)pgeom);
	SwapEndian(*pgeom);
#endif*/
	stm.Read(pgs);
	//SwapEndian(pgs);
	pgeom->Ibody = pgs.Ibody;
	float imin = max(max(pgeom->Ibody.x,pgeom->Ibody.y),pgeom->Ibody.z)*0.01f;
	pgeom->Ibody.x = max(pgeom->Ibody.x,imin); 
	pgeom->Ibody.y = max(pgeom->Ibody.y,imin); 
	pgeom->Ibody.z = max(pgeom->Ibody.z,imin); 
	pgeom->q = pgs.q;
	pgeom->origin = pgs.origin;
	pgeom->V = pgs.V;
	pgeom->surface_idx = pgs.surface_idx;
	pgeom->nRefCount = 1;
	pgeom->pMatMapping = 0;
	pgeom->nMats = 0;
	pgeom->pGeom = LoadGeometry(stm,pVertices,pIndices,pIds);
	if (pgeom->pGeom->GetType()==GEOM_SPHERE)
		pgeom->pGeom->CalcPhysicalProperties(pgeom);
	return pgeom;
}


IGeometry *CGeomManager::CloneGeometry(IGeometry *pGeom)
{
	IGeometry *pClone=0;
	if (pGeom->GetType()==GEOM_TRIMESH)
		pClone = (new CTriMesh)->Clone((CTriMesh*)pGeom,0);
	else {
		CMemStream stm(false);
		SaveGeometry(stm,pGeom);
		stm.m_iPos = 0;
		pClone = LoadGeometry(stm,0,0,0);
	}
	return pClone;
}


ITetrLattice *CGeomManager::CreateTetrLattice(const Vec3 *pt,int npt, const int *pTets,int nTets)
{
	return (new CTetrLattice(GetIWorld()))->CreateLattice(pt,npt,pTets,nTets);
}


int CGeomManager::RegisterCrack(IGeometry *pGeom, Vec3 *pVtx, int idmat)
{
	int i0;
	float edgelen[3];
	Vec3 edge,pt0,n;
	Matrix33 Re,Rn;

	if (pGeom->GetType()!=GEOM_TRIMESH)
		return -1;
	for(i0=0; i0<m_nCracks && (m_pCracks[i0].pGeom!=pGeom || m_pCracks[i0].idmat!=idmat); i0++);
	if (i0<m_nCracks)
		return -1;

	ReallocateList(m_pCracks,m_nCracks,m_nCracks+1);
	m_pCracks[m_nCracks].id = m_idCrack++;
	m_pCracks[m_nCracks].pGeom = (CTriMesh*)pGeom;
	m_pCracks[m_nCracks].idmat = idmat;
	for(i0=0;i0<3;i0++) m_pCracks[m_nCracks].vtx[i0] = pVtx[i0];

	for(i0=0;i0<3;i0++) edgelen[i0] = (pVtx[inc_mod3[i0]]-pVtx[i0]).len2();
	i0 = idxmax3(edgelen); edge = pVtx[inc_mod3[i0]]-pVtx[i0];
	n = (pVtx[1]-pVtx[0]^pVtx[2]-pVtx[0]).normalized();
	if (edgelen[inc_mod3[i0]] < edgelen[dec_mod3[i0]]) {
		edge.Flip(); pt0 = pVtx[inc_mod3[i0]]; n.Flip();
	}	else pt0 = pVtx[i0];
	Re.SetRotationV0V1(edge.normalized(),Vec3(1,0,0)); n = Re*n;
	Rn.SetRotationAA(n.z,n.y, Vec3(1,0,0));

	m_pCracks[m_nCracks].Rc = Rn*Re;	
	m_pCracks[m_nCracks].pt3 = Vec2(m_pCracks[m_nCracks].Rc*(pVtx[dec_mod3[i0]]-pt0));
	m_pCracks[m_nCracks].ry3 = 1/m_pCracks[m_nCracks].pt3.y;
	m_pCracks[m_nCracks].rmaxedge = 1.0f/(m_pCracks[m_nCracks].maxedge = sqrt_tpl(edgelen[i0])); 
	m_pCracks[m_nCracks].pt0 = pt0;
	pGeom->AddRef();

	return m_pCracks[m_nCracks++].id;
}


void CGeomManager::UnregisterCrack(int id)
{
	int i;
	for(i=0;i<m_nCracks && m_pCracks[i].id!=id;i++);
	if (i<m_nCracks) {
		m_pCracks[i].pGeom->Release();
		memmove(m_pCracks+i,m_pCracks+i+1,m_nCracks-1-i);
		m_nCracks--;
		ReallocateList(m_pCracks,m_nCracks,m_nCracks);
	}
}


IGeometry *CGeomManager::GetCrackGeom(const Vec3 *pt,int idmat, geom_world_data *pgwd)
{
	/* find a crack that has sides closest to the requested
	 canonic transformation Rc: the longest edge starts at (0,0,0) and goes along x axis
	   the 3d vertex lies in xy plane and is closer to the origin than to the other longest edge end
	 [crack] Rc1*(p-oc1)*scale (M) -> [target] Rc2*(p-oc2)
	 M*Rc1*(p-oc1)*scale = Rc2*(p-oc2)
	 M: | 1 (x3_2-x3_1)/y3_1 0 |
	    | 0    y3_2/y3_1		 0 |
	    | 0         0        1 |
	 M*Rc1*(p-oc1)*scale = Rc2*(p-oc2)
	 mesh1 transform: 
	   Rc1.T*pc/scale+oc1
	   Rc1.T*M*Rc1*(p-oc1)+oc1
	 mesh1 gwd: 
	   Rc2.T*Rc1*(p-oc1)*scale+oc2
	   (Rc2.T*Rc1)*p*(scale) + oc2-(Rc2.T*Rc1)*oc1*(scale)
	 Rc =  Rn*Re
	 Re : (longest edge)->(1,0,0)
	 Rn : n'*sgn(n'.z*(Re*p3).z)->(0,0,1)
	*/
	int i,i0;
	float edgelen[3],maxedge,rmaxedge,diff,mindiff,scale,rscale;
	Vec3 edge,n,pt0,offs;
	Matrix33 Re,Rn,Rc,M;
	Vec2 pt3;
	
	for(i0=0;i0<3;i0++) edgelen[i0] = (pt[inc_mod3[i0]]-pt[i0]).len2();
	i0 = idxmax3(edgelen); edge = pt[inc_mod3[i0]]-pt[i0];
	n = (pt[1]-pt[0]^pt[2]-pt[0]).normalized();
	if (edgelen[inc_mod3[i0]] < edgelen[dec_mod3[i0]]) {
		edge.Flip(); n.Flip(); pt0 = pt[inc_mod3[i0]];
	}	else pt0 = pt[i0];
	Re.SetRotationV0V1(edge.normalized(),Vec3(1,0,0)); n = Re*n;
	Rn.SetRotationAA(n.z,n.y, Vec3(1,0,0));
	Rc = Rn*Re;	
	pt3 = Vec2(Rc*(pt[dec_mod3[i0]]-pt0));
	rmaxedge = 1.0f/(maxedge = sqrt_tpl(edgelen[i0])); 

	for(i=0,i0=-1,mindiff=1E10; i<m_nCracks; i++) if (m_pCracks[i].idmat==idmat) {
		scale = maxedge*m_pCracks[i].rmaxedge;
		diff = m_kCrackSkew*len2(m_pCracks[i].pt3*scale-pt3)*sqr(rmaxedge)+m_kCrackScale*sqr(scale);
		if (diff<mindiff) {
			mindiff = diff; i0 = i;
		}
	}
	if (i0==-1)
		return 0;

	rscale = m_pCracks[i0].maxedge*rmaxedge;
	M.SetIdentity(); 
	M(0,1) = (pt3.x*rscale-m_pCracks[i0].pt3.x)*m_pCracks[i0].ry3;
	M(1,1) = pt3.y*m_pCracks[i0].ry3*rscale;
	Re = m_pCracks[i0].Rc.T()*M*m_pCracks[i0].Rc;
	offs = m_pCracks[i0].pt0-Re*m_pCracks[i0].pt0;

	CTriMesh *pMesh = new CTriMesh;
	pMesh->Clone(m_pCracks[i0].pGeom,mesh_shared_idx|mesh_shared_mats|mesh_shared_foreign_idx);
	for(i=0;i<pMesh->m_nVertices;i++)
		pMesh->m_pVertices[i] = Re*pMesh->m_pVertices[i]+offs;
	for(i=0;i<pMesh->m_nTris;i++)
		pMesh->RecalcTriNormal(i);
	pMesh->RebuildBVTree(m_pCracks[i0].pGeom->m_pTree);

	pgwd->R = Rc.T()*m_pCracks[i0].Rc;
	pgwd->offset = pt0-pgwd->R*m_pCracks[i0].pt0*scale;
	pgwd->scale = scale;

	return pMesh;
}


IBreakableGrid2d *CGeomManager::GenerateBreakableGrid(Vec2 *ptsrc,int npt, const Vec2i &nCells, int bStaticBorder, int seed)
{	
	CBreakableGrid2d *pGrid = new CBreakableGrid2d;
	pGrid->Generate(ptsrc,npt,nCells,bStaticBorder,seed);
	return pGrid;
}


