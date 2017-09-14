// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _RENDERAUXGEOM_H_
#define _RENDERAUXGEOM_H_

#if defined(ENABLE_PROFILING_CODE) || !defined(_RELEASE) || defined(ENABLE_DEVELOPER_CONSOLE_IN_RELEASE)
	#define ENABLE_RENDER_AUX_GEOM
#endif

#if defined(ENABLE_RENDER_AUX_GEOM)

	#include <CryRenderer/IRenderAuxGeom.h>
	#include <CryRenderer/VertexFormats.h>
	#include <CryMemory/CrySizer.h>
	#include "TextMessages.h"

class ICrySizer;
class CAuxGeomCB;
struct SAuxGeomCBRawDataPackaged;

struct IRenderAuxGeomImpl
{
public:

	virtual void Flush(SAuxGeomCBRawDataPackaged& data, size_t begin, size_t end, bool reset = false) = 0;
	virtual void RT_Flush(SAuxGeomCBRawDataPackaged& data, size_t begin, size_t end, bool reset = false) = 0;

	virtual void DrawStringImmediate(IFFont_RenderProxy* pFont, float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx) = 0;

	virtual void DrawBufferRT(const SAuxVertex* data, int numVertices, int blendMode, const Matrix44* matViewProj, int texID) = 0;

	virtual void FlushTextMessages(CTextMessages& tMessages, bool reset) = 0;
};

class CAuxGeomCB : public IRenderAuxGeom
{
public:
	// interface
	virtual void                SetRenderFlags(const SAuxGeomRenderFlags& renderFlags) override;
	virtual SAuxGeomRenderFlags GetRenderFlags() override;

	virtual void                DrawPoint(const Vec3& v, const ColorB& col, uint8 size = 1) override;
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size = 1) override;
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size = 1) override;

	virtual void                DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness = 1.0f) override;
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f) override;
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) override;
	virtual void				DrawLines(const Vec3* v, const uint32* packedColorARGB8888, uint32 numPoints, float thickness = 1.0f, bool alphaFlag = true) override;
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f) override;
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f) override;
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness = 1.0f) override;
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness = 1.0f) override;

	virtual void                DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2) override;
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col) override;
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col) override;
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col) override;
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col) override;

	virtual void                DrawBuffer(const SAuxVertex* inVertices, uint32 numVertices, bool textured) override;

	virtual void                DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;
	virtual void                DrawAABBs(const AABB* aabbs, uint32 aabbCount, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;
	virtual void                DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;

	virtual void                DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;
	virtual void                DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;

	virtual void                DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded = true) override;
	virtual void                DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) override;
	virtual void                DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) override;

	virtual void                DrawBone(const Vec3& rParent, const Vec3& rBone, ColorB col) override;

	virtual void                RenderTextQueued(Vec3 pos, const SDrawTextInfo& ti, const char* text) override;
	virtual void                DrawStringImmediate(IFFont_RenderProxy* pFont, float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx) override;

	virtual void                DrawBufferRT(const SAuxVertex* data, int numVertices, int blendMode, const Matrix44* matViewProj, int texID) override;

	virtual int                 SetTexture(int texID) override;
	virtual int                 PushMatrix(const Matrix34& mat) override;
	virtual Matrix34*           GetMatrix() override;
	virtual void                SetMatrixIndex(int matID) override;

	void                        Flush(bool reset);
	virtual void                Flush() override;
	virtual void                Commit(uint frames = 0) override;
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
		SAuxPushBufferEntry() : m_textureID(-1)
		{
		}

		SAuxPushBufferEntry(uint32 numVertices, uint32 numIndices, uint32 vertexOffs, uint32 indexOffs, uint32 transMatrixIdx, int worldMatrixIdx, const SAuxGeomRenderFlags& renderFlags, int texID = -1)
			: m_numVertices(numVertices)
			, m_numIndices(numIndices)
			, m_vertexOffs(vertexOffs)
			, m_indexOffs(indexOffs)
			, m_transMatrixIdx(transMatrixIdx)
			, m_worldMatrixIdx(worldMatrixIdx)
			, m_renderFlags(renderFlags)
			, m_textureID(texID)
		{
		}

		SAuxPushBufferEntry(uint32 drawParamOffs, uint32 transMatrixIdx, int worldMatrixIdx, const SAuxGeomRenderFlags& renderFlags, int texID = -1)
			: m_numVertices(0)
			, m_numIndices(0)
			, m_vertexOffs(drawParamOffs)
			, m_indexOffs(0)
			, m_transMatrixIdx(transMatrixIdx)
			, m_worldMatrixIdx(worldMatrixIdx)
			, m_renderFlags(renderFlags)
			, m_textureID(texID)
		{
			CRY_ASSERT(e_Obj == GetPrimType(m_renderFlags));
		}

		bool GetDrawParamOffs(uint32& drawParamOffs) const
		{
			if (e_Obj == GetPrimType(m_renderFlags))
			{
				drawParamOffs = m_vertexOffs;
				return true;
			}
			return false;
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
		int                 m_textureID;
		SAuxGeomRenderFlags m_renderFlags;
	};

	using AuxPushBuffer = std::vector<SAuxPushBufferEntry>;
	using AuxSortedPushBuffer = std::vector<const SAuxPushBufferEntry*>;
	using AuxVertexBuffer = std::vector<SAuxVertex>;
	using AuxIndexBuffer = std::vector<vtx_idx>;
	using AuxDrawObjParamBuffer = std::vector<SAuxDrawObjParams>;
	using AuxOrthoMatrixBuffer = stl::aligned_vector<Matrix44, 16>;
	using AuxWorldMatrixBuffer = std::deque<Matrix34>;

	struct SAuxGeomCBRawData
	{
	public:
		SAuxGeomCBRawData()
			: m_isUsed(false)
			, m_curRenderFlags(e_Def3DPublicRenderflags)
			, m_curTransMatIdx(-1)
			, m_curWorldMatIdx(-1)
			, m_textureID(-1)
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
			m_textureID = -1;
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
		int                   m_textureID;
		uint                  m_uCount;
		bool                  m_isUsed;
	};

public:
	// c/dtor
	CAuxGeomCB(IRenderAuxGeomImpl* pRenderAuxGeom);
	virtual ~CAuxGeomCB() override;

	// get methods for private flags
	static EPrimType       GetPrimType(const SAuxGeomRenderFlags& renderFlags);
	static bool            IsThickLine(const SAuxGeomRenderFlags& renderFlags);
	static EAuxDrawObjType GetAuxObjType(const SAuxGeomRenderFlags& renderFlags);
	static uint8           GetPointSize(const SAuxGeomRenderFlags& renderFlags);
	static bool            IsTextured(const SAuxGeomRenderFlags& renderFlags);

	// memory usage
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
		for (auto pData : m_cbData)
		{
			pData->GetMemoryUsage(pSizer);
		}
	}

	void FreeMemory()
	{
		for (auto pData : m_cbData)
		{
			stl::reconstruct(*pData);
		}
	}

	// setting orthogonal projection
	void SetOrthoMode(bool enable, Matrix44A* pMatrix = nullptr)
	{
		if (enable)
		{
			CRY_ASSERT(pMatrix);
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

		// for triangles

		// for lines
		e_LineListParam_ProcessThickLines = 0x00000001,

		// for points

		// for objects
	};

private:
	uint32 CreatePointRenderFlags(uint8 size);
	uint32 CreateLineRenderFlags(bool indexed);
	uint32 CreateTriangleRenderFlags(bool indexed, bool textured = false);
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

	using CBList = std::list<SAuxGeomCBRawData*>;
	CBList             m_cbData;

	SAuxGeomCBRawData* m_cbCurrent;

	SAuxGeomCBRawData* AddCBuffer()
	{
		SAuxGeomCBRawData* ptr = new SAuxGeomCBRawData;
		m_cbData.push_back(ptr);
		return ptr;
	}
};

class CAuxGeomCBWorkerThread final : public CAuxGeomCB
{
	SAuxGeomCBRawData*          m_cbProcessed;
	SAuxGeomCBRawData* volatile m_CBReady;

public:
	CAuxGeomCBWorkerThread(IRenderAuxGeomImpl* pRenderAuxGeom) : CAuxGeomCB(pRenderAuxGeom), m_cbProcessed(), m_CBReady() {}
	virtual void Flush() override;
	virtual void Commit(uint frames = 0) override;
	virtual void Process() override;
};

// package CAuxGeomCB::SAuxGeomCBRawData ptr via seperate struct as nested types cannot be forward declared
struct SAuxGeomCBRawDataPackaged
{
	SAuxGeomCBRawDataPackaged(CAuxGeomCB::SAuxGeomCBRawData* pData)
		: m_pData(pData)
	{
		CRY_ASSERT(m_pData);
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

inline uint32 CAuxGeomCB::CreateTriangleRenderFlags(bool indexed, bool textured)
{
	uint32 idx = indexed ? e_TriListInd : e_TriList;
	uint32 tex = textured ? 1 : 0;

	return m_cbCurrent->m_curRenderFlags.m_renderFlags | (idx << e_PrimTypeShift) | tex;
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
			CRY_ASSERT(e_Obj == primType);
			return(e_Obj);
		}
	}
}

inline bool CAuxGeomCB::IsThickLine(const SAuxGeomRenderFlags& renderFlags)
{
	EPrimType primType(GetPrimType(renderFlags));
	//CRY_ASSERT(e_LineList == primType);

	if (e_LineList == primType)
	{
		return (0 != (renderFlags.m_renderFlags & e_LineListParam_ProcessThickLines));
	}
	else
	{
		return(false);
	}
}

inline CAuxGeomCB::EAuxDrawObjType CAuxGeomCB::GetAuxObjType(const SAuxGeomRenderFlags& renderFlags)
{
	EPrimType primType(GetPrimType(renderFlags));
	CRY_ASSERT(e_Obj == primType);

	uint32 objType((renderFlags.m_renderFlags & e_PrivateRenderflagsMask));
	switch (objType)
	{
	case eDOT_Sphere:
	default:
		{
			CRY_ASSERT(eDOT_Sphere == objType);
			return(eDOT_Sphere);
		}
	case eDOT_Cone:
		{
			CRY_ASSERT(eDOT_Cone == objType);
			return(eDOT_Cone);
		}
	case eDOT_Cylinder:
		{
			CRY_ASSERT(eDOT_Cylinder == objType);
			return(eDOT_Cylinder);
		}
	}
}

inline uint8 CAuxGeomCB::GetPointSize(const SAuxGeomRenderFlags& renderFlags)
{
	EPrimType primType(GetPrimType(renderFlags));
	CRY_ASSERT(e_PtList == primType);

	if (e_PtList == primType)
	{
		return(renderFlags.m_renderFlags & e_PrivateRenderflagsMask);
	}
	else
	{
		return(0);
	}
}

inline bool CAuxGeomCB::IsTextured(const SAuxGeomRenderFlags& renderFlags)
{
	EPrimType primType = GetPrimType(renderFlags);

	return (e_TriList == primType || e_TriListInd == primType) && (renderFlags.m_renderFlags & e_PrivateRenderflagsMask) ? true : false;
}

#endif // #if defined(ENABLE_RENDER_AUX_GEOM)

class CAuxGeomCB_Null final : public IRenderAuxGeom
{
public:
	// interface
	virtual void                SetRenderFlags(const SAuxGeomRenderFlags& renderFlags) override                                                                         {}
	virtual SAuxGeomRenderFlags GetRenderFlags() override                                                                                                               { return SAuxGeomRenderFlags(); }

	virtual void                DrawPoint(const Vec3& v, const ColorB& col, uint8 size = 1) override                                                                    {}
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size = 1) override                                                 {}
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size = 1) override                                                 {}

	virtual void                DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness = 1.0f) override                     {}
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f) override                                          {}
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) override                                          {}
	virtual void				DrawLines(const Vec3* v, const uint32* packedColor, uint32 numPoints, float thickness = 1.0f, bool alphaFlag=true) override             {}
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f) override   {}
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f) override   {}
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness = 1.0f) override                          {}
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness = 1.0f) override                          {}

	virtual void                DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2) override    {}
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col) override                                                              {}
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col) override                                                              {}
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col) override                       {}
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col) override                       {}

	virtual void                DrawBuffer(const SAuxVertex* inVertices, uint32 numVertices, bool textured) override                                                    {};

	virtual void                DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override                           {}
	virtual void                DrawAABBs(const AABB* aabbs, uint32 aabbCount, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override       {}
	virtual void                DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override {}

	virtual void                DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override             {}
	virtual void                DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override    {}

	virtual void                DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded = true) override                                           {}
	virtual void                DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) override              {}
	virtual void                DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) override          {}

	virtual void                DrawBone(const Vec3& rParent, const Vec3& rBone, ColorB col) override                                                                   {}

	virtual void                RenderTextQueued(Vec3 pos, const SDrawTextInfo& ti, const char* text) override                                                          {}

	virtual void                DrawBufferRT(const SAuxVertex* data, int numVertices, int blendMode, const Matrix44* matViewProj, int texID) override                   {}

	virtual int                 PushMatrix(const Matrix34& mat) override                                                                                                { return -1; }
	virtual Matrix34*           GetMatrix() override                                                                                                                    { return nullptr; }
	virtual void                SetMatrixIndex(int matID) override                                                                                                      {}

	virtual void                Flush() override                                                                                                                        {}
	virtual void                Commit(uint frames = 0) override                                                                                                        {}

public:
	CAuxGeomCB_Null() = default;
};

#endif // #ifndef _RENDERAUXGEOM_H_
