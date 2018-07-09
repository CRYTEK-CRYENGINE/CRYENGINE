// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class AbstractMeshElement
{
protected:
	virtual void GenMesh() = 0;
	int          GetMeshDataSize() const;

public:
	AbstractMeshElement() :
		m_meshDirty(true),
		m_vertexBuffer(~0u),
		m_indexBuffer(~0u)
	{}

	virtual ~AbstractMeshElement();

	void                          ValidateMesh();

	int                           GetVertexCount() const  { return m_vertices.size(); }
	int                           GetIndexCount()  const  { return m_indices.size();  }

	std::vector<SVF_P3F_C4B_T2F>& GetVertices()           { return m_vertices; }
	std::vector<uint16>&          GetIndices()            { return m_indices;  }

	buffer_handle_t               GetVertexBuffer() const { return m_vertexBuffer; }
	buffer_handle_t               GetIndexBuffer()  const { return m_indexBuffer;  }

	void                          MarkDirty()             { m_meshDirty = true; }

protected:
	std::vector<SVF_P3F_C4B_T2F> m_vertices;
	std::vector<uint16>          m_indices;
	bool                         m_meshDirty;

	buffer_handle_t              m_vertexBuffer;
	buffer_handle_t              m_indexBuffer;
};
