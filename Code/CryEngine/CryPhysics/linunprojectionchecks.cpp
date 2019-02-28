// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "unprojectionchecks.h"

CUnprojectionChecker g_Unprojector;
unprojection_check CUnprojectionChecker::table[2][NPRIMS][NPRIMS] = { 
	{
		{ (unprojection_check)box_box_lin_unprojection, (unprojection_check)box_tri_lin_unprojection, default_unprojection, 
			(unprojection_check)box_ray_lin_unprojection, (unprojection_check)box_sphere_lin_unprojection, (unprojection_check)box_cylinder_lin_unprojection, 
			(unprojection_check)box_capsule_lin_unprojection, default_unprojection },
		{ (unprojection_check)tri_box_lin_unprojection, (unprojection_check)tri_tri_lin_unprojection, default_unprojection, 
			(unprojection_check)tri_ray_lin_unprojection,	(unprojection_check)tri_sphere_lin_unprojection, (unprojection_check)tri_cylinder_lin_unprojection, 
			(unprojection_check)tri_capsule_lin_unprojection, default_unprojection },
		{ default_unprojection, default_unprojection, default_unprojection, default_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection },
		{ (unprojection_check)ray_box_lin_unprojection, (unprojection_check)ray_tri_lin_unprojection, default_unprojection, 
			default_unprojection, (unprojection_check)ray_sphere_lin_unprojection, (unprojection_check)ray_cylinder_lin_unprojection, 
			(unprojection_check)ray_capsule_lin_unprojection, default_unprojection },
		{ (unprojection_check)sphere_box_lin_unprojection, (unprojection_check)sphere_tri_lin_unprojection, default_unprojection, 
			(unprojection_check)sphere_ray_lin_unprojection, (unprojection_check)sphere_sphere_lin_unprojection, 
			(unprojection_check)sphere_cylinder_lin_unprojection, (unprojection_check)sphere_capsule_lin_unprojection, default_unprojection },
		{ (unprojection_check)cylinder_box_lin_unprojection, (unprojection_check)cylinder_tri_lin_unprojection, default_unprojection, 
			(unprojection_check)cylinder_ray_lin_unprojection, (unprojection_check)cylinder_sphere_lin_unprojection, 
			(unprojection_check)cyl_cyl_lin_unprojection, (unprojection_check)cylinder_capsule_lin_unprojection, default_unprojection },
		{ (unprojection_check)capsule_box_lin_unprojection, (unprojection_check)capsule_tri_lin_unprojection, default_unprojection, 
			(unprojection_check)capsule_ray_lin_unprojection,	(unprojection_check)capsule_sphere_lin_unprojection, 
			(unprojection_check)capsule_cylinder_lin_unprojection, (unprojection_check)capsule_capsule_lin_unprojection, default_unprojection },
		{ default_unprojection, default_unprojection, default_unprojection, default_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection }
	},{
		{ default_unprojection, default_unprojection, default_unprojection, (unprojection_check)box_ray_rot_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection },
		{ default_unprojection, (unprojection_check)tri_tri_rot_unprojection, default_unprojection, (unprojection_check)tri_ray_rot_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection },
		{ default_unprojection, default_unprojection, default_unprojection, default_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection },
		{ (unprojection_check)ray_box_rot_unprojection, (unprojection_check)ray_tri_rot_unprojection, default_unprojection, default_unprojection,
			(unprojection_check)ray_sphere_rot_unprojection, (unprojection_check)ray_cyl_rot_unprojection, 
			(unprojection_check)ray_capsule_rot_unprojection, default_unprojection },
		{ default_unprojection, default_unprojection, default_unprojection, (unprojection_check)sphere_ray_rot_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection },
		{ default_unprojection, default_unprojection, default_unprojection, (unprojection_check)cyl_ray_rot_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection },
		{ default_unprojection, default_unprojection, default_unprojection, (unprojection_check)capsule_ray_rot_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection },
		{ default_unprojection, default_unprojection, default_unprojection, default_unprojection,
			default_unprojection, default_unprojection, default_unprojection, default_unprojection }
	}
};

/*CUnprojectionChecker::CUnprojectionChecker() 
{
	for(int i=0;i<2;i++) for(int j=0;j<NPRIMS;j++) for(int k=0;k<NPRIMS;k++)
		table[i][j][k] = default_unprojection;

	table[0][triangle::type][triangle::type] = (unprojection_check)tri_tri_lin_unprojection;
	table[0][triangle::type][box::type] = (unprojection_check)tri_box_lin_unprojection;
	table[0][box::type][triangle::type] = (unprojection_check)box_tri_lin_unprojection;
	table[0][triangle::type][cylinder::type] = (unprojection_check)tri_cylinder_lin_unprojection;
	table[0][cylinder::type][triangle::type] = (unprojection_check)cylinder_tri_lin_unprojection;
	table[0][triangle::type][sphere::type] = (unprojection_check)tri_sphere_lin_unprojection;
	table[0][sphere::type][triangle::type] = (unprojection_check)sphere_tri_lin_unprojection;
	table[0][triangle::type][capsule::type] = (unprojection_check)tri_capsule_lin_unprojection;
	table[0][capsule::type][triangle::type] = (unprojection_check)capsule_tri_lin_unprojection;
	table[0][triangle::type][ray::type] = (unprojection_check)tri_ray_lin_unprojection;
	table[0][ray::type][triangle::type] = (unprojection_check)ray_tri_lin_unprojection;
	//table[0][triangle::type][plane::type] = (unprojection_check)tri_plane_lin_unprojection;
	//table[0][plane::type][triangle::type] = (unprojection_check)plane_tri_lin_unprojection;
	table[0][box::type][box::type] = (unprojection_check)box_box_lin_unprojection;
	table[0][box::type][cylinder::type] = (unprojection_check)box_cylinder_lin_unprojection;
	table[0][cylinder::type][box::type] = (unprojection_check)cylinder_box_lin_unprojection;
	table[0][box::type][sphere::type] = (unprojection_check)box_sphere_lin_unprojection;
	table[0][sphere::type][box::type] = (unprojection_check)sphere_box_lin_unprojection;
	table[0][box::type][capsule::type] = (unprojection_check)box_capsule_lin_unprojection;
	table[0][capsule::type][box::type] = (unprojection_check)capsule_box_lin_unprojection;
	table[0][cylinder::type][sphere::type] = (unprojection_check)cylinder_sphere_lin_unprojection;
	table[0][sphere::type][cylinder::type] = (unprojection_check)sphere_cylinder_lin_unprojection;
	table[0][cylinder::type][capsule::type] = (unprojection_check)cylinder_capsule_lin_unprojection;
	table[0][capsule::type][cylinder::type] = (unprojection_check)capsule_cylinder_lin_unprojection;
	table[0][sphere::type][sphere::type] = (unprojection_check)sphere_sphere_lin_unprojection;
	table[0][cylinder::type][cylinder::type] = (unprojection_check)cyl_cyl_lin_unprojection;
	table[0][capsule::type][capsule::type] = (unprojection_check)capsule_capsule_lin_unprojection;
	table[0][sphere::type][capsule::type] = (unprojection_check)sphere_capsule_lin_unprojection;
	table[0][capsule::type][sphere::type] = (unprojection_check)capsule_sphere_lin_unprojection;
	table[0][ray::type][box::type] = (unprojection_check)ray_box_lin_unprojection;
	table[0][box::type][ray::type] = (unprojection_check)box_ray_lin_unprojection;
	table[0][ray::type][cylinder::type] = (unprojection_check)ray_cylinder_lin_unprojection;
	table[0][cylinder::type][ray::type] = (unprojection_check)cylinder_ray_lin_unprojection;
	table[0][ray::type][sphere::type] = (unprojection_check)ray_sphere_lin_unprojection;
	table[0][sphere::type][ray::type] = (unprojection_check)sphere_ray_lin_unprojection;
	table[0][ray::type][capsule::type] = (unprojection_check)ray_capsule_lin_unprojection;
	table[0][capsule::type][ray::type] = (unprojection_check)capsule_ray_lin_unprojection;

	table[1][triangle::type][triangle::type] = (unprojection_check)tri_tri_rot_unprojection;
	table[1][triangle::type][ray::type] = (unprojection_check)tri_ray_rot_unprojection;
	table[1][ray::type][triangle::type] = (unprojection_check)ray_tri_rot_unprojection;
	table[1][cylinder::type][ray::type] = (unprojection_check)cyl_ray_rot_unprojection;
	table[1][ray::type][cylinder::type] = (unprojection_check)ray_cyl_rot_unprojection;
	table[1][box::type][ray::type] = (unprojection_check)box_ray_rot_unprojection;
	table[1][ray::type][box::type] = (unprojection_check)ray_box_rot_unprojection;
	table[1][capsule::type][ray::type] = (unprojection_check)capsule_ray_rot_unprojection;
	table[1][ray::type][capsule::type] = (unprojection_check)ray_capsule_rot_unprojection;
	table[1][sphere::type][ray::type] = (unprojection_check)sphere_ray_rot_unprojection;
	table[1][ray::type][sphere::type] = (unprojection_check)ray_sphere_rot_unprojection;
}*/

#define UPDATE_IDBEST(tbest,newid) \
	(idbest&=~-bBest) |= (newid)&-bBest; \
	(tbest.x*=bBest^1) += t.x*bBest;	\
	(tbest.y*=bBest^1) += t.y*bBest

#define UPDATE_IDTBEST(tbest,newid) \
	(idbest&=~-bBest) |= (newid)&-bBest; \
	(tbest.x*=bBest^1) += t.x*bBest;	\
	(tbest.y*=bBest^1) += t.y*bBest
#define idbest_type int
#define HALFX(x) ((x)>>1)

int default_unprojection(unprojection_mode*, const primitive*,int,const primitive*,int, contact*, geom_contact_area*)
{
	return 0;
}

int tri_tri_lin_unprojection(unprojection_mode *pmode, const triangle *ptri1,int iFeature1, const triangle *ptri2,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	Vec3r edge0,edge1,n,dp,dir;
	quotient t,tmax(0,1),t0,t1;
	int itri,i,j,bContact,bBest,idbest=-1,isg;
	const triangle *ptri[2] = { ptri1,ptri2 };
	real dist,mindist,sg;

	for(itri=0;itri<2;itri++) { // check for vertex-face contacts (itri provides face, itri^1 - vertex)
		isg = ((itri<<1)-1); sg = sgnnz(ptri[itri]->n*pmode->dir)*isg;
		i=0; mindist = (ptri[itri^1]->pt[0]*ptri[itri]->n)*sg; // find vertex of itri^1 that is the closest to the face of itri
		dist = (ptri[itri^1]->pt[1]*ptri[itri]->n)*sg; i += isneg(dist-mindist); mindist = min(dist,mindist);
		bBest = isneg((ptri[itri^1]->pt[2]*ptri[itri]->n)*sg-mindist); (i&=bBest^1)|=bBest<<1;

		t.set(((ptri[itri]->pt[0]-ptri[itri^1]->pt[i])*ptri[itri]->n)*isg, pmode->dir*ptri[itri]->n).fixsign();
		dir = pmode->dir*isg;	dp = ptri[itri^1]->pt[i]*t.y + dir*t.x;
		bContact = isneg(max(max(
			(dp-ptri[itri]->pt[0]*t.y ^ ptri[itri]->pt[1]-ptri[itri]->pt[0])*ptri[itri]->n,
			(dp-ptri[itri]->pt[1]*t.y ^ ptri[itri]->pt[2]-ptri[itri]->pt[1])*ptri[itri]->n),
			(dp-ptri[itri]->pt[2]*t.y ^ ptri[itri]->pt[0]-ptri[itri]->pt[2])*ptri[itri]->n));
		bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,itri<<2 | i);
	}

	for(i=0;i<3;i++) { // check for edge-edge contacts
		edge0 = ptri1->pt[inc_mod3[i]]-ptri1->pt[i];
		for(j=0;j<3;j++) { 
			edge1 = ptri2->pt[inc_mod3[j]]-ptri2->pt[j];
			dp = ptri2->pt[j]-ptri1->pt[i];
			n =  edge0 ^ edge1;
			if (n.len2() < (real)1E-8*edge0.len2()*edge1.len2())
				continue;
			t.set(dp*n, pmode->dir*n).fixsign();
			dp = dp*t.y-pmode->dir*t.x;
			t0.set((dp ^ edge1)*n, n.len2()*t.y);
			t1.set((dp ^ edge0)*n, n.len2()*t.y);
			bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y) & isneg(fabs_tpl(t1.x*2-t1.y)-t1.y);	// contact point belongs to both edges
			bBest = bContact & isneg(tmax-t);
			UPDATE_IDBEST(tmax,i<<2 | j | 0x80);
		}
	}

	if (idbest==-1 || fabs_tpl(tmax.y)<(real)1E-20)
		return 0;

	if (idbest & 0x80) {
		i = idbest>>2 & 3; j = idbest & 3;
		edge0 = ptri1->pt[inc_mod3[i]]-ptri1->pt[i];
		edge1 = ptri2->pt[inc_mod3[j]]-ptri2->pt[j];
		dp = ptri2->pt[j]-ptri1->pt[i];
		n =  edge0 ^ edge1;
		t0.set((dp*tmax.y-pmode->dir*tmax.x ^ edge1)*n, n.len2()*tmax.y);
		t0.y = 1.0/t0.y;
		pcontact->t = tmax.x*t0.y*n.len2();
		pcontact->pt = ptri1->pt[i] + edge0*(t0.x*t0.y) + pmode->dir*pcontact->t;
		pcontact->n = n*sgnnz((ptri2->n^edge1)*n); //sgnnz((ptri2->pt[dec_mod3[j]]-ptri1->pt[dec_mod3[i]])*n);
		pcontact->iFeature[0] = 0xA0 | i;
		pcontact->iFeature[1] = 0xA0 | j;
	} else {
		itri = idbest>>2; i = idbest & 3;
		pcontact->t = tmax.val();
		pcontact->pt = ptri[itri^1]->pt[i] + pmode->dir*(pcontact->t*itri);
		pcontact->n = ptri[itri]->n*(1-(itri<<1));
		pcontact->iFeature[itri] = 0x40;
		pcontact->iFeature[itri^1] = 0x80 | i;
	}

	return 1;
}

int tri_box_lin_unprojection(unprojection_mode *pmode, const triangle *ptri,int iFeature1, const box *pbox,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	Vec3r pt[3],n,dir,edge0,ptbox,dp,ncontact,dp_x_edge1;
	Vec3i sgdir,sgnorm;
	int i,j,idir,idbest=-1,bContact,bBest,ix,iy,isgx,isgy,isg;
	quotient t,tmax(0,1),t0,t1;
	real dist,mindist;

	pt[0] = pbox->Basis*(ptri->pt[0]-pbox->center);
	pt[1] = pbox->Basis*(ptri->pt[1]-pbox->center);
	pt[2] = pbox->Basis*(ptri->pt[2]-pbox->center);
	n = pbox->Basis*ptri->n;
	dir = pbox->Basis*pmode->dir;
	sgdir.x=sgnnz(dir.x); sgdir.y=sgnnz(dir.y); sgdir.z=sgnnz(dir.z);
	sgnorm.x=sgnnz(n.x); sgnorm.y=sgnnz(n.y); sgnorm.z=sgnnz(n.z);

	// box vertex - triangle face, check only the box vertex that is the farthest from the triangle (along flipped normal)
	// Proof: suppose that not the farthest point is the global maximum. but then some edges from this point to the farthest
	// point will penetrate the triangle (since the box is convex), meaning this point is not the global maximum
	ptbox.Set(-pbox->size.x*sgnorm.x, -pbox->size.y*sgnorm.y, -pbox->size.z*sgnorm.z);
	t.set((pt[0]-ptbox)*n, -(dir*n)).fixsign();
	ptbox = ptbox*t.y - dir*t.x;
	bContact = 1^(isneg((pt[1]-pt[0]^ptbox-pt[0]*t.y)*n) | isneg((pt[2]-pt[1]^ptbox-pt[1]*t.y)*n) | isneg((pt[0]-pt[2]^ptbox-pt[2]*t.y)*n));
	bBest = bContact & isneg(tmax-t);
	UPDATE_IDBEST(tmax,0);

	// triangle vertex - box face
	for(j=0;j<3;j++) { // i-triangle vertex, j-box face (with sign in direction of movement)
		i=0; mindist = pt[0][j]*sgdir[j]; // find triangle vertex that is the farthest from corresponding box face (along its inside normal)
		dist = pt[1][j]*sgdir[j]; i += isneg(dist-mindist); mindist = min(dist,mindist);
		dist = pt[2][j]*sgdir[j]; bBest = isneg(dist-mindist); (i&=bBest^1)|=bBest<<1;

		ix = inc_mod3[j]; iy = dec_mod3[j];
		t.set(pbox->size[j]-pt[i][j]*sgdir[j], dir[j]*sgdir[j]); // check if contact point belongs to box face
		bContact = isneg(fabs_tpl(pt[i][ix]*t.y+dir[ix]*t.x)-pbox->size[ix]*t.y) & isneg(fabs_tpl(pt[i][iy]*t.y+dir[iy]*t.x)-pbox->size[iy]*t.y);
		bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,0x40 | (i<<2 | j));
	}

	// triangle edge - box edge
	for(i=0;i<3;i++) { 
		edge0 = pt[inc_mod3[i]]-pt[i];
		for(idir=0;idir<3;idir++) {
			ix = inc_mod3[idir]; iy = dec_mod3[idir];
			ncontact[idir]=0; ncontact[ix]=edge0[iy]; ncontact[iy]=-edge0[ix];
			if (ncontact.len2() < (real)1E-8*edge0.len2())
				continue;
			t.y = dir*ncontact; t.y *= isg = sgnnz(t.y);
			// choose box edge along idir that will generate maximum t (only it can be the global maximum for the entire unprojection)
			// Proof: there's a continuous range along dir where the edge intersects infinite box slab in direction idir.
			// if touch point is the global separation point, then the edge will intersect the slab, but not the box while continuing moving along dir.
			// but it cannot leave the box immediately after the touch point, because it doesn't touch box feature different from slab feature
			// (if it touches a vertex, we'll hopefully detect it as a face-vertex contact)
			// NOTE: if maximum t is generated by 2 box edges, and the triangle edge crosses both of them, then which one we choose doesn't matter,
			// and if the triangle edge crosses only one, then tri vertex-box face contact will be detected (so choosing max t edge only is safe)
			isgx = sgnnz(ncontact[ix])*isg; isgy = sgnnz(ncontact[iy])*isg;
			ptbox[idir]=-pbox->size[idir]; ptbox[ix]=pbox->size[ix]*isgx; ptbox[iy]=pbox->size[iy]*isgy;
			dp = ptbox-pt[i]; t.x = dp*ncontact*isg; dp = dp*t.y-dir*t.x;
			dp_x_edge1[idir]=0; dp_x_edge1[ix]=dp[iy]; dp_x_edge1[iy]=-dp[ix];
			t0.set(dp_x_edge1*ncontact, ncontact.len2()*t.y); // check if t0 is in (0..1) and t1 is in (0..2*size)
			t1.set((dp^edge0)*ncontact, t0.y);
			bContact = t0.isin01() & isneg(fabs_tpl(t1.x-t1.y*pbox->size[idir])-t1.y*pbox->size[idir]);
			bBest = bContact & isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x80 | (i<<4 | idir<<2 | isgy+1 | isgx+1>>1));
		}
	}

	if (idbest==-1)
		return 0;

	switch (idbest & 0xC0) {	
		case 0:	// triangle face - box vertex
			ptbox.Set(-pbox->size.x*sgnorm.x, -pbox->size.y*sgnorm.y, -pbox->size.z*sgnorm.z);
			pcontact->t = tmax.val();
			pcontact->pt = pbox->center+ptbox*pbox->Basis;
			pcontact->n = ptri->n;
			pcontact->iFeature[0] = 0x40;
			pcontact->iFeature[1] = 1-sgnorm.z<<1 | 1-sgnorm.y | 1-sgnorm.x>>1;
			break;

		case 0x40: // triangle vertex - box face
			i = idbest>>2&3; j = idbest&3;
			pcontact->t = tmax.val();
			pcontact->pt = ptri->pt[i] + pmode->dir*pcontact->t;
			pcontact->n = pbox->Basis.GetRow(j)*-sgdir[j];
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40 | j<<1 | sgdir[j]+1>>1;
			break;

		default: // triangle edge - box edge
			i = idbest>>4&3; idir=idbest>>2&3; isgy=(idbest&2)-1; isgx=((idbest&1)<<1)-1;
			edge0 = pt[inc_mod3[i]]-pt[i];
			ix = inc_mod3[idir]; iy = dec_mod3[idir];
			ncontact[idir]=0; ncontact[ix]=edge0[iy]; ncontact[iy]=-edge0[ix];
			isg = sgnnz(dir*ncontact);
			isgx = sgnnz(ncontact[ix])*isg; isgy = sgnnz(ncontact[iy])*isg;
			ptbox[idir]=-pbox->size[idir]; ptbox[ix]=pbox->size[ix]*isgx; ptbox[iy]=pbox->size[iy]*isgy;
			dp = (ptbox-pt[i])*tmax.y-dir*tmax.x;
			dp_x_edge1[idir]=0; dp_x_edge1[ix]=dp[iy]; dp_x_edge1[iy]=-dp[ix];
			t0.set(dp_x_edge1*ncontact, ncontact.len2()*tmax.y); t0.y = 1.0/t0.y;
			pcontact->t = tmax.x*t0.y*ncontact.len2();
			pcontact->pt = ptri->pt[i] + (ptri->pt[inc_mod3[i]]-ptri->pt[i])*t0.x*t0.y + pmode->dir*pcontact->t;
			pcontact->n = ncontact*pbox->Basis;
			pcontact->n *= sgnnz(pcontact->n*(pbox->center-pcontact->pt));
			pcontact->iFeature[0] = 0xA0 | i;
			pcontact->iFeature[1] = 0x20 | idbest&0xF;
	}

	return 1;
}

int box_tri_lin_unprojection(unprojection_mode *pmode, const box *pbox,int iFeature1, const triangle *ptri,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  tri_box_lin_unprojection(pmode,ptri,iFeature2,pbox,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}

int tri_cylinder_lin_unprojection(unprojection_mode *pmode, const triangle *ptri,int iFeature1, const cylinder *pcyl,int iFeature2, 
																	contact *pcontact, geom_contact_area *parea)
{
	Vec3r n,center,dp,edge,vec0,vec1,ptcyl;
	quotient t,t0,t1,tmax(0,1);
	real nlen,nlen2,a,b,c,d,r2=sqr(pcyl->r),dist,mindist;
	int i,j,idbest=-1,bContact,bBest,bCapped=iszero(iFeature2-0x43)^1;

	// triangle vertices - cylinder side
	for(i=0;i<3;i++) {
		dp = ptri->pt[i]-pcyl->center;
		vec0 = dp ^ pcyl->axis; vec1 = pmode->dir^pcyl->axis;
		a = vec1*vec1; b = vec1*vec0; c = vec0*vec0-r2; d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); t.set(-b+d,a);
			bContact = isneg(fabs_tpl((dp*t.y+pmode->dir*t.x)*pcyl->axis) - t.y*pcyl->hh);
			bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
			UPDATE_IDBEST(tmax,0x40 | i);
		}
	}

	// triangle edges - cylinder side
	for(i=0;i<3;i++) {
		edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
		dp = ptri->pt[i]-pcyl->center; n = edge^pcyl->axis; nlen2 = n.len2();
		c = dp*n; a = pmode->dir*n; b = a*c; a*=a; c=c*c-r2*nlen2; d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); t.set(-b+d,a);
			dp = (pcyl->center-ptri->pt[i])*t.y-pmode->dir*t.x;
			t0.set((dp^pcyl->axis)*n, nlen2*t.y);
			t1.set((dp^edge)*n, nlen2*t.y);
			bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y) & isneg(fabs_tpl(t1.x)-t1.y*pcyl->hh);
			bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
			UPDATE_IDBEST(tmax,0x60 | i);
		}
	}

	if (bCapped) {
		// cylinder cap edges - triangle face
		n = pcyl->axis*(pcyl->axis*ptri->n)-ptri->n;
		nlen = n.len();
		i = isneg(pcyl->axis*ptri->n); // choose cap closest to the triangle face
		center = pcyl->center + pcyl->axis*(pcyl->hh*((i<<1)-1));
		t.set(((center-ptri->pt[0])*nlen+n*pcyl->r)*ptri->n, ptri->n*pmode->dir).fixsign();
		ptcyl = center*(t.y*nlen) + n*(pcyl->r*t.y) - pmode->dir*t.x; t.y *= nlen;
		bContact = 1^(isneg((ptri->pt[1]-ptri->pt[0]^ptcyl-ptri->pt[0]*t.y)*ptri->n) | 
									isneg((ptri->pt[2]-ptri->pt[1]^ptcyl-ptri->pt[1]*t.y)*ptri->n) | 
									isneg((ptri->pt[0]-ptri->pt[2]^ptcyl-ptri->pt[2]*t.y)*ptri->n));
		bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
		UPDATE_IDBEST(tmax,i);

		// triangle vertices - cylinder cap faces
		j = (isnonneg(pcyl->axis*pmode->dir)<<1)-1; // choose cap that lies farther along unprojection direction
		i=0; mindist = (ptri->pt[0]*pcyl->axis)*j; // find triangle vertex that is the closest to the cap face
		dist = (ptri->pt[1]*pcyl->axis)*j; i += isneg(dist-mindist); mindist = min(mindist,dist);
		dist = (ptri->pt[2]*pcyl->axis)*j; bBest = isneg(dist-mindist); (i&=bBest^1)|=bBest<<1;

		center = pcyl->center + pcyl->axis*(pcyl->hh*j);
		t.set((center-ptri->pt[i])*pcyl->axis, pmode->dir*pcyl->axis).fixsign();
		bContact = isneg((ptri->pt[i]*t.y+pmode->dir*t.x-center*t.y).len2()-r2*t.y*t.y);
		bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
		UPDATE_IDBEST(tmax,0x20 | (i<<1 | j+1>>1));

		// triangle edges - cylinder cap edges
		for(i=0;i<3;i++) {
			edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
			for(j=-1;j<=1;j+=2) {
				center = pcyl->center+pcyl->axis*pcyl->hh*j;
				dp = ptri->pt[i]-center;
				vec0 = pcyl->axis^(dp^edge); vec1 = pcyl->axis^(pmode->dir^edge);
				a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2*sqr(edge*pcyl->axis); d = b*b-a*c;
				if (d>=0) {
					d = sqrt_tpl(d); t.set(-b+d,a);
					t0.set(-((dp*t.y+pmode->dir*t.x)*pcyl->axis), (edge*pcyl->axis)*t.y);
					bContact = isneg(fabs_tpl(t0.x*2-t0.y)-fabs_tpl(t0.y));
					bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
					UPDATE_IDBEST(tmax,0x80 | i<<1 | j+1>>1);
				}
			}
		}

		// we also need to process degenerate cases when contacting triangle edge lies in cap plane (edge-cap edge check doesn't detect them)
		if (fabs_tpl(fabs_tpl(ptri->n*pcyl->axis)-1.0f)<1E-4f) { // potential triangle face - cylinder cap contact
			center = pcyl->center + pcyl->axis*(pcyl->hh*sgnnz(pmode->dir*pcyl->axis));
			d = pmode->dir*ptri->n; j = sgnnz(d);
			t.set(((center-ptri->pt[0])*ptri->n)*j, fabs_tpl(d));
			if (t>tmax && t<pmode->tmax) {
				pcontact->n = ptri->n;
				pcontact->iFeature[1] = 0x40 | (j+1>>1)+1;

				if (((ptri->pt[0]-center)*t.y+pmode->dir*t.x).len2() < r2*t.y*t.y) {	// triangle's 1st vertex is inside cylinder cap
					pcontact->t = t.val();
					pcontact->pt = ptri->pt[0] + pmode->dir*pcontact->t;
					pcontact->iFeature[0] = 0x80;
					return 1;
				}
				ptcyl = center*t.y - pmode->dir*t.x;
				bContact = 1^(isneg((ptri->pt[1]-ptri->pt[0]^ptcyl-ptri->pt[0]*t.y)*ptri->n) | 
											isneg((ptri->pt[2]-ptri->pt[1]^ptcyl-ptri->pt[1]*t.y)*ptri->n) | 
											isneg((ptri->pt[0]-ptri->pt[2]^ptcyl-ptri->pt[2]*t.y)*ptri->n));
				if (bContact) { // cylinder cap center is inside triangle
					pcontact->t = t.val();
					pcontact->pt = center;
					pcontact->iFeature[0] = 0x40;
					return 1;
				}
				for(i=0;i<3;i++) {
					edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
					vec0 = (ptri->pt[i]-center)*t.y+pmode->dir*t.x; vec1 = edge*t.y;
					a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2*sqr(t.y); d = b*b-a*c;
					if (d>=0) {
						d = sqrt_tpl(d); t0.set(-b-d,a);
						for(j=0;j<2;j++,t0.x+=d*2) if (isneg(fabs_tpl(t0.x*2-t0.y)-t0.y)) {
							pcontact->t = t.val();
							pcontact->pt = ptri->pt[i] + pmode->dir*pcontact->t + edge*t0.val();
							pcontact->iFeature[0] = 0xA0 | i;
							return 1;
						}
					}
				}
			}
		} else for(i=0;i<3;i++) {
			edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
			if (sqr(edge*pcyl->axis)<edge.len2()*(real)1E-4) { // potential traingle edge - cylinder cap contact
				j = sgnnz(pcyl->axis*pmode->dir);
				center = pcyl->center+pcyl->axis*pcyl->hh*j; dp = center-ptri->pt[i];	
				t.set((dp*pcyl->axis)*edge.len2()-(edge*pcyl->axis)*(edge*dp), // calculate when the closest point on edge will touch the cap
					(pmode->dir*pcyl->axis)*edge.len2()-(edge*pcyl->axis)*(edge*pmode->dir)).fixsign();
				if (t>tmax && t<pmode->tmax) {
					pcontact->t = t.val();
					t0.set(edge*(dp-pmode->dir*pcontact->t), edge.len2());
					pcontact->pt = ptri->pt[i] + pmode->dir*pcontact->t;
					if (fabs_tpl(t0.x*2-t0.y)<t0.y && (edge*t0.x+(pcontact->pt-center)*t0.y).len2()<r2*sqr(t0.y))
						pcontact->pt += edge*(t0.val());
					else if ((pcontact->pt+edge-center).len2()<r2)
						pcontact->pt += edge;
					else if ((pcontact->pt-center).len2()>r2)
						continue;
					pcontact->n = pcyl->axis*-j;
					pcontact->iFeature[0] = 0xA0 | i;
					pcontact->iFeature[1] = 0x40 | (j+1>>1)+1;
					return 1;
				}
			}
		}
	}

	if (idbest==-1 || fabs_tpl(tmax.y)<(real)1E-20)
		return 0;

	switch (idbest & 0xE0) { 
		case 0:	// triangle face - cylinder cap edge
			center = pcyl->center+pcyl->axis*(pcyl->hh*((idbest<<1)-1));
			n = pcyl->axis*(pcyl->axis*ptri->n)-ptri->n;
			t0.y = 1.0/(tmax.y*nlen);
			pcontact->t = tmax.x*nlen*t0.y;
			pcontact->pt = center + n*(pcyl->r*t0.y*tmax.y);
			pcontact->n = ptri->n;
			pcontact->iFeature[0] = 0x40;
			pcontact->iFeature[1] = 0x20 | idbest;
			break;

		case 0x20: // triangle vertex - cylinder cap face
			i = idbest>>1&3; j = idbest&1;
			pcontact->t = tmax.val();
			pcontact->pt = ptri->pt[i] + pmode->dir*pcontact->t;
			pcontact->n = pcyl->axis*(1-(j<<1));
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40 | j+1;
			break;

		case 0x40: // triangle vertex - cylinder side
			i = idbest & 3;
			pcontact->t = tmax.val();
			pcontact->pt = ptri->pt[i] + pmode->dir*pcontact->t;
			pcontact->n = pcyl->center-pcontact->pt;
			pcontact->n -= pcyl->axis*(pcyl->axis*pcontact->n);
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40;
			break;

		case 0x60: // triangle edge - cylinder side
			i = idbest&3;
			edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
			dp = pcyl->center-ptri->pt[i]; n = edge^pcyl->axis; nlen2 = n.len2();
			dp = dp*tmax.y-pmode->dir*tmax.x;
			t0.set((dp^pcyl->axis)*n, nlen2*tmax.y); t0.y = 1.0/t0.y;
			pcontact->t = tmax.x*t0.y*nlen2;
			pcontact->pt = ptri->pt[i] + edge*t0.x*t0.y + pmode->dir*pcontact->t;
			pcontact->n = pcyl->center-pcontact->pt;
			pcontact->n -= pcyl->axis*(pcyl->axis*pcontact->n);
			pcontact->iFeature[0] = 0xA0 | i;
			pcontact->iFeature[1] = 0x40;
			break;

		default: // triangle edge - cylinder cap edge
			i = idbest>>1&3; j = idbest&1;
			center = pcyl->center+pcyl->axis*pcyl->hh*((j<<1)-1);
			edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
			dp = ptri->pt[i]-center;
			t0.set(-((dp*tmax.y+pmode->dir*tmax.x)*pcyl->axis), (edge*pcyl->axis)*tmax.y); t0.y = 1.0/t0.y;
			pcontact->t = tmax.x*t0.y*(edge*pcyl->axis);
			pcontact->pt = ptri->pt[i] + edge*t0.x*t0.y + pmode->dir*pcontact->t;
			pcontact->n = pcontact->pt-pcyl->center;
			pcontact->n -= pcyl->axis*(pcyl->axis*pcontact->n);
			pcontact->n = edge^(pcyl->axis^pcontact->n);
			pcontact->n *= sgnnz((pcyl->center-pcontact->pt)*pcontact->n);
			pcontact->iFeature[0] = 0xA0 | i;
			pcontact->iFeature[1] = 0x20 | j;
	}

	return 1;
}

int cylinder_tri_lin_unprojection(unprojection_mode *pmode, const cylinder *pcyl,int iFeature1, const triangle *ptri,int iFeature2, 
																	contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  tri_cylinder_lin_unprojection(pmode,ptri,iFeature2,pcyl,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t; pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}

int tri_sphere_lin_unprojection(unprojection_mode *pmode, const triangle *ptri,int iFeature1, const sphere *psphere,int iFeature2, 
																contact *pcontact, geom_contact_area *parea)
{
	quotient t,tmax(pmode->tmin,1),t0;
	Vec3r center,edge,vec0,vec1,dp;
	real a,b,c,d,r2=sqr(psphere->r);
	int i,bBest,bContact,idbest=-1;

	// triangle face - sphere
	t.set((psphere->center-ptri->pt[0])*ptri->n-psphere->r, pmode->dir*ptri->n).fixsign();
	center = psphere->center*t.y - pmode->dir*t.x;
	bContact = 1^(isneg((ptri->pt[1]-ptri->pt[0]^center-ptri->pt[0]*t.y)*ptri->n) | 
								isneg((ptri->pt[2]-ptri->pt[1]^center-ptri->pt[1]*t.y)*ptri->n) | 
								isneg((ptri->pt[0]-ptri->pt[2]^center-ptri->pt[2]*t.y)*ptri->n));
	bBest = bContact & isneg(tmax-t);
	UPDATE_IDBEST(tmax,0);

	// triangle vertices - sphere
	for(i=0;i<3;i++) {
		vec0 = ptri->pt[i]-psphere->center; vec1 = pmode->dir;
		a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2; d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); t.set(-b-d,a);	bBest = isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x40 | i);
			t.set(-b+d,a); bBest = isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x40 | i);
		}
	}

	// triangle edges - sphere
	for(i=0;i<3;i++) {
		edge = ptri->pt[inc_mod3[i]]-ptri->pt[i]; dp = ptri->pt[i]-psphere->center;
		vec0 = edge ^ dp; vec1 = edge ^ pmode->dir;
		a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2*edge.len2(); d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); t.set(-b-d,a); 
			t0.set(((psphere->center-ptri->pt[i])*t.y-pmode->dir*t.x)*edge, t.y*edge.len2());
			bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y); bBest = bContact & isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x80 | i);
			t.x += d*2;	t0.set(((psphere->center-ptri->pt[i])*t.y-pmode->dir*t.x)*edge, t.y*edge.len2());
			bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y); bBest = bContact & isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x80 | i);
		}
	}

	if (idbest==-1)
		return 0;

	switch (idbest & 0xC0) {
		case 0: // triangle face - sphere
			pcontact->t = tmax.val();
			pcontact->pt = psphere->center - ptri->n*psphere->r;
			pcontact->n = ptri->n;
			pcontact->iFeature[0] = 0x40;
			pcontact->iFeature[1] = 0x40;
			break;
		
		case 0x40:
			i = idbest&3;
			pcontact->t = tmax.val();
			pcontact->pt = ptri->pt[i]+pmode->dir*pcontact->t;
			pcontact->n = psphere->center-pcontact->pt;
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40;
			break;

		case 0x80:
			i = idbest&3;
			edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
			t0.set(((psphere->center-ptri->pt[i])*tmax.y-pmode->dir*tmax.x)*edge, tmax.y*edge.len2()); t0.y = 1.0/t0.y;
			pcontact->t = tmax.x*t0.y*edge.len2();
			pcontact->pt = ptri->pt[i] + edge*(t0.x*t0.y) + pmode->dir*pcontact->t;
			pcontact->n = psphere->center-pcontact->pt;
			pcontact->iFeature[0] = 0xA0 | i;
			pcontact->iFeature[1] = 0x40;
	}

	return 1;
}

int sphere_tri_lin_unprojection(unprojection_mode *pmode, const sphere *psphere,int iFeature1, const triangle *ptri,int iFeature2, 
																contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  tri_sphere_lin_unprojection(pmode,ptri,iFeature2,psphere,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t; pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}

int tri_capsule_lin_unprojection(unprojection_mode *pmode, const triangle *ptri,int iFeature1, const capsule *pcaps,int iFeature2, 
																 contact *pcontact, geom_contact_area *parea)
{
	float tmin0 = pmode->tmin;
	int bContact0,bContact1,bContact2;
	sphere sph;
	pcontact->t = 0;

	bContact0 = tri_cylinder_lin_unprojection(pmode, ptri,iFeature1, pcaps,0x43, pcontact,parea);
	pmode->tmin += (pcontact->t-pmode->tmin)*bContact0;

	sph.center = pcaps->center-pcaps->axis*pcaps->hh; sph.r = pcaps->r;
	bContact1 = tri_sphere_lin_unprojection(pmode, ptri,iFeature1, &sph,-1, pcontact,parea);
	pmode->tmin += (pcontact->t-pmode->tmin)*bContact1; pcontact->iFeature[1] += bContact1;

	sph.center = pcaps->center+pcaps->axis*pcaps->hh;
	bContact2 = tri_sphere_lin_unprojection(pmode, ptri,iFeature1, &sph,-1, pcontact,parea);
	pcontact->iFeature[1] += bContact2*2;
	pmode->tmin = tmin0;

	return bContact0|bContact1|bContact2;
}

int capsule_tri_lin_unprojection(unprojection_mode *pmode, const capsule *pcaps,int iFeature1, const triangle *ptri,int iFeature2, 
																 contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  tri_capsule_lin_unprojection(pmode,ptri,iFeature2,pcaps,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t; pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}


int tri_ray_lin_unprojection(unprojection_mode *pmode, const triangle *ptri,int iFeature1, const ray *pray,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	Vec3r pt,ptnew,n,dp,edge;
	quotient t,tmax(0,1),t0,t1;
	int i,bContact,bBest,idbest=-1;
	real nlen2;

	// triangle - ray ends 
	for(i=0,pt=pray->origin; i<2; i++,pt+=pray->dir) {
		t.set((pt-ptri->pt[0])*ptri->n, pmode->dir*ptri->n);
		ptnew = pt*t.y - pmode->dir*t.x;
		bContact = 1^(isneg((ptri->pt[1]-ptri->pt[0]^ptnew-ptri->pt[0]*t.y)*ptri->n) | 
									isneg((ptri->pt[2]-ptri->pt[1]^ptnew-ptri->pt[1]*t.y)*ptri->n) | 
									isneg((ptri->pt[0]-ptri->pt[2]^ptnew-ptri->pt[2]*t.y)*ptri->n));
		bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,i);
	}

	// triangle edges - ray
	for(i=0;i<3;i++) {
		edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
		n = edge^pray->dir; nlen2 = n.len2(); dp = pray->origin-ptri->pt[i];
		t.set(n*dp, n*pmode->dir).fixsign(); dp = dp*t.y - pmode->dir*t.x;
		t0.set((dp^pray->dir)*n, nlen2*t.y);
		t1.set((dp^edge)*n, nlen2*t.y);
		bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y) & isneg(fabs_tpl(t1.x*2-t1.y)-t1.y);
		bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,0x80 | i);
	}

	if (idbest==-1)
		return 0;

	if (idbest&0x80) { // triangle edge - ray
		i = idbest&3;
		edge = ptri->pt[inc_mod3[i]]-ptri->pt[i];
		n = edge^pray->dir; nlen2 = n.len2();
		dp = (pray->origin-ptri->pt[i])*tmax.y - pmode->dir*tmax.x;
		t0.set((dp^pray->dir)*n, nlen2*tmax.y); t0.y = 1.0/t0.y;
		pcontact->t = tmax.x*t0.y*nlen2;
		pcontact->pt = ptri->pt[i] + edge*t0.x*t0.y + pmode->dir*pcontact->t;
		pcontact->n = pray->dir^edge;
		pcontact->n *= sgnnz((ptri->pt[i]-ptri->pt[dec_mod3[i]])*pcontact->n);
		pcontact->iFeature[0] = 0xA0 | i;
		pcontact->iFeature[1] = 0xA0;
	} else { // triangle face - ray end
		pcontact->t = tmax.val();
		pcontact->pt = pray->origin+pray->dir*idbest;
		pcontact->n = ptri->n;
		pcontact->iFeature[0] = 0x40;
		pcontact->iFeature[1] = 0x80|idbest;
	}

	return 1;
}

int ray_tri_lin_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const triangle *ptri,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  tri_ray_lin_unprojection(pmode,ptri,iFeature2,pray,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}

int tri_plane_lin_unprojection(unprojection_mode *pmode, const triangle *ptri,int iFeature1, const plane *pplane,int iFeature2, 
															 contact *pcontact, geom_contact_area *parea)
{
	quotient t;
	real dist,mindist;
	int i,bBest;

	// the closest to the plane triangle vertex - plane
	i=0; mindist = (ptri->pt[0]-pplane->origin)*pplane->n;
	dist = (ptri->pt[1]-pplane->origin)*pplane->n; i += isneg(dist-mindist); mindist = min(dist,mindist);
	dist = (ptri->pt[2]-pplane->origin)*pplane->n; bBest = isneg(dist-mindist); (i&=bBest^1)|=bBest<<1;

	t.set((pplane->origin-ptri->pt[0])*pplane->n, pmode->dir*pplane->n).fixsign();
	if (t.x<0)
		return 0;

	pcontact->t = t.val();
	pcontact->pt = ptri->pt[i] + pmode->dir*pcontact->t;
	pcontact->n = -pplane->n;
	pcontact->iFeature[0] = 0x80 | i;
	pcontact->iFeature[1] = 0x40;

	return 1;
}

int plane_tri_lin_unprojection(unprojection_mode *pmode, const plane *pplane,int iFeature1, const triangle *ptri,int iFeature2, 
															 contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  tri_plane_lin_unprojection(pmode,ptri,iFeature2,pplane,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}


int crop_polygon_with_bound(const Vec2 *ptsrc,const int *idedgesrc,int nsrc, const Vec2 &sz,int icode,int iedge, 
														Vec2 *ptdst,int *idedgedst)
{
	int ic=icode&1,i,inext,sg=icode>>1&1,istart=-1,bInside,n,ndst;
	float bound = sz[ic]*((icode&2)-1);

	for(i=0;i<nsrc;i++) {
		bInside = -(isneg(bound-ptsrc[i][ic])^sg);
		istart = istart&~bInside | i&bInside;
	}
	if (istart==-1)
		return 0;

	for(i=istart,n=ndst=0,bInside=1; n<nsrc; n++,i=inext) {
		ptdst[ndst] = ptsrc[i];
		idedgedst[ndst] = idedgesrc[i];
		ndst += bInside;
		inext = i+1 & i+1-nsrc>>31;
		if ((ptsrc[i][ic]-bound)*(ptsrc[inext][ic]-bound)<0) {
			Vec2 d = ptsrc[inext]-ptsrc[i]; bInside ^= 1;
			ptdst[ndst][ic] = bound; ptdst[ndst][ic^1] = ptsrc[i][ic^1]+d[ic^1]*(bound-ptsrc[i][ic])/d[ic];
			idedgedst[ndst] = idedgedst[ndst]&-bInside | iedge&~-bInside; // change edge to bound edge only for inside-outside switches
			++ndst;
		}
	}
	return ndst;
}

void crop_segment_with_bound(Vec2 *ptseg,float bound,int ic, int *piFeature0,int *piFeature1, int iedge0,int iedge1)
{
	int imin=isneg(ptseg[1][ic]-ptseg[0][ic]),imax=imin^1;
	if (ptseg[imax][ic]<-bound || ptseg[imin][ic]>bound) {
		ptseg[0].zero(); ptseg[1].zero();
	}	else {
		Vec2 d = ptseg[imax]-ptseg[imin];
		float denom=0;
		if (ptseg[imin][ic]<-bound || ptseg[imax][ic]>bound) 
			denom = 1.0f/d[ic];
		if (ptseg[imin][ic] < -bound)	{
			ptseg[imin][ic^1] = ptseg[imin][ic^1] + d[ic^1]*(-bound-ptseg[imin][ic])*denom;
			ptseg[imin][ic] = -bound;
			piFeature0[imin] = iedge0; piFeature1[imin] = iedge1;
		}
		if (ptseg[imax][ic] > bound) {
			ptseg[imax][ic^1] = ptseg[imin][ic^1] + d[ic^1]*(bound-ptseg[imin][ic])*denom;
			ptseg[imax][ic] = bound;
			piFeature0[imax] = iedge0 | 2>>ic; piFeature1[imax] = iedge1;
		}
	}
}

inline int edge_from_vtx(int ivtx0,int ivtx1) {
	int idir = ivtx0^ivtx1; idir = isneg(3-idir)+isneg(1-idir);
	return idir<<2 | ivtx0>>inc_mod3[idir]&1 | (ivtx0>>dec_mod3[idir])<<1&2;
}
inline int vtx_from_edges(int iedge0,int iedge1) {
	int idir0=iedge0>>2, idir1=iedge1>>2;
	return (iedge0&1)<<inc_mod3[idir0] | (iedge0>>1&1)<<dec_mod3[idir0] | (iedge1>>iszero(idir0-dec_mod3[idir1])&1)<<idir0;
}

int box_box_lin_unprojection(unprojection_mode *pmode, const box *pbox1,int iFeature1, const box *pbox2,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	const box *pbox[2] = { pbox1,pbox2 };
	Vec3r axes[3],center[2],origin,pt0,pt1,pt,dp,n,dir[2],size[2]={ pbox1->size,pbox2->size };
	real nlen2,dirlen2=1;
	int i,iz,ibox,idir0,idir1,ix,iy,ix0,iy0,ix1,iy1,bFindMinUnproj;
	idbest_type idbest=-1;
	Vec3_tpl<int> isg0,isg1;
	int icode,sg,bBest;
	quotient t,tmin(pmode->tmax,1),t0,t1;

	center[0] = pbox1->Basis*(pbox2->center-pbox1->center);
	center[1] = pbox2->Basis*(pbox1->center-pbox2->center);
	dir[0] = pbox1->Basis*pmode->dir;
	dir[1] = pbox2->Basis*-pmode->dir;
	SetBasisFromMtx(axes, pbox2->Basis*pbox1->Basis.T());
	bFindMinUnproj = iszero(pmode->dir.len2());

	// each contact we check guarantees that boxes are fully separated at it (while also guaranteeing that no closer contact of the
	// selected group of features can be the global separation minimum), so the contact with the minimum unprojection length
	// is the global separation minimum, and we don't even need to verify that boxes are in contact at this point 
	// (i.e. check finite versions of features in addition to infinite)

	// face - the closest vertex
	for(ibox=0,icode=0;ibox<2;ibox++,icode+=8) { // ibox provides face, ibox^1 - vertex
		for(iz=0;iz<3;iz++,icode+=8) {
			if (bFindMinUnproj)
				dir[ibox].zero()[iz] = -sgnnz(center[ibox][iz]);
			ix = inc_mod3[iz]; iy = dec_mod3[iz];
			origin.zero(); origin[iz] = size[ibox][iz]*(isg0.z=-sgnnz(dir[ibox][iz]));
			isg1.x = sgnnz(axes[0][iz]); isg1.y = sgnnz(axes[1][iz]); isg1.z = sgnnz(axes[2][iz]);
			pt = center[ibox] - (axes[0]*(size[ibox^1].x*isg1.x) + axes[1]*(size[ibox^1].y*isg1.y) + axes[2]*(size[ibox^1].z*isg1.z))*isg0.z;
			t.set(pt[iz]-origin[iz], dir[ibox][iz]).fixsign();
			bBest = isneg(-t.x) & isneg(t-tmin);
			UPDATE_IDTBEST(tmin,icode + (isg1.z+1)*2 + isg1.y+1 + HALFX(isg1.x+1));
		}
		TransposeBasis(axes);
	}

	// edge - edge
	for(idir1=0,icode=256;idir1<3;idir1++,icode+=52) for(idir0=0;idir0<3;idir0++,icode+=4)  {
		ix0 = inc_mod3[idir0]; iy0 = dec_mod3[idir0];
		ix1 = inc_mod3[idir1]; iy1 = dec_mod3[idir1];
		n[idir0]=0; n[ix0]=-axes[idir1][iy0]; n[iy0]=axes[idir1][ix0]; nlen2=n.len2();
		if (nlen2 < (real)1E-8)
			continue;
		if (bFindMinUnproj)
			dir[0] = n*-sgnnz(n*center[0]);
		t.y = dir[0]*n; t.y *= sg = sgnnz(t.y);
		// choose edges along idir0,idir1 that will generate maximum t
		// Proof of validity is similar to box-triangle: if boxes separate, but infinite slabs (along idir0 and idir1) do not, 
		// then the global minimum contact should be at box feature that is not a slab feature.
		isg0.x = -sgnnz(n[ix0])*sg; isg0.y = -sgnnz(n[iy0])*sg;
		isg1.x = sgnnz(axes[ix1]*n)*sg; isg1.y = sgnnz(axes[iy1]*n)*sg;
		pt0[idir0]=-size[0][idir0]; pt0[ix0]=size[0][ix0]*isg0.x; pt0[iy0]=size[0][iy0]*isg0.y;
		pt1 = center[0] - axes[idir1]*size[1][idir1] + axes[ix1]*(size[1][ix1]*isg1.x) + axes[iy1]*(size[1][iy1]*isg1.y);
		dp = pt1-pt0;	t.x = (dp*n)*sg;
		bBest = isneg(-t.x) & isneg(sqr(t)*dir[0].len2() - sqr(tmin)*dirlen2);
		UPDATE_IDTBEST(tmin, icode + (isg1.y+1+HALFX(isg1.x+1))*16 + (isg0.y+1+HALFX(isg0.x+1)));
		dirlen2 += (dir[0].len2()-dirlen2)*bBest;
	}

	if (idbest==-1)
		return 0;

	int bContact;
	if (idbest & 0x100) { // edge-edge
		LOCAL_NAME_OVERRIDE_OK
		Vec3i isg0,isg1;
		idir0=idbest>>2&3; isg0.y=(idbest&2)-1; isg0.x=((idbest&1)<<1)-1;
		idir1=idbest>>6&3; isg1.y=(idbest>>4&2)-1; isg1.x=((idbest>>4&1)<<1)-1;
		ix0 = inc_mod3[idir0]; iy0 = dec_mod3[idir0];
		ix1 = inc_mod3[idir1]; iy1 = dec_mod3[idir1];
		n[idir0]=0; n[ix0]=-axes[idir1][iy0]; n[iy0]=axes[idir1][ix0]; nlen2 = n.len2();
		if (bFindMinUnproj)	{
			tmin.x *= (dir[0] = n.normalized())*n;
			dir[0] *= -sgnnz(n*center[0]);
			pmode->dir = dir[0]*pbox[0]->Basis;
			dir[1] = pbox[1]->Basis*-pmode->dir;
		}
		pt0[idir0]=-size[0][idir0]; pt0[ix0]=size[0][ix0]*isg0.x; pt0[iy0]=size[0][iy0]*isg0.y;
		pt1 = center[0] - axes[idir1]*size[1][idir1] + axes[ix1]*size[1][ix1]*isg1.x + axes[iy1]*size[1][iy1]*isg1.y;
		dp = pt1-pt0;	dp = dp*tmin.y - dir[0]*tmin.x;
		t0.set((dp^axes[idir1])*n, nlen2*tmin.y); t0.y = (real)1.0/t0.y;
		pcontact->t = tmin.x*t0.y*nlen2;
		pt0[idir0] += t0.x*t0.y;
		bContact = inrange(t0.x*t0.y,(real)0,size[0][idir0]*2) & isneg(fabs_tpl(axes[idir1]*(pt0+dir[0]*pcontact->t-center[0]))-size[1][idir1]);
		pcontact->pt = pbox1->center + pt0*pbox1->Basis + pmode->dir*pcontact->t;
		pcontact->n = (n*pbox1->Basis)*sgnnz(n*(center[0]-dir[0]*pcontact->t));
		pcontact->iFeature[0] = 0x20 | idir0<<2|isg0.y+1|isg0.x+1>>1;
		pcontact->iFeature[1] = 0x20 | idir1<<2|isg1.y+1|isg1.x+1>>1;
	} else { // face-vertex
		LOCAL_NAME_OVERRIDE_OK
		Vec3i isg0,isg1;
		ibox = idbest>>5; iz = idbest>>3&3; 
		if (bFindMinUnproj) {
			dir[ibox].zero()[iz] = -sgnnz(center[ibox][iz]);
			pmode->dir = (dir[ibox]*(1-(ibox<<1)))*pbox[ibox]->Basis;
			dir[ibox^1] = pbox[ibox^1]->Basis*(pmode->dir*((ibox<<1)-1));
		}
		isg0.z=-sgnnz(dir[ibox][iz]);
		isg1.z=(idbest>>1&2)-1; isg1.y=(idbest&2)-1; isg1.x=(idbest<<1&2)-1;
		pcontact->t = tmin.val();
		pcontact->pt = pbox[ibox^1]->center + pmode->dir*pcontact->t*ibox -
			(Vec3(pbox[ibox^1]->size.x*isg1.x, pbox[ibox^1]->size.y*isg1.y, pbox[ibox^1]->size.z*isg1.z)*isg0.z)*pbox[ibox^1]->Basis;
		Vec3 ptface = (pbox[ibox]->Basis*(pcontact->pt-pbox[ibox]->center-pmode->dir*(pcontact->t*(ibox^1)))).abs() - pbox[ibox]->size;
		bContact = isneg(max(ptface[inc_mod3[iz]], ptface[dec_mod3[iz]]));
		pcontact->n = pbox[ibox]->Basis.GetRow(iz)*(isg0.z*(1-(ibox<<1)));
		pcontact->iFeature[ibox] = 0x40 | iz | isg0.z+1>>1;
		pcontact->iFeature[ibox^1] = ((idbest^isg0.z>>31)^7)&7;
	}

	if (parea) {
		// check if the contact is face-face, and build area if it is
		LOCAL_NAME_OVERRIDE_OK
		Vec3i isg0,isg1;
		float diff0,diff1;
		for(idir1=0; idir1<3; idir1++) 
		if (max(max(fabs_tpl(axes[idir1].x),fabs_tpl(axes[idir1].y)),fabs_tpl(axes[idir1].z)) > pmode->maxcos) { // axes[i] is close to some coordinate axis
			//fabs_tpl(axes[idir1].x)+fabs_tpl(axes[idir1].y)+fabs_tpl(axes[idir1].z)<(real)1.05) { 
			//iz = isneg((real)0.997-fabs_tpl(axes[idir1].x))*1 + // find axis coordinate that is close to one
			//		 isneg((real)0.997-fabs_tpl(axes[idir1].y))*2 + 
			//		 isneg((real)0.997-fabs_tpl(axes[idir1].z))*3 - 1;
			//bBest = -isneg((real)0.997-fabs_tpl(axes[idir1].x)); iz = iz&~bBest;
			//bBest = -isneg((real)0.997-fabs_tpl(axes[idir1].y)); iz = iz&~bBest|1&bBest;
			//bBest = -isneg((real)0.997-fabs_tpl(axes[idir1].z)); iz = iz&~bBest|2&bBest;
			//if (iz<0)
			//	continue;
			iz = idxmax3(axes[idir1].abs());
			diff0 = fabs_tpl(center[0][iz]-dir[0][iz]*pcontact->t) - (pbox[0]->size[iz]+fabs_tpl(axes[0][iz])*pbox[1]->size.x+
				fabs_tpl(axes[1][iz])*pbox[1]->size.y+fabs_tpl(axes[2][iz])*pbox[1]->size.z);
			diff1 = fabs_tpl(center[1][idir1]-dir[1][idir1]*pcontact->t) - (pbox[1]->size[idir1]+fabs_tpl(axes[idir1][0])*pbox[0]->size.x+
				fabs_tpl(axes[idir1][1])*pbox[0]->size.y+fabs_tpl(axes[idir1][2])*pbox[0]->size.z);
			if (min(fabs_tpl(diff0),fabs_tpl(diff1))<(pbox[0]->size[iz]+pbox[1]->size[idir1])*0.03f && fabs_tpl(dir[0][iz])>0.01f) { 
				// confirmed face-face contact
				int iprev,ivtx0,ivtx1,iface[2],idedge[2][8];
				Vec2 ptc2d,sz2d,axisx2d,axisy2d,pt2d[2][8];
				Vec3 pt3d;	
				parea->type = geom_contact_area::polygon;
				parea->n1 = pbox[1]->Basis.GetRow(idir1);
				parea->n1 *= (isg1.z = sgnnz(parea->n1*pmode->dir));
				pcontact->n = pbox[0]->Basis.GetRow(iz)*-dir[0][iz];
				ix0 = inc_mod3[iz]; iy0 = dec_mod3[iz];
				ix1 = inc_mod3[idir1]; iy1 = dec_mod3[idir1];
				
				ptc2d.set(center[0][ix0]-dir[0][ix0]*pcontact->t, center[0][iy0]-dir[0][iy0]*pcontact->t);
				sz2d.set(pbox[0]->size[ix0], pbox[0]->size[iy0]);
				axisx2d.set(axes[ix1][ix0], axes[ix1][iy0]);
				axisy2d.set(axes[iy1][ix0], axes[iy1][iy0]);
				pt3d[iz] = pbox[0]->size[iz]*(isg0.z=-sgnnz(dir[0][iz]));
				isg0.z = -isg0.z>>31&3; isg1.z = -isg1.z>>31&3;	
				iface[0] = 0x40 | iz<<1 | isg0.z&1;
				iface[1] = 0x40 | idir1<<1 | isg1.z&1;

				for(i=0;i<4;i++) {
					i = i+1&3; ix = (i^i>>1)&1; iy = (i^isg1.z)>>1&1; ivtx1 = ix<<ix1 | iy<<iy1;
					i = i-1&3; ix = (i^i>>1)&1; iy = (i^isg1.z)>>1&1; ivtx0 = ix<<ix1 | iy<<iy1;
					pt2d[0][i] = ptc2d + axisx2d*(pbox[1]->size[ix1]*((ix<<1)-1)) + axisy2d*(pbox[1]->size[iy1]*((iy<<1)-1));
					idedge[0][i] = 1<<4 | edge_from_vtx((isg1.z&1)<<idir1|ivtx0, (isg1.z&1)<<idir1|ivtx1);
				}
				parea->npt = crop_polygon_with_bound(pt2d[0],idedge[0],					4, sz2d,0,iy0<<2|isg0.z&1, pt2d[1],idedge[1]);
				parea->npt = crop_polygon_with_bound(pt2d[1],idedge[1],parea->npt, sz2d,1,ix0<<2|isg0.z<<1&2, pt2d[0],idedge[0]);
				parea->npt = crop_polygon_with_bound(pt2d[0],idedge[0],parea->npt, sz2d,2,iy0<<2|2|isg0.z&1, pt2d[1],idedge[1]);
				parea->npt = crop_polygon_with_bound(pt2d[1],idedge[1],parea->npt, sz2d,3,ix0<<2|1|isg0.z<<1&2, pt2d[0],idedge[0]);
				for(i=0;i<parea->npt;i++) {
					iprev = i-1&-i>>31 | parea->npt-1&i-1>>31; 
					pt3d[ix0] = pt2d[0][i].x; pt3d[iy0] = pt2d[0][i].y;
					parea->pt[i] = pt3d*pbox[0]->Basis + pbox[0]->center + pmode->dir*pcontact->t;
					parea->piPrim[0][i] = parea->piPrim[1][i] = 0;
					if ((idedge[0][iprev]^idedge[0][i])>>4) {
						parea->piFeature[idedge[0][iprev]>>4][i] = 0x20 | idedge[0][iprev]&0xF;
						parea->piFeature[idedge[0][i]>>4][i] = 0x20 | idedge[0][i]&0xF;
					} else {
						parea->piFeature[idedge[0][i]>>4][i] = vtx_from_edges(idedge[0][iprev]&0xF,idedge[0][i]&0xF);
						parea->piFeature[idedge[0][i]>>4^1][i] = iface[idedge[0][i]>>4^1];
					}
				}
				if (parea->npt)
					goto gotarea;
			}
		}

		// check if the contact is face-edge, and build area if it is
		for(int idx=0;idx<9;idx++) {
			idir1 = isneg(2-idx)+isneg(5-idx); iz = idx-idir1*3; // idir1 = idx/3; iz = idx%3
			if (sqr(axes[idir1][iz]) < (1-pmode->maxcos)*2) {	// check all near-zeros in Basis21 buffer
				ix1 = inc_mod3[idir1]; iy1 = dec_mod3[idir1];
				for(ibox=0;ibox<2;ibox++) {
					if (fabs_tpl(fabs_tpl(center[ibox][iz]-dir[ibox][iz]*pcontact->t) -
						(pbox[ibox]->size[iz]+fabs_tpl(axes[ix1][iz])*pbox[ibox^1]->size[ix1]+fabs_tpl(axes[iy1][iz])*pbox[ibox^1]->size[iy1])) < 
						min(pbox[ibox]->size[iz],pbox[1]->size[ix1]+pbox[ibox^1]->size[iy1])*0.05f) 
					{	// confirmed face(ibox.iz) - edge(ibox^1.idir1) contact
						Vec2 pt2d[2],sz2d,axisx2d,axisy2d;
						Vec3 ptc,pt3d;
						ptc = center[ibox]-dir[ibox]*pcontact->t;
						ix0 = inc_mod3[iz]; iy0 = dec_mod3[iz]; isg0.z = sgnnz(ptc[iz]);
						parea->type = geom_contact_area::polyline;
						axisx2d.set(axes[ix1][ix0],axes[ix1][iy0]) *= (isg1.x = sgnnz(axes[ix1][iz])*-isg0.z);
						axisy2d.set(axes[iy1][ix0],axes[iy1][iy0]) *= (isg1.y = sgnnz(axes[iy1][iz])*-isg0.z);
						pt2d[0].set(ptc[ix0],ptc[iy0]) += axisx2d*pbox[ibox^1]->size[ix1]+axisy2d*pbox[ibox^1]->size[iy1];
						sz2d.set(axes[idir1][ix0],axes[idir1][iy0]) *= pbox[ibox^1]->size[idir1];
						pt2d[1] = pt2d[0]+sz2d; pt2d[0] -= sz2d;
						parea->piFeature[ibox][0] = parea->piFeature[ibox][1] = 0x40 | iz<<1 | 1+isg0.z>>1;
						parea->piFeature[ibox^1][0] = (isg1.x+1>>1)<<ix1 | (isg1.y+1>>1)<<iy1;
						parea->piFeature[ibox^1][1] = parea->piFeature[ibox^1][0] | 1<<idir1;

						int iedge1 = 0x20 | idir1<<2 | 1+isg1.x>>1 | 1+isg1.y;
						crop_segment_with_bound(pt2d,pbox[ibox]->size[ix0],0, parea->piFeature[ibox],parea->piFeature[ibox^1], 0x20|iy0<<2|1+isg0.z>>1,iedge1); 
						crop_segment_with_bound(pt2d,pbox[ibox]->size[iy0],1, parea->piFeature[ibox],parea->piFeature[ibox^1], 0x20|ix0<<2|1+isg0.z,iedge1);
						if (len2(pt2d[0]-pt2d[1])>sqr(pmode->minPtDist)) {
							pt3d[iz] = pbox[ibox]->size[iz]*isg0.z;
							for(i=0;i<2;i++) {
								pt3d[ix0] = pt2d[i].x; pt3d[iy0] = pt2d[i].y; 
								parea->pt[i] = pt3d*pbox[ibox]->Basis + pbox[ibox]->center + pmode->dir*(pcontact->t*(ibox^1));
								parea->piPrim[0][i] = parea->piPrim[1][i] = 0;
							}
							parea->npt = 2;
							if (ibox) {
								origin = pbox[0]->center + pbox[0]->Basis.GetRow(ix1)*(isg1.x*pbox[0]->size[ix1]) + pbox[0]->Basis.GetRow(iy1)*(isg1.y*pbox[0]->size[iy1]);
								origin += pmode->dir*pcontact->t;
								n = pbox[0]->Basis.GetRow(idir1);
								for(i=0;i<2;i++)
									parea->pt[i] = origin+n*(n*(parea->pt[i]-origin));
							}
							Vec3 ns[2];
							ns[ibox] = pbox[ibox]->Basis.GetRow(iz)*isg0.z;
							ns[ibox^1] = pbox[ibox^1]->Basis.GetRow(idir1);
							ns[ibox^1] = ns[ibox^1]^(ns[ibox^1]^ns[ibox]);
							ns[ibox^1] = ns[ibox^1].len2()>0.001f ? ns[ibox^1].normalized() : -ns[ibox];
							pcontact->n = ns[0]; parea->n1 = ns[1];
						}
						if (parea->npt)
							goto gotarea;
					}
					idir0=idir1;idir1=iz;iz=idir0;
					ix1 = inc_mod3[idir1]; iy1 = dec_mod3[idir1];
					TransposeBasis(axes);
				}
			}
		}

		gotarea:
		if (parea->npt) {
			pcontact->pt = parea->pt[0];
			pcontact->iFeature[0] = parea->piFeature[0][0];
			pcontact->iFeature[1] = parea->piFeature[1][0];
			bContact = 1;
		}
	}

	return bContact;
}


static geom_contact_area dummyArea;
Vec3 dummyAreaPt[16];
static int dummyAreaInt[16];

int box_cylinder_lin_unprojection(unprojection_mode *pmode, const box *pbox,int iFeature1, const cylinder *pcyl,int iFeature2,
																	contact *pcontact, geom_contact_area *parea)
{
	Vec3r center,axis,dir,size=pbox->size,c,n,pt,vec0,vec1,dir_best(ZERO);
	real nlen,dlen,r=pcyl->r,hh=pcyl->hh,ka,kb,kc,kd,e=pmode->minPtDist;
	Vec2r pt2d;
	quotient tmin(pmode->tmax,1),t,t0,tmax;
	int i,j,idbest=-1,bFindMinUnproj,bBest,bSeparated,icap,bContact,bCapped=iszero(iFeature2-0x43)^1;
	Vec3i isg,ic; 
	center = pbox->Basis*(pcyl->center-pbox->center);
	axis = pbox->Basis*pcyl->axis;
	dir = pbox->Basis*pmode->dir;
	bFindMinUnproj = iszero(pmode->dir.len2());
	kd = sgnnz((center-axis*hh).len2()-(center+axis*hh).len2())*(real)0.0005;

	// box.face - cyl.capedge - may need this to get proper t even for capsules
	for(ic.z=0;ic.z<3;ic.z++) {
		if (bFindMinUnproj)
			dir.zero()[ic.z] = -sgnnz(center[ic.z]);
		isg.z = -sgnnz(dir[ic.z]); icap = sgnnz(axis[ic.z]*-isg.z+kd);
		c.z = center[ic.z]+axis[ic.z]*(hh*icap);
		(n = axis*axis[ic.z])[ic.z]-=1; n*=isg.z;
		nlen = max((real)0.001,n.len());
		t.set(((c.z-size[ic.z]*isg.z)*nlen+n[ic.z]*r)*-isg.z, fabs_tpl(dir[ic.z])*nlen);
		bBest = isneg(-t.x) & isneg(t-tmin);
		UPDATE_IDBEST(tmin,ic.z<<2|isg.z+1|icap+1>>1);
		dir_best = dir_best*(bBest^1) + dir*bBest;
	}

	if (bCapped) {
		// box.vertex - cyl.cap
		if (bFindMinUnproj)
			dir = axis*-sgnnz(center*axis);
		icap = sgnnz(dir*axis);
		t.y = fabs_tpl(dir*axis);	ka = axis.abs()*size;
		t.x = (center*axis)*icap + hh + ka;
		t.x += min(hh,ka)*fabs_tpl(t.y-(real)0.999); // give face-capedge some priority boost for degenerate cases
		bBest = isneg(-t.x) & isneg(t-tmin);
		UPDATE_IDBEST(tmin,0x20 | icap+1>>1);
		dir_best = dir_best*(bBest^1) + dir*bBest;
	}

	// box.vertex - cyl.side
	for(i=0;i<8;i++) {
		isg.x=(i>>1&2)-1; isg.y=(i&2)-1; isg.z=(i<<1&2)-1;
		pt.Set(size.x*isg.x, size.y*isg.y, size.z*isg.z) -= center;
		if (bFindMinUnproj)	{
			dir = (pt-axis*(pt*axis)).normalized();
			dir *= -sgnnz(dir*center);
		}
		vec0 = pt^axis; vec1 = dir^axis;
		ka=vec1*vec1; kb=vec0*vec1; kc=vec0*vec0-r*r; kd=kb*kb-ka*kc;
		if (isnonneg(kd) & (isneg(kb)|isneg(kb*kb-kd))) {
			t.set(-kb+sqrt_tpl(kd), ka);
			pt = pt*t.y + dir*t.x; // cyl.center->contact point vector
			pt -= axis*(pt*axis); // cyl.axis->contact point vector
			bSeparated = isneg(pt.x*isg.x-e) & isneg(pt.y*isg.y-e) & isneg(pt.z*isg.z-e); // no outgoing box edge points 'inside' the cylinder
			bBest = isneg(t-tmin) & bSeparated;
			UPDATE_IDBEST(tmin,0x40 | i);
			dir_best = dir_best*(bBest^1) + dir*bBest;
		}
	}

	// box.edge - cyl.side
	for(ic.z=0;ic.z<3;ic.z++) {
		ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];
		n = -cross_with_ort(axis,ic.z);	nlen = n.len();
		if (bFindMinUnproj)	{
			dir = n*-sgnnz(n*center);	dlen = nlen;
		} else dlen = 1;
		isg.z = sgnnz(dir*n);
		isg.x = -sgnnz(n[ic.x])*isg.z; isg.y = -sgnnz(n[ic.y])*isg.z;
		pt[ic.z]=0; pt[ic.x]=size[ic.x]*isg.x; pt[ic.y]=size[ic.y]*isg.y;	
		t.set((r*nlen+((center-pt)*n)*isg.z)*dlen, fabs_tpl(dir*n));
		t += isneg(t.x)*isneg(-idbest-1)*r*0.001f;
		bBest = isneg(-t.x) & isneg(t-tmin);
		UPDATE_IDBEST(tmin,0x60 | ic.z<<3 | isg.z+1<<1 | isg.y+1 | isg.x+1>>1);
		dir_best = dir_best*(bBest^1) + dir*bBest;
	}

	if (bCapped) {
		// box.edge - cyl.capedge
		if (bFindMinUnproj)
			dir = dir_best; // finding real minimizing edge-capedge unprojection involves solving 4th order eq., not worth the effort here
		tmax.set(0,1); j=idbest; // for edge-capedge search for max contacting t, since we don't calculate mindir anyway
		for(ic.z=0;ic.z<3;ic.z++) if (fabs_tpl(axis[ic.z])>0.01f) {
			ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];	
			for(i=0;i<8;i++) {
				icap=(i>>1&2)-1; isg.x=(i<<1&2)-1; isg.y=(i&2)-1;
				pt[ic.z]=0; pt[ic.x]=size[ic.x]*isg.x; pt[ic.y]=size[ic.y]*isg.y;
				pt -= center+axis*(hh*icap); // pt = edge center - cyl.cap center
				vec1 = axis^cross_with_ort(dir,ic.z); vec0 = axis^cross_with_ort(pt,ic.z);
				ka=vec1*vec1; kb=vec0*vec1; kc=vec0*vec0-sqr(r*axis[ic.z]); kd=kb*kb-ka*kc;
				if (isnonneg(kd) & (isneg(kb)|isneg(kb*kb-kd))) {
					t.set(-kb+sqrt_tpl(kd), ka);
					pt = pt*t.y+dir*t.x;
					t0.set(-(pt*axis), axis[ic.z]*t.y).fixsign();
					bContact = isneg(fabs_tpl(t0.x)-size[ic.z]*fabs_tpl(t0.y));
					(pt *= fabs_tpl(axis[ic.z]))[ic.z] += t0.x;	// pt = pt+dir*t+edge*t0-c
					bSeparated = isneg(((pt^axis)*cross_with_ort(axis,ic.z))*axis[ic.z]*-icap); // intersection along the edge lies outside the cylinder
					pt = axis^pt; // tangential direction at cap point, neither positive nor flipped should point inside the box
					bSeparated &= isneg(pt[ic.x]*isg.x) ^ isneg(pt[ic.y]*isg.y);
					bBest = isneg(tmax-t) & bContact & bSeparated;	
					UPDATE_IDBEST(tmax,0x80 | ic.z<<3|icap+1<<1|isg.y+1|isg.x+1>>1);
				}
			}
		}
		i=idbest; idbest=j; t=tmax;
		bBest = (iszero(t.x)^1) & isneg(t-tmin);
		UPDATE_IDBEST(tmin,i);
	}

	if (idbest==-1)
		return 0;

	if (bFindMinUnproj) {
		dir = dir_best;
		if (fabs_tpl(dir.len2()-(real)1.0)>(real)0.001)
			dir.normalize();
		pmode->dir = dir*pbox->Basis;
	}
	bContact = 0;
	switch (idbest&0xE0) {
		case 0x00: // box.face - cyl.capedge
			ic.z=idbest>>2; isg.z=(idbest&2)-1; icap=(idbest<<1&2)-1;
			pcontact->t = tmin.val();
			(n = axis*axis[ic.z])[ic.z]-=1; n*=isg.z;
			if ((nlen = n.len()) < (real)0.001) {
				n = dir*pcontact->t-center;
				nlen = max((real)1e-10, (n -= axis*(n*axis)).len());
			}
			pt = center+axis*(hh*icap)+n*(r/nlen);
			ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];
			bContact = isneg(fabs_tpl(pt[ic.x]-dir[ic.x]*pcontact->t)-size[ic.x]-e) & isneg(fabs_tpl(pt[ic.y]-dir[ic.y]*pcontact->t)-size[ic.y]-e);
			pcontact->pt = pt*pbox->Basis + pbox->center;
			pcontact->n = pbox->Basis.GetRow(ic.z)*isg.z;
			pcontact->iFeature[0] = 0x40 | ic.z<<1 | isg.z+1>>1;
			pcontact->iFeature[1] = 0x20 | icap+1>>1;
			break;
		case 0x20: // box.vertex - cyl.cap
			icap = (idbest<<1&2)-1;
			isg.Set(-sgnnz(dir.x),-sgnnz(dir.y),-sgnnz(dir.z));
			pcontact->t = tmin.val();
			i = idxmax3(axis.abs());
			if (fabs_tpl(axis[i]) > (real)0.999) {
				pt.zero()[i] = size[i]*isg[i];
				pt += dir*pcontact->t-center-axis*(hh*icap);
				isg[inc_mod3[i]] = -sgnnz(pt[inc_mod3[i]]);
				isg[dec_mod3[i]] = -sgnnz(pt[dec_mod3[i]]);
				pt = Vec3r(size.x*isg.x,size.y*isg.y,size.z*isg.z) + dir*pcontact->t;
				bContact = isneg((pt-center-axis*(hh*icap)).len2()-sqr(r+e));
				isg[inc_mod3[i]] &= -bContact; isg[dec_mod3[i]] &= -bContact;
			}
			pt = Vec3r(size.x*isg.x,size.y*isg.y,size.z*isg.z) + dir*pcontact->t;
			if (!bContact)
				bContact = isneg((pt-center-axis*(hh*icap)).len2()-sqr(r+e));
			pcontact->pt = pt*pbox->Basis + pbox->center;
			pcontact->n = pcyl->axis*(hh*-icap);
			pcontact->iFeature[0] = isg.z+1<<1|isg.y+1|isg.x+1>>1;
			pcontact->iFeature[1] = 0x41+(icap+1>>1);
			break;
		case 0x40: // box.vertex - cyl.side
			isg.Set((idbest>>1&2)-1,(idbest&2)-1,(idbest<<1&2)-1);
			pt.Set(size.x*isg.x, size.y*isg.y, size.z*isg.z);
			pcontact->t = tmin.val();
			pt += dir*pcontact->t;
			bContact = isneg(fabs_tpl((pt-center)*axis)-hh-e);
			pcontact->pt = pt*pbox->Basis + pbox->center;
			pcontact->n = pcyl->center-pcontact->pt;
			pcontact->n -= pcyl->axis*(pcontact->n*pcyl->axis);
			pcontact->iFeature[0] = isg.z+1<<1|isg.y+1|isg.x+1>>1;
			pcontact->iFeature[1] = 0x40;
			break;
		case 0x60: // box.edge - cyl.side
			ic.z = idbest>>3&3; ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];
			isg.Set((idbest<<1&2)-1,(idbest&2)-1,(idbest>>1&2)-1);
			n = -cross_with_ort(axis,ic.z);
			pt[ic.z]=0; pt[ic.x]=size[ic.x]*isg.x; pt[ic.y]=size[ic.y]*isg.y;
			t0.set(((center-pt)*tmin.y-dir*tmin.x ^ axis)*n, n.len2()*tmin.y); 
			if (fabs(t0.y) > 1e-10f) {
				t0.y = (real)1.0/t0.y;
				pt[ic.z] = (t0.x*=t0.y);
				pcontact->t = tmin.x*t0.y*n.len2();
			} else
				pcontact->t = tmin.val();
			pt += dir*pcontact->t;
			bContact = isneg(fabs_tpl(t0.x)-size[ic.z]-e) & isneg(fabs_tpl((pt-center)*axis)-hh-e);
			pcontact->pt = pt*pbox->Basis + pbox->center;
			n = pcyl->center-pcontact->pt;
			pcontact->n = n-pcyl->axis*(n*pcyl->axis);
			if (!bContact) {
				pt[ic.z]=0; pcontact->pt = pt*pbox->Basis + pbox->center;
			}
			pcontact->iFeature[0] = 0x20 | ic.z<<2|isg.y+1|isg.x+1>>1;
			pcontact->iFeature[1] = 0x40;
			break;
		case 0x80: // box.edge - cyl.capedge
			ic.z = idbest>>3&3; ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];
			icap=(idbest>>1&2)-1; isg.y=(idbest&2)-1; isg.x=(idbest<<1&2)-1;
			pt[ic.z]=0; pt[ic.x]=size[ic.x]*isg.x; pt[ic.y]=size[ic.y]*isg.y;
			c = center+axis*(hh*icap);
			t0.set(((c-pt)*tmin.y-dir*tmin.x)*axis, axis[ic.z]*tmin.y); t0.y=(real)1.0/t0.y;
			pt[ic.z] += (t0.x*=t0.y);
			pcontact->t = tmin.x*t0.y*axis[ic.z];
			vec0 = pt; pt += dir*pcontact->t;
			bContact = isneg(fabs_tpl(t0.x)-size[ic.z]-e);
			pcontact->pt = pt*pbox->Basis + pbox->center;
			n = cross_with_ort(pt-c ^ axis,ic.z);
			pcontact->n = (n*sgnnz(n*vec0))*pbox->Basis;
			pcontact->iFeature[0] = 0x20 | ic.z<<2|isg.y+1|isg.x+1>>1;
			pcontact->iFeature[1] = 0x20 | icap+1>>1;
	}

	if (!parea && pmode->bCheckContact) {
		parea = &dummyArea;
		parea->pt = dummyAreaPt;
		parea->piPrim[0]=parea->piPrim[1]=parea->piFeature[0]=parea->piFeature[1] = dummyAreaInt;
		parea->npt = 0; parea->nmaxpt = 16;
		parea->minedge = 0;
	}

	if (parea) {
		const real tol = (real)0.1;
		int iCheckEdgeSide = -1;
		if (max(max(fabs_tpl(axis.x),fabs_tpl(axis.y)),fabs_tpl(axis.z)) > pmode->maxcos) { // axis is close to some coordinate axis
			ic.z = -1; // find axis coordinate that is close to one
			bBest = -isneg((real)0.998-fabs_tpl(axis.x)); ic.z = ic.z&~bBest;
			bBest = -isneg((real)0.998-fabs_tpl(axis.y)); ic.z = ic.z&~bBest|1&bBest;
			bBest = -isneg((real)0.998-fabs_tpl(axis.z)); ic.z = ic.z&~bBest|2&bBest;
			if (ic.z>=0) {
				c = center-dir*pcontact->t+axis*(hh*(icap=sgnnz(axis*dir))); 
				ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];	isg.z = -sgnnz(dir[ic.z]);
				dlen = sqr(max((real)0.0,fabs_tpl(c[ic.x])-size[ic.x]))+sqr(max((real)0.0,fabs_tpl(c[ic.y])-size[ic.y]));
				// check for box.face-cyl.cap area contact
				if (fabs_tpl(fabs_tpl(c[ic.z])-size[ic.z])<min(size[ic.z],hh)*tol && dlen<r*r && fabs_tpl(axis*dir)>(real)0.1) { 
					if (bCapped) {
						pt[ic.z] = size[ic.z]*isg.z; pt2d.set(r,0);
						for(i=0;i<8;i++) { // add points on cap edge with 45 degrees step (if they are inside)
							pt[ic.x] = c[ic.x]+pt2d.x; pt[ic.y] = c[ic.y]+pt2d.y;
							parea->pt[parea->npt] = pt*pbox->Basis + pbox->center + pmode->dir*pcontact->t;
							parea->piPrim[0][parea->npt] = parea->piPrim[1][parea->npt] = 0;
							parea->piFeature[0][parea->npt] = 0x40 | ic.z<<1 | isg.z+1>>1;
							parea->piFeature[1][parea->npt] = 0x20 | icap+1>>1;
							parea->npt += isneg(fabs_tpl(pt[ic.x])-size[ic.x]) & isneg(fabs_tpl(pt[ic.y])-size[ic.y]);
							nlen=pt2d.x; pt2d.x=(pt2d.x-pt2d.y)*(sqrt2/2); pt2d.y=(nlen+pt2d.y)*(sqrt2/2); // rotate by 45 degrees
						}
						// add circle-rect intersection points
						for(i=0;i<4;i++) if ((kd = r*r-sqr((pt[ic[i&1]] = size[ic[i&1]]*((i&2)-1))-c[ic[i&1]]))>0) 
							for(j=0,pt[ic[i&1^1]]=c[ic[i&1^1]]-(kd=sqrt_tpl(kd)); j<2; j++,pt[ic[i&1^1]]+=kd*2) {//if (fabs_tpl(pt[ic[i&1^1]])<size[ic[i&1^1]]) {
								nlen = pt[ic[i&1^1]]; 
								pt[ic[i&1^1]] = max(-size[ic[i&1^1]],min(size[ic[i&1^1]],pt[ic[i&1^1]]));
								parea->pt[parea->npt] = pt*pbox->Basis + pbox->center + pmode->dir*pcontact->t;
								pt[ic[i&1^1]] = nlen;
								parea->piPrim[0][parea->npt] = parea->piPrim[1][parea->npt] = 0;
								parea->piFeature[0][parea->npt] = 0x20 | ic[i&1^1]<<2 | isg.z+1>>(i&1^1) | (i&2)>>(i&1);
								parea->piFeature[1][parea->npt++] = 0x20 | icap+1>>1;
							}
						parea->type = geom_contact_area::polygon;
						pcontact->n = pbox->Basis.GetRow(ic.z)*isg.z;
						parea->n1 = pcyl->axis*icap;
						goto gotarea;
					}
				} else // check for box.edge-cyl.side area contact later (if other area contacts fail)
					iCheckEdgeSide = ic.z;
			}
		}

		for(ic.z=0;ic.z<3;ic.z++) if (sqr(axis[ic.z]) < (1-pmode->maxcos)*2) {
			c = center-dir*pcontact->t;
			ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];	isg.z = -sgnnz(dir[ic.z]);
			Vec2 pts2d[2];

			// check for box.face-cyl.side area contact
			if (fabs_tpl(fabs_tpl(c[ic.z])-size[ic.z]-r) < min(size[ic.z],r)*tol) {
				pts2d[0].set(c[ic.x]-axis[ic.x]*hh, c[ic.y]-axis[ic.y]*hh);
				pts2d[1].set(c[ic.x]+axis[ic.x]*hh, c[ic.y]+axis[ic.y]*hh);
				parea->piFeature[1][0] = parea->piFeature[1][1] = 0x40;
				parea->piFeature[0][0] = parea->piFeature[0][1] = 0x40 | ic.z<<1|isg.z+1>>1;
				parea->piPrim[0][0]=parea->piPrim[0][1]=parea->piPrim[1][0]=parea->piPrim[1][1] = 0;
				pt[ic.z] = size[ic.z]*isg.z;
				crop_segment_with_bound(pts2d,size[ic.x],0, parea->piFeature[0],parea->piFeature[1], 0x20|ic.y<<2|1+isg.z>>1,0x40); 
        crop_segment_with_bound(pts2d,size[ic.y],1, parea->piFeature[0],parea->piFeature[1], 0x20|ic.x<<2|1+isg.z,0x40);

				if (len2(pts2d[0]-pts2d[1])>e*e) {
					parea->type = geom_contact_area::polyline;
					for(i=0;i<2;i++) {
						pt[ic.x] = pts2d[i].x; pt[ic.y] = pts2d[i].y;
						parea->pt[i] = pt*pbox->Basis + pbox->center + pmode->dir*pcontact->t;
					} 
					pcontact->n = pbox->Basis.GetRow(ic.z)*isg.z;
					parea->n1  = pcyl->axis^(pcyl->axis^pcontact->n);
					parea->n1 = parea->n1.len2()>0.001f ? parea->n1.normalized() : -pcontact->n;
					parea->npt = 2; goto gotarea;
				}
			}

			if (bCapped) { // check for box.edge-cyl.cap area contact
				c += axis*(hh*(icap=sgnnz(axis*dir))); pt[ic.z] = 0;
				for(j=0,i=1;i<4;i++) { // find the edge in direction ic.z that is the closest to the cap plane
					pt[ic.x]=size[ic.x]*((i<<1&2)-1); pt[ic.y]=size[ic.y]*((i&2)-1); t.x=fabs_tpl((pt-c)*axis);
					pt[ic.x]=size[ic.x]*((j<<1&2)-1); pt[ic.y]=size[ic.y]*((j&2)-1); t.y=fabs_tpl((pt-c)*axis);
					bBest = isneg(t.x-t.y); j = j&~-bBest | i&-bBest;
				}
				isg.x=(j<<1&2)-1; isg.y=(j&2)-1; pt[ic.x]=size[ic.x]*isg.x; pt[ic.y]=size[ic.y]*isg.y;
				dlen = sqr(c[ic.x]-pt[ic.x])+sqr(c[ic.y]-pt[ic.y]);
				if (fabs_tpl((c-pt)*axis)<hh*tol && dlen<r*r) {
					dlen = sqrt_tpl(r*r-dlen);
					if (c[ic.z]-dlen<size[ic.z] || c[ic.z]+dlen>-size[ic.z]) {
						for(i=0;i<2;i++) {
							pt[ic.z] = min(size[ic.z],max(-size[ic.z],c[ic.z]+dlen*(i*2-1)));
							parea->pt[i] = pt*pbox->Basis + pbox->center + pmode->dir*pcontact->t;
						}
						parea->type = geom_contact_area::polyline;
						parea->piPrim[0][0]=parea->piPrim[0][1]=parea->piPrim[1][0]=parea->piPrim[1][1] = 0;
						parea->piFeature[0][0] = parea->piFeature[0][1] = 0x20 | ic.z<<2|isg.y+1|isg.x+1>>1;
						parea->piFeature[1][0] = parea->piFeature[1][1] = 0x40 | (icap+1>>1)+1;
						parea->n1 = pcyl->axis*icap;
						pcontact->n = pbox->Basis.GetRow(ic.z);
						pcontact->n = pcontact->n^(pcontact->n^parea->n1);
						pcontact->n = pcontact->n.len2()>0.001f ? pcontact->n.normalized() : -parea->n1;
						parea->npt = 2; goto gotarea;
					}
				}
			}
		}

		// check box.edge-cyl.side area contact 
		if ((ic.z=iCheckEdgeSide)>=0) {
			ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];	isg.z = -sgnnz(dir[ic.z]);
			c = center-dir*pcontact->t;
			dlen = sqr(max((real)0.0,fabs_tpl(c[ic.x])-size[ic.x]))+sqr(max((real)0.0,fabs_tpl(c[ic.y])-size[ic.y]));
			if (fabs_tpl(c[ic.z])-size[ic.z]<hh && fabs_tpl(dlen-r*r)<sqr(r*tol)) { 
				parea->type = geom_contact_area::polyline;
				c[ic.z] = center[ic.z]-dir[ic.z]*pcontact->t-fabs_tpl(axis[ic.z])*hh; 
				isg.x = sgnnz(c[ic.x]); isg.y = sgnnz(c[ic.y]);
				pt[ic.x] = size[ic.x]*isg.x; pt[ic.y] = size[ic.y]*isg.y; pt[ic.z] = max(c[ic.z],-size[ic.z]);
				parea->pt[0] = pt*pbox->Basis + pbox->center + pmode->dir*pcontact->t;
				pt[ic.z] = min(c[ic.z]+fabs_tpl(axis[ic.z])*hh*2,size[ic.z]);
				parea->pt[1] = pt*pbox->Basis + pbox->center + pmode->dir*pcontact->t;
				parea->piFeature[0][0] = parea->piFeature[0][1] = 0x20 | ic.z<<2|isg.y+1|isg.x+1>>1;
				parea->piFeature[1][0] = parea->piFeature[1][1] = 0x40;
				parea->piPrim[0][0]=parea->piPrim[0][1]=parea->piPrim[1][0]=parea->piPrim[1][1] = 0;
				if (!bContact) {
					pcontact->n = pcyl->center-parea->pt[0];
					(pcontact->n -= pcyl->axis*(pcontact->n*pcyl->axis)).normalize();
				}
				parea->n1 = -pcontact->n;
				parea->npt = 2;
			}
		}

		gotarea:
		if (parea->npt) {
			pcontact->pt = parea->pt[0];
			pcontact->iFeature[0] = parea->piFeature[0][0];
			pcontact->iFeature[1] = parea->piFeature[1][0];
			bContact = 1;
		}
	}

	if (bContact & (bCapped^1) && (!parea || parea->npt<2)) {
		// make sure contact point lies on primitives (there might be false hits if they were initially separated)
		e = 1.1f;
		pt = (pbox->Basis*(pcontact->pt-pbox->center-pmode->dir*pcontact->t)).abs();
		bContact = isneg(max(max(pt.x-size.x*e,pt.y-size.y*e),pt.z-size.z*e));
		c = pcontact->pt-pcyl->center; nlen = c*pcyl->axis;
		bContact &= isneg(max(fabs_tpl(nlen)-pcyl->hh*e, (c-pcyl->axis*nlen).len2() - sqr(pcyl->r*e)));
		if (!bContact)
			return 0;
	}

	if (bContact & bFindMinUnproj && parea && parea->npt<2) {
		Vec3 rdown = pcontact->pt-pcyl->center;	rdown -= pcyl->axis*(pcyl->axis*rdown);
		Vec3r org = center+pbox->Basis*rdown;
		quotient tray[2] = { quotient(-pcyl->hh), quotient(pcyl->hh) };
		for(i=0; i<3; i++) {
			j = sgnnz(axis[i]);
			tray[0] = max(tray[0], quotient(-size[i]-org[i]*j,axis[i]*j));
			tray[1] = min(tray[1], quotient( size[i]-org[i]*j,axis[i]*j));
		}
		if (tray[1]-tray[0] > pcyl->hh) {
			parea->type = geom_contact_area::polyline;
			for(i=0;i<2;i++) 
				parea->pt[i] = pcyl->center + rdown + pcyl->axis*tray[i].val() + pmode->dir*pcontact->t;
			parea->n1 = rdown.normalized();
			parea->piFeature[1][0]=parea->piFeature[1][1] = 0x20; 
			parea->piFeature[0][0]=parea->piFeature[0][1] = pcontact->iFeature[0];
			parea->npt = 2;
		}
	}

	return bContact|(pmode->bCheckContact^1);
}


void flip_contact(const unprojection_mode *pmode, contact *pcontact, geom_contact_area *parea)
{
	pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
	int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	if (parea && parea->npt) {
		Vec3 n=-pcontact->n;	pcontact->n=parea->n1; parea->n1=n;
		for(int i=0;i<parea->npt;i++) {
			iFeature=parea->piFeature[0][i]; parea->piFeature[0][i]=parea->piFeature[1][i]; parea->piFeature[1][i]=iFeature;
			parea->pt[i] -= pmode->dir*pcontact->t;
			parea->pt[i] -= pcontact->n*(pcontact->n*(parea->pt[i]-pcontact->pt));
		}
	}
}

int cylinder_box_lin_unprojection(unprojection_mode *pmode, const cylinder *pcyl,int iFeature1, const box *pbox,int iFeature2, 
																	contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  box_cylinder_lin_unprojection(pmode,pbox,iFeature2,pcyl,iFeature1,pcontact,parea);
	if (res)
		flip_contact(pmode,pcontact,parea);
	pmode->dir.Flip();
	return res;
}

int box_sphere_lin_unprojection(unprojection_mode *pmode, const box *pbox,int iFeature1, const sphere *psph,int iFeature2,
																contact *pcontact, geom_contact_area *parea)
{
	Vec3r center,size=pbox->size,vec0,vec1,dir,pt;
	real r=psph->r,ka,kb,kc,kd,dlen;
	quotient t,tmax(pmode->tmin,1);
	int i,ix,iy,iz,niz,isum,idbest=-1,bBest,bContact;
	Vec3i isg;
	center = pbox->Basis*(psph->center-pbox->center);
	dir = pbox->Basis*pmode->dir;

	pcontact->iFeature[1] = 0x40;
	if (pmode->dir.len2()==0) {	// minimum unprojection dir requested
		for(iz=isum=niz=0,dlen=0; iz<3; iz++) {
			i = isneg(ka=size[iz]-fabs_tpl(center[iz]));
			niz+=i; isum+=i*iz;
			dlen += sqr(min((real)0,ka));
		}
		if (dlen>r*r)
			return 0;
		if (niz==0) {
			//Vec3r csize(fabs_tpl(center.x)*size.y*size.z, fabs_tpl(center.y)*size.x*size.z, fabs_tpl(center.z)*size.x*size.y);
			Vec3r csize(fabs_tpl(center.x)-size.x, fabs_tpl(center.y)-size.y, fabs_tpl(center.z)-size.z);
			niz=1; isum=idxmax3(csize);
		}
		switch (niz) {
			case 1: 
				iz=isum; isg.z=sgnnz(center[iz]); dir.zero()[iz]=-isg.z; pcontact->t=r+size[iz]-isg.z*center[iz]; 
				pcontact->iFeature[0] = 0x40|iz<<1|isg.z+1>>1; break;
			case 2: 
				iz=3-isum; ix=inc_mod3[iz]; iy=dec_mod3[iz]; isg.x=sgnnz(center[ix]); isg.y=sgnnz(center[iy]);
				pt[ix]=size[ix]*isg.x; pt[iy]=size[iy]*isg.y; pt[iz]=center[iz];
				dlen=(dir=pt-center).len(); pcontact->t=r-dlen; dir/=dlen; 
				pcontact->iFeature[0] = 0x20|iz<<2|isg.y+1|isg.x+1>>1; break;
			case 3: isg.x=sgnnz(center.x); isg.y=sgnnz(center.y); isg.z=sgnnz(center.z);
				pt.x=size.x*isg.x; pt.y=size.y*isg.y; pt.z=size.z*isg.z;
				dlen=(dir=pt-center).len(); pcontact->t=r-dlen; dir/=dlen; 
				pcontact->iFeature[0] = isg.z+1<<1|isg.y+1|isg.z+1>>1;
		}
		pmode->dir = dir*pbox->Basis;
		pcontact->n = -pmode->dir;
		pcontact->pt = psph->center+pmode->dir*r;
		return 1;
	}

	// box face - sphere
	for(iz=0;iz<3;iz++) {
		isg.z = -sgnnz(dir[iz]); ix = inc_mod3[iz]; iy = dec_mod3[iz];
		t.set(center[iz]*-isg.z+(size[iz]+r), fabs_tpl(dir[iz]));
		bContact = isneg(fabs_tpl(center[ix]*t.y-dir[ix]*t.x)-size[ix]*t.y) & isneg(fabs_tpl(center[iy]*t.y-dir[iy]*t.x)-size[iy]*t.y);
		bBest = isneg(tmax-t) & bContact;
		UPDATE_IDBEST(tmax,iz<<1|isg.z+1>>1);
	}

	// box edge - sphere
	for(iz=0;iz<3;iz++) {
		ix = inc_mod3[iz]; iy = dec_mod3[iz];
		for(i=0;i<4;i++) {
			isg.x = (i<<1&2)-1; isg.y = (i&2)-1;
			pt[ix]=size[ix]*isg.x; pt[iy]=size[iy]*isg.y; pt[iz]=0;
			vec0 = cross_with_ort(center-pt,iz); vec1 = cross_with_ort(-dir,iz);
			ka=vec1*vec1; kb=vec0*vec1; kc=vec0*vec0-r*r; kd=kb*kb-ka*kc;
			if (isnonneg(kd) & (isneg(kb)|isneg(kb*kb-kd))) {
				t.set(-kb+sqrt_tpl(kd), ka);
				bContact = isneg(fabs_tpl(center[iz]*t.y-dir[iz]*t.x)-size[iz]*t.y);
				bBest = isneg(tmax-t) & bContact;
				UPDATE_IDBEST(tmax,0x10 | iz<<2|isg.y+1|isg.x+1>>1);
			}
		}
	}

	// box vertex - sphere
	for(i=0;i<8;i++) {
		isg.x=(i<<1&2)-1; isg.y=(i&2)-1; isg.z=(i>>1&2)-1;
		pt(size.x*isg.x, size.y*isg.y, size.z*isg.z);
		vec0=center-pt; vec1=-dir;
		ka=vec1*vec1; kb=vec0*vec1; kc=vec0*vec0-r*r; kd=kb*kb-ka*kc;
		if (isnonneg(kd) & (isneg(kb)|isneg(kb*kb-kd))) {
			t.set(-kb+sqrt_tpl(kd), ka);
			bBest = isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x20 | isg.z+1<<1|isg.y+1|isg.x+1>>1);
		}
	}

	if (idbest==-1)
		return 0;

	pcontact->t = tmax.val();
	switch (idbest&0xF0) {
		case 0x00: // box face - sphere
			iz=idbest>>1; isg.z=(idbest<<1&2)-1;
			pcontact->n = pbox->Basis.GetRow(iz)*isg.z;
			pcontact->iFeature[0] = 0x40 | iz<<1|isg.z+1>>1;
			break;
		case 0x10: // box edge - sphere
			iz=idbest>>2&3; ix=inc_mod3[iz]; iy=dec_mod3[iz]; isg.y=(idbest&2)-1; isg.x=(idbest<<1&2)-1;
			pt[ix]=size[ix]*isg.x; pt[iy]=size[iy]*isg.y; pt[iz]=center[iz]-dir[iz]*pcontact->t;
			pcontact->n = (center-dir*pcontact->t-pt).normalized()*pbox->Basis;
			pcontact->iFeature[0] = 0x20 | idbest&0xF;
			break;
		default: // box vertex - sphere
			isg.z=(idbest>>1&2)-1; isg.y=(idbest&2)-1; isg.x=(idbest<<1&2)-1;
			pt(size.x*isg.x, size.y*isg.y, size.z*isg.z);
			pcontact->n = (center-dir*pcontact->t-pt).normalized()*pbox->Basis;
			pcontact->iFeature[0] = idbest&0xF;
	}
	pcontact->pt = psph->center-pcontact->n*r;

	return 1;
}

int sphere_box_lin_unprojection(unprojection_mode *pmode, const sphere *psph,int iFeature1, const box *pbox,int iFeature2, 
																contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  box_sphere_lin_unprojection(pmode,pbox,iFeature2,psph,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}

int box_capsule_lin_unprojection(unprojection_mode *pmode, const box *pbox,int iFeature1, const capsule *pcaps,int iFeature2, 
																 contact *pcontact, geom_contact_area *parea)
{
	float tmin0 = pmode->tmin;
	real tcyl;
	int bContact0,bContact1,bContact2;
	sphere sph;
	pcontact->t = 0;
	#define check_cap(prim,pprim,iend) \
		sph.center = pcaps->center+pcaps->axis*pcaps->hh*(iend*2-3); sph.r = pcaps->r;	pmode->dir.zero(); \
		if (bContact##iend = prim##_sphere_lin_unprojection(pmode, pprim,iFeature1, &sph,-1, &capcont,parea))	\
			if (capcont.n*pcaps->axis*(iend*2-3)*pcaps->hh > pcaps->hh*0.1f) {	\
				pmode->dir = pcaps->axis*(3-iend*2); \
				if (bContact##iend = prim##_sphere_lin_unprojection(pmode, pprim,iFeature1, &sph,-1, &capcont,parea) && (!pcontact->t || capcont.t<pcontact->t))	\
					*pcontact = capcont, dir_best = pmode->dir;	\
			}	else if (bContact##iend = capcont.t>pcontact->t)	\
				*pcontact = capcont, dir_best = pmode->dir;

	if (pmode->dir.len2()==0) {
		Vec3 dir_best(ZERO);
		contact capcont; capcont.taux=0;
		if (bContact0 = box_cylinder_lin_unprojection(pmode, pbox,iFeature1, pcaps,0x43, pcontact,parea))
			dir_best = pmode->dir;
		else
			pcontact->t = 0;
		tcyl = pcontact->t;
		check_cap(box,pbox,1)
		check_cap(box,pbox,2)
		pmode->dir = dir_best;
	}	else {
		bContact0 = box_cylinder_lin_unprojection(pmode, pbox,iFeature1, pcaps,0x43, pcontact,parea);
		tcyl = pcontact->t;
		pmode->tmin += (pcontact->t-pmode->tmin)*bContact0;
		sph.center = pcaps->center-pcaps->axis*pcaps->hh; sph.r = pcaps->r;
		bContact1 = box_sphere_lin_unprojection(pmode, pbox,iFeature1, &sph,-1, pcontact,parea);
		pmode->tmin += (pcontact->t-pmode->tmin)*bContact1; pcontact->iFeature[1] += bContact1;
		sph.center = pcaps->center+pcaps->axis*pcaps->hh;
		bContact2 = box_sphere_lin_unprojection(pmode, pbox,iFeature1, &sph,-1, pcontact,parea);
		pcontact->iFeature[1] += bContact2*2;
		pmode->tmin = tmin0;
	}
	if (parea && fabs_tpl(pcontact->t-tcyl)>pcaps->r*0.02f)
		parea->npt = 0;

	return bContact0|bContact1|bContact2;
}

int capsule_box_lin_unprojection(unprojection_mode *pmode, const capsule *pcaps,int iFeature1, const box *pbox,int iFeature2, 
																 contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  box_capsule_lin_unprojection(pmode,pbox,iFeature2,pcaps,iFeature1,pcontact,parea);
	if (res) 
		flip_contact(pmode,pcontact,parea);
	pmode->dir.Flip();
	return res;
}

int cylinder_sphere_lin_unprojection(unprojection_mode *pmode, const cylinder *pcyl,int iFeature1, const sphere *psph,int iFeature2,
																		 contact *pcontact, geom_contact_area *parea)
{
	Vec3r axis,center,dir,vec0,vec1,c,pt2d;
	real rs=psph->r,rc=pcyl->r,hh=pcyl->hh,ka,kb,kc,kd,roots[4];
	int i,icap,bContact,bBest,idbest=-1,bCapped=iszero(iFeature1-0x43)^1;
	quotient t,tmax(pmode->tmin,1);
	axis = pcyl->axis;
	center = psph->center-pcyl->center;
	dir = pmode->dir;

	pcontact->iFeature[1] = 0x40;
	if (pmode->dir.len2()==0) {	// minimum unprojection dir requested
		real h,r2d,dist[2];
		int bOutside[2];
		h = center*axis; r2d = (center-axis*h).len();
		if (r2d<(real)1e-8)
			center = pcyl->axis.GetOrthogonal().normalized()*(r2d=pcyl->r*0.5f) + axis*h;
		bOutside[0] = isneg(dist[0] = hh-fabs_tpl(h));
		bOutside[1] = isneg(dist[1] = rc-r2d);
		if (bOutside[0]+bOutside[1]==0)
			bOutside[1] = (bOutside[0] = isneg(dist[0]-dist[1]))^1;
		switch (bOutside[0]|bOutside[1]<<1) {
			case 1: // unproject to cap
				icap = sgnnz(h); pmode->dir = axis*-icap; pcontact->t = rs+dist[0];
				pcontact->iFeature[0] = 0x41+(icap+1>>1); break;
			case 2: // unproject to side
				pmode->dir = (-center+axis*h)/r2d; pcontact->t=rs+dist[1];
				pcontact->iFeature[0] = 0x40; break;
			case 3: // unproject to cap edge
				icap = sgnnz(h); pmode->dir = axis*dist[0]*icap+(center-axis*h)*(dist[1]/r2d); 
				pcontact->t=pmode->dir.len(); pmode->dir/=pcontact->t; pcontact->t=rs-pcontact->t;
				pcontact->iFeature[0] = 0x20|icap+1>>1;
		}
		pcontact->n = -pmode->dir;
		pcontact->pt = psph->center-pcontact->n*psph->r;
		return 1;
	}

	// sphere - cyl.side
	vec1 = -dir^axis; vec0 = center^axis;
	ka=vec1*vec1; kb=vec0*vec1; kc=vec0*vec0-sqr(rc+rs); kd=kb*kb-ka*kc;
	if (isnonneg(kd) & (isneg(kb)|isneg(kb*kb-kd))) {
		t.set(-kb+sqrt_tpl(kd), ka);
		bContact = isneg(fabs_tpl((center*t.y-dir*t.x)*axis)-hh*t.y) & isneg(t.x-pmode->tmax*t.y);
		bBest = isneg(tmax-t) & bContact;
		UPDATE_IDBEST(tmax,0x10);
	}

	if (bCapped) {
		// sphere - cyl.cap face
		icap = -sgnnz(dir*axis);
		t.set((rs+hh)-(center*axis)*icap, fabs_tpl(dir*axis));
		bContact = isneg((center*t.y-dir*t.x^axis).len2()-sqr(rc*t.y)) & isneg(t.x-pmode->tmax*t.y);
		bBest = isneg(tmax-t) & bContact;
		UPDATE_IDBEST(tmax,icap+1>>1);

		// sphere - cyl.cap edge
		for(icap=-1; icap<=1; icap+=2) {
			c = center-axis*(hh*icap);
			polynomial_tpl<real,4> p = (P2(-(dir*dir))+P1(2*(c*dir))+rc*rc+rs*rs-c*c).sqr()-
				(P2(-sqr(dir*axis))+P1(2*(c*axis)*(dir*axis))+rs*rs-sqr(c*axis))*(4*rc*rc);
			for(i=p.findroots(0,pmode->tmax,roots)-1;i>=0;i--) {
				t.set(roots[i],1);
				bContact = isneg(rs*rs+rc*rc-(c-dir*t.x).len2()); // checks for phantom root
				bBest = isneg(tmax-t) & bContact;
				UPDATE_IDBEST(tmax,0x20|icap+1>>1);
	/*if (bBest) {
	prim_inters inters;
	int cylinder_sphere_intersection(const cylinder *pcyl, const sphere *psphere, prim_inters *pinters);
	if (!cylinder_sphere_intersection(pcyl,psph,&inters))
	bBest = 0;
	}*/
			}
	/*real ht,r2dt,distt,tt;
	for(i=p.findroots(-4,4,roots,20)-1;i>=0;i--) {
	ht = (c-dir*roots[i])*axis;
	r2dt = sqrt_tpl(rs*rs-ht*ht);
	distt = (c-dir*roots[i]).len2();
	tt = p.eval(roots[i]);
	}
	i = p.nroots(-4,4);*/
		}
	}

	if (idbest<0)
		return 0;

	pcontact->t = tmax.val();
	icap = (idbest<<1&2)-1;
	switch (idbest&0xF0) {
		case 0x00: // sphere - cyl.cap face
			pcontact->n = pcyl->axis*icap; pcontact->iFeature[0] = 0x40|(icap+1>>1)+1; break;
		case 0x10: // sphere - cyl.side
			pcontact->n = center-dir*pcontact->t; (pcontact->n -= axis*(axis*pcontact->n)).normalize();
			pcontact->iFeature[0] = 0x40; break;
		case 0x20: // sphere - cyl.cap edge
			c = center-axis*(hh*icap)-dir*pcontact->t; pt2d = (c-axis*(c*axis)).normalized()*rc; 
			pcontact->n = (c-pt2d).normalized(); pcontact->iFeature[0] = 0x20|icap+1>>1;
	}
	pcontact->pt = psph->center-pcontact->n*psph->r;
	return 1;
}

int sphere_cylinder_lin_unprojection(unprojection_mode *pmode, const sphere *psph,int iFeature1, const cylinder *pcyl,int iFeature2, 
																		 contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  cylinder_sphere_lin_unprojection(pmode,pcyl,iFeature2,psph,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}

int sphere_sphere_lin_unprojection(unprojection_mode *pmode, const sphere *psph1,int iFeature1, const sphere *psph2,int iFeature2, 
																	 contact *pcontact, geom_contact_area *parea)
{
	Vec3r dc,vec0,vec1;
	real ka,kb,kc,kd,dclen,t;
	dc = psph1->center-psph2->center;

	pcontact->iFeature[0] = pcontact->iFeature[1] = 0x40;
	if (pmode->dir.len2()==0) {	// minimum unprojection dir requested
		dclen = dc.len();
		pmode->dir = dclen>0 ? dc/dclen : Vec3r(0.0,0.0,1.0); 
		pcontact->t = psph1->r+psph2->r-dclen;
		pcontact->n = -pmode->dir;
		pcontact->pt = psph2->center-pcontact->n*psph2->r;
		return 1;
	}

	vec1 = pmode->dir; vec0 = dc;
	ka=vec1*vec1; kb=vec0*vec1; kc=vec0*vec0-sqr(psph1->r+psph2->r); kd=kb*kb-ka*kc;
	if (isnonneg(kd) & (isneg(kb)|isneg(kb*kb-kd))) {
		t = (-kb+sqrt_tpl(kd))/ka;
		if (t>pmode->tmin) {
			pcontact->t = t;
			pcontact->n = (psph2->center-(psph1->center+pmode->dir*pcontact->t)).normalized();
			pcontact->pt = psph2->center-pcontact->n*psph2->r;
			return 1;
		}
	}

	return 0;
}

int cyl_cyl_lin_unprojection(unprojection_mode *pmode, const cylinder *pcyl1,int iFeature1, const cylinder *pcyl2,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	const cylinder *pcyl[2] = { pcyl1,pcyl2 };
	Vec3r axis[2],center[2],dir,ptc,dp,dc,n,axisx,axisy,u,l,a1,dir_best(ZERO);
	Vec2r c2d,dir2d,pt2d[4],ptbest2d;
	quotient tmax(0,1),t,t0,t1;
	real cosa,sina,rsina=0,a,b,c,d,roots[4],nlen,denom,r0=0,r1,dist,tbound[2];
	int i,j,j1,bContact,bBest,icyl,nroots,idbest=-1,iter,bSeparated,bFindMinUnproj,bNoContact=0,bCapped[2];
	Vec3 ncap[2];
	float ncaplen[2] = {0};
	bCapped[0] = iszero(iFeature1-0x43)^1;
	bCapped[1] = iszero(iFeature2-0x43)^1;

	if (bFindMinUnproj = (pmode->dir.len2()<0.5f)) 
		tmax.x = pmode->tmax;
	cosa = pcyl[0]->axis*pcyl[1]->axis; 
	sina = (axisx = pcyl[0]->axis^pcyl[1]->axis).len();	
	dc = pcyl[1]->center - pcyl[0]->center;
	axisx = sina>0.001f ? (Vec3r)(axisx*(rsina=1/sina)) : (Vec3r)(dc-pcyl[0]->axis*(pcyl[0]->axis*dc)).normalized();

	// cap edge - cap face
	if (bCapped[0]&bCapped[1]) for(icyl=0;icyl<2;icyl++) { // icyl provides cap edge, icyl^1 - cap face
		if (bFindMinUnproj)
			dir = axis[1] = pcyl[icyl^1]->axis*sgnnz(pcyl[icyl^1]->axis*(pcyl[icyl]->center-pcyl[icyl^1]->center));
		else {
			axis[1] = pcyl[icyl^1]->axis*(sgnnz(pcyl[icyl^1]->axis*pmode->dir)*(1-icyl*2));
			dir = pmode->dir;
		}
		dir *= 1-icyl*2;
		if (fabs_tpl(a = pcyl[icyl]->axis*axis[1]) < 0.001f)
			a = pcyl[icyl]->axis*(pcyl[icyl]->center-pcyl[icyl^1]->center);
		axis[0] = pcyl[icyl]->axis*-sgnnz(a);
		center[0] = axis[0]*pcyl[icyl]->hh + pcyl[icyl]->center;
		center[1] = axis[1]*pcyl[icyl^1]->hh + pcyl[icyl^1]->center;
		n = axis[0]*(axis[0]*axis[1]) - axis[1];
		if (n.len2()>sqr((real)0.01))
			nlen = n.len();
		else { // in case of degenerate n (co-axis cylinders) mask it to point to center, not to plane
			n = center[1]-center[0]; n -= pcyl[icyl]->axis*(n*pcyl[icyl]->axis);
			if (n.len2()>0.0001f)
				nlen = n.len();
			else {
				n.zero(); nlen=1;
			}
		}
		ncap[icyl] = n; ncaplen[icyl] = nlen;
		ptc = center[0]*nlen + n*pcyl[icyl]->r;
		t.set((center[1]*nlen-ptc)*axis[1], (axis[1]*dir)*(1-icyl*2)*nlen).fixsign();
		ptc = ptc*t.y + dir*(t.x*(1-icyl*2)*nlen);
		bContact = isneg((ptc - center[1]*(nlen*t.y)).len2() - sqr(pcyl[1]->r*nlen*t.y));
		if (bFindMinUnproj) {
			bBest = isneg(t-tmax) & isneg(min(pcyl[0]->r,pcyl[1]->r)*-0.002f-t.x);
			dir_best = dir_best*(bBest^1) + dir*bBest;
			bNoContact = bNoContact&(bBest^1) | (bContact^1)&bBest;
		}	else
			bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,icyl);
	}

	// cap edge - side
	ptbest2d.zero();
	for(icyl=0; icyl<2; icyl++) if (bCapped[icyl]) { // icyl provides edge, icyl^1 - side
		axisy=pcyl[icyl^1]->axis^axisx;	r0=pcyl[icyl]->r; r1=pcyl[icyl^1]->r; dir=pmode->dir*(icyl*2-1);
		for(i=-1; i<=1; i+=2) {
			dc = pcyl[icyl^1]->center - pcyl[icyl]->center - pcyl[icyl]->axis*(pcyl[icyl]->hh*i);
			c2d.set(dc*axisx, dc*axisy); 

			if (bFindMinUnproj) {
				if (sina>0.005f) {
					if (c2d.y*i*(1-icyl*2)<0)
						continue;
				} else if (dc.len2()>(dc+pcyl[icyl]->axis*(pcyl[icyl]->hh*i*2)).len2())
					continue;
				Vec2i sg(sgnnz(c2d.x),sgnnz(c2d.y));
				pt2d[2].set(r0*sg.x, r0*sg.y*fabs_tpl(cosa));
				pt2d[3].set(sg.x*fabs_tpl(cosa), sg.y);
				j1 = 1-sg.x*sg.y>>1;
				int idx[2] = {0,SINCOSTABSZ};
				do {
					iter = idx[0]+idx[1]>>1;
					pt2d[0].set(g_costab[iter]*pt2d[2].x, g_sintab[iter]*pt2d[2].y); // point on ellipse
					pt2d[1].set(g_costab[iter]*pt2d[3].x, g_sintab[iter]*pt2d[3].y); // ellipse normal
					idx[isneg(pt2d[1]^c2d-pt2d[0])^j1] = iter;
				} while(idx[1]-idx[0]>1);

				dist = len2(pt2d[1] = pt2d[0]-c2d);
				// Dot product of pt2d[1] and pt2d[0] must be inlined to avoid triggering compiler bug in VS2015
				if ((pt2d[1].x*pt2d[0].x+pt2d[1].y*pt2d[0].y)<0 && dist<sqr(r1) && r1-(dist=sqrt_tpl(dist))<tmax) {
					ptbest2d.set(pt2d[0].x, g_sintab[iter]*sg.y*r0);
					dp = dc-axisx*pt2d[0].x-(pcyl[icyl]->axis^axisx)*(ptbest2d.y*sgnnz(cosa));
					bNoContact = isneg(pcyl[icyl^1]->hh-fabs_tpl(dp*pcyl[icyl^1]->axis));
					tmax = r1-dist;
					dp -= pcyl[icyl^1]->axis*(dp*pcyl[icyl^1]->axis);
					dir_best = dp.normalized()*(icyl*2-1);
					idbest = 0x40 | icyl*2 | i+1>>1;
				}
			} else {
				int bBoundTouched = 0;
				dir2d.set(dir*axisx, dir*axisy);

				if (pmode->tmax<min(pcyl[0]->r,pcyl[1]->r) && fabs_tpl(cosa)>0.003f || len2(dir2d)<sqr(0.005f)) {
					tbound[0] = 0;
					tbound[1] = pmode->tmax;
				} else {
					if (fabs_tpl(cosa)>0.003f) {
						// find t0,t1 where the circle enters the inner and outer boundaries respectively
						denom = 1/sqrt_tpl(sqr(dir2d.y)+sqr(dir2d.x*cosa));	// pt2d[0] - dir-ellipse touch point
						pt2d[0].set(dir2d.y,-dir2d.x*sqr(cosa)) *= (denom*r0); pt2d[0] *= sgnnz((pt2d[0]^dir2d)*(c2d^dir2d));
						pt2d[1] = dir2d*(denom*r0*fabs_tpl(cosa)); // pt2d[1] - dir-ellipse intersection point
						denom = 1/sqrt_tpl(sqr(dir2d.x)+sqr(dir2d.y*cosa));	// pt2d[2] - dir.rot90cw()-ellipse touch point
						pt2d[2].set(dir2d.x,dir2d.y*sqr(cosa)) *= (denom*r0); pt2d[2] *= sgnnz(dir2d*pt2d[2]);
						denom = 1/len2(dir2d);
						pt2d[2] = dir2d*((dir2d*pt2d[2])*denom);
						pt2d[3] = dir2d.rot90cw()*((dir2d.rot90cw()*pt2d[0])*denom)+pt2d[2];
						j = 0;
					} else {
						pt2d[2].set(-r0,0); pt2d[3].set(r0,0);
						j = 1;
					}
					
					for(; j<2; j++) {
						t0.set(0,1);
						// circle-line touch point (additionally check for contact)
						Vec2r l2d = pt2d[j*2+1]-pt2d[j*2];
						t.set(r1*len(l2d)-(c2d-pt2d[j*2]^l2d)*sgn(dir2d^l2d), fabs_tpl(dir2d^l2d));
						if (inrange(((c2d-pt2d[j*2])*t.y+dir2d*t.x)*l2d, (real)0,len2(l2d)*t.y) && t>t0)
							if (t<pmode->tmax) {
								t0 = t; bBoundTouched = 1;
							} else
								t0 = pmode->tmax;
						// circle-line ends touch points
						for(j1=0; j1<2; j1++) {
							a = len2(dir2d); b = (c2d-pt2d[j*2+j1])*dir2d; c = len2(c2d-pt2d[j*2+j1])-r1*r1; d = b*b-a*c;
							denom = a*t0.x+b*t0.y;
							if (d>0 && (denom<0 || d*sqr(t0.y)>sqr(denom))) {
								denom = a*pmode->tmax+b;
								if (denom>0 && d<sqr_signed(a*pmode->tmax+b))	{
									t0.set(-b+sqrt_tpl(d),a); bBoundTouched = 1;
								} else
									t0.set(pmode->tmax,1);
							}
						}
						tbound[j] = (t0.y==1 ? t0.x : t0.x/t0.y);
					}
				}

				if (fabs_tpl(cosa)>0.003f) {
					real a0=r0*r0*(1+sqr(cosa))+r1*r1, b0=sqr(r0*r0*cosa)+sqr(r1*r0)*(1+sqr(cosa)), cx2,cy2,p,q,Q,Q0,Q1; 
					#define calcQ \
						cx2 = sqr(c2d.x+dir2d.x*t.x); cy2 = sqr(c2d.y+dir2d.y*t.x);	\
						a = a0-cx2-cy2; b = b0-sqr(r0)*(cx2*sqr(cosa)+cy2);	\
						p = b-a*a*(1.0f/3); q = a*(b*(1.0f/6)-a*a*(1.0f/27))-c*0.5f; \
						Q = cube(p)*(1.0f/27)+sqr(q)
					c = sqr(r1*r0*r0*cosa);
					// check Q signs on both ends, if they are the same - skip the test
					t.x = tbound[0]; calcQ; Q0 = Q;
					t.x = tbound[1]; calcQ; Q1 = Q;

					if (Q0*Q1<0) {
						iter=0; do {
							t.x = (tbound[0]+tbound[1])*0.5f; 
							calcQ;
							tbound[isneg(Q)] = t.x;
						} while(++iter<16);
					} else
						continue;
				} else {
					if (!bBoundTouched)
						continue;
					tbound[0] = tbound[1];
				}

				t.set((tbound[0]+tbound[1])*0.5f,1);
				c2d += dir2d*t.x;
				Vec2i sg(sgnnz(c2d.x),sgnnz(c2d.y));
				pt2d[2].set(r0*sg.x, r0*sg.y*fabs_tpl(cosa));
				pt2d[3].set(sg.x*sqr(cosa), sg.y);
				j1 = 1-sg.x*sg.y>>1;
				int idx[2] = {0,SINCOSTABSZ};
				do {
					iter = idx[0]+idx[1]>>1;
					pt2d[0].set(g_costab[iter]*pt2d[2].x, g_sintab[iter]*pt2d[2].y); // point on ellipse
					pt2d[1].set(g_costab[iter]*pt2d[3].x, g_sintab[iter]*pt2d[3].y); // ellipse normal
					idx[isneg(pt2d[1]^c2d-pt2d[0])^j1] = iter;
				} while(idx[1]-idx[0]>1);
				pt2d[0].set(g_costab[idx[0]]*sg.x, g_sintab[idx[0]]*sg.y) *= r0;

				dp = dc+dir*t.x-axisx*pt2d[0].x-(pcyl[icyl]->axis^axisx)*(pt2d[0].y*sgnnz(cosa));
				bContact = isneg(fabs_tpl(dp*pcyl[icyl^1]->axis) - pcyl[icyl^1]->hh);
				bBest = bContact & isneg(tmax-t);
				UPDATE_IDBEST(tmax,0x40 | icyl*2 | i+1>>1);
				ptbest2d = ptbest2d*(bBest^1) + pt2d[0]*bBest;
			}
		}
	}

	// side - side
	dc = pcyl2->center-pcyl1->center;
	if (bFindMinUnproj)
		dir = axisx*-sgnnz(dc*axisx);
	else dir = pmode->dir;
	t.set((dc*axisx)*sgnnz(dir*axisx)-pcyl[0]->r-pcyl[1]->r, fabs_tpl(dir*axisx));
	for(i=0; i<2; i++,t.x+=(pcyl[0]->r+pcyl[1]->r)*2) {
		dp = dc*t.y-dir*t.x;
		t0.set((dp^pcyl2->axis)*axisx, t.y);
		t1.set((dp^pcyl1->axis)*axisx, t.y);
		bContact = isneg(fabs_tpl(t0.x)-t0.y*pcyl1->hh*sina) & isneg(fabs_tpl(t1.x)-t1.y*pcyl2->hh*sina);
		if (bFindMinUnproj) {
			bBest = isneg(-t.x) & isneg(t-tmax);
			dir_best = dir_best*(bBest^1) + dir*bBest;
			bNoContact = bNoContact&(bBest^1) | (bContact^1)&bBest;
		} else
			bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,0xC0);
	}

	// cap edge - cap edge
	if (sina*(bCapped[0]&bCapped[1])>0.001f) {
		// real edge-edge min unprojection will require iterative solving, so instead use the best unprojection dir found so far
		dir = bFindMinUnproj ? (bNoContact ? (Vec3r)(pcyl1->center-pcyl2->center).normalized() : (Vec3r)dir_best) : (Vec3r)pmode->dir;
		u = (pcyl[0]->axis*cosa-pcyl[1]->axis)*rsina;	
		l = pcyl[0]->axis^u; a1 = pcyl[1]->axis*rsina; 
		r0 = pcyl[0]->r; r1 = pcyl[1]->r;
		for(i=-1; i<=1; i+=2) {
			axis[0] = pcyl[0]->axis*i; center[0] = pcyl[0]->center + axis[0]*pcyl[0]->hh;
			for(j=-1; j<=1; j+=2) {
				axis[1] = pcyl[1]->axis*j; center[1] = pcyl[1]->center + axis[1]*pcyl[1]->hh;
				dc = center[0]-center[1];
				polynomial_tpl<real,1> udc = P1(u*dir)+u*dc, a1dc = P1(a1*dir)+a1*dc, ldc = P1(l*dir)+l*dc;
				polynomial_tpl<real,2> dc2 = P2(dir.len2())+P1(dir*dc)*2+dc*dc;
				polynomial_tpl<real,4> colls = (r0*r0-a1dc.sqr())*ldc.sqr()*4 - (udc*a1dc*2+dc2+r0*r0-r1*r1).sqr();
				if (colls.nroots(0,pmode->tmax) && (nroots = colls.findroots(0,pmode->tmax,roots))) {
					assert(nroots<=4);
					if (bFindMinUnproj) {
						for(nroots--; nroots>=0; nroots--) {
							t = roots[nroots]; dp = dc+dir*t.x;
							bSeparated = isneg(sqr(r1)-(dp+u*(a1*dp)).len2());
							bSeparated &= isneg(pcyl[1]->hh*0.999f-fabs_tpl((center[0]+dir*t.x-pcyl[1]->center)*pcyl[1]->axis));
							if (bSeparated)
								break;
						}
						bBest = (bNoContact | isneg(t-tmax)) & bSeparated;
						dir_best = dir_best*(bBest^1) + dir*bBest;
						bNoContact &= bBest^1;
						UPDATE_IDBEST(tmax,0x80 | i+1 | j+1>>1);
					} else if (nroots) {
						t = roots[nroots-1];
						bBest = isneg(tmax-t);
						UPDATE_IDBEST(tmax,0x80 | i+1 | j+1>>1);
					}
				}
			}
		}
	}

	if (idbest==-1 || bNoContact && (!(bCapped[0]&bCapped[1]) || tmax>min(pcyl[0]->r,pcyl[1]->r)*0.1f))
		return 0;
	if (bFindMinUnproj)
		pmode->dir = dir_best.normalized();

	switch (idbest & 0xC0) {
		case 0: // cap edge - cap face
			icyl = idbest;
			axis[1] = pcyl[icyl^1]->axis*(sgnnz(pcyl[icyl^1]->axis*pmode->dir)*(1-icyl*2));
			if (fabs_tpl(a = pcyl[icyl]->axis*axis[1]) < 0.01f)
				a = pcyl[icyl]->axis*(pcyl[icyl]->center-pcyl[icyl^1]->center);
			axis[0] = pcyl[icyl]->axis*-sgnnz(a);
			center[0] = axis[0]*pcyl[icyl]->hh + pcyl[icyl]->center;
			pcontact->t = tmax.val();
			pcontact->pt = center[0] + ncap[icyl]*(pcyl[icyl]->r/ncaplen[icyl]) + pmode->dir*(pcontact->t*(icyl^1));
			pcontact->n = axis[1]*(icyl*2-1);
			pcontact->iFeature[icyl] = 0x20 | isnonneg(pcyl[icyl]->axis*axis[0]);
			pcontact->iFeature[icyl^1] = 0x40 | isnonneg(pcyl[icyl^1]->axis*axis[1])+1;
			break;

		case 0x40: // cap edge - side
			icyl = idbest>>1&1; i = (idbest*2&2)-1;
			pcontact->t = tmax.val();
			pcontact->pt = pcyl[icyl]->center + pcyl[icyl]->axis*(pcyl[icyl]->hh*i) + pmode->dir*(pcontact->t*(1-icyl)) +
										 axisx*ptbest2d.x + (pcyl[icyl]->axis^axisx)*(ptbest2d.y*sgnnz(cosa));
			pcontact->n = ((pcyl[icyl^1]->center+pmode->dir*(pcontact->t*icyl))-pcontact->pt)*(1-icyl*2);
			pcontact->n -= pcyl[icyl^1]->axis*(pcontact->n*pcyl[icyl^1]->axis);
			pcontact->iFeature[icyl] = 0x20 | i+1>>1;
			pcontact->iFeature[icyl^1] = 0x40;
			break;

		case 0x80: // cap edge - cap edge
			i = (idbest&2)-1; j = (idbest*2&2)-1;
			pcontact->t = tmax.val();
			center[0] = pcyl[0]->center + pcyl[0]->axis*(pcyl[0]->hh*i) + pmode->dir*pcontact->t;
			center[1] = pcyl[1]->center + pcyl[1]->axis*(pcyl[1]->hh*j);
			a = a1*(center[0]-center[1]);
			pcontact->pt = center[0] + u*a;
			pcontact->pt += l*(sqrt_tpl(fabs_tpl(r0*r0-a*a))*sgnnz(l*(center[1]-pcontact->pt)));
			pcontact->n = (pcyl[0]->axis ^ pcontact->pt-center[0]) ^ (pcyl[1]->axis ^ pcontact->pt-center[1]);
			pcontact->n *= sgnnz(pcontact->n*(pcyl[1]->center-pcyl[0]->center-pmode->dir*pcontact->t));
			pcontact->iFeature[0] = 0x20 | i+1>>1;
			pcontact->iFeature[1] = 0x20 | j+1>>1;
			break;

		case 0xC0: // side - side
			pcontact->t = tmax.val();
			dp = pcyl2->center-pcyl1->center - pmode->dir*pcontact->t;
			t1.set((dp^pcyl1->axis)*axisx, (real)1);//sina);
			//pcontact->t = tmax.x*t0.y;
			pcontact->pt = pcyl2->center + pcyl2->axis*t1.x;//val();
			pcontact->n = axisx*sgnnz(axisx*dp);
			pcontact->pt -= pcontact->n*pcyl2->r;
			pcontact->iFeature[0] = pcontact->iFeature[1] = 0x40;
			break;
	}

	// make sure contact point lies on primitives (there might be false hits if they were initially separated)
	/*a = pmode->minPtDist*30.0f;
	for(icyl=0,bContact=1; icyl<2; icyl++) {
		dp = pcontact->pt-pcyl[icyl]->center-pmode->dir*(pcontact->t*(1-icyl)); nlen = dp*pcyl[icyl]->axis;
		bContact &= inrange(max(-a*pcyl[icyl]->r,(dp-pcyl[icyl]->axis*nlen).len2()-sqr(pcyl[icyl]->r)) +
												max(-a*pcyl[icyl]->r,(fabs_tpl(nlen)-pcyl[icyl]->hh)*pcyl[icyl]->r), a*-1.99f*pcyl[icyl]->r, a*2.0f*pcyl[icyl]->r);
	}
	if (!bContact)
		return 0;*/

	if (parea) {
		parea->npt = 0;
		dc = pcyl[0]->center+pmode->dir*pcontact->t-pcyl[1]->center;
		if (sqr(sina) < (1-pmode->maxcos)*2) {
			parea->n1 = -pcontact->n;
			if (fabs_tpl(fabs_tpl(dc*pcyl[0]->axis)-(pcyl[0]->hh+pcyl[1]->hh)) < min(pcyl[0]->hh,pcyl[1]->hh)*0.05f*(bCapped[0]&bCapped[1]))	{	
				// check for cap-cap area contact
				i = -sgnnz(pcyl[0]->axis*dc); j = sgnnz(pcyl[1]->axis*dc);
				center[0] = pcyl[0]->center+pmode->dir*pcontact->t+pcyl[0]->axis*(pcyl[0]->hh*i);
				center[1] = pcyl[1]->center+pcyl[1]->axis*(pcyl[1]->hh*j);
				for(icyl=0; icyl<2; icyl++) {
					axisy = pcyl[icyl]->axis^axisx;
					for(j1=0,pt2d[0].set(pcyl[icyl]->r,0); j1<8; j1++) {
						parea->pt[parea->npt] = center[icyl]+axisx*pt2d[0].x+axisy*pt2d[0].y;
						parea->piFeature[icyl^1][parea->npt] = 0x40 | ((i&-icyl | j&~-icyl)+1>>1)+1;
						parea->piFeature[icyl][parea->npt] = 0x20 | (i&~-icyl | j&-icyl)+1>>1;
						if (icyl)
							parea->pt[parea->npt] -= pcyl[0]->axis*(pcyl[0]->axis*(parea->pt[parea->npt]-center[0]));
						parea->npt += isneg((parea->pt[parea->npt]-center[icyl^1]).len2()-sqr(pcyl[icyl^1]->r*1.01f));
						a=pt2d[0].x; pt2d[0].x=(pt2d[0].x-pt2d[0].y)*(sqrt2/2); pt2d[0].y=(a+pt2d[0].y)*(sqrt2/2); // rotate by 45 degrees
					}
				}
				// add circle intersection points
				d = (center[1]-center[0]).len2();
				if (inrange(d, sqr(pcyl[0]->r-pcyl[1]->r)*(real)1.001f, sqr(pcyl[0]->r+pcyl[1]->r)*(real)0.999f)) {
					b = 1.0/(d = sqrt_tpl(d));
					a = ((sqr(pcyl[0]->r)-sqr(pcyl[1]->r))*b+d)*0.5f;
					c = sqrt_tpl(sqr(pcyl[0]->r)-a*a);
					u = (center[1]-center[0])*b; l = pcyl[0]->axis^u;
					for(j1=-1; j1<=1; j1+=2) {
						parea->pt[parea->npt] = center[0]+u*a+l*(c*j1);
						parea->piFeature[0][parea->npt] = 0x20 | i+1>>1;
						parea->piFeature[1][parea->npt] = 0x20 | j+1>>1;
						parea->npt++;
					}
				}
				if (parea->npt>1) {
					pcontact->n = pcyl[0]->axis*i;
					parea->n1 = pcyl[1]->axis*j;
					goto gotareacc;
				}
				parea->npt = 0;
			}

			if (fabs_tpl((dc-pcyl[0]->axis*(dc*pcyl[0]->axis)).len2()-sqr(pcyl[0]->r+pcyl[1]->r)) < min(pcyl[0]->r,pcyl[1]->r)*0.05f) {	
				// check for side-side edge contact
				dp = (pcyl[0]->axis*(dc*pcyl[0]->axis)-dc).normalized();
				for(icyl=0; icyl<2; icyl++) for(i=-1; i<=1; i+=2)
				if (fabs_tpl((dc*(1-icyl*2)+pcyl[icyl]->axis*(pcyl[icyl]->hh*i))*pcyl[icyl^1]->axis) < pcyl[icyl^1]->hh*1.002f) {
					parea->pt[parea->npt] = pcyl[icyl]->center+pmode->dir*(pcontact->t*(1-icyl))+pcyl[icyl]->axis*(pcyl[icyl]->hh*i)+
						dp*(pcyl[icyl]->r*(1-icyl*2));
					parea->piFeature[icyl^1][parea->npt] = 0x40;
					parea->piFeature[icyl][parea->npt] = 0x20 | i+1>>1;
					parea->npt++;	
				}
				if (parea->npt)
					goto gotareacc;
			}
		}

		if (sqr(cosa)<(1-pmode->maxcos)*2) for(icyl=0,dc.Flip(); icyl<2; icyl++,dc.Flip()) 
		if (bCapped[icyl] & inrange(fabs_tpl(dc*pcyl[icyl]->axis), (pcyl[icyl]->hh+pcyl[icyl^1]->r)*(real)0.99, (pcyl[icyl]->hh+pcyl[icyl^1]->r)*(real)1.01)) { 
			// check for icyl^1 side - icyl cap contact
			a=1-sqr(cosa); b=dc*pcyl[icyl^1]->axis-(dc*pcyl[icyl]->axis)*cosa; 
			c=dc.len2()-sqr(pcyl[icyl]->axis*dc)-sqr(pcyl[icyl]->r); d=b*b-a*c;
			if (d>0) {
				i = sgnnz(dc*pcyl[icyl]->axis);
				j = sgnnz(d-b*b);
				if (d*j < sqr_signed(pcyl[icyl^1]->hh*a+fabs_tpl(b)*j))	{
					d = sqrt_tpl(d); a = 1/a;
					for(j=-1; j<=1; j+=2) if (fabs_tpl((-b+d*j)*a) < pcyl[icyl^1]->hh) {
						parea->pt[parea->npt] = pcyl[icyl^1]->axis*((-b+d*j)*a);
						parea->piFeature[icyl^1][parea->npt] = 0x20 | i;
						parea->piFeature[icyl][parea->npt++] = 0x40 | (j+1>>1)+1;
					}
				}
				for(j=-1; j<=1; j+=2) if ((dc+pcyl[icyl^1]->axis*(pcyl[icyl^1]->hh*j)^pcyl[icyl]->axis).len2()<sqr(pcyl[icyl]->r)) {
					parea->pt[parea->npt] = pcyl[icyl^1]->axis*(pcyl[icyl^1]->hh*j);
					parea->piFeature[icyl^1][parea->npt] = 0x40;
					parea->piFeature[icyl][parea->npt++] = 0x20 | j+1>>1;
				}
				for(j=0; j<parea->npt; j++) {
					dp = pcyl[icyl]->axis*(pcyl[icyl]->axis*(dc+parea->pt[j])-pcyl[icyl]->hh*i);
					parea->pt[j] += pcyl[icyl^1]->center+pmode->dir*(pcontact->t*icyl)-dp;
				}
				if (parea->npt>1) {
					Vec3 ns[2];
					ns[icyl] = pcyl[icyl]->axis*sgnnz(pcyl[icyl]->axis*(parea->pt[0]-pcyl[icyl]->center-pmode->dir*(pcontact->t*(1-icyl))));
					ns[icyl^1] = pcyl[icyl^1]->axis*sgnnz(pcyl[icyl^1]->axis*(parea->pt[0]-pcyl[icyl^1]->center-pmode->dir*(pcontact->t*icyl)));;
					ns[icyl^1] = pcyl[icyl^1]->axis^(pcyl[icyl^1]->axis^ns[icyl]);
					ns[icyl^1] = ns[icyl^1].len2()>0.001f ? ns[icyl^1].normalized() : -ns[icyl];
					pcontact->n = ns[0]; parea->n1 = ns[1];
					if (icyl) {
						for(i=0;i<parea->npt;i++)
							parea->pt[i] += pcontact->n*(pcontact->n*(pcontact->pt-parea->pt[i]));
					}
					break;
				}
				parea->npt = 0;
			}
		}

		gotareacc:;
	}

	return 1;
}

int cylinder_capsule_lin_unprojection(unprojection_mode *pmode, const cylinder *pcyl,int iFeature1, const capsule *pcaps,int iFeature2, 
																			contact *pcontact, geom_contact_area *parea)
{
	float tmin0 = pmode->tmin;
	real tcyl;
	int bContact0,bContact1,bContact2;
	sphere sph;
	pcontact->t = 0;

	if (pmode->dir.len2()==0) {
		Vec3 dir_best(ZERO);
		contact capcont;
		if (bContact0 = cyl_cyl_lin_unprojection(pmode, pcyl,iFeature1, pcaps,0x43, pcontact,parea))
			dir_best = pmode->dir;
		tcyl = pcontact->t;
		check_cap(cylinder,pcyl,1)
		check_cap(cylinder,pcyl,2)
		pmode->dir = dir_best;
	}	else {
		bContact0 = cyl_cyl_lin_unprojection(pmode, pcyl,iFeature1, pcaps,0x43, pcontact,parea);
		tcyl = pcontact->t;
		pmode->tmin += (pcontact->t-pmode->tmin)*bContact0;
		sph.center = pcaps->center-pcaps->axis*pcaps->hh; sph.r = pcaps->r;
		bContact1 = cylinder_sphere_lin_unprojection(pmode, pcyl,iFeature1, &sph,-1, pcontact,parea);
		pmode->tmin += (pcontact->t-pmode->tmin)*bContact1; pcontact->iFeature[1] += bContact1;
		sph.center = pcaps->center+pcaps->axis*pcaps->hh;
		bContact2 = cylinder_sphere_lin_unprojection(pmode, pcyl,iFeature1, &sph,-1, pcontact,parea);
		pcontact->iFeature[1] += bContact2*2;
		pmode->tmin = tmin0;
	}
	if (parea && fabs_tpl(tcyl-pcontact->t)>pcaps->r*0.02f)
		parea->npt = 0;

	return bContact0|bContact1|bContact2;
}

int capsule_cylinder_lin_unprojection(unprojection_mode *pmode, const capsule *pcaps,int iFeature1, const cylinder *pcyl,int iFeature2, 
																			contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  cylinder_capsule_lin_unprojection(pmode,pcyl,iFeature2,pcaps,iFeature1,pcontact,parea);
	if (res) 
		flip_contact(pmode,pcontact,parea);
	pmode->dir.Flip();
	return res;
}

int capsule_capsule_lin_unprojection(unprojection_mode *pmode, const capsule *pcaps1,int iFeature1, const capsule *pcaps2,int iFeature2, 
																		 contact *pcontact, geom_contact_area *parea)
{
	float tmin0 = pmode->tmin;
	int bContact,bContactSph,i,icaps;
	sphere sph,sph1;
	const capsule *pcaps[2] = { pcaps1,pcaps2 };
	pcontact->t = 0;

	if (pmode->dir.len2()==0) {
		Vec3 dir_best(ZERO);
		contact capcont;
		if (bContact = cyl_cyl_lin_unprojection(pmode, pcaps1,0x43, pcaps2,0x43, pcontact,parea))
			dir_best = pmode->dir;
		for(icaps=0;icaps<2;icaps++) for(i=-1;i<=1;i+=2) {
			sph.center = pcaps[icaps]->center+pcaps[icaps]->axis*(pcaps[icaps]->hh*i); sph.r = pcaps[icaps]->r;	pmode->dir.zero();
			if (cylinder_sphere_lin_unprojection(pmode, pcaps[icaps^1],0x43, &sph,-1, &capcont,parea) && capcont.t>pcontact->t) 
			{ if (icaps==0)
					capcont.pt-=pmode->dir*capcont.t, capcont.n.Flip(), pmode->dir.Flip();
				*pcontact = capcont; dir_best = pmode->dir;	bContact = 1;
			}
			sph1.center = pcaps[icaps^1]->center+pcaps[icaps^1]->axis*(pcaps[icaps^1]->hh*i*(1-icaps*2));	sph1.r = pcaps[icaps^1]->r;	pmode->dir.zero();
			if (sphere_sphere_lin_unprojection(pmode, &sph1,-1, &sph,-1, &capcont,parea) &&
					min(capcont.t-pcontact->t, (real)(capcont.n*pcaps[icaps]->axis)*-i)>0) 
			{ if (icaps==0)
					capcont.pt-=pmode->dir*capcont.t, capcont.n.Flip(), pmode->dir.Flip();
				*pcontact = capcont; dir_best = pmode->dir; bContact = 1;
			}
		}
		pmode->dir = dir_best;
	}	else {
		bContact = cyl_cyl_lin_unprojection(pmode, pcaps1,0x43, pcaps2,0x43, pcontact,parea);
		pmode->tmin += (pcontact->t-pmode->tmin)*bContact;
		for(pmode->dir.Flip(),icaps=0; icaps<2; icaps++,pmode->dir.Flip()) for(i=-1;i<=1;i+=2) {
			sph.center = pcaps[icaps]->center+pcaps[icaps]->axis*(pcaps[icaps]->hh*i); sph.r = pcaps[icaps]->r;
			bContact |= (bContactSph = cylinder_sphere_lin_unprojection(pmode, pcaps[icaps^1],0x43, &sph,-1, pcontact,parea));
			pmode->tmin += (pcontact->t-pmode->tmin)*bContactSph;
			if (bContactSph & (icaps^1))
				pcontact->pt-=pmode->dir*pcontact->t, pcontact->n.Flip();
			sph1.center = pcaps[icaps^1]->center+pcaps[icaps^1]->axis*(pcaps[icaps^1]->hh*i*(1-icaps*2));	sph1.r = pcaps[icaps^1]->r;
			bContact |= (bContactSph = sphere_sphere_lin_unprojection(pmode, &sph1,-1, &sph,-1, pcontact,parea));
			pmode->tmin += (pcontact->t-pmode->tmin)*bContactSph;
			if (bContactSph & (icaps^1))
				pcontact->pt-=pmode->dir*pcontact->t, pcontact->n.Flip();
		}
		pmode->dir.Flip();
		pmode->tmin = tmin0;
	}
	if (parea)
		parea->npt = 0;

	return bContact;
}

int sphere_capsule_lin_unprojection(unprojection_mode *pmode, const sphere *psph,int iFeature1, const capsule *pcaps,int iFeature2, 
																		contact *pcontact, geom_contact_area *parea)
{
	float tmin0 = pmode->tmin;
	real tcyl;
	int bContact0,bContact1,bContact2;
	sphere sph;
	pcontact->t = 0;

	if (pmode->dir.len2()==0) {
		Vec3 dir_best(ZERO);
		contact capcont;
		if (bContact0 = sphere_cylinder_lin_unprojection(pmode, psph,iFeature1, pcaps,0x43, pcontact,parea))
			dir_best = pmode->dir;
		tcyl = pcontact->t;
		sph.center = pcaps->center-pcaps->axis*pcaps->hh; sph.r = pcaps->r;	pmode->dir.zero();
		if ((bContact1 = sphere_sphere_lin_unprojection(pmode, psph,-1, &sph,-1, &capcont,parea)) && 
			  (capcont.t>pcontact->t || capcont.n*pcaps->axis>0))
			*pcontact = capcont, dir_best = pmode->dir;
		sph.center = pcaps->center+pcaps->axis*pcaps->hh;	pmode->dir.zero();
		if ((bContact2 = sphere_sphere_lin_unprojection(pmode, psph,-1, &sph,-1, &capcont,parea)) && 
				(capcont.t>pcontact->t || capcont.n*pcaps->axis<0))
			*pcontact = capcont, dir_best = pmode->dir;
		pmode->dir = dir_best;
	}	else {
		bContact0 = sphere_cylinder_lin_unprojection(pmode, psph,iFeature1, pcaps,0x43, pcontact,parea);
		tcyl = pcontact->t;
		pmode->tmin += (pcontact->t-pmode->tmin)*bContact0;
		sph.center = pcaps->center-pcaps->axis*pcaps->hh; sph.r = pcaps->r;
		bContact1 = sphere_sphere_lin_unprojection(pmode, psph,-1, &sph,-1, pcontact,parea);
		pmode->tmin += (pcontact->t-pmode->tmin)*bContact1; pcontact->iFeature[1] += bContact1;
		sph.center = pcaps->center+pcaps->axis*pcaps->hh;
		bContact2 = sphere_sphere_lin_unprojection(pmode, psph,-1, &sph,-1, pcontact,parea);
		pcontact->iFeature[1] += bContact2*2;
		pmode->tmin = tmin0;
	}
	if (parea && fabs_tpl(tcyl-pcontact->t)>pcaps->r*0.02f)
		parea->npt = 0;

	return bContact0|bContact1|bContact2;
}

int capsule_sphere_lin_unprojection(unprojection_mode *pmode, const capsule *pcaps,int iFeature1, const sphere *psph,int iFeature2, 
																		contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  sphere_capsule_lin_unprojection(pmode,psph,iFeature2,pcaps,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
		if (parea) {
			for(int i=0;i<parea->npt;i++) {
				iFeature=parea->piFeature[0][i]; parea->piFeature[0][i]=parea->piFeature[1][i]; parea->piFeature[1][i]=iFeature;
			}
			parea->n1.Flip();
		}
	}
	pmode->dir.Flip();
	return res;
}


int ray_box_lin_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const box *pbox,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	Vec3r pt[2],dir,edge0,ptbox,dp,ncontact,dp_x_edge1;
	Vec3i sgdir;
	int i,j,idir,idbest=-1,bContact,bBest,ix,iy,isgx,isgy,isg;
	quotient t,tmax(0,1),t0,t1;
	real dist,mindist;

	pt[0] = pbox->Basis*(pray->origin-pbox->center);
	pt[1] = pbox->Basis*(pray->origin+pray->dir-pbox->center);
	dir = pbox->Basis*pmode->dir;
	sgdir.x=sgnnz(dir.x); sgdir.y=sgnnz(dir.y); sgdir.z=sgnnz(dir.z);

	// ray end - box face
	for(j=0;j<3;j++) { // j-box face (with sign in direction of movement)
		i=0; mindist = pt[0][j]*sgdir[j]; // find triangle vertex that is the farthest from corresponding box face (along its inside normal)
		dist = pt[1][j]*sgdir[j]; i += isneg(dist-mindist); mindist = min(dist,mindist);

		ix = inc_mod3[j]; iy = dec_mod3[j];
		t.set(pbox->size[j]-pt[i][j]*sgdir[j], dir[j]*sgdir[j]); // check if contact point belongs to box face
		bContact = isneg(fabs_tpl(pt[i][ix]*t.y+dir[ix]*t.x)-pbox->size[ix]*t.y) & isneg(fabs_tpl(pt[i][iy]*t.y+dir[iy]*t.x)-pbox->size[iy]*t.y);
		bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,0x40 | (i<<2 | j));
	}

	// ray - box edge
	edge0 = pt[1]-pt[0];
	for(idir=0;idir<3;idir++) {
		ix = inc_mod3[idir]; iy = dec_mod3[idir];
		ncontact[idir]=0; ncontact[ix]=edge0[iy]; ncontact[iy]=-edge0[ix];
		if (ncontact.len2() > (real)1E-8*edge0.len2()) {
			t.y = dir*ncontact; t.y *= isg = sgnnz(t.y);
			isgx = sgnnz(ncontact[ix])*isg; isgy = sgnnz(ncontact[iy])*isg;
			ptbox[idir]=-pbox->size[idir]; ptbox[ix]=pbox->size[ix]*isgx; ptbox[iy]=pbox->size[iy]*isgy;
			dp = ptbox-pt[0]; t.x = dp*ncontact*isg; dp = dp*t.y-dir*t.x;
			dp_x_edge1[idir]=0; dp_x_edge1[ix]=dp[iy]; dp_x_edge1[iy]=-dp[ix];
			t0.set(dp_x_edge1*ncontact, ncontact.len2()*t.y); // check if t0 is in (0..1) and t1 is in (0..2*size)
			t1.set((dp^edge0)*ncontact, t0.y);
			bContact = t0.isin01() & isneg(fabs_tpl(t1.x-t1.y*pbox->size[idir])-t1.y*pbox->size[idir]);
			bBest = bContact & isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x80 | (idir<<2 | isgy+1 | isgx+1>>1));
		}
	}

	if (idbest==-1)
		return 0;

	switch (idbest & 0xC0) {	
		case 0x40: // ray end - box face
			i = idbest>>2&3; j = idbest&3;
			pcontact->t = tmax.val();
			pcontact->pt = pray->origin + pray->dir*i + pmode->dir*pcontact->t;
			pcontact->n = pbox->Basis.GetRow(j)*-sgdir[j];
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40 | j<<1 | sgdir[j]+1>>1;
			break;

		default: // ray - box edge
			idir=idbest>>2&3; isgy=(idbest&2)-1; isgx=((idbest&1)<<1)-1;
			edge0 = pt[1]-pt[0];
			ix = inc_mod3[idir]; iy = dec_mod3[idir];
			ncontact[idir]=0; ncontact[ix]=edge0[iy]; ncontact[iy]=-edge0[ix];
			isg = sgnnz(dir*ncontact);
			isgx = sgnnz(ncontact[ix])*isg; isgy = sgnnz(ncontact[iy])*isg;
			ptbox[idir]=-pbox->size[idir]; ptbox[ix]=pbox->size[ix]*isgx; ptbox[iy]=pbox->size[iy]*isgy;
			dp = (ptbox-pt[0])*tmax.y-dir*tmax.x;
			dp_x_edge1[idir]=0; dp_x_edge1[ix]=dp[iy]; dp_x_edge1[iy]=-dp[ix];
			t0.set(dp_x_edge1*ncontact, ncontact.len2()*tmax.y); t0.y = 1.0/t0.y;
			pcontact->t = tmax.x*t0.y*ncontact.len2();
			pcontact->pt = pray->origin + pray->dir*t0.x*t0.y + pmode->dir*pcontact->t;
			pcontact->n = ncontact*pbox->Basis;
			pcontact->n *= sgnnz((pbox->center-pcontact->pt)*pcontact->n);
			pcontact->iFeature[0] = 0xA0 | i;
			pcontact->iFeature[1] = 0x20 | idbest&0xF;
	}

	return 1;
}

int box_ray_lin_unprojection(unprojection_mode *pmode, const box *pbox,int iFeature1, const ray *pray,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  ray_box_lin_unprojection(pmode,pray,iFeature2,pbox,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}


int ray_cylinder_lin_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const cylinder *pcyl,int iFeature2, 
																	contact *pcontact, geom_contact_area *parea)
{
	Vec3r n,center,dp,edge,vec0,vec1,ptcyl;
	quotient t,t0,t1,tmax(0,1);
	real nlen2,a,b,c,d,r2=sqr(pcyl->r),dist,mindist;
	int i,j,idbest=-1,bContact,bBest,bCapped=iszero(iFeature2-0x43)^1;

	// ray ends - cylinder side
	for(i=0;i<2;i++) {
		dp = pray->origin+pray->dir*i-pcyl->center;
		vec0 = dp ^ pcyl->axis; vec1 = pmode->dir^pcyl->axis;
		a = vec1*vec1; b = vec1*vec0; c = vec0*vec0-r2; d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); t.set(-b+d,a);
			bContact = isneg(fabs_tpl((dp*t.y+pmode->dir*t.x)*pcyl->axis) - t.y*pcyl->hh);
			bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
			UPDATE_IDBEST(tmax,0x40 | i);
		}
	}

	// ray - cylinder side
	edge = pray->dir;
	dp = pray->origin-pcyl->center; n = edge^pcyl->axis; nlen2 = n.len2();
	c = dp*n; a = pmode->dir*n; b = a*c; a*=a; c=c*c-r2*nlen2; d = b*b-a*c;
	if (d>=0) {
		d = sqrt_tpl(d); t.set(-b+d,a);
		dp = (pcyl->center-pray->origin)*t.y-pmode->dir*t.x;
		t0.set((dp^pcyl->axis)*n, nlen2*t.y);
		t1.set((dp^edge)*n, nlen2*t.y);
		bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y) & isneg(fabs_tpl(t1.x)-t1.y*pcyl->hh);
		bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
		UPDATE_IDBEST(tmax,0x60);
	}

	if (bCapped) {
		// ray ends - cylinder cap faces
		j = (isnonneg(pcyl->axis*pmode->dir)<<1)-1; // choose cap that lies farther along unprojection direction
		i=0; mindist = (pray->origin*pcyl->axis)*j; // find ray end that is the closest to the cap face
		dist = ((pray->origin+pray->dir)*pcyl->axis)*j; i += isneg(dist-mindist); mindist = min(mindist,dist);

		center = pcyl->center + pcyl->axis*(pcyl->hh*j);
		t.set((center-pray->origin-pray->dir*i)*pcyl->axis, pmode->dir*pcyl->axis).fixsign();
		bContact = isneg(((pray->origin+pray->dir*i)*t.y+pmode->dir*t.x-center*t.y).len2()-r2*t.y*t.y);
		bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
		UPDATE_IDBEST(tmax,0x20 | (i<<1 | j+1>>1));

		// ray - cylinder cap edges
		for(j=-1;j<=1;j+=2) {
			center = pcyl->center+pcyl->axis*pcyl->hh*j;
			dp = pray->origin-center;
			vec0 = pcyl->axis^(dp^edge); vec1 = pcyl->axis^(pmode->dir^edge);
			a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2*sqr(edge*pcyl->axis); d = b*b-a*c;
			if (d>=0) {
				d = sqrt_tpl(d); t.set(-b+d,a);
				t0.set(-((dp*t.y+pmode->dir*t.x)*pcyl->axis), (edge*pcyl->axis)*t.y);
				bContact = isneg(fabs_tpl(t0.x*2-t0.y)-fabs_tpl(t0.y));
				bBest = bContact & isneg(tmax-t) & isneg(t.x-pmode->tmax*t.y);
				UPDATE_IDBEST(tmax,0x80 | j+1>>1);
			}
		}

		// we also need to process degenerate cases when te ray lies in cap plane (edge-cap edge check doesn't detect them)
		if (sqr(edge*pcyl->axis)<edge.len2()*(real)1E-4) { // potential ray - cylinder cap contact
			j = sgnnz(pcyl->axis*pmode->dir);
			center = pcyl->center+pcyl->axis*pcyl->hh*j; dp = center-pray->origin;	
			t.set((dp*pcyl->axis)*edge.len2()-(edge*pcyl->axis)*(edge*dp), // calculate when the closest point on edge will touch the cap
				(pmode->dir*pcyl->axis)*edge.len2()-(edge*pcyl->axis)*(edge*pmode->dir)).fixsign();
			if (t>tmax && t<pmode->tmax) {
				pcontact->t = t.val();
				t0.set(edge*(dp-pmode->dir*pcontact->t), edge.len2());
				pcontact->pt = pray->origin + pmode->dir*pcontact->t;
				if (fabs_tpl(t0.x*2-t0.y)<t0.y && (edge*t0.x+(pcontact->pt-center)*t0.y).len2()<r2*sqr(t0.y))
					pcontact->pt += edge*(t0.val());
				else if ((pcontact->pt+edge-center).len2()<r2)
					pcontact->pt += edge;
				else if ((pcontact->pt-center).len2()<=r2) {
					pcontact->n = pcyl->axis*-j;
					pcontact->iFeature[0] = 0xA0;
					pcontact->iFeature[1] = 0x40 | (j+1>>1)+1;
					return 1;
				}
			}
		}
	}

	if (idbest==-1 || fabs_tpl(tmax.y)<(real)1E-20)
		return 0;

	switch (idbest & 0xE0) { 
		case 0x20: // ray end - cylinder cap face
			i = idbest>>1&3; j = idbest&1;
			pcontact->t = tmax.val();
			pcontact->pt = pray->origin+pray->dir*i + pmode->dir*pcontact->t;
			pcontact->n = pcyl->axis*(1-(j<<1));
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40 | j+1;
			break;

		case 0x40: // ray end - cylinder side
			i = idbest & 3;
			pcontact->t = tmax.val();
			pcontact->pt = pray->origin+pray->dir*i + pmode->dir*pcontact->t;
			pcontact->n = pcyl->center-pcontact->pt;
			pcontact->n -= pcyl->axis*(pcyl->axis*pcontact->n);
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40;
			break;

		case 0x60: // ray - cylinder side
			dp = pcyl->center-pray->origin; n = edge^pcyl->axis; nlen2 = n.len2();
			dp = dp*tmax.y-pmode->dir*tmax.x;
			t0.set((dp^pcyl->axis)*n, nlen2*tmax.y); t0.y = 1.0/t0.y;
			pcontact->t = tmax.x*t0.y*nlen2;
			pcontact->pt = pray->origin + edge*t0.x*t0.y + pmode->dir*pcontact->t;
			pcontact->n = pcyl->center-pcontact->pt;
			pcontact->n -= pcyl->axis*(pcyl->axis*pcontact->n);
			pcontact->iFeature[0] = 0xA0;
			pcontact->iFeature[1] = 0x40;
			break;

		default: // ray - cylinder cap edge
			j = idbest&1;
			center = pcyl->center+pcyl->axis*pcyl->hh*((j<<1)-1);
			dp = pray->origin-center;
			t0.set(-((dp*tmax.y+pmode->dir*tmax.x)*pcyl->axis), (edge*pcyl->axis)*tmax.y); t0.y = 1.0/t0.y;
			pcontact->t = tmax.x*t0.y*(edge*pcyl->axis);
			pcontact->pt = pray->origin + edge*t0.x*t0.y + pmode->dir*pcontact->t;
			pcontact->n = pcontact->pt-pcyl->center;
			pcontact->n -= pcyl->axis*(pcyl->axis*pcontact->n);
			pcontact->n = edge^(pcyl->axis^pcontact->n);
			pcontact->n *= sgnnz((pcyl->center-pcontact->pt)*pcontact->n);
			pcontact->iFeature[0] = 0xA0;
			pcontact->iFeature[1] = 0x20 | j;
	}

	return 1;
}

int cylinder_ray_lin_unprojection(unprojection_mode *pmode, const cylinder *pcyl,int iFeature1, const ray *pray,int iFeature2, 
																	contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  ray_cylinder_lin_unprojection(pmode,pray,iFeature2,pcyl,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}


int ray_sphere_lin_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const sphere *psphere,int iFeature2, 
																contact *pcontact, geom_contact_area *parea)
{
	quotient t,tmax(pmode->tmin,1),t0;
	Vec3r center,edge,vec0,vec1,dp;
	real a,b,c,d,r2=sqr(psphere->r);
	int i,bBest,bContact,idbest=-1;

	// ray ends - sphere
	for(i=0;i<2;i++) {
		vec0 = pray->origin+pray->dir*i-psphere->center; vec1 = pmode->dir;
		a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2; d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); t.set(-b-d,a);	bBest = isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x40 | i);
			t.set(-b+d,a); bBest = isneg(tmax-t);
			UPDATE_IDBEST(tmax,0x40 | i);
		}
	}

	// ray - sphere
	edge = pray->dir; dp = pray->origin-psphere->center;
	vec0 = edge ^ dp; vec1 = edge ^ pmode->dir;
	a = vec1*vec1; b = vec0*vec1; c = vec0*vec0-r2*edge.len2(); d = b*b-a*c;
	if (d>=0) {
		d = sqrt_tpl(d); t.set(-b-d,a); 
		t0.set(((psphere->center-pray->origin)*t.y-pmode->dir*t.x)*edge, t.y*edge.len2());
		bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y); bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,0x80 | i);
		t.x += d*2;	t0.set(((psphere->center-pray->origin)*t.y-pmode->dir*t.x)*edge, t.y*edge.len2());
		bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y); bBest = bContact & isneg(tmax-t);
		UPDATE_IDBEST(tmax,0x80 | i);
	}

	if (idbest==-1)
		return 0;

	switch (idbest & 0xC0) {
		case 0x40:
			i = idbest&3;
			pcontact->t = tmax.val();
			pcontact->pt = pray->origin+pray->dir*i+pmode->dir*pcontact->t;
			pcontact->n = psphere->center-pcontact->pt;
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40;
			break;

		case 0x80:
			t0.set(((psphere->center-pray->origin)*tmax.y-pmode->dir*tmax.x)*edge, tmax.y*edge.len2()); t0.y = 1.0/t0.y;
			pcontact->t = tmax.x*t0.y*edge.len2();
			pcontact->pt = pray->origin + edge*(t0.x*t0.y) + pmode->dir*pcontact->t;
			pcontact->n = psphere->center-pcontact->pt;
			pcontact->iFeature[0] = 0xA0 | i;
			pcontact->iFeature[1] = 0x40;
	}

	return 1;
}

int sphere_ray_lin_unprojection(unprojection_mode *pmode, const sphere *psphere,int iFeature1, const ray *pray,int iFeature2, 
																contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  ray_sphere_lin_unprojection(pmode,pray,iFeature2,psphere,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t;	pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}


int ray_capsule_lin_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const capsule *pcaps,int iFeature2, 
																 contact *pcontact, geom_contact_area *parea)
{
	float tmin0 = pmode->tmin;
	int bContact0,bContact1,bContact2;
	sphere sph;
	pcontact->t = 0;

	bContact0 = ray_cylinder_lin_unprojection(pmode, pray,iFeature1, pcaps,0x43, pcontact,parea);
	pmode->tmin += (pcontact->t-pmode->tmin)*bContact0;

	sph.center = pcaps->center-pcaps->axis*pcaps->hh; sph.r = pcaps->r;
	bContact1 = ray_sphere_lin_unprojection(pmode, pray,iFeature1, &sph,-1, pcontact,parea);
	pmode->tmin += (pcontact->t-pmode->tmin)*bContact1; pcontact->iFeature[1] += bContact1;

	sph.center = pcaps->center+pcaps->axis*pcaps->hh;
	bContact2 = ray_sphere_lin_unprojection(pmode, pray,iFeature1, &sph,-1, pcontact,parea);
	pcontact->iFeature[1] += bContact2*2;
	pmode->tmin = tmin0;

	return bContact0|bContact1|bContact2;
}

int capsule_ray_lin_unprojection(unprojection_mode *pmode, const capsule *pcaps,int iFeature1, const ray *pray,int iFeature2, 
																 contact *pcontact, geom_contact_area *parea)
{
	pmode->dir.Flip();
	int res =  ray_capsule_lin_unprojection(pmode,pray,iFeature2,pcaps,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt -= pmode->dir*pcontact->t; pcontact->n.Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	pmode->dir.Flip();
	return res;
}

#undef UPDATE_IDBEST
