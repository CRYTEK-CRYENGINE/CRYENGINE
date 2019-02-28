// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Raster.h"
#include "Quadtree/Quadtree.h"

const float CBBoxRaster::cMaxExt = (float)(CBBoxRaster::scRasterRes) -0.001f;

inline const bool RayAABBIntersect(const Vec3& crOrigin, const Vec3& crDir, const Vec3& crMin, const Vec3& crMax, float& rIn, float& rOut)
{
	rIn = -99999.f, rOut = 99999;
	float newInT, newOutT;

	if (crDir.x == 0)
	{
		if ((crOrigin.x < crMin.x) || (crOrigin.x > crMax.x))
			return false;
	}
	else
	{
		newInT = (crMin.x - crOrigin.x) / crDir.x;
		newOutT = (crMax.x - crOrigin.x) / crDir.x;
		if (newOutT > newInT)
		{
			if (newInT > rIn)
				rIn = newInT;
			if (newOutT < rOut)
				rOut = newOutT;
		}
		else
		{
			if (newOutT > rIn)
				rIn = newOutT;
			if (newInT < rOut)
				rOut = newInT;
		}
		if (rIn > rOut)
			return false;
	}

	if (crDir.y == 0)
	{
		if ((crOrigin.y < crMin.y) || (crOrigin.y > crMax.y))
			return false;
	}
	else
	{
		newInT = (crMin.y - crOrigin.y) / crDir.y;
		newOutT = (crMax.y - crOrigin.y) / crDir.y;
		if (newOutT > newInT)
		{
			if (newInT > rIn)
				rIn = newInT;
			if (newOutT < rOut)
				rOut = newOutT;
		}
		else
		{
			if (newOutT > rIn)
				rIn = newOutT;
			if (newInT < rOut)
				rOut = newInT;
		}
		if (rIn > rOut)
			return false;
	}

	if (crDir.z == 0)
	{
		if ((crOrigin.z < crMin.z) || (crOrigin.z > crMax.z))
			return false;
	}
	else
	{
		newInT = (crMin.z - crOrigin.z) / crDir.z;
		newOutT = (crMax.z - crOrigin.z) / crDir.z;
		if (newOutT > newInT)
		{
			if (newInT > rIn)
				rIn = newInT;
			if (newOutT < rOut)
				rOut = newOutT;
		}
		else
		{
			if (newOutT > rIn)
				rIn = newOutT;
			if (newInT < rOut)
				rOut = newInT;
		}
		if (rIn > rOut)
			return false;
	}
	if (rIn >= 0 || rOut >= 0)
		return true;
	return false;
}

inline const Vec3 ScaleVec(const Vec3& crVec, const Vec3& crScale)
{
	Vec3 vec(crVec);
	vec.x *= crScale.x;
	vec.y *= crScale.y;
	vec.z *= crScale.z;
	return vec;
}

void CBBoxRaster::OffsetSampleAABB(const Vec3& crOrigin, const AABB& crAABB, Vec3& rNewOrigin)
{
	//first calc direction toward center and offset til it is outside using radius
	const Vec3 cBBoxCenter = (crAABB.max + crAABB.min) * 0.5f;
	const float cX = crAABB.min.x - crAABB.max.x;
	const float cY = crAABB.min.y - crAABB.max.y;
	const float cRadius = sqrtf(cX * cX + cY * cY) * 0.5f;
	Vec3 dir(cBBoxCenter.x - crOrigin.x, cBBoxCenter.y - crOrigin.y, 0.f);
	const float cDist = dir.len();
	if (cDist < 0.01f)
	{
		dir.x = 1.f;
		dir.y = 0.f;              //choose some arbitrary direction
	}
	else
	{
		dir.x *= 1.f / cDist; //normalize
		dir.y *= 1.f / cDist; //normalize
	}
	rNewOrigin.z = crOrigin.z;//keep at the same height
	rNewOrigin.x = crOrigin.x - dir.x * (cRadius - cDist + 0.01f);
	rNewOrigin.y = crOrigin.y - dir.y * (cRadius - cDist + 0.01f);
}

namespace
{
const float cLim = (float)CBBoxRaster::scRasterRes;
const float cLimEps = cLim - 0.001f;
const float cCorrScale = (cLim + 1.f) / cLim;
const float cInvCorrScale = 1.f / cCorrScale;
const float cCorrTrans = 0.5f / cLim;
const uint32 cMiddleValInt = (float)(CBBoxRaster::scRasterRes >> 1);
const float cMiddleVal = (float)cMiddleValInt;
const Vec3 cCorrTransVec(cCorrTrans, cCorrTrans, cCorrTrans);
};

void CBBoxRaster::ProjectTrianglesOS
(
  const Vec3 cWorldMin,
  const Vec3 cWorldMax,
  const Matrix34& crOSToWorldRotMatrix,
  const std::vector<SMatChunk>& crChunks,
  bool useConservativeFill,
  const ITriangleValidator* cpTriangleValidator
)
{
	memset(m_pRasterXY, 0, (scRasterRes * scRasterRes) >> 2);
	memset(m_pRasterZY, 0, (scRasterRes * scRasterRes) >> 2);
	memset(m_pRasterZX, 0, (scRasterRes * scRasterRes) >> 2);
	const Vec3 cSize = cWorldMax - cWorldMin;
	const Vec3 cScale(cMaxExt / cSize.x, cMaxExt / cSize.y, cMaxExt / cSize.z);
	const Vec3 cInvScale(1.f / cScale.x, 1.f / cScale.y, 1.f / cScale.z);
	m_WorldToOSTrans = -((cWorldMin + cWorldMax) * 0.5f);
	m_OSScale = cScale;

	const float cConservativeThreshold = 8.f;
	//if relation between x,y and z is larger than cConservativeThreshold, use conservative fill (otherwise pixels get too small)
	const float cMinVal = std::min(std::min(m_OSScale.x, m_OSScale.y), m_OSScale.z);
	const float cMaxVal = std::max(std::max(m_OSScale.x, m_OSScale.y), m_OSScale.z);
	if (cMaxVal / cMinVal > cConservativeThreshold)
		useConservativeFill = true;

	CSimpleTriangleRasterizer triRasterizer(scRasterRes, scRasterRes);
	CRasterTriangleSink rasterTriSinkXY(m_pRasterXY, *this);
	CRasterTriangleSink rasterTriSinkZY(m_pRasterZY, *this);
	CRasterTriangleSink rasterTriSinkZX(m_pRasterZX, *this);

	const float cOffset = (float)(scRasterRes >> 1);
	const Vec3 cOffsetVec(cOffset, cOffset, cOffset);
	const std::vector<SMatChunk>::const_iterator cEnd = crChunks.end();
	for (std::vector<SMatChunk>::const_iterator iter = crChunks.begin(); iter != cEnd; ++iter)
	{
		const uint8* cpPositions = iter->pPositions;
		const vtx_idx* cpIndices = iter->pIndices;
		const uint32 cPosStride = iter->posStride;
		const uint32 cStartIndex = iter->startIndex;
		const uint32 cNum = iter->num;
		const Matrix34& crLocalTM = iter->localTM;

		//rotate object into non translated world space
		for (int i = cStartIndex; i < cStartIndex + cNum; i += 3)
		{
			//project into 0..scRasterRes
#if defined(_DEBUG)
			const float cTestVal = 0.1f;
#endif
			Vec3 pos = *(Vec3*)(&cpPositions[cPosStride * cpIndices[i]]);
			const Vec3 cAWorldPos = crOSToWorldRotMatrix.TransformPoint(crLocalTM.TransformPoint(pos));
			pos = *(Vec3*)(&cpPositions[cPosStride * cpIndices[i + 1]]);
			const Vec3 cBWorldPos = crOSToWorldRotMatrix.TransformPoint(crLocalTM.TransformPoint(pos));
			pos = *(Vec3*)(&cpPositions[cPosStride * cpIndices[i + 2]]);
			const Vec3 cCWorldPos = crOSToWorldRotMatrix.TransformPoint(crLocalTM.TransformPoint(pos));

			if (cpTriangleValidator && !cpTriangleValidator->ValidateTriangle(cAWorldPos, cBWorldPos, cCWorldPos))
				continue;

			const Vec3 cA = ScaleVec(cAWorldPos + m_WorldToOSTrans, m_OSScale) + cOffsetVec;
			const Vec3 cB = ScaleVec(cBWorldPos + m_WorldToOSTrans, m_OSScale) + cOffsetVec;
			const Vec3 cC = ScaleVec(cCWorldPos + m_WorldToOSTrans, m_OSScale) + cOffsetVec;

			float x[3] = { cA.x, cB.x, cC.x };
			float y[3] = { cA.y, cB.y, cC.y };
			float z[3] = { cA.z, cB.z, cC.z };
			uint32 negativeCounts[3] = { 0, 0, 0 };//tracks number of vertices on the side < scRasterRes/2
			for (int i = 0; i < 3; ++i)
			{
				//enforce range
				x[i] = std::max(0.f, std::min(x[i] * cCorrScale - cCorrTrans, cLimEps));
				if (x[i] < cMiddleVal)
					++negativeCounts[0];
				y[i] = std::max(0.f, std::min(y[i] * cCorrScale - cCorrTrans, cLimEps));
				if (y[i] < cMiddleVal)
					++negativeCounts[1];
				z[i] = std::max(0.f, std::min(z[i] * cCorrScale - cCorrTrans, cLimEps));
				if (z[i] < cMiddleVal)
					++negativeCounts[2];
			}
			//fill mode just determines for the entire triangle whether to set it to the negative or positive side
			//(important for ray tracing from inside the raster cube)
			if (useConservativeFill)
			{
				rasterTriSinkXY.SetFillMode(negativeCounts[2] > 1);
				triRasterizer.CallbackFillConservative(x, y, &rasterTriSinkXY);
				rasterTriSinkZY.SetFillMode(negativeCounts[0] > 1);
				triRasterizer.CallbackFillConservative(z, y, &rasterTriSinkZY);
				rasterTriSinkZX.SetFillMode(negativeCounts[1] > 1);
				triRasterizer.CallbackFillConservative(z, x, &rasterTriSinkZX);
			}
			else
			{
				rasterTriSinkXY.SetFillMode(negativeCounts[2] > 1);
				triRasterizer.CallbackFillSubpixelCorrect(x, y, &rasterTriSinkXY);
				rasterTriSinkZY.SetFillMode(negativeCounts[0] > 1);
				triRasterizer.CallbackFillSubpixelCorrect(z, y, &rasterTriSinkZY);
				rasterTriSinkZX.SetFillMode(negativeCounts[1] > 1);
				triRasterizer.CallbackFillSubpixelCorrect(z, x, &rasterTriSinkZX);
			}
		}
	}
}

const bool CBBoxRaster::RayTriangleIntersection
(
  const Vec3& crWSOrigin,
  const Vec3& crWorldDir,
  const bool cOffsetXY,
  const float cStepWidth
) const
{
	//project into object space
	const float cOffset = (float)(scRasterRes >> 1);
	const Vec3 cOffsetVec(cOffset, cOffset, cOffset);
	Vec3 origin = ScaleVec((crWSOrigin + m_WorldToOSTrans), m_OSScale) + cOffsetVec;
	//let occlusion walk a bit to the outside
	origin *= cInvCorrScale;
	origin += cCorrTransVec;

	//adjust direction vector to object scale
	Vec3 crWSDir(crWorldDir.x * m_OSScale.x, crWorldDir.y * m_OSScale.y, crWorldDir.z * m_OSScale.z);
	crWSDir.Normalize();

	//determine if the origin is within the cube
	float in, out;
	if (RayAABBIntersect(origin, crWSDir, Vec3(0, 0, 0), Vec3(cMaxExt, cMaxExt, cMaxExt), in, out))
	{
		const Vec3 cEnd = origin + out * crWSDir;
		bool insideBox = false;
		if (in < 0.f)
		{
			//check if origin really is inside xy raster
			const uint32 cX = std::min(scRasterRes - 1, (((origin.x - floorf(origin.x)) >= 0.5f) ? (uint32)origin.x + 1 : (uint32)origin.x));
			const uint32 cY = std::min(scRasterRes - 1, (((origin.y - floorf(origin.y)) >= 0.5f) ? (uint32)origin.y + 1 : (uint32)origin.y));
			if (GetEntry(m_pRasterXY, cX, cY))
			{
				insideBox = true;//might fail if there are holes inside the xy raster
				if (cOffsetXY)
				{
					//offset origin in xy and set to not inside
					AABB xyBox;
					xyBox.Reset();
					RetrieveXYBBox(xyBox, crWSOrigin.z + m_WorldToOSTrans.z);
					Vec3 newOrigin;
					OffsetSampleAABB(crWSOrigin, xyBox, newOrigin);
					origin = ScaleVec((newOrigin + m_WorldToOSTrans), m_OSScale) + cOffsetVec;
					insideBox = false;
					//get the in/out params again
					RayAABBIntersect(origin, crWSDir, Vec3(0, 0, 0), Vec3(cMaxExt, cMaxExt, cMaxExt), in, out);
					in = 0.f;
				}
			}
			else
				in = 0.f;
		}
		Vec3 start = origin;
		float rayLen = out;
		if (!insideBox)
		{
			start = origin + in * crWSDir;
			rayLen = out - in;
		}

		if (rayLen < 0.0001f)
			return false;

		//trace backward
		for (float curRayLen = rayLen * 0.98f; curRayLen > 0.0001f; curRayLen -= cStepWidth)
		{
			const Vec3 cRayPos = start + curRayLen * crWSDir;
			const uint32 cX = std::min(scRasterRes - 1, (((cRayPos.x - floorf(cRayPos.x)) >= 0.5f) ? (uint32)cRayPos.x + 1 : (uint32)cRayPos.x));
			const uint32 cY = std::min(scRasterRes - 1, (((cRayPos.y - floorf(cRayPos.y)) >= 0.5f) ? (uint32)cRayPos.y + 1 : (uint32)cRayPos.y));
			const uint32 cZ = std::min(scRasterRes - 1, (((cRayPos.z - floorf(cRayPos.z)) >= 0.5f) ? (uint32)cRayPos.z + 1 : (uint32)cRayPos.z));

			const bool cYNegative = (cY < cMiddleValInt);
			const bool cXNegative = (cX < cMiddleValInt);

			if (GetEntry(m_pRasterXY, cX, cY))
			{
				const uint32 cZYRes = GetEntryExact(m_pRasterZY, cZ, cY);
				if ((cZYRes & (cXNegative ? 0x1 : 0x2)) == 0)
					continue;//wrong side
				const uint32 cZXRes = GetEntryExact(m_pRasterZX, cZ, cX);
				if ((cZXRes & (cYNegative ? 0x1 : 0x2)) == 0)
					continue;//wrong side
				return true;
			}
		}
	}
	return false;
}

const bool CBBoxRaster::RetrieveXYBBox(AABB& rXYBox, const float cHeight, const bool cUseDefault) const
{
	//determine xy extension on bottom of object
	//determine row of ZY and ZX raster
	//check row corresponding to 1 unit of height or the last
	const int32 cMiddleHeight = cUseDefault ? 0 : (int32)(cHeight * m_OSScale.z) + (scRasterRes >> 1);
	const int32 cIntHeight = std::max(0, std::min((int32)(scRasterRes - 1), cMiddleHeight));
	int pX0 = -1, pX1 = -1;
	int curLine = cIntHeight;
	while (pX0 == -1 && curLine < scRasterRes)
	{
		for (int i = 0; i < scRasterRes; ++i)
		{
			if (GetEntry(m_pRasterZX, curLine, i))
			{
				pX0 = i;
				break;
			}
		}
		if (pX0 == -1)
			++curLine;
	}
	if (pX0 == -1)
		return true;
	for (int i = scRasterRes - 1; i >= 0; --i)
	{
		if (GetEntry(m_pRasterZX, curLine, i))
		{
			pX1 = i;
			break;
		}
	}
	if (pX1 < pX0)
		return false;
	curLine = cIntHeight;
	int pY0 = -1, pY1 = -1;
	while (pY0 == -1 && curLine < scRasterRes)
	{
		for (int i = 0; i < scRasterRes; ++i)
		{
			if (GetEntry(m_pRasterZY, curLine, i))
			{
				pY0 = i;
				break;
			}
		}
		if (pY0 == -1)
			++curLine;
	}
	if (pY0 == -1)
		return true;
	for (int i = scRasterRes - 1; i >= 0; --i)
	{
		if (GetEntry(m_pRasterZY, curLine, i))
		{
			pY1 = i;
			break;
		}
	}
	if (pY1 < pY0)
		return false;
	//now gen bounding box
	const int32 cOffset = (scRasterRes >> 1);
	rXYBox.min.x = (float)(pX0 - cOffset) / m_OSScale.x - m_WorldToOSTrans.x;
	rXYBox.max.x = (float)(pX1 - cOffset) / m_OSScale.x - m_WorldToOSTrans.x;
	rXYBox.min.y = (float)(pY0 - cOffset) / m_OSScale.y - m_WorldToOSTrans.y;
	rXYBox.max.y = (float)(pY1 - cOffset) / m_OSScale.y - m_WorldToOSTrans.y;
	return true;
}

void CBBoxRaster::SaveTGA(const char* cpName) const
{
	string name(cpName);
	string::size_type pos = name.find(".tga");
	if (pos == string::npos)
		pos = name.find(".TGA");
	if (pos != string::npos)
		name = name.substr(0, pos - 1);
	static uint32 pixels[scRasterRes * scRasterRes];
	memset(pixels, 0, scRasterRes * scRasterRes * sizeof(uint32));
	//this could be way faster but its debug stuff anyway
	for (int i = 0; i < scRasterRes; ++i)
		for (int j = 0; j < scRasterRes; ++j)
			pixels[j * scRasterRes + i] = GetEntry(m_pRasterXY, i, scRasterRes - 1 - j) ? 0xFFFFFFFF : 0;
	NQT::SaveTGA32(name + "XY.tga", pixels, scRasterRes);
	memset(pixels, 0, scRasterRes * scRasterRes * sizeof(uint32));
	for (int i = 0; i < scRasterRes; ++i)
		for (int j = 0; j < scRasterRes; ++j)
			pixels[j * scRasterRes + i] = GetEntry(m_pRasterZY, scRasterRes - 1 - j, i) ? 0xFFFFFFFF : 0;
	NQT::SaveTGA32(name + "ZY.tga", pixels, scRasterRes);
	memset(pixels, 0, scRasterRes * scRasterRes * sizeof(uint32));
	for (int i = 0; i < scRasterRes; ++i)
		for (int j = 0; j < scRasterRes; ++j)
			pixels[j * scRasterRes + i] = GetEntry(m_pRasterZX, scRasterRes - 1 - j, i) ? 0xFFFFFFFF : 0;
	NQT::SaveTGA32(name + "ZX.tga", pixels, scRasterRes);
}

