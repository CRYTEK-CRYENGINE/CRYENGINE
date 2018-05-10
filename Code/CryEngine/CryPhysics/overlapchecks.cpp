// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "overlapchecks.h"

//COverlapChecker g_Overlapper;
#undef g_Overlapper
#define g_Overlapper (*pOverlapper)

overlap_check COverlapChecker::table[NPRIMS][NPRIMS] = {
	{ (overlap_check)box_box_overlap_check, (overlap_check)box_tri_overlap_check, (overlap_check)box_heightfield_overlap_check, 
		(overlap_check)box_ray_overlap_check, (overlap_check)box_sphere_overlap_check, default_overlap_check, 
		default_overlap_check, (overlap_check)box_voxgrid_overlap_check },
	{	(overlap_check)tri_box_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check,
		(overlap_check)tri_sphere_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check },
	{ (overlap_check)heightfield_box_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check,
		(overlap_check)heightfield_sphere_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check },
	{ (overlap_check)ray_box_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check,
		default_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check },
	{ (overlap_check)sphere_box_overlap_check, (overlap_check)sphere_tri_overlap_check, (overlap_check)sphere_heightfield_overlap_check, 
		default_overlap_check, (overlap_check)sphere_sphere_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check },
	{ default_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check,
		default_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check },
	{ default_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check,
		default_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check },
	{ (overlap_check)voxgrid_box_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check,
		default_overlap_check, default_overlap_check, default_overlap_check, default_overlap_check }
};

overlap_check COverlapChecker::default_function = default_overlap_check; 
/*COverlapChecker::COverlapCheckerInit COverlapChecker::init;

COverlapChecker::COverlapCheckerInit::COverlapCheckerInit() 
{
	for(int i=0;i<NPRIMS;i++) for(int j=0;j<NPRIMS;j++)
		table[i][j] = default_overlap_check;
	table[box::type][box::type] = (overlap_check)box_box_overlap_check;
	table[box::type][triangle::type] = (overlap_check)box_tri_overlap_check;
	table[triangle::type][box::type] = (overlap_check)tri_box_overlap_check;
	table[box::type][heightfield::type] = (overlap_check)box_heightfield_overlap_check;
	table[heightfield::type][box::type] = (overlap_check)heightfield_box_overlap_check;
	table[box::type][voxelgrid::type] = (overlap_check)box_voxgrid_overlap_check;
	table[voxelgrid::type][box::type] = (overlap_check)voxgrid_box_overlap_check;
	table[heightfield::type][voxelgrid::type] = (overlap_check)heightfield_voxgrid_overlap_check;
	table[voxelgrid::type][heightfield::type] = (overlap_check)voxgrid_heightfield_overlap_check;
	table[box::type][ray::type] = (overlap_check)box_ray_overlap_check;
	table[ray::type][box::type] = (overlap_check)ray_box_overlap_check;
	table[box::type][sphere::type] = (overlap_check)box_sphere_overlap_check;
	table[sphere::type][box::type] = (overlap_check)sphere_box_overlap_check;
	table[triangle::type][sphere::type] = (overlap_check)tri_sphere_overlap_check;
	table[sphere::type][triangle::type] = (overlap_check)sphere_tri_overlap_check;
	table[sphere::type][sphere::type] = (overlap_check)sphere_sphere_overlap_check;
	table[heightfield::type][sphere::type] = (overlap_check)heightfield_sphere_overlap_check;
	table[sphere::type][heightfield::type] = (overlap_check)sphere_heightfield_overlap_check;
}*/

int default_overlap_check(const primitive*, const primitive *, COverlapChecker*)
{
	return 1;
}

int box_box_overlap_check(const box *box1, const box *box2, COverlapChecker *pOverlapper)
{
	const Vec3 &a=box1->size, &b=box2->size;
	Matrix33 Basis21,Basis21abs;
	Vec3 center21;
	float t1,t2,t3,e;
	if ((box1->bOriented|box2->bOriented<<16)!=g_Overlapper.iPrevCode) {
		if (!box1->bOriented)
			Basis21 = box2->Basis.T();
		else if (box2->bOriented)
			Basis21 = box1->Basis*box2->Basis.T();
		else
			Basis21 = box1->Basis;
		g_Overlapper.iPrevCode = box1->bOriented|box2->bOriented<<16;
		g_Overlapper.Basis21 = Basis21;
		g_Overlapper.Basis21abs = (Basis21abs = Basis21).Fabs();
	} else {
		Basis21abs = g_Overlapper.Basis21abs;
		Basis21 = g_Overlapper.Basis21;
	}

	e=(a.x+a.y+a.z)*1e-4f;
	center21 = box2->center-box1->center;
	if (box1->bOriented)
		center21 = box1->Basis*center21;

	// node1 basis vectors
	if (fabsf(center21.x) > a.x+b*Basis21abs.GetRow(0))
		return 0;
	if (fabsf(center21.y) > a.y+b*Basis21abs.GetRow(1))
		return 0;
	if (fabsf(center21.z) > a.z+b*Basis21abs.GetRow(2))
		return 0;

	// node2 basis vectors
	if (fabsf(center21*Basis21.GetColumn(0)) > a*Basis21abs.GetColumn(0)+b.x)
		return 0;
	if (fabsf(center21*Basis21.GetColumn(1)) > a*Basis21abs.GetColumn(1)+b.y)
		return 0;
	if (fabsf(center21*Basis21.GetColumn(2)) > a*Basis21abs.GetColumn(2)+b.z)
		return 0;

	// node1->axes[0] x node2->axes[0]
	t1 = a.y*Basis21abs(2,0) + a.z*Basis21abs(1,0);
	t2 = b.y*Basis21abs(0,2) + b.z*Basis21abs(0,1);
	t3 = center21.z*Basis21(1,0) - center21.y*Basis21(2,0);
	if (fabsf(t3) > t1+t2+e)
		return 0;

	// node1->axes[0] x node2->axes[1]
	t1 = a.y*Basis21abs(2,1) + a.z*Basis21abs(1,1);
	t2 = b.x*Basis21abs(0,2) + b.z*Basis21abs(0,0);
	t3 = center21.z*Basis21(1,1) - center21.y*Basis21(2,1);
	if(fabsf(t3) > t1+t2+e)
		return 0;

	// node1->axes[0] x node2->axes[2]
	t1 = a.y*Basis21abs(2,2) + a.z*Basis21abs(1,2);
	t2 = b.x*Basis21abs(0,1) + b.y*Basis21abs(0,0);
	t3 = center21.z*Basis21(1,2) - center21.y*Basis21(2,2);
	if(fabsf(t3) > t1+t2+e)
		return 0;

	// node1->axes[1] x node2->axes[0]
	t1 = a.x*Basis21abs(2,0) + a.z*Basis21abs(0,0);
	t2 = b.y*Basis21abs(1,2) + b.z*Basis21abs(1,1);
	t3 = center21.x*Basis21(2,0) - center21.z*Basis21(0,0);
	if(fabsf(t3) > t1+t2+e)
		return 0;

	// node1->axes[1] x node2->axes[1]
	t1 = a.x*Basis21abs(2,1) + a.z*Basis21abs(0,1);
	t2 = b.x*Basis21abs(1,2) + b.z*Basis21abs(1,0);
	t3 = center21.x*Basis21(2,1) - center21.z*Basis21(0,1);
	if(fabsf(t3) > t1+t2+e)
		return 0;

	// node1->axes[1] x node2->axes[2]
	t1 = a.x*Basis21abs(2,2) + a.z*Basis21abs(0,2);
	t2 = b.x*Basis21abs(1,1) + b.y*Basis21abs(1,0);
	t3 = center21.x*Basis21(2,2) - center21.z*Basis21(0,2);
	if(fabsf(t3) > t1+t2+e)
		return 0;

	// node1->axes[2] x node2->axes[0]
	t1 = a.x*Basis21abs(1,0) + a.y*Basis21abs(0,0);
	t2 = b.y*Basis21abs(2,2) + b.z*Basis21abs(2,1);
	t3 = center21.y*Basis21(0,0) - center21.x*Basis21(1,0);
	if(fabsf(t3) > t1+t2+e)
		return 0;
	
	// node1->axes[2] x node2->axes[1]
	t1 = a.x*Basis21abs(1,1) + a.y*Basis21abs(0,1);
	t2 = b.x*Basis21abs(2,2) + b.z*Basis21abs(2,0);
	t3 = center21.y*Basis21(0,1) - center21.x*Basis21(1,1);
	if(fabsf(t3) > t1+t2+e)
		return 0;

	// node1->axes[2] x node2->axes[2]
	t1 = a.x*Basis21abs(1,2) + a.y*Basis21abs(0,2);
	t2 = b.x*Basis21abs(2,1) + b.y*Basis21abs(2,0);
	t3 = center21.y*Basis21(0,2) - center21.x*Basis21(1,2);
	if(fabsf(t3) > t1+t2+e)
		return 0;

	return 1;
}

int box_heightfield_overlap_check(const box *pbox, const heightfield *phf, COverlapChecker*)
{
	box boxtr;
	Vec3 vtx[4];
	triangle hftri;
	Vec2 sz,ptmin,ptmax;
	int ix,iy,ix0,iy0,ix1,iy1;
	float hmax;

	// find the 3 lowest vertices
	if (phf->bOriented) {
		if (pbox->bOriented)
			boxtr.Basis = pbox->Basis*phf->Basis.T();
		else
			boxtr.Basis = phf->Basis.T();
		boxtr.center = phf->Basis*(pbox->center-phf->origin);
	} else {
		boxtr.Basis = pbox->Basis;
		boxtr.center = pbox->center-phf->origin;
	}
	boxtr.bOriented = 1;
	boxtr.Basis.SetRow(0,boxtr.Basis.GetRow(0)*-sgnnz(boxtr.Basis(0,2)));
	boxtr.Basis.SetRow(1,boxtr.Basis.GetRow(1)*-sgnnz(boxtr.Basis(1,2)));
	boxtr.Basis.SetRow(2,boxtr.Basis.GetRow(2)*-sgnnz(boxtr.Basis(2,2)));
	boxtr.size = pbox->size;
	vtx[0] = pbox->size*boxtr.Basis;
	boxtr.Basis.SetRow(0,-boxtr.Basis.GetRow(0));	vtx[1]=pbox->size*boxtr.Basis; boxtr.Basis.SetRow(0,-boxtr.Basis.GetRow(0));
	boxtr.Basis.SetRow(1,-boxtr.Basis.GetRow(1));	vtx[2]=pbox->size*boxtr.Basis; boxtr.Basis.SetRow(1,-boxtr.Basis.GetRow(1));
	boxtr.Basis.SetRow(2,-boxtr.Basis.GetRow(2));	vtx[3]=pbox->size*boxtr.Basis; boxtr.Basis.SetRow(2,-boxtr.Basis.GetRow(2));

	// find the underlying grid rectangle
	sz.x = sz.y = 0;
	sz.x = max(sz.x, fabsf(vtx[1].x)); sz.y = max(sz.y, fabsf(vtx[1].y));
	sz.x = max(sz.x, fabsf(vtx[2].x)); sz.y = max(sz.y, fabsf(vtx[2].y));
	sz.x = max(sz.x, fabsf(vtx[3].x)); sz.y = max(sz.y, fabsf(vtx[3].y));
	ptmin.x = (boxtr.center.x-sz.x)*phf->stepr.x; ptmin.y = (boxtr.center.y-sz.y)*phf->stepr.y; 
	ptmax.x = (boxtr.center.x+sz.x)*phf->stepr.x; ptmax.y = (boxtr.center.y+sz.y)*phf->stepr.y;
	ix0 = float2int(ptmin.x-0.5f); iy0 = float2int(ptmin.y-0.5f); ix0 &= ~(ix0>>31); iy0 &= ~(iy0>>31);
	ix1 = min(float2int(ptmax.x+0.5f),phf->size.x); iy1 = min(float2int(ptmax.y+0.5f),phf->size.y);
	vtx[0]+=boxtr.center;	vtx[1]+=boxtr.center;	vtx[2]+=boxtr.center;	vtx[3]+=boxtr.center;

	if ((ix1-ix0)*(iy1-iy0)<=6) {
		// check if all heightfield points are below the lowest box point
		for(ix=ix0,hmax=0;ix<=ix1;ix++) for(iy=iy0;iy<=iy1;iy++) 
			hmax = max(hmax,phf->getheight(ix,iy));
		if (hmax<vtx[0].z)
			return 0;

		// check for intersection with each underlying triangle
		for(ix=ix0;ix<ix1;ix++) for(iy=iy0;iy<iy1;iy++) {
			hftri.pt[0].x=hftri.pt[2].x = ix*phf->step.x; hftri.pt[0].y=hftri.pt[1].y = iy*phf->step.y; 
			hftri.pt[1].x = hftri.pt[0].x+phf->step.x; hftri.pt[2].y = hftri.pt[0].y+phf->step.y;
			hftri.pt[0].z = phf->getheight(ix,iy); hftri.pt[1].z = phf->getheight(ix+1,iy); 
			hftri.pt[2].z = phf->getheight(ix,iy+1);
			hftri.n = hftri.pt[1]-hftri.pt[0] ^ hftri.pt[2]-hftri.pt[0];
			if (box_tri_overlap_check(&boxtr,&hftri))
				return 1;
			hftri.pt[0] = hftri.pt[2]; hftri.pt[2].x += phf->step.x; hftri.pt[2].z = phf->getheight(ix+1,iy+1);
			hftri.n = hftri.pt[1]-hftri.pt[0] ^ hftri.pt[2]-hftri.pt[0];
			if (box_tri_overlap_check(&boxtr,&hftri))
				return 1;
		}
	}	else for(ix=ix0;ix<=ix1;ix++) for(iy=iy0;iy<=iy1;iy++) 
			if (phf->getheight(ix,iy)>vtx[0].z)
				return 1;

	return 0;
}

int heightfield_box_overlap_check(const heightfield *phf, const box *pbox, COverlapChecker *pOverlapper) 
{
	return box_heightfield_overlap_check(pbox,phf,pOverlapper);
}

int box_voxgrid_overlap_check(const box *pbox, const voxelgrid *pgrid, COverlapChecker*)
{
	int i,j,nTris=0,nTrisDst,icell;
	box boxtr;
	Vec3 dim;
	Vec3i ic,iBBox[2];
	const int MAXTESTRIS = 8;
	int idxbuf[(MAXTESTRIS+1)*2], *plist=idxbuf, *plistDst=idxbuf+MAXTESTRIS+1;
	triangle atri;

	if (pgrid->bOriented) {
		if (pbox->bOriented)
			boxtr.Basis = pbox->Basis*pgrid->Basis.T();
		else
			boxtr.Basis = pgrid->Basis.T();
		boxtr.center = pgrid->Basis*(pbox->center-pgrid->origin);
	} else {
		boxtr.Basis = pbox->Basis;
		boxtr.center = pbox->center-pgrid->origin;
	}
	boxtr.size = pbox->size;
	boxtr.bOriented = 1;

	dim = boxtr.size*boxtr.Basis.Fabs();
	for(i=0;i<3;i++) {
		iBBox[0][i] = max(0, float2int((boxtr.center[i]-dim[i])*pgrid->stepr[i]-0.5f));
		iBBox[1][i] = min(pgrid->size[i], float2int((boxtr.center[i]+dim[i])*pgrid->stepr[i]+0.5f));
	}

	if ((iBBox[1]-iBBox[0]).GetVolume()>18)
		return 1;

	boxtr.Basis = pbox->Basis*pgrid->R;
	boxtr.center = ((pbox->center-pgrid->offset)*pgrid->R)*pgrid->rscale;
	boxtr.size = pbox->size*pgrid->rscale;

	for(ic.z=iBBox[0].z;ic.z<iBBox[1].z;ic.z++) for(ic.y=iBBox[0].y;ic.y<iBBox[1].y;ic.y++) for(ic.x=iBBox[0].x;ic.x<iBBox[1].x;ic.x++) {
		icell = ic*pgrid->stride;
		nTrisDst = unite_lists(plist,nTris, pgrid->pTriBuf+pgrid->pCellTris[icell],pgrid->pCellTris[icell+1]-pgrid->pCellTris[icell], 
			plistDst,MAXTESTRIS+1);
		if (nTrisDst>MAXTESTRIS)
			return 1;
		swap(nTris,nTrisDst); swap(plist,plistDst);
	}

	for(i=0;i<nTris;i++) {
		for(j=0;j<3;j++) atri.pt[j] = pgrid->pVtx[pgrid->pIndices[plist[i]*3+j]];
		atri.n = pgrid->pNormals[plist[i]];
		if (box_tri_overlap_check(&boxtr,&atri))
			return 1;
	}

	return 0;
}

int voxgrid_box_overlap_check(const voxelgrid *pgrid, const box *pbox, COverlapChecker *pOverlapper) 
{
	return box_voxgrid_overlap_check(pbox,pgrid,pOverlapper);
}

int box_tri_overlap_check(const box *pbox, const triangle *ptri, COverlapChecker*)
{
	Vec3 pt[3],n;
	float l1,l2,l3,l,c;
	if (pbox->bOriented) {
		pt[0] = pbox->Basis*(ptri->pt[0]-pbox->center); 
		pt[1] = pbox->Basis*(ptri->pt[1]-pbox->center);	
		pt[2] = pbox->Basis*(ptri->pt[2]-pbox->center);
		n = pbox->Basis*ptri->n;
	} else {
		pt[0] = ptri->pt[0]-pbox->center; 
		pt[1] = ptri->pt[1]-pbox->center;	
		pt[2] = ptri->pt[2]-pbox->center;
		n = ptri->n;
	}

	// check box normals
	l1=fabsf(pt[0].x-pt[1].x); l2=fabsf(pt[1].x-pt[2].x); l3=fabsf(pt[2].x-pt[0].x); l=l1+l2+l3; // half length = l/4
	c = (l1*(pt[0].x+pt[1].x)+l2*(pt[1].x+pt[2].x)+l3*(pt[2].x+pt[0].x))*0.5f; // center = c/l
	if (fabsf(c) > (pbox->size.x+l*0.25f)*l) 
		return 0;
	l1=fabsf(pt[0].y-pt[1].y); l2=fabsf(pt[1].y-pt[2].y); l3=fabsf(pt[2].y-pt[0].y); l=l1+l2+l3;
	c = (l1*(pt[0].y+pt[1].y)+l2*(pt[1].y+pt[2].y)+l3*(pt[2].y+pt[0].y))*0.5f; 
	if (fabsf(c) > (pbox->size.y+l*0.25f)*l) 
		return 0;
	l1=fabsf(pt[0].z-pt[1].z); l2=fabsf(pt[1].z-pt[2].z); l3=fabsf(pt[2].z-pt[0].z); l=l1+l2+l3;
	c = (l1*(pt[0].z+pt[1].z)+l2*(pt[1].z+pt[2].z)+l3*(pt[2].z+pt[0].z))*0.5f; 
	if (fabsf(c) > (pbox->size.z+l*0.25f)*l) 
		return 0;

	// check triangle normal
	if (fabsf(n.x)*pbox->size.x+fabsf(n.y)*pbox->size.y+fabsf(n.z)*pbox->size.z < fabsf(n*pt[0]))
		return 0;

	Vec3 edge,triproj1,triproj2;
	float boxproj;

	// check triangle edges - box edges cross products
	for(int i=0;i<3;i++) {
		edge = pt[inc_mod3[i]]-pt[i]; triproj1 = edge^pt[i]; triproj2 = edge^pt[dec_mod3[i]];
		boxproj = fabsf(pbox->size.y*edge.z) + fabsf(pbox->size.z*edge.y);
		if (fabsf((triproj1.x+triproj2.x)*0.5f) > boxproj+fabsf(triproj1.x-triproj2.x)*0.5f)
			return 0;
		boxproj = fabsf(pbox->size.x*edge.z) + fabsf(pbox->size.z*edge.x);
		if (fabsf((triproj1.y+triproj2.y)*0.5f) > boxproj+fabsf(triproj1.y-triproj2.y)*0.5f)
			return 0;
		boxproj = fabsf(pbox->size.x*edge.y) + fabsf(pbox->size.y*edge.x);
		if (fabsf((triproj1.z+triproj2.z)*0.5f) > boxproj+fabsf(triproj1.z-triproj2.z)*0.5f)
			return 0;
	}

	return 1;
}

int tri_box_overlap_check(const triangle *ptri, const box *pbox, COverlapChecker *pOverlapper)
{
	return box_tri_overlap_check(pbox,ptri,pOverlapper);
}

int box_ray_overlap_check(const box *pbox, const ray *pray, COverlapChecker*)
{
	Vec3 l,m,al;
	if (pbox->bOriented) {
		l = pbox->Basis*pray->dir*0.5f;
		m = pbox->Basis*(pray->origin-pbox->center)+l;
	} else {
		l = pray->dir*0.5f;
		m = pray->origin+l-pbox->center;
	}
	al.x=fabsf(l.x); al.y=fabsf(l.y); al.z=fabsf(l.z);

	// separating axis check for line and box
	if (fabsf(m.x) > pbox->size.x+al.x)
		return 0;
	if (fabsf(m.y) > pbox->size.y+al.y)
		return 0;
	if (fabsf(m.z) > pbox->size.z+al.z)
		return 0;

	if (fabsf(m.z*l.y-m.y*l.z) > pbox->size.y*al.z+pbox->size.z*al.y)
		return 0;
	if (fabsf(m.x*l.z-m.z*l.x) > pbox->size.x*al.z+pbox->size.z*al.x)
		return 0;
	if (fabsf(m.x*l.y-m.y*l.x) > pbox->size.x*al.y+pbox->size.y*al.x)
		return 0;

	return 1;
}

int ray_box_overlap_check(const ray *pray, const box *pbox, COverlapChecker *pOverlapper)
{
	return box_ray_overlap_check(pbox,pray,pOverlapper);
}

int box_sphere_overlap_check(const box *pbox, const sphere *psph, COverlapChecker*)
{
	Vec3 center, dist;
	if (pbox->bOriented)
		center = pbox->Basis*(psph->center-pbox->center);
	else
		center = psph->center-pbox->center;
	dist.x = max(0.0f, fabsf(center.x)-pbox->size.x);
	dist.y = max(0.0f, fabsf(center.y)-pbox->size.y);
	dist.z = max(0.0f, fabsf(center.z)-pbox->size.z);
	return isneg(dist.len2()-sqr(psph->r));
}

int sphere_box_overlap_check(const sphere *psph, const box *pbox, COverlapChecker *pOverlapper)
{
	return box_sphere_overlap_check(pbox,psph,pOverlapper);
}


quotientf tri_sphere_dist2(const triangle* ptri, const sphere* psph, int& bFace)
{
	int i,bInside[2];
	float rvtx[3],elen2[2],denom;
	Vec3 edge[2],dp;
	bFace = 0;

	rvtx[0] = (ptri->pt[0]-psph->center).len2();
	rvtx[1] = (ptri->pt[1]-psph->center).len2();
	rvtx[2] = (ptri->pt[2]-psph->center).len2();

	i = idxmin3(rvtx);
	dp = psph->center-ptri->pt[i];

	edge[0] = ptri->pt[inc_mod3[i]]-ptri->pt[i]; elen2[0] = edge[0].len2();
	edge[1] = ptri->pt[dec_mod3[i]]-ptri->pt[i]; elen2[1] = edge[1].len2();
	bInside[0] = isneg((dp^edge[0])*ptri->n);
	bInside[1] = isneg((edge[1]^dp)*ptri->n);
	rvtx[i] = rvtx[i]*elen2[bInside[0]] - sqr(max(0.0f,dp*edge[bInside[0]]))*(bInside[0]|bInside[1]); 
	denom = elen2[bInside[0]];

	if (bInside[0]&bInside[1]) {
		if (edge[0]*edge[1]<0) {
			edge[0] = ptri->pt[dec_mod3[i]]-ptri->pt[inc_mod3[i]];
			dp = psph->center-ptri->pt[inc_mod3[i]];
			if ((dp^edge[0])*ptri->n>0)
				return quotientf(rvtx[inc_mod3[i]]*edge[0].len2()-sqr(dp*edge[0]), edge[0].len2());
		}
		bFace = 1;
		return quotientf(sqr((psph->center-ptri->pt[0])*ptri->n),1.0f);
	}
	return quotientf(rvtx[i],denom);
}

int tri_sphere_overlap_check(const triangle *ptri, const sphere *psph, COverlapChecker*)
{
	int bFace;
	return isneg(tri_sphere_dist2(ptri,psph,bFace)-sqr(psph->r));

	/*float r2=sqr(psph->r),edgelen2;
	Vec3 edge,dp0,dp1;

	if ((ptri->pt[0]-psph->center).len2()<r2)
		return 1;
	if ((ptri->pt[1]-psph->center).len2()<r2)
		return 1;
	if ((ptri->pt[2]-psph->center).len2()<r2)
		return 1;

	edgelen2 = (edge = ptri->pt[1]-ptri->pt[0]).len2(); dp0 = ptri->pt[0]-psph->center; dp1 = ptri->pt[1]-psph->center;
	if (isneg((dp0^edge).len2()-r2*edgelen2) & isneg((dp0*edge)*(dp1*edge)))
		return 1;
	edgelen2 = (edge = ptri->pt[2]-ptri->pt[1]).len2(); dp0 = ptri->pt[1]-psph->center; dp1 = ptri->pt[2]-psph->center;
	if (isneg((dp0^edge).len2()-r2*edgelen2) & isneg((dp0*edge)*(dp1*edge)))
		return 1;
	edgelen2 = (edge = ptri->pt[0]-ptri->pt[2]).len2(); dp0 = ptri->pt[2]-psph->center; dp1 = ptri->pt[0]-psph->center;
	if (isneg((dp0^edge).len2()-r2*edgelen2) & isneg((dp0*edge)*(dp1*edge)))
		return 1;

	return 0;*/
}

int sphere_tri_overlap_check(const sphere *psph, const triangle *ptri, COverlapChecker *pOverlapper)
{
	return tri_sphere_overlap_check(ptri,psph,pOverlapper);
}

int sphere_sphere_overlap_check(const sphere *psph1, const sphere *psph2, COverlapChecker*)
{
	return isneg((psph1->center-psph2->center).len2()-sqr(psph1->r+psph2->r));
}

int heightfield_sphere_overlap_check(const heightfield *phf, const sphere *psph, COverlapChecker*)
{
	Vec3 center;
	Vec2i irect[2];
	int ix,iy,bContact=0;

	center = phf->Basis*(psph->center-phf->origin);
	irect[0].x = min(phf->size.x,max(0,float2int((center.x-psph->r)*phf->stepr.x-0.5f)));
	irect[0].y = min(phf->size.y,max(0,float2int((center.y-psph->r)*phf->stepr.y-0.5f)));
	irect[1].x = min(phf->size.x,max(0,float2int((center.x+psph->r)*phf->stepr.x+0.5f)));
	irect[1].y = min(phf->size.y,max(0,float2int((center.y+psph->r)*phf->stepr.y+0.5f)));

	for(ix=irect[0].x;ix<irect[1].x;ix++) for(iy=irect[0].y;iy<irect[1].y;iy++)
		bContact |= isneg(center.z-psph->r-phf->getheight(ix,iy));

	return bContact;
}

int sphere_heightfield_overlap_check(const sphere *psph, const heightfield *phf, COverlapChecker *pOverlapper)
{
	return heightfield_sphere_overlap_check(phf,psph,pOverlapper);
}

#undef g_Overlapper
