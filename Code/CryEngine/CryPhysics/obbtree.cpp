// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "bvtree.h"
#include "geometry.h"
#include "obbtree.h"
#include "trimesh.h"

void COBBTree::SetParams(int nMinTrisPerNode, int nMaxTrisPerNode, float skipdim)
{
	m_nMinTrisPerNode = nMinTrisPerNode; m_nMaxTrisPerNode = nMaxTrisPerNode;
	m_maxSkipDim = skipdim;
}

float COBBTree::Build(CGeometry *pMesh)
{
	m_pMesh = (CTriMesh*)pMesh;
	m_pNodes = new OBBnode[m_nNodesAlloc=256];
	memset(m_pMapVtxUsed = new int[(m_pMesh->m_nVertices-1>>5)+1], 0, (m_pMesh->m_nVertices-1>>3)+1);
	m_pVtxUsed = new Vec3[m_pMesh->m_nVertices];
	m_pTri2Node = new index_t[m_pMesh->m_nTris];
	m_nMaxTrisInNode = 0;
	m_nNodes = 2;
	memset(&m_pNodes[0], 0, m_nNodes * sizeof(m_pNodes[0]));
	m_pNodes[0].iparent = m_pNodes[1].iparent = -1;

	float volume = BuildNode(0, 0,m_pMesh->m_nTris, 0);

	if (m_nNodesAlloc>m_nNodes) {
		OBBnode *pNodes = m_pNodes;
		memcpy(m_pNodes = new OBBnode[m_nNodesAlloc=m_nNodes], pNodes, sizeof(OBBnode)*m_nNodes);
		delete[] pNodes;
	}
	delete[] m_pMapVtxUsed;
	delete[] m_pVtxUsed;

	m_maxSkipDim *= max(max(m_pNodes[0].size.x,m_pNodes[0].size.y),m_pNodes[0].size.z);
	return volume*8;
}

float COBBTree::BuildNode(int iNode, int iTriStart,int nTris, int nDepth)
{
	int nHullTris,i,j,nPts,iStart=1<<30,iEnd=-1,idx0=iTriStart*3,nidx=nTris*3;
	index_t *pHullTris=0;

	// calculate convex hull of vertices
	//	mark all vertices used by this set of triangles
	for(i=0; i<nidx; i++) {
		m_pMapVtxUsed[m_pMesh->m_pIndices[idx0+i]>>5] |= 1<<(m_pMesh->m_pIndices[idx0+i]&31);
		iStart = min(iStart, (int)m_pMesh->m_pIndices[idx0+i]);
		iEnd = max(iEnd, (int)m_pMesh->m_pIndices[idx0+i]);
	}
	//	group these vertices in one array
	for(i=iStart,nPts=0; i<=iEnd; i++) if (m_pMapVtxUsed[i>>5] & 1<<(i&31)) {
		m_pVtxUsed[nPts++]=m_pMesh->m_pVertices[i]; m_pMapVtxUsed[i>>5] &= ~(1<<(i&31));
	}
	//	give these vertices to qhull routine
	if (nDepth>4 || nTris<=100 || !(nHullTris = qhull(m_pVtxUsed,nPts, pHullTris))) {
		nHullTris=nTris; pHullTris=m_pMesh->m_pIndices+idx0;
	}

	Vec3r axes[3],mean; 
	Matrix33r Basis; 

	if (nHullTris) {
		ComputeMeshEigenBasis(m_pMesh->m_pVertices,strided_pointer<index_t>(pHullTris),nHullTris, axes,mean);
		Basis = GetMtxFromBasis(axes);
	} else // convex hull not computed. probably we have some degenerate case, so use AABB
		Basis.SetIdentity();

	if (pHullTris && pHullTris!=m_pMesh->m_pIndices+idx0) 
		delete[] pHullTris;

	// calculate bounding box center and dimensions
	Vec3r pt,ptmin(VMAX),ptmax(VMIN);
	for(i=0;i<nPts;i++) {
		pt = Basis*m_pVtxUsed[i];
		for(j=0;j<3;j++) {
			ptmin[j] = min_safe(ptmin[j], pt[j]);
			ptmax[j] = max_safe(ptmax[j], pt[j]);
		}
	}
	if (iNode==0) {
		Vec3 ptminAA,ptmaxAA;	ptminAA=ptmaxAA = m_pVtxUsed[0];
		for(i=1;i<nPts;i++)
			ptminAA=min(ptminAA,m_pVtxUsed[i]), ptmaxAA=max(ptmaxAA,m_pVtxUsed[i]);
		if ((ptmaxAA-ptminAA).GetVolume()<(ptmax-ptmin).GetVolume()) {
			Basis.SetIdentity(); ptmin=ptminAA; ptmax=ptmaxAA;
		}
	}
	SetBasisFromMtx(m_pNodes[iNode].axes, Basis);
	m_pNodes[iNode].size = (ptmax-ptmin)*0.5f;
	m_pNodes[iNode].center = ((ptmax+ptmin)*(real)0.5)*Basis;
	float mindim = max(max(m_pNodes[iNode].size.x,m_pNodes[iNode].size.y),m_pNodes[iNode].size.z)*0.001f;
	for(i=0;i<3;i++)
		m_pNodes[iNode].size[i] = max(mindim, m_pNodes[iNode].size[i]);

	if (nTris<=m_nMaxTrisPerNode) {
		m_pNodes[iNode].ichild = iTriStart;
		m_pNodes[iNode].ntris = nTris;
		m_nMaxTrisInNode = max(m_nMaxTrisInNode, nTris);
		for(i=iTriStart;i<iTriStart+nTris;i++)
			m_pTri2Node[i] = iNode;
		return m_pNodes[iNode].size.GetVolume();
	}

	// separate geometry into two parts
#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_APPLE
	volatile // compiler bug workaround?
#endif
	int iAxis;
	int numtris[3],/*nTrisAx[3],*/iPart,iMode[3],idx;
	float x0,x1,x2,cx,sz,xlim[2],bounds[3][2],diff[3],axdiff[3];
	Vec3 axis,center;

	for(iAxis=0;iAxis<3;iAxis++) {
		sz = m_pNodes[iNode].size[iAxis];
		if (sz<mindim*10) {
			axdiff[iAxis] = -1E10f; //nTrisAx[iAxis] = 0;
			continue;
		}
		axis = m_pNodes[iNode].axes[iAxis];
		cx = axis*m_pNodes[iNode].center;
		bounds[0][0]=bounds[1][0]=bounds[2][0] = -sz; 
		bounds[0][1]=bounds[1][1]=bounds[2][1] = sz; 
		numtris[0]=numtris[1]=numtris[2] = 0;
		for(i=iTriStart;i<iTriStart+nTris;i++) {
			center = m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3]]+m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+1]]+
							 m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+2]];
			x0 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+0]]*axis-cx;
			x1 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+1]]*axis-cx;
			x2 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+2]]*axis-cx;
			xlim[0] = min(min(x0,x1),x2); xlim[1] = max(max(x0,x1),x2);
			/*for(j=0,center.zero(),xlim[0]=sz,xlim[1]=-sz;j<3;j++) {
				center += m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+j]];
				x = axis*m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+j]]-cx;
				xlim[0] = min(xlim[0],x); xlim[1] = max(xlim[1],x);
			}*/
			iPart = isneg(xlim[1])^1; // mode0: group all triangles that are entirely below center
			bounds[0][iPart] = minmax(bounds[0][iPart],xlim[iPart^1],iPart^1); numtris[0]+=iPart;
			iPart = isnonneg(xlim[0]); // mode1: group all triangles that are entirely above center
			bounds[1][iPart] = minmax(bounds[1][iPart],xlim[iPart^1],iPart^1); numtris[1]+=iPart;
			iPart = isnonneg(((center*axis)*(1.0f/3)-cx)); // mode2: sort triangles basing on centroids only
			bounds[2][iPart] = minmax(bounds[2][iPart],xlim[iPart^1],iPart^1); numtris[2]+=iPart;
		}
		for(i=0;i<3;i++) diff[i] = bounds[i][1]-bounds[i][0]-sz*(isneg(numtris[i]-m_nMinTrisPerNode)|isneg(nTris-numtris[i]-m_nMinTrisPerNode));
		iMode[iAxis] = idxmax3(diff); //nTrisAx[iAxis] = numtris[iMode[iAxis]];
		axdiff[iAxis] = diff[iMode[iAxis]]*m_pNodes[iNode].size[dec_mod3[iAxis]]*m_pNodes[iNode].size[inc_mod3[iAxis]];
	}

	iAxis = idxmax3(axdiff);
	axis = m_pNodes[iNode].axes[iAxis];
	cx = axis*m_pNodes[iNode].center;
	for(i=j=iTriStart;i<iTriStart+nTris;i++) {
		x0 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+0]]*axis-cx;
		x1 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+1]]*axis-cx;
		x2 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+2]]*axis-cx;

#if 0
		if ((unsigned)iAxis >= 3)
			OutputDebugString ("iAxis>=3!");
		else
			if ((unsigned)iMode[iAxis] >= 3)
				OutputDebugString("iMode[iAxis]>=3!");
#endif

		switch(iMode[iAxis]) {
			case 0: iPart = isneg(max(max(x0,x1),x2))^1; break;
			case 1: iPart = isnonneg(min(min(x0,x1),x2)); break;
			case 2: iPart = isnonneg(x0+x1+x2);
		}
		if (iPart==0) {
			// swap triangles
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
		for(i=iTriStart;i<iTriStart+nTris;i++)
			m_pTri2Node[i] = iNode;
		return m_pNodes[iNode].size.GetVolume();
	}

	// proceed with the children
	if (m_nNodes+2 > m_nNodesAlloc) {
		OBBnode *pNodes = m_pNodes;
		memcpy(m_pNodes = new OBBnode[m_nNodesAlloc+=256], pNodes, m_nNodes*sizeof(OBBnode));
		delete[] pNodes;
	}

	m_pNodes[iNode].ichild = m_nNodes;
	m_pNodes[m_nNodes].iparent = m_pNodes[m_nNodes+1].iparent = iNode;
	m_pNodes[m_nNodes].ntris = m_pNodes[m_nNodes+1].ntris = 0;
	iNode = m_nNodes;	m_nNodes += 2;
	float res = BuildNode(iNode+1, iTriStart+j,nTris-j, nDepth+1);
	res += BuildNode(iNode, iTriStart,j, nDepth+1);
	return res;
}

void COBBTree::SetGeomConvex()
{
	m_maxSkipDim = (m_pNodes[0].size.x+m_pNodes[0].size.y+m_pNodes[0].size.z)*10.0f;
}


void COBBTree::GetBBox(box *pbox)
{
	pbox->Basis = GetMtxFromBasis(m_pNodes[0].axes);
	pbox->bOriented = 1;
	pbox->center = m_pNodes[0].center;
	pbox->size = m_pNodes[0].size;
}

void COBBTree::GetNodeBV(BV *&pBV,int iNode, int iCaller)
{
	iNode = min(iNode, m_nNodes-1);
	pBV = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	pBV->type = box::type;
	((BBox*)pBV)->iNode = iNode;
	((BBox*)pBV)->abox.Basis = GetMtxFromBasis(m_pNodes[iNode].axes);
	((BBox*)pBV)->abox.bOriented = 1;
	((BBox*)pBV)->abox.center = m_pNodes[iNode].center;
	((BBox*)pBV)->abox.size = m_pNodes[iNode].size;
}

void COBBTree::GetNodeBV(BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode, int iCaller)
{
	iNode = min(iNode, m_nNodes-1);
	pBV = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	pBV->type = box::type;
	((BBox*)pBV)->iNode = iNode;
	box boxstatic;
	boxstatic.Basis = GetMtxFromBasis(m_pNodes[iNode].axes);
	boxstatic.bOriented = 1;
	boxstatic.center = m_pNodes[iNode].center;
	boxstatic.size = m_pNodes[iNode].size;
	ExtrudeBox(&boxstatic, sweepdir,sweepstep, &((BBox*)pBV)->abox);
}

void COBBTree::GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode, int iCaller)
{
	iNode = min(iNode, m_nNodes-1);
	pBV = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	pBV->type = box::type;
	pBV->iNode = iNode;
	((BBox*)pBV)->abox.Basis = GetMtxFromBasis(m_pNodes[iNode].axes)*Rw.T();
	((BBox*)pBV)->abox.bOriented = 1;
	((BBox*)pBV)->abox.center = Rw*m_pNodes[iNode].center*scalew + offsw;
	((BBox*)pBV)->abox.size = m_pNodes[iNode].size*scalew;
}

float COBBTree::SplitPriority(const BV* pBV)
{
	BBox *pbox = (BBox*)pBV;
	return pbox->abox.size.GetVolume()*(m_pNodes[pbox->iNode].ntris-1>>31 & 1);
}

void COBBTree::GetNodeChildrenBVs(const Matrix33 &Rw,const Vec3 &offsw,float scalew, 
																	const BV *pBV_parent, BV *&pBV_child1,BV *&pBV_child2, int iCaller)
{
	BBox *bbox_parent=(BBox*)pBV_parent, *&bbox_child1=(BBox*&)pBV_child1, *&bbox_child2=(BBox*&)pBV_child2;
	bbox_child1 = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	bbox_child2 = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	bbox_child2->iNode = (bbox_child1->iNode = m_pNodes[bbox_parent->iNode].ichild)+1;
	bbox_child1->type = bbox_child2->type = box::type;

	OBBnode *pNode = m_pNodes + bbox_child1->iNode;
	bbox_child1->abox.Basis = GetMtxFromBasis(pNode->axes)*Rw.T();
	bbox_child1->abox.bOriented = 1+bbox_child1->iNode;
	bbox_child1->abox.center = Rw*pNode->center*scalew + offsw;
	bbox_child1->abox.size = pNode++->size*scalew;
	
	bbox_child2->abox.Basis = GetMtxFromBasis(pNode->axes)*Rw.T();
	bbox_child2->abox.bOriented = 1+bbox_child2->iNode;
	bbox_child2->abox.center = Rw*pNode->center*scalew + offsw;
	bbox_child2->abox.size = pNode->size*scalew;
}

void COBBTree::GetNodeChildrenBVs(const BV *pBV_parent, BV *&pBV_child1,BV *&pBV_child2, int iCaller)
{
	BBox *bbox_parent=(BBox*)pBV_parent, *&bbox_child1=(BBox*&)pBV_child1, *&bbox_child2=(BBox*&)pBV_child2;
	bbox_child1 = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	bbox_child2 = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	bbox_child2->iNode = (bbox_child1->iNode = m_pNodes[bbox_parent->iNode].ichild)+1;
	bbox_child1->type = bbox_child2->type = box::type;

	OBBnode *pNode = m_pNodes + bbox_child1->iNode;
	bbox_child1->abox.Basis = GetMtxFromBasis(pNode->axes);
	bbox_child1->abox.bOriented = 1+bbox_child1->iNode;
	bbox_child1->abox.center = pNode->center;
	bbox_child1->abox.size = pNode++->size;

	bbox_child2->abox.Basis = GetMtxFromBasis(pNode->axes);
	bbox_child2->abox.bOriented = 1+bbox_child2->iNode;
	bbox_child2->abox.center = pNode->center;
	bbox_child2->abox.size = pNode->size;	
}

void COBBTree::GetNodeChildrenBVs(const BV *pBV_parent, const Vec3 &sweepdir,float sweepstep, BV *&pBV_child1,BV *&pBV_child2, int iCaller)
{
	BBox *bbox_parent=(BBox*)pBV_parent, *&bbox_child1=(BBox*&)pBV_child1, *&bbox_child2=(BBox*&)pBV_child2;
	bbox_child1 = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	bbox_child2 = g_BBoxBuf + min(g_BBoxBufPos++, k_BBoxBufSize-1);
	bbox_child2->iNode = (bbox_child1->iNode = m_pNodes[bbox_parent->iNode].ichild)+1;
	bbox_child1->type = bbox_child2->type = box::type;

	OBBnode *pNode = m_pNodes + bbox_child1->iNode;
	box boxstatic;
	boxstatic.bOriented = 1;
	boxstatic.Basis = GetMtxFromBasis(pNode->axes);
	boxstatic.center = pNode->center;
	boxstatic.size = pNode++->size;
	ExtrudeBox(&boxstatic, sweepdir,sweepstep, &bbox_child1->abox);
	bbox_child1->abox.bOriented = 1+bbox_child1->iNode;

	boxstatic.Basis = GetMtxFromBasis(pNode->axes);
	boxstatic.center = pNode->center;
	boxstatic.size = pNode->size;	
	ExtrudeBox(&boxstatic, sweepdir,sweepstep, &bbox_child2->abox);
	bbox_child1->abox.bOriented = 1+bbox_child2->iNode;
}

void COBBTree::ReleaseLastBVs(int iCaller)
{
	g_BBoxBufPos-=2;
}

void COBBTree::ReleaseLastSweptBVs(int iCaller)
{
	g_BBoxBufPos-=2;
}

int COBBTree::GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, geometry_under_test *pGTest,
															geometry_under_test *pGTestOp)
{
	return m_pMesh->GetPrimitiveList(m_pNodes[iNode].ichild,m_pNodes[iNode].ntris, pBVCollider->type,*pBVCollider,bColliderLocal, pGTest,pGTestOp,
		pGTest->primbuf,pGTest->idbuf);
}

void COBBTree::MarkUsedTriangle(int itri, geometry_under_test *pGTest)
{
	if (!pGTest->pUsedNodesMap) return;
	int iNode = m_pTri2Node[itri];
	if (!(pGTest->pUsedNodesMap[iNode>>5] & 1<<(iNode&31)) && 
			max(max(m_pNodes[iNode].size.x,m_pNodes[iNode].size.y),m_pNodes[iNode].size.z) < m_maxSkipDim) 
	{
		do {
			pGTest->pUsedNodesMap[iNode>>5] |= 1<<(iNode&31);
			pGTest->pUsedNodesIdx[pGTest->nUsedNodes = min(pGTest->nUsedNodes+1,pGTest->nMaxUsedNodes-1)] = iNode;
			pGTest->bCurNodeUsed = 1;
			iNode ^= 1; // the other child of the same parent
			if (!(pGTest->pUsedNodesMap[iNode>>5] & 1<<(iNode&31)))
				break;
			iNode = m_pNodes[iNode].iparent;
		} while (iNode>=0);
	}
}

int COBBTree::PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl)
{
	if (m_maxSkipDim<=0) {
		pGTest->pUsedNodesMap = 0;
		pGTest->pUsedNodesIdx = 0;
		pGTest->nMaxUsedNodes = 0;
	} else {
		int mapsz = (m_nNodes-1>>5) + 1;
		IF (mapsz<=(int)(CRY_ARRAY_COUNT(g_UsedNodesMap))-g_UsedNodesMapPos,1) {
			pGTest->pUsedNodesMap = g_UsedNodesMap+g_UsedNodesMapPos; g_UsedNodesMapPos += mapsz;
		} else {
			pGTest->pUsedNodesMap = new int[mapsz];
		}
		pGTest->pUsedNodesIdx = g_UsedNodesIdx+g_UsedNodesIdxPos;
		pGTest->nMaxUsedNodes = min(32,(int)(CRY_ARRAY_COUNT(g_UsedNodesIdx)-g_UsedNodesIdxPos));
		g_UsedNodesIdxPos += pGTest->nMaxUsedNodes;
	}
	pGTest->nUsedNodes = -1;
	return 1;
}

void COBBTree::CleanupAfterIntersectionTest(geometry_under_test *pGTest)
{
	if (pGTest->pUsedNodesMap){
		IF ((unsigned int)(pGTest->pUsedNodesMap-g_UsedNodesMap) > (unsigned int)CRY_ARRAY_COUNT(g_UsedNodesMap), 0) 
		{
			delete[] pGTest->pUsedNodesMap; 
			return;
		}
		if (pGTest->nUsedNodes < pGTest->nMaxUsedNodes-1) {
			for(int i=0;i<=pGTest->nUsedNodes;i++) 
				pGTest->pUsedNodesMap[pGTest->pUsedNodesIdx[i]>>5] &= ~(1<<(pGTest->pUsedNodesIdx[i]&31));
		} else
			memset(pGTest->pUsedNodesMap, 0, ((m_nNodes-1>>5)+1)*4);
	}
}


void COBBTree::GetMemoryStatistics(ICrySizer *pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "OBB trees");
	pSizer->AddObject(this, sizeof(COBBTree));
	pSizer->AddObject(m_pNodes, m_nNodesAlloc*sizeof(m_pNodes[0]));
	if (m_pTri2Node)
		pSizer->AddObject(m_pTri2Node, m_pMesh->m_nTris*sizeof(m_pTri2Node[0]));
}


void COBBTree::Save(CMemStream &stm)
{
	stm.Write(m_nNodes);
	stm.Write(m_pNodes, m_nNodes*sizeof(m_pNodes[0]));
	stm.Write(m_nMaxTrisInNode);
	stm.Write(m_nMinTrisPerNode);
	stm.Write(m_nMaxTrisPerNode);
	stm.Write(m_maxSkipDim);
}

void COBBTree::Load(CMemStream &stm, CGeometry *pGeom)
{
	m_pMesh = (CTriMesh*)pGeom;
	stm.Read(m_nNodes);
	m_pNodes = new OBBnode[m_nNodesAlloc=m_nNodes];
	stm.ReadType(m_pNodes, m_nNodes);
	m_pTri2Node = new index_t[m_pMesh->m_nTris];
	for(int i=0;i<m_nNodes;i++) for(int j=0;j<m_pNodes[i].ntris;j++)
		m_pTri2Node[m_pNodes[i].ichild+j] = i;

	stm.Read(m_nMaxTrisInNode);
	stm.Read(m_nMinTrisPerNode);
	stm.Read(m_nMaxTrisPerNode);
	stm.Read(m_maxSkipDim);
}

int COBBTree::SanityCheck()
{
	int iCaller = MAX_PHYS_THREADS;
	const int bufLength = CRY_ARRAY_COUNT(g_BBoxBuf);
	return SanityCheckTree(this, (bufLength-1)/4);
}
