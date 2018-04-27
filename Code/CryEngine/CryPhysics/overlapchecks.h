// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef overlapchecks_h
#define overlapchecks_h

typedef int (*overlap_check)(const primitive*,const primitive*,class COverlapChecker*);

int default_overlap_check(const primitive*, const primitive*, class COverlapChecker*);
int box_box_overlap_check(const box *box1, const box *box2, class COverlapChecker*);
int box_heightfield_overlap_check(const box *pbox, const heightfield *phf, class COverlapChecker *pOverlapper=0);
int heightfield_box_overlap_check(const heightfield *phf, const box *pbox, class COverlapChecker *pOverlapper=0);
int box_voxgrid_overlap_check(const box *pbox, const voxelgrid *phf, class COverlapChecker *pOverlapper=0);
int voxgrid_box_overlap_check(const voxelgrid *phf, const box *pbox, class COverlapChecker *pOverlapper=0);
int box_tri_overlap_check(const box *pbox, const triangle *ptri, class COverlapChecker *pOverlapper=0);
int tri_box_overlap_check(const triangle *ptri, const box *pbox, class COverlapChecker *pOverlapper=0);
int box_ray_overlap_check(const box *pbox, const ray *pray, class COverlapChecker *pOverlapper=(COverlapChecker*)0);
int ray_box_overlap_check(const ray *pray, const box *pbox, class COverlapChecker *pOverlapper=0);
int box_sphere_overlap_check(const box *pbox, const sphere *psph, class COverlapChecker *pOverlapper=0);
int sphere_box_overlap_check(const sphere *psph, const box *pbox, class COverlapChecker *pOverlapper=0);
int tri_sphere_overlap_check(const triangle *ptri, const sphere *psph, class COverlapChecker *pOverlapper=0);
int sphere_tri_overlap_check(const sphere *psph, const triangle *ptri, class COverlapChecker *pOverlapper=0);
int heightfield_sphere_overlap_check(const heightfield *phf, const sphere *psph, class COverlapChecker *pOverlapper=0);
int sphere_heightfield_overlap_check(const sphere *psph, const heightfield *phf, class COverlapChecker *pOverlapper=0);
int sphere_sphere_overlap_check(const sphere *psph1, const sphere *psph2, class COverlapChecker *pOverlapper=0);

quotientf tri_sphere_dist2(const triangle *ptri, const sphere *psph, int &bFace);

class COverlapChecker {
public:

	COverlapChecker() { Init(); }

	void Init() { iPrevCode = -1; }
	ILINE int Check(int type1,int type2, primitive *prim1,primitive *prim2) {
		return table[type1][type2](prim1,prim2,this);
	}
	ILINE int CheckExists(int type1,int type2) {
		return table[type1][type2]!=default_overlap_check;
	}

	static overlap_check table[NPRIMS][NPRIMS];
	static overlap_check default_function; 

	//static COverlapCheckerInit init;

	int iPrevCode;
	Matrix33 Basis21;
	Matrix33 Basis21abs;
};
//extern COverlapChecker g_Overlapper;

#endif
