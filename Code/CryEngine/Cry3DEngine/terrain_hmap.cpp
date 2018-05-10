// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_hmap.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: highmap
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"

namespace
{
static inline uint32 encodeby1(uint32 n)
{
	n &= 0x0000ffff;
	n = (n | (n << 8)) & 0x00FF00FF;
	n = (n | (n << 4)) & 0x0F0F0F0F;
	n = (n | (n << 2)) & 0x33333333;
	n = (n | (n << 1)) & 0x55555555;
	return n;
}
}

/* //TODO: NOT CURRENTLY SUPPORTED DUE TO NEW INTEGER DATA IMPLEMENTATION
   static ILINE void SetHeightLocal( SRangeInfo & ri, int nX, int nY, float fHeight )
   {
   float fCompHeight = (ri.fRange>(0.01f/65535.f)) ? ((fHeight - ri.fOffset)/ri.fRange) : 0;
   uint16 usNewZValue = ((uint16)CLAMP((int)fCompHeight, 0, 65535)) & ~STYPE_BIT_MASK;
   uint16 usOldSurfTypes = ri.GetRawDataLocal(nX, nY) & STYPE_BIT_MASK;
   ri.SetDataLocal(nX, nY, usNewZValue | usOldSurfTypes);
   }
 */
static ILINE float GetHeightLocal(SRangeInfo const& ri, int nX, int nY, float fX, float fY)
{
	float fZ = ri.GetHeight(nX, nY) * (1.f - fX) * (1.f - fY)
	           + ri.GetHeight(nX + 1, nY) * fX * (1.f - fY)
	           + ri.GetHeight(nX, nY + 1) * (1.f - fX) * fY
	           + ri.GetHeight(nX + 1, nY + 1) * fX * fY;

	return fZ;
}

/*  //TODO: NOT CURRENTLY SUPPORTED DUE TO NEW INTEGER DATA IMPLEMENTATION
   static ILINE void SetHeightUnits( SRangeInfo & ri, int nX_units, int nY_units, float fHeight )
   {
   ri.nModified = true;

   int nMask = ri.nSize-2;
   if (ri.nUnitBitShift == 0)
   {
    // full lod is available
    SetHeightLocal(ri, nX_units&nMask, nY_units&nMask, fHeight);
   }
   }
 */
static float GetHeightUnits(SRangeInfo const& ri, int nX_units, int nY_units, int nUnitToSectorBS)
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
		assert(fX + fInvStep <= 1.f);

		int nY = nY_units >> ri.nUnitBitShift;
		float fY = (nY_units * fInvStep) - nY;
		assert(fY + fInvStep <= 1.f);

		return GetHeightLocal(ri, nX & nMask, nY & nMask, fX, fY);
	}
}

static void Get4HeightsUnits(SRangeInfo const& ri, int nX_units, int nY_units, float afZ[4], int nUnitToSectorBS, int nSectorSizeUnits)
{
	int nMask = ri.nSize - 2;
	if (ri.nUnitBitShift == 0)
	{
		// full lod is available
		nX_units &= nMask;
		nY_units &= nMask;
		afZ[0] = ri.GetHeight(nX_units, nY_units);
		afZ[1] = ri.GetHeight(nX_units + 1, nY_units);
		afZ[2] = ri.GetHeight(nX_units, nY_units + 1);
		afZ[3] = ri.GetHeight(nX_units + 1, nY_units + 1);
	}
	else
	{
		float fInvStep = (ri.nSize > 1) ? (1.f / (nSectorSizeUnits / (ri.nSize - 1))) : 1.f;

		// interpolate
		int nX = nX_units >> ri.nUnitBitShift;
		float fX = (nX_units * fInvStep) - nX;
		assert(fX + fInvStep <= 1.f);

		int nY = nY_units >> ri.nUnitBitShift;
		float fY = (nY_units * fInvStep) - nY;
		assert(fY + fInvStep <= 1.f);

		nX &= nMask;
		nY &= nMask;
		afZ[0] = GetHeightLocal(ri, nX, nY, fX, fY);
		afZ[1] = GetHeightLocal(ri, nX, nY, fX + fInvStep, fY);
		afZ[2] = GetHeightLocal(ri, nX, nY, fX, fY + fInvStep);
		afZ[3] = GetHeightLocal(ri, nX, nY, fX + fInvStep, fY + fInvStep);
	}
}

float CHeightMap::GetZApr(float xWS, float yWS) const
{
	if (!Cry3DEngineBase::GetTerrain()->GetParentNode())
		return TERRAIN_BOTTOM_LEVEL;

	float fZ;

	// convert into hmap space
	float x1 = xWS * CTerrain::GetInvUnitSize();
	float y1 = yWS * CTerrain::GetInvUnitSize();

	if (!Cry3DEngineBase::GetTerrain() || x1 < 1 || y1 < 1)
		return TERRAIN_BOTTOM_LEVEL;

	int nX = fastftol_positive(x1);
	int nY = fastftol_positive(y1);

	int heightMapSize = int(CTerrain::GetTerrainSize() * CTerrain::GetHeightMapUnitSizeInverted());

	if (!Cry3DEngineBase::GetTerrain() || nX < 1 || nY < 1 || nX >= heightMapSize || nY >= heightMapSize)
	{
		fZ = TERRAIN_BOTTOM_LEVEL;
	}
	else
	{
		float dx1 = x1 - nX;
		float dy1 = y1 - nY;

		float afZCorners[4];

		const CTerrainNode* pNode = Cry3DEngineBase::GetTerrain()->GetSecInfoUnits(nX, nY);

		if (pNode && pNode->m_rangeInfo.pHMData)
		{
			Get4HeightsUnits(pNode->m_rangeInfo, nX, nY, afZCorners, GetTerrain()->m_nUnitsToSectorBitShift, pNode->GetSectorSizeInHeightmapUnits());

			if (dx1 + dy1 < 1.f)
			{
				// Lower triangle.
				fZ = afZCorners[0] * (1.f - dx1 - dy1)
				     + afZCorners[1] * dx1
				     + afZCorners[2] * dy1;
			}
			else
			{
				// Upper triangle.
				fZ = afZCorners[3] * (dx1 + dy1 - 1.f)
				     + afZCorners[2] * (1.f - dx1)
				     + afZCorners[1] * (1.f - dy1);
			}
			if (fZ < TERRAIN_BOTTOM_LEVEL)
				fZ = TERRAIN_BOTTOM_LEVEL;
		}
		else
			fZ = TERRAIN_BOTTOM_LEVEL;
	}
	return fZ;
}

float CHeightMap::GetZMax(float x0, float y0, float x1, float y1) const
{
	// Convert to grid units.
	int nGridSize = int(CTerrain::GetTerrainSize() * CTerrain::GetHeightMapUnitSizeInverted());
	int nX0 = clamp_tpl(int(x0 * CTerrain::GetInvUnitSize()), 0, nGridSize - 1);
	int nY0 = clamp_tpl(int(y0 * CTerrain::GetInvUnitSize()), 0, nGridSize - 1);
	int nX1 = clamp_tpl(int_ceil(x1 * CTerrain::GetInvUnitSize()), 0, nGridSize - 1);
	int nY1 = clamp_tpl(int_ceil(y1 * CTerrain::GetInvUnitSize()), 0, nGridSize - 1);

	return GetZMaxFromUnits(nX0, nY0, nX1, nY1);
}

bool CHeightMap::RayTrace(Vec3 const& vStart, Vec3 const& vEnd, SRayTrace* prt, bool bClampAbove)
{
	FUNCTION_PROFILER_3DENGINE;

	CTerrain* pTerrain = GetTerrain();

	if (!pTerrain->GetParentNode())
		return false;

	// Temp storage to avoid tests.
	SRayTrace s_rt;
	SRayTrace& rt = prt ? *prt : s_rt;

	float unitSize = (float)CTerrain::GetHeightMapUnitSize();
	float invUnitSize = CTerrain::GetInvUnitSize();
	int nGridSize = (int)(CTerrain::GetTerrainSize() * invUnitSize);

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
		CTerrainNode const* pNode = pTerrain->GetSecInfoUnits(nX, nY);

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
				float afZ[4];
				Get4HeightsUnits(ri, nX, nY, afZ, pTerrain->m_nUnitsToSectorBitShift, pNode->GetSectorSizeInHeightmapUnits());

				// Further zmin check.
				if (fMinZ <= afZ[0] || fMinZ <= afZ[1] || fMinZ <= afZ[2] || fMinZ <= afZ[3])
				{
					if (prt)
					{
						IMaterial* pMat = pTerrain->GetSurfaceTypes()[nType].pLayerMat;
						;
						prt->pMaterial = pMat;
						if (pMat && pMat->GetSubMtlCount() > 2)
							prt->pMaterial = pMat->GetSubMtl(2);
					}

					// Select common point on both tris.
					float fX0 = nX * unitSize;
					float fY0 = nY * unitSize;
					Vec3 vEndRel = vEnd - Vec3(fX0 + unitSize, fY0, afZ[1]);

					//
					// Intersect with bottom triangle.
					//
					Vec3 vTriDir1(afZ[0] - afZ[1], afZ[0] - afZ[2], unitSize);
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
					Vec3 vTriDir2(afZ[2] - afZ[3], afZ[1] - afZ[3], unitSize);
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
								rt.vHit.z = afZ[0] - ((vEnd.x - fX0) * rt.vNorm.x + (vEnd.y - fY0) * rt.vNorm.y) / rt.vNorm.z;
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
								rt.vHit.z = afZ[3] - ((vEnd.x - fX0 - unitSize) * rt.vNorm.x + (vEnd.y - fY0 - unitSize) * rt.vNorm.y) / rt.vNorm.z;
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
		float fX = (float)(nX1 * CTerrain::GetHeightMapUnitSize());

		int nY1 = nY & ~(nStepUnits - 1);
		if (vDelta.y >= 0.f)
			nY1 += nStepUnits;
		float fY = (float)(nY1 * CTerrain::GetHeightMapUnitSize());

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

bool CHeightMap::IntersectWithHeightMap(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip)
{
	// FUNCTION_PROFILER_3DENGINE;
	// convert into hmap space
	float fInvUnitSize = CTerrain::GetInvUnitSize();
	vStopPoint.x *= fInvUnitSize;
	vStopPoint.y *= fInvUnitSize;
	vStartPoint.x *= fInvUnitSize;
	vStartPoint.y *= fInvUnitSize;
	fDist *= fInvUnitSize;

	int heightMapSize = int(CTerrain::GetTerrainSize() * CTerrain::GetHeightMapUnitSizeInverted());

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

	CTerrain* const __restrict pTerrain = Cry3DEngineBase::GetTerrain();
	const int nUnitsToSectorBitShift = m_nUnitsToSectorBitShift;
	for (nTest = 0; nTest < nSteps && nTest < nMaxTestsToScip; nTest++)
	{
		// leave the underground first
		if (IsPointUnderGround(pTerrain, nUnitsToSectorBitShift, fastround_positive(vPos.x), fastround_positive(vPos.y), vPos.z))
			vPos += vDir;
		else
			break;
	}

	nMaxTestsToScip = min(nMaxTestsToScip, 4);

	for (; nTest < nSteps - nMaxTestsToScip; nTest++)
	{
		vPos += vDir;
		if (IsPointUnderGround(pTerrain, nUnitsToSectorBitShift, fastround_positive(vPos.x), fastround_positive(vPos.y), vPos.z))
			return true;
	}

	return false;
}

bool CHeightMap::GetHole(float x, float y) const
{
	int nX_units = int(float(x) * CTerrain::GetInvUnitSize());
	int nY_units = int(float(y) * CTerrain::GetInvUnitSize());
	int nTerrainSize_units = int((float(CTerrain::GetTerrainSize()) * CTerrain::GetInvUnitSize()));

	if (nX_units < 0 || nX_units >= nTerrainSize_units || nY_units < 0 || nY_units >= nTerrainSize_units)
		return true;

	return GetSurfTypeFromUnits(nX_units, nY_units) == SRangeInfo::e_hole;
}

float CHeightMap::GetZSafe(float x, float y)
{
	if (x >= 0 && y >= 0 && x < CTerrain::GetTerrainSize() && y < CTerrain::GetTerrainSize())
		return max(GetZ(x, y), (float)TERRAIN_BOTTOM_LEVEL);

	return TERRAIN_BOTTOM_LEVEL;
}

uint8 CHeightMap::GetSurfaceTypeID(float x, float y) const
{
	x = crymath::clamp<float>(x, 0, (float)CTerrain::GetTerrainSize());
	y = crymath::clamp<float>(y, 0, (float)CTerrain::GetTerrainSize());
	
	return GetSurfTypeFromUnits(int(x * CTerrain::GetInvUnitSize()), int(y * CTerrain::GetInvUnitSize()));
}

SSurfaceTypeItem CHeightMap::GetSurfaceTypeItem(float x, float y) const
{
	x = crymath::clamp<float>(x, 0, (float)CTerrain::GetTerrainSize());
	y = crymath::clamp<float>(y, 0, (float)CTerrain::GetTerrainSize());

	return GetSurfTypeItemfromUnits(int(x * CTerrain::GetInvUnitSize()), int(y * CTerrain::GetInvUnitSize()));
}

float CHeightMap::GetZ(float x, float y, bool bUpdatePos /*= false*/) const
{
	if (!Cry3DEngineBase::GetTerrain()->GetParentNode())
		return TERRAIN_BOTTOM_LEVEL;

	return GetZfromUnits(int(x * CTerrain::GetInvUnitSize()), int(y * CTerrain::GetInvUnitSize()));
}

void CHeightMap::SetZ(const float x, const float y, float fHeight)
{
	return SetZfromUnits(int(x * CTerrain::GetInvUnitSize()), int(y * CTerrain::GetInvUnitSize()), fHeight);
}

uint8 CHeightMap::GetSurfTypeFromUnits(uint32 nX_units, uint32 nY_units) const
{
	if (CTerrain* pTerrain = Cry3DEngineBase::GetTerrain())
	{
		pTerrain->ClampUnits(nX_units, nY_units);
		const CTerrainNode* pNode = pTerrain->GetSecInfoUnits(nX_units, nY_units);
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
	}
	return SRangeInfo::e_undefined;
}

SSurfaceTypeItem CHeightMap::GetSurfTypeItemfromUnits(uint32 x, uint32 y) const
{
	if (CTerrain* pTerrain = Cry3DEngineBase::GetTerrain())
	{
		pTerrain->ClampUnits(x, y);
		const CTerrainNode* pNode = pTerrain->GetSecInfoUnits(x, y);
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
	}

	SSurfaceTypeItem st;
	return st;
}

float CHeightMap::GetZfromUnits(uint32 nX_units, uint32 nY_units) const
{
	if (CTerrain* pTerrain = Cry3DEngineBase::GetTerrain())
	{
		pTerrain->ClampUnits(nX_units, nY_units);
		CTerrainNode* pNode = pTerrain->GetSecInfoUnits(nX_units, nY_units);
		if (pNode)
		{
			SRangeInfo& ri = pNode->m_rangeInfo;
			if (ri.pHMData)
			{
				return GetHeightUnits(ri, nX_units, nY_units, pTerrain->m_nUnitsToSectorBitShift);
			}
		}
	}
	return 0;
}

void CHeightMap::SetZfromUnits(uint32 nX_units, uint32 nY_units, float fHeight)
{
	if (!Cry3DEngineBase::GetTerrain())
		return;

	assert(0); //TODO: NOT CURRENTLY SUPPORTED DUE TO NEW INTEGER DATA IMPLEMENTATION
	/*
	   Cry3DEngineBase::GetTerrain()->ClampUnits(nX_units, nY_units);
	   CTerrainNode * pNode = Cry3DEngineBase::GetTerrain()->GetSecInfoUnits(nX_units, nY_units);
	   if(!pNode)
	    return;
	   SRangeInfo& ri = pNode->m_rangeInfo;
	   if (!ri.pHMData)
	    return;

	   SetHeightUnits( ri, nX_units, nY_units, fHeight );*/
}

float CHeightMap::GetZMaxFromUnits(uint32 nX0_units, uint32 nY0_units, uint32 nX1_units, uint32 nY1_units) const
{
	if (!Cry3DEngineBase::GetTerrain())
		return TERRAIN_BOTTOM_LEVEL;

	Array2d<struct CTerrainNode*>& sectorLayer = Cry3DEngineBase::GetTerrain()->m_arrSecInfoPyramid[0];
	uint32 nLocalMask = (1 << m_nUnitsToSectorBitShift) - 1;

	uint32 nSizeXY = int(Cry3DEngineBase::GetTerrain()->GetTerrainSize() * CTerrain::GetInvUnitSize());
	assert(nX0_units <= nSizeXY && nY0_units <= nSizeXY);
	assert(nX1_units <= nSizeXY && nY1_units <= nSizeXY);

	float fZMax = TERRAIN_BOTTOM_LEVEL;

	// Iterate sectors.
	uint32 nX0_sector = nX0_units >> m_nUnitsToSectorBitShift,
	       nX1_sector = nX1_units >> m_nUnitsToSectorBitShift,
	       nY0_sector = nY0_units >> m_nUnitsToSectorBitShift,
	       nY1_sector = nY1_units >> m_nUnitsToSectorBitShift;
	for (uint32 nX_sector = nX0_sector; nX_sector <= nX1_sector; nX_sector++)
	{
		for (uint32 nY_sector = nY0_sector; nY_sector <= nY1_sector; nY_sector++)
		{
			CTerrainNode& node = *sectorLayer[nX_sector][nY_sector];
			SRangeInfo& ri = node.m_rangeInfo;
			if (!ri.pHMData)
				continue;

			assert((1 << (m_nUnitsToSectorBitShift - ri.nUnitBitShift)) == ri.nSize - 1);

			// Iterate points in sector.
			uint32 nX0_pt = (nX_sector == nX0_sector ? nX0_units & nLocalMask : 0) >> ri.nUnitBitShift;
			uint32 nX1_pt = (nX_sector == nX1_sector ? nX1_units & nLocalMask : nLocalMask) >> ri.nUnitBitShift;
			;
			uint32 nY0_pt = (nY_sector == nY0_sector ? nY0_units & nLocalMask : 0) >> ri.nUnitBitShift;
			;
			uint32 nY1_pt = (nY_sector == nY1_sector ? nY1_units & nLocalMask : nLocalMask) >> ri.nUnitBitShift;
			;

			float fSectorZMax;
			if ((nX1_pt - nX0_pt + 1) * (nY1_pt - nY0_pt + 1) >= (uint32)(ri.nSize - 1) * (ri.nSize - 1))
			{
				fSectorZMax = node.m_boxHeigtmapLocal.max.z;
			}
			else
			{
				uint32 sectorZMax = 0;
				for (uint32 nX_pt = nX0_pt; nX_pt <= nX1_pt; nX_pt++)
				{
					for (uint32 nY_pt = nY0_pt; nY_pt <= nY1_pt; nY_pt++)
					{
						int nCellLocal = nX_pt * ri.nSize + nY_pt;
						assert(nCellLocal >= 0 && nCellLocal < ri.nSize * ri.nSize);
						uint32 height = ri.GetRawDataByIndex(nCellLocal).height;
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

bool CHeightMap::IsMeshQuadFlipped(const float x, const float y, const float unitSize) const
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

bool CHeightMap::IsPointUnderGround(CTerrain* const __restrict pTerrain,
                                    int nUnitsToSectorBitShift,
                                    uint32 nX_units,
                                    uint32 nY_units,
                                    float fTestZ)
{
	//  FUNCTION_PROFILER_3DENGINE;

	if (GetCVars()->e_TerrainOcclusionCullingDebug)
	{
		Vec3 vTerPos(0, 0, 0);
		vTerPos.Set((float)(nX_units * CTerrain::GetHeightMapUnitSize()), (float)(nY_units * CTerrain::GetHeightMapUnitSize()), 0);
		vTerPos.z = pTerrain->GetZfromUnits(nX_units, nY_units);
		DrawSphere(vTerPos, 1.f, Col_Red);
	}

	pTerrain->ClampUnits(nX_units, nY_units);
	CTerrainNode* pNode = pTerrain->GetSecInfoUnits(nX_units, nY_units);
	if (!pNode)
		return false;

	SRangeInfo& ri = pNode->m_rangeInfo;

	if (!ri.pHMData || (pNode->m_bNoOcclusion != 0) || (pNode->m_bHasHoles != 0))
		return false;

	if (fTestZ < ri.fOffset)
		return true;

	float fZ = GetHeightUnits(ri, nX_units, nY_units, GetTerrain()->m_nUnitsToSectorBitShift);

	return fTestZ < fZ;
}

CHeightMap::SCachedHeight CHeightMap::m_arrCacheHeight[nHMCacheSize * nHMCacheSize];
CHeightMap::SCachedSurfType CHeightMap::m_arrCacheSurfType[nHMCacheSize * nHMCacheSize];

float CHeightMap::GetHeightFromUnits_Callback(int ix, int iy)
{
	const uint32 idx = encodeby1(ix & ((nHMCacheSize - 1))) | (encodeby1(iy & ((nHMCacheSize - 1))) << 1);
	CHeightMap::SCachedHeight& rCache = m_arrCacheHeight[idx];

	// Get copy of the cached value
	CHeightMap::SCachedHeight cacheCopy(rCache);
	if (cacheCopy.x == ix && cacheCopy.y == iy)
		return cacheCopy.fHeight;

	assert(sizeof(m_arrCacheHeight[0]) == 8);

	cacheCopy.x = ix;
	cacheCopy.y = iy;
	cacheCopy.fHeight = Cry3DEngineBase::GetTerrain()->GetZfromUnits(ix, iy);

	// Update cache by new value
	rCache.packedValue = cacheCopy.packedValue;

	return cacheCopy.fHeight;
}

unsigned char CHeightMap::GetSurfaceTypeFromUnits_Callback(int ix, int iy)
{
	/*
	   static int nAll=0;
	   static int nBad=0;

	   if(nAll && nAll/1000000*1000000 == nAll)
	   {
	   Error("SurfaceType_RealReads = %.2f", (float)nBad/nAll);
	   nAll=0;
	   nBad=0;

	   if(sizeof(m_arrCacheSurfType[0][0]) != 4)
	    Error("CHeightMap::GetSurfaceTypeFromUnits_Callback:  sizeof(m_arrCacheSurfType[0][0]) != 4");
	   }

	   nAll++;
	 */

	const uint32 idx = encodeby1(ix & ((nHMCacheSize - 1))) | (encodeby1(iy & ((nHMCacheSize - 1))) << 1);
	CHeightMap::SCachedSurfType& rCache = m_arrCacheSurfType[idx];

	// Get copy of the cached value
	CHeightMap::SCachedSurfType cacheCopy(rCache);
	if (cacheCopy.x == ix && cacheCopy.y == iy)
		return cacheCopy.surfType;

	//  nBad++;

	cacheCopy.x = ix;
	cacheCopy.y = iy;
	cacheCopy.surfType = Cry3DEngineBase::GetTerrain()->GetSurfTypeFromUnits(ix, iy);

	// Update cache by new value
	rCache.packedValue = cacheCopy.packedValue;

	return cacheCopy.surfType;
}
