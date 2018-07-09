// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TriMesh_h__
#define __TriMesh_h__
#pragma once

#include <Cry3DEngine/IIndexedMesh.h>
#include "Util/bitarray.h"

struct SSubObjSelOptions;

typedef std::vector<int> MeshElementsArray;

//////////////////////////////////////////////////////////////////////////
// Vertex used in the TriMesh.
//////////////////////////////////////////////////////////////////////////
struct CTriVertex
{
	Vec3 pos;
	//float weight;    // Selection weight in 0-1 range.
};

//////////////////////////////////////////////////////////////////////////
// Triangle face used by the Triangle mesh.
//////////////////////////////////////////////////////////////////////////
struct CTriFace
{
	uint32        v[3];    // Indices to vertices array.
	uint32        uv[3];   // Indices to texture coordinates array.
	Vec3          n[3];    // Vertex normals at face vertices.
	Vec3          normal;  // Face normal.
	uint32        edge[3]; // Indices to the face edges.
	unsigned char MatID;   // Index of face sub material.
	unsigned char flags;   // see ETriMeshFlags
};

//////////////////////////////////////////////////////////////////////////
// Mesh edge.
//////////////////////////////////////////////////////////////////////////
struct CTriEdge
{
	uint32 v[2];    // Indices to edge vertices.
	int    face[2]; // Indices to edge faces (-1 if no face).
	uint32 flags;   // see ETriMeshFlags

	CTriEdge() {}
	bool operator==(const CTriEdge& edge) const
	{
		if ((v[0] == edge.v[0] && v[1] == edge.v[1]) ||
		    (v[0] == edge.v[1] && v[1] == edge.v[0]))
			return true;
		return false;
	}
	bool operator!=(const CTriEdge& edge) const { return !(*this == edge); }
	bool operator<(const CTriEdge& edge) const  { return (*(uint64*)v < *(uint64*)edge.v); }
	bool operator>(const CTriEdge& edge) const  { return (*(uint64*)v > *(uint64*)edge.v); }
};

//////////////////////////////////////////////////////////////////////////
// Mesh line.
//////////////////////////////////////////////////////////////////////////
struct CTriLine
{
	uint32 v[2];   // Indices to edge vertices.

	CTriLine() {}
	bool operator==(const CTriLine& edge) const
	{
		if ((v[0] == edge.v[0] && v[1] == edge.v[1]) ||
		    (v[0] == edge.v[1] && v[1] == edge.v[0]))
			return true;
		return false;
	}
	bool operator!=(const CTriLine& edge) const { return !(*this == edge); }
	bool operator<(const CTriLine& edge) const  { return (*(uint64*)v < *(uint64*)edge.v); }
	bool operator>(const CTriLine& edge) const  { return (*(uint64*)v > *(uint64*)edge.v); }
};

//////////////////////////////////////////////////////////////////////////
struct CTriMeshPoly
{
	std::vector<uint32> v;       // Indices to vertices array.
	std::vector<uint32> uv;      // Indices to texture coordinates array.
	std::vector<Vec3>   n;       // Vertex normals at face vertices.
	Vec3                normal;  // Polygon normal.
	uint32              edge[3]; // Indices to the face edges.
	unsigned char       MatID;   // Index of face sub material.
	unsigned char       flags;   // optional flags.
};

//////////////////////////////////////////////////////////////////////////
// CTriMesh is used in the Editor as a general purpose editable triangle mesh.
//////////////////////////////////////////////////////////////////////////
class CTriMesh
{
public:
	enum EStream
	{
		VERTICES,
		FACES,
		EDGES,
		TEXCOORDS,
		COLORS,
		WEIGHTS,
		LINES,
		WS_POSITIONS,
		LAST_STREAM,
	};
	enum ECopyFlags
	{
		COPY_VERTICES  = BIT(1),
		COPY_FACES     = BIT(2),
		COPY_EDGES     = BIT(3),
		COPY_TEXCOORDS = BIT(4),
		COPY_COLORS    = BIT(5),
		COPY_VERT_SEL  = BIT(6),
		COPY_EDGE_SEL  = BIT(7),
		COPY_FACE_SEL  = BIT(8),
		COPY_WEIGHTS   = BIT(9),
		COPY_LINES     = BIT(10),
		COPY_ALL       = 0xFFFF,
	};
	// geometry data
	CTriFace*      pFaces;
	CTriEdge*      pEdges;
	CTriVertex*    pVertices;
	SMeshTexCoord* pUV;
	SMeshColor*    pColors;     // If allocated same size as pVerts array.
	Vec3*          pWSVertices; // World space vertices.
	float*         pWeights;
	CTriLine*      pLines;

	int            nFacesCount;
	int            nVertCount;
	int            nUVCount;
	int            nEdgeCount;
	int            nLinesCount;

	AABB           bbox;

	//////////////////////////////////////////////////////////////////////////
	// Selections.
	//////////////////////////////////////////////////////////////////////////
	CBitArray vertSel;
	CBitArray edgeSel;
	CBitArray faceSel;
	// Every bit of the selection mask correspond to a stream, if bit is set this stream have some elements selected
	int       streamSelMask;

	// Selection element type.
	// see ESubObjElementType
	int selectionType;

	//////////////////////////////////////////////////////////////////////////
	// Vertices of the front facing triangles.
	CBitArray frontFacingVerts;

	//////////////////////////////////////////////////////////////////////////
	// Functions.
	//////////////////////////////////////////////////////////////////////////
	CTriMesh();
	~CTriMesh();

	int GetFacesCount() const  { return nFacesCount; }
	int GetVertexCount() const { return nVertCount; }
	int GetUVCount() const     { return nUVCount; }
	int GetEdgeCount() const   { return nEdgeCount; }
	int GetLinesCount() const  { return nLinesCount; }

	//////////////////////////////////////////////////////////////////////////
	void SetFacesCount(int nNewCount) { ReallocStream(FACES, nNewCount); }
	void SetVertexCount(int nNewCount)
	{
		ReallocStream(VERTICES, nNewCount);
		if (pColors)
			ReallocStream(COLORS, nNewCount);
		ReallocStream(WEIGHTS, nNewCount);
	}
	void SetColorsCount(int nNewCount) { ReallocStream(COLORS, nNewCount); }
	void SetUVCount(int nNewCount)     { ReallocStream(TEXCOORDS, nNewCount); }
	void SetEdgeCount(int nNewCount)   { ReallocStream(EDGES, nNewCount); }
	void SetLinesCount(int nNewCount)  { ReallocStream(LINES, nNewCount); }

	void ReallocStream(int stream, int nNewCount);
	void GetStreamInfo(int stream, void*& pStream, int& nElementSize) const;
	int  GetStreamSize(int stream) const { return m_streamSize[stream]; };

	void SetFromMesh(CMesh& mesh);
	void UpdateIndexedMesh(IIndexedMesh* pIndexedMesh) const;
	// Calculate per face normal.
	void CalcFaceNormals();

	//////////////////////////////////////////////////////////////////////////
	// Welding functions.
	//////////////////////////////////////////////////////////////////////////
	void SharePositions();
	void ShareUV();
	//////////////////////////////////////////////////////////////////////////
	// Recreate edges of the mesh.
	void UpdateEdges();

	void Copy(CTriMesh& fromMesh, int nCopyFlags = COPY_ALL);

	//////////////////////////////////////////////////////////////////////////
	// Sub-object selection specific methods.
	//////////////////////////////////////////////////////////////////////////
	// Return true if something is selected.
	bool       UpdateSelection();
	// Clear all selections, return true if something was selected.
	bool       ClearSelection();
	void       SoftSelection(const SSubObjSelOptions& options);
	CBitArray* GetStreamSelection(int nStream)  { return m_streamSel[nStream]; };
	// Returns true if specified stream have any selected elements.
	bool       StreamHaveSelection(int nStream) { return (streamSelMask & (1 << nStream)) != 0; }
	void       GetEdgesByVertex(MeshElementsArray& inVertices, MeshElementsArray& outEdges);
	void       GetFacesByVertex(MeshElementsArray& inVertices, MeshElementsArray& outFaces);

private:
	void* ReAllocElements(void* old_ptr, int new_elem_num, int size_of_element);
	void  CopyStream(CTriMesh& fromMesh, int stream);

	// For internal use.
	int        m_streamSize[LAST_STREAM];
	CBitArray* m_streamSel[LAST_STREAM];
};

#endif // __TriMesh_h__

