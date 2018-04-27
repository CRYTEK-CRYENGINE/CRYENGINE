// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __OFFMESH_LINKS_H__
#define __OFFMESH_LINKS_H__

#pragma once

#include "../MNM/MNM.h"

struct NavigationMesh;
struct IAIPathAgent;

namespace MNM
{
//////////////////////////////////////////////////////////////////////////
/// One of this objects is bound to every Navigation Mesh
/// Keeps track of off-mesh links per Tile
/// Each tile can have up to 1024 Triangle links (limited by the Tile link structure within the mesh)
///
/// Some triangles in the NavigationMesh will have a special link with an index
/// which allows to access this off-mesh data
struct OffMeshNavigation
{
private:

	//////////////////////////////////////////////////////////////////////////
	//Note: This structure could hold any data for the link
	//      For the time being it will store the necessary SO info to interface with the current SO system
	struct TriangleLink
	{
		TriangleID    startTriangleID;
		TriangleID    endTriangleID;
		OffMeshLinkID linkID;
	};
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	struct TileLinks
	{
		TileLinks()
			: triangleLinks(NULL)
			, triangleLinkCount(0)
		{

		}

		~TileLinks()
		{
			SAFE_DELETE_ARRAY(triangleLinks);
		}

		void CopyLinks(TriangleLink* links, uint16 linkCount);

		TriangleLink* triangleLinks;
		uint16        triangleLinkCount;
	};

public:

	struct QueryLinksResult
	{
		QueryLinksResult(const TriangleLink* _firstLink, uint16 _linkCount)
			: pFirstLink(_firstLink)
			, currentLink(0)
			, linkCount(_linkCount)
		{

		}

		WayTriangleData GetNextTriangle() const
		{
			if (currentLink < linkCount)
			{
				currentLink++;
				return WayTriangleData(pFirstLink[currentLink - 1].endTriangleID, pFirstLink[currentLink - 1].linkID);
			}

			return WayTriangleData(0, 0);
		}

	private:
		const TriangleLink* pFirstLink;
		mutable uint16      currentLink;
		uint16              linkCount;
	};

#if DEBUG_MNM_ENABLED
	struct ProfileMemoryStats
	{
		ProfileMemoryStats()
			: offMeshTileLinksMemory(0)
			, totalSize(0)
		{

		}

		size_t offMeshTileLinksMemory;
		size_t totalSize;
	};
#endif

	void             AddLink(NavigationMesh& navigationMesh, const TriangleID startTriangleID, const TriangleID endTriangleID, OffMeshLinkID& linkID);
	void             RemoveLink(NavigationMesh& navigationMesh, const TriangleID boundTriangleID, const OffMeshLinkID linkID);
	void             InvalidateLinks(const TileID tileID);

	QueryLinksResult GetLinksForTriangle(const TriangleID triangleID, const uint16 index) const;

#if DEBUG_MNM_ENABLED
	ProfileMemoryStats GetMemoryStats(ICrySizer* pSizer) const;
#endif

private:
	typedef std::unordered_map<TileID, TileLinks, stl::hash_uint32> TTilesLinks;

	TTilesLinks          m_tilesLinks;

	static OffMeshLinkID s_linkIDGenerator;
};
}

#endif //__OFFMESH_LINKS_H__
