// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCacheMeshManager.cpp
//  Created:     18/1/2013 by Axel Gneiting
//  Description: Manages static meshes for geometry caches
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(USE_GEOM_CACHES)

	#include "GeomCacheMeshManager.h"
	#include "GeomCacheDecoder.h"

void CGeomCacheMeshManager::Reset()
{
	stl::free_container(m_meshMap);
}

bool CGeomCacheMeshManager::ReadMeshStaticData(CGeomCacheStreamReader& reader, const GeomCacheFile::SMeshInfo& meshInfo, SGeomCacheStaticMeshData& staticMeshData) const
{
	FUNCTION_PROFILER_3DENGINE;

	if (staticMeshData.m_constantStreams & GeomCacheFile::eStream_Indices)
	{
		if (!ReadMeshIndices(reader, meshInfo, staticMeshData, staticMeshData.m_indices))
		{
			return false;
		}
	}

	if ((staticMeshData.m_constantStreams & GeomCacheFile::eStream_Positions) != 0)
	{
		staticMeshData.m_positions.resize(staticMeshData.m_numVertices);

		strided_pointer<Vec3> positions(&staticMeshData.m_positions[0], sizeof(Vec3));
		if (!ReadMeshPositions(reader, meshInfo, positions))
		{
			return false;
		}
	}

	if ((staticMeshData.m_constantStreams & GeomCacheFile::eStream_Texcoords) != 0)
	{
		staticMeshData.m_texcoords.resize(staticMeshData.m_numVertices);

		strided_pointer<Vec2> texcoords(&staticMeshData.m_texcoords[0], sizeof(Vec2));
		if (!ReadMeshTexcoords(reader, meshInfo, texcoords))
		{
			return false;
		}
	}

	if ((staticMeshData.m_constantStreams & GeomCacheFile::eStream_QTangents) != 0)
	{
		staticMeshData.m_tangents.resize(staticMeshData.m_numVertices);

		strided_pointer<SPipTangents> tangents(&staticMeshData.m_tangents[0], sizeof(SPipTangents));
		if (!ReadMeshQTangents(reader, meshInfo, tangents))
		{
			return false;
		}
	}

	if ((staticMeshData.m_constantStreams & GeomCacheFile::eStream_Colors) != 0)
	{
		UCol defaultColor;
		defaultColor.dcolor = 0xFFFFFFFF;
		staticMeshData.m_colors.resize(staticMeshData.m_numVertices, defaultColor);

		strided_pointer<UCol> colors(&staticMeshData.m_colors[0], sizeof(UCol));
		if (!ReadMeshColors(reader, meshInfo, colors))
		{
			return false;
		}
	}

	if (staticMeshData.m_bUsePredictor)
	{
		uint32 predictorDataSize;
		if (!reader.Read(&predictorDataSize))
		{
			return false;
		}

		staticMeshData.m_predictorData.resize(predictorDataSize);
		if (!reader.Read(&staticMeshData.m_predictorData[0], predictorDataSize))
		{
			return false;
		}
	}

	return true;
}

_smart_ptr<IRenderMesh> CGeomCacheMeshManager::ConstructStaticRenderMesh(CGeomCacheStreamReader& reader,
                                                                         const GeomCacheFile::SMeshInfo& meshInfo, SGeomCacheStaticMeshData& staticMeshData, const char* pFileName)
{
	FUNCTION_PROFILER_3DENGINE;

	std::vector<char> vertexData(staticMeshData.m_numVertices * sizeof(SVF_P3F_C4B_T2F), 0);
	std::vector<SPipTangents> tangentData(staticMeshData.m_numVertices);

	std::vector<vtx_idx> indices;
	strided_pointer<Vec3> positions((Vec3*)(&vertexData[0] + offsetof(SVF_P3F_C4B_T2F, xyz)), sizeof(SVF_P3F_C4B_T2F));
	strided_pointer<UCol> colors((UCol*)(&vertexData[0] + offsetof(SVF_P3F_C4B_T2F, color)), sizeof(SVF_P3F_C4B_T2F));
	strided_pointer<Vec2> texcoords((Vec2*)(&vertexData[0] + offsetof(SVF_P3F_C4B_T2F, st)), sizeof(SVF_P3F_C4B_T2F));
	strided_pointer<SPipTangents> tangents((SPipTangents*)((char*)(&tangentData[0])), sizeof(SPipTangents));

	if (!ReadMeshIndices(reader, meshInfo, staticMeshData, indices)
	    || !ReadMeshPositions(reader, meshInfo, positions)
	    || !ReadMeshTexcoords(reader, meshInfo, texcoords)
	    || !ReadMeshQTangents(reader, meshInfo, tangents))
	{
		return NULL;
	}

	if (meshInfo.m_constantStreams & GeomCacheFile::eStream_Colors)
	{
		if (!ReadMeshColors(reader, meshInfo, colors))
		{
			return NULL;
		}
	}
	else
	{
		UCol defaultColor;
		defaultColor.dcolor = 0xFFFFFFFF;

		const uint numVertices = meshInfo.m_numVertices;
		for (uint i = 0; i < numVertices; ++i)
		{
			colors[i] = defaultColor;
		}
	}

	TMeshMap::iterator findIter = m_meshMap.find(staticMeshData.m_hash);
	if (findIter != m_meshMap.end())
	{
		++findIter->second.m_refCount;
		return findIter->second.m_pRenderMesh;
	}

	_smart_ptr<IRenderMesh> pRenderMesh = gEnv->pRenderer ?
	                                      gEnv->pRenderer->CreateRenderMeshInitialized(&vertexData[0], meshInfo.m_numVertices,
	                                                                                   EDefaultInputLayouts::P3F_C4B_T2F, &indices[0], indices.size(), prtTriangleList, "GeomCacheConstantMesh", pFileName, eRMT_Static, 1, 0,
	                                                                                   NULL, NULL, false, false, &tangentData[0]) : nullptr;

	CRenderChunk chunk;
	chunk.nNumVerts = meshInfo.m_numVertices;
	uint32 currentIndexOffset = 0;

	if (gEnv->pRenderer)
	{
		for (unsigned int i = 0; i < meshInfo.m_numMaterials; ++i)
		{
			chunk.nFirstIndexId = currentIndexOffset;
			chunk.nNumIndices = staticMeshData.m_numIndices[i];
			chunk.m_nMatID = staticMeshData.m_materialIds[i];
			pRenderMesh->SetChunk(i, chunk);
			currentIndexOffset += chunk.nNumIndices;
		}
	}

	SMeshMapInfo meshMapInfo;
	meshMapInfo.m_refCount = 1;
	meshMapInfo.m_pRenderMesh = pRenderMesh;

	m_meshMap[staticMeshData.m_hash] = meshMapInfo;
	return pRenderMesh;
}

_smart_ptr<IRenderMesh> CGeomCacheMeshManager::GetStaticRenderMesh(const uint64 hash) const
{
	TMeshMap::const_iterator findIter = m_meshMap.find(hash);
	if (findIter != m_meshMap.end())
	{
		return findIter->second.m_pRenderMesh;
	}

	return NULL;
}

void CGeomCacheMeshManager::RemoveReference(SGeomCacheStaticMeshData& staticMeshData)
{
	TMeshMap::iterator findIter = m_meshMap.find(staticMeshData.m_hash);
	if (findIter != m_meshMap.end())
	{
		uint& refCount = findIter->second.m_refCount;
		--refCount;

		if (refCount == 0)
		{
			m_meshMap.erase(findIter);
		}
	}
}

bool CGeomCacheMeshManager::ReadMeshIndices(CGeomCacheStreamReader& reader, const GeomCacheFile::SMeshInfo& meshInfo,
                                            SGeomCacheStaticMeshData& staticMeshData, std::vector<vtx_idx>& indices) const
{
	const uint16 numMaterials = meshInfo.m_numMaterials;
	staticMeshData.m_numIndices.reserve(numMaterials);

	for (unsigned int i = 0; i < numMaterials; ++i)
	{
		uint32 numIndices;
		if (!reader.Read(&numIndices))
		{
			return false;
		}

		staticMeshData.m_numIndices.push_back(numIndices);

		const uint indicesStart = indices.size();
		indices.resize(indicesStart + numIndices);
		if (!reader.Read(&indices[indicesStart], numIndices))
		{
			return false;
		}
	}

	return true;
}

bool CGeomCacheMeshManager::ReadMeshPositions(CGeomCacheStreamReader& reader, const GeomCacheFile::SMeshInfo& meshInfo, strided_pointer<Vec3> positions) const
{
	const Vec3 aabbMin = Vec3(meshInfo.m_aabbMin[0], meshInfo.m_aabbMin[1], meshInfo.m_aabbMin[2]);
	const Vec3 aabbMax = Vec3(meshInfo.m_aabbMax[0], meshInfo.m_aabbMax[1], meshInfo.m_aabbMax[2]);
	const AABB meshAABB(aabbMin, aabbMax);
	const Vec3 aabbSize = meshAABB.GetSize();
	const Vec3 posConvertFactor = Vec3(1.0f / float((2 << (meshInfo.m_positionPrecision[0] - 1)) - 1),
	                                   1.0f / float((2 << (meshInfo.m_positionPrecision[1] - 1)) - 1),
	                                   1.0f / float((2 << (meshInfo.m_positionPrecision[2] - 1)) - 1));

	const uint numVertices = meshInfo.m_numVertices;
	for (uint i = 0; i < numVertices; ++i)
	{
		GeomCacheFile::Position position;

		if (!reader.Read(&position))
		{
			return false;
		}

		positions[i] = GeomCacheDecoder::DecodePosition(aabbMin, aabbSize, position, posConvertFactor);
	}

	return true;
}

bool CGeomCacheMeshManager::ReadMeshTexcoords(CGeomCacheStreamReader& reader, const GeomCacheFile::SMeshInfo& meshInfo, strided_pointer<Vec2> texcoords) const
{
	const uint numVertices = meshInfo.m_numVertices;
	for (uint i = 0; i < numVertices; ++i)
	{
		GeomCacheFile::Texcoords texcoord;

		if (!reader.Read(&texcoord))
		{
			return false;
		}

		texcoords[i] = GeomCacheDecoder::DecodeTexcoord(texcoord);
	}

	return true;
}

bool CGeomCacheMeshManager::ReadMeshQTangents(CGeomCacheStreamReader& reader, const GeomCacheFile::SMeshInfo& meshInfo, strided_pointer<SPipTangents> tangents) const
{
	const uint numVertices = meshInfo.m_numVertices;
	for (uint i = 0; i < numVertices; ++i)
	{
		GeomCacheFile::QTangent qTangent;

		if (!reader.Read(&qTangent))
		{
			return false;
		}

		Quat qDecodedTangent = GeomCacheDecoder::DecodeQTangent(qTangent);
		GeomCacheDecoder::ConvertToTangentAndBitangent(qDecodedTangent, tangents[i]);
	}

	return true;
}

bool CGeomCacheMeshManager::ReadMeshColors(CGeomCacheStreamReader& reader, const GeomCacheFile::SMeshInfo& meshInfo, strided_pointer<UCol> colors) const
{
	const uint numVertices = meshInfo.m_numVertices;
	for (int colorIndex = 2; colorIndex >= 0; --colorIndex)
	{
		for (uint i = 0; i < numVertices; ++i)
		{
			GeomCacheFile::Color color;

			if (!reader.Read(&color))
			{
				return false;
			}

			colors[i].bcolor[colorIndex] = color;
		}
	}

	for (uint i = 0; i < numVertices; ++i)
	{
		GeomCacheFile::Color color;

		if (!reader.Read(&color))
		{
			return false;
		}

		colors[i].bcolor[3] = color;
	}

	return true;
}

#endif
