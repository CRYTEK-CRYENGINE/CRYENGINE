// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "terrain.h"

namespace
{

ILINE uint32 encodeby1(uint32 n)
{
	n &= 0x0000ffff;
	n = (n | (n << 8)) & 0x00FF00FF;
	n = (n | (n << 4)) & 0x0F0F0F0F;
	n = (n | (n << 2)) & 0x33333333;
	n = (n | (n << 1)) & 0x55555555;
	return n;
}

ILINE float GetHeightLocal(SRangeInfo const& ri,  int nX, int nY, float fX, float fY)
{
	float fZ = ri.GetHeight(nX, nY) * (1.f - fX) * (1.f - fY)
	         + ri.GetHeight(nX + 1, nY) * fX * (1.f - fY)
	         + ri.GetHeight(nX, nY + 1) * (1.f - fX) * fY
	         + ri.GetHeight(nX + 1, nY + 1) * fX * fY;
	return fZ;
}

ILINE float GetHeightTriangulated(Vec4 const& vZ, float fX, float fY)
{
	if (fX + fY < 1.f)
	{
		// Lower triangle.
		return vZ[0] * (1.f - fX - fY)
			+ vZ[1] * fX
			+ vZ[2] * fY;
	}
	else
	{
		// Upper triangle.
		return vZ[3] * (fX + fY - 1.f)
			+ vZ[2] * (1.f - fX)
			+ vZ[1] * (1.f - fY);
	}
}

float GetHeightUnits(SRangeInfo const& ri, int nX_units, int nY_units, int nUnitToSectorBS)
{
	int nMask = ri.nSize - 2;

	if (ri.nUnitBitShift == 0)
	{
		// full lod is available
		return ri.GetHeight(nX_units & nMask, nY_units & nMask);
	}
	else
	{
		float fInvStep = (ri.nSize > 1) ? (1.f / ((1 << nUnitToSectorBS) / (ri.nSize - 1))) : 1.f;

		// interpolate
		int nX = nX_units >> ri.nUnitBitShift;
		float fX = (nX_units * fInvStep) - nX;
		CRY_MATH_ASSERT(fX + fInvStep <= 1.f);

		int nY = nY_units >> ri.nUnitBitShift;
		float fY = (nY_units * fInvStep) - nY;
		CRY_MATH_ASSERT(fY + fInvStep <= 1.f);

		return GetHeightLocal(ri, nX & nMask, nY & nMask, fX, fY);
	}
}

Vec4 Get4HeightsUnits(CTerrainNode const& node, int nX_units, int nY_units)
{
	SRangeInfo const& ri = node.m_rangeInfo;
	int nMask = ri.nSize - 2;
	if (ri.nUnitBitShift == 0)
	{
		// full lod is available
		nX_units &= nMask;
		nY_units &= nMask;
	#ifdef CRY_PLATFORM_SSE2
		u32v4 vdata = convert<u32v4>(
			ri.GetRawData(nX_units, nY_units).raw,
			ri.GetRawData(nX_units + 1, nY_units).raw,
			ri.GetRawData(nX_units, nY_units + 1).raw,
			ri.GetRawData(nX_units + 1, nY_units + 1).raw
		);
		return ri.RawDataToHeight(vdata);
	#else
		return Vec4(
			ri.GetHeight(nX_units, nY_units),
			ri.GetHeight(nX_units + 1, nY_units),
			ri.GetHeight(nX_units, nY_units + 1),
			ri.GetHeight(nX_units + 1, nY_units + 1)
		);
	#endif
	}
	else
	{
		float fInvStep = float(ri.nSize - 1) / float(node.GetSectorSizeInHeightmapUnits());

		// interpolate
		int nX = nX_units >> ri.nUnitBitShift;
		float fX = (nX_units * fInvStep) - nX;
		CRY_MATH_ASSERT(fX + fInvStep <= 1.f);

		int nY = nY_units >> ri.nUnitBitShift;
		float fY = (nY_units * fInvStep) - nY;
		CRY_MATH_ASSERT(fY + fInvStep <= 1.f);

		nX &= nMask;
		nY &= nMask;
		return Vec4(
			GetHeightLocal(ri, nX, nY, fX, fY),
			GetHeightLocal(ri, nX, nY, fX + fInvStep, fY),
			GetHeightLocal(ri, nX, nY, fX, fY + fInvStep),
			GetHeightLocal(ri, nX, nY, fX + fInvStep, fY + fInvStep)
		);
	}
}

}

ILINE Vec4 CTerrain::Get4ZUnits(int ux, int uy) const
{
	const CTerrainNode* pNode = GetSecInfoUnits(ux, uy);
	if (pNode && pNode->m_rangeInfo.pHMData)
		return Get4HeightsUnits(*pNode, ux, uy);
	return Vec4(TERRAIN_BOTTOM_LEVEL);
}

float CTerrain::GetZApr(float x, float y) const
{
	// convert into hmap space
	float uX = x * GetInvUnitSize();
	float uY = y * GetInvUnitSize();

	int nX = int(uX);
	int nY = int(uY);

	if (m_pParentNode && min(x, y) > 0 && InsideTerrainUnits(nX, nY))
	{
		Vec4 vZ = Get4ZUnits(nX, nY);
		return GetHeightTriangulated(vZ, uX - nX, uY - nY);
	}

	return TERRAIN_BOTTOM_LEVEL;
}

Vec4 CTerrain::GetNormalAndZ(float x, float y, float size) const
{
	static Vec4 vNoTerrain(0, 0, 0, TERRAIN_BOTTOM_LEVEL);

	if (!m_pParentNode)
		return vNoTerrain;

	if (size == 0)
	{
		// Get normal and height at point
		float uX = x * GetInvUnitSize();
		float uY = y * GetInvUnitSize();

		int nX = int(uX);
		int nY = int(uY);

		if (min(x, y) < 0 || !InsideTerrainUnits(nX, nY))
			return vNoTerrain;

		Vec4 vZ = Get4ZUnits(nX, nY);

		float fX = uX - nX;
		float fY = uY - nY;
		if (fX + fY < 1.f)
		{
			// Lower triangle.
			return Vec4(
				Vec3(vZ[0] - vZ[1], vZ[0] - vZ[2], m_fUnitSize).GetNormalizedFast(), // normal
				vZ[0] * (1.f - fX - fY) + vZ[1] * fX + vZ[2] * fY); // z
		}
		else
		{
			// Upper triangle.
			return Vec4(
				Vec3(vZ[2] - vZ[3], vZ[1] - vZ[3], m_fUnitSize).GetNormalizedFast(), // normal
				vZ[3] * (fX + fY - 1.f) + vZ[2] * (1.f - fX) + vZ[1] * (1.f - fY)); // z
		}
	}

	// Use hardware Vec4 for 4 coords x0, y0, x1, y1
	Vec4 wRect(x - size, y - size, x + size, y + size);

	if (min(wRect.x, wRect.y) < 0.0f || max(wRect.z, wRect.w) >= (float)m_nTerrainSize)
	{
		wRect = crymath::clamp(wRect, Vec4(0.0f), Vec4((float)m_nTerrainSize - 1.0f));
		size = min(wRect.z - wRect.x, wRect.w - wRect.y);
		if (size < 0.0f)
			return vNoTerrain;
	}

	// Convert to terrain units, get ints and fracs
	Vec4 uRect = wRect * m_fInvUnitSize;
	Vec4i nRect = Vec4i(uRect);
	Vec4 fRect = uRect - Vec4(nRect);

	// Query the points
	Vec4 zs;
	static bool checkSame = true;
	if (checkSame && nRect.x == nRect.z && nRect.y == nRect.w)
	{
		Vec4 zc = Get4ZUnits(nRect.x, nRect.y);
		zs[0] = GetHeightTriangulated(zc, fRect.x, fRect.y);
		zs[1] = GetHeightTriangulated(zc, fRect.z, fRect.y);
		zs[2] = GetHeightTriangulated(zc, fRect.x, fRect.w);
		zs[3] = GetHeightTriangulated(zc, fRect.z, fRect.w);
	}
	else
	{
		zs[0] = GetHeightTriangulated(Get4ZUnits(nRect.x, nRect.y), fRect.x, fRect.y);
		zs[1] = GetHeightTriangulated(Get4ZUnits(nRect.z, nRect.y), fRect.z, fRect.y);
		zs[2] = GetHeightTriangulated(Get4ZUnits(nRect.x, nRect.w), fRect.x, fRect.w);
		zs[3] = GetHeightTriangulated(Get4ZUnits(nRect.z, nRect.w), fRect.z, fRect.w);
	}

	Vec4 nz(
		Vec3(zs[0] + zs[2] - zs[1] - zs[3], zs[0] + zs[1] - zs[2] - zs[3], size * 4.0f).GetNormalizedFast(), // normal
		(zs[0] + zs[1] + zs[2] + zs[3]) * 0.25f); // height
	return nz;
}

float CTerrain::GetZMax(float x0, float y0, float x1, float y1) const
{
	// Convert to grid units.
	int nGridSize = int(CTerrain::GetTerrainSize() * CTerrain::GetHeightMapUnitSizeInverted());
	int nX0 = clamp_tpl(int(x0 * CTerrain::GetInvUnitSize()), 0, nGridSize - 1);
	int nY0 = clamp_tpl(int(y0 * CTerrain::GetInvUnitSize()), 0, nGridSize - 1);
	int nX1 = clamp_tpl(int_ceil(x1 * CTerrain::GetInvUnitSize()), 0, nGridSize - 1);
	int nY1 = clamp_tpl(int_ceil(y1 * CTerrain::GetInvUnitSize()), 0, nGridSize - 1);

	return GetZMaxFromUnits(nX0, nY0, nX1, nY1);
}

bool CTerrain::RayTrace(Vec3 const& vStart, Vec3 const& vEnd, SRayTrace* prt, bool bClampAbove)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!GetParentNode() || m_arrSecInfoPyramid.empty())
		return false;

	// Temp storage to avoid tests.
	SRayTrace s_rt;
	SRayTrace& rt = prt ? *prt : s_rt;

	float unitSize = (float)GetHeightMapUnitSize();
	float invUnitSize = GetInvUnitSize();
	int nGridSize = (int)(GetTerrainSize() * invUnitSize);

	// Convert to grid units.
	Vec3 vDelta = vEnd - vStart;
	float fMinZ = min(vStart.z, vEnd.z);

	int nX = clamp_tpl(int(vStart.x * invUnitSize), 0, nGridSize - 1);
	int nY = clamp_tpl(int(vStart.y * invUnitSize), 0, nGridSize - 1);

	int nEndX = clamp_tpl(int(vEnd.x * invUnitSize), 0, nGridSize - 1);
	int nEndY = clamp_tpl(int(vEnd.y * invUnitSize), 0, nGridSize - 1);

	for (;; )
	{
		// Get heightmap node for current point.
		CTerrainNode const* pNode = GetSecInfoUnits(nX, nY);

		int nStepUnits = 1;

		// When entering new node, check with bounding box.
		if (pNode && pNode->m_rangeInfo.pHMData && fMinZ <= pNode->m_boxHeigtmapLocal.max.z)
		{
			SRangeInfo const& ri = pNode->m_rangeInfo;

			// Evaluate individual sectors.
			assert((1 << (m_nUnitsToSectorBitShift - ri.nUnitBitShift)) == ri.nSize - 1);

			// Get cell for starting point.
			SSurfaceTypeLocal si;
			SSurfaceTypeLocal::DecodeFromUint32(ri.GetDataUnits(nX, nY).surface, si);
			int nType = si.GetDominatingSurfaceType() & SRangeInfo::e_index_hole;
			if (nType != SRangeInfo::e_index_hole)
			{
				// Get cell vertex values.
				Vec4 vZ = Get4HeightsUnits(*pNode, nX, nY);

				// Further zmin check.
				if (fMinZ <= vZ[0] || fMinZ <= vZ[1] || fMinZ <= vZ[2] || fMinZ <= vZ[3])
				{
					if (prt)
					{
						IMaterial* pMat = GetSurfaceTypes()[nType].pLayerMat;
						prt->pMaterial = pMat;
						if (pMat && pMat->GetSubMtlCount() > 2)
							prt->pMaterial = pMat->GetSubMtl(2);
					}

					// Select common point on both tris.
					float fX0 = nX * unitSize;
					float fY0 = nY * unitSize;
					Vec3 vEndRel = vEnd - Vec3(fX0 + unitSize, fY0, vZ[1]);

					//
					// Intersect with bottom triangle.
					//
					Vec3 vTriDir1(vZ[0] - vZ[1], vZ[0] - vZ[2], unitSize);
					float fET1 = vEndRel * vTriDir1;
					if (fET1 < 0.f)
					{
						// End point below plane. Find intersection time.
						float fDT = vDelta * vTriDir1;
						if (fDT <= fET1)
						{
							rt.fInterp = 1.f - fET1 / fDT;
							rt.vHit = vStart + vDelta * rt.fInterp;

							// Check against tri boundaries.
							if (rt.vHit.x >= fX0 && rt.vHit.y >= fY0 && rt.vHit.x + rt.vHit.y <= fX0 + fY0 + unitSize)
							{
								rt.vNorm = vTriDir1.GetNormalized();
								return true;
							}
						}
					}

					//
					// Intersect with top triangle.
					//
					Vec3 vTriDir2(vZ[2] - vZ[3], vZ[1] - vZ[3], unitSize);
					float fET2 = vEndRel * vTriDir2;
					if (fET2 < 0.f)
					{
						// End point below plane. Find intersection time.
						float fDT = vDelta * vTriDir2;
						if (fDT <= fET2)
						{
							rt.fInterp = 1.f - fET2 / fDT;
							rt.vHit = vStart + vDelta * rt.fInterp;

							// Check against tri boundaries.
							if (rt.vHit.x <= fX0 + unitSize && rt.vHit.y <= fY0 + unitSize && rt.vHit.x + rt.vHit.y >= fX0 + fY0 + unitSize)
							{
								rt.vNorm = vTriDir2.GetNormalized();
								return true;
							}
						}
					}

					// Check for end point below terrain, to correct for leaks.
					if (bClampAbove && nX == nEndX && nY == nEndY)
					{
						if (fET1 < 0.f)
						{
							// Lower tri.
							if (vEnd.x + vEnd.y <= fX0 + fY0 + unitSize)
							{
								rt.fInterp = 1.f;
								rt.vNorm = vTriDir1.GetNormalized();
								rt.vHit = vEnd;
								rt.vHit.z = vZ[0] - ((vEnd.x - fX0) * rt.vNorm.x + (vEnd.y - fY0) * rt.vNorm.y) / rt.vNorm.z;
								return true;
							}
						}
						if (fET2 < 0.f)
						{
							// Upper tri.
							if (vEnd.x + vEnd.y >= fX0 + fY0 + unitSize)
							{
								rt.fInterp = 1.f;
								rt.vNorm = vTriDir2.GetNormalized();
								rt.vHit = vEnd;
								rt.vHit.z = vZ[3] - ((vEnd.x - fX0 - unitSize) * rt.vNorm.x + (vEnd.y - fY0 - unitSize) * rt.vNorm.y) / rt.vNorm.z;
								return true;
							}
						}
					}
				}
			}
		}
		else
		{
			// Skip entire node.
			nStepUnits = 1 << (m_nUnitsToSectorBitShift + 0 /*pNode->m_nTreeLevel*/);
			assert(!pNode || nStepUnits == int(pNode->m_boxHeigtmapLocal.max.x - pNode->m_boxHeigtmapLocal.min.x) * invUnitSize);
			assert(!pNode || pNode->m_nTreeLevel == 0);
		}

		// Step out of cell.
		int nX1 = nX & ~(nStepUnits - 1);
		if (vDelta.x >= 0.f)
			nX1 += nStepUnits;
		float fX = (float)(nX1 * GetHeightMapUnitSize());

		int nY1 = nY & ~(nStepUnits - 1);
		if (vDelta.y >= 0.f)
			nY1 += nStepUnits;
		float fY = (float)(nY1 * GetHeightMapUnitSize());

		if (abs((fX - vStart.x) * vDelta.y) < abs((fY - vStart.y) * vDelta.x))
		{
			if (fX * vDelta.x >= vEnd.x * vDelta.x)
				break;
			if (nX1 > nX)
			{
				nX = nX1;
				if (nX >= nGridSize)
					break;
			}
			else
			{
				nX = nX1 - 1;
				if (nX < 0)
					break;
			}
		}
		else
		{
			if (fY * vDelta.y >= vEnd.y * vDelta.y)
				break;
			if (nY1 > nY)
			{
				nY = nY1;
				if (nY >= nGridSize)
					break;
			}
			else
			{
				nY = nY1 - 1;
				if (nY < 0)
					break;
			}
		}
	}

	return false;
}

bool CTerrain::IntersectWithHeightMap(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip)
{
	// FUNCTION_PROFILER_3DENGINE;
	// convert into hmap space
	float fInvUnitSize = GetInvUnitSize();
	vStopPoint.x *= fInvUnitSize;
	vStopPoint.y *= fInvUnitSize;
	vStartPoint.x *= fInvUnitSize;
	vStartPoint.y *= fInvUnitSize;
	fDist *= fInvUnitSize;

	int heightMapSize = int(GetTerrainSize() * GetHeightMapUnitSizeInverted());

	// clamp points
	const bool cClampStart = (vStartPoint.x < 0) | (vStartPoint.y < 0) | (vStartPoint.x > heightMapSize) | (vStartPoint.y > heightMapSize);
	if (cClampStart)
	{
		const float fHMSize = (float)heightMapSize;
		AABB boxHM(Vec3(0.f, 0.f, 0.f), Vec3(fHMSize, fHMSize, fHMSize));

		Lineseg ls;
		ls.start = vStartPoint;
		ls.end = vStopPoint;

		Vec3 vRes;
		if (Intersect::Lineseg_AABB(ls, boxHM, vRes) == 0x01)
			vStartPoint = vRes;
		else
			return false;
	}

	const bool cClampStop = (vStopPoint.x < 0) | (vStopPoint.y < 0) | (vStopPoint.x > heightMapSize) | (vStopPoint.y > heightMapSize);
	if (cClampStop)
	{
		const float fHMSize = (float)heightMapSize;
		AABB boxHM(Vec3(0.f, 0.f, 0.f), Vec3(fHMSize, fHMSize, fHMSize));

		Lineseg ls;
		ls.start = vStopPoint;
		ls.end = vStartPoint;

		Vec3 vRes;
		if (Intersect::Lineseg_AABB(ls, boxHM, vRes) == 0x01)
			vStopPoint = vRes;
		else
			return false;
	}

	Vec3 vDir = (vStopPoint - vStartPoint);

	int nSteps = fastftol_positive(fDist / GetCVars()->e_TerrainOcclusionCullingStepSize); // every 4 units
	if (nSteps != 0)
	{
		switch (GetCVars()->e_TerrainOcclusionCulling)
		{
		case 4:                     // far objects are culled less precise but with far hills as well (same culling speed)
			if (nSteps > GetCVars()->e_TerrainOcclusionCullingMaxSteps)
				nSteps = GetCVars()->e_TerrainOcclusionCullingMaxSteps;
			vDir /= (float)nSteps;
			break;
		default:                      // far hills are not culling
			vDir /= (float)nSteps;
			if (nSteps > GetCVars()->e_TerrainOcclusionCullingMaxSteps)
				nSteps = GetCVars()->e_TerrainOcclusionCullingMaxSteps;
			break;
		}
	}

	// scan hmap in sector

	Vec3 vPos = vStartPoint;

	int nTest = 0;

	for (nTest = 0; nTest < nSteps && nTest < nMaxTestsToScip; nTest++)
	{
		// leave the underground first
		if (IsPointUnderGround(fastround_positive(vPos.x), fastround_positive(vPos.y), vPos.z))
			vPos += vDir;
		else
			break;
	}

	nMaxTestsToScip = min(nMaxTestsToScip, 4);

	for (; nTest < nSteps - nMaxTestsToScip; nTest++)
	{
		vPos += vDir;
		if (IsPointUnderGround(fastround_positive(vPos.x), fastround_positive(vPos.y), vPos.z))
			return true;
	}

	return false;
}

bool CTerrain::GetHole(float x, float y) const
{
	int nX_units = int(float(x) * GetInvUnitSize());
	int nY_units = int(float(y) * GetInvUnitSize());
	int nTerrainSize_units = int((float(GetTerrainSize()) * GetInvUnitSize()));

	if (nX_units < 0 || nX_units >= nTerrainSize_units || nY_units < 0 || nY_units >= nTerrainSize_units)
		return true;

	return GetSurfTypeFromUnits(nX_units, nY_units) == SRangeInfo::e_hole;
}

float CTerrain::GetZSafe(float x, float y)
{
	if (x >= 0 && y >= 0 && x < GetTerrainSize() && y < GetTerrainSize())
		return max(GetZ(x, y), (float)TERRAIN_BOTTOM_LEVEL);

	return TERRAIN_BOTTOM_LEVEL;
}

uint8 CTerrain::GetSurfaceTypeID(float x, float y) const
{
	x = crymath::clamp<float>(x, 0, (float)GetTerrainSize());
	y = crymath::clamp<float>(y, 0, (float)GetTerrainSize());
	
	return GetSurfTypeFromUnits(int(x * GetInvUnitSize()), int(y * GetInvUnitSize()));
}

SSurfaceTypeItem CTerrain::GetSurfaceTypeItem(float x, float y) const
{
	x = crymath::clamp<float>(x, 0, (float)GetTerrainSize());
	y = crymath::clamp<float>(y, 0, (float)GetTerrainSize());

	return GetSurfTypeItemfromUnits(int(x * GetInvUnitSize()), int(y * GetInvUnitSize()));
}

float CTerrain::GetZ(float x, float y, bool bUpdatePos /*= false*/) const
{
	if (!GetParentNode())
		return TERRAIN_BOTTOM_LEVEL;

	return GetZfromUnits(int(x * GetInvUnitSize()), int(y * GetInvUnitSize()));
}

void CTerrain::SetZ(const float x, const float y, float fHeight)
{
	assert(0); //TODO: NOT CURRENTLY SUPPORTED DUE TO NEW INTEGER DATA IMPLEMENTATION
}

uint8 CTerrain::GetSurfTypeFromUnits(int nX_units, int nY_units) const
{
	ClampUnits(nX_units, nY_units);
	const CTerrainNode* pNode = GetSecInfoUnitsSafe(nX_units, nY_units);
	if (pNode)
	{
		const SRangeInfo& ri = pNode->m_rangeInfo;

		if (ri.pHMData && ri.pSTPalette)
		{
			SSurfaceTypeLocal si;
			SSurfaceTypeLocal::DecodeFromUint32(ri.GetDataUnits(nX_units, nY_units).surface, si);
			int nType = si.GetDominatingSurfaceType() & SRangeInfo::e_index_hole;

			return ri.pSTPalette[nType];
		}
	}
	return SRangeInfo::e_undefined;
}

SSurfaceTypeItem CTerrain::GetSurfTypeItemfromUnits(int x, int y) const
{
	ClampUnits(x, y);
	const CTerrainNode* pNode = GetSecInfoUnitsSafe(x, y);
	if (pNode)
	{
		const SRangeInfo& ri = pNode->m_rangeInfo;

		if (ri.pHMData && ri.pSTPalette)
		{
			SSurfaceTypeLocal si;
			SSurfaceTypeLocal::DecodeFromUint32(ri.GetDataUnits(x, y).surface, si);

			assert(si.ty[1] < 127);

			SSurfaceTypeItem es;

			for (int s = 0; s < SSurfaceTypeLocal::kMaxSurfaceTypesNum; s++)
			{
				if (si.we[s])
				{
					es.ty[s] = ri.pSTPalette[si.ty[s]];
					es.we[s] = si.we[s];
				}
			}

			es.SetHole(si.GetDominatingSurfaceType() == SRangeInfo::e_index_hole);

			return es;
		}
	}
	else
	{
		assert(!"Sector not found");
	}

	SSurfaceTypeItem st;
	return st;
}

float CTerrain::GetZfromUnits(int nX_units, int nY_units) const
{
	ClampUnits(nX_units, nY_units);
	CTerrainNode* pNode = GetSecInfoUnitsSafe(nX_units, nY_units);
	if (pNode)
	{
		SRangeInfo& ri = pNode->m_rangeInfo;
		if (ri.pHMData)
		{
			return GetHeightUnits(ri, nX_units, nY_units, m_nUnitsToSectorBitShift);
		}
	}
	return 0;
}

float CTerrain::GetZMaxFromUnits(int nX0_units, int nY0_units, int nX1_units, int nY1_units) const
{
	Array2d<struct CTerrainNode*>& sectorLayer = m_arrSecInfoPyramid[0];
	int nLocalMask = (1 << m_nUnitsToSectorBitShift) - 1;
#if defined(USE_CRY_ASSERT)
	int nSizeXY = int(Cry3DEngineBase::GetTerrain()->GetTerrainSize() * CTerrain::GetInvUnitSize());
#endif
	assert(nX0_units <= nSizeXY && nY0_units <= nSizeXY);
	assert(nX1_units <= nSizeXY && nY1_units <= nSizeXY);

	float fZMax = TERRAIN_BOTTOM_LEVEL;

	// Iterate sectors.
	int nX0_sector = nX0_units >> m_nUnitsToSectorBitShift,
	    nX1_sector = nX1_units >> m_nUnitsToSectorBitShift,
	    nY0_sector = nY0_units >> m_nUnitsToSectorBitShift,
	    nY1_sector = nY1_units >> m_nUnitsToSectorBitShift;
	for (int nX_sector = nX0_sector; nX_sector <= nX1_sector; nX_sector++)
	{
		for (int nY_sector = nY0_sector; nY_sector <= nY1_sector; nY_sector++)
		{
			CTerrainNode& node = *sectorLayer[nX_sector][nY_sector];
			SRangeInfo& ri = node.m_rangeInfo;
			if (!ri.pHMData)
				continue;

			assert((1 << (m_nUnitsToSectorBitShift - ri.nUnitBitShift)) == ri.nSize - 1);

			// Iterate points in sector.
			int nX0_pt = (nX_sector == nX0_sector ? nX0_units & nLocalMask : 0) >> ri.nUnitBitShift;
			int nX1_pt = (nX_sector == nX1_sector ? nX1_units & nLocalMask : nLocalMask) >> ri.nUnitBitShift;
			int nY0_pt = (nY_sector == nY0_sector ? nY0_units & nLocalMask : 0) >> ri.nUnitBitShift;
			int nY1_pt = (nY_sector == nY1_sector ? nY1_units & nLocalMask : nLocalMask) >> ri.nUnitBitShift;

			float fSectorZMax;
			if ((nX1_pt - nX0_pt + 1) * (nY1_pt - nY0_pt + 1) >= (int)(ri.nSize - 1) * (ri.nSize - 1))
			{
				fSectorZMax = node.m_boxHeigtmapLocal.max.z;
			}
			else
			{
				int sectorZMax = 0;
				for (int nX_pt = nX0_pt; nX_pt <= nX1_pt; nX_pt++)
				{
					for (int nY_pt = nY0_pt; nY_pt <= nY1_pt; nY_pt++)
					{
						int nCellLocal = nX_pt * ri.nSize + nY_pt;
						assert(nCellLocal >= 0 && nCellLocal < ri.nSize * ri.nSize);
						int height = ri.GetRawDataByIndex(nCellLocal).height;
						sectorZMax = max(sectorZMax, height);
					}
				}
				fSectorZMax = ri.fOffset + sectorZMax * ri.fRange;
			}

			fZMax = max(fZMax, fSectorZMax);
		}
	}
	return fZMax;
}

bool CTerrain::IsMeshQuadFlipped(const float x, const float y, const float unitSize) const
{
	bool bFlipped = false;

	// Flip winding order to prevent surface type interpolation over long edges
	int nType10 = GetSurfaceTypeID(x + unitSize, y);
	int nType01 = GetSurfaceTypeID(x, y + unitSize);

	if (nType10 != nType01)
	{
		int nType00 = GetSurfaceTypeID(x, y);
		int nType11 = GetSurfaceTypeID(x + unitSize, y + unitSize);

		if ((nType10 == nType00 && nType10 == nType11)
		    || (nType01 == nType00 && nType01 == nType11))
		{
			bFlipped = true;
		}
	}

	return bFlipped;
}

bool CTerrain::IsPointUnderGround(int nX_units, int nY_units, float fTestZ)
{
	//  FUNCTION_PROFILER_3DENGINE;

	if (GetCVars()->e_TerrainOcclusionCullingDebug)
	{
		Vec3 vTerPos(0, 0, 0);
		vTerPos.Set((float)(nX_units * GetHeightMapUnitSize()), (float)(nY_units * GetHeightMapUnitSize()), 0);
		vTerPos.z = GetZfromUnits(nX_units, nY_units);
		DrawSphere(vTerPos, 1.f, Col_Red);
	}

	ClampUnits(nX_units, nY_units);
	CTerrainNode* pNode = GetSecInfoUnitsSafe(nX_units, nY_units);
	if (!pNode)
		return false;

	SRangeInfo& ri = pNode->m_rangeInfo;

	if (!ri.pHMData || (pNode->m_bNoOcclusion != 0) || (pNode->m_bHasHoles != 0))
		return false;

	if (fTestZ < ri.fOffset)
		return true;

	float fZ = GetHeightUnits(ri, nX_units, nY_units, m_nUnitsToSectorBitShift);

	return fTestZ < fZ;
}

CTerrain::SCachedHeight CTerrain::m_arrCacheHeight[nHMCacheSize * nHMCacheSize];
CTerrain::SCachedSurfType CTerrain::m_arrCacheSurfType[nHMCacheSize * nHMCacheSize];

float CTerrain::GetHeightFromUnits_Callback(int ix, int iy)
{
	const uint32 idx = encodeby1(ix & ((nHMCacheSize - 1))) | (encodeby1(iy & ((nHMCacheSize - 1))) << 1);
	SCachedHeight& rCache = m_arrCacheHeight[idx];

	// Get copy of the cached value
	SCachedHeight cacheCopy(rCache);
	if (cacheCopy.x == ix && cacheCopy.y == iy)
		return cacheCopy.fHeight;

	assert(sizeof(m_arrCacheHeight[0]) == 8);

	cacheCopy.x = ix;
	cacheCopy.y = iy;
	cacheCopy.fHeight = Cry3DEngineBase::GetTerrain() ? Cry3DEngineBase::GetTerrain()->GetZfromUnits(ix, iy) : 0.0f;

	// Update cache by new value
	rCache.packedValue = cacheCopy.packedValue;

	return cacheCopy.fHeight;
}

uint8 CTerrain::GetSurfaceTypeFromUnits_Callback(int ix, int iy)
{
	const uint32 idx = encodeby1(ix & ((nHMCacheSize - 1))) | (encodeby1(iy & ((nHMCacheSize - 1))) << 1);
	SCachedSurfType& rCache = m_arrCacheSurfType[idx];

	// Get copy of the cached value
	SCachedSurfType cacheCopy(rCache);
	if (cacheCopy.x == ix && cacheCopy.y == iy)
		return cacheCopy.surfType;

	//  nBad++;

	cacheCopy.x = ix;
	cacheCopy.y = iy;
	cacheCopy.surfType = Cry3DEngineBase::GetTerrain() ? Cry3DEngineBase::GetTerrain()->GetSurfTypeFromUnits(ix, iy) : 0;

	// Update cache by new value
	rCache.packedValue = cacheCopy.packedValue;

	return cacheCopy.surfType;
}

void CTerrain::ResetHeightMapCache()
{
	memset(m_arrCacheHeight, 0, sizeof(m_arrCacheHeight));
	assert(sizeof(m_arrCacheHeight[0]) == 8);
	memset(m_arrCacheSurfType, 0, sizeof(m_arrCacheSurfType));
	assert(sizeof(m_arrCacheSurfType[0]) == 8);
}
