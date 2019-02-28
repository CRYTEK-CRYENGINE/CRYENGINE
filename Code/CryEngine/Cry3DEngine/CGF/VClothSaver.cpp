// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VClothSaver.h"
#include "ChunkData.h"
#include "../../CryCommon/Cry3DEngine/CGF/CGFContent.h"

CSaverVCloth::CSaverVCloth(CChunkData& chunkData, const SVClothInfoCGF* pVClothInfo, bool swapEndian)
	: m_pChunkData(&chunkData),
	m_pVClothInfo(pVClothInfo),
	m_bSwapEndian(swapEndian)
{

}

void CSaverVCloth::WriteChunkHeader()
{
	if (!m_pVClothInfo) return;
	const int vertexCount = (int)m_pVClothInfo->m_vertices.size();
	VCLOTH_CHUNK chunk;
	ZeroStruct(chunk);

	chunk.vertexCount = vertexCount;
	chunk.bendTriangleCount = (int)m_pVClothInfo->m_triangles.size();
	chunk.bendTrianglePairCount = (int)m_pVClothInfo->m_trianglePairs.size();
	chunk.nndcNotAttachedOrderedIdxCount = (int)m_pVClothInfo->m_nndcNotAttachedOrderedIdx.size();
	chunk.linkCount[eVClothLink_Stretch] = (int)m_pVClothInfo->m_links[eVClothLink_Stretch].size();
	chunk.linkCount[eVClothLink_Shear] = (int)m_pVClothInfo->m_links[eVClothLink_Shear].size();
	chunk.linkCount[eVClothLink_Bend] = (int)m_pVClothInfo->m_links[eVClothLink_Bend].size();

	SwapEndian(chunk, m_bSwapEndian);
	m_pChunkData->Add(chunk);
}

void CSaverVCloth::WriteChunkVertices()
{
	if (!m_pVClothInfo) return;
	const int vertexCount = (int)m_pVClothInfo->m_vertices.size();
	DynArray<SVClothChunkVertex> chunkVertices;
	chunkVertices.resize(vertexCount);

	for (int vid = 0; vid < vertexCount; ++vid)
	{
		const SVClothVertex& vertex = m_pVClothInfo->m_vertices[vid];
		SVClothChunkVertex& chunkVertex = chunkVertices[vid];

		chunkVertex.attributes = vertex.attributes;
   }

   SwapEndian(chunkVertices.begin(), chunkVertices.size(), m_bSwapEndian);
   m_pChunkData->AddData(chunkVertices.begin(), chunkVertices.size_mem());
}

void CSaverVCloth::WriteTriangleData()
{
	if (!m_pVClothInfo) return;
	DynArray<char> buffer;
	buffer.resize(max(m_pVClothInfo->m_triangles.size_mem(), m_pVClothInfo->m_trianglePairs.size_mem()));

	memcpy(&buffer[0], m_pVClothInfo->m_triangles.begin(), m_pVClothInfo->m_triangles.size_mem());
	SwapEndian((SVClothBendTriangle*)buffer.begin(), m_pVClothInfo->m_triangles.size(), m_bSwapEndian);
	m_pChunkData->AddData(buffer.begin(), m_pVClothInfo->m_triangles.size_mem());

	memcpy(&buffer[0], m_pVClothInfo->m_trianglePairs.begin(), m_pVClothInfo->m_trianglePairs.size_mem());
	SwapEndian((SVClothBendTrianglePair*)buffer.begin(), m_pVClothInfo->m_trianglePairs.size(), m_bSwapEndian);
	m_pChunkData->AddData(buffer.begin(), m_pVClothInfo->m_trianglePairs.size_mem());
}

void CSaverVCloth::WriteNndcNotAttachedOrdered()
{
	if (!m_pVClothInfo) return;
	DynArray<char> bufferNndc;
	bufferNndc.resize(m_pVClothInfo->m_nndcNotAttachedOrderedIdx.size_mem());
	memcpy(&bufferNndc[0], m_pVClothInfo->m_nndcNotAttachedOrderedIdx.begin(), m_pVClothInfo->m_nndcNotAttachedOrderedIdx.size_mem());
	SwapEndian((SVClothNndcNotAttachedOrderedIdx*)bufferNndc.begin(), m_pVClothInfo->m_nndcNotAttachedOrderedIdx.size(), m_bSwapEndian);
	m_pChunkData->AddData(bufferNndc.begin(), m_pVClothInfo->m_nndcNotAttachedOrderedIdx.size_mem());
}

void CSaverVCloth::WriteLinks()
{
	if (!m_pVClothInfo) return;
	DynArray<SVClothLink> links;
	DynArray<char> buffer;

	for (int lid = 0; lid < eVClothLink_COUNT; ++lid)
	{
		links.clear();
		links.reserve(m_pVClothInfo->m_links[lid].size());

		for (int i = 0; i < m_pVClothInfo->m_links[lid].size(); ++i)
		{
			const SVClothLink& link = m_pVClothInfo->m_links[lid][i];
			links.insert(links.end(), link);
		}

		buffer.resize(links.size_mem());
		memcpy(&buffer[0], links.begin(), links.size_mem());
		SwapEndian((int*)buffer.begin(), links.size(), m_bSwapEndian);
		m_pChunkData->AddData(buffer.begin(), links.size_mem());
	}
}
