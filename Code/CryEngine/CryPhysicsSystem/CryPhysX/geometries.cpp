// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "../../CryPhysics/geoman.h"
#include "geometries.h"
#include <CryNetwork/ISerialize.h>

using namespace primitives;
using namespace cpx;
using namespace Helper;

//////////////////////////////////////////////////////////////////////////

PxConvexMesh *g_cylMesh = 0;

void CGeomManager::InitGeoman()	{	m_nCracks = 0; }
void CGeomManager::ShutDownGeoman() {}

void GetTriangles(const PxTriangleMesh *pMesh, mesh_data *pdata) 
{ 
	pdata->pIndices = (index_t*)pMesh->getTriangles();
	pdata->nTris = pMesh->getNbTriangles(); 
}

void GetTriangles(const PxConvexMesh *pMesh, mesh_data *pdata)
{
	int nPolies = pMesh->getNbPolygons(), nTris=0;
	for(int i=0;i<nPolies; i++) {
		PxHullPolygon pg;	pMesh->getPolygonData(i,pg);
		nTris += pg.mNbVerts-2;
	}
	pdata->pIndices = new index_t[nTris*3];
	pdata->flags |= mesh_shared_idx;
	const uint8 *pIdx = pMesh->getIndexBuffer();
	for(int i=nTris=0; i<nPolies; i++) {
		PxHullPolygon pg;	pMesh->getPolygonData(i,pg);
		for(int j=0; j<pg.mNbVerts-2; j++,nTris++)
			((Vec3_tpl<index_t>*)pdata->pIndices)[nTris].Set(pIdx[pg.mIndexBase],pIdx[pg.mIndexBase+j+1],pIdx[pg.mIndexBase+(j+2 & j+2-(int)pg.mNbVerts>>31)]);
	}
	pdata->nTris = nTris;
}

static std::vector<char> matsDummy;
static std::vector<Vec3> normsDummy;
static std::vector<trinfo> topolDummy;
template<class Tmesh> void FillMeshData(const Tmesh* pMesh, mesh_data* pdata)
{
	pdata->pVertices = strided_pointer<Vec3>((Vec3*)pMesh->getVertices());
	pdata->nVertices = pMesh->getNbVertices();
	GetTriangles(pMesh, pdata);
	if (pdata->nTris > matsDummy.size()) {
		matsDummy.resize(pdata->nTris+63&~63, 0);
		normsDummy.resize(pdata->nTris+63&~63, Vec3(0,0,1));
		topolDummy.resize(pdata->nTris+63&~63, {{-1,-1,-1}});
	}
	pdata->pMats = &matsDummy[0];
	pdata->pNormals = &normsDummy[0];
	pdata->pTopology = &topolDummy[0];
}

IGeometry* CGeomManager::CreateMesh(strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char *pIds, int *pForeignIdx, int nTris,
	int flags, float approx_tolerance, SMeshBVParams *pParams)
{
	int nVtx;
	for(int i=nVtx=0; i<nTris*3; i++) nVtx = max(nVtx,pIndices[i]+1);

	if (approx_tolerance > 0.9f) {
		#define vtx(idx) pVertices[pIndices[i*3+idx]]
		Vec3 center(ZERO);
		float areaTot = 0;
		for(int i=0; i<nTris; i++) {
			float area = (vtx(1)-vtx(0)^vtx(2)-vtx(0)).len();
			center += (vtx(0)+vtx(1)+vtx(2))*(1.0f/3)*area;
			areaTot += area;
		}
		center /= max(1e-10f,areaTot);
		Matrix33 C(ZERO), Ctmp;
		for(int i=0;i<nTris;i++) {
			float area = (vtx(1)-vtx(0)^vtx(2)-vtx(0)).len();
			Vec3 m = (vtx(0)+vtx(1)+vtx(2));
			C += dotproduct_matrix(m,m,Ctmp)*area;
			for(int j=0;j<3;j++) C += dotproduct_matrix(vtx(j),vtx(j),Ctmp)*area;
		}
		#undef vtx

		PxQuat axesPx;
		PxDiagonalize(*(PxMat33*)&C, axesPx);
		box boxEigen;
		boxEigen.Basis = Matrix33(!Q(axesPx));
		Vec3 eigen;
		for(int i=0;i<3;i++) eigen[i] = fabs(boxEigen.Basis.GetRow(i)*(C*boxEigen.Basis.GetRow(i)));
		Vec3 BBox[] = { Vec3(ZERO),Vec3(ZERO) };
		for(int i=0;i<nTris*3;i++) {
			BBox[0] = min(BBox[0], boxEigen.Basis*(pVertices[pIndices[i]]-center));
			BBox[1] = max(BBox[1], boxEigen.Basis*(pVertices[pIndices[i]]-center));
		}
		boxEigen.size = (BBox[1]-BBox[0])*0.5f;
		boxEigen.center = center+((BBox[1]+BBox[0])*0.5f)*boxEigen.Basis;

		if (flags & mesh_approx_box || !(flags & (mesh_approx_sphere | mesh_approx_cylinder | mesh_approx_capsule)))
			return CreatePrimitive(box::type, &boxEigen);
		if (flags & mesh_approx_sphere) {
			sphere sph; sph.center = boxEigen.center;	sph.r = (boxEigen.size.x+boxEigen.size.y+boxEigen.size.z)*(1.0f/3);
			return CreatePrimitive(sphere::type, &sph);
		}
		if (flags & (mesh_approx_cylinder | mesh_approx_capsule)) {
			cylinder cyl;
			int i = idxmax3(min((eigen-eigen.GetPermutated(0)).abs(), (eigen-eigen.GetPermutated(1)).abs()));
			cyl.center = center;
			cyl.axis = boxEigen.Basis.GetRow(i);
			cyl.r = (boxEigen.size.x+boxEigen.size.y+boxEigen.size.z-boxEigen.size[i])*0.5f;
			cyl.hh = boxEigen.size[i] - cyl.r*iszero(flags & mesh_approx_cylinder);
			return CreatePrimitive(flags & mesh_approx_cylinder ? cylinder::type:capsule::type, &cyl);
		}
	}

	PhysXGeom	*pGeom = new PhysXGeom();	
	memset(pGeom->m_geom.mesh.pdata = new mesh_data, 0, sizeof(mesh_data));
	pGeom->m_type = GEOM_TRIMESH;
	pGeom->m_geom.mesh.pMesh = nullptr;
	pGeom->m_geom.mesh.pMeshConvex = nullptr;

	if (flags & mesh_multicontact0 && nTris<30 || nTris<=12) {
		PxConvexMeshDesc cmd;
		cmd.points.count = nVtx;
		cmd.points.data = pVertices.data;
		cmd.points.stride = pVertices.iStride;
		cmd.flags = PxConvexFlag::eCOMPUTE_CONVEX;
		PxDefaultMemoryOutputStream buf;
		PxConvexMeshCookingResult::Enum result;
		g_cryPhysX.Cooking()->cookConvexMesh(cmd, buf, &result);
		if (pGeom->m_geom.mesh.pMeshConvex = g_cryPhysX.Physics()->createConvexMesh(PxDefaultMemoryInputData(buf.getData(), buf.getSize())))
			FillMeshData(pGeom->m_geom.mesh.pMeshConvex, pGeom->m_geom.mesh.pdata);
	}	else {
		PxTriangleMeshDesc tmd;
		tmd.points.data = pVertices.data;
		tmd.points.count = nVtx;
		tmd.points.stride = pVertices.iStride;
		tmd.triangles.data = pIndices.data;
		tmd.triangles.count = nTris;
		tmd.triangles.stride = 3*sizeof(unsigned short);
		tmd.flags = PxMeshFlag::e16_BIT_INDICES;
		PxTolerancesScale scale; PxCookingParams cookParams(scale);
		//cookParams.meshPreprocessParams |= PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
		if (sizeof(index_t)==4)
			cookParams.meshPreprocessParams |= PxMeshPreprocessingFlag::eFORCE_32BIT_INDICES;
		g_cryPhysX.Cooking()->setParams(cookParams);
		if (pGeom->m_geom.mesh.pMesh = g_cryPhysX.Cooking()->createTriangleMesh(tmd, g_cryPhysX.Physics()->getPhysicsInsertionCallback()))
			FillMeshData(pGeom->m_geom.mesh.pMesh, pGeom->m_geom.mesh.pdata);
	}
	return pGeom;
}

PhysXGeom::~PhysXGeom() 
{
	switch (m_type) {
		case GEOM_TRIMESH: 
			if (m_geom.mesh.pMesh) m_geom.mesh.pMesh->release(); 
			if (m_geom.mesh.pMeshConvex) m_geom.mesh.pMeshConvex->release(); 
			if (m_geom.mesh.pdata->flags & mesh_shared_idx) delete[] m_geom.mesh.pdata->pIndices;
			delete m_geom.mesh.pdata; 
			break;
		case GEOM_HEIGHTFIELD: 
			m_geom.hf.pHF->release(); 
			break;
	}
}

void PhysXGeom::GetBBox(primitives::box* pbox)
{
	switch (m_type) {
		case GEOM_SPHERE:
			pbox->Basis.SetIdentity();
			pbox->center = m_geom.sph.center;
			pbox->size = Vec3(m_geom.sph.r);
			break;
		case GEOM_CAPSULE: case GEOM_CYLINDER:
			pbox->Basis = Matrix33::CreateRotationV0V1(Vec3(0,0,1),m_geom.caps.axis);
			pbox->center = m_geom.caps.center;
			pbox->size = Vec3(m_geom.caps.r,m_geom.caps.r, m_geom.caps.hh+m_geom.caps.r*(m_type==GEOM_CAPSULE));
			break;
		case GEOM_BOX:
			*pbox = m_geom.box;
			break;
		case GEOM_TRIMESH: {
			PxBounds3 bbox = m_geom.mesh.pMesh ? m_geom.mesh.pMesh->getLocalBounds() : m_geom.mesh.pMeshConvex->getLocalBounds();
			pbox->Basis.SetIdentity();
			pbox->center = V(bbox.getCenter());
			pbox->size = V(bbox.getExtents());
			} break;
		default:
			pbox->Basis.SetIdentity();
			pbox->center.zero();
			pbox->size.zero();
	}
}

float PhysXGeom::GetVolume()
{
	float V = 0;
	switch (m_type) {
		case GEOM_SPHERE:	V = (4*gf_PI/3)*cube(m_geom.sph.r); break;
		case GEOM_CAPSULE: V = (4*gf_PI/3)*cube(m_geom.caps.r);
		case GEOM_CYLINDER: V += gf_PI*sqr(m_geom.caps.r)*2*m_geom.caps.hh; break;
		case GEOM_BOX: V = m_geom.box.size.GetVolume()*8; break;
		default: {
			box bbox; GetBBox(&bbox);
			V = bbox.size.GetVolume()*8;
		}
	}
	return V;
}

int PhysXGeom::Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2, intersection_params* pparams, geom_contact*& pcontacts)
{ 
	QuatT pose[2] = { QuatT(IDENTITY), QuatT(IDENTITY) };
	Diag33 scale[2] = { Diag33(1), Diag33(1) };
	for(int i=0;i<2;i++) if (geom_world_data *pdata = ((geom_world_data*[2]){pdata1,pdata2})[i]) {
		scale[i] = Diag33(pdata->R.GetColumn(0).len(), pdata->R.GetColumn(1).len(), pdata->R.GetColumn(2).len());
		pose[i].q = Quat(pdata->R * Diag33(scale[i]).invert());
		scale[i] = scale[i]*Diag33(pdata->scale);
		pose[i].t = pdata->offset;
	}
	static geom_contact g_cnt;
	pcontacts = &g_cnt;
	return CreateAndUse(pose[0],scale[0], [&](const PxGeometry& geom0) {
		return ((PhysXGeom*)pCollider)->CreateAndUse(pose[1],scale[1], [&](const PxGeometry& geom1) {
			return PxGeometryQuery::overlap(geom0,T(pose[0]), geom1,T(pose[1]));
		});
	});
}

//////////////////////////////////////////////////////////////////////////

void CGeomManager::DestroyGeometry(IGeometry *pGeom) {}

//////////////////////////////////////////////////////////////////////////

void CGeomManager::SaveGeometry(CMemStream &stm, IGeometry *pGeom) {}
IGeometry *CGeomManager::LoadGeometry(CMemStream &stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char *pIds) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }
IGeometry *CGeomManager::CloneGeometry(IGeometry *pGeom) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }

ITetrLattice *CGeomManager::CreateTetrLattice(const Vec3 *pt, int npt, const int *pTets, int nTets) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }
int CGeomManager::RegisterCrack(IGeometry *pGeom, Vec3 *pVtx, int idmat) { CRY_PHYSX_LOG_FUNCTION; _RETURN_INT_DUMMY_; }
void CGeomManager::UnregisterCrack(int i) {}
IGeometry *CGeomManager::GetCrackGeom(const Vec3 *pt, int idmat, geom_world_data *pgwd) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }

IBreakableGrid2d *CGeomManager::GenerateBreakableGrid(Vec2 *ptsrc, int npt, const Vec2i &nCells, int bStaticBorder, int seed) { CRY_PHYSX_LOG_FUNCTION; _RETURN_PTR_DUMMY_; }

//////////////////////////////////////////////////////////////////////////

IGeometry *CGeomManager::CreatePrimitive(int type, const primitives::primitive *pprim)
{
	PhysXGeom	*pGeom = new PhysXGeom;	
	switch (pGeom->m_type = type) {
		case cylinder::type: 
		case capsule::type: pGeom->m_geom.caps = *(const capsule*)pprim; break;
		case sphere::type: pGeom->m_geom.sph = *(const sphere*)pprim; break;
		case box::type: pGeom->m_geom.box = *(const box*)pprim; break;
		case heightfield::type: {
			const heightfield &hf = *(const heightfield*)pprim;
			if (max(hf.size.x,hf.size.y) > 8192)
				return 0;
			PxHeightFieldDesc hfd;
			hfd.format = PxHeightFieldFormat::eS16_TM;
			hfd.nbColumns = hf.size.x;
			hfd.nbRows = hf.size.y;
			float hmin=0,hmax=0;
			for(int ix=0;ix<hf.size.x;ix++) for(int iy=0;iy<hf.size.y;iy++) {
				float h = hf.getheight(ix,iy);
				hmin = min(hmin,h); hmax = max(hmax,h);
			}
			PxHeightFieldSample *data = new PxHeightFieldSample[hf.size.x*hf.size.y];
			float scale = max(hmax,-hmin)/0x7fff, rscale=scale>0 ? 1/scale:0;
			for(int ix=0;ix<hf.size.x;ix++) for(int iy=0;iy<hf.size.y;iy++) {
				PxHeightFieldSample &cell = data[iy*hf.size.x+ix];
				cell.height = (int)(hf.getheight(ix,iy)*rscale);
				cell.materialIndex0=cell.materialIndex1 = hf.gettype(ix,iy);
			}
			hfd.samples.data = data;
			hfd.samples.stride = sizeof(PxHeightFieldSample);
			pGeom->m_geom.hf.pHF = g_cryPhysX.Cooking()->createHeightField(hfd, g_cryPhysX.Physics()->getPhysicsInsertionCallback());
			pGeom->m_geom.hf.hscale = scale>0 ? scale : 1.0f;
			pGeom->m_geom.hf.step = hf.step;
			pGeom->m_geom.hf.origin = hf.origin;
			break;
		}
		case ray::type:
			pGeom->m_geom.ray = *(const ray*)pprim;
			break;
		default: return 0;
	}
	return pGeom;
}

const primitive* PhysXGeom::GetData()
{
	if (m_type==GEOM_TRIMESH) {
		m_geom.mesh.pdata->pMats = &matsDummy[0];
		m_geom.mesh.pdata->pNormals = &normsDummy[0];
		m_geom.mesh.pdata->pTopology = &topolDummy[0];
		return m_geom.mesh.pdata;
	}	else if (m_type==GEOM_HEIGHTFIELD)
		return 0;
	return (const primitive*)&m_geom;
}

phys_geometry *CGeomManager::RegisterGeometry(IGeometry *pGeom,int defSurfaceIdx, int *pMatMapping,int nMats)
{
	phys_geometry *pgeom = new phys_geometry;
	memset(pgeom, 0, sizeof(phys_geometry));
	if (pMatMapping)
		memcpy(pgeom->pMatMapping = new int[nMats], pMatMapping, nMats*sizeof(int));
	pgeom->nMats = nMats;
	pgeom->nRefCount = 1;
	pgeom->q.SetIdentity();
	PhysXGeom *geom = (PhysXGeom*)pGeom;
	switch (geom->m_type) {
		case GEOM_SPHERE: {
			const sphere& sph = geom->m_geom.sph;
			pgeom->origin = sph.center;
			pgeom->V = 4.0f/3*g_PI*cube(sph.r);
			float x2 = sqr(sph.r)*0.4f*pgeom->V;
			pgeom->Ibody.Set(x2,x2,x2);
		} break;
		case GEOM_CAPSULE: {
			const capsule& caps = geom->m_geom.caps;
			pgeom->origin = caps.center;
			pgeom->q.SetRotationV0V1(Vec3(0,0,1),caps.axis);
			float Vcap = 4.0f/3*(float)g_PI*cube(caps.r);
			pgeom->V = sqr(caps.r)*caps.hh*((float)g_PI*2) + Vcap;
			float r2=sqr(caps.r), x2=(float)g_PI*caps.hh*sqr(r2)*0.5f, z2=(float)g_PI*r2*cube(caps.hh)*(2.0f/3);
			pgeom->Ibody.Set(x2+z2+Vcap*(r2*0.4f+sqr(caps.hh)), x2+z2+Vcap*(r2*0.4f+sqr(caps.hh)), x2*2+Vcap*r2*0.4f);
		} break;
		case GEOM_CYLINDER: {
			const cylinder& cyl = (const cylinder&)geom->m_geom.caps;
			pgeom->origin = cyl.center;
			pgeom->q.SetRotationV0V1(Vec3(0,0,1),cyl.axis);
			pgeom->V = sqr(cyl.r)*cyl.hh*(g_PI*2);
			float x2=g_PI*cyl.hh*sqr(sqr(cyl.r))*(1.0/2), z2=g_PI*sqr(cyl.r)*cube(cyl.hh)*(2.0f/3);
			pgeom->Ibody.Set(x2+z2,x2+z2,x2*2);
		} break;
		case GEOM_BOX: {
			const box& Box = geom->m_geom.box;
			pgeom->origin = Box.center;
			pgeom->q = Quat(Box.Basis.T());
			pgeom->V = Box.size.GetVolume()*8;
			pgeom->Ibody = (sqr(Diag33(Box.size.GetPermutated(0)))+sqr(Diag33(Box.size.GetPermutated(1)))) * Vec3(pgeom->V*(1.0/3));
		}	break;
		case GEOM_TRIMESH: {
			PxBounds3 bbox = geom->m_geom.mesh.pMesh ? geom->m_geom.mesh.pMesh->getLocalBounds() : geom->m_geom.mesh.pMeshConvex->getLocalBounds();
			pgeom->origin = V(bbox.getCenter());
			pgeom->V = V(bbox.getDimensions()).GetVolume();
			pgeom->Ibody = (sqr(Diag33(V(bbox.getExtents()).GetPermutated(0)))+sqr(Diag33(V(bbox.getExtents()).GetPermutated(1)))) * Vec3(pgeom->V*(1.0/3));
		}
		case GEOM_RAY: {

		}
	}
	pgeom->surface_idx = defSurfaceIdx;
	(pgeom->pGeom=pGeom)->AddRef();	
	return pgeom;
}

int CGeomManager::AddRefGeometry(phys_geometry *pgeom) { return CryInterlockedIncrement(&pgeom->nRefCount); }

int CGeomManager::UnregisterGeometry(phys_geometry *pgeom)
{
	int res = CryInterlockedDecrement(&pgeom->nRefCount);
	if (res <= 0)	{
		delete[] pgeom->pMatMapping;
		pgeom->pGeom->Release();
		delete pgeom;
	}
	return res;
}

template<typename Tprim> PhysXGeom* LoadPrim(CMemStream &stm, CGeomManager *geoman) 
{
	Tprim prim; stm.Read(prim);
	return (PhysXGeom*)geoman->CreatePrimitive(Tprim::type, &prim);
}

phys_geometry *CGeomManager::LoadPhysGeometry(CMemStream &stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char *pIds)
{
	int ver; stm.Read(ver);
	if (ver!=PHYS_GEOM_VER)
		return 0;
	
	PhysXGeom *pGeom = nullptr;
	phys_geometry_serialize pgs;
	stm.Read(pgs);
	int type; stm.Read(type);
	switch (type) {
		case GEOM_CYLINDER: pGeom = LoadPrim<cylinder>(stm,this); break;
		case GEOM_CAPSULE: pGeom = LoadPrim<capsule>(stm,this); break;
		case GEOM_SPHERE: pGeom = LoadPrim<sphere>(stm,this); break;
		case GEOM_BOX: pGeom = LoadPrim<box>(stm,this); break;
		case GEOM_TRIMESH: {
			int nvtx,nTris,flags,dummy;
			stm.Read(nvtx);	stm.Read(nTris);
			stm.Read(dummy);stm.Read(flags);
			bool bVtxMap,bForeignIdx; 
			stm.Read(bVtxMap);
			stm.m_iPos += sizeof(uint16)*nvtx*bVtxMap;
			stm.Read(bForeignIdx);
			strided_pointer<Vec3> pVtx = strided_pointer<Vec3>((Vec3*)pVertices.data,pVertices.iStride);
			uint16 *pIdx = nullptr;
			if (bForeignIdx && !(flags & mesh_full_serialization)) {
				pIdx = new uint16[nTris*3];
				for(int i=0; i<nTris; i++) {
					int idx = stm.Read<uint16>();
					for(int j=0;j<3;j++) pIdx[i*3+j] = pIndices[idx*3+j];
				}
			}	else {
				stm.m_iPos += sizeof(uint16)*nTris*bForeignIdx;
				if (!(flags & mesh_shared_vtx)) {
					pVtx = strided_pointer<Vec3>((Vec3*)(stm.m_pBuf+stm.m_iPos));
					stm.m_iPos += nvtx*sizeof(Vec3);
				}
				pIdx = (uint16*)(stm.m_pBuf+stm.m_iPos);
			}
			pGeom = (PhysXGeom*)CreateMesh(pVtx,pIdx,0,0,nTris,flags);
			break;
		}
	}

	phys_geometry *pgeom = new phys_geometry;
	memset(pgeom, 0, sizeof(phys_geometry));
	pgeom->Ibody = pgs.Ibody;
	pgeom->q = pgs.q;
	pgeom->origin = pgs.origin;
	pgeom->V = pgs.V;
	pgeom->surface_idx = pgs.surface_idx;
	pgeom->nRefCount = 1;
	pgeom->pGeom = pGeom;
	
	return pgeom;
}

void CGeomManager::SavePhysGeometry(CMemStream &stm, phys_geometry *pgeom) 
{
	stm.Write(PHYS_GEOM_VER);
	phys_geometry_serialize pgs;
	pgs.Ibody = pgeom->Ibody;
	pgs.q = pgeom->q;
	pgs.origin = pgeom->origin;
	pgs.V = pgeom->V;
	pgs.surface_idx = pgeom->surface_idx;
	stm.Write(pgs);

	int itype = pgeom->pGeom->GetType();
	stm.Write(itype);
	if (itype != GEOM_TRIMESH) {
		int size = 0;
		switch (itype) {
			case GEOM_CYLINDER: case GEOM_CAPSULE: size = sizeof(capsule); break;
			case GEOM_SPHERE: size = sizeof(sphere); break;
			case GEOM_BOX: size = sizeof(box);
		}
		stm.Write(pgeom->pGeom->GetData(), size);
	} else {
		mesh_data &md = *(mesh_data*)pgeom->pGeom->GetData();
		stm.Write(md.nVertices);
		stm.Write(md.nTris);
		stm.Write(8);	// dummy max vertex valency
		stm.Write(md.flags & ~(mesh_shared_vtx | mesh_shared_idx));
		stm.Write(false); // no vtx map
		stm.Write(false); // no foreign idx
		for(int i=0; i<md.nVertices; stm.Write(md.pVertices[i++]));
		for(int i=0; i<md.nTris*3; stm.Write((uint16)md.pIndices[i++]));
	}
}

void CGeomManager::SetGeomMatMapping(phys_geometry *pgeom, int *pMatMapping, int nMats)
{
	delete[] pgeom->pMatMapping; pgeom->pMatMapping = nullptr;
	if ((pgeom->nMats=nMats) && pMatMapping)
		memcpy(pgeom->pMatMapping = new int[nMats], pMatMapping, nMats*sizeof(int));
}
