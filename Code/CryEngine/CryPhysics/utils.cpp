// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>

// Mass properties calculations 
void compute_projection_integrals(Vec3r *ab, real pi[10])
{
	real a0[4],b0[4],a1[3],b1[3],C[4][3],da,db;
	int i,edge;
	for(i=0;i<10;i++) pi[i]=0;

	for(edge=0;edge<3;edge++,ab++) {
		for(i=1,a0[0]=ab[0].x;i<4;i++) a0[i]=a0[i-1]*ab[0].x;
		for(i=1,b0[0]=ab[0].y;i<4;i++) b0[i]=b0[i-1]*ab[0].y;
		for(i=1,a1[0]=ab[1].x;i<3;i++) a1[i]=a1[i-1]*ab[1].x;
		for(i=1,b1[0]=ab[1].y;i<3;i++) b1[i]=b1[i-1]*ab[1].y;
		for(i=1,C[0][0]=a1[1]+a1[0]*a0[0]+a0[1]; i<3; i++) C[0][i]=a1[0]*C[0][i-1]+a0[i+1];
		for(i=1,C[1][0]=b1[1]+b1[0]*b0[0]+b0[1]; i<3; i++) C[1][i]=b1[0]*C[1][i-1]+b0[i+1];
		da=a1[0]-a0[0]; db=b1[0]-b0[0];
		C[2][0] = 3*a1[1]+2*a1[0]*a0[0]+a0[1];
		C[2][1] = a0[0]*C[2][0]+4*a1[2];
		C[2][2] = 4*b1[2]+3*b1[1]*b0[0]+2*b1[0]*b0[1]+b0[2];
		C[3][0] = 3*a0[1]+2*a0[0]*a1[0]+a1[1];
		C[3][1] = a1[0]*C[3][0]+4*a0[2];
		C[3][2] = 4*b0[2]+3*b0[1]*b1[0]+2*b0[0]*b1[1]+b1[2];
		pi[0] += db*(a0[0]+a1[0]);
		for(i=0;i<3;i++) { 
			pi[i+1] += db*C[0][i];
			pi[i+4] += da*C[1][i];
		}
		pi[7] += db*(b1[0]*C[2][0]+b0[0]*C[3][0]);
		pi[8] += db*(b1[0]*C[2][1]+b0[0]*C[3][1]);
		pi[9] += da*(a1[0]*C[2][2]+a0[0]*C[3][2]);
	}

	pi[0]*=0.5;
	pi[1]*=1.0/6; pi[2]*=1.0/12; pi[3]*=1.0/20;
	pi[4]*=-1.0/6; pi[5]*=-1.0/12; pi[6]*=-1.0/20;
	pi[7]*=1.0/24; pi[8]*=1.0/60; pi[9]*=-1.0/60;
}

void compute_face_integrals(Vec3r *p,Vec3r n, real fi[12])
{
	real pi[10],k[4],t,w;
	int i;

	compute_projection_integrals(p, pi);

	w = -(n*p[0]);
	for(k[0]=1.0/n.z,i=1;i<4;i++) k[i]=k[i-1]*k[0];

	for(i=0;i<3;i++) fi[i*3+0] = k[0]*pi[i+1]; // a, a2, a3
	for(i=0;i<3;i++) fi[i*3+1] = k[0]*pi[i+4]; // b, b2, b3

	// a2b, b2g, g2a
	fi[9] = k[0]*pi[8];
	fi[10] = -k[1]*(n.x*pi[9] + n.y*pi[6] + w*pi[5]);
	fi[11] = k[2]*(n.x*n.x*pi[3] + n.y*n.y*pi[9] + w*w*pi[1] + 2*(n.x*n.y*pi[8] + n.x*w*pi[2] + n.y*w*pi[7]));

	for(i=0,t=n.x; i<3; i++,t*=n.x) pi[i+1]*=t;
	for(i=0,t=n.y; i<3; i++,t*=n.y) pi[i+4]*=t;
	for(i=0;i<3;i++) pi[i+7] *= n.x*n.y;
	pi[8]*=n.x; pi[9]*=n.y;

	// g, g2, g3
	fi[2] = -k[1]*(pi[1] + pi[4] + w*pi[0]);
	fi[5] = k[2]*(pi[2] + 2*pi[7] + pi[5] + w*(2*(pi[1] + pi[4]) + w*pi[0]));
	fi[8] = -k[3]*(pi[3] + 3*(pi[8] + pi[9]) + pi[6] + w*(3*(pi[2] + pi[5] + 2*pi[7] + w*(pi[1] + pi[4])) + w*w*pi[0]));
}

real ComputeMassProperties(strided_pointer<const Vec3> points, const index_t *faces, int nfaces, Vec3r &center,Matrix33r &I)
{
	real M=0,fi[12],nmax,diag[3]={0,0,0};
	Vec3r n,p[4];
	int i,g=0;
	center.zero();
	I.SetZero();

	for(nfaces--; nfaces>=0; nfaces--,faces+=3) {
		n = points[faces[1]]-points[faces[0]] ^ points[faces[2]]-points[faces[0]];
		if (n.len2()==0) 
			continue;
		n.normalize();

		for(i=0,nmax=-1;i<3;i++) if (n[i]*n[i]>nmax) 
		{	nmax = n[i]*n[i]; g=i; }
		for(i=0;i<3;i++) p[i] = points[faces[i]].GetPermutated(g);
		p[3]=p[0]; n = n.GetPermutated(g);

		compute_face_integrals(p,n, fi);
		g = g^g>>1^1;
		for(i=0;i<4;i++){
			Vec3r pr(fi[i*3],fi[i*3+1],fi[i*3+2]);
			Vec3r p1 = pr.GetPermutated(g);
			fi[i*3] = p1.x;	fi[i*3+1] = p1.y;	fi[i*3+2] = p1.z;
		}
		n = n.GetPermutated(g);

		M += n.x*fi[0];
		for(i=0;i<3;i++) {
			center[i] += n[i]*fi[i+3];
			diag[i] += n[i]*fi[i+6];
		}
		I(0,1) += n.x*fi[9];
		I(1,2) += n.y*fi[10];
		I(0,2) += n.z*fi[11];
	}

	if (M>0)
		center /= M*2;
	I(0,0) = (diag[1]+diag[2])*(1.0/3) - M*(center.y*center.y + center.z*center.z);
	I(1,1) = (diag[0]+diag[2])*(1.0/3) - M*(center.x*center.x + center.z*center.z);
	I(2,2) = (diag[0]+diag[1])*(1.0/3) - M*(center.x*center.x + center.y*center.y);
	I(1,0) = I(0,1) = -I(0,1)*(real)0.5+M*center.x*center.y;
	I(2,0) = I(0,2) = -I(0,2)*(real)0.5+M*center.x*center.z;
	I(2,1) = I(1,2) = -I(1,2)*(real)0.5+M*center.y*center.z;
	return M;
}


int ChoosePrimitiveForMesh(strided_pointer<const Vec3> pVertices,strided_pointer<const unsigned short> pIndices,int nTris, const Vec3r *eigen_axes,
													 const Vec3r &center, int flags, float tolerance, primitive *&pprim)
{
	static cylinder acyl;
	static box abox;
	static sphere asphere;
	int i,j,ibest;
	real error_max[3],error_avg[4],locerror,locarea;

	if (flags & mesh_approx_capsule) {
		float r[3],h[3],V[3],area[2],zloc,rinv;
		Matrix33 Basis = GetMtxFromBasis(eigen_axes);
		Vec3 axis,ptloc,n,ptmin,ptmax,c;
		int iz,itype;
		error_avg[3]=(real)1E10; ibest=3;

		ptmin=ptmax = Basis*pVertices[pIndices[0]];
		for(i=1; i<nTris*3; i++) {
			ptloc = Basis*pVertices[pIndices[i]];
			ptmin = min(ptmin,ptloc); ptmax = max(ptmax,ptloc);
		}
		c = ((ptmin+ptmax)*0.5f)*Basis;

		for(iz=0;iz<3;iz++) {
			axis = eigen_axes[iz];
			for(i=0,r[iz]=h[iz]=0;i<nTris*3;i++) {
				ptloc = pVertices[pIndices[i]]-c; zloc = ptloc*axis;
				r[iz] = max(r[iz],(ptloc-axis*zloc).len2()); h[iz] = max(h[iz],zloc);
			}
			r[iz] = sqrt_tpl(r[iz]); h[iz] -= r[iz];
			h[iz] = max(r[iz]*0.01f, h[iz]);
			if (fabs_tpl(r[iz])<(real)1E-5 || fabs_tpl(h[iz])<(real)1E-5)
				continue;
			V[iz] = sqr(r[iz])*((4.0f/3)*r[iz]+h[iz]*2);
			rinv = (real)1/r[iz];
			error_max[iz] = error_avg[iz] = 0;
			area[0] = area[1] = 0;

			for(i=0;i<nTris;i++) {
				n = pVertices[pIndices[i*3+1]]-pVertices[pIndices[i*3]] ^ pVertices[pIndices[i*3+2]]-pVertices[pIndices[i*3]];
				if (n.len2()==0) 
					continue;
				locarea=n.len(); n/=locarea; locarea*=(real)0.5;
				Vec3 ptc = (pVertices[pIndices[i*3]]+pVertices[pIndices[i*3+1]]+pVertices[pIndices[i*3+2]])*(1.0f/3)-c;
				zloc = ptc*axis;
				// locerror will contain maximum distance from triangle points to the capsule surface, normalized by capsule radius
				real locerrorSide=fabs_tpl((ptc-axis*(ptc*axis)).len()*rinv-(real)1), locerrorCap=0; 
				for(j=0;j<3;j++) {
					ptloc = pVertices[pIndices[i*3+j]]-c;
					locerrorCap = max(locerrorCap, fabs_tpl((ptloc-axis*(h[iz]*sgnnz(zloc))).len()*rinv-(real)1));
					locerrorSide = max(locerrorSide, fabs_tpl((ptloc-axis*(ptloc*axis)).len()*rinv-(real)1));
				}
				locerror = min(locerrorSide, locerrorCap);
				itype = isneg(locerrorCap-locerrorSide); // 0-capsule side, 1-cap
				error_max[iz] = max(error_max[iz],locerror);
				error_avg[iz] += locerror*locarea;
				area[itype] += locarea;
			}
			error_avg[iz] /= max(1e-20f,area[0]+area[1]);
			// additionally check if object area is close to that of the capsule
			float areaCaps = 4*gf_PI*r[iz]*(r[iz]+h[iz]);
			locerror = fabs_tpl(area[0]+area[1]-areaCaps)/areaCaps;
			error_max[iz] = max(error_max[iz], locerror);
			error_avg[iz] = error_avg[iz]*(real)0.6+locerror*(real)0.4;
		}
		float Vmin=min(min(V[0],V[1]),V[2]), Vmaxinv=1/max(max(max(V[0],V[1]),V[2]),1e-20f);
		for(iz=0;iz<3;iz++)
			error_avg[iz] = (error_avg[iz] + (V[iz]-Vmin)*Vmaxinv)*0.5f;
		ibest = idxmin3(error_avg);

		if (error_max[ibest]<tolerance*1.5f && error_avg[ibest]<tolerance) {
			acyl.axis = eigen_axes[ibest];
			acyl.center = c;
			acyl.r = r[ibest];
			acyl.hh = h[ibest];
			pprim = &acyl;
			return capsule::type;
		}
	}

	if (flags & mesh_approx_box) {
		int itry,iside;
		Matrix33 Basis = GetMtxFromBasis(eigen_axes);
		Vec3 size[2],rsize,pt,ptmin,ptmax,c[2];
		real area;
		error_max[0]=error_avg[0]=error_max[1]=error_avg[1] = 1E10;

		for(itry=0;itry<2;itry++) {
			ptmin=ptmax = Basis*pVertices[pIndices[0]];
			for(i=1; i<nTris*3; i++) {
				pt = Basis*pVertices[pIndices[i]];
				ptmin.x = min(ptmin.x,pt.x); ptmax.x = max(ptmax.x,pt.x);
				ptmin.y = min(ptmin.y,pt.y); ptmax.y = max(ptmax.y,pt.y);
				ptmin.z = min(ptmin.z,pt.z); ptmax.z = max(ptmax.z,pt.z);
			}
			c[itry] = ((ptmin+ptmax)*0.5f)*Basis;
			size[itry] = (ptmax-ptmin)*0.5f;
			if (size[itry].x*size[itry].y*size[itry].z==0)
				continue;
			error_max[itry]=error_avg[itry] = 0;
			rsize.x=1.0f/size[itry].x; rsize.y=1.0f/size[itry].y; rsize.z=1.0f/size[itry].z;
			for(i=0,area=0; i<nTris; i++) {
				pt = Basis*((pVertices[pIndices[i*3]]+pVertices[pIndices[i*3+1]]+pVertices[pIndices[i*3+2]])*(1.0f/3)-c[itry]);
				pt.x = fabsf(pt.x*rsize.x); pt.y = fabsf(pt.y*rsize.y); pt.z = fabsf(pt.z*rsize.z);
				locarea = (pVertices[pIndices[i*3+1]]-pVertices[pIndices[i*3]] ^ pVertices[pIndices[i*3+2]]-pVertices[pIndices[i*3]]).len();
				iside = idxmax3(&pt.x);	locerror = 0;
				for(j=0;j<3;j++) 
					locerror = max(locerror,fabs_tpl((fabs_tpl((pVertices[pIndices[i*3+j]]-c[itry])*Basis.GetRow(iside)))*rsize[iside]-(real)1));
				error_max[itry] = max(error_max[itry],locerror);
				error_avg[itry] += locerror*locarea;
				area += locarea;
			}
			error_avg[itry] /= area;
			// additionally check if object area is close to that of the box
			locerror = fabs_tpl((size[itry].x*size[itry].y+size[itry].x*size[itry].z+size[itry].y*size[itry].z)*16/area-1);
			error_max[itry] = max(error_max[itry], locerror);
			error_avg[itry] = error_avg[itry]*(real)0.7+locerror*(real)0.3;
			Basis.SetIdentity(); // try axis aligned box after eigen-vectors aligned 
		}

		ibest = isneg(error_avg[1]-error_avg[0]*0.95f); // favor axis-aligned box slightly
		if (error_max[ibest]<tolerance*1.5f && error_avg[ibest]<tolerance) {
			abox.size = size[ibest];
			abox.center = c[ibest];
			if (ibest) {
				abox.bOriented = 0;
				abox.Basis.SetIdentity();
			} else {
				abox.bOriented = 1;
				abox.Basis = GetMtxFromBasis(eigen_axes);
			}
			pprim = &abox;
			return box::type;
		}
	}

	if (flags & mesh_approx_sphere) {
		Vec3r p0,p1,p2,n;
		real r,rinv,area;
		for(i=0,r=0;i<nTris*3;i++) r += (pVertices[pIndices[i]]-center).len();
		r /= nTris*3;	rinv = (real)1.0/r;
		error_max[0]=error_avg[0]=area = 0;
		for(i=0;i<nTris;i++) {
			p0=pVertices[pIndices[i*3]]; p1=pVertices[pIndices[i*3+1]]; p2=pVertices[pIndices[i*3+2]];
			locerror = fabs_tpl((p0-center).len()-r)*rinv;
			locerror = max(locerror, fabs_tpl((p1-center).len()-r)*rinv);
			locerror = max(locerror, fabs_tpl((p2-center).len()-r)*rinv);
			n = p1-p0^p2-p0; locarea = n.len();	
			if (locarea>1E-5)
				locerror = max(locerror, fabs_tpl(((p0-center)*n)/locarea-r)*rinv);
			error_max[0] = max(error_max[0],locerror);
			error_avg[0] += locerror*locarea;
			area += locarea;
		}
		error_avg[0] /= area;
		if (error_max[0]<tolerance*1.5f && error_avg[0]<tolerance) {
			asphere.r = r;
			asphere.center = center;
			pprim = &asphere;
			return sphere::type;
		}
	}

	if (flags & mesh_approx_cylinder) {
		float r[3],h[3],area[2],zloc,rinv,hinv;
		Matrix33 Basis = GetMtxFromBasis(eigen_axes);
		Vec3 axis,ptloc,n,ptmin,ptmax,c;
		int iz,bBest,itype;
		error_avg[3]=(real)1E10; ibest=3;

		ptmin=ptmax = Basis*pVertices[pIndices[0]];
		for(i=1; i<nTris*3; i++) {
			ptloc = Basis*pVertices[pIndices[i]];
			ptmin = min(ptmin,ptloc); ptmax = max(ptmax,ptloc);
		}
		c = ((ptmin+ptmax)*0.5f)*Basis;

		for(iz=0;iz<3;iz++) {
			axis = eigen_axes[iz];
			for(i=0,r[iz]=h[iz]=0;i<nTris*3;i++) {
				ptloc = pVertices[pIndices[i]]-c; zloc = ptloc*axis;
				r[iz] = max(r[iz],(ptloc-axis*zloc).len2()); h[iz] = max(h[iz],zloc);
			}
			r[iz] = sqrt_tpl(r[iz]);
			if (fabs_tpl(r[iz])<(real)1E-5 || fabs_tpl(h[iz])<(real)1E-5)
				continue;
			rinv = (real)1/r[iz];
			hinv = (real)1/h[iz];
			error_max[iz] = error_avg[iz] = 0;
			area[0] = area[1] = 0;

			for(i=0;i<nTris;i++) {
				n = pVertices[pIndices[i*3+1]]-pVertices[pIndices[i*3]] ^ pVertices[pIndices[i*3+2]]-pVertices[pIndices[i*3]];
				if (n.len2()==0) 
					continue;
				locarea=n.len(); n/=locarea; locarea*=(real)0.5; zloc=fabs_tpl(n*axis);
				itype = isneg((real)0.5-zloc); // 0-cylinder side, 1-cap
				locerror = 0;	// locerror will contain maximum distance from from triangle points to the cyl surface, normalized by cyl size
				if (itype) for(j=0;j<3;j++)
					locerror = max(locerror, fabs_tpl(fabs_tpl((pVertices[pIndices[i*3+j]]-c)*axis)*hinv-(real)1));
				else for(j=0;j<3;j++) {
					ptloc = pVertices[pIndices[i*3+j]]-c;
					locerror = max(locerror, fabs_tpl((ptloc-axis*(ptloc*axis)).len()*rinv-(real)1));
				}
				error_max[iz] = max(error_max[iz],locerror);
				error_avg[iz] += locerror*locarea;
				area[itype] += locarea;
			}
			error_avg[iz] /= (area[0]+area[1]);
			// additionally check if object area is close to that of the cylinder
			locerror = fabs_tpl((area[0]-r[iz]*h[iz]*g_PI*4)*(rinv*hinv*((real)0.5/g_PI)));
			locerror = max(locerror, real(fabs_tpl((area[1]-r[iz]*r[iz]*g_PI*2)*(rinv*rinv*((real)0.5/g_PI))))   );
			error_max[iz] = max(error_max[iz], locerror);
			error_avg[iz] = error_avg[iz]*(real)0.7+locerror*(real)0.3;
			bBest = isneg(error_avg[iz]-error_avg[ibest]);
			ibest = ibest&~-bBest | iz&-bBest;
		}

		if (ibest<3 && error_max[ibest]<tolerance*1.5f && error_avg[ibest]<tolerance) {
			acyl.axis = eigen_axes[ibest];
			acyl.center = c;
			acyl.r = r[ibest];
			acyl.hh = h[ibest];
			pprim = &acyl;
			return cylinder::type;
		}
	}

	return triangle::type;
}

void ExtrudeBox(const box *pbox, const Vec3& dir,float step, box *pextbox)
{
	float proj,maxproj;
	int i;
	maxproj = (pbox->Basis.GetRow(0)-dir*(dir*pbox->Basis.GetRow(0))).len2()*pbox->size[0];
	proj = (pbox->Basis.GetRow(1)-dir*(dir*pbox->Basis.GetRow(1))).len2()*pbox->size[1];
	i = isneg(maxproj-proj); maxproj = max(proj,maxproj);
	proj = (pbox->Basis.GetRow(2)-dir*(dir*pbox->Basis.GetRow(2))).len2()*pbox->size[2];
	i |= isneg(maxproj-proj)<<1; i &= 2|(i>>1^1);

	pextbox->Basis.SetRow(2,dir);
	pextbox->Basis.SetRow(0,(pbox->Basis.GetRow(i)-dir*(dir*pbox->Basis.GetRow(i))).normalized());
	pextbox->Basis.SetRow(1, pextbox->Basis.GetRow(2)^pextbox->Basis.GetRow(0));
	pextbox->bOriented = 1;
	Matrix33 mtx = pextbox->Basis*pbox->Basis.T();
	(pextbox->size = mtx.Fabs()*pbox->size).z += fabs_tpl(step)*0.5f;
	pextbox->center = pbox->center + dir*(step*0.5f);
}

real RotatePointToPlane(const Vec3r &pt, const Vec3r &axis,const Vec3r &center, const Vec3r &n,const Vec3r &origin)
{
	Vec3r ptc,ptz,ptx,pty;
	real kcos,ksin,k,a,b,c,d;
	ptc = pt-center;
	ptz = axis*(axis*ptc); ptx = ptc-ptz; pty = axis^ptx;
	kcos = ptx*n; ksin = pty*n; k = (ptz+center-origin)*n;
	a = sqr(ksin)+sqr(kcos); b = ksin*k; c = sqr(k)-sqr(kcos); d = b*b-a*c;
	if (d>=0)
		return asin_tpl((sqrt_tpl(d)*sgnnz(b)-b)/a);
	else return 0;
}


int BakeScaleIntoGeometry(phys_geometry *&pgeom,IGeomManager *pGeoman, const Vec3& s, int bReleaseOld)
{
	IGeometry *pGeomScaled=0;
	if (phys_geometry *pAdam = (phys_geometry*)pgeom->pGeom->GetForeignData(DATA_UNSCALED_GEOM)) {
		if (bReleaseOld)
			pGeoman->UnregisterGeometry(pgeom), bReleaseOld=0;
		pgeom = pAdam;
	}
	switch (int itype=pgeom->pGeom->GetType()) {
		case GEOM_BOX: { 
			const primitives::box *pbox = (const primitives::box*)pgeom->pGeom->GetData();
			primitives::box boxScaled;	
			boxScaled = *pbox;
			Diag33 smtx(s);
			boxScaled.size = pbox->Basis*smtx*pbox->Basis.T()*pbox->size;
			boxScaled.center = smtx*pbox->center;
			pGeomScaled = pGeoman->CreatePrimitive(primitives::box::type, &boxScaled);
		} break;
		case GEOM_CAPSULE: case GEOM_CYLINDER: {
			const primitives::cylinder *pcyl = (const primitives::cylinder*)pgeom->pGeom->GetData();
			primitives::cylinder cylScaled;	
			int iaxis = idxmax3(pcyl->axis.abs());
			cylScaled.axis = pcyl->axis;
			cylScaled.hh = pcyl->hh*s[iaxis];
			float sr = (s.x+s.y+s.z-s[iaxis])*0.5f;
			cylScaled.r = pcyl->r*sr;
			if (itype==GEOM_CAPSULE)
				cylScaled.hh += pcyl->r*(s[iaxis]-sr);
			cylScaled.center = Diag33(s)*pcyl->center;
			pGeomScaled = pGeoman->CreatePrimitive(pgeom->pGeom->GetType(), &cylScaled);
		}	break;
		case GEOM_TRIMESH: {
			pGeomScaled = pGeoman->CloneGeometry(pgeom->pGeom);
			mesh_data *pmd = (mesh_data*)pGeomScaled->GetData();
			Diag33 smtx(s);
			for(int i=0; i<pmd->nVertices; i++)
				pmd->pVertices[i] = smtx*pmd->pVertices[i];
			pGeomScaled->SetData(pmd);
		} break;
	}
	if (pGeomScaled) {
		if (bReleaseOld)
			pGeoman->UnregisterGeometry(pgeom);
		pGeomScaled->SetForeignData(pgeom,DATA_UNSCALED_GEOM);
		pgeom = pGeoman->RegisterGeometry(pGeomScaled, pgeom->surface_idx, pgeom->pMatMapping,pgeom->nMats);
		pgeom->nRefCount = 0;	pGeomScaled->Release();
		return 1;
	}
	return 0;
}


Vec3 get_xqs_from_matrices(Matrix34 *pMtx3x4,Matrix33 *pMtx3x3, Vec3 &pos,quaternionf &q,float &scale, phys_geometry **ppgeom,IGeomManager *pGeoman)
{
	Vec3 s;
	if (pMtx3x4) {
		s.Set(pMtx3x4->GetColumn(0).len(), pMtx3x4->GetColumn(1).len(), pMtx3x4->GetColumn(2).len());
		q = quaternionf(Matrix33(pMtx3x4->GetColumn(0)/s.x, pMtx3x4->GetColumn(1)/s.y, pMtx3x4->GetColumn(2)/s.z));
		pos = pMtx3x4->GetTranslation();
	} else if (pMtx3x3) {
		s.Set(pMtx3x3->GetColumn(0).len(), pMtx3x3->GetColumn(1).len(), pMtx3x3->GetColumn(2).len());
		q = quaternionf(Matrix33(pMtx3x3->GetColumn(0)/s.x, pMtx3x3->GetColumn(1)/s.y, pMtx3x3->GetColumn(2)/s.z));
	} else
		return Vec3(1);
	scale = min(min(s.x,s.y),s.z);
	if (fabs_tpl(scale-1.0f)<0.001f)
		scale = 1.0f;
	else
		s /= scale;
	if (s.len2()>3.03f && ppgeom && pGeoman)
		if (!BakeScaleIntoGeometry(*ppgeom,pGeoman,s))
			scale *= (s.x+s.y+s.z)*(1.0f/3);
	return s;
}

static inline int32 iabsf(int32 x)
{
	return (~0x80000000)&x;
}
static inline int GetProjCubePlane(const Vec3 &in)
{
	// This breaks strict aliasing, but for this is okay
	// since the points are const and stored on the heap!
	static_assert(sizeof(int32)==sizeof(float), "Invalid type size!");
	const int32* pt = (const int32*)&in;
	int iPlane = isneg(iabsf(pt[0])-iabsf(pt[1]));
	iPlane |= isneg(iabsf(pt[iPlane])-iabsf(pt[2]))<<1; iPlane &= 2|(iPlane>>1^1);
	return iPlane<<1 | isnonneg(pt[iPlane]);
}


void RasterizePolygonIntoCubemap(const Vec3 *pt,int npt, int iPass, int *pGrid[6],int nRes, float rmin,float rmax,float zscale)
{
	int iPlane,iPlaneEnd,ipt,ipt1,i,j,iBrd[2],iBrdNext[2],idx,lxdir,iSign,ixlim[2],iylim,imask,idcell,iz,
		irmin,iyend,iOrder,nCells,ixleft,iEnter,nPlanes,maskUsed,loopIter;
	Vec3i ic;
	Vec2i icell,ibound;
	Vec3 nplane,n,rn;
	Vec2 ptint[2],dp[2];
	float kres,krres,koffs,dz,dp0[2],denom;
	quotientf z;

	struct cube_plane {
		int iEnter,iExit;
		float minz;
		int npt;
		Vec2 pt[32];
	};
	cube_plane planes[6];

	nplane = pt[1]-pt[0] ^ pt[2]-pt[0];
	if (isnonneg(nplane*pt[0])^iPass)
		return;
	kres = 0.5f*nRes; krres = 2.0f/nRes;
	koffs = 1.0f-krres*0.5f;
	irmin = float2int(rmin*zscale);
	for(i=0;i<6;i++) {
		planes[i].npt=0; planes[i].iExit=-1; planes[i].minz = rmax;
	}
	z.x = pt[0]*nplane;

	for(i=0;i<3;i++) for(ipt=0;ipt<npt;ipt++) {
		planes[i*2+0].minz = min(planes[i*2+0].minz,max(0.0f,-pt[ipt][i]));
		planes[i*2+1].minz = min(planes[i*2+1].minz,max(0.0f,pt[ipt][i]));
	}

	for(ipt=0;ipt<npt;ipt++) {
		ipt1 = ipt+1 & ipt-npt+1>>31;
		iPlane = GetProjCubePlane(pt[ipt]);
		iPlaneEnd = GetProjCubePlane(pt[ipt1]);
		n = pt[ipt]^pt[ipt1]; rn.zero(); 
		ic.z = iPlane>>1;	denom = 1.0f/(fabsf(pt[ipt][ic.z])+isneg(fabsf(pt[ipt][ic.z])-1E-5f)*1E4f);	// store the starting point
		planes[iPlane].pt[planes[iPlane].npt++&31].set(pt[ipt][inc_mod3[ic.z]]*denom, pt[ipt][dec_mod3[ic.z]]*denom); 
		maskUsed = 0;
		assert(iPlane<6 && iPlaneEnd<6);

		for(nPlanes=0; iPlane!=iPlaneEnd && nPlanes<6; nPlanes++) {
			maskUsed |= 1<<iPlane;
			ic.z = iPlane>>1; iSign = ((iPlane&1)<<1)-1; 
			ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];
			ibound.x = sgnnz(n[ic.y])*iSign; ibound.y = -sgnnz(n[ic.x])*iSign;
			ptint[0].x = ibound.x; // intersect line with face boundary conditions and take the intersection that is inside face
			ptint[0].y = -n[ic.z]*iSign-n[ic.x]*ptint[0].x;	// only check boundaries that lie farther along ccw movement of line around origin-edge plane normal
			ptint[1].y = ibound.y; 
			ptint[1].x = -n[ic.z]*iSign-n[ic.y]*ptint[1].y;
			idx = inrange(ptint[1].x, -n[ic.x],n[ic.x]);
			loopIter = 0;
			nextidx:
				if (rn[ic[idx^1]]==0) rn[ic[idx^1]] = 1.0f/(n[ic[idx^1]]+isneg(fabsf(n[ic[idx^1]])-1E-5f)*1E4f);
				j = ic[idx]<<1 | ibound[idx]+1>>1;
			if (j!=iPlaneEnd && maskUsed & 1<<j && ++loopIter<8) {
				idx^=1; goto nextidx;
			}
			ptint[idx][idx^1] *= rn[ic[idx^1]];
			// store ptint[idx] in iPlane's point list
			planes[iPlane].pt[planes[iPlane].npt++&31] = ptint[idx];
			planes[iPlane].iExit = idx+1-ibound[idx];
			iPlane = j;
			iEnter = (idx^1)+1-iSign;
			if (planes[iPlane].iExit>=0) { // add corner points between the last exit point and this enter point
				iOrder = (((iPlane&1)<<1)-1)*((iPass<<1)-1); j = iOrder>>31;
				for(i=planes[iPlane].iExit; i!=iEnter; i=i+iOrder&3)
					planes[iPlane].pt[planes[iPlane].npt++&31].set(1-((i+j^i+j<<1)&2),1-(i+j&2));
				planes[iPlane].iExit = -1;
			}	else
				planes[iPlane].iEnter = iEnter;
			// store ptint[idx] in the new iPlane's point list (transform it to the new iPlane beforehand)
			ptint[idx][idx] = ptint[idx][idx^1];
			ptint[idx][idx^1] = iSign;
			planes[iPlane].pt[planes[iPlane].npt++&31] = ptint[idx];
			if (planes[iPlane].npt>32)
				planes[iPlane].npt = 0; // should not happen
		}
	}

	for(iPlane=nCells=i=0,ic.z=6;iPlane<6;iPlane++) {
		j = iszero(planes[iPlane].npt)^1;
		nCells += j; j <<= iPlane>>1;	// j = plane axis bit
		ic.z -= (iPlane>>1)+1 & -(j & (i&j^j))>>31; // subtract plane axis index+1 from the sum if this axis hasn't been encountered yet
		i |= j;	// accumulate used axes mask
	}
	if (nCells==4 && ic.z>=1 && ic.z<=3) { 
		// we have 4 sides that form a 'ring', meaning one (and only one) axis is unaffected edges
		ic.z--; iPlane = isneg(nplane[ic.z])^iPass | ic.z<<1;
		iOrder = (((iPlane&1)<<1)-1)*((iPass<<1)-1); j = iOrder>>31;
		for(i=0; planes[iPlane].npt<4; i=i+iOrder&3)
			planes[iPlane].pt[planes[iPlane].npt++&31].set(1-((i+j^i+j<<1)&2),1-(i+j&2));
	}

	// rasterize resulting polygons in each plane
	for(iPlane=nCells=0;iPlane<6;iPlane++) {
		iOrder = (((iPlane&1)<<1)-1)*((iPass<<1)-1); j = iOrder>>31;
		if (planes[iPlane].iExit>=0) // close pending exits
			for(i=planes[iPlane].iExit; i!=planes[iPlane].iEnter; i=i+iOrder&3)
				planes[iPlane].pt[planes[iPlane].npt++&31].set(1-((i+j^i+j<<1)&2),1-(i+j&2));

		if (planes[iPlane].npt && planes[iPlane].npt<32) {
			ic.z = iPlane>>1; ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z]; iSign = (iPlane&1)*2-1;
			dz = nplane[ic.x]*krres; 

			for(i=1,iBrd[0]=iBrd[1]=0; i<planes[iPlane].npt; i++) {	// find the uppermost and the lowest points
				imask = -isneg(planes[iPlane].pt[iBrd[0]].y-planes[iPlane].pt[i].y);
				iBrd[0] = iBrd[0]&~imask | i&imask;
				imask = -isneg(planes[iPlane].pt[i].y-planes[iPlane].pt[iBrd[1]].y);
				iBrd[1] = iBrd[1]&~imask | i&imask;
			}
			iyend = min(nRes-1,max(0,float2int((planes[iPlane].pt[iBrd[1]].y+koffs)*kres+0.5f)));
			ixleft = min(nRes-1,max(0,float2int((planes[iPlane].pt[iBrd[0]].x+koffs)*kres)));
			icell.y = min(nRes-1,max(0,float2int((planes[iPlane].pt[iBrd[0]].y+koffs)*kres)));
			iBrd[1] = iBrd[0];

			do {
				iBrdNext[0] = iBrd[0]+iOrder;
				iBrdNext[0] += planes[iPlane].npt & iBrdNext[0]>>31; // wrap -1 to npt-1 and npt to 0
				iBrdNext[0] &= iBrdNext[0]-planes[iPlane].npt>>31;
				iBrdNext[1] = iBrd[1]-iOrder;
				iBrdNext[1] += planes[iPlane].npt & iBrdNext[1]>>31; // wrap -1 to npt-1 and npt to 0
				iBrdNext[1] &= iBrdNext[1]-planes[iPlane].npt>>31;
				idx = isneg(planes[iPlane].pt[iBrdNext[0]].y-planes[iPlane].pt[iBrdNext[1]].y);
				dp[0] = planes[iPlane].pt[iBrdNext[0]]-planes[iPlane].pt[iBrd[0]];
				dp0[0] = (dp[0]^planes[iPlane].pt[iBrd[0]]+Vec2(koffs,koffs))*kres;
				dp[1] = planes[iPlane].pt[iBrdNext[1]]-planes[iPlane].pt[iBrd[1]];
				dp0[1] = (dp[1]^planes[iPlane].pt[iBrd[1]]+Vec2(koffs,koffs))*kres;
				lxdir = sgnnz(dp[0].x);
				ixlim[0] = min(nRes-1,max(0,float2int((min(planes[iPlane].pt[iBrd[0]].x, planes[iPlane].pt[iBrdNext[0]].x)+koffs)*kres)));
				ixlim[1] = min(nRes-1,max(0,float2int((max(planes[iPlane].pt[iBrd[1]].x, planes[iPlane].pt[iBrdNext[1]].x)+koffs)*kres)));
				icell.y = max(icell.y,iyend);
				iylim = min(nRes-1,max(iyend,float2int((planes[iPlane].pt[iBrdNext[idx]].y+koffs)*kres)));
				do {
					// search left or right (dep. on sgn(dp.x)) from the left border to find the leftmost filled cell
					// left: iterate while point is inside and x!=xmin-1, increment x after loop; right: while point is outside and x!=xmax
					for(icell.x=ixleft; isneg((dp[0]^icell)*lxdir-dp0[0]*lxdir) & (iszero(ixlim[lxdir+1>>1]+(lxdir>>31)-icell.x)^1); icell.x+=lxdir);
					icell.x -= lxdir>>31; ixleft = icell.x;
					// search right from the leftmost cell to the end of the right border, filling data
					z.y = nplane[ic.x]*(icell.x*krres-koffs)+nplane[ic.y]*(icell.y*krres-koffs)+nplane[ic.z]*iSign;
					idcell = icell.x+icell.y*nRes;
					// 1st (front face) pass output: 
					//  iz<rmin - set the highest bit
					//  else - update z value in the lower 30 bits
					// 2nd (back face) pass output:
					//  iz<rmin - clear the highest bit
					//  else - update z value in the lower 30 bits
					// after both passes: 
					//  if the highest bit is set, change cell z to irmin
					if (iPass==0) for(; isneg((dp[1]^icell)-dp0[1]) & isneg(icell.x-ixlim[1]-1); icell.x++,z.y+=dz,idcell++) {
						iz = float2int(max(planes[iPlane].minz,min(fabsf(z.val()),rmax))*zscale); nCells++;
						imask = iz-irmin>>31; iz = (iz|imask) & (1u<<31)-1;
						pGrid[iPlane][idcell] = min(pGrid[iPlane][idcell]&(1u<<31)-1,(unsigned int)iz) | (pGrid[iPlane][idcell] | imask)&(1<<31);
					} else for(; isneg((dp[1]^icell)-dp0[1]) & isneg(icell.x-ixlim[1]-1); icell.x++,z.y+=dz,idcell++)  {
						iz = float2int(max(planes[iPlane].minz,min(fabsf(z.val()),rmax))*zscale); nCells++;
						//pGrid[iPlane][idcell] &= irmin-iz>>31 | (1u<<31)-1;
						imask = iz-irmin>>31; iz = (iz|imask) & (1u<<31)-1;
						pGrid[iPlane][idcell] = min(pGrid[iPlane][idcell]&(1u<<31)-1,(unsigned int)iz) | (pGrid[iPlane][idcell] & ~imask)&(1<<31);
					}
				} while(--icell.y>=iylim);
				iBrd[idx] = iBrdNext[idx];
				ixleft = ixleft&-idx | ixlim[0]&~-idx; // update ixleft if the left branch advances
			} while(iylim>iyend);
		}
	}

	if (nCells==0) { // do not allow objects to take no rasterized space
		iPlane = GetProjCubePlane(pt[0]);
		ic.z = iPlane>>1; ic.x = inc_mod3[ic.z]; ic.y = dec_mod3[ic.z];
		denom = 1.0f/fabsf(pt[0][ic.z]);
		icell.x = min(nRes-1,max(0,float2int((pt[0][ic.x]*denom+koffs)*kres)));
		icell.y = min(nRes-1,max(0,float2int((pt[0][ic.y]*denom+koffs)*kres)));
		idcell = icell.x+icell.y*nRes;
		iz = float2int(min(fabsf(pt[0][ic.z]),rmax)*zscale);
		if (iPass==0) {
			imask = iz-irmin>>31; iz = (iz|imask) & (1u<<31)-1;
			pGrid[iPlane][idcell] = min(pGrid[iPlane][idcell]&(1u<<31)-1,(unsigned int)iz) | (pGrid[iPlane][idcell] | imask)&(1<<31);
		} else
			pGrid[iPlane][idcell] &= irmin-iz>>31 | (1u<<31)-1;
	}
}


inline int get_cubemap_cell_buddy(int idCell,int iBuddy, int nRes)
{
	int idx,istep,bWrap,idBuddy,idWrappedBuddy;
	Vec3i icell,iaxis;
	idx = iBuddy&1; // step axis: 0 - local x, 1 - local y
	istep = (iBuddy&2)-1; // step direction

	icell[idx] = (idCell>>8*idx & 0xFF)+istep; // unpacked cell (x,y,z)
	icell[idx^1] = idCell>>8*(idx^1) & 0xFF;
	icell.z = nRes-1 & -(idCell>>16&1);

	iaxis.z = idCell>>17;
	iaxis.x = inc_mod3[iaxis.z];
	iaxis.y = dec_mod3[iaxis.z];

	idBuddy = icell.x | icell.y<<8 | idCell&0x70000;
	idWrappedBuddy = icell[idx^1]<<8*idx | icell.z<<8*(idx^1) | iaxis[idx]<<17 | istep+1<<15;

	bWrap = icell[idx]>>31 | nRes-1-icell[idx]>>31;
	return idWrappedBuddy&bWrap | idBuddy&~bWrap;
}

void GrowAndCompareCubemaps(SOcclusionCubeMap* cubemapOcc, SOcclusionCubeMap* cubemap, int nGrow, int &nCells,int &nOccludedCells) PREFAST_SUPPRESS_WARNING(6262)
{
	struct cell_info {
		int idcell;
		int z;
	};
	int i,iPlane,icell,icell1,idcell,idcell1,ix,iy,bVisible,ipass,ihead=0,itail=0,itailend,imaxz=(1u<<31)-1,nRes=cubemap->N, planeBit;
	cell_info queue[4000];
	nCells = nOccludedCells = 0;

	if (cubemap->bMode==0) {
		for(iPlane=0;iPlane<6;iPlane++) {
			for(iy=0;iy<nRes;iy++) for(ix=0;ix<nRes;ix++) {
				icell = iy*nRes+ix;
				if (cubemap->grid[iPlane][icell]<imaxz)
				{
					nCells++;
					if (cubemap->grid[iPlane][icell]>cubemapOcc->grid[iPlane][icell]) // not visible
						nOccludedCells++;
					if (nGrow>0) for(i=0,idcell=ix|iy<<8|iPlane<<16;i<4;i++) { // enqueue neighbouring unused cells
						idcell1 = get_cubemap_cell_buddy(idcell, i, nRes);
						icell1 = (idcell1>>8&0xFF)*nRes + (idcell1&0xFF);
						if (cubemap->grid[idcell1>>16][icell1]>=imaxz) {
							queue[ihead].idcell = idcell1; queue[ihead].z = cubemap->grid[iPlane][icell]; 
							ihead = ihead+1 & CRY_ARRAY_COUNT(queue)-1;
						}
					}
				}
			}
		}

		for(ipass=0;ipass<nGrow;ipass++) 
			for(itailend=ihead; itail!=itailend; itail = itail+1 & CRY_ARRAY_COUNT(queue)-1) {
				idcell = queue[itail].idcell; 
				icell = (idcell>>8&0xFF)*nRes + (idcell&0xFF);
				iPlane = idcell>>16;
				if (cubemap->grid[iPlane][icell]<imaxz)
					continue;
				cubemap->grid[iPlane][icell] = queue[itail].z;

				bVisible = isneg(cubemap->grid[iPlane][icell]-cubemapOcc->grid[iPlane][icell]);
				nCells++; nOccludedCells += (bVisible^1);

				for(i=0;i<4;i++) { // enqueue neighbouring unused cells
					idcell1 = get_cubemap_cell_buddy(idcell, i, nRes);
					icell1 = (idcell1>>8&0xFF)*nRes + (idcell1&0xFF);
					if (cubemap->grid[idcell1>>16][icell1]>=imaxz) {
						queue[ihead].idcell = idcell1; queue[ihead].z = cubemap->grid[iPlane][icell];
						ihead = ihead+1 & CRY_ARRAY_COUNT(queue)-1;
					}
				}
			}
	} else { // bMode == 1  reverse the compares
		for(iPlane=0, planeBit=1;iPlane<6;iPlane++, planeBit<<=1) if (cubemap->usedPlanes&planeBit) {
			for(iy=0;iy<nRes;iy++) for(ix=0;ix<nRes;ix++) {
				icell = iy*nRes+ix;
				if (cubemap->grid[iPlane][icell]>0)
				{
					nCells++;
					if (cubemap->grid[iPlane][icell]<cubemapOcc->grid[iPlane][icell]) // not visible
						nOccludedCells++;
					if (nGrow>0) for(i=0,idcell=ix|iy<<8|iPlane<<16;i<4;i++) { // enqueue neighbouring unused cells
						idcell1 = get_cubemap_cell_buddy(idcell, i, nRes);
						icell1 = (idcell1>>8&0xFF)*nRes + (idcell1&0xFF);
						if (cubemap->grid[idcell1>>16][icell1]==0) {
							queue[ihead].idcell = idcell1; queue[ihead].z = cubemap->grid[iPlane][icell]; 
							ihead = ihead+1 & CRY_ARRAY_COUNT(queue)-1;
						}
					}
				}
			}
		}

		for(ipass=0;ipass<nGrow;ipass++) 
			for(itailend=ihead; itail!=itailend; itail = itail+1 & CRY_ARRAY_COUNT(queue)-1) {
				idcell = queue[itail].idcell; 
				icell = (idcell>>8&0xFF)*nRes + (idcell&0xFF);
				iPlane = idcell>>16;
				if (cubemap->grid[iPlane][icell]>0)
					continue;
				cubemap->grid[iPlane][icell] = queue[itail].z;

				bVisible = isneg(cubemapOcc->grid[iPlane][icell]-cubemap->grid[iPlane][icell]);
				nCells++; nOccludedCells += (bVisible^1);

				for(i=0;i<4;i++) { // enqueue neighbouring unused cells
					idcell1 = get_cubemap_cell_buddy(idcell, i, nRes);
					icell1 = (idcell1>>8&0xFF)*nRes + (idcell1&0xFF);
					if (cubemap->grid[idcell1>>16][icell1]==0) {
						queue[ihead].idcell = idcell1; queue[ihead].z = cubemap->grid[iPlane][icell];
						ihead = ihead+1 & CRY_ARRAY_COUNT(queue)-1;
					}
				}
			}
	}
}

void debug_calc_tri_resistance(const Vec3 *pt,const Vec3 &n, const Vec3 &v,const Vec3 &w, Vec3 &P,Vec3 &L)
{
	float square = (pt[1]-pt[0] ^ pt[2]-pt[0]).len2()*0.25f;
	if (square<sqr(0.01f)) {
		Vec3 r = (pt[0]+pt[1]+pt[2])*(1.0f/3);
		Vec3 vloc=v+(w^r), dP=n*((n*vloc)*sqrt_tpl(square));
		P += dP; L += r^dP;
		return;
	}
	Vec3 subpt[3];
	subpt[0]=(pt[0]+pt[2])*0.5f; subpt[1]=pt[0]; subpt[2]=(pt[0]+pt[1])*0.5f;
	debug_calc_tri_resistance(subpt,n, v,w, P,L);
	subpt[0]=pt[1]; subpt[1]=(pt[1]+pt[2])*0.5f;
	debug_calc_tri_resistance(subpt,n, v,w, P,L);
	subpt[0]=(pt[0]+pt[2])*0.5f; subpt[2]=pt[2];
	debug_calc_tri_resistance(subpt,n, v,w, P,L);
	subpt[1]=(pt[0]+pt[1])*0.5f; subpt[2]=(pt[1]+pt[2])*0.5f;
	debug_calc_tri_resistance(subpt,n, v,w, P,L);
}


int crop_polygon_with_plane(const Vec3 *ptsrc,int nsrc, Vec3 *ptdst, const Vec3 &n,float d)
{
	int i0,i1,ndst,iCount;
	for(i0=0;i0<nsrc && ptsrc[i0]*n>=d;i0++);
	if (i0==nsrc)
		return 0;
	for(iCount=ndst=0;iCount<nsrc;iCount++,i0=i1) {
		i1 = i0+1 & i0-nsrc+1>>31;
		ptdst[ndst] = ptsrc[i0];
		ndst += isneg(ptsrc[i0]*n-d);
		if ((ptsrc[i0]*n-d)*(ptsrc[i1]*n-d)<0)
			ptdst[ndst++] = ptsrc[i0]+(ptsrc[i1]-ptsrc[i0])*((d-ptsrc[i0]*n)/((ptsrc[i1]-ptsrc[i0])*n));
	}
	return ndst;
}


void CalcMediumResistance(const Vec3 *ptsrc,int npt, const Vec3& n, const plane &waterPlane,
													const Vec3 &vworld,const Vec3 &wworld,const Vec3 &com, Vec3 &P,Vec3 &L)
{
	int i;
	Vec3 pt0[16],pt[16],v,w,rotax,dP(ZERO),dL(ZERO);
	float x0,y0,dx,dy,Fxy,Fxx,Fxxy,Fxyy,Fxxx,square=0,sina;
	npt = crop_polygon_with_plane(ptsrc,npt, pt0, waterPlane.n,waterPlane.origin*waterPlane.n);
	for(i=0;i<npt;i++) pt0[i]-=com;
	npt = crop_polygon_with_plane(pt0,npt, pt, wworld^n,vworld*n);

	/*Vec3 dP1(zero),dL1(zero); pt[npt] = pt[0];
	for(i=0,pt0[0].zero();i<npt;i++) pt0[0]+=pt[i]; pt0[0]/=npt;
	for(i=0;i<npt;i++) {
		pt0[1]=pt[i]; pt0[2]=pt[i+1]; 
		debug_calc_tri_resistance(pt0,n, vworld,wworld, dP1,dL1);
	}*/

	rotax = n^Vec3(0,0,1);
	sina = rotax.len();
	if (sina>0.001f)
		rotax/=sina;
	else 
		rotax.Set(1,0,0);
	v = vworld.GetRotated(rotax,n.z,sina);
	w = wworld.GetRotated(rotax,n.z,sina);
	for(i=0;i<npt;i++)
		pt[i] = pt[i].GetRotated(rotax,n.z,sina);
	if (npt>0)
		pt[npt] = pt[0];

	for(i=0;i<npt;i++) {
		square += pt[i].x*pt[i+1].y-pt[i+1].x*pt[i].y;
		x0=pt[i].x; y0=pt[i].y; dx=pt[i+1].x-pt[i].x; dy=pt[i+1].y-pt[i].y;
		Fxy = x0*y0 + (dx*y0+dy*x0)*0.5f + dx*dy*(1.0f/3);
		Fxx = x0*x0 + dx*x0 + dx*dx*(1.0f/3);
		dP.z += dy*(w.x*Fxy-w.y*0.5f*Fxx);
		dL.x += v.z*dy*Fxy;
		dL.y -= v.z*dy*0.5f*Fxx;
		Fxxy = dx*dx*dy*0.25f + (dx*dx*y0+dx*dy*x0*2)*(1.0f/3) + (x0*y0*dx*2+x0*x0*dy)*0.5f + x0*x0*y0;
		Fxyy = dy*dy*dx*0.25f + (dy*dy*x0+dy*dx*y0*2)*(1.0f/3) + (y0*x0*dy*2+y0*y0*dx)*0.5f + y0*y0*x0;
		Fxxx = dx*dx*dx*0.25f + dx*dx*x0 + dx*x0*x0*1.5f + x0*x0*x0;
		dL.x += dy*(w.x*Fxyy-w.y*0.5f*Fxxy);
		dL.y -= dy*(w.x*0.5f*Fxxy-w.y*(1.0f/3)*Fxxx);
	}
	dP.z += v.z*square*0.5f;
	P -= dP.GetRotated(rotax,n.z,-sina);
	L -= dL.GetRotated(rotax,n.z,-sina);
}


//static const int g_nRanges = 5;
static int g_rngb2a[] = { 0,'A',26,'a',52,'0',62,'+',63,'/' };
static int g_rnga2b[] = { '+',62,'/',63,'0',52,'A',0,'a',26 };
inline int mapsymb(int symb, int *pmap,int n) {
	int i,j;
	for(i=j=0;j<n;j++) i+=isneg(symb-pmap[j*2]); i=n-1-i;
	return symb-pmap[i*2]+pmap[i*2+1];
}

int bin2ascii(const unsigned char *pin,int sz, unsigned char *pout) 
{
	int a0,a1,a2,i,j,chr[4],count0;
	unsigned char *pout0=pout,c=1;
	for(i=count0=0; i<sz; i+=3) {
		a0 = pin[i]; j=isneg(i+1-sz); a1 = pin[i+j]&-j; j=isneg(i+2-sz); a2 = pin[i+j*2]&-j;
		chr[0] = a0>>2; chr[1] = a0<<4&0x30 | (a1>>4)&0x0F; 
		chr[2] = a1<<2&0x3C | a2>>6&0x03; chr[3] = a2&0x03F;
		for(j=0;j<4;j++) if ((c=mapsymb(chr[j],g_rngb2a,5))=='A' && count0<99)
			count0++;
		else { 
			if (count0) {
				flush_rle:
				if (count0<3)
					for(;count0;count0--) *pout++='A';
				else {
					int dec = count0/10;
					*pout++='#'; *pout++=dec+'0'; *pout++=count0-dec*10+'0';
					count0 = 0;
				}
			}
			*pout++ = c;
		}
	}
	if (count0) {
		c=0; goto flush_rle;
	} else if (c)
		*pout++ = 0;
	return pout-pout0;	
}

int ascii2bin(const unsigned char *pin,int sz, unsigned char *pout)
{
	int a[4],nout,count0;
	const unsigned char *pin0=pin;
	for(nout=count0=0; *pin || count0; nout+=3) {
		for(int i=0;i<4;i++) if (count0>0) 
			a[i]=0, count0--; 
		else if (*pin!='#')
			a[i] = mapsymb(*pin++,g_rnga2b,5);
		else {
			count0 = (pin[1]-'0')*10+pin[2]-'0'-1;
			a[i]=0; pin+=3;
		}
		if (nout>sz-3)
			return 0;
		*pout++ = a[0]<<2 | a[1]>>4; *pout++ = a[1]<<4&0xF0 | a[2]>>2&0x0F; *pout++ = a[2]<<6&0xC0 | a[3];
	}
	return nout;
}


int CoverPolygonWithCircles(strided_pointer<Vec2> pt,int npt,bool bConsecutive, const Vec2 &center, 
														Vec2 *&centers,float *&radii, float minCircleRadius)
{
	intptr_t imask;
	int i,nCircles=0,nSkipped;
	Vec2 pts[3],bisector;
	float r,area,l2;
	ptitem2d *pdata,*plist,*pvtx,*pvtx_max,*pvtx_left,*pvtx_right;

	static Vec2 g_centers[32];
	static float g_radii[32];
	centers = g_centers;
	radii = g_radii;
	if (npt<2)
		return 0;
	pdata = plist = new ptitem2d[npt];
	for(i=0,r=0; i<npt; i++) {
		pdata[i].pt = pt[i]-center; 
		pdata[i].next = pdata+(i+1 & i+1-npt>>31);
		pdata[i].prev = pdata+i-1+(npt & i-1>>31);
		r = max(r, len2(pdata[i].pt));
	}
	if (r < sqr(minCircleRadius)) {
		g_centers[0] = center;
		g_radii[0] = sqrt_tpl(r);
		delete[] pdata;
		return 1;
	}
	if (!bConsecutive) {
		edgeitem *pcontour = new edgeitem[npt],*pedge;
		if (qhull2d(pdata,npt,pcontour)) {
			plist=pvtx=(pedge=pcontour)->pvtx; npt=0; 
			do {
				pvtx->next = pedge->next->pvtx;
				pvtx->prev = pedge->prev->pvtx;
				pvtx = pvtx->next; npt++;
			}	while((pedge=pedge->next)!=pcontour);
		}
		delete[] pcontour;
	}
	for(i=0,area=0,pvtx=plist; i<npt; i++,pvtx=pvtx->next)
		area += pvtx->pt^pvtx->next->pt;
	if (fabs_tpl(area*0.5f-r*g_PI) < area*0.4f) {
		// one circle fits the figure quite ok
		g_centers[0] = center;
		g_radii[0] = sqrt_tpl(r);
		delete[] pdata;
		return 1;
	}

	do {
		pvtx=pvtx_max=plist; do { // find the farthest from the center vertex
			imask = -isneg(len2(pvtx_max->pt) - len2(pvtx->pt));
			pvtx_max = (ptitem2d*)((intptr_t)pvtx_max&~imask | (intptr_t)pvtx&imask);
		} while((pvtx=pvtx->next)!=plist);
		l2 = len2(pvtx_max->pt); 

		// find the farthest from the center vertex in +30 degrees vicinity of the global maximum
		for(pvtx=(pvtx_left=pvtx_max)->next; 
			pvtx!=pvtx_max && sqr(pvtx->pt^pvtx_max->pt) < 0.25f*len2(pvtx->pt)*l2 && pvtx->pt*pvtx_max->pt>0; pvtx=pvtx->next) 
		{ imask = -((intptr_t)isneg(len2(pvtx_left->pt) - len2(pvtx->pt)) | iszero((intptr_t)pvtx_left-(intptr_t)pvtx_max));
			pvtx_left = (ptitem2d*)((intptr_t)pvtx_left&~imask | (intptr_t)pvtx&imask);
		}
		// find the farthest from the center vertex in -30 degrees vicinity of the global maximum
		for(pvtx=(pvtx_right=pvtx_max)->prev; 
			pvtx!=pvtx_max && sqr(pvtx->pt^pvtx_max->pt) < 0.25f*len2(pvtx->pt)*l2 && pvtx->pt*pvtx_max->pt>0; pvtx=pvtx->prev) 
		{ imask = -((intptr_t)isneg(len2(pvtx_right->pt) - len2(pvtx->pt)) | iszero((intptr_t)pvtx_right-(intptr_t)pvtx_max));
			pvtx_right = (ptitem2d*)((intptr_t)pvtx_right&~imask | (intptr_t)pvtx&imask);
		}

		// find a circle w/ center on left-right bisector that covers all 3 max vertices
		bisector = norm(pvtx_left->pt+pvtx_right->pt);
		pts[0]=pvtx_left->pt; pts[1]=pvtx_right->pt; pts[2]=pvtx_max->pt;
		for(i=0,r=0;i<3;i++) {
			float x = bisector*pts[i];
			r = max(r,(sqr(x)+sqr(bisector^pts[i]))/(2*x));
		}
		g_centers[nCircles] = center+bisector*r;
		g_radii[nCircles++] = r;

		// remove all vertices that lie inside (or close enough to) this new circle
		for(i=nSkipped=0,pvtx=plist; i<npt; i++,pvtx=pvtx->next)
		if (len2(pvtx->pt+center-g_centers[nCircles-1])<r*1.1f) {
			pvtx->next->prev = pvtx->prev; pvtx->prev->next = pvtx->next;
			nSkipped++;
			if (pvtx==plist) {
				if (pvtx->prev!=pvtx)
					plist = pvtx->prev;
				else 
					goto allcircles;
			}
		}
		npt -= nSkipped;
	} while(nSkipped && nCircles<CRY_ARRAY_COUNT(g_centers));

	allcircles:
	delete[] pdata;
	return nCircles;
}

void WritePacked(CStream &stm, int num)
{
	int i; for(i=0;i<16 && (unsigned int)num>=1u<<i*2;i++);
	stm.WriteNumberInBits(i,5);
	if (i>0)
		stm.WriteNumberInBits(num,i*2);
}
void WritePacked(CStream &stm, uint64 num)
{
	WritePacked(stm,(int)(num & 0xFFFFFFFF));
	WritePacked(stm,(int)(num>>32 & 0xFFFFFFFF));
}

void ReadPacked(CStream &stm,int &num)
{
	int i; num=0;
	stm.ReadNumberInBits(i,5);
	if(i>16)
		CryFatalError("ReadPacked i==%i", i);
	if (i>0)
		stm.ReadNumberInBits(num,i*2);
}
void ReadPacked(CStream &stm,uint64 &num)
{
	int ilo,ihi;
	ReadPacked(stm,ilo);
	ReadPacked(stm,ihi);
	num = (uint64)(unsigned int)ihi<<32 | (uint64)(unsigned int)ilo;
}

void WriteCompressedPos(CStream &stm, const Vec3 &pos, bool bCompress)
{
	if(!inrange(pos.x,0.0f,2048.0f) || !inrange(pos.y,0.0f,2048.0f) || !inrange(pos.z,0.0f,512.0f) || !bCompress) {
		stm.Write(false);
		stm.Write(pos);
	} else {
		stm.Write(true);
		stm.WriteNumberInBits(float2int(pos.x*512.0f),20);
		stm.WriteNumberInBits(float2int(pos.y*512.0f),20);
		stm.WriteNumberInBits(float2int(pos.z*512.0f),18);
	}
	//stm.WritePkd(CStreamData_WorldPos(const_cast<Vec3&>(pos)));
}
void ReadCompressedPos(CStream &stm, Vec3 &pos, bool &bWasCompressed)
{
	stm.Read(bWasCompressed);
	if (bWasCompressed) {
		unsigned int ix,iy,iz;
		stm.ReadNumberInBits(ix, 20); pos.x = ix*(1.0f/512);
		stm.ReadNumberInBits(iy, 20); pos.y = iy*(1.0f/512);
		stm.ReadNumberInBits(iz, 18); pos.z = iz*(1.0f/512);
	} else
		stm.Read(pos);
	//stm.ReadPkd(CStreamData_WorldPos(pos));
}
Vec3 CompressPos(const Vec3 &pos)
{
	if(!inrange(pos.x,0.0f,2048.0f) || !inrange(pos.y,0.0f,2048.0f) || !inrange(pos.z,0.0f,512.0f))
		return pos;
	return Vec3(float2int(pos.x*512.0f)*(1.0f/512), float2int(pos.y*512.0f)*(1.0f/512), float2int(pos.z*512.0f)*(1.0f/512));
	//return CStreamData_WorldPos(const_cast<Vec3&>(pos)).GetCompressed();
}

bool getCompressedQuat(const quaternionf &q, Vec3_tpl<short> &res)
{
	Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(q));
	bool bGimbalLocked;
	if (bGimbalLocked = fabs_tpl(angles.y)>g_PI*0.5f-0.03f)
		angles = Ang3::GetAnglesXYZ(Matrix33(q*Quat::CreateRotationAA((float)g_PI/6,Vec3(0,1,0))));
	res.x = max(-32768,min(32767,float2int(angles.x*(32767/g_PI))));
	res.y = max(-32768,min(32767,float2int(angles.y*(32767/(g_PI*0.5f)))));
	res.z = max(-32768,min(32767,float2int(angles.z*(32767/g_PI))));
	return bGimbalLocked;
}
void WriteCompressedQuat(CStream &stm, const quaternionf &q)
{
	Vec3_tpl<short> sangles;
	stm.Write(getCompressedQuat(q,sangles));
	stm.Write(sangles.x); stm.Write(sangles.y); stm.Write(sangles.z);
}
void ReadCompressedQuat(CStream &stm,quaternionf &q)
{
	bool bGimbalLocked; stm.Read(bGimbalLocked);
	Vec3_tpl<short> sangles; stm.Read(sangles.x); stm.Read(sangles.y); stm.Read(sangles.z);
	q = Quat::CreateRotationXYZ( Ang3(sangles.x*(g_PI/32767),sangles.y*(g_PI*0.5f/32767),sangles.z*(g_PI/32767)));
	if (bGimbalLocked)
		q *= Quat::CreateRotationAA((float)-g_PI/6,Vec3(0,1,0));
}
quaternionf CompressQuat(const quaternionf &q)
{
	Vec3_tpl<short> sangles;
	bool bGimbalLocked = getCompressedQuat(q,sangles);
	quaternionf qres = Quat::CreateRotationXYZ(Ang3(sangles.x*(g_PI/32767),sangles.y*(g_PI*0.5f/32767),sangles.z*(g_PI/32767)));
	if (bGimbalLocked)
		qres *= Quat::CreateRotationAA((float)-g_PI/6,Vec3(0,1,0));
	return qres;
}

template <int npt>
class Rasteriser
{
public:
	static ILINE int Inside(const int* isum)
	{
		if (npt==3)	return isum[0] | isum[1] | isum[2];
		if (npt==4)	return isum[0] | isum[1] | isum[2] | isum[3];
		if (npt==8)	return isum[0] | isum[1] | isum[2] | isum[3] | isum[4] | isum[5] | isum[6] | isum[7];
		int inside = 0;
		for (int i=0; i<npt; i++) inside |= isum[i];
		return inside;  // inside>=0 then the point is inside the polygon frustrum
	}

	static ILINE void Update(int* isum, const int* inc, int step)
	{
		if (npt==3) {
			isum[0] += inc[0*step]; isum[1] += inc[1*step]; isum[2] += inc[2*step];
		} else if (npt==4) {
			isum[0] += inc[0*step]; isum[1] += inc[1*step]; isum[2] += inc[2*step]; isum[3] += inc[3*step];
		} else if (npt==8) {
			isum[0] += inc[0*step]; isum[1] += inc[1*step]; isum[2] += inc[2*step]; isum[3] += inc[3*step];
			isum[4] += inc[4*step]; isum[5] += inc[5*step]; isum[6] += inc[6*step]; isum[7] += inc[7*step];
		} else for (int i=0; i<npt; i++) isum[i] += inc[i*step];
	}

	static void ILINE ProcessBounds(float p0x, float p0z, float p1x, float p1z, float& xMin, float& xMax)
	{
		/* This intersects the segments against the z=+x and z=-x planes
		 * If the intersection has a lambda>=0 && lambda<=1 && z-value>0
		 * then xMin=-1 or xMax=+1 is set accordingly
		 * For the z=-x plane, lam1 = (-p0x-p0z)/d1
		 * For the z=+x plane, lam2 = (-p0x+p0z)/d2
		 */
		const float dx = p1x - p0x, dz = p1z - p0z;
		const float d1 = dx + dz, d2 = dx - dz;
		const float e1 = -p0x - p0z, e2 = -p0x + p0z;
		const float test1 = (float)__fsel(e1, d1-e1, e1-d1);
		const float test2 = (float)__fsel(e2, d2-e2, e2-d2);
		const float zd1 = (p0z*d1 + e1*dz);
		const float zd2 = (p0z*d2 + e2*dz);
		if (test1>0.f && (float)__fsel(d1,zd1,-zd1)>0.f) xMin= -1.f;
		if (test2>0.f && (float)__fsel(d2,zd2,-zd2)>0.f) xMax= +1.f;
		if (p0z>0.f) { // Only use the point if its z is positive
			float x = p0x * (float)__fres(fabsf(p0z)+1e-6f);
			xMin = min(x, xMin); xMax = max(x, xMax);
		}
	}


	static void Raster(const Vec3 *pt, int iPass, SOcclusionCubeMap* cubemap)
	{
		enum {maxPoints=8};
		const float scaleSign[2] = {-1.f, +1.f};
		const int N = cubemap->N;
		const int irmin = cubemap->irmin;
		int planeUsed=0, nCells=0;
		Vec3i inormal[maxPoints];
		int isum[maxPoints];
		int isumRewind[maxPoints];
		Vec3 bb[2] = {Vec3(+1e8f), Vec3(-1e8f)};
		Vec3i ibb[2];
		const Vec3 d0 = pt[1] - pt[0], d1 = pt[2] - pt[1];
		Vec3 planeNorm = (iPass==0) ? d1.cross(d0) : d0.cross(d1);		// NB a valid face on iPass==0 points away from the origin

		if (planeNorm.dot(pt[0]) <= 0.f)
			return;

		// Temp data buffers
		floatint* temp = (floatint*)(cubemap->scratch[0]);
		int* cells = cubemap->scratch[1];

		// z-distance = plane0/planeNorm.dot(position on cubeFace)
		float plane0 = planeNorm.dot(pt[0]) * cubemap->zscale * cubemap->halfN;
		planeNorm *= 1.f/plane0;	// Dont use __fres here, nust preserve accuracy!

		for (int i=0; i<npt; i++) {
			Vec3 tmp, normal;
			int i1 = negmask(i+1-npt)&(i+1);
			tmp = pt[i]*2048.f;
			bb[0] = min(bb[0], tmp);
			bb[1] = max(bb[1], tmp);
			normal = pt[i1].cross(pt[i]);
			normal *= scaleSign[1-iPass]*65535.f*(float)__fres(1e-6f + fabsf(normal.x) + fabsf(normal.y) + fabsf(normal.z));
			inormal[i] = Vec3i((int)normal.z, (int)normal.y, (int)normal.x);
		}
		
		ibb[0] = Vec3i((int)bb[0].x, (int)bb[0].y, (int)bb[0].z);
		ibb[1] = Vec3i((int)bb[1].x, (int)bb[1].y, (int)bb[1].z);

		// Project onto each face to discover the bounding area - this is split to minimise the load-hit-stores
		int minX[6], minY[6], maxX[6], maxY[6];
		for (int iPlane=0, planeBit=1; iPlane<6; iPlane++, planeBit<<=1) {
			const float scale = scaleSign[iPlane&1];
			const int iscale = ((iPlane&1)<<1) - 1;
			const int az = iPlane>>1, ay = dec_mod3[az], ax = inc_mod3[az];
			const int imaxZ = ibb[iPlane&1][az] * iscale;
			const int iminZ = ibb[1^(iPlane&1)][az] * iscale;
			int outofbounds = negmask((imaxZ-1) | (imaxZ-ibb[0][ax]) | (imaxZ+ibb[1][ax]) | (imaxZ-ibb[0][ay]) | (imaxZ+ibb[1][ay]));
			if (outofbounds) continue;
			planeUsed |= planeBit;
			float ixMin=1.f, ixMax=-1.f, iyMin=1.f, iyMax=-1.f;
			IF (iminZ>0, 1) { // Simple projection test to discover the bounds :)
				for (int i=0; i<npt; i++) {
					float z = (float)__fres(fabsf(pt[i][az])+1e-6f), x = pt[i][ax]*z, y = pt[i][ay]*z;
					ixMin = min(x, ixMin); ixMax = max(x, ixMax);
					iyMin = min(y, iyMin); iyMax = max(y, iyMax);
				}
			} else { // Use a more extensive, line-intersection method for discovering the bounds (RARE)
				for (int i=0; i<npt; i++) {
					int i1 = negmask(i+1-npt)&(i+1);
					ProcessBounds(pt[i][ax], pt[i][az]*scale, pt[i1][ax], pt[i1][az]*scale, ixMin, ixMax);
					ProcessBounds(pt[i][ay], pt[i][az]*scale, pt[i1][ay], pt[i1][az]*scale, iyMin, iyMax);
				}
			}
			ixMin = (ixMin+1.f)*cubemap->halfN; ixMax = (ixMax+1.f)*cubemap->halfN;
			iyMin = (iyMin+1.f)*cubemap->halfN; iyMax = (iyMax+1.f)*cubemap->halfN;
			minX[iPlane] = (int)(ixMin); maxX[iPlane] = (int)(ixMax);
			minY[iPlane] = (int)(iyMin); maxY[iPlane] = (int)(iyMax);
		}
			
		for (int iPlane=0, planeBit=1; iPlane<6; iPlane++, planeBit<<=1) if (planeUsed & planeBit) {
			int* rawdata = cubemap->grid[iPlane];
			const int iscale = ((iPlane&1)<<1) - 1;
			const float scale = scaleSign[iPlane&1];
			const int az = iPlane>>1, ay = dec_mod3[az], ax = inc_mod3[az];
			const int ix0 = max_fast((int)0, (int)minX[iPlane]);
			const int iy0 = max_fast((int)0, (int)minY[iPlane]);
			const int ixEnd = min_fast((int)N-1, (int)maxX[iPlane]);
			const int iyEnd = min_fast((int)N-1, (int)maxY[iPlane]);
			float dot = cubemap->halfN*(-planeNorm[ax] - planeNorm[ay] + scale*planeNorm[az]) + ((float)ix0*planeNorm[ax] + (float)iy0*planeNorm[ay]);
			const float dotDx = planeNorm[ax], dotDy = planeNorm[ay];
			int n=0;
			for (int i=0;i<npt;i++) {
				isum[i] = ((N*(-inormal[i][ax] - inormal[i][ay] + iscale*inormal[i][az]))>>1) + ix0*inormal[i][ax] + iy0*inormal[i][ay];
				isumRewind[i] = inormal[i][ay] - (ixEnd-ix0+1) * inormal[i][ax];
			}
			for (int iy=iy0; iy<=iyEnd; iy++) {
				int cell = iy*N + ix0;
				float d = dot;
				for (int ix=ix0; ix<=ixEnd; ix++, cell++)
				{
					temp[n].fval = d;
					cells[n] = cell;
					n+=1+negmask(Inside(isum));  // if inside>=0 increase n
					Update(isum, &inormal[0][ax], 3);
					d += dotDx;
				}
				Update(isum, isumRewind, 1);
				dot += dotDy;
			}
			// 1st (front face) pass output: 
			//  iz<rmin - set the highest bit
			//  else - update z value in the lower 30 bits
			// 2nd (back face) pass output:
			//  iz<rmin - clear the highest bit
			//  else - update z value in the lower 30 bits
			// after both passes: 
			//  if the highest bit is set, change cell z to irmin
			if (iPass==0) for (int i=0; i<n; i++) {    // This loop is split from the previous to mitigate LHS on consoles
				int iz = temp[i].ival;                 // Just use the integer representation of the denominator (no need to do a divide!) instead reverse the comparisons
				int mask = negmask(iz-irmin);          // mask = 0 if closer than rmin limit
				iz = iz&mask;
				rawdata[cells[i]] = max_fast(rawdata[cells[i]]&((1u<<31)-1),iz) | ((1u<<31)&~mask);
			}
			else for (int i=0; i<n; i++) {
				int iz = temp[i].ival;
				int mask = negmask(iz-irmin);
				iz = iz&mask;
				rawdata[cells[i]] = max_fast(rawdata[cells[i]]&((1u<<31)-1),iz) | ((1u<<31)&rawdata[cells[i]]&mask);
			}
			nCells += n;
		}
		cubemap->usedPlanes |= planeUsed;

		if (nCells==0) { // do not allow objects to take no rasterized space
			int iPlane = GetProjCubePlane(pt[0]);
			const int az = iPlane>>1, ay = dec_mod3[az], ax = inc_mod3[az];
			int* rawdata = cubemap->grid[iPlane];
			const float scale = scaleSign[iPlane&1];
			float z = 1.f/(fabsf(pt[0][az])+1e-6f);
			int ix0 = min(N-1,max(0, float2int((pt[0][ax]*z+1.f)*cubemap->halfN)));
			int iy0 = min(N-1,max(0, float2int((pt[0][ay]*z+1.f)*cubemap->halfN)));
			int cell = iy0*N + ix0;
			floatint tmp; tmp.fval = z*cubemap->rmax*(1.f/65535.f);
			int iz = tmp.ival;
			int mask = negmask(iz-irmin);
			iz = iz&mask;
			if (iPass==0)
				rawdata[cell] = max_fast(rawdata[cell]&((1u<<31)-1),iz) | ((1u<<31)&~mask);
			else
				rawdata[cell] = max_fast(rawdata[cell]&((1u<<31)-1),iz) | ((1u<<31)&rawdata[cell]&mask);
			cubemap->usedPlanes |= 1 << iPlane;
		}
	}
};

void RasterizePolygonIntoCubemap(const Vec3 *pt,int npt, int iPass, SOcclusionCubeMap* cubemap)
{
	if (cubemap->bMode==0) RasterizePolygonIntoCubemap(pt, npt, iPass, cubemap->grid, cubemap->N, cubemap->rmin, cubemap->rmax, cubemap->zscale);
	else if (npt==3) Rasteriser<3>::Raster(pt, iPass, cubemap);
	else if (npt==4) Rasteriser<4>::Raster(pt, iPass, cubemap);
	else if (npt==8) Rasteriser<8>::Raster(pt, iPass, cubemap);
	else CryFatalError("Unsupported polygon size for Occlusion-Rasteriser");
}


SOcclusionCubeMap::SOcclusionCubeMap()
	: N(0)
	, rmax(0.0f)
	, rmin(0.0f)
	, halfN(0.0f)
	, zscale(0.0f)
	, irmin(0)
	, allocation(nullptr)
	, usedPlanes(0)
{	
	ZeroArray(grid);
	ZeroArray(scratch);
	
#if !defined(_RELEASE) && !defined(STANDALONE_PHYSICS)
	for (int i=0; i<CRY_ARRAY_COUNT(debugTextureId); i++)
		debugTextureId[i] = -1;
#endif
}

SOcclusionCubeMap::~SOcclusionCubeMap()
{
	Free();
}

void SOcclusionCubeMap::Init(int __N, float __rmin, float __rmax)
{
	float fN = (float)__N;
	rmax = __rmax;
	rmin = __rmin;
	halfN = 0.5f*fN;
	zscale = 65535.f/__rmax;
	floatint tmp; tmp.fval = 1.f/max(1.f,__rmin*zscale);
	irmin = tmp.ival;
	if (N != __N) {
		Free();
		Allocate(N=__N);
	}
}

void SOcclusionCubeMap::Free()
{
	N = 0;
	if (allocation) { delete [] allocation; allocation = NULL; grid[0] = NULL; }
	FreeTextures();
}

void SOcclusionCubeMap::Allocate(int n)
{
	assert(grid[0]==NULL);
	allocation = new int[16/sizeof(int)+8*sqr(N)]; // Store an extra two buffers, which are needed by the rasteriser
	int* base = (int*)((((intptr_t)allocation) + 15)&~15); // alignup to 16
	for (int i=0; i<6; i++)
		grid[i] = base + sqr(N)*i;
	scratch[0] = grid[0] + 6*sqr(N);
	scratch[1] = grid[0] + 7*sqr(N);
}

void SOcclusionCubeMap::Reset()
{
	if (bMode)
	{
		for (int i=0; i<6; i++)
			memset(grid[i], 0, sizeof(grid[0][0])*sqr(N));
	}
	else
	{
		for (int i=0; i<6; i++)
			for (int j=0; j<sqr(N); j++)
				grid[i][j] = (1u<<31)-1;
	}
	usedPlanes = -iszero(bMode); // mode==1 ? 0 : -1
}

void SOcclusionCubeMap::ProcessRmin()
{
 	// Scan the map and set cell to irmin if highest bit is set
 	// NB, the six faces are stored contiguously.
	int v = bMode ? irmin : float2int(rmin*zscale);
	// Using a manually un-rolled loop - 30 % faster
 	int count = sqr(N)*6;         
 	int * __restrict rawdata = &grid[0][0];
 	int mask, n = (count + 7) >> 3;
 	switch(count & 7) {
 	case 0:  do {  mask = *rawdata>>31; *rawdata = v&mask | *rawdata&~mask; rawdata++;
 	case 7:        mask = *rawdata>>31; *rawdata = v&mask | *rawdata&~mask; rawdata++;
 	case 6:        mask = *rawdata>>31; *rawdata = v&mask | *rawdata&~mask; rawdata++;
 	case 5:        mask = *rawdata>>31; *rawdata = v&mask | *rawdata&~mask; rawdata++;
 	case 4:        mask = *rawdata>>31; *rawdata = v&mask | *rawdata&~mask; rawdata++;
 	case 3:        mask = *rawdata>>31; *rawdata = v&mask | *rawdata&~mask; rawdata++;
 	case 2:        mask = *rawdata>>31; *rawdata = v&mask | *rawdata&~mask; rawdata++;
 	case 1:        mask = *rawdata>>31; *rawdata = v&mask | *rawdata&~mask; rawdata++;
 			} while(--n > 0);
 	}
}
	
void SOcclusionCubeMap::GetMemoryStatistics(ICrySizer *pSizer)
{
	pSizer->AddObject(this, sizeof(*this));
	if (N>0) pSizer->AddObject(allocation, sizeof(int)*16/sizeof(int)+8*sqr(N));
}

#if !defined(_RELEASE) && !defined(STANDALONE_PHYSICS)
void SOcclusionCubeMap::FreeTextures() {}
void SOcclusionCubeMap::DebugDrawToScreen(float xpos, float ypos, float imageWidth)	{}
#endif

/*#if !defined(_RELEASE) && !defined(STANDALONE_PHYSICS)
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>

void SOcclusionCubeMap::FreeTextures()
{
	for (int i=0; i<6; i++) {
		if (debugTextureId[i]>=0) {
			gEnv->pRenderer->RemoveTexture(debugTextureId[i]);
			debugTextureId[i] = -1;
		}
	}
}

void SOcclusionCubeMap::DebugDrawToScreen(float xpos, float ypos, float imageWidth)
{
	uint8* debugData = new uint8[sqr(N)*6];
	
	// Map for converting cube map coordinates to sensible, contiguous texture coordinates
	int map[6][4] = { {0,0,1,N}, {N-1,0,-1,N}, {0,N-1,N,-1}, {0,0,N,1}, {0,0,1,N}, {0,N*(N-1),1,-N} };
	for (int i=0; i<6; i++)
	{
		int x0 = map[i][0], y0 = map[i][1], dx = map[i][2], dy = map[i][3];
		uint8* rawdata = &debugData[i*sqr(N)];
		int y = y0;
		for (int iy=0; iy<N; iy++, y+=dy) {
			int x = x0; 
			for (int ix=0 ; ix<N; ix++, x+=dx) {
				uint8 p = 0;
				int z = grid[i][iy*N + ix];
				if (bMode)  // Remap the values
				{
					floatint tmp; tmp.ival = z;
					z = (int)(clamp_tpl(1.f/max(1e-6f,tmp.fval), 0.f, 65535.f));
				}
				p = (255 - min(255,(z >> 8)));
				rawdata[y+x] = p;
			}
		}
	}

	///////////////////////////////////////////////
	// Draw:              +z                     //
	//            -x  -y  +x  +y                 //
	//                    -z                     //
	///////////////////////////////////////////////
	for (int i=0; i<6; i++)
	{
		uint8* rawdata = &debugData[i*sqr(N)];
		if (debugTextureId[i]==-1)
			debugTextureId[i] = gEnv->pRenderer->DownLoadToVideoMemory(rawdata, N, N, eTF_L8, eTF_L8, 1, false, FILTER_NONE, 0);
		else
			gEnv->pRenderer->UpdateTextureInVideoMemory(debugTextureId[i], rawdata, 0, 0, N, N, eTF_L8);
	}
	int screenW = gEnv->pRenderer->GetWidth();
	int screenH = gEnv->pRenderer->GetHeight();
	gEnv->pRenderer->SetState(GS_NODEPTHTEST);
	gEnv->pRenderer->Set2DMode(true, screenW, screenH);
	float spacing = imageWidth + 5.f;
	gEnv->pRenderer->DrawImage(xpos + spacing*0.f, ypos, imageWidth, imageWidth, debugTextureId[2], 0.f,0.f,1.f,1.f,  1.f,1.f,1.f,1.f,  false);
	gEnv->pRenderer->DrawImage(xpos + spacing*1.f, ypos, imageWidth, imageWidth, debugTextureId[0], 0.f,0.f,1.f,1.f,  1.f,1.f,1.f,1.f,  false);
	gEnv->pRenderer->DrawImage(xpos + spacing*2.f, ypos, imageWidth, imageWidth, debugTextureId[3], 0.f,0.f,1.f,1.f,  1.f,1.f,1.f,1.f,  false);
	gEnv->pRenderer->DrawImage(xpos + spacing*3.f, ypos, imageWidth, imageWidth, debugTextureId[1], 0.f,0.f,1.f,1.f,  1.f,1.f,1.f,1.f,  false);
	gEnv->pRenderer->DrawImage(xpos + spacing*2.f, ypos + spacing, imageWidth, imageWidth, debugTextureId[4], 0.f,0.f,1.f,1.f,  1.f,1.f,1.f,1.f,  false);
	gEnv->pRenderer->DrawImage(xpos + spacing*2.f, ypos - spacing, imageWidth, imageWidth, debugTextureId[5], 0.f,0.f,1.f,1.f,  1.f,1.f,1.f,1.f,  false);
	gEnv->pRenderer->Set2DMode(false,screenW, screenH);

	// Can free the memory as the renderer copies it to its own buffers
	delete [] debugData;
}

#endif  // _RELEASE
*/

struct vtxthunk {
	vtxthunk *next[2];
	vtxthunk *jump;
	Vec2 *pt;
	int bProcessed;
};
int g_bBruteforceTriangulation = 0, g_nTriangulationErrors = 0;

int TriangulatePolyBruteforce(Vec2 *pVtx, int nVtx, int *pTris, int szTriBuf)
{
	int i, nThunks, nNonEars, nTris = 0;
	vtxthunk *ptr, *ptr0, bufThunks[32], *pThunks = nVtx <= 31 ? bufThunks : new vtxthunk[nVtx + 1];

	ptr = ptr0 = pThunks;
	for (i = nThunks = 0; i < nVtx; i++) if (!is_unused(pVtx[i].x)) {
		pThunks[nThunks].next[0] = pThunks + nThunks - 1;
		pThunks[nThunks].next[1] = pThunks + nThunks + 1;
		pThunks[nThunks].pt = pVtx + i;
		ptr = pThunks + nThunks++;
	}
	if (nThunks < 3)
		return 0;
	ptr->next[1] = ptr0; ptr0->next[0] = ptr;
	for (i = 0; i < nThunks; i++)
		pThunks[i].bProcessed = (*pThunks[i].next[1]->pt - *pThunks[i].pt ^ *pThunks[i].next[0]->pt - *pThunks[i].pt) > 0;

	for (nNonEars = 0; nNonEars < nThunks && nTris < szTriBuf; ptr0 = ptr0->next[1]) {
		if (nThunks == 3) {
			pTris[nTris * 3] = ptr0->pt - pVtx; pTris[nTris * 3 + 1] = ptr0->next[1]->pt - pVtx;
			pTris[nTris * 3 + 2] = ptr0->next[0]->pt - pVtx; nTris++;
			break;
		}
		for (i = 0; (*ptr0->next[1]->pt - *ptr0->pt^*ptr0->next[0]->pt - *ptr0->pt) < 0 && i < nThunks; ptr0 = ptr0->next[1], i++);
		if (i == nThunks)
			break;
		for (ptr = ptr0->next[1]->next[1]; ptr != ptr0->next[0] && ptr->bProcessed; ptr = ptr->next[1]);	// find the 1st non-convex vertex after ptr0
		for (; ptr != ptr0->next[0] && min(min(*ptr0->pt - *ptr0->next[0]->pt ^ *ptr->pt - *ptr0->next[0]->pt,
			*ptr0->next[1]->pt - *ptr0->pt ^ *ptr->pt - *ptr0->pt),
			*ptr0->next[0]->pt - *ptr0->next[1]->pt ^ *ptr->pt - *ptr0->next[1]->pt) < 0; ptr = ptr->next[1]);
		if (ptr == ptr0->next[0]) { // vertex is an ear, output the corresponding triangle
			pTris[nTris * 3] = ptr0->pt - pVtx; pTris[nTris * 3 + 1] = ptr0->next[1]->pt - pVtx;
			pTris[nTris * 3 + 2] = ptr0->next[0]->pt - pVtx;	nTris++;
			ptr0->next[1]->next[0] = ptr0->next[0];
			ptr0->next[0]->next[1] = ptr0->next[1];
			nThunks--; nNonEars = 0;
		}
		else
			nNonEars++;
	}

	if (pThunks != bufThunks) delete[] pThunks;
	return nTris;
}

int TriangulatePoly(Vec2 *pVtx, int nVtx, int *pTris, int szTriBuf)
{
	if (nVtx < 3)
		return 0;
	vtxthunk *pThunks, *pPrevThunk, *pContStart, **pSags, **pBottoms, *pPinnacle, *pBounds[2], *pPrevBounds[2], *ptr, *ptr_next;
	vtxthunk bufThunks[32], *bufSags[16], *bufBottoms[16];
	int i, nThunks, nBottoms = 0, nSags = 0, iBottom = 0, nConts = 0, j, isag, nThunks0, nTris = 0, nPrevSags, nTrisCnt, iter, nDegenTris = 0;
	float ymax, ymin, e, area0 = 0, area1 = 0, cntarea, minCntArea;

	isag = is_unused(pVtx[0].x); ymin = ymax = pVtx[isag].y;
	for (i = isag; i < nVtx; i++) if (!is_unused(pVtx[i].x)) {
		ymin = min(ymin, pVtx[i].y); ymax = max(ymax, pVtx[i].y);
	}
	e = (ymax - ymin)*0.0005f;
	for (i = 1 + isag; i < nVtx; i++) if (!is_unused(pVtx[i].x)) {
		j = i < nVtx - 1 && !is_unused(pVtx[i + 1].x) ? i + 1 : isag;
		if ((ymin = min(pVtx[j].y, pVtx[i - 1].y)) > pVtx[i].y - e)
			if ((pVtx[j] - pVtx[i] ^ pVtx[i - 1] - pVtx[i]) > 0)
				nBottoms++; // we have a bottom
			else if (ymin > pVtx[i].y + 1E-8f)
				nSags++;	// we have a sag
	}
	else {
		nConts++; isag = ++i;
	}
	nSags += nConts;
	if (nConts - 2 >> 31 & g_bBruteforceTriangulation)
		return TriangulatePolyBruteforce(pVtx, nVtx, pTris, szTriBuf);
	pThunks = nVtx + nSags * 2 <= CRY_ARRAY_COUNT(bufThunks) ? bufThunks : new vtxthunk[nVtx + nSags * 2];

	for (i = nThunks = 0, pContStart = pPrevThunk = pThunks; i < nVtx; i++) if (!is_unused(pVtx[i].x)) {
		pThunks[nThunks].next[1] = pThunks + nThunks;
		pThunks[nThunks].next[1] = pPrevThunk->next[1];
		pPrevThunk->next[1] = pThunks + nThunks;
		pThunks[nThunks].next[0] = pPrevThunk;
		pThunks[nThunks].jump = 0;
		pPrevThunk = pThunks + nThunks;
		pThunks[nThunks].bProcessed = 0;
		pThunks[nThunks++].pt = &pVtx[i];
	}
	else {
		pPrevThunk->next[1] = pContStart;
		pContStart->next[0] = pThunks + nThunks - 1;
		pContStart = pPrevThunk = pThunks + nThunks;
	}

	for (i = j = 0, cntarea = 0, minCntArea = 1; i < nThunks; i++) {
		cntarea += *pThunks[i].pt ^ *pThunks[i].next[1]->pt; j++;
		if (pThunks[i].next[1] != pThunks + i + 1) {
			if (j >= 3) {
				area0 += cntarea;
				minCntArea = min(cntarea, minCntArea);
			}
			cntarea = 0; j = 0;
		}
	}
	if (minCntArea > 0 && nConts > 1) {
		// if all contours are positive, triangulate them as separate (it's more safe)
		for (i = 0; i < nThunks; i++) if (pThunks[i].next[0] != pThunks + i - 1) {
			nTrisCnt = TriangulatePoly(pThunks[i].pt, (pThunks[i].next[0]->pt - pThunks[i].pt) + 2, pTris + nTris * 3, szTriBuf - nTris * 3);
			for (j = 0, isag = pThunks[i].pt - pVtx; j < nTrisCnt * 3; j++)
				pTris[nTris * 3 + j] += isag;
			i = pThunks[i].next[0] - pThunks;	nTris += nTrisCnt;
		}
		if (pThunks != bufThunks) delete[] pThunks;
		return nTris;
	}

	pSags = nSags <= CRY_ARRAY_COUNT(bufSags) ? bufSags : new vtxthunk*[nSags];
	pBottoms = nSags + nBottoms <= CRY_ARRAY_COUNT(bufBottoms) ? bufBottoms : new vtxthunk*[nSags + nBottoms];

	for (i = nSags = nBottoms = 0; i < nThunks; i++) {
		if ((ymin = min(pThunks[i].next[1]->pt->y, pThunks[i].next[0]->pt->y)) > pThunks[i].pt->y - e)
			if ((*pThunks[i].next[1]->pt - *pThunks[i].pt ^ *pThunks[i].next[0]->pt - *pThunks[i].pt) >= 0)
				pBottoms[nBottoms++] = pThunks + i; // we have a bottom
			else if (ymin > pThunks[i].pt->y + e)
				pSags[nSags++] = pThunks + i;	// we have a sag
	}
	iBottom = -1; pBounds[0] = pBounds[1] = pPrevBounds[0] = pPrevBounds[1] = 0;
	nThunks0 = nThunks; nPrevSags = nSags; iter = nThunks * 4;

	do {
	nextiter:
		if (!pBounds[0]) { // if bounds are empty, get the next available bottom
			for (++iBottom; iBottom < nBottoms && !pBottoms[iBottom]->next[0]; iBottom++);
			if (iBottom >= nBottoms) break;
			pBounds[0] = pBounds[1] = pPinnacle = pBottoms[iBottom];
		}
		pBounds[0]->bProcessed = pBounds[1]->bProcessed = 1;
		if (pBounds[0] == pPrevBounds[0] && pBounds[1] == pPrevBounds[1] && nSags == nPrevSags || !pBounds[0]->next[0] || !pBounds[1]->next[0]) {
			pBounds[0] = pBounds[1] = 0; continue;
		}
		pPrevBounds[0] = pBounds[0]; pPrevBounds[1] = pBounds[1];	nPrevSags = nSags;

		// check if left or right is a top
		for (i = 0; i < 2; i++)
			if (pBounds[i]->next[0]->pt->y < pBounds[i]->pt->y && pBounds[i]->next[1]->pt->y <= pBounds[i]->pt->y &&
				(*pBounds[i]->next[0]->pt - *pBounds[i]->pt ^ *pBounds[i]->next[1]->pt - *pBounds[i]->pt) > 0)
			{
				if (pBounds[i]->jump) do {
					ptr = pBounds[i]->jump;	pBounds[i]->jump = 0; pBounds[i] = ptr;
				} while (pBounds[i]->jump);
				else {
					pBounds[i]->jump = pBounds[i ^ 1];
					pBounds[0] = pBounds[1] = 0; goto nextiter;
				}
				if (!pBounds[0]->next[0] || !pBounds[1]->next[0]) {
					pBounds[0] = pBounds[1] = 0; goto nextiter;
				}
			}
		i = isneg(pBounds[1]->next[1]->pt->y - pBounds[0]->next[0]->pt->y);
		ymax = pBounds[i ^ 1]->next[i ^ 1]->pt->y;
		ymin = min(pBounds[0]->pt->y, pBounds[1]->pt->y);

		for (j = 0, isag = -1; j < nSags; j++) if (inrange(pSags[j]->pt->y, ymin, ymax) && // find a sag in next left-left-right-next right quad
			pSags[j] != pBounds[0]->next[0] && pSags[j] != pBounds[1]->next[1] &&
			(*pBounds[0]->pt - *pBounds[0]->next[0]->pt ^ *pSags[j]->pt - *pBounds[0]->next[0]->pt) >= 0 &&
			(*pBounds[1]->pt - *pBounds[0]->pt ^ *pSags[j]->pt - *pBounds[0]->pt) >= 0 &&
			(*pBounds[1]->next[1]->pt - *pBounds[1]->pt ^ *pSags[j]->pt - *pBounds[1]->pt) >= 0 &&
			(*pBounds[0]->next[0]->pt - *pBounds[1]->next[1]->pt ^ *pSags[j]->pt - *pBounds[1]->next[1]->pt) >= 0)
		{
			ymax = pSags[j]->pt->y; isag = j;
		}

		if (isag >= 0) { // build a bridge between the sag and the highest active point
			if (pSags[isag]->next[0]) {
				pPinnacle->next[1]->next[0] = pThunks + nThunks; pSags[isag]->next[0]->next[1] = pThunks + nThunks + 1;
				pThunks[nThunks].next[0] = pThunks + nThunks + 1;	pThunks[nThunks].next[1] = pPinnacle->next[1];
				pThunks[nThunks + 1].next[1] = pThunks + nThunks;	pThunks[nThunks + 1].next[0] = pSags[isag]->next[0];
				pPinnacle->next[1] = pSags[isag]; pSags[isag]->next[0] = pPinnacle;
				pThunks[nThunks].pt = pPinnacle->pt; pThunks[nThunks + 1].pt = pSags[isag]->pt;
				pThunks[nThunks].jump = pThunks[nThunks + 1].jump = 0;
				pThunks[nThunks].bProcessed = pThunks[nThunks + 1].bProcessed = 0;
				if (pBounds[1] == pPinnacle)
					pBounds[1] = pThunks + nThunks;
				for (ptr = pThunks + nThunks, j = 0; ptr != pBounds[1]->next[1] && j < nThunks; ptr = ptr->next[1], j++)
					if (min(ptr->next[0]->pt->y, ptr->next[1]->pt->y) > ptr->pt->y) { // ptr is a bottom
						pBottoms[nBottoms++] = ptr; break;
					}
				pBounds[1] = pPinnacle;	pPinnacle = pSags[isag];
				nThunks += 2;
			}
			for (j = isag; j < nSags - 1; j++) pSags[j] = pSags[j + 1];
			--nSags;
			continue;
		}

		// create triangles featuring the new vertex
		for (ptr = pBounds[i]; ptr != pBounds[i ^ 1] && nTris < szTriBuf; ptr = ptr_next)
			if ((*ptr->next[i ^ 1]->pt - *ptr->pt ^ *ptr->next[i]->pt - *ptr->pt)*(1 - i * 2) > 0 || pBounds[0]->next[0] == pBounds[1]->next[1]) {
				// output the triangle
				pTris[nTris * 3] = pBounds[i]->next[i]->pt - pVtx; pTris[nTris * 3 + 1 + i] = ptr->pt - pVtx;
				pTris[nTris * 3 + 2 - i] = ptr->next[i ^ 1]->pt - pVtx;
				Vec2 edge0 = pVtx[pTris[nTris * 3 + 1]] - pVtx[pTris[nTris * 3]], edge1 = pVtx[pTris[nTris * 3 + 2]] - pVtx[pTris[nTris * 3]];
				float darea = edge0 ^ edge1;
				area1 += darea;
				nDegenTris += isneg(sqr(darea) - sqr(0.02f)*(edge0*edge0)*(edge1*edge1));
				nTris++;
				ptr->next[i ^ 1]->next[i] = ptr->next[i]; ptr->next[i]->next[i ^ 1] = ptr->next[i ^ 1];
				pBounds[i] = ptr_next = ptr->next[i ^ 1];
				if (pPinnacle == ptr)
					pPinnacle = ptr->next[i];
				ptr->next[0] = ptr->next[1] = 0; ptr->bProcessed = 1;
			}
			else
				break;

		if ((pBounds[i] = pBounds[i]->next[i]) == pBounds[i ^ 1]->next[i ^ 1])
			pBounds[0] = pBounds[1] = 0;
		else if (pBounds[i]->pt->y > pPinnacle->pt->y)
			pPinnacle = pBounds[i];
	} while (nTris < szTriBuf && --iter);

	if (pThunks != bufThunks) delete[] pThunks;
	if (pBottoms != bufBottoms) delete[] pBottoms;
	if (pSags != bufSags) delete[] pSags;

	int bProblem = nTris<nThunks0 - nConts * 2 || fabs_tpl(area0 - area1)>area0*0.003f || nTris >= szTriBuf;
	if (bProblem || nDegenTris)
		if (nConts == 1)
			return TriangulatePolyBruteforce(pVtx, nVtx, pTris, szTriBuf);
		else
			g_nTriangulationErrors += bProblem;

	return nTris;
}

