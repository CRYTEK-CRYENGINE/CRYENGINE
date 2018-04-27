// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "bvtree.h"
#include "geometry.h"
#include "aabbtree.h"
#include "trimesh.h"

void CAABBTree::SetParams(int nMinTrisPerNode, int nMaxTrisPerNode, float skipdim, const Matrix33 &Basis, int planeOptimisation)
{
	m_nMinTrisPerNode = nMinTrisPerNode; m_nMaxTrisPerNode = nMaxTrisPerNode;
	m_maxSkipDim = skipdim;
	m_Basis = Basis;
	m_bOriented = m_Basis.IsIdentity()^1;
	m_axisSplitMask = planeOptimisation ? 0 : -1;
}

float CAABBTree::Build(CGeometry *pMeshIn)
{
	CTriMesh* pMesh = (CTriMesh*)pMeshIn;
	m_pMesh = (CTriMesh*)pMeshIn;
	const int nTris = pMesh->m_nTris;
	m_pNodes = new AABBnode[m_nNodesAlloc=1+nTris*2];
	memset(m_pNodes, 0, m_nNodesAlloc * sizeof(m_pNodes[0]));
	m_pTri2Node = new int[nTris];
	m_nMaxTrisInNode = 0;
	m_nNodes = 2;
	m_pNodes[0].ntris = m_pNodes[min(m_nNodesAlloc-1,1)].ntris = 0;

	Vec3 (*bbtri)[3] = (Vec3(*)[3]) new Vec3[nTris*3];
	Vec3 ptmin(ZERO),ptmax(ZERO),pt;
	if (nTris)
		ptmin = ptmax = m_Basis*pMesh->m_pVertices[pMesh->m_pIndices[0]];
	int i;
	
	// Calculate an aabb for each tri, upfront, including the basis transformation
	for(i=0; i<nTris; i++) {
		const int i0 = pMesh->m_pIndices[i*3+0];
		const int i1 = pMesh->m_pIndices[i*3+1];
		const int i2 = pMesh->m_pIndices[i*3+2];
		const Vec3 v0 = m_Basis * pMesh->m_pVertices[i0];
		const Vec3 v1 = m_Basis * pMesh->m_pVertices[i1];
		const Vec3 v2 = m_Basis * pMesh->m_pVertices[i2];
		const float minX = min(min(v0.x, v1.x), v2.x);
		const float minY = min(min(v0.y, v1.y), v2.y);
		const float minZ = min(min(v0.z, v1.z), v2.z);
		const float maxX = max(max(v0.x, v1.x), v2.x);
		const float maxY = max(max(v0.y, v1.y), v2.y);
		const float maxZ = max(max(v0.z, v1.z), v2.z);
		bbtri[i][0] = Vec3(minX, minY, minZ);
		bbtri[i][1] = Vec3(maxX, maxY, maxZ);
		bbtri[i][2] = (v0+v1+v2)*(1.0/3.0);
		ptmin.x = min(ptmin.x,minX); ptmax.x = max(ptmax.x,maxX);
		ptmin.y = min(ptmin.y,minY); ptmax.y = max(ptmax.y,maxY);
		ptmin.z = min(ptmin.z,minZ); ptmax.z = max(ptmax.z,maxZ);
	}

	m_size = (ptmax-ptmin)*0.5f;
	m_center = ((ptmax+ptmin)*0.5f)*m_Basis;
	float mindim = max(max(m_size.x,m_size.y),m_size.z);
	m_maxSkipDim *= mindim; mindim *= 0.001f;
	m_size.Set(max(m_size.x,mindim), max(m_size.y,mindim), max(m_size.z,mindim));

	if (m_axisSplitMask==0) {	// Enable plane optimisation, turn off smallest dimension for splitting
		const int minIdx = idxmin3(m_size); const int minIdx1 = dec_mod3[minIdx], minIdx2 = inc_mod3[minIdx];
		if ((10.f*m_size[minIdx])<m_size[minIdx1] && (10.f*m_size[minIdx])<m_size[minIdx2])
			m_axisSplitMask = minIdx1 | (minIdx2<<2);
		else
			m_axisSplitMask=-1;	// re-enable all axes
	}

	float volume = BuildNode(0, 0,nTris, m_Basis*m_center,m_size, bbtri, 0);

	if (m_nNodesAlloc>m_nNodes) {
		AABBnode *pNodes = m_pNodes;
		memcpy(m_pNodes = new AABBnode[m_nNodesAlloc=m_nNodes], pNodes, sizeof(m_pNodes[0])*m_nNodes);
		delete[] pNodes;
	}
	m_nBitsLog = m_nNodes<=256 ? 3 : 4;
	int *pNewTri2Node = new int[(nTris-1>>5-m_nBitsLog)+1];
	memset(pNewTri2Node,0,sizeof(int)*((nTris-1>>5-m_nBitsLog)+1));
	for(i=0;i<nTris;i++)
		pNewTri2Node[i>>5-m_nBitsLog] |= m_pTri2Node[i] << ((i&(1<<5-m_nBitsLog)-1)<<m_nBitsLog);
	delete[] m_pTri2Node;
	m_pTri2Node = pNewTri2Node;
	delete[] (Vec3*)bbtri;
	
	return volume*8+isneg(63-m_nMaxTrisInNode)*1E10f;
}


float CAABBTree::BuildNode(int iNode, int iTriStart,int nTris, Vec3 center,Vec3 size, Vec3 (*bbtri)[3], int nDepth)
{
	int i,j;
	Vec3 ptmin(VMAX),ptmax(VMIN),pt,rsize;
	Vec3 ptmin2(VMAX),ptmax2(VMIN);
	float mindim = max(max(size.x,size.y),size.z)*0.001f;

	for(i=iTriStart;i<(iTriStart+nTris);i++) {
		ptmin.x = min_safe(ptmin.x,bbtri[i][0].x); ptmax.x = max_safe(ptmax.x,bbtri[i][1].x);
		ptmin.y = min_safe(ptmin.y,bbtri[i][0].y); ptmax.y = max_safe(ptmax.y,bbtri[i][1].y);
		ptmin.z = min_safe(ptmin.z,bbtri[i][0].z); ptmax.z = max_safe(ptmax.z,bbtri[i][1].z);
	}

	ptmin += size-center; ptmax += size-center;

	if (size.x>mindim) {
		rsize.x = (0.5f*128)/size.x;
		m_pNodes[iNode].minx = float2int(min(max(ptmin.x*rsize.x-0.5f,0.0f),127.0f));
		m_pNodes[iNode].maxx = float2int(min(max(ptmax.x*rsize.x-0.5f,0.0f),127.0f));
		m_pNodes[iNode].minx = min((int)m_pNodes[iNode].minx,(int)m_pNodes[iNode].maxx);
		m_pNodes[iNode].maxx = max((int)m_pNodes[iNode].minx,(int)m_pNodes[iNode].maxx);
		ptmin.x = m_pNodes[iNode].minx*size.x*(2.0f/128);
		ptmax.x = (m_pNodes[iNode].maxx+1)*size.x*(2.0f/128);
		center.x += (ptmax.x+ptmin.x)*0.5f-size.x;
		size.x = (ptmax.x-ptmin.x)*0.5f;
	} else {
		m_pNodes[iNode].minx = 0; m_pNodes[iNode].maxx = 127;
	}
	if (size.y>mindim) {
		rsize.y = (0.5f*128)/size.y;
		m_pNodes[iNode].miny = float2int(min(max(ptmin.y*rsize.y-0.5f,0.0f),127.0f));
		m_pNodes[iNode].maxy = float2int(min(max(ptmax.y*rsize.y-0.5f,0.0f),127.0f));
		m_pNodes[iNode].miny = min((int)m_pNodes[iNode].miny,(int)m_pNodes[iNode].maxy);
		m_pNodes[iNode].maxy = max((int)m_pNodes[iNode].miny,(int)m_pNodes[iNode].maxy);
		ptmin.y = m_pNodes[iNode].miny*size.y*(2.0f/128);
		ptmax.y = (m_pNodes[iNode].maxy+1)*size.y*(2.0f/128);
		center.y += (ptmax.y+ptmin.y)*0.5f-size.y;
		size.y = (ptmax.y-ptmin.y)*0.5f;
	} else {
		m_pNodes[iNode].miny = 0; m_pNodes[iNode].maxy = 127;
	}
	if (size.z>mindim) {
		rsize.z = (0.5f*128)/size.z;
		m_pNodes[iNode].minz = float2int(min(max(ptmin.z*rsize.z-0.5f,0.0f),127.0f));
		m_pNodes[iNode].maxz = float2int(min(max(ptmax.z*rsize.z-0.5f,0.0f),127.0f));
		m_pNodes[iNode].minz = min((int)m_pNodes[iNode].minz,(int)m_pNodes[iNode].maxz);
		m_pNodes[iNode].maxz = max((int)m_pNodes[iNode].minz,(int)m_pNodes[iNode].maxz);
		ptmin.z = m_pNodes[iNode].minz*size.z*(2.0f/128);
		ptmax.z = (m_pNodes[iNode].maxz+1)*size.z*(2.0f/128);
		center.z += (ptmax.z+ptmin.z)*0.5f-size.z;
		size.z = (ptmax.z-ptmin.z)*0.5f;
	} else {
		m_pNodes[iNode].minz = 0; m_pNodes[iNode].maxz = 127;
	}
	//center += (ptmax+ptmin)*0.5f-size; size = (ptmax-ptmin)*0.5f;

	if ((nTris | m_maxDepth-2-nDepth>>31)<=m_nMaxTrisPerNode) {
		m_pNodes[iNode].ichild = iTriStart;
		m_pNodes[iNode].ntris = nTris;
		m_nMaxTrisInNode = max(m_nMaxTrisInNode, nTris);
		m_pNodes[iNode].bSingleColl = isneg(max(max(size.x,size.y),size.z)-m_maxSkipDim);
		for(i=iTriStart;i<iTriStart+nTris;i++)
			m_pTri2Node[i] = iNode;
		return size.GetVolume();
	}

#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_APPLE
	volatile // compiler bug workaround?
#endif
	int iAxis, allowedAxis=-1;
	int numtris[3],/*nTrisAx[3],*/iPart,iMode[3]={2,2,2},idx;
	float c, cx,xlim[2],bounds[3][2],diff[3],axdiff[3];
	Vec3 axis;

	if (m_axisSplitMask>=0) {	// As an optimisation, choose the largest dimension that is not blocked
		const int i0 = m_axisSplitMask&3, i1 = (m_axisSplitMask>>2)&3;
		allowedAxis = (size[i0] > size[i1]) ? i0 : i1;
	}

	for(iAxis=0;iAxis<3;iAxis++) {
		if (size[iAxis]<mindim || (negmask(-allowedAxis-1)&notzero(iAxis-allowedAxis))) { // ignore this axis if blocked, or size is too small
			axdiff[iAxis] = -1E10f; //nTrisAx[iAxis] = 0;
			continue;
		}
		axis = m_Basis.GetRow(iAxis); cx = center[iAxis];
		bounds[0][0]=bounds[1][0]=bounds[2][0] = -size[iAxis]; 
		bounds[0][1]=bounds[1][1]=bounds[2][1] = size[iAxis]; 
		numtris[0]=numtris[1]=numtris[2] = 0;
		for(i=iTriStart;i<iTriStart+nTris;i++) {
			/*for(j=0,c.zero(),xlim[0]=size[iAxis],xlim[1]=-size[iAxis]; j<3; j++) {
				c += m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+j]];
				x = axis*m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+j]]-cx;
				xlim[0] = min(xlim[0],x); xlim[1] = max(xlim[1],x);
			}*/
			xlim[0] = bbtri[i][0][iAxis] - cx;	// min
			xlim[1] = bbtri[i][1][iAxis] - cx;	// max
			c = bbtri[i][2][iAxis] - cx;
			if (xlim[1]>=0.f) { // mode j=0: group all triangles that are entirely below center
				bounds[0][1] = min(bounds[0][1],xlim[0]); numtris[0]++;
			} else
				bounds[0][0] = max(bounds[0][0],xlim[1]);
			if (xlim[0]>=0.f) { // mode j=1: group all triangles that are entirely above center
				bounds[1][1] = min(bounds[1][1],xlim[0]); numtris[1]++;
			} else
				bounds[1][0] = max(bounds[1][0],xlim[1]);
			if (c>=0.f) { // mode j=2: sort triangles basing on centroids only
				bounds[2][1] = min(bounds[2][1],xlim[0]); numtris[2]++;
			} else
				bounds[2][0] = max(bounds[2][0],xlim[1]);
		}
		for(i=0;i<3;i++) diff[i] = bounds[i][1]-bounds[i][0]-size[iAxis]*((isneg(numtris[i]-m_nMinTrisPerNode)|isneg(nTris-numtris[i]-m_nMinTrisPerNode))*8);
		iMode[iAxis] = idxmax3(diff); //nTrisAx[iAxis] = numtris[iMode[iAxis]];
		axdiff[iAxis] = diff[iMode[iAxis]]*size[dec_mod3[iAxis]]*size[inc_mod3[iAxis]];
	}

	iAxis = idxmax3(axdiff);
	axis = m_Basis.GetRow(iAxis); cx = center[iAxis];
	// Choose which type of triangle we are looking for 0->1, 1->0, 2->2
	iPart = iMode[iAxis]^1^(iMode[iAxis]>>1);

	for(i=j=iTriStart;i<iTriStart+nTris;i++) {
#if 0
		if ((unsigned)iAxis >= 3)
			OutputDebugString ("iAxis>=3!");
		else
		if ((unsigned)iMode[iAxis] >= 3)
			OutputDebugString("iMode[iAxis]>=3!");
#endif
		if ((bbtri[i][iPart][iAxis] - cx)<0.f) {
			// swap triangles
			Vec3 tmp0 = bbtri[i][0], tmp1 = bbtri[i][1], tmp2 = bbtri[i][2];
			bbtri[i][0] = bbtri[j][0]; bbtri[i][1] = bbtri[j][1]; bbtri[i][2] = bbtri[j][2];
			bbtri[j][0] = tmp0; bbtri[j][1] = tmp1; bbtri[j][2] = tmp2;
			idx=m_pMesh->m_pIndices[i*3+0]; m_pMesh->m_pIndices[i*3+0]=m_pMesh->m_pIndices[j*3+0]; m_pMesh->m_pIndices[j*3+0]=idx;
			idx=m_pMesh->m_pIndices[i*3+1]; m_pMesh->m_pIndices[i*3+1]=m_pMesh->m_pIndices[j*3+1]; m_pMesh->m_pIndices[j*3+1]=idx;
			idx=m_pMesh->m_pIndices[i*3+2]; m_pMesh->m_pIndices[i*3+2]=m_pMesh->m_pIndices[j*3+2]; m_pMesh->m_pIndices[j*3+2]=idx;
			if (m_pMesh->m_pIds) {
				idx=m_pMesh->m_pIds[i]; m_pMesh->m_pIds[i]=m_pMesh->m_pIds[j]; m_pMesh->m_pIds[j]=idx;
			}
			if (m_pMesh->m_pForeignIdx) {
				idx=m_pMesh->m_pForeignIdx[i]; m_pMesh->m_pForeignIdx[i]=m_pMesh->m_pForeignIdx[j]; m_pMesh->m_pForeignIdx[j]=idx;
			}
			if (m_pMesh->m_pIdxNew2Old) {
				idx=m_pMesh->m_pIdxNew2Old[i]; m_pMesh->m_pIdxNew2Old[i]=m_pMesh->m_pIdxNew2Old[j]; m_pMesh->m_pIdxNew2Old[j]=idx;
			}
			j++;
		}
	}
	j -= iTriStart;

	if (j<m_nMinTrisPerNode || j>nTris-m_nMinTrisPerNode) {
		m_pNodes[iNode].ichild = iTriStart;
		m_pNodes[iNode].ntris = nTris;
		m_nMaxTrisInNode = max(m_nMaxTrisInNode, nTris);
		m_pNodes[iNode].bSingleColl = isneg(max(max(size.x,size.y),size.z)-m_maxSkipDim);
		for(i=iTriStart;i<iTriStart+nTris;i++)
			m_pTri2Node[i] = iNode;
		return size.GetVolume();
	}

	// proceed with the children
	if (m_nNodes+2 > m_nNodesAlloc) {
		AABBnode *pNodes = m_pNodes;
		memcpy(m_pNodes = new AABBnode[m_nNodesAlloc+=32], pNodes, m_nNodes*sizeof(AABBnode));
		delete[] pNodes;
	}

	m_pNodes[iNode].ichild = m_nNodes;
	m_pNodes[iNode].bSingleColl = 0;
	NO_BUFFER_OVERRUN
	m_pNodes[m_nNodes].ntris = m_pNodes[m_nNodes+1].ntris = 0;
	iNode = m_nNodes;	m_nNodes += 2;
	float volume = 0;
	volume += BuildNode(iNode+1, iTriStart+j,nTris-j, center,size, bbtri, nDepth+1);
	volume += BuildNode(iNode, iTriStart,j, center,size, bbtri, nDepth+1);
	return volume;
}

void CAABBTree::SetGeomConvex()
{
	for(int i=0;i<m_nNodes;i++)
		m_pNodes[i].bSingleColl = 1;
}


void CAABBTree::GetBBox(box *pbox)
{
	pbox->Basis = m_Basis;
	pbox->bOriented = m_bOriented;
	pbox->center = m_center;
	pbox->size = m_size;
}

void CAABBTree::GetNodeBV(BV *&pBV,int iNode, int iCaller)
{
	pBV = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	pBV->type = box::type;
	((BBox*)pBV)->iNode = 0;
	((BBox*)pBV)->abox.Basis = m_Basis;
	((BBox*)pBV)->abox.bOriented = m_bOriented;
	GetRootNodeDim(((BBox*)pBV)->abox.center, ((BBox*)pBV)->abox.size);
}

void CAABBTree::GetNodeBV(BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode, int iCaller)
{
	pBV = g_BBoxExtBuf; g_BBoxExtBufPos = 0;
	pBV->type = box::type;
	((BBoxExt*)pBV)->iNode = 0;
	box &boxstatic = ((BBoxExt*)pBV)->aboxStatic;
	boxstatic.Basis = m_Basis;
	boxstatic.bOriented = m_bOriented;
	GetRootNodeDim(boxstatic.center, boxstatic.size);
	ExtrudeBox(&boxstatic, sweepdir,sweepstep, &((BBoxExt*)pBV)->abox);
}

void CAABBTree::GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode, int iCaller)
{
	pBV = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	pBV->type = box::type;
	pBV->iNode = 0;
	((BBox*)pBV)->abox.Basis = m_Basis*Rw.T();
	((BBox*)pBV)->abox.bOriented = 1;
	Vec3 center,size;
	GetRootNodeDim(center,size);
	((BBox*)pBV)->abox.center = Rw*center*scalew + offsw;
	((BBox*)pBV)->abox.size = size*scalew;
}


float CAABBTree::SplitPriority(const BV* pBV)
{
	BBox *pbox = (BBox*)pBV;
	return pbox->abox.size.GetVolume()*(m_pNodes[pbox->iNode].ntris-1>>31 & 1);
}

void CAABBTree::GetNodeChildrenBVs(const Matrix33 &Rw,const Vec3 &offsw,float scalew, 
																	 const BV *pBV_parent, BV *&pBV_child1,BV *&pBV_child2, int iCaller)
{
	BBox * boxBuf = g_BBoxBuf;
	BBox * bbox_parent=(BBox*)pBV_parent;
	BBox * __restrict bbox_child1;
	BBox * __restrict bbox_child2;

	bbox_child1 = boxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	bbox_child2 = boxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);

	const int ichild = m_pNodes[bbox_parent->iNode].ichild;

	bbox_child1->iNode = ichild;
	bbox_child2->iNode = ichild	+ 1;
	bbox_child1->type = bbox_child2->type = box::type;
	bbox_child1->abox.Basis = bbox_parent->abox.Basis; bbox_child2->abox.Basis = bbox_parent->abox.Basis;
	bbox_child1->abox.bOriented = bbox_child2->abox.bOriented = 1;

	const Vec3 &size=bbox_parent->abox.size, &center=bbox_parent->abox.center;

	Vec3 ptmax,ptmin;
	ptmin.x = m_pNodes[ichild].minx*size.x*(2.0/128);
	ptmax.x = (m_pNodes[ichild].maxx+1)*size.x*(2.0/128);
	ptmin.y = m_pNodes[ichild].miny*size.y*(2.0/128);
	ptmax.y = (m_pNodes[ichild].maxy+1)*size.y*(2.0/128);
	ptmin.z = m_pNodes[ichild].minz*size.z*(2.0/128);
	ptmax.z = (m_pNodes[ichild].maxz+1)*size.z*(2.0/128);
	bbox_child1->abox.center = center+((ptmax+ptmin)*0.5f-size)*bbox_parent->abox.Basis;
	bbox_child1->abox.size = (ptmax-ptmin)*0.5f;

	ptmin.x = m_pNodes[ichild+1].minx*size.x*(2.0/128);
	ptmax.x = (m_pNodes[ichild+1].maxx+1)*size.x*(2.0/128);
	ptmin.y = m_pNodes[ichild+1].miny*size.y*(2.0/128);
	ptmax.y = (m_pNodes[ichild+1].maxy+1)*size.y*(2.0/128);
	ptmin.z = m_pNodes[ichild+1].minz*size.z*(2.0/128);
	ptmax.z = (m_pNodes[ichild+1].maxz+1)*size.z*(2.0/128);
	bbox_child2->abox.center = center+((ptmax+ptmin)*0.5f-size)*bbox_parent->abox.Basis;
	bbox_child2->abox.size = (ptmax-ptmin)*0.5f;

	// Doing this at the end eliminates any pointer
	// aliasing and allows the compiler to optimise correctly
	pBV_child1 = (BV*)bbox_child1;
	pBV_child2 = (BV*)bbox_child2;
}

void CAABBTree::GetNodeChildrenBVs(const BV *pBV_parent, BV *&pBV_child1,BV *&pBV_child2, int iCaller)
{
	BBox * boxBuf = g_BBoxBuf;
	BBox * bbox_parent=(BBox*)pBV_parent;
	BBox * __restrict bbox_child1;
	BBox * __restrict bbox_child2;
	
	bbox_child1 = boxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	bbox_child2 = boxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);

	const int ichild = m_pNodes[bbox_parent->iNode].ichild;

	bbox_child1->iNode = ichild;
	bbox_child2->iNode = ichild	+ 1;

	bbox_child1->type = bbox_child2->type = box::type;
	if (bbox_child1->abox.bOriented = bbox_child2->abox.bOriented = bbox_parent->abox.bOriented) {
		bbox_child1->abox.Basis = bbox_parent->abox.Basis; 
		bbox_child2->abox.Basis = bbox_parent->abox.Basis;
	}

	const Vec3 &size=bbox_parent->abox.size, &center=bbox_parent->abox.center;

	Vec3 ptmax,ptmin;
	ptmin.x = m_pNodes[ichild].minx*size.x*(2.0/128);
	ptmax.x = (m_pNodes[ichild].maxx+1)*size.x*(2.0/128);
	ptmin.y = m_pNodes[ichild].miny*size.y*(2.0/128);
	ptmax.y = (m_pNodes[ichild].maxy+1)*size.y*(2.0/128);
	ptmin.z = m_pNodes[ichild].minz*size.z*(2.0/128);
	ptmax.z = (m_pNodes[ichild].maxz+1)*size.z*(2.0/128);
	if (bbox_parent->abox.bOriented)
		bbox_child1->abox.center = center+((ptmax+ptmin)*0.5f-size)*bbox_parent->abox.Basis;
	else
		bbox_child1->abox.center = center+((ptmax+ptmin)*0.5f-size);
	bbox_child1->abox.size = (ptmax-ptmin)*0.5f;

	ptmin.x = m_pNodes[ichild+1].minx*size.x*(2.0/128);
	ptmax.x = (m_pNodes[ichild+1].maxx+1)*size.x*(2.0/128);
	ptmin.y = m_pNodes[ichild+1].miny*size.y*(2.0/128);
	ptmax.y = (m_pNodes[ichild+1].maxy+1)*size.y*(2.0/128);
	ptmin.z = m_pNodes[ichild+1].minz*size.z*(2.0/128);
	ptmax.z = (m_pNodes[ichild+1].maxz+1)*size.z*(2.0/128);
	if (bbox_parent->abox.bOriented)
		bbox_child2->abox.center = center+((ptmax+ptmin)*0.5f-size)*bbox_parent->abox.Basis;
	else
		bbox_child2->abox.center = center+((ptmax+ptmin)*0.5f-size);
	bbox_child2->abox.size = (ptmax-ptmin)*0.5f;

	// Doing this at the end eliminates any pointer
	// aliasing and allows the compiler to optimise correctly
	pBV_child1 = (BV*)bbox_child1;
	pBV_child2 = (BV*)bbox_child2;
}

void CAABBTree::GetNodeChildrenBVs(const BV *pBV_parent, const Vec3 &sweepdir,float sweepstep, BV *&pBV_child1,BV *&pBV_child2, int iCaller)
{
	BBoxExt * boxBuf = g_BBoxExtBuf;
	BBoxExt * bbox_parent=(BBoxExt*)pBV_parent;
	BBoxExt * __restrict bbox_child1;
	BBoxExt * __restrict bbox_child2;

	bbox_child1 = boxBuf + min(g_BBoxExtBufPos++, k_BBoxExtBufSize-1);
	bbox_child2 = boxBuf + min(g_BBoxExtBufPos++, k_BBoxExtBufSize-1);

	const int ichild = m_pNodes[bbox_parent->iNode].ichild;
	bbox_child1->iNode = ichild;
	bbox_child2->iNode = ichild	+ 1;

	bbox_child1->type = bbox_child2->type = box::type;
	bbox_child1->aboxStatic.bOriented = bbox_child2->aboxStatic.bOriented = bbox_parent->aboxStatic.bOriented;
	bbox_child1->aboxStatic.Basis = bbox_parent->aboxStatic.Basis; 
	bbox_child2->aboxStatic.Basis = bbox_parent->aboxStatic.Basis;

	const Vec3 &size=bbox_parent->aboxStatic.size, &center=bbox_parent->aboxStatic.center;

	Vec3 ptmax,ptmin;
	ptmin.x = m_pNodes[ichild].minx*size.x*(2.0/128);
	ptmax.x = (m_pNodes[ichild].maxx+1)*size.x*(2.0/128);
	ptmin.y = m_pNodes[ichild].miny*size.y*(2.0/128);
	ptmax.y = (m_pNodes[ichild].maxy+1)*size.y*(2.0/128);
	ptmin.z = m_pNodes[ichild].minz*size.z*(2.0/128);
	ptmax.z = (m_pNodes[ichild].maxz+1)*size.z*(2.0/128);
	if (bbox_parent->aboxStatic.bOriented)
		bbox_child1->aboxStatic.center = center+((ptmax+ptmin)*0.5f-size)*bbox_parent->aboxStatic.Basis;
	else
		bbox_child1->aboxStatic.center = center+((ptmax+ptmin)*0.5f-size);
	bbox_child1->aboxStatic.size = (ptmax-ptmin)*0.5f;


	ptmin.x = m_pNodes[ichild+1].minx*size.x*(2.0/128);
	ptmax.x = (m_pNodes[ichild+1].maxx+1)*size.x*(2.0/128);
	ptmin.y = m_pNodes[ichild+1].miny*size.y*(2.0/128);
	ptmax.y = (m_pNodes[ichild+1].maxy+1)*size.y*(2.0/128);
	ptmin.z = m_pNodes[ichild+1].minz*size.z*(2.0/128);
	ptmax.z = (m_pNodes[ichild+1].maxz+1)*size.z*(2.0/128);
	if (bbox_parent->abox.bOriented)
		bbox_child2->aboxStatic.center = center+((ptmax+ptmin)*0.5f-size)*bbox_parent->aboxStatic.Basis;
	else
		bbox_child2->aboxStatic.center = center+((ptmax+ptmin)*0.5f-size);
	bbox_child2->aboxStatic.size = (ptmax-ptmin)*0.5f;

	ExtrudeBox(&bbox_child1->aboxStatic, sweepdir,sweepstep, &bbox_child1->abox);
	bbox_child1->abox.bOriented = 1+bbox_child1->iNode;
	ExtrudeBox(&bbox_child2->aboxStatic, sweepdir,sweepstep, &bbox_child2->abox);
	bbox_child2->abox.bOriented = 1+bbox_child2->iNode;

	// Doing this at the end eliminates any pointer
	// aliasing and allows the compiler to optimise correctly
	pBV_child1 = (BV*)bbox_child1;
	pBV_child2 = (BV*)bbox_child2;
}

void CAABBTree::ReleaseLastBVs(int iCaller)
{
	g_BBoxBufPos-=2;
}

void CAABBTree::ReleaseLastSweptBVs(int iCaller)
{
	g_BBoxExtBufPos-=2;
}

int CAABBTree::GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
															 geometry_under_test *pGTest,geometry_under_test *pGTestOp)
{
	return m_pMesh->GetPrimitiveList(m_pNodes[iNode].ichild,m_pNodes[iNode].ntris, pBVCollider->type,*pBVCollider,bColliderLocal, pGTest,pGTestOp,
		pGTest->primbuf,pGTest->idbuf);
}


void CAABBTree::MarkUsedTriangle(int itri, geometry_under_test *pGTest)
{
	if (!pGTest->pUsedNodesMap) return;
	int iNode = m_pTri2Node[itri>>5-m_nBitsLog]>>((itri&(1<<5-m_nBitsLog)-1)<<m_nBitsLog) & (1<<(1<<m_nBitsLog))-1;
	if (m_pNodes[iNode].bSingleColl) {
		pGTest->pUsedNodesMap[iNode>>5] |= 1<<(iNode&31);
		pGTest->pUsedNodesIdx[pGTest->nUsedNodes = min(pGTest->nUsedNodes+1,pGTest->nMaxUsedNodes-1)] = iNode;
		pGTest->bCurNodeUsed = 1;
	}
}


int CAABBTree::PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl)
{
	if (m_maxSkipDim<=0 || m_pMesh->m_nTris<=16) {
		pGTest->pUsedNodesMap = 0;
		pGTest->pUsedNodesIdx = 0;
		pGTest->nMaxUsedNodes = 0;
	} else {
		int mapsz = (m_nNodes-1>>5) + 1;
		if (mapsz<=(int)(CRY_ARRAY_COUNT(g_UsedNodesMap))-g_UsedNodesMapPos) {
			pGTest->pUsedNodesMap = g_UsedNodesMap+g_UsedNodesMapPos; g_UsedNodesMapPos += mapsz;
		} else {
			pGTest->pUsedNodesMap = new int[mapsz];
    }
		pGTest->pUsedNodesIdx = g_UsedNodesIdx+g_UsedNodesIdxPos;
		pGTest->nMaxUsedNodes = min(32,(int)(CRY_ARRAY_COUNT(g_UsedNodesIdx)-g_UsedNodesIdxPos));
		g_UsedNodesIdxPos += pGTest->nMaxUsedNodes;
	}
	pGTest->nUsedNodes = -1; pGTest->pUsedNodesMap = 0;
	return 1;
}

void CAABBTree::CleanupAfterIntersectionTest(geometry_under_test *pGTest)
{
	if (!pGTest->pUsedNodesMap)
		return;
	if ((unsigned int)(pGTest->pUsedNodesMap-g_UsedNodesMap) > (unsigned int)CRY_ARRAY_COUNT(g_UsedNodesMap)) {
		delete[] pGTest->pUsedNodesMap; return;
	}
	if (pGTest->nUsedNodes < pGTest->nMaxUsedNodes-1) {
		for(int i=0;i<=pGTest->nUsedNodes;i++) 
			pGTest->pUsedNodesMap[pGTest->pUsedNodesIdx[i]>>5] &= ~(1<<(pGTest->pUsedNodesIdx[i]&31));
	} else
		memset(pGTest->pUsedNodesMap, 0, ((m_nNodes-1>>5)+1)*4);
}


void CAABBTree::GetMemoryStatistics(ICrySizer *pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "AABB trees");
	pSizer->AddObject(this, sizeof(CAABBTree));
	pSizer->AddObject(m_pNodes, m_nNodesAlloc*sizeof(m_pNodes[0]));
	if (m_pTri2Node)
		pSizer->AddObject(m_pTri2Node, ((m_pMesh->m_nTris-1>>5-m_nBitsLog)+1)*sizeof(int));
}

const int AABB_VER = 1;

void CAABBTree::Save(CMemStream &stm)
{
	stm.Write(-AABB_VER);
	stm.Write(m_nNodes);
	stm.Write(m_pNodes, m_nNodes*sizeof(m_pNodes[0]));
	stm.Write(m_center);
	stm.Write(m_size);
	stm.Write(m_Basis);
	stm.Write(m_nMaxTrisInNode);
	stm.Write(m_nMinTrisPerNode);
	stm.Write(m_nMaxTrisPerNode);
	stm.Write(m_maxSkipDim);
}

void CAABBTree::Load(CMemStream &stm, CGeometry *pGeom)
{
	m_pMesh = (CTriMesh*)pGeom;
	int i,ver; stm.Read(ver);
	if (ver>=0)
		m_nNodes = ver;
	else
		stm.Read(m_nNodes);
	m_nBitsLog = m_nNodes<=256 ? 3 : 4;
	m_pNodes = new AABBnode[m_nNodesAlloc=m_nNodes];
	if (ver==-AABB_VER)
		stm.ReadType(m_pNodes, m_nNodes);
	else if (ver>=0) {
		AABBnodeV0 *pNodes = new AABBnodeV0[m_nNodes];
		stm.ReadType(pNodes, m_nNodes);
		for(i=0;i<m_nNodes;i++) {
			m_pNodes[i].ichild = pNodes[i].ichild;
			m_pNodes[i].minx = pNodes[i].minx;
			m_pNodes[i].maxx = pNodes[i].maxx;
			m_pNodes[i].miny = pNodes[i].miny;
			m_pNodes[i].maxy = pNodes[i].maxy;
			m_pNodes[i].minz = pNodes[i].minz;
			m_pNodes[i].maxz = pNodes[i].maxz;
			m_pNodes[i].ntris = pNodes[i].ntris;
			m_pNodes[i].bSingleColl = pNodes[i].bSingleColl;	
		}
		delete[] pNodes;
	}
	m_pTri2Node = new int[(m_pMesh->m_nTris-1>>5-m_nBitsLog)+1];
	assert(m_pMesh); // placed here just to suppress a warning from the code analyzer
	memset(m_pTri2Node,0,sizeof(int)*((m_pMesh->m_nTris-1>>5-m_nBitsLog)+1));
	for(i=0;i<m_nNodes;i++)
	{
		PREFAST_ASSUME(i > 0 && i < m_nNodes);;
		for(int j=0;j<(int)m_pNodes[i].ntris;j++)
		{
			m_pTri2Node[m_pNodes[i].ichild+j>>5-m_nBitsLog] |= i << ((m_pNodes[i].ichild+j&(1<<5-m_nBitsLog)-1)<<m_nBitsLog);
		}
	}

	stm.Read(m_center);
	stm.Read(m_size);
	stm.Read(m_Basis);
	m_bOriented = m_Basis.IsIdentity()^1;

	stm.Read(m_nMaxTrisInNode);
	stm.Read(m_nMinTrisPerNode);
	stm.Read(m_nMaxTrisPerNode);
	stm.Read(m_maxSkipDim);
}

int CAABBTree::SanityCheck()
{
	int iCaller = MAX_PHYS_THREADS;
	const int maxDepth1 = (CRY_ARRAY_COUNT(g_BBoxBuf)-1)/4;
	const int maxDepth2 = (CRY_ARRAY_COUNT(g_BBoxExtBuf)-1)/2;
	return SanityCheckTree(this, min(maxDepth1, maxDepth2));
}
