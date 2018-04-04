// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "VertexList.h"
#include "AILog.h"
#include <CrySystem/ISystem.h>
#include <CrySystem/File/CryFile.h>

#define BAI_VERTEX_FILE_VERSION 1

enum { BUCKET_COUNT = 8192 };

CVertexList::CVertexList()
{
	m_hashSpace = new CHashSpace<SVertexRecord, VertexHashSpaceTraits>(Vec3(7, 7, 7), BUCKET_COUNT, VertexHashSpaceTraits(*this));
	m_obstacles.clear();
}

CVertexList::~CVertexList()
{
	delete m_hashSpace;
}

int CVertexList::FindVertex(const ObstacleData& od) const
{
	const Obstacles::const_iterator oiend = m_obstacles.end();
	int index = 0;

	for (Obstacles::const_iterator oi = m_obstacles.begin(); oi != oiend; ++oi, ++index)
	{
		if ((*oi) == od)
			return index;
	}

	return -1;
}

int CVertexList::AddVertex(const ObstacleData& od)
{
	int index = FindVertex(od);
	if (index < 0)
	{
		m_obstacles.push_back(od);
		index = (int)m_obstacles.size() - 1;
		m_hashSpace->AddObject(SVertexRecord(index));
	}

	return index;
}

const ObstacleData& CVertexList::GetVertex(int index) const
{
	if (!IsIndexValid(index))
	{
#if !defined(_DEBUG)
		AIError("CVertexList::GetVertex Tried to retrieve a non existing vertex (%d) from vertex list (size %d).Please regenerate the triangulation [Design bug]",
#else
		AIError("CVertexList::GetVertex Tried to retrieve a non existing vertex (%d) from vertex list (size %" PRISIZE_T ").Please regenerate the triangulation [Design bug]",

#endif
		        index, m_obstacles.size());
		return m_obstacles[0];
	}
	return m_obstacles[index];
}

ObstacleData& CVertexList::ModifyVertex(int index)
{
	if (!IsIndexValid(index))
	{
#if !defined(_DEBUG)
		AIError("CVertexList::ModifyVertex Tried to retrieve a non existing vertex (%d) from vertex list (size %d).Please regenerate the triangulation [Design bug]",
#else
		AIError("CVertexList::ModifyVertex Tried to retrieve a non existing vertex (%d) from vertex list (size %" PRISIZE_T ").Please regenerate the triangulation [Design bug]",
#endif
		        index, m_obstacles.size());
		static ObstacleData od;
		return od;
	}
	return m_obstacles[index];
}

bool CVertexList::ReadFromFile(const char* fileName)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Navigation, 0, "Triangulation vertices");

	m_obstacles.clear();
	m_hashSpace->Clear(true);

	CCryFile file;
	if (!file.Open(fileName, "rb"))
	{
		AIError("CVertexList::ReadFromFile could not open vertex file: Regenerate triangulation in the editor [Design bug]");
		return false;
	}

	int iNumber;

	AILogLoading("Verifying BAI file version");
	file.ReadType(&iNumber);
	if (iNumber != BAI_VERTEX_FILE_VERSION)
	{
		AIError("CVertexList::ReadFromFile Wrong vertex list BAI file version - found %d expected %d: Regenerate triangulation in the editor [Design bug]", iNumber, BAI_VERTEX_FILE_VERSION);
		file.Close();
		return false;
	}

	// Read number of descriptors.
	file.ReadType(&iNumber);

	if (iNumber > 0)
	{
		std::vector<ObstacleDataDesc> obDescs(iNumber);
		file.ReadType(&obDescs[0], iNumber);
		m_obstacles.resize(iNumber);
		std::vector<uint32> cellCounts(BUCKET_COUNT, 0);
		for (int i = 0; i < iNumber; ++i)
		{
			m_obstacles[i].vPos = obDescs[i].vPos;
			m_obstacles[i].vDir = obDescs[i].vDir;
			m_obstacles[i].fApproxRadius = obDescs[i].fApproxRadius;
			m_obstacles[i].flags = obDescs[i].flags;
			m_obstacles[i].approxHeight = obDescs[i].approxHeight;
			m_hashSpace->RecordBucketUsage(SVertexRecord(i), cellCounts);
		}

		m_hashSpace->ReserveSpaceInBuckets(cellCounts);

		for (int i = 0; i < iNumber; ++i)
		{
			m_hashSpace->AddObject(SVertexRecord(i));
		}
	}

	file.Close();

	return true;
}

//===================================================================
// GetVerticesInRange
//===================================================================
void CVertexList::GetVerticesInRange(std::vector<std::pair<float, unsigned>>& vertsOut, const Vec3& pos, float range, unsigned char flags)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);
	vertsOut.clear();
	SVertCollector collector(*this, vertsOut, flags);
	m_hashSpace->ProcessObjectsWithinRadius(pos, range, collector);
}

void CVertexList::GetMemoryStatistics(ICrySizer* pSizer)
{
	pSizer->AddObject(m_hashSpace, m_hashSpace->MemStats());
	pSizer->AddContainer(m_obstacles);
}

//===================================================================
// Reset
//===================================================================
void CVertexList::Reset()
{
	const Obstacles::iterator oiend = m_obstacles.end();
	for (Obstacles::iterator oi = m_obstacles.begin(); oi != oiend; ++oi)
	{
	  ObstacleData& od = *oi;
	  od.ClearNavNodes();
	}
}
