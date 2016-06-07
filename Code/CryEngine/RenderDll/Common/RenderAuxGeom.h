// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _RENDERAUXGEOM_H_
#define _RENDERAUXGEOM_H_

#if defined(ENABLE_PROFILING_CODE) || !defined(_RELEASE)
	#define ENABLE_RENDER_AUX_GEOM
#endif

#if defined(ENABLE_RENDER_AUX_GEOM)

	#include <CryRenderer/IRenderAuxGeom.h>
	#include <CryRenderer/VertexFormats.h>
	#include <CryMemory/CrySizer.h>

class ICrySizer;
class CAuxGeomCB;
struct SAuxGeomCBRawDataPackaged;

struct IRenderAuxGeomImpl
{
public:
	virtual ~IRenderAuxGeomImpl(){}
	virtual void Flush(SAuxGeomCBRawDataPackaged& data, size_t begin, size_t end, bool reset = false) = 0;
	virtual void RT_Flush(SAuxGeomCBRawDataPackaged& data, size_t begin, size_t end, bool reset = false) = 0;

	virtual void FlushTextMessages(CTextMessages& tMessages, bool reset) = 0;
};

class CAuxGeomCB : public IRenderAuxGeom
{
public:
	// interface
	virtual void                SetRenderFlags(const SAuxGeomRenderFlags& renderFlags);
	virtual SAuxGeomRenderFlags GetRenderFlags();

	virtual void                DrawPoint(const Vec3& v, const ColorB& col, uint8 size = 1);
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size = 1);
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size = 1);

	virtual void                DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness = 1.0f);
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f);
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f);
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f);
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f);
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness = 1.0f);
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness = 1.0f);

	virtual void                DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2);
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col);
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col);
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col);
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col);

	virtual void                DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle);
	virtual void                DrawAABBs(const AABB* aabbs, uint32 aabbCount, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle);
	virtual void                DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle);

	virtual void                DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle);
	virtual void                DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle);

	virtual void                DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded = true);
	virtual void                DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true);
	virtual void                DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true);

	virtual void                DrawBone(const Vec3& rParent, const Vec3& rBone, ColorB col);

	virtual void                RenderText(Vec3 pos, SDrawTextInfo& ti, const char* format, va_list args);

	virtual int                 PushMatrix(const Matrix34&  mat);
	virtual Matrix34*           GetMatrix();
	virtual void                SetMatrixIndex(int matID);

	void                        Flush(bool reset);
	virtual void                Flush();
	virtual void                Commit(uint frames = 0);
	virtual void                Process();

public:
	enum EPrimType
	{
		e_PtList,
		e_LineList,
		e_LineListInd,
		e_TriList,
		e_TriListInd,
		e_Obj,

		e_NumPrimTypes,

		e_PrimTypeInvalid
	};

	enum EAuxDrawObjType
	{
		eDOT_Sphere,
		eDOT_Cone,
		eDOT_Cylinder
	};

	struct SAuxDrawObjParams
	{
		SAuxDrawObjParams()
		{
			m_matWorld.SetIdentity();
			m_color = 0;
			m_size = 0;
			m_shaded = false;
		}

		SAuxDrawObjParams(const Matrix34& matWorld, const uint32& color, float size, bool shaded)
			: m_matWorld(matWorld)
			, m_color(color)
			, m_size(size)
			, m_shaded(shaded)
		{
		}

		Matrix34 m_matWorld;
		uint32   m_color;
		float    m_size;
		bool     m_shaded;
	};

	struct SAuxPushBufferEntry
	{
		SAuxPushBufferEntry()
		{
		}

		SAuxPushBufferEntry(uint32 numVertices, uint32 numIndices, uint32 vertexOffs, uint32 indexOffs, uint32 transMatrixIdx, int worldMatrixIdx, const SAuxGeomRenderFlags& renderFlags)
			: m_numVertices(numVertices)
			, m_numIndices(numIndices)
			, m_vertexOffs(vertexOffs)
			, m_indexOffs(indexOffs)
			, m_transMatrixIdx(transMatrixIdx)
			, m_worldMatrixIdx(worldMatrixIdx)
			, m_renderFlags(renderFlags)
		{
		}

		SAuxPushBufferEntry(uint32 drawParamOffs, uint32 transMatrixIdx, int worldMatrixIdx, const SAuxGeomRenderFlags& renderFlags)
			: m_numVertices(0)
			, m_numIndices(0)
			, m_vertexOffs(drawParamOffs)
			, m_indexOffs(0)
			, m_transMatrixIdx(transMatrixIdx)
			, m_worldMatrixIdx(worldMatrixIdx)
			, m_renderFlags(renderFlags)
		{
			assert(e_Obj == GetPrimType(m_renderFlags));
		}

		bool GetDrawParamOffs(uint32& drawParamOffs) const
		{
			if (e_Obj == GetPrimType(m_renderFlags))
			{
				drawParamOffs = m_vertexOffs;
				return(true);
			}
			return(false);
		}

		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(this, sizeof(*this));
		}
		uint32              m_numVertices;
		uint32              m_numIndices;
		uint32              m_vertexOffs;
		uint32              m_indexOffs;
		int                 m_transMatrixIdx;
		int                 m_worldMatrixIdx;
		SAuxGeomRenderFlags m_renderFlags;
	};

	typedef std::vector<SAuxPushBufferEntry>        AuxPushBuffer;
	typedef std::vector<const SAuxPushBufferEntry*> AuxSortedPushBuffer;
	typedef std::vector<SAuxVertex>                 AuxVertexBuffer;
	typedef std::vector<vtx_idx>                    AuxIndexBuffer;
	typedef std::vector<SAuxDrawObjParams>          AuxDrawObjParamBuffer;
	typedef stl::aligned_vector<Matrix44, 16>       AuxOrthoMatrixBuffer;
	typedef std::deque<Matrix34>                    AuxWorldMatrixBuffer;

	struct SAuxGeomCBRawData
	{
	public:
		SAuxGeomCBRawData() 
			: m_isUsed(false)
			, m_curRenderFlags(e_Def3DPublicRenderflags)
			, m_curTransMatIdx(-1)
			, m_curWorldMatIdx(-1)
			, m_uCount(0) 
		{}

		void GetSortedPushBuffer(size_t begin, size_t end, AuxSortedPushBuffer& auxSortedPushBuffer) const;
		void GetMemoryUsage(ICrySizer* pSizer) const;

		void Reset()
		{
			m_auxPushBuffer.resize(0);
			m_auxVertexBuffer.resize(0);
			m_auxIndexBuffer.resize(0);
			m_auxDrawObjParamBuffer.resize(0);
			m_auxOrthoMatrices.resize(0);
			m_auxWorldMatrices.resize(0);
			m_TextMessages.Clear();

			m_curRenderFlags = e_Def3DPublicRenderflags;
			m_curTransMatIdx = -1;
			m_curWorldMatIdx = -1;
			m_uCount = 0;
		}

		bool IsUsed()
		{
			return m_isUsed;
		}

		void SetUsed(bool used)
		{
			m_isUsed = used;
		}

		void SetCount(uint count)
		{
			m_uCount = count;
		}

		uint Count()
		{
			return m_uCount > 0 ? m_uCount-- : m_uCount;
		}

	public:
		AuxPushBuffer         m_auxPushBuffer;
		AuxVertexBuffer       m_auxVertexBuffer;
		AuxIndexBuffer        m_auxIndexBuffer;
		AuxDrawObjParamBuffer m_auxDrawObjParamBuffer;
		AuxOrthoMatrixBuffer  m_auxOrthoMatrices;
		AuxWorldMatrixBuffer  m_auxWorldMatrices;
		CTextMessages         m_TextMessages;
		SAuxGeomRenderFlags   m_curRenderFlags;
		int                   m_curTransMatIdx;
		int                   m_curWorldMatIdx;
		uint                  m_uCount;
		bool                  m_isUsed;
	};

public:
	// c/dtor
	CAuxGeomCB(IRenderAuxGeomImpl* pRenderAuxGeom);
	virtual ~CAuxGeomCB();

	// get methods for private flags
	static EPrimType       GetPrimType(const SAuxGeomRenderFlags& renderFlags);
	static bool            IsThickLine(const SAuxGeomRenderFlags& renderFlags);
	static EAuxDrawObjType GetAuxObjType(const SAuxGeomRenderFlags& renderFlags);
	static uint8           GetPointSize(const SAuxGeomRenderFlags& renderFlags);

	// memory usage
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
		for (CBList::const_iterator it = m_cbData.begin(); it != m_cbData.end(); ++it)
			(*it)->GetMemoryUsage(pSizer);
	}

	void FreeMemory()
	{
		for (CBList::iterator it = m_cbData.begin(); it != m_cbData.end(); ++it)
			stl::reconstruct(**it);
	}

	// setting orthogonal projection
	void SetOrthoMode(bool enable, Matrix44A* pMatrix = 0)
	{
		if (enable)
		{
			assert(pMatrix);
			m_cbCurrent->m_curTransMatIdx = m_cbCurrent->m_auxOrthoMatrices.size();
			m_cbCurrent->m_auxOrthoMatrices.push_back(*pMatrix);
		}
		else
			m_cbCurrent->m_curTransMatIdx = -1;
	}

private:
	enum EAuxGeomPrivateRenderflagBitMasks
	{
		// public field starts at bit 22

		e_PrimTypeShift          = 19,
		e_PrimTypeMask           = 0x7 << e_PrimTypeShift,

		e_PrivateRenderflagsMask = (1 << 19) - 1
	};

	enum EAuxGeomPrivateRenderflags
	{
		// for non-indexed triangles
		e_TriListParam_ProcessThickLines = 0x00000001,

		// for triangles

		// for lines

		// for points

		// for objects
	};

private:
	uint32 CreatePointRenderFlags(uint8 size);
	uint32 CreateLineRenderFlags(bool indexed);
	uint32 CreateTriangleRenderFlags(bool indexed);
	uint32 CreateObjectRenderFlags(const EAuxDrawObjType& objType);

	void   DrawThickLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness);

	void   AddPushBufferEntry(uint32 numVertices, uint32 numIndices, const SAuxGeomRenderFlags& renderFlags);
	void   AddPrimitive(SAuxVertex*& pVertices, uint32 numVertices, const SAuxGeomRenderFlags& renderFlags);
	void   AddIndexedPrimitive(SAuxVertex*& pVertices, uint32 numVertices, vtx_idx*& pIndices, uint32 numIndices, const SAuxGeomRenderFlags& renderFlags);
	void   AddObject(SAuxDrawObjParams*& pDrawParams, const SAuxGeomRenderFlags& renderFlags);

protected:
	size_t GetLastFlushPos() const
	{
		return m_lastFlushPos;
	}

	size_t GetCurFlushPos() const
	{
		return m_cbCurrent->m_auxPushBuffer.size();
	}

	void UpdateLastFlushPos()
	{
		m_lastFlushPos = GetCurFlushPos();
	}

	int GetTransMatrixIndex() const
	{
		return m_cbCurrent->m_curTransMatIdx;
	}

	int GetWorldMatrixIndex() const
	{
		return m_cbCurrent->m_curWorldMatIdx;
	}

	SAuxGeomCBRawData* AccessData()
	{
		return m_cbCurrent;
	}

protected:
	struct PushBufferSortFunc
	{
		bool operator()(const SAuxPushBufferEntry* lhs, const SAuxPushBufferEntry* rhs) const
		{
			if (lhs->m_renderFlags.m_renderFlags != rhs->m_renderFlags.m_renderFlags)
				return lhs->m_renderFlags.m_renderFlags < rhs->m_renderFlags.m_renderFlags;

			if (lhs->m_worldMatrixIdx != rhs->m_worldMatrixIdx)
				return lhs->m_worldMatrixIdx < rhs->m_worldMatrixIdx;

			return lhs->m_transMatrixIdx < rhs->m_transMatrixIdx;
		}
	};

	IRenderAuxGeomImpl* m_pRenderAuxGeom;

	size_t              m_lastFlushPos;

	typedef std::list<SAuxGeomCBRawData*> CBList;
	CBList             m_cbData;

	SAuxGeomCBRawData* m_cbCurrent;

	SAuxGeomCBRawData* AddCBuffer()
	{
		SAuxGeomCBRawData* ptr = new SAuxGeomCBRawData;
		m_cbData.push_back(ptr);
		return ptr;
	}
};

class CAuxGeomCBMainThread : public CAuxGeomCB
{
protected:
	SAuxGeomCBRawData* volatile m_CBReady;

public:
	CAuxGeomCBMainThread(IRenderAuxGeomImpl* pRenderAuxGeom) : CAuxGeomCB(pRenderAuxGeom), m_CBReady(0) {}
	virtual void Commit(uint frames = 0);
	virtual void Process();
};

class CAuxGeomCBWorkerThread : public CAuxGeomCBMainThread
{
	SAuxGeomCBRawData* m_cbProcessed;

public:
	CAuxGeomCBWorkerThread(IRenderAuxGeomImpl* pRenderAuxGeom) : CAuxGeomCBMainThread(pRenderAuxGeom), m_cbProcessed(0) {}
	virtual void Flush();
	virtual void Commit(uint frames = 0);
	virtual void Process();
};

// package CAuxGeomCB::SAuxGeomCBRawData ptr via seperate struct as nested types cannot be forward declared
struct SAuxGeomCBRawDataPackaged
{
	SAuxGeomCBRawDataPackaged(CAuxGeomCB::SAuxGeomCBRawData* pData)
		: m_pData(pData)
	{
		assert(m_pData);
	}

	CAuxGeomCB::SAuxGeomCBRawData* m_pData;
};

inline uint32 CAuxGeomCB::CreatePointRenderFlags(uint8 size)
{
	return(m_cbCurrent->m_curRenderFlags.m_renderFlags | (e_PtList << e_PrimTypeShift) | size);
}

inline uint32 CAuxGeomCB::CreateLineRenderFlags(bool indexed)
{
	if (false != indexed)
	{
		return(m_cbCurrent->m_curRenderFlags.m_renderFlags | (e_LineListInd << e_PrimTypeShift));
	}
	else
	{
		return(m_cbCurrent->m_curRenderFlags.m_renderFlags | (e_LineList << e_PrimTypeShift));
	}
}

inline uint32 CAuxGeomCB::CreateTriangleRenderFlags(bool indexed)
{
	if (false != indexed)
	{
		return(m_cbCurrent->m_curRenderFlags.m_renderFlags | (e_TriListInd << e_PrimTypeShift));
	}
	else
	{
		return(m_cbCurrent->m_curRenderFlags.m_renderFlags | (e_TriList << e_PrimTypeShift));
	}
}

inline uint32 CAuxGeomCB::CreateObjectRenderFlags(const EAuxDrawObjType& objType)
{
	return(m_cbCurrent->m_curRenderFlags.m_renderFlags | (e_Obj << e_PrimTypeShift) | objType);
}

inline CAuxGeomCB::EPrimType CAuxGeomCB::GetPrimType(const SAuxGeomRenderFlags& renderFlags)
{
	uint32 primType((renderFlags.m_renderFlags & e_PrimTypeMask) >> e_PrimTypeShift);
	switch (primType)
	{
	case e_PtList:
		{
			return(e_PtList);
		}
	case e_LineList:
		{
			return(e_LineList);
		}
	case e_LineListInd:
		{
			return(e_LineListInd);
		}
	case e_TriList:
		{
			return(e_TriList);
		}
	case e_TriListInd:
		{
			return(e_TriListInd);
		}
	case e_Obj:
	default:
		{
			assert(e_Obj == primType);
			return(e_Obj);
		}
	}
}

inline bool CAuxGeomCB::IsThickLine(const SAuxGeomRenderFlags& renderFlags)
{
	EPrimType primType(GetPrimType(renderFlags));
	assert(e_TriList == primType);

	if (e_TriList == primType)
	{
		return(0 != (renderFlags.m_renderFlags & e_TriListParam_ProcessThickLines));
	}
	else
	{
		return(false);
	}
}

inline CAuxGeomCB::EAuxDrawObjType CAuxGeomCB::GetAuxObjType(const SAuxGeomRenderFlags& renderFlags)
{
	EPrimType primType(GetPrimType(renderFlags));
	assert(e_Obj == primType);

	uint32 objType((renderFlags.m_renderFlags & e_PrivateRenderflagsMask));
	switch (objType)
	{
	case eDOT_Sphere:
	default:
		{
			assert(eDOT_Sphere == objType);
			return(eDOT_Sphere);
		}
	case eDOT_Cone:
		{
			assert(eDOT_Cone == objType);
			return(eDOT_Cone);
		}
	case eDOT_Cylinder:
		{
			assert(eDOT_Cylinder == objType);
			return(eDOT_Cylinder);
		}
	}
}

inline uint8 CAuxGeomCB::GetPointSize(const SAuxGeomRenderFlags& renderFlags)
{
	EPrimType primType(GetPrimType(renderFlags));
	assert(e_PtList == primType);

	if (e_PtList == primType)
	{
		return(renderFlags.m_renderFlags & e_PrivateRenderflagsMask);
	}
	else
	{
		return(0);
	}
}

#endif // #if defined(ENABLE_RENDER_AUX_GEOM)

class CAuxGeomCB_Null : public IRenderAuxGeom
{
public:
	// interface
	virtual void                SetRenderFlags(const SAuxGeomRenderFlags& renderFlags)                                                                         {}
	virtual SAuxGeomRenderFlags GetRenderFlags()                                                                                                               { return SAuxGeomRenderFlags(); }

	virtual void                DrawPoint(const Vec3& v, const ColorB& col, uint8 size = 1)                                                                    {}
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size = 1)                                                 {}
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size = 1)                                                 {}

	virtual void                DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness = 1.0f)                     {}
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f)                                          {}
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f)                                          {}
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f)   {}
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f)   {}
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness = 1.0f)                          {}
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness = 1.0f)                          {}

	virtual void                DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2)    {}
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col)                                                              {}
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col)                                                              {}
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col)                       {}
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col)                       {}

	virtual void                DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)                           {}
	virtual void                DrawAABBs(const AABB* aabbs, uint32 aabbCount, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)       {}
	virtual void                DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) {}

	virtual void                DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)             {}
	virtual void                DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)    {}

	virtual void                DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded = true)                                           {}
	virtual void                DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true)              {}
	virtual void                DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true)          {}

	virtual void                DrawBone(const Vec3& rParent, const Vec3& rBone, ColorB col)                                                                   {}

	virtual void                RenderText(Vec3 pos, SDrawTextInfo& ti, const char* format, va_list args)                                                      {}

	virtual int                 PushMatrix(const Matrix34&  mat)                                                                                                      { return -1; }
	virtual Matrix34*           GetMatrix()                                                                                                                    { return nullptr; }
	virtual void                SetMatrixIndex(int matID)                                                                                                    {}

	virtual void                Flush()                                                                                                                        {}
	virtual void                Commit(uint frames = 0)                                                                                                        {}

public:
	CAuxGeomCB_Null() {}
	~CAuxGeomCB_Null() {}
};

#endif // #ifndef _RENDERAUXGEOM_H_
