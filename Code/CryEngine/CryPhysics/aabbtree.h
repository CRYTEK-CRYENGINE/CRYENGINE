// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef aabbtree_h
#define aabbtree_h

struct AABBnode {
	unsigned int ichild;
	unsigned char minx;
	unsigned char maxx;
	unsigned char miny;
	unsigned char maxy;
	unsigned char minz;
	unsigned char maxz;
	unsigned char ntris;
	unsigned char bSingleColl;

	AUTO_STRUCT_INFO;
};

struct AABBnodeV0 {
	unsigned int minx : 7;
	unsigned int maxx : 7;
	unsigned int miny : 7;
	unsigned int maxy : 7;
	unsigned int minz : 7;
	unsigned int maxz : 7;
	unsigned int ichild : 15;
	unsigned int ntris : 6;
	unsigned int bSingleColl : 1;

	AUTO_STRUCT_INFO_LOCAL;
};	

class CTriMesh;

class CAABBTree : public CBVTree {
public:
	// cppcheck-suppress uninitMemberVar
	CAABBTree() { m_pNodes=0; m_pTri2Node=0; m_maxDepth=64; }
	virtual ~CAABBTree() { 
		if (m_pNodes) delete[] m_pNodes; m_pNodes=0; 
		if (m_pTri2Node) delete[] m_pTri2Node; m_pTri2Node=0; 
	}

	virtual int GetType()  { return BVT_AABB; }

	float Build(CGeometry *pMesh);
	virtual void SetGeomConvex();

	void SetParams(int nMinTrisPerNode,int nMaxTrisPerNode, float skipdim, const Matrix33 &Basis, int planeOptimisation=0);
	float BuildNode(int iNode, int iTriStart,int nTris, Vec3 center,Vec3 size, Vec3 (*bbtri)[3], int nDepth);

	virtual int PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl);
	virtual void CleanupAfterIntersectionTest(geometry_under_test *pGTest);
	virtual void GetBBox(box *pbox); 
	virtual int MaxPrimsInNode() { return m_nMaxTrisInNode; }
	virtual void GetNodeBV(BV *&pBV, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) {}
	virtual float SplitPriority(const BV* pBV);
	virtual void GetNodeChildrenBVs(const Matrix33 &Rw,const Vec3 &offsw,float scalew, const BV *pBV_parent, BV *&pBV_child1,BV *&pBV_child2, int iCaller=0);
	virtual void GetNodeChildrenBVs(const BV *pBV_parent, BV *&pBV_child1,BV *&pBV_child2, int iCaller=0);
	virtual void GetNodeChildrenBVs(const BV *pBV_parent, const Vec3 &sweepdir,float sweepstep, BV *&pBV_child1,BV *&pBV_child2, int iCaller=0);
	virtual void ReleaseLastBVs(int iCaller);
	virtual void ReleaseLastSweptBVs(int iCaller);
	virtual int GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
		geometry_under_test *pGTest,geometry_under_test *pGTestOp);
	virtual int GetNodeContentsIdx(int iNode, int &iStartPrim) { iStartPrim = m_pNodes[iNode].ichild; return m_pNodes[iNode].ntris; }
	virtual void MarkUsedTriangle(int itri, geometry_under_test *pGTest);
	virtual float GetMaxSkipDim() { return m_maxSkipDim/max(max(m_size.x,m_size.y),m_size.z); }
	void GetRootNodeDim(Vec3 &center,Vec3 &size)
	{
		Vec3 ptmin,ptmax;
		ptmin.x = m_pNodes[0].minx*m_size.x*(2.0f/128);
		ptmax.x = (m_pNodes[0].maxx+1)*m_size.x*(2.0f/128);
		ptmin.y = m_pNodes[0].miny*m_size.y*(2.0f/128);
		ptmax.y = (m_pNodes[0].maxy+1)*m_size.y*(2.0f/128);
		ptmin.z = m_pNodes[0].minz*m_size.z*(2.0f/128);
		ptmax.z = (m_pNodes[0].maxz+1)*m_size.z*(2.0f/128);
		center = m_center+((ptmax+ptmin)*0.5f-m_size)*m_Basis;
		size = (ptmax-ptmin)*0.5f;
	}

	virtual void GetMemoryStatistics(ICrySizer *pSizer);
	virtual void Save(CMemStream &stm);
	virtual void Load(CMemStream &stm, CGeometry *pGeom);
	
	virtual int SanityCheck();

	CTriMesh *m_pMesh;
	AABBnode *m_pNodes;
	int m_nNodes,m_nNodesAlloc;
	Vec3 m_center;
	Vec3 m_size;
	Matrix33 m_Basis;
	int16 m_bOriented;
	int16 m_axisSplitMask;
	int *m_pTri2Node,m_nBitsLog;
	int m_nMaxTrisPerNode,m_nMinTrisPerNode;
	int m_nMaxTrisInNode;
	float m_maxSkipDim;
	int m_maxDepth;
};

#endif
