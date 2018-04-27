// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Tile.h"
#include <CryCore/TypeInfo_impl.h>
#include "DebugDrawContext.h"

STRUCT_INFO_BEGIN(MNM::Tile::STriangle)
STRUCT_VAR_INFO(vertex, TYPE_ARRAY(3, TYPE_INFO(Index)))
STRUCT_BITFIELD_INFO(linkCount, TYPE_INFO(uint16), 4)
STRUCT_BITFIELD_INFO(firstLink, TYPE_INFO(uint16), 12)
STRUCT_VAR_INFO(islandID, TYPE_INFO(StaticIslandID))
STRUCT_INFO_END(MNM::Tile::STriangle)

STRUCT_INFO_BEGIN(MNM::Tile::SBVNode)
STRUCT_BITFIELD_INFO(leaf, TYPE_INFO(uint16), 1)
STRUCT_BITFIELD_INFO(offset, TYPE_INFO(uint16), 15)
STRUCT_VAR_INFO(aabb, TYPE_INFO(AABB))
STRUCT_INFO_END(MNM::Tile::SBVNode)

STRUCT_INFO_BEGIN(MNM::Tile::SLink)
STRUCT_BITFIELD_INFO(side, TYPE_INFO(uint32), 4)
STRUCT_BITFIELD_INFO(edge, TYPE_INFO(uint32), 2)
STRUCT_BITFIELD_INFO(triangle, TYPE_INFO(uint32), 10)
STRUCT_INFO_END(MNM::Tile::SLink)

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
	#define TILE_VALIDATE() ValidateTriangleLinks()
#else
	#define TILE_VALIDATE()
#endif

namespace MNM
{
STile::STile()
	: triangles(nullptr)
	, vertices(nullptr)
	, nodes(nullptr)
	, links(nullptr)
	, triangleCount(0)
	, vertexCount(0)
	, nodeCount(0)
	, linkCount(0)
	, hashValue(0)
#if MNM_USE_EXPORT_INFORMATION
	, connectivity()
#endif
{
}

void STile::GetTileData(Tile::STileData& outTileData) const
{
	outTileData.pTriangles = GetTriangles();
	outTileData.pVertices = GetVertices();
	outTileData.pLinks = GetLinks();
	outTileData.pNodes = GetBVNodes();

	outTileData.trianglesCount = triangleCount;
	outTileData.verticesCount = vertexCount;
	outTileData.linksCount = linkCount;
	outTileData.bvNodesCount = nodeCount;

	outTileData.hashValue = hashValue;
}

void STile::SetTriangles(std::unique_ptr<Tile::STriangle[]>&& pTriangles_, uint16 count)
{
	triangles = pTriangles_.release();
	triangleCount = count;
}

void STile::SetVertices(std::unique_ptr<Tile::Vertex[]>&& pVertices_, uint16 count)
{
	vertices = pVertices_.release();
	vertexCount = count;
}

void STile::SetNodes(std::unique_ptr<Tile::SBVNode[]>&& pNodes_, uint16 count)
{
	nodes = pNodes_.release();
	nodeCount = count;
}

void STile::SetLinks(std::unique_ptr<Tile::SLink[]>&& pLinks_, uint16 count)
{
	links = pLinks_.release();
	linkCount = count;
}

void STile::SetHashValue(uint32 value)
{
	hashValue = value;
}

template<typename TData, typename TCount>
void CopyTileData(const TData* pNewData, const TCount newCount, TData*& pOutData, TCount& outCount)
{
	if (outCount != newCount)
	{
		delete[] pOutData;
		pOutData = nullptr;

		outCount = newCount;

		if (newCount)
		{
			pOutData = new TData[newCount];
		}
	}

	if (newCount)
	{
		// #MNM_TODO pavloi 2016.07.26: assert fails for Tile::Vertex and Tile::SBVNode
		// #MNM_TODO pavloi 2016.07.26: can't use std::is_trivially_copyable - unexpectedly it's not implemented for clang/gcc in our build configurations
		//COMPILE_TIME_ASSERT(std::is_trivially_copyable<TData>::value);
		memcpy(pOutData, pNewData, sizeof(TData) * newCount);
	}
}

void STile::CopyTriangles(const Tile::STriangle* _triangles, uint16 count)
{
#if MNM_USE_EXPORT_INFORMATION
	InitConnectivity(triangleCount, count);
#endif

	CopyTileData(_triangles, count, triangles, triangleCount);
}

void STile::CopyVertices(const Tile::Vertex* _vertices, uint16 count)
{
	CopyTileData(_vertices, count, vertices, vertexCount);

	TILE_VALIDATE();
}

void STile::CopyNodes(const Tile::SBVNode* _nodes, uint16 count)
{
	CopyTileData(_nodes, count, nodes, nodeCount);
}

void STile::CopyLinks(const Tile::SLink* _links, uint16 count)
{
	CopyTileData(_links, count, links, linkCount);
}

void STile::AddOffMeshLink(const TriangleID triangleID, const uint16 offMeshIndex)
{
	TILE_VALIDATE();

	uint16 triangleIdx = ComputeTriangleIndex(triangleID);
	assert(triangleIdx < triangleCount);
	if (triangleIdx < triangleCount)
	{
		//Figure out if this triangle has already off-mesh connections
		//Off-mesh link is always the first if exists
		Tile::STriangle& triangle = triangles[triangleIdx];

		const size_t MaxLinkCount = 1024 * 6;
		Tile::SLink tempLinks[MaxLinkCount];

		bool hasOffMeshLink = links && (triangle.linkCount > 0) && (triangle.firstLink < linkCount) && (links[triangle.firstLink].side == Tile::SLink::OffMesh);

		// Try enabling DEBUG_MNM_DATA_CONSISTENCY_ENABLED if you get this
		CRY_ASSERT_MESSAGE(!hasOffMeshLink, "Not adding offmesh link, already exists");

		if (!hasOffMeshLink)
		{
			//Add off-mesh link for triangle
			{
				if (triangle.firstLink)
				{
					assert(links);
					PREFAST_ASSUME(links);
					memcpy(tempLinks, links, sizeof(Tile::SLink) * triangle.firstLink);
				}

				tempLinks[triangle.firstLink].side = Tile::SLink::OffMesh;
				tempLinks[triangle.firstLink].triangle = offMeshIndex;

				//Note this is not used
				tempLinks[triangle.firstLink].edge = 0;

				const int countDiff = linkCount - triangle.firstLink;
				if (countDiff > 0)
				{
					assert(links);
					memcpy(&tempLinks[triangle.firstLink + 1], &links[triangle.firstLink], sizeof(Tile::SLink) * countDiff);
				}
			}

			CopyLinks(tempLinks, linkCount + 1);

			//Re-arrange link indices for triangles
			triangle.linkCount++;

			for (uint16 tIdx = (triangleIdx + 1); tIdx < triangleCount; ++tIdx)
			{
				triangles[tIdx].firstLink++;
			}
		}
	}

	TILE_VALIDATE();
}

void STile::UpdateOffMeshLink(const TriangleID triangleID, const uint16 offMeshIndex)
{
	TILE_VALIDATE();

	uint16 triangleIndex = ComputeTriangleIndex(triangleID);
	assert(triangleIndex < triangleCount);
	if (triangleIndex < triangleCount)
	{
		//First link is always off-mesh if exists
		const uint16 linkIdx = triangles[triangleIndex].firstLink;
		assert(linkIdx < linkCount);
		if (linkIdx < linkCount)
		{
			Tile::SLink& link = links[linkIdx];
			assert(link.side == Tile::SLink::OffMesh);
			if (link.side == Tile::SLink::OffMesh)
			{
				link.triangle = offMeshIndex;
			}
		}
	}

	TILE_VALIDATE();
}

void STile::RemoveOffMeshLink(const TriangleID triangleID)
{
	TILE_VALIDATE();

	// Find link to be removed
	uint16 linkToRemoveIdx = 0xFFFF;
	uint16 boundTriangleIdx = ComputeTriangleIndex(triangleID);
	if (boundTriangleIdx < triangleCount)
	{
		const uint16 firstLink = triangles[boundTriangleIdx].firstLink;
		if ((triangles[boundTriangleIdx].linkCount > 0) && (firstLink < linkCount) && (links[firstLink].side == Tile::SLink::OffMesh))
			linkToRemoveIdx = firstLink;
	}

	// Try enabling DEBUG_MNM_DATA_CONSISTENCY_ENABLED if you get this
	CRY_ASSERT_MESSAGE(linkToRemoveIdx != 0xFFFF, "Trying to remove off mesh link that doesn't exist");

	if (linkToRemoveIdx != 0xFFFF)
	{
		assert(linkCount > 1);

		const size_t MaxLinkCount = 1024 * 6;
		Tile::SLink tempLinks[MaxLinkCount];

		if (linkToRemoveIdx)
			memcpy(tempLinks, links, sizeof(Tile::SLink) * linkToRemoveIdx);

		const int diffCount = linkCount - (linkToRemoveIdx + 1);
		if (diffCount > 0)
		{
			memcpy(&tempLinks[linkToRemoveIdx], &links[linkToRemoveIdx + 1], sizeof(Tile::SLink) * diffCount);
		}

		CopyLinks(tempLinks, linkCount - 1);

		//Re-arrange link indices for triangles
		triangles[boundTriangleIdx].linkCount--;

		for (uint16 tIdx = (boundTriangleIdx + 1); tIdx < triangleCount; ++tIdx)
		{
			triangles[tIdx].firstLink--;
		}
	}

	TILE_VALIDATE();
}

void STile::Swap(STile& other)
{
	std::swap(triangles, other.triangles);
	std::swap(vertices, other.vertices);
	std::swap(nodes, other.nodes);
	std::swap(links, other.links);

#if MNM_USE_EXPORT_INFORMATION
	InitConnectivity(triangleCount, other.triangleCount);
#endif

	std::swap(triangleCount, other.triangleCount);
	std::swap(vertexCount, other.vertexCount);
	std::swap(nodeCount, other.nodeCount);
	std::swap(linkCount, other.linkCount);
	std::swap(hashValue, other.hashValue);

	TILE_VALIDATE();
}

void STile::Destroy()
{
	delete[] triangles;
	triangles = 0;

	delete[] vertices;
	vertices = 0;

	delete[] nodes;
	nodes = 0;

	delete[] links;
	links = 0;

#if MNM_USE_EXPORT_INFORMATION
	SAFE_DELETE_ARRAY(connectivity.trianglesAccessible);
	connectivity.tileAccessible = 0;
#endif

	triangleCount = 0;
	vertexCount = 0;
	nodeCount = 0;
	linkCount = 0;
	hashValue = 0;
}

ColorF CalculateColorFromMultipleItems(uint32 itemIdx, uint32 itemsCount)
{
	if (itemsCount == 0)
		return Col_White;

	const float hueValue = itemIdx / (float)itemsCount;

	ColorF color;
	color.fromHSV(hueValue, 1.0f, 1.0f);
	color.a = 1.0f;
	return color;
}

void STile::Draw(size_t drawFlags, vector3_t origin, TileID tileID, const std::vector<float>& islandAreas, const ITriangleColorSelector& colorSelector) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const size_t expectedMaxTriCount = 64;
	const size_t expectedMaxOffmeshTriCount = 8;

	const ColorB triangleColorConnected(Col_Azure, 0.65f);
	const ColorB triangleColorBackface(Col_Gray, 0.65f);
	const ColorB triangleColorDisconnected(Col_Red, 0.65f);
	const ColorB boundaryColor(Col_Black);

	const Vec3 offset = origin.GetVec3() + Vec3(0.0f, 0.0f, 0.05f);
	const Vec3 loffset(offset + Vec3(0.0f, 0.0f, 0.0005f));

	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	SAuxGeomRenderFlags oldFlags = renderAuxGeom->GetRenderFlags();

	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	renderFlags.SetAlphaBlendMode(e_AlphaBlended);

	if (!(drawFlags & DrawTriangleBackfaces))
	{
		renderFlags.SetDepthWriteFlag(e_DepthWriteOff);
	}

	renderAuxGeom->SetRenderFlags(renderFlags);

	if (drawFlags & DrawTriangles)
	{
		ColorB currentColor = Col_Black;
		std::vector<Vec3> triVertices;
		triVertices.reserve(3 * expectedMaxTriCount);
		std::vector<Vec3> backfaceTriVertices;
		if (drawFlags & DrawTriangleBackfaces)
		{
			backfaceTriVertices.reserve(3 * expectedMaxTriCount);
		}

		for (size_t i = 0; i < triangleCount; ++i)
		{
			const Tile::STriangle& triangle = triangles[i];
			
			// Getting triangle color
#if MNM_USE_EXPORT_INFORMATION
			ColorB triangleColor = ((drawFlags & DrawAccessibility) && (connectivity.trianglesAccessible != NULL) && !connectivity.trianglesAccessible[i]) ? triangleColorDisconnected : triangleColorConnected;
#else
			ColorB triangleColor = triangleColorConnected;
#endif

			if (!(drawFlags & DrawAccessibility))
			{
				triangleColor = colorSelector.GetAnnotationColor(triangle.areaAnnotation);
			}
			if (drawFlags & DrawIslandsId)
			{
				triangleColor = CalculateColorFromMultipleItems(triangle.islandID, static_cast<uint32>(islandAreas.size()));
			}

			if (triangleColor != currentColor)
			{
				if (triVertices.size())
				{
					renderAuxGeom->DrawTriangles(triVertices.data(), triVertices.size(), currentColor);
					triVertices.clear();
				}
				currentColor = triangleColor;
			}

			const Vec3 v0 = vertices[triangle.vertex[0]].GetVec3() + offset;
			const Vec3 v1 = vertices[triangle.vertex[1]].GetVec3() + offset;
			const Vec3 v2 = vertices[triangle.vertex[2]].GetVec3() + offset;

			triVertices.push_back(v0);
			triVertices.push_back(v1);
			triVertices.push_back(v2);

			if (drawFlags & DrawTriangleBackfaces)
			{
				backfaceTriVertices.push_back(v1);
				backfaceTriVertices.push_back(v0);
				backfaceTriVertices.push_back(v2);
			}

			// Islands
			bool drawIslandData = ((drawFlags & DrawIslandsId) && triangle.islandID > MNM::Constants::eStaticIsland_InvalidIslandID && triangle.islandID < islandAreas.size());
			if ((drawFlags & DrawTrianglesId) || drawIslandData)
			{
				const Vec3 triCenter = ((v0 + v1 + v2) / 3.0f) + Vec3(.0f, .0f, .1f);

				stack_string text;
				if ((drawFlags & DrawTrianglesId) && drawIslandData)
				{
					text.Format("id: %d, area: %f, islandID: %u", ComputeTriangleID(tileID, static_cast<uint16>(i)), static_cast<uint16>(islandAreas[triangle.islandID - 1]), triangle.islandID);
				}
				else if (drawFlags & DrawTrianglesId)
				{
					text.Format("id: %d", ComputeTriangleID(tileID, static_cast<uint16>(i)));
				}
				else
				{
					text.Format("area: %f, islandID: %u", islandAreas[triangle.islandID - 1], triangle.islandID);
				}

				CDebugDrawContext dc;
				dc->Draw3dLabelEx(triCenter, 1.2f, ColorB(255, 255, 255), true, true, true, false, "%s", text.c_str());
			}
		}

		if (triVertices.size())
		{
			renderAuxGeom->DrawTriangles(triVertices.data(), triVertices.size(), currentColor);
		}
		if (backfaceTriVertices.size())
		{
			renderAuxGeom->DrawTriangles(backfaceTriVertices.data(), backfaceTriVertices.size(), triangleColorBackface);
		}
	}

	renderAuxGeom->SetRenderFlags(oldFlags);

	{
		std::vector<Vec3> internalLinkLines;
		std::vector<Vec3> externalLinkLines;
		std::vector<Vec3> offmeshLinkLines;
		std::vector<Vec3> boundaryLines;
		if (drawFlags & DrawInternalLinks)
		{
			internalLinkLines.reserve(6 * expectedMaxTriCount);
		}
		if (drawFlags & DrawExternalLinks)
		{
			externalLinkLines.reserve(6 * expectedMaxTriCount);
		}
		if (drawFlags & DrawOffMeshLinks)
		{
			offmeshLinkLines.reserve(6 * expectedMaxOffmeshTriCount);
		}
		if (drawFlags & DrawMeshBoundaries)
		{
			boundaryLines.reserve(2 * expectedMaxTriCount);
		}

		for (size_t i = 0; i < triangleCount; ++i)
		{
			const Tile::STriangle& triangle = triangles[i];
			
			Vec3 triVertices[3];
			triVertices[0] = vertices[triangle.vertex[0]].GetVec3();
			triVertices[1] = vertices[triangle.vertex[1]].GetVec3();
			triVertices[2] = vertices[triangle.vertex[2]].GetVec3();
			
			size_t linkedEdges = 0;

			for (size_t l = 0; l < triangle.linkCount; ++l)
			{
				const Tile::SLink& link = links[triangle.firstLink + l];
				const size_t edge = link.edge;
				linkedEdges |= static_cast<size_t>((size_t)1 << edge);

				const uint16 vi0 = link.edge;
				const uint16 vi1 = (link.edge + 1) % 3;

				assert(vi0 < 3);
				assert(vi1 < 3);

				const Vec3 v0 = triVertices[vi0] + loffset;
				const Vec3 v1 = triVertices[vi1] + loffset;

				if (link.side == Tile::SLink::OffMesh)
				{
					if (drawFlags & DrawOffMeshLinks)
					{
						const Vec3 a = triVertices[0] + offset;
						const Vec3 b = triVertices[1] + offset;
						const Vec3 c = triVertices[2] + offset;

						offmeshLinkLines.push_back(a);
						offmeshLinkLines.push_back(b);
						offmeshLinkLines.push_back(b);
						offmeshLinkLines.push_back(c);
						offmeshLinkLines.push_back(c);
						offmeshLinkLines.push_back(a);
					}
				}
				else if (link.side != Tile::SLink::Internal)
				{
					if (drawFlags & DrawExternalLinks)
					{
						// TODO: compute clipped edge
						externalLinkLines.push_back(v0);
						externalLinkLines.push_back(v1);
					}
				}
				else
				{
					if (drawFlags & DrawInternalLinks)
					{
						internalLinkLines.push_back(v0);
						internalLinkLines.push_back(v1);
					}
				}
			}

			if (drawFlags & DrawMeshBoundaries)
			{
				for (size_t e = 0; e < 3; ++e)
				{
					if ((linkedEdges & static_cast<size_t>((size_t)1 << e)) == 0)
					{
						boundaryLines.push_back(triVertices[e] + loffset);
						boundaryLines.push_back(triVertices[(e + 1) % 3] + loffset);
					}
				}
			}
		}

		if (internalLinkLines.size())
		{
			renderAuxGeom->DrawLines(internalLinkLines.data(), internalLinkLines.size(), Col_White);
		}
		if (externalLinkLines.size())
		{
			renderAuxGeom->DrawLines(externalLinkLines.data(), externalLinkLines.size(), Col_White, 4.0f);
		}
		if (offmeshLinkLines.size())
		{
			renderAuxGeom->DrawLines(offmeshLinkLines.data(), offmeshLinkLines.size(), Col_Red, 8.0f);
		}
		if (boundaryLines.size())
		{
			renderAuxGeom->DrawLines(boundaryLines.data(), boundaryLines.size(), Col_Black, 8.0f);
		}
	}
}

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
void STile::ValidateTriangleLinks()
{
	uint16 nextLink = 0;

	for (uint16 i = 0; i < triangleCount; ++i)
	{
		const Tile::STriangle& triangle = triangles[i];

		CRY_ASSERT_MESSAGE(triangle.firstLink <= linkCount || linkCount == 0, "Out of range link");

		CRY_ASSERT_MESSAGE(nextLink == triangle.firstLink, "Links are not contiguous");

		nextLink += triangle.linkCount;

		for (uint16 l = 0; l < triangle.linkCount; ++l)
		{
			uint16 linkIdx = triangle.firstLink + l;

			CRY_ASSERT_MESSAGE(links[linkIdx].side != Tile::SLink::OffMesh || l == 0, "Off mesh links should always be first");
		}
	}

	CRY_ASSERT(nextLink == linkCount);
}

void STile::ValidateTriangles() const
{
	const Tile::VertexIndex verticesMaxIndex = vertexCount;
	for (size_t triangleIdx = 0; triangleIdx < triangleCount; ++triangleIdx)
	{
		const Tile::STriangle& tri = triangles[triangleIdx];
		for (int i = 0; i < 3; ++i)
		{
			if (tri.vertex[i] >= verticesMaxIndex)
			{
				CRY_ASSERT_MESSAGE(tri.vertex[i] < verticesMaxIndex, "MNM traingle invalid vertex index");
			}
		}
	}
}

#endif

vector3_t::value_type STile::GetTriangleArea(const TriangleID triangleID) const
{
	const Tile::STriangle& triangle = triangles[ComputeTriangleIndex(triangleID)];
	return GetTriangleArea(triangle);
}

vector3_t::value_type STile::GetTriangleArea(const Tile::STriangle& triangle) const
{
	const vector3_t v0 = vector3_t(vertices[triangle.vertex[0]]);
	const vector3_t v1 = vector3_t(vertices[triangle.vertex[1]]);
	const vector3_t v2 = vector3_t(vertices[triangle.vertex[2]]);

	const vector3_t::value_type len0 = (v0 - v1).len();
	const vector3_t::value_type len1 = (v0 - v2).len();
	const vector3_t::value_type len2 = (v1 - v2).len();

	const vector3_t::value_type s = (len0 + len1 + len2) / vector3_t::value_type(2);

	return sqrtf(s * (s - len0) * (s - len1) * (s - len2));
}

//////////////////////////////////////////////////////////////////////////

#if MNM_USE_EXPORT_INFORMATION

bool STile::ConsiderExportInformation() const
{
	// TODO FrancescoR: Remove if it's not necessary anymore or refactor it.
	return true;
}

void STile::InitConnectivity(uint16 oldTriangleCount, uint16 newTriangleCount)
{
	if (ConsiderExportInformation())
	{
		// By default all is accessible
		connectivity.tileAccessible = 1;
		if (oldTriangleCount != newTriangleCount)
		{
			SAFE_DELETE_ARRAY(connectivity.trianglesAccessible);

			if (newTriangleCount)
			{
				connectivity.trianglesAccessible = new uint8[newTriangleCount];
			}
		}

		if (newTriangleCount)
		{
			memset(connectivity.trianglesAccessible, 1, sizeof(uint8) * newTriangleCount);
		}
		connectivity.triangleCount = newTriangleCount;
	}
}

void STile::ResetConnectivity(uint8 accessible)
{
	if (ConsiderExportInformation())
	{
		assert(connectivity.triangleCount == triangleCount);

		connectivity.tileAccessible = accessible;

		if (connectivity.trianglesAccessible != NULL)
		{
			memset(connectivity.trianglesAccessible, accessible, sizeof(uint8) * connectivity.triangleCount);
		}
	}
}

#endif
}
