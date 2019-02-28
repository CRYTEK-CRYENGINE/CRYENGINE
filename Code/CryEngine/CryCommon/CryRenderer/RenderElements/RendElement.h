// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RENDELEMENT_H__
#define __RENDELEMENT_H__

#include <CryRenderer/VertexFormats.h>
#include <CryMath/Cry_Color.h>

class CRenderElement;
struct CRenderChunk;
struct PrimitiveGroup;
class CShader;
struct SShaderTechnique;
class CParserBin;
struct SParserFrame;
class CRenderView;

enum EDataType
{
	eDATA_Unknown = 0,
	eDATA_Sky,
	eDATA_ClientPoly,
	eDATA_Flare,
	eDATA_Terrain,
	eDATA_SkyZone,
	eDATA_Mesh,
	eDATA_LensOptics,
	eDATA_OcclusionQuery,
	eDATA_Particle,
	eDATA_HDRSky,
	eDATA_FogVolume,
	eDATA_WaterVolume,
	eDATA_WaterOcean,
	eDATA_GameEffect,
	eDATA_BreakableGlass,
	eDATA_GeomCache,
};

enum ERenderElementFlags
{
	FCEF_TRANSFORM             = BIT(0),
	FCEF_DIRTY                 = BIT(1),
	FCEF_NODEL                 = BIT(2),
	FCEF_DELETED               = BIT(3),

	FCEF_UPDATEALWAYS          = BIT(8),

	FCEF_SKINNED               = BIT(11),
	FCEF_PRE_DRAW_DONE         = BIT(12),
};

typedef uintptr_t stream_handle_t;

struct SStreamInfo
{
	stream_handle_t hStream = ~0u;
	uint32          nStride =  0u; // NOTE: for index buffers this needs to contain the index format
	uint32          nSlot   =  0u;
};


class IRenderElement
{
public:
	IRenderElement() {}
	virtual ~IRenderElement() {};

	virtual CRenderChunk*      mfGetMatInfo() = 0;
	virtual TRenderChunkArray* mfGetMatInfoList() = 0;
	virtual int                mfGetMatId() = 0;
	virtual void               mfReset() = 0;
	virtual bool               mfIsHWSkinned() = 0;
	virtual CRenderElement*      mfCopyConstruct(void) = 0;
	virtual void               mfCenter(Vec3& centr, CRenderObject* pObj, const SRenderingPassInfo& passInfo) = 0;
	virtual void               mfGetBBox(Vec3& vMins, Vec3& vMaxs) const = 0;

	virtual bool  mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation = false) = 0;

	virtual void  mfExport(struct SShaderSerializeContext& SC) = 0;
	virtual void  mfImport(struct SShaderSerializeContext& SC, uint32& offset) = 0;


	//////////////////////////////////////////////////////////////////////////
	// ~Pipeline 2.0 methods.
	//////////////////////////////////////////////////////////////////////////

	virtual InputLayoutHandle GetVertexFormat() const = 0;

	//! Compile is called on a non mesh render elements, must be called only in rendering thread
	//! Returns false if compile failed, and render element must not be rendered
	virtual bool          Compile(CRenderObject* pObj, CRenderView *pRenderView, bool updateInstanceDataOnly) = 0;

	//! Custom Drawing for the non mesh render elements.
	//! Must be thread safe for the parallel recording
	virtual void          DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx, class CDeviceCommandList* commandList) = 0;

	//////////////////////////////////////////////////////////////////////////
	// ~Pipeline 2.0 methods.
	//////////////////////////////////////////////////////////////////////////

	virtual int           Size() = 0;

	virtual void         Release(bool bForce = false) = 0;
	virtual void         GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

class CRenderElement : public IRenderElement
{
	static int s_nCounter;
public:
	static CRenderElement s_RootGlobal;
	static CRenderElement *s_pRootRelease[];
	CRenderElement*       m_NextGlobal;
	CRenderElement*       m_PrevGlobal;

	EDataType           m_Type;

	uint32     m_nID;
	uint16     m_Flags;
	uint16     m_nFrameUpdated;

	enum { MAX_CUSTOM_TEX_BINDS_NUM = 2 };
	void* m_CustomData;
	int   m_CustomTexBind[MAX_CUSTOM_TEX_BINDS_NUM];

	static CryCriticalSection s_accessLock;

public:
	struct SGeometryInfo
	{
		uint32        bonesRemapGUID               = 0u; // Input parameter to fetch correct skinning stream.

		int           primitiveType                = 0; //!< \see eRenderPrimitiveType
		InputLayoutHandle eVertFormat              = EDefaultInputLayouts::Empty;

		int32         nFirstIndex                  = 0;
		int32         nNumIndices                  = 0;
		uint32        nFirstVertex                 = 0u;
		uint32        nNumVertices                 = 0u;

		uint32        nNumVertexStreams            = 0u;

		SStreamInfo   indexStream;
		SStreamInfo   vertexStreams[VSF_NUM]; // contains only nNumVertexStreams elements

		void*         pTessellationAdjacencyBuffer = nullptr;
		void*         pSkinningExtraBonesBuffer    = nullptr;
		uint32        nTessellationPatchIDOffset   = 0u;

		inline uint32 CalcStreamMask()
		{
			uint32 streamMask = 0;
			for (uint32 s = 0; s < nNumVertexStreams; ++s)
				streamMask |= 1U << vertexStreams[s].nSlot;

			return streamMask;
		}

		inline uint32 CalcLastStreamSlot()
		{
			uint32 lastStreamSlot = 0;
			for (uint32 s = 0; s < nNumVertexStreams; ++s)
				lastStreamSlot = std::max(lastStreamSlot, vertexStreams[s].nSlot);

			return lastStreamSlot;
		}
	};

public:
	CRenderElement(bool bGlobal);
	CRenderElement();
	virtual ~CRenderElement();

	inline void UnlinkGlobal()
	{
		if (!m_NextGlobal || !m_PrevGlobal)
			return;
		m_NextGlobal->m_PrevGlobal = m_PrevGlobal;
		m_PrevGlobal->m_NextGlobal = m_NextGlobal;
		m_NextGlobal = m_PrevGlobal = NULL;
	}
	inline void LinkGlobal(CRenderElement* Before)
	{
		if (m_NextGlobal || m_PrevGlobal)
			return;
		m_NextGlobal = Before->m_NextGlobal;
		Before->m_NextGlobal->m_PrevGlobal = this;
		Before->m_NextGlobal = this;
		m_PrevGlobal = Before;
	}

	const char*      mfTypeString();
	inline EDataType mfGetType() { return m_Type; }
	void             mfSetType(EDataType t) { m_Type = t; }

	inline uint32 mfGetFlags(void)         { return m_Flags; }
	inline void   mfSetFlags(uint32 fl)    { m_Flags = fl; }
	inline void   mfUpdateFlags(uint32 fl) { m_Flags |= fl; }
	inline void   mfClearFlags(uint32 fl)  { m_Flags &= ~fl; }
	inline bool   mfCheckUpdate(InputLayoutHandle eVertFormat, int Flags, uint16 nFrame, bool bTessellation = false)
	{
		if (nFrame != m_nFrameUpdated || (m_Flags & (FCEF_DIRTY | FCEF_SKINNED | FCEF_UPDATEALWAYS)))
		{
			m_nFrameUpdated = nFrame;
			return mfUpdate(eVertFormat, Flags, bTessellation);
		}
		return true;
	}

	virtual CRenderChunk*      mfGetMatInfo();
	virtual TRenderChunkArray* mfGetMatInfoList();
	virtual int                mfGetMatId();
	virtual void               mfReset();
	virtual bool               mfIsHWSkinned() { return false; }
	virtual CRenderElement*    mfCopyConstruct(void);
	virtual void               mfCenter(Vec3& centr, CRenderObject* pObj, const SRenderingPassInfo& passInfo);
	virtual void               mfGetBBox(Vec3& vMins, Vec3& vMaxs) const
	{
		vMins.Set(0, 0, 0);
		vMaxs.Set(0, 0, 0);
	}
	virtual void  mfGetPlane(Plane& pl);


	virtual void* mfGetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags);

	virtual bool  mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation = false) { return true; }

	virtual void  mfExport(struct SShaderSerializeContext& SC)                               { CryFatalError("mfExport has not been implemented for this render element type"); }
	virtual void  mfImport(struct SShaderSerializeContext& SC, uint32& offset)               { CryFatalError("mfImport has not been implemented for this render element type"); }


	//////////////////////////////////////////////////////////////////////////
	// ~Pipeline 2.0 methods.
	//////////////////////////////////////////////////////////////////////////

	virtual InputLayoutHandle GetVertexFormat() const                                                    { return InputLayoutHandle::Unspecified; };
	virtual bool          GetGeometryInfo(SGeometryInfo& streams, bool bSupportTessellation = false) { return false; }

	//! Compile is called on a non mesh render elements, must be called only in rendering thread
	//! Returns false if compile failed, and render element must not be rendered
	virtual bool          Compile(CRenderObject* pObj, CRenderView *pRenderView, bool updateInstanceDataOnly)  { return false; };

	//! Custom Drawing for the non mesh render elements.
	//! Must be thread safe for the parallel recording
	virtual void          DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx, class CDeviceCommandList* commandList)  {};
	
	//////////////////////////////////////////////////////////////////////////
	// ~Pipeline 2.0 methods.
	//////////////////////////////////////////////////////////////////////////

	virtual int           Size()                                                            { return 0; }

	virtual void         Release(bool bForce = false);
	virtual void         GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }


	static void          ShutDown();
	static void          Tick();

	static void          Cleanup();
};

#include "CREMesh.h"
#include "CRESky.h"
#include "CREOcclusionQuery.h"
#include "CREFogVolume.h"
#include "CREWaterVolume.h"
#include "CREWaterOcean.h"
#include "CREGameEffect.h"
#include "CREBreakableGlass.h"
#include <Cry3DEngine/CREGeomCache.h>

#endif  // __RENDELEMENT_H__
