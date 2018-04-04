// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __GRAPHLINKMANAGER_H__
#define __GRAPHLINKMANAGER_H__

#include "GraphStructures.h"

class CGraphLinkManager
{
public:
	enum
	{
		BUCKET_SIZE_SHIFT = 7
	};
	enum
	{
		BUCKET_SIZE = 128
	};

	CGraphLinkManager();
	~CGraphLinkManager();

	void           Clear();

	unsigned       CreateLink();
	void           DestroyLink(unsigned linkIndex);

	ILINE unsigned GetNextNode(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].nextNodeIndex;
	}
	void           SetNextNode(unsigned linkId, unsigned nodeIndex) { GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].nextNodeIndex = nodeIndex; }

	ILINE unsigned GetPrevNode(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[GetReversed(linkId ^ 1)].nextNodeIndex;
	}

	ILINE unsigned GetNextLink(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].nextLinkIndex;
	}
	void SetNextLink(unsigned linkId, unsigned nextLink)
	{
		GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].nextLinkIndex = nextLink;
	}

	void SetMaxWaterDepth(unsigned linkId, float depth)
	{
		depth *= 100.0f;
		Limit<float>(depth, (float)-std::numeric_limits<short>::max(), (float)std::numeric_limits<short>::max());

		int16 depthInCm = NavGraphUtils::InCentimeters(depth);
		GetBidirectionalData(linkId)->directionalData[1].waterDepthInCmLow = depthInCm & 0xff;
		GetBidirectionalData(linkId)->directionalData[1].waterDepthInCmHigh = (depthInCm >> 8);
	}

	void SetMinWaterDepth(unsigned linkId, float depth)
	{
		depth *= 100.0f;
		Limit<float>(depth, (float)-std::numeric_limits<short>::max(), (float)std::numeric_limits<short>::max());

		int16 depthInCm = NavGraphUtils::InCentimeters(depth);
		GetBidirectionalData(linkId)->directionalData[0].waterDepthInCmLow = depthInCm & 0xff;
		GetBidirectionalData(linkId)->directionalData[0].waterDepthInCmHigh = (depthInCm >> 8);
	}

	ILINE float GetMaxWaterDepth(unsigned linkId) const
	{
		return NavGraphUtils::InMeters(GetMaxWaterDepthInCm(linkId));
	}
	ILINE float GetMinWaterDepth(unsigned linkId) const
	{
		return NavGraphUtils::InMeters(GetMinWaterDepthInCm(linkId));
	}

	ILINE int16 GetMaxWaterDepthInCm(unsigned linkId) const
	{
		int low = GetBidirectionalData(linkId)->directionalData[1].waterDepthInCmLow;
		int high = GetBidirectionalData(linkId)->directionalData[1].waterDepthInCmHigh;
		return (int16)((high << 8) | low);
	}

	ILINE int16 GetMinWaterDepthInCm(unsigned linkId) const
	{
		int low = GetBidirectionalData(linkId)->directionalData[0].waterDepthInCmLow;
		int high = GetBidirectionalData(linkId)->directionalData[0].waterDepthInCmHigh;
		return (int16)((high << 8) | low);
	}

	/// Use this when setting up the radius during creation
	void        SetRadius(unsigned linkId, float radius);
	void        SetRadiusInCm(unsigned linkId, int16 radius);

	ILINE float GetRadius(unsigned linkId) const
	{
		return NavGraphUtils::InMeters(GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].maxRadiusInCm);
	}
	ILINE float GetOrigRadius(unsigned linkId) const
	{
		return NavGraphUtils::InMeters(GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].origMaxRadiusInCm);
	}

	ILINE int16 GetRadiusInCm(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].maxRadiusInCm;
	}
	ILINE int16 GetOrigRadiusInCm(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].origMaxRadiusInCm;
	}

	/// Use this when modifying the radius during game
	void ModifyRadius(unsigned linkId, float radius)
	{
		int radiusInCm = (int)(radius * 100.0f);
		Limit<int>(radiusInCm, -std::numeric_limits<short>::max(), std::numeric_limits<short>::max());
		GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].maxRadiusInCm = radiusInCm;
	}

	/// Use this when modifying the radius during game
	void ModifyRadiusInCm(unsigned linkId, int16 radius)
	{
		GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].maxRadiusInCm = radius;
	}

	/// Indicates if a link has been changed from its original value
	ILINE bool IsLinkModified(unsigned linkId) const
	{
		return abs(GetRadiusInCm(linkId) - GetOrigRadiusInCm(linkId)) > 1;
	}

	/// Restores the link radius to its original value
	void RestoreLink(unsigned linkId)
	{
		GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].maxRadiusInCm = GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].origMaxRadiusInCm;
	}

	ILINE bool IsNewLink(unsigned linkId) const
	{
		if (abs(GetRadiusInCm(linkId) + 25) < 25)
			return true;
		return false;
	}

	ILINE unsigned int GetStartIndex(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].startIndex;
	}

	void               SetStartIndex(unsigned linkId, unsigned int index);
	ILINE unsigned int GetEndIndex(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].endIndex;
	}
	void        SetEndIndex(unsigned linkId, unsigned int index);

	void        SetEdgeCenter(unsigned linkId, const Vec3& center) { GetBidirectionalData(linkId)->vEdgeCenter = center; }
	ILINE Vec3& GetEdgeCenter(unsigned linkId)
	{
		return GetBidirectionalData(linkId)->vEdgeCenter;
	}
	ILINE const Vec3& GetEdgeCenter(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->vEdgeCenter;
	}

	void        SetExposure(unsigned linkId, float fExposure) { GetBidirectionalData(linkId)->fExposure = fExposure; }
	ILINE float GetExposure(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->fExposure;
	}

	// NOTE Apr 26, 2007: <pvl> "simplicity" of a link makes sense only with
	// human waypoint links.  For criterion of simplicity see
	// CWaypointHumanNavRegion::CheckWaypointLinkWalkability().
	ILINE unsigned IsSimple(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[0].simplePassabilityCheck;
	}
	void SetSimple(unsigned linkId, bool linear)
	{
		GraphLinkBidirectionalData* data = GetBidirectionalData(linkId);
		data->directionalData[0].simplePassabilityCheck = linear;
		data->directionalData[1].simplePassabilityCheck = true;
	}

	ILINE bool SimplicityComputed(unsigned linkId) const
	{
		return GetBidirectionalData(linkId)->directionalData[1].simplePassabilityCheck;
	}

	// NOTE Oct 28, 2009: <pvl> memory allocated for link data buckets
	size_t MemoryAllocated() const;

	// NOTE Oct 28, 2009: <pvl> bucket memory that's allocated but on the free list
	size_t MemoryUnused() const;

	// NOTE Oct 28, 2009: <pvl> just for convenience - difference between allocated and unused
	size_t MemoryUsed() const;

	// NOTE Oct 29, 2009: <pvl> deallocate buckets that only contain unused link data
	void CollectGarbage();

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		pSizer->AddContainer(m_buckets);
		for (size_t i = 0; i < m_buckets.size(); ++i)
			pSizer->AddObject(m_buckets[i], sizeof(GraphLinkBidirectionalData), BUCKET_SIZE);
	};

private:
	// The low bit of the pointer is a flag - it must be masked out.
	GraphLinkBidirectionalData*       GetBidirectionalData(unsigned linkId);
	const GraphLinkBidirectionalData* GetBidirectionalData(unsigned linkId) const;

	bool                              GetReversed(unsigned linkId) const { return (linkId & 1) != 0; }

	bool                              IsBucketInUse(unsigned bucketIndex) const;
	void                              MarkBucketForDeletion(unsigned bucketIndex);
	void                              MarkUnused(GraphLinkBidirectionalData* data) const;
	void                              MarkUsed(GraphLinkBidirectionalData* data) const;
	bool                              IsUsed(const GraphLinkBidirectionalData* data) const;
	void                              MarkToBeDeleted(GraphLinkBidirectionalData* data) const;
	bool                              IsToBeDeleted(const GraphLinkBidirectionalData* data) const;

	std::vector<GraphLinkBidirectionalData*> m_buckets;
	unsigned m_freeList;
};

inline CGraphLinkManager::CGraphLinkManager()
	: m_freeList(0)
{
}

inline CGraphLinkManager::~CGraphLinkManager()
{
	Clear();
}

inline void CGraphLinkManager::Clear()
{
	for (int i = 0, count = int(m_buckets.size()); i < count; ++i)
		delete[] m_buckets[i];
	m_buckets.clear();

	m_freeList = 0;
}

inline void CGraphLinkManager::SetRadius(unsigned linkId, float radius)
{
	int radiusInCm = NavGraphUtils::InCentimeters(radius);
	// NOTE Jul 27, 2007: <pvl> radius being 0.0 seems to have a special interpretation,
	// don't let it go to zero (i.e. completely impassable) just because of
	// quantization - set it to 1cm instead.  This prevents graph consistency
	// checking from getting confused and flagging false errors.
	if (radiusInCm == 0 && radius != 0)
		radiusInCm = (radius > 0);

	GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].maxRadiusInCm = radiusInCm;
	GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].origMaxRadiusInCm = radiusInCm;
}

inline void CGraphLinkManager::SetRadiusInCm(unsigned linkId, int16 radiusInCm)
{
	// NOTE Jul 27, 2007: <pvl> radius being 0.0 seems to have a special interpretation,
	// don't let it go to zero (i.e. completely impassable) just because of
	// quantization - set it to 1cm instead.  This prevents graph consistency
	// checking from getting confused and flagging false errors.
	GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].maxRadiusInCm = radiusInCm;
	GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].origMaxRadiusInCm = radiusInCm;
}

inline void CGraphLinkManager::SetStartIndex(unsigned linkId, unsigned int index)
{
	assert(index < 3 && index >= 0);
	GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].startIndex = index;
}

inline void CGraphLinkManager::SetEndIndex(unsigned linkId, unsigned int index)
{
	assert(index < 3 && index >= 0);
	GetBidirectionalData(linkId)->directionalData[GetReversed(linkId)].endIndex = index;
}

inline GraphLinkBidirectionalData* CGraphLinkManager::GetBidirectionalData(unsigned linkId)
{
	unsigned bidirectionalIndex = (linkId >> 1) - 1;
	return &m_buckets[bidirectionalIndex >> BUCKET_SIZE_SHIFT][bidirectionalIndex & (BUCKET_SIZE - 1)];
}

inline const GraphLinkBidirectionalData* CGraphLinkManager::GetBidirectionalData(unsigned linkId) const
{
	unsigned bidirectionalIndex = (linkId >> 1) - 1;
	return &m_buckets[bidirectionalIndex >> BUCKET_SIZE_SHIFT][bidirectionalIndex & (BUCKET_SIZE - 1)];
}

inline void CGraphLinkManager::MarkUnused(GraphLinkBidirectionalData* data) const
{
	data->directionalData[1].nextLinkIndex = 0x7ffff;
}

inline void CGraphLinkManager::MarkUsed(GraphLinkBidirectionalData* data) const
{
	data->directionalData[1].nextLinkIndex = 0;
}

inline bool CGraphLinkManager::IsUsed(const GraphLinkBidirectionalData* data) const
{
	return data->directionalData[1].nextLinkIndex != 0x7ffff;
}

inline void CGraphLinkManager::MarkToBeDeleted(GraphLinkBidirectionalData* data) const
{
	data->directionalData[1].nextLinkIndex = 0x80000;
}

inline bool CGraphLinkManager::IsToBeDeleted(const GraphLinkBidirectionalData* data) const
{
	return data->directionalData[1].nextLinkIndex == 0x80000;
}

inline unsigned CGraphLinkManager::CreateLink()
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Navigation, 0, "Links");
	if (!m_freeList)
	{
		m_buckets.push_back(new GraphLinkBidirectionalData[BUCKET_SIZE]);
		GraphLinkBidirectionalData* pLinks = m_buckets.back();
		pLinks[0].directionalData[0].nextLinkIndex = m_freeList;
		MarkUnused(&pLinks[0]);
		unsigned nextLink = m_freeList;
		for (int i = 1; i < BUCKET_SIZE; ++i)
		{
			pLinks[i].directionalData[0].nextLinkIndex = nextLink;
			MarkUnused(&pLinks[i]);
			nextLink = ((((m_buckets.size() - 1) << BUCKET_SIZE_SHIFT) + i) + 1) << 1;
		}
		m_freeList = nextLink;
	}

	unsigned link = m_freeList;
	GraphLinkBidirectionalData* bidirectionalData = GetBidirectionalData(link);
	m_freeList = bidirectionalData->directionalData[0].nextLinkIndex;
	bidirectionalData->directionalData[0].nextLinkIndex = 0;
	MarkUsed(bidirectionalData);
	return link;
}

inline void CGraphLinkManager::DestroyLink(unsigned linkIndex)
{
	linkIndex = linkIndex & ~1;
	if (GraphLinkBidirectionalData* pData = GetBidirectionalData(linkIndex))
	{
		*pData = GraphLinkBidirectionalData();
		pData->directionalData[0].nextLinkIndex = m_freeList;
		MarkUnused(pData);
		m_freeList = linkIndex;
	}
}

inline size_t CGraphLinkManager::MemoryAllocated() const
{
	size_t mem = 0;

	for (int i = 0, count = int(m_buckets.size()); i < count; ++i)
	{
		if (!m_buckets[i])
			continue;
		mem += sizeof(GraphLinkBidirectionalData) * BUCKET_SIZE;
	}

	return mem;
}

inline size_t CGraphLinkManager::MemoryUnused() const
{
	size_t mem = 0;

	unsigned freeLink = m_freeList;
	while (freeLink)
	{
		mem += sizeof(GraphLinkBidirectionalData);
		const GraphLinkBidirectionalData* freeLinkData = GetBidirectionalData(freeLink);
		freeLink = freeLinkData->directionalData[0].nextLinkIndex;
	}

	return mem;
}

inline size_t CGraphLinkManager::MemoryUsed() const
{
	return MemoryAllocated() - MemoryUnused();
}

// NOTE Nov 2, 2009: <pvl> bucket is in use if any of the slots it contains is in use
inline bool CGraphLinkManager::IsBucketInUse(unsigned bucketIndex) const
{
	GraphLinkBidirectionalData* slotData = m_buckets[bucketIndex];
	for (unsigned slot = 0; slot < BUCKET_SIZE; ++slot, ++slotData)
	{
		if (IsUsed(slotData))
			return true;
	}
	return false;
}

inline void CGraphLinkManager::MarkBucketForDeletion(unsigned bucketIndex)
{
	GraphLinkBidirectionalData* slotData = m_buckets[bucketIndex];
	for (unsigned slot = 0; slot < BUCKET_SIZE; ++slot, ++slotData)
		MarkToBeDeleted(slotData);
}

inline void CGraphLinkManager::CollectGarbage()
{
	std::vector<int> bucketsToDelete;
	bucketsToDelete.reserve(m_buckets.size());

	for (int i = 0, count = int(m_buckets.size()); i < count; ++i)
	{
		if (m_buckets[i] == 0) continue;

		if (!IsBucketInUse(i))
		{
			bucketsToDelete.push_back(i);
			MarkBucketForDeletion(i);
		}
	}

	// NOTE Nov 2, 2009: <pvl> now walk the free list and remove and link data slots
	// from it that live in buckets about to be deleted
	if (m_freeList)
	{
		// NOTE Nov 2, 2009: <pvl> unfortunately we have to treat 'm_freeList'
		// as a special case
		GraphLinkBidirectionalData* data = GetBidirectionalData(m_freeList);
		while (IsToBeDeleted(data))
		{
			m_freeList = data->directionalData[0].nextLinkIndex;
			if (m_freeList == 0) break;
			data = GetBidirectionalData(m_freeList);
		}

		if (m_freeList)
		{
			unsigned freeLink = m_freeList;
			while (freeLink)
			{
				unsigned markedLink = freeLink;
				const GraphLinkBidirectionalData* markedLinkData = GetBidirectionalData(markedLink);
				do
				{
					markedLink = markedLinkData->directionalData[0].nextLinkIndex;
					if (markedLink == 0) break;
					markedLinkData = GetBidirectionalData(markedLink);
				}
				while (IsToBeDeleted(markedLinkData));

				GetBidirectionalData(freeLink)->directionalData[0].nextLinkIndex = markedLink;
				freeLink = markedLink;
			}
		}
	}

	for (int i = 0, count = int(bucketsToDelete.size()); i < count; ++i)
	{
		delete[] m_buckets[bucketsToDelete[i]];
		m_buckets[bucketsToDelete[i]] = 0;
	}
}

#endif //__GRAPHLINKMANAGER_H__
