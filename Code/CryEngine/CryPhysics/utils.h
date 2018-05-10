// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef utils_h
#define utils_h

#include <CryMath/Cry_Math.h>
#include <CryMemory/MemoryAccess.h>

using namespace primitives;

void ExtrudeBox(const primitives::box *pbox, const Vec3& dir,float step, primitives::box *pextbox);

#define USE_MATRIX_POOLS

//#define si (3)
//#define sj (1)

ILINE int max_fast(int op1,int op2) { return op1 - (op1-op2 & (op1-op2)>>31); }
ILINE int min_fast(int op1,int op2) { return op2 + (op1-op2 & (op1-op2)>>31); }

#ifdef max
 #undef max
#endif
#ifdef min
 #undef min
#endif
#if !defined(__GNUC__)
	ILINE double min(double op1,double op2) { return (op1+op2-fabs(op1-op2))*0.5; }
	ILINE double max(double op1,double op2) { return (op1+op2+fabs(op1-op2))*0.5; }
	ILINE float min(float op1,float op2) { return (op1+op2-fabsf(op1-op2))*0.5f; }
	ILINE float max(float op1,float op2) { return (op1+op2+fabsf(op1-op2))*0.5f; }
	ILINE int max(int op1,int op2) { return op1 - (op1-op2 & (op1-op2)>>31); }
	ILINE int min(int op1,int op2) { return op2 + (op1-op2 & (op1-op2)>>31); }
#endif 
inline double minmax(double op1,double op2,int bMax) { return (op1+op2+fabs(op1-op2)*(bMax*2-1))*0.5; }
inline float minmax(float op1,float op2,int bMax) { return (op1+op2+fabsf(op1-op2)*(bMax*2-1))*0.5f; }
inline double minmax(double op1,double op2,double dMax) { return (op1+op2+fabs(op1-op2)*(dMax*2.-1.))*0.5; }
inline float minmax(float op1,float op2,float fMax) { return (op1+op2+fabsf(op1-op2)*(fMax*2.f-1.f))*0.5f; }
inline int minmax(int op1,int op2,int bMax) {	return (op1&-bMax | op2&~-bMax) + ((op1-op2 & (op1-op2)>>31)^-bMax)+bMax; }
template<class F> inline F max_safe(F op1,F op2) { return op1>op2 ? op1:op2; }//int mask=isneg(op2-op1); return op1*mask+op2*(mask^1); }
template<class F> inline F min_safe(F op1,F op2) { return op1<op2 ? op1:op2; }//{ int mask=isneg(op1-op2); return op1*mask+op2*(mask^1); }

template<class F> ILINE Vec3_tpl<F> min_safe(const Vec3_tpl<F> &v0,const Vec3_tpl<F> &v1) { 
	return Vec3_tpl<F>(min_safe(v0.x,v1.x), min_safe(v0.y,v1.y), min_safe(v0.z,v1.z)); 
}
template<class F> ILINE Vec3_tpl<F> max_safe(const Vec3_tpl<F> &v0,const Vec3_tpl<F> &v1) { 
	return Vec3_tpl<F>(max_safe(v0.x,v1.x), max_safe(v0.y,v1.y), max_safe(v0.z,v1.z)); 
}

template<class F> ILINE Vec2_tpl<F> min_safe(const Vec2_tpl<F> &v0,const Vec2_tpl<F> &v1) { 
	return Vec2_tpl<F>(min_safe(v0.x,v1.x), min_safe(v0.y,v1.y));
}
template<class F> ILINE Vec2_tpl<F> max_safe(const Vec2_tpl<F> &v0,const Vec2_tpl<F> &v1) { 
	return Vec2_tpl<F>(max_safe(v0.x,v1.x), max_safe(v0.y,v1.y));
}

template<class F> ILINE F len(const Vec2_tpl<F> &v) { return sqrt_tpl(v.x*v.x+v.y*v.y); }
template<class F> ILINE F len2(const Vec2_tpl<F> &v) { return v.x*v.x+v.y*v.y; }
template<class F> ILINE Vec2_tpl<F> norm(const Vec2_tpl<F> &v) { 
	F rlen = v.x*v.x+v.y*v.y;
	if (rlen>0) {
		rlen = (F)1/sqrt_tpl(rlen);	return Vec2_tpl<F>(v.x*rlen, v.y*rlen);
	}
	return Vec2_tpl<F>(1,0);
}

struct ort_gen {
	ort_gen() {}
	Vec3 operator[](int i) { return Vec3(iszero(i),i&1,i>>1); }
};
#define ort ort_gen()
#define ortx (Vec3(1,0,0))
#define orty (Vec3(0,1,0))
#define ortz (Vec3(0,0,1))

inline float cubert_approx(float x) 
{
	union { int ix; float fx; };
	fx=x; ix = 0x2a51067f + ix/3;
	return fx;
}

template<class F> bool box_segment_intersect(const Vec2_tpl<F> box[2], const Vec2_tpl<F>& p0, const Vec2_tpl<F>& p1 )
{
	Vec2_tpl<F> d = p1-p0;

	// cross x sides
	if (d.x != F(0)) {
		for (int i = 0; i < 2; i++) {
			F t = (box[i].x - p0.x) / d.x;
			if (t > F(0) && t < F(1)) {
				F y = p0.y + d.y * t;
				if (y > box[0].y && y < box[1].y)
					return true;
			}
		}
	}
	// cross y sides
	if (d.y != F(0)) {
		for (int i = 0; i < 2; i++) {
			F t = (box[i].y - p0.y) / d.y;
			if (t > F(0) && t < F(1)) {
				F x = p0.x + d.x * t;
				if (x > box[0].x && x < box[1].x)
					return true;
			}
		}
	}
	return false;
}

struct Vec3mem : Vec3 {
	Vec3 operator-(const Vec3& op) const { return sub(op); }
	float operator*(const Vec3 &op) const { return dot(op); }
	Vec3 operator^(const Vec3& op) const { return cross(op); }
};

#include "matrixnm.h"
#include "vectorn.h"
#include "quotient.h"
#include <CryPhysics/polynomial.h>
#include <CryPhysics/primitives.h>

union floatint {
	float fval;
	int ival;
};

#if CRY_PLATFORM_SSE2
ILINE int float2int(float x)
{
#ifdef _DEBUG
	assert((_mm_getcsr()>>13&3) == 0); // check that bits 13+14 are zero (RoundToNearest)
#endif
	return _mm_cvtss_si32(_mm_load_ss(&x));
}
#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
const float fmag = (1.5f*(1<<23));
const int imag = -((23+127)<<23 | 1<<22);
// had to rewrite in assembly, since it's impossible to tell the compiler that in this case 
// addition is not associative, i.e. (x+0.5)+fmag!=x+(fmag+0.5)
__declspec(naked) ILINE int float2int(float x)
{ __asm {
	fld [esp+4]
	fadd fmag
	mov eax, imag
	fstp [esp-4]
	add eax, [esp-4]
	ret
}}
#else 
ILINE int float2int(float x)
{
	// To expensive to check and set every call and too platform specific to replicate in asm
	// Every previous attempt to replicate above asm code in regular c failed miserably. 
	return (int)(x<0.f?x-0.5f:x+0.5f);
}
#endif 

template<class F> 
ILINE F _condmax(F, int masknot) {
	return 1<<(sizeof(F)*8-2) & ~masknot;
}
ILINE float _condmax(float, int masknot) {
	return 1E30f*(masknot+1);
}

ILINE int negmask(int32 x)
{
	return (int32)(x) >> (sizeof(x)*8-1);
}

ILINE int64 negmask(int64 x)
{
	return (int64)(x) >> (sizeof(x)*8-1);
}

ILINE int32 notzero(int32 x)
{
	return ((uint32)x>>31)|((uint32)(-x)>>31);
}

ILINE int64 notzero(int64 x)
{
	return ((uint64)x>>63)|((uint64)(-x)>>63);
}

ILINE int apply_min(int x, int lower, int& mask)
{
	mask = (x-lower)>>31;
	return (x&~mask)|(lower&mask); // return x < lower ? lower : x
}

ILINE int apply_max(int x, int upper, int& mask)
{
	mask = (upper-x)>>31;
	return (x&~mask)|(upper&mask); // return x > upper ? upper : x
}

ILINE int apply_clamp(int x, int lower, int upper, int& mask1, int& mask2)
{
	return apply_max(apply_min(x, lower, mask1), upper, mask2);
}


// Import std swap.
using std::swap;
//template<class F> ILINE void swap(F &v0,F &v1) { F vt=v0; v0=v1; v1=vt; }

template<class F> 
ILINE int unite_lists(F *pSrc0,int nSrc0, F *pSrc1,int nSrc1, F *pDst,int szdst)	//AMD Port
{
	int i0,i1,n;
	INT_PTR inrange0=-nSrc0>>31,inrange1=-nSrc1>>31; //AMD Port
	F a0,a1,ares,dummy=0;
	INT_PTR pDummy( (INT_PTR) &dummy );
	pSrc0 = (F*)((INT_PTR)pSrc0&inrange0 | pDummy&~inrange0); // make pSrc point to valid data even if nSrc is zero	//AMD Port
	pSrc1 = (F*)((INT_PTR)pSrc1&inrange1 | pDummy&~inrange1);	//AMD Port
	for(n=i0=i1=0; (inrange0 | inrange1) & n-szdst>>31; inrange0=(i0+=isneg(a0-ares-1))-nSrc0>>31, inrange1=(i1+=isneg(a1-ares-1))-nSrc1>>31)
	{
		a0 = pSrc0[i0&inrange0] + _condmax(pSrc0[0],inrange0); //(1<<(sizeof(index)*8-2)&~inrange0);
		a1 = pSrc1[i1&inrange1] + _condmax(pSrc1[0],inrange1); //(1<<(sizeof(index)*8-2)&~inrange1);
		pDst[n++] = ares = min(a0,a1);
	}
	return n;
}

template<class F> 
ILINE int intersect_lists(F *pSrc0,int nSrc0, F *pSrc1,int nSrc1, F *pDst)
{
	int i0,i1,n;
	for(i0=i1=n=0; ((i0-nSrc0)&(i1-nSrc1))<0; ) {
		const F a = pSrc0[i0];
		const F b = pSrc1[i1];
		F equal = iszero(a-b);
		pDst[n] = a;
		n += equal;
		i0 += isneg(a-b) + equal;
		i1 += isneg(b-a) + equal;
	}
	return n;
}

typedef void* (*qhullmalloc)(size_t);
int qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris, qhullmalloc qmalloc = 0);

int qhull2d(ptitem2d *pts,int nVtx, edgeitem *edges, int nMaxEdges=0);

real ComputeMassProperties(strided_pointer<const Vec3> points, const index_t *faces, int nfaces, Vec3r &center,Matrix33r &I);

template<class ftype> 
ILINE Matrix33_tpl<ftype>& OffsetInertiaTensor(Matrix33_tpl<ftype> &I,const Vec3_tpl<ftype> &center,ftype M)
{
	I(0,0) += M*(center.y*center.y + center.z*center.z);
	I(1,1) += M*(center.x*center.x + center.z*center.z);
	I(2,2) += M*(center.x*center.x + center.y*center.y);
	I(1,0) = I(0,1) = I(0,1)-M*center.x*center.y;
	I(2,0) = I(0,2) = I(0,2)-M*center.x*center.z;
	I(2,1) = I(1,2) = I(1,2)-M*center.y*center.z;
	return I;
}

enum booltype { bool_intersect=1 };

int boolean2d(booltype type, Vec2 *ptbuf1,int npt1, Vec2 *ptbuf2,int npt2,int bClosed, Vec2 *&ptbuf,int *&pidbuf);

real RotatePointToPlane(const Vec3r &pt, const Vec3r &axis,const Vec3r &center, const Vec3r &n,const Vec3r &origin);

template<class ftype,int si,int sj> 
ILINE Matrix33_tpl<ftype> GetMtxStrided(const ftype *pdata) {
	Matrix33_tpl<ftype> res;
	res(0,0)=pdata[0*si+0*sj]; res(0,1)=pdata[0*si+1*sj]; res(0,2)=pdata[0*si+2*sj];
	res(1,0)=pdata[1*si+0*sj]; res(1,1)=pdata[1*si+1*sj]; res(1,2)=pdata[1*si+2*sj];
	res(2,0)=pdata[2*si+0*sj]; res(2,1)=pdata[2*si+1*sj]; res(2,2)=pdata[2*si+2*sj];
	return res;
}
template<class ftype,int si,int sj> 
ILINE void SetMtxStrided(const Matrix33_tpl<ftype> &mtx, ftype *pdata) {
	pdata[0*si+0*sj]=mtx(0,0); pdata[0*si+1*sj]=mtx(0,1); pdata[0*si+2*sj]=mtx(0,2);
	pdata[1*si+0*sj]=mtx(1,0); pdata[1*si+1*sj]=mtx(1,1); pdata[1*si+2*sj]=mtx(1,2);
	pdata[2*si+0*sj]=mtx(2,0); pdata[2*si+1*sj]=mtx(2,1); pdata[2*si+2*sj]=mtx(2,2);
}
template<class ftype> 
ILINE Matrix33_tpl<ftype> GetMtxFromBasis(const Vec3_tpl<ftype> *pBasis) { 
	Matrix33_tpl<ftype> res;
	res(0,0)=pBasis[0].x; res(0,1)=pBasis[0].y; res(0,2)=pBasis[0].z;
	res(1,0)=pBasis[1].x; res(1,1)=pBasis[1].y; res(1,2)=pBasis[1].z;
	res(2,0)=pBasis[2].x; res(2,1)=pBasis[2].y; res(2,2)=pBasis[2].z;
	return res;
}
template<class ftype> 
ILINE Matrix33_tpl<ftype> GetMtxFromBasisT(const Vec3_tpl<ftype> *pBasis) { 
	Matrix33_tpl<ftype> res;
	res(0,0)=pBasis[0].x; res(1,0)=pBasis[0].y; res(2,0)=pBasis[0].z;
	res(0,1)=pBasis[1].x; res(1,1)=pBasis[1].y; res(2,1)=pBasis[1].z;
	res(0,2)=pBasis[2].x; res(1,2)=pBasis[2].y; res(2,2)=pBasis[2].z;
	return res;
}
template<class ftype,class ftype1> 
ILINE void SetBasisFromMtx(Vec3_tpl<ftype1> *pBasis, const Matrix33_tpl<ftype> &mtx) { 
	pBasis[0].x=mtx(0,0); pBasis[0].y=mtx(0,1); pBasis[0].z=mtx(0,2);
	pBasis[1].x=mtx(1,0); pBasis[1].y=mtx(1,1); pBasis[1].z=mtx(1,2);
	pBasis[2].x=mtx(2,0); pBasis[2].y=mtx(2,1); pBasis[2].z=mtx(2,2);
}
template<class ftype,class ftype1> 
ILINE void SetBasisTFromMtx(Vec3_tpl<ftype1> *pBasis, const Matrix33_tpl<ftype> &mtx) { 
	pBasis[0].x=mtx(0,0); pBasis[0].y=mtx(1,0); pBasis[0].z=mtx(2,0);
	pBasis[1].x=mtx(0,1); pBasis[1].y=mtx(1,1); pBasis[1].z=mtx(2,1);
	pBasis[2].x=mtx(0,2); pBasis[2].y=mtx(1,2); pBasis[2].z=mtx(2,2);
}

template<class ftype> 
ILINE void TransposeBasis(Vec3_tpl<ftype> *axes) {
	ftype t;
	t=axes[0].y; axes[0].y=axes[1].x; axes[1].x=t;
	t=axes[0].z; axes[0].z=axes[2].x; axes[2].x=t;
	t=axes[1].z; axes[1].z=axes[2].y; axes[2].y=t;
}
template<class ftype> 
ILINE void IdentityBasis(Vec3_tpl<ftype> *axes) {
	axes[0].Set((ftype)1,(ftype)0,(ftype)0);
	axes[1].Set((ftype)0,(ftype)1,(ftype)0);
	axes[2].Set((ftype)0,(ftype)0,(ftype)1);
}

ILINE void LeftOffsetSpatialMatrix(matrixf &mtx, const Vec3 &offset){
	Matrix33 A=GetMtxStrided<float,6,1>(mtx.data); 
	Matrix33 B=GetMtxStrided<float,6,1>(mtx.data+3);
	Matrix33 C=GetMtxStrided<float,6,1>(mtx.data+18); 
	Matrix33 D=GetMtxStrided<float,6,1>(mtx.data+21); 
	Matrix33 rmtx;

	crossproduct_matrix(offset,rmtx);

	SetMtxStrided<float,6,1>(C+rmtx*A, mtx.data+18);
	SetMtxStrided<float,6,1>(D+rmtx*B, mtx.data+21);
}

ILINE void RightOffsetSpatialMatrix(matrixf &mtx, const Vec3 &offset){
	Matrix33 A=GetMtxStrided<float,6,1>(mtx.data), B=GetMtxStrided<float,6,1>(mtx.data+3),
						 C=GetMtxStrided<float,6,1>(mtx.data+18), D=GetMtxStrided<float,6,1>(mtx.data+21), rmtx;
	crossproduct_matrix(offset,rmtx);
	SetMtxStrided<float,6,1>(A+B*rmtx, mtx.data);
	SetMtxStrided<float,6,1>(C+D*rmtx, mtx.data+18);
}

template<class itype>
void ComputeMeshEigenBasis(strided_pointer<const Vec3> pVertices,strided_pointer<itype> pIndices,int nTris, Vec3r *eigen_axes,Vec3r &center)
{
	int i,j,k;
	Vec3r m,mean(ZERO),v[3];
	real s,t,sum=0, mtxbuf[9];
	matrix C(3,3, mtx_symmetric, mtxbuf); C.zero();

	// find mean point
	for(i=0; i<nTris*3; i+=3) {
		v[0] = pVertices[pIndices[i+0]];
		v[1] = pVertices[pIndices[i+1]];
		v[2] = pVertices[pIndices[i+2]];
		s = (v[1]-v[0] ^ v[2]-v[0]).len()*(real)0.5;
		mean += (v[0]+v[1]+v[2])*real(1.0/3)*s;
		sum += s;
	}
	if (sum==0) {
		center = nTris>0 ? pVertices[pIndices[0]] : Vec3(ZERO);
		IdentityBasis(eigen_axes);
		return;
	}
	mean /= sum;

	// calculate covariance matrix
	for(i=0; i<nTris*3; i+=3) {
		v[0].x = pVertices[pIndices[i+0]].x-mean.x; v[0].y = pVertices[pIndices[i+0]].y-mean.y; v[0].z = pVertices[pIndices[i+0]].z-mean.z;
    v[1].x = pVertices[pIndices[i+1]].x-mean.x; v[1].y = pVertices[pIndices[i+1]].y-mean.y; v[1].z = pVertices[pIndices[i+1]].z-mean.z;
    v[2].x = pVertices[pIndices[i+2]].x-mean.x; v[2].y = pVertices[pIndices[i+2]].y-mean.y; v[2].z = pVertices[pIndices[i+2]].z-mean.z;
		s = (v[1]-v[0] ^ v[2]-v[0]).len()*(real)0.5;
		m = v[0]+v[1]+v[2]; 
		for(j=0;j<3;j++) for(k=j;k<3;k++) {
			t = s*real(1.0/12)*(m[j]*m[k] + v[0][j]*v[0][k] + v[1][j]*v[1][k] + v[2][j]*v[2][k]);
			C[j][k] += t; if (k!=j) C[k][j] += t;
		}
	}

	// find eigenvectors of covariance matrix (normalized)
	real eval[3];
	matrix eigenBasis(3,3,0,eigen_axes[0]);
	C.jacobi_transformation(eigenBasis,eval,0);
	center = mean;
}

ILINE void SpatialTranspose(matrixf &src, matrixf& dst) {
	int i,j;
	dst.nRows = src.nCols; dst.nCols = src.nRows;
	for(i=0;i<src.nRows;i++) for(j=0;j<3;j++) {
		dst[j][i] = src[i][j+3];
		dst[j+3][i] = src[i][j];
	}
}

template<class ftype> 
ILINE Vec3_tpl<ftype> cross_with_ort(const Vec3_tpl<ftype> &vec, int iz) {
	Vec3_tpl<ftype> res(ZERO);
	int ix=inc_mod3[iz], iy=dec_mod3[iz];
	res[iz]=0; res[ix]=vec[iy]; res[iy]=-vec[ix];
	return res;
}

template<class dtype> dtype* ReallocateList(dtype *&plist, int szold,int sznew, bool bZero=false, bool bDeleteOld=true)
{
	dtype *newlist = new dtype[max(1,sznew)];
	if (bZero)
		memset(newlist+szold,0,sizeof(dtype)*max(0,sznew-szold));
	memcpy(newlist,plist,min(szold,sznew)*sizeof(dtype));
	if (bDeleteOld)
		delete[] plist;
	plist = newlist;
	return plist;
}

Vec3 get_xqs_from_matrices(Matrix34 *pMtx3x4,Matrix33 *pMtx3x3, Vec3 &pos,quaternionf &q,float &scale, struct phys_geometry **ppgeom=0,struct IGeomManager *pGeoman=0);
const int DATA_UNSCALED_GEOM = -999;
int BakeScaleIntoGeometry(struct phys_geometry *&pgeom,struct IGeomManager *pGeoman, const Vec3& s, int bReleaseOld=0);

//int BreakPolygon(Vec2 *ptSrc,int nPt, int nCellx,int nCelly, int maxPatchTris, Vec2 *&ptout,int *&nPtOut, 
//								 float jointhresh=0.5f,int seed=-1);

struct SOcclusionCubeMap
{
	enum { bMode = 0 };     // 0 - use original code, 1 - new code

	int N;                  // 6, N x N occlusion maps
	float rmax;             // maximum explosion distance
	float rmin;             // min explosion distance
	float halfN;            // halfN = 0.5f*fN
	float zscale;           // fixed point scale factor, 65535.f/rmax
	int irmin;              // cached value
	int * grid[6];          // the rawdata - address of grid[0] is 16 byte aligned
	int * scratch[2];       // two scratch arrays each NxN in size
	int * allocation;       // store the unaligned allocation pointer
	int usedPlanes;         // store which planes were touched by the rasteriser - speeds up the comparisons
	
	SOcclusionCubeMap();
	~SOcclusionCubeMap();
	void Free();
	void Init(int __N, float __rmin, float __rmax);
	void Reset();
	void Allocate(int N);
	void ProcessRmin();
	void GetMemoryStatistics(ICrySizer *pSizer);
				
	ILINE float ConvertToDistance(int rawdata) { // Used for debug code
		floatint tmp; tmp.ival = rawdata;
		if (bMode)
			rawdata = (int)(clamp_tpl(1.f/max(1e-6f,tmp.fval), 0.f, 65535.f));
		return (float)rawdata*rmax*(1.0/65535.0f);
	}

#if defined(_RELEASE) || defined(STANDALONE_PHYSICS)
	void DebugDrawToScreen(float xpos, float ypos, float width) {}
	void FreeTextures() {}
#else
	void DebugDrawToScreen(float xpos, float ypos, float width);
	void FreeTextures();
	int debugTextureId[6];
#endif
};

void RasterizePolygonIntoCubemap(const Vec3 *pt,int npt, int iPass, SOcclusionCubeMap* cubeMap);



void GrowAndCompareCubemaps(SOcclusionCubeMap* cubemapOcc, SOcclusionCubeMap* cubemap, int nGrow, int &nCells,int &nOccludedCells);

void CalcMediumResistance(const Vec3 *ptsrc,int npt, const Vec3& n, const plane &waterPlane,
													const Vec3 &vworld,const Vec3 &wworld,const Vec3 &com, Vec3 &P,Vec3 &L);

int CoverPolygonWithCircles(strided_pointer<Vec2> pt,int npt,bool bConsecutive, const Vec2 &center, Vec2 *&centers,
														float *&radii, float minCircleRadius);

int ChoosePrimitiveForMesh(strided_pointer<const Vec3> pVertices,strided_pointer<const unsigned short> pIndices,int nTris, const Vec3r *eigen_axes,
													 const Vec3r &center, int flags, float tolerance, primitive *&pprim);
void ExtrudeBox(const box *pbox, const Vec3& dir,float step, box *pextbox);

int TriangulatePoly(Vec2 *pVtx, int nVtx, int *pTris, int szTriBuf);

template<class CellChecker> 
int DrawRayOnGrid(primitives::grid *pgrid, Vec3 &origin,Vec3 &dir, CellChecker &cell_checker) 
{
	int i,ishort,ilong,bStep,bPrevStep,ilastcell;
	float dshort,dlong,frac;
	quotientf tx[2],ty[2],t[2]={ quotientf(0,1), quotientf(1,1) };
	Vec2i istep,icell,idirsgn;

	// crop ray with grid bounds
	idirsgn.set(sgnnz(dir.x),sgnnz(dir.y));
	if (!pgrid->iscyclic()) {
	i = idirsgn.x;
	tx[1-i>>1].set(-origin.x*i, dir.x*i);
	tx[1+i>>1].set((pgrid->size.x*pgrid->step.x-origin.x)*i, dir.x*i);
	i = idirsgn.y;
	ty[1-i>>1].set(-origin.y*i, dir.y*i);
	ty[1+i>>1].set((pgrid->size.y*pgrid->step.y-origin.y)*i, dir.y*i);
	t[0] = max(t[0].set(0,1),max(tx[0],ty[0]));
	t[1] = min(t[1].set(1,1),min(tx[1],ty[1]));
	IF (t[0]>=t[1], 0)
		return 0;
	IF (t[0]>0, 0)
		origin += dir*t[0].val();
	IF (t[0]>0 || t[1]<1, 0)
		dir *= (t[1]-t[0]).val();
	}

	ilong = isneg(fabs_tpl(dir.x)*pgrid->stepr.x-fabs_tpl(dir.y)*pgrid->stepr.y);
	ishort = ilong^1;
	dshort = fabs_tpl(dir[ishort])*pgrid->stepr[ishort]; 
	dlong  = fabs_tpl(dir[ilong])*pgrid->stepr[ilong];
	istep[ilong] = idirsgn[ilong];
	istep[ishort] = idirsgn[ishort];

	icell.set(float2int(origin.x*pgrid->stepr.x-0.5f), float2int(origin.y*pgrid->stepr.y-0.5f));
	icell = pgrid->cropxy(icell,0);
	ilastcell = pgrid->crop(float2int((origin.y+dir.y)*pgrid->stepr.y-0.5f),1,0)<<16 | 
							pgrid->crop(float2int((origin.x+dir.x)*pgrid->stepr.x-0.5f),0,0)&0xFFFF;
	IF (cell_checker.check_cell(icell,ilastcell) || (icell.y<<16|icell.x)==ilastcell, 0)
		return 1;
	IF (fabs_tpl(dir[ilong])*pgrid->stepr[ilong]<0.0001f, 0)
		return 0;

	t[0].set((icell[ilong]+(istep[ilong]+1>>1))*pgrid->step[ilong]-origin[ilong], dir[ilong]);
	frac = (origin[ishort]+dir[ishort]*t[0].val())*pgrid->stepr[ishort] - icell[ishort];
	frac = (1-idirsgn[ishort]>>1)+frac*idirsgn[ishort];
	IF (frac>1.0f, 0) {
		icell[ishort] += istep[ishort];
		IF (cell_checker.check_cell(icell,ilastcell) || (icell.y<<16|icell.x)==ilastcell, 0)
			return 1;
		frac -= 1;
	}
	frac *= dlong;
	icell[ilong] += istep[ilong];

	bPrevStep = 0;
	do {
		IF (cell_checker.check_cell(icell,ilastcell) || (icell.y<<16|icell.x)==ilastcell, 0)
			return 1;
		frac += dshort*(bPrevStep^1); bStep = isneg(dlong-frac);
		icell[ishort] += bStep*istep[ishort]; frac -= dlong*bStep;
		icell[ilong] += istep[ilong]&~-bStep; 
		bPrevStep = bStep;
	} while(true);

	return 0;
}

inline Vec2i idx2offs(int i) { return Vec2i(1-(i&2) & -(i&1), (i&2)-1 & ~-(i&1)); }

struct jgrid_checker {
	grid *pgrid;
	char *pCellMask;
	Vec2 org,dir,dirn,*ppt,*pnorms;
	int iedge[2],iedgeExit0;
	int iprevcell;
	int bMarkCenters;

	int check_cell(const Vec2i &icell, int &ilastcell) {
		int i,idx,icurcell,mask=2,bhit;
		Vec2 c,step=pgrid->step,pt,pt0(pgrid->origin.x,pgrid->origin.y);

		idx = icell*pgrid->stride;
		pCellMask[idx] |= 2;	// mark the cell as used
		c.set(step.x*(icell.x+0.5f), step.y*(icell.y+0.5f));
		icurcell = icell.y<<16|icell.x;
		pCellMask[idx] |= bMarkCenters & isneg((c-org)*dir); // set a bit if the cell's center is outside the current edge
		if (pnorms)
			pnorms[idx] += dirn.rot90cw();

		if ((icurcell|iprevcell>>31)!=iprevcell) { // false if this is the first edge cell
			if (iedge[0]!=(iedge[1]|iedge[0]>>31)) for(i=iedge[1]+1&3; i!=iedge[0]; i=i+1&3)
				mask |= 4<<i;
			pCellMask[Vec2i(iprevcell&0xFFFF,iprevcell>>16)*pgrid->stride] &= mask;
			for(i=0;i<2;i++) { // compute the enter point
				bhit = -inrange((dirn^org-c)*(1-i*2)-fabs_tpl(dirn[i^1])*step[i], -dirn[i]*step[i^1]*1.001f,dirn[i]*step[i^1]*1.001f);
				(iedge[0] &= ~bhit) |= (i^1 | (i^isnonneg(dirn[i]))<<1) & bhit;
			}
			if (ppt && icurcell!=ilastcell) {
				for(pt=org+dirn*(dirn*(c-org)),i=0; (fabs_tpl(pt.x-c.x)>step.x*0.5f || fabs_tpl(pt.y-c.y)>step.y*0.5f) && i<4; i++)
					pt = org+dirn*(dirn*(c+Vec2(step.x*(0.5f-(i&1)),0)+Vec2(0,step.y*(0.5f-(i>>1)))-org));
				ppt[idx] = pt+pt0;
			}
		}	else if (ppt)
			ppt[idx] = org+pt0;
		if (icurcell!=ilastcell)	{	// false if this is the last edge cell
			for(i=0;i<2;i++) { // compute the leave point
				bhit = -inrange((dirn^org-c)*(1-i*2)+fabs_tpl(dirn[i^1])*step[i], -dirn[i]*step[i^1]*1.001f,dirn[i]*step[i^1]*1.001f);
				(iedge[1] &= ~bhit) |= (i^1 | (i^isneg(dirn[i]))<<1) & bhit;
			}
			iedgeExit0 += iedge[1]+1 & iedgeExit0>>31;
		}	else if (ppt)
			ppt[idx] = org+dir+pt0;

		iprevcell = icurcell;
		return 0;
	}

	int *pqueue;
	int ihead,itail,nQueuedCells;
	int szQueue;

	void MarkCellInterior(int i) {
		int j,icell; pCellMask[i] |= 2;
		for(j=0;j<4;j++) if (!(pCellMask[icell = i+idx2offs(j)*pgrid->stride] & 2))
			MarkCellInterior(icell);
	}

	void MarkCellInteriorQueue(int icell)
	{
		int j,icellNext;
		pCellMask[pqueue[ihead=itail=0] = icell] |= 2;
		for(nQueuedCells=1; nQueuedCells>0; ) {
			icell = pqueue[itail]; itail = itail+1 & itail+1-szQueue>>31;	nQueuedCells--;
			for(j=0;j<4;j++) if (!(pCellMask[icellNext = icell+idx2offs(j)*pgrid->stride] & 2)) {
				pCellMask[icellNext] |= 2; 
				if (nQueuedCells==szQueue) {
					ReallocateList(pqueue,szQueue,szQueue+64);
					memmove(pqueue+itail+64, pqueue+itail, (szQueue-ihead-1)*sizeof(pqueue[0]));
					if (itail>0) itail+=64;	szQueue+=64;
				}
				ihead = ihead+1 & ihead+1-szQueue>>31; nQueuedCells++;
				pqueue[ihead] = icellNext;
			}
		}
	}
};


int bin2ascii(const unsigned char *pin,int sz, unsigned char *pout);
int ascii2bin(const unsigned char *pin,int sz, unsigned char *pout);

template<class T> ILINE T *_align16(T *ptr) { return (T*)(((INT_PTR)ptr-1&~15)+16); }

void WritePacked(CStream &stm, int num);
void WritePacked(CStream &stm, uint64 num);
void ReadPacked(CStream &stm,int &num);
void ReadPacked(CStream &stm,uint64 &num);
void WriteCompressedPos(CStream &stm, const Vec3 &pos, bool bCompress=true);
void ReadCompressedPos(CStream &stm, Vec3 &pos, bool &bWasCompressed);
Vec3 CompressPos(const Vec3 &pos);
const int CMP_QUAT_SZ = 49;
void WriteCompressedQuat(CStream &stm, const quaternionf &q);
void ReadCompressedQuat(CStream &stm,quaternionf &q);
quaternionf CompressQuat(const quaternionf &q);
const int CMP_VEL_SZ = 48;
ILINE void WriteCompressedVel(CStream &stm, const Vec3 &vel, const float maxvel) {
	stm.Write((short)max(-32768,min(32767,float2int(vel.x*(32767.0f/maxvel)))));
	stm.Write((short)max(-32768,min(32767,float2int(vel.y*(32767.0f/maxvel)))));
	stm.Write((short)max(-32768,min(32767,float2int(vel.z*(32767.0f/maxvel)))));
}
ILINE void ReadCompressedVel(CStream &stm, Vec3 &vel, const float maxvel) {
	short i;
	stm.Read(i); vel.x = i*(maxvel/32767.0f);
	stm.Read(i); vel.y = i*(maxvel/32767.0f);
	stm.Read(i); vel.z = i*(maxvel/32767.0f);
}
ILINE Vec3 CompressVel(const Vec3 &vel, const float maxvel) {
	Vec3 res;
	res.x = max(-32768,min(32767,float2int(vel.x*(32767.0f/maxvel)))) * (maxvel/32767.0f);
	res.y = max(-32768,min(32767,float2int(vel.y*(32767.0f/maxvel)))) * (maxvel/32767.0f);
	res.z = max(-32768,min(32767,float2int(vel.z*(32767.0f/maxvel)))) * (maxvel/32767.0f);
	return res;
}
ILINE void WriteCompressedFloat(CStream &stm, float f, const float maxval,const int nBits) {
	const int imax = (1<<nBits-1)-1;
	stm.WriteNumberInBits(max(-imax-1,min(imax,float2int(f*(imax/maxval)))),nBits);
}
ILINE void ReadCompressedFloat(CStream &stm, float &f, const float maxval,const int nBits) {
	const int imax = (1<<nBits-1)-1;
	unsigned int num;
	stm.ReadNumberInBits(num,nBits);
	f = ((int)num | ((int)num<<32-nBits & 1<<31)>>31-nBits)*(maxval/imax);
}
ILINE float CompressFloat(float f,const float maxval,const int nBits) {
	const int imax = (1<<nBits-1)-1;
	return max(-imax-1,min(imax,float2int(f*(imax/maxval)))) * (maxval/imax);
}


ILINE intptr_t iszero_mask(void *ptr) { return (intptr_t)ptr>>sizeof(intptr_t)*8-1 ^ (intptr_t)ptr-1>>sizeof(intptr_t)*8-1; }
template<class ftype> ILINE bool AABB_overlap(const Vec3_tpl<ftype> *BBox0, const Vec3_tpl<ftype> *BBox1) {
	return (ftype)(0) > max(max(fabs_tpl(BBox0[0].x+BBox0[1].x-BBox1[0].x-BBox1[1].x) - (BBox0[1].x-BBox0[0].x)-(BBox1[1].x-BBox1[0].x),
											  fabs_tpl(BBox0[0].y+BBox0[1].y-BBox1[0].y-BBox1[1].y) - (BBox0[1].y-BBox0[0].y)-(BBox1[1].y-BBox1[0].y)),
											  fabs_tpl(BBox0[0].z+BBox0[1].z-BBox1[0].z-BBox1[1].z) - (BBox0[1].z-BBox0[0].z)-(BBox1[1].z-BBox1[0].z));
}
template<class ftype> ILINE bool AABB_overlap(const Vec3_tpl<ftype> &BBox0min, const Vec3_tpl<ftype> &BBox0max,
																							 const Vec3_tpl<ftype> &BBox1min, const Vec3_tpl<ftype> &BBox1max) {
	return (ftype)(0) > max(max(fabs_tpl(BBox0min.x+BBox0max.x-BBox1min.x-BBox1max.x) - (BBox0max.x-BBox0min.x)-(BBox1max.x-BBox1min.x),
											  fabs_tpl(BBox0min.y+BBox0max.y-BBox1min.y-BBox1max.y) - (BBox0max.y-BBox0min.y)-(BBox1max.y-BBox1min.y)),
											  fabs_tpl(BBox0min.z+BBox0max.z-BBox1min.z-BBox1max.z) - (BBox0max.z-BBox0min.z)-(BBox1max.z-BBox1min.z));
}
template<class ftype> ILINE bool PtInAABB(const Vec3_tpl<ftype> *BBox0, const Vec3_tpl<ftype> &pt) {
	return (ftype)(0) > max(max(fabs_tpl(BBox0[0].x+BBox0[1].x-pt.x*2) - (BBox0[1].x-BBox0[0].x),
												fabs_tpl(BBox0[0].y+BBox0[1].y-pt.y*2) - (BBox0[1].y-BBox0[0].y)),
												fabs_tpl(BBox0[0].z+BBox0[1].z-pt.z*2) - (BBox0[1].z-BBox0[0].z));
}

ILINE int check_mask(unsigned int *pMask, int i) {
	return pMask[i>>5]>>(i&31) & 1;
}
ILINE void set_mask(unsigned int *pMask, int i) {
	pMask[i>>5] |= 1u<<(i&31);
}
ILINE void clear_mask(unsigned int *pMask, int i) {
	pMask[i>>5] &= ~(1u<<(i&31));
}


struct sub_int {
	int *data;
	int idx;
	sub_int(int &_data,int _idx) : data(&_data), idx(_idx) {}
	sub_int& operator=(const sub_int &src) { return *this=(int)src; }
	operator int() const { 
		int lo = (*data<<16)>>16, hi = *data>>16;
		return lo+(hi-lo & -idx); 
	}
	sub_int& operator=(int newval) { 
		((*data) &= 0xFFFF0000 ^ -idx) |= newval + ((newval<<16)-newval & -idx);
		return *this; 
	}
};

struct two_ints_in_one {
	int data;

	int operator[](int idx) const {
		int lo = (data<<16)>>16, hi = data>>16;
		return lo+(hi-lo & -idx); 
	}
	sub_int operator[](int idx) { return sub_int(data,idx); }
};


inline int bitcount(int x) {
	x = ((x&0xAAAAAAAA)>>1 )+(x&0x55555555);
	x = ((x&0xCCCCCCCC)>>2 )+(x&0x33333333);
	x = ((x&0xF0F0F0F0)>>4 )+(x&0x0F0F0F0F);
	x = ((x&0xFF00FF00)>>8 )+(x&0x00FF00FF);
	x = ((x&0xFFFF0000)>>16)+(x&0x0000FFFF);
	return x;
}

const int SINCOSTABSZ = 1024, SINCOSTABSZ_LOG2 = 10;
extern float g_sintab[SINCOSTABSZ+1];
struct costab {
	float operator[](int idx) const { return g_sintab[SINCOSTABSZ-idx]; }
};

#ifdef __clang__
#define g_costab costab()	// WORKAROUND: For some reason gets confused and expects lambda function
#else
#define g_costab (costab())
#endif

struct subref {
	void set(int _iPtrOffs, int _iSzFixed, int _iSzOffs, int _iSzUnit, subref *_next)	{
		iPtrOffs=_iPtrOffs; iSzFixed=_iSzFixed; iSzOffs=_iSzOffs; iSzUnit=_iSzUnit; next=_next;
	}
	int iPtrOffs;
	int iSzFixed;
	int iSzOffs;
	int iSzUnit;
	subref *next;
};

// spinlocks
/*ILINE void SpinLock(volatile int *pLock,int checkVal,int setVal) { 
#if CRY_PLATFORM_X86
	__asm {
	mov edx, setVal
	mov ecx, pLock
	Spin:
		mov eax, checkVal
		lock cmpxchg [ecx], edx
	jnz Spin }
#else
	while(_InterlockedCompareExchange(pLock,setVal,checkVal)!=checkVal);
#endif
}

ILINE void AtomicAdd(volatile int *pVal, int iAdd) {
#if CRY_PLATFORM_X86
	__asm {
		mov edx, pVal
		mov eax, iAdd
		lock add [edx], eax
	}
#else
	_InterlockedExchangeAdd(pVal,iAdd);
#endif
}*/


#if defined(EXCLUDE_PHYSICS_THREAD)
	ILINE int AtomicCAS(volatile int *pLock, int setVal, int checkVal) { const int res = *(int*)pLock; if (res == checkVal) { *(int*)pLock = setVal; } return res; }
#else
	ILINE int AtomicCAS(volatile int *pLock, int setVal, int checkVal) { return CryInterlockedCompareExchange((volatile LONG*) pLock, setVal, checkVal ); }
#endif


//#define IMPATIENT_LOCKS
#ifdef IMPATIENT_LOCKS
extern int g_maxLockTicks;
#define TestbedPlaceholder foo(); int g_maxLockTicks=60000000; void foo1

extern "C" __declspec(dllimport) LONG __stdcall InterlockedCompareExchange(volatile LONG*,LONG,LONG);

struct ImpatientWriteLock {
	ImpatientWriteLock(volatile int &lock, int active=1) {
		int64 ticks = CryGetTicks();
		pLock = &lock;
		if (bActive=active) while(InterlockedCompareExchange((volatile LONG*)&lock,WRITE_LOCK_VAL,0)!=0) {
			if (g_maxLockTicks && CryGetTicks()-ticks > g_maxLockTicks) {
				lock = -10;
				*(int*)0 = 0;
			}
			CrySleep(0);
		}
	}
	~ImpatientWriteLock() { if (bActive) { while(*pLock==-10); AtomicAdd(pLock,-WRITE_LOCK_VAL); } }
	void SetActive(int active) { bActive = active; }
	volatile int *pLock;
	int bActive;
};

struct ImpatientReadLock {
	ImpatientReadLock(volatile int &lock, int active=1) {
		pLock = &lock; int64 ticks = CryGetTicks();
		if (bActive = active) {
			AtomicAdd(&lock, 1); 
			volatile char *pw=(volatile char*)&lock+1+eBigEndian; for(;*pw;) 
				if (g_maxLockTicks && CryGetTicks()-ticks > g_maxLockTicks) {
					lock = -10;
					*(int*)0 = 0;
				}
		}
	}
	~ImpatientReadLock() { if (bActive) { while(*pLock==-10); AtomicAdd(pLock,-1); } }
	void SetActive(int active) { bActive = active; }
	volatile int *pLock;
	int bActive;
};

#define WriteLock ImpatientWriteLock
#define ReadLock ImpatientReadLock
#define WriteLockCond ImpatientWriteLock
#define ReadLockCond ImpatientReadLock
#endif

/*struct ReadLock {
	volatile int *prw;
	ReadLock(volatile int &rw) { 
		AtomicAdd(prw=&rw,1); 
		volatile char *pw=(volatile char*)&rw+2; for(;*pw;); 
	}
	~ReadLock() { AtomicAdd(prw,-1); }
};

struct ReadLockCond {
	volatile int *prw;
	int bActivated;
	ReadLockCond(volatile int &rw,int bActive) { 
		if (bActive) { 
			AtomicAdd(&rw,1);	bActivated = 1;
			volatile char *pw=(volatile char*)&rw+2; for(;*pw;); 
		} else bActivated = 0;
		prw = &rw; 
	}
	~ReadLockCond() { AtomicAdd(prw,-bActivated); }
};

struct WriteLock {
	volatile int *prw;
	WriteLock(volatile int &rw) { SpinLock(&rw,0,WRITE_LOCK_VAL); prw=&rw; }
	~WriteLock() { AtomicAdd(prw,-WRITE_LOCK_VAL); }
};

struct WriteLockCond {
	volatile int *prw;
	int iActive;
	WriteLockCond(volatile int &rw,int bActive) { 
		if (bActive)
			SpinLock(&rw,0,iActive=WRITE_LOCK_VAL);
		else 
			iActive = 0;
		prw = &rw; 
	}
	~WriteLockCond() { AtomicAdd(prw,-iActive); }
};*/

template<typename T, size_t HASH_BITS = 10> class CPhysHashMap { 
  struct entry { 
    entry* next;
    uint32 key; 
    T value; 
    entry() : next() {}
  }; 
  struct entry_block { 
    char*  buffer; 
    uint32 counter; 
    entry_block* prev; 
    entry_block(char* _buffer, entry_block* _prev) : buffer(_buffer), counter(), prev(_prev) {}
  }; 
  enum { hash_bits=HASH_BITS, hash_size=(1UL<<HASH_BITS), refill_size=hash_size, hash_mask=(hash_size-1) }; 

  entry* m_buckets[hash_size]; 
  entry_block* m_entry_pool; 
  volatile int m_atomic_lock; 

  const uint32 hash(const uint32 value) const { return (value+(value>>hash_bits)+(value>>hash_bits*2))&hash_mask; } 

  entry* AllocateEntry(const uint32 key) { 
    if (m_entry_pool == NULL || m_entry_pool->counter == refill_size) { 
      const size_t entrySize=sizeof(entry)*refill_size; 
      const size_t blockSize=sizeof(entry_block); 
      char* buffer=(char*) CryModuleMemalign(128, entrySize+blockSize);
      if (buffer==NULL) return NULL; 
      memset(buffer, 0, entrySize+blockSize); 
      entry_block* block=new (buffer) entry_block(buffer+blockSize, m_entry_pool); 
      m_entry_pool=block; 
    } 
    return new (m_entry_pool->buffer + m_entry_pool->counter++ * sizeof(entry)) entry; 
  } 
  T* Retrieve(const uint32 key) {
    const uint32 idx=hash(key); 
    SpinLock(&m_atomic_lock, 0, 1); 
    entry **last=&m_buckets[idx], *iter=m_buckets[idx];
    while (iter) { if (iter->key==key) { m_atomic_lock = 0; return &iter->value;} last=&iter->next; iter=iter->next; }
    entry* entry=AllocateEntry(key); 
    entry->key=key; *last=entry; 
    m_atomic_lock = 0; 
    return &entry->value; 
  }

public: 
  CPhysHashMap() : m_entry_pool(), m_atomic_lock() { memset(m_buckets,0,sizeof(m_buckets)); }
  ~CPhysHashMap() {
    SpinLock(&m_atomic_lock, 0, 1); 
    for (entry_block* iter = m_entry_pool; iter; iter = iter->prev) 
      CryModuleMemalignFree(iter->buffer-sizeof(entry_block)); 
    m_atomic_lock = 0; 
  } 
  T* Get(const uint32 key) { return Retrieve(hash(key)); } 
  bool Contains(const uint32 key) const { return Retrieve(hash(key), key) ? true : false; }
  uint32 GetIterator(strided_pointer<T>& ptr) { 
    if (m_entry_pool) { 
      ptr=strided_pointer<T>(&reinterpret_cast<entry*>(m_entry_pool->buffer)->value,sizeof(entry)); 
      return m_entry_pool->counter; 
    } 
    return 0; 
  }
};

//////////////////////////////////////////////////////////////////////////
// Return tag name combined with number, ex: Name_1, Name_2
//////////////////////////////////////////////////////////////////////////
ILINE const char* numbered_tag(const char *s,unsigned int num)
{
	static char str[64];
	assert( strlen(s) < 20 );
	cry_sprintf(str, "%s_%u", s, num);
	return str;
}

#endif
