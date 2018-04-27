// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __terrainobjectman_h__
#define __terrainobjectman_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMath/Cry_Math.h>
#include <vector>
#include "Quadtree/Quadtree.h"
#include "Raster.h"

class CTerrainObjectMan;

inline const float RoundToInt(const float cVal)
{
	return (cVal - floor(cVal) < 0.5f) ? floor(cVal) : ceil(cVal);
}

inline void UnpackColor(const uint32 cCol, ColorF& rFloatColor)
{
	rFloatColor.r = (float)(cCol & 0x000000FF) / 255.f;
	rFloatColor.g = (float)((cCol & 0x0000FF00) >> 8) / 255.f;
	rFloatColor.b = (float)((cCol & 0x00FF0000) >> 16) / 255.f;
	rFloatColor.a = (float)((cCol & 0xFF000000) >> 24) / 255.f;
}

inline void PackColor(uint32& rColour, const ColorF& crFloatColor)
{
	rColour =
	  ((uint32)std::max(0.f, std::min(255.f * crFloatColor.r, 255.f)) |
	   ((uint32)std::max(0.f, std::min(255.f * crFloatColor.g, 255.f)) << 8) |
	   ((uint32)std::max(0.f, std::min(255.f * crFloatColor.b, 255.f)) << 16) |
	   ((uint32)RoundToInt(std::max(0.f, std::min(255.f * crFloatColor.a, 255.f))) << 24));
}

// Description:
//		unpacks a colour on the fly from uint32 to Vec3
inline const Vec3 UnpackColor(const uint32 cCol)
{
	return Vec3((float)(cCol & 0x000000FF) / 255.f, (float)((cCol & 0x0000FF00) >> 8) / 255.f, (float)((cCol & 0x00FF0000) >> 16) / 255.f);
}

// Description:
//		unpacks all 4 colour components on the fly from uint32 to ColorF
inline const ColorF UnpackColor4(const uint32 cCol)
{
	return ColorF((float)(cCol & 0x000000FF) / 255.f, (float)((cCol & 0x0000FF00) >> 8) / 255.f, (float)((cCol & 0x00FF0000) >> 16) / 255.f, (float)((cCol & 0xFF000000) >> 24) / 255.f);
}

// Description:
//		packs a colour on the fly from uint32 to Vec3
inline const uint32 PackColor(const Vec3& crCol)
{
	return
	  ((uint32)std::max(0.f, std::min(255.f * crCol.x + 0.5f, 255.f)) |
	   ((uint32)std::max(0.f, std::min(255.f * crCol.y + 0.5f, 255.f)) << 8) |
	   ((uint32)std::max(0.f, std::min(255.f * crCol.z + 0.5f, 255.f)) << 16));
}

// Description:
//		packs a colour on the fly to uint32 from ColorF
inline const uint32 PackColor4(const ColorF& crCol)
{
	return
	  ((uint32)std::max(0.f, std::min(255.f * crCol.r + 0.5f, 255.f)) |
	   ((uint32)std::max(0.f, std::min(255.f * crCol.g + 0.5f, 255.f)) << 8) |
	   ((uint32)std::max(0.f, std::min(255.f * crCol.b + 0.5f, 255.f)) << 16) |
	   ((uint32)std::max(0.f, std::min(255.f * crCol.a + 0.5f, 255.f)) << 24));
}

// Description:
//	handles access to obstructions, memory might be larger than 64 MB->split into parts
//	obstructions are in heightmap space
class CObstructionAccessManager
{
public:
	CObstructionAccessManager() : m_ObstructionMapRes(0), m_HeightObstrGridResolution(0){}
	void         DeAllocateObstructionAmount()      { m_ObstructionAmounts.swap(std::vector<std::vector<uint8>>()); } //frees memory used for obstruction amounts
	void         DeAllocateObjectIndices()          { m_ObjectIndices.swap(std::vector<std::vector<uint32>>()); }     //frees memory used for obstruction indices
	const uint32 GetObstructionMapRes() const       { return m_ObstructionMapRes; }
	const uint32 GetObstructionMapHeightRes() const { return m_HeightObstrGridResolution; }
	void         AllocateAndInit(const uint32 cRes);

	const bool   IsSectorEmpty(const uint32 cRes, const uint16 cX, const uint16 cY) const
	{
		assert(!m_ObjectIndices.empty());
		uint32 basisIndex, offsetIndex;
		GetAccessIndices(basisIndex, offsetIndex, cRes, cX, cY);
		return m_ObjectIndices[basisIndex].empty();
	}

	const uint32 GetObjectIndex(const uint32 cRes, const uint16 cX, const uint16 cY) const
	{
		assert(!m_ObjectIndices.empty());
		uint32 basisIndex, offsetIndex;
		GetAccessIndices(basisIndex, offsetIndex, cRes, cX, cY);
		if (m_ObjectIndices[basisIndex].empty())
			return 0xFFFFFFFF;//means not set
		return m_ObjectIndices[basisIndex][offsetIndex];
	}

	void InitSector(const uint32 cRes, const uint16 cX, const uint16 cY)
	{
		assert(!m_ObjectIndices.empty());
		uint32 basisIndex, offsetIndex;
		GetAccessIndices(basisIndex, offsetIndex, cRes, cX, cY);
		std::vector<uint32>& rObjVec = m_ObjectIndices[basisIndex];
		if (rObjVec.empty())
		{
			//initialize this one
			rObjVec.resize(scSectorSize * scSectorSize);
			const std::vector<uint32>::const_iterator cObjEnd = rObjVec.end();
			for (std::vector<uint32>::iterator iter = rObjVec.begin(); iter != cObjEnd; ++iter)
				*iter = 0xFFFFFFFF;
		}
	}

	uint32& GetObjectIndexToWrite(const uint32 cRes, const uint16 cX, const uint16 cY)
	{
		//access to write
		assert(!m_ObjectIndices.empty());
		uint32 basisIndex, offsetIndex;
		GetAccessIndices(basisIndex, offsetIndex, cRes, cX, cY);
		std::vector<uint32>& rObjVec = m_ObjectIndices[basisIndex];
		if (rObjVec.empty())
		{
			//initialize this one
			rObjVec.resize(scSectorSize * scSectorSize);
			const std::vector<uint32>::const_iterator cObjEnd = rObjVec.end();
			for (std::vector<uint32>::iterator iter = rObjVec.begin(); iter != cObjEnd; ++iter)
				*iter = 0xFFFFFFFF;
		}
		return rObjVec[offsetIndex];
	}

	const uint8 GetObstructionAmount(const uint32 cRes, const uint16 cX, const uint16 cY) const
	{
		assert(!m_ObstructionAmounts.empty());
		uint32 basisIndex, offsetIndex;
		GetAccessIndices(basisIndex, offsetIndex, cRes, cX, cY);
		if (m_ObstructionAmounts[basisIndex].empty())
			return 0;
		return m_ObstructionAmounts[basisIndex][offsetIndex];
	}

	uint8& GetObstructionAmountToWrite(const uint32 cRes, const uint16 cX, const uint16 cY)
	{
		//access to write
		assert(!m_ObstructionAmounts.empty());
		uint32 basisIndex, offsetIndex;
		GetAccessIndices(basisIndex, offsetIndex, cRes, cX, cY);
		std::vector<uint8>& rObstrVec = m_ObstructionAmounts[basisIndex];
		if (rObstrVec.empty())
		{
			//initialize this one
			rObstrVec.resize(scSectorSize * scSectorSize);
			memset(&rObstrVec[0], 0, scSectorSize * scSectorSize);
		}
		return rObstrVec[offsetIndex];
	}

	const uint8 GetObstructionHeight(const uint32 cRes, const uint16 cX, const uint16 cY) const
	{
		assert(!m_ObstructionHeights.empty());
		uint32 basisIndex, offsetIndex;
		GetAccessIndicesHeightLow(basisIndex, offsetIndex, cRes, cX, cY);
		return m_ObstructionHeights[basisIndex][offsetIndex];
	}

	uint8& GetObstructionHeight(const uint32 cRes, const uint16 cX, const uint16 cY)
	{
		assert(!m_ObstructionHeights.empty());
		uint32 basisIndex, offsetIndex;
		GetAccessIndicesHeightLow(basisIndex, offsetIndex, cRes, cX, cY);
		return m_ObstructionHeights[basisIndex][offsetIndex];
	}

	static const uint32 scSectorSize = 64;
	static const uint32 scSectorSizeShift = 6;

private:
	uint32 m_ObstructionMapRes;                           //resolution of obstruction map

	//due to memory requirements of 8x8 km, object indices and obstruction amounts are handled on demand and sectorwise
	//means each sector is only allocated if it is written to it
	//sectors are also used to mark areas without any interests (so not placing any SH samples)

	std::vector<std::vector<uint32>> m_ObjectIndices;     //object indices
	std::vector<std::vector<uint8>>  m_ObstructionAmounts;//corresponding obstruction amounts

	//obstruction height takes 16 MB for 8x8 km (half res and uint8)
	//therefore it is not handled sectorwise as above ones
	std::vector<std::vector<uint8>> m_ObstructionHeights;    //allocation parts * vector of height obstruction amounts
	uint32                          m_AllocPartsHeightLow;   //lower allocation parts, y is divided by that (m_ObstructionHeights)
	uint32                          m_YPartMappingHeightLow; //lower mapping of y to lower allocation part (m_ObstructionHeights)

	uint32                          m_HeightObstrGridResolution; //grid resolution corresponding to m_HeightObstructionMap, lower res

	void GetAccessIndices(uint32& rBasisIndex, uint32& rOffsetIndex, const uint32 cRes, const uint16 cX, const uint16 cY) const;
	void GetAccessIndicesHeightLow(uint32& rBasisIndex, uint32& rOffsetIndex, const uint32 cRes, const uint16 cX, const uint16 cY) const;
};

// Description:
//		large representation of an terrain object
struct STempBBoxData
{
	AABB         bbox;
	ColorF       col;
	CBBoxRaster* pBBoxRaster;
	bool         onGround;

	STempBBoxData() : bbox(Vec3(0, 0, 0), Vec3(0, 0, 0)), col(0.f, 0.f, 0.f, 0.f), onGround(false), pBBoxRaster(NULL)
	{}

	STempBBoxData(const AABB& crBBox, const ColorF& crCol, CBBoxRaster* pBBoxRasterCopy, const bool cOnGround = true)
		: bbox(crBBox.min, crBBox.max), col(crCol), pBBoxRaster(pBBoxRasterCopy), onGround(cOnGround)
	{}
};

// Description:
//		small cache optimized representation of an terrain object
struct SBBoxData
{
	AABB         bbox;
	uint32       col;
	CBBoxRaster* pBBoxRaster;
	bool         onGround;

	SBBoxData() : bbox(Vec3(0, 0, 0), Vec3(0, 0, 0)), col(0), onGround(false), pBBoxRaster(NULL)
	{}

	SBBoxData(const AABB& crBBox, ColorF& crCol, CBBoxRaster* pBBoxRasterCopy, const bool cOnGround = true)
		: bbox(crBBox.min, crBBox.max), col(crCol.pack_abgr8888()), pBBoxRaster(pBBoxRasterCopy), onGround(cOnGround)
	{}
};

// Description:
//		terrain object for indirect lighting is specified by 1 or 2 bounding boxes, a position and a colour per bounding box
template<class SBBoxDataType>
struct STerrainObject
{
	std::vector<SBBoxDataType> bboxCol;//world space bounding boxes with an attached colour
	uint16                     posX, posY; //terrain position
	float                      radius;     //radius of object

	STerrainObject() : posX(0), posY(0), id(scNoID)
	{}

	const uint32 GetID() const
	{
		return (id & scIDMask);
	}

	void SetID(const uint32 cID)
	{
		id = (cID & scIDMask);
	}

	void SetOffsetted(const bool cDoOffset)
	{
		id |= cDoOffset ? scOffsetSample : 0;
	}

	const bool IsOffsetted() const
	{
		return ((id & scOffsetSample) != 0);
	}

	void SetVeg(const bool cIsVeg)
	{
		id |= cIsVeg ? scIsVeg : 0;
	}

	const bool IsVeg() const
	{
		return ((id & scIsVeg) != 0);
	}

private:
	uint32              id; //id

	static const uint32 scNoID = (1 << 29);
	static const uint32 scOffsetSample = (1 << 31);
	static const uint32 scIsVeg = (1 << 30);
	static const uint32 scIDMask = ~(scOffsetSample | scIsVeg);

	friend class CTerrainObjectMan;
};

typedef STerrainObject<SBBoxData>     TTerrainObject;
typedef STerrainObject<STempBBoxData> TTempTerrainObject;

// Description:
//		struct which represents an object for ray casting for a certain wedge
struct SAngleBox
{
	static const uint8 scAlphaLTZ = 0x1;
	static const uint8 scVeg = 0x2;

	float              angleMin;
	float              angleMax;
	ColorF             col;
	float              alphaMul;
	float              stepWidthRasterTable;
	CBBoxRaster*       pBBoxRaster;
	uint8              flags;

	const bool HasAlphaLTZ() const                 { return flags & scAlphaLTZ; }
	void       SetAlphaLTZ(const bool cSet = true) { flags = cSet ? (flags | scAlphaLTZ) : (flags & ~scAlphaLTZ); }

	const bool IsVeg() const                       { return flags & scVeg; }
	void       SetIsVeg(const bool cSet = true)    { flags = cSet ? (flags | scVeg) : (flags & ~scVeg); }

	SAngleBox()
		: angleMin(0.f), angleMax(0.f), col(0.f, 0.f, 0.f, 0.f), alphaMul(1.f), flags(scAlphaLTZ), pBBoxRaster(NULL), stepWidthRasterTable(1.5f)
	{}
};

// Description:
//		Class that generates and manages objects on the terrain taken into account for indirect lighting gen
class CTerrainObjectMan
{
public: // -----------------------------------------------------------------------

	typedef uint32                                                                                                                   TContentType;//index into m_Objects
	typedef NQT::CQuadTree<TContentType, 32 /*16*//*max elems per leaf cell*/, uint16 /*pos type*/, uint32 /*big index type*/, true> TQuadTree;
	typedef TQuadTree::TVec2F                                                                                                        TVec2F;
	typedef TQuadTree::TVec2                                                                                                         TVec2;

	// Description:
	//		constructor taking heightmap width
	CTerrainObjectMan(const uint32 cWidth);
	// Description:
	//		destructor
	~CTerrainObjectMan();
	// Description:
	//		retrieves the ray intersection data from the quadtree, returns true if at least one object has been retrieved
	const bool CalcRayIntersectionData
	(
	  const Vec3& crOrigin,
	  const Vec2& crDir,
	  const float cLength = 15.f,
	  const bool cUpdateRayIntersectionContents = true
	);
	// Description:
	//		performs ray intersection with the objects on the terrain previously retrieved by GetRayIntersectionData
	//		returns true if intersected and sets up the colour in that case
	//		performs ray triangle intersection test on bounding box if selected (by compiler)
	const bool RayIntersection(ColorF& rCollisionColour, const float cHeight) const;
	// Description:
	//		adds an terrain object, returns true if object has been successfully added
	const bool AddObject
	(
	  const float cHeight,
	  TTempTerrainObject& rObj,
	  uint32& rID,
	  const bool cIsVegetation = false,
	  const bool cOffsetSample = false
	);
	// Description:
	//		converts objects into more cache friendly structure and also
	//		generates a map telling for each point whether it is inside an object or not, queried in heightmap space
	//		terrain marks contains true if the visibility generation for the terrain indirect lighting requires ray casting
	//			it is accessed heightmap space
	const bool ConvertAndGenObstructionMap
	(
	  const uint32 cGridResolution,
	  std::vector<uint8>& rTerrainMapMarks,
	  const unsigned int cTerrainMapRes,
	  const unsigned int cUpdateRadius
	);
	// Description:
	//		retrieves the radius of an object
	const float GetRadius(const uint32 cObjectID) const;
	// Description:
	//		returns true if it is vegetation
	const bool IsVeg(const uint32 cObjectID) const;
	// Description:
	//		returns 1 if obstructed, 2 if inside a box to offset relative to, 0 if outside all objects
	const uint8 IsObstructed
	(
	  const uint16 cX,
	  const uint16 cY,
	  const uint16 cResolutionBase,
	  uint32& rObstructionObjectIndex,
	  float& rObstructionAmount,
	  const bool cRetrieveAmount = false
	) const;
	// Description:
	//		returns the max height of objects around position cX, cY within a certain bound (15 m)
	const float GetObstructionHeight(const uint16 cX, const uint16 cY, const uint16 cResolutionBase) const;
	// Description:
	//		returns the resolution of the obstruction map
	const uint32 GetObstructionMapRes() const { return m_ObstrMan.GetObstructionMapRes(); }
	// Description:
	//		frees obstruction amount and
	void FreeObstructionAmount() { m_ObstrMan.DeAllocateObstructionAmount(); }
	// Description:
	//		frees obstruction amount and
	void FreeObstructionIndices() { m_ObstrMan.DeAllocateObjectIndices(); }
	// Description:
	//		returns true if sector is unused by any objects
	const bool IsSectorEmpty(const uint32 cRes, const uint16 cX, const uint16 cY) const
	{
		return m_ObstrMan.IsSectorEmpty(cRes, cX, cY);
	}
	// Description:
	//		returns a terrain object to the corresponding id
	const TTerrainObject& GetTerrainObjectByID(const uint32 cID) const;

	static const uint8    scObstructed;           //constant for obstruction indication of a sample by an object
	static const uint8    scNotObstructed;        //constant for no obstruction indication
	static const uint8    scObstructedForOffset;  //constant for obstruction indication of a sample by an object where sample has to be offseted

private:

	static const uint32              scMaxRayCastObjectCount = 8; //max number of objects retrieved
	static const uint32              scAroundObjectRadius = 16;   //radius where object height matters for max object height per texel
	static const float               scFullObstructionMargin;     //margin for objects to be marked as fully obstructed by an object

	std::vector<TTempTerrainObject>  m_TempObjects;   //all objects on the terrain before optimization
	std::vector<TTerrainObject>      m_Objects;       //all objects on the terrain after optimization
	TQuadTree                        m_QuadTree;      //quadtree managing these objects
	std::vector<const TContentType*> m_LastResults;   //cached results from last query
	const uint32                     m_cWidth;        //terrain width units (in meters)

	CObstructionAccessManager        m_ObstrMan;      //obstruction manager for amount and object indices

	std::vector<SAngleBox>           m_RayIntersectionObjects; //ray intersection data set up by CalcRayIntersectionData and used by RayIntersection

	Vec3                             m_RayIntersectionOrigin; //corresponding origin
	Vec2                             m_RayIntersectionDir;    //corresponding dir (only xy set, height specific)

	//ray aabb intersection test, if true, than the min and max angles are stored in rOutput
	const bool RayAABBNoZ(const Vec3& crOrigin, const Vec2& crDir, const AABB& crAABB, Vec2& rOutput, const bool cInsideBox, const bool cRecursed) const;

	//calculate min and max angle of bounding box projection onto unit sphere around origin
	//cXAxisAdjustable tells which axis can be adjusted (3d test turned into 2D test)
	void GetMinMaxAngle(const Vec3& crOrigin, const Vec2& crHitPoint, Vec2& rMinMaxAngle, const AABB& crAABB, const bool cXAxisAdjustable) const;

	void ConvertObject(TTempTerrainObject& rObj);

	// Description:
	//		works on m_LastResults to prepare the intersection data for direction crDir
	// NOTE: objects below horizon of crOrigin are discarded
	const bool CalcRayIntersection
	(
	  const bool cIsTerrainOccl,
	  const Vec3& crOrigin,
	  const Vec2& crDir,
	  const bool cInsideReturnsHit
	);
};

//calc the objects a ray in a certain direction intersects
//calculate for all objects the min and max angle of wedges intersecting
inline const bool CTerrainObjectMan::CalcRayIntersectionData
(
  const Vec3& crOrigin,
  const Vec2& crDir,
  const float cLength,
  const bool cUpdateRayIntersectionContents
)
{
	//get objects from quadtree
	if (cUpdateRayIntersectionContents)
		m_QuadTree.GetRayIntersectionContents
		(
		  m_LastResults,
		  TVec2F(crOrigin.x, crOrigin.y),
		  TVec2F(crDir.x, crDir.y),
		  cLength,
		  scMaxRayCastObjectCount
		);
	return CalcRayIntersection(false, crOrigin, crDir, false);
}

inline CTerrainObjectMan::CTerrainObjectMan(const uint32 cWidthUnits)
	: m_QuadTree(cWidthUnits, TVec2(cWidthUnits >> 1, cWidthUnits >> 1), (logf((float)cWidthUnits) + 8), 16 * 1024 * 1024, 16 * 1024 * 1024),
	m_cWidth(cWidthUnits)
{
	m_LastResults.reserve(scMaxRayCastObjectCount);
	m_TempObjects.reserve(4 * 1024 * 1024 / sizeof(TTempTerrainObject));
}

inline CTerrainObjectMan::~CTerrainObjectMan()
{}

inline const float CTerrainObjectMan::GetRadius(const uint32 cObjectID) const
{
	assert(cObjectID < m_Objects.size());
	return m_Objects[cObjectID].radius;//id corresponds exactly to the index
}

inline const bool CTerrainObjectMan::IsVeg(const uint32 cObjectID) const
{
	assert(cObjectID < m_Objects.size());
	return m_Objects[cObjectID].IsVeg();//id corresponds exactly to the index
}

inline const TTerrainObject& CTerrainObjectMan::GetTerrainObjectByID(const uint32 cID) const
{
	assert(cID < m_Objects.size());
	return m_Objects[cID];//id corresponds exactly to the index
}

#endif //__terrainobjectman_h__

