// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "GraphNodeManager.h"

inline IAISystem::tNavCapMask TypeFromTypeIndex(int typeIndex)
{
	return IAISystem::tNavCapMask(1 << typeIndex);
}

inline int CGraphNodeManager::TypeSizeFromTypeIndex(unsigned typeIndex) const
{
	return m_typeSizes[typeIndex];
}

bool Match(IAISystem::tNavCapMask type, IAISystem::tNavCapMask mask)
{
	return (type & mask) != 0;
}

CGraphNodeManager::CGraphNodeManager() :
	m_freeBuckets(IAISystem::NAV_TYPE_COUNT, BucketHeader::InvalidNextFreeBucketIdx)
{
	static_assert((sizeof(GraphNode_Unset) % 4) == 0 && sizeof(GraphNode_Unset) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_Triangular) % 4) == 0 && sizeof(GraphNode_Triangular) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_WaypointHuman) % 4) == 0 && sizeof(GraphNode_WaypointHuman) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_Waypoint3DSurface) % 4) == 0 && sizeof(GraphNode_Waypoint3DSurface) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_Flight) % 4) == 0 && sizeof(GraphNode_Flight) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_Volume) % 4) == 0 && sizeof(GraphNode_Volume) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_Road) % 4) == 0 && sizeof(GraphNode_Road) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_SmartObject) % 4) == 0 && sizeof(GraphNode_SmartObject) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_Free2D) % 4) == 0 && sizeof(GraphNode_Free2D) < 1024, "Invalid type size!");
	static_assert((sizeof(GraphNode_CustomNav) % 4) == 0 && sizeof(GraphNode_CustomNav) < 1024, "Invalid type size!");

	m_typeSizes[0] = sizeof(GraphNode_Unset);
	m_typeSizes[1] = sizeof(GraphNode_Triangular);
	m_typeSizes[2] = sizeof(GraphNode_WaypointHuman);
	m_typeSizes[3] = sizeof(GraphNode_Waypoint3DSurface);
	m_typeSizes[4] = sizeof(GraphNode_Flight);
	m_typeSizes[5] = sizeof(GraphNode_Volume);
	m_typeSizes[6] = sizeof(GraphNode_Road);
	m_typeSizes[7] = sizeof(GraphNode_SmartObject);
	m_typeSizes[8] = sizeof(GraphNode_Free2D);
	m_typeSizes[9] = sizeof(GraphNode_CustomNav);
}

CGraphNodeManager::~CGraphNodeManager()
{
	Clear(~0);
}

void CGraphNodeManager::Clear(IAISystem::tNavCapMask typeMask)
{
	for (int i = 0, count = int(m_buckets.size()); i < count; ++i)
	{
		if (m_buckets[i] && Match(m_buckets[i]->type, typeMask))
		{
			delete[] (reinterpret_cast<char*>(m_buckets[i]));
			m_buckets[i] = 0;
		}
	}

	for (unsigned i = 0, numTypes = m_freeBuckets.size(); i < numTypes; ++i)
	{
		IAISystem::tNavCapMask type = TypeFromTypeIndex(i);

		if (Match(type, typeMask))
		{
			m_freeBuckets[i] = BucketHeader::InvalidNextFreeBucketIdx;
		}
	}
}

unsigned CGraphNodeManager::CreateNode(IAISystem::tNavCapMask type, const Vec3& pos, unsigned ID)
{
	static_assert(BUCKET_SIZE <= 255, "Invalid bucket size!");

	int typeIndex = TypeIndexFromType(type);
	if (typeIndex < 0)
		return 0;

	if (typeIndex >= (int)m_freeBuckets.size())
	{
		m_freeBuckets.resize(typeIndex + 1, BucketHeader::InvalidNextFreeBucketIdx);
	}

	const int typeSize = TypeSizeFromTypeIndex(typeIndex);

	uint16 freeBucketIdx = m_freeBuckets[typeIndex];

	if (freeBucketIdx == BucketHeader::InvalidNextFreeBucketIdx)
	{
		int bucketAllocationSize = sizeof(BucketHeader) + typeSize * BUCKET_SIZE;
		BucketHeader* pHeader = reinterpret_cast<BucketHeader*>(new char[bucketAllocationSize]);
		pHeader->type = type.GetFullMask();
		pHeader->nextAvailable = 0;
		uint8* pNodes = (uint8*)(pHeader + 1);
		for (int i = 0; i < BUCKET_SIZE - 1; ++i)
			pNodes[i * typeSize] = (i + 1);
		pNodes[(BUCKET_SIZE - 1) * typeSize] = BucketHeader::InvalidNextAvailableIdx;
		pHeader->nextFreeBucketIdx = BucketHeader::InvalidNextFreeBucketIdx;
		pHeader->SetNodeSize(typeSize);
		pHeader->nodes = (uint8*)(pHeader + 1);

		freeBucketIdx = (uint16)m_buckets.size();
		m_buckets.resize((size_t)(freeBucketIdx + 1));
		m_buckets[freeBucketIdx] = pHeader;
		m_freeBuckets[typeIndex] = freeBucketIdx;
	}

	BucketHeader* freeBucket = m_buckets[freeBucketIdx];

	int allocIdx = freeBucket->nextAvailable;
	assert(allocIdx < BUCKET_SIZE);

	freeBucket->nextAvailable = freeBucket->nodes[allocIdx * typeSize];

	if (freeBucket->nextAvailable == BucketHeader::InvalidNextAvailableIdx)
	{
		m_freeBuckets[typeIndex] = freeBucket->nextFreeBucketIdx;
		freeBucket->nextFreeBucketIdx = BucketHeader::InvalidNextFreeBucketIdx;
	}

	GraphNode* pNode = reinterpret_cast<GraphNode*>(freeBucket->nodes + allocIdx * typeSize);

	IAISystem::ENavigationType actualType = (IAISystem::ENavigationType)(unsigned)type;
	switch (actualType)
	{
	case IAISystem::NAV_UNSET:
		pNode = new(pNode) GraphNode_Unset(actualType, pos, ID);
		break;
	case IAISystem::NAV_TRIANGULAR:
		pNode = new(pNode) GraphNode_Triangular(actualType, pos, ID);
		break;
	case IAISystem::NAV_WAYPOINT_HUMAN:
		pNode = new(pNode) GraphNode_WaypointHuman(actualType, pos, ID);
		break;
	case IAISystem::NAV_WAYPOINT_3DSURFACE:
		pNode = new(pNode) GraphNode_Waypoint3DSurface(actualType, pos, ID);
		break;
	case IAISystem::NAV_FLIGHT:
		pNode = new(pNode) GraphNode_Flight(actualType, pos, ID);
		break;
	case IAISystem::NAV_VOLUME:
		pNode = new(pNode) GraphNode_Volume(actualType, pos, ID);
		break;
	case IAISystem::NAV_ROAD:
		pNode = new(pNode) GraphNode_Road(actualType, pos, ID);
		break;
	case IAISystem::NAV_SMARTOBJECT:
		pNode = new(pNode) GraphNode_SmartObject(actualType, pos, ID);
		break;
	case IAISystem::NAV_FREE_2D:
		pNode = new(pNode) GraphNode_Free2D(actualType, pos, ID);
		break;
	case IAISystem::NAV_CUSTOM_NAVIGATION:
		pNode = new(pNode) GraphNode_CustomNav(actualType, pos, ID);
		break;
	}

	assert(pNode->nRefCount == 0);

	return ((freeBucketIdx << BUCKET_SIZE_SHIFT) + allocIdx) + 1;
}

void CGraphNodeManager::DestroyNode(unsigned index)
{
	// GraphNodes are optimized for size, and we avoid a virtual table by not having a virtual destructor.

	GraphNode* pNode = GetNode(index);
	switch (pNode->navType)
	{
	case IAISystem::NAV_UNSET:
		static_cast<GraphNode_Unset*>(pNode)->~GraphNode_Unset();
		break;
	case IAISystem::NAV_TRIANGULAR:
		static_cast<GraphNode_Triangular*>(pNode)->~GraphNode_Triangular();
		break;
	case IAISystem::NAV_WAYPOINT_HUMAN:
		static_cast<GraphNode_WaypointHuman*>(pNode)->~GraphNode_WaypointHuman();
		break;
	case IAISystem::NAV_WAYPOINT_3DSURFACE:
		static_cast<GraphNode_Waypoint3DSurface*>(pNode)->~GraphNode_Waypoint3DSurface();
		break;
	case IAISystem::NAV_FLIGHT:
		static_cast<GraphNode_Flight*>(pNode)->~GraphNode_Flight();
		break;
	case IAISystem::NAV_VOLUME:
		static_cast<GraphNode_Volume*>(pNode)->~GraphNode_Volume();
		break;
	case IAISystem::NAV_ROAD:
		static_cast<GraphNode_Road*>(pNode)->~GraphNode_Road();
		break;
	case IAISystem::NAV_SMARTOBJECT:
		static_cast<GraphNode_SmartObject*>(pNode)->~GraphNode_SmartObject();
		break;
	case IAISystem::NAV_FREE_2D:
		static_cast<GraphNode_Free2D*>(pNode)->~GraphNode_Free2D();
		break;
	case IAISystem::NAV_CUSTOM_NAVIGATION:
		static_cast<GraphNode_CustomNav*>(pNode)->~GraphNode_CustomNav();
		break;
	default:
		break;
	}

	// Push node onto bucket free list
	uint16 bucketIdx = (index - 1) / BUCKET_SIZE;
	uint8 idxInBucket = (index - 1) % BUCKET_SIZE;
	int typeIndex = TypeIndexFromType(pNode->navType);

	BucketHeader* pBucket = m_buckets[bucketIdx];
	if (pBucket->nextAvailable == BucketHeader::InvalidNextAvailableIdx)
	{
		pBucket->nextFreeBucketIdx = m_freeBuckets[typeIndex];
		m_freeBuckets[typeIndex] = bucketIdx;
	}

	pBucket->nodes[idxInBucket * TypeSizeFromTypeIndex(typeIndex)] = pBucket->nextAvailable;
	pBucket->nextAvailable = idxInBucket;
}

size_t CGraphNodeManager::NodeMemorySize() const
{
	size_t mem = 0;

	for (int i = 0, count = int(m_buckets.size()); i < count; ++i)
	{
		if (!m_buckets[i])
			continue;

		unsigned typeSize = TypeSizeFromTypeIndex(TypeIndexFromType(m_buckets[i]->type));
		mem += sizeof(BucketHeader) + typeSize * BUCKET_SIZE;
	}
	return mem;
}

void CGraphNodeManager::GetMemoryStatistics(ICrySizer* pSizer)
{
	pSizer->AddContainer(m_buckets);

	for (size_t i = 0; i < m_buckets.size(); ++i)
	{
		if (m_buckets[i])
		{
			size_t typeSize = TypeSizeFromTypeIndex(TypeIndexFromType(m_buckets[i]->type));
			pSizer->AddObject(m_buckets[i], sizeof(BucketHeader) + typeSize * BUCKET_SIZE);
		}
	}
}
