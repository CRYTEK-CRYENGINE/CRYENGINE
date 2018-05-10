// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"

Vec2 g_BoolPtBufThread[MAX_PHYS_THREADS+1][4096];
int g_BoolIdBufThread[MAX_PHYS_THREADS+1][4096];
int g_BoolGridThread[MAX_PHYS_THREADS+1][4096];
unsigned int g_BoolHashThread[MAX_PHYS_THREADS+1][8192];
struct inters2d {
	Vec2 pt;
	int iedge[2];
};
inters2d g_BoolIntersThread[MAX_PHYS_THREADS+1][256];

#undef S

inline int line_seg_inters(const Vec2 *seg0, const Vec2 *seg1, Vec2 *ptres)
{
	float denom,t0,t1,sg;
	Vec2 dp0,dp1,ds;
	dp0 = seg0[1]-seg0[0]; dp1 = seg1[1]-seg1[0];	ds = seg1[0]-seg0[0];
	denom = dp0^dp1;

	if (sqr(denom) > 1E-6f*len2(dp0)*len2(dp1)) { // stable intersection
		t0 = ds^dp1; t1 = ds^dp0;
		sg = sgnnz(denom); denom*=sg; t0*=sg; t1*=sg;
		if (isneg(fabs_tpl(t0*2-denom)-denom) & isneg(fabs_tpl(t1*2-denom)-denom)) {
			ptres[0] = seg0[0] + dp0*(t0/denom); return 1;
		} else return 0;
	}

	if (sqr(ds^dp0) < 1E-6f*len2(ds)*len2(dp0)) { // segments are [almost] parallel and touching
		float t[2][2]; const Vec2 *ppt[2][2]; int idir;
		t[0][0]=0; t[0][1]=len2(dp0); ppt[0][0]=seg0; ppt[0][1]=seg0+1;
		t0=(seg1[0]-seg0[0])*dp0; t1=(seg1[1]-seg0[0])*dp0;
		idir = isneg(t1-t0); t[1][idir]=t0; t[1][idir^1]=t0; ppt[1][idir]=seg1; ppt[1][idir^1]=seg1+1;
		if (max(t[0][0],t[1][0])>min(t[0][1],t[1][1]))
			return 0;
		ptres[0] = *ppt[0][isneg(t[0][0]-t[1][0])];
		ptres[1] = *ppt[1][isneg(t[1][1]-t[0][1])];
		return 2;
	}
	return 0;
}

inline Vec2i& get_cell(const Vec2 &pt, const Vec2 &rstep, Vec2i &ipt) 
{
	ipt.set(float2int(pt.x*rstep.x-0.5f),float2int(pt.y*rstep.y-0.5f));
	return ipt;
}

inline void get_rect(const Vec2i &ipt0,const Vec2i &ipt1,Vec2i *irect,const Vec2i &isz) 
{
	irect[0].x = max(0,min(ipt0.x,ipt1.x)); irect[0].y = min(isz.y,max(0,min(ipt0.y,ipt1.y)));
	irect[1].x = min(isz.x-1,max(ipt0.x,ipt1.x)); irect[1].y = min(isz.y,max(ipt0.y,ipt1.y));
}

inline int check_if_inside(int iCaller, int iobj, Vec2i &ipt,const Vec2i &isz,const Vec2 *ptsrc, const Vec2 &pt) 
{
	int bInside,i,bStop=0;
	Vec2 pt0,pt1,dp;
	quotientf ycur;
	if ((unsigned int)ipt.x>=(unsigned int)isz.x || ipt.y<0)
		return 0;
	int *g_BoolGrid = ::g_BoolGridThread[iCaller];
	unsigned int *g_BoolHash = ::g_BoolHashThread[iCaller];

	for(bInside=0; ipt.y<=isz.y && !bStop; ipt.y++)
	for(i=g_BoolGrid[ipt.y*isz.x+ipt.x]; i<g_BoolGrid[ipt.y*isz.x+ipt.x+1]; i++) if (g_BoolHash[i]>>31^iobj) {
		pt0 = ptsrc[g_BoolHash[i]&0x7FFFFFFF]; pt1 = ptsrc[(g_BoolHash[i]&0x7FFFFFFF)+1]; dp = pt1-pt0;
		ycur.set((dp^pt0)+pt.x*dp.y, dp.x).fixsign();
		if (isneg(fabs_tpl((pt0.x+pt1.x)-pt.x*2)-fabs_tpl(pt0.x-pt1.x)) & isneg(pt.y-ycur))
		{ bInside -= sgn(dp.x); bStop=1; }
	}
	return isneg(-bInside);
}

static int __bforce1=0,__bforce2=0;
int boolean2d(booltype type, Vec2 *ptbuf1,int npt1, Vec2 *ptbuf2,int npt2,int bClosed, Vec2 *&ptres,int *&pidres)
{
	int iCaller = get_iCaller();
	Vec2 *g_BoolPtBuf = ::g_BoolPtBufThread[iCaller];
	int *g_BoolIdBuf = ::g_BoolIdBufThread[iCaller];
	int *g_BoolGrid = ::g_BoolGridThread[iCaller];
	unsigned int *g_BoolHash = ::g_BoolHashThread[iCaller];
	inters2d *g_BoolInters = ::g_BoolIntersThread[iCaller];

	Vec2 *ptsrc[2] = { ptbuf1,ptbuf2 };
	int npt[2] = { npt1,npt2 };
	Vec2 dp,dp1,ptmin[2],ptmax[2],sz,ptbox[2],ptint[2],pttest,rstep;
	int iobj,i,j,idx,nsz,n,npttmp,nptres,ninters=0,istart,ix,iy,bInside,bPrevInside,idx_next,idx_prev,inext,/*inext1,*/iobj1;
	Vec2i ipt0,ipt1,irect[2],isz;
	float t,ratioxy,ratioyx;

//for(i=0;i<max(npt1,npt2);i++) g_BoolIdBuf[i]=0;

	pidres = g_BoolIdBuf;
	if (npt1<3 || __bforce2) 
	{	ptres = ptbuf2; return npt2&-(bClosed|__bforce2); }
	if (npt2<2+bClosed || __bforce1) 
	{ ptres = ptbuf1; return npt1&-(bClosed|__bforce1); }

	for(iobj=0;iobj<2;iobj++) {
		ptmin[iobj] = ptmax[iobj] = ptsrc[iobj][0];
		for(i=1;i<npt[iobj];i++) {
			ptmin[iobj].x = min(ptmin[iobj].x,ptsrc[iobj][i].x); ptmin[iobj].y = min(ptmin[iobj].y,ptsrc[iobj][i].y);
			ptmax[iobj].x = max(ptmax[iobj].x,ptsrc[iobj][i].x); ptmax[iobj].y = max(ptmax[iobj].y,ptsrc[iobj][i].y);
		}
	}
	ptbox[0].x = max(ptmin[0].x,ptmin[1].x); ptbox[0].y = max(ptmin[0].y,ptmin[1].y);
	ptbox[1].x = min(ptmax[0].x,ptmax[1].x); ptbox[1].y = min(ptmax[0].y,ptmax[1].y);
	sz = ptbox[1]-ptbox[0];
	sz.x += fabs_tpl(sz.y)*1E-5f;	sz.y += fabs_tpl(sz.x)*1E-5f;
	if (min(sz.x,sz.y) <  max(1e-8f,min(max(ptmax[0].x-ptmin[0].x,ptmax[0].y-ptmin[0].y),max(ptmax[1].x-ptmin[1].x,ptmax[1].y-ptmin[1].y))*0.001f))
		return 0;
	ptbox[0] -= sz*0.01f; ptbox[1] += sz*0.01f;	sz = ptbox[1]-ptbox[0];
	sz.x += fabs_tpl(sz.y)*0.01f;	sz.y += fabs_tpl(sz.x)*0.01f;

	i = (int)(sizeof(g_BoolGridThread[0])/sizeof(g_BoolGridThread[0][0])-1);
	npttmp = min(npt[0]+npt[1]<<1, i);
	ratioyx = max(min(sz.y/sz.x,(float)npttmp),1.0f);
	ratioxy = max(min(sz.x/sz.y,(float)npttmp),1.0f);
	isz.y = max(1,float2int(sqrt_tpl(npttmp*4*ratioyx+1)*0.5f-1.0f));
	isz.x = max(1,float2int(isz.y*ratioxy-0.5f));
	if (isz.x*(isz.y+1)>i) {
		isz.y = min(isz.y,i); isz.x = i/(isz.y+1);
	}
	rstep.set(isz.x/sz.x, isz.y/sz.y);
	nsz = isz.x*(isz.y+1);
	if (nsz>=sizeof(g_BoolGridThread[0])/sizeof(g_BoolGridThread[0][0])-1)
		return 0;
	npt[1] -= bClosed^1;

	for(i=0;i<=nsz;i++) g_BoolGrid[i] = 0;
	
	for(iobj=0;iobj<2;iobj++) {	// calculate number of elements in each cell
		for(get_cell(ptsrc[iobj][i=0]-ptbox[0],rstep,ipt0); i<npt[iobj]; i++) { 
			get_cell(ptsrc[iobj][i+1]-ptbox[0],rstep,ipt1); get_rect(ipt0,ipt1,irect,isz);
			for(ix=irect[0].x;ix<=irect[1].x;ix++) for(iy=irect[0].y;iy<=irect[1].y;iy++)
				g_BoolGrid[iy*isz.x+ix]++;
			ipt0 = ipt1;
		}
	}
	
	for(i=1;i<=nsz;i++) g_BoolGrid[i]+=g_BoolGrid[i-1];
	PREFAST_ASSUME(nsz>0);
	if (g_BoolGrid[nsz-1]>(int)(sizeof(g_BoolHashThread[0])/sizeof(g_BoolHashThread[0][0])))
		return 0;

	for(iobj=0;iobj<2;iobj++) {	// put each line segment into the corresponding hash cell(s)
		for(get_cell(ptsrc[iobj][i=0]-ptbox[0],rstep,ipt0); i<npt[iobj]; i++) { 
			get_cell(ptsrc[iobj][i+1]-ptbox[0],rstep,ipt1);	get_rect(ipt0,ipt1,irect,isz);
			for(ix=irect[0].x;ix<=irect[1].x;ix++) for(iy=irect[0].y;iy<=irect[1].y;iy++)
				g_BoolHash[--g_BoolGrid[iy*isz.x+ix]] = iobj<<31 | i;
			ipt0 = ipt1;
		}
	}

	// select iobj which is likely to have shorter edges
	if (bClosed)
		iobj = isneg((ptmax[1].x-ptmin[1].x+ptmax[1].y-ptmin[1].y)*npt[0] - 
								 (ptmax[0].x-ptmin[0].x+ptmax[0].y-ptmin[0].y)*npt[1]);
	else iobj = 1;

	// build list of intersections by traversing iobj 
	if (!bClosed) {
		g_BoolInters[0].pt = ptsrc[1][0];
		g_BoolInters[0].iedge[0] = -1; g_BoolInters[0].iedge[1] = 0;
		ninters = 1;
	}
	for(get_cell(ptsrc[iobj][0]-ptbox[0],rstep,ipt0),i=0; i<npt[iobj]; i++) { 
		get_cell(ptsrc[iobj][i+1]-ptbox[0],rstep,ipt1);	get_rect(ipt0,ipt1,irect,isz);
		istart = ninters;

		for(ix=irect[0].x;ix<=irect[1].x;ix++) for(iy=irect[0].y;iy<=irect[1].y;iy++)		NO_BUFFER_OVERRUN
		for(j=g_BoolGrid[iy*isz.x+ix]; j<g_BoolGrid[iy*isz.x+ix+1]; j++) if (g_BoolHash[j]>>31^iobj) {
			for(n=line_seg_inters(ptsrc[iobj]+i, ptsrc[iobj^1]+(g_BoolHash[j]&0x7FFFFFFF), ptint)-1; n>=0; n--) {
				dp = ptsrc[iobj][i+1]-ptsrc[iobj][i]; t = (ptint[n]-ptsrc[iobj][i])*dp;
				for(idx=istart; idx<ninters && fabs_tpl((g_BoolInters[idx].pt-ptsrc[iobj][i])*dp-t)>t*1E-7f; idx++);
				if (idx<ninters)
					continue; // ignore possible intersections with the same line found in different cell
				if (ninters==sizeof(g_BoolIntersThread[0])/sizeof(g_BoolIntersThread[0][0])-1)
					return 0;
				for(idx=ninters-1; idx>=istart && (g_BoolInters[idx].pt-ptsrc[iobj][i])*dp>t; idx--)
					g_BoolInters[idx+1] = g_BoolInters[idx];
				g_BoolInters[idx+1].pt = ptint[n];
				g_BoolInters[idx+1].iedge[iobj] = i;
				g_BoolInters[idx+1].iedge[iobj^1] = g_BoolHash[j]&0x7FFFFFFF;
				ninters++;
			}
		}
		ipt0 = ipt1;
	}
	if (!bClosed) {
		if (ninters==sizeof(g_BoolIntersThread[0])/sizeof(g_BoolIntersThread[0][0])-1)
			return 0;
		g_BoolInters[ninters].pt = ptsrc[1][npt[1]];
		g_BoolInters[ninters].iedge[0] = -1; g_BoolInters[ninters].iedge[1] = npt[1];
		g_BoolInters[ninters+1] = g_BoolInters[ninters]; ninters++;
	}	else if (ninters>0)
		g_BoolInters[ninters] = g_BoolInters[0];

	nptres = 0; ptres = g_BoolPtBuf; 
	// if there were no intersections, return the object that is inside the other
	if (ninters-(bClosed^1)*2==0) {
		get_cell(ptsrc[iobj][0]-ptbox[0],rstep,ipt0);
		bInside = check_if_inside(iCaller,iobj,ipt0,isz,ptsrc[iobj^1],ptsrc[iobj][0]);
		npt[1] += bClosed^1;
		if (bClosed) // assume that objects cannot have empty intersection area, thus if iobj is not inside iobj^1, ibj1^1 should be inside iobj
			iobj ^= bInside^1;
		ptres = ptsrc[iobj]; 
		for(nptres=0;nptres<npt[iobj];nptres++)
			g_BoolIdBuf[nptres] = nptres+1<<iobj*16;
		return nptres & -(bClosed|bInside);
	}
	npt[1] += bClosed^1; // compensate for 1 subtracted earlier

	// build boolean intersection by at each intersection point selecting the stripe that is more 'inward' than the other one
	for(idx=bPrevInside=0; idx<ninters; idx++) {
		idx_next = idx+1; //& ~(ninters-2-idx>>31);
		idx_prev = idx-1; idx_prev = idx_prev&~(idx_prev>>31) | ninters-1&idx_prev>>31;
		i = g_BoolInters[idx].iedge[iobj]; inext = i+1 & i+1-npt[iobj]>>31;
		dp = ptsrc[iobj][inext]-ptsrc[iobj][i];
//		inext1 = min(inext+1,npt[iobj]-1) & (i+1-npt[iobj]>>31 | ~-bClosed);

		j = g_BoolInters[idx].iedge[iobj^1];
		if (j>=0) {
			dp1 = ptsrc[iobj^1][j+1 & j+1-npt[iobj^1]>>31]-ptsrc[iobj^1][j];
			bInside = isneg(dp^dp1);
		} else {
			pttest = g_BoolInters[idx].pt;
			get_cell(pttest-ptbox[0],rstep,ipt0);
			bInside = check_if_inside(iCaller,iobj,ipt0,isz,ptsrc[iobj^1],pttest);
		}
		/*
		// build boolean intersection by selecting fragments (stripes between 2 intersection points) of the object that is inside another object
		// cannot just switch "inside" state at each intersection, since it's less stable in case of degenerate contacts
		// find some representative point for this fragment
		if (g_BoolInters[idx_next].iedge[iobj]==i && (g_BoolInters[idx_next].pt-ptsrc[iobj][i])*dp>(g_BoolInters[idx].pt-ptsrc[iobj][i])*dp)
			pttest = (g_BoolInters[idx].pt+g_BoolInters[idx_next].pt)*0.5f; // next intersection lies on the same edge
		else if ((g_BoolInters[idx].pt-ptsrc[iobj][i])*dp < dp.len2()*0.95f*0.95f)
			pttest = (g_BoolInters[idx].pt+ptsrc[iobj][inext1])*0.5f; // this intersection is not too close to the edge end
		else if (g_BoolInters[idx_next].iedge[iobj]==inext)
			pttest = (ptsrc[iobj][inext]+g_BoolInters[idx_next].pt)*0.5f; // next intersection lies on the next edge
		else 
			pttest = (ptsrc[iobj][inext]+ptsrc[iobj][inext1])*0.5f;

		// check if a ray upwards from pttest first hits right-to-left edge of iobj^1, in this case we are inside
		get_cell(pttest-ptbox[0],rstep,ipt0);
		bInside = check_if_inside(iobj,ipt0,isz,ptsrc[iobj^1],pttest);*/
		
		iobj1 = iobj^bInside^1;
		if (bInside | bPrevInside | bClosed) {
			g_BoolPtBuf[nptres] = g_BoolInters[idx].pt;
			g_BoolIdBuf[nptres++] = g_BoolInters[idx].iedge[1]+1<<16 | g_BoolInters[idx].iedge[0]+1;
			if (nptres>=(int)(sizeof(g_BoolPtBufThread[0])/sizeof(g_BoolPtBufThread[0][0])))
				return nptres;
		}
		if (bInside | bClosed) {
			i = g_BoolInters[idx].iedge[iobj1];
			inext = i+1 & ~(npt[iobj1]-2-i>>31);
			dp = ptsrc[iobj1][inext]-ptsrc[iobj1][i];
			bool bForceFirstStep = i==g_BoolInters[idx_next].iedge[iobj1] && 
				(g_BoolInters[idx].pt-ptsrc[iobj1][i])*dp > (g_BoolInters[idx_next].pt-ptsrc[iobj1][i])*dp;
			for(; bForceFirstStep || 
					i!=g_BoolInters[idx_next].iedge[iobj1] && (iobj1==iobj || i!=g_BoolInters[idx_prev].iedge[iobj1]); 
					i=i+1&~(npt[iobj1]-2-i>>31)) 
			{
				g_BoolPtBuf[nptres] = ptsrc[iobj1][i+1];
				g_BoolIdBuf[nptres++] = i+2<<iobj1*16;
				if (nptres>=(int)(sizeof(g_BoolPtBufThread[0])/sizeof(g_BoolPtBufThread[0][0])))
					return nptres;
				bForceFirstStep = false;
			}
		}
		bPrevInside = bInside;
	}

	return nptres;
}
