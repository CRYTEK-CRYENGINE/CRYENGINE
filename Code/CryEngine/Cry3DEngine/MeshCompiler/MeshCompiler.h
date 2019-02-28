// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MeshCompiler.h
//  Version:     v1.00
//  Created:     5/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MeshCompiler_h__
#define __MeshCompiler_h__
#pragma once

#include <Cry3DEngine/IIndexedMesh.h>

namespace mesh_compiler
{

enum EMeshCompileFlags
{
	MESH_COMPILE_OPTIMIZE                    = BIT(0),
	MESH_COMPILE_TANGENTS                    = BIT(1),
	MESH_COMPILE_IGNORE_TANGENT_SPACE_ERRORS = BIT(2),
	MESH_COMPILE_USE_CUSTOM_NORMALS          = BIT(3),
	MESH_COMPILE_VALIDATE                    = BIT(4),
};

//////////////////////////////////////////////////////////////////////////
class CMeshCompiler
{
public:
	CMeshCompiler();
	~CMeshCompiler();

#if CRY_PLATFORM_WINDOWS
	// for flags see EMeshCompilerFlags
	bool Compile(CMesh& mesh, int flags);
#endif

	void SetVertexRemapping(std::vector<int>* pVertexMap)
	{
		m_pVertexMap = pVertexMap;
	}

	void SetIndexRemapping(std::vector<int>* pIndexMap)
	{
		m_pIndexMap = pIndexMap;
	}

	static inline bool IsEquivalentVec3dCheckYFirst(const Vec3& v0, const Vec3& v1, float fEpsilon)
	{
		if (fabsf(v0.y - v1.y) < fEpsilon)
			if (fabsf(v0.x - v1.x) < fEpsilon)
				if (fabsf(v0.z - v1.z) < fEpsilon)
					return true;
		return false;
	}

	static inline const Vec3& ToVec3(const Vec3& vec)    { return vec; }
	static inline const Vec3  ToVec3(const Vec3f16& vec) { return vec.ToVec3(); }

	template<class T>
	static inline int FindInPosBuffer_VF_P3X(const Vec3& vPosToFind, const T* pVertBuff, std::vector<int>* pHash, float fEpsilon)
	{
		for (uint32 i = 0; i < pHash->size(); i++)
			if (IsEquivalentVec3dCheckYFirst(ToVec3(pVertBuff[(*pHash)[i]].xyz), vPosToFind, fEpsilon))
				return (*pHash)[i];

		return -1;
	}

	template<class V, class I>
	void WeldPos_VF_P3X(
	  PodArray<V>& vertices,
	  PodArray<SPipTangents>& tangents,
	  PodArray<SPipNormal>& normals,
	  PodArray<I>& indices,
	  float fEpsilon,
	  const AABB& boxBoundary)
	{
		const int numVertices = vertices.Count();
		V* pTmpVerts = new V[numVertices];
		SPipTangents* pTmpTangents = new SPipTangents[tangents.Count()];

#if ENABLE_NORMALSTREAM_SUPPORT
		SPipNormal* pTmpNormals = NULL;
		if (normals.Count() > 0)
			pTmpNormals = new SPipNormal[normals.Count()];

		SPipNormal emptyNormal;
#endif

		int nCurVertex = 0;
		PodArray<I> newIndices;
		std::vector<int> arrHashTable[256];
		newIndices.reserve(indices.size());
		std::vector<int>* pHash = 0;

		float fHashElemSize = 256.0f / max(boxBoundary.max.x - boxBoundary.min.x, 0.01f);

		for (uint32 i = 0; i < indices.size(); i++)
		{
			int v = indices[i];

			assert(v < vertices.Count());

			V& vPos = vertices[v];
			SPipTangents& vTang = tangents[v];
#if ENABLE_NORMALSTREAM_SUPPORT
			SPipNormal& vNorm = pTmpNormals ? normals[v] : emptyNormal;
#endif

			bool bInRange(
			  vPos.xyz.x > boxBoundary.min.x && vPos.xyz.y > boxBoundary.min.y && vPos.xyz.z > boxBoundary.min.z &&
			  vPos.xyz.x < boxBoundary.max.x && vPos.xyz.y < boxBoundary.max.y && vPos.xyz.z < boxBoundary.max.z);

			int nHashValue = int((vPos.xyz.x - boxBoundary.min.x) * fHashElemSize);

			pHash = &arrHashTable[(unsigned char)(nHashValue)];
			int nFind = FindInPosBuffer_VF_P3X(ToVec3(vPos.xyz), pTmpVerts, pHash, bInRange ? fEpsilon : 0.01f);
			if (nFind < 0)
			{
				pHash->push_back(nCurVertex);

				// make sure neighbor hashes also have this vertex
				if (bInRange && fEpsilon > 0.01f)
				{
					pHash = &arrHashTable[(unsigned char)(nHashValue + 1)];
					if (FindInPosBuffer_VF_P3X(ToVec3(vPos.xyz), pTmpVerts, pHash, fEpsilon) < 0)
						pHash->push_back(nCurVertex);

					pHash = &arrHashTable[(unsigned char)(nHashValue - 1)];
					if (FindInPosBuffer_VF_P3X(ToVec3(vPos.xyz), pTmpVerts, pHash, fEpsilon) < 0)
						pHash->push_back(nCurVertex);
				}

				PREFAST_ASSUME(nCurVertex < numVertices);

				// add new vertex
				pTmpVerts[nCurVertex] = vPos;
				pTmpTangents[nCurVertex] = vTang;
#if ENABLE_NORMALSTREAM_SUPPORT
				if (pTmpNormals)
					pTmpNormals[nCurVertex] = vNorm;
#endif
				newIndices.push_back(nCurVertex);
				nCurVertex++;
			}
			else
			{
				newIndices.push_back(nFind);
			}
		}

		indices.Clear();
		indices.AddList(newIndices);

		vertices.Clear();
		vertices.AddList(pTmpVerts, nCurVertex);

		tangents.Clear();
		tangents.AddList(pTmpTangents, nCurVertex);

#if ENABLE_NORMALSTREAM_SUPPORT
		if (pTmpNormals)
		{
			normals.Clear();
			normals.AddList(pTmpNormals, nCurVertex);

			delete[] pTmpNormals;
		}
#endif

		delete[] pTmpVerts;
		delete[] pTmpTangents;
	}

	static bool CompareMeshes(const CMesh& mesh1, const CMesh& mesh2);

	const char* GetLastError() const
	{
		return m_LastError;
	}

private:
#if CRY_PLATFORM_WINDOWS
	bool        CreateIndicesAndDeleteDuplicateVertices(CMesh& mesh);
	bool        StripifyMesh_Forsyth(CMesh& mesh);
	static bool CheckForDegenerateFaces(const CMesh& mesh);
	static void FindVertexRanges(CMesh& mesh);
#endif

private:
	struct SBasisFace
	{
		int v[3];
	};
	std::vector<const SMeshFace*> m_vhash_table[MAX_SUB_MATERIALS];
	std::vector<SBasisFace>       m_thash_table[MAX_SUB_MATERIALS];

	std::vector<int>*             m_pVertexMap;
	std::vector<int>*             m_pIndexMap;

	string                        m_LastError;
};

} // namespace mesh_compiler

#endif // __MeshCompiler_h__
