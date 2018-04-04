// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef _VERTEX_LIST_
#define _VERTEX_LIST_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAgent.h>
#include "GraphStructures.h"
#include "HashSpace.h"

class CCryFile;

class CVertexList
{
public:
	CVertexList();
	~CVertexList();
	int                 AddVertex(const ObstacleData& od);

	const ObstacleData& GetVertex(int index) const;
	ObstacleData&       ModifyVertex(int index);
	int                 FindVertex(const ObstacleData& od) const;

	bool                IsIndexValid(int index) const { return index >= 0 && index < (int)m_obstacles.size(); }

	bool                ReadFromFile(const char* fileName);

	void                Clear()       { stl::free_container(m_obstacles); m_hashSpace->Clear(true); }
	void                Reset();
	int                 GetSize()     { return m_obstacles.size(); }
	int                 GetCapacity() { return m_obstacles.capacity(); }

	void                GetVerticesInRange(std::vector<std::pair<float, unsigned>>& vertsOut, const Vec3& pos, float range, unsigned char flags);
	void                GetMemoryStatistics(ICrySizer* pSizer);

private:

	struct SVertexRecord
	{
		inline SVertexRecord() {}
		inline SVertexRecord(unsigned vertIndex) : vertIndex(vertIndex) {}
		inline const Vec3& GetPos(CVertexList& vertList) const        { return vertList.GetVertex(vertIndex).vPos; }
		inline bool        operator==(const SVertexRecord& rhs) const { return rhs.vertIndex == vertIndex; }
		unsigned           vertIndex;
	};

	class VertexHashSpaceTraits
	{
	public:
		inline VertexHashSpaceTraits(CVertexList& vertexList) : vertexList(vertexList) {}
		inline const Vec3& operator()(const SVertexRecord& item) const { return item.GetPos(vertexList); }
		CVertexList&       vertexList;
	};

	struct SVertCollector
	{
		SVertCollector(CVertexList& vertList, std::vector<std::pair<float, unsigned>>& verts, unsigned char flags)
			: vertList(vertList), verts(verts), flags(flags) {}

		void operator()(const SVertexRecord& record, float distSq)
		{
			if (vertList.GetVertex(record.vertIndex).flags & flags)
				verts.push_back(std::make_pair(distSq, record.vertIndex));
		}

		CVertexList&                             vertList;
		unsigned char                            flags;
		std::vector<std::pair<float, unsigned>>& verts;
	};

	/// Our spatial structure
	CHashSpace<SVertexRecord, VertexHashSpaceTraits>* m_hashSpace;

	Obstacles m_obstacles;
};

#endif // #ifndef _VERTEX_LIST_
