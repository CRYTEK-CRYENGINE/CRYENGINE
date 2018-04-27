// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FbxScene.h"
#include <CryMath/Cry_Geo.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CrySystem/ITimer.h>
#include <unordered_map>

// The terminology in the FBX documentation is different from the one used in games. The main concepts
// are related as follows (FBX on left hand side):
// Control point = vertex position
// Polygon vertex = index
// Layer element = vertex attribute.

namespace
{

enum EElement
{
	eElement_Material,
	eElement_Position,
	eElement_Normal,
	eElement_UV,
	eElement_COUNT
};

// Overview of algorithm:
//
// The FBX mesh format (FbxMesh) supports multiple mapping modes and addressing modes for layer elements
// (vertex attributes). The engine mesh format (CMesh), on the other hand, is less flexible and stores an
// attribute value for every vertex (in FBX speak: mapping mode eByControlPoint for all layers).
// The purpose of CreateEngineMesh() is to create a CMesh from an FbxMesh.
//
// First, we create a vertex for each index and gather all attributes mapped to it.
// Essentially, this converts an indexed mesh to a non-indexed mesh, since triples of vertices
// describe faces.
// Then, we sort vertices and try to remove duplicates, thereby creating an indexed mesh again.

struct SFatIndex
{
	int elements[eElement_COUNT];

	SFatIndex()
	{
		for (int i = 0; i < eElement_COUNT; ++i)
		{
			elements[i] = -1;
		}
	}
};

struct SVertex
{
	int           material;
	int           uberMaterial;
	Vec3          position;
	Vec3          normal;
	SMeshTexCoord texCoords;
};

// Relative epsilon comparison.
// see: https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
bool AlmostEqual(float lhp, float rhp)
{
	// Calculate the difference.
	const float diff = fabs(lhp - rhp);
	lhp = fabs(lhp);
	rhp = fabs(rhp);
	// Find the largest
	float largest = (rhp > lhp) ? rhp : lhp;

	return (diff <= largest * FLT_EPSILON);
}

// Note: Vec3::IsEquivalent uses absolute epsilon comparison, with relatively large epsilon.
bool AlmostEqual(const Vec3& lhp, const Vec3& rhp)
{
	return
	  AlmostEqual(lhp.x, rhp.x) &&
	  AlmostEqual(lhp.y, rhp.y) &&
	  AlmostEqual(lhp.z, rhp.z);
}

bool AlmostEqual(const SMeshTexCoord& lhp, const SMeshTexCoord& rhp)
{
	const Vec2 vl = lhp.GetUV();
	const Vec2 vr = rhp.GetUV();
	return AlmostEqual(vl.x, vr.x) && AlmostEqual(vl.y, vr.y);
}

bool AlmostEqual(const SVertex& lhp, const SVertex& rhp)
{
	return
	  lhp.material == rhp.material &&
	  AlmostEqual(lhp.position, rhp.position) &&
	  AlmostEqual(lhp.normal, rhp.normal) &&
	  AlmostEqual(lhp.texCoords, rhp.texCoords);
}

void SetElementIndices(
  std::vector<SFatIndex>& fatIndices,
  int element, // in EElement
  const FbxLayerElement* pLayerElement,
  const FbxLayerElementArrayTemplate<int>& indexArray,
  const FbxMesh& mesh)
{
	const size_t nIndices = fatIndices.size();

	if (pLayerElement->GetMappingMode() == FbxLayerElement::eByControlPoint)
	{
		if (pLayerElement->GetReferenceMode() == FbxLayerElement::eDirect)
		{
			for (int i = 0; i < nIndices; ++i)
			{
				fatIndices[i].elements[element] = fatIndices[i].elements[eElement_Position];
			}
		}
		else if (pLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
		{
			for (int i = 0; i < nIndices; ++i)
			{
				fatIndices[i].elements[element] = indexArray.GetAt(fatIndices[i].elements[eElement_Position]);
			}
		}
	}
	else if (pLayerElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
	{
		if (pLayerElement->GetReferenceMode() == FbxLayerElement::eDirect)
		{
			for (int i = 0; i < nIndices; ++i)
			{
				fatIndices[i].elements[element] = i;
			}
		}
		else if (pLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
		{
			for (int i = 0; i < nIndices; ++i)
			{
				fatIndices[i].elements[element] = indexArray.GetAt(i);
			}
		}
	}
	else if (pLayerElement->GetMappingMode() == FbxLayerElement::eAllSame)
	{
		if (pLayerElement->GetReferenceMode() == FbxLayerElement::eDirect)
		{
			for (int i = 0; i < nIndices; ++i)
			{
				fatIndices[i].elements[element] = 0;
			}
		}
		else if (pLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
		{
			for (int i = 0; i < nIndices; ++i)
			{
				fatIndices[i].elements[element] = indexArray.GetAt(0);
			}
		}
	}
	else if (pLayerElement->GetMappingMode() == FbxLayerElement::eByPolygon)
	{
		const int nFaces = mesh.GetPolygonCount();

		if (pLayerElement->GetReferenceMode() == FbxLayerElement::eDirect)
		{
			int cursor = 0;
			for (int i = 0; i < nFaces; ++i)
			{
				for (int j = 0; j < mesh.GetPolygonSize(i); ++j)
				{
					fatIndices[cursor++].elements[element] = i;
				}
			}
		}
		else if (pLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
		{
			int cursor = 0;
			for (int i = 0; i < nFaces; ++i)
			{
				for (int j = 0; j < mesh.GetPolygonSize(i); ++j)
				{
					fatIndices[cursor++].elements[element] = indexArray.GetAt(i);
				}
			}
		}
	}
	else
	{
		// Ignoring mapping mode eByEdge for now, since no assets use it anyway.
		assert(0);
	}
}

int Sign(float x)
{
	if (x > 0.0f)
	{
		return 1;
	}
	else if (x < 0.0f)
	{
		return -1;
	}
	return 0;
}

int CompareLex(const Vec3& lhp, const Vec3& rhp)
{
	if (lhp.x != rhp.x) return Sign(lhp.x - rhp.x);
	if (lhp.y != rhp.y) return Sign(lhp.y - rhp.y);
	if (lhp.z != rhp.z) return Sign(lhp.z - rhp.z);
	return 0;
}

int CompareLex(const SMeshTexCoord& lhp, const SMeshTexCoord& rhp)
{
	if (lhp == rhp)
	{
		return 0;
	}
	else if (lhp < rhp)
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

struct CompareVertices
{
	const SVertex* const vertices;

	explicit CompareVertices(const SVertex* vertices)
		: vertices(vertices)
	{}

	// Sort elements lexicographically.
	bool operator()(int lhp, int rhp)
	{
		int sign = 0;

		sign = vertices[lhp].material - vertices[rhp].material;
		if (sign) return sign < 0;

		// Lexicographic sorting is somewhat expensive, so we don't do it and,
		// as a consequence, generate sub-optimal meshes (with regard to number of vertices).
#if 0
		sign = CompareLex(vertices[lhp].position, vertices[rhp].position);
		if (sign) return sign < 0;

		sign = CompareLex(vertices[lhp].normal, vertices[rhp].normal);
		if (sign) return sign < 0;

		sign = CompareLex(vertices[lhp].texCoords, vertices[rhp].texCoords);
		if (sign) return sign < 0;
#endif

		// If vertices are lexicographically equal, we sort by indices.
		return lhp < rhp;
	}
};

struct CompareUberMaterial
{
	bool operator()(const SVertex& lhp, const SVertex& rhp)
	{
		return lhp.uberMaterial < rhp.uberMaterial;
	}
};

struct CompareTriangles
{
	const SVertex* const pVertices;
	const int* const     pIndices;

	CompareTriangles(const SVertex* pVertices, const int* pIndices)
		: pVertices(pVertices)
		, pIndices(pIndices)
	{}

	bool operator()(int lhp, int rhp)
	{
		return pVertices[pIndices[3 * lhp]].material < pVertices[pIndices[3 * rhp]].material;
	}
};

Vec3 ToVec3(const FbxVector4& v)
{
	return Vec3(v[0], v[1], v[2]);
}

SMeshTexCoord ToTexCoords(const FbxVector2& v)
{
	return SMeshTexCoord(v[0], v[1]);
}

} // namespace

void LogPrintf(const char* szFormat, ...);

// Creates triangle indices. Assumes that input faces are convex polygons. Faces with more than
// three vertices are replaced by triangle fans.
void Triangulate(const FbxMesh& inputMesh, const std::vector<SFatIndex>& in, std::vector<SFatIndex>& out)
{
	const int nFaces = inputMesh.GetPolygonCount();
	int t0 = 0;
	for (int i = 0; i < nFaces; ++i)
	{
		const int nFaceSize = inputMesh.GetPolygonSize(i);
		int t1 = t0 + 1;
		for (int j = 2; j < nFaceSize; ++j)
		{
			out.push_back(in[t0]);
			out.push_back(in[t1]);
			out.push_back(in[t1 + 1]);
			t1++;
		}
		t0 += nFaceSize;
	}
}

// Converts the input FBX to a list of vertices, where triples of vertices describe triangles
// (triangles have no vertices in common, so primitive type is triangle list).
void CreateDirectMesh(const FbxMesh& inputMesh, std::vector<SVertex>& vertices)
{
	assert(vertices.empty());

	const int nIndices = inputMesh.GetPolygonVertexCount();
	const int nFaces = inputMesh.GetPolygonCount();

	// We ignore every layer but the first one.
	const FbxLayer* const pLayer0 = inputMesh.GetLayer(0);

	std::vector<SFatIndex> fatIndices(nIndices);

	// Set fat indices.
	{
		int cursor = 0;
		for (int i = 0; i < nFaces; ++i)
		{
			for (int j = 0; j < inputMesh.GetPolygonSize(i); ++j)
			{
				fatIndices[cursor].elements[eElement_Position] = inputMesh.GetPolygonVertex(i, j);
				cursor++;
			}
		}
	}

	// Set normals.
	const FbxLayerElementNormal* const pInputNormals = pLayer0 ? pLayer0->GetNormals() : nullptr;
	const bool bHasNormals = pInputNormals != nullptr;
	if (bHasNormals)
	{
		SetElementIndices(fatIndices, eElement_Normal, pInputNormals, pInputNormals->GetIndexArray(), inputMesh);
	}
	else
	{
		LogPrintf("Mesh is missing normals. Setting normals to zero-vector.\n");
	}

	// Set UV coordinates. Ignore all but first one.
	const bool bHasTexCoords = pLayer0 && pLayer0->GetUVSetCount() > 0;
	if (bHasTexCoords)
	{
		const FbxLayerElementUV* const pInputTexCoords = pLayer0->GetUVSets().GetFirst();
		SetElementIndices(fatIndices, eElement_UV, pInputTexCoords, pInputTexCoords->GetIndexArray(), inputMesh);
	}

	// Set materials.
	const bool bHasMaterials = pLayer0 && pLayer0->GetMaterials() != nullptr;
	if (bHasMaterials)
	{
		const FbxLayerElementMaterial* const pInputMaterials = pLayer0->GetMaterials();
		SetElementIndices(fatIndices, eElement_Material, pInputMaterials, pInputMaterials->GetIndexArray(), inputMesh);

		// The index array of the material layer has special meaning, and the direct array is no longer used.
		// See http://help.autodesk.com/view/FBX/2016/ENU/?guid=__cpp_ref_class_fbx_layer_element_material_html
		assert(pInputMaterials->GetReferenceMode() == FbxLayerElement::eIndexToDirect);
	}
	else
	{
		LogPrintf("Mesh has no material layer, faces will be assigned dummy material.\n");
	}

	std::vector<SFatIndex> tris;
	Triangulate(inputMesh, fatIndices, tris);
	fatIndices.swap(tris);

	const int nTriIndices = fatIndices.size();

	// Create vertices.
	vertices.resize(nTriIndices);
	for (int i = 0; i < nTriIndices; ++i)
	{
		if (bHasMaterials)
		{
			vertices[i].material = fatIndices[i].elements[eElement_Material]; // No direct array. See remarks about materials above.
		}
		else
		{
			vertices[i].material = -1; // Assign dummy material.
		}

		vertices[i].position = ToVec3(inputMesh.GetControlPointAt(fatIndices[i].elements[eElement_Position]));

		if (bHasNormals)
		{
			vertices[i].normal = ToVec3(pInputNormals->GetDirectArray().GetAt(fatIndices[i].elements[eElement_Normal]));
		}
		else
		{
			vertices[i].normal = Vec3(ZERO);
		}

		if (bHasTexCoords)
		{
			const FbxLayerElementUV* const pInputTexCoords = pLayer0->GetUVSets().GetFirst();
			vertices[i].texCoords = ToTexCoords(pInputTexCoords->GetDirectArray().GetAt(fatIndices[i].elements[eElement_UV]));
		}
	}

	// If the mesh lacks uv coordinates, we assign arbitary values, so that IIndexedMesh::Optimize() does not complain.
	if (!bHasTexCoords)
	{
		for (int i = 0; i < nTriIndices; i += 3)
		{
			vertices[i + 0].texCoords = SMeshTexCoord(0.0f, 0.0f);
			vertices[i + 1].texCoords = SMeshTexCoord(1.0f, 0.0f);
			vertices[i + 2].texCoords = SMeshTexCoord(1.0f, 1.0f);
		}
	}

	// If the vertices of a single triangle have different material indices, we always use the index
	// of the first vertex.
	for (int i = 0; i < nTriIndices; i += 3)
	{
		vertices[i + 2].material = vertices[i + 1].material = vertices[i].material;
	}
}

// Translate material indices.
// The material IDs stored in the engine mesh are relative to pNode, so we replace them
// with the globally unique ID of a material.
// Out-of-range indices are mapped to dummy material (id 0).
// Also, we assign the uber-material index.
void TranslateMaterialIndices(
  std::vector<SVertex>& vertices,
  const FbxNode* pNode,
  const std::vector<std::unique_ptr<FbxTool::SMaterial>>& materials,
  SCreateEngineMeshStats* pStats)
{
	const int nMaterials = pNode->GetMaterialCount();

	// Maps old indices to new (translated) indices, such that map[old] == new.
	std::vector<int> map(pNode->GetMaterialCount(), -1);

	for (size_t i = 0; i < vertices.size(); ++i)
	{
		int submaterial = 0;
		if (vertices[i].material >= 0 && vertices[i].material < nMaterials)
		{
			if (map[vertices[i].material] < 0)
			{
				const FbxSurfaceMaterial* const pMaterial = pNode->GetMaterial(vertices[i].material);
				if (pMaterial)
				{
					int j = 0;
					while (j < materials.size())
					{
						if (!stricmp(pMaterial->GetName(), materials[j]->szName))
						{
							break;
						}
						j++;
					}
					assert(j < materials.size());
					map[vertices[i].material] = j;
					if (pStats)
					{
						pStats->translateMaterialIndices.nSearches++;
					}
				}
				else
				{
					map[vertices[i].material] = 0;
				}
			}
			else if (pStats)
			{
				pStats->translateMaterialIndices.nLookups++;
			}
			submaterial = map[vertices[i].material];
		}
		else
		{
			/*
			   LogPrintf("While translating material indices, index %d is out of range (node has %d materials attached to it). Vertex is assigned dummy material.\n",
			   vertices[i].material, pNode->GetMaterialCount());
			 */
		}

		vertices[i].material = submaterial % MAX_SUB_MATERIALS;
		vertices[i].uberMaterial = submaterial / MAX_SUB_MATERIALS;
	}
}

// Creates mesh from vertices in range [beginVertex, endVertex)
void CreateMesh(const SVertex* vertices, int nIndices, CMesh& outputMesh, SCreateEngineMeshStats* pStats)
{
	ITimer* const pTimer = gEnv->pSystem->GetITimer();
	float startTime, endTime;

	// Sort vertices.
	startTime = pTimer->GetAsyncCurTime();
	std::vector<int> sortedVertices(nIndices);
	for (int i = 0; i < nIndices; ++i)
	{
		sortedVertices[i] = i;
	}
	std::sort(sortedVertices.begin(), sortedVertices.end(), CompareVertices(vertices));
	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->createMesh.elapsedSortVertices += endTime - startTime;
	}

	std::vector<int> indices(nIndices, -1);
	std::vector<SVertex> uniqueVertices;
	uniqueVertices.reserve(nIndices);

	// Indices are inverse mapping of sorted vertices. Making unique vertices as we go.
	startTime = pTimer->GetAsyncCurTime();
	for (int i = 0; i < nIndices; ++i)
	{
		if (i > 0 && AlmostEqual(vertices[sortedVertices[i]], vertices[sortedVertices[i - 1]]))
		{
			assert(indices[sortedVertices[i - 1]] >= 0);
			indices[sortedVertices[i]] = indices[sortedVertices[i - 1]];
		}
		else
		{
			uniqueVertices.push_back(vertices[sortedVertices[i]]);
			indices[sortedVertices[i]] = uniqueVertices.size() - 1;
		}
	}
	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->createMesh.elapsedMakeUniqueVertices += endTime - startTime;
	}

	// Debug: In-range check of indices.
	for (int i = 0; i < nIndices; ++i)
	{
		assert(0 <= indices[i] && indices[i] < uniqueVertices.size());
	}

	// Debug: In-range check of material indices.
	for (int i = 0; i < nIndices; ++i)
	{
		assert(vertices[i].material >= 0 && vertices[i].material < MAX_SUB_MATERIALS);
	}

	// Sort triangles.

	startTime = pTimer->GetAsyncCurTime();

	std::vector<int> tris(nIndices / 3);
	for (size_t i = 0; i < tris.size(); ++i)
	{
		tris[i] = i;
	}
	std::sort(tris.begin(), tris.end(), CompareTriangles(&uniqueVertices[0], &indices[0]));

	std::vector<int> sortedIndices(nIndices);
	for (size_t i = 0; i < tris.size(); ++i)
	{
		sortedIndices[3 * i + 0] = indices[3 * tris[i] + 0];
		sortedIndices[3 * i + 1] = indices[3 * tris[i] + 1];
		sortedIndices[3 * i + 2] = indices[3 * tris[i] + 2];
	}

	indices.clear();

	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->createMesh.elapsedSortTriangles += endTime - startTime;
	}

	// Collect subsets.
	startTime = pTimer->GetAsyncCurTime();
	std::vector<std::pair<int, int>> subsetIndexRanges;
	int firstSubsetIndex = 0;
	for (int i = 1; i < nIndices; ++i)
	{
		if (uniqueVertices[sortedIndices[i]].material != uniqueVertices[sortedIndices[i - 1]].material)
		{
			subsetIndexRanges.push_back(std::make_pair(firstSubsetIndex, i - firstSubsetIndex));
			firstSubsetIndex = i;
		}
	}
	subsetIndexRanges.push_back(std::make_pair(firstSubsetIndex, nIndices - firstSubsetIndex));
	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->createMesh.elapsedCollectSubsets += endTime - startTime;
	}

	const int nSubsets = subsetIndexRanges.size();
	assert(nSubsets < MAX_SUB_MATERIALS);

	outputMesh.m_subsets.resize(nSubsets);

	// Maps material IDs to subsets (note that material IDs might be negative).
	// Invariant: m_subsets[map[n]].nMatID == n, for material ID n.
	std::unordered_map<int, int> map;

	startTime = pTimer->GetAsyncCurTime();
	for (size_t i = 0; i < nSubsets; ++i)
	{
		SMeshSubset& subset = outputMesh.m_subsets[i];
		memset(&subset, 0, sizeof(SMeshSubset));

		subset.nFirstIndexId = subsetIndexRanges[i].first;
		subset.nNumIndices = subsetIndexRanges[i].second;

		int min = INT_MAX;
		int max = 0;
		for (int j = 0; j < subset.nNumIndices; ++j)
		{
			int index = sortedIndices[subset.nFirstIndexId + j];
			min = std::min(index, min);
			max = std::max(index, max);
		}

		subset.nFirstVertId = min;
		subset.nNumVerts = (max - min) + 1;

		subset.nMatID = uniqueVertices[subset.nFirstVertId].material;

		map[subset.nMatID] = i;
	}
	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->createMesh.elapsedProcessSubsets += endTime - startTime;
	}

	// Create CMesh.

	startTime = pTimer->GetAsyncCurTime();

	// Create CMesh faces.
	const int nTriangles = nIndices / 3;
	outputMesh.SetFaceCount(nTriangles);
	SMeshFace* const pFaces = outputMesh.GetStreamPtr<SMeshFace>(CMesh::FACES);
	int cursor = 0;
	for (int i = 0; i < nIndices; i += 3)
	{
		SMeshFace face;
		face.v[0] = sortedIndices[i + 0];
		face.v[1] = sortedIndices[i + 1];
		face.v[2] = sortedIndices[i + 2];
		face.nSubset = map[uniqueVertices[sortedIndices[i]].material];

		pFaces[cursor] = face;
		cursor++;
	}

	// Create CMesh vertices.
	{
		const int nVertices = uniqueVertices.size();
		outputMesh.SetVertexCount(nVertices);
		outputMesh.SetTexCoordsCount(nVertices);

		Vec3* const pOutputPositions = outputMesh.GetStreamPtr<Vec3>(CMesh::POSITIONS);
		Vec3* const pOutputNormals = outputMesh.GetStreamPtr<Vec3>(CMesh::NORMALS);
		SMeshTexCoord* const pOutputTexCoords = outputMesh.GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);

		for (int i = 0; i < nVertices; ++i)
		{
			pOutputPositions[i] = uniqueVertices[i].position;
			pOutputNormals[i] = uniqueVertices[i].normal;
			pOutputTexCoords[i] = uniqueVertices[i].texCoords;
		}
	}

	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->createMesh.elapsedCreateCMesh += endTime - startTime;
	}
}

const char* CreateEngineMesh(
  const FbxMesh& inputMesh,
  std::vector<std::unique_ptr<FbxTool::SDisplayMesh>>& outputMeshes,
  const FbxNode* pNode,
  const std::vector<std::unique_ptr<FbxTool::SMaterial>>& materials,
  SCreateEngineMeshStats* pStats)
{
	if (inputMesh.GetControlPointsCount() == 0)
	{
		return "Mesh has no vertices";
	}

	if (inputMesh.GetPolygonCount() == 0)
	{
		return "Mesh has no polygons";
	}

	ITimer* const pTimer = gEnv->pSystem->GetITimer();
	const float totalStartTime = pTimer->GetAsyncCurTime();
	float startTime, endTime;

	std::vector<SVertex> vertices;

	startTime = pTimer->GetAsyncCurTime();
	CreateDirectMesh(inputMesh, vertices);
	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->elapsedCreateDirectMesh += endTime - startTime;
	}

	startTime = pTimer->GetAsyncCurTime();
	TranslateMaterialIndices(vertices, pNode, materials, pStats);
	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->elapsedTranslateMaterialIndices += endTime - startTime;
	}

	// Group vertices by uber-material.
	std::sort(vertices.begin(), vertices.end(), CompareUberMaterial());

	// All vertices of a sub-mesh share the same uber-material.
	std::vector<std::pair<int, int>> subMeshes;

	// Find sub-mesh ranges.
	int firstVertex = 0;
	for (size_t i = 1; i < vertices.size(); ++i)
	{
		if (vertices[i].uberMaterial != vertices[firstVertex].uberMaterial)
		{
			subMeshes.push_back(std::make_pair(firstVertex, (int)(i)));
			firstVertex = i;
		}
	}
	subMeshes.push_back(std::make_pair(firstVertex, (int)(vertices.size())));

	startTime = pTimer->GetAsyncCurTime();
	for (size_t i = 0; i < subMeshes.size(); ++i)
	{
		const int nIndices = subMeshes[i].second - subMeshes[i].first;
		std::unique_ptr<FbxTool::SDisplayMesh> pDisplayMesh(new FbxTool::SDisplayMesh());
		pDisplayMesh->pEngineMesh.reset(new CMesh());
		const SVertex* const pFirstVertex = &vertices[subMeshes[i].first];
		CreateMesh(pFirstVertex, nIndices, *pDisplayMesh->pEngineMesh, pStats);
		pDisplayMesh->uberMaterial = pFirstVertex->uberMaterial;
		outputMeshes.emplace_back(std::move(pDisplayMesh));
	}
	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->elapsedCreateMesh += endTime - startTime;
	}

	const float totalEndTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->elapsedTotal += totalEndTime - totalStartTime;
	}

	return nullptr;
}

void SCreateEngineMeshStats::Log(const SCreateEngineMeshStats& stats)
{
	LogPrintf("CreateEngineMesh statistics:\n");

	LogPrintf("\ttTotal: %f\n", stats.elapsedTotal);
	LogPrintf("\ttCreateDirectMesh: %f\n", stats.elapsedCreateDirectMesh);
	LogPrintf("\ttTranslateMaterialIndices: %f\n", stats.elapsedTranslateMaterialIndices);
	LogPrintf("\ttCreateMesh: %f\n", stats.elapsedCreateMesh);

	LogPrintf("\ttranslateMaterialIndices.nSearches: %d\n", stats.translateMaterialIndices.nSearches);
	LogPrintf("\ttranslateMaterialIndices.nLookups: %d\n", stats.translateMaterialIndices.nLookups);

	LogPrintf("\tcreateMesh.tSortVertices: %f\n", stats.createMesh.elapsedSortVertices);
	LogPrintf("\tcreateMesh.tMakeUniqueVertices: %f\n", stats.createMesh.elapsedMakeUniqueVertices);
	LogPrintf("\tcreateMesh.tSortTriangles: %f\n", stats.createMesh.elapsedSortTriangles);
	LogPrintf("\tcreateMesh.tCollectSubsets: %f\n", stats.createMesh.elapsedCollectSubsets);
	LogPrintf("\tcreateMesh.tProcessSubsets: %f\n", stats.createMesh.elapsedProcessSubsets);
	LogPrintf("\tcreateMesh.tCreateCMesh: %f\n", stats.createMesh.elapsedCreateCMesh);
}

void SCreateMeshStats::Log(const SCreateMeshStats& stats)
{
	LogPrintf("CreateMesh statistics:\n");

	LogPrintf("\ttTotal: %f\n", stats.elapsedTotal);
	LogPrintf("\ttSearchMaterials: %f\n", stats.elapsedSearchMaterials);

	SCreateEngineMeshStats::Log(stats.createEngineMesh);
}

