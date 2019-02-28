// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AreaGrid.h"
#include "Area.h"
#include <CryRenderer/IRenderAuxGeom.h>

static constexpr int GridCellSize = 4;
static constexpr float GridCellSizeR = 1.0f / GridCellSize;

//////////////////////////////////////////////////////////////////////////
CAreaGrid::~CAreaGrid()
{
	Reset();
}

//////////////////////////////////////////////////////////////////////////
bool CAreaGrid::GetAreas(uint32 const x, uint32 const y, TAreaPointers& outAreas)
{
	CRY_ASSERT(x < m_numCellsPerAxis && y < m_numCellsPerAxis);

	uint32 const* const pBitsLHS = m_bitFieldX.data() + (m_bitFieldSizeU32 * x);
	uint32 const* const pBitsRHS = m_bitFieldY.data() + (m_bitFieldSizeU32 * y);

	for (uint32 i = 0, offset = 0; i < m_bitFieldSizeU32; ++i, offset += 32)
	{
		uint32 currentBitField = pBitsLHS[i] & pBitsRHS[i];

		for (uint32 j = 0; currentBitField != 0; ++j, currentBitField >>= 1)
		{
			if (currentBitField & 1)
			{
				outAreas.push_back(m_pAreas->at(offset + j));
			}
		}
	}

	return !outAreas.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CAreaGrid::GetAreaIndex(CArea const* const pArea, size_t& outIndex)
{
	bool bSuccess = false;

	if (m_pAreas != nullptr)
	{
		TAreaPointers::const_iterator Iter(m_pAreas->begin());
		TAreaPointers::const_iterator const IterEnd(m_pAreas->end());

		for (; Iter != IterEnd; ++Iter)
		{
			if ((*Iter) == pArea)
			{
				outIndex = std::distance(m_pAreas->begin(), Iter);
				CRY_ASSERT(outIndex < m_pAreas->size());
				bSuccess = true;

				break;
			}
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CAreaGrid::AddAreaBit(const Vec2i& start, const Vec2i& end, uint32 areaIndex)
{
	CRY_ASSERT(start.x >= 0 && start.y >= 0 && end.x < (int)m_numCellsPerAxis && end.y < (int)m_numCellsPerAxis);
	CRY_ASSERT(start.x <= end.x && start.y <= end.y);

	uint32* pBits;
	const uint32 bucketBit = 1 << (areaIndex & 31);
	const uint32 bucketIndex = areaIndex >> 5;

	// -- X ---
	pBits = m_bitFieldX.data() + (m_bitFieldSizeU32 * start.x) + bucketIndex;
	for (int i = start.x; i <= end.x; i++, pBits += m_bitFieldSizeU32)
		*pBits |= bucketBit;

	// -- Y ---
	pBits = m_bitFieldY.data() + (m_bitFieldSizeU32 * start.y) + bucketIndex;
	for (int i = start.y; i <= end.y; i++, pBits += m_bitFieldSizeU32)
		*pBits |= bucketBit;

	m_areaBounds[areaIndex][0] = start;
	m_areaBounds[areaIndex][1] = end;
}

//////////////////////////////////////////////////////////////////////////
void CAreaGrid::RemoveAreaBit(uint32 areaIndex)
{
	Vec2i& start = m_areaBounds[areaIndex][0];
	Vec2i& end = m_areaBounds[areaIndex][1];

	if (start == Vec2i(-1, -1) && end == Vec2i(-1, -1))
	{
		return; // Hasn't been added yet
	}

	CRY_ASSERT(start.x >= 0 && start.y >= 0 && end.x < (int)m_numCellsPerAxis && end.y < (int)m_numCellsPerAxis);
	CRY_ASSERT(start.x <= end.x && start.y <= end.y);

	uint32* pBits;
	const uint32 bucketBit = ~(1 << (areaIndex & 31));
	const uint32 bucketIndex = areaIndex >> 5;

	// -- X ---
	pBits = m_bitFieldX.data() + (m_bitFieldSizeU32 * start.x) + bucketIndex;
	for (int i = start.x; i <= end.x; i++, pBits += m_bitFieldSizeU32)
		*pBits &= bucketBit;

	// -- Y ---
	pBits = m_bitFieldY.data() + (m_bitFieldSizeU32 * start.y) + bucketIndex;
	for (int i = start.y; i <= end.y; i++, pBits += m_bitFieldSizeU32)
		*pBits &= bucketBit;

	start = Vec2i(-1, -1);
	end = Vec2i(-1, -1);
}

//////////////////////////////////////////////////////////////////////////
void CAreaGrid::ClearAllBits()
{
	std::fill(m_bitFieldX.begin(), m_bitFieldX.end(), 0);
	std::fill(m_bitFieldY.begin(), m_bitFieldY.end(), 0);
	std::fill(m_areaBounds.begin(), m_areaBounds.end(), std::array<Vec2i, 2> {
		{ ZERO, ZERO }
	});
}

//////////////////////////////////////////////////////////////////////////
bool CAreaGrid::ResetArea(CArea* pArea)
{
	bool bSuccess = false;

	if (m_pAreas != nullptr)
	{
		CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

		uint32 index = 0;
		TAreaPointers::const_iterator iter(m_pAreas->begin());
		TAreaPointers::const_iterator const iterEnd(m_pAreas->end());
		uint32 const numAreas = std::min<uint32>(m_maxNumAreas, static_cast<uint32>(m_pAreas->size()));

		for (; iter != iterEnd && index < numAreas; ++iter, ++index)
		{
			if (*iter == pArea)
			{
				RemoveAreaBit(index);
				AddArea(pArea, index);
				bSuccess = true;
			}
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CAreaGrid::AddArea(CArea* pArea, uint32 areaIndex)
{
	// Calculate a loose bounding box (i.e. one that covers all the region we will have to check this
	// shape for, including fade area).
	Vec2 vBBCentre(0.0f, 0.0f);
	Vec2 vBBExtent(0.0f, 0.0f);

	switch (pArea ? pArea->GetAreaType() : EEntityAreaType(-1))
	{
	case ENTITY_AREA_TYPE_SPHERE:
		{
			Vec3 sphereCenter(ZERO);
			float sphereRadius(0.0f);
			if (pArea)
				pArea->GetSphere(sphereCenter, sphereRadius);
			vBBCentre = Vec2(sphereCenter.x, sphereCenter.y);
			float const greatestFadeDistance = pArea->GetGreatestFadeDistance();
			vBBExtent = Vec2(sphereRadius + greatestFadeDistance, sphereRadius + greatestFadeDistance);
		}
		break;
	case ENTITY_AREA_TYPE_SHAPE:
		{
			Vec2 shapeMin, shapeMax;
			pArea->GetBBox(shapeMin, shapeMax);
			vBBCentre = (shapeMax + shapeMin) * 0.5f;
			float const greatestFadeDistance = pArea->GetGreatestFadeDistance();
			vBBExtent = (shapeMax - shapeMin) * 0.5f + Vec2(greatestFadeDistance, greatestFadeDistance);
		}
		break;
	case ENTITY_AREA_TYPE_BOX:
		{
			Vec3 boxMin, boxMax;
			pArea->GetBox(boxMin, boxMax);
			Matrix34 tm;
			pArea->GetMatrix(tm);
			vBBCentre = Vec2(tm.GetTranslation());
			vBBExtent = Matrix33(tm).GetFabs() * (Vec2(boxMax - boxMin) * 0.5f);
			float const greatestFadeDistance = pArea->GetGreatestFadeDistance();
			vBBExtent += Vec2(greatestFadeDistance, greatestFadeDistance);
		}
		break;
	case ENTITY_AREA_TYPE_SOLID:
		{
			AABB boundbox;
			pArea->GetSolidBoundBox(boundbox);
			Matrix34 tm;
			pArea->GetMatrix(tm);
			float const greatestFadeDistance = pArea->GetGreatestFadeDistance();

			boundbox.SetTransformedAABB(tm, boundbox);
			vBBExtent = Vec2(std::abs(boundbox.max.x - boundbox.min.x), std::abs(boundbox.max.y - boundbox.min.y)) * 0.5f;
			vBBExtent += Vec2(greatestFadeDistance, greatestFadeDistance);
			vBBCentre = Vec2(boundbox.min.x + boundbox.max.x, boundbox.min.y + boundbox.max.y) * 0.5f;
		}
		break;
	}

	// Convert BB pos into grid coordinates
	Vec2i start((int)((vBBCentre.x - vBBExtent.x) * GridCellSizeR), (int)((vBBCentre.y - vBBExtent.y) * GridCellSizeR));
	Vec2i end((int)((vBBCentre.x + vBBExtent.x) * GridCellSizeR), (int)((vBBCentre.y + vBBExtent.y) * GridCellSizeR));

	if ((end.x | end.y) < 0 || start.x >= (int)m_numCellsPerAxis || start.y > (int)m_numCellsPerAxis)
		return;

	start.x = std::max<int>(start.x, 0);
	start.y = std::max<int>(start.y, 0);
	end.x = std::min<int>(end.x, (int)m_numCellsPerAxis - 1);
	end.y = std::min<int>(end.y, (int)m_numCellsPerAxis - 1);

	AddAreaBit(start, end, areaIndex);

#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
	//query BB extents to see if they are correctly added to the grid
	//Debug_CheckBB(vBBCentre, vBBExtent, pArea);
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CAreaGrid::Compile(TAreaPointers const& areas)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	int const terrainSize = gEnv->p3DEngine->GetTerrainSize();
	uint32 const numCellsPerAxis = static_cast<uint32>(terrainSize / GridCellSize);

	// No point creating an area grid if there are no areas.
	if (areas.empty() || numCellsPerAxis == 0)
	{
		Reset();
		return;
	}

	uint32 const oldNumCellsPerAxis = m_numCellsPerAxis;
	uint32 const numAreas = static_cast<uint32>(areas.size());
	uint32 const bitFieldSizeU32 = ((numAreas + 31) >> 5);

	if (numCellsPerAxis != oldNumCellsPerAxis || numAreas > m_maxNumAreas || m_bitFieldSizeU32 != bitFieldSizeU32)
	{
		// Full reset and reallocate bit-fields
		Reset();
		m_numCellsPerAxis = numCellsPerAxis;
		m_bitFieldSizeU32 = bitFieldSizeU32;
		m_maxNumAreas = numAreas;

		CRY_ASSERT(m_bitFieldX.empty());

		m_bitFieldX.resize(m_bitFieldSizeU32 * m_numCellsPerAxis);
		m_bitFieldY.resize(m_bitFieldSizeU32 * m_numCellsPerAxis);
		m_areaBounds.resize(m_maxNumAreas);
	}

	m_pAreas = &areas;
	ClearAllBits();
	uint32 areaIndex = 0;

	for (auto const pArea : areas)
	{
		AddArea(pArea, areaIndex++);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaGrid::Reset()
{
	m_bitFieldX.clear();
	m_bitFieldY.clear();
	m_areaBounds.clear();
	m_bitFieldSizeU32 = 0;
	m_maxNumAreas = 0;
	m_numCellsPerAxis = 0;
	m_pAreas = nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CAreaGrid::GetAreas(Vec3 const& position, TAreaPointers& outAreas)
{
	if (!m_bitFieldX.empty())
	{
		uint32 const gridX = static_cast<uint32>(position.x * GridCellSizeR);
		uint32 const gridY = static_cast<uint32>(position.y * GridCellSizeR);

		if (gridX < m_numCellsPerAxis && gridY < m_numCellsPerAxis)
		{
			return GetAreas(gridX, gridY, outAreas);
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAreaGrid::Draw()
{
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
	if (m_pAreas != nullptr && !m_pAreas->empty())
	{
		static TAreaPointers s_tempAreas;

		static ColorF const colorsArray[] = {
			ColorF(1.0f, 0.0f, 0.0f, 1.0f),
			ColorF(0.0f, 1.0f, 0.0f, 1.0f),
			ColorF(0.0f, 0.0f, 1.0f, 1.0f),
			ColorF(1.0f, 1.0f, 0.0f, 1.0f),
			ColorF(1.0f, 0.0f, 1.0f, 1.0f),
			ColorF(0.0f, 1.0f, 1.0f, 1.0f),
			ColorF(1.0f, 1.0f, 1.0f, 1.0f),
		};

		static float const color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		static Vec3 const
		p1(-GridCellSize * 0.5f, -GridCellSize * 0.5f, 0.0f),
		p2(+GridCellSize * 0.5f, -GridCellSize * 0.5f, 0.0f),
		p3(+GridCellSize * 0.5f, +GridCellSize * 0.5f, 0.0f),
		p4(-GridCellSize * 0.5f, +GridCellSize * 0.5f, 0.0f);

		I3DEngine* const p3DEngine = gEnv->p3DEngine;
		IRenderAuxGeom* const pRC = gEnv->pAuxGeomRenderer;
		pRC->SetRenderFlags(e_Def3DPublicRenderflags);
		Vec3 const& camPos(gEnv->pSystem->GetViewCamera().GetPosition());
		uint32 cell = 0;

		for (uint32 gridX = 0; gridX < m_numCellsPerAxis; gridX++)
		{
			float const cellX = (static_cast<float>(gridX) + 0.5f) * GridCellSize;

			for (uint32 gridY = 0; gridY < m_numCellsPerAxis; gridY++)
			{
				if (GetAreas(gridX, gridY, s_tempAreas))
				{
					Vec3 const gridCenter = Vec3((float(gridX) + 0.5f) * GridCellSize, (float(gridY) + 0.5f) * GridCellSize, 0.0f);

					ColorF colour(0.0f, 0.0f, 0.0f, 0.0f);
					float divisor = 0.0f;

					for (auto const pArea : s_tempAreas)
					{
						// Pick a random color from the array.
						size_t areaIndex = 0;

						if (GetAreaIndex(pArea, areaIndex))
						{
							ColorF const areaColour = colorsArray[areaIndex % (sizeof(colorsArray) / sizeof(ColorF))];
							colour += areaColour;
							++divisor;
						}
						else
						{
							// Areas must be known!
							CRY_ASSERT(false);

							if (divisor == 0.0f)
							{
								divisor = 0.1f;
							}
						}
					}

					// Immediately clear the array to prevent it from re-entering this loop and messing up drawing of the grid.
					s_tempAreas.clear();

					// "divisor" won't be 0!
					colour /= divisor;

					ColorB const colourB = colour;
					Vec3 points[4] = { gridCenter + p1, gridCenter + p2, gridCenter + p3, gridCenter + p4 };

					for (auto& point : points)
					{
						point.z = p3DEngine->GetTerrainElevation(point.x, point.y);
					}

					pRC->DrawTriangle(points[0], colourB, points[1], colourB, points[2], colourB);
					pRC->DrawTriangle(points[2], colourB, points[3], colourB, points[0], colourB);
				}

				if (CVar::pDrawAreaGridCells->GetIVal() != 0)
				{
					++cell;
					float const cellY = (static_cast<float>(gridY) + 0.5f) * GridCellSize;
					float const cellZ = p3DEngine->GetTerrainElevation(cellX, cellY);
					Vec3 const cellCenter(cellX, cellY, cellZ);

					Vec3 screenPos(ZERO);
					gEnv->pRenderer->ProjectToScreen(cellCenter.x, cellCenter.y, cellCenter.z, &screenPos.x, &screenPos.y, &screenPos.z);

					const int w = IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceX();
					const int h = IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceZ();

					screenPos.x = screenPos.x * 0.01f * w;
					screenPos.y = screenPos.y * 0.01f * h;

					if (
					  (screenPos.x >= 0.0f) && (screenPos.y >= 0.0f) &&
					  (screenPos.z >= 0.0f) && (screenPos.z <= 1.0f) &&
					  cellCenter.GetSquaredDistance(camPos) < 625.0f)
					{
						pRC->SetRenderFlags(e_Def2DPublicRenderflags);
						IRenderAuxText::Draw2dLabel(
						  screenPos.x,
						  screenPos.y,
						  1.35f,
						  color,
						  false,
						  "Cell: %u X: %u Y: %u\n",
						  cell,
						  gridX,
						  gridY);
						pRC->SetRenderFlags(e_Def3DPublicRenderflags);
					}
				}
			}
		}

		float const greenColor[4] = { 0.0f, 1.0f, 0.0f, 0.7f };
		IRenderAuxText::Draw2dLabel(400.0f, 10.0f, 1.5f, greenColor, false, "Area Grid Mem Use: num cells: %d, memAlloced: %.2fk",
		                            m_numCellsPerAxis, (4 * m_bitFieldSizeU32 * m_numCellsPerAxis * 2) / 1024.f);
	}
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
}

#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void CAreaGrid::Debug_CheckBB(Vec2 const& vBBCentre, Vec2 const& vBBExtent, CArea const* const pArea)
{
	Vec2 const minPos(vBBCentre.x - vBBExtent.x, vBBCentre.y - vBBExtent.y);
	TAreaPointers tempAreas(16);
	GetAreas(minPos, tempAreas);
	bool bFoundMin = false;

	for (auto const pTempArea : tempAreas)
	{
		if (pTempArea == pArea)
		{
			bFoundMin = true;
			break;
		}
	}

	Vec2 const maxPos(vBBCentre.x + vBBExtent.x, vBBCentre.y + vBBExtent.y);
	tempAreas.clear();
	GetAreas(maxPos, tempAreas);
	bool bFoundMax = false;

	for (auto const pTempArea : tempAreas)
	{
		if (pTempArea == pArea)
		{
			bFoundMax = true;
			break;
		}
	}

	//caveat: this may fire if the area was clamped
	if ((!bFoundMin || !bFoundMax) /*&& !bClamped*/)
	{
		CryLogAlways("Error: AABB extent was not found in grid\n");
	}
}
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
