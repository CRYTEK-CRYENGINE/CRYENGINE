// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// Used with permission to distribute for non-commercial purposes.
//
//
//	File:TangentSpaceCalculation.h
//  Description: calculated the tangent space base vector for a given mesh
//  Dependencies: none
//  Documentation: "How to calculate tangent base vectors.doc"
//
//	History:
//	- 12/07/2002: Created by Martin Mittring as part of CryEngine
//  - 08/18/2003: MM improved stability (no illegal floats) with bad input data
//  - 08/19/2003: MM added check for input data problems (DebugMesh() is deactivated by default)
//  - 10/02/2003: MM removed rundundant code
//  - 10/01/2004: MM added errorcodes (NAN texture coordinates)
//  - 05/21/2005: MM made proxy interface typesafe
//  - 22/09/2012: Bogdan refactored and added Mikkelsen's Tangent Basis algorithm to have better support for mirrored geometry
//
//////////////////////////////////////////////////////////////////////

#ifndef TANGENTSPACECALCULATION_H
#define TANGENTSPACECALCULATION_H

#pragma once

enum eCalculateTangentSpaceErrorCode
{
	CALCULATE_TANGENT_SPACE_NO_ERRORS,
	BROKEN_TEXTURE_COORDINATES,
	VERTICES_SHARING_COORDINATES,
	ALL_VERTICES_ON_THE_SAME_VECTOR,
	MEMORY_ALLOCATION_FAILED
};

class ITriangleInputProxy
{
public:
	virtual ~ITriangleInputProxy(){}

	virtual uint32 GetTriangleCount() const = 0;
	virtual void   GetTriangleIndices(const uint32 indwTriNo, uint32 outdwPos[3], uint32 outdwNorm[3], uint32 outdwUV[3]) const = 0;
	virtual void   GetPos(const uint32 indwPos, Vec3& outfPos) const = 0;
	virtual void   GetUV(const uint32 indwPos, Vec2& outfUV) const = 0;
	virtual void   GetNorm(const uint32 indwTriNo, const uint32 indwVertNo, Vec3& outfNorm) const = 0;
};

class CTangentSpaceCalculation
{
public:
	//! /param inInput - the normals are only used as smoothing input - we calculate the normals ourself
	eCalculateTangentSpaceErrorCode CalculateTangentSpace(const ITriangleInputProxy& inInput, bool bUseCustomNormals, bool bIgnoreDegeneracies, string& errorMessage);

	size_t                          GetBaseCount();
	void                            GetTriangleBaseIndices(const uint32 indwTriNo, uint32 outdwBase[3]);

	//! returns a orthogonal base (perpendicular and normalized)
	void GetBase(const uint32 indwPos, float* outU, float* outV, float* outN);

private:

	struct CBase33
	{
		CBase33();
		CBase33(const Vec3& Uval, const Vec3& Vval, const Vec3& Nval);

		Vec3 u;
		Vec3 v;
		Vec3 n; // part of the tangent base but can be used also as vertex normal
	};

	struct CVec3PredicateLess
	{
		bool operator()(const Vec3& first, const Vec3& second) const;
	};

	struct CBase33PredicateLess
	{
		bool operator()(const CBase33& first, const CBase33& second) const;
	};

	struct CBaseIndex
	{
		// position index in the positions stream
		uint32 m_posIndex;
		// normal index in the vertex normals stream
		uint32 m_normIndex;
	};

	struct CBaseIndexOrder : public std::binary_function<CBaseIndex, CBaseIndex, bool>
	{
		bool operator()(const CBaseIndex& a, const CBaseIndex& b) const;
	};

	struct CTriBaseIndex
	{
		uint32 p[3]; //!< index in m_BaseVectors
	};

	// [dwTriangleCount]
	std::vector<CTriBaseIndex> m_trianglesBaseAssigment;
	// [0..] generated output data
	std::vector<CBase33>       m_baseVectors;

	eCalculateTangentSpaceErrorCode CalculateTangentSpaceMikk(const ITriangleInputProxy& inInput, string& errorMessage);

	CBase33& GetBase(std::multimap<CBaseIndex, uint32, CBaseIndexOrder>& inMap, const uint32 indwPosNo, const uint32 indwNormNo);
	uint32   AddUV2Base(std::multimap<CBaseIndex, uint32, CBaseIndexOrder>& inMap, const uint32 indwPosNo, const uint32 indwNormNo, const Vec3& inU, const Vec3& inV, const Vec3& inNormN, bool bIgnoreDegeneracies);
	void     AddNormal2Base(std::multimap<CBaseIndex, uint32, CBaseIndexOrder>& inMap, const uint32 indwPosNo, const uint32 indwNormNo, const Vec3& inNormal);
	Vec3     Rotate(const Vec3& vFrom, const Vec3& vTo, const Vec3& vInput);
	void     DebugMesh(const ITriangleInputProxy& inInput) const;
	float    CalcAngleBetween(const Vec3& invA, const Vec3& invB);
};

#endif
