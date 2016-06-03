// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Tile.h"
#include <CryCore/TypeInfo_impl.h>
#include "DebugDrawContext.h"

STRUCT_INFO_BEGIN(MNM::Tile::Triangle)
STRUCT_VAR_INFO(vertex, TYPE_ARRAY(3, TYPE_INFO(Index)))
STRUCT_BITFIELD_INFO(linkCount, TYPE_INFO(uint16), 4)
STRUCT_BITFIELD_INFO(firstLink, TYPE_INFO(uint16), 12)
STRUCT_VAR_INFO(islandID, TYPE_INFO(StaticIslandID))
STRUCT_INFO_END(MNM::Tile::Triangle)

STRUCT_INFO_BEGIN(MNM::Tile::BVNode)
STRUCT_BITFIELD_INFO(leaf, TYPE_INFO(uint16), 1)
STRUCT_BITFIELD_INFO(offset, TYPE_INFO(uint16), 15)
STRUCT_VAR_INFO(aabb, TYPE_INFO(AABB))
STRUCT_INFO_END(MNM::Tile::BVNode)

STRUCT_INFO_BEGIN(MNM::Tile::Link)
STRUCT_BITFIELD_INFO(side, TYPE_INFO(uint32), 4)
STRUCT_BITFIELD_INFO(edge, TYPE_INFO(uint32), 2)
STRUCT_BITFIELD_INFO(triangle, TYPE_INFO(uint32), 10)
STRUCT_INFO_END(MNM::Tile::Link)

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
	#define TILE_VALIDATE() ValidateTriangleLinks()
#else
	#define TILE_VALIDATE()
#endif

namespace MNM
{
Tile::Tile()
	: triangles(0)
	, vertices(0)
	, nodes(0)
	, links(0)
	, triangleCount(0)
	, vertexCount(0)
	, nodeCount(0)
	, linkCount(0)
	, hashValue(0)
{

}

void Tile::CopyTriangles(Triangle* _triangles, uint16 count)
{

#if MNM_USE_EXPORT_INFORMATION
	InitConnectivity(triangleCount, count);
#endif

	if (triangleCount != count)
	{
		delete[] triangles;
		triangles = 0;

		triangleCount = count;

		if (count)
			triangles = new Triangle[count];
	}

	if (count)
		memcpy(triangles, _triangles, sizeof(Triangle) * count);
}

void Tile::CopyVertices(Vertex* _vertices, uint16 count)
{
	if (vertexCount != count)
	{
		delete[] vertices;
		vertices = 0;

		vertexCount = count;

		if (count)
			vertices = new Vertex[count];
	}

	if (count)
		memcpy(vertices, _vertices, sizeof(Vertex) * count);

	TILE_VALIDATE();
}

void Tile::CopyNodes(BVNode* _nodes, uint16 count)
{
	if (nodeCount != count)
	{
		delete[] nodes;
		nodes = 0;

		nodeCount = count;

		if (count)
			nodes = new BVNode[count];
	}

	if (count)
		memcpy(nodes, _nodes, sizeof(BVNode) * count);
}

void Tile::CopyLinks(Link* _links, uint16 count)
{
	if (linkCount != count)
	{
		delete[] links;
		links = 0;

		linkCount = count;

		if (count)
			links = new Link[count];
	}

	if (count)
		memcpy(links, _links, sizeof(Link) * count);
}

void Tile::AddOffMeshLink(const TriangleID triangleID, const uint16 offMeshIndex)
{
	TILE_VALIDATE();

	uint16 triangleIdx = ComputeTriangleIndex(triangleID);
	assert(triangleIdx < triangleCount);
	if (triangleIdx < triangleCount)
	{
		//Figure out if this triangle has already off-mesh connections
		//Off-mesh link is always the first if exists
		Triangle& triangle = triangles[triangleIdx];

		const size_t MaxLinkCount = 1024 * 6;
		Tile::Link tempLinks[MaxLinkCount];

		bool hasOffMeshLink = links && (triangle.linkCount > 0) && (triangle.firstLink < linkCount) && (links[triangle.firstLink].side == Link::OffMesh);

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
					memcpy(tempLinks, links, sizeof(Link) * triangle.firstLink);
				}

				tempLinks[triangle.firstLink].side = Link::OffMesh;
				tempLinks[triangle.firstLink].triangle = offMeshIndex;

				//Note this is not used
				tempLinks[triangle.firstLink].edge = 0;

				const int countDiff = linkCount - triangle.firstLink;
				if (countDiff > 0)
				{
					assert(links);
					memcpy(&tempLinks[triangle.firstLink + 1], &links[triangle.firstLink], sizeof(Link) * countDiff);
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

void Tile::UpdateOffMeshLink(const TriangleID triangleID, const uint16 offMeshIndex)
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
			Link& link = links[linkIdx];
			assert(link.side == Link::OffMesh);
			if (link.side == Link::OffMesh)
			{
				link.triangle = offMeshIndex;
			}
		}
	}

	TILE_VALIDATE();
}

void Tile::RemoveOffMeshLink(const TriangleID triangleID)
{
	TILE_VALIDATE();

	// Find link to be removed
	uint16 linkToRemoveIdx = 0xFFFF;
	uint16 boundTriangleIdx = ComputeTriangleIndex(triangleID);
	if (boundTriangleIdx < triangleCount)
	{
		const uint16 firstLink = triangles[boundTriangleIdx].firstLink;
		if ((triangles[boundTriangleIdx].linkCount > 0) && (firstLink < linkCount) && (links[firstLink].side == Link::OffMesh))
			linkToRemoveIdx = firstLink;
	}

	// Try enabling DEBUG_MNM_DATA_CONSISTENCY_ENABLED if you get this
	CRY_ASSERT_MESSAGE(linkToRemoveIdx != 0xFFFF, "Trying to remove off mesh link that doesn't exist");

	if (linkToRemoveIdx != 0xFFFF)
	{
		assert(linkCount > 1);

		const size_t MaxLinkCount = 1024 * 6;
		Tile::Link tempLinks[MaxLinkCount];

		if (linkToRemoveIdx)
			memcpy(tempLinks, links, sizeof(Link) * linkToRemoveIdx);

		const int diffCount = linkCount - (linkToRemoveIdx + 1);
		if (diffCount > 0)
		{
			memcpy(&tempLinks[linkToRemoveIdx], &links[linkToRemoveIdx + 1], sizeof(Link) * diffCount);
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

void Tile::Swap(Tile& other)
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

void Tile::Destroy()
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

ColorF CalculateColorForIsland(StaticIslandID islandID, uint16 totalIslands)
{
	if (totalIslands == 0)
		return Col_White;

	const float hueValue = islandID / (float) totalIslands;

	ColorF color;
	color.fromHSV(hueValue, 1.0f, 1.0f);
	color.a = 1.0f;
	return color;
}

void Tile::Draw(size_t drawFlags, vector3_t origin, TileID tileID, const std::vector<float>& islandAreas) const
{
	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	const ColorB triangleColorConnected(Col_Azure, 0.65f);
	const ColorB triangleColorDisconnected(Col_Red, 0.65f);
	const ColorB boundaryColor(Col_Black);

	const Vec3 offset = origin.GetVec3() + Vec3(0.0f, 0.0f, 0.05f);
	const Vec3 loffset(offset + Vec3(0.0f, 0.0f, 0.0005f));

	SAuxGeomRenderFlags oldFlags = renderAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(oldFlags);

	renderFlags.SetAlphaBlendMode(e_AlphaBlended);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);

	renderAuxGeom->SetRenderFlags(renderFlags);

	if (drawFlags & DrawTriangles)
	{
		for (size_t i = 0; i < triangleCount; ++i)
		{
			const Triangle& triangle = triangles[i];

			const Vec3 v0 = vertices[triangle.vertex[0]].GetVec3() + offset;
			const Vec3 v1 = vertices[triangle.vertex[1]].GetVec3() + offset;
			const Vec3 v2 = vertices[triangle.vertex[2]].GetVec3() + offset;

#if MNM_USE_EXPORT_INFORMATION
			ColorB triangleColor = ((drawFlags & DrawAccessibility) && (connectivity.trianglesAccessible != NULL) && !connectivity.trianglesAccessible[i]) ? triangleColorDisconnected : triangleColorConnected;
#else
			ColorB triangleColor = triangleColorConnected;
#endif

			// Islands
			bool drawIslandData = ((drawFlags & DrawIslandsId) && triangle.islandID > MNM::Constants::eStaticIsland_InvalidIslandID && triangle.islandID < islandAreas.size());
			if (drawFlags & DrawIslandsId)
			{
				triangleColor = CalculateColorForIsland(triangle.islandID, static_cast<uint16>(islandAreas.size()));
			}
			renderAuxGeom->DrawTriangle(v0, triangleColor, v1, triangleColor, v2, triangleColor);

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
	}

	renderAuxGeom->SetRenderFlags(oldFlags);

	for (size_t i = 0; i < triangleCount; ++i)
	{
		const Triangle& triangle = triangles[i];
		size_t linkedEdges = 0;

		for (size_t l = 0; l < triangle.linkCount; ++l)
		{
			const Link& link = links[triangle.firstLink + l];
			const size_t edge = link.edge;
			linkedEdges |= static_cast<size_t>((size_t)1 << edge);

			const uint16 vi0 = link.edge;
			const uint16 vi1 = (link.edge + 1) % 3;

			assert(vi0 < 3);
			assert(vi1 < 3);

			const Vec3 v0 = vertices[triangle.vertex[vi0]].GetVec3() + loffset;
			const Vec3 v1 = vertices[triangle.vertex[vi1]].GetVec3() + loffset;

			if (link.side == Link::OffMesh)
			{
				if (drawFlags & DrawOffMeshLinks)
				{
					const Vec3 a = vertices[triangle.vertex[0]].GetVec3() + offset;
					const Vec3 b = vertices[triangle.vertex[1]].GetVec3() + offset;
					const Vec3 c = vertices[triangle.vertex[2]].GetVec3() + offset;

					renderAuxGeom->DrawLine(a, Col_Red, b, Col_Red, 8.0f);
					renderAuxGeom->DrawLine(b, Col_Red, c, Col_Red, 8.0f);
					renderAuxGeom->DrawLine(c, Col_Red, a, Col_Red, 8.0f);
				}
			}
			else if (link.side != Link::Internal)
			{
				if (drawFlags & DrawExternalLinks)
				{
					// TODO: compute clipped edge
					renderAuxGeom->DrawLine(v0, Col_Green, v1, Col_ForestGreen);
				}
			}
			else
			{
				if (drawFlags & DrawInternalLinks)
					renderAuxGeom->DrawLine(v0, Col_White, v1, Col_White);
			}
		}

		if (drawFlags & DrawMeshBoundaries)
		{
			for (size_t e = 0; e < 3; ++e)
			{
				if ((linkedEdges & static_cast<size_t>((size_t)1 << e)) == 0)
				{
					const Vec3 a = vertices[triangle.vertex[e]].GetVec3() + loffset;
					const Vec3 b = vertices[triangle.vertex[(e + 1) % 3]].GetVec3() + loffset;

					renderAuxGeom->DrawLine(a, Col_Black, b, Col_Black, 8.0f);
				}
			}
		}
	}
}

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
void Tile::ValidateTriangleLinks()
{
	uint16 nextLink = 0;

	for (uint16 i = 0; i < triangleCount; ++i)
	{
		const Triangle& triangle = triangles[i];

		CRY_ASSERT_MESSAGE(triangle.firstLink <= linkCount || linkCount == 0, "Out of range link");

		CRY_ASSERT_MESSAGE(nextLink == triangle.firstLink, "Links are not contiguous");

		nextLink += triangle.linkCount;

		for (uint16 l = 0; l < triangle.linkCount; ++l)
		{
			uint16 linkIdx = triangle.firstLink + l;

			CRY_ASSERT_MESSAGE(links[linkIdx].side != Link::OffMesh || l == 0, "Off mesh links should always be first");
		}
	}

	CRY_ASSERT(nextLink == linkCount);
}
#endif

vector3_t::value_type Tile::GetTriangleArea(const TriangleID triangleID) const
{
	const Tile::Triangle& triangle = triangles[ComputeTriangleIndex(triangleID)];

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

bool Tile::ConsiderExportInformation() const
{
	// TODO FrancescoR: Remove if it's not necessary anymore or refactor it.
	return true;
}

void Tile::InitConnectivity(uint16 oldTriangleCount, uint16 newTriangleCount)
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

void Tile::ResetConnectivity(uint8 accessible)
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
