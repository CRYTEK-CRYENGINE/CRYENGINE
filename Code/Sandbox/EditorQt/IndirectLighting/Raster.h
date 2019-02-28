// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __raster_h__
#define __raster_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMath/Cry_Math.h>
#include "../LightmapCompiler/SimpleTriangleRasterizer.h"

struct SMatChunk
{
	Matrix34 localTM;
	uint32   startIndex;
	int32    num;
	int32    matIndex;
	uint32   posStride;
	vtx_idx* pIndices;
	uint8*   pPositions;
	SMatChunk()
		: startIndex(0), num(0), matIndex(-1), posStride(sizeof(Vec3)), pIndices(NULL), pPositions(NULL)
	{
		localTM.SetIdentity();
	}
};

//interface for an optional triangle validator getting the triangle world positions passed
struct ITriangleValidator
{
	virtual const bool ValidateTriangle(const Vec3 crA, const Vec3 crB, const Vec3 crC) const { return true; }
};

//class containing a XY, ZX and ZY raster-bit-table and the matrix which transforms into it
//object is transformed into world space without offset
//a non uniform scale is applied to fit all 3 raster tables
class CBBoxRaster
{
public:
	static const uint32 scRasterRes = 32;
	enum EBufferSel
	{
		eBS_XY,
		eBS_ZY,
		eBS_ZX
	};

	CBBoxRaster();  //default ctor
	~CBBoxRaster(); //dtor

	//project once, each subsequent call will invalidate results from before
	void ExecuteProjectTrianglesOS
	(
	  const Vec3& crWorldMin,
	  const Vec3& crWorldMax,
	  const Matrix34& crOSToWorldRotMatrix,
	  const std::vector<SMatChunk>& crChunks,
	  bool useConservativeFill,
	  const ITriangleValidator* cpTriangleValidator
	);//projects triangles into scaled object space raster tables

	//returns result of ray-material query
	const bool    RayTriangleIntersection(const Vec3& crWSOrigin, const Vec3& crWSDir, const bool cOffsetXY, const float cStepWidth = 1.5f) const;

	const bool    GetEntry(const uint32* const cpBuffer, const uint32 cX, const uint32 cY) const; //returns the value in the corresponding table

	const uint32  GetEntryExact(const uint32* const cpBuffer, const uint32 cX, const uint32 cY) const;  //returns the exact bit in the corresponding table

	void          SetEntry(uint32* pBuffer, const uint32 cX, const uint32 cY, const bool cFillModeNegative); //sets the value in the corresponding table

	void          SaveTGA(const char* cpName) const;

	const uint32* GetBuffer(const EBufferSel cBufSel);

	const bool    RetrieveXYBBox(AABB& rXYBox, const float cHeight = 1.f, const bool cUseDefault = false) const;//retrieves height on bottom of object

	//uses ray aabb intersection to find entry point into bounding box
	//return false if failed
	static void OffsetSampleAABB(const Vec3& crOrigin, const AABB& crAABB, Vec3& rNewOrigin);

	//project once, each subsequent call will invalidate results from before
	void ProjectTrianglesOS
	(
	  const Vec3 cWorldMin,
	  const Vec3 cWorldMax,
	  const Matrix34& crOSToWorldRotMatrix,
	  const std::vector<SMatChunk>& crChunks,
	  bool useConservativeFill,
	  const ITriangleValidator* cpTriangleValidator
	);//projects triangles into scaled object space raster tables

private:
	uint32             m_pRasterXY[(scRasterRes * scRasterRes) >> 2]; //XY raster bit table, 2 bit per entry
	uint32             m_pRasterZY[(scRasterRes * scRasterRes) >> 2]; //ZY raster bit table, 2 bit per entry
	uint32             m_pRasterZX[(scRasterRes * scRasterRes) >> 2]; //ZX raster bit table, 2 bit per entry
	Vec3               m_WorldToOSTrans;//subtract that
	Vec3               m_OSScale;//scale to fit the whole raster tables

	static const float cMaxExt;
};

inline CBBoxRaster::CBBoxRaster()
{}

inline CBBoxRaster::~CBBoxRaster()
{}

inline const bool CBBoxRaster::GetEntry(const uint32* const cpBuffer, const uint32 cX, const uint32 cY) const
{
	//return true if on either negative or positive side
	assert(cX < scRasterRes && cY < scRasterRes);
	const uint32 cIndex = (cY * scRasterRes + cX);
	const uint32 cMask = (1 << ((cIndex & 15) << 1)) | (1 << (((cIndex & 15) << 1) + 1));
	return ((cpBuffer[cIndex >> 4] & cMask) != 0);
}

inline const uint32 CBBoxRaster::GetEntryExact(const uint32* const cpBuffer, const uint32 cX, const uint32 cY) const
{
	//return 0 if not entry, 1 if negative and 2 if positive
	assert(cX < scRasterRes && cY < scRasterRes);
	const uint32 cIndex = (cY * scRasterRes + cX);
	const uint32 cMask0 = (1 << ((cIndex & 15) << 1));
	const uint32 cMask1 = (1 << (((cIndex & 15) << 1) + 1));
	return (((cpBuffer[cIndex >> 4] & cMask0) != 0) ?
	        ((cpBuffer[cIndex >> 4] & cMask1) != 0) ? (1 + 2) : 1 : ((cpBuffer[cIndex >> 4] & cMask1) != 0) ? 2 : 0);
}

inline void CBBoxRaster::SetEntry(uint32* pBuffer, const uint32 cX, const uint32 cY, const bool cFillModeNegative)
{
	assert(cX < scRasterRes && cY < scRasterRes);
	const uint32 cIndex = (cY * scRasterRes + cX);
	const uint32 cBufIndex = cIndex >> 4;
	if (cFillModeNegative)
		pBuffer[cBufIndex] |= (1 << ((cIndex & 15) << 1));
	else
		pBuffer[cBufIndex] |= (1 << (((cIndex & 15) << 1) + 1));
}

inline const uint32* CBBoxRaster::GetBuffer(const CBBoxRaster::EBufferSel cBufSel)
{
	return (cBufSel == eBS_XY) ? m_pRasterXY : (cBufSel == eBS_ZY) ? m_pRasterZY : m_pRasterZX;
}

//-------------------------------------------------------------------------------------------------------

class CRasterTriangleSink : public CSimpleTriangleRasterizer::IRasterizeSink
{
public:
	CRasterTriangleSink(uint32* pBuf, CBBoxRaster& rBBoxRaster);
	virtual void Line(const float, const float, const int cIniLeft, const int cIniRight, const int cLineY);
	void         SetFillMode(const bool cNegative);

private:
	uint32*      m_pBuf;        //pointer to buffer where to set the entries
	CBBoxRaster& m_rBBoxRaster; //reference to box raster setting the bit entries
	bool         m_FillMode;    //fill mode: either 1 or 2 according to position of triangle
};

inline CRasterTriangleSink::CRasterTriangleSink(uint32* pBuf, CBBoxRaster& rBBoxRaster)
	: m_pBuf(pBuf), m_rBBoxRaster(rBBoxRaster), m_FillMode(false)
{}

inline void CRasterTriangleSink::Line
(
  const float,
  const float,
  const int cIniLeft,
  const int cIniRight,
  const int cLineY
)
{
	for (int x = cIniLeft; x < cIniRight; ++x)
		m_rBBoxRaster.SetEntry(m_pBuf, x, cLineY, m_FillMode);
}

inline void CRasterTriangleSink::SetFillMode(const bool cNegative)
{
	m_FillMode = cNegative;
}

#endif // __raster_h__

