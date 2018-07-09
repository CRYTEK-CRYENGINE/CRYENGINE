// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef tetrlattice_h
#define tetrlattice_h
#pragma once

enum ltension_type { LPull,LPush,LShift,LTwist,LBend };
enum lvtx_flags { lvtx_removed=1,lvtx_removed_new=2,lvtx_processed=4,lvtx_inext_log2=8 };
enum ltet_flags { ltet_removed=1,ltet_removed_new=2,ltet_processed=4,ltet_inext_log2=8 };

struct STetrahedron {
	int flags;
	float M,Minv;
  float Vinv;
	Matrix33 Iinv;
	Vec3 Pext,Lext;
	float area;
	int ivtx[4];
	int ibuddy[4];
	float fracFace[4];
	int idxface[4];
	int idx;
};

struct SCGTetr {
	Vec3 dP,dL;
	float Minv;
	Matrix33 Iinv;
};

enum lface_flags { lface_processed=1 };

struct SCGFace {
	int itet,iface;
	Vec3 rv,rw;
	Vec3 dv,dw;
	Vec3 dP,dL;
	Vec3 P,L;
	SCGTetr *pTet[2];
	Vec3 r0,r1;
	Matrix33 vKinv,wKinv;
	int flags;
};


class CTetrLattice : public ITetrLattice {
public:
	CTetrLattice(IPhysicalWorld *pWorld);
	CTetrLattice(CTetrLattice *src, int bCopyData);
	~CTetrLattice();
	virtual void Release() { delete this; }

	CTetrLattice *CreateLattice(const Vec3 *pVtx,int nVtx, const int *pTets,int nTets);
	void SetMesh(CTriMesh *pMesh);
	void SetGrid(const box &bbox);
	void SetIdMat(int id) { m_idmat = id; }

	virtual int SetParams(pe_params *_params);
	virtual int GetParams(pe_params *_params);

	void Subtract(IGeometry *pGeonm, const geom_world_data *pgwd1,const geom_world_data *pgwd2);
	int CheckStructure(float time_interval,const Vec3 &gravity, const plane *pGround,int nPlanes,pe_explosion *pexpl, int maxIters=100000,int bLogTension=0);
	void Split(CTriMesh **pChunks,int nChunks, CTetrLattice **pLattices); 
	int Defragment();
	virtual void DrawWireframe(IPhysRenderer *pRenderer, geom_world_data *gwd, int idxColor);
	float GetLastTension(int &itype) { itype=m_imaxTension; return m_maxTension; }
	int AddImpulse(const Vec3 &pt, const Vec3 &impulse,const Vec3 &momentum, const Vec3 &gravity,float worldTime);

	virtual IGeometry *CreateSkinMesh(int nMaxTrisPerBVNode);
	virtual int CheckPoint(const Vec3 &pt, int *idx, float *w);

	int GetFaceByBuddy(int itet,int itetBuddy) {
		int i,ibuddy=0,imask;
		for(i=1;i<4;i++) {
			imask = -iszero(m_pTetr[itet].ibuddy[i]-itetBuddy);
			ibuddy = ibuddy&~imask | i&imask;
		}
		return ibuddy;
	}
	Vec3 GetTetrCenter(int i) {
		return (m_pVtx[m_pTetr[i].ivtx[0]]+m_pVtx[m_pTetr[i].ivtx[1]]+m_pVtx[m_pTetr[i].ivtx[2]]+m_pVtx[m_pTetr[i].ivtx[3]])*0.25f;
	}
	template<class T> int GetFaceIdx(int itet, int iface, T* idx) {
		for(int i=0,j=3-iface,dir=(iface&1)*2-1; i<3; (j+=dir)&=3)
			idx[i++] = m_pTetr[itet].ivtx[j];
		return 3;
	}

	IPhysicalWorld *m_pWorld;

	CTriMesh *m_pMesh;
	Vec3 *m_pVtx;
	int m_nVtx;
	STetrahedron *m_pTetr;
	int m_nTetr;
	int *m_pVtxFlags;
	int m_nMaxCracks;
	int m_idmat;
	float m_maxForcePush,m_maxForcePull,m_maxForceShift;
	float m_maxTorqueTwist,m_maxTorqueBend;
	float m_crackWeaken;
	float m_density;
	int m_nRemovedTets;
	int *m_pVtxRemap;
	int m_flags;
	float m_maxTension;
	int m_imaxTension;
	float m_lastImpulseTime;

	Matrix33 m_RGrid;
	Vec3 m_posGrid;
	Vec3 m_stepGrid,m_rstepGrid;
	Vec3i m_szGrid,m_strideGrid;
	int *m_pGridTet0,*m_pGrid;

	static SCGFace *g_Faces;
	static SCGTetr *g_Tets;
	static int g_nFacesAlloc,g_nTetsAlloc;
};


class CBreakableGrid2d : public IBreakableGrid2d {
public:
	CBreakableGrid2d() { m_pt=0; m_pTris=0; m_pCellDiv=0; m_nTris=0; m_nTris0=1<<31; m_pCellQueue=new int[m_szCellQueue=32]; }
	~CBreakableGrid2d() { 
		if (m_pt) delete[] m_pt; 
		if (m_pTris) delete[] m_pTris;
		if (m_pCellDiv) delete[] m_pCellDiv;
		if (m_pCellQueue) delete[] m_pCellQueue;
	}
	void Generate(Vec2 *ptsrc,int npt, const Vec2i &nCells, int bStaticBorder, int seed=-1);

	virtual int *BreakIntoChunks(const Vec2 &pt, float r, Vec2 *&ptout, int maxPatchTris,float jointhresh,int seed=-1, 
		float filterAng=0.0f,float ry=0.0f);
	virtual grid *GetGridData() { return &m_coord; }
	virtual bool IsEmpty() { return m_nTris==0; }
	virtual void Release() { delete this; }
	virtual float GetFracture() { return (float)(m_nTris0-m_nTris)/(float)m_nTris0; }

	void MarkCellInterior(int i);
	int get_neighb(int iTri,int iEdge);
	void get_edge_ends(int iTri,int iEdge, int &iend0,int &iend1);
	int CropSpikes(int imin,int imax, int *queue,int szQueue, int flags,int flagsNew, float thresh);

	virtual void GetMemoryStatistics(ICrySizer *pSizer) const {
		SIZER_COMPONENT_NAME(pSizer, "brekable2d grid");
		if (m_pt) pSizer->AddObject(m_pt, m_coord.size.x*m_coord.size.y*(sizeof(m_pt[0])+sizeof(m_pTris[0])*2+sizeof(m_pCellDiv[0])));
		if (m_pCellQueue) pSizer->AddObject(m_pCellQueue, m_szCellQueue);
	}

	enum tritypes { TRI_AVAILABLE=1<<29, TRI_FIXED=1<<27, TRI_EMPTY=1<<26, TRI_STABLE=1<<25, TRI_PROCESSED=1<<24 };
	primitives::grid m_coord;
	Vec2 *m_pt;
	int *m_pTris;
	char *m_pCellDiv;
	int m_nTris,m_nTris0;
	int *m_pCellQueue; 
	int m_szCellQueue;
};

#endif
