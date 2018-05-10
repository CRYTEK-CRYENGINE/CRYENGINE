// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IndexedMesh.h
//  Created:     28/5/2001 by Vladimir Kajalin
////////////////////////////////////////////////////////////////////////////

#ifndef IDX_MESH_H
#define IDX_MESH_H

#include <CryCore/Containers/CryArray.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <Cry3DEngine/IIndexedMesh.h>

class CIndexedMesh
	: public IIndexedMesh
	  , public CMesh
	  , public stl::intrusive_linked_list_node<CIndexedMesh>
	  , public Cry3DEngineBase
{
public:
	CIndexedMesh();
	virtual ~CIndexedMesh();

	//////////////////////////////////////////////////////////////////////////
	// IIndexedMesh
	//////////////////////////////////////////////////////////////////////////

	virtual void Release()
	{
		delete this;
	}

	// gives read-only access to mesh data
	virtual void GetMeshDescription(SMeshDescription& meshDesc) const
	{
		meshDesc.m_pFaces = m_pFaces;
		meshDesc.m_pVerts = m_pPositions;
		meshDesc.m_pVertsF16 = m_pPositionsF16;
		meshDesc.m_pNorms = m_pNorms;
		meshDesc.m_pColor = m_pColor0;
		meshDesc.m_pTexCoord = m_pTexCoord;
		meshDesc.m_pIndices = m_pIndices;
		meshDesc.m_nFaceCount = GetFaceCount();
		meshDesc.m_nVertCount = GetVertexCount();
		meshDesc.m_nCoorCount = GetTexCoordCount();
		meshDesc.m_nIndexCount = GetIndexCount();
	}

	virtual CMesh* GetMesh()
	{
		return this;
	}

	virtual void SetMesh(CMesh& mesh)
	{
		CopyFrom(mesh);
	}

	virtual void FreeStreams()
	{
		return CMesh::FreeStreams();
	}

	virtual int GetFaceCount() const
	{
		return CMesh::GetFaceCount();
	}

	virtual void SetFaceCount(int nNewCount)
	{
		CMesh::SetFaceCount(nNewCount);
	}

	virtual int GetVertexCount() const
	{
		return CMesh::GetVertexCount();
	}

	virtual void SetVertexCount(int nNewCount)
	{
		CMesh::SetVertexCount(nNewCount);
	}

	virtual void SetColorCount(int nNewCount)
	{
		CMesh::ReallocStream(COLORS_0, nNewCount);
	}

	virtual int GetTexCoordCount() const
	{
		return CMesh::GetTexCoordCount();
	}

	virtual void SetTexCoordCount(int nNewCount)
	{
		CMesh::ReallocStream(TEXCOORDS, nNewCount);
	}

	virtual int GetTangentCount() const
	{
		return CMesh::GetTangentCount();
	}

	virtual void SetTangentCount(int nNewCount)
	{
		CMesh::ReallocStream(TANGENTS, nNewCount);
	}

	virtual void SetTexCoordsAndTangentsCount(int nNewCount)
	{
		CMesh::SetTexCoordsAndTangentsCount(nNewCount);
	}

	virtual int GetIndexCount() const
	{
		return CMesh::GetIndexCount();
	}

	virtual void SetIndexCount(int nNewCount)
	{
		CMesh::SetIndexCount(nNewCount);
	}

	virtual void AllocateBoneMapping()
	{
		ReallocStream(BONEMAPPING, GetVertexCount());
	}

	virtual int GetSubSetCount() const
	{
		return m_subsets.size();
	}

	virtual void SetSubSetCount(int nSubsets)
	{
		m_subsets.resize(nSubsets);
	}

	virtual const SMeshSubset& GetSubSet(int nIndex) const
	{
		return m_subsets[nIndex];
	}

	virtual void SetSubsetBounds(int nIndex, const Vec3& vCenter, float fRadius)
	{
		m_subsets[nIndex].vCenter = vCenter;
		m_subsets[nIndex].fRadius = fRadius;
	}

	virtual void SetSubsetIndexVertexRanges(int nIndex, int nFirstIndexId, int nNumIndices, int nFirstVertId, int nNumVerts)
	{
		m_subsets[nIndex].nFirstIndexId = nFirstIndexId;
		m_subsets[nIndex].nNumIndices = nNumIndices;
		m_subsets[nIndex].nFirstVertId = nFirstVertId;
		m_subsets[nIndex].nNumVerts = nNumVerts;
	}

	virtual void SetSubsetMaterialId(int nIndex, int nMatID)
	{
		m_subsets[nIndex].nMatID = nMatID;
	}

	virtual void SetSubsetMaterialProperties(int nIndex, int nMatFlags, int nPhysicalizeType)
	{
		m_subsets[nIndex].nMatFlags = nMatFlags;
		m_subsets[nIndex].nPhysicalizeType = nPhysicalizeType;
	}

	virtual AABB GetBBox() const
	{
		return m_bbox;
	}

	virtual void SetBBox(const AABB& box)
	{
		m_bbox = box;
	}

	virtual void CalcBBox();

#if CRY_PLATFORM_WINDOWS
	virtual void Optimize(const char* szComment = NULL);
#endif

	virtual void RestoreFacesFromIndices();

	//////////////////////////////////////////////////////////////////////////

	void GetMemoryUsage(class ICrySizer* pSizer) const;
};

#endif // IDX_MESH_H
