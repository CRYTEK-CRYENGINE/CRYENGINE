// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainBeachGen.h"
#include ".\Terrain\Heightmap.h"

//min size of water area with beaches
#define MIN_UNITS_IN_WATER_AREA 8
#define BOTTOM_LEVEL            (-10000)

//////////////////////////////////////////////////////////////////////////
CTerrainBeachGenerator::CTerrainBeachGenerator(CHeightmap* pTerrain)
{
	m_pFile = 0;

	m_nMaxAreaSize = 0;
	m_pTerrain = pTerrain;

	m_pHeightmapData = pTerrain->GetData();

	SSectorInfo ssi;
	pTerrain->GetSectorsInfo(ssi);
	m_sectorSize = ssi.sectorSize;
	m_unitSize = ssi.unitSize;
	m_terrainSize = ssi.sectorSize * ssi.numSectors;
	//m_sectorTableSize = 32;
	m_sectorTableSize = ssi.numSectors;
	m_heightmapSize = pTerrain->GetHeight();

	m_waterLevel = pTerrain->GetWaterLevel();

	m_fShoreSize = 2;

	m_arrBeachMap.Allocate(m_terrainSize / m_unitSize + 1);
	m_WaterAreaMap.Allocate(m_terrainSize / m_unitSize + 1);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainBeachGenerator::Generate(CFile& file)
{
	m_pFile = &file;

	int nAreasFound = MarkWaterAreas();

	int i, x, y;

	std::vector<CTerrainSectorBeachInfo*> sectors;
	sectors.resize(m_sectorTableSize * m_sectorTableSize);
	for (x = 0; x < m_sectorTableSize; x++)
	{
		for (y = 0; y < m_sectorTableSize; y++)
		{
			CTerrainSectorBeachInfo* pSector = new CTerrainSectorBeachInfo;
			sectors[x + y * m_sectorTableSize] = pSector;
			pSector->m_nOriginX = x * m_sectorSize;
			pSector->m_nOriginY = y * m_sectorSize;
		}
	}

	for (x = 0; x < m_sectorTableSize; x++)
	{
		for (y = 0; y < m_sectorTableSize; y++)
		{
			MakeBeachStage1(sectors[x + y * m_sectorTableSize]);
		}
	}

	for (x = 0; x < m_sectorTableSize; x++)
	{
		for (y = 0; y < m_sectorTableSize; y++)
		{
			MakeBeachStage2(sectors[x + y * m_sectorTableSize]);
		}
	}

	for (i = 0; i < m_sectorTableSize * m_sectorTableSize; i++)
	{
		delete sectors[i];
	}
}

//////////////////////////////////////////////////////////////////////////
float CTerrainBeachGenerator::GetZSafe(int x, int y)
{
	if (x >= 0 && y >= 0 && x < m_terrainSize && y < m_terrainSize)
	{
		x /= m_unitSize;
		y /= m_unitSize;
		float z = m_pHeightmapData[(y * m_heightmapSize + x)];
		if (z > BOTTOM_LEVEL)
			return z;
	}
	return BOTTOM_LEVEL;
}

//////////////////////////////////////////////////////////////////////////
float CTerrainBeachGenerator::GetZSafe(float fx, float fy)
{
	int x = int(fx);
	int y = int(fy);
	if (x >= 0 && y >= 0 && x < m_terrainSize && y < m_terrainSize)
	{
		x /= m_unitSize;
		y /= m_unitSize;
		float z = m_pHeightmapData[(y * m_heightmapSize + x) / m_unitSize];
		if (z > BOTTOM_LEVEL)
			return z;
	}
	return BOTTOM_LEVEL;
}

//////////////////////////////////////////////////////////////////////////
float CTerrainBeachGenerator::GetZApr(float x1, float y1)
{
	float dDownLandZ;

	if (x1 < 1 || y1 < 1 || x1 >= m_terrainSize || y1 >= m_terrainSize)
		dDownLandZ = BOTTOM_LEVEL;
	else
	{
		// convert into hmap space
		x1 /= m_unitSize;
		y1 /= m_unitSize;

		int nX = int(x1);
		int nY = int(y1);

		//int nX = (int)x1;
		//int nY = (int)y1;

		float dx1 = x1 - nX;
		float dy1 = y1 - nY;

		float dDownLandZ0 =
		  (1.f - dx1) * m_pHeightmapData[nX + nY * m_heightmapSize] +
		  (dx1) * m_pHeightmapData[nX + 1 + nY * m_heightmapSize];

		float dDownLandZ1 =
		  (1.f - dx1) * m_pHeightmapData[nX + (nY + 1) * m_heightmapSize] +
		  (dx1) * m_pHeightmapData[nX + 1 + (nY + 1) * m_heightmapSize];

		dDownLandZ = (1 - dy1) * dDownLandZ0 + (dy1) * dDownLandZ1;

		if (dDownLandZ < BOTTOM_LEVEL)
			dDownLandZ = BOTTOM_LEVEL;
	}

	return dDownLandZ;
}

//////////////////////////////////////////////////////////////////////////
int CTerrainBeachGenerator::MarkWaterAreas()
{
	uint16 nMaxAreaId = 0;
	int nSize = m_terrainSize / m_unitSize;

	for (int x = 0; x < nSize; x++)
	{
		for (int y = 0; y < nSize; y++)
		{
			bool in_water = GetZSafe(x * m_unitSize, y * m_unitSize) < m_waterLevel;
			m_WaterAreaMap[x][y] = in_water ? 1 : 0;
		}
	}

	m_lstWaterAreaSizeTable.clear();
	for (int x = 0; x < nSize; x++)
		for (int y = 0; y < nSize; y++)
		{
			while (m_lstWaterAreaSizeTable.size() <= m_WaterAreaMap[x][y])
				m_lstWaterAreaSizeTable.push_back(int(0));

			m_lstWaterAreaSizeTable[m_WaterAreaMap[x][y]]++;
		}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainBeachGenerator::MakeBeachStage1(CTerrainSectorBeachInfo* pSector)
{
	for (int x = pSector->m_nOriginX; x <= pSector->m_nOriginX + m_sectorSize; x += m_unitSize)
		for (int y = pSector->m_nOriginY; y <= pSector->m_nOriginY + m_sectorSize; y += m_unitSize)
		{
			bool in_water = GetZSafe(x, y) < m_waterLevel;
			bool beach = 0;
			for (int _x = x - m_unitSize; _x <= x + m_unitSize; _x += m_unitSize)
				for (int _y = y - m_unitSize; _y <= y + m_unitSize; _y += m_unitSize)
				{
					bool _in_water = (GetZSafe(_x, _y) < m_waterLevel)
					                 && (_x > 0 && _y > 0 && _x < m_terrainSize && _y < m_terrainSize)
					                 && (m_lstWaterAreaSizeTable[(m_WaterAreaMap[_x / m_unitSize][_y / m_unitSize])] > MIN_UNITS_IN_WATER_AREA);

					if (in_water != _in_water && (x == _x || y == _y))
					{
						beach = true;
						break;
					}
				}

			m_arrBeachMap[x / m_unitSize][y / m_unitSize].beach = beach;
			m_arrBeachMap[x / m_unitSize][y / m_unitSize].in_water = in_water;
		}
}

void CTerrainBeachGenerator::MakeBeachStage2(CTerrainSectorBeachInfo* pSector)
{
	SVF_P3F_C4B_T2F* m_pVertBufferBeach = 0;
	int m_nVertNumBeach = 0;

	/*
	   if(!m_bBeachPresent)
	   {
	   if(hFileToSave)
	    GetSystem()->GetIPak()->FPrintf(hFileToSave, "BeachInfo(%8d)",GetSecIndex());
	   // m_nVertNumBeach is zero = no beach here
	   if(hFileToSave)
	    GetSystem()->GetIPak()->FWrite(&m_nVertNumBeach,1,sizeof(m_nVertNumBeach),hFileToSave);
	   return;
	   }
	 */

	int group;
	for (group = 0; group < MAX_BEACH_GROUPS; group++)
		pSector->m_arrlistBeachVerts[group].clear();

	for (int x = pSector->m_nOriginX; x <= pSector->m_nOriginX + m_sectorSize; x += m_unitSize)
		for (int y = pSector->m_nOriginY; y <= pSector->m_nOriginY + m_sectorSize; y += m_unitSize)
		{
			if (m_arrBeachMap[x / m_unitSize][y / m_unitSize].beach &&
			    !m_arrBeachMap[x / m_unitSize][y / m_unitSize].in_water)
			{
				Vec3 water_dir(0, 0, 0);
				for (int _x = x - m_unitSize; _x <= x + m_unitSize; _x += m_unitSize)
					for (int _y = y - m_unitSize; _y <= y + m_unitSize; _y += m_unitSize)
						if (_x >= 0 && _y >= 0 && _x < m_terrainSize && _y < m_terrainSize)
							if (m_arrBeachMap[_x / m_unitSize][_y / m_unitSize].in_water)
								water_dir += Vec3(float(_x - x), float(_y - y), 0);

				water_dir.Normalize();
				Vec3 border_pos((float)x, (float)y, m_waterLevel);

				CTerrainSectorBeachInfo::BeachPairStruct pair;
				pair.water_dir = water_dir;

				water_dir /= 100;
				int t = 0;
				while (GetZApr(border_pos.x, border_pos.y) > m_waterLevel && t < 100)
				{
					border_pos += water_dir;
					t++;
				}

				//  assert(t<100);

				/*      if( border_pos.x>=m_nOriginX &&
				   border_pos.y>=m_nOriginY &&
				   border_pos.x<(m_nOriginX+m_sectorSize) &&
				   border_pos.y<(m_nOriginY+m_sectorSize) )*/
				{
					pair.pos = border_pos; //+ Vec3(0,0,0.01f);
					pSector->m_lstUnsortedBeachVerts.push_back(pair);
				}
				/*      else
				   {
				   if(GetSectorFromPoint(border_pos.x,border_pos.y) == this
				   }*/
			}
		}

	if (!pSector->m_lstUnsortedBeachVerts.size())
	{
		string str;
		str.Format("BeachInfo(%8d)", GetSecIndex(pSector));
		m_pFile->Write((const char*)str, str.GetLength());

		// m_nVertNumBeach is zero = no beach here
		m_pFile->Write(&m_nVertNumBeach, sizeof(m_nVertNumBeach));
		return;
	}

	for (group = 0; group < MAX_BEACH_GROUPS; group++)
	{
		int prev_closest_id = -1;
		pSector->m_arrlistBeachVerts[group].clear();

		// search for free starting point
		int first;
		for (first = 0; first < pSector->m_lstUnsortedBeachVerts.size(); first++)
			if (pSector->m_lstUnsortedBeachVerts[first].busy < 2)
				break;

		if (first >= pSector->m_lstUnsortedBeachVerts.size())
			break;   // no more free points

		pSector->m_arrlistBeachVerts[group].push_back(pSector->m_lstUnsortedBeachVerts[first]);
		pSector->m_lstUnsortedBeachVerts[first].busy++;

		for (int pass = 0; pass < pSector->m_lstUnsortedBeachVerts.size(); pass++)
		{
			// find totaly free point closest to last result point
			int closest_id = -1;
			{
				float closest_dist = 100000;
				for (int j = 0; j < pSector->m_lstUnsortedBeachVerts.size(); j++)
					if (pSector->m_lstUnsortedBeachVerts[j].busy == 0)
					{
						float dist = pSector->m_arrlistBeachVerts[group].back().pos.GetDistance(pSector->m_lstUnsortedBeachVerts[j].pos);
						if (dist < closest_dist && dist < 4 && dist != 0)
						{
							closest_dist = dist;
							closest_id = j;
						}
					}
			}

			if (closest_id >= 0)
			{
				// if found add into current group
				if (prev_closest_id >= 0)
					pSector->m_lstUnsortedBeachVerts[prev_closest_id].busy++;
				prev_closest_id = closest_id;

				pSector->m_arrlistBeachVerts[group].push_back(pSector->m_lstUnsortedBeachVerts[closest_id]);
				pSector->m_lstUnsortedBeachVerts[closest_id].busy++;
			}
			else
			{
				if (prev_closest_id >= 0)
					pSector->m_lstUnsortedBeachVerts[prev_closest_id].busy++;
				prev_closest_id = closest_id;

				closest_id = -1;
				{
					// try to find semi free point as last
					float closest_dist = 100000;
					for (int j = 0; j < pSector->m_lstUnsortedBeachVerts.size(); j++)
						if (pSector->m_lstUnsortedBeachVerts[j].busy == 1)
						{
							// skip point if it is already used in this group
							int k;
							for (k = 0; k < pSector->m_arrlistBeachVerts[group].size(); k++)
								if (pSector->m_arrlistBeachVerts[group][k].pos.GetDistance(pSector->m_lstUnsortedBeachVerts[j].pos) == 0)
									break;

							if (k < pSector->m_arrlistBeachVerts[group].size())
								continue;

							float dist = pSector->m_arrlistBeachVerts[group].back().pos.GetDistance(pSector->m_lstUnsortedBeachVerts[j].pos);
							if (dist < closest_dist && dist < 4 && dist != 0)
							{
								closest_dist = dist;
								closest_id = j;
							}
						}
				}

				if (closest_id >= 0)
				{
					// if found add into current group
					pSector->m_arrlistBeachVerts[group].push_back(pSector->m_lstUnsortedBeachVerts[closest_id]);
					pSector->m_lstUnsortedBeachVerts[closest_id].busy++;
				}

				if (pSector->m_arrlistBeachVerts[group].size() == 1)
					pSector->m_arrlistBeachVerts[group].clear();

				break;
			}
		}
	}

	//  for(int u=0; u<UnsortedBeachVerts.size(); u++)
	//  CSystem::GetRenderer()->DrawLabel(UnsortedBeachVerts[u].pos,1,"%d",UnsortedBeachVerts[u].busy);

	// smooth
	for (group = 0; group < MAX_BEACH_GROUPS; group++)
	{
		int numv = pSector->m_arrlistBeachVerts[group].size();
		for (int i = 1; i < numv - 1; i++)
		{
			Vec3 dir1 = pSector->m_arrlistBeachVerts[group][i + 1].pos - pSector->m_arrlistBeachVerts[group][i].pos;
			dir1.Normalize();
			Vec3 dir2 = pSector->m_arrlistBeachVerts[group][i - 1].pos - pSector->m_arrlistBeachVerts[group][i].pos;
			dir2.Normalize();

			Vec3 dir;

			if (dir1.Dot(dir2) > -0.99f)
				dir = dir1 + dir2;
			else
				dir = dir1.Cross(Vec3(0, 0, 1));

			dir.Normalize();
			if (dir.Dot(pSector->m_arrlistBeachVerts[group][i].water_dir) < 0)
				dir = -dir;
			pSector->m_arrlistBeachVerts[group][i].water_dir = dir;
		}
	}

	for (group = 0; group < MAX_BEACH_GROUPS; group++)
	{
		int numv = pSector->m_arrlistBeachVerts[group].size();
		for (int i = 0; i < numv - 1; i++)
		{
			CTerrainSectorBeachInfo::BeachPairStruct* p1 = &pSector->m_arrlistBeachVerts[group][i];
			CTerrainSectorBeachInfo::BeachPairStruct* p2 = &pSector->m_arrlistBeachVerts[group][i + 1];

			int _x = int((p1->pos + p1->water_dir * 2).x);
			int _y = int((p1->pos + p1->water_dir * 2).y);

			//      int nAreaID = m_WaterAreaMap[_x/m_unitSize][_y/m_unitSize];
			float fBeachSize = (float)m_fShoreSize;  //1.f+8.f*((float)m_lstWaterAreaSizeTable[nAreaID]/(float)m_nMaxAreaSize);

			// left
			p1->pos1 = p1->pos;

			int l = 0;
			while (p1->pos1.z < m_waterLevel + 0.1f * fBeachSize && l < 70)
			{
				p1->pos1 = p1->pos1 - pSector->m_arrlistBeachVerts[group][i].water_dir * 0.05f;
				p1->pos1.z = GetZApr(p1->pos1.x, p1->pos1.y);
				l++;
			}

			p1->pos2 = p1->pos + p1->water_dir * fBeachSize;

			p1->posm = p1->pos + Vec3(0, 0, 0.2f * fBeachSize / 4);

			// right
			p2->pos1 = p2->pos;

			l = 0;
			while (p2->pos1.z < m_waterLevel + 0.1f * fBeachSize && l < 70)
			{
				p2->pos1 = p2->pos1 - p2->water_dir * 0.05f;
				p2->pos1.z = GetZApr(p2->pos1.x, p2->pos1.y);
				l++;
			}

			p2->pos2 = p2->pos + p2->water_dir * fBeachSize;

			p2->posm = p2->pos + Vec3(0, 0, 0.2f * fBeachSize / 4);
		}
	}

	pSector->m_lstUnsortedBeachVerts.clear();

	/////////////////////////////////////////////////////////////////////////////////////////////
	// stage 3
	/////////////////////////////////////////////////////////////////////////////////////////////

	/*
	   if(!m_bBeachPresent)
	   {
	   if(hFileToSave)
	    GetSystem()->GetIPak()->FPrintf(hFileToSave, "BeachInfo(%8d)",GetSecIndex());
	   // m_nVertNumBeach is zero = no beach here
	   if(hFileToSave)
	    GetSystem()->GetIPak()->FWrite(&m_nVertNumBeach,1,sizeof(m_nVertNumBeach),hFileToSave);
	   return;
	   }
	 */

	// count elements
	int nElements = 0;
	for (group = 0; group < MAX_BEACH_GROUPS; group++)
	{
		int numv = pSector->m_arrlistBeachVerts[group].size();
		for (int i = 0; i < numv - 1; i++)
			nElements++;
	}

	if (!nElements)
	{
		//m_bBeachPresent = false;
		{
			string str;
			str.Format("BeachInfo(%8d)", GetSecIndex(pSector));
			m_pFile->Write((const char*)str, str.GetLength());

			// m_nVertNumBeach is zero = no beach here
			m_pFile->Write(&m_nVertNumBeach, sizeof(m_nVertNumBeach));
			return;
		}
	}

	// make buffer
	m_nVertNumBeach = 6 * nElements;
	m_pVertBufferBeach = new SVF_P3F_C4B_T2F[m_nVertNumBeach];

	std::vector<uint16> lstBeachIndices;

	// tmp buff
	SVF_P3F_C4B_T2F verts[6];

	// color
	Vec3 vTerrainColor(1, 1, 1); //= GetSystem()->GetI3DEngine()->GetWorldColor();//*GetSystem()->GetI3DEngine()->GetWorldBrightnes();

	for (int i = 0; i < 6; i++)
	{
		verts[i].color.bcolor[0] = (byte)min(255.0f, vTerrainColor[0] * 512.0f);
		verts[i].color.bcolor[1] = (byte)min(255.0f, vTerrainColor[1] * 512.0f);
		verts[i].color.bcolor[2] = (byte)min(255.0f, vTerrainColor[2] * 512.0f);
		verts[i].color.bcolor[3] = 0;
	}

	verts[2].color.bcolor[3] = 255;
	verts[3].color.bcolor[3] = 255;

	// tex coord
	verts[0].st = Vec2(0.0f, 1.0f);
	verts[1].st = Vec2(1.0f, 1.0f);
	verts[2].st = Vec2(0.0f, 0.5f);
	verts[3].st = Vec2(1.0f, 0.5f);
	verts[4].st = Vec2(0.0f, 0.0f);
	verts[5].st = Vec2(1.0f, 0.0f);

	// make final buffer
	int nPos = 0;
	int nIndex = 0;
	for (group = 0; group < MAX_BEACH_GROUPS; group++)
	{
		int numv = pSector->m_arrlistBeachVerts[group].size();
		for (int i = 0; i < numv - 1; i++)
		{
			CTerrainSectorBeachInfo::BeachPairStruct* p1 = &pSector->m_arrlistBeachVerts[group][i];
			CTerrainSectorBeachInfo::BeachPairStruct* p2 = &pSector->m_arrlistBeachVerts[group][i + 1];

			// pos
			verts[0].xyz = p1->pos1;

			verts[1].xyz = p2->pos1;

			verts[2].xyz = p1->posm;

			verts[3].xyz = p2->posm;

			verts[4].xyz = p1->pos2;

			verts[5].xyz = p2->pos2;

			// draw
			memcpy(&m_pVertBufferBeach[nPos], verts, sizeof(verts));
			nPos += 6;

			lstBeachIndices.push_back(nIndex + 0);
			lstBeachIndices.push_back(nIndex + 1);
			lstBeachIndices.push_back(nIndex + 2);

			lstBeachIndices.push_back(nIndex + 3);
			lstBeachIndices.push_back(nIndex + 2);
			lstBeachIndices.push_back(nIndex + 1);

			lstBeachIndices.push_back(nIndex + 2);
			lstBeachIndices.push_back(nIndex + 3);
			lstBeachIndices.push_back(nIndex + 4);

			lstBeachIndices.push_back(nIndex + 5);
			lstBeachIndices.push_back(nIndex + 4);
			lstBeachIndices.push_back(nIndex + 3);

			nIndex += 6;
		}
	}

	// save calculated data into file
	// header
	string str;
	str.Format("BeachInfo(%8d)", GetSecIndex(pSector));
	m_pFile->Write((const char*)str, str.GetLength());

	// m_nVertNumBeach is zero = no beach here
	m_pFile->Write(&m_nVertNumBeach, sizeof(m_nVertNumBeach));
	m_pFile->Write(m_pVertBufferBeach, m_nVertNumBeach * sizeof(SVF_P3F_C4B_T2F));

	// indices
	int nIndCount = lstBeachIndices.size();
	m_pFile->Write(&nIndCount, sizeof(nIndCount));
	m_pFile->Write(&lstBeachIndices[0], nIndCount * sizeof(uint16));

	/*
	   // make vertex buffer
	   if(m_pRenderMeshBeach)
	   GetRenderer()->DeleteRenderMesh(m_pRenderMeshBeach);
	   m_pRenderMeshBeach = GetRenderer()->CreateRenderMeshInitialized(
	   m_pVertBufferBeach, m_nVertNumBeach, VERTEX_FORMAT_P3F_COL4UB_TEX2F,
	   lstBeachIndices.GetElements(), lstBeachIndices.size(), R_PRIMV_TRIANGLES, "WaterBeach");

	   m_pRenderMeshBeach->SetChunk( m_pTerrain->m_pSHShore,
	   0, m_nVertNumBeach,
	   0, lstBeachIndices.size());

	   assert(m_pRenderMeshBeach);
	 */
	delete[] m_pVertBufferBeach;
	m_pVertBufferBeach = 0;
}

