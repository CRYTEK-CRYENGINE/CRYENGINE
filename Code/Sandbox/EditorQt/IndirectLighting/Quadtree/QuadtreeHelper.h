// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   quadtree helper functions and structures
 */
#ifndef QUADTREE_HELPER_H
#define QUADTREE_HELPER_H

#pragma once

#include <CryMath/Cry_Math.h>
#include <vector>
#include <set>
#include <limits>     //numeric_limits
#include <memory.h>   //memcpy
#include <algorithm>  //sort

namespace
{
//!< LOKI stuff to be able to partial template specialize global functions
template<typename T>
struct SType2Type
{
	typedef T OriginalType;
};

//!< retrieves for a certain type the minimum radius of distance (below resulting in no position changes for subdivision)
template<class TUnitType>
const TUnitType GetMinSubdivDist(SType2Type<TUnitType> );
};

//!< namespace for all quadtree related types
namespace NQT
{
//!< fast static vector class
//!< provides necessary vector-like interface but is allocated on heap
//!< used to avoid reallocations or static data (unusable for MT)
template<class T>
class CTravVec
{
public:
	CTravVec();
	void         push_back(const T& rAdd);
	const uint32 size() const;
	T&         operator[](const uint32 cIndex);
	const T&   operator[](const uint32 cIndex) const;
	const bool empty() const;

private:
	static const uint32 scMaxArrayCount = 4096;

	uint8               m_Mem[sizeof(T) * scMaxArrayCount];
	uint32              m_Count;
};

//!< quadtree child indices
//!< A denotes added, S denotes subtracted and always related to XY
typedef enum {SS = 0, AS = 1, SA = 2, AA = 3}                   TEChildIndex;
typedef enum {SS_E = 0, AS_E = 1, SA_E = 2, AA_E = 3, NP_E = 4} TEChildIndexExt; //extended version with a denotion of a non pos

//!< InsertLeaf ret value
typedef enum
{
	SUCCESS = 0,
	POS_EQUAL_TO_EXISTING_LEAF,
	NOT_ENOUGH_SUBDIV_LEVELS,
	TOO_MANY_LEAVES,
	LEAVE_OUTSIDE_QUADTREE,
	INSERTION_RESULT_COUNT
} EInsertionResult;

//!< PropagateLeavesToNewNodeFallback ret value
//!< we need to know whether it has not been inserted in the cell where the pos actually lies in or just in  shared radius cell
typedef enum
{
	POS_SUCCESS = 0,
	POS_FAILED,
	NON_POS_FAILED,
} EPosInsertionResult;

//!< saves a square 32 bit TGA texture
const bool SaveTGA32(const char* cpFilename, const uint32* cpBuffer, const uint32 cSize);

//!< round function for a particular type, preserves the value if float or double is invoked (partial template specification)
template<typename T>
const T Round(const double cInput, SType2Type<T> );

//!< type independent 2D vector
template<class TCompType>
struct SVec2
{
	TCompType x, y;

	SVec2(const TCompType cX = 0, const TCompType cY = 0);
	template<class TCastCompType>
	const float GetDistSq(const SVec2<TCastCompType>& crPos) const;
	template<class TCastCompType>
	const bool  operator!=(const SVec2<TCastCompType>& crPos) const;
	const float GetDist(const SVec2<TCompType>& crPos) const;
	const bool  operator==(const SVec2<TCompType>& crPos) const;
	const float Dot(const SVec2<TCompType>& crPos) const;
	template<class TCastCompType>
	operator SVec2<TCastCompType>() const;
	SVec2<TCompType> operator*(const TCompType& crScale) const;

	void             SwapEndianess();
};

template<class TCompType>
const SVec2<TCompType> operator-(const SVec2<TCompType>& crPos0, const SVec2<TCompType>& crPos1);

template<class TCompType>
const SVec2<TCompType> operator+(const SVec2<TCompType>& crPos0, const SVec2<TCompType>& crPos1);

template<class TPosType>    //!< types the center type according to the position type to use float always but in the case of pos double's
struct SCenterType
{
	typedef float Type;
};

template<>
struct SCenterType<double>
{
	typedef double Type;
};

//!< matrix for 2d orientation of quadtree
struct S2DMatrix
{
	float m00, m01, m10, m11;

	S2DMatrix();
	S2DMatrix(const S2DMatrix& crCopyFrom);
	S2DMatrix(const float cM00, const float cM01, const float cM10, const float cM11);
	template<class TCompType>
	const SVec2<TCompType> RotateVec2(const SVec2<TCompType>& crPos) const;
	template<class TCompType>
	const SVec2<TCompType> RotateVec2Inv(const SVec2<TCompType>& crPos) const;

	void                   SwapEndianess();
};

//!< holds the child indices a leaf will go to, can be multiple if using radius
template<bool TUseRadius, uint32 TMaxCellElems, class TIndexType>
struct SChildIndices
{
	TEChildIndex indices[TUseRadius ? 4 : 1]; //!< static array with child indices, if no radius is used, it can only go to one quad cell index
	uint32       count;                       //!< current number of stored indices (always reset and started from index 0)
	//!< initialized to false, indicates whether this leaf has already been reinserted again, important for cloning if to insert a leaf multiple times as for TUseRadius
	bool         reinserted;

	SChildIndices();          //!< ctor setting count to 0 for radius usage, to 1 otherwise
	void ResetForRecursion(); //!< resets for next usage, leaves reinserted unchanged however
	void CompleteReset();     //!< completely resets it, used to be able to use a static array rather then lots of reallocation
};

//!< selects an array size according to the radius usage
template<bool TAllCells, class Type>
struct SArraySelector
{
	Type elems[TAllCells ? 4 : 1]; //!< 4 elems (one for each cell if requested, otherwise 1)
};

//!< encapsulates the container type for content retrieval
template<class TContentSort, class TIndexType>
class CSearchContainer
{
public:
	//!< constructor reserving some memory too
	explicit CSearchContainer
	(
	  std::vector<typename TContentSort::TRetrievalType>& rContentVec,
	  const TIndexType cExpectedElements,
	  const bool
	);
	explicit CSearchContainer
	(
	  std::vector<typename TContentSort::TRetrievalType>& rContentVec,
	  const float cRadius,
	  const bool cSort
	);
	//!< fills the vector with all gathered elements in sorted order
	//!< performs binary search
	void       FillVector(const TIndexType&);
	void       FillVector(const float);
	//!< adds an element
	void       Insert(const TContentSort& crElementToAdd, const float cCompareValue);
	//!< returns true if contents is to be added or not, compares the squared distance but returns always true in case of the number retrieval and not yet enough stored elements
	const bool CheckInsertCriteria(const float cDistSq) const;

private:
	std::vector<TContentSort>                           m_ElemContainer; //!< container type chosen according to TUseVector
	std::vector<float>                                  m_ElemDist[2];   //!< container record for all current distances to compute threshold for new insertion, sorted by bucket sort, 2nd used for copying to not use swap
	float                                               m_CompareValue;  //!< max compare value from last element
	uint32                                              m_ElemCount;     //!< elem count to retrieve, template arg for FillVector takes care of radius too
	std::vector<typename TContentSort::TRetrievalType>& m_rContentVec;   //reference to vector to be filled
	bool m_Sort;                                                         //!< true if to sort elements, only effective in radius case

	const float GetNewCompareValue() const;                       //!< returns new compare value for count based content search
	void        RecordCompareValue(const float cNewCompareValue); //!< records new compare value and resorts the contents
};

//!< performs ray - cell square intersection and stores for each child index the hit result
//!< also returns true if ray hits the cell
//!< done in aligned space after all positions have been rotated
template<class TPosType>
const bool RayCellIntersection
(
  const float cSideLength,                 // cell side length
  const SVec2<TPosType>& crRayPos,         // ray origin
  const SVec2<TPosType>& crRayDir,         // ray direction
  const SVec2<TPosType>& crCenterPos,      // cell center pos
  const float cRayLength,                  //ray length
  bool childHits[4],                       //hit result for children
  const SVec2<TPosType> cChildCenterPos[4] // children cell center pos
);

//!< helper function for RayCellIntersection
//!< adjusts the childHits vec with the result for the behind ray and beyond ray length
template<class TPosType>
void CheckChildCenters
(
  const float cSideLength,                 // cell side length
  const SVec2<TPosType>& crRayPos,         // ray origin
  const SVec2<TPosType>& crRayDir,         // ray direction
  const SVec2<TPosType>& crCenterPos,      // cell center pos
  const float cRayLength,                  //ray length
  bool childHits[4],                       //hit result for children
  const SVec2<TPosType> cChildCenterPos[4] // children cell center pos
);

//!< performs ray - circle intersection test and checks the ray length
//!< returns true if intersection
template<class TPosType, class TLeafPosType>
const bool RayLeafIntersection
(
  const float cRadius,                  // leaf radius
  const SVec2<TPosType>& crRayPos,      // ray origin
  const SVec2<TPosType>& crRayDir,      // ray direction
  const SVec2<TLeafPosType>& crLeafPos, // leaf pos
  const float cRayLength                //ray length
);
}//NQT

#endif//QUADTREE_HELPER

