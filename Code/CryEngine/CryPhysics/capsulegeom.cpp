// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "unprojectionchecks.h"
#include "bvtree.h"
#include "singleboxtree.h"
#include "geometry.h"
#include "cylindergeom.h"
#include "capsulegeom.h"
#include "spheregeom.h"
#include "trimesh.h"
#include "boxgeom.h"


CCapsuleGeom* CCapsuleGeom::CreateCapsule(capsule *pcaps)
{
	m_cyl.center = pcaps->center;
	m_cyl.axis = pcaps->axis;
	m_cyl.hh = pcaps->hh;
	m_cyl.r = pcaps->r;

	box bbox;
	bbox.Basis.SetRow(2,m_cyl.axis);
	bbox.Basis.SetRow(0,m_cyl.axis.GetOrthogonal().normalized());
	bbox.Basis.SetRow(1,m_cyl.axis ^ bbox.Basis.GetRow(0));
	bbox.bOriented = 1;
	bbox.center = m_cyl.center;
	bbox.size.z = m_cyl.hh+m_cyl.r;
	bbox.size.x = bbox.size.y = m_cyl.r;
	m_Tree.SetBox(&bbox);
	m_Tree.Build(this);
	m_minVtxDist = (m_cyl.r+m_cyl.hh)*1E-4f;
	return this;
}


int CCapsuleGeom::CalcPhysicalProperties(phys_geometry *pgeom)
{ 
	pgeom->pGeom = this;
	pgeom->origin = m_cyl.center;
	pgeom->q.SetRotationV0V1(Vec3(0,0,1),m_cyl.axis);
	float Vcap = 4.0f/3*(float)g_PI*cube(m_cyl.r);
	pgeom->V = sqr(m_cyl.r)*m_cyl.hh*((float)g_PI*2) + Vcap;
	float r2=sqr(m_cyl.r), x2=(float)g_PI*m_cyl.hh*sqr(r2)*0.5f, z2=(float)g_PI*r2*cube(m_cyl.hh)*(2.0f/3);
	pgeom->Ibody.Set(x2+z2+Vcap*(r2*0.4f+sqr(m_cyl.hh)), x2+z2+Vcap*(r2*0.4f+sqr(m_cyl.hh)), x2*2+Vcap*r2*0.4f);
	return 1;
}


int CCapsuleGeom::PointInsideStatus(const Vec3 &pt)
{
	Vec3 ptr,ptc; float h;
	ptc=ptr = pt-m_cyl.center; h = m_cyl.axis*ptr; 
	ptr -= m_cyl.axis*h; ptc -= m_cyl.axis*(m_cyl.hh*sgnnz(h));
	return isneg(min(ptc.len2()-sqr(m_cyl.r), max(ptr.len2()-sqr(m_cyl.r),fabs_tpl(h)-m_cyl.hh)));
}


int CCapsuleGeom::PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl, bool bKeepPrevContacts)
{
	CCylinderGeom::PrepareForIntersectionTest(pGTest,pCollider,pGTestColl,bKeepPrevContacts);
	pGTest->typeprim = capsule::type;
	pGTest->szprim = sizeof(capsule);
	return 1;
}


int CCapsuleGeom::GetUnprojectionCandidates(int iop,const contact *pcontact, primitive *&pprim,int *&piFeature, geometry_under_test *pGTest)
{
	if (pGTest->bTransformUpdated) {
		pprim = pGTest->primbuf1;
		PrepareCylinder((cylinder*)pprim, pGTest);
	}
	cylinder *pcyl = (cylinder*)pprim;
	pGTest->idbuf[0] = (char)-1;

	int iFeature = pcontact->iFeature[iop];
	float r2,hh;
	r2 = (pcontact->pt-pcyl->center^pcyl->axis).len2();
	hh = (pcontact->pt-pcyl->center)*pcyl->axis;

	if (pcontact->iFeature[iop]==0x40 || fabs_tpl(r2-sqr(pcyl->r))<sqr(m_minVtxDist) && fabs_tpl(fabs_tpl(hh)-pcyl->hh)<m_minVtxDist) {
		pGTest->edges[0].dir = pcyl->axis;
		pGTest->edges[0].n[0] = pcyl->center-pcontact->pt^pcyl->axis;
		pGTest->edges[0].n[1] = -pGTest->edges[0].n[0];
		pGTest->edges[0].idx = 0;
		pGTest->edges[0].iFeature = iFeature;
		pGTest->nEdges = 1;
	} else
		pGTest->nEdges = 0;
	pGTest->nSurfaces = 0;

	return 1;
}


int CCapsuleGeom::FindClosestPoint(geom_world_data *pgwd, int &iPrim,int &iFeature, const Vec3 &ptdst0,const Vec3 &ptdst1,
																	 Vec3 *ptres, int nMaxIters)
{
	Vec3r axis,center,pt,l,n,ptdst[]={ptdst0,ptdst1},ptresi[2];
	real r,hh,r2,n2,t0/*,t1*/;
	int i,bLine;
	axis = pgwd->R*m_cyl.axis;
	center = pgwd->R*m_cyl.center*pgwd->scale + pgwd->offset;
	r = m_cyl.r*pgwd->scale; r2 = r*r; hh = m_cyl.hh*pgwd->scale;
	pt = ptdst0-center; ptres[1] = ptdst0;

	if (bLine = isneg(r2*1E-6f-(l=ptdst1-ptdst0).len2())) {
		n = l^axis;	n2 = n.len2();
		if (isneg(n2*r2-sqr(pt*n)) & inrange(t0=(-pt^axis)*n, (real)0,n2) & inrange(/*t1=*/(-pt^l)*n, -n2*hh,n2*hh))	{
			ptres[1] = ptdst0+l*(t0/n2);	// line-capsule side distance
			ptres[0] = center+axis*(axis*(ptres[1]-center));
			ptres[0] += (ptres[1]-ptres[0]).normalized()*r;
			return 1;
		}
		for(i=-1;i<=1;i+=2) if (inrange(t0=l*(center+axis*(hh*i)-ptdst0),(real)0,l.len2()) &&
			(((ptdst0-center)*axis)*l.len2()+(l*axis)*t0)*i > hh*l.len2()) 
		{
			ptres[1] = ptdst0+l*(t0/l.len2());
			ptres[0] = center+axis*(hh*i); // line-spherical cap
			ptres[0] += (ptres[1]-ptres[0]).normalized()*r;
			return 1;
		}
	}

	assert(bLine<2);
	ptresi[1].zero();
	for(i=0;i<=bLine;i++)	{
		pt = ptdst[i]-center;
		if (fabsf((float)(pt*axis))<hh) { // the closest point lies on capsule side
			pt -= axis*(axis*pt);
			ptresi[i] = ptdst[i]-pt+pt.normalized()*r;
			continue;
		}
		ptresi[i] = center+axis*(hh*sgnnz(pt*axis)); // ..cap
		ptresi[i] += (ptdst[i]-ptresi[i]).normalized()*r;
	}
	i = bLine & isneg((ptresi[1]-ptdst[1]).len2()-(ptresi[0]-ptdst[0]).len2());
	ptres[0] = ptresi[i];
	ptres[1] = ptdst[i];
	return 1;
}


int CCapsuleGeom::UnprojectSphere(Vec3 center,float r,float rsep, contact *pcontact)
{
	float hh = m_cyl.axis*(center-m_cyl.center);
	if (fabs_tpl(hh) < m_cyl.hh) {
		pcontact->n = center-m_cyl.center; pcontact->n -= m_cyl.axis*(m_cyl.axis*pcontact->n);
		if (pcontact->n.len2() > sqr(m_cyl.r+rsep))
			return 0;
		pcontact->pt = m_cyl.center+m_cyl.axis*hh+pcontact->n.normalize()*m_cyl.r;
		pcontact->iFeature[0] = 0x40;
		return 1;
	}

	Vec3 ccap = m_cyl.center+m_cyl.axis*(m_cyl.hh*sgnnz(hh));
	if ((ccap-center).len2() > sqr(m_cyl.r+rsep))
		return 0;
	pcontact->n = (center-ccap).normalized();
	pcontact->pt = ccap+pcontact->n*m_cyl.r;
	pcontact->iFeature[0] = 0x42-isneg(hh);
	return 1;
}


float CCapsuleGeom::CalculateBuoyancy(const plane *pplane, const geom_world_data *pgwd, Vec3 &massCenter)
{
	float V;
	Vec3 massCap;

	V = CCylinderGeom::CalculateBuoyancy(pplane,pgwd,massCenter);
	/*massCenter *= V;

	CSphereGeom sph;
	sph.m_sphere.r = m_cyl.r;
	sph.m_sphere.center = m_cyl.center+m_cyl.axis*m_cyl.hh;
	float Vcap = sph.CalculateBuoyancy(pplane,pgwd,massCap);
	massCenter += massCap*Vcap; V += Vcap;

	sph.m_sphere.center = m_cyl.center-m_cyl.axis*m_cyl.hh;
	Vcap = sph.CalculateBuoyancy(pplane,pgwd,massCap);
	massCenter += massCap*Vcap; V += Vcap;

	if (V>0)
		massCenter/=V;*/
	return V;

	/*for(i=0;i<2;i++) {
		cosa = pplane->n*axis; sina = (pplane->n^axis).len();
		angle = atan2(sina,cosa);
		dist = (pplane->origin-center)*pplane->n;
		Vcap = (2.0f*g_PI/3)*cube(r)*isnonneg(dist);
		ccap = (center+axis*(r*(3.0f/8)))*Vcap; 
		if (fabs_tpl(dist)<r*0.999f) {
			if (fabs_tpl(dist)>r*sina*0.999f) {
				Vslice = g_PI*((2.0f/3)*cube(r)-r*r*fabs_tpl(dist)+(1.0f/3)*cube(fabs_tpl(dist));
				cslice = center+axis*(dist/cosa*Vslice);
				cslice += pplane->n*(sgnnz(cosa)*g_PI*0.5f*(sqr(r*r)*0.5f-sqr(r*dist)+sqr(dist*dist)*0.5f));
				j = isneg(dist*cosa)*-sgnnz(dist);
				ccap += cslice*j;
				Vcap += Vslice*j;
			} else { // use an approximate solution
				r1 = sqrt_tpl(r*r-dist*dist);
				h0 = dist/sina; h1 = h0*cosa;
				S0 = h0*dist+r*r*(g_PI*0.5f+asin_tpl(h0/r));
				S1 = h1*sqrt_tpl(sqr(r1)-sqr(h1))+r1*(g_PI*0.5f+asin_tpl(h1/r1));
				V = angle*(2.0f/3/g_PI)*(S*r+Sslice*rslice);
			}
		}
		V += Vcap;
		massCenter += ccap;
	}*/
}


void CCapsuleGeom::CalculateMediumResistance(const plane *pplane, const geom_world_data *pgwd, Vec3 &dPres,Vec3 &dLres)
{
	Vec3 n,rotax,center,dPcap,dLcap,ptside[4];
	float sina,r,hh,x0,y0,x1,y1,dx;
	int i;
	CSphereGeom sph;
	dPres.zero(); dLres.zero();

	r = m_cyl.r*pgwd->scale;
	hh = m_cyl.hh*pgwd->scale;
	n = pgwd->R*-m_cyl.axis;
	rotax = n^Vec3(0,0,1);
	sina = rotax.len();
	if (sina>0.001f)
		rotax/=sina;
	else 
		rotax.Set(1,0,0);
	center = pgwd->R*m_cyl.center*pgwd->scale+pgwd->offset+n*hh;

	x1 = 0.965925826f; y1 = 0.258819045f;	// 15 degrees sin/cos
	ptside[0] = Vec3(x0=r,y0=0,0).GetRotated(rotax,n.z,-sina)+center;
	for(i=0; i<12; i++) {
		ptside[1] = ptside[0];
		dx = x0; x0 = (x0*sqrt3-y0)*0.5f; y0 = (y0*sqrt3+dx)*0.5f;
		ptside[0] = Vec3(x0,y0,0).GetRotated(rotax,n.z,-sina)+center;
		ptside[2] = ptside[1]-n*(hh*2); ptside[3] = ptside[0]-n*(hh*2);
		CalcMediumResistance(ptside,4, Vec3(x1,y1,0).GetRotated(rotax,n.z,-sina),
			*pplane,pgwd->v,pgwd->w,pgwd->centerOfMass,dPres,dLres);
		dx = x1; x1 = (x1*sqrt3-y1)*0.5f; y1 = (y1*sqrt3+dx)*0.5f;
	}

	sph.m_sphere.r = m_cyl.r;
	sph.m_sphere.center = m_cyl.center+m_cyl.axis*m_cyl.hh;
	sph.CalculateMediumResistance(pplane,pgwd, dPcap,dLcap);
	dPres += dPcap; dLres += dLcap;
	sph.m_sphere.center = m_cyl.center-m_cyl.axis*m_cyl.hh;
	sph.CalculateMediumResistance(pplane,pgwd, dPcap,dLcap);
	dPres += dPcap; dLres += dLcap;
}

int CCapsuleGeom::DrawToOcclusionCubemap(const geom_world_data *pgwd, int iStartPrim,int nPrims, int iPass, SOcclusionCubeMap* cubeMap)
{
	CBoxGeom geombox;
	geombox.GetBox() = m_Tree.m_Box;
	geombox.GetBox().size.z -= (1.f-0.707f)*geombox.GetBox().size.x;
	geombox.GetBox().size.x *= 0.707f;
	geombox.GetBox().size.y *= 0.707f;
	return geombox.DrawToOcclusionCubemap(pgwd,iStartPrim,nPrims,iPass,cubeMap);
}

