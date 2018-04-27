// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MeshCompiler.cpp
//  Version:     v1.00
//  Created:     5/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MeshCompiler.h"
#include "TangentSpaceCalculation.h"
#if CRY_PLATFORM_WINDOWS
	#include "ForsythFaceReorderer.h"
#endif

#include <cstring>    // memset()

namespace mesh_compiler
{

//////////////////////////////////////////////////////////////////////////
CMeshCompiler::CMeshCompiler()
	: m_pVertexMap(0)
	, m_pIndexMap(0)
{
}

//////////////////////////////////////////////////////////////////////////
CMeshCompiler::~CMeshCompiler()
{
}

#if CRY_PLATFORM_WINDOWS

namespace
{

struct VertexLess
{
	const CMesh& mesh;

	VertexLess(const CMesh& a_mesh)
		: mesh(a_mesh)
	{
		assert(mesh.m_pPositionsF16 == 0);
	}

	bool operator()(int a, int b) const
	{
		if (mesh.m_pTopologyIds && mesh.m_pTopologyIds[a] != mesh.m_pTopologyIds[b])
		{
			return mesh.m_pTopologyIds[a] < mesh.m_pTopologyIds[b];
		}

		int res = memcmp(&mesh.m_pPositions[a], &mesh.m_pPositions[b], sizeof(mesh.m_pPositions[a]));
		if (res)
		{
			return res < 0;
		}

		if ((mesh.m_pNorms && (res = memcmp(&mesh.m_pNorms[a], &mesh.m_pNorms[b], sizeof(mesh.m_pNorms[a])))) ||
		    (mesh.m_pTexCoord && (res = memcmp(&mesh.m_pTexCoord[a], &mesh.m_pTexCoord[b], sizeof(mesh.m_pTexCoord[a])))) ||
		    (mesh.m_pColor0 && (res = memcmp(&mesh.m_pColor0[a], &mesh.m_pColor0[b], sizeof(mesh.m_pColor0[a])))) ||
		    (mesh.m_pColor1 && (res = memcmp(&mesh.m_pColor1[a], &mesh.m_pColor1[b], sizeof(mesh.m_pColor1[a])))) ||
		    (mesh.m_pVertMats && (res = memcmp(&mesh.m_pVertMats[a], &mesh.m_pVertMats[b], sizeof(mesh.m_pVertMats[a])))) ||
		    (mesh.m_pTangents && (res = memcmp(&mesh.m_pTangents[a], &mesh.m_pTangents[b], sizeof(mesh.m_pTangents[a])))))
		{
			return res < 0;
		}

		return false;
	}
};

// Copies a vertex from old to new mesh
inline void CopyMeshVertex(CMesh& newMesh, int newVertex, const CMesh& oldMesh, int oldVertex)
{
	assert(newVertex < newMesh.GetVertexCount());
	assert(newMesh.m_pPositionsF16 == 0);
	assert(oldMesh.m_pPositionsF16 == 0);

	newMesh.m_pPositions[newVertex] = oldMesh.m_pPositions[oldVertex];
	if (oldMesh.m_pNorms)
		newMesh.m_pNorms[newVertex] = oldMesh.m_pNorms[oldVertex];
	if (oldMesh.m_pTopologyIds)
		newMesh.m_pTopologyIds[newVertex] = oldMesh.m_pTopologyIds[oldVertex];
	if (oldMesh.m_pTexCoord)
		newMesh.m_pTexCoord[newVertex] = oldMesh.m_pTexCoord[oldVertex];
	if (oldMesh.m_pColor0)
		newMesh.m_pColor0[newVertex] = oldMesh.m_pColor0[oldVertex];
	if (oldMesh.m_pColor1)
		newMesh.m_pColor1[newVertex] = oldMesh.m_pColor1[oldVertex];
	if (oldMesh.m_pVertMats)
		newMesh.m_pVertMats[newVertex] = oldMesh.m_pVertMats[oldVertex];
	if (oldMesh.m_pTangents)
		newMesh.m_pTangents[newVertex] = oldMesh.m_pTangents[oldVertex];
}

// Modified version of MeshUtils::Mesh::ComputeVertexRemapping()
// Computes vertexOldToNew and vertexNewToOld by detecting duplicate vertices
void ComputeVertexRemapping(const CMesh& mesh, std::vector<int>& vertexOldToNew, std::vector<int>& vertexNewToOld)
{
	const size_t nVerts = mesh.GetVertexCount();

	vertexNewToOld.resize(nVerts);
	for (size_t i = 0; i < nVerts; ++i)
	{
		vertexNewToOld[i] = i;
	}

	VertexLess less(mesh);
	std::sort(vertexNewToOld.begin(), vertexNewToOld.end(), less);

	vertexOldToNew.resize(nVerts);

	int nVertsNew = 0;
	for (size_t i = 0; i < nVerts; ++i)
	{
		if (i == 0 || less(vertexNewToOld[i - 1], vertexNewToOld[i]))
		{
			vertexNewToOld[nVertsNew++] = vertexNewToOld[i];
		}
		vertexOldToNew[vertexNewToOld[i]] = nVertsNew - 1;
	}
	vertexNewToOld.resize(nVertsNew);
}

} // namespace

//////////////////////////////////////////////////////////////////////////

namespace
{
class CMeshInputProxy
	: public ITriangleInputProxy
{
	struct Index
	{
		int index;
		int origPos;
	};

	template<class TComparator>
	void prepareUniqueIndices(std::vector<int>& outIndices, std::vector<Index>& tmp, const TComparator& comparator)
	{
		const int faceCount = m_mesh.GetFaceCount();

		tmp.resize(faceCount * 3);
		outIndices.resize(faceCount * 3, -1);

		for (int i = 0; i < faceCount; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				tmp[i * 3 + j].index = m_mesh.m_pFaces[i].v[j];
				tmp[i * 3 + j].origPos = i * 3 + j;
			}
		}

		std::sort(tmp.begin(), tmp.end(), comparator);

		int curIndex = -1;
		for (int i = 0, n = faceCount * 3; i < n; ++i)
		{
			if (curIndex < 0 || comparator(tmp[i - 1], tmp[i]))
			{
				curIndex = tmp[i].index;
			}
			outIndices[tmp[i].origPos] = curIndex;
		}
	}

	const char* ValidateMesh() const
	{
		if (m_mesh.m_pPositionsF16)
		{
			return "the mesh has 16-bit positions";
		}
		if (!m_mesh.m_pFaces)
		{
			return "the mesh has no stream with faces";
		}
		if (!m_mesh.m_pPositions)
		{
			return "the mesh has no stream with positions";
		}
		if (!m_mesh.m_pNorms)
		{
			return "the mesh has no stream with normals";
		}
		if (!m_mesh.m_pTexCoord)
		{
			return "the mesh has no stream with texture coordinates";
		}

		const int faceCount = m_mesh.GetFaceCount();
		const int vertexCount = m_mesh.GetVertexCount();
		const int texCoordCount = m_mesh.GetTexCoordCount();
		if (faceCount <= 0)
		{
			return "face count in the mesh is 0";
		}
		if (vertexCount <= 0)
		{
			return "vertex count in the mesh is 0";
		}
		if (texCoordCount <= 0)
		{
			return "texture coordinate count in the mesh is 0";
		}
		if (vertexCount != texCoordCount)
		{
			return "mismatch in number of positions and texture coordinates in the mesh";
		}

		for (int i = 0; i < faceCount; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				const int vIdx = m_mesh.m_pFaces[i].v[j];
				if (vIdx < 0 || vIdx >= vertexCount)
				{
					return "a face in the mesh has vertex index that is out of range";
				}
			}
		}

		// Trying to trigger a crash if a stream size is not correct
		{
			Vec3 v(0.0f, 0.0f, 0.0f);
			SMeshNormal n(v);
			SMeshTexCoord uv(0, 0);

			v = m_mesh.m_pPositions[0];  // cppcheck-suppress redundantAssignment
			v = m_mesh.m_pPositions[vertexCount - 1];
			n = m_mesh.m_pNorms[0];  // cppcheck-suppress redundantAssignment
			n = m_mesh.m_pNorms[vertexCount - 1];
			uv = m_mesh.m_pTexCoord[0]; // cppcheck-suppress redundantAssignment
			uv = m_mesh.m_pTexCoord[vertexCount - 1];
		}

		return 0;
	}

public:
	CMeshInputProxy(const CMesh& inMesh)
		: m_mesh(inMesh)
	{
		m_pErrorText = ValidateMesh();
		if (m_pErrorText)
		{
			return;
		}

		assert(m_mesh.m_pPositionsF16 == 0);

		struct PositionComparator
		{
			const Vec3* const pPositions;
			const int* const  pTopologyIds;

			PositionComparator(const Vec3* const a_pPositions, const int* const a_pTopologyIds)
				: pPositions(a_pPositions)
				, pTopologyIds(a_pTopologyIds)
			{
			}

			bool operator()(const Index& v0, const Index& v1) const
			{
				if (pTopologyIds)
				{
					const int a = pTopologyIds[v0.index];
					const int b = pTopologyIds[v1.index];
					if (a != b)
					{
						return a < b;
					}
				}
				const Vec3& a = pPositions[v0.index];
				const Vec3& b = pPositions[v1.index];
				if (a.x != b.x)
				{
					return a.x < b.x;
				}
				if (a.y != b.y)
				{
					return a.y < b.y;
				}
				return a.z < b.z;
			}
		};

		struct NormalComparator
		{
			const SMeshNormal* const pNormals;

			NormalComparator(const SMeshNormal* const a_pNormals)
				: pNormals(a_pNormals)
			{
			}

			bool operator()(const Index& v0, const Index& v1) const
			{
				const SMeshNormal& a = pNormals[v0.index];
				const SMeshNormal& b = pNormals[v1.index];

				return a < b;
			}
		};

		struct TexCoordComparator
		{
			const SMeshTexCoord* const pTexCoords;

			TexCoordComparator(const SMeshTexCoord* const a_pTexCoords)
				: pTexCoords(a_pTexCoords)
			{
			}

			bool operator()(const Index& v0, const Index& v1) const
			{
				const SMeshTexCoord& a = pTexCoords[v0.index];
				const SMeshTexCoord& b = pTexCoords[v1.index];

				return a < b;
			}
		};

		std::vector<Index> tmp;
		prepareUniqueIndices(m_posIndx, tmp, PositionComparator(m_mesh.m_pPositions, m_mesh.m_pTopologyIds));
		prepareUniqueIndices(m_normIndx, tmp, NormalComparator(m_mesh.m_pNorms));
		prepareUniqueIndices(m_texCoordIndx, tmp, TexCoordComparator(m_mesh.m_pTexCoord));
	}

	const char* GetErrorText() const
	{
		return m_pErrorText;
	}

	// interface ITriangleInputProxy ----------------------------------------------

	//! /return 0..
	uint32 GetTriangleCount() const
	{
		return m_mesh.GetFaceCount();
	}

	//! /param indwTriNo 0..
	//! /param outdwPos
	//! /param outdwNorm
	//! /param outdwUV
	void GetTriangleIndices(const uint32 indwTriNo, uint32 outdwPos[3], uint32 outdwNorm[3], uint32 outdwUV[3]) const
	{
		const int* const pPosInds = &m_posIndx[indwTriNo * 3];
		const int* const pNormInds = &m_normIndx[indwTriNo * 3];
		const int* const pTexCoordInds = &m_texCoordIndx[indwTriNo * 3];

		for (int j = 0; j < 3; ++j)
		{
			outdwPos[j] = pPosInds[j];
			outdwUV[j] = pTexCoordInds[j];
			outdwNorm[j] = pNormInds[j];
		}
	}

	//! /param indwPos 0..
	//! /param outfPos
	void GetPos(const uint32 indwPos, Vec3& outfPos) const
	{
		assert(!m_pErrorText);
		assert((int)indwPos < m_mesh.GetVertexCount());
		outfPos = m_mesh.m_pPositions[indwPos];
	}

	//! /param indwPos 0..
	//! /param outfUV
	void GetUV(const uint32 indwPos, Vec2& outfUV) const
	{
		assert(!m_pErrorText);
		assert((int)indwPos < m_mesh.GetTexCoordCount());
		outfUV = m_mesh.m_pTexCoord[indwPos].GetUV();
	}

	//! /param indwTriNo 0..
	//! /param indwVertNo 0..
	//! /param outfNorm
	void GetNorm(const uint32 indwTriNo, const uint32 indwVertNo, Vec3& outfNorm) const
	{
		assert(!m_pErrorText);
		assert((int)indwTriNo < m_mesh.GetFaceCount());
		assert((int)indwVertNo < 3);
		const int vIdx = m_mesh.m_pFaces[indwTriNo].v[indwVertNo];
		assert(vIdx < m_mesh.GetVertexCount());
		outfNorm = m_mesh.m_pNorms[vIdx].GetN();
	}
	//-----------------------------------------------------------------------------

private:
	const CMesh&     m_mesh;
	const char*      m_pErrorText;
	std::vector<int> m_posIndx;       // indices of unique positions (in mesh.m_pPositions) for each corner of each triangle
	std::vector<int> m_normIndx;      // indices of unique normals (in mesh.m_pNorms) for each corner of each triangle
	std::vector<int> m_texCoordIndx;  // indices of unique texture coordinates normals (in mesh.m_pTexCoord) for each corner of each triangle
};
}

//do not use vec3 lib to keep it the fallback as it was
inline static Vec3 CrossProd(const Vec3& a, const Vec3& b)
{
	Vec3 ret;
	ret.x = a.y * b.z - a.z * b.y;
	ret.y = a.z * b.x - a.x * b.z;
	ret.z = a.x * b.y - a.y * b.x;
	return ret;
}

inline static void GetOtherBaseVec(const Vec3& s, Vec3& a, Vec3& b)
{
	if (fabsf(s.z) > 0.5f)
	{
		a.x = s.z;
		a.y = s.y;
		a.z = -s.x;
	}
	else
	{
		a.x = s.y;
		a.y = -s.x;
		a.z = s.z;
	}

	b = CrossProd(s, a).normalize();
	a = CrossProd(b, s).normalize();
}

//check packed tangent space and ensure some useful values, fix always according to normal
static void VerifyTangentSpace(SMeshTangents& rTangents, const SMeshNormal& rNormal)
{
	Vec3 normal = rNormal.GetN();

	if (normal.GetLengthSquared() < 0.1f)
		normal = Vec3(0, 0, 1);
	else if (normal.GetLengthSquared() < 0.9f)
		normal.Normalize();

	//unpack first(necessary since the quantization can introduce errors whereas the original float data were different)
	Vec3 tangent, bitangent;
	rTangents.GetTB(tangent, bitangent);

	//check if they are equal
	const bool cIsEqual = (tangent == bitangent);
	//check if they are zero
	const bool cTangentIsZero = (tangent.GetLengthSquared() < 0.01f);
	const bool cBitangentIsZero = (bitangent.GetLengthSquared() < 0.01f);
	const bool cbHasBeenChanged = (cIsEqual || cTangentIsZero || cBitangentIsZero);

	if (cIsEqual)
	{
		//fix case where both vec's are equal
		GetOtherBaseVec(normal, tangent, bitangent);
	}
	else if (cTangentIsZero)
	{
		//fix case where tangent is zero
		bitangent.Normalize();//just to make sure
		if (abs(bitangent * normal) > 0.9f)//if angle between both vecs is too low, calc new one for both
			GetOtherBaseVec(normal, tangent, bitangent);
		else
			tangent = CrossProd(normal, bitangent);
	}
	else if (cBitangentIsZero)
	{
		//fix case where bitangent is zero
		tangent.Normalize();//just to make sure
		if (abs(tangent * normal) > 0.9f)//if angle between both vecs is too low, calc new one for both
			GetOtherBaseVec(normal, tangent, bitangent);
		else
			bitangent = CrossProd(tangent, normal);
	}

	//pack altered tangent vecs
	if (cbHasBeenChanged)
	{
		rTangents = SMeshTangents(tangent, bitangent, normal);
	}
}

static void debugDumpMesh(CMesh& mesh, const char* filename)
{
	FILE* f;
	f = fopen(filename, "wb");
	if (!f)
	{
		return;
	}

	fprintf(f, "[Pos]\n");
	for (int i = 0; i < mesh.GetVertexCount(); ++i)
	{
		fprintf(f, "\t%g %g %g\n", mesh.m_pPositions[i].x, mesh.m_pPositions[i].y, mesh.m_pPositions[i].z);
	}

	fprintf(f, "[Nor]\n");
	for (int i = 0; i < mesh.GetVertexCount(); ++i)
	{
		const Vec3 n = mesh.m_pNorms[i].GetN();
		fprintf(f, "\t%g %g %g\n", n.x, n.y, n.z);
	}

	if (mesh.m_pTexCoord)
	{
		fprintf(f, "[TexCoord]\n");
		for (int i = 0; i < mesh.GetTexCoordCount(); ++i)
		{
			const Vec2 uv = mesh.m_pTexCoord[i].GetUV();
			fprintf(f, "\t%g %g\n", uv.x, uv.y);
		}
	}

	fprintf(f, "[Faces]\n");
	for (int i = 0; i < mesh.GetIndexCount(); i += 3)
	{
		fprintf(f, "\t%u %u %u\n", (uint)mesh.m_pIndices[i + 0], (uint)mesh.m_pIndices[i + 1], (uint)mesh.m_pIndices[i + 2]);
	}

	fclose(f);
}

//////////////////////////////////////////////////////////////////////////
// Optimizes CMesh.
// IMPLEMENTATION:
// . Sort|Group faces by materials
// . Create vertex buffer with sequence of (possibly non-unique) vertices, 3 verts per face
// . For each (non-unique) vertex calculate the tangent base
// . Index the mesh (Compact Vertices): detect and delete duplicate vertices
// . Remove degenerated triangles in the generated mesh (GetIndices())
// . Sort vertices and indices for GPU cache
bool CMeshCompiler::Compile(CMesh& mesh, int flags)
{
	assert(mesh.m_pPositionsF16 == 0);

	if (mesh.GetFaceCount() == 0)
	{
		// the mesh is either empty or already compiled

		const int cVertexCount = mesh.GetVertexCount();
		if (cVertexCount == 0)
		{
			// the mesh is empty, nothing to do
			return true;
		}

		// the mesh is already compiled, likely to have a refresh here: just verify and correct tangent space
		if (mesh.m_pTangents && mesh.m_pNorms)
		{
			for (int i = 0; i < cVertexCount; ++i)
			{
				VerifyTangentSpace(mesh.m_pTangents[i], mesh.m_pNorms[i]);
			}
		}

		return true;
	}

	// the mesh has faces - it means that it's a non-compiled mesh. let's compile it.

	// Check input data
	{
		if (mesh.GetIndexCount() > 0)
		{
			m_LastError.Format(
			  "Mesh compilation failed - input mesh has both indices and faces. Contact an RC programmer.");
			return false;
		}

		const int vertexCount = mesh.GetVertexCount();
		const int faceCount = mesh.GetFaceCount();

		for (int i = 0; i < faceCount; ++i)
		{
			const SMeshFace& face = mesh.m_pFaces[i];
			if (face.nSubset < 0 || face.nSubset >= MAX_SUB_MATERIALS)
			{
				m_LastError.Format(
				  "Mesh compilation failed - face %d has bad subset index %d (allowed range is [0;%d]). Contact an RC programmer.",
				  i, (int)face.nSubset, MAX_SUB_MATERIALS - 1);
				return false;
			}
			for (int j = 0; j < 3; ++j)
			{
				const int vIdx = mesh.m_pFaces[i].v[j];
				if (vIdx < 0 || vIdx >= vertexCount)
				{
					m_LastError.Format(
					  "Mesh compilation failed - face %d has bad vertex index %d (allowed range is [0;%d]). Contact an RC programmer.",
					  i, vIdx, vertexCount - 1);
					return false;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Calculate Tangent Space.
	// Results will be stored in bases[] and m_thash_table[]
	//////////////////////////////////////////////////////////////////////////

	std::vector<SMeshTangents> bases;

	// m_thash_table[] contains a std::vector<SBasisFace> per subset.
	// Vector contains faces belonging to the subset.
	// Face contains three indices of elements in bases[].
	static_assert(CRY_ARRAY_COUNT(m_thash_table) == MAX_SUB_MATERIALS, "Invalid array size!");
	for (int i = 0; i < MAX_SUB_MATERIALS; ++i)
	{
		m_thash_table[i].clear();
	}

	if (flags & MESH_COMPILE_TANGENTS)
	{
		// Generate tangent basis vectors before indexing per-material

		CMeshInputProxy Input(mesh);
		if (Input.GetErrorText())
		{
			m_LastError.Format("Mesh compilation failed - %s. Contact an RC or Editor programmer.", Input.GetErrorText());
			return false;
		}

		CTangentSpaceCalculation tangents;
		string errorMessage;

		// calculate the base matrices
		const bool bUseCustomNormals = (flags & MESH_COMPILE_USE_CUSTOM_NORMALS) != 0;
		const bool bIgnoreSomeErrors = (flags & MESH_COMPILE_IGNORE_TANGENT_SPACE_ERRORS) != 0;
		const eCalculateTangentSpaceErrorCode nErrorCode = tangents.CalculateTangentSpace(Input, bUseCustomNormals, bIgnoreSomeErrors, errorMessage);

		if (nErrorCode != CALCULATE_TANGENT_SPACE_NO_ERRORS)
		{
			const char* errorCodeMessage;

			switch (nErrorCode)
			{
			case VERTICES_SHARING_COORDINATES:
				errorCodeMessage = "\nReason: two vertices of a face share the same coordinate. Run the Select Degenerate Faces Script and fix any highlighted vertices.\n";
				break;
			case ALL_VERTICES_ON_THE_SAME_VECTOR:
				errorCodeMessage = "\nReason: three vertices of a face lie on the same line. Run the Select Degenerate Faces Script and fix any highlighted vertices.\n";
				break;
			case BROKEN_TEXTURE_COORDINATES:
				errorCodeMessage = "\nReason: texture coordinates are not valid. Double check that the UV's have space on the UV map.\n";
				break;
			case MEMORY_ALLOCATION_FAILED:
				errorCodeMessage = "\nReason: failed to allocate memory for Mikkelsen's Tangent Basis algorithm.";
				break;
			default:
				errorCodeMessage = "\nInternal error.\n";
				break;
			}

			m_LastError.Format(" CalculateTangentSpace() failed, fix model.\n\nErrorCode:%d\n\n%s%s", nErrorCode, errorMessage.c_str(), errorCodeMessage);
			return false;
		}

		const uint32 dwCnt = tangents.GetBaseCount();
		const uint32 dwTris = Input.GetTriangleCount();

		bases.resize(dwCnt);

		std::vector<int> basisIndices;
		basisIndices.resize(dwTris * 3);

		for (uint32 dwTri = 0; dwTri < dwTris; dwTri++)
		{
			uint32 dwBaseIndx[3];

			tangents.GetTriangleBaseIndices(dwTri, dwBaseIndx);

			// for every corner of the triangle
			for (uint32 i = 0; i < 3; i++)
			{
				assert(dwBaseIndx[i] < dwCnt);
				basisIndices[dwTri * 3 + i] = dwBaseIndx[i];  // set the base vector
			}
		}

		for (uint32 i = 0; i < dwCnt; i++)
		{
			Vec3 Tangent, Bitangent, Normal;

			tangents.GetBase(i, (float*)&Tangent, (float*)&Bitangent, (float*)&Normal);

			bases[i] = SMeshTangents(Tangent, Bitangent, Normal);

			VerifyTangentSpace(bases[i], SMeshNormal(Normal));
		}

		const int faceCount = mesh.GetFaceCount();
		for (int i = 0; i < faceCount; i++)
		{
			SBasisFace fc;

			fc.v[0] = basisIndices[i * 3 + 0];
			fc.v[1] = basisIndices[i * 3 + 1];
			fc.v[2] = basisIndices[i * 3 + 2];

			const SMeshFace& face = mesh.m_pFaces[i];
			m_thash_table[face.nSubset].push_back(fc);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	// Create new mesh that will store non-unique vertices, 3 vertices per face

	const int max_vert_num = mesh.GetFaceCount() * 3;

	CMesh outMesh;
	outMesh.CopyFrom(mesh);
	outMesh.SetVertexCount(max_vert_num);
	outMesh.ReallocStream(CMesh::VERT_MATS, max_vert_num);
	if (mesh.m_pTopologyIds)
		outMesh.ReallocStream(CMesh::TOPOLOGY_IDS, max_vert_num);
	if (mesh.m_pTexCoord)
		outMesh.ReallocStream(CMesh::TEXCOORDS, max_vert_num);
	if (flags & MESH_COMPILE_TANGENTS)
		outMesh.ReallocStream(CMesh::TANGENTS, max_vert_num);
	if (mesh.m_pColor0)
		outMesh.ReallocStream(CMesh::COLORS_0, max_vert_num);
	if (mesh.m_pColor1)
		outMesh.ReallocStream(CMesh::COLORS_1, max_vert_num);

	// temporarily store original subset index in subset's nNumVerts
	{
		const uint32 nSubsets = outMesh.GetSubSetCount();
		assert(nSubsets <= MAX_SUB_MATERIALS);
		for (uint32 i = 0; i < nSubsets; i++)
		{
			outMesh.m_subsets[i].nNumVerts = i;
		}
	}

	// Sort subsets depending on their physicalization type (don't do it for character meshes (with mapping)).
	if (!m_pVertexMap)
	{
		// move normal physicalize subsets to the beginning (needed for breakable objects)
		for (uint32 i = 0; i < (uint32)outMesh.m_subsets.size(); i++)
		{
			const SMeshSubset& outSubset = outMesh.m_subsets[i];
			if (outSubset.nPhysicalizeType == PHYS_GEOM_TYPE_DEFAULT)
			{
				const SMeshSubset tmp = outSubset;
				outMesh.m_subsets.erase(outMesh.m_subsets.begin() + i);
				outMesh.m_subsets.insert(outMesh.m_subsets.begin(), tmp);
			}
		}
		// move physicalize proxy subsets to the end
		for (int nSubset = (int)outMesh.m_subsets.size() - 1; nSubset >= 0; --nSubset)
		{
			const SMeshSubset& outSubset = outMesh.m_subsets[nSubset];
			if (outSubset.nPhysicalizeType != PHYS_GEOM_TYPE_NONE && outSubset.nPhysicalizeType != PHYS_GEOM_TYPE_DEFAULT)
			{
				const SMeshSubset tmp = outSubset;
				outMesh.m_subsets.erase(outMesh.m_subsets.begin() + nSubset);
				outMesh.m_subsets.push_back(tmp);
			}
		}
	}

	// m_vhash_table[] contains a std::vector<SMeshFace> per subset.
	// Vector contains faces belonging to the subset.
	// Face contains three indices of elements in mesh.m_pVertices[].
	static_assert(CRY_ARRAY_COUNT(m_vhash_table) == MAX_SUB_MATERIALS, "Invalid array size!");
	for (int i = 0; i < MAX_SUB_MATERIALS; ++i)
	{
		m_vhash_table[i].clear();
	}
	for (int i = 0, n = mesh.GetFaceCount(); i < n; ++i)
	{
		const SMeshFace& face = mesh.m_pFaces[i];
		assert(face.nSubset < MAX_SUB_MATERIALS);
		m_vhash_table[face.nSubset].push_back(&face);
	}

	// Fill the new mesh with vertices
	{
		int buff_vert_count = 0;

		for (int t = 0; t < outMesh.GetSubSetCount(); t++)
		{
			SMeshSubset& subset = outMesh.m_subsets[t];
			// memorize the starting index of this material's face range
			subset.nFirstIndexId = buff_vert_count;

			// scan through all the faces using the shader #t.
			// note: subset's nNumVerts contains original subset index
			const size_t nNumFacesInSubset = m_vhash_table[subset.nNumVerts].size();
			for (size_t i = 0; i < nNumFacesInSubset; ++i)
			{
				const SMeshFace* const pFace = m_vhash_table[subset.nNumVerts][i];

				for (int v = 0; v < 3; ++v)
				{
					CopyMeshVertex(outMesh, buff_vert_count, mesh, pFace->v[v]);

					if (!bases.empty())
					{
						const SBasisFace& tFace = m_thash_table[subset.nNumVerts][i];
						outMesh.m_pTangents[buff_vert_count] = bases[tFace.v[v]];
					}

					// store subset id to prevent vertex sharing between materials during re-compacting
					outMesh.m_pVertMats[buff_vert_count] = pFace->nSubset;

					++buff_vert_count;
				}
			}

			subset.nNumIndices = buff_vert_count - subset.nFirstIndexId;
		}

		if (buff_vert_count != max_vert_num)
		{
			m_LastError.Format("Mesh compilation failed - internal error in handling vertices (real#:%d, expected#:%d). Contact an RC programmer.", buff_vert_count, max_vert_num);
			return false;
		}
	}

	if (!CreateIndicesAndDeleteDuplicateVertices(outMesh))
	{
		return false;
	}

	bool bFoundDegenerateFaces = false;
	if (flags & MESH_COMPILE_VALIDATE)
	{
		bFoundDegenerateFaces = CheckForDegenerateFaces(outMesh);
	}

	if (flags & MESH_COMPILE_OPTIMIZE)
	{
		const bool bOk = StripifyMesh_Forsyth(outMesh);
		if (!bOk)
		{
			m_LastError.Format("Mesh compilation failed - stripifier failed. Contact an RC programmer.");
			return false;
		}
	}
	else
	{
		if (m_pIndexMap || m_pVertexMap)
		{
			m_LastError.Format("Mesh compilation failed - face and/or index maps cannot be requested without OPTIMIZE. Contact an RC programmer.");
			return false;
		}
	}

	FindVertexRanges(outMesh);

	// Copy modified mesh back to original one.
	mesh.CopyFrom(outMesh);

	// Calculate bounding box.
	mesh.m_bbox.Reset();
	for (int i = 0, n = mesh.GetVertexCount(); i < n; ++i)
	{
		mesh.m_bbox.Add(mesh.m_pPositions[i]);
	}

	if (flags & MESH_COMPILE_VALIDATE)
	{
		if (bFoundDegenerateFaces)
		{
			m_LastError.Format("Mesh contains degenerate faces.");
			return false;
		}

		const char* pErrorDescription = 0;
		if (!mesh.Validate(&pErrorDescription))
		{
			m_LastError.Format("Internal error in mesh compiling (%s). Contact an RC programmer.", pErrorDescription);
			return false;
		}
	}

	return true;
}

bool CMeshCompiler::StripifyMesh_Forsyth(CMesh& mesh)
{
	if (mesh.GetFaceCount() > 0)
	{
		// We don't support stripifying of meshes with explicit faces, we support meshes with index array only
		return false;
	}

	enum { kCACHESIZE_GEFORCE3 = 24 };
	const size_t cacheSize = kCACHESIZE_GEFORCE3;
	enum { kVerticesPerFace = 3 };

	// Prepare mapping buffers
	if (m_pIndexMap)
	{
		const int n = mesh.GetIndexCount();
		m_pIndexMap->resize(n);
		for (int i = 0; i < n; ++i)
		{
			(*m_pIndexMap)[i] = mesh.m_pIndices[i];
		}
	}
	if (m_pVertexMap)
	{
		const int n = mesh.GetVertexCount();
		m_pVertexMap->resize(n, -1);
	}

	CMesh newMesh;
	newMesh.CopyFrom(mesh);

	// TODO: make those variables members of CMeshCompiler so we don't need to allocate memory every time
	ForsythFaceReorderer ffr;
	std::vector<uint32> buffer0;
	std::vector<uint32> buffer1;

	// Reserve space
	//
	// We will use buffer0 for both subset's indices and for mapping from old vertex indices
	// to new vertex indices (number of vertices decreases in case some vertices are not
	// referenced from indices). In the latter case having size of buffer0 equal to number of
	// indices is not enough if indices refer vertices in a spare fashion. Unfortunately,
	// to compute range of referenced vertices we need to scan all indices in all subsets
	// which is not fast.
	{
		int maxIndexCountInSubset = 0;
		int maxVertexCountInSubset = 0;

		for (int i = 0; i < newMesh.GetSubSetCount(); i++)
		{
			const SMeshSubset& subset = mesh.m_subsets[i];

			if (subset.nNumIndices == 0)
			{
				continue;
			}
			if (subset.nNumIndices < 0)
			{
				assert(0);
				return false;
			}
			if (subset.nNumIndices % kVerticesPerFace != 0)
			{
				assert(0);
				return false;
			}
			if (subset.nFirstIndexId % kVerticesPerFace != 0)
			{
				assert(0);
				return false;
			}

			if (maxIndexCountInSubset < subset.nNumIndices)
			{
				maxIndexCountInSubset = subset.nNumIndices;
			}

			int subsetMinIndex = mesh.m_pIndices[subset.nFirstIndexId];
			int subsetMaxIndex = subsetMinIndex;
			for (int j = 1; j < subset.nNumIndices; ++j)
			{
				const int idx = mesh.m_pIndices[subset.nFirstIndexId + j];
				if (idx < subsetMinIndex)
				{
					subsetMinIndex = idx;
				}
				else if (idx > subsetMaxIndex)
				{
					subsetMaxIndex = idx;
				}
			}

			if (maxVertexCountInSubset < subsetMaxIndex - subsetMinIndex + 1)
			{
				maxVertexCountInSubset = subsetMaxIndex - subsetMinIndex + 1;
			}
		}

		buffer0.resize(max(maxIndexCountInSubset, maxVertexCountInSubset));
		buffer1.resize(maxIndexCountInSubset);
	}

	int newVertexCount = 0;

	for (int i = 0; i < newMesh.GetSubSetCount(); i++)
	{
		const SMeshSubset& subset = mesh.m_subsets[i];

		if (subset.nNumIndices == 0)
		{
			continue;
		}

		int subsetMinIndex = mesh.m_pIndices[subset.nFirstIndexId];
		int subsetMaxIndex = subsetMinIndex;
		for (int j = 1; j < subset.nNumIndices; ++j)
		{
			const int idx = mesh.m_pIndices[subset.nFirstIndexId + j];
			if (idx < subsetMinIndex)
			{
				subsetMinIndex = idx;
			}
			else if (idx > subsetMaxIndex)
			{
				subsetMaxIndex = idx;
			}
		}

		for (int j = 0; j < subset.nNumIndices; ++j)
		{
			buffer0[j] = mesh.m_pIndices[subset.nFirstIndexId + j] - subsetMinIndex;
		}

		const bool bOk = ffr.reorderFaces(
		  cacheSize,
		  kVerticesPerFace,
		  subset.nNumIndices,
		  &buffer0[0],  // inVertexIndices
		  &buffer1[0],  // outVertexIndices
		  0);           // faceToOldFace[] - we don't need it

		if (!bOk)
		{
			return false;
		}

		// Reorder vertices

		SMeshSubset& newSubset = newMesh.m_subsets[i];
		newSubset.nFirstVertId = newVertexCount;
		newSubset.nNumVerts = 0;
		newSubset.nFirstIndexId = subset.nFirstIndexId;
		newSubset.nNumIndices = subset.nNumIndices;

		const int oldSubsetVertexCount = (int)subsetMaxIndex - (int)subsetMinIndex + 1;

		assert(buffer0.size() >= (size_t)oldSubsetVertexCount);
		memset(&buffer0[0], -1, sizeof(buffer0[0]) * oldSubsetVertexCount);

		for (int j = 0; j < subset.nNumIndices; ++j)
		{
			const uint32 idx = buffer1[j];
			const int oldVertexIndex = subsetMinIndex + idx;
			if (buffer0[idx] == -1)
			{
				if (m_pVertexMap)
				{
					(*m_pVertexMap)[oldVertexIndex] = newVertexCount;
				}
				buffer0[idx] = newVertexCount;
				//copy from old -> new vertex buffer
				CopyMeshVertex(newMesh, newVertexCount, mesh, oldVertexIndex);
				++newVertexCount;
				++newSubset.nNumVerts;
			}
			newMesh.m_pIndices[subset.nFirstIndexId + j] = buffer0[idx];
		}
	}

	newMesh.SetVertexCount(newVertexCount);

	mesh.CopyFrom(newMesh);

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// Input:
//   mesh contains mesh.GetVertexCount() vertices (vertex data are stored in
//   m_pPositions[] m_pNorms[] and in other data streams).
//   Face and index streams are ignored.
// Output:
//   1) mesh contains unique vertices only.
//   2) data stream mesh.m_pIndices has "inputMesh.GetVertexCount()"
//      indices (one output index per each input vertex).
//      note that an output index points to an *unique* vertex in the
//      output mesh.
//   3) data stream mesh.m_pFaces is empty.
//
// For example vertices [A, B, B, C, A, D] will be transformed to
// [A, B, C, D], and index array created will be [0, 1, 1, 2, 0, 3].
//
// Note that mesh.subsets[] is neither used nor changed.
//
bool CMeshCompiler::CreateIndicesAndDeleteDuplicateVertices(CMesh& mesh)
{
	assert(mesh.m_pPositionsF16 == 0);

	const int oldVertexCount = mesh.GetVertexCount();
	if (oldVertexCount <= 0)
	{
		return true;
	}

	CMesh oldMesh;
	oldMesh.CopyFrom(mesh);

	std::vector<int> vertexOldToNew;
	std::vector<int> vertexNewToOld;
	ComputeVertexRemapping(oldMesh, vertexOldToNew, vertexNewToOld);

	const int newVertexCount = (int)vertexNewToOld.size();

	assert(vertexOldToNew.size() == oldVertexCount);
	const uint maxVertexCount = (sizeof(vtx_idx) == 2 ? 0xffff : 0x7fffffff);
	if (newVertexCount > maxVertexCount)
	{
		m_LastError.Format("Too many vertices in mesh after compilation: %u (limit is %u).", (uint)newVertexCount, (uint)maxVertexCount);
		return false;
	}

	for (int i = 0; i < newVertexCount; ++i)
	{
		CopyMeshVertex(mesh, i, oldMesh, vertexNewToOld[i]);
	}

	mesh.SetVertexCount(newVertexCount);
	if (mesh.m_pNorms)
		mesh.ReallocStream(CMesh::NORMALS, newVertexCount);
	if (mesh.m_pTexCoord)
		mesh.ReallocStream(CMesh::TEXCOORDS, newVertexCount);
	if (mesh.m_pColor0)
		mesh.ReallocStream(CMesh::COLORS_0, newVertexCount);
	if (mesh.m_pColor1)
		mesh.ReallocStream(CMesh::COLORS_1, newVertexCount);
	if (mesh.m_pTangents)
		mesh.ReallocStream(CMesh::TANGENTS, newVertexCount);
	mesh.ReallocStream(CMesh::TOPOLOGY_IDS, 0);
	mesh.ReallocStream(CMesh::VERT_MATS, 0);
	mesh.SetFaceCount(0);
	mesh.SetIndexCount(oldVertexCount);
	for (int i = 0; i < oldVertexCount; ++i)
	{
		mesh.m_pIndices[i] = vertexOldToNew[i];
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMeshCompiler::CheckForDegenerateFaces(const CMesh& mesh)
{
	for (int i = 0; i < mesh.GetSubSetCount(); i++)
	{
		const SMeshSubset& subset = mesh.m_subsets[i];
		for (int j = subset.nFirstIndexId; j < subset.nFirstIndexId + subset.nNumIndices; j += 3)
		{
			if (mesh.m_pIndices[j + 0] == mesh.m_pIndices[j + 1] ||
			    mesh.m_pIndices[j + 1] == mesh.m_pIndices[j + 2] ||
			    mesh.m_pIndices[j + 2] == mesh.m_pIndices[j + 0])
			{
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMeshCompiler::FindVertexRanges(CMesh& mesh)
{
	assert(mesh.m_pPositionsF16 == 0);

	const int nNumIndices = mesh.GetIndexCount();

	// Find vertex range (both index and spacial ranges) for each material (needed for rendering)
	for (int i = 0; i < mesh.GetSubSetCount(); i++)
	{
		SMeshSubset& subset = mesh.m_subsets[i];

		if (subset.nNumIndices == 0)
		{
			subset.nNumVerts = 0;
			continue;
		}

		if (subset.nNumIndices + subset.nFirstIndexId > nNumIndices)
		{
			assert(0);
			continue;
		}

		int nMin = INT_MAX;
		int nMax = INT_MIN;
		Vec3 vMin = SetMaxBB();
		Vec3 vMax = SetMinBB();

		for (int j = subset.nFirstIndexId; j < subset.nNumIndices + subset.nFirstIndexId; j++)
		{
			int index = mesh.m_pIndices[j];
			Vec3 v = mesh.m_pPositions[index];
			vMin.CheckMin(v);
			vMax.CheckMax(v);
			nMin = min(nMin, index);
			nMax = max(nMax, index);
		}
		subset.vCenter = (vMin + vMax) * 0.5f;
		subset.fRadius = (vMin - subset.vCenter).GetLength();
		subset.nFirstVertId = nMin;
		subset.nNumVerts = nMax - nMin + 1;
	}
}

#endif   // #if CRY_PLATFORM_WINDOWS

//////////////////////////////////////////////////////////////////////////
bool CMeshCompiler::CompareMeshes(const CMesh& mesh1, const CMesh& mesh2)
{
	if (mesh1.m_subsets.size() != mesh2.m_subsets.size())
		return false;

	if (mesh1.GetFaceCount() != mesh2.GetFaceCount())
		return false;
	if (mesh1.GetVertexCount() != mesh2.GetVertexCount())
		return false;
	if (mesh1.GetTexCoordCount() != mesh2.GetTexCoordCount())
		return false;
	if (mesh1.GetIndexCount() != mesh2.GetIndexCount())
		return false;

	if (!mesh1.CompareStreams(mesh2))
		return false;

	return true;
}

} // namespace mesh_compiler
