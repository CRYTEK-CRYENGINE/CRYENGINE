// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "intersectionchecks.h"

CIntersectionChecker g_Intersector;
intersection_check CIntersectionChecker::table[NPRIMS][NPRIMS] = {
	{ (intersection_check)box_box_intersection, (intersection_check)box_tri_intersection, default_intersection, 
		(intersection_check)box_ray_intersection, (intersection_check)box_sphere_intersection, (intersection_check)box_cylinder_intersection, 
		(intersection_check)box_capsule_intersection, default_intersection },
	{ (intersection_check)tri_box_intersection, (intersection_check)tri_tri_intersection, default_intersection, 
		(intersection_check)tri_ray_intersection, (intersection_check)tri_sphere_intersection, (intersection_check)tri_cylinder_intersection, 
		(intersection_check)tri_capsule_intersection, default_intersection },
	{ default_intersection, default_intersection, default_intersection, default_intersection, 
		default_intersection, default_intersection, default_intersection, default_intersection },
	{ (intersection_check)ray_box_intersection, (intersection_check)ray_tri_intersection, default_intersection, 
		default_intersection, (intersection_check)ray_sphere_intersection, (intersection_check)ray_cylinder_intersection, 
		(intersection_check)ray_capsule_intersection, default_intersection },
	{ (intersection_check)sphere_box_intersection, (intersection_check)sphere_tri_intersection, default_intersection, 
		(intersection_check)sphere_ray_intersection, (intersection_check)sphere_sphere_intersection, (intersection_check)sphere_cylinder_intersection, 
		(intersection_check)sphere_capsule_intersection, default_intersection },
	{ (intersection_check)cylinder_box_intersection, (intersection_check)cylinder_tri_intersection, default_intersection, 
		(intersection_check)cylinder_ray_intersection, (intersection_check)cylinder_sphere_intersection, (intersection_check)cylinder_cylinder_intersection, 
		(intersection_check)cylinder_capsule_intersection, default_intersection },
	{ (intersection_check)capsule_box_intersection, (intersection_check)capsule_tri_intersection, default_intersection, 
		(intersection_check)capsule_ray_intersection, (intersection_check)capsule_sphere_intersection, (intersection_check)capsule_cylinder_intersection, 
		(intersection_check)capsule_capsule_intersection, default_intersection },
	{ default_intersection, default_intersection, default_intersection, default_intersection, 
		default_intersection, default_intersection, default_intersection, default_intersection }
};

intersection_check CIntersectionChecker::default_function = default_intersection; 

/*CIntersectionChecker::CIntersectionChecker() 
{
	for(int i=0;i<NPRIMS;i++) for(int j=0;j<8;j++)
		table[i][j] = default_intersection;
	table[triangle::type][triangle::type] = (intersection_check)tri_tri_intersection;
	table[triangle::type][box::type] = (intersection_check)tri_box_intersection;
	table[box::type][triangle::type] = (intersection_check)box_tri_intersection;
	table[triangle::type][cylinder::type] = (intersection_check)tri_cylinder_intersection;
	table[cylinder::type][triangle::type] = (intersection_check)cylinder_tri_intersection;
	table[triangle::type][sphere::type] = (intersection_check)tri_sphere_intersection;
	table[sphere::type][triangle::type] = (intersection_check)sphere_tri_intersection;
	table[triangle::type][capsule::type] = (intersection_check)tri_capsule_intersection;
	table[capsule::type][triangle::type] = (intersection_check)capsule_tri_intersection;
	table[triangle::type][ray::type] = (intersection_check)tri_ray_intersection;
	table[ray::type][triangle::type] = (intersection_check)ray_tri_intersection;
	table[box::type][box::type] = (intersection_check)box_box_intersection;
	table[box::type][cylinder::type] = (intersection_check)box_cylinder_intersection;
	table[cylinder::type][box::type] = (intersection_check)cylinder_box_intersection;
	table[box::type][sphere::type] = (intersection_check)box_sphere_intersection;
	table[sphere::type][box::type] = (intersection_check)sphere_box_intersection;
	table[box::type][capsule::type] = (intersection_check)box_capsule_intersection;
	table[capsule::type][box::type] = (intersection_check)capsule_box_intersection;
	table[box::type][ray::type] = (intersection_check)box_ray_intersection;
	table[ray::type][box::type] = (intersection_check)ray_box_intersection;
	table[cylinder::type][cylinder::type] = (intersection_check)cylinder_cylinder_intersection;
	table[cylinder::type][sphere::type] = (intersection_check)cylinder_sphere_intersection;
	table[sphere::type][cylinder::type] = (intersection_check)sphere_cylinder_intersection;
	table[cylinder::type][capsule::type] = (intersection_check)cylinder_capsule_intersection;
	table[capsule::type][cylinder::type] = (intersection_check)capsule_cylinder_intersection;
	table[cylinder::type][ray::type] = (intersection_check)cylinder_ray_intersection;
	table[ray::type][cylinder::type] = (intersection_check)ray_cylinder_intersection;
	table[sphere::type][capsule::type] = (intersection_check)sphere_capsule_intersection;
	table[capsule::type][sphere::type] = (intersection_check)capsule_sphere_intersection;
	table[sphere::type][sphere::type] = (intersection_check)sphere_sphere_intersection;
	table[sphere::type][ray::type] = (intersection_check)sphere_ray_intersection;
	table[ray::type][sphere::type] = (intersection_check)ray_sphere_intersection;
	table[capsule::type][capsule::type] = (intersection_check)capsule_capsule_intersection;
	table[capsule::type][ray::type] = (intersection_check)capsule_ray_intersection;
	table[ray::type][capsule::type] = (intersection_check)ray_capsule_intersection;
	//table[triangle::type][plane::type] = (intersection_check)tri_plane_intersection;
	//table[plane::type][triangle::type] = (intersection_check)plane_tri_intersection;
	//table[box::type][plane::type] = (intersection_check)box_plane_intersection;
	//table[plane::type][box::type] = (intersection_check)plane_box_intersection;
	//table[cylinder::type][plane::type] = (intersection_check)cylinder_plane_intersection;
	//table[plane::type][cylinder::type] = (intersection_check)plane_cylinder_intersection;
	//table[sphere::type][plane::type] = (intersection_check)sphere_plane_intersection;
	//table[plane::type][sphere::type] = (intersection_check)plane_sphere_intersection;
	//table[ray::type][plane::type] = (intersection_check)ray_plane_intersection;
	//table[plane::type][ray::type] = (intersection_check)plane_ray_intersection;
}*/

int default_intersection(const primitive*,const primitive*, prim_inters *pinters)
{
	return 0;
}

int tri_tri_intersection(const triangle *ptri1, const triangle *ptri2, prim_inters *pinters)
{
	// axes: n2 x n  n1 x n  n = n1 x n2
	Vec3r axes[3]; axes[2] = ptri1->n^ptri2->n;
	axes[0] = ptri2->n^axes[2]; axes[1] = ptri1->n^axes[2];
	real nlen2 = axes[2].len2(), ptw;
	Vec3r ptloc,sgtest, pt0=Vec3r(ptri1->pt[0]*ptri1->n,-ptri2->pt[0]*ptri2->n,ptri1->pt[0]*axes[2])*GetMtxFromBasis(axes);

	Vec2r pt2d[2][3];
	int i, sgnx[2][3];

	for(i=0;i<3;i++) { 
		ptloc=ptri1->pt[i]*nlen2-pt0; pt2d[0][i].set(axes[1]*ptloc,axes[2]*ptloc);
		ptloc=ptri2->pt[i]*nlen2-pt0; pt2d[1][i].set(axes[0]*ptloc,axes[2]*ptloc);
		sgnx[0][i] = sgnnz(pt2d[0][i].x);	sgnx[1][i] = sgnnz(pt2d[1][i].x);
	}

	quotient t[2],tc[2],tr[2],tmin[2],tmax[2];
	int imin,imax,iEdge[2][2], nCross,iNotCross,bCross;

	for(i=0;i<2;i++) {
		bCross = sgnx[i][0]*sgnx[i][1]-1>>1;
		nCross = -bCross;	iNotCross = 0;
		bCross = sgnx[i][1]*sgnx[i][2]-1>>1;
		nCross -= bCross;	iNotCross |= 1&~bCross;
		bCross = sgnx[i][2]*sgnx[i][0]-1>>1;
		nCross -= bCross;	(iNotCross&=bCross) |= 2&~bCross;
		if (nCross!=2) 
			return 0;

		t[0].x = pt2d[i][dec_mod3[iNotCross]] ^ pt2d[i][inc_mod3[iNotCross]];
		t[0].y = pt2d[i][dec_mod3[iNotCross]].x - pt2d[i][inc_mod3[iNotCross]].x;
		t[1].x = pt2d[i][iNotCross] ^ pt2d[i][dec_mod3[iNotCross]];
		t[1].y = pt2d[i][iNotCross].x - pt2d[i][dec_mod3[iNotCross]].x;
		t[0].fixsign(); t[1].fixsign();	

		sgtest.Set(t[1].x*t[0].y-t[0].x*t[1].y, pt2d[i][iNotCross].x, pt2d[i][inc_mod3[iNotCross]].x);
		imin = isneg(sgtest[idxmax3(sgtest.abs())]);
		//imin = isneg(t[1].x*t[0].y-t[0].x*t[1].y);
		iEdge[i][imin] = inc_mod3[iNotCross]; iEdge[i][imin^1] = dec_mod3[iNotCross];
		tmin[i] = t[imin]; tmax[i] = t[imin^1];
		tc[i] = (t[0]+t[1])*(real)0.5; tr[i] = fabs_tpl(t[0]-t[1])*(real)0.5;
	}

	if (fabs_tpl((tc[0]-tc[1]).x) > (tr[0]+tr[1]).x)
		return 0;
	
	imax = (tmin[0]-tmin[1]).isneg();	// start_t = max(min1,min2);
	imin = (tmax[1]-tmax[0]).isneg();	// end_t = min(max1,max2)

	// 1 division scheme - less accurate
	ptw = nlen2*nlen2*tmin[imax].y*tmax[imin].y;
	if (ptw==0)
		return 0;		 
	ptw = (real)1.0/ptw; pt0 *= nlen2*tmin[imax].y*tmax[imin].y;
	pinters->pt[0] = (pt0 + axes[2]*(tmin[imax].x*tmax[imin].y))*ptw;
	pinters->pt[1] = (pt0 + axes[2]*(tmax[imin].x*tmin[imax].y))*ptw;

	//nlen2 = (real)1.0/nlen2; pt0 *= nlen2;
	//pinters->pt[0] = pt0 + axes[2]*(nlen2*nlen2*tmin[imax].val());
	//pinters->pt[1] = pt0 + axes[2]*(nlen2*nlen2*tmax[imin].val());

	pinters->iFeature[0][imax] = iEdge[imax][0] | 0xA0;
	pinters->iFeature[0][imax^1] = 0x40;
	pinters->iFeature[1][imin] = iEdge[imin][1] | 0xA0;
	pinters->iFeature[1][imin^1] = 0x40;
 
	return 1;
}

int tri_box_intersection(const triangle *ptri, const box *pbox, prim_inters *pinters)
{
	Vec3r dp,dp1,pt[3],n,ptbox;
	quotient t[3][2],t1[2],dist,mindist;
	int i,j,idir,iStart,iStartBest,iEndBest,bOutside[3],bBest,bEnd,bSwitch,iEndCur,bState;
	real sg;
	Vec3i axsg;

	pt[0] = pbox->Basis*(ptri->pt[0]-pbox->center);
	pt[1] = pbox->Basis*(ptri->pt[1]-pbox->center);
	pt[2] = pbox->Basis*(ptri->pt[2]-pbox->center);
	n = pbox->Basis*ptri->n;

	for(i=0,iStart=-1; i<3; i++) {
		bOutside[i] = isneg(pbox->size.x-fabs_tpl(pt[i].x)) | isneg(pbox->size.y-fabs_tpl(pt[i].y)) | isneg(pbox->size.z-fabs_tpl(pt[i].z));
		(iStart&=~-bOutside[i]) |= i&-bOutside[i];
	}	

	if (iStart<0) // all triangle points are inside the box
		return 0;

	if (bOutside[0]+bOutside[1]+bOutside[2]<3) {
		// if at least one triangle point is inside the box, use the closest box face normal as intersection normal
		i = bOutside[0]+(bOutside[0]&bOutside[1]); // i - index of the 1st inside vertex
		pinters->ptbest = ptri->pt[i];
		dp(fabs_tpl(fabs_tpl(pt[i].x)-pbox->size.x), fabs_tpl(fabs_tpl(pt[i].y)-pbox->size.y), fabs_tpl(fabs_tpl(pt[i].z)-pbox->size.z));
		j = idxmin3(dp); // j - the closest box face
		pinters->n = pbox->Basis.GetRow(j)*-sgnnz(pt[i][j]);
		pinters->nBestPtVal = 6;
	}

	i=iStart; iEndCur=iStartBest=iEndBest=-1; bState=0; mindist.set(1,0);
	do {
		dp = pt[dec_mod3[i]]-pt[i];
		t[i][0].set(0,1); t[i][1].set(1,1);
		for(j=0;j<3;j++) { // crop edge with box sides (plane sandwiches)
			idir = isneg(dp[j]); sg = 1-(idir<<1);
			t1[idir].set((-pbox->size[j]-pt[i][j])*sg, dp[j]*sg);
			t1[idir^1].set((pbox->size[j]-pt[i][j])*sg,dp[j]*sg);
			t[i][0] = max(t[i][0],t1[0]);
			t[i][1] = min(t[i][1],t1[1]);
		}

		if (t[i][1]>t[i][0]) // edge intersects the box
		for(j=0;j<2;j++) {
			//bVertexContact = isneg(sqr(fabs_tpl(t[i][j].x*2-t[i][j].y)-t[i][j].y)*dplen2 - pinters->minPtDist2*sqr(t[i][j].y)*4);
			bSwitch = t[i][j].isin01();
			bEnd = bSwitch & bState; // remember starting (ccw) point of every stripe (it'll serve as end point in result)
			(iEndCur&=~-bEnd) |= (i<<1|j)&-bEnd;
			// if a point is a stripe start, check if it is the current closest to the suggested stripe start
			dp1 = (ptri->pt[i]-pinters->pt[1]);	
			dist.set((dp1*t[i][j].y+(ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][j].x).len2(), sqr(t[i][j].y));
			bBest = isneg(dist-mindist) & bSwitch & (bState^1);
			(mindist.x*=1-bBest) += dist.x*bBest;
			(mindist.y*=1-bBest) += dist.y*bBest;
			(iStartBest&=~-bBest) |= (i<<1|j)&-bBest;
			(iEndBest&=~-bBest) |= iEndCur&-bBest;
			bState ^= bSwitch;
		}
	}	while((i=dec_mod3[i])!=iStart);

	if (iStartBest<0) { // triangle edges do not intersect box
		// find the box vertices that have the min. and max. (signed) distance to place
		// check if the line between them intersects the triangle
		Vec3r ptmin(pbox->size.x*-sgnnz(n.x),pbox->size.y*-sgnnz(n.y),pbox->size.z*-sgnnz(n.z));
		dp = ptmin*-2;
		t1[0].set((pt[0]-ptmin)*n,dp*n);
		bBest = t1[0].isin01();
		ptmin = ptmin*t1[0].y + dp*t1[0].x;
		for(i=0; i<3; i++)	// check if this point lies in triangle's voronoi region
			bBest &= isneg((ptmin-pt[i]*t1[0].y ^ pt[inc_mod3[i]]-pt[i])*n);
		if (!bBest)
			return 0;

		pinters->pt[0] = pinters->pt[1] = (ptmin/t1[0].y)*pbox->Basis+pbox->center;
		pinters->iFeature[0][0] = pinters->iFeature[1][0] = 0x40;
		pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;
		pinters->ptbest = pinters->pt[0];
		pinters->n = ptri->n;
		pinters->nBestPtVal = 1000;
		goto haveinters;
	}

	iEndBest = iEndBest-(iEndBest>>31) | iEndCur&iEndBest>>31;
	if (iEndBest<0)
		return 0;

	i = iStartBest>>1;
	pinters->pt[0] = ptri->pt[i] + (ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][iStartBest&1].val();
	pinters->iFeature[0][0] = 0xA0 | dec_mod3[i];
	pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;

	i = iEndBest>>1;
	pinters->pt[1] = ptri->pt[i] + (ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][iEndBest&1].val();
	pinters->iFeature[1][0] = 0xA0 | dec_mod3[i];

	haveinters:
	iStart = idxmax3(n.abs());
	// if triangle's area is comparable with the most parallel box face, check box edges vs triangle also
	if ((pt[1]-pt[0]^pt[2]-pt[0]).len2() > sqr(pbox->size[inc_mod3[iStart]]*pbox->size[dec_mod3[iStart]])) { 
		int nborderpt0 = pinters->nborderpt;
		for(i=0;i<12;i++) {
			idir = i>>2; axsg[idir]=-1; axsg[inc_mod3[idir]]=((i&1)<<1)-1; axsg[dec_mod3[idir]]=(i&2)-1;
			ptbox(pbox->size.x*axsg.x, pbox->size.y*axsg.y, pbox->size.z*axsg.z);
			t1[0].set((pt[0]-ptbox)*n,n[idir]);
			if (inrange(t1[0].x,(real)0,pbox->size[idir]*t1[0].y*2)) {
				t1[0].fixsign();
				(ptbox*=t1[0].y)[idir] += t1[0].x;
				for(j=0,bSwitch=1;j<3;j++)
					bSwitch &= isneg((ptbox-pt[j]*t1[0].y ^ pt[inc_mod3[j]]-pt[j])*n);
				if (bSwitch & isneg(pinters->nborderpt-pinters->nbordersz))
					pinters->ptborder[pinters->nborderpt++] = (ptbox/t1[0].y)*pbox->Basis+pbox->center;
			}
		}
		// only mark this triangle's normal as intersection normal if the box has at least 2 edge intersections
		// with it, and the box center is in triangle's Voronoi region
		if (fabs_tpl(n[iStart])>0.98f && pinters->nborderpt>nborderpt0+1 && pinters->nBestPtVal<6) {
			ptbox.zero()[iStart] = pbox->size[iStart]*sgnnz(pt[0][iStart]);
			if (isneg((ptbox-pt[0]^pt[1]-pt[0])*n) & isneg((ptbox-pt[1]^pt[2]-pt[1])*n) & isneg((ptbox-pt[2]^pt[0]-pt[2])*n)) {
				pinters->nBestPtVal = 6; pinters->n = ptri->n;
				pinters->ptbest = pinters->ptborder[nborderpt0];
			}
		}
	}

	return 1;
}

int box_tri_intersection(const box *pbox, const triangle *ptri, prim_inters *pinters)
{
	int res = tri_box_intersection(ptri,pbox,pinters);
	Vec3 pt; int iFeature;
	pt=pinters->pt[0]; pinters->pt[0]=pinters->pt[1]; pinters->pt[1]=pt;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	pinters->n.Flip();
	return res;
}

int tri_cylinder_intersection(const triangle *ptri, const cylinder *pcyl, prim_inters *pinters)
{
	Vec3r pc,dp,dp1,vec1,vec0;
	Vec3 pt,axisx,axisy;
	Vec2 pt2d;
	real r2,a,b,c,d;
	quotient t[3][2],dist,mindist;
	int i,j,iStart,iStartBest,iEndBest,bOutside,bEnd,iEndCur,bSwitch,bState,bBest,sg,bInters;//,bVertexContact,bPrevVertexContact;
	r2 = sqr(pcyl->r);

	for(i=0,iStart=-1; i<3; i++) {
		pc = ptri->pt[i]-pcyl->center;
		bOutside = isneg(r2 - (pc^pcyl->axis).len2()) | isneg(pcyl->hh - fabs_tpl(pc*pcyl->axis));
		(iStart&=~-bOutside) |= i&-bOutside;
	}

	if (iStart<0) // all triangle points are inside the cylinder
		return 0;

	i=iStart; iEndCur=iStartBest=iEndBest=-1; bState=0; mindist.set(1,0);
	do {	// find edge-cylinder intersections
		dp = ptri->pt[dec_mod3[i]]-ptri->pt[i]; pc = ptri->pt[i]-pcyl->center;
		t[i][0].x = t[i][1].x = -pc*pcyl->axis;
		t[i][0].y = t[i][1].y = dp*pcyl->axis; sg = sgnnz(t[i][0].y); 
		t[i][sg+1>>1].x += pcyl->hh; t[i][sg+1>>1^1].x -= pcyl->hh;
		t[i][0].x*=sg; t[i][0].y*=sg; t[i][1].x*=sg; t[i][1].y*=sg;

		vec1 = dp^pcyl->axis; vec0 = pc^pcyl->axis;
		a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2; d = b*b-a*c;
		if (d>=0 && vec1.len2()>(real)1E-10) {
			d = sqrt_tpl(d); 
			t[i][0] = max(t[i][0],quotient(-b-d,a));
			t[i][1] = min(t[i][1],quotient(-b+d,a));
			bInters = isneg(t[i][0]-t[i][1]);
		} else
			bInters = isneg(vec0.len2()-r2);

		if (bInters) for(j=0;j<2;j++) {
			//bVertexContact = isneg(sqr(t[i][j].x-t[i][j].y)*dplen2-pinters->minPtDist2*sqr(t[i][j].y)); // t is close to 1
			//bPrevVertexContact = isneg(sqr(t[i][j].x)*dplen2-pinters->minPtDist2*sqr(t[i][j].y));	// t is close to 0
			bSwitch = isneg(fabs_tpl(t[i][j].x*2-t[i][j].y)-t[i][j].y); // t is in (0..1)
	 		bEnd = bSwitch & bState; // remember starting (ccw) point of every stripe (it'll serve as end point in result)
			(iEndCur&=~-bEnd) |= (i<<1|j)&-bEnd;
			// if a point is a stripe start, check if it is the current closest to the suggested stripe start
			dp1 = (ptri->pt[i]-pinters->pt[1]);	
			dist.set((dp1*t[i][j].y+(ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][j].x).len2(), sqr(t[i][j].y));
			bBest = isneg(dist-mindist) & bSwitch & (bState^1);
			(mindist.x*=1-bBest) += dist.x*bBest;
			(mindist.y*=1-bBest) += dist.y*bBest;
			(iStartBest&=~-bBest) |= (i<<1|j)&-bBest;
			(iEndBest&=~-bBest) |= iEndCur&-bBest;
			bState ^= bSwitch;
		}
	}	while((i=dec_mod3[i])!=iStart);

	if (iStartBest<0) { // triangle edges do not intersect cylinder
		Vec3r ptmin,ptmax;
		ptmax = ptri->n-pcyl->axis*(ptri->n*pcyl->axis);
		if (ptmax.len2()>1E-4f)
			ptmax.normalize();
		else
			ptmax.zero();
		// choose the points on cylinder caps that have the maximum and the minimum projections on triangle normal
		ptmax = ptmax*pcyl->r + pcyl->axis*pcyl->hh*(j=sgnnz(pcyl->axis*ptri->n));
		ptmin = pcyl->center - ptmax; ptmax += pcyl->center; pt = ptmin;
		// check if a line between them intersects triangle
		t[0][0].set((ptri->pt[0]-ptmin)*ptri->n, (ptmax-ptmin)*ptri->n);
		bInters = isneg(fabs_tpl(t[0][0].x*2-t[0][0].y)-t[0][0].y); // t is in (0..1)
		ptmin = ptmin*t[0][0].y + (ptmax-ptmin)*t[0][0].x;
		for(i=0; i<3; i++)	// check if this point lies in triangle's voronoi region
			bInters &= isneg((ptmin-ptri->pt[i]*t[0][0].y ^ ptri->pt[inc_mod3[i]]-ptri->pt[i])*ptri->n);
		if (!bInters)
			return 0;

		pinters->pt[0] = pinters->pt[1] = ptmin/t[0][0].y;
		pinters->iFeature[0][0] = pinters->iFeature[1][0] = 0x40;
		pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;
		pinters->ptbest = pinters->pt[0];
		pinters->n = ptri->n;	// force triangle normal as global intersection normal in this case
		pinters->nBestPtVal = 1000;

		// check the bottommost cylinder edge (wrt triangle normal)
		if (inrange((pt-ptri->pt[0])*ptri->n, (pcyl->hh+pcyl->r)*-0.2f,0.0f) &&
				(pt-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n<0 && (pt-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n<0 &&
				(pt-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n<0 && pinters->nborderpt<pinters->nbordersz) 
			pinters->ptborder[pinters->nborderpt++] = pt;
		dist.set((ptri->pt[0]-pt)*ptri->n, fabs_tpl(pcyl->axis*ptri->n));
		if (inrange(dist.x, (real)0,pcyl->hh*2*dist.y))
			pt += pcyl->axis*(j*dist.val());
		else
			pt += pcyl->axis*(pcyl->hh*2*j);
		if (inrange((pt-ptri->pt[0])*ptri->n, (pcyl->hh+pcyl->r)*-0.2f,0.0f) &&
				(pt-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n<0 && (pt-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n<0 &&
				(pt-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n<0 && pinters->nborderpt<pinters->nbordersz) 
			pinters->ptborder[pinters->nborderpt++] = pt;

		// add circle points that are below the triangle as ptborder
		pt = pcyl->center - pcyl->axis*(pcyl->hh*j);
		axisx = pcyl->axis.GetOrthogonal().normalized()*pcyl->r;
		axisy = axisx^pcyl->axis;
		for(i=0,pt2d.set(1,0); i<8; i++) {
			Vec3 pt1 = pinters->ptborder[pinters->nborderpt] = pt+axisx*pt2d.x+axisy*pt2d.y;
			pinters->nborderpt += isneg((pt1-ptri->pt[0])*ptri->n) & isneg(pinters->nborderpt-pinters->nbordersz);
			float tmp=pt2d.x; pt2d.x=(pt2d.x-pt2d.y)*(sqrt2/2); pt2d.y=(tmp+pt2d.y)*(sqrt2/2); // rotate by 45 degrees
		}
		return 1;
	}

	iEndBest = iEndBest-(iEndBest>>31) | iEndCur&iEndBest>>31;
	if (iEndBest<0)
		return 0;

	i = iStartBest>>1;
	pinters->pt[0] = ptri->pt[i] + (ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][iStartBest&1].val();
	pinters->iFeature[0][0] = 0xA0 | dec_mod3[i];
	pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;

	i = iEndBest>>1;
	pinters->pt[1] = ptri->pt[i] + (ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][iEndBest&1].val();
	pinters->iFeature[1][0] = 0xA0 | dec_mod3[i];

	// if cylinder cap's lowest (wrt trinagle normal) point is inside tringle's voronoi region, force triangle normal as global intersection normal
	// make this check only if the cylinder is not too below the triangle surface 
	pt = pcyl->center - pcyl->axis*(pcyl->hh*(j=sgnnz(pcyl->axis*ptri->n)));
	if ((pcyl->axis^ptri->n).len2()>0.001f) {
		pt += ((ptri->n^pcyl->axis)^pcyl->axis).normalized()*pcyl->r;
		if ((pt-ptri->pt[0])*ptri->n > (pcyl->hh+pcyl->r)*-0.2f &&
				(pt-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n<0 && (pt-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n<0 &&
				(pt-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n<0 && pinters->nborderpt<pinters->nbordersz) 
		{
			pinters->ptbest = pt;
			pinters->n = ptri->n; pinters->nBestPtVal = 1000;
			pinters->ptborder[pinters->nborderpt++] = pt;
		}
		dist.set((ptri->pt[0]-pt)*ptri->n, fabs_tpl(pcyl->axis*ptri->n));
		if (inrange(dist.x, (real)0,pcyl->hh*2*dist.y))
			pt += pcyl->axis*(j*dist.val());
		else
			pt += pcyl->axis*(pcyl->hh*2*j);
		if (inrange((pt-ptri->pt[0])*ptri->n, (pcyl->hh+pcyl->r)*-0.2f,0.0f) &&
				(pt-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n<0 && (pt-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n<0 &&
				(pt-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n<0 && pinters->nborderpt<pinters->nbordersz) 
			pinters->ptborder[pinters->nborderpt++] = pt;
	} else if ((pt-ptri->pt[0])*ptri->n > (pcyl->hh+pcyl->r)*-0.2f) {
		pinters->ptbest = pt;
		pinters->n = ptri->n; pinters->nBestPtVal = 1000;
		// add circle points to ptborder if they are inside the triangle and below it
		axisx = pcyl->axis.GetOrthogonal().normalized()*pcyl->r;
		axisy = axisx^pcyl->axis;
		for(i=0,pt2d.set(1,0); i<8; i++) {
			Vec3 pt1 = pinters->ptborder[pinters->nborderpt] = pt+axisx*pt2d.x+axisy*pt2d.y;
			pinters->nborderpt += isneg((pt1-ptri->pt[0])*ptri->n) & isneg((pt1-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n) & 
				isneg((pt1-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n) & isneg((pt1-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n) &
				isneg(pinters->nborderpt-pinters->nbordersz);
			float tmp=pt2d.x; pt2d.x=(pt2d.x-pt2d.y)*(sqrt2/2); pt2d.y=(tmp+pt2d.y)*(sqrt2/2); // rotate by 45 degrees
		}
	}

	return 1;
}

int cylinder_tri_intersection(const cylinder *pcyl, const triangle *ptri, prim_inters *pinters)
{
	int res = tri_cylinder_intersection(ptri,pcyl,pinters);
	Vec3 pt; int iFeature;
	pt=pinters->pt[0]; pinters->pt[0]=pinters->pt[1]; pinters->pt[1]=pt;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}

int tri_capsule_intersection(const triangle *ptri, const capsule *pcaps, prim_inters *pinters)
{
	Vec3r pc,dp,dp1,vec1,vec0;
	Vec3 pt,axisx,axisy;
	Vec2 pt2d;
	real r2,a,b,c,d;
	quotient t[3][2],dist,mindist;
	int i,j,iStart,iStartBest,iEndBest,bOutside,bEnd,iEndCur,bSwitch,bState,bBest,sg,bInters;
	r2 = sqr(pcaps->r);

	for(i=0,iStart=-1; i<3; i++) {
		pc = ptri->pt[i]-pcaps->center;
		bOutside = (isneg(r2 - (pc^pcaps->axis).len2()) | isneg(pcaps->hh - fabs_tpl(pc*pcaps->axis))) &
							 isneg(r2 - (pc-pcaps->axis*(sgnnz(pc*pcaps->axis)*pcaps->hh)).len2());
		(iStart&=~-bOutside) |= i&-bOutside;
	}

	if (iStart<0) // all triangle points are inside the capsule
		return 0;

	i=iStart; iEndCur=iStartBest=iEndBest=-1; bState=0; mindist.set(1,0);
	do {	// find edge-capsule intersections
		dp = ptri->pt[dec_mod3[i]]-ptri->pt[i]; pc = ptri->pt[i]-pcaps->center;
		t[i][0].x = t[i][1].x = -pc*pcaps->axis;
		t[i][0].y = t[i][1].y = dp*pcaps->axis; sg = sgnnz(t[i][0].y); 
		t[i][sg+1>>1].x += pcaps->hh; t[i][sg+1>>1^1].x -= pcaps->hh;
		t[i][0].x*=sg; t[i][0].y*=sg; t[i][1].x*=sg; t[i][1].y*=sg;

		a = dp.len2(); b = dp*(pc+pcaps->axis*(pcaps->hh*sg)); c = (pc+pcaps->axis*(pcaps->hh*sg)).len2()-r2;
		if ((d=b*b-a*c)>=0)	{
			d = sqrt_tpl(d);
			if (quotient(-b+d,a) < t[i][0]) {
				t[i][0].set(-b-d,a); t[i][1].set(-b+d,a);
			} else
				t[i][0] = min(t[i][0],quotient(-b-d,a));
		}
		b = dp*(pc-pcaps->axis*(pcaps->hh*sg)); c = (pc-pcaps->axis*(pcaps->hh*sg)).len2()-r2;
		if ((d=b*b-a*c)>=0)	{
			d = sqrt_tpl(d);
			if (quotient(-b-d,a) > t[i][1]) {
				t[i][0].set(-b-d,a); t[i][1].set(-b+d,a);
			} else
				t[i][1] = max(t[i][1],quotient(-b+d,a));
		}

		vec1 = dp^pcaps->axis; vec0 = pc^pcaps->axis;
		a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2; d = b*b-a*c;
		if (d>=0 && vec1.len2()>(real)1E-10) {
			d = sqrt_tpl(d); 
			t[i][0] = max(t[i][0],quotient(-b-d,a));
			t[i][1] = min(t[i][1],quotient(-b+d,a));
			bInters = isneg(t[i][0]-t[i][1]);
		} else
			bInters = isneg(vec0.len2()-r2);

		if (bInters) for(j=0;j<2;j++) {
			bSwitch = isneg(fabs_tpl(t[i][j].x*2-t[i][j].y)-t[i][j].y); // t is in (0..1)
	 		bEnd = bSwitch & bState; // remember starting (ccw) point of every stripe (it'll serve as end point in result)
			(iEndCur&=~-bEnd) |= (i<<1|j)&-bEnd;
			// if a point is a stripe start, check if it is the current closest to the suggested stripe start
			dp1 = (ptri->pt[i]-pinters->pt[1]);	
			dist.set((dp1*t[i][j].y+(ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][j].x).len2(), sqr(t[i][j].y));
			bBest = isneg(dist-mindist) & bSwitch & (bState^1);
			(mindist.x*=1-bBest) += dist.x*bBest;
			(mindist.y*=1-bBest) += dist.y*bBest;
			(iStartBest&=~-bBest) |= (i<<1|j)&-bBest;
			(iEndBest&=~-bBest) |= iEndCur&-bBest;
			bState ^= bSwitch;
		}
	}	while((i=dec_mod3[i])!=iStart);

	if (iStartBest<0) { // triangle edges do not intersect capsule
		Vec3r ptmin,ptmax;
		// choose the points on capsule caps that have the maximum and the minimum projections on triangle normal
		ptmax = ptri->n*pcaps->r + pcaps->axis*(pcaps->hh*(j=sgnnz(pcaps->axis*ptri->n)));
		ptmin = pcaps->center - ptmax; ptmax += pcaps->center; pt = ptmin;
		// check if a line between them intersects triangle
		t[0][0].set((ptri->pt[0]-ptmin)*ptri->n, (ptmax-ptmin)*ptri->n);
		bInters = isneg(fabs_tpl(t[0][0].x*2-t[0][0].y)-t[0][0].y); // t is in (0..1)
		ptmin = ptmin*t[0][0].y + (ptmax-ptmin)*t[0][0].x;
		for(i=0; i<3; i++)	// check if this point lies in triangle's voronoi region
			bInters &= isneg((ptmin-ptri->pt[i]*t[0][0].y ^ ptri->pt[inc_mod3[i]]-ptri->pt[i])*ptri->n);
		if (!bInters)
			return 0;

		pinters->pt[0] = pinters->pt[1] = ptmin/t[0][0].y;
		pinters->iFeature[0][0] = pinters->iFeature[1][0] = 0x40;
		pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;
		pinters->ptbest = pinters->pt[0];
		pinters->n = ptri->n;	// force triangle normal as global intersection normal in this case
		pinters->nBestPtVal = 1000;

		// check the bottommost capsule edge (wrt triangle normal)
		if (inrange((pt-ptri->pt[0])*ptri->n, (pcaps->hh+pcaps->r)*-0.2f,0.0f) &&
				(pt-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n<0 && (pt-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n<0 &&
				(pt-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n<0 && pinters->nborderpt<pinters->nbordersz) 
			pinters->ptborder[pinters->nborderpt++] = pt;
		if (fabs_tpl(ptri->n*pcaps->axis) < 0.1f) {
			dist.set((ptri->pt[0]-pt)*ptri->n, fabs_tpl(pcaps->axis*ptri->n));
			if (inrange(dist.x, (real)0,pcaps->hh*2*dist.y))
				pt += pcaps->axis*(j*dist.val());
			else
				pt += pcaps->axis*(pcaps->hh*2*j);
			if (inrange((pt-ptri->pt[0])*ptri->n, (pcaps->hh+pcaps->r)*-0.2f,0.0f) &&
					(pt-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n<0 && (pt-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n<0 &&
					(pt-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n<0 && pinters->nborderpt<pinters->nbordersz) 
				pinters->ptborder[pinters->nborderpt++] = pt;
		}
		return 1;
	}

	iEndBest = iEndBest-(iEndBest>>31) | iEndCur&iEndBest>>31;
	if (iEndBest<0)
		return 0;

	i = iStartBest>>1;
	pinters->pt[0] = ptri->pt[i] + (ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][iStartBest&1].val();
	pinters->iFeature[0][0] = 0xA0 | dec_mod3[i];
	pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;

	i = iEndBest>>1;
	pinters->pt[1] = ptri->pt[i] + (ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][iEndBest&1].val();
	pinters->iFeature[1][0] = 0xA0 | dec_mod3[i];

	// if capsule cap's lowest (wrt trinagle normal) point is inside tringle's voronoi region, force triangle normal as global intersection normal
	pt = pcaps->center - pcaps->axis*(pcaps->hh*(j=sgnnz(pcaps->axis*ptri->n))) - ptri->n*pcaps->r;
	if (//(pt-ptri->pt[0])*ptri->n > (pcaps->hh+pcaps->r)*-0.2f &&
			(pt-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n<0 && (pt-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n<0 &&
			(pt-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n<0 && pinters->nborderpt<pinters->nbordersz) 
	{
		pinters->ptbest = pt;
		pinters->n = ptri->n; pinters->nBestPtVal = 1000;
		pinters->ptborder[pinters->nborderpt++] = pt;
	}
	if (fabs_tpl(ptri->n*pcaps->axis) < 0.1f) {
		dist.set((ptri->pt[0]-pt)*ptri->n, fabs_tpl(pcaps->axis*ptri->n));
		if (inrange(dist.x, (real)0,pcaps->hh*2*dist.y))
			pt += pcaps->axis*(j*dist.val());
		else
			pt += pcaps->axis*(pcaps->hh*2*j);
		if (inrange((pt-ptri->pt[0])*ptri->n, (pcaps->hh+pcaps->r)*-0.2f,0.0f) &&
				(pt-ptri->pt[0]^ptri->pt[1]-ptri->pt[0])*ptri->n<0 && (pt-ptri->pt[1]^ptri->pt[2]-ptri->pt[1])*ptri->n<0 &&
				(pt-ptri->pt[2]^ptri->pt[0]-ptri->pt[2])*ptri->n<0 && pinters->nborderpt<pinters->nbordersz) 
			pinters->ptborder[pinters->nborderpt++] = pt;
	}

	return 1;
}

int capsule_tri_intersection(const capsule *pcaps, const triangle *ptri, prim_inters *pinters)
{
	int res = tri_capsule_intersection(ptri,pcaps,pinters);
	Vec3 pt; int iFeature;
	pt=pinters->pt[0]; pinters->pt[0]=pinters->pt[1]; pinters->pt[1]=pt;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}

int tri_sphere_intersection(const triangle *ptri, const sphere *psphere, prim_inters *pinters)
{
	Vec3r pc,dp,dp1,vec1,vec0;
	real r2,a,b,c,d;
	quotient t[3][2],dist,mindist;
	int i,j,iStart,iStartBest,iEndBest,bOutside,bEnd,iEndCur,bState,bBest,bSwitch,bFaceContact=0;
	r2 = sqr(psphere->r);

	for(i=0,iStart=-1; i<3; i++) {
		bOutside = isneg(r2 - (ptri->pt[i]-psphere->center).len2());
		(iStart&=~-bOutside) |= i&-bOutside;
	}	

	if (iStart<0) // all triangle points are inside the sphere
		return 0;

	bOutside = isneg(psphere->r-fabs_tpl((psphere->center-ptri->pt[0])*ptri->n));
	for(i=0; i<3; i++)	// check if this point lies in triangle's voronoi region
		bOutside |= isneg((ptri->pt[inc_mod3[i]]-ptri->pt[i] ^ psphere->center-ptri->pt[i])*ptri->n);
	if (!bOutside) {
		// choose the point on the sphere that has minimum projection on triangle normal
		pinters->nBestPtVal = 1000;
		pinters->ptbest = psphere->center - ptri->n*psphere->r;
		pinters->n = ptri->n;
		bFaceContact = 1;
	}

	i=iStart; iEndCur=iStartBest=iEndBest=-1; bState=0; mindist.set(1,0);
	do {
		dp = ptri->pt[dec_mod3[i]]-ptri->pt[i]; pc = ptri->pt[i]-psphere->center;
		a = dp.len2(); b = dp*pc; c = pc.len2()-r2; d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d);
			t[i][0].set(-b-d,a);
			t[i][1].set(-b+d,a);

			if (t[i][0]<t[i][1]) for(j=0;j<2;j++) {
				bSwitch = isneg(fabs_tpl(t[i][j].x*2-t[i][j].y)-t[i][j].y); // t is in (0..1)
	 			bEnd = bSwitch & bState; // remember starting (ccw) point of every stripe (it'll serve as end point in result)
				(iEndCur&=~-bEnd) |= (i<<1|j)&-bEnd;
				// if a point is a stripe start, check if it is the current closest to the suggested stripe start
				dp1 = (ptri->pt[i]-pinters->pt[1]);	
				dist.set((dp1*t[i][j].y+(ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][j].x).len2(), sqr(t[i][j].y));
				bBest = isneg(dist-mindist) & bSwitch & (bState^1);
				(mindist.x*=1-bBest) += dist.x*bBest;
				(mindist.y*=1-bBest) += dist.y*bBest;
				(iStartBest&=~-bBest) |= (i<<1|j)&-bBest;
				(iEndBest&=~-bBest) |= iEndCur&-bBest;
				bState ^= bSwitch;
			}
		}
	}	while((i=dec_mod3[i])!=iStart);

	if (iStartBest<0) { // triangle edges do not intersect sphere
		if (!bFaceContact)
			return 0;
		pinters->pt[0] = pinters->pt[1] = pinters->ptbest;
		pinters->iFeature[0][0] = pinters->iFeature[1][0] = 0x40;
		pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;
		return 1;
	}

	iEndBest = iEndBest-(iEndBest>>31) | iEndCur&iEndBest>>31;
	if (iEndBest<0)
		return 0;

	i = iStartBest>>1;
	pinters->pt[0] = ptri->pt[i] + (ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][iStartBest&1].val();
	pinters->iFeature[0][0] = 0xA0 | dec_mod3[i];
	pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;

	i = iEndBest>>1;
	pinters->pt[1] = ptri->pt[i] + (ptri->pt[dec_mod3[i]]-ptri->pt[i])*t[i][iEndBest&1].val();
	pinters->iFeature[1][0] = 0xA0 | dec_mod3[i];

	return 1;
}

int sphere_tri_intersection(const sphere *psphere, const triangle *ptri, prim_inters *pinters)
{
	int res = tri_sphere_intersection(ptri,psphere,pinters);
	Vec3 pt; int iFeature;
	pt=pinters->pt[0]; pinters->pt[0]=pinters->pt[1]; pinters->pt[1]=pt;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}

int ray_tri_intersection(const ray * __restrict pray, const triangle * __restrict ptri, prim_inters * __restrict pinters)
{
	const float fDotDir0 = (pray->dir*ptri->n);

	if (sqr(fDotDir0) > pinters->minPtDist2*sqr(1E-4f))
	{
		const float fSign = (float)__fsel(fDotDir0, 1.0f, -1.0f);
		const float fDotPt0 = ((ptri->pt[0]-pray->origin)*ptri->n) * fSign;
		const float fDotDir = fDotDir0 * fSign;

		const Vec3r pt = pray->origin*fDotDir + pray->dir*fDotPt0;
		const real nlen2 = ptri->n.len2()*fDotDir;

		if (fDotDir < fabsf(fDotPt0*2.0f-fDotDir))
			return 0;

		const Vec3 edge0 = ptri->pt[1]-ptri->pt[0];
		if( sqr_signed(ptri->n*(edge0 ^ pt-ptri->pt[0]*fDotDir))+pinters->minPtDist2*edge0.len2()*nlen2 < 0.0f)
			return 0;

		const Vec3 edge1 = ptri->pt[2]-ptri->pt[1];
		if( sqr_signed(ptri->n*(edge1 ^ pt-ptri->pt[1]*fDotDir))+pinters->minPtDist2*edge1.len2()*nlen2 < 0.0f)
			return 0;

		const Vec3 edge2 = ptri->pt[0]-ptri->pt[2];
		if( sqr_signed(ptri->n*(edge2 ^ pt-ptri->pt[2]*fDotDir))+pinters->minPtDist2*edge2.len2()*nlen2 < 0.0f)
			return 0;

		const Vec3 outPt = pray->origin + pray->dir* (fDotPt0/fDotDir);	//div0 check needed?
		pinters->pt[0] = outPt;
		pinters->pt[1] = outPt;
		pinters->n = ptri->n;
		pinters->iFeature[0][0] = pinters->iFeature[1][0] = 0x40;
		pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x20;
		return 1;
	}

	return 0;
}

int tri_ray_intersection(const triangle *ptri, const ray *pray, prim_inters *pinters)
{
	int res = ray_tri_intersection(pray,ptri,pinters);
	int iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	pinters->n.Flip();
	return res;
}

int tri_plane_intersection(const triangle *ptri, const plane *pplane, prim_inters *pinters)
{
	// axes: n2 x n  n1 x n  n = n1 x n2
	Vec3r axes[3]; axes[2] = ptri->n^pplane->n;
	axes[0] = pplane->n^axes[2]; axes[1] = ptri->n^axes[2];
	real nlen2 = axes[2].len2(), ptw;
	Vec3r ptloc, pt0=Vec3r(ptri->pt[0]*ptri->n, -pplane->origin*pplane->n, ptri->pt[0]*axes[2])*GetMtxFromBasis(axes);

	Vec2r pt2d[3];
	int sgnx[3],nCross,iNotCross,bCross;
	quotient tmin,tmax;

	ptloc=ptri->pt[0]*nlen2-pt0; pt2d[0].set(axes[1]*ptloc,axes[2]*ptloc); sgnx[0]=sgnnz(pt2d[0].x);	
	ptloc=ptri->pt[1]*nlen2-pt0; pt2d[1].set(axes[1]*ptloc,axes[2]*ptloc); sgnx[1]=sgnnz(pt2d[1].x);	
	ptloc=ptri->pt[2]*nlen2-pt0; pt2d[2].set(axes[1]*ptloc,axes[2]*ptloc); sgnx[2]=sgnnz(pt2d[2].x);

	bCross = sgnx[0]*sgnx[1]-1>>1;
	nCross = -bCross;	iNotCross = 0;
	bCross = sgnx[1]*sgnx[2]-1>>1;
	nCross -= bCross;	iNotCross |= 1&~bCross;
	bCross = sgnx[2]*sgnx[0]-1>>1;
	nCross -= bCross;	(iNotCross&=bCross) |= 2&~bCross;
	if (nCross!=2) 
		return 0;

	tmax.x = pt2d[dec_mod3[iNotCross]] ^ pt2d[inc_mod3[iNotCross]];
	tmax.y = pt2d[dec_mod3[iNotCross]].x - pt2d[inc_mod3[iNotCross]].x;
	tmin.x = pt2d[iNotCross] ^ pt2d[dec_mod3[iNotCross]];
	tmin.y = pt2d[iNotCross].x - pt2d[dec_mod3[iNotCross]].x;
	
	ptw = (real)1.0/(nlen2*tmin.y*tmax.y);
	pinters->pt[0] = (pt0*tmin.y*tmax.y + axes[2]*tmin.x*tmax.y)*ptw;
	pinters->iFeature[0][0] = 0x20 | dec_mod3[iNotCross];
	pinters->pt[1] = (pt0*tmin.y*tmax.y + axes[2]*tmax.x*tmin.y)*ptw;
	pinters->iFeature[1][0] = 0x20 | inc_mod3[iNotCross];
	pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;

	return 1;
}

int plane_tri_intersection(const plane *pplane, const triangle *ptri, prim_inters *pinters)
{
	int res = tri_plane_intersection(ptri,pplane,pinters);
	Vec3 pt; int iFeature;
	pt=pinters->pt[0]; pinters->pt[0]=pinters->pt[1]; pinters->pt[1]=pt;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int box_box_intersection(const box *pbox1, const box *pbox2, prim_inters *pinters)
{
	Vec3r center[2],axes[3],dp,pt0;
	Vec3_tpl<int> axsg;
	int ibox,i,idir,iEdge;
	int bBest,sg,bLocInters[2];
	quotient t[2],t1[2];
	const box *pbox[2] = { pbox1,pbox2 };

	center[0] = pbox1->Basis*(pbox2->center-pbox1->center);
	center[1] = pbox2->Basis*(pbox1->center-pbox2->center);
	SetBasisFromMtx(axes, pbox2->Basis*pbox1->Basis.T());
	pinters->nborderpt = 0;

	for(ibox=0; ibox<2; ibox++) {
		for(iEdge=0;iEdge<12;iEdge++) {
			idir = iEdge>>2; axsg[idir]=-1; axsg[inc_mod3[idir]]=(((iEdge&1)<<1)-1); axsg[dec_mod3[idir]]=((iEdge&2)-1);
			pt0 = center[ibox] + axes[0]*(pbox[ibox^1]->size.x*axsg.x) + axes[1]*(pbox[ibox^1]->size.y*axsg.y) + axes[2]*(pbox[ibox^1]->size.z*axsg.z);
			dp = axes[idir];

			t[0].set(0,1); t[1].set(pbox[ibox^1]->size[idir]*2,1);
			for(i=0,bLocInters[0]=0,bLocInters[1]=0; i<3; i++) {
				sg = sgnnz(dp[i]); 
				t1[0].set(-pbox[ibox]->size[i]-pt0[i]*sg, fabs_tpl(dp[i]));
				t1[1].set( pbox[ibox]->size[i]-pt0[i]*sg, fabs_tpl(dp[i]));
				bLocInters[0] |= (bBest = isneg(t[0]-t1[0]));	// t[0] = max(t[0],t1[0]);
				t[0].x = t[0].x*(1-bBest) + t1[0].x*bBest;
				t[0].y = t[0].y*(1-bBest) + t1[0].y*bBest;
				bLocInters[1] |= (bBest = isneg(t1[1]-t[1]));	// t[1] = min(t[1],t1[1]);
				t[1].x = t[1].x*(1-bBest) + t1[1].x*bBest;
				t[1].y = t[1].y*(1-bBest) + t1[1].y*bBest;
			}
			if (t[0]<t[1]) {
				if (bLocInters[0])
					pinters->ptborder[pinters->nborderpt++] = (pt0+dp*t[0].val())*pbox[ibox]->Basis + pbox[ibox]->center;
				if (bLocInters[1])
					pinters->ptborder[pinters->nborderpt++] = (pt0+dp*t[1].val())*pbox[ibox]->Basis + pbox[ibox]->center;
			}
		}
		TransposeBasis(axes);
	}

	if (!pinters->nborderpt)
		return 0;

	pinters->pt[0] = pinters->pt[1] = pinters->ptborder[0];
	pinters->n.zero(); // normal and features not filled
	pinters->iFeature[0][0]=pinters->iFeature[0][1]=pinters->iFeature[1][0]=pinters->iFeature[1][1] = 0x40;
	return 1;
}


int box_cylinder_intersection(const box *pbox, const cylinder *pcyl, prim_inters *pinters)
{
	Vec3r axis,center,n,pt,vec0,vec1,size,c,nx;
	int i,ix,iy,iz,idir,icap,isgz,imask,bNega,bNegad,bInRange,bLocInters[2],sg,bBest;
	quotient t[2],t1[2];
	real ka,kb,kc,kd,rnz,rx2,rx,ry,r=pcyl->r,hh=pcyl->hh;
	axis = pbox->Basis*pcyl->axis;
	center = pbox->Basis*(pcyl->center-pbox->center);
	size = pbox->size;
	pinters->nborderpt = 0;

	// check box edges - cylinder intersections
	for(iz=0;iz<3;iz++) {
		ix = inc_mod3[iz]; iy = dec_mod3[iz];
		for(i=0;i<4;i++) {
			// edge-cap intersections
			pt[ix]=size[ix]*((i<<1&2)-1); pt[iy]=size[iy]*((i&2)-1); pt[iz]=0;
			t[0].set((center-pt)*axis-hh, axis[iz]); (pt*=t[0].y)[iz]=t[0].x;
			if (isneg(fabs_tpl(t[0].x)-size[iz]*fabs_tpl(t[0].y)) & isneg((pt-center*t[0].y^axis).len2()-sqr(r*t[0].y)))
				pinters->ptborder[pinters->nborderpt++] = (pt/t[0].y)*pbox->Basis+pbox->center;
			t[0].x+=hh*2; pt[iz]=t[0].x;
			if (isneg(fabs_tpl(t[0].x)-size[iz]*fabs_tpl(t[0].y)) & isneg((pt-center*t[0].y^axis).len2()-sqr(r*t[0].y)))
				pinters->ptborder[pinters->nborderpt++] = (pt/t[0].y)*pbox->Basis+pbox->center;

			// edge-side intersections
			pt[ix]=size[ix]*((i<<1&2)-1); pt[iy]=size[iy]*((i&2)-1); pt[iz]=0;
			vec0=pt-center^axis; vec1=-cross_with_ort(axis,iz);
			ka=vec1*vec1; kb=vec0*vec1; kc=vec0*vec0-r*r; kd=kb*kb-ka*kc;
			if (kd>0) {
				kd=sqrt_tpl(kd); t[0].set(-kb-kd,ka); (pt*=t[0].y)[iz]=t[0].x;
				if (isneg(fabs_tpl(t[0].x)-size[iz]*t[0].y) & isneg(fabs_tpl((pt-center*t[0].y)*axis)-hh*t[0].y))
					pinters->ptborder[pinters->nborderpt++] = (pt/t[0].y)*pbox->Basis+pbox->center;
				t[0].x+=kd*2; pt[iz]=t[0].x;
				if (isneg(fabs_tpl(t[0].x)-size[iz]*t[0].y) & isneg(fabs_tpl((pt-center*t[0].y)*axis)-hh*t[0].y))
					pinters->ptborder[pinters->nborderpt++] = (pt/t[0].y)*pbox->Basis+pbox->center;
			}
		}
	}

	// check cylinder cap edges - box intersections
	for(iz=0;iz<3;iz++) if (fabs_tpl(axis[iz])<(real)0.999) {
		ix = inc_mod3[iz]; iy = dec_mod3[iz];
		(n = axis*axis[iz])[iz]-=1; 
		for(i=0;i<4;i++) {
			icap = (i<<1&2)-1; isgz = (i&2)-1; 
			if (sqr(center[iz]+axis[iz]*(hh*icap)-size[iz]*isgz)*n.len2()*fabs_tpl(n[iz]) < sqr(n[iz]*r)*fabs_tpl(n[iz])) {
				n.normalize();
				// inters_point(sg) = ( center.z + n*(size.z*isgz-center.z) + sg*nx*(r^2*nz^2-(size.z*isgz-center.z)^2*n^2)^1/2 )/n.z
				c = center+axis*(icap*hh); ry = size[iz]*isgz-c[iz]; rx2 = sqr(r*n[iz])-ry*ry; nx = n^axis;
				// for x,y coordinates check |inters_point(sg).x| < size.x, group into | a+sg*b^1/2 | < c
				ka = c[ix]*n[iz]+n[ix]*ry; kb = nx[ix]; kc = size[ix]*n[iz]; kd = kc*kc-ka*ka-kb*kb*rx2;
				bInRange = isneg(sqr(ka*kb*2)*rx2-kd*kd); bNegad = isneg(ka*kb*kd); bNega = isneg(ka*kb);
				imask =  (bNegad&bNega | (bNegad^1)&(bInRange^bNega))<<1 | ((bNegad^1)&(bNega^1) | bNegad&(bInRange^bNega^1));
				ka = c[iy]*n[iz]+n[iy]*ry; kb = nx[iy]; kc = size[iy]*n[iz]; kd = kc*kc-ka*ka-kb*kb*rx2;
				bInRange = isneg(sqr(ka*kb*2)*rx2-kd*kd); bNegad = isneg(ka*kb*kd); bNega = isneg(ka*kb);
				imask &= (bNegad&bNega | (bNegad^1)&(bInRange^bNega))<<1 | ((bNegad^1)&(bNega^1) | bNegad&(bInRange^bNega^1));
				if (imask) {
					rx = sqrt_tpl(max((real)0,rx2)); rnz = (real)1.0/n[iz];
					if (imask&1)
						pinters->ptborder[pinters->nborderpt++] = (c+(n*ry-nx*rx)*rnz)*pbox->Basis+pbox->center;
					if (imask&2)
						pinters->ptborder[pinters->nborderpt++] = (c+(n*ry+nx*rx)*rnz)*pbox->Basis+pbox->center;
				}
			}
		}
	}

	// check cylinder axis-box intersections
	t[0].set(-hh,1); t[1].set(hh,1);
	for(iz=bLocInters[0]=bLocInters[1]=0; iz<3; iz++) {
		idir = isneg(axis[iz]); sg = 1-(idir<<1); 
		t1[idir]. set((-size[iz]-center[iz])*sg, fabs_tpl(axis[iz]));
		t1[idir^1].set((size[iz]-center[iz])*sg, fabs_tpl(axis[iz]));
		bLocInters[0] |= (bBest = isneg(t[0]-t1[0]));	// t[0] = max(t[0],t1[0]);
		t[0].x = t[0].x*(bBest^1) + t1[0].x*bBest;
		t[0].y = t[0].y*(bBest^1) + t1[0].y*bBest;
		bLocInters[1] |= (bBest = isneg(t1[1]-t[1]));	// t[1] = min(t[1],t1[1]);
		t[1].x = t[1].x*(bBest^1) + t1[1].x*bBest;
		t[1].y = t[1].y*(bBest^1) + t1[1].y*bBest;
	}
	if (t[0]<t[1]) {
		if (bLocInters[0])
			pinters->ptborder[pinters->nborderpt++] = (center+axis*t[0].val())*pbox->Basis + pbox->center;
		if (bLocInters[1])
			pinters->ptborder[pinters->nborderpt++] = (center+axis*t[1].val())*pbox->Basis + pbox->center;
	}

	if (!pinters->nborderpt)
		return 0;

	pinters->pt[0] = pinters->pt[1] = pinters->ptborder[0];
	pinters->n.zero(); // normal and features not filled
	pinters->iFeature[0][0]=pinters->iFeature[0][1]=pinters->iFeature[1][0]=pinters->iFeature[1][1] = 0x40;
	return 1;
}

int cylinder_box_intersection(const cylinder *pcyl, const box *pbox, prim_inters *pinters)
{
	int res = box_cylinder_intersection(pbox,pcyl,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	pinters->n.Flip();
	return res;
}


int box_sphere_intersection(const box *pbox, const sphere *psphere, prim_inters *pinters)
{
	int idir;
	Vec3 center,dir;
	center = pbox->Basis*(psphere->center-pbox->center);

	for(idir=0;idir<3;idir++)
		dir[idir] = min(0.0f,pbox->size[idir]-fabs_tpl(center[idir]))*sgnnz(center[idir]); 
	pinters->ptborder[0]=pinters->pt[0]=pinters->pt[1] = (center+dir)*pbox->Basis+pbox->center;
	pinters->nborderpt = 1;
	pinters->n = psphere->center - pinters->pt[0];
	pinters->iFeature[0][0] = pinters->iFeature[1][0] = 0x40;
	pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;

	return isneg(dir.len2()-sqr(psphere->r));
}

int sphere_box_intersection(const sphere *psphere, const box *pbox, prim_inters *pinters)
{
	int res = box_sphere_intersection(pbox,psphere,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	pinters->n.Flip();
	return res;
}


int box_capsule_intersection(const box *pbox, const capsule *pcaps, prim_inters *pinters)
{
	Vec3r axis,center,n,pt,vec0,vec1,size;
	int i,ix,iy,iz,idir,icap,bLocInters[2],sg,bBest;
	quotient t[2],t1[2];
	real ka,kb,kc,kd,r=pcaps->r,hh=pcaps->hh;
	axis = pbox->Basis*pcaps->axis;
	center = pbox->Basis*(pcaps->center-pbox->center);
	size = pbox->size;
	pinters->nborderpt = 0;

	// check box edges - capsule intersections
	for(iz=0;iz<3;iz++) {
		ix = inc_mod3[iz]; iy = dec_mod3[iz];
		for(i=0;i<4;i++) {
			// edge-cap intersections
			pt[ix]=size[ix]*((i<<1&2)-1); pt[iy]=size[iy]*((i&2)-1); pt[iz]=0;
			sg = sgnnz(axis[iz]);	t[0].y=t[1].y = fabs_tpl(axis[iz]);
			t[0].x=t[1].x = ((center-pt)*axis)*sg; t[0].x -= hh; t[1].x += hh;

			kb=(-center[iz]-axis[iz]*(hh*sg)); kc=(pt-center-axis*(hh*sg)).len2()-r*r;
			if ((kd=kb*kb-kc)>=0)	{
				kd = sqrt_tpl(kd);
				if ((inrange((-kb-kd)*t[0].y, t[0].x,t[1].x)^1) & isneg(fabs_tpl(-kb-kd)-size[iz]))
					pt[iz] = -kb-kd, pinters->ptborder[pinters->nborderpt++] = pt*pbox->Basis+pbox->center;
				if ((inrange((-kb+kd)*t[0].y, t[0].x,t[1].x)^1) & isneg(fabs_tpl(-kb+kd)-size[iz]))
					pt[iz] = -kb+kd, pinters->ptborder[pinters->nborderpt++] = pt*pbox->Basis+pbox->center;
			}
			pt[iz]=0; kb=(-center[iz]+axis[iz]*(hh*sg)); kc=(pt-center+axis*(hh*sg)).len2()-r*r;
			if ((kd=kb*kb-kc)>=0)	{
				kd = sqrt_tpl(kd);
				if ((inrange((-kb-kd)*t[0].y, t[0].x,t[1].x)^1) & isneg(fabs_tpl(-kb-kd)-size[iz]))
					pt[iz] = -kb-kd, pinters->ptborder[pinters->nborderpt++] = pt*pbox->Basis+pbox->center;
				if ((inrange((-kb+kd)*t[0].y, t[0].x,t[1].x)^1) & isneg(fabs_tpl(-kb+kd)-size[iz]))
					pt[iz] = -kb+kd, pinters->ptborder[pinters->nborderpt++] = pt*pbox->Basis+pbox->center;
			}

			// edge-side intersections
			pt[ix]=size[ix]*((i<<1&2)-1); pt[iy]=size[iy]*((i&2)-1); pt[iz]=0;
			vec0=pt-center^axis; vec1=-cross_with_ort(axis,iz);
			ka=vec1*vec1; kb=vec0*vec1; kc=vec0*vec0-r*r; kd=kb*kb-ka*kc;
			if (kd>0) {
				kd=sqrt_tpl(kd); t[0].set(-kb-kd,ka); (pt*=t[0].y)[iz]=t[0].x;
				if (isneg(fabs_tpl(t[0].x)-size[iz]*t[0].y) & isneg(fabs_tpl((pt-center*t[0].y)*axis)-hh*t[0].y))
					pinters->ptborder[pinters->nborderpt++] = (pt/t[0].y)*pbox->Basis+pbox->center;
				t[0].x+=kd*2; pt[iz]=t[0].x;
				if (isneg(fabs_tpl(t[0].x)-size[iz]*t[0].y) & isneg(fabs_tpl((pt-center*t[0].y)*axis)-hh*t[0].y))
					pinters->ptborder[pinters->nborderpt++] = (pt/t[0].y)*pbox->Basis+pbox->center;
			}
		}
	}

	// check capsule axis-box intersections
	t[0].set(-hh-r,1); t[1].set(hh+r,1);
	for(iz=bLocInters[0]=bLocInters[1]=0; iz<3; iz++) {
		idir = isneg(axis[iz]); sg = 1-(idir<<1); 
		t1[idir]. set((-size[iz]-center[iz])*sg, fabs_tpl(axis[iz]));
		t1[idir^1].set((size[iz]-center[iz])*sg, fabs_tpl(axis[iz]));
		bLocInters[0] |= (bBest = isneg(t[0]-t1[0]));	// t[0] = max(t[0],t1[0]);
		t[0].x += (t1[0].x-t[0].x)*bBest; t[0].y += (t1[0].y-t[0].y)*bBest;
		bLocInters[1] |= (bBest = isneg(t1[1]-t[1]));	// t[1] = min(t[1],t1[1]);
		t[1].x += (t1[1].x-t[1].x)*bBest; t[1].y += (t1[1].y-t[1].y)*bBest;
	}
	if (t[0]<t[1]) {
		if (bLocInters[0])
			pinters->ptborder[pinters->nborderpt++] = (center+axis*t[0].val())*pbox->Basis + pbox->center;
		if (bLocInters[1])
			pinters->ptborder[pinters->nborderpt++] = (center+axis*t[1].val())*pbox->Basis + pbox->center;
	}

	// check capsule caps - box side intersections
	for(icap=0,center-=axis*hh; icap<2; icap++,center+=axis*(hh*2)) {
		for(iz=0;iz<3;iz++)
			vec0[iz] = min((real)0.0,size[iz]-fabs_tpl(center[iz]))*sgnnz(center[iz]); 
		if (vec0.len2()<r*r)
			pinters->ptborder[pinters->nborderpt++] = (center+vec0)*pbox->Basis+pbox->center;
	}

	if (!pinters->nborderpt)
		return 0;

	pinters->pt[0] = pinters->pt[1] = pinters->ptborder[0];
	pinters->n.zero(); // normal and features not filled
	pinters->iFeature[0][0]=pinters->iFeature[0][1]=pinters->iFeature[1][0]=pinters->iFeature[1][1] = 0x40;
	return 1;
}

int capsule_box_intersection(const capsule *pcaps, const box *pbox, prim_inters *pinters)
{
	int res = box_capsule_intersection(pbox,pcaps,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	pinters->n.Flip();
	return res;
}


int box_ray_intersection(const box *pbox, const ray *pray, prim_inters *pinters)
{
	quotient t[2],t1[2];
	int i,idir,iBest[2]={-1,-1},sg;
	Vec3r origin=pbox->Basis*(pray->origin-pbox->center), dir=pbox->Basis*pray->dir;

	t[0].set(0,1); t[1].set(1,1);
	for(i=0;i<3;i++) {
		idir = isneg(dir[i]); sg = 1-(idir<<1);
		t1[idir].set((-pbox->size[i]-origin[i])*sg, dir[i]*sg);
		t1[idir^1].set((pbox->size[i]-origin[i])*sg,dir[i]*sg);
		if (t1[0]>t[0]) // t[0] = max(t[0],t1[0]);
		{ t[0]=t1[0]; iBest[0]=i; }
		if (t1[1]<t[1])	// t[1] = min(t[1],t1[1]);
		{ t[1]=t1[1]; iBest[1]=i; }
		//bBest = isneg(t[0]-t1[0]); (iBest&=~-bBest)|=i&-bBest; 
		//t[0].x=t[0].x*(bBest^1)+t1[0].x*bBest; t[0].y=t[0].y*(bBest^1)+t1[0].y*bBest; 
		//t[1] = min(t[1],t1[1]);
	}

	if (t[0]>t[1] || (iBest[0]&iBest[1])==-1)
		return 0;

	i = -(iBest[0]>>31);
	pinters->pt[0]=pinters->pt[1] = pray->origin + pray->dir*t[i].val();
	pinters->n = pbox->Basis.GetRow(iBest[i]);
	pinters->n *= sgnnz(pinters->n*(pinters->pt[0]-pbox->center));
	pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x20;
	return 1;
}

int ray_box_intersection(const ray *pray, const box *pbox, prim_inters *pinters)
{
	int res = box_ray_intersection(pbox,pray,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int box_plane_intersection(const box *pbox, const plane *pplane, prim_inters *pinters)
{
	Vec3 dir =
		pbox->Basis.GetRow(0)*pbox->size.x*sgnnz(pbox->Basis.GetRow(0)*pplane->n) + 
		pbox->Basis.GetRow(1)*pbox->size.y*sgnnz(pbox->Basis.GetRow(1)*pplane->n) + 
		pbox->Basis.GetRow(2)*pbox->size.z*sgnnz(pbox->Basis.GetRow(2)*pplane->n);

	pinters->pt[0]=pinters->pt[1] = pbox->center-dir;
	pinters->n = -pplane->n;
	pinters->iFeature[0][0] = pinters->iFeature[1][0] = 0x40;
	pinters->iFeature[0][1] = pinters->iFeature[1][1] = 0x40;
	return isneg((pinters->pt[0]-pplane->origin)*pplane->n);
}

int plane_box_intersection(const plane *pplane, const box *pbox, prim_inters *pinters)
{
	int res = box_plane_intersection(pbox,pplane,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


inline int root_inrange(float a,float ad,float b,float hca,float d,float h) {
	int sg = sgnnz((-b+d)*ad+hca);
	return isneg(d*sqr_signed(ad)*sg - sqr_signed(h*a+(b*ad-hca)*sg));
}

int cylinder_cylinder_intersection(const cylinder *pcyl1, const cylinder *pcyl2, prim_inters *pinters)
{
  const cylinder *pcyl[2] = { pcyl1,pcyl2 };
	Vec3 center[2],dc,pt,axisx,axisy,u,l,a1;
	Vec2 c2d;
	float a,b,c,d,k,r0,r1,h,span2,p,q,cosa,sina,rsina;
	quotient t;
	int icyl,i,j,sg,bHasInters=0;
	pinters->nborderpt = 0;

	cosa = pcyl[0]->axis*pcyl[1]->axis; 
	sina = (axisx = pcyl[0]->axis^pcyl[1]->axis).len();	

	if (sina>0.003f) {
		axisx *= (rsina=1/sina);
		// check axis-axis closest points to lie within cylinder bounds
		dc = pcyl[1]->center-pcyl[0]->center;
		if (isneg(fabs_tpl(dc*axisx)-pcyl[0]->r-pcyl[1]->r) & 
				isneg(fabs_tpl((dc^pcyl[1]->axis)*axisx)-pcyl[0]->hh*sina) & 
				isneg(fabs_tpl((dc^pcyl[0]->axis)*axisx)-pcyl[1]->hh*sina))
			bHasInters = 1;
	} else
		axisx = pcyl[0]->axis.GetOrthogonal().normalized();

	for(icyl=0; icyl<2; icyl++) {
		axisy = pcyl[icyl^1]->axis^axisx;	
		r0 = pcyl[icyl]->r; r1 = pcyl[icyl^1]->r;

		// check for icyl axis - icyl^1 cap intersections
		t.set((pcyl[icyl^1]->center-pcyl[icyl]->center)*pcyl[icyl^1]->axis-pcyl[icyl^1]->hh, cosa);
		pt = (pcyl[icyl]->center-pcyl[icyl^1]->center)*t.y + pcyl[icyl]->axis*t.x;
		pt -= pcyl[icyl^1]->axis*(pt*pcyl[icyl^1]->axis);
		for(i=0; i<2; i++,t.x+=pcyl[icyl^1]->hh*2,pt+=pcyl[icyl]->axis*(pcyl[icyl^1]->hh*2))
			if (isneg(fabs_tpl(t.x)-fabs_tpl(t.y)*pcyl[icyl]->hh) & isneg(pt.len2()<sqr(r1)))
				bHasInters = 1;

		// check for icyl axis - icyl^1 side intersections
		center[0] = pcyl[icyl]->center-pcyl[icyl^1]->center;
		a = 1-sqr(cosa); 
		if (a>0.0001f) {
			b = pcyl[icyl]->axis*center[0]-(center[0]*pcyl[icyl^1]->axis)*cosa;
			c = center[0].len2()-sqr(pcyl[icyl^1]->axis*center[0])-sqr(r1);
			d = b*b-a*c; 
			if (d>0 &&
					(root_inrange(a,1,b,0,d,pcyl[icyl]->hh) && root_inrange(a,cosa,b,-(center[0]*pcyl[icyl^1]->axis)*a,d,pcyl[icyl^1]->hh) || 
					root_inrange(a,1,b,0,-d,pcyl[icyl]->hh) && root_inrange(a,cosa,b,-(center[0]*pcyl[icyl^1]->axis)*a,-d,pcyl[icyl^1]->hh)))
				bHasInters = 1;
		}

		// check for icyl cap - icyl^1 side intersections
		for(i=-1; i<=1; i+=2) {
			dc = center[0]+pcyl[icyl]->axis*(pcyl[icyl]->hh*i);
			c2d.set(-(dc*axisx), -(dc*axisy)); 
			if (fabs_tpl(cosa)<0.003f) {
				if (fabs_tpl(c2d.y) < r1 && inrange(sqr(r1)-sqr(c2d.y), sqr_signed(fabs_tpl(c2d.x)-r0),sqr_signed(fabs_tpl(c2d.x)+r0)) &&
						fabs_tpl(fabs_tpl(dc*pcyl[icyl^1]->axis)) < pcyl[icyl^1]->hh)
						//fabs_tpl((dc+axisx*(r0*sgnnz(c2d.x)))*pcyl[icyl^1]->axis) < pcyl[icyl^1]->hh)
					bHasInters = 1;
			} else {
				a = r0*r0*(1+sqr(cosa)) + r1*r1 - sqr(c2d.x) - sqr(c2d.y);
				b = sqr(r0*r0*cosa) + sqr(r1*r0)*(1+sqr(cosa)) - sqr(r0)*(sqr(c2d.x)*sqr(cosa)+sqr(c2d.y)); 
				c = sqr(r1*r0*r0*cosa);
				p = b-a*a*(1.0f/3); q = a*(b*(1.0f/6)-a*a*(1.0f/27))-c*0.5f;
				if (cube(p)*(1.0f/27)+sqr(q) > 0)	{
					// now check if the closest point on the edge to the icyl^1 axis is inside h range
					Vec2i sg2d(sgnnz(c2d.x),sgnnz(c2d.y));
					Vec2 pt2d,kpt2d,n2d,kn2d;
					kpt2d.set(r0*sg2d.x, r0*sg2d.y*fabs_tpl(cosa));
					kn2d.set(sg2d.x*fabs_tpl(cosa), sg2d.y);
					j = 1-sg2d.x*sg2d.y>>1;
					int iangle,idx[2] = {0,SINCOSTABSZ};
					do {
						iangle = idx[0]+idx[1]>>1;
						pt2d.set(g_costab[iangle]*kpt2d.x, g_sintab[iangle]*kpt2d.y);
						n2d.set(g_costab[iangle]*kn2d.x, g_sintab[iangle]*kn2d.y);
						idx[isneg(n2d^c2d-pt2d)^j] = iangle;
					} while(idx[1]-idx[0] > 1);

					if (fabs_tpl(dc*pcyl[icyl^1]->axis+sina*sgnnz(cosa)*g_sintab[iangle]*sg2d.y*r0*(icyl*2-1)) < pcyl[icyl^1]->hh) {
						bHasInters = 1;
						int icenter,idx1,imiddle,iquad,ipass;
						icenter = idx[0]*sg2d.x*sg2d.y + SINCOSTABSZ*(1-sg2d.x) & SINCOSTABSZ*4-1; 
						kpt2d.set(r0, r0*fabs_tpl(cosa));
						for(ipass=-1; ipass<=1; ipass+=2) {
							idx[0] = icenter; idx1=idx[1] = idx[0]+ipass*SINCOSTABSZ;
							do {
								imiddle = idx[0]+idx[1]>>1;
								iquad = imiddle>>SINCOSTABSZ_LOG2 & 3;
								iangle = (SINCOSTABSZ-1 & -(iquad&1)) + (imiddle & SINCOSTABSZ-1)*(1-(iquad&1)*2);
								pt2d.set(g_costab[iangle]*(1-((iquad^iquad<<1)&2))*kpt2d.x, g_sintab[iangle]*(1-(iquad&2))*kpt2d.y); 
								idx[isneg(sqr(r1)-len2(pt2d-c2d))] = imiddle;
							} while(fabs_tpl(idx[0]-idx[1])>2);
							pt2d.y = sgnnz(cosa)*g_sintab[iangle]*(1-(iquad&2))*r0;
							if (idx[1]!=idx1 && fabs_tpl(dc*pcyl[icyl^1]->axis+sina*pt2d.y*(icyl*2-1)) < pcyl[icyl^1]->hh) {
								Vec3 offs = dc+axisx*pt2d.x+(pcyl[icyl]->axis^axisx)*pt2d.y;
								if ((offs^pcyl[icyl^1]->axis).len2()<sqr(pcyl[icyl^1]->r))
									pinters->ptborder[pinters->nborderpt++] = offs+pcyl[icyl^1]->center;
							}
						}
					}
				}
			}
		}
	}

	// check for icyl cap - icyl^1 cap intersections
	if (sina>0.003f) {
		u = (pcyl[0]->axis*cosa-pcyl[1]->axis)*rsina;	
		l = pcyl[0]->axis^u; a1 = pcyl[1]->axis*rsina; 
		for(i=-1;i<=1;i+=2) for(j=-1;j<=1;j+=2) {
			center[0] = pcyl[0]->center+pcyl[0]->axis*(pcyl[0]->hh*i)-pcyl[1]->center;
			center[1] = pcyl[1]->axis*(pcyl[1]->hh*j);
			h = a1*(center[0]-=center[1]); center[0] += u*h;
			b = l*center[0]; 
			c = center[0].len2()-sqr(pcyl[1]->r);
			d = b*b-c; span2 = sqr(pcyl[0]->r)-sqr(h);
			sg = sgnnz(d-b*b); k = d-span2-b*b;
			if (min(span2,d) > 0) {
				if (inrange(b*b, d,span2) | isneg(sqr_signed(k)*sg-span2*sqr(b)*4))	{
					d = sqrt_tpl(d);
					for(sg=-1; sg<=1; sg+=2) if (sqr(-b+d*sg)<span2)
						pinters->ptborder[pinters->nborderpt++] = center[0]+l*(-b+d*sg)+center[1]+pcyl[1]->center;
				}
				if (sqr(2*center[0]*l)*span2 > max(0.0f,center[0].len2()+span2-sqr(pcyl[1]->r))) {
					span2 = sqrt_tpl(span2);
					for(sg=-1; sg<=1; sg+=2) if ((center[0]+l*(span2*sg)).len2()<sqr(pcyl[1]->r))
						pinters->ptborder[pinters->nborderpt++] = center[0]+l*(span2*sg)+center[1]+pcyl[1]->center;
				}
			}
		}
	}

	if (pinters->nborderpt)	{
		pinters->pt[0]=pinters->pt[1] = pinters->ptborder[0];
		return 1;
	}
	return bHasInters;
}


int cylinder_sphere_intersection(const cylinder *pcyl, const sphere *psphere, prim_inters *pinters)
{
	Vec3 dir,dc;
	float dh,h;
	int bOutside;

	bOutside = isneg(dh = pcyl->hh - fabs_tpl(h = (psphere->center-pcyl->center)*pcyl->axis));
	dir = pcyl->axis*dh*bOutside*sgnnz(h);
	bOutside = isneg(sqr(pcyl->r) - ((dc = psphere->center-pcyl->center) ^ pcyl->axis).len2());
	if (bOutside) {
		dc -= pcyl->axis*(dc*pcyl->axis); dc -= dc.normalized()*pcyl->r;
		dir += dc;
	}

	pinters->pt[0]=pinters->pt[1] = psphere->center+dir;
	pinters->n = psphere->center - pinters->pt[0];
	pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x40;
	return isneg(dir.len2() - sqr(psphere->r));
}

int sphere_cylinder_intersection(const sphere *psphere, const cylinder *pcyl, prim_inters *pinters)
{
	int res = cylinder_sphere_intersection(pcyl,psphere,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int cylinder_capsule_intersection(const cylinder *pcyl, const capsule *pcaps, prim_inters *pinters)
{
	Vec3 dc,axisx,axisy,dir;
	Vec2 c2d;
	float a,b,c,r0=pcyl->r,r1=pcaps->r,h,p,q,cosa,sina;
	int i,j,bHasInters=0,bOutside;
	pinters->nborderpt = 0;
	pinters->n.zero();

	cosa = pcyl->axis*pcaps->axis; 
	sina = (axisx = pcyl->axis^pcaps->axis).len();	

	if (sina>0.003f) {
		axisx /= sina;
		// check axis-axis closest points to lie within cylinder bounds
		dc = pcaps->center-pcyl->center;
		if (isneg(fabs_tpl(dc*axisx)-r0-r1) & 
				isneg(fabs_tpl((dc^pcaps->axis)*axisx)-pcyl->hh*sina) & 
				isneg(fabs_tpl((dc^pcyl->axis)*axisx)-pcaps->hh*sina))
			bHasInters = 1;
	} else
		axisx = pcyl->axis.GetOrthogonal().normalized();
	axisy = pcaps->axis^axisx;

	// check for capsule's spheres - cylinder intersections
	for(i=-1; i<=1; i+=2) {
		dc = pcaps->center+pcaps->axis*(pcaps->hh*i)-pcyl->center;
		bOutside = isneg(pcyl->hh - fabs_tpl(h=dc*pcyl->axis));
		dir = pcyl->axis*((pcyl->hh-fabs_tpl(h))*bOutside*sgnnz(h));
		bOutside = isneg(sqr(r0) - (dc^pcyl->axis).len2());
		if (bOutside) {
			dc -= pcyl->axis*(dc*pcyl->axis); dc -= dc.normalized()*r0;
			dir += dc;
		}
		if (dir.len2()<r1*r1) {
			pinters->ptborder[pinters->nborderpt++] = pcaps->center+pcaps->axis*(pcaps->hh*i)+dir;
			pinters->n = -dir;
			bHasInters = 1;
		}
	}

	// check for cylinder cap edge - capsule side intersections
	for(i=-1; i<=1; i+=2) {
		dc = pcyl->center+pcyl->axis*(pcyl->hh*i)-pcaps->center;
		c2d.set(-(dc*axisx), -(dc*axisy)); 
		if (fabs_tpl(cosa)<0.003f) {
			if (fabs_tpl(c2d.y) < r1 && inrange(sqr(r1)-sqr(c2d.y), sqr_signed(fabs_tpl(c2d.x)-r0),sqr_signed(fabs_tpl(c2d.x)+r0)) &&
					fabs_tpl(fabs_tpl(dc*pcaps->axis)) < pcaps->hh)
				bHasInters = 1;
		} else {
			a = r0*r0*(1+sqr(cosa)) + r1*r1 - sqr(c2d.x) - sqr(c2d.y);
			b = sqr(r0*r0*cosa) + sqr(r1*r0)*(1+sqr(cosa)) - sqr(r0)*(sqr(c2d.x)*sqr(cosa)+sqr(c2d.y)); 
			c = sqr(r1*r0*r0*cosa);
			p = b-a*a*(1.0f/3); q = a*(b*(1.0f/6)-a*a*(1.0f/27))-c*0.5f;
			if (cube(p)*(1.0f/27)+sqr(q) > 0)	{
				// now check if the closest point on the edge to the capsule axis is inside h range
				Vec2i sg2d(sgnnz(c2d.x),sgnnz(c2d.y));
				Vec2 pt2d,kpt2d,n2d,kn2d;
				kpt2d.set(r0*sg2d.x, r0*sg2d.y*fabs_tpl(cosa));
				kn2d.set(sg2d.x*fabs_tpl(cosa), sg2d.y);
				j = 1-sg2d.x*sg2d.y>>1;
				int iangle,idx[2] = {0,SINCOSTABSZ};
				do {
					iangle = idx[0]+idx[1]>>1;
					pt2d.set(g_costab[iangle]*kpt2d.x, g_sintab[iangle]*kpt2d.y);
					n2d.set(g_costab[iangle]*kn2d.x, g_sintab[iangle]*kn2d.y);
					idx[isneg(n2d^c2d-pt2d)^j] = iangle;
				} while(idx[1]-idx[0] > 1);

				if (fabs_tpl(dc*pcaps->axis-sina*sgnnz(cosa)*g_sintab[iangle]*sg2d.y*r0) < pcaps->hh) {
					bHasInters = 1;
					int icenter,idx1,imiddle,iquad,ipass;
					icenter = idx[0]*sg2d.x*sg2d.y + SINCOSTABSZ*(1-sg2d.x) & SINCOSTABSZ*4-1; 
					kpt2d.set(r0, r0*fabs_tpl(cosa));
					for(ipass=-1; ipass<=1; ipass+=2) {
						idx[0] = icenter; idx1=idx[1] = idx[0]+ipass*SINCOSTABSZ;
						do {
							imiddle = idx[0]+idx[1]>>1;
							iquad = imiddle>>SINCOSTABSZ_LOG2 & 3;
							iangle = (SINCOSTABSZ-1 & -(iquad&1)) + (imiddle & SINCOSTABSZ-1)*(1-(iquad&1)*2);
							pt2d.set(g_costab[iangle]*(1-((iquad^iquad<<1)&2))*kpt2d.x, g_sintab[iangle]*(1-(iquad&2))*kpt2d.y); 
							idx[isneg(sqr(r1)-len2(pt2d-c2d))] = imiddle;
						} while(fabs_tpl(idx[0]-idx[1])>2);
						pt2d.y = sgnnz(cosa)*g_sintab[iangle]*(1-(iquad&2))*r0;
						if (idx[1]!=idx1 && fabs_tpl(dc*pcaps->axis-sina*pt2d.y) < pcaps->hh)
							pinters->ptborder[pinters->nborderpt++] = dc+axisx*pt2d.x+(pcyl->axis^axisx)*pt2d.y+pcaps->center;
					}
				}
			}
		}
	}

	if (pinters->nborderpt) {
		pinters->pt[0]=pinters->pt[1] = pinters->ptborder[0];
		return 1;
	}

	return bHasInters;
}

int capsule_cylinder_intersection(const capsule *pcaps, const cylinder *pcyl, prim_inters *pinters)
{
	int res = cylinder_capsule_intersection(pcyl,pcaps,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int cylinder_ray_intersection(const cylinder *pcyl, const ray *pray, prim_inters *pinters)
{
	int sg,bHit,bHit0,bHitCap,bHitSide=0;
	quotient t,tcap;
	Vec3r vec0,vec1;
	real a,b,c,d;

	sg = sgnnz(pcyl->axis*pray->dir);
	t.set(pcyl->axis*((pcyl->center-pray->origin)*sg-pcyl->axis*pcyl->hh), fabs_tpl(pray->dir*pcyl->axis));
	bHit=bHit0 = isneg(fabs_tpl(t.x*2-t.y)-t.y) & 
							 isneg(((pray->origin-pcyl->center)*t.y + pray->dir*t.x ^ pcyl->axis).len2() - sqr(pcyl->r*t.y));
	t.set(pcyl->axis*((pcyl->center-pray->origin)*sg-pcyl->axis*(pcyl->hh*(bHit*2-1))), fabs_tpl(pray->dir*pcyl->axis));
	bHit = isneg(fabs_tpl(t.x*2-t.y)-t.y) & 
		     isneg(((pray->origin-pcyl->center)*t.y + pray->dir*t.x ^ pcyl->axis).len2() - sqr(pcyl->r*t.y));
	tcap = t;	bHitCap = bHit;

	if (!bHit0) {
		vec0 = pray->origin-pcyl->center ^ pcyl->axis; vec1 = pray->dir ^ pcyl->axis;
		a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-sqr(pcyl->r); d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); t.set(-b-d, a);
			bHit = isneg(fabs_tpl(t.x*2-t.y)-t.y) & 
				isneg(fabs_tpl(((pray->origin-pcyl->center)*t.y+pray->dir*t.x)*pcyl->axis)-pcyl->hh*t.y);
			t.x += d*(bHit^1)*2;
			bHitSide = bHit = isneg(fabs_tpl(t.x*2-t.y)-t.y) & 
				isneg(fabs_tpl(((pray->origin-pcyl->center)*t.y+pray->dir*t.x)*pcyl->axis)-pcyl->hh*t.y);
		}
		if (!bHitSide)
			t=tcap, bHit=bHitCap;
	}

	if (!bHit || t.x*t.y<0)
		return 0;

	pinters->pt[0]=pinters->pt[1] = pray->origin + pray->dir*t.val();
	if (bHitSide) {
		pinters->n = pinters->pt[0]-pcyl->center;
		(pinters->n -= pcyl->axis*(pcyl->axis*pinters->n)).normalize();
		pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
	} else {
		sg = sgnnz((pinters->pt[0]-pcyl->center)*pcyl->axis);
		pinters->n = pcyl->axis*sg;
		pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x41+(sg+1>>1);
	}
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x20;
	return 1;
}

int ray_cylinder_intersection(const ray *pray, const cylinder *pcyl, prim_inters *pinters)
{
	int res = cylinder_ray_intersection(pcyl,pray,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int cylinder_plane_intersection(const cylinder *pcyl, const plane *pplane, prim_inters *pinters)
{
	Vec3 dirdown = pcyl->axis*(pcyl->axis*pplane->n);
	if (dirdown.len2() > sqr(pcyl->r*1E-3f))
		dirdown.normalize() *= pcyl->r;

	pinters->pt[0]=pinters->pt[1] = pcyl->center - pcyl->axis*pcyl->hh*sgnnz(pcyl->axis*pplane->n) + dirdown;
	pinters->n = -pplane->n;
	pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x40;
	return isneg((pinters->pt[0]-pplane->origin)*pplane->n);
}

int plane_cylinder_intersection(const plane *pplane, const cylinder *pcyl, prim_inters *pinters)
{
	int res = cylinder_plane_intersection(pcyl,pplane,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int sphere_sphere_intersection(const sphere *psphere1, const sphere *psphere2, prim_inters *pinters)
{
	Vec3r dc = psphere1->center-psphere2->center;
	if (dc.len2() > sqr(psphere1->r+psphere2->r))
		return 0;

	dc.normalize();
	pinters->pt[0]=pinters->pt[1] = (dc*(psphere1->r-psphere2->r) + psphere1->center+psphere2->center)*(real)0.5;
	pinters->n = psphere2->center - psphere1->center;
	pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x40;
	return 1;
}


int sphere_ray_intersection(const sphere *psphere, const ray *pray, prim_inters *pinters)
{
	int bHit;
	real a,b,c,d;
	quotient t;

	a=pray->dir.len2(); b=pray->dir*(pray->origin-psphere->center); c=(pray->origin-psphere->center).len2()-sqr(psphere->r);
	if ((d=b*b-a*c)>=0) {
		d=sqrt_tpl(d); t.set(-b-d,a);
		bHit = isneg(fabs_tpl(t.x*2-t.y)-t.y);
		t.x += d*(bHit^1)*2;
		bHit = isneg(fabs_tpl(t.x*2-t.y)-t.y);
		if (!bHit)
			return 0;

		pinters->pt[0]=pinters->pt[1] = pray->origin + pray->dir*t.val();
		pinters->n = (pinters->pt[0]-psphere->center).normalized();
		pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
		pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x20;
		return 1;
	}
	return 0;
}

int ray_sphere_intersection(const ray *pray, const sphere *psphere, prim_inters *pinters)
{
	int res = sphere_ray_intersection(psphere,pray,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int sphere_plane_intersection(const sphere *psphere, const plane *pplane, prim_inters *pinters)
{
	pinters->pt[0]=pinters->pt[1] = psphere->center-pplane->n*psphere->r;
	pinters->n = -pplane->n;
	pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x40;
	return isneg((pinters->pt[0]-pplane->origin)*pplane->n);
}

int plane_sphere_intersection(const plane *pplane, const sphere *psphere, prim_inters *pinters)
{
	int res = sphere_plane_intersection(psphere,pplane,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int ray_plane_intersection(const ray *pray, const plane *pplane, prim_inters *pinters)
{
	quotient t(pplane->n*(pplane->origin-pray->origin), pray->dir*pplane->n);
	if (t.y-isneg(t.x*2-t.y))
		return 0;

	pinters->pt[0]=pinters->pt[1] = pray->origin + pray->dir*t.val();
	pinters->n = -pplane->n;
	pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x20;
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x40;
	return 1;
}

int plane_ray_intersection(const plane *pplane, const ray *pray, prim_inters *pinters)
{
	int res = ray_plane_intersection(pray,pplane,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int capsule_capsule_intersection(const capsule *pcaps1, const capsule *pcaps2, prim_inters *pinters)
{
	const capsule *pcaps[2] = { pcaps1,pcaps2 };
	Vec3 dc,dir;
	real dlen2,t,t0;
	int i,icaps;
	pinters->nborderpt = 0;

	// check for capsule's spheres - capsule intersections
	for(icaps=0;icaps<2;icaps++) for(i=-1;i<=1;i+=2) {
		dir = pcaps[icaps^1]->center-pcaps[icaps]->center-pcaps[icaps]->axis*(pcaps[icaps]->hh*i);
		dir -= pcaps[icaps^1]->axis*max(-pcaps[icaps^1]->hh, min(pcaps[icaps^1]->hh, dir*pcaps[icaps^1]->axis));
		if (dir.len2() < sqr(pcaps[0]->r+pcaps[1]->r)) {
			pinters->ptborder[pinters->nborderpt++] = pcaps[icaps]->center+pcaps[icaps]->axis*(pcaps[icaps]->hh*i)+dir.normalize()*pcaps[icaps]->r;
			pinters->n = dir*(1-icaps*2);
		}
	}

	// check axis-axis closest points to lie within cylinder bounds
	dc = pcaps[1]->center-pcaps[0]->center;
	dlen2 = (dir = pcaps[0]->axis^pcaps[1]->axis).len2();
	if (isneg(sqr(t=dc*dir)-sqr(pcaps[0]->r+pcaps[1]->r)*dlen2) & 
			isneg(fabs_tpl(t0=(dc^pcaps[1]->axis)*dir)-pcaps[0]->hh*dlen2) & 
			isneg(fabs_tpl((dc^pcaps[0]->axis)*dir)-pcaps[1]->hh*dlen2))
	{
		dlen2 = (real)1/dlen2;
		pinters->ptborder[pinters->nborderpt++] = pcaps[0]->center + pcaps[0]->axis*(t0*dlen2) + 
			(dir*=sqrt_tpl(dlen2)*sgnnz(t))*pcaps[0]->r;
		pinters->n = dir;			
	}

	if (pinters->nborderpt<=0)
		return 0;
	pinters->pt[0]=pinters->pt[1] = pinters->ptborder[0];
	pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x40;
	return 1;
}


int capsule_sphere_intersection(const cylinder *pcaps, const sphere *psphere, prim_inters *pinters)
{
	Vec3 dir;
	dir = pcaps->center-psphere->center;
	dir -= pcaps->axis*max(-pcaps->hh, min(pcaps->hh, dir*pcaps->axis));
	if (dir.len2() < sqr(pcaps->r+psphere->r)) {
		pinters->pt[0]=pinters->pt[1] = psphere->center+dir.normalize()*psphere->r;
		pinters->n = -dir;
		pinters->iFeature[0][0]=pinters->iFeature[1][0] = 0x40;
		pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x40;
		return 1;
	}
	return 0;
}

int sphere_capsule_intersection(const sphere *psphere, const capsule *pcaps, prim_inters *pinters)
{
	int res = capsule_sphere_intersection(pcaps,psphere,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}


int capsule_ray_intersection(const capsule *pcaps, const ray *pray, prim_inters *pinters)
{
	int icap,iFeature=-1,bHit;
	quotient t,tmin(1,1);
	Vec3r vec0,vec1;
	real a,b,c,d;

	a = pray->dir.len2();
	for(icap=0;icap<2;icap++) {
		vec0 = pcaps->center+pcaps->axis*(pcaps->hh*(icap*2-1));
		b = pray->dir*(pray->origin-vec0); c = (pray->origin-vec0).len2()-sqr(pcaps->r);
		float axcdiff=(pray->origin-pcaps->center)*pcaps->axis, axdir=pray->dir*pcaps->axis;
		if ((d=b*b-a*c)>=0) {
			d=sqrt_tpl(d); t.set(-b-d,a);
			bHit = inrange(t.x*tmin.y, (real)0,t.y*tmin.x) & isneg(pcaps->hh*t.y-fabs_tpl(axcdiff*t.y+axdir*t.x));
			tmin.x += (t.x-tmin.x)*bHit; tmin.y += (t.y-tmin.y)*bHit;
			iFeature = 0x41+icap & -bHit | iFeature & ~-bHit;
			t.x += d*2;
			bHit = inrange(t.x*tmin.y, (real)0,t.y*tmin.x) & isneg(pcaps->hh*t.y-fabs_tpl(axcdiff*t.y+axdir*t.x));
			tmin.x += (t.x-tmin.x)*bHit; tmin.y += (t.y-tmin.y)*bHit;
			iFeature = 0x41+icap & -bHit | iFeature & ~-bHit;
		}
	}

	vec0 = pray->origin-pcaps->center ^ pcaps->axis; vec1 = pray->dir ^ pcaps->axis;
	a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-sqr(pcaps->r); d = b*b-a*c;
	if (d>=0) {
		d = sqrt_tpl(d); t.set(-b-d, a);
		bHit = inrange(t.x*tmin.y, (real)0,t.y*tmin.x) & 
					 isneg(fabs_tpl(((pray->origin-pcaps->center)*t.y+pray->dir*t.x)*pcaps->axis)-pcaps->hh*t.y);
		tmin.x += (t.x-tmin.x)*bHit; tmin.y += (t.y-tmin.y)*bHit;
		iFeature = 0x40 & -bHit | iFeature & ~-bHit;
		t.x += d*2;
		bHit = inrange(t.x*tmin.y, (real)0,t.y*tmin.x) & 
					 isneg(fabs_tpl(((pray->origin-pcaps->center)*t.y+pray->dir*t.x)*pcaps->axis)-pcaps->hh*t.y);
		tmin.x += (t.x-tmin.x)*bHit; tmin.y += (t.y-tmin.y)*bHit;
		iFeature = 0x40 & -bHit | iFeature & ~-bHit;
	}

	if (iFeature<0)
		return 0;

	pinters->pt[0]=pinters->pt[1] = pray->origin + pray->dir*tmin.val();
	if (iFeature==0x40) {
		pinters->n = pinters->pt[0]-pcaps->center;
		pinters->n -= pcaps->axis*(pcaps->axis*pinters->n);
	} else
		pinters->n = pinters->pt[0]-pcaps->center-pcaps->axis*(pcaps->hh*((iFeature-0x41)*2-1));
	pinters->n.normalize();
	pinters->iFeature[0][0]=pinters->iFeature[1][0] = iFeature;
	pinters->iFeature[0][1]=pinters->iFeature[1][1] = 0x20;
	return 1;
}

int ray_capsule_intersection(const ray *pray, const capsule *pcaps, prim_inters *pinters)
{
	int res = capsule_ray_intersection(pcaps,pray,pinters), iFeature;
	iFeature=pinters->iFeature[0][0]; pinters->iFeature[0][0]=pinters->iFeature[1][1]; pinters->iFeature[1][1]=iFeature;
	iFeature=pinters->iFeature[0][1]; pinters->iFeature[0][1]=pinters->iFeature[1][0]; pinters->iFeature[1][0]=iFeature;
	return res;
}
