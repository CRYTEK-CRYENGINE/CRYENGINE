// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"

#if defined(USE_MATRIX_POOLS)
	DECLARE_MTXNxM_POOL(float,512)
	DECLARE_VECTORN_POOL(float,256)
#ifndef REAL_IS_FLOAT
	DECLARE_MTXNxM_POOL(real,512)
	DECLARE_VECTORN_POOL(real,256) 
#endif
#endif 

int g_bHasSSE;
inline float getlothresh(float) { return 1E-10f; }
inline float gethithresh(float) { return 1E10f; }
inline double getlothresh(double) { return 1E-20; }
inline double gethithresh(double) { return 1E20; }

template<class ftype>
int matrix_tpl<ftype>::jacobi_transformation(matrix_tpl<ftype> &evec, ftype *eval, ftype prec) const
{
	if (!(flags & mtx_symmetric) || nCols!=nRows) return 0;

	matrix_tpl a(*this);
	int n = nRows, p,q,r,iter,pmax,qmax,sz=nRows*nCols;
	ftype theta,t,s,c,apr,aqr,arp,arq,thresh=prec,amax;
	evec.identity();
	evec.flags = (evec.flags & mtx_foreign_data) | mtx_orthogonal | mtx_normal;
	
	for(iter=0; iter<nRows*nCols*10; iter++) {
		for(p=0,amax=thresh,pmax=-1;p<n-1;p++) for(q=p+1;q<n;q++)	if (sqr(a[p][q])>amax) 
		{	amax=sqr(a[p][q]); pmax=p; qmax=q; }
		if (pmax==-1) 
			goto exitjacobi;
		p=pmax; q=qmax;
		theta = (ftype)0.5*(a[q][q]-a[p][p])/a[p][q];
		if (fabs_tpl(theta)<gethithresh(theta)) {
			t = sqrt_tpl(theta*theta+1);
			if (theta>0) t = -theta-t;
			else t -= theta;
			c = 1/sqrt_tpl(1+t*t); s = t*c;
			for(r=0;r<n;r++) { arp=a[r][p];arq=a[r][q]; a[r][p]=c*arp-s*arq; a[r][q]=c*arq+s*arp; }
			for(r=0;r<n;r++) { apr=a[p][r];aqr=a[q][r]; a[p][r]=c*apr-s*aqr; a[q][r]=c*aqr+s*apr; }
			for(r=0;r<n;r++) { apr=evec[p][r];aqr=evec[q][r]; evec[p][r]=c*apr-s*aqr; evec[q][r]=c*aqr+s*apr; }
		}
		a[p][q] = 0;
		if (iter==sz+1) thresh += getlothresh(thresh);
	}
	iter=0;
	exitjacobi:
	for(p=0;p<n*n;p++) {
		t = fabs_tpl(evec.data[p]);
		if (t<(ftype)1E-6) evec.data[p]=0;
		else if (fabs_tpl(t-1)<getlothresh(t)) 
			evec.data[p]=sgnnz(evec.data[p]);
	}
	for(p=0;p<n;p++) eval[p] = a[p][p];
	return iter; // not converged during iterations limit
}

template<class ftype>
int matrix_tpl<ftype>::conjugate_gradient(ftype *startx,ftype *rightside, ftype minlen,ftype minel) const
{
	ftype a,b,r2,r2new,denom,maxel;
	int i,iter=nRows*3;
	minlen*=minlen; 

	ftype buf[24],*pbuf = nRows>8 ? new ftype[nRows*3]:buf;
	vectorn_tpl<ftype> x(nRows,startx),rh(nRows,rightside),r(nRows,pbuf),p(nRows,pbuf+nRows),Ap(nRows,pbuf+nRows*2);
	(r=rh)-=*this*x; p=r;
	r2=r.len2();

	do {
		Ap = *this*p; denom = p*Ap;
		if (sqr(denom)<1E-30) break;
		a = r2/denom;
		r -= Ap*a;
		r2new=r.len2();
		if (r2new>r2*500)
			break;
		x += p*a;
		b = r2new/r2; r2=r2new;
		(p*=b)+=r;
		for(i=0,maxel=0;i<nRows;i++) maxel = max(maxel,fabs_tpl(r[i]));
	} while (--iter && (r2new>minlen || maxel>minel));

	if (pbuf!=buf) delete pbuf;
	return nRows-iter;
}

template<class ftype>
int matrix_tpl<ftype>::biconjugate_gradient(ftype *startx,ftype *rightside, ftype minlen,ftype minel) const
{
	ftype a,b,r2,r2new,err,denom,maxel;
	int i,iter=nRows*3;
	minlen*=minlen; 

	ftype buf[48],*pbuf = nRows>8 ? new ftype[nRows*6]:buf;
	vectorn_tpl<ftype> x(nRows,startx),rh(nRows,rightside),r(nRows,pbuf),rc(nRows,pbuf+nRows),p(nRows,pbuf+nRows*2),
		pc(nRows,pbuf+nRows*3),Ap(nRows,pbuf+nRows*4),tmp(nRows,pbuf+nRows*5);
	(r=rh)-=*this*x; rc=r; p=r; pc=rc; r2 = r.len2();

	do {
		Ap = *this*p;	denom = pc*Ap;
		if (sqr(denom)<1E-30) break;
		a = r2/denom;
		r -= Ap*a;
		rc -= (tmp=pc**this)*a;
		x += p*a;
		r2new = rc*r;
		b = r2new/r2;	r2 = r2new;
		(p*=b)+=r; (pc*=b)+=rc;
		for(err=maxel=0,i=0;i<nRows;i++) {
			err += sqr(r.data[i]);
			maxel = max(maxel,fabs_tpl(r[i]));
		}
	} while(--iter && (err>minlen || maxel>minel) && sqr(r2)>1E-30);

	if (pbuf!=buf) delete pbuf;
	return nRows-iter;
}

template<class ftype>
int matrix_tpl<ftype>::minimum_residual(ftype *startx,ftype *rightside, ftype minlen,ftype minel) const
{
	ftype a,b,r2,r2new,err,denom,maxel;
	int i,iter=nRows*3;
	minlen*=minlen; 

	ftype buf[40],*pbuf = nRows>8 ? new ftype[nRows*5]:buf;
	vectorn_tpl<ftype> x(nRows,startx),rh(nRows,rightside),r(nRows,pbuf),rc(nRows,pbuf+nRows),p(nRows,pbuf+nRows*2),
		Ap(nRows,pbuf+nRows*4);
	(r=rh)-=*this*x; 
	rc=*this*r; 
	p=r; 
	r2 = rc*r;
	do {
		Ap = *this*p;	denom = Ap.len2();
		if (sqr(denom)<1E-30) break;
		a = r2/denom;
		r -= Ap*a; rc = *this*r;
		x += p*a;
		r2new = rc*r;
		b = r2new/r2;	r2 = r2new;
		(p*=b)+=r;
		for(err=maxel=0,i=0;i<nRows;i++) {
			err += sqr(r.data[i]);
			maxel = max(maxel,fabs_tpl(r[i]));
		}
	} while(--iter && (err>minlen || maxel>minel) && sqr(r2)>1E-30);

	if (pbuf!=buf) delete pbuf;
	return nRows-iter;
}

template<class ftype>
int matrix_tpl<ftype>::LUdecomposition(ftype *&LUdata,int *&LUidx) const
{
	if (nRows!=nCols) return 0;
	int i,j,k,imax,alloc=((LUdata==0) | (LUidx==0)<<1);
	ftype aij,bij,maxaij,t;
	if (alloc & 1) LUdata = new ftype[nRows*nRows];
	if (alloc & 2) LUidx = new int[nRows];
	matrix_tpl<ftype> LU(nRows,nRows,0,LUdata);
	LU = *this;

	for(j=0;j<nRows;j++) {
		for(i=0;i<=j;i++) {
			for(k=0,bij=LU[i][j]; k<i; k++) bij -= LU[i][k]*LU[k][j];
			LU[i][j] = bij;
		}
		for(maxaij=0,imax=j ;i<nRows;i++) {
			for(k=0,aij=LU[i][j]; k<j; k++) aij -= LU[i][k]*LU[k][j];
			LU[i][j] = aij;
			if (aij*aij > maxaij*maxaij) 
			{ maxaij=aij; imax=i; }
		}
		LUidx[j] = imax;
		if (j==nRows-1 && LU[j][j]!=0) break; // no aij in this case
		if (maxaij==0) { // the matrix is singular
			if (alloc & 1) delete[] LUdata;
			if (alloc & 2) delete[] LUidx;
			return 0;
		}
		if (imax!=j) for(k=0;k<nRows;k++) 
		{ t=LU[imax][k]; LU[imax][k]=LU[j][k]; LU[j][k]=t; }
		maxaij = (ftype)1.0/maxaij;
		for(i=j+1;i<nRows;i++)
			LU[i][j] *= maxaij;
	}
	return 1;
}

template<class ftype>
int matrix_tpl<ftype>::solveAx_b(ftype *x,ftype *b, ftype *LUdata,int *LUidx) const
{
	int LUidx_buf[16],alloc=0;
  if (!LUdata) {
    if (nRows*nRows*2<CRY_ARRAY_COUNT(mtx_pool)) {
      if (mtx_pool_pos+nRows*nRows > CRY_ARRAY_COUNT(mtx_pool))
        mtx_pool_pos = 0;
      LUdata = mtx_pool+mtx_pool_pos; mtx_pool_pos += nRows*nRows;
    }
    if (nRows<=CRY_ARRAY_COUNT(LUidx_buf)) LUidx = LUidx_buf;
    alloc = (LUdata==0) | (LUidx==0)<<1;
    if (!LUdecomposition(LUdata,LUidx)) return 0;
  }

	int i,j; ftype xi;
	matrix_tpl<ftype> LU(nRows,nRows,0,LUdata);
	for(i=0;i<nRows;i++) x[i]=b[i];
	for(i=0;i<nRows;i++) {
		xi=x[i]; x[i]=x[LUidx[i]]; x[LUidx[i]]=xi;
		for(j=0;j<i;j++) x[i]-=LU[i][j]*x[j];
	}
	for(i=nRows-1;i>=0;i--) {
		for(j=i+1;j<nRows;j++) x[i]-=LU[i][j]*x[j];
		x[i] /= LU[i][i];
	}

	if (alloc & 1) delete[] LUdata;
	if (alloc & 2) delete[] LUidx;
	return 1;
}

template<class ftype>
matrix_tpl<ftype>& matrix_tpl<ftype>::invert()
{
	int i,j; ftype det=0;
	if (flags & mtx_orthogonal) { 
		transpose();
    goto done; 
  }
	if (nRows!=nCols)
		goto done; 
	
	if (nRows==1)
		data[0]=(ftype)1.0/data[0];
	else if (nRows==2) {
		det = data[0]*data[3]-data[1]*data[2];
		if (det==0) goto done;	det = (ftype)1.0/det;
		ftype oldata[4]; for(i=0;i<4;i++) oldata[i]=data[i];
		data[0]=oldata[3]*det; data[1]=-oldata[1]*det;
		data[2]=-oldata[2]*det; data[3]=oldata[0]*det;
	} else if (nRows==3) {
		for(i=0;i<3;i++) det+=data[i]*data[inc_mod3[i]+3]*data[dec_mod3[i]+6];
		for(i=0;i<3;i++) det-=data[dec_mod3[i]]*data[inc_mod3[i]+3]*data[i+6];
		if (det==0) goto done;	det = (ftype)1.0/det;
		ftype oldata[9]; for(i=0;i<9;i++) oldata[i]=data[i];
		for(i=0;i<3;i++) for(j=0;j<3;j++)
			data[i+j*3] = (oldata[dec_mod3[i]*3+dec_mod3[j]]*oldata[inc_mod3[i]*3+inc_mod3[j]]-
										 oldata[dec_mod3[i]*3+inc_mod3[j]]*oldata[inc_mod3[i]*3+dec_mod3[j]])*det;
	} else {
	    ftype *LUdata=0,*colmark; int *LUidx=0,LUidx_buf[32], alloc=0;
	    if (nRows*nRows*2<CRY_ARRAY_COUNT(mtx_pool)) {
	      if (mtx_pool_pos+nRows*nRows > CRY_ARRAY_COUNT(mtx_pool))
	        mtx_pool_pos = 0;
	      LUdata = mtx_pool+mtx_pool_pos; mtx_pool_pos += nRows*nRows;
	    } else alloc = 1;
	    if (nRows<=CRY_ARRAY_COUNT(LUidx_buf)) LUidx = LUidx_buf;
	    else alloc |= 2;
	    if (!LUdecomposition(LUdata,LUidx)) goto done;
	 
	    if (nRows*2<CRY_ARRAY_COUNT(mtx_pool)) {
	      if (mtx_pool_pos+nRows > CRY_ARRAY_COUNT(mtx_pool))
	        mtx_pool_pos=0;
	      colmark=mtx_pool+mtx_pool_pos; mtx_pool_pos+=nRows;
	    } else {
	      colmark = new ftype[nRows]; alloc |= 4;
	    }

		for(i=0;i<nRows;i++) colmark[i]=0;
		for(i=0;i<nRows;i++) {
			colmark[i]=1;	solveAx_b(data+i*nRows,colmark, LUdata,LUidx); colmark[i]=0;
		}
		transpose();

		if (alloc & 1) delete[] LUdata;
		if (alloc & 2) delete[] LUidx;
		if (alloc & 4) delete[] colmark;
	}
	flags = flags & (mtx_normal | mtx_orthogonal | mtx_symmetric | mtx_PD | mtx_PSD | mtx_diagonal | mtx_identity | mtx_foreign_data);

done:
	return *this;
}

template<class ftype>
ftype matrix_tpl<ftype>::determinant(ftype *LUdata,int *LUidx) const
{
	if (nRows!=nCols) return 0;
	int i,j; ftype det=0;
	if (nRows==1)
		det = data[0];
	else if (nRows==2)
		det = data[0]*data[3]-data[1]*data[2];
	else if (nRows==3) {
		for(i=0;i<3;i++) det+=data[i]*data[inc_mod3[i]+3]*data[dec_mod3[i]+6];
		for(i=0;i<3;i++) det-=data[dec_mod3[i]]*data[inc_mod3[i]+3]*data[i+6];
	} else {
		int LUidx_buf[32],alloc=0;
		if (!LUdata || !LUidx) { 
			if (nRows*nRows*2<CRY_ARRAY_COUNT(mtx_pool)) {
				if (mtx_pool_pos+nRows*nRows > CRY_ARRAY_COUNT(mtx_pool))
					mtx_pool_pos = 0;
				LUdata = mtx_pool+mtx_pool_pos; mtx_pool_pos += nRows*nRows;
			} else alloc = 1;
			if (nRows<=CRY_ARRAY_COUNT(LUidx_buf)) LUidx = LUidx_buf;
			else alloc |= 2;
			if (!LUdecomposition(LUdata,LUidx)) return 0;
		}

		for(i=j=0,det=1;i<nRows;i++,j+=nRows+1) det*=LUdata[j];
		for(i=j=0;i<nRows;i++) j+=LUidx[i]!=i;
		if (j&2) det*=-1;

		if (alloc & 1) delete[] LUdata;
		if (alloc & 2) delete[] LUidx;
	}
	return det;
}

// Linear programming with simplex method

template<class ftype>
int matrix_tpl<ftype>::LPsimplex(int m1,int m2, ftype &objfunout,ftype *xout, int nvars, ftype e) const
{
	int M=nRows-2,N=nCols-1,i,j,imax,jmax,iobjfun=M, res=0,iter=(M+N)*8;
	ftype rpivot,t;
	const int imask=0x7FFFFFFF;
	int *irow=new int[M],*icol=new int[N];
	#define a (*this)

	if (e<0) {
		for(i=nRows*nCols-1,e=0;i>=0;i--)	e+=data[i];
		e *= (ftype)1E-6/(nRows*nCols);
	}

	if (nvars<0) nvars = N+m1+m2;
	if (m1<M) {
		iobjfun++;
		for(j=0;j<N;j++) {
			for(t=0,i=0;i<M;i++) t-=a[i][j];
			a[iobjfun][j] = t;
		}
	}
	for(i=0;i<N;i++) icol[i]=i;
	for(i=0;i<M;i++) irow[i]=N+i|~imask;

	do {
		for(t=0,j=0,jmax=-1;j<N;j++) if(icol[j]<nvars && a[iobjfun][j]>t)
		{	jmax=j; t=a[iobjfun][j]; }
		if (jmax<0)
		{ res=1; break;	}
		for(imax=0; imax<M && a[imax][jmax]>0; imax++);
		if (imax==M)
		{ res=2; break; }
		for(i=imax+1;i<M;i++) if (a[i][jmax]<0 && a[i][N]*a[imax][jmax] < a[imax][N]*a[i][jmax])
			imax=i;
		varexchange:
		rpivot = (ftype)1.0/a[imax][jmax];
		for(j=0;j<nCols;j++) a[imax][j]*=-rpivot;
		a[imax][jmax] = rpivot;
		for(i=0;i<nRows;i++) if(i!=imax) {
			a[i][jmax] *= rpivot;
			for(j=0;j<nCols;j++) if(j!=jmax)
				a[i][j] -= a[imax][j]*a[i][jmax];
		}

		if ((irow[imax]&imask)>=N+m1 && (irow[imax]&imask)<N+m1+m2 && (irow[imax]&~imask)!=0) {
			a[iobjfun][jmax] += 1; // remove slack variable from objective function, nonnegativity constraint will enforce feasibility
			for(i=0;i<=M;i++) a[i][jmax]*=-1;
		}

		i=irow[imax]&imask; irow[imax]=icol[jmax]; icol[jmax]=i;
	} while(--iter);

	if (m1<M) {
		if (res==2 || sqr(a[M][N])<e*e)
			res = 0;
		else {
			for(i=0;i<M;i++) {
				if ((irow[i]&imask)>M+m1+m2) {
					for(jmax=0;jmax<N && icol[jmax]>=nvars;jmax++);
					imax=i; goto varexchange;
				} else if ((irow[i]&imask)>N+m1 && (irow[i]&~imask)!=0) {
					for(j=0;j<N;j++) a[i][j]*=-1;
					irow[i]&=imask;
				}
			}
			res = LPsimplex(M,0, objfunout,xout, nvars, e);
		}
	} else {
		if (xout) {
			for(i=0;i<N;i++) xout[i]=0;
			for(i=0;i<M;i++) if((irow[i]&imask)<N)
				xout[irow[i]&imask] = a[i][N];
		}
		objfunout = a[iobjfun][N];
	}
	delete[] irow; delete[] icol;
	return res;

	#undef a
}

#ifdef PIII_SSE
// TODO: AMD64: put into a separate file

// SSE-optimized code from "Streaming SIMD Extensions - Matrix Multiplication" document by Intel Corp.
__declspec(naked) void PIII_Mult00_6x6_6x6(float *src1, float *src2, float *dst)
{
	__asm {
	mov ecx, dword ptr [esp+8] // src2
	movlps xmm3, qword ptr [ecx+72]
	mov edx, dword ptr [esp+4] // src1
	// Loading first 4 columns (upper 4 rows) of src2.
	movaps xmm0, xmmword ptr [ecx]
	movlps xmm1, qword ptr [ecx+24]
	movhps xmm1, qword ptr [ecx+32]
	movaps xmm2, xmmword ptr [ecx+48]
	movhps xmm3, qword ptr [ecx+80]
	// Calculating first 4 elements in the first row of the destination matrix.
	movss xmm4, dword ptr [edx]
	movss xmm5, dword ptr [edx+4]
	mov eax, dword ptr [esp+0Ch] // dst
	shufps xmm4, xmm4, 0
	movss xmm6, dword ptr [edx+8]
	shufps xmm5, xmm5, 0
	movss xmm7, dword ptr [edx+12]
	mulps xmm4, xmm0
	shufps xmm6, xmm6, 0
	shufps xmm7, xmm7, 0
	mulps xmm5, xmm1
	mulps xmm6, xmm2
	addps xmm5, xmm4
	mulps xmm7, xmm3
	addps xmm6, xmm5
	addps xmm7, xmm6
	movaps xmmword ptr [eax], xmm7
	// Calculating first 4 elements in the second row of the destination matrix.
	movss xmm4, dword ptr [edx+24]
	shufps xmm4, xmm4, 0
	mulps xmm4, xmm0
	movss xmm5, dword ptr [edx+28]
	shufps xmm5, xmm5, 0
	mulps xmm5, xmm1
	movss xmm6, dword ptr [edx+32]
	shufps xmm6, xmm6, 0
	movss xmm7, dword ptr [edx+36]
	shufps xmm7, xmm7, 0
	mulps xmm6, xmm2
	mulps xmm7, xmm3
	addps xmm7, xmm6
	addps xmm5, xmm4
	addps xmm7, xmm5
	// Calculating first 4 elements in the third row of the destination matrix.
	movss xmm4, dword ptr [edx+48]
	movss xmm5, dword ptr [edx+52]
	movlps qword ptr [eax+24], xmm7 // save 2nd
	movhps qword ptr [eax+32], xmm7 // row
	movss xmm6, dword ptr [edx+56]
	movss xmm7, dword ptr [edx+60]
	shufps xmm4, xmm4, 0
	shufps xmm5, xmm5, 0
	shufps xmm6, xmm6, 0
	shufps xmm7, xmm7, 0
	mulps xmm4, xmm0
	mulps xmm5, xmm1
	mulps xmm6, xmm2
	mulps xmm7, xmm3
	addps xmm5, xmm4

	addps xmm7, xmm6
	addps xmm7, xmm5
	movaps xmmword ptr [eax+48], xmm7
	// Calculating first 4 elements in the fourth row of the destination matrix.
	movss xmm4, dword ptr [edx+72]
	movss xmm5, dword ptr [edx+76]
	movss xmm6, dword ptr [edx+80]
	movss xmm7, dword ptr [edx+84]
	shufps xmm4, xmm4, 0
	shufps xmm5, xmm5, 0
	shufps xmm6, xmm6, 0
	shufps xmm7, xmm7, 0
	mulps xmm4, xmm0
	mulps xmm5, xmm1
	mulps xmm6, xmm2
	mulps xmm7, xmm3
	addps xmm4, xmm5
	addps xmm6, xmm4
	addps xmm7, xmm6
	movlps qword ptr [eax+72], xmm7
	movhps qword ptr [eax+80], xmm7
	// Calculating first 4 elements in the fifth row of the destination matrix.
	movss xmm4, dword ptr [edx+96]
	movss xmm5, dword ptr [edx+100]
	movss xmm6, dword ptr [edx+104]
	movss xmm7, dword ptr [edx+108]
	shufps xmm4, xmm4, 0
	shufps xmm5, xmm5, 0
	shufps xmm6, xmm6, 0
	shufps xmm7, xmm7, 0
	mulps xmm4, xmm0
	mulps xmm5, xmm1
	mulps xmm6, xmm2
	mulps xmm7, xmm3
	addps xmm5, xmm4
	addps xmm7, xmm6
	addps xmm7, xmm5
	movaps xmmword ptr [eax+96], xmm7
	// Calculating first 4 elements in the sixth row of the destination matrix.
	movss xmm4, dword ptr [edx+120]
	movss xmm5, dword ptr [edx+124]
	movss xmm6, dword ptr [edx+128]
	movss xmm7, dword ptr [edx+132]
	shufps xmm4, xmm4, 0
	shufps xmm5, xmm5, 0
	shufps xmm6, xmm6, 0
	shufps xmm7, xmm7, 0
	mulps xmm4, xmm0
	mulps xmm5, xmm1
	mulps xmm6, xmm2
	mulps xmm7, xmm3
	addps xmm4, xmm5
	addps xmm6, xmm4
	addps xmm7, xmm6
	movhps qword ptr [eax+128], xmm7
	movlps qword ptr [eax+120], xmm7
	// Loading first 4 columns (lower 2 rows) of src2.
	movlps xmm0, qword ptr [ecx+96]
	movhps xmm0, qword ptr [ecx+104]
	movlps xmm1, qword ptr [ecx+120]
	movhps xmm1, qword ptr [ecx+128]
	// Calculating first 4 elements in the first row of the destination matrix.
	movss xmm2, dword ptr [edx+16]
	shufps xmm2, xmm2, 0
	movss xmm4, dword ptr [edx+40]
	movss xmm3, dword ptr [edx+20]
	movss xmm5, dword ptr [edx+44]
	movaps xmm6, xmmword ptr [eax]
	movlps xmm7, qword ptr [eax+24]
	shufps xmm3, xmm3, 0
	shufps xmm5, xmm5, 0
	movhps xmm7, qword ptr [eax+32]
	shufps xmm4, xmm4, 0
	mulps xmm5, xmm1
	mulps xmm2, xmm0
	mulps xmm3, xmm1
	mulps xmm4, xmm0
	addps xmm6, xmm2
	addps xmm7, xmm4
	addps xmm7, xmm5
	addps xmm6, xmm3
	movlps qword ptr [eax+24], xmm7
	movaps xmmword ptr [eax], xmm6
	movhps qword ptr [eax+32], xmm7
	// Calculating first 4 elements in the third row of the destination matrix.
	movss xmm2, dword ptr [edx+64]
	movss xmm4, dword ptr [edx+88]
	movss xmm5, dword ptr [edx+92]
	movss xmm3, dword ptr [edx+68]
	movaps xmm6, xmmword ptr [eax+48]
	movlps xmm7, qword ptr [eax+72]
	movhps xmm7, qword ptr [eax+80]
	shufps xmm2, xmm2, 0
	shufps xmm4, xmm4, 0
	shufps xmm5, xmm5, 0
	shufps xmm3, xmm3, 0
	mulps xmm2, xmm0
	mulps xmm4, xmm0
	mulps xmm5, xmm1
	mulps xmm3, xmm1
	addps xmm6, xmm2
	addps xmm6, xmm3
	addps xmm7, xmm4
	addps xmm7, xmm5
	movlps qword ptr [eax+72], xmm7
	movaps xmmword ptr [eax+48], xmm6
	movhps qword ptr [eax+80], xmm7
	// Calculating first 4 elements in the fifth row of the destination matrix.
	movss xmm2, dword ptr [edx+112]
	movss xmm3, dword ptr [edx+116]
	movaps xmm6, xmmword ptr [eax+96]
	shufps xmm2, xmm2, 0
	shufps xmm3, xmm3, 0
	mulps xmm2, xmm0
	mulps xmm3, xmm1
	addps xmm6, xmm2
	addps xmm6, xmm3
	movaps xmmword ptr [eax+96], xmm6
	// Calculating first 4 elements in the sixth row of the destination matrix.
	movss xmm4, dword ptr [edx+136]
	movss xmm5, dword ptr [edx+140]
	movhps xmm7, qword ptr [eax+128]
	movlps xmm7, qword ptr [eax+120]
	shufps xmm4, xmm4, 0
	shufps xmm5, xmm5, 0
	mulps xmm4, xmm0
	mulps xmm5, xmm1
	addps xmm7, xmm4
	addps xmm7, xmm5
	// Calculating last 2 columns of the destination matrix.
	movlps xmm0, qword ptr [ecx+16]
	movhps xmm0, qword ptr [ecx+40]
	movhps qword ptr [eax+128], xmm7
	movlps qword ptr [eax+120], xmm7
	movlps xmm2, qword ptr [ecx+64]
	movhps xmm2, qword ptr [ecx+88]
	movaps xmm3, xmm2
	shufps xmm3, xmm3, 4Eh
	movlps xmm4, qword ptr [ecx+112]
	movhps xmm4, qword ptr [ecx+136]
	movaps xmm5, xmm4
	shufps xmm5, xmm5, 4Eh
	movlps xmm6, qword ptr [edx]
	movhps xmm6, qword ptr [edx+24]
	movaps xmm7, xmm6
	shufps xmm7, xmm7, 0F0h
	mulps xmm7, xmm0
	shufps xmm6, xmm6, 0A5h
	movaps xmm1, xmm0
	shufps xmm1, xmm1, 4Eh
	mulps xmm1, xmm6
	addps xmm7, xmm1
	movlps xmm6, qword ptr [edx+8]
	movhps xmm6, qword ptr [edx+32]
	movaps xmm1, xmm6
	shufps xmm1, xmm1, 0F0h
	shufps xmm6, xmm6, 0A5h
	mulps xmm1, xmm2
	mulps xmm6, xmm3
	addps xmm7, xmm1
	addps xmm7, xmm6
	movhps xmm6, qword ptr [edx+40]
	movlps xmm6, qword ptr [edx+16]
	movaps xmm1, xmm6
	shufps xmm1, xmm1, 0F0h
	shufps xmm6, xmm6, 0A5h
	mulps xmm1, xmm4
	mulps xmm6, xmm5
	addps xmm7, xmm1
	addps xmm7, xmm6
	movlps qword ptr [eax+16], xmm7
	movhps qword ptr [eax+40], xmm7
	movlps xmm6, qword ptr [edx+48]
	movhps xmm6, qword ptr [edx+72]
	movaps xmm7, xmm6
	shufps xmm7, xmm7, 0F0h
	mulps xmm7, xmm0
	shufps xmm6, xmm6, 0A5h
	movaps xmm1, xmm0
	shufps xmm1, xmm1, 4Eh
	mulps xmm1, xmm6
	addps xmm7, xmm1
	movhps xmm6, qword ptr [edx+80]
	movlps xmm6, qword ptr [edx+56]
	movaps xmm1, xmm6
	shufps xmm1, xmm1, 0F0h
	shufps xmm6, xmm6, 0A5h
	mulps xmm1, xmm2
	mulps xmm6, xmm3
	addps xmm7, xmm1
	addps xmm7, xmm6
	movlps xmm6, qword ptr [edx+64]
	movhps xmm6, qword ptr [edx+88]
	movaps xmm1, xmm6
	shufps xmm1, xmm1, 0F0h
	shufps xmm6, xmm6, 0A5h
	mulps xmm1, xmm4
	mulps xmm6, xmm5
	addps xmm7, xmm1
	addps xmm7, xmm6
	movlps qword ptr [eax+64], xmm7
	movhps qword ptr [eax+88], xmm7
	movlps xmm6, qword ptr [edx+96]
	movhps xmm6, qword ptr [edx+120]
	movaps xmm7, xmm6
	shufps xmm7, xmm7, 0F0h
	mulps xmm7, xmm0
	shufps xmm6, xmm6, 0A5h
	movaps xmm1, xmm0
	shufps xmm1, xmm1, 4Eh
	mulps xmm1, xmm6
	addps xmm7, xmm1
	movlps xmm6, qword ptr [edx+104]
	movhps xmm6, qword ptr [edx+128]
	movaps xmm1, xmm6
	shufps xmm1, xmm1, 0F0h
	shufps xmm6, xmm6, 0A5h
	mulps xmm1, xmm2
	mulps xmm6, xmm3
	addps xmm7, xmm1
	addps xmm7, xmm6
	movlps xmm6, qword ptr [edx+112]
	movhps xmm6, qword ptr [edx+136]
	movaps xmm1, xmm6
	shufps xmm1, xmm1, 0F0h
	shufps xmm6, xmm6, 0A5h
	mulps xmm1, xmm4
	mulps xmm6, xmm5
	addps xmm7, xmm1
	addps xmm7, xmm6
	movlps qword ptr [eax+112], xmm7
	movhps qword ptr [eax+136], xmm7
	ret }
}
#endif

void ForceCompilerToGenerateCodeForMatrixTemplates()
{
	matrixf mtxf; float f,*pf; int *pi;
	mtxf.jacobi_transformation(mtxf,0);
	mtxf.conjugate_gradient(0,0);
	mtxf.biconjugate_gradient(0,0);
	mtxf.minimum_residual(0,0);
	mtxf.LPsimplex(0,0,f);
	mtxf.LUdecomposition(pf,pi);
	mtxf.solveAx_b(pf,pf);
	mtxf.determinant();
	mtxf.invert();

	matrix mtxr; real r,*pr;
	mtxr.jacobi_transformation(mtxr,0);
	mtxr.conjugate_gradient(0,0);
	mtxr.biconjugate_gradient(0,0);
	mtxr.minimum_residual(0,0);
	mtxr.LPsimplex(0,0,r);
	mtxr.LUdecomposition(pr,pi);
	mtxr.solveAx_b(pr,pr);
	mtxr.determinant();
	mtxr.invert();
}
