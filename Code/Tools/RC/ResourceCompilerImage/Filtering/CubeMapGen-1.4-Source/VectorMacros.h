// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//  VectorMacros.hpp 
// 
//  Fast operations on vectors, stored as arrays of floats
//
//--------------------------------------------------------------------------------------
//  (C) 2001-2005 ATI Research, Inc.  All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef VECTORMACROS_HPP
#define VECTORMACROS_HPP

#include "types.h"
#include <math.h>

//disable warning about doubles being converted down to float
#pragma warning (disable : 4244 )

#define VM_LARGE_FLOAT 3.7e37f

#define VM_MIN(a, b)  (((a) < (b)) ? (a) : (b)) 
#define VM_MAX(a, b)  (((a) > (b)) ? (a) : (b)) 

//clamping macros
#define VM_CLAMP(d, s, mn, mx){(d) = ((s)<(mx))?( ((s)>(mn))?(s):(mn) ):(mx); }

#define VM_CLAMP2_UNTYPED(d, s, mn, mx) {VM_CLAMP(d[0], s[0], mn, mx); VM_CLAMP(d[1], s[1], mn, mx);}
#define VM_CLAMP2(d, s, mn, mx) VM_CLAMP2_UNTYPED(((float32 *)(d)), ((float32 *)(s)), (float32)(mn), (float32)(mx)) 

#define VM_CLAMP3_UNTYPED(d, s, mn, mx) {VM_CLAMP(d[0], s[0], mn, mx); VM_CLAMP(d[1], s[1], mn, mx); VM_CLAMP(d[2], s[2], mn, mx);}
#define VM_CLAMP3(d, s, mn, mx) VM_CLAMP3_UNTYPED(((float32 *)(d)), ((float32 *)(s)), (float32)(mn), (float32)(mx)) 

#define VM_CLAMP4_UNTYPED(d, s, mn, mx) {VM_CLAMP(d[0], s[0], mn, mx); VM_CLAMP(d[1], s[1], mn, mx); VM_CLAMP(d[2], s[2], mn, mx); VM_CLAMP(d[3], s[3], mn, mx);}
#define VM_CLAMP4(d, s, mn, mx) VM_CLAMP4_UNTYPED(((float32 *)(d)), ((float32 *)(s)), (float32)(mn), (float32)(mx)) 


//set vectors
#define VM_SET2_UNTYPED(d, f) { d[0]=f; d[1]=f;} 
#define VM_SET2(d, f) VM_SET2_UNTYPED(((float32 *)(d)), ((float32)(f))) 

#define VM_SET3_UNTYPED(d, f) { d[0]=f; d[1]=f; d[2]=f; } 
#define VM_SET3(d, f) VM_SET3_UNTYPED(((float32 *)(d)), ((float32)(f))) 

#define VM_SET4_UNTYPED(d, f) { d[0]=f; d[1]=f; d[2]=f; d[3]=f; } 
#define VM_SET4(d, f) VM_SET4_UNTYPED(((float32 *)(d)), ((float32)(f))) 


//copy vectors
#define VM_COPY2_UNTYPED(d, s) { d[0]=s[0]; d[1]=s[1];} 
#define VM_COPY2(d, s) VM_COPY2_UNTYPED(((float32 *)(d)), ((float32 *)(s))) 

#define VM_COPY3_UNTYPED(d, s) { d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; } 
#define VM_COPY3(d, s) VM_COPY3_UNTYPED(((float32 *)(d)), ((float32 *)(s))) 

#define VM_COPY4_UNTYPED(d, s) { d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d[3]=s[3]; } 
#define VM_COPY4(d, s) VM_COPY4_UNTYPED(((float32 *)(d)), ((float32 *)(s))) 


//add two vectors
#define VM_ADD2_UNTYPED(d, sa, sb) { d[0]=sa[0]+sb[0]; d[1]=sa[1]+sb[1]; } 
#define VM_ADD2(d, sa, sb) VM_ADD3_UNTYPED(((float32 *)(d)), ((float32 *)(sa)), ((float32 *)(sb))) 

#define VM_ADD3_UNTYPED(d, sa, sb) { d[0]=sa[0]+sb[0]; d[1]=sa[1]+sb[1]; d[2]=sa[2]+sb[2]; } 
#define VM_ADD3(d, sa, sb) VM_ADD3_UNTYPED(((float32 *)(d)), ((float32 *)(sa)), ((float32 *)(sb))) 

#define VM_ADD4_UNTYPED(d, sa, sb) { d[0]=sa[0]+sb[0]; d[1]=sa[1]+sb[1]; d[2]=sa[2]+sb[2]; d[3]=sa[3]+sb[3]; } 
#define VM_ADD4(d, sa, sb) VM_ADD4_UNTYPED(((float32 *)(d)), ((float32 *)(sa)), ((float32 *)(sb))) 


//subtract two vectors
#define VM_SUB2_UNTYPED(d, sa, sb) { d[0]=sa[0]-sb[0]; d[1]=sa[1]-sb[1]; } 
#define VM_SUB2(d, sa, sb) VM_SUB2_UNTYPED(((float32 *)(d)), ((float32 *)(sa)), ((float32 *)(sb))) 

#define VM_SUB3_UNTYPED(d, sa, sb) { d[0]=sa[0]-sb[0]; d[1]=sa[1]-sb[1]; d[2]=sa[2]-sb[2]; } 
#define VM_SUB3(d, sa, sb) VM_SUB3_UNTYPED(((float32 *)(d)), ((float32 *)(sa)), ((float32 *)(sb))) 

#define VM_SUB4_UNTYPED(d, sa, sb) { d[0]=sa[0]-sb[0]; d[1]=sa[1]-sb[1]; d[2]=sa[2]-sb[2]; d[3]=sa[3]-sb[3]; } 
#define VM_SUB4(d, sa, sb) VM_SUB4_UNTYPED(((float32 *)(d)), ((float32 *)(sa)), ((float32 *)(sb))) 


//multiply all elements of a vector by a scalar
#define VM_SCALE2_UNTYPED(d, s, f) {d[0]=s[0]*f; d[1]=s[1]*f; }
#define VM_SCALE2(d, s, f) VM_SCALE2_UNTYPED(((float32 *)(d)), ((float32 *)(s)), ((float32)(f)) ) 

#define VM_SCALE3_UNTYPED(d, s, f) {d[0]=s[0]*f; d[1]=s[1]*f; d[2]=s[2]*f; }
#define VM_SCALE3(d, s, f) VM_SCALE3_UNTYPED(((float32 *)(d)), ((float32 *)(s)), ((float32)(f)) ) 

#define VM_SCALE4_UNTYPED(d, s, f) {d[0]=s[0]*f; d[1]=s[1]*f; d[2]=s[2]*f; d[3]=s[3]*f; }
#define VM_SCALE4(d, s, f) VM_SCALE4_UNTYPED(((float32 *)(d)), ((float32 *)(s)), ((float32)(f)) ) 


//add a scalar to all elements of a vector
#define VM_BIAS2_UNTYPED(d, s, f) { d[0]=s[0]+f; d[1]=s[1]+f; } 
#define VM_BIAS2(d, s, f) VM_BIAS2_UNTYPED(((float32 *)(d)), ((float32 *)(s)), ((float32)(f))) 

#define VM_BIAS3_UNTYPED(d, s, f) { d[0]=s[0]+f; d[1]=s[1]+f; d[2]=s[2]+f; } 
#define VM_BIAS3(d, s, f) VM_BIAS3_UNTYPED(((float32 *)(d)), ((float32 *)(s)), ((float32)(f))) 

#define VM_BIAS4_UNTYPED(d, s, f) { d[0]=s[0]+f; d[1]=s[1]+f; d[2]=s[2]+f; d[3]=s[3]+f; } 
#define VM_BIAS4(d, s, f) VM_BIAS4_UNTYPED(((float32 *)(d)), ((float32 *)(s)), ((float32)(f))) 


//3D cross product
#define VM_XPROD3_UNTYPED(d, sa, sb) { d[0]=sa[1]*sb[2]-sa[2]*sb[1]; d[1]=sa[2]*sb[0]-sa[0]*sb[2]; d[2]=sa[0]*sb[1]-sa[1]*sb[0]; } 
#define VM_XPROD3(d, sa, sb) VM_XPROD3_UNTYPED(((float32 *)(d)), ((float32 *)(sa)), ((float32 *)(sb))) 


//dot products
#define VM_DOTPROD2_UNTYPED(sa, sb) (sa[0]*sb[0]+ sa[1]*sb[1]) 
#define VM_DOTPROD2(sa, sb) VM_DOTPROD2_UNTYPED(((float32 *)(sa)), ((float32 *)(sb))) 

#define VM_DOTPROD3_UNTYPED(sa, sb) (sa[0]*sb[0]+ sa[1]*sb[1]+ sa[2]*sb[2]) 
#define VM_DOTPROD3(sa, sb) VM_DOTPROD3_UNTYPED(((float32 *)(sa)), ((float32 *)(sb))) 

#define VM_DOTPROD4_UNTYPED(sa, sb) (sa[0]*sb[0]+ sa[1]*sb[1]+ sa[2]*sb[2] + sa[3]*sb[3]) 
#define VM_DOTPROD4(sa, sb) VM_DOTPROD4_UNTYPED(((float32 *)(sa)), ((float32 *)(sb))) 


//dp3 then and add 4th component from second arguement
#define VM_DOTPROD3ADD_UNTYPED(pt, pl) (pt[0]*pl[0]+ pt[1]*pl[1]+ pt[2]*pl[2] + pl[3]) 
#define VM_DOTPROD3ADD(pt, pl) VM_DOTPROD3ADD_UNTYPED(((float32 *)(pt)), ((float32 *)(pl))) 


//normalize vectors
#define VM_NORM3_UNTYPED(d, s) {float64 __idsq; __idsq=1.0/sqrt(VM_DOTPROD3_UNTYPED(s,s)); d[0]=s[0]*__idsq; d[1]=s[1]*__idsq; d[2]=s[2]*__idsq; }
#define VM_NORM3_UNTYPED_F32(d, s) {float32 __idsq; __idsq=1.0/sqrt(VM_DOTPROD3_UNTYPED(s,s)); d[0]=s[0]*__idsq; d[1]=s[1]*__idsq; d[2]=s[2]*__idsq; }
#define VM_NORM3(d, s)  VM_NORM3_UNTYPED_F32(((float32 *)(d)), ((float32 *)(s))) 

#define VM_NORM4_UNTYPED(d, s) {float64 __idsq; __idsq=1.0/sqrt(VM_DOTPROD4_UNTYPED(s,s)); d[0]=s[0]*__idsq; d[1]=s[1]*__idsq; d[2]=s[2]*__idsq; d[3]=s[3]*__idsq; }
#define VM_NORM4_UNTYPED_F32(d, s) {float32 __idsq; __idsq=1.0/sqrt(VM_DOTPROD4_UNTYPED(s,s)); d[0]=s[0]*__idsq; d[1]=s[1]*__idsq; d[2]=s[2]*__idsq; d[3]=s[3]*__idsq; }
#define VM_NORM4(d, s)  VM_NORM4_UNTYPED_F32(((float32 *)(d)), ((float32 *)(s))) 


//safely normalize vectors, deal with 0 length case
#define VM_SAFENORM3_UNTYPED(d, s) {float32 __idsq, __dp; __dp = VM_DOTPROD3_UNTYPED(s,s); \
   __idsq=( (__dp > 0.0f)?(1.0/sqrt(__dp)):0.0f ) ; d[0]=s[0]*__idsq; d[1]=s[1]*__idsq; d[2]=s[2]*__idsq; }
#define VM_SAFENORM3(d, s)  VM_NORM3_UNTYPED_F32(((float32 *)(d)), ((float32 *)(s))) 



//absolute value
#define VM_ABS2_UNTYPED(d, s) { d[0] = fabs(s[0]); d[1] = fabs(s[1]); } 
#define VM_ABS2(d, s) VM_ABS2_UNTYPED(((float32 *)(d)), ((float32 *)(s)) ) 

#define VM_ABS3_UNTYPED(d, s) { d[0] = fabs(s[0]); d[1] = fabs(s[1]); d[2] = fabs(s[2]); } 
#define VM_ABS3(d, s) VM_ABS3_UNTYPED(((float32 *)(d)), ((float32 *)(s)) ) 

#define VM_ABS4_UNTYPED(d, s) { d[0] = fabs(s[0]); d[1] = fabs(s[1]); d[2] = fabs(s[2]); d[3] = fabs(s[3]); } 
#define VM_ABS4(d, s) VM_ABS4_UNTYPED(((float32 *)(d)), ((float32 *)(s)) ) 


//projection of a vector onto another vector (assumes vector v is normalized)
// computes d, which is the parallel component of s onto vector v
#define VM_PROJ3_UNTYPED(d, s, v) { float64 __dp; __dp = VM_DOTPROD3_UNTYPED(s, v); VM_SCALE3_UNTYPED(d, s, __dp); }
#define VM_PROJ3(d, s, v)     VM_PROJ3_UNTYPED(((float32 *)(d)), ((float32 *)(s)), ((float32 *)(v)) ) 
#define VM_PROJ3_F64(d, s, v) VM_PROJ3_UNTYPED(((float64 *)(d)), ((float64 *)(s)), ((float64 *)(v)) ) 


//compute component of a vector perpendicular to another vector
// d is perpendicular component of s onto vector v
//  this macro first computes the parallel projection, then subtracts off from the original vector 
//  to obtain the perpendicular component
#define VM_PERP3_UNTYPED(d, s, v) {float64 __proj[3]; VM_PROJ3_UNTYPED(__proj, s, v); VM_SUB3_UNTYPED(d, s, __proj); }
#define VM_PERP3(d, s, v)     VM_PERP3_UNTYPED(((float32 *)(d)), ((float32 *)(s)), ((float32 *)(v)) ) 
#define VM_PERP3_F64(d, s, v) VM_PERP3_UNTYPED(((float64 *)(d)), ((float64 *)(s)), ((float64 *)(v)) ) 






#endif //VECTORMACROS_HPP
