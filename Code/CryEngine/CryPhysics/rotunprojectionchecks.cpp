// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "unprojectionchecks.h"

#define UPDATE_IDBEST(tbest,newid) \
	(idbest&=~-bBest) |= (newid)&-bBest; \
	(tbest.x*=bBest^1) += tsin.x*bBest;	\
	(tbest.y*=bBest^1) += tsin.y*bBest


int tri_tri_rot_unprojection(unprojection_mode *pmode, const triangle *ptri1,int iFeature1, const triangle *ptri2,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	const triangle *ptri[2] = { ptri1,ptri2 };
	Vec3r pt,ptz[2][3],ptx[2][3],pty[2][3],rotax,pt0,pt1,edge0,edge1,n0z,n0x,n0y,ptz0,ptx0,pty0,n,pt0_rot,
		pt1_rot,edge0_rot,ptz1,ptx1,pty1,dir0,dir1,norm[2];
	quotient tsin,tcos,tmin(pmode->tmax,1),t0,t1;
	real kcos,ksin,k,a,b,c,d,sg0,sg1;
	int itri,i,j,j1,bContact,bBest,idbest=-1,imask=iFeature2<<8 | iFeature1;

	// compute components for vec.rot(t) = vecz + vecx*cos(t) + vecy*sin(t)
	for(itri=0,rotax=pmode->dir;itri<2;itri++,rotax*=-1) for(i=0;i<3;i++) {
		pt = ptri[itri]->pt[i]-pmode->center;
		ptz[itri][i] = rotax*(pt*rotax); ptx[itri][i] = pt-ptz[itri][i]; pty[itri][i] = rotax^ptx[itri][i];
	}

	// vertex-face contacts, (p[i].rot(t)-origin)*n = 0; -t for the 2nd triangle
	for(itri=0;itri<2;itri++) {for(i=0;i<3;i++) {if ((0x40<<(itri<<3) | (0x80|i)<<((itri^1)<<3))!=imask) {
		kcos = ptx[itri^1][i]*ptri[itri]->n; ksin = pty[itri^1][i]*ptri[itri]->n; 
		k = (ptz[itri^1][i]-ptri[itri]->pt[0]+pmode->center)*ptri[itri]->n;
		a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); tsin.set(-b-d,a);
			for(j=0;j<2;j++,tsin.x+=d*2) 
			if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
					isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1) (=eliminate foreign root)
			{
				tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
				pt = ptx[itri^1][i]*tcos.x + pty[itri^1][i]*tsin.x + (ptz[itri^1][i]+pmode->center)*tsin.y;
				bContact = // point is inside itri
					isneg((pt-ptri[itri]->pt[0]*tsin.y ^ ptri[itri]->pt[1]-ptri[itri]->pt[0])*ptri[itri]->n) & 
					isneg((pt-ptri[itri]->pt[1]*tsin.y ^ ptri[itri]->pt[2]-ptri[itri]->pt[1])*ptri[itri]->n) & 
					isneg((pt-ptri[itri]->pt[2]*tsin.y ^ ptri[itri]->pt[0]-ptri[itri]->pt[2])*ptri[itri]->n);
				// additionally check that triangles don't intersect during contact
				pt = ptx[itri^1][inc_mod3[i]]*tcos.x + pty[itri^1][inc_mod3[i]]*tsin.x + (ptz[itri^1][inc_mod3[i]] + 
					pmode->center - ptri[itri]->pt[0])*tsin.y;
				sg0 = pt*ptri[itri]->n + pmode->minPtDist*tsin.y;
				pt = ptx[itri^1][dec_mod3[i]]*tcos.x + pty[itri^1][dec_mod3[i]]*tsin.x + (ptz[itri^1][dec_mod3[i]] + 
					pmode->center - ptri[itri]->pt[0])*tsin.y;
				sg1 = pt*ptri[itri]->n + pmode->minPtDist*tsin.y;	
				bContact &= isnonneg(sg0*sg1);
				bBest = bContact & isneg(tsin-tmin);
				UPDATE_IDBEST(tmin,itri<<2 | i);
			}
		}
	} else { // check if triangles are already separated in their initial state
		sg0 = (ptri[itri^1]->pt[inc_mod3[i]]-ptri[itri]->pt[0])*ptri[itri]->n + pmode->minPtDist;
		sg1 = (ptri[itri^1]->pt[dec_mod3[i]]-ptri[itri]->pt[0])*ptri[itri]->n + pmode->minPtDist;	
		bBest = isnonneg(sg0) & isnonneg(sg1); tsin.set(0,1);
		UPDATE_IDBEST(tmin,itri<<2 | i);	
	} } }

	rotax = pmode->dir;
	n0z = rotax*(ptri[0]->n*rotax); n0x = ptri[0]->n-n0z; n0y = rotax^n0x;
	// edge-edge contacts
	// (p0.rot(t)-p1)*(l0.rot(t)^l1) = 0
	// p0.rot(t)*(l0.rot(t)^l1) - p1*(l0.rot(t)^l1) = 0;
	// l1*(p0^l0).rot(t) + l0.rot(t)*(p1^l1) = 0;
	// l1*(p0^l0).rot(t) + l0*(p1^l1).rot(-t) = 0; - this one's just to get symmetrical form
	// note that p.rot(t)^l.rot(t)==(p^l).rot(t) only if p is relative to the center of rotation
	for(i=0;i<3;i++) {
		pt0 = ptri[0]->pt[i]-pmode->center;
		edge0 = ptri[0]->pt[inc_mod3[i]]-ptri[0]->pt[i];
		pt = pt0^edge0; ptz0 = rotax*(pt*rotax); ptx0 = pt-ptz0; pty0 = rotax^ptx0;
		for(j=0;j<3;j++) if ((0xA0|i | (0xA0|j)<<8)!=imask) {
			pt1 = ptri[1]->pt[j]-pmode->center;
			edge1 = ptri[1]->pt[inc_mod3[j]]-ptri[1]->pt[j];
			pt = pt1^edge1; ptz1 = rotax*(pt*rotax); ptx1 = pt-ptz1; pty1 = rotax^ptx1;
			kcos = edge1*ptx0+edge0*ptx1; ksin = edge1*pty0-edge0*pty1; k = edge1*ptz0+edge0*ptz1;
			a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
			if (d>=0) {
				d = sqrt_tpl(d); tsin.set(-b-d,a);
				for(j1=0;j1<2;j1++,tsin.x+=d*2) 
				if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
						isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1) (=eliminate foreign root)
				{
					tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
					pt0_rot = ptx[0][i]*tcos.x + pty[0][i]*tsin.x + ptz[0][i]*tsin.y;
					pt1_rot = ptx[0][inc_mod3[i]]*tcos.x + pty[0][inc_mod3[i]]*tsin.x + ptz[0][inc_mod3[i]]*tsin.y;
					edge0_rot = pt1_rot-pt0_rot;
					n = edge0_rot^edge1; pt = pt1*tsin.y-pt0_rot;
					t0.set((pt^edge1)*n, n.len2());
					t1.set((pt^edge0_rot)*n, t0.y*tsin.y);
					bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y) & isneg(fabs_tpl(t1.x*2-t1.y)-t1.y);
					// additionally check that triangles don't intersect during contact
					dir0 = (n0z*tsin.y+n0x*tcos.x+n0y*tsin.x)^edge0_rot;
					dir1 = ptri[1]->n^edge1;
					bContact &= isneg((dir0*n)*(dir1*n));
					bBest = bContact & isneg(tsin-tmin);
					UPDATE_IDBEST(tmin,i<<2 | j | 0x80);
				}
			}
		} else { // check if triangles are already separated in their initial state
			edge1 = ptri[1]->pt[inc_mod3[j]]-ptri[1]->pt[j]; n = edge0^edge1;
			dir0 = ptri[0]->n^edge0; dir1 = ptri[1]->n^edge1;
			bBest = isneg((dir0*n)*(dir1*n));	tsin.set(0,1);
			// additionally check that edges intersection point will not move inside the 2nd triangle during unprojection
			bBest &= isneg(dir1*(pmode->dir ^ (ptri[0]->pt[i]-pmode->center)*n.len2() + edge0*((ptri[1]->pt[j]-ptri[0]->pt[i]^edge1)*n)));
			UPDATE_IDBEST(tmin,i<<2 | j | 0x80);
		}
	}

	if (idbest==-1)
		return 0;

	if (idbest & 0x80) {
		i = idbest>>2 & 3; j = idbest & 3;
		pcontact->t = tmin.val();
		pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
		pt0_rot = ptx[0][i]*pcontact->taux + pty[0][i]*pcontact->t + ptz[0][i] + pmode->center;
		pt1_rot = ptx[0][inc_mod3[i]]*pcontact->taux + pty[0][inc_mod3[i]]*pcontact->t + ptz[0][inc_mod3[i]] + pmode->center;
		edge0_rot = pt1_rot-pt0_rot;
		edge1 = ptri[1]->pt[inc_mod3[j]]-ptri[1]->pt[j];
		pt = ptri[1]->pt[j]-pt0_rot; n = edge0_rot ^ edge1;
		t0.set((pt^edge1)*n, n.len2());
		pcontact->pt = pt0_rot + edge0_rot*t0.val();
		pcontact->n = n*sgnnz((ptri2->n^edge1)*n);
		pcontact->iFeature[0] = 0xA0 | i;
		pcontact->iFeature[1] = 0xA0 | j;
	} else {
		itri = idbest>>2; i = idbest & 3;
		pcontact->t = tmin.val();
		pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
		pcontact->pt = ptz[itri^1][i] + ptx[itri^1][i]*max(pcontact->taux,(real)(itri^1)) + pty[itri^1][i]*(pcontact->t*itri) + pmode->center;
		norm[0] = n0z+n0x*pcontact->taux+n0y*pcontact->t; norm[1] = -ptri[1]->n;
		pcontact->n = norm[itri];
		pcontact->iFeature[itri] = 0x40;
		pcontact->iFeature[itri^1] = 0x80 | i;
	}

	return 1;
}


int ray_tri_rot_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const triangle *ptri,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	Vec3r pt,ptz[2],ptx[2],pty[2],rotax=pmode->dir,edge1,ptz0,ptx0,pty0,pt1,pt0_rot,pt1_rot,edge0_rot,ptz1,ptx1,pty1,n;
	quotient tsin,tcos,tmin(pmode->tmax,1),t0,t1;
	real kcos,ksin,k,a,b,c,d;
	int i,j,bContact,bBest,idbest=-1,imask=iFeature2<<8 | iFeature1;

	pt = pray->origin-pmode->center; 
	ptz[0] = rotax*(pt*rotax); ptx[0] = pt-ptz[0]; pty[0] = rotax^ptx[0];
	pt += pray->dir; 
	ptz[1] = rotax*(pt*rotax); ptx[1] = pt-ptz[1]; pty[1] = rotax^ptx[1];

	// ray end - triangle face
	for(i=1;i>=0;i--) {
		kcos = ptx[i]*ptri->n; ksin = pty[i]*ptri->n; 
		k = (ptz[i]-ptri->pt[0]+pmode->center)*ptri->n;
		a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); tsin.set(-b-d,a);
			for(j=0;j<2;j++,tsin.x+=d*2) 
			if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
					isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1)
			{
				tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
				pt = ptx[i]*tcos.x + pty[i]*tsin.x + (ptz[i]+pmode->center)*tsin.y;
				bContact = // point is inside itri
					isneg((pt-ptri->pt[0]*tsin.y ^ ptri->pt[1]-ptri->pt[0])*ptri->n) & 
					isneg((pt-ptri->pt[1]*tsin.y ^ ptri->pt[2]-ptri->pt[1])*ptri->n) & 
					isneg((pt-ptri->pt[2]*tsin.y ^ ptri->pt[0]-ptri->pt[2])*ptri->n);
				bBest = bContact & isneg(tsin-tmin);
				UPDATE_IDBEST(tmin,i);
			}
		}
		if ((pray->origin-pmode->center).len2()<sqr(pmode->minPtDist))
			break;
	}

	if (idbest>=0) {
		i = idbest & 1;
		pcontact->t = tmin.val();
		pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
		pcontact->pt = ptz[i] + ptx[i]*pcontact->taux + pty[i]*pcontact->t + pmode->center;
		pcontact->n = -ptri->n;
		pcontact->iFeature[0] = 0x80 | i;
		pcontact->iFeature[1] = 0x40;
		return 1;
	}

	// ray - triangle edge
	pt = pray->origin-pmode->center^pray->dir; ptz0 = rotax*(pt*rotax); ptx0 = pt-ptz0; pty0 = rotax^ptx0;
	for(i=0;i<3;i++) if ((0xA0 | (0xA0|i)<<8)!=imask) {
		pt1 = ptri->pt[i]-pmode->center; edge1 = ptri->pt[inc_mod3[i]]-ptri->pt[i];
		pt = pt1^edge1; ptz1 = rotax*(pt*rotax); ptx1 = pt-ptz1; pty1 = rotax^ptx1;
		kcos = edge1*ptx0+pray->dir*ptx1; ksin = edge1*pty0-pray->dir*pty1; k = edge1*ptz0+pray->dir*ptz1;
		a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); tsin.set(-b-d,a);
			for(j=0;j<2;j++,tsin.x+=d*2) 
			if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
					isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1)
			{
				tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
				pt0_rot = ptx[0]*tcos.x + pty[0]*tsin.x + ptz[0]*tsin.y;
				pt1_rot = ptx[1]*tcos.x + pty[1]*tsin.x + ptz[1]*tsin.y;
				edge0_rot = pt1_rot-pt0_rot;
				n = edge0_rot^edge1; pt = pt1*tsin.y-pt0_rot;
				t0.set((pt^edge1)*n, n.len2());
				t1.set((pt^edge0_rot)*n, t0.y*tsin.y);
				bContact = isneg(fabs_tpl(t0.x*2-t0.y)-t0.y) & isneg(fabs_tpl(t1.x*2-t1.y)-t1.y);
				bBest = bContact & isneg(tsin-tmin);
				UPDATE_IDBEST(tmin,i | 0x80);
			}
		}
	}

	if (idbest<0)
		return 0;

	i = idbest & 3;
	pcontact->t = tmin.val();
	pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
	pt0_rot = ptx[0]*pcontact->taux + pty[0]*pcontact->t + ptz[0] + pmode->center;
	pt1_rot = ptx[1]*pcontact->taux + pty[1]*pcontact->t + ptz[1] + pmode->center;
	edge0_rot = pt1_rot-pt0_rot;
	edge1 = ptri->pt[inc_mod3[i]]-ptri->pt[i];
	pt = ptri->pt[i]-pt0_rot; n = edge0_rot ^ edge1;
	t0.set((pt^edge1)*n, n.len2());
	pcontact->pt = pt0_rot + edge0_rot*t0.val();
	pcontact->n = n*sgnnz((ptri->n^edge1)*n);
	pcontact->iFeature[0] = 0xA0;
	pcontact->iFeature[1] = 0xA0 | i;
	return 1;
}

int tri_ray_rot_unprojection(unprojection_mode *pmode, const triangle *ptri,int iFeature1, const ray *pray,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	int res =  ray_tri_rot_unprojection(pmode,pray,iFeature2,ptri,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt = pcontact->pt.GetRotated(pmode->center,pmode->dir, pcontact->taux,-pcontact->t);
		pcontact->n = pcontact->n.GetRotated(pmode->dir, pcontact->taux,-pcontact->t).Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	return res;
}


int ray_cyl_rot_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const cylinder *pcyl,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	Vec3r center,ccap,v,vx,vy,vz,l,lx,ly,lz,vsin,vcos,pt,ptz[2],ptx[2],pty[2], rotax=pmode->dir, axis=pcyl->axis;
	real a,b,c,d,k,ksin,kcos, hh=pcyl->hh, r2=sqr(pcyl->r), len2=pray->dir.len2(), roots[4];
	quotient tmax(0,1),tsin,tcos,t;
	int i,j,icap,idbest=-1,bContact,bBest;
	polynomial_tpl<real,4> pn;
	center = pcyl->center-pmode->center;

	pt = pray->origin-pmode->center; 
	ptz[0] = rotax*(pt*rotax); ptx[0] = pt-ptz[0]; pty[0] = rotax^ptx[0];
	pt += pray->dir; 
	ptz[1] = rotax*(pt*rotax); ptx[1] = pt-ptz[1]; pty[1] = rotax^ptx[1];

	for(i=1; i>=0; i--) {
		// ray end - cylinder cap
		kcos = ptx[i]*axis; ksin = pty[i]*axis; 
		for(icap=-1; icap<=1; icap+=2) {
			k = (ptz[i]-center-axis*(hh*icap))*axis;
			a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
			if (d>=0) {
				d = sqrt_tpl(d); tsin.set(-b-d,a);
				for(j=0;j<2;j++,tsin.x+=d*2) 
				if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
						isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1) = remove phantom roots
				{
					tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
					pt = ptx[i]*tcos.x + pty[i]*tsin.x + ptz[i]*tsin.y;
					bContact = isneg((pt-(center+axis*(hh*icap))*tsin.y).len2() - r2*sqr(tsin.y));
					bBest = bContact & isneg(tmax-tsin);
					UPDATE_IDBEST(tmax,icap+1|i);
				}
			}
		}

		// ray end - cylinder side
		v = ptz[i]-center ^ axis;
		vcos = ptx[i]^axis; vsin = pty[i]^axis;
		pn = psqr(P2(sqr(vsin)-sqr(vcos)) + P1((v*vsin)*2) + sqr(v)+sqr(vcos)-r2) - psqr(P1(vcos*vsin)+v*vcos)*((real)1-P2(1))*4;
		if (pn.nroots(0,1)) for(j=pn.findroots(0,1,roots)-1;j>=0;j--) {
			tsin.set(roots[j],1);
			pt = ptz[i]+ptx[i]*sqrt_tpl(1-sqr(tsin.x))+pty[i]*tsin.x-center;
			bContact = isneg(fabs_tpl(pt*axis)-hh) & inrange((pt^axis).len2(), r2*sqr((real)0.99),r2*sqr((real)1.01));
			bBest = isneg(tmax-tsin) & bContact;
			UPDATE_IDBEST(tmax,0x10|i);
		}

		if ((pray->origin-pmode->center).len2()<sqr(pmode->minPtDist))
			break;
	}

	// ray - cylinder side
	lz = rotax*(pray->dir*rotax); lx = pray->dir-lz; ly = rotax^lx;
	v = pray->origin-pmode->center^pray->dir;
	vz = rotax*(v*rotax); vx = v-vz; vy = rotax^vx;
	v = axis^center;
	kcos = axis*vx-v*lx; ksin = axis*vy-v*ly; k = axis*vz-v*lz;
	vcos = lx^axis; vsin = ly^axis; v = lz^axis;
	polynomial_tpl<real,2> sub = psqr(P1((kcos*ksin-r2*(vcos*vsin))*2)+(kcos*k-r2*(vcos*v))*2);
	pn = psqr(P2(sqr(ksin)-sqr(kcos)-r2*(sqr(vsin)-sqr(vcos)))+P1(((ksin*k)-r2*(vsin*v))*2)+sqr(kcos)+sqr(k)-r2*(sqr(vcos)+sqr(v))) - sub;
	if (pn.nroots(0,1)) for(j=pn.findroots(0,1,roots)-1;j>=0;j--) {
		tsin.set(roots[j],1);	tcos.x = sqrt_tpl(1-sqr(tsin.x));
		pt = ptz[0] + ptx[0]*tcos.x + pty[0]*tsin.x - center;
		l = lz + lx*tcos.x + ly*tsin.x;	v = l^axis;	k = v.len2();
		bContact = inrange(sqr(pt*v), r2*k*sqr((real)0.99),r2*k*sqr((real)1.01));	// remove phantom roots
		t.set((-pt^axis)*v, k);
		(pt*=t.y) += l*t.x;
		bContact &= inrange(t.x, (real)0,t.y);
		bContact &= isneg(fabs_tpl(pt*axis)-hh*t.y);
		bBest = isneg(tmax-tsin) & bContact;
		UPDATE_IDBEST(tmax,0x20);
	}

	// ray - cylinder cap
	for(icap=-1; icap<=1; icap+=2) {
		ccap = center+axis*(hh*icap);
		vcos = (axis^vx)-(axis^(ccap^lx)); vsin = (axis^vy)-(axis^(ccap^ly)); v = (axis^vz)-(axis^(ccap^lz));
		kcos = lx*axis; ksin = ly*axis; k = lz*axis;
		pn = psqr(P2(sqr(vsin)-sqr(vcos)-r2*(sqr(ksin)-sqr(kcos)))+P1(((vsin*v)-r2*(ksin*k))*2)+sqr(vcos)+sqr(v)-r2*(sqr(kcos)+sqr(k))) - 
				 psqr(P1((vcos*vsin-r2*(kcos*ksin))*2)+(vcos*v-r2*(kcos*k))*2);
		if (pn.nroots(0,1)) for(j=pn.findroots(0,1,roots)-1;j>=0;j--) {
			tsin.set(roots[j],1);	tcos.x = sqrt_tpl(1-sqr(tsin.x));
			pt = ptz[0] + ptx[0]*tcos.x + pty[0]*tsin.x;
			l = lz + lx*tcos.x + ly*tsin.x;
			t.set((ccap-pt)*axis, l*axis);
			bContact = inrange(((pt-ccap)*t.y+l*t.x).len2(), r2*sqr(t.y*(real)0.96),r2*sqr(t.y*(real)1.04)); // remove phantom roots
			bContact &= inrange(t.x, (real)0,t.y);
			bBest = isneg(tmax-tsin) & bContact;
			UPDATE_IDBEST(tmax,0x40|icap+1>>1);
		}
	}

	if (idbest<0)
		return 0;

	switch (idbest & 0xF0) {
		case 0x00: // ray end - cyl cap
			i = idbest & 1;	icap = (idbest&2)-1;
			pcontact->t = tmax.val();
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pcontact->pt = ptz[i] + ptx[i]*pcontact->taux + pty[i]*pcontact->t + pmode->center;
			pcontact->n = axis*-icap;
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x41 + (icap+1>>1);
			break;
		case 0x10: // ray end - cyl side
			i = idbest & 1;
			pcontact->t = tmax.x;
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pcontact->pt = ptz[i] + ptx[i]*pcontact->taux + pty[i]*pcontact->t + pmode->center;
			pcontact->n = pcyl->center-pcontact->pt;
			(pcontact->n -= pcyl->axis*(pcyl->axis*pcontact->n)).normalize();
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40;
			break;
		case 0x20: // ray - cyl side
			pcontact->t = tmax.x;
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pt = ptz[0] + ptx[0]*pcontact->taux + pty[0]*pcontact->t + pmode->center;
			l = lz + lx*pcontact->taux + ly*pcontact->t;
			t.set((pcyl->center-pt^axis)*(l^axis),(l^axis).len2());
			pcontact->pt = pt+l*t.val();
			pcontact->n = (l^axis).normalized();
			pcontact->n *= sgnnz(pcontact->n*(pcyl->center-pcontact->pt));
			pcontact->iFeature[0] = 0x20;
			pcontact->iFeature[1] = 0x40;
			break;
		case 0x40: // ray - cyl cap
			icap = ((idbest&1)<<1)-1;
			pcontact->t = tmax.x;
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pt = ptz[0] + ptx[0]*pcontact->taux + pty[0]*pcontact->t;
			l = lz + lx*pcontact->taux + ly*pcontact->t;
			ccap = center+axis*(hh*icap);
			t.set((ccap-pt)*axis,l*axis);
			pcontact->pt = pt+l*t.val();
			pcontact->n = pcontact->pt-ccap; pcontact->pt += pmode->center;
			pcontact->n = ((axis^pcontact->n)^l).normalized();
			pcontact->n *= sgnnz(pcontact->n*(pcyl->center-pcontact->pt));
			pcontact->iFeature[0] = 0x20;
			pcontact->iFeature[1] = 0x20|icap+1>>1;
	}
	return 1;
}

int cyl_ray_rot_unprojection(unprojection_mode *pmode, const cylinder *pcyl,int iFeature1, const ray *pray,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	int res =  ray_cyl_rot_unprojection(pmode,pray,iFeature2,pcyl,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt = pcontact->pt.GetRotated(pmode->center,pmode->dir, pcontact->taux,-pcontact->t);
		pcontact->n = pcontact->n.GetRotated(pmode->dir, pcontact->taux,-pcontact->t).Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	return res;
}


int ray_box_rot_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const box *pbox,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	int i,j,ifeat,idir,ix,iy,bContact,bBest,idbest=-1;
	Vec3r pt,ptz[2],ptx[2],pty[2],ptz0,ptx0,pty0,pt1,pt0_rot,pt1_rot,edge0_rot,ptz1,ptx1,pty1,origin,dir,center,rotax,n,size;
	quotient tsin,tcos,tmax(0,1),t0,t1;
	real kcos,ksin,k,a,b,c,d;

	origin = pbox->Basis*(pray->origin-pbox->center);
	dir = pbox->Basis*pray->dir;
	center = pbox->Basis*(pmode->center-pbox->center);
	rotax = pbox->Basis*pmode->dir;
	size = pbox->size;
	pt = origin-center; ptz[0] = rotax*(pt*rotax); ptx[0] = pt-ptz[0]; pty[0] = rotax^ptx[0];
	pt += dir; ptz[1] = rotax*(pt*rotax); ptx[1] = pt-ptz[1]; pty[1] = rotax^ptx[1];

	// ray end - box face
	for(i=1;i>=0;i--) {
		for(ifeat=0;ifeat<6;ifeat++) {
			idir = ifeat>>1; ix = inc_mod3[idir]; iy = dec_mod3[idir];
			kcos = ptx[i][idir]; ksin = pty[i][idir]; 
			k = ptz[i][idir]-size[idir]*((ifeat&1)*2-1)+center[idir];
			a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
			if (d>=0) {
				d = sqrt_tpl(d); tsin.set(-b-d,a);
				for(j=0;j<2;j++,tsin.x+=d*2) 
				if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
						isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1) = remove phantom roots
				{
					tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
					pt = ptx[i]*tcos.x + pty[i]*tsin.x + (ptz[i]+center)*tsin.y;
					bContact = // point is inside the face
						isneg(fabs_tpl(pt[ix])-size[ix]*tsin.y) & isneg(fabs_tpl(pt[iy])-size[iy]*tsin.y);
					bBest = bContact & isneg(tmax-tsin);
					UPDATE_IDBEST(tmax,i|ifeat<<1);
				}
			}
		}
		if ((pray->origin-pmode->center).len2()<sqr(pmode->minPtDist))
			break;
	}

	// ray - box edge
	pt = origin-center^dir; ptz0 = rotax*(pt*rotax); ptx0 = pt-ptz0; pty0 = rotax^ptx0;
	for(ifeat=0;ifeat<12;ifeat++) {
		idir = ifeat>>2; ix = inc_mod3[idir]; iy = dec_mod3[idir];
		pt1[idir]=0; pt1[ix]=size[ix]*((ifeat&1)*2-1); pt1[iy]=size[iy]*((ifeat&2)-1);
		pt1 = pt1-center; 
		pt = cross_with_ort(pt1,idir); ptz1 = rotax*(pt*rotax); ptx1 = pt-ptz1; pty1 = rotax^ptx1;
		kcos = ptx0[idir]+dir*ptx1; ksin = pty0[idir]-dir*pty1; k = ptz0[idir]+dir*ptz1;
		a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); tsin.set(-b-d,a);
			for(j=0;j<2;j++,tsin.x+=d*2) 
			if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
					isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1)
			{
				tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
				pt0_rot = ptx[0]*tcos.x + pty[0]*tsin.x + ptz[0]*tsin.y;
				pt1_rot = ptx[1]*tcos.x + pty[1]*tsin.x + ptz[1]*tsin.y;
				edge0_rot = pt1_rot-pt0_rot;
				n = cross_with_ort(edge0_rot,idir); pt = pt1*tsin.y-pt0_rot;
				t0.set(cross_with_ort(pt,idir)*n, n.len2());
				t1.set((pt^edge0_rot)*n, t0.y*tsin.y);
				bContact = inrange(t0.x, (real)0,t0.y) & isneg(fabs_tpl(t1.x)-fabs_tpl(t1.y)*size[idir]);
				bBest = bContact & isneg(tmax-tsin);
				UPDATE_IDBEST(tmax,ifeat | 0x80);
			}
		}
	}

	if (idbest<0)
		return 0;

	if ((idbest & 0x80)==0) {
		i = idbest & 1;	ifeat = idbest>>1; idir = ifeat>>1;
		pcontact->t = tmax.val();
		pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
		pcontact->pt = (ptz[i] + ptx[i]*pcontact->taux + pty[i]*pcontact->t + center)*pbox->Basis + pbox->center;
		pcontact->n.zero()[idir]=1-(ifeat&1)*2; pcontact->n = pcontact->n*pbox->Basis;
		pcontact->iFeature[0] = 0x80 | i;
		pcontact->iFeature[1] = 0x40 | ifeat;
	} else {
		ifeat = idbest&0x7F; idir = ifeat>>2; ix = inc_mod3[idir]; iy = dec_mod3[idir];
		pcontact->t = tmax.val();
		pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
		pt0_rot = ptx[0]*pcontact->taux + pty[0]*pcontact->t + ptz[0] + center;
		pt1_rot = ptx[1]*pcontact->taux + pty[1]*pcontact->t + ptz[1] + center;
		edge0_rot = pt1_rot-pt0_rot;
		pt[idir]=0; pt[ix]=size[ix]*((ifeat&1)*2-1); pt[iy]=size[iy]*((ifeat&2)-1);
		n = cross_with_ort(edge0_rot,idir);
		t0.set(cross_with_ort(pt-pt0_rot,idir)*n, n.len2());
		pt = pt0_rot + edge0_rot*t0.val();
		pcontact->n = (n*-sgnnz(n*pt))*pbox->Basis;
		pcontact->pt = pt*pbox->Basis+pbox->center;
		pcontact->iFeature[0] = 0xA0;
		pcontact->iFeature[1] = 0x20 | ifeat;
	}

	return 1;
}

int box_ray_rot_unprojection(unprojection_mode *pmode, const box *pbox,int iFeature1, const ray *pray,int iFeature2, 
														 contact *pcontact, geom_contact_area *parea)
{
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	int res =  ray_box_rot_unprojection(pmode,pray,iFeature2,pbox,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt = pcontact->pt.GetRotated(pmode->center,pmode->dir, pcontact->taux,-pcontact->t);
		pcontact->n = pcontact->n.GetRotated(pmode->dir, pcontact->taux,-pcontact->t).Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	return res;
}


int ray_capsule_rot_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const capsule *pcaps,int iFeature2, 
																 contact *pcontact, geom_contact_area *parea)
{
	Vec3r center,ccap,v,vx,vy,vz,l,lx,ly,lz,vsin,vcos,pt,ptz[2],ptx[2],pty[2], rotax=pmode->dir, axis=pcaps->axis;
	real a,b,c,d,k,ksin,kcos, hh=pcaps->hh, r2=sqr(pcaps->r), len2=pray->dir.len2(), roots[4];
	quotient tmax(0,1),tsin,tcos,t;
	int i,j,icap,idbest=-1,bContact,bBest;
	polynomial_tpl<real,4> pn;
	center = pcaps->center-pmode->center;

	pt = pray->origin-pmode->center; 
	ptz[0] = rotax*(pt*rotax); ptx[0] = pt-ptz[0]; pty[0] = rotax^ptx[0];
	pt += pray->dir; 
	ptz[1] = rotax*(pt*rotax); ptx[1] = pt-ptz[1]; pty[1] = rotax^ptx[1];

	for(i=1; i>=0; i--) {
		// ray end - capsule cap
		for(icap=-1; icap<=1; icap+=2) {
			ccap = center+axis*(hh*icap);
			kcos = ptx[i]*(ptz[i]-ccap)*2; ksin = pty[i]*(ptz[i]-ccap)*2;	k = ptx[i].len2()+(ptz[i]-ccap).len2()-r2;
			a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
			if (d>=0) {
				d = sqrt_tpl(d); tsin.set(-b-d,a);
				for(j=0;j<2;j++,tsin.x+=d*2) 
				if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
						isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1) = remove phantom roots
				{
					tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
					pt = ptx[i]*tcos.x + pty[i]*tsin.x + ptz[i]*tsin.y;
					bBest = isneg(tmax-tsin);
					UPDATE_IDBEST(tmax,icap+1|i);
				}
			}
		}

		// ray end - capsule side
		v = ptz[i]-center ^ axis;
		vcos = ptx[i]^axis; vsin = pty[i]^axis;
		pn = psqr(P2(sqr(vsin)-sqr(vcos)) + P1((v*vsin)*2) + sqr(v)+sqr(vcos)-r2) - psqr(P1(vcos*vsin)+v*vcos)*((real)1-P2(1))*4;
		if (pn.nroots(0,1)) for(j=pn.findroots(0,1,roots)-1;j>=0;j--) {
			tsin.set(roots[j],1);
			pt = ptz[i]+ptx[i]*sqrt_tpl(1-sqr(tsin.x))+pty[i]*tsin.x-center;
			bContact = isneg(fabs_tpl(pt*axis)-hh) & inrange((pt^axis).len2(), r2*sqr((real)0.99),r2*sqr((real)1.01));
			bBest = isneg(tmax-tsin) & bContact;
			UPDATE_IDBEST(tmax,0x10|i);
		}

		if ((pray->origin-pmode->center).len2()<sqr(pmode->minPtDist))
			break;
	}

	// ray - capsule side
	lz = rotax*(pray->dir*rotax); lx = pray->dir-lz; ly = rotax^lx;
	v = pray->origin-pmode->center^pray->dir;
	vz = rotax*(v*rotax); vx = v-vz; vy = rotax^vx;
	v = axis^center;
	kcos = axis*vx-v*lx; ksin = axis*vy-v*ly; k = axis*vz-v*lz;
	vcos = lx^axis; vsin = ly^axis; v = lz^axis;
	polynomial_tpl<real,2> sub = psqr(P1((kcos*ksin-r2*(vcos*vsin))*2)+(kcos*k-r2*(vcos*v))*2);
	pn = psqr(P2(sqr(ksin)-sqr(kcos)-r2*(sqr(vsin)-sqr(vcos)))+P1(((ksin*k)-r2*(vsin*v))*2)+sqr(kcos)+sqr(k)-r2*(sqr(vcos)+sqr(v))) - sub;
	if (pn.nroots(0,1)) for(j=pn.findroots(0,1,roots)-1;j>=0;j--) {
		tsin.set(roots[j],1);	tcos.x = sqrt_tpl(1-sqr(tsin.x));
		pt = ptz[0] + ptx[0]*tcos.x + pty[0]*tsin.x - center;
		l = lz + lx*tcos.x + ly*tsin.x;	v = l^axis;	k = v.len2();
		bContact = inrange(sqr(pt*v), r2*k*sqr((real)0.99),r2*k*sqr((real)1.01));	// remove phantom roots
		t.set((-pt^axis)*v, k);
		(pt*=t.y) += l*t.x;
		bContact &= inrange(t.x, (real)0,t.y);
		bContact &= isneg(fabs_tpl(pt*axis)-hh*t.y);
		bBest = isneg(tmax-tsin) & bContact;
		UPDATE_IDBEST(tmax,0x20);
	}

	// ray - capsule cap
	for(icap=-1; icap<=1; icap+=2) {
		ccap = center+axis*(hh*icap);
		vcos = (lx^ccap)+vx; vsin = (ly^ccap)+vy; v = (lz^ccap)+vz;
		pn = psqr(P2(sqr(vsin)-sqr(vcos)) + P1((v*vsin)*2) + sqr(v)+sqr(vcos)-r2*sqr(pray->dir)) - psqr(P1(vcos*vsin)+v*vcos)*((real)1-P2(1))*4;
		if (pn.nroots(0,1)) for(j=pn.findroots(0,1,roots)-1;j>=0;j--) {
			tsin.set(roots[j],1);	tcos.x = sqrt_tpl(1-sqr(tsin.x));
			pt = ptz[0] + ptx[0]*tcos.x + pty[0]*tsin.x;
			l = lz + lx*tcos.x + ly*tsin.x;
			t.set((ccap-pt)*l, l.len2());
			bContact = inrange(((pt-ccap)*t.y+l*t.x).len2(), r2*sqr(t.y*(real)0.99),r2*sqr(t.y*(real)1.01)); // remove phantom roots
			bContact &= inrange(t.x, (real)0,t.y);
			bBest = isneg(tmax-tsin) & bContact;
			UPDATE_IDBEST(tmax,0x40|icap+1>>1);
		}
	}

	if (idbest<0)
		return 0;

	switch (idbest & 0xF0) {
		case 0x00: // ray end - capsule cap
			i = idbest & 1;	icap = (idbest&2)-1;
			pcontact->t = tmax.val();
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pcontact->pt = ptz[i] + ptx[i]*pcontact->taux + pty[i]*pcontact->t + pmode->center;
			pcontact->n = (pcaps->center+pcaps->axis*(pcaps->hh*icap)-pcontact->pt).normalized();
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x41 + (icap+1>>1);
			break;
		case 0x10: // ray end - capsule side
			i = idbest & 1;
			pcontact->t = tmax.x;
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pcontact->pt = ptz[i] + ptx[i]*pcontact->taux + pty[i]*pcontact->t + pmode->center;
			pcontact->n = pcaps->center-pcontact->pt;
			(pcontact->n -= pcaps->axis*(pcaps->axis*pcontact->n)).normalize();
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0x40;
			break;
		case 0x20: // ray - capsule side
			pcontact->t = tmax.x;
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pt = ptz[0] + ptx[0]*pcontact->taux + pty[0]*pcontact->t + pmode->center;
			l = lz + lx*pcontact->taux + ly*pcontact->t;
			t.set((pcaps->center-pt^axis)*(l^axis),(l^axis).len2());
			pcontact->pt = pt+l*t.val();
			pcontact->n = (l^axis).normalized();
			pcontact->n *= sgnnz(pcontact->n*(pcaps->center-pcontact->pt));
			pcontact->iFeature[0] = 0x20;
			pcontact->iFeature[1] = 0x40;
			break;
		case 0x40: // ray - capsule cap
			icap = ((idbest&1)<<1)-1;
			pcontact->t = tmax.x;
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pt = ptz[0] + ptx[0]*pcontact->taux + pty[0]*pcontact->t;
			l = lz + lx*pcontact->taux + ly*pcontact->t;
			ccap = center+axis*(hh*icap);
			t.set((ccap-pt)*l, l.len2());
			pcontact->pt = pt+l*t.val();
			pcontact->n = (ccap-pcontact->pt).normalized();
			pcontact->pt += pmode->center;
			pcontact->iFeature[0] = 0x20;
			pcontact->iFeature[1] = 0x20|icap+1>>1;
	}
	return 1;
}

int capsule_ray_rot_unprojection(unprojection_mode *pmode, const capsule *pcaps,int iFeature1, const ray *pray,int iFeature2, 
																 contact *pcontact, geom_contact_area *parea)
{
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	int res =  ray_capsule_rot_unprojection(pmode,pray,iFeature2,pcaps,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt = pcontact->pt.GetRotated(pmode->center,pmode->dir, pcontact->taux,-pcontact->t);
		pcontact->n = pcontact->n.GetRotated(pmode->dir, pcontact->taux,-pcontact->t).Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	return res;
}


int ray_sphere_rot_unprojection(unprojection_mode *pmode, const ray *pray,int iFeature1, const sphere *psph,int iFeature2, 
																contact *pcontact, geom_contact_area *parea)
{
	Vec3r center,v,vx,vy,vz,l,lx,ly,lz,vsin,vcos,pt,ptz[2],ptx[2],pty[2], rotax=pmode->dir;
	real a,b,c,d,k,ksin,kcos, r2=sqr(psph->r), len2=pray->dir.len2(), roots[4];
	quotient tmax(0,1),tsin,tcos,t;
	int i,j,idbest=-1,bContact,bBest;
	polynomial_tpl<real,4> pn;
	center = psph->center-pmode->center;

	pt = pray->origin-pmode->center; 
	ptz[0] = rotax*(pt*rotax); ptx[0] = pt-ptz[0]; pty[0] = rotax^ptx[0];
	pt += pray->dir; 
	ptz[1] = rotax*(pt*rotax); ptx[1] = pt-ptz[1]; pty[1] = rotax^ptx[1];

	for(i=1; i>=0; i--) {
		// ray end - sphere
		kcos = ptx[i]*(ptz[i]-center)*2; ksin = pty[i]*(ptz[i]-center)*2;	k = ptx[i].len2()+(ptz[i]-center).len2()-r2;
		a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
		if (d>=0) {
			d = sqrt_tpl(d); tsin.set(-b-d,a);
			for(j=0;j<2;j++,tsin.x+=d*2) 
			if (isneg(fabs_tpl(tsin.x*2-tsin.y)-tsin.y) & // tsin in (0..1)
					isneg((ksin*tsin.x+k*tsin.y)*kcos)) // tcos in (0..1) = remove phantom roots
			{
				tcos.set(sqrt_tpl(sqr(tsin.y)-sqr(tsin.x)),tsin.y);
				pt = ptx[i]*tcos.x + pty[i]*tsin.x + ptz[i]*tsin.y;
				bBest = isneg(tmax-tsin);
				UPDATE_IDBEST(tmax,i);
			}
		}
		if ((pray->origin-pmode->center).len2()<sqr(pmode->minPtDist))
			break;
	}

	// ray - sphere
	lz = rotax*(pray->dir*rotax); lx = pray->dir-lz; ly = rotax^lx;
	v = pray->origin-pmode->center^pray->dir;
	vz = rotax*(v*rotax); vx = v-vz; vy = rotax^vx;
	vcos = (lx^center)+vx; vsin = (ly^center)+vy; v = (lz^center)+vz;
	pn = psqr(P2(sqr(vsin)-sqr(vcos)) + P1((v*vsin)*2) + sqr(v)+sqr(vcos)-r2*sqr(pray->dir)) - psqr(P1(vcos*vsin)+v*vcos)*((real)1-P2(1))*4;
	if (pn.nroots(0,1)) for(j=pn.findroots(0,1,roots)-1;j>=0;j--) {
		tsin.set(roots[j],1);	tcos.x = sqrt_tpl(1-sqr(tsin.x));
		pt = ptz[0] + ptx[0]*tcos.x + pty[0]*tsin.x;
		l = lz + lx*tcos.x + ly*tsin.x;
		t.set((center-pt)*l, l.len2());
		bContact = inrange(((pt-center)*t.y+l*t.x).len2(), r2*sqr(t.y*(real)0.99),r2*sqr(t.y*(real)1.01)); // remove phantom roots
		bContact &= inrange(t.x, (real)0,t.y);
		bBest = isneg(tmax-tsin) & bContact;
		UPDATE_IDBEST(tmax,0x40);
	}

	if (idbest<0)
		return 0;

	switch (idbest & 0xF0) {
		case 0x00: // ray end - sphere
			i = idbest & 1;
			pcontact->t = tmax.val();
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pcontact->pt = ptz[i] + ptx[i]*pcontact->taux + pty[i]*pcontact->t + pmode->center;
			pcontact->n = (psph->center-pcontact->pt).normalized();
			pcontact->iFeature[0] = 0x80 | i;
			pcontact->iFeature[1] = 0;
			break;
		case 0x40: // ray - sphere
			pcontact->t = tmax.x;
			pcontact->taux = sqrt_tpl(1-sqr(pcontact->t));
			pt = ptz[0] + ptx[0]*pcontact->taux + pty[0]*pcontact->t;
			l = lz + lx*pcontact->taux + ly*pcontact->t;
			t.set((center-pt)*l, l.len2());
			pcontact->pt = pt+l*t.val();
			pcontact->n = (center-pcontact->pt).normalized();
			pcontact->pt += pmode->center;
			pcontact->iFeature[0] = 0x20;
			pcontact->iFeature[1] = 0;
	}
	return 1;
}

int sphere_ray_rot_unprojection(unprojection_mode *pmode, const sphere *psph,int iFeature1, const ray *pray,int iFeature2, 
																contact *pcontact, geom_contact_area *parea)
{
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	int res =  ray_sphere_rot_unprojection(pmode,pray,iFeature2,psph,iFeature1,pcontact,parea);
	if (res) {
		pcontact->pt = pcontact->pt.GetRotated(pmode->center,pmode->dir, pcontact->taux,-pcontact->t);
		pcontact->n = pcontact->n.GetRotated(pmode->dir, pcontact->taux,-pcontact->t).Flip();
		int iFeature = pcontact->iFeature[0]; pcontact->iFeature[0]=pcontact->iFeature[1]; pcontact->iFeature[1]=iFeature;
	}
	(const_cast<unprojection_mode*>(pmode))->dir.Flip();
	return res;
}

#undef UPDATE_IDBEST
