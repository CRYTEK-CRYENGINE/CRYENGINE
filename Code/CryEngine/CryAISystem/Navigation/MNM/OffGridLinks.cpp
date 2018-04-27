// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OffGridLinks.h"

#include "../NavigationSystem/NavigationSystem.h"
#include "Tile.h"

// This needs to be static because OffMeshNavigationManager expects all link ids to be
// unique, and there is one of these per navigation mesh.
MNM::OffMeshLinkID MNM::OffMeshNavigation::s_linkIDGenerator = MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID;

void MNM::OffMeshNavigation::TileLinks::CopyLinks(TriangleLink* links, uint16 linkCount)
{
	if (triangleLinkCount != linkCount)
	{
		SAFE_DELETE_ARRAY(triangleLinks);

		triangleLinkCount = linkCount;

		if (linkCount)
			triangleLinks = new TriangleLink[linkCount];
	}

	if (linkCount)
		memcpy(triangleLinks, links, sizeof(TriangleLink) * linkCount);
}

void MNM::OffMeshNavigation::AddLink(NavigationMesh& navigationMesh, const TriangleID startTriangleID, const TriangleID endTriangleID, OffMeshLinkID& linkID)
{
	// We only support 1024 links per tile
	const int kMaxTileLinks = 1024;

	//////////////////////////////////////////////////////////////////////////
	/// Figure out the tile to operate on.
	MNM::TileID tileID = MNM::ComputeTileID(startTriangleID);

	//////////////////////////////////////////////////////////////////////////
	// Attempt to find the current link data for this tile or create one if not already cached
	TileLinks& tileLinks = m_tilesLinks[tileID];
	CRY_ASSERT_TRACE(tileLinks.triangleLinkCount + 1 < kMaxTileLinks, ("Maximum number of offmesh links in tile reached! (tileID = %u, links count = %d)", tileID, tileLinks.triangleLinkCount));

	//////////////////////////////////////////////////////////////////////////
	// Find if we have entries for this triangle, if not insert at the end
	uint16 triangleIdx = tileLinks.triangleLinkCount;  // Default to end
	{
		for (uint16 idx = 0; idx < tileLinks.triangleLinkCount; ++idx)
		{
			if (tileLinks.triangleLinks[idx].startTriangleID == startTriangleID)
			{
				triangleIdx = idx;
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// If a valid link ID has been passed in, use it instead of generating a new one
	// This can occur when re-adding existing links to a modified mesh.
	if (linkID == MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID)
	{
		// Generate new link id
		// NOTE: Zero is an invalid link ID
		while (++s_linkIDGenerator == MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID)
			;
		linkID = s_linkIDGenerator;
	}
	// A link ID should never be larger than the latest generated
	CRY_ASSERT_TRACE(linkID <= s_linkIDGenerator, ("A link ID should not be larger than the latest generated (linkID = %u, lastGenerated = %u)", linkID, s_linkIDGenerator));

	//////////////////////////////////////////////////////////////////////////
	// Begin insert/copy process
	TriangleLink tempTriangleLinks[kMaxTileLinks];

	// If not the first index, copy the current links up to the target index
	// into the temporary buffer
	if (triangleIdx > 0)
	{
		memcpy(tempTriangleLinks, tileLinks.triangleLinks, sizeof(TriangleLink) * triangleIdx);
	}

	// Insert the new link
	tempTriangleLinks[triangleIdx].startTriangleID = startTriangleID;
	tempTriangleLinks[triangleIdx].endTriangleID = endTriangleID;
	tempTriangleLinks[triangleIdx].linkID = linkID;

	// Now copy the remaining links after the insert, including the one just overwritten
	const int diffCount = tileLinks.triangleLinkCount - triangleIdx;
	if (diffCount > 0)
	{
		memcpy(&tempTriangleLinks[triangleIdx + 1], &tileLinks.triangleLinks[triangleIdx], sizeof(TriangleLink) * diffCount);
	}

	// Now copy the update list back into the source
	tileLinks.CopyLinks(tempTriangleLinks, tileLinks.triangleLinkCount + 1);

	//////////////////////////////////////////////////////////////////////////
	// Now add/update the link on the tile
	// NOTE: triangleLinkCount will have been updated due to the previous copy
	if (triangleIdx < (tileLinks.triangleLinkCount - 1))
	{
		// Update index value for all link entries for this triangle after this insert
		// since we shifted the indices with the memcpy
		TriangleID currentTriangleID = startTriangleID;
		for (uint16 idx = triangleIdx + 1; idx < tileLinks.triangleLinkCount; ++idx)
		{
			if (tileLinks.triangleLinks[idx].startTriangleID != currentTriangleID)
			{
				currentTriangleID = tileLinks.triangleLinks[idx].startTriangleID;
				navigationMesh.navMesh.UpdateOffMeshLinkForTile(tileID, currentTriangleID, idx);
			}
		}
	}
	else
	{
		// Add the new link to the off-mesh link to the tile itself
		navigationMesh.navMesh.AddOffMeshLinkToTile(tileID, startTriangleID, triangleIdx);
	}
}

void MNM::OffMeshNavigation::RemoveLink(NavigationMesh& navigationMesh, const TriangleID boundTriangleID, const OffMeshLinkID linkID)
{
	MNM::TileID tileID = ComputeTileID(boundTriangleID);

	TTilesLinks::iterator tileLinksIt = m_tilesLinks.find(tileID);

	if (tileLinksIt != m_tilesLinks.end())
	{
		const int maxTileLinks = 1024;
		TriangleLink tempTriangleLinks[maxTileLinks];

		TileLinks& tileLinks = tileLinksIt->second;

		//Note: Should be ok to copy this way, we are not going to have many links per tile...
		uint16 copyCount = 0;
		for (uint16 triIdx = 0; triIdx < tileLinks.triangleLinkCount; ++triIdx)
		{
			TriangleLink& triangleLink = tileLinks.triangleLinks[triIdx];
			if (triangleLink.linkID != linkID)
			{
				// Retain link
				tempTriangleLinks[copyCount] = triangleLink;
				copyCount++;
			}
		}

		// Copy the retained links to the cached link list
		tileLinks.CopyLinks(tempTriangleLinks, copyCount);

		//Update triangle off-mesh indices
		uint16 boundTriangleLeftLinks = 0;
		TriangleID currentTriangleID(0);
		for (uint16 triIdx = 0; triIdx < tileLinks.triangleLinkCount; ++triIdx)
		{
			TriangleLink& triangleLink = tileLinks.triangleLinks[triIdx];

			if (currentTriangleID != triangleLink.startTriangleID)
			{
				currentTriangleID = triangleLink.startTriangleID;
				navigationMesh.navMesh.UpdateOffMeshLinkForTile(tileID, currentTriangleID, triIdx);
			}
			boundTriangleLeftLinks += (currentTriangleID == boundTriangleID) ? 1 : 0;
		}

		if (!boundTriangleLeftLinks)
		{
			navigationMesh.navMesh.RemoveOffMeshLinkFromTile(tileID, boundTriangleID);
		}
	}
}

void MNM::OffMeshNavigation::InvalidateLinks(const TileID tileID)
{
	// Remove all links associated with the provided tile ID

	TTilesLinks::iterator tileIt = m_tilesLinks.find(tileID);
	if (tileIt != m_tilesLinks.end()) // notice: if this iterator points to end(), it means that we never attempted to add a link to given tile
	{
		m_tilesLinks.erase(tileIt);
	}
}

MNM::OffMeshNavigation::QueryLinksResult MNM::OffMeshNavigation::GetLinksForTriangle(const TriangleID triangleID, const uint16 index) const
{
	const MNM::TileID& tileID = ComputeTileID(triangleID);

	TTilesLinks::const_iterator tileCit = m_tilesLinks.find(tileID);

	if (tileCit != m_tilesLinks.end())
	{
		const TileLinks& tileLinks = tileCit->second;
		if (index < tileLinks.triangleLinkCount)
		{
			TriangleLink* pFirstTriangle = &tileLinks.triangleLinks[index];
			uint16 linkCount = 1;

			for (uint16 triIdx = index + 1; triIdx < tileLinks.triangleLinkCount; ++triIdx)
			{
				if (tileLinks.triangleLinks[triIdx].startTriangleID == triangleID)
				{
					linkCount++;
				}
				else
				{
					break;
				}
			}

			return QueryLinksResult(pFirstTriangle, linkCount);
		}

	}

	return QueryLinksResult(NULL, 0);
}

#if DEBUG_MNM_ENABLED

MNM::OffMeshNavigation::ProfileMemoryStats MNM::OffMeshNavigation::GetMemoryStats(ICrySizer* pSizer) const
{
	ProfileMemoryStats memoryStats;

	// Tile links memory
	size_t previousSize = pSizer->GetTotalSize();
	for (TTilesLinks::const_iterator tileIt = m_tilesLinks.begin(); tileIt != m_tilesLinks.end(); ++tileIt)
	{
		pSizer->AddObject(&(tileIt->second), tileIt->second.triangleLinkCount * sizeof(TriangleLink));
	}
	pSizer->AddHashMap(m_tilesLinks);

	memoryStats.offMeshTileLinksMemory += (pSizer->GetTotalSize() - previousSize);

	//Object size
	previousSize = pSizer->GetTotalSize();
	pSizer->AddObjectSize(this);

	memoryStats.totalSize = pSizer->GetTotalSize() - previousSize;

	return memoryStats;
}

#endif
