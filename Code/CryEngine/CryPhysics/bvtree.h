// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef bvtree_h
#define bvtree_h


////////////////////////// bounding volumes ////////////////////////

struct BV {
	int type;
	int iNode; 
	inline operator primitive*();
};
struct BV_Primitive : public BV { primitive p; };
inline BV::operator primitive*() { return &static_cast<BV_Primitive *>(this)->p; }

struct BBox : BV {
	box abox;
};

struct BVheightfield : BV {
	heightfield hf;
};

struct BVvoxelgrid : BV {
	voxelgrid voxgrid;
};

struct BVray : BV {
	ray aray;
};

class CGeometry;
class CBVTree;

struct surface_desc {
	Vec3 n;
	int idx;
	int iFeature;
};
struct edge_desc {
	Vec3 dir;
	Vec3 n[2];
	int idx;
	int iFeature;
};

struct geometry_under_test {
	CGeometry *pGeometry;
	CBVTree *pBVtree;
	int *pUsedNodesMap;
	int *pUsedNodesIdx;
	int nUsedNodes;
	int nMaxUsedNodes;
	int bStopIntersection;
	int bCurNodeUsed;

	Matrix33 R,R_rel;
	Vec3 offset,offset_rel;
	float scale,rscale, scale_rel,rscale_rel;
	int bTransformUpdated;

	Vec3 v;
	Vec3 w,centerOfMass;
	Vec3 centerOfRotation;
	intersection_params *pParams;

	Vec3 axisContactNormal;
	Vec3 sweepdir,sweepdir_loc;
	float sweepstep,sweepstep_loc;
	Vec3 ptOutsidePivot;

	int typeprim;
	primitive *primbuf; // used to get node contents
	primitive *primbuf1;// used to get unprojection candidates
	int szprimbuf,szprimbuf1;
	int *iFeature_buf; // feature that led to this primitive
	char *idbuf; // ids of unprojection candidates
	int szprim;

	surface_desc *surfaces;	// the last potentially surfaces 
	edge_desc *edges;	// the last potentially contacting edges
	int nSurfaces,nEdges;
	float minAreaEdge;

	geom_contact *contacts;
	int *pnContacts;
	int nMaxContacts;

	int iCaller;
};

enum BVtreetypes { BVT_OBB=0, BVT_AABB=1, BVT_SINGLEBOX=2, BVT_RAY=3, BVT_HEIGHTFIELD=4, BVT_VOXEL=5 };

class CBVTree {
public:
	virtual ~CBVTree() {}
	virtual int GetType() = 0;
	virtual void GetBBox(box *pbox) {}
	virtual int MaxPrimsInNode() { return 1; }
	virtual float Build(CGeometry *pGeom) = 0;
	virtual void SetGeomConvex() {}

	virtual int PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl) {
		pGTest->pUsedNodesMap = 0;
		pGTest->pUsedNodesIdx = 0;
		pGTest->nMaxUsedNodes = 0;
		pGTest->nUsedNodes = -1;
		return 1;
	}

	virtual void CleanupAfterIntersectionTest(geometry_under_test *pGTest) {}
	virtual void GetNodeBV(BV *&pBV, int iNode=0, int iCaller=0) = 0;
	virtual void GetNodeBV(BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) = 0;
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode=0, int iCaller=0) = 0;
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) = 0;
	virtual float SplitPriority(const BV* pBV) { return 0.0f; }
	virtual void GetNodeChildrenBVs(const Matrix33 &Rw,const Vec3 &offsw,float scalew, const BV *pBV_parent, BV *&pBV_child1,BV *&pBV_child2, int iCaller=0) {}
	virtual void GetNodeChildrenBVs(const BV *pBV_parent, BV *&pBV_child1,BV *&pBV_child2, int iCaller=0) {}
	virtual void GetNodeChildrenBVs(const BV *pBV_parent, const Vec3 &sweepdir,float sweepstep, BV *&pBV_child1,BV *&pBV_child2, int iCaller=0) {}
	virtual void ReleaseLastBVs(int iCaller=0) {}
	virtual void ReleaseLastSweptBVs(int iCaller=0) {}
	virtual void ResetCollisionArea() {} 
	virtual float GetMaxSkipDim() { return 0; }
  
	virtual void GetMemoryStatistics(ICrySizer *pSizer) {}
	virtual void Save(CMemStream &stm) {}
	virtual void Load(CMemStream &stm, CGeometry *pGeom) {}

	virtual int SanityCheck() { return 1; }

	virtual int GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
		geometry_under_test *pGTest,geometry_under_test *pGTestOp) = 0;

	virtual int GetNodeContentsIdx(int iNode, int &iStartPrim) { iStartPrim=0; return 1; }
	virtual void MarkUsedTriangle(int itri, geometry_under_test *pGTest) {}
};

#endif
