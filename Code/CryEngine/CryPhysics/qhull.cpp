// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"

///////////////////////////// Qhull 3d ///////////////////////////////

struct ptitem {
	ptitem *next,*prev;
};

struct qhtritem {
	qhtritem *next,*prev;
	ptitem *ptassoc;
	Vec3 n,pt0;
	int idx[3];
	qhtritem *buddy[3];
	int deleted;
};

inline bool e_cansee(const Vec3 &dp, const Vec3 &n, float e=0.002f) { return sqr_signed(dp*n) > sqr_signed(e)*dp.len2()*n.len2(); }

inline void relocate_ptritem(qhtritem *&ptr,intptr_t diff) {
	ptr = (qhtritem*)((intptr_t)ptr+diff & ~-iszero((intptr_t)ptr));	// offset the pointer, but leave out 0s
}

inline void relocate_tritem(qhtritem *ptr,intptr_t diff) {
	relocate_ptritem(ptr->next,diff); relocate_ptritem(ptr->prev,diff);
	relocate_ptritem(ptr->buddy[0],diff); relocate_ptritem(ptr->buddy[1],diff);	relocate_ptritem(ptr->buddy[2],diff);
}

template<class item> void delete_item_from_list(item *p) {
	if (p->prev) p->prev->next = p->next;
	if (p->next) p->next->prev = p->prev;
	p->prev = p->next = 0;
}
template<class item> void add_item_to_list(item *&pin, item *pnew) {
	if (!pin) pin=pnew->next=pnew->prev=pnew;
	else {
		pnew->next = pin->next;	pnew->prev = pin;
		pin->next->prev = pnew;	pin->next = pnew;
	}
}
template<class item> void merge_lists(item *&plist, item *pnew) {
	if (!pnew) return;
	if (!plist) plist = pnew;
	else {
		plist->next->prev = pnew->prev;
		pnew->prev->next = plist->next;
		plist->next = pnew;
		pnew->prev = plist;
	}
}

void associate_ptlist_with_trilist(ptitem *ptlist, qhtritem *trilist, ptitem *pt0,strided_pointer<Vec3mem> pvtx) {
	if (!ptlist) return;
	ptitem *pt=ptlist,*ptnext,*ptlast=ptlist->prev;
	qhtritem *tr; int i;
	do {
		ptnext = pt->next;
		delete_item_from_list(pt);
		tr=trilist; i=pt-pt0; do {
			if (e_cansee(pvtx[i]-tr->pt0,tr->n)) {
				add_item_to_list(tr->ptassoc, pt);
				break;
			}
			tr=tr->next;
		} while(tr!=trilist); 
		if (pt==ptlast) break;
		pt = ptnext;
	} while(true);
}

static inline void swap(int *v,void **p, int i1,int i2)
{
	int ti=v[i1]; v[i1]=v[i2]; v[i2]=ti;
	void *tp=p[i1]; p[i1]=p[i2]; p[i2]=tp;
}
static void qsort(int *v,void **p, int left,int right)
{
	if (left>=right) return;
	int i,last; 
	swap(v,p, left, left+right>>1);
	for(last=left,i=left+1; i<=right; i++)
	if (v[i] < v[left])
		swap(v,p, ++last, i);
	swap(v,p, left,last);

	qsort(v,p, left,last-1);
	qsort(v,p, last+1,right);
}
static int bin_search(int *v,int n,int idx)
{
	int left=0,right=n,m;
	do {
		m = left+right>>1;
		if (v[m]==idx) return m;
		if (v[m]<idx) left=m;
		else right=m;
	} while (left<right-1);
	return left;
}

int __qhullcalled=0;

int qhull(strided_pointer<Vec3> _pts, int npts, index_t*& pTris, qhullmalloc qmalloc)
{
#if CRY_PLATFORM_64BIT
	static ptitem ptbuf[4096];
	static qhtritem trbuf[4096];
	static qhtritem *tmparr_ptr_buf[2048];
	static int tmparr_idx_buf[2048];
#else
	static ptitem ptbuf[1024];
	static qhtritem trbuf[1024];
	static qhtritem *tmparr_ptr_buf[512];
	static int tmparr_idx_buf[512];
#endif
	static volatile int g_lockQhull;
	int iter=0,maxiter=0;
	__qhullcalled++;
	strided_pointer<Vec3mem> pts = strided_pointer<Vec3mem>((Vec3mem*)_pts.data,_pts.iStride);

	ptitem *pt,*ptmax,*ptdeleted,*ptlist = npts>CRY_ARRAY_COUNT(ptbuf) ? new ptitem[npts] : ptbuf;
	qhtritem *tr,*trnext,*trend,*trnew,*trdata=trbuf,*trstart=0,*trlast,*trbest;
	int i,j,k,ti,trdatasz=CRY_ARRAY_COUNT(trbuf), bidx[6],n,next_iter,delbuds;
	qhtritem **tmparr_ptr=tmparr_ptr_buf;
	int *tmparr_idx=tmparr_idx_buf, tmparr_sz=512;
	float dist,maxdist/*,e*/;
	Vec3 pmin(VMAX),pmax(VMIN);
	WriteLock lock(g_lockQhull);

	/*if (npts<=8) {
		if (!pTris)
			pTris = (index_t*)trbuf;
		int nnew,nsplits,itri,ptmask=0;
		Vec3 norm;

		for(bidx[0]=0,i=1;i<npts;i++)
			bidx[0] += i-bidx[0] & -isneg(pts[i].x-pts[bidx[0]].x);
		for(bidx[1]=0,i=1;i<npts;i++)
			bidx[1] += i-bidx[1] & -isneg((pts[bidx[1]]-pts[bidx[0]]).len2()-(pts[i]-pts[bidx[0]]).len2());
		norm = pts[bidx[1]]-pts[bidx[0]];
		for(bidx[2]=0,i=1;i<npts;i++)
			bidx[2] += i-bidx[2] & -isneg((norm^pts[bidx[2]]-pts[bidx[0]]).len2()-(norm^pts[i]-pts[bidx[0]]).len2());
		norm = pts[bidx[1]]-pts[bidx[0]] ^ pts[bidx[2]]-pts[bidx[0]];
		for(bidx[3]=0,i=1;i<npts;i++)
			bidx[3] += i-bidx[3] & -isneg(fabs_tpl((pts[bidx[3]]-pts[bidx[0]])*norm)-fabs_tpl((pts[i]-pts[bidx[0]])*norm));
		if ((pts[bidx[3]]-pts[bidx[0]])*norm>0) {
			i=bidx[1]; bidx[1]=bidx[2]; bidx[2]=i;
		}
		for(i=0;i<4;i++) ptmask |= 1<<bidx[i];

		if (!pTris)
			pTris = (index_t*)trbuf;
		pTris[0]=bidx[0]; pTris[1]=bidx[1]; pTris[2]=bidx[2];
		for(i=0;i<3;i++) {
			pTris[i*3+3]=bidx[3]; pTris[i*3+4]=bidx[i]; pTris[i*3+5]=bidx[dec_mod3[i]];
		}

		for(i=0,n=4;i<npts;i++) if (!(ptmask>>i & 1)) {
			for(j=0,nnew=n;j<n;j++) if (e_cansee(pts[i]-pts[pTris[j*3]], pts[pTris[j*3+1]]-pts[pTris[j*3]]^pts[pTris[j*3+2]]-pts[pTris[j*3]])) {
				for(k=0;k<3;k++) bidx[k]=pTris[j*3+k]; // if vertex i sees triangle j
				for(ti=nsplits=0;ti<n;ti++) for(k=0;k<3;k++)
					if ((pTris[ti*3]==bidx[k] && pTris[ti*3+2]==bidx[inc_mod3[k]] || pTris[ti*3+1]==bidx[k] && pTris[ti*3]==bidx[inc_mod3[k]] ||
							 pTris[ti*3+2]==bidx[k] && pTris[ti*3+1]==bidx[inc_mod3[k]]) &&
							!e_cansee(pts[i]-pts[pTris[ti*3]], pts[pTris[ti*3+1]]-pts[pTris[ti*3]]^pts[pTris[ti*3+2]]-pts[pTris[ti*3]]))
					{	// if triangle j shares an edge k with triangle ti and vertex i doesn't see triangle ti
						itri = nsplits ? nnew+nsplits-1:j; nsplits++;
						pTris[itri*3]=i; pTris[itri*3+1]=bidx[k]; pTris[itri*3+2]=bidx[inc_mod3[k]];
						break;
					}
				nnew += max(0,nsplits-1);
			}	n = nnew;

//ptmask|=1<<i;
//for(j1=0;j1<n;j1++) for(i1=0;i1<npts;i1++) if (ptmask>>i1 & 1)
//if (i1!=pTris[j1*3] && i1!=pTris[j1*3+1] && i1!=pTris[j1*3+2] &&
//		e_cansee(pts[i1]-pts[pTris[j1*3]],pts[pTris[j1*3+1]]-pts[pTris[j1*3]]^pts[pTris[j1*3+2]]-pts[pTris[j1*3]]))
//k=0;
		}

		return n;
	}*/

	// select points for initial tetrahedron
	// first, find 6 points corresponding to min and max coordinates
	for(i=1;i<npts;i++) {
		if (pts[i].x>pmax.x) { pmax.x=pts[i].x; bidx[0]=i; }
		if (pts[i].x<pmin.x) { pmin.x=pts[i].x; bidx[1]=i; }
		if (pts[i].y>pmax.y) { pmax.y=pts[i].y; bidx[2]=i; }
		if (pts[i].y<pmin.y) { pmin.y=pts[i].y; bidx[3]=i; }
		if (pts[i].z>pmax.z) { pmax.z=pts[i].z; bidx[4]=i; }
		if (pts[i].z<pmin.z) { pmin.z=pts[i].z; bidx[5]=i; }
	}
//	e = max(max(pmax.x-pmin.x,pmax.y-pmin.y),pmax.z-pmin.z)*0.01f;

	for(bidx[0]=0,i=1;i<npts;i++)
		bidx[0] += i-bidx[0] & -isneg(pts[i].x-pts[bidx[0]].x);
	for(bidx[1]=0,i=1;i<npts;i++)
		bidx[1] += i-bidx[1] & -isneg((pts[bidx[1]]-pts[bidx[0]]).len2()-(pts[i]-pts[bidx[0]]).len2());
	Vec3 norm = pts[bidx[1]]-pts[bidx[0]];
	for(bidx[2]=0,i=1;i<npts;i++)
		bidx[2] += i-bidx[2] & -isneg((norm^pts[bidx[2]]-pts[bidx[0]]).len2()-(norm^pts[i]-pts[bidx[0]]).len2());
	norm = pts[bidx[1]]-pts[bidx[0]] ^ pts[bidx[2]]-pts[bidx[0]];
	for(bidx[3]=0,i=1;i<npts;i++)
		bidx[3] += i-bidx[3] & -isneg(fabs_tpl((pts[bidx[3]]-pts[bidx[0]])*norm)-fabs_tpl((pts[i]-pts[bidx[0]])*norm));
	if ((pts[bidx[3]]-pts[bidx[0]])*norm>0) {
		i=bidx[1]; bidx[1]=bidx[2]; bidx[2]=i;
	}
	/*// select 4 unique points from the 6 found above
	for(k=0;k<2;k++) { 
		for(i=0;i<3;i++) for(j=i+1;j<4;j++) if (bidx[i]==bidx[j]) {
			ti=bidx[4+k]; bidx[4+k]=bidx[j]; bidx[j]=ti;
			goto nextk;
		}	break;
		nextk:;
	}
	if (k==2) { // if 4 unique points cannot be found, select 4 arbitrary points
		bidx[0]=0; bidx[1]=npts/3; bidx[2]=npts*2/3; bidx[3]=npts-1;
	}
	Vec3 cp1 = pts[bidx[1]]-pts[bidx[0]] ^ pts[bidx[2]]-pts[bidx[0]];
	if (sqr(cp1*(pts[bidx[3]]-pts[bidx[0]])) < cube(e)) { // coplanar points, find another 4th poind
		for(i=0,maxdist=0; i<npts; i++) {
			dist = fabs_tpl((pts[i]-pts[bidx[0]])*cp1);
			if (dist > maxdist) {
				maxdist=dist; bidx[3]=i;
			}
		}
	}*/

	// build a double linked list from all points
	for(i=0;i<npts;i++) {
		ptlist[i].prev = ptlist+i-1;
		ptlist[i].next = ptlist+i+1;
	}
	ptlist[0].prev = ptlist+npts-1;	
	ptlist[npts-1].next = ptlist;
	// remove selected points from the list
	for(i=0;i<4;i++) delete_item_from_list(ptlist+bidx[i]);
	// assign 3 points to each of 4 initial triangles
	for(i=0;i<4;i++) {
		for(j=k=0; j<4; j++)
			if (j!=i) trbuf[i].idx[k++]=bidx[j]; // skip i.th point in i.th triangle
		trbuf[i].n = pts[trbuf[i].idx[1]]-pts[trbuf[i].idx[0]] ^ pts[trbuf[i].idx[2]]-pts[trbuf[i].idx[0]];
		trbuf[i].pt0 = pts[trbuf[i].idx[0]];
		if (e_cansee(pts[bidx[i]]-trbuf[i].pt0,trbuf[i].n,0)) { // flip the orientation so that ccw normal points outwards
			ti=trbuf[i].idx[0]; trbuf[i].idx[0]=trbuf[i].idx[2]; trbuf[i].idx[2]=ti;
			trbuf[i].n = -trbuf[i].n; 
		}
		trbuf[i].ptassoc = 0;
		trbuf[i].deleted = 0;
		add_item_to_list(trstart, trbuf+i);
	}
	// fill buddy links for each triangle
	for(i=0;i<4;i++) for(j=0;j<4;j++) if (j!=i) for(k=0;k<3;k++) for(ti=0;ti<3;ti++) 
	if (trbuf[i].idx[k]==trbuf[j].idx[ti] && trbuf[i].idx[k==2?0:k+1]==trbuf[j].idx[ti==0?2:ti-1]) {
		trbuf[i].buddy[k] = trbuf+j; break;
	}
	trend = trstart+4;
	for(i=0;i<4;i++) if (trbuf[i].n.len2()<1E-6f) {
		#ifdef _DEBUG
			//OutputDebugString("WARNING: convex hull not computed because of degenerate initial triangles\n");
		#endif
		n=0; goto endqhull; // some degenerate case, don't party with it
	}
	// associate points with one of the initial triangles
	for(i=0;i<npts;i++) if (ptlist[i].next) break;
	associate_ptlist_with_trilist(ptlist+i, trstart, ptlist,pts);

#define DELETE_TRI(ptri) { \
		merge_lists(ptdeleted, (ptri)->ptassoc); \
		if ((ptri)==trstart) trstart=(ptri)->next; \
		if ((ptri)==trnext) trnext=(ptri)->next; \
		delete_item_from_list(ptri); (ptri)->deleted=1;	\
	}

	// main loop
	iter=0;maxiter=npts*npts*2;	
	ptmax=trstart->ptassoc;
	tr=trstart; do {
		trnext = tr->next;
		pt=tr->ptassoc; if (pt) {
			// find the fartherst of the associated with the triangle points
			maxdist=-1E37f; do {
				if ((dist = pts[(int)(pt-ptlist)]*tr->n) > maxdist) 
				{	maxdist = dist; ptmax = pt;	}
				pt = pt->next;
			} while (pt!=tr->ptassoc);
			ptdeleted = 0; 
			if (tr->ptassoc==ptmax) tr->ptassoc=ptmax->next;
			delete_item_from_list(ptmax);
			if (tr->ptassoc==ptmax) tr->ptassoc=0;

			// find the triangle that the point can see "most confidently"
			tr=trstart; trlast=tr->prev; ti=ptmax-ptlist; maxdist=-1E37f;
			do {
				trnext = tr->next; 
				if ((pts[ti]-tr->pt0)*tr->n > maxdist) {
					maxdist = (pts[ti]-tr->pt0)*tr->n;
					trbest = tr;
				}
				if (tr==trlast) break;
				tr = trnext;
			} while (true);

			// "flood fill" triangles that the point can see around that one
			DELETE_TRI(trbest)
			tr = trbest->next=trbest->prev = trbest;
			do {
				if (tr->buddy[0] && !tr->buddy[0]->deleted && e_cansee(pts[ti]-tr->buddy[0]->pt0,tr->buddy[0]->n,0)) {
					DELETE_TRI(tr->buddy[0])
					add_item_to_list(tr, tr->buddy[0]);
				}
				if (tr->buddy[1] && !tr->buddy[1]->deleted && e_cansee(pts[ti]-tr->buddy[1]->pt0,tr->buddy[1]->n,0)) {
					DELETE_TRI(tr->buddy[1])
					add_item_to_list(tr, tr->buddy[1]);
				}
				if (tr->buddy[2] && !tr->buddy[2]->deleted && e_cansee(pts[ti]-tr->buddy[2]->pt0,tr->buddy[2]->n,0)) {
					DELETE_TRI(tr->buddy[2])
					add_item_to_list(tr, tr->buddy[2]);
				}
				tr = tr->next;
			} while(tr!=trbest);

			/*// delete triangles that the point can see
			tr=trstart; trlast=tr->prev; ti=ptmax-ptlist;
			do {
				trnext = tr->next; 
				if (pts[ti]*tr->n > tr->d) 
					DELETE_TRI(tr)
				if (tr==trlast) break;
				tr = trnext;
			} while (true);*/

			// delete near-visible triangles around deleted area edges to preserve hole convexity
			// do as many iterations as needed
			do {
				tr=trstart; trlast=tr->prev; next_iter=0;
				do {
					trnext = tr->next; 
					if (e_cansee(pts[ti]-tr->pt0,tr->n,-0.001f)) {
						delbuds = tr->buddy[0]->deleted+tr->buddy[1]->deleted+tr->buddy[2]->deleted;
						if (delbuds >= 2) {	// delete triangles that have 2+ buddies deleted
							if (tr==trlast) trlast=tr->next;
							DELETE_TRI(tr);	next_iter=1;
						} else if (delbuds==1) { // follow triangle fan around both shared edge ends
							int bi,bi0,bi1,nfantris,fandir;
							qhtritem *fantris[64],*tr1;

							for(bi0=0; bi0<2 && !tr->buddy[bi0]->deleted; bi0++); // bi0 - deleted buddy index
							for(fandir=-1; fandir<=1; fandir+=2) { // follow fans in 2 possible directions
								tr1=tr; bi1=bi0; nfantris=0;
								do {
									if (nfantris==64)
										break;
									bi=bi1+fandir; if (bi>2) bi-=3; if (bi<0) bi+=3;
									for(bi1=0; bi1<3 && tr1->buddy[bi]->buddy[bi1]!=tr1; bi1++); 
									fantris[nfantris++] = tr1; // store this triangle in a temporary fan list
									tr1=tr1->buddy[bi]; bi=bi1;	// go to the next fan triangle
									if (!e_cansee(pts[ti]-tr1->pt0,tr1->n,-0.002f)) 
										break; // discard this fan
									if (tr1->deleted) {
										if (tr1!=tr->buddy[bi0]) { 
											// delete fan only if it ended on _another_ deleted triangle
											for(--nfantris;nfantris>=0;nfantris--) {
												if (fantris[nfantris]==trlast) trlast=fantris[nfantris]->next;
												DELETE_TRI(fantris[nfantris])
											}
											next_iter=1;
										}	break; // fan end
									}
								} while(true);
							}
						}
					}
					if (tr==trlast) break;
					tr = trnext;
				} while (tr);
			} while (next_iter && trstart);

			if (!trstart || trstart->deleted) {
				#ifdef _DEBUG
					//OutputDebugString("WARNING: convex hull not computed because all triangles were deleted (too thing or too small objects)\n");
				#endif
				n=0; goto endqhull;
			}

			// find triangles that shared an edge with deleted triangles
			trnew=0; tr=trstart; do {
				for(i=0;i<3;i++) if (tr->buddy[i]->deleted) {
					// create a new triangle
					if (trend>=trdata+trdatasz) {
						qhtritem* trdata_new = new qhtritem[trdatasz+=256];
						memcpy(trdata_new, trdata, (trend-trdata)*sizeof(qhtritem));
						intptr_t diff = (intptr_t)trdata_new-(intptr_t)trdata;
						for(n=0;n<trdatasz-256;n++) 
							relocate_tritem(trdata_new+n,diff);
						relocate_ptritem(trend,diff); relocate_ptritem(trstart,diff);
						relocate_ptritem(trnext,diff);relocate_ptritem(tr,diff);
						relocate_ptritem(trbest,diff);relocate_ptritem(trnew,diff);
						if (trdata!=trbuf) delete trdata;
						trdata = trdata_new;
					}
					trend->idx[0] = ptmax-ptlist;
					trend->idx[1] = tr->idx[i==2 ? 0:i+1];
					trend->idx[2] = tr->idx[i];
					trend->ptassoc = 0;	trend->deleted = 0;
					trend->n = pts[trend->idx[1]]-pts[trend->idx[0]] ^ pts[trend->idx[2]]-pts[trend->idx[0]];
					trend->pt0 = pts[trend->idx[0]];
					trend->buddy[1] = tr; tr->buddy[i] = trend;
					trend->buddy[0]=trend->buddy[2]=0;
					add_item_to_list(trnew, trend++);
				}
				tr=tr->next;
			} while (tr!=trstart);

			// sort pointers to the new triangles by their 2nd vertex index
			n=trend-trnew; if (tmparr_sz<n) {
				if (tmparr_idx!=tmparr_idx_buf) delete[] tmparr_idx;
				if (tmparr_ptr!=tmparr_ptr_buf) delete[] tmparr_ptr;
				tmparr_idx = new int[n];
				tmparr_ptr = new qhtritem*[n];
			}
			for(tr=trnew,i=0;tr<trend;tr++,i++) { tmparr_idx[i]=tr->idx[2]; tmparr_ptr[i]=tr; }
			qsort(tmparr_idx,(void**)tmparr_ptr, 0,trend-trnew-1);

			// find 0th buddy for each new triangle (i.e. the triangle, which has its idx[2]==tr->idx[1]
			for(tr=trnew;tr<trend;tr++) {
				i = bin_search(tmparr_idx,n, tr->idx[1]);
				tr->buddy[0] = tmparr_ptr[i];
				tmparr_ptr[i]->buddy[2] = tr;
			}
			for(tr=trnew;tr<trend;tr++)
			if (!tr->buddy[0] || !tr->buddy[2]) {
				#ifdef _DEBUG
				//DEBUG_BREAK;
				//OutputDebugString("WARNING: failed to compute convex hull due to geometric error\n");
				#endif
				goto endqh;
			}

			// assign all points from the deleted triangles to the new triangles
			associate_ptlist_with_trilist(ptdeleted,trnew, ptlist,pts);
			// add new triangles to the list
			merge_lists(trnext, trnew);
		} else if (trnext==trstart)
			break; // all triangles in queue have no associated vertices
		tr = trnext;
	} while (++iter<maxiter);

	#ifdef _DEBUG
	//if (iter>=maxiter)						                                         
		//OutputDebugString("WARNING: failed to compute convex hull during iterations limit\n");
	#endif
	endqh:
	
	// build the final triangle list
	for(tr=trstart,n=1; tr->next!=trstart; tr=tr->next,n++);
	if (!pTris)
		pTris = !qmalloc ? new index_t[n*3] : (index_t*) qmalloc(sizeof(index_t)*n*3);
	i=0; tr=trstart; do {
	  pTris[i]=tr->idx[0]; pTris[i+1]=tr->idx[1]; pTris[i+2]=tr->idx[2];
		tr = tr->next; i+=3;
	} while(tr!=trstart);

	endqhull:
	if (ptlist!=ptbuf) delete[] ptlist;
	if (tmparr_idx!=tmparr_idx_buf) delete[] tmparr_idx;
	if (tmparr_ptr!=tmparr_ptr_buf) delete[] tmparr_ptr;
	if (trdata!=trbuf) delete[] trdata;

	return n;
}


///////////////////////////// Qhull 2d ///////////////////////////////

inline void delete_item(ptitem2d *pitem, ptitem2d *&pref) {
	pitem->next->prev = pitem->prev;
	pitem->prev->next = pitem->next;
	if (pref==pitem)
		pref = pitem->next!=pitem ? pitem->next : 0;
}
inline void add_item(ptitem2d *pitem, ptitem2d *&pref) {
	if (pref) {
		pref->next->prev = pitem; pitem->next = pref->next;
		pref->next = pitem; pitem->prev = pref;
	} else {
		pitem->next=pitem->prev = pitem;
		pref = pitem;
	}
}

static inline void swap(edgeitem *plist, int i1,int i2) {
	int ti=plist[i1].idx; plist[i1].idx=plist[i2].idx; plist[i2].idx=ti;
}
static void qsort(edgeitem *plist, int left,int right)
{
	if (left>=right) return;
	int i,last; 
	swap(plist, left, left+right>>1);
	for(last=left,i=left+1; i<=right; i++)
	if (plist[plist[i].idx].area < plist[plist[left].idx].area)
		swap(plist, ++last, i);
	swap(plist, left,last);

	qsort(plist, left,last-1);
	qsort(plist, last+1,right);
}


int qhull2d(ptitem2d *pts,int nVtx, edgeitem *edges, int nMaxContacts)
{
	intptr_t imask;
	int i,j,i0,i1,nEdges;
	Vec2 edge;
	ptitem2d *ppt,*ppt_best,*ppt_next,*plist;
	edgeitem *pedge,*pstart,*pend,*pminedge,*pnewedge;

	for(i=i0=0;i<nVtx;i++) { // find the 1st point - the one w/ max y
		imask = -isneg(pts[i0].pt.y-pts[i].pt.y);
		i0 = i&imask | i0&~imask;
	}
	// find the 2nd point - the one farthest from the 1st one
	for(i1=0,i=1; i<nVtx; i++) {
		imask = -isneg(len2(pts[i1].pt-pts[i0].pt)-len2(pts[i].pt-pts[i0].pt));
		i1 = i&imask | i1&~imask;
	}
	// form the 1st two edges
	edges[0].prev=edges[0].next=edges+1; edges[0].pvtx=pts+i0; edges[0].plist=0;
	edges[1].prev=edges[1].next=edges+0; edges[1].pvtx=pts+i1; edges[1].plist=0;
	edge = pts[i1].pt-pts[i0].pt;
	for(i=0; i<nVtx; i++) if ((i-i0)*(i-i1))
		add_item(pts+i,edges[isneg(pts[i].pt-pts[i0].pt ^ edge)].plist);
	nEdges = 2;
	
	do {
		for(i=0;i<nEdges && !edges[i].plist;i++);
		if (i==nEdges)
			break;
		// find up the farthest point associated with the edge
		edge = edges[i].next->pvtx->pt-edges[i].pvtx->pt; ppt=ppt_best = edges[i].plist;
		do {
			imask = -(intptr_t)isneg((ppt_best->pt-edges[i].pvtx->pt^edge) - (ppt->pt-edges[i].pvtx->pt^edge));
			ppt_best = (ptitem2d*)((intptr_t)ppt&imask | (intptr_t)ppt_best&~imask);
		} while((ppt=ppt->next)!=edges[i].plist);
		// trace contour from i cw while edges are facing the point ppt_best (pstart - 1st edge to be deleted)
		/*for(pstart=edges+i; pstart->prev!=edges+i && 
			(ppt_best->pt-pstart->prev->pvtx->pt ^ pstart->pvtx->pt-pstart->prev->pvtx->pt)>0; pstart=pstart->prev);
		// trace contour from i ccw while edges are facing the point ppt_best (pend - edge after the last edge to be deleted)
		for(pend=edges[i].next; pend!=edges+i && 
			(ppt_best->pt-pend->pvtx->pt ^ pend->next->pvtx->pt-pend->pvtx->pt)>0; pend=pend->next);*/
		pstart=edges+i; pend=edges[i].next;
		// delete point ppt_best from the ith edge associated list
		delete_item(ppt_best,edges[i].plist);
		// merge point lists for edges pstart-pend
		for(pedge=pstart,plist=0; pedge!=pend && !pedge->plist; pedge=pedge->next);
		if (pedge!=pend) for(plist=pedge->plist,pedge->plist=0,pedge=pedge->next; pedge!=pend; pedge=pedge->next) if (pedge->plist) {
			plist->next->prev = pedge->plist->prev; pedge->plist->prev->next = plist->next;
			plist->next = pedge->plist; pedge->plist->prev = plist;
			pedge->plist = 0;
		}
		// create a new edge between pstart and pend (first one will override pstart)
		pstart->next=edges+nEdges; edges[nEdges].prev=pstart; 
		pend->prev = edges+nEdges; edges[nEdges].next=pend;
		edges[nEdges].pvtx = ppt_best;
		pstart->plist = edges[nEdges].plist = 0;
		// associate some points from that list with one of the 2 new edges
		if (plist) {
			LOCAL_NAME_OVERRIDE_OK
			edgeitem *pedge[2]; Vec2 edge[2]; float dist[2];
			ppt = plist; pedge[0] = pstart; pedge[1] = edges+nEdges;
			edge[0] = pedge[0]->next->pvtx->pt-pedge[0]->pvtx->pt;
			edge[1] = pedge[1]->next->pvtx->pt-pedge[1]->pvtx->pt;
			do {
				ppt_next = ppt->next;
				dist[0] = (ppt->pt-pedge[0]->pvtx->pt ^ edge[0])*len2(edge[1]);
				dist[1] = (ppt->pt-pedge[1]->pvtx->pt ^ edge[1])*len2(edge[0]);
				i = isneg(dist[0]-dist[1]);
				if (dist[i]>0)
					add_item(ppt,pedge[i]->plist);
			}	while((ppt=ppt_next)!=plist);
		}
		nEdges++;
	} while(true);

	if (nMaxContacts) { // chop off vertices until we have only the required amount left
		Vec2 edge[2];
		for(i=0;i<nEdges;i++)	{
			edge[0] = edges[i].prev->pvtx->pt-edges[i].pvtx->pt; edge[1] = edges[i].next->pvtx->pt-edges[i].pvtx->pt;
			edges[i].area = edge[1] ^ edge[0]; edges[i].areanorm2 = len2(edge[0])*len2(edge[1]);
			edges[i].idx = i;
		}
		// sort edges by the area of triangles
		qsort(edges,0,nEdges-1);
		// transform sorted array into a linked list
		pminedge = edges+edges[0].idx;
		pminedge->next1=pminedge->prev1 = edges+edges[0].idx; 
		for(pedge=pminedge,i=1; i<nEdges; i++) {
			edges[edges[i].idx].prev1 = pedge; edges[edges[i].idx].next1 = pedge->next1;
			pedge->next1->prev1 = edges+edges[i].idx; pedge->next1 = edges+edges[i].idx; pedge = edges+edges[i].idx;
		}
		
		while(nEdges>2 && (nEdges>nMaxContacts || sqr(pminedge->area)<sqr(0.15f)*pminedge->areanorm2)) {
			LOCAL_NAME_OVERRIDE_OK
			edgeitem *pedge[2];
			// delete the ith vertex with the minimum area triangle
			pminedge->next->prev = pminedge->prev; pminedge->prev->next = pminedge->next;
			pminedge->next1->prev1 = pminedge->prev1; pminedge->prev1->next1 = pminedge->next1;
			pedge[0] = pminedge->prev; pedge[1] = pminedge->next; 
			pminedge->next=pminedge->prev = 0; pminedge = pminedge->next1;
			nEdges--;
			for(i=0;i<2;i++) {
				edge[0] = pedge[i]->prev->pvtx->pt-pedge[i]->pvtx->pt; edge[1] = pedge[i]->next->pvtx->pt-pedge[i]->pvtx->pt;
				pedge[i]->area = edge[1]^edge[0]; pedge[i]->areanorm2 = len2(edge[0])*len2(edge[1]); // update area
				for(pnewedge=pedge[i]->next1,j=0; pnewedge!=pminedge && pnewedge->area<pedge[i]->area && j<nEdges+1; pnewedge=pnewedge->next1,j++);
				if (pedge[i]->next1!=pnewedge) { // re-insert pedge[i] before pnewedge
					if (pedge[i]==pminedge) pminedge = pedge[i]->next1;
					if (pedge[i]!=pnewedge) {
						pedge[i]->next1->prev1 = pedge[i]->prev1; pedge[i]->prev1->next1 = pedge[i]->next1;	
						pedge[i]->prev1 = pnewedge->prev1; pedge[i]->next1 = pnewedge;
						pnewedge->prev1->next1 = pedge[i]; pnewedge->prev1 = pedge[i];
					}
				}	else {
					for(pnewedge=pedge[i]->prev1,j=0; pnewedge->next1!=pminedge && pnewedge->area>pedge[i]->area && j<nEdges+1; pnewedge=pnewedge->prev1,j++);
					if (pedge[i]->prev1!=pnewedge) { // re-insert pedge[i] after pnewedge
						if (pnewedge->next1==pminedge) pminedge = pedge[i];
						if (pedge[i]!=pnewedge) {
							pedge[i]->next1->prev1 = pedge[i]->prev1; pedge[i]->prev1->next1 = pedge[i]->next1;	
							pedge[i]->prev1 = pnewedge; pedge[i]->next1 = pnewedge->next1;
							pnewedge->next1->prev1 = pedge[i]; pnewedge->next1 = pedge[i];
						}
					}
				}
			}
		}
	}

	return nEdges;
}
