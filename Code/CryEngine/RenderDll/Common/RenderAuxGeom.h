// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _RENDERAUXGEOM_H_
#define _RENDERAUXGEOM_H_

#if defined(ENABLE_PROFILING_CODE) || !defined(_RELEASE) || defined(ENABLE_DEVELOPER_CONSOLE_IN_RELEASE)
	#define ENABLE_RENDER_AUX_GEOM
#endif

#if defined(ENABLE_RENDER_AUX_GEOM)

	#include <CryRenderer/IRenderAuxGeom.h>
	#include <CryRenderer/VertexFormats.h>
	#include <CryMemory/CrySizer.h>
	#include "RenderOutput.h"
	#include "TextMessages.h"

class ICrySizer;
class CAuxGeomCB;
struct SAuxGeomCBRawDataPackagedConst;
struct IRenderAuxGeom;

struct IRenderAuxGeomImpl
{
public:
	virtual void RT_Flush(const SAuxGeomCBRawDataPackagedConst& data) = 0;
};

class CRenderAuxGeomD3D;
class CAuxGeomCBCollector;

class CAuxGeomCB : public IRenderAuxGeom
{
public:
	friend class CRenderAuxGeomD3D;
	friend class CD3D9Renderer;
	friend class CAuxGeomCBCollector;
	using SAux2DImages = std::vector<SRender2DImageDescription>;

	void Merge(const CAuxGeomCB* pAuxGeomCB);

	// interface
	SAuxGeomRenderFlags SetRenderFlags(const SAuxGeomRenderFlags& renderFlags) final;
	SAuxGeomRenderFlags GetRenderFlags() final;

	const CCamera&      GetCamera() const final;

	void                SetCurrentDisplayContext(const SDisplayContextKey& displayContextKey) override;

	void                DrawPoint(const Vec3& v, const ColorB& col, uint8 size = 1) override;
	void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size = 1) override;
	void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size = 1) override;

	void                DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness = 1.0f) override;
	void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f) override;
	void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) override;
	void                DrawLines(const Vec3* v, const uint32* packedColorARGB8888, uint32 numPoints, float thickness = 1.0f, bool alphaFlag = true) override;
	void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f) override;
	void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f) override;
	void                DrawLineStrip(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) override;
	void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness = 1.0f) override;
	void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness = 1.0f) override;

	void                DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2) override;
	void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col) override;
	void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col) override;
	void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col) override;
	void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col) override;

	void                DrawBuffer(const SAuxVertex* inVertices, uint32 numVertices, bool textured) override;
	SAuxVertex*         BeginDrawBuffer(uint32 maxVertices, bool textured) override;
	void                EndDrawBuffer(uint32 numVertices) override;

	void                DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;
	void                DrawAABBs(const AABB* aabbs, uint32 aabbCount, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;
	void                DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;

	void                DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;
	void                DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) override;

	void                DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded = true) override;
	void                DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) override;
	void                DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) override;

	void                DrawBone(const Vec3& rParent, const Vec3& rBone, ColorB col) override;

	void                RenderTextQueued(Vec3 pos, const SDrawTextInfo& ti, const char* text) override;

	int                 SetTexture(int texID) override;
	int                 PushMatrix(const Matrix34& mat) override;
	Matrix34*           GetMatrix() override;
	void                SetMatrixIndex(int matID) override;
	void                SetOrthographicProjection(bool enable, float l = 0, float r = 1, float b = 0, float t = 1, float n = -1e10, float f = 1e10) override;

	void                PushImage(const SRender2DImageDescription &image) override;

	void                Submit(uint frames = 0) override;

	void                Draw2dImages(SAux2DImages& images, bool reset);
	void                DrawTextMessages(CTextMessages& tMessages, bool reset);

private:
	// These functions are private so that aux geom command buffer cannot be changed.
	// In ideal case, the camera must be set during construction and everything should be rendered/created relative to it.
	// SetCamera is only accessible to the AuxGeomCBCollector so that it can update AuxGeomCBs.
	void                SetCamera(const CCamera& camera);
	void                SetUsingCustomCamera(bool isUsing);
	bool                IsUsingCustomCamera() const;

public:
	enum EPrimType
	{
		e_PtList,
		e_LineList,
		e_LineListInd,
		e_LineStrip,
		e_LineStripInd,
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

		SAuxPushBufferEntry(uint32 numVertices, uint32 numIndices, uint32 vertexOffs, uint32 indexOffs, uint32 transMatrixIdx, int worldMatrixIdx, const SAuxGeomRenderFlags& renderFlags, int texID = -1, const SDisplayContextKey& displayContextKey = {})
			: m_numVertices(numVertices)
			, m_numIndices(numIndices)
			, m_vertexOffs(vertexOffs)
			, m_indexOffs(indexOffs)
			, m_transMatrixIdx(transMatrixIdx)
			, m_worldMatrixIdx(worldMatrixIdx)
			, m_renderFlags(renderFlags)
			, m_textureID(texID)
			, m_displayContextKey(displayContextKey)
		{
		}

		SAuxPushBufferEntry(uint32 drawParamOffs, uint32 transMatrixIdx, int worldMatrixIdx, const SAuxGeomRenderFlags& renderFlags, int texID = -1, const SDisplayContextKey& displayContextKey = {})
			: m_numVertices(0)
			, m_numIndices(0)
			, m_vertexOffs(drawParamOffs)
			, m_indexOffs(0)
			, m_transMatrixIdx(transMatrixIdx)
			, m_worldMatrixIdx(worldMatrixIdx)
			, m_renderFlags(renderFlags)
			, m_textureID(texID)
			, m_displayContextKey(displayContextKey)
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

		uint32                  m_numVertices;
		uint32                  m_numIndices;
		uint32                  m_vertexOffs;
		uint32                  m_indexOffs;
		int                     m_transMatrixIdx;
		int                     m_worldMatrixIdx;
		int                     m_textureID;
		SAuxGeomRenderFlags     m_renderFlags;

		SDisplayContextKey m_displayContextKey;
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
			, m_usingCustomCamera(false)
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
			m_2dImages.clear();

			m_curRenderFlags = e_Def3DPublicRenderflags;
			m_curTransMatIdx = -1;
			m_curWorldMatIdx = -1;
			m_textureID = -1;
			m_uCount = 0;
		}

		bool IsUsed() const
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
		AuxPushBuffer           m_auxPushBuffer;
		AuxVertexBuffer         m_auxVertexBuffer;
		AuxIndexBuffer          m_auxIndexBuffer;
		AuxDrawObjParamBuffer   m_auxDrawObjParamBuffer;
		AuxOrthoMatrixBuffer    m_auxOrthoMatrices;
		AuxWorldMatrixBuffer    m_auxWorldMatrices;
		CTextMessages           m_TextMessages;
		SAux2DImages            m_2dImages;
		SAuxGeomRenderFlags     m_curRenderFlags;
		int                     m_curTransMatIdx;
		int                     m_curWorldMatIdx;
		int                     m_textureID;
		uint                    m_uCount;
		bool                    m_isUsed = false;

		SDisplayContextKey displayContextKey;
		
		// Camera used for 3D->2D elements projection
		CCamera                 m_camera;
		bool                    m_usingCustomCamera;
	};

public:
	// c/dtor
	CAuxGeomCB();
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
		m_rawData->GetMemoryUsage(pSizer);
	}

	void FreeMemory()
	{
		m_rawData->Reset();
	}

	// setting orthogonal projection
	void SetOrthoMode(bool enable, Matrix44A* pMatrix = nullptr)
	{
		if (enable)
		{
			assert( pMatrix!=nullptr );
		}

		if (enable && pMatrix)
		{
			m_rawData->m_curTransMatIdx = m_rawData->m_auxOrthoMatrices.size();
			m_rawData->m_auxOrthoMatrices.push_back(*pMatrix);
		}
		else
			m_rawData->m_curTransMatIdx = -1;
	}

private:
	enum EAuxGeomPrivateRenderflagBitMasks
	{
		// public field starts at bit 21

		e_PrimTypeShift          = 18,
		e_PrimTypeMask           = 0x7 << e_PrimTypeShift,

		e_PrivateRenderflagsMask = (1 << 18) - 1
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
	void                Flush(bool reset);
	void                Flush();

	uint32 CreatePointRenderFlags(uint8 size);
	uint32 CreateLineRenderFlags(bool indexed);
	uint32 CreateLineStripRenderFlag(bool indexed);
	uint32 CreateTriangleRenderFlags(bool indexed, bool textured = false);
	uint32 CreateObjectRenderFlags(const EAuxDrawObjType& objType);

	void   DrawThickLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness);
	void   DrawThickLineStrip(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness);

	void   AddPushBufferEntry(uint32 numVertices, uint32 numIndices, const SAuxGeomRenderFlags& renderFlags);

	void   AddPrimitive(SAuxVertex*& pVertices, uint32 numVertices, const SAuxGeomRenderFlags& renderFlags);
	void   AddIndexedPrimitive(SAuxVertex*& pVertices, uint32 numVertices, vtx_idx*& pIndices, uint32 numIndices, const SAuxGeomRenderFlags& renderFlags);
	void   AddObject(SAuxDrawObjParams*& pDrawParams, const SAuxGeomRenderFlags& renderFlags);

public:
	struct SActiveDrawBufferInfo
	{
		uint32_t m_maxVertices;
		enum State : uint8
		{
			Enabled = BIT(0),
			Textured = BIT(1),
			UseScratchBuffer = BIT(2),

			None = 0,
			All = Enabled | Textured | UseScratchBuffer
		} m_state;
	};
private:
	SActiveDrawBufferInfo m_activeDrawBufferInfo;
	void SetActivateDrawBuffer(bool enabled = true);
	void SetUseScratchActiveDrawBuffer(bool use = true);
	void SetTexturedActiveDrawBuffer(bool textured = true);

	bool IsDrawBufferActive();
	bool IsActiveDrawBufferUsingScratch();
	bool IsActiveDrawBufferTextured();

protected:
	int GetTransMatrixIndex() const
	{
		return m_rawData->m_curTransMatIdx;
	}

	int GetWorldMatrixIndex() const
	{
		return m_rawData->m_curWorldMatIdx;
	}

	SAuxGeomCBRawData* AccessData()
	{
		return m_rawData;
	}
	const SAuxGeomCBRawData* AccessData() const
	{
		return m_rawData;
	}

protected:
	struct PushBufferSortFunc
	{
		bool operator()(const SAuxPushBufferEntry* lhs, const SAuxPushBufferEntry* rhs) const
		{
			if (lhs->m_displayContextKey != rhs->m_displayContextKey)
				return lhs->m_displayContextKey < rhs->m_displayContextKey;

			if (lhs->m_renderFlags.m_renderFlags != rhs->m_renderFlags.m_renderFlags)
				return lhs->m_renderFlags.m_renderFlags < rhs->m_renderFlags.m_renderFlags;

			if (lhs->m_worldMatrixIdx != rhs->m_worldMatrixIdx)
				return lhs->m_worldMatrixIdx < rhs->m_worldMatrixIdx;

			return lhs->m_transMatrixIdx < rhs->m_transMatrixIdx;
		}
	};

	SAuxGeomCBRawData*			   m_rawData = nullptr;
	static const size_t            m_maxScratchBufferVertices = 16 * 1024;
	std::array<SAuxVertex, m_maxScratchBufferVertices * sizeof(SAuxVertex)> m_scratchBuffer;
};
DEFINE_ENUM_FLAG_OPERATORS(CAuxGeomCB::SActiveDrawBufferInfo::State);


// package CAuxGeomCB::SAuxGeomCBRawData ptr via seperate struct as nested types cannot be forward declared
struct SAuxGeomCBRawDataPackagedConst
{
	SAuxGeomCBRawDataPackagedConst(const CAuxGeomCB::SAuxGeomCBRawData* pData)
		: m_pData(pData)
	{
		CRY_ASSERT(m_pData);
	}

	const CAuxGeomCB::SAuxGeomCBRawData* m_pData;
};

inline uint32 CAuxGeomCB::CreatePointRenderFlags(uint8 size)
{
	return(m_rawData->m_curRenderFlags.m_renderFlags | (e_PtList << e_PrimTypeShift) | size);
}

inline uint32 CAuxGeomCB::CreateLineRenderFlags(bool indexed)
{
	if (false != indexed)
	{
		return(m_rawData->m_curRenderFlags.m_renderFlags | (e_LineListInd << e_PrimTypeShift));
	}
	else
	{
		return(m_rawData->m_curRenderFlags.m_renderFlags | (e_LineList << e_PrimTypeShift));
	}
}

inline uint32 CAuxGeomCB::CreateLineStripRenderFlag(bool indexed)
{
	if (false != indexed)
	{
		return(m_rawData->m_curRenderFlags.m_renderFlags | (e_LineStripInd << e_PrimTypeShift));
	}
	else
	{
		return(m_rawData->m_curRenderFlags.m_renderFlags | (e_LineStrip << e_PrimTypeShift));
	}
}

inline uint32 CAuxGeomCB::CreateTriangleRenderFlags(bool indexed, bool textured)
{
	uint32 idx = indexed ? e_TriListInd : e_TriList;
	uint32 tex = textured ? 1 : 0;

	return m_rawData->m_curRenderFlags.m_renderFlags | (idx << e_PrimTypeShift) | tex;
}

inline uint32 CAuxGeomCB::CreateObjectRenderFlags(const EAuxDrawObjType& objType)
{
	return(m_rawData->m_curRenderFlags.m_renderFlags | (e_Obj << e_PrimTypeShift) | objType);
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
	case e_LineStrip:
		{
			return(e_LineStrip);
		}
	case e_LineStripInd:
		{
			return(e_LineStripInd);
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
	SAuxGeomRenderFlags SetRenderFlags(const SAuxGeomRenderFlags& renderFlags) final                                                                         { return SAuxGeomRenderFlags(); }
	SAuxGeomRenderFlags GetRenderFlags() final                                                                                                               { return SAuxGeomRenderFlags(); }

	const CCamera&      GetCamera() const final                                                                                                              { static CCamera camera; return camera; }

	void                SetCurrentDisplayContext(const SDisplayContextKey& displayContextKey) final                                               {};

	void                DrawPoint(const Vec3& v, const ColorB& col, uint8 size = 1) final                                                                    {}
	void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size = 1) final                                                 {}
	void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size = 1) final                                                 {}

	void                DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness = 1.0f) final                     {}
	void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f) final                                          {}
	void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) final                                          {}
	void                DrawLines(const Vec3* v, const uint32* packedColorARGB8888, uint32 numPoints, float thickness = 1.0f, bool alphaFlag = true) final   {}
	void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f) final   {}
	void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f) final   {}
	void                DrawLineStrip(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) final										 {}
	void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness = 1.0f) final                          {}
	void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness = 1.0f) final                          {}

	void                DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2) final    {}
	void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col) final                                                              {}
	void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col) final                                                              {}
	void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col) final                       {}
	void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col) final                       {}

	void                DrawBuffer(const SAuxVertex* inVertices, uint32 numVertices, bool textured) final                                                    {}
	SAuxVertex*         BeginDrawBuffer(uint32 maxVertices, bool textured) final                                                                             { return nullptr; }
	void                EndDrawBuffer(uint32 numVertices) final                                                                                              {}

	void                DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) final                           {}
	void                DrawAABBs(const AABB* aabbs, uint32 aabbCount, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) final       {}
	void                DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) final {}

	void                DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) final             {}
	void                DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) final    {}

	void                DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded = true) final                                           {}
	void                DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) final              {}
	void                DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) final          {}

	void                DrawBone(const Vec3& rParent, const Vec3& rBone, ColorB col) final                                                                   {}

	void                RenderTextQueued(Vec3 pos, const SDrawTextInfo& ti, const char* text) final                                                          {}

	void                PushImage(const SRender2DImageDescription &image) final                                                                              {};

	int                 PushMatrix(const Matrix34& mat) final                                                                                                { return -1; }
	Matrix34*           GetMatrix() final                                                                                                                    { return nullptr; }
	void                SetMatrixIndex(int matID) final                                                                                                      {}
	void                SetOrthographicProjection(bool enable, float l = 0, float r = 1, float b = 0, float t = 1, float n = -1e10, float f = 1e10) final	 {}

	void                Submit(uint frames = 0) final                                                                                                        {}

public:
	CAuxGeomCB_Null() = default;
};

#endif // #ifndef _RENDERAUXGEOM_H_
