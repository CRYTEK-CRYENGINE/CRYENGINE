// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   CREClientPoly.cpp : implementation of 3D Client polygons RE.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "CREClientPoly.h"
#include "Common/RenderView.h"

#include "DriverD3D.h"

//////////////////////////////////////////////////////////////////////////
void CRenderPolygonDataPool::Clear()
{
	m_vertexCount = 0;
	m_vertices.resize(0);
	m_tangents.resize(0);
	m_indices.resize(0);

	if (m_vertexBuffer)
		CDeviceBufferManager::Instance()->Destroy(m_vertexBuffer);
	if (m_tangentsBuffer)
		CDeviceBufferManager::Instance()->Destroy(m_tangentsBuffer);
	if (m_indexBuffer)
		CDeviceBufferManager::Instance()->Destroy(m_indexBuffer);

	m_vertexBuffer = 0;
	m_tangentsBuffer = 0;
	m_indexBuffer = 0;
}

//////////////////////////////////////////////////////////////////////////
void CRenderPolygonDataPool::UpdateAPIBuffers()
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER)

	// Already allocated.
	if (m_vertexBuffer)
		return;

	// Update GPU buffers from CPU only temporary buffers
	int numVertices = m_vertices.size() / sizeof(VertexFormat);
	if (numVertices > 0)
	{
		m_vertexBuffer = CDeviceBufferManager::Instance()->Create(BBT_VERTEX_BUFFER, BU_STATIC, numVertices * sizeof(VertexFormat));
		CDeviceBufferManager::Instance()->UpdateBuffer(m_vertexBuffer, &m_vertices[0], CDeviceBufferManager::AlignBufferSizeForStreaming(numVertices * sizeof(VertexFormat)));

		assert(numVertices == m_tangents.size());

		m_tangentsBuffer = CDeviceBufferManager::Instance()->Create(BBT_VERTEX_BUFFER, BU_STATIC, numVertices * sizeof(SPipTangents));
		CDeviceBufferManager::Instance()->UpdateBuffer(m_tangentsBuffer, &m_tangents[0], CDeviceBufferManager::AlignBufferSizeForStreaming(numVertices * sizeof(SPipTangents)));
	}

	if (m_indices.size() > 0)
	{
		m_indexBuffer = CDeviceBufferManager::Instance()->Create(BBT_INDEX_BUFFER, BU_STATIC, m_indices.size() * sizeof(uint16));
		CDeviceBufferManager::Instance()->UpdateBuffer(m_indexBuffer, &m_indices[0], CDeviceBufferManager::AlignBufferSizeForStreaming(m_indices.size() * sizeof(uint16)));
	}
}

//////////////////////////////////////////////////////////////////////////

//===============================================================

CRenderElement* CREClientPoly::mfCopyConstruct(void)
{
	CREClientPoly* cp = new CREClientPoly;
	*cp = *this;
	return cp;
}

//////////////////////////////////////////////////////////////////////////
void CREClientPoly::AssignPolygon(const SRenderPolygonDescription& poly, const SRenderingPassInfo& passInfo, CRenderPolygonDataPool* pPolygonDataPool)
{
	auto& vertexPool = pPolygonDataPool->m_vertices;
	auto& indexPool = pPolygonDataPool->m_indices;
	auto& tangentsPool = pPolygonDataPool->m_tangents;

	m_pPolygonDataPool = pPolygonDataPool;

	m_Shader = poly.shaderItem;
	m_vertexCount = poly.numVertices;

	m_nCPFlags = 0;

	if (poly.afterWater)
		m_nCPFlags |= CREClientPoly::efAfterWater;
	if (passInfo.IsShadowPass())
		m_nCPFlags |= CREClientPoly::efShadowGen;

	auto& verts = poly.pVertices;
	auto& tangs = poly.pTangents;

	size_t nSize = CDeviceObjectFactory::GetInputLayoutDescriptor(EDefaultInputLayouts::P3F_C4B_T2F)->m_Strides[0] * poly.numVertices;
	int nOffs = vertexPool.size();
	vertexPool.resize(nOffs + nSize);
	SVF_P3F_C4B_T2F* vt = (SVF_P3F_C4B_T2F*)&vertexPool[nOffs];
	m_vertexOffset = nOffs;
	for (int i = 0; i < poly.numVertices; i++, vt++)
	{
		vt->xyz = verts[i].xyz;
		vt->st = verts[i].st;
		vt->color.dcolor = verts[i].color.dcolor;
	}

	m_tangentOffset = tangentsPool.size();
	tangentsPool.resize(tangentsPool.size() + poly.numVertices);

	if (tangs)
	{
		for (int i = 0; i < poly.numVertices; i++)
		{
			tangentsPool[m_tangentOffset + i] = tangs[i];
		}
	}
	else
	{
		m_tangentOffset = -1;
	}

	m_indexOffset = indexPool.size();

	int nBaseVertexOffset = pPolygonDataPool->m_vertexCount;

	if (poly.pIndices && poly.numIndices)
	{
		indexPool.resize(m_indexOffset + poly.numIndices);
		uint16* poolIndices = &indexPool[m_indexOffset];
		m_indexCount = poly.numIndices;
		for (int i = 0; i < poly.numIndices; ++i)
		{
			poolIndices[i] = nBaseVertexOffset + poly.pIndices[i];
		}
	}
	else
	{
		indexPool.resize(m_indexOffset + (poly.numVertices - 2) * 3);
		uint16* poolIndices = &indexPool[m_indexOffset];
		for (int i = 0; i < poly.numVertices - 2; ++i, poolIndices += 3)
		{
			poolIndices[0] = nBaseVertexOffset + 0;
			poolIndices[1] = nBaseVertexOffset + 1;
			poolIndices[2] = nBaseVertexOffset + i + 2;
		}
		m_indexCount = (poly.numVertices - 2) * 3;
	}

	pPolygonDataPool->m_vertexCount += poly.numVertices;
}

//////////////////////////////////////////////////////////////////////////
bool CREClientPoly::GetGeometryInfo(SGeometryInfo& geomInfo, bool bSupportTessellation)
{
	geomInfo.bonesRemapGUID = 0;

	geomInfo.primitiveType = eptTriangleList;
	geomInfo.eVertFormat = EDefaultInputLayouts::P3F_C4B_T2F;

	geomInfo.nFirstIndex = m_indexOffset;
	geomInfo.nNumIndices = m_indexCount;

	geomInfo.nFirstVertex = m_vertexOffset;
	geomInfo.nNumVertices = m_vertexCount;

	geomInfo.nNumVertexStreams = 2;

	geomInfo.indexStream.nStride = Index16;
	geomInfo.indexStream.hStream = m_pPolygonDataPool->m_indexBuffer;

	geomInfo.vertexStreams[VSF_GENERAL].nSlot = VSF_GENERAL;
	geomInfo.vertexStreams[VSF_GENERAL].nStride = CDeviceObjectFactory::GetInputLayoutDescriptor(EDefaultInputLayouts::P3F_C4B_T2F)->m_Strides[0];
	geomInfo.vertexStreams[VSF_GENERAL].hStream = m_pPolygonDataPool->m_vertexBuffer;

	geomInfo.vertexStreams[VSF_TANGENTS].nSlot = VSF_TANGENTS;
	geomInfo.vertexStreams[VSF_TANGENTS].nStride = CDeviceObjectFactory::GetInputLayoutDescriptor(EDefaultInputLayouts::T4S_B4S)->m_Strides[0];
	geomInfo.vertexStreams[VSF_TANGENTS].hStream = m_pPolygonDataPool->m_tangentsBuffer;

	return true;
}
