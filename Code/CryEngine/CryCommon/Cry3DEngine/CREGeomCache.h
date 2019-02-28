// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   CREGeomCache.h
//  Created:     17/10/2012 by Axel Gneiting
//  Description: Backend part of geometry cache rendering
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _CREGEOMCACHE_
#define _CREGEOMCACHE_

#pragma once

#if defined(USE_GEOM_CACHES)

class CREGeomCache : public CRenderElement
{
public:
	struct SMeshInstance
	{
		AABB     m_aabb;
		Matrix34 m_matrix;
		Matrix34 m_prevMatrix;
	};

	struct SMeshRenderData
	{
		DynArray<SMeshInstance> m_instances;
		_smart_ptr<IRenderMesh> m_pRenderMesh;
	};

public:
	CREGeomCache();
	~CREGeomCache();

	bool        Update(const int flags, const bool bTesselation);
	static void UpdateModified();

	// CRenderElement interface
	virtual bool mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation) override;
	
	// CREGeomCache interface
	virtual void                       InitializeRenderElement(const uint numMeshes, _smart_ptr<IRenderMesh>* pMeshes, uint16 materialId);
	virtual void                       SetupMotionBlur(CRenderObject* pRenderObject, const SRenderingPassInfo& passInfo);

	virtual volatile int*              SetAsyncUpdateState(int& threadId);
	virtual DynArray<SMeshRenderData>* GetMeshFillDataPtr();
	virtual DynArray<SMeshRenderData>* GetRenderDataPtr();
	virtual void                       DisplayFilledBuffer(const int threadId);

	// accessors for new render pipeline
	virtual InputLayoutHandle GetVertexFormat() const override;
	virtual bool          GetGeometryInfo(SGeometryInfo& streams, bool bSupportTessellation = false) override;
	virtual void          DrawToCommandList(CRenderObject* pObj, const SGraphicsPipelinePassContext& ctx, CDeviceCommandList* commandList) override;

private:
	uint16        m_materialId;
	volatile bool m_bUpdateFrame[2];
	volatile int  m_transformUpdateState[2];

	//! We use a double buffered m_meshFillData array for input from the main thread. When data
	//! was successfully sent from the main thread it gets copied to m_meshRenderData
	//! This simplifies the cases where frame data is missing, e.g. meshFillData is not updated for a frame
	//! Note that meshFillData really needs to be double buffered because the copy occurs in render thread
	//! so the next main thread could already be touching the data again
	//! Note: m_meshRenderData is directly accessed for ray intersections via GetRenderDataPtr.
	//! This is safe, because it's only used in editor.
	DynArray<SMeshRenderData>         m_meshFillData[2];
	DynArray<SMeshRenderData>         m_meshRenderData;

	static CryCriticalSection         ms_updateListCS[2];
	static std::vector<CREGeomCache*> ms_updateList[2];
};

#endif
#endif
