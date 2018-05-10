/*
	basic math definitions
*/

#pragma once

//#include "stdafx.h"	//for defines into other projects

#include <math.h>
#include <CryMath/Cry_Math.h>
typedef Vec3_tpl<double> TVec;
typedef Vec2_tpl<double> TVec2D;

namespace NSH
{
	const double g_cPi = 3.1415926535897932384626433832795;

	//!< generate random number between 0 and 1
	inline double Random()
	{
		const double s_cInvRandMax = 1. / (double)RAND_MAX;
		return ((double)rand() * s_cInvRandMax);
	}

	//!< returns the real sign
	template<class TFloatType>
	inline const TFloatType Sign(const TFloatType cValue)
	{
		return ((cValue < (TFloatType)0.0)? (TFloatType)-1.0 : (TFloatType)1.0);
	}

	//!< sin fct. coverage , float type
	//!< @param cX argument for sin
	inline const float Sin(const float cX)
	{
		return sinf(cX);
	}

	//!< sin fct. coverage , double type
	//!< @param cX argument for sin
	inline const double Sin(const double cX)
	{
		return sin(cX);
	}

	//!< abs fct. coverage , float type
	//!< @param cX argument for abs
	inline const float Abs(const float cX)
	{
		return fabsf(cX);
	}

	//!< abs fct. coverage , double type
	//!< @param cX argument for abs
	inline const double Abs(const double cX)
	{
		return fabs(cX);
	}

	//!< tan fct. coverage , float type
	//!< @param cX argument for tan
	inline const float Tan(const float cX)
	{
		return tanf(cX);
	}

	//!< tan fct. coverage , double type
	//!< @param cX argument for tan
	inline const double Tan(const double cX)
	{
		return tan(cX);
	}

	//!< cos fct. coverage , float type
	//!< @param cX argument for cos
	inline const float Cos(const float cX)
	{
		return cosf(cX);
	}

	//!< cos fct. coverage , double type
	//!< @param cX argument for cos
	inline const double Cos(const double cX)
	{
		return cos(cX);
	}

	//!< acos fct. coverage , float type
	//!< @param cX argument for acos
	inline const float ACos(const float cX)
	{
		return acosf(cX);
	}

	//!< acos fct. coverage , double type
	//!< @param cX argument for acos
	inline const double ACos(const double cX)
	{
		return acos(cX);
	}

	//!< asin fct. coverage , float type
	//!< @param cX argument for asin
	inline const float ASin(const float cX)
	{
		return asinf(cX);
	}

	//!< cos fct. coverage , double type
	//!< @param cX argument for asin
	inline const double ASin(const double cX)
	{
		return asin(cX);
	}

	//!< square root fct. coverage , float type
	//!< @param cX argument for sqrt
	//!< @return sqrt
	inline const float Sqrt(const float cX)
	{
		return sqrtf(cX);
	}

	//!< square root fct. for int parameters
	//!< @param cX argument for sqrt
	//!< @return sqrt
	inline const uint32 Sqrt(const uint32 cX)
	{
		return (uint32) sqrtf((float)cX);
	}

	//!< square root fct. coverage , double type
	//!< @param cX argument for sqrt
	//!< @return sqrt
	inline const double Sqrt(const double cX)
	{
		return sqrt(cX);
	}

	//!< normalizes a vector and returns it
	inline TVec& Normalize(TVec& rVec)
	{
		rVec.Normalize();
		return rVec;
	}

	//!< normalizes a vector and returns it
	inline Vec3& Normalize(Vec3& rVec)
	{
		rVec.Normalize();
		return rVec;
	}
	inline Vec3 GetNormalized(const Vec3& rVec)
	{
		Vec3 temp(rVec);
		temp.Normalize();
		return temp;
	}

	//!< normalizes a vector and returns it
	template<class TFloatType>
	inline const bool IsNormalized(const Vec3_tpl<TFloatType>& crVec)
	{
		const TFloatType s_cThreshold = (TFloatType)0.01;
		return (Abs(crVec.GetLengthSquared() - 1.) < s_cThreshold);
	}

	//!< computes the area of a triangle
	template<class TFloatType>
	inline const TFloatType CalcTriangleArea(const Vec3_tpl<TFloatType>& crA, const Vec3_tpl<TFloatType>& crB, const Vec3_tpl<TFloatType>& crC)
	{
		return (((crC - crA).Cross(crB - crA)).GetLength() * 0.5f);
	}

	//!< calculates barycentric coordinates for a certain point in the triangle, returns true if it fails
	template<class TFloatType>
	inline const bool CalcBaryCoords
		(const Vec3_tpl<TFloatType>& crA, const Vec3_tpl<TFloatType>& crB, const Vec3_tpl<TFloatType>& crC, 
		const Vec3_tpl<TFloatType>& crPos, TFloatType& rAlpha, TFloatType& rBeta, TFloatType& rGamma)
	{
		//calc triangle area
		const TFloatType cTriangleArea = CalcTriangleArea(crA, crB, crC);
		//first sub tri
		const TFloatType a1 = CalcTriangleArea(crPos, crB, crC);
		//second sub tri  
		const TFloatType a2 = CalcTriangleArea(crA, crPos, crC);
		//third sub tri  
		const TFloatType a3 = CalcTriangleArea(crA, crB, crPos);
		//check sum, must be close to real area
		const TFloatType asum = a1 + a2 + a3;
		bool cBaryFailCond = false;
		if(Abs(asum - cTriangleArea) > cTriangleArea * (TFloatType)0.01)
			cBaryFailCond = true;	//area difference to large
		else
		{
			//calc alpha beta and gamma components as area ratio
			const TFloatType invArea = 1./cTriangleArea;
			rAlpha		= a1 * invArea;
			rBeta			= a2 * invArea;
			rGamma		= a3 * invArea;
		}
		return cBaryFailCond;
	}

	//!< returns the vector containing for each coordinate the largest value from two vectors, needed by KD tree, must be implemented for each KD-tree key-type
	template<class TFloatType>
	inline const Vec3_tpl<TFloatType> VecMax(const Vec3_tpl<TFloatType>& crV0, const Vec3_tpl<TFloatType>& crV1)
	{
		Vec3_tpl<TFloatType> v;
		for(int i=0;i<3;i++) 
			v[i] = std::max(crV0[i],crV1[i]);
		return v;
	}

	//!< returns the vector containing for each coordinate the smallest value from two vectors, needed by KD tree, must be implemented for each KD-tree key-type
	template<class TFloatType>
	inline const Vec3_tpl<TFloatType> VecMin(const Vec3_tpl<TFloatType>& crV0, const Vec3_tpl<TFloatType>& crV1)
	{
		Vec3_tpl<TFloatType> v;
		for(int i=0;i<3;i++) 
			v[i] = std::min(crV0[i],crV1[i]);
		return v;
	}

	//!< dot product function needed by KD tree, must be implemented for each KD-tree key-type
	template<class TFloatType>
	inline const TFloatType Dot(const Vec3_tpl<TFloatType>& crV0,const Vec3_tpl<TFloatType>& crV1)
	{
		return crV0.x * crV1.x + crV0.y * crV1.y + crV0.z * crV1.z;
	}


	//!< ray - triangle intersection routines
	namespace NRayTriangleIntersect
	{
		//!< retrieves the barycentric coordinates of an intersection, there is no test if it really hits
		inline void RetrieveBarycentricCoords(const Vec3& crOrigin, const Vec3& crDir, const Vec3& crV0, const Vec3& crV1, const Vec3& crV2, Vec3& rBary)
		{
			// find vectors for two edges sharing crV0 
			const Vec3 cEdge1 = crV1 - crV0;
			const Vec3 cEdge2 = crV2 - crV0;
			const Vec3 cPVec = crDir % cEdge2;
			const float cDet = cEdge1 * cPVec;
			const Vec3 cTVec = crOrigin - crV0;
			rBary.y = cTVec * cPVec;
			const Vec3 cQVec = cTVec % cEdge1;
			rBary.z = crDir * cQVec;
			const float inv_det = 1.f / cDet;
			rBary.y *= inv_det;
			rBary.z *= inv_det;
			rBary.x = 1.f - (rBary.y + rBary.z);
		}

		//!< ray triangle intersection for barycentric coords retrieval
		inline const bool RayTriangleIntersect(const Vec3& crOrigin, const Vec3& crDir, const Vec3 &crV0, const Vec3 &crV1, const Vec3 &crV2, Vec3 &rOutputPos) 
		{
			const float scEpsilon = 0.000001f;
			//find vectors for two edges sharing crV0
			const Vec3 cEdge1 = crV1 - crV0;
			const Vec3 cEdge2 = crV2 - crV0;
			//begin calculating determinant - also used to calculate U parameter
			const Vec3 cPVec  =  crDir % cEdge2; 
			//if determinant is near zero, ray lies in plane of triangle 
			const float cDet = cEdge1 * cPVec;
			if (cDet < scEpsilon) 
				return false;
			//calculate distance from crV0 to ray origin
			const Vec3 cTVec = crOrigin - crV0;
			//calculate U parameter and test bounds
			const float cU = cTVec * cPVec;
			if (cU < 0.0f || cU > cDet) 
				return false;
			//prepare to test V parameter
			const Vec3 cQVec = cTVec % cEdge1;
			//calculate V parameter and test bounds
			const float cV = (crDir * cQVec);
			if ( cV < 0.0f || (cU + cV) > cDet) 
				return false;
			//we have an intersection and we can calculate cT
			const float cT = (cEdge2 * cQVec) / cDet;
			//we use cT as a scale parameter, to get the 3D-intersection point
			rOutputPos = (crDir * cT) + crOrigin;
			return true;
		}

		//!< ray triangle intersection for intersection tests
		static const bool RayTriangleIntersectTest(const Vec3& crOrigin, const Vec3& crDir, const Vec3 &crV0, const Vec3 &crV1, const Vec3 &crV2, Vec3 &rOutputPos, float& rT, float& rBaryU, float& rBaryV, const bool cCull) 
		{
			const float scEpsilon = 0.000001f;
			// find vectors for two edges sharing crV0 
			const Vec3 cEdge1 = crV1 - crV0;
			const Vec3 cEdge2 = crV2 - crV0;
			// begin calculating determinant - also used to calculate U parameter 
			const Vec3 cPVec = crDir % cEdge2;
			// if determinant is near zero, ray lies in plane of triangle 
			const float cDet = cEdge1 * cPVec;
			if(cCull)
			{
				//culling is desired 
				if (cDet < scEpsilon)
					return false;
				// calculate distance from crV0 to ray origin 
				const Vec3 cTVec = crOrigin - crV0;
				// calculate U parameter and test bounds 
				rBaryU = cTVec * cPVec;
				if (rBaryU < 0.0 || rBaryU > cDet)
					return false;
				// prepare to test V parameter 
				const Vec3 cQVec = cTVec % cEdge1;
				// calculate V parameter and test bounds 
				rBaryV = crDir * cQVec;
				if (rBaryV < 0.0 || rBaryU + rBaryV > cDet)
					return false;
				// calculate rT, scale parameters, ray intersects triangle 
				rT = cEdge2 * cQVec;
				const float inv_det = 1.f / cDet;
				rT *= inv_det;
				rOutputPos = (crDir * rT) + crOrigin;
				rBaryU *= inv_det;
				rBaryV *= inv_det;
			}
			else                    
			{
				//no culling desired
				if (cDet > -scEpsilon && cDet < scEpsilon)//ray is nearly in the plane of the triangle
					return false;
				const float inv_det = 1.f / cDet;
				// calculate distance from crV0 to ray origin 
				const Vec3 cTVec = crOrigin - crV0;
				// calculate U parameter and test bounds 
				rBaryU = (cTVec * cPVec) * inv_det;
				if (rBaryU < 0.0 || rBaryU > 1.0)
					return false;
				// prepare to test V parameter 
				const Vec3 cQVec = cTVec % cEdge1;
				// calculate V parameter and test bounds 
				rBaryV = (crDir * cQVec) * inv_det;
				if (rBaryV < 0.0 || rBaryU + rBaryV > 1.0)
					return false;
				// calculate rT, ray intersects triangle 
				rT = (cEdge2 * cQVec) * inv_det;
				rOutputPos = (crDir * rT) + crOrigin;
			}
			return true;
		}
	}//NRayTriangleIntersect
}


