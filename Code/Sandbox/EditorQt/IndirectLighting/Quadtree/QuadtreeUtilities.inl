// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   quadtree implementation of non quadtree class functions
 */

#include <CryCore/Platform/platform.h>

namespace
{
template<class TUnitType>
inline const TUnitType GetMinSubdivDist(SType2Type<TUnitType> )
{
	return 4;
}

template<>
inline const double GetMinSubdivDist(SType2Type<double> )
{
	return 0.000000000000000004;
}

template<>
inline const float GetMinSubdivDist(SType2Type<float> )
{
	return 0.00000004f;
}

//!< lerps between to colours represented as float vec's
inline const uint32 LerpColour(const float cVal, const Vec3& crFrom, const Vec3& crTo)
{
	const Vec3 cCol = crTo * cVal + crFrom * (1.0 - cVal);
	return ((uint32)cCol.z) | ((uint32)cCol.y << 8) | ((uint32)cCol.x << 16);
}

//!< adds alpha to a colour
inline const uint32 AddAlpha(const uint32 cCol, const float cAlpha)
{
	assert(cAlpha >= 0.f && cAlpha <= 1.f);
	return (cCol | ((uint32)(cAlpha * 255.f) << 24));
}
}

namespace NQT
{
//-------------------------------begin CTravVec-------------------------------------------------------------

template<class T>
inline CTravVec<T>::CTravVec() : m_Count(0)
{}

template<class T>
inline void CTravVec<T >::push_back(const T& crAdd)
{
	assert(m_Count + 1 < scMaxArrayCount);
	::new((void*)(&((T*)m_Mem)[m_Count++]))T(crAdd);
}

template<class T>
inline const uint32 CTravVec<T >::size() const
{
	return m_Count;
}

template<class T>
inline T& CTravVec<T >::operator[](const uint32 cIndex)
{
	assert(cIndex < m_Count);
	return ((T*)m_Mem)[cIndex];
}

template<class T>
inline const T& CTravVec<T >::operator[](const uint32 cIndex) const
{
	assert(cIndex < m_Count);
	return ((T*)m_Mem)[cIndex];
}

template<class T>
inline const bool CTravVec<T >::empty() const
{
	return (m_Count == 0);
}

//-------------------------------end CTravVec-------------------------------------------------------------

inline const bool SaveTGA32(const char* cpFilename, const uint32* cpBuffer, const uint32 cSize)
{
	FILE* pFile = fopen(cpFilename, "wb");
	if (!pFile)  return false;
	uint8 header[18];
	memset(&header, 0, sizeof(header));
	header[2] = 2;
	*(reinterpret_cast<unsigned short*>(&header[12])) = cSize;
	*(reinterpret_cast<unsigned short*>(&header[14])) = cSize;
	header[16] = 0x20;
	header[17] = 0x28;
	fwrite(&header, 1, sizeof(header), pFile);
	fwrite(cpBuffer, 1, cSize * cSize * sizeof(uint32), pFile);
	fclose(pFile);
	return true;
}

template<typename T>
inline const T Round(const double cInput, SType2Type<T> )
{
	if (cInput > 0.0)
	{
		const double cFrac = cInput - floor(cInput);
		return (T)((cFrac < 0.5) ? floor(cInput) : ceil(cInput));
	}
	else
	{
		const double cFrac = cInput - floor(cInput);
		return (T)((cFrac > 0.5) ? ceil(cInput) : floor(cInput));
	}
}

//leave input the same for rounding to float
template<>
inline const float Round(const double cInput, SType2Type<float> )
{
	return cInput;
}

//leave input the same for rounding to double
template<>
inline const double Round(const double cInput, SType2Type<double> )
{
	return cInput;
}

template<class TCompType>
inline SVec2<TCompType>::SVec2(const TCompType cX, const TCompType cY) : x(cX), y(cY)
{}

template<class TCompType>
inline const bool SVec2<TCompType >::operator==(const SVec2<TCompType>& crPos) const
{
	return (crPos.x == x && crPos.y == y);
}

template<>
inline const bool SVec2<float >::operator==(const SVec2<float>& crPos) const
{
	static const float scThreshold = 0.00000001f;
	return (fabs(crPos.x - x) < scThreshold && fabs(crPos.y - y) < scThreshold);
}

template<>
inline const bool SVec2<double >::operator==(const SVec2<double>& crPos) const
{
	static const double scThreshold = 0.000000000000000001f;
	return (fabs(crPos.x - x) < scThreshold && fabs(crPos.y - y) < scThreshold);
}

template<class TCompType>
inline SVec2<TCompType> SVec2<TCompType >::operator*(const TCompType& crScale) const
{
	return SVec2<TCompType>(x * crScale, y * crScale);
}

template<class TCompType>
inline const SVec2<TCompType> operator-(const SVec2<TCompType>& crPos0, const SVec2<TCompType>& crPos1)
{
	return SVec2<TCompType>(crPos0.x - crPos1.x, crPos0.y - crPos1.y);
}

template<class TCompType>
inline const SVec2<TCompType> operator+(const SVec2<TCompType>& crPos0, const SVec2<TCompType>& crPos1)
{
	return SVec2<TCompType>(crPos0.x + crPos1.x, crPos0.y + crPos1.y);
}

template<class TCompType>
template<class TCastCompType>
inline const bool SVec2<TCompType >::operator!=(const SVec2<TCastCompType>& crPos) const
{
	const float cXDiff = (float)x - (float)crPos.x;
	const float cYDiff = (float)y - (float)crPos.y;
	return (cXDiff * cXDiff + cYDiff * cYDiff);
}

template<class TCompType>
template<class TCastCompType>
inline const float SVec2<TCompType >::GetDistSq(const SVec2<TCastCompType>& crPos) const
{
	const float cXDiff = (float)x - (float)crPos.x;
	const float cYDiff = (float)y - (float)crPos.y;
	return (cXDiff * cXDiff + cYDiff * cYDiff);
}

template<class TCompType>
inline void SVec2<TCompType >::SwapEndianess()
{
	SwapEndian(x);
	SwapEndian(y);
}

template<class TCompType>
inline const float SVec2<TCompType >::Dot(const SVec2<TCompType>& crPos) const
{
	return crPos.x * x + crPos.y * y;
}

template<class TCompType>
inline const float SVec2<TCompType >::GetDist(const SVec2<TCompType>& crPos) const
{
	return sqrtf(GetDistSq(crPos));
}

template<class TCompType>
template<class TCastCompType>
inline SVec2<TCompType>::operator SVec2<TCastCompType>() const
{
	return SVec2<TCastCompType>(Round((double)x, SType2Type<TCastCompType>()), Round((double)y, SType2Type<TCastCompType>()));
}

inline S2DMatrix::S2DMatrix()
{
	m00 = m11 = 1.0f;
	m01 = m10 = 0.0f;
}

inline S2DMatrix::S2DMatrix(const S2DMatrix& crCopyFrom)
{
	m00 = crCopyFrom.m00;
	m01 = crCopyFrom.m01;
	m10 = crCopyFrom.m10;
	m11 = crCopyFrom.m11;
}

inline S2DMatrix::S2DMatrix(const float cM00, const float cM01, const float cM10, const float cM11)
{
	m00 = cM00;
	m01 = cM01;
	m10 = cM10;
	m11 = cM11;
}

template<class TCompType>
inline const SVec2<TCompType> S2DMatrix::RotateVec2(const SVec2<TCompType>& crPos) const
{
	const Vec2 crInput((float)crPos.x, (float)crPos.y);
	return SVec2<TCompType>(Round(crInput.x * m00 + crInput.y * m10, SType2Type<TCompType>()), Round(crInput.x * m01 + crInput.y * m11, SType2Type<TCompType>()));
}

template<class TCompType>
inline const SVec2<TCompType> S2DMatrix::RotateVec2Inv(const SVec2<TCompType>& crPos) const
{
	const Vec2 crInput((float)crPos.x, (float)crPos.y);
	Vec2 rotPos;
	rotPos.x = crInput.x * m00 + crInput.y * m01;
	rotPos.y = crInput.x * m10 + crInput.y * m11;
	return SVec2<TCompType>(Round(crInput.x * m00 + crInput.y * m01, SType2Type<TCompType>()), Round(crInput.x * m10 + crInput.y * m11, SType2Type<TCompType>()));
}

inline void S2DMatrix::SwapEndianess()
{
	SwapEndian(m00);
	SwapEndian(m01);
	SwapEndian(m10);
	SwapEndian(m11);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SHeader::SHeader()
	: headerSize(sizeof(typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SHeader))
{}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SHeader::SwapEndianess()
{
	SwapEndian(headerSize);
	SwapEndian(fullStreamSize);
	orientMatrix.SwapEndianess();
	SwapEndian(minSideLength);
	SwapEndian(leafCount);
	SwapEndian(uniqueLeaveCount);
	SwapEndian(nodeCount);
	centerPos.SwapEndianess();
	SwapEndian(sideLength);
	SwapEndian(maxSubDivLevels);
}

//-------------------------CSearchContainer--------------------------------------------------------------------

template<class TContentSort, class TIndexType>
inline CSearchContainer<TContentSort, TIndexType>::CSearchContainer
(
  std::vector<typename TContentSort::TRetrievalType>& rContentVec,
  const TIndexType cExpectedElements,
  const bool   /*no effect*/
)
	: m_CompareValue(std::numeric_limits<float>::max()), m_ElemCount((uint32)cExpectedElements),
	m_rContentVec(rContentVec), m_Sort(true)
{
	m_ElemContainer.reserve(cExpectedElements * 2);   //to avoid repeating allocations
	m_ElemDist[0].resize(cExpectedElements);
	m_ElemDist[1].resize(cExpectedElements);  //copy dest
	for (int i = 0; i < cExpectedElements; ++i)
		m_ElemDist[0][i] = std::numeric_limits<float>::max();  //set initially to float max
}

//important to initialize m_ElemCount with 0, check against count in CheckInsertCriteria
template<class TContentSort, class TIndexType>
inline CSearchContainer<TContentSort, TIndexType>::CSearchContainer
(
  std::vector<typename TContentSort::TRetrievalType>& rContentVec,
  const float cRadius,
  const bool cSort
)
	: m_CompareValue(cRadius * cRadius), m_ElemCount(0), m_rContentVec(rContentVec), m_Sort(cSort)
{
	//radius version
	if (!m_Sort)
		m_ElemContainer.reserve(20);    //to avoid repeating allocations
}

template<class TContentSort, class TIndexType>
inline const float CSearchContainer<TContentSort, TIndexType >::GetNewCompareValue() const
{
	return *(m_ElemDist[0].rbegin());
}

template<class TContentSort, class TIndexType>
void CSearchContainer<TContentSort, TIndexType >::RecordCompareValue(const float cNewCompareValue)
{
	//size needs to be already updated, so cNewCompareValue for the new element is to be inserted
	//keep always sorted
	m_ElemDist[1].swap(m_ElemDist[0]);   //copy to 2nd vector
	const std::vector<float>::const_iterator cEnd = m_ElemDist[0].end();
	std::vector<float>::const_iterator iter1 = m_ElemDist[1].begin();
	std::vector<float>::iterator iter0 = m_ElemDist[0].begin();
	float curVal;
	while ((curVal = *iter1++) < cNewCompareValue)
	{
		//keep first elements in order
		*iter0++ = curVal;
	}
	assert(iter0 != m_ElemDist[0].end()); //didnt fulfill insertion criteria
	*iter0++ = cNewCompareValue;          //copy current element
	while (iter0 != cEnd)
	{
		//add remaining elements in order
		*iter0++ = *iter1++;
	}
}

template<class TContentSort, class TIndexType>
inline void CSearchContainer<TContentSort, TIndexType >::Insert(const TContentSort& crElementToAdd, const float cCompareValue)
{
	assert(CheckInsertCriteria(cCompareValue));
	if (!m_Sort)
		m_rContentVec.push_back(crElementToAdd.contRetrieval);
	else
	{
		m_ElemContainer.push_back(crElementToAdd);
		if (m_ElemCount > 0)
		{
			//non radius version toggled through m_ElemCount > 0 (never the case with radius invocation)
			RecordCompareValue(cCompareValue);
			m_CompareValue = GetNewCompareValue();
		}
	}
}

template<class TContentSort, class TIndexType>
inline const bool CSearchContainer<TContentSort, TIndexType >::CheckInsertCriteria(const float cDistSq) const
{
	return (cDistSq < m_CompareValue);
}

template<class TContentSort, class TIndexType>
inline void CSearchContainer<TContentSort, TIndexType >::FillVector(const TIndexType&)
{
	//fixed elem number invocation
	//perform binary search
	std::sort(m_ElemContainer.begin(), m_ElemContainer.end());
	const typename std::vector<TContentSort>::const_iterator cEnd = m_ElemContainer.begin() + std::min((size_t)m_ElemCount, m_ElemContainer.size());
	for (typename std::vector<TContentSort>::const_iterator iter = m_ElemContainer.begin(); iter != cEnd; ++iter)
		m_rContentVec.push_back(iter->contRetrieval);
}

template<class TContentSort, class TIndexType>
inline void CSearchContainer<TContentSort, TIndexType >::FillVector(const float)
{
	if (!m_Sort)
		return;
	//radius invocation
	//perform binary search
	std::sort(m_ElemContainer.begin(), m_ElemContainer.end());
	const typename std::vector<TContentSort>::const_iterator cEnd = m_ElemContainer.end();
	for (typename std::vector<TContentSort>::const_iterator iter = m_ElemContainer.begin(); iter != cEnd; ++iter)
		m_rContentVec.push_back(iter->contRetrieval);
}

//-------------------------end CSearchContainer--------------------------------------------------------------------

template<class TPosType>
inline void CheckChildCenters
(
  const float cSideLength,
  const SVec2<TPosType>& crRayPos,
  const SVec2<TPosType>& crRayDir,
  const SVec2<TPosType>& crCenterPos,
  const float cRayLength,
  bool childHits[4],
  const SVec2<TPosType> childCenters[4]
)
{
	//check whether a length check is necessary
	const float cHalfSideLength = cSideLength * 0.5f;
	const float cChildHalfSideLength = cSideLength * 0.25f;
	if (crRayPos.GetDist(crCenterPos) > cRayLength) //dist to child cell might be larger than ray length
	{
		for (int i = 0; i < 4; ++i)
		{
			if (childHits[i] == true)
			{
				//verify with pos and ray length
				if (crRayPos.GetDist(childCenters[i]) + 1.415f /*sqrt(2)*/ * cChildHalfSideLength > cRayLength)
					childHits[i] = false;
			}
		}
	}
	//check dot product between ray dir and center->origin vec
	for (int i = 0; i < 4; ++i)
	{
		if (childHits[i] == true)
		{
			//if ray pos does not start in the cell itself, check dot product of ray
			if ((childCenters[i] - crRayPos).Dot(crRayDir) < 0)
				childHits[i] = false;
		}
	}
	//check ray pos start
	const TEChildIndex cChildIndex =
	  ((TPosType)crRayPos.y < crCenterPos.y) ?
	  ((TPosType)crRayPos.x >= crCenterPos.x) ?
	  AS : SS :
	  ((TPosType)crRayPos.x >= crCenterPos.x) ?
	  AA : SA;
	const SVec2<TPosType>& crChildPos = childCenters[cChildIndex];
	if (crRayPos.x > (float)crChildPos.x - cChildHalfSideLength && crRayPos.x < (float)crChildPos.x + cChildHalfSideLength &&
	    crRayPos.y > (float)crChildPos.y - cChildHalfSideLength && crRayPos.y < (float)crChildPos.y + cChildHalfSideLength)
		childHits[cChildIndex] = true;     //ray starts within cell
}

template<class TPosType>
inline const bool RayCellIntersection
(
  const float cSideLength,
  const SVec2<TPosType>& crRayPos,
  const SVec2<TPosType>& crRayDir,
  const SVec2<TPosType>& crCenterPos,
  const float cRayLength,
  bool childHits[4],
  const SVec2<TPosType> cChildCenterPos[4]
)
{
	//positions are suppose to be in non rotated space
	const float cHalfSideLength = 0.5f * cSideLength;
	//check special case with parallel lines
	if (fabs(crRayDir.x) < 0.001f)
	{
		if (fabs(crRayPos.y - crCenterPos.y) < cHalfSideLength)
		{
			if (crRayPos.y < crCenterPos.y)
			{
				childHits[0] = childHits[1] = true;
				childHits[2] = childHits[3] = false;
			}
			else
			{
				childHits[0] = childHits[1] = false;
				childHits[2] = childHits[3] = true;
			}
			CheckChildCenters<TPosType>(cSideLength, crRayPos, crRayDir, crCenterPos, cRayLength, childHits, cChildCenterPos);
			return true;
		}
		else
		{
			childHits[0] = childHits[1] = childHits[2] = childHits[3] = false;
			return false;
		}
	}
	if (fabs(crRayDir.y) < 0.001f)
	{
		if (fabs(crRayPos.x - crCenterPos.x) < cHalfSideLength)
		{
			if (crRayPos.x < crCenterPos.x)
			{
				childHits[0] = childHits[2] = true;
				childHits[1] = childHits[3] = false;
			}
			else
			{
				childHits[0] = childHits[2] = false;
				childHits[1] = childHits[3] = true;
			}
			CheckChildCenters<TPosType>(cSideLength, crRayPos, crRayDir, crCenterPos, cRayLength, childHits, cChildCenterPos);
			return true;
		}
		else
		{
			childHits[0] = childHits[1] = childHits[2] = childHits[3] = false;
			return false;
		}
	}
	//compute intersection of both y and x axis of square and check the intersection point against the square
	//first cache some values
	const float cUYPos = crCenterPos.y - cHalfSideLength;
	const float cLYPos = crCenterPos.y + cHalfSideLength;
	const float cLXPos = crCenterPos.x - cHalfSideLength;
	const float cRXPos = crCenterPos.x + cHalfSideLength;

	const float cInvDY = crRayDir.y / crRayDir.x;   //x gradient units per y
	const float cInvDX = crRayDir.x / crRayDir.y;   //y gradient units per x
	//compute each other intersection dimension value
	const float cX0 = (cUYPos - crRayPos.y) * cInvDX + crRayPos.x;    //subtracted x axis
	const float cX1 = (cLYPos - crRayPos.y) * cInvDX + crRayPos.x;    //added x axis
	const float cY0 = (cLXPos - crRayPos.x) * cInvDY + crRayPos.y;    //subtracted y axis
	const float cY1 = (cRXPos - crRayPos.x) * cInvDY + crRayPos.y;    //added y axis

	uint32 hitCount = 0;
	//determine the 2 quads the ray intersects
	TEChildIndex hitCells[4];  //only first 2 counting, if more are set, all cells are getting marked as hit (too expensive tests otherwise)
	if (fabs(cX0 - crCenterPos.x) <= cHalfSideLength)
		hitCells[hitCount++] = (cX0 < crCenterPos.x) ? SS : AS; //subtracted x axis
	if (fabs(cX1 - crCenterPos.x) <= cHalfSideLength)
		hitCells[hitCount++] = (cX1 < crCenterPos.x) ? SA : AA; //added x axis
	if (fabs(cY0 - crCenterPos.y) <= cHalfSideLength)
		hitCells[hitCount++] = (cY0 < crCenterPos.y) ? SS : SA; //subtracted y axis
	if (fabs(cY1 - crCenterPos.y) <= cHalfSideLength)
		hitCells[hitCount++] = (cY1 < crCenterPos.y) ? AS : AA; //added y axis
	if (hitCount > 2)                                         //corner has been hit, mark as all cells have been hit conservatively
	{
		childHits[0] = childHits[1] = childHits[2] = childHits[3] = true;
		CheckChildCenters<TPosType>(cSideLength, crRayPos, crRayDir, crCenterPos, cRayLength, childHits, cChildCenterPos);
		return true;
	}
	if (hitCount == 0) //possible as from a child node of a conservative test above
	{
		childHits[0] = childHits[1] = childHits[2] = childHits[3] = false;
		return false;
	}
	//determine intersection of ray with middle point constraints for special cases
	//where ray only intersects either both horizontal or both vertical axis
	const float cMiddleX = (crCenterPos.y - crRayPos.y) * cInvDX + crRayPos.x;
	const float cMiddleY = (crCenterPos.x - crRayPos.x) * cInvDY + crRayPos.y;

	const bool cMiddleXS = (fabs(cMiddleX - crCenterPos.x) <= cHalfSideLength && cMiddleX < crCenterPos.x);   //middle pos ray intersects subtracted
	const bool cMiddleXA = (fabs(cMiddleX - crCenterPos.x) <= cHalfSideLength && cMiddleX >= crCenterPos.x);  //middle pos ray intersects added
	const bool cMiddleYS = (fabs(cMiddleY - crCenterPos.y) <= cHalfSideLength && cMiddleY < crCenterPos.y);   //middle pos ray intersects subtracted
	const bool cMiddleYA = (fabs(cMiddleY - crCenterPos.y) <= cHalfSideLength && cMiddleY >= crCenterPos.y);  //middle pos ray intersects added

	childHits[SS] = (hitCells[0] == SS || hitCells[1] == SS || cMiddleXS || cMiddleYS);
	childHits[AS] = (hitCells[0] == AS || hitCells[1] == AS || cMiddleXA || cMiddleYS);
	childHits[SA] = (hitCells[0] == SA || hitCells[1] == SA || cMiddleXS || cMiddleYA);
	childHits[AA] = (hitCells[0] == AA || hitCells[1] == AA || cMiddleXA || cMiddleYA);

	CheckChildCenters<TPosType>(cSideLength, crRayPos, crRayDir, crCenterPos, cRayLength, childHits, cChildCenterPos);
	return (hitCount > 0);
}

template<class TPosType, class TLeafPosType>
inline const bool RayLeafIntersection
(
  const float cRadius,
  const SVec2<TPosType>& crRayPos,
  const SVec2<TPosType>& crRayDir,
  const SVec2<TLeafPosType>& crLeafPos,
  const float cRayLength
)
{
	const SVec2<TPosType> cLeafPos = (SVec2<typename SCenterType<TPosType>::Type> )crLeafPos;
	const float cRayCenterDist = crRayPos.GetDist(cLeafPos);
	//check if pos is inside this radius
	if (cRayCenterDist < cRadius)
		return true;  //inside bounding circle
	//check against ray length
	if (cRayCenterDist - cRadius > cRayLength)
		return false;  //ray cant reach leaf circle
	//check that leaf is not behind ray origin
	if ((cLeafPos - crRayPos).Dot(crRayDir) < 0)
		return false;  //behind ray origin
	//check ray actually intersects circle around leaf pos
	//center circle around 0,0
	const SVec2<TPosType> cRayOrigin = crRayPos - cLeafPos;
	const SVec2<TPosType> cRayEnd = cRayOrigin + crRayDir * cRayLength;
	const float cDet = cRayOrigin.x * cRayEnd.y - cRayEnd.x * cRayOrigin.y;
	if (cRadius * cRadius * cRayLength * cRayLength - cDet * cDet <= 0)
		return false;  //does not have two intersection points
	return true;
}

template<bool TUseRadius, uint32 TMaxCellElems, class TIndexType>
inline SChildIndices<TUseRadius, TMaxCellElems, TIndexType>::SChildIndices() : count(TUseRadius ? 0 : 1), reinserted(false)
{}

template<bool TUseRadius, uint32 TMaxCellElems, class TIndexType>
inline void SChildIndices<TUseRadius, TMaxCellElems, TIndexType >::ResetForRecursion()
{
	count = TUseRadius ? 0 : 1;
}

template<bool TUseRadius, uint32 TMaxCellElems, class TIndexType>
inline void SChildIndices<TUseRadius, TMaxCellElems, TIndexType >::CompleteReset()
{
	ResetForRecursion();
	reinserted = false;
}
};//NQT

