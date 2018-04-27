// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef geometries_h
#define geometries_h
#pragma once

using namespace physx;

class PhysXGeom	: public IGeometry
{
public:
	~PhysXGeom();

	virtual int  GetType() { return m_type; }
	virtual int  AddRef() { return ++m_nRefCount; }
	virtual void Release() { if (--m_nRefCount<=0) delete this; }
	virtual void Lock(int bWrite = 1) {}
	virtual void Unlock(int bWrite = 1) {}
	virtual void GetBBox(primitives::box* pbox);
	virtual int  CalcPhysicalProperties(phys_geometry* pgeom) { return 1; }
	virtual int  PointInsideStatus(const Vec3& pt) { return 0; }
	virtual int IntersectLocked(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2, intersection_params* pparams, 
		geom_contact*& pcontacts, WriteLockCond& lock, int iCaller = MAX_PHYS_THREADS) { return Intersect(pCollider,pdata1,pdata2,pparams,pcontacts); }
	virtual int Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2, intersection_params* pparams, geom_contact*& pcontacts);
	virtual int FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1, Vec3* ptres, int nMaxIters = 10) { return 0; }
	virtual void  CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin, const Vec3& centerOfMass, Vec3& P, Vec3& L) {}
	virtual float CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& submergedMassCenter) { return 0; }
	virtual void CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres, Vec3& dLres) {}
	virtual void DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int iLevel, int idxColor) {}
	virtual int  GetPrimitiveId(int iPrim, int iFeature) { return 0; }
	virtual int  GetPrimitive(int iPrim, primitives::primitive* pprim) { return 0; }
	virtual int  GetForeignIdx(int iPrim) { return 0; }
	virtual Vec3 GetNormal(int iPrim, const Vec3& pt) { return Vec3(0,0,1); }
	virtual int  GetFeature(int iPrim, int iFeature, Vec3* pt) { return 0; }
	virtual int  IsConvex(float tolerance) { return 0; }
	virtual void PrepareForRayTest(float raylen) {}
	virtual float BuildOcclusionCubemap(geom_world_data* pgwd, int iMode, SOcclusionCubeMap* cubemap0, SOcclusionCubeMap* cubemap1, int nGrow) { return 0; }
	virtual void GetMemoryStatistics(ICrySizer* pSizer) {}
	virtual void Save(CMemStream& stm) {}
	virtual void Load(CMemStream& stm) {}
	virtual void Load(CMemStream& stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char* pIds) {}
	virtual int  GetPrimitiveCount() { return 1; }
	virtual const primitives::primitive* GetData();
	virtual void SetData(const primitives::primitive*) {}
	virtual float GetVolume();
	virtual Vec3 GetCenter() { return Vec3(0); }
	virtual int Subtract(IGeometry* pGeom, geom_world_data* pdata1, geom_world_data* pdata2, int bLogUpdates = 1) { return 0; }
	virtual int GetSubtractionsCount() { return 0; }
	virtual void* GetForeignData(int iForeignData = 0) { return nullptr; }
	virtual int GetiForeignData() { return 0; }
	virtual void SetForeignData(void* pForeignData, int iForeignData) {}
	virtual int GetErrorCount() { return 0; }
	virtual void DestroyAuxilaryMeshData(int idata) {}
	virtual void RemapForeignIdx(int* pCurForeignIdx, int* pNewForeignIdx, int nTris) {}
	virtual void AppendVertices(Vec3* pVtx, int* pVtxMap, int nVtx) {}
	virtual float GetExtent(EGeomForm eForm) const { return 0; }
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const {}
	virtual void CompactMemory() {}
	virtual int Boxify(primitives::box* pboxes, int nMaxBoxes, const SBoxificationParams& params) { return 0; }
	virtual int Proxify(IGeometry**& pOutGeoms, SProxifyParams* pparams = 0) { return 0; }
	virtual int SanityCheck() { return 1; }

	template<typename Func> auto CreateAndUse(QuatT& trans, const Diag33& scale, Func func) -> auto const;

	int m_type;
	union UGeom {
		UGeom() {}
		primitives::box box;
		primitives::capsule caps;
		primitives::sphere sph;
		primitives::ray ray;

		struct {
			PxTriangleMesh *pMesh;
			PxConvexMesh *pMeshConvex;
			mesh_data *pdata;
		} mesh;
		struct {
			PxHeightField *pHF;
			Vec3 origin;
			float hscale;
			Vec2 step;
		} hf;
	} m_geom;
	volatile int m_nRefCount = 1;
};

extern PxConvexMesh *g_cylMesh;
template<typename Func> auto PhysXGeom::CreateAndUse(QuatT& trans, const Diag33& scale, Func func) -> auto const
{
	if (!g_cylMesh) {
		PxConvexMeshDesc cmd;
		const int tess = 32;
		Vec3 pt[tess*2],pt0(1,1,0);
		const float dsin=sin(gf_PI2/tess), dcos=cos(gf_PI2/tess);
		for(int i=0; i<tess; i++,pt0=pt0.GetRotated(Vec3(1,0,0),dcos,dsin))	
			(pt[i+tess]=(pt[i]=pt0)).x = -1;
		cmd.points.count = tess*2;
		cmd.points.data = pt;
		cmd.points.stride = sizeof(Vec3);
		cmd.flags = PxConvexFlag::eCOMPUTE_CONVEX;
		PxDefaultMemoryOutputStream buf;
		PxConvexMeshCookingResult::Enum result;
		cpx::g_cryPhysX.Cooking()->cookConvexMesh(cmd, buf, &result);
		g_cylMesh = cpx::g_cryPhysX.Physics()->createConvexMesh(PxDefaultMemoryInputData(buf.getData(), buf.getSize()));
	}

	switch (m_type) {
		case GEOM_CYLINDER: case GEOM_CAPSULE: {
			float r = m_geom.caps.r*(scale*m_geom.caps.axis.GetOrthogonal().normalized()).len(), hh = m_geom.caps.hh*(scale*m_geom.caps.axis).len();
			trans = trans*QuatT(Quat::CreateRotationV0V1(Vec3(1,0,0),m_geom.caps.axis), scale*m_geom.caps.center);
			return m_type==GEOM_CAPSULE ? 
				func(PxCapsuleGeometry(r,hh)) :
				func(PxConvexMeshGeometry(g_cylMesh,PxMeshScale(PxVec3(hh,r,r),PxQuat0)));
		}
		case GEOM_BOX: {
			// non-uniform scaling is supported for axis-ligned Basis only (scale can be projected into the box's frame)
			Vec3 sz = max(max(scale.x,scale.y),scale.z)-min(min(scale.x,scale.y),scale.z)>0.001f ? (m_geom.box.Basis*(scale*(m_geom.box.size*m_geom.box.Basis))).abs() : m_geom.box.size*scale.x;
			trans = trans*QuatT(!Quat(m_geom.box.Basis), scale*m_geom.box.center);
			return func(PxBoxGeometry(V(sz)));
		}
		case GEOM_SPHERE:
			trans = trans*QuatT(Quat(IDENTITY), scale*m_geom.sph.center);
			return func(PxSphereGeometry(m_geom.sph.r*scale.x )); // scale sphere radius according to scale.x - to enable at least some homogeneous scaling for spheres
		case GEOM_HEIGHTFIELD:
			trans = trans*QuatT(Quat::CreateRotationAA(2*gf_PI/3,Vec3(1/sqrt3)), scale*m_geom.hf.origin);
			return func(PxHeightFieldGeometry(m_geom.hf.pHF,PxMeshGeometryFlags(),m_geom.hf.hscale,m_geom.hf.step.x,m_geom.hf.step.y));
		case GEOM_TRIMESH:
			return m_geom.mesh.pMesh ? 
				func(PxTriangleMeshGeometry(m_geom.mesh.pMesh, PxMeshScale(V(scale),PxQuat0))) : 
				func(PxConvexMeshGeometry(m_geom.mesh.pMeshConvex, PxMeshScale(V(scale),PxQuat0)));
	}
	return func(PxSphereGeometry(1));
}

#endif