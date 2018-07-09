// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IVertexAnimation.h>

struct RChunk;

typedef uint16 SoftwareVertexBlendIndex;
typedef uint8  SoftwareVertexBlendWeight;

struct CRY_ALIGN(64) SSoftwareVertexFormat
{
	Vec3 position;
	uint32 color;
	Quat tangent;
	SoftwareVertexBlendWeight weights[8];
	SoftwareVertexBlendIndex indices[8];
	Vec2 coord;
};

struct SSoftwareVertexFrameFormat
{
	Vec3    position;
	vtx_idx index;
};

struct SSoftwareVertexFrame
{
	string name;
	DynArray<SSoftwareVertexFrameFormat> vertices;

	uint  index;
	float vertexMaxLength;
};

struct STangentUpdateTriangles
{
	float   t1, t2, r;
	vtx_idx idx1, idx2, idx3;
};

class CSoftwareVertexFrames :
	public IVertexFrames
{
public:
	bool                        Create(const CSkinningInfo& skinningInfo, const Vec3& positionOffset);

	uint                        GetCount() const                       { return uint(m_frames.size()); }
	uint                        GetVertexDeltasCount() const           { return m_numVertexDeltas; }

	const SSoftwareVertexFrame* GetFrames() const                      { return &m_frames[0]; }

	const char*                 GetNameByIndex(const uint index) const { return m_frames[index].name; }
	uint                        GetIndexByName(const char* name) const;

private:
	DynArray<SSoftwareVertexFrame> m_frames;
	uint                           m_numVertexDeltas;
};

class CSoftwareMesh
{
public:
	CSoftwareMesh();

public:
	bool                                             Create(const CMesh& mesh, const DynArray<RChunk>& renderChunks, const Vec3& positionOffset);
	bool                                             Create(IRenderMesh& renderMesh, const DynArray<RChunk>& renderChunks, const Vec3& positionOffset);
	bool                                             IsInitialized() const;

	const SSoftwareVertexFormat*                     GetVertices() const     { return m_vertices.begin(); }
	uint                                             GetVertexCount() const  { return uint(m_vertices.size()); }
	strided_pointer<const Vec3>                      GetPositions() const    { return strided_pointer<const Vec3>(&m_vertices[0].position, sizeof(m_vertices[0])); }
	strided_pointer<const uint32>                    GetColors() const       { return strided_pointer<const uint32>(&m_vertices[0].color, sizeof(m_vertices[0])); }
	strided_pointer<const Vec2>                      GetCoords() const       { return strided_pointer<const Vec2>(&m_vertices[0].coord, sizeof(m_vertices[0])); }
	strided_pointer<const Quat>                      GetTangents() const     { return strided_pointer<const Quat>(&m_vertices[0].tangent, sizeof(m_vertices[0])); }
	strided_pointer<const SoftwareVertexBlendIndex>  GetBlendIndices() const { return strided_pointer<const SoftwareVertexBlendIndex>(&m_vertices[0].indices[0], sizeof(m_vertices[0])); }
	strided_pointer<const SoftwareVertexBlendWeight> GetBlendWeights() const { return strided_pointer<const SoftwareVertexBlendWeight>(&m_vertices[0].weights[0], sizeof(m_vertices[0])); }
	uint                                             GetBlendCount() const   { return m_blendCount; }

	CSoftwareVertexFrames&                           GetVertexFrames()       { return m_vertexFrames; }
	const CSoftwareVertexFrames&   GetVertexFrames() const { return m_vertexFrames; }

	uint                           GetIndexCount() const   { return uint(m_indices.size()); }
	const vtx_idx*                 GetIndices() const      { return m_indices.begin(); }

	void                           GenerateTangentUpdateData(IRenderMesh& renderMesh);
	uint                           GetTangentUpdateDataCount() const   { return uint(m_precTangentData.size()); }
	uint                           GetTangentUpdateTriIdsCount() const { return uint(m_precTangentVertIds.size()); }
	const STangentUpdateTriangles* GetTangentUpdateData() const        { return m_precTangentData.begin(); }
	const vtx_idx*                 GetTangentUpdateVertIds() const     { return m_precTangentVertIds.begin(); }

private:
	ILINE void                                 AllocateVertices(const uint count);
	ILINE void                                 AllocateIndices(const uint count);

	strided_pointer<Vec3>                      GetWritePositions()    { return strided_pointer<Vec3>(&m_vertices[0].position, sizeof(m_vertices[0])); }
	strided_pointer<uint32>                    GetWriteColors()       { return strided_pointer<uint32>(&m_vertices[0].color, sizeof(m_vertices[0])); }
	strided_pointer<Vec2>                      GetWriteCoords()       { return strided_pointer<Vec2>(&m_vertices[0].coord, sizeof(m_vertices[0])); }
	strided_pointer<Quat>                      GetWriteTangents()     { return strided_pointer<Quat>(&m_vertices[0].tangent, sizeof(m_vertices[0])); }
	strided_pointer<SoftwareVertexBlendIndex>  GetWriteBlendIndices() { return strided_pointer<SoftwareVertexBlendIndex>(&m_vertices[0].indices[0], sizeof(m_vertices[0])); }
	strided_pointer<SoftwareVertexBlendWeight> GetWriteBlendWeights() { return strided_pointer<SoftwareVertexBlendWeight>(&m_vertices[0].weights[0], sizeof(m_vertices[0])); }

private:
	DynArray<SSoftwareVertexFormat>   m_vertices;
	uint                              m_blendCount;

	CSoftwareVertexFrames             m_vertexFrames;

	DynArray<vtx_idx>                 m_indices;

	DynArray<STangentUpdateTriangles> m_precTangentData;
	DynArray<vtx_idx>                 m_precTangentVertIds;
};

class CVertexData
{
public:
	CVertexData() :
		m_vertexCount(0),
		pIndices(0),
		m_indexCount(0)
	{
	}

public:
	const uint                    GetVertexCount() const { return m_vertexCount; }

	strided_pointer<Vec3>         GetPositions()         { return pPositions; }
	strided_pointer<uint32>       GetColors()            { return pColors; }
	strided_pointer<Vec2>         GetCoords()            { return pCoords; }
	strided_pointer<SPipTangents> GetTangents()          { return pTangents; }
	strided_pointer<Vec3>         GetVelocities()        { return pVelocities; }

	const uint                    GetIndexCount() const  { return m_indexCount; }

	vtx_idx*                      GetIndices()           { return pIndices; }

public:
	strided_pointer<Vec3>         pPositions;
	strided_pointer<uint32>       pColors;
	strided_pointer<Vec2>         pCoords;
	strided_pointer<SPipTangents> pTangents;
	strided_pointer<Vec3>         pVelocities;

	uint                          m_vertexCount;

	vtx_idx*                      pIndices;
	uint                          m_indexCount;
};

/*
   CSoftwareMesh
 */

ILINE void CSoftwareMesh::AllocateVertices(const uint count)
{
	m_vertices.resize(count);
}

ILINE void CSoftwareMesh::AllocateIndices(const uint count)
{
	m_indices.resize(count);
}
