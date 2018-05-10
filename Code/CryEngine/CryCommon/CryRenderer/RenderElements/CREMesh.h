// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CREMESH_H__
#define __CREMESH_H__

class CREMesh : public CRenderElement
{
public:

	struct CRenderChunk* m_pChunk;
	class CRenderMesh*   m_pRenderMesh;

	//! Copy of Chunk to avoid indirections.
	int32  m_nFirstIndexId;
	int32  m_nNumIndices;

	uint32 m_nFirstVertId;
	uint32 m_nNumVerts;

protected:
	CREMesh()
	{
		mfSetType(eDATA_Mesh);
		mfUpdateFlags(FCEF_TRANSFORM);

		m_pChunk = NULL;
		m_pRenderMesh = NULL;
		m_nFirstIndexId = -1;
		m_nNumIndices = -1;
		m_nFirstVertId = 0;
		m_nNumVerts = 0;
	}

	virtual ~CREMesh();

	// Ideally should be declared and left unimplemented to prevent slicing at compile time
	// but this would prevent auto code gen in renderer later on.
	// To track potential slicing, uncomment the following (and their equivalent in CREMeshImpl)
	//CREMesh(CREMesh&);
	//CREMesh& operator=(CREMesh& rhs);
};

#endif // __CREMESH_H__
