// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainObjectMan.h"
#include <limits>

const uint8 CTerrainObjectMan::scObstructed = 1;
const uint8 CTerrainObjectMan::scNotObstructed = 0;
const uint8 CTerrainObjectMan::scObstructedForOffset = 2;
const float CTerrainObjectMan::scFullObstructionMargin = 0.5f;

void CObstructionAccessManager::GetAccessIndices(uint32& rBasisIndex, uint32& rOffsetIndex, const uint32 cRes, const uint16 cX, const uint16 cY) const
{
	const float cConvFactor = m_ObstructionMapRes / cRes;
	const uint16 cConvertedX = (uint16)((float)cX * cConvFactor);
	const uint16 cConvertedY = (uint16)((float)cY * cConvFactor);
	const uint32 cIndexX = (cConvertedX >> scSectorSizeShift);
	const uint32 cIndexY = (cConvertedY >> scSectorSizeShift);
	const uint32 cNumElemsRow = (m_ObstructionMapRes >> scSectorSizeShift);//elems per row
	rBasisIndex = (cIndexX * cNumElemsRow) + cIndexY;//index into sector
	assert(rBasisIndex < cNumElemsRow * cNumElemsRow);
	rOffsetIndex = ((cConvertedX & (scSectorSize - 1)) << scSectorSizeShift) + (cConvertedY & (scSectorSize - 1));
	assert(rOffsetIndex < scSectorSize * scSectorSize);
}

void CObstructionAccessManager::GetAccessIndicesHeightLow(uint32& rBasisIndex, uint32& rOffsetIndex, const uint32 cRes, const uint16 cX, const uint16 cY) const
{
	const float cConvFactor = m_HeightObstrGridResolution / cRes;
	const uint16 cConvertedX = (uint16)((float)cX * cConvFactor);
	const uint16 cConvertedY = (uint16)((float)cY * cConvFactor);
	rBasisIndex = cConvertedY / m_YPartMappingHeightLow;
	assert(rBasisIndex < m_AllocPartsHeightLow);
	const uint32 cOffsetYIndex = cConvertedY - (rBasisIndex * m_YPartMappingHeightLow);
	rOffsetIndex = m_HeightObstrGridResolution * cOffsetYIndex + cConvertedX;
	assert(rOffsetIndex < (m_HeightObstrGridResolution * m_HeightObstrGridResolution / m_AllocPartsHeightLow));
}

void CObstructionAccessManager::AllocateAndInit(const uint32 cRes)
{
	//perform allocation in 4 MB parts (related to uint32)
	//treat every single vector on its own
	static const uint32 cSingleAllocationSize = 4096 * 1024;
	m_ObstructionMapRes = cRes;

	//first sector stuff
	const uint32 cNumElemsRow = (m_ObstructionMapRes >> scSectorSizeShift);//elems per row
	assert((cNumElemsRow << scSectorSizeShift) == m_ObstructionMapRes);
	m_ObjectIndices.resize(cNumElemsRow * cNumElemsRow);

	//now obstruction height
	m_HeightObstrGridResolution = cRes >> 1;//lower res for height
	if (cSingleAllocationSize >= m_HeightObstrGridResolution * m_HeightObstrGridResolution * sizeof(uint8))
		m_AllocPartsHeightLow = 1;
	else
		m_AllocPartsHeightLow = m_HeightObstrGridResolution * m_HeightObstrGridResolution * sizeof(uint8) / cSingleAllocationSize;
	m_YPartMappingHeightLow = m_HeightObstrGridResolution / m_AllocPartsHeightLow;
	assert(m_YPartMappingHeightLow * m_AllocPartsHeightLow == m_HeightObstrGridResolution);
	const uint32 cSingleCountHeightLow = (m_HeightObstrGridResolution * m_HeightObstrGridResolution) / m_AllocPartsHeightLow;
	assert(cSingleCountHeightLow * m_AllocPartsHeightLow == (m_HeightObstrGridResolution * m_HeightObstrGridResolution));//must be a multiple of it
	assert(sizeof(uint8) * cSingleCountHeightLow <= cSingleAllocationSize);//check single allocation size
	m_ObstructionHeights.resize(m_AllocPartsHeightLow);
	//init data
	for (int i = 0; i < m_AllocPartsHeightLow; ++i)
	{
		m_ObstructionHeights[i].resize(cSingleCountHeightLow);
		const std::vector<uint8>::const_iterator cObstrEnd = m_ObstructionHeights[i].end();
		for (std::vector<uint8>::iterator iter = m_ObstructionHeights[i].begin(); iter != cObstrEnd; ++iter)
			*iter = 0;
	}
}

const bool CTerrainObjectMan::CalcRayIntersection
(
  const bool cIsTerrainOccl,
  const Vec3& crOrigin,
  const Vec2& crDir,
  const bool cInsideReturnsHit
)
{
	//construct m_RayIntersectionData for queries to RayIntersection
	m_RayIntersectionObjects.resize(0);
	Vec2 minMaxAngles;
	//iterate all bounding boxes and check if the ray actually hits it, also compute min and max angle plus store colour
	const std::vector<const TContentType*>::const_iterator cEnd = m_LastResults.end();
	for (std::vector<const TContentType*>::const_iterator iter = m_LastResults.begin(); iter != cEnd; ++iter)
	{
		assert(**iter < (TContentType)m_Objects.size());
		const TTerrainObject& crObj = m_Objects[**iter];
		const bool cIsVeg = crObj.IsVeg();
		const uint32 cRCObjID = crObj.GetID();
		const uint32 cMaxCount = crObj.bboxCol.size();
		for (int i = 0; i < cMaxCount; ++i)
		{
			const AABB& crBox = crObj.bboxCol[i].bbox;
			const bool cAboveObject = (crOrigin.z >= crBox.max.z);
			if (cAboveObject)
				continue;//we do not consider objects below horizon
			SAngleBox angleBox;
			angleBox.SetIsVeg(cIsVeg);
			angleBox.pBBoxRaster = crObj.bboxCol[i].pBBoxRaster;
			const Vec3 cSize = crBox.max - crBox.min;
			const float cMaxSize = std::max(cSize.x, std::max(cSize.y, cSize.z));
			angleBox.stepWidthRasterTable = 1.3f + (5.f - 1.3f) * std::max(0.f, 1.f - cMaxSize / 8.0f);
			angleBox.col = ColorF(crObj.bboxCol[i].col);
			angleBox.alphaMul = 1.f - angleBox.col.a;
			angleBox.SetAlphaLTZ(angleBox.col.a < 1.f);
			//first check if it lies inside the box and if so above or underneath
			uint8 flags;
			flags = (crOrigin.x > crBox.min.x) << 0;
			flags |= (crOrigin.x < crBox.max.x) << 1;
			flags |= (crOrigin.y > crBox.min.y) << 2;
			flags |= (crOrigin.y < crBox.max.y) << 3;
			const bool cInsideBox = (flags == 0xF);

			bool breakCond = false;
			if (!angleBox.pBBoxRaster)
			{
				const bool cBelowObject = (crOrigin.z <= crBox.min.z);
				if (cInsideBox && (cBelowObject /* || cAboveObject*/))
				{
					breakCond = true;
					//ray test query at box center height
					//we get hit point if sample does not lie under or above the object
					//sample above or underneath the box
					//calculate the 2 lines the ray intersects
					Vec2 intersectionLines;//lines where to check for the intersection
					intersectionLines.x = (crDir.x <= 0) ? crBox.min.x : crBox.max.x;
					intersectionLines.y = (crDir.y <= 0) ? crBox.min.y : crBox.max.y;
					Vec2 xyIntersection;
					if (fabs(crDir.y) < 0.05f)
					{
						xyIntersection.x = crOrigin.x;
						xyIntersection.y = intersectionLines.y;
					}
					else if (fabs(crDir.x) < 0.05f)
					{
						xyIntersection.x = intersectionLines.x;
						xyIntersection.y = crOrigin.y;
					}
					else
					{
						Vec2 lineIntersections; //other coordinate at each intersection line
						lineIntersections.x = crOrigin.x + crDir.x * (intersectionLines.y - crOrigin.y) / crDir.y;//x coord for y line intersection
						lineIntersections.y = crOrigin.y + crDir.y * (intersectionLines.x - crOrigin.x) / crDir.x;//y coord for x line intersection
						if (lineIntersections.x <= crBox.max.x && lineIntersections.x >= crBox.min.x)//inside y extension of box
						{
							xyIntersection.x = lineIntersections.x;
							xyIntersection.y = intersectionLines.y;
						}
						else
						{
							xyIntersection.x = intersectionLines.x;
							xyIntersection.y = lineIntersections.y;
						}
					}
					//					if(cBelowObject)//below object
					{
						angleBox.angleMax = 1.f;
						Vec3 intersection(xyIntersection.x, xyIntersection.y, crBox.min.z);
						intersection -= crOrigin;
						intersection.Normalize();
						angleBox.angleMin = intersection.z;
						m_RayIntersectionObjects.push_back(angleBox);
						continue;
					}
					/*					else//above object
					          {
					            angleBox.angleMin = -1.f;
					            Vec3 intersection(xyIntersection.x, xyIntersection.y, crBox.max.z);
					            intersection -= crOrigin;
					            intersection.Normalize();
					            angleBox.angleMax = std::min(-0.1f, intersection.z);//cannot go above horizon
					          }
					 */
				}
			}
			if (!breakCond)
			{
				const bool cInsideEnter = cInsideBox;
				if (cInsideEnter)
				{
					angleBox.angleMin = -1.f;
					angleBox.angleMax = 1.f;
					m_RayIntersectionObjects.push_back(angleBox);
					continue;
				}
				else if (RayAABBNoZ(crOrigin, crDir, crBox, minMaxAngles, cInsideBox, false))
				{
					angleBox.angleMin = minMaxAngles.x;
					angleBox.angleMax = minMaxAngles.y;
				}
				else
					continue;
				angleBox.angleMin = std::min(1.f, angleBox.angleMin);
				angleBox.angleMax = std::min(1.f, std::max(angleBox.angleMax, angleBox.angleMin));//avoid rounding errors
				m_RayIntersectionObjects.push_back(angleBox);
			}
		}
	}
	m_RayIntersectionOrigin = crOrigin;
	m_RayIntersectionDir = crDir;

	return !m_RayIntersectionObjects.empty();
}

const bool CTerrainObjectMan::RayIntersection(ColorF& rCollisionColour, const float cHeight) const
{
	//call wasts time
	//	if(m_RayIntersectionObjects.empty())
	//		return false;
	//now test in ascending order
	const std::vector<SAngleBox>::const_iterator cEnd = m_RayIntersectionObjects.end();
	bool foundHit = false;
	float alphaVis = 0.f;
	//calculate ray direction
	Vec3 dir(0.f, 0.f, cHeight);
	const float cScaleXY = sqrtf(1.f - cHeight * cHeight) /
	                       sqrtf(m_RayIntersectionDir.x * m_RayIntersectionDir.x + m_RayIntersectionDir.y * m_RayIntersectionDir.y);
	dir.x = m_RayIntersectionDir.x * cScaleXY;
	dir.y = m_RayIntersectionDir.y * cScaleXY;
	assert(abs(dir.len() - 1.f) < 0.001f);//check if normalized
	for (std::vector<SAngleBox>::const_iterator iter = m_RayIntersectionObjects.begin(); iter != cEnd; ++iter)
	{
		const SAngleBox& crAngleBox = *iter;
		if (cHeight >= crAngleBox.angleMin && cHeight <= crAngleBox.angleMax)
		{
			const ColorF& crCol = crAngleBox.col;
			if (crCol.a <= 0.05f)
				continue;

			//perform ray triangle intersection test
			if (crAngleBox.pBBoxRaster && !crAngleBox.pBBoxRaster->RayTriangleIntersection
			    (
			      m_RayIntersectionOrigin,
			      dir,
			      crAngleBox.HasAlphaLTZ(),//offset if inside
			      crAngleBox.stepWidthRasterTable
			    ))
				continue;
			if (crAngleBox.HasAlphaLTZ())//we have an alpha hit, continue then, .a denotes amount of opacity, 0->transparent, 1 fully opaque
			{
				//amount of opacity, if we have already a hit, add the remaining transparent amount times material alpha
				alphaVis = alphaVis + (1.f - alphaVis) * crCol.a;
				if (foundHit)
				{
					//multiply with previous hit
					rCollisionColour.r = crCol.a * rCollisionColour.r * crCol.r + crAngleBox.alphaMul * rCollisionColour.r;
					rCollisionColour.g = crCol.a * rCollisionColour.g * crCol.g + crAngleBox.alphaMul * rCollisionColour.g;
					rCollisionColour.b = crCol.a * rCollisionColour.b * crCol.b + crAngleBox.alphaMul * rCollisionColour.b;
				}
				else
				{
					rCollisionColour.r = crCol.r * crCol.a + crAngleBox.alphaMul;
					rCollisionColour.g = crCol.g * crCol.a + crAngleBox.alphaMul;
					rCollisionColour.b = crCol.b * crCol.a + crAngleBox.alphaMul;
				}
				foundHit = true;
				continue;
			}
			else
				alphaVis = 1.f;
			rCollisionColour = crCol;
			rCollisionColour.a = alphaVis;
			return true;
		}
	}
	rCollisionColour.a = alphaVis;
	return foundHit;
}

void CTerrainObjectMan::GetMinMaxAngle(const Vec3& crOrigin, const Vec2& crHitPoint, Vec2& rMinMaxAngle, const AABB& crAABB, const bool cXAxisAdjustable) const
{
	//calculate min and max angle of bounding box projection onto unit sphere around origin
	//get shortest point of intersection into AABB quad in xy-plane, use hitPoint if not inside the xy-cell
	Vec3 min0, max0, min1, max1;
	if (cXAxisAdjustable)
	{
		min0 = Vec3(crAABB.min.x, crHitPoint.y, crAABB.min.z);
		min0 -= crOrigin;
		min1 = Vec3(crAABB.max.x, crHitPoint.y, crAABB.min.z);
		min1 -= crOrigin;
		max0 = Vec3(crAABB.min.x, crHitPoint.y, crAABB.max.z);
		max0 -= crOrigin;
		max1 = Vec3(crAABB.max.x, crHitPoint.y, crAABB.max.z);
		max1 -= crOrigin;
	}
	else
	{
		min0 = Vec3(crHitPoint.x, crAABB.min.y, crAABB.min.z);
		min0 -= crOrigin;
		min1 = Vec3(crHitPoint.x, crAABB.max.y, crAABB.min.z);
		min1 -= crOrigin;
		max0 = Vec3(crHitPoint.x, crAABB.min.y, crAABB.max.z);
		max0 -= crOrigin;
		max1 = Vec3(crHitPoint.x, crAABB.max.y, crAABB.max.z);
		max1 -= crOrigin;
	}
	min0.Normalize();
	min1.Normalize();
	max0.Normalize();
	max1.Normalize();
	rMinMaxAngle.x = std::min(min0.z, min1.z);
	rMinMaxAngle.y = std::max(max0.z, max1.z);
}

const bool CTerrainObjectMan::RayAABBNoZ(const Vec3& crOrigin, const Vec2& crDir, const AABB& crAABB, Vec2& rOutput, const bool cInsideBox, const bool cRecursed) const
{
	//i=0
	if ((crDir[0] > 0) && (crOrigin[0] < crAABB.min[0]))
	{
		const float cCos = (-crOrigin[0] + crAABB.min[0]) / crDir[0];
		const Vec2 cHitPoint(crAABB.min[0], crOrigin[1] + (crDir[1] * cCos));
		if ((cHitPoint[1] > crAABB.min[1]) && (cHitPoint[1] < crAABB.max[1]))
		{
			GetMinMaxAngle(crOrigin, cHitPoint, rOutput, crAABB, true);
			return true;
		}
	}
	if ((crDir[0] < 0) && (crOrigin[0] > crAABB.max[0]))
	{
		const float cCos = (crOrigin[0] - crAABB.max[0]) / crDir[0];
		const Vec2 cHitPoint(crAABB.max[0], crOrigin[1] - (crDir[1] * cCos));
		if ((cHitPoint[1] > crAABB.min[1]) && (cHitPoint[1] < crAABB.max[1]))
		{
			GetMinMaxAngle(crOrigin, cHitPoint, rOutput, crAABB, true);
			return true;
		}
	}
	//i=1
	if ((crDir[1] > 0) && (crOrigin[1] < crAABB.min[1]))
	{
		const float cCos = (-crOrigin[1] + crAABB.min[1]) / crDir[1];
		const Vec2 cHitPoint(crOrigin[0] + (crDir[0] * cCos), crAABB.min[1]);
		if ((cHitPoint[0] > crAABB.min[0]) && (cHitPoint[0] < crAABB.max[0]))
		{
			GetMinMaxAngle(crOrigin, cHitPoint, rOutput, crAABB, false);
			return true;
		}
	}
	if ((crDir[1] < 0) && (crOrigin[1] > crAABB.max[1]))
	{
		const float cCos = (crOrigin[1] - crAABB.max[1]) / crDir[1];
		const Vec2 cHitPoint(crOrigin[0] - (crDir[0] * cCos), crAABB.max[1]);
		if ((cHitPoint[0] > crAABB.min[0]) && (cHitPoint[0] < crAABB.max[0]))
		{
			GetMinMaxAngle(crOrigin, cHitPoint, rOutput, crAABB, false);
			return true;
		}
	}
	return false;//no intersection
}

const bool CTerrainObjectMan::AddObject
(
  const float cHeight,
  TTempTerrainObject& rObj,
  uint32& rID,
  const bool cIsVegetation,
  const bool cOffsetSample
)
{
	rID = 0xFFFFFFFF;
	static bool errorOutput = false;
	static const uint32 scMaxElems = std::min((uint32)((2 << (sizeof(TContentType) * 8 - 1)) - (TContentType)1), (uint32)m_QuadTree.GetLeafCapacity());
	assert(rObj.bboxCol.size() > 0);
	assert(m_TempObjects.size() < scMaxElems);
	if (m_TempObjects.size() >= scMaxElems)
	{
		if (!errorOutput)
		{
			GetIEditorImpl()->GetSystem()->GetILog()->Log("Error: More than %d objects active on map for ray casting, calculation cancelled", scMaxElems);
			CLogFile::WriteLine("Error: Too many objects active on map for ray casting, calculation cancelled");
		}
		errorOutput = true;
		return false;
	}
	//project radius, get xy extension, pay attention that pos is set to an integer -> adjust radius accordingly
	float radius = 0.f;
	for (int i = 0; i < rObj.bboxCol.size(); ++i)
	{
		const float cX = rObj.bboxCol[i].bbox.min.x - rObj.bboxCol[i].bbox.max.x;
		const float cY = rObj.bboxCol[i].bbox.min.y - rObj.bboxCol[i].bbox.max.y;
		float curRadius = sqrtf(cX * cX + cY * cY) * 0.5;
		float centerX = (rObj.bboxCol[i].bbox.min.x + rObj.bboxCol[i].bbox.max.x) * 0.5f;
		float centerY = (rObj.bboxCol[i].bbox.min.y + rObj.bboxCol[i].bbox.max.y) * 0.5f;
		centerX -= (float)(uint32)centerX;
		centerY -= (float)(uint32)centerY;
		curRadius += sqrtf(centerX * centerX + centerY * centerY);
		radius = std::max(radius, curRadius);
	}
	//bias radius a bit
	radius += 0.3f;

	const float cMinHeightForRC = 1.4f;
	const float cMaxRadius = 30.f;
	const float cMinRadius = 2.f;

	//do not check height, planes should stay supported, vegetation do not get here anyway
	if ((cHeight > cMinHeightForRC || radius > cMinRadius) && radius < cMaxRadius)
	{
		const NQT::EInsertionResult cRes = m_QuadTree.InsertLeaf(TVec2(rObj.posX, rObj.posY), (TContentType)m_TempObjects.size(), radius);
		if (NQT::POS_EQUAL_TO_EXISTING_LEAF == cRes)
		{
			//objects are at the same position, add object as sub material
			//do not add to quadtree but add object itself
			const uint32* pIndex;
			m_QuadTree.GetLeafByPos(pIndex, TVec2(rObj.posX, rObj.posY));
			if (*pIndex < m_TempObjects.size())
			{
				//add sub material
				TTempTerrainObject& rExistObj = m_TempObjects[*pIndex];
				const std::vector<STempBBoxData>::const_iterator cTempBoxEnd = rObj.bboxCol.end();
				for (std::vector<STempBBoxData>::const_iterator iter = rObj.bboxCol.begin(); iter != cTempBoxEnd; ++iter)
					rExistObj.bboxCol.push_back(*iter);
			}
		}
		else if (NQT::NOT_ENOUGH_SUBDIV_LEVELS == cRes)//use closest object and attach it there
		{
			//do not add to quadtree but add object itself
			std::vector<TQuadTree::TContentRetrievalType> results;
			m_QuadTree.GetClosestContents(TVec2(rObj.posX, rObj.posY), (uint32)1, results);
			if (results.empty() || *(results[0].first) >= m_TempObjects.size())
				return false;
			//add sub material
			TTempTerrainObject& rExistObj = m_TempObjects[*(results[0].first)];
			const std::vector<STempBBoxData>::const_iterator cTempBoxEnd = rObj.bboxCol.end();
			for (std::vector<STempBBoxData>::const_iterator iter = rObj.bboxCol.begin(); iter != cTempBoxEnd; ++iter)
				rExistObj.bboxCol.push_back(*iter);
		}
		else if (NQT::SUCCESS != cRes)
			return false;
	}
	rID = m_TempObjects.size();
	rObj.SetID(rID);
	rObj.SetOffsetted(cOffsetSample);
	rObj.SetVeg(cIsVegetation);
	rObj.radius = radius;
	m_TempObjects.push_back(rObj);
	return true;
}

struct STempBBoxDataSorter
{
	bool operator()(const STempBBoxData& cr1, const STempBBoxData& cr2) const
	{
		return (cr1.bbox.GetSize().GetLengthSquared() < cr2.bbox.GetSize().GetLengthSquared());
	}
};

void CTerrainObjectMan::ConvertObject(TTempTerrainObject& rObj)
{
	STempBBoxDataSorter sorter;
	TTerrainObject obj;
	obj.posX = rObj.posX;
	obj.posY = rObj.posY;
	obj.radius = rObj.radius;
	obj.id = rObj.id;

	//also sort from small to large so that multiple material objects can also be hit at the small materials (bounding box always around everything)
	std::sort(rObj.bboxCol.begin(), rObj.bboxCol.end(), sorter);

	obj.bboxCol.reserve(rObj.bboxCol.size());

	const std::vector<STempBBoxData>::iterator cEnd = rObj.bboxCol.end();
	for (std::vector<STempBBoxData>::iterator iter = rObj.bboxCol.begin(); iter != cEnd; ++iter)
	{
		STempBBoxData& rBox = *iter;
		SBBoxData newBox(rBox.bbox, rBox.col, rBox.pBBoxRaster, rBox.onGround);
		obj.bboxCol.push_back(newBox);
	}

	m_Objects.push_back(obj);
}

const bool CTerrainObjectMan::ConvertAndGenObstructionMap
(
  const uint32 cGridResolution,
  std::vector<uint8>& rTerrainMapMarks,
  const unsigned int cTerrainMapRes,
  const unsigned int cUpdateRadius
)
{
	assert(m_Objects.empty());
	assert(cGridResolution >= 2);
	m_ObstrMan.AllocateAndInit(cGridResolution);

	//world convert factors to cGridResolution
	assert(m_cWidth >= cGridResolution);
	const uint32 cWorldFactor = m_cWidth / cGridResolution;
	const float cWorldFactorInv = 1.f / (float)cWorldFactor;
	uint32 worldFactorShift = 0;
	while ((cGridResolution << worldFactorShift) != m_cWidth)
		++worldFactorShift;
	assert((cGridResolution << worldFactorShift) == m_cWidth);

	//world convert factors to cTerrainMapRes
	const uint32 cWorldMapMarksFactor = m_cWidth / cTerrainMapRes;
	const float cWorldMapMarksFactorInv = 1.f / (float)cWorldMapMarksFactor;
	uint32 worldMapMarksFactorShift = 0;
	while ((cTerrainMapRes << worldMapMarksFactorShift) != m_cWidth)
		++worldMapMarksFactorShift;
	assert((cTerrainMapRes << worldMapMarksFactorShift) == m_cWidth);
	const uint32 cHeightWorldFactor = m_cWidth / m_ObstrMan.GetObstructionMapHeightRes();
	uint32 index = 0;

	m_Objects.reserve(m_TempObjects.size());

	const std::vector<TTempTerrainObject>::iterator cObjectEnd = m_TempObjects.end();
	for (std::vector<TTempTerrainObject>::iterator objectIter = m_TempObjects.begin(); objectIter != cObjectEnd; ++objectIter)
	{
		TTempTerrainObject& rObj = *objectIter;
		ConvertObject(rObj);//convert this object and add to m_Objects
		for (int i = 0; i < rObj.bboxCol.size(); ++i)
		{
			const STempBBoxData& crBoxData = rObj.bboxCol[i];
			const AABB& crBBox = crBoxData.bbox;
			//get bbox from raster
			AABB xyBBox(crBBox);
			if (crBoxData.pBBoxRaster)
				crBoxData.pBBoxRaster->RetrieveXYBBox(xyBBox, 0.f, true);
			//determine x and y coords to check on the grid
			uint32 startX = ((uint32)std::max(0.f, xyBBox.min.x) >> worldFactorShift) << worldFactorShift;
			uint32 startY = ((uint32)std::max(0.f, xyBBox.min.y) >> worldFactorShift) << worldFactorShift;
			uint32 endX = std::max((uint32)0, std::min(m_cWidth - 1, (((uint32)xyBBox.max.x + (cWorldFactor - 1)) >> worldFactorShift) << worldFactorShift));
			uint32 endY = std::max((uint32)0, std::min(m_cWidth - 1, (((uint32)xyBBox.max.y + (cWorldFactor - 1)) >> worldFactorShift) << worldFactorShift));
			if (crBoxData.onGround)
			{
				for (uint32 x = startX; x <= endX; x += cWorldFactor)
					for (uint32 y = startY; y <= endY; y += cWorldFactor)
					{
						//mark as obstructed, ray starts inside bbox
						float obstruction = 1.f;
						//0.5f weighting each for x and y
						if ((float)x < xyBBox.min.x)
							obstruction = ((cWorldFactor - (xyBBox.min.x - (float)x)) * cWorldFactorInv);
						else if ((float)x > xyBBox.max.x)
							obstruction = ((cWorldFactor - ((float)x - xyBBox.max.x)) * cWorldFactorInv);
						obstruction *= 0.5f;
						if ((float)y < xyBBox.min.y)
							obstruction += ((cWorldFactor - (xyBBox.min.y - (float)y)) * cWorldFactorInv) * 0.5f;
						else if ((float)y > xyBBox.max.y)
							obstruction += ((cWorldFactor - ((float)y - xyBBox.max.y)) * cWorldFactorInv) * 0.5f;
						else
							obstruction += 0.5f;
						assert(obstruction <= 1.f);
						//WORLD_HMAP_COORDINATE_EXCHANGE
						//only set if really inside box
						if (x >= xyBBox.min.x && x <= xyBBox.max.x && y >= xyBBox.min.y && y <= xyBBox.max.y)
						{
							uint32& rObjectIndex = m_ObstrMan.GetObjectIndexToWrite(cGridResolution, y / cWorldFactor, x / cWorldFactor);
							if (rObjectIndex == 0xFFFFFFFF)
								rObjectIndex = index;
							else
							{
								//prefer non vegetation objects
								const bool cExistingIsVeg = m_TempObjects[rObjectIndex].IsVeg();
								const bool cIsVeg = rObj.IsVeg();
								if (cExistingIsVeg && !cIsVeg || (!cIsVeg && !cExistingIsVeg && m_TempObjects[rObjectIndex].radius < rObj.radius))
									rObjectIndex = index;
							}
						}
					}
				//make sure sector is set
				if (m_ObstrMan.IsSectorEmpty(cGridResolution, startY / cWorldFactor, startX / cWorldFactor))
					m_ObstrMan.InitSector(cGridResolution, startY / cWorldFactor, startX / cWorldFactor);
			}

			//terrain acc map
			{
				//update rTerrainMapMarks
				const uint32 cCenterRadius = rObj.radius + cUpdateRadius;
				const float cCenterRadiusF = rObj.radius + (float)cUpdateRadius;
				const float cCenterRadiusSq = cCenterRadiusF * cCenterRadiusF;
				float cLerpMax = rObj.IsVeg() ? rObj.radius - 2.f : rObj.radius - 1.f;
				cLerpMax = std::max(0.f, cLerpMax);//clamp
				const float cInterpolRange = 1.f / (cCenterRadiusF - cLerpMax);

				const float cCenterX = (xyBBox.min.x + xyBBox.max.x) * 0.5f;
				const float cCenterY = (xyBBox.min.y + xyBBox.max.y) * 0.5f;
				const int32 cICenterX = (int32)ceilf(cCenterX);
				const int32 cICenterY = (int32)ceilf(cCenterY);

				const int32 cMarkStartX =
				  std::max((uint32)0, std::min(m_cWidth - 1, ((cICenterX > cCenterRadius) ? cICenterX - cCenterRadius : 0)));
				const int32 cMarkStartY =
				  std::max((uint32)0, std::min(m_cWidth - 1, ((cICenterY > cCenterRadius) ? cICenterY - cCenterRadius : 0)));
				const int32 cMarkEndX =
				  std::max((uint32)0, std::min(m_cWidth - 1, ((cICenterX + cCenterRadius < m_cWidth - 1) ? cICenterX + cCenterRadius : m_cWidth - 1)));
				const int32 cMarkEndY =
				  std::max((uint32)0, std::min(m_cWidth - 1, ((cICenterY + cCenterRadius < m_cWidth - 1) ? cICenterY + cCenterRadius : m_cWidth - 1)));

				for (int32 x = cMarkStartX / cWorldMapMarksFactor; x <= cMarkEndX / cWorldMapMarksFactor; ++x)
					for (int32 y = cMarkStartY / cWorldMapMarksFactor; y <= cMarkEndY / cWorldMapMarksFactor; ++y)
					{
						const int32 cXVal = x * cWorldMapMarksFactor;
						const int32 cYVal = y * cWorldMapMarksFactor;
						//WORLD_HMAP_COORDINATE_EXCHANGE
						uint8& rTerrainMarkVal = rTerrainMapMarks[x * cTerrainMapRes + y];
						const float cCurDistSq =
						  ((float)cXVal - cCenterX) * ((float)cXVal - cCenterX) + ((float)cYVal - cCenterY) * ((float)cYVal - cCenterY);
						if (cCurDistSq < cCenterRadiusSq)
						{
							//lerp between cUpdateRadius/3 .. cUpdateRadius from 1..0
							const uint8 cValToSet = (uint8)(255.f * (1.f - std::max(0.f, sqrtf(cCurDistSq) - cLerpMax) * cInterpolRange));
							rTerrainMarkVal = std::max(cValToSet, rTerrainMarkVal);
						}
					}
			}

			//update obstruction height
			startX = (startX > scAroundObjectRadius) ? startX - scAroundObjectRadius : 0;
			startY = (startY > scAroundObjectRadius) ? startY - scAroundObjectRadius : 0;
			endX = (endX < m_cWidth - scAroundObjectRadius) ? endX + scAroundObjectRadius : m_cWidth - 1;
			endY = (endY < m_cWidth - scAroundObjectRadius) ? endY + scAroundObjectRadius : m_cWidth - 1;
			const float cMaxHeight = crBoxData.bbox.max.z;
			for (uint32 x = startX; x <= endX; x += cHeightWorldFactor)
				for (uint32 y = startY; y <= endY; y += cHeightWorldFactor)
				{
					uint8& rIndex = m_ObstrMan.GetObstructionHeight(m_ObstrMan.GetObstructionMapHeightRes(), y / cHeightWorldFactor, x / cHeightWorldFactor);
					rIndex = std::max(std::min((uint8)255, (uint8)ceil(cMaxHeight)), rIndex);
				}
		}
		++index;
	}
	m_TempObjects.clear();
	return true;
}

const float CTerrainObjectMan::GetObstructionHeight(const uint16 cX, const uint16 cY, const uint16 cResolutionBase) const
{
	return m_ObstrMan.GetObstructionHeight(cResolutionBase, cX, cY);
}

const uint8 CTerrainObjectMan::IsObstructed
(
  const uint16 cX,
  const uint16 cY,
  const uint16 cResolutionBase,
  uint32& rObstructionObjectIndex,
  float& rObstructionAmount,
  const bool cRetrieveAmount
) const
{
	//queried in heightmap space
	assert(cResolutionBase <= m_cWidth);
	rObstructionObjectIndex = m_ObstrMan.GetObjectIndex(cResolutionBase, cX, cY);
	rObstructionAmount = 1.f;
	if (rObstructionObjectIndex != 0xFFFFFFFF)
	{
		assert(rObstructionObjectIndex < m_Objects.size());
		const TTerrainObject& crObj = m_Objects[rObstructionObjectIndex];
		if (crObj.IsOffsetted())
			return scObstructedForOffset;
		//if triangle intersections are active, only return if it is obstructed for offset
		rObstructionAmount = 0.f;
		return scNotObstructed;
	}
	rObstructionAmount = 0.f;
	return scNotObstructed;
}

