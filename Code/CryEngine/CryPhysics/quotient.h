// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef quotient_h
#define quotient_h
#pragma once

// warning: all comparisons assume quotent's y>=0, use fixsign() to ensure this is the case
#define sgnnz_result_type(F) int
#define isneg_result_type(F) int

template<class ftype> class quotient_tpl {
public:
	quotient_tpl() {}

	quotient_tpl(type_min) { x=-1;y=0; }
	quotient_tpl(type_max) { x=1;y=0; }
	explicit quotient_tpl(ftype _x,ftype _y=1) { x=_x;y=_y; }
	quotient_tpl(const quotient_tpl<ftype>& src) { x=src.x;y=src.y; }
	template<class ftype1> quotient_tpl(const quotient_tpl<ftype1>& src) { x=src.x;y=src.y; }
	// cppcheck-suppress zerodiv
	template<class ftype1,class ftype2> ILINE quotient_tpl& set(ftype1 nx,ftype2 ny) { (*this=nx)/=ny; return *this; }

	ILINE quotient_tpl& operator=(const quotient_tpl<ftype>& src) { x=src.x;y=src.y; return *this; }
	template<class ftype1> ILINE quotient_tpl& operator=(const quotient_tpl<ftype1>& src) { x=src.x;y=src.y; return *this; }
	ILINE quotient_tpl& operator=(ftype src) { x=src;y=1; return *this; }

	ILINE quotient_tpl& fixsign() { int sgny=::sgnnz(y); x*=sgny; y*=sgny; return *this; }
	ILINE ftype val() { return y!=0 ? x/y : 0; }

	ILINE quotient_tpl operator-() const { return quotient_tpl(-x,y); }

	ILINE quotient_tpl operator*(ftype op) const { quotient_tpl rvo(x*op,y); return rvo;}
	ILINE quotient_tpl operator/(ftype op) const { quotient_tpl rvo(x,y*op); return rvo;}
	ILINE quotient_tpl operator+(ftype op) const { quotient_tpl rvo(x+y*op,y); return rvo;}
	ILINE quotient_tpl operator-(ftype op) const { quotient_tpl rvo(x-y*op,y); return rvo;}

	ILINE quotient_tpl& operator*=(ftype op) { x*=op; return *this; }
	ILINE quotient_tpl& operator/=(ftype op) { y*=op; return *this; }
	ILINE quotient_tpl& operator+=(ftype op) { x+=op*y; return *this; }
	ILINE quotient_tpl& operator-=(ftype op) { x-=op*y; return *this; }

	ILINE bool operator==(ftype op) const { return x==op*y; }
	ILINE bool operator!=(ftype op) const { return x!=op*y; }
	ILINE bool operator<(ftype op) const { return x-op*y<0; }
	ILINE bool operator>(ftype op) const { return x-op*y>0; }
	ILINE bool operator<=(ftype op) const { return x-op*y<=0; }
	ILINE bool operator>=(ftype op) const { return x-op*y>=0; }

	ILINE int sgn() { return ::sgn(x); }
	ILINE sgnnz_result_type(ftype) sgnnz() { return ::sgnnz(x); }
	ILINE isneg_result_type(ftype) isneg() { return ::isneg(x); }
	ILINE int isnonneg() { return ::isnonneg(x); }
	ILINE isneg_result_type(ftype) isin01() { return ::isneg(fabs_tpl(x*2-y)-fabs_tpl(y)); }

	ftype x,y;
};

inline float getmin(float) { return 1E-20f; }
inline double getmin(double) { return 1E-80; }
template<> inline quotient_tpl<float>::quotient_tpl(type_min) { x=-1.f;y=1E-20f; }
template<> inline quotient_tpl<float>::quotient_tpl(type_max) { x=1.f;y=1E-20f; }
template<> inline quotient_tpl<double>::quotient_tpl(type_min) { x=-1.;y=1E-80; }
template<> inline quotient_tpl<double>::quotient_tpl(type_max) { x=1.;y=1E-80; }

template<class ftype1,class ftype2>
ILINE quotient_tpl<ftype1> operator*(const quotient_tpl<ftype1> &op1,const quotient_tpl<ftype2> &op2) { quotient_tpl<ftype1> rvo(op1.x*op2.x,op1.y*op2.y); return rvo;}
template<class ftype1,class ftype2>
ILINE quotient_tpl<ftype1> operator/(const quotient_tpl<ftype1> &op1,const quotient_tpl<ftype2> &op2) { quotient_tpl<ftype1> rvo(op1.x*op2.y,op1.y*op2.x); return rvo;}
template<class ftype1,class ftype2>
ILINE quotient_tpl<ftype1> operator+(const quotient_tpl<ftype1> &op1,const quotient_tpl<ftype2> &op2) 
{ /*op1.y==op2.y ? quotient_tpl<ftype1>(op1.x+op2.x,op1.y) :*/ quotient_tpl<ftype1> rvo(op1.x*op2.y+op2.x*op1.y, op1.y*op2.y); return rvo;}
template<class ftype1,class ftype2>
ILINE quotient_tpl<ftype1> operator-(const quotient_tpl<ftype1> &op1,const quotient_tpl<ftype2> &op2) 
{ /*op1.y==op2.y ? quotient_tpl<ftype1>(op1.x-op2.x,op1.y) :*/ quotient_tpl<ftype1> rvo(op1.x*op2.y-op2.x*op1.y, op1.y*op2.y); return rvo;}

template<class ftype1,class ftype2>
ILINE quotient_tpl<ftype1>& operator*=(quotient_tpl<ftype1> &op1,const quotient_tpl<ftype2> &op2) { op1.x*=op2.x; op1.y*=op2.y; return op1; }
template<class ftype1,class ftype2>
ILINE quotient_tpl<ftype1>& operator/=(quotient_tpl<ftype1> &op1,const quotient_tpl<ftype2> &op2) { op1.x*=op2.y; op1.y*=op2.x; return op1; }
template<class ftype1,class ftype2>	ILINE quotient_tpl<ftype1>& operator+=(quotient_tpl<ftype1> &op1,const quotient_tpl<ftype2> &op2) 
{ /*if (op1.y==op2.y) op1.x+=op2.x; else*/ { op1.x=op1.x*op2.y+op2.x*op1.y; op1.y*=op2.y; } return op1;	}
template<class ftype1,class ftype2>	ILINE quotient_tpl<ftype1>& operator-=(quotient_tpl<ftype1> &op1,const quotient_tpl<ftype2> &op2) 
{ /*if (op1.y==op2.y) op1.x-=op2.x; else*/ { op1.x=op1.x*op2.y-op2.x*op1.y; op1.y*=op2.y; } return op1;	}

template<class ftype> ILINE quotient_tpl<ftype> operator*(ftype op, const quotient_tpl<ftype> &q) { return quotient_tpl<ftype>(q.x*op,q.y); }
template<class ftype> ILINE quotient_tpl<ftype> operator/(ftype op, const quotient_tpl<ftype> &q) { return quotient_tpl<ftype>(q.x,q.y*op); }
template<class ftype> ILINE quotient_tpl<ftype> operator+(ftype op, const quotient_tpl<ftype> &q) { return quotient_tpl<ftype>(op*q.y+q.x,q.y); }
template<class ftype> ILINE quotient_tpl<ftype> operator-(ftype op, const quotient_tpl<ftype> &q) { return quotient_tpl<ftype>(op*q.y-q.x,q.y); }

template<class ftype> ILINE bool operator==(ftype op1, const quotient_tpl<ftype> &op2) { return op1*op2.y==op2.x; }
template<class ftype> ILINE bool operator!=(ftype op1, const quotient_tpl<ftype> &op2) { return op1*op2.y!=op2.x; }
template<class ftype> ILINE bool operator<(ftype op1, const quotient_tpl<ftype> &op2) { return op1*op2.y-op2.x<0; }
template<class ftype> ILINE bool operator>(ftype op1, const quotient_tpl<ftype> &op2) { return op1*op2.y-op2.x>0; }
template<class ftype> ILINE bool operator<=(ftype op1, const quotient_tpl<ftype> &op2) { return op1*op2.y-op2.x<=0; }
template<class ftype> ILINE bool operator>=(ftype op1, const quotient_tpl<ftype> &op2) { return op1*op2.y-op2.x>=0; }

template<class ftype1,class ftype2> ILINE bool operator==(const quotient_tpl<ftype1> &op1, const quotient_tpl<ftype2> &op2) 
{ return op1.x*op2.y==op2.x*op1.y; }
template<class ftype1,class ftype2> ILINE bool operator!=(const quotient_tpl<ftype1> &op1, const quotient_tpl<ftype2> &op2) 
{ return op1.x*op2.y!=op2.x*op1.y; }
template<class ftype1,class ftype2> ILINE bool operator<(const quotient_tpl<ftype1> &op1, const quotient_tpl<ftype2> &op2) 
{ return op1.x*op2.y-op2.x*op1.y + getmin(op1.x)*(op1.x-op2.x) < 0; }
template<class ftype1,class ftype2> ILINE bool operator>(const quotient_tpl<ftype1> &op1, const quotient_tpl<ftype2> &op2) 
{ return op1.x*op2.y-op2.x*op1.y + getmin(op1.x)*(op1.x-op2.x) > 0; }
template<class ftype1,class ftype2> ILINE bool operator<=(const quotient_tpl<ftype1> &op1, const quotient_tpl<ftype2> &op2) 
{ return op1.x*op2.y-op2.x*op1.y + getmin(op1.x)*(op1.x-op2.x) <= 0; }
template<class ftype1,class ftype2> ILINE bool operator>=(const quotient_tpl<ftype1> &op1, const quotient_tpl<ftype2> &op2) 
{ return op1.x*op2.y-op2.x*op1.y + getmin(op1.x)*(op1.x-op2.x) >= 0; }

template<class ftype> ILINE int sgn(const quotient_tpl<ftype> &op) { return sgn(op.x); }
template<class ftype> ILINE sgnnz_result_type(ftype) sgnnz(const quotient_tpl<ftype> &op) { return sgnnz(op.x); }
template<class ftype> ILINE isneg_result_type(ftype) isneg(const quotient_tpl<ftype> &op) { return isneg(op.x); }
template<class ftype> ILINE int isnonneg(const quotient_tpl<ftype> &op) { return isnonneg(op.x); }
template<class ftype> ILINE int sgn_safe(const quotient_tpl<ftype> &op) { return sgn(op.x)*sgnnz(op.y); }
template<class ftype> ILINE sgnnz_result_type(ftype) sgnnz_safe(const quotient_tpl<ftype> &op) { return sgnnz(op.x*op.y); }
template<class ftype> ILINE isneg_result_type(ftype) isneg_safe(const quotient_tpl<ftype> &op) { return isneg(op.x*op.y); }
template<class ftype> ILINE int isnonneg_safe(const quotient_tpl<ftype> &op) { return isnonneg(op.x)*isnonneg(op.y); }
template<class ftype> ILINE int inrange(const quotient_tpl<ftype> &op, ftype end0, ftype end1) { return isneg(sgn_safe(op-end0)*sgn_safe(op-end1)); }

namespace crymath {

template<class ftype> ILINE quotient_tpl<ftype> abs(const quotient_tpl<ftype> op) { return quotient_tpl<ftype>(abs(op.x), abs(op.y)); }

} // namespace crymath

template<class ftype> ILINE quotient_tpl<ftype> sqr_signed(const quotient_tpl<ftype>& op)  { return op * crymath::abs(op); }

template<class ftype> ILINE quotient_tpl<ftype> fake_atan2(ftype y,ftype x) {
	quotient_tpl<ftype> res;
	ftype src[2] = { x,y };
	int ix=isneg(x),iy=isneg(y),iflip=isneg(fabs_tpl(x)-fabs_tpl(y));
	res.x = src[iflip^1]*(1-iflip*2)*sgnnz(src[iflip]); res.y = fabs_tpl(src[iflip]);
	res += (iy*2+(ix^iy)+(iflip^ix^iy))*2;
	return res;
}

typedef quotient_tpl<float> quotientf;
typedef quotient_tpl<real> quotient;
typedef quotient_tpl<int> quotienti;

#endif
