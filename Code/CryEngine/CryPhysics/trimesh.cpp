// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "overlapchecks.h"
#include "intersectionchecks.h"
#include "unprojectionchecks.h"
#include "bvtree.h"
#include "aabbtree.h"
#include "obbtree.h"
#include "singleboxtree.h"
#include "geometry.h"
#include "trimesh.h"
#include "raybv.h"
#include "raygeom.h"
#include <CryMath/GeomQuery.h>

InitTriMeshGlobals::InitTriMeshGlobals() {
  for(int iCaller=0; iCaller<=MAX_PHYS_THREADS; iCaller++) {
    memset(G(UsedVtxMap), 0, sizeof(G(UsedVtxMap)));
    memset(G(UsedTriMap), 0, sizeof(G(UsedTriMap)));
    G(BrdPtBufPos) = 0; G(PolyPtBufPos) = 0;
  }
}
static InitTriMeshGlobals initTriMeshGlobals;
static volatile int g_lockBool2d=0;

CTriMesh::CTriMesh()
	: m_pTree(nullptr)
	, m_pIndices(nullptr)
	, m_pIds(nullptr)
	, m_pForeignIdx(nullptr)
	, m_pVertices(nullptr)
	, m_pNormals(nullptr)
	, m_pVtxMap(nullptr)
	, m_pTopology(nullptr)
	, m_nTris(0)
	, m_nVertices(0)
	, m_pIslands(nullptr)
	, m_nIslands(0)
	, m_pTri2Island(nullptr)
	, m_flags(0)
	, m_pIdxNew2Old(nullptr)
	, m_nMaxVertexValency(0)
	, m_nHashPlanes(0)
	, m_lockHash(0)
	, m_bMultipart(0)
	, m_V(0.0f)
	, m_nErrors(0)
	, m_pMeshUpdate(nullptr)
	, m_iLastNewTriIdx(BOP_NEWIDX0)
	, m_nMessyCutCount(0)
	, m_nSubtracts(0)
	, m_pUsedTriMap(nullptr)
	, m_ivtx0(0)
	, m_voxgrid(0)
{ 
	ZeroArray(m_pHashGrid);
	ZeroArray(m_pHashData);

	static_assert(CRY_ARRAY_COUNT(m_bConvex) == CRY_ARRAY_COUNT(m_ConvexityTolerance), "Invalid array size!");
	for(int i=0;i<CRY_ARRAY_COUNT(m_bConvex);i++)	{
		m_bConvex[i]=0;
		m_ConvexityTolerance[i]=-1;
	}
	
	MARK_UNUSED m_V,m_center;
	
	//CGeometry
	m_iCollPriority = 1;
}


CTriMesh::~CTriMesh()
{
	if (m_pTree) delete m_pTree;
	if (m_pTopology && !(m_flags & mesh_shared_topology)) delete[] m_pTopology;
	if (m_pIndices && !(m_flags & mesh_shared_idx)) delete[] m_pIndices;
	if (m_pIds && !(m_flags & mesh_shared_mats)) delete[] m_pIds;
	if (m_pForeignIdx && !(m_flags & mesh_shared_foreign_idx)) delete[] m_pForeignIdx;
	if (m_pVertices && !(m_flags & mesh_shared_vtx)) delete[] m_pVertices.data;
	if (m_pNormals && !(m_flags & mesh_shared_normals)) delete[] m_pNormals;
	for(int i=0;i<m_nHashPlanes;i++) {
		delete[] m_pHashGrid[i]; delete[] m_pHashData[i];
	}
	if (m_pTri2Island) delete[] m_pTri2Island;
	if (m_pIslands) delete[] m_pIslands;
	if (m_pVtxMap) delete[] m_pVtxMap;
	if (m_pMeshUpdate) delete m_pMeshUpdate;
	if (m_pUsedTriMap) delete[] m_pUsedTriMap;
	if (m_voxgrid) delete m_voxgrid;
}

inline void swap(int *v, int i1,int i2)
{	int ti=v[i1]; v[i1]=v[i2]; v[i2]=ti; }

int bin_search(int *v,index_t *idx,int n,int refidx)
{
	int left=0,right=n,m;
	do {
		m = left+right>>1;
		if (idx[v[m]*3]==refidx) return m;
		if (idx[v[m]*3]<refidx) left=m;
		else right=m;
	} while (left<right-1);
	return left;
}

template<class ftype> void qsort(int *v,strided_pointer<ftype> pkey, int left,int right)
{
	int i, j, mi;
	ftype m;

	i = left;
	j = right;
	mi = (left + right)/2;
	m = pkey[v[mi]];
	while (i <= j) {
		while(pkey[v[i]] < m) i++;
		while(m < pkey[v[j]]) j--;
		if (i <= j) {
			swap( v, i, j );
			i++; j--;
		}
	}
	if (left < j) qsort(v, pkey, left, j);
	if (i < right) qsort(v, pkey, i, right);
}

void qsort(int *v, int left,int right)
{
	int i,j, mi;
	int m;

	i = left;
	j = right;
	mi = (left + right)/2;
	m = v[mi];
	while (i <= j) {
		while(v[i] < m) i++;
		while(m < v[j]) j--;
		if (i <= j) {
			swap( v, i, j );
			i++; j--;
		}
	}
	if (left < j) qsort(v, left, j);
	if (i < right) qsort(v, i, right);
}


int __gtrimesh = 0;

CTriMesh* CTriMesh::CreateTriMesh(strided_pointer<const Vec3> pVertices,strided_pointer<unsigned short> pIndices,char *pIds,int *pForeignIdx,
																	int nTris, int flags, int nMinTrisPerNode,int nMaxTrisPerNode, float favorAABB)
{
	__gtrimesh++;
	int i;
	if (sizeof(index_t)<4)
		nTris = min(nTris,10000);
	m_nTris = nTris;
	m_pNormals = new Vec3[m_nTris];
	m_flags = flags & (mesh_shared_vtx|mesh_shared_idx|mesh_shared_mats|mesh_shared_foreign_idx|mesh_keep_vtxmap|mesh_always_static|mesh_full_serialization);
	if (!(flags & mesh_shared_idx) || pIndices.iStride!=sizeof(index_t))	{
		m_pIndices = new index_t[m_nTris*3];
		for(i=0;i<m_nTris*3;i++) m_pIndices[i] = pIndices[i];
		m_flags &= ~mesh_shared_idx;
	} else
		m_pIndices = (index_t*)pIndices.data;
	m_nVertices = 0;
	for(i=0;i<m_nTris;i++)
		m_nVertices = max(m_nVertices, (int)max(max(m_pIndices[i*3],m_pIndices[i*3+1]),m_pIndices[i*3+2]));
	m_nVertices++;
	if (!(flags & mesh_shared_vtx))	{
		m_pVertices = strided_pointer<Vec3>(new Vec3[m_nVertices]);
		for(i=0;i<m_nVertices;i++) m_pVertices[i] = pVertices[i];
	} else
		m_pVertices = strided_pointer<Vec3>((Vec3*)pVertices.data,pVertices.iStride); // removes const modifier
	if (!(flags & mesh_VoxelGrid)) {
		unsigned char *pValency = new unsigned char[m_nVertices];
		memset(pValency, 0, m_nVertices);
		for(i=nTris*3-1;i>=0;i--) pValency[m_pIndices[i]]++;
		for(m_nMaxVertexValency=i=0;i<m_nVertices;i++)
			m_nMaxVertexValency = max(m_nMaxVertexValency, (int)pValency[i]);
		delete[] pValency;
	} else
		m_nMaxVertexValency = 8;

	if (!(flags & mesh_shared_mats) && pIds) {
		m_pIds = new char[m_nTris];
		for(i=0;i<nTris;i++) m_pIds[i] = pIds[i];
	}	else
		m_pIds = pIds;
	if (!(flags & mesh_shared_foreign_idx) && pForeignIdx)
		memcpy(m_pForeignIdx = new int[m_nTris], pForeignIdx, m_nTris*sizeof(int));
	else if ((flags & mesh_shared_foreign_idx) && !pForeignIdx)	{
		m_pForeignIdx = new int[m_nTris]; m_flags &= ~mesh_shared_foreign_idx;
		for(i=0;i<m_nTris;i++) m_pForeignIdx[i] = i;
	}	else
		m_pForeignIdx = pForeignIdx;

	int *vsort[3],j,k,*pList[4],nList[4];
	struct vtxinfo {
		int id;
		Vec3i isorted;
	};
	vtxinfo *pVtx;
	float coord;

	// for mesh connectivity calculations, temporarily assign same ids to the vertices that have the same coordinates
	pVtx = new vtxinfo[m_nVertices];
	if (!(flags & mesh_no_vtx_merge)) {
		static float mergeTol = 1e-5f;
		for(i=0;i<3;i++) vsort[i] = new int[max(m_nTris,m_nVertices)];
		for(i=0;i<4;i++) pList[i] = new int[m_nVertices];
		for(i=0;i<m_nVertices;i++) vsort[0][i]=vsort[1][i]=vsort[2][i]=i;
		for(j=0;j<3;j++) {
			qsort(vsort[j],strided_pointer<float>((float*)m_pVertices.data+j,m_pVertices.iStride),0,m_nVertices-1);
			for(i=0;i<m_nVertices;i++) pVtx[vsort[j][i]].isorted[j] = i;
		}
		for(i=0;i<m_nVertices;i++) pVtx[i].id=-1;
		for(i=0;i<m_nVertices;i++) if (pVtx[i].id<0) {
			for(j=0;j<3;j++) {
				for(k=pVtx[i].isorted[j],coord=m_pVertices[i][j]; k>0 && fabs_tpl(m_pVertices[vsort[j][k-1]][j]-coord)<=mergeTol; k--);
				for(nList[j]=0; k<m_nVertices && fabs_tpl(m_pVertices[vsort[j][k]][j]-coord)<=mergeTol; k++)
					pList[j][nList[j]++] = vsort[j][k];
				qsort(pList[j],0,nList[j]-1);
			}
			nList[3] = intersect_lists(pList[0],nList[0], pList[1],nList[1], pList[3]);
			nList[0] = intersect_lists(pList[3],nList[3], pList[2],nList[2], pList[0]);
			for(k=0;k<nList[0];k++)
				pVtx[pList[0][k]].id = i;
		}
		for(i=0;i<4;i++) delete[] pList[i];
		for(i=0;i<3;i++) delete[] vsort[i];

		if (!(flags & mesh_no_filter)) {
			// move degenerate triangles to the end of the list
			for(i=0,j=m_nTris; i<j;)
			if (iszero(pVtx[m_pIndices[i*3]].id^pVtx[m_pIndices[i*3+1]].id) | iszero(pVtx[m_pIndices[i*3+1]].id^pVtx[m_pIndices[i*3+2]].id) |
					iszero(pVtx[m_pIndices[i*3+2]].id^pVtx[m_pIndices[i*3]].id))
			{
				j--;
				for(k=0;k<3;k++) { int itmp=m_pIndices[j*3+k]; m_pIndices[j*3+k]=m_pIndices[i*3+k]; m_pIndices[i*3+k]=itmp; }
				if (m_pIds) { char stmp=m_pIds[j]; m_pIds[j]=m_pIds[i]; m_pIds[i]=stmp; }
				if (m_pForeignIdx) { int itmp=m_pForeignIdx[j]; m_pForeignIdx[j]=m_pForeignIdx[i]; m_pForeignIdx[i]=itmp; }
			}	else i++;
			m_nTris = i;
		}
	} else
		for(i=0;i<m_nVertices;i++) pVtx[i].id = i;

	for(i=0;i<m_nTris*3;i++) m_pIndices[i] = pVtx[m_pIndices[i]].id;
	if (flags & (mesh_keep_vtxmap|mesh_keep_vtxmap_for_saving))	{
		if (!(flags & mesh_keep_vtxmap))
			for(i=j=0;i<m_nVertices;i++) j|=sqr(pVtx[i].id-i);
		else j=1;
		if (j) {
			m_pVtxMap = new int[m_nVertices];
			for(i=0;i<m_nVertices;i++)
				m_pVtxMap[i] = pVtx[i].id;
		}
	}
	delete[] pVtx;

	box bbox;
	Vec3r axes[3],center;
	if (flags & (mesh_AABB_rotated | mesh_SingleBB)) {
		index_t *pHullTris=0;
		int nHullTris;
		if (m_nTris<=20 || !(nHullTris = qhull(m_pVertices,m_nVertices, pHullTris))) {
			nHullTris=m_nTris; pHullTris=m_pIndices;
		}
		ComputeMeshEigenBasis(m_pVertices,strided_pointer<index_t>(pHullTris),nHullTris, axes,center);
		if (pHullTris!=m_pIndices)
			delete[] pHullTris;
	}

	if (flags & mesh_SingleBB) {
		CSingleBoxTree *pTree = new CSingleBoxTree;
		bbox.Basis = GetMtxFromBasis(axes);
			Vec3 pt,ptmin(VMAX),ptmax(VMIN);
			for(i=0;i<m_nTris;i++) for(j=0;j<3;j++) {
				pt = bbox.Basis*m_pVertices[m_pIndices[i*3+j]];
				ptmin.x=min_safe(ptmin.x,pt.x); ptmin.y=min_safe(ptmin.y,pt.y); ptmin.z=min_safe(ptmin.z,pt.z);
				ptmax.x=max_safe(ptmax.x,pt.x); ptmax.y=max_safe(ptmax.y,pt.y); ptmax.z=max_safe(ptmax.z,pt.z);
			}
			bbox.center = ((ptmin+ptmax)*bbox.Basis)*0.5f;
			bbox.bOriented = 1;
			bbox.size = (ptmax-ptmin)*0.5f;
		float mindim = max(max(bbox.size.x,bbox.size.y),bbox.size.z)*0.001f;
		bbox.size.x=max(bbox.size.x,mindim); bbox.size.y=max(bbox.size.y,mindim);	bbox.size.z=max(bbox.size.z,mindim);
		pTree->SetBox(&bbox);
		pTree->Build(this);
		pTree->m_nPrims = m_nTris;
		m_pTree = pTree;
	} else if (flags & (mesh_AABB|mesh_AABB_rotated|mesh_OBB)) {
		CBVTree *pTrees[3];
		index_t *pTreeIndices[3];
		char *pTreeIds[3];
		int *pTreeFIdx[3];
		float volumes[3],skipdim;
		int nTrees=0,iTreeBest;
		skipdim = flags & mesh_multicontact2 ? 0.0f : (flags & mesh_multicontact0 ? 10.0f : 0.1f);

		if (flags & mesh_AABB) {
			Matrix33 Basis; Basis.SetIdentity();
			CAABBTree *pTree = new CAABBTree;
			pTree->SetParams(nMinTrisPerNode,nMaxTrisPerNode,skipdim,Basis,flags&mesh_AABB_plane_optimise);
			volumes[nTrees] = (pTrees[nTrees]=pTree)->Build(this);
			++nTrees;
		}
		if (flags & mesh_AABB_rotated) {
			if (nTrees>0) {
				memcpy(pTreeIndices[nTrees-1] = new index_t[m_nTris*3], m_pIndices, sizeof(m_pIndices[0])*m_nTris*3);
				if (m_pIds) memcpy(pTreeIds[nTrees-1] = new char[m_nTris], m_pIds, sizeof(m_pIds[0])*m_nTris);
				if (m_pForeignIdx) memcpy(pTreeFIdx[nTrees-1] = new int[m_nTris], m_pForeignIdx, sizeof(m_pForeignIdx[0])*m_nTris);
			}
			CAABBTree *pTree = new CAABBTree;
			Matrix33 Basis = GetMtxFromBasis(axes);
			pTree->SetParams(nMinTrisPerNode,nMaxTrisPerNode,skipdim,Basis,flags&mesh_AABB_plane_optimise);
			volumes[nTrees] = (pTrees[nTrees]=pTree)->Build(this)*1.01f; // favor non-oriented AABBs slightly
			++nTrees;
		}
		if (flags & mesh_OBB) {
			if (nTrees>0) {
				memcpy(pTreeIndices[nTrees-1] = new index_t[m_nTris*3], m_pIndices, sizeof(m_pIndices[0])*m_nTris*3);
				if (m_pIds) memcpy(pTreeIds[nTrees-1] = new char[m_nTris], m_pIds, sizeof(m_pIds[0])*m_nTris);
				if (m_pForeignIdx) memcpy(pTreeFIdx[nTrees-1] = new int[m_nTris], m_pForeignIdx, sizeof(m_pForeignIdx[0])*m_nTris);
			}
			COBBTree *pTree = new COBBTree;
			pTree->SetParams(nMinTrisPerNode,nMaxTrisPerNode,skipdim);
			volumes[nTrees] = (pTrees[nTrees]=pTree)->Build(this)*favorAABB;
			++nTrees;
		}
		for(iTreeBest=0,i=1;i<nTrees;i++) if (volumes[i]<volumes[iTreeBest])
			iTreeBest = i;
		m_pTree = pTrees[iTreeBest];
		if (iTreeBest!=nTrees-1) {
			memcpy(m_pIndices, pTreeIndices[iTreeBest], sizeof(m_pIndices[0])*m_nTris*3);
			if (m_pIds) memcpy(m_pIds, pTreeIds[iTreeBest], sizeof(m_pIds[0])*m_nTris);
			if (m_pForeignIdx) memcpy(m_pForeignIdx, pTreeFIdx[iTreeBest], sizeof(m_pForeignIdx[0])*m_nTris);
		}
		for(i=0;i<nTrees;i++) if (i!=iTreeBest)
			delete pTrees[i];
		for(i=0;i<nTrees-1;i++) {
			delete[] pTreeIndices[i];
			if (m_pIds) delete[] pTreeIds[i];
			if (m_pForeignIdx) delete[] pTreeFIdx[i];
		}
	}
	m_flags &= ~(mesh_OBB | mesh_AABB | mesh_AABB_rotated | mesh_SingleBB);
	if (m_pTree) switch (m_pTree->GetType()) {
		case BVT_AABB: m_flags |= mesh_AABB; break;
		case BVT_OBB:	m_flags |= mesh_OBB; break;
		case BVT_SINGLEBOX:	m_flags |= mesh_SingleBB; break;
	}

	// BVtree may have sorted indices, so calc normals and topology after the tree
	strided_pointer<Vec3mem> pVtxMem = strided_pointer<Vec3mem>((Vec3mem*)m_pVertices.data,m_pVertices.iStride);
	for(i=0;i<m_nTris;i++) // precalculate normals
		m_pNormals[i] = (pVtxMem[m_pIndices[i*3+1]]-pVtxMem[m_pIndices[i*3]] ^
										 pVtxMem[m_pIndices[i*3+2]]-pVtxMem[m_pIndices[i*3]]).normalized();

	// fill topology information - find 3 edge neighbours for every triangle
	m_pTopology = new trinfo[m_nTris];
	CalculateTopology(m_pIndices);

	if (!(flags & mesh_VoxelGrid)) {
		// detect if we have unconnected regions (floodfill starting from the 1st tri)
		m_bMultipart = BuildIslandMap()>1;

		if ((m_bIsConvex = IsConvex(0.02f)) && m_pTree)
			m_pTree->SetGeomConvex();
		if (m_pTree) {
			m_pTree->GetBBox(&bbox);
			m_minVtxDist = max(max(bbox.size.x,bbox.size.y),bbox.size.z)*0.0002f;
		}
	} else {
		m_bMultipart = 1;
		m_bIsConvex = 0;
	}
	return this;
}


CTriMesh *CTriMesh::Clone(CTriMesh *src, int flags)
{
	m_lockUpdate = 0;
	m_flags = src->m_flags & ~(mesh_shared_vtx|mesh_shared_idx|mesh_shared_mats|mesh_shared_foreign_idx) | flags;
	m_nTris = src->m_nTris; m_nVertices = src->m_nVertices;

	if (!(flags & mesh_shared_vtx)) {
		m_pVertices = strided_pointer<Vec3>(new Vec3[m_nVertices]);
		for(int i=0;i<m_nVertices;i++) m_pVertices[i] = src->m_pVertices[i];
	} else m_pVertices = src->m_pVertices;
	if (!(flags & mesh_shared_idx))
		memcpy(m_pIndices = new index_t[m_nTris*3], src->m_pIndices,m_nTris*3*sizeof(m_pIndices[0]));
	else m_pIndices = src->m_pIndices;
	if (!(flags & mesh_shared_mats) && src->m_pIds)
		memcpy(m_pIds = new char[m_nTris], src->m_pIds,m_nTris*sizeof(m_pIds[0]));
	else m_pIds = src->m_pIds;
	if (!(flags & mesh_shared_foreign_idx) && src->m_pForeignIdx)
		memcpy(m_pForeignIdx = new int[m_nTris], src->m_pForeignIdx,m_nTris*sizeof(m_pForeignIdx[0]));
	else m_pForeignIdx = src->m_pForeignIdx;
	if (!(flags & mesh_shared_topology))
		memcpy(m_pTopology = new trinfo[m_nTris], src->m_pTopology,m_nTris*sizeof(m_pTopology[0]));
	else m_pTopology = src->m_pTopology;
	memcpy(m_pNormals = new Vec3[m_nTris], src->m_pNormals,m_nTris*sizeof(m_pNormals[0]));
	if (src->m_pVtxMap)
		memcpy(m_pVtxMap = new int[m_nVertices], src->m_pVtxMap,m_nVertices*sizeof(m_pVtxMap[0]));

	m_nMaxVertexValency = src->m_nMaxVertexValency;
	for(int i=0;i<4;i++) {
		m_bConvex[i] = src->m_bConvex[i];
		m_ConvexityTolerance[i] = src->m_ConvexityTolerance[i];
	}
	m_bMultipart = src->m_bMultipart;

	m_minVtxDist = src->m_minVtxDist;
	m_iCollPriority = src->m_iCollPriority;
	m_bIsConvex = src->m_bIsConvex;
	m_pForeignData = src->m_pForeignData;
	m_iForeignData = src->m_iForeignData;
	m_nSubtracts = src->m_nSubtracts;
	m_nErrors = src->m_nErrors;

	CMemStream stm(false);
	src->m_pTree->Save(stm);
	switch (src->m_pTree->GetType()) {
		case BVT_OBB: m_pTree = new COBBTree; break;
		case BVT_AABB: m_pTree = new CAABBTree; break;
		case BVT_SINGLEBOX: m_pTree = new CSingleBoxTree; break;
	}
	stm.m_iPos = 0;
	if (m_pTree)
		m_pTree->Load(stm,this);

	if (src->m_pTri2Island && (m_nIslands = src->m_nIslands)) {
		memcpy(m_pIslands = new mesh_island[m_nIslands], src->m_pIslands, m_nIslands*sizeof(mesh_island));
		memcpy(m_pTri2Island = new tri_flags[m_nTris], src->m_pTri2Island, m_nTris*sizeof(tri_flags));
	}

	return this;
}


int CTriMesh::CalculateTopology(index_t *pIndices, int bCheckOnly)
{
	int i,j,i1,j1,nTris,itri,*pTris,*pVtxTris,*pEdgeTris,nBadEdges[2]={0,0};
	Vec3 edge,v0,v1;
	float elen2;
	quotientf sina,minsina;
	trinfo dummy,*pTopology = !bCheckOnly ? m_pTopology : &dummy;
	strided_pointer<Vec3mem> pVtxMem = strided_pointer<Vec3mem>((Vec3mem*)m_pVertices.data,m_pVertices.iStride);
	memset(pTris = new int[m_nVertices+1], 0,(m_nVertices+1)*sizeof(int)); // for each used vtx, points to the corresponding tri list start
	pVtxTris = new int[m_nTris*4]; // holds tri lists for each used vtx
	pEdgeTris = pVtxTris+m_nTris*3;

	for(i=0;i<m_nTris;i++) for(j=0;j<3;j++)
		pTris[pIndices[i*3+j]]++;
	for(i=0;i<m_nVertices;i++) pTris[i+1] += pTris[i];
	for(i=m_nTris-1;i>=0;i--) for(j=0;j<3;j++)
		pVtxTris[--pTris[pIndices[i*3+j]]] = i;
	for(i=0;i<m_nTris;i++) for(j=0;j<3;j++) {
		nTris = intersect_lists(pVtxTris+pTris[pIndices[i*3+j]], pTris[pIndices[i*3+j]+1]-pTris[pIndices[i*3+j]],
			pVtxTris+pTris[pIndices[i*3+inc_mod3[j]]], pTris[pIndices[i*3+inc_mod3[j]]+1]-pTris[pIndices[i*3+inc_mod3[j]]], pEdgeTris);
    assert(nTris <= m_nTris); // check for possible memory corruption in case of degenerative triangles
		// if (nTris!=2) - the mesh is not manifold
		if (nTris==2)
			pTopology[i & ~-bCheckOnly].ibuddy[j] = pEdgeTris[iszero(i-pEdgeTris[0])];
		else {
			// among the other triangles sharing this edge, find the one that has it in reverse order
			edge = pVtxMem[m_pIndices[i*3+inc_mod3[j]]]-pVtxMem[m_pIndices[i*3+j]];
			v0 = pVtxMem[m_pIndices[i*3+dec_mod3[j]]]-pVtxMem[m_pIndices[i*3+j]];
			v0 = v0*(elen2=edge.len2())-edge*(edge*v0);
			itri = -1; minsina.set(3,1);
			for(i1=0;i1<nTris;i1++) if (pEdgeTris[i1]!=i) for(j1=0;j1<3;j1++)
				if (pIndices[i*3+j]==pIndices[pEdgeTris[i1]*3+inc_mod3[j1]] && pIndices[i*3+inc_mod3[j]]==pIndices[pEdgeTris[i1]*3+j1]) {
					v1 = pVtxMem[m_pIndices[pEdgeTris[i1]*3+dec_mod3[j1]]]-pVtxMem[m_pIndices[i*3+j]];
					v1 = v1*elen2-edge*(edge*v1);
					sina.set(sqr_signed((v0^v1)*edge), v0.len2()*v1.len2()*elen2);
					float denom = isneg(1e10f-sina.y)*1e-15f;
					sina.x*=denom; sina.y*=denom;
					sina += isneg(sina)*2;
					if (sina<minsina) {
						minsina = sina; itri = pEdgeTris[i1];
					}
				}
			pTopology[i & ~-bCheckOnly].ibuddy[j] = itri;
			nBadEdges[min(nTris-1,1)]++;
		}
	}
/*	for(i=0;i<m_nTris;i++) for(j=0;j<3;j++) if ((idx=m_pTopology[i].ibuddy[j])>=0 &&
		pIndices[i*3+j]+pIndices[i*3+inc_mod3[j]] != pIndices[idx*3+GetEdgeByBuddy(idx,i)]+pIndices[idx*3+inc_mod3[GetEdgeByBuddy(idx,i)]])
		i=i;*/

	delete[] pVtxTris;
	delete[] pTris;
	return !bCheckOnly ? ((m_nErrors=nBadEdges[0]+nBadEdges[1])==0) : nBadEdges[0]+nBadEdges[1];
}


void CTriMesh::RebuildBVTree(CBVTree *pRefTree)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int nMinTrisPerNode=2,nMaxTrisPerNode=4,i,j,jnext,*pIdxOld2New;
	Vec3 n,nnext,BBox[2];
	trinfo t,tnext;
	box bbox;
	CBVTree *pTree = pRefTree ? pRefTree : m_pTree;
	pIdxOld2New = new int[m_nTris*2];
	m_pIdxNew2Old = pIdxOld2New+m_nTris;
	for(i=0;i<m_nTris;i++)
		m_pIdxNew2Old[i] = i;
	bbox.Basis.SetIdentity();
	if (pTree)
		pTree->GetBBox(&bbox);

	if (m_nTris>0) {
		if (m_flags & mesh_OBB) {
		if (pTree) {
				nMinTrisPerNode = ((COBBTree*)pTree)->m_nMinTrisPerNode;
				nMaxTrisPerNode = ((COBBTree*)pTree)->m_nMaxTrisPerNode;
			}
		} else if (m_flags & mesh_AABB) {
			if (pTree) {
				nMinTrisPerNode = ((CAABBTree*)pTree)->m_nMinTrisPerNode;
				nMaxTrisPerNode = ((CAABBTree*)pTree)->m_nMaxTrisPerNode;
			}
		} else if (!(m_flags & mesh_SingleBB))
			return;
		else if (m_nTris<=12) {
			if (pTree)
				bbox = ((CSingleBoxTree*)pTree)->m_Box;
			BBox[0]=BBox[1] = bbox.Basis*m_pVertices[m_pIndices[0]];
			for(i=0;i<m_nTris;i++) for(j=0;j<3;j++)	{
				n = bbox.Basis*m_pVertices[m_pIndices[i*3+j]];
				BBox[0] = min(BBox[0],n); BBox[1] = max(BBox[1],n);
			}
			bbox.size = (BBox[1]-BBox[0])*0.5f;
			bbox.center = ((BBox[1]+BBox[0])*0.5f)*bbox.Basis;
		}	else
			m_flags = m_flags & ~mesh_SingleBB | mesh_AABB;
	} else {
		m_flags = (m_flags & ~(mesh_AABB|mesh_OBB) | mesh_SingleBB);
		bbox.Basis.SetIdentity();
		bbox.center.zero(); bbox.size.Set(1E-10f,1E-10f,1E-10f);
	}
	delete m_pTree;
	float skipdim = m_flags & mesh_multicontact2 ? 0.0f:0.01f;
	if (m_flags & (mesh_AABB|mesh_force_AABB)) {
		CBVTree *pTreeOrg = m_pTree;
		((CAABBTree*)(m_pTree = new CAABBTree))->SetParams(nMinTrisPerNode,nMaxTrisPerNode,skipdim,bbox.Basis);
		if (!pTreeOrg)
			((CAABBTree*)m_pTree)->m_maxDepth = 15;
		m_flags = m_flags & ~(mesh_OBB|mesh_SingleBB) | mesh_AABB;
	} else if (m_flags & mesh_OBB)
		((COBBTree*)(m_pTree = new COBBTree))->SetParams(nMinTrisPerNode,nMaxTrisPerNode,skipdim);
	else {
		((CSingleBoxTree*)(m_pTree = new CSingleBoxTree))->SetBox(&bbox);
		((CSingleBoxTree*)m_pTree)->m_nPrims = m_nTris;
	}
	m_pTree->Build(this);

	for(i=0;i<CRY_ARRAY_COUNT(m_bConvex);m_ConvexityTolerance[i++]=-1);
	if (m_bIsConvex = IsConvex(0.02f))
		m_pTree->SetGeomConvex();

	for(i=0;i<m_nTris;i++)
		pIdxOld2New[m_pIdxNew2Old[i]] = i;
	for(i=0;i<m_nTris;i++) for(j=0;j<3;j++)
		m_pTopology[i].ibuddy[j] = pIdxOld2New[max((index_t)0,m_pTopology[i].ibuddy[j])] | m_pTopology[i].ibuddy[j]>>31;
	for(i=0;i<m_nTris;i++) if (pIdxOld2New[i]!=i) {
		jnext = pIdxOld2New[i]; nnext = m_pNormals[i]; tnext = m_pTopology[i];
		do {
			j = jnext; n = nnext; t = tnext;
			nnext = m_pNormals[j];  m_pNormals[j] = n;
			tnext = m_pTopology[j]; m_pTopology[j] = t;
			jnext = pIdxOld2New[j]; pIdxOld2New[j] = j;
		}	while(j!=i);
	}

/*#ifdef _DEBUG
	int idx;
	for(i=0;i<m_nTris;i++) if (pIdxOld2New[i]!=i)
		i=i;
	for(i=0;i<m_nTris;i++) for(j=0;j<3;j++) if ((idx=m_pTopology[i].ibuddy[j])>=0 &&
		m_pIndices[i*3+j]+m_pIndices[i*3+inc_mod3[j]] != m_pIndices[idx*3+GetEdgeByBuddy(idx,i)]+m_pIndices[idx*3+inc_mod3[GetEdgeByBuddy(idx,i)]])
		i=i;
	for(i=0;i<m_nTris;i++) {
		n = (m_pVertices[m_pIndices[i*3+1]]-m_pVertices[m_pIndices[i*3]] ^ m_pVertices[m_pIndices[i*3+2]]-m_pVertices[m_pIndices[i*3]]).normalized();
		if (m_pNormals[i]*n<0.998f)
			i=i;
	}
#endif*/

	delete[] pIdxOld2New; m_pIdxNew2Old=0;

	for(i=0;i<m_nHashPlanes;i++) {
		delete[] m_pHashGrid[i]; delete[] m_pHashData[i];
	}
	m_nHashPlanes = 0;
}


int CTriMesh::BuildIslandMap()
{
	if (m_pIslands)
		return m_nIslands;

	m_nIslands = 0;
	if (m_nTris>0) {
		int i,iFreeTri,itri,imask,*pTris,nTris,nTrisAlloc;
		Vec3 vtx[3];
		m_pTri2Island = new tri_flags[m_nTris];
		for(i=0;i<m_nTris;i++) {
			m_pTri2Island[i].inext=i+1; m_pTri2Island[i].iprev=i-1;	m_pTri2Island[i].bFree=1;
		}
		m_pTri2Island[0].iprev = m_nTris-1;
		m_pTri2Island[m_nTris-1].inext = 0;
		iFreeTri=0;	m_V=0; m_center.zero();
		pTris = new int[nTrisAlloc=256];

		do {
			if ((m_nIslands & 3)==0)
				ReallocateList(m_pIslands,m_nIslands,m_nIslands+4);
			m_pTri2Island[pTris[0] = iFreeTri].bFree = 0; nTris = 1;
			m_pIslands[m_nIslands].V = 0;
			m_pIslands[m_nIslands].center.zero();
			m_pIslands[m_nIslands].iParent=m_pIslands[m_nIslands].iChild=m_pIslands[m_nIslands].iNext = -1;
			m_pIslands[m_nIslands].itri = 0x7FFF;
			m_pIslands[m_nIslands].bProcessed = 0;
			m_pIslands[m_nIslands].nTris = 0;

			while(nTris) {
				itri = pTris[--nTris];
				m_pTri2Island[m_pTri2Island[itri].iprev].inext = m_pTri2Island[itri].inext;
				m_pTri2Island[m_pTri2Island[itri].inext].iprev = m_pTri2Island[itri].iprev;
				imask = -iszero(iFreeTri-itri);
				iFreeTri = iFreeTri&~imask | m_pTri2Island[itri].inext&imask;
				m_pTri2Island[itri].iprev = m_nIslands;
				m_pTri2Island[itri].inext = m_pIslands[m_nIslands].itri;
				m_pIslands[m_nIslands].itri = itri;
				m_pIslands[m_nIslands].nTris++;
				for(i=0;i<3;i++) vtx[i] = m_pVertices[m_pIndices[itri*3+i]];
				m_pIslands[m_nIslands].V += (Vec2(vtx[1])-Vec2(vtx[0]) ^ Vec2(vtx[2])-Vec2(vtx[0]))*(vtx[0].z+vtx[1].z+vtx[2].z);
				m_pIslands[m_nIslands].center += vtx[0]+vtx[1]+vtx[2];
				for(i=0;i<3;i++) if (m_pTopology[itri].ibuddy[i]>=0 && m_pTri2Island[m_pTopology[itri].ibuddy[i]].bFree) {
					if (nTris==nTrisAlloc)
						ReallocateList(pTris, nTris, nTrisAlloc+=256);
					m_pTri2Island[pTris[nTris++] = m_pTopology[itri].ibuddy[i]].bFree = 0;
				}
			}
			m_V += (m_pIslands[m_nIslands].V*=1.0f/6);
			m_center += (m_pIslands[m_nIslands].center/=(m_pIslands[m_nIslands].nTris*3))*m_pIslands[m_nIslands].V;
			m_nIslands++;
		}	while(m_pTri2Island[iFreeTri].bFree);
		if (m_V>0)
			m_center /= m_V;

		delete[] pTris;
	}

	return m_nIslands;
}


CTriMesh *CTriMesh::SplitIntoIslands(plane *pGround,int nPlanes, int bOriginallyMobile)
{
	WriteLock lock0(m_lockUpdate);
	if (BuildIslandMap()<2)
		return 0;
	if (!ExpandVtxList())
		return 0;

	int iCaller = get_iCaller();
	int i,j,imask,isle,idx,itri,ivtx,nRemovedTri=0,*pTriMap,*pVtxMap;
	CTriMesh *pMesh=0,*pPrevMesh=0;
	bop_meshupdate *pmu,*pmuPrev;
	geom_contact *pcontact;
	CRayGeom aray;
	geom_world_data gwd;
	intersection_params ip;
	box bbox;
	m_pTree->GetBBox(&bbox);
	const Vec3 dirn = Vec3(0,0,1);
	aray.CreateRay(bbox.center,Vec3(0,0,(bbox.size.x+bbox.size.y+bbox.size.z)*3),&dirn);
	ip.bThreadSafe = ip.bThreadSafeMesh = 1;

	memset(pVtxMap = new int[m_nVertices], -1, m_nVertices*sizeof(int));
	memset(pTriMap = new int[m_nTris], -1, m_nTris*sizeof(int));

	for(i=0;i<m_nIslands;i++) m_pIslands[i].bProcessed = 1;
	for(i=0;i<m_nTris;i++) for(idx=0;idx<nPlanes;idx++) for(j=0;j<3;j++)
		m_pIslands[m_pTri2Island[i].iprev].bProcessed &= isnonneg((m_pVertices[m_pIndices[i*3+j]]-pGround[idx].origin)*pGround[idx].n);

	// for islands with negative volumes, check inside what islands they are
	for(isle=0; isle<m_nIslands; isle++) if (m_pIslands[isle].V<0) {
		i = m_pIndices[(itri=m_pIslands[isle].itri)*3];
		for(; itri!=0x7FFF; itri=m_pTri2Island[itri].inext)	for(j=0;j<3;j++) {
			imask = -isneg(m_pVertices[i].z-m_pVertices[m_pIndices[itri*3+j]].z);
			i = i&~imask | m_pIndices[itri*3+j]&imask;
		}
		aray.m_ray.origin = m_pVertices[i];
		if (Intersect(&aray,&gwd,0,&ip,pcontact)) {
			m_pIslands[isle].iParent = m_pTri2Island[pcontact->iPrim[0]].iprev;
			m_pIslands[isle].iNext = m_pIslands[m_pIslands[isle].iParent].iChild;
			m_pIslands[m_pIslands[isle].iParent].iChild = isle;
			m_pIslands[isle].bProcessed = m_pIslands[m_pIslands[isle].iParent].bProcessed;
		}
	}

	if (bOriginallyMobile) for(i=0,idx=1; i<m_nIslands; i++)
		idx &= m_pIslands[i].bProcessed; // skip the 1st island (thus keep it in the mesh) if all islands are marked for splitting
	else idx = 0;
	for(; idx<m_nIslands; idx++) if (m_pIslands[idx].bProcessed && m_pIslands[idx].iParent==-1) {
		pmu = new bop_meshupdate;
		pMesh = new CTriMesh();
		pMesh->m_nRefCount = 0;
		pMesh->m_lockUpdate = 0;
		pMesh->SetForeignData(pPrevMesh,-2);
		pPrevMesh = pMesh;
		pMesh->m_flags = m_flags & ~(mesh_shared_vtx|mesh_shared_idx|mesh_shared_mats|mesh_shared_foreign_idx);
		pMesh->m_nMaxVertexValency = m_nMaxVertexValency;
		pMesh->m_bMultipart = 1+(m_pIslands[idx].iChild>>31); // multipart if it has inside areas
		pMesh->m_minVtxDist = m_minVtxDist;
		pMesh->m_pMeshUpdate = pmu;
		pmu->nextRef = m_refmu.nextRef; pmu->prevRef = &m_refmu;
		m_refmu.nextRef->prevRef = pmu; m_refmu.nextRef = pmu;
		(pmu->pMesh[0] = pMesh)->AddRef(); (pmu->pMesh[1] = this)->AddRef();
		pMesh->m_V = m_pIslands[idx].V;
		pMesh->m_center = m_pIslands[idx].center;

		isle = idx; do {
			for(itri=m_pIslands[isle].itri,i=pMesh->m_nVertices; itri!=0x7FFF; itri=m_pTri2Island[itri].inext) for(j=0;j<3;j++,i-=imask) {
				imask = pVtxMap[m_pIndices[itri*3+j]]>>31;
				pVtxMap[m_pIndices[itri*3+j]] += i+1 & imask;
			}

			ReallocateList(pMesh->m_pVertices.data, pMesh->m_nVertices,i);
			if (m_pVtxMap) {
				ReallocateList(pMesh->m_pVtxMap, pMesh->m_nVertices,i);
				for(j=0;j<i;j++) pMesh->m_pVtxMap[j] = j;
			}
			ReallocateList(pMesh->m_pIndices, pMesh->m_nTris*3,(pMesh->m_nTris+m_pIslands[isle].nTris)*3);
			ReallocateList(pMesh->m_pNormals, pMesh->m_nTris,pMesh->m_nTris+m_pIslands[isle].nTris);
			ReallocateList(pMesh->m_pTopology, pMesh->m_nTris,pMesh->m_nTris+m_pIslands[isle].nTris);
			if (m_pIds)
				ReallocateList(pMesh->m_pIds, pMesh->m_nTris,pMesh->m_nTris+m_pIslands[isle].nTris);
			if (m_pForeignIdx)
				ReallocateList(pMesh->m_pForeignIdx, pMesh->m_nTris,pMesh->m_nTris+m_pIslands[isle].nTris);
			ReallocateList(pmu->pNewVtx, pMesh->m_nVertices,i);
			ReallocateList(pmu->pNewTri, pMesh->m_nTris,pMesh->m_nTris+m_pIslands[isle].nTris);
			pMesh->m_nVertices = pmu->nNewVtx = i;

			for(itri=m_pIslands[isle].itri,i=pMesh->m_nTris; itri!=0x7FFF; itri=m_pTri2Island[itri].inext,i++) {
				pmu->pNewTri[i].areaOrg = m_pNormals[itri]*(
					m_pVertices[m_pIndices[itri*3+1]]-m_pVertices[m_pIndices[itri*3]] ^ m_pVertices[m_pIndices[itri*3+2]]-m_pVertices[m_pIndices[itri*3]]);
				for(j=0;j<3;j++) {
					pMesh->m_pIndices[i*3+j] = ivtx = pVtxMap[m_pIndices[itri*3+j]];
					pmu->pNewVtx[ivtx].idx = ivtx;
					pMesh->m_pVertices[ivtx] = m_pVertices[m_pIndices[itri*3+j]];
					pmu->pNewTri[i].iVtx[j] = -(ivtx+1);
					pmu->pNewVtx[ivtx].iBvtx = m_pIndices[itri*3+j];
					pmu->pNewVtx[ivtx].idxTri[0]=pmu->pNewVtx[ivtx].idxTri[1] = 0;
					pmu->pNewTri[i].area[j].zero()[j] = pmu->pNewTri[i].areaOrg;
				}
				pMesh->m_pNormals[i] = m_pNormals[itri];
				if (m_pIds) pMesh->m_pIds[i] = m_pIds[itri];
				if (m_pForeignIdx) {
					pmu->pNewTri[i].idxOrg = m_pForeignIdx[itri];
					pMesh->m_pForeignIdx[i] = BOP_NEWIDX0+i;
				}
				pmu->pNewTri[i].idxNew = BOP_NEWIDX0+i;
				pmu->pNewTri[i].iop = 1;
				pTriMap[itri] = i;
			}

			for(itri=m_pIslands[isle].itri,i=pMesh->m_nTris; itri!=0x7FFF; itri=m_pTri2Island[itri].inext,i++) for(j=0;j<3;j++)
				pMesh->m_pTopology[i].ibuddy[j] = pTriMap[max((index_t)0,m_pTopology[itri].ibuddy[j])] | m_pTopology[itri].ibuddy[j]>>31;
			pMesh->m_iLastNewTriIdx = BOP_NEWIDX0+(pMesh->m_nTris = pmu->nNewTri = i);

			for(itri=m_pIslands[isle].itri; itri!=0x7FFF; itri=m_pTri2Island[itri].inext)	{
				for(j=0;j<3;j++) pVtxMap[m_pIndices[itri*3+j]] = -1;
				pTriMap[itri] = -2;	nRemovedTri++;
			}
		}	while ((isle = max(m_pIslands[isle].iChild,m_pIslands[isle].iNext))>=0);

		pMesh->RebuildBVTree(m_pTree);
		pMesh->m_flags |= -0 & mesh_should_die;
	}

	if (m_pForeignIdx && nRemovedTri>0) {
		for(i=j=0; i<m_nTris; i++)
			j += iszero(pTriMap[i]+2);
		if (j) {
			pmu = new bop_meshupdate;
			pmu->pMesh[0]=pmu->pMesh[1] = this;
			AddRef(); AddRef();
			pmu->pRemovedTri = new int[(pmu->nRemovedTri=j)+1];
			for(i=j=0; i<m_nTris; i++) {
				pmu->pRemovedTri[j] = m_pForeignIdx[i];
				j += iszero(pTriMap[i]+2);
			}
			if (m_pMeshUpdate) {
				for(pmuPrev=m_pMeshUpdate;pmuPrev->next;pmuPrev=pmuPrev->next);
				pmuPrev->next = pmu;
			} else
				m_pMeshUpdate = pmu;
		}
	}

	if (nRemovedTri>0)
		CompactTriangleList(pTriMap);

	delete[] pTriMap; delete[] pVtxMap;
	delete[] m_pIslands; m_pIslands = 0;
	delete[] m_pTri2Island; m_pTri2Island = 0;
  m_nIslands = 0;

/*#ifdef _DEBUG
	for(i=0;i<m_nTris;i++) for(j=0;j<3;j++) if ((idx=m_pTopology[i].ibuddy[j])>=0 &&
			m_pIndices[i*3+j]+m_pIndices[i*3+inc_mod3[j]] != m_pIndices[idx*3+GetEdgeByBuddy(idx,i)]+m_pIndices[idx*3+inc_mod3[GetEdgeByBuddy(idx,i)]])
		i=i;
	CalculateTopology(m_pIndices);
#endif*/

	if (nRemovedTri>0) {
		MARK_UNUSED m_V,m_center;
		RebuildBVTree();
	}

	return pMesh;
}


void CTriMesh::Empty()
{
	WriteLock lock(m_lockUpdate);
	if (m_nTris>0) {
		bop_meshupdate *pmuPrev, *pmu=new bop_meshupdate;
		pmu->pRemovedTri = new int[(pmu->nRemovedTri = m_nTris)+1];
		pmu->pMesh[0]=pmu->pMesh[1] = this;
		AddRef(); AddRef();
		for(int i=0;i<m_nTris;i++)
			pmu->pRemovedTri[i] = m_pForeignIdx[i];

		if (m_pMeshUpdate) {
			for(pmuPrev=m_pMeshUpdate;pmuPrev->next;pmuPrev=pmuPrev->next);
			pmuPrev->next = pmu;
		} else
			m_pMeshUpdate = pmu;
		MARK_UNUSED m_V,m_center;
		RebuildBVTree();

		m_nTris = 0;
	}
}


void CTriMesh::CollapseTriangleToLine(int itri,int ivtx, int *pTriMap, bop_meshupdate *pmu)
{
	// welds the ivtx+1'th vertex of itri to its ivtx'th vertex
	if (itri>=0 && pTriMap[itri]!=-2) {
		int i,j,iprev,iedge;
		int ibuddy[3] = { m_pTopology[itri].ibuddy[dec_mod3[ivtx]],m_pTopology[itri].ibuddy[inc_mod3[ivtx]],m_pTopology[itri].ibuddy[ivtx] };

		if (!(pmu->nWeldedVtx & 31)) ReallocateList(pmu->pWeldedVtx, pmu->nWeldedVtx,pmu->nWeldedVtx+32);
		pmu->pWeldedVtx[pmu->nWeldedVtx++].set(m_pIndices[itri*3+ivtx], m_pIndices[itri*3+inc_mod3[ivtx]]);

		for(i=ibuddy[1],iprev=itri,j=0; i>=0 && i!=ibuddy[2] && j<32; i=m_pTopology[i].ibuddy[inc_mod3[iedge]],j++) {
			iedge = GetEdgeByBuddy(i,iprev);
			m_pIndices[i*3+inc_mod3[iedge]] = m_pIndices[itri*3+ivtx];
			RecalcTriNormal(i); iprev = i;
		}
		for(i=0;i<2;i++) if (ibuddy[i]>=0)
			m_pTopology[ibuddy[i]].ibuddy[GetEdgeByBuddy(ibuddy[i],itri)] = ibuddy[i^1];
		pTriMap[itri] = -2;
		if (!(pmu->nRemovedTri & 31)) ReallocateList(pmu->pRemovedTri, pmu->nRemovedTri,pmu->nRemovedTri+32);
		pmu->pRemovedTri[pmu->nRemovedTri++] = pmu->pRemovedVtx[itri];

		if (ibuddy[2]>=0 && pTriMap[ibuddy[2]]!=-2) {
			ivtx = GetEdgeByBuddy(ibuddy[2],itri); itri = ibuddy[2];
			ibuddy[0] = m_pTopology[itri].ibuddy[inc_mod3[ivtx]];
			ibuddy[1] = m_pTopology[itri].ibuddy[dec_mod3[ivtx]];
			for(i=0;i<2;i++) if (ibuddy[i]>=0)
				m_pTopology[ibuddy[i]].ibuddy[GetEdgeByBuddy(ibuddy[i],itri)] = ibuddy[i^1];
			pTriMap[itri] = -2;
			if (!(pmu->nRemovedTri & 31)) ReallocateList(pmu->pRemovedTri, pmu->nRemovedTri,pmu->nRemovedTri+32);
			pmu->pRemovedTri[pmu->nRemovedTri++] = pmu->pRemovedVtx[itri];
		}
	}
}


int CTriMesh::FilterMesh(float minlen,float minangle,int bLogUpdates)
{
	if (m_flags & mesh_no_filter)
		return 0;
	int i,j,iedge,ibuddy,ibuddy1,*pTriMap,nChanges,iter=0;
	int idxmin,idxmax,imask,ivtx[3],*pVtxMap=m_pVtxMap;
	Vec3 vtx[4];
	float len0,len1,minlen2=sqr(minlen),minangle2=sqr(minangle);
	bop_meshupdate mu;
	memset(pTriMap = new int[m_nTris], -1, m_nTris*sizeof(int));
	if (!pVtxMap)	{
		pVtxMap = new int[m_nVertices];
		for(i=0;i<m_nVertices;i++) pVtxMap[i] = i;
	}
	mu.pRemovedVtx = m_pForeignIdx ? m_pForeignIdx : pTriMap;	// pRemovedVtx contains a valid pointer to foreign indices

	for(i=0,imask=1; i<m_nTris; i++) for(j=0;j<3;j++)
		if ((m_pVertices[idxmin = m_pIndices[i*3+j]]-m_pVertices[idxmax = m_pIndices[i*3+inc_mod3[j]]]).len2() < minlen2) {
			imask = idxmax-idxmin>>31; idxmin ^= idxmax&imask; idxmax ^= idxmin&imask; idxmin ^= idxmax&imask;
			pVtxMap[idxmax] = idxmin | 1<<30;
		}

	if (imask!=1) {
		for(i=m_nVertices-1;i>=0;i--) {
			for(j=i,imask=0; pVtxMap[j]!=j; imask|=pVtxMap[j],j=pVtxMap[j]&~(1<<30));
			if (j!=i && imask & 1<<30) {
				pVtxMap[i] = j;	m_pVertices[i] = m_pVertices[j];
				if (!(mu.nWeldedVtx & 31))
					ReallocateList(mu.pWeldedVtx, mu.nWeldedVtx,mu.nWeldedVtx+32);
				mu.pWeldedVtx[mu.nWeldedVtx++].set(j,i);
			}
		}
		for(i=0;i<m_nTris;i++)	{
			for(j=0;j<3;j++) ivtx[j] = pVtxMap[m_pIndices[i*3+j]];
			if (iszero(ivtx[0]-ivtx[1]) | iszero(ivtx[0]-ivtx[2]) | iszero(ivtx[1]-ivtx[2])) {
				pTriMap[i] = -2;
				if (!(mu.nRemovedTri & 31))
					ReallocateList(mu.pRemovedTri, mu.nRemovedTri,mu.nRemovedTri+32);
				mu.pRemovedTri[mu.nRemovedTri++] = mu.pRemovedVtx[i];
			}	else if (m_pIndices[i*3]-ivtx[0] | m_pIndices[i*3+1]-ivtx[1] | m_pIndices[i*3+2]-ivtx[2]) {
				for(j=0;j<3;j++) m_pIndices[i*3+j] = ivtx[j];
				RecalcTriNormal(i);
			}
		}

		CompactTriangleList(pTriMap, true);
		mu.pRemovedVtx = m_pForeignIdx ? m_pForeignIdx : pTriMap;
		memset(pTriMap, -1, m_nTris*sizeof(int));
		CalculateTopology(m_pIndices);
	}

	//do {
		for(i=nChanges=0; i<m_nTris; i++) if (pTriMap[i]!=-2) {
			for(j=0;j<3 && (m_pTopology[i].ibuddy[j]==-1 || m_pTopology[i].ibuddy[j]!=m_pTopology[i].ibuddy[dec_mod3[j]]); j++)
				vtx[j] = m_pVertices[m_pIndices[i*3+j]];
			if (j<3) { // remove 2 triangles if they are 'glued' together, forming a 0-thickness fin
				ibuddy = m_pTopology[i].ibuddy[j];
				for(iedge=0; iedge<2 && m_pTopology[ibuddy].ibuddy[iedge]==i; iedge++);

				pTriMap[i] = pTriMap[ibuddy] = -2;
				if (!(mu.nRemovedTri & 31)) ReallocateList(mu.pRemovedTri, mu.nRemovedTri,mu.nRemovedTri+32);
				mu.pRemovedTri[mu.nRemovedTri++] = mu.pRemovedVtx[i];
				if (!(mu.nRemovedTri & 31)) ReallocateList(mu.pRemovedTri, mu.nRemovedTri,mu.nRemovedTri+32);
				mu.pRemovedTri[mu.nRemovedTri++] = mu.pRemovedVtx[ibuddy];

				if ((ibuddy1 = m_pTopology[i].ibuddy[inc_mod3[j]])>=0)
					m_pTopology[ibuddy1].ibuddy[GetEdgeByBuddy(ibuddy1,i)] = m_pTopology[ibuddy].ibuddy[iedge];
				if ((ibuddy1 = m_pTopology[ibuddy].ibuddy[iedge])>=0)
					m_pTopology[ibuddy1].ibuddy[GetEdgeByBuddy(ibuddy1,ibuddy)] = m_pTopology[i].ibuddy[inc_mod3[j]];
				nChanges++; continue;
			}
			/*if (max(max((vtx[0]-vtx[1]).len2(),(vtx[0]-vtx[2]).len2()),(vtx[1]-vtx[2]).len2()) < minlen2) {
				// remove the entire triangle, collapse its buddies into lines
				iedge = GetEdgeByBuddy(m_pTopology[i].ibuddy[2],i);
				CollapseTriangleToLine(m_pTopology[i].ibuddy[0],GetEdgeByBuddy(m_pTopology[i].ibuddy[0],i), pTriMap, &mu);
				CollapseTriangleToLine(m_pTopology[i].ibuddy[2],iedge, pTriMap, &mu);
				nChanges++;
			}	else*/ {
				for(j=0; j<3 && (vtx[inc_mod3[j]]-vtx[j] ^ vtx[dec_mod3[j]]-vtx[j]).len2() >
						(len1=(vtx[inc_mod3[j]]-vtx[j]).len2())*(len0=(vtx[dec_mod3[j]]-vtx[j]).len2())*minangle2; j++);
				if (j<3) {
					/*if (len0 < max(minlen,len1*0.05f))
						j = inc_mod3[j];
					else if (len1 < max(minlen,len0*0.05f))
						j = dec_mod3[j];
					else*/ if (!inrange(len0,len1*0.95f,len1*1.05f)) {
						// triangle is a T-junction, split the 'base' triangle in two
						j = len0<len1 ? dec_mod3[j] : inc_mod3[j]; // j - the junction vertex
						if ((ibuddy = m_pTopology[i].ibuddy[inc_mod3[j]])>=0) {
							iedge = GetEdgeByBuddy(ibuddy, i);
							vtx[3] = m_pVertices[m_pIndices[ibuddy*3+dec_mod3[iedge]]];
							if ((vtx[j]-vtx[3]^vtx[dec_mod3[j]]-vtx[3]).len2() > // avoid looped switching
									(vtx[j]-vtx[3]).len2()*(vtx[dec_mod3[j]]-vtx[3]).len2()*minangle2 &&
									(vtx[j]-vtx[3]^vtx[inc_mod3[j]]-vtx[3]).len2() >
									(vtx[j]-vtx[3]).len2()*(vtx[inc_mod3[j]]-vtx[3]).len2()*minangle2	&&
									(m_pTopology[ibuddy].ibuddy[inc_mod3[iedge]] | m_pTopology[i].ibuddy[dec_mod3[j]])>=0)
							{
								m_pIndices[i*3+dec_mod3[j]] = m_pIndices[ibuddy*3+dec_mod3[iedge]];
								m_pIndices[ibuddy*3+inc_mod3[iedge]] = m_pIndices[i*3+j];
								if (!(mu.nTJFixes & 31)) ReallocateList(mu.pTJFixes, mu.nTJFixes,mu.nTJFixes+32);
								mu.pTJFixes[mu.nTJFixes++].set(mu.pRemovedVtx[i],inc_mod3[j], mu.pRemovedVtx[ibuddy],iedge, m_pIndices[i*3+j]);

								ibuddy1 = m_pTopology[ibuddy].ibuddy[inc_mod3[iedge]];
								m_pTopology[ibuddy1].ibuddy[GetEdgeByBuddy(ibuddy1,ibuddy)] = i;
								ibuddy1 = m_pTopology[i].ibuddy[dec_mod3[j]];
								m_pTopology[ibuddy1].ibuddy[GetEdgeByBuddy(ibuddy1,i)] = ibuddy;
								m_pTopology[i].ibuddy[inc_mod3[j]] = m_pTopology[ibuddy].ibuddy[inc_mod3[iedge]];
								m_pTopology[ibuddy].ibuddy[iedge] = m_pTopology[i].ibuddy[dec_mod3[j]];
								m_pTopology[i].ibuddy[dec_mod3[j]] = ibuddy;
								m_pTopology[ibuddy].ibuddy[inc_mod3[iedge]] = i;
								RecalcTriNormal(i); RecalcTriNormal(ibuddy);
								nChanges++;
							}
						} //continue;
					}
					//CollapseTriangleToLine(i,inc_mod3[j], pTriMap, &mu); nChanges++;
				}
			}
		}
	//} while(nChanges && ++iter<5);

	mu.pRemovedVtx = 0;
	if (mu.nWeldedVtx+mu.nRemovedTri+mu.nTJFixes && bLogUpdates) {
		bop_meshupdate *pbop,*pUpdate=new bop_meshupdate(mu);
		pUpdate->pMesh[0] = pUpdate->pMesh[1] = this;
		pUpdate->nextRef=pUpdate->prevRef = pUpdate;
		AddRef(); AddRef();
		if (m_pMeshUpdate) {
			for(pbop=m_pMeshUpdate; pbop->next; pbop=pbop->next);
			pbop->next = pUpdate;
		} else
			m_pMeshUpdate = pUpdate;
	}
	mu.Reset();

	if (nChanges || iter>0)	{
		CompactTriangleList(pTriMap, true);
		CalculateTopology(m_pIndices);
		MARK_UNUSED m_V,m_center;
	}
	if (m_pVtxMap!=pVtxMap)
		delete[] pVtxMap;
	delete[] pTriMap;

#ifdef _DEBUG
	//CalculateTopology(m_pIndices);
#endif

	return iter;
}


template<class dtype> void ReallocateListCompact(dtype *&plist, int sznew)
{
	dtype *newlist = new dtype[max(1,sznew)];
	memcpy(newlist,plist,sznew*sizeof(dtype));
	delete[] plist;
	plist = newlist;
}

void CTriMesh::CompactTriangleList(int *pTriMap, bool bNoRealloc)
{
	int i,j,itri,idx,imask;

	for(i=0,itri=m_nTris-1; true; i++,itri--) {
		for(; i<itri && pTriMap[i]!=-2; i++);
		for(;	i<itri && pTriMap[itri]==-2; itri--) pTriMap[itri]=-1;
		if (i>=itri)
			break;
		for(j=0;j<3;j++) {
			m_pIndices[i*3+j] = m_pIndices[itri*3+j];
			m_pTopology[i].ibuddy[j] = m_pTopology[itri].ibuddy[j];
		}
		if (m_pIds) m_pIds[i] = m_pIds[itri];
		if (m_pForeignIdx) m_pForeignIdx[i] = m_pForeignIdx[itri];
		m_pNormals[i] = m_pNormals[itri];
		pTriMap[itri] = i; pTriMap[i] = -1;
	}
	itri += pTriMap[itri]&1;
	for(i=0;i<itri;i++) for(j=0;j<3;j++) {
		idx = pTriMap[max((index_t)0,m_pTopology[i].ibuddy[j])]; imask = (idx|m_pTopology[i].ibuddy[j])>>31;
		m_pTopology[i].ibuddy[j] = m_pTopology[i].ibuddy[j]&imask | idx&~imask;
	}

	if (!bNoRealloc)
	{
		ReallocateList(m_pIndices, itri*3,itri*3);
		ReallocateList(m_pNormals, itri,itri);
		ReallocateList(m_pTopology,itri,itri);
		if (m_pIds) ReallocateList(m_pIds, itri,itri);
		if (m_pForeignIdx) ReallocateList(m_pForeignIdx, itri,itri);
	}
	m_nTris = itri;
}


float CTriMesh::GetExtent(EGeomForm eForm) const
{
	if (eForm == GeomForm_Vertices)
		return (float)m_nVertices;

	CGeomExtent& ext = m_Extents.Make(eForm);
	if (!ext) {
		int nParts = TriMeshPartCount(eForm, m_nTris*3);
		ext.ReserveParts(nParts);
		for (int i=0; i<nParts ; i++) {
			int nIndices[3];
			Vec3 aVec[3];
			for (int v = TriIndices(nIndices, i, eForm)-1; v >= 0; v--)
				aVec[v] = m_pVertices[ m_pIndices[nIndices[v]] ];
			ext.AddPart( max(TriExtent(eForm, aVec, m_center), 0.f) );
		}
	}
	return ext.TotalExtent();
}

void CTriMesh::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{
	int nTri;

	if (eForm == GeomForm_Vertices)
	{
		for (auto& ran : points) {
			nTri = seed.GetRandom(0, m_nTris - 1);
			ran.vPos = m_pVertices[m_pIndices[nTri * 3 + seed.GetRandom(0, 2)]];
			ran.vNorm = m_pNormals[nTri];
		}
	}
	else
	{
		CGeomExtent const& extent = m_Extents[eForm];
		if (!extent.NumParts())
			return points.fill(ZERO);
		for (auto& ran : points) {
			int nPart = extent.RandomPart(seed);

			int aIndices[3];
			PosNorm aRan[3];
			for (int v = TriIndices(aIndices, nPart, eForm) - 1; v >= 0; v--)
				aRan[v].vPos = m_pVertices[m_pIndices[aIndices[v]]];
			TriRandomPos(ran, seed, eForm, aRan, m_center, false);
			nTri = eForm == GeomForm_Edges ? nPart / 3 : nPart;
			ran.vNorm = m_pNormals[nTri];
		}
	}
}

float CTriMesh::GetVolume()
{
	if (is_unused(m_V)) {
		m_V = 0;
		for(int itri=0; itri<m_nTris; itri++) {
			Vec3 vtx[3]; for(int i=0;i<3;i++) vtx[i] = m_pVertices[m_pIndices[itri*3+i]];
			m_V += (Vec2(vtx[1])-Vec2(vtx[0]) ^ Vec2(vtx[2])-Vec2(vtx[0]))*(vtx[0].z+vtx[1].z+vtx[2].z);
		}
		m_V *= (1.0f/6);
	}
	return m_V;
}

Vec3 CTriMesh::GetCenter()
{
	if (is_unused(m_center)) {
		int i,j;
		m_center.zero();
		for(i=0;i<m_nTris;i++) for(j=0;j<3;j++)
			m_center += m_pVertices[m_pIndices[i*3+j]];
		if (m_nTris>0)
			m_center /= m_nTris*3;
	}
	return m_center;
}


int CTriMesh::IsConvex(float tolerance)
{
	int idx,i,j;
	for(idx=0; idx<CRY_ARRAY_COUNT(m_bConvex)-1 && m_ConvexityTolerance[idx]>=0 && m_ConvexityTolerance[idx]!=tolerance; idx++);
	if (m_ConvexityTolerance[idx]==tolerance)	{
		m_ConvexityTolerance[idx]=m_ConvexityTolerance[0]; m_ConvexityTolerance[0]=tolerance;
		i=m_bConvex[idx]; m_bConvex[idx]=m_bConvex[0]; m_bConvex[0]=i;
		return i;
	}

	if (m_bMultipart)
		return m_bConvex[idx]=0;

	m_ConvexityTolerance[idx] = tolerance;
	tolerance = sqr(tolerance);
	for(i=0;i<m_nTris;i++) for(j=0;j<3;j++) if (m_pTopology[i].ibuddy[j]>=0) {
		Vec3 cross = m_pNormals[i]^m_pNormals[m_pTopology[i].ibuddy[j]];
		if (cross.len2()>tolerance && cross*(m_pVertices[m_pIndices[i*3+inc_mod3[j]]]-m_pVertices[m_pIndices[i*3+j]])<0)
			return m_bConvex[idx]=0;
	} else
		return m_bConvex[idx]=0;
	return m_bConvex[idx]=1;
}


int CTriMesh::CalcPhysicalProperties(phys_geometry *pgeom)
{
	pgeom->pGeom = this;

	if (!(m_flags & mesh_always_static)) {
		ReadLock lock(m_lockUpdate);
		Vec3r center,Ibody;
		Matrix33r I; real Irm[9],Rframe2body[9];
		pgeom->V = ComputeMassProperties(m_pVertices,m_pIndices,m_nTris, center,I);
		if (is_unused(m_V))
			m_V = pgeom->V;

		matrix eigenBasis(3,3,0,Rframe2body);
		SetMtxStrided<real,3,1>(I,Irm);
		matrix(3,3,mtx_symmetric,Irm).jacobi_transformation(eigenBasis, Ibody);
		pgeom->origin = center;
		pgeom->Ibody = Ibody;
		pgeom->q = quaternionf((Matrix33)GetMtxStrided<real,3,1>(Rframe2body).T());
	} else {
		pgeom->V = 0;
		pgeom->origin.zero();
		pgeom->Ibody.zero();
		pgeom->q.SetIdentity();
	}
	if (m_flags & mesh_should_die)
		pgeom->V = -1e10f;

	return 1;
}


int CTriMesh::PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl, bool bKeepPrevContacts)
{
	const int iCaller = pGTest->iCaller;
	pGTest->pGeometry = this;
	pGTest->pBVtree = m_pTree;
	CBVTree *const __restrict pTree = m_pTree;
	pTree->PrepareForIntersectionTest(pGTest, pCollider,pGTestColl);

	pGTest->typeprim = indexed_triangle::type;
	int nNodeTris = pTree->MaxPrimsInNode();

	IF (nNodeTris<=(int)(CRY_ARRAY_COUNT(g_IdxTriBuf))-g_IdxTriBufPos, 1) {
		pGTest->primbuf = g_IdxTriBuf+g_IdxTriBufPos; g_IdxTriBufPos+=nNodeTris;
	}
	else
		pGTest->primbuf = new indexed_triangle[nNodeTris];
	pGTest->szprimbuf = nNodeTris;

	IF (m_nMaxVertexValency<=(int)(CRY_ARRAY_COUNT(g_IdxTriBuf))-g_IdxTriBufPos, 1) {
		pGTest->primbuf1 = g_IdxTriBuf+g_IdxTriBufPos; g_IdxTriBufPos+=m_nMaxVertexValency;
	}
	else
		pGTest->primbuf1 = new indexed_triangle[m_nMaxVertexValency];
	pGTest->szprimbuf1 = m_nMaxVertexValency;

	IF (m_nMaxVertexValency<=(int)(CRY_ARRAY_COUNT(g_iFeatureBuf))-g_iFeatureBufPos, 1) {
		pGTest->iFeature_buf = g_iFeatureBuf+g_iFeatureBufPos; g_iFeatureBufPos+=m_nMaxVertexValency;
	}
	else
		pGTest->iFeature_buf = new int[m_nMaxVertexValency];

	int szbuf = max(nNodeTris,m_nMaxVertexValency);
	IF (szbuf<=(int)(CRY_ARRAY_COUNT(g_IdBuf))-g_IdBufPos, 1) {
		pGTest->idbuf = g_IdBuf+g_IdBufPos; g_IdBufPos+=szbuf;
	}
	else
		pGTest->idbuf = new char[szbuf];

	pGTest->szprim = sizeof(indexed_triangle);
	IF (m_nMaxVertexValency<=(int)(CRY_ARRAY_COUNT(g_SurfaceDescBuf))-g_SurfaceDescBufPos, 1) {
		pGTest->surfaces = g_SurfaceDescBuf+g_SurfaceDescBufPos; g_SurfaceDescBufPos+=m_nMaxVertexValency;
	}
	else
		pGTest->surfaces = new surface_desc[m_nMaxVertexValency];

	IF (m_nMaxVertexValency<=(int)(CRY_ARRAY_COUNT(g_EdgeDescBuf))-g_EdgeDescBufPos, 1) {
		pGTest->edges = g_EdgeDescBuf+g_EdgeDescBufPos; g_EdgeDescBufPos+=m_nMaxVertexValency;
	}
	else
		pGTest->edges = new edge_desc[m_nMaxVertexValency];

	pGTest->minAreaEdge = 0;
	if (!bKeepPrevContacts)
		g_BrdPtBufPos = g_BrdPtBufStart = 0;
	g_BrdPtBufStart = g_BrdPtBufPos;

	return 1;
}

void CTriMesh::CleanupAfterIntersectionTest(geometry_under_test *pGTest)
{
	m_pTree->CleanupAfterIntersectionTest(pGTest);
	const int iCaller = pGTest->iCaller;
	if ((unsigned int)((indexed_triangle*)pGTest->primbuf-g_IdxTriBuf) > (unsigned int)(CRY_ARRAY_COUNT(g_IdxTriBuf)))
		delete[] pGTest->primbuf;
	if ((unsigned int)((indexed_triangle*)pGTest->primbuf1-g_IdxTriBuf) > (unsigned int)(CRY_ARRAY_COUNT(g_IdxTriBuf)))
		delete[] pGTest->primbuf1;
	if ((unsigned int)(pGTest->iFeature_buf-g_iFeatureBuf) > (unsigned int)(CRY_ARRAY_COUNT(g_iFeatureBuf)))
		delete[] pGTest->iFeature_buf;
	if ((unsigned int)(pGTest->idbuf-g_IdBuf) > (unsigned int)(CRY_ARRAY_COUNT(g_IdBuf)))
		delete[] pGTest->idbuf;
	if ((unsigned int)(pGTest->surfaces-g_SurfaceDescBuf) > (unsigned int)(CRY_ARRAY_COUNT(g_SurfaceDescBuf)))
		delete[] pGTest->surfaces;
	if ((unsigned int)(pGTest->edges-g_EdgeDescBuf) > (unsigned int)(CRY_ARRAY_COUNT(g_EdgeDescBuf)))
		delete[] pGTest->edges;
}


int CTriMesh::GetFeature(int iPrim,int iFeature, Vec3 *pt)
{
	int npt=0;
	switch (iFeature & 0x60) {
		case 0x40: pt[2]=m_pVertices[m_pIndices[iPrim*3+dec_mod3[(iFeature&0x1F)]]]; npt++;
		case 0x20: pt[1]=m_pVertices[m_pIndices[iPrim*3+inc_mod3[(iFeature&0x1F)]]]; npt++;
		case 0   : pt[0]=m_pVertices[m_pIndices[iPrim*3+(iFeature&0x1F)]]; npt++;
	}
	return npt;
}

void CTriMesh::PrepareTriangle(int itri,triangle *ptri, const geometry_under_test *pGTest)
{
	int idx = itri*3;
	ptri->pt[0] = pGTest->R*m_pVertices[m_pIndices[idx  ]]*pGTest->scale + pGTest->offset;
	ptri->pt[1] = pGTest->R*m_pVertices[m_pIndices[idx+1]]*pGTest->scale + pGTest->offset;
	ptri->pt[2] = pGTest->R*m_pVertices[m_pIndices[idx+2]]*pGTest->scale + pGTest->offset;
	ptri->n = pGTest->R*m_pNormals[itri];
}

int CTriMesh::PreparePrimitive(geom_world_data *pgwd, primitive *&pprim, int iCaller)
{
	indexed_triangle *ptri;
	pprim = ptri=g_IdxTriBuf;
	ptri->pt[0] = pgwd->R*m_pVertices[m_pIndices[0]]*pgwd->scale + pgwd->offset;
	ptri->pt[1] = pgwd->R*m_pVertices[m_pIndices[1]]*pgwd->scale + pgwd->offset;
	ptri->pt[2] = pgwd->R*m_pVertices[m_pIndices[2]]*pgwd->scale + pgwd->offset;
	ptri->n = pgwd->R*m_pNormals[0];
	return indexed_triangle::type;
}


const int MAX_LOOP = 10;


int CTriMesh::TraceTriangleInters(int iop, primitive *pprims[], int idx_buddy,int type_buddy, prim_inters *pinters,
																	geometry_under_test *pGTest, border_trace *pborder)
{
	indexed_triangle *ptri = (indexed_triangle*)pprims[iop];
	int itri,itri0=ptri->idx,itri_end,itri_prev,itri_cur,iedge,itypes[2],iter;
	Vec3 dir0,dir1;
	itypes[iop] = triangle::type;
	itypes[iop^1] = type_buddy;

	do {
		itri = m_pTopology[itri0].ibuddy[pinters->iFeature[1][iop] & 0x1F];
		if ((pinters->pt[1]-pborder->pt_end).len2()<pborder->end_dist2 &&
				((itri|iop<<31)==pborder->itri_end && GetEdgeByBuddy(itri,itri0)==pborder->iedge_end || (pborder->npt&~-pborder->bExactBorder)>3)
				|| itri<0)
			return 2;
		if (pborder->npt>=pborder->szbuf)
			return 0;
		PrepareTriangle(itri,ptri, pGTest);
		//m_pTree->MarkUsedTriangle(itri, pGTest);

		if (!g_Intersector.Check(itypes[0],itypes[1], pprims[0], pprims[1], pinters)) {
			// if we start from a point close to vertex, check a triangle fan around the vertex rather than just edge owner
			// return 0 if we fail to close the intersection region
			iedge = GetEdgeByBuddy(itri, itri0);
			itri_end = itri;
			if ((pinters->pt[1]-ptri->pt[iedge]).len2() < sqr(m_minVtxDist)) { // start edge vertex
				itri_end = itri0; itri_prev = itri;
				itri = m_pTopology[itri].ibuddy[dec_mod3[iedge]];
			} else if ((pinters->pt[1]-ptri->pt[inc_mod3[iedge]]).len2() < sqr(m_minVtxDist)) { // end edge
				itri_end = itri; itri_prev = itri0;
				itri = m_pTopology[itri0].ibuddy[dec_mod3[pinters->iFeature[1][iop] & 0x1F]];
			}	else
				return 0;
			if (itri<0)
				return 0;

			iter=0; do {
				PrepareTriangle(itri,ptri, pGTest);
				//m_pTree->MarkUsedTriangle(itri, pGTest);
				if (g_Intersector.Check(itypes[0],itypes[1], pprims[0],pprims[1], pinters))
					goto have_inters;
				itri_cur = itri;
				itri = m_pTopology[itri].ibuddy[dec_mod3[GetEdgeByBuddy(itri, itri_prev)]];
				itri_prev = itri_cur;
			} while(itri>=0 && itri!=itri_end && ++iter<30);

			return 0;
		}
		have_inters:
		// store information about the last intersection segment
		pborder->itri[pborder->npt][iop] = itri | pinters->iFeature[1][iop]<<IFEAT_LOG2;
		pborder->itri[pborder->npt][iop^1] = idx_buddy | pinters->iFeature[1][iop^1]<<IFEAT_LOG2;
		pborder->pt[pborder->npt] = pinters->pt[1];
		pborder->n_sum[iop] += ptri->n;
		if (fabs_tpl(pborder->n_best.z)<fabs_tpl(ptri->n.z))
			pborder->n_best = ptri->n*(1-iop*2);
		pborder->ntris[iop]++;
		// do checks against looping in degenerate cases
		if (pborder->npt>30+pborder->bExactBorder*200 && (pborder->pt[pborder->npt]-pborder->pt[pborder->iMark]).len2()<pborder->end_dist2*0.01f)
			return 0;
		--pborder->nLoop;
		pborder->iMark += pborder->npt-pborder->iMark & pborder->nLoop>>31;
		pborder->nLoop += MAX_LOOP+1 & pborder->nLoop>>31;
		pborder->npt++;

		ptri->idx = itri0 = itri;
	} while(!(pinters->iFeature[1][iop^1] & 0x80)); // iterate while triangle from another mesh is not ended

	return 1;
}


int CTriMesh::GetUnprojectionCandidates(int iop,const contact *pcontact, primitive *&pprim,int *&piFeature, geometry_under_test *pGTest)
{
	int i,itri,iFeature = pcontact->iFeature[iop];
	indexed_triangle *ptri = (indexed_triangle*)pGTest->primbuf1;
	intptr_t idmask = ~iszero_mask(m_pIds);
	char idnull=(char)-1, *ptrnull=&idnull, *pIds = (char*)((intptr_t)m_pIds&idmask | (intptr_t)ptrnull&~idmask);
	itri = ((indexed_triangle*)pprim)->idx;
	PrepareTriangle(itri,ptri,pGTest); ptri->idx = itri;
	pprim = ptri;
	piFeature = pGTest->iFeature_buf;
	pGTest->bTransformUpdated = 0;

	if ((iFeature & 0x60)!=0) { // if feature is not vertex, but is very close to one, change it to vertex
		for(i=0;i<3 && (ptri->pt[i]-pcontact->pt).len2()>sqr(m_minVtxDist);i++);
		if (i<3)
			iFeature = 0x80 | i;
	}

	switch (iFeature & 0x60) {
		case 0x00: { // vertex - return triangle fan around it (except the previous contacting triangle)
				int itri_prev,itri0,iedge,ntris=0;
				itri_prev = itri0 = itri;

				pGTest->surfaces[0].n = ptri->n;
				pGTest->surfaces[0].idx = itri;
				pGTest->surfaces[0].iFeature = 0x40;

				iedge = iFeature & 0x1F;
				pGTest->edges[0].dir = ptri->pt[inc_mod3[iedge]]-ptri->pt[iedge];
				pGTest->edges[0].n[0] = ptri->n ^ pGTest->edges[0].dir;
				pGTest->edges[0].idx = itri;
				pGTest->edges[0].iFeature = 0x20 | iedge;

				itri = m_pTopology[itri].ibuddy[dec_mod3[iFeature & 0x1F]];
				if (itri>=0) do {
					PrepareTriangle(itri,ptri+ntris,pGTest);
					ptri[ntris].idx = itri;
					pGTest->idbuf[ntris] = pIds[itri&idmask];

					iedge = GetEdgeByBuddy(itri,itri_prev);
					piFeature[ntris] = 0x80 | iedge;
					pGTest->edges[ntris+1].dir = ptri[ntris].pt[inc_mod3[iedge]]-ptri[ntris].pt[iedge];
					pGTest->edges[ntris+1].n[0] = pGTest->edges[ntris+1].dir ^ pGTest->surfaces[ntris].n;
					pGTest->edges[ntris+1].n[1] = ptri[ntris].n ^ pGTest->edges[ntris+1].dir;
					pGTest->edges[ntris+1].idx = itri;
					pGTest->edges[ntris+1].iFeature = 0x20 | iedge;

					pGTest->surfaces[ntris+1].n = ptri[ntris].n;
					pGTest->surfaces[ntris+1].idx = itri;
					pGTest->surfaces[ntris+1].iFeature = 0x40;

					itri_prev = itri;
					itri = m_pTopology[itri].ibuddy[dec_mod3[iedge]];
					ntris++;
				} while(itri>=0 && itri!=itri0 && ntris<pGTest->szprimbuf1-1);

				pGTest->edges[0].n[1] = pGTest->edges[0].dir ^ ptri[max(0,ntris-1)].n;
				pGTest->nSurfaces = ntris+1;
				pGTest->nEdges = ntris+1;
				pprim = ptri;
				return ntris;
			}

		case 0x20: // edge - switch to the corresponding buddy
			pGTest->edges[0].dir = ptri->pt[inc_mod3[iFeature & 0x1F]] - ptri->pt[iFeature & 0x1F];
			pGTest->edges[0].n[0] = ptri->n^pGTest->edges[0].dir;
			pGTest->edges[0].idx = itri;
			pGTest->edges[0].iFeature = iFeature;
			pGTest->nEdges = 1;

			pGTest->surfaces[0].n = ptri->n;
			pGTest->surfaces[0].idx = itri;
			pGTest->surfaces[0].iFeature = 0x40;

			i=itri; itri=m_pTopology[itri].ibuddy[iFeature & 0x1F];
			pGTest->nSurfaces = 1;
			if (itri<0) {
				pGTest->edges[0].n[1] = pGTest->edges[0].n[0];
				return 0;
			}

			piFeature[0] = GetEdgeByBuddy(itri,i) | 0xA0;
			PrepareTriangle(itri,ptri,pGTest);
			pGTest->idbuf[0] = pIds[itri&idmask];
			ptri->idx = itri;

			pGTest->edges[0].n[1] = pGTest->edges[0].dir^ptri->n;
			pGTest->surfaces[1].n = ptri->n;
			pGTest->surfaces[1].idx = itri;
			pGTest->surfaces[1].iFeature = 0x40;
			pGTest->nSurfaces = 2;
			return 1;

		case 0x40: // face - keep the previous contacting triangle
			pGTest->surfaces[0].n = ptri->n;
			pGTest->surfaces[0].idx = itri;
			pGTest->surfaces[0].iFeature = 0x40;
			pGTest->nSurfaces = 1;
			pGTest->nEdges = 0;
			piFeature[0] = 0x40;
			return 1;
	}
	return 0;
}


#undef g_Overlapper
COverlapChecker g_Overlapper;

int CTriMesh::GetPrimitiveList(int iStart,int nPrims, int typeCollider,primitive *pCollider,int bColliderLocal,
															 geometry_under_test *pGTest,geometry_under_test *pGTestOp, primitive *pRes,char *pResId)
{
	intptr_t mask;
	int i,j,iEnd=min(m_nTris,iStart+nPrims),bContact,bPivotCulling;
	char *pIds,noid=(char)-2,*pnoid=&noid;
	mask = ~iszero_mask(m_pIds);
	intptr_t inv_mask = ~mask;
	pIds = (char*)((intptr_t)m_pIds&mask | (intptr_t)pnoid&inv_mask);
	bPivotCulling = isneg(pGTestOp->ptOutsidePivot.x-1E10f);

	if (bColliderLocal==1) { // collider is in local coordinate frame
	for(i=iStart,j=0; i<iEnd; i++,j+=bContact) {
		indexed_triangle *ptri = (indexed_triangle*)pRes+j;
		ptri->pt[0] = m_pVertices[m_pIndices[i*3+0]]; ptri->pt[1] = m_pVertices[m_pIndices[i*3+1]];
		ptri->pt[2] = m_pVertices[m_pIndices[i*3+2]]; ptri->n = m_pNormals[i];
		pResId[j] = pIds[i&mask]; // material -1 signals that the triangle should be skipped
		bContact = g_Overlapper.Check(typeCollider,triangle::type, pCollider,ptri) & (iszero(pResId[j]+1)^1);
		PrepareTriangle(i,ptri, pGTest);
		if (bPivotCulling)
			bContact &= isneg((ptri->pt[0]-pGTestOp->ptOutsidePivot)*ptri->n);
		ptri->idx = i;
	} } else if (bColliderLocal==0) { // collider is in his own coordinate frame
	for(i=iStart,j=0; i<iEnd; i++,j+=bContact) {
		indexed_triangle *ptri = (indexed_triangle*)pRes+j;
		ptri->pt[0] = pGTest->R_rel*m_pVertices[m_pIndices[i*3+0]]*pGTest->scale_rel + pGTest->offset_rel;
		ptri->pt[1] = pGTest->R_rel*m_pVertices[m_pIndices[i*3+1]]*pGTest->scale_rel + pGTest->offset_rel;
		ptri->pt[2] = pGTest->R_rel*m_pVertices[m_pIndices[i*3+2]]*pGTest->scale_rel + pGTest->offset_rel;
		ptri->n = pGTest->R_rel*m_pNormals[i];
		pResId[j] = pIds[i&mask];
		bContact = g_Overlapper.Check(typeCollider,triangle::type, pCollider,ptri) & (iszero(pResId[j]+1)^1);
		PrepareTriangle(i,ptri, pGTest);
		if (bPivotCulling)
			bContact &= isneg((ptri->pt[0]-pGTestOp->ptOutsidePivot)*ptri->n);
		ptri->idx = i;
	} } else { // no checks for collider, get all tris unconditionally (used in sweep check)
	for(i=iStart,j=0; i<iEnd; i++,j++) {
		indexed_triangle *ptri = (indexed_triangle*)pRes+j;
		PrepareTriangle(i,ptri, pGTest);
		ptri->idx = i;
	} }
	pGTest->bTransformUpdated = 0;
 	return j;
}

void update_unprojection(real t, unprojection_mode *pmode, geometry_under_test *pGTest)
{
	if (pmode->imode==0)
		pGTest->offset = pmode->offset0 + pmode->dir*t;
	else {
		Matrix33 Rw;
		real cosa=cos_tpl(t), sina=sin_tpl(t);
		Rw.SetRotationAA(cosa,sina, pmode->dir);
		(pGTest->R = Rw) *= pmode->R0;
		pGTest->offset = Rw*(pmode->offset0-pmode->center) + pmode->center;
	}
	pGTest->bTransformUpdated = 1;
}


int CTriMesh::RegisterIntersection(primitive *pprim1,primitive *pprim2, geometry_under_test *pGTest1,geometry_under_test *pGTest2,
																	 prim_inters *pinters)
{
	//CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	geometry_under_test *pGTest[2];
  pGTest[0] = pGTest1;   pGTest[1] = pGTest2;
	indexed_triangle tri = *(indexed_triangle*)pprim1, tri1;
	primitive *pprims[2];
  char *ptr[2];
  pprims[0] = &tri; pprims[1] = pprim2;
	int iFeature_dummy = -1;
  int *piFeature[2];
  piFeature[0] = piFeature[1] = &iFeature_dummy;
	int idx_prim[2] = { ((indexed_triangle*)pprim1)->idx, -1 };
	int bNoUnprojection=0,bSurfaceSurfaceContact=0,bSurfaceEdgeContact=0,bUseLSNormal=0;
	int i,j,res=0,ipt,ibest,jbest,iop,nprims1,nprims2,nSmallSteps;
	int indexed_triangle::*pidxoffs=0;
	const int iCaller = pGTest1->iCaller;
	float len,maxlen;
	real t,tmax;
	border_trace border;
	geom_contact *pres = pGTest[0]->contacts + *pGTest[0]->pnContacts;
	Vec3r n_avg;
	Vec3 unprojPlaneNorm;
	if (pGTest[1]->typeprim==indexed_triangle::type) {
		idx_prim[1] = ((indexed_triangle*)pprims[1])->idx;
		pidxoffs = &indexed_triangle::idx;
		/*if (pGTest[0]->ptOutsidePivot.x<1E10) {
			Vec3 inters_dir = (pinters->pt[0]+pinters->pt[1])*0.5f-pGTest[0]->ptOutsidePivot;
			if (sqr_signed(inters_dir*((indexed_triangle*)pprims[1])->n) > inters_dir.len2()*sqr(0.4f))
				return 0;
		}*/
	}

	border.end_dist2 = sqr(m_minVtxDist*2);
	if (!pGTest1->pParams->bExactBorder) {
		for(i=g_BrdPtBufStart;i<g_BrdPtBufPos && (g_BrdPtBuf[i]-pinters->pt[1]).len2()>border.end_dist2;i++);
		if (i<g_BrdPtBufPos)
			return 0;
	} else {
		for(i=g_BrdPtBufStart; i<g_BrdPtBufPos &&
			((g_BrdiTriBuf[i][0]&IDXMASK)-idx_prim[0] | (g_BrdiTriBuf[i][1]&IDXMASK)-idx_prim[1]); i++);
		if (i<g_BrdPtBufPos)
			return 0;
	}
	border.bExactBorder = pGTest1->pParams->bExactBorder;

	pres->iPrim[0] = ((indexed_triangle*)pprims[0])->idx;
	pres->iPrim[1] = ((indexed_triangle*)pprims[1])->*pidxoffs;
	pres->bBorderConsecutive = true;

	float maxcos=1.0f,mincos2=0;
	if (pres->parea) {
		if (pGTest2->pParams->maxSurfaceGapAngle<0.2f) {
			maxcos = 1.0f-sqr(pGTest2->pParams->maxSurfaceGapAngle)*2;
			mincos2 = sqr(pGTest2->pParams->maxSurfaceGapAngle);
		} else {
			maxcos = cos_tpl(pGTest2->pParams->maxSurfaceGapAngle);
			mincos2 = sqr(cos_tpl(g_PI*0.5f-pGTest2->pParams->maxSurfaceGapAngle));
		}
	}
	pres->id[0] = pinters->id[0];
	pres->id[1] = pinters->id[1];
	pres->iNode[0] = pinters->iNode[0];
	pres->iNode[1] = pinters->iNode[1];

	border.itri = g_BrdiTriBuf + g_BrdPtBufPos;
	border.seglen = g_BrdSeglenBuf;
	border.pt = g_BrdPtBuf + g_BrdPtBufPos;
	border.szbuf = min(100+pGTest1->pParams->bExactBorder*2900, (int)(CRY_ARRAY_COUNT(g_BrdPtBuf)) - g_BrdPtBufPos-16);
	if (border.szbuf<0)
		return 0;
	pres->ptborder = border.pt;
	pres->idxborder = border.itri;
	border.n_sum[0].zero(); border.n_sum[1].zero();
	border.ntris[0] = border.ntris[1] = 0;
	border.n_best.zero();

	iop = pinters->iFeature[0][1]>>7; // index of mesh whose primitive has boundary on intersection start
	border.pt_end = pinters->pt[0];
	border.itri_end = ((indexed_triangle*)pprims[iop])->idx | iop<<31;
	border.iedge_end = pinters->iFeature[0][iop] & 0x1F;

	pres->center = border.pt[0] = pinters->pt[0];
	border.pt[1] = pinters->pt[1];
	border.itri[0][iop] = idx_prim[iop] | pinters->iFeature[0][iop]<<IFEAT_LOG2;
	border.itri[1][iop] = idx_prim[iop] | pinters->iFeature[1][iop]<<IFEAT_LOG2;
	border.itri[0][iop^1] = idx_prim[iop^1] | pinters->iFeature[0][iop^1]<<IFEAT_LOG2;
	border.itri[1][iop^1] = idx_prim[iop^1] | pinters->iFeature[1][iop^1]<<IFEAT_LOG2;
	border.npt = 2;
	border.iMark = 1;
	border.nLoop = MAX_LOOP;
	iop = pinters->iFeature[1][1]>>7; // index of mesh whose primitive has boundary on intersection end
	if (pGTest[iop^1]->typeprim==indexed_triangle::type) {
		border.n_sum[iop^1] = ((indexed_triangle*)pprims[iop^1])->n;
		border.n_best = border.n_sum[iop^1]*(iop*2-1);
		border.ntris[iop^1] = 1;
	}

	if ((pinters->iFeature[1][0] | pinters->iFeature[1][1]) & 0x80
			&& !pGTest[0]->pParams->bNoBorder) { // if at least one triangle ended
			// && (!pGTest1->bStopAtFirstTri || (pinters->pt[0]-pinters->pt[1]).len2()<sqr(m_minVtxDist*10))) {
		do {
			if (iop==1 && pprims[1]==pprim2) { // since we are about to change triangle being traced, use a local copy
				tri1 = *(indexed_triangle*)pprim2; pprims[1] = &tri1;
			}
			border.n_sum[iop] += ((indexed_triangle*)pprims[iop])->n; border.ntris[iop]++;
			if (fabs_tpl(border.n_best.z) < fabs_tpl(((indexed_triangle*)pprims[iop])->n.z))
				border.n_best = ((indexed_triangle*)pprims[iop])->n*(1-iop*2);
			res = ((CTriMesh*)pGTest[iop]->pGeometry)->TraceTriangleInters(iop, pprims, idx_prim[iop^1],pGTest[iop^1]->typeprim,
				pinters, pGTest[iop], &border);
			pGTest1->pParams->bBothConvex &= res>>1; // if failed to close a contour - don't use optiomizations for convex objects
			idx_prim[iop] = ((indexed_triangle*)pprims[iop])->idx; iop ^= 1;
		} while(res==1);

		if (res==2) for(i=1;i<border.npt;i++) {
			CBVTree *pBVtree0 = pGTest[0]->pBVtree;
			pBVtree0->MarkUsedTriangle(border.itri[i][0]&IDXMASK,pGTest[0]);
			CBVTree *pBVtree1 = pGTest[1]->pBVtree;
			pBVtree1->MarkUsedTriangle(border.itri[i][1]&IDXMASK,pGTest[1]);
		}
	} else {
		border.ntris[0]=1; border.n_sum[0]=tri.n;
	};
	pres->bClosed = res>>1;
	pres->nborderpt = border.npt;
	g_BrdPtBufPos += border.npt;
	border.n_sum[1].Flip();
	n_avg = (border.ntris[0]<border.ntris[1] && border.ntris[0] || !border.ntris[1] ?
		border.n_sum[0]:border.n_sum[1]).normalized();
	if (fabs_tpl(border.n_best.z) < 0.95f)
		border.n_best = n_avg;
	pres->dir = -n_avg;

	unprojection_mode unproj;
	contact contact_cur, contact_best;
	unproj.center.zero();

	Vec3 vrel_loc, vrel=pGTest[0]->v-pGTest[1]->v, pthit=border.pt[0], pthit1=border.pt[0];
	float vrel_loc2,vrel2=0,dist,maxdist=0,maxdist1=0;
	for(i=0;i<border.npt;i++) {
		vrel_loc = pGTest[0]->v-pGTest[1]->v + (pGTest[0]->w^border.pt[i]-pGTest[0]->centerOfMass) -
			(pGTest[1]->w^border.pt[i]-pGTest[1]->centerOfMass);
		if (vrel_loc*n_avg>0 && (vrel_loc2=vrel_loc.len2()) > vrel2) {
			vrel2=vrel_loc2; vrel=vrel_loc;
		}
		if ((dist=(border.pt[i]-pGTest[0]->centerOfRotation^pGTest[0]->pParams->axisOfRotation).len2())>maxdist) {
			maxdist = dist; pthit = border.pt[i];
		}
		if ((dist=(border.pt[i]-pGTest[0]->centerOfRotation).len2())>maxdist1) {
			maxdist1 = dist; pthit1 = border.pt[i];
		}
	}

	if (pGTest[0]->pParams->iUnprojectionMode<=0) {
		unproj.imode = 0;
		unproj.dir = -vrel;
		if (unproj.dir.len2() < sqr(pGTest[0]->pParams->vrel_min)) {
			unproj.R0 = pGTest[0]->R; unproj.offset0 = pGTest[0]->offset;
			bUseLSNormal=1; goto end_unproj;
		}
		if (unproj.dir.len2()==0 && pGTest2->pGeometry->GetType()==GEOM_RAY) {
			Vec3 dir = pGTest2->R*((CRayGeom*)pGTest2->pGeometry)->m_ray.dir;
			if (n_avg*dir>0)
				return 0;
			unproj.dir = ((n_avg^dir)^dir).normalized();
			unproj.vel = 1.0f;
		}	else
			unproj.dir /= max(1e-8f, unproj.vel=unproj.dir.len());
		tmax = unproj.tmax = pGTest[0]->pParams->time_interval*unproj.vel;
	} else {
		if (pGTest[0]->pParams->axisOfRotation.len2()==0) {
			maxdist = maxdist1; pthit = pthit1;
		}
		if (maxdist < sqr(pGTest[0]->pParams->minAxisDist)) {
			unproj.R0 = pGTest[0]->R; unproj.offset0 = pGTest[0]->offset;
			bUseLSNormal=1; goto end_unproj;
		}
		unproj.imode = 1;
		unproj.center = pGTest[0]->centerOfRotation;
		unproj.dir = pGTest[0]->pParams->axisOfRotation;
		if (unproj.dir.len2()>0)
			unproj.dir *= -sgnnz(n_avg*(unproj.dir^pthit-unproj.center));
		else {
			//unproj.dir = (pthit-unproj.center^vrel).normalized();
			//unproj.dir *= -sgnnz(n_avg*(unproj.dir^pthit-unproj.center));
			if (n_avg*(pthit-unproj.center)>0)
				return 0;
			unproj.dir = (n_avg^pthit-unproj.center).normalized();
			pGTest[0]->pParams->axisOfRotation = unproj.dir;
		}
		unproj.vel = unproj.dir.len();
		unproj.dir /= unproj.vel;
		tmax = pGTest[0]->pParams->time_interval*unproj.vel;
		unproj.tmax = sin_tpl(min(real(g_PI*0.5f),tmax));
	}
	unproj.minPtDist = min(m_minVtxDist,pGTest2->pGeometry->m_minVtxDist);
	unprojPlaneNorm = pGTest[0]->pParams->unprojectionPlaneNormal;

	do {
		unproj.R0 = pGTest[0]->R;
		unproj.offset0 = pGTest[0]->offset;
		bSurfaceSurfaceContact = 0;
		bSurfaceEdgeContact = 0;
		if (unproj.imode==0 && unprojPlaneNorm.len2()>0 && fabs_tpl(unproj.dir*unprojPlaneNorm)<0.707f)
			(unproj.dir -= unprojPlaneNorm*(unproj.dir*unprojPlaneNorm)).normalize();	// do not restrict direction to requested plane if angle>pi/4

		// select the intersection segment with maximum length as unprojection starting point
		ibest = border.npt-1;
		maxlen = (border.pt[0]-border.pt[border.npt-1]).len2();
		for(i=0;i<border.npt-1;i++) if ((len=(border.pt[i]-border.pt[i+1]).len2()) > maxlen) {
			maxlen=len; ibest=i;
		}

		PrepareTriangle(border.itri[ibest][0]&IDXMASK,(triangle*)pprims[0],pGTest[0]);
		((indexed_triangle*)pprims[0])->idx = border.itri[ibest][0]&IDXMASK;
		if (border.itri[ibest][1]>=0) {
			((CTriMesh*)pGTest[1]->pGeometry)->PrepareTriangle(border.itri[ibest][1]&IDXMASK,(triangle*)pprims[1],pGTest[1]);
			((indexed_triangle*)pprims[1])->idx = border.itri[ibest][1]&IDXMASK;
		}

		t = 0;
		contact_best.pt = unproj.center;
		contact_best.n = pGTest[isneg(pGTest[1]->w.len2()-pGTest[0]->w.len2())]->axisContactNormal;
		contact_best.taux = contact_cur.taux = 1;
		if (!g_Unprojector.Check(&unproj, pGTest[0]->typeprim,pGTest[1]->typeprim, pprims[0],-1,pprims[1],-1, &contact_best) || !(contact_best.t>0)) {
			if ((border.npt>3 || pinters->nBestPtVal || !g_Unprojector.CheckExists(unproj.imode,pGTest[0]->typeprim,pGTest[1]->typeprim)) && bUseLSNormal==0)
				bUseLSNormal = 1;
			else
				bNoUnprojection = 1;
			goto end_unproj;
		}
		if (unproj.imode)
			contact_best.t = atan2(contact_best.t,contact_best.taux);

		for(nSmallSteps=0; contact_best.t>0 && nSmallSteps<3; ) {
			// modify pGTest[0] in accordance with contact_best.t
			t += contact_best.t;
			update_unprojection(t, &unproj, pGTest[0]);
			if (t>tmax) {
				bNoUnprojection = 1; goto end_unproj;
			}

			pres->iPrim[0] = ((indexed_triangle*)pprims[0])->idx;
			pres->iPrim[1] = ((indexed_triangle*)pprims[1])->*pidxoffs;
			nprims1 = pGTest[0]->pGeometry->GetUnprojectionCandidates(0,&contact_best, pprims[0],piFeature[0], pGTest[0]);
			nprims2 = pGTest[1]->pGeometry->GetUnprojectionCandidates(1,&contact_best, pprims[1],piFeature[1], pGTest[1]);

			ibest=jbest=0; contact_best.t=0;
			for(i=0,ptr[0]=(char*)pprims[0]; i<nprims1; i++,ptr[0]=ptr[0]+pGTest[0]->szprim)
				for(j=0,ptr[1]=(char*)pprims[1]; j<nprims2; j++,ptr[1]=ptr[1]+pGTest[1]->szprim)
          if (g_Unprojector.Check(&unproj, pGTest[0]->typeprim,pGTest[1]->typeprim, (primitive*)ptr[0],piFeature[0][i],(primitive*)ptr[1],piFeature[1][j], &contact_cur) &&
							contact_cur.t>contact_best.t)
					{
						ibest=i; jbest=j; contact_best=contact_cur;
						pres->id[0]=pGTest[0]->idbuf[i]; pres->id[1]=pGTest[1]->idbuf[j];
					}
			pprims[0] = (primitive*)(UINT_PTR)pprims[0] + pGTest[0]->szprim*ibest;
      pprims[1] = (primitive*)(UINT_PTR)pprims[1] + pGTest[1]->szprim*jbest;

			if (unproj.imode) {
				contact_best.t = atan2(contact_best.t,contact_best.taux);
				i = isneg(contact_best.t-(real)1E-3);
			} else
				i = isneg(contact_best.t-m_minVtxDist);
			nSmallSteps = (nSmallSteps & -i)+i;
		}

		// check if we have surface-surface or edge-surface contact (in the latter case test if both edge buddies are outside the surface)
		if (pres->parea) {
			for(i=0;i<pGTest[0]->nSurfaces;i++) for(j=0;j<pGTest[1]->nSurfaces;j++)
				if (pGTest[0]->surfaces[i].n*pGTest[1]->surfaces[j].n < -maxcos)
				{ bSurfaceSurfaceContact=1; goto end_unproj; } // surface - surface contact
			for(iop=0;iop<2;iop++) for(i=0;i<pGTest[iop]->nSurfaces;i++) for(j=0;j<pGTest[iop^1]->nEdges;j++)
				if (sqr(pGTest[iop]->surfaces[i].n*pGTest[iop^1]->edges[j].dir) < mincos2*pGTest[iop^1]->edges[j].dir.len2() &&
						sqr_signed(pGTest[iop]->surfaces[i].n*pGTest[iop^1]->edges[j].n[0]) > -mincos2*pGTest[iop^1]->edges[j].n[0].len2() &&
						sqr_signed(pGTest[iop]->surfaces[i].n*pGTest[iop^1]->edges[j].n[1]) > -mincos2*pGTest[iop^1]->edges[j].n[1].len2())
				{ bSurfaceEdgeContact=1; goto end_unproj; } // surface - edge contact
		}
		end_unproj:

		if (!bUseLSNormal && unproj.imode==1 && (contact_best.pt-unproj.center ^ unproj.dir).len2()<sqr(pGTest[0]->pParams->minAxisDist)) {
			unproj.imode = 0;
			unproj.dir = -contact_best.n;
			unproj.vel = unproj.dir*(pGTest[1]->v+(pGTest[1]->w^contact_best.pt-pGTest[1]->centerOfMass) -
				pGTest[0]->v-(pGTest[0]->w^contact_best.pt-pGTest[0]->centerOfMass));
			tmax = unproj.tmax = pGTest[0]->pParams->maxUnproj;
			pGTest[0]->R = unproj.R0;
			pGTest[0]->offset = unproj.offset0;
			bUseLSNormal = -1;
			bNoUnprojection = 0;
		} else if ((bUseLSNormal==1 || t>tmax) && bUseLSNormal!=-1) {	// if length is out of bounds (relative to vel)
			//   calc least squares normal for border
			//   request calculation of linear unprojection along this normal
			Vec3r inters_center,p0,p1,ntest;
			Matrix33r inters_C;
			real inters_len,det,maxdet;
			Vec3 n;

			pGTest[0]->R = unproj.R0;
			pGTest[0]->offset = unproj.offset0;

			if (pinters->nBestPtVal > border.npt) {
				inters_center = pinters->ptbest;
				n = n_avg = pinters->n;
			} else if (border.npt < 3)	{ // not enough points to calc least squares normal, use average normal
				n = border.n_best; inters_center = (border.pt[0]+border.pt[1])*0.5f;
			} else {
				border.pt[border.npt] = border.pt[0];
				for(ipt=0,inters_len=0,inters_center.zero(); ipt<border.npt; ipt++) {
					inters_len += border.seglen[ipt] = (border.pt[ipt+1]-border.pt[ipt]).len();
					inters_center += (border.pt[ipt+1]+border.pt[ipt])*border.seglen[ipt];
				}
				if (inters_len>0)
					inters_center /= inters_len*2;
				else
					inters_center = border.pt[0];

				for(ipt=0,inters_C.SetZero(),ntest.zero(); ipt<border.npt; ipt++) { // form inters_C matrix for least squares normal calculation
					p0 = border.pt[ipt]-inters_center; p1 = border.pt[ipt+1]-inters_center;
					ntest += p0^p1;
					for(i=0;i<3;i++) for(j=0;j<3;j++)
						inters_C(i,j) += border.seglen[ipt]*(2*(p0[i]*p0[j]+p1[i]*p1[j]) + p0[i]*p1[j]+p0[j]*p1[i]);
				}
				if (ntest.len2()<sqr(m_minVtxDist))
					n = border.n_best;
				else {
					// since inter_C matrix is (it least it should be) singular, we try to set one coordinate to 1
					// and find which one provides most stable solution
					for(i=0,maxdet=0;i<3;i++) {
						det = inters_C(inc_mod3[i],inc_mod3[i])*inters_C(dec_mod3[i],dec_mod3[i]) -
									inters_C(dec_mod3[i],inc_mod3[i])*inters_C(inc_mod3[i],dec_mod3[i]);
						if (fabs(det)>fabs(maxdet))
						{ maxdet=det; j=i; }
					}
					if (fabs(maxdet) < (real)1E-20) // normal is ill defined
						n = n_avg;
					else {
						det = 1.0/maxdet;
						n[j] = 1;
						n[inc_mod3[j]] = -(inters_C(inc_mod3[j],j)*inters_C(dec_mod3[j],dec_mod3[j]) -
							inters_C(dec_mod3[j],j)*inters_C(inc_mod3[j],dec_mod3[j]))*det;
						n[dec_mod3[j]] = -(inters_C(inc_mod3[j],inc_mod3[j])*inters_C(dec_mod3[j],j) -
							inters_C(dec_mod3[j],inc_mod3[j])*inters_C(inc_mod3[j],j))*det;
						n.normalize();
						if (ntest.len2()<sqr(sqr(m_minVtxDist*10)))
							ntest = n_avg;
						if (n*ntest<0)
							n.Flip();
					}
				}
			}

			unproj.imode = 0;
			pres->dir = unproj.dir = -n;
			unproj.vel = 0;
			tmax = unproj.tmax = pGTest[0]->pParams->maxUnproj;
			bUseLSNormal = -1;
			bNoUnprojection = 0;
			unprojPlaneNorm.zero();
			pres->center = inters_center;
		} else break;
	} while (true);

	//pres->iPrim[0] = pGTest[0]->typeprim==indexed_triangle::type ? ((indexed_triangle*)pprims[0])->idx : 0;
	//pres->iPrim[1] = pGTest[1]->typeprim==indexed_triangle::type ? ((indexed_triangle*)pprims[1])->idx : 0;
	if (pGTest[1]->typeprim!=indexed_triangle::type)
		pres->iPrim[1] = 0;

	if (bNoUnprojection) {
		pres->t = 0.0001;
		pres->pt = border.pt[0];
		pres->n = n_avg;
		pres->iFeature[0] = pinters->iFeature[0][0];
		pres->iFeature[1] = pinters->iFeature[0][1];
		pres->vel = -1;
		pres->parea = 0;
		pres->iUnprojMode = 0;
	} else {
		pres->t = t;
		pres->pt = contact_best.pt;
		pres->iFeature[0] = contact_best.iFeature[0];
		pres->iFeature[1] = contact_best.iFeature[1];
		pres->n = contact_best.n.normalized();
		pres->dir = unproj.dir;
		pres->iUnprojMode = unproj.imode;
		pres->vel = unproj.vel;

		if (bSurfaceSurfaceContact || bSurfaceEdgeContact) {
			coord_plane surface; surface.n.zero();
			Vec2 *ptbuf1,*ptbuf2,*ptbuf;
			int *idbuf1[2],*idbuf2[2],*idbuf,id,idmask,bEdgeEdge;
			int npt1,npt2,npt;
			G(PolyPtBufPos) = 0;
			WriteLock lock(g_lockBool2d);

			if (bSurfaceSurfaceContact) {
				pres->parea->type = geom_contact_area::polygon;
				pres->n = pGTest[0]->surfaces[i].n;	iop = 0;
				pres->parea->n1 = pGTest[1]->surfaces[j].n;
				npt1 = pGTest[0]->pGeometry->PreparePolygon(&surface, pGTest[0]->surfaces[i].idx,pGTest[0]->surfaces[i].iFeature, pGTest[0],
					ptbuf1,idbuf1[0],idbuf1[1]);
				npt2 = pGTest[1]->pGeometry->PreparePolygon(&surface, pGTest[1]->surfaces[j].idx,pGTest[1]->surfaces[j].iFeature, pGTest[1],
					ptbuf2,idbuf2[0],idbuf2[1]);
				npt = boolean2d(bool_intersect, ptbuf1,npt1, ptbuf2,npt2,1, ptbuf,idbuf);
			} else {
				pres->parea->type = geom_contact_area::polyline;
				pres->n = pGTest[iop]->surfaces[i].n*(sgnnz(pGTest[iop]->surfaces[i].n*pres->n)*(1-iop*2));
				pres->parea->n1 = (pGTest[iop^1]->edges[j].dir^(pGTest[iop]->surfaces[i].n^pGTest[iop^1]->edges[j].dir)).normalize();
				pres->parea->n1 *= -sgnnz(pres->parea->n1*pres->n);
				if (iop) {
					Vec3 ntmp=pres->n; pres->n=pres->parea->n1; pres->parea->n1=ntmp;
				}
				npt1 = pGTest[iop]->pGeometry->PreparePolygon(&surface, pGTest[iop]->surfaces[i].idx,pGTest[iop]->surfaces[i].iFeature, pGTest[iop],
					ptbuf1,idbuf1[0],idbuf1[1]);
				npt2 = pGTest[iop^1]->pGeometry->PreparePolyline(&surface, pGTest[iop^1]->edges[j].idx,pGTest[iop^1]->edges[j].iFeature, pGTest[iop^1],
					ptbuf2,idbuf2[0],idbuf2[1]);
				npt = boolean2d(bool_intersect, ptbuf1,npt1, ptbuf2,npt2,0, ptbuf,idbuf);
			}

			if (npt) {
				for(i=0;i<min(npt,pres->parea->nmaxpt);i++) {
					pres->parea->pt[i] = surface.origin + surface.axes[0]*ptbuf[i].x + surface.axes[1]*ptbuf[i].y;
					bEdgeEdge = -((idbuf[i]>>16)-1>>31 | (idbuf[i]&0xFFFF)-1>>31)^1;
					id = (idbuf[i]&0xFFFF)-1;	idmask = id>>31; id -= idmask;
					pres->parea->piPrim[iop][i] = idbuf1[bEdgeEdge][id]>>8 | idmask;
					pres->parea->piFeature[iop][i] = idbuf1[bEdgeEdge][id]&0xFF | idmask;
					id = (idbuf[i]>>16)-1; idmask = id>>31; id -= idmask;
					pres->parea->piPrim[iop^1][i] = idbuf2[bEdgeEdge][id]>>8 | idmask;
					pres->parea->piFeature[iop^1][i] = idbuf2[bEdgeEdge][id]&0xFF | idmask;
				}
				pres->parea->npt = i;
			} else pres->parea->npt = 0;

			g_nAreaPt += pres->parea->npt;
			g_nAreas++;

		} else
			pres->parea = 0;
	}

	if (!pGTest[0]->pParams->bExactBorder)
		if (pinters->nborderpt) {
			for(i=0;i<pinters->nborderpt;i++)
				pres->ptborder[pres->nborderpt++] = pinters->ptborder[i];
			g_BrdPtBufPos += pinters->nborderpt;
			pres->bBorderConsecutive = false;
		} else if (pinters->nBestPtVal>100) {
			pres->nborderpt = 1;
			pres->ptborder = &pres->center;
		}

	pGTest[0]->R = unproj.R0;
	pGTest[0]->offset = unproj.offset0;

	// allocate slots for the next intersection
	(*pGTest[0]->pnContacts)++;
	if (*pGTest[0]->pnContacts>=pGTest[0]->nMaxContacts || pGTest[0]->pParams->bStopAtFirstTri || pGTest[0]->pParams->bBothConvex)
		pGTest[0]->bStopIntersection = 1;
	else {
		pres = pGTest[0]->contacts + *pGTest[0]->pnContacts;
		if (!pGTest[0]->pParams->bNoAreaContacts &&
				g_nAreas<CRY_ARRAY_COUNT(g_AreaBuf) && g_nAreaPt<CRY_ARRAY_COUNT(g_AreaPtBuf))
		{
			pres->parea = g_AreaBuf+g_nAreas;
			pres->parea->pt = g_AreaPtBuf+g_nAreaPt;
			pres->parea->piPrim[0] = g_AreaPrimBuf0+g_nAreaPt; pres->parea->piFeature[0] = g_AreaFeatureBuf0+g_nAreaPt;
			pres->parea->piPrim[1] = g_AreaPrimBuf1+g_nAreaPt; pres->parea->piFeature[1] = g_AreaFeatureBuf1+g_nAreaPt;
			pres->parea->npt = 0; pres->parea->nmaxpt = CRY_ARRAY_COUNT(g_AreaPtBuf)-g_nAreaPt;
			pres->parea->minedge = min(pGTest[0]->minAreaEdge, pGTest[1]->minAreaEdge);
		} else
			pres->parea = 0;
	}
	return 1;
}


int CTriMesh::GetNeighbouringEdgeId(int vtxid, int ivtx)
{
	int iedge,itri,itrinew,itri0,iter=50;
	itri0=itri = vtxid>>8; iedge = dec_mod3[vtxid&31];
	do {
		if (m_pIndices[itri*3+iedge]==ivtx) // check leaving edge
			return itri<<8 | iedge | 0x20;
		if ((itrinew=m_pTopology[itri].ibuddy[iedge])<0)
			return -1;
		iedge = GetEdgeByBuddy(itrinew, itri); itri = itrinew;
		if (m_pIndices[itri*3+inc_mod3[iedge]]==ivtx) // check entering edge
			return itri<<8 | iedge | 0x20;
		iedge = dec_mod3[iedge];
	} while(itri!=itri0 && --iter>0);
	return -1;
}


int CTriMesh::PreparePolygon(coord_plane *psurface, int iPrim,int iFeature, geometry_under_test *pGTest, Vec2 *&ptbuf,
														 int *&pVtxIdBuf,int *&pEdgeIdBuf)
{
	int *pUsedVtxMap,UsedVtxIdx[16],nUsedVtx, *pUsedTriMap,UsedTriIdx[16],nUsedTri;
	int i,ihead,itail,ntri,nvtx,itri,itri_parent,ivtx0,ivtx,itri_new,iedge,iedge_open,ivtx0_new,ivtx_start,ivtx_end,ivtx_end1,
		ivtx_head,iorder,nSides;
	Vec3 n0,n,edge,pt0;

	if (psurface->n.len2()==0) {
		psurface->n = pGTest->R*m_pNormals[iPrim];
		psurface->axes[0] = pGTest->R*(m_pVertices[m_pIndices[iPrim*3+1]]-m_pVertices[m_pIndices[iPrim*3]]).normalized();
		psurface->axes[1] = psurface->n^psurface->axes[0];
		psurface->origin = pGTest->R*m_pVertices[m_pIndices[iPrim*3]]*pGTest->scale + pGTest->offset;
	}

	if ((m_nVertices-1>>5)+1<CRY_ARRAY_COUNT(g_UsedVtxMap))
		pUsedVtxMap = g_UsedVtxMap;
	else memset(pUsedVtxMap = new int[(m_nVertices-1>>5)+1], 0, ((size_t)m_nVertices-1>>5)+1<<2);
	if ((m_nTris-1>>5)+1<CRY_ARRAY_COUNT(g_UsedTriMap))
		pUsedTriMap = g_UsedTriMap;
	else memset(pUsedTriMap = new int[(m_nTris-1>>5)+1], 0, ((size_t)m_nTris-1>>5)+1<<2);
	nUsedVtx = nUsedTri = 0;

	#define ENQUEUE_TRI \
	if (itri_new>=0 && !(pUsedTriMap[itri_new>>5] & 1<<(itri_new&31)) && m_pNormals[itri_new]*n0>1.0f-2E-4f) {	\
		ihead = ihead+1&(int)(CRY_ARRAY_COUNT(g_TriQueue))-1; \
		ntri = min(ntri+1,(int)(CRY_ARRAY_COUNT(g_TriQueue))); \
		g_TriQueue[ihead].itri = itri_new; \
		g_TriQueue[ihead].itri_parent = itri;	\
		g_TriQueue[ihead].ivtx0 = ivtx0_new; \
		pUsedTriMap[itri_new>>5] |= 1<<(itri_new&31);	\
		UsedTriIdx[nUsedTri] = itri_new; nUsedTri = min(nUsedTri+1,15); \
		m_pTree->MarkUsedTriangle(itri_new, pGTest); \
	}

	ihead=-1; itail=0; n0=m_pNormals[iPrim]; itri=iPrim; ntri=0; nvtx=3;
	g_VtxList[0].ivtx=m_pIndices[itri*3+0]; g_VtxList[0].ibuddy[1]=1; g_VtxList[0].ibuddy[0]=2; g_VtxList[0].id=iPrim<<8;
	g_VtxList[1].ivtx=m_pIndices[itri*3+1]; g_VtxList[1].ibuddy[1]=2;	g_VtxList[1].ibuddy[0]=0;	g_VtxList[1].id=iPrim<<8|1;
	g_VtxList[2].ivtx=m_pIndices[itri*3+2]; g_VtxList[2].ibuddy[1]=0;	g_VtxList[2].ibuddy[0]=1;	g_VtxList[2].id=iPrim<<8|2;
	itri_new=m_pTopology[itri].ibuddy[0]; ivtx0_new=0; ENQUEUE_TRI
	itri_new=m_pTopology[itri].ibuddy[1]; ivtx0_new=1; ENQUEUE_TRI
	itri_new=m_pTopology[itri].ibuddy[2]; ivtx0_new=2; ENQUEUE_TRI
	ivtx_head = 0;

	while(ntri>0) {
		itri = g_TriQueue[itail].itri; itri_parent = g_TriQueue[itail].itri_parent;
		ivtx_head = ivtx0 = g_TriQueue[itail].ivtx0; itail = itail+1&CRY_ARRAY_COUNT(g_TriQueue)-1; ntri--;
		iedge = GetEdgeByBuddy(itri,itri_parent);
		ivtx = m_pIndices[itri*3+dec_mod3[iedge]];
		nSides = 0;

		if (pUsedVtxMap[ivtx>>5] & 1<<(ivtx&31)) {
			// triangle's free vertex is already in the list
			if (g_VtxList[ivtx_start = g_VtxList[ivtx0].ibuddy[0]].ivtx==ivtx) {
				ivtx_end = g_VtxList[g_VtxList[ivtx_start].ibuddy[1]].ibuddy[1]; // tringle touches previous buddy
				iedge_open = dec_mod3[iedge];	ivtx0_new = ivtx_head = ivtx_start; nSides++;
			}
			if (g_VtxList[ivtx_end1 = g_VtxList[g_VtxList[ivtx0].ibuddy[1]].ibuddy[1]].ivtx==ivtx) {
				ivtx_end = ivtx_end1; ivtx_start = ivtx0_new = ivtx0; // triangle touches next buddy
				iedge_open = inc_mod3[iedge]; nSides++;
			}
			if (nSides==2) { // if triangle touches both neighbours, it must have filled interior hole, trace it and "glue" sides
				ivtx_start = g_VtxList[ivtx0].ibuddy[0];
				ivtx_end = g_VtxList[g_VtxList[ivtx0].ibuddy[1]].ibuddy[1];
				for(; g_VtxList[ivtx_start].ivtx==g_VtxList[ivtx_end].ivtx;
						ivtx_start=g_VtxList[ivtx_start].ibuddy[0],ivtx_end=g_VtxList[ivtx_end].ibuddy[1]);
				ivtx_start = g_VtxList[ivtx_start].ibuddy[1];
			}
			if (nSides>0) { // remove one vertex if triangle touches only one neighbour
				g_VtxList[ivtx_start].ibuddy[1] = ivtx_end;
				g_VtxList[ivtx_end].ibuddy[0] = ivtx_start;
			}
		}
		if (nSides==0) { // add new vertex to the list if triangle does not touch neighbours
			if (nvtx==CRY_ARRAY_COUNT(g_VtxList))
				break;
			g_VtxList[nvtx].ivtx = ivtx;
			g_VtxList[nvtx].id = itri<<8|dec_mod3[iedge];
			g_VtxList[nvtx].ibuddy[1] = g_VtxList[ivtx0].ibuddy[1];
			g_VtxList[nvtx].ibuddy[0] = ivtx0;
			g_VtxList[ivtx0].ibuddy[1] = g_VtxList[g_VtxList[ivtx0].ibuddy[1]].ibuddy[0] = nvtx++;
			pUsedVtxMap[ivtx>>5] |= 1<<(ivtx&31);
			UsedVtxIdx[nUsedVtx] = ivtx; nUsedVtx = min(nUsedVtx+1,15);

			itri_new = m_pTopology[itri].ibuddy[inc_mod3[iedge]]; ivtx0_new = ivtx0;
			ENQUEUE_TRI
			iedge_open = dec_mod3[iedge];
			ivtx0_new = g_VtxList[ivtx0].ibuddy[1];
		}
		if (nSides<2) {
			itri_new = m_pTopology[itri].ibuddy[iedge_open];
			ENQUEUE_TRI
		}
	}
	#undef ENQUEUE_TRI

	if (pUsedVtxMap!=g_UsedVtxMap)
		delete[] pUsedVtxMap;
	else if (nUsedVtx>=15)
		memset(g_UsedVtxMap,0,((size_t)m_nVertices-1>>5)+1<<2);
	else for(i=0;i<nUsedVtx;i++)
		g_UsedVtxMap[UsedVtxIdx[i]>>5] &= ~(1<<(UsedVtxIdx[i]&31));
	if (pUsedTriMap!=g_UsedTriMap)
		delete[] pUsedTriMap;
	else if (nUsedTri>=15)
		memset(g_UsedTriMap,0,((size_t)m_nTris-1>>5)+1<<2);
	else for(i=0;i<nUsedTri;i++)
		g_UsedTriMap[UsedTriIdx[i]>>5] &= ~(1<<(UsedTriIdx[i]&31));

	ivtx_start = ivtx_head; nvtx = 0;
	ptbuf = g_PolyPtBuf + g_PolyPtBufPos;
	pVtxIdBuf = g_PolyVtxIdBuf + g_PolyPtBufPos;
	pEdgeIdBuf = g_PolyEdgeIdBuf + g_PolyPtBufPos;
	iorder = isnonneg((pGTest->R*n0)*psurface->n);
	do {
		pt0 = pGTest->R*m_pVertices[g_VtxList[ivtx_head].ivtx]*pGTest->scale + pGTest->offset - psurface->origin;
		ptbuf[nvtx].set(psurface->axes[0]*pt0, psurface->axes[1]*pt0); pVtxIdBuf[nvtx]=g_VtxList[ivtx_head].id;
		if ((unsigned int)(g_VtxList[ivtx_head].id>>8)>=(unsigned int)m_nTris)
			break;
		pEdgeIdBuf[nvtx++] = GetNeighbouringEdgeId(g_VtxList[ivtx_head].id, g_VtxList[g_VtxList[ivtx_head].ibuddy[iorder]].ivtx);
		ivtx_head = g_VtxList[ivtx_head].ibuddy[iorder];
	} while(ivtx_head!=ivtx_start && nvtx<CRY_ARRAY_COUNT(g_VtxList));
	ptbuf[nvtx] = ptbuf[0];	pVtxIdBuf[nvtx] = pVtxIdBuf[0]; pEdgeIdBuf[nvtx] = pEdgeIdBuf[0];
	g_PolyPtBufPos += nvtx+1;

	return nvtx;
}

int CTriMesh::PreparePolyline(coord_plane *psurface, int iPrim,int iFeature, geometry_under_test *pGTest, Vec2 *&ptbuf,
															int *&pVtxIdBuf,int *&pEdgeIdBuf)
{
	int *pUsedVtxMap,UsedVtxIdx[16],nUsedVtx=0;
	int nvtx,iedge,itri,itri_prev,ivtx,itri0,iorder,i,iprev,iter=0;
	Vec3 n0,edge,pt;

	if ((m_nVertices-1>>5)+1<CRY_ARRAY_COUNT(g_UsedVtxMap))
		pUsedVtxMap = g_UsedVtxMap;
	else memset(pUsedVtxMap = new int[(m_nVertices-1>>5)+1], 0, ((size_t)m_nVertices-1>>5)+1<<2);

	iedge = iFeature & 0x1F;
	itri_prev = itri0 = iPrim;
	nvtx = 2;
	g_VtxList[0].ivtx = m_pIndices[itri0*3+iedge];
	g_VtxList[1].ivtx = m_pIndices[itri0*3+inc_mod3[iedge]];
	g_VtxList[0].ibuddy[0] = g_VtxList[1].ibuddy[1] = -1;
	g_VtxList[0].ibuddy[1] = 1; g_VtxList[1].ibuddy[0] = 0;
	g_VtxList[0].id = itri0<<8|iedge;	g_VtxList[1].id = itri0<<8|inc_mod3[iedge];
	edge = m_pVertices[m_pIndices[itri0*3+inc_mod3[iedge]]] - m_pVertices[m_pIndices[itri0*3+iedge]];
	ivtx = m_pIndices[itri0*3+iedge]; pUsedVtxMap[ivtx>>5] |= 1<<(ivtx&31);	UsedVtxIdx[nUsedVtx++] = ivtx;
	ivtx = m_pIndices[itri0*3+inc_mod3[iedge]]; pUsedVtxMap[ivtx>>5] |= 1<<(ivtx&31);	UsedVtxIdx[nUsedVtx++] = ivtx;
	n0 = psurface->n*pGTest->R;
	n0 = (edge^(edge^n0)).normalized();

	for(iorder=1,iprev=nvtx-1;iorder>=0;iorder--) {
		do {
			itri = m_pTopology[itri_prev].ibuddy[iedge];
			if (itri<0 || itri==itri0)
				break;
			iedge = dec_mod3[GetEdgeByBuddy(itri,itri_prev)];
			edge = m_pVertices[m_pIndices[itri*3+inc_mod3[iedge]]] - m_pVertices[m_pIndices[itri*3+iedge]];
			if (fabsf(edge*n0)<1E-2f && sqr((m_pVertices[m_pIndices[itri*3+iedge]]+edge*0.5f-psurface->origin)*psurface->n) < edge.len2()*sqr(0.02f)) {
				itri0=itri; iedge=dec_mod3[iedge]; ivtx=m_pIndices[itri*3+inc_mod3[iedge]];
				if (pUsedVtxMap[ivtx>>5] & 1<<(ivtx&31))
					break;
				// add edge to list
				if (nvtx==CRY_ARRAY_COUNT(g_VtxList))
					break;
				g_VtxList[nvtx].ivtx = ivtx;
				g_VtxList[nvtx].id = itri<<8 | inc_mod3[iedge];
				g_VtxList[nvtx].ibuddy[iorder] = -1; g_VtxList[nvtx].ibuddy[iorder^1] = iprev;
				g_VtxList[iprev].ibuddy[iorder] = nvtx; iprev = nvtx++;
				pUsedVtxMap[ivtx>>5] |= 1<<(ivtx&31);
				UsedVtxIdx[nUsedVtx] = ivtx; nUsedVtx = min(nUsedVtx+1,15);
			}
			itri_prev = itri;
		} while(++iter<100);
		itri_prev = itri0 = iPrim;
		iedge = dec_mod3[iFeature & 0x1F];
		iprev = 0;
	}
	IF (pUsedVtxMap!=g_UsedVtxMap, 0){
		delete[] pUsedVtxMap;
	}
	else if (nUsedVtx>=15)
		memset(g_UsedVtxMap,0,((size_t)m_nVertices-1>>5)+1<<2);
	else for(i=0;i<nUsedVtx;i++)
		g_UsedVtxMap[UsedVtxIdx[i]>>5] &= ~(1<<(UsedVtxIdx[i]&31));

	ptbuf = g_PolyPtBuf + g_PolyPtBufPos;
	pVtxIdBuf = g_PolyVtxIdBuf + g_PolyPtBufPos;
	pEdgeIdBuf = g_PolyEdgeIdBuf + g_PolyPtBufPos;
	for(nvtx=0,ivtx=iprev; ivtx>=0 && nvtx<CRY_ARRAY_COUNT(g_VtxList); ivtx=g_VtxList[ivtx].ibuddy[1],nvtx++) {
		pt = pGTest->R*m_pVertices[g_VtxList[ivtx].ivtx]*pGTest->scale + pGTest->offset - psurface->origin;
		ptbuf[nvtx].set(psurface->axes[0]*pt, psurface->axes[1]*pt); pVtxIdBuf[nvtx]=g_VtxList[ivtx].id;
		pEdgeIdBuf[nvtx] = g_VtxList[ivtx].ibuddy[1]>=0 ?
			GetNeighbouringEdgeId(g_VtxList[ivtx].id, g_VtxList[g_VtxList[ivtx].ibuddy[1]].ivtx) : -1;
	}
	ptbuf[nvtx] = ptbuf[0]; pVtxIdBuf[nvtx] = pVtxIdBuf[0];	pEdgeIdBuf[nvtx] = pEdgeIdBuf[0];
	g_PolyPtBufPos += nvtx+1;

	return nvtx;
}

template <class ftype> class vector2_w_enforcer {
public:
  vector2_w_enforcer(Vec3_tpl<ftype> &_vec0,Vec3_tpl<ftype> &_vec1,ftype &_w) : vec0(&_vec0),vec1(&_vec1),w(&_w) {}
	~vector2_w_enforcer() {
		if (*w!=(ftype)1.0 && fabs_tpl(*w)>(ftype)1E-20) {
			float rw = 1.0f/(*w);
			*vec0*=rw; *vec1*=rw;
		}
	}
	Vec3_tpl<ftype> *vec0,*vec1;
	ftype *w;
};

int CTriMesh::FindClosestPoint(geom_world_data *pgwd, int &iPrim,int &iFeature, const Vec3 &ptdst0,const Vec3 &ptdst1,
															 Vec3 *ptres, int nMaxIters)
{
	Vec3 pt[3],n,nprev,edge,dp,seg,cross,ptdst,ptseg[3];
	int i,itri,ivtx,iedge,bEdgeOut[4],bOut,itri0,inewtri,itrimax,bBest,iter;
	float t,tmax,dist[3],ptdstw=1.0f;
	int iPrim_hist[4]={-1,-1,-1,-1},iFeature_hist[4]={-1,-1,-1,-1},ihist=0,bLooped;
	int bLine = isneg(1E-4f-(seg=ptdst1-ptdst0).len2());
	ptseg[0] = ptdst0; ptseg[1] = ptdst1;
	ptres[1] = ptdst0;
	ptres[0].Set(1E10f,1E10f,1E10f);
	vector2_w_enforcer<float> wdiv(ptres[1],ptres[0],ptdstw);

	if ((unsigned int)iPrim<(unsigned int)m_nTris && (iFeature&0x1F)<3) do {
		if ((iFeature&0x60)==0x40) { // face
			pt[0] = pgwd->R*m_pVertices[m_pIndices[iPrim*3+0]]*pgwd->scale + pgwd->offset;
			pt[1] = pgwd->R*m_pVertices[m_pIndices[iPrim*3+1]]*pgwd->scale + pgwd->offset;
			pt[2] = pgwd->R*m_pVertices[m_pIndices[iPrim*3+2]]*pgwd->scale + pgwd->offset;
			n = pgwd->R*m_pNormals[iPrim];

			if (bLine) {
				dist[0]=(ptseg[0]-pt[0])*n; dist[1]=(ptseg[1]-pt[0])*n;
				if (dist[0]*dist[1]<0 && fabsf(ptdstw=seg*n)>1E-4f) {
					ptres[1] = (ptseg[0]*ptdstw + seg*((pt[0]-ptseg[0])*n))*sgnnz(ptdstw);
					ptdstw = fabsf(ptdstw);
					pt[0]*=ptdstw; pt[1]*=ptdstw; pt[2]*=ptdstw;
				} else {
					ptres[1] = ptseg[isneg(dist[1]-dist[0])];
					ptdstw = 1.0f;
				}
			}	else {
				ptres[1] = ptdst0; ptdstw = 1.0f;
			}

			bEdgeOut[0] = isneg((pt[1]-pt[0]^ptres[1]-pt[0])*n);
			bEdgeOut[1] = isneg((pt[2]-pt[1]^ptres[1]-pt[1])*n);
			bEdgeOut[2] = isneg((pt[0]-pt[2]^ptres[1]-pt[2])*n);
			ptres[0] = ptres[1]-n*((ptres[1]-pt[0])*n);
			if (bEdgeOut[0]+bEdgeOut[1]+bEdgeOut[2]==0)
				return nMaxIters; // ptres[1] is in triangle's voronoi region

			i = (bEdgeOut[0]^1)+((bEdgeOut[0]|bEdgeOut[1])^1); // i = index of the 1st non-zero (unity) bEdgeOut
			if (m_pTopology[iPrim].ibuddy[i]>=0) {
				iFeature = GetEdgeByBuddy(m_pTopology[iPrim].ibuddy[i], iPrim) | 0x20;
				iPrim = m_pTopology[iPrim].ibuddy[i];
			} else {
				edge = pt[inc_mod3[i]]-pt[i]; t = min(ptdstw=edge.len2(),max(0.0f,(ptdst0-pt[i])*edge));
				ptres[0] = pt[i]*ptdstw+edge*t; ptres[1] *= ptdstw;
				iFeature = 0x20 | i; return nMaxIters;
			}

			/*for(i=0;i<3;i++) if (bEdgeOut[i]) {
				if ((ptres[1]-pt[i])*(pt[inc_mod3[i]]-pt[i])>0 && (ptres[1]-pt[inc_mod3[i]])*(pt[i]-pt[inc_mod3[i]])>0) {
					// ptres[1] is possibly in edge's voronoi region
					if (m_pTopology[iPrim].ibuddy[i]<0) {
						iFeature = 0x20 | i; return nMaxIters;
					}
					iFeature = GetEdgeByBuddy(m_pTopology[iPrim].ibuddy[i], iPrim) | 0x20;
					iPrim = m_pTopology[iPrim].ibuddy[i];
					n = pgwd->R*m_pNormals[iPrim];
					if ((pt[i]-pt[inc_mod3[i]]^ptres[1]-pt[i])*n<0) {
						edge = pt[inc_mod3[i]]-pt[i]; // ptres[1] is in edge's voronoi region
						//ptres[0] = pt[i]+edge*((edge*(ptres[1]-pt[i]))/edge.len2());
						ptres[0] = pt[i]*(t=edge.len2()) + edge*((edge*(ptres[1]-pt[i])));
						ptdstw *= t; ptres[1] *= t;
						return nMaxIters;
					} else {
						iFeature = 0x40; break;
					}
				}
			}
			if (i==3) {	// switch to vertex that is closest to the point
				i = isneg((pt[1]-ptres[1]).len2()-(pt[0]-ptres[1]).len2());
				i |= isneg((pt[2]-ptres[1]).len2()-(pt[i]-ptres[1]).len2())<<1; i &= 2|(i>>1^1);
				iFeature = i;
			}*/
		} else if ((iFeature&0x60)==0x20) {	// edge
			iedge = iFeature & 0x1F;
			n = pgwd->R*m_pNormals[iPrim];
			pt[0] = pgwd->R*m_pVertices[m_pIndices[iPrim*3+iedge]]*pgwd->scale + pgwd->offset;
			pt[1] = pgwd->R*m_pVertices[m_pIndices[iPrim*3+inc_mod3[iedge]]]*pgwd->scale + pgwd->offset;
			edge = pt[1]-pt[0];

			if (bLine) {
				cross = seg^edge;
				t = (pt[0]-ptseg[0]^edge)*cross; ptdstw = cross.len2();
				i = isneg(1E-6f-ptdstw); ptdstw = ptdstw*i + (i^1); // set ptdstw to 1 if it's too small - point is ill defined anyway
				t = min(ptdstw,max(0.0f,t));
				dist[0] = (ptseg[0]-pt[0]^edge).len2()*ptdstw;
				dist[1] = (ptseg[1]-pt[0]^edge).len2()*ptdstw;
				ptseg[2] = ptseg[0]*ptdstw + seg*t; dist[2] = sqr((ptseg[0]-pt[0])*cross)*edge.len2();
				i = idxmin3(dist);
				ptres[1] = ptseg[i]; ptdstw = ptdstw*(i>>1)+(i>>1^1); // reset ptdstw to 1.0 if i is not 2
				pt[0] *= ptdstw; pt[1] *= ptdstw; edge *= ptdstw;
			}	else {
				ptres[1] = ptdst0; ptdstw = 1.0f;
			}

			if (m_pTopology[iPrim].ibuddy[iedge]<0)
				iFeature = 0x40;
			else {
				nprev = pgwd->R*m_pNormals[m_pTopology[iPrim].ibuddy[iedge]];
				dp = ptres[1]-pt[0];
				bEdgeOut[0] = isnonneg((n^edge)*dp); // point leans to the 1st triangle
				bEdgeOut[1] = isneg((nprev^edge)*dp); // point leans to the 2nd triangle
				bEdgeOut[2] = isneg((ptres[1]-pt[0])*edge); // point leans to start vertex
				bEdgeOut[3] = isnonneg((ptres[1]-pt[1])*edge); // point leans to end vertex
				//ptres[0] = pt[0]+edge*((edge*dp)/edge.len2());
				ptres[0] = pt[0]*(t=edge.len2()) + edge*((edge*(ptres[1]-pt[0])));
				ptdstw *= t; ptres[1] *= t;

				if (bEdgeOut[0]+bEdgeOut[1]+bEdgeOut[2]+bEdgeOut[3]==0)
					return nMaxIters;
				if (bEdgeOut[0]+bEdgeOut[1]) {
					if (bEdgeOut[1]) iPrim = m_pTopology[iPrim].ibuddy[iedge];
					iFeature = 0x40;
				} else
					iFeature = inc_mod3[iedge]&-bEdgeOut[3] | iedge&~-bEdgeOut[3];
			}
		}	else if ((iFeature&0x60)==0) { // vertex
			pt[0] = pgwd->R*m_pVertices[m_pIndices[iPrim*3+(iFeature&0x1F)]]*pgwd->scale + pgwd->offset;

			if (bLine) {
				dist[0] = (ptseg[0]-pt[0]).len2();
				dist[1] = (ptseg[1]-pt[0]).len2();
				dist[2] = (pt[0]-ptseg[0]^seg).len2(); ptdstw=seg.len2(); dist[0]=sqr(dist[0])*ptdstw; dist[1]=sqr(dist[1])*ptdstw;
				ptseg[2] = ptseg[0]*ptdstw + seg*(seg*(pt[0]-ptseg[0]));
				i = idxmin3(dist);
				ptres[1] = ptseg[i];	ptdstw = ptdstw*(i>>1)+(i>>1^1); // reset ptdstw to 1.0 if i is not 2
				pt[0] *= ptdstw;
			} else {
				ptres[1] = ptdst0; ptdstw = 1.0f;
			}

			dp=ptres[1]-pt[0]; itri=itrimax=itri0=iPrim; ivtx=iFeature&0x1F; tmax=0; bOut=0;
			if (m_pTopology[itri].ibuddy[ivtx]>=0)
				nprev = pgwd->R*m_pNormals[m_pTopology[itri].ibuddy[ivtx]];
			else nprev.zero();
			iter=35; do {
				n = pgwd->R*m_pNormals[itri];
				bOut |= isneg((nprev^n)*dp+1E-7f);
				bBest = isneg(tmax-(t=n*dp));
				tmax = tmax*(bBest^1)+t*bBest;
				itrimax = itrimax&~-bBest | itri&-bBest;
				if ((inewtri = m_pTopology[itri].ibuddy[dec_mod3[ivtx]])<0)
					break;
				ivtx = GetEdgeByBuddy(inewtri,itri); itri = inewtri; nprev = n;
			} while(itri!=itri0 && --iter);
			ptres[0] = pt[0];
			if (!bOut)
				return nMaxIters;
			iPrim = itrimax; iFeature = 0x40;
		} else
			return -1;

		bLooped = iszero(iPrim-iPrim_hist[0] | iFeature-iFeature_hist[0]) | iszero(iPrim-iPrim_hist[1] | iFeature-iFeature_hist[1]) |
			iszero(iPrim-iPrim_hist[2] | iFeature-iFeature_hist[2]) | iszero(iPrim-iPrim_hist[3] | iFeature-iFeature_hist[3]);
		iPrim_hist[ihist] = iPrim; iFeature_hist[ihist] = iFeature; ihist = ihist+1&3;
		// if line mode and looped, don't allow final iteration to be face one (face doesn't compute ptres[1] properly)
	} while(!bLooped && --nMaxIters>0 || bLine && iFeature_hist[ihist-2&3]==0x40);
	else
		return -1;

	return max(nMaxIters,0);
}


int CTriMesh::PointInsideStatusMesh(const Vec3 &pt, indexed_triangle *pHitTri)
{
	int res=0;
	float mindist=1E20f;
	triangle atri;
	ray aray;
	int icell,i;
	Vec3 ptgrid;
	prim_inters inters;

	//if (m_nTris<4)
	//	return 0;
	PrepareForRayTest(0);

	ptgrid = m_hashgrid[0].Basis*(pt-m_hashgrid[0].origin);
	icell = min(m_hashgrid[0].size.x-1,max(0, float2int(m_hashgrid[0].stepr.x*ptgrid.x-0.5f)))*m_hashgrid[0].stride.x +
					min(m_hashgrid[0].size.y-1,max(0, float2int(m_hashgrid[0].stepr.y*ptgrid.y-0.5f)))*m_hashgrid[0].stride.y;
	aray.origin = pt;
	aray.dir = m_hashgrid[0].Basis.GetRow(2)*(m_hashgrid[0].step.x*m_hashgrid[0].size.x + m_hashgrid[0].step.y*m_hashgrid[0].size.y);
	for(i=m_pHashGrid[0][icell]; i<m_pHashGrid[0][icell+1]; i++) {
		atri.pt[0] = m_pVertices[m_pIndices[m_pHashData[0][i]*3+0]];
		atri.pt[1] = m_pVertices[m_pIndices[m_pHashData[0][i]*3+1]];
		atri.pt[2] = m_pVertices[m_pIndices[m_pHashData[0][i]*3+2]];
		atri.n = m_pNormals[m_pHashData[0][i]];
		if (ray_tri_intersection(&aray,&atri, &inters) && (inters.pt[0]-aray.origin)*aray.dir<mindist) {
			mindist = (inters.pt[0]-aray.origin)*aray.dir;
			res = isnonneg(aray.dir*atri.n);
			if (pHitTri) {
				pHitTri->pt[0]=atri.pt[0]; pHitTri->pt[1]=atri.pt[1]; pHitTri->pt[2]=atri.pt[2];
				pHitTri->n=atri.n; pHitTri->idx=m_pHashData[0][i];
			}
		}
	}

	return res;
}


void CTriMesh::PrepareForRayTest(float raylen)
{
	{ ReadLock lock(m_lockHash);
		if (m_nHashPlanes>0)
			return;
	}
	ReadLock lock0(m_lockUpdate);
	WriteLock lock(m_lockHash);
	if (m_nHashPlanes==0) {
		coord_plane hashplane;
		box bbox; m_pTree->GetBBox(&bbox);
		float rcellsize=0;
		Vec3i isz;
		int i,iPlane,b2Planes,bForce=isneg(raylen);
		raylen *= 1-2*bForce;
		Vec3 szOrg = bbox.size;
		bbox.size.x = max(bbox.size.x, raylen*0.5f);
		bbox.size.y = max(bbox.size.y, raylen*0.5f);
		bbox.size.z = max(bbox.size.z, raylen*0.5f);
		if (m_nTris>0) {
			for(i=1,iPlane=0;i<3;i++)
				iPlane += i-iPlane & -isneg(fabs_tpl(bbox.Basis.GetRow(iPlane)*m_pNormals[0]) - fabs_tpl(bbox.Basis.GetRow(i)*m_pNormals[0]));
			i = sgnnz(bbox.Basis.GetRow(iPlane)*m_pNormals[0]);
		} else
			i=iPlane = 1;
		bbox.Basis.SetRow(iPlane, bbox.Basis.GetRow(iPlane)*i);
		bbox.Basis.SetRow(inc_mod3[iPlane], bbox.Basis.GetRow(inc_mod3[iPlane])*i);

		if (raylen>0) {
			rcellsize = 1.0f/raylen;
			for(i=0;i<3;i++)
				isz[i] = float2int(bbox.size[i]*2*rcellsize+0.5f);
			for(i=0;i<3;i++) if (isz[inc_mod3[i]]*isz[dec_mod3[i]]>4096) {
				isz[inc_mod3[i]] = min(64,isz[inc_mod3[i]]);
				isz[dec_mod3[i]] = min(64,isz[dec_mod3[i]]);
			}
			if (m_nTris*(1-bForce) > (isz.x*isz.y+isz.x*isz.z+isz.y*isz.z)*6)
				return; // hash will be too inefficient
		}

		iPlane = isneg(bbox.size.y*bbox.size.z-bbox.size.x*bbox.size.z); // initially set iPlane correspond to the bbox axis w/ max. face area
		iPlane |= isneg(bbox.size[inc_mod3[iPlane]]*bbox.size[dec_mod3[iPlane]]-bbox.size.x*bbox.size.y)<<1; iPlane&=~(iPlane>>1);
		b2Planes = isneg((raylen>0 ? raylen*0.5f : (szOrg.x+szOrg.y+szOrg.z)*0.2f)-szOrg[iPlane]);
		for(i=0; i<1+b2Planes; i++,iPlane=inc_mod3[iPlane]) {
			hashplane.n = bbox.Basis.GetRow(iPlane);
			hashplane.axes[0] = bbox.Basis.GetRow(inc_mod3[iPlane]);
			hashplane.axes[1] = bbox.Basis.GetRow(dec_mod3[iPlane]);
			hashplane.origin = bbox.center;
			HashTrianglesToPlane(hashplane,Vec2(bbox.size[inc_mod3[iPlane]]*2,bbox.size[dec_mod3[iPlane]]*2),
				m_hashgrid[i],m_pHashGrid[i],m_pHashData[i], rcellsize);
		}
		m_nHashPlanes = i;
	}
}


void CTriMesh::HashTrianglesToPlane(const coord_plane &hashplane, const Vec2 &hashsize, grid &hashgrid,index_t *&pHashGrid,index_t *&pHashDataRet,
																		float rcellsize, float pad, const index_t* pIndices, int nTris, int maxhash)
{
	float maxsz,tsx,tex,tsy,tey,ts,te;
	Vec2 sz,pt[3],edge[3],step,rstep,ptc,sg;
	Vec2i isz,irect[2],ipt;
	int i,itri,ipass,bFilled,imax,ix,iy,idx;
	index_t dummy,*pgrid,iHashMax=0;
	Vec3 ptcur,origin;
	if (!pIndices) {
		pIndices=m_pIndices; nTris=m_nTris;
	}

	maxsz = max(hashsize.x,hashsize.y)*pad;
	sz.set(hashsize.x+maxsz, hashsize.y+maxsz);
	sz.x = max(0.0001f, max(sz.x, sz.y*0.001f));
	sz.y = max(sz.y, sz.x*0.001f); // make sure sz[imax]/sz[imax^1] produces sane results
	imax = isneg(sz.x-sz.y);
	if (rcellsize==0) {
		isz[imax] = float2int(sqrt_tpl(nTris*2*sz[imax]/sz[imax^1]));
		isz[imax^1] = max(1,nTris*2/max(1,isz[imax]));
	} else {
		isz.x = float2int(sz.x*rcellsize+0.499f);
		isz.y = float2int(sz.y*rcellsize+0.499f);
	}
	if (!maxhash)
		maxhash = 4*(1<<sizeof(index_t)); // 64 on PC, 16 on consoles
	isz.x = min_safe(maxhash,max_safe(1,isz.x));
	isz.y = min_safe(maxhash,max_safe(1,isz.y));
	memset(pHashGrid = new index_t[isz.x*isz.y+1], 0, (isz.x*isz.y+1)*sizeof(index_t));
	step.set(sz.x/isz.x, sz.y/isz.y);
	rstep.set(isz.x/sz.x, isz.y/sz.y);
	origin = hashplane.origin - hashplane.axes[0]*(step.x*isz.x*0.5f)-hashplane.axes[1]*(step.y*isz.y*0.5f);
	index_t* pHashData = &dummy;

	for(ipass=0;ipass<2;ipass++) {
		for(itri=nTris-1;itri>=0;itri--) { // iterate tris in reversed order to get them in accending order in grid (since the algorithm reverses order)
			i = isneg((m_pVertices[pIndices[itri*3+1]]-m_pVertices[pIndices[itri*3]] ^ m_pVertices[pIndices[itri*3+2]]-m_pVertices[pIndices[itri*3]])*hashplane.n)<<1;
			ptcur = m_pVertices[pIndices[itri*3+i]]-origin; pt[0].set(ptcur*hashplane.axes[0],ptcur*hashplane.axes[1]);
			ptcur = m_pVertices[pIndices[itri*3+1]]-origin; pt[1].set(ptcur*hashplane.axes[0],ptcur*hashplane.axes[1]);
			ptcur = m_pVertices[pIndices[itri*3+(i^2)]]-origin; pt[2].set(ptcur*hashplane.axes[0],ptcur*hashplane.axes[1]);
			edge[0] = pt[1]-pt[0]; edge[1] = pt[2]-pt[1]; edge[2] = pt[0]-pt[2];
			irect[1] = irect[0].set(float2int(pt[0].x*rstep.x-0.5f), float2int(pt[0].y*rstep.y-0.5f));
			ipt.set(float2int(pt[1].x*rstep.x-0.5f), float2int(pt[1].y*rstep.y-0.5f));
			irect[0].set(min(irect[0].x,ipt.x), min(irect[0].y,ipt.y));
			irect[1].set(max(irect[1].x,ipt.x), max(irect[1].y,ipt.y));
			ipt.set(float2int(pt[2].x*rstep.x-0.5f), float2int(pt[2].y*rstep.y-0.5f));
			irect[0].set(max(0,min(irect[0].x,ipt.x)), max(0,min(irect[0].y,ipt.y)));
			irect[1].set(min(isz.x-1,max(irect[1].x,ipt.x)), min(isz.y-1,max(irect[1].y,ipt.y)));

			for(iy=irect[0].y;iy<=irect[1].y;iy++)
			for(pgrid=pHashGrid+iy*isz.x+(ix=irect[0].x),ptc.set((ix+0.5f)*step.x,(iy+0.5f)*step.y);ix<=irect[1].x;ix++,pgrid++,ptc.x+=step.x) {
				bFilled = isneg(ptc-pt[0]^edge[0]) & isneg(ptc-pt[1]^edge[1]) & isneg(ptc-pt[2]^edge[2]); // check if cell center is inside triangle
				for(i=0;i<3;i++) { // for each edge, find intersections with cell x and y bounds, then check intersection of x and y ranges
					sg.x = sgn(edge[i].x); sg.y = sgn(edge[i].y);
					tsx = max(0.0f, (ptc.x-step.x*sg.x*0.5f-pt[i].x)*sg.x);
					tex = min(fabsf(edge[i].x), (ptc.x+step.x*sg.x*0.5f-pt[i].x)*sg.x);
					tsy = max(0.0f, (ptc.y-step.y*sg.y*0.5f-pt[i].y)*sg.y);
					tey = min(fabsf(edge[i].y), (ptc.y+step.y*sg.y*0.5f-pt[i].y)*sg.y);
					ts = tsx*fabsf(edge[i].y)+tsy*fabsf(edge[i].x)+fabsf(tsx*fabsf(edge[i].y)-tsy*fabsf(edge[i].x));
					te = tex*fabsf(edge[i].y)+tey*fabsf(edge[i].x)-fabsf(tex*fabsf(edge[i].y)-tey*fabsf(edge[i].x));
					// test tsx,tex and tsy,tey as well, since ts,te might be indefinite (0,0)
					bFilled |= isneg(tsx-tex-1E-10f) & isneg(tsy-tey-1E-10f) & isneg(ts-te-1E-10f);
				}
				*pgrid -= ipass & -bFilled;	// during the 2nd pass if bFilled, decrement *pgrid prior to storing
				idx = max((index_t)0,min(iHashMax,*pgrid));
				(pHashData[idx] &= ~-bFilled) |= itri&-bFilled; // store itri if bFilled is 1
				*pgrid += (ipass^1) & bFilled; // during the 1st pass if bFilled, increment *pgrid after storing
			}
		}
		if (ipass==0) {
			for(i=1;i<=isz.x*isz.y;i++) pHashGrid[i]+=pHashGrid[i-1];
			pHashData = new index_t[iHashMax = pHashGrid[isz.x*isz.y]];
			memset(pHashData, 0,iHashMax*sizeof(index_t));
			iHashMax--;
		}
	}

	hashgrid.Basis.SetRow(0,hashplane.axes[0]);
	hashgrid.Basis.SetRow(1,hashplane.axes[1]);
	hashgrid.Basis.SetRow(2,hashplane.n);
	hashgrid.origin = origin;
	hashgrid.step = step;
	hashgrid.stepr = rstep;
	hashgrid.size = isz;
	hashgrid.stride.set(1,isz.x);
	pHashDataRet = pHashData;
}


int CTriMesh::Intersect(IGeometry *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts)
{
	if (m_voxgrid && pCollider->GetType()==GEOM_RAY) {
		const ray& aray = ((CRayGeom*)pCollider)->m_ray;
		Vec3 org=aray.origin, dir=aray.dir;
		if (pdata1)
			org=((org-pdata1->offset)*pdata1->R)/pdata1->scale, dir=(dir*pdata1->R)/pdata1->scale;
		m_voxgrid->lastHit = m_voxgrid->RayTrace(org,dir);
	}

	if (pCollider->GetType()==GEOM_RAY && m_nHashPlanes>0) {
		ray aray;
		index_t trilist[3][1024];
		int nTris[3],iPlane,iListPlane,iListRes,iListTmp,ix,iy,iCell,bNoBackoff;
		Vec2 pt[2];
		Vec2i ipt[2],irect[2];
		float rscale = pdata1->scale==1.0f ? 1.0f:1.0f/pdata1->scale;
		indexed_triangle atri;
		prim_inters inters;
		unprojection_mode unproj;
		int i,j,i1,jmax,nSmallSteps,iEdge,bActive,bThreadSafe,bThreadSafeMesh,overflowed=0,valid=0, nContacts=0;
		int iCaller = get_iCaller();
		intptr_t idmask = ~iszero_mask(m_pIds);
		char idnull=(char)-1, *pidnull=&idnull, *pIds=(char*)((intptr_t)m_pIds&idmask|(intptr_t)pidnull&~idmask);
		bool bStopAtFirstTri;
		real t;
		contact contact_best,contact_cur;
		CRayGeom *pRay = (CRayGeom*)pCollider;
		ray *pray = (ray*)&pRay->m_ray;
		bNoBackoff = iszero((INT_PTR)m_pTree);

		aray.origin=pray->origin; aray.dir=pray->dir;
		if (pdata2) {
			aray.origin = pdata2->offset+pdata2->R*pray->origin*pdata2->scale;
			aray.dir = pdata2->R*pray->dir*pdata2->scale;
		}
		aray.origin = ((aray.origin-pdata1->offset)*pdata1->R)*rscale;
		aray.dir = (aray.dir*pdata1->R)*rscale;
		unproj.dir.zero();

		if (pparams) {
			if (((CGeometry*)pCollider)->m_iCollPriority<m_iCollPriority)
				unproj.imode = -1;
			else {
				unproj.imode = pparams->iUnprojectionMode;
				unproj.center = ((pparams->centerOfRotation-pdata1->offset)*pdata1->R)*rscale;
				if (unproj.imode) {
					unproj.dir = pparams->axisOfRotation;
					unproj.tmax = g_PI*0.5;
				}	else {
					unproj.dir = -pdata1->v+(pdata2 ? pdata2->v:Vec3(0));
					if (unproj.dir.len2()>0)
						unproj.dir.normalize();
					unproj.tmax = pparams->maxUnproj;
				}
				unproj.dir = unproj.dir*pdata1->R;
				unproj.minPtDist = m_minVtxDist;
			}
			bStopAtFirstTri = pparams->bStopAtFirstTri;
			pparams->pGlobalContacts = g_Contacts;
			bThreadSafe = pparams->bThreadSafe; bThreadSafeMesh = pparams->bThreadSafeMesh;
			j = pparams->bKeepPrevContacts; bActive = 0;
		}	else {
			unproj.imode = -1;
			bStopAtFirstTri = false;
			bThreadSafe=bThreadSafeMesh = 0; j = 0; bActive = 1;
		}

		ReadLockCond lock0(m_lockUpdate, (bThreadSafeMesh|isneg(m_lockUpdate))^1);
		ReadLock lock1(m_lockHash);
		if (m_nHashPlanes<=0) // can be changed by now due to mt
			goto skiphashes;
		if (!j)
			g_nTotContacts = 0;
		pcontacts = g_Contacts+g_nTotContacts;

		if ((unsigned int)pdata1->iStartNode-1u<(unsigned int)m_nTris) {
			atri.n = m_pNormals[pdata1->iStartNode-1];
			atri.pt[0] = m_pVertices[m_pIndices[(pdata1->iStartNode-1)*3+0]];
			atri.pt[1] = m_pVertices[m_pIndices[(pdata1->iStartNode-1)*3+1]];
			atri.pt[2] = m_pVertices[m_pIndices[(pdata1->iStartNode-1)*3+2]];
			atri.idx = pdata1->iStartNode-1;
			if (ray_tri_intersection(&aray,&atri,&inters)) {
				bStopAtFirstTri = true; goto cached_inters;
			}
		}

		for(iPlane=iListRes=0; iPlane<m_nHashPlanes; iPlane++) {
			pt[0] = Vec2(m_hashgrid[iPlane].Basis*(aray.origin-m_hashgrid[iPlane].origin));
			pt[1] = pt[0]+Vec2(m_hashgrid[iPlane].Basis*aray.dir);
			ipt[0].set(float2int(pt[0].x*m_hashgrid[iPlane].stepr.x-0.5f), float2int(pt[0].y*m_hashgrid[iPlane].stepr.y-0.5f));
			ipt[1].set(float2int(pt[1].x*m_hashgrid[iPlane].stepr.x-0.5f), float2int(pt[1].y*m_hashgrid[iPlane].stepr.y-0.5f));
			irect[0].set(max(0,min(ipt[0].x,ipt[1].x)), max(0,min(ipt[0].y,ipt[1].y)));
			irect[1].set(min(m_hashgrid[iPlane].size.x-1,max(ipt[0].x,ipt[1].x)), min(m_hashgrid[iPlane].size.y-1,max(ipt[0].y,ipt[1].y)));
			if (bNoBackoff-1 & (irect[0].x+1-irect[1].x>>31 | irect[0].x+1-irect[1].x>>31)) // can be ineffective for long rays
				goto skiphashes;

			nTris[iListPlane = inc_mod3[iListRes]] = 0; iListTmp = inc_mod3[iListPlane];
			for(iy=irect[0].y; iy<=irect[1].y; iy++)
			for(iCell=iy*m_hashgrid[iPlane].stride.y+(ix=irect[0].x)*m_hashgrid[iPlane].stride.x; ix<=irect[1].x; ix++,iCell+=m_hashgrid[iPlane].stride.x) {
				iListPlane^=iListTmp; iListTmp^=iListPlane; iListPlane^=iListTmp;
				nTris[iListPlane] = unite_lists(m_pHashData[iPlane]+m_pHashGrid[iPlane][iCell],m_pHashGrid[iPlane][iCell+1]-m_pHashGrid[iPlane][iCell],
					trilist[iListTmp],nTris[iListTmp], trilist[iListPlane],sizeof(trilist[0])/sizeof(trilist[0][0]));
			}
			if (nTris[iListPlane] < sizeof(trilist[0])/sizeof(trilist[0][0])) {
				if (iPlane>0 && !overflowed) {
					nTris[iListTmp] = intersect_lists(trilist[iListRes],nTris[iListRes], trilist[iListPlane],nTris[iListPlane], trilist[iListTmp]);
					iListRes = iListTmp;
				}	else
					iListRes = iListPlane;
				overflowed = 0;	valid = 1;
			} else
				overflowed = 1;
		}
		if (!valid)
			goto skiphashes;

		if (nTris[iListRes]) {
			for(i=0;i<nTris[iListRes] && g_nTotContacts+nContacts<g_maxContacts;i++) {
				atri.n = m_pNormals[trilist[iListRes][i]];
				atri.pt[0] = m_pVertices[m_pIndices[trilist[iListRes][i]*3+0]];
				atri.pt[1] = m_pVertices[m_pIndices[trilist[iListRes][i]*3+1]];
				atri.pt[2] = m_pVertices[m_pIndices[trilist[iListRes][i]*3+2]];
				atri.idx = trilist[iListRes][i];
				if (ray_tri_intersection(&aray,&atri,&inters)) {
					cached_inters:
					if (unproj.imode>=0) {
						// here goes a simplified version of RegisterIntersection
						if (unproj.imode>=0 && unproj.dir.len2()==0) {
							if (atri.n*aray.dir>0)
								continue;
							unproj.dir = atri.n^aray.dir;
							if (unproj.imode==0)
								unproj.dir = unproj.dir^aray.dir;
							//if (unproj.dir.len2()<0.002) unproj.dir = aray.dir.orthogonal();
							if (unproj.dir.len2()<0.002f) unproj.dir.SetOrthogonal(aray.dir);
							unproj.dir.normalize();
						}

						if (!g_Unprojector.Check(&unproj, triangle::type,ray::type, &atri,-1,&aray,-1, &contact_best))
							continue;
						if (unproj.imode)
							contact_best.t = atan2(contact_best.t,contact_best.taux);
						pcontacts[nContacts].iPrim[0] = atri.idx;

						for(nSmallSteps=0,t=0; contact_best.t>0 && nSmallSteps<3 && (contact_best.iFeature[0] & 0x80); ) {
							if (unproj.imode)
								contact_best.t = atan2(contact_best.t,contact_best.taux);
							t += contact_best.t;
							if (t>unproj.tmax)
								break;

							if ((j = m_pTopology[atri.idx].ibuddy[contact_best.iFeature[0]&0x1F])<0)
								break;
							atri.n = m_pNormals[j];
							if (unproj.imode)	{
								real cost=cos_tpl(t), sint=sin_tpl(t);
								for(i1=0;i1<3;i1++) atri.pt[i1] = m_pVertices[m_pIndices[j*3+i1]].GetRotated(unproj.center,unproj.dir,cost,sint);
								atri.n = atri.n.GetRotated(unproj.dir,cost,sint);
							}	else
								for(i1=0;i1<3;i1++) atri.pt[i1] = m_pVertices[m_pIndices[j*3+i1]]+unproj.dir*t;
							iEdge = GetEdgeByBuddy(j,atri.idx);
							atri.idx = j;

							contact_best.t = 0;
							if (g_Unprojector.Check(&unproj, triangle::type,ray::type, &atri,iEdge|0xA0,&aray,0xA0, &contact_cur)) {
								contact_best = contact_cur;
								pcontacts[nContacts].iPrim[0] = atri.idx;
								if (unproj.imode) {
									contact_best.t = atan2(contact_best.t,contact_best.taux);
									j = isneg(contact_best.t-(real)1E-3);
								} else
									j = isneg(contact_best.t-m_minVtxDist);
								nSmallSteps = (nSmallSteps & -j)+j;
							}
						}

						pcontacts[nContacts].t = (t+contact_best.t)*(unproj.imode+(1-unproj.imode)*pdata1->scale);
						pcontacts[nContacts].pt = pdata1->R*contact_best.pt*pdata1->scale+pdata1->offset;
						pcontacts[nContacts].n = pdata1->R*contact_best.n; // not normalized!
						pcontacts[nContacts].dir = pdata1->R*unproj.dir;
						pcontacts[nContacts].iFeature[0] = contact_best.iFeature[0];
						pcontacts[nContacts].iFeature[1] = contact_best.iFeature[1];
						pcontacts[nContacts].iUnprojMode = unproj.imode;
					} else {
						pcontacts[nContacts].pt = pdata1->R*inters.pt[0]*pdata1->scale+pdata1->offset;
						pcontacts[nContacts].t = (pdata1->R*(inters.pt[0]-aray.origin))*(pdata2 ? pdata2->R*pRay->m_dirn:pRay->m_dirn)*pdata1->scale;
						pcontacts[nContacts].n = pdata1->R*inters.n;
						pcontacts[nContacts].dir.zero();
						pcontacts[nContacts].iPrim[0] = atri.idx;
						pcontacts[nContacts].iFeature[0] = 0x40;
						pcontacts[nContacts].iFeature[1] = 0x20;
						pcontacts[nContacts].iUnprojMode = 0;
					}

					pcontacts[nContacts].vel = 0;
					pcontacts[nContacts].ptborder = &pcontacts[nContacts].pt;
					pcontacts[nContacts].nborderpt = 1;
					pcontacts[nContacts].parea = 0;
					pcontacts[nContacts].id[0] = pIds[atri.idx&idmask];
					pcontacts[nContacts].id[1] = -1;
					pcontacts[nContacts].iPrim[1] = 0;
					pcontacts[nContacts].iNode[0] = pcontacts[nContacts].iPrim[0]+1;
					pcontacts[nContacts].iNode[1] = 0;
					pcontacts[nContacts].center = pcontacts[nContacts].pt;
					nContacts++;
					if (bStopAtFirstTri)
						break;
				}
			}

			// sort contacts in descending t order
			geom_contact tmpcontact;
			for(i=0;i<nContacts-1;i++) {
				for(jmax=i,j=i+1; j<nContacts; j++) {
					idmask = -isneg(pcontacts[jmax].t-pcontacts[j].t);
					jmax = jmax&~idmask | j&idmask;
				}
				if (jmax!=i) {
					tmpcontact=pcontacts[i]; pcontacts[i]=pcontacts[jmax]; pcontacts[jmax]=tmpcontact;
				}
			}
			g_nTotContacts += nContacts;
			return nContacts;
		}
		return 0;
	}
skiphashes:
	return CGeometry::Intersect(pCollider,pdata1,pdata2,pparams,pcontacts);
}


void CTriMesh::CalcVolumetricPressure(geom_world_data *gwd, const Vec3 &epicenter,float k,float rmin,
																			const Vec3 &centerOfMass, Vec3 &P,Vec3 &L)
{
	ReadLock lock(m_lockUpdate);
	int i,j,npt,iter;
	Vec3 pt[64],ptc,dP,Pres(ZERO),Lres(ZERO),c,cm;
	float r2,t1,t2,rmin2,rscale;
	c = (epicenter-gwd->offset)*gwd->R;
	cm = (centerOfMass-gwd->offset)*gwd->R;
	rscale = 1.0f/gwd->scale;
	rmin2 = sqr(rmin*rscale);
	k *= rscale*rscale;

	for(i=0;i<m_nTris;i++) if (m_pNormals[i]*(m_pVertices[m_pIndices[i*3]]-c)>0) {
		pt[0]=m_pVertices[m_pIndices[i*3]]; pt[1]=m_pVertices[m_pIndices[i*3+1]]; pt[2]=m_pVertices[m_pIndices[i*3+2]];
		iter=0; npt=3; do {
			npt -= 3;
			for(j=0; j<3 && (pt[npt+inc_mod3[j]]-pt[npt+j] ^ pt[npt+inc_mod3[j]]-c)*m_pNormals[i]>1E-4; j++);
			if (j==3 && npt<=sizeof(pt)/sizeof(Vec3)-9) { // closest to the epicenter point lies inside triangle, split on this point
				ptc = c-m_pNormals[i]*(m_pNormals[i]*(c-pt[npt]));
				pt[npt+3]=pt[npt];pt[npt+5]=pt[npt+2]; pt[npt+6]=pt[npt];pt[npt+7]=pt[npt+1];
				for(j=0;j<3;j++) pt[npt+j*4] = ptc;
				npt += 9;
			} else {
				for(j=0;j<3;j++) {
					t1 = (pt[npt+j]-c).len2(); t2 = (pt[npt+inc_mod3[j]]-c).len2();
					if ((t1>rmin || t2>rmin) && (t1<(0.7f*0.7f)*t2 || t1>(1.0f/0.7f/0.7f)*t2))
						break;
				}
				if (j<3 && npt<=sizeof(pt)/sizeof(Vec3)-12) {
					for(j=0;j<3;j++) { // split triangle on into 4 if difference between distances from any 2 vertices to epicenter is too big
						pt[npt+j*3+3] = pt[npt+j];
						pt[npt+j*3+4] = (pt[npt+j]+pt[npt+inc_mod3[j]])*0.5f;
						pt[npt+j*3+5] = (pt[npt+j]+pt[npt+dec_mod3[j]])*0.5f;
					}
					for(j=0;j<3;j++)
						pt[npt+j] = (pt[npt+j*3+3]+pt[npt+inc_mod3[j]*3+3])*0.5f;
					npt += 12;
				}	else {
					ptc = (pt[npt]+pt[npt+1]+pt[npt+2])*(1.0f/3); r2 = (ptc-c).len2();
					dP = m_pNormals[i]*(((ptc-c)*m_pNormals[i])*(pt[npt+1]-pt[npt]^pt[npt+2]-pt[npt]).len()*0.5f*k/(sqrt_tpl(r2)*max(r2,rmin2)));
					Pres += dP; Lres += ptc-cm^dP;
				}
			}
		} while(npt>0 && ++iter<300);
	}
	P += gwd->R*Pres;
	L += gwd->R*Lres;
}


float CalcPyramidVolume(const Vec3 &pt0,const Vec3 &pt1,const Vec3 &pt2,const Vec3 &pt3, Vec3 &com)
{
	com = (pt0+pt1+pt2+pt3)*0.25f;
	return fabsf((pt1-pt0^pt2-pt0)*(pt3-pt0))*(1.0f/6);
}

float CTriMesh::CalculateBuoyancy(const plane *pplane, const geom_world_data *pgwd, Vec3 &massCenter)
{
	geometry_under_test gtest;
	triangle tri,tri0;

	Matrix33 planerot(Quat::CreateRotationV0V1(pplane->n,Vec3(0,0,1)));
	gtest.R = planerot;
	gtest.offset = gtest.R*(pgwd->offset-pplane->origin);
	gtest.R *= pgwd->R;
	gtest.scale = pgwd->scale;
	int itri,i,j,imask,nAbove,iLow,iHigh,sign[3],nPieces;
	float t,V[4],Vaccum=0;
	Vec3 com[4],com_accum(ZERO),pt[3];

	for(itri=0;itri<m_nTris;itri++) {
		PrepareTriangle(itri,&tri, &gtest);
		for(i=iLow=iHigh=0,nAbove=3; i<3; i++) {
			nAbove -= (sign[i] = isneg(tri.pt[i].z));
			imask = -isneg(tri.pt[i].z-tri.pt[iLow].z); iLow = i&imask | iLow&~imask;
			imask = -isneg(tri.pt[iHigh].z-tri.pt[i].z); iHigh = i&imask | iHigh&~imask;
		}
		for(i=j=0;i<3;i++) if (sign[i]^sign[inc_mod3[i]]) {
			t = -tri.pt[i].z/(tri.pt[inc_mod3[i]].z-tri.pt[i].z);
			PREFAST_SUPPRESS_WARNING(6386)
			pt[j++] = tri.pt[i]*(1.0f-t)+tri.pt[inc_mod3[i]]*t;
		}

		if (nAbove<2) {
			(tri0.pt[0] = tri.pt[0]).z = tri.pt[iHigh].z;
			(tri0.pt[1] = tri.pt[1]).z = tri.pt[iHigh].z;
			(tri0.pt[2] = tri.pt[2]).z = tri.pt[iHigh].z;
			V[0] = CalcPyramidVolume(tri.pt[iHigh],tri0.pt[inc_mod3[iHigh]],tri.pt[inc_mod3[iHigh]],tri0.pt[dec_mod3[iHigh]], com[0]);
			V[1] = CalcPyramidVolume(tri.pt[iHigh],tri0.pt[dec_mod3[iHigh]],tri.pt[dec_mod3[iHigh]],tri.pt[inc_mod3[iHigh]], com[1]);
			V[2] = fabsf((tri0.pt[1]-tri0.pt[0]^tri0.pt[2]-tri0.pt[0]).z*0.5f*tri0.pt[0].z);
			(com[2] = (tri0.pt[0]+tri0.pt[1]+tri0.pt[2])*(1.0f/3)).z = tri0.pt[0].z*0.5f;
			if (nAbove==1) {
				V[2] *= -1.0f; tri0.pt[iHigh].z = 0; nPieces = 4;
				V[3] = CalcPyramidVolume(tri.pt[iHigh],tri0.pt[iHigh],pt[0],pt[1], com[3]);
			}	else nPieces = 3;
		} else if (nAbove==2) {
			(tri0.pt[0] = tri.pt[iLow]).z = 0; nPieces = 1;
			V[0] = CalcPyramidVolume(tri.pt[iLow],tri0.pt[0],pt[0],pt[1], com[0]);
		} else nPieces = 0;

		sign[0] = -sgn(tri.n.z);
		for(i=0;i<nPieces;i++) {
			V[i] *= sign[0]; Vaccum += V[i]; com_accum += com[i]*V[i];
		}
	}

	if (Vaccum>0)
		massCenter = (com_accum/Vaccum)*planerot+pplane->origin;
	else massCenter.zero();

	return Vaccum;
}


void CTriMesh::CalculateMediumResistance(const plane *pplane, const geom_world_data *pgwd, Vec3 &dPres,Vec3 &dLres)
{
	geometry_under_test gtest;
	triangle tri;
	gtest.R = pgwd->R;
	gtest.offset = pgwd->offset;
	gtest.scale = pgwd->scale;
	int itri;

	dPres.zero(); dLres.zero();
	for(itri=0;itri<m_nTris;itri++) {
		PrepareTriangle(itri,&tri, &gtest);
		CalcMediumResistance(tri.pt,3,tri.n, *pplane, pgwd->v,pgwd->w,pgwd->centerOfMass, dPres,dLres);
	}
}

int CTriMesh::DrawToOcclusionCubemap(const geom_world_data *pgwd, int iStartPrim,int nPrims, int iPass, SOcclusionCubeMap* cubeMap)
{
	Vec3 pt[3];
	for(int i=iStartPrim;i<iStartPrim+nPrims;i++) {
		pt[0] = pgwd->R*m_pVertices[m_pIndices[i*3  ]]*pgwd->scale + pgwd->offset;
		pt[1] = pgwd->R*m_pVertices[m_pIndices[i*3+1]]*pgwd->scale + pgwd->offset;
		pt[2] = pgwd->R*m_pVertices[m_pIndices[i*3+2]]*pgwd->scale + pgwd->offset;
		RasterizePolygonIntoCubemap(pt,3, iPass, cubeMap);
	}
	return nPrims;
}

namespace {
	template<class T> struct SplitArray {
		SplitArray() { sz[0]=sz[1]=0; ptr[0]=ptr[1]=0; }
		T *ptr[2];
		int sz[2];
		T& operator[](int idx) { return ptr[idx>>27][idx & ~(1<<27)]; }
		T MapSafe(int idx) {
			int iarr=idx>>27, islot=idx & ~(1<<27);
			int bValid = -inrange(islot, -1,sz[iarr]);
			int ival = ptr[iarr][islot & bValid] | ~bValid;
			return idx & (ival>>31) | ival & ~(ival>>31);
		}
	};
}

void CTriMesh::RemapForeignIdx(int *pCurForeignIdx, int *pNewForeignIdx, int nTris)
{
	if (m_pForeignIdx) {
		int i,iop;
		SplitArray<int> pIdxMap;
		bop_meshupdate *pmu;

		for(i=0;i<nTris;i++)
			pIdxMap.sz[pCurForeignIdx[i]>>27] = max(pIdxMap.sz[pCurForeignIdx[i]>>27], pCurForeignIdx[i]&~BOP_NEWIDX0);
		++pIdxMap.sz[0]; ++pIdxMap.sz[1];
		pIdxMap.ptr[1] = (pIdxMap.ptr[0] = new int[pIdxMap.sz[0]+pIdxMap.sz[1]])+pIdxMap.sz[0];
		for(i=0;i<pIdxMap.sz[0]+pIdxMap.sz[1];i++) pIdxMap.ptr[0][i] = -1;

		for(i=0;i<nTris;i++)
			pIdxMap[pCurForeignIdx[i]] = pNewForeignIdx[i];
		for(i=0;i<m_nTris;i++)
			m_pForeignIdx[i] = pIdxMap.MapSafe(m_pForeignIdx[i]);

		for(pmu=m_pMeshUpdate; pmu; pmu=pmu->next) {
			for(i=0;i<pmu->nNewVtx;i++)
				pmu->pNewVtx[i].idxTri[0] = pIdxMap.MapSafe(pmu->pNewVtx[i].idxTri[0]);
			for(i=0;i<pmu->nNewTri;i++) if (pmu->pNewTri[i].iop==0)
				pmu->pNewTri[i].idxOrg = pIdxMap.MapSafe(pmu->pNewTri[i].idxOrg);
			for(i=0;i<pmu->nRemovedTri;i++)
				pmu->pRemovedTri[i] = pIdxMap.MapSafe(pmu->pRemovedTri[i]);
			for(i=0;i<pmu->nTJFixes;i++) {
				pmu->pTJFixes[i].iABC = pIdxMap.MapSafe(pmu->pTJFixes[i].iABC);
				pmu->pTJFixes[i].iACJ = pIdxMap.MapSafe(pmu->pTJFixes[i].iACJ);
			}
		}
		for(pmu=(bop_meshupdate*)m_refmu.nextRef; pmu!=&m_refmu; pmu=(bop_meshupdate*)pmu->nextRef) {
			for(i=0;i<pmu->nNewVtx;i++)
				pmu->pNewVtx[i].idxTri[1] = pIdxMap.MapSafe(pmu->pNewVtx[i].idxTri[1]);
			iop = this!=pmu->pMesh[0];
			for(i=0;i<pmu->nNewTri;i++)	if (pmu->pNewTri[i].iop==iop)
				pmu->pNewTri[i].idxOrg = pIdxMap.MapSafe(pmu->pNewTri[i].idxOrg);
		}

		delete[] pIdxMap.ptr[0];
	}
}

void CTriMesh::AppendVertices(Vec3 *pVtx,int *pVtxMap, int nVtx)
{
	if (nVtx>0) {
		if (!ExpandVtxList()) {
			m_flags |= mesh_no_booleans; return;
		}
		Vec3 *pvtx = m_pVertices.data;
		int *pmap = m_pVtxMap;
		ReallocateList(m_pVertices.data, m_nVertices,m_nVertices+nVtx, false,false);
		if (m_pVtxMap) ReallocateList(m_pVtxMap, m_nVertices,m_nVertices+nVtx, false,false);
		delete[] pmap; delete[] pvtx;
		m_pVertices.iStride = sizeof(Vec3); m_flags &= ~mesh_shared_vtx;

		int i;
		for(i=0;i<nVtx;i++) m_pVertices[m_nVertices+i] = pVtx[i];
		if (m_pVtxMap && pVtxMap)
			for(i=0;i<nVtx;i++) m_pVtxMap[m_nVertices+i] = pVtxMap[i];
		m_nVertices += nVtx;
	}
}


float CTriMesh::GetIslandDisk(int matid, const Vec3 &ptref, Vec3 &center,Vec3 &normal,float &peakDist)
{
	if (!m_pIds)
		return 0;
	ReadLock lock(m_lockUpdate);
	const int szq=256;
	int i,j,itri,itri1=-1,ihead,itail,queue[szq];
	Vec3 cnt,cnt0(1E10f),ntot;
	float area,totArea,det,maxdet,r;
	Matrix33 C,Ctmp;
	box bbox;

	for(itri=0; itri<m_nTris; itri++) if (m_pIds[itri]==matid) {
		cnt = (m_pVertices[m_pIndices[itri*3]]+m_pVertices[m_pIndices[itri*3+1]]+m_pVertices[m_pIndices[itri*3+2]])*(1.0f/3);
		if ((cnt-ptref).len2()<(cnt0-ptref).len2()) {
			cnt0=cnt; itri1=itri;
		}
	}
	if (itri1<0)
		return 0;

	queue[itail=0]=itri1; ihead=1;
	cnt.zero(); totArea=0; C.SetZero(); ntot.zero();
	do {
		itri = queue[itail]; ++itail&=szq-1;
		const Vec3 &pt0=m_pVertices[m_pIndices[itri*3]], &pt1=m_pVertices[m_pIndices[itri*3+1]], &pt2=m_pVertices[m_pIndices[itri*3+2]];
		totArea += (area=(pt1-pt0^pt2-pt0)*normal);
		cnt += (pt0+pt1+pt2)*(area*(1.0f/3));	ntot += m_pNormals[itri];
		for(i=0;i<3;i++) if ((itri1=m_pTopology[itri].ibuddy[i])>=0)
			if (m_pIds[itri1]==matid) {
				if (ihead!=(itail-1&szq-1)) {
					m_pIds[queue[ihead]=itri1]=(char)255;	++ihead&=szq-1;
				}
			}	else if (m_pIds[itri1]!=(char)255)
				C += dotproduct_matrix(m_pNormals[itri1],m_pNormals[itri1],Ctmp);
	} while(ihead!=itail);
	if (totArea!=0)
		cnt /= totArea;

	for(i=0,j=-1,maxdet=0;i<3;i++) {
		det = C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],dec_mod3[i]) - C(dec_mod3[i],inc_mod3[i])*C(inc_mod3[i],dec_mod3[i]);
		if (fabs_tpl(det)>fabs_tpl(maxdet))
			maxdet=det, j=i;
	}
	if (j>=0) {
		det = 1.0/maxdet;
		normal[j] = 1;
		normal[inc_mod3[j]] = -(C(inc_mod3[j],j)*C(dec_mod3[j],dec_mod3[j]) - C(dec_mod3[j],j)*C(inc_mod3[j],dec_mod3[j]))*det;
		normal[dec_mod3[j]] = -(C(inc_mod3[j],inc_mod3[j])*C(dec_mod3[j],j) - C(dec_mod3[j],inc_mod3[j])*C(inc_mod3[j],j))*det;
		normal.normalize() *= sgnnz(ntot*normal);
	}	else
		return 0;

	m_pTree->GetBBox(&bbox); r = max(max(bbox.size.x,bbox.size.y),bbox.size.z)*2; peakDist=0;
	for(itri=0;itri<m_nTris;itri++) if (m_pIds[itri]==(char)255) {
		for(i=0;i<3;i++) {
			if ((itri1=m_pTopology[itri].ibuddy[i])<0 || m_pIds[itri1]!=(char)255 && m_pIds[itri1]!=matid) {
				r = min(r,(cnt-m_pVertices[m_pIndices[itri*3+i]]).len2());
				r = min(r,(cnt-m_pVertices[m_pIndices[itri*3+inc_mod3[i]]]).len2());
			}
			peakDist = max(peakDist, (m_pVertices[m_pIndices[itri*3+i]]-cnt)*normal);
		}
		m_pIds[itri] = matid;
	}

	r = sqrt_tpl(r);
	center = cnt;
	return r;
}


void CTriMesh::DrawWireframe(IPhysRenderer *pRenderer, geom_world_data *gwd, int iLevel, int idxColor)
{
	int i,ix,iy;
	if (iLevel<0 && m_voxgrid && m_voxgrid->data)	{
		Vec3 offs(ZERO); Quat q(IDENTITY); float scale=1;
		if (gwd)
			offs=gwd->offset, q=Quat(gwd->R), scale=gwd->scale;
		m_voxgrid->DrawHelpers(pRenderer,offs,q,scale, -iLevel-1);
	}
	if (iLevel==0)
		pRenderer->DrawGeometry(this,gwd,idxColor);
	else if (iLevel>=0) {
		int iCaller = get_iCaller();
		BV *pbbox;
		ResetGlobalPrimsBuffers(iCaller);
		m_pTree->ResetCollisionArea();
		m_pTree->GetNodeBV(gwd->R,gwd->offset,gwd->scale, pbbox, 0,iCaller);
		if (pbbox->type==box::type)
			DrawBBox(pRenderer,idxColor,gwd, m_pTree,(BBox*)pbbox,iLevel-1,0,iCaller);
		else if (pbbox->type==voxelgrid::type) {
			voxelgrid *pgrid = (voxelgrid*)(primitive*)*pbbox;
			Vec3 pt0,pt1;
			for(i=0;i<3;i++) for(ix=0;ix<=pgrid->size[inc_mod3[i]];ix++) for(iy=0;iy<=pgrid->size[dec_mod3[i]];iy++) {
				pt0[inc_mod3[i]] = ix*pgrid->step[inc_mod3[i]]; pt0[dec_mod3[i]] = iy*pgrid->step[dec_mod3[i]];
				pt0[i] = 0; (pt1=pt0)[i] = pgrid->size[i]*pgrid->step[i];
				pRenderer->DrawLine(pgrid->origin+pt0*pgrid->Basis, pgrid->origin+pt1*pgrid->Basis,idxColor);
			}
		}
	}
}


void CTriMesh::GetMemoryStatistics(ICrySizer *pSizer)
{
	if (GetType()==GEOM_TRIMESH)
		pSizer->AddObject(this, sizeof(CTriMesh));
	m_pTree->GetMemoryStatistics(pSizer);
	int i, ivtx0; for(i=0,ivtx0=m_nVertices;i<m_nTris*3;i++) ivtx0=min(ivtx0,(int)m_pIndices[i]);

	{ SIZER_COMPONENT_NAME(pSizer, "mesh data");
		if (!(m_flags & mesh_shared_idx))
			pSizer->AddObject(m_pIndices, m_nTris*3*sizeof(m_pIndices[0]));
		if (m_pIds && !(m_flags & mesh_shared_mats))
			pSizer->AddObject(m_pIds, m_nTris*sizeof(m_pIds[0]));
		if (m_pForeignIdx && !(m_flags & mesh_shared_foreign_idx))
			pSizer->AddObject(m_pForeignIdx, m_nTris*sizeof(m_pForeignIdx[0]));
		pSizer->AddObject(m_pNormals, m_nTris*sizeof(m_pNormals[0]));
		if (!(m_flags & mesh_shared_vtx))
			pSizer->AddObject(&m_pVertices[ivtx0], (m_nVertices-ivtx0)*sizeof(m_pVertices[0]));
		if (m_pVtxMap)
			pSizer->AddObject(m_pVtxMap+ivtx0, (m_nVertices-ivtx0)*sizeof(m_pVtxMap[0]));
		pSizer->AddObject(m_pIslands, m_nIslands*sizeof(m_pIslands[0]));
		pSizer->AddObject(m_pTri2Island, m_nTris*sizeof(m_pTri2Island[0]));
	}

	if (ivtx0) {
		SIZER_COMPONENT_NAME(pSizer, "wasted mesh data");
		if (!(m_flags & mesh_shared_vtx))
			pSizer->AddObject(&m_pVertices[0], ivtx0*sizeof(m_pVertices[0]));
		if (m_pVtxMap)
			pSizer->AddObject(m_pVtxMap, ivtx0*sizeof(m_pVtxMap[0]));
	}

	{ SIZER_COMPONENT_NAME(pSizer, "auxilary data");
		pSizer->AddObject(m_pTopology, m_nTris*sizeof(m_pTopology[0]));
		for(i=0;i<m_nHashPlanes;i++) {
			pSizer->AddObject(m_pHashGrid[i], (m_hashgrid[i].size.x*m_hashgrid[i].size.y+1)*sizeof(m_pHashGrid[i][0]));
			pSizer->AddObject(m_pHashData[i], m_pHashGrid[i][m_hashgrid[i].size.x*m_hashgrid[i].size.y]*sizeof(m_pHashData[i][0]));
		}
		for(bop_meshupdate *pmu=m_pMeshUpdate; pmu; pmu=pmu->next) {
			pSizer->AddObject(pmu, sizeof(*pmu));
			pSizer->AddObject(pmu->pRemovedVtx, sizeof(pmu->pRemovedVtx[0]), pmu->nRemovedVtx);
			pSizer->AddObject(pmu->pRemovedTri, sizeof(pmu->pRemovedTri[0]), pmu->nRemovedTri);
			pSizer->AddObject(pmu->pNewVtx, sizeof(pmu->pNewVtx[0]), pmu->nNewVtx);
			pSizer->AddObject(pmu->pNewTri, sizeof(pmu->pNewTri[0]), pmu->nNewTri);
			pSizer->AddObject(pmu->pWeldedVtx, sizeof(pmu->pWeldedVtx[0]), pmu->nWeldedVtx);
			pSizer->AddObject(pmu->pTJFixes, sizeof(pmu->pTJFixes[0]), pmu->nTJFixes);
			pSizer->AddObject(pmu->pMovedBoxes, sizeof(pmu->pMovedBoxes[0]), pmu->nMovedBoxes);
		}
	}
}

void CTriMesh::Save(CMemStream &stm)
{
	int i;
	int dummy[4] = { 0 };

	stm.Write(m_nVertices);
	stm.Write(m_nTris);
	stm.Write(m_nMaxVertexValency);
	stm.Write(m_flags);
	if (m_pVtxMap) {
		stm.Write(true);
		for(i=0;i<m_nVertices;i++)
			stm.Write(*(unsigned short*)(m_pVtxMap+i));
	} else stm.Write(false);

	if (m_pForeignIdx) {
		stm.Write(true);
		for(i=0;i<m_nTris;i++)
			stm.Write(*(unsigned short*)(m_pForeignIdx+i));
	} else
		stm.Write(false);
	if (!m_pForeignIdx || m_flags & mesh_full_serialization) {
		if (!(m_flags & mesh_shared_vtx))
			for(i=0;i<m_nVertices;i++) stm.Write(m_pVertices[i]);
		for(i=0;i<m_nTris*3;i++) stm.Write(*(short*)(m_pIndices+i));
		if (m_pIds) {
			stm.Write(true);
			stm.Write(m_pIds, sizeof(m_pIds[0])*m_nTris);
		} else
			stm.Write(false);
	}

	stm.Write(dummy);
	stm.Write((int)0);
	/*stm.Write(m_nHashPlanes);
	for(i=0;i<m_nHashPlanes;i++) {
		stm.Write(m_pHashGrid[i], (m_hashgrid[i].size.x*m_hashgrid[i].size.y+1)*sizeof(m_pHashGrid[i][0]));
		stm.Write(m_pHashData[i], m_pHashGrid[i][m_hashgrid[i].size.x*m_hashgrid[i].size.y]*sizeof(m_pHashData[i][0]));
	}*/

	for(i=0;i<4;i++) {
		stm.Write(m_bConvex[i]);
		stm.Write(m_ConvexityTolerance[i]);
	}

	stm.Write(m_pTree->GetType());
	m_pTree->Save(stm);
}

static Vec3 *g_pVtxBuf=0;
static int *g_pVtxUsed=0;
static int *g_pMapBuf=0,g_nVtxAlloc=0;

static inline void ExpandGlobalLoadState(int nVerts)
{
	if (g_nVtxAlloc>0)
		delete[] g_pVtxBuf,delete[] g_pMapBuf, delete[] g_pVtxUsed;
	g_pVtxBuf = new Vec3[g_nVtxAlloc = (nVerts-1 & ~511)+512];
	g_pVtxUsed = new int[g_nVtxAlloc];
	g_pMapBuf = new int[g_nVtxAlloc];
}

void CTriMesh::CleanupGlobalLoadState()
{
	delete[] g_pVtxBuf;
	delete[] g_pVtxUsed;
	delete[] g_pMapBuf;
	g_pVtxBuf = NULL;
	g_pVtxUsed = NULL;
	g_pMapBuf = NULL;
	g_nVtxAlloc=0;
}

void CTriMesh::Load(CMemStream &stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char *pIds)
{
	int i,j,dummy[4];
	bool bForeignIdx,bVtxMap,bIds;
	box bbox;

	stm.Read(m_nVertices);
	stm.Read(m_nTris);
	stm.Read(m_nMaxVertexValency);
	stm.Read(m_flags);
	m_flags |= mesh_keep_vtxmap;
	PREFAST_SUPPRESS_WARNING(6239)
	if (sizeof(index_t)<4 && (m_nTris>10000 || m_nVertices>30000))
		goto meshfail;

	if (!(m_flags & mesh_shared_vtx) && m_nVertices>g_nVtxAlloc)
		ExpandGlobalLoadState(m_nVertices);

	m_pIndices = new index_t[m_nTris*3];

	stm.Read(bVtxMap);
	if (bVtxMap) {
		m_pVtxMap = (m_flags & mesh_shared_vtx) ? new int[m_nVertices] : g_pMapBuf;
		for(i=0;i<m_nVertices;i++)
			m_pVtxMap[i] = stm.Read<uint16>();
	}

	stm.Read(bForeignIdx);
	if (bForeignIdx && !(m_flags & mesh_full_serialization)) {
		if (!(m_flags & mesh_shared_vtx))	{
			m_pVertices = strided_pointer<Vec3>(g_pVtxBuf);//new Vec3[m_nVertices];
			for(i=0;i<m_nVertices;i++) m_pVertices[i] = pVertices[i];
		} else
			m_pVertices = strided_pointer<Vec3>((Vec3*)pVertices.data,pVertices.iStride);
		if (pIds) {
			m_pIds = new char[m_nTris];
			for(i=0;i<m_nTris;i++) m_pIds[i] = pIds[i];
		} else
			m_pIds = 0;

		m_pForeignIdx = new int[m_nTris];
		for(i=0;i<m_nTris;i++) {
			m_pForeignIdx[i] = stm.Read<uint16>();
			for(j=0;j<3;j++) m_pIndices[i*3+j] = pIndices[m_pForeignIdx[i]*3+j];
			if (pIds) m_pIds[i] = pIds[m_pForeignIdx[i]];
		}
	}	else {
		if (bForeignIdx) {
			m_pForeignIdx = new int[m_nTris];
			for(i=0;i<m_nTris;i++)
				m_pForeignIdx[i] = stm.Read<uint16>();
		}
		if (!(m_flags & mesh_shared_vtx)) {
			m_pVertices = strided_pointer<Vec3>(g_pVtxBuf);//new Vec3[m_nVertices];
			for(i=0;i<m_nVertices;i++) stm.Read(m_pVertices[i]);
		} else
			m_pVertices = strided_pointer<Vec3>((Vec3*)pVertices.data,pVertices.iStride);
		for(i=0;i<m_nTris*3;i++) { m_pIndices[i] = stm.Read<uint16>(); }
		stm.Read(bIds);
		if (bIds) {
			if (!m_pIds) m_pIds = new char[m_nTris];
			stm.ReadRaw(m_pIds, m_nTris);
		} else if (m_pIds) {
			delete[] m_pIds; m_pIds = 0;
		}
	}
	if (bVtxMap) {
		for(i=0;i<m_nTris*3;i++) m_pIndices[i] = m_pVtxMap[m_pIndices[i]];
		if (!(m_flags & mesh_keep_vtxmap)) {
			delete[] m_pVtxMap; m_pVtxMap = 0;
		}
	}
	m_pNormals = new Vec3[m_nTris];
	for(i=0;i<m_nTris;i++)
		RecalcTriNormal(i);

	m_pTopology = new trinfo[m_nTris];
	CalculateTopology(m_pIndices);

	stm.ReadRaw(dummy, sizeof dummy);

	stm.Read(i);
	if (i) {
		meshfail:
		m_nTris = 0;
		CSingleBoxTree *pTree = new CSingleBoxTree;
		bbox.Basis.SetIdentity(); bbox.center.zero(); bbox.size.zero();
		pTree->SetBox(&bbox); pTree->Build(this); pTree->m_nPrims = m_nTris;
		m_pTree = pTree;
		return;
	}
	/*stm.Read(m_nHashPlanes);
	for(i=0;i<m_nHashPlanes;i++) {
		stm.Write(m_pHashGrid[i], (m_hashgrid[i].size.x*m_hashgrid[i].size.y+1)*sizeof(m_pHashGrid[i][0]));
		stm.Write(m_pHashData[i], m_pHashGrid[i][m_hashgrid[i].size.x*m_hashgrid[i].size.y]*sizeof(m_pHashData[i][0]));

		m_pHashGrid[i] = new index_t[m_hashgrid[i].size.x*m_hashgrid[i].size.y+1];
		stm.ReadType(m_pHashGrid[i], m_hashgrid[i].size.x*m_hashgrid[i].size.y+1);
		m_pHashData[i] = new index_t[m_pHashGrid[i][m_hashgrid[i].size.x*m_hashgrid[i].size.y]];
		stm.ReadType(m_pHashData[i], m_pHashGrid[i][m_hashgrid[i].size.x*m_hashgrid[i].size.y]);
	}*/

	m_bMultipart = BuildIslandMap()>1;
	for(i=0;i<4;i++) {
		stm.Read(m_bConvex[i]);	m_bConvex[i] &= m_bMultipart^1;
		stm.Read(m_ConvexityTolerance[i]);
	}
	m_bIsConvex = IsConvex(0.02f);

	stm.Read(i);
	switch (i) {
		case BVT_OBB: m_pTree = new COBBTree; break;
		case BVT_AABB: m_pTree = new CAABBTree; break;
		case BVT_SINGLEBOX: m_pTree = new CSingleBoxTree; break;
		//default: m_nTris=0; return;
	}
	m_pTree->Load(stm,this);
	if (i==BVT_SINGLEBOX && m_bIsConvex)
		m_pTree->SetGeomConvex();
	if (m_nMaxVertexValency<3) {
		unsigned char *pValency = new unsigned char[m_nVertices];
		memset(pValency, 0, m_nVertices);
		for(i=m_nTris*3-1;i>=0;i--) pValency[m_pIndices[i]]++;
		for(m_nMaxVertexValency=i=0;i<m_nVertices;i++)
			m_nMaxVertexValency = max(m_nMaxVertexValency, (int)pValency[i]);
		delete[] pValency;
	}

	m_pTree->GetBBox(&bbox);
	m_minVtxDist = max(max(bbox.size.x,bbox.size.y),bbox.size.z)*0.0002f;

	if (!(m_flags & mesh_shared_vtx)) {
		if (m_pVtxMap) for(i=0,m_ivtx0=m_nVertices;i<m_nTris*3;i++)
			m_ivtx0 = min(m_ivtx0, min((int)m_pIndices[i],(int)m_pVtxMap[m_pIndices[i]]));
		else for(i=0,m_ivtx0=m_nVertices;i<m_nTris*3;i++)
			m_ivtx0 = min(m_ivtx0, (int)m_pIndices[i]);
		if (m_ivtx0*4>m_nVertices) {
			memcpy(m_pVertices.data=new Vec3[m_nVertices-m_ivtx0], g_pVtxBuf+m_ivtx0, (m_nVertices-m_ivtx0)*sizeof(m_pVertices.data[0]));
			for(i=0;i<m_nTris*3;i++) m_pIndices[i]-=m_ivtx0;
			if (m_pVtxMap) {
				memcpy(m_pVtxMap=new int[m_nVertices-m_ivtx0], g_pMapBuf+m_ivtx0, (m_nVertices-m_ivtx0)*sizeof(m_pVtxMap[0]));
				for(i=0;i<m_nVertices-m_ivtx0;i++) m_pVtxMap[i]-=m_ivtx0;
			}
			m_nVertices -= m_ivtx0;
		}
	}
	if (m_pVertices.data==g_pVtxBuf)
		memcpy(m_pVertices.data=new Vec3[m_nVertices], g_pVtxBuf, m_nVertices*sizeof(g_pVtxBuf[0]));
	if (m_pVtxMap==g_pMapBuf)
		memcpy(m_pVtxMap=new int[m_nVertices], g_pMapBuf, m_nVertices*sizeof(int));
}

void CTriMesh::CompactMemory()
{
	// compact vertices together in case they are sparse - this should never be called on procedurally breakable meshes
	if (m_flags & mesh_shared_vtx)
		return;

	if (m_nVertices>g_nVtxAlloc)
		ExpandGlobalLoadState(m_nVertices);

	int i, n=0;
	memset(g_pVtxUsed,-1,sizeof(g_pVtxUsed[0])*m_nVertices);
	for (i=0; i<m_nTris*3; i++)	// Find which verts are used and create a remapping
		if (g_pVtxUsed[m_pIndices[i]]==-1)
			g_pVtxUsed[m_pIndices[i]]=n++;

	if (n<m_nVertices) {
		Vec3* pNewVertices = new Vec3[n];
		for (i=0; i<m_nTris*3; i++)	{ // Create new verts, and remap the indices
			pNewVertices[g_pVtxUsed[m_pIndices[i]]]=m_pVertices[m_pIndices[i]];
			m_pIndices[i] = g_pVtxUsed[m_pIndices[i]];
		}
		if (m_pVtxMap)
			delete[] m_pVtxMap;
		delete[] m_pVertices.data;
		m_pVertices = strided_pointer<Vec3>(pNewVertices); m_nVertices=n; m_ivtx0=0; m_pVtxMap=0;
		m_flags|=mesh_no_booleans; // cppcheck-suppress memleak
	}	
}

int CTriMesh::ExpandVtxList()
{
	if (m_ivtx0>0) {
		int i;
		Vec3 *pvtx = m_pVertices.data;
		int *pmap = m_pVtxMap;
		memcpy((m_pVertices.data = new Vec3[m_nVertices+m_ivtx0])+m_ivtx0, pvtx, m_nVertices*sizeof(Vec3));
		for(i=0;i<m_nTris*3;i++) m_pIndices[i] += m_ivtx0;
		if (m_pVtxMap) {
			memcpy((m_pVtxMap = new int[m_nVertices+m_ivtx0])+m_ivtx0, pmap, m_nVertices*sizeof(int));
			for(i=0;i<m_nVertices;i++) m_pVtxMap[i] += m_ivtx0;
		}
		delete[] pvtx; delete[] pmap;
		m_nVertices+=m_ivtx0; m_ivtx0=0;
	}
	return 1;
}


int CTriMesh::UnprojectSphere(Vec3 center,float r,float rsep,contact *pcontact)
{
	ReadLock lock(m_lockUpdate);
	int i,iedge=0,itri,itriNext,nOutside,idx[3],bInside,icell;
	triangle atri;

	if ((unsigned int)(itri=pcontact->iFeature[0])<(unsigned int)m_nTris) {
		for(i=0;i<3;i++)
			atri.pt[i] = m_pVertices[m_pIndices[itri*3+i]];
		for(i=nOutside=0;i<3;i++)
			nOutside+=isneg((atri.pt[inc_mod3[i]]-atri.pt[i] ^ center-atri.pt[i])*m_pNormals[itri]);
		if (nOutside==0) {
			pcontact->pt = center + m_pNormals[itri]*(m_pNormals[itri]*(atri.pt[0]-center));
			pcontact->n = m_pNormals[itri];
			if (pcontact->n*(center-pcontact->pt) > 0)
				return isneg((pcontact->pt-center).len2()-sqr(rsep));
		}
	}

	PrepareForRayTest(0);
	if (!m_pUsedTriMap)
		m_pUsedTriMap = new unsigned int[m_nTris+31>>5];
	memset(m_pUsedTriMap, 0, (m_nTris+31>>5)*4);

	Vec3 edge,pt=m_hashgrid[0].Basis*(center-m_hashgrid[0].origin);
	icell = min(m_hashgrid[0].size.x-1,max(0, float2int(m_hashgrid[0].stepr.x*pt.x-0.5f)))*m_hashgrid[0].stride.x +
					min(m_hashgrid[0].size.y-1,max(0, float2int(m_hashgrid[0].stepr.y*pt.y-0.5f)))*m_hashgrid[0].stride.y;
	prim_inters inters;
	ray aray; aray.origin = center;
	aray.dir = m_hashgrid[0].Basis.GetRow(2)*(m_hashgrid[0].step.x*m_hashgrid[0].size.x + m_hashgrid[0].step.y*m_hashgrid[0].size.y);
	aray.dir *= sgnnz(aray.dir*(center-GetCenter()));
	for(i=m_pHashGrid[0][icell],bInside=0; i<m_pHashGrid[0][icell+1]; i++) {
		atri.n = m_pNormals[itri=m_pHashData[0][i]];
		for(int j=0;j<3;j++)
			atri.pt[j] = m_pVertices[m_pIndices[itri*3+j]];
		if (bInside = ray_tri_intersection(&aray,&atri, &inters))
			break;
	}

	if (!bInside) {
		CRayGeom gray;
		gray.m_dirn = (m_center-center).normalized();
		gray.m_ray.origin=center; gray.m_ray.dir=gray.m_dirn*(rsep*3.5f);
		geom_world_data gwd;
		intersection_params ip;
		geom_contact *pcontacts;
		ip.bThreadSafe=ip.bThreadSafeMesh = 1;
		ip.bStopAtFirstTri = true;
		if (!Intersect(&gray,&gwd,0,&ip,pcontacts))
			return 0;
		itri = pcontacts->iPrim[0];
	}

	while(true) {
		for(i=0;i<3;i++)
			atri.pt[i] = m_pVertices[m_pIndices[itri*3+i]];
		if (check_mask(m_pUsedTriMap,itri)) {
			edge = atri.pt[inc_mod3[iedge]]-atri.pt[iedge];
			pt = atri.pt[iedge] + edge*(max(0.0f,min(1.0f,(edge*(center-atri.pt[iedge]))/edge.len2())));
			break;
		}
		set_mask(m_pUsedTriMap, itri);
		for(i=nOutside=0;i<3;i++)
			idx[nOutside]=i, nOutside+=isneg((atri.pt[inc_mod3[i]]-atri.pt[i] ^ center-atri.pt[i])*m_pNormals[itri]);
		if (nOutside==0) {
			pt = center + m_pNormals[itri]*(m_pNormals[itri]*(atri.pt[0]-center));
			break;
		}
		i = idx[check_mask(m_pUsedTriMap,m_pTopology[itri].ibuddy[idx[0]]) & 1-nOutside>>31];
		itriNext = m_pTopology[itri].ibuddy[i];
		iedge = GetEdgeByBuddy(itriNext, itri);
		itri = itriNext;
	}
	if ((pt-center).len2()*(1-bInside) > sqr(rsep))
		return 0;
	pcontact->n = (center-pt).normalized() * (1-bInside*2);
	pcontact->iFeature[0] = itri;
	pcontact->pt = pt;
	return 1;
}

namespace TriMeshStaticData
{
	void GetMemoryUsage(ICrySizer *pSizer)
	{
		pSizer->AddObject(g_pVtxBuf, sizeof(Vec3)*g_nVtxAlloc);
		pSizer->AddObject(g_pMapBuf, sizeof(int)*g_nVtxAlloc);
	}
}


inline Vec3i float2int(const Vec3 &v) { return Vec3i(float2int(v.x),float2int(v.y),float2int(v.z)); }

template<bool rngchk> CTriMesh::voxgrid_tpl<rngchk> CTriMesh::Voxelize(quaternionf q, float celldim, const index_t* pIndices, int nTris, int pad)
{
	int i,ix,iy,iz,idmask=-1;
	float rcelldim = 1.0f/celldim;
	char *pIds=0, id0=0;
	if (!pIndices) {
		pIndices=m_pIndices; nTris=m_nTris;	pIds=m_pIds;
	}
	if (!pIds) {
		pIds=&id0; idmask=0;
	}
	Vec3 ptmin,ptmax;	ptmin=ptmax=m_pVertices[pIndices[0]]*q;
	for(i=1;i<nTris*3;i++) {
		Vec3 pt = m_pVertices[pIndices[i]]*q;
		ptmin = min(ptmin,pt); ptmax = max(ptmax,pt);
	}
	Vec3 c=(ptmin+ptmax)*0.5f, sz=(ptmax-ptmin)*0.5f;
	Vec3i voxdim = float2int(sz*rcelldim+Vec3(0.5f))+Vec3i(pad+1);
	voxgrid_tpl<rngchk> grid(voxdim,q*c);	grid.q=q;	grid.celldim=celldim;	grid.rcelldim=rcelldim;

	for(i=0;i<nTris;i++) if (pIds[i&idmask]>=0) {
		Vec3 vtx[3]; for(int j=0;j<3;j++) vtx[j] = m_pVertices[pIndices[i*3+j]]*q-c;
		float area = Vec2(vtx[1])-Vec2(vtx[0])^Vec2(vtx[2])-Vec2(vtx[0]);
		if (fabs(area)*sqr(rcelldim)>1e-6f) {
			int dir = sgnnz(area);
			Vec3 zblend = Vec3(vtx[0].z,vtx[1].z,vtx[2].z)/area;
			ptmin = min(min(vtx[0],vtx[1]),vtx[2])*rcelldim; ptmax=max(max(vtx[0],vtx[1]),vtx[2])*rcelldim;
			Vec2i iBBox[2] = { Vec2i(float2int(ptmin.x-0.5f),float2int(ptmin.y-0.5f)), Vec2i(float2int(ptmax.x+0.5f),float2int(ptmax.y+0.5f)) };
			for(ix=iBBox[0].x;ix<iBBox[1].x;ix++) for(iy=iBBox[0].y;iy<iBBox[1].y;iy++) {
				Vec2 pt=Vec2(ix+0.5f,iy+0.5f)*celldim, ptx[3]={ Vec2(vtx[0])-pt,Vec2(vtx[1])-pt,Vec2(vtx[2])-pt };
				Vec3 coord = Vec3(ptx[1]^ptx[2], ptx[2]^ptx[0], ptx[0]^ptx[1]);
				if (min(min(coord.x*area,coord.y*area),coord.z*area) > 0)
					for(iz=float2int((coord*zblend)*rcelldim)-1; iz>-voxdim.z; iz--)
						grid(ix,iy,iz) += dir;
			}
		}
	}
	vox_iterate(grid,ix,iy,iz) {
		int &vox = grid(ix,iy,iz); vox = min(1,max(0,vox));
	}
	return grid;
}

template<bool rngchk> Vec3i CTriMesh::voxgrid_tpl<rngchk>::RayTrace(const Vec3& orgw, const Vec3& dirw)
{
	Vec3 org=(orgw-center)*q, dir=dirw*q, diff=org.abs()-Vec3(sz)*celldim;
	Vec3_tpl<quotientf> t;
	int i;
	if (max(max(diff.x,diff.y),diff.z)>0) {
		for(i=0;i<3;i++) t[i].set(-sz[i]*celldim-org[i]*sgnnz(dir[i]), fabs(dir[i]));
		quotientf tmax = max(max(t[0],t[1]),t[2]);
		if (tmax<=0 || sqr_signed(tmax)>dir.len2())
			return Vec3i(0);
		Vec3 offs = dir*tmax.val();
		org += offs; dir -= offs;
	}
	Vec3i iorg=float2int(org*rcelldim-Vec3(0.5f)), idir=Vec3i(sgnnz(dir.x),sgnnz(dir.y),sgnnz(dir.z)), istep=max(idir,Vec3i(0));	
	for(i=0;i<3;i++) iorg[i] = min(sz[i]-1,max(-sz[i],iorg[i]));
	do {
		if ((*this)(iorg) & bitVis)
			return iorg;
		for(i=0;i<3;i++) t[i].set(((iorg[i]+istep[i])*celldim-org[i])*idir[i], fabs(dir[i]));
		i = idxmin3(t);	
	} while (inrange(iorg[i]+=idir[i], -sz[i]-1,sz[i]) && ((Vec3(iorg)+Vec3(0.5f))*celldim-org).len2()<dir.len2());
	return sz;
}

#include "boxgeom.h"
template<bool rngchk> void CTriMesh::voxgrid_tpl<rngchk>::DrawHelpers(IPhysRenderer *pRenderer, const Vec3& pos,const quaternionf& qrot,float scale, int flags)
{
	int ix,iy,iz,bitmax,v;
	box boxvox; boxvox.center.zero(); boxvox.Basis.SetIdentity(); boxvox.size=Vec3(0.49f);
	CBoxGeom boxGeom; boxGeom.CreateBox(&boxvox);
	geom_world_data gwd;
	gwd.R = Matrix33(qrot*q);	gwd.scale = celldim*scale;
	Vec3 offs0 = pos + qrot*center*scale;
	union {	float f; uint i; } u = { bitVis };
	bitmax = (u.i>>23)-127;	bitmax += (bitVis>>bitmax)-1;

	vox_iterate((*this),ix,iy,iz) if ((v=(*this)(ix,iy,iz))&bitVis) {
		gwd.offset = offs0+gwd.R*Vec3(ix+0.5f,iy+0.5f,iz+0.5f)*gwd.scale;
		pRenderer->DrawGeometry(&boxGeom, &gwd, 4+(v>>bitmax-1&2));
	}

	if (flags & 1) {
		if (surf.norm && surf.vtx) for(ix=0;ix<surf.ncells;ix++)
			pRenderer->DrawLine(pos+qrot*surf.vtx[ix]*scale, pos+qrot*(surf.vtx[ix]+surf.norm[ix]*celldim*2)*scale, 4);
		if (surf.nbcnt && surf.vtx) for(ix=0;ix<surf.ncells;ix++) for(iy=surf.nbcnt[ix];iy<surf.nbcnt[ix+1];iy++)
			pRenderer->DrawLine(pos+qrot*surf.vtx[ix]*scale, pos+qrot*surf.vtx[surf.nbidx[iy]]*scale, 2);
	}

	if (lastHit.x<sz.x) {
		char buf[32]; sprintf(buf,"%d,%d,%d",lastHit.x,lastHit.y,lastHit.z);
		if (bitVis==4) {
			strcat(buf," (");	strcat(ltoa(packnb(*this,lastHit,2,4).count,buf+strlen(buf),10),")");	
		}
		pRenderer->DrawText(pos+qrot*(center+q*Vec3(lastHit)*celldim)*scale,buf,1, ((*this)(lastHit) & bitVis)==bitVis);
	}

	for(Vec3 dir(1,ix=0,0); ix<3; ix++,dir=dir.GetRotated(Vec3(1/sqrt3),-0.5f,sqrt3*0.5f))
		pRenderer->DrawLine(offs0-gwd.R*dir*sz[ix]*gwd.scale, offs0+gwd.R*dir*sz[ix]*gwd.scale, 2);
}

quotientf EdgeDistSq(const CTriMesh &mesh, int istart,int iend,int ivtx)
{
	Vec3 edge = mesh.m_pVertices[mesh.m_pIndices[iend]]-mesh.m_pVertices[mesh.m_pIndices[istart]];
	return quotientf(sqr(edge^mesh.m_pVertices[mesh.m_pIndices[ivtx]]-mesh.m_pVertices[mesh.m_pIndices[istart]]),edge.len2());
}

Vec3 SVD3x3(const Matrix33 &C)
{
	int i,j; float maxdet;
	for(i=0,j=-1,maxdet=0;i<3;i++) {
		float det = C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],dec_mod3[i]) - C(dec_mod3[i],inc_mod3[i])*C(inc_mod3[i],dec_mod3[i]);
		if (fabs_tpl(det)>fabs_tpl(maxdet))
			maxdet=det, j=i;
	}
	if (j<0)
		return Vec3(ZERO);
	Vec3 nbest; nbest[j] = maxdet;
	nbest[inc_mod3[j]] = -(C(inc_mod3[j],j)*C(dec_mod3[j],dec_mod3[j]) - C(dec_mod3[j],j)*C(inc_mod3[j],dec_mod3[j]));
	nbest[dec_mod3[j]] = -(C(inc_mod3[j],inc_mod3[j])*C(dec_mod3[j],j) - C(dec_mod3[j],inc_mod3[j])*C(inc_mod3[j],j));
	return nbest.normalized();
}

int crop_polygon_with_bound(const Vec2 *ptsrc,const int *idedgesrc,int nsrc, const Vec2 &sz,int icode,int iedge, Vec2 *ptdst,int *idedgedst);
float CroppedRectArea(const Vec2 &center, const Vec2 &dx,const Vec2 &dy, const Vec2 &bounds)
{
	Vec2 pt[16]={ center+dx+dy,center-dx+dy,center-dx-dy,center+dx-dy };
	int idummy[8],npt,i;
	for(i=0,npt=4;i<4;i++)
		npt = crop_polygon_with_bound(pt+(i&1)*8,idummy,npt,bounds,i,0,pt+(i&1^1)*8,idummy);
	float area=0;
	for(i=0,pt[npt]=pt[0];i<npt;i++)
		area += (pt[i].x-pt[i+1].x)*(pt[i].y+pt[i+1].y);
	return fabs_tpl(area)*0.5f;
}


int CTriMesh::Boxify(primitives::box *pboxes,int nMaxBoxes, const SBoxificationParams &params)
{
	float maxdot=cos_tpl(params.maxFaceTiltAngle), mindot=sqrt_tpl(1-maxdot*maxdot);
	box BBox; GetBBox(&BBox);
	float celldim=max(max(BBox.size.x,BBox.size.y),BBox.size.z)/params.voxResolution*2.001f, rcelldim=1.0f/celldim;
	float minPatchArea=params.minFaceArea,maxCurve=maxdot,maxPatchDist=params.distFilter,minLayerFilling=params.minLayerFilling,
		maxLayerReusage=params.maxLayerReusage,minMatchArea=0.8f,maxPatchFaceDist=max(celldim,maxPatchDist),maxIslandConnections=params.maxVoxIslandConnections;

	int i,j,ix,iy,iz,nboxes=0;
	float *areas = new float[m_nTris];
	for(i=0;i<m_nTris;i++)
		areas[i] = (m_pNormals[i]*(m_pVertices[m_pIndices[i*3+1]]-m_pVertices[m_pIndices[i*3]] ^ m_pVertices[m_pIndices[i*3+2]]-m_pVertices[m_pIndices[i*3]]))*0.5f;
	int *triarea = new int[m_nTris], *tri2patch;
	for(i=0;i<m_nTris;i++) triarea[i]=i;
	qsort(triarea,strided_pointer<float>(areas),0,m_nTris-1);
	memset(tri2patch = new int[m_nTris], 0, m_nTris*sizeof(int));
	ptitem2d *pt2d = new ptitem2d[m_nVertices];
	edgeitem *edges = new edgeitem[m_nVertices];
	int *vtxmask = new int[(m_nVertices-1>>5)+1];

	struct tripatch {
		quaternionf q;
		Vec3 sz,center;
		float area;
	} *patches = new tripatch[m_nTris];
	const int szQueue = 256;
	struct {
		int itri,itriBase,iedgeBase,bSmall;
		void set(int _itri,int _itriBase,int _iedgeBase,int _bSmall) { itri=_itri;itriBase=_itriBase;iedgeBase=_iedgeBase;bSmall=_bSmall; }
	}	queue[szQueue];
	int itriBig,itri,itri1,itail,ihead,nPatches=0,ivtxMin,ivtxMax;

	for(itriBig=m_nTris-1; itriBig>=0; itriBig--) {
		if (tri2patch[itri = triarea[itriBig]])
			continue;
		Matrix33 C,tmp; C.SetZero();
		tripatch curPatch; curPatch.area=0; curPatch.center.zero();
		Vec3 n = m_pNormals[itri];
		queue[0].set(itri,itri,0,0); ihead=0; itail=-1;
		memset(vtxmask,0,(m_nVertices-1>>5)*4+4);
		ivtxMin=m_nVertices-1; ivtxMax=0;

		do { // grow patches around large triangles
			itri = queue[itail = itail+1 & szQueue-1].itri;
			int bSmall = queue[itail].bSmall, bSmall1;
			if (!bSmall) {
				Vec3 varea = m_pVertices[m_pIndices[itri*3+1]]-m_pVertices[m_pIndices[itri*3]] ^ m_pVertices[m_pIndices[itri*3+2]]-m_pVertices[m_pIndices[itri*3]];
				C += Matrix33(IDENTITY)*varea.len2()-dotproduct_matrix(varea,varea,tmp);
				curPatch.area += areas[itri];
				curPatch.center += (m_pVertices[m_pIndices[itri*3]]+m_pVertices[m_pIndices[itri*3]+1]+m_pVertices[m_pIndices[itri*3]+2])*areas[itri];
			}
			int itriBase = queue[itail].itriBase & bSmall | itri & ~bSmall;
			int iedgeBase = queue[itail].iedgeBase & bSmall;
			for(i=0;i<3;i++) {
				vtxmask[(j=m_pIndices[itri*3+i])>>5] |= 1<<(m_pIndices[itri*3+i]&31);
				ivtxMin = min(ivtxMin,j); ivtxMax=max(ivtxMax,j);
			}
			for(i=0;i<3;i++) if ((itri1=m_pTopology[itri].ibuddy[i])>=0 && !tri2patch[itri1])	{
				j = iedgeBase | i&~bSmall;	bSmall1 = 0;
				if (m_pNormals[itri1]*n>maxdot || (bSmall1=EdgeDistSq(*this,itriBase*3+j,itriBase*3+inc_mod3[j],itri1*3+dec_mod3[GetEdgeByBuddy(itri1,itri)])<sqr(maxPatchDist)))
					queue[ihead=ihead+1&szQueue-1].set(itri1,itriBase,j,-bSmall1), tri2patch[itri1]=nPatches+1;
			}
		} while(ihead!=itail);

		Vec3 nbest = SVD3x3(C);
		if (nbest.len2()) {	// find the optimal 2d box for the patch
			curPatch.center /= curPatch.area*3.0f;
			n = nbest*sgnnz(nbest*n);
			Vec2_tpl<Vec3> axes; axes.x=n.GetOrthogonal().normalized(); axes.y=n^axes.x;
			for(j=0,i=ivtxMin;i<=ivtxMax;i++) if (vtxmask[i>>5] & 1<<(i&31))	{
				pt2d[j].pt.set(axes.x*m_pVertices[i],axes.y*m_pVertices[i]); pt2d[j++].iContact=i;
			}
			int nhull = qhull2d(pt2d,j,edges);
			for(i=0,curPatch.area=1e20f; i<nhull; i++) {
				axes.x = m_pVertices[edges[i].next->pvtx->iContact]-m_pVertices[edges[i].pvtx->iContact];
				(axes.x -= n*(n*axes.x)).normalize();
				quaternionf q = Quat(Matrix33::CreateFromVectors(axes.x,n^axes.x,n));
				Vec3 ptmin,ptmax; ptmin=ptmax = m_pVertices[edges[0].pvtx->iContact]*q;
				for(j=1;j<nhull;j++) {
					Vec3 pt = m_pVertices[edges[j].pvtx->iContact]*q;
					ptmin = min(ptmin,pt); ptmax = max(ptmax,pt);
				}
				float area = (ptmax.x-ptmin.x)*(ptmax.y-ptmin.y);
				if (area < curPatch.area)	{
					curPatch.area=area; curPatch.q=q; curPatch.sz=(ptmax-ptmin)*0.5f;
					curPatch.center = q*Vec3((ptmax.x+ptmin.x)*0.5f,(ptmax.y+ptmin.y)*0.5f,ptmax.z);
				}
			}
			if (curPatch.area > minPatchArea)
				patches[nPatches++] = curPatch;
		}
	}

	for(i=0;i<nPatches;i++) triarea[i]=i;
	qsort(triarea,strided_pointer<float>(&patches[0].area,sizeof(tripatch)),0,nPatches-1);
	voxgrid voxmain = Voxelize<true>(Quat(IDENTITY),celldim);
	for(i=nPatches-1;i>=0;i--) {
		if (patches[j = triarea[i]].area<0)
			continue;
		voxgrid voxpatch = Voxelize<true>(patches[j].q,celldim);
		int iz0,iz1,nReused,nFilled;
		Vec3 offs = voxpatch.q*Vec3(0.5f)+(voxpatch.center-voxmain.center)*rcelldim-Vec3(0.5f);
		vox_iterate(voxpatch,ix,iy,iz) { // copy 'used' markers from the main grid
			Vec3 pt = voxpatch.q*Vec3(ix,iy,iz)+offs;
			voxpatch(ix,iy,iz) |= voxmain(float2int(pt.x),float2int(pt.y),float2int(pt.z))&2;
		}
		Vec3 bbox[2]; bbox[0]=bbox[1] = (patches[j].center-voxpatch.center)*voxpatch.q; bbox[0]-=patches[j].sz; bbox[1]+=patches[j].sz;
		Vec2i ibbox[2] = { Vec2i(float2int(bbox[0].x*rcelldim-0.5f),float2int(bbox[0].y*rcelldim-0.5f)), Vec2i(float2int(bbox[1].x*rcelldim+0.5f),float2int(bbox[1].y*rcelldim+0.5f)) };
		for(iz=iz0=iz1=float2int(((patches[j].center-voxpatch.center)*voxpatch.q).z*rcelldim-0.5f); iz>=-voxpatch.sz.z; iz--) {
			for(ix=ibbox[0].x,nFilled=nReused=0;ix<ibbox[1].x;ix++) for(iy=ibbox[0].y;iy<ibbox[1].y;iy++)	{
				int &vox=voxpatch(ix,iy,iz); nFilled+=vox&1; nReused+=vox>>1; 
			}	// grow the patch's box 'down' while it has enough volume filled
			if (nFilled < (ibbox[1].x-ibbox[0].x)*(ibbox[1].y-ibbox[0].y)*minLayerFilling)
				break;
			iz1 += iz-iz1 & -isneg(nReused-nFilled*maxLayerReusage);
		}
		Vec3 dx=voxpatch.q*Vec3(0.5f,0,0), dy=voxpatch.q*Vec3(0,0.5f,0);
		for(ix=ibbox[0].x;ix<ibbox[1].x;ix++) for(iy=ibbox[0].y;iy<ibbox[1].y;iy++)	for(iz=iz0;iz>=iz1;iz--) {
			Vec3 pt = voxpatch.q*Vec3(ix,iy,iz)+offs;
			char val = (voxpatch(ix,iy,iz)&1)<<1;
			for(int ioffs=0;ioffs<4;ioffs++) { // apply the 'used' markers back to main (oversample x4 to avoid gaps due to aliasing)
				Vec3 pt1 = pt+dx*(ioffs&1)+dy*(ioffs>>1);
				voxmain(float2int(pt1.x),float2int(pt1.y),float2int(pt1.z)) |= val;
			}
		}
		voxmain(-1,0,0)=0; voxpatch.Free();
		Vec3_tpl<Vec3> axes(patches[j].q*Vec3(1,0,0),patches[j].q*Vec3(0,1,0),patches[j].q*Vec3(0,0,1));
		float z = (patches[j].center-voxpatch.center)*axes.z-iz1*celldim;
		for(ix=i-1;ix>=0;ix--) if (patches[iy=triarea[ix]].area>0) { // remove patches that coincide "enough" with the new box's faces
			Vec3_tpl<Vec3> axes1(patches[iy].q*Vec3(1,0,0),patches[iy].q*Vec3(0,1,0),patches[iy].q*Vec3(0,0,1));
			Vec3 dc = patches[iy].center-patches[j].center, axx,axy; Vec2 sz;
			float area,z1=-(dc*axes.z),zfix;
			if (axes1.z*axes.z<-maxdot && fabs_tpl(z1-z)<maxPatchFaceDist) {
				axx=axes.x; axy=axes.y; sz=Vec2(patches[j].sz);	zfix=2.0f;
			} else if (fabs_tpl(axes1.z*axes.z)<mindot) {
				Vec2 axdot(fabs_tpl(axes1.z*axes.x), fabs_tpl(axes1.z*axes.y));
				if (max(axdot.x,axdot.y)<maxdot)
					continue;
				int iax = isneg(axdot.y-axdot.x);
				axx=axes[iax]; axy=axes.z; sz=Vec2(patches[j].sz[iax],patches[j].sz.z); zfix=0;
			} else
				continue;
			area = CroppedRectArea(Vec2(dc*axx,dc*axy),Vec2(axes1.x*axx,axes1.x*axy)*patches[iy].sz.x,Vec2(axes1.y*axx,axes1.y*axy)*patches[iy].sz.y,sz);
			if (area > patches[iy].area*minMatchArea)
				patches[iy].area = -1;
			if (area*zfix > patches[j].area)
				z = max(celldim*0.5f,z1);
		}
		patches[j].area = -1;
		pboxes[nboxes].Basis = Matrix33(!patches[j].q);
		pboxes[nboxes].center = patches[j].center-axes.z*(z*0.5f);
		pboxes[nboxes].size = Vec3(patches[j].sz.x,patches[j].sz.y,z*0.5f);
		if (++nboxes>=nMaxBoxes)
			break;
	}

	Vec3i queueVox[256+(4096-256)*eBigEndian],ci;
	vox_iterate(voxmain,ix,iy,iz) if ((voxmain(ix,iy,iz)&7)==1) {
		ci=queueVox[0] = Vec3i(ix,iy,iz); ihead=0; itail=-1; j=0;
		do { // locate islands of 'unused' voxel; build semi-optimal boxes around them
			Vec3i ivox=queueVox[++itail], ivox1,d;
			for(d=Vec3(1); d.z<4; i=++d.x>>2,d.x-=3&-i,i=(d.y+=i)>>2,d.y-=3&-i,d.z+=i) {
				int &buddy = voxmain(ivox1=ivox+d-Vec3i(2)); j += iszero((buddy&7)-3);
				if ((buddy&7)==1)
					buddy|=4, queueVox[ihead=min(ihead+1,(int)(CRY_ARRAY_COUNT(queueVox))-1)]=ivox1, ci+=ivox1;
			}
		} while(ihead!=itail);
		if (j>(itail+1)*maxIslandConnections)
			continue;
		Vec3 c = Vec3(ci)/(ihead+1);
		Matrix33 C,tmp; C.SetZero();
		for(i=0;i<=ihead;i++) {
			Vec3 pt = Vec3(queueVox[i])-c;
			C += Matrix33(IDENTITY)*pt.len2()-dotproduct_matrix(pt,pt,tmp);
		}
		Vec3 axis = SVD3x3(C), ptmin,ptmax,axx;
		if (!axis.len2()) axis = Vec3(0,0,1);
		i = isneg(fabs_tpl(axis.y)-fabs_tpl(axis.x));
		axx = Vec3(i^1,i,0); (axx -= axis*(axx*axis)).normalize();
		quaternionf q = !Quat(Matrix33::CreateFromVectors(axx,axis^axx,axis));
		ptmin=ptmax = q*(Vec3(queueVox[0])-c);
		for(i=1;i<=ihead;i++) {
			Vec3 pt = q*(Vec3(queueVox[i])-c);
			ptmin = min(ptmin,pt); ptmax = max(ptmax,pt);
		}
		if (nboxes>=nMaxBoxes)
			goto boxnomore;
		pboxes[nboxes].Basis = Matrix33(q);
		pboxes[nboxes].center = voxmain.center+(((ptmax+ptmin+Vec3(1))*0.5f)*q+c)*celldim;
		pboxes[nboxes++].size = (ptmax-ptmin+Vec3(1))*0.5f*celldim;
	}
	boxnomore:
	voxmain.Free();	delete[] vtxmask; delete[] edges; delete[] pt2d;
	delete[] tri2patch; delete[] triarea; delete[] areas;

	return nboxes;
}

static int ineighb[27*4] = {-1};
static Vec3i idx2dir[27],idx2dir1[19];
static int dirIsAxis;
static float inv[27] = {0};

template<class itype> inline int getNextBit(itype& mask)
{
	if (!mask)
		return -1;
	int bit = ilog2((mask^mask-1)+1)-1;
	mask &= ~((itype)1<<bit);
	return bit;
}

inline int check_islands(unsigned int mask, Vec4i *centers=0)
{
	int queue[32],ihead,itail,nislands=0;
	for(; mask; nislands++) {
		for(queue[0]=getNextBit(mask),itail=0,ihead=1; itail!=ihead; )
			for(int idx=queue[itail++],i=0,j;i<4;i++) if (mask & 1<<(j=ineighb[idx*4+i]))
				mask &= ~(1<<(queue[ihead++]=j));
		if (centers) {
			Vec3i c(0); for(int i=0;i<ihead;i++) c+=idx2dir[queue[i]];
			centers[nislands] = Vec4i(c,ihead);
		}
	}
	return nislands;
}

inline int packdir(const Vec3i& idir) { return (idir.z&31)<<10 | (idir.y&31)<<5 | (idir.x&31); }
inline Vec3i unpackdir(int i,int bit) { i=(unsigned int)i>>bit & ~(bit>>31); return Vec3i((i<<32-5)>>32-5, (i<<32-10)>>32-5, (i<<32-15)>>32-5); }

struct SNbList {
	int mask;
	int count,count1;
};
template<bool rngchk> inline SNbList packnb(CTriMesh::voxgrid_tpl<rngchk>& vox, const Vec3i& ic,int ibit,int ibitDir) 
{ 
	SNbList res; res.mask=res.count=res.count1=0;
	Vec3i dir0 = unpackdir(vox(ic),ibitDir);
	for(int i=0,j,j0,v; i<27; res.count+=j, res.count1+=j0 & dirIsAxis>>i++) {
		Vec3i dir1 = unpackdir(v=vox(ic+idx2dir[i]),ibitDir);
		res.mask |= (j = (j0=v>>ibit & 1) & ~(dir0*dir1>>31))<<i;
	}
	return res;
}
template<bool rngchk> inline int packnb(CTriMesh::voxgrid_tpl<rngchk>& vox, const Vec3i& ic,int ibit) 
{ 
	int mask=0; 
	for(int i=0;i<27;i++)
		mask |= (vox(ic+idx2dir[i])>>ibit & 1)<<i;
	return mask;
}

inline void count_neighbors(CTriMesh::voxgrid_tpl<false>& vox, int bitSrc, int bitDst, int auxAND=-1)
{	// count neighbors in principal directions using running sums
	Vec3i ic,dc;
	auxAND &= ~(7<<bitDst);
	for(int ix=0,mask=1; ix<3; ix++,mask=7,bitSrc=bitDst) {
		int iy=inc_mod3[ix], iz=dec_mod3[ix], sum,v0,v1,auxAdd=ix>>1;
		for(ic[iz]=-vox.sz[iz]+1;ic[iz]<vox.sz[iz]-1;ic[iz]++) for(ic[iy]=-vox.sz[iy]+1;ic[iy]<vox.sz[iy]-1;ic[iy]++)
			for(ic[ix]=-vox.sz[ix]+1,dc.zero()[ix]=1,v0=0,sum=vox(ic)>>bitSrc&mask; ic[ix]<vox.sz[ix]-1; ic[ix]++,v0=v1) {
				v1 = vox(ic)>>bitSrc & mask; 
				sum += vox(ic+dc)-v0;
				(vox(ic) &= auxAND) |= sum+auxAdd<<bitDst;
			}
	}
}

inline int dilatable(CTriMesh::voxgrid_tpl<false>& vox, const Vec3i& ic)
{ 
	if ((vox(ic) & 2+8)!=2)
		return 0;
	SNbList nb = packnb(vox,ic,1,4);
	if (!inrange(nb.count-1,3,21))	// has 4..20 neighbors
		return 0;
	int res = 
		check_islands(nb.mask&~(1<<13))==1 && // all neighbors are in one island if the center is removed
		check_islands(nb.mask^(1<<27)-1)==1;	 // all free neighbors are in one island if the center stays
	vox(ic) |= (res^1)<<3;
	return res;
}

inline int is_line(const SNbList& nb, int maxIsleSz, Vec3i *idir=0)
{
	static Vec4i isle[19];
	int state0=nb.mask>>13&1, count=nb.count-state0, mask=nb.mask&~(1<<13);
	if (count<2)
		return 0;
	else if (count==2) {
		int i=getNextBit(mask), j=ilog2(mask);
		if (idir)	{
			idir[0]=idx2dir[i]; idir[1]=idx2dir[j];
		}
		return isneg(idx2dir[i]*idx2dir[j]);
	} else if (check_islands(mask,isle)==2 && max(max(Vec3i(isle[0])*Vec3i(isle[1]),isle[0].w-maxIsleSz-1),isle[1].w-maxIsleSz)<0) {
		if (idir) for(int i=0;i<2;i++) 
			idir[i] = float2int(Vec3(Vec3i(isle[i])).GetNormalizedFast());
		return 1;
	}
	return 0;
}

inline Vec3i trace_line(CTriMesh::voxgrid_tpl<false>& vox, const Vec3i& ic, Vec3i indir, int ibit)
{
	int imax = 13;
	for(int i=0;i<27;i++) if (idx2dir[i]*indir>idx2dir[imax]*indir && vox(ic+idx2dir[i]) & 1<<ibit)
		imax = i;
	return imax!=13 ? idx2dir[imax] : Vec3i(0);
}

inline bool is_surface(CTriMesh::voxgrid_tpl<false>& vox, const Vec3i& ic)
{
	int ncnt=0;
	for(int i=0;i<6;i++) { // count filled face neighbors
		Vec3 ic1=ic; ic1[i>>1] += (i<<1&2)-1;
		ncnt += vox(ic1)&1;
	}
	return ncnt<6;
}

template<class t_prim> inline int cell_inside(CTriMesh::voxgrid_tpl<false>&, const t_prim&, const Vec3i&);
template<> inline int cell_inside(CTriMesh::voxgrid_tpl<false> &vox, const box& box, const Vec3i& ipt)
{
	Vec3 ptloc = (box.Basis*((Vec3(ipt)+Vec3(0.5f))*vox.celldim-box.center)).abs()-box.size;
	return isneg(max(max(ptloc.x,ptloc.y),ptloc.z));	
}
template<> inline int cell_inside(CTriMesh::voxgrid_tpl<false> &vox, const cylinder& cyl, const Vec3i& ipt)
{
	Vec3 ptloc = (Vec3(ipt)+Vec3(0.5f))*vox.celldim-cyl.center;
	return isneg(max(fabs(ptloc*cyl.axis)-cyl.hh, (ptloc-cyl.axis*(cyl.axis*ptloc)).len2()-sqr(cyl.r)));
}
template<> inline int cell_inside(CTriMesh::voxgrid_tpl<false> &vox, const capsule& caps, const Vec3i& ipt)
{
	Vec3 ptloc = (Vec3(ipt)+Vec3(0.5f))*vox.celldim-caps.center; 
	float h = caps.axis*ptloc; ptloc -= caps.axis*min(caps.hh,max(-caps.hh,h));
	return isneg(ptloc.len2()-sqr(caps.r));
}

#define voxiter_BBox for(ipt.x=BBox[0].x;ipt.x<=BBox[1].x;ipt.x++) for(ipt.y=BBox[0].y;ipt.y<=BBox[1].y;ipt.y++) for(ipt.z=BBox[0].z;ipt.z<=BBox[1].z;ipt.z++)
template<class t_prim> void RemovePrim(CTriMesh::voxgrid_tpl<false>& vox, Vec3i* BBox, const t_prim& prim, float fillThresh=0.5f)
{
	Vec3i ipt;
	for(int ipass=0;ipass<3;ipass++)
		for(int iz=0; iz<3; iz++)	{	// trace lines in xyz; blend inside/outside state along the length; average between xyz, fill if >0.5
			int ix=inc_mod3[iz], iy=dec_mod3[iz];
			for(ipt[ix]=BBox[0][ix];ipt[ix]<=BBox[1][ix];ipt[ix]++) for(ipt[iy]=BBox[0][iy];ipt[iy]<=BBox[1][iy];ipt[iy]++) {
				int bounds[2] = { 1<<29 }; float val[3];	
				for(ipt[iz]=BBox[0][iz];ipt[iz]<=BBox[1][iz];ipt[iz]++) {
					int i = cell_inside(vox,prim,ipt); 
					bounds[0] += ipt[iz]-bounds[0] & (1<<29)-1-bounds[0]>>31 & -i; 
					bounds[1] += ipt[iz]-bounds[1] & -i;
				}
				if (bounds[0]==1<<29)
					continue;
				if (!ipass) {
					for(int i=0;i<2;i++) { ipt[iz]=bounds[i]; val[i]=(vox(ipt)&1 & isneg(fabs_tpl(bounds[i])-(vox.sz[iz]-2-(bounds[i]>>31))))*1024; }
					float dval = (val[1]-val[0])/max(1,bounds[1]-bounds[0]);
					for(ipt[iz]=bounds[0];ipt[iz]<=bounds[1];ipt[iz]++,val[0]+=dval)
						vox(ipt) += float2int(val[0])<<19;
				} else if (bounds[1]>bounds[0]+1)	{
					float sum,k=0.5f; 
					for(ipt[iz]=bounds[0],sum=vox(ipt)>>19,val[0]=val[1]=0; ipt[iz]<bounds[1]; ipt[iz]++, val[0]=val[1],val[1]=val[2],k=1.0f/3) {
						val[2] = vox(ipt)>>19;
						(vox(ipt) &= ~0u>>13) |= float2int((sum += (vox(ipt+ort[iz])>>19)-val[0])*k)<<19;
					}
					(vox(ipt) &= ~0u>>13) |= float2int((sum-val[0])*(1.0f/2))<<19;
				}
			}
		}	
	int thresh = float2int(1024*3*fillThresh);
	voxiter_BBox if (cell_inside(vox,prim,ipt)) { 
		int& v=vox(ipt), fill=isneg(thresh-(v>>19)); 
		v = fill | v & ~(1|-1<<19); 
	}	
}

Vec2i CheckPrim(CTriMesh::voxgrid_tpl<false>& vox, int itype, const primitive *prim, float &V, float inflate=0, float fillThresh=-1)
{
	Vec3i BBox[2] = { -vox.sz+Vec3i(1), vox.sz-Vec3i(2) }, ipt;
	Vec2i ncells(0);

	switch (itype) {
		case box::type:	{
			box box = *(primitives::box*)prim; 
			box.Basis *= Matrix33(vox.q); box.center = (box.center-vox.center)*vox.q;
			V = box.size.GetVolume()*8;
			box.size += Vec3(inflate);
			Matrix33 mtx = box.Basis; 
			Vec3 sz = box.size*mtx.Fabs();
			BBox[0] = max(BBox[0], float2int((box.center-sz)*vox.rcelldim));
			BBox[1] = min(BBox[1], float2int((box.center+sz)*vox.rcelldim));
			if (fillThresh<0)	voxiter_BBox { 
				int i = cell_inside(vox,box,ipt); ncells += Vec2i(i & vox(ipt) & 1,i);	
			}	else
				RemovePrim(vox,BBox,box,fillThresh);
		}	break;

		case cylinder::type: {
			cylinder cyl = *(cylinder*)prim; 
			cyl.axis = cyl.axis*vox.q; cyl.center = (cyl.center-vox.center)*vox.q;
			V = gf_PI*sqr(cyl.r)*2*cyl.hh;
			cyl.r += inflate; cyl.hh += inflate;
			for(int i=0;i<3;i++) {
				Vec3 ax = cyl.axis*cyl.axis[i]; ax[i]-=1; ax.normalize()*=cyl.r;
				BBox[0][i] = max(BBox[0][i], float2int((cyl.center[i]-fabs(ax[i])-fabs(cyl.axis[i])*cyl.hh)*vox.rcelldim));
				BBox[1][i] = min(BBox[1][i], float2int((cyl.center[i]+fabs(ax[i])+fabs(cyl.axis[i])*cyl.hh)*vox.rcelldim));
			}
			if (fillThresh<0) voxiter_BBox { 
				int i = cell_inside(vox,cyl,ipt); ncells += Vec2i(i & vox(ipt) & 1,i);	
			}	else
				RemovePrim(vox,BBox,cyl,fillThresh);
		}	break;

		case capsule::type: {
			capsule caps = *(capsule*)prim;	
			caps.axis = caps.axis*vox.q; caps.center = (caps.center-vox.center)*vox.q;
			V = gf_PI*sqr(caps.r)*2*caps.hh+4.0f/3*gf_PI*cube(caps.r);
			caps.r += inflate; caps.hh += inflate;
			BBox[0] = max(BBox[0], float2int((caps.center-Vec3(caps.r)-caps.axis.abs()*caps.hh)*vox.rcelldim));
			BBox[1] = min(BBox[1], float2int((caps.center+Vec3(caps.r)+caps.axis.abs()*caps.hh)*vox.rcelldim));
			if (fillThresh<0) voxiter_BBox { 
				int i = cell_inside(vox,caps,ipt); ncells += Vec2i(i & vox(ipt) & 1,i);	
			}	else
				RemovePrim(vox,BBox,caps,fillThresh);
		}	break;
	}
	return ncells;
}

struct SVoxLine {
	Vec3 ends[2];
	int closed;
};
struct SVoxSurf {
	Vec3 center;
	Vec3 norm;
	int count;
	float rating;
	box sbox;
	cylinder cyl;
};

inline bool MergeLine(SVoxLine& ldst, const SVoxLine& lsrc)
{
	Vec3 org[]={ ldst.ends[0],lsrc.ends[0] }, dir[]={ ldst.ends[1]-ldst.ends[0], lsrc.ends[1]-lsrc.ends[0] };
	int len[]={ dir[0].len2(), dir[1].len2() }, i=isneg(len[0]-len[1]), closed[]={ ldst.closed,lsrc.closed };
	if (sqr(dir[0]*dir[1])>len[0]*len[1]*sqr(0.95f)	&& // almost parallel
			fabs_tpl((org[0]*2+dir[0]-org[1]*2-dir[1])*dir[i]) <= len[i]+fabs_tpl(dir[0]*dir[1]) && // ends overlap along the longest dir
			((org[i^1]-org[i]^dir[i]).len2() <= len[i]*4 || (org[i^1]+dir[i^1]-org[i]^dir[i]).len2() <= len[i]*4)) // distance is <=2 voxels
	{
		Vec3 pt[] = { org[i],org[i^1],org[i^1]+dir[i^1] }, ptlen = Vec3(0,(pt[1]-org[i])*dir[i],(pt[2]-org[i])*dir[i]);
		ldst.ends[0] = pt[idxmin3(ptlen)];
		pt[0] += dir[i]; ptlen[0] = len[i];
		ldst.ends[1] = pt[idxmax3(ptlen)];
		for(int iendDst=ldst.closed=0;iendDst<2;iendDst++) for(int isrc=0;isrc<2;isrc++) for(int iendSrc=0;iendSrc<2;iendSrc++) 
			ldst.closed |= iszero((org[isrc]+dir[isrc]*iendSrc-ldst.ends[iendDst]).len2())<<iendDst & closed[isrc];
		return true;
	}
	return false;
}

#include "cylindergeom.h"
#include "capsulegeom.h"

int CTriMesh::Proxify(IGeometry **&pOutGeoms, SProxifyParams *pparams)
{
	SProxifyParams params;
	if (pparams) params = *pparams;
	int isle,nTris,nIslands, i,j;
	Vec3i ic;
	index_t *pTris=0;
	int *nbcnt=0,*nbidx=0,nbcntAlloc=0,nbidxAlloc=0,nGeoms=0;
	Vec3r *norm[2] = {0,0};
	Vec3 *vtx=0, *pNormals=0;
	int *pVtxMap = params.convexHull ? new int[m_nVertices+(m_nVertices>>5)+1] : 0;
	const int MAXPRIMS = 256;
	SVoxLine voxline[MAXPRIMS];
	SVoxSurf voxsurf[MAXPRIMS];
	CGeometry **pGeoms = new CGeometry*[params.maxGeoms];
	CRayGeom gray;

	if (ineighb[0]<0)	{
		for(ic.z=0,dirIsAxis=0;ic.z<3;ic.z++) for(ic.y=0;ic.y<3;ic.y++) for(ic.x=0;ic.x<3;ic.x++) {
			for(Vec3i ic1(j=0);ic1.z<3;ic1.z++) for(ic1.y=0;ic1.y<3;ic1.y++) for(ic1.x=0;ic1.x<3;ic1.x++)
				if ((ic-ic1).len2()==1 && ic1.len2()!=3) 
					ineighb[ic*Vec3i(1,3,9)*4+j++] = ic1*Vec3i(1,3,9); 
			if (j<4)
				ineighb[ic*Vec3i(1,3,9)*4+j] = ineighb[ic*Vec3i(1,3,9)*4];
			idx2dir[ic*Vec3i(1,3,9)] = ic-Vec3i(1);
			dirIsAxis |= iszero((ic-Vec3i(1)).len2()-1) << ic*Vec3i(1,3,9);
		}
		for(i=j=0;i<27;j+=inrange(idx2dir[i++].len2(),0,3))	idx2dir1[j]=idx2dir[i];
		for(i=1;i<27;i++) inv[i] = 1.0f/i;
	}

	auto GetIsland = [this,&pTris,&pNormals](int isle,int nTris=0) 
	{
		for(int i=m_pIslands[isle].itri; i!=0x7fff; i=m_pTri2Island[i].inext) {
			((Vec3_tpl<index_t>*)pTris)[nTris] = ((Vec3_tpl<index_t>*)m_pIndices)[i];
			pNormals[nTris++] = m_pNormals[i];
		}
		return nTris;
	};
	auto qhullIslands = [this,&pTris,&pNormals,pVtxMap](uint64 islandMap)
	{
		memset(pVtxMap+m_nVertices,0,((m_nVertices>>5)+1)*sizeof(int));
		int imin=m_nVertices,imax=-1,i,j;
		for(int isle=getNextBit(islandMap); isle>=0; isle=getNextBit(islandMap)) 
			for(i=m_pIslands[isle].itri; i!=0x7fff; i=m_pTri2Island[i].inext) for(j=0;j<3;j++) {
				int ivtx = m_pIndices[i*3+j];
				pVtxMap[m_nVertices+(ivtx>>5)] |= 1<<(ivtx&31);
				imin=min(imin,ivtx); imax=max(imax,ivtx);
			}
		for(i=imin,j=0; i<=imax; i++) if (pVtxMap[m_nVertices+(i>>5)] & 1<<(i&31)) {
			pVtxMap[j]=i;	swap(m_pVertices[i], m_pVertices[j++]);
		}
		delete[] pTris;	pTris=nullptr;
		int nTris = qhull(m_pVertices,j,pTris);
		if (nTris>m_nTris || !pNormals) {
			delete pNormals; pNormals = new Vec3[max(nTris,m_nTris)];
		}
		for(i=0;i<nTris;i++)
			pNormals[i] = (m_pVertices[pTris[i*3+1]]-m_pVertices[pTris[i*3]] ^ m_pVertices[pTris[i*3+2]]-m_pVertices[pTris[i*3]]).normalized();
		for(i=imax; i>=imin; i--) if (pVtxMap[m_nVertices+(i>>5)] & 1<<(i&31))
			swap(m_pVertices[i],m_pVertices[--j]);
		for(i=0;i<nTris*3;i++) pTris[i]=pVtxMap[pTris[i]];
		return nTris;
	};

	if (params.reuseVox & params.flipCurCell) {
		if (!m_voxgrid || m_voxgrid->lastHit.x>=m_voxgrid->sz.x)
			return 0;
		for(i=0;i<m_voxgrid->nflips && (m_voxgrid->flipHist[i]-m_voxgrid->lastHit).len2();i++);
		if (i<m_voxgrid->nflips)
			memmove(m_voxgrid->flipHist+i,m_voxgrid->flipHist+i+1,(--m_voxgrid->nflips-i)*sizeof(Vec3i));
		else {
			if (!(m_voxgrid->nflips & 15))
				ReallocateList(m_voxgrid->flipHist,m_voxgrid->nflips,m_voxgrid->nflips+16);
			m_voxgrid->flipHist[m_voxgrid->nflips++] = m_voxgrid->lastHit;
		}
	}

	nIslands = BuildIslandMap();
	params.islandMap &= (1ull<<nIslands)-1;

	if (params.closeHoles) {
		char *pHoleEdges = new char[m_nVertices*3];
		int *pHoleVtx = new int[m_nVertices], *pHoleTris = new int[m_nVertices*6];
		int nTrisAlloc=m_nTris, nTris00=m_nTris;
		Vec2 *pHoleVtx2d = new Vec2[m_nVertices];
		uint64 mask = params.islandMap;
		memset(pHoleEdges, 0, m_nVertices*3);

		for(isle=getNextBit(mask); isle>=0; isle=getNextBit(mask)) {
			int nTris0=m_nTris, itriLast;
			for(i=m_pIslands[isle].itri; i!=0x7fff; i=m_pTri2Island[i].inext)
				for(int ivtx=(itriLast=i,0);ivtx<3;ivtx++) if (m_pTopology[i].ibuddy[ivtx]<0) {
					Vec2i mark(-1,0); int count=10,nHoleVtx=0;
					for(int iedge=ivtx,itri=i,itri1;(itri-mark.x|iedge-mark.y);) if ((itri1=m_pTopology[itri].ibuddy[inc_mod3[iedge]])>=0)	{
						iedge=GetEdgeByBuddy(itri1,itri); itri=itri1;
						mark -= (Vec2i(itri,iedge)-mark)*(--count>>31);	count&=63;
					}	else {
						if (pHoleEdges[itri] & 1<<(iedge=inc_mod3[iedge]))
							break;
						pHoleEdges[itri] |= 1<<iedge;
						pHoleVtx[nHoleVtx++] = m_pIndices[itri*3+iedge];
						mark.set(-1,0);	count=10;
					}
					if (!nHoleVtx)
						continue;
					Vec3 c(0),ntest(0); float brdlen=0;
					for(int j1=j=0;j<nHoleVtx;j++) { 
						float seglen = (m_pVertices[pHoleVtx[j1=j+1&j+1-nHoleVtx>>31]]-m_pVertices[pHoleVtx[j]]).len();
						c += (m_pVertices[pHoleVtx[j1]]+m_pVertices[pHoleVtx[j]])*seglen; brdlen+=seglen;
					}	
					c /= brdlen*2;
					Matrix33 C(ZERO),Ctmp,Ctmp1;
					for(j=0;j<nHoleVtx;j++) {
						Vec3 p0 = m_pVertices[pHoleVtx[j]]-c, p1 = m_pVertices[pHoleVtx[j+1&j+1-nHoleVtx>>31]]-c;
						float seglen = (p1-p0).len(); ntest += p0^p1;
						(dotproduct_matrix(p0,p0,Ctmp) += dotproduct_matrix(p1,p1,Ctmp1))*=2;
						Ctmp+=dotproduct_matrix(p0,p1,Ctmp1);	C+=(Ctmp+dotproduct_matrix(p1,p0,Ctmp1))*seglen;
					}
					Vec3 n = SVD3x3(C); n *= sgnnz(ntest*n);
					Vec3 axisx=n.GetOrthogonal().normalized(), axisy=n^axisx;
					for(j=0;j<nHoleVtx;j++)
						pHoleVtx2d[j].set(axisx*(m_pVertices[pHoleVtx[j]]-c), axisy*(m_pVertices[pHoleVtx[j]]-c));
					MARK_UNUSED pHoleVtx2d[nHoleVtx].x;
					int nHoleTris = TriangulatePoly(pHoleVtx2d,nHoleVtx+1,pHoleTris,m_nVertices*6);
					if (m_nTris+nHoleTris>nTrisAlloc) {
						ReallocateList(m_pIndices,m_nTris*3,(nTrisAlloc=max(m_nTris+nHoleTris,nTrisAlloc+256))*3,false,!(m_flags & mesh_shared_idx));
						ReallocateList(m_pNormals,m_nTris,nTrisAlloc);
						if (m_pIds) ReallocateList(m_pIds,m_nTris,nTrisAlloc);
						ReallocateList(m_pTri2Island,m_nTris,nTrisAlloc,true);
						ReallocateList(m_pTopology,m_nTris,nTrisAlloc);
					}
					for(j=0;j<nHoleTris;j++) {
						for(int ivtx=0;ivtx<3;ivtx++) m_pIndices[(m_nTris+j)*3+ivtx] = pHoleVtx[pHoleTris[j*3+2-ivtx]];
						m_pTri2Island[m_nTris+j].iprev=isle; m_pTri2Island[m_nTris+j].inext=m_nTris+j+1;
						RecalcTriNormal(m_nTris+j);
					}
					m_nTris += nHoleTris;
				}
			if (m_nTris>nTris0) {
				m_pTri2Island[itriLast].inext = nTris0; 
				m_pTri2Island[m_nTris-1].inext = 0x7fff;
			}
		}
		delete[] pHoleVtx2d; delete[] pHoleTris; 
		delete[] pHoleVtx; delete[] pHoleEdges;

		if (m_nTris>nTris00) {
			if (m_pTree->GetType()==BVT_OBB) {
				COBBTree &bvt = *(COBBTree*)m_pTree;
				OBBnode *nodes = bvt.m_pNodes+bvt.m_pNodes[0].ichild;
				if (bvt.m_pNodes[0].ntris || nodes[1].ichild+nodes[1].ntris!=nTris00) {
					if (bvt.m_nNodesAlloc<bvt.m_nNodes+2)
						ReallocateList(bvt.m_pNodes, bvt.m_nNodes,bvt.m_nNodesAlloc=bvt.m_nNodes+2);
					nodes = bvt.m_pNodes+bvt.m_nNodes;
					nodes[0] = bvt.m_pNodes[0]; nodes[1] = bvt.m_pNodes[0];
					nodes[0].iparent = nodes[1].iparent = 0;
					bvt.m_pNodes[0].ichild = bvt.m_nNodes;
					nodes[1].ichild = nTris00;
					bvt.m_nNodes += 2;
				}
				nodes[1].ntris = m_nTris-nTris00;
				Matrix33& Basis	 = *(Matrix33*)nodes[1].axes;
				for(i=nodes[1].ichild*3,nodes[1].center.zero(); i<(nodes[1].ichild+nodes[1].ntris)*3; i++)
					nodes[1].center += m_pVertices[m_pIndices[i]];
				for(i=nodes[1].ichild*3,nodes[1].size.zero(),nodes[1].center/=nodes[1].ntris*3; i<(nodes[1].ichild+nodes[1].ntris)*3; i++)
					nodes[1].size = max(nodes[1].size, (Basis*(m_pVertices[m_pIndices[i]]-nodes[1].center)).abs());
			}
			CalculateTopology(m_pIndices);
		}
	}

	if (params.mergeIslands) {
		if ((params.islandMap & (1ull<<nIslands)-1)==(1<<nIslands)-1 && !params.convexHull) {
			pTris=m_pIndices;	pNormals=m_pNormals; nTris=m_nTris; 
		} else if (!params.convexHull) {
			pTris = new index_t[m_nTris*3];	pNormals = new Vec3[m_nTris];
			for(i=getNextBit(params.islandMap),nTris=0; i>=0; i=getNextBit(params.islandMap))
				nTris = GetIsland(i,nTris);
		} else
			nTris = qhullIslands(params.islandMap);
		isle=0; params.islandMap=0; goto GotIslands;
	} else {
		pTris = new index_t[m_nTris*3];	pNormals = new Vec3[m_nTris];
	}

	for(isle=getNextBit(params.islandMap); isle>=0 && nGeoms<params.maxGeoms; isle=getNextBit(params.islandMap)) {
		nTris = params.convexHull ? qhullIslands(1ull<<isle) : GetIsland(isle);
		GotIslands:
		// voxelize with global eigenmatrix
		Vec3r axes[3],center;	ComputeMeshEigenBasis(m_pVertices,strided_pointer<index_t>(pTris),nTris,axes,center);
		Quat qgrid = is_unused(params.qForced) ? Quat(Matrix33(axes[0],axes[1],axes[2])) : params.qForced;
		Vec3 BBox[2]; BBox[0]=BBox[1] = m_pVertices[pTris[0]]*qgrid;
		for(i=1; i<nTris*3; i++) {
			Vec3 v=m_pVertices[pTris[i]]*qgrid;
			BBox[0]=min(BBox[0],v); BBox[1]=max(BBox[1],v);
		}
		Vec3 sz = BBox[1]-BBox[0];
		if (params.forceBBox) {
			box bbox;	bbox.Basis = Matrix33(!qgrid);
			bbox.size = sz*0.5f+Vec3(params.inflatePrims); 
			bbox.center = qgrid*(BBox[0]+BBox[1])*0.5f;
			pGeoms[nGeoms++] = (new CBoxGeom)->CreateBox(&bbox); 
			continue;
		}
		float celldim = max(max(sz.x,sz.y),sz.z)*1.001f/params.ncells;
		voxgrid_tpl<false> vox;
		if (params.reuseVox)
			memcpy(&vox, m_voxgrid, sizeof(vox));
		else 
			vox = Voxelize<false>(qgrid,celldim,pTris,nTris,1);
		vox.persistent = false;
		sz = Vec3(vox.sz)*vox.celldim;

		Vec3i *queue = new Vec3i[vox.sz.GetVolume()*8+1];
		int ihead=0,itail;
		auto build_surface = [&](int bit,int bitIdx,int bitDir,bool useCorners) 
		{
			Vec3i *pidx2dir; int ndirs,j0,has0;
			if (useCorners)
				pidx2dir=idx2dir,ndirs=27,j0=13,has0=1;
			else
				pidx2dir=idx2dir1,ndirs=18,j0=32,has0=0;
			if (nbcntAlloc < ihead+1)	{
				ReallocateList(nbcnt,0,nbcntAlloc=ihead+1);
				for(j=0;j<2;j++) ReallocateList(norm[j],0,nbcntAlloc);
				ReallocateList(vtx,0,nbcntAlloc);
			}
			for(i=0;i<ihead;i++) {
				Vec3i dir0 = unpackdir(vox(queue[i]),bitDir); uint v;
				for(j=0,nbcnt[i]=-has0;j<ndirs;j++)
					v=vox(queue[i]+pidx2dir[j]), nbcnt[i] += v>>bit & 1 & ~(dir0*unpackdir(v,bitDir)>>31);
			}
			for(i=0,nbcnt[ihead]=0;i<ihead;i++) nbcnt[i+1]+=nbcnt[i];
			if (nbidxAlloc < nbcnt[ihead])
				ReallocateList(nbidx,0,nbidxAlloc=nbcnt[ihead]);
			for(i=0;i<ihead;i++) {
				Vec3i dir0 = unpackdir(vox(queue[i]),bitDir); uint v;
				for(j=0;j<ndirs;j++) if ((v=vox(queue[i]+pidx2dir[j]))>>bit & ~iszero(j-j0) & 1 && unpackdir(v,bitDir)*dir0>=0)
					nbidx[--nbcnt[i]] = v>>bitIdx;
			}
		};

		if (params.findPrimLines | params.findPrimSurfaces) {
			// dilate the grid until only skeletons are left, store results in bit 1
			vox_iterate1(vox,ic) vox(ic)|=(vox(ic)&1)<<1; 
			vox_iterate1(vox,ic) if (dilatable(vox,ic))
				queue[ihead++] = ic;
			for(i=ihead;i>0;i--) vox(queue[i-1])&=1;
			for(;i<ihead;i++)	if (is_line(packnb(vox,queue[i],1,4),3)) {
				vox(queue[i])|=2; queue[i--]=queue[--ihead];
			}
 			for(int ilayer=itail=0,ihead1; itail<ihead; ilayer++) {
				for(ihead1=ihead;itail<ihead;) for(j=0,ic=queue[itail++];j<27;j++) if ((vox(ic+idx2dir[j])&6)==2)
					vox(queue[ihead1++]=ic+idx2dir[j]) |= 4;
				for(i=ihead;i<ihead1;i++) {
					Vec3i dir(0); for(j=0,ic=queue[i];j<27;j++) // accumulate from-outside dir for the potential next layer
						dir += idx2dir[j]*(vox(ic+idx2dir[j])>>1&1^1);
					(vox(ic) &= 3+8) |= packdir(dir)<<4;
				}
				for(i=j=ihead;i<ihead1;i++) if (dilatable(vox,queue[i]))
					queue[ihead++] = queue[i];
				for(i=ihead;i>j;i--) vox(queue[i-1])&=~2;
				for(;i<ihead;i++) if (is_line(packnb(vox,queue[i],1,4),3)) {
					vox(queue[i])|=2; queue[i--]=queue[--ihead];
				}
			}
			vox_iterate1(vox,ic) vox(ic)&=~(8|4);
		}

		int nline=0,nsurf=0;
		SNbList nb;
		if (params.findPrimSurfaces) vox_iterate1(vox,ic) 
			if ((vox(ic)&(2|8))==2 && inrange((nb=packnb(vox,ic,1,4)).count,4,21) && nb.count1<6 && check_islands(nb.mask^(1<<27)-1)==2 && nsurf<MAXPRIMS) {	// find surface skeletons
				for(queue[itail=0]=ic,ihead=1,vox(ic)|=8|4; itail!=ihead; )
					for(Vec3i ic1=queue[i=0,itail++]; i<27; i++) // trace voxels with 4..12 neighbors and 2 islands of free neighbors
						if ((vox(ic1+idx2dir[i])&(2|4))==2 && inrange((nb=packnb(vox,ic1+idx2dir[i],1,4)).count,4,21) && nb.count1<6 && check_islands(nb.mask^(1<<27)-1)==2)
							(vox(queue[ihead] = ic1+idx2dir[i]) &= ~0u>>13) |= 8|4|ihead<<19, ihead = min((1<<13)-1,ihead+1);
				int clearMask = 4;
				if (ihead >= params.minSurfCells) {
					Vec3 c(ZERO),Ibody; Matrix33 I(ZERO),Basis;
					for(i=0; i<ihead; c+=queue[i++]);
					for(i=0,c/=ihead; i<ihead; i++) {
						Matrix33 Ipt(ZERO); I+=OffsetInertiaTensor(Ipt,Vec3(queue[i])-c,1.0f);
					}
					matrixf eigenMtx(3,3,0,&Basis(0,0));
					matrixf(3,3,mtx_symmetric,&I(0,0)).jacobi_transformation(eigenMtx,&Ibody.x);
					float Iz=Ibody[i=idxmin3(Ibody)], Ix=max(max(Ibody.x,Ibody.y),Ibody.z), Iy=Ibody.x+Ibody.y+Ibody.z-Ix-Iz;
					if (min(Ix,Iy) > Iz*(0.5f+sqr(params.capsHRratio)*(1.0f/6)) && nline<MAXPRIMS) { 
						// if the surface's inertia tensor it similar to that of a long rod, register it as a line candidate as well
						Vec3 axis = Basis.GetRow(i);
						float end0=(queue[0]-c)*axis, end1=end0, end;
						for(i=1;i<ihead;i++) {
							end0 = min(end0, end=(Vec3(queue[i])-c)*axis);
							end1 = max(end1, end);
						}
						voxline[nline].ends[0] = c+axis*end0;
						voxline[nline].ends[1] = c+axis*end1;
						voxline[nline++].closed = 0;
					}
					build_surface(2,19,4,true);
					for(i=0;i<ihead;i++) {
						Matrix33 C(ZERO),tmp;
						for(j=nbcnt[i];j<nbcnt[i+1];j++)
							C += dotproduct_matrix(Vec3(queue[nbidx[j]]-queue[i]),Vec3(queue[nbidx[j]]-queue[i]),tmp);
						Vec3 n=SVD3x3(C), dir(0);
						n *= sgnnz(n*unpackdir(vox(queue[i]),4));
						for(j=nbcnt[i];j<nbcnt[i+1];j++)
							dir += queue[nbidx[j]]-queue[i];
						if ((dir-n*(n*dir)).len2()>1)
							n *= 0.5f;
						norm[0][i] = n;
					}
					for(int iter=sqrt(ihead)*0.5f*params.surfPrimIters; iter>0; iter--,swap(norm[1],norm[0]))
						for(i=0;i<ihead;i++) { // average normals from neighbors
							Vec3r n(ZERO); for(j=nbcnt[i];j<nbcnt[i+1];j++) n+=norm[0][nbidx[j]];
							norm[1][i] = n*inv[nbcnt[i+1]-nbcnt[i]];
						}
					if (vox.surf.norm)	for(i=0;i<ihead;i++)
						vox.surf.norm[&vox(queue[i])-vox.data] = norm[0][i];
					for(i=0; i<ihead && nsurf<MAXPRIMS; i++) {
						for(j=nbcnt[i];j<nbcnt[i+1] && norm[0][nbidx[j]].len2()<norm[0][i].len2();j++);
						if (j==nbcnt[i+1] && norm[0][i].len2()>sqr(params.surfMinNormLen)) { // find local maxima of normal lengths - they are centers of box/cylider candidates
							int is; Vec3 n = norm[0][i].normalized();
							for(is=0;is<nsurf && ((voxsurf[is].center-queue[i]).len2()>sqr(params.surfMergeDist) || fabs(voxsurf[is].norm*n)<0.95f);is++);
							if (is==nsurf) {
								memset(voxsurf+nsurf, 0, sizeof(voxsurf[0]));
								voxsurf[nsurf].center = queue[i];
								voxsurf[nsurf].norm = n;
								voxsurf[nsurf].rating = norm[0][i].len();
								voxsurf[nsurf++].count = 1;
							}	else {
								voxsurf[is].center = (voxsurf[is].center*voxsurf[is].count+queue[i])/(voxsurf[is].count+1);
								voxsurf[is].norm = (voxsurf[is].norm+n*sgnnz(n*voxsurf[is].norm)).normalized();
								voxsurf[is].rating = max(voxsurf[is].rating, (float)norm[0][i].len());
								voxsurf[is].count++;
							}
						}
					}
				}	else
					clearMask = 8;
				for(i=0;i<ihead;i++) vox(queue[i]) &= ~(clearMask | -1<<19);
			}

			Vec3i idir[2];
			if (params.findPrimLines) vox_iterate1(vox,ic) 
				if ((vox(ic)&(2|4|8))==2 && is_line(packnb(vox,ic,1,4),5,idir) && nline<MAXPRIMS) { // find line skeletons
					int closed[2] = {0,0}; ihead=0;
					for(Vec3i ic1=ic; (closed[0]=(idir[0].x|idir[0].y|idir[0].z)) && (vox(ic1+idir[0])&(2|8))==2 && packnb(vox,ic1+idir[0],1,4).count1<6; 
							ic1+=idir[0],idir[0]=trace_line(vox,ic1,ic1-ic,1))
						queue[ihead++] = ic1+idir[0];
					for(i=ihead-1,itail=0; itail<i; itail++,i--)
						swap(queue[i],queue[itail]);
					queue[ihead++] = ic;
					for(Vec3i ic1=ic; (closed[1]=(idir[1].x|idir[1].y|idir[1].z)) && (vox(ic1+idir[1])&(2|8))==2 && packnb(vox,ic1+idir[1],1,4).count1<6; 
							ic1+=idir[1],idir[1]=trace_line(vox,ic1,ic1-ic,1))
						queue[ihead++] = ic1+idir[1];
					ihead = min(ihead,vox.sz.GetVolume()*8+1>>1);
					if (ihead<=4)
						continue;
					Vec3 *pt = (Vec3*)(queue+ihead), pt0,pt1,pt2,sum;
					for(i=0;i<ihead;i++) pt[i] = queue[i];
					for(int iter=0;iter<2;iter++)	{ // several averaging passes
						pt0.zero(); pt1=pt[0]; pt[0]=(sum=pt[0]+pt[1])*0.5f; 
						for(i=1; i<ihead-1; i++,pt0=pt1,pt1=pt2) {
							pt2 = pt[i]; pt[i] = (sum+=pt[i+1]-pt0)*(1.0f/3);
						}	pt[i] = (sum-pt0)*0.5f;
					}
					for(int i0=(i=2)-2; i<ihead-2; i++) {	// find local minima of (-1..0),(0..+1) dirs dot, treat them as borders if below threshold
						float dot[3]; for(j=-1;j<=1;j++)
							dot[j+1] = Vec3(pt[i+1+j]-pt[i+j]).GetNormalizedFast()*Vec3(pt[i+j]-pt[i-1+j]).GetNormalizedFast();
						int i1 = i+(ihead-1-i & ihead-4-i>>31);
						if (idxmin3(dot)==1 && dot[1]<params.maxLineDot || i1==ihead-1) {
							for(j=i0+1; j<i1 && (pt[j]-pt[i0]^pt[i1]-pt[i0]).len2()<(pt[i1]-pt[i0]).len2()*sqr(params.maxLineDist); j++);
							if (i1-i0>=params.minLineCells && j==i1) { // register capsule candidate
								for(j=i0;j<=i1;j++) vox(queue[j])|=4;
								for(j=0,voxline[nline].closed=0;j<2;j++) voxline[nline].closed |= (closed[j]&1)<<j;
								voxline[nline].ends[0] = float2int(pt[i0]); 
								voxline[nline].ends[1] = float2int(pt[i1]);
								for(j=0;j<nline;j++) if (MergeLine(voxline[j],voxline[nline])) {
									if (j!=nline-1) 
										swap(voxline[j],voxline[nline-1]); 
									j=0; --nline;
								}
								nline++;
							}
							i0 = i1+1;
						}
					}
				}

		float raylen = sz.x+sz.y+sz.z;
		const int NRAYS = 64;
		const float cosTick=cos(2*gf_PI/NRAYS), sinTick=sin(2*gf_PI/NRAYS);
		Vec3 pthit[NRAYS],nhit[NRAYS];
		quotientf hratio[NRAYS];
		int nhits,nGeoms0=nGeoms;

		auto LongestNormStreak = [&](std::function<bool(const Vec3&,const Vec3&)> ncheck)
		{	// find the longest streak or norms that pass a check
			Vec2i rngmax(-NRAYS>>1,ncheck(nhit[NRAYS>>1],nhit[NRAYS>>1]) ? 1:0), rngcur=rngmax;
			for(i=rngcur.x+1; i<NRAYS && rngmax.y<NRAYS; i++)
				if (ncheck(nhit[rngcur.x&NRAYS-1],nhit[i&NRAYS-1]))
					rngcur.y++; 
				else if (!rngcur.y)
					rngcur.x = i+1;
				else {
					rngmax += (rngcur-rngmax)*isneg(rngmax.y-rngcur.y);
					rngcur.set(i--,0);
				}
			return rngmax.y>rngcur.y ? rngmax:rngcur;
		};

		auto CheckOppo = [&](int start,int len, int minlen, float maxdiff, const Vec3& c,const Vec3& axis) 
		{	// find the longest streak of hits opposing [start,start+len) with same-ish ratio to the original hits
			for(i=start;i<start+len;i++) hratio[i-start].set((pthit[i+(NRAYS>>1)&NRAYS-1]-c)*axis, (pthit[i&NRAYS-1]-c)*axis);
			for(i=0;i<len-1;i++) for(j=len-1;j>i;j--) if (hratio[j]<hratio[j-1])
				swap(hratio[j],hratio[j+1]); // sort maxlen hits by ratios to the opposing hit dist 
			Vec2i rngmax(2-len,1), rngcur=rngmax;
			for(i=rngcur.x+1; max(max(i,rngmax.y),rngcur.y)<len; i++)
				if (inrange(hratio[i+(len&i>>31)]/hratio[i-1+(len&i-1>>31)], 0.95f,1.05f))
					rngcur.y++;
				else {
					rngmax += (rngcur-rngmax)*isneg(rngmax.y-rngcur.y);
					rngcur.set(i,1);
				}
			rngmax = rngmax.y>rngcur.y ? rngmax:rngcur;
			return rngmax.y>=minlen && inrange(hratio[rngmax.x+(len&rngmax.x>>31)],-1-maxdiff,-1+maxdiff) ? hratio[rngmax.x+(len&rngmax.x>>31)] : quotientf(-1);
		};

		auto IntersectRay = [&gray,this]()	-> geom_contact*
		{
			geom_contact *pcont; 
			for(int icont=Intersect(&gray,0,0,0,pcont)-1; icont>=0;icont--) if (pcont[icont].id[0]>=0 && pcont[icont].n*gray.m_dirn>0)
				return pcont+icont;
			return 0;
		};

		for(int isurf=0; isurf<nsurf; isurf++) { // surface (box or cylinder) candidates
			Vec3 c=gray.m_ray.origin=vox.center+vox.q*(Vec3(voxsurf[isurf].center)+Vec3(0.5f))*vox.celldim, n=vox.q*voxsurf[isurf].norm;
			gray.m_ray.dir = (gray.m_dirn = n)*raylen;
			// shoot ray from center in +-z to find thickness; refine center and thickness if possible
			for(i=nhits=0,nhit[0].zero(),nhit[1].zero(); i<2; i++,gray.m_dirn.Flip(),gray.m_ray.dir.Flip()) 
				if (geom_contact *pcont = IntersectRay())
					nhit[nhits]=pcont->n, pthit[nhits++]=pcont->pt;
			if (max(fabs(nhit[0]*n),fabs(nhit[1]*n)) > params.surfNormRefineThresh) {
				gray.m_ray.dir = (gray.m_dirn = n = nhit[isneg(fabs(nhit[0]*n)-fabs(nhit[1]*n))])*(sz.x+sz.y+sz.z);
				for(i=nhits=0; i<2; i++,gray.m_dirn.Flip(),gray.m_ray.dir.Flip()) 
					if (geom_contact *pcont = IntersectRay())
						pthit[nhits++] = pcont->pt;
			}
			if (nhits==2 && fabs((pthit[1]-pthit[0])*n)*0.5f-min(fabs(n*(pthit[0]-c)),fabs(n*(pthit[1]-c))) < vox.celldim*1.5f)
				c = (pthit[0]+pthit[1])*0.5f;
			float zh = max(fabs(n*(pthit[0]-c)),fabs(n*(pthit[1]-c)));
			gray.m_ray.dir = (gray.m_dirn = n.GetOrthogonal().normalized())*raylen;
			memset(nhit,0,sizeof(nhit));	memset(pthit,0,sizeof(pthit));
			for(i=nhits=0; i<NRAYS; i++,gray.m_ray.dir=(gray.m_dirn=gray.m_dirn.GetRotated(n,cosTick,sinTick))*raylen) 
				if (geom_contact *pcont = IntersectRay())	{
					pthit[i]=pcont->pt; nhit[i]=pcont->n; nhits++; // shoot a fan of rays around center
				}
			if (nhits*2 < NRAYS)
				continue;
			
			float r0=0, r=0;
			for(i=0;i<NRAYS;i++) r0+=(pthit[i]-c).len()*nhit[i].len2(); // r0 - average dist
			for(i=j=0,r0/=nhits; i<NRAYS; i++) {
				float ri = (pthit[i]-c).len()*nhit[i].len2(); 
				int inrng = isneg(fabs(ri-r0)-r0*0.2f);
				r += ri*inrng; j += inrng;
			}
			cylinder cyl;
			cyl.r = j*3>nhits	? r/j : r0;	// average dist without extremes
			cyl.center=c;	cyl.axis=n; cyl.hh=zh;

			box box; 
			// axisx = longest streak of same-ish normals
			Vec2i rng = LongestNormStreak([](const Vec3& n0,const Vec3& ni) { return n0*ni>0.99f; });
			Vec3 axisx = nhit[rng.x&NRAYS-1], axisy = n^(axisx-=n*(axisx*n)).normalize();
			for(box.size.x=0,i=rng.x; i<rng.x+rng.y; i++) 
				box.size.x = max(box.size.x,(pthit[i&NRAYS-1]-c)*axisx);
			float sizeopp = box.size.x*CheckOppo(rng.x,rng.y,rng.y>>1,0.2f, c,axisx).val();
			c += axisx*(box.size.x+sizeopp)*0.5f; box.size.x = (box.size.x-sizeopp)*0.5f;
			rng = LongestNormStreak([&axisy](const Vec3& n0,const Vec3& ni) { return fabs(axisy*ni)>0.95f && n0*ni>0; });
			i=sgnnz(axisy*nhit[rng.x&NRAYS-1]); axisx*=i; axisy*=i;
			for(box.size.y=0,i=rng.x; i<rng.x+rng.y; i++) 
				box.size.y = max(box.size.y,(pthit[i&NRAYS-1]-c)*axisy); // size.y - from the longest streak of normals close to axisy
			sizeopp = box.size.y*CheckOppo(rng.x,rng.y,rng.y>>1,0.2f, c,axisy).val();
			c += axisy*(box.size.y+sizeopp)*0.5f; box.size.y = (box.size.y-sizeopp)*0.5f;
			box.center = c;	box.size.z = zh;
			box.Basis = Matrix33(axisx,axisy,n).T();
			voxsurf[isurf].sbox = box;
			voxsurf[isurf].cyl = cyl;
		}

		for(i=0;i<nsurf-1;i++) for(j=nsurf-1;j>i;j--) if (voxsurf[j].sbox.size.GetVolume()>voxsurf[j-1].sbox.size.GetVolume())
			swap(voxsurf[j],voxsurf[j-1]); // sort surfaces by decreasing box volumes
		for(; nsurf && voxsurf[nsurf-1].sbox.size.GetVolume()>0; nsurf--);

		for(i=0; i<nsurf && nGeoms<params.maxGeoms; i++) if (!(params.skipPrimMask & 1ull<<i)) {
			float Vbox[2],Vcyl[2];
			Vec2i ncellBox[2],ncellCyl[2];
			ncellBox[1] = CheckPrim(vox, box::type, &voxsurf[i].sbox, Vbox[0], vox.celldim);
			ncellCyl[1] = CheckPrim(vox, cylinder::type, &voxsurf[i].cyl, Vcyl[0], vox.celldim);
			Vbox[1] = (ncellBox[0] = CheckPrim(vox, box::type, &voxsurf[i].sbox, Vbox[0])).x*cube(vox.celldim);
			Vcyl[1] = (ncellCyl[0] = CheckPrim(vox, cylinder::type, &voxsurf[i].cyl, Vcyl[0])).x*cube(vox.celldim);
			bool okBox = Vbox[1] > Vbox[0]*params.primVfillSurf && ncellBox[1].x-ncellBox[0].x < (ncellBox[1].y-ncellBox[0].y)*params.primSurfOutside;
			bool okCyl = Vcyl[1] > Vcyl[0]*params.primVfillSurf && ncellCyl[1].x-ncellCyl[0].x < (ncellCyl[1].y-ncellCyl[0].y)*params.primSurfOutside;
			if (!okCyl && okBox || okCyl && okBox && quotientf(Vbox[1],Vbox[0])>quotientf(Vcyl[1],Vcyl[0])) {
				voxsurf[i].sbox.size += Vec3(params.inflatePrims);
				pGeoms[nGeoms++] = (new CBoxGeom)->CreateBox(&voxsurf[i].sbox);
				pGeoms[nGeoms-1]->SetForeignData(0,i);
				CheckPrim(vox, box::type,&voxsurf[i].sbox, Vbox[0], vox.celldim*params.primVoxInflate, params.primRefillThresh);
			} else if (okCyl) {
				voxsurf[i].cyl.r+=params.inflatePrims; voxsurf[i].cyl.hh+=params.inflatePrims;
				pGeoms[nGeoms++] = (new CCylinderGeom)->CreateCylinder(&voxsurf[i].cyl);
				pGeoms[nGeoms-1]->SetForeignData(0,i);
				CheckPrim(vox, cylinder::type,&voxsurf[i].cyl, Vcyl[0], vox.celldim*params.primVoxInflate, params.primRefillThresh);
			}
		}

		for(i=0;i<nline-1;i++) for(j=nline-1;j>i;j--) if ((voxline[j].ends[1]-voxline[j].ends[0]).len2()>(voxline[j-1].ends[1]-voxline[j-1].ends[0]).len2())
			swap(voxline[j],voxline[j-1]); // sor line by decreasing line lengths

		for(int iline=0; iline<nline && nGeoms<params.maxGeoms; iline++) if (!(params.skipPrimMask & 1ull<<iline+nsurf)) { // line (capsule or cylinder) condidates
			Vec3 end[2]; for(i=0;i<2;i++) end[i]=vox.center+vox.q*(Vec3(voxline[iline].ends[i])+Vec3(0.5f))*vox.celldim;
			Vec3 c0=(end[0]+end[1])*0.5f, c,n=(end[1]-end[0]).normalized();
			// shoot a fan of rays around center
			gray.m_ray.origin = c0;
			gray.m_ray.dir = (gray.m_dirn = n.GetOrthogonal().normalized())*raylen;
			memset(nhit,0,sizeof(nhit));	memset(pthit,0,sizeof(pthit));
			Matrix33 C(ZERO),tmp;	float len;
			for(i=nhits=0,c.zero(),len=0; i<NRAYS; i++,gray.m_ray.dir=(gray.m_dirn=gray.m_dirn.GetRotated(n,cosTick,sinTick))*raylen) 
				if (geom_contact *pcont = IntersectRay())	{	// shoot a fan of rays around the center
					if (i>0 && nhit[i-1].len2()>0) {
						float leni = (pcont->pt-pthit[i-1]).len(); 
						c += (pcont->pt+pthit[i-1])*leni; len+=leni;
					}
					pthit[i]=pcont->pt; nhit[i]=pcont->n; nhits++; 
					C += dotproduct_matrix(pcont->n,pcont->n,tmp);
				}
			if (nhit[0].len2()*nhit[i-1].len2()>0) {
				float leni = (pthit[0]-pthit[i-1]).len();
				c += (pthit[0]+pthit[i-1])*leni; len+=leni;
			}
			if (nhits*2 < NRAYS || !len)
				continue;
			cylinder cyl;
			gray.m_ray.origin = c/=len*2;	 
			cyl.axis = SVD3x3(C);	cyl.axis *= sgnnz(n*cyl.axis);
			gray.m_ray.dir = (gray.m_dirn = cyl.axis.GetOrthogonal().normalized())*raylen;
			for(i=nhits=0,cyl.r=0; i<NRAYS; i++,gray.m_ray.dir=(gray.m_dirn=gray.m_dirn.GetRotated(cyl.axis,cosTick,sinTick))*raylen) 
				if (geom_contact *pcont = IntersectRay())
					cyl.r+=pcont->t, nhits++;
			int type = cylinder::type;
			cyl.r /= nhits;	
			gray.m_ray.dir = (gray.m_dirn=-cyl.axis)*raylen;
			for(i=0;i<2;i++,gray.m_dirn.Flip(),gray.m_ray.dir.Flip()) if (geom_contact *pcont = IntersectRay())	{
				float curlen = (end[i]-c)*gray.m_dirn;
				if (inrange((pcont->pt-c)*gray.m_dirn, curlen,curlen*1.5f))
					end[i] = pcont->pt;
			}
			cyl.hh = ((end[1]-end[0])*cyl.axis)*0.5f;
			cyl.center = c+cyl.axis*(cyl.axis*(end[1]-c)-cyl.hh);
			if (cyl.hh*2 > cyl.r*params.capsHRratio) {
				type = capsule::type;
				for(i=0;i<2;i++) if (!(voxline[iline].closed & 1<<i))	// open end = cap sphere end
					cyl.center+=cyl.axis*cyl.r*(0.5f-i), cyl.hh-=cyl.r*0.5f;
				else { // closed end - start as cylinder end end reduce until end voxel is occupied
					Vec3 ax=cyl.axis*(i*2-1), pt=cyl.center+ax*cyl.r;
					voxgrid &voxsafe = *(voxgrid*)&vox;
					float h; for(h=cyl.hh; h>cyl.hh-cyl.r && !(voxsafe(float2int(((pt+ax*h-vox.center)*vox.q)*vox.rcelldim))&1); h-=vox.celldim);
					if (h>cyl.hh-cyl.r) {
						h = (cyl.hh-h)*0.5f; cyl.center-=ax*h, cyl.hh-=h;
					}
				}
			}
			float Vcyl[2]; Vcyl[0] = CheckPrim(vox,type,&cyl,Vcyl[1]).x*cube(vox.celldim);
			if (Vcyl[0] > Vcyl[1]*params.primVfillLine)	{ // output cylinder or capsyle
				cyl.hh+=params.inflatePrims; cyl.r+=params.inflatePrims;
				pGeoms[nGeoms++] = type==capsule::type ?  (new CCapsuleGeom)->CreateCapsule((capsule*)&cyl) : (new CCylinderGeom)->CreateCylinder(&cyl);
				pGeoms[nGeoms-1]->SetForeignData(0,iline+nsurf);
				CheckPrim(vox, type,&cyl, Vcyl[1], vox.celldim*params.primVoxInflate, params.primRefillThresh);
			}
		}

		if (nGeoms>=params.maxGeoms)
			break;

		// make 2 hashplanes with the same res
		grid hashgrid[2];
		index_t *pHashGrid[2],*pHashData[2],*pCellTris=new index_t[nTris*3];
		coord_plane hashplane;
		hashplane.origin = vox.center;
		for(i=0;i<2;i++) {
			hashplane.n=vox.q*ort[i]; hashplane.axes[0]=vox.q*ort[inc_mod3[i]]; hashplane.axes[1]=vox.q*ort[dec_mod3[i]];
			HashTrianglesToPlane(hashplane, Vec2(sz[inc_mod3[i]],sz[dec_mod3[i]])*2, hashgrid[i],pHashGrid[i],pHashData[i], 1/celldim, 0,pTris,nTris, max(vox.sz[inc_mod3[i]],vox.sz[dec_mod3[i]])*2);
		}

		if ((params.findPrimLines | params.findPrimSurfaces | params.reuseVox) & params.findMeshes) 
			vox_iterate1(vox,ic) vox(ic)&=1;
		if (params.findMeshes) vox_iterate1(vox,ic) if ((vox(ic)&3)==1 && is_surface(vox,ic)) {
			for(vox(queue[0]=ic)|=2,ihead=1,itail=0; itail<ihead;) // trace voxel surfaces
				for(Vec3i ic1=queue[i=0,itail++];i<27;i++) if ((vox(ic1+idx2dir[i])&3)==1 && is_surface(vox,ic1+idx2dir[i]))
					vox(queue[ihead]=ic1+idx2dir[i]) |= 2|ihead<<4, ihead++;
			if (ihead<params.surfMeshMinCells)
				continue;
			build_surface(1,4,-1,true);

			for(i=0;i<ihead;i++) { // for each surface voxel, refine vertex/normal from the original mesh
				vtx[i] = vox.center+vox.q*(Vec3(queue[i])+Vec3(0.5f))*vox.celldim;
				int nCellTris=0,offsEnd=params.surfRefineWithMesh ? 14:0;
				for(int offs=0,icell[2]; offs<offsEnd; offs+=2) { // collect triangles	in the vicinity
					Vec4i ipt = Vec4i(queue[i]+vox.sz,0); ipt[offs>>2]+=1-(offs&2);
					for(j=0;j<2;j++) icell[j] = Vec2i(ipt[inc_mod3[j]],ipt[dec_mod3[j]])*hashgrid[j].stride;
					int ntris = intersect_lists(pHashData[0]+pHashGrid[0][icell[0]],pHashGrid[0][icell[0]+1]-pHashGrid[0][icell[0]], 
						pHashData[1]+pHashGrid[1][icell[1]],pHashGrid[1][icell[1]+1]-pHashGrid[1][icell[1]], pCellTris+nCellTris);
					nCellTris = unite_lists(pCellTris,nCellTris, pCellTris+nCellTris,ntris, pCellTris+(j=nCellTris+ntris),nTris*3-nCellTris-ntris);
					memcpy(pCellTris, pCellTris+j, nCellTris*sizeof(pCellTris[0]));
				}

				struct closest_point { 
					closest_point(quotientf d2,const Vec3& pt,int i0,int i1=-1) : dist2(d2),pt(pt),ivtx0(i0),ivtx1(i1+(i0+1&i1>>31)) {}
					quotientf dist2; Vec3 pt; int ivtx0,ivtx1; 
				} ptbest = closest_point(quotientf(sqr(vox.celldim),1),Vec3(0),-2);
				for(int itri=0,ivtx; itri<nCellTris; itri++) { // find the closest point on the mesh
					Vec3 v[3],edge,n=pNormals[pCellTris[itri]]; float dist,len2;
					for(ivtx=0;ivtx<3;ivtx++) v[ivtx] = m_pVertices[pTris[pCellTris[itri]*3+ivtx]];
					for(ivtx=0;ivtx<3;ivtx++) if ((v[inc_mod3[ivtx]]-v[ivtx] ^ vtx[i]-v[ivtx])*n < 0) {
						for(j=0;j<3;j++) if (len2=(edge=v[inc_mod3[j]]-v[j]).len2(), inrange(edge*(vtx[i]-v[j]),0.0f,len2)) 
							if (quotientf(dist=(vtx[i]-v[j]^edge).len2(),len2) < ptbest.dist2)
								ptbest = closest_point(quotientf(dist,len2), v[j]*len2+edge*(edge*(vtx[i]-v[j])), pTris[pCellTris[itri]*3+j], pTris[pCellTris[itri]*3+inc_mod3[j]]);
						ivtx = idxmin3(Vec3((v[0]-vtx[i]).len2(),(v[1]-vtx[i]).len2(),(v[2]-vtx[i]).len2()));
						if ((dist=(v[ivtx]-vtx[i]).len2()) < ptbest.dist2)
							ptbest = closest_point(quotientf(dist), v[ivtx], pTris[pCellTris[itri]*3+ivtx]); 
						break;
					}
					if (ivtx==3 && sqr(dist = (vtx[i]-v[0])*n) < ptbest.dist2)
						ptbest = closest_point(quotientf(sqr(dist)), vtx[i]-n*dist,-1), norm[0][i]=n;
				}

				if (ptbest.ivtx0!=-2) {
					if (ptbest.ivtx0>=0)	{ // if closest pt is edge or vertex, average normal from all involved tris
						for(int itri=(norm[0][i].zero(),0);itri<nCellTris;itri++) {
							for(int idx=j=0;idx<3;idx++) {
								int ivtx = pTris[pCellTris[itri]*3+idx];
								j += iszero(ivtx-ptbest.ivtx0) + iszero(ivtx-ptbest.ivtx1);
							}
							norm[0][i] += pNormals[pCellTris[itri]]*isneg(1-j);
						}	
						norm[0][i].normalize();
					}
					vtx[i] = ptbest.pt*(ptbest.dist2.y==1.0f ? 1.0f:1.0f/ptbest.dist2.y);
				}	else {
					Matrix33 C(ZERO),tmp;	// compute normal from voxel neighbors
					for(j=nbcnt[i];j<nbcnt[i+1];j++)
						C += dotproduct_matrix(Vec3(queue[nbidx[j]]-queue[i]),Vec3(queue[nbidx[j]]-queue[i]),tmp);
					Vec3 n=SVD3x3(C); Vec3i dir[2]={Vec3i(0),Vec3i(0)};
					for(j=0;j<27;j++) dir[vox(queue[i]+idx2dir[j])&1] += idx2dir[j]; 
					j = isneg(dir[0].len2()-dir[1].len2());
					norm[0][i] = vox.q*n*(sgnnz(n*dir[j])*(1-j*2));
				}
			}

			for(int iter=params.surfMeshIters; iter>0; iter--,swap(norm[1],norm[0]))
				for(i=0;i<ihead;i++) { // average normals from neighbors
					Vec3 n(ZERO); for(j=nbcnt[i];j<nbcnt[i+1];j++) n+=norm[0][nbidx[j]];
					norm[1][i] = n*inv[nbcnt[i+1]-nbcnt[i]];
				}

			if (params.storeVox) {
				vox.surf.Free();	// save debug info
				memcpy(vox.surf.cells=new Vec3i[vox.surf.ncells=ihead], queue, ihead*sizeof(Vec3i));
				memcpy(vox.surf.vtx=new Vec3[ihead], vtx, ihead*sizeof(Vec3));
				for(i=0,vox.surf.norm=new Vec3[ihead];i<ihead;i++) vox.surf.norm[i]=norm[0][i];
				memcpy(vox.surf.nbcnt=new int[ihead+1], nbcnt, (ihead+1)*sizeof(int));
				memcpy(vox.surf.nbidx=new int[nbcnt[ihead]], nbidx, nbcnt[ihead]*sizeof(int));
				vox.bitVis = 6;
			}

			for(i=0;i<ihead;i++) // mark voxels with local minima normals as vertices
				for(float minmax=1-(int)params.surfMaxAndMinNorms*2; minmax<=1; minmax+=2) {
					for(j=nbcnt[i];j<nbcnt[i+1] && norm[0][nbidx[j]].len2()*minmax>norm[0][i].len2()*minmax;j++);
					vox(queue[i]) |= iszero(j-nbcnt[i+1])<<2;
				}

			// if clusters of vertices are possible:
			// find cluster's center
			// find axis that minimizes ((pt[i]-c)*axis)^2
			// find vertices wit min and max projections on this axis

			for(i=0;i<vox.nflips;i++)
				vox(vox.flipHist[i]) ^= (vox(vox.flipHist[i]) & 2)<<1;

			int *voridx=(int*)norm[1], *vqueue=voridx+ihead, *vtxidx=vqueue+ihead, *edges=vtxidx+ihead, vhead,vtail,nvtx,nedges,nTris=0;
			Vec3_tpl<unsigned short> *pOutTris = (Vec3_tpl<unsigned short>*)(edges+ihead);
			memset(voridx,-1,ihead*sizeof(int));
			for(i=vhead=nvtx=0;i<ihead;i++) if (vox(queue[i]) & 4) {
				vtxidx[nvtx] = i;	// output a vertex
				vtx[voridx[vqueue[vhead++] = i] = nvtx++] = vtx[i]+norm[0][i]*params.inflateMeshes;	
			}
			for(vtail=nedges=0; vtail<vhead;) { // grow voronoi regions around vertices; output edges when regions meet
				i = vqueue[vtail++];
				if (nedges+nbcnt[i+1]-nbcnt[i] > ihead)
					break; // edge buffer overflow
				int ilastVtx = -1;
				for(j=nbcnt[i];j<nbcnt[i+1];j++) if (voridx[nbidx[j]]<0) 
					voridx[vqueue[vhead++] = nbidx[j]] = voridx[i];
				else if (voridx[i]<voridx[nbidx[j]] && voridx[nbidx[j]]!=ilastVtx)
					edges[nedges++] = voridx[i]<<16 | (ilastVtx=voridx[nbidx[j]]);
			}
			for(i=0,memset(nbcnt,0,(nvtx+1)*sizeof(nbcnt[0]));i<nedges;i++) 
				nbcnt[edges[i]>>16]++;
			for(i=0;i<nvtx;i++) nbcnt[i+1]+=nbcnt[i];
			for(i=0,memset(nbidx,-1,nbcnt[nvtx]*sizeof(nbidx[0]));i<nedges;i++)	{ // register only unique vtxi-vtxj edges
				int v0=edges[i]>>16, v1=edges[i]&0xffff;
				for(j=nbcnt[v0];j<nbcnt[v0+1] && nbidx[j]>=0 && nbidx[j]!=v1;j++);
				if (j==nbcnt[v0+1] || nbidx[j]<0)
					nbidx[--nbcnt[v0]] = v1;
			}
			for(i=0;i<nvtx;i++) for(j=nbcnt[i];j<nbcnt[i+1]-1 && nbidx[j]>=0;j++)
				for(int j1=j+1;j1<nbcnt[i+1] && nbidx[j1]>=0;j1++) { // create a triangle if any 2 connections of vtxi are connected to each other
					int v0=min(nbidx[j],nbidx[j1]), v1=nbidx[j]+nbidx[j1]-v0;
					for(int j2=nbcnt[v0]; j2<nbcnt[v0+1] && nbidx[j2]>=0; j2++) if (nbidx[j2]==v1) {
						int flip = -isneg((vtx[v0]-vtx[i]^vtx[v1]-vtx[i])*(norm[0][vtxidx[i]]+norm[0][vtxidx[v0]]+norm[0][vtxidx[v1]]));
						pOutTris[nTris++].Set(i,v0+(v1-v0&flip),v1+(v0-v1&flip)); break;
					}
				}

			if (nvtx && nTris) {
				CTriMesh *pMesh = new CTriMesh();
				pGeoms[nGeoms++] = pMesh->CreateTriMesh(vtx,&pOutTris[0].x,(char*)pOutTris,0,nTris,(nTris<11 ? mesh_SingleBB : mesh_AABB|mesh_AABB_rotated|mesh_OBB)|mesh_multicontact1);
				if (nGeoms>=params.maxGeoms)
					break;
			}
		}
		delete[] queue;
		for(i=0;i<2;i++) delete[] pHashGrid[i], delete[] pHashData[i]; delete[] pCellTris;
		if (params.storeVox) {
			if (m_voxgrid) delete m_voxgrid;
			(*(m_voxgrid=new voxgrid_tpl<false>)=vox).persistent = true;
		}	else if (!params.reuseVox)
			vox.Free();
	}

	if (pTris!=m_pIndices) delete[] pTris;
	if (pNormals!=m_pNormals) delete[] pNormals;
	delete[] pVtxMap;
	delete[] nbcnt; delete[] nbidx;
	delete[] norm[0]; delete[] norm[1]; delete[] vtx;
	
	pOutGeoms = (IGeometry**)pGeoms;
	return nGeoms;
}


int CTriMesh::SanityCheck()
{
	return m_pTree->SanityCheck();
}


template<class T> void flip(T &a,T &b) { T c=a; a=b; b=c; }
void CTriMesh::Flip()
{
	for(int i=0;i<m_nTris;i++) {
		flip(m_pIndices[i*3+1],m_pIndices[i*3+2]);
		flip(m_pTopology[i].ibuddy[0],m_pTopology[i].ibuddy[2]);
		m_pNormals[i].Flip();
	}
	if (!is_unused(m_V))
		m_V = -m_V;
}

float	CTriMesh::ComputeVesselVolume(const plane *pplane, IGeometry **pFloaters,QuatTS *qtsFloaters,int nFloaters, int ihash, index_t *pIndices,Vec3 *pNormals, float dim, float &Vsum,Vec3 &comSum)
{
	Vec3 massCenter;
	geom_world_data gwd;
	prim_inters inters;
	const grid& g = m_hashgrid[ihash];
	ray aray; aray.dir=-g.Basis.GetRow(2)*dim;
	float V = -CalculateBuoyancy(pplane,&gwd,massCenter);	Vsum=V;	comSum=massCenter;
	for(int i=0; i<nFloaters; i++) {
		box bbox; pFloaters[i]->GetBBox(&bbox);
		bbox.size = (bbox.Basis = g.Basis*Matrix33(qtsFloaters[i].q)*bbox.Basis.T()).Fabs()*bbox.size*qtsFloaters[i].s;
		bbox.center = g.Basis*((aray.origin=qtsFloaters[i]*bbox.center)-g.origin);
		Vec2_tpl<int> ibbox[2],ic;
		ibbox[0].set(float2int((bbox.center.x-bbox.size.x)*g.stepr.x-0.5f), float2int((bbox.center.y-bbox.size.y)*g.stepr.y-0.5f));
		ibbox[1].set(float2int((bbox.center.x+bbox.size.x)*g.stepr.x-0.5f), float2int((bbox.center.y+bbox.size.y)*g.stepr.y-0.5f));
		int bOutside=1, icell,itri;
		for(ic.x=max(0,min(g.size.x,ibbox[0].x)); ic.x<=max(-1,min(g.size.x-1,ibbox[1].x)); ic.x++)
			for(ic.y=max(0,min(g.size.y,ibbox[0].y)); ic.y<=max(-1,min(g.size.y-1,ibbox[1].y)); ic.y++)
				for(int j=m_pHashGrid[ihash][icell=g.stride*ic]; j<m_pHashGrid[ihash][icell+1]; j++)
					bOutside &= m_pTri2Island[m_pHashData[ihash][j]].bFree;
		if (!bOutside) {
			ic.set(min(g.size.x-1,max(0,float2int(bbox.center.x*g.stepr.x-0.5f))), min(g.size.y-1,max(0,float2int(bbox.center.y*g.stepr.y-0.5f))));
			aray.origin = g.origin + Vec3((ic.x+0.5f)*g.step.x, (ic.y+0.5f)*g.step.y, g.Basis.GetRow(2)*(aray.origin-g.origin))*g.Basis;
			float dist,minDist=1e10f,minDot=1.0f;
			for(int j=m_pHashGrid[ihash][icell=g.stride*ic]; j<m_pHashGrid[ihash][icell+1]; j++)
				if (!m_pTri2Island[itri=m_pHashData[ihash][j]].bFree) {
					triangle tri; tri.n = pNormals[itri];
					for(int i1=0;i1<3;i1++) tri.pt[i1]=m_pVertices[pIndices[itri*3+i1]];
					if (tri_ray_intersection(&tri,&aray,&inters) && (dist=aray.dir*(inters.pt[0]-aray.origin))<minDist)
						minDist=dist,minDot=tri.n*aray.dir;
				}
			if (minDot<0) {
				gwd.offset=qtsFloaters[i].t; gwd.R=Matrix33(qtsFloaters[i].q); gwd.scale=qtsFloaters[i].s;
				V -= pFloaters[i]->CalculateBuoyancy(pplane,&gwd,massCenter);
			}
		}
	}
	return V;
}

int CTriMesh::FloodFill(const Vec3& org, float &Vtrg, const Vec3& gdir, float e, Vec2* &ptborder,QuatT &qtBorder,float &depth,int bConvexBorder,
	IGeometry **pFloaters,QuatTS *qtsFloaters,int nFloaters, float &Vcombined, Vec3 &comCombined)
{
#define GetVesselVolume ComputeVesselVolume(&waterPlane,pFloaters,qtsFloaters,nFloaters,ihash,pIndices,pNormals,dim,Vsum,comSum)
	// shoot a ray down, then descend to a local minimum vtx
	box bbox; m_pTree->GetBBox(&bbox);
	float dim = bbox.size.x+bbox.size.y+bbox.size.z;
	CRayGeom aray(org,gdir*dim*3.0f);
	int ncont; geom_contact *pcont;
	geom_world_data gwd; gwd.R.SetIdentity(); gwd.offset.zero(); gwd.scale=1.0f;
	if (!(ncont = Intersect(&aray,&gwd,0,0,pcont)))
		return 0;

	WriteLock lock(m_lockUpdate),lock1(m_lockHash);
	int i,ihash,j,nTris=m_nTris,ibrd0,*pVtxVal,*pBrdNext,*pBrdPrev,*pVtx2Tri,*pTriMap;
	index_t *pIndices = m_pIndices;
	trinfo *pTopology = m_pTopology;
	Vec3 *pNormals = m_pNormals;
	plane waterPlane; waterPlane.n=-gdir;

	BuildIslandMap();
	for(ihash=0;ihash<m_nHashPlanes && (m_hashgrid[ihash].Basis.GetRow(2)*gdir)>-0.998f;ihash++);
	if (ihash==m_nHashPlanes) {
		for(i=0;i<m_nHashPlanes;i++)	{
			delete[] m_pHashGrid[i]; delete[] m_pHashData[i];
		}
		coord_plane hashplane;
		hashplane.n=-gdir; hashplane.axes[0]=gdir^(hashplane.axes[1]=-gdir.GetOrthogonal().normalized());
		Vec2 bbox2d[2]; bbox2d[0]=bbox2d[1] = Vec2(hashplane.axes[0]*m_pVertices[0],hashplane.axes[1]*m_pVertices[0]);
		for(i=1;i<m_nVertices;i++) {
			Vec2 pt2d(hashplane.axes[0]*m_pVertices[i], hashplane.axes[1]*m_pVertices[i]);
			bbox2d[0] = min(bbox2d[0],pt2d); bbox2d[1] = max(bbox2d[1],pt2d);
		}
		hashplane.origin = (bbox2d[0]+bbox2d[1])*0.5f;
		HashTrianglesToPlane(hashplane,bbox2d[1]-bbox2d[0],m_hashgrid[0],m_pHashGrid[0],m_pHashData[0]);
		ihash = 0; m_nHashPlanes = 1;
	}

	ptborder = new Vec2[nTris+1];
	memset(pVtxVal=new int[m_nVertices+1],0,(m_nVertices+1)*sizeof(int));
	memset(pBrdNext=new int[m_nVertices],-1,m_nVertices*sizeof(int));
	pBrdPrev=new int[m_nVertices]; pVtx2Tri=new int[m_nTris*3];
	memset(pTriMap=new int[m_nTris+1],-1,(m_nTris+1)*sizeof(int)); ++pTriMap;
	for(i=0;i<m_nTris*3;i++) pVtxVal[pIndices[i]]++;
	for(i=0;i<m_nVertices;i++) pVtxVal[i+1]+=pVtxVal[i];
	for(i=0;i<m_nTris;i++) for(j=0;j<3;j++)	pVtx2Tri[--pVtxVal[pIndices[i*3+j]]]=i;
	m_pIndices=new index_t[m_nTris*3]; m_pTopology=new trinfo[m_nTris]; m_pNormals=new Vec3[m_nTris]; m_nTris=0;
	for(i=0;i<nTris;i++) m_pTri2Island[i].bFree=1;

	int ivtxBott,ivtxBott0,nTris0,nNewTris;
	float V[2],Vsum;
	Vec3 comSum;
	ivtxBott = pIndices[pcont[ncont-1].iPrim[0]*3];
	do {
		for(i=pVtxVal[ivtxBott0=ivtxBott]; i<pVtxVal[ivtxBott0+1]; i++) {
			j=pVtx2Tri[i]; int ivtxBott1 = pIndices[j*3+idxmax3(Vec3(m_pVertices[pIndices[j*3]]*gdir,m_pVertices[pIndices[j*3+1]]*gdir,m_pVertices[pIndices[j*3+2]]*gdir))];
			ivtxBott += ivtxBott1-ivtxBott & -isneg((m_pVertices[ivtxBott]-m_pVertices[ivtxBott1])*gdir);
		}
	} while(ivtxBott!=ivtxBott0);

	pBrdNext[ivtxBott]=pBrdPrev[ivtxBott] = ibrd0=ivtxBott;
	depth = m_pVertices[ivtxBott]*gdir;
	V[0]=1; V[1]=0; nTris0=0;

	do {
		if (V[1]>V[0]) {
			ivtxBott0=ivtxBott; nTris0=m_nTris; V[0]=V[1];
		}
		do { // mark triangles around the bottom vtx
			int itri,ivtx,i1,bBorder;
			for(i=pVtxVal[ivtxBott],nNewTris=0; i<pVtxVal[ivtxBott+1]; i++) if (m_pTri2Island[itri=pVtx2Tri[i]].bFree) {
				m_pTri2Island[itri].bFree=0; nNewTris++;
				m_pTopology[m_nTris]=pTopology[itri]; m_pNormals[m_nTris]=pNormals[itri];	pTriMap[itri]=m_nTris;
				((Vec3_tpl<index_t>*)m_pIndices)[m_nTris++] = ((Vec3_tpl<index_t>*)pIndices)[itri];
				for(j=0;j<3;j++) if ((ivtx=pIndices[itri*3+j])!=ivtxBott)	{
					for(i1=pVtxVal[ivtx],bBorder=0; i1<pVtxVal[ivtx+1]; i1++)
						bBorder |= m_pTri2Island[pVtx2Tri[i1]].bFree;
					if (bBorder & pBrdNext[ivtx]>>31)	{	// add vtx to the border
						pBrdNext[ivtx]=pBrdNext[ibrd0]; pBrdPrev[ivtx]=ibrd0;
						pBrdPrev[pBrdNext[ibrd0]]=ivtx;	pBrdNext[ibrd0]=ivtx;
					} else if (!bBorder && pBrdNext[ivtx]>=0) {	// remove vtx from the border
						pBrdNext[pBrdPrev[ivtx]]=pBrdNext[ivtx]; pBrdPrev[pBrdNext[ivtx]]=pBrdPrev[ivtx];
						ibrd0 += pBrdNext[ibrd0]-ibrd0 & -iszero(ibrd0-ivtx);
					}
				}
			}
			pBrdNext[pBrdPrev[ivtxBott]]=pBrdNext[ivtxBott]; pBrdPrev[pBrdNext[ivtxBott]]=pBrdPrev[ivtxBott];
			ibrd0 += pBrdNext[ibrd0]-ibrd0 & -iszero(ibrd0-ivtxBott);
			if (!nNewTris) {
				m_nTris=nTris0; goto fullspill;
			}
			// find the bottommost border vtx; repeat if it's lower than the previous lowest vtx
			for(ivtxBott=ibrd0,i=pBrdNext[ibrd0]; i!=ibrd0; i=pBrdNext[i])
				ivtxBott += i-ivtxBott & -isneg(m_pVertices[ivtxBott]*gdir-m_pVertices[i]*gdir);
		} while(m_pVertices[ivtxBott0]*gdir < m_pVertices[ivtxBott]*gdir);
		waterPlane.origin = m_pVertices[ivtxBott];
	} while((V[1]=GetVesselVolume)<Vtrg);

fullspill:
	waterPlane.origin = m_pVertices[ivtxBott0];
	float Vcur,h[]={ 0,(m_pVertices[ivtxBott0]-m_pVertices[ivtxBott])*gdir };
	V[0] = GetVesselVolume;
	if (!nNewTris)
		Vtrg = V[0];
	i = 0;
	if (V[0]>=Vtrg) {
		// would have to spill some water into a separate volue on the same mesh
		m_nTris = nTris0;	waterPlane.origin += gdir*dim*0.0003f;
		Vtrg = GetVesselVolume;
	}	else do {
		float hcur = h[0]+(h[1]-h[0])*expf(logf((Vtrg-V[0])/(V[1]-V[0]))*(1-(i<<1&1)*(1.0f/3)));
		waterPlane.origin = m_pVertices[ivtxBott0]-gdir*hcur;
		Vcur = GetVesselVolume;
		h[isneg(Vtrg-Vcur)] = hcur;
	} while(fabs_tpl(Vcur-Vtrg)>e && ++i<100);
	depth -= waterPlane.origin*gdir;
	Vcombined = Vsum; comCombined = comSum;

	int npt;
	prim_inters inters,inters1;
	ptitem2d *pt2d=new ptitem2d[m_nTris*2];
	geometry_under_test gtest; gtest.R.SetIdentity(); gtest.offset.zero(); gtest.scale=1.0f;
	indexed_triangle triMesh,triWater;
	primitive *ptri[2] = { &triMesh,&triWater };
	Vec3 axis = -gdir.GetOrthogonal().normalized(), axisx=gdir^axis;
	triWater.n = -gdir;
	for(i=0;i<3;i++,axis=axis.GetRotated(triWater.n,-0.5f,sqrt3*0.5f))
		triWater.pt[i] = waterPlane.origin+axis*dim*2;
	float maxDist=-1e10f,dist;
	for(i=npt=0,triMesh.idx=-1; i<m_nTris; i++) {
		for(j=0;j<3;j++) triMesh.pt[j]=m_pVertices[m_pIndices[i*3+j]]; triMesh.n=m_pNormals[i];
		if (tri_tri_intersection(&triMesh,&triWater,&inters1)) {
			for(j=0;j<2;j++) pt2d[npt++].pt.set(inters1.pt[j]*axisx,inters1.pt[j]*axis);
			if ((dist=(pt2d[npt-1].pt.x,pt2d[npt-2].pt.x))>maxDist)
				triMesh.idx=i, maxDist=dist, inters=inters1;
		}
		for(j=0;j<3;j++)
			m_pTopology[i].ibuddy[j] = pTriMap[m_pTopology[i].ibuddy[j]];
	}
	if (triMesh.idx>=0) {
		for(j=0;j<3;j++) triMesh.pt[j]=m_pVertices[m_pIndices[triMesh.idx*3+j]]; triMesh.n=m_pNormals[triMesh.idx];
		border_trace border; memset(&border,0,sizeof(border));
		border.pt = new Vec3[border.szbuf=m_nTris];
		border.pt_end=inters.pt[0]; border.end_dist2=sqr(dim*0.0005f);
		for(j=0;j<2;j++) border.pt[j]=inters.pt[j];	border.npt=2;
		border.itri_end=triMesh.idx; border.iedge_end=inters.iFeature[0][0]&3;
		border.itri = new int[m_nTris][2];
		if (!bConvexBorder && TraceTriangleInters(0,ptri,0,triangle::type,&inters,&gtest,&border)==2) {
			for(i=0,npt=border.npt-1;i<npt;i++)
				ptborder[i].set(border.pt[i]*axisx,border.pt[i]*axis);
		} else {
			edgeitem *edges = new edgeitem[npt+1],*pedge;
			npt = qhull2d(pt2d,npt,edges);
			for(i=0,pedge=edges;i<npt;i++,pedge=pedge->next)
				ptborder[i] = pedge->pvtx->pt;
			delete[] edges;
		}
		qtBorder.q = Quat(Matrix33(axisx,axis,-gdir));
		qtBorder.t = qtBorder.q*Vec3(0,0,-gdir*inters.pt[0]);
		delete[] border.itri; delete[] border.pt;	delete[] pt2d;
	}
	delete[] m_pNormals; delete[] m_pTopology; delete[] m_pIndices;
	m_pIndices=pIndices; m_pTopology=pTopology; m_pNormals=pNormals; m_nTris=nTris;
	delete[] (pTriMap-1); delete[] pVtx2Tri; delete[] pBrdPrev; delete[] pBrdNext; delete[] pVtxVal;
	return npt;
}
